#include <kernel/kernel.h>
#include "memory.h"

// memory/pmem.c
extern void pmem_init();
extern void pmem_test();

// memory/vmem.c
extern void vmem_init();
extern void vmem_init_2();
extern addr_t vmem_pdeAlloc();
extern void vmem_test();

// memory/vmalloc.c
extern void vmalloc_init(addr_t pdeVm);
extern void vmalloc_test();

// segments.c
extern void init_segments(addr_t kernel_stack);

// boot.S
extern void *__boot_stack_top;

#include <string.h>

void move_stack(uint32_t ka_curr, uint32_t ka_new, uint32_t size) {
	memcpy((void*)ka_new, (void*)ka_curr, size);

	uint32_t esp; read_reg(%esp, esp);
	uint32_t ebp; read_reg(%ebp, ebp);

	print_info("%x %x\n", esp, ebp);

	uint32_t newEsp = ka_new + esp - ka_curr;
	uint32_t newEbp = ka_new + ebp - ka_curr;

	print_info("%x %x\n", newEsp, newEbp);

	load_reg(%esp, ka_new + esp - ka_curr);
	load_reg(%ebp, ka_new + ebp - ka_curr);

}


void memory_init(int runTest) {

	pmem_init();
	print_info("pmem_init OK\n");
	/* pmem_test(); */

	vmem_init();
	print_info("vmem_init OK\n");

	init_segments((uint32_t)&__boot_stack_top);

	vmem_init_2();
	print_info("vmem_init_2 OK\n");

	addr_t pdeVm = vmem_pdeAlloc();
	vmalloc_init(pdeVm);
	print_info("vmalloc_init OK\n");


#if 0

	va_t kernelStack = (va_t){MMAP_KERNEL_STACK_TOP};
	print_info(">> kernelStack: %x -> %x\n", (uint32_t)&__boot_stack_top, va2kaddr(kernelStack));
	init_segments(va2kaddr((va_t){MMAP_KERNEL_STACK_TOP}));

	va_t addr;
	for (addr.a = MMAP_KERNEL_STACK_BASE ; addr.a <= MMAP_KERNEL_STACK_TOP ; addr.a += 0x1000) {
		PmemFrame_t * frame;
		int r = pmem_frameAlloc(&frame);
		ASSERT(r == 0);

		print_info(">> map %x\n", addr.a);
		vmem_map(frame, addr, PTE_W | PTE_P);
	}



	move_stack((uint32_t)&__boot_stack_top + (0x4000-MMAP_KERNEL_STACK_SIZE),
			   va2kaddr((va_t){MMAP_KERNEL_STACK_BASE}), MMAP_KERNEL_STACK_SIZE);


	while(1);
#endif

	//vmalloc_test();
	// while(1);

	if (runTest) {
		print_info("**** vmem_test *****\n");
		vmem_test();
		print_info("test 1 end\n\n");
		vmem_test();
		print_info("test end\n\n");
		while(1);
	}
}
