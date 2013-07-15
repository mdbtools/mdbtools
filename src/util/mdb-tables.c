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

#ifdef DMALLOC
#include "dmalloc.h"
#endif

struct type_struct {
	char *name;
	int value;
} types[] = {
	{ "form", MDB_FORM },
   	{ "table", MDB_TABLE },
   	{ "macro", MDB_MACRO },
   	{ "systable", MDB_SYSTEM_TABLE },
   	{ "report", MDB_REPORT },
   	{ "query", MDB_QUERY },
   	{ "linkedtable", MDB_LINKED_TABLE },
   	{ "module", MDB_MODULE },
   	{ "relationship", MDB_RELATIONSHIP },
   	{ "dbprop", MDB_DATABASE_PROPERTY },
   	{ "any", MDB_ANY },
   	{ "all", MDB_ANY },
   	{ NULL, 0 }
};

char *
valid_types()
{
	static char ret[256]; /* be sure to allow for enough space if adding more */
	int i = 0;	
	
	ret[0] = '\0';
	while (types[i].name) {
		strcat(ret, types[i].name);
		strcat(ret, " ");
		i++;
	}
	return ret;
}
int 
get_obj_type(char *typename, int *ret)
{
	int i=0;
	int found=0;

	char *s = typename;

	while (*s) { *s=tolower(*s); s++; }

	while (types[i].name) {
		if (!strcmp(types[i].name, typename)) {
			found=1;
			*ret = types[i].value;
		}
		i++;
	}
	return found;
}
int
main (int argc, char **argv)
{
	unsigned int   i;
	MdbHandle *mdb;
	MdbCatalogEntry *entry;
	char *delimiter = NULL;
	int line_break=0;
	int skip_sys=1;
	int show_type=0;
	int opt;
	int objtype = MDB_TABLE;


	if (argc < 2) {
		fprintf (stderr, "Usage: %s [-S] [-1 | -d<delimiter>] [-t <type>] [-T] <file>\n",argv[0]);
		fprintf (stderr, "       Valid types are: %s\n",valid_types());

		exit (1);
	}

	while ((opt=getopt(argc, argv, "S1Td:t:"))!=-1) {
        switch (opt) {
        case 'S':
            skip_sys = 0;
		break;
        case 'T':
            show_type = 1;
		break;
        case '1':
            line_break = 1;
        break;
        case 't':
            if (!get_obj_type(optarg, &objtype)) {
				fprintf(stderr,"Invalid type name.\n");
				fprintf (stderr, "Valid types are: %s\n",valid_types());
				exit(1);
			}
        break;
        case 'd':
            delimiter = (char *) g_strdup(optarg);
        break;
		}
	}

 
 	/* open the database */
 	if (!(mdb = mdb_open (argv[optind], MDB_NOFLAGS))) {
		fprintf(stderr,"Couldn't open database.\n");
		exit(1);
	}
	

 	/* read the catalog */
 	if (!mdb_read_catalog (mdb, MDB_ANY)) {
		fprintf(stderr,"File does not appear to be an Access database\n");
		exit(1);
	}

 	/* loop over each entry in the catalog */
 	for (i=0; i < mdb->num_catalog; i++) {
		entry = g_ptr_array_index (mdb->catalog, i);

		if (entry->object_type != objtype && objtype!=MDB_ANY)
			continue;
		if (skip_sys && mdb_is_system_table(entry))
			continue;

		if (show_type) {
			if (delimiter)
				puts(delimiter);
			printf("%d ", entry->object_type);
		}
		if (line_break) 
			printf ("%s\n", entry->object_name);
		else if (delimiter) 
			printf ("%s%s", entry->object_name, delimiter);
		else 
			printf ("%s ", entry->object_name);
	}
	if (!line_break) 
		fprintf (stdout, "\n");
 
	mdb_close(mdb);
	g_free(delimiter);

	return 0;
}

