/* MDB Tools - A library for reading MS Access database files
 * Copyright (C) 2004 Brian Bruns
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

#ifdef DMALLOC
#include "dmalloc.h"
#endif

/*
 * Temp table routines.  These are currently used to generate mock results for
 * commands like "list tables" and "describe table"
 */

void
mdb_fill_temp_col(MdbColumn *tcol, char *col_name, int col_size, int col_type, int is_fixed)
{
	memset(tcol,0,sizeof(MdbColumn));
	strcpy(tcol->name, col_name);
	tcol->col_type = col_type;
	if ((col_type == MDB_TEXT) || (col_type == MDB_MEMO)) {
		tcol->col_size = col_size;
	} else {
		tcol->col_size = mdb_col_fixed_size(tcol);
	}
	tcol->is_fixed = is_fixed;
}
void
mdb_fill_temp_field(MdbField *field, void *value, int siz, int is_fixed, int is_null, int start, int colnum)
{
   	field->value = value;
   	field->siz = siz;
   	field->is_fixed = is_fixed;
   	field->is_null = is_null;
   	field->start = start;
   	field->colnum = colnum;
}
MdbTableDef *
mdb_create_temp_table(MdbHandle *mdb, char *name)
{
	MdbCatalogEntry *entry;
	MdbTableDef *table;

	/* dummy up a catalog entry */
	entry = (MdbCatalogEntry *) g_malloc0(sizeof(MdbCatalogEntry));
	entry->mdb = mdb;
	entry->object_type = MDB_TABLE;
	entry->table_pg = 0;
	strcpy(entry->object_name, name);

	table = mdb_alloc_tabledef(entry);
	table->columns = g_ptr_array_new();
	table->is_temp_table = 1;
	table->temp_table_pages = g_ptr_array_new();

	return table;
}
void
mdb_temp_table_add_col(MdbTableDef *table, MdbColumn *col)
{
	col->col_num = table->num_cols;
	if (!col->is_fixed)
		col->var_col_num = table->num_var_cols++;
	g_ptr_array_add(table->columns, g_memdup(col, sizeof(MdbColumn)));
	table->num_cols++;
}
/*
 * Should be called after setting up all temp table columns
 */
void mdb_temp_columns_end(MdbTableDef *table)
{
	MdbColumn *col;
	unsigned int i;
	unsigned int start = 0;

	for (i=0; i<table->num_cols; i++) {
		col = g_ptr_array_index(table->columns, i);
		if (col->is_fixed) {
			col->fixed_offset = start;
			start += col->col_size;
		}
	}
}
