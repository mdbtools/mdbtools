# Generated automatically from Makefile.in by configure.

SHELL = /bin/sh

top_srcdir = .
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

ACLOCAL = @ACLOCAL@
AUTOCONF = @AUTOCONF@
AUTOMAKE = @AUTOMAKE@
AUTOHEADER = @AUTOHEADER@
RM = @RM@
subdirs = libmdb util extras

all: 
	@for dir in ${subdirs}; do \
	 (cd $$dir && $(MAKE) all) \
         || case "$(MFLAGS)" in *k*) fail=yes;; *) exit 1;; esac; \
        done && test -z "$$fail"
clean: 
	@for dir in ${subdirs} include; do \
	 (cd $$dir && $(MAKE) clean && rm -f Makefile) \
         || case "$(MFLAGS)" in *k*) fail=yes;; *) exit 1;; esac; \
         rm -f config.log config.status Makefile config.cache; \
        done && test -z "$$fail"
install: 
	@for dir in ${subdirs} include; do \
	 (cd $$dir && $(MAKE) install) \
         || case "$(MFLAGS)" in *k*) fail=yes;; *) exit 1;; esac; \
        done && test -z "$$fail"
uninstall: 
	@for dir in ${subdirs} include; do \
	 (cd $$dir && $(MAKE) uninstall) \
         || case "$(MFLAGS)" in *k*) fail=yes;; *) exit 1;; esac; \
        done && test -z "$$fail"

# automatic re-running of configure if the configure.in file has changed
${srcdir}/configure: configure.in
	cd ${srcdir} && autoconf

# autoheader might not change config.h.in so touch a stamp file
${srcdir}/config.h.in: stamp-h.in

${srcdir}/stamp-h.in: configure.in
	cd ${srcdir} && autoheader
	echo timestamp > ${srcdir}/stamp-h.in

config.h: stamp-h

stamp-h: config.h.in config.status
	./config.status

Makefile: Makefile.in config.status
	./config.status

config.status: configure
	./config.status --recheck

