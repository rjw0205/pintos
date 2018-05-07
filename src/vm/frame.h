#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "lib/kernel/list.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"

struct frame_table_entry{
	void *kernel_virtual_address;
  	void *physical_address;
  	struct list_elem elem;
};

void frame_table_init(void);
void * frame_alloc(enum palloc_flags flags);
void frame_table_entry_insert(void * page);
void frame_table_entry_free(void * page);
void frame_table_entry_evict(void);

#endif

