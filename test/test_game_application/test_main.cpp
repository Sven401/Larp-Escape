#include <unity.h>

#include <cstdint>
#include <limits>

#include "GameFakes.h"

// Host suite compiles only the hardware-free domain implementation. Controller
// and state machine are normal library sources; no Arduino test shim is included.
#include "../../src/GameDomain/GameDefinitions.cpp"
#include "../../src/GameDomain/GameProgress.cpp"
#include "../../src/GameDomain/KeystoneMonitor.cpp"
#include "../../src/GameDomain/KeystoneTransition.cpp"
#include "../../src/GameDomain/PuzzleProgress.cpp"

using game_test::GameRig;
using larp::application::GamePhase;
using larp::domain::defaultRoleDefinitions;
using larp::ports::GameDiagnosticEvent;

static void assertPhase(GamePhase expected, const GameRig &rig) {
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(expected),
                            static_cast<std::uint8_t>(rig.phase()));
}

void test_construction_builds_the_graph_without_reading_or_writing_ports() {
    const bool diagnosticSettings[] = {true, false};
    for (bool diagnosticsEnabled : diagnosticSettings) {
        GameRig rig(123456U, diagnosticsEnabled);
        TEST_ASSERT_EQUAL_INT(9, rig.game.machine.stateCount());
        TEST_ASSERT_EQUAL_INT(-1, rig.game.machine.currentStateIndex());
        TEST_ASSERT_EQUAL_UINT(0U, rig.clock.nowReads);
        TEST_ASSERT_EQUAL_UINT(0U, rig.clock.delayCalls);
        TEST_ASSERT_EQUAL_UINT(0U, rig.crystals.reads);
        TEST_ASSERT_EQUAL_UINT(0U, rig.wake.reads);
        TEST_ASSERT_EQUAL_UINT(0U, rig.audio.busyReads);
        TEST_ASSERT_EQUAL_UINT(0U, rig.audio.wakeups);
        TEST_ASSERT_TRUE(rig.cards.timeouts.empty());
        TEST_ASSERT_TRUE(rig.audio.tracks.empty());
        TEST_ASSERT_TRUE(rig.lights.bursts.empty());
        TEST_ASSERT_TRUE(rig.lights.ripples.empty());
        TEST_ASSERT_TRUE(rig.lights.densities.empty());
        TEST_ASSERT_TRUE(rig.communicator.messages.empty());
        TEST_ASSERT_TRUE(rig.diagnostics.records.empty());
    }
}

void test_standby_requires_all_wake_inputs_and_maps_each_ripple() {
    GameRig rig;
    assertPhase(GamePhase::Standby, rig);
    TEST_ASSERT_FALSE(rig.game.snapshot().rolePresent);
    rig.wake.signals = {{true, false, true, false}};
    rig.tick();
    assertPhase(GamePhase::Standby, rig);
    TEST_ASSERT_EQUAL_UINT(2U, rig.lights.ripples.size());
    TEST_ASSERT_EQUAL_UINT8(0U, rig.lights.ripples[0].index);
    TEST_ASSERT_EQUAL_UINT8(22U, rig.lights.ripples[0].origin);
    TEST_ASSERT_EQUAL_UINT8(2U, rig.lights.ripples[1].index);
    TEST_ASSERT_EQUAL_UINT8(16U, rig.lights.ripples[1].origin);
    TEST_ASSERT_TRUE(rig.cards.timeouts.empty());
    rig.wake.signals = {{true, true, true, true}};
    rig.tick();
    assertPhase(GamePhase::Idle, rig);
    TEST_ASSERT_TRUE(rig.lights.bursts.empty());
    rig.tick();
    TEST_ASSERT_EQUAL_UINT(2U, rig.lights.bursts.size());
    TEST_ASSERT_EQUAL_UINT(1U, rig.lights.mutedPalettes);
    TEST_ASSERT_EQUAL_UINT8(1U, rig.lights.densities.back());
}

