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
#define MDB_MAX_COLS 256
#define MDB_CATALOG_PG 18
#define MDB_MEMO_OVERHEAD 12
#define MDB_BIND_SIZE 2048

enum {
	MDB_VER_JET3 = 0,
	MDB_VER_JET4 = 1
};
enum {
	MDB_FORM = 0,
	MDB_TABLE,
	MDB_MACRO,
	MDB_SYSTEM_TABLE,
	MDB_REPORT,
	MDB_QUERY,
	MDB_LINKED_TABLE,
	MDB_MODULE,
	MDB_RELATIONSHIP,
	MDB_UNKNOWN_09,
	MDB_UNKNOWN_0A,
	MDB_DATABASE_PROPERTY
};
enum {
	MDB_BOOL = 0x01,
	MDB_BYTE = 0x02,
	MDB_INT = 0x03,
	MDB_LONGINT = 0x04,
	MDB_MONEY = 0x05,
	MDB_FLOAT = 0x06,
	MDB_DOUBLE = 0x07,
	MDB_SDATETIME = 0x08,
	MDB_TEXT = 0x0a,
	MDB_OLE = 0x0b,
	MDB_MEMO = 0x0c,
	MDB_REPID = 0x0f
};

/* SARG operators */
enum {
	MDB_EQUAL = 1,
	MDB_GT,
	MDB_LT,
	MDB_GTEQ,
	MDB_LTEQ,
	MDB_ISNULL,
	MDB_NOTNULL
};

enum {
	MDB_ASC,
	MDB_DESC
};

/* hash to store registered backends */
GHashTable	*mdb_backends;

typedef struct {
	char **types_table;
} MdbBackend;

typedef struct {
	int           fd;
	char          *filename;
	guint16       cur_pg;
	guint16       row_num;
	unsigned int  cur_pos;
	unsigned char pg_buf[MDB_PGSIZE];
	unsigned char alt_pg_buf[MDB_PGSIZE];
	int		num_catalog;
	GArray		*catalog;
	int		pg_size;
	guint32		jet_version;
	guint32		db_key;
	char		db_passwd[14];
	MdbBackend	*default_backend;
	char			*backend_name;
} MdbHandle; 

typedef struct {
	MdbHandle	*mdb;
	char           object_name[MDB_MAX_OBJ_NAME+1];
	int            object_type;
	unsigned long  table_pg; /* misnomer since object may not be a table */
	unsigned long  kkd_pg;
	unsigned int   kkd_rowid;
	int		num_props;
	GArray		*props;
	GPtrArray	*columns;
} MdbCatalogEntry;

typedef struct {
	MdbCatalogEntry *entry;
	char	name[MDB_MAX_OBJ_NAME+1];
	int	num_cols;
	GPtrArray	*columns;
	int	num_rows;
	int	index_start;
	int	num_real_idxs;
	int	num_idxs;
	GPtrArray	*indices;
	int	first_data_pg;
	int	cur_pg_num;
	int	cur_phys_pg;
	int	cur_row;
} MdbTableDef;

typedef struct {
	int		index_num;
	char		name[MDB_MAX_OBJ_NAME+1];
	unsigned char	primary_key;
	int		first_pg;
	unsigned char	sort_order;
} MdbIndex;

typedef struct {
	char		name[MDB_MAX_OBJ_NAME+1];
} MdbColumnProp;

typedef struct {
	char		name[MDB_MAX_OBJ_NAME+1];
	int		col_type;
	int		col_size;
	void		*bind_ptr;
	GHashTable	*properties;
	int		num_sargs;
	GPtrArray	*sargs;
	unsigned char   is_fixed;
	int		query_order;
} MdbColumn;

typedef union {
	int	i;
	double	d;
	char	s[256];
} MdbAny;

typedef struct {
	int	op;
	MdbAny	value;
} MdbSarg;

/* mem.c */
extern void mdb_init();
extern void mdb_exit();
extern MdbHandle *mdb_alloc_handle();
extern void mdb_free_handle(MdbHandle *mdb);
extern void mdb_free_catalog(MdbHandle *mdb);
extern MdbTableDef *mdb_alloc_tabledef(MdbCatalogEntry *entry);

/* file.c */
extern size_t mdb_read_pg(MdbHandle *mdb, unsigned long pg);
extern size_t mdb_read_alt_pg(MdbHandle *mdb, unsigned long pg);
extern unsigned char mdb_get_byte(MdbHandle *mdb, int offset);
extern int    mdb_get_int16(MdbHandle *mdb, int offset);
extern long   mdb_get_int32(MdbHandle *mdb, int offset);
extern double mdb_get_double(MdbHandle *mdb, int offset);
extern MdbHandle *mdb_open(char *filename);

/* catalog.c */
extern void mdb_catalog_dump(MdbHandle *mdb, int obj_type);
extern int mdb_catalog_rows(MdbHandle *mdb);
extern MdbCatalogEntry *mdb_get_catalog_entry(MdbHandle *mdb, int rowid, MdbCatalogEntry *entry);
extern char *mdb_get_objtype_string(int obj_type);

/* table.c */
extern MdbTableDef *mdb_read_table(MdbCatalogEntry *entry);

/* data.c */
extern void mdb_data_dump(MdbTableDef *table);
extern void mdb_bind_column(MdbTableDef *table, int col_num, void *bind_ptr);
extern int mdb_rewind_table(MdbTableDef *table);
extern int mdb_fetch_row(MdbTableDef *table);
extern int mdb_is_fixed_col(MdbColumn *col);
extern char *mdb_col_to_string(MdbHandle *mdb, int start, int datatype, int size);


/* dump.c */
extern void buffer_dump(const unsigned char* buf, int start, int end);

/* backend.c */
extern char *mdb_get_coltype_string(MdbBackend *backend, int col_type);
extern void mdb_init_backends();
extern void mdb_register_backend(MdbBackend *backend, char *backend_name);
extern int  mdb_set_default_backend(MdbHandle *mdb, char *backend_name);
extern char *mdb_get_relationships(MdbHandle *mdb);

#endif /* _mdbtools_h_ */
