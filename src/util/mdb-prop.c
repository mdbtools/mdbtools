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
#include "mdbver.h"

void dump_kkd(MdbHandle *mdb, void *kkd, size_t len);

int
main(int argc, char **argv)
{
	MdbHandle *mdb;
	MdbTableDef *table;
	char *table_name = NULL;
	char *locale = NULL;
	char *name;
	gchar *propColName;
	void *buf;
	int col_num;
	int found = 0;
	int print_mdbver = 0;
    GError *error = NULL;

	GOptionContext *opt_context;
	GOptionEntry entries[] = {
		{"version", 0, 0, G_OPTION_ARG_NONE, &print_mdbver, "Show mdbtools version and exit", NULL},
		{NULL}
	};
	opt_context = g_option_context_new("<file> <object name> [<prop col>] - display properties of an object in an Access database");
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
	if (argc != 3 && argc != 4) {
		fputs("Wrong number of arguments.\n\n", stderr);
		fputs(g_option_context_get_help(opt_context, TRUE, NULL), stderr);
		return 1;
	}

	table_name = g_locale_to_utf8(argv[2], -1, NULL, NULL, NULL);
	if (argc < 4)
		propColName = g_strdup("LvProp");
	else
		propColName = g_locale_to_utf8(argv[3], -1, NULL, NULL, NULL);
	setlocale(LC_CTYPE, locale);
	if (!table_name || !propColName) {
		return 1;
	}

	mdb = mdb_open(argv[1], MDB_NOFLAGS);
	if (!mdb) {
		return 1;
	}

	table = mdb_read_table_by_name(mdb, "MSysObjects", MDB_ANY);
	if (!table) {
		g_free(table_name);
		g_free(propColName);
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
		g_free(table_name);
		g_free(propColName);
		mdb_close(mdb);
		printf("Column %s not found in MSysObjects!\n", propColName);
		return 1;
	}

	while(mdb_fetch_row(table)) {
		if (!strcmp(name, table_name)) {
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
	g_free(table_name);

	if (!found) {
		printf("Object %s not found in database file!\n", propColName);
	}
	
	g_free(propColName);

	return !found;
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
