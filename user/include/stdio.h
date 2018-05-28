#ifndef __STDIO_H_
#define __STDIO_H_

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int getchar();
extern void putchar(int c);

// user/stdio.c
extern int	printf(const char *fmt, ...);

// lib/vsprintf.c
extern int vsprintf(char *buf, const char *fmt, va_list args);

extern int iscons(int fdnum);

#ifdef __cplusplus
}
#endif

#endif /* __STDIO_H_ */
