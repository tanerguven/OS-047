#ifndef _TOOLS_NETWORK_H_
#define _TOOLS_NETWORK_H_

#ifndef KERNEL
#include <stdint.h>
#include <stdlib.h>
#include "../include/kernel/trap.h"
#else
#include <kernel/trap.h>
#endif

typedef struct {
	uint32_t h1;
	uint32_t size;
	int no;
	uint8_t type;
	uint8_t dest;
	uint32_t xor;
	uint32_t sum;
	uint32_t h2;

	// 4 byte align olmak zorunda
	uint16_t _1;
} __attribute__((packed)) PackageHeader_t;

typedef struct {
	PackageHeader_t header;
} __attribute__((packed)) PackageInit_t;

typedef struct {
	PackageHeader_t header;

	struct {
		uint8_t machineId;
		// 4 byte align
		uint8_t _1;
		uint16_t _2;
	} d;
} __attribute__((packed)) PackageInitResponse_t;

typedef struct {
	PackageHeader_t header;
	struct {
		uint32_t id;
		struct trapframe userRegs;
	} d;
} __attribute__((packed)) PackageTaskState_t;

/*
typedef struct {
	PackageHeader_t header;
	struct {
		int _;
	} d;
} __attribute__((packed)) PackageCommand_t;


typedef struct {
	PackageHeader_t header;
	struct {
		int _;
	} d;
} __attribute__((packed)) PackageMigration_t;
*/



#define PACKAGE_TYPE_INIT 0
#define PACKAGE_TYPE_INITRESPONSE 1

#define PACKAGE_TYPE_COMMAND 2
#define PACKAGE_TYPE_COMMANDRESPONSE 3

#define PACKAGE_TYPE_MIGRATION 4
#define PACKAGE_TYPE_MIGRATIONRESPONSE 5
#define PACKAGE_TYPE_MIGRATION_TASKSTATE 6

extern int netprot_createPackage(PackageHeader_t * header, int no, uint8_t type, uint8_t dest, void * data, size_t dataSize);
extern int netprot_checkPackage(PackageHeader_t * header, void * data);

#ifdef KERNEL
extern void network_recv(void * buf, size_t size);
extern void network_send(void * buf, size_t size);
#endif

#endif /* _TOOLS_NETWORK_H_ */
