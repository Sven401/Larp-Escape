#include "StateMachine.h"
#include <iostream>
#include "OptionConfig.h"
#include "OptionDefinitions.h"
#include "Gamestate.h"
//#include <esp_now.h>

ReichstagGame::ReichstagGame(NFCReader &nfcReader, KeyMatrix &keymatrix, DFMinniHandler &dfmHandler, MyTwinkleFox &twinkleFox, Burst &burst, Ripple ripples[4])
    : nfcReader(nfcReader), keymatrix(keymatrix), dfmHandler(dfmHandler), twinkleFox(twinkleFox), burst(burst), ripples(ripples)
{
    options = getDefaultOptions();
    setup();
}

OptionConfig *ReichstagGame::getOptionConfig(std::array<uint8_t, 7> &keyStone)
{
    Serial.println("Getting option config for keyStone...");

    for (auto &option : options)
    {
        if (option.isRFIDequal(keyStone))
        { // Muss sicherstellen, dass isRFIDequal std::array akzeptiert
            Serial.println("Matching option found.");
            currentOptionConfig = &option;
            Serial.println(currentOptionConfig->getHegemon().c_str());
            return &option;
        }
    }

    Serial.println("No matching option found.");
    return nullptr;
}

// --- Helper Functions ---
std::array<uint8_t, 7> ReichstagGame::getKeyStone()
{
    Serial.println("Getting current keyStone from NFC reader...");
    return nfcReader.getCard(0U); // This would return the current keyStone based on the RFID reader
}

