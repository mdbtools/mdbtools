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

void dump_ole(MdbTableDef *table, char *colname, char *sargname);

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
char *sargname = NULL;


	if (argc<2) {
		fprintf(stderr,"Usage: %s <file> <table.column> [sargs]\n",argv[0]);
		exit(1);
	}
	
	mdb_init();
	mdb = mdb_open(argv[1]);
	dot = strchr(argv[2],'.');
	if (argc>3) sargname = argv[3];

	if (!dot) {
		fprintf(stderr,"Usage: %s <file> <table.column> [sarg]\n",argv[0]);
		exit(1);
	}
	tabname = argv[2];
	*dot='\0';
	colname = ++dot;

	mdb_read_catalog(mdb, MDB_TABLE);

	for (i=0;i<mdb->num_catalog;i++) {
		entry = g_ptr_array_index(mdb->catalog,i);
		if (entry->object_type == MDB_TABLE &&
			!strcmp(entry->object_name,tabname)) {
				table = mdb_read_table(entry);
				mdb_read_columns(table);
				dump_ole(table, colname, sargname);
		}
	}

	mdb_free_handle(mdb);
	mdb_exit();

	exit(0);
}

void dump_ole(MdbTableDef *table, char *colname, char *sargname)
{
int i, found = 0;
char ole_data[200000];
int len;
MdbColumn *col;
MdbSarg sarg;
char *sargcol, *sargop, *sargval;

	for (i=0;i<=table->num_cols;i++) {
		col=g_ptr_array_index(table->columns,i);
		printf("%d colname %s\n", i, col->name);
		if (col && !strcmp(col->name,colname)) {
			found = i+1;
		}
	}
	printf("column %d\n",found);
	mdb_bind_column(table, found, ole_data);
	mdb_bind_len(table, found, &len);

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
		buffer_dump(ole_data, 0, len);
		printf("---\n");
	}

}

