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

static char *sanitize_name(char *str, int sanitize);
static void generate_table_schema(MdbCatalogEntry *entry, char *namespace, int sanitize);

int
main (int argc, char **argv)
{
	int   i;
	MdbHandle *mdb;
	MdbCatalogEntry *entry;
	char		*the_relation;
	char *tabname = NULL;
	char *namespace = "";
	int s = 0;
	int opt;

	if (argc < 2) {
		fprintf (stderr, "Usage: [options] %s <file> [<backend>]\n",argv[0]);
		fprintf (stderr, "where options are:\n");
		fprintf (stderr, "  -T <table>     Only create schema for named table\n");
		fprintf (stderr, "  -N <namespace> Prefix identifiers with namespace\n");
		fprintf (stderr, "  -S             Sanitize names (replace spaces etc. with underscore)\n");
		exit (1);
	}

	while ((opt=getopt(argc, argv, "T:N:S:"))!=-1) {
		switch (opt) {
			case 'T':
				tabname = (char *) malloc(strlen(optarg)+1);
				strcpy(tabname, optarg);
			break;
			case 'N':
				namespace = (char *) malloc(strlen(optarg)+1);
				strcpy(namespace, optarg);
			break;
			case 'S':
				s = 1;
			break;
     }
  }
 
 mdb_init();

 /* open the database */

 mdb = mdb_open (argv[optind], MDB_NOFLAGS);
 if (argc - optind >= 2) {
	if (!mdb_set_default_backend(mdb, argv[optind + 1])) {
		fprintf(stderr,"Invalid backend type\n");
		mdb_exit();
		exit(1);
	}
 }

	/* read the catalog */
 	if (!mdb_read_catalog (mdb, MDB_TABLE)) {
		fprintf(stderr,"File does not appear to be an Access database\n");
		exit(1);
	}

	/* Print out a little message to show that this came from mdb-tools.
	   I like to know how something is generated. DW */
	fprintf(stdout,"-------------------------------------------------------------\n");
	fprintf(stdout,"-- MDB Tools - A library for reading MS Access database files\n");
	fprintf(stdout,"-- Copyright (C) 2000-2004 Brian Bruns\n");
	fprintf(stdout,"-- Files in libmdb are licensed under LGPL and the utilities under\n");
	fprintf(stdout,"-- the GPL, see COPYING.LIB and COPYING files respectively.\n");
	fprintf(stdout,"-- Check out http://mdbtools.sourceforge.net\n");
	fprintf(stdout,"-------------------------------------------------------------\n\n");

	/* loop over each entry in the catalog */

	for (i=0; i < mdb->num_catalog; i++) {
		entry = g_ptr_array_index (mdb->catalog, i);

		/* if it's a table */

		if (entry->object_type == MDB_TABLE) {
			/* skip the MSys tables */
			if ((tabname && !strcmp(entry->object_name,tabname)) ||
				(!tabname && strncmp (entry->object_name, "MSys", 4))) {
	   
				   generate_table_schema(entry, namespace, s);
			}
		}
	}

	fprintf (stdout, "\n\n");
	fprintf (stdout, "-- CREATE ANY Relationships ...\n");
	fprintf (stdout, "\n");
	the_relation=mdb_get_relationships(mdb);
	while (the_relation[0] != '\0') {
		fprintf(stdout,"%s\n",the_relation);
		the_relation=mdb_get_relationships(mdb);
	}            
 

	mdb_free_handle (mdb);
	mdb_exit();

	exit(0);
}
static void
generate_table_schema(MdbCatalogEntry *entry, char *namespace, int sanitize)
{
	MdbTableDef *table;
	MdbHandle *mdb = entry->mdb;
	int i;
	MdbColumn *col;

	/* make sure it's a table (may be redundant) */

	if (strcmp (mdb_get_objtype_string (entry->object_type), "Table"))
		   return;

	/* drop the table if it exists */
	fprintf (stdout, "DROP TABLE %s%s;\n", namespace, sanitize_name(entry->object_name,sanitize));

	/* create the table */
	fprintf (stdout, "CREATE TABLE %s%s\n", namespace, sanitize_name(entry->object_name,sanitize));
	fprintf (stdout, " (\n");
	       	       
	table = mdb_read_table (entry);

	/* get the columns */
	mdb_read_columns (table);

	/* loop over the columns, dumping the names and types */

	for (i = 0; i < table->num_cols; i++) {
		col = g_ptr_array_index (table->columns, i);
		   
		fprintf (stdout, "\t%s\t\t\t%s", sanitize_name(col->name,sanitize), 
		mdb_get_coltype_string (mdb->default_backend, col->col_type));
		   
		if (mdb_coltype_takes_length(mdb->default_backend, 
			col->col_type)) {

			/* more portable version from DW patch */	
			if (col->col_size == 0) 
	    			fprintf (stdout, " (255)");
			else 
	    			fprintf (stdout, " (%d)", col->col_size);
		}
		   
		if (i < table->num_cols - 1)
			fprintf (stdout, ", \n");
		else
			fprintf (stdout, "\n");
	} /* for */

	fprintf (stdout, ");\n");
	fprintf (stdout, "-- CREATE ANY INDEXES ...\n");
	fprintf (stdout, "\n");

}

static char *sanitize_name(char *str, int sanitize)
{
	static char namebuf[256];
	char *p = namebuf;

	if (!sanitize)
		return str;
		
	while (*str) {
		*p = isalnum(*str) ? *str : '_';
		p++;
		str++;
	}
	
	*p = 0;
										
	return namebuf;
}

