#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <string.h>
#include <hash.h>
#include <user/syscall.h>
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "filesys/directory.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "vm/page.h"

static void syscall_handler (struct intr_frame *);

static struct lock syscall_lock;

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
  check_user_mem (f, arg0, num * sizeof (arg0));

static void
check_user_mem (struct intr_frame *f, void *addr, size_t size)
{
  if (size == 0) {
    return;
  }

  struct thread *t = thread_current ();
  void *uaddr = pg_round_down (addr);
  void *end = addr + size;

  for (; uaddr < end; uaddr += PGSIZE) {
    if (!is_user_vaddr (uaddr)) {
      goto fail;
    }
    spage_t *sp = spage_get (&t->spage_table, uaddr);
    void *pg = pagedir_get_page (t->pagedir, uaddr);
    if (sp == NULL && pg == NULL) {
      if (!spage_grow_stack (f->esp, uaddr)) {
        goto fail;
      }
    }
  }
  return;

fail:
  thread_exit ();
}

static void
check_user_str (struct intr_frame *f, const char *str)
{
  // TODO: consider optimizing this function
  if (str == NULL) {
    thread_exit ();
  }
  do {
    check_user_mem (f, (void *) str, sizeof(char));
    str++;
  } while (*str != 0);
}

static struct fd *
fd_lookup (unsigned fd)
{
  struct thread *t = thread_current ();
  struct fd lookup;
  lookup.fd = fd;
  struct hash_elem *e = hash_find (&t->fds, &lookup.hash_elem);
  return e ? hash_entry (e, struct fd, hash_elem) : NULL;
}

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init (&syscall_lock);
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

  int status = *(int *) arg0;
  struct thread *t = thread_current ();
  t->rc = status;

  thread_exit ();
}

static void
SYSCALL_FUNC(exec)
{
  CHECK_ARGS(1);
  const char *cmd = *(char **) arg0;

  check_user_str (f, cmd);

  lock_acquire (&syscall_lock);
  f->eax = process_execute (cmd);
  lock_release (&syscall_lock);
}

static void
SYSCALL_FUNC(wait)
{
  CHECK_ARGS(1);
  tid_t tid = *(tid_t *) arg0;

  f->eax = process_wait (tid);
}

static void
SYSCALL_FUNC(create)
{
  CHECK_ARGS(2);
  const char *file = *(char **) arg0;
  unsigned size    = *(unsigned *) arg1;

  check_user_str (f, file);

  if (strlen (file) > NAME_MAX) {
    f->eax = false;
    return;
  }

  lock_acquire (&syscall_lock);
  f->eax = filesys_create (file, size, false);
  lock_release (&syscall_lock);
  return;
}

static void
SYSCALL_FUNC(remove)
{
  CHECK_ARGS(1);
  const char *file = *(char **) arg0;

  check_user_str (f, file);

  if (strlen (file) > NAME_MAX) {
    f->eax = false;
    return;
  }

  lock_acquire (&syscall_lock);
  f->eax = filesys_remove (file);
  lock_release (&syscall_lock);
  return;
}

static void
SYSCALL_FUNC(open)
{
  CHECK_ARGS(1);
  const char *file = *(char **) arg0;

  check_user_str (f, file);

  struct fd *fd = calloc (1, sizeof (*fd));
  if (fd == NULL) {
    f->eax = -1;
    return;
  }

  lock_acquire (&syscall_lock);
  fd->file = filesys_open (file);
  lock_release (&syscall_lock);
  if (fd->file == NULL) {
    free (fd);
    f->eax = -1;
    return;
  }

  struct thread *t = thread_current ();

  lock_acquire (&syscall_lock);

  struct hash_iterator i;
  hash_first (&i, &t->fds);
  unsigned maxfd = 0;
  while (hash_next (&i)) {
    struct fd *e = hash_entry (hash_cur (&i), struct fd, hash_elem);
    if (e->fd > maxfd) maxfd = e->fd;
  }

  fd->fd = (maxfd == 0) ? 3 : maxfd + 1;
  hash_insert (&t->fds, &fd->hash_elem);

  lock_release (&syscall_lock);

  f->eax = fd->fd;
  return;
}

