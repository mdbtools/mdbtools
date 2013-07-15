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

#include <locale.h>
#include "mdbtools.h"
#include "mdbver.h"
#include "mdbprivate.h"

#ifdef DMALLOC
#include "dmalloc.h"
#endif

int
main(int argc, char **argv)
{
	MdbHandle *mdb;
	int print_mdbver = 0;
	int opt;

  	/* setlocale (LC_ALL, ""); */
    	bindtextdomain (PACKAGE, LOCALEDIR);
      	textdomain (PACKAGE);
	while ((opt=getopt(argc, argv, "M"))!=-1) {
		switch (opt) {
			case 'M':
				print_mdbver = 1;
				break;
			default:
				break;
		}
	}

	if (print_mdbver) {
		fprintf(stdout,"%s\n", MDB_FULL_VERSION);
		if (argc-optind < 1) exit(0);
	}

	/* 
	** optind is now the position of the first non-option arg, 
	** see getopt(3) 
	*/
	if (argc-optind < 1) {
		fprintf(stderr,_("Usage: %s [-M] <file>\n"),argv[0]);
		exit(1);
	}

	if (!(mdb = mdb_open(argv[optind], MDB_NOFLAGS))) {
		fprintf(stderr,_("Error: unable to open file %s\n"),argv[optind]);
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
	default:
		printf(_("unknown database version\n"));
		break;
	}
	
	mdb_close(mdb);

	return 0;
}

