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
	unsigned int j;
	MdbHandle *mdb;
	MdbTableDef *table;
	MdbColumn *col;
	char **bound_values;
	int  *bound_lens; 
	char *delimiter = NULL;
	char *row_delimiter = NULL;
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
			delimiter = (char *) g_strdup(optarg);
		break;
		case 'R':
			row_delimiter = (char *) g_strdup(optarg);
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
	if (!delimiter) {
		delimiter = (char *) g_strdup(",");
	}
	if (!row_delimiter) {
		row_delimiter = (char *) g_strdup("\n");
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
		g_free (delimiter);
		g_free (row_delimiter);
		exit(1);
	}

	mdb_init();

	if (!(mdb = mdb_open(argv[optind], MDB_NOFLAGS))) {
		g_free (delimiter);
		g_free (row_delimiter);
		mdb_exit();
		exit(1);
	}

	table = mdb_read_table_by_name(mdb, argv[argc-1], MDB_TABLE);
	if (!table) {
		g_free (delimiter);
		g_free (row_delimiter);
		mdb_close(mdb);
		mdb_exit();
		exit(0);
	}

	mdb_read_columns(table);
	mdb_rewind_table(table);
	
	bound_values = (char **) g_malloc(table->num_cols * sizeof(char *));
	bound_lens = (int *) g_malloc(table->num_cols * sizeof(int));
	for (j=0;j<table->num_cols;j++) {
		bound_values[j] = (char *) g_malloc0(MDB_BIND_SIZE);
		mdb_bind_column(table, j+1, bound_values[j], &bound_lens[j]);
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

		for (j=0;j<table->num_cols;j++) {
			col=g_ptr_array_index(table->columns,j);
			if ((col->col_type == MDB_OLE)
			 && ((j==0) || (col->cur_value_len))) {
				mdb_ole_read(mdb, col, bound_values[j], MDB_BIND_SIZE);
			}
			if (j>0) {
				fprintf(stdout,"%s",delimiter);
			}
			if (insert_statements && !bound_lens[j]) {
				print_col("NULL",0,col->col_type);
			} else {
				print_col(bound_values[j], quote_text, col->col_type);
			}
		}
		if (insert_statements) fprintf(stdout,")");
		fprintf(stdout,"%s", row_delimiter);
	}
	for (j=0;j<table->num_cols;j++) {
		g_free(bound_values[j]);
	}
	g_free(bound_values);
	g_free(bound_lens);
	mdb_free_tabledef(table);

	g_free (delimiter);
	g_free (row_delimiter);
	mdb_close(mdb);
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

