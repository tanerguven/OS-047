#ifndef _TASK_H_
#define _TASK_H_

#include <kernel/trap.h>
#include "memory/memory.h"

typedef struct {
	uint32_t _ : 4;
	uint32_t m : 8;
	uint32_t t : 20;
} __attribute__((packed)) TaskId_t;

typedef struct Task {
	struct Task * prev, * next;

	uint32_t id; // machine:8 bit, task:20 bit
	int state; // process durumu

	uint32_t esp, ebp, eip; // kernel mode taskswitch regs
	addr_t stack_top; // kernel stack

	struct trapframe userRegs; // kullanici modu registerlari

	TaskMemory_t mem; // page table ve diger bellek islemleri

	int sleep_returnMs; // sleep sistem cagrisinda kullaniliyor
	int migrationState; // task migration icin kullaniliyor

	int exitStatus;

	uint32_t asmstack; // FIXME: silinecek
	int programLoaded; // FIXME: silinecek

	void (*task_switch_method)(struct Task*);

	int waitOnSwitch; // debug
} Task_t;

#define TASK_STATE_NEW 0
#define TASK_STATE_RUNNING 1
#define TASK_STATE_READY 2
#define TASK_STATE_WAITING 3
#define TASK_STATE_TERMINATED 4
#define TASK_STATE_MIGRATING 5
// ara islemlerde hata kontrolu icin kullaniliyor. bu durum calisma sirasinda kullanilmiyor.
#define TASK_STATE_REMOVED 1234


Task_t * getActiveTask();
int do_fork();

static inline TaskId_t readTaskId(int id) {
	return *(TaskId_t*)&id;
}

static inline int createTaskId(int machine, int id) {
	return ((machine & 0xff) << 20) | (id & 0xfffff);
}

#endif /* _TASK_H_ */
