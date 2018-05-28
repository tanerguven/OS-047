#include <types.h>
#include <errno.h>
#include <string.h>
#include <kernel/kernel.h>
#include <data_structures/list2.h>

#include "memory.h"

// #define USE_RB_TREE

#define BLOCK_INFO_KEY1 0x12345678
#define BLOCK_INFO_KEY2 0x98765432

DEFINE_LIST(blocks);
DEFINE_LIST(freeBlocks);
//DEFINE_LIST(pdeBlocks);
typedef struct VmallocBlockInfo {
	uint32_t _key1;
	uint32_t pageIndex;
	uint32_t pageCount;
	li_blocks_node lin_blocks;
	li_freeBlocks_node lin_freeBlocks;
	// li_pdeBlocks_node lin_pdeBlocks;
	// int used;
	size_t usedSize;
	uint32_t blockStartAddr;
	struct VmallocBlockInfo * parent;
	uint32_t _key2;
} VmallocBlockInfo_t;
DEFINE_LIST_OFFSET(blocks, VmallocBlockInfo_t, lin_blocks);
DEFINE_LIST_OFFSET(freeBlocks, VmallocBlockInfo_t, lin_freeBlocks);
//DEFINE_LIST_OFFSET(pdeBlocks, VmallocBlockInfo_t, lin_pdeBlocks);

void VmallocBlockInfo_init(VmallocBlockInfo_t * x) {
	memset(x, 0, sizeof(VmallocBlockInfo_t));
	x->_key1 = BLOCK_INFO_KEY1;
	x->_key2 = BLOCK_INFO_KEY2;
}


int VmallocBlockInfo_getStartAddr(addr_t addr, addr_t * start, size_t * size) {
	VmallocBlockInfo_t * header = (VmallocBlockInfo_t*)(addr & 0xffc00000);

	// check
	int perm = vmem_getPerm(NULL, kaddr2va((addr_t)header));
	if ((perm&PTE_P) != PTE_P)
		return -2;

	if (header->_key1 != BLOCK_INFO_KEY1 || header->_key2 != BLOCK_INFO_KEY2)
		return -3; // FIXME: warning



	if ((header->usedSize & 0xfff) == 0)
		*start = (header->pageIndex + 2) * 0x1000;
	else
		*start = header->pageIndex * 0x1000 + sizeof(VmallocBlockInfo_t)*2;

	*size = header->usedSize;
	ASSERT3(((*start+*size) & 0x3fffff), ==, 0);

	return 0;
}

/*
 * block_list: tum bloklarin bulundugu liste:
 * [free block(3)] -> [block(2)] -> [free block(100)] -> [block(1)] -> [block(2)] -> []
 *
 * - free blocklar birlestirilerek saklanir.
 */
static li_blocks block_list;
static li_freeBlocks freeBlock_list;
//static li_pdeBlocks pdeBlock_list;

static const addr_t firstVm = 0x20000000;
static const size_t vmSize = 32*1024*1024;

void vmalloc_status(size_t * freeSize, size_t * freeBlockCount) {
	li_freeBlocks_node * n;

	*freeBlockCount = freeBlock_list.size;
	*freeSize = 0;
	for (n = li_freeBlocks_begin(&freeBlock_list) ; n != li_freeBlocks_end(&freeBlock_list) ; n = n->next) {
		VmallocBlockInfo_t * b = li_freeBlocks_value(n);
		*freeSize += b->pageCount * 0x1000;
	}
}

/* page count'un sigabilecegi en kucuk page block arar */
VmallocBlockInfo_t *freeBlocks_find(uint32_t pageCount) {

#ifdef VMALLOC_USE_RBTREE
	PANIC("VMALLOC_USE_RBTREE not implemented");
#else
	VmallocBlockInfo_t * minBlock = NULL;
	li_freeBlocks_node * n;
	for (n = li_freeBlocks_begin(&freeBlock_list) ; n != li_freeBlocks_end(&freeBlock_list) ; n = n->next) {
		VmallocBlockInfo_t * b = li_freeBlocks_value(n);
		// print_info("[%x] %d KB\n", b->pageIndex, b->pageCount * 4);

		if (b->pageCount >= pageCount) {
			if (minBlock == NULL || minBlock->pageCount > b->pageCount)
				minBlock = b;
		}
	}

	return minBlock;
#endif /* VMALLOC_USE_RBTREE */
}

