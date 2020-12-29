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

#include "mdbtools.h"

static gint mdb_col_comparer(MdbColumn **a, MdbColumn **b)
{
	if ((*a)->col_num > (*b)->col_num)
		return 1;
	else if ((*a)->col_num < (*b)->col_num)
		return -1;
	else
		return 0;
}

MdbTableDef *mdb_alloc_tabledef(MdbCatalogEntry *entry)
{
	MdbTableDef *table = g_malloc0(sizeof(MdbTableDef));
	table->entry=entry;
	snprintf(table->name, sizeof(table->name), "%s", entry->object_name);

	return table;	
}
void mdb_free_tabledef(MdbTableDef *table)
{
	if (!table) return;
	if (table->is_temp_table) {
		guint i;
		/* Temp table pages are being stored in memory */
		for (i=0; i<table->temp_table_pages->len; i++)
			g_free(g_ptr_array_index(table->temp_table_pages,i));
		g_ptr_array_free(table->temp_table_pages, TRUE);
		/* Temp tables use dummy entries */
		g_free(table->entry);
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
	int row_start, pg_row;
	void *buf, *pg_buf = mdb->pg_buf;
	guint i;

	if (!mdb_read_pg(mdb, entry->table_pg)) {
        fprintf(stderr, "mdb_read_table: Unable to read page %lu\n", entry->table_pg);
        return NULL;
    }
	if (mdb_get_byte(pg_buf, 0) != 0x02) {
        fprintf(stderr, "mdb_read_table: Page %lu [size=%d] is not a valid table definition page (First byte = 0x%02X, expected 0x02)\n",
                entry->table_pg, (int)fmt->pg_size, mdb_get_byte(pg_buf, 0));
		return NULL;
    }
	table = mdb_alloc_tabledef(entry);

	mdb_get_int16(pg_buf, 8); /* len */

	/* Note that num_rows may be zero if the database was improperly closed.
	 * See https://github.com/mdbtools/mdbtools/issues/120 for discussion. */
	table->num_rows = mdb_get_int32(pg_buf, fmt->tab_num_rows_offset);
	table->num_var_cols = mdb_get_int16(pg_buf, fmt->tab_num_cols_offset-2);
	table->num_cols = mdb_get_int16(pg_buf, fmt->tab_num_cols_offset);
	table->num_idxs = mdb_get_int32(pg_buf, fmt->tab_num_idxs_offset);
	table->num_real_idxs = mdb_get_int32(pg_buf, fmt->tab_num_ridxs_offset);

	/* grab a copy of the usage map */
	pg_row = mdb_get_int32(pg_buf, fmt->tab_usage_map_offset);
	if (mdb_find_pg_row(mdb, pg_row, &buf, &row_start, &(table->map_sz))) {
        fprintf(stderr, "mdb_read_table: Unable to find page row %d\n", pg_row);
		mdb_free_tabledef(table);
		return NULL;
	}
	table->usage_map = g_memdup((char*)buf + row_start, table->map_sz);
	if (mdb_get_option(MDB_DEBUG_USAGE)) 
		mdb_buffer_dump(buf, row_start, table->map_sz);
	mdb_debug(MDB_DEBUG_USAGE,"usage map found on page %ld row %d start %d len %d",
		pg_row >> 8, pg_row & 0xff, row_start, table->map_sz);

	/* grab a copy of the free space page map */
	pg_row = mdb_get_int32(pg_buf, fmt->tab_free_map_offset);
	if (mdb_find_pg_row(mdb, pg_row, &buf, &row_start, &(table->freemap_sz))) {
        fprintf(stderr, "mdb_read_table: Unable to find page row %d\n", pg_row);
		mdb_free_tabledef(table);
		return NULL;
	}
	table->free_usage_map = g_memdup((char*)buf + row_start, table->freemap_sz);
	mdb_debug(MDB_DEBUG_USAGE,"free map found on page %ld row %d start %d len %d\n",
		pg_row >> 8, pg_row & 0xff, row_start, table->freemap_sz);

	table->first_data_pg = mdb_get_int16(pg_buf, fmt->tab_first_dpg_offset);

	if (entry->props)
		for (i=0; i<entry->props->len; ++i) {
			MdbProperties *props = g_ptr_array_index(entry->props, i);
			if (!props->name)
				table->props = props;
		}

	return table;
}
MdbTableDef *mdb_read_table_by_name(MdbHandle *mdb, gchar *table_name, int obj_type)
{
	unsigned int i;
	MdbCatalogEntry *entry;

	mdb_read_catalog(mdb, obj_type);

	for (i=0; i<mdb->num_catalog; i++) {
		entry = g_ptr_array_index(mdb->catalog, i);
		if (!g_ascii_strcasecmp(entry->object_name, table_name))
			return mdb_read_table(entry);
	}

	return NULL;
}


guint32 
read_pg_if_32(MdbHandle *mdb, int *cur_pos)
{
	char c[4];

	read_pg_if_n(mdb, c, cur_pos, 4);
	return mdb_get_int32(c, 0);
}
guint16 
read_pg_if_16(MdbHandle *mdb, int *cur_pos)
{
	char c[2];

	read_pg_if_n(mdb, c, cur_pos, 2);
	return mdb_get_int16(c, 0);
}
guint8
read_pg_if_8(MdbHandle *mdb, int *cur_pos)
{
	guint8 c;

	read_pg_if_n(mdb, &c, cur_pos, 1);
	return c;
}
/*
 * Read data into a buffer, advancing pages and setting the
 * page cursor as needed.  In the case that buf in NULL, pages
 * are still advanced and the page cursor is still updated.
 */
void * 
read_pg_if_n(MdbHandle *mdb, void *buf, int *cur_pos, size_t len)
{
	char* _buf = buf;

	/* Advance to page which contains the first byte */
	while (*cur_pos >= mdb->fmt->pg_size) {
		mdb_read_pg(mdb, mdb_get_int32(mdb->pg_buf,4));
		*cur_pos -= (mdb->fmt->pg_size - 8);
	}
	/* Copy pages into buffer */
	while (*cur_pos + len >= (size_t)mdb->fmt->pg_size) {
		int piece_len = mdb->fmt->pg_size - *cur_pos;
		if (_buf) {
			memcpy(_buf, mdb->pg_buf + *cur_pos, piece_len);
			_buf += piece_len;
		}
		len -= piece_len;
		mdb_read_pg(mdb, mdb_get_int32(mdb->pg_buf,4));
		*cur_pos = 8;
	}
	/* Copy into buffer from final page */
	if (len && _buf) {
		memcpy(_buf, mdb->pg_buf + *cur_pos, len);
	}
	*cur_pos += len;
	return _buf;
}


void mdb_append_column(GPtrArray *columns, MdbColumn *in_col)
{
	g_ptr_array_add(columns, g_memdup(in_col,sizeof(MdbColumn)));
}
void mdb_free_columns(GPtrArray *columns)
{
	guint i, j;
	MdbColumn *col;

	if (!columns) return;
	for (i=0; i<columns->len; i++) {
		col = (MdbColumn *) g_ptr_array_index(columns, i);
		if (col->sargs) {
			for (j=0; j<col->sargs->len; j++) {
				g_free( g_ptr_array_index(col->sargs, j));
			}
			g_ptr_array_free(col->sargs, TRUE);
		}
		g_free(col);
	}
	g_ptr_array_free(columns, TRUE);
}
GPtrArray *mdb_read_columns(MdbTableDef *table)
{
	MdbHandle *mdb = table->entry->mdb;
	MdbFormatConstants *fmt = mdb->fmt;
	MdbColumn *pcol;
	unsigned char *col;
	unsigned int i;
	guint j;
	int cur_pos;
	size_t name_sz;
	GPtrArray *allprops;
	
	table->columns = g_ptr_array_new();

	col = g_malloc(fmt->tab_col_entry_size);

	cur_pos = fmt->tab_cols_start_offset + 
		(table->num_real_idxs * fmt->tab_ridx_entry_size);

	/* new code based on patch submitted by Tim Nelson 2000.09.27 */

	/* 
	** column attributes 
	*/
	for (i=0;i<table->num_cols;i++) {
#ifdef MDB_DEBUG
	/* printf("column %d\n", i);
	mdb_buffer_dump(mdb->pg_buf, cur_pos, fmt->tab_col_entry_size); */
#endif
		read_pg_if_n(mdb, col, &cur_pos, fmt->tab_col_entry_size);
		pcol = g_malloc0(sizeof(MdbColumn));

		pcol->table = table;

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
			pcol->col_scale = col[11];
			pcol->col_prec = col[12];
		}

		// col_flags_offset == 13 or 15
		pcol->is_fixed = col[fmt->col_flags_offset] & 0x01 ? 1 : 0;
		pcol->is_long_auto = col[fmt->col_flags_offset] & 0x04 ? 1 : 0;
		pcol->is_uuid_auto = col[fmt->col_flags_offset] & 0x40 ? 1 : 0;

		// tab_col_offset_fixed == 14 or 21
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
		char *tmp_buf;
		pcol = g_ptr_array_index(table->columns, i);

		if (IS_JET3(mdb))
			name_sz = read_pg_if_8(mdb, &cur_pos);
		else
			name_sz = read_pg_if_16(mdb, &cur_pos);
		tmp_buf = g_malloc(name_sz);
		read_pg_if_n(mdb, tmp_buf, &cur_pos, name_sz);
		mdb_unicode2ascii(mdb, tmp_buf, name_sz, pcol->name, sizeof(pcol->name));
		g_free(tmp_buf);
	}

	/* Sort the columns by col_num */
	g_ptr_array_sort(table->columns, (GCompareFunc)mdb_col_comparer);

	allprops = table->entry->props;
	if (allprops)
		for (i=0;i<table->num_cols;i++) {
			pcol = g_ptr_array_index(table->columns, i);
			for (j=0; j<allprops->len; ++j) {
				MdbProperties *props = g_ptr_array_index(allprops, j);
				if (props->name && !strcmp(props->name, pcol->name)) {
					pcol->props = props;
					break;
				}

			}
		}
	table->index_start = cur_pos;
	return table->columns;
}

