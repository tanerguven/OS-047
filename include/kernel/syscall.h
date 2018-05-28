#ifndef _KERNEL_SYSCALL_H_
#define _KERNEL_SYSCALL_H_

#include <kernel/trap.h>
#include <types.h>

#define SYS_cputs 1
#define SYS_sleep 2
#define SYS_getpid 3
#define SYS_getmachineid 4
#define SYS_fork 5
#define SYS_exec 6
#define SYS_exit 7
#define SYS_cgetc 8
#define MAX_SYSCALL_COUNT 1024

#define SYSCALL_DEFINE0(name)											\
	extern void sys_##name() {											\
	struct trapframe* tf __attribute__((unused)) = curr_registers();	\

#define SYSCALL_DEFINE1(name, type1, var1)		\
	SYSCALL_DEFINE0(name)						\
	type1 var1 = (type1)tf->regs.edx;

#define SYSCALL_DEFINE2(name, type1, var1, type2, var2) \
	SYSCALL_DEFINE1(name, type1, var1);					\
	type2 var2 = (type2)tf->regs.ecx;

#define SYSCALL_DEFINE3(name, type1, var1, type2, var2, type3, var3)	\
	SYSCALL_DEFINE2(name, type1, var1, type2, var2);					\
	type3 var3 = (type3)tf->regs.ebx;

#define SYSCALL_DEFINE4(name, type1, var1, type2, var2, type3, var3, type4, var4) \
	SYSCALL_DEFINE3(name, type1, var1, type2, var2, type3, var3);		\
	type4 var4 = (type4)tf->regs.edi;

#define SYSCALL_DEFINE5(name, type1, var1, type2, var2, type3, var3, type4, var4, type5, var5) \
	SYSCALL_DEFINE4(name, type1, var1, type2, var2, type3, var3, type4, var4); \
	type5 var5 = (type5)tf->regs.esi;

#define SYSCALL_END(name) }

static inline void set_return(struct trapframe *tf, uint32_t val) {
	tf->regs.eax = val;
}

#define SYSCALL_RETURN(val) \
	set_return(tf, (uint32_t)val);


static inline int32_t syscall(int num, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
{
	int32_t ret;
	asm volatile("int %1\n"
		: "=a" (ret)
		: "i" (T_SYSCALL),
		  "a" (num),
		  "d" (a1),
		  "c" (a2),
		  "b" (a3),
		  "D" (a4),
		  "S" (a5)
		: "cc", "memory");
	return ret;
}

#define _syscall0(r, name) \
	r name() {											\
		return (r)syscall(SYS_##name, 0, 0, 0, 0, 0);	\
	}

#define _syscall1(r, name, t1, p1) \
	r name(t1 p1) { \
		return (r)(syscall(SYS_##name, (uint32_t)p1, 0, 0, 0, 0));	\
	}

#define _syscall2(r, name, t1, p1, t2, p2)			\
	r name(t1 p1, t2 p2) {													\
		return (r)syscall(SYS_##name, (uint32_t)p1, (uint32_t)p2, 0, 0, 0); \
	}

#define _syscall3(r, name, t1, p1, t2, p2, t3, p3)							\
	r name(t1 p1, t2 p2, t3 p3) {												\
		return (r)syscall(SYS_##name, (uint32_t)p1, (uint32_t)p2, (uint32_t)p3, 0, 0); \
	}

#define _syscall4(r, name, t1, p1, t2, p2, t3, p3, t4, p4)					\
	r name(t1 p1, t2 p2, t3 p3, t4 p4) {										\
		return (r)syscall(SYS_##name, (uint32_t)p1, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, 0); \
	}

#define _syscall5(r, name, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5)			\
	r name(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5) {								\
		return (r)syscall(SYS_##name, (uint32_t)p1, (uint32_t)p2, (uint32_t)p3, (uint32_t)p4, (uint32_t)p5); \
	}

#endif /* _KERNEL_SYSCALL_H_ */
