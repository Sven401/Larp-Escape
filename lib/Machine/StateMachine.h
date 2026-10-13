#ifndef _STATEMACHINE_H
#define _STATEMACHINE_H

#include "State.h"
#include <functional>
#include <memory>
#include <vector>

namespace MyMachine {
using myMachine::State;

// Owns every state returned by addState. Returned pointers are borrowed and
// remain valid until this machine is destroyed. Use the machine on one context.
class StateMachine {
  public:
    StateMachine();
    ~StateMachine();
    StateMachine(const StateMachine &) = delete;
    StateMachine &operator=(const StateMachine &) = delete;
    StateMachine(StateMachine &&) = delete;
    StateMachine &operator=(StateMachine &&) = delete;

    void init();
    void run();
    State *addState(std::function<void()> functionPointer);
    // Invalid pointer targets return nullptr; invalid indexes leave state intact.
    State *transitionTo(State *s);
    int transitionTo(int i);
    int currentStateIndex() const;
    int stateCount() const;
    bool executeOnce = true;

  private:
    friend class myMachine::State;
    bool hasState(int index) const;
    int stateIndex(const State *state) const;

    std::vector<std::unique_ptr<State>> states;
    int currentState = -1;
    std::size_t transitionRevision = 0;
    bool running = false;
};

} // namespace MyMachine

#endif // _STATEMACHINE_H