void mdb_table_dump(MdbCatalogEntry *entry)
{
MdbTableDef *table;
MdbColumn *col;
int coln;
MdbIndex *idx;
unsigned int i, bitn;
guint32 pgnum;

	table = mdb_read_table(entry);
	fprintf(stdout,"definition page     = %lu\n",entry->table_pg);
	fprintf(stdout,"number of datarows  = %d\n",table->num_rows);
	fprintf(stdout,"number of columns   = %d\n",table->num_cols);
	fprintf(stdout,"number of indices   = %d\n",table->num_real_idxs);

	if (table->props)
		mdb_dump_props(table->props, stdout, 0);
	mdb_read_columns(table);
	mdb_read_indices(table);

	for (i=0;i<table->num_cols;i++) {
		col = g_ptr_array_index(table->columns,i);
	
		fprintf(stdout,"column %d Name: %-20s Type: %s(%d)\n",
			i, col->name,
			mdb_get_colbacktype_string(col),
			col->col_size);
		if (col->props)
			mdb_dump_props(col->props, stdout, 0);
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

const char *
mdb_table_get_prop(const MdbTableDef *table, const gchar *key) {
	if (!table->props)
		return NULL;
	return g_hash_table_lookup(table->props->hash, key);
}

const char *
mdb_col_get_prop(const MdbColumn *col, const gchar *key) {
	if (!col->props)
		return NULL;
	return g_hash_table_lookup(col->props->hash, key);
}

int mdb_col_is_shortdate(const MdbColumn *col) {
    const char *format = mdb_col_get_prop(col, "Format");
    return format && !strcmp(format, "Short Date");
}
