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

GHashTable *mdb_backends;

   /*    Access data types */
MdbBackendType mdb_access_types[] = {
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
MdbBackendType mdb_oracle_types[] = {
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
MdbBackendType mdb_sybase_types[] = {
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
MdbBackendType mdb_postgres_types[] = {
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
MdbBackendType mdb_mysql_types[] = {
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

char *bound_values[MDB_MAX_COLS];
char *relationships[4];
MdbColumn *col;
MdbCatalogEntry *entry;
MdbTableDef *table;
int did_first;

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

/*
** mdb_init_backends() initializes the mdb_backends hash and loads the builtin
** backends
*/
void mdb_init_backends()
{
MdbBackend *backend;

	mdb_backends = g_hash_table_new(g_str_hash, g_str_equal);

	backend = (MdbBackend *) g_malloc0(sizeof(MdbBackend));
	backend->types_table = mdb_access_types;
	mdb_register_backend(backend, "access");

	backend = (MdbBackend *) g_malloc0(sizeof(MdbBackend));
	backend->types_table = mdb_sybase_types;
	mdb_register_backend(backend, "sybase");

	backend = (MdbBackend *) g_malloc0(sizeof(MdbBackend));
	backend->types_table = mdb_oracle_types;
	mdb_register_backend(backend, "oracle");

	backend = (MdbBackend *) g_malloc0(sizeof(MdbBackend));
	backend->types_table = mdb_postgres_types;
	mdb_register_backend(backend, "postgres");

	backend = (MdbBackend *) g_malloc0(sizeof(MdbBackend));
	backend->types_table = mdb_mysql_types;
	mdb_register_backend(backend, "mysql");
}
void mdb_register_backend(MdbBackend *backend, char *backend_name)
{
	g_hash_table_insert(mdb_backends,backend_name,backend);
}
static gboolean mdb_drop_backend(gpointer key, gpointer value, gpointer data)
{
	MdbBackend *backend = (MdbBackend *)value;
	g_free (backend);
	return TRUE;
}
void mdb_remove_backends()
{
	g_hash_table_foreach_remove(mdb_backends, mdb_drop_backend, NULL);
	g_hash_table_destroy(mdb_backends);
}
int mdb_set_default_backend(MdbHandle *mdb, char *backend_name)
{
MdbBackend *backend;

	backend = (MdbBackend *) g_hash_table_lookup(mdb_backends, backend_name);
	if (backend) {
		mdb->default_backend = backend;
		mdb->backend_name = (char *) malloc(strlen(backend_name)+1);
		strcpy(mdb->backend_name, backend_name);
		did_first = 0;
		return 1;
	} else {
		return 0;
	}
}

static void 
do_first (MdbHandle *mdb) 
{
	int   i, k;

	mdb_read_catalog (mdb, MDB_TABLE);

    /* loop over each entry in the catalog */
    for (i=0; i < mdb->num_catalog; i++) {
      entry = g_ptr_array_index (mdb->catalog, i);
      if ((entry->object_type == MDB_TABLE) &&
            (strncmp (entry->object_name, "MSysRelationships", 17) == 0))
		{
    		table = mdb_read_table (entry);
			if ( table->num_rows > 0 ) {
				mdb_read_columns(table);
				mdb_rewind_table(table);
				for (k=0;k<table->num_cols;k++) {
					bound_values[k] = (char *) malloc(MDB_BIND_SIZE);
					bound_values[k][0] = '\0';
					mdb_bind_column(table,k+1,bound_values[k]);
				}
				relationships[0] = (char *) malloc(256); /* child column */
				relationships[1] = (char *) malloc(256); /* child table */
				relationships[2] = (char *) malloc(256); /* parent column */
				relationships[3] = (char *) malloc(256); /* parent table */
			} /* if num_rows > 0 */
			did_first = 1;
			return;
		} /* if MSysRelationships */
	} /* for */
}

char *mdb_get_relationships(MdbHandle *mdb) {

int   k;
static char text[255];

/*
 * generate relationships by "reading" the MSysRelationships table
 *   szColumn contains the column name of the child table
 *   szObject contains the table name of the child table
 *   szReferencedColumn contains the column name of the parent table
 *   szReferencedObject contains the table name of the parent table
 */
  sprintf(text,"%c",0);
  if ( did_first == 0)
    do_first(mdb);
  if (table->cur_row < table->num_rows) {
    if (mdb_fetch_row(table)) {
       relationships[0][0] = '\0';
       relationships[1][0] = '\0';
       relationships[2][0] = '\0';
       relationships[3][0] = '\0';
       for (k=0;k<table->num_cols;k++) {
          col=g_ptr_array_index(table->columns,k);
          if (strncmp(col->name,"szColumn",8) == 0)
             strcpy(relationships[0],bound_values[k]);
          else if (strncmp(col->name,"szObject",8) == 0)
             strcpy(relationships[1],bound_values[k]);
          else if (strncmp(col->name,"szReferencedColumn",18) == 0)
             strcpy(relationships[2],bound_values[k]);
          else if (strncmp(col->name,"szReferencedObject",18) == 0)
             strcpy(relationships[3],bound_values[k]);
       }
		if (strncmp(mdb->backend_name,"oracle",6) == 0) {
			sprintf(text,"alter table %s add constraint %s_%s foreign key (%s) \
				references %s(%s)",
				relationships[1],relationships[3],relationships[1],
				relationships[0],relationships[3],relationships[2]);
		} else {
			sprintf(text,"-- relationships are not supported for %s",
				mdb->backend_name);
		} /* else */
    } /* got a row */
  } else {
    for (k=0;k<table->num_cols;k++) {
       free(bound_values[k]);
    }
    free(relationships[0]);
    free(relationships[1]);
    free(relationships[2]);
    free(relationships[3]);
    did_first = 0;
  }
  return text;
}


