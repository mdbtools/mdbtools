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

/* callbacks */
void
gmdb_table_def_cb(GtkList *list, GtkWidget *w, gpointer data)
{
MdbCatalogEntry *entry;

	printf("here\n");
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
gmdb_table_select_cb(GnomeIconList *gil, int num, GdkEvent *ev, gpointer data)
{
int child_num;
int i,j=0;
MdbCatalogEntry *entry;
gchar *text;

	text = (gchar *) gnome_icon_list_get_icon_data(gil, num);
	printf ("text !%s!\n",text);

	for (i=0;i<mdb->num_catalog;i++) {
		entry = g_ptr_array_index(mdb->catalog,i);
		if (entry->object_type==MDB_TABLE && 
		   !strcmp(entry->object_name,text)) {
			selected_table = i;
		}
	}
}
/* functions */
void
gmdb_table_add_icon(gchar *text)
{
GnomeIconList *gil;
int pos;

	gil = glade_xml_get_widget (mainwin_xml, "table_iconlist");

	pos = gnome_icon_list_append(gil, "table_big.xpm", text);
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
