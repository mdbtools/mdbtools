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

#if SQL

typedef struct GMdbSQLWindow {
	GtkWidget *window;
	GtkWidget *textbox;
	GtkWidget *combo;
	GtkWidget *clist;
	GtkWidget *scroll;
	GtkWidget *ctree;
	GtkCTreeNode *current_node;
	GList *history;
} GMdbSQLWindow;

GList *sql_list;

extern GtkWidget *app;
extern MdbHandle *mdb;
extern MdbSQL *sql;

void gmdb_sql_tree_populate(MdbHandle *mdb, GladeXML *xml);

void
gmdb_sql_close_all()
{
	GladeXML *xml;
	GtkWidget *win;

	while (xml = g_list_nth_data(sql_list, 0)) {
		win = glade_xml_get_widget (xml, "sql_window");
		sql_list = g_list_remove(sql_list, xml);
		if (win) gtk_widget_destroy(win);
	}
}

/* callbacks */
void
gmdb_sql_close_cb(GtkWidget *w, GladeXML *xml)
{
	GtkWidget *win;
	sql_list = g_list_remove(sql_list, xml);
	win = glade_xml_get_widget (xml, "sql_window");
	if (win) gtk_widget_destroy(win);
}

void 
gmdb_sql_dnd_dataget_cb(
    GtkWidget *w, GdkDragContext *dc,
    GtkSelectionData *selection_data, guint info, guint t,
    GladeXML *xml)
{
gchar tablename[256];
//gchar *tablename = "Orders";
gchar *name;
GtkTreeSelection *select;
GtkTreeStore *store;
GtkTreeView *tree;
GtkTreeIter *iter, iter2;

	tree = (GtkTreeView *) glade_xml_get_widget(xml, "sql_treeview");
	select = gtk_tree_view_get_selection(GTK_TREE_VIEW (tree));
	store = (GtkTreeStore *) gtk_tree_view_get_model(tree);
	gtk_tree_selection_get_selected (select, NULL, &iter2);
	gtk_tree_model_get (GTK_TREE_MODEL(store), &iter2, 0, &name, -1);

	strcpy(tablename,name);
	g_free(name);
	printf("table %s\n",tablename);
	//strcpy(tablename, "Shippers");
	gtk_selection_data_set(
		selection_data,
		GDK_SELECTION_TYPE_STRING,
		8,  /* 8 bits per character. */
		tablename, strlen(tablename));
}
void gmdb_sql_dnd_datareceived_cb(
        GtkWidget *w,
        GdkDragContext *dc,
        gint x, gint y,
        GtkSelectionData *selection_data,
        guint info, guint t,
        GladeXML *xml)
{
gchar *buf, *lastbuf;
GtkTextIter iter, start, end;
GtkTextBuffer *txtbuffer;
int len;
GtkWidget *textview;

	textview = glade_xml_get_widget(xml, "sql_textview");
	buf = selection_data->data;
	txtbuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
	if (gtk_text_buffer_get_char_count(txtbuffer)==0) {
		gtk_text_buffer_get_iter_at_offset (txtbuffer, &iter, 0);
		gtk_text_buffer_insert(txtbuffer, &iter, "select * from ", 14);
	}
	gtk_widget_grab_focus(GTK_WIDGET(textview));
}

void
gmdb_sql_select_hist_cb(GtkList *list, GtkWidget *w, GMdbSQLWindow *sqlwin)
{
guint child_num;
gchar *buf;
GtkTextBuffer *txtbuffer;

	child_num = gtk_list_child_position(list, w);
	buf = (gchar *) gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(sqlwin->combo)->entry));
	txtbuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(sqlwin->textbox));
	gtk_text_buffer_set_text(txtbuffer, buf, strlen(buf));
}

