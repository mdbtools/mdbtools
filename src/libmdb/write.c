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

#ifdef DMALLOC
#include "dmalloc.h"
#endif


//static int mdb_copy_index_pg(MdbTableDef *table, MdbIndex *idx, MdbIndexPage *ipg);
static int mdb_add_row_to_leaf_pg(MdbTableDef *table, MdbIndex *idx, MdbIndexPage *ipg, MdbField *idx_fields);

void
_mdb_put_int16(unsigned char *buf, guint32 offset, guint32 value)
{
	buf[offset] = value % 256;
	value /= 256;
	buf[offset+1] = value % 256;
}
void
_mdb_put_int32(unsigned char *buf, guint32 offset, guint32 value)
{
	buf[offset] = value % 256;
	value /= 256;
	buf[offset+1] = value % 256;
	value /= 256;
	buf[offset+2] = value % 256;
	value /= 256;
	buf[offset+3] = value % 256;
}
ssize_t
mdb_write_pg(MdbHandle *mdb, unsigned long pg)
{
	ssize_t len;
	struct stat status;
	off_t offset = pg * mdb->fmt->pg_size;

	fstat(mdb->f->fd, &status);
	/* is page beyond current size + 1 ? */
	if (status.st_size < offset + mdb->fmt->pg_size) {
		fprintf(stderr,"offset %lu is beyond EOF\n",offset);
		return 0;
	}
	lseek(mdb->f->fd, offset, SEEK_SET);
	len = write(mdb->f->fd,mdb->pg_buf,mdb->fmt->pg_size);
	if (len==-1) {
		perror("write");
		return 0;
	} else if (len<mdb->fmt->pg_size) {
	/* fprintf(stderr,"EOF reached %d bytes returned.\n",len, mdb->pg_size); */
		return 0;
	}
	mdb->cur_pos = 0;
	return len;
}

