#include "vm/swap.h"
#include <bitmap.h>
#include "threads/vaddr.h"
#include "devices/block.h"

static struct block *swap_dev;
static struct bitmap *swap_map;
static struct lock swap_lock;

#define SECTORS_IN_PAGE (PGSIZE / BLOCK_SECTOR_SIZE)

void swap_init (void)
{
  swap_dev = block_get_role (BLOCK_SWAP);
  ASSERT(swap_dev != NULL);

  block_sector_t size = block_size (swap_dev);
  swap_map = bitmap_create (size * BLOCK_SECTOR_SIZE / PGSIZE);
  ASSERT(swap_map != NULL);

  lock_init (&swap_lock);
}

size_t swap_write (void *kpage)
{
  lock_acquire (&swap_lock);

  /* Find an empty swap bit. */
  size_t swap_off = bitmap_scan_and_flip (swap_map, 0, 1, false);
  if (swap_off == BITMAP_ERROR) {
    goto done;
  }

  /* Write kpage to device, one sector at a time. */
  size_t off;
  block_sector_t sector = swap_off * SECTORS_IN_PAGE;
  for (off = 0; off < SECTORS_IN_PAGE; off++) {
    void *buf = kpage + off * BLOCK_SECTOR_SIZE;
    block_write (swap_dev, sector + off, buf);
  }

done:
  lock_release (&swap_lock);
  return swap_off;
}

void swap_read (spage_t *sp, void *kpage)
{
  lock_acquire (&swap_lock);

  /* Read kpage from device, one sector at a time. */
  size_t off;
  block_sector_t sector = sp->swap_off * SECTORS_IN_PAGE;
  for (off = 0; off < SECTORS_IN_PAGE; off++) {
    void *buf = kpage + off * BLOCK_SECTOR_SIZE;
    block_read (swap_dev, sector + off, buf);
  }

  /* Mark slot as available. */
  swap_destroy (sp->swap_off);

  lock_release (&swap_lock);
}

void swap_destroy (off_t swap_off)
{
  ASSERT (bitmap_test (swap_map, swap_off));
  bitmap_set (swap_map, swap_off, false);
}

