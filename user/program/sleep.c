#ifdef _USER_PROGRAM
#include <user.h>
#endif

#include <stdio.h>
#include <string.h>

int main(int argc, char** argv) {
	int i;
	for (i = 0 ; ; i++) {
		int pid = getpid();
		int machineId = getmachineid();
		printf("[machine:%d - pid:%d.%d] %d\n", machineId, (pid>>20) & 0xff, pid & 0xfffff, i);
		sleep(300);
	}

	return 0;
}
