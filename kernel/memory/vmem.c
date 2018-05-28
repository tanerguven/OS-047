#include <types.h>
#include <string.h>
#include <errno.h>
#include <kernel/kernel.h>
#include <kernel/trap.h>
#include <data_structures/list2.h>
#include <util.h>

#include "memory.h"

/* Paging modunda, kullanilan Page Directory Entry (PDE)'lere erisilebilmesi icin onlarin da bellekte saklanmasi gerek.
   Recursive bir durum var.

   EmptyPdeFrame_t : PDE'leri map etmek icin kullanilabilecek pageler.
     - 4 MB'lik parcalar halinde, listede saklaniyor
	 - Bir EmptyPdeFrame_t icerisinde 1024 tane page sakliyor.
	 - EmptyPdeFrame_t.status dizisi linked list mantiginda. Her eleman sonrakinin indexini gosteriyor.
 */
DEFINE_LIST(emptyPdeFrames);
#define KEY_1 0x13579246
#define KEY_2 0x24680135
typedef struct {
	uint32_t key1; // debug icin

	/* bostaki page sayisi */
	size_t emptyCount;

	/* kendi icindeki 1024 elemani yonetmek icin dizi seklinde linked list
	   >= 0 : sonrakinin indexi
	   -1 : sonraki NULL ise
	   -2 : eleman listede degil */
	int16_t first;
	int16_t last;
	int16_t status[1024];
	/* diger EmptyPdeFrame_t'ler ile baglanti icin linked list */
	li_emptyPdeFrames_node lin;
	uint32_t key2; // debug icin
} EmptyPdeFrame_t;
DEFINE_LIST_OFFSET(emptyPdeFrames, EmptyPdeFrame_t, lin);

/* EmptyPdeFrame_t'nin init fonksiyonu */
void EmptyPdeFrame_init(EmptyPdeFrame_t * e) {
	e->key1 = KEY_1;
	e->key2 = KEY_2;
	e->emptyCount = 0;
	e->first = -1;
	e->last = -1;
	memset(&e->lin, 0, sizeof(e->lin));
}

static li_emptyPdeFrames emptyPdeFrame_list;

/* link.ld'de belirlenen kernelin bellekte yerlesim adresleri */
extern void *__text_start;
extern void *__rodata_end;
extern void *__data_end;

static TaskMemory_t kvmem;
/* baslangicta statik olarak olusturulan 2 tane EmptyPdeFrame_t'nin adresi */
static EmptyPdeFrame_t * reserved;

static uint32_t mappedPageCount = 0;

static va_t copyPage;

// vmalloc.c'de bulunan fonksiyon. vmem_pdeFree isleminde gerekli
extern int VmallocBlockInfo_getStartAddr(addr_t addr, addr_t * start, size_t * size);


/* ilk task olusturulurken page tablolarinin atamasi */
void vmem_setTask0(TaskMemory_t * taskMem) {
	taskMem->pagedir_pa = kvmem.pagedir_pa;
	taskMem->pagedir = kvmem.pagedir;
	taskMem->pagetables = kvmem.pagetables;
}

#include "../task.h"

void * kmalloc(size_t size) {
	PmemFrame_t * frame;
	int r;

	size = roundUp(size);
	ASSERT((size % 0x1000) == 0);

	int pageCount = size/0x1000;
	addr_t addr = vmalloc(pageCount);
	if (addr == 0)
		return NULL;

	int i;
	for (i = 0 ; i < pageCount ; i++) {
		r = pmem_frameAlloc(&frame);
		if (r != 0)
			goto error;
		ASSERT(kvmem.pagetables[VM_PDX(kaddr2va(addr+i*0x1000).a)] != NULL);
		// print_info("%x\n", kaddr2va(addr+i*0x1000).a);

		vmem_map(&kvmem, frame, kaddr2va(addr+i*0x1000), PTE_W | PTE_P);
		Task_t * t = getActiveTask();
		ASSERT(t != NULL);
		ASSERT(t->mem.pagetables != NULL);
		ASSERT(t->mem.pagetables[VM_PDX(kaddr2va(addr+i*0x1000).a)] != NULL);
	}

	return (void*)addr;

error:
	for ( i = i-1 ; i >= 0 ; i--)
		vmem_unmap(&kvmem, kaddr2va(addr+i*0x1000));
	return NULL;
}

/* frame ve addr allocate edip map eder */
void kmalloc2(TaskMemory_t * mem, PmemFrame_t ** frame, addr_t * addr) {
	int r;
	r = pmem_frameAlloc(frame);
	ASSERT(r == 0);
	*addr = vmalloc(1);
	vmem_map(mem, *frame, kaddr2va(*addr), PTE_W | PTE_P);
}

