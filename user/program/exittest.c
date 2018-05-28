#ifdef _USER_PROGRAM
#include <user.h>
#endif

#include <stdio.h>
#include <string.h>

int main(int argc, char** argv) {

	int i = 0;
	while(1) {
		printf("[0x%x] A %d\n", getpid(), i++);
		if (i == 10)
			exit(0);
	}

	return 0;
}
