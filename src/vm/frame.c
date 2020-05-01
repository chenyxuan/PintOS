//
// Created by sjtu-ypm on 19-4-8.
//

#include <devices/block.h>
#include <threads/vaddr.h>
#include <stdio.h>
#include <userprog/syscall.h>
#include "string.h"
#include "frame.h"
#include "swap.h"
#include "../threads/thread.h"
#include "../userprog/pagedir.h"
#include "../threads/malloc.h"
#include "../lib/debug.h"
#include "../lib/stddef.h"
#include "../lib/kernel/hash.h"
#include "page.h"




static struct hash frame_table;
static struct list frame_clock_list;
static struct lock all_lock;
//static struct lock frame_lock, frame_clock_lock;
struct frame_item* current_frame;


void* frame_get_used_frame(void *upage);
void frame_current_clock_to_next();
void frame_current_clock_to_prev();


static bool frame_hash_less(const struct hash_elem *a,
                     const struct hash_elem *b,
                     void *aux UNUSED);
static unsigned frame_hash(const struct hash_elem *e,
                    void* aux UNUSED);


void frame_init(){
  hash_init(&frame_table, frame_hash, frame_hash_less, NULL);
  list_init(&frame_clock_list);
//  lock_init(&frame_clock_lock);
//  lock_init(&frame_lock);
  lock_init(&all_lock);
  current_frame = NULL;
}

void *frame_get_frame(enum palloc_flags flag, void *upage) {
//  ASSERT(thread == thread_current());
//  printf("%d, lock0\n", thread_current()->tid);
  lock_acquire(&all_lock);

  ASSERT (pg_ofs (upage) == 0);
  ASSERT (is_user_vaddr (upage));

  void *frame = palloc_get_page(PAL_USER | flag);


  if (frame == NULL){
    frame = frame_get_used_frame(upage);
    if (flag & PAL_ZERO)
      memset (frame, 0, PGSIZE);
    if (flag & PAL_ASSERT)
      PANIC ("frame_get: out of pages");
  }

  if (frame == NULL){
    lock_release(&all_lock);
    return NULL;
  }

  ASSERT(pg_ofs(frame) == 0);
  struct frame_item* tmp = (struct frame_item*)malloc(sizeof (struct frame_item));
  tmp->frame = frame;
  tmp->upage = upage;
  tmp->t = thread_current();
  tmp->pinned = true;
//  lock_acquire(&frame_lock);
  hash_insert(&frame_table, &tmp->hash_elem);
//  lock_release(&frame_lock);

  lock_release(&all_lock);
//  printf("unlock0\n");

//  frame_set_pinned_false(frame);
//  printf("get_frame:%p\n", frame);
  return frame;
}

void frame_free_frame(void *frame){
//  printf("%d, lock1\n", thread_current()->tid);
  lock_acquire(&all_lock);

  struct frame_item* t = frame_lookup(frame);

  if (t == NULL)
    PANIC("try_free_a frame_that_not_exist!!");
  if (!t->pinned){
//    lock_acquire(&frame_clock_lock);
    if (current_frame == t){
      if (list_empty(&frame_clock_list))
        current_frame = NULL;
      else
        frame_current_clock_to_next();
    }
    list_remove(&t->list_elem);
//    lock_release(&frame_clock_lock);
  }
//  printf("haha\n");

//  lock_acquire(&frame_lock);
  hash_delete(&frame_table, &t->hash_elem);
//  lock_release(&frame_lock);
  free(t);
//  printf("free:%p\n", frame);
  palloc_free_page(frame);

  lock_release(&all_lock);
//  printf("unlock1\n");
}

bool frame_get_pinned(void* frame){
  struct frame_item* t = frame_lookup(frame);
  if (t == NULL)
    PANIC("try_set_pinned_of_a frame_that_not_exist!!");
  return t->pinned;
}

