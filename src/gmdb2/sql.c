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
#include "gmdb.h"

#if SQL

GList *sql_list;

extern MdbHandle *mdb;
extern MdbSQL *sql;

static void gmdb_sql_tree_populate (MdbHandle*, GladeXML*);
static void gmdb_sql_load_query (GladeXML*, gchar*);
static void gmdb_sql_save_query (GladeXML*, gchar*);
static void gmdb_sql_save_as_cb (GtkWidget*, GladeXML*);

void
gmdb_sql_close_all()
{
	GladeXML *xml;
	GtkWidget *win;

	while ((xml = g_list_nth_data(sql_list, 0))) {
		win = glade_xml_get_widget (xml, "sql_window");
		sql_list = g_list_remove(sql_list, xml);
		if (win) gtk_widget_destroy(win);
	}
}

gchar* gmdb_export_get_filepath (GladeXML*);	/* from table_export.c */

/* callbacks */
static void
gmdb_sql_write_rslt_cb(GtkWidget *w, GladeXML *xml)
{
	/* We need to re-run the whole query because some information is not stored
	 * in the TreeStore, such as column types.
	 */
	gchar *file_path;
	GladeXML *sql_xml;
	GtkWidget *filesel, *dlg;
	FILE *outfile;
	int i;
	int need_headers = 0;
	gchar delimiter[11];
	gchar quotechar[5];
	gchar escape_char[5];
	int bin_mode;
	gchar lineterm[5];

	guint len;
	gchar *buf;
	GtkTextIter start, end;
	GtkTextBuffer *txtbuffer;
	GtkWidget *textview;
	char **bound_values;
	int *bound_lens; 
	MdbSQLColumn *sqlcol;
	long row;
	char *value;
	size_t length;
	MdbTableDef *table;
	MdbColumn *col = NULL;

	gmdb_export_get_delimiter(xml, delimiter, sizeof(delimiter));
	gmdb_export_get_lineterm(xml, lineterm, sizeof(lineterm));
	gmdb_export_get_quotechar(xml, quotechar, sizeof(quotechar));
	gmdb_export_get_escapechar(xml, escape_char, sizeof(escape_char));
	bin_mode = gmdb_export_get_binmode(xml);
	need_headers = gmdb_export_get_headers(xml);
	file_path = gmdb_export_get_filepath(xml);

	if ((outfile=fopen(file_path, "w"))==NULL) {
		dlg = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (w)),
		    GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_WARNING, GTK_BUTTONS_CLOSE,
		    _("Unable to open file."));
		gtk_dialog_run (GTK_DIALOG (dlg));
		gtk_widget_destroy (dlg);
		return;
	}

	/* Get SQL */
	filesel = glade_xml_get_widget (xml, "export_dialog");
	sql_xml = g_object_get_data(G_OBJECT(filesel), "sql_xml");
	//printf("sql_xml %p\n",sql_xml);
	textview = glade_xml_get_widget(sql_xml, "sql_textview");
	txtbuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
	len = gtk_text_buffer_get_char_count(txtbuffer);
	gtk_text_buffer_get_iter_at_offset (txtbuffer, &start, 0);
	gtk_text_buffer_get_iter_at_offset (txtbuffer, &end, len);
	buf = gtk_text_buffer_get_text(txtbuffer, &start, &end, FALSE);


	/* ok now execute it */
	mdb_sql_run_query(sql, buf);
	if (mdb_sql_has_error(sql)) {
		GtkWidget* dlg = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (w)),
		    GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_WARNING, GTK_BUTTONS_CLOSE,
		    "%s", mdb_sql_last_error(sql));
		gtk_dialog_run (GTK_DIALOG (dlg));
		gtk_widget_destroy (dlg);
		mdb_sql_reset(sql);
		
		fclose(outfile);
		gtk_widget_destroy(filesel);
		return;
	}

	bound_values = (char **) g_malloc(sql->num_columns * sizeof(char *));
	bound_lens = (int *) g_malloc(sql->num_columns * sizeof(int));

	for (i=0; i<sql->num_columns; i++) {
		/* bind columns */
		bound_values[i] = (char *) g_malloc0(MDB_BIND_SIZE); 
		mdb_sql_bind_column(sql, i+1, bound_values[i], &bound_lens[i]);

		/* display column titles */
		if (need_headers) {
			if (i>0)
				fputs(delimiter, outfile);
			sqlcol = g_ptr_array_index(sql->columns,i);
			gmdb_print_col(outfile, sqlcol->name, quotechar[0]!='\0', MDB_TEXT, 0, quotechar, escape_char, bin_mode);
		}
	}

	row = 0;
	while (mdb_fetch_row(sql->cur_table)) {
		row++;
		for (i=0; i<sql->num_columns; i++) { 
			if (i>0)
				fputs(delimiter, outfile);

			sqlcol = g_ptr_array_index(sql->columns, i);

			/* Find col matching sqlcol */
			table = sql->cur_table;
			for (i=0; i<table->num_cols; i++) {
				col = g_ptr_array_index(table->columns, i);
				if (!strcasecmp(sqlcol->name, col->name))
					break;
			}
			/* assert(i!=table->num_cols). Can't happen, already checked. */

			/* Don't quote NULLs */
			if (bound_lens[i] && sqlcol->bind_type != MDB_OLE) {
				if (col->col_type == MDB_OLE) {
					value = mdb_ole_read_full(mdb, col, &length);
				} else {
					value = bound_values[i];
					length = bound_lens[i];
				}
				gmdb_print_col(outfile, value, quotechar[0]!='\0', col->col_type, length, quotechar, escape_char, bin_mode);
				if (col->col_type == MDB_OLE)
					free(value);
			}
		}
		fputs(lineterm, outfile);
	}

	/* free the memory used to bind */
	for (i=0; i<sql->num_columns; i++) {
		g_free(bound_values[i]);
	}

	mdb_sql_reset(sql);
	g_free(buf);

	fclose(outfile);

	dlg = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (w)),
	    GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_WARNING, GTK_BUTTONS_CLOSE,
	    _("%ld rows successfully exported."), row);
	gtk_dialog_run (GTK_DIALOG (dlg));
	gtk_widget_destroy (dlg);
	
	gtk_widget_destroy(filesel);
}


