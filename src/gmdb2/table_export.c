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
print_quote(FILE *outfile, int need_quote, char quotechar, char *colsep, char *str)
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
char msg[100];
GtkWidget *combo, *fentry, *checkbox;
GtkWidget *exportwin;

	
	combo = glade_xml_get_widget (exportwin_xml, "sep_combo");
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

	combo = glade_xml_get_widget (exportwin_xml, "term_combo");
	str = (gchar *) gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(combo)->entry));
	if (!strcmp(str,LF)) { strcpy(lineterm, "\n"); }
	else if (!strcmp(str,CR)) { strcpy(lineterm, "\r"); }
	else if (!strcmp(str,CRLF)) { strcpy(lineterm, "\r\n"); }

	combo = glade_xml_get_widget (exportwin_xml, "quote_combo");
	str = (gchar *) gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(combo)->entry));
	if (!strcmp(str,ALWAYS)) { need_quote = 1; }
	else if (!strcmp(str,NEVER)) { need_quote = 0; }
	else if (!strcmp(str,AUTOMAT)) { need_quote = -1; }

	combo = glade_xml_get_widget (exportwin_xml, "qchar_combo");
	str = (gchar *) gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(combo)->entry));
	quotechar = str[0];

	/* headers */
	checkbox = glade_xml_get_widget (exportwin_xml, "header_checkbox");
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkbox)))
		need_headers = 1;
	else
		need_headers = 0;

	fentry = glade_xml_get_widget (exportwin_xml, "filename_entry");
	file_path = (gchar *) gtk_entry_get_text(GTK_ENTRY(fentry));

	// printf("file path %s\n",file_path);
	if ((outfile=fopen(file_path, "w"))==NULL) {
		gmdb_info_msg("Unable to Open File!");
		return;
	}

	/* read table */
	table = mdb_read_table(cat_entry);
	mdb_read_columns(table);
	mdb_rewind_table(table);

	for (i=0;i<table->num_cols;i++) {
		/* bind columns */
		bound_data[i] = (char *) malloc(MDB_BIND_SIZE);
		bound_data[i][0] = '\0';
		mdb_bind_column(table, i+1, bound_data[i]);

		/* display column titles */
		col=g_ptr_array_index(table->columns,i);
		if (need_headers)  {
			if (i>0) fprintf(outfile,delimiter);
			print_quote(outfile, need_quote, quotechar, delimiter, col->name);
			fprintf(outfile,"%s", col->name);
			print_quote(outfile, need_quote, quotechar, delimiter, col->name);
		}
	}
	if (need_headers) fprintf(outfile,lineterm);

	/* fetch those rows! */
	while(mdb_fetch_row(table)) {
		for (i=0;i<table->num_cols;i++) {
			if (i>0) fprintf(outfile,delimiter);
			print_quote(outfile, need_quote, quotechar, delimiter, bound_data[i]);
			fprintf(outfile,"%s", bound_data[i]);
			print_quote(outfile, need_quote, quotechar, delimiter, bound_data[i]);
		}
		fprintf(outfile,lineterm);
		rows++;
	}

	/* free the memory used to bind */
	for (i=0;i<table->num_cols;i++) {
		free(bound_data[i]);
	}

	fclose(outfile);
	exportwin = glade_xml_get_widget (exportwin_xml, "export_dialog");
	gtk_widget_destroy(exportwin);
	sprintf(msg,"%d Rows exported successfully.\n", rows);
	gmdb_info_msg(msg);
}
void gmdb_table_export(MdbCatalogEntry *entry) {
GtkWidget *export_button;
GtkWidget *close_button;
GList *glist = NULL;
GtkWidget *combo;
   
	cat_entry = entry;

	/* load the interface */
	exportwin_xml = glade_xml_new("gladefiles/gmdb-export.glade", NULL, NULL);
	/* connect the signals in the interface */
	glade_xml_signal_autoconnect(exportwin_xml);
	

	/* Create the widgets */
	combo = glade_xml_get_widget (exportwin_xml, "term_combo");
	glist = g_list_append(glist, LF);
	glist = g_list_append(glist, CR);
	glist = g_list_append(glist, CRLF);
    	gtk_combo_set_popdown_strings(GTK_COMBO(combo), glist);
	g_list_free(glist);

	combo = glade_xml_get_widget (exportwin_xml, "sep_combo");
	glist = NULL;
	glist = g_list_append(glist, COMMA);
	glist = g_list_append(glist, TAB);
	glist = g_list_append(glist, SPACE);
	glist = g_list_append(glist, COLON);
	glist = g_list_append(glist, SEMICOLON);
	glist = g_list_append(glist, PIPE);
    	gtk_combo_set_popdown_strings(GTK_COMBO(combo), glist);
	g_list_free(glist);

	combo = glade_xml_get_widget (exportwin_xml, "quote_combo");
	glist = NULL;
	glist = g_list_append(glist, ALWAYS);
	glist = g_list_append(glist, NEVER);
	glist = g_list_append(glist, AUTOMAT);
    	gtk_combo_set_popdown_strings(GTK_COMBO(combo), glist);
	g_list_free(glist);

	combo = glade_xml_get_widget (exportwin_xml, "qchar_combo");
	glist = NULL;
	glist = g_list_append(glist, "\"");
	glist = g_list_append(glist, "'");
	glist = g_list_append(glist, "`");
    	gtk_combo_set_popdown_strings(GTK_COMBO(combo), glist);
	g_list_free(glist);
}
