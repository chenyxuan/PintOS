/*
	Author: Chen
	Implementation: Supplemental Page Table
*/

#include <stdio.h>
#include "page.h"
#include "frame.h"
#include "swap.h"
#include "../threads/thread.h"
#include "../userprog/pagedir.h"
#include "../userprog/syscall.h"
#include "../lib/stddef.h"
#include "../threads/malloc.h"
#include "../lib/debug.h"
#include "../threads/vaddr.h"

#define PAGE_PAL_FLAG			0
#define PAGE_INST_MARGIN		32
#define PAGE_STACK_SIZE			0x800000
#define PAGE_STACK_UNDERLINE	(PHYS_BASE - PAGE_STACK_SIZE)

bool page_hash_less(const struct hash_elem *lhs,
					 const struct hash_elem *rhs,
					 void *aux UNUSED);
					 
unsigned page_hash(const struct hash_elem *e,
					void *aux UNUSED);
					
void page_destroy_frame_likes(struct hash_elem *e,
							void *aux UNUSED);

static struct lock page_lock;

void page_lock_init() {
	lock_init(&page_lock);
}
/* basic life cycle */
page_table_t*
page_create() {
	page_table_t *t = malloc(sizeof(page_table_t));

	if(t != NULL) {
		if(page_init(t) == false) {
			free(t);
			return NULL;
		}
		else {
			return t;
		}
	}
	else {
		return NULL;
	}
}
/* return whether page init is successful or not, btw, this function will create an initial virtual stack slot */
bool
page_init(page_table_t *page_table) {
	return hash_init(page_table, page_hash, page_hash_less, NULL);
}

/* destroy page_table and recycle FRAME and SWAP slot */
void
page_destroy(page_table_t *page_table) {
	lock_acquire(&page_lock);
	hash_destroy(page_table, page_destroy_frame_likes);
	lock_release(&page_lock);
}

/* find the element with key = upage in page table*/
struct page_table_elem*
page_find(page_table_t *page_table, void *upage) {
	struct hash_elem *e;
	struct page_table_elem tmp;

    ASSERT(page_table != NULL);
	tmp.key = upage;
	e = hash_find(page_table, &(tmp.elem));
	
	if(e != NULL) {
		return hash_entry(e, struct page_table_elem, elem);
	}
	else {
		return NULL;
	}
}

/* page fault handler of page table*/

/*
	given a hash table and virtual address(page), allocate a frame for this page
	return whether the dealt is successful or not.
*/

bool
page_page_fault_handler(const void *vaddr, bool to_write, void *esp) {
//	printf("page_falut--%08x %08x %08x\n", vaddr, esp, PAGE_STACK_UNDERLINE);
	struct thread *cur = thread_current();
	page_table_t *page_table = cur->page_table;
	uint32_t *pagedir = cur->pagedir;
	void *upage = pg_round_down(vaddr);
	
	bool success = true;
	lock_acquire(&page_lock);
	
	struct page_table_elem *t = page_find(page_table, upage);
	void *dest = NULL;
	
	ASSERT(is_user_vaddr(vaddr));
	ASSERT(!(t != NULL && t->status == FRAME));
//	printf("vaddr:%p    esp:%p\n", vaddr, esp, PAGE_STACK_UNDERLINE);
	if(to_write == true && t != NULL && t->writable == false)
	  return false;
		
	if(upage >= PAGE_STACK_UNDERLINE) {
		if(vaddr >= esp - PAGE_INST_MARGIN) {
			if(t == NULL) {
				dest = frame_get_frame(PAGE_PAL_FLAG, upage);
				if(dest == NULL) {
					success = false;
				}
				else {
					t = malloc(sizeof(*t));
					t->key = upage;
					t->value = dest;
					t->status = FRAME;
					t->writable = true;
					t->origin = NULL;
					hash_insert(page_table, &t->elem);
				}
			}
			else {
				switch(t->status) {
					case SWAP:
						dest = frame_get_frame(PAGE_PAL_FLAG, upage);
						if(dest == NULL) {
							success = false;
							break;
						}
						swap_load((index_t) t->value, dest);
						t->value = dest;
						t->status = FRAME;
		  //            printf("swap :%d, %p->%p\n", cur->tid, t->key, t->value);
						break;
					default:
/*
						printf("fuck\n");
						printf("status:%s\n", t->status == FILE ? "FILE" : "FRAME");
*/
						success = false;
				}
			}
		}
		else {
			success = false;
		}
	}
	else {
		if(t == NULL) {
			success = false;
		}
		else {
			switch(t->status) {
				case SWAP:
					dest = frame_get_frame(PAGE_PAL_FLAG, upage);
					if(dest == NULL) {
						success = false;
						break;
					}
					swap_load((index_t)t->value, dest);
					t->value = dest;
					t->status = FRAME;
//                    printf("swap to frame:%d, %p->%p\n", cur->tid, t->key, t->value);
					break;
				case FILE:
					dest = frame_get_frame(PAGE_PAL_FLAG, upage);
					if(dest == NULL) {
						success = false;
						break;
					}
					mmap_read_file(t->value, upage, dest);
					t->value = dest;
					t->status = FRAME;
//                    printf("file to frame:%d, %p->%p\n", cur->tid, t->key, t->value);
					break;
				default:
					success = false;
			}
		}
	}
	
	frame_set_pinned_false(dest);
	lock_release(&page_lock);
	if(success) {
		ASSERT(pagedir_set_page(pagedir, t->key, t->value, t->writable));
	}
	return success;
}

