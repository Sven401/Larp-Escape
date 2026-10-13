#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <unity.h>

#include "../../src/Companion/StatusProtocol.cpp"
#include "Companion/StatusProtocol.h"

using larp::application::GamePhase;
using larp::application::GameSnapshot;
using larp::companion::encodeStatus;
using larp::companion::maxStatusJsonBytes;
using larp::companion::StatusEncodeResult;
using larp::domain::RoleId;

void setUp() {}
void tearDown() {}

namespace {
GameSnapshot idleSnapshot() {
    return {GamePhase::Idle, false, RoleId::Puppenspielerin, 0U, 0U, 0U, false, false, false};
}

void assertEncodeResult(StatusEncodeResult expected, StatusEncodeResult actual) {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(expected), static_cast<int>(actual));
}

constexpr char idleJson[] =
    "{\"schemaVersion\":1,\"capabilities\":[\"status.read\"],\"game\":{"
    "\"phase\":\"idle\",\"role\":null,\"crystalCount\":0,\"roundCount\":0,"
    "\"seenRoleCount\":0,\"incorrectCrystal\":false,\"keystoneError\":false,"
    "\"audioBusy\":false}}";
} // namespace

void test_read_only_status_matches_the_version_1_golden_document() {
    char output[maxStatusJsonBytes] = {};
    std::size_t written = 999U;
    assertEncodeResult(StatusEncodeResult::Success,
                       encodeStatus(idleSnapshot(), output, sizeof(output), written));
    TEST_ASSERT_EQUAL_STRING(idleJson, output);
    TEST_ASSERT_EQUAL_UINT32(sizeof(idleJson) - 1U, written);
    TEST_ASSERT_NULL(std::strstr(output, "uid"));
    TEST_ASSERT_NULL(std::strstr(output, "ACTIVATE"));
}

void test_every_game_phase_has_a_stable_wire_name() {
    const std::array<GamePhase, 9> phases = {
        {GamePhase::Standby, GamePhase::Idle, GamePhase::WaitingForCrystals,
         GamePhase::FirstCrystalPlaced, GamePhase::SecondCrystalPlaced,
         GamePhase::ThirdCrystalPlaced, GamePhase::GameCompleted, GamePhase::Bonus,
         GamePhase::Error}};
    const std::array<const char *, 9> names = {
        {"standby", "idle", "waiting_for_crystals", "first_crystal_placed", "second_crystal_placed",
         "third_crystal_placed", "game_completed", "bonus", "error"}};
    for (std::size_t index = 0U; index < phases.size(); ++index) {
        TEST_ASSERT_EQUAL_STRING(names[index], larp::companion::phaseWireName(phases[index]));
        GameSnapshot snapshot = idleSnapshot();
        snapshot.phase = phases[index];
        char output[maxStatusJsonBytes] = {};
        char expectedField[64] = {};
        std::size_t written = 0U;
        assertEncodeResult(StatusEncodeResult::Success,
                           encodeStatus(snapshot, output, sizeof(output), written));
        std::snprintf(expectedField, sizeof(expectedField), "\"phase\":\"%s\"", names[index]);
        TEST_ASSERT_NOT_NULL(std::strstr(output, expectedField));
    }
}

void test_every_role_has_a_stable_ascii_wire_name() {
    const std::array<RoleId, 6> roles = {{RoleId::Puppenspielerin, RoleId::Krieger, RoleId::Narr,
                                          RoleId::Abenteurerin, RoleId::Ueberlebende,
                                          RoleId::Wissende}};
    const std::array<const char *, 6> names = {
        {"puppenspielerin", "krieger", "narr", "abenteurerin", "ueberlebende", "wissende"}};
    for (std::size_t index = 0U; index < roles.size(); ++index) {
        TEST_ASSERT_EQUAL_STRING(names[index], larp::companion::roleWireName(roles[index]));
        GameSnapshot snapshot = idleSnapshot();
        snapshot.rolePresent = true;
        snapshot.roleId = roles[index];
        char output[maxStatusJsonBytes] = {};
        char expectedField[64] = {};
        std::size_t written = 0U;
        assertEncodeResult(StatusEncodeResult::Success,
                           encodeStatus(snapshot, output, sizeof(output), written));
        std::snprintf(expectedField, sizeof(expectedField), "\"role\":\"%s\"", names[index]);
        TEST_ASSERT_NOT_NULL(std::strstr(output, expectedField));
    }
}

