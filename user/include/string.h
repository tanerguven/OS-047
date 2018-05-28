#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void *memcpy (void *__restrict __dest,
		     __const void *__restrict __src, size_t __n);

extern void *memset (void *__s, int __c, size_t __n);

extern int strcmp (__const char *__s1, __const char *__s2);

extern char *strcpy (char *__restrict __dest, __const char *__restrict __src);

extern size_t strlen (__const char *__s);

extern char *strcat (char *__restrict __dest, __const char *__restrict __src);

extern int strncmp (const char * s1,const char * s2, size_t n);

extern char* strncpy (char * s1, const char * s2, size_t n);

extern char* strncat (char *s1, const char *s2, size_t n);

extern int strnlen(const char *s, size_t size);

extern void *memmove (void *__dest, __const void *__src, size_t __n);

extern int memcmp(const void* ptr1, const void* ptr2, size_t num);

extern char *strchr(const char *s, int c);
extern char *strrchr(const char *s, int c);

#ifdef __cplusplus
}
#endif
