#include "gmdb.h"

extern GtkWidget *app;
extern MdbHandle *mdb;

typedef struct GMdbTableExportDialog {
	MdbCatalogEntry *entry;
	GtkWidget *dialog;
	GtkWidget *lineterm;
	GtkWidget *colsep;
	GtkWidget *quote;
	GtkWidget *quotechar;
	GtkWidget *headers;
	GtkWidget *filesel;
} GMdbTableExportDialog;

GMdbTableExportDialog *export;

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
gmdb_export_file_cb(GtkWidget *selector, GMdbTableExportDialog *export)
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

	
	str = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(export->colsep)->entry));
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

	str = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(export->lineterm)->entry));
	if (!strcmp(str,LF)) { strcpy(lineterm, "\n"); }
	else if (!strcmp(str,CR)) { strcpy(lineterm, "\r"); }
	else if (!strcmp(str,CRLF)) { strcpy(lineterm, "\r\n"); }

	str = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(export->quote)->entry));
	if (!strcmp(str,ALWAYS)) { need_quote = 1; }
	else if (!strcmp(str,NEVER)) { need_quote = 0; }
	else if (!strcmp(str,AUTOMAT)) { need_quote = -1; }

	str = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(export->quotechar)->entry));
	quotechar = str[0];

	/* headers */
	str = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(export->headers)->entry));
	if (str && str[0]=='Y') need_headers = 1;

	file_path = gtk_file_selection_get_filename (GTK_FILE_SELECTION(export->filesel));
	// printf("file path %s\n",file_path);
	if ((outfile=fopen(file_path, "w"))==NULL) {
		gmdb_info_msg("Unable to Open File!");
		return;
	}

	/* read table */
	table = mdb_read_table(export->entry);
	mdb_read_columns(table);
	mdb_rewind_table(table);

	for (i=0;i<table->num_cols;i++) {
		/* bind columns */
		bound_data[i] = (char *) malloc(MDB_BIND_SIZE);
		bound_data[i][0] = '\0';
		mdb_bind_column(table, i+1, bound_data[i], NULL);

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
	gtk_widget_destroy(export->dialog);
	sprintf(msg,"%d Rows exported successfully.\n", rows);
	gmdb_info_msg(msg);
}
void
gmdb_export_button_cb(GtkWidget *w, GMdbTableExportDialog *export)
{
    /* just print a string so that we know we got there */
    export->filesel = gtk_file_selection_new("Please choose a filename.");

    gtk_signal_connect (
        GTK_OBJECT (GTK_FILE_SELECTION(export->filesel)->ok_button),
        "clicked", GTK_SIGNAL_FUNC (gmdb_export_file_cb), export);

    gtk_signal_connect_object ( GTK_OBJECT (
		GTK_FILE_SELECTION(export->filesel)->ok_button),
		"clicked", GTK_SIGNAL_FUNC (gtk_widget_destroy),
		(gpointer) export->filesel);

    gtk_signal_connect_object (
        GTK_OBJECT (GTK_FILE_SELECTION(export->filesel)->cancel_button),
            "clicked", GTK_SIGNAL_FUNC (gtk_widget_destroy),
            (gpointer) export->filesel);

    gtk_widget_show (export->filesel);
}

GtkWidget *
gmdb_export_add_combo(GtkWidget *table, int row, gchar *text, GList *strings)
{
GtkWidget *combo;
GtkWidget *label;

    label = gtk_label_new(text);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_RIGHT);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row + 1,
        GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 5, 0);
    gtk_widget_show(label);

    combo = gtk_combo_new();
    gtk_combo_set_popdown_strings(GTK_COMBO(combo), strings);
    gtk_table_attach(GTK_TABLE(table), combo, 1, 2, row, row + 1,
        GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 5, 0);
    gtk_widget_show(combo);
	return combo;
}

void gmdb_table_export(MdbCatalogEntry *entry) {
GtkWidget *export_button;
GtkWidget *close_button;
GList *glist = NULL;
GtkWidget *table;
   
	export = g_malloc(sizeof(GMdbTableExportDialog));
	export->entry = entry;

	/* Create the widgets */
	export->dialog = gtk_dialog_new();
	gtk_widget_set_uposition(export->dialog, 300, 300);

    table = gtk_table_new(3,2,FALSE);
    gtk_widget_show(table);

	glist = g_list_append(glist, LF);
	glist = g_list_append(glist, CR);
	glist = g_list_append(glist, CRLF);
	export->lineterm = gmdb_export_add_combo(table, 0, "Line Terminator:",glist);
	g_list_free(glist);
	gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(export->lineterm)->entry), FALSE);

	glist = NULL;
	glist = g_list_append(glist, COMMA);
	glist = g_list_append(glist, TAB);
	glist = g_list_append(glist, SPACE);
	glist = g_list_append(glist, COLON);
	glist = g_list_append(glist, SEMICOLON);
	glist = g_list_append(glist, PIPE);
	export->colsep = gmdb_export_add_combo(table, 1, "Column Separator:",glist);
	g_list_free(glist);

	glist = NULL;
	glist = g_list_append(glist, ALWAYS);
	glist = g_list_append(glist, NEVER);
	glist = g_list_append(glist, AUTOMAT);
	export->quote = gmdb_export_add_combo(table, 2, "Quotes:",glist);
	g_list_free(glist);
	gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(export->quote)->entry), FALSE);

	glist = NULL;
	glist = g_list_append(glist, "\"");
	glist = g_list_append(glist, "'");
	glist = g_list_append(glist, "`");
	export->quotechar = gmdb_export_add_combo(table, 3, "Quote Character:",glist);
	g_list_free(glist);

	glist = NULL;
	glist = g_list_append(glist, "Yes");
	glist = g_list_append(glist, "No");
	export->headers = gmdb_export_add_combo(table, 4, "Include Headers:",glist);
	g_list_free(glist);
	gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(export->headers)->entry), FALSE);

	export_button = gtk_button_new_with_label("Export");
	gtk_signal_connect ( GTK_OBJECT (export_button),
		"clicked", GTK_SIGNAL_FUNC (gmdb_export_button_cb), export);

	gtk_container_add (GTK_CONTAINER (GTK_DIALOG(export->dialog)->action_area),
		export_button);

	close_button = gtk_button_new_with_label("Cancel");
	gtk_signal_connect_object (GTK_OBJECT (close_button), "clicked",
		GTK_SIGNAL_FUNC (gtk_widget_destroy), GTK_OBJECT(export->dialog));
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG(export->dialog)->action_area),
		close_button);

	/* Add the table, and show everything we've added to the dialog. */

	gtk_container_add (GTK_CONTAINER (GTK_DIALOG(export->dialog)->vbox), table);
	gtk_widget_show_all (export->dialog);
}
