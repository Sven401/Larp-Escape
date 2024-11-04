// GameState.cpp
#include "GameState.h"

GameState::GameState() {}

void GameState::addTransition(InputAction *action)
{
    transitions.push_back(action);
}

void GameState::checkTransitions()
{
    for (auto *action : transitions)
    {
        action->execute();
    }
}
