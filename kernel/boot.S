/* boot.S - bootstrap the kernel */
/* Copyright (C) 1999, 2001  Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA. */

#define ASM_FILE        1
#include <kernel/multiboot.h>

/* C symbol format. HAVE_ASM_USCORE is defined by configure. */
#ifdef HAVE_ASM_USCORE
# define EXT_C(sym)                     _ ## sym
#else
# define EXT_C(sym)                     sym
#endif

#define STACK_SIZE                      0x4000

# define MULTIBOOT_HEADER_FLAGS         0x00000003

		.text
		.align  4
multiboot_header:
		.long   MULTIBOOT_HEADER_MAGIC
		.long   MULTIBOOT_HEADER_FLAGS
		.long   -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)

		.globl  start
start:

		mov   %ebx, _multiboot_info
		mov   %eax, _multiboot_magic

		/* Initialize the stack pointer. */
		movl    $(__boot_stack + STACK_SIZE - 0x200), %esp
		/* 0x200 -> stackin ustunden trapframe'nin sigabilecegi bir bosluk */

		/* Reset EFLAGS. */
		pushl   $0
		popf

		call    EXT_C(main)

loop:   hlt
        jmp     loop


halt_message:
		.asciz  "Halted."


		.global _multiboot_info, _multiboot_magic
_multiboot_info:
		.long 0
_multiboot_magic:
		.long 0

.bss
		/* stack alani. */
		.p2align 12
		.globl __boot_stack
__boot_stack:
		.space STACK_SIZE
		.globl __boot_stack_top
__boot_stack_top:
