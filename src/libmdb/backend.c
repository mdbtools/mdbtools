/* MDB Tools - A library for reading MS Access database files
 * Copyright (C) 2000-2011 Brian Bruns and others
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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*
** functions to deal with different backend database engines
*/

#include "mdbtools.h"

/*    Access data types */
static const MdbBackendType mdb_access_types[] = {
    { .name = "Unknown 0x00" },
    { .name = "Boolean" },
    { .name = "Byte" },
    { .name = "Integer" },
    { .name = "Long Integer" },
    { .name = "Currency" },
    { .name = "Single" },
    { .name = "Double" },
    { .name = "DateTime", .needs_quotes = 1 },
    { .name = "Binary" },
    { .name = "Text", .needs_length = 1, .needs_quotes = 1 },
    { .name = "OLE", .needs_length = 1, .needs_quotes = 1 },
    { .name = "Memo/Hyperlink", .needs_length = 1, .needs_quotes = 1 },
    { .name = "Unknown 0x0d" },
    { .name = "Unknown 0x0e" },
    { .name = "Replication ID" },
    { .name = "Numeric", .needs_length = 1, .needs_scale = 1 },
};

/*    Oracle data types */
static const MdbBackendType mdb_oracle_types[] = {
    { .name = "Oracle_Unknown 0x00" },
    { .name = "NUMBER(1)" },
    { .name = "NUMBER(3)" },
    { .name = "NUMBER(5)" },
    { .name = "NUMBER(11)" },
    { .name = "NUMBER(15,2)" },
    { .name = "FLOAT" },
    { .name = "FLOAT" },
    { .name = "TIMESTAMP" },
    { .name = "BINARY" },
    { .name = "VARCHAR2", .needs_length = 1, .needs_quotes = 1 },
    { .name = "BLOB" },
    { .name = "CLOB" },
    { .name = "Oracle_Unknown 0x0d" },
    { .name = "Oracle_Unknown 0x0e" },
    { .name = "NUMBER", .needs_length = 1 },
    { .name = "NUMBER", .needs_length = 1 },
};
static const MdbBackendType mdb_oracle_shortdate_type =
		    { .name = "DATE" };

/*    Sybase/MSSQL data types */
static const MdbBackendType mdb_sybase_types[] = {
    { .name = "Sybase_Unknown 0x00" },
    { .name = "bit" },
    { .name = "char", .needs_length = 1, .needs_quotes = 1 },
    { .name = "smallint" },
    { .name = "int" },
    { .name = "money" },
    { .name = "real" },
    { .name = "float" },
    { .name = "smalldatetime" },
    { .name = "Sybase_Unknown 0x09" },
    { .name = "varchar", .needs_length = 1, .needs_quotes = 1 },
    { .name = "varbinary", .needs_length = 1, .needs_quotes = 1 },
    { .name = "text", .needs_length = 1, .needs_quotes = 1 },
    { .name = "Sybase_Unknown 0x0d" },
    { .name = "Sybase_Unknown 0x0e" },
    { .name = "Sybase_Replication ID" },
    { .name = "numeric", .needs_length = 1, .needs_scale = 1 },
};
static const MdbBackendType mdb_sybase_shortdate_type =
		    { .name = "DATE" };

/*    Postgres data types */
static const MdbBackendType mdb_postgres_types[] = {
    { .name = "Postgres_Unknown 0x00" },
    { .name = "BOOL" },
    { .name = "SMALLINT" },
    { .name = "INTEGER" },
    { .name = "INTEGER" }, /* bigint */
    { .name = "NUMERIC(15,2)" }, /* money deprecated ? */
    { .name = "REAL" },
    { .name = "DOUBLE PRECISION" },
    { .name = "TIMESTAMP WITHOUT TIME ZONE" },
    { .name = "BYTEA" },
    { .name = "VARCHAR", .needs_length = 1, .needs_quotes = 1 },
    { .name = "BYTEA" },
    { .name = "TEXT" },
    { .name = "Postgres_Unknown 0x0d" },
    { .name = "Postgres_Unknown 0x0e" },
    { .name = "UUID" },
    { .name = "NUMERIC", .needs_length = 1, .needs_scale = 1 },
};
static const MdbBackendType mdb_postgres_shortdate_type =
		    { .name = "DATE" };
