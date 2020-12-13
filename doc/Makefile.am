SHELL = /bin/sh
TXT2MAN = ./txt2man
PRODUCT = MDBTools

dist_man_MANS =
if ENABLE_MAN
  dist_man_MANS += mdb-tables.1 mdb-ver.1 mdb-export.1 mdb-schema.1 mdb-sql.1 \
	mdb-array.1 mdb-header.1 mdb-hexdump.1 mdb-parsecsv.1 mdb-prop.1 mdb-import.1 \
	mdb-count.1 mdb-json.1 mdb-queries.1
endif
CLEANFILES = ${dist_man_MANS}
EXTRA_DIST	= mdb-tables.txt mdb-ver.txt mdb-export.txt mdb-schema.txt mdb-sql.txt \
	mdb-array.txt mdb-header.txt mdb-hexdump.txt mdb-parsecsv.txt mdb-prop.txt mdb-import.txt \
	mdb-count.txt mdb-json.txt mdb-queries.txt \
        txt2man

.txt.1:
	$(TXT2MAN) -t $* -r "$(PRODUCT) $(VERSION)" -s 1 -v "Executable programs or shell commands" $(srcdir)/$< > $@
