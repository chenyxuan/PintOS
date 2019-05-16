/*
	Author: Chen
	Implementation: Supplemental Page Table
*/

/*
	Page table mainly deal with such two situations:
	1. When page fault occurs, supplemental page table helps get the page to frame.
	2. When process exits, supplemental page table helps recycle resources.
	
	As is known to us all, the second situation is trivial :
		What we need to do is only iterate each element in page table and free them all (in fact, only FRAME and SWAP), then we destroy page table.

	Let's talk about the first situation.
	Where is physical address of our page?
		Maybe in RAM, ok, in fact, if it's really true, then page fault should not occur.
		Maybe in SWAP, a unfortunate frame has been evicted in this space.
		Maybe in stack (at the top of virtual address space) and we haven't apply a frame for it.
		Maybe in file system, we haven't load it ---- and this is what we need to do.
		
	When a virtual address is given to us, we hope to figure out what it is.
	if page ----> FRAME, ASSERT false.
	if page ----> SWAP, obtain a frame and fill it with SWAP slot.
	if page ----> FILE, obtain a frame and fill it with FILESTREAM.
	Return the frame to caller. 
	
	Parallelism seems has nothing to do with me, after all, one process(thread) can't make two page fault at the same time.
	BUT FRAME and SWAP need it!
*/

#ifndef SUPPLEMENTAL_PAGE_TABLE_MODULE
#define SUPPLEMENTAL_PAGE_TABLE_MODULE

#include "../lib/stdint.h"
#include "../lib/kernel/hash.h"
#include "../threads/palloc.h"
#include "../threads/thread.h"
typedef struct hash page_table_t;

enum page_status {
	FRAME,
	SWAP,
	FILE
};

struct page_table_elem {
	void *key, *value, *origin;
	enum page_status status;
	bool writable;
	/*
		key		:	virtual address of page
		value	:	physical address of frame	status = FRAME
		         or index of SWAP slot			status = SWAP
		         or mapid of mapped file		status = FILE
	*/
	struct hash_elem elem;
};

void page_lock_init();
/* basic life cycle */
page_table_t *page_create();
bool page_init(page_table_t *page_table);
void page_destroy(page_table_t *page_table);
struct page_table_elem* page_find(page_table_t *page_table, void *upage);
struct page_table_elem* page_find_with_lock(page_table_t *page_table, void *upage);

/* page fault handler */
bool
page_page_fault_handler(const void *vaddr, bool to_write, void *esp);

/* interfaces for other modules */
bool page_set_frame(void *upage, void *kpage, bool wb);
bool page_available_upage(page_table_t *page_table, void *upage);
bool page_install_file(page_table_t *page_table, struct mmap_handler *mh, void *key);
bool page_status_eviction(struct thread *cur, void *upage, void *index, bool to_swap);
bool page_unmap(page_table_t *page_table, void *upage);


#endif

