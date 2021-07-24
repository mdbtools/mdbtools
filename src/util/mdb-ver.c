/* MDB Tools - A library for reading MS Access database file
 * Copyright (C) 2000-2004 Brian Bruns
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

int
main(int argc, char **argv)
{
	MdbHandle *mdb;
	int print_mdbver = 0;

	GOptionEntry entries[] = {
		{ "version", 'M', 0, G_OPTION_ARG_NONE, &print_mdbver, "Show mdbtools version and exit", NULL},
		{ NULL },
	};
	GError *error = NULL;
	GOptionContext *opt_context;

	opt_context = g_option_context_new("<file> - display MDB file version");
	g_option_context_add_main_entries(opt_context, entries, NULL /*i18n*/);
	// g_option_context_set_strict_posix(opt_context, TRUE); /* options first, requires glib 2.44 */
	if (!g_option_context_parse (opt_context, &argc, &argv, &error))
	{
		fprintf(stderr, "option parsing failed: %s\n", error->message);
		fputs(g_option_context_get_help(opt_context, TRUE, NULL), stderr);
		exit (1);
	}

	if (print_mdbver) {
		if (argc > 1) {
			fputs(g_option_context_get_help(opt_context, TRUE, NULL), stderr);
		}
		fprintf(stdout,"%s\n", MDB_FULL_VERSION);
		exit(argc > 1);
	}

	if (argc != 2) {
		fputs("Wrong number of arguments.\n\n", stderr);
		fputs(g_option_context_get_help(opt_context, TRUE, NULL), stderr);
		exit(1);
	}

	if (!(mdb = mdb_open(argv[1], MDB_NOFLAGS))) {
		fprintf(stderr,"Error: unable to open file %s\n", argv[1]);
		exit(1);
	}
	switch(mdb->f->jet_version) {
	case MDB_VER_JET3:
		printf("JET3\n");
		break;
	case MDB_VER_JET4:
		printf("JET4\n");
		break;
	case MDB_VER_ACCDB_2007:
		printf("ACE12\n");
		break;
	case MDB_VER_ACCDB_2010:
		printf("ACE14\n");
		break;
	case MDB_VER_ACCDB_2013:
		printf("ACE15\n");
		break;
	case MDB_VER_ACCDB_2016:
		printf("ACE16\n");
		break;
	case MDB_VER_ACCDB_2019:
		printf("ACE17\n");
		break;
	default:
		printf("unknown database version\n");
		break;
	}
	
	mdb_close(mdb);
	g_option_context_free(opt_context);

	return 0;
}

