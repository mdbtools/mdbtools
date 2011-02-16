lib_LTLIBRARIES	=	libmdb.la
libmdb_la_SOURCES=	catalog.c mem.c file.c table.c data.c dump.c backend.c money.c sargs.c index.c like.c write.c stats.c map.c props.c worktable.c options.c iconv.c
libmdb_la_LDFLAGS = -version-info  1:0:0
AM_CPPFLAGS	=	-I$(top_srcdir)/include $(GLIB_CFLAGS)
LIBS = $(GLIB_LIBS) @LIBS@
