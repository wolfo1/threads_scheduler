#ifndef _THREAD_H_
#define _THREAD_H_
#include "uthreads.h"
#include <iostream>

#define MEMORY_ERROR "system error: Memory allocation failed."

enum state {RUNNING, READY, BLOCKED};

class Thread
{
 protected:
  int _id;
  state _state;
  char* _stack;
  int _quantum_counter = 0;
  int _sleep_until = -1;
  bool is_blocked = false;

 public:
  Thread(int);
  ~Thread();
  int get_id() const { return this->_id; }
  state get_state() { return this->_state; }
  void set_state(state state) { this->_state = state; }
  char* get_stack_ptr() { return this->_stack; }
  void inc_quantum_counter() { this->_quantum_counter++; }
  int get_quantum_counter() const { return this->_quantum_counter; }
  int get_sleep_timer() const { return this->_sleep_until; }
  void set_sleep_timer(int quantums) { this->_sleep_until = quantums; }
  void set_blocked(bool status) { this->is_blocked = status; }
  bool get_blocked() const { return this->is_blocked; }
};
#endif //_THREAD_H_
