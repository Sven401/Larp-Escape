#pragma once

#include <cstdint>
#include <vector>

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

    int roundCounter;
    CardUid currentKeyStone;
    std::vector<CardUid> seenKeyStones;
    KeystoneMonitor keystoneMonitor;
    PuzzleProgress puzzleProgress;
    bool incorrectCrystal;
    std::uint8_t retryCount;
};
} // namespace domain
} // namespace larp