static const MdbBackendType mdb_postgres_serial_type =
		    { .name = "SERIAL" };

/*    MySQL data types */
static const MdbBackendType mdb_mysql_types[] = {
    { .name = "text", .needs_quotes = 1 },
    { .name = "boolean" },
    { .name = "tinyint" },
    { .name = "smallint" },
    { .name = "int" },
    { .name = "float" },
    { .name = "float" },
    { .name = "float" },
    { .name = "datetime", .needs_quotes = 1 },
    { .name = "varchar", .needs_length = 1, .needs_quotes = 1 },
    { .name = "varchar", .needs_length = 1, .needs_quotes = 1 },
    { .name = "varchar", .needs_length = 1, .needs_quotes = 1 },
    { .name = "text", .needs_quotes = 1 },
    { .name = "blob" },
    { .name = "text", .needs_quotes = 1 },
    { .name = "char(38)" },
    { .name = "numeric", .needs_length = 1, .needs_scale = 1 },
};
static const MdbBackendType mdb_mysql_shortdate_type =
		    { .name = "date" };

/*    sqlite data types */
static const MdbBackendType mdb_sqlite_types[] = {
    { .name = "BLOB" },
    { .name = "INTEGER" },
    { .name = "INTEGER" },
    { .name = "INTEGER" },
    { .name = "INTEGER" },
    { .name = "REAL" },
    { .name = "REAL" },
    { .name = "REAL" },
    { .name = "DateTime", .needs_quotes = 1 },
    { .name = "BLOB", .needs_quotes = 1 },
    { .name = "varchar", .needs_quotes = 1 },
    { .name = "BLOB", .needs_quotes = 1 },
    { .name = "TEXT", .needs_quotes = 1 },
    { .name = "BLOB", .needs_quotes = 1 },
    { .name = "BLOB", .needs_quotes = 1 },
    { .name = "INTEGER" },
    { .name = "INTEGER" },
};

enum {
	MDB_BACKEND_ACCESS = 1,
	MDB_BACKEND_ORACLE,
	MDB_BACKEND_SYBASE,
	MDB_BACKEND_POSTGRES,
	MDB_BACKEND_MYSQL,
	MDB_BACKEND_SQLITE,
};

static void mdb_drop_backend(gpointer key, gpointer value, gpointer data);

static gchar*
quote_generic(const gchar *value, gchar quote_char, gchar escape_char) {
	gchar *result, *pr;
	unsigned char c;

	pr = result = g_malloc(1+4*strlen(value)+2); // worst case scenario

	*pr++ = quote_char;
	while ((c=*(unsigned char*)value++)) {
		if (c<32) {
			sprintf(pr, "\\%03o", c);
			pr+=4;
			continue;
		}
		else if (c == quote_char) {
			*pr++ = escape_char;
		}
		*pr++ = c;
	}
	*pr++ = quote_char;
	*pr++ = '\0';
	return result;
}
static gchar*
quote_schema_name_bracket_merge(const gchar* schema, const gchar *name) {
	if (schema)
		return g_strconcat("[", schema, "_", name, "]", NULL);
	else
		return g_strconcat("[", name, "]", NULL);
}

/*
 * For backends that really does support schema
 * returns "name" or "schema"."name"
 */
static gchar*
quote_schema_name_dquote(const gchar* schema, const gchar *name)
{
	if (schema) {
		gchar *frag1 = quote_generic(schema, '"', '"');
		gchar *frag2 = quote_generic(name, '"', '"');
		gchar *result = g_strconcat(frag1, ".", frag2, NULL);
		g_free(frag1);
		g_free(frag2);
		return result;
	}
	return quote_generic(name, '"', '"');
}

/*
 * For backends that really do NOT support schema
 * returns "name" or "schema_name"
 */
/*
static gchar*
quote_schema_name_dquote_merge(const gchar* schema, const gchar *name)
{
	if (schema) {
		gchar *combined = g_strconcat(schema, "_", name, NULL);
		gchar *result = quote_generic(combined, '"', '"');
		g_free(combined);
		return result;
	}
	return quote_generic(name, '"', '"');
}*/

static gchar*
quote_schema_name_rquotes_merge(const gchar* schema, const gchar *name)
{
	if (schema) {
		gchar *combined = g_strconcat(schema, "_", name, NULL);
		gchar *result = quote_generic(combined, '`', '`');
		g_free(combined);
		return result;
	}
	return quote_generic(name, '`', '`');
}

