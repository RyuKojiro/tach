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
		.tv_nsec = NSEC_PER_SEC / 1000, /* 1ms */
	};

	struct kevent ev;
	EV_SET(&ev, fileno(stdin), EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, 0);

	const int kq = kqueue();
	if (kq == -1) {
		err(EX_OSERR, "kqueue");
	}

	struct timespec last;
	clock_gettime(CLOCK_MONOTONIC, &last);
	const struct timespec start = last;

	struct kevent triggered;
	struct timespec now;
	for (int nev = 0; nev != -1; nev = kevent(kq, &ev, 1, &triggered, 1, &timeout)) {

		/* There is only one event at a time */
		if (nev) {
			/*
			 * 8 digits on the left-hand-side will allow for a process spanning
			 * ~3.17 years of runtime to not have problems with running out of
			 * timestamp columns.
			 */
			clock_gettime(CLOCK_MONOTONIC, &now);

			const struct timespec diff = timespec_subtract(&now, &last);
			#define columns (80 - 15)
			char buf[columns];
			while (fgets(buf, columns, stdin)) {
				printf("%8ld.%03ld ] %s", diff.tv_sec, diff.tv_sec, buf);
			}

			last = now;
		}
	}

	clock_gettime(CLOCK_MONOTONIC, &now);
	const struct timespec diff = timespec_subtract(&now, &start);
	printf("Total: %8ld.%03ld\n", diff.tv_sec, diff.tv_sec);
	return EX_OK;
}
