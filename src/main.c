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
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/ioctl.h>
#include <sysexits.h>
#include <time.h>
#include <unistd.h>

#include "linebuffer.h"
#include "time.h"
#include "pipe.h"

/*
 * 8 digits on the left-hand-side will allow for a process
 * spanning ~3.17 years of runtime to not have problems
 * with running out of timestamp columns.
 */
#define TS_FMT        "%8ld.%03ld"
#define TS_WIDTH      (8 + 1 + 3) /* sec + '.' + nsec */
#define TS_ARG(ts)    ts.tv_sec, (ts.tv_nsec / NSEC_PER_MSEC)

#define SEP_WIDTH     (3) /* " | " */
#define SEP_FMT       COLOR_RESET " " COLOR_SEP " " COLOR_RESET " "
#define SEP_FMT_ERR   COLOR_RESET " " COLOR_ERR " " COLOR_RESET " "

#define COLOR_RESET   "\x1b[0m"
#define COLOR_SEP     "\x1b[30;47m"
#define COLOR_ERR     "\x1b[30;101m"
#define COLOR_FAST    "\x1b[90m"

#define EVENT_COUNT   (5)

/* Timer display refresh rate */
static const struct timespec timeout = {
	.tv_nsec = 17 * NSEC_PER_MSEC, /* ~60 Hz */
};

static void winch(struct linebuffer *lb_stdout, struct linebuffer *lb_stderr) {
	/* Get window size */
	struct winsize w;
	ioctl(fileno(stdout), TIOCGWINSZ, &w);

	/* Update buffer */
	size_t bufsize = w.ws_col ? (w.ws_col - TS_WIDTH - SEP_WIDTH) : PIPE_BUF;
	lb_resize(lb_stdout, bufsize);
	lb_resize(lb_stderr, bufsize);
}

static __attribute__((noreturn)) void usage(const char *progname) {
	errx(EX_USAGE, "usage: %s [-lp] command [arg0 ...]", progname);
}

