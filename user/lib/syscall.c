#include <types.h>
#include <kernel/syscall.h>

_syscall2(void, cputs, const char*, s, size_t, len);
_syscall1(int, sleep, int, ms);
_syscall0(int, getpid);
_syscall0(int, getmachineid);
_syscall0(int, fork);
_syscall1(void, exec, const char*, s);
_syscall1(void, exit, int, status);
_syscall0(int, cgetc);
