PROGNAME= tach
CFLAGS?=  -Wall -ggdb -std=c99
LDFLAGS=  -lutil
SRCS=     src/main.c src/time.c src/linebuffer.c src/pipe.c
OBJS=     $(SRCS:.c=.o)
PREFIX?=  /usr/local
DESTDIR?= /
SECTION=  1
MANPAGE=  docs/$(PROGNAME).$(SECTION)

$(PROGNAME): $(OBJS)
	$(CC) $(LDFLAGS) -o $(PROGNAME) $(OBJS)

linux:
	$(MAKE) $(MAKEFLAGS) LDFLAGS="$(LDFLAGS) -lkqueue -lpthread -lrt" CFLAGS="$(CFLAGS) -D_POSIX_C_SOURCE=200809L -D_BSD_SOURCE -I/usr/include/kqueue"

install: $(PROGNAME)
	install -m 0755 $(PROGNAME) $(DESTDIR)/$(PREFIX)/bin
	install -m 0644 $(MANPAGE) $(DESTDIR)/$(PREFIX)/share/man/man$(SECTION)

dtrace: src/probes.h src/probes.o
	$(MAKE) CFLAGS="$(CFLAGS) -DDTRACE" OBJS="$(OBJS) src/probes.o" $(MAKEFLAGS)

clean: 
	rm -f $(PROGNAME) $(OBJS) src/probes.o src/probes.h

src/probes.h: src/probes.d
	dtrace -h -o $@ -s src/probes.d

src/probes.o: src/probes.d
	$(MAKE) CFLAGS="$(CFLAGS) -DDTRACE" $(MAKEFLAGS) src/linebuffer.o
	dtrace -G -o $@ -s src/probes.d src/linebuffer.o

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: clean install linux
	
.POSIX:
