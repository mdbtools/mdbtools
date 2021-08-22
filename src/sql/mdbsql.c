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

#include <stdarg.h>
#include "mdbsql.h"

#ifdef HAVE_STRPTIME
#include <time.h>
#include <stdio.h>
#endif

#include <locale.h>

/** \addtogroup mdbsql
 *  @{
 */

/* Prevent warnings from -Wmissing-prototypes.  */
#ifdef YYPARSE_PARAM
#if defined __STDC__ || defined __cplusplus
int yyparse (void *YYPARSE_PARAM);
#else
int yyparse ();
#endif
#else /* ! YYPARSE_PARAM */
#if defined __STDC__ || defined __cplusplus
int yyparse (void);
#else
int yyparse ();
#endif
#endif /* ! YYPARSE_PARAM */

static MdbSargNode * mdb_sql_alloc_node(void);

void
mdb_sql_error(MdbSQL* sql, const char* fmt, ...)
{
va_list ap;

	va_start(ap, fmt);
	vsnprintf(sql->error_msg, sizeof(sql->error_msg), fmt, ap);
	va_end(ap);

	fprintf(stderr, "%s\n", sql->error_msg);
}

MdbSQL *mdb_sql_init()
{
MdbSQL *sql = g_malloc0(sizeof(MdbSQL));
	sql->columns = g_ptr_array_new();
	sql->tables = g_ptr_array_new();
	sql->bound_values = g_ptr_array_new();
	sql->sarg_tree = NULL;
	sql->sarg_stack = NULL;
	sql->max_rows = -1;
	sql->limit = -1;
	sql->limit_percent = 0;

	return sql;
}

#ifndef _
#define _(x) x
#endif

/**
 *
 * @param sql: MdbSQL object to execute the query on.
 * @param querystr: SQL query string to execute.
 *
 * Parses \p querystr and executes it within the given \p sql object.
 *
 * @return  the updated MDB SQL object, or NULL on error
 **/
MdbSQL*
mdb_sql_run_query (MdbSQL* sql, const gchar* querystr) {
	g_return_val_if_fail (sql, NULL);
	g_return_val_if_fail (querystr, NULL);

	sql->error_msg[0]='\0';

	if (parse_sql (sql, querystr)) {
		/* end unsafe */
		mdb_sql_error (sql, _("Could not parse '%s' command"), querystr);
		mdb_sql_reset (sql);
		return NULL;
	}

	if (sql->cur_table == NULL) {
		/* Invalid column name? (should get caught by mdb_sql_select,
		 * but it appeared to happen anyway with 0.5) */
		mdb_sql_error (sql, _("Got no result for '%s' command"), querystr);
		return NULL;
	}

	if (mdb_sql_bind_all(sql) == -1) {
		mdb_sql_error (sql, _("Failed to bind columns for '%s' command"), querystr);
		return NULL;
	}

	return sql;
}

void mdb_sql_set_maxrow(MdbSQL *sql, int maxrow)
{
	sql->max_rows = maxrow;
}

static void mdb_sql_free_columns(GPtrArray *columns)
{
	int i;
	if (!columns) return;
	for (i=0; i<columns->len; i++) {
		MdbSQLColumn *c = (MdbSQLColumn *)g_ptr_array_index(columns, i);
		g_free(c->name);
		g_free(c);
	}
	g_ptr_array_free(columns, TRUE);
}
static void mdb_sql_free_tables(GPtrArray *tables)
{
	int i;
	if (!tables) return;
	for (i=0; i<tables->len; i++) {
		MdbSQLTable *t = (MdbSQLTable *)g_ptr_array_index(tables, i);
		g_free(t->name);
		g_free(t);
	}
	g_ptr_array_free(tables, TRUE);
}

/* This gives us a nice, uniform place to keep up with memory that needs to be freed */
static void mdb_sql_free(MdbSQL *sql)
{
	/* Free MdbTableDef structures	*/
	if (sql->cur_table) {
		mdb_index_scan_free(sql->cur_table);
		mdb_free_tabledef(sql->cur_table);
		sql->cur_table = NULL;
	}

	/* Free MdbSQLColumns and MdbSQLTables */
	mdb_sql_free_columns(sql->columns);
	mdb_sql_free_tables(sql->tables);

	/* Free sargs */
	if (sql->sarg_tree) {
		mdb_sql_free_tree(sql->sarg_tree);
		sql->sarg_tree = NULL;
	}

	g_list_free(sql->sarg_stack);
	sql->sarg_stack = NULL;

	/* Free bindings  */
	mdb_sql_unbind_all(sql);
	g_ptr_array_free(sql->bound_values, TRUE);
}

