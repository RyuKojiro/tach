PROGNAME=ltime
CFLAGS=-Wall -ggdb
SRCS=ltime.c
OBJS=$(SRCS:.c=.o)

$(PROGNAME): $(OBJS)
	$(CC) $(LDFLAGS) -o $(PROGNAME) $(OBJS)

clean: 
	rm -f $(PROGNAME) $(OBJS)

.PHONY: clean
	
.POSIX:
