#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <inttypes.h>
#include <hash.h>
#include "filesys/file.h"
#include "threads/thread.h"

typedef enum spage_type {
  /* TODO: come up with types */
  FILE,
  MMAP,
  SWAP
} spage_type_t;

typedef struct spage {
  void *uaddr;                 /* User address ptr. */
  spage_type_t type;           /* TODO: Type of page, can be: swap, or mmap'd. */
  bool loaded;                 /* Allows for lazy loading. */
  bool writable;               /* Is page writable. */
  bool dirty;                  /* Has the page been modified? */
                               /* TODO: what other addr info do we need? */

  struct file *file_ptr;       /* File ptr for file on disk, only valid if type == MMAP. */
  off_t file_off;              /* Offset into the file. */
  size_t file_read_bytes;      /* Bytes to be read. */
  size_t file_zero_bytes;      /* Bytes to be zero'd. */

  size_t swap_off;             /* Offset into the swap table. */
  bool swap_write;             /* Does the swap slot have write permission. */

  struct hash_elem hash_elem;  /* Used for managing the hash table. */
} spage_t;

void spage_init (struct thread *);
void spage_free (struct hash *);
void spage_create_file (void *, struct file *, off_t, uint32_t, uint32_t, bool);
void spage_create_swap (void *, size_t);
bool spage_load (spage_t *);
bool spage_grow_stack (void *, void *);
spage_t *spage_get (struct hash *, void *);

unsigned spage_hash (const struct hash_elem *, void *);
void spage_destroy (struct hash_elem *, void *);
bool spage_less (const struct hash_elem *a, const struct hash_elem *b, void *t);

#endif /* vm/page.h */
