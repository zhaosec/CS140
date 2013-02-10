#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

/* Retrieve the n-th argument */
#define GET_ARGUMENT(sp, n) (*(sp + n))

static void syscall_handler (struct intr_frame *);
bool valid_vaddr_range(const void * vaddr, unsigned size);

void  _halt (void);
void  _exit (int status);
pid_t _exec (const char *cmd_line);
int   _wait (pid_t pid);
bool  _create (const char *file, unsigned initial_size);
bool  _remove (const char *file);
int   _open (const char *file);
int   _filesize (int fd);
int   _read (int fd, void *buffer, unsigned size);
int   _write (int fd, const void *buffer, unsigned size);
void  _seek (int fd, unsigned position);
unsigned _tell (int fd);
void  _close (int fd);

struct lock global_lock_filesys;

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init (&global_lock_filesys);
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{

  /* Convert ESP to a int pointer */
  int * esp = (int *)f->esp;
  uint32_t arg1, arg2, arg3;

  /* Get syscall number */
  int syscall_no = *esp;
  switch(syscall_no)
  {
    case SYS_HALT:
      _halt ();
      break;

    case SYS_EXIT:
      arg1 = GET_ARGUMENT(esp, 1);
      _exit (arg1);
      break;

    case SYS_EXEC:
      arg1 = GET_ARGUMENT(esp, 1);
      f->eax = (uint32_t) _exec ((char*) arg1);
      break;

    case SYS_WAIT:
      arg1 = GET_ARGUMENT(esp, 1);
      f->eax = (uint32_t) _wait ((int) arg1);
      break;

    case SYS_CREATE:
      arg1 = GET_ARGUMENT(esp, 1);
      arg2 = GET_ARGUMENT(esp, 2);
      f->eax = (uint32_t) _create ((const char*)arg1, (unsigned)arg2);
      break;

    case SYS_REMOVE:
      arg1 = GET_ARGUMENT(esp, 1);
      f->eax = (uint32_t) _remove ((const char*)arg1);
      break;

    case SYS_OPEN:
      arg1 = GET_ARGUMENT(esp, 1);
      f->eax = (uint32_t) _open ((const char*)arg1);
      break;

    case SYS_FILESIZE:
      arg1 = GET_ARGUMENT(esp, 1);
      f->eax = (uint32_t) _filesize ((int)arg1);
      break;

    case SYS_READ:
      arg1 = GET_ARGUMENT(esp, 1);
      arg2 = GET_ARGUMENT(esp, 2);
      arg3 = GET_ARGUMENT(esp, 3);
      f->eax = (uint32_t) _read ((int)arg1, (void*)arg2, (unsigned)arg3);
      break;

    case SYS_WRITE:
      arg1 = GET_ARGUMENT(esp, 1);
      arg2 = GET_ARGUMENT(esp, 2);
      arg3 = GET_ARGUMENT(esp, 3);
      f->eax = (uint32_t) _write ((int)arg1, (const void*)arg2, (unsigned)arg3);
      break;

    case SYS_SEEK:
      arg1 = GET_ARGUMENT(esp, 1);
      arg2 = GET_ARGUMENT(esp, 2);
      _seek ((int)arg1, (unsigned)arg2);
      break;

    case SYS_TELL:
      arg1 = GET_ARGUMENT(esp, 1);
      f->eax = (uint32_t) _tell ((int)arg1);
      break;

    case SYS_CLOSE:
      arg1 = GET_ARGUMENT(esp, 1);
      _close ((int)arg1);
      break;

    default:
      break;
  }
}

/* Return true if virtual address range [vaddr, vadd+size] is valid */
inline bool
valid_vaddr_range(const void * vaddr, unsigned size)
{
  /* If the address exceeds PHYS_BASE, exit -1 */
  if (!is_user_vaddr (vaddr) || !is_user_vaddr (vaddr + size))
    return false;
  return true;
}

void
_halt (void)
{
  // TODO
}

pid_t
_exec (const char *cmd_line)
{
  /* Check address */
  if (!valid_vaddr_range(cmd_line, strlen(cmd_line)))
    _exit (-1);

  /* Lock on process_execute since it needs to open the executable file */
  lock_acquire (&global_lock_filesys);
  pid_t pid = (pid_t) process_execute(cmd_line);
  lock_release (&global_lock_filesys);

  if (pid == (pid_t) TID_ERROR)
    return -1;
  return pid;
}

void
_exit(int status)
{
  struct thread * cur_thread = thread_current();
  cur_thread->exit_status->exit_value = status;
  thread_exit ();
}


int
_wait(pid_t pid)
{
  return process_wait(pid);
}

bool
_create (const char *file, unsigned initial_size)
{
  if (!valid_vaddr_range (file, strlen (file)))
    _exit (-1);

  lock_acquire (&global_lock_filesys);
  bool success = filesys_create (file, initial_size);
  lock_release (&global_lock_filesys);
  return success;
}

bool
_remove (const char *file)
{
  if (!valid_vaddr_range (file, strlen (file)))
    _exit (-1);

  lock_acquire (&global_lock_filesys);
  bool success = filesys_remove (file);
  lock_release (&global_lock_filesys);
  return success;
}

int
_open (const char *file)
{
  if (!valid_vaddr_range (file, strlen (file)))
    _exit (-1);

  lock_acquire (&global_lock_filesys);
  struct file* f_struct = filesys_open (file);
  lock_release (&global_lock_filesys);

  // TODO not finished yet
  return -1;
}

int
_filesize (int fd)
{
  // TODO
  return 0;
}

int
_read (int fd, void *buffer, unsigned size)
{
  // TODO
  return 0;
}

int
_write (int fd, const void *buffer, unsigned size)
{
  // TODO: A simple write added for testing purpose. Need more work
  int result = 0;
  if (fd == STDOUT_FILENO)
  {
    putbuf (buffer, size);
    result = size;
  }
  return result;
}

void
_seek (int fd, unsigned position)
{
  // TODO
}

unsigned
_tell (int fd)
{
  // TODO
  return 0;
}

void
_close (int fd)
{
  // TODO
}

