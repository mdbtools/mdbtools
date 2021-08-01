ACLOCAL_AMFLAGS	=	-I m4
SUBDIRS	=	src include doc

DEFDIR	=	$(prefix)

EXTRA_DIST	=	HACKING HACKING.md libmdb.pc.in libmdbsql.pc.in README.md

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libmdb.pc
if SQL
pkgconfig_DATA += libmdbsql.pc
endif
