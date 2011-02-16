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

#ifdef __cplusplus
  extern "C" {
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <glib.h>

#ifdef HAVE_ICONV
#include <iconv.h>
#endif

#define MDB_DEBUG 0

#define MDB_PGSIZE 4096
//#define MDB_MAX_OBJ_NAME (256*3) /* unicode 16 -> utf-8 worst case */
#define MDB_MAX_OBJ_NAME 256
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
	MDB_UNKNOWN_0A, /* User access */
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
	MDB_DATETIME = 0x08,
	MDB_BINARY = 0x09,
	MDB_TEXT = 0x0a,
	MDB_OLE = 0x0b,
	MDB_MEMO = 0x0c,
	MDB_REPID = 0x0f,
	MDB_NUMERIC = 0x10
};

/* SARG operators */
enum {
	MDB_OR = 1,
	MDB_AND,
	MDB_NOT,
	MDB_EQUAL,
	MDB_GT,
	MDB_LT,
	MDB_GTEQ,
	MDB_LTEQ,
	MDB_LIKE,
	MDB_ISNULL,
	MDB_NOTNULL
};

typedef enum {
	MDB_TABLE_SCAN,
	MDB_LEAF_SCAN,
	MDB_INDEX_SCAN
} MdbStrategy;

typedef enum {
	MDB_NOFLAGS = 0x00,
	MDB_WRITABLE = 0x01
} MdbFileFlags;

enum {
	MDB_DEBUG_LIKE = 0x0001,
	MDB_DEBUG_WRITE = 0x0002,
	MDB_DEBUG_USAGE = 0x0004,
	MDB_DEBUG_OLE = 0x0008,
	MDB_DEBUG_ROW = 0x0010,
	MDB_DEBUG_PROPS = 0x0020,
	MDB_USE_INDEX = 0x0040,
	MDB_NO_MEMO = 0x0080, /* don't follow memo fields */
};

#define mdb_is_logical_op(x) (x == MDB_OR || \
				x == MDB_AND || \
				x == MDB_NOT )

#define mdb_is_relational_op(x) (x == MDB_EQUAL || \
				x == MDB_GT || \
				x == MDB_LT || \
				x == MDB_GTEQ || \
				x == MDB_LTEQ || \
				x == MDB_LIKE || \
				x == MDB_ISNULL || \
				x == MDB_NOTNULL )

enum {
	MDB_ASC,
	MDB_DESC
};

enum {
	MDB_IDX_UNIQUE = 0x01,
	MDB_IDX_IGNORENULLS = 0x02,
	MDB_IDX_REQUIRED = 0x08 
};

/* export schema options */
enum {
	MDB_SHEXP_DROPTABLE = 1<<0, /* issue drop table during export */
	MDB_SHEXP_CST_NOTNULL = 1<<1, /* generate NOT NULL constraints */
	MDB_SHEXP_CST_NOTEMPTY = 1<<2, /* <>'' constraints */
	MDB_SHEXP_COMMENTS = 1<<3, /* export comments on columns & tables */
	MDB_SHEXP_DEFVALUES = 1<<4, /* export default values */
	MDB_SHEXP_INDEXES = 1<<5, /* export indices */
	MDB_SHEXP_RELATIONS = 1<<6, /* export relation (foreign keys) */
	MDB_SHEXP_SANITIZE = 1<<7 /* clean up names */
};
#define MDB_SHEXP_DEFAULT (MDB_SHEXP_CST_NOTNULL | MDB_SHEXP_COMMENTS | MDB_SHEXP_INDEXES | MDB_SHEXP_RELATIONS)

#define IS_JET4(mdb) (mdb->f->jet_version==MDB_VER_JET4)
#define IS_JET3(mdb) (mdb->f->jet_version==MDB_VER_JET3)

/* forward declarations */
typedef struct mdbindex MdbIndex;
typedef struct mdbsargtree MdbSargNode;

typedef struct {
	char *name;
	unsigned char needs_length; /* or precision */
	unsigned char needs_scale;
	unsigned char needs_quotes;
} MdbBackendType;
		
