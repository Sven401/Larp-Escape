#include "NfcProbe.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
using namespace nfcprobe;
using Bytes = std::vector<std::uint8_t>;

Bytes unhex(const std::string &s) {
    Bytes out;
    for (std::size_t i = 0; i < s.size(); i += 2) out.push_back(std::uint8_t(std::stoul(s.substr(i, 2), nullptr, 16)));
    return out;
}
const Bytes Old = unhex("019728350101000000000000000000010001010000F4617B57");
const Bytes New = unhex("01972835010200000000000000000001000101000006D5B37E");
Bytes ndef(const Bytes &payload, bool extra = false) {
    const std::string mime = "application/vnd.yourapp.tagdata";
    Bytes b = {std::uint8_t(extra ? 0x92 : 0xd2), std::uint8_t(mime.size()), std::uint8_t(payload.size())};
    b.insert(b.end(), mime.begin(), mime.end()); b.insert(b.end(), payload.begin(), payload.end());
    if (extra) { const Bytes record = {0x51, 1, 4, 'T', 2, 'e', 'n', 'X'}; b.insert(b.end(), record.begin(), record.end()); }
    return b;
}
Bytes response(std::uint8_t command, Bytes payload) {
    Bytes f = {0, 0, 0xff, std::uint8_t(payload.size() + 2), std::uint8_t(0 - (payload.size() + 2)), 0xd5, command};
    f.insert(f.end(), payload.begin(), payload.end());
    std::uint8_t sum = 0; for (std::size_t i = 5; i < f.size(); ++i) sum = std::uint8_t(sum + f[i]);
    f.push_back(std::uint8_t(0 - sum)); f.push_back(0); return f;
}
void frameTests() {
    auto f = response(3, {0x32, 1, 6, 7}); std::uint8_t out[32] = {}; std::size_t size = 999;
    assert(decodeFrame(f.data(), f.size(), 3, out, sizeof(out), size) && size == 4 && out[0] == 0x32);
    for (std::size_t i = 0; i < f.size(); ++i) {
        auto changed = f; changed[i] ^= 1; size = 999;
        assert(!decodeFrame(changed.data(), changed.size(), 3, out, sizeof(out), size) && size == 0);
    }
    for (std::size_t n = 0; n < f.size(); ++n) assert(!decodeFrame(f.data(), n, 3, out, sizeof(out), size));
    assert(!decodeFrame(f.data(), f.size(), 4, out, sizeof(out), size));
    assert(!decodeFrame(f.data(), f.size(), 3, out, 3, size));
    const Bytes ack = {0, 0, 0xff, 0, 0xff, 0};
    assert(!decodeFrame(ack.data(), ack.size(), 3, out, sizeof(out), size));
    std::uint8_t frame[32]; const std::uint8_t command[] = {2};
    assert(encodeCommand(command, 1, frame, sizeof(frame)) == 9);
    const Bytes expected = {0, 0, 0xff, 2, 0xfe, 0xd4, 2, 0x2a, 0};
    assert(std::equal(expected.begin(), expected.end(), frame));
    assert(!encodeCommand(command, 1, frame, 8));
    Target target;
    const Bytes targetBytes = {1, 1, 0, 4, 0x20, 4, 8, 1, 2, 3, 2, 0x75};
    assert(decodeTarget(targetBytes.data(), targetBytes.size(), target) == Result::Ok && target.uidSize == 4);
    assert(decodeTarget(targetBytes.data(), targetBytes.size() - 1, target) == Result::Error && target.uidSize == 0);
    const Bytes none = {0}; assert(decodeTarget(none.data(), none.size(), target) == Result::NoTarget);
}
void codecTests() {
    assert(crc32(reinterpret_cast<const std::uint8_t *>("123456789"), 9) == 0xcbf43926U);
    assert(crc32(Old.data(), 21) == 0x577b61f4U);
    for (bool extra : {false, true}) {
        auto b = ndef(Old, extra); assert(isPrepared(b.data(), b.size()));
        Snapshot s; s.rawSize = s.size = std::uint16_t(b.size()); std::memcpy(s.bytes, b.data(), b.size());
        assert(prepareChange(s)); const auto expected = ndef(New, extra);
        assert(std::equal(expected.begin(), expected.end(), s.bytes)); assert(!prepareChange(s));
        for (std::size_t n = 0; n < b.size(); ++n) assert(!isPrepared(b.data(), n));
        for (std::size_t i = 33; i < 58; ++i) { auto bad = b; bad[i] ^= 1; assert(!isPrepared(bad.data(), bad.size())); }
    }
    auto chunked = ndef(Old); chunked[0] |= 0x20; assert(!isPrepared(chunked.data(), chunked.size()));
    auto noEnd = ndef(Old); noEnd[0] &= ~0x40; assert(!isPrepared(noEnd.data(), noEnd.size()));
    auto duplicate = ndef(Old); duplicate[0] &= ~0x40; auto second = ndef(Old); second[0] &= ~0x80;
    duplicate.insert(duplicate.end(), second.begin(), second.end()); assert(!isPrepared(duplicate.data(), duplicate.size()));
}

