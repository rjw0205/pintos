#include "lib/kernel/list.h"
#include "vm/frame.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "threads/malloc.h"

static struct list frame_table;
static struct lock frame_lock;

void frame_table_init(void) {
  list_init(&frame_table);
  lock_init(&frame_lock);
}

void * frame_alloc(enum palloc_flags flags) {
  void * page = palloc_get_page(flags);
  if (page == NULL) {
    // page allocation failed. Need to swap frame to allocate memory to such page
    return NULL;
    //frame_table_entry_evict();
  }
  frame_table_entry_insert(page);
  return page;
}

void frame_table_entry_insert(void * page){
  struct frame_table_entry *frame_table_entry = malloc(sizeof(struct frame_table_entry));
  frame_table_entry->kernel_virtual_address = page;
  frame_table_entry->physical_address = vtop(page);
  
  lock_acquire(&frame_lock);
  list_push_front(&frame_table, &frame_table_entry->elem);
  lock_release(&frame_lock);
  return;
}

void frame_table_entry_free(void * page){
  struct list_elem *e;

  for (e = list_begin (&frame_table); e != list_end (&frame_table);
       e = list_next (e))
    {
      struct frame_table_entry * fte  = list_entry (e, struct frame_table_entry, elem);
      if(fte->kernel_virtual_address == page){
          lock_acquire(&frame_lock);
          list_remove(&fte->elem);
          palloc_free_page(fte->kernel_virtual_address);
          free(fte);
          lock_release(&frame_lock);
          return;
      }
    }
    PANIC("dd");
}

void frame_table_entry_evict(void){
  PANIC("??");
  return;
}