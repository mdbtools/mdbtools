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
#include <mdbtools.h>

GtkWidget *file_selector;
MdbHandle *mdb;
extern int main_show_debug;
extern GladeXML *mainwin_xml;

void gmdb_file_open_recent_1() { gmdb_file_open_recent("menu_recent1"); }
void gmdb_file_open_recent_2() { gmdb_file_open_recent("menu_recent2"); }
void gmdb_file_open_recent_3() { gmdb_file_open_recent("menu_recent3"); }
void gmdb_file_open_recent_4() { gmdb_file_open_recent("menu_recent4"); }
void gmdb_file_open_recent(gchar *menuname) 
{ 
gchar *text, cfgname[100];

	sprintf(cfgname,"/gmdb/RecentFiles/%s.filepath", menuname);
	text = gnome_config_get_string(cfgname);
	gmdb_file_open(text);
	g_free(text);
	gmdb_load_recent_files();
}
static void
gmdb_file_shuffle_recent(gchar *file_path)
{
gchar *text, cfgname[100];
int i, index=0;

	for (i=1; i<=4; i++) {
		sprintf(cfgname,"/gmdb/RecentFiles/menu_recent%d.filepath", i);
		text = gnome_config_get_string(cfgname);
		if (text && !strcmp(text,file_path)) {
			index = i;
			break;
		}
		g_free(text);
	}
	printf("found file %slocation at menu %d\n",file_path, index);
	/* it is the most recently used file, we're done */
	if (index==1) return;

	/* if this file is not on the recent list bump item 4 */
	if (!index) index=4;

	for (i=1; i<index; i++) {
		sprintf(cfgname, "/gmdb/RecentFiles/menu_recent%d.filepath", i);
		text = gnome_config_get_string(cfgname);
		if (text) {
			sprintf(cfgname, 
				"/gmdb/RecentFiles/menu_recent%d.filepath", 
				i+1);
			gnome_config_set_string(cfgname, text);
			g_free(text);

			sprintf(cfgname, 
				"/gmdb/RecentFiles/menu_recent%d.basename", 
				i);
			text = gnome_config_get_string(cfgname);
			sprintf(cfgname, 
				"/gmdb/RecentFiles/menu_recent%d.basename", 
				i+1);
			gnome_config_set_string(cfgname, text);
			g_free(text);
		}
	}
}
static void
gmdb_file_add_recent(gchar *file_path)
{
	gchar basename[33];
	int i;

	for (i=strlen(file_path);i>=0 && file_path[i]!='/';i--);
	if (file_path[i]=='/') {
		strncpy(basename,&file_path[i+1],32);
	} else {
		strncpy(basename,file_path,32);
	}
	basename[33]='\0';
	gnome_config_set_string("/gmdb/RecentFiles/menu_recent1.basename", basename);
	gnome_config_set_string("/gmdb/RecentFiles/menu_recent1.filepath", file_path);
	gnome_config_sync();
}
void
gmdb_file_open(gchar *file_path)
{
	GtkWidget *win;
	gchar *file_name;
	gchar title[100];
	int i;

	gmdb_reset_widgets();
	mdb = mdb_open(file_path);
	if (!mdb) {
		gnome_warning_dialog("Unable to open file.");
		return;
	}
	gmdb_file_shuffle_recent(file_path);
	gmdb_file_add_recent(file_path);

	sql->mdb = mdb;
	mdb_read_catalog(mdb, MDB_ANY);
	gmdb_table_populate(mdb);
	gmdb_query_populate(mdb);
	gmdb_form_populate(mdb);
	gmdb_report_populate(mdb);
	gmdb_macro_populate(mdb);
	gmdb_module_populate(mdb);
	//if (main_show_debug) gmdb_debug_init(mdb);
	
	for (i=strlen(file_path);i>0 && file_path[i-1]!='/';i--);
	file_name=&file_path[i];

	win = (GtkWidget *) glade_xml_get_widget (mainwin_xml, "gmdb");
	g_snprintf(title, 100, "%s - MDB File Viewer",file_name);
	gtk_window_set_title(GTK_WINDOW(win), title);

	gmdb_set_sensitive(TRUE);
}

void
gmdb_file_open_cb(GtkWidget *selector, gpointer data)
{
gchar *file_path;
	file_path = (gchar *) gtk_file_selection_get_filename (GTK_FILE_SELECTION(file_selector));
	gmdb_file_open(file_path);
	gmdb_load_recent_files();
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
	gmdb_reset_widgets();
	gmdb_debug_close_all();
	gmdb_sql_close_all();
}
