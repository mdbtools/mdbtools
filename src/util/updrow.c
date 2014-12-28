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

void read_to_row(MdbTableDef *table, char *sargname);

int
main(int argc, char **argv)
{
MdbHandle *mdb;
MdbTableDef *table;
char *colname, *tabname;
char *colval;
int colnum;
char *sargname = NULL;
char *updstr = NULL;
char data[255];
int len;


	if (argc<4) {
		fprintf(stderr,"Usage: %s <file> <table> <sargs> <updstr>\n",argv[0]);
		exit(1);
	}
	
	mdb = mdb_open(argv[1], MDB_WRITABLE);
	tabname = argv[2];
	sargname = argv[3];
	updstr = g_strdup(argv[4]);

	table = mdb_read_table_by_name(mdb, tabname, MDB_TABLE);

	if (table) {
		mdb_read_columns(table);
		mdb_read_indices(table);
		printf("updstr %s\n",updstr);
		colname = strtok(updstr,"=");
		colval = strtok(NULL,"=");
		colnum = mdb_bind_column_by_name(table, colname, data, &len);
		printf("column %d\n", colnum);
		read_to_row(table, sargname);
		printf("current value of %s is %s, changing to %s\n", colname, data, colval);
		len = strlen(colval);
		strcpy(data,colval);
		mdb_update_row(table);
		mdb_free_tabledef(table);
	}

	mdb_close(mdb);
	return 0;
}

void read_to_row(MdbTableDef *table, char *sargname)
{
	static MdbSargNode sarg;
	char *sargcol, *sargop, *sargval;
	unsigned int i;
	MdbColumn *col;


	if (sargname) {
		sargcol = strtok(sargname," ");
		for (i=0;i<table->num_cols;i++) {
			col=g_ptr_array_index(table->columns,i);
			if (!g_ascii_strcasecmp(col->name, (char *)sargcol)) {
				sarg.col = col;
				break;
			}
		}

		sargop = strtok(NULL," ");
		sargval = strtok(NULL," ");
		printf("col %s op %s val %s\n",sargcol,sargop,sargval);
        	sarg.op = MDB_EQUAL; /* only support = for now, sorry */
		sarg.value.i = atoi(sargval);
		table->sarg_tree = &sarg;

		// mdb_add_sarg_by_name(table, sargcol, &sarg);
	}

        mdb_rewind_table(table);
	while (mdb_fetch_row(table)) {
		printf("row found at page %d row %d\n", 
			table->cur_phys_pg, table->cur_row);
		return;
	}
}

