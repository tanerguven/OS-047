#include <kernel/kernel.h>
#include <asm/io.h>
#include <asm/trap.h>

// picirq.c
extern void picenable(int irq);

// scheduler.c
void scheduler_timerUpdate(uint32_t counter);

void init_timer(uint32_t freq) {
	uint32_t divisor = 1193180 / freq;
	outb(0x43, 0x36);
	outb(0x40, divisor & 0xff);
	outb(0x40, (divisor>>8) & 0xff);
	picenable(IRQ_TIMER);
}

static uint32_t timerCounter = 0;

void isr_irq_timer() {
	timerCounter++;
	/*
	if (timerCounter % 100 == 0)
		print_info("timerCounter: %d\n", timerCounter);
	*/

	scheduler_timerUpdate(timerCounter);
}
