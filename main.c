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
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/event.h>
#include <sys/ioctl.h>
#include <sysexits.h>
#include <unistd.h>
#include <util.h>

#include "time.h"
#include "linebuffer.h"

#define TS_WIDTH      (8 + 1 + 3) /* sec + '.' + nsec */
#define SEP_WIDTH     (3) /* " | " */

#define COLOR_RESET   "\x1b[0m"
#define COLOR_SEP     "\x1b[30;47m"
#define COLOR_ERR     "\x1b[30;101m"

#define FMT_TS        "%8ld.%03ld"
#define FMT_SEP       " " COLOR_SEP " " COLOR_RESET " "
#define FMT_SEP_ERR   " " COLOR_ERR " " COLOR_RESET " "
#define ARG_TS(ts)    ts.tv_sec, (ts.tv_nsec / NSEC_PER_MSEC)

/*
 * pipe(2) creates a descriptor pair where the 0 index is the output, and the 1
 * index is the input. Rather than hardcode these indexes everywhere, let's use
 * named indexes.
 */
enum {
	PIPE_OUT,
	PIPE_IN,
};

/* Dynamic line buffers */
static struct linebuffer *lb_stdout;
static struct linebuffer *lb_stderr;

/* Signal handling */
static volatile int interrupted;

static void interrupt(int sig) {
	assert(sig == SIGINT);

	interrupted++;
}

static void winch(int sig) {
	assert(sig == SIGWINCH);

	/* Get window size */
	struct winsize w;
	ioctl(fileno(stdout), TIOCGWINSZ, &w);

	/* Update buffer */
	size_t bufsize = w.ws_col ? (w.ws_col - TS_WIDTH - SEP_WIDTH) : PIPE_BUF;
	lb_resize(lb_stdout, bufsize);
	lb_resize(lb_stderr, bufsize);
}

static void become(int fds[], int target) {
	while ((dup2(fds[PIPE_IN], target) == -1) && (errno == EINTR));
	close(fds[PIPE_OUT]);
	close(fds[PIPE_IN]);
}

static void mkpipe(int fds[2], bool usepty) {
	if (usepty) {
		if(openpty(&fds[PIPE_OUT], &fds[PIPE_IN], NULL, NULL, NULL)) {
			err(EX_OSERR, "openpty");
		}
	} else {
		if(pipe(fds) == -1) {
			err(EX_OSERR, "pipe");
		}
	}
}

static __attribute__((noreturn)) void usage(void) {
	errx(EX_USAGE, "ltime [-p] command [arg1 ...]");
}

int main(int argc, char * const argv[]) {
	bool usepty = true;

	int ch;
	while ((ch = getopt(argc, argv, "p")) != -1) {
		switch (ch) {
			case 'p': {
					usepty = false;
				} break;
			default:
				usage();
		}
	}
	argc -= optind;
	argv += optind;

	/* Catch SIGINT to make sure we get a chance to print final stats */
	signal(SIGINT, interrupt);

	/* Setup stdout and stderr pipes */
	int stdout_pair[2];
	int stderr_pair[2];

	/* Create whichever pipe type is appropriate */
	mkpipe(stdout_pair, usepty);
	mkpipe(stderr_pair, usepty);

	/*
	 *  Fork and connect the pipes to the child process
	 *
	 *      <- Flow direction <-
	 *   PIPE_OUT  (pipe)  PIPE_IN
	 * Parent [==============] Child
	 *  child_stdout        stdout
	 *  child_stderr        stderr
	 */
	switch (vfork()) {
		case -1: { /* error */
			err(EX_OSERR, "vfork");
		} break;
		case 0: { /* child */
			become(stdout_pair, STDOUT_FILENO);
			become(stderr_pair, STDERR_FILENO);

			execvp(argv[0], argv);
			err(EX_OSERR, "execv");
		} break;
	}

	const int child_stdout = stdout_pair[PIPE_OUT];
	const int child_stderr = stderr_pair[PIPE_OUT];

	close(stdout_pair[PIPE_IN]);
	close(stderr_pair[PIPE_IN]);

	/* Get everything ready for kqueue */
	const struct timespec timeout = {
		.tv_nsec = NSEC_PER_MSEC,
	};

	struct kevent ev[2];
	EV_SET(ev + 0, child_stdout, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, 0);
	EV_SET(ev + 1, child_stderr, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, 0);

	const int kq = kqueue();
	if (kq == -1) {
		err(EX_OSERR, "kqueue");
	}

	/* Timestamp the process start */
	struct timespec last;
	clock_gettime(CLOCK_MONOTONIC, &last);
	const struct timespec start = last;

	/* Allocate line buffers */
	lb_stdout = lb_create();
	lb_stderr = lb_create();

	/* Set up terminal width info tracking */
	winch(SIGWINCH);
	signal(SIGWINCH, winch);

	bool wrap = false;
	bool nl = true;
	bool first = true;
	struct kevent triggered;
	struct timespec now, max = {0,0};
	const char *lastsep = FMT_SEP;
	for (int nev = 0; nev != -1; nev = kevent(kq, ev, 2, &triggered, 1, &timeout)) {
		int fd = (int)triggered.ident;
		struct linebuffer *lb = (fd == child_stdout ? lb_stdout : lb_stderr);
		const char *sep = (fd == child_stdout ? FMT_SEP : FMT_SEP_ERR);

		/* Get the timestamp of this output, and calculate the offset */
		clock_gettime(CLOCK_MONOTONIC, &now);
		const struct timespec diff = timespec_subtract(&now, &last);

		/* There is only one event at a time */
		if (nev) {
			if (triggered.flags & EV_EOF) {
				break;
			}

			if (nl || wrap) {
				if (!first) {
					if (nl) {
						printf(FMT_TS "%s", ARG_TS(diff), lastsep);
					} else if (wrap) {
						printf("%*s%s", TS_WIDTH, "", lastsep);
					}

					printf("\n");
				}
				lb_reset(lb);

				/* Update running statistics */
				if (timespec_compare(&diff, &max) > 0) {
					max = diff;
				}

				/* Update the last timestamp to diff against */
				last = now;
				first = false;
			}

			/* Read the triggering event */
			nl = lb_read(lb, fd);
			wrap = lb_full(lb);

			printf("%*s%s%s\r", TS_WIDTH, "", sep, lb->buf);
		} else if (!first) {
			/*
			 * 8 digits on the left-hand-side will allow for a process
			 * spanning ~3.17 years of runtime to not have problems
			 * with running out of timestamp columns.
			 */
			printf(FMT_TS "%s\r", ARG_TS(diff), sep);
		}
		fflush(stdout);

		/* Check to see if we got a SIGINT */
		if (interrupted) {
			break;
		}

		/* Store this separator for blanking out before the newline */
		lastsep = sep;
	}
	lb_destroy(lb_stdout);
	lb_destroy(lb_stderr);
	printf("\n");

	const struct timespec diff = timespec_subtract(&now, &start);
	printf("Total: %6lu.%06lu\n", diff.tv_sec, diff.tv_nsec / NSEC_PER_USEC);
	printf("Max:   %6lu.%06lu\n", max.tv_sec, max.tv_nsec / NSEC_PER_USEC);
	return EX_OK;
}
