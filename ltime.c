/*
 * Copyright (c) 2018 Daniel Loffgren
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <assert.h>
#include <err.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/event.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sysexits.h>
#include <time.h>

#define NSEC_PER_MSEC (1000000L)
#define NSEC_PER_SEC  (1000000000L)
#define TS_WIDTH      (8 + 1 + 3) /* sec + '.' + nsec */
#define SEP_WIDTH     (3) /* " ] " */

static char *buf;
static size_t bufsize;

static void winch(int sig) {
	assert(sig == SIGWINCH);

	/* Get window size */
	struct winsize w;
	ioctl(fileno(stdout), TIOCGWINSZ, &w);

	/* Update buffer */
	bufsize = w.ws_col - TS_WIDTH - SEP_WIDTH + 1;
	buf = realloc(buf, bufsize);
	buf = memset(buf, 0, bufsize);
}

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
		.tv_nsec = NSEC_PER_MSEC,
	};

	struct kevent ev;
	EV_SET(&ev, fileno(stdin), EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, 0);

	const int kq = kqueue();
	if (kq == -1) {
		err(EX_OSERR, "kqueue");
	}

	/* Timestamp the process start */
	struct timespec last;
	clock_gettime(CLOCK_MONOTONIC, &last);
	const struct timespec start = last;

	/* Set up terminal width info */
	winch(SIGWINCH);
	signal(SIGWINCH, winch);

	struct kevent triggered;
	for (int nev = 0; nev != -1; nev = kevent(kq, &ev, 1, &triggered, 1, &timeout)) {

		/* There is only one event at a time */
		if (nev) {
			if (triggered.flags & EV_EOF) {
				break;
			}

			/* Get the timestamp of this output, and calculate the offset */
			struct timespec now;
			clock_gettime(CLOCK_MONOTONIC, &now);
			const struct timespec diff = timespec_subtract(&now, &last);

			bool wrap = false;
			for (ssize_t got = 0; got < triggered.data; got += getline(&buf, &bufsize, stdin)) {
				/*
				 * 8 digits on the left-hand-side will allow for a process spanning
				 * ~3.17 years of runtime to not have problems with running out of
				 * timestamp columns.
				 */
				if(!wrap) {
					printf("%8ld.%03ld", diff.tv_sec, (diff.tv_nsec / NSEC_PER_MSEC));
				}
				else {
					printf("%*s", TS_WIDTH, "");
				}

				printf(" ] %s", buf);

				/*
				 * If there isn't a newline in this chunk -- perhaps there is
				 * more than one screen-width's worth of data, or stdout was
				 * fflushed without a newline -- then get the next chunk
				 * prepared to be a wrap.
				 */
				if(!strchr(buf, '\n')) {
					printf("\n");
					wrap = true;
				}
				else {
					wrap = false;
				}
			}

			/* Update the last timestamp to diff against */
			last = now;
		}
	}
	free(buf);

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	const struct timespec diff = timespec_subtract(&now, &start);
	printf("Total: %8ld.%03ld\n", diff.tv_sec, diff.tv_nsec);
	return EX_OK;
}
