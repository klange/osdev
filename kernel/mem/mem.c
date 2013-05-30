/* vim: tabstop=4 shiftwidth=4 noexpandtab
 *
 * Kernel Memory Manager
 */

#include <mem.h>
#include <system.h>
#include <process.h>
#include <logging.h>
#include <signal.h>

#define KERNEL_HEAP_INIT 0x02000000
#define KERNEL_HEAP_END  0x20000000

extern void *end;
uintptr_t placement_pointer = (uintptr_t)&end;
uintptr_t heap_end = (uintptr_t)NULL;

void
kmalloc_startat(
		uintptr_t address
		) {
	placement_pointer = address;
}

/*
 * kmalloc() is the kernel's dumb placement allocator
 */
uintptr_t
kmalloc_real(
		size_t size,
		int align,
		uintptr_t * phys
		) {
	if (heap_end) {
		void * address;
		if (align) {
			address = valloc(size);
		} else {
			address = malloc(size);
		}
		if (phys) {
			page_t *page = get_page((uintptr_t)address, 0, kernel_directory);
			*phys = page->frame * 0x1000 + ((uintptr_t)address & 0xFFF);
		}
		return (uintptr_t)address;
	}

	if (align && (placement_pointer & 0xFFFFF000)) {
		placement_pointer &= 0xFFFFF000;
		placement_pointer += 0x1000;
	}
	if (phys) {
		*phys = placement_pointer;
	}
	uintptr_t address = placement_pointer;
	placement_pointer += size;
	return (uintptr_t)address;
}
/*
 * Normal
 */
uintptr_t
kmalloc(
		size_t size
		) {
	return kmalloc_real(size, 0, NULL);
}
/*
 * Aligned
 */
uintptr_t
kvmalloc(
		size_t size
		) {
	return kmalloc_real(size, 1, NULL);
}
/*
 * With a physical address
 */
uintptr_t
kmalloc_p(
		size_t size,
		uintptr_t *phys
		) {
	return kmalloc_real(size, 0, phys);
}
/*
 * Aligned, with a physical address
 */
uintptr_t
kvmalloc_p(
		size_t size,
		uintptr_t *phys
		) {
	return kmalloc_real(size, 1, phys);
}

/*
 * Frame Allocation
 */

uint32_t *frames;
uint32_t nframes;

#define INDEX_FROM_BIT(b) (b / 0x20)
#define OFFSET_FROM_BIT(b) (b % 0x20)

void
set_frame(
		uintptr_t frame_addr
		) {
	uint32_t frame  = frame_addr / 0x1000;
	uint32_t index  = INDEX_FROM_BIT(frame);
	uint32_t offset = OFFSET_FROM_BIT(frame);
	frames[index] |= (0x1 << offset);
}

void
clear_frame(
		uintptr_t frame_addr
		) {
	uint32_t frame  = frame_addr / 0x1000;
	uint32_t index  = INDEX_FROM_BIT(frame);
	uint32_t offset = OFFSET_FROM_BIT(frame);
	frames[index] &= ~(0x1 << offset);
}

uint32_t
test_frame(
		uintptr_t frame_addr
		) {
	uint32_t frame  = frame_addr / 0x1000;
	uint32_t index  = INDEX_FROM_BIT(frame);
	uint32_t offset = OFFSET_FROM_BIT(frame);
	return (frames[index] & (0x1 << offset));
}

uint32_t
first_frame() {
	uint32_t i, j;

	for (i = 0; i < INDEX_FROM_BIT(nframes); ++i) {
		if (frames[i] != 0xFFFFFFFF) {
			for (j = 0; j < 32; ++j) {
				uint32_t testFrame = 0x1 << j;
				if (!(frames[i] & testFrame)) {
					return i * 0x20 + j;
				}
			}
		}
	}

	kprintf("\033[1;37;41mWARNING: System claims to be out of usable memory, which means we probably overwrote the page frames.\033[0m\n");

#if 0
	signal_t * sig = malloc(sizeof(signal_t));
	sig->handler = current_process->signals.functions[SIGSEGV];
	sig->signum  = SIGSEGV;
	handle_signal((process_t *)current_process, sig);
#endif

	STOP;

	return -1;
}