void blocks_print() {
	print_info("vmalloc blocks:\n");
	li_blocks_node * n;
	for (n = li_blocks_begin(&block_list) ; n != li_blocks_end(&block_list) ; n = n->next) {
		VmallocBlockInfo_t * b = li_blocks_value(n);
		print_info("[%x-%x] %d\n", b->pageIndex*0x1000, (b->pageIndex+b->pageCount)*0x1000, b->usedSize/1024);
	}
}

void freeBlocks_remove(VmallocBlockInfo_t * block) {
#ifdef VMALLOC_USE_RBTREE
	PANIC("VMALLOC_USE_RBTREE not implemented");
#else
	li_freeBlocks_erase(&block->lin_freeBlocks);
#endif /* VMALLOC_USE_RBTREE */
}

void freeBlocks_put(VmallocBlockInfo_t * block) {
#ifdef VMALLOC_USE_RBTREE
	PANIC("VMALLOC_USE_RBTREE not implemented");
#else
	ASSERT(block->usedSize == 0);
	li_freeBlocks_insert(li_freeBlocks_begin(&freeBlock_list), &block->lin_freeBlocks);
#endif /* VMALLOC_USE_RBTREE */
}


/**
   4 KB align adres dondurur.

   pdeVm varsa 4 MB + header seklinde adres dondurur. ikl page headeri da icerir.
 */