/* Verify that there's not already a page at that virtual
 address, then map our page there. */
bool page_set_frame(void *upage, void *kpage, bool wb) {
	struct thread *cur = thread_current();
	page_table_t *page_table = cur->page_table;
	uint32_t *pagedir = cur->pagedir;

	bool success = true;
	lock_acquire(&page_lock);
	
	struct page_table_elem *t = page_find(page_table, upage);
	if(t == NULL) {
		t = malloc(sizeof(*t));
		t->key = upage;
		t->value = kpage;
		t->status = FRAME;
		t->origin = NULL;
		t->writable = wb;
		hash_insert(page_table, &t->elem);
//    printf("stack %p->%p\n", t->key, t->value);
	}
	else {
		success = false;
	}
	
	lock_release(&page_lock);
	
	if(success) {
		ASSERT(pagedir_set_page(pagedir, t->key, t->value, t->writable));
	}
	return success;
}

/* check if upage is below stack underline and available*/
bool page_available_upage(page_table_t *page_table, void *upage) {
	return upage < PAGE_STACK_UNDERLINE && page_find(page_table, upage) == NULL;
}

/* check if upage is accessible for a non-stack visit */
bool page_accessible_upage(page_table_t *page_table, void *upage) {
	return upage < PAGE_STACK_UNDERLINE && page_find(page_table, upage) != NULL;
}

/* install file to page_table */
bool page_install_file(page_table_t *page_table, struct mmap_handler *mh, void *key) {
	struct thread *cur = thread_current();
	bool success = true;
	lock_acquire(&page_lock);
	if(page_available_upage(page_table, key)) {
		struct page_table_elem *e = malloc(sizeof(*e));
		e->key = key;
		e->value = mh;
		e->status = FILE;
		e->writable = mh->writable;
		e->origin = mh;
		hash_insert(page_table, &e->elem);
	}
	else {
		success = false;
	}	
	lock_release(&page_lock);
	return success;
}

/* unmount a file */
bool page_unmap(page_table_t *page_table, void *upage) {
	struct thread *cur = thread_current();
	bool success = true;
	lock_acquire(&page_lock);
	
	if(page_accessible_upage(page_table, upage)) {
		struct page_table_elem *t = page_find(page_table, upage);

		ASSERT( t != NULL );
		switch(t->status) {
			case FILE:
				hash_delete(page_table, &(t->elem));
				free(t);
				break;
			case FRAME:
			    if(pagedir_is_dirty(cur->pagedir, t->key)) {
			    	mmap_write_file(t->origin, t->key, t->value);
			    }
			    pagedir_clear_page(cur->pagedir, t->key);
			    hash_delete(page_table, &(t->elem));
         		frame_free_frame(t->value);
			    free(t);
			    break;
			default:
				success = false;
		}
		
	}
	else {
		success = false;
	}
	lock_release(&page_lock);
	return success;
}

/* switch a page from FRAME to SWAP */
bool page_status_eviction(struct thread *cur, void *upage, void *index, bool to_swap) {
	struct page_table_elem *t = page_find(cur->page_table, upage);
    bool success = true;
    
	if(t != NULL && t->status == FRAME) {
		if(to_swap) {
			t->value = index;
			t->status = SWAP;
		}
		else {
//            printf("frame to file :%d, %p->%p\n", cur->tid, t->value, t->origin);
			ASSERT( t->origin != NULL );
			t->value = t->origin;
			t->status = FILE;
		}
		pagedir_clear_page(cur->pagedir, upage);
	}
	else {
//	  printf("--%d, %p, %p, %d--", cur->tid, t, upage, ((struct mmap_handler*)t->origin)->is_static_data);
	  if (t != NULL){
	    printf("%s\n", t->status == FILE ? "flie" : "swap");
	  }
	  else
	    puts("NULL");
	  success = false;
	}

	return success;
	
}


/* used to check whether the two pages are the same */
bool page_hash_less(const struct hash_elem *lhs, const struct hash_elem *rhs, void *aux UNUSED) {
	return hash_entry(lhs, struct page_table_elem, elem)->key < hash_entry(rhs, struct page_table_elem, elem)->key;
}

/* hash function of element in hash table*/
unsigned page_hash(const struct hash_elem *e, void* aux UNUSED){
	struct page_table_elem *t = hash_entry(e, struct page_table_elem, elem);
	return hash_bytes(&(t->key), sizeof(t->key));
}

/* return FRAME and SWAP slot back */
/* TODO_end: maybe there is something need to be written back to FILESYS */
/* note: nothing to do with FILESYS, FILESYS key in this table is read-only */

void page_destroy_frame_likes(struct hash_elem *e, void *aux UNUSED) {
	struct page_table_elem *t = hash_entry(e, struct page_table_elem, elem);

	if(t->status == FRAME) {
//    printf("des-value: %p\n", t->value);
	  pagedir_clear_page(thread_current()->pagedir, t->key);
		frame_free_frame(t->value);
	}
	else if(t->status == SWAP) {
		swap_free((index_t) t->value);
	}
	free(t);
}


struct page_table_elem* page_find_with_lock(page_table_t *page_table, void *upage){
	lock_acquire(&page_lock);
	struct page_table_elem* tmp = page_find(page_table, upage);
	lock_release(&page_lock);
	return tmp;
}