static int 
mdb_is_col_indexed(MdbTableDef *table, int colnum)
{
	unsigned int i, j;
	MdbIndex *idx;

	for (i=0;i<table->num_idxs;i++) {
		idx = g_ptr_array_index (table->indices, i);
		for (j=0;j<idx->num_keys;j++) {
			if (idx->key_col_num[j]==colnum) return 1;
		}
	}
	return 0;
}
static int
mdb_crack_row4(MdbTableDef *table, int row_start, int row_end, MdbField *fields)
{
	MdbCatalogEntry *entry = table->entry;
	MdbHandle *mdb = entry->mdb;
	MdbColumn *col;
	unsigned char *pg_buf = mdb->pg_buf;
	unsigned int i;
	unsigned int row_var_cols=0, row_fixed_cols, row_cols;
	unsigned int fixed_cols_found;
	unsigned int col_start;
	unsigned char *nullmask;
	unsigned int bitmask_sz;
	unsigned int byte_num, bit_num;
	unsigned int *var_col_offsets = NULL;

	if (mdb_get_option(MDB_DEBUG_ROW)) {
		buffer_dump(pg_buf, row_start, row_end);
	}

	row_cols = mdb_pg_get_int16(mdb, row_start);

	bitmask_sz = (row_cols + 7) / 8;
	nullmask = &pg_buf[row_end - bitmask_sz + 1];

	/* read table of variable column locations */
	if (table->num_var_cols > 0) {
		row_var_cols = mdb_pg_get_int16(mdb, row_end - bitmask_sz - 1);
		var_col_offsets = (int *)g_malloc((row_var_cols+1)*sizeof(int));
		for (i=0; i<row_var_cols+1; i++) {
			var_col_offsets[i] = mdb_pg_get_int16(mdb,
				row_end - bitmask_sz - 3 - (i*2));
		}
	}
	fixed_cols_found = 0;
	row_fixed_cols = row_cols - row_var_cols;

	/* read information into fields[] */
	for (i=0;i<table->num_cols;i++) {
		col = g_ptr_array_index(table->columns,i);
		fields[i].colnum = i;
		fields[i].is_fixed = (mdb_is_fixed_col(col)) ? 1 : 0;
		byte_num = col->col_num / 8;
		bit_num = col->col_num % 8;
		/* logic on nulls is reverse, 1 is not null, 0 is null */
		fields[i].is_null = nullmask[byte_num] & (1 << bit_num) ? 0 : 1;

		if ((fields[i].is_fixed)
		 && (fixed_cols_found < row_fixed_cols)) {
			col_start = col->fixed_offset + 2;
			fields[i].start = row_start + col_start;
			fields[i].value = &pg_buf[row_start + col_start];
			fields[i].siz = col->col_size;
			fixed_cols_found++;
		/* Use col->var_col_num because a deleted column is still
		 * present in the variable column offsets table for the row */
		} else if ((!fields[i].is_fixed)
		 && (col->var_col_num < row_var_cols)) {
			col_start = var_col_offsets[col->var_col_num];
			fields[i].start = row_start + col_start;
			fields[i].value = &pg_buf[row_start + col_start];
			fields[i].siz = var_col_offsets[(col->var_col_num)+1] -
		                col_start;
		} else {
			fields[i].start = 0;
			fields[i].value = NULL;
			fields[i].siz = 0;
			fields[i].is_null = 1;
		}
	}
	g_free(var_col_offsets);

	return row_cols;
}
static int
mdb_crack_row3(MdbTableDef *table, int row_start, int row_end, MdbField *fields)
{
	MdbCatalogEntry *entry = table->entry;
	MdbHandle *mdb = entry->mdb;
	MdbColumn *col;
	unsigned char *pg_buf = mdb->pg_buf;
	unsigned int i;
	unsigned int row_var_cols = 0, row_fixed_cols, row_cols;
	unsigned int fixed_cols_found, var_cols_found;
	unsigned int col_start;
	unsigned char *nullmask;
	unsigned int bitmask_sz;
	unsigned int byte_num, bit_num;
	unsigned int *var_col_offsets = NULL;
	unsigned int num_jumps = 0, jumps_used = 0;
	unsigned int col_ptr, row_len;

	if (mdb_get_option(MDB_DEBUG_ROW)) {
		buffer_dump(pg_buf, row_start, row_end);
	}

	row_cols = pg_buf[row_start];

	bitmask_sz = (row_cols + 7) / 8;
	nullmask = &pg_buf[row_end - bitmask_sz + 1];

	/* read table of variable column locations */
	if (table->num_var_cols > 0) {
		row_var_cols = pg_buf[row_end - bitmask_sz];
		row_len = row_end - row_start + 1;
		num_jumps = (row_len - 1) / 256;
		col_ptr = row_end - bitmask_sz - num_jumps - 1;
		/* If last jump is a dummy value, ignore it */
		if ((col_ptr-row_start-row_var_cols)/256 < num_jumps)
			num_jumps--;

		var_col_offsets = (int *)g_malloc((row_var_cols+1)*sizeof(int));
		jumps_used = 0;
		for (i=0; i<row_var_cols+1; i++) {
			if ((jumps_used < num_jumps)
			 && (i == pg_buf[row_end-bitmask_sz-jumps_used-1])) {
				jumps_used++;
			}
			var_col_offsets[i] = pg_buf[col_ptr-i]+(jumps_used*256);
		}
	}
	fixed_cols_found = 0;
	var_cols_found = 0;
	row_fixed_cols = row_cols - row_var_cols;

	if (mdb_get_option(MDB_DEBUG_ROW)) {
		fprintf(stdout,"bitmask_sz %d num_jumps %d\n",bitmask_sz, num_jumps);
		fprintf(stdout,"row_var_cols %d\n", row_var_cols);
		fprintf(stdout,"row_fixed_cols %d\n", row_fixed_cols);
	}

	/* read information into fields[] */
	for (i=0;i<table->num_cols;i++) {
		col = g_ptr_array_index (table->columns, i);
		fields[i].colnum = i;
		fields[i].is_fixed = (mdb_is_fixed_col(col)) ? 1 : 0;
		byte_num = col->col_num / 8;
		bit_num = col->col_num % 8;
		/* logic on nulls is reverse, 1 is not null, 0 is null */
		fields[i].is_null = nullmask[byte_num] & (1 << bit_num) ? 0 : 1;

		if ((fields[i].is_fixed)
		 && (fixed_cols_found < row_fixed_cols)) {
			col_start = col->fixed_offset + 1;
			fields[i].start = row_start + col_start;
			fields[i].value = &pg_buf[row_start + col_start];
			fields[i].siz = col->col_size;
			fixed_cols_found++;
		} else if ((!fields[i].is_fixed)
		   && (var_cols_found < row_var_cols)) {
			col_start = var_col_offsets[var_cols_found];
			fields[i].start = row_start + col_start;
			fields[i].value = &pg_buf[row_start + col_start];
			fields[i].siz = var_col_offsets[var_cols_found+1] -
				col_start;
			var_cols_found++;
		} else {
			fields[i].start = 0;
			fields[i].value = NULL;
			fields[i].siz = 0;
			fields[i].is_null = 1;
		}
	}
	g_free(var_col_offsets);

	return row_cols;
}
/**
 * mdb_crack_row:
 * @table: Table that the row belongs to
 * @row_start: offset to start of row on current page
 * @row_end: offset to end of row on current page
 * @fields: pointer to MdbField array to be popluated by mdb_crack_row
 *
 * Cracks a row buffer apart into its component fields.  
 * 
 * A row buffer is that portion of a data page which contains the values for
 * that row.  Its beginning and end can be found in the row offset table.
 *
 * The resulting MdbField array contains pointers into the row for each field 
 * present.  Be aware that by modifying field[]->value, you would be modifying 
 * the row buffer itself, not a copy.
 *
 * This routine is mostly used internally by mdb_fetch_row() but may have some
 * applicability for advanced application programs.
 *
 * Return value: number of fields present.
 */
