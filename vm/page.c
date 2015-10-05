#include <stdint.h>
#include <stdbool.h>
#include "vm/page.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"

bool
add_stack_page (uint32_t *pd, void *addr)
{
  bool success;
  void *kpage = palloc_get_page (PAL_USER);
  if (kpage == NULL) {
    return false;
  }

  void *uaddr = pg_round_down (addr);
  success = pagedir_set_page (pd, uaddr, kpage, true);
  if (!success) {
    return false;
  }
  return true;
}

bool 
is_stack_push (void *esp, void *addr)
{
  if (esp > addr) {
    return (esp - addr) <= 32;
  } else {
    return (addr - esp) <= 32;
  }
}

