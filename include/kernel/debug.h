#ifndef _KERNEL_DEBUG_H
#define _KERNEL_DEBUG_H

#include <asm/x86.h>

#define __KERNEL_DEBUG_interrupt_check  1
#define __KERNEL_DEBUG_test 1

#define debug_test(level) (__KERNEL_DEBUG_##level == 1)

#if debug_test(test)
# define ASSERT_DTEST(p) ASSERT(p)
# define ASSERT3_DTEST(args...) ASSERT3(args)
#else
# define ASSERT_DTEST(p) (void)0
# define ASSERT3_DTEST(args...) (void)0
#endif

#if debug_test(interrupt_check)
# define ASSERT_int_disable() ASSERT(!(eflags_read() & FL_IF))
# define ASSERT_int_enable() ASSERT(eflags_read() & FL_IF)
#else
# define ASSERT_int_disable() (void)0
# define ASSERT_int_enable() (void)0
#endif

// TODO: fonksiyonlarin kac kere cagrildigini sayan birsey

#endif /* _KERNEL_DEBUG_H */
