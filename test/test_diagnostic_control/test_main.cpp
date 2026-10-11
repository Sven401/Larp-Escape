#include "CompanionIntegration/DiagnosticControl.h"
#include <cstdio>
#include <cstdlib>
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
    bool armOk = true, timerArmed = false, timerFault = false, timerConfirmed = false,
         expireOnArm = false;
    std::uint32_t timerNow = 0, timerDue = 0;
    unsigned acquireCalls = 0, armCalls = 0, renders = 0, offs = 0, releases = 0,
             nextId = 10;
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
    bool armLedOffTimer(std::uint16_t holdMs) noexcept override {
        ++armCalls;
        if (!owned || !armOk || timerFault || timerArmed || holdMs != 500)
            return false;
        timerDue = timerNow + holdMs;
        timerArmed = true;
        timerConfirmed = false;
        if (expireOnArm)
            advanceTimer(timerDue);
        return true;
    }
    bool render(std::uint8_t index, const std::array<std::uint8_t, 3> &rgb) noexcept override {
        ++renders;
        const bool accepted = owned && timerArmed && index == 0 &&
                              rgb == std::array<std::uint8_t, 3>{{16, 0, 0}} && onOk;
        if (accepted)
            light = true;
        return accepted;
    }
    bool restoreOff() noexcept override {
        ++offs;
        timerArmed = false;
        if (offOk)
            light = false;
        return offOk && !timerFault;
    }
    bool timedOffConfirmed() noexcept override {
        return timerConfirmed && !timerFault && !light;
    }
    void advanceTimer(std::uint32_t now) noexcept {
        timerNow = now;
        if (timerArmed && static_cast<std::int32_t>(timerNow - timerDue) >= 0) {
            if (timerNow != timerDue)
                timerFault = true;
            if (restoreOff())
                timerConfirmed = true;
            else
                timerFault = true;
        }
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
static void testRightsAndConsent() {
    Fixture noRole(false);
    Grant sentinel;
    sentinel.runId = uuid(99);
    CHECK(noRole.control.authorize(uuid(1), noRole.binding, true, 1000, 100, sentinel) ==
          Admission::PermissionDenied);
    CHECK(sentinel.runId == uuid(99));
    CHECK(noRole.port.acquireCalls == 0);
    Fixture f;
    CHECK(f.control.authorize(uuid(1), f.binding, false, 1000, 100, sentinel) ==
          Admission::PermissionDenied);
    CHECK(f.port.acquireCalls == 0);
    auto other = f.binding;
    ++other.token;
    CHECK(f.control.authorize(uuid(1), other, true, 1000, 100, sentinel) ==
          Admission::PermissionDenied);
    f.port.idle = false;
    CHECK(f.control.authorize(uuid(1), f.binding, true, 1000, 100, sentinel) == Admission::Busy);
    CHECK(!f.control.granted());
}
static void testGrantBoundsAndHistory() {
    Fixture f;
    CHECK(f.control.authorize(uuid(1), f.binding, true, 60001, 100, f.grant) == Admission::Invalid);
    CHECK(f.control.authorize(uuid(1), f.binding, true, 0, 100, f.grant) == Admission::Invalid);
    CHECK(f.control.authorize(Uuid{}, f.binding, true, 1000, 100, f.grant) == Admission::Invalid);
    f.authorize(1000);
    CHECK(f.grant.issuedDeviceMs == 100 && f.grant.expiresDeviceMs == 1100);
    f.control.tick(1100);
    CHECK(!f.control.granted() && !f.port.owned);
    CHECK(f.control.authorize(uuid(1), f.binding, true, 1000, 1100, f.grant) ==
          Admission::Consumed);
    f.port.nextId = 10;
    CHECK(f.control.authorize(uuid(2), f.binding, true, 1000, 1100, f.grant) ==
          Admission::OwnerError);
    CHECK(!f.port.owned);
    f.port.nextId = 11;
    for (unsigned i = 2; i <= 128; ++i) {
        CHECK(f.control.authorize(uuid(i), f.binding, true, 1, 1100 + i, f.grant) ==
              Admission::Accepted);
        f.control.retire(1100 + i);
    }
    CHECK(f.control.authorize(uuid(129), f.binding, true, 1, 1300, f.grant) == Admission::Capacity);
}
static void testWakeCorrelationAndForeignEdges() {
    Fixture f;
    f.authorize();
    auto req = f.request();
    CHECK(f.control.submit(req, f.binding, 110) == Admission::Accepted);
    f.control.edge(1, true, 120);
    f.control.edge(3, true, 121);
    f.control.edge(1, false, 130);
    f.control.finishWake(140);
    auto reply = f.reply();
    CHECK(reply.outcome == Outcome::Completed && reply.reason == Reason::None);
    CHECK(reply.request.runId == req.runId && reply.request.grantId == req.grantId &&
          reply.request.deviceId == req.deviceId && reply.request.bootId == req.bootId &&
          reply.request.stepId == req.stepId);
    CHECK(reply.startedDeviceMs == 110 && reply.endedDeviceMs == 140 && reply.edgeCount == 3);
    CHECK(reply.edges[1].channel == 3 && reply.edges[0].active && !reply.edges[2].active);
    CHECK(f.port.renders == 0 && f.port.offs == 0 && f.port.owned);
    CHECK(f.control.submit(req, f.binding, 150) == Admission::Consumed);
    req.stepId = 2;
    CHECK(f.control.submit(req, f.binding, 150) == Admission::Accepted);
}
static void testLowPolarityOverflowAndInvalidChannel() {
    Fixture f;
    f.port.high = false;
    f.authorize();
    auto req = f.request();
    CHECK(f.control.submit(req, f.binding, 110) == Admission::Accepted);
    f.control.edge(1, false, 120);
    f.control.edge(1, true, 130);
    for (unsigned i = 0; i < 70; ++i)
        f.control.edge(4, true, 140 + i);
    f.control.finishWake(220);
    auto result = f.reply();
    CHECK(!result.activeHigh && result.edges[0].active && !result.edges[1].active);
    CHECK(result.overflow && result.edgeCount == 64);
    req.stepId = 2;
    CHECK(f.control.submit(req, f.binding, 230) == Admission::Accepted);
    f.control.edge(5, true, 240);
    CHECK(f.reply().reason == Reason::OwnerError);
}
static void testLedHoldAndDuplicate() {
    Fixture f;
    f.authorize();
    auto req = f.request(Operation::LedSingle);
    CHECK(f.control.submit(req, f.binding, 110) == Admission::Accepted);
    CHECK(f.port.light && f.port.renders == 1);
    CHECK(f.control.submit(req, f.binding, 120) == Admission::Busy);
    f.control.tick(609);
    CHECK(f.port.light && f.control.pending());
    f.port.advanceTimer(500);
    f.control.tick(610);
    CHECK(!f.port.light && !f.control.pending());
    CHECK(f.control.submit(req, f.binding, 620) == Admission::Busy);
    auto other = f.binding;
    ++other.token;
    Reply unchanged;
    unchanged.request.stepId = 99;
    CHECK(!f.control.takeReply(other, unchanged) && unchanged.request.stepId == 99);
    auto result = f.reply();
    CHECK(result.outcome == Outcome::Completed && result.renderCalls == 2 && result.restoredOff);
    CHECK(f.control.submit(req, f.binding, 630) == Admission::Consumed);
    CHECK(f.port.renders == 1 && f.port.offs == 2);
}
static void testLedOwnerTimerCutsLightWithoutControlTick() {
    Fixture f;
    f.authorize();
    auto req = f.request(Operation::LedSingle);
    CHECK(f.control.submit(req, f.binding, 110) == Admission::Accepted);
    CHECK(f.port.armCalls == 1 && f.port.light && f.control.pending());
    f.port.advanceTimer(499);
    CHECK(f.port.light && f.port.offs == 0);
    f.port.advanceTimer(500);
    CHECK(!f.port.light && f.port.offs == 1 && f.control.pending());
    // Completion remains a separate owner action and cannot relight the LED.
    f.control.tick(610);
    CHECK(!f.port.light && !f.control.pending());
    CHECK(f.reply().restoredOff);
}
static void testTimerArmFailurePreventsRenderAndLatchesOwner() {
    Fixture f;
    f.authorize();
    f.port.armOk = false;
    auto req = f.request(Operation::LedSingle);
    CHECK(f.control.submit(req, f.binding, 110) == Admission::Accepted);
    CHECK(f.port.armCalls == 1 && f.port.renders == 0 && !f.port.light);
    CHECK(f.reply().outcome == Outcome::Failed);
    f.control.retire(120);
    CHECK(f.port.owned && f.port.releases == 0);
    CHECK(f.control.authorize(uuid(2), f.binding, true, 1000, 130, f.grant) ==
          Admission::OwnerError);
}
static void testTimerOffFailureRemainsFencedAfterLaterControlTick() {
    Fixture f;
    f.authorize();
    f.port.offOk = false;
    auto req = f.request(Operation::LedSingle);
    CHECK(f.control.submit(req, f.binding, 110) == Admission::Accepted);
    f.port.advanceTimer(500);
    CHECK(f.port.timerFault && f.port.light && f.control.pending());
    f.port.offOk = true;
    f.control.tick(610);
    auto result = f.reply();
    CHECK(result.outcome == Outcome::Failed && result.reason == Reason::OwnerError &&
          !result.restoredOff);
    f.control.retire(620);
    CHECK(f.port.owned && f.port.releases == 0);
}
static void testMissingTimerCallbackCannotBecomeCompletedOnLateTick() {
    Fixture f;
    f.authorize();
    auto req = f.request(Operation::LedSingle);
    CHECK(f.control.submit(req, f.binding, 110) == Admission::Accepted);
    // No independent owner timer event is delivered.
    f.control.tick(610);
    auto result = f.reply();
    CHECK(result.outcome == Outcome::Failed && result.reason == Reason::OwnerError);
    CHECK(!f.port.light);
    f.control.retire(620);
    CHECK(f.port.owned && f.port.releases == 0);
}
static void testLateTimerCallbackCannotConfirmDeadline() {
    Fixture f;
    f.authorize();
    auto req = f.request(Operation::LedSingle);
    CHECK(f.control.submit(req, f.binding, 110) == Admission::Accepted);
    f.port.advanceTimer(501);
    CHECK(!f.port.light && f.port.timerFault && !f.port.timedOffConfirmed());
    f.control.tick(611);
    auto result = f.reply();
    CHECK(result.outcome == Outcome::Failed && result.reason == Reason::OwnerError);
    f.control.retire(620);
    CHECK(f.port.owned);
}
static void testExpiredTimerCannotBeFollowedByLateRender() {
    Fixture f;
    f.authorize();
    f.port.expireOnArm = true;
    auto req = f.request(Operation::LedSingle);
    CHECK(f.control.submit(req, f.binding, 110) == Admission::Accepted);
    CHECK(!f.port.light && f.port.offs == 2 && f.port.renders == 1);
    auto result = f.reply();
    CHECK(result.outcome == Outcome::Failed && result.reason == Reason::OwnerError);
}
static void testUnsupportedAndStale() {
    Fixture f;
    f.authorize(1100);
    auto req = f.request();
    auto bad = req;
    bad.schemaVersion = 2;
    CHECK(f.control.submit(bad, f.binding, 110) == Admission::Invalid);
    bad = req;
    bad.deviceId = identity(3);
    CHECK(f.control.submit(bad, f.binding, 110) == Admission::Invalid);
    bad = req;
    ++bad.bootId;
    CHECK(f.control.submit(bad, f.binding, 110) == Admission::Invalid);
    bad = req;
    bad.runId = uuid(2);
    CHECK(f.control.submit(bad, f.binding, 110) == Admission::Invalid);
    bad = req;
    bad.grantId = uuid(2);
    CHECK(f.control.submit(bad, f.binding, 110) == Admission::Invalid);
    bad = req;
    bad.timeoutMs = 999;
    CHECK(f.control.submit(bad, f.binding, 110) == Admission::Invalid);
    bad = req;
    bad.timeoutMs = 10001;
    CHECK(f.control.submit(bad, f.binding, 110) == Admission::Invalid);
    bad = req;
    bad.channel = 2;
    CHECK(f.control.submit(bad, f.binding, 110) == Admission::Invalid);
    bad = req;
    bad.operation = static_cast<Operation>(99);
    CHECK(f.control.submit(bad, f.binding, 110) == Admission::Invalid);
    bad = f.request(Operation::LedSingle);
    bad.rgb[0] = 17;
    CHECK(f.control.submit(bad, f.binding, 110) == Admission::Invalid);
    bad = f.request(Operation::LedSingle);
    bad.index = 1;
    CHECK(f.control.submit(bad, f.binding, 110) == Admission::Invalid);
    bad = f.request(Operation::LedSingle);
    bad.holdMs = 501;
    CHECK(f.control.submit(bad, f.binding, 110) == Admission::Invalid);
    CHECK(f.control.submit(req, f.binding, 201) == Admission::Expired);
    CHECK(f.port.renders == 0 && !f.control.pending());
}
static void testCancelDisconnectRevocationAndExpiry() {
    for (unsigned mode = 0; mode < 5; ++mode) {
        Fixture f;
        f.authorize(1000);
        auto req = f.request(Operation::LedSingle);
        CHECK(f.control.submit(req, f.binding, 100) == Admission::Accepted);
        auto other = f.binding;
        ++other.token;
        f.control.cancel(other, 110);
        f.control.disconnect(other, 111);
        CHECK(f.control.granted());
        if (mode == 0)
            f.control.cancel(f.binding, 120);
        if (mode == 1)
            f.control.disconnect(f.binding, 120);
        if (mode == 2) {
            CHECK(f.access.revokeGameMaster(identity(2)) ==
                  companion::IdentityAccessOutcome::Revoked);
            f.control.tick(120);
        }
        if (mode == 3)
            f.control.tick(1100);
        if (mode == 4)
            f.control.retire(120);
        CHECK(!f.control.granted() && !f.port.light && !f.port.owned);
        auto result = f.reply();
        CHECK(result.outcome == Outcome::Cancelled && result.restoredOff);
        CHECK(f.control.submit(req, f.binding, 1200) == Admission::PermissionDenied);
    }
}
static void testTimeoutClockAndOwnerFaults() {
    Fixture timeout;
    timeout.authorize();
    auto req = timeout.request();
    CHECK(timeout.control.submit(req, timeout.binding, 110) == Admission::Accepted);
    timeout.control.tick(1110);
    CHECK(timeout.reply().reason == Reason::Expired);
    CHECK(timeout.control.submit(req, timeout.binding, 1120) == Admission::Consumed);
    Fixture backwards;
    backwards.authorize();
    req = backwards.request(Operation::LedSingle);
    CHECK(backwards.control.submit(req, backwards.binding, 110) == Admission::Accepted);
    backwards.control.tick(109);
    CHECK(backwards.reply().reason == Reason::OwnerError);
    CHECK(!backwards.port.light && !backwards.control.granted());
    CHECK(backwards.control.authorize(uuid(2), backwards.binding, true, 1000, 120,
                                      backwards.grant) == Admission::OwnerError);
    Fixture onFail;
    onFail.authorize();
    onFail.port.onOk = false;
    req = onFail.request(Operation::LedSingle);
    CHECK(onFail.control.submit(req, onFail.binding, 110) == Admission::Accepted);
    auto result = onFail.reply();
    CHECK(result.outcome == Outcome::Failed && result.restoredOff && onFail.port.offs == 1);
    Fixture offFail;
    offFail.authorize();
    offFail.port.offOk = false;
    req = offFail.request(Operation::LedSingle);
    CHECK(offFail.control.submit(req, offFail.binding, 110) == Admission::Accepted);
    offFail.control.tick(610);
    result = offFail.reply();
    CHECK(result.outcome == Outcome::Failed && result.reason == Reason::OwnerError &&
          !result.restoredOff);
    offFail.control.retire(620);
    CHECK(offFail.port.owned && offFail.port.light && offFail.port.releases == 0);
    CHECK(offFail.control.authorize(uuid(2), offFail.binding, true, 1000, 630, offFail.grant) ==
          Admission::OwnerError);
}
static void testConnectionGenerationAndFinalStep() {
    Fixture f;
    f.authorize();
    auto req = f.request(Operation::LedSingle);
    CHECK(f.control.submit(req, f.binding, 110) == Admission::Accepted);
    const auto original = f.binding;
    CHECK(f.access.disconnect(original) == companion::IdentityAccessOutcome::Disconnected);
    CHECK(f.access.connect(42, f.binding) == companion::IdentityAccessOutcome::Connected);
    CHECK(f.binding.token != original.token);
    f.control.tick(120);
    CHECK(!f.control.granted() && !f.port.light);
    Reply result;
    CHECK(!f.control.takeReply(f.binding, result));
    CHECK(f.control.takeReply(original, result) && result.reason == Reason::PermissionDenied);
    CHECK(f.control.submit(req, f.binding, 130) == Admission::PermissionDenied);
    Fixture final;
    final.authorize();
    req = final.request();
    req.stepId = 0xffffffffU;
    CHECK(final.control.submit(req, final.binding, 110) == Admission::Accepted);
    final.control.finishWake(120);
    CHECK(final.reply().request.stepId == 0xffffffffU);
    req.stepId = 1;
    CHECK(final.control.submit(req, final.binding, 130) == Admission::Consumed);
}
static void testAcquireEntitlementChangeAndTimeBounds() {
    Fixture f;
    f.port.revokeOnAcquire = &f.access;
    CHECK(f.control.authorize(uuid(1), f.binding, true, 1000, 100, f.grant) ==
          Admission::PermissionDenied);
    CHECK(!f.control.granted() && !f.port.owned && f.port.releases == 1);
    Fixture clock;
    CHECK(clock.control.authorize(uuid(1), clock.binding, true, 1000, 9007199254740990ULL,
                                  clock.grant) == Admission::Invalid);
    clock.control.tick(9007199254740992ULL);
    CHECK(clock.control.authorize(uuid(1), clock.binding, true, 1000, 9007199254740992ULL,
                                  clock.grant) == Admission::OwnerError);
}
int main() {
    testRightsAndConsent();
    testGrantBoundsAndHistory();
    testWakeCorrelationAndForeignEdges();
    testLowPolarityOverflowAndInvalidChannel();
    testLedHoldAndDuplicate();
    testLedOwnerTimerCutsLightWithoutControlTick();
    testTimerArmFailurePreventsRenderAndLatchesOwner();
    testTimerOffFailureRemainsFencedAfterLaterControlTick();
    testMissingTimerCallbackCannotBecomeCompletedOnLateTick();
    testLateTimerCallbackCannotConfirmDeadline();
    testExpiredTimerCannotBeFollowedByLateRender();
    testUnsupportedAndStale();
    testCancelDisconnectRevocationAndExpiry();
    testTimeoutClockAndOwnerFaults();
    testConnectionGenerationAndFinalStep();
    testAcquireEntitlementChangeAndTimeBounds();
    std::printf("Diagnostic C++11 owner: 16 scenarios, %u checks passed; fake ports, no hardware; "
                "owner %u bytes\n",
                checks, static_cast<unsigned>(sizeof(Control)));
}
