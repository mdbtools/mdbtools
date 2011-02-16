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

#include <gtk/gtkmessagedialog.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-help.h>

extern GtkWidget *app;
extern MdbHandle *mdb;

GladeXML *schemawin_xml;
static gchar backend[100];
static gchar tabname[MDB_MAX_OBJ_NAME+1];
static gchar file_path[PATH_MAX+1];
static gchar relation;
static gchar drops;
static guint32 export_options;

#define ALL_TABLES   "(All Tables)"

static void
gmdb_schema_export()
{
FILE *outfile;

	GtkWidget *dlg;

	printf("file path %s\n",file_path);
	if ((outfile=fopen(file_path, "w"))==NULL) {
		GtkWidget* dlg = gtk_message_dialog_new (NULL,
		    GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_WARNING, GTK_BUTTONS_CLOSE,
		    _("Unable to open file."));
		gtk_dialog_run (GTK_DIALOG (dlg));
		gtk_widget_destroy (dlg);
		return;
	}
	mdb_set_default_backend(mdb,backend);

	mdb_print_schema(mdb, outfile, *tabname?tabname:NULL, NULL, export_options);

	fclose(outfile);
	dlg = gtk_message_dialog_new (NULL,
	    GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_WARNING, GTK_BUTTONS_CLOSE,
	    _("Schema exported successfully."));
	gtk_dialog_run (GTK_DIALOG (dlg));
	gtk_widget_destroy (dlg);
}
void
gmdb_schema_export_cb(GtkWidget *w, gpointer data)
{
GtkWidget *schemawin, *combo, *checkbox, *entry;

	schemawin = glade_xml_get_widget (schemawin_xml, "schema_dialog");

	entry = glade_xml_get_widget (schemawin_xml, "filename_entry");
	strncpy(file_path,gtk_entry_get_text(GTK_ENTRY(entry)),PATH_MAX);
	file_path[PATH_MAX]=0;

	combo = glade_xml_get_widget (schemawin_xml, "table_combo");
	strncpy(tabname,gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(combo)->entry)),MDB_MAX_OBJ_NAME);
	tabname[MDB_MAX_OBJ_NAME]=0;
	if (!strcmp(tabname,ALL_TABLES)) tabname[0]='\0';

	combo = glade_xml_get_widget (schemawin_xml, "backend_combo");
	if (!strcmp(gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(combo)->entry)),"Oracle")) strcpy(backend,"oracle");
	else if (!strcmp(gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(combo)->entry)),"Sybase")) strcpy(backend,"sybase");
	else if (!strcmp(gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(combo)->entry)),"MS SQL Server")) strcpy(backend,"sybase");
	else if (!strcmp(gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(combo)->entry)),"PostgreSQL")) strcpy(backend,"postgres");
	else if (!strcmp(gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(combo)->entry)),"MySQL")) strcpy(backend,"mysql");
	else strcpy(backend,"access");

	export_options = MDB_SHEXP_DEFAULT & ~ (MDB_SHEXP_RELATIONS|MDB_SHEXP_DROPTABLE);
	checkbox = glade_xml_get_widget (schemawin_xml, "rel_checkbox");
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkbox)))
		export_options |= MDB_SHEXP_RELATIONS;
	checkbox = glade_xml_get_widget (schemawin_xml, "drop_checkbox");
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkbox)))
		export_options |= MDB_SHEXP_DROPTABLE;
	// TODO add support for other options
	//printf("%s %s %02X\n",tabname,backend,export_options);

	gtk_widget_destroy(schemawin);
	gmdb_schema_export();
}
void
gmdb_schema_help_cb(GtkWidget *w, gpointer data) 
{
	GError *error = NULL;

	gnome_help_display("gmdb.xml", "gmdb-schema", &error);
	if (error != NULL) {
		g_warning (error->message);
		g_error_free (error);
	}
}

void 
gmdb_schema_new_cb(GtkWidget *w, gpointer data) 
{
	GList *glist = NULL;
	GtkWidget *combo;
	MdbCatalogEntry *entry;
	int i;
   
	/* load the interface */
	schemawin_xml = glade_xml_new(GMDB_GLADEDIR "gmdb-schema.glade", NULL, NULL);
	/* connect the signals in the interface */
	glade_xml_signal_autoconnect(schemawin_xml);
	
	/* set signals with user data, anyone know how to do this in glade? */
	combo = glade_xml_get_widget (schemawin_xml, "table_combo");

	glist = g_list_append(glist, ALL_TABLES);
	/* add all user tables in catalog to list */
	for (i=0; i < mdb->num_catalog; i++) {
		entry = g_ptr_array_index (mdb->catalog, i);
		if (mdb_is_user_table(entry)) {
			glist = g_list_append(glist, entry->object_name);
		}
	} /* for */
	gtk_combo_set_popdown_strings(GTK_COMBO(combo), glist);
	g_list_free(glist);
}
