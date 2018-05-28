#include <stdarg.h>
#include "stdio.h"

#include "user.h"

static char buf[1024];

int printf(const char *fmt, ...) {
	va_list args;
	int i;

	va_start(args, fmt);
	i = vsprintf(buf, fmt, args);
	va_end(args);

	cputs(buf, strlen(buf));

	return i;
}

/*
int getchar() {
	return sys_cgetc();
}

void putchar(int c) {
	printf("%c", c);
}

int iscons(int fdnum) {
	return 1;
}
*/
