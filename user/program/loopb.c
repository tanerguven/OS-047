#ifdef _USER_PROGRAM
#include <user.h>
#endif

#include <stdio.h>
#include <string.h>

int main(int argc, char** argv) {

	int i = 0;
	while(1) {
		printf("[BBBBBB] machine:%x - pid:0x%x counter:%d\n", getmachineid(), getpid(), i++);
	}

	return 0;
}
