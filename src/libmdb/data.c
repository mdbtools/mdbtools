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

#define _XOPEN_SOURCE
#include <time.h>
#include <math.h>
#include "mdbtools.h"

#ifdef DMALLOC
#include "dmalloc.h"
#endif

#define OFFSET_MASK 0x1fff

char *mdb_money_to_string(MdbHandle *mdb, int start);
char *mdb_numeric_to_string(MdbHandle *mdb, int start, int prec, int scale);

static int _mdb_attempt_bind(MdbHandle *mdb, 
	MdbColumn *col, unsigned char isnull, int offset, int len);
static char *mdb_date_to_string(void *buf, int start);
#ifdef MDB_COPY_OLE
static size_t mdb_copy_ole(MdbHandle *mdb, void *dest, int start, int size);
#endif

static char date_fmt[64] = "%x %X";
static int noleap_cal[] = {0,31,59,90,120,151,181,212,243,273,304,334,365};
static int leap_cal[]   = {0,31,60,91,121,152,182,213,244,274,305,335,366};


void mdb_set_date_fmt(const char *fmt)
{
		date_fmt[63] = 0; 
		strncpy(date_fmt, fmt, 63);
}

/* Some databases (eg PostgreSQL) do not understand integer 0/1 values
 * as TRUE/FALSE, so provide a means to override the values used to be
 * the SQL Standard TRUE/FALSE values.
 */
static char boolean_false_number[] = "0";
static char boolean_true_number[]  = "1";

static char boolean_false_word[]   = "FALSE";
static char boolean_true_word[]    = "TRUE";

static char *boolean_false_value   = boolean_false_number;
static char *boolean_true_value    = boolean_true_number;

void mdb_set_boolean_fmt_words()
{
	boolean_false_value = boolean_false_word;
	boolean_true_value  = boolean_true_word;
}

void mdb_bind_column(MdbTableDef *table, int col_num, void *bind_ptr, int *len_ptr)
{
	MdbColumn *col;

	/* 
	** the column arrary is 0 based, so decrement to get 1 based parameter 
	*/
	col=g_ptr_array_index(table->columns, col_num - 1);
	
	if (bind_ptr)
		col->bind_ptr = bind_ptr;
	if (len_ptr)
		col->len_ptr = len_ptr;
}
int
mdb_bind_column_by_name(MdbTableDef *table, gchar *col_name, void *bind_ptr, int *len_ptr)
{
	unsigned int i;
	int col_num = -1;
	MdbColumn *col;
	
	for (i=0;i<table->num_cols;i++) {
		col=g_ptr_array_index(table->columns,i);
		if (!g_ascii_strcasecmp(col->name,col_name)) {
			col_num = i + 1;
			if (bind_ptr)
				col->bind_ptr = bind_ptr;
			if (len_ptr)
				col->len_ptr = len_ptr;
			break;
		}
	}

	return col_num;
}

/**
 * mdb_find_pg_row
 * @mdb: Database file handle
 * @pg_row: Lower byte contains the row number, the upper three contain page
 * @buf: Pointer for returning a pointer to the page
 * @off: Pointer for returning an offset to the row
 * @len: Pointer for returning the length of the row
 * 
 * Returns: 0 on success. -1 on failure.
 */
int mdb_find_pg_row(MdbHandle *mdb, int pg_row, void **buf, int *off, size_t *len)
{
	unsigned int pg = pg_row >> 8;
	unsigned int row = pg_row & 0xff;
    int result = 0;

	if (mdb_read_alt_pg(mdb, pg) != mdb->fmt->pg_size)
		return -1;
	mdb_swap_pgbuf(mdb);
	result = mdb_find_row(mdb, row, off, len);
	mdb_swap_pgbuf(mdb);
    *off &= OFFSET_MASK;
	*buf = mdb->alt_pg_buf;
	return result;
}

int mdb_find_row(MdbHandle *mdb, int row, int *start, size_t *len)
{
	int rco = mdb->fmt->row_count_offset;
	int next_start;

	if (row > 1000) return -1;

	*start = mdb_get_int16(mdb->pg_buf, rco + 2 + row*2);
	next_start = (row == 0) ? mdb->fmt->pg_size :
		mdb_get_int16(mdb->pg_buf, rco + row*2) & OFFSET_MASK;
	*len = next_start - (*start & OFFSET_MASK);

	if ((*start & OFFSET_MASK) >= mdb->fmt->pg_size ||
			(*start & OFFSET_MASK) > next_start ||
			next_start > mdb->fmt->pg_size)
		return -1;

	return 0;
}

