AUTOMAKE_OPTIONS = subdir-objects
noinst_PROGRAMS	=	unittest
lib_LTLIBRARIES	=	libmdbodbc.la
AM_CFLAGS	=	-I$(top_srcdir)/include $(GLIB_CFLAGS) $(ODBC_CFLAGS)
libmdbodbc_la_SOURCES	= odbc.c connectparams.c
libmdbodbc_la_LIBADD	= ../libmdb/libmdb.la ../sql/libmdbsql.la $(ODBC_LIBS)
libmdbodbc_la_LDFLAGS	= -avoid-version -export-symbols-regex '^(SQL|ODBCINST)' $(ODBC_LDFLAGS)
if FAKE_GLIB
libmdbodbc_la_SOURCES += ../libmdb/fakeglib.c
endif
if ICONV
lib_LTLIBRARIES += libmdbodbcW.la
libmdbodbcW_la_SOURCES	= $(libmdbodbc_la_SOURCES)
libmdbodbcW_la_LIBADD	= $(libmdbodbc_la_LIBADD) @LIBICONV@
libmdbodbcW_la_LDFLAGS	= $(libmdbodbc_la_LDFLAGS)
libmdbodbcW_la_CFLAGS	= $(AM_CFLAGS) -D ENABLE_ODBC_W=1
endif
LIBS	=	 $(GLIB_LIBS)
unittest_LDADD	=	libmdbodbc.la ../libmdb/libmdb.la ../sql/libmdbsql.la
