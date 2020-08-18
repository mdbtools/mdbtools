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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

%option noyywrap
%option case-insensitive
%option never-interactive
%option nounput
%option noinput
%option yylineno
%option reentrant
%option bison-bridge
%option bison-locations

 // ensure that lexer will be 8-bit (and not just 7-bit)
%option 8bit

%{
#include <string.h>
#include "mdbsql.h"
struct sql_context;
#include "parser.h"

#define YY_NEVER_INTERACTIVE 1

#ifndef YY_NO_UNPUT
#define YY_NO_UNPUT	// unused
#endif

#ifdef _MSC_VER
#define YY_NO_UNISTD_H
#endif

%}



%%
select	{ return SELECT; }
from		{ return FROM; }
connect	{ return CONNECT; }
disconnect	{ return DISCONNECT; }
to		{ return TO; }
list		{ return LIST; }
where		{ return WHERE; }
tables	{ return TABLES; }
table	{ return TABLE; }
describe	{ return DESCRIBE; }
and		{ return AND; }
or		{ return OR; }
not		{ return NOT; }
is		{ return IS; }
null		{ return NUL; }
(<=)		{ return LTEQ; }
(>=)		{ return GTEQ; }
like		{ return LIKE; }
limit	{ return LIMIT; }
count		{ return COUNT; }
strptime	{ return STRPTIME; }
[ \t\r]	;

\"[^"]*\"\"  {
		yyless(yyleng-1);
		yymore();
	}

\"[^"]*\"  {
		int ip, op, ilen;
		ilen = strlen(yytext);
		yylval->name = malloc(ilen-1);
		for (ip=1, op=0; ip<ilen-1; ip++, op++) {
			if (yytext[ip] != '"') {
			        yylval->name[op] = yytext[ip];
			} else if (yytext[ip+1] == '"') {
			        yylval->name[op] = yytext[ip++];
			}
		}
		yylval->name[op]='\0';
		return IDENT;
	}

[a-z\xa0-\xff][a-z0-9_#@\xa0-\xff]*		{ yylval->name = g_strdup(yytext); return NAME; }

'[^']*''  {
		yyless(yyleng-1);
		yymore();
	}
'[^']*'  {
		yylval->name = g_strdup(yytext);
		return STRING;
	}

(-*[0-9]+|([0-9]*\.[0-9]+)(e[-+]?[0-9]+)?) {
		yylval->name = g_strdup(yytext); return NUMBER;
	}
~?(\/?[a-z0-9\.\xa0-\xff]+)+ {
		yylval->name = g_strdup(yytext); return PATH;
	}

.	{ return yytext[0]; }
%%

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
