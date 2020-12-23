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

#define _GNU_SOURCE
#include "mdbfakeglib.h"

#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>

/* string functions */

void *g_memdup(const void *src, size_t len) {
    void *dst = malloc(len);
    memcpy(dst, src, len);
    return dst;
}

int g_str_equal(const void *str1, const void *str2) {
    return strcmp(str1, str2) == 0;
}

// max_tokens not yet implemented
char **g_strsplit(const char *haystack, const char *needle, int max_tokens) {
    char **ret = NULL;
    char *found = NULL;
    size_t components = 2; // last component + terminating NULL
    
    while ((found = strstr(haystack, needle))) {
        components++;
        haystack = found + strlen(needle);
    }

    ret = calloc(components, sizeof(char *));

    int i = 0;
    while ((found = strstr(haystack, needle))) {
        // Windows lacks strndup
        size_t chunk_len = found - haystack;
        char *chunk = malloc(chunk_len + 1);
        memcpy(chunk, haystack, chunk_len);
        chunk[chunk_len] = 0;

        ret[i++] = chunk;
        haystack = found + strlen(needle);
    }
    ret[i] = strdup(haystack);

    return ret;
}

void g_strfreev(char **dir) {
    int i=0;
    while (dir[i]) {
        free(dir[i]);
        i++;
    }
    free(dir);
}

char *g_strconcat(const char *first, ...) {
    char *ret = NULL;
    size_t len = strlen(first);
    char *arg = NULL;
    va_list argp;

    va_start(argp, first);
    while ((arg = va_arg(argp, char *))) {
        len += strlen(arg);
    }
    va_end(argp);

    ret = malloc(len+1);

    char *pos = strcpy(ret, first) + strlen(first);

    va_start(argp, first);
    while ((arg = va_arg(argp, char *))) {
        pos = strcpy(pos, arg) + strlen(arg);
    }
    va_end(argp);

    ret[len] = '\0';

    return ret;
}

#if defined _WIN32 && !defined(HAVE_VASPRINTF) && !defined(HAVE_VASNPRINTF)
int vasprintf(char **ret, const char *format, va_list ap) {
    int len;
    int retval;
    char *result;
    if ((len = _vscprintf(format, ap)) < 0)
        return -1;
    if ((result = malloc(len+1)) == NULL)
        return -1;
    if ((retval = vsprintf_s(result, len+1, format, ap)) == -1) {
        free(result);
        return -1;
    }
    *ret = result;
    return retval;
}
#endif

char *g_strdup(const char *input) {
    size_t len = strlen(input);
    return g_memdup(input, len+1);
}

char *g_strdup_printf(const char *format, ...) {
    char *ret = NULL;
    va_list argp;

    va_start(argp, format);
#ifdef HAVE_VASNPRINTF
    size_t len = 0;
    ret = vasnprintf(ret, &len, format, argp);
#else
    int gcc_is_dumb = vasprintf(&ret, format, argp);
    (void)gcc_is_dumb;
#endif
    va_end(argp);

    return ret;
}

gchar *g_strdelimit(gchar *string, const gchar *delimiters, gchar new_delimiter) {
    char *orig = string;
    if (delimiters == NULL)
        delimiters = G_STR_DELIMITERS;
    size_t n = strlen(delimiters);
    while (*string) {
        size_t i;
        for (i=0; i<n; i++) {
            if (*string == delimiters[i]) {
                *string = new_delimiter;
                break;
            }
        }
        string++;
    }

    return orig;
}

void g_printerr(const gchar *format, ...) {
    va_list argp;
    va_start(argp, format);
    vfprintf(stderr, format, argp);
    va_end(argp);
}

/* GString */

GString *g_string_new (const gchar *init) {
    GString *str = calloc(1, sizeof(GString));
    str->str = strdup(init ? init : "");
    str->len = strlen(str->str);
    str->allocated_len = str->len+1;
    return str;
}

GString *g_string_assign(GString *string, const gchar *rval) {
    size_t len = strlen(rval);
    string->str = realloc(string->str, len+1);
    strncpy(string->str, rval, len+1);
    string->len = len;
    string->allocated_len = len+1;
    return string;
}

GString * g_string_append (GString *string, const gchar *val) {
    size_t len = strlen(val);
    string->str = realloc(string->str, string->len + len + 1);
    strncpy(&string->str[string->len], val, len+1);
    string->len += len;
    string->allocated_len = string->len + len + 1;
    return string;
}

gchar *g_string_free (GString *string, gboolean free_segment) {
    char *data = string->str;
    free(string);
    if (free_segment) {
        free(data);
        return NULL;
    }
    return data;
}

/* GHashTable */

typedef struct MyNode {
    char *key;
    void *value;
} MyNode;

