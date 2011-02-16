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
#include "pixmaps/pk.xpm"

extern GtkWidget *app;
extern MdbHandle *mdb;

typedef struct GMdbDefWindow {
    gchar table_name[MDB_MAX_OBJ_NAME];
    GtkWidget *window;
} GMdbDefWindow;

static GList *window_list;

/* callbacks */
static gint
gmdb_table_def_close(GtkList *list, GtkWidget *w, GMdbDefWindow *defw)
{
	window_list = g_list_remove(window_list, defw);
	g_free(defw);
	return FALSE;
}

GtkWidget *
gmdb_table_def_new(MdbCatalogEntry *entry)
{
MdbTableDef *table;
MdbIndex *idx;
MdbColumn *col;
GtkWidget *clist;
GtkWidget *scroll;
GdkPixmap *pixmap;
GdkBitmap *mask;
int i,j;
gchar *titles[] = { "", "Column", "Name", "Type", "Size", "Allow Nulls" };
gchar *row[6];
GMdbDefWindow *defw;
GtkStyle *style;

	/* do we have an active window for this object? if so raise it */
	for (i=0;i<g_list_length(window_list);i++) {
		defw = g_list_nth_data(window_list, i);
		if (!strcmp(defw->table_name, entry->object_name) && entry->object_type == MDB_TABLE) {
			gdk_window_raise (defw->window->window);
			return defw->window;
		}
	}

	defw = g_malloc(sizeof(GMdbDefWindow));
	strcpy(defw->table_name, entry->object_name);

	defw->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);	
	gtk_window_set_title(GTK_WINDOW(defw->window), entry->object_name);
	gtk_widget_set_usize(defw->window, 300,200);
	gtk_widget_show(defw->window);

	gtk_signal_connect (GTK_OBJECT (defw->window), "delete_event",
		GTK_SIGNAL_FUNC (gmdb_table_def_close), defw);

	scroll = gtk_scrolled_window_new(NULL,NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_widget_show (scroll);
	gtk_container_add(GTK_CONTAINER(defw->window), scroll);

	/* read table */
	table = mdb_read_table(entry);
	mdb_read_columns(table);
	mdb_rewind_table(table);

	clist = gtk_clist_new_with_titles(5, titles);
	gtk_clist_set_column_width (GTK_CLIST(clist), 0, 20);
	gtk_clist_set_column_width (GTK_CLIST(clist), 1, 35);
	gtk_clist_set_column_width (GTK_CLIST(clist), 2, 80);
	gtk_clist_set_column_width (GTK_CLIST(clist), 3, 60);
	gtk_clist_set_column_width (GTK_CLIST(clist), 4, 30);
	gtk_clist_set_column_width (GTK_CLIST(clist), 5, 30);
	gtk_widget_show(clist);
	gtk_container_add(GTK_CONTAINER(scroll),clist);

	for (i=0;i<table->num_cols;i++) {
		/* display column titles */
		col=g_ptr_array_index(table->columns,i);
		row[0] = (char *) g_malloc0(MDB_BIND_SIZE);
		row[1] = (char *) g_malloc0(MDB_BIND_SIZE);
		row[2] = (char *) g_malloc0(MDB_BIND_SIZE);
		row[3] = (char *) g_malloc0(MDB_BIND_SIZE);
		row[4] = (char *) g_malloc0(MDB_BIND_SIZE);
		row[5] = (char *) g_malloc0(MDB_BIND_SIZE);
		strcpy(row[0],"");
		sprintf(row[1],"%d", col->col_num+1);
		strcpy(row[2],col->name);
		strcpy(row[3], mdb_get_colbacktype_string(col));
		sprintf(row[4],"%d",col->col_size);
		if (col->is_fixed) {
			strcpy(row[5],"No");
		} else {
			strcpy(row[5],"Yes");
		}
		gtk_clist_append(GTK_CLIST(clist), row);
	}
	style = gtk_widget_get_style(clist);
	pixmap = gdk_pixmap_create_from_xpm(clist->window,
		&mask,
		&style->bg[GTK_STATE_NORMAL], 
		GMDB_ICONDIR "pk.xpm");
	//pixmap = gdk_pixmap_colormap_create_from_xpm_d( NULL,
		//gtk_widget_get_colormap(app), &mask, NULL, pk_xpm);

	mdb_read_indices(table);
	for (i=0;i<table->num_idxs;i++) {
		idx = g_ptr_array_index (table->indices, i);
		if (idx->index_type==1) {
    		for (j=0;j<idx->num_keys;j++) {
			if (pixmap) gtk_clist_set_pixmap(GTK_CLIST(clist), idx->key_col_num[j]-1,0, pixmap, mask);
    		}
		// } else {
    		//for (j=0;j<idx->num_keys;j++) {
				//sprintf(tmpstr,"%s:%d",idx->name,j);
				//gtk_clist_set_text(GTK_CLIST(clist), idx->key_col_num[j]-1,0, tmpstr);
			//}
		} 
	} /* for */

	/* add this one to the window list */
	window_list = g_list_append(window_list, defw);

	return defw->window;
}