static gchar*
quote_with_squotes(const gchar* value)
{
	return quote_generic(value, '\'', '\'');
}

static int mdb_col_is_shortdate(const MdbColumn *col) {
    const char *format = mdb_col_get_prop(col, "Format");
    return format && !strcmp(format, "Short Date");
}

const MdbBackendType*
mdb_get_colbacktype(const MdbColumn *col) {
	MdbBackend *backend = col->table->entry->mdb->default_backend;
	int col_type = col->col_type;
	if (col_type > 0x10 )
		return NULL;
	if (col_type == MDB_LONGINT && col->is_long_auto && backend->type_autonum)
		return backend->type_autonum;
	if (col_type == MDB_DATETIME && backend->type_shortdate) {
		if (mdb_col_is_shortdate(col))
			return backend->type_shortdate;
	}
	return &backend->types_table[col_type];
}

const char *
mdb_get_colbacktype_string(const MdbColumn *col)
{
	const MdbBackendType *type = mdb_get_colbacktype(col);
	if (!type) {
   		// return NULL;
		static __thread char buf[16];
		snprintf(buf, sizeof(buf), "type %04x", col->col_type);
		return buf;
	}
	return type->name;
}
int
mdb_colbacktype_takes_length(const MdbColumn *col)
{
	const MdbBackendType *type = mdb_get_colbacktype(col);
	if (!type) return 0;
	return type->needs_length;
}

/**
 * mdb_init_backends
 *
 * Initializes the mdb_backends hash and loads the builtin backends.
 * Use mdb_remove_backends() to destroy this hash when done.
 */
void mdb_init_backends(MdbHandle *mdb)
{
    if (mdb->backends) {
        mdb_remove_backends(mdb);
    }
	mdb->backends = g_hash_table_new(g_str_hash, g_str_equal);

	mdb_register_backend(mdb, "access",
		MDB_SHEXP_DROPTABLE|MDB_SHEXP_CST_NOTNULL|MDB_SHEXP_DEFVALUES,
		mdb_access_types, NULL, NULL,
		"Date()", "Date()",
		"-- That file uses encoding %s\n",
		"DROP TABLE %s;\n",
		NULL,
		NULL,
		NULL,
		quote_schema_name_bracket_merge);
	mdb_register_backend(mdb, "sybase",
		MDB_SHEXP_DROPTABLE|MDB_SHEXP_CST_NOTNULL|MDB_SHEXP_CST_NOTEMPTY|MDB_SHEXP_COMMENTS|MDB_SHEXP_DEFVALUES,
		mdb_sybase_types, &mdb_sybase_shortdate_type, NULL,
		"getdate()", "getdate()",
		"-- That file uses encoding %s\n",
		"DROP TABLE %s;\n",
		"ALTER TABLE %s ADD CHECK (%s <>'');\n",
		"COMMENT ON COLUMN %s.%s IS %s;\n",
		"COMMENT ON TABLE %s IS %s;\n",
		quote_schema_name_dquote);
	mdb_register_backend(mdb, "oracle",
		MDB_SHEXP_DROPTABLE|MDB_SHEXP_CST_NOTNULL|MDB_SHEXP_COMMENTS|MDB_SHEXP_INDEXES|MDB_SHEXP_RELATIONS|MDB_SHEXP_DEFVALUES,
		mdb_oracle_types, &mdb_oracle_shortdate_type, NULL,
		"current_date", "sysdate",
		"-- That file uses encoding %s\n",
		"DROP TABLE %s;\n",
		NULL,
		"COMMENT ON COLUMN %s.%s IS %s;\n",
		"COMMENT ON TABLE %s IS %s;\n",
		quote_schema_name_dquote);
	mdb_register_backend(mdb, "postgres",
		MDB_SHEXP_DROPTABLE|MDB_SHEXP_CST_NOTNULL|MDB_SHEXP_CST_NOTEMPTY|MDB_SHEXP_COMMENTS|MDB_SHEXP_INDEXES|MDB_SHEXP_RELATIONS|MDB_SHEXP_DEFVALUES|MDB_SHEXP_BULK_INSERT,
		mdb_postgres_types, &mdb_postgres_shortdate_type, &mdb_postgres_serial_type,
		"current_date", "now()",
		"SET client_encoding = '%s';\n",
		"DROP TABLE IF EXISTS %s;\n",
		"ALTER TABLE %s ADD CHECK (%s <>'');\n",
		"COMMENT ON COLUMN %s.%s IS %s;\n",
		"COMMENT ON TABLE %s IS %s;\n",
		quote_schema_name_dquote);
	mdb_register_backend(mdb, "mysql",
		MDB_SHEXP_DROPTABLE|MDB_SHEXP_CST_NOTNULL|MDB_SHEXP_CST_NOTEMPTY|MDB_SHEXP_INDEXES|MDB_SHEXP_DEFVALUES|MDB_SHEXP_BULK_INSERT,
		mdb_mysql_types, &mdb_mysql_shortdate_type, NULL,
		"current_date", "now()",
		"-- That file uses encoding %s\n",
		"DROP TABLE IF EXISTS %s;\n",
		"ALTER TABLE %s ADD CHECK (%s <>'');\n",
		NULL,
		NULL,
		quote_schema_name_rquotes_merge);
	mdb_register_backend(mdb, "sqlite",
		MDB_SHEXP_DROPTABLE|MDB_SHEXP_RELATIONS|MDB_SHEXP_DEFVALUES|MDB_SHEXP_BULK_INSERT,
		mdb_sqlite_types, NULL, NULL,
		"date('now')", "date('now')",
		"-- That file uses encoding %s\n",
		"DROP TABLE IF EXISTS %s;\n",
		NULL,
		NULL,
		NULL,
		quote_schema_name_rquotes_merge);
}

