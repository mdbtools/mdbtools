#include "gmdb.h"
#include "gtkhlist.h"

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

GList *window_list;

extern GtkWidget *app;
extern MdbHandle *mdb;
extern MdbSQL *sql;


void gmdb_sql_ctree_populate(MdbHandle *mdb, GMdbSQLWindow *sqlwin);

/* callbacks */
gint
gmdb_sql_close(GtkHList *hlist, GtkWidget *w, GMdbSQLWindow *sqlwin)
{
    window_list = g_list_remove(window_list, sql);
    g_free(sql);
    return FALSE;
}

void
gmdb_sql_tree_select_cb(GtkCTree *tree, GList *node, gint column, GMdbSQLWindow *sqlwin)
{
	sqlwin->current_node = node;
}
void 
gmdb_sql_dnd_dataget_cb(
    GtkWidget *w, GdkDragContext *dc,
    GtkSelectionData *selection_data, guint info, guint t,
    GMdbSQLWindow *sqlwin)
{
gchar tablename[256];
gchar *text[2];

	gtk_ctree_get_node_info(GTK_CTREE(sqlwin->ctree), sqlwin->current_node, text, NULL, NULL, NULL, NULL, NULL, NULL, NULL);

	strcpy(tablename, "Shippers");
	gtk_selection_data_set(
		selection_data,
		GDK_SELECTION_TYPE_STRING,
		8,  /* 8 bits per character. */
		text[0], strlen(text[0]));
}
void gmdb_sql_dnd_datareceived_cb(
        GtkWidget *w,
        GdkDragContext *dc,
        gint x, gint y,
        GtkSelectionData *selection_data,
        guint info, guint t,
        GMdbSQLWindow *sqlwin)
{
gchar *buf;

	buf = selection_data->data;
	if (gtk_text_get_length(GTK_TEXT(sqlwin->textbox))==0) {
		gtk_text_insert(GTK_TEXT(sqlwin->textbox), NULL, NULL, NULL, "select * from ", 14);
	}
	gtk_text_insert(GTK_TEXT(sqlwin->textbox), NULL, NULL, NULL, buf, strlen(buf));
	gtk_widget_grab_focus(GTK_WIDGET(sqlwin->textbox));
}

void
gmdb_sql_select_hist_cb(GtkList *list, GtkWidget *w, GMdbSQLWindow *sqlwin)
{
guint child_num;
gchar *buf;

	child_num = gtk_list_child_position(list, w);
	buf = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(sqlwin->combo)->entry));
	gtk_editable_delete_text(GTK_EDITABLE(sqlwin->textbox), 0, -1);
	gtk_text_insert(GTK_TEXT(sqlwin->textbox), NULL, NULL, NULL, buf, strlen(buf));
}

void
gmdb_sql_execute_cb(GtkWidget *w, GMdbSQLWindow *sqlwin)
{
guint len;
gchar *buf;
gchar *bound_data[256];
int i;
MdbSQLColumn *sqlcol;
gchar *titles[256];

	/* stuff this query on the history */
	len = gtk_text_get_length(GTK_TEXT(sqlwin->textbox));
	buf = gtk_editable_get_chars(GTK_EDITABLE(sqlwin->textbox), 0, -1);	
	sqlwin->history = g_list_prepend(sqlwin->history, g_strdup(buf));
	gtk_combo_set_popdown_strings(GTK_COMBO(sqlwin->combo), sqlwin->history);

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
    for (i=0;i<sql->num_columns;i++) {
        bound_data[i] = (char *) malloc(MDB_BIND_SIZE);
        bound_data[i][0] = '\0';
        mdbsql_bind_column(sql, i+1, bound_data[i]);
		sqlcol = g_ptr_array_index(sql->columns,i);
		titles[i] = sqlcol->name;
	}
    if (sqlwin->clist) {
		gtk_container_remove(GTK_CONTAINER(sqlwin->scroll), sqlwin->clist);
	}
    sqlwin->clist = gtk_clist_new_with_titles(sql->num_columns, titles);
	gtk_container_add(GTK_CONTAINER(sqlwin->scroll),sqlwin->clist);
	gtk_widget_show(sqlwin->clist);

    while(mdb_fetch_row(sql->cur_table)) {
		gtk_clist_append(GTK_CLIST(sqlwin->clist), bound_data);
    }


	/* free the memory used to bind */
	for (i=0;i<sql->num_columns;i++) {
		free(bound_data[i]);
	}

	mdb_sql_reset(sql);
	g_free(buf);
}

