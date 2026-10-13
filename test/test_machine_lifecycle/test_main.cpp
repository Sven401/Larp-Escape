#include "StateMachine.h"
#include <memory>
#include <type_traits>
#include <unity.h>

static_assert(!std::is_copy_constructible<MyMachine::StateMachine>::value,
              "A machine cannot share owned states through an implicit copy");
static_assert(!std::is_move_constructible<MyMachine::StateMachine>::value,
              "State owner pointers require a stable machine address");
static_assert(!std::is_copy_constructible<myMachine::State>::value,
              "State callbacks and transitions have one owner");
static_assert(!std::is_move_constructible<myMachine::State>::value,
              "Borrowed state handles remain at a stable address");

void setUp() {}
void tearDown() {}

void test_empty_machine_and_invalid_initial_targets_are_safe() {
    MyMachine::StateMachine machine;
    machine.init();
    machine.run();
    TEST_ASSERT_EQUAL_INT(0, machine.stateCount());
    TEST_ASSERT_EQUAL_INT(-1, machine.currentStateIndex());
    TEST_ASSERT_EQUAL_INT(-1, machine.transitionTo(-2));
    TEST_ASSERT_EQUAL_INT(-1, machine.transitionTo(0));
    TEST_ASSERT_NULL(machine.transitionTo(static_cast<myMachine::State *>(nullptr)));
    TEST_ASSERT_EQUAL_INT(-1, machine.currentStateIndex());
}

void test_rejected_direct_targets_leave_current_state_and_entry_flag_intact() {
    MyMachine::StateMachine machine;
    MyMachine::StateMachine foreign;
    int calls = 0;
    machine.addState([&calls]() { ++calls; });
    myMachine::State *foreignState = foreign.addState([]() {});
    machine.run();
    TEST_ASSERT_FALSE(machine.executeOnce);

    TEST_ASSERT_EQUAL_INT(0, machine.transitionTo(-1));
    TEST_ASSERT_EQUAL_INT(0, machine.transitionTo(-9));
    TEST_ASSERT_EQUAL_INT(0, machine.transitionTo(1));
    TEST_ASSERT_EQUAL_INT(0, machine.transitionTo(10000));
    TEST_ASSERT_NULL(machine.transitionTo(static_cast<myMachine::State *>(nullptr)));
    TEST_ASSERT_NULL(machine.transitionTo(foreignState));
    TEST_ASSERT_EQUAL_INT(0, machine.currentStateIndex());
    TEST_ASSERT_FALSE(machine.executeOnce);
    machine.run();
    TEST_ASSERT_EQUAL_INT(2, calls);
}

void test_invalid_guard_targets_are_rejected_without_hiding_a_valid_guard() {
    MyMachine::StateMachine machine;
    MyMachine::StateMachine foreign;
    int rejectedGuardCalls = 0;
    int targetCalls = 0;
    myMachine::State *initial = machine.addState([]() {});
    myMachine::State *target = machine.addState([&targetCalls]() { ++targetCalls; });
    myMachine::State *foreignTarget = foreign.addState([]() {});
    const std::function<bool()> invalidGuard = [&rejectedGuardCalls]() {
        ++rejectedGuardCalls;
        return true;
    };

    TEST_ASSERT_FALSE(initial->addTransition(invalidGuard, -1));
    TEST_ASSERT_FALSE(initial->addTransition(invalidGuard, 2));
    TEST_ASSERT_FALSE(initial->addTransition(invalidGuard, foreignTarget));
    TEST_ASSERT_FALSE(
        initial->addTransition(invalidGuard, static_cast<myMachine::State *>(nullptr)));
    TEST_ASSERT_FALSE(initial->addTransition(std::function<bool()>(), target));
    TEST_ASSERT_TRUE(initial->addTransition([]() { return true; }, target));
    machine.run();
    machine.run();
    TEST_ASSERT_EQUAL_INT(0, rejectedGuardCalls);
    TEST_ASSERT_EQUAL_INT(1, targetCalls);
}

void test_invalid_transition_reconfiguration_preserves_existing_target() {
    MyMachine::StateMachine machine;
    int targetCalls = 0;
    myMachine::State *initial = machine.addState([]() {});
    myMachine::State *target = machine.addState([&targetCalls]() { ++targetCalls; });
    TEST_ASSERT_EQUAL_INT(-1, initial->setTransition(0, target->index));
    TEST_ASSERT_TRUE(initial->addTransition([]() { return true; }, target));
    TEST_ASSERT_EQUAL_INT(-1, initial->setTransition(-1, target->index));
    TEST_ASSERT_EQUAL_INT(-1, initial->setTransition(1, target->index));
    TEST_ASSERT_EQUAL_INT(-1, initial->setTransition(10000, target->index));
    TEST_ASSERT_EQUAL_INT(-1, initial->setTransition(0, -1));
    TEST_ASSERT_EQUAL_INT(-1, initial->setTransition(0, 2));
    machine.run();
    machine.run();
    TEST_ASSERT_EQUAL_INT(1, targetCalls);
}

