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
	MdbTableDef *table;

	if (argc<2) {
		fprintf(stderr,"Usage: %s <file> <table>\n",argv[0]);
		exit(1);
	}
	
	mdb_init();
	mdb = mdb_open(argv[1], MDB_NOFLAGS);

	table = mdb_read_table_by_name(mdb, argv[2], MDB_TABLE);
	if (table) {
		mdb_read_columns(table);
		mdb_data_dump(table);
		mdb_free_tabledef(table);
	}

	mdb_close(mdb);
	mdb_exit();

	exit(0);
}