void mdb_register_backend(MdbHandle *mdb, char *backend_name, guint32 capabilities,
        const MdbBackendType *backend_type, const MdbBackendType *type_shortdate, const MdbBackendType *type_autonum,
        const char *short_now, const char *long_now,
        const char *charset_statement, const char *drop_statement,
        const char *constaint_not_empty_statement, const char *column_comment_statement,
        const char *table_comment_statement, gchar* (*quote_schema_name)(const gchar*, const gchar*))
{
	MdbBackend *backend = (MdbBackend *) g_malloc0(sizeof(MdbBackend));
	backend->capabilities = capabilities;
	backend->types_table = backend_type;
	backend->type_shortdate = type_shortdate;
	backend->type_autonum = type_autonum;
	backend->short_now = short_now;
	backend->long_now = long_now;
	backend->charset_statement = charset_statement;
	backend->drop_statement = drop_statement;
	backend->constaint_not_empty_statement = constaint_not_empty_statement;
	backend->column_comment_statement = column_comment_statement;
	backend->table_comment_statement = table_comment_statement;
	backend->quote_schema_name  = quote_schema_name;
	g_hash_table_insert(mdb->backends, backend_name, backend);
}

/**
 * mdb_remove_backends
 *
 * Removes all entries from and destroys the mdb_backends hash.
 */
void
mdb_remove_backends(MdbHandle *mdb)
{
	g_hash_table_foreach(mdb->backends, mdb_drop_backend, NULL);
	g_hash_table_destroy(mdb->backends);
}
static void mdb_drop_backend(gpointer key, gpointer value, gpointer data)
{
	MdbBackend *backend = (MdbBackend *)value;
	g_free (backend);
}

/**
 * mdb_set_default_backend
 * @mdb: Handle to open MDB database file
 * @backend_name: Name of the backend to set as default
 *
 * Sets the default backend of the handle @mdb to @backend_name.
 *
 * Returns: 1 if successful, 0 if unsuccessful.
 */
int mdb_set_default_backend(MdbHandle *mdb, const char *backend_name)
{
	MdbBackend *backend;

    if (!mdb->backends) {
        mdb_init_backends(mdb);
    }
	backend = (MdbBackend *) g_hash_table_lookup(mdb->backends, backend_name);
	if (backend) {
		mdb->default_backend = backend;
		g_free(mdb->backend_name); // NULL is ok
		mdb->backend_name = (char *) g_strdup(backend_name);
		mdb->relationships_table = NULL;
		return 1;
	} else {
		return 0;
	}
}


/**
 * Generates index name based on backend.
 *
 * You should free() the returned value once you are done with it.
 *
 * @param backend backend we are generating indexes for
 * @param table table being processed
 * @param idx index being processed
 * @return the index name
 */
