#include <kernel/kernel.h>
#include <kernel/trap.h>
#include "network.h"
#include "../task.h"
#include "../kernel.h"

// main.c
extern void runProgram(const char * path);

extern int migration_recvTask(PackageHeader_t * header);


#define BUFSIZE 512*1024

static struct {
	size_t (*send)(void*,size_t);
	size_t (*recv)(void*,size_t);

	int nextNo;
	int state;

	uint8_t machineId;

	char buf[BUFSIZE];
} net;

void network_recv(void * buf, size_t size);
void network_send(void * buf, size_t size);

void package_init();

int getMachineId() {
	return net.machineId;
}

void network_init(size_t (*send_f)(void*,size_t), size_t (*recv_f)(void*,size_t)) {
	net.send = send_f;
	net.recv = recv_f;
	net.nextNo = 0;
	net.state = 0;
	net.machineId = 0;

	PackageInit_t package;
	netprot_createPackage(&package.header, net.nextNo++, 0, PACKAGE_TYPE_INIT, NULL, 0);

	network_send(&package, sizeof(package));
	print_info(">> package_size:%d\n", package.header.size);

	package_init();
}

void network_recv(void * buf, size_t size) {
	int i = 0;
	int rc;
	while (i < size) {
		rc = net.recv(((char*)buf) + i, size - i);
		ASSERT3(rc, >=, 0);
		i += rc;
	}
}

void network_send(void * buf, size_t size) {
	int r = net.send(buf, size);
	ASSERT(r == size);
}

extern void loadProgram(Task_t * task, const char * path);

extern void switch_to_task_migration(Task_t * newtask);

void package_command(PackageHeader_t * header) {
	int r;
	print_info("-- package_command\n");
	network_recv(net.buf, header->size);
	r = netprot_checkPackage(header, net.buf);
	if (r != 0) {
		print_info("paket hatali: %d\n", r);
		return;
	}

	uint32_t command = *(uint32_t*)net.buf;
	if (command == 1) {
		ASSERT3(getActiveTask()->programLoaded, ==, 1);

		r = do_fork();
		if (r == 0) {
			PANIC("HATA: bu satir calismamali");
		} else {
			char * path = ((char*)net.buf) + 4;
			Task_t * child = allTasks_get1(r);
			ASSERT(child != NULL);

			child->task_switch_method = switch_to_task_migration;
			loadProgram(child, path);
			return;
		}
	} else if (command == 2) {
		uint32_t * x = (uint32_t*)net.buf;
		int pid = x[1];

		print_info("migrate %d.%d\n", (pid>>20) & 0xff, pid & 0xfffff);
		Task_t * t = allTasks_get1(pid);
		ASSERT(t != NULL);
		t->migrationState = 1;
	}
}


void package_init() {
	int r;
	print_info("[network] machineId bekleniyor\n");
	PackageInitResponse_t response;
	network_recv(&response, sizeof(response));

	r = netprot_checkPackage(&response.header, &response.d);
	ASSERT3(r, ==, 0);

	net.machineId = response.d.machineId;
	print_info("MachineId = %d\n", net.machineId);
}

extern uint32_t asmstack;

void network_interrupt() {
	print_info("network_interrupt\n");

	int r;
	if (net.machineId == 0)
		return;

	PackageHeader_t header;
	network_recv(&header, sizeof(header));
	r = netprot_checkPackage(&header, NULL);
	if (r != 0) {
		print_info("r != 0");
		return;
	}

	if (header.size > BUFSIZE) {
		print_info("header.size > BUFSIZE");
		return;
	}

	switch (header.type) {
	case PACKAGE_TYPE_COMMAND:
		package_command(&header);
		break;
	case PACKAGE_TYPE_MIGRATION_TASKSTATE:
		migration_recvTask(&header);
		break;
	case PACKAGE_TYPE_MIGRATION:
		PANIC("HATA: bu paket PACKAGE_TYPE_MIGRATION_TASKSTATE icerisine gelmeli");
		break;
	default:
		PANIC("--");
	}

}