void vmem_copyPage(TaskMemory_t * srcMem, PmemFrame_t * dstFrame, va_t src) {
	ASSERT3(dstFrame->n, >, 0);

	int n = dstFrame->n;
	vmem_map(srcMem, dstFrame, copyPage, PTE_P | PTE_W);
	tlb_invalidate(va2kaddr(copyPage));

	memcpy((void*)va2kaddr(copyPage), (void*)va2kaddr(src), 0x1000);
	vmem_unmap(srcMem, copyPage);
	ASSERT3(n, ==, dstFrame->n);
}

/* fork sirasindaki bellek islemleri */
void vmem_fork(TaskMemory_t * srcMem, TaskMemory_t * newMem) {
	int i;
	int r;

	PmemFrame_t * frame;
	addr_t addr;

	// page directory
	kmalloc2(&kvmem, &frame, &addr);
	newMem->pagedir_pa = pmem_frameAddr(frame);
	newMem->pagedir = (PageDirectory_t*)addr;
	memset(newMem->pagedir, 0, sizeof(PageDirectory_t));

	// page tables
	kmalloc2(&kvmem, &frame, &addr);
	newMem->pagetables = (PageTable_t**)addr;
	memset(newMem->pagetables, 0, 1024 * sizeof(PageTable_t*));

	// link kernel pages
	for (i = VM_PDX(MMAP_KERNEL_BASE) ; i < VM_PDX(MMAP_KERNEL_STACK_BASE) ; i++) {
		/* if (vmem.pagetables[i] != NULL) */
		/* 	print_info("%x - %x\n", i*0x1000*1024, (i+1)*0x1000*1024); */

		if (kvmem.pagetables[i] != NULL) {
			newMem->pagedir->e[i] = kvmem.pagedir->e[i];
			newMem->pagetables[i] = kvmem.pagetables[i];
		}
	}

	for (i = VM_PDX(MMAP_USER_BASE) ; i < VM_PDX(MMAP_USER_STACK_TOP) ; i++) {
		/* if (vmem.pagetables[i] != NULL) */
			/* print_info("%x - %x\n", i*0x1000*1024, (i+1)*0x1000*1024); */

		if (srcMem->pagetables[i] != NULL) {
			print_info("%x - %x\n", i*0x1000*1024, (i+1)*0x1000*1024);

			int j;
			for (j = 0 ; j < 1024 ; j++) {
				if (!srcMem->pagetables[i]->e[j].present)
					continue;

				va_t va = (va_t) { i * 1024*0x1000 + j*0x1000 };
				int perm = PTE_getPerm(&srcMem->pagetables[i]->e[j]);

				/* page al & map et */
				PmemFrame_t * frame;
				r = pmem_frameAlloc(&frame);
				ASSERT(r == 0);
				vmem_map(newMem, frame, va, perm);
				ASSERT(frame->n == 1);

				/* kopyala */
				vmem_copyPage(srcMem, frame, va);
			}
		}
	}
}

#if 0
uint32_t vmem_getUserPageCount(TaskMemory_t * mem) {
	uint32_t count = 0;
	for (i = VM_PDX(MMAP_USER_BASE) ; i < VM_PDX(MMAP_USER_STACK_TOP) ; i++) {
		if (mem->pagetables[i] != NULL) {
			for (j = 0 ; j < 1024 ; j++) {
				if (mem->pagetables[i]->e[j].present)
					count++
			}
		}
	}
	return count;
}
#endif

extern uint32_t asmstack;

int vmem_saveUserMemory(TaskMemory_t * mem, void * buf, size_t size, addr_t * start) {
	int i, j;

	char * b = (char*)buf;
	size_t w = 0;

	for (i = VM_PDX(MMAP_USER_BASE) ; i < VM_PDX(MMAP_USER_STACK_TOP) ; i++) {
		if (mem->pagetables[i] != NULL) {

			if ((w + 0x2000) > *start+size)
				break;

			print_info("%d: ", i);
			for (j = 0 ; j < 1024 ; j++) {
				if (mem->pagetables[i]->e[j].present) {
					// memset(&b[w], 0, 4);
					addr_t addr = i*1024*0x1000 + j*0x1000;
					print_info("%x ", addr);

					*(addr_t*)(&b[w]) = addr | PTE_getPerm(&mem->pagetables[i]->e[j]);
					w += 4;

					// memset(&b[w], 0, 0x1000);
					memcpy(&b[w], (void*)va2kaddr((va_t){addr}), 0x1000);
					w += 0x1000;

				}
			}
			print_info("\n");
		}
	}

	ASSERT(b[0] != 0);

	*start += w;


	if (i == VM_PDX(MMAP_USER_STACK_TOP))
		return 1;

	return 0;
}

