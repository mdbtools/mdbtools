/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#ifndef __GTK_HLIST_H__
#define __GTK_HLIST_H__


#include <gdk/gdk.h>
#include <gtk/gtkenums.h>
#include <gtk/gtkcontainer.h>
#include <gtk/gtklistitem.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_TYPE_HLIST                  (gtk_hlist_get_type ())
#define GTK_HLIST(obj)                  (GTK_CHECK_CAST ((obj), GTK_TYPE_HLIST, GtkHList))
#define GTK_HLIST_CLASS(klass)          (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_HLIST, GtkHListClass))
#define GTK_IS_HLIST(obj)               (GTK_CHECK_TYPE ((obj), GTK_TYPE_HLIST))
#define GTK_IS_HLIST_CLASS(klass)       (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_HLIST))


typedef struct _GtkHList	      GtkHList;
typedef struct _GtkHListClass  GtkHListClass;

struct _GtkHList
{
  GtkContainer container;

  GList *children;
  GList *selection;

  GList *undo_selection;
  GList *undo_unselection;

  GtkWidget *last_focus_child;
  GtkWidget *undo_focus_child;

  guint htimer;
  guint vtimer;

  gint anchor;
  gint drag_pos;
  GtkStateType anchor_state;

  guint selection_mode : 2;
  guint drag_selection:1;
  guint add_mode:1;

  gboolean horizontal;
  guint horiz_spacing;
};

struct _GtkHListClass
{
  GtkContainerClass parent_class;

  void (* selection_changed) (GtkHList	*list);
  void (* select_child)	     (GtkHList	*list,
			      GtkWidget *child);
  void (* unselect_child)    (GtkHList	*list,
			      GtkWidget *child);
};


GtkType	   gtk_hlist_get_type		  (void);
GtkWidget* gtk_hlist_new			  (void);
void	   gtk_hlist_insert_items	  (GtkHList	    *list,
					   GList	    *items,
					   gint		     position);
void	   gtk_hlist_append_items	  (GtkHList	    *list,
					   GList	    *items);
void	   gtk_hlist_prepend_items	  (GtkHList	    *list,
					   GList	    *items);
void	   gtk_hlist_remove_items	  (GtkHList	    *list,
					   GList	    *items);
void	   gtk_hlist_remove_items_no_unref (GtkHList	    *list,
					   GList	    *items);
void	   gtk_hlist_clear_items		  (GtkHList	    *list,
					   gint		     start,
					   gint		     end);
void	   gtk_hlist_select_item		  (GtkHList	    *list,
					   gint		     item);
void	   gtk_hlist_unselect_item	  (GtkHList	    *list,
					   gint		     item);
void	   gtk_hlist_select_child	  (GtkHList	    *list,
					   GtkWidget	    *child);
void	   gtk_hlist_unselect_child	  (GtkHList	    *list,
					   GtkWidget	    *child);
gint	   gtk_hlist_child_position	  (GtkHList	    *list,
					   GtkWidget	    *child);
void	   gtk_hlist_set_selection_mode	  (GtkHList	    *list,
					   GtkSelectionMode  mode);
void	   gtk_hlist_set_horizontal_mode  (GtkHList	    *list,
					   gboolean  horiz);

void       gtk_hlist_extend_selection      (GtkHList          *list,
					   GtkScrollType     scroll_type,
					   gfloat            position,
					   gboolean          auto_start_selection);
void       gtk_hlist_start_selection       (GtkHList          *list);
void       gtk_hlist_end_selection         (GtkHList          *list);
void       gtk_hlist_select_all            (GtkHList          *list);
void       gtk_hlist_unselect_all          (GtkHList          *list);
void       gtk_hlist_scroll_horizontal     (GtkHList          *list,
					   GtkScrollType     scroll_type,
					   gfloat            position);
void       gtk_hlist_scroll_vertical       (GtkHList          *list,
					   GtkScrollType     scroll_type,
					   gfloat            position);
void       gtk_hlist_toggle_add_mode       (GtkHList          *list);
void       gtk_hlist_toggle_focus_row      (GtkHList          *list);
void       gtk_hlist_toggle_row            (GtkHList          *list,
					   GtkWidget        *item);
void       gtk_hlist_undo_selection        (GtkHList          *list);
void       gtk_hlist_end_drag_selection    (GtkHList          *list);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_HLIST_H__ */
