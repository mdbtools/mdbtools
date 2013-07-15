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

/* Objects attached to debug_window:
 *   GList *nav_list: list of pages viewed, entries are guint32 *'s
 *   guint *nav_elem: index to current page in nav_list
 *   GladeXML *debugwin_xml: the associated glade hierarchy
 *   gboolean *dissect: whether dissecting is turned on
 */ 

#include <glade/glade.h>
#include <gtk/gtkmessagedialog.h>
#include <libgnome/gnome-i18n.h>
#include "gmdb.h"

extern MdbHandle *mdb;

GList *debug_list = NULL;

#define LINESZ 77

/* prototypes */
static void gmdb_debug_init(MdbHandle *mdb, GladeXML *xml);
static void gmdb_debug_text_on(GtkWidget *textbox, int start_byte, int end_byte);
static void gmdb_debug_text_off(GtkWidget *textbox);
static GtkTreeIter *gmdb_debug_add_item(GtkTreeStore *store, GtkTreeIter *iter, gchar *text, int start, int len);
static void gmdb_debug_clear(GladeXML *xml);
static void gmdb_debug_dissect(GtkTreeStore *store, char *fbuf, int offset, int len);
static guint16 get_uint16(void *c);
static guint32 get_uint32(void *c);
static long gmdb_get_max_page(MdbHandle *mdb);
static void gmdb_debug_display(GladeXML *xml, guint32 page);
static void gmdb_debug_jump(GladeXML *xml, int msb);

/* value to string stuff */
typedef struct GMdbValStr {
	gint value;
	gchar *string;
} GMdbValStr;

GMdbValStr table_types[] = {
	{ 0x4e, "User" },
	{ 0x53, "System" },
	{ 0, NULL }
};
GMdbValStr column_types[] = {
	{ 0x01, "boolean" },
	{ 0x02, "byte" },
	{ 0x03, "int" },
	{ 0x04, "longint" },
	{ 0x05, "money" },
	{ 0x06, "float" },
	{ 0x07, "double" },
	{ 0x08, "short datetime" },
	{ 0x09, "binary" },
	{ 0x0a, "text" },
	{ 0x0b, "OLE" },
	{ 0x0c, "memo/hyperlink" },
	{ 0x0d, "Unknown" },
	{ 0x0f, "GUID" },
	{ 0, NULL }
};
GMdbValStr object_types[] = {
	{ 0x00, "Database Definition" },
	{ 0x01, "Data" },
	{ 0x02, "Table Definition" },
	{ 0x03, "Index" },
	{ 0x04, "Leaf Index" },
	{ 0, NULL }
};

/* callbacks */
static void
gmdb_debug_select_cb(GtkTreeSelection *select, GladeXML *xml)
{
	int start_row, end_row;
	int start_col, end_col;
	GtkTreeIter iter;
	GtkTreeModel *model;
	gint32 start, end, len;
	GtkWidget *textview;
	gchar *fieldname;

	fprintf(stderr, "select_cb fired\n");

	textview = glade_xml_get_widget (xml, "debug_textview");
	gmdb_debug_text_off(textview);

	if (!select)
		return;
	if (!gtk_tree_selection_get_selected (select, &model, &iter))
		return;

	gtk_tree_model_get (model, &iter, 0, &fieldname, 
		1, &start, 
		2, &len, -1);
	g_free (fieldname);
	if ((start == -1) || (len < 1))
		return;

	end = start + len - 1;
	start_row = LINESZ * (start / 16);
	end_row = LINESZ * (end / 16);
	start_col = 8 + (start % 16) * 3;
	end_col = 8 + (end % 16) * 3;

	if (start_row == end_row) {
		gmdb_debug_text_on(textview, 
			start_row + start_col,
			start_row + end_col + 2);
		gmdb_debug_text_on(textview, 
			start_row + 59 + (start % 16),
			start_row + 59 + (end % 16) + 1);
	} else {
		int i;
		gmdb_debug_text_on(textview, 
			start_row + start_col,
			/* 55 = 8 (addr) + 15 (bytes) * 3 (%02x " ") + 2 (last byte) */
			start_row + 55); 
		gmdb_debug_text_on(textview, 
			start_row + 59 + (start % 16),
			start_row + 75); 
		for (i=start_row + LINESZ; i < end_row; i+=LINESZ) {
			gmdb_debug_text_on(textview, i + 8, i + 55);
			gmdb_debug_text_on(textview, i + 59, i + 75);
		}
		gmdb_debug_text_on(textview, 
			end_row + 8,
			end_row + end_col + 2); 
		gmdb_debug_text_on(textview,
			end_row + 59,
			end_row + 59 + (end % 16) + 1); 
	}
}
void
gmdb_debug_forward_cb(GtkWidget *w, gpointer data)
{
	guint *nav_elem;
	guint32 *page;
	GladeXML *xml;
	GList *nav_list;
	guint num_items;

	nav_list = g_object_get_data(G_OBJECT(w),"nav_list");
	nav_elem = g_object_get_data(G_OBJECT(w),"nav_elem");
	num_items = g_list_length(nav_list);
	if (!nav_elem || *nav_elem == num_items)
		return;
	(*nav_elem)++;
	g_object_set_data(G_OBJECT(w), "nav_elem", nav_elem);
	page = g_list_nth_data(nav_list,(*nav_elem) - 1);

	xml = g_object_get_data(G_OBJECT(w), "debugwin_xml");
	gmdb_debug_display(xml, *page);
}
void
gmdb_debug_back_cb(GtkWidget *w, gpointer data)
{
	guint *nav_elem;
	guint32 *page;
	GladeXML *xml;
	GList *nav_list;

	nav_list = g_object_get_data(G_OBJECT(w),"nav_list");
	nav_elem = g_object_get_data(G_OBJECT(w),"nav_elem");
	if (!nav_elem || *nav_elem==1)
		return; /* at top of list already */
	(*nav_elem)--;
	g_object_set_data(G_OBJECT(w), "nav_elem", nav_elem);
	page = g_list_nth_data(nav_list,(*nav_elem) - 1);
	
	xml = g_object_get_data(G_OBJECT(w), "debugwin_xml");
	gmdb_debug_display(xml, *page);
}
static void
gmdb_nav_add_page(GtkWidget *win, guint32 page_num)
{
	GList *nav_list = NULL;
	GList *link = NULL;
	guint *nav_elem;
	guint num_items;
	int i;

	nav_list = g_object_get_data(G_OBJECT(win),"nav_list");
	nav_elem = g_object_get_data(G_OBJECT(win),"nav_elem");
	if (!nav_elem) {
		nav_elem = g_malloc0(sizeof(guint));
	}
	
	/*
	 * If we are positioned in the middle of the list and jumping from here
	 * clear the end of the list first.
	 */
	num_items = g_list_length(nav_list);
	if (num_items > *nav_elem) {
		for (i=num_items - 1; i >= *nav_elem; i--) {
			link = g_list_nth(nav_list,i);
			nav_list = g_list_remove_link(nav_list, link);
			g_free(link->data);
			g_list_free_1(link);
		}
	}

	nav_list = g_list_append(nav_list, g_memdup(&page_num, 4));
	*nav_elem = g_list_length(nav_list);

	g_object_set_data(G_OBJECT(win), "nav_list", nav_list);
	g_object_set_data(G_OBJECT(win), "nav_elem", nav_elem);
}

