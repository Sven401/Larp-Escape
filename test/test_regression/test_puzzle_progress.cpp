#include <unity.h>

#include "GameDomain/PuzzleProgress.h"
#include "../../src/GameDomain/PuzzleProgress.cpp"

using larp::domain::CrystalRecordResult;
using larp::domain::PuzzleProgress;
using larp::domain::defaultRoleDefinitions;

void test_puzzle_progress_records_unique_crystals_in_order_and_completes_at_three()
{
    PuzzleProgress progress;
    const auto &role = defaultRoleDefinitions()[0];

    TEST_ASSERT_EQUAL_INT(0, static_cast<int>(progress.count()));
    TEST_ASSERT_FALSE(progress.isComplete());
    TEST_ASSERT_FALSE(progress.hasPendingAudio());
    TEST_ASSERT_NULL(progress.crystalAt(0));

    TEST_ASSERT_EQUAL_INT(static_cast<int>(CrystalRecordResult::Added),
                          static_cast<int>(progress.record(role.crystals[0])));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CrystalRecordResult::Duplicate),
                          static_cast<int>(progress.record(role.crystals[0])));
    TEST_ASSERT_EQUAL_INT(1, static_cast<int>(progress.count()));
    TEST_ASSERT_EQUAL_INT(role.crystals[0].audioTrack, progress.crystalAt(0)->audioTrack);
    TEST_ASSERT_TRUE(progress.hasPendingAudio());

    TEST_ASSERT_EQUAL_INT(static_cast<int>(CrystalRecordResult::Added),
                          static_cast<int>(progress.record(role.crystals[1])));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CrystalRecordResult::Added),
                          static_cast<int>(progress.record(role.crystals[2])));
    TEST_ASSERT_TRUE(progress.isComplete());
    TEST_ASSERT_EQUAL_INT(3, static_cast<int>(progress.count()));
    TEST_ASSERT_EQUAL_INT(role.crystals[2].position.row, progress.crystalAt(2)->position.row);

    const larp::domain::CrystalDefinition extra = {
        "Extra", larp::domain::CrystalCategory::Item, {8, larp::domain::GridColumn::I}, 199};
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CrystalRecordResult::Full),
                          static_cast<int>(progress.record(extra)));
    TEST_ASSERT_EQUAL_INT(3, static_cast<int>(progress.count()));
}

void test_puzzle_progress_tracks_audio_and_resets_round_state()
{
    PuzzleProgress progress;
    const auto &role = defaultRoleDefinitions()[0];
    progress.record(role.crystals[0]);
    progress.record(role.crystals[1]);
    progress.record(role.crystals[2]);

    TEST_ASSERT_TRUE(progress.hasPendingAudio());
    TEST_ASSERT_TRUE(progress.markAudioPlayed(1));
    TEST_ASSERT_TRUE(progress.hasPendingAudio());
    TEST_ASSERT_FALSE(progress.markAudioPlayed(3));
    TEST_ASSERT_NOT_NULL(progress.crystalAt(1));
    TEST_ASSERT_NULL(progress.crystalAt(3));

    TEST_ASSERT_TRUE(progress.markAudioPlayed(0));
    TEST_ASSERT_TRUE(progress.markAudioPlayed(2));
    TEST_ASSERT_FALSE(progress.hasPendingAudio());

    progress.reset();
    TEST_ASSERT_EQUAL_INT(0, static_cast<int>(progress.count()));
    TEST_ASSERT_FALSE(progress.isComplete());
    TEST_ASSERT_FALSE(progress.hasPendingAudio());
    TEST_ASSERT_NULL(progress.crystalAt(0));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CrystalRecordResult::Added),
                          static_cast<int>(progress.record(role.crystals[0])));
}
