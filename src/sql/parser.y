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

%{
#include "mdbsql.h"

struct sql_context;
#include "parser.h"

typedef void *yyscan_t;
typedef struct yy_buffer_state* YY_BUFFER_STATE;
extern int yylex_init(yyscan_t* scanner);
extern int yylex_destroy(yyscan_t scanner);
extern int yylex(YYSTYPE* yylval_param, YYLTYPE* yyloc, yyscan_t yyscanner);
extern YY_BUFFER_STATE yy_scan_string(const char* buffer, yyscan_t scanner);

/** error handler for bison */
void yyerror(YYLTYPE* yyloc, struct sql_context* sql_ctx, char const* msg);

typedef struct sql_context
{
  // lexer context
  yyscan_t flex_scanner;

  MdbSQL *mdb;
} sql_context;

#define scanner parser_ctx->flex_scanner

// we want verbose error messages
#define YYERROR_VERBOSE 1
%}

// make the parser reentrant
%locations
%define api.pure
%lex-param {void * scanner}
%parse-param {struct sql_context* parser_ctx}

%union {
	char *name;
	double dval;
	int ival;
}

%start stmt

%token <name> IDENT NAME PATH STRING NUMBER
%token SELECT FROM WHERE CONNECT DISCONNECT TO LIST TABLES AND OR NOT LIMIT COUNT STRPTIME
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
	| error { yyclearin; mdb_sql_reset(parser_ctx->mdb); }
	;

query:
	SELECT column_list FROM table where_clause limit_clause {
	                mdb_sql_select(parser_ctx->mdb);
		}
	|	CONNECT TO database { 
	                mdb_sql_open(parser_ctx->mdb, $3); free($3);
		}
	|	DISCONNECT { 
	                mdb_sql_close(parser_ctx->mdb);
		}
	|	DESCRIBE TABLE table { 
	                mdb_sql_describe_table(parser_ctx->mdb);
		}
	|	LIST TABLES { 
	                mdb_sql_listtables(parser_ctx->mdb);
		}
	;

where_clause:
	/* empty */
	| WHERE sarg_list
	;

limit_clause:
	/* empty */
	| LIMIT NUMBER { mdb_sql_add_limit(parser_ctx->mdb, $2); free($2); }
	;

sarg_list:
	sarg 
	| '(' sarg_list ')'
	| NOT sarg_list { mdb_sql_add_not(parser_ctx->mdb); }
	| sarg_list OR sarg_list { mdb_sql_add_or(parser_ctx->mdb); }
	| sarg_list AND sarg_list { mdb_sql_add_and(parser_ctx->mdb); }
	;

sarg:
	identifier operator constant	{ 
	                        mdb_sql_add_sarg(parser_ctx->mdb, $1, $2, $3);
				free($1);
				free($3);
				}
	| constant operator identifier {
	                        mdb_sql_add_sarg(parser_ctx->mdb, $3, $2, $1);
				free($1);
				free($3);
				}
	| constant operator constant {
	                        mdb_sql_eval_expr(parser_ctx->mdb, $1, $2, $3);
				free($1);
				free($3);
	}
	| identifier nulloperator	{ 
	                        mdb_sql_add_sarg(parser_ctx->mdb, $1, $2, NULL);
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
	STRPTIME '(' constant ',' constant ')' { 
	        $$ = mdb_sql_strptime(parser_ctx->mdb, $3, $5);
		free($3);
		free($5);
	}
	| NUMBER { $$ = $1; }
	| STRING { $$ = $1; }
	;

database:
	PATH
	|	NAME 
	;

table:
	identifier { mdb_sql_add_table(parser_ctx->mdb, $1); free($1); }
	;

column_list:
	COUNT '(' '*' ')'	{ mdb_sql_sel_count(parser_ctx->mdb); }
	| '*'	{ mdb_sql_all_columns(parser_ctx->mdb); }
	|	column  
	|	column ',' column_list 
	;
	 

column:
	identifier { mdb_sql_add_column(parser_ctx->mdb, $1); free($1); }
	;

%%


int parse_sql( MdbSQL * mdb, const gchar *str)
{
  sql_context ctx;
  ctx.mdb = mdb;

  yylex_init(&ctx.flex_scanner);
  yy_scan_string(str, ctx.flex_scanner);
  int res = yyparse(&ctx);
  yylex_destroy(ctx.flex_scanner);

  return res;
}

void yyerror(YYLTYPE* yyloc,sql_context* sql_ctx, char const * msg)
{
	fprintf(stderr,"Error at Line : %s\n", msg);
	mdb_sql_error(sql_ctx->mdb, "%s", msg);
}
