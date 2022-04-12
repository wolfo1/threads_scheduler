#include "Thread.h"

Thread::Thread (int id)
{
  _id = id;
  _state = READY;
  _stack = new char[STACK_SIZE];
  if (_stack == nullptr)
  {
      std::cerr << MEMORY_ERROR << std::endl;
      exit(1);
  }
}

Thread::~Thread ()
{
  delete _stack;
}

