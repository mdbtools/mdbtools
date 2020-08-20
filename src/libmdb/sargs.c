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

/*
 * code for handling searchable arguments (sargs) used primary by the sql 
 * engine to support where clause handling.  The sargs are configured in 
 * a tree with AND/OR operators connecting the child nodes. NOT operations
 * have only one child on the left side.  Logical operators (=,<,>,etc..)
 * have no children.
 *
 * datatype support is a bit weak at this point.  To add more types create
 * a mdb_test_[type]() function and invoke it from mdb_test_sarg()
 */

#include <time.h>
#include "mdbtools.h"

void
mdb_sql_walk_tree(MdbSargNode *node, MdbSargTreeFunc func, gpointer data)
{
	if (func(node, data))
		return;
	if (node->left) mdb_sql_walk_tree(node->left, func, data);
	if (node->right) mdb_sql_walk_tree(node->right, func, data);
}
int 
mdb_test_string(MdbSargNode *node, char *s)
{
int rc;

	if (node->op == MDB_LIKE) {
		return mdb_like_cmp(s,node->value.s);
	}
	rc = strncmp(node->value.s, s, 255);
	switch (node->op) {
		case MDB_EQUAL:
			if (rc==0) return 1;
			break;
		case MDB_GT:
			if (rc<0) return 1;
			break;
		case MDB_LT:
			if (rc>0) return 1;
			break;
		case MDB_GTEQ:
			if (rc<=0) return 1;
			break;
		case MDB_LTEQ:
			if (rc>=0) return 1;
			break;
		default:
			fprintf(stderr, "Calling mdb_test_sarg on unknown operator.  Add code to mdb_test_string() for operator %d\n",node->op);
			break;
	}
	return 0;
}
int mdb_test_int(MdbSargNode *node, gint32 i)
{
	switch (node->op) {
		case MDB_EQUAL:
			//fprintf(stderr, "comparing %ld and %ld\n", i, node->value.i);
			if (node->value.i == i) return 1;
			break;
		case MDB_GT:
			if (node->value.i < i) return 1;
			break;
		case MDB_LT:
			if (node->value.i > i) return 1;
			break;
		case MDB_GTEQ:
			if (node->value.i <= i) return 1;
			break;
		case MDB_LTEQ:
			if (node->value.i >= i) return 1;
			break;
		default:
			fprintf(stderr, "Calling mdb_test_sarg on unknown operator.  Add code to mdb_test_int() for operator %d\n",node->op);
			break;
	}
	return 0;
}

/* Actually used to not rely on libm. 
 * Maybe there is a cleaner and faster solution? */
static double poor_mans_trunc(double x)
{
	char buf[16];
	sprintf(buf, "%.6f", x);
	sscanf(buf, "%lf", &x);
	return x;
}

int mdb_test_double(int op, double vd, double d)
{
	switch (op) {
		case MDB_EQUAL:
			//fprintf(stderr, "comparing %lf and %lf\n", d, node->value.d);
			if (vd == d) return 1;
			break;
		case MDB_GT:
			if (vd < d) return 1;
			break;
		case MDB_LT:
			if (vd > d) return 1;
			break;
		case MDB_GTEQ:
			if (vd <= d) return 1;
			break;
		case MDB_LTEQ:
			if (vd >= d) return 1;
			break;
		default:
			fprintf(stderr, "Calling mdb_test_sarg on unknown operator.  Add code to mdb_test_double() for operator %d\n",op);
			break;
	}
	return 0;
}

#if 0 // Obsolete
int
mdb_test_date(MdbSargNode *node, double td)
{
	struct tm found;
	/* TODO: you should figure out a way to pull mdb_date_to_string in here
	 * char date_tmp[MDB_BIND_SIZE];
	 */

	time_t found_t;
	time_t asked_t;

	double diff;

	mdb_date_to_tm(td, &found);

	asked_t = node->value.i;
	found_t = mktime(&found);

	diff = difftime(asked_t, found_t);

	switch (node->op) {
	case MDB_EQUAL:
		if (diff==0) return 1;
		break;
	case MDB_GT:
		if (diff<0) return 1;
		break;
	case MDB_LT:
		if (diff>0) return 1;
		break;
	case MDB_GTEQ:
		if (diff<=0) return 1;
		break;
	case MDB_LTEQ:
		if (diff>=0) return 1;
		break;
	default:
		fprintf(stderr, "Calling mdb_test_sarg on unknown operator. Add code to mdb_test_date() for operator %d\n", node->op);
		break;
	}
	return 0;
}
#endif


