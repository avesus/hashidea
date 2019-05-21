CC = gcc
CFLAGS = -O3 -g -std=c11 -D_GNU_SOURCE
LDLIBS = -lpthread -lm -lgsl -lgslcblas -lm -latomic
SRCS = $(wildcard *.c)
OBJS = $(patsubst %.c,%.o,$(SRCS))

all:	$(OBJS)

%.o:	%.c hash.h hashstat.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	/bin/rm -f *.o *~


