/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 * Hacked (C) 1996 Tristan Tarrant
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include "progress.h"

static const int progress_x = 5;
static const int progress_y = 5;

void
progress_cancel (GtkWidget * widget, gpointer data)
{
  Progress_t *p = (Progress_t *) data;

  (*p->callback) ();
}


Progress_t *
progress_new (char *title, char *text,
	      GtkSignalFunc callback, gpointer callback_data)
{
  GtkWidget *button, *label;
  GtkBox *vbox, *hbox;
  Progress_t *p;

  p = (Progress_t *) malloc (sizeof (Progress_t));
  p->callback = callback;

  p->shell = gtk_dialog_new ();
  gtk_widget_set_uposition (p->shell, progress_x, progress_y);
  gtk_window_set_title (GTK_WINDOW (p->shell), title);
  vbox = GTK_BOX (GTK_DIALOG (p->shell)->vbox);
  hbox = GTK_BOX (GTK_DIALOG (p->shell)->action_area);

  gtk_container_border_width (GTK_CONTAINER (vbox), 7);

  label = gtk_label_new (text);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_box_pack_start (vbox, label, FALSE, TRUE, 0);

  p->pbar = gtk_progress_bar_new ();
  gtk_widget_set_usize (p->pbar, 200, 20);
  gtk_box_pack_start (vbox, p->pbar, TRUE, TRUE, 0);

  button = gtk_toggle_button_new_with_label ("Cancel");
  gtk_signal_connect (GTK_OBJECT (button), "clicked",
		      (GtkSignalFunc) progress_cancel, p);
  gtk_box_pack_start (hbox, button, TRUE, TRUE, 0);

  gtk_widget_show (label);
  gtk_widget_show (p->pbar);
  gtk_widget_show (button);
  gtk_widget_show (GTK_WIDGET (p->shell));
  return p;
}

void
progress_free (Progress_t * p)
{
  if (p)
    {
      gtk_widget_destroy (p->shell);
      free (p);
    }
}

void
progress_update (Progress_t * p, gfloat newval)
{
  if (p)
    gtk_progress_bar_update (GTK_PROGRESS_BAR (p->pbar), newval);
}
