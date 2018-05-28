/*
  kaynaklar
  - http://rachid.koucha.free.fr/tech_corner/pty_pdip.html
  - https://en.wikibooks.org/wiki/Serial_Programming/termios
*/

#define _XOPEN_SOURCE 600
#define _DEFAULT_SOURCE
#define _BSD_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <sys/time.h>
#include <assert.h>

#include <termios.h>
#include <string.h>
#include <pthread.h>

#include "network.h"

pthread_mutex_t mallocMutex = PTHREAD_MUTEX_INITIALIZER;

void * malloc2(size_t size) {
	int r;
	r = pthread_mutex_lock(&mallocMutex);
	if (r != 0)
		return NULL;

	void * ret = malloc(size);

	pthread_mutex_unlock(&mallocMutex);
	return ret;

}
#undef malloc
#define malloc (0)

static inline long getTimeMs() {
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC_RAW, &t);
	return t.tv_sec * 1000 + t.tv_nsec / 1000000;
}



int recvPackage(int fdm, void * buf, size_t size) {
	int i = 0;
	int rc;
	while (i < size) {
		rc = read(fdm, ((char*)buf) + i, size - i);
		// printf("read %lu - %d\n", size-i, rc);
		if (rc < 0)
			return -1;
		i += rc;
	}
	return 0;
}

int sendPackage(int fdm, void * buf, size_t size) {
	int i = 0;
	int rc;
	while (i < size) {
		rc = write(fdm, ((char*)buf) + i, size - i);
		// printf("write %lu - %d\n", size-i, rc);
		if (rc < 0)
			return -1;
		i += rc;
	}
	return 0;
}

int createPts() {
	int rc;
	int fdm = posix_openpt(O_RDWR);
	if (fdm < 0) {
		fprintf(stderr, "Error %d on posix_openpt()\n", errno);
		return -1;
	}

	rc = grantpt(fdm);
	if (rc != 0) {
		fprintf(stderr, "Error %d on grantpt()\n", errno);
		return -1;
	}

	rc = unlockpt(fdm);
	if (rc != 0) {
		fprintf(stderr, "Error %d on unlockpt()\n", errno);
		return -1;
	}

	struct termios termios_p;
	rc = tcgetattr(fdm, &termios_p);
	assert(rc == 0);

	// asagidaki ayarlar ile calisiyor. Ozellikle CR ayarinin kapatilmasi gerek.
	termios_p.c_iflag = IGNCR;
	termios_p.c_oflag = 0;

	rc = tcsetattr(fdm, TCSANOW, &termios_p);
	assert(rc == 0);

	rc = tcgetattr(fdm, &termios_p);
	assert(rc == 0);
	return fdm;

}

int no = 1;

void cmd_runProgram(int fdm, const char * name) {
	int rc;
	size_t len = strlen(name);

	PackageHeader_t package;
	size_t dataSize = sizeof(uint32_t) + 100;
	uint32_t * data = malloc2(dataSize);
	data[0] = 1;
	memcpy(data+1, name, len+1);

	rc = netprot_createPackage(&package, no++, PACKAGE_TYPE_COMMAND, 0, data, dataSize);
	assert(rc == 0);

	sendPackage(fdm, &package, sizeof(package));
	sendPackage(fdm, data, dataSize);
	free(data);
}

void cmd_startMigrate(int fdm, int machineId, int pid) {
	int rc;
	uint32_t data[2];
	data[0] = 2;
	data[1] = (machineId<<20) | pid;
	uint32_t dataSize = sizeof(data);

	PackageHeader_t package;
	rc = netprot_createPackage(&package, no++, PACKAGE_TYPE_COMMAND, 0, data, dataSize);
	assert(rc == 0);

	sendPackage(fdm, &package, sizeof(package));
	sendPackage(fdm, data, dataSize);
}

int migrate_read(int fdm, PackageTaskState_t * taskState, PackageHeader_t * headerList[10], char * dataList[10]) {
	int rc;
	int count = 0;

	rc = recvPackage(fdm, taskState, sizeof(*taskState));
	if (rc < 0)
		return -1;

	rc = netprot_checkPackage(&taskState->header, &taskState->d);
	if (rc != 0)
		return -2;

	if (taskState->header.type != PACKAGE_TYPE_MIGRATION_TASKSTATE)
		return -3;


	while (1) {
		PackageHeader_t * header = malloc2(sizeof(PackageHeader_t));
		rc = recvPackage(fdm, header, sizeof(*header));
		if (rc < 0)
			return -1;
		rc = netprot_checkPackage(header, NULL);
		if (rc < 0)
			return -2;

		if (header->type != PACKAGE_TYPE_MIGRATION)
			return -3;

		if (header->type == PACKAGE_TYPE_MIGRATION) {
			headerList[count] = header;
			dataList[count] = NULL;
			count++;
			if (header->size == 0)
				break;

			char * buf = malloc2(header->size);
			rc = recvPackage(fdm, buf, header->size);
			if (rc < 0)
				return -1;
			rc = netprot_checkPackage(header, buf);
			if (rc != 0)
				return -2;

			dataList[count-1] = buf;
		}
	}

	return count;
}

