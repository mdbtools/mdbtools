#include "gmdb.h"

extern GtkWidget *app;
extern MdbHandle *mdb;

typedef struct GMdbInfoWindow {
	GtkWidget *window;
} GMdbInfoWindow;

GMdbInfoWindow *infow;

/* callbacks */
GtkWidget *
gmdb_info_new()
{
GtkWidget *propswin, *label;
GladeXML *propswin_xml;
gchar title[100];
gchar tmpstr[20];
gchar *filename, *filepath;
int i;
struct stat st;

	/* load the interface */
	propswin_xml = glade_xml_new("gladefiles/gmdb-props.glade", NULL, NULL);
	/* connect the signals in the interface */
	glade_xml_signal_autoconnect(propswin_xml);
	
	filepath = g_strdup(mdb->filename);
	for (i=strlen(filepath);i>0 && filepath[i-1]!='/';i--);
	filename=&filepath[i];
	
	propswin = glade_xml_get_widget (propswin_xml, "props_dialog");
	g_snprintf(title, 100, "%s Properties",filename);
	gtk_window_set_title(GTK_WINDOW(propswin), title);
		
	label = glade_xml_get_widget (propswin_xml, "props_filename");
	gtk_label_set_text(GTK_LABEL(label), filename);	
		
	label = glade_xml_get_widget (propswin_xml, "props_jetver");
	gtk_label_set_text(GTK_LABEL(label), mdb->jet_version == MDB_VER_JET3 ? "3 (Access 97)" : "4 (Access 2000/XP)");	
		
	assert( fstat(mdb->fd, &st)!=-1 );
	sprintf(tmpstr, "%ld K", st.st_size/1024);
	label = glade_xml_get_widget (propswin_xml, "props_filesize");
	gtk_label_set_text(GTK_LABEL(label), tmpstr);	
		
	sprintf(tmpstr, "%ld", st.st_size / mdb->pg_size);
	label = glade_xml_get_widget (propswin_xml, "props_numpages");
	gtk_label_set_text(GTK_LABEL(label), tmpstr);	

	sprintf(tmpstr, "%d", mdb->num_catalog);
	label = glade_xml_get_widget (propswin_xml, "props_numobjs");
	gtk_label_set_text(GTK_LABEL(label), tmpstr);	

	g_free(filepath);
}
