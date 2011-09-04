# Converting DocBook to HTML (several small files)
# http://www.freebsd.org/tutorials/docproj-primer/x3132.html#AEN3140
SHELL = /bin/sh
TXT2MAN = txt2man
PRODUCT = MDBTools

dist_man_MANS	= mdb-tables.1 mdb-ver.1 mdb-export.1 mdb-schema.1 mdb-sql.1 \
	mdb-array.1 mdb-header.1 mdb-hexdump.1 mdb-parsecsv.1 mdb-prop.1 gmdb2.1
DISTCLEANFILES = ${dist_man_MANS}
EXTRA_DIST	= mdb-tables.txt mdb-ver.txt mdb-export.txt mdb-schema.txt mdb-sql.txt faq.html txt2man install.sgml

html:	install.tgz

dist:	install.tgz man 
#dist:	man 

# To make the userguide, export DOCBOOK_DSL TO point to docbook.dsl.

install.tgz: $(srcdir)/install.sgml
	if test -n "${DOCBOOK_DSL}"; then \
	        rm -rf html && \
	        mkdir html && \
	        cd html && pwd && \
	        openjade -d ${DOCBOOK_DSL} -t sgml ../$(srcdir)/install.sgml; \
	        test -f book1.htm && \
	        ln -s book1.htm index.html && cd .. && \
	        if ! [ -L userguide ]; then \
	                ln -s html install; \
	        fi; \
	        tar zcf install.tgz install/* \
	; fi

man:	mdb-tables.1 mdb-ver.1 mdb-export.1 mdb-sql.1

.txt.1:
	- $(TXT2MAN) -P $(PRODUCT) -t $(PRODUCT) -r $(VERSION) -s 1 $(srcdir)/$< > $@