int 
mdb_find_end_of_row(MdbHandle *mdb, int row)
{
	int rco = mdb->fmt->row_count_offset;
	int row_end;

#if 1
	if (row > 1000) return -1;

	row_end = (row == 0) ? mdb->fmt->pg_size :
		mdb_get_int16(mdb->pg_buf, rco + row*2) & OFFSET_MASK;
#else
	/* Search the previous "row start" values for the first non-'lookupflag'
	 * one. If we don't find one, then the end of the page is the correct
	 * value.
	 */
	int i, row_start;

	if (row > 1000) return -1;

	/* if lookupflag is not set, it's good (deleteflag is ok) */
	for (i = row; i > 0; i--) {
		row_start = mdb_get_int16(mdb->pg_buf, (rco + i*2));
		if (!(row_start & 0x8000)) {
			break;
		}
	}

	row_end = (i == 0) ? mdb->fmt->pg_size : row_start & OFFSET_MASK;
#endif
	return row_end - 1;
}
int mdb_is_null(unsigned char *null_mask, int col_num)
{
int byte_num = (col_num - 1) / 8;
int bit_num = (col_num - 1) % 8;

	if ((1 << bit_num) & null_mask[byte_num]) {
		return 0;
	} else {
		return 1;
	}
}
/* bool has to be handled specially because it uses the null bit to store its 
** value*/
static size_t 
mdb_xfer_bound_bool(MdbHandle *mdb, MdbColumn *col, int value)
{
	col->cur_value_len = value;
	if (col->bind_ptr) {
		strcpy(col->bind_ptr,
                       value ? boolean_false_value : boolean_true_value);
	}
	if (col->len_ptr) {
		*col->len_ptr = strlen(col->bind_ptr);
	}

	return 1;
}
static size_t
mdb_xfer_bound_ole(MdbHandle *mdb, int start, MdbColumn *col, int len)
{
	size_t ret = 0;
	if (len) {
		col->cur_value_start = start;
		col->cur_value_len = len;
	} else {
		col->cur_value_start = 0;
		col->cur_value_len = 0;
	}
#ifdef MDB_COPY_OLE
	if (col->bind_ptr || col->len_ptr) {
		ret = mdb_copy_ole(mdb, col->bind_ptr, start, len);
	}
#else
	if (col->bind_ptr) {
		memcpy(col->bind_ptr, mdb->pg_buf + start, MDB_MEMO_OVERHEAD);
	}
	ret = MDB_MEMO_OVERHEAD;
#endif
	if (col->len_ptr) {
		*col->len_ptr = ret;
	}
	return ret;
}
static size_t
mdb_xfer_bound_data(MdbHandle *mdb, int start, MdbColumn *col, int len)
{
int ret;
	//if (!strcmp("Name",col->name)) {
		//printf("start %d %d\n",start, len);
	//}
	if (len) {
		col->cur_value_start = start;
		col->cur_value_len = len;
	} else {
		col->cur_value_start = 0;
		col->cur_value_len = 0;
	}
	if (col->bind_ptr) {
		if (!len) {
			strcpy(col->bind_ptr, "");
		} else {
			//fprintf(stdout,"len %d size %d\n",len, col->col_size);
			char *str;
			if (col->col_type == MDB_NUMERIC) {
				str = mdb_numeric_to_string(mdb, start, col->col_scale, col->col_prec);
			} else {
				str = mdb_col_to_string(mdb, mdb->pg_buf, start, col->col_type, len);
			}
			snprintf(col->bind_ptr, MDB_BIND_SIZE, "%s", str);
			g_free(str);
		}
		ret = strlen(col->bind_ptr);
		if (col->len_ptr) {
			*col->len_ptr = ret;
		}
		return ret;
	}
	return 0;
}
int mdb_read_row(MdbTableDef *table, unsigned int row)
{
	MdbHandle *mdb = table->entry->mdb;
	MdbColumn *col;
	unsigned int i;
	int row_start;
	size_t row_size;
	int delflag, lookupflag;
	MdbField fields[256];
	int num_fields;

	if (table->num_rows == 0) 
		return 0;

	if (mdb_find_row(mdb, row, &row_start, &row_size)) {
		fprintf(stderr, "warning: mdb_find_row failed.\n");
		return 0;
	}
    if (row_size == 0) {
		fprintf(stderr, "warning: row_size = 0.\n");
		return 0;
    }

	delflag = lookupflag = 0;
	if (row_start & 0x8000) lookupflag++;
	if (row_start & 0x4000) delflag++;
	row_start &= OFFSET_MASK; /* remove flags */
#if MDB_DEBUG
	fprintf(stdout,"Row %d bytes %d to %d %s %s\n", 
		row, row_start, row_start + row_size - 1,
		lookupflag ? "[lookup]" : "",
		delflag ? "[delflag]" : "");
#endif	

	if (!table->noskip_del && delflag) {
		return 0;
	}

	num_fields = mdb_crack_row(table, row_start, row_size, fields);
	if (num_fields < 0)
		return 0;
	if (!mdb_test_sargs(table, fields, num_fields)) return 0;
	
#if MDB_DEBUG
	fprintf(stdout,"sarg test passed row %d \n", row);
#endif 

#if MDB_DEBUG
	mdb_buffer_dump(mdb->pg_buf, row_start, row_size);
#endif

	/* take advantage of mdb_crack_row() to clean up binding */
	/* use num_cols instead of num_fields -- bsb 03/04/02 */
	for (i = 0; i < table->num_cols; i++) {
		col = g_ptr_array_index(table->columns,fields[i].colnum);
		_mdb_attempt_bind(mdb, col, fields[i].is_null,
			fields[i].start, fields[i].siz);
	}

	return 1;
}
static int _mdb_attempt_bind(MdbHandle *mdb, 
	MdbColumn *col, 
	unsigned char isnull, 
	int offset, 
	int len)
{
	if (col->col_type == MDB_BOOL) {
		mdb_xfer_bound_bool(mdb, col, isnull);
	} else if (isnull) {
		mdb_xfer_bound_data(mdb, 0, col, 0);
	} else if (col->col_type == MDB_OLE) {
		mdb_xfer_bound_ole(mdb, offset, col, len);
	} else {
		//if (!mdb_test_sargs(mdb, col, offset, len)) {
			//return 0;
		//}
		mdb_xfer_bound_data(mdb, offset, col, len);
	}
	return 1;
}

