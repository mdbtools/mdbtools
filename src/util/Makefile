
CC = gcc

INC = -I ../include `glib-config --cflags`
LIBS = -L ../libmdb -lmdb `glib-config --libs`
PROGS = prcat prkkd prtable prdata prdump schema
PRCATOBJS = prcat.o
PRKKDOBJS = prkkd.o
PRTABLEOBJS = prtable.o
PRDATAOBJS = prdata.o
PRDUMPOBJS = prdump.o
SCHEMAOBJS = schema.o

all: $(PROGS)

prcat: $(PRCATOBJS)
	$(CC) -g -o $@ $(PRCATOBJS) $(LIBS)
		
prkkd: $(PRKKDOBJS)
	$(CC) -g -o $@ $(PRKKDOBJS) $(LIBS)
		
prtable: $(PRTABLEOBJS)
	$(CC) -g -o $@ $(PRTABLEOBJS) $(LIBS)
		
prdata: $(PRDATAOBJS)
	$(CC) -g -o $@ $(PRDATAOBJS) $(LIBS)
		
prdump: $(PRDUMPOBJS)
	$(CC) -g -o $@ $(PRDUMPOBJS) $(LIBS)

schema: $(SCHEMAOBJS)
	$(CC) -g -o $@ $(SCHEMAOBJS) $(LIBS)

clean: 
	rm -f core *.o $(PROGS)

.c.o:
	$(CC) -g -c $< $(INC)