// Simulates RF-visible tag memory and status words. It does not use the production encoders.
class Tag final : public Channel {
 public:
    explicit Tag(bool type4, unsigned model = 213, bool extra = false) : t4(type4), model(model), data(1100, 0) {
        const auto message = ndef(Old, extra);
        if (t4) {
            data[0] = 0; data[1] = std::uint8_t(message.size()); std::copy(message.begin(), message.end(), data.begin() + 2);
        } else {
            const std::uint8_t uid[] = {4, 1, 2, 3, 4, 5, 6};
            std::memcpy(data.data(), uid, 3); std::memcpy(data.data() + 4, uid + 3, 4);
            data[12] = 0xe1; data[13] = 0x10; data[14] = model == 213 ? 0x12 : model == 215 ? 0x3e : 0x6d;
            std::size_t start = 16;
            if (model == 213) { const Bytes lock = {1, 3, 0xa0, 0x0c, 0x34}; std::copy(lock.begin(), lock.end(), data.begin() + start); start += lock.size(); }
            tlv = start; data[start++] = 3; data[start++] = std::uint8_t(message.size());
            std::copy(message.begin(), message.end(), data.begin() + start); data[start + message.size()] = 0xfe;
            const auto config = lockPage() * 4; data[config + 3] = 0xbd; data[config + 7] = 0xff;
        }
    }
    std::uint32_t now() const override { return ticks; }
    unsigned lockPage() const { return model == 213 ? 40 : model == 215 ? 130 : 226; }
    bool command(const std::uint8_t *b, std::size_t n, std::uint8_t *out, std::size_t cap, std::size_t &received) override {
        ++ticks; ++commands; received = 0;
        if (failRead && !writes) return false;
        Bytes reply;
        if (b[0] == 0x4a) {
            if (noTarget) reply = {0};
            else if (t4) reply = {1, 1, 0, 4, 0x20, 4, 8, 1, 2, 3, 2, 0x75};
            else reply = {1, 1, 0, 0x44, 0, 7, 4, 1, 2, 3, 4, 5, 6};
        } else if (b[0] == 0x42) {
            assert(!t4 && n == 2 && b[1] == 0x60);
            ++rounds; if (changeBefore && rounds == 2) data[tlv + 2 + 33 + 5] ^= 4;
            reply = {0, 0, 4, std::uint8_t(unsupported ? 5 : 4), 2, 1, 0,
                     std::uint8_t(model == 213 ? 0x0f : model == 215 ? 0x11 : 0x13), 3};
        } else {
            assert(n >= 3 && b[0] == 0x40 && b[1] == 1); b += 2; n -= 2;
            reply.push_back(0);
            if (t4) {
                assert(n >= 5 && b[0] == 0);
                const auto offset = (unsigned(b[2]) << 8) | b[3];
                if (b[1] == 0xa4) {
                    if (b[2] == 4) selected = 0;
                    else { assert(n == 7); selected = (unsigned(b[5]) << 8) | b[6];
                        if (selected == 0xe104) { ++rounds; if (changeBefore && rounds == 2) data[2 + 33 + 5] ^= 4; }
                    }
                } else if (b[1] == 0xb0) {
                    const auto count = b[4];
                    if (selected == 0xe103) {
                        Bytes cc = {0, 15, 0x20, 0, 64, 0, 64, 4, 6, 0xe1, 4, 4, 2, 0, std::uint8_t(readonly ? 0xff : 0)};
                        if (badCapacity) { cc[11] = 0; cc[12] = 20; }
                        assert(offset + count <= cc.size()); reply.insert(reply.end(), cc.begin() + offset, cc.begin() + offset + count);
                    } else { assert(selected == 0xe104 && offset + count <= data.size());
                        reply.insert(reply.end(), data.begin() + offset, data.begin() + offset + count); }
                } else if (b[1] == 0xd6) {
                    assert(selected == 0xe104 && n == std::size_t(5 + b[4]) && offset + b[4] <= 1026);
                    ++writes;
                    if (writes == failWrite) return false;
                    if (rfWriteError) { reply[0] = 1; return finish(reply, out, cap, received); }
                    std::copy(b + 5, b + n, data.begin() + offset);
                    if (corruptReadback && offset == 0 && b[6]) data[35] ^= 1;
                } else assert(false);
                reply.push_back(0x90); reply.push_back(0);
            } else {
                if (b[0] == 0x30) {
                    assert(n == 2); const auto offset = unsigned(b[1]) * 4;
                    reply.insert(reply.end(), data.begin() + offset, data.begin() + offset + 16);
                    if (readonly && b[1] == 0) reply[1 + 15] = 0x0f;
                    if (badCapacity && b[1] == 0) reply[1 + 14] = 0xff;
                } else if (b[0] == 0xa2) {
                    assert(n == 6 && b[1] >= 4 && b[1] < lockPage());
                    assert(unsigned(b[1]) * 4 < 16 + unsigned(data[14]) * 8);
                    ++writes; if (writes == failWrite) return false;
                    if (rfWriteError) { reply[0] = 1; return finish(reply, out, cap, received); }
                    std::copy(b + 2, b + 6, data.begin() + unsigned(b[1]) * 4);
                    if (corruptReadback && data[tlv + 1]) data[tlv + 2 + 33] ^= 1;
                } else assert(false);
            }
        }
        return finish(reply, out, cap, received);
    }
    bool finish(const Bytes &b, std::uint8_t *out, std::size_t cap, std::size_t &received) {
        if (b.size() > cap) return false;
        std::copy(b.begin(), b.end(), out); received = b.size(); return true;
    }
    bool t4; unsigned model, selected = 0; Bytes data; std::size_t tlv = 0;
    unsigned ticks = 0, writes = 0, rounds = 0, commands = 0, failWrite = 0;
    bool noTarget = false, readonly = false, badCapacity = false, unsupported = false,
         changeBefore = false, corruptReadback = false, rfWriteError = false, failRead = false;
};

