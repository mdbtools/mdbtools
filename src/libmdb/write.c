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
static int mdb_is_null(unsigned char *null_mask, int col_num)
{
int byte_num = (col_num - 1) / 8;
int bit_num = (col_num - 1) % 8;

	if ((1 << bit_num) & null_mask[byte_num]) {
		return 0;
	} else {
		return 1;
	}
}
int
mdb_crack_row(MdbTableDef *table, int row_start, int row_end, MdbField *fields)
{
MdbCatalogEntry *entry = table->entry;
MdbHandle *mdb = entry->mdb;
MdbColumn *col;
int var_cols, fixed_cols, num_cols, i;
unsigned char null_mask[33]; /* 256 columns max / 8 bits per byte */
int bitmask_sz;

	printf("field 0 %s\n", fields[0].value);

	if (mdb->jet_version==MDB_VER_JET4) {
		num_cols = mdb_get_int16(mdb, row_start);
	} else {
		num_cols = mdb->pg_buf[row_start];
	}

	var_cols = 0; /* mdb->pg_buf[row_end-1]; */
	fixed_cols = 0; /* num_cols - var_cols; */
	for (i = 0; i < table->num_cols; i++) {
		col = g_ptr_array_index (table->columns, i);
		if (mdb_is_fixed_col(col))
			fixed_cols++;
		else
			var_cols++;
	}

	bitmask_sz = (num_cols - 1) / 8 + 1;

	for (i=0;i<bitmask_sz;i++) {
		null_mask[i]=mdb->pg_buf[row_end - bitmask_sz + i + 1];
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
