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

#include <gtk/gtkmessagedialog.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-help.h>

extern GtkWidget *app;
extern MdbHandle *mdb;
GladeXML *exportwin_xml;
MdbCatalogEntry *cat_entry;

#define COMMA	"Comma (,)"
#define TAB		"Tab"
#define SPACE	"Space"
#define COLON	"Colon (:)"
#define SEMICOLON	"Semicolon (;)"
#define PIPE	"Pipe (|)"

#define LF "Unix (linefeed only)"
#define CR "Mac (carriage return only)"
#define CRLF "Windows (CR + LF)"

#define ALWAYS "Always"
#define NEVER "Never"
#define AUTOMAT "Automatic (where necessary)"

void
gmdb_print_quote(FILE *outfile, int need_quote, char quotechar, char *colsep, char *str)
{
	if (need_quote==1) {
		fprintf(outfile, "%c", quotechar);
	} else if (need_quote==-1) {
		if (strstr(str,colsep)) {
			fprintf(outfile, "%c", quotechar);
		}
	}
}

void
gmdb_export_get_delimiter(GladeXML *xml, gchar *delimiter, int max_buf)
{
	GtkWidget *combo;
	gchar *str;

	combo = glade_xml_get_widget(xml, "sep_combo");
	str = (gchar *) gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(combo)->entry));
	if (!strcmp(str,COMMA)) { strcpy(delimiter, ","); }
	else if (!strcmp(str,TAB)) { strcpy(delimiter, "\t"); }
	else if (!strcmp(str,SPACE)) { strcpy(delimiter, " "); }
	else if (!strcmp(str,COLON)) { strcpy(delimiter, ":"); }
	else if (!strcmp(str,SEMICOLON)) { strcpy(delimiter, ";"); }
	else if (!strcmp(str,PIPE)) { strcpy(delimiter, "|"); }
	else {
		strncpy(delimiter,str, 10);
		delimiter[10]='\0';
	}
}

void
gmdb_export_get_lineterm(GladeXML *xml, gchar *lineterm, int max_buf)
{
	GtkWidget *combo;
	gchar *str;

	combo = glade_xml_get_widget(xml, "term_combo");
	str = (gchar *) gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(combo)->entry));
	if (!strcmp(str,LF)) { strcpy(lineterm, "\n"); }
	else if (!strcmp(str,CR)) { strcpy(lineterm, "\r"); }
	else if (!strcmp(str,CRLF)) { strcpy(lineterm, "\r\n"); }
}

int
gmdb_export_get_quote(GladeXML *xml)
{
	GtkWidget *combo;
	int need_quote = 0;
	gchar *str;

	combo = glade_xml_get_widget(xml, "quote_combo");
	str = (gchar *) gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(combo)->entry));
	if (!strcmp(str,ALWAYS)) { need_quote = 1; }
	else if (!strcmp(str,NEVER)) { need_quote = 0; }
	else if (!strcmp(str,AUTOMAT)) { need_quote = -1; }

	return need_quote;
}

char
gmdb_export_get_quotechar(GladeXML *xml)
{
	GtkWidget *combo;
	gchar *str;
	char quotechar;

	combo = glade_xml_get_widget(xml, "qchar_combo");
	str = (gchar *) gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(combo)->entry));
	quotechar = str[0];

	return quotechar;
}
int
gmdb_export_get_headers(GladeXML *xml)
{
	GtkWidget *checkbox;

	checkbox = glade_xml_get_widget(xml, "headers_checkbox");
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkbox)))
		return 1;
	else
		return 0;
}
gchar *
gmdb_export_get_filepath(GladeXML *xml)
{
	GtkWidget *fentry;

	fentry = glade_xml_get_widget(xml, "filename_entry");
	return (gchar *) gtk_entry_get_text(GTK_ENTRY(fentry));
}

