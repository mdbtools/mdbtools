#include "gmdb.h"
#include <glade/glade.h>
extern GtkWidget *app;
extern MdbHandle *mdb;

GladeXML *debugwin_xml;

#define LINESZ 77

typedef struct GMdbDebugTab {
	GtkWidget *ctree;
	GtkWidget *textbox;
	GtkWidget *entry;
	GtkWidget *pglabel;
	int page_num;
	int max_page;
	int linesz;
	GdkFont *font;
	GdkColor black;
	GdkColor white;
} GMdbDebugTab;

typedef struct GMdbDebugRange {
	gint start_byte;
	gint end_byte;
} GMdbDebugRange;

GMdbDebugTab *dbug;

/* prototypes */
void gmdb_debug_text_on(GtkWidget *textbox, int start_byte, int end_byte);
void gmdb_debug_text_off(GtkWidget *textbox, int start_byte, int end_byte);
GtkTreeIter *gmdb_debug_add_item(GtkTreeStore *store, GtkTreeIter *iter, gchar *text, int start, int end);
void gmdb_debug_clear(GMdbDebugTab *dbug);
void gmdb_debug_dissect(GtkTreeStore *store, char *fbuf, int offset, int len);
static guint16 get_uint16(unsigned char *c);
static guint32 get_uint24(unsigned char *c);
static guint32 get_uint32(unsigned char *c);
static long gmdb_get_max_page(MdbHandle *mdb);

/* value to string stuff */
typedef struct GMdbValStr {
	gint value;
	gchar *string;
} GMdbValStr;

