EXTRA_PROGRAMS = fuzz_mdb fuzz_write

# Force C++ linking for fuzz targets
nodist_EXTRA_fuzz_mdb_SOURCES = dummy.cxx
fuzz_mdb_SOURCES = fuzz_mdb.c
nodist_EXTRA_fuzz_write_SOURCES = dummy_write.cxx
fuzz_write_SOURCES = fuzz_write.c

AM_CFLAGS = -I$(top_srcdir)/include $(GLIB_CFLAGS) -g
AM_LDFLAGS = -static
LDADD = ../libmdb/libmdb.la @LIB_FUZZING_ENGINE@

