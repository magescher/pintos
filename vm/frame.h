#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "threads/palloc.h"

void *falloc_get_frame (enum palloc_flags flags);

#endif /* vm/frame.h */