GMdbValStr table_types[] = {
	{ 0x4e, "System Table" },
	{ 0x53, "User Table" },
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
	{ 0x00, "Database Definition Page" },
	{ 0x01, "Data Page" },
	{ 0x02, "Table Definition Page" },
	{ 0x03, "Index Page" },
	{ 0x04, "Leaf Index Page" },
	{ 0, NULL }
};
/* callbacks */
void
gmdb_debug_select_cb(GtkCTree *tree, GList *node, gint column, GMdbDebugTab *dbug)
{
GMdbDebugRange *range;
int start_row, end_row;
int start_col, end_col;
int i;

	range = gtk_ctree_node_get_row_data(tree, GTK_CTREE_NODE(node));
	/* container node or otherwise non-represented in the data */
	if (range->start_byte == -1 || range->end_byte == -1) return;

	start_row = range->start_byte / 16;
	end_row = range->end_byte / 16;
	start_col = 8 + (range->start_byte % 16) * 3;
	end_col = 8 + (range->end_byte % 16) * 3;

	/* freeze/thaw needed because of redrawing glitch...only part of text had
	** correct colors, rest would come back on resize. */
	/*
	gtk_text_freeze(GTK_TEXT(dbug->textbox));
	*/
	if (start_row == end_row) {
		gmdb_debug_text_on(dbug->textbox, 
			dbug->linesz * start_row + start_col,
			dbug->linesz * start_row + end_col + 2);
		gmdb_debug_text_on(dbug->textbox, 
			dbug->linesz * start_row + 59 + (range->start_byte % 16),
			dbug->linesz * start_row + 59 + (range->end_byte % 16) + 1);
	} else {
		gmdb_debug_text_on(dbug->textbox, 
			dbug->linesz * start_row + start_col,
			/* 55 = 8 (addr) + 15 (bytes) * 3 (%02x " ") + 2 (last byte) */
			dbug->linesz * start_row + 55); 
		gmdb_debug_text_on(dbug->textbox, 
			dbug->linesz * start_row + 59 + (range->start_byte % 16),
			dbug->linesz * start_row + 75); 
		for (i=start_row + 1; i < end_row; i++) {
			gmdb_debug_text_on(dbug->textbox, 
				dbug->linesz * i + 8, dbug->linesz * i + 55);
			gmdb_debug_text_on(dbug->textbox, 
				dbug->linesz * i + 59, dbug->linesz * i + 75);
		}
		gmdb_debug_text_on(dbug->textbox, 
			dbug->linesz * end_row + 8,
			dbug->linesz * end_row + end_col + 2); 
		gmdb_debug_text_on(dbug->textbox,
			dbug->linesz * end_row + 59,
			dbug->linesz * end_row + 59 + (range->end_byte % 16) + 1); 
	}
	/* gtk_text_thaw(GTK_TEXT(dbug->textbox)); */
}
void
gmdb_debug_unselect_cb(GtkCTree *tree, GList *node, gint column, GMdbDebugTab *dbug)
{
GMdbDebugRange *range;
int start_row, end_row;
int start_col, end_col;
int i;

	range = gtk_ctree_node_get_row_data(tree, GTK_CTREE_NODE(node));
	/* container node or otherwise non-represented in the data */
	if (range->start_byte == -1 || range->end_byte == -1) return;

	start_row = range->start_byte / 16;
	end_row = range->end_byte / 16;
	start_col = 8 + (range->start_byte % 16) * 3;
	end_col = 8 + (range->end_byte % 16) * 3;

	if (start_row == end_row) {
		gmdb_debug_text_off(dbug->textbox, 
			dbug->linesz * start_row + start_col,
			dbug->linesz * start_row + end_col + 2);
		gmdb_debug_text_off(dbug->textbox,
			dbug->linesz * start_row + 59 + (range->start_byte % 16),
			dbug->linesz * start_row + 59 + (range->end_byte % 16) + 1);
	} else {
		gmdb_debug_text_off(dbug->textbox,
			dbug->linesz * start_row + start_col,
			/* 55 = 8 (addr) + 15 (bytes) * 3 (%02x " ") + 2 (last byte) */
			dbug->linesz * start_row + 55); 
		gmdb_debug_text_off(dbug->textbox,
			dbug->linesz * start_row + 59 + (range->start_byte % 16),
			dbug->linesz * start_row + 75); 
		for (i=start_row + 1; i < end_row; i++) {
			gmdb_debug_text_off(dbug->textbox,
				dbug->linesz * i + 8, dbug->linesz * i + 55);
			gmdb_debug_text_off(dbug->textbox, 
				dbug->linesz * i + 59, dbug->linesz * i + 75);
		}
		gmdb_debug_text_off(dbug->textbox, 
			dbug->linesz * end_row + 8,
			dbug->linesz * end_row + end_col + 2); 
		gmdb_debug_text_off(dbug->textbox, 
			dbug->linesz * end_row + 59,
			dbug->linesz * end_row + 59 + (range->end_byte % 16) + 1); 
	}
}
void
gmdb_debug_display_cb(GtkWidget *w, gpointer *dbug)
{
int page;
off_t pos;
unsigned char *fbuf;
unsigned char *tbuf;
int i,j;
int length;
char line[80];
char field[10];
GtkTextBuffer *buffer;
GtkTextIter iter;
GtkWidget *entry;
GtkTextView *textview;

	if (!mdb) return;

	entry = glade_xml_get_widget (debugwin_xml, "debug_entry");
	textview = glade_xml_get_widget (debugwin_xml, "debug_textview");
	
	page = atol(gtk_entry_get_text(GTK_ENTRY(entry)));
	if (page>gmdb_get_max_page(mdb) || page<0) {
		gmdb_info_msg("Page entered is outside valid page range.");
	}

	// gmdb_debug_clear(dbug);

	pos = lseek(mdb->fd, 0, SEEK_CUR);
	lseek(mdb->fd, page * mdb->pg_size, SEEK_SET);
	
	fbuf = (unsigned char *) malloc(mdb->pg_size);
	tbuf = (unsigned char *) malloc( (mdb->pg_size / 16) * 80);
	memset(tbuf, 0, (mdb->pg_size / 16) * 80);
	length = read(mdb->fd, fbuf, mdb->pg_size);
	if (length<mdb->pg_size) {
	}
	i = 0;
	while (i<length) {
		sprintf(line,"%06x  ", i);

		for(j=0; j<16; j++) {
			sprintf(field, "%02x ", fbuf[i+j]);
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
		//if (!dbug->linesz) dbug->linesz = strlen(line);
	}
	buffer = gtk_text_view_get_buffer(textview);
	gtk_text_buffer_get_iter_at_offset (buffer, &iter, 0);
	gtk_text_buffer_insert(buffer,&iter,tbuf,strlen(tbuf));
	//gtk_editable_select_region(GTK_EDITABLE(dbug->textbox), 9, 15);
	
	GtkWidget *tree = glade_xml_get_widget(debugwin_xml, "debug_treeview");
	GtkTreeStore *store = gtk_tree_store_new(1, G_TYPE_STRING);
	gmdb_debug_dissect(store, fbuf, 0, length);
	gtk_tree_view_set_model(GTK_TREE_VIEW(tree), store);

	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("Field",
		renderer, "text", 0, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW (tree), column);
	free(fbuf);
	free(tbuf);
}
/* functions */
static long
gmdb_get_max_page(MdbHandle *mdb)
{
struct stat st;

	assert( fstat(mdb->fd, &st)!=-1 );
	return st.st_size/mdb->pg_size;
}
gchar *
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
	} while (*strptr);
	return "unknown";
}
static guint16 
get_uint16(unsigned char *c)
{
guint16 i;

i =c[1]; i<<=8;
i+=c[0];

return i;
}
static guint32 
get_uint24(unsigned char *c)
{
guint32 l;

l =c[2]; l<<=8;
l+=c[1]; l<<=8;
l+=c[0];

return l;
}
static guint32 
get_uint32(unsigned char *c)
{
guint32 l;

l =c[3]; l<<=8;
l+=c[2]; l<<=8;
l+=c[1]; l<<=8;
l+=c[0];

return l;
}
void 
gmdb_debug_dissect_column(GtkTreeStore *store, GtkTreeIter *parent, char *fbuf, int offset)
{
gchar str[100];
GtkCTreeNode *node;

	snprintf(str, 100, "Column Type: 0x%02x (%s)", fbuf[offset],
		gmdb_val_to_str(column_types, fbuf[offset]));
	gmdb_debug_add_item(store, parent, str, offset, offset);
	snprintf(str, 100, "Column #: %d", get_uint16(&fbuf[offset+1]));
	gmdb_debug_add_item(store, parent, str, offset+1, offset+2);
	snprintf(str, 100, "VarCol Offset: %d", get_uint16(&fbuf[offset+3]));
	gmdb_debug_add_item(store, parent, str, offset+3, offset+4);
	snprintf(str, 100, "Unknown", get_uint32(&fbuf[offset+5]));
	gmdb_debug_add_item(store, parent, str, offset+5, offset+8);
	snprintf(str, 100, "Unknown", get_uint32(&fbuf[offset+9]));
	gmdb_debug_add_item(store, parent, str, offset+9, offset+12);
	snprintf(str, 100, "Variable Column: %s", 
		fbuf[offset+13] & 0x01  ? "No" : "Yes");
	gmdb_debug_add_item(store, parent, str, offset+13, offset+13);
	snprintf(str, 100, "Fixed Col Offset: %d", get_uint16(&fbuf[offset+14]));
	gmdb_debug_add_item(store, parent, str, offset+14, offset+15);
	snprintf(str, 100, "Column Length: %d", get_uint16(&fbuf[offset+16]));
	gmdb_debug_add_item(store, parent, str, offset+16, offset+17);
}
void 
gmdb_debug_dissect_index1(GtkTreeStore *store, GtkTreeIter *parent, char *fbuf, int offset)
{
gchar str[100];
GtkCTreeNode *node;

	snprintf(str, 100, "Unknown");
	gmdb_debug_add_item(store, parent, str, offset, offset+3);
	snprintf(str, 100, "Rows in Index: %lu", get_uint32(&fbuf[offset+4])); 
	gmdb_debug_add_item(store, parent, str, offset+4, offset+7);
}
void 
gmdb_debug_add_page_ptr(GtkTreeStore *store, GtkTreeIter *parent, char *fbuf, const char *label, int offset)
{
gchar str[100];
GtkCTreeNode *node;

	snprintf(str, 100, "%s", label);
	node = gmdb_debug_add_item(store, parent, str, offset, offset+3);

	snprintf(str, 100, "Row Number: %u", fbuf[offset]);
	gmdb_debug_add_item(store, node, str, offset, offset);
	snprintf(str, 100, "Page Number: %lu", get_uint24(&fbuf[offset+1])); 
	gmdb_debug_add_item(store, node, str, offset+1, offset+3);
}
void 
gmdb_debug_dissect_tabledef_pg(GtkTreeStore *store, char *fbuf, int offset, int len)
{
gchar str[100];
guint32 i, num_idx, num_cols;
int newbase;
GtkTreeIter *node, *container;

	snprintf(str, 100, "Next TDEF Page: 0x%06x (%lu)", 
		get_uint32(&fbuf[offset+4]), get_uint32(&fbuf[offset+4]));
	gmdb_debug_add_item(store, NULL, str, offset+4, offset+7);
	snprintf(str, 100, "Length of Data: %lu", get_uint32(&fbuf[offset+8]));
	gmdb_debug_add_item(store, NULL, str, offset+8, offset+11);
	snprintf(str, 100, "# of Records: %lu", get_uint32(&fbuf[offset+12]));
	gmdb_debug_add_item(store, NULL, str, offset+12, offset+15);
	snprintf(str, 100, "Autonumber Value: %lu", get_uint32(&fbuf[offset+16]));
	gmdb_debug_add_item(store, NULL, str, offset+16, offset+19);
	snprintf(str, 100, "Table Type: 0x%02x (%s)", fbuf[offset+20],
		gmdb_val_to_str(table_types, fbuf[offset+20]));
	gmdb_debug_add_item(store, NULL, str, offset+20, offset+20);
	num_cols = get_uint16(&fbuf[offset+21]);
	snprintf(str, 100, "# of Columns: %u", num_cols);
	gmdb_debug_add_item(store, NULL, str, offset+21, offset+22);
	snprintf(str, 100, "# of VarCols: %u", 
		get_uint16(&fbuf[offset+23]));
	gmdb_debug_add_item(store, NULL, str, offset+23, offset+24);
	snprintf(str, 100, "# of Columns: %u", 
		get_uint16(&fbuf[offset+25]));
	gmdb_debug_add_item(store, NULL, str, offset+25, offset+26);
	snprintf(str, 100, "# of Index Entries: %lu", 
		get_uint32(&fbuf[offset+27]));
	gmdb_debug_add_item(store, NULL, str, offset+27, offset+30);

	num_idx = get_uint32(&fbuf[offset+31]);
	snprintf(str, 100, "# of Real Indices: %lu", num_idx);

	gmdb_debug_add_item(store, NULL, str, offset+31, offset+34);
	gmdb_debug_add_page_ptr(store, NULL, fbuf, "Used Pages Pointer", offset+35);

	container = gmdb_debug_add_item(store, NULL, "Index Entries", -1, -1);
	for (i=0;i<num_idx;i++) {
		snprintf(str, 100, "Index %d", i+1);
		node = gmdb_debug_add_item(store, container, str, offset+43+(8*i), offset+43+(8*i)+7);
		gmdb_debug_dissect_index1(store, node, fbuf, offset+43+(8*i));
	}
	newbase = offset + 43 + (8*i);

	container = gmdb_debug_add_item(store, NULL, "Column Data", -1, -1);
	for (i=0;i<num_cols;i++) {
		snprintf(str, 100, "Column %d", i+1);
		node = gmdb_debug_add_item(store, container, str, newbase + (18*i), newbase + (18*i) + 17);
		gmdb_debug_dissect_column(store, node, fbuf, newbase + (18*i));
	}
	newbase += 18*num_cols;

	container = gmdb_debug_add_item(store, NULL, "Column Names", -1, -1);
	for (i=0;i<num_cols;i++) {
		char *tmpstr;
		int namelen;

		namelen = fbuf[newbase];
		tmpstr = malloc(namelen + 1);
		strncpy(tmpstr, &fbuf[newbase+1], namelen);
		tmpstr[namelen]=0;
		snprintf(str, 100, "Column %d: %s", i+1, tmpstr);
		free(tmpstr);
		node = gmdb_debug_add_item(store, container, str, newbase + 1, newbase + namelen);
		newbase += namelen + 1;
	}
}
void gmdb_debug_dissect(GtkTreeStore *store, char *fbuf, int offset, int len)
{
gchar str[100];


	snprintf(str, 100, "Object Type: 0x%02x (%s)", fbuf[offset],
		gmdb_val_to_str(object_types, fbuf[offset]));
	gmdb_debug_add_item(store, NULL, str, 0, 0);
	switch (fbuf[offset]) {
		case 0x00:
			//gmdb_debug_dissect_dbpage(store, fbuf, 1, len);
			break;
		case 0x01:
			//gmdb_debug_dissect_dbpage(store, fbuf, 1, len);
			break;
		case 0x02:
			gmdb_debug_dissect_tabledef_pg(store, fbuf, 0, len);
			break;
		case 0x03:
			//gmdb_debug_dissect_dbpage(store, fbuf, 1, len);
			break;
		case 0x04:
			//gmdb_debug_dissect_dbpage(store, fbuf, 1, len);
			break;
	}
}

