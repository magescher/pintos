#include <inttypes.h>
#include <string.h>
#include "vm/page.h"
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
  spage_t *spage = (spage_t *) calloc (sizeof(spage_t), 1);
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

bool
spage_load (spage_t *sp)
{
  struct thread *t = thread_current ();
  enum palloc_flags flags = PAL_USER;
  void *kpage = NULL;

  switch (sp->type) {
  case FILE:
    /* Get a page of memory. */
    if (sp->file_zero_bytes > 0) {
      flags |= PAL_ZERO;
    }
    kpage = palloc_get_page (flags);

    /* TODO: this won't fail when we switch to falloc. */
    ASSERT(kpage != NULL);

    /* Load this page. */
    file_seek(sp->file_ptr, sp->file_off);
    off_t bytes = file_read (sp->file_ptr, kpage, sp->file_read_bytes);

    if (bytes != sp->file_read_bytes) {
        palloc_free_page (kpage);
        return false;
    }

    memset (kpage + sp->file_read_bytes, 0, sp->file_zero_bytes);

    /* Add the page to the process's address space. */
    lock_acquire (&t->lock_pd);
    if (!pagedir_set_page (t->pagedir, sp->uaddr, kpage, sp->writable)) {
        palloc_free_page (kpage);
        return false;
    }
    lock_release (&t->lock_pd);

    sp->loaded = true;
    break;
  case MMAP:
    /* TODO: do this. */
    ASSERT(false);
    break;
  case SWAP:
    /* TODO: do this. */
    ASSERT(false);
    break;
  default:
    /* There are no other trusted values for sp->type. */
    ASSERT(false);
    break;
  }

  return true;
}

spage_t *
spage_get (struct hash *t, void *uaddr)
{
  spage_t lookup;
  lookup.uaddr = uaddr;
  struct hash_elem *e = hash_find (t, &lookup.hash_elem);
  return e ? hash_entry (e, spage_t, hash_elem) : NULL;
}

