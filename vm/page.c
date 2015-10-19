#include <inttypes.h>
#include <string.h>
#include "vm/page.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"

unsigned
spage_hash (const struct hash_elem *p, void *t UNUSED)
{
  const spage_t *e = hash_entry (p, spage_t, hash_elem);
  return (unsigned) (e->uaddr);
}

void
spage_destroy (struct hash_elem *p, void *t UNUSED)
{
  const spage_t *e = hash_entry (p, spage_t, hash_elem);
  free ((void *) e);
}

bool
spage_less (const struct hash_elem *a, const struct hash_elem *b, void *t UNUSED)
{
  spage_t *x = hash_entry (a, spage_t, hash_elem);
  spage_t *y = hash_entry (b, spage_t, hash_elem);
  return (x->uaddr < y->uaddr);
}

void
spage_init (struct thread *t)
{
  hash_init (&t->spage_table, spage_hash, spage_less, t);
}

void
spage_free (struct hash *h)
{
  hash_destroy (h, spage_destroy);
}

void
spage_create_file (void *uaddr, struct file *file_ptr, off_t file_off,
                   uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{
  struct thread *t = thread_current ();
  spage_t *spage = (spage_t *) calloc (1, sizeof(spage_t));
  spage->uaddr = uaddr;
  spage->loaded = false;
  spage->writable = writable;
  spage->type = FILE;
  spage->file_ptr = file_ptr;
  spage->file_off = file_off;
  spage->file_read_bytes = read_bytes;
  spage->file_zero_bytes = zero_bytes;

  hash_insert (&t->spage_table, &spage->hash_elem);
}

void
spage_create_swap (void *uaddr, size_t swap_off)
{
  struct thread *t = thread_current ();
  spage_t *spage = (spage_t *) calloc (1, sizeof(spage_t));
  spage->uaddr = uaddr;
  spage->swap_off = swap_off;
  spage->type = SWAP;

  hash_insert (&t->spage_table, &spage->hash_elem);
}

bool
spage_load (spage_t *sp)
{
  struct thread *t = thread_current ();
  enum palloc_flags flags = PAL_USER;
  if (sp->file_zero_bytes > 0) {
    flags |= PAL_ZERO;
  }

  void *kpage = falloc_get_page (sp->uaddr, flags);
  if (!pagedir_set_page (t->pagedir, sp->uaddr, kpage, sp->writable)) {
      falloc_free_page (kpage);
      return false;
  }

  switch (sp->type) {
  case FILE:
    /* Load a page directly from a file. */
    file_seek(sp->file_ptr, sp->file_off);
    off_t bytes = file_read (sp->file_ptr, kpage, sp->file_read_bytes);
    if (bytes != (off_t) sp->file_read_bytes) {
        falloc_free_page (kpage);
        return false;
    }
    memset (kpage + sp->file_read_bytes, 0, sp->file_zero_bytes);
    sp->loaded = true;
    break;
  case SWAP:
    /* Load a page from swap. */
    swap_read (sp, kpage);
    hash_delete (&t->spage_table, &sp->hash_elem);
    break;
  default:
    /* There are no other trusted values for sp->type. */
    ASSERT(false);
    break;
  }

  return true;
}

/* Set maximum stack to 8MB. */
#define STACK_SIZE (8 * 1024 * 1024)

bool
spage_grow_stack (void *esp, void *addr)
{
  if (!is_user_vaddr(addr) || esp - 32 > addr) {
    return false;
  }

  if (esp >= PHYS_BASE - STACK_SIZE) {
    return false;
  }

  struct thread *t = thread_current ();
  void *uaddr = pg_round_down (addr);

  if (pagedir_get_page (t->pagedir, uaddr) != NULL) {
    return false;
  }

  void *kpage = falloc_get_page (uaddr, PAL_USER | PAL_ZERO);
  ASSERT(kpage != NULL);

  lock_acquire (&t->lock_pd);
  bool success = pagedir_set_page (t->pagedir, uaddr, kpage, true);
  lock_release (&t->lock_pd);

  if (!success) {
    falloc_free_page (kpage);
  }
  return success;
}

spage_t *
spage_get (struct hash *t, void *addr)
{
  spage_t lookup;
  lookup.uaddr = pg_round_down(addr);
  struct hash_elem *e = hash_find (t, &lookup.hash_elem);
  return e ? hash_entry (e, spage_t, hash_elem) : NULL;
}