gmdb_clear_node_cb(GtkWidget *ctree, GtkCTreeNode *node, gpointer data)
{
gpointer rowdata;

	rowdata = gtk_ctree_node_get_row_data(GTK_CTREE(ctree), node);
	g_free(rowdata);
	gtk_ctree_remove_node(GTK_CTREE(ctree), node);
}

void
gmdb_debug_clear(GMdbDebugTab *dbug)
{
GtkCTreeNode *node;
gpointer data;
GtkTextBuffer *buffer;

	/* clear the tree */ 
	gtk_ctree_post_recursive(dbug->ctree, NULL, gmdb_clear_node_cb, NULL);

/*
	node = gtk_ctree_node_nth(GTK_CTREE(dbug->ctree), 0);
	data = gtk_ctree_node_get_row_data(GTK_CTREE(dbug->ctree), node);
	g_free(data);
	gtk_ctree_remove_node(GTK_CTREE(dbug->ctree), node);
*/

	/* call delete text last because remove_node fires unselect signal */
	//gtk_editable_delete_text(GTK_EDITABLE(dbug->textbox),0, -1);
	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (dbug->textbox));
	gtk_text_buffer_set_text(buffer, "", 0);

}

GtkTreeIter *
gmdb_debug_add_item(GtkTreeStore *store, GtkTreeIter *iter1, gchar *text, int start, int end)
{
gchar *nodetext[2];
GtkCTreeNode *node;
GMdbDebugRange *range;
GtkTreeIter *iter2;

	iter2 = g_malloc(sizeof(GtkTreeIter));
	gtk_tree_store_append(store, iter2, iter1);
	gtk_tree_store_set(store, iter2, 0, text, -1);
#if 0
	nodetext[0] = text;
	nodetext[1] = "0";
	node = gtk_ctree_insert_node(GTK_CTREE(dbug->ctree), parent, NULL, nodetext, 0, NULL, NULL, NULL, NULL, FALSE, FALSE);
	range = g_malloc(sizeof(GMdbDebugRange));
	range->start_byte = start;
	range->end_byte = end;
	gtk_ctree_node_set_row_data(GTK_CTREE(dbug->ctree), node, range);
#endif

	return iter2;
}

