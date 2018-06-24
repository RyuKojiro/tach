#include <assert.h>
#include <err.h>
#include <stdio.h>
#include <string.h>
#include <sys/event.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sysexits.h>
#include <time.h>

#define NSEC_PER_SEC (1000000000L)

static struct timespec timespec_subtract(const struct timespec *minuend,
		const struct timespec *subtrahend) {
	/*
	 * minuend - subtrahend = result
	 *
	 * The minuend must be larger than the subtrahend.
	 */
	assert(minuend->tv_sec >= subtrahend->tv_sec);
	if (minuend->tv_sec == subtrahend->tv_sec) {
		assert(minuend->tv_nsec >= subtrahend->tv_nsec);
	}

	/*
	 * Borrow from the seconds place.
	 *
	 * tv_nsec is a long, and so should be capable of holding a spare
	 * second (in nanoseconds) on all platforms.
	 */
	const int borrow = minuend->tv_nsec < subtrahend->tv_nsec;

	struct timespec result = {
		.tv_sec = minuend->tv_sec - borrow - subtrahend->tv_sec,
		.tv_nsec = minuend->tv_nsec + (borrow * NSEC_PER_SEC) - subtrahend->tv_nsec,
	};

	return result;
}

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
	clock_gettime(CLOCK_MONOTONIC, &last);

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
			printf("%8ld.%03ld ] %s", diff.tv_sec, diff.tv_sec,
					(const char *)triggered.data); // TODO: Solve newlines :(

			last = now;
		}
	}

	printf("I'm alive\n");
	return EX_OK;
}
