#include <algorithm>
#include <type_traits>
#include <unity.h>
#include <utility>

#include "../../src/GameDomain/GameDefinitions.cpp"
#include "../../src/GameDomain/GameProgress.cpp"
#include "../../src/GameDomain/KeystoneMonitor.cpp"
#include "../../src/GameDomain/PuzzleProgress.cpp"
#include "GameDomain/GameProgress.h"
#include "GameDomain/KnownRoleHistory.h"

using larp::domain::CardUid;
using larp::domain::defaultRoleDefinitions;
using larp::domain::GameProgress;
using larp::domain::KnownRoleHistory;

static_assert(std::is_trivially_copyable<KnownRoleHistory>::value,
              "The bounded history keeps its entries in value storage");
static_assert(std::is_const<typename std::remove_reference<
                  decltype(*std::declval<KnownRoleHistory &>().begin())>::type>::value,
              "Iteration cannot bypass known-role and uniqueness checks");

void setUp() {}
void tearDown() {}

void test_empty_history_capacity_matches_known_role_catalogue() {
    KnownRoleHistory history;
    TEST_ASSERT_TRUE(history.empty());
    TEST_ASSERT_EQUAL_UINT(0, static_cast<unsigned>(history.size()));
    TEST_ASSERT_EQUAL_UINT(static_cast<unsigned>(defaultRoleDefinitions().size()),
                           static_cast<unsigned>(history.capacity()));
    TEST_ASSERT_TRUE(history.begin() == history.end());
    for (const auto &role : defaultRoleDefinitions())
        TEST_ASSERT_FALSE(history.contains(role.uid));
}

void test_unknown_and_empty_cards_do_not_enter_the_history() {
    KnownRoleHistory history;
    const CardUid empty = {};
    const CardUid unknown = {{1, 2, 3, 4, 5, 6, 7}};
    CardUid nearKnown = defaultRoleDefinitions()[0].uid;
    nearKnown[6] ^= 0xFF;
    TEST_ASSERT_FALSE(history.push_back(empty));
    TEST_ASSERT_FALSE(history.push_back(unknown));
    TEST_ASSERT_FALSE(history.push_back(nearKnown));
    TEST_ASSERT_TRUE(history.empty());
    TEST_ASSERT_FALSE(history.contains(empty));
    TEST_ASSERT_FALSE(history.contains(unknown));
}

void test_duplicate_role_card_keeps_one_history_entry() {
    KnownRoleHistory history;
    const CardUid role = defaultRoleDefinitions()[2].uid;
    TEST_ASSERT_TRUE(history.push_back(role));
    for (int repeatedRead = 0; repeatedRead < 32; ++repeatedRead)
        TEST_ASSERT_FALSE(history.push_back(role));
    TEST_ASSERT_EQUAL_UINT(1, static_cast<unsigned>(history.size()));
    TEST_ASSERT_TRUE(history.contains(role));
    TEST_ASSERT_EQUAL_MEMORY(role.data(), history.begin()->data(), role.size());
}

void test_full_history_retains_order_and_rejects_further_entries() {
    KnownRoleHistory history;
    const auto &roles = defaultRoleDefinitions();
    for (std::size_t remaining = roles.size(); remaining > 0; --remaining)
        TEST_ASSERT_TRUE(history.push_back(roles[remaining - 1].uid));
    TEST_ASSERT_EQUAL_UINT(static_cast<unsigned>(history.capacity()),
                           static_cast<unsigned>(history.size()));

    for (const auto &role : roles) {
        TEST_ASSERT_TRUE(history.contains(role.uid));
        TEST_ASSERT_FALSE(history.push_back(role.uid));
    }
    const CardUid unknown = {{1, 2, 3, 4, 5, 6, 7}};
    TEST_ASSERT_FALSE(history.push_back(unknown));
    TEST_ASSERT_FALSE(history.contains(unknown));
    TEST_ASSERT_EQUAL_UINT(static_cast<unsigned>(roles.size()),
                           static_cast<unsigned>(history.size()));

    std::size_t position = 0;
    for (const CardUid &recorded : history) {
        const CardUid &expected = roles[roles.size() - 1 - position++].uid;
        TEST_ASSERT_EQUAL_MEMORY(expected.data(), recorded.data(), expected.size());
    }
    TEST_ASSERT_EQUAL_UINT(static_cast<unsigned>(roles.size()), static_cast<unsigned>(position));
}

void test_collection_api_supports_const_std_find_without_mutation() {
    KnownRoleHistory history;
    const CardUid known = defaultRoleDefinitions()[4].uid;
    const CardUid missing = defaultRoleDefinitions()[5].uid;
    history.push_back(known);
    const KnownRoleHistory &readOnly = history;
    TEST_ASSERT_TRUE(std::find(readOnly.begin(), readOnly.end(), known) != readOnly.end());
    TEST_ASSERT_TRUE(std::find(history.begin(), history.end(), missing) == history.end());
    TEST_ASSERT_EQUAL_UINT(1, static_cast<unsigned>(readOnly.size()));
}

void test_clear_reuses_every_slot_without_old_duplicate_state() {
    KnownRoleHistory history;
    for (int game = 0; game < 32; ++game) {
        for (const auto &role : defaultRoleDefinitions())
            TEST_ASSERT_TRUE(history.push_back(role.uid));
        TEST_ASSERT_EQUAL_UINT(static_cast<unsigned>(history.capacity()),
                               static_cast<unsigned>(history.size()));
        history.clear();
        TEST_ASSERT_TRUE(history.empty());
        TEST_ASSERT_TRUE(history.begin() == history.end());
        for (const auto &role : defaultRoleDefinitions())
            TEST_ASSERT_FALSE(history.contains(role.uid));
    }
}

void test_round_reset_preserves_history_and_game_reset_releases_its_slots() {
    GameProgress progress;
    for (const auto &role : defaultRoleDefinitions())
        TEST_ASSERT_TRUE(progress.seenKeyStones.push_back(role.uid));
    progress.resetRound();
    TEST_ASSERT_EQUAL_UINT(static_cast<unsigned>(defaultRoleDefinitions().size()),
                           static_cast<unsigned>(progress.seenKeyStones.size()));
    progress.resetGame();
    TEST_ASSERT_TRUE(progress.seenKeyStones.empty());
    for (const auto &role : defaultRoleDefinitions())
        TEST_ASSERT_TRUE(progress.seenKeyStones.push_back(role.uid));
    TEST_ASSERT_EQUAL_UINT(static_cast<unsigned>(defaultRoleDefinitions().size()),
                           static_cast<unsigned>(progress.seenKeyStones.size()));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_empty_history_capacity_matches_known_role_catalogue);
    RUN_TEST(test_unknown_and_empty_cards_do_not_enter_the_history);
    RUN_TEST(test_duplicate_role_card_keeps_one_history_entry);
    RUN_TEST(test_full_history_retains_order_and_rejects_further_entries);
    RUN_TEST(test_collection_api_supports_const_std_find_without_mutation);
    RUN_TEST(test_clear_reuses_every_slot_without_old_duplicate_state);
    RUN_TEST(test_round_reset_preserves_history_and_game_reset_releases_its_slots);
    return UNITY_END();
}