static void
gmdb_sql_results_cb(GtkWidget *w, GladeXML *xml)
{
	GladeXML *dialog_xml;
	GtkWidget *but;
	GtkWidget *filesel;

	/* load the interface */
	dialog_xml = glade_xml_new(GMDB_GLADEDIR "gmdb-export.glade", NULL, NULL);
	/* connect the signals in the interface */
	glade_xml_signal_autoconnect(dialog_xml);

	filesel = glade_xml_get_widget (dialog_xml, "export_dialog");
	gtk_window_set_title(GTK_WINDOW(filesel), "Save Results As");

	but = glade_xml_get_widget (dialog_xml, "export_button");
	gtk_widget_hide(but);

	but = glade_xml_get_widget (dialog_xml, "save_button");
	gtk_widget_show(but);

	gmdb_table_export_populate_dialog(dialog_xml);

	but = glade_xml_get_widget (dialog_xml, "save_button");
	g_signal_connect (G_OBJECT (but), "clicked",
		G_CALLBACK (gmdb_sql_write_rslt_cb), dialog_xml);

	g_object_set_data(G_OBJECT(filesel), "sql_xml", xml);
}
static void
gmdb_sql_save_cb(GtkWidget *w, GladeXML *xml)
{
	GtkWidget *textview;
	gchar *str;

	textview = glade_xml_get_widget (xml, "sql_textview");
	str = g_object_get_data(G_OBJECT(textview), "file_name");
	if (!str) {
		gmdb_sql_save_as_cb(w, xml);
		return;
	}
	gmdb_sql_save_query(xml, str);
}
static void
gmdb_sql_save_as_cb(GtkWidget *w, GladeXML *xml)
{
	GtkWindow *parent_window = (GtkWindow *) glade_xml_get_widget (xml, "gmdb");
	GtkWidget *dialog = gtk_file_chooser_dialog_new ("Save Query As",
					      parent_window,
					      GTK_FILE_CHOOSER_ACTION_SAVE,
					      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					      GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
					      NULL);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
	  {
	    char *filename;

	    filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
	    gmdb_sql_save_query(xml, filename);
	  }

	gtk_widget_destroy (dialog);
}
static void
gmdb_sql_open_cb(GtkWidget *w, GladeXML *xml)
{
	GtkWindow *parent_window = (GtkWindow *) glade_xml_get_widget (xml, "gmdb");
	GtkWidget *dialog = gtk_file_chooser_dialog_new ("Open SQL Query",
					      parent_window,
					      GTK_FILE_CHOOSER_ACTION_OPEN,
					      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					      GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
					      NULL);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
	  {
	    char *filename;

	    filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
	    gmdb_sql_load_query(xml, filename);
	  }

	gtk_widget_destroy (dialog);

}
static void
gmdb_sql_copy_cb(GtkWidget *w, GladeXML *xml)
{
	GtkTextBuffer *txtbuffer;
	GtkClipboard *clipboard;
	GtkWidget *textview;

	textview = glade_xml_get_widget(xml, "sql_textview");
	clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
	txtbuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
	gtk_text_buffer_copy_clipboard(txtbuffer, clipboard);
}
static void
gmdb_sql_cut_cb(GtkWidget *w, GladeXML *xml)
{
	GtkTextBuffer *txtbuffer;
	GtkClipboard *clipboard;
	GtkWidget *textview;

	textview = glade_xml_get_widget(xml, "sql_textview");
	clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
	txtbuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
	gtk_text_buffer_cut_clipboard(txtbuffer, clipboard, TRUE);
}
static void
gmdb_sql_paste_cb(GtkWidget *w, GladeXML *xml)
{
	GtkTextBuffer *txtbuffer;
	GtkClipboard *clipboard;
	GtkWidget *textview;

	textview = glade_xml_get_widget(xml, "sql_textview");
	clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
	txtbuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
	gtk_text_buffer_paste_clipboard(txtbuffer, clipboard, NULL, TRUE);
}
static void
gmdb_sql_close_cb(GtkWidget *w, GladeXML *xml)
{
	GtkWidget *win;
	sql_list = g_list_remove(sql_list, xml);
	win = glade_xml_get_widget (xml, "sql_window");
	if (win) gtk_widget_destroy(win);
}

