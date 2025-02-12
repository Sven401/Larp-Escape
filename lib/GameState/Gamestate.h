// Add this section if it does not exist
#ifndef GAMESTATE_H
#define GAMESTATE_H

#pragma once

#include <vector>
#include <utility>
#include "OptionConfig.h"
#include "OptionDefinitions.h"
#include "TwinkleFox.h"
#include "NFCReader.h"
#include "KeyMatrix.h"
#include "DFMinniHandler.h"
#include "StateMachine.h"
#include "OptionConfig.h"
#include "OptionDefinitions.h"
#include <vector>


/**
 * @class ReichstagGame
 * @brief Manages the state and transitions of the Reichstag game.
 * 
 * This class handles the different states of the game, transitions between states,
 * and game-specific logic such as crystal placement and error handling.
 * 
 * @note This class uses a state machine to manage game states.
 */
class ReichstagGame {
public:
    /**
     * @brief Constructs a new ReichstagGame object.
     */
    ReichstagGame(NFCReader nfcReader, KeyMatrix keymatrix, DFMinniHandler dfmHandler, TwinkleFox twinkleFox);

    /**
     * @brief Handles the idle state of the game.
     */
    void stateIdle();

    /**
     * @brief Handles the state when the game is waiting for crystals to be placed.
     */
    void stateWaitingForCrystals();

    /**
     * @brief Handles the state when the first crystal has been placed.
     */
    void stateFirstCrystalPlaced();

    /**
     * @brief Handles the state when the second crystal has been placed.
     */
    void stateSecondCrystalPlaced();

    /**
     * @brief Handles the state when the third crystal has been placed.
     */
    void stateThirdCrystalPlaced();

    /**
     * @brief Handles the state when the game is completed.
     */
    void stateGameCompleted();

    /**
     * @brief Handles the error state of the game.
     */
    void stateErrorState();

    /**
     * @brief Transitions the game to the state waiting for crystals.
     * @return true if the transition is successful, false otherwise.
     */
    bool transitionToWaitingForCrystals();

    /**
     * @brief Transitions the game to the next crystal state.
     * @return true if the transition is successful, false otherwise.
     */
    bool transitionToCrystal();

    /**
     * @brief Transitions the game to the completed state.
     * @return true if the transition is successful, false otherwise.
     */
    bool transitionToGameCompleted();

    /**
     * @brief Transitions the game to the error state.
     * @return true if the transition is successful, false otherwise.
     */
    bool transitionToError();

    /**
     * @brief Transitions the game to the idle state.
     * @return true if the transition is successful, false otherwise.
     */
    bool transitionToIdle();

    /**
     * @brief Sets up the state transitions for the game.
     */
    void setupTransitions();

    /**
     * @brief Sets up the game.
     */
    void setup();

    StateMachine machine;                              ///< Manages the game states.

private:
    /**
     * @brief Retrieves the option configuration for a given keystone.
     * @param keyStone The keystone to retrieve the configuration for.
     * @return A pointer to the option configuration.
     */
    OptionConfig* getOptionConfig(uint8_t* keyStone);

    /**
     * @brief Retrieves the current keystone.
     * @return A pointer to the current keystone.
     */
    uint8_t* getKeyStone();

    /**
     * @brief Retrieves the current crystal.
     * @return The current crystal.
     */
    int getCrystal();


    /**
     * @brief Checks if a new crystal is valid.
     * @return true if the new crystal is valid, false otherwise.
     */
    enum CrystalCheckResult {                            ///< Enumerates the possible crystal check results.
        NO_NEW_CRYSTAL,                                  ///< No new crystal has been placed.
        VALID_CRYSTAL,                                   ///< A valid crystal has been placed.
        INVALID_CRYSTAL                                  ///< An invalid crystal has been placed.
    };

    CrystalCheckResult newCrystalisValid();

    /**
     * @brief Resets the game.
     * @return true if the game is successfully reset, false otherwise.
     */
    bool gameReset();

    /**
     * @brief Resets the game state for a new round.
     * 
     * This function is responsible for resetting all necessary game state variables
     * and conditions to their initial values in preparation for a new round of the game.
     * Does not lose track of the correct played rounds.
     * @return true if the reset was successful, false otherwise.
     */
    bool roundReset();

    int roundCounter;                                    ///< Counts the completed game rounds. eg. for how many keystones the correct crystals have been placed.
    uint8_t *currentKeyStone;                            ///< Stores the current keystone.
    bool errorState;                                     ///< Indicates if the game is in an error state.
    std::vector<OptionConfig> options;                   ///< Stores the game options.
    std::vector<const ButtonPair*> seenOptionButtons;    ///< Stores the seen option buttons.
    const ButtonPair* currentButton;                     ///< Stores the currently valid button pair. eg Crystal
    uint8_t correctCrystals;                             ///< Number of correctly placed crystals (max. 3).
    bool incorrectCrystal;                               ///< Indicates if an incorrect crystal has been placed (max. 1).
    OptionConfig* currentOptionConfig;                   ///< Stores the current option configuration.
    State *Idle;
    State *WaitingForCrystals;
    State *FirstCrystalPlaced;
    State *SecondCrystalPlaced;
    State *ThirdCrystalPlaced;
    State *GameCompleted;
    State *ErrorState;
    NFCReader& nfcReader;
    KeyMatrix& keymatrix;
    DFMinniHandler& dfmHandler;
    TwinkleFox& twinkleFox;

};

#endif //GAMESTATE_H