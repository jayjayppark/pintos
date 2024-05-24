/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "hash.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "include/threads/mmu.h"
#include "include/userprog/process.h"

struct list frame_list;
/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
	list_init(&frame_list);
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. 
 * 페이지 폴트가 일어나면, 페이지 폴트 핸들러는 vm_try_handle_fault 함수에게 제어권을 넘긴다.
 * 	함수 역할: 페이지 폴트가 유효한지 먼저 확인
 * bogus 오류인 경우 일부 내용으 페이지에 로드하고 사용자 프로그램에 제어 권한을 반환함*/
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage is already occupied or not. */
	typedef (*initializer_by_type)(struct page *, enum vm_type, void *);
	initializer_by_type initializer = NULL;
	
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		struct page *page = (struct page*)malloc(sizeof(struct page));

		switch (VM_TYPE(type))
		{
		case (VM_ANON):/* constant-expression */
			initializer = anon_initializer;
			break;
		
		case (VM_FILE):
			initializer = file_backed_initializer;
			break;
		}

		uninit_new(page, upage, init, type, aux, initializer);
		page->writable = writable;

		/* TODO: Insert the page into the spt. */
		return spt_insert_page(spt, page);
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	struct page *page = (struct page*)malloc(sizeof(struct page));
	page->va = pg_round_down(va);
	/* TODO: Fill this function. */
	struct hash_elem *e = hash_find(&spt->hash_spt, &page->hash_elem);
	free(page);

	return  e != NULL ? hash_entry (e, struct page, hash_elem) : NULL;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED, struct page *page UNUSED) {

	/* TODO: Fill this function. */
	if(!hash_insert(&spt->hash_spt, &page->hash_elem))
		return true;
	else
		return false;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */
	struct thread *curr = thread_current();
	struct list_elem *frame_e;
	for(frame_e = list_begin(&frame_list); frame_e != list_end(&frame_list); frame_e = list_next(frame_e)){
		victim = list_entry(frame_e, struct frame, frame_elem);
		if(!pml4_is_accessed(curr->pml4, victim->page->va))
			return victim;
		else
			pml4_set_accessed(curr->pml4, victim->page->va, 0);	
		
	}
	return list_entry(list_begin(&frame_list), struct frame, frame_elem);
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */
	// printf("\n\n%p\n\n", victim->kva);
	if(victim->page != NULL)
		swap_out(victim->page);
	return victim;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	struct frame *frame = (struct frame *)malloc(sizeof (struct frame));
	frame->kva = palloc_get_page(PAL_USER | PAL_ZERO);
	/* TODO: Fill this function. */
	if(frame->kva == NULL){
		frame = vm_evict_frame();
		frame->page = NULL;
		return frame;
	}
	list_push_back(&frame_list, &frame->frame_elem);
	frame->page = NULL;
	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {

	void *stack_bottom = thread_current()->stack_bottom;
	bool success = false;

	if(vm_alloc_page(VM_ANON | VM_MARKER_0, stack_bottom-PGSIZE, 1)){
		success = vm_claim_page(stack_bottom-PGSIZE);

		if(success){
			thread_current()->stack_bottom = stack_bottom-PGSIZE;
		}
	}
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	// printf(f->rsp);
	if (addr == NULL || is_kernel_vaddr(addr)) {
        return false;
	}

	/* 페이지가 물리 메모리에 존재하는데 폴트가 났음 : 1. 페이지 보호 위반 2. 비정렬된 접근 3. 페이지 테이블 손상 4. 메모리 매핑 오류 5. 하드웨어 문제 */
    if (!not_present) 
		return false; 
		
	/* 페이지가 물리 메모리에 존재하지 않는 경우 */
	if(!vm_claim_page(addr)){ // uninit 페이지가 존재하면 물리 매핑 해줌
		void *stack_pointer = user ? f->rsp : thread_current()->rsp_pointer;
		/* stack 영역에 의한 fault일 경우 새로운 스택 페이지를 할당하는 과정 */
		/* uninit 이 존재하지 않으면 즉, 할당받은 적이 없는 페이지이면 stack growth */
		if (addr >= stack_pointer - 8 && addr >= USER_STACK - (1 << 20) && addr <= USER_STACK) {
			vm_stack_growth(addr);
			return true;
		}
		return false; // 페이지가 존재하지 않는데 uninit 도 없고, stack grow 의 대상도 아님.
	}
	return true;
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function */
	struct thread *curr = thread_current();
	page = spt_find_page(&curr->spt, va);
	if(page == NULL)
		return false;
	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	struct thread *curr = thread_current();
	
	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	pml4_set_page(curr->pml4, page->va, frame->kva, page->writable);

	return swap_in (page, frame->kva);
}

uint64_t hash_func (const struct hash_elem *e, void *aux){
	struct page *page = hash_entry(e, struct page, hash_elem);

	return hash_bytes(&page->va, sizeof page->va);
}

/* Compares the value of two hash elements A and B, given
 * auxiliary data AUX.  Returns true if A is less than B, or
 * false if A is greater than or equal to B. */
bool hash_less (const struct hash_elem *a, const struct hash_elem *b, void *aux){
	struct page *a_p = hash_entry(a, struct page, hash_elem);
	struct page *b_p = hash_entry(b, struct page, hash_elem);

	return a_p->va < b_p->va;
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	hash_init (&spt->hash_spt, &hash_func, &hash_less, NULL); 
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
		
	struct hash_iterator iter;
	hash_first(&iter, &src->hash_spt);

	while(hash_next(&iter)){
		struct page *page = hash_entry(hash_cur(&iter), struct page, hash_elem);
		enum vm_type type = page->operations->type; // 이 페이지의 현재 상태 타입

		if(type == VM_UNINIT){ // page->uninit.type 이 페이지가 다음에 접근될 때 적용할 type
			if(!vm_alloc_page_with_initializer(page->uninit.type, page->va, page->writable, page->uninit.init, page->uninit.aux))
				return false;
		}else{
			if(!vm_alloc_page(type, page->va, page->writable))
				return false;

			if(!vm_claim_page(page->va))
				return false;
		}

		if(type != VM_UNINIT){
			struct page *child_page = spt_find_page(dst, page->va);
			memcpy(child_page->frame->kva, page->frame->kva, PGSIZE);
		}
	}
	return true;
}

void hash_destructor (struct hash_elem *e, void *aux){
	struct page *page = hash_entry(e, struct page, hash_elem);
	if(page != NULL){
		destroy(page);
		free(page);
	}
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	hash_clear(&spt->hash_spt, hash_destructor);
}