void
alloc_frame(
		page_t *page,
		int is_kernel,
		int is_writeable
		) {
	if (page->frame != 0) {
		page->present = 1;
		page->rw      = (is_writeable == 1) ? 1 : 0;
		page->user    = (is_kernel == 1)    ? 0 : 1;
		return;
	} else {
		uint32_t index = first_frame();
		assert(index != (uint32_t)-1 && "Out of frames.");
		set_frame(index * 0x1000);
		page->present = 1;
		page->rw      = (is_writeable == 1) ? 1 : 0;
		page->user    = (is_kernel == 1)    ? 0 : 1;
		page->frame   = index;
	}
}

void
dma_frame(
		page_t *page,
		int is_kernel,
		int is_writeable,
		uintptr_t address
		) {
	/* Page this address directly */
	page->present = 1;
	page->rw      = (is_writeable) ? 1 : 0;
	page->user    = (is_kernel)    ? 0 : 1;
	page->frame   = address / 0x1000;
}

void
free_frame(
		page_t *page
		) {
	uint32_t frame;
	if (!(frame = page->frame)) {
		assert(0);
		return;
	} else {
		clear_frame(frame * 0x1000);
		page->frame = 0x0;
	}
}

uintptr_t
memory_use() {
	uintptr_t ret = 0;
	uint32_t i, j;
	for (i = 0; i < INDEX_FROM_BIT(nframes); ++i) {
		for (j = 0; j < 32; ++j) {
			uint32_t testFrame = 0x1 << j;
			if (frames[i] & testFrame) {
				ret++;
			}
		}
	}
	return ret * 4;
}

uintptr_t
memory_total(){
	return nframes * 4;
}

void
paging_install(uint32_t memsize) {
	nframes = memsize  / 4;
	frames  = (uint32_t *)kmalloc(INDEX_FROM_BIT(nframes * 8));
	memset(frames, 0, INDEX_FROM_BIT(nframes));

	uintptr_t phys;
	kernel_directory = (page_directory_t *)kvmalloc_p(sizeof(page_directory_t),&phys);
	memset(kernel_directory, 0, sizeof(page_directory_t));

	for (uintptr_t i = 0; i < placement_pointer + 0x3000; i += 0x1000) {
		alloc_frame(get_page(i, 1, kernel_directory), 1, 0);
	}
	/* XXX VGA TEXT MODE VIDEO MEMORY EXTENSION */
	for (uintptr_t j = 0xb8000; j < 0xc0000; j += 0x1000) {
		alloc_frame(get_page(j, 1, kernel_directory), 0, 1);
	}
	isrs_install_handler(14, page_fault);
	kernel_directory->physical_address = (uintptr_t)kernel_directory->physical_tables;

	/* Kernel Heap Space */
	for (uintptr_t i = placement_pointer; i < KERNEL_HEAP_INIT; i += 0x1000) {
		alloc_frame(get_page(i, 1, kernel_directory), 1, 0);
	}
	/* And preallocate the page entries for all the rest of the kernel heap as well */
	for (uintptr_t i = KERNEL_HEAP_INIT; i < KERNEL_HEAP_END; i += 0x1000) {
		get_page(i, 1, kernel_directory);
	}

	current_directory = clone_directory(kernel_directory);
	switch_page_directory(kernel_directory);
}

void
debug_print_directory() {
	debug_print(INFO, " ---- [k:0x%x u:0x%x]", kernel_directory, current_directory);
	for (uintptr_t i = 0; i < 1024; ++i) {
		if (!current_directory->tables[i] || (uintptr_t)current_directory->tables[i] == (uintptr_t)0xFFFFFFFF) {
			continue;
		}
		if (kernel_directory->tables[i] == current_directory->tables[i]) {
			debug_print(INFO, "  0x%x - kern [0x%x/0x%x] 0x%x", current_directory->tables[i], &current_directory->tables[i], &kernel_directory->tables[i], i * 0x1000 * 1024);
		} else {
			debug_print(INFO, "  0x%x - user [0x%x] 0x%x [0x%x]", current_directory->tables[i], &current_directory->tables[i], i * 0x1000 * 1024, kernel_directory->tables[i]);
			for (uint16_t j = 0; j < 1024; ++j) {
#if 0
				page_t *  p= &current_directory->tables[i]->pages[j];
				if (p->frame) {
					debug_print(INFO, "    0x%x - 0x%x %s", p->frame * 0x1000, p->frame * 0x1000 + 0xFFF, p->present ? "[present]" : "");
				}
#endif
			}
		}
	}
	debug_print(INFO, " ---- [done]");
}

