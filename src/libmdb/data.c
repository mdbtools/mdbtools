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

char *mdb_col_to_string(const char *buf, int datatype, int size);

void mdb_data_dump(MdbTableDef *table)
{
MdbColumn col;
MdbHandle *mdb = table->entry->mdb;
int i, j, pg_num;
int rows, num_cols, var_cols, fixed_cols;
int row_start, row_end;
int fixed_cols_found, var_cols_found;
int col_start, len;
int eod; /* end of data */
int delflag, lookupflag;

	for (pg_num=1;pg_num<=table->num_pgs;pg_num++) {
		mdb_read_pg(mdb,table->first_data_pg + pg_num);
		rows = mdb_get_int16(mdb,8);
		fprintf(stdout,"Rows on page %d: %d\n", 
			pg_num + table->first_data_pg, 
			rows);
		row_end=2047;
		for (i=0;i<rows;i++) {
			row_start = mdb_get_int16(mdb,10+i*2); 
			delflag = lookupflag = 0;
			if (row_start & 0x8000) delflag++;
			if (row_start & 0x4000) lookupflag++;
			row_start &= 0x0FFF; /* remove flags */
			fprintf(stdout,"Pg %d Row %d bytes %d to %d %s %s\n", 
				pg_num, i, row_start, row_end,
				lookupflag ? "[lookup]" : "",
				delflag ? "[delflag]" : "");
			
			if (delflag || lookupflag) {
				row_end = row_start-1;
				continue;
			}
			buffer_dump(mdb->pg_buf, row_start, row_end);

			num_cols = mdb->pg_buf[row_start];
			var_cols = mdb->pg_buf[row_end-1];
			fixed_cols = num_cols - var_cols;
			eod = mdb->pg_buf[row_end-2-var_cols];

			fprintf(stdout,"#cols: %-3d #varcols %-3d EOD %-3d\n", num_cols, var_cols, eod);

			col_start = 1;
			fixed_cols_found = 0;
			var_cols_found = 0;
			for (j=0;j<table->num_cols;j++) {
				col = g_array_index(table->columns,MdbColumn,j);
				if (mdb_is_fixed_col(&col) &&
				    ++fixed_cols_found <= fixed_cols) {
					fprintf(stdout,"fixed col %s = %s\n",col.name,mdb_col_to_string(&mdb->pg_buf[row_start + col_start],col.col_type,0));
					col_start += col.col_size;
				}
			}
			for (j=0;j<table->num_cols;j++) {
				col = g_array_index(table->columns,MdbColumn,j);
				if (!mdb_is_fixed_col(&col) &&
				    ++var_cols_found <= var_cols) {
					col_start = mdb->pg_buf[row_end-1-var_cols_found];
					if (var_cols_found==var_cols) 
						len=eod - col_start;
					else 
						len=col_start - mdb->pg_buf[row_end-1-var_cols_found-1];
					fprintf(stdout,"coltype %d colstart %d len %d\n",col.col_type,col_start, len);
					fprintf(stdout,"var col %s = %s\n", col.name, mdb_col_to_string(&mdb->pg_buf[row_start + col_start],col.col_type,len));
					col_start += len;
				}
			}
			row_end = row_start-1;
		}
	}
}

int mdb_is_fixed_col(MdbColumn *col)
{
	/* FIX ME -- not complete */
	if (col->col_type==MDB_TEXT)
		return FALSE;
	else 
		return TRUE;
}
char *mdb_col_to_string(const char *buf, int datatype, int size)
{
/* FIX ME -- fix for big endian boxes */
static char text[256];

	switch (datatype) {
		case MDB_INT:
			sprintf(text,"%ld",*((gint16 *)buf));
			return "test";
		break;
		case MDB_LONGINT:
			sprintf(text,"%ld",*((gint32 *)buf));
			return text;
		break;
		case MDB_TEXT:
			strncpy(text, buf, size);
			text[size]='\0';
			return text;
		break;
	}
	return NULL;
}
