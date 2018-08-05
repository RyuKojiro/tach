PROGNAME=ltime
CFLAGS=-Wall -ggdb -std=c99
SRCS=src/main.c src/time.c src/linebuffer.c
OBJS=$(SRCS:.c=.o)

$(PROGNAME): $(OBJS)
	$(CC) $(LDFLAGS) -o $(PROGNAME) $(OBJS)

clean: 
	rm -f $(PROGNAME) $(OBJS)

.PHONY: clean
	
.POSIX:
