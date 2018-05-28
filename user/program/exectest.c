#ifdef _USER_PROGRAM
#include <user.h>
#endif

#include <stdio.h>
#include <string.h>

int main(int argc, char** argv) {

	int pid = fork();
	while(1) {
		if (pid == 0) {
			printf("[%x] child process\n", getpid());
			exec("hello");
			printf("HATA!!! [%x] exec hatali\n", getpid());
		} else {
			printf("[%x] parent process. Child pid: %x\n", getpid(), pid);
		}
		sleep(1000);
	}

	return 0;
}
