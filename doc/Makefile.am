# Converting DocBook to HTML (several small files)
# http://www.freebsd.org/tutorials/docproj-primer/x3132.html#AEN3140
# version: $Id: Makefile.am,v 1.1 2003/01/05 15:00:23 brianb Exp $
SHELL = /bin/sh
TXT2MAN = $(srcdir)/txt2man
RELEASE = 0.5
PRODUCT = MDBTools

man_MANS	= mdb-tables.1 mdb-ver.1 mdb-export.1 mdb-schema.1 mdb-sql.1

EXTRA_DIST	= mdb-tables.txt mdb-ver.txt mdb-export.txt mdb-schema.txt mdb-sql.txt

#html::	userguide.tgz

#dist:	userguide.tgz man 
dist:	man 

# To make the userguide, export DOCBOOK_DSL TO point to docbook.dsl.

#userguide.tgz: $(srcdir)/userguide.sgml
#	if test -n "${DOCBOOK_DSL}"; then \
#	        rm -rf html && \
#	        mkdir html && \
#	        cd html && pwd && \
#	        openjade -d ${DOCBOOK_DSL} -t sgml ../$(srcdir)/userguide.sgml; \
#	        test -f book1.htm && \
#	        ln -s book1.htm index.html && cd .. && \
#	        if ! [ -L userguide ]; then \
#	                ln -s html userguide; \
#	        fi; \
#	        tar zcf userguide.tgz userguide/* \
#	; fi

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