static char *
mdb_get_index_name(int backend, MdbTableDef *table, MdbIndex *idx)
{
	char *index_name;

	switch(backend){
		case MDB_BACKEND_MYSQL:
			// appending table name to index often makes it too long for mysql
			index_name = malloc(strlen(idx->name)+5+1);
			if (idx->index_type==1)
				// for mysql name of primary key is not used
				strcpy(index_name, "_pkey");
			else {
				strcpy(index_name, idx->name);
			}
			break;
		default:
			index_name = malloc(strlen(table->name)+strlen(idx->name)+5+1);
			strcpy(index_name, table->name);
			if (idx->index_type==1)
				strcat(index_name, "_pkey");
			else {
				strcat(index_name, "_");
				strcat(index_name, idx->name);
				strcat(index_name, "_idx");
			}
	}

	return index_name;
}

/**
 * mdb_print_indexes
 * @output: Where to print the sql
 * @table: Table to process
 * @dbnamespace: Target namespace/schema name
 */
static void
mdb_print_indexes(FILE* outfile, MdbTableDef *table, char *dbnamespace)
{
	unsigned int i, j;
	char* quoted_table_name;
	char* index_name;
	char* quoted_name;
	int backend;
	MdbHandle* mdb = table->entry->mdb;
	MdbIndex *idx;
	MdbColumn *col;


	if (!strcmp(mdb->backend_name, "postgres")) {
		backend = MDB_BACKEND_POSTGRES;
	} else if (!strcmp(mdb->backend_name, "mysql")) {
		backend = MDB_BACKEND_MYSQL;
	} else if (!strcmp(mdb->backend_name, "oracle")) {
		backend = MDB_BACKEND_ORACLE;
	} else {
		fprintf(outfile, "-- Indexes are not implemented for %s\n\n", mdb->backend_name);
		return;
	}

	/* read indexes */
	mdb_read_indices(table);

	fprintf (outfile, "-- CREATE INDEXES ...\n");

	quoted_table_name = mdb->default_backend->quote_schema_name(dbnamespace, table->name);

	for (i=0;i<table->num_idxs;i++) {
		idx = g_ptr_array_index (table->indices, i);
		if (idx->index_type==2)
			continue;

		index_name = mdb_get_index_name(backend, table, idx);
		quoted_name = mdb->default_backend->quote_schema_name(dbnamespace, index_name);
		if (idx->index_type==1) {
			switch (backend) {
				case MDB_BACKEND_ORACLE:
				case MDB_BACKEND_POSTGRES:
					fprintf (outfile, "ALTER TABLE %s ADD CONSTRAINT %s PRIMARY KEY (", quoted_table_name, quoted_name);
					break;
				case MDB_BACKEND_MYSQL:
					fprintf (outfile, "ALTER TABLE %s ADD PRIMARY KEY (", quoted_table_name);
					break;
			}
		} else {
			switch (backend) {
				case MDB_BACKEND_ORACLE:
				case MDB_BACKEND_POSTGRES:
					fprintf(outfile, "CREATE");
					if (idx->flags & MDB_IDX_UNIQUE)
						fprintf (outfile, " UNIQUE");
					fprintf(outfile, " INDEX %s ON %s (", quoted_name, quoted_table_name);
					break;
				case MDB_BACKEND_MYSQL:
					fprintf(outfile, "ALTER TABLE %s ADD", quoted_table_name);
					if (idx->flags & MDB_IDX_UNIQUE)
						fprintf (outfile, " UNIQUE");
					fprintf(outfile, " INDEX %s (", quoted_name);
					break;
			}
		}
		g_free(quoted_name);
		free(index_name);

		for (j=0;j<idx->num_keys;j++) {
			if (j)
				fprintf(outfile, ", ");
			col=g_ptr_array_index(table->columns,idx->key_col_num[j]-1);
			quoted_name = mdb->default_backend->quote_schema_name(NULL, col->name);
			fprintf (outfile, "%s", quoted_name);
			if (idx->index_type!=1 && idx->key_col_order[j])
				/* no DESC for primary keys */
				fprintf(outfile, " DESC");

			g_free(quoted_name);

		}
		fprintf (outfile, ");\n");
	}
	fputc ('\n', outfile);

	g_free(quoted_table_name);
}

