lib_LTLIBRARIES	=	libmdbsql.la
libmdbsql_la_SOURCES=	mdbsql.c parser.y lexer.l 
INCLUDES	=	-I$(top_srcdir)/include `glib-config --cflags`
LIBS	=	`glib-config --libs`
LDADD	=	../libmdb/libmdb.la 
#YACC = yacc -d

dist-hook:
	rm -f $(distdir)/parser.c $(distdir)/parser.h $(distdir)/lexer.c
