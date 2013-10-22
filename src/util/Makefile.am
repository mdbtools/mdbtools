bin_PROGRAMS	=	mdb-export mdb-array mdb-schema mdb-tables mdb-parsecsv mdb-header mdb-sql mdb-ver mdb-prop 
noinst_PROGRAMS = mdb-import prtable prcat prdata prkkd prdump prole updrow prindex
LIBS	=	$(GLIB_LIBS) @LIBS@ @LEXLIB@ 
DEFS = @DEFS@ -DLOCALEDIR=\"$(localedir)\"
AM_CFLAGS	=	-I$(top_srcdir)/include $(GLIB_CFLAGS)
LDADD	=	../libmdb/libmdb.la 
if SQL
mdb_sql_LDADD = ../libmdb/libmdb.la ../sql/libmdbsql.la $(LIBREADLINE)
endif
EXTRA_DIST = mdbtools.bash-completion