void vmem_loadUserMemory(TaskMemory_t * mem, void * buf, size_t size) {
	size_t w = 0;
	char * b = (char*)buf;
	int r;

	uint32_t cr3;
	read_reg(%cr3, cr3);
	load_reg(%cr3, mem->pagedir_pa);

	while (w < size) {
		/* adres ve izinleri oku */
		va_t addr = (va_t){ (*(addr_t*)&b[w]) & 0xfffff000 };
		int perm = (*(addr_t*)&b[w]) & 0xfff;
		w += 4;
		ASSERT(addr.a >= MMAP_USER_BASE && addr.a < MMAP_USER_STACK_TOP);
		ASSERT3(addr.a, >=, 0x1000);

		// print_info("[%x] %x\n", addr.a, perm);

		/* page al */
		PmemFrame_t * frame;
		r = pmem_frameAlloc(&frame);
		ASSERT(r == 0);
		vmem_map(mem, frame, addr, perm);

		/* kopyala */
		// print_info("memcpy: %x %x\n", addr.a, va2kaddr(addr));
		memcpy((void*)va2kaddr(addr), &b[w], 0x1000);
		w += 0x1000;
	}

	load_reg(%cr3, cr3);
}

void vmem_init() {
	PmemFrame_t * frame;
	int i, r;

	PmemFrame_t * reservedFrame[3];
	addr_t reservedVa[3];

	ASSERT3(sizeof(EmptyPdeFrame_t), <, 0x1000);
	li_emptyPdeFrames_init(&emptyPdeFrame_list);

	// reserved copy page
	PmemFrame_t * copyPageFrame;
	r = pmem_frameAlloc(&copyPageFrame);
	copyPageFrame->n = 1;
	copyPageFrame->free = 0;
	ASSERT(r == 0);
	copyPage = (va_t){ pmem_frameAddr(copyPageFrame) };
	// memset(copyPage, 0, 0x1000);

	// page directory
	r = pmem_frameAlloc(&frame);
	ASSERT(r == 0);
	kvmem.pagedir_pa = pmem_frameAddr(frame);
	kvmem.pagedir = (PageDirectory_t*)kvmem.pagedir_pa;
	memset(kvmem.pagedir, 0, 0x1000);

	// page tables
	r = pmem_frameAlloc(&frame);
	ASSERT(r == 0);
	kvmem.pagetables = (PageTable_t**)(pmem_frameAddr(frame));
	memset(kvmem.pagetables, 0, 0x1000);

	// reserve 3 vm pages
	for (i = 0 ; i < 3 ; i++) {
		r = pmem_frameAlloc(&reservedFrame[i]);
		ASSERT(r == 0);
		reservedFrame[i]->n = 1;
		reservedVa[i] = pmem_frameAddr(reservedFrame[i]);
	}

	/* paging icin kullanilan pde'ler de fiziksel page harcadigi icin
	   pde sayisi da dusunulerek bire bir map edilecek page sayisi
	   hesaplaniyor */
	// 3 reserved, 1 copyPage
	uint32_t page_count = pmem_getKernelMemEnd() / 0x1000 + 1 + 3 + 1;
	uint32_t pde_count = page_count / 1024 + 1;
	/* pde'lerin page'leri eklendiginde pde sayisi yetmiyorsa
	   pde sayisini arttir */
	if (pde_count * 1024 < page_count + pde_count)
		pde_count++;
	/* pde sayisini ekle */
	page_count += pde_count;

	/* page'ler page_table'lara sigmak zorunda */
	ASSERT3(page_count, <=, pde_count * 1024);

	addr_t lastAddr = 0;

	/* kernel kismini map et */
	mappedPageCount = 0;
	for (i = 0 ; i < pde_count ; i++) {
		int j;

		/* page tablenin saklanacagi fiziksel page */
		r = pmem_frameAlloc(&frame);
		ASSERT(r == 0);

#if MMAP_SEG_KERNEL_BASE != 0
		/* kernel segmentation kullanilarak calistirilacaksa */
		PDE_set( &kvmem.pagedir->e[i+VM_PDX(MMAP_KERNEL_BASE)],
				 pmem_frameIndex(frame), PTE_P | PTE_W );
		kvmem.pagetables[i+VM_PDX(MMAP_KERNEL_BASE)] = (PageTable_t*)pmem_frameAddr(frame);
#endif

		/* page table icin alinan bellegi page table olarak map et */
		PDE_set( &kvmem.pagedir->e[i], pmem_frameIndex(frame), PTE_P | PTE_W );
		print_info("PDE_set %d\n", i);
		kvmem.pagetables[i] = (PageTable_t*)pmem_frameAddr(frame);

		/* kullanilan fiziksel bellegi 1-1 map et */
		for (j = 0 ; j < 1024 ; j++) {
			if (mappedPageCount > page_count) {
				PTE_set(&kvmem.pagetables[i]->e[j], 0, 0);
			} else {
				uint32_t pageindex = (i * 1024 + j);
				uint32_t pa = pageindex << 12;
				lastAddr = pa;

				if ((pa >= (uint32_t)&__text_start) && (pa < (uint32_t)&__rodata_end)) {
					/* text ve read only data bolumlerine yazma izni yok */
					PTE_set( &kvmem.pagetables[i]->e[j], pageindex, PTE_P);
				} else {
					PTE_set( &kvmem.pagetables[i]->e[j], pageindex, PTE_P | PTE_W);
				}
				mappedPageCount++;
			}
		}
		/* */
	}

	// print_info("page_count: %d\n", page_count);
	// print_info("pde_count: %d\n", pde_count);

	// kontrol
	r = pmem_frameAlloc(&frame);
	ASSERT(r == 0);
	ASSERT3(pmem_frameAddr(frame), ==, page_count*0x1000+0x1000);
	ASSERT3(pmem_frameAddr(frame), ==, lastAddr + 0x1000);
	frame->n = 1;
	pmem_refCountDec(frame);

	// print_info("load cr3 OK\n");

	// print_info("a: %d KB\n", page_count * 4);

#if 0
	uint32_t reserved_1_start = page_count * 0x1000;
	uint32_t reserved_2_start = (reserved_1_start + 1024*0x1000) & ~(1024*0x1000-1);
	uint32_t reserved_2_end = reserved_2_start + 1024*0x1000;
	print_info("reserved vmem : %x - %x\n", reserved_1_start, reserved_2_start);
	print_info("reserved vmem : %x - %x\n", reserved_2_start, reserved_2_end);
#endif

	ASSERT(sizeof(EmptyPdeFrame_t) < 0x1000-256);

	reserved = (EmptyPdeFrame_t *)reservedVa[0];
	EmptyPdeFrame_init(reserved);
	reserved->emptyCount = 2;
	reserved->first = 1;
	reserved->last = 2;
	for (i = 0 ; i < 1024 ; i++)
		reserved->status[i] = -1;
	reserved->status[1] = 2;


	PTE_set( &kvmem.pagetables[VM_PDX(reservedVa[1])]->e[VM_PTX(reservedVa[1])], 0, 0);
	pmem_refCountDec(reservedFrame[1]);
	ASSERT(reservedFrame[1]->free == 1);

	pmem_refCountDec(reservedFrame[2]);
	PTE_set( &kvmem.pagetables[VM_PDX(reservedVa[2])]->e[VM_PTX(reservedVa[2])], 0, 0);
	ASSERT(reservedFrame[2]->free == 1);

	ASSERT3(emptyPdeFrame_list.size, ==, 0);
	r = li_emptyPdeFrames_push_front(&emptyPdeFrame_list, &reserved->lin);
	ASSERT(r == 0);
	ASSERT3(emptyPdeFrame_list.size, ==, 1);

	print_info("%x %x %x %x\n", reserved, reserved->key1, reserved->key2, reserved->emptyCount);

	pmem_refCountDec(copyPageFrame);
	PTE_set( &kvmem.pagetables[VM_PDX(copyPage.a)]->e[VM_PTX(copyPage.a)], 0, 0);
	ASSERT(copyPageFrame->free == 1);

	// FIXME: --
	// r = pmem_frameAlloc(&frame);
	// PTE_set( &vmem.pagetables[0]->e[0], pmem_frameIndex(frame), PTE_P);
	ASSERT(kvmem.pagetables[0] != NULL);
	PTE_set( &kvmem.pagetables[0]->e[0], 0, 0);

	/* enable paging */
	load_reg(%cr3, kvmem.pagedir_pa);
	uint32_t cr0; read_reg(%cr0, cr0);
	cr0 |= CR0_PE|CR0_PG|CR0_AM|CR0_NE|CR0_TS|CR0_EM|CR0_MP|CR0_WP;
	cr0 &= ~(CR0_TS|CR0_EM);
	load_reg(%cr0, cr0);
	// print_info("load cr0 OK\n");
}

