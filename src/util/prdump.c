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
MdbCatalogEntry entry;
MdbTableDef *table;
GList *l;
int j;
int page, start, stop;

	if (argc<4) {
		fprintf(stderr,"Usage: %s <file> <page> <start> <stop>\n",argv[0]);
		exit(1);
	}
	
	mdb_init();
	mdb = mdb_open(argv[1]);

	mdb_read_catalog(mdb, MDB_TABLE);

	sscanf (argv [2], "%d", &page);
	sscanf (argv [3], "%d", &start);
	sscanf (argv [4], "%d", &stop);
	mdb_read_pg (mdb, page);
	for (j = start; j < stop; j++)
	  {
	    fprintf (stderr, "%4x %4x %c\n", j, mdb->pg_buf [j], mdb->pg_buf [j]);
	  }

	mdb_free_handle(mdb);
	mdb_exit();

	exit(0);
}