/**
 * mdb_get_relationships
 * @mdb: Handle to open MDB database file
 * @tablename: Name of the table to process. Process all tables if NULL.
 *
 * Generates relationships by reading the MSysRelationships table.
 *   'szColumn' contains the column name of the child table.
 *   'szObject' contains the table name of the child table.
 *   'szReferencedColumn' contains the column name of the parent table.
 *   'szReferencedObject' contains the table name of the parent table.
 *   'grbit' contains integrity constraints.
 *
 * Returns: a string stating that relationships are not supported for the
 *   selected backend, or a string containing SQL commands for setting up
 *   the relationship, tailored for the selected backend.
 *   Returns NULL on last iteration.
 *   The caller is responsible for freeing this string.
 */
static char *
mdb_get_relationships(MdbHandle *mdb, const gchar *dbnamespace, const char* tablename)
{
	unsigned int i;
	gchar *text = NULL;  /* String to be returned */
	char **bound = mdb->relationships_values;  /* Bound values */
	int backend = 0;
	char *quoted_table_1, *quoted_column_1,
	     *quoted_table_2, *quoted_column_2,
	     *constraint_name, *quoted_constraint_name;
	long grbit;

	if (!strcmp(mdb->backend_name, "oracle")) {
		backend = MDB_BACKEND_ORACLE;
	} else if (!strcmp(mdb->backend_name, "postgres")) {
		backend = MDB_BACKEND_POSTGRES;
	} else if (!strcmp(mdb->backend_name, "sqlite")) {
		backend = MDB_BACKEND_SQLITE;
	} else if (!mdb->relationships_table) {
        return (char *) g_strconcat(
                "-- relationships are not implemented for ",
                mdb->backend_name, "\n", NULL);
	}

	if (!mdb->relationships_table) {
		mdb->relationships_table = mdb_read_table_by_name(mdb, "MSysRelationships", MDB_TABLE);
		if (!mdb->relationships_table || !mdb->relationships_table->num_rows) {
			fprintf(stderr, "No MSysRelationships\n");
			return NULL;
		}

		mdb_read_columns(mdb->relationships_table);
		for (i=0;i<5;i++) {
			bound[i] = (char *) g_malloc0(MDB_BIND_SIZE);
		}
		mdb_bind_column_by_name(mdb->relationships_table, "szColumn", bound[0], NULL);
		mdb_bind_column_by_name(mdb->relationships_table, "szObject", bound[1], NULL);
		mdb_bind_column_by_name(mdb->relationships_table, "szReferencedColumn", bound[2], NULL);
		mdb_bind_column_by_name(mdb->relationships_table, "szReferencedObject", bound[3], NULL);
		mdb_bind_column_by_name(mdb->relationships_table, "grbit", bound[4], NULL);
		mdb_rewind_table(mdb->relationships_table);
	}
    if (mdb->relationships_table->cur_row >= mdb->relationships_table->num_rows) {  /* past the last row */
        for (i=0;i<5;i++)
            g_free(bound[i]);
        mdb->relationships_table = NULL;
        return NULL;
    }

	while (1) {
		if (!mdb_fetch_row(mdb->relationships_table)) {
			for (i=0;i<5;i++)
				g_free(bound[i]);
			mdb->relationships_table = NULL;
			return NULL;
		}
		if (!tablename || !strcmp(bound[1], tablename))
			break;
	}

	quoted_table_1 = mdb->default_backend->quote_schema_name(dbnamespace, bound[1]);
	quoted_column_1 = mdb->default_backend->quote_schema_name(dbnamespace, bound[0]);
	quoted_table_2 = mdb->default_backend->quote_schema_name(dbnamespace, bound[3]);
	quoted_column_2 = mdb->default_backend->quote_schema_name(dbnamespace, bound[2]);
	grbit = atoi(bound[4]);
	constraint_name = g_strconcat(bound[1], "_", bound[0], "_fk", NULL);
	quoted_constraint_name = mdb->default_backend->quote_schema_name(dbnamespace, constraint_name);
	g_free(constraint_name);

	if (grbit & 0x00000002) {
		text = g_strconcat(
			"-- Relationship from ", quoted_table_1,
			" (", quoted_column_1, ")"
			" to ", quoted_table_2, "(", quoted_column_2, ")",
			" does not enforce integrity.\n", NULL);
	} else {
		switch (backend) {
		  case MDB_BACKEND_ORACLE:
                        text = g_strconcat(
                                "ALTER TABLE ", quoted_table_1,
                                " ADD CONSTRAINT ", quoted_constraint_name,
                                " FOREIGN KEY (", quoted_column_1, ")"
                                " REFERENCES ", quoted_table_2, "(", quoted_column_2, ")",
                                (grbit & 0x00001000) ? " ON DELETE CASCADE" : "",
                                ";\n", NULL);

                        break;
		  case MDB_BACKEND_POSTGRES:
		  case MDB_BACKEND_SQLITE:
			text = g_strconcat(
				"ALTER TABLE ", quoted_table_1,
				" ADD CONSTRAINT ", quoted_constraint_name,
				" FOREIGN KEY (", quoted_column_1, ")"
				" REFERENCES ", quoted_table_2, "(", quoted_column_2, ")",
				(grbit & 0x00000100) ? " ON UPDATE CASCADE" : "",
				(grbit & 0x00001000) ? " ON DELETE CASCADE" : "",
				";\n", NULL);

			break;
		}
	}
	g_free(quoted_table_1);
	g_free(quoted_column_1);
	g_free(quoted_table_2);
	g_free(quoted_column_2);
	g_free(quoted_constraint_name);

	return (char *)text;
}

