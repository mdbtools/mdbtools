
CC = gcc

INC = -I ../include `glib-config --cflags`
LIBS = -L ../libmdb -lmdb `glib-config --libs`
PROGS = prcat
PRCATOBJS = prcat.o

all: $(PROGS)

prcat: $(PRCATOBJS)
	$(CC) -g -o $@ $(PRCATOBJS) $(LIBS)
		
clean: 
	rm -f core *.o $(PROGS)

.c.o:
	$(CC) -g -c $< $(INC)
