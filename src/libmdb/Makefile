
CC = gcc

INC = -I ../include `glib-config --cflags`
OBJS = catalog.o mem.o file.o kkd.o table.o data.o dump.o

all: libmdb

libmdb: $(OBJS)
	rm -f libmdb.a
	ar cq libmdb.a $(OBJS)
clean: 
	rm -f core *.o *.a 

.c.o:
	$(CC) -g -c $< $(INC)
