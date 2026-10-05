#include "GameDomain/GameDefinitions.h"

namespace larp {
namespace domain {
namespace {
const std::array<RoleDefinition, 6> kDefaultRoleDefinitions = {{
    {RoleId::Puppenspielerin,
     {{0x04, 0x9D, 0xD1, 0xCA, 0x6E, 0x67, 0x80}},
     "Puppenspieler:in",
     230,
     {{{"Spinne", CrystalCategory::Animal, {1, GridColumn::A}, 132},
       {"Marionette", CrystalCategory::Item, {1, GridColumn::H}, 131},
       {"Lila", CrystalCategory::Color, {4, GridColumn::E}, 130}}}},
    {RoleId::Krieger,
     {{0x04, 0xB1, 0xD1, 0xCA, 0x6E, 0x67, 0x80}},
     "Krieger:in",
     210,
     {{{"Schild", CrystalCategory::Item, {2, GridColumn::B}, 112},
       {"Rot", CrystalCategory::Color, {6, GridColumn::B}, 111},
       {"Löwe", CrystalCategory::Animal, {3, GridColumn::F}, 110}}}},
    {RoleId::Narr,
     {{0x04, 0xA1, 0xD1, 0xCA, 0x6E, 0x67, 0x80}},
     "Narr / Närrin",
     220,
     {{{"Käfig", CrystalCategory::Item, {2, GridColumn::C}, 120},
       {"Orange", CrystalCategory::Color, {9, GridColumn::B}, 121},
       {"Rabe", CrystalCategory::Animal, {9, GridColumn::H}, 122}}}},
    {RoleId::Abenteurerin,
     {{0x04, 0xA5, 0xD1, 0xCA, 0x6E, 0x67, 0x80}},
     "Abenteurer:in",
     200,
     {{{"Seil", CrystalCategory::Item, {5, GridColumn::H}, 102},
       {"Grün", CrystalCategory::Color, {2, GridColumn::E}, 101},
       {"Fuchs", CrystalCategory::Animal, {2, GridColumn::F}, 100}}}},
    {RoleId::Ueberlebende,
     {{0x04, 0xAD, 0xD1, 0xCA, 0x6E, 0x67, 0x80}},
     "Überlebende:r",
     250,
     {{{"Schwer", CrystalCategory::Item, {9, GridColumn::D}, 150},
       {"Weiß", CrystalCategory::Color, {7, GridColumn::G}, 151},
       {"Ratte", CrystalCategory::Animal, {3, GridColumn::G}, 152}}}},
    {RoleId::Wissende,
     {{0x04, 0xA9, 0xD1, 0xCA, 0x6E, 0x67, 0x80}},
     "Wissende:r",
     240,
     {{{"Buch", CrystalCategory::Item, {9, GridColumn::F}, 141},
       {"Blau  ", CrystalCategory::Color, {2, GridColumn::G}, 140},
       {"Pfau", CrystalCategory::Animal, {8, GridColumn::E}, 142}}}},
}};
} // namespace

const std::array<RoleDefinition, 6> &defaultRoleDefinitions() {
    return kDefaultRoleDefinitions;
}

const RoleDefinition *findRoleByUid(const CardUid &uid) {
    for (const RoleDefinition &role : defaultRoleDefinitions()) {
        if (role.uid == uid) {
            return &role;
        }
    }
    return nullptr;
}

const CrystalDefinition *findCrystalByPosition(const RoleDefinition &role, GridPosition position) {
    for (const CrystalDefinition &crystal : role.crystals) {
        if (crystal.position == position) {
            return &crystal;
        }
    }
    return nullptr;
}

const char *crystalCategoryName(CrystalCategory category) {
    switch (category) {
    case CrystalCategory::Animal:
        return "Tier";
    case CrystalCategory::Item:
        return "Gegenstand";
    case CrystalCategory::Color:
        return "Farbe";
    }
    return "Unbekannt";
}
} // namespace domain
} // namespace larp
