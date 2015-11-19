#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include <stdbool.h>
#include "lib/kernel/list.h"
#include "filesys/off_t.h"
#include "devices/block.h"

struct cache_desc {
  block_sector_t sector_idx;
  bool dirty;
  bool accessed;

  struct condition *cond;

  struct inode *inode;
  uint8_t *blk;

  struct list_elem elem;
} cache_desc_t;

// initialize cache data structures
void cache_init (void);

// reserve a block in buffer cache dedicated to hold this sector
// possibly evicting some other unused buffer
// either grant exclusive or shared access
struct cache_desc *cache_get_desc (block_sector_t sector, bool exclusive);

// release access to cache block
void cache_put(struct cache_desc *d);

// read cache block from disk, returns pointer to data
void *cache_read(struct cache_desc *d);

// fill cache block with zeros, returns pointer to data
void *cache_zero(struct cache_desc *d);

// mark cache block dirty (must be written back)
void cache_mark_dirty(struct cache_desc *d);

#endif /* filesys/cache.h */
