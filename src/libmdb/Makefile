
CC = gcc

INC = -I ../include
OBJS = catalog.o mem.o file.o 

all: libmdb

libmdb: $(OBJS)
	ar cq libmdb.a $(OBJS)
clean: 
	rm -f core *.o *.a 

.c.o:
	$(CC) -g -c $< $(INC)
