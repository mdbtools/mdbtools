include_HEADERS = connectparams.h
SQLDIR         =    ../sql
SQLSOURCES     =    mdbsql.c parser.c lexer.c
MDBDIR         =    ../libmdb
MDBSOURCES     =    backend.c index.c money.c catalog.c sargs.c \
				data.c like.c table.c dump.c file.c mem.c \
				map.c props.c worktable.c options.c \
				write.c stats.c iconv.c

noinst_PROGRAMS	=	unittest 
lib_LTLIBRARIES	=	libmdbodbc.la
AM_CPPFLAGS	=	-I$(top_srcdir)/include $(GLIB_CFLAGS) $(ODBC_CFLAGS)
libmdbodbc_la_SOURCES=	odbc.c connectparams.c
libmdbodbc_la_LIBADD=	$(patsubst %, $(SQLDIR)/%, \
				$(patsubst %.c, %.lo, $(SQLSOURCES))) \
				$(patsubst %, $(MDBDIR)/%, \
				$(patsubst %.c, %.lo, $(MDBSOURCES))) \
				../libmdb/libmdb.la
LIBS	=	@LEXLIB@ $(GLIB_LIBS) $(ODBC_LIBS)
unittest_LDADD	=	libmdbodbc.la ../libmdb/libmdb.la ../sql/libmdbsql.la

## Need blank statement to avoid compiling odbc.c
odbc: $(EXTRA_LTLIBRARIES)
	@echo ''
