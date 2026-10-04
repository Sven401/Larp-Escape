#include "StateMachine.h"
#include <iostream>
#include "GameDomain/KeystoneTransition.h"
#include "Gamestate.h"
#include <Arduino.h>
#include <array>

ReichstagGame::ReichstagGame(larp::ports::ICardReader &cardReader, larp::ports::ICrystalInput &crystalInput, larp::ports::IWakeInput &wakeInput, larp::ports::IAudioPlayer &audioPlayer, larp::ports::IClock &clockPort, larp::ports::ILightEffects &lightEffects, larp::ports::ICommunicator &communicator)
    : cardReader(cardReader), crystalInput(crystalInput), wakeInput(wakeInput), audioPlayer(audioPlayer), clockPort(clockPort), lightEffects(lightEffects), communicator(communicator)
{
    lastTransitionCardReadMilliseconds = clockPort.nowMilliseconds();
    lastKeystoneReadMilliseconds = lastTransitionCardReadMilliseconds;
    setup();
}

const larp::domain::RoleDefinition *ReichstagGame::getRoleDefinition(
    const larp::domain::CardUid &cardUid) const
{
    const larp::domain::RoleDefinition *role = larp::domain::findRoleByUid(cardUid);
    if (role != nullptr)
    {
        Serial.println("Matching role found.");
        Serial.println(role->name);
        return role;
    }

    Serial.println("No matching role found.");
    return nullptr;
}

// --- Helper Functions ---
std::array<uint8_t, 7> ReichstagGame::getKeyStone()
{
    Serial.println("Getting current keyStone from NFC reader...");
    return cardReader.readCard(100U); // This would return the current keyStone based on the RFID reader
}

ReichstagGame::CrystalCheckResult ReichstagGame::newCrystalisValid()
{
    std::vector<larp::domain::GridPosition> crystals = crystalInput.readPressedPositions();
    if (!crystals.empty())
    {
        Serial.print(crystals.size());
        Serial.print(" crystal(s) found. ");
        if (!gameProgress.currentRoleDefinition)
        {
            Serial.println("Error: currentRoleDefinition is nullptr!");
            return NO_NEW_CRYSTAL;
        }
        for (auto &crystal : crystals)
        {
            Serial.print("Crystal: ");
            Serial.print(static_cast<int>(crystal.row));
            Serial.print(" ");
            Serial.print(static_cast<int>(crystal.column));
            Serial.println();
        }
        for (auto &crystal : crystals)
        {
            const larp::domain::CrystalDefinition *crystalDefinition =
                larp::domain::findCrystalByPosition(*gameProgress.currentRoleDefinition, crystal);
            if (crystalDefinition != nullptr)
            {
                const larp::domain::CrystalRecordResult result =
                    gameProgress.puzzleProgress.record(*crystalDefinition);
                if (result == larp::domain::CrystalRecordResult::Added)
                {
                    lightEffects.setCrystalPalette(gameProgress.currentRoleDefinition->id,
                                                   crystalDefinition->position);
                    Serial.println("Valid crystal found and added.");
                    Serial.print("Correct crystals: ");
                    Serial.println(gameProgress.puzzleProgress.count());
                    Serial.print(crystalDefinition->symbol);
                    Serial.print(larp::domain::crystalCategoryName(crystalDefinition->category));
                    return VALID_CRYSTAL;
                }
            }
            else
            {
                Serial.println("Invalid crystal detected.");
                return INVALID_CRYSTAL;
            }
        }
    }
    return NO_NEW_CRYSTAL;
}

void ReichstagGame::handleCrystaldetection(std::uint32_t intervalMilliseconds)
{
    if (!intervalElapsed(lastCrystalScanMilliseconds, crystalScanTimerStarted,
                         intervalMilliseconds))
    {
        return;
    }

    CrystalCheckResult result = newCrystalisValid();
    if (result == VALID_CRYSTAL)
    {
        gameProgress.correctCrystal = true;
        gameProgress.incorrectCrystal = false;
        Serial.println("Valid crystal placed.");
    }
    else if (result == INVALID_CRYSTAL)
    {
        gameProgress.correctCrystal = false;
        gameProgress.incorrectCrystal = true;
        Serial.println("Invalid crystal placed.");
    }
}

bool ReichstagGame::gameReset()
{
    gameProgress.resetGame();
    return true;
}

bool ReichstagGame::roundReset()
{
    gameProgress.resetRound();
    return true;
}

