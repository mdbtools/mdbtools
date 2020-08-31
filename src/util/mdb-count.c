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

int main(int argc, char **argv) {
    
    unsigned int i;
    MdbHandle *mdb;
    MdbCatalogEntry *entry;
    MdbTableDef *table;
    int found = 0;

	if (argc < 3) {
		fprintf(stderr, "Usage: %s <file> <table>\n", argv[0]);
        return 1;
	}
	
    // open db and try to read table:
	mdb = mdb_open(argv[1], MDB_NOFLAGS);
    if (!mdb) {
        return 1;
    }
	if (!mdb_read_catalog(mdb, MDB_TABLE)) {
        return 1;
    }
	for (i = 0; i < mdb->num_catalog; i++) {
		entry = g_ptr_array_index(mdb->catalog, i);
		if (entry->object_type == MDB_TABLE && !g_ascii_strcasecmp(entry->object_name, argv[2])) {
            table = mdb_read_table(entry);
            fprintf(stdout, "%d\n", table->num_rows);
            found = 1;
            break;
		}
	}
    
    // check was found:
	if (!found) {
		fprintf(stderr, "No table named %s found (among %d tables in file).\n", argv[2], mdb->num_catalog);
        return 1;
	}

	mdb_close(mdb);
	return 0;
}
