#include "CompanionIntegration/DiagnosticTransport.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace larp;
using namespace larp::diagnostic;
static unsigned checks = 0;
#define CHECK(expression)                                                                          \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(expression)) {                                                                       \
            std::fprintf(stderr, "FAIL line %u: %s\n", __LINE__, #expression);                     \
            std::exit(1);                                                                          \
        }                                                                                          \
    } while (false)
struct Publisher : companion::IAuthorityPublisher {
    companion::AuthorityState state;
    companion::AuthorityUpdate
    advanceAuthority(std::uint32_t p, bool gm, companion::TrustedAuthority &out) noexcept override {
        return state.advance(p, gm, out);
    }
    companion::AuthorityUpdate
    advanceAuthorityIfCurrent(const companion::TrustedAuthority &old, std::uint32_t p, bool gm,
                              companion::TrustedAuthority &out) noexcept override {
        return state.advanceIfCurrent(old, p, gm, out);
    }
    companion::AuthorityUpdate
    closeAuthorityIfCurrent(const companion::TrustedAuthority &old,
                            companion::TrustedAuthority &out) noexcept override {
        return state.closeIfCurrent(old, out);
    }
    void suspendAuthority() noexcept override {
        state.suspend();
    }
    bool readAuthority(companion::TrustedAuthority &out) const noexcept override {
        return state.read(out);
    }
};
static Uuid uuid(unsigned n) {
    Uuid id;
    id.bytes[6] = 0x40;
    id.bytes[8] = 0x80;
    id.bytes[15] = static_cast<std::uint8_t>(n);
    return id;
}
static companion::Identity identity(unsigned n) {
    companion::Identity id;
    id.bytes[0] = static_cast<std::uint8_t>(n);
    return id;
}
struct Port : IPort {
    companion::IdentityAccess *revokeOnAcquire = nullptr;
    bool idle = true, owned = false, light = false, onOk = true, offOk = true, high = true;
    unsigned acquireCalls = 0, renders = 0, offs = 0, releases = 0, nextId = 10;
    bool acquire(Uuid &id, bool &polarity) noexcept override {
        ++acquireCalls;
        if (!idle || owned)
            return false;
        owned = true;
        id = uuid(nextId++);
        polarity = high;
        if (revokeOnAcquire)
            revokeOnAcquire->revokeGameMaster(identity(2));
        return true;
    }
    bool render(std::uint8_t index, const std::array<std::uint8_t, 3> &rgb) noexcept override {
        ++renders;
        light = true;
        return owned && index == 0 && rgb == std::array<std::uint8_t, 3>{{16, 0, 0}} && onOk;
    }
    // This control-frame fixture has no independent LED deadline. An LED
    // request must stay closed instead of treating a black frame as evidence.
    bool armLedOffTimer(std::uint16_t) noexcept override { return false; }
    bool timedOffConfirmed() noexcept override { return false; }
    bool restoreOff() noexcept override {
        ++offs;
        if (offOk)
            light = false;
        return offOk;
    }
    void release() noexcept override {
        ++releases;
        owned = false;
    }
};
struct Fixture {
    Publisher pub;
    companion::IdentityAccess access{pub};
    Port port;
    companion::BoundConnection binding;
    Control control{access, port, identity(1), 123};
    Grant grant;
    Fixture(bool gm = true) {
        CHECK(access.begin(identity(1)) == companion::IdentityAccessOutcome::Initialized);
        if (gm)
            CHECK(access.assignGameMaster(identity(2), 7) ==
                  companion::IdentityAccessOutcome::Assigned);
        CHECK(access.connect(42, binding) == companion::IdentityAccessOutcome::Connected);
        companion::VerifiedPeerEvidence proof;
        proof.peer = identity(2);
        proof.encrypted = proof.mitm = proof.secureConnections = proof.bonded = true;
        CHECK(access.authenticate(binding, proof) == companion::IdentityAccessOutcome::Verified);
    }
    void authorize(std::uint32_t ttl = 30000) {
        CHECK(control.authorize(uuid(1), binding, true, ttl, 100, grant) == Admission::Accepted);
    }
    Request request(Operation operation = Operation::WakeCapture) {
        Request req;
        req.runId = grant.runId;
        req.grantId = grant.grantId;
        req.deviceId = identity(1);
        req.bootId = 123;
        req.stepId = 1;
        req.operation = operation;
        return req;
    }
    Reply reply() {
        Reply result;
        CHECK(control.takeReply(binding, result));
        CHECK(!control.takeReply(binding, result));
        return result;
    }
};