void ReichstagGame::playCrystalAudio(std::size_t crystalIndex)
{
    const larp::domain::RecordedCrystal *crystal = gameProgress.puzzleProgress.crystalAt(crystalIndex);
    if (crystal == nullptr)
    {
        Serial.println("No recorded crystal available for audio playback.");
        return;
    }

    Serial.print("Playing audio file: ");
    Serial.println(crystal->audioTrack);
    audioPlayer.playTrack(crystal->audioTrack);
    gameProgress.puzzleProgress.markAudioPlayed(crystalIndex);
}

// --- STATES ---
void ReichstagGame::stateIdle()
{
    if (machine.executeOnce)
    {
        lightEffects.fireBurst(120U, {0U, 255U, 0U});
        lastMillis = clockPort.nowMilliseconds();
        lightEffects.fireBurst(15U);
        Serial.println("Idle state entered.");
        lightEffects.setTwinkleDensity(1U);
        lightEffects.setMutedPalette();
        gameProgress.retryCount = 0 ;
    };
    lightEffects.stepBrightnessToward(larp::ports::BrightnessRamp::Idle, 20U,
                                      static_cast<std::uint16_t>(INTERVALL));

    /*
    EVERY_N_SECONDS(1)
    {
        auto newKeyStone = cardReader.readCard(100U);
        Serial.println("STATE IDLE: Aktualisiere nur, wenn newKeyStone nicht leer ist oder nach 3 Versuchen");
        Serial.print("Aktueller Keystone: ");
        for (uint8_t byte : gameProgress.currentKeyStone)
        {
            Serial.print(byte, HEX);
            Serial.print(" ");
        }
        Serial.println();
        Serial.print("vorheriger Keystone: ");
        for (uint8_t byte : gameProgress.lastKeyStone)
        {
            Serial.print(byte, HEX);
            Serial.print(" ");
        }
        // Aktualisiere nur, wenn newKeyStone nicht leer ist oder nach 3 Versuchen
        if (newKeyStone != std::array<uint8_t, 7>{} || gameProgress.retryCount >= 3)
        {

            Serial.println("~~~~~~~~~~~~~~~~ New current keystone from StateIdle.");
            gameProgress.lastKeyStone = gameProgress.currentKeyStone;
            gameProgress.currentKeyStone = newKeyStone;

            Serial.print("Aktueller Keystone: ");
            for (uint8_t byte : gameProgress.currentKeyStone)
            {
                Serial.print(byte, HEX);
                Serial.print(" ");
            }
            Serial.println();
            Serial.print("vorheriger Keystone: ");
            for (uint8_t byte : gameProgress.lastKeyStone)
            {
                Serial.print(byte, HEX);
                Serial.print(" ");
            }

            gameProgress.retryCount = 0; // Reset retries nach erfolgreichem Lesen
        }
        else
        {
            gameProgress.retryCount++; // Retry hochzählen, falls keine gültige ID
        }
    }
    */
    }

void ReichstagGame::stateWaitingForCrystals()
{
    if (machine.executeOnce)
    {
        audioPlayer.wakeup();
        lastMillis = clockPort.nowMilliseconds();
        lightEffects.fireBurst(15U);
        Serial.println("Waiting for crystals state entered.");
        lightEffects.setTwinkleSpeed(4U);
        lightEffects.setTwinkleDensity(2U);
        gameProgress.retryCount = 0;
    }
    lightEffects.stepBrightnessToward(larp::ports::BrightnessRamp::WaitingForCrystals, 50U,
                                      static_cast<std::uint16_t>(INTERVALL));
    handleCrystaldetection(100);
    handleKeystonedetection();
}

void ReichstagGame::stateFirstCrystalPlaced()
{
    if (machine.executeOnce)
    {
        lastMillis = clockPort.nowMilliseconds();
        lightEffects.fireBurst(15U);
        Serial.println("First crystal placed state entered.");
        lightEffects.setTwinkleDensity(3U);
        playCrystalAudio(0);
        gameProgress.retryCount = 0;
    }
    lightEffects.stepBrightnessToward(larp::ports::BrightnessRamp::FirstCrystalPlaced, 100U,
                                      static_cast<std::uint16_t>(INTERVALL));
    handleCrystaldetection(100);
    handleKeystonedetection();
}

void ReichstagGame::stateSecondCrystalPlaced()
{
    if (machine.executeOnce)
    {
        lastMillis = clockPort.nowMilliseconds();
        lightEffects.fireBurst(15U);
        lightEffects.setTwinkleDensity(4U);
        playCrystalAudio(1);
        gameProgress.retryCount = 0;
    }
    lightEffects.stepBrightnessToward(larp::ports::BrightnessRamp::SecondCrystalPlaced, 150U,
                                      static_cast<std::uint16_t>(INTERVALL));
    handleCrystaldetection(100);
    handleKeystonedetection();
}