void vmem_init_2() {
#if MMAP_SEG_KERNEL_BASE != 0
	for (uint32_t i = 0 ; i < (mappedPageCount >> 10) + 1 ; i++) {

		print_info("PDE_remove %d\n", i);
		PDE_set( &kvmem.pagedir->e[i], 0, 0);
		// vmem.pagetables[i] = (PageTable_t*)pmem_frameAddr(frame);
		kvmem.pagetables[i] = 0;
		// FIXME: free page?

	}
	cr3_reload();
#endif
}


/* pde icin sanal bellek alani bulur ve fiziksel bellek map edip adresi doner */
addr_t vmem_pdeAlloc() {
	int i, r;

	// print_info("[vmem_pdeAlloc]\n");
	ASSERT3(emptyPdeFrame_list.size, >, 0);
	EmptyPdeFrame_t * pdeFrame = li_emptyPdeFrames_front(&emptyPdeFrame_list);
	ASSERT(pdeFrame->key1 == KEY_1 && pdeFrame->key2 == KEY_2);
	ASSERT3(pdeFrame->first, >, 0);
	ASSERT3(emptyPdeFrame_list.size, >, 0);

	addr_t addr;

	if (emptyPdeFrame_list.size == 1 && pdeFrame->emptyCount == 1) { // extend reserved
		addr = ((addr_t)pdeFrame & 0xfffff000) + pdeFrame->first * 0x1000;

		li_emptyPdeFrames_erase(&pdeFrame->lin);
		// FIXME: add to full
		pdeFrame->emptyCount = 0;
		pdeFrame->status[pdeFrame->first] = -2;
		pdeFrame->first = pdeFrame->last = -1;

		PmemFrame_t * frame;
		r = pmem_frameAlloc(&frame);
		ASSERT(r == 0);

		// map pdeFrame & pdeVm addr
		va_t va = kaddr2va(addr);
		PTE_set(&kvmem.pagetables[VM_PDX(va.a)]->e[VM_PTX(va.a)], pmem_frameIndex(frame), PTE_P | PTE_W);

		addr_t newAddr;
		addr_t newSize;

		print_info("vmem_pdeAlloc\n");
		r = __vmalloc(addr, &newAddr, &newSize);
		ASSERT(r == 0); // hata durumunda sanal bellek bitti

		print_info("newAddr: %x\n", newAddr);

		ASSERT3(newSize, >, 0x2000);
		ASSERT_KERNEL_ADDR(newAddr);

		pdeFrame = (EmptyPdeFrame_t*)newAddr;
		EmptyPdeFrame_init(pdeFrame);

		for (i = 2 ; i < 1023 ; i++)
			pdeFrame->status[i] = i+1;
		pdeFrame->status[0] = pdeFrame->status[1] = pdeFrame->status[1023] = -1;
		pdeFrame->emptyCount = 1024 - 3;
		pdeFrame->first = 2;
		pdeFrame->last = 1022;
		r = li_emptyPdeFrames_push_front(&emptyPdeFrame_list, &pdeFrame->lin);
		ASSERT(r == 0);

		ASSERT3(emptyPdeFrame_list.size, ==, 1);
	}

	if (emptyPdeFrame_list.size == 1) {
		ASSERT3(pdeFrame->emptyCount, >, 1);
		ASSERT3(pdeFrame->status[pdeFrame->first], >, -1);
	}

	ASSERT(pdeFrame->key1 == KEY_1 && pdeFrame->key2 == KEY_2);

	addr = ((addr_t)pdeFrame & 0xfffff000) + pdeFrame->first * 0x1000;
	ASSERT_KERNEL_ADDR(addr);

	// ilk bos eleman bilgisini ilerlet ve sayiyi azalt
	int prevFirst = pdeFrame->first;
	pdeFrame->first = pdeFrame->status[pdeFrame->first];
	pdeFrame->status[prevFirst] = -2;
	pdeFrame->emptyCount--;
	if (pdeFrame->first == -1)
		ASSERT3(pdeFrame->emptyCount, ==, 0);

	if (pdeFrame->emptyCount == 0) {
		li_emptyPdeFrames_erase(&pdeFrame->lin);
		pdeFrame->status[pdeFrame->first] = -2;
		pdeFrame->first = pdeFrame->last = -1;
	}

	// adrese frame map et
	PmemFrame_t * frame;
	r = pmem_frameAlloc(&frame);
	ASSERT(r == 0);
	va_t va = kaddr2va(addr);

	ASSERT(kvmem.pagetables[VM_PDX(va.a)] != NULL);
	PTE_set(&kvmem.pagetables[VM_PDX(va.a)]->e[VM_PTX(va.a)], pmem_frameIndex(frame), PTE_W | PTE_P);

	return addr;
}

