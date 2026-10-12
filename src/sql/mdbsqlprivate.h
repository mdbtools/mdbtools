/* MDB Tools - A library for reading MS Access database file
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

/* Declarations shared by the SQL parser and mdbsql.c; not installed */

#ifndef _mdbsqlprivate_h_
#define _mdbsqlprivate_h_

#include "mdbsql.h"

/* A literal in an SQL statement */
typedef enum {
	MDB_SQL_CONST_NULL,
	MDB_SQL_CONST_STRING,	/* text is the (unquoted) string */
	MDB_SQL_CONST_NUMBER,	/* text is the number as written */
	MDB_SQL_CONST_DATE	/* d is a JET date */
} MdbSQLConstType;

typedef struct {
	MdbSQLConstType type;
	char *text;
	double d;
} MdbSQLConst;

/* Types of the constant stored in a sarg node until the column it is
 * compared with is known; see mdb_sql_resolve_sargcol() */
#define MDB_SQL_VAL_NUMBER 0xf0	/* value.s is the number as written */
#define MDB_SQL_VAL_DATE 0xf1	/* value.d is a JET date */
#define MDB_SQL_VAL_NULL 0xf2

/* A column reference, possibly qualified: [table.]column */
typedef struct {
	char *qualifier;	/* NULL if not qualified */
	char *name;
} MdbSQLColRef;

MdbSQLColRef *mdbsql_colref_new(char *qualifier, char *name);
void mdbsql_colref_free(MdbSQLColRef *ref);
int mdbsql_add_column(MdbSQL *sql, const char *qualifier, const char *name, const char *alias);
int mdbsql_add_table(MdbSQL *sql, const char *name, const char *alias);

MdbSQLConst *mdbsql_const_new(MdbSQLConstType type, const char *text, double d);
MdbSQLConst *mdbsql_const_from_literal(const char *literal);
MdbSQLConst *mdbsql_const_date(MdbSQL *sql, const char *text);
void mdbsql_const_free(MdbSQLConst *c);
MdbSQLConst *mdbsql_strptime(MdbSQL *sql, MdbSQLConst *data, MdbSQLConst *format);
int mdbsql_add_sarg(MdbSQL *sql, const char *qualifier, const char *col_name, int op, MdbSQLConst *c);
int mdbsql_eval_expr(MdbSQL *sql, MdbSQLConst *c1, int op, MdbSQLConst *c2);

#endif
