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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "mdbsql.h"
#include <stdarg.h>

#ifdef DMALLOC
#include "dmalloc.h"
#endif

void mdb_dump_results(MdbSQL *sql);

#ifdef HAVE_WORDEXP_H
#define HAVE_WORDEXP
#include <wordexp.h>
#endif

char *g_input_ptr;

void
mdb_sql_error(MdbSQL* sql, char *fmt, ...)
{
va_list ap;

	va_start(ap, fmt);
	vfprintf (stderr, fmt, ap);
	vsprintf(sql->error_msg, fmt, ap);
	va_end(ap);
	fprintf(stderr,"\n");
}
void
mdb_sql_clear_error(MdbSQL* sql)
{
	sql->error_msg[0]='\0';
}
char *
mdb_sql_last_error(MdbSQL* sql)
{
	return sql->error_msg;
}
unsigned char
mdb_sql_has_error(MdbSQL* sql)
{
	return (sql->error_msg[0] ? 1 : 0);
}
int mdb_sql_yyinput(char *buf, int need)
{
int cplen, have;

	have = strlen(g_input_ptr);
	cplen = need > have ? have : need;
	
	if (cplen>0) {
		memcpy(buf, g_input_ptr, cplen);
		g_input_ptr += cplen;
	} 
	return cplen;
}
MdbSQL *mdb_sql_init()
{
MdbSQL *sql;

	mdb_init();
	sql = (MdbSQL *) g_malloc0(sizeof(MdbSQL));
	sql->columns = g_ptr_array_new();
	sql->tables = g_ptr_array_new();
	sql->sarg_tree = NULL;
	sql->sarg_stack = NULL;
	sql->max_rows = -1;

	return sql;
}

#ifndef _
#define _(x) x
#endif

void mdb_sql_bind_all (MdbSQL*);

/**
 * mdb_sql_run_query:
 * @sql: MDB SQL object to execute the query on.
 * @querystr: SQL query string to execute.
 *
 * Parses @querystr and executes it within the given @sql object.
 *
 * Returns: the updated MDB SQL object, or NULL on error
 **/
