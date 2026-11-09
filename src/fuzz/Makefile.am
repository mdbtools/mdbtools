EXTRA_PROGRAMS = fuzz_mdb fuzz_sql

# Force C++ linking for fuzz targets
nodist_EXTRA_fuzz_mdb_SOURCES = dummy.cxx
fuzz_mdb_SOURCES = fuzz_mdb.c
nodist_EXTRA_fuzz_sql_SOURCES = dummy_sql.cxx
fuzz_sql_SOURCES = fuzz_sql.c

AM_CFLAGS = -I$(top_srcdir)/include $(GLIB_CFLAGS) -g
AM_LDFLAGS = -static
LDADD = ../libmdb/libmdb.la @LIB_FUZZING_ENGINE@
fuzz_sql_LDADD = ../sql/libmdbsql.la ../libmdb/libmdb.la @LIB_FUZZING_ENGINE@

