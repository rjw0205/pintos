#include "lib/kernel/hash.h"
#include "vm/page.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "threads/malloc.h"

struct lock page_lock;

unsigned spt_hash_func(struct hash_elem *hash_elem, void *aux UNUSED){
  struct supplement_page_table_entry *spte = hash_entry(hash_elem, struct supplement_page_table_entry, elem);
  return hash_int(spte->upage);
}

bool spt_less_func(struct hash_elem *a_, struct hash_elem *b_, void *aux UNUSED){
  struct supplement_page_table_entry *a = hash_entry(a_, struct supplement_page_table_entry, elem);
  struct supplement_page_table_entry *b = hash_entry(b_, struct supplement_page_table_entry, elem);

  return a->upage < b->upage;
}

void spt_action_function(struct hash_elem * elem, void *aux UNUSED){
  struct supplement_page_table_entry * spte = hash_entry(elem, struct supplement_page_table_entry, elem);
  free(spte);
}

void supplement_page_table_init(void){
  hash_init(&thread_current()->supplement_page_table,spt_hash_func,spt_less_func, NULL);
  lock_init(&page_lock);
}

void supplement_page_table_destroy(void){
  hash_destroy(&thread_current()->supplement_page_table, spt_action_function);
}

bool supplement_page_table_insert(void *upage, int status ,struct file * file, int ofs, int read_bytes, int zero_bytes, bool writeable){
  struct supplement_page_table_entry *spte = (struct supplement_page_table_entry *) malloc(sizeof(struct supplement_page_table_entry));
  if (spte == NULL){
    return false;
  }
  //spte->status = ON_FRAME;
  spte->upage = upage;
  spte->status = status;
  spte->file = file;
  spte->ofs = ofs;
  spte->read_bytes = read_bytes;
  spte->zero_bytes = zero_bytes;
  spte->on_frame = false;
  spte->writeable = writeable;

  struct hash_elem *elem;
  lock_acquire(&page_lock);
  elem = hash_insert(&thread_current()->supplement_page_table, &spte->elem);
  lock_release(&page_lock);
  if(elem != NULL){
    free(spte);
    return false;
  }
  return true;
}

bool load_from_supplement_page_table(void *upage){
  struct supplement_page_table_entry spte;
  spte.upage = pg_round_down(upage);
  struct hash_elem *elem = hash_find(&thread_current()->supplement_page_table, &spte.elem);
  if (elem == NULL)
    return false;
  struct supplement_page_table_entry * spte2;
  spte2 = hash_entry (elem, struct supplement_page_table_entry, elem);
  if (spte2->status == 1){ //file
    /* Get a page of memory. */
    uint8_t *kpage = frame_alloc(PAL_USER);
    if (kpage == NULL)
      return false;

    /* Load this page. */
    if (file_read_at(spte2->file, kpage, spte2->read_bytes, spte2->ofs) != (int) spte2->read_bytes)
      {
        frame_table_entry_free (kpage);
        return false; 
      }
    memset (kpage + spte2->read_bytes, 0, spte2->zero_bytes);

    /* Add the page to the process's address space. */
    if (!install_page (spte2->upage, kpage, spte2->writeable)) 
      {
        frame_table_entry_free (kpage);
        return false; 
      }
    spte2->on_frame = true;
    return true;
  }
  else if (spte2->status == 2){
    //swap
    return false;
  }
  else if (spte2->status == 3){
    //zeros 
    return false;
  }
  return false;
}