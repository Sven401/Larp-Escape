#include "GameDomain/GameProgress.h"

namespace larp {
namespace domain {
GameProgress::GameProgress()
    : roundCounter(0), currentKeyStone(), lastKeyStone(), seenKeyStones(), keystoneMonitor(),
      puzzleProgress(), incorrectCrystal(false), correctCrystal(false),
      currentRoleDefinition(nullptr), retryCount(0) {}

void GameProgress::resetRound() {
    puzzleProgress.reset();
    // A pending crystal belongs to the cancelled/completed round. It must not
    // advance a later role after the old puzzle has been cleared.
    correctCrystal = false;
    incorrectCrystal = false;
    retryCount = 0;
    keystoneMonitor.resetMisses();
}

void GameProgress::resetGame() {
    roundCounter = 0;
    currentKeyStone = {};
    keystoneMonitor.reset();
    puzzleProgress.reset();
    // A pending crystal belongs to the cancelled/completed round. It must not
    // advance a later role after the old puzzle has been cleared.
    correctCrystal = false;
    incorrectCrystal = false;
    seenKeyStones.clear();
    retryCount = 0;
}

bool GameProgress::consumeErrorTransition(bool audioBusy) {
    if (audioBusy) {
        return false;
    }

    if (incorrectCrystal) {
        incorrectCrystal = false;
        return true;
    }

    if (keystoneMonitor.hasError()) {
        keystoneMonitor.clearError();
        return true;
    }

    return false;
}
} // namespace domain
} // namespace larp
