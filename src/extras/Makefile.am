bin_PROGRAMS	=	mdb-dump
mdb_dump_SOURCES = mdb-dump.c mdbsupport.c
EXTRA_DIST = mdbsupport.h
AM_CPPFLAGS	=	-I$(top_srcdir)/include $(GLIB_CFLAGS)
LDADD	=	../libmdb/libmdb.la 
LIBS	=	$(GLIB_LIBS)
