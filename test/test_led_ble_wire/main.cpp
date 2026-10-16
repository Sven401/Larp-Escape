#include "../../firmware/led/include/LedWire.h"
#include "../../firmware/led/include/NfcBleWire.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#else
#include <openssl/hmac.h>
#endif

using namespace larp::ledwire;
static unsigned assertions = 0;
static void check(bool value, const char *message) {
    ++assertions;
    if (!value) throw std::runtime_error(message);
}
class Crypto : public IHmac256 {
  public:
    bool calculate(const Secret &secret, const std::uint8_t *data, std::size_t size,
                   Secret &result) noexcept override {
#ifdef _WIN32
        BCRYPT_ALG_HANDLE algorithm = nullptr;
        BCRYPT_HASH_HANDLE hash = nullptr;
        if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr,
                                        BCRYPT_ALG_HANDLE_HMAC_FLAG) != 0) return false;
        const auto create = BCryptCreateHash(algorithm, &hash, nullptr, 0,
                                             const_cast<PUCHAR>(secret.data()),
                                             static_cast<ULONG>(secret.size()), 0);
        bool ok = create == 0;
        if (ok) ok = BCryptHashData(hash, const_cast<PUCHAR>(data),
                                   static_cast<ULONG>(size), 0) == 0;
        if (ok) ok = BCryptFinishHash(hash, result.data(),
                                     static_cast<ULONG>(result.size()), 0) == 0;
        if (hash) BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return ok;
#else
        unsigned written = 0;
        return HMAC(EVP_sha256(), secret.data(), static_cast<int>(secret.size()),
                    data, size, result.data(), &written) && written == result.size();