void ReichstagGame::stateThirdCrystalPlaced()
{
    if (machine.executeOnce)
    {
        lastMillis = clockPort.nowMilliseconds();
        lightEffects.fireBurst(15U);
        lightEffects.setTwinkleDensity(5U);
        playCrystalAudio(2);
    }
    lightEffects.stepBrightnessToward(larp::ports::BrightnessRamp::ThirdCrystalPlaced, 200U,
                                      static_cast<std::uint16_t>(INTERVALL));
}

void ReichstagGame::stateGameCompleted()
{
    if (machine.executeOnce)
    {
        lightEffects.fireBurst(30U);
        lastMillis = clockPort.nowMilliseconds();
        // Überprüfen, ob der aktuelle KeyStone bereits in gameProgress.seenKeyStones existiert
        if (std::find(gameProgress.seenKeyStones.begin(), gameProgress.seenKeyStones.end(), gameProgress.currentKeyStone) == gameProgress.seenKeyStones.end())
        {
            gameProgress.seenKeyStones.push_back(gameProgress.currentKeyStone); // Nur hinzufügen, wenn er nicht existiert
        }
        lightEffects.setTwinkleDensity(6U);
        audioPlayer.playTrack(gameProgress.currentRoleDefinition->audioTrack);
        gameProgress.roundCounter++;
        gameProgress.lastKeyStone = gameProgress.currentKeyStone;
    }
    lightEffects.stepBrightnessToward(larp::ports::BrightnessRamp::GameCompleted, 255U,
                                      static_cast<std::uint16_t>(INTERVALL));
}

void ReichstagGame::stateErrorState()
{
    if (machine.executeOnce)
    {
        lastMillis = clockPort.nowMilliseconds();
        lightEffects.setTwinkleDensity(3U);
        audioPlayer.playTrack(998);
        gameReset();
    }
}

void ReichstagGame::stateBonusState()
{
    if (machine.executeOnce)
    {
        lightEffects.setTwinkleDensity(7U);
        audioPlayer.playTrack(900);

        lastMillis = clockPort.nowMilliseconds();
        Serial.print("Bonus Stage reached");
        randomSeed(analogRead(0));
        for (int i = 0; i < 10; i++)
        {                                // Print multiple random confetti dots
            int tabs = random(0, 10);    // Random tab position
            int newlines = random(0, 5); // Random vertical position
            for (int j = 0; j < newlines; j++)
                Serial.println(); // Move down
            for (int j = 0; j < tabs; j++)
                Serial.print("\t"); // Move right
            Serial.print("* ");     // Print a confetti character
        }
    }
}

void ReichstagGame::stateStandByState()
{
    if (machine.executeOnce)
    {
        lightEffects.setTwinkleDensity(0U);
        gameReset();
        roundReset();
    }
    const std::array<bool, 4> wakeSignals = wakeInput.readWakeSignals();
    if (wakeSignals[0])
    {
        Serial.println("ripples0");
        lightEffects.fireRipple(0U, 60U, 22U);
    };
    if (wakeSignals[1])
    {
        Serial.println("ripples1");
        lightEffects.fireRipple(1U, 60U, 2U);
    };
    if (wakeSignals[2])
    {
        Serial.println("ripples2");
        lightEffects.fireRipple(2U, 60U, 16U);
    };
    if (wakeSignals[3])
    {
        Serial.println("ripples3");
        lightEffects.fireRipple(3U, 60U, 10U);
    };
}

bool ReichstagGame::transitionToBonusState()
{
    clockPort.delayMilliseconds(500);
    if (audioPlayer.isBusy())
        return false;
    if (gameProgress.seenKeyStones.size() == 6)
    {
        return true;
    }
    return false;
}

