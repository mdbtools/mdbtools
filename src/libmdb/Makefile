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
LIBLIST = libmdb.a

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
