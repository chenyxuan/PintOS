//
// Created by sjtu-ypm on 19-4-8.
//

#ifndef MYPINTOS_FRAME_H
#define MYPINTOS_FRAME_H

#include "../lib/stdbool.h"
#include "../threads/palloc.h"

struct frame_item{
    void *frame;
    void *upage;
    struct thread* t;
    bool pinned;
    struct hash_elem hash_elem;
    struct list_elem list_elem;
};

void *frame_lookup(void *frame);

//init frame_table
//used in thread/init.c
void  frame_init();

//get a frame from user pool, which must be mapped from upage
//in other words, in page_table, upage->frame_get_frame(flag, upage)
//flag is used by palloc_get_page
void* frame_get_frame(enum palloc_flags flag, void *upage);

//free a frame that got from frame_get_frame
void  frame_free_frame(void *frame);

//get pinned accord to frame
//if a page is pinned, it won't be swaped to the disk
bool  frame_get_pinned(void* frame);

// set frame pinned to new_value
// return whether the set_pinned success
bool frame_set_pinned_false(void* frame);


#endif //MYPINTOS_FRAME_H
