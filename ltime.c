
#include <stdio.h>
#include <sysexits.h>
#include <sys/time.h>

int main(int argc, char * const argv[]) {
	(void)argc;
	(void)argv;

	struct timespec timeout = {
		.tv_nsec = 1000000, /* 1ms */
	};

	printf("I'm alive\n");
	return EX_OK;
}
