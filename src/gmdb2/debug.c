#include "gmdb.h"
#include <glade/glade.h>
extern GtkWidget *app;
extern MdbHandle *mdb;

GladeXML *debugwin_xml;

#define LINESZ 77

/* prototypes */
static void gmdb_debug_init(MdbHandle *mdb, GladeXML *xml);
static void gmdb_debug_text_on(GtkWidget *textbox, int start_byte, int end_byte);
static void gmdb_debug_text_off(GtkWidget *textbox);
GtkTreeIter *gmdb_debug_add_item(GtkTreeStore *store, GtkTreeIter *iter, gchar *text, int start, int end);
static void gmdb_debug_clear(GladeXML *xml);
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
	{ 0x4e, "User Table" },
	{ 0x53, "System Table" },
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
gmdb_debug_select_cb(GtkTreeSelection *select, GladeXML *xml)
{
int start_row, end_row;
int start_col, end_col;
int i;
GtkTreeIter iter;
GtkTreeModel *model;
gchar *fieldname;
gint32 start, end;
GtkWidget *textview;

	if (!select) return;

	if (gtk_tree_selection_get_selected (select, &model, &iter)) {
		gtk_tree_model_get (model, &iter, 0, &fieldname, 
			1, &start, 
			2, &end, -1);
		g_free (fieldname);
	}

	if (start == -1 || end == -1) return;

	start_row = start / 16;
	end_row = end / 16;
	start_col = 8 + (start % 16) * 3;
	end_col = 8 + (end % 16) * 3;

	textview = glade_xml_get_widget (xml, "debug_textview");
	gmdb_debug_text_off(textview);

	if (start_row == end_row) {
		gmdb_debug_text_on(textview, 
			LINESZ * start_row + start_col,
			LINESZ * start_row + end_col + 2);
		gmdb_debug_text_on(textview, 
			LINESZ * start_row + 59 + (start % 16),
			LINESZ * start_row + 59 + (end % 16) + 1);
	} else {
		gmdb_debug_text_on(textview, 
			LINESZ * start_row + start_col,
			/* 55 = 8 (addr) + 15 (bytes) * 3 (%02x " ") + 2 (last byte) */
			LINESZ * start_row + 55); 
		gmdb_debug_text_on(textview, 
			LINESZ * start_row + 59 + (start % 16),
			LINESZ * start_row + 75); 
		for (i=start_row + 1; i < end_row; i++) {
			gmdb_debug_text_on(textview, 
				LINESZ * i + 8, LINESZ * i + 55);
			gmdb_debug_text_on(textview, 
				LINESZ * i + 59, LINESZ * i + 75);
		}
		gmdb_debug_text_on(textview, 
			LINESZ * end_row + 8,
			LINESZ * end_row + end_col + 2); 
		gmdb_debug_text_on(textview,
			LINESZ * end_row + 59,
			LINESZ * end_row + 59 + (end % 16) + 1); 
	}
}
void
gmdb_debug_display_cb(GtkWidget *w, GladeXML *xml)
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

	entry = glade_xml_get_widget (xml, "debug_entry");
	textview = glade_xml_get_widget (xml, "debug_textview");
	
	page = atol(gtk_entry_get_text(GTK_ENTRY(entry)));
	if (page>gmdb_get_max_page(mdb) || page<0) {
		gmdb_info_msg("Page entered is outside valid page range.");
	}

	gmdb_debug_clear(xml);

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
	}
	buffer = gtk_text_view_get_buffer(textview);
	gtk_text_buffer_get_iter_at_offset (buffer, &iter, 0);
	gtk_text_buffer_insert(buffer,&iter,tbuf,strlen(tbuf));
	
	GtkWidget *tree = glade_xml_get_widget(xml, "debug_treeview");
	GtkTreeView *store = gtk_tree_view_get_model(GTK_TREE_VIEW(tree));

	gmdb_debug_dissect(store, fbuf, 0, length);

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
GtkTreeIter *node;

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

static void
gmdb_debug_clear(GladeXML *xml)
{
gpointer data;
GtkTextBuffer *buffer;
GtkWidget *treeview, *textview, *store;

	textview = glade_xml_get_widget (xml, "debug_textview");
	treeview = glade_xml_get_widget (xml, "debug_treeview");
	store = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview));

	/* clear the tree */ 
	gtk_tree_store_clear(GTK_TREE_STORE(store));

	/* call delete text last because remove_node fires unselect signal */
	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (textview));
	gtk_text_buffer_set_text(buffer, "", 0);

}

GtkTreeIter *
gmdb_debug_add_item(GtkTreeStore *store, GtkTreeIter *iter1, gchar *text, int start, int end)
{
GtkTreeIter *iter2;

	iter2 = g_malloc(sizeof(GtkTreeIter));
	gtk_tree_store_append(store, iter2, iter1);
	gtk_tree_store_set(store, iter2, 0, text, 1, start, 2, end, -1);

	return iter2;
}