void test_unbound_state_and_empty_callbacks_are_safe() {
    myMachine::State unbound;
    MyMachine::StateMachine machine;
    myMachine::State *initial = machine.addState(std::function<void()>());
    myMachine::State *target = machine.addState(std::function<void()>());
    TEST_ASSERT_EQUAL_INT(-1, unbound.index);
    TEST_ASSERT_EQUAL_INT(-1, unbound.execute());
    TEST_ASSERT_FALSE(unbound.addTransition([]() { return true; }, initial));
    TEST_ASSERT_FALSE(unbound.addTransition([]() { return true; }, 0));
    TEST_ASSERT_TRUE(initial->addTransition([]() { return true; }, target));
    machine.run();
    TEST_ASSERT_EQUAL_INT(target->index, machine.currentStateIndex());
    machine.run();
    TEST_ASSERT_FALSE(machine.executeOnce);
}

void test_repeated_destruction_releases_state_and_transition_callback_captures() {
    for (int repetition = 0; repetition < 64; ++repetition) {
        std::weak_ptr<int> stateCapture;
        std::weak_ptr<int> guardCapture;
        {
            MyMachine::StateMachine machine;
            std::shared_ptr<int> stateToken(new int(1));
            std::shared_ptr<int> guardToken(new int(2));
            stateCapture = stateToken;
            guardCapture = guardToken;
            myMachine::State *state = machine.addState([stateToken]() { (void)*stateToken; });
            TEST_ASSERT_TRUE(
                state->addTransition([guardToken]() { return *guardToken == 0; }, state));
            stateToken.reset();
            guardToken.reset();
            TEST_ASSERT_FALSE(stateCapture.expired());
            TEST_ASSERT_FALSE(guardCapture.expired());
            machine.run();
        }
        TEST_ASSERT_TRUE(stateCapture.expired());
        TEST_ASSERT_TRUE(guardCapture.expired());
    }
}

struct ThirdEvaluationGuard {
    int count;
    ThirdEvaluationGuard() : count(0) {}
    bool operator()() {
        return ++count == 3;
    }
};

void test_mutable_guard_state_survives_successive_ticks() {
    MyMachine::StateMachine machine;
    myMachine::State *initial = machine.addState([]() {});
    myMachine::State *target = machine.addState([]() {});
    TEST_ASSERT_TRUE(initial->addTransition(ThirdEvaluationGuard(), target));
    machine.run();
    TEST_ASSERT_EQUAL_INT(initial->index, machine.currentStateIndex());
    machine.run();
    TEST_ASSERT_EQUAL_INT(initial->index, machine.currentStateIndex());
    machine.run();
    TEST_ASSERT_EQUAL_INT(target->index, machine.currentStateIndex());
}

void test_guard_can_append_transitions_without_invalidation_or_same_tick_execution() {
    MyMachine::StateMachine machine;
    int appendedGuardCalls = 0;
    bool appended = false;
    myMachine::State *initial = machine.addState([]() {});
    myMachine::State *target = machine.addState([]() {});
    TEST_ASSERT_TRUE(initial->addTransition(
        [&]() {
            if (!appended) {
                appended = true;
                for (int addition = 0; addition < 128; ++addition) {
                    initial->addTransition(
                        [&appendedGuardCalls]() {
                            ++appendedGuardCalls;
                            return true;
                        },
                        target);
                }
            }
            return false;
        },
        target));
    machine.run();
    TEST_ASSERT_EQUAL_INT(initial->index, machine.currentStateIndex());
    TEST_ASSERT_EQUAL_INT(0, appendedGuardCalls);
    machine.run();
    TEST_ASSERT_EQUAL_INT(target->index, machine.currentStateIndex());
    TEST_ASSERT_EQUAL_INT(1, appendedGuardCalls);
}

