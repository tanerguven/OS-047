#include <kernel/kernel.h>
#include <kernel/multiboot.h>
#include <errno.h>
#include <util.h>

#include "memory.h"

/* public */ void pmem_init();
/* public */ int pmem_refcountDec(PmemFrame_t * f);
/* public */ int pmem_frameAlloc(PmemFrame_t ** frame);
/* public */ addr_t pmem_getKernelMemEnd(PmemFrame_t ** frame);
/* public */ PmemFrame_t * pmem_getFrame(addr_t frameAddr);

/* link.ld */
extern void* __kernel_end;

static struct {
	PmemFrame_t * frames;
	uint32_t frameCount;
	addr_t kernelMemEnd; /* kernele ayrilan alanin sonu */

	PmemFrame_t * firstFreeFrame;
	PmemFrame_t * lastFreeFrame;
	size_t freeFrameCount;

	addr_t initrdStart;
	addr_t initrdEnd;
} pmem = { 0 };

const PmemFrame_t * pmem_frames_const = NULL;

addr_t pmem_getKernelMemEnd(PmemFrame_t ** frame) {
	return pmem.kernelMemEnd;
}

PmemFrame_t * pmem_getFrame(addr_t frameAddr) {
	return &pmem.frames[frameAddr/0x1000];
}

void pmem_getInitrdAddr(addr_t * start, size_t * size) {
	*start = pmem.initrdStart;
	*size = pmem.initrdEnd - pmem.initrdStart;
}

static inline void pmem_frameInit(PmemFrame_t * f, int type, uint32_t refCount) {
	f->free = 0;
	f->type = type;
	f->n = refCount;
}

static inline PmemFrame_t * pmem_nextFreeFrame(PmemFrame_t *f) {
	return (f->free) ? (pmem.frames + f->n) : NULL;
}


static inline void pmem_putFreeFrame(PmemFrame_t * frame) {
	// ASSERT3(pmem_frameIndex(f), !=, 0);
	ASSERT_DTEST(frame->free);
	ASSERT3_DTEST(frame->n, ==, 0);

	if (pmem.firstFreeFrame == NULL) {
		pmem.firstFreeFrame = pmem.lastFreeFrame = frame;
	} else {
		pmem.lastFreeFrame->n = pmem_frameIndex(frame);
		pmem.lastFreeFrame = frame;
	}
	pmem.freeFrameCount++;
}

static PmemFrame_t * pmem_getFreeFrame() {
	PmemFrame_t * f = pmem.firstFreeFrame;
	if (f == NULL)
		return NULL;

	if (pmem.lastFreeFrame == f)
		pmem.lastFreeFrame = pmem.firstFreeFrame = NULL;
	else
		pmem.firstFreeFrame = pmem_nextFreeFrame(f);

	pmem.freeFrameCount--;

	f->n = 0; // refcount = 0
	return f;
}

int pmem_frameAlloc(PmemFrame_t ** frame) {
	// FIXME: synchronization

	PmemFrame_t * newFrame;

	/* get free fame */
	newFrame = pmem_getFreeFrame();
	if (newFrame == NULL) {
		return -ENOMEM;
	}

	ASSERT_DTEST(newFrame->free);
	ASSERT3_DTEST(newFrame->type, ==, pmem_frame_type_AVAILABLE);
	newFrame->free = 0;
	*frame = newFrame;

	ASSERT(newFrame != pmem_frames_const);
	// ASSERT_DTEST(pmem_frameIndex(newFrame) != 0);

	return 0;
}


int pmem_refCountDec(PmemFrame_t * frame) {
	// FIXME: synchronization

	ASSERT_DTEST(!frame->free);
	ASSERT_DTEST(pmem_frameIndex(frame) != 0);
	if (--frame->n == 0) {
		frame->free = 1;
		pmem_putFreeFrame(frame);
		return 0;
	}

	ASSERT3_DTEST(frame->n, >, -1);

	return frame->n;
}

static void pmem_detect() {
	int i;

	size_t ramSize = (_multiboot_info->mem_upper) + (1<<10);

	if (_multiboot_info->mods_count == 1) {
		multiboot_module_t *a = (multiboot_module_t*)_multiboot_info->mods_addr;
		pmem.initrdStart = a[0].mod_start;
		pmem.initrdEnd = a[0].mod_end;
		print_info(">> initrd: %08x - %08x\n", pmem.initrdStart, pmem.initrdEnd);
	} else if (_multiboot_info->mods_count > 1) {
		PANIC("birden fazla initrd desteklenmiyor");
	} else {
		PANIC("initrd not found\n");
	}

	/* pages'i kernelin bittiği noktadan başlat */
	if (pmem.initrdEnd > (uint32_t)&__kernel_end)
		pmem.frames = (PmemFrame_t*)roundUp(pmem.initrdEnd);
	else
		pmem.frames = (PmemFrame_t*)roundUp((addr_t)&__kernel_end);

	pmem_frames_const = pmem.frames;

	pmem.frameCount = ramSize >> 2;
	pmem.kernelMemEnd = roundUp((addr_t)(pmem.frames + pmem.frameCount));

	for (i = 0 ; i < pmem.frameCount ; i++)
		pmem_frameInit(&pmem.frames[i], pmem_frame_type_UNKNOWN, 1);

	print_info("memory size: %d KB\n", pmem.frameCount * 4);
}


