//
// Created by wangnick on 19/04/23.
//

#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include "devices/block.h"

void cache_init (void);
void cache_read (block_sector_t sector, void *buffer);
void cache_write (block_sector_t sector, const void *buffer);
void cache_done (void);

#endif /* filesys/cache.h */
