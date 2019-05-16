#include <stdio.h>
#include "userprog/syscall.h"
#include <syscall-nr.h>
#include <devices/input.h>
#include <filesys/filesys.h>
#include <threads/malloc.h>
#include <filesys/file.h>
#include <threads/pte.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "pagedir.h"
#include "devices/shutdown.h"
#include "lib/user/syscall.h"
#include "process.h"
#ifdef VM
#include "vm/page.h"
#endif
#ifdef FILESYS
#include "filesys/directory.h"
#include "filesys/inode.h"
#endif

static void syscall_handler(struct intr_frame *);

/* Implementation by ypm Started */
static void syscall_halt(struct intr_frame *f);
static void syscall_exit(struct intr_frame *f, int return_value);
static void syscall_exec(struct intr_frame *f, const char *cmd_line);
static void syscall_wait(struct intr_frame *f, pid_t pid);
static void syscall_open(struct intr_frame *f, const char *name);
static void syscall_create(struct intr_frame *f, const char *name, unsigned initial_size);
static void syscall_remove(struct intr_frame *f, const char *name);
static void syscall_filesize(struct intr_frame *f, int fd);
static void syscall_read(struct intr_frame *f, int fd, const void *buffer, unsigned size);
static void syscall_write(struct intr_frame *f, int fd, const void *buffer, unsigned size);
static void syscall_seek(struct intr_frame *f, int fd, unsigned position);
static void syscall_tell(struct intr_frame *f, int fd);
static void syscall_close(struct intr_frame *f, int fd);

bool syscall_check_user_string(const char *str);
bool syscall_check_user_buffer (const char *str, int size, bool write);

static struct lock filesys_lock;
/* Implementation by ypm Ended */

/* Implementation by ymt Started */
static void syscall_mmap(struct intr_frame *f, int fd, const void *obj_vaddr);
static void syscall_munmap(struct intr_frame *f, mapid_t mapid);

#ifdef FILESYS
static void syscall_chdir(struct intr_frame *f, const char *dir);
static void syscall_mkdir(struct intr_frame *f, const char *dir);
static void syscall_readdir(struct intr_frame *f, int fd, char *name);
static void syscall_isdir(struct intr_frame *f, int fd);
static void syscall_inumber(struct intr_frame *f, int fd);
#endif

/* Implementation by ymt Ended */


void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&filesys_lock);
}

void syscall_file_close(struct file* file){
  lock_acquire(&filesys_lock);
  file_close(file);
  lock_release(&filesys_lock);
}

struct file* syscall_file_open(const char * name){
  lock_acquire(&filesys_lock);
  struct file* tmp = filesys_open(name);
  lock_release(&filesys_lock);
  return tmp;
}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
#ifdef VM
  /* 	Implementation by Chen Started */
  thread_current ()->esp = f->esp;
  /* 	Implementation by Chen ended */