void
mdb_sql_close(MdbSQL *sql)
{
	if (sql->mdb) {
		mdb_close(sql->mdb);
		sql->mdb = NULL;
	} else {
		mdb_sql_error(sql, "Not connected.");
	}
}

MdbHandle *mdb_sql_open(MdbSQL *sql, char *db_name)
{
	sql->mdb = mdb_open(db_name, MDB_NOFLAGS);
	if ((!sql->mdb) && (!strstr(db_name, ".mdb"))) {
		char *tmpstr = g_strconcat(db_name, ".mdb", NULL);
		sql->mdb = mdb_open(tmpstr, MDB_NOFLAGS);
		g_free(tmpstr);
	}
	if (!sql->mdb) {
		mdb_sql_error(sql, "Unable to locate database %s", db_name);
	}

	return sql->mdb;
}
static MdbSargNode *
mdb_sql_alloc_node()
{
	return (MdbSargNode *) g_malloc0(sizeof(MdbSargNode));
}
void
mdb_sql_free_tree(MdbSargNode *tree)
{

	if (tree->left) mdb_sql_free_tree(tree->left);
	if (tree->right) mdb_sql_free_tree(tree->right);
	if (tree->parent) g_free(tree->parent);
	g_free(tree);
}
void
mdb_sql_push_node(MdbSQL *sql, MdbSargNode *node)
{
	sql->sarg_stack = g_list_append(sql->sarg_stack, node);
	/*
	 * Tree builds from bottom to top, so we should be left with
	 * the correct tree root when done
	 */
	sql->sarg_tree = node;
}
MdbSargNode *
mdb_sql_pop_node(MdbSQL *sql)
{
	GList *glist;
	MdbSargNode *node;

	glist = g_list_last(sql->sarg_stack);
	if (!glist) return NULL;
	
	node = glist->data;
#if 0
	if (node->op==MDB_EQUAL) 
		printf("popping %d\n", node->value.i);
	else
		printf("popping %s\n", node->op == MDB_OR ? "OR" : "AND");
#endif
	sql->sarg_stack = g_list_remove(sql->sarg_stack, node);
	return node;
}

void 
mdb_sql_add_not(MdbSQL *sql)
{
	MdbSargNode *node, *left;

	left = mdb_sql_pop_node(sql);
	if (!left) {
		mdb_sql_error(sql, "parse error near 'NOT'");
		mdb_sql_reset(sql);
		return;
	}
	node = mdb_sql_alloc_node();
	node->op = MDB_NOT;
	node->left = left;
	mdb_sql_push_node(sql, node);
}
void 
mdb_sql_add_or(MdbSQL *sql)
{
	MdbSargNode *node, *left, *right;

	left = mdb_sql_pop_node(sql);
	right = mdb_sql_pop_node(sql);
	if (!left || !right) {
		mdb_sql_error(sql, "parse error near 'OR'");
		mdb_sql_reset(sql);
		return;
	}
	node = mdb_sql_alloc_node();
	node->op = MDB_OR;
	node->left = left;
	node->right = right;
	mdb_sql_push_node(sql, node);
}
void 
mdb_sql_add_and(MdbSQL *sql)
{
	MdbSargNode *node, *left, *right;

	left = mdb_sql_pop_node(sql);
	right = mdb_sql_pop_node(sql);
	if (!left || !right) {
		mdb_sql_error(sql, "parse error near 'AND'");
		mdb_sql_reset(sql);
		return;
	}
	node = mdb_sql_alloc_node();
	node->op = MDB_AND;
	node->left = left;
	node->right = right;
	mdb_sql_push_node(sql, node);
}
void 
mdb_sql_dump_node(MdbSargNode *node, int level)
{
	int i;
	int mylevel = level+1;

	if (!level) 
		printf("root  ");
	for (i=0;i<mylevel;i++) printf("--->");
	switch (node->op) {
		case MDB_OR: 
			printf(" or\n"); 
			break;
		case MDB_AND: 
			printf(" and\n"); 
			break;
		case MDB_NOT: 
			printf(" not\n"); 
			break;
		case MDB_LT: 
			printf(" < %d\n", node->value.i); 
			break;
		case MDB_GT: 
			printf(" > %d\n", node->value.i); 
			break;
		case MDB_LIKE: 
			printf(" like %s\n", node->value.s); 
			break;
		case MDB_ILIKE:
			printf(" ilike %s\n", node->value.s);
			break;
		case MDB_EQUAL: 
			printf(" = %d\n", node->value.i); 
			break;
	}
	if (node->left) {
		printf("left  ");
		mdb_sql_dump_node(node->left, mylevel);
	}
	if (node->right) {
		printf("right ");
		mdb_sql_dump_node(node->right, mylevel);
	}
}

