SUBDIRS = help gladefiles pixmaps
bin_PROGRAMS	= gmdb2 
include_HEADERS = gmdb.h
gmdb2_SOURCES = main2.c file.c util.c table.c query.c module.c macro.c report.c form.c info.c table_def.c table_data.c table_export.c debug.c sql.c schema.c prefs.c
LIBS =	-rdynamic $(GNOME_LIBS) @LEXLIB@ 
INCLUDES	=	-I$(top_srcdir)/include \
	$(GNOME_CFLAGS) \
        -DDATADIR=\""$(datadir)"\"                      \
        -DGMDB_ICONDIR=\""$(datadir)/gmdb/glade/"\"        \
        -DGMDB_GLADEDIR=\""$(datadir)/gmdb/glade/"\" 

#LDADD	=	../libmdb/libmdb.la 
LDADD = ../libmdb/libmdb.la ../sql/libmdbsql.la
#if SQL
#gmdb_LDADD = ../libmdb/libmdb.la ../sql/libmdbsql.la
#endif
