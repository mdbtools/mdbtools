#include "gmdb.h"
#include <mdbtools.h>

GtkWidget *file_selector;
MdbHandle *mdb;
extern int main_show_debug;

void
gmdb_file_open(gchar *file_path)
{
	mdb = mdb_open(file_path, MDB_NOFLAGS);
	if (!mdb) {
		gmdb_info_msg("Unable to open file.");
		return;
	}
	sql->mdb = mdb;
	mdb_read_catalog(mdb, MDB_TABLE);
	gmdb_table_populate(mdb);
	gmdb_query_populate(mdb);
	gmdb_form_populate(mdb);
	gmdb_report_populate(mdb);
	gmdb_macro_populate(mdb);
	gmdb_module_populate(mdb);
	if (main_show_debug) gmdb_debug_init(mdb);
}

void
gmdb_file_open_cb(GtkWidget *selector, gpointer data)
{
gchar *file_path;
	file_path = gtk_file_selection_get_filename (GTK_FILE_SELECTION(file_selector));
	gmdb_file_open(file_path);
}


void
gmdb_file_select_cb(GtkWidget *button, gpointer data)
{
	/*just print a string so that we know we got there*/
	file_selector = gtk_file_selection_new("Please select a database.");

   	gtk_signal_connect (
		GTK_OBJECT (GTK_FILE_SELECTION(file_selector)->ok_button),
   		"clicked", GTK_SIGNAL_FUNC (gmdb_file_open_cb), NULL);
   
	gtk_signal_connect_object ( GTK_OBJECT (
		GTK_FILE_SELECTION(file_selector)->ok_button),
   		"clicked", GTK_SIGNAL_FUNC (gtk_widget_destroy),
   		(gpointer) file_selector);

   	gtk_signal_connect_object (
		GTK_OBJECT (GTK_FILE_SELECTION(file_selector)->cancel_button),
   			"clicked", GTK_SIGNAL_FUNC (gtk_widget_destroy),
   			(gpointer) file_selector);
   
	gtk_widget_show (file_selector);
}
void
gmdb_file_close_cb(GtkWidget *button, gpointer data)
{
}
