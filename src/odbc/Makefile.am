include_HEADERS = connectparams.h
SQLDIR         =    ../sql
SQLSOURCES     =    mdbsql.c parser.c lexer.c
MDBDIR         =    ../libmdb
MDBSOURCES     =    backend.c index.c money.c catalog.c kkd.c sargs.c \
				data.c like.c table.c dump.c file.c mem.c
bin_PROGRAMS	=	unittest
lib_LTLIBRARIES	=	libmdbodbc.la
INCLUDES	=	-I ../../include $(GLIB_CFLAGS)
libmdbodbc_la_SOURCES=	odbc.c connectparams.c
libmdbodbc_la_LIBADD=	$(patsubst %, $(SQLDIR)/%, \
				$(patsubst %.c, %.lo, $(SQLSOURCES))) \
				$(patsubst %, $(MDBDIR)/%, \
				$(patsubst %.c, %.lo, $(MDBSOURCES)))
LIBS	=	@LEXLIB@ $(GLIB_LIBS)
unittest_LDADD	=	libmdbodbc.la ../libmdb/libmdb.la ../sql/libmdbsql.la

## Need blank statement to avoid compiling odbc.c
odbc: $(EXTRA_LTLIBRARIES)
	@echo ''
