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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <gtk/gtkiconview.h>
#include <glade/glade.h>
#include "gmdb.h"

extern GladeXML* mainwin_xml;
extern MdbHandle *mdb;
int selected_table = -1;

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
	gmdb_debug_new_cb(w, (gpointer) &entry->table_pg);
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
gmdb_table_select_cb (GtkIconView* giv, gpointer data) {
	int i;
	MdbCatalogEntry *entry;
	gchar *text;
	GList *selection;
	GtkTreeModel *store;
	GtkTreePath *path;
	GtkTreeIter iter;

	selected_table = -1;

	selection = gtk_icon_view_get_selected_items (giv);
	if (g_list_length (selection) < 1) {
		gmdb_table_set_sensitive (FALSE);
		g_list_free (selection);
		return;
	}

	store = gtk_icon_view_get_model (giv);
	path = (GtkTreePath*) selection->data;
	if (!gtk_tree_model_get_iter (store, &iter, path)) {
		/* FIXME */
		g_error ("Failed to get selection iter!!!");
		g_list_foreach (selection, (GFunc) gtk_tree_path_free, NULL);
		g_list_free (selection);
	}

	gtk_tree_model_get (store, &iter, 1, &text, -1);
	for (i=0;i<mdb->num_catalog;i++) {
		entry = g_ptr_array_index(mdb->catalog,i);
		if (entry->object_type==MDB_TABLE && 
		   !strcmp(entry->object_name,text)) {
			selected_table = i;
		}
	}
	g_free (text);
	if (selected_table>0) {
		gmdb_table_set_sensitive(TRUE);
	} else {
		gmdb_table_set_sensitive(FALSE);
	}

	g_list_foreach (selection, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (selection);
}

static gboolean
gmdb_table_popup_cb (GtkIconView* giv, GdkEvent* event, gpointer data) {
	GtkWidget* menu = GTK_WIDGET (data);
	GdkEventButton *event_button;

	if (event->type == GDK_BUTTON_PRESS) {
		event_button = (GdkEventButton *) event;
		if (event_button->button == 3) {
			GtkTreePath *path = gtk_icon_view_get_path_at_pos (giv, (gint) event_button->x, (gint) event_button->y);
			if (path) {
				gtk_icon_view_select_path (giv, path);
				gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, event_button->button, event_button->time);
				return TRUE;
			}
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
gmdb_table_init_popup (GtkWidget* w) {
GtkWidget *menu, *mi;
	GtkIconView *giv = GTK_ICON_VIEW (w);

	menu = gtk_menu_new();
	gtk_widget_show(menu);
	mi = gtk_menu_item_new_with_label("Definition");
	gtk_widget_show(mi);
	g_signal_connect_swapped (G_OBJECT (mi), "activate",
		G_CALLBACK (gmdb_table_def_cb), giv);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
	mi = gtk_menu_item_new_with_label("Data");
	gtk_widget_show(mi);
	g_signal_connect_swapped (G_OBJECT (mi), "activate",
		G_CALLBACK (gmdb_table_data_cb), giv);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
	mi = gtk_menu_item_new_with_label("Export");
	gtk_widget_show(mi);
	g_signal_connect_swapped (G_OBJECT (mi), "activate",
		G_CALLBACK (gmdb_table_export_cb), giv);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
	mi = gtk_separator_menu_item_new();
	gtk_widget_show(mi);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
	mi = gtk_menu_item_new_with_label("Debug");
	gtk_widget_show(mi);
	g_signal_connect_swapped (G_OBJECT (mi), "activate",
		G_CALLBACK (gmdb_table_debug_cb), giv);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
	//mi = gtk_menu_item_new_with_label("Usage Map");
	//gtk_widget_show(mi);
	//gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);

	g_signal_connect (giv, "button_press_event", G_CALLBACK (gmdb_table_popup_cb), menu);
}	
