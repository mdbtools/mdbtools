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
	unsigned int i;
	int var_cols = 0, row_var_cols, fixed_cols = 0, row_fixed_cols, num_cols;
	int var_cols_found, fixed_cols_found, var_entry_pos;
	int col_start, next_col;
	unsigned char *nullmask;
	int bitmask_sz;
	int byte_num, bit_num;
	int eod, len; /* end of data */
	int real_offset = 0;
	int row_pos;

	if (mdb_get_option(MDB_DEBUG_ROW)) {
		buffer_dump(mdb->pg_buf, row_start, row_end+1);
	}

	num_cols = mdb_pg_get_int16(mdb, row_start);

	/* compute nulls first to help with fixed colnum's */
	bitmask_sz = (num_cols - 1) / 8 + 1;
	nullmask = &mdb->pg_buf[row_end - bitmask_sz + 1];

	for (i=0;i<table->num_cols;i++) {
		col = g_ptr_array_index (table->columns, i);
		row_pos = col->col_num;
		byte_num = row_pos / 8;
		bit_num = row_pos % 8;
		/* logic on nulls is reverse, 1 is not null, 0 is null */
		fields[i].is_null = nullmask[byte_num] & 1 << bit_num ? 0 : 1;
		//printf("row_pos %d col %d is %s\n", row_pos, i, fields[i].is_null ? "null" : "not null");
	}

	/* fields are ordered fixed then variable */
	for (i = 0; i < table->num_cols; i++) {
		col = g_ptr_array_index (table->columns, i);
		if (mdb_is_fixed_col(col)) {
			fixed_cols++;
			fields[i].colnum = i;
			fields[i].siz = col->col_size;
			fields[i].is_fixed = 1;
		}
	}
	for (i = 0; i < table->num_cols; i++) {
		col = g_ptr_array_index (table->columns, i);
		if (!mdb_is_fixed_col(col)) {
			var_cols++;
			fields[i].colnum = i;
			fields[i].is_fixed = 0;
		}
	}
	row_var_cols = mdb_pg_get_int16(mdb, row_end - bitmask_sz - 1);

	/* find the end of data pointer */
	eod = mdb_pg_get_int16(mdb, row_end - 3 - var_cols*2 - bitmask_sz);

	/* actual cols on this row */
	fixed_cols_found = 0;
	var_cols_found = 0;
	row_fixed_cols = num_cols - row_var_cols;

	for (i=0;i<table->num_cols;i++) {
		col = g_ptr_array_index(table->columns,i);
		if (mdb_is_fixed_col(col)) {
			if (fixed_cols_found <= row_fixed_cols) {
				real_offset += col->col_size;
				fields[i].start = row_start + col->fixed_offset + 2;
				fields[i].value = &mdb->pg_buf[row_start + col->fixed_offset + 2];
			} else {
				fields[i].start = 0;
				fields[i].value = NULL;
				fields[i].siz = 0;
				fields[i].is_null = 1;
			}
			fixed_cols_found++;
		}
	}

	col_start = mdb_pg_get_int16(mdb, row_end - 3 - bitmask_sz);

	for (i=0;i<table->num_cols;i++) {
		col = g_ptr_array_index(table->columns,i);
		if (!mdb_is_fixed_col(col)) {
			var_cols_found++;
			if (var_cols_found <= row_var_cols)  {
				if (var_cols_found==row_var_cols)  {
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
				//printf("len = %d eod %d col_start %d\n",len, eod, col_start);
				//printf("is_null %d\n",fields[i].is_null);
				fields[i].start = row_start + col_start;
				fields[i].value = &mdb->pg_buf[row_start +col_start];
				fields[i].siz = len;
				col_start += len;
			} else {
				fields[i].start = 0;
				fields[i].value = NULL;
				fields[i].siz = 0;
				fields[i].is_null = 1;
			}
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
	unsigned int i;
	int var_cols = 0, fixed_cols = 0, num_cols;
	int row_var_cols = 0, row_fixed_cols = 0;
	int var_cols_found, fixed_cols_found, var_entry_pos;
	int col_start;
	unsigned char *nullmask;
	int bitmask_sz;
	int byte_num, bit_num;
	int num_of_jumps = 0, jumps_used = 0;
	int eod, len; /* end of data */
	int row_pos, col_ptr;

	if (mdb_get_option(MDB_DEBUG_ROW)) {
		buffer_dump(mdb->pg_buf, row_start, row_end+1);
	}

	num_cols = mdb->pg_buf[row_start];

	//if (num_cols != table->num_cols) {
		//fprintf(stderr,"WARNING: number of table columns does not match number of row columns, strange results may occur\n");
	//}

	/* compute nulls first to help with fixed colnum's */
	bitmask_sz = (num_cols - 1) / 8 + 1;
	nullmask = &mdb->pg_buf[row_end - bitmask_sz + 1];

	for (i=0;i<table->num_cols;i++) {
		col = g_ptr_array_index (table->columns, i);
		row_pos = col->col_num;
		byte_num = row_pos / 8;
		bit_num = row_pos % 8;
		/* logic on nulls is reverse, 1 is not null, 0 is null */
		fields[i].is_null = nullmask[byte_num] & 1 << bit_num ? 0 : 1;
		//printf("col %d is %s\n", i, fields[i].is_null ? "null" : "not null");
	}

	/* how many fixed cols? */
	for (i = 0; i < table->num_cols; i++) {
		col = g_ptr_array_index (table->columns, i);
		if (mdb_is_fixed_col(col)) {
			fixed_cols++;
			fields[i].colnum = i;
			fields[i].siz = col->col_size;
			fields[i].is_fixed = 1;
		}
	}
	/* how many var cols? */
	for (i = 0; i < table->num_cols; i++) {
		col = g_ptr_array_index (table->columns, i);
		if (!mdb_is_fixed_col(col)) {
			var_cols++;
			fields[i].colnum = i;
			fields[i].is_fixed = 0;
		}
	}

	/* find the end of data pointer */
	eod = mdb->pg_buf[row_end-1-var_cols-bitmask_sz];

	/* data starts at 1 */
	col_start = 1;

	/* actual cols on this row */
	fixed_cols_found = 0;
	var_cols_found = 0;

	/* if fixed columns add up to more than 256, we need a jump */
	if (col_start >= 256) {
		num_of_jumps++;
		jumps_used++;
		row_start = row_start + col_start - (col_start % 256);
	}

	/* compute the number of jumps (row size - overhead) / 256 
	 * but you have to include the jump table itself, thus
	 * the loop.  */
	col_start = row_start;
	while (col_start+256 < row_end-bitmask_sz-1-var_cols-num_of_jumps){
		col_start += 256;
		num_of_jumps++;
	}

	col_ptr = row_end - bitmask_sz - num_of_jumps - 1;
	if (mdb->pg_buf[col_ptr]==0xFF) {
		col_ptr--;
	}

	row_var_cols = mdb->pg_buf[row_end - bitmask_sz - num_of_jumps];
	row_fixed_cols = num_cols - row_var_cols;

	if (mdb_get_option(MDB_DEBUG_ROW)) {
		fprintf(stdout,"bitmask_sz %d num_of_jumps %d\n",bitmask_sz, num_of_jumps);
		fprintf(stdout,"var_cols %d row_var_cols %d\n",var_cols, row_var_cols);
		fprintf(stdout,"fixed_cols %d row_fixed_cols %d\n",fixed_cols, row_fixed_cols);
	}
	/* loop through fixed columns and add values to fields[] */
	for (i=0;i<table->num_cols;i++) {
		col = g_ptr_array_index(table->columns,i);
		if (mdb_is_fixed_col(col)) {
			if (fixed_cols_found <= row_fixed_cols) {
				fields[i].start = row_start + col->fixed_offset + 1;
				fields[i].value = &mdb->pg_buf[row_start + col->fixed_offset + 1];
				if (col->col_type != MDB_BOOL)
					col_start += col->col_size;
			} else {
				fields[i].start = 0;
				fields[i].value = NULL;
				fields[i].siz = 0;
				fields[i].is_null = 1;
			}
			fixed_cols_found++;
		}
	}

	/* col_start is now the offset to the first variable length field */
	col_start = mdb->pg_buf[col_ptr];
	//fprintf(stdout,"col_ptr %d col_start %d\n",col_ptr, col_start);

	for (i=0;i<table->num_cols;i++) {
		col = g_ptr_array_index(table->columns,i);
		/* if it's a var_col and we aren't looking at a column
		 * added after this row was created */
		if (!mdb_is_fixed_col(col)) {
			var_cols_found++;
			if (var_cols_found <= row_var_cols) {

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
				if (var_cols_found==row_var_cols)  {
					len=eod - col_start;
					//printf("len = %d eod %d col_start %d\n",len, eod, col_start);
				} else  {
					var_entry_pos = 
						row_end - 
						bitmask_sz -
						var_cols_found - 1;
					len=mdb->pg_buf[var_entry_pos] - col_start;
				} /* if found==var_cols */
				while (len<0) len+=256;
				fields[i].start = row_start + col_start;
				fields[i].value = &mdb->pg_buf[row_start +col_start];
				fields[i].siz = len;
				col_start += len;
			} else {
				fields[i].start = 0;
				fields[i].value = NULL;
				fields[i].siz = 0;
				fields[i].is_null = 1;
			}
		} /* if !fixed */
	} /* for */
	return num_cols;

}
/**
 * mdb_crack_row:
 * @mdb: Database handle
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
			memcpy(&row_buffer[pos], fields[i].value, fields[i].siz);
			pos += fields[i].siz;
		}
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
	for (i=num_fields; i>num_fields-var_cols; i--) {
		row_buffer[pos++] = fields[i-1].offset & 0xff;
		row_buffer[pos++] = (fields[i-1].offset >> 8) & 0xff;
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
	unsigned int i;

	row_buffer[pos++] = num_fields;

	/* Fixed length columns */
	for (i=0;i<num_fields;i++) {
		if (fields[i].is_fixed) {
			fields[i].offset = pos;
			memcpy(&row_buffer[pos], fields[i].value, fields[i].siz);
			pos += fields[i].siz;
		}
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
	row_buffer[pos] = pos;
	pos++;

	for (i=num_fields;i>num_fields-var_cols;i--) {
		row_buffer[pos++] = fields[i-1].offset % 256;
	}

	row_buffer[pos++] = var_cols;

	pos += mdb_pack_null_mask(&row_buffer[pos], num_fields, fields);

	return pos;
}
int
mdb_pack_row(MdbTableDef *table, unsigned char *row_buffer, int unsigned num_fields, MdbField *fields)
{
	if (IS_JET4(table->entry->mdb)) {
		return mdb_pack_row4(table, row_buffer, num_fields, fields);
	} else {
		return mdb_pack_row3(table, row_buffer, num_fields, fields);
	}
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
mdb_update_index(MdbTableDef *table, MdbIndex *idx, int num_fields, MdbField *fields, guint32 pgnum, guint16 rownum)
{
	MdbCatalogEntry *entry = table->entry;
	MdbHandle *mdb = entry->mdb;
	int idx_xref[16];
	int i, j;
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
