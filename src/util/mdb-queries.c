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
	char *sql_sorting = malloc(bind_size);
	char *sql_group_by = malloc(bind_size);
	char *sql_having = malloc(bind_size);
	int flagint;
	char *locale = NULL;
	int print_mdbver = 0;
	int saw_case_5 = 0;
	
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

				while (mdb_fetch_row(table)) {
					if(strcmp(query_id,objectid) == 0) {
						flagint = atoi(flag);
						//we have a row for our query
						switch(atoi(attribute)) {
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
								if(strcmp(sql_tables,"") == 0) {
									strcpy(sql_tables,name1);
								} else {
									strcat(sql_tables,",");
									strcat(sql_tables,name1);
								}
								saw_case_5 = 1;
								break;
							case 6:		// column name
								if(strcmp(sql_columns,"") == 0) {
									strcpy(sql_columns,expression);
								} else {
									strcat(sql_columns,",");
									strcat(sql_columns,expression);
								}
								break;
							case 7:		// join/relationship where clause
								fprintf(stderr,"## join tables: %s - %s\n",name1,name2);
								fprintf(stderr,"## join clause: %s\n",expression);
								if(saw_case_5) {
                                    strcpy(sql_tables, " /*7a*/ ");
                                    strcat(sql_tables, name1);
                                    saw_case_5 = 0;
								} else {
                                    strcat(sql_tables, " /*7b2*/ ");
                                    strcat(sql_tables, name1);
								}
                                switch(flagint) {
                                    case 1: strcat(sql_tables, "\n INNER"); break;
                                    case 2: strcat(sql_tables, "\n  LEFT"); break;
                                    case 3: strcat(sql_tables, "\n RIGHT"); break;
                                    default: sprintf(sql_tables + strlen(sql_tables), " /* unknown flagint: %d */ ", flagint);
                                }
                                strcat(sql_tables," JOIN ");
                                strcat(sql_tables, name2);
                                strcat(sql_tables,"\n    ON ");
                                strcat(sql_tables,expression);
                                fprintf(stderr, "attribute: %s, flagint: %d, name1: %s, name2: %s, expression: %s, order: %s\n",
                                        attribute, flagint, name1, name2, expression, order);
								break;
							case 8:		// where clause
								strcpy(sql_where,expression);
								break;
						    case 9:     // group by
                                if(strcmp(sql_group_by,"") == 0) {
                                    strcpy(sql_group_by," GROUP BY ");
                                } else {
                                    strcat(sql_group_by,",");
                                }
                                strcat(sql_group_by,expression);
						        break;
						    case 10:    // having
						        strcpy(sql_having,"HAVING ");
                                strcat(sql_having,expression);
						        break;
							case 11:		// sorting
								if(strcmp(sql_sorting,"") == 0) {
									strcpy(sql_sorting," ORDER BY ");
									strcat(sql_sorting,expression);
									if(strcmp(name1,"D") == 0) {
										strcat(sql_sorting," DESCENDING");
									}
								}
								break;
						    default:
						        fprintf(stderr, "attribute: %s, flagint: %d, name1: %s, name2: %s, expression: %s, order: %s\n",
                                        attribute, flagint, name1, name2, expression, order);
						}
					}
				}
				
				/*fprintf(stderr,"sql_tables: %s\n",sql_tables);
				fprintf(stderr,"sql_columns: %s\n",sql_columns);
				fprintf(stderr,"sql_where: %s\n",sql_where);
				fprintf(stderr,"sql_sorting: %s\n",sql_sorting);*/
				
				/* print out the sql statement */
                fprintf(stdout,"SELECT%s %s\n  FROM %s\n",sql_predicate,sql_columns,sql_tables);
				if(*sql_where) {
					fprintf(stdout," WHERE %s\n", sql_where);
				}
                if(*sql_group_by) {
                    fprintf(stdout, "%s\n", sql_group_by);
                }
                if(*sql_having) {
                    fprintf(stdout, "%s\n", sql_having);
                }
                if(*sql_sorting) {
                    fprintf(stdout, "%s\n", sql_sorting);
                }
				mdb_free_tabledef(table);
			}
			free(query_id);
		} else {
			fprintf(stderr,"Couldn't locate the specified query: %s\n",query_name);
		}
	}

	mdb_close(mdb);
	g_free(query_name);
	
	return 0;
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
