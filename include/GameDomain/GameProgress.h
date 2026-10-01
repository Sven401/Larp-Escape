#pragma once

#include <cstdint>
#include <vector>

#include "GameDomain/GameDefinitions.h"
#include "GameDomain/KeystoneMonitor.h"
#include "GameDomain/PuzzleProgress.h"

namespace larp
{
namespace domain
{
struct GameProgress
{
    GameProgress();

    void resetRound();
    void resetGame();
    bool consumeErrorTransition(bool audioBusy);

    int roundCounter;
    CardUid currentKeyStone;
    CardUid lastKeyStone;
    std::vector<CardUid> seenKeyStones;
    KeystoneMonitor keystoneMonitor;
    PuzzleProgress puzzleProgress;
    bool incorrectCrystal;
    bool correctCrystal;
    const RoleDefinition *currentRoleDefinition;
    std::uint8_t retryCount;
};
} // namespace domain
} // namespace larp
