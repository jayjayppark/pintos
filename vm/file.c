/* file.c: Implementation of memory backed file object (mmaped object). */

#include <string.h>
#include "vm/vm.h"
#include "include/threads/vaddr.h"

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
}

static bool
lazy_load_segment (struct page *page, void *aux) {
	struct load_segment_aux *con = aux;

	// 파일로부터 con->read_bytes만큼 데이터를 읽어 페이지 프레임에 씁니다.
	if (file_read_at(con->file, page->frame->kva, con->read_bytes, con->ofs) != con->read_bytes)
		return false;

	// 페이지에 남은 부분은 0으로 초기화합니다.
	memset(page->frame->kva + con->read_bytes, 0, con->zero_bytes);

	return true;
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {
	size_t read_bytes = length;

	while (read_bytes > 0) {
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* TODO: Set up aux to pass information to the lazy_load_segment. */
		struct load_segment_aux *aux = (struct load_segment_aux *)malloc(sizeof(struct load_segment_aux));
		aux->file = file;
		aux->ofs = offset;
		aux->read_bytes = page_read_bytes;
		aux->zero_bytes = page_zero_bytes;

		if (!vm_alloc_page_with_initializer (VM_FILE, addr,
					writable, lazy_load_segment, aux)) {
			return NULL;
		}

		/* Advance. */
		read_bytes -= page_read_bytes;
		addr += PGSIZE;
		offset += page_read_bytes; 
	}
	return addr;
}

/* Do the munmap */
void
do_munmap (void *addr) {
}
