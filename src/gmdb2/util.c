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

void gmdb_info_msg(gchar *message) {
GtkWidget *dialog, *label, *okay_button;
   
	/* Create the widgets */
	dialog = gtk_dialog_new();
	gtk_widget_set_uposition(dialog, 300, 300);
	label = gtk_label_new (message);
	gtk_widget_set_usize(label, 250, 100);
	okay_button = gtk_button_new_with_label("Okay");
   
	/* Ensure that the dialog box is destroyed when the user clicks ok. */
   
	gtk_signal_connect_object (GTK_OBJECT (okay_button), "clicked",
		GTK_SIGNAL_FUNC (gtk_widget_destroy), dialog);
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG(dialog)->action_area),
		okay_button);

	/* Add the label, and show everything we've added to the dialog. */

	gtk_container_add (GTK_CONTAINER (GTK_DIALOG(dialog)->vbox), label);
	gtk_widget_show_all (dialog);
}
