#include <mdbtools.h>
#include <mdbsql.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <glade/glade.h>

#ifndef _gmdb_h_
#define _gmdb_h_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void gmdb_info_msg(gchar *message);
GtkWidget *gmdb_info_new(void);

/* main2.c */
void gmdb_info_cb(GtkWidget *w, gpointer data);
void gmdb_prefs_cb(GtkWidget *w, gpointer data);
void gmdb_help_cb(GtkWidget *w, gpointer data);
void gmdb_about_cb(GtkWidget *w, gpointer data);
void gmdb_load_recent_files(void);

GtkWidget *gmdb_table_data_new(MdbCatalogEntry *entry);
GtkWidget *gmdb_table_def_new(MdbCatalogEntry *entry);
GtkWidget *gmdb_table_export_new(MdbCatalogEntry *entry);
void gmdb_table_export(MdbCatalogEntry *entry) ;

void gmdb_table_export_populate_dialog(GladeXML *xml);

/* file.c */
void gmdb_file_select_cb(GtkWidget *w, gpointer data);
void gmdb_file_close_cb(GtkWidget *w, gpointer data);
void gmdb_file_open_recent_1(void);
void gmdb_file_open_recent_2(void);
void gmdb_file_open_recent_3(void);
void gmdb_file_open_recent_4(void);
void gmdb_file_open(gchar *file_path);

void gmdb_sql_new_window_cb(GtkWidget *w, gpointer data);

void gmdb_table_add_tab(GtkWidget *notebook);
void gmdb_debug_tab_new(GtkWidget *notebook);

/* debug.c */
void gmdb_debug_jump_cb(GtkWidget *w, gpointer data);
void gmdb_debug_jump_msb_cb(GtkWidget *w, gpointer data);
void gmdb_debug_display_cb(GtkWidget *w, gpointer data);
void gmdb_debug_close_cb(GtkWidget *w, gpointer data);
void gmdb_debug_forward_cb(GtkWidget *w, gpointer data);
void gmdb_debug_back_cb(GtkWidget *w, gpointer data);
void gmdb_debug_new_cb(GtkWidget *w, gpointer data);
void gmdb_debug_set_dissect_cb(GtkWidget *w, gpointer data);
void gmdb_debug_close_all(void);

/* sql.c */
void gmdb_sql_save_as_cb(GtkWidget *w, GladeXML *xml);
void gmdb_sql_new_cb(GtkWidget *w, gpointer data);
void gmdb_sql_close_all(void);
void gmdb_sql_save_query(GladeXML *xml, gchar *file_path);

unsigned long gmdb_prefs_get_maxrows(void);

extern GtkWidget *gmdb_prefs_new(void);

/* schema.c */
void gmdb_schema_new_cb(GtkWidget *w, gpointer data);
void gmdb_schema_export_cb(GtkWidget *w, gpointer data);
void gmdb_schema_help_cb(GtkWidget *w, gpointer data);

/* table.c */
void gmdb_table_debug_cb(GtkList *list, GtkWidget *w, gpointer data);
void gmdb_table_export_cb(GtkList *list, GtkWidget *w, gpointer data);
void gmdb_table_def_cb(GtkList *list, GtkWidget *w, gpointer data);
void gmdb_table_unselect_cb (GtkIconView*, gpointer);
void gmdb_table_select_cb (GtkIconView*, gpointer);
void gmdb_table_data_cb(GtkList *list, GtkWidget *w, gpointer data);
void gmdb_table_init_popup (GtkWidget*);
void gmdb_table_set_sensitive(gboolean b);

/* table_export.c */
void gmdb_export_help_cb(GtkWidget *w, gpointer data);
void gmdb_table_export_button_cb(GtkWidget *w, gpointer data);
void gmdb_print_quote(FILE *outfile, int need_quote, char quotechar, char *colsep, char *str);
void gmdb_export_get_delimiter(GladeXML *xml, gchar *delimiter, int max_buf);
void gmdb_export_get_lineterm(GladeXML *xml, gchar *lineterm, int max_buf);
int gmdb_export_get_quote(GladeXML *xml);
char gmdb_export_get_quotechar(GladeXML *xml);
int gmdb_export_get_headers(GladeXML *xml);
gchar *gmdb_export_get_filepath(GladeXML *xml);

extern MdbSQL *sql;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#endif
