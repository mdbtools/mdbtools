/* MDB Tools - A library for reading MS Access database file
 * Copyright (C) 2000-2004 Brian Bruns
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
#include "mdbver.h"
#include "mdbprivate.h"
#include <locale.h>

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

	mdb_init();

	if (!(mdb = mdb_open(argv[optind]))) {
		fprintf(stderr,_("Error: unable to open file %s\n"),argv[optind]);
		exit(1);
	}
	if (IS_JET3(mdb)) {
		printf("JET3\n");
	} else if (IS_JET4(mdb)) {
		printf("JET4\n");
	} else {
		printf(_("unknown database version\n"));
	}
	
	mdb_free_handle(mdb);
	mdb_exit();

	exit(0);
}

