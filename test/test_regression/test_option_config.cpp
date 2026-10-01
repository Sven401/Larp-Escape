#include "FastLED.h"
#include <unity.h>

#include <array>
#include <cstdint>
#include <vector>

#include "../../lib/Config/OptionConfig.h"
#include "../../lib/Config/OptionDefinitions.h"
#include "../../lib/Config/OptionConfig.cpp"
#include "../../lib/Config/OptionDefinitions.cpp"

namespace fl
{
const TProgmemRGBPalette16 Purple_p = {};
const TProgmemRGBPalette16 Red_p = {};
const TProgmemRGBPalette16 Orange_p = {};
const TProgmemRGBPalette16 Green_p = {};
const TProgmemRGBPalette16 White_p = {};
const TProgmemRGBPalette16 Blue_p = {};
}

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
        int row;
        ColLetter column;
        int audioTrack;
        bool hasPalette;
    };
    const ExpectedCrystal expected[] = {
        {0, 1, A, 132, false}, {0, 1, H, 131, false}, {0, 4, E, 130, true},
        {1, 2, B, 112, false}, {1, 6, B, 111, true}, {1, 3, F, 110, false},
        {2, 2, C, 120, false}, {2, 9, B, 121, true}, {2, 9, H, 122, false},
        {3, 5, H, 102, false}, {3, 2, E, 101, true}, {3, 2, F, 100, false},
        {4, 9, D, 150, false}, {4, 7, G, 151, true}, {4, 3, G, 152, false},
        {5, 9, F, 141, false}, {5, 2, G, 140, true}, {5, 8, E, 142, false},
    };
    std::vector<OptionConfig> options = getDefaultOptions();

    for (const auto &crystal : expected)
    {
        TEST_ASSERT_TRUE(crystal.optionIndex < options.size());
        const ButtonPair *match = options[crystal.optionIndex].isValidCrystal(crystal.row, crystal.column);
        TEST_ASSERT_NOT_NULL(match);
        TEST_ASSERT_EQUAL_INT(crystal.audioTrack, match->audioFile);
        TEST_ASSERT_EQUAL_INT(crystal.hasPalette, match->color != nullptr);
        TEST_ASSERT_TRUE(match == options[crystal.optionIndex].isValidCrystal(crystal.row, crystal.column));
    }
}

void test_unmapped_crystal_positions_are_rejected()
{
    std::vector<OptionConfig> options = getDefaultOptions();

    for (auto &option : options)
    {
        TEST_ASSERT_NULL(option.isValidCrystal(0, A));
        TEST_ASSERT_NULL(option.isValidCrystal(10, I));
    }
}

HostSerial Serial;