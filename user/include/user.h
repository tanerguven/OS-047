#ifndef _USER_LIB_USER_H_
#define _USER_LIB_USER_H_

#include <types.h>
extern void cputs(const char *s, size_t len);
extern int sleep(int ms);
extern int getpid();
extern int getmachineid();
extern int fork();
extern void exec(const char * s);
extern void exit(int status);
extern int cgetc();

#endif /* _USER_LIB_USER_H_ */