void test_empty_and_unknown_cards_cannot_select_a_role() {
    GameRig rig;
    rig.wakeToIdle();
    rig.tick(1000U);
    assertPhase(GamePhase::Idle, rig);
    TEST_ASSERT_FALSE(rig.game.snapshot().rolePresent);
    rig.cards.present = {{1U, 2U, 3U, 4U, 5U, 6U, 7U}};
    rig.tick(1000U);
    assertPhase(GamePhase::Idle, rig);
    TEST_ASSERT_FALSE(rig.game.snapshot().rolePresent);
    TEST_ASSERT_TRUE(rig.diagnostics.contains(GameDiagnosticEvent::RoleRejected));
    TEST_ASSERT_TRUE(rig.audio.tracks.empty());
}

void test_role_selection_uses_card_interval_and_entry_effects_once() {
    GameRig rig;
    rig.wakeToIdle();
    const auto &role = defaultRoleDefinitions()[1];
    rig.cards.present = role.uid;
    rig.tick(999U);
    TEST_ASSERT_TRUE(rig.cards.timeouts.empty());
    rig.tick(1U);
    assertPhase(GamePhase::WaitingForCrystals, rig);
    TEST_ASSERT_EQUAL_UINT(1U, rig.cards.timeouts.size());
    TEST_ASSERT_EQUAL_UINT16(100U, rig.cards.timeouts[0]);
    TEST_ASSERT_EQUAL_UINT(0U, rig.audio.wakeups);
    rig.tick();
    rig.tick();
    TEST_ASSERT_EQUAL_UINT(1U, rig.audio.wakeups);
    TEST_ASSERT_EQUAL_UINT8(4U, rig.lights.speeds.back());
    TEST_ASSERT_EQUAL_UINT8(2U, rig.lights.densities.back());
    const auto status = rig.game.snapshot();
    TEST_ASSERT_TRUE(status.rolePresent);
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(role.id),
                            static_cast<std::uint8_t>(status.roleId));
    TEST_ASSERT_EQUAL_UINT8(0U, status.crystalCount);
    TEST_ASSERT_EQUAL_UINT(0U, rig.clock.delayCalls);
}

void test_crystals_detected_while_busy_keep_unique_audio_order() {
    GameRig rig;
    rig.wakeToIdle();
    const auto &role = defaultRoleDefinitions()[2];
    rig.selectRole(role);
    rig.crystals.positions = {role.crystals[0].position};
    rig.tick(100U);
    assertPhase(GamePhase::FirstCrystalPlaced, rig);
    rig.tick();
    TEST_ASSERT_TRUE(rig.audio.busy);
    TEST_ASSERT_EQUAL_UINT16(role.crystals[0].audioTrack, rig.audio.tracks[0]);

    rig.crystals.positions = {role.crystals[0].position, role.crystals[1].position};
    rig.tick(100U);
    rig.tick(100U);
    assertPhase(GamePhase::FirstCrystalPlaced, rig);
    TEST_ASSERT_EQUAL_UINT8(2U, rig.game.snapshot().crystalCount);
    TEST_ASSERT_EQUAL_UINT(2U, rig.lights.palettes.size());
    rig.crystals.positions.push_back(role.crystals[2].position);
    rig.tick(100U);
    TEST_ASSERT_EQUAL_UINT8(3U, rig.game.snapshot().crystalCount);
    TEST_ASSERT_EQUAL_UINT(1U, rig.audio.tracks.size());

    rig.audio.finish();
    rig.tick();
    assertPhase(GamePhase::SecondCrystalPlaced, rig);
    rig.tick();
    TEST_ASSERT_EQUAL_UINT16(role.crystals[1].audioTrack, rig.audio.tracks[1]);
    rig.audio.finish();
    rig.tick();
    assertPhase(GamePhase::ThirdCrystalPlaced, rig);
    rig.tick();
    TEST_ASSERT_EQUAL_UINT16(role.crystals[2].audioTrack, rig.audio.tracks[2]);
    rig.audio.finish();
    rig.tick(499U);
    assertPhase(GamePhase::ThirdCrystalPlaced, rig);
    rig.tick(1U);
    assertPhase(GamePhase::GameCompleted, rig);
    TEST_ASSERT_EQUAL_UINT(3U, rig.audio.tracks.size());
    rig.tick();
    TEST_ASSERT_EQUAL_UINT16(role.audioTrack, rig.audio.tracks[3]);
    TEST_ASSERT_EQUAL_UINT32(1U, rig.game.snapshot().roundCount);
    TEST_ASSERT_EQUAL_UINT8(1U, rig.game.snapshot().seenRoleCount);
    rig.tick(100U);
    TEST_ASSERT_EQUAL_UINT(4U, rig.audio.tracks.size());
    TEST_ASSERT_EQUAL_UINT(0U, rig.clock.delayCalls);
}

