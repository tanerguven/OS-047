#ifndef _INCLUDE_KERNEL_H_
#define _INCLUDE_KERNEL_H_

#define _KERNEL_SRC_

#include <types.h>

// panic.cpp
extern void __panic(const char *msg, const char* file, int line);
extern void __panic_assert(const char* file, int line, const char* d);
extern void __panic_assert3(const char* file, int line, const char *c_a, uint32_t v_a,
							const char *op, const char *c_b, uint32_t v_b);

// console.c
extern int console_printf(const char *fmt, ...);


#define PANIC(msg) __panic(msg, __FILE__, __LINE__)
#define ASSERT(b) ((b) ? (void)0 : __panic_assert(__FILE__, __LINE__, #b))
#define ASSERT3(a, op, b) \
	((a op b) ? (void)0 : __panic_assert3(__FILE__, __LINE__, #a, a, #op, #b, b))

#define print_info(args...) console_printf(args)
#define print_warning(args...) console_printf(args)
#define print_error(args...) console_printf(args)

#include "debug.h"

#endif /* _INCLUDE_KERNEL_H_ */
