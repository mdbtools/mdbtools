#include "gtkhlist.h"
#include <mdbtools.h>
#include <mdbsql.h>
#include "gmdb.h"
#include "code.xpm"
#include "forms.xpm"
#include "macros.xpm"
#include "query.xpm"
#include "reports.xpm"


GtkWidget *app;
MdbSQL *sql;

GtkWidget *main_notebook;
int main_show_debug;


/* called when the user closes the window */
static gint
delete_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
        /* signal the main loop to quit */
        gtk_main_quit();
        /* return FALSE to continue closing the window */
        return FALSE;
}

static void
gmdb_info_cb(GtkWidget *button, gpointer data)
{
	gmdb_info_new();
}

/* show the debug tab */
static void
show_debug_cb(GtkWidget *button, gpointer data)
{
	if (main_show_debug) {
		main_show_debug = 0;
		gtk_notebook_remove_page(GTK_NOTEBOOK(main_notebook), 6);
	} else {
		main_show_debug = 1;
		gmdb_debug_tab_new(main_notebook);
		gtk_notebook_set_page(GTK_NOTEBOOK(main_notebook), -1);
	}
}
/* a callback for the buttons */
static void
gmdb_about_cb(GtkWidget *button, gpointer data)
{
const gchar *authors[] = {
	"Brian Bruns",
	NULL
};
#ifdef HAVE_GNOME
gtk_widget_show (gnome_about_new ("Gtk MDB Viewer", "0.1",
                 "Copyright 2002 Brian Bruns",
                  (const gchar **) authors,
                 _("The Gtk-MDB Viewer is the grapical interface to "
                   "MDB Tools.  It lets you view, print and export data "
		   "from MDB files produced by MS Access 97/2000/XP."),
                   NULL));
#else
	gmdb_info_msg("Gtk MDB Viewer 0.1\nCopyright 2002 Brian Bruns\n"
					"The Gtk-MDB Viewer is the grapical interface to\n"
					"MDB Tools.  It lets you view, print and export data\n"
					"from MDB files produced by MS Access 97/2000/XP.\n");
#endif
}

/* a callback for the buttons */
static void
a_callback(GtkWidget *button, gpointer data)
{

        /*just print a string so that we know we got there*/
        g_print("Inside Callback\n");
}

#ifdef HAVE_GNOME

GnomeUIInfo file_menu[] = {
        GNOMEUIINFO_MENU_OPEN_ITEM(gmdb_file_select_cb, "Open Database..."),
        GNOMEUIINFO_MENU_CLOSE_ITEM(gmdb_file_close_cb, "Close Database"),
        GNOMEUIINFO_SEPARATOR,
        GNOMEUIINFO_MENU_PRINT_SETUP_ITEM(a_callback, NULL),
	GNOMEUIINFO_MENU_PRINT_ITEM("Print...",NULL),
        GNOMEUIINFO_SEPARATOR,
        GNOMEUIINFO_MENU_EXIT_ITEM(gtk_main_quit,NULL),
        GNOMEUIINFO_END
};

GnomeUIInfo edit_menu[] = {
        GNOMEUIINFO_MENU_UNDO_ITEM("Undo", a_callback),
        GNOMEUIINFO_SEPARATOR,
        GNOMEUIINFO_MENU_CUT_ITEM("Cut",a_callback),
        GNOMEUIINFO_MENU_COPY_ITEM("Copy", a_callback),
        GNOMEUIINFO_MENU_PASTE_ITEM("Paste", a_callback),
        GNOMEUIINFO_END
};
GnomeUIInfo view_menu[] = {
        GNOMEUIINFO_ITEM_NONE("File _Info","Display Information about MDB File",
                              gmdb_info_cb),
        GNOMEUIINFO_ITEM_NONE("_Debug Tab","Show file debugger tab",
                              show_debug_cb),
        GNOMEUIINFO_END
};
GnomeUIInfo tools_menu[] = {
        GNOMEUIINFO_ITEM_NONE("S_QL Window...","Run SQL Query Tool", gmdb_sql_new_window_cb),
        GNOMEUIINFO_ITEM_NONE("E_xport Schema...","Export the database schema DDL to a file",
                              a_callback),
        GNOMEUIINFO_END
};

