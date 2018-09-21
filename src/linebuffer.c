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

#include "linebuffer.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void _lb_sanitycheck(struct linebuffer *lb) {
	/* Ensure buf is either NULL, or NULL terminated */
	assert((lb->buf == NULL) || (lb->buf[lb->cur] == '\0'));
}

struct linebuffer *lb_create(void) {
	return calloc(sizeof(struct linebuffer), 1);
}

void lb_destroy(struct linebuffer *line) {
	_lb_sanitycheck(line);

	if (line->buf) {
		free(line->buf);
	}
	if (line->tmp) {
		free(line->tmp);
	}
	free(line);
}

void lb_resize(struct linebuffer *line, size_t size) {
	line->buf = realloc(line->buf, size + 1);
	line->len = size;

	lb_reset(line);
}

void lb_reset(struct linebuffer *line) {
	memset(line->buf, 0, line->len + 1);
	line->cur = 0;
}

bool lb_read(struct linebuffer *line, int fd) {
	_lb_sanitycheck(line);
	assert(line->buf);

	/*
	 * If the previous line was terminated by a carriage return, reposition at
	 * the beginning of the linebuffer.
	 */
	if(line->cr) {
		line->cur = 0;
		line->buf[0] = '\0';
		line->cr = false;
	}

	char *now = line->buf + line->cur;
	bool newline = false;

	/*
	 * If there is a tmp string buffered up from a previous read, use that,
	 * otherwise actually read from the file descriptor.
	 */
	ssize_t cur;
	if (line->tmp) {
		cur = (ssize_t)strlen(line->tmp);
		strncpy(now, line->tmp, cur);
		free(line->tmp);
		line->tmp = NULL;
	} else {
		cur = read(fd, now, line->len - line->cur);
	}

	/*
	 * NULL terminate the buffer, regardless of where it came from.
	 * Technically, this makes the line buffer diverge from the screen contents
	 * whenever there is a carriage return followed by a shorter line. However,
	 * due to the fact that we will only end up printing up to the NULL
	 * terminator, the results will be visually identical.
	 */
	now[cur] = '\0';

	/*
	 * Don't allow reading of more than a single "line" per call. If we read
	 * too much, split the line in two, and put the remainder in a buffer for
	 * the next caller to get.
	 */
	char *nl = strpbrk(now, "\n\r");
	if (nl) {
		switch (*nl) {
			case '\n': {
				newline = true;
			} break;
			case '\r': {
				line->cr = true;
			} break;
		}

		/*
		 * If there is a newline and it's at the tail end, chop it off.
		 * If it's not the tail end, then split the buffer, hold onto the
		 * latter half, and return the first half.
		 */
		*nl = '\0';

		if (nl - now != cur - 1) {
			line->tmp = strdup(nl+1);
			cur = nl - now;
		} else {
			cur--;
		}
	}

	line->cur += (size_t)cur;

	_lb_sanitycheck(line);
	return newline;
}

bool lb_full(struct linebuffer *line) {
	_lb_sanitycheck(line);

	return line->cur == line->len;
}