void
gmdb_debug_text_on(GtkWidget *textbox, 
	int start_byte, int end_byte)
{
gchar *text;
GtkTextBuffer *buffer;
GtkTextTag *tag;
GtkTextIter start, end;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (textbox));

	tag = gtk_text_buffer_create_tag (buffer, NULL,
		"foreground", "white", NULL);  
	tag = gtk_text_buffer_create_tag (buffer, NULL,
		"background", "blue", NULL);  

	gtk_text_buffer_get_iter_at_offset (buffer, &start, start_byte);
	gtk_text_buffer_get_iter_at_offset (buffer, &end, end_byte);
	gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(dbug->textbox), 
		&start, 0.0, FALSE, 0.0, 0.0);
	gtk_text_buffer_apply_tag (buffer, tag, &start, &end);
}

static void
gmdb_debug_text_off(GtkWidget *textbox, 
	int start_byte, int end_byte)
{
gchar *text;
GtkTextBuffer *buffer;
GtkTextTag *tag;
GtkTextIter start, end;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (textbox));

	tag = gtk_text_buffer_create_tag (buffer, NULL,
		"foreground", "black", NULL);  
	tag = gtk_text_buffer_create_tag (buffer, NULL,
		"background", "white", NULL);  

	gtk_text_buffer_get_iter_at_offset (buffer, &start, start_byte);
	gtk_text_buffer_get_iter_at_offset (buffer, &end, end_byte);
	gtk_text_buffer_apply_tag (buffer, tag, &start, &end);
}

