# Converting DocBook to HTML (several small files)
# http://www.freebsd.org/tutorials/docproj-primer/x3132.html#AEN3140
# version: $Id: Makefile.am,v 1.3 2003/01/14 17:37:19 brianb Exp $
SHELL = /bin/sh
TXT2MAN = $(srcdir)/txt2man
RELEASE = 0.5
PRODUCT = MDBTools

man_MANS	= mdb-tables.1 mdb-ver.1 mdb-export.1 mdb-schema.1 mdb-sql.1

EXTRA_DIST	= mdb-tables.txt mdb-ver.txt mdb-export.txt mdb-schema.txt mdb-sql.txt faq.html txt2man

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

mdb-tables.1: mdb-tables.txt
	- $(TXT2MAN) -P $(PRODUCT) -t $(PRODUCT) -r $(RELEASE) $(srcdir)/$< > $@
	
mdb-ver.1: mdb-ver.txt
	- $(TXT2MAN) -P $(PRODUCT) -t $(PRODUCT) -r $(RELEASE) $(srcdir)/$< > $@

mdb-export.1: mdb-export.txt
	- $(TXT2MAN) -P $(PRODUCT) -t $(PRODUCT) -r $(RELEASE) $(srcdir)/$< > $@

mdb-schema.1: mdb-schema.txt
	- $(TXT2MAN) -P $(PRODUCT) -t $(PRODUCT) -r $(RELEASE) $(srcdir)/$< > $@

mdb-sql.1: mdb-sql.txt
	- $(TXT2MAN) -P $(PRODUCT) -t $(PRODUCT) -r $(RELEASE) $(srcdir)/$< > $@


