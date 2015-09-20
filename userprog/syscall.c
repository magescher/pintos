#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"

static void syscall_handler (struct intr_frame *);

/* generate function signature for syscall */
#define SYSCALL_FUNC(name)   \
syscall_handler_ ## name (     \
  void *arg0 UNUSED,          \
  void *arg1 UNUSED,          \
  void *arg2 UNUSED,          \
  struct intr_frame *f UNUSED \
)

/* using within syscall to verify arguments */
#define CHECK_ARGS(num) \
  check_user_mem (arg0, num * sizeof (arg0));

static void
check_user_mem (void *addr, size_t size)
{
  if (addr + size >= PHYS_BASE) {
    thread_exit ();
  }
}

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
SYSCALL_FUNC(halt)
{
  shutdown_power_off ();
}

static void
SYSCALL_FUNC(exit)
{
  CHECK_ARGS(1);
  /* TODO: return status code
  int status = *(int *) arg0;
  */

  thread_exit ();
}

static void
SYSCALL_FUNC(write)
{
  CHECK_ARGS(3);
  int fd          = *(int *) arg0;
  const void *buf = *(void **) arg1;
  size_t count    = *(size_t *) arg2;

  /* verify entire user provided buffer is valid */
  check_user_mem ((void *) buf, count);

  /* sanity check fd */
  if (fd < 0 || fd > MAX_FD) {
    f->eax = -1;
    return;
  } else if (fd == 1 || fd == 2) {
    putbuf (buf, count);
    f->eax = count;
    return;
  }

  /* TODO: implement write for other fds */
  f->eax = -1;
  return;
}

/* used to generate switch statement code */
#define SYSCALL_CALL(name)                       \
  syscall_handler_ ## name (arg0, arg1, arg2, f); \
  break;

static void
syscall_handler (struct intr_frame *f) 
{
  int *call = &((int *) f->esp)[0];
  void *arg0 = &((void **) f->esp)[1];
  void *arg1 = &((void **) f->esp)[2];
  void *arg2 = &((void **) f->esp)[3];

  check_user_mem(call, sizeof (call));

  /* look up vector number in table */
  switch (*call) {
  case SYS_HALT:  SYSCALL_CALL(halt);
  case SYS_EXIT:  SYSCALL_CALL(exit);
  case SYS_WRITE: SYSCALL_CALL(write);
  }
}

