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

#define MDB_DEBUG_USAGE 0

static gint mdb_col_comparer(MdbColumn *a, MdbColumn *b)
{
	if (a->col_num > b->col_num)
		return 1;
	else if (a->col_num < b->col_num)
		return -1;
	else
		return 0;
}

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
MdbFormatConstants *fmt = mdb->fmt;
int len;
int rownum, row_start, row_end;

	table = mdb_alloc_tabledef(entry);

	mdb_read_pg(mdb, entry->table_pg);
	len = mdb_get_int16(mdb,8);

	table->num_rows = mdb_get_int32(mdb, fmt->tab_num_rows_offset);
	table->num_cols = mdb_get_int16(mdb, fmt->tab_num_cols_offset);
	table->num_idxs = mdb_get_int32(mdb, fmt->tab_num_idxs_offset); 
	table->num_real_idxs = mdb_get_int32(mdb, fmt->tab_num_ridxs_offset); 

	/* grab a copy of the usage map */
	rownum = mdb->pg_buf[fmt->tab_usage_map_offset];
	mdb_read_alt_pg(mdb, mdb_get_int24(mdb, fmt->tab_usage_map_offset + 1)); 
	mdb_swap_pgbuf(mdb);
	row_start = mdb_get_int16(mdb, (fmt->row_count_offset + 2) + (rownum*2));
   row_end = mdb_find_end_of_row(mdb, rownum);
	table->map_sz = row_end - row_start + 1;
	table->usage_map = malloc(table->map_sz);
	memcpy(table->usage_map, &mdb->pg_buf[row_start], table->map_sz);
#if MDB_DEBUG_USAGE
	buffer_dump(mdb->pg_buf, row_start, row_end);
#endif
	/* swap back */
	mdb_swap_pgbuf(mdb);
#if MDB_DEBUG_USAGE
	printf ("usage map found on page %ld start %d end %d\n", mdb_get_int24(mdb, fmt->tab_usage_map_offset + 1), row_start, row_end);
#endif


	table->first_data_pg = mdb_get_int16(mdb, fmt->tab_first_dpg_offset);

	return table;
}

/*
** read the next page if offset is > pg_size
** return true if page was read
*/ 
static int read_pg_if(MdbHandle *mdb, int *cur_pos, int offset)
{
	if (*cur_pos + offset >= mdb->fmt->pg_size) {
		mdb_read_pg(mdb, mdb_get_int32(mdb,4));
		*cur_pos = 8 - (mdb->fmt->pg_size - (*cur_pos));
		return 1;
	}
	return 0;
}

