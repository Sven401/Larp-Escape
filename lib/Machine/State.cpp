#include "State.h"
#include "StateMachine.h"
#include <utility>

namespace myMachine {

State::State() : index(-1), owner(nullptr) {}
State::State(int stateIndex, MyMachine::StateMachine *machine)
    : index(stateIndex), owner(machine) {}
State::~State() = default;

bool State::addTransition(std::function<bool()> conditionFunction, State *state) {
    if (owner == nullptr)
        return false;
    // Identity is checked by the owner before the target is dereferenced.
    return addTransition(std::move(conditionFunction), owner->stateIndex(state));
}

bool State::addTransition(std::function<bool()> conditionFunction, int stateNumber) {
    if (!conditionFunction || !hasTarget(stateNumber))
        return false;
    std::unique_ptr<Transition> transition(
        new Transition{std::move(conditionFunction), stateNumber});
    transitions.push_back(std::move(transition));
    return true;
}

int State::evalTransitions() {
    const std::size_t transitionCount = transitions.size();
    for (std::size_t position = 0; position < transitionCount; ++position) {
        // Transition objects stay stable even if a guard adds another transition.
        // Capture the selected target before invoking the guard; additions and
        // target reconfiguration take effect on the next tick.
        const std::size_t revision = owner == nullptr ? 0 : owner->transitionRevision;
        const Transition &transition = *transitions[position];
        const int target = transition.stateNumber;
        if (transition.conditionFunction && hasTarget(target)) {
            const bool matches = transition.conditionFunction();
            if (owner != nullptr && owner->transitionRevision != revision)
                return owner->currentState;
            if (matches)
                return target;
        }
    }
    return index;
}

int State::execute() {
    const std::size_t revision = owner == nullptr ? 0 : owner->transitionRevision;
    if (stateLogic)
        stateLogic();
    if (owner != nullptr && owner->transitionRevision != revision)
        return owner->currentState;
    return evalTransitions();
}

int State::setTransition(int transitionIndex, int stateNumber) {
    if (transitionIndex < 0 || static_cast<std::size_t>(transitionIndex) >= transitions.size() ||
        !hasTarget(stateNumber))
        return -1;
    transitions[transitionIndex]->stateNumber = stateNumber;
    return stateNumber;
}

bool State::hasTarget(int stateNumber) const {
    return owner != nullptr && owner->hasState(stateNumber);
}

} // namespace myMachine
