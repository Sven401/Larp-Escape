#include "FastLED.h"
#include <unity.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <tuple>

#include "GameDomain/GameDefinitions.h"
#include "../../lib/LedFx/CrystalPaletteMap.cpp"
#include "../../src/GameDomain/GameDefinitions.cpp"

const TProgmemRGBPalette16 Purple_p = {};
const TProgmemRGBPalette16 Red_p = {};
const TProgmemRGBPalette16 Orange_p = {};
const TProgmemRGBPalette16 Green_p = {};
const TProgmemRGBPalette16 White_p = {};
const TProgmemRGBPalette16 Blue_p = {};

using larp::domain::CardUid;
using larp::domain::CrystalCategory;
using larp::domain::GridColumn;
using larp::domain::GridPosition;
using larp::domain::RoleId;

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

void test_default_roles_have_unique_uids_names_and_audio_tracks()
{
    struct ExpectedRole
    {
        RoleId id;
        uint8_t uidCode;
        const char *name;
        int audioTrack;
    };
    const ExpectedRole expected[] = {
        {RoleId::Puppenspielerin, 0x9D, "Puppenspieler:in", 230},
        {RoleId::Krieger, 0xB1, "Krieger:in", 210},
        {RoleId::Narr, 0xA1, "Narr / Närrin", 220},
        {RoleId::Abenteurerin, 0xA5, "Abenteurer:in", 200},
        {RoleId::Ueberlebende, 0xAD, "Überlebende:r", 250},
        {RoleId::Wissende, 0xA9, "Wissende:r", 240},
    };
    const auto &roles = larp::domain::defaultRoleDefinitions();
    TEST_ASSERT_EQUAL_INT(6, static_cast<int>(roles.size()));
    for (std::size_t i = 0; i < roles.size(); ++i)
    {
        TEST_ASSERT_EQUAL_INT(static_cast<int>(expected[i].id), static_cast<int>(roles[i].id));
        TEST_ASSERT_EQUAL_STRING(expected[i].name, roles[i].name);
        TEST_ASSERT_EQUAL_INT(expected[i].audioTrack, roles[i].audioTrack);
        TEST_ASSERT_EQUAL_INT(3, static_cast<int>(roles[i].crystals.size()));
        TEST_ASSERT_EQUAL_INT(0x04, roles[i].uid[0]);
        TEST_ASSERT_EQUAL_INT(expected[i].uidCode, roles[i].uid[1]);
        TEST_ASSERT_EQUAL_INT(0xD1, roles[i].uid[2]);
        TEST_ASSERT_EQUAL_INT(0xCA, roles[i].uid[3]);
        TEST_ASSERT_EQUAL_INT(0x6E, roles[i].uid[4]);
        TEST_ASSERT_EQUAL_INT(0x67, roles[i].uid[5]);
        TEST_ASSERT_EQUAL_INT(0x80, roles[i].uid[6]);
        TEST_ASSERT_EQUAL_PTR(&roles[i], larp::domain::findRoleByUid(roles[i].uid));
    }
    TEST_ASSERT_EQUAL_INT(7, static_cast<int>(std::tuple_size<CardUid>::value));
}

void test_unknown_role_uid_is_rejected()
{
    const CardUid unknownUid = {{0x04, 0xFF, 0xD1, 0xCA, 0x6E, 0x67, 0x80}};
    const CardUid zeroUid = {{0, 0, 0, 0, 0, 0, 0}};
    TEST_ASSERT_NULL(larp::domain::findRoleByUid(unknownUid));
    TEST_ASSERT_NULL(larp::domain::findRoleByUid(zeroUid));
}