int
mdb_crack_row(MdbTableDef *table, int row_start, int row_end, MdbField *fields)
{
MdbCatalogEntry *entry = table->entry;
MdbHandle *mdb = entry->mdb;

	if (IS_JET4(mdb)) {
		return mdb_crack_row4(table, row_start, row_end, fields);
	} else {
		return mdb_crack_row3(table, row_start, row_end, fields);
	}
}

static int
mdb_pack_null_mask(unsigned char *buffer, int num_fields, MdbField *fields)
{
	int pos = 0, bit = 0, byte = 0;
	int i;

	/* 'Not null' bitmap */
	for (i=0; i<num_fields; i++) {
		/* column is null if bit is clear (0) */
		if (!fields[i].is_null) {
			byte |= 1 << bit;
			//printf("%d %d %d %d\n", i, bit, 1 << bit, byte);
		}
		bit++;
		if (bit==8) {
			buffer[pos++] = byte;
			bit = byte = 0;
		}
	}
	/* if we've written any bits to the current byte, flush it */
	if (bit)
		buffer[pos++] = byte;
	
	return pos;
}
/* fields must be ordered with fixed columns first, then vars, subsorted by 
 * column number */
static int
mdb_pack_row4(MdbTableDef *table, unsigned char *row_buffer, unsigned int num_fields, MdbField *fields)
{
	unsigned int pos = 0;
	unsigned int var_cols = 0;
	unsigned int i;

	row_buffer[pos++] = num_fields & 0xff;
	row_buffer[pos++] = (num_fields >> 8) & 0xff; 

	/* Fixed length columns */
	for (i=0;i<num_fields;i++) {
		if (fields[i].is_fixed) {
			fields[i].offset = pos;
			if (!fields[i].is_null) {
				memcpy(&row_buffer[pos], fields[i].value, fields[i].siz);
			}
			pos += fields[i].siz;
		}
	}
	/* For tables without variable-length columns */
	if (table->num_var_cols == 0) {
		pos += mdb_pack_null_mask(&row_buffer[pos], num_fields, fields);
		return pos;
	}
	/* Variable length columns */
	for (i=0;i<num_fields;i++) {
		if (!fields[i].is_fixed) {
			var_cols++;
			fields[i].offset = pos;
			if (! fields[i].is_null) {
				memcpy(&row_buffer[pos], fields[i].value, fields[i].siz);
				pos += fields[i].siz;
			}
		}
	}
	/* EOD */
	row_buffer[pos] = pos & 0xff;
	row_buffer[pos+1] = (pos >> 8) & 0xff;
	pos += 2;

	/* Offsets of the variable-length columns */
	for (i=num_fields; i>0; i--) {
		if (!fields[i-1].is_fixed) {
			row_buffer[pos++] = fields[i-1].offset & 0xff;
			row_buffer[pos++] = (fields[i-1].offset >> 8) & 0xff;
		}
	}
	/* Number of variable-length columns */
	row_buffer[pos++] = var_cols & 0xff;
	row_buffer[pos++] = (var_cols >> 8) & 0xff;

	pos += mdb_pack_null_mask(&row_buffer[pos], num_fields, fields);
	return pos;
}