static void
generate_table_schema(FILE *outfile, MdbCatalogEntry *entry, char *dbnamespace, guint32 export_options)
{
	MdbTableDef *table;
	MdbHandle *mdb = entry->mdb;
	MdbColumn *col;
	unsigned int i;
	char* quoted_table_name;
	char* quoted_name;
	MdbProperties *props;
	const char *prop_value;

	quoted_table_name = mdb->default_backend->quote_schema_name(dbnamespace, entry->object_name);

	/* drop the table if it exists */
	if (export_options & MDB_SHEXP_DROPTABLE)
		fprintf (outfile, mdb->default_backend->drop_statement, quoted_table_name);

	/* create the table */
	fprintf (outfile, "CREATE TABLE %s\n", quoted_table_name);
	fprintf (outfile, " (\n");

	table = mdb_read_table (entry);

	/* get the columns */
	mdb_read_columns (table);

	/* loop over the columns, dumping the names and types */
	for (i = 0; i < table->num_cols; i++) {
		col = g_ptr_array_index (table->columns, i);

		quoted_name = mdb->default_backend->quote_schema_name(NULL, col->name);
		fprintf (outfile, "\t%s\t\t\t%s", quoted_name,
			mdb_get_colbacktype_string (col));
		g_free(quoted_name);

		if (mdb_colbacktype_takes_length(col)) {

			/* more portable version from DW patch */
			if (col->col_size == 0)
				fputs(" (255)", outfile);
			else if (col->col_scale != 0)
				fprintf(outfile, " (%d, %d)", col->col_scale, col->col_prec);
			else
				fprintf(outfile, " (%d)", col->col_size);
		}

		if (export_options & MDB_SHEXP_CST_NOTNULL) {
			if (col->col_type == MDB_BOOL) {
				/* access booleans are never null */
				fputs(" NOT NULL", outfile);
			} else {
				const gchar *not_null = mdb_col_get_prop(col, "Required");
				if (not_null && not_null[0]=='y')
					fputs(" NOT NULL", outfile);
			}
		}

		if (export_options & MDB_SHEXP_DEFVALUES) {
			int done = 0;
			if (col->props) {
				gchar *defval = g_hash_table_lookup(col->props->hash, "DefaultValue");
				if (defval) {
					size_t def_len = strlen(defval);
					fputs(" DEFAULT ", outfile);
					/* ugly hack to detect the type */
					if (defval[0]=='"' && defval[def_len-1]=='"') {
						/* this is a string */
						gchar *output_default = malloc(def_len-1);
						gchar *output_default_escaped;
						memcpy(output_default, defval+1, def_len-2);
						output_default[def_len-2] = 0;
						output_default_escaped = quote_with_squotes(output_default);
						fputs(output_default_escaped, outfile);
						g_free(output_default_escaped);
						free(output_default);
					} else if (!strcmp(defval, "Yes"))
						fputs("TRUE", outfile);
					else if (!strcmp(defval, "No"))
						fputs("FALSE", outfile);
					else if (!g_ascii_strcasecmp(defval, "date()")) {
						if (mdb_col_is_shortdate(col))
							fputs(mdb->default_backend->short_now, outfile);
						else
							fputs(mdb->default_backend->long_now, outfile);
					}
					else
						fputs(defval, outfile);
					done = 1;
				}
			}
			if (!done && col->col_type == MDB_BOOL)
				/* access booleans are false by default */
				fputs(" DEFAULT FALSE", outfile);
		}
		if (i < table->num_cols - 1)
			fputs(", \n", outfile);
		else
			fputs("\n", outfile);
	} /* for */

	fputs(");\n", outfile);

	/* Add the constraints on columns */
	for (i = 0; i < table->num_cols; i++) {
		col = g_ptr_array_index (table->columns, i);
		props = col->props;
		if (!props)
			continue;

		quoted_name = mdb->default_backend->quote_schema_name(NULL, col->name);

		if (export_options & MDB_SHEXP_CST_NOTEMPTY) {
			prop_value = mdb_col_get_prop(col, "AllowZeroLength");
			if (prop_value && prop_value[0]=='n')
					fprintf(outfile,
						mdb->default_backend->constaint_not_empty_statement,
						quoted_table_name, quoted_name);
		}

		if (export_options & MDB_SHEXP_COMMENTS) {
			prop_value = mdb_col_get_prop(col, "Description");
			if (prop_value) {
				char *comment = quote_with_squotes(prop_value);
				fprintf(outfile,
					mdb->default_backend->column_comment_statement,
					quoted_table_name, quoted_name, comment);
				g_free(comment);
			}
		}

		g_free(quoted_name);
	}

	/* Add the comments on table */
	if (export_options & MDB_SHEXP_COMMENTS) {
		prop_value = mdb_table_get_prop(table, "Description");
		if (prop_value) {
			char *comment = quote_with_squotes(prop_value);
			fprintf(outfile,
				mdb->default_backend->table_comment_statement,
				quoted_table_name, comment);
			g_free(comment);
		}
	}
	fputc('\n', outfile);


	if (export_options & MDB_SHEXP_INDEXES)
		// prints all the indexes of that table
		mdb_print_indexes(outfile, table, dbnamespace);

	g_free(quoted_table_name);

	mdb_free_tabledef (table);
}


