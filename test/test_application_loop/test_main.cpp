#include "../../src/GameDomain/GameDefinitions.cpp"
#include "../../src/GameDomain/GameProgress.cpp"
#include "../../src/GameDomain/KeystoneMonitor.cpp"
#include "../../src/GameDomain/KeystoneTransition.cpp"
#include "../../src/GameDomain/PuzzleProgress.cpp"
#include "../test_game_application/GameFakes.h"
#include "LarpEscapeApp.h"
#include "ClosedDiagnosticArbiter.h"
#include <unity.h>
#include <vector>

class RuntimeProbe : public larp::ports::IRuntimeService {
  public:
    RuntimeProbe(std::vector<int> &order, int id) : order(order), id(id) {}
    void service() override {
        order.push_back(id);
        ++calls;
    }
    std::vector<int> &order;
    int id;
    unsigned calls = 0;
};
class SnapshotProbe : public larp::application::IGameSnapshotSink {
  public:
    SnapshotProbe(std::vector<int> &order) : order(order) {}
    void publish(const larp::application::GameSnapshot &value) override {
        order.push_back(3);
        values.push_back(value);
    }
    std::vector<int> &order;
    std::vector<larp::application::GameSnapshot> values;
};

class ParkProbe : public larp::application::ILedParkPort {
  public:
    bool requestPark(std::uint32_t ticket) override {
        ++requests;
        requested = ticket;
        return accepts;
    }
    bool offFrameSubmittedFor(std::uint32_t ticket) const override {
        return ticket == requested && submitted;
    }
    bool forceBlackFromDeadline(std::uint32_t ticket) override {
        independent = ticket == requested;
        return independent;
    }
    std::uint32_t requested = 0;
    unsigned requests = 0;
    bool accepts = true, submitted = false, independent = false;
};
class DeadlineProbe : public larp::application::ILedParkDeadline {
  public:
    bool arm(std::uint32_t ticket, larp::application::ILedParkPort &) override {
        armed = ticket;
        return accepts;
    }
    bool firedFor(std::uint32_t ticket) const override { return ticket == armed && fired; }
    bool missedFor(std::uint32_t ticket) const override { return ticket == armed && missed; }
    bool independentBlackSubmittedFor(std::uint32_t ticket) const override {
        return ticket == armed && independent;
    }
    std::uint32_t armed = 0;
    bool accepts = true, fired = false, missed = false, independent = false;
};

void test_closed_arbiter_fences_game_before_timer_and_never_grants_on_black_frame() {
    game_test::GameRig rig;
    std::vector<int> order;
    RuntimeProbe audio(order, 1), communication(order, 2);
    SnapshotProbe sink(order);
    larp::application::LarpEscapeApp app(rig.game, audio, communication, sink);
    ParkProbe lights;
    DeadlineProbe timer;
    larp::application::ClosedDiagnosticArbiter arbiter(app, lights, timer);
    TEST_ASSERT_TRUE(arbiter.requestClosedHandoff(73, true));
    TEST_ASSERT_TRUE(app.gameFencedForDiagnostics());
    TEST_ASSERT_EQUAL_UINT32(73, timer.armed);
    TEST_ASSERT_EQUAL_UINT32(73, lights.requested);
    lights.submitted = true;
    timer.fired = true;
    timer.independent = true;
    TEST_ASSERT_TRUE(arbiter.blackFrameSubmitted());
    TEST_ASSERT_TRUE(arbiter.timerExpired());
    TEST_ASSERT_TRUE(arbiter.independentBlackSubmitted());
    TEST_ASSERT_FALSE(arbiter.diagnosticGrantAvailable());
    arbiter.update();
    TEST_ASSERT_TRUE(order.empty());
    TEST_ASSERT_FALSE(arbiter.requestClosedHandoff(74, true));
}

void test_closed_arbiter_timer_failure_or_miss_keeps_game_fenced() {
    for (unsigned failure = 0; failure < 3; ++failure) {
        game_test::GameRig rig;
        std::vector<int> order;
        RuntimeProbe audio(order, 1), communication(order, 2);
        SnapshotProbe sink(order);
        larp::application::LarpEscapeApp app(rig.game, audio, communication, sink);
        ParkProbe lights;
        DeadlineProbe timer;
        timer.accepts = failure != 0;
        lights.accepts = failure != 1;
        larp::application::ClosedDiagnosticArbiter arbiter(app, lights, timer);
        TEST_ASSERT_EQUAL(failure == 2, arbiter.requestClosedHandoff(9, true));
        if (failure == 2) timer.missed = true;
        TEST_ASSERT_EQUAL(static_cast<unsigned>(larp::application::ClosedHandoff::Fault),
                          static_cast<unsigned>(arbiter.state()));
        TEST_ASSERT_TRUE(app.gameFencedForDiagnostics());
        TEST_ASSERT_FALSE(arbiter.diagnosticGrantAvailable());
        arbiter.update();
        TEST_ASSERT_TRUE(order.empty());
    }
}