#endif
    }
};
class Control : public IControl {
  public:
    Snapshot value;
    unsigned submissions = 0, stops = 0;
    bool accept = true;
    Effect requested = Effect::Off;
    Control() { value.flags = Ready | Off; value.mode = Mode::LedOnly; }
    Snapshot snapshot() const noexcept override { return value; }
    bool submit(Operation op, Effect effect, std::uint32_t revision,
                std::uint32_t &target) noexcept override {
        ++submissions;
        if (!accept || revision != value.revision) return false;
        requested = op == Operation::Start ? effect : Effect::Off;
        target = ++value.revision;
        value.flags |= Pending;
        return true;
    }
    void apply() {
        value.appliedRevision = value.revision;
        value.effect = requested;
        value.flags = Ready | (requested == Effect::Off ? Off : 0);
    }
    void stopOnDisconnect() noexcept override {
        ++stops;
        ++value.revision;
        value.appliedRevision = value.revision;
        value.effect = Effect::Off;
        value.flags = Ready | Off;
    }
};
static Message hex(const std::string &text) {
    Message m;
    check(text.size() % 2 == 0 && text.size() <= 256, "fixture hex bounds");
    m.size = static_cast<std::uint16_t>(text.size() / 2);
    for (std::size_t i = 0; i < m.size; ++i)
        m.bytes[i] = static_cast<std::uint8_t>(std::stoul(text.substr(i * 2, 2), nullptr, 16));
    return m;
}
static std::string fixture;
static Message get(const std::string &name) {
    const auto key = fixture.find("\"" + name + "\"");
    check(key != std::string::npos, "fixture key");
    const auto first = fixture.find('"', fixture.find(':', key) + 1);
    const auto last = fixture.find('"', first + 1);
    return hex(fixture.substr(first + 1, last - first - 1));
}
template <std::size_t N> static std::array<std::uint8_t, N> fixed(const std::string &name) {
    const auto m = get(name);
    check(m.size == N, "fixture fixed size");
    std::array<std::uint8_t, N> out = {};
    std::copy_n(m.bytes.begin(), N, out.begin());
    return out;
}
static bool same(const Message &a, const Message &b) {
    return a.size == b.size && std::equal(a.bytes.begin(), a.bytes.begin() + a.size, b.bytes.begin());
}
static void sign(Crypto &crypto, const Secret &key, const char *domain,
                 Message &m, std::size_t body) {
    const std::string prefix = std::string("LARP-LED/1/") + domain;
    std::vector<std::uint8_t> bytes(prefix.begin(), prefix.end());
    bytes.insert(bytes.end(), m.bytes.begin(), m.bytes.begin() + body);
    Secret mac = {};
    check(crypto.calculate(key, bytes.data(), bytes.size(), mac), "actual host HMAC");
    m.size = static_cast<std::uint16_t>(body + 32);
    std::copy(mac.begin(), mac.end(), m.bytes.begin() + body);
}
struct Harness {
    Crypto crypto;
    Control control;
    Session session;
    Secret key;
    Harness() : session(crypto, control), key(fixed<32>("secret")) {
        check(session.configure(fixed<16>("device"), key), "configure");
        check(session.beginConnection(fixed<16>("boot"), fixed<16>("connection"), 1000), "connect");
    }
    void auth() {
        Message out;
        check(session.receive(get("hello"), 1001, out) == SessionResult::Reply, "challenge reply");
        check(same(out, get("challenge")), "C++/Python challenge bytes");
        check(session.receive(get("proof"), 1002, out) == SessionResult::Reply, "grant reply");
        check(same(out, get("granted")), "C++/Python granted bytes");
        check(session.authenticated(), "authenticated");
    }
    Message command(std::uint32_t request = 1, std::uint32_t revision = 0,
                    std::uint32_t expires = 4002, std::uint8_t op = 1, std::uint8_t effect = 1) {
        auto m = get("command_start_twinkle");
        write32(m.bytes.data() + 18, request); write32(m.bytes.data() + 22, revision);
        write32(m.bytes.data() + 26, expires); m.bytes[30] = op; m.bytes[31] = effect;
        sign(crypto, key, "command", m, 32);
        return m;
    }
};
static void happyPath() {
    Harness h; h.auth();
    Message out;
    auto command = get("command_start_twinkle");
    check(h.session.receive(command, 1010, out) == SessionResult::Accepted && !out.size,
          "write acceptance is not physical-driver reply");
    check(h.control.submissions == 1 && h.session.pending(), "one owner submission");
    check(!h.session.service(1010, out), "no result before applied revision");
    h.control.apply();
    check(h.session.service(1011, out) && same(out, get("result_start")), "actual applied result");
    check(h.session.publishStatus(1250, out) && same(out, get("status_twinkle")), "status vector");
    check(h.session.receive(get("command_stop"), 1251, out) == SessionResult::Accepted, "stop accepted");
    h.control.apply();
    check(h.session.service(1252, out) && same(out, get("result_stop")), "off result vector");
    h.session.disconnect();
    check(h.control.stops == 1 && !h.session.authenticated(), "disconnect revokes and stops");
}
static void wakeStatus() {
    for (const bool all : {false, true}) {
        Harness h; Message out;
        h.control.value.wakeSupported = true;
        h.control.value.wakeValid = all;
        h.control.value.wakeMask = all ? 15 : 0;
        check(h.session.receive(get("hello"), 1001, out) == SessionResult::Reply, "wake challenge");
        check(h.session.receive(get("proof"), 1002, out) == SessionResult::Reply &&
              same(out, get(all ? "granted_wake_all" : "granted_wake_unknown")),
              "wake grant has independent Python HMAC bytes");
        h.control.value.wakeValid = true; h.control.value.wakeMask = 5;
        check(h.session.publishStatus(1250, out) && same(out, get("status_wake_1_3")),
              "simultaneous wake1+3 is authenticated status");
        auto tampered = out; tampered.bytes[37] ^= 1;
        sign(h.crypto, h.key, "status", tampered, 38);
        check(!std::equal(tampered.bytes.begin() + 38, tampered.bytes.begin() + 70,
                          out.bytes.begin() + 38), "wake mask belongs to authenticated body");
    }
    for (unsigned mask = 0; mask < 16; ++mask) {
        Harness h; h.auth(); Message out;
        h.control.value.wakeSupported = h.control.value.wakeValid = true;
        h.control.value.wakeMask = static_cast<std::uint8_t>(mask);
        check(h.session.publishStatus(1250, out) && out.size == 70 && out.bytes[37] == (0xc0 | mask),
              "all supported valid wake masks and unchanged status size");
    }
    const struct Bad { bool supported, valid; std::uint8_t mask; } invalid[] = {
        {false, true, 0}, {false, true, 1}, {false, false, 1}, {true, false, 1},
        {true, true, 0x10}, {true, true, 0x20}, {true, true, 0x40}, {true, true, 0x80}
    };
    for (const auto &bad : invalid) {
        Harness h; h.auth(); Message out;
        h.control.value.wakeSupported = bad.supported;
        h.control.value.wakeValid = bad.valid; h.control.value.wakeMask = bad.mask;
        check(!h.session.publishStatus(1250, out) && !out.size && !h.session.connected(),
              "invalid wake combinations never leave as status");
    }
    {
        Harness h; h.auth(); Message out;
        h.control.value.wakeSupported = true; // I2C fault must not block LED control.
        check(h.session.receive(get("command_start_twinkle"), 1010, out) == SessionResult::Accepted,
              "supported unknown wake allows independent LED operation");
        h.control.apply();
        h.control.value.wakeValid = true; h.control.value.wakeMask = 15;
        check(h.session.service(1011, out) && same(out, get("result_start")),
              "Results carry no wake data and remain byte-identical");
    }
}
static void authentication() {
    {
        Harness h; Message out;
        auto bad = get("hello"); bad.bytes[2] = 0;
        check(h.session.receive(bad, 1001, out) == SessionResult::Reply, "different client nonce challenge");
        check(h.session.receive(get("proof"), 1002, out) == SessionResult::Rejected,
              "proof binds exact client nonce");
        check(!h.session.authenticated(), "bad proof closes");
    }
    {
        Harness h; Message out;
        check(h.session.receive(get("proof"), 1001, out) == SessionResult::Rejected,
              "proof requires issued challenge");
    }
    {
        Harness h; Message out;
        h.session.receive(get("hello"), 1001, out);
        auto bad = get("proof"); bad.bytes[33] ^= 1;
        check(h.session.receive(bad, 1002, out) == SessionResult::Rejected, "bad MAC");
    }
    {
        Harness h; Message out;
        h.session.receive(get("hello"), 1001, out);
        check(h.session.receive(get("hello"), 10999, out) == SessionResult::Reply, "duplicate hello");
        check(h.session.receive(get("proof"), 11000, out) == SessionResult::Rejected,
              "duplicate cannot extend absolute auth deadline");
    }
    {
        Harness h; h.auth(); Message out;
        auto bad = h.command(); bad.bytes[40] ^= 1;
        check(h.session.receive(bad, 1010, out) == SessionResult::Rejected &&
              h.control.submissions == 0 && h.control.stops == 1, "tampered command closes");
    }
    {
        Harness h; h.auth(); Message out;
        h.session.disconnect();
        auto changed = fixed<16>("connection"); changed[0] ^= 1;
        check(h.session.beginConnection(fixed<16>("boot"), changed, 1200), "new link");
        h.session.receive(get("hello"), 1201, out);
        check(h.session.receive(get("proof"), 1202, out) == SessionResult::Rejected,
              "proof cannot migrate to new link");
    }
    {
        Harness h; h.auth(); Message out;
        auto bad = h.command(); bad.bytes[2] ^= 1; sign(h.crypto, h.key, "command", bad, 32);
        check(h.session.receive(bad, 1010, out) == SessionResult::Rejected, "wrong session even valid MAC");
    }
}
static void admission() {
    for (const auto mode : {Mode::Running, Mode::Paused, Mode::Unknown}) {
        Harness h; h.auth(); Message out;
        h.control.value.mode = mode;
        check(h.session.receive(h.command(), 1010, out) == SessionResult::Reply &&
              out.bytes[38] == static_cast<std::uint8_t>(Outcome::GameActive) &&
              h.control.submissions == 0, "non-stopped game mode fails closed");
    }
    {
        Harness h; h.auth(); Message out;
        h.control.value.mode = Mode::Stopped;
        check(h.session.receive(h.command(), 1010, out) == SessionResult::Accepted, "explicit stopped allowed");
    }
    for (const auto effect : {1U, 2U, 3U}) {
        Harness h; h.auth(); Message out;
        check(h.session.receive(h.command(1, 0, 4002, 1, static_cast<std::uint8_t>(effect)), 1010, out) ==
              SessionResult::Accepted, "three fixed effects accepted");
    }
    const struct Case { std::uint32_t revision, expires; std::uint8_t op, effect; Outcome outcome; } cases[] = {
        {1, 4002, 1, 1, Outcome::StaleRevision}, {0, 1010, 1, 1, Outcome::Expired},
        {0, 1009, 1, 1, Outcome::Expired}, {0, 4011, 1, 1, Outcome::Expired},
        {0, 4002, 1, 0, Outcome::InvalidCommand}, {0, 4002, 2, 1, Outcome::InvalidCommand},
        {0, 4002, 9, 1, Outcome::InvalidCommand}, {0, 4002, 1, 4, Outcome::InvalidCommand}};
    for (const auto &c : cases) {
        Harness h; h.auth(); Message out;
        check(h.session.receive(h.command(1, c.revision, c.expires, c.op, c.effect), 1010, out) ==
              SessionResult::Reply && out.bytes[38] == static_cast<std::uint8_t>(c.outcome),
              "invalid request outcome");
        check(h.control.submissions == 0 && h.session.nextRequest() == 2,
              "valid authenticated rejection consumes request without output action");
    }
    {
        Harness h; h.auth(); Message out;
        h.control.accept = false;
        h.session.receive(h.command(), 1010, out);
        check(out.bytes[38] == static_cast<std::uint8_t>(Outcome::Unavailable), "driver rejection");
    }
}
static void receiptsAndDriver() {
    {
        Harness h; h.auth(); Message out;
        auto cmd = h.command();
        h.session.receive(cmd, 1010, out);
        check(h.session.receive(cmd, 1011, out) == SessionResult::Accepted &&
              h.control.submissions == 1, "pending duplicate never reexecutes");
        h.control.apply(); h.session.service(1012, out);
        const auto receipt = out;
        check(h.session.receive(cmd, 50000, out) == SessionResult::Reply && same(out, receipt) &&
              h.control.submissions == 1, "exact duplicate receipt never reexecutes expired command");
        auto changed = h.command(1, 0, 4002, 1, 2);
        h.session.receive(changed, 1013, out);
        check(out.bytes[38] == static_cast<std::uint8_t>(Outcome::Conflict) &&
              h.control.submissions == 1, "same request different effect conflict");
    }
    {
        Harness h; h.auth(); Message out;
        h.session.receive(h.command(2), 1010, out);
        check(out.bytes[38] == static_cast<std::uint8_t>(Outcome::OutOfOrder) &&
              h.session.nextRequest() == 1, "skipped request does not consume");
    }
    {
        Harness h; h.auth(); Message out;
        h.session.receive(h.command(), 1010, out);
        h.control.value.flags = Fault | Off;
        check(h.session.service(1011, out) && out.bytes[38] == static_cast<std::uint8_t>(Outcome::DriverFault),
              "driver fault is never Applied");
    }
    {
        Harness h; h.auth(); Message out;
        h.session.receive(h.command(), 1010, out);
        ++h.control.value.revision;
        check(h.session.service(1011, out) && out.bytes[38] == static_cast<std::uint8_t>(Outcome::Superseded),
              "replacement revision not mistaken for applied");
    }
    {
        Harness h; h.auth(); Message out;
        h.session.receive(h.command(), 1010, out);
        check(!h.session.service(3009, out), "driver deadline not early");
        check(h.session.service(3010, out) && out.bytes[38] == static_cast<std::uint8_t>(Outcome::DriverFault)
              && h.control.stops == 1, "driver timeout stops output");
    }
}
static void framing() {
    for (const auto &name : {"hello", "challenge", "proof", "command_start_twinkle", "result_stop"}) {
        const auto message = get(name);
        Assembler assembler;
        Message out;
        std::array<std::uint8_t, 20> frame = {};
        std::size_t written = 0;
        for (std::uint16_t offset = 0; offset < message.size; offset += 12) {
            check(encodeFrame(message, 1, offset, frame, written) && written <= 20, "bounded frame");
            const auto result = assembler.accept(frame.data(), written, 1000 + offset, out);
            const bool last = offset + 12 >= message.size;
            check(result == (last ? FrameResult::Complete : FrameResult::Receiving), "sequential frame");
            if (!last) check(assembler.accept(frame.data(), written, 1001 + offset, out) ==
                             FrameResult::Duplicate, "exact fragment duplicate");
            if (last) check(same(message, out), "roundtrip exact bytes");
        }
        encodeFrame(message, 1, 0, frame, written);
        check(assembler.accept(frame.data(), written, 1300, out) == FrameResult::Rejected,
              "completed transfer cannot replay");
    }
    {
        Assembler a; Message out; std::array<std::uint8_t, 20> f = {}; std::size_t n = 0;
        auto m = get("challenge");
        encodeFrame(m, 1, 0, f, n); a.accept(f.data(), n, 1000, out);
        encodeFrame(m, 1, 12, f, n);
        check(a.accept(f.data(), n, 3000, out) == FrameResult::Rejected, "absolute frame timeout");
    }
    {
        Assembler a; Message out; std::array<std::uint8_t, 20> f = {}; std::size_t n = 0;
        auto m = get("challenge");
        encodeFrame(m, 1, 0, f, n); a.accept(f.data(), n, 0xfffffff0U, out);
        encodeFrame(m, 1, 12, f, n);
        check(a.accept(f.data(), n, 20, out) == FrameResult::Receiving, "assembly clock wrap");
        encodeFrame(m, 2, 24, f, n);
        check(a.accept(f.data(), n, 30, out) == FrameResult::Rejected, "mixed transfer rejected");
    }
}
class NfcPort : public larp::nfcble::Port {
 public:
    unsigned submissions = 0, cancellations = 0;
    bool ready = false, accepted = true;
    larp::nfcble::Reply reply;
    bool submit(bool, const std::uint8_t *, std::uint8_t) override { ++submissions; return accepted; }
    bool poll(larp::nfcble::Reply &out) override { if (!ready) return false; out = reply; ready = false; return true; }
    void cancel() override { ++cancellations; }
};
static void nfcWire() {
    Harness h; NfcPort port; larp::nfcble::Session nfc(h.session, port); Message out;
    check(!nfc.receive(get("nfc_read"), 1002, out), "NFC before auth rejected");
    check(port.submissions == 0, "no NFC without auth"); h.auth();
    auto bad = get("nfc_read"); bad.bytes[40] ^= 1;
    check(!nfc.receive(bad, 1003, out) && port.submissions == 0, "NFC bad MAC rejected");
    bad = get("nfc_read"); bad.bytes[2] ^= 1;
    check(!nfc.receive(bad, 1003, out), "NFC wrong session rejected");
    check(nfc.receive(get("nfc_read"), 1003, out) && out.size == 0 && nfc.busy(), "NFC asynchronous read");
    check(nfc.receive(get("nfc_read"), 1004, out) && port.submissions == 1, "pending NFC duplicate never reexecutes");
    check(nfc.receive(get("nfc_change"), 1005, out) && out.bytes[34] == 10 && port.submissions == 1, "NFC busy rejects another command");
    // Busy response advanced the result sequence, start a fresh session for golden bytes.
    nfc.reset();
    check(port.cancellations == 1 && !nfc.busy(), "NFC reset cancels owner");
    check(nfc.receive(get("nfc_read"), 1003, out), "NFC read after reset");
    auto expected = get("nfc_read_result");
    port.reply.outcome = 0; port.reply.kind = 2; port.reply.uidSize = 7; port.reply.payloadSize = 25;
    std::copy_n(expected.bytes.data() + 37, 7, port.reply.uid);
    std::copy_n(expected.bytes.data() + 48, 25, port.reply.payload); port.ready = true;
    check(nfc.service(1010, out) && same(out, expected), "C++ NFC read response matches Python HMAC");
    const auto submitted = port.submissions;
    check(nfc.receive(get("nfc_read"), 1011, out) && same(out, expected) && port.submissions == submitted, "completed duplicate cached");
    check(nfc.receive(get("nfc_change"), 1250, out) && !out.size, "NFC bound write accepted");
    expected = get("nfc_write_result"); port.reply.outcome = 8;
    std::copy_n(expected.bytes.data() + 48, 25, port.reply.payload); port.ready = true;
    check(nfc.service(1252, out) && same(out, expected), "C++ NFC verified write matches Python HMAC");
    check(nfc.receive(get("nfc_change"), 1253, out) && same(out, expected), "write duplicate never rewrites");
    h.session.disconnect();
    check(!nfc.receive(get("nfc_read"), 1254, out), "closed NFC session rejected");
    Harness late; late.auth(); NfcPort other; larp::nfcble::Session expired(late.session, other);
    check(expired.receive(get("nfc_read"), 4002, out) && out.bytes[34] == 11 && other.submissions == 0, "expired NFC does not touch tag");
    check(expired.receive(get("nfc_read"), 4003, out) && out.bytes[34] == 11 && other.submissions == 0, "expired NFC cached");
}
int main(int argc, char **argv) {
    try {
        check(argc == 2, "fixture path argument");
        std::ifstream source(argv[1]);
        check(source.good(), "fixture exists");
        fixture.assign(std::istreambuf_iterator<char>(source), std::istreambuf_iterator<char>());
        happyPath(); wakeStatus(); authentication(); admission(); receiptsAndDriver(); framing(); nfcWire();
        std::cout << "PASS: " << assertions << " assertions; real host HMAC, Python golden bytes, auth/replay/owner/framing.\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL after " << assertions << " assertions: " << error.what() << "\n";
        return 1;
    }
}