void *g_hash_table_lookup(GHashTable *table, const void *key) {
    guint i;
    for (i=0; i<table->array->len; i++) {
        MyNode *node = g_ptr_array_index(table->array, i);
        if (table->compare(key, node->key))
            return node->value;
    }
    return NULL;
}

gboolean g_hash_table_lookup_extended (GHashTable *table, const void *lookup_key,
        void **orig_key, void **value) {
    guint i;
    for (i=0; i<table->array->len; i++) {
        MyNode *node = g_ptr_array_index(table->array, i);
        if (table->compare(lookup_key, node->key)) {
            *orig_key = node->key;
            *value = node->value;
            return TRUE;
        }
    }
    return FALSE;
}

void g_hash_table_insert(GHashTable *table, void *key, void *value) {
    MyNode *node = calloc(1, sizeof(MyNode));
    node->value = value;
    node->key = key;
    g_ptr_array_add(table->array, node);
}

gboolean g_hash_table_remove(GHashTable *table, gconstpointer key) {
    int found = 0;
    for (guint i=0; i<table->array->len; i++) {
        MyNode *node = g_ptr_array_index(table->array, i);
        if (found) {
            table->array->pdata[i-1] = table->array->pdata[i];
        } else if (!found && table->compare(key, node->key)) {
            found = 1;
        }
    }
    if (found) {
        table->array->len--;
    }
    return found;
}

GHashTable *g_hash_table_new(GHashFunc hashes, GEqualFunc equals) {
    GHashTable *table = calloc(1, sizeof(GHashTable));
    table->array = g_ptr_array_new();
    table->compare = equals;
    return table;
}

void g_hash_table_foreach(GHashTable *table, GHFunc function, void *data) {
    guint i;
    for (i=0; i<table->array->len; i++) {
        MyNode *node = g_ptr_array_index(table->array, i);
        function(node->key, node->value, data);
    }
}

void g_hash_table_destroy(GHashTable *table) {
    guint i;
    for (i=0; i<table->array->len; i++) {
        MyNode *node = g_ptr_array_index(table->array, i);
        free(node);
    }
    g_ptr_array_free(table->array, TRUE);
    free(table);
}

/* GPtrArray */

void g_ptr_array_sort(GPtrArray *array, GCompareFunc func) {
    qsort(array->pdata, array->len, sizeof(void *), func);
}

void g_ptr_array_foreach(GPtrArray *array, GFunc function, gpointer user_data) {
    guint i;
    for (i=0; i<array->len; i++) {
        function(g_ptr_array_index(array, i), user_data);
    }
}

GPtrArray *g_ptr_array_new() {
    GPtrArray *array = malloc(sizeof(GPtrArray));
    array->len = 0;
    array->pdata = NULL;
    return array;
}

void g_ptr_array_add(GPtrArray *array, void *entry) {
    array->pdata = realloc(array->pdata, (array->len+1) * sizeof(void *));
    array->pdata[array->len++] = entry;
}

gboolean g_ptr_array_remove(GPtrArray *array, gpointer data) {
    int found = 0;
    for (guint i=0; i<array->len; i++) {
        if (found) {
            array->pdata[i-1] = array->pdata[i];
        } else if (!found && array->pdata[i] == data) {
            found = 1;
        }
    }
    if (found) {
        array->len--;
    }
    return found;
}

void g_ptr_array_free(GPtrArray *array, gboolean something) {
    free(array->pdata);
    free(array);
}

/* GList */

GList *g_list_append(GList *list, void *data) {
    GList *new_list = calloc(1, sizeof(GList));
    new_list->data = data;
    new_list->next = list;
    if (list)
        list->prev = new_list;
    return new_list;
}

GList *g_list_last(GList *list) {
    while (list && list->next) {
        list = list->next;
    }
    return list;
}

GList *g_list_remove(GList *list, void *data) {
    GList *link = list;
    while (link) {
        if (link->data == data) {
            GList *return_list = list;
            if (link->prev)
                link->prev->next = link->next;
            if (link->next)
                link->next->prev = link->prev;
            if (link == list)
                return_list = link->next;
            free(link);
            return return_list;
        }
        link = link->next;
    }
    return list;
}

void g_list_free(GList *list) {
    GList *next = NULL;
    while (list) {
        next = list->next;
        free(list);
        list = next;
    }
}

/* GOption */

void g_option_context_add_main_entries (GOptionContext *context,
        const GOptionEntry *entries,
        const gchar *translation_domain) {
    context->entries = entries;
}

