#ifndef _STATEMACHINE_H
#define _STATEMACHINE_H

#include <LinkedList.h>
#include "State.h"
#include <functional>
using namespace myMachine;

namespace MyMachine {

class StateMachine
{
public:
  // Constructor and Destructor
  StateMachine();
  ~StateMachine();

  // Methods
  void init();
  void run();
  State *addState(std::function<void()> functionPointer);
  State *transitionTo(State *s);
  int transitionTo(int i);
  bool executeOnce = true; // Indicates that a transition to a different state has occurred


private:
  // Attributes
  LinkedList<State *> *stateList;
  int currentState = -1;   // Indicates the current state number
};

} // namespace MyMachine

#endif // _STATEMACHINE_H
