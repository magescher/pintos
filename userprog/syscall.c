#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/init.h"

static void syscall_handler (struct intr_frame *);

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
syscall_handler_halt (void)
{
  shutdown_power_off ();
}

static void
syscall_handler_exit (void *arg0, struct intr_frame *f UNUSED)
{
  /* check that all arguments are valid */
  check_user_mem (arg0, sizeof (arg0));

  /* TODO: find out how to return status */
  thread_exit ();
}

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
  case SYS_HALT:
    syscall_handler_halt ();
  case SYS_EXIT:
    syscall_handler_exit (arg0, f);
    break;
  }
}

