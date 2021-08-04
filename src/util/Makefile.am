AUTOMAKE_OPTIONS = subdir-objects
SUBDIRS = bash-completion
bin_PROGRAMS	=	mdb-export mdb-array mdb-schema mdb-tables mdb-parsecsv mdb-header mdb-ver mdb-prop mdb-count mdb-queries mdb-json
noinst_PROGRAMS = mdb-import prtable prcat prdata prkkd prdump prole updrow prindex
noinst_HEADERS = base64.h
LIBS	=	$(GLIB_LIBS) @LIBS@
DEFS = @DEFS@ -DLOCALEDIR=\"$(localedir)\"
AM_CFLAGS	=	-I$(top_srcdir)/include $(GLIB_CFLAGS) -Wsign-compare
LDADD	=	../libmdb/libmdb.la 
if SQL
bin_PROGRAMS += mdb-sql
mdb_sql_LDADD = ../libmdb/libmdb.la ../sql/libmdbsql.la $(LIBREADLINE)
endif
