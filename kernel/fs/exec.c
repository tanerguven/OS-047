#include <types.h>
#include <kernel/syscall.h>
#include <kernel/trap.h>
#include <asm/descriptors.h>
#include <string.h>
#include <kernel/kernel.h>
#include "../memory/memory.h"
#include "elf.h"
#include <util.h>
#include "../task.h"
#include "../memory/memory.h"

int init_user_stack(TaskMemory_t * mem, char *const argv[], addr_t *user_esp) {

	addr_t esp = (addr_t)va2kaddr((va_t){ MMAP_USER_STACK_TOP });
	int r;
#if 0
	addr_t arg_ptr[20];
#endif
	int argc = 0;

	PmemFrame_t * frame;
	r = pmem_frameAlloc(&frame);

	ASSERT(r == 0);
	vmem_map(mem, frame, (va_t){MMAP_USER_STACK_TOP-0x1000}, PTE_P | PTE_U | PTE_W);
	// vmem_map(mem, frame, kaddr2va((addr_t)espK), PTE_P | PTE_U | PTE_W);
	// ASSERT(vmem_getPerm(mem, kaddr2va((addr_t)espK)) != 0);

#if 0
	task_curr->pgdir.count_stack = 1;
	/* argv'yi stack'e bas */
	for (i = 0 ; argv[i] != NULL ; i++) {
		esp -= strlen(argv[i]) + 1;
		strcpy((char*)esp, argv[i]);
		arg_ptr[i] = esp;
	}
	argc = i;
	while ( --i > -1 ) {
		esp -= sizeof(addr_t);
		*(addr_t*)esp = kaddr2uaddr(arg_ptr[i]);
	}
#endif

	/* argv, argc, eip degerlerini stacke bas */
	((addr_t*)esp)[-1] = kaddr2uaddr(esp); /* argv */
	((addr_t*)esp)[-2] = argc; /* argc */
	((addr_t*)esp)[-3] = va2uaddr((va_t){0xffffffff}); /* return eip */
	esp -= sizeof(addr_t) * 3;

	*user_esp = kaddr2uaddr(esp);
	return 0;
}


static int segmentAlloc(TaskMemory_t * mem, va_t va, size_t len, int perm) {
	int r;
	va_t i;

	addr_t end = roundUp(va.a + len);
	va.a = roundDown(va.a);

	// print_info("segment: %x - %x\n", va.a, end);

	for (i = va ; i.a < end ; i.a += 0x1000) {
		PmemFrame_t * frame;
		r = pmem_frameAlloc(&frame);
		ASSERT(r == 0);
		vmem_map(mem, frame, i, perm);
	}

	return 0;
}

static void unmapUserMemory(TaskMemory_t * mem) {
	int i, j;
	for (i = VM_PDX(MMAP_USER_BASE) ; i < VM_PDX(MMAP_USER_STACK_TOP) ; i++) {
		if (mem->pagetables[i] == NULL)
			continue;
		for (j = 0 ; j < 1024 ; j++) {
			if (!mem->pagetables[i]->e[j].present)
				continue;
			va_t va = (va_t) { i * 1024*0x1000 + j*0x1000 };
			vmem_unmap(mem, va);
		}
	}
}

