
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

	struct kevent ev;
	EV_SET(&ev, fileno(stdin), EVFILT_READ, EV_ADD, 0, 0, 0);

	const int kq = kqueue();
	if (kq == -1) {
		err(EX_OSERR, "kqueue");
	}

	struct timespec last;

	int nev;
	struct kevent triggered;
	while ((nev = kevent(kq, &triggered, 1, &ev, 1, &timeout))) {
		if (nev == -1) {
			err(EX_IOERR, "kevent");
		}

		/* There is only one event at a time */
		if (nev) {
			/*
			 * 8 digits on the left-hand-side will allow for a process spanning
			 * ~3.17 years of runtime to not have problems with running out of
			 * timestamp columns.
			 */
			struct timespec now;
			clock_gettime(CLOCK_MONOTONIC, &now);

			const struct timespec diff = timespec_subtract(&now, &last);
			printf("%8lu.%3lu ] %s", diff.tv_sec, diff.tv_sec,
					(const char *)triggered.data); // TODO: Solve newlines :(

			last = now;
		}
	}

	printf("I'm alive\n");
	return EX_OK;
}
