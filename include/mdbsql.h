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
	MdbSargNode *sarg_tree;
	GList *sarg_stack;
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

extern MdbSQL *mdb_sql_init();
extern MdbSQLSarg *mdb_sql_alloc_sarg();
extern MdbSQLColumn *mdb_sql_alloc_column();
extern MdbSQLTable *mdb_sql_alloc_table();
extern MdbHandle *mdb_sql_open(MdbSQL *sql, char *db_name);
extern int mdb_sql_add_sarg(MdbSQL *sql, char *col_name, int op, char *constant);
extern void mdb_sql_all_columns(MdbSQL *sql);
extern int mdb_sql_add_column(MdbSQL *sql, char *column_name);
extern int mdb_sql_add_table(MdbSQL *sql, char *table_name);
extern void mdb_sql_dump(MdbSQL *sql);
extern void mdb_sql_exit(MdbSQL *sql);
extern void mdb_sql_reset(MdbSQL *sql);
extern void mdb_sql_listtables(MdbSQL *sql);
extern void mdb_sql_select(MdbSQL *sql);
extern void mdbsql_bind_all(MdbSQL *sql);

#endif
