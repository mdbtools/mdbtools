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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include "mdbsql.h"


MdbSQL *_mdb_sql(MdbSQL *sql)
{
static MdbSQL *g_sql;

	if (sql) {
		g_sql = sql;
	} else {
		return g_sql;
	}
}

%}

%union {
	char *name;
	double dval;
	int ival;
}



%token <name> IDENT NAME PATH STRING NUMBER 
%token SELECT FROM WHERE CONNECT DISCONNECT TO LIST TABLES WHERE AND
%token DESCRIBE TABLE
%token LTEQ GTEQ LIKE

%type <name> database
%type <name> constant
%type <ival> operator

%%

query:
	SELECT column_list FROM table where_clause {
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

sarg_list:
	sarg
	| sarg AND sarg_list
	;

sarg:
	NAME operator constant	{ 
				mdb_sql_add_sarg(_mdb_sql(NULL), $1, $2, $3);
				free($1);
				free($3);
				}
	| constant operator NAME {
				mdb_sql_add_sarg(_mdb_sql(NULL), $3, $2, $1);
				free($1);
				free($3);
				}
	;

operator:
	'='	{ $$ = MDB_EQUAL; }
	| '>'	{ $$ = MDB_GT; }
	| '<'	{ $$ = MDB_LT; }
	| LTEQ	{ $$ = MDB_LTEQ; }
	| GTEQ	{ $$ = MDB_GTEQ; }
	| LIKE	{ $$ = MDB_LIKE; }
	;
constant:
	NUMBER { $$ = $1; }
	| STRING { $$ = $1; }
	;

database:
	PATH
	|	NAME 

table:
	NAME { mdb_sql_add_table(_mdb_sql(NULL), $1); free($1); }
	| IDENT { mdb_sql_add_table(_mdb_sql(NULL), $1); free($1); }
	;

column_list:
	'*'	{ mdb_sql_all_columns(_mdb_sql(NULL)); }
	|	column  
	|	column ',' column_list 
	;
	 
column:
	NAME { mdb_sql_add_column(_mdb_sql(NULL), $1); free($1); }
	| IDENT { mdb_sql_add_column(_mdb_sql(NULL), $1); free($1); }
	;

%%