#if 0
void
gmdb_debug_tab_new(GtkWidget *notebook)
{
GtkWidget *tabbox;
GtkWidget *label;
GtkWidget *label1;
GtkWidget *button;
GtkWidget *pixmapwid;
GtkWidget *frame;
GtkWidget *hbox;
GtkWidget *hpane;
GtkWidget *vbox;
GtkWidget *scroll;
GtkWidget *scroll2;
GdkPixmap *pixmap;
GdkBitmap *mask;
GdkColormap *cmap;

	dbug = g_malloc0(sizeof(GMdbDebugTab));
	dbug->max_page = -1;
	dbug->font = gdk_font_load("-*-*-medium-r-normal-*-10-*-*-*-m-*-*-*");
	cmap = gdk_colormap_get_system();
	gdk_color_white(cmap, &dbug->white);
	gdk_color_black(cmap, &dbug->black);

	vbox = gtk_vbox_new (FALSE,5);
	gtk_widget_show (vbox);

	hbox = gtk_hbox_new (FALSE,5);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 10);

	label1 = gtk_label_new ("Page Number");
	gtk_widget_show (label1);
	gtk_box_pack_start (GTK_BOX (hbox), label1, FALSE, FALSE, 10);

	dbug->pglabel = gtk_label_new (":");
	gtk_widget_show (dbug->pglabel);
	gtk_box_pack_start (GTK_BOX (hbox), dbug->pglabel, FALSE, FALSE, 0);

	dbug->entry = gtk_entry_new ();
	gtk_widget_show (dbug->entry);
	gtk_box_pack_start (GTK_BOX (hbox), dbug->entry, TRUE, TRUE, 0);
	gtk_signal_connect ( GTK_OBJECT (dbug->entry),
		 "activate", GTK_SIGNAL_FUNC (gmdb_debug_display_cb), dbug);

	button = gtk_button_new_with_label ("Display");
	gtk_widget_show (button);
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);

	gtk_signal_connect ( GTK_OBJECT (button),
		 "clicked", GTK_SIGNAL_FUNC (gmdb_debug_display_cb), dbug);

	hpane = gtk_hpaned_new ();
	gtk_paned_set_position(GTK_PANED(hpane), 200);
    gtk_paned_set_gutter_size(GTK_PANED(hpane), 12);
	gtk_widget_show(hpane);
	gtk_box_pack_start (GTK_BOX (vbox), hpane, TRUE, TRUE, 10);

	scroll = gtk_scrolled_window_new(NULL,NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
		GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_widget_show (scroll);
	gtk_container_add(GTK_CONTAINER(hpane), scroll);

	dbug->ctree = gtk_ctree_new (1, 0);
	gtk_widget_show (dbug->ctree);
	gtk_container_add (GTK_CONTAINER (scroll), dbug->ctree);

	gtk_signal_connect ( GTK_OBJECT (dbug->ctree),
		"tree-select-row", GTK_SIGNAL_FUNC (gmdb_debug_select_cb), dbug);
	gtk_signal_connect ( GTK_OBJECT (dbug->ctree),
		"tree-unselect-row", GTK_SIGNAL_FUNC (gmdb_debug_unselect_cb), dbug);
	
	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
	gtk_container_set_border_width (GTK_CONTAINER (frame), 0);
	//gtk_widget_set_usize (frame, 100, 75);
	gtk_widget_show (frame);
	gtk_container_add (GTK_CONTAINER (hpane), frame);

	scroll2 = gtk_scrolled_window_new(NULL,NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll2),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_widget_show (scroll2);
	gtk_container_add(GTK_CONTAINER(frame), scroll2);

	dbug->textbox = gtk_text_view_new ();
	gtk_widget_modify_font(dbug->textbox, 
		pango_font_description_from_string("Courier"));
	gtk_widget_show (dbug->textbox);
	gtk_container_add(GTK_CONTAINER(scroll2), dbug->textbox);

	/* set selection callback for list */
	//gtk_signal_connect ( GTK_OBJECT (table_list),
		// "select-child", GTK_SIGNAL_FUNC (gmdb_table_select_cb), NULL);

	/* create a picture/label box and use for tab */
    tabbox = gtk_hbox_new (FALSE,5);
	pixmap = gdk_pixmap_colormap_create_from_xpm_d( NULL,  
		gtk_widget_get_colormap(app), &mask, NULL, debug_xpm);

	/* a pixmap widget to contain the pixmap */
	pixmapwid = gtk_pixmap_new( pixmap, mask );
	gtk_widget_show( pixmapwid );
	gtk_box_pack_start (GTK_BOX (tabbox), pixmapwid, FALSE, TRUE, 0);

	label = gtk_label_new ("Debug");
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (tabbox), label, FALSE, TRUE, 0);

	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), vbox, tabbox);

	if (mdb) gmdb_debug_init(mdb);
}
#endif
void
gmdb_debug_close_cb(GtkWidget *w, gpointer *data)
{
	GtkWidget *w;
	w = glade_xml_get_widget (debugwin_xml, "debug_window");
	gtk_widget_destroy(w);
}
void
gmdb_debug_new_cb(GtkWidget *w, gpointer *data)
{
GtkTextView *textview;

	/* load the interface */
	debugwin_xml = glade_xml_new("gladefiles/gmdb-debug.glade", NULL, NULL);
	/* connect the signals in the interface */
	glade_xml_signal_autoconnect(debugwin_xml);
	
	/* this should be a preference, needs to be fixed width */
	textview = glade_xml_get_widget (debugwin_xml, "debug_textview");
	gtk_widget_modify_font(textview, 
		pango_font_description_from_string("Courier"));
			                			                
	if (mdb) gmdb_debug_init(mdb);
/*
	GtkTreeIter iter;
	GtkWidget *tree = glade_xml_get_widget(debugwin_xml, "debug_treeview");
	GtkTreeStore *store = gtk_tree_store_new(1, G_TYPE_STRING);
	gtk_tree_store_append(store, &iter, NULL);
	gtk_tree_store_set(store, &iter, 0, "Test", -1);
	gtk_tree_view_set_model(GTK_TREE_VIEW(tree), store);

	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("Field",
		renderer, "text", 0, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW (tree), column);
	*/
}
void gmdb_debug_init(MdbHandle *mdb)
{
struct stat st;
char tmpstr[100];
GtkWidget *pglabel, *entry;

	pglabel = glade_xml_get_widget (debugwin_xml, "debug_num_label");
	sprintf(tmpstr, "(0-%d):", gmdb_get_max_page(mdb));
	gtk_label_set_text(GTK_LABEL(pglabel), tmpstr);
	entry = glade_xml_get_widget (debugwin_xml, "debug_entry");
	gtk_widget_grab_focus(GTK_WIDGET(entry));
}
