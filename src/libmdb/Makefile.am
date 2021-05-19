lib_LTLIBRARIES	=	libmdb.la
libmdb_la_SOURCES=	catalog.c file.c table.c data.c dump.c backend.c money.c sargs.c index.c like.c write.c stats.c map.c props.c worktable.c options.c iconv.c version.c rc4.c
libmdb_la_LDFLAGS = -version-info $(VERSION_INFO)
if FAKE_GLIB
libmdb_la_SOURCES += fakeglib.c
libmdb_la_LDFLAGS += -export-symbols-regex '^(g_|mdb_|_mdb_put_int16$$|_mdb_put_int32$$)'
else
libmdb_la_LDFLAGS += -export-symbols-regex '^(mdb_|_mdb_put_int16$$|_mdb_put_int32$$)'
endif
AM_CFLAGS	=	-I$(top_srcdir)/include $(GLIB_CFLAGS) -Wsign-compare
if FUZZER_ENABLED
AM_CFLAGS += -fsanitize=fuzzer-no-link -fsanitize=address
libmdb_la_LDFLAGS += -fsanitize=fuzzer -fsanitize=address
endif
LIBS = $(GLIB_LIBS) @LIBS@ @LIBICONV@
