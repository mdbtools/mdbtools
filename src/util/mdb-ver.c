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
MdbCatalogEntry entry;
MdbTableDef *table;
MdbColumn *col;
/* doesn't handle tables > 256 columns.  Can that happen? */
char *bound_values[256]; 
char *delimiter = ",";
char header_row = 1;
char quote_text = 1;
int  opt;

	/* 
	** optind is now the position of the first non-option arg, 
	** see getopt(3) 
	*/
	if (argc < 2) {
		fprintf(stderr,"Usage: %s <file>\n",argv[0]);
		exit(1);
	}

	mdb_init();

	if (!(mdb = mdb_open(argv[optind]))) {
		exit(1);
	}
	if (IS_JET3(mdb)) {
		printf("JET3\n");
	} else if (IS_JET4(mdb)) {
		printf("JET4\n");
	} else {
		printf("unknown\n");
	}
	
	mdb_free_handle(mdb);
	mdb_exit();

	exit(0);
}

