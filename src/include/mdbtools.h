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

#ifndef _mdbtools_h_
#define _mdbtools_h_

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <glib.h>

#define MDB_PGSIZE 2048
#define MDB_MAX_OBJ_NAME 30
#define MDB_CATALOG_PG 18

enum {
	MDB_FORM = 0,
	MDB_TABLE,
	MDB_MACRO,
	MDB_SYSTEM_TABLE,
	MDB_REPORT,
	MDB_QUERY,
	MDB_LINKED_TABLE,
	MDB_MODULE,
	MDB_UNKNOWN_08,
	MDB_UNKNOWN_09,
	MDB_UNKNOWN_0A,
	MDB_UNKNOWN_0B
};

typedef struct {
	int           fd;
	char          *filename;
	guint16       cur_pg;
	guint16       row_num;
	unsigned int  cur_pos;
	unsigned char pg_buf[MDB_PGSIZE];
	GList         *catalog;
} MdbHandle; 

typedef struct {
	MdbHandle	*mdb;
	char           object_name[MDB_MAX_OBJ_NAME+1];
	int            object_type;
	unsigned long  table_pg;
	unsigned long  kkd_pg;
	unsigned int   kkd_rowid;
	int		num_props;
	GArray		*props;
	GArray		*columns;
} MdbCatalogEntry;

typedef struct {
	char		name[MDB_MAX_OBJ_NAME+1];
} MdbColumnProp;

typedef struct {
	char		name[MDB_MAX_OBJ_NAME+1];
	GHashTable	*properties;
} MdbColumn;

/* mem.c */
extern MdbHandle *mdb_alloc_handle();
extern void mdb_free_handle(MdbHandle *mdb);
extern void mdb_free_catalog(MdbHandle *mdb);

extern size_t mdb_read_pg(MdbHandle *mdb, unsigned long pg);
extern int    mdb_get_int16(MdbHandle *mdb, int offset);
extern long   mdb_get_int32(MdbHandle *mdb, int offset);
extern MdbHandle *mdb_open(char *filename);
extern void   mdb_catalog_dump(MdbHandle *mdb, int obj_type);
extern int mdb_catalog_rows(MdbHandle *mdb);
extern MdbCatalogEntry *mdb_get_catalog_entry(MdbHandle *mdb, int rowid, MdbCatalogEntry *entry);
#endif /* _mdbtools_h_ */