/* Read next data page into mdb->pg_buf */
int mdb_read_next_dpg(MdbTableDef *table)
{
	MdbCatalogEntry *entry = table->entry;
	MdbHandle *mdb = entry->mdb;
	int next_pg;

#ifndef SLOW_READ
	while (1) {
		next_pg = mdb_map_find_next(mdb, table->usage_map,
			table->map_sz, table->cur_phys_pg);
		if (next_pg < 0)
			break; /* unknow map type: goto fallback */
		if (!next_pg)
			return 0;

		if (!mdb_read_pg(mdb, next_pg)) {
			fprintf(stderr, "error: reading page %d failed.\n", next_pg);
			return 0;
		}

		table->cur_phys_pg = next_pg;
		if (mdb->pg_buf[0]==MDB_PAGE_DATA && mdb_get_int32(mdb->pg_buf, 4)==(long)entry->table_pg)
			return table->cur_phys_pg;

		/* On rare occasion, mdb_map_find_next will return a wrong page */
		/* Found in a big file, over 4,000,000 records */
		fprintf(stderr,
			"warning: page %d from map doesn't match: Type=%d, buf[4..7]=%ld Expected table_pg=%ld\n",
			next_pg, mdb->pg_buf[0], mdb_get_int32(mdb->pg_buf, 4), entry->table_pg);
	}
	fprintf(stderr, "Warning: defaulting to brute force read\n");
#endif 
	/* can't do a fast read, go back to the old way */
	do {
		if (!mdb_read_pg(mdb, table->cur_phys_pg++))
			return 0;
	} while (mdb->pg_buf[0]!=MDB_PAGE_DATA || mdb_get_int32(mdb->pg_buf, 4)!=(long)entry->table_pg);
	/* fprintf(stderr,"returning new page %ld\n", table->cur_phys_pg); */
	return table->cur_phys_pg;
}
int mdb_rewind_table(MdbTableDef *table)
{
	table->cur_pg_num=0;
	table->cur_phys_pg=0;
	table->cur_row=0;

	return 0;
}
int 
mdb_fetch_row(MdbTableDef *table)
{
	MdbHandle *mdb = table->entry->mdb;
	MdbFormatConstants *fmt = mdb->fmt;
	unsigned int rows;
	int rc;
	guint32 pg;

	if (table->num_rows==0)
		return 0;

	/* initialize */
	if (!table->cur_pg_num) {
		table->cur_pg_num=1;
		table->cur_row=0;
		if ((!table->is_temp_table)&&(table->strategy!=MDB_INDEX_SCAN))
			if (!mdb_read_next_dpg(table)) return 0;
	}

	do {
		if (table->is_temp_table) {
			GPtrArray *pages = table->temp_table_pages;
			rows = mdb_get_int16(
				g_ptr_array_index(pages, table->cur_pg_num-1),
				fmt->row_count_offset);
			if (table->cur_row >= rows) {
				table->cur_row = 0;
				if (++table->cur_pg_num > (unsigned int)pages->len)
					return 0;
			}
			memcpy(mdb->pg_buf,
				g_ptr_array_index(pages, table->cur_pg_num-1),
				fmt->pg_size);
		} else if (table->strategy==MDB_INDEX_SCAN) {
		
			if (!mdb_index_find_next(table->mdbidx, table->scan_idx, table->chain, &pg, (guint16 *) &(table->cur_row))) {
				mdb_index_scan_free(table);
				return 0;
			}
			mdb_read_pg(mdb, pg);
		} else {
			rows = mdb_get_int16(mdb->pg_buf,fmt->row_count_offset);

			/* if at end of page, find a new data page */
			if (table->cur_row >= rows) {
				table->cur_row=0;
	
				if (!mdb_read_next_dpg(table)) {
					return 0;
				}
			}
		}

		/* printf("page %d row %d\n",table->cur_phys_pg, table->cur_row); */
		rc = mdb_read_row(table, table->cur_row);
		table->cur_row++;
	} while (!rc);

	return 1;
}
void mdb_data_dump(MdbTableDef *table)
{
	unsigned int i;
	char *bound_values[MDB_MAX_COLS]; 

	for (i=0;i<table->num_cols;i++) {
		bound_values[i] = (char *) g_malloc(256);
		mdb_bind_column(table, i+1, bound_values[i], NULL);
	}
	mdb_rewind_table(table);
	while (mdb_fetch_row(table)) {
		for (i=0;i<table->num_cols;i++) {
			fprintf(stdout, "column %d is %s\n", i+1, bound_values[i]);
		}
	}
	for (i=0;i<table->num_cols;i++) {
		g_free(bound_values[i]);
	}
}