void
gmdb_debug_jump_cb(GtkWidget *w, gpointer data)
{
	gmdb_debug_jump(g_object_get_data(G_OBJECT(w), "debugwin_xml"), 0);
}
void
gmdb_debug_jump_msb_cb(GtkWidget *w, gpointer data)
{
	gmdb_debug_jump(g_object_get_data(G_OBJECT(w), "debugwin_xml"), 1);
}
static void
gmdb_debug_jump(GladeXML *xml, int msb)
{
	GtkTextView *textview;
	GtkTextBuffer *txtbuffer;
	GtkTextIter start, end;
	GtkWidget *entry;
	gchar *text;
	gchar page[12];
	gchar digits[4][3];
	gchar *hex_digit;
	int i, num_digits = 0;

	textview = (GtkTextView *) glade_xml_get_widget (xml, "debug_textview");
	txtbuffer = gtk_text_view_get_buffer(textview);
	if (!gtk_text_buffer_get_selection_bounds(txtbuffer, &start, &end)) {
		/* FIX ME -- replace with text in status bar */
		fprintf(stderr, "Nothing selected\n");
		return;
	}
	text = g_strdup(gtk_text_buffer_get_text(txtbuffer, &start, &end, FALSE));
	//fprintf(stderr, "selected text = %s\n",text);
	hex_digit = strtok(text, " ");
	strcpy(page, "0x");
	do {
		if (strlen(hex_digit)>2) {
			fprintf(stderr, "Not a hex value\n");
			return;
		}
		strcpy(digits[num_digits++],hex_digit);
	} while (num_digits < 4 && (hex_digit = strtok(NULL," ")));
	if (msb) {
		for (i=0;i<num_digits;i++) {
			strcat(page, digits[i]);
		}
	} else {
		for (i=num_digits-1;i>=0;i--) {
			strcat(page, digits[i]);
		}
	}
	g_free(text);
	//fprintf(stderr, "going to page %s\n",page);
	entry = glade_xml_get_widget (xml, "debug_entry");
	gtk_entry_set_text(GTK_ENTRY(entry),page);
	gmdb_debug_display_cb(entry, NULL);
}

/*
 * w: pointer to GtkEntry 'debug_entry' 
 */
