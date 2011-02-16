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

	while ((opt=getopt(argc, argv, "T:N:S"))!=-1) {
		switch (opt) {
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