/* Parses date format specifier into JET date representation (double) */
char *
mdb_sql_strptime(MdbSQL *sql, char *data, char *format)
{
#ifndef HAVE_STRPTIME
	mdb_sql_error(sql, "Your system doesn't support strptime.");
	mdb_sql_reset(sql);
	return NULL;
#else
	char *p, *pszDate;
	struct tm tm={0};
	double date=0;

	if (data[0]!='\'' || *(p=&data[strlen(data)-1])!='\'') {
		mdb_sql_error(sql, "First parameter of strptime (data) must be a string.");
		mdb_sql_reset(sql);
		return NULL;
	}
	*p=0; ++data;
	if (format[0]!='\'' || *(p=&format[strlen(format)-1])!='\'') {
		mdb_sql_error(sql, "Second parameter of strptime (format) must be a string.");
		mdb_sql_reset(sql);
		return NULL;
	}
	*p=0; ++format;
	if (!strptime(data, format, &tm)) {
		mdb_sql_error(sql, "strptime('%s','%s') failed.", data, format);
		mdb_sql_reset(sql);
		return NULL;
	}
	mdb_tm_to_date(&tm, &date);
	/* It seems that when just using a time offset without date in strptime, 
	 * we always get 1 as decimal part, een though it should be 0 for time */
	if (date < 2 && date > 1) date--;
	if ((pszDate=malloc(16))) {
		char cLocale=localeconv()->decimal_point[0], *p;
		snprintf(pszDate, 16, "%lf", date);
		if (cLocale!='.') for (p=pszDate; *p; p++) if (*p==cLocale) *p='.';
	}
	return pszDate;
#endif
}

/* evaluate a expression involving 2 constants and add answer to the stack */
int 
mdb_sql_eval_expr(MdbSQL *sql, char *const1, int op, char *const2)
{
	long val1, val2, value, compar;
	unsigned char illop = 0; 
	MdbSargNode *node;

	if (const1[0]=='\'' && const2[0]=='\'') {
		value = strcoll(const1, const2);
		switch (op) {
			case MDB_EQUAL: compar = (value ? 0 : 1); break;
			case MDB_GT: compar = (value > 0); break;
			case MDB_GTEQ: compar = (value >= 0); break;
			case MDB_LT: compar = (value < 0); break;
			case MDB_LTEQ: compar = (value <= 0); break;
			case MDB_LIKE: compar = mdb_like_cmp(const1,const2); break;
			case MDB_ILIKE: compar = mdb_ilike_cmp(const1,const2); break;
			case MDB_NEQ: compar = (value ? 1 : 0); break;
			default: illop = 1;
		}
	} else if (const1[0]!='\'' && const2[0]!='\'') {
		val1 = atol(const1);
		val2 = atol(const2);
		switch (op) {
			case MDB_EQUAL: compar = (val1 == val2); break;
			case MDB_GT: compar = (val1 > val2); break;
			case MDB_GTEQ: compar = (val1 >= val2); break;
			case MDB_LT: compar = (val1 < val2); break;
			case MDB_LTEQ: compar = (val1 <= val2); break;
			case MDB_NEQ: compar = (val1 != val2); break;
			default: illop = 1;
		}
	} else {
		mdb_sql_error(sql, "Comparison of strings and numbers not allowed.");
		/* the column and table names are no good now */
		mdb_sql_reset(sql);
		return 1;
	}
	if (illop) {
		mdb_sql_error(sql, "Illegal operator used for comparison of literals.");
		/* the column and table names are no good now */
		mdb_sql_reset(sql);
		return 1;
	}
	node = mdb_sql_alloc_node();
	node->op = MDB_EQUAL;
	node->col = NULL;
	node->value.i = (compar ? 1 : 0);
	mdb_sql_push_node(sql, node);
	return 0;
}
int 
mdb_sql_add_sarg(MdbSQL *sql, char *col_name, int op, char *constant)
{
	char *p;
	MdbSargNode *node;

	node = mdb_sql_alloc_node();
	node->op = op;
	/* stash the column name until we finish with the grammar */
	node->parent = (void *) g_strdup(col_name);

	if (!constant) {
		/* XXX - do we need to check operator? */
		mdb_sql_push_node(sql, node);
		return 0;
	}
	/* FIX ME -- we should probably just be storing the ascii value until the 
	** column definition can be checked for validity
	*/
	if (constant[0]=='\'') {
		snprintf(node->value.s, sizeof(node->value.s), "%.*s", (int)strlen(constant) - 2, &constant[1]);
		node->val_type = MDB_TEXT;
	} else if ((p=strchr(constant, '.'))) {
		*p=localeconv()->decimal_point[0];
		node->value.d = strtod(constant, NULL);
		node->val_type = MDB_DOUBLE;
	} else {
		node->value.i = atoi(constant);
		node->val_type = MDB_INT;
	}

	mdb_sql_push_node(sql, node);

	return 0;
}
void
mdb_sql_all_columns(MdbSQL *sql)
{
	sql->all_columns=1;	
}
void
mdb_sql_sel_count(MdbSQL *sql)
{
	sql->sel_count=1;
}

