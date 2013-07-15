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

int
dbcc_page_usage(MdbTableDef *table)
{
}
int
dbcc_idx_page_usage(MdbTableDef *table)
{
}
int
dbcc_lost_pages(MdbTableDef *table)
{
}

main (int argc, char **argv)
{
int   i, j, k;
MdbHandle *mdb;
MdbCatalogEntry *entry;
MdbTableDef *table;
char *tabname = NULL;
int opt;

 if (argc < 2) {
   fprintf (stderr, "Usage: %s <file> [-T <table>]\n",argv[0]);
   exit (1);
 }

  while ((opt=getopt(argc, argv, "T:"))!=-1) {
     switch (opt) {
       case 'T':
         tabname = (char *) g_strdup(optarg);
         break;
     }
  }
 
 /* open the database */

 mdb = mdb_open (argv[optind], MDB_NOFLAGS);

 /* read the catalog */
 
 mdb_read_catalog (mdb, MDB_TABLE);

 /* loop over each entry in the catalog */

 for (i=0; i < mdb->num_catalog; i++) 
   {
     entry = g_ptr_array_index (mdb->catalog, i);

     /* if it's a table */

	if (entry->object_type == MDB_TABLE) {
		if ((tabname && !strcmp(entry->object_name,tabname)) ||
			(!tabname /* && mdb_is_user_table(entry) */)) {
			int ret;

	       		table = mdb_read_table(entry);

	       		/* get the columns */
			mdb_read_columns(table);
			fprintf(stdout,"Check 1: Checking data page usage map\n");
			ret = dbcc_page_usage(table);
			fprintf(stdout,"Check 1: %s\n", ret ? "Failed" : "Passed");
			fprintf(stdout,"Check 2: Checking index page usage map\n");
			fprintf(stdout,"Check 3: Checking for lost pages\n");
			ret = dbcc_lost_pages(table);
			//check_ret(table, ret);

			mdb_free_tabledef(table);
		}
	}
   }

	g_free(tabname);
	mdb_close (mdb);

	return 0;
}

