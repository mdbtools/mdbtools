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
#include "mdbver.h"

int main(int argc, char **argv) {
    
    unsigned int i;
    MdbHandle *mdb;
    MdbCatalogEntry *entry;
    MdbTableDef *table;
    int found = 0;
    char *locale = NULL;
    char *table_name = NULL;
    GError *error = NULL;
	int print_mdbver = 0;

	GOptionContext *opt_context;
	GOptionEntry entries[] = {
		{"version", 0, 0, G_OPTION_ARG_NONE, &print_mdbver, "Show mdbtools version and exit", NULL},
		{NULL}
	};
	opt_context = g_option_context_new("<file> <table> - print the number of records in an Access database");
	g_option_context_add_main_entries(opt_context, entries, NULL /*i18n*/);
	locale = setlocale(LC_CTYPE, "");
	if (!g_option_context_parse (opt_context, &argc, &argv, &error))
	{
		fprintf(stderr, "option parsing failed: %s\n", error->message);
		fputs(g_option_context_get_help(opt_context, TRUE, NULL), stderr);
		return 1;
	}
	if (print_mdbver) {
		if (argc > 1) {
			fputs(g_option_context_get_help(opt_context, TRUE, NULL), stderr);
		}
		fprintf(stdout,"%s\n", MDB_FULL_VERSION);
		exit(argc > 1);
	}
	if (argc != 3) {
		fputs("Wrong number of arguments.\n\n", stderr);
		fputs(g_option_context_get_help(opt_context, TRUE, NULL), stderr);
		return 1;
	}
	table_name = g_locale_to_utf8(argv[2], -1, NULL, NULL, &error);
	setlocale(LC_CTYPE, locale);
	if (!table_name) {
		fprintf(stderr, "Error converting table argument: %s\n", error->message);
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
		if (entry->object_type == MDB_TABLE && !g_ascii_strcasecmp(entry->object_name, table_name)) {
            table = mdb_read_table(entry);
            fprintf(stdout, "%d\n", table->num_rows);
            found = 1;
            break;
		}
	}
    
    // check was found:
	if (!found) {
		fprintf(stderr, "No table named %s found (among %d tables in file).\n", table_name, mdb->num_catalog);
        return 1;
	}

	mdb_close(mdb);
	return 0;
}