void
gmdb_export_help_cb(GtkWidget *w, gpointer data)
{
	GError *error = NULL;

	gnome_help_display("gmdb.xml", "gmdb-table-export", &error);
	if (error != NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}
}
void
gmdb_table_export_button_cb(GtkWidget *w, gpointer data)
{
gchar *file_path;
FILE *outfile;
gchar *bound_data[256];
MdbTableDef *table;
MdbColumn *col;
int i;
int need_headers = 0;
int need_quote = 0;
gchar delimiter[11];
gchar quotechar;
gchar lineterm[5];
gchar *str;
int rows=0;

	GtkWidget *exportwin, *dlg;

	gmdb_export_get_delimiter(exportwin_xml, delimiter, 10);
	gmdb_export_get_lineterm(exportwin_xml, lineterm, 5);
	need_quote = gmdb_export_get_quote(exportwin_xml);
	quotechar = gmdb_export_get_quotechar(exportwin_xml);
	need_headers = gmdb_export_get_headers(exportwin_xml);
	file_path = gmdb_export_get_filepath(exportwin_xml);

	// printf("file path %s\n",file_path);
	if ((outfile=fopen(file_path, "w"))==NULL) {
		GtkWidget* dlg = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (w)),
		    GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_WARNING, GTK_BUTTONS_CLOSE,
		    _("Unable to open file."));
		gtk_dialog_run (GTK_DIALOG (dlg));
		gtk_widget_destroy (dlg);
		return;
	}

	/* read table */
	table = mdb_read_table(cat_entry);
	mdb_read_columns(table);
	mdb_rewind_table(table);

	for (i=0;i<table->num_cols;i++) {
		/* bind columns */
		bound_data[i] = (char *) g_malloc0(MDB_BIND_SIZE);
		mdb_bind_column(table, i+1, bound_data[i], NULL);

		/* display column titles */
		col=g_ptr_array_index(table->columns,i);
		if (need_headers)  {
			if (i>0) fputs(delimiter, outfile);
			gmdb_print_quote(outfile, need_quote, quotechar, delimiter, col->name);
			fputs(col->name, outfile);
			gmdb_print_quote(outfile, need_quote, quotechar, delimiter, col->name);
		}
	}
	if (need_headers) fputs(lineterm, outfile);

	/* fetch those rows! */
	while(mdb_fetch_row(table)) {
		for (i=0;i<table->num_cols;i++) {
			if (i>0) fputs(delimiter, outfile);
			gmdb_print_quote(outfile, need_quote, quotechar, delimiter, bound_data[i]);
			fputs(bound_data[i], outfile);
			gmdb_print_quote(outfile, need_quote, quotechar, delimiter, bound_data[i]);
		}
		fputs(lineterm, outfile);
		rows++;
	}

	/* free the memory used to bind */
	for (i=0;i<table->num_cols;i++) {
		g_free(bound_data[i]);
	}

	fclose(outfile);
	exportwin = glade_xml_get_widget (exportwin_xml, "export_dialog");
	gtk_widget_destroy(exportwin);
	dlg = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (w)),
	    GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE,
	    _("%d rows successfully exported."), rows);
	gtk_dialog_run (GTK_DIALOG (dlg));
	gtk_widget_destroy (dlg);
}
void gmdb_table_export(MdbCatalogEntry *entry) 
{
GtkWidget *export_button;
GtkWidget *close_button;
   
	cat_entry = entry;

	/* load the interface */
	exportwin_xml = glade_xml_new(GMDB_GLADEDIR "gmdb-export.glade", NULL, NULL);
	/* connect the signals in the interface */
	glade_xml_signal_autoconnect(exportwin_xml);
	gmdb_table_export_populate_dialog(exportwin_xml);
}
void
gmdb_table_export_populate_dialog(GladeXML *xml)
{
	GList *glist = NULL;
	GtkWidget *combo;

	/* Create the widgets */
	combo = glade_xml_get_widget (xml, "term_combo");
	glist = g_list_append(glist, LF);
	glist = g_list_append(glist, CR);
	glist = g_list_append(glist, CRLF);
    	gtk_combo_set_popdown_strings(GTK_COMBO(combo), glist);
	g_list_free(glist);

	combo = glade_xml_get_widget (xml, "sep_combo");
	glist = NULL;
	glist = g_list_append(glist, COMMA);
	glist = g_list_append(glist, TAB);
	glist = g_list_append(glist, SPACE);
	glist = g_list_append(glist, COLON);
	glist = g_list_append(glist, SEMICOLON);
	glist = g_list_append(glist, PIPE);
    	gtk_combo_set_popdown_strings(GTK_COMBO(combo), glist);
	g_list_free(glist);

	combo = glade_xml_get_widget (xml, "quote_combo");
	glist = NULL;
	glist = g_list_append(glist, ALWAYS);
	glist = g_list_append(glist, NEVER);
	glist = g_list_append(glist, AUTOMAT);
    	gtk_combo_set_popdown_strings(GTK_COMBO(combo), glist);
	g_list_free(glist);

	combo = glade_xml_get_widget (xml, "qchar_combo");
	glist = NULL;
	glist = g_list_append(glist, "\"");
	glist = g_list_append(glist, "'");
	glist = g_list_append(glist, "`");
    	gtk_combo_set_popdown_strings(GTK_COMBO(combo), glist);
	g_list_free(glist);
}
