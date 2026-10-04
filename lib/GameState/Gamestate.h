// Add this section if it does not exist
#ifndef GAMESTATE_H
#define GAMESTATE_H

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "GameDomain/GameDefinitions.h"
#include "GameDomain/GameProgress.h"
#include "GamePorts/IAudioPlayer.h"
#include "GamePorts/ICardReader.h"
#include "GamePorts/ICommunicator.h"
#include "GamePorts/ICrystalInput.h"
#include "GamePorts/IClock.h"
#include "GamePorts/ILightEffects.h"
#include "GamePorts/IWakeInput.h"
#include "StateMachine.h"

using namespace MyMachine;

/**
 * @class ReichstagGame
 * @brief Manages the state and transitions of the Reichstag game.
 *
 * This class handles the different states of the game, transitions between states,
 * and game-specific logic such as crystal placement and error handling.
 *
 * @note This class uses a state machine to manage game states.
 */
class ReichstagGame
{
public:
    /**
     * @brief Constructs a new ReichstagGame object.
     */
    ReichstagGame(larp::ports::ICardReader &cardReader, larp::ports::ICrystalInput &crystalInput, larp::ports::IWakeInput &wakeInput, larp::ports::IAudioPlayer &audioPlayer, larp::ports::IClock &clockPort, larp::ports::ILightEffects &lightEffects, larp::ports::ICommunicator &communicator);

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

    void stateBonusState();

    void stateStandByState();

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

    bool transitionToBonusState();

    bool transitionToStandBy();
    bool transitionFromStandBY();
    bool transitionToIdleBonus();

    /**
     * @brief Sets up the state transitions for the game.
     */
    void setupTransitions();

    /**
     * @brief Sets up the game.
     */
    void setup();

    StateMachine machine; ///< Manages the game states.

private:
    /**
     * @brief Finds the domain role matching a card UID.
     * @param cardUid Fixed-size UID read from the role card.
     * @return A pointer to the immutable role definition, or nullptr when unknown.
     */
    const larp::domain::RoleDefinition *getRoleDefinition(
        const larp::domain::CardUid &cardUid) const;

    /**
     * @brief Retrieves the current keystone.
     * @return A pointer to the current keystone.
     */
    std::array<uint8_t, 7> getKeyStone();

    /**
     * @brief Retrieves the current crystal.
     * @return The current crystal.
     */
    int getCrystal();

    /**
     * @brief Checks if a new crystal is valid.
     * @return true if the new crystal is valid, false otherwise.
     */
    enum CrystalCheckResult
    {                   ///< Enumerates the possible crystal check results.
        NO_NEW_CRYSTAL, ///< No new crystal has been placed.
        VALID_CRYSTAL,  ///< A valid crystal has been placed.
        INVALID_CRYSTAL ///< An invalid crystal has been placed.
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

    void handleCrystaldetection(std::uint32_t intervalMilliseconds);
    void playCrystalAudio(std::size_t crystalIndex);

    void handleKeystonedetection();

    larp::domain::GameProgress gameProgress;
    State *Idle;
    State *WaitingForCrystals;
    State *FirstCrystalPlaced;
    State *SecondCrystalPlaced;
    State *ThirdCrystalPlaced;
    State *GameCompleted;
    State *BonusState;
    State *ErrorState;
    State *StandByState;
    larp::ports::ICardReader &cardReader;
    larp::ports::ICrystalInput &crystalInput;
    larp::ports::IWakeInput &wakeInput;
    larp::ports::IAudioPlayer &audioPlayer;
    larp::ports::IClock &clockPort;
    larp::ports::ILightEffects &lightEffects;
    larp::ports::ICommunicator &communicator;
    const int INTERVALL = 150;
    unsigned long timeout = 600000;
    unsigned long lastMillis = 0;
    std::uint32_t lastTransitionCardReadMilliseconds = 0;
    std::uint32_t lastKeystoneReadMilliseconds = 0;
    std::uint32_t lastCrystalScanMilliseconds = 0;
    bool transitionCardReadTimerStarted = false;
    bool keystoneReadTimerStarted = false;
    bool crystalScanTimerStarted = false;

    bool intervalElapsed(std::uint32_t &lastTriggerMilliseconds, bool &timerStarted,
                        std::uint32_t intervalMilliseconds);

};

#endif // GAMESTATE_H