PmemFrame_t * vmem_getFrame(TaskMemory_t * mem, va_t va) {
	if (mem->pagetables[VM_PDX(va.a)] == NULL)
		return NULL;

	addr_t frameAddr = mem->pagetables[VM_PDX(va.a)]->e[VM_PTX(va.a)].pageindex * 0x1000;

	return pmem_getFrame(frameAddr);
}

int vmem_pdeFree(addr_t addr) {
	// print_info("pdeFree %x\n", addr);
	int r;

	ASSERT(addr != 0);

	r = vmem_unmap(&kvmem, kaddr2va(addr));
	if (r != 0)
		return r;

	if (addr < 0x10000000) {
		ASSERT3(addr, >, (addr_t)reserved);
		ASSERT3(addr, <=, ((addr_t)reserved) + 0x2000);

		print_info("WARNING: free reserved not implemented: %x\n", addr);

		print_info("addr: %x\n", addr);

		int index = (addr-(addr_t)reserved)/0x1000;

		reserved->status[index] = -1;
		if (reserved->first == -1) {
			ASSERT3(reserved->emptyCount, ==, 0);
			reserved->last = reserved->first = index;
		} else {
			ASSERT3(reserved->emptyCount, >, 0);
			reserved->status[reserved->last] = index;
			reserved->last = index;
		}

		reserved->emptyCount++;
		ASSERT3(reserved->emptyCount, >, 0);

		r = li_emptyPdeFrames_push_front(&emptyPdeFrame_list, &reserved->lin);
		ASSERT(r == 0);

		return 0;
	}

	// FIMXE: adresi belirle
	va_t va = kaddr2va(addr);
	ASSERT(kvmem.pagetables[VM_PDX(va.a)]->e[0].present);
	ASSERT(!kvmem.pagetables[VM_PDX(va.a)]->e[1].present);

	// pdeFrame yapisinin adresini bul
	addr_t start;
	size_t size;
	r = VmallocBlockInfo_getStartAddr(addr, &start, &size);
	ASSERT3(r, ==, 0);

	// pdeFrame yapisini kontrol et
	EmptyPdeFrame_t * pdeFrame = (EmptyPdeFrame_t *)start;
	ASSERT3(pdeFrame->key1, ==, KEY_1);
	ASSERT3(pdeFrame->key2, ==, KEY_2);
	int index = (addr >> 12) % 1024;
	ASSERT3(pdeFrame->status[index], ==, -2);


	// ters: adrese frame map et
	PmemFrame_t * frame = vmem_getFrame(&kvmem, va);
	ASSERT(frame != NULL);
	int a = pmem_frameIndex(frame);
	// FIXME: bir hata var ????
	if (a != 0) {
		// print_info("%x %x %x\n", a, va.a, addr);
		pmem_refCountDec(frame);
		// while(1);
	}
	PTE_set(&kvmem.pagetables[VM_PDX(va.a)]->e[VM_PTX(va.a)], 0, 0);

	// ters: (pdeAlloc) ilk bos eleman bilgisini ilerlet ve sayiyi azalt
	pdeFrame->status[index] = -1;
	if (pdeFrame->first == -1) {
		ASSERT3(pdeFrame->emptyCount, ==, 0);
		pdeFrame->first = pdeFrame->last = index;
	} else {
		ASSERT3(pdeFrame->emptyCount, >, 0);
		pdeFrame->status[pdeFrame->last] = index;
		pdeFrame->last = index;
	}
	pdeFrame->emptyCount++;
	if (pdeFrame->emptyCount == 1) {
		r = li_emptyPdeFrames_push_front(&emptyPdeFrame_list, &pdeFrame->lin);
		ASSERT(r == 0);
		ASSERT3(emptyPdeFrame_list.size, >, 0);
	}


	if (pdeFrame->emptyCount == 1021) {
		// ters: extend reserved. gerekli mi ???
	}

	return 0;
}