gchar *g_option_context_get_help (GOptionContext *context,
        gboolean main_help, void *group) {
#if defined(__APPLE__) || defined(__FreeBSD__)
    const char * appname = getprogname();
#elif HAVE_DECL_PROGRAM_INVOCATION_SHORT_NAME
    const char * appname = program_invocation_short_name;
#else
    const char * appname = "mdb-util";
#endif

    char *help = malloc(4096);
    char *end = help + 4096;
    char *p = help;
    p += snprintf(p, end - p,
            "Usage:\n  %s [OPTION\xE2\x80\xA6] %s\n\n", appname, context->desc);
    p += snprintf(p, end - p,
            "Help Options:\n  -h, --%-20s%s\n\n", "help", "Show help options");
    p += snprintf(p, end - p,
            "Application Options:\n");
    int i=0;
    for (i=0; context->entries[i].long_name; i++) {
        p += snprintf(p, end - p, "  ");
        if (context->entries[i].short_name) {
            p += snprintf(p, end - p, "-%c, ", context->entries[i].short_name);
        }
        p += snprintf(p, end - p, "--");
        if (context->entries[i].arg_description) {
            char *long_name = g_strconcat(
                    context->entries[i].long_name, "=",
                    context->entries[i].arg_description, NULL);
            p += snprintf(p, end - p, "%-20s", long_name);
            free(long_name);
        } else {
            p += snprintf(p, end - p, "%-20s", context->entries[i].long_name);
        }
        if (!context->entries[i].short_name) {
            p += snprintf(p, end - p, "    ");
        }
        p += snprintf(p, end - p, "%s\n", context->entries[i].description);
    }
    p += snprintf(p, end - p, "\n");
    return help;
}

GOptionContext *g_option_context_new(const char *description) {
    GOptionContext *ctx = calloc(1, sizeof(GOptionContext));
    ctx->desc = description;
    return ctx;
}

gboolean g_option_context_parse(GOptionContext *context,
        gint *argc, gchar ***argv, GError **error) {
    int i;
    int count = 0;
    int len = 0;
    if (*argc == 2 &&
            (strcmp((*argv)[1], "-h") == 0 || strcmp((*argv)[1], "--help") == 0)) {
        fprintf(stderr, "%s", g_option_context_get_help(context, TRUE, NULL));
        exit(0);
    }
    for (i=0; context->entries[i].long_name; i++) {
        GOptionArg arg = context->entries[i].arg;
        count++;
        len++;
        if (arg == G_OPTION_ARG_STRING || arg == G_OPTION_ARG_INT) {
            len++;
        }
    }
    struct option *long_opts = calloc(count+1, sizeof(struct option));
    char *short_opts = calloc(1, len+1);
    int j=0;
    for (i=0; i<count; i++) {
        const GOptionEntry *entry = &context->entries[i];
        GOptionArg arg = entry->arg;
        short_opts[j++] = entry->short_name;
        if (arg == G_OPTION_ARG_STRING || arg == G_OPTION_ARG_INT) {
            short_opts[j++] = ':';
        }
        long_opts[i].name = entry->long_name;
        long_opts[i].has_arg = entry->arg == G_OPTION_ARG_NONE ? no_argument : required_argument;
    }
    int c;
    int longindex = 0;
    opterr = 0;
    while ((c = getopt_long(*argc, *argv, short_opts, long_opts, &longindex)) != -1) {
        if (c == '?') {
            *error = malloc(sizeof(GError));
            (*error)->message = malloc(100);
            if (optopt) {
                snprintf((*error)->message, 100, "Unrecognized option: -%c", optopt);
            } else {
                snprintf((*error)->message, 100, "Unrecognized option: %s", (*argv)[optind-1]);
            }
            free(short_opts);
            free(long_opts);
            return FALSE;
        }
        const GOptionEntry *entry = NULL;
        if (c == 0) {
            entry = &context->entries[longindex];
        } else {
            for (i=0; i<count; i++) {
                if (context->entries[i].short_name == c) {
                    entry = &context->entries[i];
                    break;
                }
            }
        }
        if (entry->arg == G_OPTION_ARG_NONE) {
            *(int *)entry->arg_data = !(entry->flags & G_OPTION_FLAG_REVERSE);
        } else if (entry->arg == G_OPTION_ARG_INT) {
            char *endptr = NULL;
            *(int *)entry->arg_data = strtol(optarg, &endptr, 10);
            if (*endptr) {
                *error = malloc(sizeof(GError));
                (*error)->message = malloc(100);
                snprintf((*error)->message, 100, "Argument to --%s must be an integer", entry->long_name);
                free(short_opts);
                free(long_opts);
                return FALSE;
            }
        } else if (entry->arg == G_OPTION_ARG_STRING) {
            *(char **)entry->arg_data = strdup(optarg);
        }
    }
    *argc -= (optind - 1);
    *argv += (optind - 1);
    free(short_opts);
    free(long_opts);

    return TRUE;
}

void g_option_context_free(GOptionContext *context) {
    free(context);
}
