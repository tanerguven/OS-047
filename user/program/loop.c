#ifdef _USER_PROGRAM
#include <user.h>
#endif

#include <stdio.h>
#include <string.h>

int main(int argc, char** argv) {

	printf("bir tusa basin\n");
	int c = cgetc();

	int i = 0;
	while(1) {
		printf("[0x%x] A %d, c==%c\n", getpid(), i++, c);
	}

	return 0;
}