void test_invalid_crystal_waits_for_busy_audio_then_resets_game() {
    GameRig rig;
    rig.wakeToIdle();
    const auto &role = defaultRoleDefinitions()[0];
    rig.selectRole(role);
    rig.audio.busy = true;
    rig.crystals.positions = {{0U, larp::domain::GridColumn::A}};
    rig.tick(100U);
    assertPhase(GamePhase::WaitingForCrystals, rig);
    TEST_ASSERT_TRUE(rig.game.snapshot().incorrectCrystal);
    TEST_ASSERT_TRUE(rig.audio.tracks.empty());
    rig.audio.finish();
    rig.tick();
    assertPhase(GamePhase::Error, rig);
    rig.tick();
    TEST_ASSERT_EQUAL_UINT16(998U, rig.audio.tracks.back());
    TEST_ASSERT_FALSE(rig.game.snapshot().incorrectCrystal);
    TEST_ASSERT_EQUAL_UINT8(0U, rig.game.snapshot().crystalCount);
    TEST_ASSERT_EQUAL_UINT32(0U, rig.game.snapshot().roundCount);
    rig.audio.finish();
    rig.tick(499U);
    assertPhase(GamePhase::Error, rig);
    rig.tick(1U);
    assertPhase(GamePhase::Idle, rig);
    TEST_ASSERT_EQUAL_UINT(0U, rig.clock.delayCalls);
}

void test_matching_keystone_breaks_miss_streak_and_fifth_miss_raises_error() {
    GameRig rig;
    rig.wakeToIdle();
    const auto &role = defaultRoleDefinitions()[0];
    rig.selectRole(role);
    rig.cards.present = {};
    for (unsigned int index = 0U; index < 4U; ++index)
        rig.tick(2000U);
    assertPhase(GamePhase::WaitingForCrystals, rig);
    rig.cards.present = role.uid;
    rig.tick(2000U);
    rig.cards.present = {};
    for (unsigned int index = 0U; index < 4U; ++index)
        rig.tick(2000U);
    assertPhase(GamePhase::WaitingForCrystals, rig);
    rig.audio.busy = true;
    rig.tick(2000U);
    TEST_ASSERT_TRUE(rig.game.snapshot().keystoneError);
    assertPhase(GamePhase::WaitingForCrystals, rig);
    rig.audio.finish();
    rig.tick();
    assertPhase(GamePhase::Error, rig);
    rig.tick();
    TEST_ASSERT_EQUAL_UINT16(998U, rig.audio.tracks.back());
    TEST_ASSERT_FALSE(rig.game.snapshot().keystoneError);
}

