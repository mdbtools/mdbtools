
CC = gcc

INC = -I ../include `glib-config --cflags`
LIBS = -L ../libmdb -lmdb `glib-config --libs`
PROGS = prcat prkkd prtable prdata
PRCATOBJS = prcat.o
PRKKDOBJS = prkkd.o
PRTABLEOBJS = prtable.o
PRDATAOBJS = prdata.o

all: $(PROGS)

prcat: $(PRCATOBJS)
	$(CC) -g -o $@ $(PRCATOBJS) $(LIBS)
		
prkkd: $(PRKKDOBJS)
	$(CC) -g -o $@ $(PRKKDOBJS) $(LIBS)
		
prtable: $(PRTABLEOBJS)
	$(CC) -g -o $@ $(PRTABLEOBJS) $(LIBS)
		
prdata: $(PRDATAOBJS)
	$(CC) -g -o $@ $(PRDATAOBJS) $(LIBS)
		
clean: 
	rm -f core *.o $(PROGS)

.c.o:
	$(CC) -g -c $< $(INC)
