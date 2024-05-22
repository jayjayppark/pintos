/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "include/threads/mmu.h"
#include "include/userprog/process.h"

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void
vm_file_init (void) {
}

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
	struct thread *curr = thread_current();
	// if(pml4_is_dirty(curr->pml4, page->va)){
	// 	file_write_at(page->file, )
	// }

	// if(page->frame){
	// 	list_remove(&page->frame->frame_elem);
	// 	page->frame->page = NULL;
	// 	page->frame = NULL;
	// 	free(page);
	// }
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {
	lock_acquire(&fd_lock);
	struct file* cpy_file = file_reopen(file);
	uint32_t read_bytes = (length > file_length(cpy_file)) ? file_length(cpy_file) : length;
	uint32_t zero_bytes = PGSIZE - (read_bytes % PGSIZE);
	void *upage = addr;
	ASSERT((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT(pg_ofs(upage) == 0);
	ASSERT(offset % PGSIZE == 0);

	while (read_bytes > 0 || zero_bytes > 0)
	{
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* TODO: Set up aux to pass information to the lazy_load_segment. */
		struct container *aux = (struct container *)malloc(sizeof(struct container));
		aux->file = cpy_file;
		aux->ofs = offset;
		aux->page_read_bytes = page_read_bytes;
		aux->page_zero_bytes = page_zero_bytes;

		if (!vm_alloc_page_with_initializer(VM_FILE, upage, writable, lazy_load_segment, aux)){
			free(aux);
			lock_release(&fd_lock);
			return false;
		}

		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		upage += PGSIZE;
		offset += page_read_bytes;
	}
	lock_release(&fd_lock);
	return addr;
}

/* Do the munmap */
void
do_munmap (void *addr) {
}