int mdb_is_fixed_col(MdbColumn *col)
{
	return col->is_fixed;
}
#if 0
static char *mdb_data_to_hex(MdbHandle *mdb, char *text, int start, int size) 
{
int i;

	for (i=start; i<start+size; i++) {
		sprintf(&text[(i-start)*2],"%02x", mdb->pg_buf[i]);
	}
	text[(i-start)*2]='\0';

	return text;
}
#endif
/*
 * ole_ptr should point to the original blob value of the field.
 * If omited, there will be no multi-page check to that the caller is
 * responsible for not calling this function. Then, it doesn't have to
 * preserve the original value.
 */
size_t 
mdb_ole_read_next(MdbHandle *mdb, MdbColumn *col, void *ole_ptr)
{
	guint32 ole_len;
	void *buf;
	int row_start;
	size_t len;

	if (ole_ptr) {
		ole_len = mdb_get_int32(ole_ptr, 0);
		mdb_debug(MDB_DEBUG_OLE,"ole len = %d ole flags = %02x",
		    ole_len & 0x00ffffff, ole_len >> 24);

		if ((ole_len & 0x80000000)
		 || (ole_len & 0x40000000))
			/* inline or single-page fields don't have a next */
			return 0;
	}
	mdb_debug(MDB_DEBUG_OLE, "pg_row %d", col->cur_blob_pg_row);
	if (!col->cur_blob_pg_row)
		return 0; /* we are done */
	if (mdb_find_pg_row(mdb, col->cur_blob_pg_row,
		&buf, &row_start, &len)) {
		return 0;
	}
	if (len < 4)
		return 0;
	mdb_debug(MDB_DEBUG_OLE,"start %d len %d", row_start, len);

	if (col->bind_ptr)
		memcpy(col->bind_ptr, (char*)buf + row_start + 4, len - 4);
	col->cur_blob_pg_row = mdb_get_int32(buf, row_start);

	return len - 4;
}
size_t 
mdb_ole_read(MdbHandle *mdb, MdbColumn *col, void *ole_ptr, size_t chunk_size)
{
	guint32 ole_len;
	void *buf;
	int row_start;
	size_t len;

	ole_len = mdb_get_int32(ole_ptr, 0);
	mdb_debug(MDB_DEBUG_OLE,"ole len = %d ole flags = %02x",
		ole_len & 0x00ffffff, ole_len >> 24);

	col->chunk_size = chunk_size;

	if (ole_len & 0x80000000) {
		/* inline ole field, if we can satisfy it, then do it */
		len = col->cur_value_len - MDB_MEMO_OVERHEAD;
		if (chunk_size < len)
			return 0;
		if (col->bind_ptr)
			memcpy(col->bind_ptr, &mdb->pg_buf[col->cur_value_start +
					MDB_MEMO_OVERHEAD], len);
		return len;
	} else if (ole_len & 0x40000000) {
		col->cur_blob_pg_row = mdb_get_int32(ole_ptr, 4);
		mdb_debug(MDB_DEBUG_OLE,"ole row = %d ole pg = %ld",
			col->cur_blob_pg_row & 0xff,
			col->cur_blob_pg_row >> 8);

		if (mdb_find_pg_row(mdb, col->cur_blob_pg_row,
			&buf, &row_start, &len)) {
			return 0;
		}
		mdb_debug(MDB_DEBUG_OLE,"start %d len %d", row_start, len);

		if (col->bind_ptr) {
			memcpy(col->bind_ptr, (char*)buf + row_start, len);
			if (mdb_get_option(MDB_DEBUG_OLE))
				mdb_buffer_dump(col->bind_ptr, 0, 16);
		}
		return len;
	} else if ((ole_len & 0xf0000000) == 0) {
		col->cur_blob_pg_row = mdb_get_int32(ole_ptr, 4);
		mdb_debug(MDB_DEBUG_OLE,"ole row = %d ole pg = %ld",
			col->cur_blob_pg_row & 0xff,
			col->cur_blob_pg_row >> 8);

		if (mdb_find_pg_row(mdb, col->cur_blob_pg_row,
			&buf, &row_start, &len) || len < 4) {
			return 0;
		}
		mdb_debug(MDB_DEBUG_OLE,"start %d len %d", row_start, len);

		if (col->bind_ptr) 
			memcpy(col->bind_ptr, (char*)buf + row_start + 4, len - 4);
		col->cur_blob_pg_row = mdb_get_int32(buf, row_start);
		mdb_debug(MDB_DEBUG_OLE, "next pg_row %d", col->cur_blob_pg_row);

		return len - 4;
	} else {
		fprintf(stderr,"Unhandled ole field flags = %02x\n", ole_len >> 24);
		return 0;
	}
}
/*
 * mdb_ole_read_full calls mdb_ole_read then loop over mdb_ole_read_next as much as necessary.
 * returns the result in a big buffer.
 * The call must free it.
 * Note that this function is not indempotent: It may be called only once per column after each bind.
 */
