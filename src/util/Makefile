# Generated automatically from Makefile.in by configure.


SHELL = /bin/sh

top_srcdir = ..
srcdir = .
prefix = /usr/local
exec_prefix = ${prefix}
bindir = $(exec_prefix)/bin
infodir = $(exec_prefix)/info
libdir = $(exec_prefix)/lib
mandir = $(exec_prefix)/man/man1

CC = gcc
CPPFLAGS = 
CFLAGS = $(CPPFLAG) -g -O2
LDFLAGS = 
LIBS = 
INSTALL= @INSTALL@
INC = -I ../include `glib-config --cflags`
LIBS = -L ../libmdb -lmdb `glib-config --libs`
PROGS = prcat prkkd prtable prdata prdump mdb-schema mdb-export mdb-tables mdb-header mdb-parsecsv mdb-array
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
ARROBJS = mdb-array.o

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

mdb-array: $(ARROBJS)
	$(CC) -g -o $@ $(ARROBJS) $(LIBS)

clean: 
	rm -f core *.o $(PROGS)

.c.o:
	$(CC) -g -c $< $(INC)

install: 
	@for prog in $(PROGS); do \
	 echo "               copying $$prog to $(bindir)"; \
	 cp $$prog $(bindir); \
        done

uninstall: 
	@for prog in $(PROGS); do \
	 echo "               removing $$prog from $(bindir)"; \
	 rm -f $(bindir)/$$prog; \
        done
