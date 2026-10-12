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

%code requires {
#include "mdbsqlprivate.h"
}

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
	MdbSQLConst *cval;
	MdbSQLColRef *colref;
}

%destructor { free($$); } <name>
%destructor { mdbsql_const_free($$); } <cval>
%destructor { mdbsql_colref_free($$); } <colref>

%start stmt

%token <name> IDENT NAME PATH STRING NUMBER DATE_LITERAL
%token OPENING CLOSING
%token SELECT FROM WHERE CONNECT DISCONNECT TO LIST TABLES AND OR NOT LIMIT COUNT STRPTIME
%token DESCRIBE TABLE TOP PERCENT AS
%token LTEQ GTEQ NEQ LIKE ILIKE IS NUL

%type <name> database
%type <cval> constant
%type <ival> operator
%type <ival> nulloperator
%type <name> identifier
%type <name> alias
%type <colref> column_ref

//
// operator precedence
//

// left associativity means that 1+2+3 translates to (1+2)+3
// the order of operators here determines their precedence

%left OR
%left AND
%right NOT
%left EQ LTEQ GTEQ NEQ LT GT LIKE ILIKE IS

%%

stmt:
	query
	| error { yyclearin; mdb_sql_reset(parser_ctx->mdb); }
	;

query:
	SELECT top_clause column_list FROM table where_clause limit_clause {
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

top_clause:
	/* empty */
	|	TOP NUMBER { mdb_sql_add_limit(parser_ctx->mdb, $2, 0); free($2); }
	|	TOP NUMBER PERCENT {
			if (mdb_sql_add_limit(parser_ctx->mdb, $2, 1)) {
				yyerror(NULL, parser_ctx, "Percent values must be between 0 and 100");
			}
			free($2);
		}
	;

where_clause:
	/* empty */
	| WHERE sarg_list
	;

limit_clause:
	/* empty */
	| LIMIT NUMBER {
			if (mdb_sql_get_limit(parser_ctx->mdb) != -1) {
				yyerror(NULL, parser_ctx, "Can not have TOP and LIMIT clauses");
			} else {
				mdb_sql_add_limit(parser_ctx->mdb, $2, 0);
			}
			free($2);
		}
	;

sarg_list:
	sarg 
	| OPENING sarg_list CLOSING
	| NOT sarg_list { mdb_sql_add_not(parser_ctx->mdb); }
	| sarg_list OR sarg_list { mdb_sql_add_or(parser_ctx->mdb); }
	| sarg_list AND sarg_list { mdb_sql_add_and(parser_ctx->mdb); }
	;

sarg:
	column_ref operator constant	{
				int rc = mdbsql_add_sarg(parser_ctx->mdb, $1->qualifier, $1->name, $2, $3);
				mdbsql_colref_free($1);
				mdbsql_const_free($3);
				if (rc) YYERROR;
				}
	| constant operator column_ref {
				int rc;
				switch($2) {
					case MDB_GT:
						$2 = MDB_LT;
						break;
					case MDB_LT:
						$2 = MDB_GT;
						break;
					case MDB_GTEQ:
						$2 = MDB_LTEQ;
						break;
					case MDB_LTEQ:
						$2 = MDB_GTEQ;
						break;
				}
				if ($2 == MDB_LIKE || $2 == MDB_ILIKE) {
					mdb_sql_error(parser_ctx->mdb, "The pattern of LIKE must be on its right");
					rc = 1;
				} else {
					rc = mdbsql_add_sarg(parser_ctx->mdb, $3->qualifier, $3->name, $2, $1);
				}
				mdbsql_const_free($1);
				mdbsql_colref_free($3);
				if (rc) YYERROR;
				}
	| constant operator constant {
				int rc = mdbsql_eval_expr(parser_ctx->mdb, $1, $2, $3);
				mdbsql_const_free($1);
				mdbsql_const_free($3);
				if (rc) YYERROR;
	}
	| column_ref nulloperator	{ 
				mdbsql_add_sarg(parser_ctx->mdb, $1->qualifier, $1->name, $2, NULL);
				mdbsql_colref_free($1);
				}
	;

identifier:
	NAME
	| IDENT
	;

column_ref:
	identifier { $$ = mdbsql_colref_new(NULL, $1); }
	| identifier '.' identifier { $$ = mdbsql_colref_new($1, $3); }
	;

operator:
	EQ	{ $$ = MDB_EQUAL; }
	| GT	{ $$ = MDB_GT; }
	| LT	{ $$ = MDB_LT; }
	| LTEQ	{ $$ = MDB_LTEQ; }
	| GTEQ	{ $$ = MDB_GTEQ; }
	| NEQ	{ $$ = MDB_NEQ; }
	| LIKE	{ $$ = MDB_LIKE; }
	| ILIKE	{ $$ = MDB_ILIKE; }
	;

nulloperator:
	IS NUL	{ $$ = MDB_ISNULL; }
	| IS NOT NUL	{ $$ = MDB_NOTNULL; }
	;

constant:
	STRPTIME constant ',' constant CLOSING {
		$$ = mdbsql_strptime(parser_ctx->mdb, $2, $4);
		mdbsql_const_free($2);
		mdbsql_const_free($4);
		if (!$$) YYERROR;
	}
	| NUMBER { $$ = mdbsql_const_new(MDB_SQL_CONST_NUMBER, $1, 0); free($1); }
	| STRING { $$ = mdbsql_const_from_literal($1); free($1); }
	| DATE_LITERAL {
		$$ = mdbsql_const_date(parser_ctx->mdb, $1);
		free($1);
		if (!$$) YYERROR;
	}
	;

database:
	PATH
	|	NAME
	|	IDENT
	;

table:
	identifier alias {
			mdbsql_add_table(parser_ctx->mdb, $1, $2);
			free($1);
			free($2);
		}
	;

alias:
	/* empty */ { $$ = NULL; }
	| identifier
	| AS identifier { $$ = $2; }
	;

column_list:
	COUNT '*' CLOSING	{ mdb_sql_sel_count(parser_ctx->mdb); }
	| '*'	{ mdb_sql_all_columns(parser_ctx->mdb); }
	|	column  
	|	column ',' column_list 
	;
	 

column:
	column_ref alias {
			mdbsql_add_column(parser_ctx->mdb, $1->qualifier, $1->name, $2);
			mdbsql_colref_free($1);
			free($2);
		}
	| identifier '.' '*' {
			mdbsql_add_column(parser_ctx->mdb, $1, "*", NULL);
			free($1);
		}
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
	mdb_sql_error(sql_ctx->mdb, "%s", msg);
}
