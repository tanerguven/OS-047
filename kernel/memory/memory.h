#ifndef _MEMORY_MEMORY_H_
#define _MEMORY_MEMORY_H_

#include <types.h>

/*------------------------------*
 *			pmem.c				*
 *------------------------------*/

#define pmem_frame_type_UNKNOWN 0
#define pmem_frame_type_AVAILABLE 1
#define pmem_frame_type_KERNEL 2

typedef struct {
	uint32_t n : 24; /* (free) ? next free index : reference count */
	uint32_t free : 1;
	uint32_t type: 2;
	uint32_t __ : 5; /* unused */
} PmemFrame_t;

extern void pmem_init();
extern int pmem_refCountDec(PmemFrame_t * f);
extern int pmem_frameAlloc(PmemFrame_t ** frame);
extern addr_t pmem_getKernelMemEnd();
extern PmemFrame_t * pmem_getFrame(addr_t frameAddr);
extern void pmem_getInitrdAddr(addr_t * start, size_t * size);

extern const PmemFrame_t * pmem_frames_const;

static inline int pmem_frameIndex(PmemFrame_t * frame) {
	return ((addr_t)frame-(addr_t)pmem_frames_const) / sizeof(PmemFrame_t);
}


static inline addr_t pmem_frameAddr(PmemFrame_t * frame) {
	return pmem_frameIndex(frame) << 12;
}

/*------------------------------*
 *			vmem.c				*
 *------------------------------*/

/* Present */
#define PTE_P	0x001
/* Writable */
#define PTE_W	0x002
/* User */
#define PTE_U	0x004
/* Write-Through */
#define PTE_WT	0x008
/* Cache-Disable */
#define PTE_CD	0x010
/* Accessed */
#define PTE_A	0x020
/* Dirty */
#define PTE_D	0x040

/* page directory entry */
typedef struct PDE {
	uint32_t present: 1;
	uint32_t writable: 1;
	uint32_t user: 1;
	uint32_t write_throught: 1;
	uint32_t cache_disable: 1;
	uint32_t accessed: 1;
	uint32_t __6: 1;
	uint32_t page_size: 1;
	uint32_t __8: 1;
	uint32_t __9_11: 3;
	uint32_t pagetable_pageindex: 20;
} __attribute__((packed)) PDE_t;

/* page table entry */
typedef struct PTE {
	uint32_t present: 1;
	uint32_t rw: 1;
	uint32_t user: 1;
	uint32_t write_throught: 1;
	uint32_t cache_disable: 1;
	uint32_t accessed: 1;
	uint32_t dirty: 1;
	uint32_t __7: 1;
	uint32_t global: 1;
	uint32_t __9_11: 3;
	uint32_t pageindex: 20;
} __attribute__((packed)) PTE_t;

typedef struct PageDirectory {
	PDE_t e[1024];
} PageDirectory_t;

typedef struct PageTable {
	PTE_t e[1024];
} PageTable_t;


typedef struct {
	uint32_t pagedir_pa; // physical address
	PageDirectory_t* pagedir;
	PageTable_t** pagetables;

	addr_t lastAddr;
	size_t countProgramPage;
} TaskMemory_t;

static inline int PTE_getPerm(PTE_t *p) {
	return (*(uint32_t*)p) & 0xfff;
}

/* virtual address page directory index */
static inline uint32_t VM_PDX(addr_t va) {
	return (va >> 22) & 0x3ff;
}

/* virtual address page table index */
static inline uint32_t VM_PTX(addr_t va) {
	return (va >> 12) & 0x3ff;
}

/* page directory index -> virtual adress */
static inline addr_t VM_PDX_start(uint32_t pdx) {
	return pdx << 22;
}

static inline void PDE_set(PDE_t *p, uint32_t pageindex, int perm) {
	*(uint32_t*)p = ((pageindex<<12) & 0xfffff000) | (perm & 0xfff);
}

static inline void PTE_set(PTE_t *p, uint32_t pageindex, int perm) {
	*(uint32_t*)p = ((pageindex<<12) & 0xfffff000) | (perm & 0xfff);
}

extern void vmem_map_4MB(va_t pdeVm, va_t vm);
extern void vmem_map(TaskMemory_t * mem, PmemFrame_t * frame, va_t va, int perm);
extern int vmem_getPerm(TaskMemory_t * mem, va_t addr);
extern int vmem_unmap(TaskMemory_t * mem, va_t va);
extern int vmem_unmap_4MB(va_t pdeVm, va_t va);

extern PDE_t vmem_getPDE(TaskMemory_t * mem, va_t va);
extern PTE_t vmem_getPTE(TaskMemory_t * mem, va_t va);
extern int vmem_changePerm(TaskMemory_t * mem, va_t va, int perm);
extern PmemFrame_t * vmem_getFrame(TaskMemory_t * mem, va_t va);

extern void vmem_setTask0(TaskMemory_t * taskMem);
extern void vmem_fork(TaskMemory_t * srcMem, TaskMemory_t * newMem);
extern void vmem_copyPage(TaskMemory_t * srcMem, PmemFrame_t * dstFrame, va_t src);

extern void * kmalloc();

/*------------------------------*
 *			vmalloc.c			*
 *------------------------------*/

extern int __vmalloc(uint32_t pdeVm, addr_t * addr, uint32_t * size);
extern addr_t vmalloc(uint32_t pageCount);
extern int vfree(addr_t addr);
extern int __vfree(addr_t pdeVm, addr_t addr);
extern void vmalloc_status(size_t * freeSize, size_t * freeBlockCount);


/*------------------------------*
 *								*
 *------------------------------*/


#define MMAP_KERNEL_BASE 0xC0000000
#define MMAP_KERNEL_STACK_TOP 0xF0000000
#define MMAP_USER_BASE 0x00000000
#define MMAP_USER_STACK_TOP 0xB0000000


#define MMAP_KERNEL_STACK_SIZE 0x2000
#define MMAP_KERNEL_STACK_BASE (MMAP_KERNEL_STACK_TOP - MMAP_KERNEL_STACK_SIZE)



#ifndef MMAP_SEG_KERNEL_BASE
# define MMAP_SEG_KERNEL_BASE MMAP_KERNEL_BASE
#endif
#ifndef MMAP_SEG_USER_BASE
# define MMAP_SEG_USER_BASE MMAP_USER_BASE
#endif


static inline uint32_t va2kaddr(va_t addr) {
	return addr.a - MMAP_SEG_KERNEL_BASE;
}

static inline va_t uaddr2va(uint32_t addr) {
	return (va_t){addr + MMAP_SEG_USER_BASE};
}

static inline va_t kaddr2va(uint32_t addr) {
	return (va_t){ addr + MMAP_SEG_KERNEL_BASE };
}

static inline uint32_t va2uaddr(va_t addr) {
	return addr.a - MMAP_SEG_USER_BASE;
}


static inline uint32_t uaddr2kaddr(uint32_t addr) {
	return va2kaddr(uaddr2va(addr));
}



static inline uint32_t kaddr2uaddr(uint32_t addr) {
	return va2uaddr(kaddr2va(addr));
}


static inline void testAddr(void * addr) {
	volatile uint32_t * x = (uint32_t*)addr;
	volatile uint32_t y = x[0];
	x[0] = y;
}

#define ASSERT_KERNEL_ADDR(addr) do {		 \
	ASSERT3((uint32_t)addr, <=, 0x40000000); \
	} while(0)

#endif /* _MEMORY_MEMORY_H_ */
