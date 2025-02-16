#ifndef _STATE_H
#define _STATE_H

#include <LinkedList.h>
#include <functional>

// Ensure that the Transition structure is not ambiguous
namespace myMachine
{

  /*
   * Transition is a structure that holds the address of
   * a function that evaluates whether or not to transition
   * from the current state and the number of the state to transition to
   */
  struct Transition
  {
    std::function<bool()> conditionFunction;
    int stateNumber;
  };

  /*
   * State represents a state in the state machine.
   * It consists mainly of the address of the function
   * that contains the state logic and a collection of transitions
   * to other states.
   */
  class State
  {
  public:
    State();
    ~State();

    void addTransition(std::function<bool()> conditionFunction, State *s);
    void addTransition(std::function<bool()> conditionFunction, int stateNumber);
    int evalTransitions();
    int execute();
    int setTransition(int index, int stateNumber); // Can now dynamically set the transition
    int index;

    // that represents the state logic
    std::function<void()> stateLogic;
    LinkedList<Transition *> *transitions;
  }; // namespace StateMachine

};

#endif // _STATE_H
