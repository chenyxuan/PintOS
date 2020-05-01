#include "threads/thread.h"
#include "list.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include <filesys/filesys.h>
#ifdef VM
#include <vm/page.h>
#include <userprog/syscall.h>
#endif
#include "filesys/file.h"
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"

#ifdef USERPROG
#include "userprog/process.h"
#include "malloc.h"

#endif

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* Implementation by ypm Started */
static struct list file_list;
/* Implementation by ypm Ended */

/* The average load of system */
static struct real_num_fixed32 load_avg;

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;

/* List of all processes.  Processes are added to this list
   when they are first scheduled and removed when they exit. */
static struct list all_list;

/* Implementation by Wang Started */
static struct list children;
/* Implementation by Wang Ended */

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Stack frame for kernel_thread(). */
struct kernel_thread_frame
{
    void *eip;                  /* Return address. */
    thread_func *function;      /* Function to call. */
    void *aux;                  /* Auxiliary data for function. */
};

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *running_thread (void);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static bool is_thread (struct thread *) UNUSED;
static void *alloc_frame (struct thread *, size_t size);
static void schedule (void);
void thread_schedule_tail (struct thread *prev);
static tid_t allocate_tid (void);

/* Implementation by Wang Started */
struct child_message *thread_get_child_message(tid_t tid)
{
  struct list_elem *e;
  struct child_message *l;
  for (e = list_rbegin (&children); e != list_rend (&children); e = list_prev (e))
  {
    l = list_entry (e, struct child_message, allelem);
    if (l->tid == tid)
      return l;
  }
  return NULL;
}
/* Implementation by Wang Ended */

/* For comparing two threads by their priorities.
   Return true if a_ points to the thread with higher priority. */
bool
thread_priority_more (const struct list_elem *a_, const struct list_elem *b_,
                      void *aux UNUSED)
{
  const struct thread *a = list_entry (a_, struct thread, elem);
  const struct thread *b = list_entry (b_, struct thread, elem);

  return thread_get_certain_priority (a) > thread_get_certain_priority (b);
}

/* Check if current thread need to yield
   (its priority is lower than the first one in the ready list). */
void
thread_revolt (void)
{
  if (thread_current () != idle_thread &&
      !list_empty (&ready_list) &&
      thread_get_priority () <
      thread_get_certain_priority (
              list_entry (list_begin (&ready_list), struct thread, elem)))
  {
    thread_yield ();
  }
}

/* Insert a thread to the ready list in order. */
void
thread_insert_ready_list (struct list_elem *elem)
{
  if (!list_empty (&ready_list))
    list_insert_ordered (&ready_list, elem, thread_priority_more, NULL);
  else
    list_push_back (&ready_list, elem);
}

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
void
thread_init (void)
{
  ASSERT (intr_get_level () == INTR_OFF);

  lock_init (&tid_lock);
  list_init (&ready_list);
  list_init (&all_list);
  list_init (&children);

  /* Implementation by ypm Started */
  list_init(&file_list);
  /* Implementation by ypm Ended */

  /* Set up a thread structure for the running thread. */
  initial_thread = running_thread ();
  init_thread (initial_thread, "main", PRI_DEFAULT);
  initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid ();

  /* Initializes the load average */
  load_avg = real_num_fixed32_init(0);
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void)
{
  /* Create the idle thread. */
  struct semaphore idle_started;
  sema_init (&idle_started, 0);
  thread_create ("idle", PRI_MIN, idle, &idle_started);

  /* Start preemptive thread scheduling. */
  intr_enable ();

  /* Wait for the idle thread to initialize idle_thread. */
  sema_down (&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void
thread_tick (void)
{
  struct thread *t = thread_current ();

  /* Update statistics. */
  if (t == idle_thread)
    idle_ticks++;
#ifdef USERPROG
    else if (t->pagedir != NULL)
    user_ticks++;
#endif
  else
    kernel_ticks++;

  /* Enforce preemption. */
  if (++thread_ticks >= TIME_SLICE)
    intr_yield_on_return ();
}

/* Prints thread statistics. */
void
thread_print_stats (void)
{
  printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
          idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
tid_t
thread_create (const char *name, int priority,
               thread_func *function, void *aux)
{
  struct thread *t;
  struct kernel_thread_frame *kf;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;
  tid_t tid;

  ASSERT (function != NULL);

  /* Allocate thread. */
  t = palloc_get_page (PAL_ZERO);
  if (t == NULL)
    return TID_ERROR;

  /* Initialize thread. */
  init_thread (t, name, priority);
  tid = t->tid = allocate_tid ();

  /* Implementation by ymt Started */
#ifdef FILESYS
  t->current_dir = thread_current()->current_dir;
#endif
  /* Implementation by ymt Ended */

  /* Implementation by Wang Started */
  struct child_message *own = palloc_get_page (PAL_ZERO);
  own->tid = tid;
  own->tchild = t;
  own->exited = false;
  own->terminated = false;
  own->load_failed = false;
  own->return_value = 0;
  own->sema_finished = &t->sema_finished;
  own->sema_started = &t->sema_started;
  list_push_back (&children, &own->allelem);
  t->message_to_grandpa = own;
  /* Implementation by Wang Ended */

  /* Stack frame for kernel_thread(). */
  kf = alloc_frame (t, sizeof *kf);
  kf->eip = NULL;
  kf->function = function;
  kf->aux = aux;

  /* Stack frame for switch_entry(). */
  ef = alloc_frame (t, sizeof *ef);
  ef->eip = (void (*) (void)) kernel_thread;

  /* Stack frame for switch_threads(). */
  sf = alloc_frame (t, sizeof *sf);
  sf->eip = switch_entry;
  sf->ebp = 0;

  /* Implementation by ypm Started */
#ifdef USERPROG
  t->exec_file = NULL;
#endif
  /* Implementation by ypm Ended */

  /* Add to run queue. */
  thread_unblock (t);
  thread_revolt ();
  return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void
thread_block (void)
{
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF);

  thread_current ()->status = THREAD_BLOCKED;
  schedule ();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void
thread_unblock (struct thread *t)
{
  enum intr_level old_level;

  ASSERT (is_thread (t));

  old_level = intr_disable ();
  ASSERT (t->status == THREAD_BLOCKED);
  thread_insert_ready_list (&t->elem);
  t->status = THREAD_READY;
  intr_set_level (old_level);
}

/* Returns the name of the running thread. */
const char *
thread_name (void)
{
  return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void)
{
  struct thread *t = running_thread ();

  /* Make sure T is really a thread.
     If either of these assertions fire, then your thread may
     have overflowed its stack.  Each thread has less than 4 kB
     of stack, so a few big automatic arrays or moderate
     recursion can cause stack overflow. */
  ASSERT (is_thread (t));
  ASSERT (t->status == THREAD_RUNNING);

  return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void)
{
  return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void)
{
  ASSERT (!intr_context ());

#ifdef USERPROG
  process_exit ();

  /* Implementation by Wang Started */
  sema_up (&thread_current ()->sema_finished);
  /* Implementation by Wang Ended */

  /* Implementation by ypm Started */
  struct thread* cur = thread_current();
  if (!list_empty(&file_list)){
    struct list_elem* i;
    for (i = list_begin(&file_list); i != list_end(&file_list); i = list_next(i)){
      struct file_handle* hd;
      hd = list_entry(i, struct file_handle, elem);
      if (hd->owned_thread == cur){
        syscall_file_close(hd->opened_file);
        i = list_prev(i);
        list_remove(&(hd->elem));
        free(hd);
      }
    }
  }
  if(cur->exec_file != NULL){
    syscall_file_close(cur->exec_file);
  }
  /* Implementation by ypm Ended */

#endif

  /* Remove thread from all threads list, set our status to dying,
     and schedule another process.  That process will destroy us
     when it calls thread_schedule_tail(). */
  intr_disable ();
  list_remove (&thread_current ()->allelem);
  thread_current ()->status = THREAD_DYING;
  schedule ();
  NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void)
{
  struct thread *cur = thread_current ();
  enum intr_level old_level;

  ASSERT (!intr_context ());

  old_level = intr_disable ();
  if (cur != idle_thread)
    thread_insert_ready_list (&cur->elem);
  cur->status = THREAD_READY;
  schedule ();
  intr_set_level (old_level);
}

/* Invoke function 'func' on all threads, passing along 'aux'.
   This function must be called with interrupts off. */
void
thread_foreach (thread_action_func *func, void *aux)
{
  struct list_elem *e;

  ASSERT (intr_get_level () == INTR_OFF);

  for (e = list_begin (&all_list); e != list_end (&all_list);
       e = list_next (e))
  {
    struct thread *t = list_entry (e, struct thread, allelem);
    func (t, aux);
  }
}


/* Handle sleep_ticks for each thread */
static void
thread_sleep_ticks_handler(struct thread *t, void *args UNUSED)
{
  if (t->sleep_ticks > 0)
  {
    (t->sleep_ticks)--;
    if (!(t->sleep_ticks))
      thread_unblock(t);
  }
}


/* Sets the current thread's priority to NEW_PRIORITY. */
void
thread_set_priority (int new_priority)
{
  if (thread_mlfqs)
    return;
  /* Warning: if current thread is donated,
     this behaviour should be delayed until the donation is released! */
  if (thread_current ()->max_donate == 0)
  {
    thread_current ()->priority = new_priority;
    thread_revolt ();
  }
  else
  {
    /* Delay this behaviour temporarily. */
    thread_current ()->priority_to_set = new_priority;
  }
}

/* Returns the current thread's priority. */
int
thread_get_priority (void)
{
  return thread_get_certain_priority (thread_current ());
}

/* Returns the certain thread's priority. */
int
thread_get_certain_priority (const struct thread *t)
{
  return t->priority + (thread_mlfqs ? 0 : t->max_donate) ;
}

/* Update thread priority using recent_cpu and nice value of threads */
static void
thread_update_priority(struct thread* t, void *args UNUSED){
  if(t == idle_thread)
    return;
  int recent_cpu_div4 = real_num_fixed32_trunc(real_num_fixed32_div_int(t->recent_cpu, 4));
  t->priority =  PRI_MAX - recent_cpu_div4 - t->nice * 2;
}

void thread_ready_list_sort(void){
  list_sort(&ready_list, thread_priority_more, NULL);
}

/* Sets the current thread's nice value to NICE. */
void
thread_set_nice (int nice)
{
  thread_current()->nice = nice;
  if(thread_mlfqs){
    thread_update_priority(thread_current(), NULL);
  }

}

/* Returns the current thread's nice value. */
int
thread_get_nice (void)
{
  struct thread *cur = thread_current();
  return cur->nice;
}

/* Update load_avg
 * load_avg = 59/60 * load_avg + 1/60 * ready_threads
 * */
static void
thread_update_load_avg(void)
{

  ASSERT(thread_mlfqs);
  int ready_threads = 0;
  struct list_elem *e;
  for (e = list_begin (&all_list); e != list_end (&all_list);
       e = list_next (e))
  {
    struct thread *t = list_entry (e, struct thread, allelem);
    if ((t->status == THREAD_RUNNING || t->status == THREAD_READY) && t != idle_thread)
      ready_threads++;
  }
  load_avg = real_num_fixed32_div_int(real_num_fixed32_add_int
                                              (real_num_fixed32_mul_int(load_avg, 59),ready_threads),60);
}

/* Returns 100 times the system load average. */
int
thread_get_load_avg (void)
{
  ASSERT(thread_mlfqs);
  return real_num_fixed32_round(real_num_fixed32_mul_int(load_avg, 100));
}

/* Add recent_cpu of the running thread by 1 per second */
void thread_add_recent_cpu(void)
{
  ASSERT(thread_mlfqs);
  struct thread* cur = thread_current();
  if (cur != idle_thread)
    cur->recent_cpu = real_num_fixed32_add_int(cur->recent_cpu, 1);
  thread_update_priority(thread_current(), NULL);
}


/* Update recent_cpu
 * recent_cpu = recent_cpu * (load_avg * 2 / (load_avg * 2 + 1)) + nice
 * Nice is thread's nice value
 * */
static void
thread_update_recent_cpu(struct thread *t, void* aux UNUSED)
{
  ASSERT(thread_mlfqs);
  int load_avg2 = thread_get_load_avg() * 2;
  t->recent_cpu = real_num_fixed32_add_int(real_num_fixed32_mul
                                                   (real_num_fixed32_div_int2(load_avg2, load_avg2 + 100),
                                                    t->recent_cpu), t->nice);
}

/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void)
{
  ASSERT(thread_mlfqs);
  return real_num_fixed32_trunc(
          real_num_fixed32_mul_int(thread_current()->recent_cpu, 100)
  );
}

/* Implementation by ypm Started */
void
thread_timer(bool full_second){
  if (thread_mlfqs){
    thread_add_recent_cpu();
    if (full_second){
      thread_update_load_avg();
      thread_foreach(thread_update_recent_cpu, NULL);
      thread_foreach(thread_update_priority, NULL);
      thread_ready_list_sort();
    }
  }
  thread_foreach(thread_sleep_ticks_handler, NULL);
}
/* Implementation by ypm Ended */

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED)
{
  struct semaphore *idle_started = idle_started_;
  idle_thread = thread_current ();
  sema_up (idle_started);

  for (;;)
  {
    /* Let someone else run. */
    intr_disable ();
    thread_block ();

    /* Re-enable interrupts and wait for the next one.

       The `sti' instruction disables interrupts until the
       completion of the next instruction, so these two
       instructions are executed atomically.  This atomicity is
       important; otherwise, an interrupt could be handled
       between re-enabling interrupts and waiting for the next
       one to occur, wasting as much as one clock tick worth of
       time.

       See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
       7.11.1 "HLT Instruction". */
    asm volatile ("sti; hlt" : : : "memory");
  }
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux)
{
  ASSERT (function != NULL);

  intr_enable ();       /* The scheduler runs with interrupts off. */
  function (aux);       /* Execute the thread function. */
  thread_exit ();       /* If function() returns, kill the thread. */
}

/* Returns the running thread. */
struct thread *
running_thread (void)
{
  uint32_t *esp;

  /* Copy the CPU's stack pointer into `esp', and then round that
     down to the start of a page.  Because `struct thread' is
     always at the beginning of a page and the stack pointer is
     somewhere in the middle, this locates the curent thread. */
  asm ("mov %%esp, %0" : "=g" (esp));
  return pg_round_down (esp);
}

/* Returns true if T appears to point to a valid thread. */
static bool
is_thread (struct thread *t)
{
  return t != NULL && t->magic == THREAD_MAGIC;
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread (struct thread *t, const char *name, int priority)
{
  enum intr_level old_level;

  ASSERT (t != NULL);
  ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
  ASSERT (name != NULL);

  memset (t, 0, sizeof *t);
  t->status = THREAD_BLOCKED;
  strlcpy (t->name, name, sizeof t->name);
  t->stack = (uint8_t *) t + PGSIZE;
  t->priority = priority;
  t->magic = THREAD_MAGIC;

  if(thread_mlfqs) {
    t->nice = 0;
    t->recent_cpu = real_num_fixed32_init(0);
    t->priority =  PRI_MAX;
  }

  list_init (&t->lock_list);
  t->priority_to_set = -1;
  t->max_donate = 0;
  t->father = NULL;

  /* Implementation by Chen Started */
  t->return_value = 0;
  /* Implementation by Chen Ended */

  /* Implementation by Wang Started */
  t->grandpa_died = false;
  list_init (&t->child_list);
  sema_init (&t->sema_finished, 0);
  sema_init (&t->sema_started, 0);
  /* Implementation by Wang Ended */
#ifdef VM
  /* Implementation by ymt Started */
  list_init(&t->mmap_file_list);
  t->next_mapid = 1;
  /* Implementation by ymt Ended */
#endif
  old_level = intr_disable ();
  list_push_back (&all_list, &t->allelem);
  intr_set_level (old_level);
}

/* Allocates a SIZE-byte frame at the top of thread T's stack and
   returns a pointer to the frame's base. */
static void *
alloc_frame (struct thread *t, size_t size)
{
  /* Stack data is always allocated in word-size units. */
  ASSERT (is_thread (t));
  ASSERT (size % sizeof (uint32_t) == 0);

  t->stack -= size;
  return t->stack;
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *
next_thread_to_run (void)
{
  if (list_empty (&ready_list))
    return idle_thread;
  else
    return list_entry (list_pop_front (&ready_list), struct thread, elem);
}

/* Completes a thread switch by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.  This function is normally invoked by
   thread_schedule() as its final action before returning, but
   the first time a thread is scheduled it is called by
   switch_entry() (see switch.S).

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function.

   After this function and its caller returns, the thread switch
   is complete. */
void
thread_schedule_tail (struct thread *prev)
{
  struct thread *cur = running_thread ();

  ASSERT (intr_get_level () == INTR_OFF);

  /* Mark us as running. */
  cur->status = THREAD_RUNNING;

  /* Start new time slice. */
  thread_ticks = 0;

#ifdef USERPROG
  /* Activate the new address space. */
  process_activate ();

#endif

  /* If the thread we switched from is dying, destroy its struct
     thread.  This must happen late so that thread_exit() doesn't
     pull out the rug under itself.  (We don't free
     initial_thread because its memory was not obtained via
     palloc().) */
  if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread)
  {
    ASSERT (prev != cur);
    palloc_free_page (prev);
  }
}

/* Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.

   It's not safe to call printf() until thread_schedule_tail()
   has completed. */
static void
schedule (void)
{
  struct thread *cur = running_thread ();
  struct thread *next = next_thread_to_run ();
  struct thread *prev = NULL;

  ASSERT (intr_get_level () == INTR_OFF);
  ASSERT (cur->status != THREAD_RUNNING);
  ASSERT (is_thread (next));

  if (cur != next)
    prev = switch_threads (cur, next);
  thread_schedule_tail (prev);
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void)
{
  static tid_t next_tid = 1;
  tid_t tid;

  lock_acquire (&tid_lock);
  tid = next_tid++;
  lock_release (&tid_lock);

  return tid;
}

/* Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof (struct thread, stack);

/* Implementation by ypm Started */
/* Terminate thread with a return value FINAL_VALUE */
void
thread_exit_with_return_value(struct intr_frame *f, int return_value) {
  struct thread *cur = thread_current();
  cur->return_value = return_value;
  f->eax = (uint32_t)return_value;
  thread_exit();
}

void
thread_file_list_inster(struct file_handle* fh){
  list_push_back(&file_list, &(fh->elem));
}

/* Get the file_handle pointer according to fd
 * Return NULL if fd is invalid
 * */
struct file_handle* syscall_get_file_handle(int fd){
  struct thread* cur =  thread_current();
  struct list_elem* i;
  for (i = list_begin(&file_list); i != list_end(&file_list); i = list_next(i)){
    struct file_handle* t;
    t = list_entry(i, struct file_handle, elem);
    if (t->fd == fd){
      if (t->owned_thread != cur)
        return NULL;
      else
        return t;
    }
  }
  return NULL;
}


/* Implementation by ypm Ended */

/* Implementation by ymt Started */
#ifdef FILESYS
/*
 * Set main thread's current directory
 * MUST use after filesys init
 * */
void set_main_thread_dir()
{
  initial_thread->current_dir = dir_open_root();
}
#endif
/* Implementation by ymt Ended */