void test_maximum_counters_and_boolean_values_fit_the_fixed_status_capacity() {
    const GameSnapshot snapshot = {GamePhase::SecondCrystalPlaced,
                                   true,
                                   RoleId::Puppenspielerin,
                                   3U,
                                   std::numeric_limits<std::uint32_t>::max(),
                                   6U,
                                   true,
                                   true,
                                   true};
    constexpr char expected[] =
        "{\"schemaVersion\":1,\"capabilities\":[\"status.read\"],\"game\":{"
        "\"phase\":\"second_crystal_placed\",\"role\":\"puppenspielerin\","
        "\"crystalCount\":3,\"roundCount\":4294967295,\"seenRoleCount\":6,"
        "\"incorrectCrystal\":true,\"keystoneError\":true,\"audioBusy\":true}}";
    char output[maxStatusJsonBytes] = {};
    std::size_t written = 0U;
    assertEncodeResult(StatusEncodeResult::Success,
                       encodeStatus(snapshot, output, sizeof(output), written));
    TEST_ASSERT_EQUAL_STRING(expected, output);
    TEST_ASSERT_EQUAL_UINT32(sizeof(expected) - 1U, written);
    TEST_ASSERT_TRUE(written + 1U <= maxStatusJsonBytes);
}

void test_exact_buffer_size_includes_the_terminator_and_preserves_canaries() {
    std::array<char, maxStatusJsonBytes + 2U> storage;
    storage.fill('Z');
    char *output = storage.data() + 1U;
    const std::size_t capacity = sizeof(idleJson);
    std::size_t written = 999U;
    assertEncodeResult(StatusEncodeResult::Success,
                       encodeStatus(idleSnapshot(), output, capacity, written));
    TEST_ASSERT_EQUAL_STRING(idleJson, output);
    TEST_ASSERT_EQUAL_UINT32(capacity - 1U, written);
    TEST_ASSERT_EQUAL_CHAR('Z', storage[0]);
    TEST_ASSERT_EQUAL_CHAR('Z', storage[capacity + 1U]);
    TEST_ASSERT_EQUAL_CHAR('\0', output[capacity - 1U]);
}

void test_every_smaller_capacity_fails_empty_without_overwriting_canaries() {
    for (std::size_t capacity = 0U; capacity < sizeof(idleJson); ++capacity) {
        std::array<char, maxStatusJsonBytes + 2U> storage;
        storage.fill('Z');
        char *output = storage.data() + 1U;
        std::size_t written = 999U;
        assertEncodeResult(StatusEncodeResult::BufferTooSmall,
                           encodeStatus(idleSnapshot(), output, capacity, written));
        TEST_ASSERT_EQUAL_UINT32(0U, written);
        TEST_ASSERT_EQUAL_CHAR('Z', storage[0]);
        TEST_ASSERT_EQUAL_CHAR('Z', storage[capacity + 1U]);
        TEST_ASSERT_EQUAL_CHAR(capacity == 0U ? 'Z' : '\0', output[0]);
    }
}

void test_null_buffers_fail_without_claiming_written_bytes() {
    std::size_t written = 999U;
    assertEncodeResult(StatusEncodeResult::BufferTooSmall,
                       encodeStatus(idleSnapshot(), nullptr, maxStatusJsonBytes, written));
    TEST_ASSERT_EQUAL_UINT32(0U, written);
    written = 999U;
    assertEncodeResult(StatusEncodeResult::BufferTooSmall,
                       encodeStatus(idleSnapshot(), nullptr, 0U, written));
    TEST_ASSERT_EQUAL_UINT32(0U, written);
}