int migrate_write(int fdm, PackageTaskState_t * taskState, PackageHeader_t * headerList[10], char * dataList[10], int count) {
	int rc;
	int i;

	rc = netprot_checkPackage(&taskState->header, &taskState->d);
	assert(rc == 0);
	sendPackage(fdm, taskState, sizeof(*taskState));

	for (i = 0 ; i < count ; i++) {
		rc = netprot_checkPackage(headerList[i], NULL);
		if (rc != 0)
			return -1;

		rc = sendPackage(fdm, headerList[i], sizeof(*headerList[i]));
		if (rc < 0)
			return -2;

		if (dataList[i] == NULL) {
			assert(count-1 == i);
			break;
		}

		rc = netprot_checkPackage(headerList[i], dataList[i]);
		if (rc != 0)
			return -1;

		rc = sendPackage(fdm, dataList[i], headerList[i]->size);
		if (rc < 0)
			return -2;
	}

	return 0;
}

struct List {
	struct List * next;
	int cmd;

	int cmdStatus;
	PackageTaskState_t taskState;
	PackageHeader_t * headerList[10];
	char * bufList[10];
	int count;
};


typedef struct {
	int no;
	int fdm;

	pthread_mutex_t listMutex;
	struct List * last;

	int machineStarted;
} ThreadData_t;

int waitConnection(ThreadData_t * t) {
	int rc;
	PackageInit_t init;

	do {
		printf("[%d] baglanti bekleniyor, size: %lu\n", t->no, sizeof(init));
		recvPackage(t->fdm, &init, sizeof(init));
		rc = netprot_checkPackage(&init.header, NULL);
		if (rc != 0)
			printf("paket hatali\n");
		sleep(1);
	} while (rc != 0);

	printf("[%d] baglanti var\n", t->no);
	PackageInitResponse_t initResponse;
	initResponse.d.machineId = t->no;
	rc = netprot_createPackage(&initResponse.header, no++, PACKAGE_TYPE_INITRESPONSE, 0, &initResponse.d, sizeof(initResponse.d));
	printf("%d\n", rc);
	if (rc != 0)
		return -1;

	printf("[%d] sending response... %lu\n", t->no, sizeof(initResponse));
	sendPackage(t->fdm, &initResponse, sizeof(initResponse));

	return 0;
}

int runCommands(ThreadData_t * t) {
	char str[100];
	int rc;
	while (1) {
		// fgets(str, 100, stdin);

		while (1) {

			do {
				rc = pthread_mutex_lock(&t->listMutex);
			} while (rc != 0);

			struct List * l = t->last;
			if (t->last != NULL)
				t->last = l->next;
			pthread_mutex_unlock(&t->listMutex);


			if (l == NULL) {
				sleep(1);
				continue;
			}

			printf("[%d] cmd %d\n", t->no, l->cmd);

			if (l->cmd == 1) {
				cmd_runProgram(t->fdm, "loop");
			} else if (l->cmd == 2) {
				cmd_runProgram(t->fdm, "loopb");
			} else if (l->cmd == 3) {
				cmd_runProgram(t->fdm, "loopc");
			} else if (l->cmd == 15) {
				l->cmdStatus = 1;
				cmd_startMigrate(t->fdm, 1, 2);
				l->cmdStatus = 2;

				memset(l->bufList, 0, sizeof(l->bufList));
				printf("[%d] waiting...\n", t->no);
				l->count = migrate_read(t->fdm, &l->taskState, l->headerList, l->bufList);
				if (l->count < 0)
					return -1;

				printf("[%d] migrate_read OK : %d\n", t->no, l->count);
				l->cmdStatus = 3;
			} else if (l->cmd == 16) {
				l->cmdStatus = 1;
				rc = migrate_write(t->fdm, &l->taskState, l->headerList, l->bufList, l->count);
				if (rc != 0)
					return -1;
				printf("[%d] migrate_write OK : %d\n", t->no, l->count);
				l->cmdStatus = 3;
			}

		}


		cmd_runProgram(t->fdm, "loop");
		// cmd_runProgram(fdm, "emptyloop");

		// fgets(str, 100, stdin);

		// sleep(1);
		// cmd_runProgram(fdm, "loopb");

		while (1) {
			sleep(1);
			cmd_startMigrate(t->fdm, 1, 2);

			PackageTaskState_t taskState;
			PackageHeader_t * headerList[10];
			char * bufList[10];
			memset(bufList, 0, sizeof(bufList));
			printf("[%d] waiting...\n", t->no);
			int count = migrate_read(t->fdm, &taskState, headerList, bufList);
			if (count < 0)
				return -1;

			printf("[%d] migrate_read OK : %d\n", t->no, count);

			sleep(1);
			rc = migrate_write(t->fdm, &taskState, headerList, bufList, count);
			if (rc != 0)
				return -1;

			printf("[%d] migrate_write OK : %d\n", t->no, count);

			int i;
			for (i = 0 ; i < 10 ; i++) {
				if (bufList[i] != NULL)
					free(bufList[i]);
			}

			sleep(2);
		}


		fgets(str, 100, stdin);
	}

}