void vmem_map_4MB(va_t pdeVm, va_t vm) {
	int r;
	int i;
	PmemFrame_t * pdeFrame;
	PmemFrame_t * vmFrame;

	ASSERT3((pdeVm.a & 0xfff), ==, 0);
	ASSERT3(VM_PTX(vm.a), ==, 0);

	// vm blocka ait pde olmamali
	ASSERT(kvmem.pagetables[VM_PDX(vm.a)] == NULL);
	ASSERT(kvmem.pagedir->e[VM_PDX(vm.a)].present == 0);

	pdeFrame = vmem_getFrame(&kvmem, pdeVm);
	ASSERT(pdeFrame != NULL);
	ASSERT(kvmem.pagetables[VM_PDX(pdeVm.a)] != NULL);
	ASSERT(kvmem.pagetables[VM_PDX(pdeVm.a)]->e[VM_PTX(pdeVm.a)].present != 0);
	ASSERT3(kvmem.pagetables[VM_PDX(pdeVm.a)]->e[VM_PTX(pdeVm.a)].pageindex, ==, pmem_frameIndex(pdeFrame));

	// create pde for vm
	// ASSERT(kvmem.pagetables[VM_PDX(vm.a)] != NULL);
	if (kvmem.pagetables[VM_PDX(vm.a)] == NULL) {
		// FIXME: vmem_vmallocInit fonksiyonunun tamamlanmasi durumunu kontrol et
		ASSERT(getActiveTask() == NULL); // yukaridaki kontrol olunca buna gerek yok

		PDE_set( &kvmem.pagedir->e[VM_PDX(vm.a)], pmem_frameIndex(pdeFrame), PTE_P | PTE_W  );
		kvmem.pagetables[VM_PDX(vm.a)] = (PageTable_t*)va2kaddr(pdeVm);
	}

	ASSERT_KERNEL_ADDR(kvmem.pagetables[VM_PDX(vm.a)]);
	testAddr(kvmem.pagetables[VM_PDX(vm.a)]);

	// alloc frame for vm
	r = pmem_frameAlloc(&vmFrame);
	ASSERT(r == 0);
	ASSERT(vmFrame != pmem_frames_const);

	// bloktaki tum pageleri empty olarak isaretle
	for (i = 0 ; i < 1024 ; i++) {
		PTE_set(&kvmem.pagetables[VM_PDX(vm.a)]->e[i], 0, 0);
	}

	// map block header frame & header addr
	PTE_set(&kvmem.pagetables[VM_PDX(vm.a)]->e[VM_PTX(vm.a)], pmem_frameIndex(vmFrame), PTE_P | PTE_W );

}