void*
mdb_ole_read_full(MdbHandle *mdb, MdbColumn *col, size_t *size)
{
	char ole_ptr[MDB_MEMO_OVERHEAD];
	char *result = malloc(MDB_BIND_SIZE * 64);
	size_t result_buffer_size = MDB_BIND_SIZE * 64;
	size_t len, pos;

	memcpy(ole_ptr, col->bind_ptr, MDB_MEMO_OVERHEAD);

	len = mdb_ole_read(mdb, col, ole_ptr, MDB_BIND_SIZE * 64);
	memcpy(result, col->bind_ptr, len);
	pos = len;
	while ((len = mdb_ole_read_next(mdb, col, ole_ptr))) {
		if (pos+len >= result_buffer_size) {
			result_buffer_size += MDB_BIND_SIZE * 64;
			result = realloc(result, result_buffer_size);
		}
		memcpy(result + pos, col->bind_ptr, len);
		pos += len;
	}
	if (size)
		*size = pos;
	return result;
}

#ifdef MDB_COPY_OLE
static size_t mdb_copy_ole(MdbHandle *mdb, void *dest, int start, int size)
{
	guint32 ole_len;
	gint32 row_start, pg_row;
	size_t len;
	void *buf, *pg_buf = mdb->pg_buf;

	if (size<MDB_MEMO_OVERHEAD) {
		return 0;
	} 

	/* The 16 bit integer at offset 0 is the length of the memo field.
	 * The 32 bit integer at offset 4 contains page and row information.
	 */
	ole_len = mdb_get_int32(pg_buf, start);

	if (ole_len & 0x80000000) {
		/* inline */
		len = size - MDB_MEMO_OVERHEAD;
		if (dest) memcpy(dest, pg_buf + start + MDB_MEMO_OVERHEAD, len);
		return len;
	} else if (ole_len & 0x40000000) {
		/* single page */
		pg_row = mdb_get_int32(pg_buf, start+4);
		mdb_debug(MDB_DEBUG_OLE,"Reading LVAL page %06x", pg_row >> 8);

		if (mdb_find_pg_row(mdb, pg_row, &buf, &row_start, &len)) {
			return 0;
		}
		mdb_debug(MDB_DEBUG_OLE,"row num %d start %d len %d",
			pg_row & 0xff, row_start, len);

		if (dest)
			memcpy(dest, buf + row_start, len);
		return len;
	} else if ((ole_len & 0xff000000) == 0) { // assume all flags in MSB
		/* multi-page */
		int cur = 0;
		pg_row = mdb_get_int32(pg_buf, start+4);
		do {
			mdb_debug(MDB_DEBUG_OLE,"Reading LVAL page %06x",
				pg_row >> 8);

			if (mdb_find_pg_row(mdb,pg_row,&buf,&row_start,&len) || len < 4) {
				return 0;
			}

			mdb_debug(MDB_DEBUG_OLE,"row num %d start %d len %d",
				pg_row & 0xff, row_start, len);

			if (dest) 
				memcpy(dest+cur, buf + row_start + 4, len - 4);
			cur += len - 4;

			/* find next lval page */
			pg_row = mdb_get_int32(buf, row_start);
		} while ((pg_row >> 8));
		return cur;
	} else {
		fprintf(stderr, "Unhandled ole field flags = %02x\n", ole_len >> 24);
		return 0;
	}
}
#endif
static char *mdb_memo_to_string(MdbHandle *mdb, int start, int size)
{
	guint32 memo_len;
	gint32 row_start, pg_row;
	size_t len;
	void *buf, *pg_buf = mdb->pg_buf;
	char *text = (char *) g_malloc(MDB_BIND_SIZE);

	if (size<MDB_MEMO_OVERHEAD) {
		strcpy(text, "");
		return text;
	} 

#if MDB_DEBUG
	mdb_buffer_dump(pg_buf, start, MDB_MEMO_OVERHEAD);
#endif

	/* The 32 bit integer at offset 0 is the length of the memo field
	 *   with some flags in the high bits.
	 * The 32 bit integer at offset 4 contains page and row information.
	 */
	memo_len = mdb_get_int32(pg_buf, start);

	if (memo_len & 0x80000000) {
		/* inline memo field */
		mdb_unicode2ascii(mdb, (char*)pg_buf + start + MDB_MEMO_OVERHEAD,
			size - MDB_MEMO_OVERHEAD, text, MDB_BIND_SIZE);
		return text;
	} else if (memo_len & 0x40000000) {
		/* single-page memo field */
		pg_row = mdb_get_int32(pg_buf, start+4);
#if MDB_DEBUG
		printf("Reading LVAL page %06x\n", pg_row >> 8);
#endif
		if (mdb_find_pg_row(mdb, pg_row, &buf, &row_start, &len)) {
			strcpy(text, "");
			return text;
		}
#if MDB_DEBUG
		printf("row num %d start %d len %d\n",
			pg_row & 0xff, row_start, len);
		mdb_buffer_dump(buf, row_start, len);
#endif
		mdb_unicode2ascii(mdb, (char*)buf + row_start, len, text, MDB_BIND_SIZE);
		return text;
	} else if ((memo_len & 0xff000000) == 0) { // assume all flags in MSB
		/* multi-page memo field */
		guint32 tmpoff = 0;
		char *tmp;

		tmp = (char *) g_malloc(memo_len);
		pg_row = mdb_get_int32(pg_buf, start+4);
		do {
#if MDB_DEBUG
			printf("Reading LVAL page %06x\n", pg_row >> 8);
#endif
			if (mdb_find_pg_row(mdb,pg_row,&buf,&row_start,&len)) {
				g_free(tmp);
				strcpy(text, "");
				return text;
			}
#if MDB_DEBUG
			printf("row num %d start %d len %d\n",
				pg_row & 0xff, row_start, len);
#endif
			if (tmpoff + len - 4 > memo_len)
				break;

			/* Stop processing on zero length multiple page memo fields */
			if (len < 4)
				break;

			memcpy(tmp + tmpoff, (char*)buf + row_start + 4, len - 4);
			tmpoff += len - 4;
		} while (( pg_row = mdb_get_int32(buf, row_start) ));
		if (tmpoff < memo_len) {
			fprintf(stderr, "Warning: incorrect memo length\n");
		}
		mdb_unicode2ascii(mdb, tmp, tmpoff, text, MDB_BIND_SIZE);
		g_free(tmp);
		return text;
	} else {
		fprintf(stderr, "Unhandled memo field flags = %02x\n", memo_len >> 24);
		strcpy(text, "");
		return text;
	}
}

