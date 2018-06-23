PROGNAME=ltime
CFLAGS=-Wall
SRCS=ltime.c
OBJS=$(SRCS:.c=.o)

$(PROGNAME): $(OBJS)
	$(CC) $(LDFLAGS) -o $(PROGNAME) $(OBJS)
