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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "mdbtools.h"

int
main(int argc, char **argv)
{
MdbHandle *mdb;
int j;
int page, start, stop;

	if (argc<4) {
		fprintf(stderr,"Usage: %s <file> <page> <start> <stop>\n",argv[0]);
		exit(1);
	}
	
	mdb = mdb_open(argv[1], MDB_NOFLAGS);

	mdb_read_catalog(mdb, MDB_TABLE);

	sscanf (argv [2], "%d", &page);
	sscanf (argv [3], "%d", &start);
	sscanf (argv [4], "%d", &stop);
	mdb_read_pg (mdb, page);
	for (j = start; j < stop; j++)
	  {
	    fprintf (stderr, "%4x %4x %c\n", j, mdb->pg_buf [j], mdb->pg_buf [j]);
	  }

	mdb_close(mdb);

	return 0;
}