void pmem_init() {
	int i;
	int r;

	ASSERT(sizeof(PmemFrame_t) == 4);
	ASSERT(pmem.frames == 0);
	ASSERT(pmem.frameCount == 0);

	pmem_detect();

	multiboot_memory_map_t *mmap = (multiboot_memory_map_t*)_multiboot_info->mmap_addr;

	if (!(_multiboot_info->flags & MULTIBOOT_INFO_MEM_MAP)) {
		print_error("0x%08x\n", _multiboot_info);
		PANIC("multiboot memory map okunamiyor");
	}

	while ((uint32_t)mmap < _multiboot_info->mmap_addr + _multiboot_info->mmap_length) {
		uint32_t start = roundUp(mmap->addr_low);
		uint32_t end = roundDown(mmap->addr_low + mmap->len_low);

		/* FIXME: grub multiboot bilgilerinde sonda start=end=0 var
		   arada start=end=0 olabilir mi? kontrol et */
		if (start == 0 && start == end)
			break;

		if (mmap->type == MULTIBOOT_MEMORY_AVAILABLE) {
			/* 4 GB'dan buyuk fiziksel adresleri yoksay */
			if (mmap->addr_high > 0)
				break;

			for (i = start ; i < end ; i += 0x1000) {
				if (i < 0x100000) {
					/* low memorydeki alani uygun fakat dolu olarak isaretle */
					pmem_frameInit(&pmem.frames[i>>12], pmem_frame_type_AVAILABLE, 1);
				} else if (i < pmem.kernelMemEnd) {
					/* page type kernel */
					pmem_frameInit(&pmem.frames[i>>12], pmem_frame_type_KERNEL, 1);
				} else {
					/* bos alan, bastan dolu isaretle sonra reference count eksilt */
					pmem_frameInit(&pmem.frames[i>>12], pmem_frame_type_AVAILABLE, 1);
					r = pmem_refCountDec(&pmem.frames[i>>12]);
					ASSERT3(r, ==, 0);
				}
			}
		}

		mmap = (multiboot_memory_map_t*)((uint32_t)mmap + mmap->size + sizeof(mmap->size));
	}


	print_info("pmem datastructure size: %d KB\n", (int)(pmem.kernelMemEnd-(int)__kernel_end)/1024);
	print_info("pmem.freeFrameCount: %d\n", pmem.freeFrameCount);
	print_info("free memory: %d KB\n", pmem.frameCount * 4);
}


// #ifdef TEST_ENABLED

static void __pmem_test() {
	int i, r, j;
	PmemFrame_t * frame = NULL;
	PmemFrame_t * testListFirst = NULL;
	addr_t testAddr;

	uint32_t freeFrameCount = pmem.freeFrameCount;

	i = 0;
	while (pmem.freeFrameCount > 0) {
		i++;
		r = pmem_frameAlloc(&frame);
		ASSERT3(r, ==, 0);
		ASSERT3(frame->type, ==, pmem_frame_type_AVAILABLE);
		ASSERT3(frame->free, ==, 0);
		ASSERT3(frame->n, ==, 0);

		if (testListFirst != NULL) {
			frame->n = ((uint32_t)testListFirst-(uint32_t)pmem.frames) / sizeof(PmemFrame_t);
		}
		testListFirst = frame;

		testAddr = ((uint32_t)frame - (uint32_t)pmem.frames) / sizeof(PmemFrame_t);

		for (j = 0 ; j < 1024 ; j++) {
			uint32_t * writeAddr = ((uint32_t*)testAddr) + i;
			*writeAddr = 0xffffffff;
			ASSERT3(*writeAddr, ==, 0xffffffff);
			((uint32_t*)testAddr)[i] = 0x00000000;
			ASSERT3(*writeAddr, ==, 0x00000000);
		}
	}

	ASSERT(pmem.firstFreeFrame == NULL);
	ASSERT(pmem.lastFreeFrame == NULL);

	while (testListFirst != NULL) {
		ASSERT3(pmem.freeFrameCount, <, freeFrameCount);

		frame = testListFirst;
		ASSERT(frame != NULL);

		if (frame->n == 0)
			testListFirst = NULL;
		else
			testListFirst = &pmem.frames[frame->n];

		frame->free = 0;
		frame->n = 1; // referenceCount = 1

		r = pmem_refCountDec(frame);
		ASSERT3(r, ==, 0);
		ASSERT3(frame->type, ==, pmem_frame_type_AVAILABLE);
		ASSERT3(frame->free, ==, 1);
		ASSERT3(frame->n, ==, 0);
	}

	ASSERT3(pmem.freeFrameCount, ==, freeFrameCount);
}

void pmem_test() {
	int i;
	for (i = 0 ; i < 2 ; i++)
		__pmem_test();

	print_info("pmem_test OK\n");
}

// #endif /* TEST_ENABLED */
