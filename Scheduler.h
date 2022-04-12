#ifndef _SCHEDULER_H_
#define _SCHEDULER_H_
#include "Thread.h"
#include "uthreads.h"

#include <array>
#include <list>
#include <setjmp.h>
#include <iostream>
#include <sys/time.h>
#include <signal.h>

#define MAX_THREADS_ERROR "thread library error: Maximum threads exceeded."
#define NO_THREAD_ERROR "thread library error: No such thread exists."
#define SLEEP_MAIN_THREAD_ERROR "thread library error: Can't put the main thread to sleep."
#define SIGACTION_ERROR "system error: sigaction error."
#define SETITIMER_ERROR "system error: setitimer error."
#define MEMORY_ERROR "system error: Memory allocation failed."

enum signal_status {BLOCK, UNBLOCK};

class Scheduler
{
 protected:
  std::array<Thread*, MAX_THREAD_NUM> _threads{};
  std::list<Thread*> _ready_threads{};
  std::list<Thread*> _blocked_threads{};
  std::list<Thread*> _sleeping_threads{};
  Thread* _running_thread;
  int _quantum_usecs;
  int _quantum_counter = 0;
  struct sigaction _sa{};
  struct itimerval _timer{};

 public:
    static Scheduler* scheduler;
    static Scheduler* init(int);
  // Constructor
  explicit Scheduler(int);
  ~Scheduler();
  int spawn_thread(thread_entry_point);
  int get_free_id ();
  Thread* get_running_thread() { return this->_running_thread; }
  int terminate_thread(int);
  int block_thread(int, bool);
  int resume_thread(int);
  int sleep_thread(int);
  void quantum();
  void wake_threads();
  int get_quantum_counter() const { return this->_quantum_counter; }
  int get_thread_quantum_counter(int);
  static void set_signals(signal_status);
};
#endif //_SCHEDULER_H_
