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

int
main(int argc, char **argv)
{

#if 0
	MdbHandle *mdb;
	MdbTableDef *table;

	if (argc<3) {
		fprintf(stderr,"Usage: %s <file> <table>\n",argv[0]);
		exit(1);
	}
	
	mdb_init();
	mdb = mdb_open(argv[1], MDB_NOFLAGS);

	table = mdb_read_table_by_name(mdb, argv[2], MDB_TABLE);
	if (!table) {
		fprintf(stderr,"No table named %s found.\n", argv[2]);
		exit(1);
	}
	/* FIXME: table->entry->*kkd* are not initialized */
	mdb_kkd_dump(table->entry);

	mdb_close(mdb);
	mdb_exit();
#endif
	exit(0);
}
