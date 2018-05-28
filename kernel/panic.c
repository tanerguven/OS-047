#include <kernel/kernel.h>

void __panic(const char *msg, const char* file, int line) {
	asm("cli");
	asm("cld");
	print_error("Panic: %s:%d\n", file, line);
	print_error("%s\n", msg);
	while(1)
		asm("hlt");
}

void __panic_assert3(const char* file, int line, const char* c_a, uint32_t v_a,
					 const char *op, const char *c_b, uint32_t v_b) {
	asm("cli");
	asm("cld");
	print_error("Panic: %s:%d\n", file, line);
	print_error("assertion failed: %s %s %s\n", c_a, op, c_b);
	print_error("%s=%08x, %s=%08x\n", c_a, v_a, c_b, v_b);
	while(1)
		asm("hlt");
}

void __panic_assert(const char* file, int line, const char* d) {
	asm("cli");
	asm("cld");
	print_error("Panic: %s:%d\n", file, line);
	print_error("assertion failed: %s\n", d);
	while(1)
		asm("hlt");
}
