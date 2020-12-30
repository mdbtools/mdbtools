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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
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

#ifdef HAVE_GLIB
#include <glib.h>
#else
#include <mdbfakeglib.h>
#endif

#ifdef HAVE_ICONV
#include <iconv.h>
#else
#ifdef HAVE_XLOCALE_H
#include <xlocale.h>
#endif
#include <locale.h>
#endif

#ifdef _WIN32
#include <io.h>
#endif

#define MDB_DEBUG 0

#define MDB_PGSIZE 4096
//#define MDB_MAX_OBJ_NAME (256*3) /* unicode 16 -> utf-8 worst case */
#define MDB_MAX_OBJ_NAME 256
#define MDB_MAX_COLS 256
#define MDB_MAX_IDX_COLS 10
#define MDB_CATALOG_PG 18
#define MDB_MEMO_OVERHEAD 12
#define MDB_BIND_SIZE 16384 // override with mdb_set_bind_size(MdbHandle*, size_t)

// This attribute is not supported by all compilers:
// M$VC see http://stackoverflow.com/questions/1113409/attribute-constructor-equivalent-in-vc
#define MDB_DEPRECATED(type, funcname) type __attribute__((deprecated)) funcname

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
	MDB_VER_JET4 = 0x01,
	MDB_VER_ACCDB_2007 = 0x02,
	MDB_VER_ACCDB_2010 = 0x03,
	MDB_VER_ACCDB_2013 = 0x04,
	MDB_VER_ACCDB_2016 = 0x05
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
	MDB_NUMERIC = 0x10,
	MDB_COMPLEX = 0x12
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
	MDB_SHEXP_BULK_INSERT = 1 << 7 /* export data in bulk inserts */
};
#define MDB_SHEXP_DEFAULT (MDB_SHEXP_CST_NOTNULL | MDB_SHEXP_COMMENTS | MDB_SHEXP_INDEXES | MDB_SHEXP_RELATIONS)

/* csv export binary options */
enum {
	MDB_BINEXPORT_STRIP,
	MDB_BINEXPORT_RAW,
	MDB_BINEXPORT_OCTAL,
	MDB_BINEXPORT_HEXADECIMAL,

    /* Flags that can be OR'ed into the above when calling mdb_print_col */
	MDB_EXPORT_ESCAPE_CONTROL_CHARS = (1 << 4)
};

#define IS_JET4(mdb) (mdb->f->jet_version==MDB_VER_JET4) /* obsolete */
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
	const MdbBackendType *types_table;
	const MdbBackendType *type_shortdate;
	const MdbBackendType *type_autonum;
	const char *short_now;
	const char *long_now;
	const char *date_fmt;
	const char *shortdate_fmt;
	const char *charset_statement;
	const char *drop_statement;
	const char *constaint_not_empty_statement;
	const char *column_comment_statement;
	const char *per_column_comment_statement;
	const char *table_comment_statement;
	const char *per_table_comment_statement;
	gchar* (*quote_schema_name)(const gchar*, const gchar*);
} MdbBackend;

typedef struct {
	gboolean collect;
	unsigned long pg_reads;
} MdbStatistics;

typedef struct {
	FILE        *stream;
	gboolean      writable;
	guint32		jet_version;
	guint32		db_key;
	char		db_passwd[14];
	MdbStatistics	*stats;
	/* free map */
	int  map_sz;
	unsigned char *free_map;
	/* reference count */
	int refs;
	guint16 code_page;
	guint16 lang_id;
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
	MdbFormatConstants *fmt;
    size_t bind_size;
    char date_fmt[64];
    char shortdate_fmt[64];
    const char *boolean_false_value;
    const char *boolean_true_value;
	unsigned int  num_catalog;

    // Non-cloneable fields start here
	GPtrArray	*catalog;
	MdbBackend	*default_backend;
	char		*backend_name;
	struct S_MdbTableDef *relationships_table;
	char        *relationships_values[5];
	MdbStatistics *stats;
    GHashTable *backends;
#ifdef HAVE_ICONV
	iconv_t	iconv_in;
	iconv_t	iconv_out;
#elif defined(_WIN32) || defined(WIN32) || defined(_WIN64) || defined(WIN64) || defined(WINDOWS)
    _locale_t locale;
#else
    locale_t locale;
#endif
} MdbHandle; 

