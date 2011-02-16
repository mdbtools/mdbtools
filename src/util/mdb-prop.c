/* MDB Tools - A library for reading MS Access database file
 * Copyright (C) 2000-2011 Brian Bruns and others
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

void dump_kkd(MdbHandle *mdb, void *kkd, size_t len);

int
main(int argc, char **argv)
{
	MdbHandle *mdb;
	MdbTableDef *table;
	gchar name[256];
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

	mdb_init();

	mdb = mdb_open(argv[1], MDB_NOFLAGS);
	if (!mdb) {
		mdb_exit();
		return 1;
	}

	table = mdb_read_table_by_name(mdb, "MSysObjects", MDB_ANY);
	if (!table) {
		mdb_close(mdb);
		mdb_exit();
		return 1;
	}
	mdb_read_columns(table);
	mdb_rewind_table(table);

	mdb_bind_column_by_name(table, "Name", name, NULL);
	buf = g_malloc(MDB_BIND_SIZE);
	col_num = mdb_bind_column_by_name(table, propColName, buf, NULL);
	if (col_num < 1) {
		g_free(buf);
		mdb_free_tabledef(table);
		mdb_close(mdb);
		mdb_exit();
		printf("Column %s not found!\n", argv[3]);
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
		dump_kkd(mdb, kkd, size);
		free(kkd);
	}

	g_free(buf);
	mdb_free_tabledef(table);
	mdb_close(mdb);
	mdb_exit();

	return 0;
}
void dump_kkd(MdbHandle *mdb, void *kkd, size_t len)
{
	GArray *aprops = kkd_to_props(mdb, kkd, len);
	int i;
	for (i=0; i<aprops->len; ++i) {
		MdbProperties *props = g_array_index(aprops, MdbProperties*, i);
		mdb_dump_props(props, stdout, 1);
	}
}
