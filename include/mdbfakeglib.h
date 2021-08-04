/* fakeglib.c - A shim for applications that require GLib
 * without the whole kit and kaboodle.
 *
 * Copyright (C) 2020 Evan Miller
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

#ifndef _mdbfakeglib_h_
#define _mdbfakeglib_h_

#include <time.h>
#include <locale.h>
#include <inttypes.h>
#include <strings.h>

typedef uint16_t guint16;
typedef uint32_t guint32;
typedef uint64_t guint64;
typedef int32_t gint32;
typedef char gchar;
typedef int gboolean;
typedef int gint;
typedef unsigned int guint;
typedef void * gpointer;
typedef const void * gconstpointer;
typedef uint8_t guint8;
typedef guint32 GQuark;
typedef guint32 gunichar;
typedef signed long gssize;

typedef guint (*GHashFunc)(gconstpointer);
typedef int (*GCompareFunc)(gconstpointer, gconstpointer);
typedef gboolean (*GEqualFunc)(gconstpointer, gconstpointer);
typedef void (*GFunc) (gpointer data, gpointer user_data);
typedef void (*GHFunc)(gpointer key, gpointer value, gpointer data);
typedef gboolean (*GHRFunc)(gpointer key, gpointer value, gpointer data);

typedef struct GString {
  gchar  *str;
  size_t len;
  size_t allocated_len;
} GString;

typedef struct GPtrArray {
    void **pdata;
    guint len;
} GPtrArray;

typedef struct GList {
  gpointer data;
  struct GList *next;
  struct GList *prev;
} GList;

typedef struct GHashTable {
    GEqualFunc  compare;
    GPtrArray  *array;
} GHashTable;

typedef struct GError {
    GQuark       domain;
    gint         code;
    gchar       *message;
} GError;

typedef enum GOptionArg {
    G_OPTION_ARG_NONE,
    G_OPTION_ARG_STRING,
    G_OPTION_ARG_INT,
    G_OPTION_ARG_CALLBACK,
    G_OPTION_ARG_FILENAME
} GOptionArg;

typedef enum GOptionFlags {
    G_OPTION_FLAG_NONE,
    G_OPTION_FLAG_REVERSE
} GOptionFlags;

typedef struct GOptionEntry {
  const gchar *long_name;
  gchar        short_name;
  gint         flags;

  GOptionArg   arg;
  gpointer     arg_data;

  const gchar *description;
  const gchar *arg_description;
} GOptionEntry;

typedef struct GOptionContext {
    const char *desc;
    const GOptionEntry *entries;
} GOptionContext;

#define g_str_hash NULL

#define G_GUINT32_FORMAT PRIu32

#define g_return_val_if_fail(a, b) if (!a) { return b; }

#define g_ascii_strcasecmp strcasecmp
#define g_malloc0(len) calloc(1, len)
#define g_malloc malloc
#define g_free free
#define g_realloc realloc

#define	G_STR_DELIMITERS "_-|> <."

#define g_ptr_array_index(array, i) \
    ((void **)array->pdata)[i]

#define TRUE 1
#define FALSE 0

#define GUINT32_SWAP_LE_BE(l) __builtin_bswap32((uint32_t)(l))

/* string functions */
void *g_memdup(const void *src, size_t len);
int g_str_equal(const void *str1, const void *str2);
char **g_strsplit(const char *haystack, const char *needle, int max_tokens);
void g_strfreev(char **dir);
char *g_strconcat(const char *first, ...);
char *g_strdup(const char *src);
char *g_strndup(const char *src, size_t len);
char *g_strdup_printf(const char *format, ...);
gchar *g_strdelimit(gchar *string, const gchar *delimiters, gchar new_delimiter);
void g_printerr(const gchar *format, ...);

/* conversion */
gint g_unichar_to_utf8(gunichar c, gchar *dst);
gchar *g_locale_to_utf8(const gchar *opsysstring, size_t len,
        size_t *bytes_read, size_t *bytes_written, GError **error);
gchar *g_utf8_casefold(const gchar *str, gssize len);
gchar *g_utf8_strdown(const gchar *str, gssize len);

/* GString */
GString *g_string_new(const gchar *init);
GString *g_string_assign(GString *string, const gchar *rval);
GString * g_string_append (GString *string, const gchar *val);
gchar *g_string_free (GString *string, gboolean free_segment);

/* GHashTable */
void *g_hash_table_lookup(GHashTable *tree, const void *key);
gboolean g_hash_table_lookup_extended(GHashTable *table, const void *lookup_key,
        void **orig_key, void **value);
void g_hash_table_insert(GHashTable *tree, void *key, void *value);
gboolean g_hash_table_remove(GHashTable *hash_table, const void *key);
GHashTable *g_hash_table_new(GHashFunc hashes, GEqualFunc equals);
void g_hash_table_foreach(GHashTable *tree, GHFunc function, void *data);
void g_hash_table_foreach_remove(GHashTable *tree, GHRFunc function, void *data);
void g_hash_table_destroy(GHashTable *tree);

/* GPtrArray */
void g_ptr_array_sort(GPtrArray *array, GCompareFunc func);
void g_ptr_array_foreach(GPtrArray *array, GFunc function, gpointer user_data);
GPtrArray *g_ptr_array_new(void);
void g_ptr_array_add(GPtrArray *array, void *entry);
gboolean g_ptr_array_remove (GPtrArray *array, gpointer data);
void g_ptr_array_free(GPtrArray *array, gboolean something);

/* GList */
GList *g_list_append(GList *list, void *data);
GList *g_list_last(GList *list);
GList *g_list_remove(GList *list, void *data);
void g_list_free(GList *list);

/* GOption */
GOptionContext *g_option_context_new(const char *description);
void g_option_context_add_main_entries (GOptionContext *context,
        const GOptionEntry *entries,
        const gchar *translation_domain);
gchar *g_option_context_get_help (GOptionContext *context,
        gboolean main_help, void *group);
gboolean g_option_context_parse (GOptionContext *context,
        gint *argc, gchar ***argv, GError **error);
void g_option_context_free (GOptionContext *context);

#endif
