#include "Gamestate.h"

#include <algorithm>
#include <array>

#include "GameDomain/KeystoneTransition.h"
#include "GameDomain/TransitionWait.h"

ReichstagGame::ReichstagGame(larp::ports::ICardReader &cardReader,
                             larp::ports::ICrystalInput &crystalInput,
                             larp::ports::IWakeInput &wakeInput,
                             larp::ports::IAudioPlayer &audioPlayer, larp::ports::IClock &clockPort,
                             larp::ports::ILightEffects &lightEffects,
                             larp::ports::ICommunicator &communicator,
                             larp::ports::IGameDiagnostics *diagnostics)
    : cardReader(cardReader), crystalInput(crystalInput), wakeInput(wakeInput),
      audioPlayer(audioPlayer), clockPort(clockPort), lightEffects(lightEffects),
      communicator(communicator), diagnostics(diagnostics) {
    setup();
}

larp::application::GameSnapshot ReichstagGame::snapshot() const {
    using larp::application::GamePhase;
    const int stateIndex = machine.currentStateIndex();
    const GamePhase phase = stateIndex >= 0 && stateIndex <= 8 ? static_cast<GamePhase>(stateIndex)
                                                               : GamePhase::Standby;
    return {phase,
            gameProgress.currentRoleDefinition != nullptr,
            gameProgress.currentRoleDefinition != nullptr ? gameProgress.currentRoleDefinition->id
                                                          : larp::domain::RoleId::Puppenspielerin,
            static_cast<std::uint8_t>(gameProgress.puzzleProgress.count()),
            static_cast<std::uint32_t>(gameProgress.roundCounter),
            static_cast<std::uint8_t>(gameProgress.seenKeyStones.size()),
            gameProgress.incorrectCrystal,
            gameProgress.keystoneMonitor.hasError(),
            audioPlayer.isBusy()};
}

void ReichstagGame::report(larp::ports::GameDiagnosticEvent event, std::uint32_t value,
                           std::uint32_t auxiliary) const {
    if (diagnostics != nullptr)
        diagnostics->report(event, value, auxiliary);
}

void ReichstagGame::beginState(larp::application::GamePhase phase) {
    transitionWaitStarted.fill(false);
    report(larp::ports::GameDiagnosticEvent::PhaseEntered, static_cast<std::uint32_t>(phase));
}

const larp::domain::RoleDefinition *
ReichstagGame::getRoleDefinition(const larp::domain::CardUid &cardUid) const {
    const larp::domain::RoleDefinition *role = larp::domain::findRoleByUid(cardUid);
    if (role != nullptr) {
        report(larp::ports::GameDiagnosticEvent::RoleRecognized,
               static_cast<std::uint32_t>(role->id));
    } else {
        report(larp::ports::GameDiagnosticEvent::RoleRejected);
    }
    return role;
}

ReichstagGame::CrystalCheckResult ReichstagGame::newCrystalisValid() {
    const auto crystals = crystalInput.readPressedPositions();
    if (!crystals.empty()) {
        report(larp::ports::GameDiagnosticEvent::CrystalScan,
               static_cast<std::uint32_t>(crystals.size()));
        if (!gameProgress.currentRoleDefinition) {
            report(larp::ports::GameDiagnosticEvent::CrystalWithoutRole);
            return NO_NEW_CRYSTAL;
        }
        for (const auto &crystal : crystals) {
            report(larp::ports::GameDiagnosticEvent::CrystalObserved, crystal.row,
                   static_cast<std::uint32_t>(crystal.column));
        }
        for (const auto &crystal : crystals) {
            const larp::domain::CrystalDefinition *crystalDefinition =
                larp::domain::findCrystalByPosition(*gameProgress.currentRoleDefinition, crystal);
            if (crystalDefinition != nullptr) {
                const larp::domain::CrystalRecordResult result =
                    gameProgress.puzzleProgress.record(*crystalDefinition);
                if (result == larp::domain::CrystalRecordResult::Added) {
                    lightEffects.setCrystalPalette(gameProgress.currentRoleDefinition->id,
                                                   crystalDefinition->position);
                    report(larp::ports::GameDiagnosticEvent::CrystalRecorded,
                           static_cast<std::uint32_t>(gameProgress.puzzleProgress.count()),
                           crystalDefinition->audioTrack);
                    return VALID_CRYSTAL;
                }
            } else {
                report(larp::ports::GameDiagnosticEvent::CrystalRejected, crystal.row,
                       static_cast<std::uint32_t>(crystal.column));
                return INVALID_CRYSTAL;
            }
        }
    }
    return NO_NEW_CRYSTAL;
}