void
gmdb_debug_display_cb(GtkWidget *w, gpointer data)
{
	int page;
	GtkWidget *win;
	gchar *s;
	GladeXML *xml;

	fprintf(stderr, "display fired\n");

	if (!mdb) return;

	win = gtk_widget_get_toplevel(w);
	xml = g_object_get_data(G_OBJECT(win), "debugwin_xml");
	
	s = g_strdup(gtk_entry_get_text(GTK_ENTRY(w)));
	if (!strncmp(s,"0x",2)) {
		page = strtol(s+2, NULL, 16);
	} else {
		page = strtol(s, NULL, 10);
	}
	g_free(s);

	if (page>gmdb_get_max_page(mdb) || page<0) {
		GtkWidget* dlg = gtk_message_dialog_new (GTK_WINDOW(win),
		    GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_WARNING,
		    GTK_BUTTONS_CLOSE,
		    _("Page entered is outside valid page range."));
		gtk_dialog_run (GTK_DIALOG (dlg));
		gtk_widget_destroy (dlg);
	}

	/* add to the navigation list */
	gmdb_nav_add_page(win, page);
	/* gmdb_debug_display handles the mechanics of getting the page up */
	gmdb_debug_display(xml, page);
}
static void
gmdb_debug_display(GladeXML *xml, guint32 page)
{
	char *fbuf;
	char *tbuf;
	int i, j;
	char line[80];
	char field[10];
	GtkTextBuffer *buffer;
	GtkTextIter iter;
	GtkTextView *textview;
	GtkWidget *entry, *window;
	char *pagestr;
	gboolean *dissect;
	GtkWidget *tree;
	GtkTreeView *store;

	textview = (GtkTextView *) glade_xml_get_widget (xml, "debug_textview");
	gmdb_debug_clear(xml);

	pagestr = g_strdup_printf("%lu", (unsigned long)page);
	entry = glade_xml_get_widget (xml, "debug_entry");
	gtk_entry_set_text(GTK_ENTRY(entry),pagestr);
	g_free(pagestr);

	mdb_read_pg(mdb, page);
	fbuf = g_malloc(mdb->fmt->pg_size);
	memcpy(fbuf, mdb->pg_buf, mdb->fmt->pg_size);

	tbuf = g_malloc0( (mdb->fmt->pg_size / 16) * sizeof(line));
	i = 0;
	while (i<mdb->fmt->pg_size) {
		sprintf(line,"%06x  ", i);

		for(j=0; j<16; j++) {
			sprintf(field, "%02x ", ((unsigned char*)fbuf)[i+j]);
			strcat(line,field);
		}

		strcat(line, "  |");

		for(j=0; j<16; j++) {
			sprintf(field, "%c", (isprint(fbuf[i+j])) ? fbuf[i+j] : '.');
			strcat(line,field);
		}
		strcat(line, "|\n");
		i += 16;
		strcat(tbuf, line);
	}
	buffer = gtk_text_view_get_buffer(textview);
	gtk_text_buffer_get_iter_at_offset (buffer, &iter, 0);
	gtk_text_buffer_insert(buffer,&iter,tbuf,strlen(tbuf));
	
	tree = glade_xml_get_widget(xml, "debug_treeview");
	store = (GtkTreeView *) gtk_tree_view_get_model(GTK_TREE_VIEW(tree));

	window = glade_xml_get_widget(xml, "debug_window");
	dissect = g_object_get_data(G_OBJECT(window),"dissect");
	if (!dissect || *dissect)
		gmdb_debug_dissect(GTK_TREE_STORE(store), fbuf, 0, mdb->fmt->pg_size);

	g_free(fbuf);
	g_free(tbuf);
}
/* functions */
static long
gmdb_get_max_page(MdbHandle *mdb)
{
struct stat st;

	assert( fstat(mdb->f->fd, &st)!=-1 );
	return (st.st_size/mdb->fmt->pg_size)-1;
}
static gchar *
gmdb_val_to_str(GMdbValStr *valstr, gint val)
{
gchar *strptr;
int i = 0;

	do {
		strptr = valstr[i].string;
		if (val == valstr[i].value) {
			return strptr;
		}
		i++;	
	} while (strptr);
	return "unknown";
}
static guint16 
get_uint16(void *c)
{
	guint16 i;
	memcpy(&i, c, 2);
	return GINT16_FROM_LE(i);
}
static guint32 
get_uint32(void *c)
{
	guint32 l;
	memcpy(&l, c, 4);
	return GINT32_FROM_LE(l);
}
static void 
gmdb_debug_dissect_column(GtkTreeStore *store, GtkTreeIter *parent, char *fbuf, int offset)
{
	gchar *str;
	unsigned char *foff = (unsigned char*)fbuf + offset;

	str = g_strdup_printf("Column Type: 0x%02x (%s)", foff[0],
		gmdb_val_to_str(column_types, foff[0]));
	gmdb_debug_add_item(store, parent, str, offset, 1);
	str = g_strdup_printf("Column #: %d", get_uint16(foff+1));
	gmdb_debug_add_item(store, parent, str, offset+1, 2);
	str = g_strdup_printf("VarCol Offset: %d", get_uint16(foff+3));
	gmdb_debug_add_item(store, parent, str, offset+3, 2);
	str = g_strdup_printf("Unknown");
	gmdb_debug_add_item(store, parent, str, offset+5, 4);
	str = g_strdup_printf("Unknown");
	gmdb_debug_add_item(store, parent, str, offset+9, 4);
	str = g_strdup_printf("Variable Column: %s", 
		(foff[13] & 0x01) ? "No" : "Yes");
	gmdb_debug_add_item(store, parent, str, offset+13, 1);
	str = g_strdup_printf("Fixed Col Offset: %d", get_uint16(foff+14));
	gmdb_debug_add_item(store, parent, str, offset+14, 2);
	str = g_strdup_printf("Column Length: %d", get_uint16(foff+16));
	gmdb_debug_add_item(store, parent, str, offset+16, 2);
}
static void 
gmdb_debug_dissect_index1(GtkTreeStore *store, GtkTreeIter *parent, char *fbuf, int offset)
{
	gchar *str;

	gmdb_debug_add_item(store, parent, g_strdup("Unknown"), offset, 4);
	str = g_strdup_printf("Rows in Index: %lu",
		(unsigned long)get_uint32(fbuf+offset+4)); 
	gmdb_debug_add_item(store, parent, str, offset+4, 4);
}
static void 
gmdb_debug_dissect_index2(GtkTreeStore *store, GtkTreeIter *parent, char *fbuf, int offset)
{
	gchar *str;
	unsigned char flags;

	str = g_strdup("Column mask");
	gmdb_debug_add_item(store, parent, str, offset, 30);
	str = g_strdup("Unknown");
	gmdb_debug_add_item(store, parent, str, offset+30, 4);
	str = g_strdup("Root index page");
	gmdb_debug_add_item(store, parent, str, offset+34, 4);

	flags = fbuf[offset+38];
	str = g_strconcat("Index Flags:",
		(flags & MDB_IDX_UNIQUE) ? " Unique" : "",
		(flags & MDB_IDX_IGNORENULLS) ? " IgnoreNulls" : "",
		(flags & MDB_IDX_REQUIRED) ? " Required" : "",
		NULL);
	if (strcmp(str, "Index Flags:") == 0) {
		gchar *str2 = g_strconcat(str, " None", NULL);
		g_free(str);
		str = str2;
	}
	gmdb_debug_add_item(store, parent, str, offset+38, 1);
}
static void 
gmdb_debug_add_page_ptr(GtkTreeStore *store, GtkTreeIter *parent, char *fbuf, const char *label, int offset)
{
	gchar *str;
	GtkTreeIter *node;
	guint32 pg_row = get_uint32(fbuf+offset);

	str = g_strdup(label);
	node = gmdb_debug_add_item(store, parent, str, offset, 4);

	str = g_strdup_printf("Row Number: %u", pg_row & 0xff);
	gmdb_debug_add_item(store, node, str, offset, 1);
	str = g_strdup_printf("Page Number: %u", pg_row >> 8);
	gmdb_debug_add_item(store, node, str, offset+1, 3);
}
static void 
gmdb_debug_dissect_row(GtkTreeStore *store, GtkTreeIter *parent, char *fbuf, int offset, int len)
{
	gchar *str;
	int bitmask_sz;
	int num_cols, var_cols, var_cols_loc, fixed_end, eod_ptr;
	int i;

	num_cols = fbuf[offset];
	str = g_strdup_printf("Num columns: %u", num_cols);
	gmdb_debug_add_item(store, parent, str, offset, 1);
	bitmask_sz = (num_cols+7) / 8;
	var_cols_loc = offset + len - bitmask_sz - 1;
	var_cols = fbuf[var_cols_loc];
	fixed_end = offset + fbuf[var_cols_loc - 1] - 1; /* work even if 0 b/c of EOD */
	str = g_strdup("Fixed columns");
	gmdb_debug_add_item(store, parent, str, offset + 1, fixed_end - offset);
	for (i=0;i<var_cols;i++) {
	}
	eod_ptr = var_cols_loc - var_cols - 1;
	for (i=0;i<var_cols;i++) {
		int loc1 = fbuf[var_cols_loc - i - 1],
		    loc2 = fbuf[var_cols_loc - i - 2];
		str = g_strdup_printf("Var col %d", var_cols-i);
		gmdb_debug_add_item(store, parent, str,
			offset + loc1, loc2 - loc1);
	}
	str = g_strdup_printf("End of data (EOD): 0x%02x (%u)",
		((unsigned char*)fbuf)[eod_ptr], ((unsigned char*)fbuf)[eod_ptr]);
	gmdb_debug_add_item(store, parent, str, eod_ptr, 1);
	for (i=0;i<var_cols;i++) {
		str = g_strdup_printf("Var col %d offset: 0x%02x (%u)", var_cols-i,
			((unsigned char*)fbuf)[eod_ptr+i+1], ((unsigned char*)fbuf)[eod_ptr+i+1]);
		gmdb_debug_add_item(store, parent, str, eod_ptr + i + 1, 1);
	}
	str = g_strdup_printf("Num var cols: %u", var_cols);
	gmdb_debug_add_item(store, parent, str, var_cols_loc, 1);
	str = g_strdup("Null mask");
	gmdb_debug_add_item(store, parent, str, var_cols_loc + 1,
		offset + len - (var_cols_loc + 1));
}
static void 
gmdb_debug_dissect_index_pg(GtkTreeStore *store, char *fbuf, int offset, int len)
{
	gchar *str;
	guint32 tdef;

	str = g_strdup_printf("Page free space: %u", get_uint16(fbuf+offset+2));
	gmdb_debug_add_item(store, NULL, str, offset+2, 2);

	tdef = get_uint32(fbuf+offset+4);
	str = g_strdup_printf("Parents TDEF page: 0x%06x (%lu)", tdef,
		(unsigned long)tdef);
	gmdb_debug_add_item(store, NULL, str, offset+4, 4);
}