unsigned cases = 0;
void transactionTests(bool type4, unsigned model) {
    for (bool extra : {false, true}) {
        Tag tag(type4, model, extra); Session session(tag); Snapshot s;
        const auto before = tag.data;
        assert(session.read(s) == Result::Ok && tag.writes == 0 && isPrepared(s.bytes + s.offset, s.size));
        assert(session.change(s) == Result::Verified && tag.writes > 0);
        const auto expected = ndef(New, extra);
        assert(s.size == expected.size() && std::equal(expected.begin(), expected.end(), s.bytes + s.offset));
        const auto count = tag.writes;
        assert(session.change(s) == Result::NotPrepared && tag.writes == count);
        // Every modified physical byte belongs to the expected NDEF payload/CRC.
        const auto begin = type4 ? 2U : unsigned(tag.tlv + 2);
        for (std::size_t i = 0; i < before.size(); ++i) {
            if (before[i] != tag.data[i]) assert(i >= begin && i < begin + expected.size());
        }
        ++cases;
    }
    for (unsigned failure = 1; failure <= 3; ++failure) {
        Tag tag(type4, model); tag.failWrite = failure; Session session(tag); Snapshot s;
        assert(session.change(s) == Result::Unknown && tag.writes == failure && s.size == 0); ++cases;
    }
    { Tag tag(type4, model); tag.readonly = true; Session session(tag); Snapshot s;
      assert(session.change(s) == Result::ReadOnly && tag.writes == 0); ++cases; }
    { Tag tag(type4, model); tag.badCapacity = true; Session session(tag); Snapshot s;
      assert(session.change(s) == Result::Unsupported && tag.writes == 0); ++cases; }
    { Tag tag(type4, model); tag.changeBefore = true; Session session(tag); Snapshot s;
      assert(session.change(s) == Result::Changed && tag.writes == 0); ++cases; }
    { Tag tag(type4, model); tag.corruptReadback = true; Session session(tag); Snapshot s;
      assert(session.change(s) == Result::Unknown && tag.writes > 0); ++cases; }
    { Tag tag(type4, model); tag.rfWriteError = true; Session session(tag); Snapshot s;
      assert(session.change(s) == Result::Unknown && tag.writes == 1); ++cases; }
    { Tag tag(type4, model); tag.failRead = true; Session session(tag); Snapshot s;
      assert(session.change(s) == Result::Error && tag.writes == 0); ++cases; }
    { Tag tag(type4, model); tag.noTarget = true; Session session(tag); Snapshot s;
      assert(session.change(s) == Result::NoTarget && tag.writes == 0); ++cases; }
    if (!type4) {
        { Tag tag(false, model); tag.unsupported = true; Session session(tag); Snapshot s;
          assert(session.change(s) == Result::Unsupported && tag.writes == 0); ++cases; }
        for (unsigned offset : {10U, 11U, 4 * (model == 213 ? 40U : model == 215 ? 130U : 226U),
                                4 * (model == 213 ? 40U : model == 215 ? 130U : 226U) + 1,
                                4 * (model == 213 ? 40U : model == 215 ? 130U : 226U) + 2}) {
            Tag tag(false, model); tag.data[offset] = 1; Session session(tag); Snapshot s;
            assert(session.change(s) == Result::ReadOnly && tag.writes == 0); ++cases;
        }
        { Tag tag(false, model); tag.data[4 * tag.lockPage() + 7] = 4; Session session(tag); Snapshot s;
          assert(session.change(s) == Result::ReadOnly && tag.writes == 0); ++cases; }
        { Tag tag(false, model); tag.data[4 * tag.lockPage() + 4] = 0x40; Session session(tag); Snapshot s;
          assert(session.change(s) == Result::ReadOnly && tag.writes == 0); ++cases; }
        { Tag tag(false, model); tag.data[tag.tlv] = 2; Session session(tag); Snapshot s;
          assert(session.change(s) == Result::Unsupported && tag.writes == 0); ++cases; }
        { Tag tag(false, model); tag.data[tag.tlv + 1] = 0xff; Session session(tag); Snapshot s;
          assert(session.change(s) == Result::Unsupported && tag.writes == 0); ++cases; }
    }
}
int main() {
    frameTests(); codecTests();
    transactionTests(true, 0);
    for (unsigned model : {213U, 215U, 216U}) transactionTests(false, model);
    std::cout << "NFC frame/target/CRC/NDEF checks and " << cases << " transaction cases passed; no hardware\n";
}
