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
** These are the Access datatype names, each backend will have to provide
** its own mapping.
*/
static char *type_name[] = {"Unknown 0x00",
                        "Boolean",
                        "Byte",
                        "Integer",
                        "Long Integer",
                        "Currency",
                        "Single",
                        "Double",
                        "DateTime (Short)",
                        "Unknown 0x09",
                        "Text",
                        "OLE",
                        "Memo/Hyperlink",
                        "Unknown 0x0d",
                        "Unknown 0x0e",
			"Replication ID"
                };

        if (col_type > 0x0f) {
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

MdbTableDef *mdb_read_table(MdbCatalogEntry *entry)
{
MdbTableDef *table;
MdbHandle *mdb = entry->mdb;
int len, i;

	table = mdb_alloc_tabledef(entry);

	mdb_read_pg(mdb, entry->table_pg);
	len = mdb_get_int16(mdb,8);

	table->num_rows = mdb_get_int32(mdb,12);
	table->num_cols = mdb_get_int16(mdb,25);
	table->num_pgs = mdb_get_int32(mdb,27);
	table->first_data_pg = mdb_get_int16(mdb,36);

	return table;
}

GPtrArray *mdb_read_columns(MdbTableDef *table)
{
MdbHandle *mdb = table->entry->mdb;
MdbColumn col;
int len, i;
int cur_col, cur_name;
int col_type, col_size;
int col_start, name_start;
char name[MDB_MAX_OBJ_NAME+1];
int name_sz;
	
	table->columns = g_ptr_array_new();

	col_start = 43 + (table->num_pgs * 8);
	name_start = col_start + (table->num_cols * 18);

	cur_col = col_start;
	cur_name = name_start;

	for (i=0;i<table->num_cols;i++) {
		memset(&col,'\0', sizeof(MdbColumn));

		col.col_type = mdb->pg_buf[cur_col];
		col.col_size = mdb_get_int16(mdb,cur_col+16);
		/* get the name */
		name_sz = mdb->pg_buf[cur_name];
		memcpy(col.name,&mdb->pg_buf[cur_name+1],name_sz);
		col.name[name_sz]='\0';

		cur_col += 18;
		cur_name += name_sz + 1;
		mdb_append_column(table->columns, &col);
	}

	return table->columns;
}

void mdb_table_dump(MdbCatalogEntry *entry)
{
MdbTableDef *table;
MdbColumn *col;
MdbHandle *mdb = entry->mdb;
int i;

	table = mdb_read_table(entry);
	fprintf(stdout,"number of datarows  = %d\n",table->num_rows);
	fprintf(stdout,"number of columns   = %d\n",table->num_cols);
	fprintf(stdout,"number of datapages = %d\n",table->num_pgs);
	fprintf(stdout,"first data page     = %d\n",table->first_data_pg);

	mdb_read_columns(table);
	for (i=0;i<table->num_cols;i++) {
		col = g_ptr_array_index(table->columns,i);
	
		fprintf(stdout,"column %d Name: %-20s Type: %s(%d)\n",
			i, col->name,
			mdb_get_coltype_string(col->col_type),
			col->col_size);
	}
}