void ReichstagGame::handleCrystaldetection(std::uint32_t intervalMilliseconds) {
    if (!intervalElapsed(lastCrystalScanMilliseconds, crystalScanTimerStarted,
                         intervalMilliseconds))
        return;

    const CrystalCheckResult result = newCrystalisValid();
    if (result == VALID_CRYSTAL) {
        gameProgress.correctCrystal = true;
        gameProgress.incorrectCrystal = false;
    } else if (result == INVALID_CRYSTAL) {
        gameProgress.correctCrystal = false;
        gameProgress.incorrectCrystal = true;
    }
}

bool ReichstagGame::gameReset() {
    gameProgress.resetGame();
    return true;
}

bool ReichstagGame::roundReset() {
    gameProgress.resetRound();
    return true;
}

void ReichstagGame::playCrystalAudio(std::size_t crystalIndex) {
    const larp::domain::RecordedCrystal *crystal =
        gameProgress.puzzleProgress.crystalAt(crystalIndex);
    if (crystal == nullptr) {
        report(larp::ports::GameDiagnosticEvent::CrystalAudioUnavailable,
               static_cast<std::uint32_t>(crystalIndex));
        return;
    }

    report(larp::ports::GameDiagnosticEvent::CrystalAudioStarted, crystal->audioTrack);
    audioPlayer.playTrack(crystal->audioTrack);
    gameProgress.puzzleProgress.markAudioPlayed(crystalIndex);
}

void ReichstagGame::stateIdle() {
    if (machine.executeOnce) {
        lightEffects.fireBurst(120U, {0U, 255U, 0U});
        lastMillis = clockPort.nowMilliseconds();
        lightEffects.fireBurst(15U);
        beginState(larp::application::GamePhase::Idle);
        lightEffects.setTwinkleDensity(1U);
        lightEffects.setMutedPalette();
        gameProgress.retryCount = 0;
    }
    lightEffects.stepBrightnessToward(larp::ports::BrightnessRamp::Idle, 20U,
                                      static_cast<std::uint16_t>(INTERVALL));
}

void ReichstagGame::stateWaitingForCrystals() {
    if (machine.executeOnce) {
        audioPlayer.wakeup();
        lastMillis = clockPort.nowMilliseconds();
        lightEffects.fireBurst(15U);
        beginState(larp::application::GamePhase::WaitingForCrystals);
        lightEffects.setTwinkleSpeed(4U);
        lightEffects.setTwinkleDensity(2U);
        gameProgress.retryCount = 0;
    }
    lightEffects.stepBrightnessToward(larp::ports::BrightnessRamp::WaitingForCrystals, 50U,
                                      static_cast<std::uint16_t>(INTERVALL));
    handleCrystaldetection(100U);
    handleKeystonedetection();
}

void ReichstagGame::stateFirstCrystalPlaced() {
    if (machine.executeOnce) {
        lastMillis = clockPort.nowMilliseconds();
        lightEffects.fireBurst(15U);
        beginState(larp::application::GamePhase::FirstCrystalPlaced);
        lightEffects.setTwinkleDensity(3U);
        playCrystalAudio(0U);
        gameProgress.retryCount = 0;
    }
    lightEffects.stepBrightnessToward(larp::ports::BrightnessRamp::FirstCrystalPlaced, 100U,
                                      static_cast<std::uint16_t>(INTERVALL));
    handleCrystaldetection(100U);
    handleKeystonedetection();
}

