bin_PROGRAMS	=	mdb-export mdb-array mdb-schema mdb-tables mdb-parsecsv mdb-header mdb-sql prtable prcat prdata prkkd prdump 
INCLUDES	=	-I$(top_srcdir)/include `glib-config --cflags`
LIBS	=	`glib-config --libs` $(READLINE_LIBS) @LEXLIB@ 
LDADD	=	../libmdb/libmdb.la 
mdb_sql_LDADD = ../libmdb/libmdb.la ../sql/libmdbsql.la
