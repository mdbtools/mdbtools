#include "gmdb.h"

extern GtkWidget *app;
extern MdbHandle *mdb;

typedef struct GMdbInfoWindow {
	GtkWidget *window;
} GMdbInfoWindow;

GMdbInfoWindow *infow;

/* callbacks */
gint
gmdb_info_dismiss_cb(GtkWidget *w, gpointer data)
{
	g_free(infow);
	infow = NULL;

	return FALSE;
}

void
gmdb_info_add_keyvalue(GtkWidget *table, gint row, gchar *key, gchar *value)
{
GtkWidget *label;

	label = gtk_label_new(key);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_RIGHT);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1, 
		GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 5, 0);
	gtk_widget_show(label);
	label = gtk_label_new(value);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
	gtk_table_attach(GTK_TABLE(table), label, 1, 2, row, row+1, 
		GTK_FILL| GTK_EXPAND, GTK_FILL | GTK_EXPAND, 5, 0);
	gtk_widget_show(label);
}

GtkWidget *
gmdb_info_new()
{
GtkWidget *vbox;
GtkWidget *button;
GtkWidget *table;
char tmpstr[20];
struct stat st;

	if (infow) {
		return infow->window;
	}

	infow = g_malloc(sizeof(GMdbInfoWindow));
	infow->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);	
	gtk_window_set_title(GTK_WINDOW(infow->window), "File Info");
	gtk_widget_set_uposition(infow->window, 300,300);
	gtk_widget_show(infow->window);

	gtk_signal_connect ( GTK_OBJECT (infow->window),
		 "destroy", GTK_SIGNAL_FUNC (gmdb_info_dismiss_cb), infow->window);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(vbox);
	gtk_container_add(GTK_CONTAINER(infow->window), vbox);

	table = gtk_table_new(3,2,FALSE);
	gtk_widget_show(table);
	gmdb_info_add_keyvalue(table, 0, "File Name:", mdb->filename);
	gmdb_info_add_keyvalue(table, 1, "JET Version:", mdb->jet_version == MDB_VER_JET3 ? "3 (Access 97)" : "4 (Access 2000/XP)");
    assert( fstat(mdb->fd, &st)!=-1 );
    sprintf(tmpstr, "%ld", st.st_size);
	gmdb_info_add_keyvalue(table, 2, "File Size:", tmpstr);
	sprintf(tmpstr, "%ld", st.st_size / mdb->pg_size);
	gmdb_info_add_keyvalue(table, 3, "Number of Pages:", tmpstr);
	sprintf(tmpstr, "%d", mdb->num_catalog);
	gmdb_info_add_keyvalue(table, 4, "Number of Tables:", tmpstr);

	gtk_box_pack_start (GTK_BOX (vbox), table, TRUE, TRUE, 5);

	button = gtk_button_new_with_label("Close");
    gtk_widget_show (button);
	gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 5);

	gtk_signal_connect_object ( GTK_OBJECT (button),
		 "clicked", GTK_SIGNAL_FUNC (gtk_widget_destroy), 
		GTK_OBJECT(infow->window));

	return infow->window;
}