void
gmdb_sql_execute_cb(GtkWidget *w, GladeXML *xml)
{
guint len;
gchar *buf;
gchar *bound_data[256];
int i;
MdbSQLColumn *sqlcol;
gchar *titles[256];
GtkTextBuffer *txtbuffer;
GtkTextIter start, end;
GtkWidget *textview, *combo, *treeview, *store;
GList *history;
GType *gtypes;
GtkTreeIter iter;
GtkTreeViewColumn *column;

	/* stuff this query on the history */
	textview = glade_xml_get_widget(xml, "sql_textview");
	combo = glade_xml_get_widget(xml, "sql_combo");
	txtbuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
        len = gtk_text_buffer_get_char_count(txtbuffer);
	gtk_text_buffer_get_iter_at_offset (txtbuffer, &start, 0);
	gtk_text_buffer_get_iter_at_offset (txtbuffer, &end, len);
	buf = gtk_text_buffer_get_text(txtbuffer, &start, &end, FALSE);

	/* add to the history */
	history = g_object_get_data(G_OBJECT(combo),"hist_list");
	history = g_list_prepend(history, g_strdup(buf));
	g_object_set_data(G_OBJECT(combo), "hist_list", history);
	gtk_combo_set_popdown_strings(GTK_COMBO(combo), history);

	/* ok now execute it */
	g_input_ptr = buf;
	/* begin unsafe */
	_mdb_sql(sql);
	if (yyparse()) {
		/* end unsafe */
		gmdb_info_msg("Couldn't parse SQL");
		mdb_sql_reset(sql);
		return;
	}

	treeview = glade_xml_get_widget(xml, "sql_results");

	gtypes = g_malloc(sizeof(GType) * sql->num_columns);
	for (i=0;i<sql->num_columns;i++) 
		gtypes[i]=G_TYPE_STRING;

	store = gtk_tree_view_get_model(treeview);
	if (store) {
		i=0;
		while (column = gtk_tree_view_get_column(GTK_TREE_VIEW(treeview), i)) {
			gtk_tree_view_remove_column(GTK_TREE_VIEW(treeview), column);
		}
		gtk_widget_destroy(store);
	}
	store = (GtkWidget *) gtk_list_store_newv(sql->num_columns, gtypes);
	g_free(gtypes);

	gtk_tree_view_set_model(GTK_TREE_VIEW(treeview), GTK_TREE_MODEL(store));
	
	GtkCellRenderer *renderer;
	renderer = gtk_cell_renderer_text_new(); 

	for (i=0;i<sql->num_columns;i++) { 
		bound_data[i] = (char *) malloc(MDB_BIND_SIZE); 
		bound_data[i][0] = '\0';
        	mdbsql_bind_column(sql, i+1, bound_data[i]);
		sqlcol = g_ptr_array_index(sql->columns,i);
		column = gtk_tree_view_column_new_with_attributes(sqlcol->name, renderer, "text", i, NULL); 
		gtk_tree_view_append_column(GTK_TREE_VIEW (treeview), column); 
	}

	while(mdb_fetch_row(sql->cur_table)) {
		gtk_list_store_append(GTK_LIST_STORE(store), &iter);
		for (i=0;i<sql->num_columns;i++) { 
			gtk_list_store_set(GTK_LIST_STORE(store), 
				&iter, i, (gchar *) bound_data[i], -1);
		}
	}


	/* free the memory used to bind */
	for (i=0;i<sql->num_columns;i++) {
		free(bound_data[i]);
	}

	mdb_sql_reset(sql);
	g_free(buf);
}

