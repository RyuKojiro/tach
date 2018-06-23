
#include <err.h>
#include <stdio.h>
#include <sys/event.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sysexits.h>

int main(int argc, char * const argv[]) {
	(void)argc;
	(void)argv;

	const struct timespec timeout = {
		.tv_nsec = 1000000, /* 1ms */
	};

	const int in = fileno(stdin);
	const int out = fileno(stdout);

	struct kevent ev;
	EV_SET(&ev, in, EVFILT_READ, EV_ADD, 0, 0, 0);

	const int kq = kqueue();
	if (kq == -1) {
		err(EX_OSERR, "kqueue");
	}

	printf("I'm alive\n");
	return EX_OK;
}