#endif

  /* Implementation by ypm Started */
  if (!syscall_check_user_buffer(f->esp, 4, false))
    thread_exit_with_return_value(f, -1);

  int call_num = *((int *) f->esp);
  void *arg1 = f->esp + 4, *arg2 = f->esp + 8, *arg3 = f->esp + 12;

  switch (call_num){
    case SYS_EXIT:
    case SYS_EXEC:
    case SYS_WAIT:
    case SYS_TELL:
    case SYS_CLOSE:
    case SYS_REMOVE:
    case SYS_OPEN:
    case SYS_FILESIZE:
#ifdef VM
    case SYS_MUNMAP:
#endif

      /* Implementation by ymt Started */
#ifdef FILESYS
    case SYS_CHDIR:
    case SYS_MKDIR:
    case SYS_ISDIR:
    case SYS_INUMBER:
#endif
      /* Implementation by ymt Ended */

      if (!syscall_check_user_buffer(arg1, 4, false))
        thread_exit_with_return_value(f, -1);
      break;

    case SYS_CREATE:
    case SYS_SEEK:
#ifdef VM
    case SYS_MMAP:
#endif
      /* Implementation by ymt Started */
#ifdef FILESYS
    case SYS_READDIR:
#endif
      /* Implementation by ymt Ended */
      if (!syscall_check_user_buffer(arg1, 8, false))
        thread_exit_with_return_value(f, -1);
      break;

    case SYS_READ:
    case SYS_WRITE:
      if (!syscall_check_user_buffer(arg1, 12, false))
        thread_exit_with_return_value(f, -1);
      break;

    default: break;
  }


  switch (call_num)
  {
    case SYS_HALT:
      syscall_halt(f);
      break;

    case SYS_EXIT:
      syscall_exit(f, *((int *) arg1));
      break;

    case SYS_EXEC:
      syscall_exec(f, *((void **) arg1));
      break;

    case SYS_WAIT:
      syscall_wait(f, *((pid_t *) arg1));
      break;

    case SYS_TELL:
      syscall_tell(f, *((int *) arg1));
      break;

    case SYS_CLOSE:
      syscall_close(f, *((int *) arg1));
      break;

    case SYS_REMOVE:
      syscall_remove(f, *((void **) arg1));
      break;

    case SYS_OPEN:
      syscall_open(f, *((void **) arg1));
      break;

    case SYS_FILESIZE:
      syscall_filesize(f, *((int *) arg1));
      break;
      /* Implementation by ymt Started */
#ifdef VM
    case SYS_MUNMAP:
      syscall_munmap(f, *((mapid_t *) arg1));
      break;

    case SYS_MMAP:
      syscall_mmap(f, *((int *) arg1), *((void **) arg2));
      break;
#endif
#ifdef FILESYS
    case SYS_CHDIR:
      syscall_chdir(f, *((char **) arg1));
      break;

    case SYS_MKDIR:
      syscall_mkdir(f, *((char **) arg1));
      break;

    case SYS_READDIR:
      syscall_readdir(f, *((int *) arg1), *((char **) arg2));
      break;

    case SYS_ISDIR:
      syscall_isdir(f, *((int *) arg1));
      break;

    case SYS_INUMBER:
      syscall_inumber(f, *((int *) arg1));
      break;

#endif
      /* Implementation by ymt Ended */
    case SYS_CREATE:
      syscall_create(f, *((void **) arg1), *((unsigned *) arg2));
      break;

    case SYS_SEEK:
      syscall_seek(f, *((int *) arg1), *((unsigned *) arg2));
      break;

    case SYS_READ:
      syscall_read(f, *((int *) arg1), *((void **) arg2), *((unsigned *) arg3));
      break;

    case SYS_WRITE:
      syscall_write(f, *((int *) arg1), *((void **) arg2),
                    *((unsigned *) arg3));
      break;

    default:
      thread_exit_with_return_value(f, -1);
  }
  /* Implementation by ypm Ended */
}


/* Implementation by Wang Started */
static void
syscall_exec (struct intr_frame *f, const char *cmd_line)
{
  if (!syscall_check_user_string(cmd_line))
    thread_exit_with_return_value(f, -1);
  lock_acquire(&filesys_lock);
  f->eax = (uint32_t)process_execute (cmd_line);
  lock_release(&filesys_lock);
  struct list_elem *e;
  struct thread *cur = thread_current ();
  struct child_message *l;
  for (e = list_begin (&cur->child_list); e != list_end (&cur->child_list); e = list_next (e))
  {
    l = list_entry (e, struct child_message, elem);
    if (l->tid == f->eax)
    {
      sema_down (l->sema_started);
      if (l->load_failed)
        f->eax = (uint32_t)-1;
      return;
    }
  }
}

static void
syscall_wait (struct intr_frame *f, pid_t pid)
{
  f->eax = (uint32_t)process_wait (pid);
}

/* Implementation by Wang Ended */



/* Implementation by ypm Started */
static void
syscall_halt(struct intr_frame *f){
  shutdown_power_off();
}

static void
syscall_exit(struct intr_frame *f, const int return_value){

  /* Implementation by Wang Started */
  struct thread *cur = thread_current ();
  if (!cur->grandpa_died)
  {
    cur->message_to_grandpa->exited = true;
    cur->message_to_grandpa->return_value = return_value;
  }
  /* Implementation by Wang Ended */

  thread_exit_with_return_value(f, return_value);
}


