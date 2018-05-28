#ifndef _UTIL_H_
#define _UTIL_H_

#include <types.h>
#include <string.h>

static inline uint32_t roundUp(uint32_t x) {
	return ((x+0x1000-1) & ~(0x1000-1));
}

static inline uint32_t roundDown(uint32_t x) {
	return (x & ~(0x1000-1));
}

static inline uint32_t isRounded(uint32_t x) {
	return !(x & (0xFFF));
}

static inline char isClean(uint8_t* ptr, uint32_t size) {
	uint32_t i;
	for (i = 0 ; i < size ; i++) {
		if (ptr[i] != 0)
			return 0;
	}
	return 1;
}

static inline char isSet(uint8_t* ptr, uint32_t size) {
	uint32_t i;
	for (i = 0 ; i < size ; i++) {
		if (ptr[i] != 0xFF)
			return 0;
	}
	return 1;
}

static inline uint8_t mem_equals_1(const void *a, const void* b) {
	return *(uint8_t*)a == *(uint8_t*)b;
}
static inline uint8_t mem_equals_2(const void *a, const void *b) {
	return *(uint16_t*)a == *(uint16_t*)b;
}
static inline uint8_t mem_equals_3(const void *a, const void *b) {
    return (*(uint32_t*)a & 0XFFFFFF) == (*(uint32_t*)b & 0xFFFFFF );
}
static inline uint8_t mem_equals_4(const void *a, const void *b) {
    return *(uint32_t*)a == *(uint32_t*)b;
}

#define __STR(x) #x
#define TO_STR(x) __STR(x)

#endif /* _UTIL_H_ */
