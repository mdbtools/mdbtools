if HAVE_GTK
bin_PROGRAMS	= gmdb
include_HEADERS = gmdb.h gtkhlist.h debug.xpm forms.xpm macros.xpm pk.xpm \
	table.xpm query.xpm code.xpm reports.xpm
gmdb_SOURCES = debug.c file.c form.c gtkhlist.c info.c macro.c main.c \
	module.c query.c report.c sql.c table.c table_data.c table_def.c \
	table_export.c util.c
LIBS	=	$(GMDB_LIBS) @LEXLIB@ 
INCLUDES	=	-I$(top_srcdir)/include $(GMDB_CFLAGS)
#LDADD	=	../libmdb/libmdb.la 
LDADD = ../libmdb/libmdb.la ../sql/libmdbsql.la
#if SQL
#gmdb_LDADD = ../libmdb/libmdb.la ../sql/libmdbsql.la
#endif

endif