typedef struct {
	guint32 capabilities; /* see MDB_SHEXP_* */
	MdbBackendType *types_table;
	MdbBackendType *type_shortdate;
	MdbBackendType *type_autonum;
	const char *short_now;
	const char *long_now;
	const char *charset_statement;
	const char *drop_statement;
	const char *constaint_not_empty_statement;
	const char *column_comment_statement;
	const char *table_comment_statement;
	gchar* (*quote_schema_name)(const gchar*, const gchar*);
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
	ssize_t		pg_size;
	guint16		row_count_offset; 
	guint16		tab_num_rows_offset;
	guint16		tab_num_cols_offset;
	guint16		tab_num_idxs_offset;
	guint16		tab_num_ridxs_offset;
	guint16		tab_usage_map_offset;
	guint16		tab_first_dpg_offset;
	guint16		tab_cols_start_offset;
	guint16		tab_ridx_entry_size;
	guint16		col_flags_offset;
	guint16		col_size_offset;
	guint16		col_num_offset;
	guint16		tab_col_entry_size;
	guint16         tab_free_map_offset;
	guint16		tab_col_offset_var;
	guint16		tab_col_offset_fixed;
	guint16		tab_row_col_num_offset;
} MdbFormatConstants; 

typedef struct {
	MdbFile       *f;
	guint32       cur_pg;
	guint16       row_num;
	unsigned int  cur_pos;
	unsigned char pg_buf[MDB_PGSIZE];
	unsigned char alt_pg_buf[MDB_PGSIZE];
	unsigned int  num_catalog;
	GPtrArray	*catalog;
	MdbBackend	*default_backend;
	char		*backend_name;
	MdbFormatConstants *fmt;
	MdbStatistics *stats;
#ifdef HAVE_ICONV
	iconv_t	iconv_in;
	iconv_t	iconv_out;
#endif
} MdbHandle; 

typedef struct {
	MdbHandle	*mdb;
	char           object_name[MDB_MAX_OBJ_NAME+1];
	int            object_type;
	unsigned long  table_pg; /* misnomer since object may not be a table */
	//int			num_props; please use props->len
	GArray		*props; /* GArray of MdbProperties */
	GArray		*columns;
	int		flags;
} MdbCatalogEntry;

typedef struct {
	gchar *name;
	GHashTable	*hash;
} MdbProperties;

typedef union {
	int	i;
	double	d;
	char	s[256];
} MdbAny;

struct S_MdbTableDef; /* forward definition */
typedef struct {
	struct S_MdbTableDef *table;
	char		name[MDB_MAX_OBJ_NAME+1];
	int		col_type;
	int		col_size;
	void	*bind_ptr;
	int		*len_ptr;
	GHashTable	*properties;
	unsigned int	num_sargs;
	GPtrArray	*sargs;
	GPtrArray	*idx_sarg_cache;
	unsigned char   is_fixed;
	int		query_order;
	/* col_num is the current column order, 
	 * does not include deletes */
	int		col_num;	
	int		cur_value_start;
	int 		cur_value_len;
	/* MEMO/OLE readers */
	guint32		cur_blob_pg_row;
	int		chunk_size;
	/* numerics only */
	int		col_prec;
	int		col_scale;
	unsigned char     is_long_auto;
	unsigned char     is_uuid_auto;
	MdbProperties	*props;
	/* info needed for handling deleted/added columns */
	int 		fixed_offset;
	unsigned int	var_col_num;
	/* row_col_num is the row column number order, 
	 * including deleted columns */
	int		row_col_num;
} MdbColumn;

struct mdbsargtree {
	int       op;
	MdbColumn *col;
	MdbAny    value;
	void      *parent;
	MdbSargNode *left;
	MdbSargNode *right;
};

typedef struct {
	guint32 pg;
	int start_pos;
	int offset;
	int len;
	guint16 idx_starts[2000];	
	unsigned char cache_value[256];
} MdbIndexPage;

typedef int (*MdbSargTreeFunc)(MdbSargNode *, gpointer data);

#define MDB_MAX_INDEX_DEPTH 10

typedef struct {
	int cur_depth;
	guint32 last_leaf_found;
	int clean_up_mode;
	MdbIndexPage pages[MDB_MAX_INDEX_DEPTH];
} MdbIndexChain;

