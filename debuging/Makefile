C = gcc
CFLAGS= -std=c99
LDLIBS=  -lpthread

all: debug_hashint parse

parse.o: parse.c 
	$(C) $(CFLAGS) -c parse.c $(LDLIBS)

parse: parse.o

debug_hashint.o: debug_hashint.c
	$(C) $(CFLAGS) -c debug_hashint.c $(LDLIBS)

debug_hashint: debug_hashint.o

clean:
	rm -f *~ *.o ./#* debug_hashint parse