int __vmalloc(uint32_t pdeVm, addr_t * addr, uint32_t * size) {
	int r;

	// pdeVm !=0 ise align icin 8 MB adres ariyoruz
	int requiredPageCount = (pdeVm != 0) ? 2048 : *size + 3;

	// print_info("[__vmalloc] %x\n", pdeVm);

	if (freeBlock_list.size == 0)
		return -1;

	// find virtual memory block
	VmallocBlockInfo_t * minBlock = freeBlocks_find(requiredPageCount);

	ASSERT(minBlock != NULL);
	// print_info("minBLock : %x-%x (%d KB)\n", minBlock->pageIndex*4*1024, (minBlock->pageIndex+minBlock->pageCount)*4*1024, minBlock->pageCount*4);

	VmallocBlockInfo_t * newBlock;

	if (pdeVm == 0) {
		if (minBlock->pageCount > requiredPageCount + 6) {
			minBlock->pageCount -= requiredPageCount;

			// UYARI ayni fonksiyon map sirasinda 1 kere recursive calisiyor
			int minBlockPageCount = minBlock->pageCount;

			addr_t newBlockVm = (minBlock->pageIndex + minBlock->pageCount) * 0x1000;
			addr_t blockStartAddr = newBlockVm;

			PmemFrame_t * frame;
			r = pmem_frameAlloc(&frame);
			ASSERT(r == 0);
			vmem_map(NULL, frame, kaddr2va(newBlockVm), PTE_P | PTE_W);

			/* newBlock bilgilerini kaydet */
			newBlock = (VmallocBlockInfo_t *)newBlockVm;
			VmallocBlockInfo_init(newBlock);
			newBlock->pageIndex =  newBlockVm / 0x1000;
			newBlock->pageCount = requiredPageCount;
			newBlock->usedSize = -1;
			newBlock->blockStartAddr = blockStartAddr;

			// print_info("[split page] %x-%x,", minBlock->pageIndex*0x1000, (minBlock->pageIndex+minBlock->pageCount)*0x1000);
			// print_info("%x-%x\n", newBlock->pageIndex*0x1000, (newBlock->pageIndex+newBlock->pageCount)*0x1000);

			/* newBlocku block listesine adres sirasina gore ekle (minBlock'un sonrasina) */

			if (minBlockPageCount == minBlock->pageCount) {
				li_blocks_insert(minBlock->lin_blocks.next, &newBlock->lin_blocks);
			} else {
				VmallocBlockInfo_t * x = li_blocks_value(minBlock->lin_blocks.next);
				ASSERT(li_blocks_value(x->lin_blocks.prev) == minBlock);

				addr_t addr = (x->pageIndex + x->pageCount) * 0x1000;
				ASSERT3(addr, ==, newBlock->pageIndex*0x1000);

				li_blocks_insert(x->lin_blocks.next, &newBlock->lin_blocks);
			}


		} else {
			if (freeBlock_list.size == 1) {
				/*
				  freeBlock_list eleman sayisi 0 olunca bir bug var.
				  sanal bellek oldugu icin, simdilik tamamen tukenmesi durumunu ekelemeye gerek yok
				  son kalan 4 MB kullanilamiyor
				*/
				return -1;
			}

			// remove from freeBlock_list
			freeBlocks_remove(minBlock);
			newBlock = minBlock;
			newBlock->usedSize = -1;
		}

	} else {
		/* pdeVm != 0 */
		if (minBlock->pageCount > requiredPageCount + 6) {
			minBlock->pageCount -= requiredPageCount;

			addr_t newBlockVm = (minBlock->pageIndex + minBlock->pageCount) * 0x1000;
			addr_t blockStartAddr = newBlockVm;

			newBlockVm = ((newBlockVm+0x800000-1) & ~(0x800000-1)); // 4MB align

			vmem_map_4MB(kaddr2va(pdeVm), kaddr2va(newBlockVm));

			/* newBlock bilgilerini kaydet */
			newBlock = (VmallocBlockInfo_t *)newBlockVm;
			VmallocBlockInfo_init(newBlock);
			newBlock->pageIndex =  newBlockVm / 0x1000;
			newBlock->pageCount = requiredPageCount - (newBlockVm-blockStartAddr)/0x1000;
			newBlock->usedSize = -1;
			newBlock->blockStartAddr = blockStartAddr;
			newBlock->parent = NULL;

			// print_info("[split page] %x-%x,", minBlock->pageIndex*0x1000, (minBlock->pageIndex+minBlock->pageCount)*0x1000);
			// print_info("%x-%x\n", newBlock->pageIndex*0x1000, (newBlock->pageIndex+newBlock->pageCount)*0x1000);

			/* newBlocku block listesine adres sirasina gore ekle (minBlock'un sonrasina) */
			li_blocks_insert(minBlock->lin_blocks.next, &newBlock->lin_blocks);
		} else {
			PANIC("test edilmemis durum\n");
			addr_t newBlockVm = minBlock->pageIndex * 0x1000;
			addr_t blockStartAddr = newBlockVm;

			newBlockVm = ((newBlockVm+0x800000-1) & ~(0x800000-1)); // 4MB align
			vmem_map_4MB(kaddr2va(pdeVm), kaddr2va(newBlockVm));

			/* newBlock bilgilerini kaydet */
			newBlock = (VmallocBlockInfo_t *)newBlockVm;
			VmallocBlockInfo_init(newBlock);
			newBlock->pageIndex =  newBlockVm / 0x1000;
			newBlock->pageCount = requiredPageCount;
			newBlock->usedSize = -1;
			newBlock->blockStartAddr = blockStartAddr;
			newBlock->parent = minBlock;

			// UYARI: block listesine eklenmeyecek

			// remove from freeBlock_list
			freeBlocks_remove(minBlock);
			newBlock = minBlock;
			newBlock->usedSize = -1;
		}
	}

#if 0
	if (pdeVm != 0 || minBlock->pageCount > requiredPageCount + 4) {
		/* vmem_map gerektiren durum */

		// print_info("[split page] %x-%x\n", minBlock->pageIndex*0x1000, (minBlock->pageIndex+minBlock->pageCount)*0x1000);
		minBlock->pageCount -= requiredPageCount;

		addr_t newBlockVm = (minBlock->pageIndex + minBlock->pageCount) * 0x1000;
		addr_t blockStartAddr = newBlockVm;

		/* map header memory */
		if (pdeVm != 0) {
			newBlockVm = ((newBlockVm+0x800000-1) & ~(0x800000-1)); // 4MB align
			ASSERT3((newBlockVm & 0x3fffff), ==, 0);
			vmem_map_4MB(kaddr2va(pdeVm), kaddr2va(newBlockVm));
		} else {
			PmemFrame_t * frame;
			r = pmem_frameAlloc(&frame);
			ASSERT(r == 0);
			vmem_map(frame, kaddr2va(newBlockVm), PTE_P | PTE_W);
		}

		/* newBlock bilgilerini kaydet */
		newBlock = (VmallocBlockInfo_t *)newBlockVm;
		VmallocBlockInfo_init(newBlock);
		newBlock->pageIndex =  newBlockVm / 0x1000;
		newBlock->pageCount = requiredPageCount;
		newBlock->usedSize = -1;
		newBlock->blockStartAddr = blockStartAddr;

		// print_info("[split page] %x-%x,", minBlock->pageIndex*0x1000, (minBlock->pageIndex+minBlock->pageCount)*0x1000);
		// print_info("%x-%x\n", newBlock->pageIndex*0x1000, (newBlock->pageIndex+newBlock->pageCount)*0x1000);

		/* newBlocku block listesine adres sirasina gore ekle (minBlock'un sonrasina) */
		li_blocks_insert(minBlock->lin_blocks.next, &newBlock->lin_blocks);

	} else {
		/* vmem_map'e gerek yok dogrudan block dondurulur */

		if (freeBlock_list.size == 1) {
			/*
			  freeBlock_list eleman sayisi 0 olunca bir bug var.
			  sanal bellek oldugu icin, simdilik tamamen tukenmesi durumunu ekelemeye gerek yok
			  son kalan 4 MB kullanilamiyor
			*/
			return -1;
		}

		// remove from freeBlock_list
		freeBlocks_remove(minBlock);
		newBlock = minBlock;
		newBlock->usedSize = -1;
	}
#endif

	if (pdeVm != 0) {
		*addr = newBlock->pageIndex * 0x1000 + sizeof(VmallocBlockInfo_t)*2;
		newBlock->usedSize = 1024 * 0x1000 - sizeof(VmallocBlockInfo_t)*2;
		*size = newBlock->usedSize;
		// print_info("%x %d\n", *addr, *size);
	} else {
		*addr = (newBlock->pageIndex + 2) * 0x1000;
		newBlock->usedSize = (requiredPageCount-3) * 0x1000;
		ASSERT3(*size, >=, newBlock->usedSize/0x1000);
		*size = newBlock->usedSize/0x1000;
		// print_info("ret %x %d %d %d -- %x\n", *addr, *size, newBlock);
	}

	return 0;
}


