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

   /*    Access data types */
char *mdb_access_types[] = 
	{"Unknown 0x00",
         "Boolean",
         "Byte",
         "Integer",
         "Long Integer",
         "Currency",
         "Single",
         "Double",
         "DateTime (Short)",
         "Unknown 0x09",
         "Text",
         "OLE",
         "Memo/Hyperlink",
         "Unknown 0x0d",
         "Unknown 0x0e",
  	"Replication ID",
	"Numeric"};

/*    Oracle data types */
char *mdb_oracle_types[] = 
        {"Oracle_Unknown 0x00",
         "NUMBER",
         "NUMBER",
         "NUMBER",
         "NUMBER",
         "NUMBER",
         "FLOAT",
         "FLOAT",
         "DATE",
         "Oracle_Unknown 0x09",
         "VARCHAR2",
         "BLOB",
         "CLOB",
         "Oracle_Unknown 0x0d",
         "Oracle_Unknown 0x0e",
  "NUMBER",
  "NUMBER"};

/*    Sybase/MSSQL data types */
char *mdb_sybase_types[] = 
        {"Sybase_Unknown 0x00",
         "bit",
         "char",
         "smallint",
         "int",
         "money",
         "real",
         "float",
         "smalldatetime",
         "Sybase_Unknown 0x09",
         "varchar",
         "varbinary",
         "text",
         "Sybase_Unknown 0x0d",
         "Sybase_Unknown 0x0e",
  	"Sybase_Replication ID",
	"numeric"};

/*    Postgres data types */
char *mdb_postgres_types[] =
 {"Postgres_Unknown 0x00",
         "Bool",
         "Int2",
         "Int4",
         "Int8",
         "Money",
         "Float4",
         "Float8",
         "Timestamp",
         "Postgres_Unknown 0x09",
         "Char",
         "Postgres_Unknown 0x0b",
         "Postgres_Unknown 0x0c",
         "Postgres_Unknown 0x0d",
         "Postgres_Unknown 0x0e",
    "Serial",
	 "Postgres_Unknown 0x10"};

char *bound_values[MDB_MAX_COLS];
char *relationships[4];
MdbColumn *col;
MdbCatalogEntry *entry;
MdbTableDef *table;
int did_first;

char *mdb_get_coltype_string(MdbBackend *backend, int col_type)
{
char buf[100];
        if (col_type > 0x10) {
                // return NULL;
					sprintf(buf,"type %04x", col_type);
                return buf;
        } else {
                return backend->types_table[col_type];
        }
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
}
void mdb_register_backend(MdbBackend *backend, char *backend_name)
{
	g_hash_table_insert(mdb_backends,backend_name,backend);
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

static void do_first (MdbHandle *mdb) 
{
int   i, j, k;
static char text[255];
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
			sprintf(text,"relationships are not supported for %s",
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


