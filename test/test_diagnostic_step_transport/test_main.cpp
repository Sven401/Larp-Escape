#include "CompanionIntegration/DiagnosticStepTransport.h"
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

struct StepFixture : Fixture {
    StepTransport transport{access, control};
    ReplyAssembly assembly{access};
    companion::TrustedAuthority authority;
    std::array<std::uint8_t, StepLimits::RequestBytes> wire = {};
    std::array<std::uint8_t, StepLimits::FrameBytes> frame = {};
    Request req;
    std::size_t written = 0;
    explicit StepFixture(Operation operation = Operation::WakeCapture) {
        authorize();
        CHECK(access.route(binding, authority));
        req = request(operation);
        CHECK(encodeStep(17, req, wire.data(), wire.size()));
    }
    Admission send(std::uint64_t received = 100, std::uint64_t now = 100) {
        return transport.receive(wire.data(), wire.size(), binding, authority, received, now);
    }
    bool next(std::uint64_t now = 201, std::size_t capacity = 128) {
        return transport.nextFrame(binding, now, frame.data(), capacity, written);
    }
    void wake(unsigned count = 64) {
        CHECK(send() == Admission::Accepted);
        for (unsigned i = 0; i < count; ++i)
            control.edge(1 + i % 4, i % 2 == 0, 110 + i);
        control.finishWake(200);
    }
    void startAssembly(std::uint64_t now = 100, std::uint64_t deadline = 1100) {
        CHECK(assembly.begin(17, req, binding, authority, now, deadline));
    }
    AssemblyResult consume(std::uint64_t now = 201) {
        return assembly.receive(frame.data(), written, binding, authority, now);
    }
};
static void codecs() {
    StepFixture f;
    Request out;
    out.stepId = 99;
    std::uint32_t message = 99;
    for (std::size_t n = 0; n < 129; ++n)
        if (n != 80) {
            CHECK(!decodeStep(f.wire.data(), n, message, out));
            CHECK(out.stepId == 99 && message == 99);
        }
    CHECK(!decodeStep(nullptr, 80, message, out));
    for (unsigned offset : {0U, 1U, 2U, 3U, 4U, 8U, 24U, 38U, 40U, 54U, 56U, 64U, 73U}) {
        auto bad = f.wire;
        bad[offset] = 0;
        CHECK(!decodeStep(bad.data(), bad.size(), message, out));
    }
    for (unsigned offset = 74; offset < 80; ++offset) {
        auto bad = f.wire;
        bad[offset] = 1;
        CHECK(!decodeStep(bad.data(), bad.size(), message, out));
    }
    auto bad = f.wire;
    bad[72] = 2;
    CHECK(!decodeStep(bad.data(), bad.size(), message, out));
    CHECK(decodeStep(f.wire.data(), f.wire.size(), message, out));
    CHECK(message == 17 && out.stepId == 1 && out.timeoutMs == 1000);
    StepFixture led(Operation::LedSingle);
    bad = led.wire;
    bad[75] = 17;
    CHECK(!decodeStep(bad.data(), bad.size(), message, out));
}
static void fullWake() {
    StepFixture f;
    f.wake();
    f.startAssembly();
    unsigned frames = 0;
    while (f.next()) {
        CHECK(f.written <= 128);
        const auto status = f.consume();
        ++frames;
        CHECK(status == (frames == 7 ? AssemblyResult::Complete : AssemblyResult::Receiving));
        if (frames == 1)
            CHECK(f.consume() == AssemblyResult::Duplicate);
    }
    CHECK(frames == 7);
    Reply reply;
    CHECK(f.assembly.take(202, reply));
    CHECK(!f.assembly.take(202, reply));
    CHECK(reply.edgeCount == 64 && reply.request.stepId == 1 &&
          reply.request.runId == f.grant.runId);
    CHECK(reply.startedDeviceMs == 100 && reply.endedDeviceMs == 200 && !reply.overflow);
    for (unsigned i = 0; i < 64; ++i)
        CHECK(reply.edges[i].deviceMs == 110 + i && reply.edges[i].channel == 1 + i % 4 &&
              reply.edges[i].rawHigh == (i % 2 == 0));
    std::array<std::uint8_t, StepLimits::ReplyBytes> bytes = {};
    std::size_t n = 0;
    CHECK(encodeStepReply(17, reply, bytes.data(), bytes.size(), n));
    CHECK(n == 744);
    std::printf("wakeHex=");
    for (std::size_t i = 0; i < n; ++i)
        std::printf("%02x", bytes[i]);
    std::printf("\n");
    Reply decoded;
    std::uint32_t id = 0;
    CHECK(decodeStepReply(bytes.data(), n, id, decoded));
    CHECK(id == 17);
    for (unsigned offset : {96U, 97U, 98U, 99U, 100U, 101U, 102U, 103U, 112U, 113U}) {
        auto bad = bytes;
        bad[offset] = 255;
        CHECK(!decodeStepReply(bad.data(), n, id, decoded));
    }
    CHECK(!decodeStepReply(bytes.data(), 745, id, decoded));
    CHECK(!decodeStepReply(bytes.data(), 743, id, decoded));
    CHECK(f.send(203, 203) == Admission::Consumed);
    CHECK(f.port.renders == 0);
}
static void ledAndOverflow() {
    StepFixture led(Operation::LedSingle);
    CHECK(led.send() == Admission::Accepted);
    led.startAssembly();
    CHECK(!led.next(599));
    unsigned count = 0;
    while (led.next(600, 20)) {
        CHECK(led.written <= 20);
        ++count;
        CHECK(led.consume(600) ==
              (count == 13 ? AssemblyResult::Complete : AssemblyResult::Receiving));
    }
    CHECK(count == 13);
    Reply reply;
    CHECK(led.assembly.take(601, reply));
    CHECK(reply.renderCalls == 2 && reply.restoredOff && reply.edgeCount == 0 && !led.port.light);
    std::array<std::uint8_t, StepLimits::ReplyBytes> bytes = {};
    std::size_t n = 0;
    CHECK(encodeStepReply(17, reply, bytes.data(), bytes.size(), n));
    std::printf("ledHex=");
    for (std::size_t i = 0; i < n; ++i)
        std::printf("%02x", bytes[i]);
    std::printf("\n");
    StepFixture overflow;
    overflow.wake(65);
    overflow.startAssembly();
    while (overflow.next())
        overflow.consume();
    CHECK(overflow.assembly.take(202, reply));
    CHECK(reply.edgeCount == 64 && reply.overflow);
}
static void badSegments() {
    for (unsigned mode = 0; mode < 9; ++mode) {
        StepFixture f;
        f.wake();
        f.startAssembly();
        CHECK(f.next());
        if (mode == 0) {
            f.frame[8] = 0xff;
            f.frame[9] = 0xff;
        }
        if (mode == 1)
            f.frame[10] = 1;
        if (mode == 2)
            f.frame[4] = 18;
        if (mode == 3)
            f.frame[3] = 0x82;
        if (mode == 4) {
            CHECK(f.consume() == AssemblyResult::Receiving);
            f.frame[12] ^= 1;
        }
        if (mode == 5) {
            CHECK(f.consume() == AssemblyResult::Receiving);
            CHECK(f.next());
            f.frame[8] -= 10;
        }
        if (mode == 6)
            f.written = 129;
        if (mode == 7)
            f.written = 12;
        if (mode == 8)
            f.frame[12 + 64] = 2; // complete payload has wrong step correlation
        auto status = f.consume();
        if (mode == 8) {
            while (status == AssemblyResult::Receiving && f.next())
                status = f.consume();
        }
        CHECK(status == AssemblyResult::Rejected);
        Reply out;
        out.request.stepId = 99;
        CHECK(!f.assembly.take(202, out));
        CHECK(out.request.stepId == 99);
        CHECK(f.consume() == AssemblyResult::Closed);
    }
}
static void fencesAndDeadlines() {
    StepFixture expired;
    CHECK(expired.send(100, 1100) == Admission::Expired);
    CHECK(!expired.control.pending());
    StepFixture future;
    CHECK(future.send(101, 100) == Admission::Expired);
    StepFixture delay(Operation::LedSingle);
    CHECK(delay.send(100, 600) == Admission::Expired);
    CHECK(delay.port.renders == 0);
    StepFixture queued;
    CHECK(queued.send(100, 900) == Admission::Accepted);
    queued.transport.tick(1100);
    CHECK(!queued.control.pending() && !queued.next(1100));
    StepFixture later(Operation::LedSingle);
    CHECK(later.send(100, 300) == Admission::Accepted);
    later.startAssembly();
    CHECK(!later.next(799));
    CHECK(later.next(800));
    CHECK(later.consume(800) == AssemblyResult::Complete);
    Reply out;
    CHECK(later.assembly.take(801, out));
    CHECK(out.startedDeviceMs == 300 && out.endedDeviceMs == 800);
    for (unsigned mode = 0; mode < 5; ++mode) {
        StepFixture f;
        f.wake();
        f.startAssembly();
        CHECK(f.next());
        CHECK(f.consume() == AssemblyResult::Receiving);
        if (mode == 0) {
            f.access.revokeGameMaster(identity(2));
            CHECK(!f.next(202));
            CHECK(f.consume(202) == AssemblyResult::Expired);
        }
        if (mode == 1) {
            f.transport.disconnect(f.binding, 202);
            CHECK(!f.next(202));
            f.assembly.retire();
            CHECK(f.consume(202) == AssemblyResult::Closed);
        }
        if (mode == 2) {
            CHECK(f.consume(1100) == AssemblyResult::Expired);
            CHECK(!f.next(1100));
        }
        if (mode == 3) {
            CHECK(f.consume(200) == AssemblyResult::Expired);
        }
        if (mode == 4) {
            auto other = f.binding;
            ++other.token;
            CHECK(f.assembly.receive(f.frame.data(), f.written, other, f.authority, 202) ==
                  AssemblyResult::Rejected);
        }
        CHECK(!f.assembly.take(1101, out));
    }
    StepFixture after;
    after.wake();
    after.startAssembly();
    while (after.next())
        after.consume();
    after.access.revokeGameMaster(identity(2));
    CHECK(!after.assembly.take(202, out));
    StepFixture changedGrant;
    changedGrant.wake();
    CHECK(changedGrant.next());
    changedGrant.control.cancel(changedGrant.binding, 202);
    Grant replacement;
    CHECK(changedGrant.control.authorize(uuid(2), changedGrant.binding, true, 10000, 203,
                                         replacement) == Admission::Accepted);
    CHECK(!changedGrant.next(203));
    StepFixture cancel(Operation::LedSingle);
    CHECK(cancel.send() == Admission::Accepted);
    cancel.startAssembly();
    cancel.control.cancel(cancel.binding, 200);
    CHECK(cancel.next(200));
    CHECK(cancel.consume(200) == AssemblyResult::Complete);
    CHECK(cancel.assembly.take(200, out));
    CHECK(out.outcome == Outcome::Cancelled && out.reason == Reason::Cancelled && out.restoredOff);
    StepFixture capacity;
    capacity.wake();
    CHECK(!capacity.next(201, 12));
    CHECK(capacity.next(201, 13));
    CHECK(capacity.written == 13);
    StepFixture role;
    auto auth = role.authority;
    ++auth.epoch;
    CHECK(role.transport.receive(role.wire.data(), role.wire.size(), role.binding, auth, 100,
                                 100) == Admission::PermissionDenied);
    CHECK(!role.control.pending());
}
static void ownerTimeBoundaries() {
    Fixture led;
    led.authorize();
    CHECK(led.control.submit(led.request(Operation::LedSingle), led.binding, 100) ==
          Admission::Accepted);
    led.control.tick(599);
    CHECK(led.control.pending());
    led.control.tick(600);
    CHECK(!led.control.pending());
    Reply result = led.reply();
    CHECK(result.outcome == Outcome::Completed && result.endedDeviceMs == 600);
    std::array<std::uint8_t, StepLimits::ReplyBytes> wire = {};
    std::size_t written = 0;
    CHECK(encodeStepReply(17, result, wire.data(), wire.size(), written));
    auto unchanged = wire;
    result.endedDeviceMs = 599;
    CHECK(!encodeStepReply(17, result, wire.data(), wire.size(), written));
    CHECK(!written && wire == unchanged);
    result.endedDeviceMs = 1100;
    CHECK(!encodeStepReply(17, result, wire.data(), wire.size(), written));
    CHECK(!written && wire == unchanged);
    Fixture wake;
    wake.authorize();
    CHECK(wake.control.submit(wake.request(), wake.binding, 100) == Admission::Accepted);
    wake.control.tick(1100);
    result = wake.reply();
    CHECK(result.outcome == Outcome::Rejected && result.reason == Reason::Expired);
    // Internal owner timeout is retained locally; it is too late for a v1 reply.
    CHECK(!encodeStepReply(17, result, wire.data(), wire.size(), written));
    CHECK(!written && wire == unchanged);
}
static void independentTimeBoundaries(const char *path) {
    auto *file = std::fopen(path, "rb");
    CHECK(file != nullptr);
    unsigned vectors = 0;
    for (;;) {
        const int expected = std::fgetc(file);
        if (expected == EOF)
            break;
        CHECK(expected == 0 || expected == 1);
        const int low = std::fgetc(file), high = std::fgetc(file);
        CHECK(low != EOF && high != EOF);
        const std::size_t length = static_cast<unsigned>(low | (high << 8));
        std::array<std::uint8_t, StepLimits::ReplyBytes> bytes = {};
        CHECK(length >= StepLimits::ReplyHeader && length <= bytes.size());
        CHECK(std::fread(bytes.data(), 1, length, file) == length);
        Reply result;
        result.request.stepId = 999;
        std::uint32_t message = 999;
        CHECK(decodeStepReply(bytes.data(), length, message, result) == (expected != 0));
        if (!expected)
            CHECK(message == 999 && result.request.stepId == 999);
        StepFixture fixture;
        Request request;
        CHECK(decodeStep(bytes.data(), StepLimits::RequestBytes, message, request));
        CHECK(fixture.assembly.begin(message, request, fixture.binding, fixture.authority, 100,
                                     1100));
        AssemblyResult status = AssemblyResult::Receiving;
        for (std::size_t at = 0; at < length;) {
            const std::size_t count = length - at > 116 ? 116 : length - at;
            auto &frame = fixture.frame;
            frame[0] = 'D';
            frame[1] = 'G';
            frame[2] = 1;
            frame[3] = 0x84;
            for (unsigned i = 0; i < 4; ++i)
                frame[4 + i] = static_cast<std::uint8_t>(message >> (8 * i));
            frame[8] = static_cast<std::uint8_t>(length);
            frame[9] = static_cast<std::uint8_t>(length >> 8);
            frame[10] = static_cast<std::uint8_t>(at);
            frame[11] = static_cast<std::uint8_t>(at >> 8);
            std::memcpy(frame.data() + 12, bytes.data() + at, count);
            status = fixture.assembly.receive(frame.data(), count + 12, fixture.binding,
                                              fixture.authority, 201);
            at += count;
            if (at < length)
                CHECK(status == AssemblyResult::Receiving);
        }
        CHECK(status == (expected ? AssemblyResult::Complete : AssemblyResult::Rejected));
        CHECK(fixture.assembly.take(202, result) == (expected != 0));
        ++vectors;
    }
    CHECK(std::fclose(file) == 0);
    CHECK(vectors == 10);
}
int main(int argc, char **argv) {
    codecs();
    fullWake();
    ledAndOverflow();
    badSegments();
    fencesAndDeadlines();
    const unsigned baseline = checks;
    CHECK(argc == 2);
    ownerTimeBoundaries();
    independentTimeBoundaries(argv[1]);
    std::printf(
        "Diagnostic step transport: %u assertions; synthetic components, no GATT/hardware.\n",
        checks);
    std::printf("Baseline %u; new boundary assertions %u.\n", baseline, checks - baseline);
    std::printf("Host sizes: step transport %u, reply assembly %u bytes.\n",
                static_cast<unsigned>(sizeof(StepTransport)),
                static_cast<unsigned>(sizeof(ReplyAssembly)));
}
