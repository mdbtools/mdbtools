%{
/* MDB Tools - A library for reading MS Access database file
 * Copyright (C) 2000 Brian Bruns
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include <string.h>
#include "mdbsql.h"
#include "parser.h"

%}

%%
select	{ return SELECT; }
from		{ return FROM; }
connect	{ return CONNECT; }
disconnect	{ return DISCONNECT; }
to		{ return TO; }
list		{ return LIST; }
where		{ return WHERE; }
and		{ return AND; }
tables	{ return TABLES; }
table	{ return TABLE; }
describe	{ return DESCRIBE; }
(<=)		{ return LTEQ; }
(>=)		{ return GTEQ; }
like		{ return LIKE; }
[ \t\r]	;
\"[A-z][A-z0-9 ]*\"   {
				yylval.name = strdup(&yytext[1]);
				yylval.name[strlen(yylval.name)-1]='\0';
				return IDENT;
			}
[A-z][A-z0-9]*		{ yylval.name = strdup(yytext); return NAME; }

'.*'			{ yylval.name = strdup(yytext); return STRING; }
([0-9]+|([0-9]*\.[0-9+)([eE][-+]?[0-9]+)?) { 
				yylval.name = strdup(yytext); return NUMBER; 
			}
~?(\/?[A-z0-9\.]+)+		{ yylval.name = strdup(yytext); return PATH; }
.	{ return yytext[0]; }
%%

int yywrap()
{
	return 1;
}
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