int mdb_sql_add_column(MdbSQL *sql, char *column_name)
{
	MdbSQLColumn *c = g_malloc0(sizeof(MdbSQLColumn));
	c->name = g_strdup(column_name);
	g_ptr_array_add(sql->columns, c);
	sql->num_columns++;
	return 0;
}
int mdb_sql_add_limit(MdbSQL *sql, char *limit, int percent)
{
	sql->limit = atoi(limit);
	sql->limit_percent = percent;

	if (sql->limit_percent && (sql->limit < 0 || sql->limit > 100)) {
		return 1;
	}

	return 0;
}

int mdb_sql_get_limit(MdbSQL *sql)
{
	return sql->limit;
}

int mdb_sql_add_function1(MdbSQL *sql, char *func_name, char *arg1)
{
	fprintf(stderr, "calling function %s with %s", func_name, arg1);
	return 0;
}
int mdb_sql_add_table(MdbSQL *sql, char *table_name)
{
	MdbSQLTable *t = g_malloc0(sizeof(MdbSQLTable));
	t->name = g_strdup(table_name);
	t->alias = NULL;
	g_ptr_array_add(sql->tables, t);
	sql->num_tables++;
	return 0;
}
void mdb_sql_dump(MdbSQL *sql)
{
	unsigned int i;
	MdbSQLColumn *c;
	MdbSQLTable *t;

	for (i=0;i<sql->num_columns;i++) {
		c = g_ptr_array_index(sql->columns,i);
		printf("column = %s\n",c->name);
	}
	for (i=0;i<sql->num_tables;i++) {
		t = g_ptr_array_index(sql->tables,i);
		printf("table = %s\n",t->name);
	}
}
void mdb_sql_exit(MdbSQL *sql)
{
	mdb_sql_free(sql);
	if (sql->mdb)
		mdb_close(sql->mdb);
	
	/* Cleanup the SQL engine object */
	g_free(sql);
}
void mdb_sql_reset(MdbSQL *sql)
{
	mdb_sql_free(sql);

	/* Reset columns */
	sql->num_columns = 0;
	sql->columns = g_ptr_array_new();

	/* Reset MdbSQL tables */
	sql->num_tables = 0;
	sql->tables = g_ptr_array_new();

	/* Reset bindings */
	sql->bound_values = g_ptr_array_new();

	sql->all_columns = 0;
	sql->sel_count = 0;
	sql->max_rows = -1;
	sql->row_count = 0;
	sql->limit = -1;
}
static void print_break(int sz, int first)
{
int i;
	if (first) {
		fprintf(stdout,"+");
	}
	for (i=0;i<sz;i++) {
		fprintf(stdout,"-");
	}
	fprintf(stdout,"+");
}
static void print_value(char *v, int sz, int first)
{
int i;
int vlen;

	if (first) {
		fprintf(stdout,"|");
	}
	vlen = strlen(v);
	for (i=0;i<sz;i++) {
		fprintf(stdout,"%c",i >= vlen ? ' ' : v[i]);
	}
	fprintf(stdout,"|");
}
void mdb_sql_listtables(MdbSQL *sql)
{
	unsigned int i;
	MdbCatalogEntry *entry;
	MdbHandle *mdb = sql->mdb;
	MdbField fields[1];
	unsigned char row_buffer[MDB_PGSIZE];
	int row_size;
	MdbTableDef *ttable;
	gchar tmpstr[100];
	int tmpsiz;

	if (!mdb) {
		mdb_sql_error(sql, "You must connect to a database first");
		return;
	}
	mdb_read_catalog (mdb, MDB_TABLE);

	ttable = mdb_create_temp_table(mdb, "#listtables");
	mdb_sql_add_temp_col(sql, ttable, 0, "Tables", MDB_TEXT, 30, 0);

 	/* add all user tables in catalog to list */
 	for (i=0; i < mdb->num_catalog; i++) {
 		entry = g_ptr_array_index (mdb->catalog, i);
 		if (mdb_is_user_table(entry)) {
			//col = g_ptr_array_index(table->columns,0);
			tmpsiz = mdb_ascii2unicode(mdb, entry->object_name, 0, tmpstr, sizeof(tmpstr));
			mdb_fill_temp_field(&fields[0],tmpstr, tmpsiz, 0,0,0,0);
			row_size = mdb_pack_row(ttable, row_buffer, 1, fields);
			mdb_add_row_to_pg(ttable,row_buffer, row_size);
			ttable->num_rows++;
		}
	}
	sql->cur_table = ttable;

}
int
mdb_sql_add_temp_col(MdbSQL *sql, MdbTableDef *ttable, int col_num, char *name, int col_type, int col_size, int is_fixed)
{
	MdbColumn tcol;
	MdbSQLColumn *sqlcol;

	mdb_fill_temp_col(&tcol, name, col_size, col_type, is_fixed);
	mdb_temp_table_add_col(ttable, &tcol);
	mdb_sql_add_column(sql, name);
	sqlcol = g_ptr_array_index(sql->columns,col_num);
	sqlcol->disp_size = mdb_col_disp_size(&tcol);

	return 0;
}
void mdb_sql_describe_table(MdbSQL *sql)
{
	MdbTableDef *ttable, *table = NULL;
	MdbSQLTable *sql_tab;
	MdbHandle *mdb = sql->mdb;
	MdbColumn *col;
	unsigned int i;
	MdbField fields[3];
	char tmpstr[256];
	unsigned char row_buffer[MDB_PGSIZE];
	int row_size;
	gchar col_name[100], col_type[100], col_size[100];
	int tmpsiz;

	if (!mdb) {
		mdb_sql_error(sql, "You must connect to a database first");
		return;
	}

	sql_tab = g_ptr_array_index(sql->tables,0);

	table = mdb_read_table_by_name(mdb, sql_tab->name, MDB_TABLE);
	if (!table) {
		mdb_sql_error(sql, "%s is not a table in this database", sql_tab->name);
		/* the column and table names are no good now */
		mdb_sql_reset(sql);
		return;
	}

	if (!mdb_read_columns(table)) {
		mdb_sql_error(sql, "Could not read columns of table %s", sql_tab->name);
		/* the column and table names are no good now */
		mdb_sql_reset(sql);
		return;
    }

	ttable = mdb_create_temp_table(mdb, "#describe");

	mdb_sql_add_temp_col(sql, ttable, 0, "Column Name", MDB_TEXT, 30, 0);
	mdb_sql_add_temp_col(sql, ttable, 1, "Type", MDB_TEXT, 20, 0);
	mdb_sql_add_temp_col(sql, ttable, 2, "Size", MDB_TEXT, 10, 0);

	for (i=0;i<table->num_cols;i++) {

		col = g_ptr_array_index(table->columns,i);
		tmpsiz = mdb_ascii2unicode(mdb, col->name, 0, col_name, sizeof(col_name));
		mdb_fill_temp_field(&fields[0],col_name, tmpsiz, 0,0,0,0);

		snprintf(tmpstr, sizeof(tmpstr), "%s", mdb_get_colbacktype_string(col));
		tmpsiz = mdb_ascii2unicode(mdb, tmpstr, 0, col_type, sizeof(col_type));
		mdb_fill_temp_field(&fields[1],col_type, tmpsiz, 0,0,0,1);

		snprintf(tmpstr, sizeof(tmpstr), "%d", col->col_size);
		tmpsiz = mdb_ascii2unicode(mdb, tmpstr, 0, col_size, sizeof(col_size));
		mdb_fill_temp_field(&fields[2],col_size, tmpsiz, 0,0,0,2);

		row_size = mdb_pack_row(ttable, row_buffer, 3, fields);
		mdb_add_row_to_pg(ttable,row_buffer, row_size);
		ttable->num_rows++;
	}

	/* the column and table names are no good now */
	//mdb_sql_reset(sql);
	sql->cur_table = ttable;
}

