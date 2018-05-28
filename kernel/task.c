#include <kernel/kernel.h>
#include <asm/x86.h>
#include <kernel/trap.h>
#include <string.h>
#include <util.h>
#include <kernel/syscall.h>

#include "task.h"
#include "memory/memory.h"
#include "kernel.h"

// FIXME: -
extern void print_trapframe(struct trapframe *tf);

#define KERNEL_STACK_SIZE 0x4000

extern void *__boot_stack_top;
extern uint32_t asmstack;

static Task_t * activeTask;


Task_t * getActiveTask() {
	return activeTask;
}

static uint32_t nextTaskId = 0;

static Task_t * alloc_task();
static addr_t alloc_stack();

static void switch_to_task(Task_t * newtask);
static void switch_to_task_pc(Task_t * newtask);
void switch_to_task_notReady(Task_t * task);

void allTasks_add(Task_t * task);
void allTasks_remove(Task_t * task);
Task_t * allTasks_get1(int id);

void task_init() {
	Task_t t;
	activeTask = &t;
	vmem_setTask0(&t.mem);

	activeTask = alloc_task();
	memset(activeTask, 0, sizeof(Task_t));
	memcpy(&activeTask->mem, &t.mem, sizeof(t.mem));

	ASSERT3(nextTaskId, >=, 0);

	activeTask->id = createTaskId(getMachineId(), nextTaskId++);
	activeTask->state = TASK_STATE_NEW;

	activeTask->task_switch_method = switch_to_task;
	activeTask->stack_top = (addr_t)&__boot_stack_top;

	allTasks_add(activeTask);

	// add to running tasks
	scheduler_addToRunning(activeTask);
}



Task_t * create_task(int id) {
	SAVE_ALL_REGISTERS_TO_STACK();

	uint32_t eip;
	Task_t * newtask = alloc_task();
	Task_t * parent = getActiveTask();

	if (!newtask)
		return (Task_t*)(-1);
	memset(newtask, 0, sizeof(*newtask));

	newtask->id = id;
	newtask->state = TASK_STATE_NEW;
	newtask->task_switch_method = switch_to_task_pc;
	newtask->programLoaded = parent->programLoaded;
	if (newtask->programLoaded)
		memcpy(&newtask->userRegs, (void*)asmstack, sizeof(newtask->userRegs));

	vmem_fork(&activeTask->mem, &newtask->mem);

	// newtask->parent
	// newtask->state

	// FIXME: stack top bottom ters???
	/* kernel stackini kopyala */
	newtask->stack_top = alloc_stack()+KERNEL_STACK_SIZE;
	// print_info("copy stack %08x -> %08x\n", activeTask->stack_top, newtask->stack_top);
	memcpy((void*)(newtask->stack_top-KERNEL_STACK_SIZE),
		   (void*)(activeTask->stack_top-KERNEL_STACK_SIZE), KERNEL_STACK_SIZE);

	eip = read_eip();
	/* bu satir iki kere calisir, eip==1 bizim task switch sirasinda bastigimiz deger */
	if (eip == 1)
		return NULL; /* child calismaya basliyor */

	newtask->eip = eip;
	read_reg(%esp, newtask->esp);
	read_reg(%ebp, newtask->ebp);

	newtask->esp += newtask->stack_top - activeTask->stack_top;
	newtask->ebp += newtask->stack_top - activeTask->stack_top;

	allTasks_add(newtask);

	return newtask;
}

int do_fork() {

	int id = createTaskId(getMachineId(), nextTaskId++);
	Task_t * t = create_task(id);

	if ((int)t == (-1))
		return -1;

	if (t == NULL)
		return 0;

	scheduler_addToRunning(t);

	return t->id;
}


static Task_t * alloc_task() {
	Task_t * t = kmalloc(sizeof(Task_t));
	ASSERT(t != NULL);
	return t;
}

#if 0
static void free_task(Task_t * task) {
	task->id = -1;
}
#endif

