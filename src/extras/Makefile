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

PROGS = mdb-dump
OBJS = dump.o 

all: $(PROGS)

clean: 
	rm -f core *.o $(PROGS)

mdb-dump: dump.o
	$(CC) -g -o $@ $< 

.c.o:
	$(CC) -g -c $<

install: 
	@for prog in $(PROGS); do \
	 echo "             copying $$prog to $(bindir)"; \
	 cp $$prog $(bindir); \
        done

uninstall: 
	@for prog in $(PROGS); do \
	 echo "             removing $$prog from $(bindir)"; \
	 rm -f $(bindir)/$$prog; \
        done
