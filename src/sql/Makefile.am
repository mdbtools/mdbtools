bin_PROGRAMS	=	mdb-sql
INCLUDES	=	-I$(top_srcdir)/include `glib-config --cflags`
LDADD	=	../libmdb/libmdb.la 
LIBS	=	`glib-config --libs` $(READLINE_LIBS) @LEXLIB@
mdb_sql_SOURCES	= mdbsql.c main.c parser.y lexer.l 
YACC = yacc -d

dist-hook:
	rm -f $(distdir)/parser.c $(distdir)/parser.h $(distdir)/lexer.c
