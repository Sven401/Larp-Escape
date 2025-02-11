#pragma once
#include <StateMachine.h>
#include "OptionConfig.h"
#include "OptionDefinitions.h"
#include <vector>

// Global variables
extern int roundCounter;
extern uint8_t currentKeyStone;
extern int correctCrystals;
extern bool errorState;
extern std::vector<OptionConfig> options;
extern StateMachine gameState;

// Function declarations
OptionConfig* getOptionConfig(uint8_t* keyStone);
OptionConfig* currentOptionConfig;
uint8_t* getKeyStone();
int getCrystal();
ColLetter getColLetter(int crystal);
bool isValidCrystal();
bool gameReset();

// State functions
void stateIdle();
void stateWaitingForCrystals();
void stateFirstCrystalPlaced();
void stateSecondCrystalPlaced();
void stateThirdCrystalPlaced();
void stateGameCompleted();
void stateErrorState();

// Transition functions
bool transitionToWaitingForCrystals();
bool transitionToFirstCrystal();
bool transitionToSecondCrystal();
bool transitionToThirdCrystal();
bool transitionToGameCompleted();
bool transitionToError();
bool transitionToIdle();

// Setup transitions
void setupTransitions();