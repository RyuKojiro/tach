PROGNAME= tach
CFLAGS=   -Wall -ggdb -std=c99
LDFLAGS=  -lutil
SRCS=     src/main.c src/time.c src/linebuffer.c src/pipe.c
OBJS=     $(SRCS:.c=.o)
PREFIX?=  /usr/local
DESTDIR?= /
SECTION=  1
MANPAGE=  docs/$(PROGNAME).$(SECTION)

all: test

$(PROGNAME): $(OBJS)
	$(CC) $(LDFLAGS) -o $(PROGNAME) $(OBJS)

linux:
	$(MAKE) $(MAKEFLAGS) LDFLAGS="$(LDFLAGS) -lkqueue -lpthread -lrt" CFLAGS="$(CFLAGS) -D_POSIX_C_SOURCE=200809L -D_BSD_SOURCE -I/usr/include/kqueue"

install: $(PROGNAME)
	install -m 0755 $(PROGNAME) $(DESTDIR)/$(PREFIX)/bin
	install -m 0644 $(MANPAGE) $(DESTDIR)/$(PREFIX)/share/man/man$(SECTION)

clean: 
	rm -f $(PROGNAME) $(OBJS)

test: $(PROGNAME)
	tests/suite.exp

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: clean install linux test

.POSIX:
