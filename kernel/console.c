#include <string.h>
#include <stdarg.h>
#include <types.h>
#include <asm/io.h>
#include <asm/trap.h>

// picirq.c
extern void picenable(int irq);

// lib/vsprintf.c
extern int vsprintf(char *buf, const char *fmt, va_list args);

static void cga_putc(int c);

void console_init() {

}

void console_putc(int c) {
	cga_putc(c);
}


int console_printf(const char *fmt, ...) {
	va_list args;
	int i;
	char buf[1024];

	va_start(args, fmt);
	i = vsprintf(buf, fmt, args);
	va_end(args);

	const char *c = buf;
	for (; *c != '\0' ; c++)
	    console_putc(*c);

	return i;
}

/*******************************************************************
 * 							uart
 *******************************************************************/

/*******************************************************************
 * 							CGA
 *******************************************************************/

#define CRTPORT 0x3d4

/*
 * 16 KB CGA memory
 * Ekranda 25 x 80 karakter var. Her karakter 16 bit.
 * Bir sayfa 4 KB.
 */
static uint16_t (*cga_memory)[80] = (uint16_t(*)[80])0xb8000;

#define TAB_SIZE 8
#define CHAR_COLOR 0x0700

static void cga_putc(int c) {
	int cursor_pos;

	// read cursor position
	outb(CRTPORT, 14);
	cursor_pos = inb(CRTPORT + 1) << 8;
	outb(CRTPORT, 15);
	cursor_pos |= inb(CRTPORT + 1);

	uint8_t cursor_x = cursor_pos % 80;
	uint8_t cursor_y = cursor_pos / 80;

	switch (c & 0xff) {
	case '\b': // backspace
		if (cursor_x | cursor_y) {
			if (cursor_x-- == 0) {
				cursor_x = 0;
				cursor_y--;
			}
			cga_memory[cursor_y][cursor_x] = (' ' & 0xff) | CHAR_COLOR;
		}
		break;
	case '\n': // enter
		cursor_y++;
		cursor_x = 0;
		break;
	case '\t': // tab
		cursor_x += TAB_SIZE - (cursor_x % TAB_SIZE);
		break;
	default:
		cga_memory[cursor_y][cursor_x] = (c & 0xff) | CHAR_COLOR;
		if (++cursor_x == 0) {
			cursor_x = 0;
			cursor_y++;
		}
	}

	if (cursor_y > 24) { // scroll up
		memmove(&cga_memory[0][0], &cga_memory[1][0], sizeof(cga_memory[0]) * 25);
		cursor_y--;
		memset(&cga_memory[cursor_y][cursor_x], 0, sizeof(cga_memory[0]) - cursor_x);
	}

	cursor_pos = cursor_y * 80 + cursor_x;

	outb(CRTPORT, 14);
	outb(CRTPORT+1, cursor_pos >> 8);
	outb(CRTPORT, 15);
	outb(CRTPORT+1, cursor_pos);
	cga_memory[cursor_y][cursor_x] = ' ' | CHAR_COLOR;
}