static int
mdb_pack_row3(MdbTableDef *table, unsigned char *row_buffer, unsigned int num_fields, MdbField *fields)
{
	unsigned int pos = 0;
	unsigned int var_cols = 0;
	unsigned int i, j;
	unsigned char *offset_high;

	row_buffer[pos++] = num_fields;

	/* Fixed length columns */
	for (i=0;i<num_fields;i++) {
		if (fields[i].is_fixed) {
			fields[i].offset = pos;
			if (!fields[i].is_null) {
				memcpy(&row_buffer[pos], fields[i].value, fields[i].siz);
			}
			pos += fields[i].siz;
		}
	}
	/* For tables without variable-length columns */
	if (table->num_var_cols == 0) {
		pos += mdb_pack_null_mask(&row_buffer[pos], num_fields, fields);
		return pos;
	}
	/* Variable length columns */
	for (i=0;i<num_fields;i++) {
		if (!fields[i].is_fixed) {
			var_cols++;
			fields[i].offset = pos;
			if (! fields[i].is_null) {
				memcpy(&row_buffer[pos], fields[i].value, fields[i].siz);
				pos += fields[i].siz;
			}
		}
	}

	offset_high = (unsigned char *) g_malloc(var_cols+1);
	offset_high[0] = (pos << 8) & 0xff;
	j = 1;
	
	/* EOD */
	row_buffer[pos] = pos & 0xff;
	pos++;

	/* Variable length column offsets */
	for (i=num_fields; i>0; i--) {
		if (!fields[i-1].is_fixed) {
			row_buffer[pos++] = fields[i-1].offset & 0xff;
			offset_high[j++] = (fields[i-1].offset << 8) & 0xff;
		}
	}

	/* Dummy jump table entry */
	if (offset_high[0] < (pos+(num_fields+7)/8-1)/255) {
		row_buffer[pos++] = 0xff;
	}
	/* Jump table */
	for (i=0; i<var_cols; i++) {
		if (offset_high[i] > offset_high[i+1]) {
			row_buffer[pos++] = var_cols-i;
		}
	}
	g_free(offset_high);

	row_buffer[pos++] = var_cols;

	pos += mdb_pack_null_mask(&row_buffer[pos], num_fields, fields);
	return pos;
}
int
mdb_pack_row(MdbTableDef *table, unsigned char *row_buffer, int unsigned num_fields, MdbField *fields)
{
	if (table->is_temp_table) {
		unsigned int i;
		for (i=0; i<num_fields; i++) {
			MdbColumn *c = g_ptr_array_index(table->columns, i);
			fields[i].is_null = (fields[i].value) ? 0 : 1;
			fields[i].colnum = i;
			fields[i].is_fixed = c->is_fixed;
			if ((c->col_type != MDB_TEXT)
			 && (c->col_type != MDB_MEMO)) {
				fields[i].siz = c->col_size;
			}
		}
	}
	if (IS_JET4(table->entry->mdb)) {
		return mdb_pack_row4(table, row_buffer, num_fields, fields);
	} else {
		return mdb_pack_row3(table, row_buffer, num_fields, fields);
	}
}
int
mdb_pg_get_freespace(MdbHandle *mdb)
{
	int rows, free_start, free_end;
	int row_count_offset = mdb->fmt->row_count_offset;

	rows = mdb_pg_get_int16(mdb, row_count_offset);
	free_start = row_count_offset + 2 + (rows * 2);
        free_end = mdb_pg_get_int16(mdb, row_count_offset + (rows * 2));
	mdb_debug(MDB_DEBUG_WRITE,"free space left on page = %d", free_end - free_start);
	return (free_end - free_start);
}
unsigned char *
mdb_new_leaf_pg(MdbCatalogEntry *entry)
{
	MdbHandle *mdb = entry->mdb;
	unsigned char *new_pg;

	new_pg = (unsigned char *) g_malloc0(mdb->fmt->pg_size);
		
	new_pg[0]=0x04;
	new_pg[1]=0x01;
	_mdb_put_int32(new_pg, 4, entry->table_pg);
	
	return new_pg;
}
unsigned char *
mdb_new_data_pg(MdbCatalogEntry *entry)
{
	MdbFormatConstants *fmt = entry->mdb->fmt;
	unsigned char *new_pg;

	new_pg = (unsigned char *) g_malloc0(fmt->pg_size);
		
	new_pg[0]=0x01;
	new_pg[1]=0x01;
	_mdb_put_int16(new_pg, 2, fmt->pg_size - fmt->row_count_offset - 2);
	_mdb_put_int32(new_pg, 4, entry->table_pg);
	
	return new_pg;
}

