%{
#include "mdbsql.h"

MdbSQL *g_sql;
%}

%union {
	char *name;
	double dval;
	int ival;
}

%token NAME PATH NUMBER STRING
%token SELECT FROM WHERE CONNECT DISCONNECT TO LIST TABLES WHERE AND
%token DESCRIBE TABLE
%token LTEQ GTEQ LIKE

%%

query:
	SELECT column_list FROM table where_clause {
			mdb_sql_select(g_sql);	
		}
	|	CONNECT TO database { 
			mdb_sql_open(g_sql, $3.name); free($3.name); 
		}
	|	DISCONNECT { 
			mdb_sql_close(g_sql);
		}
	|	DESCRIBE TABLE table { 
			mdb_sql_describe_table(g_sql); 
		}
	|	LIST TABLES { 
			mdb_sql_listtables(g_sql); 
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
				mdb_sql_add_sarg(g_sql, $1.name, $2.ival, $3.name);
				free($1.name);
				free($3.name);
				}
	| constant operator NAME {
				mdb_sql_add_sarg(g_sql, $3.name, $2.ival, $1.name);
				free($1.name);
				free($3.name);
				}
	;

operator:
	'='	{ $$.ival = MDB_EQUAL; }
	| '>'	{ $$.ival = MDB_GT; }
	| '<'	{ $$.ival = MDB_LT; }
	| LTEQ	{ $$.ival = MDB_LTEQ; }
	| GTEQ	{ $$.ival = MDB_GTEQ; }
	| LIKE	{ $$.ival = MDB_LIKE; }
	;
constant:
	NUMBER { $$.name = $1.name; }
	| STRING { $$.name = $1.name; }
	;

database:
	PATH
	|	NAME 

table:
	NAME { mdb_sql_add_table(g_sql, $1.name); free($1.name); }
	;

column_list:
	'*'	{ mdb_sql_all_columns(g_sql); }
	|	column  
	|	column ',' column_list 
	;
	 
column:
	NAME { mdb_sql_add_column(g_sql, $1.name); free($1.name); }
	;

%%
