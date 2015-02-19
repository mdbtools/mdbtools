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

#include "mdbtools.h"

#ifdef DMALLOC
#include "dmalloc.h"
#endif

#undef MDB_BIND_SIZE
#define MDB_BIND_SIZE 200000

#define is_quote_type(x) (x==MDB_TEXT || x==MDB_OLE || x==MDB_MEMO || x==MDB_DATETIME || x==MDB_BINARY || x==MDB_REPID)
#define is_binary_type(x) (x==MDB_OLE || x==MDB_BINARY || x==MDB_REPID)

static char *quote_char = "\"";
static char *escape_char = "\\";
static char *separator_char = ":";
static char *row_start = "{";
static char *row_end = "}\n";
static char *delimiter = ",";
static size_t quote_len = 1; //strlen(quote_char); /* multibyte */
static size_t orig_escape_len = 1; //strlen(escape_char);
static int drop_nonascii;


//#define DONT_ESCAPE_ESCAPE
static void
print_quoted_value(FILE *outfile, char* value, int bin_len) {
	fputs(quote_char, outfile);
	int is_binary = (bin_len != -1);
	while (1) {
		if (is_binary) {
			if (!bin_len--)
				break;
		} else /* use \0 sentry */
			if (!*value)
				break;

		if (quote_len && !strncmp(value, quote_char, quote_len)) {
			fprintf(outfile, "%s%s", escape_char, quote_char);
			value += quote_len;
#ifndef DONT_ESCAPE_ESCAPE
		} else if (orig_escape_len && !strncmp(value, escape_char, orig_escape_len)) {
			fprintf(outfile, "%s%s", escape_char, escape_char);
			value += orig_escape_len;
#endif
		} else if (*value < 0x20) {
			if (!is_binary || drop_nonascii) {
				putc(' ', outfile);
				++value;
			} else {
				// escape control codes / binary data.
				fprintf(outfile, "\\x%02x", *(unsigned char*)value++);
			}
		} else {
			putc(*value++, outfile);
		}
	}
	fputs(quote_char, outfile);
}

static void
print_col(FILE *outfile, char* col_name, gchar *col_val, int col_type, int bin_len) {
	print_quoted_value(outfile, col_name, -1);
	fputs(separator_char, outfile);
	if (is_quote_type(col_type)) {
		if (!is_binary_type(col_type)) {
			bin_len = -1;
		}
		print_quoted_value(outfile, col_val, bin_len);
	} else
		fputs(col_val, outfile);
}
int
main(int argc, char **argv)
{
	unsigned int i;
	MdbHandle *mdb;
	MdbTableDef *table;
	MdbColumn *col;
	char **bound_values;
	int  *bound_lens;
	FILE *outfile = stdout;
	drop_nonascii = 0;
	int  opt;
	char *value;
	size_t length;

	while ((opt=getopt(argc, argv, "AD:"))!=-1) {
		switch (opt) {
		case 'A':
			drop_nonascii = 1;
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
		fprintf(stderr,"  -D <format>          set the date format (see strftime(3) for details)\n");
		fprintf(stderr,"  -A                   drop non ascii characters in non-binary fields\n");
		exit(1);
	}

	if (!(mdb = mdb_open(argv[optind], MDB_NOFLAGS))) {
		exit(1);
	}

	table = mdb_read_table_by_name(mdb, argv[argc-1], MDB_TABLE);
	if (!table) {
		fprintf(stderr, "Error: Table %s does not exist in this database.\n", argv[argc-1]);
		mdb_close(mdb);
		exit(1);
	}

	/* read table */
	mdb_read_columns(table);
	mdb_rewind_table(table);

	bound_values = (char **) g_malloc(table->num_cols * sizeof(char *));
	bound_lens = (int *) g_malloc(table->num_cols * sizeof(int));
	for (i=0;i<table->num_cols;i++) {
		/* bind columns */
		bound_values[i] = (char *) g_malloc0(MDB_BIND_SIZE);
		mdb_bind_column(table, i+1, bound_values[i], &bound_lens[i]);
	}

	while(mdb_fetch_row(table)) {
		fputs(row_start, outfile);
		int add_delimiter = 0;
		for (i=0;i<table->num_cols;i++) {
			col=g_ptr_array_index(table->columns,i);
			if (bound_lens[i]) {
				if (add_delimiter) {
					fputs(delimiter, outfile);
					add_delimiter = 0;
				}

				if (col->col_type == MDB_OLE) {
					value = mdb_ole_read_full(mdb, col, &length);
				} else {
					value = bound_values[i];
					length = bound_lens[i];
				}
				print_col(outfile, col->name, value, col->col_type, length);
				add_delimiter = 1;
				if (col->col_type == MDB_OLE)
					free(value);
			}
		}
		fputs(row_end, outfile);
	}

	/* free the memory used to bind */
	for (i=0;i<table->num_cols;i++) {
		g_free(bound_values[i]);
	}
	g_free(bound_values);
	g_free(bound_lens);
	mdb_free_tabledef(table);

	mdb_close(mdb);
	return 0;
}
