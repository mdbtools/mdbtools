#include "gmdb.h"
#include "gtkhlist.h"

GtkWidget *query_hlist;
extern GtkWidget *app;

void
gmdb_query_add_icon(gchar *text)
{
GList *glist = NULL;
GtkWidget *li;
GtkWidget *label;
GtkWidget *box;
GtkWidget *pixmapwid;
GdkPixmap *pixmap;
GdkBitmap *mask;

	li = gtk_list_item_new ();
        box = gtk_hbox_new (FALSE,5);
	pixmap = gdk_pixmap_colormap_create_from_xpm( NULL,  
		gtk_widget_get_colormap(app), &mask, NULL, "query.xpm");

	/* a pixmap widget to contain the pixmap */
	pixmapwid = gtk_pixmap_new( pixmap, mask );
	gtk_widget_show( pixmapwid );
	gtk_box_pack_start (GTK_BOX (box), pixmapwid, FALSE, TRUE, 0);

	label = gtk_label_new (text);
	gtk_box_pack_start (GTK_BOX (box), label, FALSE, TRUE, 0);

	gtk_container_add(GTK_CONTAINER(li), box);
	glist = g_list_append (glist, li);
	gtk_widget_show (label);
	gtk_widget_show_all (li);
	gtk_hlist_append_items(GTK_HLIST(query_hlist), glist);
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
