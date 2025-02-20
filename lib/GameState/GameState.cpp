#include "StateMachine.h"
#include <iostream>
#include "OptionConfig.h"
#include "OptionDefinitions.h"
#include "Gamestate.h"

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
        lastMillis = millis();
        burst.fire(15);
        Serial.println("Idle state entered.");
        twinkleFox.setTwinkleDensity(1);
        twinkleFox.targetPalette = MutedAllColors_p;
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

    EVERY_N_SECONDS(1)
    {
        currentKeyStone = nfcReader.getCard(0);
        Serial.println("Warten auf Schlüsselstein...");
    }
}

void ReichstagGame::stateWaitingForCrystals()
{
    if (machine.executeOnce)
    {
        lastMillis = millis();
        burst.fire(15);
        Serial.println("Waiting for crystals state entered.");
        twinkleFox.setTwinkleSpeed(4);
        twinkleFox.setTwinkleDensity(2);
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
    EVERY_N_SECONDS(2)
    {
        std::array<uint8_t, 7> presentKey = nfcReader.getCard(100);
        errorState = currentKeyStone == presentKey ? false : true;
    }
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
    EVERY_N_SECONDS(2)
    {
        std::array<uint8_t, 7> presentKey = nfcReader.getCard(100);
        errorState = currentKeyStone == presentKey ? false : true;
    }
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
    EVERY_N_SECONDS(2)
    {
        std::array<uint8_t, 7> presentKey = nfcReader.getCard(100);
        errorState = currentKeyStone == presentKey ? false : true;
        if(errorState){
            Serial.println("currentKeyStone != presentKey!!!!!!!!!");
        }
    }
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
        lastMillis = millis();
        seenKeyStones.push_back(currentKeyStone);
        twinkleFox.setTwinkleDensity(6);
        dfmHandler.playTrack(currentOptionConfig->optionAudioFile);
        roundCounter++;
    }
    const uint8_t targetBri = 250;
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
        dfmHandler.playTrack(997);
        gameReset();
        roundReset();
    }
}

void ReichstagGame::stateBonusState()
{
    if (machine.executeOnce)
    {
        lastMillis = millis();
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
        twinkleFox.setTwinkleDensity(7);
        dfmHandler.playTrack(999);
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
    if (dfmHandler.isBusy())
        return false;
    if (roundCounter == 6 | seenKeyStones.size() == 6)
    {
        return true;
    }
    return false;
}

// --- TRANSITIONS ---
bool ReichstagGame::transitionToWaitingForCrystals()
{
    if (currentKeyStone != std::array<uint8_t, 7>{})
    {
        // Check if currentKeyStone is already in seenKeyStones
        if (std::find(seenKeyStones.begin(), seenKeyStones.end(), currentKeyStone) != seenKeyStones.end())
        {
            return false; // If found, return false
        }
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
    BonusState->addTransition(std::bind(&ReichstagGame::transitionToIdle, this), Idle);

    ErrorState->addTransition(std::bind(&ReichstagGame::transitionToIdle, this), Idle);
}