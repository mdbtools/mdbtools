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
int i;
unsigned char buf[2048];
MdbHandle *mdb;
MdbCatalogEntry *entry;
GList *l;
int found = 0;


	if (argc<3) {
		fprintf(stderr,"Usage: %s <file> <table>\n",argv[0]);
		exit(1);
	}
	
	mdb_init();
	mdb = mdb_open(argv[1]);

	mdb_read_catalog(mdb, MDB_TABLE);

	for (i=0;i<mdb->num_catalog;i++) {
		entry = g_ptr_array_index(mdb->catalog,i);
		if (entry->object_type == MDB_TABLE &&
			!strcmp(entry->object_name,argv[2])) {
				mdb_table_dump(entry);
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

