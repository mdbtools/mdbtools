
CC = gcc

INC = -I ../include `glib-config --cflags`
LIBS = -L ../libmdb -lmdb `glib-config --libs`
PROGS = prcat prkkd prtable
PRCATOBJS = prcat.o
PRKKDOBJS = prkkd.o
PRTABLEOBJS = prtable.o

all: $(PROGS)

prcat: $(PRCATOBJS)
	$(CC) -g -o $@ $(PRCATOBJS) $(LIBS)
		
prkkd: $(PRKKDOBJS)
	$(CC) -g -o $@ $(PRKKDOBJS) $(LIBS)
		
prtable: $(PRTABLEOBJS)
	$(CC) -g -o $@ $(PRTABLEOBJS) $(LIBS)
		
clean: 
	rm -f core *.o $(PROGS)

.c.o:
	$(CC) -g -c $< $(INC)
