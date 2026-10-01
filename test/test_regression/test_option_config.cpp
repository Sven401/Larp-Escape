#include "FastLED.h"
#include <unity.h>

#include <array>
#include <cstdint>
#include <vector>

#include "../../lib/Config/OptionConfig.h"
#include "../../lib/Config/OptionDefinitions.h"
#include "../../lib/Config/OptionConfig.cpp"
#include "../../lib/Config/OptionDefinitions.cpp"

using larp::domain::GridColumn;
using larp::domain::GridPosition;

void test_grid_position_equality_and_ordering()
{
    const GridPosition topLeft{1, GridColumn::A};
    const GridPosition samePosition{1, GridColumn::A};
    const GridPosition laterColumn{1, GridColumn::H};
    const GridPosition laterRow{2, GridColumn::A};

    TEST_ASSERT_TRUE(topLeft == samePosition);
    TEST_ASSERT_FALSE(topLeft != samePosition);
    TEST_ASSERT_TRUE(topLeft < laterColumn);
    TEST_ASSERT_TRUE(laterColumn < laterRow);
    TEST_ASSERT_FALSE(laterRow < topLeft);
    TEST_ASSERT_FALSE(topLeft < topLeft);
}

const TProgmemRGBPalette16 Purple_p = {};
const TProgmemRGBPalette16 Red_p = {};
const TProgmemRGBPalette16 Orange_p = {};
const TProgmemRGBPalette16 Green_p = {};
const TProgmemRGBPalette16 White_p = {};
const TProgmemRGBPalette16 Blue_p = {};

struct ExpectedRole
{
    uint8_t uidCode;
    const char *name;
    int audioTrack;
};

static std::array<uint8_t, 7> uidForCode(uint8_t code)
{
    return {{0x04, code, 0xD1, 0xCA, 0x6E, 0x67, 0x80}};
}

void test_default_role_uids_and_audio_tracks_match_current_data()
{
    const ExpectedRole expected[] = {
        {0x9D, "Puppenspieler:in", 230},
        {0xB1, "Krieger:in", 210},
        {0xA1, "Narr / Närrin", 220},
        {0xA5, "Abenteurer:in", 200},
        {0xAD, "Überlebende:r", 250},
        {0xA9, "Wissende:r", 240},
    };
    std::vector<OptionConfig> options = getDefaultOptions();

    TEST_ASSERT_EQUAL_INT(6, static_cast<int>(options.size()));
    for (const auto &expectedRole : expected)
    {
        std::array<uint8_t, 7> uid = uidForCode(expectedRole.uidCode);
        const OptionConfig *match = nullptr;
        int matchCount = 0;
        for (auto &option : options)
        {
            if (option.isRFIDequal(uid))
            {
                match = &option;
                ++matchCount;
            }
        }

        TEST_ASSERT_EQUAL_INT(1, matchCount);
        TEST_ASSERT_NOT_NULL(match);
        const std::string actualName = match->getHegemon();
        TEST_ASSERT_EQUAL_STRING(expectedRole.name, actualName.c_str());
        TEST_ASSERT_EQUAL_INT(expectedRole.audioTrack, match->getOptionAudioFile());
        TEST_ASSERT_EQUAL_INT(3, static_cast<int>(match->getButtons().size()));
    }
}

void test_unknown_and_short_role_uids_are_rejected()
{
    std::vector<OptionConfig> options = getDefaultOptions();
    std::array<uint8_t, 7> unknownUid = uidForCode(0xFF);

    for (auto &option : options)
    {
        TEST_ASSERT_FALSE(option.isRFIDequal(unknownUid));
    }

    std::vector<uint8_t> shortUid(6, 0x04);
    OptionConfig malformedOption(shortUid, "short uid", {}, 0);
    TEST_ASSERT_FALSE(malformedOption.isRFIDequal(unknownUid));
}

void test_all_default_crystal_positions_and_tracks_match_current_data()
{
    struct ExpectedCrystal
    {
        std::size_t optionIndex;
        GridPosition position;
        int audioTrack;
        bool hasPalette;
    };
    const ExpectedCrystal expected[] = {
        {0, {1, GridColumn::A}, 132, false}, {0, {1, GridColumn::H}, 131, false},
        {0, {4, GridColumn::E}, 130, true}, {1, {2, GridColumn::B}, 112, false},
        {1, {6, GridColumn::B}, 111, true}, {1, {3, GridColumn::F}, 110, false},
        {2, {2, GridColumn::C}, 120, false}, {2, {9, GridColumn::B}, 121, true},
        {2, {9, GridColumn::H}, 122, false}, {3, {5, GridColumn::H}, 102, false},
        {3, {2, GridColumn::E}, 101, true}, {3, {2, GridColumn::F}, 100, false},
        {4, {9, GridColumn::D}, 150, false}, {4, {7, GridColumn::G}, 151, true},
        {4, {3, GridColumn::G}, 152, false}, {5, {9, GridColumn::F}, 141, false},
        {5, {2, GridColumn::G}, 140, true}, {5, {8, GridColumn::E}, 142, false},
    };
    std::vector<OptionConfig> options = getDefaultOptions();

    for (const auto &crystal : expected)
    {
        TEST_ASSERT_TRUE(crystal.optionIndex < options.size());
        const ButtonPair *match = options[crystal.optionIndex].isValidCrystal(crystal.position);
        TEST_ASSERT_NOT_NULL(match);
        TEST_ASSERT_EQUAL_INT(crystal.audioTrack, match->audioFile);
        TEST_ASSERT_EQUAL_INT(crystal.hasPalette, match->color != nullptr);
        TEST_ASSERT_TRUE(match == options[crystal.optionIndex].isValidCrystal(crystal.position));
    }
}

void test_unmapped_crystal_positions_are_rejected()
{
    std::vector<OptionConfig> options = getDefaultOptions();

    for (auto &option : options)
    {
        TEST_ASSERT_NULL(option.isValidCrystal(GridPosition{0, GridColumn::A}));
        TEST_ASSERT_NULL(option.isValidCrystal(GridPosition{10, GridColumn::I}));
    }
}

HostSerial Serial;