#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/synch.h"
#include "threads/malloc.h"
#include "threads/pte.h"
#include "lib/kernel/hash.h"
#include "vm/page.h"
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
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */
#define NICE_MAX 20                     /* Highest nice */
#define NICE_MIn -20                    /* Lowest nice */
#define FILE_HDL_SIZE 16                /* Default size of file handlers */

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
    int priority;                       /* Priority. */
    struct list_elem allelem;           /* List element for all threads list. */
    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /* List element. */
    int64_t wake_up_time;               /* Time to wake up current thread */
    struct list_elem alarm_elem;        /* List element for alarm queue */
    int eff_priority;                   /* Effective priority */
    struct lock *lock_to_acquire;       /* Lock this thread is waiting for */
    struct list locks_waited_by_others; /* Locks held by this thread but
                                           also waited by other threads*/
    int recent_cpu;                     /* CPU time received recently */
    int nice;                           /* Nice value of each thread*/
    bool is_kernel;                     /* True if this is a kernel process */
    bool in_syscall;                    /* True if thread is in system call */
    void *esp;                          /* Saved stack pointer for interrupt
                                           in the kernel */
    struct dir *cwd;                    /* Current working directory */

    /* Owned by userprog/process.c. */
    uint32_t *pagedir;                 /* Page directory. */
    struct hash suppl_pt;              /* Supplemental page table */
    struct exit_status *exit_status;   /* Exit status of this thread */
    struct list child_exit_status;     /* List of child processes' exit status */
    struct lock list_lock;             /* Lock on child exit status list  */

    struct file **file_handlers;       /* File handler array */
    int file_handlers_size;            /* Size of allocated file handlers */
    int file_handlers_num;             /* Number of current file handlers */
    struct file *process_file;         /* File of this current process */

    struct hash mmap_files;            /* Hashtable of memory mapped files */
    int mmap_files_num_ever;           /* # of files ever mapped, used as key */
    unsigned magic;                    /* Detects stack overflow. */
  };

/* Exit status of a process */
struct exit_status
  {
    int pid;                            /* Process Id. Same as tid in Pintos */
    int exit_value;                     /* Exit value */
    struct semaphore sema_wait;         /* Semaphore to sync on wait() */
    int ref_counter;                    /* Reference counter for release */
    struct lock counter_lock;           /* Lock on accessing ref_counter */
    struct list_elem elem;              /* List element for exit_status list */
    struct lock *list_lock;             /* Lock on list modification */
  };

/* Load status of a process */
struct load_status
  {
    struct semaphore sema_load;         /* Semaphore to sync on load() */
    bool load_success;                  /* True if load successfully */
    char *cmd_line;                     /* Command line to launch the process */
    struct thread *parent_thread;       /* Pointer to the parent thread */
  };

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init (void);
void thread_init_cwd (void);
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

int thread_get_priority (void);
void thread_set_priority (int);
void thread_set_eff_priority (struct thread *, int);
int thread_find_max_priority (struct thread *t);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

bool priority_greater_or_equal (const struct list_elem *,
                                const struct list_elem *,
                                void *);

int get_num_ready_threads (void);
void calculate_load_avg (void);
void calculate_recent_cpu (struct thread * th, void *aux UNUSED);
void calculate_recent_cpu_all (void);
void calculate_priority_advanced (struct thread * th, void *aux UNUSED);
void calculate_priority_advanced_all (void);

bool init_exit_status (struct thread *, tid_t );

bool valid_file_handler (struct thread* thread, int fd);
int thread_add_file_handler (struct thread* thread, struct file* file);
void thread_remove_file_handler (struct thread* thread, int fd);

#endif /* threads/thread.h */
