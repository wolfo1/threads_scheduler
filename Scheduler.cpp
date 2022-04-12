#include "Scheduler.h"
Scheduler* Scheduler::scheduler = nullptr;
#ifdef __x86_64__
/* code for 64 bit Intel arch */

typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
                 "rol    $0x11,%0\n"
                 : "=g" (ret)
                 : "0" (addr));
    return ret;
}

#else
/* code for 32 bit Intel arch */

typedef unsigned int address_t;
#define JB_SP 4
#define JB_PC 5


/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%gs:0x18,%0\n"
                 "rol    $0x9,%0\n"
                 : "=g" (ret)
                 : "0" (addr));
    return ret;
}


#endif

// context for each thread (based on thread ID index)
sigjmp_buf env[MAX_THREAD_NUM];

/**
 *  timer handler, after time is up, switches the thread to another.
 * @param sig
*/
static void switch_handler(int sig)
{
    // saves thread TCB context and runs a new thread.
    Scheduler::set_signals(BLOCK);
    int ret = sigsetjmp(env[Scheduler::scheduler->get_running_thread()->get_id()], 1);
    if (ret != 0)
    {
        Scheduler::set_signals(UNBLOCK);
        return;
    }
    Scheduler::scheduler->quantum ();
    Scheduler::set_signals(UNBLOCK);
}


Scheduler* Scheduler::init(int quantum_usecs)
{
    Scheduler::scheduler = new(std::nothrow) Scheduler(quantum_usecs);
    return Scheduler::scheduler;
}
/**
 * Constructor
 * @param quantum_usecs - quantum time for each thread.
*/
Scheduler::Scheduler (int quantum_usecs)
{
    _quantum_usecs = quantum_usecs;
    // Install timer_handler as the signal handler for SIGVTALRM.
    _sa.sa_handler = &switch_handler;
    if (sigaction(SIGVTALRM, &_sa, nullptr) < 0)
    {
        std::cerr << SIGACTION_ERROR << std::endl;
    }
    auto* main = new Thread(0);
    _threads.at(0) = main;
    _running_thread = main;
    quantum();
}

/**
 * Destructor
*/
Scheduler::~Scheduler ()
{
  for (Thread* thread : _threads)
    {
      delete thread;
    }
}
/**
 * Create a new thread and add it to READY threads
 * @param entry_point the entry point to the thread
 * @return thread ID, or -1 if fails.
*/
int Scheduler::spawn_thread (thread_entry_point entry_point)
{
    Scheduler::set_signals(BLOCK);
  // get a free id to the new thread, if none available raise error.
  int tid = get_free_id ();
  if (tid == -1)
  {
    std::cerr << MAX_THREADS_ERROR << std::endl;
    Scheduler::set_signals(UNBLOCK);
    return tid;
  }
  // create new thread and save TCB context to ENV.
  auto* new_thread = new(std::nothrow) Thread(tid);
  if (!new_thread)
  {
      std::cout << MEMORY_ERROR << std::endl;
      exit(1);
  }
  address_t sp = (address_t) new_thread->get_stack_ptr() + STACK_SIZE - sizeof(address_t);
  address_t pc = (address_t) entry_point;
  sigsetjmp(env[tid], 1);
  (env[tid]->__jmpbuf)[JB_SP] = translate_address(sp);
  (env[tid]->__jmpbuf)[JB_PC] = translate_address(pc);
  sigemptyset(&env[tid]->__saved_mask);
  // add thread to threads array and ready_threads queue.
  _threads.at(tid) = new_thread;
  _ready_threads.push_back(new_thread);
  Scheduler::set_signals(UNBLOCK);
  return tid;
}

/**
 * returns the first unoccupied ID in threads_array.
 * @return thread ID (index of _threads array).
**/
int Scheduler::get_free_id ()
{
  for(int i = 0; i < MAX_THREAD_NUM; i++)
    {
      if (_threads.at(i) == nullptr)
        {
          Scheduler::set_signals(UNBLOCK);
        return i;
        }
    }
    return -1;
}

/**
 * terminate a thread with given ID.
 * @param tid thread ID (index of _threads array).
 * @return 0 on successfully termination, -1 otherwise.
*/
int Scheduler::terminate_thread (int tid)
{
  Scheduler::set_signals(BLOCK);
  Thread* thread = _threads.at(tid);
  if (thread == nullptr)
  {
    std::cerr << NO_THREAD_ERROR << std::endl;
    Scheduler::set_signals(UNBLOCK);
    return -1;
  }
  // free the spot on _threads
  _threads.at(tid) = nullptr;
  // terminate the thread based on it's state. if it's running, find another thread to run.
  switch(thread->get_state())
  {
      case RUNNING:
          _running_thread = nullptr;
          delete thread;
          quantum();
          break;
      case READY:
          _ready_threads.remove(thread);
          delete thread;
          break;
      case BLOCKED:
          _blocked_threads.remove(thread);
          delete thread;
          break;
  }
  Scheduler::set_signals(UNBLOCK);
  return 0;
}