int main(int argc, char * const argv[]) {
	bool slow = false;
	bool usepty = true;
	const char * const progname = argv[0];

	/* Process any command line flags */
	int ch;
	while ((ch = getopt(argc, argv, "lp")) != -1) {
		switch (ch) {
			case 'p': {
				usepty = false;
			} break;
			case 'l': {
				slow = true;
			} break;
			default:
				usage(progname);
		}
	}
	argc -= optind;
	argv += optind;

	/* Make sure we were actually given a command */
	if (argc == 0) {
		warnx("You must specify a command.");
		usage(progname);
	}

	/* Spawn the child and hook the pipes up */
	const struct descendent child = spawn(argv, usepty);

	/* Get everything ready for kqueue */
	struct kevent ev[EVENT_COUNT];
	EV_SET(ev + 0, child.out, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, NULL);
	EV_SET(ev + 1, child.err, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, NULL);
	EV_SET(ev + 2, child.pid, EVFILT_PROC, EV_ADD | EV_ENABLE, 0, 0, NULL);

	const int kq = kqueue();
	if (kq == -1) {
		err(EX_OSERR, "kqueue");
	}

	/* Timestamp the process start */
	struct timespec last;
	clock_gettime(CLOCK_MONOTONIC, &last);
	const struct timespec start = last;

	/* Allocate line buffers */
	struct linebuffer *lb_stdout = lb_create();
	struct linebuffer *lb_stderr = lb_create();

	/* Set up terminal width info tracking */
	winch(lb_stdout, lb_stderr);
	EV_SET(ev + 3, SIGWINCH, EVFILT_SIGNAL, EV_ADD, 0, 0, NULL);

	/*
	 * Catch SIGINT to make sure we get a chance to print final stats.
	 * The signal needs to be masked to ignore first, otherwise it will
	 * terminate the process before ever getting to the kqueue.
	 */
	signal(SIGINT, SIG_IGN);
	EV_SET(ev + 4, SIGINT, EVFILT_SIGNAL, EV_ADD, 0, 0, NULL);

	/* Set the events to listen for */
	int nev = kevent(kq, ev, EVENT_COUNT, NULL, 0, NULL);
	if (nev == -1) {
		err(EX_IOERR, "kevent (set)");
	}

	/* The main kevent loop */
	bool wrap = false;
	bool nl = true;
	bool first = true;
	struct kevent triggered;
	struct timespec now, max = {0,0};
	int numlines = 0;
	const char *lastsep = SEP_FMT;
	for (nev = 0; nev != -1; nev = kevent(kq, NULL, 0, &triggered, 1, &timeout)) {
		/* Get the timestamp of this output, and calculate the offset */
		clock_gettime(CLOCK_MONOTONIC, &now);
		const struct timespec diff = timespec_subtract(&now, &last);

		/*
		 * Handle the triggering event.
		 * The triggered structure shall not be accessed outside this block.
		 */
		if (nev) {
			/* Is the child done? */
			if (triggered.flags & EV_EOF) {
				break;
			}

			/* Did we get a signal? */
			if (triggered.filter == EVFILT_SIGNAL) {
				switch (triggered.ident) {
					case SIGWINCH:
						winch(lb_stdout, lb_stderr);
						continue;
					case SIGINT:
						goto done;
				}
			}

			/* Did the child exit? */
			if (triggered.filter == EVFILT_PROC && triggered.fflags & NOTE_EXIT) {
				break;
			}

			/* Figure out which fd it is, and assign the fd-specific variables */
			const int fd = (int)triggered.ident;
			const char *sep = (fd == child.out ? SEP_FMT : SEP_FMT_ERR);
			struct linebuffer *lb = (fd == child.out ? lb_stdout : lb_stderr);

			/* Finalize the previous line and advance */
			if (nl || wrap) {
				if (first) {
					/*
					 * Line number 0 is the lead up to the first line, it is
					 * ignored and discarded to avoid ending up with a time
					 * calumn that is always guaranteed to result in a blank
					 * line at the beginning of every tach invocation.
					 */
					numlines++;
					first = false;
				} else {
					if (nl) {
						/* Print the final timestamp for this line */
						if(diff.tv_sec == 0 && diff.tv_nsec <= 1000000) {
							printf(COLOR_FAST);
						}
						printf(TS_FMT "%s", TS_ARG(diff), lastsep);

						/* Update running statistics */
						if (timespec_compare(&diff, &max)) {
							max = diff;
						}

						/* Update the start-of-line timestamp we'll diff against */
						last = now;
						numlines++;
					} else if (wrap) {
						/* Blank out the timestamp for this line, since it wraps */
						printf("%*s%s", TS_WIDTH, "", lastsep);
					}

					printf("\n");
				}

				/* We have successfully flushed this line to the terminal */
				lb_reset(lb);
			}

			/* Read the triggering event */
			if(!lb_read(lb, fd, &nl)) {
				err(EX_IOERR, "read");
			}
			wrap = lb_full(lb);

			/* Normal idle timestamp update + linebuffer update */
			printf(TS_FMT "%s%s\r", TS_ARG(diff), sep, lb->buf);

			/* Store this separator for blanking out before the newline */
			lastsep = sep;
		} else if (!first && !slow) {
			/* Normal idle timestamp update */
			printf(TS_FMT "\r", TS_ARG(diff));
		}
		fflush(stdout);
	}

	/* Did we exit the loop because of a kevent error? */
	if (nev == -1) {
		err(EX_IOERR, "kevent");
	}

done:
	/* Final timestamp, just in case we spent time waiting on a signal or EOF */
	clock_gettime(CLOCK_MONOTONIC, &now);

	/* Cleanup */
	lb_destroy(lb_stdout);
	lb_destroy(lb_stderr);
	printf("\n");

	/* Final statistics */
	const struct timespec total = timespec_subtract(&now, &start);
	printf("Total: %6lu.%06lu across %u lines\n", total.tv_sec, total.tv_nsec / NSEC_PER_USEC, numlines);
	printf("Max:   %6lu.%06lu\n", max.tv_sec, max.tv_nsec / NSEC_PER_USEC);
	return EX_OK;
}