static void 
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
GtkTreeIter iter2;

	tree = (GtkTreeView *) glade_xml_get_widget(xml, "sql_treeview");
	select = gtk_tree_view_get_selection(GTK_TREE_VIEW (tree));
	store = (GtkTreeStore *) gtk_tree_view_get_model(tree);
	gtk_tree_selection_get_selected (select, NULL, &iter2);
	gtk_tree_model_get (GTK_TREE_MODEL(store), &iter2, 0, &name, -1);

	strcpy(tablename,name);
	g_free(name);
	//strcpy(tablename, "Shippers");
	gtk_selection_data_set(
		selection_data,
		GDK_SELECTION_TYPE_STRING,
		8,  /* 8 bits per character. */
		(guchar*)tablename, strlen(tablename));
}
static void gmdb_sql_dnd_datareceived_cb(
        GtkWidget *w,
        GdkDragContext *dc,
        gint x, gint y,
        GtkSelectionData *selection_data,
        guint info, guint t,
        GladeXML *xml)
{
GtkTextIter iter;
GtkTextBuffer *txtbuffer;
GtkWidget *textview;

	textview = glade_xml_get_widget(xml, "sql_textview");
	txtbuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
	if (gtk_text_buffer_get_char_count(txtbuffer)==0) {
		gtk_text_buffer_get_iter_at_offset (txtbuffer, &iter, 0);
		gtk_text_buffer_insert(txtbuffer, &iter, "select * from ", 14);
	}
	gtk_widget_grab_focus(GTK_WIDGET(textview));
}

static void
gmdb_sql_select_hist_cb(GtkComboBox *combobox, GladeXML *xml)
{
	gchar *buf;
	GtkTextBuffer *txtbuffer;
	GtkWidget *textview;

	buf = gtk_combo_box_get_active_text(combobox);
	if (!buf) return;
	textview = glade_xml_get_widget(xml, "sql_textview");
	txtbuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
	gtk_text_buffer_set_text(txtbuffer, buf, strlen(buf));
}

