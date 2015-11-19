#include "filesys/cache.h"
#include "threads/synch.h"

static struct list cache_list;
static struct lock cache_lock;

void
cache_init (void)
{
  list_init (&cache_list);
  lock_init (&cache_lock);
}

cache_desc_t *
cache_get_desc (block_sector_t sector_idx, bool exclusive)
{
  lock_acquire (&cache_lock);
  cache_desc_t *d = cache_get (sector_idx);
  lock_release (&cache_lock);
  if (d != NULL) {
    // TODO: do something
  } else {
    // TODO: do something else
  }
}

cache_desc_t *
cache_get (block_sector_t sector_idx)
{
  struct list_elem* e;
  for(e  = list_begin (&cache_list);
      e != list_end (&cache_list);
      e  = list_next (e))
  {
    cache_desc_t *d = list_entry (e, cache_desc_t, elem);
    if (d->sector_idx == sector_idx) {
      return d;
    }
  }
  return NULL;
}

void
cache_put (cache_desc_t *d)
{

}

void *
cache_read (cache_desc_t *d)
{

}

void *
cache_zero (cache_desc_t *d)
{

}

void
cache_mark_dirty (cache_desc_t *d)
{

}
