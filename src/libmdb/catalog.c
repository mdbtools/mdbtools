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

char *mdb_get_objtype_string(int obj_type)
{
static char *type_name[] = {"Form",
			"Table",
			"Macro",
			"System Table",
			"Report",
			"Query",
			"Linked Table",
			"Module",
			"Unknown 0x08",
			"Unknown 0x09",
			"Unknown 0x0a",
			"Unknown 0x0b"
		};

	if (obj_type > 11) {
		return NULL;
	} else {
		return type_name[obj_type]; 
	}
}

MDB_CATALOG_ENTRY *mdb_catalog_entry(MDB_HANDLE *mdb, int rowid, MDB_CATALOG_ENTRY *entry)
{
int offset;
int rows;
int i,j;

	rows = mdb_get_int16(mdb, 8);

	if (rowid < 0 || rowid > rows) return NULL;

	offset = mdb_get_int16(mdb, 10 + 2 * rowid);
	/* 
	** ??? this happens, don't know what it means 
	*/
	if (offset & 0xF000) return NULL;

/*
for (j=offset;j<offset+32;j++)
	fprintf(stdout,"%02x ",mdb->pg_buf[j]);
fprintf(stdout,"\n");
*/

	memset(entry, '\0', sizeof(MDB_CATALOG_ENTRY));
	entry->object_type = mdb->pg_buf[offset+0x09];
	j=0;
	for (i=offset+31;isprint(mdb->pg_buf[i]);i++) {
		if (j<=MDB_MAX_OBJ_NAME) {
			entry->object_name[j++]=mdb->pg_buf[i];
		}
	}
	entry->object_name[j] = '\0';
	entry->kkd_pg = mdb_get_int16(mdb,offset+31+strlen(entry->object_name)+7);
	entry->kkd_rowid = mdb->pg_buf[offset+31+strlen(entry->object_name)+6];

	return entry;
}
int mdb_catalog_rows(MDB_HANDLE *mdb)
{
	return mdb_get_int16(mdb, 0x08);
}
GList *mdb_read_catalog(MDB_HANDLE *mdb, int obj_type)
{
int i;
int rows;
MDB_CATALOG_ENTRY entry;
gpointer data;

	mdb_read_pg(mdb, MDB_CATALOG_PG);
	rows = mdb_catalog_rows(mdb);
	mdb_free_catalog(mdb);
	for (i=0;i<rows;i++) {
		if (mdb_catalog_entry(mdb, i, &entry)) {
			data = g_memdup(&entry,sizeof(MDB_CATALOG_ENTRY));
			mdb->catalog = g_list_append(mdb->catalog, data);
		}
	}
	return (mdb->catalog);
}
void mdb_dump_catalog(MDB_HANDLE *mdb, int obj_type)
{
int rows;
GList *l;
MDB_CATALOG_ENTRY *entryp;

	mdb_read_catalog(mdb, obj_type);
	for (l=g_list_first(mdb->catalog);l;l=g_list_next(l)) {
                entryp = l->data;
		fprintf(stdout,"Type: %-15s Name: %-30s KKD pg: %04x row: %2d\n",
			mdb_get_objtype_string(entryp->object_type),
			entryp->object_name,
			entryp->kkd_pg,
			entryp->kkd_rowid);
        }
	return;
}

