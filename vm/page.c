#include "vm/page.h"


bool 
is_stack_push (void *esp, void *addr) {

	uint32_t addr = (uint32_t) addr;
	uint32_t stack = (uint32_t) esp;

	// Check to see if it could be a possible stack access
	return((stack - addr) <= 32 ); 

}