MdbColumn *mdb_sql_find_colbyname(MdbTableDef *table, char *name)
{
	unsigned int i;
	MdbColumn *col;

	for (i=0;i<table->num_cols;i++) {
		col=g_ptr_array_index(table->columns,i);
		if (!g_ascii_strcasecmp(col->name, name)) return col;
	}
	return NULL;
}

int mdb_sql_find_sargcol(MdbSargNode *node, gpointer data)
{
	MdbTableDef *table = data;
	MdbColumn *col;

	if (!mdb_is_relational_op(node->op)) return 0;
	if (!node->parent) return 0;

	if ((col = mdb_sql_find_colbyname(table, (char *)node->parent))) {
		node->col = col;
		/* Do conversion to required target value type.
		 * Plain integers are UNIX timestamps for backwards compatibility of parser
		*/
		if (col->col_type == MDB_DATETIME && node->val_type == MDB_INT) {
			struct tm *tmp;
#ifdef HAVE_GMTIME_R
			struct tm tm;
			tmp = gmtime_r((time_t*)&node->value.i, &tm);
#else		// regular gmtime on Windows uses thread-local storage
			tmp = gmtime((time_t*)&node->value.i);
#endif
			mdb_tm_to_date(tmp, &node->value.d);
			node->val_type = MDB_DOUBLE;
		}
	}
	return 0;
}

