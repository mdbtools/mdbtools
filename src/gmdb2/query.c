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

extern GladeXML* mainwin_xml;
GtkWidget *query_list;
extern GtkWidget *app;

void
gmdb_query_add_icon(gchar *text)
{
GnomeIconList *gil;

	gil = glade_xml_get_widget (mainwin_xml, "query_iconlist");
	gnome_icon_list_append(gil, GMDB_ICONDIR "query_big.xpm", text);

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
