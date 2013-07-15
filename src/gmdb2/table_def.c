/* MDB Tools - A library for reading MS Access database file
 * Copyright (C) 2000-2012 Brian Bruns and others
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

#include "gmdb.h"

#define COL_PK 0
#define COL_NAME 1
#define COL_TYPE 2
#define COL_TYPETEXT 3
#define COL_DESCRIPTION 4
#define COL_LEN 5
#define COL_FORMAT 6
#define COL_DECIMALPLACES 7
#define COL_INPUTMASK 8
#define COL_CAPTION 9
#define COL_DEFAULTVALUE 10
#define COL_VALIDATIONRULE 11
#define COL_VALIDATIONTEXT 12
#define COL_REQUIRED 13
#define COL_ALLOWZEROLENGTH 14

static void update_bottom_properties(GtkTreeView *treeview, GladeXML *xml);

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

static gint
gmdb_table_def_cursorchanged(GtkTreeView *treeview, GladeXML *xml)
{
	update_bottom_properties(treeview, xml);
	return FALSE;
}

void
gmdb_table_def_new(MdbCatalogEntry *entry)
{
/* FIXME: many reference should be freed */
GladeXML *xml;
GtkWindow *win;
GtkTreeView *treeview;
GtkListStore *store;
GtkTreeModel *model;
GtkTreeIter iter;
GtkTreeSelection *selection;
int i, j;
GMdbDefWindow *defw;
MdbTableDef *table;
MdbColumn *col;
const char *propval;
MdbIndex *idx;
GdkPixbuf *pixbuf;

	/* do we have an active window for this object? if so raise it */
	for (i=0;i<g_list_length(window_list);i++) {
		defw = g_list_nth_data(window_list, i);
		if (!strcmp(defw->table_name, entry->object_name) && entry->object_type == MDB_TABLE) {
			gdk_window_raise (defw->window->window);
			return;
		}
	}

	/* load the interface */
	xml = glade_xml_new(GMDB_GLADEDIR "gmdb-tabledef.glade", NULL, NULL);
	glade_xml_signal_autoconnect(xml);
	win = GTK_WINDOW(glade_xml_get_widget(xml, "window1"));

	gtk_window_set_title(win, entry->object_name);
	gtk_window_set_default_size(win, 600, 400);
	
	/* icon to be used as primary key member indicator */
	pixbuf = gdk_pixbuf_new_from_file(GMDB_ICONDIR "pk.xpm", NULL);

	store = gtk_list_store_new(15,
		GDK_TYPE_PIXBUF, /* part of primary key */
		G_TYPE_STRING, /* column name */
		G_TYPE_INT, /* type */
		G_TYPE_STRING, /* type as text */
		G_TYPE_STRING, /* description */
		G_TYPE_INT, /* length */
		G_TYPE_STRING, /* format */
		G_TYPE_INT, /* decimal places */
		G_TYPE_STRING, /* inputmask */
		G_TYPE_STRING, /* caption */
		G_TYPE_STRING, /* default value */
		G_TYPE_STRING, /* validation rule */
		G_TYPE_STRING, /* validation text */
		G_TYPE_BOOLEAN, /* required */
		G_TYPE_BOOLEAN /* allow zero length */
		);

	/* read table */
	table = mdb_read_table(entry);
	mdb_read_columns(table);
	mdb_rewind_table(table);

	for (i=0;i<table->num_cols;i++) {
		int required = 0;
		gtk_list_store_append (store, &iter);
		col=g_ptr_array_index(table->columns,i);
		gtk_list_store_set (store, &iter,
			COL_NAME, col->name,
			COL_TYPE, col->col_type,
			COL_TYPETEXT, mdb_get_colbacktype_string(col),
			COL_LEN, col->col_size,
			COL_REQUIRED, 0,
			-1);

		propval = mdb_col_get_prop(col, "Description");
		if (propval)
			gtk_list_store_set (store, &iter,
				COL_DESCRIPTION, propval,
				-1);

		propval = mdb_col_get_prop(col, "Format");
		if (propval)
			gtk_list_store_set (store, &iter,
				COL_FORMAT, propval,
				-1);

		propval = mdb_col_get_prop(col, "DecimalPlaces");
		if (propval)
			gtk_list_store_set (store, &iter,
				COL_DECIMALPLACES, atoi(propval),
				-1);

		propval = mdb_col_get_prop(col, "InputMask");
		if (propval)
			gtk_list_store_set (store, &iter,
				COL_INPUTMASK, propval,
				-1);

		propval = mdb_col_get_prop(col, "Caption");
		if (propval)
			gtk_list_store_set (store, &iter,
				COL_CAPTION, propval,
				-1);

		propval = mdb_col_get_prop(col, "DefaultValue");
		if (propval)
			gtk_list_store_set (store, &iter,
				COL_DEFAULTVALUE, propval,
				-1);

		propval = mdb_col_get_prop(col, "ValidationRule");
		if (propval)
			gtk_list_store_set (store, &iter,
				COL_VALIDATIONRULE, propval,
				-1);

		propval = mdb_col_get_prop(col, "ValidationText");
		if (propval)
			gtk_list_store_set (store, &iter,
				COL_VALIDATIONTEXT, propval,
				-1);

		propval = mdb_col_get_prop(col, "AllowZeroLength");
		if (propval)
			gtk_list_store_set (store, &iter,
				COL_ALLOWZEROLENGTH, propval,
				-1);

		if (col->col_type == MDB_BOOL)
			required = 1;
		else {
			propval = mdb_col_get_prop(col, "Required");
			if (propval && propval[0]=='y')
				required = 1;
		}
		gtk_list_store_set (store, &iter,
			COL_REQUIRED, required,
			-1);
	}

	model = GTK_TREE_MODEL(store);
	mdb_read_indices(table);
	for (i=0;i<table->num_idxs;i++) {
		idx = g_ptr_array_index (table->indices, i);
		if (idx->index_type==1)
    		for (j=0;j<idx->num_keys;j++) {
				gtk_tree_model_iter_nth_child(model, &iter, NULL, idx->key_col_num[j]-1);
				gtk_list_store_set (store, &iter,
					COL_PK, pixbuf,
					-1);
			}
	}


	treeview = GTK_TREE_VIEW(glade_xml_get_widget(xml, "columns_treeview"));
	gtk_tree_view_insert_column_with_attributes (treeview,
		-1,      
		"PK", gtk_cell_renderer_pixbuf_new(),
		"pixbuf", COL_PK,
		NULL);
	gtk_tree_view_insert_column_with_attributes (treeview,
		-1,      
		"Name", gtk_cell_renderer_text_new(),
		"text", COL_NAME,
		NULL);
	gtk_tree_view_insert_column_with_attributes (treeview,
		-1,      
		"Type", gtk_cell_renderer_text_new(),
		"text", COL_TYPETEXT,
		NULL);
	gtk_tree_view_insert_column_with_attributes (treeview,
		-1,      
		"Description", gtk_cell_renderer_text_new(),
		"text", COL_DESCRIPTION,
		NULL);

	gtk_tree_view_set_model (GTK_TREE_VIEW (treeview), model);

	/* The tree view has acquired its own reference to the
	 *  model, so we can drop ours. That way the model will
	 *  be freed automatically when the tree view is destroyed */
	g_object_unref (model);

	/* select first item */
	gtk_tree_model_iter_nth_child(model, &iter, NULL, 0);
	selection = gtk_tree_view_get_selection(treeview);
	gtk_tree_selection_select_iter(selection, &iter);
	update_bottom_properties(treeview, xml);

	gtk_widget_show(GTK_WIDGET(win));
	
	defw = g_malloc(sizeof(GMdbDefWindow));
	strcpy(defw->table_name, entry->object_name);
	defw->window = GTK_WIDGET(win);
	window_list = g_list_append(window_list, defw);

	gtk_signal_connect (GTK_OBJECT (treeview), "cursor-changed",
		GTK_SIGNAL_FUNC (gmdb_table_def_cursorchanged), xml);
	gtk_signal_connect (GTK_OBJECT (defw->window), "delete_event",
		GTK_SIGNAL_FUNC (gmdb_table_def_close), defw);
}