void ReichstagGame::stateSecondCrystalPlaced() {
    if (machine.executeOnce) {
        lastMillis = clockPort.nowMilliseconds();
        lightEffects.fireBurst(15U);
        beginState(larp::application::GamePhase::SecondCrystalPlaced);
        lightEffects.setTwinkleDensity(4U);
        playCrystalAudio(1U);
        gameProgress.retryCount = 0;
    }
    lightEffects.stepBrightnessToward(larp::ports::BrightnessRamp::SecondCrystalPlaced, 150U,
                                      static_cast<std::uint16_t>(INTERVALL));
    handleCrystaldetection(100U);
    handleKeystonedetection();
}

void ReichstagGame::stateThirdCrystalPlaced() {
    if (machine.executeOnce) {
        lastMillis = clockPort.nowMilliseconds();
        lightEffects.fireBurst(15U);
        beginState(larp::application::GamePhase::ThirdCrystalPlaced);
        lightEffects.setTwinkleDensity(5U);
        playCrystalAudio(2U);
    }
    lightEffects.stepBrightnessToward(larp::ports::BrightnessRamp::ThirdCrystalPlaced, 200U,
                                      static_cast<std::uint16_t>(INTERVALL));
}

void ReichstagGame::stateGameCompleted() {
    if (machine.executeOnce) {
        lightEffects.fireBurst(30U);
        lastMillis = clockPort.nowMilliseconds();
        if (std::find(gameProgress.seenKeyStones.begin(), gameProgress.seenKeyStones.end(),
                      gameProgress.currentKeyStone) == gameProgress.seenKeyStones.end())
            gameProgress.seenKeyStones.push_back(gameProgress.currentKeyStone);
        beginState(larp::application::GamePhase::GameCompleted);
        lightEffects.setTwinkleDensity(6U);
        audioPlayer.playTrack(gameProgress.currentRoleDefinition->audioTrack);
        ++gameProgress.roundCounter;
        gameProgress.lastKeyStone = gameProgress.currentKeyStone;
    }
    lightEffects.stepBrightnessToward(larp::ports::BrightnessRamp::GameCompleted, 255U,
                                      static_cast<std::uint16_t>(INTERVALL));
}

void ReichstagGame::stateErrorState() {
    if (machine.executeOnce) {
        lastMillis = clockPort.nowMilliseconds();
        beginState(larp::application::GamePhase::Error);
        lightEffects.setTwinkleDensity(3U);
        audioPlayer.playTrack(998U);
        gameReset();
    }
}

void ReichstagGame::stateBonusState() {
    if (machine.executeOnce) {
        lightEffects.setTwinkleDensity(7U);
        audioPlayer.playTrack(900U);
        lastMillis = clockPort.nowMilliseconds();
        beginState(larp::application::GamePhase::Bonus);
    }
}

void ReichstagGame::stateStandByState() {
    if (machine.executeOnce) {
        beginState(larp::application::GamePhase::Standby);
        lightEffects.setTwinkleDensity(0U);
        gameReset();
        roundReset();
    }
    const std::array<bool, 4> wakeSignals = wakeInput.readWakeSignals();
    const std::array<std::uint8_t, 4> rippleOrigins = {{22U, 2U, 16U, 10U}};
    for (std::size_t index = 0U; index < wakeSignals.size(); ++index) {
        if (wakeSignals[index])
            lightEffects.fireRipple(static_cast<std::uint8_t>(index), 60U, rippleOrigins[index]);
    }
}

bool ReichstagGame::transitionToBonusState() {
    if (!transitionWaitElapsed(0U, 500U))
        return false;
    if (audioPlayer.isBusy())
        return false;
    return gameProgress.seenKeyStones.size() == larp::domain::defaultRoleDefinitions().size();
}