#if 0
static int trim_trailing_zeros(char * buff)
{
	char *p;
	int n = strlen(buff);

	/* Don't need to trim strings with no decimal portion */
	if(!strchr(buff,'.'))
		return 0;

	/* Trim the zeros */
	p = buff + n - 1;
	while (p >= buff && *p == '0')
		*p-- = '\0';

	/* If a decimal sign is left at the end, remove it too */
	if (*p == '.')
		*p = '\0';

	return 0;
}
#endif


/* Date/Time is stored as a double, where the whole
   part is the days from 12/30/1899 and the fractional
   part is the fractional part of one day. */

void
mdb_tm_to_date(struct tm *t, double *td)
{
	short yr = t->tm_year + 1900;
	char leap = ((yr & 3) == 0) && ((yr % 100) != 0 || (yr % 400) == 0);
	int *cal = leap ? leap_cal : noleap_cal;
	long int time = (yr*365+(yr/4)-(yr/100)+(yr/400)+cal[t->tm_mon]+t->tm_mday)-693959;

	*td = (((long)t->tm_hour * 3600)+((long)t->tm_min * 60)+((long)t->tm_sec)) / 86400.0;
	if (time>=0) *td+=time; else *td=time-*td;
}

void
mdb_date_to_tm(double td, struct tm *t)
{
	long int day, time;
	int yr, q;
	int *cal;

	day = (long int)(td);
	time = (long int)(fabs(td - day) * 86400.0 + 0.5);
	t->tm_hour = time / 3600;
	t->tm_min = (time / 60) % 60;
	t->tm_sec = time % 60;
	t->tm_year = 1 - 1900;

	day += 693593; /* Days from 1/1/1 to 12/31/1899 */
	t->tm_wday = (day+1) % 7;

	q = day / 146097;  /* 146097 days in 400 years */
	t->tm_year += 400 * q;
	day -= q * 146097;

	q = day / 36524;  /* 36524 days in 100 years */
	if (q > 3) q = 3;
	t->tm_year += 100 * q;
	day -= q * 36524;

	q = day / 1461;  /* 1461 days in 4 years */
	t->tm_year += 4 * q;
	day -= q * 1461;

	q = day / 365;  /* 365 days in 1 year */
	if (q > 3) q = 3;
	t->tm_year += q;
	day -= q * 365;

	yr = t->tm_year + 1900;
	cal = ((yr)%4==0 && ((yr)%100!=0 || (yr)%400==0)) ?
		leap_cal : noleap_cal;
	for (t->tm_mon=0; t->tm_mon<12; t->tm_mon++) {
		if (day < cal[t->tm_mon+1]) break;
	}
	t->tm_mday = day - cal[t->tm_mon] + 1;
	t->tm_yday = day;
	t->tm_isdst = -1;
}

