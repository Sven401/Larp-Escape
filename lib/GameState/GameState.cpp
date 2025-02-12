#include "StateMachine.h"	
#include <iostream>
#include "OptionConfig.h"
#include "OptionDefinitions.h"
#include "Gamestate.h"

ReichstagGame::ReichstagGame(NFCReader& nfcReader, KeyMatrix& keymatrix, DFMinniHandler& dfmHandler, TwinkleFox& twinkleFox)
    : nfcReader(nfcReader), keymatrix(keymatrix), dfmHandler(dfmHandler), twinkleFox(twinkleFox)
{
    options = getDefaultOptions();
    setup();
}

OptionConfig* ReichstagGame::getOptionConfig(uint8_t *keyStone)
{
    for (auto &option : options)
    {
        if (option.isRFIDequal(keyStone))
        {
            currentOptionConfig = &option;
            return &option;
        }
    }
    return nullptr;
}

// --- Helper Functions ---
uint8_t* ReichstagGame::getKeyStone()
{
    return nfcReader.getCard(); // This would return the current keyStone based on the RFID reader
}

ReichstagGame::CrystalCheckResult ReichstagGame::newCrystalisValid() {
    std::vector<std::pair<int, ColLetter>> crystals = keymatrix.getLowKeys(); // Scan the keyboard for the pressed button.
    if (!crystals.empty()) {
        for (auto &crystal : crystals) {
            const ButtonPair* pair = currentOptionConfig->isValidCrystal(crystal.first, crystal.second);
            if (pair != nullptr) {
                bool pairFound = false;
                for (auto &seenPair : seenOptionButtons) {
                    if (seenPair == pair) {
                        pairFound = true;
                        break;
                    }
                }

                if (!pairFound) {
                    seenOptionButtons.push_back(pair);
                    if (pair->color != nullptr) {
                        twinkleFox.targetPalette = *pair->color;
                    }
                    correctCrystals++;
                    currentButton = pair;
                    return VALID_CRYSTAL;
                }
            } else {
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
    currentKeyStone = nullptr;
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
    currentKeyStone = nullptr;
    seenOptionButtons.clear();
    currentButton = nullptr;
    correctCrystals = 0;
    incorrectCrystal = false;
    return true;
}

// --- STATES ---
void ReichstagGame::stateIdle()
{
    if(machine.executeOnce){
        twinkleFox.setTwinkleSpeed(1);
        twinkleFox.setTwinkleDensity(1);
        twinkleFox.targetPalette = MutedAllColors_p;
    };
    EVERY_N_SECONDS(1)
    {
        currentKeyStone = nfcReader.getCard();
        Serial.println("Warten auf Schlüsselstein...");
    }
    
}

void ReichstagGame::stateWaitingForCrystals()
{
    if(machine.executeOnce){
        twinkleFox.setTwinkleSpeed(4);
        twinkleFox.setTwinkleDensity(4);
    }
    std::cout << "Schlüsselstein erkannt: " << currentKeyStone << std::endl;
    std::cout << "Warte auf ersten Lesekristall..." << std::endl;
}

void ReichstagGame::stateFirstCrystalPlaced()
{
    if(machine.executeOnce){
        twinkleFox.setTwinkleSpeed(5);
        twinkleFox.setTwinkleDensity(5);
    }
}

void ReichstagGame::stateSecondCrystalPlaced()
{
    if(machine.executeOnce){
        twinkleFox.setTwinkleSpeed(6);
        twinkleFox.setTwinkleDensity(6);
    }
}

void ReichstagGame::stateThirdCrystalPlaced()
{
    if(machine.executeOnce){
    }
    twinkleFox.setTwinkleSpeed(7);
    twinkleFox.setTwinkleDensity(7);
}

void ReichstagGame::stateGameCompleted()
{
    if(machine.executeOnce){
        twinkleFox.setTwinkleSpeed(8);
        twinkleFox.setTwinkleDensity(8);
    roundCounter++;
    }
}

void ReichstagGame::stateErrorState()
{
    if(machine.executeOnce){
        twinkleFox.setTwinkleSpeed(3);
        twinkleFox.setTwinkleDensity(3);
    }
}

// --- TRANSITIONS ---
bool ReichstagGame::transitionToWaitingForCrystals()
{
    if (currentKeyStone != nullptr)
    {
        currentOptionConfig = getOptionConfig(currentKeyStone);
        if(currentOptionConfig != nullptr){
            return true;
        }
        return false;
    }
    return false;
}

bool ReichstagGame::transitionToCrystal()
{
    if(dfmHandler.isBusy()) return false;
    return newCrystalisValid() == VALID_CRYSTAL;
}

bool ReichstagGame::transitionToGameCompleted()
{
    return correctCrystals == 3;
}

bool ReichstagGame::transitionToError()
{
    if(dfmHandler.isBusy()) return false;
    if(incorrectCrystal){
        incorrectCrystal = false;
        return true;
    }
    return false;
}

bool ReichstagGame::transitionToIdle()
{
    if(!dfmHandler.isBusy()){
        roundReset();
        return true;
    } 
    return false;
}
void ReichstagGame::setup(){
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