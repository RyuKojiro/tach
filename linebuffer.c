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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

struct linebuffer *lb_create(void) {
	return calloc(sizeof(struct linebuffer), 1);
}

void lb_destroy(struct linebuffer *line) {
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
	char *now = line->buf + line->cur;
	bool newline = false;

	ssize_t cur;
	if (line->tmp) {
		cur = (ssize_t)strlen(line->tmp);
		strncpy(now, line->tmp, cur);
		free(line->tmp);
		line->tmp = NULL;
	} else {
		cur = read(fd, now, line->len - line->cur);
		now[cur] = '\0';
	}

	char *nl = strchr(now, '\n');
	if (nl) {
		/*
		 * If there is a newline and it's at the tail end, chop it off.
		 * If it's not the tail end, then split the buffer, hold onto the
		 * latter half, and return the first half.
		 */
		*nl = '\0';
		newline = true;

		if (nl - now != cur - 1) {
			line->tmp = strdup(nl+1);
			cur = nl - now;
		} else {
			cur--;
		}
	}

	line->cur += (size_t)cur;

	return newline;
}

bool lb_full(struct linebuffer *line) {
	return line->cur == line->len;
}
