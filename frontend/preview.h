/* sane - Scanner Access Now Easy.
   Copyright (C) 1997 David Mosberger-Tang
   This file is part of the SANE package.

   SANE is free software; you can redistribute it and/or modify it under
   the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 2 of the License, or (at your
   option) any later version.

   SANE is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
   for more details.

   You should have received a copy of the GNU General Public License
   along with sane; see the file COPYING.  If not, write to the Free
   Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */
#ifndef preview_h
#define preview_h

#include <sys/types.h>

#include <sane/config.h>
#include <sane/sane.h>

typedef struct
  {
    GSGDialog *dialog;	/* the dialog for this preview */

    SANE_Value_Type surface_type;
    SANE_Unit surface_unit;
    float surface[4];	/* the corners of the scan surface (device coords) */
    float aspect;	/* the aspect ratio of the scan surface */

    int saved_dpi_valid;
    SANE_Word saved_dpi;
    int saved_coord_valid[4];
    SANE_Word saved_coord[4];

    /* desired/user-selected preview-window size: */
    int preview_width;
    int preview_height;
    u_char *preview_row;

    int scanning;
    time_t image_last_time_updated;
    gint input_tag;
    SANE_Parameters params;
    int image_offset;
    int image_x;
    int image_y;
    int image_width;
    int image_height;
    u_char *image_data;	/* 3 * image_width * image_height bytes */

    GdkGC *gc;
    int selection_drag;
    struct
      {
	int active;
	int coord[4];
      }
    selection, previous_selection;

    GtkWidget *top;	/* top-level widget */
    GtkWidget *hruler;
    GtkWidget *vruler;
    GtkWidget *viewport;
    GtkWidget *window;	/* the preview window */
    GtkWidget *cancel;	/* the cancel button */
  }
Preview;

/* Create a new preview based on the info in DIALOG.  */
extern Preview *preview_new (GSGDialog *dialog);

/* Some of the parameters may have changed---update the preview.  */
extern void preview_update (Preview *p);

/* Acquire a preview image and display it.  */
extern void preview_scan (Preview *p);

/* Destroy a preview.  */
extern void preview_destroy (Preview *p);

#endif /* preview_h */