static void 
gmdb_debug_dissect_leaf_pg(GtkTreeStore *store, char *fbuf, int offset, int len)
{
	gchar *str;
	guint32 temp;

	temp = get_uint32(fbuf+offset+4);
	str = g_strdup_printf("Parent's TDEF page: 0x%06x (%lu)", temp,
		(unsigned long)temp);
	gmdb_debug_add_item(store, NULL, str, offset+4, 4);

	temp = get_uint32(fbuf+offset+8);
	str = g_strdup_printf("Previous leaf page: 0x%06x (%lu)", temp,
		(unsigned long)temp);
	gmdb_debug_add_item(store, NULL, str, offset+8, 4);

	temp = get_uint32(fbuf+offset+12);
	str = g_strdup_printf("Next leaf page: 0x%06x (%lu)", temp,
		(unsigned long)temp);
	gmdb_debug_add_item(store, NULL, str, offset+12, 4);
}
static void 
gmdb_debug_dissect_data_pg4(GtkTreeStore *store, char *fbuf, int offset, int len)
{
	gchar *str;
	int num_rows, i, row_start, row_len;
	guint32 tdef;
	//GtkTreeIter *container;

	str = g_strdup_printf("Page free space: %u", get_uint16(fbuf+offset+2));
	gmdb_debug_add_item(store, NULL, str, offset+2, 2);

	tdef = get_uint32(fbuf+offset+4);
	str = g_strdup_printf("Parent's TDEF page: 0x%06x (%lu)", tdef,
		(unsigned long)tdef);
	gmdb_debug_add_item(store, NULL, str, offset+4, 4);

	num_rows = get_uint16(fbuf+offset+12);
	str = g_strdup_printf("Num rows: %u", num_rows);
	gmdb_debug_add_item(store, NULL, str, offset+12, 2);
	for (i=0;i<num_rows;i++) {
		row_start = get_uint16(fbuf+offset+14+(2*i));
		str = g_strdup_printf("Row %d offset: 0x%02x (%u)", 
			i+1, row_start, row_start) ;
		gmdb_debug_add_item(store, NULL, str, offset+14+(2*i), 2);
	}
	for (i=0;i<num_rows;i++) {
		row_start = get_uint16(fbuf+offset+14+(2*i));
		if (i==0)
			row_len = mdb->fmt->pg_size - row_start;
		else
			row_len = (get_uint16(fbuf+offset+14+(i-1)*2) 
				& 0x0FFF) - row_start;
		str = g_strdup_printf("Row %d", i+1);
		/*container = */gmdb_debug_add_item(store, NULL, str,
			row_start, row_len); 
		
		/* usage pages have parent id of 0 (database) and do not 
		 * follow normal row format */
		/* if (tdef)
			gmdb_debug_dissect_row(store, container, fbuf, row_start, row_len);
		*/
	}
}
static void 
gmdb_debug_dissect_data_pg3(GtkTreeStore *store, char *fbuf, int offset, int len)
{
	gchar *str;
	int num_rows, i, row_start, row_len;
	guint32 tdef;
	GtkTreeIter *container;

	str = g_strdup_printf("Page free space: %u", get_uint16(fbuf+offset+2));
	gmdb_debug_add_item(store, NULL, str, offset+2, 2);

	tdef = get_uint32(fbuf+offset+4);
	str = g_strdup_printf("Parent's TDEF page: 0x%06x (%lu)", tdef,
		(unsigned long)tdef);
	gmdb_debug_add_item(store, NULL, str, offset+4, 4);

	num_rows = get_uint16(fbuf+offset+8);
	str = g_strdup_printf("Num rows: %u", num_rows);
	gmdb_debug_add_item(store, NULL, str, offset+8, 2);
	for (i=0;i<num_rows;i++) {
		row_start = get_uint16(fbuf+offset+10+(2*i));
		str = g_strdup_printf("Row %d offset: 0x%02x (%u)", 
			i+1, row_start, row_start) ;
		gmdb_debug_add_item(store, NULL, str, offset+10+(2*i), 2);
	}
	for (i=0;i<num_rows;i++) {
		row_start = get_uint16(fbuf+offset+10+(2*i));
		if (i==0)
			row_len = mdb->fmt->pg_size - row_start;
		else
			row_len = (get_uint16(fbuf+offset+10+(i-1)*2) 
				& 0x0FFF) - row_start;
		str = g_strdup_printf("Row %d", i+1);
		container = gmdb_debug_add_item(store, NULL, str,
			row_start, row_len); 
		
		/* usage pages have parent id of 0 (database) and do not 
		 * follow normal row format */
		if (tdef)
			gmdb_debug_dissect_row(store, container, fbuf,
				row_start, row_len);
	}
}
static void 
gmdb_debug_dissect_data_pg(GtkTreeStore *store, char *fbuf, int offset, int len)
{
	if (IS_JET3(mdb))
		gmdb_debug_dissect_data_pg3(store, fbuf, offset, len);
	else
		gmdb_debug_dissect_data_pg4(store, fbuf, offset, len);
}
static void 
gmdb_debug_dissect_tabledef_pg4(GtkTreeStore *store, char *fbuf, int offset, int len)
{
	gchar *str;
	guint32 i, num_idx, num_cols, idx_entries;
	int newbase, infobase;
	GtkTreeIter *container;
	char *foff = fbuf + offset;

	str = g_strdup_printf("Next TDEF Page: 0x%06x (%lu)",
		get_uint32(foff+4), (unsigned long)get_uint32(foff+4));
	gmdb_debug_add_item(store, NULL, str, offset+4, 4);
	str = g_strdup_printf("Length of Data: %lu",
		(unsigned long)get_uint32(foff+8));
	gmdb_debug_add_item(store, NULL, str, offset+8, 4);
	str = g_strdup_printf("Records: %lu",
		(unsigned long)get_uint32(foff+16));
	gmdb_debug_add_item(store, NULL, str, offset+16, 4);
	str = g_strdup_printf("Autonumber Value: %lu",
		(unsigned long)get_uint32(foff+20));
	gmdb_debug_add_item(store, NULL, str, offset+20, 4);
	str = g_strdup_printf("Table Type: 0x%02x (%s)", foff[40],
		gmdb_val_to_str(table_types, foff[40]));
	gmdb_debug_add_item(store, NULL, str, offset+40, 1);
	str = g_strdup_printf("Max Columns: %u", get_uint16(foff+41));
	gmdb_debug_add_item(store, NULL, str, offset+41, 2);
	str = g_strdup_printf("Var Columns: %u", get_uint16(foff+43));
	gmdb_debug_add_item(store, NULL, str, offset+43, 2);
	num_cols = get_uint16(foff+45);
	str = g_strdup_printf("Columns: %u", num_cols);
	gmdb_debug_add_item(store, NULL, str, offset+45, 2);
	idx_entries = get_uint32(foff+47);
	str = g_strdup_printf("Index Entries: %lu",
		(unsigned long)idx_entries);
	gmdb_debug_add_item(store, NULL, str, offset+47, 4);
	num_idx = get_uint32(foff+51);
	str = g_strdup_printf("Real Indices: %lu",
		(unsigned long)num_idx);
	gmdb_debug_add_item(store, NULL, str, offset+51, 4);

	gmdb_debug_add_page_ptr(store, NULL, fbuf, "Used Pages Pointer", offset+55);
	gmdb_debug_add_page_ptr(store, NULL, fbuf, "Pages Freespace Pointer", offset+59);
	newbase = offset+63;

	if (num_idx > 0) {
		str = g_strdup("Index Entries");
		container = gmdb_debug_add_item(store, NULL, str, -1, 0);
	}
	for (i=0;i<num_idx;i++) {
		/*GtkTreeIter *node;*/
		str = g_strdup_printf("Index %d", i+1);
		/*node = */gmdb_debug_add_item(store, container, str, newbase, 12);
		//gmdb_debug_dissect_index1(store, node, fbuf, offset+63);
		newbase += 12;
	}

	infobase = newbase;
	newbase += 25 * num_cols;
	str = g_strdup("Column Info");
	container = gmdb_debug_add_item(store, NULL, str, -1, 0);
	for (i=0;i<num_cols;i++) {
		GtkTreeIter *node/*, *subnode*/;
//		char *tmpstr;
		int namelen;

		str = g_strdup_printf("Column %d", i+1);
		node = gmdb_debug_add_item(store, container, str, -1, 0);

		str = g_strdup("Data");
		/*subnode = */gmdb_debug_add_item(store, node, str, infobase, 25);
		//gmdb_debug_dissect_column(store, subnode, fbuf, infobase);
		infobase += 25;

		str = g_strdup("Name");
		namelen = get_uint16(fbuf+newbase);
//		tmpstr = g_malloc(namelen + 1);
//		strncpy(tmpstr, &fbuf[newbase+1], namelen);
//		tmpstr[namelen]=0;
//		str = g_strdup_printf("Column %d: %s", i+1, tmpstr);
//		g_free(tmpstr);
		gmdb_debug_add_item(store, node, str, newbase+2, namelen);
		newbase += namelen+2;
	}
}
static void 
gmdb_debug_dissect_tabledef_pg3(GtkTreeStore *store, char *fbuf, int offset, int len)
{
	gchar *str;
	guint32 i, num_idx, num_cols, idx_entries;
	int newbase;
	GtkTreeIter *node, *container;
	char *foff = fbuf + offset;

	str = g_strdup_printf("Next TDEF Page: 0x%06x (%lu)", 
		get_uint32(foff+4), (unsigned long)get_uint32(foff+4));
	gmdb_debug_add_item(store, NULL, str, offset+4, 4);
	str = g_strdup_printf("Length of Data: %lu",
		(unsigned long)get_uint32(foff+8));
	gmdb_debug_add_item(store, NULL, str, offset+8, 4);
	str = g_strdup_printf("# of Records: %lu",
		(unsigned long)get_uint32(foff+12));
	gmdb_debug_add_item(store, NULL, str, offset+12, 4);
	str = g_strdup_printf("Autonumber Value: %lu",
		(unsigned long)get_uint32(foff+16));
	gmdb_debug_add_item(store, NULL, str, offset+16, 4);
	str = g_strdup_printf("Table Type: 0x%02x (%s)", foff[20],
		gmdb_val_to_str(table_types, foff[20]));
	gmdb_debug_add_item(store, NULL, str, offset+20, 1);
	str = g_strdup_printf("Max # of Columns: %u", get_uint16(foff+21));
	gmdb_debug_add_item(store, NULL, str, offset+21, 2);
	str = g_strdup_printf("# of VarCols: %u", get_uint16(foff+23));
	gmdb_debug_add_item(store, NULL, str, offset+23, 2);
	num_cols = get_uint16(foff+25);
	str = g_strdup_printf("# of Columns: %u", num_cols);
	gmdb_debug_add_item(store, NULL, str, offset+25, 2);
	idx_entries = get_uint32(foff+27);
	str = g_strdup_printf("# of Index Entries: %lu",
		(unsigned long)idx_entries);
	gmdb_debug_add_item(store, NULL, str, offset+27, 4);
	num_idx = get_uint32(foff+31);
	str = g_strdup_printf("# of Real Indices: %lu",
		(unsigned long)num_idx);

	gmdb_debug_add_item(store, NULL, str, offset+31, 4);
	gmdb_debug_add_page_ptr(store, NULL, fbuf, "Used Pages Pointer", offset+35);
	gmdb_debug_add_page_ptr(store, NULL, fbuf, "Pages Freespace Pointer", offset+39);
	newbase = offset + 43;

	str = g_strdup("Index Entries");
	container = gmdb_debug_add_item(store, NULL, str, -1, 0);
	for (i=0;i<num_idx;i++) {
		str = g_strdup_printf("Index %d", i+1);
		node = gmdb_debug_add_item(store, container, str, newbase, 8);
		gmdb_debug_dissect_index1(store, node, fbuf, offset+43);
		newbase += 8;
	}

	str = g_strdup("Column Data");
	container = gmdb_debug_add_item(store, NULL, str, -1, 0);
	for (i=0;i<num_cols;i++) {
		str = g_strdup_printf("Column %d", i+1);
		node = gmdb_debug_add_item(store, container, str, newbase, 18);
		gmdb_debug_dissect_column(store, node, fbuf, newbase);
		newbase += 18;
	}

	str = g_strdup("Column Names");
	container = gmdb_debug_add_item(store, NULL, str, -1, 0);
	for (i=0;i<num_cols;i++) {
		char *tmpstr;
		int namelen;

		namelen = fbuf[newbase];
		tmpstr = g_malloc(namelen + 1);
		strncpy(tmpstr, &fbuf[newbase+1], namelen);
		tmpstr[namelen]=0;
		str = g_strdup_printf("Column %d: %s", i+1, tmpstr);
		g_free(tmpstr);
		node = gmdb_debug_add_item(store, container, str,
			newbase + 1, namelen);
		newbase += namelen + 1;
	}
	str = g_strdup("Index definition 1");
	container = gmdb_debug_add_item(store, NULL, str, -1, 0);
	for (i=0;i<num_idx;i++) {
		str = g_strdup_printf("Index %d", i+1);
		node = gmdb_debug_add_item(store, container, str, newbase, 39);
		gmdb_debug_dissect_index2(store, node, fbuf, newbase);
		newbase += 39;
	}
	str = g_strdup("Index definition 2");
	container = gmdb_debug_add_item(store, NULL, str, -1, 0);
	for (i=0;i<idx_entries;i++) {
		str = g_strdup_printf("Index %d", i+1);
		node = gmdb_debug_add_item(store, container, str, newbase, 20);
		newbase += 20;
	}

	str = g_strdup("Index Names");
	container = gmdb_debug_add_item(store, NULL, str, -1, 0);
	for (i=0;i<idx_entries;i++) {
		char *tmpstr;
		int namelen;

		namelen = fbuf[newbase];
		tmpstr = g_malloc(namelen + 1);
		strncpy(tmpstr, &fbuf[newbase+1], namelen);
		tmpstr[namelen]=0;
		str = g_strdup_printf("Index %d: %s", i+1, tmpstr);
		node = gmdb_debug_add_item(store, container, str,
			newbase+1, namelen);
		g_free(tmpstr);
		newbase += namelen + 1;
	}
}
static void 
gmdb_debug_dissect_tabledef_pg(GtkTreeStore *store, char *fbuf, int offset, int len)
{
	if (IS_JET3(mdb))
		gmdb_debug_dissect_tabledef_pg3(store, fbuf, offset, len);
	else
		gmdb_debug_dissect_tabledef_pg4(store, fbuf, offset, len);
}
static void 
gmdb_debug_dissect(GtkTreeStore *store, char *fbuf, int offset, int len)
{
	gchar *str;

	str = g_strdup_printf("Page Type: 0x%02x (%s)", ((unsigned char*)fbuf)[offset],
		gmdb_val_to_str(object_types, fbuf[offset]));
	gmdb_debug_add_item(store, NULL, str, 0, 1);
	switch (fbuf[offset]) {
		case 0x00:
			//gmdb_debug_dissect_dbpage(store, fbuf, 1, len);
			break;
		case 0x01:
			gmdb_debug_dissect_data_pg(store, fbuf, 0, len);
			break;
		case 0x02:
			gmdb_debug_dissect_tabledef_pg(store, fbuf, 0, len);
			break;
		case 0x03:
			gmdb_debug_dissect_index_pg(store, fbuf, 0, len);
			break;
		case 0x04:
			gmdb_debug_dissect_leaf_pg(store, fbuf, 0, len);
			break;
	}
}

