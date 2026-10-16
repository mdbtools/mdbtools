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
#include <ctype.h>
#include <limits.h>
#include "mdbsqlprivate.h"

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

	/* Errors have already been reported by mdb_sql_error() */
	if (mdb_sql_has_error(sql))
		return NULL;

	/* Statements such as CONNECT TO and DISCONNECT have no result set */
	if (sql->cur_table == NULL)
		return sql;

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
		g_free(c->col_name);
		g_free(c->qualifier);
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
		g_free(t->alias);
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

/* Parse a number written with a '.' decimal point, whatever the locale.
 * Leading and trailing blanks are allowed. Returns 1 on success. */
static int
mdb_sql_parse_number(const char *s, double *d)
{
	char buf[64], *end;
	const char *p;
	size_t len;

	while (isspace((unsigned char)*s))
		s++;
	len = strlen(s);
	while (len && isspace((unsigned char)s[len-1]))
		len--;
	if (!len || len >= sizeof(buf))
		return 0;
	for (p = s; p < s + len; p++)
		if (!isdigit((unsigned char)*p) && !strchr("+-.eE", *p))
			return 0;
	memcpy(buf, s, len);
	buf[len] = '\0';
	if ((end = strchr(buf, '.')))
		*end = localeconv()->decimal_point[0];
	*d = strtod(buf, &end);
	return end != buf && *end == '\0';
}

MdbSQLConst *
mdbsql_const_new(MdbSQLConstType type, const char *text, double d)
{
	MdbSQLConst *c = g_malloc0(sizeof(MdbSQLConst));
	c->type = type;
	c->text = text ? g_strdup(text) : NULL;
	c->d = d;
	return c;
}

/* Make a string constant from a quoted literal such as 'it''s' */
MdbSQLConst *
mdbsql_const_from_literal(const char *literal)
{
	MdbSQLConst *c = mdbsql_const_new(MDB_SQL_CONST_STRING, NULL, 0);
	size_t len = strlen(literal);
	char *d;
	const char *s;

	if (len >= 2 && literal[0] == '\'' && literal[len-1] == '\'') {
		literal++;
		len -= 2;
	}
	c->text = d = g_malloc(len + 1);
	for (s = literal; s < literal + len; s++) {
		*d++ = *s;
		if (*s == '\'' && s + 1 < literal + len && s[1] == '\'')
			s++;
	}
	*d = '\0';
	return c;
}

static int mdb_sql_parse_date(const char *s, int access_style, double *date);

/* Make a date constant from the text between the #s of a date literal */
MdbSQLConst *
mdbsql_const_date(MdbSQL *sql, const char *text)
{
	double date;

	if (!mdb_sql_parse_date(text, 1, &date)) {
		mdb_sql_error(sql, "Invalid date #%s#; use #MM/DD/YYYY [HH:MM:SS]# or #YYYY-MM-DD [HH:MM:SS]#", text);
		return NULL;
	}
	return mdbsql_const_new(MDB_SQL_CONST_DATE, NULL, date);
}

/* The value of the next ? parameter of the statement being parsed */
MdbSQLConst *
mdbsql_const_param(MdbSQL *sql)
{
	MdbSQLConst *c = NULL;
	unsigned int n = ++sql->num_params;

	if (sql->params && n <= sql->params->len)
		c = g_ptr_array_index(sql->params, n - 1);
	if (!c) {
		mdb_sql_error(sql, "No value given for parameter %u", n);
		return NULL;
	}
	return mdbsql_const_new(c->type, c->text, c->d);
}

void
mdb_sql_clear_params(MdbSQL *sql)
{
	unsigned int i;

	if (!sql->params)
		return;
	for (i=0; i<sql->params->len; i++) {
		mdbsql_const_free(g_ptr_array_index(sql->params, i));
		g_ptr_array_index(sql->params, i) = NULL;
	}
}

static int
mdb_sql_set_param(MdbSQL *sql, int n, MdbSQLConst *c)
{
	if (n < 1) {
		mdbsql_const_free(c);
		return -1;
	}
	if (!sql->params)
		sql->params = g_ptr_array_new();
	while (sql->params->len < (unsigned int)n)
		g_ptr_array_add(sql->params, NULL);
	mdbsql_const_free(g_ptr_array_index(sql->params, n - 1));
	g_ptr_array_index(sql->params, n - 1) = c;
	return 0;
}

