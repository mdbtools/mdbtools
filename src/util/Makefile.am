bin_PROGRAMS	=	mdb-export mdb-array mdb-schema mdb-tables mdb-parsecsv mdb-header mdb-sql mdb-ver prtable prcat prdata prkkd prdump prole updrow prindex
LIBS	=	$(GLIB_LIBS) $(READLINE_LIBS) @LEXLIB@ 
INCLUDES	=	-I$(top_srcdir)/include $(GLIB_CFLAGS)
LDADD	=	../libmdb/libmdb.la 
if SQL
mdb_sql_LDADD = ../libmdb/libmdb.la ../sql/libmdbsql.la
endif
