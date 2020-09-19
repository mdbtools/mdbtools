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

/* this utility dumps the C headers for an existing database */
/* it will create three files - types.h and dump_types.[ch] */

#include <string.h>
#include "mdbtools.h"

void copy_header (FILE *f)
{
 fprintf (f, "/******************************************************************/\n");
 fprintf (f, "/* THIS IS AN AUTOMATICALLY GENERATED FILE.  DO NOT EDIT IT!!!!!! */\n");
 fprintf (f, "/******************************************************************/\n");
}

int
main (int argc, char **argv)
{
unsigned int   i, j, k;
unsigned int unsupported = 0;
MdbHandle *mdb;
MdbCatalogEntry *entry;
MdbTableDef *table;
MdbColumn *col;
FILE *typesfile;
FILE *headerfile;
FILE *cfile;

 if (argc < 2) {
   fprintf (stderr, "Usage: %s <file>\n",argv[0]);
   exit (1);
 }

 /* open the database */

 mdb = mdb_open (argv[1], MDB_NOFLAGS);
 if (!mdb) {
	exit(1);
 }

 typesfile = fopen ("types.h", "w");
 headerfile = fopen ("dumptypes.h", "w");
 cfile = fopen ("dumptypes.c", "w");

 copy_header (typesfile);
 copy_header (headerfile);
 fprintf (headerfile, "#include \"types.h\"\n");
 copy_header (cfile);
 fprintf (cfile, "#include <stdio.h>\n");
 fprintf (cfile, "#include \"dumptypes.h\"\n");
 
 /* read the catalog */
 
 mdb_read_catalog (mdb, MDB_TABLE);

 /* loop over each entry in the catalog */

 for (i=0; i < mdb->num_catalog; i++) 
   {
     entry = g_ptr_array_index (mdb->catalog, i);

     if (!mdb_is_user_table(entry))
          continue;

	       fprintf (typesfile, "typedef struct _%s\n", entry->object_name);
	       fprintf (typesfile, "{\n");

	       fprintf (headerfile, "void dump_%s (%s x);\n",
			entry->object_name, entry->object_name);
	       fprintf (cfile, "void dump_%s (%s x)\n{\n",
			entry->object_name, entry->object_name);
	       fprintf (cfile, "\tfprintf (stdout, \"**************** %s ****************\\n\");\n", entry->object_name);
	       table = mdb_read_table (entry);

	       /* get the columns */
	       mdb_read_columns (table);

	       /* loop over the columns, dumping the names and types */

	       for (k = 0; k < table->num_cols; k++)
		 {
		   col = g_ptr_array_index (table->columns, k);
		   fprintf (cfile, "\tfprintf (stdout, \"x.");
		   for (j = 0; j < strlen (col->name); j++)
		     {
		       fprintf (cfile, "%c", tolower (col->name [j]));
		     }
		   fprintf (cfile, " = \");\n");
		   switch (col->col_type)
		     {
		     case MDB_INT:
		       fprintf (typesfile, "\tint\t");
		       fprintf (cfile, "\tdump_int (x.");
		       break;
		     case MDB_LONGINT:
		       fprintf (typesfile, "\tlong\t");
		       fprintf (cfile, "\tdump_long (x.");
		       break;
		     case MDB_TEXT:
		     case MDB_MEMO:
		       fprintf (typesfile, "\tchar *\t");
		       fprintf (cfile, "\tdump_string (x.");
		       break;
		     default:
               fprintf(stderr, "ERROR: unsupported type: 0x%02x\n", col->col_type);
		       unsupported = 1;
		       break;
		     }
		   for (j = 0; j < strlen (col->name); j++)
		     {
		       fprintf (typesfile, "%c", tolower (col->name [j]));
		       fprintf (cfile, "%c", tolower (col->name [j]));
		     }
		   fprintf (typesfile, ";\n");
		   fprintf (cfile, ");\n");
		 }

	       fprintf (typesfile, "\n} %s ;\n", entry->object_name);
	       fprintf (typesfile, "\n");
	       fprintf (cfile, "}\n\n");

	       mdb_free_tabledef(table);
   }

 fclose (headerfile);
 fclose (typesfile);
 fclose (cfile);
 
 mdb_close (mdb);

 exit(unsupported);
}