void test_closed_arbiter_rejects_active_game_without_arming_timer() {
    game_test::GameRig rig;
    std::vector<int> order;
    RuntimeProbe audio(order, 1), communication(order, 2);
    SnapshotProbe sink(order);
    larp::application::LarpEscapeApp app(rig.game, audio, communication, sink);
    ParkProbe lights;
    DeadlineProbe timer;
    larp::application::ClosedDiagnosticArbiter arbiter(app, lights, timer);
    arbiter.update();
    TEST_ASSERT_FALSE(arbiter.requestClosedHandoff(5, true));
    TEST_ASSERT_EQUAL_UINT32(0, timer.armed);
    TEST_ASSERT_EQUAL_UINT(0, lights.requests);
    TEST_ASSERT_FALSE(app.gameFencedForDiagnostics());
}

void test_late_independent_black_submission_is_fault_not_off_evidence() {
    TEST_ASSERT_TRUE(larp::application::softwareOffWithinDeadline(1000000, 1500000));
    TEST_ASSERT_FALSE(larp::application::softwareOffWithinDeadline(1000000, 1500001));
    TEST_ASSERT_FALSE(larp::application::softwareOffWithinDeadline(1000000, 999999));
    game_test::GameRig rig;
    std::vector<int> order;
    RuntimeProbe audio(order, 1), communication(order, 2);
    SnapshotProbe sink(order);
    larp::application::LarpEscapeApp app(rig.game, audio, communication, sink);
    ParkProbe lights;
    DeadlineProbe timer;
    larp::application::ClosedDiagnosticArbiter arbiter(app, lights, timer);
    TEST_ASSERT_TRUE(arbiter.requestClosedHandoff(19, true));
    lights.submitted = true;
    timer.fired = true;
    timer.independent = true;
    timer.missed = true; // model: the separate task completed after 500 ms
    TEST_ASSERT_TRUE(arbiter.independentBlackSubmitted());
    TEST_ASSERT_TRUE(arbiter.deadlineMissed());
    TEST_ASSERT_EQUAL(static_cast<unsigned>(larp::application::ClosedHandoff::Fault),
                      static_cast<unsigned>(arbiter.state()));
    TEST_ASSERT_FALSE(arbiter.diagnosticGrantAvailable());
    arbiter.update();
    TEST_ASSERT_TRUE(order.empty());
}

void test_application_services_then_ticks_then_publishes_copy() {
    game_test::GameRig rig;
    std::vector<int> order;
    RuntimeProbe audio(order, 1), communication(order, 2);
    SnapshotProbe sink(order);
    larp::application::LarpEscapeApp app(rig.game, audio, communication, sink);
    rig.wake.signals = {{true, true, true, true}};
    app.update();
    TEST_ASSERT_EQUAL_UINT(3, order.size());
    TEST_ASSERT_EQUAL_INT(1, order[0]);
    TEST_ASSERT_EQUAL_INT(2, order[1]);
    TEST_ASSERT_EQUAL_INT(3, order[2]);
    TEST_ASSERT_EQUAL_UINT8(static_cast<unsigned>(larp::application::GamePhase::Idle),
                            static_cast<unsigned>(sink.values[0].phase));
    // A tick selects Idle but does not run its entry action until the next tick.
    TEST_ASSERT_EQUAL_UINT(0, rig.audio.tracks.size());
    app.update();
    TEST_ASSERT_EQUAL_UINT(2, audio.calls);
    TEST_ASSERT_EQUAL_UINT(2, communication.calls);
    TEST_ASSERT_EQUAL_UINT(2, sink.values.size());
    TEST_ASSERT_EQUAL_UINT(2, rig.lights.bursts.size());
    const auto oldCopy = sink.values[0];
    rig.game.machine.transitionTo(8);
    app.update();
    TEST_ASSERT_EQUAL_UINT8(static_cast<unsigned>(larp::application::GamePhase::Idle),
                            static_cast<unsigned>(oldCopy.phase));
    TEST_ASSERT_EQUAL_UINT8(static_cast<unsigned>(larp::application::GamePhase::Error),
                            static_cast<unsigned>(sink.values.back().phase));
    TEST_ASSERT_EQUAL_UINT(0, rig.clock.delayCalls);
}

void test_diagnostic_preflight_fences_actual_application_loop_before_game_io() {
    game_test::GameRig rig;
    std::vector<int> order;
    RuntimeProbe audio(order, 1), communication(order, 2);
    SnapshotProbe sink(order);
    larp::application::LarpEscapeApp app(rig.game, audio, communication, sink);
    rig.wake.signals = {{true, true, true, true}};
    TEST_ASSERT_TRUE(app.fenceUnstartedGameForDiagnostics());
    TEST_ASSERT_TRUE(app.gameFencedForDiagnostics());
    TEST_ASSERT_FALSE(app.fenceUnstartedGameForDiagnostics());
    app.update();
    app.update();
    TEST_ASSERT_TRUE(order.empty());
    TEST_ASSERT_EQUAL_INT(-1, rig.game.machine.currentStateIndex());
    TEST_ASSERT_EQUAL_UINT(0, rig.wake.reads);
    TEST_ASSERT_EQUAL_UINT(0, rig.audio.tracks.size());
    TEST_ASSERT_EQUAL_UINT(0, rig.lights.bursts.size());
}

