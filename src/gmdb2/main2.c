/* MDB Tools - A library for reading MS Access database file
 * Copyright (C) 2000-2004 Brian Bruns
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
#include <libgnome/gnome-help.h>
#include <glade/glade.h>
#include <mdbtools.h>
#include <mdbver.h>
#include <mdbsql.h>
#include "gmdb.h"

GtkWidget *app;
GladeXML *mainwin_xml;
MdbSQL *sql;

/* called when the user closes the window */
static gint
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
	"Jeff Smith",
	"Filip Van Raemdonck",
	NULL
};
const gchar *documenters[] = {
	"Brian Bruns",
	NULL
};
GtkWidget *parent;
GdkPixbuf *pixbuf=NULL;

	parent = gtk_widget_get_toplevel (button);
	if (!GTK_WIDGET_TOPLEVEL (parent))
		parent = NULL;

	if (!pixbuf)
		pixbuf = gdk_pixbuf_new_from_file (GMDB_ICONDIR "logo.xpm", NULL);

  	gtk_show_about_dialog (parent,
   		"authors", authors,
		"comments", _("GNOME MDB Viewer is a grapical interface to "
			"MDB Tools. It lets you view and export data and schema "
			"from MDB files produced by MS Access 97/2000/XP/2003."),
		"copyright", _("Copyright 2002-2006 Brian Bruns and others"),
		"documenters", documenters,
		"logo", pixbuf,
		"name", _("GNOME MDB Viewer"),
		"version", MDB_VERSION_NO,
		"website", "http://mdbtools.sourceforge.net/",
		NULL);
}

void
gmdb_prefs_cb(GtkWidget *button, gpointer data)
{
	gmdb_prefs_new();
}
void
gmdb_info_cb(GtkWidget *button, gpointer data)
{
	gmdb_info_new();
}


/* a callback for the buttons */
static void
a_callback(GtkWidget *button, gpointer data)
{

	        /*just print a string so that we know we got there*/
	        g_print("Inside Callback\n");
}
void
gmdb_help_cb(GtkWidget *button, gpointer data)
{
	GError *error = NULL;

	gnome_help_display("gmdb.xml", NULL, &error);
	if (error != NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}

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

static void
gmdb_load_icons(GladeXML *xml)
{
	GtkWidget *icon;
	char file[256];
	GValue value = { 0, };
	int i = 0;

	char *icons[] = {
		"table_icon",
		"query_icon",
		"form_icon",
		"report_icon",
		"macro_icon",
		"module_icon",
		"debug_icon",
		NULL
	};
	char *files[] = {
		"table.xpm",
		"query.xpm",
		"forms.xpm",
		"reports.xpm",
		"macros.xpm",
		"code.xpm",
		"debug.xpm",
		NULL
	};

	while (icons[i]) {
		icon =  glade_xml_get_widget (xml, icons[i]); 

		g_value_init (&value, G_TYPE_STRING);
		g_snprintf(file,256,"%s%s", GMDB_ICONDIR, files[i]); 
		g_value_set_static_string (&value, file);
		g_object_set_property (G_OBJECT (icon), "file" , &value);
		g_value_unset (&value);
		i++;
	}
}
int main(int argc, char *argv[]) 
{
GtkWidget *gmdb;
GnomeProgram *program;

#ifdef SQL
	/* initialize the SQL engine */
	sql = mdb_sql_init();
#endif
	/* initialize MDB Tools library */
	mdb_init();

        /* Initialize GNOME */
	       /* Initialize gnome program */
	program = gnome_program_init ("gmdb", "0.2",
		LIBGNOMEUI_MODULE, argc, argv,
		GNOME_PARAM_POPT_TABLE, NULL,
		GNOME_PARAM_HUMAN_READABLE_NAME,
		_("Gnome-MDB File Viewer"),
		GNOME_PARAM_APP_DATADIR, DATADIR,
		NULL);
	//gnome_init ("gmdb", "0.2", argc, argv);
	//app = gnome_app_new ("gmdb", "Gnome-MDB File Viewer");
	glade_init();

	/* load the interface */
	mainwin_xml = glade_xml_new(GMDB_GLADEDIR "gmdb.glade", NULL, NULL);
	/* connect the signals in the interface */
	glade_xml_signal_autoconnect(mainwin_xml);

	gmdb = glade_xml_get_widget (mainwin_xml, "gmdb");
	gtk_signal_connect (GTK_OBJECT (gmdb), "delete_event",
		GTK_SIGNAL_FUNC (delete_event), NULL);

	//gmdb_load_icons(mainwin_xml);

	if (argc>1) {
		gmdb_file_open(argv[1]);
	}

	gmdb_load_recent_files();

	/* start the event loop */
	gtk_main();

#ifdef SQL
	/* free MDB Tools library */
	mdb_sql_exit(sql);
#endif
	mdb_exit();

	return 0;
}