static addr_t alloc_stack() {

	void * addr = kmalloc(KERNEL_STACK_SIZE);
	ASSERT(addr != NULL);

	// sifirlamaya gerek yok, test icin eklendi
	memset(addr, 0, KERNEL_STACK_SIZE);

	return (addr_t)addr;
}

int do_getpid() {
	return getActiveTask()->id;
}

static void switch_to_task(Task_t * newtask) {
	SAVE_ALL_REGISTERS_TO_STACK();

	// print_info("switch_to_task\n");

	read_reg(%esp, activeTask->esp);
	read_reg(%ebp, activeTask->ebp);
	activeTask->asmstack = asmstack;

	/* user registerlarini kaydet */
	memcpy(&activeTask->userRegs, (void*)asmstack, sizeof(activeTask->userRegs));

	activeTask->state = TASK_STATE_READY;
	activeTask = newtask;

	load_reg(%cr3, activeTask->mem.pagedir_pa);

	// ASSERT(activeTask->programLoaded);
	if (activeTask->programLoaded) {
		asm volatile(
			/* create trapframe */
			"movl %8, %%eax\n\t"

			"pushl $0x23\n\t" /* ss */
			"pushl %%eax\n\t" /* esp */
			"pushl %9\n\t" /* eflags */
			"pushl $0x1B\n\t" /* cs */
			"pushl %10\n\t" /* eip */

			"push %7\n\t"
			"push %6\n\t"
			"push %5\n\t"
			"push %4\n\t"
			"push %3\n\t"
			"push %2\n\t"
			"push %1\n\t"
			"push %0\n\t"

			/* load user descriptors */
			"mov $0x23, %%ax\n\t"
			"mov %%ax, %%ds\n\t"
			"mov %%ax, %%es\n\t"

			"popal\n\t"

			/* pop trapframe */
			"iret\n\t"
			"1:"
			: "+m"(newtask->userRegs.regs.edi), "+m"(newtask->userRegs.regs.esi), "+m"(newtask->userRegs.regs.ebp),
			  "+m"(newtask->userRegs.regs.oesp), "+m"(newtask->userRegs.regs.ebx), "+m"(newtask->userRegs.regs.edx),
			  "+m"(newtask->userRegs.regs.ecx), "+m"(newtask->userRegs.regs.eax)
			: "r"(newtask->userRegs.esp), "r"(newtask->userRegs.eflags), "r"(newtask->userRegs.eip)
			: "eax"
		);
	}

	/* user registerlarini stacke kopyala */
	/* memcpy((void*)asmstack, &activeTask->userRegs, sizeof(activeTask->userRegs)); */

	load_reg(%esp, activeTask->esp);
	load_reg(%ebp, activeTask->ebp);
	/* bu satirdan yeni task ile devam ediliyor */

}

void switch_to_task_notReady(Task_t * task) {
	PANIC("task_switch_notReady");
}

static void switch_to_task_pc(Task_t * newtask) {
	SAVE_ALL_REGISTERS_TO_STACK();

	print_info("switch_to_task_pc\n");

	read_reg(%esp, activeTask->esp);
	read_reg(%ebp, activeTask->ebp);

	/* user registerlarini kaydet */
	memcpy(&activeTask->userRegs, (void*)asmstack, sizeof(activeTask->userRegs));

	activeTask->state = TASK_STATE_READY;
	activeTask = newtask;
	activeTask->task_switch_method = switch_to_task;

	asm volatile(
		"movl %0, %%eax\n\t"
		"movl $0, %0\n\t" // activeTask->eip = 0
		"movl %1, %%esp\n\t" // %esp = activeTask->esp
		"movl %2, %%ebp\n\t" // %ebp = activeTask->ebp
		"movl %3, %%cr3\n\t" // %cr3 = activeTask->mem.pgdir_pa
		"pushl $1\n\t" // fork fonksiyonundaki read_eip() icin return deger 1
		"pushl %%eax\n\t" // ret ile yuklenecek program counter (eip)
		"sti\n\t"
		"ret"
		: "+m" (activeTask->eip)
		: "r" (activeTask->esp),
		  "r" (activeTask->ebp),
		  "r" (activeTask->mem.pagedir_pa)
		: "eax"
		);
}


