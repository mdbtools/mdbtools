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
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <glib.h>

#define MDB_DEBUG 0

#define MDB_PGSIZE 4096
#define MDB_MAX_OBJ_NAME 30
#define MDB_MAX_COLS 256
#define MDB_MAX_IDX_COLS 10
#define MDB_CATALOG_PG 18
#define MDB_MEMO_OVERHEAD 12
#define MDB_BIND_SIZE 16384

enum {
	MDB_PAGE_DB = 0,
	MDB_PAGE_DATA,
	MDB_PAGE_TABLE,
	MDB_PAGE_INDEX,
	MDB_PAGE_LEAF,
	MDB_PAGE_MAP
};
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
	MDB_DATABASE_PROPERTY,
	MDB_ANY = -1
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
	MDB_REPID = 0x0f,
	MDB_NUMERIC = 0x10
};

/* SARG operators */
enum {
	MDB_EQUAL = 1,
	MDB_GT,
	MDB_LT,
	MDB_GTEQ,
	MDB_LTEQ,
	MDB_LIKE,
	MDB_ISNULL,
	MDB_NOTNULL
};

enum {
	MDB_ASC,
	MDB_DESC
};

enum {
	MDB_IDX_UNIQUE = 0x01,
	MDB_IDX_IGNORENULLS = 0x02,
	MDB_IDX_REQUIRED = 0x08 
};

#define IS_JET4(mdb) (mdb->f->jet_version==MDB_VER_JET4)
#define IS_JET3(mdb) (mdb->f->jet_version==MDB_VER_JET3)

/* hash to store registered backends */
GHashTable	*mdb_backends;

typedef struct {
	char **types_table;
} MdbBackend;

typedef struct {
	gboolean collect;
	unsigned long pg_reads;
} MdbStatistics;

typedef struct {
	int           fd;
	gboolean      writable;
	char          *filename;
	guint32		jet_version;
	guint32		db_key;
	char		db_passwd[14];
	MdbBackend	*default_backend;
	char			*backend_name;
	MdbStatistics	*stats;
	/* free map */
	int  map_sz;
	unsigned char *free_map;
	/* reference count */
	int refs;
} MdbFile; 

/* offset to row count on data pages...version dependant */
typedef struct {
	int		pg_size;
	guint16		row_count_offset; 
	guint16		tab_num_rows_offset;
	guint16		tab_num_cols_offset;
	guint16		tab_num_idxs_offset;
	guint16		tab_num_ridxs_offset;
	guint16		tab_usage_map_offset;
	guint16		tab_first_dpg_offset;
	guint16		tab_cols_start_offset;
	guint16		tab_ridx_entry_size;
	guint16		col_fixed_offset;
	guint16		col_size_offset;
	guint16		col_num_offset;
	guint16		tab_col_entry_size;
} MdbFormatConstants; 

typedef struct {
	MdbFile       *f;
	guint16       cur_pg;
	guint16       row_num;
	unsigned int  cur_pos;
	unsigned char pg_buf[MDB_PGSIZE];
	unsigned char alt_pg_buf[MDB_PGSIZE];
	int		num_catalog;
	GPtrArray	*catalog;
	MdbBackend	*default_backend;
	char		*backend_name;
	MdbFormatConstants *fmt;
	MdbStatistics *stats;
} MdbHandle; 

typedef struct {
	MdbHandle	*mdb;
	char           object_name[MDB_MAX_OBJ_NAME+1];
	int            object_type;
	unsigned long  table_pg; /* misnomer since object may not be a table */
	unsigned long  kkd_pg;
	unsigned int   kkd_rowid;
	int			num_props;
	GArray		*props;
	GArray		*columns;
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
	int  noskip_del;  /* don't skip deleted rows */
	/* object allocation map */
	int  map_base_pg;
	int  map_sz;
	unsigned char *usage_map;
	/* */
	int  idxmap_base_pg;
	int  idxmap_sz;
	unsigned char *idx_usage_map;
} MdbTableDef;

typedef struct {
	int		index_num;
	char		name[MDB_MAX_OBJ_NAME+1];
	unsigned char	index_type;
	int		first_pg;
	int		num_rows;  /* number rows in index */
	int		num_keys;
	short	key_col_num[MDB_MAX_IDX_COLS];
	unsigned char	key_col_order[MDB_MAX_IDX_COLS];
	unsigned char	flags;
	MdbTableDef	*table;
} MdbIndex;