static void
gmdb_sql_execute_cb(GtkWidget *w, GladeXML *xml)
{

	guint len;
	gchar *buf;
	gchar *bound_data[256];
	int i;
	MdbSQLColumn *sqlcol;
	GtkTextBuffer *txtbuffer;
	GtkTextIter start, end;
	GtkWidget *textview, *combobox, *treeview;
	GtkTreeModel *store;
	/*GtkWidget *window;*/
	GType *gtypes;
	GtkTreeIter iter;
	GtkTreeViewColumn *column;
	long row, maxrow;
	/* GdkCursor *watch, *pointer; */

	/*  need to figure out how to clock during the treeview recalc/redraw
	window = glade_xml_get_widget(xml, "sql_window");
	watch = gdk_cursor_new(GDK_WATCH);
	gdk_window_set_cursor(GTK_WIDGET(window)->window, watch);
	gdk_cursor_unref(watch);
	*/

	/* stuff this query on the history */
	textview = glade_xml_get_widget(xml, "sql_textview");
	combobox = glade_xml_get_widget(xml, "sql_combo");
	txtbuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
	len = gtk_text_buffer_get_char_count(txtbuffer);
	gtk_text_buffer_get_iter_at_offset (txtbuffer, &start, 0);
	gtk_text_buffer_get_iter_at_offset (txtbuffer, &end, len);
	buf = gtk_text_buffer_get_text(txtbuffer, &start, &end, FALSE);

	/* add to the history */
	gtk_combo_box_prepend_text(GTK_COMBO_BOX(combobox), buf);
	gtk_combo_box_set_active(GTK_COMBO_BOX(combobox), 0);

	/* ok now execute it */
	mdb_sql_run_query(sql, buf);
	if (mdb_sql_has_error(sql)) {
		GtkWidget* dlg = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (w)),
		    GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_WARNING, GTK_BUTTONS_CLOSE,
		    "%s", mdb_sql_last_error(sql));
		gtk_dialog_run (GTK_DIALOG (dlg));
		gtk_widget_destroy (dlg);
		mdb_sql_reset(sql);
		return;
	}

	treeview = glade_xml_get_widget(xml, "sql_results");

	gtypes = g_malloc(sizeof(GType) * sql->num_columns);
	for (i=0;i<sql->num_columns;i++) 
		gtypes[i]=G_TYPE_STRING;

	store = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview));
	if (store) {
		while ((column = gtk_tree_view_get_column(GTK_TREE_VIEW(treeview), 0))) {
			gtk_tree_view_remove_column(GTK_TREE_VIEW(treeview), column);
		}
		g_object_unref(store);
	}
	store = (GtkTreeModel*)gtk_list_store_newv(sql->num_columns, gtypes);
	g_free(gtypes);

	gtk_tree_view_set_model(GTK_TREE_VIEW(treeview), store);
	
	GtkCellRenderer *renderer;
	renderer = gtk_cell_renderer_text_new(); 

	for (i=0;i<sql->num_columns;i++) { 
		bound_data[i] = (char *) g_malloc0(MDB_BIND_SIZE); 
        	mdb_sql_bind_column(sql, i+1, bound_data[i], NULL);
		sqlcol = g_ptr_array_index(sql->columns,i);
		column = gtk_tree_view_column_new_with_attributes(sqlcol->name, renderer, "text", i, NULL); 
		gtk_tree_view_append_column(GTK_TREE_VIEW (treeview), column); 
	}

	maxrow = gmdb_prefs_get_maxrows();

	row = 0;
	while(mdb_fetch_row(sql->cur_table) &&
			(!maxrow || (row < maxrow))) {
		row++;
		gtk_list_store_append(GTK_LIST_STORE(store), &iter);
		for (i=0;i<sql->num_columns;i++) { 
			gtk_list_store_set(GTK_LIST_STORE(store), 
				&iter, i, (gchar *) bound_data[i], -1);
		}
	}


	/* free the memory used to bind */
	for (i=0;i<sql->num_columns;i++) {
		g_free(bound_data[i]);
	}

	mdb_sql_reset(sql);
	g_free(buf);

	/*
	pointer = gdk_cursor_new(GDK_LEFT_PTR);
	gdk_window_set_cursor(GTK_WIDGET(window)->window, pointer);
	gdk_cursor_unref(pointer);
	*/
	
}

