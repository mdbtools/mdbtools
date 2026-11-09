EXTRA_PROGRAMS = fuzz_mdb fuzz_index

# Force C++ linking for fuzz targets
nodist_EXTRA_fuzz_mdb_SOURCES = dummy.cxx
fuzz_mdb_SOURCES = fuzz_mdb.c
nodist_EXTRA_fuzz_index_SOURCES = dummy_index.cxx
fuzz_index_SOURCES = fuzz_index.c

AM_CFLAGS = -I$(top_srcdir)/include $(GLIB_CFLAGS) -g
AM_LDFLAGS = -static
LDADD = ../libmdb/libmdb.la @LIB_FUZZING_ENGINE@

