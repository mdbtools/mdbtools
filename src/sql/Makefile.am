lib_LTLIBRARIES	=	libmdbsql.la
libmdbsql_la_SOURCES=	mdbsql.c parser.y lexer.l 
INCLUDES	=	-I$(top_srcdir)/include $(GLIB_CFLAGS)
LIBS	=	$(GLIB_LIBS)
LDADD	=	../libmdb/libmdb.la 
YACC = bison -y -d

dist-hook:
	rm -f $(distdir)/parser.c $(distdir)/parser.h $(distdir)/lexer.c
