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
CFLAGS = $(CPPFLAG) -g -O2 -DHW_LITTLE_ENDIAN
LDFLAGS = 
LIBS = 
INSTALL= @INSTALL@
LIBLIST = libmdb.a

INC = -I ../include `glib-config --cflags`
OBJS = catalog.o mem.o file.o kkd.o table.o data.o dump.o backend.o

all: libmdb

libmdb: $(OBJS)
	rm -f libmdb.a
	ar cq libmdb.a $(OBJS)
clean: 
	rm -f core *.o *.a 

.c.o:
	$(CC) $(CFLAGS) -g -c $< $(INC)

install:
	@for lib in $(LIBLIST); do \
	  echo "             copying $$lib to $(libdir)"; \
          cp $$lib $(libdir); \
	done

uninstall:
	@for lib in $(LIBLIST); do \
	  echo "             copying $$lib to $(libdir)"; \
          rm -f $(libdir)/$$lib; \
	done

backend.o: backend.c ../include/mdbtools.h
catalog.o: catalog.c ../include/mdbtools.h
data.o: data.c ../include/mdbtools.h
dump.o: dump.c ../include/mdbtools.h
file.o: file.c ../include/mdbtools.h
kkd.o: kkd.c ../include/mdbtools.h
mem.o: mem.c ../include/mdbtools.h
table.o: table.c ../include/mdbtools.h

