#ifdef _USER_PROGRAM
#include <user.h>
#endif

#include <stdio.h>
#include <string.h>

int main(int argc, char** argv) {

	while(1) {
		printf("[DDDDDD] machine:%x - pid:0x%x\n", getmachineid(), getpid());
	}

	return 0;
}