static char *
mdb_date_to_string(void *buf, int start)
{
	struct tm t;
	char *text = (char *) g_malloc(MDB_BIND_SIZE);
	double td = mdb_get_double(buf, start);

	mdb_date_to_tm(td, &t);

	strftime(text, MDB_BIND_SIZE, date_fmt, &t);

	return text;
}

static char *
mdb_uuid_to_string(MdbHandle *mdb, int start)
{
	char *text = NULL;
  unsigned short uuid1, uuid2, uuid3, uuid4, uuid5, uuid6, uuid7, uuid8;

  uuid1 = mdb_get_int16(mdb->pg_buf, start);
  uuid2 = mdb_get_int16(mdb->pg_buf, start + 2);
  uuid3 = mdb_get_int16(mdb->pg_buf, start + 4);
  uuid4 = mdb_get_int16(mdb->pg_buf, start + 6);
  uuid5 = mdb_get_int16(mdb->pg_buf, start + 8);
  uuid6 = mdb_get_int16(mdb->pg_buf, start + 10);
  uuid7 = mdb_get_int16(mdb->pg_buf, start + 12);
  uuid8 = mdb_get_int16(mdb->pg_buf, start + 14);

  text = g_strdup_printf("{%04x%04x-%04x-%04x-%04x-%04x%04x%04x}",
    uuid1, uuid2, uuid3, uuid4, uuid5, uuid6, uuid7, uuid8);

	return text;
}