void test_idle_timeout_has_exact_boundary() {
    GameRig rig;
    rig.wakeToIdle();
    rig.tick(599999U);
    assertPhase(GamePhase::Idle, rig);
    rig.tick(1U);
    assertPhase(GamePhase::Standby, rig);
    rig.tick();
    TEST_ASSERT_EQUAL_UINT8(0U, rig.lights.densities.back());
    TEST_ASSERT_EQUAL_UINT(0U, rig.clock.delayCalls);
}

void test_waiting_timeout_remains_valid_across_millisecond_wrap() {
    GameRig rig(std::numeric_limits<std::uint32_t>::max() - 500U);
    rig.wakeToIdle();
    rig.selectRole(defaultRoleDefinitions()[3]);
    const std::uint32_t waitingEntered = rig.clock.now;
    rig.tick(599999U);
    assertPhase(GamePhase::WaitingForCrystals, rig);
    rig.tick(1U);
    assertPhase(GamePhase::Standby, rig);
    TEST_ASSERT_EQUAL_UINT32(600000U, rig.clock.now - waitingEntered);
    rig.tick();
    TEST_ASSERT_EQUAL_UINT8(0U, rig.game.snapshot().crystalCount);
}

void test_repeated_role_counts_rounds_but_history_stays_unique() {
    GameRig rig;
    rig.wakeToIdle();
    const auto &role = defaultRoleDefinitions()[4];
    rig.completeRound(role);
    assertPhase(GamePhase::GameCompleted, rig);
    rig.returnToIdle();
    assertPhase(GamePhase::Idle, rig);
    TEST_ASSERT_EQUAL_UINT8(0U, rig.game.snapshot().crystalCount);
    TEST_ASSERT_EQUAL_UINT32(1U, rig.game.snapshot().roundCount);
    rig.clearRoleCardForNextRound();
    rig.completeRound(role);
    assertPhase(GamePhase::GameCompleted, rig);
    TEST_ASSERT_EQUAL_UINT32(2U, rig.game.snapshot().roundCount);
    TEST_ASSERT_EQUAL_UINT8(1U, rig.game.snapshot().seenRoleCount);
}

static void reachBonus(GameRig &rig) {
    rig.wakeToIdle();
    const auto &roles = defaultRoleDefinitions();
    for (std::size_t index = 0U; index < roles.size(); ++index) {
        rig.completeRound(roles[index]);
        assertPhase(GamePhase::GameCompleted, rig);
        TEST_ASSERT_EQUAL_UINT32(index + 1U, rig.game.snapshot().roundCount);
        TEST_ASSERT_EQUAL_UINT8(index + 1U, rig.game.snapshot().seenRoleCount);
        rig.returnToIdle();
        if (index + 1U < roles.size()) {
            assertPhase(GamePhase::Idle, rig);
            rig.clearRoleCardForNextRound();
        }
    }
    assertPhase(GamePhase::Bonus, rig);
    TEST_ASSERT_EQUAL_UINT16(900U, rig.audio.tracks.back());
}

void test_six_distinct_roles_reach_bonus_and_send_single_activation() {
    GameRig rig;
    reachBonus(rig);
    TEST_ASSERT_TRUE(rig.communicator.messages.empty());
    rig.tick(500U);
    assertPhase(GamePhase::Bonus, rig);
    TEST_ASSERT_TRUE(rig.communicator.messages.empty());
    rig.audio.finish();
    rig.tick();
    rig.tick(500U);
    assertPhase(GamePhase::Idle, rig);
    TEST_ASSERT_EQUAL_UINT(1U, rig.communicator.messages.size());
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(larp::ports::GameMessage::Activate),
                            static_cast<std::uint8_t>(rig.communicator.messages[0]));
    TEST_ASSERT_EQUAL_UINT32(0U, rig.game.snapshot().roundCount);
    TEST_ASSERT_EQUAL_UINT8(0U, rig.game.snapshot().seenRoleCount);
    rig.tick();
    rig.tick(100U);
    TEST_ASSERT_EQUAL_UINT(1U, rig.communicator.messages.size());
}