GnomeUIInfo help_menu[] = {
	GNOMEUIINFO_MENU_ABOUT_ITEM(gmdb_about_cb, NULL),
        GNOMEUIINFO_END
};

GnomeUIInfo menubar[] = {
        GNOMEUIINFO_MENU_FILE_TREE(file_menu),
        GNOMEUIINFO_SUBTREE("_Edit",edit_menu),
        GNOMEUIINFO_SUBTREE("_View",view_menu),
        GNOMEUIINFO_SUBTREE("_Tools",tools_menu),
        GNOMEUIINFO_MENU_HELP_TREE(help_menu),
        GNOMEUIINFO_END
};
#else
static GtkItemFactoryEntry menu_items[] = {
	{ "/_File", NULL, NULL, 0, "<Branch>" },
	{ "/_File/_Open",  "<control>O", gmdb_file_select_cb, 0, NULL },
	{ "/_File/_Close", "<control>C", gmdb_file_close_cb, 0, NULL },
	{ "/_File/sep1", NULL, NULL, 0, "<Separator>" },
	{ "/_File/Print Setup", NULL, NULL, 0, NULL },
	{ "/_File/_Print", NULL, NULL, 0, NULL },
	{ "/_File/E_xit", "<control>Q", gtk_main_quit, 0, NULL },
	{ "/_Edit", NULL, NULL, 0, "<Branch>" },
	{ "/_Edit/_Undo", NULL, NULL, 0, NULL },
	{ "/_File/sep2", NULL, NULL, 0, "<Separator>" },
	{ "/_Edit/Cu_t", NULL, NULL, 0, NULL },
	{ "/_Edit/_Copy", NULL, NULL, 0, NULL },
	{ "/_Edit/_Paste", NULL, NULL, 0, NULL },
	{ "/_View", NULL, NULL, 0, "<Branch>" },
	{ "/_View/File _Info", NULL, gmdb_info_cb, 0, NULL },
	{ "/_View/_Debug Tab", NULL, show_debug_cb, 0, NULL },
	{ "/_Tools", NULL, NULL, 0, "<Branch>" },
	{ "/_Tools/S_QL Window...", NULL, gmdb_sql_new_window_cb, 0, NULL },
	{ "/_Tools/E_xport Schema...", NULL, a_callback, 0, NULL },
	{ "/_Help", NULL, NULL, 0, "<Branch>" },
	{ "/_Help/About...", NULL, gmdb_about_cb, 0, NULL },
};
#endif

GtkWidget *
add_tab(GtkWidget *notebook, gchar **xpm, gchar *text)
{
GtkWidget *tabbox;
GtkWidget *label;
GtkWidget *pixmapwid;
GtkWidget *frame;
GtkWidget *hlist;
GdkPixmap *pixmap;
GdkBitmap *mask;

	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
	gtk_container_set_border_width (GTK_CONTAINER (frame), 10);
	gtk_widget_set_usize (frame, 100, 75);
	gtk_widget_show (frame);

	hlist = gtk_hlist_new ();
	gtk_widget_show (hlist);
	gtk_container_add(GTK_CONTAINER(frame), hlist);


	/* create a picture/label box and use for tab */
        tabbox = gtk_hbox_new (FALSE,5);
	pixmap = gdk_pixmap_colormap_create_from_xpm_d( NULL,  
		gtk_widget_get_colormap(app), &mask, NULL, xpm);
	

	/* a pixmap widget to contain the pixmap */
	pixmapwid = gtk_pixmap_new( pixmap, mask );
	gtk_widget_show( pixmapwid );
	gtk_box_pack_start (GTK_BOX (tabbox), pixmapwid, TRUE, TRUE, 0);

	label = gtk_label_new (text);
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (tabbox), label, TRUE, TRUE, 0);

	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), frame, tabbox);

	return hlist;
}

