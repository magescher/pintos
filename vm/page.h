#ifndef VM_PAGE_H
#define VM_PAGE_H

bool is_stack_push (void *esp, void *addr);
bool add_stack_page (uint32_t *pd, void *addr);

#endif /* vm/page.h */
