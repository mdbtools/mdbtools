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


char *mdb_get_coltype_string(int col_type)
{

/* 
** need to do this is a way that will allow multiple outputs for different
** backend servers...these are MS SQL/Sybase specific 
*/
static char *type_name[] = {"Unknown 0x00",
                        "Unknown 0x01",
                        "Unknown 0x02",
                        "smallint",
                        "int",
                        "Unknown 0x05",
                        "Unknown 0x06",
                        "Unknown 0x07",
                        "smalldatetime",
                        "Unknown 0x09",
                        "varchar",
                        "Unknown 0x0b"
                        "hyperlink -- fixme"
                };

        if (col_type > 11) {
                return NULL;
        } else {
                return type_name[col_type];
        }
}

unsigned char mdb_col_needs_size(int col_type)
{
	if (col_type == MDB_TEXT) {
		return TRUE;
	} else {
		return FALSE;
	}
}

/*
**   
*/

void mdb_table_dump(MdbCatalogEntry *entry)
{
int num_cols, num_rows, data_pgs, first_dpg;
int len, i;
int cur_col, cur_name;
int col_type, col_size;
int col_start, name_start;
char name[MDB_MAX_OBJ_NAME+1];
int name_sz;
MdbHandle *mdb = entry->mdb;

	mdb_read_pg(mdb, entry->table_pg);
	len = mdb_get_int16(mdb,8);
	num_rows = mdb_get_int32(mdb,12);
	num_cols = mdb_get_int16(mdb,25);
	data_pgs = mdb_get_int32(mdb,27);
	first_dpg = mdb_get_int16(mdb,36);
	fprintf(stdout,"number of datarows  = %d\n",num_rows);
	fprintf(stdout,"number of columns   = %d\n",num_cols);
	fprintf(stdout,"number of datapages = %d\n",data_pgs);
	fprintf(stdout,"first data page     = %d\n",first_dpg);

	col_start = 43 + (data_pgs * 8);
	name_start = col_start + (num_cols * 18);

	cur_col = col_start;
	cur_name = name_start;

	for (i=0;i<num_cols;i++) {
		col_type = mdb->pg_buf[cur_col];
		col_size = mdb_get_int16(mdb,cur_col+16);
		
		/* get the name */
		name_sz = mdb->pg_buf[cur_name];
		memcpy(name,&mdb->pg_buf[cur_name+1],name_sz);
		name[name_sz]='\0';
		fprintf(stdout,"column %2d %s\n",i,name);
		fprintf(stdout,"column type %s\n",mdb_get_coltype_string(col_type));
		fprintf(stdout,"column size %d\n",col_size);

		cur_col += 18;
		cur_name += name_sz + 1;
	}
}

