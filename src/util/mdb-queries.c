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

/**************************************************************
	This utility allows you to list the queries in the database
	and dump the SQL associated with a specific query.  
	
	The	idea behind this utility is to eventually allow the 
	results to be piped to mdb-sql allowing a (fairly) simple 
	method to execute queries that are stored in the access
	database.
	
	created by Leonard Leblanc <lleblanc@macroelite.ca>
	(with much help from the other mdb utilities)

	Modified by Evan Miller and added to repository 2020
**************************************************************/
	
#include "mdbtools.h"
#include "mdbver.h"

#define QUERY_BIND_SIZE 200000

void mdb_list_queries(MdbHandle *mdb, int line_break, char *delimiter);
char * mdb_get_query_id(MdbHandle *mdb,char *query);

int main (int argc, char **argv) {
	unsigned int i;
	MdbHandle *mdb = NULL;
	MdbCatalogEntry *entry = NULL, *sys_queries = NULL, *temp = NULL;
	MdbTableDef *table = NULL;
	char *delimiter = NULL;
	int list_only=0;
	int found_match=0;
	int line_break=0;
	int rc=0;
	char *query_id;
	char *query_name = NULL;
    size_t bind_size = QUERY_BIND_SIZE;
	
	// variables for the msysqueries table
	char *attribute = malloc(bind_size);
	char *expression = malloc(bind_size);
	char *flag = malloc(bind_size);
	char *name1 = malloc(bind_size);
	char *name2 = malloc(bind_size);
	char *objectid = malloc(bind_size);
	char *order = malloc(bind_size);
	
	//variables for the generation of sql
	char *sql_tables = malloc(bind_size);
	char *sql_predicate = malloc(bind_size);
	char *sql_columns = malloc(bind_size);
	char *sql_where = malloc(bind_size);
	char *sql_grouping = malloc(bind_size);
	char *sql_having = malloc(bind_size);
	char *sql_sorting = malloc(bind_size);
	char *sql_insert = malloc(bind_size);
	char *sql_into = malloc(bind_size);
	char *sql_target = malloc(bind_size);
	char *sql_target_columns = malloc(bind_size);
	int flagint;
	int query_type = 1;	// 1 select, 2 make-table, 3 append, 4 update, 5 delete, 6 crosstab, 7 DDL, 8 pass-through, 9 union
	char *locale = NULL;
	int print_mdbver = 0;
	
	GError *error = NULL;
	GOptionContext *opt_context;

	GOptionEntry entries[] = {
		{"list", 'L', 0, G_OPTION_ARG_NONE, &list_only, "List queries in the database (default if no query name is passed)", NULL},
		{"newline", '1', 0, G_OPTION_ARG_NONE, &line_break, "Use newline as the delimiter (used in conjunction with listing)", NULL},
		{"delimiter", 'd', 0, G_OPTION_ARG_STRING, &delimiter, "Specify delimiter to use", "delim"},
		{"version", 0, 0, G_OPTION_ARG_NONE, &print_mdbver, "Show mdbtools version and exit", NULL},
		{NULL}
	};
	opt_context = g_option_context_new("<file> <query name> - list or export queries from an Access database");
	g_option_context_add_main_entries(opt_context, entries, NULL /*i18n*/);
	locale = setlocale(LC_CTYPE, "");
	if (!g_option_context_parse (opt_context, &argc, &argv, &error))
	{
		fprintf(stderr, "option parsing failed: %s\n", error->message);
		fputs(g_option_context_get_help(opt_context, TRUE, NULL), stderr);
		exit (1);
	}
	if (print_mdbver) {
		if (argc > 1) {
			fputs(g_option_context_get_help(opt_context, TRUE, NULL), stderr);
		}
		fprintf(stdout,"%s\n", MDB_FULL_VERSION);
		exit(argc > 1);
	}
	/* let's turn list_only on if only a database filename was passed */
	if(argc == 2) {
		list_only=1;
	} else if (argc == 3) {
		query_name= g_locale_to_utf8(argv[2], -1, NULL, NULL, &error);
		if (!query_name) {
			fprintf(stderr, "argument parsing failed: %s\n", error->message);
			exit(1);
		}
	} else {
		fputs("Wrong number of arguments.\n\n", stderr);
		fputs(g_option_context_get_help(opt_context, TRUE, NULL), stderr);
		exit(1);
	}
	setlocale(LC_CTYPE, locale);
	
 	/* open the database */
	if (!(mdb = mdb_open(argv[1],MDB_NOFLAGS))) {
		fprintf(stderr,"Couldn't open database.\n");
		exit(1);
	}

    mdb_set_bind_size(mdb, bind_size);

 	/* read the catalog */
 	if (!mdb_read_catalog (mdb, MDB_ANY)) {
		fprintf(stderr,"File does not appear to be an Access database\n");
		exit(1);
	}
	
	if(list_only) { 
		mdb_list_queries(mdb,line_break,delimiter);
	} else {
		/* let's get the entry for the user specified query
			 we also want to get the catalog for the MSysQueries table
			 while we are here */
		for (i=0; i < mdb->num_catalog; i++) {
			temp = g_ptr_array_index(mdb->catalog, i);
			
			if(strcmp(temp->object_name,query_name) == 0) {
				entry = g_ptr_array_index(mdb->catalog,i);
				found_match=1;
			} else if(strcmp(temp->object_name,"MSysQueries") == 0) {
				sys_queries = g_ptr_array_index(mdb->catalog,i);
			} 
		}
		
		if(found_match) {
			/* Let's get the id for the query */
			query_id = mdb_get_query_id(mdb,entry->object_name);
			
			table = mdb_read_table(sys_queries);

			if(table) {
				mdb_read_columns(table);

				mdb_bind_column_by_name(table, "Attribute", attribute, NULL);
				mdb_bind_column_by_name(table, "Expression", expression, NULL);
				mdb_bind_column_by_name(table, "Flag", flag, NULL);
				mdb_bind_column_by_name(table, "Name1", name1, NULL);
				mdb_bind_column_by_name(table, "Name2", name2, NULL);
				mdb_bind_column_by_name(table, "ObjectId", objectid, NULL);
				mdb_bind_column_by_name(table, "Order", order, NULL);

				mdb_rewind_table(table);

				sql_tables[0] = sql_predicate[0] = sql_columns[0] = sql_where[0] = '\0';
				sql_grouping[0] = sql_having[0] = sql_sorting[0] = '\0';
				sql_insert[0] = sql_into[0] = sql_target[0] = sql_target_columns[0] = '\0';

				while (mdb_fetch_row(table)) {
					if(strcmp(query_id,objectid) == 0) {
						flagint = atoi(flag);
						//we have a row for our query
						switch(atoi(attribute)) {
							case 1:		// query type; Name1 = destination table for make-table/append
								query_type = flagint;
								strcpy(sql_target,name1);
								break;
							case 3:		// predicate
								if (flagint & 0x30) {
									strcpy(sql_predicate, " TOP ");
									strcat(sql_predicate, name1);
									if (flagint & 0x20) {
										strcat(sql_predicate, " PERCENT");
									}
								} else if (flagint & 0x8) {
									strcpy(sql_predicate, " DISTINCTROW");
								} else if (flagint & 0x2) {
									strcpy(sql_predicate, " DISTINCT");
								}
								break;
							case 5:		// table name
								if(strcmp(sql_tables,"") != 0) {
									strcat(sql_tables,",");
								}
								sprintf(sql_tables+strlen(sql_tables),"[%s]",name1);
								break;
							case 6:		// column name
								if(strcmp(sql_columns,"") != 0) {
									strcat(sql_columns,",");
								}
								strcat(sql_columns,expression);
								if(strcmp(name1,"") != 0 && strcmp(name1,expression) != 0) {
									// Name1 = column alias
									sprintf(sql_columns+strlen(sql_columns)," AS [%s]",name1);
								}
								if(strcmp(name2,"") != 0) {
									// Name2 = destination column (append query)
									if(strcmp(sql_target_columns,"") != 0) {
										strcat(sql_target_columns,",");
									}
									sprintf(sql_target_columns+strlen(sql_target_columns),"[%s]",name2);
								}
								break;
							case 7:		// join/relationship where clause
								//fprintf(stdout,"join tables: %s - %s\n",name1,name2);
								//fprintf(stdout,"join clause: %s\n",expression);
								break;
							case 8:		// where clause
								strcpy(sql_where," WHERE ");
								strcat(sql_where,expression);
								break;
							case 9:		// group by
								if(strcmp(sql_grouping,"") == 0) {
									strcpy(sql_grouping," GROUP BY ");
								} else {
									strcat(sql_grouping,",");
								}
								strcat(sql_grouping,expression);
								break;
							case 10:		// having clause
								strcpy(sql_having," HAVING ");
								strcat(sql_having,expression);
								break;
							case 11:		// sorting
								if(strcmp(sql_sorting,"") == 0) {
									strcpy(sql_sorting,"ORDER BY ");
									strcat(sql_sorting,expression);
									if(strcmp(name1,"D") == 0) {
										strcat(sql_sorting," DESCENDING");
									}
								}
								break;
						}
					}
				}
				
				/*fprintf(stdout,"sql_tables: %s\n",sql_tables);
				fprintf(stdout,"sql_columns: %s\n",sql_columns);
				fprintf(stdout,"sql_where: %s\n",sql_where);
				fprintf(stdout,"sql_sorting: %s\n",sql_sorting);*/
				
				/* append (INSERT INTO ... SELECT) and make-table (SELECT ... INTO) queries */
				if(query_type == 3) {
					sprintf(sql_insert,"INSERT INTO [%s] ",sql_target);
					if(strcmp(sql_target_columns,"") != 0) {
						sprintf(sql_insert+strlen(sql_insert),"(%s) ",sql_target_columns);
					}
				} else if(query_type == 2) {
					sprintf(sql_into," INTO [%s]",sql_target);
				}

				/* print out the sql statement */
				fprintf(stdout,"%sSELECT%s %s%s FROM %s%s%s%s %s\n",sql_insert,sql_predicate,sql_columns,sql_into,sql_tables,sql_where,sql_grouping,sql_having,sql_sorting);
						
				mdb_free_tabledef(table);
			} else {
				fprintf(stderr,"Couldn't read the MSysQueries table\n");
				rc = 1;
			}
			free(query_id);
		} else {
			fprintf(stderr,"Couldn't locate the specified query: %s\n",query_name);
			rc = 1;
		}
	}

	mdb_close(mdb);
	g_free(query_name);

	return rc;
}

