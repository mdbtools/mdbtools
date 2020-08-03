AUTOMAKE_OPTIONS = subdir-objects
SUBDIRS = bash-completion
bin_PROGRAMS	=	mdb-export mdb-array mdb-schema mdb-tables mdb-parsecsv mdb-header mdb-sql mdb-ver mdb-prop mdb-count
noinst_PROGRAMS = mdb-import prtable prcat prdata prkkd prdump prole updrow prindex
if !HAVE_GLIB
mdb_export_SOURCES = mdb-export.c ../libmdb/fakeglib.c
mdb_schema_SOURCES = mdb-schema.c ../libmdb/fakeglib.c
mdb_tables_SOURCES = mdb-tables.c ../libmdb/fakeglib.c
mdb_sql_SOURCES = mdb-sql.c ../libmdb/fakeglib.c
mdb_ver_SOURCES = mdb-ver.c ../libmdb/fakeglib.c
mdb_import_SOURCES = mdb-import.c ../libmdb/fakeglib.c
endif
LIBS	=	$(GLIB_LIBS) @LIBS@
DEFS = @DEFS@ -DLOCALEDIR=\"$(localedir)\"
AM_CFLAGS	=	-I$(top_srcdir)/include $(GLIB_CFLAGS)
LDADD	=	../libmdb/libmdb.la 
if SQL
mdb_sql_LDADD = ../libmdb/libmdb.la ../sql/libmdbsql.la $(LIBREADLINE)
endif
