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

/* Join types stored in the Flag column of MSysQueries join rows (attribute 7) */
#define QUERY_JOIN_INNER 1
#define QUERY_JOIN_LEFT  2
#define QUERY_JOIN_RIGHT 3

/* A source in the FROM clause: either a table (or saved query), possibly
 * aliased, or a join of two other sources. */
typedef struct QuerySource {
	char *name;	/* name the table is referred to by (its alias, if any); NULL for joins */
	char *sql;	/* table reference as written in the FROM clause */
	struct QuerySource *left;
	struct QuerySource *right;
	int join_type;
	char *on;
	int on_count;
} QuerySource;

typedef struct {
	char *name1;
	char *name2;
	char *expression;
	int flag;
} QueryJoin;

void mdb_list_queries(MdbHandle *mdb, int line_break, char *delimiter);
char * mdb_get_query_id(MdbHandle *mdb,char *query);
static QuerySource *query_source_table(const char *table, const char *alias);
static void query_source_free(gpointer data, gpointer user_data);
static void query_join_free(gpointer data, gpointer user_data);
static char *mdb_build_from_clause(GPtrArray *sources, GPtrArray *joins);

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
	char *sql_tables = NULL;
	GPtrArray *sources = NULL;
	GPtrArray *joins = NULL;
	QueryJoin *join;
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

				sources = g_ptr_array_new();
				joins = g_ptr_array_new();
				sql_predicate[0] = sql_columns[0] = sql_where[0] = '\0';
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
							case 5:		// table name; Name2 = alias
								g_ptr_array_add(sources, query_source_table(name1, name2));
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
							case 7:		// join: Name1, Name2 = tables (or aliases), Flag = join type
								join = g_malloc0(sizeof(QueryJoin));
								join->name1 = g_strdup(name1);
								join->name2 = g_strdup(name2);
								join->expression = g_strdup(expression);
								join->flag = flagint;
								g_ptr_array_add(joins, join);
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
				
				sql_tables = mdb_build_from_clause(sources, joins);
				g_ptr_array_foreach(sources, query_source_free, NULL);
				g_ptr_array_free(sources, TRUE);
				g_ptr_array_foreach(joins, query_join_free, NULL);
				g_ptr_array_free(joins, TRUE);

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

				g_free(sql_tables);
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

/****************************************************
		FROM clause reconstruction

		MSysQueries stores each table of a query in its own row
		(attribute 5, Name2 = alias) and each join in a row (attribute 7)
		naming the two joined tables. Like Access, fold the joins in order
		into the list of tables: the sources containing the two tables of
		a join are replaced by a join of the two, so earlier joins end up
		nested in parentheses, e.g. "(a INNER JOIN b ON ...) INNER JOIN c
		ON ...". Whatever remains in the list is separated by commas.
****************************************************/
static QuerySource *query_source_table(const char *table, const char *alias) {
	QuerySource *source = g_malloc0(sizeof(QuerySource));

	if (alias && alias[0]) {
		source->name = g_strdup(alias);
		source->sql = g_strdup_printf("[%s] AS [%s]", table, alias);
	} else {
		source->name = g_strdup(table);
		source->sql = g_strdup_printf("[%s]", table);
	}
	return source;
}

static void query_source_free(gpointer data, gpointer user_data) {
	QuerySource *source = data;

	if (!source)
		return;
	query_source_free(source->left, NULL);
	query_source_free(source->right, NULL);
	g_free(source->name);
	g_free(source->sql);
	g_free(source->on);
	g_free(source);
}

static void query_join_free(gpointer data, gpointer user_data) {
	QueryJoin *join = data;

	g_free(join->name1);
	g_free(join->name2);
	g_free(join->expression);
	g_free(join);
}

static int query_source_contains(QuerySource *source, const char *name) {
	if (source->name)
		return g_ascii_strcasecmp(source->name, name) == 0;
	return query_source_contains(source->left, name) || query_source_contains(source->right, name);
}

/* Add a join condition; several conditions are written as (a) AND (b) */
static void query_source_add_condition(QuerySource *source, const char *expression) {
	char *on;

	if (source->on_count == 0)
		on = g_strdup(expression);
	else if (source->on_count == 1)
		on = g_strdup_printf("(%s) AND (%s)", source->on, expression);
	else
		on = g_strdup_printf("%s AND (%s)", source->on, expression);
	g_free(source->on);
	source->on = on;
	source->on_count++;
}

static void query_source_append_sql(GString *out, QuerySource *source, int nested) {
	const char *join_type;

	if (source->name) {
		g_string_append(out, source->sql);
		return;
	}
	switch (source->join_type) {
		case QUERY_JOIN_LEFT:
			join_type = " LEFT JOIN ";
			break;
		case QUERY_JOIN_RIGHT:
			join_type = " RIGHT JOIN ";
			break;
		default:
			join_type = " INNER JOIN ";
			break;
	}
	if (nested)
		g_string_append(out, "(");
	query_source_append_sql(out, source->left, 1);
	g_string_append(out, join_type);
	query_source_append_sql(out, source->right, 1);
	g_string_append(out, " ON ");
	g_string_append(out, source->on);
	if (nested)
		g_string_append(out, ")");
}

static char *mdb_build_from_clause(GPtrArray *sources, GPtrArray *joins) {
	QuerySource *from, *to, *source;
	QueryJoin *join;
	GString *out;
	unsigned int i, j;

	for (j=0; j < joins->len; j++) {
		join = g_ptr_array_index(joins, j);
		from = to = NULL;
		for (i=0; i < sources->len && (!from || !to); i++) {
			source = g_ptr_array_index(sources, i);
			if (!from && query_source_contains(source, join->name1)) {
				from = source;
				if (!to && query_source_contains(source, join->name2))
					to = source;
			} else if (!to && query_source_contains(source, join->name2)) {
				to = source;
			}
		}
		if (from && from == to) {
			/* the tables are already joined: another condition of that join */
			if (!from->name)
				query_source_add_condition(from, join->expression);
			continue;
		}
		if (from)
			g_ptr_array_remove(sources, from);
		else
			from = query_source_table(join->name1, NULL);
		if (to)
			g_ptr_array_remove(sources, to);
		else
			to = query_source_table(join->name2, NULL);

		source = g_malloc0(sizeof(QuerySource));
		source->left = from;
		source->right = to;
		source->join_type = join->flag;
		query_source_add_condition(source, join->expression);
		g_ptr_array_add(sources, source);
	}

	out = g_string_new("");
	for (i=0; i < sources->len; i++) {
		if (i > 0)
			g_string_append(out, ",");
		query_source_append_sql(out, g_ptr_array_index(sources, i), 0);
	}
	return g_string_free(out, FALSE);
}