/**************************************************** 	
		mdb_list_queries
		Description: 	This function prints the list of queries to stdout
		Parameters:	MdbHandle *mdb (handle for the current database)
								int line_break (whether or not to place a line break between each query)
								char *delimiter (delimiter when not using a line break)
		Returns:			nothing
****************************************************/
void mdb_list_queries(MdbHandle *mdb, int line_break, char *delimiter) {
	unsigned int i;
	MdbCatalogEntry *entry;
	
 	/* loop over each entry in the catalog */
 	for (i=0; i < mdb->num_catalog; i++) {
		entry = g_ptr_array_index(mdb->catalog, i);

     	/* if it's a query */
     	if (entry->object_type == MDB_QUERY) {
       		if (line_break) 
				fprintf (stdout, "%s\n", entry->object_name);
			else if (delimiter) 
				fprintf (stdout, "%s%s", entry->object_name, delimiter);
			else 
				fprintf (stdout, "%s ", entry->object_name);
		}
	}
	if (!line_break) 
		fprintf (stdout, "\n");
}

/**************************************************** 	
		mdb_get_query_id
		Description: 	This function returns the id of the passed query
		Parameters:	MdbHandle *mdb (handle for the current database)
								char *query (name of the desired query to retrieve)
		Returns:			char * (id of the current query)
****************************************************/
char * mdb_get_query_id(MdbHandle *mdb, char *query) {
	unsigned int i;
	MdbCatalogEntry *entry = NULL;
	MdbTableDef *table = NULL;
	char *id = malloc(mdb->bind_size);
	char *name = malloc(mdb->bind_size);
	
 	/* loop over each entry in the catalog */
 	for (i=0; i < mdb->num_catalog; i++) {
		entry = g_ptr_array_index(mdb->catalog, i);

		if(strcmp(entry->object_name,"MSysObjects") == 0) {
			break;
		}
	}
	
	table = mdb_read_table(entry);

	if(table) {
		mdb_read_columns(table);

		mdb_bind_column_by_name(table, "Id", id, NULL);
		mdb_bind_column_by_name(table, "Name", name, NULL);

		mdb_rewind_table(table);

		while (mdb_fetch_row(table)) {
			if(strcmp(query,name) == 0) {
				break;
			}
		}
		mdb_free_tabledef(table);
	}

	free(name);
	
	return id;
}