static int loadProgram(TaskMemory_t * mem, addr_t fileStart, addr_t end, Elf32_Ehdr * header) {
	int i;
	Elf32_Phdr * ph;
	uint32_t old_pos;

	mem->lastAddr = 0;

	addr_t pos = fileStart + header->phoff;

	uint32_t cr3;
	read_reg(%cr3, cr3);
	load_reg(%cr3, mem->pagedir_pa);


	for (i = 0 ; i < header->phnum ; i++) {

		ph = (Elf32_Phdr*)pos;
		pos += sizeof(Elf32_Phdr);

		// print_info("type: %x\n", ph->type);
		// print_info("  %x - %x - %x\n", ph->vaddr, ph->memsz, ph->offset);


		if (ph->type == PhdrType_LOAD) {
			/* programin bulunacagi bellek araligini ayir */
			segmentAlloc(mem, uaddr2va(ph->vaddr), ph->memsz, PTE_P | PTE_U | PTE_W);

			/* programin bitis adresi, brk sistem cagrisi icin gerekli */
			if (roundUp(ph->vaddr + ph->memsz) > mem->lastAddr)
				mem->lastAddr = roundUp(ph->vaddr + ph->memsz);

			mem->countProgramPage += roundUp(ph->memsz) / 0x1000;

			/* dosyadan bellekte bulunacagi yere oku */
			old_pos = pos;
			pos = fileStart + ph->offset;
			memcpy((char*)uaddr2kaddr(ph->vaddr), (void*)pos, ph->memsz);
			pos = old_pos;

			/* programin sonunda kalan alani sifirla */
			if (ph->memsz > ph->filesz)
				memset((void*)(uaddr2kaddr(ph->vaddr) + ph->filesz), 0, ph->memsz - ph->filesz);
		}

	}

	load_reg(%cr3, cr3);

	return 0;
}


int _exec(Task_t * task, addr_t fileStart, size_t fileSize) {
	int r;

	addr_t fileEnd = fileStart + fileSize;
	Elf32_Ehdr * header = (Elf32_Ehdr *)fileStart;

	/* prosesin baslangictaki register degerleri */
	memset(&task->userRegs, 0, sizeof(task->userRegs));
	task->userRegs.eip = header->entry; // programin baslangic adresi
	task->userRegs.ds = task->userRegs.es = task->userRegs.ss = GD_USER_DATA | 3;
	task->userRegs.cs = GD_USER_TEXT | 3;
	task->userRegs.eflags = FL_IF; // interruplar aktif

	unmapUserMemory(&task->mem);

	/* stack'i olustur, argv'yi stacke bas ve registerlara esp degerini ata */
	r = init_user_stack(&task->mem, NULL, &task->userRegs.esp);
	if (r < 0)
		return r;

	// print_info("entry:%x - stack:%x\n", header->entry, registers.esp);
	// print_info("cs:%x ds:%x eflags:%x\n", registers.cs, registers.ds, registers.eflags);

	/* programi bellege (prosesin adres uzayina) yukle */
	Task_t * activeTask = getActiveTask();
	print_info("[%d] loadProgram to %d\n", activeTask->id & 0xfffff, task->id & 0xfffff);
	r = loadProgram(&task->mem, fileStart, fileEnd, header);
	if (r < 0)
		return r;

	task->programLoaded = 1;

	return 0;
}

int exec(addr_t fileStart, size_t fileSize) {
	int r;
	Task_t * task = getActiveTask();
	r =  _exec(task, fileStart, fileSize);
	if (r != 0)
		return r;

	asm volatile(
		/* create trapframe */
		"movl %0, %%eax\n\t"
		"pushl $0x23\n\t" /* ss */
		"pushl %%eax\n\t" /* esp */
		"pushl %1\n\t" /* eflags */
		"pushl $0x1B\n\t" /* cs */
		"push $0x1000000\n\t" /* eip */

		/* load user descriptors */
		"mov $0x23, %%ax\n\t"
		"mov %%ax, %%ds\n\t"
		"mov %%ax, %%es\n\t"

		/* pop trapframe */
		"iret\n\t"
		"1:"
		:: "r"(task->userRegs.esp), "r"(task->userRegs.eflags)
		);

	PANIC("Bu satir calismamali");

	return 0;
}

// FIXME: main.c'den tasi
extern void runProgram(const char * path);

SYSCALL_DEFINE1(exec, const char*, s) {

	/* FIXME: adresin kullaniciya ait oldugundan ve dogru oldugundan emin ol */
	int perm = vmem_getPerm(&getActiveTask()->mem, uaddr2va((addr_t)s));
	if ((perm & PTE_U) != PTE_U) {
		Task_t * task = getActiveTask();
		print_info("%x - %d - %x\n", s, task->id & 0xfffff, task->mem.lastAddr);
		PANIC("FIXME: hatali adres\n");
	}

	s = (const char *)uaddr2kaddr((addr_t)s);

	runProgram(s);

	return SYSCALL_RETURN(0);

}
SYSCALL_END(exec)
