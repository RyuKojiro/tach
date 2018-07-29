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

#define TS_WIDTH      (8 + 1 + 3) /* sec + '.' + nsec */
#define SEP_WIDTH     (3) /* " | " */

#define COLOR_RESET   "\x1b[0m"
#define COLOR_SEP     "\x1b[30;47m"

#define FMT_TS        "%8ld.%03ld"
#define FMT_SEP       " " COLOR_SEP " " COLOR_RESET " "
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

/* Dynamic line buffer */
static char *buf;
static size_t bufsize;

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
	bufsize = w.ws_col ? (w.ws_col - TS_WIDTH - SEP_WIDTH) : PIPE_BUF;
	buf = realloc(buf, bufsize + 1);
	buf = memset(buf, 0, bufsize + 1);
}

/*
 * Like getline(3), but rather than including the newline it simply
 * indicates the presence of the newline.
 */
static size_t readln(int fd, char *buf, size_t len, bool *newline) {
	static char *leftovers;
	*newline = false;

	ssize_t cur;
	if (leftovers) {
		cur = (ssize_t)strlen(leftovers);
		strncpy(buf, leftovers, cur);
		free(leftovers);
		leftovers = NULL;
	} else {
		cur = read(fd, buf, len);
		buf[cur] = '\0';
	}

	char *nl = strchr(buf, '\n');
	if (nl) {
		/*
		 * If there is a newline and it's at the tail end, chop it off.
		 * If it's not the tail end, then split the buffer, hold onto the
		 * latter half, and return the first half.
		 */
		*nl = '\0';
		*newline = true;

		if (nl - buf != cur - 1) {
			leftovers = strdup(nl+1);
			cur = nl - buf;
		} else {
			cur--;
		}
	}
	return (size_t)cur;
}

static void become(int fds[], int target) {
	while ((dup2(fds[PIPE_IN], target) == -1) && (errno == EINTR));
	close(fds[PIPE_OUT]);
	close(fds[PIPE_IN]);
}

int main(int argc, char * const argv[]) {
	(void)argc;
	(void)argv;

	/* Catch SIGINT to make sure we get a chance to print final stats */
	signal(SIGINT, interrupt);

	/* Setup stdout and stderr pipes */
	int stdout_pair[2];
	int stderr_pair[2];

	if(openpty(&stdout_pair[PIPE_OUT], &stdout_pair[PIPE_IN], NULL, NULL, NULL) ||
	   pipe(stderr_pair) == -1) {
		err(EX_OSERR , "pipe");
	}

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

			execvp(argv[1], argv + 1);
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

	/* Set up terminal width info tracking and buffer allocation */
	winch(SIGWINCH);
	signal(SIGWINCH, winch);

	bool wrap = false;
	bool nl = true;
	bool first = true;
	struct kevent triggered;
	struct timespec now, max = {0,0};
	for (int nev = 0; nev != -1; nev = kevent(kq, ev, 1, &triggered, 1, &timeout)) {

		/* Get the timestamp of this output, and calculate the offset */
		clock_gettime(CLOCK_MONOTONIC, &now);
		const struct timespec diff = timespec_subtract(&now, &last);

		/* There is only one event at a time */
		if (nev) {
			if (triggered.flags & EV_EOF) {
				break;
			}

			if (!first) {
				/* Advance the line */
				printf("\n");
			}

			size_t got = 0;
			do {
				got += readln(child_stdout, buf + got, bufsize - got, &nl);

				if (wrap) {
					/*
					 * If there isn't a newline in this chunk -- perhaps there
					 * is more than one screen-width's worth of data, or
					 * stdout was fflushed without a newline -- then get the
					 * next chunk prepared to be a wrap.
					 */
					printf("%*s" FMT_SEP, TS_WIDTH, "");
				} else {
					printf(FMT_TS FMT_SEP, ARG_TS(diff));
				}
				printf("%s\r", buf);

				wrap = !nl;
			} while (!nl && got < bufsize);

			/* Update running statistics */
			if (timespec_compare(&diff, &max) > 0) {
				max = diff;
			}

			/* Update the last timestamp to diff against */
			last = now;
			first = false;
		} else if (!first) {
			/*
			 * 8 digits on the left-hand-side will allow for a process
			 * spanning ~3.17 years of runtime to not have problems
			 * with running out of timestamp columns.
			 */
			printf(FMT_TS FMT_SEP "\r", ARG_TS(diff));
		}
		fflush(stdout);

		/* Check to see if we got a SIGINT */
		if (interrupted) {
			break;
		}
	}
	free(buf);
	printf("\n");

	const struct timespec diff = timespec_subtract(&now, &start);
	printf("Total: %6lu.%06lu\n", diff.tv_sec, diff.tv_nsec / NSEC_PER_USEC);
	printf("Max:   %6lu.%06lu\n", max.tv_sec, max.tv_nsec / NSEC_PER_USEC);
	return EX_OK;
}