int
mdb_sql_set_param_null(MdbSQL *sql, int n)
{
	return mdb_sql_set_param(sql, n, mdbsql_const_new(MDB_SQL_CONST_NULL, NULL, 0));
}

int
mdb_sql_set_param_string(MdbSQL *sql, int n, const char *value)
{
	return mdb_sql_set_param(sql, n, mdbsql_const_new(MDB_SQL_CONST_STRING, value, 0));
}

int
mdb_sql_set_param_int(MdbSQL *sql, int n, long long value)
{
	char buf[32];
	snprintf(buf, sizeof(buf), "%lld", value);
	return mdb_sql_set_param(sql, n, mdbsql_const_new(MDB_SQL_CONST_NUMBER, buf, 0));
}

int
mdb_sql_set_param_double(MdbSQL *sql, int n, double value)
{
	char buf[64], *p;
	char decimal_point = localeconv()->decimal_point[0];

	if (value != value || value - value != 0) /* NaN or infinite */
		return -1;
	snprintf(buf, sizeof(buf), "%.17g", value);
	if (decimal_point != '.' && (p = strchr(buf, decimal_point)))
		*p = '.';
	return mdb_sql_set_param(sql, n, mdbsql_const_new(MDB_SQL_CONST_NUMBER, buf, 0));
}

int
mdb_sql_set_param_date(MdbSQL *sql, int n, struct tm *value)
{
	double date;

	if (value->tm_mon < 0 || value->tm_mon > 11)
		return -1;
	mdb_tm_to_date(value, &date);
	return mdb_sql_set_param(sql, n, mdbsql_const_new(MDB_SQL_CONST_DATE, NULL, date));
}

void
mdbsql_const_free(MdbSQLConst *c)
{
	if (!c)
		return;
	g_free(c->text);
	g_free(c);
}

/* Old style constants, as passed to the public mdb_sql_* functions:
 * quoted strings, or numbers */
static MdbSQLConst *
mdb_sql_const_from_string(const char *constant)
{
	if (constant[0] == '\'')
		return mdbsql_const_from_literal(constant);
	return mdbsql_const_new(MDB_SQL_CONST_NUMBER, constant, 0);
}

/* Parses date format specifier into JET date representation (double) */
MdbSQLConst *
mdbsql_strptime(MdbSQL *sql, MdbSQLConst *data, MdbSQLConst *format)
{
#ifndef HAVE_STRPTIME
	mdb_sql_error(sql, "Your system doesn't support strptime.");
	return NULL;
#else
	struct tm tm={0};
	double date=0;

	if (data->type != MDB_SQL_CONST_STRING) {
		mdb_sql_error(sql, "First parameter of strptime (data) must be a string.");
		return NULL;
	}
	if (format->type != MDB_SQL_CONST_STRING) {
		mdb_sql_error(sql, "Second parameter of strptime (format) must be a string.");
		return NULL;
	}
	if (!strptime(data->text, format->text, &tm)) {
		mdb_sql_error(sql, "strptime('%s','%s') failed.", data->text, format->text);
		return NULL;
	}
	mdb_tm_to_date(&tm, &date);
	/* It seems that when just using a time offset without date in strptime, 
	 * we always get 1 as decimal part, een though it should be 0 for time */
	if (date < 2 && date > 1) date--;
	return mdbsql_const_new(MDB_SQL_CONST_DATE, NULL, date);
#endif
}

char *
mdb_sql_strptime(MdbSQL *sql, char *data, char *format)
{
	MdbSQLConst *c1 = mdb_sql_const_from_string(data);
	MdbSQLConst *c2 = mdb_sql_const_from_string(format);
	MdbSQLConst *date = mdbsql_strptime(sql, c1, c2);
	char *pszDate = NULL;

	mdbsql_const_free(c1);
	mdbsql_const_free(c2);
	if (!date) {
		mdb_sql_reset(sql);
		return NULL;
	}
	if ((pszDate=malloc(16))) {
		char cLocale=localeconv()->decimal_point[0], *p;
		snprintf(pszDate, 16, "%lf", date->d);
		if (cLocale!='.') for (p=pszDate; *p; p++) if (*p==cLocale) *p='.';
	}
	mdbsql_const_free(date);
	return pszDate;
}

/* Push a node for a comparison whose result is known: 1 (true), 0 (false)
 * or -1 (unknown, a comparison with NULL) */
