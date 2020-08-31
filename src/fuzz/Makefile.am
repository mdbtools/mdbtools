EXTRA_PROGRAMS = fuzz_mdb

# Force C++ linking for fuzz target
nodist_EXTRA_fuzz_mdb_SOURCES = dummy.cxx
fuzz_mdb_SOURCES = fuzz_mdb.c

AM_CFLAGS = -I$(top_srcdir)/include $(GLIB_CFLAGS) -g
AM_LDFLAGS = -static
LDADD = ../libmdb/libmdb.la @LIB_FUZZING_ENGINE@
