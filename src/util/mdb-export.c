/* MDB Tools - A library for reading MS Access database file
 * Copyright (C) 2000 Brian Bruns
 *
 *
 * This library is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "mdbtools.h"

#define is_text_type(x) (x==MDB_TEXT || x==MDB_MEMO || x==MDB_SDATETIME)
main(int argc, char **argv)
{
int rows;
int i, j;
unsigned char buf[2048];
MdbHandle *mdb;
MdbCatalogEntry *entry;
MdbTableDef *table;
MdbColumn *col;
/* doesn't handle tables > 256 columns.  Can that happen? */
char *bound_values[256]; 
char *delimiter = ",";
char header_row = 1;
char quote_text = 1;
int  opt;
char *s;

	while ((opt=getopt(argc, argv, "HQd:"))!=-1) {
		switch (opt) {
		case 'H':
			header_row = 0;
		break;
		case 'Q':
			quote_text = 0;
		break;
		case 'd':
			delimiter = (char *) malloc(strlen(optarg)+1);
			strcpy(delimiter, optarg);
		break;
		default:
		break;
		}
	}
	
	/* 
	** optind is now the position of the first non-option arg, 
	** see getopt(3) 
	*/
	if (argc-optind < 2) {
		fprintf(stderr,"Usage: %s [options] <file> <table>\n",argv[0]);
		fprintf(stderr,"where options are:\n");
		fprintf(stderr,"  -H             supress header row\n");
		fprintf(stderr,"  -Q             don't wrap text-like fields in quotes\n");
		fprintf(stderr,"  -d <delimiter> specify a column delimiter\n");
		exit(1);
	}

	mdb_init();

	if (!(mdb = mdb_open(argv[optind]))) {
		exit(1);
	}
	
	mdb_read_catalog(mdb, MDB_TABLE);

	for (i=0;i<mdb->num_catalog;i++) {
		entry = g_ptr_array_index(mdb->catalog,i);
		if (entry->object_type == MDB_TABLE &&
			!strcmp(entry->object_name,argv[argc-1])) {
			table = mdb_read_table(entry);
			mdb_read_columns(table);
			mdb_rewind_table(table);
			
        		for (j=0;j<table->num_cols;j++) {
                	bound_values[j] = (char *) malloc(MDB_BIND_SIZE);
				bound_values[j][0] = '\0';
                		mdb_bind_column(table, j+1, bound_values[j]);
        		}
			if (header_row) {
				col=g_ptr_array_index(table->columns,0);
				fprintf(stdout,"%s",col->name);
        			for (j=1;j<table->num_cols;j++) {
					col=g_ptr_array_index(table->columns,j);
					fprintf(stdout,"%s%s",delimiter,col->name);
				}
				fprintf(stdout,"\n");
			}

			while(mdb_fetch_row(table)) {
				if (quote_text && is_text_type(col->col_type)) {
					fprintf(stdout,"\"");
					for (s=bound_values[0];*s;s++) {
						if (*s=='"') fprintf(stdout,"\"\"");
						else fprintf(stdout,"%c",*s);
					}
					fprintf(stdout,"\"");
					/* fprintf(stdout,"\"%s\"",bound_values[0]); */
				} else {
					fprintf(stdout,"%s",bound_values[0]);
				}
        		for (j=1;j<table->num_cols;j++) {
					col=g_ptr_array_index(table->columns,j);
					if (quote_text && is_text_type(col->col_type)) {
						fprintf(stdout,"%s",delimiter);
						fprintf(stdout,"\"");
						for (s=bound_values[j];*s;s++) {
							if (*s=='"') fprintf(stdout,"\"\"");
							else fprintf(stdout,"%c",*s);
						}
						fprintf(stdout,"\"");
					} else {
						fprintf(stdout,"%s%s",delimiter,bound_values[j]);
					}
				}
				fprintf(stdout,"\n");
			}
        		for (j=0;j<table->num_cols;j++) {
				free(bound_values[j]);
			}
		}
	}

	mdb_free_handle(mdb);
	mdb_exit();

	exit(0);
}