static void
mdb_sql_push_const_node(MdbSQL *sql, int value)
{
	MdbSargNode *node = mdb_sql_alloc_node();
	node->op = MDB_EQUAL;
	node->col = NULL;
	node->value.i = value;
	mdb_sql_push_node(sql, node);
}

/* evaluate a expression involving 2 constants and add answer to the stack */
int
mdbsql_eval_expr(MdbSQL *sql, MdbSQLConst *c1, int op, MdbSQLConst *c2)
{
	double val1, val2;
	int value = 0, compar = 0;
	unsigned char illop = 0;

	if (c1->type == MDB_SQL_CONST_NULL || c2->type == MDB_SQL_CONST_NULL) {
		mdb_sql_push_const_node(sql, -1);
		return 0;
	}
	if (c1->type == MDB_SQL_CONST_STRING && c2->type == MDB_SQL_CONST_STRING) {
		value = strcoll(c1->text, c2->text);
		switch (op) {
			case MDB_EQUAL: compar = (value ? 0 : 1); break;
			case MDB_GT: compar = (value > 0); break;
			case MDB_GTEQ: compar = (value >= 0); break;
			case MDB_LT: compar = (value < 0); break;
			case MDB_LTEQ: compar = (value <= 0); break;
			case MDB_LIKE: compar = mdb_like_cmp(c1->text, c2->text); break;
			case MDB_ILIKE: compar = mdb_ilike_cmp(c1->text, c2->text); break;
			case MDB_NEQ: compar = (value ? 1 : 0); break;
			default: illop = 1;
		}
	} else {
		/* numbers and dates; a string must hold a number */
		if (c1->type == MDB_SQL_CONST_DATE)
			val1 = c1->d;
		else if (!mdb_sql_parse_number(c1->text, &val1))
			goto mismatch;
		if (c2->type == MDB_SQL_CONST_DATE)
			val2 = c2->d;
		else if (!mdb_sql_parse_number(c2->text, &val2))
			goto mismatch;
		switch (op) {
			case MDB_EQUAL: compar = (val1 == val2); break;
			case MDB_GT: compar = (val1 > val2); break;
			case MDB_GTEQ: compar = (val1 >= val2); break;
			case MDB_LT: compar = (val1 < val2); break;
			case MDB_LTEQ: compar = (val1 <= val2); break;
			case MDB_NEQ: compar = (val1 != val2); break;
			default: illop = 1;
		}
	}
	if (illop) {
		mdb_sql_error(sql, "Illegal operator used for comparison of literals.");
		return 1;
	}
	mdb_sql_push_const_node(sql, compar ? 1 : 0);
	return 0;

mismatch:
	mdb_sql_error(sql, "Comparison of strings and numbers not allowed.");
	return 1;
}

int 
mdb_sql_eval_expr(MdbSQL *sql, char *const1, int op, char *const2)
{
	MdbSQLConst *c1 = mdb_sql_const_from_string(const1);
	MdbSQLConst *c2 = mdb_sql_const_from_string(const2);
	int rc = mdbsql_eval_expr(sql, c1, op, c2);

	mdbsql_const_free(c1);
	mdbsql_const_free(c2);
	if (rc) {
		/* the column and table names are no good now */
		mdb_sql_reset(sql);
	}
	return rc;
}

/* Add a comparison of a column with a constant (NULL for IS [NOT] NULL)
 * to the sarg stack. The constant is converted to the type of the column
 * later, by mdb_sql_resolve_sargcol() */
int
mdbsql_add_sarg(MdbSQL *sql, const char *qualifier, const char *col_name, int op, MdbSQLConst *c)
{
	MdbSargNode *node;
	size_t name_len = strlen(col_name);
	char *stash;

	if (c && c->text && strlen(c->text) >= sizeof(node->value.s)) {
		mdb_sql_error(sql, "Constant '%.20s...' is too long", c->text);
		return 1;
	}
	node = mdb_sql_alloc_node();
	node->op = op;
	/* stash the column name until we finish with the grammar, followed
	 * by its qualifier: "name\0qualifier\0" */
	if (!qualifier)
		qualifier = "";
	stash = g_malloc(name_len + strlen(qualifier) + 2);
	memcpy(stash, col_name, name_len + 1);
	strcpy(stash + name_len + 1, qualifier);
	node->parent = stash;

	if (c) {
		switch (c->type) {
			case MDB_SQL_CONST_NULL:
				node->val_type = MDB_SQL_VAL_NULL;
				break;
			case MDB_SQL_CONST_STRING:
				node->val_type = MDB_TEXT;
				snprintf(node->value.s, sizeof(node->value.s), "%s", c->text);
				break;
			case MDB_SQL_CONST_NUMBER:
				node->val_type = MDB_SQL_VAL_NUMBER;
				snprintf(node->value.s, sizeof(node->value.s), "%s", c->text);
				break;
			case MDB_SQL_CONST_DATE:
				node->val_type = MDB_SQL_VAL_DATE;
				node->value.d = c->d;
				break;
		}
	}
	mdb_sql_push_node(sql, node);
	return 0;
}