void * run(void * ptr) {
	char str[100];
	int rc;

	ThreadData_t * threadData = (ThreadData_t*)ptr;

	printf("[%d] pts: %s\n", threadData->no, ptsname(threadData->fdm));

	sprintf(str, "tmp/pts-%d.txt", threadData->no);
	FILE * f = fopen(str, "w");
	fprintf(f, "%s", ptsname(threadData->fdm));
	fclose(f);

	int state = 0;


	while (1) {
		if (state == 0) {
			threadData->machineStarted = 0;
			rc = waitConnection(threadData);
			if (rc == 0)
				state = 1;
		} else if (state == 1) {
			threadData->machineStarted = 1;
			rc = runCommands(threadData);
			if (rc != 0)
				state = 0;
		}
	}
}

void addCmd2(ThreadData_t * t, struct List * l) {
	int rc;
	do {
		rc = pthread_mutex_lock(&t->listMutex);
	} while (rc != 0);

	if (t->last == NULL)
		t->last = l;
	else
		t->last->next = l;

	pthread_mutex_unlock(&t->listMutex);
}

struct List * addCmd(ThreadData_t * t, int cmd) {
	struct List * l = malloc2(sizeof(struct List));
	memset(l, 0, sizeof(struct List));
	l->cmd = cmd;
	l->next = NULL;

	addCmd2(t, l);

	return l;
}

void run_migrationTest_1(ThreadData_t * threadData, int testNo, int machineCount) {
	printf("migrationTest_1\n");

	if (machineCount < 2 || machineCount > 10) {
		printf("ERROR: machine count: %d\n", machineCount);
		exit(1);
	}

	int machineId[10];
	int i;
	for (i = 0 ; i < machineCount ; i++)
		machineId[i] = i;

	int notReady = 1;
	while (notReady) {
		notReady = 0;
		for (i = 0 ; i < machineCount ; i++)
			notReady |= (!threadData[i].machineStarted);
	}

	// 1. makinede programi calistir
	int cmd;
	if (testNo == 1)
		cmd = 1;
	else
		cmd = 2;
	addCmd(&threadData[machineId[0]], cmd);
	int m = 0;

	while (1) {
		sleep(3);

		// programin calistigi makineye process migration istegi yap
		struct List * l = addCmd(&threadData[m], 15);
		while (l->cmdStatus != 3)
			sleep(1);

		m = (m + 1) % machineCount;

		// gelen paketi sonraki makineye gonder
		l->cmdStatus = 0;
		l->cmd = 16;
		addCmd2(&threadData[m], l);
		while (l->cmdStatus != 3)
			sleep(1);
	}
}

int main(int argc, char ** argv) {
	int machineCount = 1;
	if (argc >= 2) {
		machineCount = atoi(argv[1]);
	}

	int migrationTest_1 = 0;
	int testNo = 0;
	if (argc >= 3)
		migrationTest_1 = (strcmp(argv[2], "MigrationTest-1") == 0);
	if (argc >= 4)
		testNo = atoi(argv[3]);

	if (machineCount <= 0 || machineCount > 10) {
		printf("wrong parameter\n");
		return -1;
	}

	ThreadData_t threadData[10];
	memset(threadData, 0, sizeof(threadData));
	pthread_t thread[10];
	int i;
	for (i = 0 ; i < machineCount ; i++) {
		threadData[i].no = i+1;
		pthread_mutex_init(&threadData[i].listMutex, NULL);

		int fdm = createPts();
		threadData[i].fdm = fdm;

		pthread_create(&thread[i], NULL, run, (void*)&threadData[i]);
	}

	if (migrationTest_1) {
		run_migrationTest_1(threadData, testNo, machineCount);
	}


	char str[100];
	while (1) {
		printf(">>");
		fgets(str, 100, stdin);

		if (strncmp(str, "test", 4) == 0) {

			int A, B;
			if (str[5] == '\0') {
				A = 0;
				B = 1;
			} else {
				A = 1;
				B = 0;
			}

			struct List * l = addCmd(&threadData[A], 15);
			while (l->cmdStatus != 3)
				sleep(1);

			l->cmdStatus = 0;
			l->cmd = 16;
			addCmd2(&threadData[B], l);

			while (l->cmdStatus != 3)
				sleep(1);

			continue;
		}

		int no = str[0] - '1';
		if (no < 0 || no > 10)
			continue;

		int cmd = str[1] - '0';

		addCmd(&threadData[no], cmd);

	}

	int waitThreads = 1;
	while (waitThreads) {
		waitThreads = 0;
		for (i = 0 ; i < machineCount ; i++)
			waitThreads |= pthread_join(thread[i], NULL);
	}

	return 0;
}
