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

#ifdef DMALLOC
#include "dmalloc.h"
#endif


static gint mdb_col_comparer(MdbColumn **a, MdbColumn **b)
{
	if ((*a)->col_num > (*b)->col_num)
		return 1;
	else if ((*a)->col_num < (*b)->col_num)
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

MdbTableDef *mdb_alloc_tabledef(MdbCatalogEntry *entry)
{
	MdbTableDef *table;

	table = (MdbTableDef *) g_malloc0(sizeof(MdbTableDef));
	table->entry=entry;
	strcpy(table->name, entry->object_name);

	return table;	
}
void mdb_free_tabledef(MdbTableDef *table)
{
	if (!table) return;
	if (table->is_temp_table) {
		unsigned int i;
		for (i=0; i<table->temp_table_pages->len; i++)
			g_free(g_ptr_array_index(table->temp_table_pages,i));
		g_ptr_array_free(table->temp_table_pages, TRUE);
	}
	mdb_free_columns(table->columns);
	mdb_free_indices(table->indices);
	g_free(table->usage_map);
	g_free(table->free_usage_map);
	g_free(table);
}
MdbTableDef *mdb_read_table(MdbCatalogEntry *entry)
{
	MdbTableDef *table;
	MdbHandle *mdb = entry->mdb;
	MdbFormatConstants *fmt = mdb->fmt;
	int len, row_start, pg_row;
	char *buf;

	table = mdb_alloc_tabledef(entry);

	mdb_read_pg(mdb, entry->table_pg);
	if (mdb->pg_buf[0] != 0x02) return NULL; /* not a valid table def page */

	len = mdb_pg_get_int16(mdb,8);

	table->num_rows = mdb_pg_get_int32(mdb, fmt->tab_num_rows_offset);
	table->num_var_cols = mdb_pg_get_int16(mdb, fmt->tab_num_cols_offset-2);
	table->num_cols = mdb_pg_get_int16(mdb, fmt->tab_num_cols_offset);
	table->num_idxs = mdb_pg_get_int32(mdb, fmt->tab_num_idxs_offset); 
	table->num_real_idxs = mdb_pg_get_int32(mdb, fmt->tab_num_ridxs_offset); 
	/* grab a copy of the usage map */
	pg_row = mdb_pg_get_int32(mdb, fmt->tab_usage_map_offset);
	mdb_find_pg_row(mdb, pg_row, &buf, &row_start, &(table->map_sz));
	table->usage_map = g_memdup(buf + row_start, table->map_sz);
	if (mdb_get_option(MDB_DEBUG_USAGE)) 
		buffer_dump(buf, row_start, row_start+table->map_sz-1);
	mdb_debug(MDB_DEBUG_USAGE,"usage map found on page %ld row %d start %d len %d",
		pg_row >> 8, pg_row & 0xff, row_start, table->map_sz);

	/* grab a copy of the free space page map */
	pg_row = mdb_pg_get_int32(mdb, fmt->tab_free_map_offset);
	mdb_find_pg_row(mdb, pg_row, &buf, &row_start, &(table->freemap_sz));
	table->free_usage_map = g_memdup(buf + row_start, table->freemap_sz);
	mdb_debug(MDB_DEBUG_USAGE,"free map found on page %ld row %d start %d len %d\n",
		pg_row >> 8, pg_row & 0xff, row_start, table->freemap_sz);

	table->first_data_pg = mdb_pg_get_int16(mdb, fmt->tab_first_dpg_offset);

	return table;
}
MdbTableDef *mdb_read_table_by_name(MdbHandle *mdb, gchar *table_name, int obj_type)
{
	unsigned int i;
	MdbCatalogEntry *entry;

	mdb_read_catalog(mdb, obj_type);

	for (i=0; i<mdb->num_catalog; i++) {
		entry = g_ptr_array_index(mdb->catalog, i);
		if (!strcasecmp(entry->object_name, table_name))
			return mdb_read_table(entry);
	}

	return NULL;
}

/*
** read the next page if offset is > pg_size
** return true if page was read
*/ 
int 
read_pg_if(MdbHandle *mdb, int *cur_pos, int offset)
{
	if (*cur_pos + offset >= mdb->fmt->pg_size) {
		mdb_read_pg(mdb, mdb_pg_get_int32(mdb,4));
		*cur_pos = 8 - (mdb->fmt->pg_size - (*cur_pos));
		return 1;
	}
	return 0;
}
guint32 
read_pg_if_32(MdbHandle *mdb, int *cur_pos)
{
	unsigned char c[4];
	int i, rc = 0;

	for (i=0;i<4;i++) {
		rc += read_pg_if(mdb, cur_pos, i);
		c[i] = mdb->pg_buf[(*cur_pos) + i];
	}
	return mdb_get_int32(c, 0);
}
guint16 
read_pg_if_16(MdbHandle *mdb, int *cur_pos)
{
	unsigned char low_byte, high_byte;
	int rc = 0;

	rc += read_pg_if(mdb, cur_pos, 0);
	low_byte = mdb->pg_buf[*cur_pos];
	rc += read_pg_if(mdb, cur_pos, 1);
	high_byte = mdb->pg_buf[(*cur_pos) + 1];

	return (high_byte * 256 + low_byte);
}
guint16 
read_pg_if_n(MdbHandle *mdb, unsigned char *buf, int *cur_pos, int len)
{
	if (*cur_pos + len < mdb->fmt->pg_size) {
		memcpy(buf, &mdb->pg_buf[*cur_pos], len);
		return 0;
	} else {
		int half = mdb->fmt->pg_size - *cur_pos;
		memcpy(buf, &mdb->pg_buf[*cur_pos], half);
		mdb_read_pg(mdb, mdb_pg_get_int32(mdb,4));
		memcpy(buf + half, &mdb->pg_buf[8], len - half);
		*cur_pos = 8 - half;
		return 1;
	}
}

void mdb_append_column(GPtrArray *columns, MdbColumn *in_col)
{
	g_ptr_array_add(columns, g_memdup(in_col,sizeof(MdbColumn)));
}
void mdb_free_columns(GPtrArray *columns)
{
	unsigned int i;

	if (!columns) return;
	for (i=0; i<columns->len; i++)
		g_free (g_ptr_array_index(columns, i));
	g_ptr_array_free(columns, TRUE);
}
GPtrArray *mdb_read_columns(MdbTableDef *table)
{
	MdbHandle *mdb = table->entry->mdb;
	MdbFormatConstants *fmt = mdb->fmt;
	MdbColumn *pcol;
	unsigned char *col;
	unsigned int i;
	int cur_pos, name_sz;
	
	table->columns = g_ptr_array_new();

	col = (unsigned char *) g_malloc(fmt->tab_col_entry_size);

	cur_pos = fmt->tab_cols_start_offset + 
		(table->num_real_idxs * fmt->tab_ridx_entry_size);

	/* new code based on patch submitted by Tim Nelson 2000.09.27 */

	/* 
	** column attributes 
	*/
	for (i=0;i<table->num_cols;i++) {
#ifdef MDB_DEBUG
	/* printf("column %d\n", i);
	buffer_dump(mdb->pg_buf, cur_pos ,cur_pos + 18); */
#endif
		read_pg_if_n(mdb, col, &cur_pos, fmt->tab_col_entry_size);
		cur_pos += fmt->tab_col_entry_size;
		pcol = (MdbColumn *) g_malloc0(sizeof(MdbColumn));

		pcol->col_type = col[0];

		// col_num_offset == 1 or 5
		pcol->col_num = col[fmt->col_num_offset];

		//fprintf(stdout,"----- column %d -----\n",pcol->col_num);
		// col_var == 3 or 7
		pcol->var_col_num = mdb_get_int16(col, fmt->tab_col_offset_var);
		//fprintf(stdout,"var column pos %d\n",pcol->var_col_num);

		// col_var == 5 or 9
		pcol->row_col_num = mdb_get_int16(col, fmt->tab_row_col_num_offset);
		//fprintf(stdout,"row column num %d\n",pcol->row_col_num);

		/* FIXME: can this be right in Jet3 and Jet4? */
		if (pcol->col_type == MDB_NUMERIC) {
			pcol->col_prec = col[11];
			pcol->col_scale = col[12];
		}

		// col_fixed_offset == 13 or 15
		pcol->is_fixed = col[fmt->col_fixed_offset] & 0x01 ? 1 : 0;

		// col_fixed_offset == 13 or 15
		pcol->fixed_offset = mdb_get_int16(col, fmt->tab_col_offset_fixed);
		//fprintf(stdout,"fixed column offset %d\n",pcol->fixed_offset);
		//fprintf(stdout,"col type %s\n",pcol->is_fixed ? "fixed" : "variable");

		if (pcol->col_type != MDB_BOOL) {
			// col_size_offset == 16 or 23
			pcol->col_size = mdb_get_int16(col, fmt->col_size_offset);
		} else {
			pcol->col_size=0;
		}
		
		g_ptr_array_add(table->columns, pcol);
	}

	g_free (col);

	/* 
	** column names - ordered the same as the column attributes table
	*/
	for (i=0;i<table->num_cols;i++) {
		pcol = g_ptr_array_index(table->columns, i);

		if (IS_JET4(mdb)) {
			char *tmp_buf;
			name_sz = read_pg_if_16(mdb, &cur_pos);
			cur_pos += 2;
			tmp_buf = (char *) g_malloc(name_sz);
			read_pg_if_n(mdb, tmp_buf, &cur_pos, name_sz);
			mdb_unicode2ascii(mdb, tmp_buf, 0, name_sz, pcol->name);
			g_free(tmp_buf);
			cur_pos += name_sz;
		} else if (IS_JET3(mdb)) {
			read_pg_if(mdb, &cur_pos, 0);
			name_sz = mdb->pg_buf[cur_pos];
			cur_pos++;
			read_pg_if_n(mdb, pcol->name, &cur_pos, name_sz);
			pcol->name[name_sz]='\0';
			cur_pos += name_sz;
		} else {
			fprintf(stderr,"Unknown MDB version\n");
		}
	}

	/* Sort the columns by col_num */
	g_ptr_array_sort(table->columns, (GCompareFunc)mdb_col_comparer);

	table->index_start = cur_pos;
	return table->columns;
}

void mdb_table_dump(MdbCatalogEntry *entry)
{
MdbTableDef *table;
MdbColumn *col;
int coln;
MdbIndex *idx;
MdbHandle *mdb = entry->mdb;
unsigned int i, bitn;
guint32 pgnum;

	table = mdb_read_table(entry);
	fprintf(stdout,"definition page     = %lu\n",entry->table_pg);
	fprintf(stdout,"number of datarows  = %d\n",table->num_rows);
	fprintf(stdout,"number of columns   = %d\n",table->num_cols);
	fprintf(stdout,"number of indices   = %d\n",table->num_real_idxs);

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
		printf("pages reserved by this object\n");
		printf("usage map pg %" G_GUINT32_FORMAT "\n",
			table->map_base_pg);
		printf("free map pg %" G_GUINT32_FORMAT "\n",
			table->freemap_base_pg);
		pgnum = mdb_get_int32(table->usage_map,1);
		/* the first 5 bytes of the usage map mean something */
		coln = 0;
		for (i=5;i<table->map_sz;i++) {
			for (bitn=0;bitn<8;bitn++) {
				if (table->usage_map[i] & 1 << bitn) {
					coln++;
					printf("%6" G_GUINT32_FORMAT, pgnum);
					if (coln==10) {
						printf("\n");
						coln = 0;
					} else {
						printf(" ");
					}
				}
				pgnum++;
			}
		}
		printf("\n");
	}
}

int mdb_is_user_table(MdbCatalogEntry *entry)
{
	return ((entry->object_type == MDB_TABLE)
	 && !(entry->flags & 0x80000002)) ? 1 : 0;
}
int mdb_is_system_table(MdbCatalogEntry *entry)
{
	return ((entry->object_type == MDB_TABLE)
	 && (entry->flags & 0x80000002)) ? 1 : 0;
}
