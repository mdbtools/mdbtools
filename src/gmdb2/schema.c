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

#include <gtk/gtkmessagedialog.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-help.h>
#include "gmdb.h"

extern GtkWidget *app;
extern MdbHandle *mdb;

GladeXML *schemawin_xml;
static gchar backend[100];
static gchar tabname[MDB_MAX_OBJ_NAME+1];
static guint32 export_options;

#define ALL_TABLES   "(All Tables)"

static struct {
	const char *option_name;
	guint32     option_value;
} capabilities_xlt[] = {
	{ "drop_checkbox", MDB_SHEXP_DROPTABLE },
	{ "cstnotnull_checkbox", MDB_SHEXP_CST_NOTNULL },
	{ "cstnotempty_checkbox", MDB_SHEXP_CST_NOTEMPTY},
	{ "comments_checkbox", MDB_SHEXP_COMMENTS},
	{ "defaults_checkbox", MDB_SHEXP_DEFVALUES },
	{ "index_checkbox", MDB_SHEXP_INDEXES},
	{ "rel_checkbox", MDB_SHEXP_RELATIONS}
};
#define n_capabilities (sizeof(capabilities_xlt)/sizeof(capabilities_xlt[0]))

static void
gmdb_schema_export(const gchar *file_path)
{
FILE *outfile;

	GtkWidget *dlg;

	//printf("file path %s\n",file_path);
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
GtkWidget *schemawin, *checkbox, *chooser;
GtkComboBox *combobox;
const gchar *file_path;
gchar *tmp;
int i;

	schemawin = glade_xml_get_widget (schemawin_xml, "schema_dialog");

	chooser = glade_xml_get_widget (schemawin_xml, "filename_entry");
	file_path = gtk_entry_get_text(GTK_ENTRY(chooser));

	combobox = GTK_COMBO_BOX(glade_xml_get_widget(schemawin_xml, "table_combo"));
	tmp = gtk_combo_box_get_active_text(combobox);
	strncpy(tabname,tmp,MDB_MAX_OBJ_NAME);
	tabname[MDB_MAX_OBJ_NAME]=0;
	if (!strcmp(tabname,ALL_TABLES))
		tabname[0]='\0';

	combobox = GTK_COMBO_BOX(glade_xml_get_widget (schemawin_xml, "backend_combo"));
	tmp = gtk_combo_box_get_active_text(combobox);
	if (!strcmp(tmp, "Oracle"))
		strcpy(backend, "oracle");
	else if (!strcmp(tmp, "Sybase"))
		strcpy(backend,"sybase");
	else if (!strcmp(tmp,"MS SQL Server"))
		strcpy(backend,"sybase");
	else if (!strcmp(tmp, "PostgreSQL"))
		strcpy(backend,"postgres");
	else if (!strcmp(tmp,"MySQL"))
		strcpy(backend,"mysql");
	else if (!strcmp(tmp,"SQLite"))
		strcpy(backend,"sqlite");
	else
		strcpy(backend,"access");

	/* make sure unknown default options are enabled */
	export_options = MDB_SHEXP_DEFAULT;
	for (i=0; i<n_capabilities; ++i)
		export_options &= ~capabilities_xlt[i].option_value;

	/* fill the bit field from the checkboxes */
	for (i=0; i<n_capabilities; ++i) {
		checkbox = glade_xml_get_widget (schemawin_xml, capabilities_xlt[i].option_name);
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkbox)))
			export_options |= capabilities_xlt[i].option_value;
	}
	//printf("%s %s %02X\n",tabname,backend,export_options);

	gmdb_schema_export(file_path);
	gtk_widget_destroy(schemawin);
}
static void
check_default_options() {
	int i;
	GtkToggleButton *checkbox;
	for (i=0; i<n_capabilities; ++i) {
		checkbox = GTK_TOGGLE_BUTTON(glade_xml_get_widget (schemawin_xml, capabilities_xlt[i].option_name));
		gtk_toggle_button_set_active(checkbox, (MDB_SHEXP_DEFAULT & capabilities_xlt[i].option_value) != 0);
	}
}
static void
refresh_available_options() {
	GtkComboBox *combobox; 
	GtkWidget *checkbox;
	guint32 capabilities;
	extern GHashTable *mdb_backends; /* FIXME */
	MdbBackend *backend_obj;
	int i;

	combobox = GTK_COMBO_BOX(glade_xml_get_widget (schemawin_xml, "backend_combo"));
	if (!combobox) return; /* window is beeing destroyed */

	const gchar *backend_name = gtk_combo_box_get_active_text(combobox);
	if      (!strcmp(backend_name,"Oracle")) strcpy(backend,"oracle");
	else if (!strcmp(backend_name,"Sybase")) strcpy(backend,"sybase");
	else if (!strcmp(backend_name,"MS SQL Server")) strcpy(backend,"sybase");
	else if (!strcmp(backend_name,"PostgreSQL")) strcpy(backend,"postgres");
	else if (!strcmp(backend_name,"MySQL")) strcpy(backend,"mysql");
	else if (!strcmp(backend_name,"SQLite")) strcpy(backend,"sqlite");
	else strcpy(backend,"access");

	backend_obj = (MdbBackend *) g_hash_table_lookup(mdb_backends, backend);
	//printf("backend_obj: %p\n", backend_obj);

	capabilities = backend_obj->capabilities;
	//printf("backend capabilities: 0x%04x\n", capabilities);
	for (i=0; i<n_capabilities; ++i) {
		checkbox = glade_xml_get_widget (schemawin_xml, capabilities_xlt[i].option_name);
		gtk_widget_set_sensitive(checkbox, (capabilities & capabilities_xlt[i].option_value) != 0);
	}
}

void
gmdb_schema_backend_cb(GtkComboBox *widget, gpointer data)
{
	refresh_available_options();
}

void
gmdb_schema_help_cb(GtkWidget *w, gpointer data) 
{
	GError *error = NULL;

	gnome_help_display("gmdb.xml", "gmdb-schema", &error);
	if (error != NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}
}

void 
gmdb_schema_new_cb(GtkWidget *w, gpointer data) 
{
	GtkComboBox *combobox;
	MdbCatalogEntry *entry;
	int i;
   
	/* load the interface */
	schemawin_xml = glade_xml_new(GMDB_GLADEDIR "gmdb-schema.glade", NULL, NULL);
	/* connect the signals in the interface */
	glade_xml_signal_autoconnect(schemawin_xml);
	/* set up capabilities call back. TODO: autoconnect should do that */
	combobox = GTK_COMBO_BOX(glade_xml_get_widget(schemawin_xml, "backend_combo"));
	gtk_combo_box_set_active(combobox, 0);
	g_signal_connect( G_OBJECT (combobox), "changed",
		G_CALLBACK(gmdb_schema_backend_cb), NULL);
	
	/* set signals with user data, anyone know how to do this in glade? */
	combobox = GTK_COMBO_BOX(glade_xml_get_widget(schemawin_xml, "table_combo"));
	gtk_combo_box_append_text(combobox, ALL_TABLES);
	/* add all user tables in catalog to list */
	for (i=0; i < mdb->num_catalog; i++) {
		entry = g_ptr_array_index (mdb->catalog, i);
		if (mdb_is_user_table(entry)) {
			gtk_combo_box_append_text(combobox, entry->object_name);
		}
	} /* for */
	gtk_combo_box_set_active(combobox, 0);

	check_default_options();
	refresh_available_options();
}
