#ifndef IO_H_
#define IO_H_

#include <types.h>

static inline uint8_t inb(uint16_t port) {
	uint8_t data;
	asm volatile("in %1,%0" : "=a" (data) : "d" (port));
	return data;
}

static inline void outb(uint16_t port, uint8_t data) {
	asm volatile("out %0,%1" : : "a" (data), "d" (port));
}

#endif /* IO_H_ */
