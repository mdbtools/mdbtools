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

#define MDB_DEBUG_OLE 1

char *mdb_money_to_string(MdbHandle *mdb, int start, char *s);
static int _mdb_attempt_bind(MdbHandle *mdb, 
	MdbColumn *col, unsigned char isnull, int offset, int len);
char *mdb_num_to_string(MdbHandle *mdb, int start, int datatype, int prec, int scale);


void mdb_bind_column(MdbTableDef *table, int col_num, void *bind_ptr)
{
MdbColumn *col;

	/* 
	** the column arrary is 0 based, so decrement to get 1 based parameter 
	*/
	col=g_ptr_array_index(table->columns, col_num - 1);
	col->bind_ptr = bind_ptr;
}
void mdb_bind_len(MdbTableDef *table, int col_num, int *len_ptr)
{
MdbColumn *col;

	col=g_ptr_array_index(table->columns, col_num - 1);
	col->len_ptr = len_ptr;
}
int 
mdb_find_end_of_row(MdbHandle *mdb, int row)
{
	MdbFormatConstants *fmt = mdb->fmt;
	int row_end;

	/* Search the previous "row start" values for the first non-deleted one.
	* If we don't find one, then the end of the page is the correct value.
	*/
#if 1
	if (row==0) {
		row_end = fmt->pg_size - 1;
	} else {
		row_end = (mdb_get_int16(mdb, ((fmt->row_count_offset + 2) + (row - 1) * 2)) & 0x0FFF) - 1;
	}
	return row_end;
#else
        for (i = row - 1; i >= 0; i--) {
                row_start = mdb_get_int16(mdb, ((fmt->row_count_offset + 2) + i * 2));
                if (!(row_start & 0x8000)) {
                        break;
                }
        }
        row_start &= 0x0FFF;

        if (i == -1) {
                row_end = fmt->pg_size - 1;
        } else {
                row_end = row_start - 1;
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
static int mdb_xfer_bound_bool(MdbHandle *mdb, MdbColumn *col, int value)
{
	col->cur_value_len = value;
	if (col->bind_ptr) {
		strcpy(col->bind_ptr,  value ? "0" : "1");
	}

	return 0;
}
static int mdb_xfer_bound_ole(MdbHandle *mdb, int start, MdbColumn *col, int len)
{
int ret;
	if (len) {
		col->cur_value_start = start;
		col->cur_value_len = len;
	} else {
		col->cur_value_start = 0;
		col->cur_value_len = 0;
	}
	if (col->bind_ptr || col->len_ptr) {
		ret = mdb_copy_ole(mdb, col->bind_ptr, start, len);
	}
	if (col->len_ptr) {
		*col->len_ptr = ret;
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
		if (len) {
//fprintf(stdout,"len %d size %d\n",len, col->col_size);
			if (col->col_type == MDB_NUMERIC) {
				strcpy(col->bind_ptr, 
				mdb_num_to_string(mdb, start, col->col_type, col->col_prec, col->col_scale));
			} else {
				strcpy(col->bind_ptr, 
				mdb_col_to_string(mdb, start, col->col_type, len));
			}
		} else {
			strcpy(col->bind_ptr,  "");
		}
		ret = strlen(col->bind_ptr);
		if (col->len_ptr) {
			*col->len_ptr = ret;
		}
		return ret;
	}
	return 0;
}
int mdb_read_row(MdbTableDef *table, int row)
{
MdbHandle *mdb = table->entry->mdb;
MdbFormatConstants *fmt = mdb->fmt;
MdbColumn *col;
int i, j, rc;
int num_cols, var_cols, fixed_cols;
int row_start, row_end;
int fixed_cols_found, var_cols_found;
int col_start, len, next_col;
int num_of_jumps=0, jumps_used=0;
int eod; /* end of data */
int delflag, lookupflag;
int bitmask_sz;
int col_ptr, deleted_columns=0;
unsigned char null_mask[33]; /* 256 columns max / 8 bits per byte */
unsigned char isnull;

	row_start = mdb_get_int16(mdb, (fmt->row_count_offset + 2) + (row*2)); 
	row_end = mdb_find_end_of_row(mdb, row);

	delflag = lookupflag = 0;
	if (row_start & 0x8000) lookupflag++;
	if (row_start & 0x4000) delflag++;
	row_start &= 0x0FFF; /* remove flags */
#if MDB_DEBUG
	fprintf(stdout,"Row %d bytes %d to %d %s %s\n", 
		row, row_start, row_end,
		lookupflag ? "[lookup]" : "",
		delflag ? "[delflag]" : "");
#endif	
	//if (!table->noskip_del && (delflag || lookupflag)) {
	if (!table->noskip_del && delflag) {
		row_end = row_start-1;
		return 0;
	}

#if MDB_DEBUG
	buffer_dump(mdb->pg_buf, row_start, row_end);
#endif

	/* find out all the important stuff about the row */
	if (IS_JET4(mdb)) {
		num_cols = mdb_get_int16(mdb, row_start);
	} else {
		num_cols = mdb->pg_buf[row_start];
	}
	var_cols = 0; /* mdb->pg_buf[row_end-1]; */
	fixed_cols = 0; /* num_cols - var_cols; */
	for (j = 0; j < table->num_cols; j++) {
		col = g_ptr_array_index (table->columns, j);
		if (mdb_is_fixed_col(col)) 
			fixed_cols++;
		else
			var_cols++;
	}
	bitmask_sz = (num_cols - 1) / 8 + 1;
	if (IS_JET4(mdb)) {
		eod = mdb_get_int16(mdb, row_end - 3 - var_cols*2 - bitmask_sz);
	} else {
		eod = mdb->pg_buf[row_end-1-var_cols-bitmask_sz];
	}
	for (i=0;i<bitmask_sz;i++) {
		null_mask[i]=mdb->pg_buf[row_end - bitmask_sz + i + 1];
	}

#if MDB_DEBUG
	fprintf(stdout,"#cols: %-3d #varcols %-3d EOD %-3d\n", 
		num_cols, var_cols, eod);
#endif

	if (IS_JET4(mdb)) {
		col_start = 2;
	} else {
		/* data starts at 1 */
		col_start = 1;
	}
	fixed_cols_found = 0;
	var_cols_found = 0;

	/* fixed columns */
	for (j=0;j<table->num_cols;j++) {
		col = g_ptr_array_index(table->columns,j);
		if (mdb_is_fixed_col(col) &&
		    ++fixed_cols_found <= fixed_cols) {
/* 
			if (!strcmp(col->name, "Type")) {
				printf("column Type, col_start %d row_start %d data %d %d\n",col_start, row_start, mdb->pg_buf[row_start + col_start], mdb->pg_buf[row_start + col_start + 1]); 
			}
*/
			isnull = mdb_is_null(null_mask, j+1); 
			rc = _mdb_attempt_bind(mdb, col, isnull,
				row_start + col_start, col->col_size);
			if (!rc) return 0;
			if (col->col_type != MDB_BOOL) 
				col_start += col->col_size;
		}
	}

	/* if fixed columns add up to more than 256, we need a jump */
       if (col_start >= 256) {
               num_of_jumps++;
               jumps_used++;
               row_start = row_start + col_start - (col_start % 256);
       }

       col_start = row_start;
       /*  */
       while (col_start+256 < row_end-bitmask_sz-1-var_cols-num_of_jumps){
               col_start += 256;
               num_of_jumps++;
       }
	if (IS_JET4(mdb)) {
		col_ptr = row_end - 2 - bitmask_sz - 1;
		eod = mdb_get_int16(mdb, col_ptr - var_cols*2);
		col_start = mdb_get_int16(mdb, col_ptr);
	} else {
		col_ptr = row_end - bitmask_sz - num_of_jumps - 1;
		if (mdb->pg_buf[col_ptr]==0xFF) {
			col_ptr--;
			deleted_columns++;
		}
		eod = mdb->pg_buf[col_ptr - var_cols];
		col_start = mdb->pg_buf[col_ptr];
	}

#if MDB_DEBUG
	fprintf(stdout,"col_start %d num_of_jumps %d\n", 
		col_start, num_of_jumps);
#endif

	/* variable columns */
	for (j=0;j<table->num_cols;j++) {
		col = g_ptr_array_index(table->columns,j);
		if (!mdb_is_fixed_col(col) &&
		    ++var_cols_found <= var_cols) {
			/* col_start = mdb->pg_buf[row_end-bitmask_sz-var_cols_found]; */
			/* more code goes here but the diff is mangled */
			
/*
			if (var_cols_found == mdb->pg_buf[row_end-bitmask_sz-jumps_used-1] &&
				jumps_used < num_of_jumps) {
				row_start += 256;
				col_start -= 256;
				jumps_used++;
			}
*/


		if (var_cols_found==var_cols)  {
				len=eod - col_start;
		} else  {
			if (IS_JET4(mdb)) {
				//next_col = mdb_get_int16(mdb, row_end - bitmask_sz - var_cols_found * 2 - 2 - 1) ;
				next_col = mdb->pg_buf[row_end - bitmask_sz - var_cols_found * 2 - 2] * 256 + 
				mdb->pg_buf[row_end - bitmask_sz - var_cols_found * 2 - 2 - 1] ;
				len = next_col - col_start;
				/* len=mdb->pg_buf[row_end - bitmask_sz - var_cols_found * 2
					- 2 - 1] - col_start;  */
				//len=mdb->pg_buf[row_end - bitmask_sz - var_cols_found * 2
					//- 2 - 1 - num_of_jumps * 2] - col_start; 
/*
				fprintf(stdout, "found %d fix %d new pos %d new start %d old start %d\n", 
					var_cols_found,
					row_end - bitmask_sz - var_cols_found * 2
               - 2 - 1,
					row_end - bitmask_sz - var_cols_found * 2
               - 2 - 1 - num_of_jumps * 2,
					mdb->pg_buf[row_end - bitmask_sz - var_cols_found * 2
               - 2 - 1], col_start);
*/
			} else {
				len=mdb->pg_buf[col_ptr - var_cols_found ] - col_start;
			}
			if (len<0)
				len+=256;
		}

			isnull = mdb_is_null(null_mask, j+1); 
#if MDB_DEBUG
			printf("binding len %d isnull %d col_start %d row_start %d row_end %d bitmask %d var_cols_found %d buf %d\n", len, isnull,col_start,row_start,row_end, bitmask_sz, var_cols_found, mdb->pg_buf[row_end - bitmask_sz - var_cols_found * 2 - 1 - num_of_jumps ]);
#endif
			rc = _mdb_attempt_bind(mdb, col, isnull,
				row_start + col_start, len);
			if (!rc) return 0;
			col_start += len;
		}
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
	} else if (col->col_type == MDB_OLE) {
		mdb_xfer_bound_ole(mdb, offset, col, len);
	} else if (isnull) {
		mdb_xfer_bound_data(mdb, 0, col, 0);
	} else {
		if (!mdb_test_sargs(mdb, col, offset, len)) {
			return 0;
		}
		mdb_xfer_bound_data(mdb, offset, col, len);
	}
	return 1;
}
int 
mdb_read_next_dpg_by_map0(MdbTableDef *table)
{
MdbCatalogEntry *entry = table->entry;
MdbHandle *mdb = entry->mdb;
int pgnum, i, bitn;

	pgnum = _mdb_get_int32(table->usage_map,1);
	/* the first 5 bytes of the usage map mean something */
	for (i=5;i<table->map_sz;i++) {
		for (bitn=0;bitn<8;bitn++) {
			if (table->usage_map[i] & 1 << bitn && pgnum > table->cur_phys_pg) {
				table->cur_phys_pg = pgnum;
				if (!mdb_read_pg(mdb, pgnum)) {
					return 0;
				} else {
					return pgnum;
				}
			}
			pgnum++;
		}
	}
	/* didn't find anything */
	return 0;
}
int 
mdb_read_next_dpg_by_map1(MdbTableDef *table)
{
MdbCatalogEntry *entry = table->entry;
MdbHandle *mdb = entry->mdb;
guint32 pgnum, i, j, bitn, map_pg;
unsigned char map_byte;

	pgnum = 0;
	//printf("map size %ld\n", table->map_sz);
	for (i=1;i<table->map_sz-1;i+=4) {
		map_pg = _mdb_get_int32(table->usage_map, i);
		//printf("loop %d pg %ld %02x%02x%02x%02x\n",i, map_pg,table->usage_map[i],table->usage_map[i+1],table->usage_map[i+2],table->usage_map[i+3]);

		if (!map_pg) continue;

		if(mdb_read_alt_pg(mdb, map_pg) != mdb->fmt->pg_size) {
			fprintf(stderr, "Oops! didn't get a full page at %d\n", map_pg);
			exit(1);
		} 
		//printf("reading page %ld\n",map_pg);
		for (j=4;j<mdb->fmt->pg_size;j++) {
			for (bitn=0;bitn<8;bitn++) {
				if (mdb->alt_pg_buf[j] & 1 << bitn && pgnum > table->cur_phys_pg) {
					table->cur_phys_pg = pgnum;
					if (!mdb_read_pg(mdb, pgnum)) {
						return 0;
					} else {
						//printf("page found at %04x %d\n",pgnum, pgnum);
						return pgnum;
					}
				}
				pgnum++;
			}
		}
	}
	/* didn't find anything */
	//printf("returning 0\n");
	return 0;
}
int 
mdb_read_next_dpg(MdbTableDef *table)
{
MdbCatalogEntry *entry = table->entry;
MdbHandle *mdb = entry->mdb;
int map_type;

#ifndef SLOW_READ
	map_type = table->usage_map[0];
	if (map_type==0) {
		return mdb_read_next_dpg_by_map0(table);
	} else if (map_type==1) {
		return mdb_read_next_dpg_by_map1(table);
	} else {
		fprintf(stderr,"Warning: unrecognized usage map type: %d, defaulting to brute force read\n",table->usage_map[0]);
	}
#endif 
	/* can't do a fast read, go back to the old way */
	do {
		if (!mdb_read_pg(mdb, table->cur_phys_pg++))
			return 0;
	} while (mdb->pg_buf[0]!=0x01 || mdb_get_int32(mdb, 4)!=entry->table_pg);
	/* fprintf(stderr,"returning new page %ld\n", table->cur_phys_pg);  */
	return table->cur_phys_pg;
}
int mdb_rewind_table(MdbTableDef *table)
{
	table->cur_pg_num=0;
	table->cur_phys_pg=0;
	table->cur_row=0;

	return 0;
}
int mdb_fetch_row(MdbTableDef *table)
{
MdbHandle *mdb = table->entry->mdb;
MdbFormatConstants *fmt = mdb->fmt;
int rows;
int rc;

	if (table->num_rows==0)
		return 0;

	/* initialize */
	if (!table->cur_pg_num) {
		table->cur_pg_num=1;
		table->cur_row=0;
		mdb_read_next_dpg(table);
	}

	do { 
		rows = mdb_get_int16(mdb,fmt->row_count_offset);

		/* if at end of page, find a new page */
		if (table->cur_row >= rows) {
			table->cur_row=0;

			if (!mdb_read_next_dpg(table)) {
				return 0;
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
MdbHandle *mdb = table->entry->mdb;
int i, j, pg_num;
int rows;
char *bound_values[MDB_MAX_COLS]; 

	for (i=0;i<table->num_cols;i++) {
		bound_values[i] = (char *) malloc(256);
		mdb_bind_column(table, i+1, bound_values[i]);
	}
	mdb_rewind_table(table);
	while (mdb_fetch_row(table)) {
		for (j=0;j<table->num_cols;j++) {
			fprintf(stdout, "column %d is %s\n", j+1, bound_values[j]);
		}
	}
	for (i=0;i<table->num_cols;i++) {
		free(bound_values[i]);
	}
}

int mdb_is_fixed_col(MdbColumn *col)
{
	return col->is_fixed;
}
static char *mdb_data_to_hex(MdbHandle *mdb, char *text, int start, int size) 
{
int i;

	for (i=start; i<start+size; i++) {
		sprintf(&text[(i-start)*2],"%02x", mdb->pg_buf[i]);
	}
	text[(i-start)*2]='\0';

	return text;
}
int mdb_copy_ole(MdbHandle *mdb, char *dest, int start, int size)
{
guint16 ole_len;
guint16 ole_flags;
guint16 row_start, row_stop;
guint8 ole_row;
guint32 lval_pg;
guint16 len, cur;

	if (size<MDB_MEMO_OVERHEAD) {
		return 0;
	} 

	ole_len = mdb_get_int16(mdb, start);
	ole_flags = mdb_get_int16(mdb, start+2);

	if (ole_flags == 0x8000) {
		len = size - MDB_MEMO_OVERHEAD;
		/* inline ole field */
		if (dest) memcpy(dest, &mdb->pg_buf[start + MDB_MEMO_OVERHEAD],
			size - MDB_MEMO_OVERHEAD);
		return len;
	} else if (ole_flags == 0x4000) {
		/* The 16 bit integer at offset 0 is the length of the memo field.
		* The 24 bit integer at offset 5 is the page it is stored on.
		*/
		ole_row = mdb->pg_buf[start+4];
		
		lval_pg = mdb_get_int24(mdb, start+5);
#if MDB_DEBUG_OLE
		printf("Reading LVAL page %06x\n", lval_pg);
#endif
		if(mdb_read_alt_pg(mdb, lval_pg) != mdb->fmt->pg_size) {
			/* Failed to read */
			return 0;
		}
		/* swap the alt and regular page buffers, so we can call get_int16 */
		mdb_swap_pgbuf(mdb);
		if (ole_row) {
			row_stop = mdb_get_int16(mdb, 10 + (ole_row - 1) * 2) & 0x0FFF;
		} else {
			row_stop = mdb->fmt->pg_size - 1;
		}
		row_start = mdb_get_int16(mdb, 10 + ole_row * 2);
#if MDB_DEBUG_OLE
			printf("row num %d row start %d row stop %d\n", ole_row, row_start, row_stop);
#endif
			len = row_stop - row_start;
			if (dest) memcpy(dest, &mdb->pg_buf[row_start], len);
			/* make sure to swap page back */
			mdb_swap_pgbuf(mdb);
			return len;
		} else if (ole_flags == 0x0000) {
			ole_row = mdb->pg_buf[start+4];
			lval_pg = mdb_get_int24(mdb, start+5);
#if MDB_DEBUG_OLE
			printf("Reading LVAL page %06x\n", lval_pg);
#endif
		/* swap the alt and regular page buffers, so we can call get_int16 */
		mdb_swap_pgbuf(mdb);
		cur=0;
		do {
			if(mdb_read_pg(mdb, lval_pg) != mdb->fmt->pg_size) {
				/* Failed to read */
				return 0;
			}
			if (ole_row) {
				row_stop = mdb_get_int16(mdb, 10 + (ole_row - 1) * 2) & 0x0FFF;
			} else {
				row_stop = mdb->fmt->pg_size - 1;
			}
			row_start = mdb_get_int16(mdb, 10 + ole_row * 2);
#if MDB_DEBUG_OLE
		printf("row num %d row start %d row stop %d\n", ole_row, row_start, row_stop);
#endif
			len = row_stop - row_start;
			if (dest) 
				memcpy(&dest[cur], &mdb->pg_buf[row_start+4], 
				len - 4);
			cur += len - 4;

			/* find next lval page */
			ole_row = mdb->pg_buf[row_start];
			lval_pg = mdb_get_int24(mdb, row_start+1);
		} while (lval_pg);
		/* make sure to swap page back */
		mdb_swap_pgbuf(mdb);
		return cur;
	} else {
		fprintf(stderr,"Unhandled ole field flags = %04x\n", ole_flags);
		return 0;
	}
}
static char *mdb_memo_to_string(MdbHandle *mdb, int start, int size)
{
MdbFormatConstants *fmt = mdb->fmt;
guint16 memo_len;
static char text[MDB_BIND_SIZE];
guint16 memo_flags;
guint16 row_start, row_stop;
guint8 memo_row;
guint32 lval_pg;
guint16 len;
int i;

	if (size<MDB_MEMO_OVERHEAD) {
		return "";
	} 

#if MDB_DEBUG
	buffer_dump(mdb->pg_buf, start, start + 12);
#endif

	memo_len = mdb_get_int16(mdb, start);
	memo_flags = mdb_get_int16(mdb, start+2);

	if (memo_flags & 0x8000) {
		/* inline memo field */
		strncpy(text, &mdb->pg_buf[start + MDB_MEMO_OVERHEAD],
			size - MDB_MEMO_OVERHEAD);
		text[size - MDB_MEMO_OVERHEAD]='\0';
		return text;
	} else if (memo_flags & 0x4000) {
		/* The 16 bit integer at offset 0 is the length of the memo field.
		* The 24 bit integer at offset 5 is the page it is stored on.
		*/
		memo_row = mdb->pg_buf[start+4];
		
		lval_pg = mdb_get_int24(mdb, start+5);
#if MDB_DEBUG
		printf("Reading LVAL page %06x\n", lval_pg);
#endif
		if(mdb_read_alt_pg(mdb, lval_pg) != fmt->pg_size) {
			/* Failed to read */
			return "";
		}
		/* swap the alt and regular page buffers, so we can call get_int16 */
		mdb_swap_pgbuf(mdb);
		if (memo_row) {
			row_stop = mdb_get_int16(mdb, fmt->row_count_offset + 2 + (memo_row - 1) * 2) & 0x0FFF;
		} else {
			row_stop = fmt->pg_size - 1;
		}
		row_start = mdb_get_int16(mdb, fmt->row_count_offset + 2 + memo_row * 2);
#if MDB_DEBUG
		printf("row num %d row start %d row stop %d\n", memo_row, row_start, row_stop);
		buffer_dump(mdb->pg_buf,row_start, row_start + len);
#endif
		len = row_stop - row_start;
		if (IS_JET3(mdb)) {
			strncpy(text, &mdb->pg_buf[row_start], len);
			text[len]='\0';
		} else {
			if (mdb->pg_buf[row_start]==0xff && 
			   mdb->pg_buf[row_start+1]==0xfe) {
				strncpy(text, &mdb->pg_buf[row_start+2], len-2);
				text[len-2]='\0';
			} else {
				/* convert unicode to ascii, rather sloppily */
				for (i=0;i<len;i+=2)
					text[i/2] = mdb->pg_buf[row_start + i];
				text[len/2]='\0';
			}
		}
		/* make sure to swap page back */
		mdb_swap_pgbuf(mdb);
		return text;
	} else { /* if (memo_flags == 0x0000) { */
		memo_row = mdb->pg_buf[start+4];
		lval_pg = mdb_get_int24(mdb, start+5);
#if MDB_DEBUG
		printf("Reading LVAL page %06x\n", lval_pg);
#endif
		/* swap the alt and regular page buffers, so we can call get_int16 */
		mdb_swap_pgbuf(mdb);
		text[0]='\0';
		do {
			if(mdb_read_pg(mdb, lval_pg) != fmt->pg_size) {
				/* Failed to read */
				return "";
			}
			if (memo_row) {
				row_stop = mdb_get_int16(mdb, 10 + (memo_row - 1) * 2) & 0x0FFF;
			} else {
				row_stop = fmt->pg_size - 1;
			}
			row_start = mdb_get_int16(mdb, 10 + memo_row * 2);
#if MDB_DEBUG
		printf("row num %d row start %d row stop %d\n", memo_row, row_start, row_stop);
#endif
			len = row_stop - row_start;
			strncat(text, &mdb->pg_buf[row_start+4], 
				strlen(text) + len - 4 > MDB_BIND_SIZE ?
				MDB_BIND_SIZE - strlen(text) :
				len - 4);

			/* find next lval page */
			memo_row = mdb->pg_buf[row_start];
			lval_pg = mdb_get_int24(mdb, row_start+1);
		} while (lval_pg);
		/* make sure to swap page back */
		mdb_swap_pgbuf(mdb);
		return text;
/*
	} else {
		fprintf(stderr,"Unhandled memo field flags = %04x\n", memo_flags);
		return "";
*/
	}
#if 0
			strncpy(text, &mdb->pg_buf[start + MDB_MEMO_OVERHEAD], 
				size - MDB_MEMO_OVERHEAD);
			text[size - MDB_MEMO_OVERHEAD]='\0';
			return text;
#endif
}
char *mdb_num_to_string(MdbHandle *mdb, int start, int datatype, int prec, int scale)
{
/* FIX ME -- not thread safe */
static char text[MDB_BIND_SIZE];
char tmpbuf[MDB_BIND_SIZE];
char mask[20];
gint32 l, whole, fraction;

	l = mdb->pg_buf[start+16] * 256 * 256 * 256 +
		mdb->pg_buf[start+15] * 256 * 256 +
		mdb->pg_buf[start+14] * 256 +
		mdb->pg_buf[start+13];

	sprintf(mask,"%%0%ldld",prec);
	sprintf(tmpbuf,mask,l);
	//strcpy(text, tmpbuf);
	//return text;
	if (!scale) {
		strcpy(text,tmpbuf);
	} else {
		memset(text,0,sizeof(text));
		strncpy(text,tmpbuf,prec-scale);
		strcat(text,".");
		strcat(text,&tmpbuf[strlen(tmpbuf)-scale]);
	}
/*
		for (i=0;i<size;i++) {
				fprintf(stdout, "%c %02x ", isprint(mdb->pg_buf[start+i]) ? mdb->pg_buf[start+i] : '.', mdb->pg_buf[start+i]);
			} 
			fprintf(stdout, "\n");
*/
		return text;
}
char *mdb_col_to_string(MdbHandle *mdb, int start, int datatype, int size)
{
/* FIX ME -- not thread safe */
static char text[MDB_BIND_SIZE];
char tmpbuf[10];
time_t t;
int i,j;

	switch (datatype) {
		case MDB_BOOL:
			/* shouldn't happen.  bools are handled specially
			** by mdb_xfer_bound_bool() */
		break;
		case MDB_BYTE:
			sprintf(text,"%d",mdb_get_byte(mdb, start));
			return text;
		break;
		case MDB_INT:
			sprintf(text,"%ld",(long)mdb_get_int16(mdb, start));
			return text;
		break;
		case MDB_LONGINT:
			sprintf(text,"%ld",mdb_get_int32(mdb, start));
			return text;
		break;
		case MDB_FLOAT:
			sprintf(text,"%f",mdb_get_single(mdb, start));
			return text;
		break;
		case MDB_DOUBLE:
			sprintf(text,"%f",mdb_get_double(mdb, start));
			return text;
		break;
		case MDB_TEXT:
			if (size<0) {
				return "";
			}
			if (IS_JET4(mdb)) {
/*
				for (i=0;i<size;i++) {
					fprintf(stdout, "%c %02x ", mdb->pg_buf[start+i], mdb->pg_buf[start+i]);
				} 
				fprintf(stdout, "\n");
*/
				if (mdb->pg_buf[start]==0xff && 
				   mdb->pg_buf[start+1]==0xfe) {
					strncpy(text, &mdb->pg_buf[start+2], size-2);
					text[size-2]='\0';
				} else {
					/* convert unicode to ascii, rather sloppily */
					for (i=0;i<size;i+=2)
						text[i/2] = mdb->pg_buf[start + i];
					text[size/2]='\0';
				}
			} else {
				strncpy(text, &mdb->pg_buf[start], size);
				text[size]='\0';
			}
			return text;
		break;
		case MDB_SDATETIME:
			t = (long int)((mdb_get_double(mdb, start) - 25569.0) * 86400.0);
			strftime(text, MDB_BIND_SIZE, "%x %X",
				(struct tm*)gmtime(&t));
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
			return 3;
		break;
		case MDB_INT:
			return 5;
		break;
		case MDB_LONGINT:
			return 7;
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
			return 12;
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
