bin_PROGRAMS	=	mdb-export mdb-array mdb-schema mdb-tables mdb-parsecsv mdb-header mdb-sql mdb-ver prtable prcat prdata prkkd prdump msysobjects 
LIBS	=	`glib-config --libs` $(READLINE_LIBS) @LEXLIB@ 
INCLUDES	=	-I$(top_srcdir)/include `glib-config --cflags`
LDADD	=	../libmdb/libmdb.la 
mdb_sql_LDADD = ../libmdb/libmdb.la ../sql/libmdbsql.la
