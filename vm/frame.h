#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>
#include "threads/palloc.h"

typedef struct frame {
  void *upage;
  void *kpage;
  struct thread *thread;
  struct list_elem list_elem;
} frame_t;

void falloc_init (void);
void *falloc_get_page (void *, enum palloc_flags);
void *falloc_evict (enum palloc_flags);
void falloc_free_page (void *);

void falloc_acquire (void);
void falloc_release (void);

#endif /* vm/frame.h */
