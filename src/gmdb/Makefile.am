bin_PROGRAMS	= gmdb
gmdb_SOURCES = debug.c file.c form.c gtkhlist.c info.c macro.c main.c \
	module.c query.c report.c sql.c table.c table_data.c table_def.c \
	table_export.c util.c
LIBS	=	$(GTK_LIBS) @LEXLIB@ 
INCLUDES	=	-I$(top_srcdir)/include $(GTK_CFLAGS)
LDADD	=	../libmdb/libmdb.la 
if SQL
gmdb_LDADD = ../libmdb/libmdb.la ../sql/libmdbsql.la
endif
