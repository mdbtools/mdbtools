/* MDB Tools - A library for reading MS Access database file
 * Copyright (C) 2000-2011 Brian Bruns and others
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

void dump_kkd(MdbHandle *mdb, void *kkd, size_t len);

int
main(int argc, char **argv)
{
	MdbHandle *mdb;
	MdbTableDef *table;
	char *name;
	gchar *propColName;
	void *buf;
	int col_num;
	int found = 0;

	if (argc < 3) {
		fprintf(stderr,"Usage: %s <file> <object name> [<prop col>]\n",
			argv[0]);
		return 1;
	}
	if (argc < 4)
		propColName = "LvProp";
	else
		propColName = argv[3];

	mdb = mdb_open(argv[1], MDB_NOFLAGS);
	if (!mdb) {
		return 1;
	}

	table = mdb_read_table_by_name(mdb, "MSysObjects", MDB_ANY);
	if (!table) {
		mdb_close(mdb);
		return 1;
	}
	mdb_read_columns(table);
	mdb_rewind_table(table);

	name = g_malloc(mdb->bind_size);
	buf = g_malloc(mdb->bind_size);
	mdb_bind_column_by_name(table, "Name", name, NULL);
	col_num = mdb_bind_column_by_name(table, propColName, buf, NULL);
	if (col_num < 1) {
		g_free(name);
		g_free(buf);
		mdb_free_tabledef(table);
		mdb_close(mdb);
		printf("Column %s not found in MSysObjects!\n", argv[3]);
		return 1;
	}

	while(mdb_fetch_row(table)) {
		if (!strcmp(name, argv[2])) {
			found = 1;
			break;
		}
	}

	if (found) {
		MdbColumn *col = g_ptr_array_index(table->columns, col_num-1);
		size_t size;
		void *kkd = mdb_ole_read_full(mdb, col, &size);
		if (size)
			dump_kkd(mdb, kkd, size);
		else
			printf("No properties.\n");
		free(kkd);
	}

	g_free(name);
	g_free(buf);
	mdb_free_tabledef(table);
	mdb_close(mdb);
	
	return 0;
}
void dump_kkd(MdbHandle *mdb, void *kkd, size_t len)
{
	GPtrArray *aprops = mdb_kkd_to_props(mdb, kkd, len);
	guint i;
	if (!aprops)
		return;
	for (i=0; i<aprops->len; ++i) {
		MdbProperties *props = g_ptr_array_index(aprops, i);
		mdb_dump_props(props, stdout, 1);
	}
}