int
mdb_update_indexes(MdbTableDef *table, int num_fields, MdbField *fields, guint32 pgnum, guint16 rownum)
{
	unsigned int i;
	MdbIndex *idx;
	
	for (i=0;i<table->num_idxs;i++) {
		idx = g_ptr_array_index (table->indices, i);
		mdb_debug(MDB_DEBUG_WRITE,"Updating %s (%d).", idx->name, idx->index_type);
		if (idx->index_type==1) {
			mdb_update_index(table, idx, num_fields, fields, pgnum, rownum);
		}
	}
	return 1;
}

int
mdb_init_index_chain(MdbTableDef *table, MdbIndex *idx)
{
	MdbCatalogEntry *entry = table->entry;
	MdbHandle *mdb = entry->mdb;

	table->scan_idx = idx;
	table->chain = g_malloc0(sizeof(MdbIndexChain));
	table->mdbidx = mdb_clone_handle(mdb);
	mdb_read_pg(table->mdbidx, table->scan_idx->first_pg);

	return 1;
}

int
mdb_update_index(MdbTableDef *table, MdbIndex *idx, unsigned int num_fields, MdbField *fields, guint32 pgnum, guint16 rownum)
{
	MdbCatalogEntry *entry = table->entry;
	MdbHandle *mdb = entry->mdb;
	int idx_xref[16];
	unsigned int i, j;
	MdbIndexChain *chain;
	MdbField idx_fields[10];

	for (i = 0; i < idx->num_keys; i++) {
		for (j = 0; j < num_fields; j++) {
			// key_col_num is 1 based, can't remember why though
			if (fields[j].colnum == idx->key_col_num[i]-1) {
				idx_xref[i] = j;
				idx_fields[i] = fields[j];
			}
		}
	}
	for (i = 0; i < idx->num_keys; i++) {
		fprintf(stdout, "key col %d (%d) is mapped to field %d (%d %d)\n",
			i, idx->key_col_num[i], idx_xref[i], fields[idx_xref[i]].colnum, 
			fields[idx_xref[i]].siz);
	}
	for (i = 0; i < num_fields; i++) {
		fprintf(stdout, "%d (%d %d)\n",
			i, fields[i].colnum, 
			fields[i].siz);
	}

	chain = g_malloc0(sizeof(MdbIndexChain));

	mdb_index_find_row(mdb, idx, chain, pgnum, rownum);
	printf("chain depth = %d\n", chain->cur_depth);
	printf("pg = %" G_GUINT32_FORMAT "\n",
		chain->pages[chain->cur_depth-1].pg);
	//mdb_copy_index_pg(table, idx, &chain->pages[chain->cur_depth-1]);
	mdb_add_row_to_leaf_pg(table, idx, &chain->pages[chain->cur_depth-1], idx_fields);
	
	return 1;
}