void test_failed_bonus_send_currently_resets_without_retry() {
    GameRig rig;
    rig.communicator.succeeds = false;
    reachBonus(rig);
    rig.audio.finish();
    rig.tick(500U);
    assertPhase(GamePhase::Idle, rig);
    TEST_ASSERT_EQUAL_UINT(1U, rig.communicator.messages.size());
    TEST_ASSERT_EQUAL_UINT8(0U, rig.game.snapshot().seenRoleCount);
    TEST_ASSERT_TRUE(rig.diagnostics.contains(GameDiagnosticEvent::ActivationSent));
    TEST_ASSERT_EQUAL_UINT32(0U, rig.diagnostics.records.back().value);
    rig.tick();
    rig.tick(1000U);
    TEST_ASSERT_EQUAL_UINT(1U, rig.communicator.messages.size());
}

void test_snapshot_is_a_value_copy_and_default_diagnostics_are_optional() {
    GameRig rig(0U, false);
    rig.wakeToIdle();
    const auto before = rig.game.snapshot();
    rig.selectRole(defaultRoleDefinitions()[5]);
    const auto after = rig.game.snapshot();
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(GamePhase::Idle),
                            static_cast<std::uint8_t>(before.phase));
    TEST_ASSERT_FALSE(before.rolePresent);
    TEST_ASSERT_TRUE(after.rolePresent);
    assertPhase(GamePhase::WaitingForCrystals, rig);
    TEST_ASSERT_TRUE(rig.diagnostics.records.empty());
}

void test_completed_round_currently_needs_two_groups_of_empty_card_reads() {
    GameRig rig;
    rig.wakeToIdle();
    rig.completeRound(defaultRoleDefinitions()[0]);
    rig.returnToIdle();
    rig.cards.present = {};
    for (unsigned int index = 0U; index < 5U; ++index)
        rig.tick(1000U);
    rig.cards.present = defaultRoleDefinitions()[1].uid;
    rig.tick(1000U);
    assertPhase(GamePhase::Idle, rig);
    TEST_ASSERT_TRUE(rig.diagnostics.contains(GameDiagnosticEvent::KeystoneBoardNotCleared));
}

void test_scan_currently_accepts_first_new_valid_position_before_invalid_position() {
    GameRig rig;
    rig.wakeToIdle();
    rig.selectRole(defaultRoleDefinitions()[0]);
    rig.crystals.positions = {defaultRoleDefinitions()[0].crystals[0].position,
                              {0U, larp::domain::GridColumn::A}};
    rig.tick(100U);
    assertPhase(GamePhase::FirstCrystalPlaced, rig);
    TEST_ASSERT_FALSE(rig.game.snapshot().incorrectCrystal);
    TEST_ASSERT_EQUAL_UINT8(1U, rig.game.snapshot().crystalCount);
    rig.tick();
    rig.tick(100U);
    TEST_ASSERT_TRUE(rig.game.snapshot().incorrectCrystal);
    rig.audio.finish();
    rig.tick();
    assertPhase(GamePhase::Error, rig);
}

void test_repeated_setup_does_not_duplicate_the_state_graph() {
    GameRig rig;
    TEST_ASSERT_EQUAL_INT(9, rig.game.machine.stateCount());
    rig.game.setup();
    rig.game.setupTransitions();
    rig.game.setup();
    TEST_ASSERT_EQUAL_INT(9, rig.game.machine.stateCount());
    rig.wakeToIdle();
    rig.selectRole(defaultRoleDefinitions()[0]);
    assertPhase(GamePhase::WaitingForCrystals, rig);
    TEST_ASSERT_EQUAL_UINT(1U, rig.audio.wakeups);
}

