#include "State.h"

namespace myMachine {

State::State() {
    transitions = new LinkedList<Transition*>();
}

State::~State() {
    delete transitions;
}

void State::addTransition(std::function<bool()> conditionFunction, State* s) {
    Transition* t = new Transition{conditionFunction, s->index};
    transitions->add(t);
}

void State::addTransition(std::function<bool()> conditionFunction, int stateNumber) {
    Transition* t = new Transition{conditionFunction, stateNumber};
    transitions->add(t);
}

int State::evalTransitions() {
    if (transitions->size() == 0) return index;
    bool result = false;

    for (int i = 0; i < transitions->size(); i++) {
        result = transitions->get(i)->conditionFunction();
        if (result == true) {
            return transitions->get(i)->stateNumber;
        }
    }
    return index;
}

int State::execute() {
    stateLogic();
    return evalTransitions();
}

int State::setTransition(int index, int stateNumber) {
    if (transitions->size() == 0) return -1;
    transitions->get(index)->stateNumber = stateNumber;
    return stateNumber;
}

} // namespace StateMachine
