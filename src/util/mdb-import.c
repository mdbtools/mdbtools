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

#define MAX_ROW_SIZE 4096

void
free_values(MdbField *fields, int num_fields)
{
	int i;

	for (i=0;i<num_fields;i++) {
		if (!fields[i].is_null)
			g_free(fields[i].value);
	}
}
int
convert_field(MdbColumn *col, char *s, MdbField *field)
{
	char *c;
	MdbAny any;

	field->colnum = col->col_num;
	field->is_fixed = col->is_fixed;
	switch (col->col_type) {
		case MDB_TEXT:
			field->value = g_strdup(s);
			field->siz = strlen(s);
			break;
		case MDB_BYTE:
			any.i = strtol(s, &c, 16);
			if (*c) return 1;
			field->siz = mdb_col_fixed_size(col);
			field->value = g_malloc(field->siz);
			((unsigned char *)field->value)[0] = any.i;
			break;
		case MDB_INT:
			any.i = strtol(s, &c, 16);
			if (*c) return 1;
			field->siz = mdb_col_fixed_size(col);
			field->value = g_malloc(field->siz);
			mdb_put_int16(field->value, 0, any.i);
			break;
		case MDB_LONGINT:
			any.i = strtol(s, &c, 16);
			if (*c) return 1;
			field->siz = mdb_col_fixed_size(col);
			field->value = g_malloc(field->siz);
			mdb_put_int32(field->value, 0, any.i);
			break;
		case MDB_BOOL:
			if (*s == '1') {
				field->is_null = 1;
			} else if (*s == '0') {
				field->is_null = 0;
			} else {
				fprintf(stderr, "%c is not a valid value for type BOOLEAN\n", *s);
				return 1;
			}
			/*
		case MDB_FLOAT:
			any.d = strtod(s, &c);
			if (*c) return 1;
			field.value = g_malloc(mdb_col_fixed_size(col));
			mdb_put_single(field.value, 0, any.i);
			break;
		case MDB_DOUBLE:
			any.d = strtod(s, &c);
			if (*c) return 1;
			field.value = g_malloc(mdb_col_fixed_size(col));
			mdb_put_double(field.value, 0, any.i);
			break;
		*/
			/*
		case MDB_MONEY:
		case MDB_SDATETIME:
		case MDB_OLE:
		case MDB_MEMO:
		case MDB_REPID:
		case MDB_NUMERIC:
			*/
		default:
			fprintf(stderr,"Conversion of type %02x not supported yet.\n", col->col_type);
			return 1;
			break;
	}
	return 0;
}
int
prep_row(MdbTableDef *table, char *line, MdbField *fields, char *delim)
{
	MdbColumn *col;
	char **sarray, *s;
	unsigned int i;

	/*
	 * pull apart the row and turn it into something respectable
	 */
	g_strdelimit(line, delim, '\n');
	sarray = g_strsplit (line, "\n", 0);
	for (i=0; (s = sarray[i]); i++) {
		if (!strlen(s)) continue;
		if (i >= table->num_cols) {
			fprintf(stderr, "Number of columns in file exceeds number in table.\n");
			g_strfreev(sarray);
			return 0;
		}
		if (s[0]=='"' && s[strlen(s)-1]=='"') {
			s[strlen(s)-1] = '\0';
			memmove(s+1, s, strlen(s));
		}
		printf("field = %s\n", s);
		col = g_ptr_array_index (table->columns, i);
		if (convert_field(col, s, &fields[i])) {
			fprintf(stderr, "Format error in column %d\n", i+1);
			g_strfreev(sarray);
			return 0;
		}
	}
	g_strfreev(sarray);

	/*
	 * verify number of columns in table against number in row
	 */
	if (i < table->num_cols) {
		fprintf(stderr, "Row has %d columns, but table has %d\n", i, table->num_cols);
		free_values(fields, i);
		return 0;
	}
	return i-1;
}
int
main(int argc, char **argv)
{
	int i, row;
	MdbHandle *mdb;
	MdbTableDef *table;
	MdbField *fields;
	char line[MAX_ROW_SIZE];
	int num_fields;
	/* doesn't handle tables > 256 columns.  Can that happen? */
	FILE *in;
	char *delimiter;
	int header_rows = 0;

	GOptionEntry entries[] = {
		{ "header", 'H', 0, G_OPTION_ARG_INT, &header_rows, "skip <rows> header rows", "row"},
		{ "delimiter", 'd', 0, G_OPTION_ARG_STRING, &delimiter, "Specify a column delimiter", "char"},
		{ NULL },
	};
	GError *error = NULL;
	GOptionContext *opt_context;

	opt_context = g_option_context_new("<mdbfile> <table> <csvfile> - import data into MDB file");
	g_option_context_add_main_entries(opt_context, entries, NULL /*i18n*/);
	// g_option_context_set_strict_posix(opt_context, TRUE); /* options first, requires glib 2.44 */
	if (!g_option_context_parse (opt_context, &argc, &argv, &error))
	{
		fprintf(stderr, "option parsing failed: %s\n", error->message);
		fputs(g_option_context_get_help(opt_context, TRUE, NULL), stderr);
		exit (1);
	}

	if (!delimiter)
		delimiter = g_strdup(",");
	if (argc != 4) {
		fputs("Wrong number of arguments.\n\n", stderr);
		fputs(g_option_context_get_help(opt_context, TRUE, NULL), stderr);
		exit(1);
	}

	if (!(mdb = mdb_open(argv[1], MDB_WRITABLE))) {
		exit(1);
	}
	
	table = mdb_read_table_by_name(mdb, argv[2], MDB_TABLE);
	if (!table) {
		fprintf(stderr, "Table %s not found in database\n", argv[2]);
		exit(1);
	}
	mdb_read_columns(table);
	mdb_read_indices(table);
	mdb_rewind_table(table);

	/*
	 * open the CSV file and read any header rows
	 */
	in = fopen(argv[3], "r");
	if (!in) {
		fprintf(stderr, "Can not open file %s\n", argv[3]);
		exit(1);
	}
	for (i=0;i<header_rows;i++)
		if (!fgets(line, sizeof(line), in)) {
			fprintf(stderr, "Error while reading header column #%d. Check -H parameter.\n", i);
			exit(1);
		}

	row = 1;
	fields = calloc(table->num_cols, sizeof(MdbField));
	while (fgets(line, sizeof(line), in)) {
		num_fields = prep_row(table, line, fields, delimiter);
		if (!num_fields) {
			fprintf(stderr, "Aborting import at row %d\n", row);
			exit(1);
		}
		/*
	 	* all the prep work is done, let's add the row
	 	*/
		mdb_insert_row(table, num_fields, fields);
	}

	mdb_free_tabledef(table);
	fclose(in);
	mdb_close(mdb);

	g_option_context_free(opt_context);
	g_free(delimiter);
	free(fields);
	return 0;
}

