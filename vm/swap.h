//
// Created by sjtu-ypm on 19-4-11.
//

#ifndef MYPINTOS_SWAP_H
#define MYPINTOS_SWAP_H

#include "../devices/block.h"

//identifier type of the swap slot
typedef  block_sector_t index_t;


//initialize swap when kernel starts
//used in thread/init.c
void swap_init();

//store the content of a kpage(frame) to a swap slot(on the disk)
//return an identifier of the swap slot
index_t swap_store(void *kpage);

//load a swap slot to the kpage(frame)
//index must be got from swap_store()
void swap_load(index_t index, void *kpage);

//free a swap slot whose identifier is index
//index must be got from swap_store()
void swap_free(index_t index);


#endif //MYPINTOS_SWAP_H