addr_t vmalloc(uint32_t pageCount) {
	int r;
	addr_t addr;
	r = __vmalloc(0, &addr, &pageCount);
	ASSERT(r == 0);
	ASSERT((addr & 0xffffff) != 0x400000);
	return addr;
}

int __vfree(addr_t pdeVm, addr_t addr) {
	int i;

	VmallocBlockInfo_t * header = (VmallocBlockInfo_t*)(addr - 2 * 0x1000);
	// print_info("vfree header: %p\n", header);

	int perm = vmem_getPerm(NULL, kaddr2va((addr_t)header));
	if ((perm&PTE_P) != PTE_P)
		return -2;
	if (header->_key1 != BLOCK_INFO_KEY1 || header->_key2 != BLOCK_INFO_KEY2)
		return -3; // FIXME: warning
	for (i = 1 ; i < header->pageCount ; i++) {
		perm = vmem_getPerm(NULL, kaddr2va((header->pageIndex+i) * 0x1000));
		if (perm != 0)
			print_info("%d\n", i);
		if (perm != 0)
			return -4; // herhangi bir mapping varsa va silinemez
	}

	// int rightMerged = 0;

	if ( li_blocks_back(&block_list) != header ) {
		VmallocBlockInfo_t * next = li_blocks_value(header->lin_blocks.next);

		if (next->usedSize == 0) {
			// sonraki ile birlestir
			header->pageCount += next->pageCount;
			li_blocks_erase(&next->lin_blocks);
			li_freeBlocks_erase(&next->lin_freeBlocks);
			memset(next, 0, sizeof(VmallocBlockInfo_t));
			vmem_unmap(NULL, kaddr2va((addr_t)next));
			// rightMerged = 1;
		}
	}

	int leftMerged = 0;

	if ( li_blocks_front(&block_list) != header ) {
		VmallocBlockInfo_t * prev = li_blocks_value(header->lin_blocks.prev);
		if (prev->usedSize == 0) {
			// onceki ile birlestir
			prev->pageCount += header->pageCount;

			li_blocks_erase(&header->lin_blocks);
			memset(header, 0, sizeof(VmallocBlockInfo_t));

			if (pdeVm != 0) {
				vmem_unmap_4MB(kaddr2va(pdeVm), kaddr2va((addr_t)header));
			} else {
				vmem_unmap(NULL, kaddr2va((addr_t)header));
			}
			leftMerged = 1;
		}
		// print_info("__vfree block: %x - %x\n", prev, ((addr_t)prev) + prev->pageCount*0x1000);
	}

	if (!leftMerged) {
		header->usedSize = 0;
		freeBlocks_put(header);
	}

	// print_info("__vfree leftMerge:%d rightMerge:%d\n", leftMerged, rightMerged);

	return 0;

}

