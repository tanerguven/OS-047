#include <kernel/kernel.h>
#include "task.h"
#include "network/network.h"
#include <string.h>
#include "kernel.h"

#define BUFSIZE (64*1024)

static char buf[BUFSIZE];

extern int vmem_saveUserMemory(TaskMemory_t * mem, void * buf, size_t size, addr_t * start);

extern void allTasks_remove(Task_t * task);
extern void allTasks_add(Task_t * task);

// FIXME: --
#define KERNEL_STACK_SIZE 0x4000

#include <kernel/trap.h>

int migration_sendTask(Task_t * task) {

	int finished = 0;
	int r;
	addr_t start = 0;
	// memcpy(buf, (void*)(task->stack_top-KERNEL_STACK_SIZE), KERNEL_STACK_SIZE);

	uint32_t cr3;
	read_reg(%cr3, cr3);
	load_reg(%cr3, task->mem.pagedir_pa);

	PackageTaskState_t taskState;
	taskState.d.id = task->id;
	memcpy(&taskState.d.userRegs, &task->userRegs, sizeof(task->userRegs));

	r = netprot_createPackage(&taskState.header, -1, PACKAGE_TYPE_MIGRATION_TASKSTATE, 1, &taskState.d, sizeof(taskState.d));
	ASSERT3(r, ==, 0);
	network_send(&taskState, sizeof(taskState));


	PackageHeader_t package;
	while (!finished) {
		print_info("start: %x\n", start);
		addr_t lastStart = start;
		finished = vmem_saveUserMemory(&task->mem, (void*)buf, BUFSIZE, &start);
		ASSERT3(buf[0], !=, 0);

		r = netprot_createPackage(&package, -1, PACKAGE_TYPE_MIGRATION, 1, buf, start-lastStart);
		ASSERT3(r, ==, 0);

		network_send(&package, sizeof(package));
		network_send(buf, package.size);
		ASSERT3(buf[0], !=, 0);

		r = netprot_checkPackage(&package, buf);
		ASSERT3(r, ==, 0);
	}

	r = netprot_createPackage(&package, -1, PACKAGE_TYPE_MIGRATION, 1, NULL, 0);
	ASSERT3(r, ==, 0);
	network_send(&package, sizeof(package));

	ASSERT(task->state == TASK_STATE_MIGRATING);

	load_reg(%cr3, cr3);

	print_info("task migration OK\n");
	return 0;
}

extern void runProgram(const char * path);

extern Task_t * create_task(int id);

extern void switch_to_task_migration(Task_t * newtask);

extern void vmem_loadUserMemory(TaskMemory_t * mem, void * buf, size_t size);

int migration_recvTask(PackageHeader_t * header) {
	print_info("[%x] migration_recvTask, %d\n", getActiveTask()->id, header->size);
	int r;


	Task_t * newtask = create_task(234);
	ASSERT3((addr_t)newtask, !=, -1);

	if (newtask != NULL) { // parent

		PackageTaskState_t taskState;
		memcpy(&taskState.header, header, sizeof(*header));


		ASSERT3(header->size, ==, sizeof(taskState.d));
		network_recv(&taskState.d, header->size);
		r = netprot_checkPackage(&taskState.header, &taskState.d);
		ASSERT3(r, ==, 0);

		newtask->id = taskState.d.id;
		memcpy(&newtask->userRegs, &taskState.d.userRegs, sizeof(taskState.d.userRegs));

		// FIXME: load migrated kernel stack ???
		while (1) {
			network_recv(header, sizeof(*header));
			ASSERT3(header->type, ==, PACKAGE_TYPE_MIGRATION);
			r = netprot_checkPackage(header, NULL);
			ASSERT3(r, ==, 0);

			if (header->size == 0)
				break;

			ASSERT3(header->size, <=, BUFSIZE);
			network_recv(buf, header->size);
			r = netprot_checkPackage(header, buf);
			ASSERT3(r, ==, 0);

			vmem_loadUserMemory(&newtask->mem, buf, header->size);
		}

		newtask->task_switch_method = switch_to_task_migration;

		newtask->migrationState = 0;
		// task daha once makinede bulunduysa guncelle
		Task_t * t = allTasks_get1(newtask->id);
		if (t != NULL)
			allTasks_remove(t);

		allTasks_add(newtask);
		scheduler_addToRunning(newtask);

		print_info("OK\n");
	} else {
		PANIC("HATA: user modda baslamali");
	}


	return 0;
}

