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


unsigned char mdb_col_needs_size(int col_type)
{
	if (col_type == MDB_TEXT) {
		return TRUE;
	} else {
		return FALSE;
	}
}

MdbTableDef *mdb_read_table(MdbCatalogEntry *entry)
{
MdbTableDef *table;
MdbHandle *mdb = entry->mdb;
int len, i;

	table = mdb_alloc_tabledef(entry);

	mdb_read_pg(mdb, entry->table_pg);
	len = mdb_get_int16(mdb,8);

	table->num_rows = mdb_get_int32(mdb,12);
	table->num_cols = mdb_get_int16(mdb,25);
	table->num_idxs = mdb_get_int32(mdb,27); 
	table->num_real_idxs = mdb_get_int32(mdb,31); 
	table->first_data_pg = mdb_get_int16(mdb,36);

	return table;
}

/*
** read the next page if offset is > pg_size
** return true if page was read
*/ 
static int read_pg_if(MdbHandle *mdb, int *cur_pos, int offset)
{
	if (*cur_pos + offset >= mdb->pg_size) {
		mdb_read_pg(mdb, mdb_get_int32(mdb,4));
		*cur_pos = 8 - (mdb->pg_size - (*cur_pos));
		return 1;
	}
	return 0;
}

GPtrArray *mdb_read_columns(MdbTableDef *table)
{
MdbHandle *mdb = table->entry->mdb;
MdbColumn col, *pcol;
int len, i;
unsigned char low_byte, high_byte;
int cur_col, cur_name;
int col_type, col_size;
char name[MDB_MAX_OBJ_NAME+1];
int name_sz;
	
	table->columns = g_ptr_array_new();

	cur_col = 43 + (table->num_real_idxs * 8);

	/* new code based on patch submitted by Tim Nelson 2000.09.27 */

	/* 
	** column attributes 
	*/
	for (i=0; i<table->num_cols;i++) {
		memset(&col,'\0', sizeof(MdbColumn));

		read_pg_if(mdb, &cur_col, 0);
		col.col_type = mdb->pg_buf[cur_col];

		read_pg_if(mdb, &cur_col, 13);
		col.is_fixed = mdb->pg_buf[cur_col+13] & 0x01 ? 1 : 0;

		read_pg_if(mdb, &cur_col, 17);
		low_byte = mdb->pg_buf[cur_col+16];
		read_pg_if(mdb, &cur_col, 18);
		high_byte = mdb->pg_buf[cur_col+17];
		col.col_size += high_byte * 256 + low_byte;

		mdb_append_column(table->columns, &col);
		cur_col += 18;
	}

	cur_name = cur_col;
	
	/* 
	** column names 
	*/
	for (i=0;i<table->num_cols;i++) {
		/* fetch the column */
		pcol = g_ptr_array_index (table->columns, i);

		/* we have reached the end of page */
		read_pg_if(mdb, &cur_name, 0);
		name_sz = mdb->pg_buf[cur_name];
		
		/* determine amount of name on this page */
		len = ((cur_name + name_sz) > mdb->pg_size) ? 
			mdb->pg_size - cur_name :
			name_sz;

		if (len) {
			memcpy(pcol->name, &mdb->pg_buf[cur_name+1], len);
		}
		/* name wrapped over page */
		if (len < name_sz) {
			/* read the next pg */
			mdb_read_pg(mdb, mdb_get_int32(mdb,4)); 
			cur_name = 8 - (mdb->pg_size - cur_name);
			/* get the rest of the name */
			memcpy(&pcol->name[len], &mdb->pg_buf[cur_name], name_sz - len);
		}
		pcol->name[name_sz]='\0';

		cur_name += name_sz + 1;
	}
	table->index_start = cur_name;
	return table->columns;
}

void mdb_table_dump(MdbCatalogEntry *entry)
{
MdbTableDef *table;
MdbColumn *col;
MdbIndex *idx;
MdbHandle *mdb = entry->mdb;
int i;

	table = mdb_read_table(entry);
	fprintf(stdout,"definition page     = %d\n",entry->table_pg);
	fprintf(stdout,"number of datarows  = %d\n",table->num_rows);
	fprintf(stdout,"number of columns   = %d\n",table->num_cols);
	fprintf(stdout,"number of indices   = %d\n",table->num_real_idxs);
	fprintf(stdout,"first data page     = %d\n",table->first_data_pg);

	mdb_read_columns(table);
	mdb_read_indices(table);

	for (i=0;i<table->num_cols;i++) {
		col = g_ptr_array_index(table->columns,i);
	
		fprintf(stdout,"column %d Name: %-20s Type: %s(%d)\n",
			i, col->name,
			mdb_get_coltype_string(mdb->default_backend, col->col_type),
			col->col_size);
	}

	for (i=0;i<table->num_idxs;i++) {
		idx = g_ptr_array_index (table->indices, i);
		mdb_index_dump(table, idx);
	}
}
