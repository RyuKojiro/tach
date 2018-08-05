PROGNAME= tach
CFLAGS=   -Wall -ggdb -std=c99
SRCS=     src/main.c src/time.c src/linebuffer.c
OBJS=     $(SRCS:.c=.o)
PREFIX?=  /usr/local
DESTDIR?= /

$(PROGNAME): $(OBJS)
	$(CC) $(LDFLAGS) -o $(PROGNAME) $(OBJS)

install: $(PROGNAME)
	install -m 0755 $(PROGNAME) $(DESTDIR)/$(PREFIX)/bin

clean: 
	rm -f $(PROGNAME) $(OBJS)

.PHONY: clean install
	
.POSIX:
