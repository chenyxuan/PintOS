#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "synch.h"
#include "../lib/kernel/hash.h"
#include "threads/real-number.h"
#include "threads/interrupt.h"
#include "filesys/off_t.h"
#include "filesys/directory.h"

/* States in a thread's life cycle. */
enum thread_status
{
    THREAD_RUNNING,     /* Running thread. */
    THREAD_READY,       /* Not running but ready to run. */
    THREAD_BLOCKED,     /* Waiting for an event to trigger. */
    THREAD_DYING        /* About to be destroyed. */
};

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;

/* Implementation by ymt Started */
typedef int mapid_t;
/* Implementation by ymt Ended */

#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */

/* Implementation by Wang Started */
struct thread;

struct child_message
{
    struct thread *tchild;              /* Thread pointer to the child. */
    tid_t tid;                          /* Thread ID. */
    bool exited;                        /* If syscall exit() is called. */
    bool terminated;                    /* If the child finishes running. */
    bool load_failed;                   /* If the child has a load fail. */
    int return_value;                   /* Return value. */
    struct semaphore *sema_finished;    /* Semaphore to finish. */
    struct semaphore *sema_started;     /* Semaphore to finish loading. */
    struct list_elem elem;              /* List element for grandpa's child list. */
    struct list_elem allelem;           /* List element for global child list. */
};
/* Implementation by Wang Ended */

/* A kernel thread or user process.

   Each thread structure is stored in its own 4 kB page.  The
   thread structure itself sits at the very bottom of the page
   (at offset 0).  The rest of the page is reserved for the
   thread's kernel stack, which grows downward from the top of
   the page (at offset 4 kB).  Here's an illustration:

        4 kB +---------------------------------+
             |          kernel stack           |
             |                |                |
             |                |                |
             |                V                |
             |         grows downward          |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             +---------------------------------+
             |              magic              |
             |                :                |
             |                :                |
             |               name              |
             |              status             |
        0 kB +---------------------------------+

   The upshot of this is twofold:

      1. First, `struct thread' must not be allowed to grow too
         big.  If it does, then there will not be enough room for
         the kernel stack.  Our base `struct thread' is only a
         few bytes in size.  It probably should stay well under 1
         kB.

      2. Second, kernel stacks must not be allowed to grow too
         large.  If a stack overflows, it will corrupt the thread
         state.  Thus, kernel functions should not allocate large
         structures or arrays as non-static local variables.  Use
         dynamic allocation with malloc() or palloc_get_page()
         instead.

   The first symptom of either of these problems will probably be
   an assertion failure in thread_current(), which checks that
   the `magic' member of the running thread's `struct thread' is
   set to THREAD_MAGIC.  Stack overflow will normally change this
   value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
   the run queue (thread.c), or it can be an element in a
   semaphore wait list (synch.c).  It can be used these two ways
   only because they are mutually exclusive: only a thread in the
   ready state is on the run queue, whereas only a thread in the
   blocked state is on a semaphore wait list. */
struct thread
{
    /* Owned by thread.c. */
    tid_t tid;                          /* Thread identifier. */
    enum thread_status status;          /* Thread state. */
    char name[16];                      /* Name (for debugging purposes). */
    uint8_t *stack;                     /* Saved stack pointer. */

    int nice;                           /* The nice level of thread, the higher the lower priority */
    struct real_num_fixed32 recent_cpu; /* Thread recent cpu usage */
    int priority;                       /* Priority. */
    int64_t sleep_ticks;                /* Ticks that the thread to sleep. */

    struct list lock_list;              /* Locks owned by this thread. */
    int priority_to_set;                /* Priority to be set. */
    int max_donate;                     /* Max Donation. */
    struct thread *father;              /* Thread who locks this thread. */
    struct list_elem donate_elem;       /* List element for donation list of locks. */

    struct list_elem allelem;           /* List element for all threads list. */

    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /* List element. */

    /* Implementation by Chen Started */
    int return_value;                   /* Return value of the thread (anyway, nobody cares)*/
    /* Implementation by Chen Ended*/

    /* Implementation by Wang Started */
    struct list child_list;             /* Child List. */
    struct semaphore sema_finished;     /* Semaphore to finish. */
    struct semaphore sema_started;      /* Semaphore to finish loading. */
    bool grandpa_died;                  /* Grandpa is dead or not. */
    struct child_message *message_to_grandpa;
    /* Child message for grandpa. */
    /* Implementation by Wang Ended */


#ifdef USERPROG
    /* Owned by userprog/process.c. */
    uint32_t *pagedir;                  /* Page directory. */

    /* Implementation by ypm Started */
    struct file* exec_file;
    /* Implementation by ypm Ended */
#endif
#ifdef VM
    /* Implementation by Chen Started */
	struct hash* page_table;
    void *esp;
    /* Implementation by Chen ended */
    /* Implementation by ymt Started */
    struct list mmap_file_list;
    mapid_t next_mapid;
    /* Implementation by ymt Ended */

#endif
    /* Implementation by ymt Started */
#ifdef FILESYS
    struct dir *current_dir;
#endif
    /* Implementation by ymt Ended */
    /* Owned by thread.c. */
    unsigned magic;                     /* Detects stack overflow. */

};

/* Implementation by ypm Started */
struct file_handle{
    int fd;
    struct file* opened_file;
    struct thread* owned_thread;
    /* Implementation by ymt Started */
#ifdef FILESYS
    struct dir* opened_dir;
#endif
    /* Implementation by ymt Ended */
    struct list_elem elem;
};
/* Implementation by ypm Ended */

/* Implementation by ymt Started */
struct mmap_handler{
    mapid_t mapid; // mmap 唯一标识符
    struct file* mmap_file; // mmap 的文件
    void* mmap_addr; // mmap的目标 vaddr
    int num_page; // mmap 的文件占用多少page
    int last_page_size; // 0表示filesize和pagesize对齐, 非0表示最后一页实际用的大小
    struct list_elem elem;
    bool writable;
    bool is_segment; // is used in load_segment
    bool is_static_data; // whether it is from static data;
    int num_page_with_segment; // total pages with zeros bytes
    off_t file_ofs;
};
/* Implementation by ymt Ended */

/* Implementation by Wang Started */
struct child_message *thread_get_child_message(tid_t tid);
/* Implementation by Wang Ended */


/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

bool thread_priority_more (const struct list_elem *a_, const struct list_elem *b_,
                           void *aux UNUSED);
void thread_revolt (void);
void thread_insert_ready_list (struct list_elem *elem);

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
                        void thread_yield (void);

/* Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func (struct thread *t, void *aux);
void thread_foreach (thread_action_func *, void *);
static void thread_sleep_ticks_handler(struct thread *t, void *args UNUSED);

int thread_get_priority (void);
int thread_get_certain_priority (const struct thread *t);
void thread_set_priority (int);
static void thread_update_priority(struct thread*, void*);
void thread_ready_list_sort(void);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);
static void thread_update_load_avg(void);
static void thread_update_recent_cpu(struct thread*, void*);
void thread_add_recent_cpu(void);

/* Implementation by ypm Started */
void thread_timer(bool);
void thread_exit_with_return_value(struct intr_frame *f, int return_value);

void thread_file_list_inster(struct file_handle* fh);
struct file_handle* syscall_get_file_handle(int fd);
/* Implementation by ypm Ended */

/* Implementation by ymt Started */
#ifdef FILESYS
void set_main_thread_dir();
#endif
/* Implementation by ymt Ended */

#endif /* threads/thread.h */

