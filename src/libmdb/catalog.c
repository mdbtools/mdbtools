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

MdbCatalogEntry *mdb_catalog_entry(MdbHandle *mdb, int rowid, MdbCatalogEntry *entry)
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
	if (offset & 0xF000) offset &= ~0xF000;

/*
for (j=offset;j<offset+32;j++)
	fprintf(stdout,"%02x ",mdb->pg_buf[j]);
fprintf(stdout,"\n");
*/

	memset(entry, '\0', sizeof(MdbCatalogEntry));
	entry->object_type = mdb->pg_buf[offset+0x09];
	j=0;
	entry->mdb = mdb;
	entry->table_pg = mdb_get_int16(mdb,offset+1);
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
int mdb_catalog_rows(MdbHandle *mdb)
{
	return mdb_get_int16(mdb, 0x08);
}
GList *mdb_read_catalog(MdbHandle *mdb, int obj_type)
{
int i;
int rows;
MdbCatalogEntry entry;
gpointer data;
int next_pg, next_pg_off;

/* 
** We are doing it the brute force way, since I can't make sense of the page
** linkage on catalog pages. What I know (or think I know) is this: some row
** offsets in the row offset table (that list of offsets at the begining of 
** the page) that the high order nibble of 0x4.  The offset then represents
** the location of a page pointer to another catalog page, however not all
** catalog pages are linked in this manner.
**
** So, we simply read the entire mdb file for pages that start 0x01 0x01 and 
** have a 32bit value of 2 (02 00 00 00) in bytes 4-7.
*/
#if 0
	next_pg = MDB_CATALOG_PG;
	mdb_free_catalog(mdb);

	while (next_pg) {
		mdb_read_pg(mdb, next_pg);
		next_pg = 0;
		rows = mdb_catalog_rows(mdb);
		for (i=0;i<rows;i++) {
			if (mdb->pg_buf[11 + 2 * i] & 0x40) {
				next_pg_off = mdb_get_int16(mdb, 10 + 2 * i) & 0x0FFF;
				next_pg = mdb_get_int16(mdb, next_pg_off+1);
				fprintf(stdout,"YES! next pg = %04x %d\n",next_pg, next_pg); 
				continue;
			}
			if (mdb_catalog_entry(mdb, i, &entry)) {
				data = g_memdup(&entry,sizeof(MdbCatalogEntry));
				mdb->catalog = g_list_append(mdb->catalog, data);
			}
		}
	}
	return (mdb->catalog);
#endif
	next_pg=0;
	while (mdb_read_pg(mdb,next_pg)) {
		if (mdb->pg_buf[0]==0x01 && 
		mdb->pg_buf[1]==0x01 &&
		mdb_get_int32(mdb,4)==2) {
			rows = mdb_catalog_rows(mdb);
			for (i=0;i<rows;i++) {
				if (mdb->pg_buf[11 + 2 * i] & 0x40) continue;
				if (mdb_catalog_entry(mdb, i, &entry)) {
					data = g_memdup(&entry,sizeof(MdbCatalogEntry));
					mdb->catalog = g_list_append(mdb->catalog, data);
				}
			}
		}
		next_pg++;
	}
}
void mdb_dump_catalog(MdbHandle *mdb, int obj_type)
{
int rows;
GList *l;
MdbCatalogEntry *entryp;

	mdb_read_catalog(mdb, obj_type);
	for (l=g_list_first(mdb->catalog);l;l=g_list_next(l)) {
                entryp = l->data;
		if (obj_type==-1 || entryp->object_type==obj_type) {
			fprintf(stdout,"Type: %-10s Name: %-18s T pg: %04x KKD pg: %04x row: %2d\n",
			mdb_get_objtype_string(entryp->object_type),
			entryp->object_name,
			entryp->table_pg,
			entryp->kkd_pg,
			entryp->kkd_rowid);
		}
        }
	return;
}