void 
mdb_sql_select(MdbSQL *sql)
{
unsigned int i,j;
MdbHandle *mdb = sql->mdb;
MdbTableDef *table = NULL;
MdbSQLTable *sql_tab;
MdbColumn *col;
MdbSQLColumn *sqlcol;
int found = 0;

	if (!mdb) {
		mdb_sql_error(sql, "You must connect to a database first");
		return;
	}

	if (!sql->num_tables) return;
	sql_tab = g_ptr_array_index(sql->tables,0);

	table = mdb_read_table_by_name(mdb, sql_tab->name, MDB_TABLE);
	if (!table) {
		mdb_sql_error(sql, "%s is not a table in this database", sql_tab->name);
		/* the column and table names are no good now */
		mdb_sql_reset(sql);
		return;
	}
	if (!mdb_read_columns(table)) {
		mdb_sql_error(sql, "Could not read columns of table %s", sql_tab->name);
		/* the column and table names are no good now */
		mdb_sql_reset(sql);
		return;
    }

	if (sql->sel_count && !sql->sarg_tree) {
		MdbTableDef *ttable = mdb_create_temp_table(mdb, "#count");
		char tmpstr[32];
		gchar row_cnt[32];
		unsigned char row_buffer[MDB_PGSIZE];
		MdbField fields[1];
		int row_size, tmpsiz;

		mdb_sql_add_temp_col(sql, ttable, 0, "count", MDB_TEXT, 30, 0);
		snprintf(tmpstr, sizeof(tmpstr), "%d", table->num_rows);
		tmpsiz = mdb_ascii2unicode(mdb, tmpstr, 0, row_cnt, sizeof(row_cnt));
		mdb_fill_temp_field(&fields[0],row_cnt, tmpsiz, 0,0,0,0);
		row_size = mdb_pack_row(ttable, row_buffer, 1, fields);
		mdb_add_row_to_pg(ttable,row_buffer, row_size);
		ttable->num_rows++;
		sql->cur_table = ttable;
		mdb_free_tabledef(table);
		return;
	}

	mdb_read_indices(table);
	mdb_rewind_table(table);

	if (sql->all_columns) {
		for (i=0;i<table->num_cols;i++) {
			col = g_ptr_array_index(table->columns,i);
			mdb_sql_add_column(sql, col->name);
		}
	}
	/* verify all specified columns exist in this table */
	for (i=0;i<sql->num_columns;i++) {
		sqlcol = g_ptr_array_index(sql->columns,i);
		found=0;
		for (j=0;j<table->num_cols;j++) {
			col=g_ptr_array_index(table->columns,j);
			if (!g_ascii_strcasecmp(sqlcol->name, col->name)) {
				sqlcol->disp_size = mdb_col_disp_size(col);
				found=1;
				break;
			}
		}
		if (!found) {
			mdb_sql_error(sql, "Column %s not found",sqlcol->name);
			mdb_index_scan_free(table);
			mdb_free_tabledef(table);
			mdb_sql_reset(sql);
			return;
		}
	}

	/* 
	 * resolve column names to MdbColumn structs
	 */
	if (sql->sarg_tree) {
		mdb_sql_walk_tree(sql->sarg_tree, mdb_sql_find_sargcol, table);
		mdb_sql_walk_tree(sql->sarg_tree, mdb_find_indexable_sargs, NULL);
	}
	/* 
	 * move the sarg_tree.  
	 * XXX - this won't work when we implement joins 
	 */
	table->sarg_tree = sql->sarg_tree;
	sql->sarg_tree = NULL;

	sql->cur_table = table;
	mdb_index_scan_init(mdb, table);

	/* We know how many rows there are, so convert limit percentage
	 * to an row count */
	if (sql->limit != -1 && sql->limit_percent) {
		sql->limit = (int)((double)table->num_rows / 100 * sql->limit);
		sql->limit_percent = 0;
	}
}