/* That function is called when selection is changed
 * It updates the window bottom information
 */
static void
update_bottom_properties_selected(GtkTreeModel *model,
	GtkTreePath *path,
	GtkTreeIter *iter,
	GladeXML *xml)
{
char tmp[20];
gint col_type;
gint size;
char *format;
gint decimalplaces;
char *inputmask;
char *caption;
char *defaultvalue;
char *validationrule;
char *validationtext;
gboolean required;
gboolean  allowzerolength;
GtkLabel *label;
GtkEntry *entry;

	gtk_tree_model_get(model, iter,
		COL_TYPE, &col_type,
		COL_LEN, &size,
		COL_FORMAT, &format,
		COL_DECIMALPLACES, &decimalplaces,
		COL_INPUTMASK, &inputmask,
		COL_CAPTION, &caption,
		COL_DEFAULTVALUE, &defaultvalue,
		COL_VALIDATIONRULE, &validationrule,
		COL_VALIDATIONTEXT, &validationtext,
		COL_REQUIRED, &required,
		COL_ALLOWZEROLENGTH, &allowzerolength,
		-1);
	
	//fprintf(stderr, "type=%d\n", col_type);
	label = GTK_LABEL(glade_xml_get_widget(xml, "size_label"));
	entry = GTK_ENTRY(glade_xml_get_widget(xml, "size_entry"));
	switch (col_type) {
	//case MDB_BOOL:
	case MDB_BYTE:
	case MDB_INT:
	case MDB_LONGINT:
	case MDB_MONEY:
	case MDB_FLOAT:
	case MDB_DOUBLE:
	case MDB_DATETIME:
	//case MDB_BINARY:
	//case MDB_TEXT:
	//case MDB_OLE:
	//case MDB_MEMO:
	//case MDB_REPID:
	//case MDB_NUMERIC:
	//case MDB_COMPLEX:
		gtk_widget_show(GTK_WIDGET(label));
		gtk_widget_show(GTK_WIDGET(entry));
		sprintf(tmp, "%d", size);
		gtk_entry_set_text(entry, tmp);
		break;
	default:
		gtk_widget_hide(GTK_WIDGET(label));
		gtk_widget_hide(GTK_WIDGET(entry));
	}

	label = GTK_LABEL(glade_xml_get_widget(xml, "format_label"));
	entry = GTK_ENTRY(glade_xml_get_widget(xml, "format_entry"));
	switch (col_type) {
	case MDB_BOOL:
	case MDB_BYTE:
	case MDB_INT:
	case MDB_LONGINT:
	case MDB_MONEY:
	case MDB_FLOAT:
	case MDB_DOUBLE:
	case MDB_DATETIME:
	//case MDB_BINARY:
	case MDB_TEXT:
	//case MDB_OLE:
	case MDB_MEMO:
	//case MDB_REPID:
	//case MDB_NUMERIC:
	//case MDB_COMPLEX:
		gtk_widget_show(GTK_WIDGET(label));
		gtk_widget_show(GTK_WIDGET(entry));
		sprintf(tmp, "%d", size);
		gtk_entry_set_text(entry, format ? format : "");
		break;
	default:
		gtk_widget_hide(GTK_WIDGET(label));
		gtk_widget_hide(GTK_WIDGET(entry));
	}
	g_free(format);

	label = GTK_LABEL(glade_xml_get_widget(xml, "decimalplaces_label"));
	entry = GTK_ENTRY(glade_xml_get_widget(xml, "decimalplaces_entry"));
	switch (col_type) {
	case MDB_BYTE:
	case MDB_INT:
	case MDB_LONGINT:
	case MDB_FLOAT:
	case MDB_DOUBLE:
		gtk_widget_show(GTK_WIDGET(label));
		gtk_widget_show(GTK_WIDGET(entry));
		if (decimalplaces) {
			sprintf(tmp, "%d", decimalplaces);
			gtk_entry_set_text(entry, tmp);
		} else
			gtk_entry_set_text(entry, "");
		break;
	default:
		gtk_widget_hide(GTK_WIDGET(label));
		gtk_widget_hide(GTK_WIDGET(entry));
	}

	label = GTK_LABEL(glade_xml_get_widget(xml, "inputmask_label"));
	entry = GTK_ENTRY(glade_xml_get_widget(xml, "inputmask_entry"));
	switch (col_type) {
	//case MDB_BOOL:
	case MDB_BYTE:
	case MDB_INT:
	case MDB_LONGINT:
	case MDB_MONEY:
	case MDB_FLOAT:
	case MDB_DOUBLE:
	case MDB_DATETIME:
	//case MDB_BINARY:
	case MDB_TEXT:
	//case MDB_OLE:
	case MDB_MEMO:
	//case MDB_REPID:
	//case MDB_NUMERIC:
	//case MDB_COMPLEX:
		gtk_widget_show(GTK_WIDGET(label));
		gtk_widget_show(GTK_WIDGET(entry));
		gtk_entry_set_text(entry, inputmask ? inputmask : "");
		break;
	default:
		gtk_widget_hide(GTK_WIDGET(label));
		gtk_widget_hide(GTK_WIDGET(entry));
	}
	g_free(inputmask);

	entry = GTK_ENTRY(glade_xml_get_widget(xml, "caption_entry"));
	gtk_entry_set_text(entry, caption ? caption : "");
	g_free(caption);

	label = GTK_LABEL(glade_xml_get_widget(xml, "defaultvalue_label"));
	entry = GTK_ENTRY(glade_xml_get_widget(xml, "defaultvalue_entry"));
	switch (col_type) {
	case MDB_BOOL:
	case MDB_BYTE:
	case MDB_INT:
	case MDB_LONGINT:
	case MDB_MONEY:
	case MDB_FLOAT:
	case MDB_DOUBLE:
	case MDB_DATETIME:
	//case MDB_BINARY:
	case MDB_TEXT:
	//case MDB_OLE:
	//case MDB_MEMO:
	//case MDB_REPID:
	//case MDB_NUMERIC:
	//case MDB_COMPLEX:
		gtk_widget_show(GTK_WIDGET(label));
		gtk_widget_show(GTK_WIDGET(entry));
		gtk_entry_set_text(entry, defaultvalue ? defaultvalue : "");
	default:
		gtk_widget_hide(GTK_WIDGET(label));
		gtk_widget_hide(GTK_WIDGET(entry));
	}
	g_free(defaultvalue);

	label = GTK_LABEL(glade_xml_get_widget(xml, "validationrule_label"));
	entry = GTK_ENTRY(glade_xml_get_widget(xml, "validationrule_entry"));
	switch (col_type) {
	case MDB_BOOL:
	case MDB_BYTE:
	case MDB_INT:
	case MDB_LONGINT:
	case MDB_MONEY:
	case MDB_FLOAT:
	case MDB_DOUBLE:
	case MDB_DATETIME:
	//case MDB_BINARY:
	case MDB_TEXT:
	//case MDB_OLE:
	case MDB_MEMO:
	//case MDB_REPID:
	//case MDB_NUMERIC:
	//case MDB_COMPLEX:
		gtk_widget_show(GTK_WIDGET(label));
		gtk_widget_show(GTK_WIDGET(entry));
		gtk_entry_set_text(entry, validationrule ? validationrule : "");
		break;
	default:
		gtk_widget_hide(GTK_WIDGET(label));
		gtk_widget_hide(GTK_WIDGET(entry));
	}
	g_free(validationrule);

	label = GTK_LABEL(glade_xml_get_widget(xml, "validationtext_label"));
	entry = GTK_ENTRY(glade_xml_get_widget(xml, "validationtext_entry"));
	switch (col_type) {
	case MDB_BOOL:
	case MDB_BYTE:
	case MDB_INT:
	case MDB_LONGINT:
	case MDB_MONEY:
	case MDB_FLOAT:
	case MDB_DOUBLE:
	case MDB_DATETIME:
	//case MDB_BINARY:
	case MDB_TEXT:
	//case MDB_OLE:
	case MDB_MEMO:
	//case MDB_REPID:
	//case MDB_NUMERIC:
	//case MDB_COMPLEX:
		gtk_widget_show(GTK_WIDGET(label));
		gtk_widget_show(GTK_WIDGET(entry));
		gtk_entry_set_text(entry, validationtext ? validationtext : "");
		break;
	default:
		gtk_widget_hide(GTK_WIDGET(label));
		gtk_widget_hide(GTK_WIDGET(entry));
	}
	g_free(validationtext);

	entry = GTK_ENTRY(glade_xml_get_widget(xml, "required_entry"));
	gtk_entry_set_text(entry, required ? "Yes" : "No");

	label = GTK_LABEL(glade_xml_get_widget(xml, "allowzerolength_label"));
	entry = GTK_ENTRY(glade_xml_get_widget(xml, "allowzerolength_entry"));
	switch (col_type) {
	//case MDB_BOOL:
	//case MDB_BYTE:
	//case MDB_INT:
	//case MDB_LONGINT:
	//case MDB_MONEY:
	//case MDB_FLOAT:
	//case MDB_DOUBLE:
	//case MDB_DATETIME:
	//case MDB_BINARY:
	case MDB_TEXT:
	//case MDB_OLE:
	case MDB_MEMO:
	//case MDB_REPID:
	//case MDB_NUMERIC:
	//case MDB_COMPLEX:
		gtk_widget_show(GTK_WIDGET(label));
		gtk_widget_show(GTK_WIDGET(entry));
		gtk_entry_set_text(entry, allowzerolength ? "Yes" : "No");
		break;
	default:
		gtk_widget_hide(GTK_WIDGET(label));
		gtk_widget_hide(GTK_WIDGET(entry));
	}
}

static void
update_bottom_properties(GtkTreeView *treeview, GladeXML *xml) {
GtkTreeSelection *selection;
	
	selection = gtk_tree_view_get_selection(treeview);
	gtk_tree_selection_selected_foreach(selection,
		(GtkTreeSelectionForeachFunc)update_bottom_properties_selected,
		xml);
}
