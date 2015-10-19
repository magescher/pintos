#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

/* Map region identifier. */
typedef int mapid_t;
#define MAP_FAILED ((mapid_t) -1)

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

typedef struct mapping {
  void *uaddr;
  size_t pg_cnt;
  mapid_t mid;

  struct hash_elem hash_elem;
} mapping_t;

void mmap_init (struct thread *);
mapping_t *mmap_get (mapid_t);
bool mmap_is_mapped (mapid_t);
mapid_t mmap_create (void *, struct file *, size_t);
void mmap_destroy (mapid_t);
void mmap_free (struct hash *);

unsigned mapping_hash (const struct hash_elem *, void *);
void mapping_destroy (struct hash_elem *, void *);
bool mapping_less (const struct hash_elem *a, const struct hash_elem *b, void *t);

#endif /* userprog/process.h */
