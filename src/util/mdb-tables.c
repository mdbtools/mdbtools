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
valid_types(void)
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
	int objtype = MDB_TABLE;
	char *str_objtype = NULL;

	GOptionEntry entries[] = {
		{ "system", 'S', G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, &skip_sys, "Include system tables", NULL},
		{ "single-column", '1', 0, G_OPTION_ARG_NONE, &line_break, "One table name per line", NULL},
		{ "delimiter", 'd', 0, G_OPTION_ARG_STRING, &delimiter, "Table name delimiter", "char"},
		{ "type", 't', 0, G_OPTION_ARG_STRING, &str_objtype, "Type of entry", "type"},
		{ "showtype", 'T', 0, G_OPTION_ARG_NONE, &show_type, "Show type", NULL},
		{ NULL },
	};
	GError *error = NULL;
	GOptionContext *opt_context;

	opt_context = g_option_context_new("<file> - show MDB files tables/entries");
	g_option_context_add_main_entries(opt_context, entries, NULL /*i18n*/);
	// g_option_context_set_strict_posix(opt_context, TRUE); /* options first, requires glib 2.44 */
	if (!g_option_context_parse (opt_context, &argc, &argv, &error))
	{
		fprintf(stderr, "option parsing failed: %s\n", error->message);
		fputs(g_option_context_get_help(opt_context, TRUE, NULL), stderr);
		fprintf(stderr, "Valid types are: %s\n",valid_types());
		exit (1);
	}

	if (argc != 2) {
		fputs("Wrong number of arguments.\n\n", stderr);
		fputs(g_option_context_get_help(opt_context, TRUE, NULL), stderr);
		fprintf(stderr, "Valid types are: %s\n",valid_types());
		exit(1);
	}

	if (str_objtype) {
		if (!get_obj_type(str_objtype, &objtype)) {
			fprintf(stderr,"Invalid type name.\n");
			fprintf (stderr, "Valid types are: %s\n",valid_types());
			exit(1);
		}
	}
	if (!delimiter)
		delimiter = g_strdup(" ");

 	/* open the database */
	if (!(mdb = mdb_open (argv[1], MDB_NOFLAGS))) {
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
	g_option_context_free(opt_context);
	g_free(delimiter);
	g_free(str_objtype);

	return 0;
}