int
mdb_insert_row(MdbTableDef *table, int num_fields, MdbField *fields)
{
	int new_row_size;
	unsigned char row_buffer[4096];
	MdbCatalogEntry *entry = table->entry;
	MdbHandle *mdb = entry->mdb;
	MdbFormatConstants *fmt = mdb->fmt;
	guint32 pgnum;
	guint16 rownum;

	if (!mdb->f->writable) {
		fprintf(stderr, "File is not open for writing\n");
		return 0;
	}
	new_row_size = mdb_pack_row(table, row_buffer, num_fields, fields);
	if (mdb_get_option(MDB_DEBUG_WRITE)) {
		buffer_dump(row_buffer, 0, new_row_size-1);
	}
	pgnum = mdb_map_find_next_freepage(table, new_row_size);
	if (!pgnum) {
		fprintf(stderr, "Unable to allocate new page.\n");
		return 0;
	}

	rownum = mdb_add_row_to_pg(table, row_buffer, new_row_size);

	if (mdb_get_option(MDB_DEBUG_WRITE)) {
		buffer_dump(mdb->pg_buf, 0, 39);
		buffer_dump(mdb->pg_buf, fmt->pg_size - 160, fmt->pg_size-1);
	}
	mdb_debug(MDB_DEBUG_WRITE, "writing page %d", pgnum);
	if (!mdb_write_pg(mdb, pgnum)) {
		fprintf(stderr, "write failed! exiting...\n");
		exit(1);
	}

	mdb_update_indexes(table, num_fields, fields, pgnum, rownum);
 
	return 1;
}
/*
 * Assumes caller has verfied space is available on page and adds the new 
 * row to the current pg_buf.
 */
