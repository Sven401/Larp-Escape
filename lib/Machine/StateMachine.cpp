#include "StateMachine.h"

namespace MyMachine {

StateMachine::StateMachine()
{
    stateList = new LinkedList<State *>();
}

StateMachine::~StateMachine()
{
    delete stateList;
}

void StateMachine::init()
{
    // Initialization code if needed
}

void StateMachine::run()
{
    // Early exit, no states are defined
    if (stateList->size() == 0)
        return;

    // Initial condition
    if (currentState == -1)
    {
        currentState = 0;
    }

    // Execute state logic and return transitioned to state number.
    int next = stateList->get(currentState)->execute();
    executeOnce = (currentState == next) ? false : true;
    currentState = next;
}

State *StateMachine::addState(std::function<void()> functionPointer)
{
    State *s = new State();
    s->stateLogic = std::move(functionPointer);
    stateList->add(s);
    s->index = stateList->size() - 1;
    return s;
}

State *StateMachine::transitionTo(State *s)
{
    this->currentState = s->index;
    this->executeOnce = true;
    return s;
}

int StateMachine::transitionTo(int i)
{
    if (i < stateList->size())
    {
        this->currentState = i;
        this->executeOnce = true;
        return i;
    }
    return currentState;
}

} // namespace MyMachine
