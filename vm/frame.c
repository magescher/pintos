#include "vm/frame.h"

void *
falloc_get_frame (enum palloc_flags flags)
{
  /* TODO: consider locking around this */
  void *page = palloc_get_page (flags);

  if (page != NULL) {
    /* TODO: add frame to frame table */
  } else {
    /* TODO: page = evict_page () */
  }

  return page;
}
