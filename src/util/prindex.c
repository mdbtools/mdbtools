/* MDB Tools - A library for reading MS Access database file
 * Copyright (C) 2000 Brian Bruns
 *
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

main(int argc, char **argv)
{
int rows;
int i, j;
unsigned char buf[2048];
MdbHandle *mdb;
MdbCatalogEntry *entry;
MdbTableDef *table;
MdbIndex *idx;
GList *l;
int found = 0;


	if (argc<4) {
		fprintf(stderr,"Usage: %s <file> <table> <index>\n",argv[0]);
		exit(1);
	}
	
	mdb_init();
	mdb = mdb_open(argv[1]);

	mdb_read_catalog(mdb, MDB_TABLE);

	for (i=0;i<mdb->num_catalog;i++) {
		entry = g_ptr_array_index(mdb->catalog,i);
		if (entry->object_type == MDB_TABLE &&
			!strcmp(entry->object_name,argv[2])) {
			        table = mdb_read_table(entry);
			        mdb_read_columns(table);
				mdb_read_indices(table);
				for (j=0;j<table->num_idxs;j++) {
					idx = g_ptr_array_index (table->indices, j);
					if (!strcmp(idx->name, argv[3])) {
						walk_index(mdb, idx);
					}
				}


				//mdb_table_dump(entry);
				found++;
		}
	}

	if (!found) {
		fprintf(stderr,"No table named %s found.\n", argv[2]);
	}
	mdb_free_handle(mdb);
	mdb_exit();

	exit(0);
}
void
walk_index(MdbHandle *mdb, MdbIndex *idx)
{
	int i, j, start, len;
	unsigned char byte;
	guint32 pg;
	int row;

	printf("name %s\n", idx->name);
	printf("root page %ld\n", idx->first_pg);
	mdb_read_pg(mdb, idx->first_pg);
	start = 0xf8; /* start byte of the index entries */
	len = -1;
	for (i=0x16;i<0xf8;i++) {
		byte = mdb->pg_buf[i];
		//printf("%02x ",byte); 
		for (j=0;j<8;j++) {
			len++;
			if ((1 << j) & byte) {
				// printf("start = %04x len = %d\n", start, len);
				buffer_dump(mdb->pg_buf, start, start+len-1);
				row = mdb->pg_buf[start+len-1];
				pg = mdb_get_int24_msb(mdb, start+len-4);
				printf("row = %d pg = %lu\n", row, pg);
				start += len;
				len = 0;
				// printf("\nbit %d set\n", j);
			}
		}
	}
}
