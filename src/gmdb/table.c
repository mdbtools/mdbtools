#include "gmdb.h"
#include "gtkhlist.h"
#include "table.xpm"

GtkWidget *table_hlist;
GtkWidget *table_data_window;
GtkWidget *table_def_window;
extern GtkWidget *app;
extern MdbHandle *mdb;
int selected_child = -1;
int selected_table = -1;
extern char *mdb_access_types[];

/* callbacks */
void
gmdb_table_def_cb(GtkHList *hlist, GtkWidget *w, gpointer data)
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
gmdb_table_export_cb(GtkHList *hlist, GtkWidget *w, gpointer data)
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
gmdb_table_data_cb(GtkHList *hlist, GtkWidget *w, gpointer data)
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
gmdb_table_select_cb(GtkHList *hlist, GtkWidget *w, gpointer data)
{
int child_num;
int i,j=0;
MdbCatalogEntry *entry;

	child_num = gtk_hlist_child_position(hlist, w);
	selected_child = child_num;
	for (i=0;i<mdb->num_catalog;i++) {
		entry = g_ptr_array_index(mdb->catalog,i);
		if (mdb_is_user_table(entry)) {
			if (j==child_num) {
				selected_table = i;
			}
			j++;
		}
	}
}

/* functions */
void
gmdb_table_add_icon(gchar *text)
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
	pixmap = gdk_pixmap_colormap_create_from_xpm_d( NULL,  
		gtk_widget_get_colormap(app), &mask, NULL, table_xpm);

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
	gtk_hlist_append_items(GTK_HLIST(table_hlist), glist);
}

void
gmdb_table_add_tab(GtkWidget *notebook)
{
GtkWidget *tabbox;
GtkWidget *label;
GtkWidget *pixmapwid;
GtkWidget *frame;
GtkWidget *hbox;
GtkWidget *vbbox;
GtkWidget *button1;
GtkWidget *button2;
GtkWidget *button3;
GdkPixmap *pixmap;
GdkBitmap *mask;

        hbox = gtk_hbox_new (FALSE,5);
	gtk_widget_show (hbox);

	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
	gtk_container_set_border_width (GTK_CONTAINER (frame), 10);
	gtk_widget_set_usize (frame, 100, 75);
	gtk_widget_show (frame);
	gtk_box_pack_start (GTK_BOX (hbox), frame, TRUE, TRUE, 0);

	table_hlist = gtk_hlist_new ();
	gtk_widget_show (table_hlist);
	gtk_container_add(GTK_CONTAINER(frame), table_hlist);

	/* set selection callback for list */
        gtk_signal_connect (
                GTK_OBJECT (table_hlist),
                "select-child", GTK_SIGNAL_FUNC (gmdb_table_select_cb), NULL);

        vbbox = gtk_vbutton_box_new ();
	gtk_container_set_border_width (GTK_CONTAINER (vbbox), 10);
	gtk_vbutton_box_set_layout_default ( GTK_BUTTONBOX_START );
	gtk_button_box_set_child_size ( GTK_BUTTON_BOX( vbbox), 80, 20);
	gtk_box_pack_start (GTK_BOX (hbox), vbbox, FALSE, FALSE, 10);
	gtk_widget_show( vbbox );

	button1 = gtk_button_new_with_label("Definition");
	gtk_box_pack_start (GTK_BOX (vbbox), button1, FALSE, TRUE, 0);
	gtk_widget_show( button1 );

	gtk_signal_connect ( GTK_OBJECT (button1),
		"clicked", GTK_SIGNAL_FUNC (gmdb_table_def_cb), NULL);

	button2 = gtk_button_new_with_label("Data");
	gtk_box_pack_start (GTK_BOX (vbbox), button2, FALSE, FALSE, 0);
	gtk_widget_show( button2 );

	gtk_signal_connect ( GTK_OBJECT (button2),
		"clicked", GTK_SIGNAL_FUNC (gmdb_table_data_cb), NULL);

	button3 = gtk_button_new_with_label("Export");
	gtk_box_pack_start (GTK_BOX (vbbox), button3, FALSE, FALSE, 0);
	gtk_widget_show( button3 );

	gtk_signal_connect ( GTK_OBJECT (button3),
		"clicked", GTK_SIGNAL_FUNC (gmdb_table_export_cb), NULL);

	/* create a picture/label box and use for tab */
        tabbox = gtk_hbox_new (FALSE,5);
	pixmap = gdk_pixmap_colormap_create_from_xpm_d( NULL,  
		gtk_widget_get_colormap(app), &mask, NULL, table_xpm);

	/* a pixmap widget to contain the pixmap */
	pixmapwid = gtk_pixmap_new( pixmap, mask );
	gtk_widget_show( pixmapwid );
	gtk_box_pack_start (GTK_BOX (tabbox), pixmapwid, FALSE, TRUE, 0);

	label = gtk_label_new ("Tables");
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (tabbox), label, FALSE, TRUE, 0);

	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), hbox, tabbox);
}

void gmdb_table_populate(MdbHandle *mdb)
{
int   i;
MdbCatalogEntry *entry;

	/* add all user tables in catalog to tab */
	for (i=0; i < mdb->num_catalog; i++) {
		entry = g_ptr_array_index (mdb->catalog, i);
		if (mdb_is_user_table(entry)) {
			gmdb_table_add_icon(entry->object_name);
		}
	} /* for */
}
