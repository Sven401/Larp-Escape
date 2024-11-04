// GameState.h
#pragma once
#include <vector>
#include "InputManager.h"

class GameState
{
public:
    GameState();
    void addTransition(InputAction *action);
    void checkTransitions();

private:
    std::vector<InputAction *> transitions;
};
