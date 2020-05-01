#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

/* Implementation by ymt Started */
struct mmap_handler* syscall_get_mmap_handle(mapid_t mapid);
bool delete_mmap_handle(struct mmap_handler *mh);
/* Implementation by ymt Ended */

#endif /* userprog/process.h */