int
mdb_sql_bind_column(MdbSQL *sql, int colnum, void *varaddr, int *len_ptr)
{
	MdbSQLColumn *sqlcol;

	if (colnum <= 0 || colnum > sql->num_columns)
		return -1;

	/* sql columns are traditionally 1 based, so decrement colnum */
	sqlcol = g_ptr_array_index(sql->columns,colnum - 1);
	return mdb_bind_column_by_name(sql->cur_table, sqlcol->name, varaddr, len_ptr);
}

int
mdb_sql_bind_all(MdbSQL *sql)
{
	unsigned int i;
	void *bound_value;

	for (i=0;i<sql->num_columns;i++) {
		bound_value = g_malloc0(sql->mdb->bind_size);
		g_ptr_array_add(sql->bound_values, bound_value);
		if (mdb_sql_bind_column(sql, i+1, bound_value, NULL) == -1) {
			mdb_sql_unbind_all(sql);
			return -1;
		}
	}
	return sql->num_columns;
}

void mdb_sql_unbind_all(MdbSQL *sql)
{
	unsigned int i;

	for (i=0;i<sql->bound_values->len;i++) {
		g_free(g_ptr_array_index(sql->bound_values, i));
	}
}

/*
 * mdb_sql_fetch_row is now just a wrapper around mdb_fetch_row.
 *   It is left here only for backward compatibility.
 */
int
mdb_sql_fetch_row(MdbSQL *sql, MdbTableDef *table)
{
	int rc = mdb_fetch_row(table);
	if (rc) {
		if (sql->limit >= 0 && sql->row_count + 1 > sql->limit) {
			return 0;
		}
		sql->row_count++;
	}
	return rc;
}

void 
mdb_sql_dump_results(MdbSQL *sql)
{
	unsigned int j;
	MdbSQLColumn *sqlcol;

	/* print header */
	for (j=0;j<sql->num_columns;j++) {
		sqlcol = g_ptr_array_index(sql->columns,j);
		print_break(sqlcol->disp_size, !j);
	}
	fprintf(stdout,"\n");
	for (j=0;j<sql->num_columns;j++) {
		sqlcol = g_ptr_array_index(sql->columns,j);
		print_value(sqlcol->name,sqlcol->disp_size,!j);
	}
	fprintf(stdout,"\n");
	for (j=0;j<sql->num_columns;j++) {
		sqlcol = g_ptr_array_index(sql->columns,j);
		print_break(sqlcol->disp_size, !j);
	}
	fprintf(stdout,"\n");

	/* print each row */
	while(mdb_fetch_row(sql->cur_table)) {
  		for (j=0;j<sql->num_columns;j++) {
			sqlcol = g_ptr_array_index(sql->columns,j);
			print_value(g_ptr_array_index(sql->bound_values, j),sqlcol->disp_size,!j);
		}
		fprintf(stdout,"\n");
	}

	/* footer */
	for (j=0;j<sql->num_columns;j++) {
		sqlcol = g_ptr_array_index(sql->columns,j);
		print_break(sqlcol->disp_size, !j);
	}

	fprintf(stdout,"\n");

	/* the column and table names are no good now */
	mdb_sql_reset(sql);
}
/** @}*/