void
mdbsql_const_list_free(GPtrArray *list)
{
	unsigned int i;

	if (!list)
		return;
	for (i=0; i<list->len; i++)
		mdbsql_const_free(g_ptr_array_index(list, i));
	g_ptr_array_free(list, TRUE);
}

/* col IN (c1, c2, ...) is added as col = c1 OR col = c2 OR ..., and
 * col NOT IN (...) as its negation */
int
mdbsql_add_in(MdbSQL *sql, const char *qualifier, const char *col_name, GPtrArray *list, int negate)
{
	unsigned int i;

	for (i=0; i<list->len; i++) {
		if (mdbsql_add_sarg(sql, qualifier, col_name, MDB_EQUAL, g_ptr_array_index(list, i)))
			return 1;
		if (i > 0)
			mdb_sql_add_or(sql);
	}
	if (negate)
		mdb_sql_add_not(sql);
	return 0;
}

/* col BETWEEN low AND high is added as col >= low AND col <= high, and
 * col NOT BETWEEN ... as its negation */
int
mdbsql_add_between(MdbSQL *sql, const char *qualifier, const char *col_name,
	MdbSQLConst *low, MdbSQLConst *high, int negate)
{
	if (mdbsql_add_sarg(sql, qualifier, col_name, MDB_GTEQ, low) ||
			mdbsql_add_sarg(sql, qualifier, col_name, MDB_LTEQ, high))
		return 1;
	mdb_sql_add_and(sql);
	if (negate)
		mdb_sql_add_not(sql);
	return 0;
}

