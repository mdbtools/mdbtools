#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <mdbtools.h>

#ifndef _mdbsql_h_
#define _mdbsql_h_

typedef struct {
	MdbHandle *mdb;
	int all_columns;
	int num_columns;
	GPtrArray *columns;
	int num_tables;
	GPtrArray *tables;
	int num_sargs;
	GPtrArray *sargs;
	MdbTableDef *cur_table;
	/* FIX ME */
	char *bound_values[256];
} MdbSQL;

typedef struct {
	char *name;
	int  disp_size;
	void *bind_addr;   /* if !NULL then cp parameter to here */
	int  bind_type;
	int  *bind_len;
	int  bind_max;
} MdbSQLColumn;

typedef struct {
	char *name;
	char *alias;
} MdbSQLTable;

typedef struct {
	char *col_name;
	MdbSarg *sarg;
} MdbSQLSarg;

char *g_input_ptr;

#undef YY_INPUT
#define YY_INPUT(b, r, ms) (r = mdb_sql_yyinput(b, ms));

MdbSQL *mdb_sql_init();
MdbSQLSarg *mdb_sql_alloc_sarg();
MdbSQLColumn *mdb_sql_alloc_column();
MdbSQLTable *mdb_sql_alloc_table();
MdbHandle *mdb_sql_open(MdbSQL *sql, char *db_name);
int mdb_sql_add_sarg(MdbSQL *sql, char *col_name, int op, char *constant);
int mdb_sql_all_columns(MdbSQL *sql);
int mdb_sql_add_column(MdbSQL *sql, char *column_name);
int mdb_sql_add_table(MdbSQL *sql, char *table_name);
void mdb_sql_dump(MdbSQL *sql);
void mdb_sql_exit(MdbSQL *sql);
void mdb_sql_reset(MdbSQL *sql);
void mdb_sql_listtables(MdbSQL *sql);
void mdb_sql_select(MdbSQL *sql);

#endif