typedef struct {
	guint32 pg;
	int mask_pos;	
	unsigned char mask_byte;
	int mask_bit;
	int offset;
	int len;
} MdbIndexPage;

#define MDB_MAX_INDEX_DEPTH 10

typedef struct {
	int cur_depth;
	MdbIndexPage pages[MDB_MAX_INDEX_DEPTH];
} MdbIndexChain;

typedef struct {
	char		name[MDB_MAX_OBJ_NAME+1];
} MdbColumnProp;

typedef struct {
	char		name[MDB_MAX_OBJ_NAME+1];
	int		col_type;
	int		col_size;
	void	*bind_ptr;
	int		*len_ptr;
	GHashTable	*properties;
	int		num_sargs;
	GPtrArray	*sargs;
	GPtrArray	*idx_sarg_cache;
	unsigned char   is_fixed;
	int		query_order;
	int		col_num;
	int		cur_value_start;
	int 	cur_value_len;
	/* numerics only */
	int		col_prec;
	int		col_scale;
} MdbColumn;

typedef struct {
	void *value;
	int siz;
	int start;
	unsigned char is_null;
	unsigned char is_fixed;
	int colnum;
	int offset;
} MdbField;

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
extern void mdb_alloc_catalog(MdbHandle *mdb);
extern MdbFile *mdb_alloc_file();
extern void mdb_free_file(MdbFile *f);

/* file.c */
extern size_t mdb_read_pg(MdbHandle *mdb, unsigned long pg);
extern size_t mdb_read_alt_pg(MdbHandle *mdb, unsigned long pg);
extern unsigned char mdb_get_byte(MdbHandle *mdb, int offset);
extern int    mdb_get_int16(MdbHandle *mdb, int offset);
extern gint32   mdb_get_int24(MdbHandle *mdb, int offset);
extern long   mdb_get_int32(MdbHandle *mdb, int offset);
extern float  mdb_get_single(MdbHandle *mdb, int offset);
extern double mdb_get_double(MdbHandle *mdb, int offset);
extern MdbHandle *mdb_open(char *filename);
extern MdbHandle *mdb_clone_handle(MdbHandle *mdb);
extern void mdb_swap_pgbuf(MdbHandle *mdb);

/* catalog.c */
GPtrArray *mdb_read_catalog(MdbHandle *mdb, int obj_type);
extern void mdb_catalog_dump(MdbHandle *mdb, int obj_type);
extern int mdb_catalog_rows(MdbHandle *mdb);
extern MdbCatalogEntry *mdb_get_catalog_entry(MdbHandle *mdb, int rowid, MdbCatalogEntry *entry);
extern char *mdb_get_objtype_string(int obj_type);

/* table.c */
extern MdbTableDef *mdb_read_table(MdbCatalogEntry *entry);
extern GPtrArray *mdb_read_columns(MdbTableDef *table);


/* data.c */
extern void mdb_data_dump(MdbTableDef *table);
extern void mdb_bind_column(MdbTableDef *table, int col_num, void *bind_ptr);
extern int mdb_rewind_table(MdbTableDef *table);
extern int mdb_fetch_row(MdbTableDef *table);
extern int mdb_is_fixed_col(MdbColumn *col);
extern char *mdb_col_to_string(MdbHandle *mdb, int start, int datatype, int size);
extern int mdb_find_end_of_row(MdbHandle *mdb, int row);


/* dump.c */
extern void buffer_dump(const unsigned char* buf, int start, int end);

/* backend.c */
extern char *mdb_get_coltype_string(MdbBackend *backend, int col_type);
extern void mdb_init_backends();
extern void mdb_register_backend(MdbBackend *backend, char *backend_name);
extern int  mdb_set_default_backend(MdbHandle *mdb, char *backend_name);
extern char *mdb_get_relationships(MdbHandle *mdb);

/* sargs.c */
extern int mdb_test_sargs(MdbHandle *mdb, MdbColumn *col, int offset, int len);

/* index.c */
extern GPtrArray *mdb_read_indices(MdbTableDef *table);
extern void mdb_index_dump(MdbTableDef *table, MdbIndex *idx);
#endif /* _mdbtools_h_ */
