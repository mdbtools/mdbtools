AUTOMAKE_OPTIONS = subdir-objects
noinst_PROGRAMS	=	unittest
noinst_HEADERS = connectparams.h mdbodbc.h
lib_LTLIBRARIES	=	libmdbodbc.la
libdir=@libdir@/odbc
AM_CFLAGS	=	-I$(top_srcdir)/include $(GLIB_CFLAGS) $(ODBC_CFLAGS)
libmdbodbc_la_SOURCES	= odbc.c connectparams.c
if UNIXODBC
libmdbodbc_la_SOURCES	+= getproperties.c
endif
libmdbodbc_la_LIBADD	= ../libmdb/libmdb.la ../sql/libmdbsql.la $(ODBC_LIBS)
libmdbodbc_la_LDFLAGS	= -avoid-version -export-symbols-regex '^(SQL|ODBCINST)' $(ODBC_LDFLAGS)
lib_LTLIBRARIES += libmdbodbcW.la
libmdbodbcW_la_SOURCES	= $(libmdbodbc_la_SOURCES) odbcw.c
libmdbodbcW_la_LIBADD	= $(libmdbodbc_la_LIBADD)
libmdbodbcW_la_LDFLAGS	= $(libmdbodbc_la_LDFLAGS)
LIBS	=	 $(GLIB_LIBS)
unittest_LDADD	=	libmdbodbc.la ../libmdb/libmdb.la ../sql/libmdbsql.la
