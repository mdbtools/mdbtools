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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* this utility dumps the schema for an existing database */
#include <ctype.h>
#include <getopt.h>
#include "mdbtools.h"

#ifdef DMALLOC
#include "dmalloc.h"
#endif

int
main (int argc, char **argv)
{
	MdbHandle *mdb;
	char *tabname = NULL;
	char *namespace = NULL;
	guint32 export_options = MDB_SHEXP_DEFAULT;
	int opt;

	if (argc < 2) {
		fprintf (stderr, "Usage: %s [options] <file> [<backend>]\n",argv[0]);
		fprintf (stderr, "where options are:\n");
		fprintf (stderr, "  -T <table>     Only create schema for named table\n");
		fprintf (stderr, "  -N <namespace> Prefix identifiers with namespace\n");
		fprintf (stderr, "  -S             Sanitize names (replace spaces etc. with underscore)\n");
		exit (1);
	}

	int digit_optind = 0;
	while (1) {
		int this_option_optind = optind ? optind : 1;
		int option_index = 0;
		static struct option long_options[] = {
			{"table", 1, NULL, 'T'},
			{"namespace", 1, NULL, 'N'},
			{"drop-table", 0, NULL, 0},
			{"no-drop-table", 0, NULL, 0},
			{"not-null", 0, NULL, 0},
			{"no-not-null", 0, NULL, 0},
			{"not-empty", 0, NULL, 0},
			{"no-not-empty", 0, NULL, 0},
			{"description", 0, NULL, 0},
			{"no-description", 0, NULL, 0},
			{"indexes", 0, NULL, 0},
			{"no-indexes", 0, NULL, 0},
			{"relations", 0, NULL, 0},
			{"no-relations", 0, NULL, 0},
			{"sanitize", 0, NULL, 'S'},
			{"no-sanitize", 0, NULL, 0},
			{NULL, 0, NULL, 0},
		};
		opt = getopt_long(argc, argv, "T:N:S", long_options, &option_index);
		if (opt == -1)
			break;

		switch (opt) {
		case 0:
			if (!strcmp(long_options[option_index].name, "drop-table")) {
				export_options |= MDB_SHEXP_DROPTABLE;
				break;
			}
			if (!strcmp(long_options[option_index].name, "no-drop-table")) {
				export_options &= ~MDB_SHEXP_DROPTABLE;
				break;
			}
			if (!strcmp(long_options[option_index].name, "not-null")) {
				export_options |= MDB_SHEXP_CST_NOTNULL;
				break;
			}
			if (!strcmp(long_options[option_index].name, "no-not-null")) {
				export_options &= ~MDB_SHEXP_CST_NOTNULL;
				break;
			}
			if (!strcmp(long_options[option_index].name, "not-empty")) {
				export_options |= MDB_SHEXP_CST_NOTEMPTY;
				break;
			}
			if (!strcmp(long_options[option_index].name, "no-not-empty")) {
				export_options &= ~MDB_SHEXP_CST_NOTEMPTY;
				break;
			}
			if (!strcmp(long_options[option_index].name, "description")) {
				export_options |= MDB_SHEXP_COMMENTS;
				break;
			}
			if (!strcmp(long_options[option_index].name, "no-description")) {
				export_options &= ~MDB_SHEXP_COMMENTS;
				break;
			}
			if (!strcmp(long_options[option_index].name, "indexes")) {
				export_options |= MDB_SHEXP_INDEXES;
				break;
			}
			if (!strcmp(long_options[option_index].name, "no-indexes")) {
				export_options &= ~MDB_SHEXP_INDEXES;
				break;
			}
			if (!strcmp(long_options[option_index].name, "relations")) {
				export_options |= MDB_SHEXP_RELATIONS;
				break;
			}
			if (!strcmp(long_options[option_index].name, "no-relations")) {
				export_options &= ~MDB_SHEXP_RELATIONS;
				break;
			}
			if (!strcmp(long_options[option_index].name, "no-sanitize")) {
				export_options &= ~MDB_SHEXP_SANITIZE;
				break;
			}
			fprintf(stderr, "unimplemented option %s", long_options[option_index].name);
			if (optarg)
				fprintf(stderr, " with arg %s", optarg);
			fputc('\n', stderr);
			exit(1);
			break;

		case 'T':
			tabname = (char *) g_strdup(optarg);
			break;

		case 'N':
			namespace = (char *) g_strdup(optarg);
			break;

		case 'S':
			export_options |= MDB_SHEXP_SANITIZE;
			break;
		}
	}
 
	mdb_init();

	/* open the database */
	mdb = mdb_open (argv[optind], MDB_NOFLAGS);
	if (!mdb) {
		fprintf(stderr, "Could not open file\n");
		mdb_exit();
		exit(1);
	}

	if (argc - optind >= 2) {
		if (!mdb_set_default_backend(mdb, argv[optind + 1])) {
			fprintf(stderr, "Invalid backend type\n");
			mdb_exit();
			exit(1);
		}
	}

	/* read the catalog */
 	if (!mdb_read_catalog (mdb, MDB_TABLE)) {
		fprintf(stderr, "File does not appear to be an Access database\n");
		exit(1);
	}

	mdb_print_schema(mdb, stdout, tabname, namespace, export_options);

	g_free(namespace);
	g_free(tabname);
	mdb_close (mdb);
	mdb_exit();

	return 0;
}

