#include <kernel/kernel.h>

#include <asm/io.h>
#include <asm/trap.h>
#include <string.h>

// picirq.c
extern void picenable(int irq);

static struct {
	uint16_t COM1;
	uint16_t COM2;
	uint16_t COM3;
	uint16_t COM4;

	void (*COM1_interrupt)();
	void (*COM2_interrupt)();
} uart;

static void uart_init_COM(uint16_t port) {
	/* https://wiki.osdev.org/Serial_Ports */

	outb(port+2, 0); // Turn off the FIFO

	// 115200 baud, 8 data bits, 1 stop bit, parity off.
	outb(port+3, 0x80);    // Unlock divisor
	outb(port+0, 115200/115200);
	outb(port+1, 0);
	outb(port+3, 0x03);    // Lock divisor, 8 data bits.
	outb(port+4, 0);
	outb(port+1, 0x01);    // Enable receive interrupts.
	outb(port+2, 0xC7); // Enable FIFO, clear them, with 14-byte threshold

	if(inb(port+5) == 0xFF) {
		console_printf("!!! uart not found : %x\n", port);
		return;
	}

	inb(port+2);
	inb(port+0);
}

void uart_init() {
	// http://wiki.osdev.org/Memory_Map_(x86)#BIOS_Data_Area_.28BDA.29
	uart.COM1 = *(uint16_t*)0x0400;
	uart.COM2 = *(uint16_t*)0x0402;
	uart.COM3 = *(uint16_t*)0x0404;
	uart.COM4 = *(uint16_t*)0x0406;

	if (uart.COM1 > 0) {
		uart_init_COM(uart.COM1);
		picenable(IRQ_COM1);
	}

	if (uart.COM2 > 0) {
		uart_init_COM(uart.COM2);
		picenable(IRQ_COM2);
	}
}

size_t uart_sendCom1(void * buf, size_t size) {
	ASSERT(uart.COM1 != 0);
	size_t i;
	char * b = (char*)buf;
	for (i = 0 ; i < size ; i++) {
		while ((inb(uart.COM1 + 5) & 0x20) == 0)
			/* if transmit not empty, wait */;
		// delay(1);
		outb(uart.COM1, b[i]);
	}
	return i;
}

size_t uart_recvCom1(void * buf, size_t size) {
	ASSERT(uart.COM1 != 0);
	size_t i = 0;
	char * b = (char*)buf;

	while (i < size && (inb(uart.COM1+5) & 0x01)) {
		char c = inb(uart.COM1);
		b[i++] = c;
	}
	return i;
}

void uart_setInterrupt(int com, void (*f)()) {
	if (com == 1)
		uart.COM1_interrupt = f;
	else if (com == 2)
		uart.COM2_interrupt = f;
	else
		PANIC("hatali parametre");
}


void isr_irq_com2() {
	if (! (inb(uart.COM2+5) & 0x01))
		return;

	if (uart.COM2_interrupt)
		uart.COM2_interrupt();
	else
		PANIC("--");
}

void isr_irq_com1() {

	if (! (inb(uart.COM1+5) & 0x01))
		return;

	if (uart.COM1_interrupt)
		uart.COM1_interrupt();
	else
		PANIC("--");
}
