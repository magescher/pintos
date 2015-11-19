#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include <stdbool.h>
#include "lib/kernel/list.h"
#include "filesys/off_t.h"
#include "devices/block.h"

typedef struct cache_desc {
  block_sector_t sector_idx;
  bool dirty;
  bool accessed;

  struct lock *lock;
  struct condition *cond;

  struct inode *inode;
  void *blk;

  struct list_elem elem;
} cache_desc_t;

// initialize cache data structures
void cache_init (void);

// reserve a block in buffer cache dedicated to hold this sector
// possibly evicting some other unused buffer
// either grant exclusive or shared access
struct cache_desc *cache_get_desc (block_sector_t sector_idx, bool exclusive);

// release access to cache block
struct cache_desc *cache_get (block_sector_t sector_idx);

// release access to cache block
void cache_put (cache_desc_t *d);

// read cache block from disk, returns pointer to data
void *cache_read (cache_desc_t *d);

// fill cache block with zeros, returns pointer to data
void *cache_zero (cache_desc_t *d);

// mark cache block dirty (must be written back)
void cache_mark_dirty (cache_desc_t *d);

#endif /* filesys/cache.h */
