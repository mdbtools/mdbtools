/* MDB Tools - A library for reading MS Access database file
 * Copyright (C) 2000 Brian Bruns
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *   
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "mdbtools.h"

main(int argc, char **argv)
{
int rows;
int i;
unsigned char buf[2048];
MdbHandle *mdb;
MdbCatalogEntry entry;

#if 0
	if (argc<2) {
		fprintf(stderr,"Usage: %s <file> <table>\n",argv[0]);
		exit(1);
	}
	
	mdb_init();
	mdb = mdb_open(argv[1]);

	mdb_read_pg(mdb, MDB_CATALOG_PG);
	rows = mdb_catalog_rows(mdb);

	for (i=0;i<rows;i++) {
  		if (mdb_read_catalog_entry(mdb, i, &entry)) {
			if (!strcmp(entry.object_name,argv[2])) {
				mdb_kkd_dump(&entry);
  			}
		}
	}

	mdb_free_handle(mdb);
	mdb_exit();
#endif

	exit(0);
}

