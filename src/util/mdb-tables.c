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

#include "mdbtools.h"

main (int argc, char **argv)
{
int   i, j, k;
MdbHandle *mdb;
MdbCatalogEntry *entry;
MdbTableDef *table;
MdbColumn *col;
char *delimiter = NULL;
int line_break=0;
int skip_sys=1;
int opt;

	if (argc < 2) {
		fprintf (stderr, "Usage: %s [-S] [-1 | -d<delimiter>] <file>\n",argv[0]);
		exit (1);
	}

	while ((opt=getopt(argc, argv, "S1d:"))!=-1) {
        switch (opt) {
        case 'S':
            skip_sys = 0;
        case '1':
            line_break = 1;
        break;
        case 'd':
            delimiter = (char *) malloc(strlen(optarg)+1);
            strcpy(delimiter, optarg);
        break;
		}
	}

 
 	/* initialize the library */
 	mdb_init();

 	/* open the database */
 	if (!(mdb = mdb_open (argv[optind]))) {
		fprintf(stderr,"Couldn't open database.\n");
		exit(1);
	}
	

 	/* read the catalog */
 	mdb_read_catalog (mdb, MDB_TABLE);

 	/* loop over each entry in the catalog */
 	for (i=0; i < mdb->num_catalog; i++) {
		entry = g_ptr_array_index (mdb->catalog, i);

     	/* if it's a table */
     	if (entry->object_type == MDB_TABLE) {
	 		/* skip the MSys tables */
			if (!skip_sys || strncmp (entry->object_name, "MSys", 4)) {
	       		if (line_break) 
					fprintf (stdout, "%s\n", entry->object_name);
				else if (delimiter) 
					fprintf (stdout, "%s%s", entry->object_name, delimiter);
				else 
					fprintf (stdout, "%s ", entry->object_name);
	     	}
		}
	}
	if (!line_break) 
		fprintf (stdout, "\n");
 
	mdb_free_handle (mdb);
	mdb_exit();
	if (delimiter) free(delimiter);

	exit(0);
}