void
gmdb_sql_new_cb (GtkWidget *w, gpointer data) {
	GtkTargetEntry src;
	GtkWidget *mi, *but, *combobox;
	GladeXML *sqlwin_xml;

	/* load the interface */
	sqlwin_xml = glade_xml_new(GMDB_GLADEDIR "gmdb-sql.glade", NULL, NULL);
	/* connect the signals in the interface */
	glade_xml_signal_autoconnect(sqlwin_xml);

	sql_list = g_list_append(sql_list, sqlwin_xml);

	mi = glade_xml_get_widget (sqlwin_xml, "save_menu");
	g_signal_connect (G_OBJECT (mi), "activate",
		G_CALLBACK (gmdb_sql_save_cb), sqlwin_xml);

	but = glade_xml_get_widget (sqlwin_xml, "save_button");
	g_signal_connect (G_OBJECT (but), "clicked",
		G_CALLBACK (gmdb_sql_save_cb), sqlwin_xml);

	mi = glade_xml_get_widget (sqlwin_xml, "save_as_menu");
	g_signal_connect (G_OBJECT (mi), "activate",
		G_CALLBACK (gmdb_sql_save_as_cb), sqlwin_xml);

	//but = glade_xml_get_widget (sqlwin_xml, "save_as_button");
	//g_signal_connect (G_OBJECT (but), "clicked",
	//	G_CALLBACK (gmdb_sql_save_as_cb), sqlwin_xml);

	mi = glade_xml_get_widget (sqlwin_xml, "results_menu");
	g_signal_connect (G_OBJECT (mi), "activate",
		G_CALLBACK (gmdb_sql_results_cb), sqlwin_xml);

	but = glade_xml_get_widget (sqlwin_xml, "results_button");
	g_signal_connect (G_OBJECT (but), "clicked",
		G_CALLBACK (gmdb_sql_results_cb), sqlwin_xml);

	mi = glade_xml_get_widget (sqlwin_xml, "open_menu");
	g_signal_connect (G_OBJECT (mi), "activate",
		G_CALLBACK (gmdb_sql_open_cb), sqlwin_xml);

	but = glade_xml_get_widget (sqlwin_xml, "open_button");
	g_signal_connect (G_OBJECT (but), "clicked",
		G_CALLBACK (gmdb_sql_open_cb), sqlwin_xml);

	mi = glade_xml_get_widget (sqlwin_xml, "paste_menu");
	g_signal_connect (G_OBJECT (mi), "activate",
		G_CALLBACK (gmdb_sql_paste_cb), sqlwin_xml);

	mi = glade_xml_get_widget (sqlwin_xml, "cut_menu");
	g_signal_connect (G_OBJECT (mi), "activate",
		G_CALLBACK (gmdb_sql_cut_cb), sqlwin_xml);

	mi = glade_xml_get_widget (sqlwin_xml, "copy_menu");
	g_signal_connect (G_OBJECT (mi), "activate",
		G_CALLBACK (gmdb_sql_copy_cb), sqlwin_xml);

	mi = glade_xml_get_widget (sqlwin_xml, "close_menu");
	g_signal_connect (G_OBJECT (mi), "activate",
		G_CALLBACK (gmdb_sql_close_cb), sqlwin_xml);

	but = glade_xml_get_widget (sqlwin_xml, "close_button");
	g_signal_connect (G_OBJECT (but), "clicked",
		G_CALLBACK (gmdb_sql_close_cb), sqlwin_xml);

	mi = glade_xml_get_widget (sqlwin_xml, "execute_menu");
	g_signal_connect (G_OBJECT (mi), "activate",
		G_CALLBACK (gmdb_sql_execute_cb), sqlwin_xml);

	combobox = glade_xml_get_widget(sqlwin_xml, "sql_combo");
	g_signal_connect (G_OBJECT(GTK_COMBO_BOX(combobox)), "changed",
		G_CALLBACK (gmdb_sql_select_hist_cb), sqlwin_xml);

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
	
	//GValue value = {0, };
	//but =  glade_xml_get_widget(sqlwin_xml, "results_button");
	//g_value_init(&value, G_TYPE_STRING);
	//g_value_set_static_string(&value, GMDB_ICONDIR "stock_export.png");
	//g_object_set_property(G_OBJECT (but), "file" , &value);
	//g_value_unset (&value);

	gtk_widget_grab_focus(GTK_WIDGET(textview));
}

/* functions */
static gchar *
gmdb_sql_get_basename(char *file_path)
{
	int i, len;
	gchar *basename;

	for (i=strlen(file_path);i>=0 && file_path[i]!='/';i--);
	len = strlen(file_path) - i + 2;
	basename = g_malloc(len);
	if (file_path[i]=='/') {
		strncpy(basename,&file_path[i+1],len);
	} else {
		strncpy(basename,file_path,len);
	}
        basename[len]='\0';

	return basename;
}

