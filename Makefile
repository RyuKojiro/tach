PROGNAME=ltime
CFLAGS=-Wall -ggdb -std=c99
SRCS=ltime.c
OBJS=$(SRCS:.c=.o)

$(PROGNAME): $(OBJS)
	$(CC) $(LDFLAGS) -o $(PROGNAME) $(OBJS)

clean: 
	rm -f $(PROGNAME) $(OBJS)

.PHONY: clean
	
.POSIX:
