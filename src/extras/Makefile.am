bin_PROGRAMS	=	mdb-hexdump
mdb_hexdump_SOURCES = mdb-dump.c mdbsupport.c
noinst_HEADERS = mdbsupport.h
AM_CFLAGS	=	-I$(top_srcdir)/include $(GLIB_CFLAGS)
LDADD	=	../libmdb/libmdb.la 
LIBS	=	$(GLIB_LIBS)