void switch_to_task_migration(Task_t * newtask) {
	SAVE_ALL_REGISTERS_TO_STACK();

	print_info("switch_to_task_migration\n");

	/* user registerlarini kaydet */
	memcpy(&activeTask->userRegs, (void*)asmstack, sizeof(activeTask->userRegs));

	read_reg(%esp, activeTask->esp);
	read_reg(%ebp, activeTask->ebp);

	activeTask->state = TASK_STATE_READY;
	activeTask = newtask;
	activeTask->task_switch_method = switch_to_task;

	load_reg(%cr3, newtask->mem.pagedir_pa);

	activeTask->programLoaded = 1;

	/*
	int id1 = (newtask->id >> 20) & 0xff;
	int id2 = getMachineId();
	if (id1 != id2) {
		print_trapframe(&newtask->userRegs);
	}
	*/

	asm volatile(
		/* create trapframe */
		"movl %8, %%eax\n\t"

		"pushl $0x23\n\t" /* ss */
		"pushl %%eax\n\t" /* esp */
		"pushl %9\n\t" /* eflags */
		"pushl $0x1B\n\t" /* cs */
		"pushl %10\n\t" /* eip */

		"push %7\n\t"
		"push %6\n\t"
		"push %5\n\t"
		"push %4\n\t"
		"push %3\n\t"
		"push %2\n\t"
		"push %1\n\t"
		"push %0\n\t"

		/* load user descriptors */
		"mov $0x23, %%ax\n\t"
		"mov %%ax, %%ds\n\t"
		"mov %%ax, %%es\n\t"

		"popal\n\t"

		/* pop trapframe */
		"iret\n\t"
		"1:"
		: "+m"(newtask->userRegs.regs.edi), "+m"(newtask->userRegs.regs.esi), "+m"(newtask->userRegs.regs.ebp),
		  "+m"(newtask->userRegs.regs.oesp), "+m"(newtask->userRegs.regs.ebx), "+m"(newtask->userRegs.regs.edx),
		  "+m"(newtask->userRegs.regs.ecx), "+m"(newtask->userRegs.regs.eax)
		: "r"(newtask->userRegs.esp), "r"(newtask->userRegs.eflags), "r"(newtask->userRegs.eip)
		: "eax"
		);

#if 0
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
		:: "r"(newtask->userRegs.esp), "r"(newtask->userRegs.eflags)
		: "eax"
		);
#endif
}

// FIXME: gecici cozum. Hash tablosu eklenmeli.
static Task_t * allTasks[50] = { NULL };

void allTasks_add(Task_t * task) {
	int i;
	for (i = 0 ; i < 50 ; i++) {
		if (allTasks[i] == NULL) {
			allTasks[i] = task;
			return;
		}
	}
	PANIC("--");
}

void allTasks_remove(Task_t * task) {
	int i;
	for (i = 0 ; i < 50 ; i++) {
		if (allTasks[i] == task) {
			allTasks[i] = NULL;
			return;
		}
	}
	PANIC("--");
}

Task_t * allTasks_get1(int id) {
	int i;
	for (i = 0 ; i < 50 ; i++) {
		if (allTasks[i]->id == id) {
			return allTasks[i];
		}
	}
	return NULL;
}

Task_t * allTasks_get(uint8_t machineId, int pid) {
	int id = (machineId << 20) | (pid & 0xfffff);
	return allTasks_get1(id);
}

SYSCALL_DEFINE0(fork) {
	print_info("sys_fork\n");
	int pid = do_fork();
	ASSERT(pid >= 0);
	if (pid != 0) {
		Task_t * child = allTasks_get1(pid);
		child->task_switch_method = switch_to_task;
		child->waitOnSwitch = 1;
		child->userRegs.regs.eax = 0;
		// scheduler_removeFromRunning(child);
	}
	return SYSCALL_RETURN(pid);
}
SYSCALL_END(getpid)
