SHELL = /bin/sh
TXT2MAN = txt2man
PRODUCT = MDBTools

# To make the userguide, export DOCBOOK_DSL TO point to docbook.dsl.
dist_man_MANS =
if ENABLE_MAN
  dist_man_MANS += mdb-tables.1 mdb-ver.1 mdb-export.1 mdb-schema.1 mdb-sql.1 \
	mdb-array.1 mdb-header.1 mdb-hexdump.1 mdb-parsecsv.1 mdb-prop.1 gmdb2.1
endif
if ENABLE_DOCBOOK
  dist_man_MANS += install.tgz
endif
CLEANFILES = ${dist_man_MANS} install install.tgz
EXTRA_DIST	= mdb-tables.txt mdb-ver.txt mdb-export.txt mdb-schema.txt mdb-sql.txt \
	mdb-array.txt mdb-header.txt mdb-hexdump.txt mdb-parsecsv.txt mdb-prop.txt gmdb2.txt \
	faq.html install.sgml

.txt.1:
	$(TXT2MAN) -t $* -r "$(PRODUCT) $(VERSION)" -s 1 -v "Executable programs or shell commands" $(srcdir)/$< > $@

# Converting DocBook to HTML (several small files)
# http://www.freebsd.org/tutorials/docproj-primer/x3132.html#AEN3140
install.tgz: $(srcdir)/install.sgml
	rm -rf html
	mkdir html
	cd html && \
	    openjade -d ${DOCBOOK_DSL} -t sgml ../install.sgml
	[ -f html/book1.htm ]
	ln -s book1.htm html/index.html
	[ -L install ] || ln -s html install
	tar zcf install.tgz install/*

clean-local:
	-rm -rf html