GPtrArray *mdb_read_columns(MdbTableDef *table)
{
MdbHandle *mdb = table->entry->mdb;
MdbFormatConstants *fmt = mdb->fmt;
MdbColumn col, *pcol;
int len, i,j;
unsigned char low_byte, high_byte;
int cur_col, cur_name;
int name_sz;
GSList	*slist = NULL;
	
	table->columns = g_ptr_array_new();

	cur_col = fmt->tab_cols_start_offset + 
		(table->num_real_idxs * fmt->tab_ridx_entry_size);

	/* new code based on patch submitted by Tim Nelson 2000.09.27 */

	/* 
	** column attributes 
	*/
	for (i=0;i<table->num_cols;i++) {
#ifdef MDB_DEBUG
	/* printf("column %d\n", i);
	buffer_dump(mdb->pg_buf, cur_col ,cur_col + 18); */
#endif
		memset(&col, 0, sizeof(col));
		col.col_num = mdb->pg_buf[cur_col + fmt->col_num_offset];

		read_pg_if(mdb, &cur_col, 0);
		col.col_type = mdb->pg_buf[cur_col];

		if (col.col_type == MDB_NUMERIC) {
			col.col_prec = mdb->pg_buf[cur_col + 11];
			col.col_scale = mdb->pg_buf[cur_col + 12];
		}

		read_pg_if(mdb, &cur_col, 13);
		col.is_fixed = mdb->pg_buf[cur_col + fmt->col_fixed_offset] & 
			0x01 ? 1 : 0;
		if (col.col_type != MDB_BOOL) {
			read_pg_if(mdb, &cur_col, 17);
			low_byte = mdb->pg_buf[cur_col + fmt->col_size_offset];
			read_pg_if(mdb, &cur_col, 18);
			high_byte = mdb->pg_buf[cur_col + fmt->col_size_offset + 1];
			col.col_size += high_byte * 256 + low_byte;
		} else
			col.col_size=0;
		
		pcol = g_memdup(&col, sizeof(MdbColumn));
		slist = g_slist_insert_sorted(slist,pcol,(GCompareFunc)mdb_col_comparer);
		cur_col += fmt->tab_col_entry_size;
	}

	cur_name = cur_col;
	
	/* 
	** column names 
	*/
	for (i=0;i<table->num_cols;i++) {
		/* fetch the column */
		pcol = g_slist_nth_data (slist, i);

		/* we have reached the end of page */
		read_pg_if(mdb, &cur_name, 0);
		name_sz = mdb->pg_buf[cur_name];
		
		if (IS_JET4(mdb)) {
			/* FIX ME - for now just skip the high order byte */
			cur_name += 2;
			/* determine amount of name on this page */
			len = ((cur_name + name_sz) > fmt->pg_size) ? 
				fmt->pg_size - cur_name :
				name_sz;
	
			/* strip high order (second) byte from unicode string */
			for (j=0;j<len;j+=2) {
				pcol->name[j/2] = mdb->pg_buf[cur_name + j];
			}
			/* name wrapped over page */
			if (len < name_sz) {
				/* read the next pg */
				mdb_read_pg(mdb, mdb_get_int32(mdb,4)); 
				cur_name = 8 - (fmt->pg_size - cur_name);
				if (len % 2) cur_name++;
				/* get the rest of the name */
				for (j=0;j<len;j+=2) {
				}
				memcpy(&pcol->name[len], &mdb->pg_buf[cur_name], name_sz - len);
			}
			pcol->name[name_sz]='\0';

			cur_name += name_sz;
		} else if (IS_JET3(mdb)) {
			/* determine amount of name on this page */
			len = ((cur_name + name_sz) > fmt->pg_size) ? 
				fmt->pg_size - cur_name :
				name_sz;
	
			if (len) {
				memcpy(pcol->name, &mdb->pg_buf[cur_name+1], len);
			}
			/* name wrapped over page */
			if (len < name_sz) {
				/* read the next pg */
				mdb_read_pg(mdb, mdb_get_int32(mdb,4)); 
				cur_name = 8 - (fmt->pg_size - cur_name);
				/* get the rest of the name */
				memcpy(&pcol->name[len], &mdb->pg_buf[cur_name], name_sz - len);
			}
			pcol->name[name_sz]='\0';

			cur_name += name_sz + 1;
		} else {
			fprintf(stderr,"Unknown MDB version\n");
		}
	}
	/* turn this list into an array */
	for (i=0;i<table->num_cols;i++) {
		pcol = g_slist_nth_data (slist, i);
		g_ptr_array_add(table->columns, pcol);
	}

	g_slist_free(slist);

	table->index_start = cur_name;
	return table->columns;
}

void mdb_table_dump(MdbCatalogEntry *entry)
{
MdbTableDef *table;
MdbColumn *col;
int coln;
MdbIndex *idx;
MdbHandle *mdb = entry->mdb;
int i,bitn;
int pgnum;

	table = mdb_read_table(entry);
	fprintf(stdout,"definition page     = %lu\n",entry->table_pg);
	fprintf(stdout,"number of datarows  = %d\n",table->num_rows);
	fprintf(stdout,"number of columns   = %d\n",table->num_cols);
	fprintf(stdout,"number of indices   = %d\n",table->num_real_idxs);
	fprintf(stdout,"first data page     = %lu\n",table->first_data_pg);

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
	if (table->usage_map) {
		printf("pages reserved by this object\n",pgnum);
		pgnum = _mdb_get_int32(table->usage_map,1);
		/* the first 5 bytes of the usage map mean something */
		coln = 0;
		for (i=5;i<table->map_sz;i++) {
			for (bitn=0;bitn<8;bitn++) {
				if (table->usage_map[i] & 1 << bitn) {
					coln++;
					printf("%6ld ",pgnum);
					if (coln==10) {
						printf("\n");
						coln = 0;
					}
				}
				pgnum++;
			}
		}
	}
}
