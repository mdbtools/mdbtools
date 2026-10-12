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
#include "mdbprivate.h"

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
	if (node->op == MDB_ILIKE) {
		return mdb_ilike_cmp(s,node->value.s);
	}
	rc = strcoll(node->value.s, s);
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
		case MDB_NEQ:
			if (rc!=0) return 1;
			break;
		default:
			fprintf(stderr, "Calling mdb_test_sarg on unknown operator.  Add code to mdb_test_string() for operator %d\n",node->op);
			break;
	}
	return 0;
}
int mdb_test_int(MdbSargNode *node, gint32 i)
{
	gint32 val = node->val_type == MDB_INT ? node->value.i : node->value.d;
	switch (node->op) {
		case MDB_EQUAL:
			//fprintf(stderr, "comparing %ld and %ld\n", i, node->value.i);
			if (val == i) return 1;
			break;
		case MDB_GT:
			if (val < i) return 1;
			break;
		case MDB_LT:
			if (val > i) return 1;
			break;
		case MDB_GTEQ:
			if (val <= i) return 1;
			break;
		case MDB_LTEQ:
			if (val >= i) return 1;
			break;
		case MDB_NEQ:
			if (val != i) return 1;
			break;
		default:
			fprintf(stderr, "Calling mdb_test_sarg on unknown operator.  Add code to mdb_test_int() for operator %d\n",node->op);
			break;
	}
	return 0;
}

/* The constant of a numeric comparison, as a double */
static double
mdb_sarg_node_double(MdbSargNode *node)
{
	return node->val_type == MDB_INT ? node->value.i : node->value.d;
}

/* Decode a NUMERIC (DECIMAL) value: a sign byte followed by a 128 bit
 * unsigned integer stored as four little endian 32 bit words, most
 * significant word first. Note that col_prec holds the scale. */
static double
mdb_numeric_to_double(const unsigned char *buf, int scale)
{
	double d = 0;
	int i;

	for (i=0; i<4; i++)
		d = d * 4294967296.0 + (guint32)mdb_get_int32((void *)buf, 1 + 4*i);
	for (i=0; i<scale; i++)
		d /= 10;
	return (buf[0] & 0x80) ? -d : d;
}

/* Actually used to not rely on libm. 
 * Maybe there is a cleaner and faster solution? */
static double poor_mans_trunc(double x)
{
	char buf[16];
	snprintf(buf, sizeof(buf), "%.6f", x);
	sscanf(buf, "%lf", &x);
	return x;
}

