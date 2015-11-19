#include "filesys/cache.h"
#include "filesys/filesys.h"
#include "threads/malloc.h"
#include "lib/string.h"

#define CACHE_MAX 64

static struct list cache_list;
static struct lock cache_lock;
static int cache_size;

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
    lock_acquire (&d->lock);
    // TODO: condition variables with readers/writers
    lock_release (&d->lock);
    return d;
  }

  if (cache_size < CACHE_MAX) {
    d = (cache_desc_t *) malloc(sizeof(cache_desc_t));
  } else {
    d = cache_evict ();
  }

  ASSERT (d != NULL);

  lock_init (&d->lock);
  lock_acquire (&d->lock);

  d->dirty = false;
  d->accessed = false;
  d->sector_idx = sector_idx;
  d->blk = malloc (BLOCK_SECTOR_SIZE);

  list_push_back (&cache_list, &d->elem);
  cache_size++;
  lock_release (&d->lock);

  return d;
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
  lock_acquire(&d->lock);
  if (!d->accessed) {
    block_read(fs_device, d->sector_idx, d->blk);
  }
  d->accessed = true;
  lock_release(&d->lock);
  return d->blk;
}

void *
cache_zero (cache_desc_t *d)
{
  lock_acquire(&d->lock);
  memset(d->blk, 0, BLOCK_SECTOR_SIZE);
  d->accessed = true;
  lock_release(&d->lock);
  return d->blk;
}

void
cache_mark_dirty (cache_desc_t *d)
{

}