static void
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
	gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(textbox), 
		&start, 0.0, FALSE, 0.0, 0.0);
	gtk_text_buffer_apply_tag (buffer, tag, &start, &end);
}

static void
gmdb_debug_text_off(GtkWidget *textbox)
{
gchar *text;
GtkTextBuffer *buffer;
GtkTextTag *tag;
int end_byte;
GtkTextIter start, end;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (textbox));

	tag = gtk_text_buffer_create_tag (buffer, NULL,
		"foreground", "black", NULL);  
	tag = gtk_text_buffer_create_tag (buffer, NULL,
		"background", "white", NULL);  

	end_byte = gtk_text_buffer_get_char_count(buffer);
	gtk_text_buffer_get_iter_at_offset (buffer, &start, 0);
	gtk_text_buffer_get_iter_at_offset (buffer, &end, end_byte);

	gtk_text_buffer_apply_tag (buffer, tag, &start, &end);
}

gint
gmdb_debug_delete_cb(GtkWidget *w, GladeXML *xml)
{
	return FALSE;
}
void
gmdb_debug_close_cb(GtkWidget *w, GladeXML *xml)
{
	GtkWidget *win;
	win = glade_xml_get_widget (xml, "debug_window");
	if (win) gtk_widget_destroy(win);
}
void
gmdb_debug_new_cb(GtkWidget *w, gpointer *data)
{
GtkTextView *textview;
guint32 page;
GtkWidget *entry, *mi, *button, *debugwin;
gchar text[20];

	/* load the interface */
	debugwin_xml = glade_xml_new("gladefiles/gmdb-debug.glade", NULL, NULL);
	/* connect the signals in the interface */
	glade_xml_signal_autoconnect(debugwin_xml);
	
	/* set signals with user data, anyone know how to do this in glade? */
	entry = glade_xml_get_widget (debugwin_xml, "debug_entry");
	g_signal_connect (G_OBJECT (entry), "activate",
		G_CALLBACK (gmdb_debug_display_cb), debugwin_xml);
	
	mi = glade_xml_get_widget (debugwin_xml, "close_menu");
	g_signal_connect (G_OBJECT (mi), "activate",
		G_CALLBACK (gmdb_debug_close_cb), debugwin_xml);
	
	button = glade_xml_get_widget (debugwin_xml, "debug_button");
	g_signal_connect (G_OBJECT (button), "clicked",
		G_CALLBACK (gmdb_debug_display_cb), debugwin_xml);
	
	debugwin = glade_xml_get_widget (debugwin_xml, "debug_window");
	gtk_signal_connect (GTK_OBJECT (debugwin), "delete_event",
		GTK_SIGNAL_FUNC (gmdb_debug_delete_cb), debugwin_xml);

	/* this should be a preference, needs to be fixed width */
	textview = glade_xml_get_widget (debugwin_xml, "debug_textview");
	gtk_widget_modify_font(textview, 
		pango_font_description_from_string("Courier"));
			                			                
	/* set up treeview, libglade only gives us the empty widget */
	GtkWidget *tree = glade_xml_get_widget(debugwin_xml, "debug_treeview");
	GtkTreeStore *store = gtk_tree_store_new(3, G_TYPE_STRING, G_TYPE_INT, G_TYPE_INT);
	gtk_tree_view_set_model(GTK_TREE_VIEW(tree), GTK_TREE_MODEL(store));

	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("Field",
		renderer, "text", 0, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW (tree), column);

	GtkTreeSelection *select = 
		gtk_tree_view_get_selection (GTK_TREE_VIEW (tree));
	gtk_tree_selection_set_mode (select, GTK_SELECTION_SINGLE);
	g_signal_connect (G_OBJECT (select), "changed",
		G_CALLBACK (gmdb_debug_select_cb), debugwin_xml);

	/* check if initial page was passed */
	if (mdb) {
		gmdb_debug_init(mdb, debugwin_xml);
		if (data) {
			page = *((guint32 *)data);
			sprintf(text,"%lu",page);
			gtk_entry_set_text(GTK_ENTRY(entry),text);
			gmdb_debug_display_cb(w, debugwin_xml);
		}
	}
}
static void gmdb_debug_init(MdbHandle *mdb, GladeXML *xml)
{
struct stat st;
char tmpstr[100];
GtkWidget *pglabel, *entry;

	pglabel = glade_xml_get_widget (xml, "debug_num_label");
	sprintf(tmpstr, "(0-%d):", gmdb_get_max_page(mdb));
	gtk_label_set_text(GTK_LABEL(pglabel), tmpstr);
	entry = glade_xml_get_widget (xml, "debug_entry");
	gtk_widget_grab_focus(GTK_WIDGET(entry));
}
