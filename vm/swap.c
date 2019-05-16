//
// Created by sjtu-ypm on 19-4-11.
//


#include <lib/debug.h>
#include <threads/pte.h>
#include <threads/malloc.h>
#include "swap.h"
#include "../lib/kernel/hash.h"

const int BLOCK_PER_PAGE = PGSIZE / BLOCK_SECTOR_SIZE;


struct swap_item{
    index_t index;
//    struct hash_elem hash_elem;
    struct list_elem list_elem;
};

//struct hash swap_table;
static struct list swap_free_list;
struct block* swap_block;
index_t top_index = 0;

index_t get_free_swap_slot();
//bool swap_hash_less(const struct hash_elem *a,
//                     const struct hash_elem *b,
//                     void *aux UNUSED);
//unsigned swap_hash(const struct hash_elem *e,
//                    void* aux UNUSED);



void swap_init(){
  swap_block = block_get_role(BLOCK_SWAP);
  ASSERT(swap_block != NULL);
  list_init(&swap_free_list);
}


index_t swap_store(void *kpage){
  ASSERT(is_kernel_vaddr(kpage));
  index_t index = get_free_swap_slot();
  if (index == (index_t)-1)
    return index;
  for (int i = 0; i < BLOCK_PER_PAGE; i++){
    block_write(swap_block, index + i, kpage + i * BLOCK_SECTOR_SIZE);
  }
  return index;
}

void swap_load(index_t index, void *kpage){
  ASSERT(index != (index_t)-1);
  ASSERT(is_kernel_vaddr(kpage));
  ASSERT(index % BLOCK_PER_PAGE == 0);

  for (int i = 0; i < BLOCK_PER_PAGE; i++){
    block_read(swap_block, index + i, kpage + i * BLOCK_SECTOR_SIZE);
  }
  swap_free(index);
}

void swap_free(index_t index){
  ASSERT(index % BLOCK_PER_PAGE == 0);

  if (top_index == index + BLOCK_PER_PAGE)
    top_index = index;
  else{
    struct swap_item* t = malloc(sizeof(struct swap_item));
    t->index = index;
    list_push_back(&swap_free_list, &t->list_elem);
  }
}


index_t get_free_swap_slot(){
  index_t res = (index_t)-1;
  if (list_empty(&swap_free_list)){
    if (top_index + BLOCK_PER_PAGE < block_size(swap_block)){
      res = top_index;
      top_index += BLOCK_PER_PAGE;
    }
  }
  else{
    struct swap_item* t = list_entry(list_front(&swap_free_list), struct swap_item, list_elem);
    list_remove(&t->list_elem);
    res = t->index;
    free(t);
  }
  return res;
}

//
//
//bool frame_hash_less(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED){
//  const struct swap_item * ta = hash_entry(a, struct swap_item, hash_elem);
//  const struct swap_item * tb = hash_entry(b, struct swap_item, hash_elem);
//  if (ta->pd == tb->pd)
//    return ta->upage < tb->upage;
//  return ta->pd < tb->pd;
//}
//
//unsigned frame_hash(const struct hash_elem *e, void* aux UNUSED){
//  struct swap_item* t = hash_entry(e, struct swap_item, hash_elem);
//  return hash_bytes(&t->pd, sizeof(t->pd) + sizeof(t->upage));
//}
