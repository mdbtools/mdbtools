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
#include "time.h"
#include "math.h"

#define MDB_DEBUG_WRITE 1

typedef struct {
	void *value;
	int siz;
	unsigned char is_null;
	unsigned char is_fixed;
	int colnum;
} MdbField;

static int 
mdb_is_col_indexed(MdbTableDef *table, int colnum)
{
int i, j;
MdbIndex *idx;

	for (i=0;i<table->num_idxs;i++) {
		idx = g_ptr_array_index (table->indices, i);
		for (j=0;j<idx->num_keys;j++) {
			if (idx->key_col_num[j]==colnum) return 1;
		}
	}
	return 0;
}
int
mdb_crack_row(MdbTableDef *table, int row_start, int row_end, MdbField *fields)
{
MdbCatalogEntry *entry = table->entry;
MdbHandle *mdb = entry->mdb;
MdbColumn *col;
int var_cols, fixed_cols, num_cols, i, totcols;
unsigned char *nullmask;
int bitmask_sz;
int byte_num, bit_num;


	printf("field 0 %s\n", fields[0].value);

	if (mdb->jet_version==MDB_VER_JET4) {
		num_cols = mdb_get_int16(mdb, row_start);
	} else {
		num_cols = mdb->pg_buf[row_start];
	}

	totcols = 0;
	var_cols = 0; /* mdb->pg_buf[row_end-1]; */
	fixed_cols = 0; /* num_cols - var_cols; */
	for (i = 0; i < table->num_cols; i++) {
		col = g_ptr_array_index (table->columns, i);
		if (mdb_is_fixed_col(col)) {
			fixed_cols++;
			fields[totcols++].colnum = i;
			fields[totcols].siz = col->col_size;
			fields[totcols].is_fixed = 1;
		}
	}
	for (i = 0; i < table->num_cols; i++) {
		col = g_ptr_array_index (table->columns, i);
		if (!mdb_is_fixed_col(col)) {
			var_cols++;
			fields[totcols++].colnum = i;
			fields[totcols].is_fixed = 0;
		}
	}

	bitmask_sz = (num_cols - 1) / 8 + 1;
	nullmask = &mdb->pg_buf[row_end - bitmask_sz + 1];

	for (i=0;i<num_cols;i++) {
		byte_num = i / 8;
		bit_num = i % 8;
		/* logic on nulls is reverse, 1 is not null, 0 is null */
		fields[i].is_null = nullmask[byte_num] & 1 << bit_num ? 0 : 1;
		printf("col %d is %s\n", i, fields[i].is_null ? "null" : "not null");
	}

	return num_cols;

}
int
mdb_pack_row(MdbTableDef *table, unsigned char *row_buffer, MdbField *fields)
{
}
int
mdb_pg_get_freespace(MdbHandle *mdb)
{
int rows, free_start, free_end;

	rows = mdb_get_int16(mdb, mdb->row_count_offset);
	free_start = mdb->row_count_offset + 2 + (rows * 2);
        free_end = mdb_get_int16(mdb, (mdb->row_count_offset + rows * 2)) -1;
#if MDB_DEBUG_WRITE
	printf("free space left on page = %d\n", free_end - free_start);
#endif
	return (free_end - free_start);
}
int 
mdb_update_row(MdbTableDef *table)
{
int row_start, row_end;
int i;
MdbColumn *col;
MdbCatalogEntry *entry = table->entry;
MdbHandle *mdb = entry->mdb;
MdbField fields[256];
unsigned char row_buffer[4096];
int old_row_size, new_row_size, delta, num_fields;

	fields[0].value = "hello";

	row_start = mdb_get_int16(mdb, (mdb->row_count_offset + 2) + (table->cur_row*2)); 
	row_end = mdb_find_end_of_row(mdb, table->cur_row);
	old_row_size = row_end - row_start;

	row_start &= 0x0FFF; /* remove flags */

	for (i=0;i<table->num_cols;i++) {
		col = g_ptr_array_index(table->columns,i);
		if (col->bind_ptr && mdb_is_col_indexed(table,i)) {
			fprintf(stderr, "Attempting to update column that is part of an index\n");
			return 0;
		}
	}
	num_fields = mdb_crack_row(table, row_start, row_end, &fields);

#if MDB_DEBUG_WRITE
	for (i=0;i<num_fields;i++) {
	}
#endif

	new_row_size = mdb_pack_row(table, row_buffer, &fields);
	delta = new_row_size - old_row_size;
	if ((mdb_pg_get_freespace(mdb) - delta) < 0) {
		fprintf(stderr, "No space left on this page, update will not occur\n");
		return 0;
	}
}
