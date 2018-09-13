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

#include <stddef.h>
#include <stdbool.h>

struct linebuffer {
	/** The actual string, guaranteed to be NULL terminated. */
	char *buf;
	/** The buffer size. */
	size_t len;
	/** The current string length. */
	size_t cur;
	/** A temporary holding buffer for reads that span a newline */
	char *tmp;
	/** This flag indicates if the preceding line was terminated by a carriage
	 *  return. If this flag is set, the next lb_read will overwrite the
	 *  existing buffer contents. */
	bool cr;
};

struct linebuffer *lb_create(void);
void lb_destroy(struct linebuffer *line);
void lb_resize(struct linebuffer *line, size_t size);
void lb_reset(struct linebuffer *line);
bool lb_full(struct linebuffer *line);

/*
 * Like getline(3), but rather than including the newline it simply returns a
 * boolean indicating the presence of the newline.
 */
bool lb_read(struct linebuffer *line, int fd);
