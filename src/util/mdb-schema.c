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
char		*the_relation;
char *tabname = NULL;
int opt;

 if (argc < 2) {
   fprintf (stderr, "Usage: %s <file> [<backend>]\n",argv[0]);
   exit (1);
 }

  while ((opt=getopt(argc, argv, "T:"))!=-1) {
     switch (opt) {
       case 'T':
         tabname = (char *) malloc(strlen(optarg)+1);
         strcpy(tabname, optarg);
         break;
     }
  }
 
 mdb_init();

 /* open the database */

 mdb = mdb_open (argv[optind]);
 if (argc - optind >2) {
	if (!mdb_set_default_backend(mdb, argv[optind + 1])) {
		fprintf(stderr,"Invalid backend type\n");
		mdb_exit();
		exit(1);
	}
 }

 /* read the catalog */
 
 mdb_read_catalog (mdb, MDB_TABLE);

 /* loop over each entry in the catalog */

 for (i=0; i < mdb->num_catalog; i++) 
   {
     entry = g_ptr_array_index (mdb->catalog, i);

     /* if it's a table */

     if (entry->object_type == MDB_TABLE)
       {
	 /* skip the MSys tables */
       if ((tabname && !strcmp(entry->object_name,tabname)) ||
           (!tabname && strncmp (entry->object_name, "MSys", 4)))
	 {
	   
	   /* make sure it's a table (may be redundant) */

	   if (!strcmp (mdb_get_objtype_string (entry->object_type), "Table"))
	     {
	       /* drop the table if it exists */
	       fprintf (stdout, "DROP TABLE %s;\n", entry->object_name);

	       /* create the table */
	       fprintf (stdout, "CREATE TABLE %s\n", entry->object_name);
	       fprintf (stdout, " (\n");
	       	       
	       table = mdb_read_table (entry);

	       /* get the columns */
	       mdb_read_columns (table);

	       /* loop over the columns, dumping the names and types */

	       for (k = 0; k < table->num_cols; k++)
		 {
		   col = g_ptr_array_index (table->columns, k);
		   
		   fprintf (stdout, "\t%s\t\t\t%s", col->name, 
			    mdb_get_coltype_string (mdb->default_backend, col->col_type));
		   
		   if (col->col_size != 0)
		     fprintf (stdout, " (%d)", col->col_size);
		   
		   if (k < table->num_cols - 1)
		     fprintf (stdout, ", \n");
		   else
		     fprintf (stdout, "\n");
		 }

	       fprintf (stdout, "\n);\n");
	       fprintf (stdout, "-- CREATE ANY INDEXES ...\n");
	       fprintf (stdout, "\n");
	     }
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