void
gmdb_sql_new_cb(GtkWidget *w, gpointer data)
{
GtkTargetEntry src;
GtkWidget *mi, *but;
GladeXML *sqlwin_xml;

	/* load the interface */
	sqlwin_xml = glade_xml_new("gladefiles/gmdb-sql.glade", NULL, NULL);
	/* connect the signals in the interface */
	glade_xml_signal_autoconnect(sqlwin_xml);

	sql_list = g_list_append(sql_list, sqlwin_xml);

	mi = glade_xml_get_widget (sqlwin_xml, "close_menu");
	g_signal_connect (G_OBJECT (mi), "activate",
		G_CALLBACK (gmdb_sql_close_cb), sqlwin_xml);

	but = glade_xml_get_widget (sqlwin_xml, "close_button");
	g_signal_connect (G_OBJECT (but), "clicked",
		G_CALLBACK (gmdb_sql_close_cb), sqlwin_xml);

	mi = glade_xml_get_widget (sqlwin_xml, "execute_menu");
	g_signal_connect (G_OBJECT (mi), "activate",
		G_CALLBACK (gmdb_sql_execute_cb), sqlwin_xml);

	but = glade_xml_get_widget (sqlwin_xml, "execute_button");
	g_signal_connect (G_OBJECT (but), "clicked",
		G_CALLBACK (gmdb_sql_execute_cb), sqlwin_xml);

	/* set up treeview, libglade only gives us the empty widget */
	GtkWidget *tree = glade_xml_get_widget(sqlwin_xml, "sql_treeview");
	GtkTreeStore *store = gtk_tree_store_new(1, G_TYPE_STRING);
	gtk_tree_view_set_model(GTK_TREE_VIEW(tree), GTK_TREE_MODEL(store));

	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("Name",
		renderer, "text", 0, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW (tree), column);

	GtkTreeSelection *select = 
		gtk_tree_view_get_selection (GTK_TREE_VIEW (tree));
	gtk_tree_selection_set_mode (select, GTK_SELECTION_SINGLE);
	//g_signal_connect (G_OBJECT (select), "changed",
		//G_CALLBACK (gmdb_sql_select_cb), sqlwin_xml);
	
	/* populate first level of tree */
	gmdb_sql_tree_populate(mdb, sqlwin_xml);

	GtkWidget *textview = glade_xml_get_widget(sqlwin_xml, "sql_textview");
	src.target = "table";
	src.flags = 0;
	src.info = 1;
	gtk_drag_source_set( tree, GDK_BUTTON1_MASK, &src, 1, GDK_ACTION_COPY);
	gtk_drag_dest_set( textview,
		//GTK_DEST_DEFAULT_MOTION | 
		GTK_DEST_DEFAULT_HIGHLIGHT ,
		// GTK_DEST_DEFAULT_DROP, 
		&src, 1, GDK_ACTION_COPY | GDK_ACTION_MOVE);
	gtk_signal_connect( GTK_OBJECT(tree), "drag_data_get",
		GTK_SIGNAL_FUNC(gmdb_sql_dnd_dataget_cb), sqlwin_xml);
	gtk_signal_connect( GTK_OBJECT(textview), "drag_data_received",
		GTK_SIGNAL_FUNC(gmdb_sql_dnd_datareceived_cb), sqlwin_xml);
	
	gtk_widget_grab_focus(GTK_WIDGET(textview));
}

/* functions */
void 
gmdb_sql_tree_populate(MdbHandle *mdb, GladeXML *xml)
{
int   i;
MdbCatalogEntry *entry;
GtkTreeIter *iter1, *iter2;

	GtkWidget *tree = glade_xml_get_widget(xml, "sql_treeview");
	GtkTreeStore *store = (GtkTreeStore *) gtk_tree_view_get_model(GTK_TREE_VIEW(tree));

	/* loop over each entry in the catalog */
	for (i=0; i < mdb->num_catalog; i++) {
		entry = g_ptr_array_index (mdb->catalog, i);

 		/* if it's a table */
		if (entry->object_type == MDB_TABLE) {
			/* skip the MSys tables */
			if (strncmp (entry->object_name, "MSys", 4)) {
				/* add table to tab */
				iter2 = g_malloc(sizeof(GtkTreeIter));
				gtk_tree_store_append(store, iter2, NULL);
				gtk_tree_store_set(store, iter2, 0, entry->object_name, -1);
	     		}
		} /* if MDB_TABLE */
	} /* for */
}
#else

void
gmdb_sql_new_cb(GtkWidget *w, gpointer data)
{
	gmdb_info_msg("SQL support was not built in.\nRun configure with the --enable-sql option.");
}

#endif
