/* MDB Tools - A library for reading MS Access database files
 * Copyright (C) 2000 Brian Bruns
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

/*
** functions to deal with different backend database engines
*/

#include "mdbtools.h"

#ifdef DMALLOC
#include "dmalloc.h"
#endif

static int is_init;
static GHashTable *mdb_backends;

   /*    Access data types */
static MdbBackendType mdb_access_types[] = {
		{"Unknown 0x00", 0,0,0 },
		{"Boolean", 0,0,0},
		{"Byte", 0,0,0},
		{"Integer", 0,0,0},
		{"Long Integer", 0,0,0},
		{"Currency", 0,0,0},
		{"Single", 0,0,0},
		{"Double", 0,0,0},
		{"DateTime (Short)", 0,0,1},
		{"Unknown 0x09", 0,0,0},
		{"Text", 1,0,1},
		{"OLE", 1,0,1},
		{"Memo/Hyperlink",1,0,1},
		{"Unknown 0x0d",0,0,0},
		{"Unknown 0x0e",0,0,0},
		{"Replication ID",0,0,0},
		{"Numeric",1,1,0}
};

/*    Oracle data types */
static MdbBackendType mdb_oracle_types[] = {
		{"Oracle_Unknown 0x00",0,0,0},
		{"NUMBER",1,0,0},
		{"NUMBER",1,0,0},
		{"NUMBER",1,0,0},
		{"NUMBER",1,0,0},
		{"NUMBER",1,0,0},
		{"FLOAT",0,0,0},
		{"FLOAT",0,0,0},
		{"DATE",0,0,0},
		{"Oracle_Unknown 0x09",0,0,0},
		{"VARCHAR2",1,0,1},
		{"BLOB",1,0,1},
		{"CLOB",1,0,1},
		{"Oracle_Unknown 0x0d",0,0,0},
		{"Oracle_Unknown 0x0e",0,0,0},
		{"NUMBER",1,0,0},
		{"NUMBER",1,0,0},
};

/*    Sybase/MSSQL data types */
static MdbBackendType mdb_sybase_types[] = {
		{"Sybase_Unknown 0x00",0,0,0},
		{"bit",0,0,0},
		{"char",1,0,1},
		{"smallint",0,0,0},
		{"int",0,0,0},
		{"money",0,0,0},
		{"real",0,0,0},
		{"float",0,0,0},
		{"smalldatetime",0,0,0},
		{"Sybase_Unknown 0x09",0,0,0},
		{"varchar",1,0,1},
		{"varbinary",1,0,1},
		{"text",1,0,1},
		{"Sybase_Unknown 0x0d",0,0,0},
		{"Sybase_Unknown 0x0e",0,0,0},
		{"Sybase_Replication ID",0,0,0},
		{"numeric",1,1,0},
};

/*    Postgres data types */
static MdbBackendType mdb_postgres_types[] = {
		{"Postgres_Unknown 0x00",0,0,0},
		{"Bool",0,0,0},
		{"Int2",0,0,0},
		{"Int4",0,0,0},
		{"Int8",0,0,0},
		{"Money",0,0,0},
		{"Float4",0,0,0},
		{"Float8",0,0,0},
		{"Timestamp",0,0,0},
		{"Postgres_Unknown 0x09",0,0,0},
		{"Char",1,0,1},
		{"Postgres_Unknown 0x0b",0,0,0},
		{"Postgres_Unknown 0x0c",0,0,0},
		{"Postgres_Unknown 0x0d",0,0,0},
		{"Postgres_Unknown 0x0e",0,0,0},
		{"Serial",0,0,0},
		{"Postgres_Unknown 0x10",0,0,0},
};
/*    MySQL data types */
static MdbBackendType mdb_mysql_types[] = {
		{"Text",1,0,1},
		{"char",0,0,0},
		{"int",0,0,0},
		{"int",0,0,0},
		{"int",0,0,0},
		{"float",0,0,0},
		{"float",0,0,0},
		{"float",0,0,0},
		{"date",0,0,1},
		{"varchar",1,0,1},
		{"varchar",1,0,1},
		{"varchar",1,0,1},
		{"text",1,0,1},
		{"blob",0,0,0},
		{"text",1,0,1},
		{"numeric",1,1,0},
		{"numeric",1,1,0},
};

static gboolean mdb_drop_backend(gpointer key, gpointer value, gpointer data);

char *mdb_get_coltype_string(MdbBackend *backend, int col_type)
{
	static char buf[16];

	if (col_type > 0x10 ) {
   		// return NULL;
		snprintf(buf,sizeof(buf), "type %04x", col_type);
		return buf;
	} else {
		return backend->types_table[col_type].name;
	}
}

