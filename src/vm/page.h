#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "lib/kernel/hash.h"

struct supplement_page_table_entry {
  struct hash_elem elem;
  void *upage;
  int status;
  int on_frame;
  struct file * file;
  int ofs;
  int read_bytes;
  int zero_bytes;
  int writeable;
};

unsigned spt_hash_func(struct hash_elem *hash_elem, void *aux);
bool spt_less_func(struct hash_elem *a_, struct hash_elem *b_, void *aux);
void spt_action_function(struct hash_elem * elem, void *aux);
void supplement_page_table_init(void);
void supplement_page_table_destroy(void);
bool supplement_page_table_insert(void *upage, int status, struct file * file, int ofs, int read_bytes, int zero_bytes, bool writeable);
bool load_from_supplement_page_table(void *upage);
#endif