void test_error_after_bonus_starts_a_fresh_wait_even_with_delayed_busy_signal() {
    GameRig rig;
    reachBonus(rig);
    rig.audio.finish();
    rig.tick(500U);
    rig.tick();
    assertPhase(GamePhase::Idle, rig);
    rig.clearRoleCardForNextRound();
    rig.selectRole(defaultRoleDefinitions()[0]);
    rig.audio.playSetsBusy = false;
    rig.crystals.positions = {{0U, larp::domain::GridColumn::A}};
    rig.tick(100U);
    assertPhase(GamePhase::Error, rig);
    rig.tick();
    TEST_ASSERT_EQUAL_UINT16(998U, rig.audio.tracks.back());
    assertPhase(GamePhase::Error, rig);
    rig.tick(499U);
    assertPhase(GamePhase::Error, rig);
    rig.tick(1U);
    assertPhase(GamePhase::Idle, rig);
}
void test_timeout_discards_pending_crystal_before_the_next_role_starts() {
    GameRig rig;
    const auto &role = defaultRoleDefinitions()[0];
    rig.wakeToIdle();
    rig.selectRole(role);
    rig.audio.busy = true;
    rig.crystals.positions = {role.crystals[0].position};
    rig.tick(100U);
    assertPhase(GamePhase::WaitingForCrystals, rig);
    TEST_ASSERT_EQUAL_UINT8(1U, rig.game.snapshot().crystalCount);

    // A pending valid crystal cannot transition while the old audio is busy.
    // The timeout cancels this round; no input from it may start the next round.
    rig.tick(600000U);
    assertPhase(GamePhase::Standby, rig);
    rig.crystals.positions.clear();
    rig.cards.present = {};
    rig.audio.finish();
    rig.tick();
    TEST_ASSERT_EQUAL_UINT8(0U, rig.game.snapshot().crystalCount);
    rig.wakeToIdle();
    assertPhase(GamePhase::Idle, rig);
    rig.selectRole(role);
    assertPhase(GamePhase::WaitingForCrystals, rig);
    TEST_ASSERT_EQUAL_UINT8(0U, rig.game.snapshot().crystalCount);
    rig.tick();
    assertPhase(GamePhase::WaitingForCrystals, rig);
    TEST_ASSERT_FALSE(rig.diagnostics.contains(GameDiagnosticEvent::CrystalAudioUnavailable));
}

void setUp() {}
void tearDown() {}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_construction_builds_the_graph_without_reading_or_writing_ports);
    RUN_TEST(test_standby_requires_all_wake_inputs_and_maps_each_ripple);
    RUN_TEST(test_empty_and_unknown_cards_cannot_select_a_role);
    RUN_TEST(test_role_selection_uses_card_interval_and_entry_effects_once);
    RUN_TEST(test_crystals_detected_while_busy_keep_unique_audio_order);
    RUN_TEST(test_invalid_crystal_waits_for_busy_audio_then_resets_game);
    RUN_TEST(test_matching_keystone_breaks_miss_streak_and_fifth_miss_raises_error);
    RUN_TEST(test_idle_timeout_has_exact_boundary);
    RUN_TEST(test_waiting_timeout_remains_valid_across_millisecond_wrap);
    RUN_TEST(test_repeated_role_counts_rounds_but_history_stays_unique);
    RUN_TEST(test_six_distinct_roles_reach_bonus_and_send_single_activation);
    RUN_TEST(test_failed_bonus_send_currently_resets_without_retry);
    RUN_TEST(test_snapshot_is_a_value_copy_and_default_diagnostics_are_optional);
    RUN_TEST(test_completed_round_currently_needs_two_groups_of_empty_card_reads);
    RUN_TEST(test_scan_currently_accepts_first_new_valid_position_before_invalid_position);
    RUN_TEST(test_repeated_setup_does_not_duplicate_the_state_graph);
    RUN_TEST(test_error_after_bonus_starts_a_fresh_wait_even_with_delayed_busy_signal);
    RUN_TEST(test_timeout_discards_pending_crystal_before_the_next_role_starts);
    return UNITY_END();
}