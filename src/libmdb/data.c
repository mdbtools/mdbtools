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


void mdb_data_dump(MdbTableDef *table)
{
MdbColumn col;
MdbHandle *mdb = table->entry->mdb;
int i, pg_num;
int rows;
int row_start, row_end;

	for (pg_num=1;pg_num<=table->num_pgs;pg_num++) {
		mdb_read_pg(mdb,table->first_data_pg + pg_num);
		rows = mdb_get_int16(mdb,8);
		fprintf(stdout,"Rows on page %d: %d\n", pg_num, rows);
		row_end=2047;
		for (i=0;i<rows;i++) {
			row_start = mdb_get_int16(mdb,10+i*2);
			fprintf(stdout,"Row %d bytes %d to %d\n", i, row_start, row_end);
			
			fprintf(stdout,"#cols: %-3d #varcols %-3d\n", mdb->pg_buf[row_start], mdb->pg_buf[row_end-1]);
			row_end = row_start-1;
		}
	}
}