guint16
mdb_add_row_to_pg(MdbTableDef *table, unsigned char *row_buffer, int new_row_size)
{
	unsigned char *new_pg;
	int num_rows, i, pos, row_start, row_end, row_size;
	MdbCatalogEntry *entry = table->entry;
	MdbHandle *mdb = entry->mdb;
	MdbFormatConstants *fmt = mdb->fmt;

	if (table->is_temp_table) {
		GPtrArray *pages = table->temp_table_pages;
		if (pages->len == 0) {
			new_pg = mdb_new_data_pg(entry);
			g_ptr_array_add(pages, new_pg);
		} else {
			new_pg = g_ptr_array_index(pages, pages->len - 1);
			if (mdb_get_int16(new_pg, 2) < new_row_size + 2) {
				new_pg = mdb_new_data_pg(entry);
				g_ptr_array_add(pages, new_pg);
			}
		}

		num_rows = mdb_get_int16(new_pg, fmt->row_count_offset);
		pos = (num_rows == 0) ? fmt->pg_size :
			mdb_get_int16(new_pg, fmt->row_count_offset + (num_rows*2));
	} else {  /* is not a temp table */
		new_pg = mdb_new_data_pg(entry);

		num_rows = mdb_pg_get_int16(mdb, fmt->row_count_offset);
		pos = fmt->pg_size;

		/* copy existing rows */
		for (i=0;i<num_rows;i++) {
			row_start = mdb_pg_get_int16(mdb, (fmt->row_count_offset + 2) + (i*2));
			row_end = mdb_find_end_of_row(mdb, i);
			row_size = row_end - row_start + 1;
			pos -= row_size;
			memcpy(&new_pg[pos], &mdb->pg_buf[row_start], row_size);
			_mdb_put_int16(new_pg, (fmt->row_count_offset + 2) + (i*2), pos);
		}
	}

	/* add our new row */
	pos -= new_row_size;
	memcpy(&new_pg[pos], row_buffer, new_row_size);
	/* add row to the row offset table */
	_mdb_put_int16(new_pg, (fmt->row_count_offset + 2) + (num_rows*2), pos);

	/* update number rows on this page */
	num_rows++;
	_mdb_put_int16(new_pg, fmt->row_count_offset, num_rows);

	/* update the freespace */
	_mdb_put_int16(new_pg,2,pos - fmt->row_count_offset - 2 - (num_rows*2));

	/* copy new page over old */
	if (!table->is_temp_table) {
		memcpy(mdb->pg_buf, new_pg, fmt->pg_size);
		g_free(new_pg);
	}

	return num_rows;
}
int 
mdb_update_row(MdbTableDef *table)
{
int row_start, row_end;
unsigned int i;
MdbColumn *col;
MdbCatalogEntry *entry = table->entry;
MdbHandle *mdb = entry->mdb;
MdbFormatConstants *fmt = mdb->fmt;
MdbField fields[256];
unsigned char row_buffer[4096];
int old_row_size, new_row_size, delta;
unsigned int num_fields;

	if (!mdb->f->writable) {
		fprintf(stderr, "File is not open for writing\n");
		return 0;
	}
	row_start = mdb_pg_get_int16(mdb, (fmt->row_count_offset + 2) + ((table->cur_row-1)*2)); 
	row_end = mdb_find_end_of_row(mdb, table->cur_row-1);
	old_row_size = row_end - row_start;

	row_start &= 0x0FFF; /* remove flags */

	mdb_debug(MDB_DEBUG_WRITE,"page %lu row %d start %d end %d", (unsigned long) table->cur_phys_pg, table->cur_row-1, row_start, row_end);
	if (mdb_get_option(MDB_DEBUG_LIKE))
		buffer_dump(mdb->pg_buf, row_start, row_end);

	for (i=0;i<table->num_cols;i++) {
		col = g_ptr_array_index(table->columns,i);
		if (col->bind_ptr && mdb_is_col_indexed(table,i)) {
			fprintf(stderr, "Attempting to update column that is part of an index\n");
			return 0;
		}
	}
	num_fields = mdb_crack_row(table, row_start, row_end, fields);

	if (mdb_get_option(MDB_DEBUG_WRITE)) {
		for (i=0;i<num_fields;i++) {
			printf("col %d %d start %d siz %d\n", i, fields[i].colnum, fields[i].start, fields[i].siz);
		}
	}
	for (i=0;i<table->num_cols;i++) {
		col = g_ptr_array_index(table->columns,i);
		if (col->bind_ptr) {
			printf("yes\n");
			fields[i].value = col->bind_ptr;
			fields[i].siz = *(col->len_ptr);
		}
	}

	new_row_size = mdb_pack_row(table, row_buffer, num_fields, fields);
	if (mdb_get_option(MDB_DEBUG_WRITE)) 
		buffer_dump(row_buffer, 0, new_row_size-1);
	delta = new_row_size - old_row_size;
	if ((mdb_pg_get_freespace(mdb) - delta) < 0) {
		fprintf(stderr, "No space left on this page, update will not occur\n");
		return 0;
	}
	/* do it! */
	mdb_replace_row(table, table->cur_row-1, row_buffer, new_row_size);
	return 0;
}
int 
mdb_replace_row(MdbTableDef *table, int row, unsigned char *new_row, int new_row_size)
{
MdbCatalogEntry *entry = table->entry;
MdbHandle *mdb = entry->mdb;
MdbFormatConstants *fmt = mdb->fmt;
unsigned char *new_pg;
guint16 num_rows;
int row_start, row_end, row_size;
int i, pos;

	if (mdb_get_option(MDB_DEBUG_WRITE)) {
		buffer_dump(mdb->pg_buf, 0, 39);
		buffer_dump(mdb->pg_buf, fmt->pg_size - 160, fmt->pg_size-1);
	}
	mdb_debug(MDB_DEBUG_WRITE,"updating row %d on page %lu", row, (unsigned long) table->cur_phys_pg);
	new_pg = mdb_new_data_pg(entry);

	num_rows = mdb_pg_get_int16(mdb, fmt->row_count_offset);
	_mdb_put_int16(new_pg, fmt->row_count_offset, num_rows);

	pos = mdb->fmt->pg_size;

	/* rows before */
	for (i=0;i<row;i++) {
		row_start = mdb_pg_get_int16(mdb, (fmt->row_count_offset + 2) + (i*2));
		row_end = mdb_find_end_of_row(mdb, i);
		row_size = row_end - row_start + 1;
		pos -= row_size;
		memcpy(&new_pg[pos], &mdb->pg_buf[row_start], row_size);
		_mdb_put_int16(new_pg, (fmt->row_count_offset + 2) + (i*2), pos);
	}
	
	/* our row */
	pos -= new_row_size;
	memcpy(&new_pg[pos], new_row, new_row_size);
	_mdb_put_int16(new_pg, (fmt->row_count_offset + 2) + (row*2), pos);
	
	/* rows after */
	for (i=row+1;i<num_rows;i++) {
		row_start = mdb_pg_get_int16(mdb, (fmt->row_count_offset + 2) + (i*2));
		row_end = mdb_find_end_of_row(mdb, i);
		row_size = row_end - row_start + 1;
		pos -= row_size;
		memcpy(&new_pg[pos], &mdb->pg_buf[row_start], row_size);
		_mdb_put_int16(new_pg, (fmt->row_count_offset + 2) + (i*2), pos);
	}

	/* almost done, copy page over current */
	memcpy(mdb->pg_buf, new_pg, fmt->pg_size);

	g_free(new_pg);

	_mdb_put_int16(mdb->pg_buf, 2, mdb_pg_get_freespace(mdb));
	if (mdb_get_option(MDB_DEBUG_WRITE)) {
		buffer_dump(mdb->pg_buf, 0, 39);
		buffer_dump(mdb->pg_buf, fmt->pg_size - 160, fmt->pg_size-1);
	}
	/* drum roll, please */
	if (!mdb_write_pg(mdb, table->cur_phys_pg)) {
		fprintf(stderr, "write failed! exiting...\n");
		exit(1);
	}
	return 0;
}
static int
mdb_add_row_to_leaf_pg(MdbTableDef *table, MdbIndex *idx, MdbIndexPage *ipg, MdbField *idx_fields) 
/*,  guint32 pgnum, guint16 rownum) 
static int
mdb_copy_index_pg(MdbTableDef *table, MdbIndex *idx, MdbIndexPage *ipg)
*/
{
	MdbCatalogEntry *entry = table->entry;
	MdbHandle *mdb = entry->mdb;
	MdbColumn *col;
	guint32 pg;
	guint16 row;
	unsigned char *new_pg;
	unsigned char key_hash[256];
	unsigned char iflag;
	int keycol;

	new_pg = mdb_new_leaf_pg(entry);

	mdb_index_page_reset(ipg);
	mdb_read_pg(mdb, ipg->pg);

	/* do we support this index type yet? */
	if (idx->num_keys > 1) {
		fprintf(stderr,"multikey indexes not yet supported, aborting\n");
		return 0;
	}
	keycol = idx->key_col_num[0];
	col = g_ptr_array_index (table->columns, keycol - 1);
	printf("keycol = %d (%s)\n", keycol, col->name);
	if (!mdb_is_fixed_col(col)) {
		fprintf(stderr,"variable length key columns not yet supported, aborting\n");
		return 0;
	}
	printf("col size = %d\n", col->col_size);

	while (mdb_index_find_next_on_page(mdb, ipg)) {

		/* check for compressed indexes.  */
		if (ipg->len < col->col_size + 1) {
			fprintf(stderr,"compressed indexes not yet supported, aborting\n");
			return 0;
		}

		pg = mdb_pg_get_int24_msb(mdb, ipg->offset + ipg->len - 4);
		row = mdb->pg_buf[ipg->offset + ipg->len - 1];
		iflag = mdb->pg_buf[ipg->offset];
		mdb_index_swap_n(&mdb->pg_buf[ipg->offset + 1], col->col_size, key_hash);
		key_hash[col->col_size - 1] &= 0x7f;
		printf("length = %d\n", ipg->len);
		printf("iflag = %d pg = %" G_GUINT32_FORMAT
			" row = %" G_GUINT16_FORMAT "\n", iflag, pg, row);
		buffer_dump(mdb->pg_buf, ipg->offset, ipg->offset + ipg->len - 1);
		buffer_dump(mdb->pg_buf, ipg->offset + 1, ipg->offset + col->col_size);
		buffer_dump(key_hash, 0, col->col_size - 1);
		ipg->offset += ipg->len;
		ipg->len = 0;
		row++;
	}
	g_free(new_pg);

	return ipg->len;
}
