#include <kernel/kernel.h>
#include <asm/descriptors.h>
#include <kernel/trap.h>
#include "memory.h"
#include <string.h>


static struct TSS tss = { 0 };

static struct SegmentDesc gdt[6] = { { 0 } };

static struct PseudoDesc gdt_pd;

/*
 * FIXME: virtual memory eklendikten sonra inline hale getir. Virtual memory ile
 * stack adresi sabit olacagi icin, fonksiyon tamamen sabit olacak.
 */
#include <kernel/trap.h>
extern struct trapframe *curr_registers() {
	return (struct trapframe*)(tss.esp0 - sizeof(struct trapframe));
}

void init_segments(addr_t kernel_stack) {

	memset(gdt, 0, sizeof(gdt));

	ASSERT3(GD_USER_TEXT >> 3, <, 6);
	ASSERT3(GD_USER_DATA >> 3, <, 6);
	ASSERT3(GD_TSS >> 3, <, 6);

	/* kernel code & data segment */
	SegmentDesc_seg(&gdt[GD_KERNEL_TEXT >> 3], STA_X | STA_R, MMAP_SEG_KERNEL_BASE, 0xffffffff, 0);
	SegmentDesc_seg(&gdt[GD_KERNEL_DATA >> 3], STA_W, MMAP_SEG_KERNEL_BASE, 0xffffffff, 0);

	/* user code & data segment */
	SegmentDesc_seg(&gdt[GD_USER_TEXT >> 3], STA_X | STA_R, MMAP_SEG_USER_BASE, 0xbfffffff, 3);
	SegmentDesc_seg(&gdt[GD_USER_DATA >> 3], STA_W, MMAP_SEG_USER_BASE, 0xbfffffff, 3);


	gdt_pd.limit = sizeof(gdt) - 1;
	gdt_pd.base = kaddr2va((addr_t)gdt).a;

	/* Task State Segment */
	tss.esp0 = kernel_stack;
	tss.ss0 = GD_KERNEL_DATA;
	SegmentDesc_seg16(&gdt[GD_TSS >> 3], STS_T32A, kaddr2va((addr_t)&tss).a, sizeof(struct TSS), 0);
	gdt[GD_TSS >> 3].s = 0;
	gdt_load((uint32_t)&gdt_pd);
	tr_load(GD_TSS);

	/* gs ve fs kullanilmiyor */
	load_reg(%gs, GD_USER_DATA | 3);
	load_reg(%fs, GD_USER_DATA | 3);
	load_reg(%es, GD_KERNEL_DATA);
	load_reg(%ds, GD_KERNEL_DATA);
	load_reg(%ss, GD_KERNEL_DATA);
	cs_set(GD_KERNEL_TEXT);
	/* ldt kullanilmiyor */
	asm volatile("lldt %%ax" :: "a" (0));

	// print_info("init_segments OK\n");
}
