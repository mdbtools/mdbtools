
CC = gcc

INC = -I ../include `glib-config --cflags`
LIBS = -L ../libmdb -lmdb `glib-config --libs`
PROGS = prcat prkkd prtable prdata prdump mdb-schema mdb-export mdb-tables mdb-header mdb-parsecsv
PRCATOBJS = prcat.o
PRKKDOBJS = prkkd.o
PRTABLEOBJS = prtable.o
PRDATAOBJS = prdata.o
PRDUMPOBJS = prdump.o
SCHEMAOBJS = schema.o
EXPORTOBJS = mdb-export.o
HEADEROBJS = header.o
TABLEOBJS = tables.o
CSVOBJS = parsecsv.o

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

mdb-schema: $(SCHEMAOBJS)
	$(CC) -g -o $@ $(SCHEMAOBJS) $(LIBS)

mdb-export: $(EXPORTOBJS)
	$(CC) -g -o $@ $(EXPORTOBJS) $(LIBS)

mdb-tables: $(TABLEOBJS)
	$(CC) -g -o $@ $(TABLEOBJS) $(LIBS)

mdb-header: $(HEADEROBJS)
	$(CC) -g -o $@ $(HEADEROBJS) $(LIBS)

mdb-parsecsv: $(CSVOBJS)
	$(CC) -g -o $@ $(CSVOBJS) $(LIBS)

clean: 
	rm -f core *.o $(PROGS)

.c.o:
	$(CC) -g -c $< $(INC)