static void
SYSCALL_FUNC(filesize)
{
  CHECK_ARGS(1);
  int fd = *(int *) arg0;

  struct fd *file = fd_lookup (fd);

  if (file != NULL) {
    lock_acquire (&syscall_lock);
    f->eax = file_length (file->file);
    lock_release (&syscall_lock);
  } else {
    f->eax = -1;
  }
}

static void
SYSCALL_FUNC(read)
{
  CHECK_ARGS(3);
  int fd        = *(int *) arg0;
  void *buf     = *(void **) arg1;
  unsigned size = *(unsigned *) arg2;

  check_user_mem (f, buf, size);

  if (size == 0) {
    f->eax = 0;
  } else if (fd == 0) {
    *(char *) buf = input_getc ();
    f->eax = 1;
  } else {
    struct fd *file = fd_lookup (fd);

    if (file != NULL) {
      lock_acquire (&syscall_lock);
      f->eax = file_read (file->file, buf, size);
      lock_release (&syscall_lock);
    } else {
      f->eax = -1;
    }
  }
}

static void
SYSCALL_FUNC(write)
{
  CHECK_ARGS(3);
  int fd          = *(int *) arg0;
  const void *buf = *(void **) arg1;
  unsigned size   = *(unsigned *) arg2;

  check_user_mem (f, (void *) buf, size);

  if (fd == 1 || fd == 2) {
    putbuf (buf, size);
    f->eax = size;
  } else {
    struct fd *file = fd_lookup (fd);

    if (file != NULL && !file_isdir (file->file)) {
      lock_acquire (&syscall_lock);
      f->eax = file_write (file->file, buf, size);
      lock_release (&syscall_lock);
    } else {
      f->eax = -1;
    }
  }
}

static void
SYSCALL_FUNC(seek)
{
  CHECK_ARGS(2);
  int fd       = *(int *) arg0;
  unsigned pos = *(unsigned *) arg1;

  struct fd *file = fd_lookup (fd);

  if (file != NULL) {
    lock_acquire (&syscall_lock);
    file_seek (file->file, pos);
    lock_release (&syscall_lock);
  }
}

static void
SYSCALL_FUNC(tell)
{
  CHECK_ARGS(1);
  int fd = *(int *) arg0;

  struct fd *file = fd_lookup(fd);

  if (file != NULL) {
    lock_acquire (&syscall_lock);
    f->eax = file_tell (file->file);
    lock_release (&syscall_lock);
  } else {
    f->eax = -1;
  }
}

static void
SYSCALL_FUNC(close)
{
  CHECK_ARGS(1);
  int fd = *(int *) arg0;

  struct fd *file = fd_lookup (fd);

  if (file != NULL) {
    struct thread *t = thread_current ();

    lock_acquire (&syscall_lock);
    file_close (file->file);
    hash_delete (&t->fds, &file->hash_elem);
    free (file);
    lock_release (&syscall_lock);
  }
}

static void
SYSCALL_FUNC(mmap)
{
  CHECK_ARGS(2);
  int fd     = *(int *) arg0;
  void *addr = *(void **) arg1;

  if (fd == 0 || fd == 1 || addr == 0 ||
      pg_ofs (addr) != 0 || !is_user_vaddr (addr)) {
    goto fail;
  }

  struct fd *file = fd_lookup (fd);
  if (file == NULL) {
    goto fail;
  }

  lock_acquire (&syscall_lock);
  size_t bytes = file_length (file->file);
  lock_release (&syscall_lock);

  if (bytes == 0) {
    goto fail;
  }

  struct thread *t = thread_current ();
  uint32_t *pd = t->pagedir;
  struct hash *sp = &t->spage_table;

  size_t off;
  for (off = 0; off < bytes; off += PGSIZE) {
    if (!is_user_vaddr (addr + off)
        || pagedir_get_page (pd, addr + off)
        || spage_get (sp, addr + off)) {
      goto fail;
    }
  }

  lock_acquire (&syscall_lock);
  struct file *re_file = file_reopen (file->file);
  lock_release (&syscall_lock);

  if (re_file == NULL) {
    goto fail;
  }

  f->eax = mmap_create (addr, re_file, bytes);
  return;

fail:
  f->eax = -1;
}