bool ReichstagGame::transitionToWaitingForCrystals() {
    if (!intervalElapsed(lastTransitionCardReadMilliseconds, transitionCardReadTimerStarted, 1000U))
        return false;

    const larp::domain::CardUid newKeyStone = cardReader.readCard(100U);
    bool candidateIsKnownRole = false;
    if (newKeyStone != larp::domain::CardUid{} &&
        gameProgress.lastKeyStone == larp::domain::CardUid{}) {
        gameProgress.currentRoleDefinition = getRoleDefinition(newKeyStone);
        candidateIsKnownRole = gameProgress.currentRoleDefinition != nullptr;
    }

    const larp::domain::KeystoneTransition decision = larp::domain::classifyKeystoneTransition(
        newKeyStone, gameProgress.lastKeyStone, candidateIsKnownRole);
    if (decision == larp::domain::KeystoneTransition::EmptyRead) {
        ++gameProgress.retryCount;
        report(larp::ports::GameDiagnosticEvent::KeystoneReadEmpty, gameProgress.retryCount);
        if (larp::domain::hasEnoughEmptyKeystoneReads(gameProgress.retryCount)) {
            gameProgress.lastKeyStone = gameProgress.currentKeyStone;
            gameProgress.currentKeyStone = newKeyStone;
            gameProgress.retryCount = 0;
        }
        return false;
    }

    if (decision == larp::domain::KeystoneTransition::RepeatedPreviousKeystone) {
        report(larp::ports::GameDiagnosticEvent::KeystoneRepeated);
        return false;
    }
    if (decision == larp::domain::KeystoneTransition::RoleAccepted) {
        gameProgress.lastKeyStone = gameProgress.currentKeyStone;
        gameProgress.currentKeyStone = newKeyStone;
        return true;
    }
    if (decision == larp::domain::KeystoneTransition::BoardNotCleared)
        report(larp::ports::GameDiagnosticEvent::KeystoneBoardNotCleared);

    gameProgress.lastKeyStone = gameProgress.currentKeyStone;
    gameProgress.currentKeyStone = newKeyStone;
    return false;
}

bool ReichstagGame::transitionToCrystal() {
    if (audioPlayer.isBusy())
        return false;
    if (gameProgress.correctCrystal || gameProgress.puzzleProgress.hasPendingAudio()) {
        gameProgress.correctCrystal = false;
        return true;
    }
    return false;
}

bool ReichstagGame::transitionToGameCompleted() {
    if (!transitionWaitElapsed(1U, 500U))
        return false;
    if (audioPlayer.isBusy())
        return false;
    return gameProgress.puzzleProgress.isComplete();
}

bool ReichstagGame::transitionToError() {
    return gameProgress.consumeErrorTransition(audioPlayer.isBusy());
}

bool ReichstagGame::transitionToIdle() {
    if (!transitionWaitElapsed(2U, 500U))
        return false;
    if (!audioPlayer.isBusy()) {
        roundReset();
        return true;
    }
    return false;
}

bool ReichstagGame::transitionToIdleBonus() {
    if (!transitionWaitElapsed(3U, 500U))
        return false;
    if (!audioPlayer.isBusy()) {
        report(larp::ports::GameDiagnosticEvent::ActivationSent,
               communicator.send(larp::ports::GameMessage::Activate) ? 1U : 0U);
        gameReset();
        return true;
    }
    return false;
}

bool ReichstagGame::transitionFromStandBY() {
    const std::array<bool, 4> wakeSignals = wakeInput.readWakeSignals();
    return wakeSignals[0] && wakeSignals[1] && wakeSignals[2] && wakeSignals[3];
}

bool ReichstagGame::transitionToStandBy() {
    return static_cast<std::uint32_t>(clockPort.nowMilliseconds() - lastMillis) >= timeout;
}

void ReichstagGame::handleKeystonedetection() {
    if (!intervalElapsed(lastKeystoneReadMilliseconds, keystoneReadTimerStarted, 2000U))
        return;

    const larp::domain::CardUid presentKey = cardReader.readCard(100U);
    const bool matchesCurrentKeystone =
        presentKey != larp::domain::CardUid{} && presentKey == gameProgress.currentKeyStone;
    gameProgress.keystoneMonitor.observe(matchesCurrentKeystone);
    if (gameProgress.keystoneMonitor.hasError())
        report(larp::ports::GameDiagnosticEvent::KeystoneMissing);
}

bool ReichstagGame::intervalElapsed(std::uint32_t &lastTriggerMilliseconds, bool &timerStarted,
                                    std::uint32_t intervalMilliseconds) {
    const std::uint32_t now = clockPort.nowMilliseconds();
    if (!timerStarted) {
        lastTriggerMilliseconds = now;
        timerStarted = true;
        return false;
    }
    if (static_cast<std::uint32_t>(now - lastTriggerMilliseconds) < intervalMilliseconds)
        return false;
    lastTriggerMilliseconds = now;
    return true;
}

