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

void setUp() {}
void tearDown() {}
int main() {
    UNITY_BEGIN();
    RUN_TEST(test_application_services_then_ticks_then_publishes_copy);
    return UNITY_END();
}
