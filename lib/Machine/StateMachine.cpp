#include "StateMachine.h"
#include <utility>

namespace MyMachine {

StateMachine::StateMachine() = default;
StateMachine::~StateMachine() = default;

void StateMachine::init() {}

void StateMachine::run() {
    if (states.empty() || running)
        return;

    if (currentState == -1) {
        currentState = 0;
        executeOnce = true;
    }
    if (!hasState(currentState))
        return;

    // A state callback may ask for a transition or call run() again. A tick
    // executes one state's logic once and keeps an explicit transition intact.
    struct RunGuard {
        bool &flag;
        explicit RunGuard(bool &value) : flag(value) {
            flag = true;
        }
        ~RunGuard() {
            flag = false;
        }
    } guard(running);

    const int executedState = currentState;
    State *state = states[executedState].get();
    const std::size_t revision = transitionRevision;
    const int next = state->execute();
    if (transitionRevision != revision || !hasState(next))
        return;

    currentState = next;
    executeOnce = executedState != next;
}

State *StateMachine::addState(std::function<void()> functionPointer) {
    std::unique_ptr<State> state(new State(stateCount(), this));
    state->stateLogic = std::move(functionPointer);
    State *result = state.get();
    states.push_back(std::move(state));
    return result;
}

State *StateMachine::transitionTo(State *state) {
    const int target = stateIndex(state);
    if (target < 0)
        return nullptr;
    transitionTo(target);
    return state;
}

int StateMachine::transitionTo(int index) {
    if (hasState(index)) {
        currentState = index;
        executeOnce = true;
        ++transitionRevision;
    }
    return currentState;
}

int StateMachine::currentStateIndex() const {
    return currentState;
}

int StateMachine::stateCount() const {
    return static_cast<int>(states.size());
}

bool StateMachine::hasState(int index) const {
    return index >= 0 && index < stateCount();
}

int StateMachine::stateIndex(const State *state) const {
    // Compare borrowed pointers before dereferencing. Foreign machine states
    // must never select one of our states merely through a matching index.
    if (state == nullptr)
        return -1;
    for (int index = 0; index < stateCount(); ++index) {
        if (states[index].get() == state)
            return index;
    }
    return -1;
}

} // namespace MyMachine
