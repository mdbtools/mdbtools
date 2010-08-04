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

static void generate_table_schema(MdbCatalogEntry *entry, char *namespace, int sanitize);

int
main (int argc, char **argv)
{
	unsigned int   i;
	MdbHandle *mdb;
	MdbCatalogEntry *entry;
	char		*the_relation;
	char *tabname = NULL;
	char *namespace = NULL;
	int s = 0;
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
				s = 1;
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

	for (i=0; i < mdb->num_catalog; i++) {
		entry = g_ptr_array_index (mdb->catalog, i);
		if (entry->object_type == MDB_TABLE) {
			if ((tabname && !strcmp(entry->object_name, tabname)) 
			 || (!tabname && mdb_is_user_table(entry))) {
				generate_table_schema(entry, namespace, s);
			}
		}
	}

	fprintf (stdout, "\n\n");
	fprintf (stdout, "-- CREATE ANY Relationships ...\n");
	fprintf (stdout, "\n");
	while ((the_relation=mdb_get_relationships(mdb)) != NULL) {
		fprintf(stdout,"%s\n",the_relation);
		g_free(the_relation);
	}            
 
	g_free(namespace);
	g_free(tabname);
	mdb_close (mdb);
	mdb_exit();

	exit(0);
}
static void
generate_table_schema(MdbCatalogEntry *entry, char *namespace, int sanitize)
{
	MdbTableDef *table;
	MdbHandle *mdb = entry->mdb;
	unsigned int i;
	MdbColumn *col;
	char* table_name;
	char* quoted_table_name;
	char* quoted_name;
	char* sql_sequences;

	if (sanitize)
		quoted_table_name = sanitize_name(entry->object_name);
	else
		quoted_table_name = mdb->default_backend->quote_name(entry->object_name);

	if (namespace) {
		table_name = malloc(strlen(namespace)+strlen(quoted_table_name)+1);
		strcpy(table_name, namespace);
		strcat(table_name, quoted_table_name);
		free(quoted_table_name);
		quoted_table_name = table_name;
	}

	/* drop the table if it exists */
	fprintf (stdout, "DROP TABLE %s;\n", quoted_table_name);

	/* create the table */
	fprintf (stdout, "CREATE TABLE %s\n", quoted_table_name);
	fprintf (stdout, " (\n");

	table = mdb_read_table (entry);

	/* get the columns */
	mdb_read_columns (table);

	/* loop over the columns, dumping the names and types */

	for (i = 0; i < table->num_cols; i++) {
		col = g_ptr_array_index (table->columns, i);

		if (sanitize)
			quoted_name = sanitize_name(col->name);
		else
			quoted_name = mdb->default_backend->quote_name(col->name);
		fprintf (stdout, "\t%s\t\t\t%s", quoted_name,
			mdb_get_coltype_string (mdb->default_backend, col->col_type));
		free(quoted_name);
		   
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

	fprintf (stdout, "-- CREATE SEQUENCES ...\n");
	fprintf (stdout, "\n");

	while ((sql_sequences = mdb_get_sequences(entry, namespace, sanitize)))
		fprintf(stdout, sql_sequences);

	/*
	for (i = 0; i < table->num_cols; i++) {
		col = g_ptr_array_index (table->columns, i);
		if (col->is_long_auto) {
			char sequence_name[256+1+256+4]; // FIXME
			char *quoted_column_name;
			quoted_column_name = mdb->default_backend->quote_name(col->name);
			sprintf(sequence_name, "%s_%s_seq", entry->object_name, col->name);
			quoted_name = mdb->default_backend->quote_name(sequence_name);
			fprintf (stdout, "CREATE SEQUENCE %s;\n", quoted_name);
			fprintf (stdout, "ALTER TABLE %s ALTER COLUMN %s SET DEFAULT pg_catalog.nextval('%s');\n",
				quoted_table_name, quoted_column_name, quoted_name);
			free(quoted_column_name);
			free(quoted_name);
		}

	}
	*/

	fprintf (stdout, "-- CREATE ANY INDEXES ...\n");
	fprintf (stdout, "\n");

	free(quoted_table_name);

	mdb_free_tabledef (table);
}
