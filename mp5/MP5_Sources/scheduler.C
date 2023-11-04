/*
 File: scheduler.C
 
 Author:
 Date  :
 
 */

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "scheduler.H"
#include "thread.H"
#include "console.H"
#include "utils.H"
#include "assert.H"
#include "simple_keyboard.H"
#include "simple_timer.H"

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* CONSTANTS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* FORWARDS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

TQueue * TQueue::head = NULL;
TQueue * TQueue::tail = NULL;

TQueue::TQueue(Thread * _thread) {
  thread = _thread;
  next = NULL;
}

void TQueue::add(TQueue * _queue) {
  if(head == NULL){
    head = _queue;
    tail = _queue;
  }
  else{
    tail->next = _queue;
    tail = _queue;
  }
}

Thread * TQueue::pop() {
  Thread * deleting_thread = head->thread;
  TQueue * temp_node = head; 
  if(head == tail){
    head = NULL;
    tail = NULL;
  }
  else{
    head = head->next;
  }
  delete temp_node;
  return deleting_thread;
}

/*--------------------------------------------------------------------------*/
/* METHODS FOR CLASS   S c h e d u l e r  */
/*--------------------------------------------------------------------------*/

Scheduler::Scheduler() {
  Console::puts("Constructed Scheduler.\n");
}

void Scheduler::yield() {
  if(Machine::interrupts_enabled()) Machine::disable_interrupts();
  Console::puts("Yielding...\n");

  if(ready_queue->head == NULL) {
    Console::puts("No threads to run.\n");
    if(!Machine::interrupts_enabled()) Machine::enable_interrupts();
    return;
  }else {
    Thread * next_thread = TQueue::pop();
    Console::puts("Next thread to run: "); Console::puti(next_thread->ThreadId()); Console::puts("\n");
    Thread::dispatch_to(next_thread);
  }
  
  Console::puts("Yield finish.\n");
  if(!Machine::interrupts_enabled()) Machine::enable_interrupts();
}

void Scheduler::resume(Thread * _thread) {
  if(Machine::interrupts_enabled()) Machine::disable_interrupts();
  Console::puts("Resuming...\n");

  TQueue * new_queue = new TQueue(_thread);
  TQueue::add(new_queue);

  Console::puts("Resume thread to ready queue: "); Console::puti(_thread->ThreadId()); Console::puts("\n");
  if(!Machine::interrupts_enabled()) Machine::enable_interrupts();
}

void Scheduler::add(Thread * _thread) {
  if(Machine::interrupts_enabled()) Machine::disable_interrupts();
  Console::puts("Adding thread: "); Console::puti(_thread->ThreadId()); Console::puts("\n");
  
  resume(_thread);

  if(!Machine::interrupts_enabled()) Machine::enable_interrupts();
}

void Scheduler::terminate(Thread * _thread) {
  if(Machine::interrupts_enabled()) Machine::disable_interrupts();
  
  Console::puts("Terminating thread: "); Console::puti(_thread->ThreadId()); Console::puts("\n");
  if(_thread == Thread::CurrentThread()) {
    _thread->delete_thread();
    yield();
  }else {
    // Currently we don't deal with this case.
    Console::puts("Thread terminating another thread.\n");
  }
  
  if(!Machine::interrupts_enabled()) Machine::enable_interrupts();
}