void test_diagnostic_preflight_rejects_started_game_without_fencing_it() {
    game_test::GameRig rig;
    std::vector<int> order;
    RuntimeProbe audio(order, 1), communication(order, 2);
    SnapshotProbe sink(order);
    larp::application::LarpEscapeApp app(rig.game, audio, communication, sink);
    app.update();
    TEST_ASSERT_FALSE(app.fenceUnstartedGameForDiagnostics());
    TEST_ASSERT_FALSE(app.gameFencedForDiagnostics());
    const unsigned previousTicks = audio.calls;
    app.update();
    TEST_ASSERT_EQUAL_UINT(previousTicks + 1U, audio.calls);
    TEST_ASSERT_EQUAL_UINT(previousTicks + 1U, sink.values.size());
}

void test_standby_fence_waits_for_entry_and_stops_later_game_io() {
    game_test::GameRig rig;
    std::vector<int> order;
    RuntimeProbe audio(order, 1), communication(order, 2);
    SnapshotProbe sink(order);
    larp::application::LarpEscapeApp app(rig.game, audio, communication, sink);
    // A partial wake schedules a light effect but leaves the game in Standby.
    // The app-loop fence must not be interpreted as a light-owner Park Ack.
    rig.wake.signals = {{true, false, false, false}};
    TEST_ASSERT_FALSE(app.fenceObservedStandbyAppLoopForDiagnostics());
    app.update();
    TEST_ASSERT_EQUAL_UINT8(static_cast<unsigned>(larp::application::GamePhase::Standby),
                            static_cast<unsigned>(sink.values.back().phase));
    TEST_ASSERT_FALSE(app.fenceObservedStandbyAppLoopForDiagnostics());
    app.update();
    TEST_ASSERT_TRUE(rig.lights.ripples.size() > 0U);
    TEST_ASSERT_TRUE(app.fenceObservedStandbyAppLoopForDiagnostics());
    const auto count = order.size();
    const auto wakeReads = rig.wake.reads;
    app.update();
    app.update();
    TEST_ASSERT_EQUAL_UINT(count, order.size());
    TEST_ASSERT_EQUAL_UINT(wakeReads, rig.wake.reads);
    TEST_ASSERT_FALSE(app.fenceObservedStandbyAppLoopForDiagnostics());
}

void test_non_standby_game_cannot_be_fenced() {
    game_test::GameRig rig;
    std::vector<int> order;
    RuntimeProbe audio(order, 1), communication(order, 2);
    SnapshotProbe sink(order);
    larp::application::LarpEscapeApp app(rig.game, audio, communication, sink);
    rig.wake.signals = {{true, true, true, true}};
    app.update();
    app.update();
    TEST_ASSERT_FALSE(app.fenceObservedStandbyAppLoopForDiagnostics());
    TEST_ASSERT_FALSE(app.gameFencedForDiagnostics());
    const auto count = order.size();
    app.update();
    TEST_ASSERT_EQUAL_UINT(count + 3U, order.size());
}

void test_standby_observation_is_revoked_when_game_wakes() {
    game_test::GameRig rig;
    std::vector<int> order;
    RuntimeProbe audio(order, 1), communication(order, 2);
    SnapshotProbe sink(order);
    larp::application::LarpEscapeApp app(rig.game, audio, communication, sink);
    app.update();
    app.update();
    rig.wake.signals = {{true, true, true, true}};
    app.update();
    TEST_ASSERT_EQUAL_UINT8(static_cast<unsigned>(larp::application::GamePhase::Idle),
                            static_cast<unsigned>(sink.values.back().phase));
    TEST_ASSERT_FALSE(app.fenceObservedStandbyAppLoopForDiagnostics());
    TEST_ASSERT_FALSE(app.gameFencedForDiagnostics());
}

void setUp() {}
void tearDown() {}
int main() {
    UNITY_BEGIN();
    RUN_TEST(test_closed_arbiter_fences_game_before_timer_and_never_grants_on_black_frame);
    RUN_TEST(test_closed_arbiter_timer_failure_or_miss_keeps_game_fenced);
    RUN_TEST(test_closed_arbiter_rejects_active_game_without_arming_timer);
    RUN_TEST(test_late_independent_black_submission_is_fault_not_off_evidence);
    RUN_TEST(test_application_services_then_ticks_then_publishes_copy);
    RUN_TEST(test_diagnostic_preflight_fences_actual_application_loop_before_game_io);
    RUN_TEST(test_diagnostic_preflight_rejects_started_game_without_fencing_it);
    RUN_TEST(test_standby_fence_waits_for_entry_and_stops_later_game_io);
    RUN_TEST(test_non_standby_game_cannot_be_fenced);
    RUN_TEST(test_standby_observation_is_revoked_when_game_wakes);
    return UNITY_END();
}
