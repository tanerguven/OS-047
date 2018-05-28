#define ASM_FILE 1
#include <asm/descriptors.h>

#define ISR_NOERRCODE(name) \
		.globl asm_##name		;\
		.type asm##name, @function	;\
		.align 2				;\
asm_##name:	\
		push $0 /* error code */;\
		pushl %ds				;\
		pushl %es				;\
		pushal					;\
		movl $isr_##name, %ebx	;\
		jmp all_isr

#define ISR(name) \
		.globl asm_##name				;\
		.type asm_##name, @function	;\
		.align 2				;\
asm_##name:	\
		pushl %ds				;\
		pushl %es				;\
		pushal					;\
		movl $isr_##name, %ebx	;\
		jmp all_isr

all_isr:
		/* load kernel descriptors */
		movw $GD_KERNEL_DATA, %ax
		movw %ax, %ds
		movw %ax, %es
		movl %esp, asmstack		;\
		/* call function */
		push %esp
		call *%ebx
		/* pop function parameter */
		addl $4, %esp

		.globl isr_return
isr_return:
		/* pop trapframe */
		popal
		pop %es
		pop %ds
		/* pop error code */
		addl $4, %esp
		iret

loop:
		jmp loop

ISR_NOERRCODE(divide_error)
ISR_NOERRCODE(debug)
ISR_NOERRCODE(nmi)
ISR_NOERRCODE(int3)
ISR_NOERRCODE(overflow)

ISR_NOERRCODE(bounds_check)
ISR_NOERRCODE(invalid_opcode)
ISR_NOERRCODE(device_not_available)
ISR(double_fault)
ISR(coprocessor_segment_overrun)

ISR(invalid_TSS)
ISR(segment_not_present)
ISR(stack_exception)
ISR(general_protection)
ISR(page_fault)

ISR(reserved)
ISR_NOERRCODE(floating_point_error)
ISR(alignment_check)
ISR_NOERRCODE(machine_check)
ISR_NOERRCODE(SIMD_floating_point_error)
ISR_NOERRCODE(syscall)

ISR_NOERRCODE(irq_timer)
ISR_NOERRCODE(irq_keyboard)
ISR_NOERRCODE(irq_com1)
ISR_NOERRCODE(irq_com2)

ISR_NOERRCODE(irq_bugfix)

	.globl asmstack
	.data                       /* Put everything below in .data section */
	.align 2                    /* Align to a 4-byte (2^2) boundary */
asmstack:
	.long  0x01234567           /* A 32-bit variable named myint */
