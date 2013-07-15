bin_PROGRAMS	=	mdb-hexdump
mdb_hexdump_SOURCES = mdb-dump.c mdbsupport.c
EXTRA_DIST = mdbsupport.h
AM_CFLAGS	=	-I$(top_srcdir)/include $(GLIB_CFLAGS)
LDADD	=	../libmdb/libmdb.la 
LIBS	=	$(GLIB_LIBS)