static void
gmdb_sql_set_file(GladeXML *xml, gchar *file_name)
{
	GtkWidget *window, *textview;
	gchar *title;
	gchar *basename;
	gchar *suffix = " - MDB Query Tool";

	basename = gmdb_sql_get_basename(file_name);
	title = g_malloc(strlen(basename) + strlen(suffix) + 1);
	sprintf(title,"%s%s", basename, suffix); 
	g_free(basename);
	window = glade_xml_get_widget(xml, "sql_window");
	gtk_window_set_title(GTK_WINDOW(window), title);
	g_free(title);
	textview = glade_xml_get_widget(xml, "sql_textview");
	g_object_set_data(G_OBJECT(textview), "file_name", file_name);
}

static void
gmdb_sql_save_query (GladeXML *xml, gchar *file_path) {
	FILE *out;
	GtkWidget *textview;
        GtkTextBuffer *txtbuffer;
	GtkTextIter start, end;
	gchar *buf;

	if (!(out=fopen(file_path, "w"))) {
		GtkWidget* dlg = gtk_message_dialog_new (NULL,
		    GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_WARNING, GTK_BUTTONS_CLOSE,
		    _("Unable to open file."));
		gtk_dialog_run (GTK_DIALOG (dlg));
		gtk_widget_destroy (dlg);
		return;
	}
	textview = glade_xml_get_widget(xml, "sql_textview");
	txtbuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
	gtk_text_buffer_get_start_iter(txtbuffer, &start);
	gtk_text_buffer_get_end_iter(txtbuffer, &end);
	buf = gtk_text_buffer_get_text(txtbuffer, &start, &end, FALSE);
	fprintf(out,"%s\n",buf);
	fclose(out);
	gmdb_sql_set_file(xml, file_path);
}
static void
gmdb_sql_load_query(GladeXML *xml, gchar *file_path)
{
	FILE *in;
	char buf[256];
	GtkWidget *textview;
        GtkTextBuffer *txtbuffer;
	GtkTextIter start, end;

	if (!(in=fopen(file_path, "r"))) {
		GtkWidget* dlg = gtk_message_dialog_new (NULL,
		    GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_WARNING, GTK_BUTTONS_CLOSE,
		    _("Unable to open file."));
		gtk_dialog_run (GTK_DIALOG (dlg));
		gtk_widget_destroy (dlg);
		return;
	}
	textview = glade_xml_get_widget(xml, "sql_textview");
	txtbuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
	gtk_text_buffer_get_start_iter(txtbuffer, &start);
	gtk_text_buffer_get_end_iter(txtbuffer, &end);
	gtk_text_buffer_delete(txtbuffer, &start, &end);
	while (fgets(buf, 255, in) && (*buf != '\0')) {
		gtk_text_buffer_get_end_iter(txtbuffer, &end);
		gtk_text_buffer_insert(txtbuffer, &end, buf, strlen(buf));
	}
	fclose(in);
	gmdb_sql_set_file(xml, file_path);
}
static void 
gmdb_sql_tree_populate(MdbHandle *mdb, GladeXML *xml)
{
int   i;
MdbCatalogEntry *entry;
GtkTreeIter *iter2;

	GtkWidget *tree = glade_xml_get_widget(xml, "sql_treeview");
	GtkTreeStore *store = (GtkTreeStore *) gtk_tree_view_get_model(GTK_TREE_VIEW(tree));

	/* add all user tables in catalog to tab */
	for (i=0; i < mdb->num_catalog; i++) {
		entry = g_ptr_array_index (mdb->catalog, i);
		if (mdb_is_user_table(entry)) {
			iter2 = g_malloc(sizeof(GtkTreeIter));
			gtk_tree_store_append(store, iter2, NULL);
			gtk_tree_store_set(store, iter2, 0, entry->object_name, -1);
		}
	} /* for */
}
#else

void
gmdb_sql_new_cb (GtkWidget *w, gpointer data) {
	GtkWidget* dlg = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (w)),
	    GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE,
	    _("SQL support was not built.\nMake sure flex and yacc are installed at build time."));
	gtk_dialog_run (GTK_DIALOG (dlg));
	gtk_widget_destroy (dlg);
}

#endif
