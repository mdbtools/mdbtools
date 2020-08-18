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

#ifndef _mdbsql_h_
#define _mdbsql_h_

#ifdef __cplusplus
  extern "C" {
#endif

#include <stdio.h>
#include <string.h>
#ifdef HAVE_GLIB
#include <glib.h>
#else
#include <mdbfakeglib.h>
#endif
#include <mdbtools.h>

typedef struct MdbSQL
{
	MdbHandle *mdb;
	int all_columns;
	int sel_count;
	unsigned int num_columns;
	GPtrArray *columns;
	unsigned int num_tables;
	GPtrArray *tables;
	MdbTableDef *cur_table;
	MdbSargNode *sarg_tree;
	GList *sarg_stack;
	/* FIX ME */
	void *bound_values[256];
	unsigned char *kludge_ttable_pg;
	long max_rows;
	char error_msg[1024];
	int limit;
	long row_count;
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

#define mdb_sql_has_error(sql) ((sql)->error_msg[0] ? 1 : 0)
#define mdb_sql_last_error(sql) ((sql)->error_msg)

void mdb_sql_error(MdbSQL* sql, char *fmt, ...);
MdbSQL *mdb_sql_init(void);
MdbSQLSarg *mdb_sql_alloc_sarg(void);
MdbHandle *mdb_sql_open(MdbSQL *sql, char *db_name);
int mdb_sql_add_sarg(MdbSQL *sql, char *col_name, int op, char *constant);
void mdb_sql_all_columns(MdbSQL *sql);
void mdb_sql_sel_count(MdbSQL *sql);
int mdb_sql_add_column(MdbSQL *sql, char *column_name);
int mdb_sql_add_table(MdbSQL *sql, char *table_name);
char *mdb_sql_strptime(MdbSQL *sql, char *data, char *format);
void mdb_sql_dump(MdbSQL *sql);
void mdb_sql_exit(MdbSQL *sql);
void mdb_sql_reset(MdbSQL *sql);
void mdb_sql_listtables(MdbSQL *sql);
void mdb_sql_select(MdbSQL *sql);
void mdb_sql_dump_node(MdbSargNode *node, int level);
void mdb_sql_close(MdbSQL *sql);
void mdb_sql_add_or(MdbSQL *sql);
void mdb_sql_add_and(MdbSQL *sql);
void mdb_sql_add_not(MdbSQL *sql);
void mdb_sql_describe_table(MdbSQL *sql);
MdbSQL* mdb_sql_run_query (MdbSQL*, const gchar*);
void mdb_sql_set_maxrow(MdbSQL *sql, int maxrow);
int mdb_sql_eval_expr(MdbSQL *sql, char *const1, int op, char *const2);
void mdb_sql_bind_all(MdbSQL *sql);
int mdb_sql_fetch_row(MdbSQL *sql, MdbTableDef *table);
int mdb_sql_add_temp_col(MdbSQL *sql, MdbTableDef *ttable, int col_num, char *name, int col_type, int col_size, int is_fixed);
void mdb_sql_bind_column(MdbSQL *sql, int colnum, void *varaddr, int *len_ptr);
int mdb_sql_add_limit(MdbSQL *sql, char *limit);


int parse_sql(MdbSQL * mdb, const gchar* str);

#ifdef __cplusplus
  }
#endif

#endif
