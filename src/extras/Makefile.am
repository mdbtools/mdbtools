bin_PROGRAMS	=	mdb-dump
mdb_dump_SOURCES = mdb-dump.c mdbsupport.c
INCLUDES	=	-I$(top_srcdir)/include `glib-config --cflags`
LDADD	=	../libmdb/libmdb.la 
LIBS	=	`glib-config --libs`
