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

#ifdef DMALLOC
#include "dmalloc.h"
#endif

#undef MDB_BIND_SIZE
#define MDB_BIND_SIZE 200000

#define is_text_type(x) (x==MDB_TEXT || x==MDB_MEMO || x==MDB_SDATETIME)

static char *sanitize_name(char *str, int sanitize);

void
print_col(gchar *col_val, int quote_text, int col_type)
{
	gchar *s;

	if (quote_text && is_text_type(col_type)) {
		fprintf(stdout,"\"");
		for (s=col_val;*s;s++) {
			if (*s=='"') fprintf(stdout,"\"\"");
			else fprintf(stdout,"%c",*s);
		}
		fprintf(stdout,"\"");
	} else {
		fprintf(stdout,"%s",col_val);
	}
}
int
main(int argc, char **argv)
{
	int i, j;
	MdbHandle *mdb;
	MdbCatalogEntry *entry;
	MdbTableDef *table;
	MdbColumn *col;
	/* doesn't handle tables > 256 columns.  Can that happen? */
	char *bound_values[256]; 
	int  bound_lens[256]; 
	char *delimiter = ",";
	char *row_delimiter = "\n";
	char header_row = 1;
	char quote_text = 1;
	char insert_statements = 0;
	char sanitize = 0;
	int  opt;

	while ((opt=getopt(argc, argv, "HQd:D:R:IS"))!=-1) {
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
		case 'R':
			row_delimiter = (char *) malloc(strlen(optarg)+1);
			strcpy(row_delimiter, optarg);
		break;
		case 'I':
			insert_statements = 1;
			header_row = 0;
		break;
		case 'S':
			sanitize = 1;
		break;
		case 'D':
			mdb_set_date_fmt(optarg);
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
		fprintf(stderr,"  -R <delimiter> specify a row delimiter\n");
		fprintf(stderr,"  -I             INSERT statements (instead of CSV)\n");
		fprintf(stderr,"  -D <format>    set the date format (see strftime(3) for details)\n");
		fprintf(stderr,"  -S             Sanitize names (replace spaces etc. with underscore)\n");
		exit(1);
	}

	mdb_init();

	if (!(mdb = mdb_open(argv[optind]))) {
		exit(1);
	}
	
 	if (!mdb_read_catalog (mdb, MDB_TABLE)) {
		fprintf(stderr,"File does not appear to be an Access database\n");
		exit(1);
	}

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
                		mdb_bind_len(table, j+1, &bound_lens[j]);
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

				if (insert_statements) {
					fprintf(stdout, "INSERT INTO %s (",
						sanitize_name(argv[optind + 1],sanitize));
        			for (j=0;j<table->num_cols;j++) {
						if (j>0) fprintf(stdout, ", ");
						col=g_ptr_array_index(table->columns,j);
						fprintf(stdout,"%s", sanitize_name(col->name,sanitize));
					} 
					fprintf(stdout, ") VALUES (");
				}

				col=g_ptr_array_index(table->columns,0);
				if (col->col_type == MDB_OLE) {
					mdb_ole_read(mdb, col, bound_values[0], MDB_BIND_SIZE);	
				}

				if (insert_statements && !bound_lens[0]) 
					print_col("NULL",0,col->col_type);
				else 	
					print_col(bound_values[0], 
					quote_text, 
					col->col_type);

        			for (j=1;j<table->num_cols;j++) {
					col=g_ptr_array_index(table->columns,j);
					if (col->col_type == MDB_OLE) {
						if (col->cur_value_len)
						mdb_ole_read(mdb, col, bound_values[j], MDB_BIND_SIZE);	
					}
					fprintf(stdout,"%s",delimiter);
					if (insert_statements && !bound_lens[j]) 
						print_col("NULL",0,col->col_type);
					else 
						print_col(bound_values[j], 
						quote_text, 
						col->col_type);
				}
				if (insert_statements) fprintf(stdout,")");
				fprintf(stdout,"%s", row_delimiter);
			}
        		for (j=0;j<table->num_cols;j++) {
				free(bound_values[j]);
			}
			mdb_free_tabledef(table);
		}
	}

	mdb_close(mdb);
	mdb_free_handle(mdb);
	mdb_exit();

	exit(0);
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