static void
syscall_open(struct intr_frame *f, const char* name){
  if (!syscall_check_user_string(name))
    thread_exit_with_return_value(f, -1);
  lock_acquire(&filesys_lock);
  struct file* tmp_file = filesys_open(name);
  lock_release(&filesys_lock);
  if (tmp_file == NULL){
    f->eax = (uint32_t)-1;
    return;
  }

  static uint32_t fd_next = 2;

  struct file_handle* handle = malloc(sizeof(struct file_handle));
  handle->opened_file = tmp_file;
  handle->owned_thread = thread_current();
  handle->fd = fd_next++;
  /* Implementation by ymt Started */
#ifdef FILESYS
  if (inode_isdir(file_get_inode(tmp_file)))
    handle->opened_dir = dir_open(inode_reopen(file_get_inode(tmp_file)));
#endif
  /* Implementation by ymt Ended */
  thread_file_list_inster(handle);
  f->eax = (uint32_t)handle->fd;
}

static void
syscall_create(struct intr_frame *f, const char * name, unsigned initial_size){
  if (!syscall_check_user_string(name))
    thread_exit_with_return_value(f, -1);

  lock_acquire(&filesys_lock);
  f->eax = (uint32_t)filesys_create(name, initial_size);
  lock_release(&filesys_lock);
}

static void
syscall_remove(struct intr_frame *f, const char* name){
  if (!syscall_check_user_string(name))
    thread_exit_with_return_value(f, -1);
  lock_acquire(&filesys_lock);
  f->eax = (uint32_t)filesys_remove(name);
  lock_release(&filesys_lock);
}

static void
syscall_filesize(struct intr_frame *f, int fd){
  struct file_handle* t = syscall_get_file_handle(fd);
  if(t != NULL){
    lock_acquire(&filesys_lock);
    f->eax = (uint32_t)file_length(t->opened_file);
    lock_release(&filesys_lock);
  }

  else
    thread_exit_with_return_value(f, -1);
}

static void
syscall_read(struct intr_frame *f, int fd, const void* buffer, unsigned size){
  if (!syscall_check_user_buffer(buffer, size, true))
    thread_exit_with_return_value(f, -1);

  if (fd == STDOUT_FILENO)
    thread_exit_with_return_value(f, -1);

  uint8_t * str = buffer;
  if (fd == STDIN_FILENO){
    while(size-- != 0)
      *(char *)str++ = input_getc();
  }
  else{
    struct file_handle* t = syscall_get_file_handle(fd);
    if (t != NULL && !inode_isdir(file_get_inode(t->opened_file))){
      lock_acquire(&filesys_lock);
      f->eax = (uint32_t)file_read(t->opened_file, (void*)buffer, size);
      lock_release(&filesys_lock);
    }
    else
      thread_exit_with_return_value(f, -1);
  }
}

static void
syscall_write(struct intr_frame *f, int fd, const void* buffer, unsigned size){
  if (!syscall_check_user_buffer(buffer, size, false))
    thread_exit_with_return_value(f, -1);

  if (fd == STDIN_FILENO)
    thread_exit_with_return_value(f, -1);

  if (fd == STDOUT_FILENO)
    putbuf(buffer, size);
  else{
    struct file_handle* t = syscall_get_file_handle(fd);
    if (t != NULL && !inode_isdir(file_get_inode(t->opened_file))){
      lock_acquire(&filesys_lock);
      f->eax = (uint32_t)file_write(t->opened_file, (void*)buffer, size);
      lock_release(&filesys_lock);
    }
    else
      thread_exit_with_return_value(f, -1);
  }
}

static void
syscall_seek(struct intr_frame *f, int fd, unsigned position){
  struct file_handle* t = syscall_get_file_handle(fd);
  if (t != NULL){
    lock_acquire(&filesys_lock);
    file_seek(t->opened_file, position);
    lock_release(&filesys_lock);
  }

  else
    thread_exit_with_return_value(f, -1);
}

static void
syscall_tell(struct intr_frame *f, int fd){
  struct file_handle* t = syscall_get_file_handle(fd);
  if (t != NULL && !inode_isdir(file_get_inode(t->opened_file))){
    lock_acquire(&filesys_lock);
    f->eax = (uint32_t)file_tell(t->opened_file);
    lock_release(&filesys_lock);
  }
  else
    thread_exit_with_return_value(f, -1);
}