// --- TRANSITIONS ---
bool ReichstagGame::transitionToWaitingForCrystals()
{
    if (!intervalElapsed(lastTransitionCardReadMilliseconds, transitionCardReadTimerStarted, 1000U))
    {
        return false;
    }
    {
        Serial.println("************************* Prüfe Übergang zu WaitingForCrystals... ********************************");

        auto newKeyStone = cardReader.readCard(100U);

        Serial.print("neuer Keystone: ");
        for (uint8_t byte : newKeyStone)
        {
            Serial.print(byte, HEX);
            Serial.print(" ");
        }
        Serial.println();

        Serial.print("Aktueller Keystone: ");
        for (uint8_t byte : gameProgress.currentKeyStone)
        {
            Serial.print(byte, HEX);
            Serial.print(" ");
        }
        Serial.println();
        Serial.print("vorheriger Keystone: ");
        for (uint8_t byte : gameProgress.lastKeyStone)
        {
            Serial.print(byte, HEX);
            Serial.print(" ");
        }
        Serial.println();

        bool candidateIsKnownRole = false;
        if (newKeyStone != std::array<uint8_t, 7>{} &&
            gameProgress.lastKeyStone == std::array<uint8_t, 7>{})
        {
            gameProgress.currentRoleDefinition = getRoleDefinition(newKeyStone);
            candidateIsKnownRole = gameProgress.currentRoleDefinition != nullptr;
        }

        const larp::domain::KeystoneTransition decision =
            larp::domain::classifyKeystoneTransition(newKeyStone, gameProgress.lastKeyStone,
                                                    candidateIsKnownRole);
        if (decision == larp::domain::KeystoneTransition::EmptyRead)
        {
            Serial.println("⚠️ Neuer Keystone ist leer – möglicherweise kein Stein erkannt.");
            ++gameProgress.retryCount;
            if (larp::domain::hasEnoughEmptyKeystoneReads(gameProgress.retryCount))
            {
                Serial.print(gameProgress.retryCount);
                Serial.println(" times empty keystone seen. ⚠️ Neuer Keystone ist leer ");
                gameProgress.lastKeyStone = gameProgress.currentKeyStone;
                gameProgress.currentKeyStone = newKeyStone;
                gameProgress.retryCount = 0;
            }
            return false;
        }

        if (decision == larp::domain::KeystoneTransition::RepeatedPreviousKeystone)
        {
            Serial.println("❌ Neuer Keystone entspricht dem letzten Keystone. Kein Übergang.");
            return false;
        }

        if (decision == larp::domain::KeystoneTransition::RoleAccepted)
        {
            Serial.println("✅ Gültiges Rollenprofil gefunden. Übergang erlaubt.");
            gameProgress.lastKeyStone = gameProgress.currentKeyStone;
            gameProgress.currentKeyStone = newKeyStone;
            return true;
        }

        if (decision == larp::domain::KeystoneTransition::UnknownRole)
        {
            Serial.println("❌ Kein gültiges Rollenprofil gefunden. Kein Übergang.");
        }
        else
        {
            Serial.println("❌ Neuer Keystone erkannt, aber kein Reset-Zustand vorher. Kein Übergang.");
        }

        gameProgress.lastKeyStone = gameProgress.currentKeyStone;
        gameProgress.currentKeyStone = newKeyStone;
        return false;
    }
    return false;
}


bool ReichstagGame::transitionToCrystal()
{
    if (audioPlayer.isBusy())
        return false;

    if (gameProgress.correctCrystal || gameProgress.puzzleProgress.hasPendingAudio())
    {
        gameProgress.correctCrystal = false;
        return true;
    }

    return false;
}

bool ReichstagGame::transitionToGameCompleted()
{
    clockPort.delayMilliseconds(500);
    Serial.println("Transition check to gamecompleted.");
    Serial.print("Correct crystals: ");
    Serial.println(gameProgress.puzzleProgress.count());
    Serial.print("Round counter: ");
    Serial.println(gameProgress.roundCounter);
    Serial.print("Seen keystones: ");
    Serial.println(gameProgress.seenKeyStones.size());
    Serial.print("DfmHandler busy: ");
    Serial.println(audioPlayer.isBusy());
    if (audioPlayer.isBusy())
        return false;
    return gameProgress.puzzleProgress.isComplete();
}

bool ReichstagGame::transitionToError()
{
    return gameProgress.consumeErrorTransition(audioPlayer.isBusy());
}

bool ReichstagGame::transitionToIdle()
{
    clockPort.delayMilliseconds(500);
    if (!audioPlayer.isBusy())
    {
        roundReset();
        return true;
    }
    return false;
}

bool ReichstagGame::transitionToIdleBonus()
{
    clockPort.delayMilliseconds(500);
    if (!audioPlayer.isBusy())
    {

        if (communicator.send(larp::ports::GameMessage::Activate))
        {
            Serial.println("Message Sent");
        }
        else
        {
            Serial.println("Send Failed");
        }
        gameReset();
        return true;
    }
    return false;
}
bool ReichstagGame::transitionFromStandBY()
{
    const std::array<bool, 4> wakeSignals = wakeInput.readWakeSignals();
    if (wakeSignals[0] && wakeSignals[1] && wakeSignals[2] && wakeSignals[3])
    {
        return true;
    };
    return false;
}
bool ReichstagGame::transitionToStandBy()
{
    return (clockPort.nowMilliseconds() - lastMillis) >= timeout;
}

