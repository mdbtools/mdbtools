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
mdb_sql_error(char *fmt, ...)
{
va_list ap;

	va_start(ap, fmt);
	vfprintf (stderr,fmt, ap);
	va_end(ap);
	fprintf(stderr,"\n");
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

int _parse(MdbSQL *sql, char *buf)
{
	g_input_ptr = buf;
	/* begin unsafe */
	_mdb_sql(sql);
	if (yyparse()) {
		/* end unsafe */
		mdb_sql_reset(sql);
		return 0;
	} else {
		return 1;
	}
}
int mdb_run_query(MdbSQL *sql, char *query)
{
	if (_parse(sql,query) && sql->cur_table) {
		mdb_sql_bind_all(sql);
		return 1;
	} else {
		return 0;
	}
}
void mdb_sql_set_maxrow(MdbSQL *sql, int maxrow)
{
	sql->max_rows = maxrow;
}
void mdb_sql_free_column(MdbSQLColumn *c)
{
	if (c->name) g_free(c->name);
	g_free(c);
	c = NULL;
}
MdbSQLColumn *mdb_sql_alloc_column()
{
MdbSQLColumn *c;

	c = (MdbSQLColumn *) g_malloc(sizeof(MdbSQLColumn));
	memset(c,0,sizeof(MdbSQLColumn));
	return c;
}
void mdb_sql_free_table(MdbSQLTable *t)
{
	if (t->name) g_free(t->name);
	g_free(t);
	t = NULL;
}
MdbSQLTable *mdb_sql_alloc_table()
{
MdbSQLTable *t;

	t = (MdbSQLTable *) g_malloc(sizeof(MdbSQLTable));
	memset(t,0,sizeof(MdbSQLTable));
	return t;
}

void
mdb_sql_close(MdbSQL *sql)
{
	if (sql->mdb) {
		mdb_close(sql->mdb);
		sql->mdb = NULL;
	} else {
		mdb_sql_error("Not connected.");
	}
}

MdbHandle *mdb_sql_open(MdbSQL *sql, char *db_name)
{
int fail = 0;
char *db_namep = db_name;

#ifdef HAVE_WORDEXP
wordexp_t words;

	if (wordexp(db_name, &words, 0)==0) {
		if (words.we_wordc>0) 
			db_namep = words.we_wordv[0];
	}
	
#endif

	if (!(sql->mdb = mdb_open(db_namep, MDB_NOFLAGS))) {
		if (!strstr(db_namep, ".mdb")) {
			char *tmpstr = (char *) malloc(strlen(db_namep)+5);
			strcpy(tmpstr,db_namep);
			strcat(tmpstr,".mdb");
			if (!(sql->mdb = mdb_open(tmpstr, MDB_NOFLAGS))) {
				fail++;
			}
			free(tmpstr);
		} else {
			fail++;
		}
	}
	if (fail) {
		mdb_sql_error("Unable to locate database %s", db_name);
	}

#ifdef HAVE_WORDEXP
	wordfree(&words);
#endif	

	return sql->mdb;
}
MdbSargNode *
mdb_sql_alloc_node()
{
	MdbSargNode *node;

	node = g_malloc0(sizeof(MdbSargNode));

	return node;
}
void
mdb_sql_free_tree(MdbSargNode *tree)
{

	if (tree->left) mdb_sql_free_tree(tree->left);
	if (tree->right) mdb_sql_free_tree(tree->right);
	g_free(tree);
	tree = NULL;
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
		mdb_sql_error("parse error near 'NOT'");
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
		mdb_sql_error("parse error near 'OR'");
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
		mdb_sql_error("parse error near 'AND'");
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
		mdb_sql_error("Comparison of strings and numbers not allowed.");
		/* the column and table names are no good now */
		mdb_sql_reset(sql);
		return 1;
	}
	if (illop) {
		mdb_sql_error("Illegal operator used for comparision of literals.");
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

	c = mdb_sql_alloc_column();
	c->name = g_strdup(column_name);
	g_ptr_array_add(sql->columns, c);
	sql->num_columns++;
	return 0;
}
int mdb_sql_add_table(MdbSQL *sql, char *table_name)
{
MdbSQLTable *t;

	t = mdb_sql_alloc_table();
	t->name = g_strdup(table_name);
	t->alias = NULL;
	g_ptr_array_add(sql->tables, t);
	sql->num_tables++;
	return 0;
}
void mdb_sql_dump(MdbSQL *sql)
{
int i;
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
int i;
MdbSQLColumn *c;
MdbSQLTable *t;

	for (i=0;i<sql->num_columns;i++) {
		c = g_ptr_array_index(sql->columns,i);
		if (c->name) g_free(c->name);
	}
	for (i=0;i<sql->num_tables;i++) {
		t = g_ptr_array_index(sql->tables,i);
		if (t->name) g_free(t->name);
	}
	if (sql->sarg_tree) {
		mdb_sql_free_tree(sql->sarg_tree);
	}
	g_list_free(sql->sarg_stack);
	sql->sarg_stack = NULL;
	g_ptr_array_free(sql->columns,TRUE);
	g_ptr_array_free(sql->tables,TRUE);
	if (sql->mdb) {
		mdb_close(sql->mdb);	
	}
}
void mdb_sql_reset(MdbSQL *sql)
{
int i;
MdbSQLColumn *c;
MdbSQLTable *t;

	if (sql->cur_table) {
		mdb_index_scan_free(sql->cur_table);
		mdb_free_tabledef(sql->cur_table);
	}
	if (sql->kludge_ttable_pg) {
		g_free(sql->kludge_ttable_pg);
		sql->kludge_ttable_pg = NULL;
	}
	for (i=0;i<sql->num_columns;i++) {
		c = g_ptr_array_index(sql->columns,i);
		mdb_sql_free_column(c);
	}
	for (i=0;i<sql->num_tables;i++) {
		t = g_ptr_array_index(sql->tables,i);
		mdb_sql_free_table(t);
	}
	if (sql->sarg_tree) {
		mdb_sql_free_tree(sql->sarg_tree);
	}
	g_list_free(sql->sarg_stack);
	sql->sarg_stack = NULL;
	g_ptr_array_free(sql->columns,TRUE);
	g_ptr_array_free(sql->tables,TRUE);

	sql->all_columns = 0;
	sql->num_columns = 0;
	sql->columns = g_ptr_array_new();
	sql->num_tables = 0;
	sql->tables = g_ptr_array_new();
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
	int i;
	MdbCatalogEntry *entry;
	MdbHandle *mdb = sql->mdb;
	MdbField fields[1];
	unsigned char row_buffer[MDB_PGSIZE];
	unsigned char *new_pg;
	int row_size;
	MdbTableDef *ttable;
	gchar tmpstr[100];
	int tmpsiz;

	if (!mdb) {
		mdb_sql_error("You must connect to a database first");
		return;
	}
	mdb_read_catalog (mdb, MDB_TABLE);

	ttable = mdb_create_temp_table(mdb, "#listtables");
	mdb_sql_add_temp_col(sql, ttable, 0, "Tables", MDB_TEXT, 30, 0);

	/* blank out the pg_buf */
	new_pg = mdb_new_data_pg(ttable->entry);
	memcpy(mdb->pg_buf, new_pg, mdb->fmt->pg_size);
	g_free(new_pg);

 	/* loop over each entry in the catalog */
 	for (i=0; i < mdb->num_catalog; i++) {
     		entry = g_ptr_array_index (mdb->catalog, i);
     		/* if it's a table */
     		if (entry->object_type == MDB_TABLE) {
       			if (strncmp (entry->object_name, "MSys", 4)) {
          			//col = g_ptr_array_index(table->columns,0);
				tmpsiz = mdb_ascii2unicode(mdb, entry->object_name, 0, 100, tmpstr);
				mdb_fill_temp_field(&fields[0],tmpstr, tmpsiz, 0,0,0,0);
				row_size = mdb_pack_row(ttable, row_buffer, 1, fields);
				mdb_add_row_to_pg(ttable,row_buffer, row_size);
				ttable->num_rows++;
			}
		}
	}
	sql->kludge_ttable_pg = g_memdup(mdb->pg_buf, mdb->fmt->pg_size);
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
	MdbCatalogEntry *entry;
	MdbHandle *mdb = sql->mdb;
	MdbColumn *col;
	int i;
	MdbField fields[3];
	char tmpstr[256];
	unsigned char row_buffer[MDB_PGSIZE];
	unsigned char *new_pg;
	int row_size;
	gchar col_name[100], col_type[100], col_size[100];
	int tmpsiz;

	if (!mdb) {
		mdb_sql_error("You must connect to a database first");
		return;
	}

	sql_tab = g_ptr_array_index(sql->tables,0);

	mdb_read_catalog(mdb, MDB_TABLE);

	for (i=0;i<mdb->num_catalog;i++) {
		entry = g_ptr_array_index(mdb->catalog,i);
		if (entry->object_type == MDB_TABLE &&
			!strcasecmp(entry->object_name,sql_tab->name)) {
				table = mdb_read_table(entry);
				break;
		}
	}
	if (!table) {
		mdb_sql_error("%s is not a table in this database", sql_tab->name);
		/* the column and table names are no good now */
		mdb_sql_reset(sql);
		return;
	}

	mdb_read_columns(table);

	ttable = mdb_create_temp_table(mdb, "#describe");

	mdb_sql_add_temp_col(sql, ttable, 0, "Column Name", MDB_TEXT, 30, 0);
	mdb_sql_add_temp_col(sql, ttable, 1, "Type", MDB_TEXT, 20, 0);
	mdb_sql_add_temp_col(sql, ttable, 2, "Size", MDB_TEXT, 10, 0);

	/* blank out the pg_buf */
	new_pg = mdb_new_data_pg(ttable->entry);
	memcpy(mdb->pg_buf, new_pg, mdb->fmt->pg_size);
	g_free(new_pg);

     for (i=0;i<table->num_cols;i++) {

        col = g_ptr_array_index(table->columns,i);
		tmpsiz = mdb_ascii2unicode(mdb, col->name, 0, 100, col_name);
		mdb_fill_temp_field(&fields[0],col_name, tmpsiz, 0,0,0,0);

		strcpy(tmpstr, mdb_get_coltype_string(mdb->default_backend, col->col_type));
		tmpsiz = mdb_ascii2unicode(mdb, tmpstr, 0, 100, col_type);
		mdb_fill_temp_field(&fields[1],col_type, tmpsiz, 0,0,0,1);

		sprintf(tmpstr,"%d",col->col_size);
		tmpsiz = mdb_ascii2unicode(mdb, tmpstr, 0, 100, col_size);
		mdb_fill_temp_field(&fields[2],col_size, tmpsiz, 0,0,0,2);

		row_size = mdb_pack_row(ttable, row_buffer, 3, fields);
		mdb_add_row_to_pg(ttable,row_buffer, row_size);
		ttable->num_rows++;
     }

	/* the column and table names are no good now */
	//mdb_sql_reset(sql);
	sql->kludge_ttable_pg = g_memdup(mdb->pg_buf, mdb->fmt->pg_size);
	sql->cur_table = ttable;
}

int mdb_sql_find_sargcol(MdbSargNode *node, gpointer data)
{
	MdbTableDef *table = data;
	int i;
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
int i,j;
MdbCatalogEntry *entry;
MdbHandle *mdb = sql->mdb;
MdbTableDef *table = NULL;
MdbSQLTable *sql_tab;
MdbColumn *col;
MdbSQLColumn *sqlcol;
int found = 0;

	if (!mdb) {
		mdb_sql_error("You must connect to a database first");
		return;
	}

	sql_tab = g_ptr_array_index(sql->tables,0);

	mdb_read_catalog(mdb, MDB_TABLE);

	for (i=0;i<mdb->num_catalog;i++) {
		entry = g_ptr_array_index(mdb->catalog,i);
		if (entry->object_type == MDB_TABLE &&
			!strcasecmp(entry->object_name,sql_tab->name)) {
				table = mdb_read_table(entry);
				break;
		}
	}
	if (!table) {
		mdb_sql_error("%s is not a table in this database", sql_tab->name);
		/* the column and table names are no good now */
		mdb_sql_reset(sql);
		return;
	}
	mdb_read_columns(table);
	mdb_read_indices(table);
	mdb_rewind_table(table);

	if (sql->all_columns) {
		for (i=0;i<table->num_cols;i++) {
			col=g_ptr_array_index(table->columns,i);
			sqlcol = mdb_sql_alloc_column();
			sqlcol->name = g_strdup(col->name);
			g_ptr_array_add(sql->columns, sqlcol);
			sql->num_columns++;
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
			mdb_sql_error("Column %s not found",sqlcol->name);
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
mdb_sql_bind_column(MdbSQL *sql, int colnum, char *varaddr)
{
MdbTableDef *table = sql->cur_table;
MdbSQLColumn *sqlcol;
MdbColumn *col;
int j;

	/* sql columns are traditionally 1 based, so decrement colnum */
	sqlcol = g_ptr_array_index(sql->columns,colnum - 1);
	for (j=0;j<table->num_cols;j++) {
		col=g_ptr_array_index(table->columns,j);
		if (!strcasecmp(sqlcol->name, col->name)) {
			/* bind the column to its listed (SQL) position */
			mdb_bind_column(table, j+1, varaddr);
			break;
		}
	}
}
void 
mdb_sql_bind_len(MdbSQL *sql, int colnum, int *len_ptr)
{
MdbTableDef *table = sql->cur_table;
MdbSQLColumn *sqlcol;
MdbColumn *col;
int j;

	/* sql columns are traditionally 1 based, so decrement colnum */
	sqlcol = g_ptr_array_index(sql->columns,colnum - 1);
	for (j=0;j<table->num_cols;j++) {
		col=g_ptr_array_index(table->columns,j);
		if (!strcasecmp(sqlcol->name, col->name)) {
			/* bind the column length to its listed (SQL) position */
			mdb_bind_len(table, j+1, len_ptr);
			break;
		}
	}
}
void 
mdb_sql_bind_all(MdbSQL *sql)
{
int i;

	for (i=0;i<sql->num_columns;i++) {
		sql->bound_values[i] = (char *) malloc(MDB_BIND_SIZE);
		sql->bound_values[i][0] = '\0';
		mdb_sql_bind_column(sql, i+1, sql->bound_values[i]);
	}
}
/*
 * mdb_sql_fetch_row should be called instead of mdb_fetch_row when there may be 
 * work tables involved (currently implemented as kludge_ttable_pg)
 */
int
mdb_sql_fetch_row(MdbSQL *sql, MdbTableDef *table)
{
	MdbHandle *mdb = table->entry->mdb;
	MdbFormatConstants *fmt = mdb->fmt;
	unsigned int rows;

        if (sql->kludge_ttable_pg) {
                memcpy(mdb->pg_buf, sql->kludge_ttable_pg, fmt->pg_size);

		rows = mdb_pg_get_int16(mdb,fmt->row_count_offset);
		if (rows > table->cur_row) {
			mdb_read_row(sql->cur_table, table->cur_row++);
			return 1;
		}
		return 0;
	} else {
		return mdb_fetch_row(table);
	}
}

void 
mdb_sql_dump_results(MdbSQL *sql)
{
int j;
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
		if (sql->bound_values[j]) free(sql->bound_values[j]);
	}

	/* the column and table names are no good now */
	mdb_sql_reset(sql);
}