#if 0
int floor_log10(double f, int is_single)
{
	unsigned int i;
	double y = 10.0;

	if (f < 0.0)
		f = -f;
	
	if ((f == 0.0) || (f == 1.0) || isinf(f)) {
		return 0;
	} else if (f < 1.0) {
		if (is_single) {
			/* The intermediate value p is necessary to prevent
			 * promotion of the comparison to type double */
			float p;
			for (i=1; (p = f * y) < 1.0; i++)
				y *= 10.0;
		} else {
			for (i=1; f * y < 1.0; i++)
				y *= 10.0;
		}
		return -(int)i;
	} else {  /* (x > 1.0) */
		for (i=0; f >= y; i++)
			y *= 10.0;
		return (int)i;
	}
}
#endif

char *mdb_col_to_string(MdbHandle *mdb, void *buf, int start, int datatype, int size)
{
	char *text = NULL;
	float tf;
	double td;

	switch (datatype) {
		case MDB_BOOL:
			/* shouldn't happen.  bools are handled specially
			** by mdb_xfer_bound_bool() */
		break;
		case MDB_BYTE:
			text = g_strdup_printf("%d", mdb_get_byte(buf, start));
		break;
		case MDB_INT:
			text = g_strdup_printf("%hd",
				(short)mdb_get_int16(buf, start));
		break;
		case MDB_LONGINT:
		case MDB_COMPLEX:
			text = g_strdup_printf("%ld",
				mdb_get_int32(buf, start));
		break;
		case MDB_FLOAT:
			tf = mdb_get_single(buf, start);
			text = g_strdup_printf("%.8e", tf);
		break;
		case MDB_DOUBLE:
			td = mdb_get_double(buf, start);
			text = g_strdup_printf("%.16e", td);
		break;
		case MDB_BINARY:
			if (size<0) {
				text = g_strdup("");
			} else {
				text = g_malloc(size);
				memcpy(text, (char*)buf+start, size);
			}
		break;
		case MDB_TEXT:
			if (size<0) {
				text = g_strdup("");
			} else {
				text = (char *) g_malloc(MDB_BIND_SIZE);
				mdb_unicode2ascii(mdb, (char*)buf + start,
					size, text, MDB_BIND_SIZE);
			}
		break;
		case MDB_DATETIME:
			text = mdb_date_to_string(buf, start);
		break;
		case MDB_MEMO:
			text = mdb_memo_to_string(mdb, start, size);
		break;
		case MDB_MONEY:
			text = mdb_money_to_string(mdb, start);
		case MDB_NUMERIC:
		break;
		case MDB_REPID:
		  text = mdb_uuid_to_string(mdb, start);
		break;
		default:
			text = g_strdup("");
		break;
	}
	return text;
}
int mdb_col_disp_size(MdbColumn *col)
{
	switch (col->col_type) {
		case MDB_BOOL:
			return 1;
		break;
		case MDB_BYTE:
			return 4;
		break;
		case MDB_INT:
			return 6;
		break;
		case MDB_LONGINT:
		case MDB_COMPLEX:
			return 11;
		break;
		case MDB_FLOAT:
			return 10;
		break;
		case MDB_DOUBLE:
			return 10;
		break;
		case MDB_TEXT:
			return col->col_size;
		break;
		case MDB_DATETIME:
			return 20;
		break;
		case MDB_MEMO:
			return 64000; 
		break;
		case MDB_MONEY:
			return 21;
		break;
	}
	return 0;
}
int mdb_col_fixed_size(MdbColumn *col)
{
	switch (col->col_type) {
		case MDB_BOOL:
			return 1;
		break;
		case MDB_BYTE:
			return -1;
		break;
		case MDB_INT:
			return 2;
		break;
		case MDB_LONGINT:
		case MDB_COMPLEX:
			return 4;
		break;
		case MDB_FLOAT:
			return 4;
		break;
		case MDB_DOUBLE:
			return 8;
		break;
		case MDB_TEXT:
			return -1;
		break;
		case MDB_DATETIME:
			return 4;
		break;
		case MDB_BINARY:
			return -1;
		break;
		case MDB_MEMO:
			return -1; 
		break;
		case MDB_MONEY:
			return 8;
		break;
	}
	return 0;
}