static void
gmdb_debug_clear(GladeXML *xml)
{
GtkTextBuffer *buffer;
GtkWidget *treeview, *textview, *store;

	textview = glade_xml_get_widget (xml, "debug_textview");
	treeview = glade_xml_get_widget (xml, "debug_treeview");
	store = (GtkWidget *) gtk_tree_view_get_model(GTK_TREE_VIEW(treeview));

	/* clear the tree */ 
	gtk_tree_store_clear(GTK_TREE_STORE(store));

	/* call delete text last because remove_node fires unselect signal */
	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (textview));
	gtk_text_buffer_set_text(buffer, "", 0);

}

static GtkTreeIter *
gmdb_debug_add_item(GtkTreeStore *store, GtkTreeIter *iter1, gchar *text, int start, int len)
{
	GtkTreeIter *iter2;

	iter2 = g_malloc(sizeof(GtkTreeIter));
	gtk_tree_store_append(store, iter2, iter1);
	gtk_tree_store_set(store, iter2, 0, text, 1, start, 2, len, -1);
	g_free(text);

	return iter2;
}

static void
gmdb_debug_text_on(GtkWidget *textbox, 
	int start_byte, int end_byte)
{
	GtkTextBuffer *buffer;
	GtkTextIter start, end;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (textbox));
	gtk_text_buffer_get_iter_at_offset (buffer, &start, start_byte);
	gtk_text_buffer_get_iter_at_offset (buffer, &end, end_byte);
	gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(textbox), 
		&start, 0.0, FALSE, 0.0, 0.0);
	gtk_text_buffer_apply_tag_by_name (buffer, "debug_on", &start, &end);
}

