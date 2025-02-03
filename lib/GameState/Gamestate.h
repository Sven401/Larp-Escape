// GameState.h
#pragma once
#include <StateMachine.h>
#include <DFRobotDFPlayerMini.h>
#include "OptionConfig.h"
#include "OptionDefinitions.h"

extern DFRobotDFPlayerMini myDFPlayer;

// Zustandsvariablen
int roundCounter = 0;                                    // Zählt die abgeschlossenen Spielrunden
int currentKeyStone = -1;                                // Speichert den aktuellen Schlüsselstein
int correctCrystals = 0;                                 // Anzahl korrekt gesetzter Kristalle
bool errorState = false;                                 // Fehlerstatus
std::vector<OptionConfig> options = getDefaultOptions(); // Game options

StateMachine gameState = StateMachine();

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

// --- STATES ---
void stateIdle()
{
    Serial.println("Warten auf Schlüsselstein...");
    correctCrystals = 0;
    errorState = false;
}

void stateWaitingForCrystals()
{
    Serial.print("Schlüsselstein erkannt: ");
    Serial.println(currentKeyStone);
    Serial.println("Warte auf ersten Lesekristall...");
}

void stateFirstCrystalPlaced()
{
    Serial.println("Erster Lesekristall korrekt erkannt.");
    myDFPlayer.play(1);
}

void stateSecondCrystalPlaced()
{
    Serial.println("Zweiter Lesekristall korrekt erkannt.");
    myDFPlayer.play(2);
}

void stateThirdCrystalPlaced()
{
    Serial.println("Dritter Lesekristall korrekt erkannt. Spiele Bonus-Audio.");
    myDFPlayer.play(3);
}

void stateGameCompleted()
{
    Serial.println("Spielrunde abgeschlossen!");
    roundCounter++;
}

void stateErrorState()
{
    Serial.println("Fehler erkannt! Spiele Fehlermeldung.");
    myDFPlayer.play(99); // Fehlermeldung
    errorState = true;
}

// --- TRANSITIONS ---
bool transitionToWaitingForCrystals()
{
    int keyStone = getKeyStone();
    if (keyStone >= 0 && currentKeyStone == -1)
    {
        currentKeyStone = keyStone;
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

// --- ERROR HANDLING ---
bool isValidCrystal(int position)
{
    int crystal = getCrystal(); // Here we scan the keyboard for the pressed button.

    if (crystal == -1)
    {
        return false;
    }

    // Now check against the currentKeyStone selected.
    for (const auto &option : options)
    {
        if (option.getRFID() == currentKeyStone)
        {
            if (option.isValidCrystal(crystal, getColLetter(crystal)))
            {
                correctCrystals++;
                return position == correctCrystals;
            }
        }
    }

    Serial.println("Falscher Kristall! Spiele Fehlermeldung.");
    myDFPlayer.play(99); // Fehlermeldung
    errorState = true;
    return false;
}

// --- Helper Functions ---
int getKeyStone()
{
    // Logic to read the RFID from the actual reader
    // Simulate the process with a dummy value
    return 1; // This would return the current keyStone based on the RFID reader
}

int getCrystal()
{
    // Simulate a crystal placement
    return 1; // This would get the current crystal from the user
}

ColLetter getColLetter(int crystal)
{
    // Logic to convert a crystal to its column letter (A, B, C...)
    return A; // For now, return a static value
}

bool gameReset()
{
    // Logic for resetting the game, for example, clearing all states
    return true;
}
