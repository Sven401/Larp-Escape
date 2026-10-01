#include "GameDomain/GameProgress.h"

namespace larp
{
namespace domain
{
GameProgress::GameProgress()
    : roundCounter(0), currentKeyStone(), seenKeyStones(), keystoneMonitor(), puzzleProgress(),
      incorrectCrystal(false), retryCount(0)
{
}

void GameProgress::resetRound()
{
    puzzleProgress.reset();
    incorrectCrystal = false;
    retryCount = 0;
    keystoneMonitor.resetMisses();
}

void GameProgress::resetGame()
{
    roundCounter = 0;
    currentKeyStone = {};
    keystoneMonitor.reset();
    puzzleProgress.reset();
    incorrectCrystal = false;
    seenKeyStones.clear();
    retryCount = 0;
}
} // namespace domain
} // namespace larp