int vmem_unmap_4MB(va_t pdeVm, va_t va) {
	// print_info("vmem_unmap_4MB: %x %x\n", pdeVm.a, va.a);
	ASSERT3((pdeVm.a & 0xfff), ==, 0);
	ASSERT3(VM_PTX(va.a), ==, 0);

	ASSERT(kvmem.pagetables[VM_PDX(va.a)] != NULL);
	ASSERT(kvmem.pagedir->e[VM_PDX(va.a)].present);

	int i;
	for (i = 1 ; i < 1024 ; i++) {
		// print_info("%x\n", i);
		ASSERT(kvmem.pagetables[VM_PDX(va.a)]->e[i].present == 0);
	}
	// PTE_set(&vmem.pagetables[VM_PDX(va.a)]->e[VM_PTX(va.a)], 0, 0);

	// reverse: alloc frame for vm & map block header frame & header addr
	PTE_t * pte = &kvmem.pagetables[VM_PDX(va.a)]->e[VM_PTX(va.a)];
	PmemFrame_t * vaFrame = pmem_getFrame(pte->pageindex * 0x1000);
	PTE_set(pte, 0, 0);

	ASSERT(vaFrame != pmem_frames_const);
	pmem_refCountDec(vaFrame);

	// reverse: create pde for vm
	ASSERT(kvmem.pagetables[VM_PDX(va.a)] != NULL); // bu alanin silinmemesi gerek

	return 0;
}

/* mem degeri girilen task icin va ve frame'i map eder */
void vmem_map(TaskMemory_t * mem, PmemFrame_t * frame, va_t va, int perm) {
	if (mem == NULL)
		mem = &kvmem;

	ASSERT(!frame->free); // map etmeden once frame allocate edilmis olmali
	ASSERT(frame->type == pmem_frame_type_AVAILABLE); // frame kullanilabilir listesinde olmali

	// if pde not exist create
	if (mem->pagetables[VM_PDX(va.a)] == NULL) {
		addr_t pde = vmem_pdeAlloc();
		ASSERT(vmem_getPerm(mem, kaddr2va(pde)) != 0);
		mem->pagetables[VM_PDX(va.a)] = (PageTable_t*)pde;
		PDE_set(&mem->pagedir->e[VM_PDX(va.a)], pmem_frameIndex(vmem_getFrame(mem, kaddr2va(pde))), perm);
	}

	// map va to frame
	PTE_set(&mem->pagetables[VM_PDX(va.a)]->e[VM_PTX(va.a)], pmem_frameIndex(frame), perm);
	frame->n++;
}

/* mem degeri girilen task icin va'yi unmap yapar */
int vmem_unmap(TaskMemory_t * mem, va_t va) {
	if (mem == NULL)
		mem = &kvmem;

	int perm = vmem_getPerm(mem, va);
	if ((perm & PTE_P) != PTE_P)
		return -1;

	PTE_t * pte = &mem->pagetables[VM_PDX(va.a)]->e[VM_PTX(va.a)];

	PmemFrame_t * frame = pmem_getFrame(pte->pageindex * 0x1000);

	ASSERT(frame != pmem_frames_const);
	PTE_set(pte, 0, 0);
	pmem_refCountDec(frame);

	// FIXME: test edilmedi
	tlb_invalidate(va.a);

	return 0;
}