int vfree(addr_t addr) {
	return __vfree(0, addr);
}

// FIXME: tasi
extern void vmem_vmallocInit(addr_t start, size_t size);

void vmalloc_init(addr_t pdeVm) {

	li_blocks_init(&block_list);
	li_freeBlocks_init(&freeBlock_list);
	vmem_map_4MB(kaddr2va(pdeVm), kaddr2va(firstVm));

	ASSERT_KERNEL_ADDR(firstVm);
	VmallocBlockInfo_t * firstHeader = (VmallocBlockInfo_t*)firstVm;

	VmallocBlockInfo_init(firstHeader);
	firstHeader->pageIndex = firstVm / 0x1000;
	firstHeader->pageCount = vmSize / 0x1000;
	firstHeader->usedSize = 0;

	li_blocks_push_back(&block_list, &firstHeader->lin_blocks);
	li_freeBlocks_push_back(&freeBlock_list, &firstHeader->lin_freeBlocks);

	ASSERT(li_blocks_front(&block_list) == firstHeader);
	ASSERT(li_freeBlocks_front(&freeBlock_list) == firstHeader);

	vmem_vmallocInit(firstVm, vmSize);
}

struct {
	addr_t addr[4096];
	int count;
} test;

void vmalloc_test() {
	test.count = 0;

	addr_t addr, addr2;
	int i;
	int r;

	addr = vmalloc(1);
	blocks_print();

	r = vfree(addr);
	ASSERT3(r, ==, 0);
	blocks_print();

	addr2 = vmalloc(1);
	ASSERT3(addr, ==, addr2);
	blocks_print();

	r = vfree(addr);
	ASSERT3(r, ==, 0);

	blocks_print();

	for (i = 0 ; i < 1024; i++) {
		addr = vmalloc(1);
		test.addr[test.count++] = addr;
	}
	print_info("vmalloc(1) loop end\n");
	// blocks_print();

	for (i = 0 ; i < test.count; i++) {
		vfree(test.addr[i]);
	}
	test.count = 0;
	print_info("vfree loop end\n");
	blocks_print();
	while(1);

	for (i = 0 ; i < 1024; i++) {
		addr = vmalloc(2);
		test.addr[test.count++] = addr;
	}
	print_info("vmalloc(2) loop end\n");


}
