
CC = gcc

PROGS = mdb-dump
OBJS = dump.o 

all: $(PROGS)

clean: 
	rm -f core *.o $(PROGS)

mdb-dump: dump.o
	$(CC) -g -o $@ $< 

.c.o:
	$(CC) -g -c $<