void ReichstagGame::handleKeystonedetection()
{
    if (!intervalElapsed(lastKeystoneReadMilliseconds, keystoneReadTimerStarted, 2000U))
    {
        return;
    }
    {
        larp::domain::CardUid presentKey = cardReader.readCard(100U);

        const bool matchesCurrentKeystone =
            presentKey != std::array<uint8_t, 7>{} && presentKey == gameProgress.currentKeyStone;
        gameProgress.keystoneMonitor.observe(matchesCurrentKeystone);
        if (gameProgress.keystoneMonitor.hasError())
        {
            Serial.println("Fehler: Schlüsselstein nicht erkannt!");
        }
    }
}

bool ReichstagGame::intervalElapsed(std::uint32_t &lastTriggerMilliseconds, bool &timerStarted,
                                   std::uint32_t intervalMilliseconds)
{
    const std::uint32_t now = clockPort.nowMilliseconds();
    if (!timerStarted)
    {
        lastTriggerMilliseconds = now;
        timerStarted = true;
        return false;
    }
    if ((now - lastTriggerMilliseconds) < intervalMilliseconds)
    {
        return false;
    }

    lastTriggerMilliseconds = now;
    return true;
}

void ReichstagGame::setup()
{
    // Definierte Zustände
    StandByState = machine.addState(std::bind(&ReichstagGame::stateStandByState, this));
    Idle = machine.addState(std::bind(&ReichstagGame::stateIdle, this));
    WaitingForCrystals = machine.addState(std::bind(&ReichstagGame::stateWaitingForCrystals, this));
    FirstCrystalPlaced = machine.addState(std::bind(&ReichstagGame::stateFirstCrystalPlaced, this));
    SecondCrystalPlaced = machine.addState(std::bind(&ReichstagGame::stateSecondCrystalPlaced, this));
    ThirdCrystalPlaced = machine.addState(std::bind(&ReichstagGame::stateThirdCrystalPlaced, this));
    GameCompleted = machine.addState(std::bind(&ReichstagGame::stateGameCompleted, this));
    BonusState = machine.addState(std::bind(&ReichstagGame::stateBonusState, this));
    ErrorState = machine.addState(std::bind(&ReichstagGame::stateErrorState, this));
    setupTransitions();
}

void ReichstagGame::setupTransitions()
{
    StandByState->addTransition(std::bind(&ReichstagGame::transitionFromStandBY, this), Idle);

    Idle->addTransition(std::bind(&ReichstagGame::transitionToWaitingForCrystals, this), WaitingForCrystals);
    Idle->addTransition(std::bind(&ReichstagGame::transitionToError, this), ErrorState);
    Idle->addTransition(std::bind(&ReichstagGame::transitionToStandBy, this), StandByState);

    WaitingForCrystals->addTransition(std::bind(&ReichstagGame::transitionToError, this), ErrorState);
    WaitingForCrystals->addTransition(std::bind(&ReichstagGame::transitionToCrystal, this), FirstCrystalPlaced);
    WaitingForCrystals->addTransition(std::bind(&ReichstagGame::transitionToStandBy, this), StandByState);

    FirstCrystalPlaced->addTransition(std::bind(&ReichstagGame::transitionToError, this), ErrorState);
    FirstCrystalPlaced->addTransition(std::bind(&ReichstagGame::transitionToCrystal, this), SecondCrystalPlaced);
    FirstCrystalPlaced->addTransition(std::bind(&ReichstagGame::transitionToStandBy, this), StandByState);

    SecondCrystalPlaced->addTransition(std::bind(&ReichstagGame::transitionToError, this), ErrorState);
    SecondCrystalPlaced->addTransition(std::bind(&ReichstagGame::transitionToCrystal, this), ThirdCrystalPlaced);
    SecondCrystalPlaced->addTransition(std::bind(&ReichstagGame::transitionToStandBy, this), StandByState);

    ThirdCrystalPlaced->addTransition(std::bind(&ReichstagGame::transitionToError, this), ErrorState);
    ThirdCrystalPlaced->addTransition(std::bind(&ReichstagGame::transitionToGameCompleted, this), GameCompleted);

    GameCompleted->addTransition(std::bind(&ReichstagGame::transitionToBonusState, this), BonusState);

    GameCompleted->addTransition(std::bind(&ReichstagGame::transitionToIdle, this), Idle);
    BonusState->addTransition(std::bind(&ReichstagGame::transitionToIdleBonus, this), Idle);

    ErrorState->addTransition(std::bind(&ReichstagGame::transitionToIdle, this), Idle);
}
