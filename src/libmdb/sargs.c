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

#include "mdbtools.h"

int mdb_test_string(MdbSarg *sarg, char *s)
{
int rc;

	if (sarg->op == MDB_LIKE) {
		return likecmp(s,sarg->value.s);
	}
	rc = strncmp(sarg->value.s, s, 255);
	switch (sarg->op) {
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
			fprintf(stderr, "Calling mdb_test_sarg on unknown operator.  Add code to mdb_test_string() for operator %d\n",sarg->op);
			break;
	}
	return 0;
}
int mdb_test_int(MdbSarg *sarg, gint32 i)
{
	switch (sarg->op) {
		case MDB_EQUAL:
			if (sarg->value.i == i) return 1;
			break;
		case MDB_GT:
			if (sarg->value.i < i) return 1;
			break;
		case MDB_LT:
			if (sarg->value.i > i) return 1;
			break;
		case MDB_GTEQ:
			if (sarg->value.i <= i) return 1;
			break;
		case MDB_LTEQ:
			if (sarg->value.i >= i) return 1;
			break;
		default:
			fprintf(stderr, "Calling mdb_test_sarg on unknown operator.  Add code to mdb_test_int() for operator %d\n",sarg->op);
			break;
	}
	return 0;
}
int mdb_test_sarg(MdbHandle *mdb, MdbColumn *col, MdbSarg *sarg, int offset, int len)
{
char tmpbuf[256];
int lastchar;

	switch (col->col_type) {
		case MDB_BYTE:
			return mdb_test_int(sarg, mdb_get_byte(mdb, offset));
			break;
		case MDB_INT:
			return mdb_test_int(sarg, mdb_get_int16(mdb, offset));
			break;
		case MDB_LONGINT:
			return mdb_test_int(sarg, mdb_get_int32(mdb, offset));
			break;
		case MDB_TEXT:
			strncpy(tmpbuf, &mdb->pg_buf[offset],255);
			lastchar = len > 255 ? 255 : len;
			tmpbuf[lastchar]='\0';
			return mdb_test_string(sarg, tmpbuf);
		default:
			fprintf(stderr, "Calling mdb_test_sarg on unknown type.  Add code to mdb_test_sarg() for type %d\n",col->col_type);
			break;
	}
	return 1;
}
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
int i;

	for (i=0;i<table->num_cols;i++) {
		col = g_ptr_array_index (table->columns, i);
		if (!strcasecmp(col->name,colname)) {
			return mdb_add_sarg(col, in_sarg);
		}
	}
	/* else didn't find the column return 0! */
	return 0;
}