using Bytes = std::array<std::uint8_t, TransportLimits::RequestBytes>;
static void put(Bytes &b, unsigned offset, std::uint64_t value, unsigned count) {
    for (unsigned i = 0; i < count; ++i)
        b[offset + i] = static_cast<std::uint8_t>(value >> (8 * i));
}
static Bytes command(ControlCommand op, unsigned run = 1, unsigned grant = 10) {
    Bytes b = {};
    b[0] = 'D';
    b[1] = 'G';
    b[2] = 1;
    b[3] = static_cast<std::uint8_t>(op);
    b[4] = 1;
    b[8] = 1;
    b[24] = 123;
    if (op != ControlCommand::Capabilities) {
        const auto id = uuid(run);
        std::memcpy(b.data() + 32, id.bytes.data(), 16);
    }
    if (op == ControlCommand::Grant) {
        put(b, 64, 10000, 4);
        b[68] = 1;
    } else if (op == ControlCommand::Cancel) {
        const auto id = uuid(grant);
        std::memcpy(b.data() + 48, id.bytes.data(), 16);
    }
    return b;
}
struct TransportFixture : Fixture {
    ControlTransport transport{access, control, identity(1), 123};
    companion::TrustedAuthority authority;
    std::array<std::uint8_t, TransportLimits::ReplyBytes> bytes = {};
    std::size_t written = 0;
    TransportFixture() {
        CHECK(access.route(binding, authority));
    }
    TransportResult send(const Bytes &b, std::uint64_t received = 100, std::uint64_t now = 100) {
        return transport.receive(b.data(), b.size(), binding, authority, received, now);
    }
    bool take(std::uint64_t now = 100) {
        return transport.takeReply(binding, now, bytes.data(), bytes.size(), written);
    }
};
static void decoder() {
    ControlRequest out;
    out.message = 99;
    auto b = command(ControlCommand::Grant);
    CHECK(decodeControlRequest(b.data(), b.size(), out));
    CHECK(out.message == 1 && out.ttlMs == 10000 && out.run == uuid(1));
    CHECK(out.boot == 123 && out.device == identity(1) && out.consent);
    for (std::size_t n = 0; n < 129; ++n) {
        if (n == b.size())
            continue;
        CHECK(!decodeControlRequest(b.data(), n, out));
        CHECK(out.message == 1);
    }
    CHECK(!decodeControlRequest(nullptr, b.size(), out));
    for (const unsigned offset : {0U, 1U, 2U, 3U, 4U, 8U, 24U, 38U, 40U, 68U}) {
        auto bad = b;
        bad[offset] = 0;
        CHECK(!decodeControlRequest(bad.data(), bad.size(), out));
    }
    for (unsigned ttl : {0U, 60001U, 0xffffffffU}) {
        auto bad = b;
        put(bad, 64, ttl, 4);
        CHECK(!decodeControlRequest(bad.data(), bad.size(), out));
    }
    b[48] = 1;
    CHECK(!decodeControlRequest(b.data(), b.size(), out));
    b = command(ControlCommand::Capabilities);
    b[32] = 1;
    CHECK(!decodeControlRequest(b.data(), b.size(), out));
    b = command(ControlCommand::Cancel);
    b[68] = 1;
    CHECK(!decodeControlRequest(b.data(), b.size(), out));
}
static void lifecycle() {
    TransportFixture f;
    CHECK(f.send(command(ControlCommand::Capabilities)) == TransportResult::Ready);
    CHECK(f.take());
    CHECK(f.written == 88 && f.bytes[3] == 0x81 && f.bytes[65] == 3 && f.bytes[66] == 1);
    CHECK(f.port.acquireCalls == 0 && !f.take());
    CHECK(f.send(command(ControlCommand::Grant)) == TransportResult::Ready);
    CHECK(f.control.granted() && f.port.acquireCalls == 1);
    CHECK(f.send(command(ControlCommand::Capabilities)) == TransportResult::Busy);
    CHECK(f.send(command(ControlCommand::Cancel, 2)) == TransportResult::Busy);
    CHECK(f.control.granted());
    CHECK(f.send(command(ControlCommand::Cancel)) == TransportResult::Ready);
    CHECK(!f.control.granted() && f.port.releases == 1);
    CHECK(f.take() && f.bytes[3] == 0x83 && f.bytes[64] == 0);
    CHECK(f.send(command(ControlCommand::Grant)) == TransportResult::Ready);
    CHECK(f.take() && f.bytes[64] == static_cast<unsigned>(Admission::Consumed));
    CHECK(f.port.acquireCalls == 1);
    CHECK(f.send(command(ControlCommand::Grant, 2), 110, 110) == TransportResult::Ready);
    CHECK(f.take(110) && f.bytes[3] == 0x82 && f.bytes[63] == 11);
    CHECK(f.bytes[68] == 110); // Device issuance, not an app timestamp.
    std::printf("grantHex=");
    for (auto byte : f.bytes)
        std::printf("%02x", byte);
    std::printf("\n");
    f.transport.disconnect(f.binding, 120);
    CHECK(!f.control.granted());
}
static void fences() {
    Fixture noRole(false);
    ControlTransport denied{noRole.access, noRole.control, identity(1), 123};
    companion::TrustedAuthority invented;
    invented.epoch = 1;
    invented.principal = 7;
    invented.gameMaster = true;
    auto deniedRequest = command(ControlCommand::Grant);
    CHECK(denied.receive(deniedRequest.data(), deniedRequest.size(), noRole.binding, invented, 100,
                         100) == TransportResult::Unauthorized);
    CHECK(noRole.port.acquireCalls == 0);
    TransportFixture f;
    auto request = command(ControlCommand::Grant);
    auto bad = request;
    bad[24] = 124;
    CHECK(f.send(bad) == TransportResult::Invalid);
    auto stale = f.authority;
    ++stale.epoch;
    CHECK(f.transport.receive(request.data(), request.size(), f.binding, stale, 100, 100) ==
          TransportResult::Unauthorized);
    CHECK(f.send(request, 100, 2100) == TransportResult::Expired);
    CHECK(f.send(request, 2200, 2100) == TransportResult::Expired);
    CHECK(f.port.acquireCalls == 0);
    CHECK(f.send(request, 2100, 2100) == TransportResult::Ready);
    auto replacement = f.binding;
    ++replacement.token;
    CHECK(!f.transport.takeReply(replacement, 2100, f.bytes.data(), f.bytes.size(), f.written));
    CHECK(!f.transport.takeReply(f.binding, 2100, f.bytes.data(), 87, f.written));
    CHECK(f.take(2100));
    f.access.revokeGameMaster(identity(2));
    f.transport.tick(2101);
    CHECK(!f.control.granted() && !f.port.owned);
    CHECK(f.send(command(ControlCommand::Cancel), 2101, 2101) == TransportResult::Unauthorized);
    TransportFixture queued;
    CHECK(queued.send(request) == TransportResult::Ready);
    queued.access.revokeGameMaster(identity(2));
    CHECK(!queued.take(101));
    CHECK(!queued.port.owned);
    TransportFixture expired;
    request = command(ControlCommand::Grant);
    put(request, 64, 1, 4);
    CHECK(expired.send(request) == TransportResult::Ready);
    CHECK(!expired.take(101) && !expired.control.granted());
    TransportFixture timeout;
    CHECK(timeout.send(command(ControlCommand::Capabilities)) == TransportResult::Ready);
    CHECK(!timeout.take(2100));
    TransportFixture reentry;
    reentry.port.revokeOnAcquire = &reentry.access;
    CHECK(reentry.send(command(ControlCommand::Grant)) == TransportResult::Unauthorized);
    CHECK(!reentry.take() && !reentry.port.owned);
}
int main() {
    decoder();
    lifecycle();
    fences();
    std::printf("Diagnostic transport: %u assertions; synthetic control path, no GATT/hardware.\n",
                checks);
}
