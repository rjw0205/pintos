#include "vm/swap.h"
#include "lib/kernel/bitmap.h"
#include "devices/block.h"
#include "threads/vaddr.h"

struct block *swap_block;
struct bitmap *swap_bitmap; // false means such swap-index is available, true means not available

size_t how_many_pages_in_swap_block;
size_t how_many_sectors_in_page = PGSIZE / BLOCK_SECTOR_SIZE;

void swap_table_init(){
  swap_block = block_get_role(BLOCK_SWAP);
  how_many_pages_in_swap_block = block_size(swap_block) / how_many_sectors_in_page;
  swap_bitmap = bitmap_create(how_many_pages_in_swap_block);
}

size_t swap_out(void *kpage){
  size_t available_swap_index = bitmap_scan(swap_bitmap, 0, 1, false);
  size_t t;
  for(t=0; t<how_many_sectors_in_page; t++){
    block_write(swap_block, available_swap_index * how_many_sectors_in_page + t, kpage + BLOCK_SECTOR_SIZE * t);
  }
  bitmap_set(swap_bitmap, available_swap_index, true);
  return available_swap_index;
}

void swap_in(size_t swap_index, void *kpage){
  size_t t;
  for(t=0; t<how_many_sectors_in_page; t++){
    block_read(swap_block, swap_index * how_many_sectors_in_page + t, kpage + BLOCK_SECTOR_SIZE * t);
  }
  bitmap_set(swap_bitmap, swap_index, false);
}