bool frame_set_pinned_false(void* frame){
//  printf("lock2\n");
  lock_acquire(&all_lock);


  struct frame_item* t = frame_lookup(frame);
  if (t == NULL){
    lock_release(&all_lock);
//    printf("unlock2\n");
    return false;
  }

  if (t->pinned == false){
    lock_release(&all_lock);
//    printf("unlock2\n");
    return true;
  }

  t->pinned = false;
//  lock_acquire(&frame_clock_lock);
  list_push_back(&frame_clock_list, &t->list_elem);
  if (list_size(&frame_clock_list) == 1)
    current_frame = t;
//  lock_release(&frame_clock_lock);

  lock_release(&all_lock);
//  printf("unlock2\n");
  return true;
}

void* frame_get_used_frame(void *upage){
  ASSERT(current_frame != NULL);
//  lock_acquire(&frame_clock_lock);

/*
  struct page_table_elem *e = page_find(current_frame->t->page_table, current_frame->upage);
  ASSERT( e != NULL && e->status == FRAME);
*/  
  while(pagedir_is_accessed(current_frame->t->pagedir, current_frame->upage)){
    pagedir_set_accessed(current_frame->t->pagedir, current_frame->upage, false);
    frame_current_clock_to_next();
    ASSERT( current_frame != NULL );
  }
  struct frame_item* t = current_frame;
  void* tmp_frame = t->frame;
//  printf("swap_free:%p, %p\n", current_frame->upage, current_frame->frame);
  index_t index = (index_t)-1;
//  struct page_table_elem* tmp = page_find(current_frame->t->page_table, current_frame->upage);
//  struct thread* cur = thread_current();
  ASSERT(page_find(current_frame->t->page_table, current_frame->upage) != NULL);
  struct page_table_elem *e = page_find(current_frame->t->page_table, current_frame->upage); 
  if (e == NULL || e->origin == NULL || ((struct mmap_handler *)(e->origin))->is_static_data){
    index = swap_store(current_frame->frame);
    if (index == -1)
      return NULL;
    ASSERT(page_status_eviction(current_frame->t, current_frame->upage, index, true));
  }
  else{
    mmap_write_file(e->origin, current_frame->upage, tmp_frame);
    ASSERT(page_status_eviction(current_frame->t, current_frame->upage, index, false));
  }

  list_remove(&t->list_elem);
  if (list_empty(&frame_clock_list))
    current_frame = NULL;
  else
    frame_current_clock_to_next();
//  pagedir_clear_page(t->t->pagedir, t->upage);
//  lock_acquire(&frame_lock);
  hash_delete(&frame_table, &t->hash_elem);
//  lock_release(&frame_lock);
  free(t);
  return tmp_frame;
}

void frame_current_clock_to_next(){
  ASSERT(current_frame != NULL);
  if (list_size(&frame_clock_list) == 1)
    return;;
  if (&current_frame->list_elem == list_back(&frame_clock_list))
    current_frame = list_entry(list_head(&frame_clock_list),
                               struct frame_item, list_elem);

  current_frame = list_entry(list_next(&current_frame->list_elem),
                             struct frame_item, list_elem);
}

void frame_current_clock_to_prev(){
  ASSERT(current_frame != NULL);
  if (list_size(&frame_clock_list) == 1)
    return;

  if (&current_frame->list_elem == list_front(&frame_clock_list))
    current_frame = list_entry(list_tail(&frame_clock_list),
                               struct frame_item, list_elem);

  current_frame = list_entry(list_prev(&current_frame->list_elem),
                             struct frame_item, list_elem);
}


void *frame_lookup(void *frame){
  struct frame_item p;
  struct hash_elem * e;
  p.frame = frame;
//  lock_acquire(&frame_lock);
  e = hash_find(&frame_table, &p.hash_elem);
//  lock_release(&frame_lock);
  return e == NULL? NULL : hash_entry(e, struct frame_item, hash_elem);
}

static bool frame_hash_less(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED){
  const struct frame_item * ta = hash_entry(a, struct frame_item, hash_elem);
  const struct frame_item * tb = hash_entry(b, struct frame_item, hash_elem);
  return ta->frame < tb->frame;
}

static unsigned frame_hash(const struct hash_elem *e, void* aux UNUSED){
  struct frame_item* t = hash_entry(e, struct frame_item, hash_elem);
  return hash_bytes(&t->frame, sizeof(t->frame));
}
