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
#include <glade/glade.h>

GtkWidget *table_list;
GtkWidget *table_data_window;
GtkWidget *table_def_window;
extern GtkWidget *app;
extern GladeXML* mainwin_xml;
extern MdbHandle *mdb;
int selected_table = -1;
extern char *mdb_access_types[];

void gmdb_table_set_sensitive(gboolean b);

/* callbacks */
void
gmdb_table_debug_cb(GtkList *list, GtkWidget *w, gpointer data)
{
MdbCatalogEntry *entry;

	/* nothing selected yet? */
	if (selected_table==-1) {
		return;
	}

	entry = g_ptr_array_index(mdb->catalog,selected_table);
	gmdb_debug_new_cb(w, &entry->table_pg);
}
void
gmdb_table_def_cb(GtkList *list, GtkWidget *w, gpointer data)
{
MdbCatalogEntry *entry;

	/* nothing selected yet? */
	if (selected_table==-1) {
		return;
	}

	entry = g_ptr_array_index(mdb->catalog,selected_table);

	gmdb_table_def_new(entry);
}
void
gmdb_table_export_cb(GtkList *list, GtkWidget *w, gpointer data)
{
MdbCatalogEntry *entry;

	/* nothing selected yet? */
	if (selected_table==-1) {
		return;
	}

	entry = g_ptr_array_index(mdb->catalog,selected_table);

	gmdb_table_export(entry);
}
void
gmdb_table_data_cb(GtkList *list, GtkWidget *w, gpointer data)
{
MdbCatalogEntry *entry;

	/* nothing selected yet? */
	if (selected_table==-1) {
		return;
	}

	entry = g_ptr_array_index(mdb->catalog,selected_table);

	gmdb_table_data_new(entry);
}
void
gmdb_table_unselect_cb(GnomeIconList *gil, int num, GdkEvent *ev, gpointer data)
{
	selected_table = -1;
	gmdb_table_set_sensitive(FALSE);
}
void
gmdb_table_select_cb(GnomeIconList *gil, int num, GdkEvent *ev, gpointer data)
{
int i;
MdbCatalogEntry *entry;
gchar *text;

	text = (gchar *) gnome_icon_list_get_icon_data(gil, num);

	selected_table = -1;
	
	for (i=0;i<mdb->num_catalog;i++) {
		entry = g_ptr_array_index(mdb->catalog,i);
		if (entry->object_type==MDB_TABLE && 
		   !strcmp(entry->object_name,text)) {
			selected_table = i;
		}
	}
	if (selected_table>0) {
		gmdb_table_set_sensitive(TRUE);
	} else {
		gmdb_table_set_sensitive(FALSE);
	}
	
}
gmdb_table_popup_cb(GtkWidget *menu, GdkEvent *event)
{
GdkEventButton *event_button;
//GtkWidget *menu;
	
	if (event->type == GDK_BUTTON_PRESS) {
		event_button = (GdkEventButton *) event;
		if (event_button->button == 3) {
			gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 
				event_button->button, event_button->time);
			return TRUE;
		}
	}
	return FALSE;
}
/* functions */
void
gmdb_table_set_sensitive(gboolean b)
{
	GtkWidget *button;

	button = (GtkWidget *) glade_xml_get_widget (mainwin_xml, "table_definition");
	gtk_widget_set_sensitive(button,b);

	button = (GtkWidget *) glade_xml_get_widget (mainwin_xml, "table_data");
	gtk_widget_set_sensitive(button,b);

	button = (GtkWidget *) glade_xml_get_widget (mainwin_xml, "table_export");
	gtk_widget_set_sensitive(button,b);
}
void
gmdb_table_init_popup()
{
GnomeIconList *gil;
GtkWidget *menu, *mi;

	gil = (GnomeIconList *) glade_xml_get_widget (mainwin_xml, "table_iconlist");

	menu = gtk_menu_new();
	gtk_widget_show(menu);
	mi = gtk_menu_item_new_with_label("Definition");
	gtk_widget_show(mi);
	g_signal_connect_swapped (G_OBJECT (mi), "activate",
		G_CALLBACK (gmdb_table_def_cb), gil);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
	mi = gtk_menu_item_new_with_label("Data");
	gtk_widget_show(mi);
	g_signal_connect_swapped (G_OBJECT (mi), "activate",
		G_CALLBACK (gmdb_table_data_cb), gil);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
	mi = gtk_menu_item_new_with_label("Export");
	gtk_widget_show(mi);
	g_signal_connect_swapped (G_OBJECT (mi), "activate",
		G_CALLBACK (gmdb_table_export_cb), gil);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
	mi = gtk_separator_menu_item_new();
	gtk_widget_show(mi);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
	mi = gtk_menu_item_new_with_label("Debug");
	gtk_widget_show(mi);
	g_signal_connect_swapped (G_OBJECT (mi), "activate",
		G_CALLBACK (gmdb_table_debug_cb), gil);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
	//mi = gtk_menu_item_new_with_label("Usage Map");
	//gtk_widget_show(mi);
	//gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);

	g_signal_connect_swapped (GTK_OBJECT (gil), "button_press_event",
		G_CALLBACK (gmdb_table_popup_cb), GTK_OBJECT(menu));
}	
void
gmdb_table_add_icon(gchar *text)
{
GnomeIconList *gil;
int pos;

	gil = (GnomeIconList *) glade_xml_get_widget (mainwin_xml, "table_iconlist");

	pos = gnome_icon_list_append(gil, GMDB_ICONDIR "table_big.xpm", text);
	gnome_icon_list_set_icon_data(gil, pos, text);
}

void gmdb_table_populate(MdbHandle *mdb)
{
int   i;
MdbCatalogEntry *entry;

	/* loop over each entry in the catalog */
	for (i=0; i < mdb->num_catalog; i++) {
		entry = g_ptr_array_index (mdb->catalog, i);

 		/* if it's a table */
		if (entry->object_type == MDB_TABLE) {
			/* skip the MSys tables */
			if (strncmp (entry->object_name, "MSys", 4)) {
				/* add table to tab */
				gmdb_table_add_icon(entry->object_name);
	     	}
		} /* if MDB_TABLE */
	} /* for */
}
