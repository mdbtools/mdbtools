#include "gmdb.h"
#include <glade/glade.h>

extern GladeXML* mainwin_xml;
GtkWidget *query_list;
extern GtkWidget *app;

void
gmdb_query_add_icon(gchar *text)
{
GnomeIconList *gil;

	gil = glade_xml_get_widget (mainwin_xml, "query_iconlist");
	gnome_icon_list_append(gil, "query_big.xpm", text);

}

void gmdb_query_populate(MdbHandle *mdb)
{
int   i;
MdbCatalogEntry *entry;

	/* loop over each entry in the catalog */
	for (i=0; i < mdb->num_catalog; i++) {
		entry = g_ptr_array_index (mdb->catalog, i);

 		/* if it's a query */
		if (entry->object_type == MDB_QUERY) {
			gmdb_query_add_icon(entry->object_name);
		} /* if MDB_QUERY */
	} /* for */
}
