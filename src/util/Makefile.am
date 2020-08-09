AUTOMAKE_OPTIONS = subdir-objects
SUBDIRS = bash-completion
bin_PROGRAMS	=	mdb-export mdb-array mdb-schema mdb-tables mdb-parsecsv mdb-header mdb-sql mdb-ver mdb-prop mdb-count
noinst_PROGRAMS = mdb-import prtable prcat prdata prkkd prdump prole updrow prindex
mdb_export_SOURCES = mdb-export.c
mdb_schema_SOURCES = mdb-schema.c
mdb_tables_SOURCES = mdb-tables.c
mdb_sql_SOURCES = mdb-sql.c
mdb_ver_SOURCES = mdb-ver.c
mdb_import_SOURCES = mdb-import.c
updrow_SOURCES = updrow.c
if FAKE_GLIB
mdb_export_SOURCES += ../libmdb/fakeglib.c
mdb_schema_SOURCES += ../libmdb/fakeglib.c
mdb_tables_SOURCES += ../libmdb/fakeglib.c
mdb_sql_SOURCES += ../libmdb/fakeglib.c
mdb_ver_SOURCES += ../libmdb/fakeglib.c
mdb_import_SOURCES += ../libmdb/fakeglib.c
updrow_SOURCES += ../libmdb/fakeglib.c
endif
LIBS	=	$(GLIB_LIBS) @LIBS@
DEFS = @DEFS@ -DLOCALEDIR=\"$(localedir)\"
AM_CFLAGS	=	-I$(top_srcdir)/include $(GLIB_CFLAGS) -Wsign-compare
LDADD	=	../libmdb/libmdb.la 
if SQL
mdb_sql_LDADD = ../libmdb/libmdb.la ../sql/libmdbsql.la $(LIBREADLINE)
endif
