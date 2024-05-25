/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"
#include "bitmap.h"
#include "include/threads/mmu.h"

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

size_t slot_max;
struct bitmap* b;

/* Initialize the data for anonymous pages */
void
vm_anon_init (void) {
	/* TODO: Set up the swap_disk. */
	swap_disk = disk_get(1, 1);
	slot_max = disk_size(swap_disk) / SLOT_SIZE;
	b = bitmap_create(slot_max);
}

/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	struct uninit_page *uninit = &page->uninit;

	page->operations = &anon_ops;

	struct anon_page *anon_page = &page->anon;

	anon_page->slot = BITMAP_ERROR;

	return true;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;
	
	if(anon_page->slot == BITMAP_ERROR || !bitmap_test(b, anon_page->slot))
		return false;

	bitmap_set(b, anon_page->slot, false);

	for(int i = 0; i < 8; i++){
		disk_read(swap_disk, anon_page->slot*8 + i, page->frame->kva + DISK_SECTOR_SIZE * i);
	}

	return true;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;
	size_t idx = bitmap_scan_and_flip(b, 0, 1, false);

	if(idx == BITMAP_ERROR)
		return false;

	for(int i = 0; i < 8; i++){
		disk_write(swap_disk, idx*8 + i, page->va + DISK_SECTOR_SIZE * i);
	}

	anon_page->slot = idx;

	// list_remove(&page->frame->frame_elem); // 이 부분 이해 필요함
	page->frame->accessed -= 1;
	page->frame->page = NULL;
	// free(page->frame);

	page->frame = NULL;

	pml4_clear_page(thread_current()->pml4, page->va);
	return true;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;
	struct thread *curr = thread_current();
	if (anon_page->slot != BITMAP_ERROR)
        bitmap_reset(b, anon_page->slot);

	pml4_activate(NULL);
	if(page->frame->accessed == 1){
		lock_acquire(&frame_lock);
		list_remove(&page->frame->frame_elem);
		lock_release(&frame_lock);
		palloc_free_page(page->frame->kva);
        free(page->frame);
    }else{
		page->frame->accessed -= 1;
	}
	page->frame->page = NULL;
	page->frame = NULL;
	pml4_clear_page(curr->pml4, page->va);
}
