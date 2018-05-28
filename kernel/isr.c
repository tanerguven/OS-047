#include <kernel/kernel.h>
#include <types.h>
//#include <util.h>

#define __STR(x) #x
#define TO_STR(x) __STR(x)

#include <asm/descriptors.h>
#include <asm/trap.h>
#include <kernel/trap.h>
#include <string.h>

#include "memory/memory.h"
#include "task.h"
#include "kernel.h"

// traphandler.S
extern void asm_divide_error();
extern void asm_debug();
extern void asm_nmi();
extern void asm_int3();
extern void asm_overflow(); /* 5 */
extern void asm_bounds_check();
extern void asm_invalid_opcode();
extern void asm_device_not_available();
extern void asm_double_fault();
extern void asm_coprocessor_segment_overrun(); /* 10 */
extern void asm_invalid_TSS();
extern void asm_segment_not_present();
extern void asm_stack_exception();
extern void asm_general_protection();
extern void asm_page_fault(); /* 15 */
extern void asm_reserved();
extern void asm_floating_point_error();
extern void asm_alignment_check();
extern void asm_machine_check();
extern void asm_SIMD_floating_point_error(); /* 20 */

extern void asm_irq_timer();
extern void asm_irq_com1();
extern void asm_irq_com2();
extern void asm_irq_keyboard();
extern void asm_syscall(); /* 0x80 */

// FIXME: gecici cozum. gercek makinede kayitli olmayan idtlerden interupt aliniyor
extern void asm_irq_bugfix();
static int isr_irq_bugfix_count = 0;
void isr_irq_bugfix() { isr_irq_bugfix_count++; }

static struct GateDesc idt[256] = { { 0 } };

/*
  FIXME: STS_IG32 kesme oldugunda kesmeleri durduruyor
  tum kesmelerin icinde kesmeler durdurulmali mi?
  durdurmayinca uart hata veriyor.
*/

static inline void GateDesc_set_trap(struct GateDesc *desc, void (*f)()) {
	// GateDesc_set(desc, STS_TG32, GD_KERNEL_TEXT, (uint32_t)f, DPL_KERNEL);
	GateDesc_set(desc, STS_IG32, GD_KERNEL_TEXT, (uint32_t)f, DPL_KERNEL);
}

static inline void GateDesc_set_system(struct GateDesc *desc, void (*f)()) {
	// GateDesc_set(desc, STS_TG32, GD_KERNEL_TEXT, (uint32_t)f, DPL_USER);
	GateDesc_set(desc, STS_IG32, GD_KERNEL_TEXT, (uint32_t)f, DPL_USER);
}


static inline void GateDesc_set_interrupt(struct GateDesc *desc, void (*f)()) {
	GateDesc_set(desc, STS_IG32, GD_KERNEL_TEXT, (uint32_t)f, DPL_KERNEL);
}

static struct PseudoDesc idt_pd;


void print_trapframe(struct trapframe *tf);

void isr_init() {
	int i;

	GateDesc_set_trap(&idt[T_DIVIDE], asm_divide_error);
	GateDesc_set_trap(&idt[T_DEBUG], asm_debug);
	GateDesc_set_trap(&idt[T_NMI], asm_nmi);
	GateDesc_set_system(&idt[T_BRKPT], asm_int3);
	GateDesc_set_system(&idt[T_OFLOW], asm_overflow);
	GateDesc_set_system(&idt[T_BOUND], asm_bounds_check);
	GateDesc_set_trap(&idt[T_ILLOP], asm_invalid_opcode);
	GateDesc_set_trap(&idt[T_DEVICE], asm_device_not_available);
	GateDesc_set_trap(&idt[T_DBLFLT], asm_double_fault);
	GateDesc_set_trap(&idt[T_COPROC], asm_coprocessor_segment_overrun);
	GateDesc_set_trap(&idt[T_TSS], asm_invalid_TSS);
	GateDesc_set_trap(&idt[T_SEGNP], asm_segment_not_present);
	GateDesc_set_trap(&idt[T_STACK], asm_stack_exception);
	GateDesc_set_trap(&idt[T_GPFLT], asm_general_protection);
	GateDesc_set_trap(&idt[T_PGFLT], asm_page_fault);
	GateDesc_set_trap(&idt[T_RES], asm_reserved);
	GateDesc_set_trap(&idt[T_FPERR], asm_floating_point_error);
	GateDesc_set_trap(&idt[T_ALIGN], asm_alignment_check);
	GateDesc_set_trap(&idt[T_MCHK], asm_machine_check);
	GateDesc_set_trap(&idt[T_SIMDERR], asm_SIMD_floating_point_error);

	for (i = 21 ; i < 32 ; i++)
		GateDesc_set_trap(&idt[i], asm_reserved);

	for (i = 32 ; i < 256 ; i++)
		GateDesc_set_interrupt(&idt[i], asm_irq_bugfix);

	GateDesc_set_interrupt(&idt[T_IRQ0+IRQ_TIMER], asm_irq_timer);
	GateDesc_set_interrupt(&idt[T_IRQ0+IRQ_KBD], asm_irq_keyboard);
	GateDesc_set_interrupt(&idt[IRQ_OFFSET+IRQ_COM1], asm_irq_com1);
	GateDesc_set_interrupt(&idt[IRQ_OFFSET+IRQ_COM2], asm_irq_com2);
	GateDesc_set_system(&idt[T_SYSCALL], asm_syscall);

	idt_pd.limit = sizeof(idt) - 1;
	idt_pd.base = kaddr2va((addr_t)idt).a;

	idt_load((uint32_t)&idt_pd);

	// print_info("init_traps OK\n");
}