/**
 * block a thread with givne thread ID.
 * @param tid thread ID (index of _threads array).
 * @return 0 on successfully block, -1 otherwise.
*/
int Scheduler::block_thread (int tid, bool sleep)
{
  Scheduler::set_signals(BLOCK);
  Thread* thread = _threads.at(tid);
  if (thread == nullptr)
  {
    std::cerr << NO_THREAD_ERROR << std::endl;
    Scheduler::set_signals(UNBLOCK);
    return -1;
  }
  // if block_thread was called from sleep_thread, don't set blocked to true
  if (!sleep)
    thread->set_blocked (true);
  // add to blocked threads list.
  _blocked_threads.push_back(thread);
  // if the thread was running, run a new thread. o/w remove it from READY threads.
  if (thread->get_state() == RUNNING)
  {
      thread->set_state(BLOCKED);
      quantum();
  }
  else if (thread->get_state() == READY)
  {
      thread->set_state(BLOCKED);
      _ready_threads.remove(thread);
  }
  Scheduler::set_signals(UNBLOCK);
  return 0;
}

int Scheduler::resume_thread (int tid)
{
  Scheduler::set_signals(BLOCK);
  Thread* thread = _threads.at(tid);
  if (thread == nullptr)
  {
    std::cerr << NO_THREAD_ERROR << std::endl;
    Scheduler::set_signals(UNBLOCK);
    return -1;
  }
  thread->set_blocked (false);
  // change thread state to READY, move it to ready threads if it's not sleeping.
  if (thread->get_state() == BLOCKED && thread->get_sleep_timer() == -1)
  {
    thread->set_state (READY);
    _ready_threads.push_back (thread);
    _blocked_threads.remove(thread);
  }
  Scheduler::set_signals(UNBLOCK);
  return 0;
}

int Scheduler::sleep_thread(int num_quantums)
{
    Scheduler::set_signals(BLOCK);
  int thread_id = _running_thread->get_id();
  if (thread_id == 0)
  {
    std::cerr << SLEEP_MAIN_THREAD_ERROR << std::endl;
    Scheduler::set_signals(UNBLOCK);
    return -1;
  }
  // Add thread to sleeping threads. Set "sleep until" to curr quantum timer + num of quantums to sleep.
  _sleeping_threads.push_back(_threads.at(thread_id));
  _threads.at(thread_id)->set_sleep_timer(this->_quantum_counter + num_quantums);
  // Block the running thread. Will call quantum in it.
  block_thread(thread_id, true);
  Scheduler::set_signals(UNBLOCK);
  return 0;
}

void Scheduler::wake_threads()
{
    // iterate over sleeping threads, and wake up threads which reached quantum limit.
    for (auto i = _sleeping_threads.begin(); i != _sleeping_threads.end();i++)
    {
        if ((*i)->get_sleep_timer() <= _quantum_counter)
        {
            Thread* thread = (*i);
            i = _sleeping_threads.erase(i);
            thread->set_sleep_timer(-1);
            if(!thread->get_blocked())
                resume_thread(thread->get_id());
        }
    }

}

/**
 * Run the next ready thread. Handle the current running thread based on it's
 * status (BLOCKED, terminated, etc).
*/
void Scheduler::quantum ()
{
  // Configure the timer to expire after quantum time*/
  Scheduler::set_signals(BLOCK);
  if (_running_thread != nullptr)
  {
      int ret = sigsetjmp(env[_running_thread->get_id()], 1);
      if (ret != 0)
      {
          Scheduler::set_signals(UNBLOCK);
          return;
      }
  }
  _timer.it_value.tv_sec = _quantum_usecs / 1000000;
  _timer.it_value.tv_usec = _quantum_usecs % 1000000;
  // if the running thread wasn't BLOCKED or TERMINATED, move it to ready threads.
  if (!(_running_thread == nullptr || _running_thread->get_state() == BLOCKED))
  {
    _running_thread->set_state (READY);
    _ready_threads.push_back (_running_thread);
  }
  // get the thread from top of ready threads queue
  _running_thread = _ready_threads.front();
  _ready_threads.pop_front();
  _running_thread->set_state (RUNNING);
  // increment quantum counters (global and thread counter).
  _quantum_counter++;
  _running_thread->inc_quantum_counter();
  // wake up sleeping threads
  wake_threads();
  // Start a virtual timer. It counts down whenever this process is executing.
  if (setitimer(ITIMER_VIRTUAL, &_timer, nullptr))
  {
      std::cerr << SETITIMER_ERROR << std::endl;
  }
  Scheduler::set_signals(UNBLOCK);
  siglongjmp(env[_running_thread->get_id()], 2);
}

int Scheduler::get_thread_quantum_counter (int tid)
{
    Scheduler::set_signals(BLOCK);
    Thread* thread = _threads.at(tid);
    if (thread == nullptr)
    {
        std::cerr << NO_THREAD_ERROR << std::endl;
        Scheduler::set_signals(UNBLOCK);
        return -1;
    }
    Scheduler::set_signals(UNBLOCK);
    return thread->get_quantum_counter();
}

/**
 * Blocks / Unblocks signals while in critical state.
 */
void Scheduler::set_signals(signal_status status)
{
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGVTALRM);
    switch(status)
    {
        case BLOCK:
            sigprocmask(SIG_SETMASK, &set, NULL);
            break;
        case UNBLOCK:
            sigprocmask(SIG_UNBLOCK, &set, NULL);
            break;
    }
}