#ifndef __ERRNO_H
#define __ERRNO_H

/* /usr/include/asm-generic/(errno.h,errno-base.h) */

/** Operation not permitted */
#define EPERM 1
/** No souch file or directory */
#define ENOENT 2
/** No souch process */
#define ESRCH 3
/** Interrupted system call */
#define EINTR 4
/** I/O error */
#define EIO 5
/** Try again */
#define EAGAIN 11
/** Out of memory */
#define ENOMEM 12
/** Invalid argument */
#define EINVAL 22

/* no syscall */
#define ENOSYS 38
#endif /* __ERRNO_H */
