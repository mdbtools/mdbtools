#include <mdbtools.h>
#include <mdbsql.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <gnome.h>

#ifndef _gmdb_h_
#define _gmdb_h_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void gmdb_info_msg(gchar *message);

GtkWidget *gmdb_info_new();

GtkWidget *gmdb_table_data_new(MdbCatalogEntry *entry);
GtkWidget *gmdb_table_def_new(MdbCatalogEntry *entry);
GtkWidget *gmdb_table_export_new(MdbCatalogEntry *entry);

void gmdb_file_select_cb(GtkWidget *w, gpointer data);
void gmdb_file_open_cb(GtkWidget *w, gpointer data);
void gmdb_file_close_cb(GtkWidget *w, gpointer data);
void gmdb_file_open(gchar *file_path);

void gmdb_sql_new_window_cb(GtkWidget *w, gpointer data);

void gmdb_table_populate(MdbHandle *mdb);
void gmdb_form_populate(MdbHandle *mdb);
void gmdb_query_populate(MdbHandle *mdb);
void gmdb_report_populate(MdbHandle *mdb);
void gmdb_macro_populate(MdbHandle *mdb);
void gmdb_module_populate(MdbHandle *mdb);
void gmdb_debug_init(MdbHandle *mdb);

void gmdb_table_add_tab(GtkWidget *notebook);
void gmdb_debug_tab_new(GtkWidget *notebook);

extern GtkWidget *table_list;
extern GtkWidget *form_list;
extern GtkWidget *query_list;
extern GtkWidget *report_list;
extern GtkWidget *macro_list;
extern GtkWidget *module_list;

extern MdbSQL *sql;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#endif
