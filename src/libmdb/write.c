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
size_t
mdb_write_pg(MdbHandle *mdb, unsigned long pg)
{
size_t len;
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
mdb_crack_row4(MdbTableDef *table, int row_start, int row_end, MdbField *fields)
{
MdbCatalogEntry *entry = table->entry;
MdbHandle *mdb = entry->mdb;
MdbColumn *col;
int i, j;
int var_cols = 0, fixed_cols = 0, num_cols, totcols = 0;
int var_cols_found, fixed_cols_found, var_entry_pos;
int col_start, next_col;
unsigned char *nullmask;
int bitmask_sz;
int byte_num, bit_num;
int eod, len; /* end of data */

	num_cols = mdb_pg_get_int16(mdb, row_start);

	for (i = 0; i < table->num_cols; i++) {
		col = g_ptr_array_index (table->columns, i);
		if (mdb_is_fixed_col(col)) {
			fixed_cols++;
			fields[totcols].colnum = i;
			fields[totcols].siz = col->col_size;
			fields[totcols++].is_fixed = 1;
		}
	}
	for (i = 0; i < table->num_cols; i++) {
		col = g_ptr_array_index (table->columns, i);
		if (!mdb_is_fixed_col(col)) {
			var_cols++;
			fields[totcols].colnum = i;
			fields[totcols++].is_fixed = 0;
		}
	}

	bitmask_sz = (num_cols - 1) / 8 + 1;
	nullmask = &mdb->pg_buf[row_end - bitmask_sz + 1];

	for (i=0;i<num_cols;i++) {
		byte_num = i / 8;
		bit_num = i % 8;
		/* logic on nulls is reverse, 1 is not null, 0 is null */
		fields[i].is_null = nullmask[byte_num] & 1 << bit_num ? 0 : 1;
		//printf("col %d is %s\n", i, fields[i].is_null ? "null" : "not null");
	}

	/* find the end of data pointer */
	eod = mdb_pg_get_int16(mdb, row_end - 3 - var_cols*2 - bitmask_sz);

	col_start = 2;

	/* actual cols on this row */
	fixed_cols_found = 0;
	var_cols_found = 0;

	totcols = 0;
	/* loop through fixed columns and add values to fields[] */
	for (j=0;j<table->num_cols;j++) {
		col = g_ptr_array_index(table->columns,j);
		if (mdb_is_fixed_col(col) && ++fixed_cols_found <= fixed_cols) {
			fields[totcols].start = row_start + col_start;
			fields[totcols++].value = &mdb->pg_buf[row_start + col_start];
			if (col->col_type != MDB_BOOL)
				col_start += col->col_size;
		}
	}
	for (j=0;j<table->num_cols;j++) {
		col = g_ptr_array_index(table->columns,j);
		if (!mdb_is_fixed_col(col) && ++var_cols_found <= var_cols) {
			if (var_cols_found==var_cols)  {
				len=eod - col_start;
				//printf("len = %d eod %d col_start %d\n",len, eod, col_start);
			} else  {
				/* position of the var table 
				 * entry for this column */
				var_entry_pos = 
					row_end - 
					bitmask_sz - 
					var_cols_found * 2 - 2 - 1;
				next_col = mdb_pg_get_int16(mdb, var_entry_pos);
				len = next_col - col_start;
			} /* if found==var_cols */
			while (len<0) len+=256;
			fields[totcols].start = row_start + col_start;
			fields[totcols].value = &mdb->pg_buf[row_start +col_start];
			fields[totcols++].siz = len;
			col_start += len;
		} /* if !fixed */
	} /* for */

	return num_cols;

}
static int
mdb_crack_row3(MdbTableDef *table, int row_start, int row_end, MdbField *fields)
{
MdbCatalogEntry *entry = table->entry;
MdbHandle *mdb = entry->mdb;
MdbColumn *col;
int i, j;
int var_cols = 0, fixed_cols = 0, num_cols, totcols = 0;
int var_cols_found, fixed_cols_found, var_entry_pos;
int col_start;
unsigned char *nullmask;
int bitmask_sz;
int byte_num, bit_num;
int num_of_jumps = 0, jumps_used = 0;
int eod, len; /* end of data */

	num_cols = mdb->pg_buf[row_start];
	if (num_cols != table->num_cols) {
		fprintf(stderr,"WARNING: number of table columns does not match number of row columns, strange results may occur\n");
	}

	/* how many fixed cols? */
	for (i = 0; i < table->num_cols; i++) {
		col = g_ptr_array_index (table->columns, i);
		if (mdb_is_fixed_col(col)) {
			fixed_cols++;
			fields[totcols].colnum = i;
			fields[totcols].siz = col->col_size;
			fields[totcols++].is_fixed = 1;
		}
	}
	/* how many var cols? */
	for (i = 0; i < table->num_cols; i++) {
		col = g_ptr_array_index (table->columns, i);
		if (!mdb_is_fixed_col(col)) {
			var_cols++;
			fields[totcols].colnum = i;
			fields[totcols++].is_fixed = 0;
		}
	}

	bitmask_sz = (num_cols - 1) / 8 + 1;
	nullmask = &mdb->pg_buf[row_end - bitmask_sz + 1];

	for (i=0;i<num_cols;i++) {
		byte_num = i / 8;
		bit_num = i % 8;
		/* logic on nulls is reverse, 1 is not null, 0 is null */
		fields[i].is_null = nullmask[byte_num] & 1 << bit_num ? 0 : 1;
		//printf("col %d is %s\n", i, fields[i].is_null ? "null" : "not null");
	}

	/* find the end of data pointer */
	eod = mdb->pg_buf[row_end-1-var_cols-bitmask_sz];

	/* data starts at 1 */
	col_start = 1;

	/* actual cols on this row */
	fixed_cols_found = 0;
	var_cols_found = 0;

	totcols = 0;
	/* loop through fixed columns and add values to fields[] */
	for (j=0;j<table->num_cols;j++) {
		col = g_ptr_array_index(table->columns,j);
		if (mdb_is_fixed_col(col) && ++fixed_cols_found <= fixed_cols) {
			fields[totcols].start = row_start + col_start;
			fields[totcols++].value = &mdb->pg_buf[row_start + col_start];
			if (col->col_type != MDB_BOOL)
				col_start += col->col_size;
		}
	}

	/* if fixed columns add up to more than 256, we need a jump */
	int col_ptr = row_end - bitmask_sz - num_of_jumps - 1;
	if (col_start >= 256) {
		num_of_jumps++;
		jumps_used++;
		row_start = row_start + col_start - (col_start % 256);
	}
	col_start = row_start;
	
	/* compute the number of jumps (row size - overhead) / 256 
	 * but you have to include the jump table itself, thus
	 * the loop.  */
	while (col_start+256 < row_end-bitmask_sz-1-var_cols-num_of_jumps){
		col_start += 256;
		num_of_jumps++;
	}
	if (mdb->pg_buf[col_ptr]==0xFF) {
		col_ptr--;
	}
	/* col_start is now the offset to the first variable length field */
	col_start = mdb->pg_buf[col_ptr];

	for (j=0;j<table->num_cols;j++) {
		col = g_ptr_array_index(table->columns,j);
		/* if it's a var_col and we aren't looking at a column
		 * added after this row was created */
		if (!mdb_is_fixed_col(col) && ++var_cols_found <= var_cols) {

			/* if the position of this var_col matches the number
			 * in the current jump table entry, then increment
			 * the jump_used and adjust the col/row_start */
			if (var_cols_found == mdb->pg_buf[row_end-bitmask_sz-jumps_used-1] &&
				jumps_used < num_of_jumps) {
				row_start += 256;
				col_start -= 256;
				jumps_used++;
			}

			/* if we have the last var_col, use the eod offset to
			 * figure out where the end is */
			if (var_cols_found==var_cols)  {
				len=eod - col_start;
				//printf("len = %d eod %d col_start %d\n",len, eod, col_start);
			} else  {
				var_entry_pos = 
					row_end - 
					bitmask_sz -
					var_cols_found - 1;
				len=mdb->pg_buf[var_entry_pos] - mdb->pg_buf[var_entry_pos+1];
			} /* if found==var_cols */
			while (len<0) len+=256;
			fields[totcols].start = row_start + col_start;
			fields[totcols].value = &mdb->pg_buf[row_start +col_start];
			fields[totcols++].siz = len;
			col_start += len;
		} /* if !fixed */
	} /* for */

	return num_cols;

}
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
/* fields must be ordered with fixed columns first, then vars, subsorted by 
 * column number */
int
mdb_pack_row(MdbTableDef *table, unsigned char *row_buffer, int num_fields, MdbField *fields)
{
int pos = 0;
int var_cols = 0;
unsigned char bit, byte;
int i;

	row_buffer[pos++] = num_fields;
	for (i=0;i<num_fields;i++) {
		if (fields[i].is_fixed) {
			fields[i].offset = pos;
			memcpy(&row_buffer[pos], fields[i].value, fields[i].siz);
			pos += fields[i].siz;
		}
	}
	for (i=0;i<num_fields;i++) {
		if (!fields[i].is_fixed) {
			var_cols++;
			fields[i].offset = pos;
			memcpy(&row_buffer[pos], fields[i].value, fields[i].siz);
			pos += fields[i].siz;
		}
	}

	/* EOD */
	row_buffer[pos] = pos;
	pos++;

	for (i=num_fields-1;i>=num_fields - var_cols;i--) {
		row_buffer[pos++] = fields[i].offset % 256;
	}

	row_buffer[pos++] = var_cols;

	byte = 0;
	bit = 0;
	for (i=0;i<num_fields;i++) {	
		/* column is null if bit is clear (0) */
		if (!fields[i].is_null) {
			byte |= 1 << bit;
			//printf("%d %d %d %d\n", i, bit, 1 << bit, byte);
		}
		bit++;
		if (bit==8) {
			row_buffer[pos++] = byte;
			bit=0;
			byte = 0;
		}
			
	}
	/* if we've written any bits to the current byte, flush it*/
	if (bit) 
		row_buffer[pos++] = byte;

	return pos;
}
int
mdb_pg_get_freespace(MdbHandle *mdb)
{
MdbFormatConstants *fmt = mdb->fmt;
int rows, free_start, free_end;

	rows = mdb_pg_get_int16(mdb, fmt->row_count_offset);
	free_start = fmt->row_count_offset + 2 + (rows * 2);
        free_end = mdb_pg_get_int16(mdb, (fmt->row_count_offset + rows * 2)) -1;
	mdb_debug(MDB_DEBUG_WRITE,"free space left on page = %d", free_end - free_start);
	return (free_end - free_start + 1);
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
	MdbHandle *mdb = entry->mdb;
	unsigned char *new_pg;

	new_pg = (unsigned char *) g_malloc0(mdb->fmt->pg_size);
		
	new_pg[0]=0x01;
	new_pg[1]=0x01;
	_mdb_put_int32(new_pg, 4, entry->table_pg);
	
	return new_pg;
}

int
mdb_update_indexes(MdbTableDef *table, int num_fields, MdbField *fields, guint32 pgnum, guint16 rownum)
{
	int i;
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
mdb_update_index(MdbTableDef *table, MdbIndex *idx, int num_fields, MdbField *fields, guint32 pgnum, guint16 rownum)
{
	MdbCatalogEntry *entry = table->entry;
	MdbHandle *mdb = entry->mdb;
	int idx_xref[16];
	int i, j;
	MdbIndexChain *chain;

	for (i = 0; i < idx->num_keys; i++) {
		for (j = 0; j < num_fields; j++) {
			// key_col_num is 1 based, can't remember why though
			if (fields[j].colnum == idx->key_col_num[i]-1)
				idx_xref[i] = j;
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
	printf("pg = %lu\n", chain->pages[chain->cur_depth-1].pg);
	mdb_copy_index_pg(table, &chain->pages[chain->cur_depth-1]);
	
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
	unsigned char *new_pg;

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

	new_pg = mdb_new_data_pg(entry);

	num_rows = mdb_pg_get_int16(mdb, fmt->row_count_offset);
	pos = mdb->fmt->pg_size;

	/* copy existing rows */
	for (i=0;i<num_rows;i++) {
		row_start = mdb_pg_get_int16(mdb, (fmt->row_count_offset + 2) + (i*2));
		row_end = mdb_find_end_of_row(mdb, i);
		row_size = row_end - row_start + 1;
		pos -= row_size;
		memcpy(&new_pg[pos], &mdb->pg_buf[row_start], row_size);
		_mdb_put_int16(new_pg, (fmt->row_count_offset + 2) + (i*2), pos);
	}

	/* add our new row */
	pos -= new_row_size;
	memcpy(&new_pg[pos], row_buffer, new_row_size);
	/* add row to the row offset table */
	_mdb_put_int16(new_pg, (fmt->row_count_offset + 2) + (num_rows*2), pos);

	/* update number rows on this page */
	num_rows++;
	_mdb_put_int16(new_pg, fmt->row_count_offset, num_rows);

	/* copy new page over old */
	memcpy(mdb->pg_buf, new_pg, fmt->pg_size);
	g_free(new_pg);

	/* update the freespace */
	_mdb_put_int16(mdb->pg_buf, 2, mdb_pg_get_freespace(mdb));

	return num_rows;
}
int 
mdb_update_row(MdbTableDef *table)
{
int row_start, row_end;
int i;
MdbColumn *col;
MdbCatalogEntry *entry = table->entry;
MdbHandle *mdb = entry->mdb;
MdbFormatConstants *fmt = mdb->fmt;
MdbField fields[256];
unsigned char row_buffer[4096];
int old_row_size, new_row_size, delta, num_fields;

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
int
mdb_copy_index_pg(MdbTableDef *table, MdbIndexPage *ipg)
{
	MdbCatalogEntry *entry = table->entry;
	MdbHandle *mdb = entry->mdb;
	guint32 pg;
	unsigned char *new_pg;

	new_pg = mdb_new_leaf_pg(entry);

	mdb_index_page_reset(ipg);
	mdb_read_pg(mdb, ipg->pg);
	while (mdb_index_find_next_on_page(mdb, ipg)) {
		pg = mdb_pg_get_int24_msb(mdb, ipg->offset + ipg->len - 3);
		printf("length = %d\n", ipg->len);
		buffer_dump(mdb->pg_buf, ipg->offset, ipg->offset + ipg->len - 1);
		ipg->offset += ipg->len;
		ipg->len = 0;
	}
	g_free(new_pg);

	return ipg->len;
}