void
mdb_print_schema(MdbHandle *mdb, FILE *outfile, char *tabname, char *dbnamespace, guint32 export_options)
{
	unsigned int   i;
	char		*the_relation;
	MdbCatalogEntry *entry;
	const char *charset;

	/* clear unsupported options */
	export_options &= mdb->default_backend->capabilities;

	/* Print out a little message to show that this came from mdb-tools.
	   I like to know how something is generated. DW */
	fputs("-- ----------------------------------------------------------\n"
		"-- MDB Tools - A library for reading MS Access database files\n"
		"-- Copyright (C) 2000-2011 Brian Bruns and others.\n"
		"-- Files in libmdb are licensed under LGPL and the utilities under\n"
		"-- the GPL, see COPYING.LIB and COPYING files respectively.\n"
		"-- Check out http://mdbtools.sourceforge.net\n"
		"-- ----------------------------------------------------------\n\n",
		outfile);

	charset = mdb_target_charset(mdb);
	if (charset) {
		fprintf(outfile, mdb->default_backend->charset_statement, charset);
		fputc('\n', outfile);
	}

	for (i=0; i < mdb->num_catalog; i++) {
		entry = g_ptr_array_index (mdb->catalog, i);
		if (entry->object_type == MDB_TABLE) {
			if ((tabname && !strcmp(entry->object_name, tabname))
			 || (!tabname && mdb_is_user_table(entry))) {
				generate_table_schema(outfile, entry, dbnamespace, export_options);
			}
		}
	}
	fprintf (outfile, "\n");

	if (export_options & MDB_SHEXP_RELATIONS) {
		fputs ("-- CREATE Relationships ...\n", outfile);
		while ((the_relation=mdb_get_relationships(mdb, dbnamespace, tabname)) != NULL) {
			fputs(the_relation, outfile);
			g_free(the_relation);
		}
	}
}
