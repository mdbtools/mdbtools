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

int
main(int argc, char **argv)
{
MdbHandle *mdb;
int j;
long int pgnum = 0, row_start, map_sz;
int coln = 1;
unsigned char *map_buf;

	if (argc<2) {
		fprintf(stderr,"Usage: %s <file>\n",argv[0]);
		exit(1);
	}
	
	mdb_init();
	mdb = mdb_open(argv[1], MDB_NOFLAGS);

	mdb_read_pg (mdb, 1);
	mdb_find_row(mdb, 0, &row_start, &map_sz);
	map_buf = &mdb->pg_buf[row_start];
	map_sz --;
	/*
	 * trim the end of a type 0 map
	 */
	if (map_buf[0]==0)
		while (map_buf[map_sz]==0xff) map_sz--;

	while (pgnum = mdb_map_find_next(mdb, map_buf, map_sz, pgnum)) {
		printf("%6lu ",(long unsigned) pgnum);
		if (coln==10) {
			printf("\n");
			coln = 0;
		}
		coln++;
	}
	if (coln!=1) printf("\n");

	mdb_close(mdb);
	mdb_exit();

	exit(0);
}

