// GameState.h
#pragma once
#include <StateMachine.h>
#include <iostream>
#include "OptionConfig.h"
#include "OptionDefinitions.h"
#include "main.cpp"

// Global variables
int roundCounter = 0;                                    // Zählt die abgeschlossenen Spielrunden
uint8_t *currentKeyStone;                                // Speichert den aktuellen Schlüsselstein
bool errorState = false;                                 // Fehlerstatus
std::vector<OptionConfig> options = getDefaultOptions(); // Game options
uint8_t correctCrystals = 0;  // Anzahl korrekt gesetzter Kristalle (max. 3)
bool incorrectCrystal = false; // Status für falsch gesetzten Kristall (max. 1)

StateMachine gameState = StateMachine();

OptionConfig *getOptionConfig(uint8_t *keyStone)
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
uint8_t *getKeyStone()
{
    return nfcReader.getCard(); // This would return the current keyStone based on the RFID reader
}

ColLetter getColLetter(int crystal)
{
    // Logic to convert a crystal to its column letter (A, B, C...)
    return A; // For now, return a static value
}
bool isValidCrystal(int row, ColLetter col)
{
    crystals = keymatrix.getLowKeys() // Here we scan the keyboard for the pressed button.

    if (crystal == -1)
    {
        return false;
    }

    if (currentOptionConfig->isValidCrystal(crystal, getColLetter(crystal)))
    {
        correctCrystals++;
        return position == correctCrystals;
    }

    std::cout << "Falscher Kristall! Spiele Fehlermeldung." << std::endl;
    errorState = true;
    return false;
}

bool gameReset()
{
    // Logic for resetting the game, for example, clearing all states
    return true;
}

// --- STATES ---
void stateIdle()
{
    if(gameState.executeOnce){
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

void stateWaitingForCrystals()
{
    if(gameState.executeOnce){
        twinkleFox.setTwinkleSpeed(4);
        twinkleFox.setTwinkleDensity(4);
    }
    std::cout << "Schlüsselstein erkannt: " << currentKeyStone << std::endl;
    std::cout << "Warte auf ersten Lesekristall..." << std::endl;
}

void stateFirstCrystalPlaced()
{
    if(gameState.executeOnce){
        currentOptionConfig->getButtons().;
    }
}

void stateSecondCrystalPlaced()
{
    if(gameState.executeOnce){
    }
}

void stateThirdCrystalPlaced()
{
    if(gameState.executeOnce){
    }
}

void stateGameCompleted()
{
    if(gameState.executeOnce){
    roundCounter++;
    }
}

void stateErrorState()
{
    if(gameState.executeOnce){
    }
}

// --- TRANSITIONS ---
bool transitionToWaitingForCrystals()
{
    uint8_t *keyStone = getKeyStone();
    if (keyStone != nullptr && currentKeyStone == nullptr)
    {
        currentKeyStone = keyStone;
        currentOptionConfig = getOptionConfig(keyStone);
        return true;
    }
    return false;
}

bool transitionToFirstCrystal()
{
    return isValidCrystal(1);
}

bool transitionToSecondCrystal()
{
    return isValidCrystal(2);
}

bool transitionToThirdCrystal()
{
    return isValidCrystal(3);
}

bool transitionToGameCompleted()
{
    return correctCrystals == 3;
}

bool transitionToError()
{
    return errorState;
}

bool transitionToIdle()
{
    return gameReset();
}

// Definierte Zustände
State *Idle = gameState.addState(&stateIdle);
State *WaitingForCrystals = gameState.addState(&stateWaitingForCrystals);
State *FirstCrystalPlaced = gameState.addState(&stateFirstCrystalPlaced);
State *SecondCrystalPlaced = gameState.addState(&stateSecondCrystalPlaced);
State *ThirdCrystalPlaced = gameState.addState(&stateThirdCrystalPlaced);
State *GameCompleted = gameState.addState(&stateGameCompleted);
State *ErrorState = gameState.addState(&stateErrorState);

void setupTransitions()
{
    Idle->addTransition(&transitionToWaitingForCrystals, WaitingForCrystals);
    Idle->addTransition(&transitionToError, ErrorState);

    WaitingForCrystals->addTransition(&transitionToFirstCrystal, FirstCrystalPlaced);
    WaitingForCrystals->addTransition(&transitionToError, ErrorState);

    FirstCrystalPlaced->addTransition(&transitionToSecondCrystal, SecondCrystalPlaced);
    FirstCrystalPlaced->addTransition(&transitionToError, ErrorState);

    SecondCrystalPlaced->addTransition(&transitionToThirdCrystal, ThirdCrystalPlaced);
    SecondCrystalPlaced->addTransition(&transitionToError, ErrorState);

    ThirdCrystalPlaced->addTransition(&transitionToGameCompleted, GameCompleted);
    ThirdCrystalPlaced->addTransition(&transitionToError, ErrorState);

    GameCompleted->addTransition(&transitionToIdle, Idle);
    ErrorState->addTransition(&transitionToIdle, Idle);
}