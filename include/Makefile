# Generated automatically from Makefile.in by configure.

SHELL = /bin/sh

top_srcdir = ..
srcdir = .
prefix = /usr/local
exec_prefix = ${prefix}
bindir = $(exec_prefix)/bin
infodir = $(exec_prefix)/info
libdir = $(exec_prefix)/lib
includedir = $(exec_prefix)/include
mandir = $(exec_prefix)/man/man1

CC = gcc
CPPFLAGS = 
CFLAGS = $(CPPFLAG) -g -O2
LDFLAGS = 
LIBS = -lmdb 
INSTALL= /usr/bin/install -c

HEADERS = mdbtools.h
OBJS = dump.o 

all: $(PROGS)

clean: 
	rm -f core *.o $(PROGS) Makefile

mdb-dump: dump.o
	$(CC) -g -o $@ $< 

.c.o:
	$(CC) $(CFLAGS) -g -c $<

install: 
	mkdir -p $(includedir)
	@for header in $(HEADERS); do \
	 echo "            copying $$header to $(includedir)"; \
	 cp $$header $(includedir); \
        done

uninstall: 
	@for header in $(HEADERS); do \
	 echo "            removing $$header from $(includedir)"; \
	 rm -f $(includedir)/$$header; \
        done
