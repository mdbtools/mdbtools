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
  	"Replication ID"};
   /*    Oracle data types */

char *mdb_oracle_types[] = 
        {"Oracle_Unknown 0x00",
         "CHAR",
         "NUMBER",
         "NUMBER",
         "NUMBER",
         "NUMBER",
         "NUMBER",
         "NUMBER",
         "DATE",
         "Oracle_Unknown 0x09",
         "VARCHAR2",
         "BLOB",
         "BLOB",
         "Oracle_Unknown 0x0d",
         "Oracle_Unknown 0x0e",
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
  	"Sybase_Replication ID"};

char *mdb_get_coltype_string(MdbBackend *backend, int col_type)
{
        if (col_type > 0x0f) {
                return NULL;
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
		return 1;
	} else {
		return 0;
	}
}