int 
mdb_sql_add_sarg(MdbSQL *sql, char *col_name, int op, char *constant)
{
	MdbSQLConst *c = constant ? mdb_sql_const_from_string(constant) : NULL;
	int rc = mdbsql_add_sarg(sql, NULL, col_name, op, c);
	mdbsql_const_free(c);
	return rc;
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

/* Takes ownership of qualifier and name */
MdbSQLColRef *
mdbsql_colref_new(char *qualifier, char *name)
{
	MdbSQLColRef *ref = g_malloc0(sizeof(MdbSQLColRef));
	ref->qualifier = qualifier;
	ref->name = name;
	return ref;
}

void
mdbsql_colref_free(MdbSQLColRef *ref)
{
	if (!ref)
		return;
	free(ref->qualifier);
	free(ref->name);
	g_free(ref);
}

/* Add a column to the select list; name may be "*" for all the columns of
 * the table qualifier. The result column is called alias, if not NULL */
int
mdbsql_add_column(MdbSQL *sql, const char *qualifier, const char *name, const char *alias)
{
	MdbSQLColumn *c = g_malloc0(sizeof(MdbSQLColumn));
	c->name = g_strdup(alias ? alias : name);
	c->col_name = g_strdup(name);
	c->qualifier = qualifier ? g_strdup(qualifier) : NULL;
	g_ptr_array_add(sql->columns, c);
	sql->num_columns++;
	return 0;
}

int mdb_sql_add_column(MdbSQL *sql, char *column_name)
{
	return mdbsql_add_column(sql, NULL, column_name, NULL);
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
int
mdbsql_add_table(MdbSQL *sql, const char *name, const char *alias)
{
	MdbSQLTable *t = g_malloc0(sizeof(MdbSQLTable));
	t->name = g_strdup(name);
	t->alias = alias ? g_strdup(alias) : NULL;
	g_ptr_array_add(sql->tables, t);
	sql->num_tables++;
	return 0;
}
int mdb_sql_add_table(MdbSQL *sql, char *table_name)
{
	return mdbsql_add_table(sql, table_name, NULL);
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
	mdb_sql_clear_params(sql);
	if (sql->params)
		g_ptr_array_free(sql->params, TRUE);
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
	sqlcol->col = g_ptr_array_index(ttable->columns, ttable->num_cols - 1);

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

/* Parse a time, "HH:MM[:SS[.FFF]][ AM|PM]", which must be all of s */
static int
mdb_sql_parse_time(const char *s, struct tm *tm)
{
	int n = 0, fields;

	tm->tm_sec = 0;
	if (!isdigit((unsigned char)*s) ||
			(fields = sscanf(s, "%2d:%2d%n:%2d%n", &tm->tm_hour, &tm->tm_min, &n, &tm->tm_sec, &n)) < 2)
		return 0;
	s += n;
	/* fractions of a second (as in ODBC's {ts '...'}) are ignored */
	if (*s == '.' && fields == 3)
		s += 1 + strspn(s + 1, "0123456789");
	while (*s == ' ')
		s++;
	if (!g_ascii_strcasecmp(s, "AM") || !g_ascii_strcasecmp(s, "PM")) {
		if (tm->tm_hour < 1 || tm->tm_hour > 12)
			return 0;
		tm->tm_hour = tm->tm_hour % 12 + (toupper((unsigned char)*s) == 'P' ? 12 : 0);
		s += 2;
	}
	return !*s && tm->tm_hour >= 0 && tm->tm_hour <= 23 && tm->tm_min >= 0 &&
		tm->tm_min <= 59 && tm->tm_sec >= 0 && tm->tm_sec <= 59;
}

/* Parse a date into a JET date value. Accepted are ISO 8601 style dates,
 * "YYYY-MM-DD[ HH:MM[:SS]]" (a "T" may separate date and time), and, if
 * access_style is set, the forms of Access date literals as well:
 * "MM/DD/YYYY", "YYYY/MM/DD" and two digit years, followed by an optional
 * "HH:MM[:SS][ AM|PM]", or a time alone */
static int
mdb_sql_parse_date(const char *s, int access_style, double *date)
{
	static const int days[] = { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
	struct tm tm = { 0 };
	int a, b, c, n = 0, digits;

	if (access_style) {
		while (*s == ' ')
			s++;
		struct tm t = { 0 };
		if (mdb_sql_parse_time(s, &t)) {
			*date = (t.tm_hour * 3600 + t.tm_min * 60 + t.tm_sec) / 86400.0;
			return 1;
		}
	}
	if (!isdigit((unsigned char)*s))
		return 0;
	digits = strspn(s, "0123456789");
	if (sscanf(s, "%4d-%2d-%2d%n", &tm.tm_year, &tm.tm_mon, &tm.tm_mday, &n) == 3) {
		s += n;
		if (*s == ' ' || *s == 'T') {
			if (!mdb_sql_parse_time(s + 1, &tm))
				return 0;
			s += strlen(s);
		}
	} else if (access_style && sscanf(s, "%d/%d/%d%n", &a, &b, &c, &n) == 3) {
		if (digits > 2) {
			tm.tm_year = a; tm.tm_mon = b; tm.tm_mday = c;
		} else {
			const char *year = s;
			int i;
			for (i = 0; i < n; i++)
				if (s[i] == '/')
					year = s + i + 1;
			tm.tm_mon = a; tm.tm_mday = b; tm.tm_year = c;
			/* two digit years are 1930 to 2029, as in Access */
			if (n - (year - s) <= 2)
				tm.tm_year = c + (c < 30 ? 2000 : 1900);
		}
		s += n;
		if (*s == ' ') {
			while (*s == ' ')
				s++;
			if (*s && !mdb_sql_parse_time(s, &tm))
				return 0;
			s += strlen(s);
		}
	} else {
		return 0;
	}
	if (*s)
		return 0;
	if (tm.tm_year < 1 || tm.tm_year > 9999 || tm.tm_mon < 1 || tm.tm_mon > 12 ||
			tm.tm_mday < 1 || tm.tm_mday > days[tm.tm_mon - 1] ||
			(tm.tm_mon == 2 && tm.tm_mday == 29 && !((tm.tm_year % 4 == 0 &&
				tm.tm_year % 100 != 0) || tm.tm_year % 400 == 0)))
		return 0;
	tm.tm_year -= 1900;
	tm.tm_mon -= 1;
	mdb_tm_to_date(&tm, date);
	return 1;
}

typedef struct {
	MdbSQL *sql; /* for error reporting; may be NULL */
	MdbTableDef *table;
	int failed;
	MdbSQLTable *sql_tab; /* for checking qualifiers; may be NULL */
} MdbSQLSargColContext;

/* Check that a column qualifier names the table, or its alias */
static int
mdb_sql_check_qualifier(MdbSQL *sql, MdbSQLTable *sql_tab, const char *qualifier)
{
	if (!qualifier || !qualifier[0] || !sql_tab)
		return 0;
	if (!g_ascii_strcasecmp(qualifier, sql_tab->name) ||
			(sql_tab->alias && !g_ascii_strcasecmp(qualifier, sql_tab->alias)))
		return 0;
	if (sql)
		mdb_sql_error(sql, "Table %s is not in the FROM clause", qualifier);
	return 1;
}

/* Resolve the column name stashed in a relational node of the sarg tree,
 * and convert the constant to the type needed to compare with it */
static int
mdb_sql_resolve_sargcol(MdbSargNode *node, gpointer data)
{
	MdbSQLSargColContext *ctx = data;
	MdbColumn *col;
	double d;

	if (ctx->failed) return 1;
	if (!mdb_is_relational_op(node->op)) return 0;
	if (!node->parent) return 0;

	if (mdb_sql_check_qualifier(ctx->sql, ctx->sql_tab,
				(char *)node->parent + strlen(node->parent) + 1))
		goto fail;
	if (!(col = mdb_sql_find_colbyname(ctx->table, (char *)node->parent))) {
		if (ctx->sql)
			mdb_sql_error(ctx->sql, "Column %s not found", (char *)node->parent);
		goto fail;
	}
	node->col = col;
	if (node->op == MDB_ISNULL || node->op == MDB_NOTNULL)
		return 0;
	switch (col->col_type) {
		case MDB_BOOL: case MDB_BYTE: case MDB_INT: case MDB_LONGINT:
		case MDB_COMPLEX: case MDB_MONEY: case MDB_NUMERIC:
		case MDB_FLOAT: case MDB_DOUBLE: case MDB_DATETIME:
			if (node->op == MDB_LIKE || node->op == MDB_ILIKE) {
				if (ctx->sql)
					mdb_sql_error(ctx->sql, "Can't use LIKE with column %s of type %s",
							col->name, mdb_get_colbacktype_string(col));
				goto fail;
			}
			break;
		case MDB_TEXT: case MDB_MEMO: case MDB_REPID:
			break;
		default:
			if (ctx->sql)
				mdb_sql_error(ctx->sql, "Can't compare column %s of type %s",
						col->name, mdb_get_colbacktype_string(col));
			goto fail;
	}

	/* A comparison with NULL is unknown, whatever the column holds */
	if (node->val_type == MDB_SQL_VAL_NULL) {
		node->col = NULL;
		node->value.i = -1;
		return 0;
	}

	/* Convert the constant to the type needed to compare with the column */
	switch (col->col_type) {
	case MDB_TEXT: case MDB_MEMO: case MDB_REPID:
		if (node->val_type == MDB_SQL_VAL_NUMBER) {
			/* compare with the number as written */
			node->val_type = MDB_TEXT;
		} else if (node->val_type == MDB_SQL_VAL_DATE) {
			if (ctx->sql)
				mdb_sql_error(ctx->sql, "Can't compare text column %s with a date", col->name);
			goto fail;
		}
		break;
	case MDB_DATETIME:
		if (node->val_type == MDB_TEXT) {
			if (!mdb_sql_parse_date(node->value.s, 0, &d)) {
				if (ctx->sql)
					mdb_sql_error(ctx->sql, "Can't compare date column %s with '%s'; use a 'YYYY-MM-DD [HH:MM:SS]' date or strptime()",
							col->name, node->value.s);
				goto fail;
			}
			node->value.d = d;
			node->val_type = MDB_DOUBLE;
		} else if (node->val_type == MDB_SQL_VAL_DATE) {
			node->val_type = MDB_DOUBLE;
		} else if (node->val_type == MDB_SQL_VAL_NUMBER) {
			if (!mdb_sql_parse_number(node->value.s, &d)) {
				if (ctx->sql)
					mdb_sql_error(ctx->sql, "Invalid number %s", node->value.s);
				goto fail;
			}
			if (!strpbrk(node->value.s, ".eE") && d > -1e15 && d < 1e15) {
				/* Plain integers are UNIX timestamps for backwards
				 * compatibility of parser */
				time_t t = (time_t)d;
				struct tm *tmp;
#ifdef HAVE_GMTIME_R
				struct tm tm;
				tmp = gmtime_r(&t, &tm);
#else		// regular gmtime on Windows uses thread-local storage
				tmp = gmtime(&t);
#endif
				if (!tmp) {
					if (ctx->sql)
						mdb_sql_error(ctx->sql, "Invalid timestamp %s", node->value.s);
					goto fail;
				}
				mdb_tm_to_date(tmp, &d);
			}
			node->value.d = d;
			node->val_type = MDB_DOUBLE;
		}
		break;
	default:
		/* numeric columns: a string constant must hold a number */
		if (node->val_type == MDB_SQL_VAL_DATE) {
			if (ctx->sql)
				mdb_sql_error(ctx->sql, "Can't compare column %s of type %s with a date",
						col->name, mdb_get_colbacktype_string(col));
			goto fail;
		}
		if (node->val_type == MDB_TEXT || node->val_type == MDB_SQL_VAL_NUMBER) {
			if (!mdb_sql_parse_number(node->value.s, &d)) {
				if (ctx->sql) {
					if (node->val_type == MDB_TEXT)
						mdb_sql_error(ctx->sql, "Can't compare column %s of type %s with '%s'",
								col->name, mdb_get_colbacktype_string(col), node->value.s);
					else
						mdb_sql_error(ctx->sql, "Invalid number %s", node->value.s);
				}
				goto fail;
			}
			/* Yes/No columns hold 1 or 0; Access writes True as -1 */
			if (col->col_type == MDB_BOOL && (node->op == MDB_EQUAL || node->op == MDB_NEQ))
				d = (d != 0);
			if (d >= INT_MIN && d <= INT_MAX && d == (int)d) {
				node->value.i = (int)d;
				node->val_type = MDB_INT;
			} else {
				node->value.d = d;
				node->val_type = MDB_DOUBLE;
			}
		}
		break;
	}
	return 0;

fail:
	ctx->failed = 1;
	return 1;
}

int mdb_sql_find_sargcol(MdbSargNode *node, gpointer data)
{
	MdbSQLSargColContext ctx = { NULL, data, 0, NULL };
	mdb_sql_resolve_sargcol(node, &ctx);
	return 0;
}

static void
mdb_sql_count_result(MdbSQL *sql, int count)
{
	MdbHandle *mdb = sql->mdb;
	MdbTableDef *ttable = mdb_create_temp_table(mdb, "#count");
	char tmpstr[32];
	gchar row_cnt[32];
	unsigned char row_buffer[MDB_PGSIZE];
	MdbField fields[1];
	int row_size, tmpsiz;

	mdb_sql_add_temp_col(sql, ttable, 0, "count", MDB_TEXT, 30, 0);
	snprintf(tmpstr, sizeof(tmpstr), "%d", count);
	tmpsiz = mdb_ascii2unicode(mdb, tmpstr, 0, row_cnt, sizeof(row_cnt));
	mdb_fill_temp_field(&fields[0],row_cnt, tmpsiz, 0,0,0,0);
	row_size = mdb_pack_row(ttable, row_buffer, 1, fields);
	mdb_add_row_to_pg(ttable,row_buffer, row_size);
	ttable->num_rows++;
	sql->cur_table = ttable;
}

/* Replace each table.* in the select list with the columns of the table */
static void
mdb_sql_expand_stars(MdbSQL *sql, MdbTableDef *table)
{
	GPtrArray *columns;
	MdbSQLColumn *sqlcol;
	MdbColumn *col;
	unsigned int i, j;

	for (i=0;i<sql->num_columns;i++) {
		sqlcol = g_ptr_array_index(sql->columns,i);
		if (!strcmp(sqlcol->col_name, "*"))
			break;
	}
	if (i == sql->num_columns)
		return;

	columns = sql->columns;
	sql->columns = g_ptr_array_new();
	sql->num_columns = 0;
	for (i=0;i<columns->len;i++) {
		sqlcol = g_ptr_array_index(columns,i);
		if (strcmp(sqlcol->col_name, "*")) {
			g_ptr_array_add(sql->columns, sqlcol);
			sql->num_columns++;
			continue;
		}
		for (j=0;j<table->num_cols;j++) {
			col = g_ptr_array_index(table->columns,j);
			mdbsql_add_column(sql, NULL, col->name, NULL);
		}
		g_free(sqlcol->name);
		g_free(sqlcol->col_name);
		g_free(sqlcol->qualifier);
		g_free(sqlcol);
	}
	g_ptr_array_free(columns, TRUE);
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
		mdb_sql_count_result(sql, table->num_rows);
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
	/* check the qualifiers, and expand table.* */
	for (i=0;i<sql->num_columns;i++) {
		sqlcol = g_ptr_array_index(sql->columns,i);
		if (mdb_sql_check_qualifier(sql, sql_tab, sqlcol->qualifier)) {
			mdb_index_scan_free(table);
			mdb_free_tabledef(table);
			mdb_sql_reset(sql);
			return;
		}
	}
	mdb_sql_expand_stars(sql, table);
	/* verify all specified columns exist in this table */
	for (i=0;i<sql->num_columns;i++) {
		sqlcol = g_ptr_array_index(sql->columns,i);
		found=0;
		for (j=0;j<table->num_cols;j++) {
			col=g_ptr_array_index(table->columns,j);
			if (!g_ascii_strcasecmp(sqlcol->col_name, col->name)) {
				sqlcol->disp_size = mdb_col_disp_size(col);
				sqlcol->col = col;
				found=1;
				break;
			}
		}
		if (!found) {
			mdb_sql_error(sql, "Column %s not found",sqlcol->col_name);
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
		MdbSQLSargColContext ctx = { sql, table, 0, sql_tab };
		mdb_sql_walk_tree(sql->sarg_tree, mdb_sql_resolve_sargcol, &ctx);
		if (ctx.failed) {
			mdb_free_tabledef(table);
			mdb_sql_reset(sql);
			return;
		}
		mdb_sql_walk_tree(sql->sarg_tree, mdb_find_indexable_sargs, NULL);
	}
	/* 
	 * move the sarg_tree.  
	 * XXX - this won't work when we implement joins 
	 */
	table->sarg_tree = sql->sarg_tree;

	if (sql->sel_count) {
		/* COUNT(*) with a WHERE clause: count the matching rows. The sarg
		 * tree stays owned by sql and is freed by mdb_sql_reset() */
		int count = 0;
		mdb_index_scan_init(mdb, table);
		while (mdb_fetch_row(table))
			count++;
		mdb_index_scan_free(table);
		table->sarg_tree = NULL;
		mdb_free_tabledef(table);
		mdb_sql_count_result(sql, count);
		return;
	}
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
	int i;

	if (colnum <= 0 || colnum > sql->num_columns)
		return -1;

	/* sql columns are traditionally 1 based, so decrement colnum */
	sqlcol = g_ptr_array_index(sql->columns,colnum - 1);
	sqlcol->bind_addr = varaddr;
	sqlcol->bind_len = len_ptr;
	sqlcol->copy_of = NULL;
	if (!sqlcol->col)
		return mdb_bind_column_by_name(sql->cur_table, sqlcol->name, varaddr, len_ptr);

	/* A column can only be bound to one buffer: when it is selected more
	 * than once, the value is copied to the others after each fetch */
	for (i=0;i<colnum-1;i++) {
		MdbSQLColumn *other = g_ptr_array_index(sql->columns,i);
		if (other->col == sqlcol->col && other->bind_addr && !other->copy_of) {
			sqlcol->copy_of = other;
			return colnum;
		}
	}
	for (i=0;i<sql->cur_table->num_cols;i++) {
		if (g_ptr_array_index(sql->cur_table->columns,i) == sqlcol->col)
			return mdb_bind_column(sql->cur_table, i+1, varaddr, len_ptr);
	}
	return -1;
}

/* Copy the values of columns selected more than once, see
 * mdb_sql_bind_column() */
static void
mdb_sql_copy_bound_values(MdbSQL *sql)
{
	unsigned int i;
	size_t len;

	for (i=0;i<sql->num_columns;i++) {
		MdbSQLColumn *sqlcol = g_ptr_array_index(sql->columns,i);
		MdbSQLColumn *src = sqlcol->copy_of;
		if (!src || !sqlcol->bind_addr)
			continue;
		len = strlen(src->bind_addr) + 1;
		if (len > sql->mdb->bind_size)
			len = sql->mdb->bind_size;
		memcpy(sqlcol->bind_addr, src->bind_addr, len);
		if (sqlcol->bind_len && src->bind_len)
			*sqlcol->bind_len = *src->bind_len;
	}
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
		mdb_sql_copy_bound_values(sql);
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
		mdb_sql_copy_bound_values(sql);
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
