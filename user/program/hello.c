#ifdef _USER_PROGRAM
#include <user.h>
#endif

#include <stdio.h>
#include <string.h>

int main(int argc, char** argv) {

	printf("Hello World!\n");

	printf("bir tusa basin\n");
	int c = cgetc();
	printf("basilan tus: '%c'\n", c);

	return 0;
}
