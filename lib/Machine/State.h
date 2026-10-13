#ifndef _STATE_H
#define _STATE_H

#include <functional>
#include <memory>
#include <vector>

namespace MyMachine {
class StateMachine;
}

namespace myMachine {

struct Transition {
    std::function<bool()> conditionFunction;
    int stateNumber;
};

// The machine owns its states; each state owns its transition callbacks.
// A default-constructed standalone state has no valid machine targets.
class State {
  public:
    State();
    ~State();
    State(const State &) = delete;
    State &operator=(const State &) = delete;
    State(State &&) = delete;
    State &operator=(State &&) = delete;

    bool addTransition(std::function<bool()> conditionFunction, State *state);
    bool addTransition(std::function<bool()> conditionFunction, int stateNumber);
    int evalTransitions();
    int execute();
    // Returns -1 for an invalid transition position or target without changing it.
    int setTransition(int transitionIndex, int stateNumber);
    const int index;
    std::function<void()> stateLogic;

  private:
    friend class MyMachine::StateMachine;
    State(int stateIndex, MyMachine::StateMachine *machine);
    bool hasTarget(int stateNumber) const;

    MyMachine::StateMachine *const owner;
    std::vector<std::unique_ptr<Transition>> transitions;
};

} // namespace myMachine

#endif // _STATE_H
