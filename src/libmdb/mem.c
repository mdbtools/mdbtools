/* MDB Tools - A library for reading MS Access database files
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
#include <locale.h>

#ifdef DMALLOC
#include "dmalloc.h"
#endif

/**
 * mdb_init:
 *
 * Initializes the LibMDB library.  This function should be called exactly once
 * by calling program and prior to any other function.
 *
 **/
void mdb_init()
{
	mdb_init_backends();
}

/**
 * mdb_exit:
 *
 * Cleans up the LibMDB library.  This function should be called exactly once
 * by the calling program prior to exiting (or prior to final use of LibMDB 
 * functions).
 *
 **/
void mdb_exit()
{
	mdb_remove_backends();
}

/* private function */
MdbStatistics *mdb_alloc_stats(MdbHandle *mdb)
{
	mdb->stats = g_malloc0(sizeof(MdbStatistics));
	return mdb->stats;
/* private function */
}
/* private function */
void 
mdb_free_stats(MdbHandle *mdb)
{
	if (!mdb->stats) return;
	g_free(mdb->stats);
	mdb->stats = NULL;
}

void mdb_alloc_catalog(MdbHandle *mdb)
{
	mdb->catalog = g_ptr_array_new();
}
void mdb_free_catalog(MdbHandle *mdb)
{
	unsigned int i;

	if (!mdb->catalog) return;
	for (i=0; i<mdb->catalog->len; i++)
		g_free (g_ptr_array_index(mdb->catalog, i));
	g_ptr_array_free(mdb->catalog, TRUE);
	mdb->catalog = NULL;
}

MdbTableDef *mdb_alloc_tabledef(MdbCatalogEntry *entry)
{
	MdbTableDef *table;

	table = (MdbTableDef *) g_malloc0(sizeof(MdbTableDef));
	table->entry=entry;
	strcpy(table->name, entry->object_name);

	return table;	
}
void 
mdb_free_tabledef(MdbTableDef *table)
{
	if (!table) return;
	g_free(table->usage_map);
	g_free(table->free_usage_map);
	g_free(table);
}

void
mdb_append_column(GPtrArray *columns, MdbColumn *in_col)
{
	MdbColumn *col;

 	col = g_memdup(in_col,sizeof(MdbColumn));
	g_ptr_array_add(columns, col);
}
void
mdb_free_columns(GPtrArray *columns)
{
	unsigned int i;

	if (!columns) return;
	for (i=0; i<columns->len; i++)
		g_free (g_ptr_array_index(columns, i));
	g_ptr_array_free(columns, TRUE);
}

void 
mdb_append_index(GPtrArray *indices, MdbIndex *in_idx)
{
	MdbIndex *idx;

 	idx = g_memdup(in_idx,sizeof(MdbIndex));
	g_ptr_array_add(indices, idx);
}
void
mdb_free_indices(GPtrArray *indices)
{
	unsigned int i;

	if (!indices) return;
	for (i=0; i<indices->len; i++)
		g_free (g_ptr_array_index(indices, i));
	g_ptr_array_free(indices, TRUE);
}
