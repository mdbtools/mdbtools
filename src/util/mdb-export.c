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

#define EXPORT_BIND_SIZE 200000

#define is_binary_type(x) (x==MDB_OLE || x==MDB_BINARY || x==MDB_REPID)

static char *escapes(char *s);

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
	int escape_cr_lf = 0;
	char *insert_dialect = NULL;
	char *shortdate_fmt = NULL;
	char *date_fmt = NULL;
	char *namespace = NULL;
	char *str_bin_mode = NULL;
	char *null_text = NULL;
	int export_flags = 0;
	char *value;
	size_t length;
	int ret;

	GOptionEntry entries[] = {
		{"no-header", 'H', G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, &header_row, "Suppress header row.", NULL},
		{"delimiter", 'd', 0, G_OPTION_ARG_STRING, &delimiter, "Specify an alternative column delimiter. Default is comma.", "char"},
		{"row-delimiter", 'R', 0, G_OPTION_ARG_STRING, &row_delimiter, "Specify a row delimiter", "char"},
		{"no-quote", 'Q', G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, &quote_text, "Don't wrap text-like fields in quotes.", NULL},
		{"quote", 'q', 0, G_OPTION_ARG_STRING, &quote_char, "Use <char> to wrap text-like fields. Default is double quote.", "char"},
		{"escape", 'X', 0, G_OPTION_ARG_STRING, &escape_char, "Use <char> to escape quoted characters within a field. Default is doubling.", "format"},
		{"escape-invisible", 'e', 0, G_OPTION_ARG_NONE, &escape_cr_lf, "Use C-style escaping for return (\\r), tab (\\t), line-feed (\\n), and back-slash (\\\\) characters. Default is to leave as they are.", NULL},
		{"insert", 'I', 0, G_OPTION_ARG_STRING, &insert_dialect, "INSERT statements (instead of CSV)", "backend"},
		{"namespace", 'N', 0, G_OPTION_ARG_STRING, &namespace, "Prefix identifiers with namespace", "namespace"},
		{"batch-size", 'S', 0, G_OPTION_ARG_INT, &batch_size, "Size of insert batches on supported platforms.", "int"},
		{"date-format", 'D', 0, G_OPTION_ARG_STRING, &shortdate_fmt, "Set the date format (see strftime(3) for details)", "format"},
		{"datetime-format", 'T', 0, G_OPTION_ARG_STRING, &date_fmt, "Set the date/time format (see strftime(3) for details)", "format"},
		{"null", '0', 0, G_OPTION_ARG_STRING, &null_text, "Use <char> to represent a NULL value", "char"},
		{"bin", 'b', 0, G_OPTION_ARG_STRING, &str_bin_mode, "Binary export mode", "strip|raw|octal|hex"},
		{"boolean-words", 'B', 0, G_OPTION_ARG_NONE, &boolean_words, "Use TRUE/FALSE in Boolean fields (default is 0/1)", NULL},
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
			export_flags |= MDB_EXPORT_BINARY_STRIP;
		else if (!strcmp(str_bin_mode, "raw"))
			export_flags |= MDB_EXPORT_BINARY_RAW;
		else if (!strcmp(str_bin_mode, "octal"))
			export_flags |= MDB_EXPORT_BINARY_OCTAL;
		else if (!strcmp(str_bin_mode, "hex"))
			export_flags |= MDB_EXPORT_BINARY_HEXADECIMAL;
		else {
			fputs("Invalid binary mode\n", stderr);
			exit(1);
		}
	} else {
		export_flags |= MDB_EXPORT_BINARY_RAW;
    }

	if (escape_cr_lf) {
		export_flags |= MDB_EXPORT_ESCAPE_CONTROL_CHARS;
	}

	/* Open file */
	if (!(mdb = mdb_open(argv[1], MDB_NOFLAGS))) {
		/* Don't bother clean up memory before exit */
		exit(1);
	}

	if (date_fmt)
		mdb_set_date_fmt(mdb, date_fmt);

	if (shortdate_fmt)
		mdb_set_shortdate_fmt(mdb, shortdate_fmt);

	if (boolean_words)
		mdb_set_boolean_fmt_words(mdb);

    mdb_set_bind_size(mdb, EXPORT_BIND_SIZE);

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

	bound_values = g_malloc(table->num_cols * sizeof(char *));
	bound_lens = g_malloc(table->num_cols * sizeof(int));
	for (i = 0; i < table->num_cols; i++) {
		/* bind columns */
		bound_values[i] = g_malloc0(EXPORT_BIND_SIZE);
		ret = mdb_bind_column(table, i + 1, bound_values[i], &bound_lens[i]);
		if (ret == -1) {
			fprintf(stderr, "Failed to bind column %d\n", i + 1);
			exit(1);
		}
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
					mdb_print_col(outfile, value, quote_text, col->col_type, length, quote_char, escape_char, export_flags);
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
					/* Correctly handle insertion of binary blobs into SQLite using the string literal notation of X'1234ABCD...' */
					if (!strcmp(mdb->backend_name, "sqlite") && is_binary_type(col->col_type)
							&& (export_flags & MDB_EXPORT_BINARY_HEXADECIMAL)) {
						char *quote_char_binary_sqlite = (char *) g_strdup("'");
						fputs("X", outfile);
						mdb_print_col(outfile, value, quote_text, col->col_type, length, quote_char_binary_sqlite, escape_char, export_flags);
						g_free (quote_char_binary_sqlite);
						/* Correctly handle insertion of binary blobs into PostgreSQL using the notation of decode('1234ABCD...', 'hex') */
					} else if (!strcmp(mdb->backend_name, "postgres") && is_binary_type(col->col_type)
							&& (export_flags & MDB_EXPORT_BINARY_HEXADECIMAL)) {
						char *quote_char_binary_postgres = (char *) g_strdup("'");
						fputs("decode(", outfile);
						mdb_print_col(outfile, value, quote_text, col->col_type, length, quote_char_binary_postgres, escape_char, export_flags);
						fputs(", 'hex')", outfile);
						g_free (quote_char_binary_postgres);
						/* No special treatment for other backends or when hexadecimal notation hasn't been selected with the -b hex command line option */
					} else {
						mdb_print_col(outfile, value, quote_text, col->col_type, length, quote_char, escape_char, export_flags);
					}
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
