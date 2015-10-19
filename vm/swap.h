#ifndef VM_SWAP_H
#define VM_SWAP_H

#include "vm/page.h"

void swap_init (void);
size_t swap_write (void *);
void swap_read (spage_t *, void *);
void swap_destroy (off_t swap_off);

#endif /* vm/swap.h */