int mdb_coltype_takes_length(MdbBackend *backend, int col_type)
{
	return backend->types_table[col_type].needs_length;
}

/**
 * mdb_init_backends
 *
 * Initializes the mdb_backends hash and loads the builtin backends.
 * Use mdb_remove_backends() to destroy this hash when done.
 */
void mdb_init_backends()
{
	mdb_backends = g_hash_table_new(g_str_hash, g_str_equal);

	mdb_register_backend(mdb_access_types, "access");
	mdb_register_backend(mdb_sybase_types, "sybase");
	mdb_register_backend(mdb_oracle_types, "oracle");
	mdb_register_backend(mdb_postgres_types, "postgres");
	mdb_register_backend(mdb_mysql_types, "mysql");
}
void mdb_register_backend(MdbBackendType *backend_type, char *backend_name)
{
	MdbBackend *backend = (MdbBackend *) g_malloc0(sizeof(MdbBackend));
	backend->types_table = backend_type;
	g_hash_table_insert(mdb_backends, backend_name, backend);
}

/**
 * mdb_remove_backends
 *
 * Removes all entries from and destroys the mdb_backends hash.
 */
void mdb_remove_backends()
{
	g_hash_table_foreach_remove(mdb_backends, mdb_drop_backend, NULL);
	g_hash_table_destroy(mdb_backends);
}
static gboolean mdb_drop_backend(gpointer key, gpointer value, gpointer data)
{
	MdbBackend *backend = (MdbBackend *)value;
	g_free (backend);
	return TRUE;
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
int mdb_set_default_backend(MdbHandle *mdb, char *backend_name)
{
	MdbBackend *backend;

	backend = (MdbBackend *) g_hash_table_lookup(mdb_backends, backend_name);
	if (backend) {
		mdb->default_backend = backend;
		mdb->backend_name = (char *) g_strdup(backend_name);
		is_init = 0;
		return 1;
	} else {
		return 0;
	}
}

/**
 * mdb_get_relationships
 * @mdb: Handle to open MDB database file
 *
 * Generates relationships by reading the MSysRelationships table.
 *   'szColumn' contains the column name of the child table.
 *   'szObject' contains the table name of the child table.
 *   'szReferencedColumn' contains the column name of the parent table.
 *   'szReferencedObject' contains the table name of the parent table.
 *
 * Returns: a string stating that relationships are not supported for the
 *   selected backend, or a string containing SQL commands for setting up
 *   the relationship, tailored for the selected backend.  The caller is
 *   responsible for freeing this string.
 */
char *mdb_get_relationships(MdbHandle *mdb)
{
	unsigned int i;
	gchar *text = NULL;  /* String to be returned */
	static char *bound[4];  /* Bound values */
	static MdbTableDef *table;  /* Relationships table */
	int backend = 0;  /* Backends: 1=oracle */

	if (strncmp(mdb->backend_name,"oracle",6) == 0) {
		backend = 1;
	} else {
		if (is_init == 0) { /* the first time through */
			is_init = 1;
			return (char *) g_strconcat(
				"-- relationships are not supported for ",
				mdb->backend_name, NULL);
		} else { /* the second time through */
			is_init = 0;
			return NULL;
		}
	}

	if (is_init == 0) {
		table = mdb_read_table_by_name(mdb, "MSysRelationships", MDB_TABLE);
		if ((!table) || (table->num_rows == 0)) {
			return NULL;
		}

		mdb_read_columns(table);
		for (i=0;i<4;i++) {
			bound[i] = (char *) g_malloc0(MDB_BIND_SIZE);
		}
		mdb_bind_column_by_name(table, "szColumn", bound[0]);
		mdb_bind_column_by_name(table, "szObject", bound[1]);
		mdb_bind_column_by_name(table, "szReferencedColumn", bound[2]);
		mdb_bind_column_by_name(table, "szReferencedObject", bound[3]);
		mdb_rewind_table(table);

		is_init = 1;
	}
	else if (table->cur_row >= table->num_rows) {  /* past the last row */
		for (i=0;i<4;i++)
			g_free(bound[i]);
		is_init = 0;
		return NULL;
	}

	if (!mdb_fetch_row(table)) {
		for (i=0;i<4;i++)
			g_free(bound[i]);
		is_init = 0;
		return NULL;
	}

	switch (backend) {
	  case 1:  /* oracle */
		text = g_strconcat("alter table ", bound[1],
			" add constraint ", bound[3], "_", bound[1],
			" foreign key (", bound[0], ")"
			" references ", bound[3], "(", bound[2], ")", NULL);
		break;
	}

	return (char *)text;
}

