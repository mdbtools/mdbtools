%{
/* MDB Tools - A library for reading MS Access database files
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
#include "mdbsql.h"

int yylex(void);
int yyerror(char *);

MdbSQL *_mdb_sql(MdbSQL *sql)
{
static MdbSQL *g_sql;

	if (sql) {
		g_sql = sql;
	}
	return g_sql;
}

%}

%union {
	char *name;
	double dval;
	int ival;
}



%token <name> IDENT NAME PATH STRING NUMBER 
%token SELECT FROM WHERE CONNECT DISCONNECT TO LIST TABLES AND OR NOT LIMIT
%token DESCRIBE TABLE
%token LTEQ GTEQ LIKE IS NUL

%type <name> database
%type <name> constant
%type <ival> operator
%type <ival> nulloperator
%type <name> identifier

%%

stmt:
	query
	| error { yyclearin; mdb_sql_reset(_mdb_sql(NULL)); }
	;

query:
	SELECT column_list FROM table where_clause limit_clause {
			mdb_sql_select(_mdb_sql(NULL));	
		}
	|	CONNECT TO database { 
			mdb_sql_open(_mdb_sql(NULL), $3); free($3); 
		}
	|	DISCONNECT { 
			mdb_sql_close(_mdb_sql(NULL));
		}
	|	DESCRIBE TABLE table { 
			mdb_sql_describe_table(_mdb_sql(NULL)); 
		}
	|	LIST TABLES { 
			mdb_sql_listtables(_mdb_sql(NULL)); 
		}
	;

where_clause:
	/* empty */
	| WHERE sarg_list
	;

limit_clause:
	/* empty */
	| LIMIT NUMBER { mdb_sql_add_limit(_mdb_sql(NULL), $2); free($2); }
	;

sarg_list:
	sarg 
	| '(' sarg_list ')'
	| NOT sarg_list { mdb_sql_add_not(_mdb_sql(NULL)); }
	| sarg_list OR sarg_list { mdb_sql_add_or(_mdb_sql(NULL)); }
	| sarg_list AND sarg_list { mdb_sql_add_and(_mdb_sql(NULL)); }
	;

sarg:
	identifier operator constant	{ 
				mdb_sql_add_sarg(_mdb_sql(NULL), $1, $2, $3);
				free($1);
				free($3);
				}
	| constant operator identifier {
				mdb_sql_add_sarg(_mdb_sql(NULL), $3, $2, $1);
				free($1);
				free($3);
				}
	| constant operator constant {
				mdb_sql_eval_expr(_mdb_sql(NULL), $1, $2, $3);
				free($1);
				free($3);
	}
	| identifier nulloperator	{ 
				mdb_sql_add_sarg(_mdb_sql(NULL), $1, $2, NULL);
				free($1);
				}
	;

identifier:
	NAME
	| IDENT
	;

operator:
	'='	{ $$ = MDB_EQUAL; }
	| '>'	{ $$ = MDB_GT; }
	| '<'	{ $$ = MDB_LT; }
	| LTEQ	{ $$ = MDB_LTEQ; }
	| GTEQ	{ $$ = MDB_GTEQ; }
	| LIKE	{ $$ = MDB_LIKE; }
	;

nulloperator:
	IS NUL	{ $$ = MDB_ISNULL; }
	| IS NOT NUL	{ $$ = MDB_NOTNULL; }
	;

constant:
	NUMBER { $$ = $1; }
	| STRING { $$ = $1; }
	;

database:
	PATH
	|	NAME 
	;

table:
	identifier { mdb_sql_add_table(_mdb_sql(NULL), $1); free($1); }
	;

column_list:
	'*'	{ mdb_sql_all_columns(_mdb_sql(NULL)); }
	|	column  
	|	column ',' column_list 
	;
	 
column:
	identifier { mdb_sql_add_column(_mdb_sql(NULL), $1); free($1); }
	;

%%
