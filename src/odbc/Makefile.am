bin_PROGRAMS	=	unittest
lib_LTLIBRARIES	=	libmdbodbc.la
libmdbodbc_la_SOURCES=	odbc.c connectparams.c
#libmdbodbc_la_LIBADD=	../libmdb/libmdb.la ../sql/libmdbsql.la
INCLUDES	=	-I ../../include `glib-config --cflags`
LIBS	=	@LEXLIB@
unittest_LDADD	=	libmdbodbc.la ../libmdb/libmdb.la ../sql/libmdbsql.la

## Need blank statement to avoid compiling odbc.c
odbc: $(EXTRA_LTLIBRARIES)
	@echo ''