ReichstagGame::CrystalCheckResult ReichstagGame::newCrystalisValid()
{
    std::vector<std::pair<int, ColLetter>> crystals = keymatrix.getLowKeys(); // Scan the keyboard for the pressed button.
    if (!crystals.empty())
    {
        Serial.print(crystals.size());
        Serial.print(" crystal(s) found. ");
        if (!currentOptionConfig)
        {
            Serial.println("Error: currentOptionConfig is nullptr!");
            return NO_NEW_CRYSTAL;
        }
        for (auto &crystal : crystals)
        {
            Serial.print("Crystal: ");
            Serial.print(crystal.first);
            Serial.print(" ");
            Serial.print(crystal.second);
            Serial.println();
        }
        for (auto &crystal : crystals)
        {
            const ButtonPair *pair = currentOptionConfig->isValidCrystal(crystal.first, crystal.second);
            if (pair != nullptr)
            {
                bool pairFound = false;
                for (auto &seenPair : seenOptionButtons)
                {
                    Serial.print("Seen pair: ");
                    Serial.print(seenPair.first->symbol.c_str());
                    if (seenPair.first == pair)
                    {
                        pairFound = true;
                        break;
                    }
                }

                if (!pairFound)
                {
                    seenOptionButtons.emplace_back(pair, false);
                    if (pair && pair->color)
                    {
                        twinkleFox.targetPalette = *pair->color;
                    }
                    correctCrystals++;
                    currentButton = pair;
                    Serial.println("Valid crystal found and added.");
                    Serial.print("Correct crystals: ");
                    Serial.println(correctCrystals);
                    Serial.print(currentButton->symbol.c_str());
                    Serial.print(currentButton->category.c_str());
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
            correctCrystal = true;
            incorrectCrystal = false;
            Serial.println("Valid crystal placed.");
        }
        else if (result == INVALID_CRYSTAL)
        {
            correctCrystal = false;
            incorrectCrystal = true;
            Serial.println("Invalid crystal placed.");
        }
    }
}

bool ReichstagGame::gameReset()
{
    // Reset global variables
    roundCounter = 0;
    currentKeyStone = {};
    errorState = false;
    seenOptionButtons.clear();
    currentButton = nullptr;
    correctCrystals = 0;
    incorrectCrystal = false;
    seenKeyStones.clear();
    retrys = 0;
    return true;
}

bool ReichstagGame::roundReset()
{
    // Reset round-specific variables
    seenOptionButtons.clear();
    currentButton = nullptr;
    correctCrystals = 0;
    incorrectCrystal = false;
    retrys = 0;
    return true;
}

// --- STATES ---
void ReichstagGame::stateIdle()
{
    if (machine.executeOnce)
    {
        burst.fire(120, {0,255,0});
        lastMillis = millis();
        burst.fire(15);
        Serial.println("Idle state entered.");
        twinkleFox.setTwinkleDensity(1);
        twinkleFox.targetPalette = MutedAllColors_p;
        retrys = 0 ;
    };
    const uint8_t targetBri = 20;
    EVERY_N_MILLISECONDS(INTERVALL)
    {
        uint8_t bri = FastLED.getBrightness();
        if (bri != targetBri)
        {
            bri += (bri < targetBri) ? 1 : -1;
            FastLED.setBrightness(bri);
        }
    }

    /*
    EVERY_N_SECONDS(1)
    {
        auto newKeyStone = nfcReader.getCard(0);
        Serial.println("STATE IDLE: Aktualisiere nur, wenn newKeyStone nicht leer ist oder nach 3 Versuchen");
        Serial.print("Aktueller Keystone: ");
        for (uint8_t byte : currentKeyStone)
        {
            Serial.print(byte, HEX);
            Serial.print(" ");
        }
        Serial.println();
        Serial.print("vorheriger Keystone: ");
        for (uint8_t byte : lastKeyStone)
        {
            Serial.print(byte, HEX);
            Serial.print(" ");
        }
        // Aktualisiere nur, wenn newKeyStone nicht leer ist oder nach 3 Versuchen
        if (newKeyStone != std::array<uint8_t, 7>{} || retrys >= 3)
        {

            Serial.println("~~~~~~~~~~~~~~~~ New current keystone from StateIdle.");
            lastKeyStone = currentKeyStone;
            currentKeyStone = newKeyStone;

            Serial.print("Aktueller Keystone: ");
            for (uint8_t byte : currentKeyStone)
            {
                Serial.print(byte, HEX);
                Serial.print(" ");
            }
            Serial.println();
            Serial.print("vorheriger Keystone: ");
            for (uint8_t byte : lastKeyStone)
            {
                Serial.print(byte, HEX);
                Serial.print(" ");
            }

            retrys = 0; // Reset retries nach erfolgreichem Lesen
        }
        else
        {
            retrys++; // Retry hochzählen, falls keine gültige ID
        }
    }
    */
    }

void ReichstagGame::stateWaitingForCrystals()
{
    if (machine.executeOnce)
    {
        dfmHandler.wakeupDFPlayer();
        lastMillis = millis();
        burst.fire(15);
        Serial.println("Waiting for crystals state entered.");
        twinkleFox.setTwinkleSpeed(4);
        twinkleFox.setTwinkleDensity(2);
        retrys = 0;
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
        lastMillis = millis();
        burst.fire(15);
        Serial.println("First crystal placed state entered.");
        twinkleFox.setTwinkleDensity(3);
        Serial.print("Playing audio file: ");
        Serial.println(seenOptionButtons[0].first->audioFile);
        dfmHandler.playTrack(seenOptionButtons[0].first->audioFile);
        seenOptionButtons[0].second = true;
        retrys = 0;
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
        lastMillis = millis();
        burst.fire(15);
        twinkleFox.setTwinkleDensity(4);
        dfmHandler.playTrack(seenOptionButtons[1].first->audioFile);
        seenOptionButtons[1].second = true;
        retrys = 0;
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
        lastMillis = millis();
        burst.fire(15);
        twinkleFox.setTwinkleDensity(5);
        dfmHandler.playTrack(seenOptionButtons[2].first->audioFile);
        seenOptionButtons[2].second = true;
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
        lastMillis = millis();
        // Überprüfen, ob der aktuelle KeyStone bereits in seenKeyStones existiert
        if (std::find(seenKeyStones.begin(), seenKeyStones.end(), currentKeyStone) == seenKeyStones.end())
        {
            seenKeyStones.push_back(currentKeyStone); // Nur hinzufügen, wenn er nicht existiert
        }
        twinkleFox.setTwinkleDensity(6);
        dfmHandler.playTrack(currentOptionConfig->optionAudioFile);
        roundCounter++;
        lastKeyStone = currentKeyStone;
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
        lastMillis = millis();
        twinkleFox.setTwinkleDensity(3);
        int errorTrackCount = sizeof(ERRORTRACK) / sizeof(ERRORTRACK[0]);
        randomSeed(analogRead(0));
        int randomIndex = random(0, errorTrackCount);
        dfmHandler.playTrack(ERRORTRACK[randomIndex]);
        gameReset();
        roundReset();
        errorState = false;
    }
}

void ReichstagGame::stateBonusState()
{
    if (machine.executeOnce)
    {
        twinkleFox.setTwinkleDensity(7);
        dfmHandler.playTrack(BONUSTRACK);
        lastMillis = millis();
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
    if (keymatrix.handlers[0].readGPIO(MCP_B7))
    {
        Serial.println("ripples0");
        ripples[0].fire(60, 22);
    };
    if (keymatrix.handlers[0].readGPIO(MCP_B6))
    {
        Serial.println("ripples1");
        ripples[1].fire(60, 2);
    };
    if (keymatrix.handlers[0].readGPIO(MCP_B4))
    {
        Serial.println("ripples2");
        ripples[2].fire(60, 16);
    };
    if (keymatrix.handlers[0].readGPIO(MCP_B3))
    {
        Serial.println("ripples3");
        ripples[3].fire(60, 10);
    };
}

bool ReichstagGame::transitionToBonusState()
{
    delay(500);
    if (dfmHandler.isBusy())
        return false;
    if (seenKeyStones.size() == options.size())
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

        auto newKeyStone = nfcReader.getCard(0);

        Serial.print("neuer Keystone: ");
        for (uint8_t byte : newKeyStone)
        {
            Serial.print(byte, HEX);
            Serial.print(" ");
        }
        Serial.println();

        Serial.print("Aktueller Keystone: ");
        for (uint8_t byte : currentKeyStone)
        {
            Serial.print(byte, HEX);
            Serial.print(" ");
        }
        Serial.println();
        Serial.print("vorheriger Keystone: ");
        for (uint8_t byte : lastKeyStone)
        {
            Serial.print(byte, HEX);
            Serial.print(" ");
        }
        Serial.println();

        // Falls der neue Keystone leer ist, aktualisiere nur den Status, aber kein Übergang
        if (newKeyStone == std::array<uint8_t, 7>{})
        {
            Serial.println("⚠️ Neuer Keystone ist leer – möglicherweise kein Stein erkannt.");
            retrys ++;
            if(retrys > 4){
                Serial.print(retrys);
                Serial.println(" times empty keystone seen. ⚠️ Neuer Keystone ist leer ");
            lastKeyStone = currentKeyStone;
            currentKeyStone = newKeyStone;
            retrys = 0;
        }
            return false;
        }

        // Falls der neue Keystone mit dem letzten übereinstimmt, keine Transition
        if (newKeyStone == lastKeyStone)
        {
            Serial.println("❌ Neuer Keystone entspricht dem letzten Keystone. Kein Übergang.");
            return false;
        }

        // Falls der letzte Keystone leer war, bedeutet das, dass das Board vorher abgeräumt wurde
        if (lastKeyStone == std::array<uint8_t, 7>{})
        {
            Serial.println("🔍 Suche OptionConfig für den neuen Keystone...");
            currentOptionConfig = getOptionConfig(newKeyStone);

            if (currentOptionConfig != nullptr)
            {
                Serial.println("✅ Gültige OptionConfig gefunden. Übergang erlaubt.");
                lastKeyStone = currentKeyStone;
                currentKeyStone = newKeyStone;
                return true;
            }
            else
            {
                Serial.println("❌ Keine gültige OptionConfig gefunden. Kein Übergang.");
                lastKeyStone = currentKeyStone;
                currentKeyStone = newKeyStone;
                return false;
            }
        }

        // Falls ein anderer Stein erkannt wurde, aber das Board nicht leer war -> Kein Übergang
        Serial.println("❌ Neuer Keystone erkannt, aber kein Reset-Zustand vorher. Kein Übergang.");
        lastKeyStone = currentKeyStone;
        currentKeyStone = newKeyStone;
        return false;
    }
    return false;
}


bool ReichstagGame::transitionToCrystal()
{
    if (dfmHandler.isBusy())
        return false;

    bool anyOptionFalse = false;
    for (size_t i = 0; i < seenOptionButtons.size(); i++)
    {
        if (!seenOptionButtons[i].second)
        {
            anyOptionFalse = true;
            break; // Wir müssen nicht weiter prüfen, wenn wir schon ein `false` gefunden haben
        }
    }

    if (correctCrystal || anyOptionFalse)
    {
        correctCrystal = false;
        return true;
    }

    return false;
}

bool ReichstagGame::transitionToGameCompleted()
{
    delay(500);
    Serial.println("Transition check to gamecompleted.");
    Serial.print("Correct crystals: ");
    Serial.println(correctCrystals);
    Serial.print("Round counter: ");
    Serial.println(roundCounter);
    Serial.print("Seen keystones: ");
    Serial.println(seenKeyStones.size());
    Serial.print("DfmHandler busy: ");
    Serial.println(dfmHandler.isBusy());
    if (dfmHandler.isBusy())
        return false;
    return correctCrystals == 3;
}

bool ReichstagGame::transitionToError()
{
    if (dfmHandler.isBusy())
        return false;
    if (incorrectCrystal)
    {
        incorrectCrystal = false;
        return true;
    }
    if (errorState)
    {
        errorState = false;
        return true;
    }
    return false;
}

bool ReichstagGame::transitionToIdle()
{
    delay(500);
    if (!dfmHandler.isBusy())
    {
        roundReset();
        return true;
    }
    return false;
}

bool ReichstagGame::transitionToIdleBonus()
{
    delay(500);
    if (!dfmHandler.isBusy())
    {
        /*
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
        */
        gameReset();
        return true;
    }
    return false;
}
bool ReichstagGame::transitionFromStandBY()
{
    if (keymatrix.handlers[0].readGPIO(MCP_B7) &&
        keymatrix.handlers[0].readGPIO(MCP_B6) &&
        keymatrix.handlers[0].readGPIO(MCP_B4) &&
        keymatrix.handlers[0].readGPIO(MCP_B3))
    {
        return true;
    };
    return false;
}
bool ReichstagGame::transitionToStandBy()
{
    return (millis() - lastMillis) >= timeout;
}

void ReichstagGame::handleKeystonedetection()
{
    EVERY_N_SECONDS(2)
    {
        std::array<uint8_t, 7> presentKey = nfcReader.getCard(100);

        if (presentKey != std::array<uint8_t, 7>{})
        {
            // Falls ein gültiger Key gelesen wurde, vergleiche mit currentKeyStone
            if (currentKeyStone == presentKey)
            {
                errorState = false; // Kein Fehler
                retrys = 0;         // Reset Fehlerzähler
            }
            else
            {
                retrys++; // Fehlerzähler erhöhen
            }
        }
        else
        {
            retrys++; // Kein gültiger Key -> als Fehler zählen
        }

        // Setze errorState nur, wenn X aufeinanderfolgende Fehler auftraten
        if (retrys >= 5)
        {
            errorState = true;
            retrys = 0;
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