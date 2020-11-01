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

#define TABLE_NAME "Orders"
#define MDB_FILE "Northwind.mdb"
#define DELIMETER "\t"

void print_table(MdbTableDef *table);

main(int argc, char **argv)
{
	MdbHandle *mdb;
	MdbTableDef *table;

	if (!(mdb = mdb_open(MDB_FILE, MDB_NOFLAGS))) {
		exit(1);
	}
	
	table = mdb_read_table_by_name(mdb, TABLE_NAME, MDB_TABLE);

	if (table) {
		print_table(table);
		mdb_free_tabledef(table);
	}

	mdb_close(mdb);
	return 0;
}

void print_table(MdbTableDef *table)
{
int j;
/* doesn't handle tables > 256 columns.  Can that happen? */
char *bound_values[256]; 
MdbColumn *col;
MdbSarg sarg;

	mdb_read_columns(table);

	sarg.op = MDB_EQUAL;
	// sarg.value.i = 11070;
	strcpy(sarg.value.s, "Reggiani Caseifici");
	mdb_add_sarg_by_name(table, "ShipName", &sarg);

	mdb_rewind_table(table);
			
	for (j=0;j<table->num_cols;j++) {
		bound_values[j] = (char *) g_malloc(MDB_BIND_SIZE);
		bound_values[j][0] = '\0';
		ret = mdb_bind_column(table, j+1, bound_values[j], NULL);
		if (ret == -1) {
			fprintf(stderr, "Failed to bind column %d\n", i + 1);
			exit(1);
		}
	}

	/* print header */
	col=g_ptr_array_index(table->columns,0);
	fprintf(stdout,"%s",col->name);
	for (j=1;j<table->num_cols;j++) {
		col=g_ptr_array_index(table->columns,j);
		fprintf(stdout,"%s%s",DELIMETER,col->name);
	}
	fprintf(stdout,"\n");

	/* print each row */
	while(mdb_fetch_row(table)) {
		fprintf(stdout,"%s",bound_values[0]);
  		for (j=1;j<table->num_cols;j++) {
			fprintf(stdout,"%s%s",DELIMETER,bound_values[j]);
		}
		fprintf(stdout,"\n");
	}

	/* clean up */
	for (j=0;j<table->num_cols;j++) {
		g_free(bound_values[j]);
	}
}
