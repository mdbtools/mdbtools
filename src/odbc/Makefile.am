noinst_PROGRAMS	=	unittest
lib_LTLIBRARIES	=	libmdbodbc.la
AM_CPPFLAGS	=	-I$(top_srcdir)/include $(GLIB_CFLAGS) $(ODBC_CFLAGS)
libmdbodbc_la_SOURCES=	odbc.c connectparams.c mdbodbc.h connectparams.h
libmdbodbc_la_LIBADD=	../libmdb/libmdb.la ../sql/libmdbsql.la
libmdbodbc_la_LDFLAGS = -version-info 1:1:0 -export-symbols-regex '^(SQL|ODBCINST)'
LIBS	=	@LEXLIB@ $(GLIB_LIBS) $(ODBC_LIBS)
unittest_LDADD	=	libmdbodbc.la ../libmdb/libmdb.la ../sql/libmdbsql.la
