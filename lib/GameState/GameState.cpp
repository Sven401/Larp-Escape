#include "StateMachine.h"
#include <iostream>
#include "CrystalPaletteMap.h"
#include "GameDomain/KeystoneTransition.h"
#include "Gamestate.h"
#include <esp_now.h>

ReichstagGame::ReichstagGame(larp::ports::ICardReader &cardReader, larp::ports::ICrystalInput &crystalInput, larp::ports::IWakeInput &wakeInput, larp::ports::IAudioPlayer &audioPlayer, larp::ports::IClock &clockPort, fl::MyTwinkleFox &twinkleFox, Burst &burst, Ripple ripples[4])
    : cardReader(cardReader), crystalInput(crystalInput), wakeInput(wakeInput), audioPlayer(audioPlayer), clockPort(clockPort), twinkleFox(twinkleFox), burst(burst), ripples(ripples)
{
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
                    const TProgmemRGBPalette16 *palette = larp::led::paletteForCrystal(
                        gameProgress.currentRoleDefinition->id, crystalDefinition->position);
                    if (palette != nullptr)
                    {
                        twinkleFox.targetPalette = *palette;
                    }
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

void ReichstagGame::handleCrystaldetection(int delay)
{
    EVERY_N_MILLIS(delay)
    {
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
        burst.fire(120, {0,255,0});
        lastMillis = clockPort.nowMilliseconds();
        burst.fire(15);
        Serial.println("Idle state entered.");
        twinkleFox.setTwinkleDensity(1);
        twinkleFox.targetPalette = MutedAllColors_p;
        gameProgress.retryCount = 0 ;
    };
    const uint8_t targetBri = 20;
    EVERY_N_MILLISECONDS(INTERVALL)
    {
        uint8_t bri = FastLED.getBrightness();
        if (bri != targetBri)
        {
            bri += (bri < targetBri) ? 1 : -1; // Increment if lower, decrement if higher
            FastLED.setBrightness(bri);
        }
    }

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
        burst.fire(15);
        Serial.println("Waiting for crystals state entered.");
        twinkleFox.setTwinkleSpeed(4);
        twinkleFox.setTwinkleDensity(2);
        gameProgress.retryCount = 0;
    }
    const uint8_t targetBri = 50;
    EVERY_N_MILLISECONDS(INTERVALL)
    {
        uint8_t bri = FastLED.getBrightness();
        if (bri != targetBri)
        {
            bri += (bri < targetBri) ? 1 : -1; // Increment if lower, decrement if higher
            FastLED.setBrightness(bri);
        }
    }
    handleCrystaldetection(100);
    handleKeystonedetection();
}

void ReichstagGame::stateFirstCrystalPlaced()
{
    if (machine.executeOnce)
    {
        lastMillis = clockPort.nowMilliseconds();
        burst.fire(15);
        Serial.println("First crystal placed state entered.");
        twinkleFox.setTwinkleDensity(3);
        playCrystalAudio(0);
        gameProgress.retryCount = 0;
    }
    const uint8_t targetBri = 100;
    EVERY_N_MILLISECONDS(INTERVALL)
    {
        uint8_t bri = FastLED.getBrightness();
        if (bri != targetBri)
        {
            bri += (bri < targetBri) ? 1 : -1; // Increment if lower, decrement if higher
            FastLED.setBrightness(bri);
        }
    }
    handleCrystaldetection(100);
    handleKeystonedetection();
}

void ReichstagGame::stateSecondCrystalPlaced()
{
    if (machine.executeOnce)
    {
        lastMillis = clockPort.nowMilliseconds();
        burst.fire(15);
        twinkleFox.setTwinkleDensity(4);
        playCrystalAudio(1);
        gameProgress.retryCount = 0;
    }
    const uint8_t targetBri = 150;
    EVERY_N_MILLISECONDS(INTERVALL)
    {
        uint8_t bri = FastLED.getBrightness();
        if (bri != targetBri)
        {
            bri += (bri < targetBri) ? 1 : -1; // Increment if lower, decrement if higher
            FastLED.setBrightness(bri);
        }
    }
    handleCrystaldetection(100);
    handleKeystonedetection();
}

void ReichstagGame::stateThirdCrystalPlaced()
{
    if (machine.executeOnce)
    {
        lastMillis = clockPort.nowMilliseconds();
        burst.fire(15);
        twinkleFox.setTwinkleDensity(5);
        playCrystalAudio(2);
    }
    const uint8_t targetBri = 200;
    EVERY_N_MILLISECONDS(INTERVALL)
    {
        uint8_t bri = FastLED.getBrightness();
        if (bri != targetBri)
        {
            bri += (bri < targetBri) ? 1 : -1; // Increment if lower, decrement if higher
            FastLED.setBrightness(bri);
        }
    }
}

void ReichstagGame::stateGameCompleted()
{
    if (machine.executeOnce)
    {
        burst.fire(30);
        lastMillis = clockPort.nowMilliseconds();
        // Überprüfen, ob der aktuelle KeyStone bereits in gameProgress.seenKeyStones existiert
        if (std::find(gameProgress.seenKeyStones.begin(), gameProgress.seenKeyStones.end(), gameProgress.currentKeyStone) == gameProgress.seenKeyStones.end())
        {
            gameProgress.seenKeyStones.push_back(gameProgress.currentKeyStone); // Nur hinzufügen, wenn er nicht existiert
        }
        twinkleFox.setTwinkleDensity(6);
        audioPlayer.playTrack(gameProgress.currentRoleDefinition->audioTrack);
        gameProgress.roundCounter++;
        gameProgress.lastKeyStone = gameProgress.currentKeyStone;
    }
    const uint8_t targetBri = 255;
    EVERY_N_MILLISECONDS(INTERVALL)
    {
        uint8_t bri = FastLED.getBrightness();
        if (bri != targetBri)
        {
            bri += (bri < targetBri) ? 1 : -1; // Increment if lower, decrement if higher
            FastLED.setBrightness(bri);
        }
    }
}

void ReichstagGame::stateErrorState()
{
    if (machine.executeOnce)
    {
        lastMillis = clockPort.nowMilliseconds();
        twinkleFox.setTwinkleDensity(3);
        audioPlayer.playTrack(998);
        gameReset();
    }
}

void ReichstagGame::stateBonusState()
{
    if (machine.executeOnce)
    {
        twinkleFox.setTwinkleDensity(7);
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
        twinkleFox.setTwinkleDensity(0);
        gameReset();
        roundReset();
    }
    const std::array<bool, 4> wakeSignals = wakeInput.readWakeSignals();
    if (wakeSignals[0])
    {
        Serial.println("ripples0");
        ripples[0].fire(60, 22);
    };
    if (wakeSignals[1])
    {
        Serial.println("ripples1");
        ripples[1].fire(60, 2);
    };
    if (wakeSignals[2])
    {
        Serial.println("ripples2");
        ripples[2].fire(60, 16);
    };
    if (wakeSignals[3])
    {
        Serial.println("ripples3");
        ripples[3].fire(60, 10);
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
    EVERY_N_SECONDS(1)
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

        strcpy(dataToSend.message, "ACTIVATE");

        // Send message
        esp_err_t result = esp_now_send(receiverMAC, (uint8_t *)&dataToSend, sizeof(dataToSend));

        if (result == ESP_OK)
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
    EVERY_N_SECONDS(2)
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
