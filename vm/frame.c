#include "vm/frame.h"
#include <list.h>
#include "userprog/pagedir.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "vm/page.h"

static struct list frame_table;
static struct lock falloc_lock;

void
falloc_init ()
{
  list_init (&frame_table);
  lock_init (&falloc_lock);
}

static void
frame_create (void *upage, void *kpage)
{
  frame_t *frame = (frame_t *) calloc (1, sizeof(frame_t));
  frame->thread = thread_current ();
  frame->upage = upage;
  frame->kpage = kpage;
  list_push_back (&frame_table, &frame->list_elem);
}

static void
frame_destroy (struct list_elem *e)
{
  frame_t *frame = list_entry (e, frame_t, list_elem);

  list_remove (e);
  palloc_free_page (frame->upage);
  free (frame);
}

void *
falloc_get_page (void *upage, enum palloc_flags flags)
{
  void *kpage = palloc_get_page (PAL_USER | flags);

  if (kpage == NULL) {
    kpage = falloc_evict (flags);
  }

  ASSERT(kpage != NULL);
  frame_create (upage, kpage);

  return kpage;
}

void *
falloc_evict (enum palloc_flags flags)
{
  struct list_elem *elem, *begin, *end;
  frame_t *frame;
  spage_t *spage;
  uint32_t *pd;

  begin = list_begin (&frame_table);
  end   = list_end (&frame_table);
  elem  = begin;

  /* Continue looping until eviction. */
  while (true) {
    frame = list_entry (elem, frame_t, list_elem);
    pd = frame->thread->pagedir;

    if (pagedir_is_accessed (pd, frame->upage)) {
      /* set A bit to 0 on first sweep. */
      pagedir_set_accessed (pd, frame->upage, false);
    } else {
      spage = spage_get (&frame->thread->spage_table, frame->upage);
      ASSERT (spage != NULL);

      if (pagedir_is_dirty (pd, frame->upage)) {
        /* write page to swap! */
        ASSERT(false);
      }
      spage->loaded = false;

      pagedir_clear_page (pd, frame->upage);
      frame_destroy (elem);

      return palloc_get_page (PAL_USER | flags);
    }

    elem = list_next (elem);
    if (elem == end) {
      elem = begin;
    }
  }
}

void
falloc_free_page (void *kpage)
{
  struct list_elem *elem;
  frame_t *frame;

  if (list_empty (&frame_table))
    return;

  for (elem = list_begin (&frame_table);
       elem!= list_end (&frame_table);
       elem = list_next (elem))
  {
    frame = list_entry (elem, frame_t, list_elem);
    if (frame->kpage == kpage) {
      frame_destroy (elem);
      break;
    }
  }
}

void
falloc_acquire ()
{
  lock_acquire (&falloc_lock);
}

void
falloc_release ()
{
  lock_release (&falloc_lock);
}

