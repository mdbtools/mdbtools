%{
#include <string.h>
#include "mdbsql.h"
#include "parser.h"

extern MdbSQL *g_sql;
%}

%%
select	{ return SELECT; }
from		{ return FROM; }
connect	{ return CONNECT; }
to		{ return TO; }
list		{ return LIST; }
where		{ return WHERE; }
and		{ return AND; }
tables	{ return TABLES; }
[ \t\r]	;
[A-z][A-z0-9]*		{ yylval.name = strdup(yytext); return NAME; }
'.*'			{ yylval.name = strdup(yytext); return STRING; }
([0-9]+|([0-9]*\.[0-9+)([eE][-+]?[0-9]+)?) { 
				yylval.name = strdup(yytext); return NUMBER; 
			}
(\/?[A-z0-9\.]+)+		{ yylval.name = strdup(yytext); return PATH; }
.	{ return yytext[0]; }
%%

void yyerror(char *s)
{
	fprintf(stderr,"Error at Line : %s near %s\n", s, yytext);
}
#if 0
int main(int argc, char **argv)
{
int i;

	g_sql = mdb_sql_init();
	yyin = stdin;
	if (yyparse()) {
		fprintf(stderr, "Couldn't parse SQL\n");
		exit(1);
	}
	mdb_sql_dump(g_sql);
	mdb_sql_exit(g_sql);	
}
#endif
