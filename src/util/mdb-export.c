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

#undef MDB_BIND_SIZE
#define MDB_BIND_SIZE 200000

#define is_quote_type(x) (x==MDB_TEXT || x==MDB_OLE || x==MDB_MEMO || x==MDB_DATETIME || x==MDB_BINARY || x==MDB_REPID)
#define is_binary_type(x) (x==MDB_OLE || x==MDB_BINARY || x==MDB_REPID)

static char *escapes(char *s);

//#define DONT_ESCAPE_ESCAPE
static void
print_col(FILE *outfile, gchar *col_val, int quote_text, int col_type, int bin_len, char *quote_char, char *escape_char, int bin_mode)
/* quote_text: Don't quote if 0.
 */
{
	size_t quote_len = strlen(quote_char); /* multibyte */

	size_t orig_escape_len = escape_char ? strlen(escape_char) : 0;

	/* double the quote char if no escape char passed */
	if (!escape_char)
		escape_char = quote_char;

	if (quote_text && is_quote_type(col_type)) {
		fputs(quote_char, outfile);
		while (1) {
			if (is_binary_type(col_type)) {
				if (bin_mode == MDB_BINEXPORT_STRIP)
					break;
				if (!bin_len--)
					break;
			} else /* use \0 sentry */
				if (!*col_val)
					break;

			if (quote_len && !strncmp(col_val, quote_char, quote_len)) {
				fprintf(outfile, "%s%s", escape_char, quote_char);
				col_val += quote_len;
#ifndef DONT_ESCAPE_ESCAPE
			} else if (orig_escape_len && !strncmp(col_val, escape_char, orig_escape_len)) {
				fprintf(outfile, "%s%s", escape_char, escape_char);
				col_val += orig_escape_len;
#endif
			} else if (is_binary_type(col_type) && bin_mode == MDB_BINEXPORT_OCTAL)
				fprintf(outfile, "\\%03o", *(unsigned char*)col_val++);
			else
				putc(*col_val++, outfile);
		}
		fputs(quote_char, outfile);
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
	char *delimiter = NULL;
	char *row_delimiter = NULL;
	char *quote_char = NULL;
	char *escape_char = NULL;
	int header_row = 1;
	int quote_text = 1;
	int boolean_words = 0;
	int batch_size = 1000;
	char *insert_dialect = NULL;
	char *date_fmt = NULL;
	char *namespace = NULL;
	char *str_bin_mode = NULL;
	char *null_text = NULL;
	int bin_mode = MDB_BINEXPORT_RAW;
	char *value;
	size_t length;

	GOptionEntry entries[] = {
		{"no-header", 'H', G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, &header_row, "Suppress header row.", NULL},
		{"no-quote", 'Q', G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, &quote_text, "Don't wrap text-like fields in quotes.", NULL},
		{"delimiter", 'd', 0, G_OPTION_ARG_STRING, &delimiter, "Specify an alternative column delimiter. Default is comma.", "char"},
		{"row-delimiter", 'R', 0, G_OPTION_ARG_STRING, &row_delimiter, "Specify a row delimiter", "char"},
		{"quote", 'q', 0, G_OPTION_ARG_STRING, &quote_char, "Use <char> to wrap text-like fields. Default is double quote.", "char"},
		{"backend", 'I', 0, G_OPTION_ARG_STRING, &insert_dialect, "INSERT statements (instead of CSV)", "backend"},
		{"date_format", 'D', 0, G_OPTION_ARG_STRING, &date_fmt, "Set the date format (see strftime(3) for details)", "format"},
		{"escape", 'X', 0, G_OPTION_ARG_STRING, &escape_char, "Use <char> to escape quoted characters within a field. Default is doubling.", "format"},
		{"namespace", 'N', 0, G_OPTION_ARG_STRING, &namespace, "Prefix identifiers with namespace", "namespace"},
		{"null", '0', 0, G_OPTION_ARG_STRING, &null_text, "Use <char> to represent a NULL value", "char"},
		{"bin", 'b', 0, G_OPTION_ARG_STRING, &str_bin_mode, "Binary export mode", "strip|raw|octal"},
		{"boolean-words", 'B', 0, G_OPTION_ARG_NONE, &boolean_words, "Use TRUE/FALSE in Boolean fields (default is 0/1)", NULL},
		{"batch-size", 'S', 0, G_OPTION_ARG_INT, &batch_size, "Size of insert batches on supported platforms.", "int"},
		{NULL},
	};
	GError *error = NULL;
	GOptionContext *opt_context;

	opt_context = g_option_context_new("<file> <table> - export data from MDB file");
	g_option_context_add_main_entries(opt_context, entries, NULL /*i18n*/);
	// g_option_context_set_strict_posix(opt_context, TRUE); /* options first, requires glib 2.44 */
	if (!g_option_context_parse (opt_context, &argc, &argv, &error))
	{
		fprintf(stderr, "option parsing failed: %s\n", error->message);
		fputs(g_option_context_get_help(opt_context, TRUE, NULL), stderr);
		exit (1);
	}

	if (argc != 3) {
		fputs("Wrong number of arguments.\n\n", stderr);
		fputs(g_option_context_get_help(opt_context, TRUE, NULL), stderr);
		exit(1);
	}

	/* Process options */
	if (quote_char)
		quote_char = escapes(quote_char);
    else if (insert_dialect && !strcmp(insert_dialect, "postgres"))
        quote_char = g_strdup("'");
    else
		quote_char = g_strdup("\"");

	if (delimiter)
		delimiter = escapes(delimiter);
	else
		delimiter = g_strdup(",");

	if (row_delimiter)
		row_delimiter = escapes(row_delimiter);
	else
		row_delimiter = g_strdup("\n");

	if (escape_char)
		escape_char = escapes(escape_char);

	if (insert_dialect)
		header_row = 0;

	if (null_text)
		null_text = escapes(null_text);
	else
		null_text = g_strdup("");

	if (str_bin_mode) {
		if (!strcmp(str_bin_mode, "strip"))
			bin_mode = MDB_BINEXPORT_STRIP;
		else if (!strcmp(str_bin_mode, "raw"))
			bin_mode = MDB_BINEXPORT_RAW;
		else if (!strcmp(str_bin_mode, "octal"))
			bin_mode = MDB_BINEXPORT_OCTAL;
		else {
			fputs("Invalid binary mode\n", stderr);
			exit(1);
		}
	}

	/* Open file */
	if (!(mdb = mdb_open(argv[1], MDB_NOFLAGS))) {
		/* Don't bother clean up memory before exit */
		exit(1);
	}

	if (date_fmt)
		mdb_set_date_fmt(mdb, date_fmt);

	if (boolean_words)
		mdb_set_boolean_fmt_words(mdb);

	if (insert_dialect)
		if (!mdb_set_default_backend(mdb, insert_dialect)) {
			fputs("Invalid backend type\n", stderr);
			/* Don't bother clean up memory before exit */
			exit(1);
		}

	table = mdb_read_table_by_name(mdb, argv[2], MDB_TABLE);
	if (!table) {
		fprintf(stderr, "Error: Table %s does not exist in this database.\n", argv[2]);
		/* Don't bother clean up memory before exit */
		exit(1);
	}

	/* read table */
	mdb_read_columns(table);
	mdb_rewind_table(table);

	bound_values = (char **) g_malloc(table->num_cols * sizeof(char *));
	bound_lens = (int *) g_malloc(table->num_cols * sizeof(int));
	for (i = 0; i < table->num_cols; i++) {
		/* bind columns */
		bound_values[i] = (char *) g_malloc0(MDB_BIND_SIZE);
		mdb_bind_column(table, i + 1, bound_values[i], &bound_lens[i]);
	}
	if (header_row) {
		for (i = 0; i < table->num_cols; i++) {
			col = g_ptr_array_index(table->columns, i);
			if (i)
				fputs(delimiter, outfile);
			fputs(col->name, outfile);
		}
		fputs(row_delimiter, outfile);
	}

	// TODO refactor this into functions
	if (mdb->default_backend->capabilities & MDB_SHEXP_BULK_INSERT) {
		//for efficiency do multi row insert on engines that support this
		int counter = 0;
		while (mdb_fetch_row(table)) {
			if (counter % batch_size == 0) {
				counter = 0; // reset to 0, prevent overflow on extremely large data sets.
				char *quoted_name;
				quoted_name = mdb->default_backend->quote_schema_name(namespace, argv[2]);
				fprintf(outfile, "INSERT INTO %s (", quoted_name);
				free(quoted_name);
				for (i = 0; i < table->num_cols; i++) {
					if (i > 0) fputs(", ", outfile);
					col = g_ptr_array_index(table->columns, i);
					quoted_name = mdb->default_backend->quote_schema_name(NULL, col->name);
					fputs(quoted_name, outfile);
					free(quoted_name);
				}
				fputs(") VALUES ", outfile);
			} else {
				fputs(", ", outfile);
			}
			fputs("(", outfile);
			for (i = 0; i < table->num_cols; i++) {
				if (i > 0)
					fputs(delimiter, outfile);
				col = g_ptr_array_index(table->columns, i);
				if (!bound_lens[i]) {
					/* Don't quote NULLs */
					if (insert_dialect)
						fputs("NULL", outfile);
					else
						fputs(null_text, outfile);
				} else {
					if (col->col_type == MDB_OLE) {
						value = mdb_ole_read_full(mdb, col, &length);
					} else {
						value = bound_values[i];
						length = bound_lens[i];
					}
					print_col(outfile, value, quote_text, col->col_type, length, quote_char, escape_char, bin_mode);
					if (col->col_type == MDB_OLE)
						free(value);
				}
			}
			fputs(")", outfile);
			if (counter % batch_size == batch_size - 1) {
				fputs(";", outfile);
				fputs(row_delimiter, outfile);
			}
			counter++;
		}
		if (counter % batch_size != 0) {
			//if our last row did not land on closing tag, close the stement here
			fputs(";", outfile);
			fputs(row_delimiter, outfile);
		}
	} else {
		while (mdb_fetch_row(table)) {

			if (insert_dialect) {
				char *quoted_name;
				quoted_name = mdb->default_backend->quote_schema_name(namespace, argv[2]);
				fprintf(outfile, "INSERT INTO %s (", quoted_name);
				free(quoted_name);
				for (i = 0; i < table->num_cols; i++) {
					if (i > 0) fputs(", ", outfile);
					col = g_ptr_array_index(table->columns, i);
					quoted_name = mdb->default_backend->quote_schema_name(NULL, col->name);
					fputs(quoted_name, outfile);
					free(quoted_name);
				}
				fputs(") VALUES (", outfile);
			}

			for (i = 0; i < table->num_cols; i++) {
				if (i > 0)
					fputs(delimiter, outfile);
				col = g_ptr_array_index(table->columns, i);
				if (!bound_lens[i]) {
					/* Don't quote NULLs */
					if (insert_dialect)
						fputs("NULL", outfile);
					else
						fputs(null_text, outfile);
				} else {
					if (col->col_type == MDB_OLE) {
						value = mdb_ole_read_full(mdb, col, &length);
					} else {
						value = bound_values[i];
						length = bound_lens[i];
					}
					print_col(outfile, value, quote_text, col->col_type, length, quote_char, escape_char, bin_mode);
					if (col->col_type == MDB_OLE)
						free(value);
				}
			}
			if (insert_dialect) fputs(");", outfile);
			fputs(row_delimiter, outfile);
		}
	}

	/* free the memory used to bind */
	for (i=0;i<table->num_cols;i++) {
		g_free(bound_values[i]);
	}
	g_free(bound_values);
	g_free(bound_lens);
	mdb_free_tabledef(table);

	mdb_close(mdb);
	g_option_context_free(opt_context);

	// g_free ignores NULL
	g_free(quote_char);
	g_free(delimiter);
	g_free(row_delimiter);
	g_free(insert_dialect);
	g_free(date_fmt);
	g_free(escape_char);
	g_free(namespace);
	g_free(str_bin_mode);
	return 0;
}

static char *escapes(char *s)
{
	char *d = (char *) g_strdup(s);
	char *t = d;
	char *orig = s;
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
	g_free(orig);
	return d;
}