void test_explicit_transition_from_state_logic_survives_old_guards_and_nested_run() {
    MyMachine::StateMachine machine;
    int initialCalls = 0;
    int targetCalls = 0;
    int obsoleteGuardCalls = 0;
    myMachine::State *target = nullptr;
    myMachine::State *initial = machine.addState([&]() {
        ++initialCalls;
        machine.transitionTo(target);
        machine.run();
    });
    target = machine.addState([&targetCalls]() { ++targetCalls; });
    myMachine::State *obsoleteTarget = machine.addState([]() {});
    TEST_ASSERT_TRUE(initial->addTransition(
        [&obsoleteGuardCalls]() {
            ++obsoleteGuardCalls;
            return true;
        },
        obsoleteTarget));
    machine.run();
    TEST_ASSERT_EQUAL_INT(1, initialCalls);
    TEST_ASSERT_EQUAL_INT(0, targetCalls);
    TEST_ASSERT_EQUAL_INT(0, obsoleteGuardCalls);
    TEST_ASSERT_EQUAL_INT(target->index, machine.currentStateIndex());
    TEST_ASSERT_TRUE(machine.executeOnce);
    machine.run();
    TEST_ASSERT_EQUAL_INT(1, targetCalls);
}

void test_explicit_transition_from_guard_stops_later_guards() {
    MyMachine::StateMachine machine;
    int laterGuardCalls = 0;
    myMachine::State *initial = machine.addState([]() {});
    myMachine::State *target = machine.addState([]() {});
    myMachine::State *obsoleteTarget = machine.addState([]() {});
    TEST_ASSERT_TRUE(initial->addTransition(
        [&]() {
            machine.transitionTo(target);
            machine.run();
            return false;
        },
        obsoleteTarget));
    TEST_ASSERT_TRUE(initial->addTransition(
        [&laterGuardCalls]() {
            ++laterGuardCalls;
            return true;
        },
        obsoleteTarget));
    machine.run();
    TEST_ASSERT_EQUAL_INT(target->index, machine.currentStateIndex());
    TEST_ASSERT_TRUE(machine.executeOnce);
    TEST_ASSERT_EQUAL_INT(0, laterGuardCalls);
}

void test_entry_action_runs_once_per_automatic_or_explicit_entry() {
    MyMachine::StateMachine machine;
    bool leaveInitial = false;
    int initialEntries = 0;
    int targetEntries = 0;
    myMachine::State *initial = machine.addState([&]() {
        if (machine.executeOnce)
            ++initialEntries;
    });
    myMachine::State *target = machine.addState([&]() {
        if (machine.executeOnce)
            ++targetEntries;
    });
    initial->addTransition([&leaveInitial]() { return leaveInitial; }, target);
    machine.run();
    machine.run();
    machine.run();
    TEST_ASSERT_EQUAL_INT(1, initialEntries);
    leaveInitial = true;
    machine.run();
    TEST_ASSERT_EQUAL_INT(0, targetEntries);
    machine.run();
    machine.run();
    TEST_ASSERT_EQUAL_INT(1, targetEntries);
    TEST_ASSERT_EQUAL_PTR(target, machine.transitionTo(target));
    machine.run();
    TEST_ASSERT_EQUAL_INT(2, targetEntries);
}

void test_state_handles_remain_stable_when_additional_states_are_created() {
    MyMachine::StateMachine machine;
    int calls = 0;
    myMachine::State *original = machine.addState([&calls]() { ++calls; });
    for (int addition = 0; addition < 128; ++addition)
        machine.addState([]() {});
    TEST_ASSERT_EQUAL_INT(129, machine.stateCount());
    TEST_ASSERT_EQUAL_INT(0, original->index);
    TEST_ASSERT_EQUAL_PTR(original, machine.transitionTo(original));
    machine.run();
    TEST_ASSERT_EQUAL_INT(1, calls);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_empty_machine_and_invalid_initial_targets_are_safe);
    RUN_TEST(test_rejected_direct_targets_leave_current_state_and_entry_flag_intact);
    RUN_TEST(test_invalid_guard_targets_are_rejected_without_hiding_a_valid_guard);
    RUN_TEST(test_invalid_transition_reconfiguration_preserves_existing_target);
    RUN_TEST(test_unbound_state_and_empty_callbacks_are_safe);
    RUN_TEST(test_repeated_destruction_releases_state_and_transition_callback_captures);
    RUN_TEST(test_mutable_guard_state_survives_successive_ticks);
    RUN_TEST(test_guard_can_append_transitions_without_invalidation_or_same_tick_execution);
    RUN_TEST(test_explicit_transition_from_state_logic_survives_old_guards_and_nested_run);
    RUN_TEST(test_explicit_transition_from_guard_stops_later_guards);
    RUN_TEST(test_entry_action_runs_once_per_automatic_or_explicit_entry);
    RUN_TEST(test_state_handles_remain_stable_when_additional_states_are_created);
    return UNITY_END();
}
