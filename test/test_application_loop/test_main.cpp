#include "../../src/GameDomain/GameDefinitions.cpp"
#include "../../src/GameDomain/GameProgress.cpp"
#include "../../src/GameDomain/KeystoneMonitor.cpp"
#include "../../src/GameDomain/KeystoneTransition.cpp"
#include "../../src/GameDomain/PuzzleProgress.cpp"
#include "../test_game_application/GameFakes.h"
#include "LarpEscapeApp.h"
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
    RUN_TEST(test_application_services_then_ticks_then_publishes_copy);
    RUN_TEST(test_diagnostic_preflight_fences_actual_application_loop_before_game_io);
    RUN_TEST(test_diagnostic_preflight_rejects_started_game_without_fencing_it);
    RUN_TEST(test_standby_fence_waits_for_entry_and_stops_later_game_io);
    RUN_TEST(test_non_standby_game_cannot_be_fenced);
    RUN_TEST(test_standby_observation_is_revoked_when_game_wakes);
    return UNITY_END();
}