int mdb_test_double(int op, double vd, double d)
{
    int ret = 0;
	switch (op) {
		case MDB_EQUAL:
			//fprintf(stderr, "comparing %lf and %lf\n", d, node->value.d);
			ret = (vd == d);
            break;
		case MDB_GT:
			ret = (vd < d);
			break;
		case MDB_LT:
			ret = (vd > d);
			break;
		case MDB_GTEQ:
			ret = (vd <= d);
			break;
		case MDB_LTEQ:
			ret = (vd >= d);
			break;
		case MDB_NEQ:
			ret = (vd != d);
			break;
		default:
			fprintf(stderr, "Calling mdb_test_sarg on unknown operator.  Add code to mdb_test_double() for operator %d\n",op);
			break;
	}
	return ret;
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
	/* index entries can't be used to test for NULL */
	if (node->op == MDB_ISNULL || node->op == MDB_NOTNULL) return 0;

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
		/* the index code only handles integer constants for these */
		if (node->col->col_type == MDB_LONGINT && node->val_type != MDB_INT)
			return 0;
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
	/* up to 255 characters, each up to 3 bytes in UTF-8 */
	char tmpbuf[1024];
	char* val;
	int ret = 1;
	/* Boolean values are stored in the null bitmap and are never NULL */
	int is_null = (col->col_type != MDB_BOOL && field->is_null);

	if (node->op == MDB_ISNULL)
		return is_null;
	if (node->op == MDB_NOTNULL)
		return !is_null;
	/* A comparison with NULL is never true */
	if (is_null)
		return 0;
	switch (col->col_type) {
		case MDB_BOOL:
			ret = mdb_test_int(node, !field->is_null);
			break;
		case MDB_BYTE:
			ret = mdb_test_double(node->op, mdb_sarg_node_double(node), ((unsigned char *)field->value)[0]);
			break;
		case MDB_INT:
			ret = mdb_test_double(node->op, mdb_sarg_node_double(node), mdb_get_int16(field->value, 0));
			break;
		case MDB_LONGINT:
		case MDB_COMPLEX:
			ret = mdb_test_double(node->op, mdb_sarg_node_double(node), (gint32)mdb_get_int32(field->value, 0));
			break;
		case MDB_MONEY:
			/* a 64 bit integer scaled by 10000 */
			if (field->siz < 8)
				return 0;
			ret = mdb_test_double(node->op, mdb_sarg_node_double(node),
				(long long)((unsigned long long)(guint32)mdb_get_int32(field->value, 0) |
					((unsigned long long)(guint32)mdb_get_int32(field->value, 4) << 32)) / 10000.0);
			break;
		case MDB_NUMERIC:
			if (field->siz < 17)
				return 0;
			ret = mdb_test_double(node->op, mdb_sarg_node_double(node),
				mdb_numeric_to_double(field->value, col->col_prec));
			break;
		case MDB_FLOAT:
			/* compare in single precision, so that 0.05 matches the
			 * single closest to 0.05 */
			ret = mdb_test_double(node->op, (float)mdb_sarg_node_double(node), mdb_get_single(field->value, 0));
			break;
		case MDB_DOUBLE:
			ret = mdb_test_double(node->op, mdb_sarg_node_double(node), mdb_get_double(field->value, 0));
			break;
		case MDB_TEXT:
			mdb_unicode2ascii(mdb, field->value, field->siz, tmpbuf, sizeof(tmpbuf));
			ret = mdb_test_string(node, tmpbuf);
			break;
		case MDB_MEMO:
		case MDB_REPID:
			val = mdb_col_to_string(mdb, mdb->pg_buf, field->start, col->col_type,
				col->col_type == MDB_REPID ? field->siz : (gint32)mdb_get_int32(field->value, 0));
			//printf("%s\n",val);
			ret = mdb_test_string(node, val);
			g_free(val);
			break;
		case MDB_DATETIME:
			ret = mdb_test_double(node->op, poor_mans_trunc(node->value.d), poor_mans_trunc(mdb_get_double(field->value, 0)));
			break;
		default:
			fprintf(stderr, "Calling mdb_test_sarg on unknown type.  Add code to mdb_test_sarg() for type %d\n",col->col_type);
			break;
	}
	return ret;
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
/* SQL three-valued logic: a comparison involving NULL is unknown, and
 * NOT unknown is still unknown, so the row is not selected either way */
#define SARG_FALSE 0
#define SARG_TRUE 1
#define SARG_UNKNOWN -1
static int
mdb_test_sarg_node_3vl(MdbHandle *mdb, MdbSargNode *node, MdbField *fields, int num_fields)
{
	int elem;
	MdbColumn *col;
	int l, r;

	if (mdb_is_relational_op(node->op)) {
		col = node->col;
		/* for const = const expressions; -1 means unknown (NULL) */
		if (!col) {
			if (node->value.i < 0)
				return SARG_UNKNOWN;
			return node->value.i ? SARG_TRUE : SARG_FALSE;
		}
		elem = mdb_find_field(col->col_num, fields, num_fields);
		if (elem < 0)
			return SARG_FALSE;
		if (col->col_type != MDB_BOOL && fields[elem].is_null &&
				node->op != MDB_ISNULL && node->op != MDB_NOTNULL)
			return SARG_UNKNOWN;
		return mdb_test_sarg(mdb, col, node, &fields[elem]) ? SARG_TRUE : SARG_FALSE;
	}
	/* logical op */
	switch (node->op) {
	case MDB_NOT:
		l = mdb_test_sarg_node_3vl(mdb, node->left, fields, num_fields);
		return l == SARG_UNKNOWN ? SARG_UNKNOWN : !l;
	case MDB_AND:
		l = mdb_test_sarg_node_3vl(mdb, node->left, fields, num_fields);
		if (l == SARG_FALSE)
			return SARG_FALSE;
		r = mdb_test_sarg_node_3vl(mdb, node->right, fields, num_fields);
		if (r == SARG_FALSE)
			return SARG_FALSE;
		return (l == SARG_UNKNOWN || r == SARG_UNKNOWN) ? SARG_UNKNOWN : SARG_TRUE;
	case MDB_OR:
		l = mdb_test_sarg_node_3vl(mdb, node->left, fields, num_fields);
		if (l == SARG_TRUE)
			return SARG_TRUE;
		r = mdb_test_sarg_node_3vl(mdb, node->right, fields, num_fields);
		if (r == SARG_TRUE)
			return SARG_TRUE;
		return (l == SARG_UNKNOWN || r == SARG_UNKNOWN) ? SARG_UNKNOWN : SARG_FALSE;
	}
	return SARG_TRUE;
}
int
mdb_test_sarg_node(MdbHandle *mdb, MdbSargNode *node, MdbField *fields, int num_fields)
{
	return mdb_test_sarg_node_3vl(mdb, node, fields, num_fields) == SARG_TRUE;
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
	sarg = g_memdup2(in_sarg,sizeof(MdbSarg));
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
