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

MdbTableDef *
mdb_create_temp_table(MdbHandle *mdb, char *name)
{
	MdbCatalogEntry entry;
	MdbTableDef *table;

	/* dummy up a catalog entry */
	memset(&entry, 0, sizeof(MdbCatalogEntry));
	entry.mdb = mdb;
	entry.object_type = MDB_TABLE;
	entry.table_pg = 0;
	strcpy(entry.object_name, name);

	table = mdb_alloc_tabledef(g_memdup(&entry, sizeof(MdbCatalogEntry)));
	table->columns = g_ptr_array_new();

	return table;
}
void
mdb_temp_table_add_col(MdbTableDef *table, MdbColumn *col)
{
	col->col_num = table->num_cols;
	mdb_append_column(table->columns, col);
	table->num_cols++;
}
