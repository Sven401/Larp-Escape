#pragma once

#include <cstdint>

#include "GameDomain/GameDefinitions.h"

namespace larp {
namespace application {
enum class GamePhase : std::uint8_t {
    Standby,
    Idle,
    WaitingForCrystals,
    FirstCrystalPlaced,
    SecondCrystalPlaced,
    ThirdCrystalPlaced,
    GameCompleted,
    Bonus,
    Error
};

// Read-only, transport-independent status. Contains no card UID or borrowed pointer.
// rolePresent indicates the retained game profile; physical card monitoring
// is independent. roleId is meaningful only when rolePresent is true.
// The phase denotes the
// selected state, whose entry action runs on the next state-machine tick.
struct GameSnapshot {
    GamePhase phase;
    bool rolePresent;
    domain::RoleId roleId;
    std::uint8_t crystalCount;
    std::uint32_t roundCount;
    std::uint8_t seenRoleCount;
    bool incorrectCrystal;
    bool keystoneError;
    bool audioBusy;
};
} // namespace application
} // namespace larp