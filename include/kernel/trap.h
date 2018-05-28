#ifndef _KERNEL_TRAP_H_
#define _KERNEL_TRAP_H_

#define T_SYSCALL		0x80

#define DPL_KERNEL		0
#define DPL_USER		3

/* pushal */
struct pushregs {
	uint32_t edi;
	uint32_t esi;
	uint32_t ebp;
	uint32_t oesp;
	uint32_t ebx;
	uint32_t edx;
	uint32_t ecx;
	uint32_t eax;
} __attribute__((packed));

struct trapframe {
	/* traphandler.S de basilanlar */
	struct pushregs regs;
	uint16_t es; uint16_t __es;
	uint16_t ds; uint16_t __ds;
	/* trap oldugunda islemcinin bastiklari */
	uint32_t error;
	uint32_t eip;
	uint16_t cs; uint16_t __cs;
	uint32_t eflags;
	/* user kernel arasi gecislerde olanlar */
	uint32_t esp;
	uint16_t ss; uint16_t __ss;
} __attribute__((packed));

extern struct trapframe *curr_registers();

#endif /* _KERNEL_TRAP_H_ */
