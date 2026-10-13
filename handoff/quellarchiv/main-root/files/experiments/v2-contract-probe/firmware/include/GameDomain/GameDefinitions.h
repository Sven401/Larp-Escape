#pragma once

#include <array>
#include <cstdint>

#include "GameDomain/GridPosition.h"

namespace larp {
namespace domain {
using CardUid = std::array<std::uint8_t, 7>;
using AudioTrack = std::uint16_t;

enum class RoleId : std::uint8_t {
    Puppenspielerin,
    Krieger,
    Narr,
    Abenteurerin,
    Ueberlebende,
    Wissende
};

enum class CrystalCategory : std::uint8_t { Animal, Item, Color };

struct CrystalDefinition {
    const char *symbol;
    CrystalCategory category;
    GridPosition position;
    AudioTrack audioTrack;
};

struct RoleDefinition {
    RoleId id;
    CardUid uid;
    const char *name;
    AudioTrack audioTrack;
    std::array<CrystalDefinition, 3> crystals;
};

const std::array<RoleDefinition, 6> &defaultRoleDefinitions();
const RoleDefinition *findRoleByUid(const CardUid &uid);
const CrystalDefinition *findCrystalByPosition(const RoleDefinition &role, GridPosition position);
const char *crystalCategoryName(CrystalCategory category);
} // namespace domain
} // namespace larp
