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

#define OFFSET_MASK 0x1fff

char *mdb_money_to_string(MdbHandle *mdb, int start, char *s);
static int _mdb_attempt_bind(MdbHandle *mdb, 
	MdbColumn *col, unsigned char isnull, int offset, int len);
static char *mdb_num_to_string(MdbHandle *mdb, int start, int datatype, int prec, int scale);
int mdb_copy_ole(MdbHandle *mdb, char *dest, int start, int size);

static char date_fmt[64] = "%x %X";

void mdb_set_date_fmt(const char *fmt)
{
		date_fmt[63] = 0; 
		strncpy(date_fmt, fmt, 63);
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
		if (!strcmp(col->name,col_name)) {
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
 * Returns: 0 on success.  1 on failure.
 */
int mdb_find_pg_row(MdbHandle *mdb, int pg_row, char **buf, int *off, int *len)
{
	unsigned int pg = pg_row >> 8;
	unsigned int row = pg_row & 0xff;

	if (mdb_read_alt_pg(mdb, pg) != mdb->fmt->pg_size)
		return 1;
	mdb_swap_pgbuf(mdb);
	*off = mdb_pg_get_int16(mdb, mdb->fmt->row_count_offset + 2 + (row*2));
	*len = mdb_find_end_of_row(mdb, row) - *off + 1;
	mdb_swap_pgbuf(mdb);
	*buf = mdb->alt_pg_buf;
	return 0;
}

int 
mdb_find_end_of_row(MdbHandle *mdb, int row)
{
	MdbFormatConstants *fmt = mdb->fmt;
	int row_end;

	/* Search the previous "row start" values for the first non-'lookupflag' one.
	* If we don't find one, then the end of the page is the correct value.
	*/
#if 1
	if (row==0) {
		row_end = fmt->pg_size - 1;
	} else {
		row_end = (mdb_pg_get_int16(mdb, ((fmt->row_count_offset + 2) + (row - 1) * 2)) & OFFSET_MASK) - 1;
	}
	return row_end;
#else
		int i, row_start;

		/* if lookupflag is	not set, it's good (deleteflag is ok) */
        for (i = row - 1; i >= 0; i--) {
                row_start = mdb_pg_get_int16(mdb, ((fmt->row_count_offset + 2) + i * 2));
                if (!(row_start & 0x8000)) {
                        break;
                }
        }

        if (i == -1) {
                row_end = fmt->pg_size - 1;
        } else {
                row_end = (row_start & OFFSET_MASK) - 1;
        }
	return row_end;
#endif
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
static int 
mdb_xfer_bound_bool(MdbHandle *mdb, MdbColumn *col, int value)
{
	
	col->cur_value_len = value;
	if (col->bind_ptr) {
		strcpy(col->bind_ptr,  value ? "0" : "1");
	}

	return 0;
}
static int mdb_xfer_bound_ole(MdbHandle *mdb, int start, MdbColumn *col, int len)
{
	int ret = 0;
	if (len) {
		col->cur_value_start = start;
		col->cur_value_len = len;
	} else {
		col->cur_value_start = 0;
		col->cur_value_len = 0;
	}
	if (col->bind_ptr || col->len_ptr) {
		//ret = mdb_copy_ole(mdb, col->bind_ptr, start, len);
		memcpy(col->bind_ptr, &mdb->pg_buf[start], MDB_MEMO_OVERHEAD);
	}
	if (col->len_ptr) {
		*col->len_ptr = MDB_MEMO_OVERHEAD;
	}
	return ret;
}
static int mdb_xfer_bound_data(MdbHandle *mdb, int start, MdbColumn *col, int len)
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
		} else if (col->col_type == MDB_NUMERIC) {
			//fprintf(stdout,"len %d size %d\n",len, col->col_size);
			char *str = mdb_num_to_string(mdb, start, col->col_type,
				col->col_prec, col->col_scale);
			strcpy(col->bind_ptr, str);
			g_free(str);
		} else {
			//fprintf(stdout,"len %d size %d\n",len, col->col_size);
			char *str = mdb_col_to_string(mdb, mdb->pg_buf, start,
				col->col_type, len);
			strcpy(col->bind_ptr, str);
			
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
	MdbFormatConstants *fmt = mdb->fmt;
	MdbColumn *col;
	unsigned int i;
	int rc;
	int row_start, row_end;
	int delflag, lookupflag;
	MdbField fields[256];
	int num_fields;

	if (table->num_rows == 0) 
		return 0;

	row_start = mdb_pg_get_int16(mdb, (fmt->row_count_offset + 2) + (row*2)); 
	row_end = mdb_find_end_of_row(mdb, row);

	delflag = lookupflag = 0;
	if (row_start & 0x8000) lookupflag++;
	if (row_start & 0x4000) delflag++;
	row_start &= OFFSET_MASK; /* remove flags */
#if MDB_DEBUG
	fprintf(stdout,"Row %d bytes %d to %d %s %s\n", 
		row, row_start, row_end,
		lookupflag ? "[lookup]" : "",
		delflag ? "[delflag]" : "");
#endif	

	if (!table->noskip_del && delflag) {
		row_end = row_start-1;
		return 0;
	}

	num_fields = mdb_crack_row(table, row_start, row_end, fields);
	if (!mdb_test_sargs(table, fields, num_fields)) return 0;
	
#if MDB_DEBUG
	fprintf(stdout,"sarg test passed row %d \n", row);
#endif 

#if MDB_DEBUG
	buffer_dump(mdb->pg_buf, row_start, row_end);
#endif

	/* take advantage of mdb_crack_row() to clean up binding */
	/* use num_cols instead of num_fields -- bsb 03/04/02 */
	for (i = 0; i < table->num_cols; i++) {
		col = g_ptr_array_index(table->columns,fields[i].colnum);
		rc = _mdb_attempt_bind(mdb, col, fields[i].is_null,
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
int mdb_read_next_dpg(MdbTableDef *table)
{
	MdbCatalogEntry *entry = table->entry;
	MdbHandle *mdb = entry->mdb;
	int next_pg;

#ifndef SLOW_READ
	next_pg = mdb_map_find_next(mdb, table->usage_map,
		table->map_sz, table->cur_phys_pg);

	if (next_pg >= 0) {
		if (mdb_read_pg(mdb, next_pg)) {
			table->cur_phys_pg = next_pg;
			return table->cur_phys_pg;
		} else {
			return 0;
		}
	}
	fprintf(stderr, "Warning: defaulting to brute force read\n");
#endif 
	/* can't do a fast read, go back to the old way */
	do {
		if (!mdb_read_pg(mdb, table->cur_phys_pg++))
			return 0;
	} while (mdb->pg_buf[0]!=0x01 || mdb_pg_get_int32(mdb, 4)!=entry->table_pg);
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
				table->cur_pg_num++;
				if (table->cur_pg_num > pages->len)
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
			rows = mdb_pg_get_int16(mdb,fmt->row_count_offset);

			/* if at end of page, find a new page */
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
int 
mdb_ole_read_next(MdbHandle *mdb, MdbColumn *col, void *ole_ptr)
{
	guint16 ole_len;
	guint16 ole_flags;
	char *buf;
	int pg_row, row_start;
	int len;

	ole_len = mdb_get_int16(ole_ptr, 0);
	ole_flags = mdb_get_int16(ole_ptr, 2);

	if (ole_flags == 0x8000) {
		/* inline fields don't have a next */
		return 0;
	} else if (ole_flags == 0x4000) {
		/* 0x4000 flagged ole's are contained on one page and thus 
		 * should be handled entirely with mdb_ole_read() */
		return 0;
	} else if (ole_flags == 0x0000) {
		pg_row = (col->cur_blob_pg << 8) & col->cur_blob_row;
		if (mdb_find_pg_row(mdb, pg_row, &buf, &row_start, &len)) {
			return 0;
		}
		if (col->bind_ptr)
			memcpy(col->bind_ptr, buf + row_start, len);
		pg_row = mdb_get_int32(buf, row_start);
		col->cur_blob_pg = pg_row >> 8;
		col->cur_blob_row = pg_row & 0xff;

		return len;
	}
	return 0;
}
int 
mdb_ole_read(MdbHandle *mdb, MdbColumn *col, void *ole_ptr, int chunk_size)
{
	guint16 ole_len;
	guint16 ole_flags;
	char *buf;
	int pg_row, row_start;
	int len;

	ole_len = mdb_get_int16(ole_ptr, 0);
	ole_flags = mdb_get_int16(ole_ptr, 2);
	mdb_debug(MDB_DEBUG_OLE,"ole len = %d ole flags = %08x",
		ole_len, ole_flags);

	col->chunk_size = chunk_size;

	if (ole_flags == 0x8000) {
		/* inline ole field, if we can satisfy it, then do it */
		len = col->cur_value_len - MDB_MEMO_OVERHEAD;
		if (chunk_size >= len) {
			if (col->bind_ptr) 
				memcpy(col->bind_ptr, 
					&mdb->pg_buf[col->cur_value_start + 
						MDB_MEMO_OVERHEAD],
					len);
			return len;
		} else {
			return 0;
		}
	} else if (ole_flags == 0x4000) {
		pg_row = mdb_get_int32(ole_ptr, 4);
		col->cur_blob_pg = pg_row >> 8;
		col->cur_blob_row = pg_row & 0xff;
		mdb_debug(MDB_DEBUG_OLE,"ole row = %d ole pg = %ld",
			col->cur_blob_row, col->cur_blob_pg);

		if (mdb_find_pg_row(mdb, pg_row, &buf, &row_start, &len)) {
			return 0;
		}
		mdb_debug(MDB_DEBUG_OLE,"start %d len %d", row_start, len);

		if (col->bind_ptr) {
			memcpy(col->bind_ptr, buf + row_start, len);
			if (mdb_get_option(MDB_DEBUG_OLE))
				buffer_dump(col->bind_ptr, 0, 16);
		}
		return len;
	} else if (ole_flags == 0x0000) {
		pg_row = mdb_get_int32(ole_ptr, 4);
		col->cur_blob_pg = pg_row >> 8;
		col->cur_blob_row = pg_row & 0xff;

		if (mdb_find_pg_row(mdb, pg_row, &buf, &row_start, &len)) {
			return 0;
		}

		if (col->bind_ptr) 
			memcpy(col->bind_ptr, buf + row_start, len);

		pg_row = mdb_get_int32(buf, row_start);
		col->cur_blob_pg = pg_row >> 8;
		col->cur_blob_row = pg_row & 0xff;

		return len;
	} else {
		fprintf(stderr,"Unhandled ole field flags = %04x\n", ole_flags);
		return 0;
	}
}
int mdb_copy_ole(MdbHandle *mdb, char *dest, int start, int size)
{
	guint16 ole_len;
	guint16 ole_flags;
	guint32 row_start, pg_row;
	guint32 len;
	char *buf;

	if (size<MDB_MEMO_OVERHEAD) {
		return 0;
	} 

	/* The 16 bit integer at offset 0 is the length of the memo field.
	 * The 32 bit integer at offset 4 contains page and row information.
	 */
	ole_len = mdb_pg_get_int16(mdb, start);
	ole_flags = mdb_pg_get_int16(mdb, start+2);

	if (ole_flags == 0x8000) {
		len = size - MDB_MEMO_OVERHEAD;
		/* inline ole field */
		if (dest) memcpy(dest, &mdb->pg_buf[start + MDB_MEMO_OVERHEAD],
			size - MDB_MEMO_OVERHEAD);
		return len;
	} else if (ole_flags == 0x4000) {
		pg_row = mdb_get_int32(mdb->pg_buf, start+4);
		mdb_debug(MDB_DEBUG_OLE,"Reading LVAL page %06x", pg_row >> 8);

		if (mdb_find_pg_row(mdb, pg_row, &buf, &row_start, &len)) {
			return 0;
		}
		mdb_debug(MDB_DEBUG_OLE,"row num %d start %d len %d",
			pg_row & 0xff, row_start, len);

		if (dest)
			memcpy(dest, buf + row_start, len);
		return len;
	} else if (ole_flags == 0x0000) {
		int cur = 0;
		pg_row = mdb_get_int32(mdb->pg_buf, start+4);
		mdb_debug(MDB_DEBUG_OLE,"Reading LVAL page %06x", pg_row >> 8);
		do {
			if (mdb_find_pg_row(mdb,pg_row,&buf,&row_start,&len)) {
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
		fprintf(stderr,"Unhandled ole field flags = %04x\n", ole_flags);
		return 0;
	}
}
static char *mdb_memo_to_string(MdbHandle *mdb, int start, int size)
{
	guint16 memo_len;
	static char text[MDB_BIND_SIZE];
	guint16 memo_flags;
	guint32 row_start, pg_row;
	guint32 len;
	char *buf;

	if (size<MDB_MEMO_OVERHEAD) {
		return "";
	} 

#if MDB_DEBUG
	buffer_dump(mdb->pg_buf, start, start + 12);
#endif

	/* The 16 bit integer at offset 0 is the length of the memo field.
	 * The 32 bit integer at offset 4 contains page and row information.
	 */
	memo_len = mdb_pg_get_int16(mdb, start);
	memo_flags = mdb_pg_get_int16(mdb, start+2);

	if (memo_flags & 0x8000) {
		/* inline memo field */
		mdb_unicode2ascii(mdb, &mdb->pg_buf[start + MDB_MEMO_OVERHEAD],
			size - MDB_MEMO_OVERHEAD, text, MDB_BIND_SIZE);
		return text;
	} else if (memo_flags & 0x4000) {
		pg_row = mdb_get_int32(mdb->pg_buf, start+4);
#if MDB_DEBUG
		printf("Reading LVAL page %06x\n", pg_row >> 8);
#endif
		if (mdb_find_pg_row(mdb, pg_row, &buf, &row_start, &len)) {
			return "";
		}
#if MDB_DEBUG
		printf("row num %d start %d len %d\n",
			pg_row & 0xff, row_start, len);
		buffer_dump(mdb->pg_buf, row_start, row_start + len);
#endif
		mdb_unicode2ascii(mdb, buf + row_start, len, text, MDB_BIND_SIZE);
		return text;
	} else { /* if (memo_flags == 0x0000) { */
		char *tmp;
		pg_row = mdb_get_int32(mdb->pg_buf, start+4);
#if MDB_DEBUG
		printf("Reading LVAL page %06x\n", pg_row >> 8);
#endif
		tmp = (char *) g_malloc(MDB_BIND_SIZE);
		tmp[0] = '\0';
		do {
			if (mdb_find_pg_row(mdb,pg_row,&buf,&row_start,&len)) {
				return "";
			}
#if MDB_DEBUG
			printf("row num %d start %d len %d\n",
				pg_row & 0xff, row_start, len);
#endif
			strncat(tmp, buf + row_start + 4, 
				strlen(tmp) + len - 4 > MDB_BIND_SIZE ?
				MDB_BIND_SIZE - strlen(tmp) : len - 4);

			/* find next lval page */
			pg_row = mdb_get_int32(mdb->pg_buf, row_start);
		} while ((pg_row >> 8));
		mdb_unicode2ascii(mdb, tmp, strlen(tmp), text, MDB_BIND_SIZE);
		g_free(tmp);
		return text;
/*
	} else {
		fprintf(stderr,"Unhandled memo field flags = %04x\n", memo_flags);
		return "";
*/
	}
}
static char *
mdb_num_to_string(MdbHandle *mdb, int start, int datatype, int prec, int scale)
{
	char *text;
	gint32 l;

	memcpy(&l, mdb->pg_buf+start+13, 4);

	text = (char *) g_malloc(prec+2);
	sprintf(text, "%0*" G_GINT32_FORMAT, prec, GINT32_FROM_LE(l));
	if (scale) {
		memmove(text+prec-scale, text+prec-scale+1, scale+1);
		text[prec-scale] = '.';
	}
	return text;
}

static int trim_trailing_zeros(char * buff, int n)
{
	char * p = buff + n - 1;

	while (p >= buff && *p == '0')
		*p-- = '\0';

	if (*p == '.')
		*p = '\0';

	return 0;
}
				
char *mdb_col_to_string(MdbHandle *mdb, unsigned char *buf, int start, int datatype, int size)
{
	/* FIX ME -- not thread safe */
	static char text[MDB_BIND_SIZE];
	int n;
	float tf;
	double td;

	struct tm t;
	long int day, time;
	int yr, q;
	int *cal;
	int noleap_cal[] = {0,31,59,90,120,151,181,212,243,273,304,334,365};
	int leap_cal[]   = {0,31,60,91,121,152,182,213,244,274,305,335,366};

	switch (datatype) {
		case MDB_BOOL:
			/* shouldn't happen.  bools are handled specially
			** by mdb_xfer_bound_bool() */
		break;
		case MDB_BYTE:
			sprintf(text,"%d",mdb_get_byte(buf, start));
			return text;
		break;
		case MDB_INT:
			sprintf(text,"%ld",(long)mdb_get_int16(buf, start));
			return text;
		break;
		case MDB_LONGINT:
			sprintf(text,"%ld",mdb_get_int32(buf, start));
			return text;
		break;
		case MDB_FLOAT:
			tf = mdb_get_single(mdb->pg_buf, start);
			n = sprintf(text,"%.*f",FLT_DIG - (int)ceil(log10(tf)), tf);
			trim_trailing_zeros(text, n);
			return text;
		break;
		case MDB_DOUBLE:
			td = mdb_get_double(mdb->pg_buf, start);
			n = sprintf(text,"%.*f",DBL_DIG - (int)ceil(log10(td)), td);
			trim_trailing_zeros(text, n);
			return text;
		break;
		case MDB_TEXT:
			if (size<0) {
				return "";
			}
			mdb_unicode2ascii(mdb, mdb->pg_buf + start, size, text, MDB_BIND_SIZE);
			return text;
		break;
		case MDB_SDATETIME:
			/* Date/Time is stored as a double, where the whole
			   part is the days from 12/30/1899 and the fractional
			   part is the fractional part of one day. */
			td = mdb_get_double(mdb->pg_buf, start);

			day = (long int)(td);
			time = (long int)(fabs(td - day) * 86400.0 + 0.5);
			t.tm_hour = time / 3600;
			t.tm_min = (time / 60) % 60;
			t.tm_sec = time % 60; 
			t.tm_year = 1 - 1900;

			day += 693593; /* Days from 1/1/1 to 12/31/1899 */
			t.tm_wday = (day+1) % 7;

			q = day / 146097;  /* 146097 days in 400 years */
			t.tm_year += 400 * q;
			day -= q * 146097;

			q = day / 36524;  /* 36524 days in 100 years */
			if (q > 3) q = 3;
			t.tm_year += 100 * q;
			day -= q * 36524;

			q = day / 1461;  /* 1461 days in 4 years */
			t.tm_year += 4 * q;
			day -= q * 1461;

			q = day / 365;  /* 365 days in 1 year */
			if (q > 3) q = 3;
			t.tm_year += q;
			day -= q * 365;

			yr = t.tm_year + 1900;
			cal = ((yr)%4==0 && ((yr)%100!=0 || (yr)%400==0)) ?
				leap_cal : noleap_cal;
			for (t.tm_mon=0; t.tm_mon<12; t.tm_mon++) {
				if (day < cal[t.tm_mon+1]) break;
			}
			t.tm_mday = day - cal[t.tm_mon] + 1;
			t.tm_yday = day;
			t.tm_isdst = -1;

			strftime(text, MDB_BIND_SIZE, date_fmt, &t);
			return text;

		break;
		case MDB_MEMO:
			return mdb_memo_to_string(mdb, start, size);
		break;
		case MDB_MONEY:
			mdb_money_to_string(mdb, start, text);
			return text;
		case MDB_NUMERIC:
		break;
		default:
			return "";
		break;
	}
	return NULL;
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
		case MDB_SDATETIME:
			return 20;
		break;
		case MDB_MEMO:
			return 255; 
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
		case MDB_SDATETIME:
			return 4;
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