typedef struct S_MdbTableDef {
	MdbCatalogEntry *entry;
	char	name[MDB_MAX_OBJ_NAME+1];
	unsigned int    num_cols;
	GPtrArray	*columns;
	unsigned int    num_rows;
	int	index_start;
	unsigned int    num_real_idxs;
	unsigned int    num_idxs;
	GPtrArray	*indices;
	guint32	first_data_pg;
	guint32	cur_pg_num;
	guint32	cur_phys_pg;
	unsigned int    cur_row;
	int  noskip_del;  /* don't skip deleted rows */
	/* object allocation map */
	guint32  map_base_pg;
	size_t map_sz;
	unsigned char *usage_map;
	/* pages with free space left */
	guint32  freemap_base_pg;
	size_t freemap_sz;
	unsigned char *free_usage_map;
	/* query planner */
	MdbSargNode *sarg_tree;
	MdbStrategy strategy;
	MdbIndex *scan_idx;
	MdbHandle *mdbidx;
	MdbIndexChain *chain;
	MdbProperties	*props;
	unsigned int num_var_cols;  /* to know if row has variable columns */
	/* temp table */
	unsigned int  is_temp_table;
	GPtrArray     *temp_table_pages;
} MdbTableDef;

struct mdbindex {
	int		index_num;
	char		name[MDB_MAX_OBJ_NAME+1];
	unsigned char	index_type;
	guint32		first_pg;
	int		num_rows;  /* number rows in index */
	unsigned int	num_keys;
	short	key_col_num[MDB_MAX_IDX_COLS];
	unsigned char	key_col_order[MDB_MAX_IDX_COLS];
	unsigned char	flags;
	MdbTableDef	*table;
};

typedef struct {
	char		name[MDB_MAX_OBJ_NAME+1];
} MdbColumnProp;

typedef struct {
	void *value;
	int siz;
	int start;
	unsigned char is_null;
	unsigned char is_fixed;
	int colnum;
	int offset;
} MdbField;

typedef struct {
	int	op;
	MdbAny	value;
} MdbSarg;

/* mem.c */
extern void mdb_init();
extern void mdb_exit();

/* file.c */
extern ssize_t mdb_read_pg(MdbHandle *mdb, unsigned long pg);
extern ssize_t mdb_read_alt_pg(MdbHandle *mdb, unsigned long pg);
extern unsigned char mdb_get_byte(void *buf, int offset);
extern int    mdb_get_int16(void *buf, int offset);
extern long   mdb_get_int32(void *buf, int offset);
extern long   mdb_get_int32_msb(void *buf, int offset);
extern float  mdb_get_single(void *buf, int offset);
extern double mdb_get_double(void *buf, int offset);
extern unsigned char mdb_pg_get_byte(MdbHandle *mdb, int offset);
extern int    mdb_pg_get_int16(MdbHandle *mdb, int offset);
extern long   mdb_pg_get_int32(MdbHandle *mdb, int offset);
extern float  mdb_pg_get_single(MdbHandle *mdb, int offset);
extern double mdb_pg_get_double(MdbHandle *mdb, int offset);
extern MdbHandle *mdb_open(const char *filename, MdbFileFlags flags);
extern void mdb_close(MdbHandle *mdb);
extern MdbHandle *mdb_clone_handle(MdbHandle *mdb);
extern void mdb_swap_pgbuf(MdbHandle *mdb);

/* catalog.c */
extern void mdb_free_catalog(MdbHandle *mdb);
extern GPtrArray *mdb_read_catalog(MdbHandle *mdb, int obj_type);
extern void mdb_dump_catalog(MdbHandle *mdb, int obj_type);
extern char *mdb_get_objtype_string(int obj_type);

