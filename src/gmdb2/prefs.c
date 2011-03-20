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
#include "gmdb.h"

#include <libgnome/gnome-config.h>

extern GtkWidget *app;
extern MdbHandle *mdb;

GladeXML *prefswin_xml;

unsigned long
gmdb_prefs_get_maxrows()
{
	gchar *str;

        str = gnome_config_get_string("/gmdb/prefs/maxrows");
	if (!str || !strlen(str)) 
		return 1000;
	else 
		return atol(str);
}

/* callbacks */
static void
gmdb_prefs_help_cb(GtkWidget *w, gpointer data)
{
	GError *error = NULL;

	gnome_help_display("gmdb.xml", "gmdb-prefs", &error);
	if (error != NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}
}

static void
gmdb_prefs_save_cb(GtkWidget *w, GladeXML *xml)
{
	GtkWidget *entry;
	GtkWidget *win;
	gchar *str;

	entry = glade_xml_get_widget (xml, "maxrows_entry");
	str = (gchar *) gtk_entry_get_text(GTK_ENTRY(entry));
	printf("str = %s\n",str);
	gnome_config_set_string("/gmdb/prefs/maxrows", str);
	gnome_config_sync();
	win = glade_xml_get_widget (xml, "prefs_dialog");
	if (win) gtk_widget_destroy(win);
}

static void
gmdb_prefs_cancel_cb(GtkWidget *w, GladeXML *xml)
{
	GtkWidget *win;

	win = glade_xml_get_widget (xml, "prefs_dialog");
	if (win) gtk_widget_destroy(win);
}

GtkWidget *
gmdb_prefs_new()
{
	GtkWidget *prefswin, *button;
	GtkWidget *entry;
	gchar *str;

	/* load the interface */
	prefswin_xml = glade_xml_new(GMDB_GLADEDIR "gmdb-prefs.glade", NULL, NULL);
	/* connect the signals in the interface */
	glade_xml_signal_autoconnect(prefswin_xml);

	entry = glade_xml_get_widget (prefswin_xml, "maxrows_entry");

	button = glade_xml_get_widget (prefswin_xml, "cancel_button");
	g_signal_connect (G_OBJECT (button), "clicked",
		G_CALLBACK (gmdb_prefs_cancel_cb), prefswin_xml);

	button = glade_xml_get_widget (prefswin_xml, "ok_button");
	g_signal_connect (G_OBJECT (button), "clicked",
		G_CALLBACK (gmdb_prefs_save_cb), prefswin_xml);

	button = glade_xml_get_widget (prefswin_xml, "help_button");
	g_signal_connect (G_OBJECT (button), "clicked",
		G_CALLBACK (gmdb_prefs_help_cb), prefswin_xml);

	str = gnome_config_get_string("/gmdb/prefs/maxrows");
	if (!str || !strlen(str)) {
		str = "1000";
		gnome_config_set_string("/gmdb/prefs/maxrows", str);
		gnome_config_sync();
	}
	gtk_entry_set_text(GTK_ENTRY(entry), str);

	prefswin = glade_xml_get_widget (prefswin_xml, "prefs_dialog");
	return prefswin;
}
