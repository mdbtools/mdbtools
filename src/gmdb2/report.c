#include "gmdb.h"
#include <glade/glade.h>

extern GladeXML* mainwin_xml;
extern GtkWidget *app;

void
gmdb_report_add_icon(gchar *text)
{
GnomeIconList *gil;

        gil = glade_xml_get_widget (mainwin_xml, "report_iconlist");
        gnome_icon_list_append(gil, "report_big.xpm", text);
}

void gmdb_report_populate(MdbHandle *mdb)
{
int   i;
MdbCatalogEntry *entry;

	/* loop over each entry in the catalog */
	for (i=0; i < mdb->num_catalog; i++) {
		entry = g_ptr_array_index (mdb->catalog, i);

 		/* if it's a report */
		if (entry->object_type == MDB_REPORT) {
			/* add table to tab */
			gmdb_report_add_icon(entry->object_name);
		} /* if MDB_REPORT */
	} /* for */
}