void test_crystal_definitions_and_palette_mapping_match_current_data()
{
    struct ExpectedCrystal
    {
        RoleId role;
        GridPosition position;
        const char *symbol;
        CrystalCategory category;
        const char *categoryName;
        uint16_t audioTrack;
        const TProgmemRGBPalette16 *palette;
    };
    const ExpectedCrystal expected[] = {
        {RoleId::Puppenspielerin, {1, GridColumn::A}, "Spinne", CrystalCategory::Animal, "Tier", 132, nullptr},
        {RoleId::Puppenspielerin, {1, GridColumn::H}, "Marionette", CrystalCategory::Item, "Gegenstand", 131, nullptr},
        {RoleId::Puppenspielerin, {4, GridColumn::E}, "Lila", CrystalCategory::Color, "Farbe", 130, &Purple_p},
        {RoleId::Krieger, {2, GridColumn::B}, "Schild", CrystalCategory::Item, "Gegenstand", 112, nullptr},
        {RoleId::Krieger, {6, GridColumn::B}, "Rot", CrystalCategory::Color, "Farbe", 111, &Red_p},
        {RoleId::Krieger, {3, GridColumn::F}, "Löwe", CrystalCategory::Animal, "Tier", 110, nullptr},
        {RoleId::Narr, {2, GridColumn::C}, "Käfig", CrystalCategory::Item, "Gegenstand", 120, nullptr},
        {RoleId::Narr, {9, GridColumn::B}, "Orange", CrystalCategory::Color, "Farbe", 121, &Orange_p},
        {RoleId::Narr, {9, GridColumn::H}, "Rabe", CrystalCategory::Animal, "Tier", 122, nullptr},
        {RoleId::Abenteurerin, {5, GridColumn::H}, "Seil", CrystalCategory::Item, "Gegenstand", 102, nullptr},
        {RoleId::Abenteurerin, {2, GridColumn::E}, "Grün", CrystalCategory::Color, "Farbe", 101, &Green_p},
        {RoleId::Abenteurerin, {2, GridColumn::F}, "Fuchs", CrystalCategory::Animal, "Tier", 100, nullptr},
        {RoleId::Ueberlebende, {9, GridColumn::D}, "Schwer", CrystalCategory::Item, "Gegenstand", 150, nullptr},
        {RoleId::Ueberlebende, {7, GridColumn::G}, "Weiß", CrystalCategory::Color, "Farbe", 151, &White_p},
        {RoleId::Ueberlebende, {3, GridColumn::G}, "Ratte", CrystalCategory::Animal, "Tier", 152, nullptr},
        {RoleId::Wissende, {9, GridColumn::F}, "Buch", CrystalCategory::Item, "Gegenstand", 141, nullptr},
        {RoleId::Wissende, {2, GridColumn::G}, "Blau  ", CrystalCategory::Color, "Farbe", 140, &Blue_p},
        {RoleId::Wissende, {8, GridColumn::E}, "Pfau", CrystalCategory::Animal, "Tier", 142, nullptr},
    };

    const auto &roles = larp::domain::defaultRoleDefinitions();
    TEST_ASSERT_EQUAL_INT(18, static_cast<int>(sizeof(expected) / sizeof(expected[0])));
    for (const auto &entry : expected)
    {
        const larp::domain::RoleDefinition *role = nullptr;
        for (const auto &candidate : roles)
        {
            if (candidate.id == entry.role)
            {
                role = &candidate;
                break;
            }
        }
        TEST_ASSERT_NOT_NULL(role);
        const larp::domain::CrystalDefinition *crystal =
            larp::domain::findCrystalByPosition(*role, entry.position);
        TEST_ASSERT_NOT_NULL(crystal);
        TEST_ASSERT_EQUAL_STRING(entry.symbol, crystal->symbol);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(entry.category), static_cast<int>(crystal->category));
        TEST_ASSERT_EQUAL_STRING(entry.categoryName, larp::domain::crystalCategoryName(crystal->category));
        TEST_ASSERT_EQUAL_INT(entry.audioTrack, crystal->audioTrack);
        TEST_ASSERT_EQUAL_PTR(entry.palette,
                              larp::led::paletteForCrystal(role->id, crystal->position));
    }
    const larp::domain::RoleDefinition *firstRole = larp::domain::findRoleByUid(roles[0].uid);
    TEST_ASSERT_NOT_NULL(firstRole);
    TEST_ASSERT_NULL(larp::domain::findCrystalByPosition(*firstRole, {0, GridColumn::A}));
    TEST_ASSERT_NULL(larp::domain::findCrystalByPosition(*firstRole, {10, GridColumn::I}));
    TEST_ASSERT_NULL(larp::led::paletteForCrystal(RoleId::Puppenspielerin, {1, GridColumn::A}));
}

void test_unknown_crystal_category_has_fallback_label()
{
    TEST_ASSERT_EQUAL_STRING("Unbekannt",
                             larp::domain::crystalCategoryName(static_cast<CrystalCategory>(255)));
}