static void
syscall_close(struct intr_frame *f, int fd){
  struct file_handle* t = syscall_get_file_handle(fd);
  if(t != NULL){
    lock_acquire(&filesys_lock);
    /* Implementation by ymt Started */
#ifdef FILESYS
    if (inode_isdir(file_get_inode(t->opened_file)))
      dir_close(t->opened_dir);
#endif
    /* Implementation by ymt Ended */
    file_close(t->opened_file);
    lock_release(&filesys_lock);
    list_remove(&t->elem);
    free(t);
  }
  else
    thread_exit_with_return_value(f, -1);
}


bool
syscall_check_user_string(const char *ustr){
  if (!syscall_translate_vaddr(ustr, false))
    return false;
  int cnt = 0;
  while(*ustr != '\0'){
    if(cnt == 4095){
      puts("String to long, please make sure it is no longer than 4096 Bytes!\n");
      return false;
    }
    cnt++;
    ustr++;
    if (((int)ustr & PGMASK) == 0){
      if (!syscall_translate_vaddr(ustr, false))
        return false;
    }
  }
  return true;
}

bool
syscall_check_user_buffer(const char* ustr, int size, bool write){
  if (!syscall_translate_vaddr(ustr + size - 1, write))
    return false;

  size >>= 12;
  do{
    if (!syscall_translate_vaddr(ustr, write))
      return false;
    ustr += 1 << 12;
  }while(size--);
  return true;
}

/* Transfer user Vaddr to kernel vaddr
 * Return NULL if user Vaddr is invalid
 * */
bool
syscall_translate_vaddr(const void *vaddr, bool write){
  if (vaddr == NULL || !is_user_vaddr(vaddr))
    return false;
#ifdef VM
  struct page_table_elem* base = page_find_with_lock(thread_current()->page_table, pg_round_down(vaddr));
  if (base == NULL){
    return page_page_fault_handler(vaddr, write, thread_current()->esp);
  }
  else
    return !(write && !(base->writable));
#else
  ASSERT(vaddr != NULL);
  return pagedir_get_page(thread_current()->pagedir, vaddr) != NULL;
#endif
}

/* Implementation by ypm Ended */


/* Implementation by ymt Started */

bool
mmap_check_mmap_vaddr(struct thread *cur, const void *vaddr, int num_page)
{
#ifdef VM
  bool res = true;
  for (int i = 0; i < num_page; i++)
  {
    if (!page_available_upage(cur->page_table, vaddr + i * PGSIZE))
      res = false;
  }
  return res;
#endif
}

bool
mmap_install_page(struct thread *cur, struct mmap_handler *mh)
{
#ifdef VM
  bool res = true;
  for (int i = 0; i < mh->num_page; i++)
  {
    if (!page_install_file(cur->page_table, mh, mh->mmap_addr + i * PGSIZE))
      res = false;
  }
  if (mh->is_segment)
  {
    for (int i = mh->num_page; i < mh->num_page_with_segment; i++)
    {
      if (!page_install_file(cur->page_table, mh, mh->mmap_addr + i * PGSIZE))
        res = false;
    }
  }
  return res;
#endif
}

void mmap_read_file(struct mmap_handler* mh, void *upage, void *kpage)
{
#ifdef VM
  if (mh->is_segment)
  {
    void* addr = mh->mmap_addr + mh->num_page * PGSIZE + mh->last_page_size;
    /* Implementation by ypm Started */
    if (mh->last_page_size != 0)
      addr -= PGSIZE;
    /* Implementation by ypm Ended */
    if (addr > upage)
    {
      if (addr - upage < PGSIZE)
      {
        file_read_at(mh->mmap_file, kpage, mh->last_page_size,
                     upage - mh->mmap_addr + mh->file_ofs);
        memset(kpage + mh->last_page_size, 0, PGSIZE - mh->last_page_size);
      }
      else
        file_read_at(mh->mmap_file, kpage, PGSIZE, upage - mh->mmap_addr + mh->file_ofs);
    }
    else
      memset(kpage, 0, PGSIZE);
  }
  else
  {
    if (mh->mmap_addr + file_length(mh->mmap_file) - upage < PGSIZE)
    {
      file_read_at(mh->mmap_file, kpage, mh->last_page_size,
                   upage - mh->mmap_addr + mh->file_ofs);
      memset(kpage + mh->last_page_size, 0, PGSIZE - mh->last_page_size);
    }
    else
      file_read_at(mh->mmap_file, kpage, PGSIZE, upage - mh->mmap_addr + mh->file_ofs);
  }
#endif
}

