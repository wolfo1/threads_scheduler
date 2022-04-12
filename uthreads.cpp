#include "uthreads.h"
#include "Scheduler.h"
#define QUANTUM_ERROR "thread library error: quantum has to be positive."
#define MAIN_THREAD_BLOCK_ERROR "thread library error: cannot block main thread (0)."


int uthread_init(int quantum_usecs)
{
  if (quantum_usecs <= 0)
    {
      std::cerr << QUANTUM_ERROR << std::endl;
      return -1;
    }
  Scheduler* scheduler = Scheduler::init(quantum_usecs);
  if (scheduler == nullptr)
      exit(1);
  Scheduler::scheduler = scheduler;
  return 0;
}

int uthread_spawn(thread_entry_point entry_point)
{
  return Scheduler::scheduler->spawn_thread(entry_point);
}

int uthread_terminate(int tid)
{
  if (tid == 0)
    {
      delete Scheduler::scheduler;
      exit(0);
    }
  return Scheduler::scheduler->terminate_thread(tid);
}

int uthread_block(int tid)
{
  if (tid == 0 || tid < 0 || tid >= MAX_THREAD_NUM)
  {
    std::cerr << MAIN_THREAD_BLOCK_ERROR << std::endl;
    return -1;
  }
  return Scheduler::scheduler->block_thread(tid, false);
}

int uthread_resume(int tid)
{
    return Scheduler::scheduler->resume_thread(tid);
}

int uthread_sleep(int num_quantums)
{
  if (num_quantums <= 0)
  {
    std::cerr << QUANTUM_ERROR << std::endl;
    return -1;
  }
  return Scheduler::scheduler->sleep_thread(num_quantums);
}

int uthread_get_tid()
{
    return Scheduler::scheduler->get_running_thread()->get_id();
}

int uthread_get_total_quantums()
{
    return Scheduler::scheduler->get_quantum_counter();
}
int uthread_get_quantums(int tid)
{
    return Scheduler::scheduler->get_thread_quantum_counter (tid);
}