void isr_page_fault(struct trapframe *tf) {
	uint32_t fault_va; read_reg(%cr2, fault_va);
	Task_t * task = getActiveTask();

	if ((tf->error & 0x4) != 0) {
		// user mode page fault

		// FIXME: exit sistem cagirisi ile birlestir
		if (fault_va == 0xffffffff) // exit icin kullanilan deger
			print_info("task %x exited\n", task->id);
		else
			print_info("!!! page fault taskId: %x\n", task->id);

		task->exitStatus = -1;

		scheduler_removeFromRunning(task);
		task->state = TASK_STATE_TERMINATED;
		scheduler_run();

		return;
	}

	print_info("pid: %x\n", task->id);
	print_info("cr2: %x\n", fault_va);
	print_trapframe(tf);

	PANIC("kernel pagefault");
}

void print_trapframe(struct trapframe *tf) {
	print_error("eip: %08x  "
                " cs: %08x  "
                " ds: %08x  "
                "esp: %08x  "
                "ebp: %08x\n"
				"errorno: %08x\n",
                tf->eip, tf->cs, tf->ds, tf->esp, tf->regs.ebp, tf->error);

	// print_error("%x %x %x\n", tf->es, tf->ds, tf->ss);
	print_error("eax: %08x  "
				"ecx: %08x  "
                "edx: %08x  "
                "ebx: %08x  "
                "edi: %08x\n"
                "esi: %08x\n",
                tf->regs.eax, tf->regs.ecx, tf->regs.edx, tf->regs.ebx,
				tf->regs.edi, tf->regs.esi);
}

#define ISR_NOT_DEFINED(name)									\
	extern void isr_##name(struct trapframe *tf) {						\
		print_error("trap handler not defined: %s\n", TO_STR(name));	\
		print_trapframe(tf);											\
		PANIC("");														\
	}

ISR_NOT_DEFINED(divide_error)
ISR_NOT_DEFINED(debug)
ISR_NOT_DEFINED(nmi)
ISR_NOT_DEFINED(int3)
ISR_NOT_DEFINED(overflow) /* 5 */
ISR_NOT_DEFINED(bounds_check)
ISR_NOT_DEFINED(invalid_opcode)
ISR_NOT_DEFINED(double_fault)
ISR_NOT_DEFINED(device_not_available)
ISR_NOT_DEFINED(coprocessor_segment_overrun) /* 10 */
ISR_NOT_DEFINED(invalid_TSS)
ISR_NOT_DEFINED(segment_not_present)
ISR_NOT_DEFINED(stack_exception)
ISR_NOT_DEFINED(general_protection)
// ISR_NOT_DEFINED(page_fault) /* 15 */
ISR_NOT_DEFINED(reserved)
ISR_NOT_DEFINED(floating_point_error)
ISR_NOT_DEFINED(alignment_check)
ISR_NOT_DEFINED(machine_check)
ISR_NOT_DEFINED(SIMD_floating_point_error) /* 20 */
