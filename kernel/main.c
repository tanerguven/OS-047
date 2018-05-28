#include <kernel/kernel.h>
#include "task.h"
#include "memory/memory.h"
#include <string.h>
#include "kernel.h"

// memory.c
extern void memory_init(int runTest);

// isr.c
extern void isr_init();

// picirq.c
extern void picinit(void);

// timer.c
extern void init_timer(uint32_t freq);

// scheduler.c
extern void scheduler_init();

// task.c
extern void task_init();

// fs/initrd.c
extern void initrd_init(addr_t initrdStart, size_t initrdSize);
extern int initrd_getFileAddr(const char * path, addr_t * addr, size_t * size);

// fs/exec.c
extern int exec(addr_t fileStart, size_t fileSize);

// uart.c
extern void uart_init();
extern size_t uart_sendCom1(void * buf, size_t size);
extern size_t uart_recvCom1(void * buf, size_t size);
extern void uart_setInterrupt(int com, void (*f)());

// network.c
extern void network_init(size_t (*send)(void*,size_t), size_t (*recv)(void*,size_t));
extern void network_interrupt();

// console.c
extern void console_init();

// keyboard.c
extern void keyboard_init();

void runProgram(const char * path);

Task_t * task0;

addr_t initrdStart;
size_t initrdSize;
char initProgram[100];
int networkEnabled = 0;
void readConfiguration();
int isFileExists(const char * path);

int main() {
	int r;

	picinit();
	console_init();
	uart_init();
	keyboard_init();

	print_info("%s\n", "***** deneme *****");

	memory_init(0);

	pmem_getInitrdAddr(&initrdStart, &initrdSize);
	initrd_init(initrdStart, initrdSize);

	readConfiguration();

	isr_init();

	if (networkEnabled) {
		network_init(uart_sendCom1, uart_recvCom1);
		uart_setInterrupt(1, network_interrupt);
	}

	init_timer(100);
	scheduler_init();
	task_init();


	r = do_fork();
	if (r != 0) {
		task0 = getActiveTask();
		scheduler_removeFromRunning(task0);
		runProgram("emptyloop");
	}

	if (initProgram[0] != '\0') {
		runProgram(initProgram);
	}

	runProgram("sleep2");

	PANIC("END");

	return 0;
}

int isFileExists(const char * path) {
	addr_t fileStart;
	size_t fileSize;
	return 0 == initrd_getFileAddr(path, &fileStart, &fileSize);
}

void readConfiguration() {
	addr_t fileStart;
	size_t fileSize;

	int r;
	r = initrd_getFileAddr("init.txt", &fileStart, &fileSize);
	if (r == 0) {
		ASSERT3(fileSize, <, 100);
		strncpy(initProgram, (void*)fileStart, fileSize);
		if (initProgram[fileSize-1] == '\n')
			initProgram[fileSize-1] = '\0';
	} else {
		initProgram[0] = '\0';
	}

	if (isFileExists("NetworkEnabled"))
		networkEnabled = 1;
}

void runProgram(const char * path) {
	int r;
	addr_t fileStart;
	size_t fileSize;
	r = initrd_getFileAddr(path, &fileStart, &fileSize);
	if (r != 0) {
		print_info("[exec] file %s not found\n", path);
		return;
	}
	// ASSERT(r == 0);
	print_info("[exec] %s\n", path);
	r = exec(fileStart, fileSize);
}

extern int _exec(Task_t * task, addr_t fileStart, size_t fileSize);

void loadProgram(Task_t * task, const char * path) {
	int r;
	addr_t fileStart;
	size_t fileSize;
	r = initrd_getFileAddr(path, &fileStart, &fileSize);
	ASSERT(r == 0);
	print_info("[exec] %s\n", path);
	r = _exec(task, fileStart, fileSize);
}
