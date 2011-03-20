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

#define is_quote_type(x) (x==MDB_TEXT || x==MDB_OLE || x==MDB_MEMO || x==MDB_DATETIME || x==MDB_BINARY)
#define is_binary_type(x) (x==MDB_OLE || x==MDB_BINARY)

static char *escapes(char *s);

//#define DONT_ESCAPE_ESCAPE
static void
print_col(gchar *col_val, int quote_text, int col_type, int bin_len, char *quote_char, char *escape_char)
{
	size_t quote_len = strlen(quote_char); /* multibyte */

	size_t orig_escape_len = escape_char ? strlen(escape_char) : 0;

	/* double the quote char if no escape char passed */
	if (!escape_char)
		escape_char = quote_char;

	if (quote_text && is_quote_type(col_type)) {
		fputs(quote_char,stdout);
		while (1) {
			if (is_binary_type(col_type)) {
				if (!bin_len--)
					break;
			} else /* use \0 sentry */
				if (!*col_val)
					break;

			if (quote_len && !strncmp(col_val, quote_char, quote_len)) {
				fprintf(stdout, "%s%s", escape_char, quote_char);
				col_val += quote_len;
#ifndef DONT_ESCAPE_ESCAPE
			} else if (orig_escape_len && !strncmp(col_val, escape_char, orig_escape_len)) {
				fprintf(stdout, "%s%s", escape_char, escape_char);
				col_val += orig_escape_len;
#endif
			} else
				putc(*col_val++, stdout);
		}
		fputs(quote_char,stdout);
	} else
		fputs(col_val,stdout);
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
	char *value;
	size_t length;

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
			if (escape_char) g_free (escape_char);
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
				fputs(delimiter, stdout);
			fputs(sanitize ? sanitize_name(col->name) : col->name, stdout);
		}
		fputs("\n", stdout);
	}

	while(mdb_fetch_row(table)) {

		if (insert_dialect) {
			char *quoted_name;
			if (sanitize)
				quoted_name = sanitize_name(argv[optind + 1]);
			else
				quoted_name = mdb->default_backend->quote_schema_name(NULL, argv[optind + 1]);
			fprintf(stdout, "INSERT INTO %s%s (", namespace, quoted_name);
			free(quoted_name);
			for (j=0;j<table->num_cols;j++) {
				if (j>0) fputs(", ", stdout);
				col=g_ptr_array_index(table->columns,j);
				if (sanitize)
					quoted_name = sanitize_name(col->name);
				else
					quoted_name = mdb->default_backend->quote_schema_name(NULL, col->name);
				fputs(quoted_name, stdout);
				free(quoted_name);
			} 
			fputs(") VALUES (", stdout);
		}

		for (j=0;j<table->num_cols;j++) {
			if (j>0)
				fputs(delimiter, stdout);
			col=g_ptr_array_index(table->columns,j);
			if (!bound_lens[j]) {
				if (insert_dialect)
					fputs("NULL", stdout);
			} else {
				if (col->col_type == MDB_OLE) {
					value = mdb_ole_read_full(mdb, col, &length);
				} else {
					value = bound_values[j];
					length = bound_lens[j];
				}
				print_col(value, quote_text, col->col_type, length, quote_char, escape_char);
				if (col->col_type == MDB_OLE)
					free(value);
			}
		}
		if (insert_dialect) fputs(");", stdout);
		fputs(row_delimiter, stdout);
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

	return 0;
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
