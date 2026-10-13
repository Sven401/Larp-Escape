#include <unity.h>

#include "GameDomain/GameProgress.h"
#include "../../src/GameDomain/GameProgress.cpp"

using larp::domain::GameProgress;
using larp::domain::defaultRoleDefinitions;

void test_round_reset_clears_only_round_progress_and_retry_streak()
{
    GameProgress progress;
    const auto &role = defaultRoleDefinitions()[0];
    const larp::domain::CardUid activeKeystone = role.uid;
    const larp::domain::CardUid previousKeystone = defaultRoleDefinitions()[1].uid;
    progress.roundCounter = 3;
    progress.currentKeyStone = activeKeystone;
    progress.lastKeyStone = previousKeystone;
    progress.correctCrystal = true;
    progress.currentRoleDefinition = &role;
    progress.seenKeyStones.push_back(role.uid);
    progress.puzzleProgress.record(role.crystals[0]);
    progress.incorrectCrystal = true;
    progress.retryCount = 4;
    for (int miss = 0; miss < 4; ++miss)
    {
        progress.keystoneMonitor.observe(false);
    }

    progress.resetRound();

    TEST_ASSERT_EQUAL_INT(3, progress.roundCounter);
    TEST_ASSERT_EQUAL_MEMORY(activeKeystone.data(), progress.currentKeyStone.data(),
                             activeKeystone.size());
    TEST_ASSERT_EQUAL_MEMORY(previousKeystone.data(), progress.lastKeyStone.data(),
                             previousKeystone.size());
    // Reset discards the pending transition signal while retaining the profile.
    TEST_ASSERT_FALSE(progress.correctCrystal);
    TEST_ASSERT_TRUE(progress.currentRoleDefinition == &role);
    TEST_ASSERT_EQUAL_INT(1, static_cast<int>(progress.seenKeyStones.size()));
    TEST_ASSERT_EQUAL_INT(0, static_cast<int>(progress.puzzleProgress.count()));
    TEST_ASSERT_FALSE(progress.incorrectCrystal);
    TEST_ASSERT_EQUAL_INT(0, progress.retryCount);
    for (int miss = 0; miss < 4; ++miss)
    {
        progress.keystoneMonitor.observe(false);
        TEST_ASSERT_FALSE(progress.keystoneMonitor.hasError());
    }
    progress.keystoneMonitor.observe(false);
    TEST_ASSERT_TRUE(progress.keystoneMonitor.hasError());
    progress.resetRound();
    TEST_ASSERT_TRUE(progress.keystoneMonitor.hasError());
}

void test_game_reset_clears_rounds_keystones_seen_tags_and_errors()
{
    GameProgress progress;
    const auto &role = defaultRoleDefinitions()[0];
    progress.roundCounter = 5;
    progress.currentKeyStone = role.uid;
    const larp::domain::CardUid previousKeystone = defaultRoleDefinitions()[1].uid;
    progress.lastKeyStone = previousKeystone;
    progress.correctCrystal = true;
    progress.currentRoleDefinition = &role;
    progress.seenKeyStones.push_back(role.uid);
    progress.puzzleProgress.record(role.crystals[0]);
    progress.incorrectCrystal = true;
    progress.retryCount = 3;
    for (std::uint8_t miss = 0; miss < larp::domain::KeystoneMonitor::ErrorThreshold; ++miss)
    {
        progress.keystoneMonitor.observe(false);
    }
    TEST_ASSERT_TRUE(progress.keystoneMonitor.hasError());

    progress.resetGame();

    const larp::domain::CardUid empty = {};
    TEST_ASSERT_EQUAL_INT(0, progress.roundCounter);
    TEST_ASSERT_EQUAL_MEMORY(empty.data(), progress.currentKeyStone.data(), empty.size());
    TEST_ASSERT_EQUAL_MEMORY(previousKeystone.data(), progress.lastKeyStone.data(),
                             previousKeystone.size());
    TEST_ASSERT_TRUE(progress.seenKeyStones.empty());
    TEST_ASSERT_EQUAL_INT(0, static_cast<int>(progress.puzzleProgress.count()));
    TEST_ASSERT_FALSE(progress.incorrectCrystal);
    TEST_ASSERT_EQUAL_INT(0, progress.retryCount);
    TEST_ASSERT_FALSE(progress.keystoneMonitor.hasError());
    // Reset discards the pending transition signal while retaining the profile.
    TEST_ASSERT_FALSE(progress.correctCrystal);
    TEST_ASSERT_TRUE(progress.currentRoleDefinition == &role);
}

void test_error_transition_waits_for_audio_then_consumes_crystal_before_keystone_error()
{
    GameProgress progress;
    progress.incorrectCrystal = true;
    for (std::uint8_t miss = 0; miss < larp::domain::KeystoneMonitor::ErrorThreshold; ++miss)
    {
        progress.keystoneMonitor.observe(false);
    }

    TEST_ASSERT_FALSE(progress.consumeErrorTransition(true));
    TEST_ASSERT_TRUE(progress.incorrectCrystal);
    TEST_ASSERT_TRUE(progress.keystoneMonitor.hasError());

    TEST_ASSERT_TRUE(progress.consumeErrorTransition(false));
    TEST_ASSERT_FALSE(progress.incorrectCrystal);
    TEST_ASSERT_TRUE(progress.keystoneMonitor.hasError());

    TEST_ASSERT_TRUE(progress.consumeErrorTransition(false));
    TEST_ASSERT_FALSE(progress.keystoneMonitor.hasError());
    TEST_ASSERT_FALSE(progress.consumeErrorTransition(false));
}