static void
gmdb_debug_text_off(GtkWidget *textbox)
{
	GtkTextBuffer *buffer;
	int end_byte;
	GtkTextIter start, end;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (textbox));
	end_byte = gtk_text_buffer_get_char_count(buffer);
	gtk_text_buffer_get_iter_at_offset (buffer, &start, 0);
	gtk_text_buffer_get_iter_at_offset (buffer, &end, end_byte);
	gtk_text_buffer_remove_all_tags (buffer, &start, &end);
}

static gint
gmdb_debug_delete_cb(GtkWidget *w, GladeXML *xml)
{
	return FALSE;
}
void
gmdb_debug_close_cb(GtkWidget *w, gpointer data)
{
	GladeXML *xml;

	fprintf(stderr, "close fired\n");
	xml = g_object_get_data(G_OBJECT(w), "debugwin_xml");
	debug_list = g_list_remove(debug_list, xml);
	gtk_widget_destroy(w);
}
void
gmdb_debug_close_all()
{
	GladeXML *xml;
	GtkWidget *win;

	fprintf(stderr, "close_all fired\n");
	while ((xml = g_list_nth_data(debug_list, 0))) {
		win = glade_xml_get_widget (xml, "debug_window");
		debug_list = g_list_remove(debug_list, xml);
		if (win) gtk_widget_destroy(win);
	}
}