void
add_icon(GtkWidget *list, gchar **xpm, gchar *text)
{
GList *glist = NULL;
GtkWidget *li;
GtkWidget *label;
GtkWidget *box;
GtkWidget *pixmapwid;
GdkPixmap *pixmap;
GdkBitmap *mask;
//GtkStyle *style;

	li = gtk_list_item_new ();
        box = gtk_hbox_new (FALSE,5);
	//style = gtk_widget_get_style( app );
	pixmap = gdk_pixmap_colormap_create_from_xpm_d( NULL,  
		gtk_widget_get_colormap(app), &mask, NULL, xpm);
	//pixmap = gdk_pixmap_create_from_xpm_d( app->window,  &mask,
                                           //&style->bg[GTK_STATE_NORMAL],
                                           //(gchar **)xpm );

	/* a pixmap widget to contain the pixmap */
	pixmapwid = gtk_pixmap_new( pixmap, mask );
	gtk_widget_show( pixmapwid );
	gtk_box_pack_start (GTK_BOX (box), pixmapwid, TRUE, TRUE, 0);

	label = gtk_label_new (text);
	gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);

	gtk_container_add(GTK_CONTAINER(li), box);
	glist = g_list_append (glist, li);
	gtk_widget_show (label);
	gtk_widget_show (li);
	gtk_hlist_append_items(GTK_HLIST(list), glist);
}
int
main(int argc, char *argv[])
{
GtkWidget *hbox;
#ifndef HAVE_GNOME
GtkWidget *vbox;
GtkWidget *menubar;
GtkItemFactory *item_factory;
GtkAccelGroup *accel_group;
gint nmenu_items = sizeof (menu_items) / sizeof (menu_items[0]);

#endif

#ifdef SQL
    /* initialize the SQL engine */
    sql = mdb_sql_init();
#endif
	/* initialize MDB Tools library */
	mdb_init();

#ifdef HAVE_GNOME
	/* Initialize GNOME */
	gnome_init ("gtk-mdb-viewer", "0.1", argc, argv);
	/* Create a Gnome app widget, which sets up a basic window
	   for your application */
	app = gnome_app_new ("gtk-mdb-viewer", "gtk-MDB File Viewer");
#else
	gtk_init(&argc, &argv);
	app = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	vbox = gtk_vbox_new (FALSE,1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 1);
	gtk_container_add (GTK_CONTAINER (app), vbox);
	gtk_widget_show (vbox);
#endif

	gtk_widget_set_uposition(GTK_WIDGET(app), 50, 50); 

	/* bind "delete_event", which is the event we get when
	   the user closes the window with the window manager,
	   to gtk_main_quit, which is a function that causes
	   the gtk_main loop to exit, and consequently to quit
	   the application */
	gtk_signal_connect (GTK_OBJECT (app), "delete_event",
		GTK_SIGNAL_FUNC (delete_event), NULL);

#ifdef HAVE_GNOME
	gnome_app_create_menus (GNOME_APP (app), menubar);
	/* create a horizontal box for the buttons and add it
	** into the app widget */
	hbox = gtk_hbox_new (FALSE,5);
	gnome_app_set_contents (GNOME_APP (app), hbox);
#else
	accel_group = gtk_accel_group_new ();
	item_factory = gtk_item_factory_new(GTK_TYPE_MENU_BAR, "<main>", accel_group);
	gtk_item_factory_create_items (item_factory, nmenu_items, menu_items, NULL);
  	gtk_window_add_accel_group (GTK_WINDOW (app), accel_group);
    /* Finally, return the actual menu bar created by the item factory. */ 
    menubar = gtk_item_factory_get_widget (item_factory, "<main>");
	gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, TRUE, 0);
	hbox = gtk_hbox_new (FALSE,5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
#endif

	/* create the tabbed interface */
	main_notebook =  gtk_notebook_new();
	gtk_box_pack_start (GTK_BOX (hbox), main_notebook, TRUE, TRUE, 0);

	gmdb_table_add_tab(main_notebook);
	query_hlist = add_tab(main_notebook, query_xpm, "Queries");
	form_hlist = add_tab(main_notebook, forms_xpm, "Forms");
	report_hlist = add_tab(main_notebook, reports_xpm, "Reports");
	macro_hlist = add_tab(main_notebook, macros_xpm, "Macros");
	module_hlist = add_tab(main_notebook, code_xpm, "Modules");
	
	//add_icon(table_hlist, "table.xpm", "Table1");

        /* show everything inside this app widget and the app
           widget itself */
        gtk_widget_show_all(app);
        
	if (argc>1) {
		gmdb_file_open(argv[1]);
	}

	/* enter the main loop */
	gtk_main ();

#ifdef SQL
	/* free MDB Tools library */
	mdb_sql_exit(sql);
#endif
	mdb_exit();
        
	return 0;
}
