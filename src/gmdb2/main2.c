/* MDB Tools - A library for reading MS Access database file
 * Copyright (C) 2000 Brian Bruns
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <gnome.h>
#include <glade/glade.h>
#include <mdbtools.h>
#include <mdbsql.h>

GtkWidget *app;
GladeXML *mainwin_xml;
MdbSQL *sql;

/* called when the user closes the window */
gint
delete_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	/* signal the main loop to quit */
	gtk_main_quit();
	/* return FALSE to continue closing the window */
	return FALSE;
}


void
gmdb_about_cb(GtkWidget *button, gpointer data)
{
const gchar *authors[] = {
	"Brian Bruns",
	NULL
};
const gchar *documenters[] = {
	"Brian Bruns",
	NULL
};
GdkPixbuf *pixbuf;

pixbuf = gdk_pixbuf_new_from_file ("logo.xpm", NULL);

gtk_widget_show (gnome_about_new ("Gnome MDB Viewer", "0.2",
                 "Copyright 2002-2003 Brian Bruns",
                 _("The Gnome-MDB Viewer is the grapical interface to "
                   "MDB Tools.  It lets you view and export data and schema"
		   "from MDB files produced by MS Access 97/2000/XP."),
                  (const gchar **) authors,
                  (const gchar **) authors,
		   NULL,
                   pixbuf));
}
void
gmdb_info_cb(GtkWidget *button, gpointer data)
{
	gmdb_info_new();
}


/* a callback for the buttons */
void
a_callback(GtkWidget *button, gpointer data)
{

	        /*just print a string so that we know we got there*/
	        g_print("Inside Callback\n");
}

void gmdb_load_recent_files()
{
GtkWidget *menuitem;
GtkWidget *menulabel;
char menuname[100];
char cfgname[100];
int i;
gchar *text, *text2;

	for (i=4;i>=1;i--) {	
		sprintf(menuname, "menu_recent%d",i);
		sprintf(cfgname, "/gmdb/RecentFiles/menu_recent%d.basename",i);
		menuitem = glade_xml_get_widget (mainwin_xml, menuname);
		menulabel = gtk_bin_get_child(GTK_BIN(menuitem));
		text = gnome_config_get_string(cfgname);
		if (!text) {
			gtk_widget_hide(menuitem);
		} else {
			text2 = g_malloc(strlen(text)+4);
			sprintf(text2,"%d. %s",i,text);
			gtk_label_set_text(GTK_LABEL(menulabel),text2);
			gtk_widget_show(menuitem);
			g_free(text2);
		}
		g_free(text);
	}
}
void
gmdb_reset_widgets()
{
GnomeIconList *gil;
int pos;

	gil = (GnomeIconList *) glade_xml_get_widget (mainwin_xml, "table_iconlist");
	gnome_icon_list_clear(gil);
	gil = (GnomeIconList *) glade_xml_get_widget (mainwin_xml, "query_iconlist");
	gnome_icon_list_clear(gil);
	gil = (GnomeIconList *) glade_xml_get_widget (mainwin_xml, "form_iconlist");
	gnome_icon_list_clear(gil);
	gil = (GnomeIconList *) glade_xml_get_widget (mainwin_xml, "report_iconlist");
	gnome_icon_list_clear(gil);
	gil = (GnomeIconList *) glade_xml_get_widget (mainwin_xml, "macro_iconlist");
	gnome_icon_list_clear(gil);
	gil = (GnomeIconList *) glade_xml_get_widget (mainwin_xml, "module_iconlist");
	gnome_icon_list_clear(gil);
}
	
void gmdb_init_popups()
{
		gmdb_table_init_popup();
}

int main(int argc, char *argv[]) 
{
GtkWidget *gmdb;

#ifdef SQL
	/* initialize the SQL engine */
	sql = mdb_sql_init();
#endif
	/* initialize MDB Tools library */
	mdb_init();

        /* Initialize GNOME */
	gnome_init ("gmdb2", "0.2", argc, argv);
	app = gnome_app_new ("gmdb2", "Gnome-MDB File Viewer");

	glade_init();

	/* load the interface */
	mainwin_xml = glade_xml_new("gladefiles/gmdb.glade", NULL, NULL);
	/* connect the signals in the interface */
	glade_xml_signal_autoconnect(mainwin_xml);

	gmdb = glade_xml_get_widget (mainwin_xml, "gmdb");
	gtk_signal_connect (GTK_OBJECT (gmdb), "delete_event",
		GTK_SIGNAL_FUNC (delete_event), NULL);

	if (argc>1) {
		gmdb_file_open(argv[1]);
	}

	gmdb_load_recent_files();
	gmdb_init_popups();

	/* start the event loop */
	gtk_main();

#ifdef SQL
	/* free MDB Tools library */
	mdb_sql_exit(sql);
#endif
	mdb_exit();

	return 0;
}
