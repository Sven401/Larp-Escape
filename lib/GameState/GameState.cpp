#include "StateMachine.h"
#include <iostream>
#include "OptionConfig.h"
#include "OptionDefinitions.h"
#include "Gamestate.h"

ReichstagGame::ReichstagGame(NFCReader &nfcReader, KeyMatrix &keymatrix, DFMinniHandler &dfmHandler, MyTwinkleFox &twinkleFox)
    : nfcReader(nfcReader), keymatrix(keymatrix), dfmHandler(dfmHandler), twinkleFox(twinkleFox)
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
    return nfcReader.getCard(); // This would return the current keyStone based on the RFID reader
}

ReichstagGame::CrystalCheckResult ReichstagGame::newCrystalisValid()
{
    std::vector<std::pair<int, ColLetter>> crystals = keymatrix.getLowKeys(); // Scan the keyboard for the pressed button.
    keymatrix.printMatrixState();
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
                    Serial.print(seenPair->symbol.c_str());
                    if (seenPair == pair)
                    {
                        pairFound = true;
                        break;
                    }
                }

                if (!pairFound)
                {
                    seenOptionButtons.push_back(pair);
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
    return true;
}

bool ReichstagGame::roundReset()
{
    // Reset round-specific variables
    currentKeyStone = {};
    seenOptionButtons.clear();
    currentButton = nullptr;
    correctCrystals = 0;
    incorrectCrystal = false;
    return true;
}

// --- STATES ---
void ReichstagGame::stateIdle()
{
    if (machine.executeOnce)
    {
        Serial.println("Idle state entered.");
        twinkleFox.setTwinkleSpeed(1);
        twinkleFox.setTwinkleDensity(1);
        twinkleFox.targetPalette = MutedAllColors_p;
    };
    EVERY_N_MILLISECONDS(100)
    {
        uint8_t bri = FastLED.getBrightness();
        if (bri != 100)
        {
            FastLED.setBrightness(bri - 1);
        }
    }

    EVERY_N_SECONDS(1)
    {
        currentKeyStone = nfcReader.getCard();
        Serial.println("Warten auf Schlüsselstein...");
    }
}

void ReichstagGame::stateWaitingForCrystals()
{
    if (machine.executeOnce)
    {
        twinkleFox.setTwinkleSpeed(4);
        twinkleFox.setTwinkleDensity(4);
    }
    EVERY_N_MILLISECONDS(100)
    {
        uint8_t bri = FastLED.getBrightness();
        if (bri != 200)
        {
            FastLED.setBrightness(bri + 1);
        }
    }
    EVERY_N_SECONDS(1)
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

void ReichstagGame::stateFirstCrystalPlaced()
{
    if (machine.executeOnce)
    {
        twinkleFox.setTwinkleSpeed(5);
        twinkleFox.setTwinkleDensity(5);
        Serial.print("Playing audio file: ");
        Serial.println(currentButton->audioFile);
        dfmHandler.playTrack(currentButton->audioFile);
    }
}

void ReichstagGame::stateSecondCrystalPlaced()
{
    if (machine.executeOnce)
    {
        twinkleFox.setTwinkleSpeed(6);
        twinkleFox.setTwinkleDensity(6);
        dfmHandler.playTrack(currentButton->audioFile);
    }
}

void ReichstagGame::stateThirdCrystalPlaced()
{
    if (machine.executeOnce)
    {
        twinkleFox.setTwinkleSpeed(7);
        twinkleFox.setTwinkleDensity(7);
        dfmHandler.playTrack(currentButton->audioFile);
    }
}

void ReichstagGame::stateGameCompleted()
{
    if (machine.executeOnce)
    {
        twinkleFox.setTwinkleSpeed(8);
        twinkleFox.setTwinkleDensity(8);
        dfmHandler.playTrack(currentOptionConfig->optionAudioFile);
        roundCounter++;
    }
}

void ReichstagGame::stateErrorState()
{
    if (machine.executeOnce)
    {
        twinkleFox.setTwinkleSpeed(3);
        twinkleFox.setTwinkleDensity(3);
        dfmHandler.playTrack(999);
    }
}

// --- TRANSITIONS ---
bool ReichstagGame::transitionToWaitingForCrystals()
{
    if (currentKeyStone != std::array<uint8_t, 7>{})
    {
        currentOptionConfig = getOptionConfig(currentKeyStone);
        if (currentOptionConfig != nullptr)
        {
            return true;
        }
        return false;
    }
    return false;
}

bool ReichstagGame::transitionToCrystal()
{
    if (dfmHandler.isBusy())
        return false;
    if (correctCrystal)
    {
        correctCrystal = false;
        return true;
    }
    return false;
}

bool ReichstagGame::transitionToGameCompleted()
{
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
    return false;
}

bool ReichstagGame::transitionToIdle()
{
    if (!dfmHandler.isBusy())
    {
        roundReset();
        return true;
    }
    return false;
}
void ReichstagGame::setup()
{
    // Definierte Zustände
    Idle = machine.addState(std::bind(&ReichstagGame::stateIdle, this));
    WaitingForCrystals = machine.addState(std::bind(&ReichstagGame::stateWaitingForCrystals, this));
    FirstCrystalPlaced = machine.addState(std::bind(&ReichstagGame::stateFirstCrystalPlaced, this));
    SecondCrystalPlaced = machine.addState(std::bind(&ReichstagGame::stateSecondCrystalPlaced, this));
    ThirdCrystalPlaced = machine.addState(std::bind(&ReichstagGame::stateThirdCrystalPlaced, this));
    GameCompleted = machine.addState(std::bind(&ReichstagGame::stateGameCompleted, this));
    ErrorState = machine.addState(std::bind(&ReichstagGame::stateErrorState, this));
    setupTransitions();
}

void ReichstagGame::setupTransitions()
{
    Idle->addTransition(std::bind(&ReichstagGame::transitionToWaitingForCrystals, this), WaitingForCrystals);
    Idle->addTransition(std::bind(&ReichstagGame::transitionToError, this), ErrorState);

    WaitingForCrystals->addTransition(std::bind(&ReichstagGame::transitionToCrystal, this), FirstCrystalPlaced);
    WaitingForCrystals->addTransition(std::bind(&ReichstagGame::transitionToError, this), ErrorState);

    FirstCrystalPlaced->addTransition(std::bind(&ReichstagGame::transitionToCrystal, this), SecondCrystalPlaced);
    FirstCrystalPlaced->addTransition(std::bind(&ReichstagGame::transitionToError, this), ErrorState);

    SecondCrystalPlaced->addTransition(std::bind(&ReichstagGame::transitionToCrystal, this), ThirdCrystalPlaced);
    SecondCrystalPlaced->addTransition(std::bind(&ReichstagGame::transitionToError, this), ErrorState);

    ThirdCrystalPlaced->addTransition(std::bind(&ReichstagGame::transitionToGameCompleted, this), GameCompleted);
    ThirdCrystalPlaced->addTransition(std::bind(&ReichstagGame::transitionToError, this), ErrorState);

    GameCompleted->addTransition(std::bind(&ReichstagGame::transitionToIdle, this), Idle);
    ErrorState->addTransition(std::bind(&ReichstagGame::transitionToIdle, this), Idle);
}