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

#define is_text_type(x) (x==MDB_TEXT || x==MDB_OLE || x==MDB_MEMO || x==MDB_SDATETIME || x==MDB_BINARY)

static char *escapes(char *s);

void
print_col(gchar *col_val, int quote_text, int col_type, int bin_length, char *quote_char, char *escape_char)
{
	gchar *s;

	if (quote_text && is_text_type(col_type)) {
		fprintf(stdout,quote_char);
		if (col_type == MDB_OLE || col_type == MDB_BINARY)	{
			while (bin_length--) {
				unsigned char c = (unsigned char)*col_val++;
				if (c>=32 && c<=127)
					putc(c, stdout);
				else
					fprintf(stdout, "\\%03o", c);
			}
		}
		else
			for (s=col_val;*s;s++) {
				if (strlen(quote_char)==1 && *s==quote_char[0]) {
					/* double the char if no escape char passed */
					if (!escape_char) {
						fprintf(stdout,"%s%s",quote_char,quote_char);
					} else {
						fprintf(stdout,"%s%s",escape_char,quote_char);
					}
				}
				else fprintf(stdout,"%c",*s);
			}
		fprintf(stdout,quote_char);
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
	char *quote_char = NULL;
	char *escape_char = NULL;
	char header_row = 1;
	char quote_text = 1;
	char *insert_dialect = NULL;
	char sanitize = 0;
	char *namespace = "";
	int  opt;

	while ((opt=getopt(argc, argv, "HQq:X:d:D:R:I:N:S"))!=-1) {
		switch (opt) {
		case 'H':
			header_row = 0;
		break;
		case 'Q':
			quote_text = 0;
		break;
		case 'q':
			quote_char = (char *) g_strdup(optarg);
		break;
		case 'd':
			delimiter = escapes(optarg);
		break;
		case 'R':
			row_delimiter = escapes(optarg);
		break;
		case 'I':
			insert_dialect = (char*) g_strdup(optarg);
			header_row = 0;
		break;
		case 'S':
			sanitize = 1;
		break;
		case 'D':
			mdb_set_date_fmt(optarg);
		break;
		case 'X':
			escape_char = (char *) g_strdup(optarg);
		break;
		case 'N':
			namespace = (char *) g_strdup(optarg);
		break;
		default:
		break;
		}
	}
	if (!quote_char) {
		quote_char = (char *) g_strdup("\"");
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
		fprintf(stderr,"  -I <backend>   INSERT statements (instead of CSV)\n");
		fprintf(stderr,"  -D <format>    set the date format (see strftime(3) for details)\n");
		fprintf(stderr,"  -S             Sanitize names (replace spaces etc. with underscore)\n");
		fprintf(stderr,"  -q <char>      Use <char> to wrap text-like fields. Default is \".\n");
		fprintf(stderr,"  -X <char>      Use <char> to escape quoted characters within a field. Default is doubling.\n");
		fprintf(stderr,"  -N <namespace> Prefix identifiers with namespace\n");
		g_free (delimiter);
		g_free (row_delimiter);
		g_free (quote_char);
		if (escape_char) g_free (escape_char);
		exit(1);
	}

	mdb_init();

	if (!(mdb = mdb_open(argv[optind], MDB_NOFLAGS))) {
		g_free (delimiter);
		g_free (row_delimiter);
		g_free (quote_char);
		if (escape_char) g_free (escape_char);
		mdb_exit();
		exit(1);
	}

	if (insert_dialect)
		if (!mdb_set_default_backend(mdb, insert_dialect)) {
			fprintf(stderr, "Invalid backend type\n");
			mdb_exit();
			exit(1);
		}

	table = mdb_read_table_by_name(mdb, argv[argc-1], MDB_TABLE);
	if (!table) {
		fprintf(stderr, "Error: Table %s does not exist in this database.\n", argv[argc-1]);
		g_free (delimiter);
		g_free (row_delimiter);
		g_free (quote_char);
		if (escape_char) g_free (escape_char);
		mdb_close(mdb);
		mdb_exit();
		exit(1);
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
		for (j=0; j<table->num_cols; j++) {
			col=g_ptr_array_index(table->columns,j);
			if (j)
				fprintf(stdout,delimiter);
			fprintf(stdout,"%s", sanitize ? sanitize_name(col->name) : col->name);
		}
		fprintf(stdout,"\n");
	}

	while(mdb_fetch_row(table)) {

		if (insert_dialect) {
			char *quoted_name;
			if (sanitize)
				quoted_name = sanitize_name(argv[optind + 1]);
			else
				quoted_name = mdb->default_backend->quote_name(argv[optind + 1]);
			fprintf(stdout, "INSERT INTO %s%s (", namespace, quoted_name);
			free(quoted_name);
			for (j=0;j<table->num_cols;j++) {
				if (j>0) fprintf(stdout, ", ");
				col=g_ptr_array_index(table->columns,j);
				if (sanitize)
					quoted_name = sanitize_name(col->name);
				else
					quoted_name = mdb->default_backend->quote_name(col->name);
				fprintf(stdout,"%s", quoted_name);
				free(quoted_name);
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
				fprintf(stdout,delimiter);
			}
			if (!bound_lens[j]) {
				print_col(insert_dialect?"NULL":"",0,col->col_type, 0, quote_char, escape_char);
			} else {
				print_col(bound_values[j], quote_text, col->col_type, bound_lens[j], quote_char, escape_char);
			}
		}
		if (insert_dialect) fprintf(stdout,");");
		fprintf(stdout, row_delimiter);
	}
	for (j=0;j<table->num_cols;j++) {
		g_free(bound_values[j]);
	}
	g_free(bound_values);
	g_free(bound_lens);
	mdb_free_tabledef(table);

	g_free (delimiter);
	g_free (row_delimiter);
	g_free (quote_char);
	if (escape_char) g_free (escape_char);
	mdb_close(mdb);
	mdb_exit();

	exit(0);
}

static char *escapes(char *s)
{
	char *d = (char *) g_strdup(s);
	char *t = d;
	unsigned char encode = 0;

	for (;*s; s++) {
		if (encode) {
			switch (*s) {
			case 'n': *t++='\n'; break;
			case 't': *t++='\t'; break;
			case 'r': *t++='\r'; break;
			default: *t++='\\'; *t++=*s; break;
			}	
			encode=0;
		} else if (*s=='\\') {
			encode=1;
		} else {
			*t++=*s;
		}
	}
	*t='\0';
	return d;
}