static void
SYSCALL_FUNC(munmap)
{
  CHECK_ARGS(1);
  mapid_t mapping = *(mapid_t *) arg0;

  mmap_destroy (mapping);
}

static void
SYSCALL_FUNC(chdir)
{
  CHECK_ARGS(1);
  const char *dir = *(const char **) arg0;

  check_user_str (f, dir);

  struct thread *t = thread_current ();
  t->cwd = filesys_chdir (dir);

  f->eax = (t->cwd != NULL);
}


static void
SYSCALL_FUNC(mkdir)
{
  CHECK_ARGS(1);
  const char *dir = *(const char **) arg0;

  check_user_str (f, dir);

  f->eax = filesys_create (dir, 0, true);
}


static void
SYSCALL_FUNC(readdir)
{
  CHECK_ARGS(2);
  int fd     = *(int *) arg0;
  char *name = *(char **) arg1;

  check_user_str (f, name);
  struct fd *file = fd_lookup (fd);

  if (file != NULL) {
    if (file_isdir (file->file)) {
      // TODO: needs implementation
      struct dir *dir = (struct dir *) file->file;
      f->eax = dir_readdir (dir, name);
    } else {
      f->eax = false;
    }
  } else {
    f->eax = false;
  }
}


static void
SYSCALL_FUNC(isdir)
{
  CHECK_ARGS(1);
  int fd = *(int *) arg0;

  struct fd *file = fd_lookup (fd);

  if (file != NULL) {
    f->eax = file_isdir (file->file);
  } else {
    f->eax = false;
  }
}


static void
SYSCALL_FUNC(inumber)
{
  CHECK_ARGS(1);
  int fd = *(int *) arg0;

  struct fd *file = fd_lookup (fd);
  if (file != NULL) {
    f->eax = file_inumber (file->file);
  } else {
    f->eax = -1;
  }
}

/* used to generate switch statement code */
#define SYSCALL_CALL(name)                       \
  syscall_handler_ ## name (arg0, arg1, arg2, f); \
  break;

static void
syscall_handler (struct intr_frame *f) 
{
  int *call  = &((int   *) f->esp)[0];
  void *arg0 = &((void **) f->esp)[1];
  void *arg1 = &((void **) f->esp)[2];
  void *arg2 = &((void **) f->esp)[3];

  check_user_mem (f, call, sizeof (call));

  /* look up vector number in table */
  switch (*call) {
  case SYS_HALT:     SYSCALL_CALL(halt);
  case SYS_EXIT:     SYSCALL_CALL(exit);
  case SYS_EXEC:     SYSCALL_CALL(exec);
  case SYS_WAIT:     SYSCALL_CALL(wait);
  case SYS_CREATE:   SYSCALL_CALL(create);
  case SYS_REMOVE:   SYSCALL_CALL(remove);
  case SYS_OPEN:     SYSCALL_CALL(open);
  case SYS_FILESIZE: SYSCALL_CALL(filesize);
  case SYS_READ:     SYSCALL_CALL(read);
  case SYS_WRITE:    SYSCALL_CALL(write);
  case SYS_SEEK:     SYSCALL_CALL(seek);
  case SYS_TELL:     SYSCALL_CALL(tell);
  case SYS_CLOSE:    SYSCALL_CALL(close);
  case SYS_MMAP:     SYSCALL_CALL(mmap);
  case SYS_MUNMAP:   SYSCALL_CALL(munmap);
  case SYS_CHDIR:    SYSCALL_CALL(chdir);
  case SYS_MKDIR:    SYSCALL_CALL(mkdir);
  case SYS_READDIR:  SYSCALL_CALL(readdir);
  case SYS_ISDIR:    SYSCALL_CALL(isdir);
  case SYS_INUMBER:  SYSCALL_CALL(inumber);
  }
}

