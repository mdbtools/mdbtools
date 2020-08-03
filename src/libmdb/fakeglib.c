
#include "mdbfakeglib.h"

#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

/* string functions */

void *g_memdup(const void *src, size_t len) {
    void *dst = malloc(len);
    memcpy(dst, src, len);
    return dst;
}

int g_str_equal(const void *str1, const void *str2) {
    return strcmp(str1, str2) == 0;
}

char **g_strsplit(const char *haystack, const char *needle, int something) {
    char **ret = NULL;
    char *found = NULL;
    size_t components = 1;
    
    while ((found = strstr(haystack, needle))) {
        components++;
        haystack = found + strlen(needle);
    }

    ret = malloc(components * sizeof(char *));

    int i = 0;
    while ((found = strstr(haystack, needle))) {
        ret[i++] = strndup(haystack, found - haystack);
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

    char *pos = stpcpy(ret, first);

    va_start(argp, first);
    while ((arg = va_arg(argp, char *))) {
        pos = stpcpy(pos, arg);
    }
    va_end(argp);

    ret[len] = '\0';

    return ret;
}

char *g_strdup_printf(const char *format, ...) {
    char *ret = NULL;
    va_list argp;

    va_start(argp, format);
    vasprintf(&ret, format, argp);
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

/* GHashTable */

typedef struct MyNode {
    char *key;
    void *value;
} MyNode;

void *g_hash_table_lookup(GHashTable *table, const void *key) {
    int i;
    for (i=0; i<table->array->len; i++) {
        MyNode *node = g_ptr_array_index(table->array, i);
        if (table->compare(key, node->key))
            return node->value;
    }
    return NULL;
}

void g_hash_table_insert(GHashTable *table, void *key, void *value) {
    MyNode *node = calloc(1, sizeof(MyNode));
    node->value = value;
    node->key = key;
    g_ptr_array_add(table->array, node);
}

GHashTable *g_hash_table_new(GHashFunc hashes, GEqualFunc equals) {
    GHashTable *table = calloc(1, sizeof(GHashTable));
    table->array = g_ptr_array_new();
    table->compare = equals;
    return table;
}

void g_hash_table_foreach(GHashTable *table, GHFunc function, void *data) {
    int i;
    for (i=0; i<table->array->len; i++) {
        MyNode *node = g_ptr_array_index(table->array, i);
        function(node->key, node->value, data);
    }
}

void g_hash_table_destroy(GHashTable *table) {
    int i;
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
    int i;
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
            return list;
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
    /* TODO */
    return NULL;
}

GOptionContext *g_option_context_new(const char *description) {
    GOptionContext *ctx = calloc(1, sizeof(GOptionContext));
    ctx->desc = description;
    return ctx;
}

gboolean g_option_context_parse(GOptionContext *context,
        gint *argc, gchar ***argv, GError **error) {
    /* TODO */
    return FALSE;
}

void g_option_context_free(GOptionContext *context) {
    free(context);
}
