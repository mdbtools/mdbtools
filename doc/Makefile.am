SHELL = /bin/sh
TXT2MAN = ./txt2man
PRODUCT = MDBTools

dist_man_MANS =
if ENABLE_MAN
  dist_man_MANS += mdb-tables.1 mdb-ver.1 mdb-export.1 mdb-schema.1 \
	mdb-array.1 mdb-header.1 mdb-hexdump.1 mdb-parsecsv.1 mdb-prop.1 mdb-import.1 \
	mdb-count.1 mdb-json.1 mdb-queries.1
if SQL
  dist_man_MANS += mdb-sql.1
endif
endif
CLEANFILES = ${dist_man_MANS}
EXTRA_DIST	= mdb-tables.txt mdb-ver.txt mdb-export.txt mdb-schema.txt \
	mdb-array.txt mdb-header.txt mdb-hexdump.txt mdb-parsecsv.txt mdb-prop.txt mdb-import.txt \
	mdb-count.txt mdb-json.txt mdb-queries.txt \
        txt2man
if SQL
EXTRA_DIST	+= mdb-sql.txt
endif

.txt.1:
	$(TXT2MAN) -t $* -r "$(PRODUCT) $(VERSION)" -s 1 -v "Executable programs or shell commands" $(srcdir)/$< > $@
