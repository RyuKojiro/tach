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

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sysexits.h>
#include <unistd.h>

#include "pipe.h"

/*
 * mkpipe creates a descriptor pair where the 0 index is the output, and the 1
 * index is the input, ala pipe(2). Rather than hardcode these indices
 * everywhere, let's give them names.
 */
enum {
	PIPE_OUT,
	PIPE_IN,
};

static void become(int fds[], int target) {
	while ((dup2(fds[PIPE_IN], target) == -1) && (errno == EINTR));
	close(fds[PIPE_OUT]);
	close(fds[PIPE_IN]);
}

static void mkpipe(int fds[2], bool usepty) {
	if (usepty) {
		if ((fds[PIPE_OUT] = posix_openpt(O_RDWR|O_NOCTTY)) < 0) {
			err(EX_OSERR, "posix_openpt");
		}

		if (grantpt(fds[PIPE_OUT])) {
			err(EX_OSERR, "grantpt");
		}

		if (unlockpt(fds[PIPE_OUT])) {
			err(EX_OSERR, "unlockpt");
		}

		char *slave;
		if(!(slave = ptsname(fds[PIPE_OUT]))) {
			err(EX_OSERR, "ptsname");
		}

		if((fds[PIPE_IN] = open(slave, O_RDWR|O_NOCTTY)) < 0) {
			err(EX_OSERR, "open");
		}
	} else {
		if(pipe(fds) == -1) {
			err(EX_OSERR, "pipe");
		}
	}
}

static void cloexec(int fd) {
	int flags = fcntl(fd, F_GETFD);
	if(fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == -1) {
		err(EX_OSERR, "fcntl");
	}
}

struct descendent spawn(char * const argv[], bool usepty) {
	/* Setup stdout and stderr pipes */
	int stdout_pair[2];
	int stderr_pair[2];

	/* Create whichever pipe type is appropriate */
	mkpipe(stdout_pair, usepty);
	mkpipe(stderr_pair, usepty);

	/* Create a close-on-exec pipe pair for communicating the exec outcome */
	int exec_pair[2];
	mkpipe(exec_pair, false);
	cloexec(exec_pair[PIPE_IN]);
	cloexec(exec_pair[PIPE_OUT]);

	/*
	 *  Fork and connect the pipes to the child process
	 *
	 *      <- Flow direction <-
	 *   PIPE_OUT  (pipe)  PIPE_IN
	 * Parent [==============] Child
	 *  child_stdout        stdout
	 *  child_stderr        stderr
	 */
	const pid_t pid = fork();
	switch (pid) {
		case -1: { /* error */
			err(EX_OSERR, "fork");
		}
		case 0: { /* child */
			become(stdout_pair, STDOUT_FILENO);
			become(stderr_pair, STDERR_FILENO);

			execvp(argv[0], argv);

			/* exec failed */
			write(exec_pair[PIPE_IN], &errno, sizeof(int));
			err(EX_OSERR, "execv");
		}
	}

	/*
	 * The parent must close its exec_pair[PIPE_IN] to make sure the child's
	 * dup is the only one PIPE_IN side remaining.
	 */
	close(exec_pair[PIPE_IN]);

	/* Check that the exec pipe was closed, indicating success */
	int rc;
	if(read(exec_pair[PIPE_OUT], &rc, sizeof(int)) > 0) {
		errno = rc;
		err(EX_OSERR, NULL);
	}
	close(exec_pair[PIPE_OUT]);

	/* Prepare the return value */
	const struct descendent result = {
		.pid = pid,
		.out = stdout_pair[PIPE_OUT],
		.err = stderr_pair[PIPE_OUT],
	};

	/* Close the parent-side input pipes. Communication is unidirectional */
	close(stdout_pair[PIPE_IN]);
	close(stderr_pair[PIPE_IN]);

	return result;
}
