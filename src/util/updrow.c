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

void read_to_row(MdbTableDef *table, char *sargname);

main(int argc, char **argv)
{
int rows;
int i;
unsigned char buf[2048];
MdbHandle *mdb;
MdbCatalogEntry *entry;
MdbTableDef *table;
GList *l;
char *dot, *colname, *tabname;
char *colop, *colval;
char *sargname = NULL;
char *updstr = NULL;
unsigned char data[255];
int len;


	if (argc<4) {
		fprintf(stderr,"Usage: %s <file> <table> <sargs> <updstr>\n",argv[0]);
		exit(1);
	}
	
	mdb_init();
	mdb = _mdb_open(argv[1], TRUE);
	tabname = argv[2];
	sargname = argv[3];
	updstr = strdup(argv[4]);

	mdb_read_catalog(mdb, MDB_TABLE);

	for (i=0;i<mdb->num_catalog;i++) {
		entry = g_ptr_array_index(mdb->catalog,i);
		if (entry->object_type == MDB_TABLE &&
			!strcmp(entry->object_name,tabname)) {
				table = mdb_read_table(entry);
				mdb_read_columns(table);
				mdb_read_indices(table);
				printf("updstr %s\n",updstr);
				colname = strtok(updstr,"=");
				colval = strtok(NULL,"=");
				bind_column(table, colname, data, &len);
				read_to_row(table, sargname);
				printf("current value of %s is %s, changing to %s\n", colname, data, colval);
				len = strlen(colval);
				strcpy(data,colval);
				mdb_update_row(table);
		}
	}

	mdb_free_handle(mdb);
	mdb_exit();

	exit(0);
}

int bind_column(MdbTableDef *table, char *colname, unsigned char *data, int *len)
{
int i, found = 0;
MdbColumn *col;

	for (i=0;i<table->num_cols;i++) {
		col=g_ptr_array_index(table->columns,i);
		printf("%d colname %s\n", i, col->name);
		if (col && !strcmp(col->name,colname)) {
			found = i+1;
		}
	}
	printf("column %d\n",found);
	mdb_bind_column(table, found, data);
	mdb_bind_len(table, found, len);
}
void read_to_row(MdbTableDef *table, char *sargname)
{
MdbSarg sarg;
char *sargcol, *sargop, *sargval;


	if (sargname) {
		sargcol = strtok(sargname," ");
		sargop = strtok(NULL," ");
		sargval = strtok(NULL," ");
		printf("col %s op %s val %s\n",sargcol,sargop,sargval);
        	sarg.op = MDB_EQUAL; /* only support = for now, sorry */
		sarg.value.i = atoi(sargval);
		mdb_add_sarg_by_name(table, sargcol, &sarg);
	}

        mdb_rewind_table(table);
	while (mdb_fetch_row(table)) {
		printf("row found at page %d row %d\n", 
			table->cur_phys_pg, table->cur_row);
		return;
	}
}