bool ReichstagGame::transitionWaitElapsed(std::size_t transitionIndex,
                                          std::uint32_t intervalMilliseconds) {
    if (transitionIndex >= transitionWaitStarted.size())
        return false;
    return larp::domain::TransitionWait::elapsed(
        clockPort.nowMilliseconds(), transitionWaitStarted[transitionIndex],
        transitionWaitStartedMilliseconds[transitionIndex], intervalMilliseconds);
}

void ReichstagGame::setup() {
    if (statesConfigured)
        return;
    statesConfigured = true;
    StandByState = machine.addState(std::bind(&ReichstagGame::stateStandByState, this));
    Idle = machine.addState(std::bind(&ReichstagGame::stateIdle, this));
    WaitingForCrystals = machine.addState(std::bind(&ReichstagGame::stateWaitingForCrystals, this));
    FirstCrystalPlaced = machine.addState(std::bind(&ReichstagGame::stateFirstCrystalPlaced, this));
    SecondCrystalPlaced =
        machine.addState(std::bind(&ReichstagGame::stateSecondCrystalPlaced, this));
    ThirdCrystalPlaced = machine.addState(std::bind(&ReichstagGame::stateThirdCrystalPlaced, this));
    GameCompleted = machine.addState(std::bind(&ReichstagGame::stateGameCompleted, this));
    BonusState = machine.addState(std::bind(&ReichstagGame::stateBonusState, this));
    ErrorState = machine.addState(std::bind(&ReichstagGame::stateErrorState, this));
    setupTransitions();
}

void ReichstagGame::setupTransitions() {
    if (!statesConfigured || transitionsConfigured)
        return;
    transitionsConfigured = true;
    StandByState->addTransition(std::bind(&ReichstagGame::transitionFromStandBY, this), Idle);
    Idle->addTransition(std::bind(&ReichstagGame::transitionToWaitingForCrystals, this),
                        WaitingForCrystals);
    Idle->addTransition(std::bind(&ReichstagGame::transitionToError, this), ErrorState);
    Idle->addTransition(std::bind(&ReichstagGame::transitionToStandBy, this), StandByState);
    WaitingForCrystals->addTransition(std::bind(&ReichstagGame::transitionToError, this),
                                      ErrorState);
    WaitingForCrystals->addTransition(std::bind(&ReichstagGame::transitionToCrystal, this),
                                      FirstCrystalPlaced);
    WaitingForCrystals->addTransition(std::bind(&ReichstagGame::transitionToStandBy, this),
                                      StandByState);
    FirstCrystalPlaced->addTransition(std::bind(&ReichstagGame::transitionToError, this),
                                      ErrorState);
    FirstCrystalPlaced->addTransition(std::bind(&ReichstagGame::transitionToCrystal, this),
                                      SecondCrystalPlaced);
    FirstCrystalPlaced->addTransition(std::bind(&ReichstagGame::transitionToStandBy, this),
                                      StandByState);
    SecondCrystalPlaced->addTransition(std::bind(&ReichstagGame::transitionToError, this),
                                       ErrorState);
    SecondCrystalPlaced->addTransition(std::bind(&ReichstagGame::transitionToCrystal, this),
                                       ThirdCrystalPlaced);
    SecondCrystalPlaced->addTransition(std::bind(&ReichstagGame::transitionToStandBy, this),
                                       StandByState);
    ThirdCrystalPlaced->addTransition(std::bind(&ReichstagGame::transitionToError, this),
                                      ErrorState);
    ThirdCrystalPlaced->addTransition(std::bind(&ReichstagGame::transitionToGameCompleted, this),
                                      GameCompleted);
    GameCompleted->addTransition(std::bind(&ReichstagGame::transitionToBonusState, this),
                                 BonusState);
    GameCompleted->addTransition(std::bind(&ReichstagGame::transitionToIdle, this), Idle);
    BonusState->addTransition(std::bind(&ReichstagGame::transitionToIdleBonus, this), Idle);
    ErrorState->addTransition(std::bind(&ReichstagGame::transitionToIdle, this), Idle);
}