int vmem_getPerm(TaskMemory_t * mem, va_t va) {
	if (mem == NULL)
		mem = &kvmem;

	if (mem->pagetables[VM_PDX(va.a)] == NULL)
		return 0;
	return PTE_getPerm(&mem->pagetables[VM_PDX(va.a)]->e[VM_PTX(va.a)]);
}

int vmem_changePerm(TaskMemory_t * mem, va_t va, int perm) {
	PANIC("test edilmedi");
	if (mem == NULL)
		mem = &kvmem;

	if (mem->pagetables[VM_PDX(va.a)] == NULL)
		return -1;
	PTE_t * pte = &mem->pagetables[VM_PDX(va.a)]->e[VM_PTX(va.a)];
	PTE_set(pte, pte->pageindex, perm);
	return 0;
}

PTE_t vmem_getPTE(TaskMemory_t * mem, va_t va) {
	PANIC("test edilmedi");
	if (mem->pagetables[VM_PDX(va.a)] == NULL) {
		return (PTE_t){ 0 };
	}
	return mem->pagetables[VM_PDX(va.a)]->e[VM_PTX(va.a)];
}

PDE_t vmem_getPDE(TaskMemory_t * mem, va_t va) {
	PANIC("test edilmedi");
	if (mem->pagetables[VM_PDX(va.a)] == NULL) {
		return (PDE_t){ 0 };
	}
	return mem->pagedir->e[VM_PDX(va.a)];
}

// FIXME: --
void vmem_vmallocInit(addr_t start, size_t size) {

	addr_t pde = vmem_pdeAlloc();

	int i;
	int count = size / (1024*0x1000);
	int x = VM_PDX(kaddr2va(start).a);
	for (i = x ; i < (x+count) ; i++) {
		if (kvmem.pagetables[i] == NULL) {
			kvmem.pagetables[i] = (PageTable_t*)pde;
			PDE_set(&kvmem.pagedir->e[i], pmem_frameIndex(vmem_getFrame(&kvmem, kaddr2va(pde))), PTE_W | PTE_U | PTE_P);
			pde = vmem_pdeAlloc();
		}
	}

	vmem_pdeFree(pde);
}


/*********************************************************
 *					Test Kodu
 ********************************************************/

static struct {
	addr_t xList[5000];
	int xi;
	int runCount;
} test = {
	{ 0 }, 0, 0
};

void vmem_test() {
	// print_info("*********** vmem_test ************\n");
	int i, j, r;
	addr_t x, xPrev;

	size_t freeSize, freeBlockCount;

	test.runCount++;
	test.xi = 0;

	// allocate all vm with pdeAlloc
	for (j = 0 ; j < 3 ; j++) {
		// print_info("j: %d\n", j);
		x = vmem_pdeAlloc();
		ASSERT((x & 0xffffff) != 0x400000);
		/* print_info("pdeAlloc: %x\n", x); */
		test.xList[test.xi++] = x;

		// 1 pde, 1 header, 2 empty, 1 x
		for (i = 5 ; i < 1024 ; i++) {
			ASSERT3(emptyPdeFrame_list.size, >, 0);
			xPrev = x;
			x = vmem_pdeAlloc();
			ASSERT((x & 0xffffff) != 0x400000);
			test.xList[test.xi++] = x;
			if (test.runCount < 2)
				ASSERT3(x, ==, xPrev+0x1000);

		}
	}

	/* ASSERT3(vmem.reservedPdeVaNext, ==, vmem.reservedPdeVaLast); */

	vmalloc_status(&freeSize, &freeBlockCount);
	print_info("freeSize: %x, count: %x\n", freeSize, freeBlockCount);

	for (i = 0 ; i < test.xi ; i++) {
		x = test.xList[i];
		r = vmem_pdeFree(x);
		ASSERT3(r, ==, 0);
	}

	vmalloc_status(&freeSize, &freeBlockCount);
	print_info("freeSize: %x, count: %x\n", freeSize, freeBlockCount);
	print_info("emptyPdeFrame_list.size: %d\n", emptyPdeFrame_list.size);

	li_emptyPdeFrames_node * n;
	for (n = li_emptyPdeFrames_begin(&emptyPdeFrame_list) ; n != li_emptyPdeFrames_end(&emptyPdeFrame_list) ; n = n->next) {
		EmptyPdeFrame_t * e = li_emptyPdeFrames_value(n);
		int count = 0;
		for (i = 0 ; i < 1024 ; i++) {
			if (e->status[i] > -1)
				count++;
		}
		int x = e->first;
		for (i = 0 ; x > -1 ; i++) {
			x = e->status[x];
		}
		print_info("%d %d %d\n", e->emptyCount, count, i-1);
		// ASSERT3(i, ==, e->emptyCount);
		ASSERT3(e->status[e->first], >, -1);
	}

}