void test_unknown_enumerations_are_rejected_when_they_are_meaningful() {
    const GamePhase unknownPhase = static_cast<GamePhase>(255U);
    const RoleId unknownRole = static_cast<RoleId>(255U);
    TEST_ASSERT_NULL(larp::companion::phaseWireName(unknownPhase));
    TEST_ASSERT_NULL(larp::companion::roleWireName(unknownRole));
    GameSnapshot snapshot = idleSnapshot();
    snapshot.phase = unknownPhase;
    char output[maxStatusJsonBytes] = "old data";
    std::size_t written = 999U;
    assertEncodeResult(StatusEncodeResult::InvalidSnapshot,
                       encodeStatus(snapshot, output, sizeof(output), written));
    TEST_ASSERT_EQUAL_UINT32(0U, written);
    TEST_ASSERT_EQUAL_STRING("", output);
    snapshot = idleSnapshot();
    snapshot.rolePresent = true;
    snapshot.roleId = unknownRole;
    output[0] = 'X';
    written = 999U;
    assertEncodeResult(StatusEncodeResult::InvalidSnapshot,
                       encodeStatus(snapshot, output, sizeof(output), written));
    TEST_ASSERT_EQUAL_UINT32(0U, written);
    TEST_ASSERT_EQUAL_STRING("", output);
}

void test_absent_role_ignores_its_unused_id_and_remains_null() {
    GameSnapshot snapshot = idleSnapshot();
    snapshot.roleId = static_cast<RoleId>(255U);
    char output[maxStatusJsonBytes] = {};
    std::size_t written = 0U;
    assertEncodeResult(StatusEncodeResult::Success,
                       encodeStatus(snapshot, output, sizeof(output), written));
    TEST_ASSERT_EQUAL_STRING(idleJson, output);
}

void test_invalid_progress_counts_are_rejected_and_clear_old_output() {
    GameSnapshot snapshot = idleSnapshot();
    snapshot.crystalCount = 4U;
    char output[maxStatusJsonBytes] = "old data";
    std::size_t written = 999U;
    assertEncodeResult(StatusEncodeResult::InvalidSnapshot,
                       encodeStatus(snapshot, output, sizeof(output), written));
    TEST_ASSERT_EQUAL_UINT32(0U, written);
    TEST_ASSERT_EQUAL_STRING("", output);
    snapshot = idleSnapshot();
    snapshot.seenRoleCount = 7U;
    output[0] = 'X';
    written = 999U;
    assertEncodeResult(StatusEncodeResult::InvalidSnapshot,
                       encodeStatus(snapshot, output, sizeof(output), written));
    TEST_ASSERT_EQUAL_UINT32(0U, written);
    TEST_ASSERT_EQUAL_STRING("", output);
}

void test_standby_can_report_a_retained_role_without_implying_card_presence() {
    const GameSnapshot snapshot = {
        GamePhase::Standby, true, RoleId::Wissende, 0U, 2U, 2U, false, false, false};
    char output[maxStatusJsonBytes] = {};
    std::size_t written = 0U;
    assertEncodeResult(StatusEncodeResult::Success,
                       encodeStatus(snapshot, output, sizeof(output), written));
    TEST_ASSERT_NOT_NULL(std::strstr(output, "\"phase\":\"standby\""));
    TEST_ASSERT_NOT_NULL(std::strstr(output, "\"role\":\"wissende\""));
    TEST_ASSERT_NOT_NULL(std::strstr(output, "\"roundCount\":2"));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_read_only_status_matches_the_version_1_golden_document);
    RUN_TEST(test_every_game_phase_has_a_stable_wire_name);
    RUN_TEST(test_every_role_has_a_stable_ascii_wire_name);
    RUN_TEST(test_maximum_counters_and_boolean_values_fit_the_fixed_status_capacity);
    RUN_TEST(test_exact_buffer_size_includes_the_terminator_and_preserves_canaries);
    RUN_TEST(test_every_smaller_capacity_fails_empty_without_overwriting_canaries);
    RUN_TEST(test_null_buffers_fail_without_claiming_written_bytes);
    RUN_TEST(test_unknown_enumerations_are_rejected_when_they_are_meaningful);
    RUN_TEST(test_absent_role_ignores_its_unused_id_and_remains_null);
    RUN_TEST(test_invalid_progress_counts_are_rejected_and_clear_old_output);
    RUN_TEST(test_standby_can_report_a_retained_role_without_implying_card_presence);
    return UNITY_END();
}
