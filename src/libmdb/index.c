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
char idx_to_text[] = {
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0-7     0x00-0x07 */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 8-15    0x09-0x0f */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 16-23   0x10-0x17 */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 24-31   0x19-0x1f */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 32-39   0x20-0x27 */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 40-47   0x29-0x2f */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 48-55   0x30-0x37 */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 56-63   0x39-0x3f */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 64-71   0x40-0x47 */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 72-79   0x49-0x4f */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, '0',  '1',  /* 80-87   0x50-0x57 */
'2',  '3',  '4',  '5',  '6',  '7',  '8',  '9',  /* 88-95   0x59-0x5f */
'A',  'B',  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 96-103  0x60-0x67 */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 104-111 0x69-0x6f */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 112-119 0x70-0x77 */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  /* 120-127 0x79-0x7f */
};

GPtrArray *mdb_read_indices(MdbTableDef *table)
{
MdbHandle *mdb = table->entry->mdb;
MdbIndex idx, *pidx;
int len, i, j;
int idx_num, key_num, col_num;
int cur_pos;
int name_sz;

/* FIX ME -- doesn't handle multipage table headers */

        table->indices = g_ptr_array_new();

        cur_pos = table->index_start + 39 * table->num_real_idxs;

	for (i=0;i<table->num_idxs;i++) {
		memset(&idx, '\0', sizeof(MdbIndex));
		idx.index_num = mdb_get_int16(mdb, cur_pos);
		cur_pos += 19;
		idx.index_type = mdb->pg_buf[cur_pos++]; 
		mdb_append_index(table->indices, &idx);
	}

	for (i=0;i<table->num_idxs;i++) {
		pidx = g_ptr_array_index (table->indices, i);
		name_sz=mdb->pg_buf[cur_pos++];
		memcpy(pidx->name, &mdb->pg_buf[cur_pos], name_sz);
		pidx->name[name_sz]='\0';		
		//fprintf(stderr, "index name %s\n", pidx->name);
		cur_pos += name_sz;
	}

	cur_pos = table->index_start;
	idx_num=0;
	for (i=0;i<table->num_real_idxs;i++) {
		do {
			pidx = g_ptr_array_index (table->indices, idx_num++);
		} while (pidx && pidx->index_type==2);

		/* if there are more real indexes than index entries left after
		   removing type 2's decrement real indexes and continue.  Happens
		   on Northwind Orders table.
		*/
		if (!pidx) {
			table->num_real_idxs--;
			continue;
		}

		pidx->num_rows = mdb_get_int32(mdb, 43+(i*8) );

		key_num=0;
		for (j=0;j<MDB_MAX_IDX_COLS;j++) {
			col_num=mdb_get_int16(mdb,cur_pos);
			cur_pos += 2;
			if (col_num != 0xFFFF) {
				/* set column number to a 1 based column number and store */
				pidx->key_col_num[key_num]=col_num + 1;
				if (mdb->pg_buf[cur_pos]) {
					pidx->key_col_order[key_num]=MDB_ASC;
				} else {
					pidx->key_col_order[key_num]=MDB_DESC;
				}
				key_num++;
			}
			cur_pos++;
		}
		pidx->num_keys = key_num;
		cur_pos += 4;
		pidx->first_pg = mdb_get_int32(mdb, cur_pos);
		cur_pos += 4;
		pidx->flags = mdb->pg_buf[cur_pos++];
	}
}
void mdb_index_walk(MdbTableDef *table, MdbIndex *idx)
{
MdbHandle *mdb = table->entry->mdb;
int cur_pos = 0;
unsigned char marker;
MdbColumn *col;
int i;

	if (idx->num_keys!=1) return;

	mdb_read_pg(mdb, idx->first_pg);
	cur_pos = 0xf8;
	
	for (i=0;i<idx->num_keys;i++) {
		marker = mdb->pg_buf[cur_pos++];
		col=g_ptr_array_index(table->columns,idx->key_col_num[i]-1);
		printf("column %d coltype %d col_size %d\n",i,col->col_type, mdb_col_fixed_size(col));
	}
}
void mdb_index_dump(MdbTableDef *table, MdbIndex *idx)
{
int i;
MdbColumn *col;

	fprintf(stdout,"index number     %d\n", idx->index_num);
	fprintf(stdout,"index name       %s\n", idx->name);
	fprintf(stdout,"index first page %d\n", idx->first_pg);
	fprintf(stdout,"index rows       %d\n", idx->num_rows);
	if (idx->index_type==1) fprintf(stdout,"index is a primary key\n");
	for (i=0;i<idx->num_keys;i++) {
		col=g_ptr_array_index(table->columns,idx->key_col_num[i]-1);
		fprintf(stdout,"Column %s(%d) Sorted %s Unique: %s\n", 
			col->name,
			idx->key_col_num[i], 
			idx->key_col_order[i]==MDB_ASC ? "ascending" : "descending",
			idx->flags & MDB_IDX_UNIQUE ? "Yes" : "No"
			);
	}
	mdb_index_walk(table, idx);
}
