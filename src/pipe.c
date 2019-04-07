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
#include <sysexits.h>
#include <unistd.h>

#if __FreeBSD__
#include <libutil.h>
#elif __linux__
#include <pty.h>
#else
#include <util.h>
#endif

#include "pipe.h"

/*
 * pipe(2) and openpty(3) create descriptor pairs where the 0 index is the
 * output, and the 1 index is the input. Rather than hardcode these indexes
 * everywhere, let's use named indices.
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
		if(openpty(&fds[PIPE_OUT], &fds[PIPE_IN], NULL, NULL, NULL)) {
			err(EX_OSERR, "openpty");
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
	const pid_t pid = vfork();
	switch (pid) {
		case -1: { /* error */
			err(EX_OSERR, "vfork");
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

	/* Check that the exec pipe was closed, indicating success */
	int rc;
	close(exec_pair[PIPE_IN]);
	if(read(exec_pair[PIPE_OUT], &rc, sizeof(int)) > 0) {
		errc(EX_OSERR, rc, NULL);
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