/* table.c */
extern MdbTableDef *mdb_alloc_tabledef(MdbCatalogEntry *entry);
extern void mdb_free_tabledef(MdbTableDef *table);
extern MdbTableDef *mdb_read_table(MdbCatalogEntry *entry);
extern MdbTableDef *mdb_read_table_by_name(MdbHandle *mdb, gchar *table_name, int obj_type);
extern void mdb_append_column(GPtrArray *columns, MdbColumn *in_col);
extern void mdb_free_columns(GPtrArray *columns);
extern GPtrArray *mdb_read_columns(MdbTableDef *table);
extern void mdb_table_dump(MdbCatalogEntry *entry);
extern guint8 read_pg_if_8(MdbHandle *mdb, int *cur_pos);
extern guint16 read_pg_if_16(MdbHandle *mdb, int *cur_pos);
extern guint32 read_pg_if_32(MdbHandle *mdb, int *cur_pos);
extern void *read_pg_if_n(MdbHandle *mdb, void *buf, int *cur_pos, size_t len);
extern int mdb_is_user_table(MdbCatalogEntry *entry);
extern int mdb_is_system_table(MdbCatalogEntry *entry);
extern const char *mdb_table_get_prop(const MdbTableDef *table, const gchar *key);
extern const char *mdb_col_get_prop(const MdbColumn *col, const gchar *key);

/* data.c */
extern int mdb_bind_column_by_name(MdbTableDef *table, gchar *col_name, void *bind_ptr, int *len_ptr);
extern void mdb_data_dump(MdbTableDef *table);
extern void mdb_bind_column(MdbTableDef *table, int col_num, void *bind_ptr, int *len_ptr);
extern int mdb_rewind_table(MdbTableDef *table);
extern int mdb_fetch_row(MdbTableDef *table);
extern int mdb_is_fixed_col(MdbColumn *col);
extern char *mdb_col_to_string(MdbHandle *mdb, void *buf, int start, int datatype, int size);
extern int mdb_find_pg_row(MdbHandle *mdb, int pg_row, void **buf, int *off, size_t *len);
extern int mdb_find_row(MdbHandle *mdb, int row, int *start, size_t *len);
extern int mdb_find_end_of_row(MdbHandle *mdb, int row);
extern int mdb_col_fixed_size(MdbColumn *col);
extern int mdb_col_disp_size(MdbColumn *col);
extern size_t mdb_ole_read_next(MdbHandle *mdb, MdbColumn *col, void *ole_ptr);
extern size_t mdb_ole_read(MdbHandle *mdb, MdbColumn *col, void *ole_ptr, int chunk_size);
extern void* mdb_ole_read_full(MdbHandle *mdb, MdbColumn *col, size_t *size);
extern void mdb_set_date_fmt(const char *);
extern int mdb_read_row(MdbTableDef *table, unsigned int row);

/* dump.c */
extern void buffer_dump(const void *buf, int start, size_t len);

/* backend.c */
extern char* sanitize_name(const char* name);
extern char* mdb_get_coltype_string(MdbBackend *backend, int col_type); /* obsolete */
extern int mdb_coltype_takes_length(MdbBackend *backend, int col_type); /* obsolete */
extern const MdbBackendType* mdb_get_colbacktype(const MdbColumn *col);
extern const char* mdb_get_colbacktype_string(const MdbColumn *col);
extern int mdb_colbacktype_takes_length(const MdbColumn *col);
extern void mdb_init_backends();
extern void mdb_register_backend(char *backend_name, guint32 capabilities, MdbBackendType *backend_type, MdbBackendType *type_shortdate, MdbBackendType *type_autonum, const char *short_now, const char *long_now, const char *charset_statement, const char *drop_statement, const char *constaint_not_empty_statement, const char *column_comment_statement, const char *table_comment_statement, gchar* (*quote_schema_name)(const gchar*, const gchar*));
extern void mdb_remove_backends();
extern int  mdb_set_default_backend(MdbHandle *mdb, const char *backend_name);
extern void mdb_print_schema(MdbHandle *mdb, FILE *outfile, char *tabname, char *namespace, guint32 export_options);

/* sargs.c */
extern int mdb_test_sargs(MdbTableDef *table, MdbField *fields, int num_fields);
extern int mdb_test_sarg(MdbHandle *mdb, MdbColumn *col, MdbSargNode *node, MdbField *field);
extern void mdb_sql_walk_tree(MdbSargNode *node, MdbSargTreeFunc func, gpointer data);
extern int mdb_find_indexable_sargs(MdbSargNode *node, gpointer data);
extern int mdb_add_sarg_by_name(MdbTableDef *table, char *colname, MdbSarg *in_sarg);
extern int mdb_test_string(MdbSargNode *node, char *s);
extern int mdb_test_int(MdbSargNode *node, gint32 i);
extern int mdb_add_sarg(MdbColumn *col, MdbSarg *in_sarg);



