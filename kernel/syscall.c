#include <kernel/kernel.h>
#include <kernel/trap.h>
#include <kernel/syscall.h>
#include <errno.h>
#include "memory/memory.h"
#include "task.h"
#include "kernel.h"

void sys_nosys();

void sys_cputs();
void sys_sleep();
void sys_getpid();
void sys_getmachineid();

extern void sys_fork();
extern void sys_exec();
extern void sys_cgetc();
void sys_exit();

void (* const syscalls[])() = {
	[0 ... MAX_SYSCALL_COUNT] = sys_nosys,
	[SYS_cputs] = sys_cputs,
	[SYS_sleep] = sys_sleep,
	[SYS_getpid] = sys_getpid,
	[SYS_getmachineid] = sys_getmachineid,
	[SYS_fork] = sys_fork,
	[SYS_exec] = sys_exec,
	[SYS_exit] = sys_exit,
	[SYS_cgetc] = sys_cgetc
};


void isr_syscall(struct trapframe *tf) {
	uint32_t no = tf->regs.eax;
	if (no < MAX_SYSCALL_COUNT) {
		syscalls[no]();
	} else {
		sys_nosys();
	}
}

extern void print_trapframe(struct trapframe *tf);

SYSCALL_DEFINE1(sleep, int, ms) {

	do_sleep(ms);

	return SYSCALL_RETURN(0);
}
SYSCALL_END(nosys)

extern uint32_t asmstack;

SYSCALL_DEFINE0(getpid) {
	int pid = do_getpid();
	return SYSCALL_RETURN(pid);
}
SYSCALL_END(getpid)

SYSCALL_DEFINE0(getmachineid) {
	int machineId = getMachineId();
	return SYSCALL_RETURN(machineId);
}
SYSCALL_END(getpid)

SYSCALL_DEFINE2(cputs, const char*, s, size_t, len) {

	/* FIXME: adresin kullaniciya ait oldugundan ve dogru oldugundan emin ol */
	if (len > 0x1000)
		return; // FIXME: --

	int perm = vmem_getPerm(&getActiveTask()->mem, uaddr2va((addr_t)s));
	if ((perm & PTE_U) != PTE_U) {
		Task_t * task = getActiveTask();
		print_info("%x - %d - %x\n", s, task->id & 0xfffff, task->mem.lastAddr);
		PANIC("FIXME: hatali adres\n");
	}

	s = (char*)uaddr2kaddr((addr_t)s);
	for (uint32_t i = 0 ; (i < len) && (*s != '\0') ; i++, s++)
		print_info("%c", *s); // FIXME: -

	return SYSCALL_RETURN(0);

}
SYSCALL_END(cputs)

SYSCALL_DEFINE1(exit, int, status) {

	Task_t * task = getActiveTask();
	scheduler_removeFromRunning(task);

	task->exitStatus = status;
	task->state = TASK_STATE_TERMINATED;

	print_info("task %x exited\n", task->id);
	scheduler_run();

	return SYSCALL_RETURN(0);
}
SYSCALL_END(nosys)


SYSCALL_DEFINE0(nosys) {
	// cli();
	print_warning("no sys, syscall no: %d\n", tf->regs.eax);

	return SYSCALL_RETURN(-ENOSYS);
}
SYSCALL_END(nosys)
