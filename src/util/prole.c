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

void dump_ole(MdbTableDef *table, char *colname, char *sargname);

int
main(int argc, char **argv)
{
	MdbHandle *mdb;
	MdbTableDef *table;
	char *dot, *colname, *tabname;
	char *sargname = NULL;


	if (argc<2) {
		fprintf(stderr,"Usage: %s <file> <table.column> [sargs]\n",argv[0]);
		exit(1);
	}
	
	mdb = mdb_open(argv[1], MDB_NOFLAGS);
	dot = strchr(argv[2],'.');
	if (argc>3) sargname = argv[3];

	if (!dot) {
		fprintf(stderr,"Usage: %s <file> <table.column> [sarg]\n",argv[0]);
		exit(1);
	}
	tabname = argv[2];
	*dot='\0';
	colname = ++dot;

	table = mdb_read_table_by_name(mdb, tabname, MDB_TABLE);

	if (table) {
		mdb_read_columns(table);
		dump_ole(table, colname, sargname);
		mdb_free_tabledef(table);
	}

	mdb_close(mdb);
	return 0;
}

void dump_ole(MdbTableDef *table, char *colname, char *sargname)
{
char ole_data[200000];
int len;
MdbSarg sarg;
char *sargcol, *sargop, *sargval;

	mdb_bind_column_by_name(table, colname, ole_data, &len);

	if (sargname) {
		sargcol = strtok(sargname," ");
		sargop = strtok(NULL," ");
		sargval = strtok(NULL," ");
		printf("col %s op %s val %s\n",sargcol,sargop,sargval);
        	sarg.op = MDB_EQUAL; /* only support = for now, sorry */
		strcpy(sarg.value.s, sargval);
		mdb_add_sarg_by_name(table, sargcol, &sarg);
	}

        mdb_rewind_table(table);
	while (mdb_fetch_row(table)) {
		mdb_buffer_dump(ole_data, 0, len);
		printf("---\n");
	}

}