void mmap_write_file(struct mmap_handler* mh, void* upage, void *kpage)
{
#ifdef VM
  if (mh->writable)
  {
    if (mh->is_segment)
    {
      void* addr = mh->mmap_addr + mh->num_page * PGSIZE + mh->last_page_size;
      if (addr > upage)
      {
        if (addr - upage < PGSIZE)
          file_write_at(mh->mmap_file, kpage, mh->last_page_size,
                        upage - mh->mmap_addr + mh->file_ofs);
        else
          file_write_at(mh->mmap_file, kpage, PGSIZE, upage - mh->mmap_addr + mh->file_ofs);
      }
    }
    else
    {
      if (mh->mmap_addr + file_length(mh->mmap_file) - upage < PGSIZE)
        file_write_at(mh->mmap_file, kpage, mh->last_page_size,
                      upage - mh->mmap_addr + mh->file_ofs);
      else
        file_write_at(mh->mmap_file, kpage, PGSIZE, upage - mh->mmap_addr + mh->file_ofs);
    }
  }
#endif
}

bool mmap_load_segment(struct file *file, off_t ofs, uint8_t *upage, uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{
#ifdef VM
  ASSERT(!((read_bytes + zero_bytes) & PGMASK))
  struct thread* cur = thread_current();
  mapid_t mapid = cur->next_mapid++;
  struct mmap_handler *mh = malloc(sizeof(struct mmap_handler));
  mh->mapid = mapid;
  mh->mmap_file = file;
  mh->writable = writable;
  mh->is_static_data = writable;
  int num_page = read_bytes / PGSIZE;
  int total_num_page = ((read_bytes + zero_bytes) / PGSIZE);
  int last_page_used = read_bytes & PGMASK;
  if (last_page_used != 0)
    num_page++;
  if (!mmap_check_mmap_vaddr(cur, upage, total_num_page))
    return false;
  mh->mmap_addr = upage;
  mh->num_page = num_page;
  mh->last_page_size = last_page_used;
  mh->num_page_with_segment = total_num_page;
  mh->is_segment = true;
  mh ->file_ofs = ofs;
  list_push_back(&(cur->mmap_file_list), &(mh->elem));
  if(!mmap_install_page(cur, mh))
    return false;
  return true;
#endif
}


static void
syscall_mmap(struct intr_frame *f, int fd, const void *obj_vaddr)
{
#ifdef VM
  if (fd == 0 || fd == 1)
  {
    f->eax = MAP_FAILED;
    return;
  }
  if (obj_vaddr == NULL || ((uint32_t) obj_vaddr % (uint32_t)PGSIZE != 0))
  {
    f->eax = MAP_FAILED;
    return;
  }

  struct thread *cur = thread_current();
  struct file_handle *fh = syscall_get_file_handle(fd);

  if (fh != NULL)
  {
    mapid_t mapid = cur->next_mapid++;
    struct mmap_handler *mh = malloc(sizeof(struct mmap_handler));
    mh->mapid = mapid;
    mh->mmap_file = file_reopen(fh->opened_file);
    mh->writable = true;
    mh->is_segment = false;
    mh->is_static_data = false;
    mh->file_ofs = 0;
    off_t file_size = file_length(mh->mmap_file);
    int num_page = file_size / PGSIZE;
    int last_page_used = file_size % PGSIZE;
    if (last_page_used != 0)
      num_page++;
    if (!mmap_check_mmap_vaddr(cur, obj_vaddr, num_page))
    {
      f->eax = MAP_FAILED;
      return;
    }
    mh->mmap_addr = obj_vaddr;
    mh->num_page = num_page;
    mh->num_page_with_segment = num_page;
    mh->last_page_size = last_page_used;
    list_push_back(&(cur->mmap_file_list), &(mh->elem));
    if(!mmap_install_page(cur, mh))
    {
      f->eax = MAP_FAILED;
      return;
    }
    f->eax = (uint32_t) mapid;
  }
  else
  {
    f->eax = MAP_FAILED;
    return;
  }
#endif
}

static void
syscall_munmap(struct intr_frame *f, mapid_t mapid)
{
#ifdef VM
  struct thread *cur = thread_current();
  if (list_empty(&cur->mmap_file_list))
  {
    f->eax = MAP_FAILED;
    return;
  }
  struct mmap_handler *mh = syscall_get_mmap_handle(mapid);
  if (mh == NULL)
  {
    f->eax = MAP_FAILED;
    return;
  }
  for (int i = 0; i < mh->num_page; i++)
  {
    if (!page_unmap(cur->page_table, mh->mmap_addr + i * PGSIZE))
    {
      delete_mmap_handle(mh);
      f->eax = MAP_FAILED;
      return;
    }
  }
  if (!delete_mmap_handle(mh))
  {
    f->eax = MAP_FAILED;
    return;
  }
#endif
}

#ifdef FILESYS

static void
syscall_chdir(struct intr_frame *f, const char *dir)
{
  if (!syscall_check_user_string(dir) || strlen(dir) == 0)
  {
    f->eax = false;
    return;
  }
  struct thread *cur = thread_current();
  struct dir* target_dir;
  char *pure_name = malloc(READDIR_MAX_LEN + 1);
  bool is_dir;
  ASSERT(dir != NULL)
  ASSERT(pure_name != NULL)
  if (is_rootpath(dir))
  {
    dir_close(cur->current_dir);
    cur->current_dir = dir_open_root();
    f->eax = true;
    free(pure_name);
  }
  else
  {
    if (path_paser(dir, &target_dir, &pure_name, &is_dir))
    {
      ASSERT(target_dir != NULL)
      ASSERT(pure_name != NULL)
      struct dir *res = subdir_lookup(target_dir, pure_name);
      if (res != NULL)
      {
        dir_close(cur->current_dir);
        cur->current_dir = res;
        f->eax = true;
      }
      else
        f->eax = false;
      dir_close(target_dir);
    }
    else
      f->eax = false;
    free(pure_name);
  }
}

static void
syscall_mkdir(struct intr_frame *f, const char *dir)
{
  if (!syscall_check_user_string(dir) || strlen(dir) == 0)
  {
    f->eax = false;
    return;
  }
  struct dir* target_dir;
  char *pure_name = malloc(READDIR_MAX_LEN + 1);
  bool is_dir;
  ASSERT(dir != NULL)
  ASSERT(pure_name != NULL)
  if(!is_rootpath(dir) && path_paser(dir, &target_dir, &pure_name, &is_dir))
  {
    ASSERT(target_dir != NULL)
    ASSERT(pure_name != NULL)
    bool res = subdir_create(target_dir, pure_name);
    dir_close(target_dir);
    f->eax = res;
  }
  else
    f->eax = false;
  free(pure_name);
}

static void
syscall_readdir(struct intr_frame *f, int fd, char *name)
{
  if (fd == 0 || fd == 1
              || !syscall_check_user_buffer(name, READDIR_MAX_LEN + 1, false))
  {
    f->eax = false;
    return;
  }
  struct file_handle *fh = syscall_get_file_handle(fd);
  if (fh != NULL && is_dirfile(fh))
  {
    f->eax = dir_readdir(fh->opened_dir, name);
  }
  else
    f->eax = false;
}

static void
syscall_isdir(struct intr_frame *f, int fd)
{
  if (fd == 0 || fd == 1)
  {
    f->eax = false;
    return;
  }
  struct file_handle *fh = syscall_get_file_handle(fd);
  f->eax = fh != NULL && is_dirfile(fh);
}

static void
syscall_inumber(struct intr_frame *f, int fd)
{
  if (fd == 0 || fd == 1)
  {
    thread_exit_with_return_value(f, -1);
  }
  struct file_handle *fh = syscall_get_file_handle(fd);
  if (fh != NULL)
  {
    f->eax = inode_get_inumber(file_get_inode(fh->opened_file));
    return;
  }
  else
    f->eax = -1;
}

#endif
/* Implementation by ymt Ended */