/* index.c */
extern GPtrArray *mdb_read_indices(MdbTableDef *table);
extern void mdb_index_dump(MdbTableDef *table, MdbIndex *idx);
extern void mdb_index_scan_free(MdbTableDef *table);
extern int mdb_index_find_next_on_page(MdbHandle *mdb, MdbIndexPage *ipg);
extern int mdb_index_find_next(MdbHandle *mdb, MdbIndex *idx, MdbIndexChain *chain, guint32 *pg, guint16 *row);
extern void mdb_index_hash_text(char *text, char *hash);
extern void mdb_index_scan_init(MdbHandle *mdb, MdbTableDef *table);
extern int mdb_index_find_row(MdbHandle *mdb, MdbIndex *idx, MdbIndexChain *chain, guint32 pg, guint16 row);
extern void mdb_index_swap_n(unsigned char *src, int sz, unsigned char *dest);
extern void mdb_free_indices(GPtrArray *indices);
void mdb_index_page_reset(MdbIndexPage *ipg);
extern int mdb_index_pack_bitmap(MdbHandle *mdb, MdbIndexPage *ipg);

/* stats.c */
extern void mdb_stats_on(MdbHandle *mdb);
extern void mdb_stats_off(MdbHandle *mdb);
extern void mdb_dump_stats(MdbHandle *mdb);

/* like.c */
extern int mdb_like_cmp(char *s, char *r);

/* write.c */
extern int mdb_crack_row(MdbTableDef *table, int row_start, int row_end, MdbField *fields);
extern guint16 mdb_add_row_to_pg(MdbTableDef *table, unsigned char *row_buffer, int new_row_size);
extern int mdb_update_index(MdbTableDef *table, MdbIndex *idx, unsigned int num_fields, MdbField *fields, guint32 pgnum, guint16 rownum);
extern int mdb_pack_row(MdbTableDef *table, unsigned char *row_buffer, unsigned int num_fields, MdbField *fields);
extern int mdb_replace_row(MdbTableDef *table, int row, void *new_row, int new_row_size);
extern int mdb_pg_get_freespace(MdbHandle *mdb);
extern int mdb_update_row(MdbTableDef *table);
extern void *mdb_new_data_pg(MdbCatalogEntry *entry);

/* map.c */
extern guint32 mdb_map_find_next_freepage(MdbTableDef *table, int row_size);
extern gint32 mdb_map_find_next(MdbHandle *mdb, unsigned char *map, unsigned int map_sz, guint32 start_pg);

/* props.c */
extern void mdb_free_props(MdbProperties *props);
extern void mdb_dump_props(MdbProperties *props, FILE *outfile, int show_name);
extern GArray* kkd_to_props(MdbHandle *mdb, void *kkd, size_t len);


/* worktable.c */
extern MdbTableDef *mdb_create_temp_table(MdbHandle *mdb, char *name);
extern void mdb_temp_table_add_col(MdbTableDef *table, MdbColumn *col);
extern void mdb_fill_temp_col(MdbColumn *tcol, char *col_name, int col_size, int col_type, int is_fixed);
extern void mdb_fill_temp_field(MdbField *field, void *value, int siz, int is_fixed, int is_null, int start, int column);
extern void mdb_temp_columns_end(MdbTableDef *table);

/* options.c */
extern int mdb_get_option(unsigned long optnum);
extern void mdb_debug(int klass, char *fmt, ...);

/* iconv.c */
extern int mdb_unicode2ascii(MdbHandle *mdb, char *src, size_t slen, char *dest, size_t dlen);
extern int mdb_ascii2unicode(MdbHandle *mdb, char *src, size_t slen, char *dest, size_t dlen);
extern void mdb_iconv_init(MdbHandle *mdb);
extern void mdb_iconv_close(MdbHandle *mdb);
extern const char* mdb_target_charset(MdbHandle *mdb);

#ifdef __cplusplus
  }
#endif

#endif /* _mdbtools_h_ */