MdbSQL*
mdb_sql_run_query (MdbSQL* sql, const gchar* querystr) {
	g_return_val_if_fail (sql, NULL);
	g_return_val_if_fail (querystr, NULL);

	g_input_ptr = (gchar*) querystr;

	/* calls to yyparse should be serialized for thread safety */

	/* begin unsafe */
	_mdb_sql (sql);
	mdb_sql_clear_error(sql);
	if (yyparse()) {
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

	mdb_sql_bind_all (sql);

	return sql;
}

void mdb_sql_set_maxrow(MdbSQL *sql, int maxrow)
{
	sql->max_rows = maxrow;
}

static void mdb_sql_free_columns(GPtrArray *columns)
{
	unsigned int i;
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
	unsigned int i;
	if (!tables) return;
	for (i=0; i<tables->len; i++) {
		MdbSQLTable *t = (MdbSQLTable *)g_ptr_array_index(tables, i);
		g_free(t->name);
		g_free(t);
	}
	g_ptr_array_free(tables, TRUE);
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
	char *db_namep = db_name;

#ifdef HAVE_WORDEXP
	wordexp_t words;

	if (wordexp(db_name, &words, 0)==0) {
		if (words.we_wordc>0) 
			db_namep = words.we_wordv[0];
	}
	
#endif

	sql->mdb = mdb_open(db_namep, MDB_NOFLAGS);
	if ((!sql->mdb) && (!strstr(db_namep, ".mdb"))) {
		char *tmpstr = (char *) g_strconcat(db_namep, ".mdb", NULL);
		sql->mdb = mdb_open(tmpstr, MDB_NOFLAGS);
		g_free(tmpstr);
	}
	if (!sql->mdb) {
		mdb_sql_error(sql, "Unable to locate database %s", db_name);
	}

#ifdef HAVE_WORDEXP
	wordfree(&words);
#endif	

	return sql->mdb;
}
MdbSargNode *
mdb_sql_alloc_node()
{
	return (MdbSargNode *) g_malloc0(sizeof(MdbSargNode));
}
void
mdb_sql_free_tree(MdbSargNode *tree)
{

	if (tree->left) mdb_sql_free_tree(tree->left);
	if (tree->right) mdb_sql_free_tree(tree->right);
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
			printf(" < %d\n", node->value.i); 
			break;
		case MDB_LIKE: 
			printf(" like %s\n", node->value.s); 
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
/* evaluate a expression involving 2 constants and add answer to the stack */
int 
mdb_sql_eval_expr(MdbSQL *sql, char *const1, int op, char *const2)
{
	long val1, val2, value, compar;
	unsigned char illop = 0; 
	MdbSargNode *node;

	if (const1[0]=='\'' && const2[0]=='\'') {
		value = strcmp(const1, const2);
		switch (op) {
			case MDB_EQUAL: compar = (value ? 0 : 1); break;
			case MDB_GT: compar = (value > 0); break;
			case MDB_GTEQ: compar = (value >= 0); break;
			case MDB_LT: compar = (value < 0); break;
			case MDB_LTEQ: compar = (value <= 0); break;
			case MDB_LIKE: compar = mdb_like_cmp(const1,const2); break;
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
			default: illop = 1;
		}
	} else {
		mdb_sql_error(sql, "Comparison of strings and numbers not allowed.");
		/* the column and table names are no good now */
		mdb_sql_reset(sql);
		return 1;
	}
	if (illop) {
		mdb_sql_error(sql, "Illegal operator used for comparision of literals.");
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
	int lastchar;
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
		lastchar = strlen(constant) > 256 ? 256 : strlen(constant);
		strncpy(node->value.s, &constant[1], lastchar - 2);;
		node->value.s[lastchar - 1]='\0';
	} else {
		node->value.i = atoi(constant);
	}

	mdb_sql_push_node(sql, node);

	return 0;
}
void
mdb_sql_all_columns(MdbSQL *sql)
{
	sql->all_columns=1;	
}
int mdb_sql_add_column(MdbSQL *sql, char *column_name)
{
	MdbSQLColumn *c;

	c = (MdbSQLColumn *) g_malloc0(sizeof(MdbSQLColumn));
	c->name = g_strdup(column_name);
	g_ptr_array_add(sql->columns, c);
	sql->num_columns++;
	return 0;
}
int mdb_sql_add_table(MdbSQL *sql, char *table_name)
{
	MdbSQLTable *t;

	t = (MdbSQLTable *) g_malloc0(sizeof(MdbSQLTable));
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
	mdb_sql_free_columns(sql->columns);
	mdb_sql_free_tables(sql->tables);

	if (sql->sarg_tree) {
		mdb_sql_free_tree(sql->sarg_tree);
		sql->sarg_tree = NULL;
	}
	g_list_free(sql->sarg_stack);
	sql->sarg_stack = NULL;

	if (sql->mdb) {
		mdb_close(sql->mdb);
	}
}
void mdb_sql_reset(MdbSQL *sql)
{
	if (sql->cur_table) {
		mdb_index_scan_free(sql->cur_table);
		mdb_free_tabledef(sql->cur_table);
		sql->cur_table = NULL;
	}

	/* Reset columns */
	mdb_sql_free_columns(sql->columns);
	sql->num_columns = 0;
	sql->columns = g_ptr_array_new();

	/* Reset tables */
	mdb_sql_free_tables(sql->tables);
	sql->num_tables = 0;
	sql->tables = g_ptr_array_new();

	/* Reset sargs */
	if (sql->sarg_tree) {
		mdb_sql_free_tree(sql->sarg_tree);
		sql->sarg_tree = NULL;
	}
	g_list_free(sql->sarg_stack);
	sql->sarg_stack = NULL;

	sql->all_columns = 0;
	sql->max_rows = -1;
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
			tmpsiz = mdb_ascii2unicode(mdb, entry->object_name, 0, tmpstr, 100);
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

	mdb_read_columns(table);

	ttable = mdb_create_temp_table(mdb, "#describe");

	mdb_sql_add_temp_col(sql, ttable, 0, "Column Name", MDB_TEXT, 30, 0);
	mdb_sql_add_temp_col(sql, ttable, 1, "Type", MDB_TEXT, 20, 0);
	mdb_sql_add_temp_col(sql, ttable, 2, "Size", MDB_TEXT, 10, 0);

     for (i=0;i<table->num_cols;i++) {

        col = g_ptr_array_index(table->columns,i);
		tmpsiz = mdb_ascii2unicode(mdb, col->name, 0, col_name, 100);
		mdb_fill_temp_field(&fields[0],col_name, tmpsiz, 0,0,0,0);

		strcpy(tmpstr, mdb_get_colbacktype_string(col));
		tmpsiz = mdb_ascii2unicode(mdb, tmpstr, 0, col_type, 100);
		mdb_fill_temp_field(&fields[1],col_type, tmpsiz, 0,0,0,1);

		sprintf(tmpstr,"%d",col->col_size);
		tmpsiz = mdb_ascii2unicode(mdb, tmpstr, 0, col_size, 100);
		mdb_fill_temp_field(&fields[2],col_size, tmpsiz, 0,0,0,2);

		row_size = mdb_pack_row(ttable, row_buffer, 3, fields);
		mdb_add_row_to_pg(ttable,row_buffer, row_size);
		ttable->num_rows++;
     }

	/* the column and table names are no good now */
	//mdb_sql_reset(sql);
	sql->cur_table = ttable;
}

int mdb_sql_find_sargcol(MdbSargNode *node, gpointer data)
{
	MdbTableDef *table = data;
	unsigned int i;
	MdbColumn *col;

	if (!mdb_is_relational_op(node->op)) return 0;
	if (!node->parent) return 0;

	for (i=0;i<table->num_cols;i++) {
		col=g_ptr_array_index(table->columns,i);
		if (!strcasecmp(col->name, (char *)node->parent)) {
			node->col = col;
			break;
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

	sql_tab = g_ptr_array_index(sql->tables,0);

	table = mdb_read_table_by_name(mdb, sql_tab->name, MDB_TABLE);
	if (!table) {
		mdb_sql_error(sql, "%s is not a table in this database", sql_tab->name);
		/* the column and table names are no good now */
		mdb_sql_reset(sql);
		return;
	}
	mdb_read_columns(table);
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
			if (!strcasecmp(sqlcol->name, col->name)) {
				sqlcol->disp_size = mdb_col_disp_size(col);
				found=1;
				break;
			}
		}
		if (!found) {
			mdb_sql_error(sql, "Column %s not found",sqlcol->name);
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
}

void 
mdb_sql_bind_column(MdbSQL *sql, int colnum, void *varaddr, int *len_ptr)
{
	MdbSQLColumn *sqlcol;

	/* sql columns are traditionally 1 based, so decrement colnum */
	sqlcol = g_ptr_array_index(sql->columns,colnum - 1);
	mdb_bind_column_by_name(sql->cur_table, sqlcol->name, varaddr, len_ptr);
}
void 
mdb_sql_bind_all(MdbSQL *sql)
{
	unsigned int i;

	for (i=0;i<sql->num_columns;i++) {
		sql->bound_values[i] = g_malloc0(MDB_BIND_SIZE);
		mdb_sql_bind_column(sql, i+1, sql->bound_values[i], NULL);
	}
}
/*
 * mdb_sql_fetch_row is now just a wrapper around mdb_fetch_row.
 *   It is left here only for backward compatibility.
 */
int
mdb_sql_fetch_row(MdbSQL *sql, MdbTableDef *table)
{
	return mdb_fetch_row(table);
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
			print_value(sql->bound_values[j],sqlcol->disp_size,!j);
		}
		fprintf(stdout,"\n");
	}

	/* footer */
	for (j=0;j<sql->num_columns;j++) {
		sqlcol = g_ptr_array_index(sql->columns,j);
		print_break(sqlcol->disp_size, !j);
	}

	fprintf(stdout,"\n");
	/* clean up */
	for (j=0;j<sql->num_columns;j++) {
		g_free(sql->bound_values[j]);
	}

	/* the column and table names are no good now */
	mdb_sql_reset(sql);
}