int
mdb_find_indexable_sargs(MdbSargNode *node, gpointer data)
{
	MdbSarg sarg;

	if (node->op == MDB_OR || node->op == MDB_NOT) return 1;

	/* 
	 * right now all we do is look for sargs that are anded together from
	 * the root.  Later we may put together OR ops into a range, and then 
	 * range scan the leaf pages. That is col1 = 2 or col1 = 4 becomes
	 * col1 >= 2 and col1 <= 4 for the purpose of index scans, and then
	 * extra rows are thrown out when the row is tested against the main
	 * sarg tree.  range scans are generally only a bit better than table
	 * scanning anyway.
	 *
	 * also, later we should support the NOT operator, but it's generally
	 * a pretty worthless test for indexes, ie NOT col1 = 3, we are 
	 * probably better off table scanning.
	 */
	if (mdb_is_relational_op(node->op) && node->col) {
		//printf("op = %d value = %s\n", node->op, node->value.s);
		sarg.op = node->op;
		sarg.value = node->value;
		mdb_add_sarg(node->col, &sarg);
	}
	return 0;
}
int 
mdb_test_sarg(MdbHandle *mdb, MdbColumn *col, MdbSargNode *node, MdbField *field)
{
	char tmpbuf[256];
	char* val;
	int ret;

	if (node->op == MDB_ISNULL)
		return field->is_null?1:0;
	else if (node->op == MDB_NOTNULL)
		return field->is_null?0:1;
	switch (col->col_type) {
		case MDB_BOOL:
			return mdb_test_int(node, !field->is_null);
			break;
		case MDB_BYTE:
			return mdb_test_int(node, (gint32)((char *)field->value)[0]);
			break;
		case MDB_INT:
			return mdb_test_int(node, (gint32)mdb_get_int16(field->value, 0));
			break;
		case MDB_LONGINT:
			return mdb_test_int(node, (gint32)mdb_get_int32(field->value, 0));
			break;
		case MDB_TEXT:
			mdb_unicode2ascii(mdb, field->value, field->siz, tmpbuf, 256);
			return mdb_test_string(node, tmpbuf);
		case MDB_MEMO:
			val = mdb_col_to_string(mdb, mdb->pg_buf, field->start, col->col_type, (gint32)mdb_get_int32(field->value, 0));
			//printf("%s\n",val);
			ret = mdb_test_string(node, val);
			g_free(val);
			return ret;
			break;
		case MDB_DATETIME:
			return mdb_test_double(node->op, poor_mans_trunc(node->value.d), poor_mans_trunc(mdb_get_double(field->value, 0)));
		default:
			fprintf(stderr, "Calling mdb_test_sarg on unknown type.  Add code to mdb_test_sarg() for type %d\n",col->col_type);
			break;
	}
	return 1;
}
int
mdb_find_field(int col_num, MdbField *fields, int num_fields)
{
	int i;

	for (i=0;i<num_fields;i++) {
		if (fields[i].colnum == col_num) return i;
	}
	return -1;
}
int
mdb_test_sarg_node(MdbHandle *mdb, MdbSargNode *node, MdbField *fields, int num_fields)
{
	int elem;
	MdbColumn *col;
	int rc;

	if (mdb_is_relational_op(node->op)) {
		col = node->col;
		/* for const = const expressions */
		if (!col) {
			return (node->value.i);
		}
		elem = mdb_find_field(col->col_num, fields, num_fields);
		if (!mdb_test_sarg(mdb, col, node, &fields[elem])) 
			return 0;
	} else { /* logical op */
		switch (node->op) {
		case MDB_NOT:
			rc = mdb_test_sarg_node(mdb, node->left, fields, num_fields);
			return !rc;
			break;
		case MDB_AND:
			if (!mdb_test_sarg_node(mdb, node->left, fields, num_fields))
				return 0;
			return mdb_test_sarg_node(mdb, node->right, fields, num_fields);
			break;
		case MDB_OR:
			if (mdb_test_sarg_node(mdb, node->left, fields, num_fields))
				return 1;
			return mdb_test_sarg_node(mdb, node->right, fields, num_fields);
			break;
		}
	}
	return 1;
}
int 
mdb_test_sargs(MdbTableDef *table, MdbField *fields, int num_fields)
{
	MdbSargNode *node;
	MdbCatalogEntry *entry = table->entry;
	MdbHandle *mdb = entry->mdb;

	node = table->sarg_tree;

	/* there may not be a sarg tree */
	if (!node) return 1;

	return mdb_test_sarg_node(mdb, node, fields, num_fields);
}
#if 0
int mdb_test_sargs(MdbHandle *mdb, MdbColumn *col, int offset, int len)
{
MdbSarg *sarg;
int i;

	for (i=0;i<col->num_sargs;i++) {
		sarg = g_ptr_array_index (col->sargs, i);
		if (!mdb_test_sarg(mdb, col, sarg, offset, len)) {
			/* sarg didn't match, no sense going on */
			return 0;	
		}
	}

	return 1;
}
#endif
int mdb_add_sarg(MdbColumn *col, MdbSarg *in_sarg)
{
MdbSarg *sarg;
        if (!col->sargs) {
		col->sargs = g_ptr_array_new();
	}
	sarg = g_memdup(in_sarg,sizeof(MdbSarg));
        g_ptr_array_add(col->sargs, sarg);
	col->num_sargs++;

	return 1;
}
int mdb_add_sarg_by_name(MdbTableDef *table, char *colname, MdbSarg *in_sarg)
{
	MdbColumn *col;
	unsigned int i;

	for (i=0;i<table->num_cols;i++) {
		col = g_ptr_array_index (table->columns, i);
		if (!g_ascii_strcasecmp(col->name,colname)) {
			return mdb_add_sarg(col, in_sarg);
		}
	}
	/* else didn't find the column return 0! */
	return 0;
}
