C = gcc
CFLAGS = -std=c99

all: c_trace input sp_tr

c_trace: c_trace.o

c_trace.o: c_trace.c 
	$(C) $(CFLAGS) -c c_trace.c

input: input.o

input.o: input.c 
	$(C) $(CFLAGS) -c input.c

sp_tr: sp_tr.o

sp_tr.o: sp_tr.c 
	$(C) $(CFLAGS) -c sp_tr.c

clean:
	rm -f *~ *.o ./#* c_trace input sp_tr