void
gmdb_sql_new_window_cb(GtkWidget *w, gpointer data)
{
GtkWidget *hpane;
GtkWidget *vpane;
GtkWidget *vbox;
GtkWidget *hbox;
GtkWidget *scroll;
GtkWidget *button;
GtkWidget *frame1;
GtkTargetEntry src;
GMdbSQLWindow *sqlwin;

	if (!mdb) {
		gmdb_info_msg("No database is open.");
		return;
	}
	sqlwin = g_malloc0(sizeof(GMdbSQLWindow));
	sqlwin->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);	
	gtk_window_set_title(GTK_WINDOW(sqlwin->window), "Query Tool");
	gtk_widget_set_usize(sqlwin->window, 400,300);
	gtk_widget_show(sqlwin->window);

	gtk_signal_connect (GTK_OBJECT (sqlwin->window), "delete_event",
		GTK_SIGNAL_FUNC (gmdb_sql_close), sqlwin);

	vpane = gtk_vpaned_new();
	gtk_widget_show(vpane);
	gtk_paned_set_position(GTK_PANED(vpane), 100);
	gtk_paned_set_gutter_size(GTK_PANED(vpane), 12);
	gtk_container_add(GTK_CONTAINER(sqlwin->window), vpane);

	hpane = gtk_hpaned_new();
	gtk_widget_show(hpane);
	gtk_paned_set_position(GTK_PANED(hpane), 100);
	gtk_paned_set_gutter_size(GTK_PANED(hpane), 12);
	gtk_container_add(GTK_CONTAINER(vpane), hpane);

	scroll = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
		GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_widget_show (scroll);
	gtk_container_add(GTK_CONTAINER(hpane), scroll);

    sqlwin->ctree = gtk_ctree_new (1, 0);
    gtk_widget_show (sqlwin->ctree);
    gtk_container_add (GTK_CONTAINER (scroll), sqlwin->ctree);
	
	gtk_signal_connect ( GTK_OBJECT (sqlwin->ctree),
		 "tree-select-row", GTK_SIGNAL_FUNC (gmdb_sql_tree_select_cb), sqlwin);

	vbox = gtk_vbox_new(FALSE,0);
    gtk_widget_show (vbox);
	gtk_container_add(GTK_CONTAINER(hpane), vbox);

    frame1 = gtk_frame_new (NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(frame1), GTK_SHADOW_IN);
    gtk_widget_show (frame1);
	gtk_box_pack_start (GTK_BOX (vbox), frame1, TRUE, TRUE, 0);

    sqlwin->textbox = gtk_text_new (NULL,NULL);
    gtk_widget_show (sqlwin->textbox);
	gtk_text_set_editable(GTK_TEXT(sqlwin->textbox), TRUE);
	gtk_container_add(GTK_CONTAINER(frame1), sqlwin->textbox);

	gtk_signal_connect ( GTK_OBJECT (sqlwin->textbox),
		 "activate", GTK_SIGNAL_FUNC (gmdb_sql_execute_cb), sqlwin);

	hbox = gtk_hbox_new(FALSE,0);
    gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	sqlwin->combo = gtk_combo_new();
    gtk_widget_show (sqlwin->combo);
	gtk_box_pack_start (GTK_BOX (hbox), sqlwin->combo, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(sqlwin->combo)->entry), FALSE);

	gtk_signal_connect ( GTK_OBJECT (GTK_COMBO(sqlwin->combo)->list),
		 "select-child", GTK_SIGNAL_FUNC (gmdb_sql_select_hist_cb), sqlwin);

	button = gtk_button_new_with_label("Execute");
    gtk_widget_show (button);
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 5);

	gtk_signal_connect ( GTK_OBJECT (button),
		 "clicked", GTK_SIGNAL_FUNC (gmdb_sql_execute_cb), sqlwin);

	sqlwin->scroll = gtk_scrolled_window_new(NULL,NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sqlwin->scroll),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_widget_show (sqlwin->scroll);
	gtk_container_add(GTK_CONTAINER(vpane), sqlwin->scroll);

	sqlwin->clist = gtk_clist_new(1);
	gtk_widget_show(sqlwin->clist);
	gtk_container_add(GTK_CONTAINER(sqlwin->scroll),sqlwin->clist);

	/* populate first level of tree */
	gmdb_sql_ctree_populate(mdb, sqlwin);
	src.target = "table";
	src.flags = 0;
	src.info = 1;
	gtk_drag_source_set( sqlwin->ctree, GDK_BUTTON1_MASK, &src, 1, GDK_ACTION_COPY);
	gtk_drag_dest_set( sqlwin->textbox, 
		GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_HIGHLIGHT |
		GTK_DEST_DEFAULT_DROP,
		&src, 1, GDK_ACTION_COPY | GDK_ACTION_MOVE);
	gtk_signal_connect( GTK_OBJECT(sqlwin->ctree), "drag_data_get",
		GTK_SIGNAL_FUNC(gmdb_sql_dnd_dataget_cb), sqlwin);
	gtk_signal_connect( GTK_OBJECT(sqlwin->textbox), "drag_data_received",
		GTK_SIGNAL_FUNC(gmdb_sql_dnd_datareceived_cb), sqlwin);
	
	gtk_widget_grab_focus(GTK_WIDGET(sqlwin->textbox));

	/* add this one to the window list */
	window_list = g_list_append(window_list, sql);
}

/* functions */
void gmdb_sql_ctree_populate(MdbHandle *mdb, GMdbSQLWindow *sqlwin)
{
int   i;
MdbCatalogEntry *entry;
gchar *text[2];

	/* add all user tables in catalog to tab */
	for (i=0; i < mdb->num_catalog; i++) {
		entry = g_ptr_array_index (mdb->catalog, i);
		if (mdb_is_user_table(entry)) {
			text[0] = entry->object_name;
			text[1] = ""; 
			gtk_ctree_insert_node(GTK_CTREE(sqlwin->ctree), NULL, NULL, text, 0, NULL, NULL, NULL, NULL, FALSE, FALSE);
		}
	} /* for */
}
#else

void
gmdb_sql_new_window_cb(GtkWidget *w, gpointer data)
{
	gmdb_info_msg("SQL support was not built in.\nRun configure with the --enable-sql option.");
}

#endif
