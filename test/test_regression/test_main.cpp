#ifdef ARDUINO
#include <Arduino.h>
#endif
#include <unity.h>
#include "StateMachine.h"

void test_default_roles_have_unique_uids_names_and_audio_tracks();
void test_unknown_role_uid_is_rejected();
void test_crystal_definitions_and_palette_mapping_match_current_data();
void test_unknown_crystal_category_has_fallback_label();
void test_grid_position_equality_and_ordering();
void test_puzzle_progress_records_unique_crystals_in_order_and_completes_at_three();
void test_puzzle_progress_tracks_audio_and_resets_round_state();
void test_keystone_monitor_raises_error_after_five_consecutive_misses();
void test_keystone_monitor_match_breaks_miss_streak_and_clears_error();
void test_empty_keystone_read_never_starts_a_role_transition();
void test_keystone_transition_requires_board_clear_and_known_role();
void test_five_empty_keystone_reads_are_required_to_clear_board_state();
void test_round_reset_clears_only_round_progress_and_retry_streak();
void test_game_reset_clears_rounds_keystones_seen_tags_and_errors();
void test_error_transition_waits_for_audio_then_consumes_crystal_before_keystone_error();



void test_machine_stays_in_state_while_guard_is_false_and_runs_target_next_tick()
{
    MyMachine::StateMachine machine;
    int idleCalls = 0;
    int activeCalls = 0;
    bool canLeaveIdle = false;

    myMachine::State *idle = machine.addState([&idleCalls]() { ++idleCalls; });
    myMachine::State *active = machine.addState([&activeCalls]() { ++activeCalls; });
    idle->addTransition([&canLeaveIdle]() { return canLeaveIdle; }, active);

    machine.run();
    TEST_ASSERT_EQUAL_INT(1, idleCalls);
    TEST_ASSERT_EQUAL_INT(0, activeCalls);

    canLeaveIdle = true;
    machine.run();
    TEST_ASSERT_EQUAL_INT(2, idleCalls);
    TEST_ASSERT_EQUAL_INT(0, activeCalls);

    machine.run();
    TEST_ASSERT_EQUAL_INT(1, activeCalls);
}

void test_machine_uses_first_matching_transition()
{
    MyMachine::StateMachine machine;
    int firstCalls = 0;
    int secondCalls = 0;
    bool secondGuardEvaluated = false;

    myMachine::State *initial = machine.addState([]() {});
    myMachine::State *first = machine.addState([&firstCalls]() { ++firstCalls; });
    myMachine::State *second = machine.addState([&secondCalls]() { ++secondCalls; });
    initial->addTransition([]() { return true; }, first);
    initial->addTransition([&secondGuardEvaluated]() {
        secondGuardEvaluated = true;
        return true;
    }, second);

    machine.run();
    machine.run();

    TEST_ASSERT_EQUAL_INT(1, firstCalls);
    TEST_ASSERT_EQUAL_INT(0, secondCalls);
    TEST_ASSERT_FALSE(secondGuardEvaluated);
}

void test_state_transition_target_can_be_reconfigured()
{
    MyMachine::StateMachine machine;
    int oldTargetCalls = 0;
    int newTargetCalls = 0;

    myMachine::State *initial = machine.addState([]() {});
    myMachine::State *oldTarget = machine.addState([&oldTargetCalls]() { ++oldTargetCalls; });
    myMachine::State *newTarget = machine.addState([&newTargetCalls]() { ++newTargetCalls; });
    initial->addTransition([]() { return true; }, oldTarget);

    TEST_ASSERT_EQUAL_INT(newTarget->index, initial->setTransition(0, newTarget->index));
    machine.run();
    machine.run();

    TEST_ASSERT_EQUAL_INT(0, oldTargetCalls);
    TEST_ASSERT_EQUAL_INT(1, newTargetCalls);
}

void setUp() {}
void tearDown() {}

int runUnityTests()
{
    UNITY_BEGIN();
    RUN_TEST(test_machine_stays_in_state_while_guard_is_false_and_runs_target_next_tick);
    RUN_TEST(test_machine_uses_first_matching_transition);
    RUN_TEST(test_state_transition_target_can_be_reconfigured);
    RUN_TEST(test_default_roles_have_unique_uids_names_and_audio_tracks);
    RUN_TEST(test_unknown_role_uid_is_rejected);
    RUN_TEST(test_crystal_definitions_and_palette_mapping_match_current_data);
    RUN_TEST(test_unknown_crystal_category_has_fallback_label);
    RUN_TEST(test_grid_position_equality_and_ordering);
    RUN_TEST(test_puzzle_progress_records_unique_crystals_in_order_and_completes_at_three);
    RUN_TEST(test_puzzle_progress_tracks_audio_and_resets_round_state);
    RUN_TEST(test_keystone_monitor_raises_error_after_five_consecutive_misses);
    RUN_TEST(test_keystone_monitor_match_breaks_miss_streak_and_clears_error);
    RUN_TEST(test_empty_keystone_read_never_starts_a_role_transition);
    RUN_TEST(test_keystone_transition_requires_board_clear_and_known_role);
    RUN_TEST(test_five_empty_keystone_reads_are_required_to_clear_board_state);
    RUN_TEST(test_round_reset_clears_only_round_progress_and_retry_streak);
    RUN_TEST(test_game_reset_clears_rounds_keystones_seen_tags_and_errors);
    RUN_TEST(test_error_transition_waits_for_audio_then_consumes_crystal_before_keystone_error);
    return UNITY_END();
}

#ifdef ARDUINO
void setup()
{
    Serial.begin(115200);
    delay(2000);
    runUnityTests();
}
void loop() {}
#else
int main()
{
    return runUnityTests();
}
#endif