void
switch_page_directory(
		page_directory_t * dir
		) {
	current_directory = dir;
	asm volatile ("mov %0, %%cr3":: "r"(dir->physical_address));
	uint32_t cr0;
	asm volatile ("mov %%cr0, %0": "=r"(cr0));
	cr0 |= 0x80000000;
	asm volatile ("mov %0, %%cr0":: "r"(cr0));
}

page_t *
get_page(
		uintptr_t address,
		int make,
		page_directory_t * dir
		) {
	address /= 0x1000;
	uint32_t table_index = address / 1024;
	if (dir->tables[table_index]) {
		return &dir->tables[table_index]->pages[address % 1024];
	} else if(make) {
		uint32_t temp;
		dir->tables[table_index] = (page_table_t *)kvmalloc_p(sizeof(page_table_t), (uintptr_t *)(&temp));
		memset(dir->tables[table_index], 0, sizeof(page_table_t));
		dir->physical_tables[table_index] = temp | 0x7; /* Present, R/w, User */
		return &dir->tables[table_index]->pages[address % 1024];
	} else {
		return 0;
	}
}

void
page_fault(
		struct regs *r)  {
	uint32_t faulting_address;
	asm volatile("mov %%cr2, %0" : "=r"(faulting_address));

	if (r->eip == SIGNAL_RETURN) {
		return_from_signal_handler();
	} else if (r->eip == THREAD_RETURN) {
		debug_print(INFO, "Returned from thread.");
		kexit(0);
	}

#if 1
	int present  = !(r->err_code & 0x1) ? 1 : 0;
	int rw       = r->err_code & 0x2    ? 1 : 0;
	int user     = r->err_code & 0x4    ? 1 : 0;
	int reserved = r->err_code & 0x8    ? 1 : 0;
	int id       = r->err_code & 0x10   ? 1 : 0;

	kprintf("\033[1;37;41mSegmentation fault. (p:%d,rw:%d,user:%d,res:%d,id:%d) at 0x%x eip:0x%x pid=%d,%d [%s]\033[0m\n",
			present, rw, user, reserved, id, faulting_address, r->eip, current_process->id, current_process->group, current_process->name);

#endif

	signal_t * sig = malloc(sizeof(signal_t));
	sig->handler = current_process->signals.functions[SIGSEGV];
	sig->signum  = SIGSEGV;
	handle_signal((process_t *)current_process, sig);

}

/*
 * Heap
 * Stop using kalloc and friends after installing the heap
 * otherwise shit will break. I've conveniently broken
 * kalloc when installing the heap, just for those of you
 * who feel the need to screw up.
 */


void
heap_install() {
	heap_end = (placement_pointer + 0x1000) & ~0xFFF;
}

void *
sbrk(
	uintptr_t increment
    ) {
#if 0
	if (current_process) {
		debug_print(INFO, "sbrk [0x%x]+0x%x pid=%d [%s]", heap_end, increment, getpid(), current_process->name);
	}
#endif
	ASSERT((increment % 0x1000 == 0) && "Kernel requested to expand heap by a non-page-multiple value");
	ASSERT((heap_end % 0x1000 == 0)  && "Kernel heap is not page-aligned!");
	ASSERT((heap_end + increment <= KERNEL_HEAP_END - 1) && "The kernel has attempted to allocate beyond the end of its heap.");
	uintptr_t address = heap_end;

	if (heap_end + increment > KERNEL_HEAP_INIT) {
		debug_print(NOTICE, "Hit the end of available kernel heap, going to allocate more (at 0x%x, want to be at 0x%x)", heap_end, heap_end + increment);
		for (uintptr_t i = heap_end; i < heap_end + increment; i += 0x1000) {
			debug_print(INFO, "Allocating frame at 0x%x...", i);
			alloc_frame(get_page(i, 0, kernel_directory), 1, 0);
		}
		debug_print(INFO, "Done.");
	}

	heap_end += increment;
	memset((void *)address, 0x0, increment);
	return (void *)address;
}