void
gmdb_debug_new_cb(GtkWidget *w, gpointer data)
{
	GtkTextView *textview;
	GtkWidget *entry, *debugwin;
	GladeXML *debugwin_xml;
	GtkWidget *tree;
	GtkTreeStore *store;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;

	/* load the interface */
	debugwin_xml = glade_xml_new(GMDB_GLADEDIR "gmdb-debug.glade", NULL, NULL);
	/* connect the signals in the interface */
	fprintf(stderr, "autoconnect signals\n");
	glade_xml_signal_autoconnect(debugwin_xml);
	
	debug_list = g_list_append(debug_list, debugwin_xml);

	/* set signals with user data, anyone know how to do this in glade? */
	entry = glade_xml_get_widget (debugwin_xml, "debug_entry");

	debugwin = glade_xml_get_widget (debugwin_xml, "debug_window");
	g_signal_connect (G_OBJECT (debugwin), "delete_event",
		G_CALLBACK (gmdb_debug_delete_cb), debugwin_xml);
	g_object_set_data (G_OBJECT (debugwin), "debugwin_xml", debugwin_xml);

	/* this should be a preference, needs to be fixed width */
	textview = (GtkTextView *) glade_xml_get_widget (debugwin_xml, "debug_textview");
	gtk_widget_modify_font(GTK_WIDGET(textview), 
		pango_font_description_from_string("Courier"));
			                			                
	/* set up treeview, libglade only gives us the empty widget */
	tree = glade_xml_get_widget(debugwin_xml, "debug_treeview");
	store = gtk_tree_store_new(3, G_TYPE_STRING, G_TYPE_INT, G_TYPE_INT);
	gtk_tree_view_set_model(GTK_TREE_VIEW(tree), GTK_TREE_MODEL(store));

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("Field",
		renderer, "text", 0, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW (tree), column);

	select = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree));
	gtk_tree_selection_set_mode (select, GTK_SELECTION_SINGLE);
	g_signal_connect (G_OBJECT (select), "changed",
		G_CALLBACK (gmdb_debug_select_cb), debugwin_xml);

	/* check if initial page was passed */
	if (mdb) {
		gmdb_debug_init(mdb, debugwin_xml);
		if (data) {
			gchar *text;
			guint32 page = *((guint32 *)data);

			text = g_strdup_printf("%lu", (unsigned long)page);
			gtk_entry_set_text(GTK_ENTRY(entry), text);
			g_free(text);
			gmdb_debug_display_cb(entry, NULL);
		}
	}
}
void 
gmdb_debug_set_dissect_cb(GtkWidget *w, gpointer data)
{
	gboolean *dissect;
	
	dissect = g_object_get_data(G_OBJECT(w),"dissect");
	if (!dissect) return;
	//printf("here %d\n", *dissect);
	*dissect = !(*dissect);
	g_object_set_data(G_OBJECT(w), "dissect", dissect);
}
static void gmdb_debug_init(MdbHandle *mdb, GladeXML *xml)
{
	gchar *str;
	GtkWidget *pglabel, *entry, *window, *textview;
	GtkTextBuffer *buffer;
	gboolean *dissect;

	pglabel = glade_xml_get_widget (xml, "debug_num_label");
	str = g_strdup_printf("(0-%ld):", gmdb_get_max_page(mdb));
	gtk_label_set_text(GTK_LABEL(pglabel), str);
	g_free(str);

	entry = glade_xml_get_widget (xml, "debug_entry");
	gtk_widget_grab_focus(GTK_WIDGET(entry));

	window = glade_xml_get_widget (xml, "debug_window");
	dissect = g_malloc0(sizeof(guint));
	*dissect = TRUE;
	g_object_set_data(G_OBJECT(window), "dissect", dissect);

	textview = glade_xml_get_widget (xml, "debug_textview");
	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (textview));
	gtk_text_buffer_create_tag (buffer, "debug_on",
		"foreground", "white", "background", "blue", NULL);  
}