typedef struct {
	MdbHandle	*mdb;
	char           object_name[MDB_MAX_OBJ_NAME+1];
	int            object_type;
	unsigned long  table_pg; /* misnomer since object may not be a table */
	//int			num_props; please use props->len
	GPtrArray		*props; /* GPtrArray of MdbProperties */
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
	unsigned char val_type;
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
	int rc;
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

/* version.c */
const char *mdb_get_version(void);

/* file.c */
ssize_t mdb_read_pg(MdbHandle *mdb, unsigned long pg);
ssize_t mdb_read_alt_pg(MdbHandle *mdb, unsigned long pg);
unsigned char mdb_get_byte(void *buf, int offset);
int    mdb_get_int16(void *buf, int offset);
long   mdb_get_int32(void *buf, int offset);
long   mdb_get_int32_msb(void *buf, int offset);
float  mdb_get_single(void *buf, int offset);
double mdb_get_double(void *buf, int offset);
unsigned char mdb_pg_get_byte(MdbHandle *mdb, int offset);
int    mdb_pg_get_int16(MdbHandle *mdb, int offset);
long   mdb_pg_get_int32(MdbHandle *mdb, int offset);
float  mdb_pg_get_single(MdbHandle *mdb, int offset);
double mdb_pg_get_double(MdbHandle *mdb, int offset);
MdbHandle *mdb_open(const char *filename, MdbFileFlags flags);
MdbHandle *mdb_open_buffer(void *buffer, size_t len, MdbFileFlags flags);
void mdb_close(MdbHandle *mdb);
MdbHandle *mdb_clone_handle(MdbHandle *mdb);
void mdb_swap_pgbuf(MdbHandle *mdb);

/* catalog.c */
void mdb_free_catalog(MdbHandle *mdb);
GPtrArray *mdb_read_catalog(MdbHandle *mdb, int obj_type);
MdbCatalogEntry *mdb_get_catalogentry_by_name(MdbHandle *mdb, const gchar* name);
void mdb_dump_catalog(MdbHandle *mdb, int obj_type);
const char *mdb_get_objtype_string(int obj_type);

/* table.c */
MdbTableDef *mdb_alloc_tabledef(MdbCatalogEntry *entry);
void mdb_free_tabledef(MdbTableDef *table);
MdbTableDef *mdb_read_table(MdbCatalogEntry *entry);
MdbTableDef *mdb_read_table_by_name(MdbHandle *mdb, gchar *table_name, int obj_type);
void mdb_append_column(GPtrArray *columns, MdbColumn *in_col);
void mdb_free_columns(GPtrArray *columns);
GPtrArray *mdb_read_columns(MdbTableDef *table);
void mdb_table_dump(MdbCatalogEntry *entry);
guint8 read_pg_if_8(MdbHandle *mdb, int *cur_pos);
guint16 read_pg_if_16(MdbHandle *mdb, int *cur_pos);
guint32 read_pg_if_32(MdbHandle *mdb, int *cur_pos);
void *read_pg_if_n(MdbHandle *mdb, void *buf, int *cur_pos, size_t len);
int mdb_is_user_table(MdbCatalogEntry *entry);
int mdb_is_system_table(MdbCatalogEntry *entry);
const char *mdb_table_get_prop(const MdbTableDef *table, const gchar *key);
const char *mdb_col_get_prop(const MdbColumn *col, const gchar *key);
int mdb_col_is_shortdate(const MdbColumn *col);

/* data.c */
int mdb_bind_column_by_name(MdbTableDef *table, gchar *col_name, void *bind_ptr, int *len_ptr);
void mdb_data_dump(MdbTableDef *table);
void mdb_date_to_tm(double td, struct tm *t);
void mdb_tm_to_date(struct tm *t, double *td);
char *mdb_uuid_to_string(const void *buf, int start);
int mdb_bind_column(MdbTableDef *table, int col_num, void *bind_ptr, int *len_ptr);
int mdb_rewind_table(MdbTableDef *table);
int mdb_fetch_row(MdbTableDef *table);
int mdb_is_fixed_col(MdbColumn *col);
char *mdb_col_to_string(MdbHandle *mdb, void *buf, int start, int datatype, int size);
int mdb_find_pg_row(MdbHandle *mdb, int pg_row, void **buf, int *off, size_t *len);
int mdb_find_row(MdbHandle *mdb, int row, int *start, size_t *len);
int mdb_find_end_of_row(MdbHandle *mdb, int row);
int mdb_col_fixed_size(MdbColumn *col);
int mdb_col_disp_size(MdbColumn *col);
size_t mdb_ole_read_next(MdbHandle *mdb, MdbColumn *col, void *ole_ptr);
size_t mdb_ole_read(MdbHandle *mdb, MdbColumn *col, void *ole_ptr, size_t chunk_size);
void* mdb_ole_read_full(MdbHandle *mdb, MdbColumn *col, size_t *size);
void mdb_set_bind_size(MdbHandle *mdb, size_t bind_size);
void mdb_set_date_fmt(MdbHandle *mdb, const char *);
void mdb_set_shortdate_fmt(MdbHandle *mdb, const char *);
void mdb_set_boolean_fmt_words(MdbHandle *mdb);
void mdb_set_boolean_fmt_numbers(MdbHandle *mdb);
int mdb_read_row(MdbTableDef *table, unsigned int row);

/* dump.c */
void mdb_buffer_dump(const void *buf, off_t start, size_t len);

/* backend.c */
void mdb_init_backends(MdbHandle *mdb);
void mdb_remove_backends(MdbHandle *mdb);
const MdbBackendType* mdb_get_colbacktype(const MdbColumn *col);
const char* mdb_get_colbacktype_string(const MdbColumn *col);
int mdb_colbacktype_takes_length(const MdbColumn *col);
void mdb_register_backend(MdbHandle *mdb, char *backend_name, guint32 capabilities,
        const MdbBackendType *backend_type,
        const MdbBackendType *type_shortdate,
        const MdbBackendType *type_autonum,
        const char *short_now, const char *long_now,
        const char *date_fmt, const char *shortdate_fmt,
        const char *charset_statement, const char *drop_statement, const char *constaint_not_empty_statement,
        const char *column_comment_statement, const char *per_column_comment_statement,
        const char *table_comment_statement, const char *per_table_comment_statement,
        gchar* (*quote_schema_name)(const gchar*, const gchar*));
int  mdb_set_default_backend(MdbHandle *mdb, const char *backend_name);
void mdb_print_schema(MdbHandle *mdb, FILE *outfile, char *tabname, char *dbnamespace, guint32 export_options);
void mdb_print_col(FILE *outfile, gchar *col_val, int quote_text, int col_type, int bin_len, char *quote_char, char *escape_char, int flags);

/* sargs.c */
int mdb_test_sargs(MdbTableDef *table, MdbField *fields, int num_fields);
int mdb_test_sarg(MdbHandle *mdb, MdbColumn *col, MdbSargNode *node, MdbField *field);
void mdb_sql_walk_tree(MdbSargNode *node, MdbSargTreeFunc func, gpointer data);
int mdb_find_indexable_sargs(MdbSargNode *node, gpointer data);
int mdb_add_sarg_by_name(MdbTableDef *table, char *colname, MdbSarg *in_sarg);
int mdb_test_string(MdbSargNode *node, char *s);
int mdb_test_int(MdbSargNode *node, gint32 i);
int mdb_add_sarg(MdbColumn *col, MdbSarg *in_sarg);



/* index.c */
GPtrArray *mdb_read_indices(MdbTableDef *table);
void mdb_index_dump(MdbTableDef *table, MdbIndex *idx);
void mdb_index_scan_free(MdbTableDef *table);
int mdb_index_find_next_on_page(MdbHandle *mdb, MdbIndexPage *ipg);
int mdb_index_find_next(MdbHandle *mdb, MdbIndex *idx, MdbIndexChain *chain, guint32 *pg, guint16 *row);
void mdb_index_hash_text(MdbHandle *mdb, char *text, char *hash);
void mdb_index_scan_init(MdbHandle *mdb, MdbTableDef *table);
int mdb_index_find_row(MdbHandle *mdb, MdbIndex *idx, MdbIndexChain *chain, guint32 pg, guint16 row);
void mdb_index_swap_n(unsigned char *src, int sz, unsigned char *dest);
void mdb_free_indices(GPtrArray *indices);
void mdb_index_page_reset(MdbHandle *mdb, MdbIndexPage *ipg);
int mdb_index_pack_bitmap(MdbHandle *mdb, MdbIndexPage *ipg);

/* stats.c */
void mdb_stats_on(MdbHandle *mdb);
void mdb_stats_off(MdbHandle *mdb);
void mdb_dump_stats(MdbHandle *mdb);

/* like.c */
int mdb_like_cmp(char *s, char *r);

/* write.c */
void mdb_put_int16(void *buf, guint32 offset, guint32 value);
void mdb_put_int32(void *buf, guint32 offset, guint32 value);
void mdb_put_int32_msb(void *buf, guint32 offset, guint32 value);
int mdb_crack_row(MdbTableDef *table, int row_start, size_t row_size, MdbField *fields);
guint16 mdb_add_row_to_pg(MdbTableDef *table, unsigned char *row_buffer, int new_row_size);
int mdb_update_index(MdbTableDef *table, MdbIndex *idx, unsigned int num_fields, MdbField *fields, guint32 pgnum, guint16 rownum);
int mdb_insert_row(MdbTableDef *table, int num_fields, MdbField *fields);
int mdb_pack_row(MdbTableDef *table, unsigned char *row_buffer, unsigned int num_fields, MdbField *fields);
int mdb_replace_row(MdbTableDef *table, int row, void *new_row, int new_row_size);
int mdb_pg_get_freespace(MdbHandle *mdb);
int mdb_update_row(MdbTableDef *table);
void *mdb_new_data_pg(MdbCatalogEntry *entry);

/* map.c */
gint32 mdb_map_find_next_freepage(MdbTableDef *table, int row_size);
gint32 mdb_map_find_next(MdbHandle *mdb, unsigned char *map, unsigned int map_sz, guint32 start_pg);

/* props.c */
void mdb_free_props(MdbProperties *props);
void mdb_dump_props(MdbProperties *props, FILE *outfile, int show_name);
GPtrArray* mdb_kkd_to_props(MdbHandle *mdb, void *kkd, size_t len);


/* worktable.c */
MdbTableDef *mdb_create_temp_table(MdbHandle *mdb, char *name);
void mdb_temp_table_add_col(MdbTableDef *table, MdbColumn *col);
void mdb_fill_temp_col(MdbColumn *tcol, char *col_name, int col_size, int col_type, int is_fixed);
void mdb_fill_temp_field(MdbField *field, void *value, int siz, int is_fixed, int is_null, int start, int column);
void mdb_temp_columns_end(MdbTableDef *table);

/* options.c */
int mdb_get_option(unsigned long optnum);
void mdb_debug(int klass, char *fmt, ...);

/* iconv.c */
int mdb_unicode2ascii(MdbHandle *mdb, const char *src, size_t slen, char *dest, size_t dlen);
int mdb_ascii2unicode(MdbHandle *mdb, const char *src, size_t slen, char *dest, size_t dlen);
void mdb_iconv_init(MdbHandle *mdb);
void mdb_iconv_close(MdbHandle *mdb);
const char* mdb_target_charset(MdbHandle *mdb);

#ifdef __cplusplus
  }
#endif

#endif /* _mdbtools_h_ */
