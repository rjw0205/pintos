#ifndef VM_SWAP_H
#define VM_SWAP_H

void swap_table_init(void);
size_t swap_out(void *kpage);
void swap_in(size_t swap_index, void *kpage);

#endif