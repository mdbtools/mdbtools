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

/* this utility dumps the schema for an existing database */
#include "mdbtools.h"

int
main (int argc, char **argv)
{
	MdbHandle *mdb;
	char *tabname = NULL;
	char *namespace = NULL;
	guint32 export_options;
	int opt_drop_table = MDB_SHEXP_DEFAULT & MDB_SHEXP_DROPTABLE;
	int opt_not_null = MDB_SHEXP_DEFAULT & MDB_SHEXP_CST_NOTNULL;
	int opt_def_values = MDB_SHEXP_DEFAULT & MDB_SHEXP_DEFVALUES;
	int opt_not_empty = MDB_SHEXP_DEFAULT & MDB_SHEXP_CST_NOTEMPTY;
	int opt_comments = MDB_SHEXP_DEFAULT & MDB_SHEXP_COMMENTS;
	int opt_indexes = MDB_SHEXP_DEFAULT & MDB_SHEXP_INDEXES;
	int opt_relations = MDB_SHEXP_DEFAULT & MDB_SHEXP_RELATIONS;

	GOptionEntry entries[] = {
		{ "table", 'T', 0, G_OPTION_ARG_STRING, &tabname, "Only create schema for named table", "table"},
		{ "namespace", 'N', 0, G_OPTION_ARG_STRING, &namespace, "Prefix identifiers with namespace", "namespace"},
		{ "drop-table", 0, 0, G_OPTION_ARG_NONE, &opt_drop_table, "Include DROP TABLE statements", NULL},
		{ "no-drop-table", 0, G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, &opt_drop_table, "Don't include DROP TABLE statements", NULL},
		{ "not-null", 0, 0, G_OPTION_ARG_NONE, &opt_not_null, "Include NOT NULL constraints", NULL},
		{ "no-not-null", 0, G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, &opt_not_null, "Don't include NOT NULL constraints", NULL},
		{ "default-values", 0, 0, G_OPTION_ARG_NONE, &opt_def_values, "Include default values", NULL},
		{ "no-default-values", 0, G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, &opt_def_values, "Don't include default values", NULL},
		{ "not-empty", 0, 0, G_OPTION_ARG_NONE, &opt_not_empty, "Include not empty constraints", NULL},
		{ "no-not_empty", 0, G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, &opt_not_empty, "Don't include not empty constraints", NULL},
		{ "comments", 0, 0, G_OPTION_ARG_NONE, &opt_comments, "Include COMMENT ON statements", NULL},
		{ "no-comments", 0, G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, &opt_comments, "Don't include COMMENT statements.", NULL},
		{ "indexes", 0, 0, G_OPTION_ARG_NONE, &opt_indexes, "Include indexes", NULL},
		{ "no-indexes", 0, G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, &opt_indexes, "Don't include indexes", NULL},
		{ "relations", 0, 0, G_OPTION_ARG_NONE, &opt_relations, "Include foreign key constraints", NULL},
		{ "no-relations", 0, G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, &opt_relations, "Don't include foreign key constraints", NULL},
		{ NULL },
	};
	GError *error = NULL;
	GOptionContext *opt_context;

	opt_context = g_option_context_new("<file> [<backend>] - Dump schema");
	g_option_context_add_main_entries(opt_context, entries, NULL /*i18n*/);
	// g_option_context_set_strict_posix(opt_context, TRUE); /* options first, requires glib 2.44 */
	if (!g_option_context_parse (opt_context, &argc, &argv, &error))
	{
		fprintf(stderr, "option parsing failed: %s\n", error->message);
		fputs(g_option_context_get_help(opt_context, TRUE, NULL), stderr);
		exit (1);
	}

	if (argc < 2 || argc > 3) {
		fputs("Wrong number of arguments.\n\n", stderr);
		fputs(g_option_context_get_help(opt_context, TRUE, NULL), stderr);
		exit(1);
	}

	/* open the database */
	mdb = mdb_open (argv[1], MDB_NOFLAGS);
	if (!mdb) {
		fprintf(stderr, "Could not open file\n");
		exit(1);
	}

	if (argc == 3) {
		if (!mdb_set_default_backend(mdb, argv[2])) {
			fprintf(stderr, "Invalid backend type\n");
			exit(1);
		}
	}

	/* read the catalog */
 	if (!mdb_read_catalog (mdb, MDB_TABLE)) {
		fputs("File does not appear to be an Access database\n", stderr);
		exit(1);
	}

	export_options = 0;
	if (opt_drop_table)
		export_options |= MDB_SHEXP_DROPTABLE;
	if (opt_not_null)
		export_options |= MDB_SHEXP_CST_NOTNULL;
	if (opt_def_values)
		export_options |= MDB_SHEXP_DEFVALUES;
	if (opt_not_empty)
		export_options |= MDB_SHEXP_CST_NOTEMPTY;
	if (opt_comments)
		export_options |= MDB_SHEXP_COMMENTS;
	if (opt_indexes)
		export_options |= MDB_SHEXP_INDEXES;
	if (opt_relations)
		export_options |= MDB_SHEXP_RELATIONS;
	mdb_print_schema(mdb, stdout, tabname, namespace, export_options);

	mdb_close (mdb);

	g_option_context_free(opt_context);
	g_free(namespace);
	g_free(tabname);
	return 0;
}

