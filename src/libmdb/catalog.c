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

#ifdef DMALLOC
#include "dmalloc.h"
#endif

char *
mdb_get_objtype_string(int obj_type)
{
static char *type_name[] = {"Form",
			"Table",
			"Macro",
			"System Table",
			"Report",
			"Query",
			"Linked Table",
			"Module",
			"Relationship",
			"Unknown 0x09",
			"Unknown 0x0a",
			"Database"
		};

	if (obj_type > 11) {
		return NULL;
	} else {
		return type_name[obj_type]; 
	}
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

GPtrArray *mdb_read_catalog (MdbHandle *mdb, int objtype)
{
	MdbCatalogEntry *entry, msysobj;
	MdbTableDef *table;
	char obj_id[256];
	char obj_name[256];
	char obj_type[256];
	char obj_flags[256];
	int type;

	if (mdb->catalog) mdb_free_catalog(mdb);
	mdb->catalog = g_ptr_array_new();
	mdb->num_catalog = 0;

	/* dummy up a catalog entry so we may read the table def */
	memset(&msysobj, 0, sizeof(MdbCatalogEntry));
	msysobj.mdb = mdb;
	msysobj.object_type = MDB_TABLE;
	msysobj.table_pg = 2;
	strcpy(msysobj.object_name, "MSysObjects");

	/* mdb_table_dump(&msysobj); */

	table = mdb_read_table(&msysobj);
	if (!table) return NULL;

	mdb_read_columns(table);

	mdb_bind_column_by_name(table, "Id", obj_id);
	mdb_bind_column_by_name(table, "Name", obj_name);
	mdb_bind_column_by_name(table, "Type", obj_type);
	mdb_bind_column_by_name(table, "Flags", obj_flags);

	mdb_rewind_table(table);

	while (mdb_fetch_row(table)) {
		type = atoi(obj_type);
		if (objtype==MDB_ANY || type == objtype) {
			// fprintf(stdout, "obj_id: %10ld objtype: %-3d obj_name: %s\n", 
			// (atol(obj_id) & 0x00FFFFFF), type, obj_name); 
			entry = (MdbCatalogEntry *) g_malloc0(sizeof(MdbCatalogEntry));
			entry->mdb = mdb;
			strcpy(entry->object_name, obj_name);
			entry->object_type = (type & 0x7F);
			entry->table_pg = atol(obj_id) & 0x00FFFFFF;
			entry->flags = atol(obj_flags);
			mdb->num_catalog++;
			g_ptr_array_add(mdb->catalog, entry); 
		}
	}
	//mdb_dump_catalog(mdb, MDB_TABLE);
 
	mdb_free_tabledef(table);

	return mdb->catalog;
}

void 
mdb_dump_catalog(MdbHandle *mdb, int obj_type)
{
	unsigned int i;
	MdbCatalogEntry *entry;

	mdb_read_catalog(mdb, obj_type);
	for (i=0;i<mdb->num_catalog;i++) {
                entry = g_ptr_array_index(mdb->catalog,i);
		if (obj_type==MDB_ANY || entry->object_type==obj_type) {
			fprintf(stdout,"Type: %-10s Name: %-18s T pg: %04x KKD pg: %04x row: %2d\n",
			mdb_get_objtype_string(entry->object_type),
			entry->object_name,
			(unsigned int) entry->table_pg,
			(unsigned int) entry->kkd_pg,
			entry->kkd_rowid);
		}
        }
	return;
}

