
CC = gcc

INC = -I ../include `glib-config --cflags`
LIBS = -L ../libmdb -lmdb `glib-config --libs`
PROGS = prcat prtable
PRCATOBJS = prcat.o
PRTABLEOBJS = prtable.o

all: $(PROGS)

prcat: $(PRCATOBJS)
	$(CC) -g -o $@ $(PRCATOBJS) $(LIBS)
		
prtable: $(PRTABLEOBJS)
	$(CC) -g -o $@ $(PRTABLEOBJS) $(LIBS)
		
clean: 
	rm -f core *.o $(PROGS)

.c.o:
	$(CC) -g -c $< $(INC)
