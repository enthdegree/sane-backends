/* sane - Scanner Access Now Easy.
   Copyright (C) 1997 David Mosberger-Tang and Tristan Tarrant
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
/*
  The preview strategy is as follows:

   1) A preview always acquires an image that covers the entire
      scan surface.  This is necessary so the user can see not
      only what is, but also what isn't selected.

   2) The preview must be zoomable so the user can precisely pick
      the selection area even for small scans on a large scan
      surface.  The user also should have the option of resizing
      the preview window.

   3) We let the user/backend pick whether a preview is in color,
      grayscale, lineart or what not.  The only options that the
      preview may (temporarily) modify are:

	- resolution (set so the preview fills the window)
	- scan area options (top-left corner, bottom-right corner)
	- preview option (to let the backend know we're doing a preview)

   4) The size of the scan surface is determined based on the constraints
      of the four corner coordinates.  Missing constraints are replaced
      by +/-INF as appropriate (-INF form top-left, +INF for bottom-right
      coords).

   5) The size of the preview window is determined based on the scan
      surface size:

          If the surface area is specified in pixels and if that size
	  fits on the screen, we use that size.  In all other cases,
	  we make the window of a size so that neither the width nor
	  the height is bigger than some fraction of the screen-size
	  while preserving the aspect ratio (a surface width or height
	  of INF implies an aspect ratio of 1).

   6) Given the preview window size and the scan surface size, we
      select the resolution so the acquired preview image just fits
      in the preview window.  The resulting resolution may be out
      of range in which case we pick the minum/maximum if there is
      a range or word-list constraint or a default value if there is
      no such constraint.

   7) Once a preview image has been acquired, we know the size of the
      preview image (in pixels).  An initial scale factor is chosen
      so the image fits into the preview window.

      */

#include <assert.h>
#include <math.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <sys/param.h>

#include <gtkglue.h>
#include <preview.h>
#include <preferences.h>

#ifndef PATH_MAX
# define PATH_MAX	1024
#endif

/* Anything bigger than 2G will do, since SANE coordinates are 32 bit
   values.  */
#define INF		5.0e9

#define MM_PER_INCH	25.4

/* Cut fp conversion routines some slack: */
#define GROSSLY_DIFFERENT(f1,f2)	(fabs ((f1) - (f2)) > 1e-3)

#ifdef __alpha__
  /* This seems to be necessary for at least some XFree86 3.1.2
     servers.  It's known to be necessary for the XF86_TGA server for
     Linux/Alpha.  Fortunately, it's no great loss so we turn this on
     by default for now.  */
# define XSERVER_WITH_BUGGY_VISUALS
#endif

/* forward declarations */
static void scan_start (Preview *p);
static void scan_done (Preview *p);

static void
draw_rect (GdkWindow *win, GdkGC *gc, int coord[4])
{
  gint x, y, w, h;

  x = coord[0];
  y = coord[1];
  w = coord[2] - x;
  h = coord[3] - y;
  if (w < 0)
    {
      x = coord[2];
      w = -w;
    }
  if (h < 0)
    {
      y = coord[3];
      h = -h;
    }
  gdk_draw_rectangle (win, gc, FALSE, x, y, w + 1, h + 1);
}

static void
draw_selection (Preview *p)
{
  if (!p->gc)
    /* window isn't mapped yet */
    return;

  if (p->previous_selection.active)
    draw_rect (p->window->window, p->gc, p->previous_selection.coord);

  if (p->selection.active)
    draw_rect (p->window->window, p->gc, p->selection.coord);

  p->previous_selection = p->selection;
}

static void
update_selection (Preview *p)
{
  float min, max, normal, dev_selection[4];
  const SANE_Option_Descriptor *opt;
  SANE_Status status;
  SANE_Word val;
  int i, optnum;

  p->previous_selection = p->selection;

  memcpy (dev_selection, p->surface, sizeof (dev_selection));
  for (i = 0; i < 4; ++i)
    {
      optnum = p->dialog->well_known.coord[i];
      if (optnum > 0)
	{
	  opt = sane_get_option_descriptor (p->dialog->dev, optnum);
	  status = sane_control_option (p->dialog->dev, optnum,
					SANE_ACTION_GET_VALUE, &val, 0);
	  if (status != SANE_STATUS_GOOD)
	    continue;
	  if (opt->type == SANE_TYPE_FIXED)
	    dev_selection[i] = SANE_UNFIX (val);
	  else
	    dev_selection[i] = val;
	}
    }
  for (i = 0; i < 2; ++i)
    {
      min = p->surface[i];
      if (min <= -INF)
	min = 0.0;
      max = p->surface[i + 2];
      if (max >= INF)
	max = p->preview_width;

      normal = ((i == 0) ? p->preview_width : p->preview_height) - 1;
      normal /= (max - min);
      p->selection.active = TRUE;
      p->selection.coord[i]     = ((dev_selection[i] - min)*normal) + 0.5;
      p->selection.coord[i + 2] = ((dev_selection[i + 2] - min)*normal) + 0.5;
      if (p->selection.coord[i + 2] < p->selection.coord[i])
	p->selection.coord[i + 2] = p->selection.coord[i];
    }
  draw_selection (p);
}

static void
get_image_scale (Preview *p, float *xscalep, float *yscalep)
{
  float xscale, yscale;

  if (p->image_width == 0)
    xscale = 1.0;
  else
    {
      xscale = p->image_width/(float) p->preview_width;
      if (p->image_height > 0 && p->preview_height*xscale < p->image_height)
	xscale = p->image_height/(float) p->preview_height;
    }
  yscale = xscale;

  if (p->surface_unit == SANE_UNIT_PIXEL
      && p->image_width <= p->preview_width
      && p->image_height <= p->preview_height)
    {
      float swidth, sheight;

      assert (p->surface_type == SANE_TYPE_INT);
      swidth  = (p->surface[GSG_BR_X] - p->surface[GSG_TL_X] + 1);
      sheight = (p->surface[GSG_BR_Y] - p->surface[GSG_TL_Y] + 1);
      xscale = 1.0;
      yscale = 1.0;
      if (p->image_width > 0 && swidth < INF)
	xscale = p->image_width/swidth;
      if (p->image_height > 0 && sheight < INF)
	yscale = p->image_height/sheight;
    }
  *xscalep = xscale;
  *yscalep = yscale;
}

static void
paint_image (Preview *p)
{
  float xscale, yscale, src_x, src_y;
  int dst_x, dst_y, height, x, y, src_offset;
  gint gwidth, gheight;

  gwidth  = p->preview_width;
  gheight = p->preview_height;

  get_image_scale (p, &xscale, &yscale);

  memset (p->preview_row, 0xff, 3*gwidth);

  /* don't draw last line unless it's complete: */
  height = p->image_y;
  if (p->image_x == 0 && height < p->image_height)
    ++height;

  /* for now, use simple nearest-neighbor interpolation: */
  src_offset = 0;
  src_x = src_y = 0.0;
  for (dst_y = 0; dst_y < gheight; ++dst_y)
    {
      y = (int) (src_y + 0.5);
      if (y >= height)
	break;
      src_offset = y*3*p->image_width;

      if (p->image_data)
	for (dst_x = 0; dst_x < gwidth; ++dst_x)
	  {
	    x = (int) (src_x + 0.5);
	    if (x >= p->image_width)
	      break;

	    p->preview_row[3*dst_x + 0] = p->image_data[src_offset + 3*x + 0];
	    p->preview_row[3*dst_x + 1] = p->image_data[src_offset + 3*x + 1];
	    p->preview_row[3*dst_x + 2] = p->image_data[src_offset + 3*x + 2];
	    src_x += xscale;
	  }
      gtk_preview_draw_row (GTK_PREVIEW (p->window), p->preview_row,
			    0, dst_y, gwidth);
      src_x = 0.0;
      src_y += yscale;
    }
}

static void
display_partial_image (Preview *p)
{
  paint_image (p);

  if (GTK_WIDGET_DRAWABLE (p->window))
    {
      GtkPreview *preview = GTK_PREVIEW (p->window);
      int src_x, src_y;

      src_x = (p->window->allocation.width - preview->buffer_width)/2;
      src_y = (p->window->allocation.height - preview->buffer_height)/2;
      gtk_preview_put (preview, p->window->window, p->window->style->black_gc,
		       src_x, src_y,
		       0, 0, p->preview_width, p->preview_height);
    }
}

static void
display_maybe (Preview *p)
{
  time_t now;

  time (&now);
  if (now > p->image_last_time_updated)
    {
      p->image_last_time_updated = now;
      display_partial_image (p);
    }
}

static void
display_image (Preview *p)
{
  if (p->params.lines <= 0 && p->image_y < p->image_height)
    {
      p->image_height = p->image_y;
      p->image_data = realloc (p->image_data,
			       3*p->image_width*p->image_height);
      assert (p->image_data);
    }
  display_partial_image (p);
  scan_done (p);
}

static void
preview_area_resize (GtkWidget *widget)
{
  float min_x, max_x, min_y, max_y, xscale, yscale, f;
  Preview *p;

  p = gtk_object_get_data (GTK_OBJECT (widget), "PreviewPointer");

  p->preview_width  = widget->allocation.width;
  p->preview_height = widget->allocation.height;

  if (p->preview_row)
    p->preview_row = realloc (p->preview_row, 3*p->preview_width);
  else
    p->preview_row = malloc (3*p->preview_width);

  /* set the ruler ranges: */

  min_x = p->surface[GSG_TL_X];
  if (min_x <= -INF)
    min_x = 0.0;

  max_x = p->surface[GSG_BR_X];
  if (max_x >=  INF)
    max_x = p->preview_width - 1;

  min_y = p->surface[GSG_TL_Y];
  if (min_y <= -INF)
    min_y = 0.0;

  max_y = p->surface[GSG_BR_Y];
  if (max_y >=  INF)
    max_y = p->preview_height - 1;

  /* convert mm to inches if that's what the user wants: */

  if (p->surface_unit == SANE_UNIT_MM)
    {
      double factor = 1.0/preferences.length_unit;
      min_x *= factor; max_x *= factor; min_y *= factor; max_y *= factor;
    }

  get_image_scale (p, &xscale, &yscale);

  if (p->surface_unit == SANE_UNIT_PIXEL)
    f = 1.0/xscale;
  else if (p->image_width)
    f = xscale*p->preview_width/p->image_width;
  else
    f = 1.0;
  gtk_ruler_set_range (GTK_RULER (p->hruler), f*min_x, f*max_x, f*min_x,
		       /* max_size */ 20);

  if (p->surface_unit == SANE_UNIT_PIXEL)
    f = 1.0/yscale;
  else if (p->image_height)
    f = yscale*p->preview_height/p->image_height;
  else
    f = 1.0;
  gtk_ruler_set_range (GTK_RULER (p->vruler), f*min_y, f*max_y, f*min_y,
		       /* max_size */ 20);

  paint_image (p);
  update_selection (p);
}

static void
get_bounds (const SANE_Option_Descriptor *opt, float *minp, float *maxp)
{
  float min, max;
  int i;

  min = -INF;
  max =  INF;
  switch (opt->constraint_type)
    {
    case SANE_CONSTRAINT_RANGE:
      min = opt->constraint.range->min;
      max = opt->constraint.range->max;
      break;

    case SANE_CONSTRAINT_WORD_LIST:
      min =  INF;
      max = -INF;
      for (i = 1; i <= opt->constraint.word_list[0]; ++i)
	{
	  if (opt->constraint.word_list[i] < min)
	    min = opt->constraint.word_list[i];
	  if (opt->constraint.word_list[i] > max)
	    max = opt->constraint.word_list[i];
	}
      break;

    default:
      break;
    }
  if (opt->type == SANE_TYPE_FIXED)
    {
      if (min > -INF && min < INF)
	min = SANE_UNFIX (min);
      if (max > -INF && max < INF)
	max = SANE_UNFIX (max);
    }
  *minp = min;
  *maxp = max;
}

static void
save_option (Preview *p, int option, SANE_Word *save_loc, int *valid)
{
  SANE_Status status;

  if (option <= 0)
    {
      *valid = 0;
      return;
    }
  status = sane_control_option (p->dialog->dev, option, SANE_ACTION_GET_VALUE,
				save_loc, 0);
  *valid = (status == SANE_STATUS_GOOD);
}

static void
restore_option (Preview *p, int option, SANE_Word saved_value, int valid)
{
  const SANE_Option_Descriptor *opt;
  SANE_Status status;
  SANE_Handle dev;

  if (!valid)
    return;

  dev = p->dialog->dev;
  status = sane_control_option (dev, option, SANE_ACTION_SET_VALUE,
				&saved_value, 0);
  if (status != SANE_STATUS_GOOD)
    {
      char buf[256];
      opt = sane_get_option_descriptor (dev, option);
      snprintf (buf, sizeof (buf), "Failed restore value of option %s: %s.",
		opt->name, sane_strstatus (status));
      gsg_error (buf);
    }
}

static void
set_option_float (Preview *p, int option, float value)
{
  const SANE_Option_Descriptor *opt;
  SANE_Handle dev;
  SANE_Word word;

  if (option <= 0 || value <= -INF || value >= INF)
    return;

  dev = p->dialog->dev;
  opt = sane_get_option_descriptor (dev, option);
  if (opt->type == SANE_TYPE_FIXED)
    word = SANE_FIX (value) + 0.5;
  else
    word = value + 0.5;
  sane_control_option (dev, option, SANE_ACTION_SET_VALUE, &word, 0);
}

static void
set_option_bool (Preview *p, int option, SANE_Bool value)
{
  SANE_Handle dev;

  if (option <= 0)
    return;

  dev = p->dialog->dev;
  sane_control_option (dev, option, SANE_ACTION_SET_VALUE, &value, 0);
}

static int
increment_image_y (Preview *p)
{
  size_t extra_size, offset;
  char buf[256];

  p->image_x = 0;
  ++p->image_y;
  if (p->params.lines <= 0 && p->image_y >= p->image_height)
    {
      offset = 3*p->image_width*p->image_height;
      extra_size = 3*32*p->image_width;
      p->image_height += 32;
      p->image_data = realloc (p->image_data, offset + extra_size);
      if (!p->image_data)
	{
	  snprintf (buf, sizeof (buf),
		    "Failed to reallocate image memory: %s.",
		    strerror (errno));
	  gsg_error (buf);
	  scan_done (p);
	  return -1;
	}
      memset (p->image_data + offset, 0xff, extra_size);
    }
  return 0;
}

static void
input_available (gpointer data, gint source, GdkInputCondition cond)
{
  SANE_Status status;
  Preview *p = data;
  u_char buf[8192];
  SANE_Handle dev;
  SANE_Int len;
  int i, j;

  dev = p->dialog->dev;
  while (1)
    {
      status = sane_read (dev, buf, sizeof (buf), &len);
      if (status != SANE_STATUS_GOOD)
	{
	  if (status == SANE_STATUS_EOF)
	    {
	      if (p->params.last_frame)
		display_image (p);
	      else
		{
		  gdk_input_remove (p->input_tag);
		  p->input_tag = -1;
		  scan_start (p);
		  break;
		}
	    }
	  else
	    {
	      snprintf (buf, sizeof (buf), "Error during read: %s.",
			sane_strstatus (status));
	      gsg_error (buf);
	    }
	  scan_done (p);
	  return;
	}
      if (!len)
	break;			/* out of data for now */

      switch (p->params.format)
	{
	case SANE_FRAME_RGB:
	  if (p->params.depth != 8)
	    goto bad_depth;

	  for (i = 0; i < len; ++i)
	    {
	      p->image_data[p->image_offset++] = buf[i];
	      if (p->image_offset%3 == 0)
		{
		  if (++p->image_x >= p->image_width
		      && increment_image_y (p) < 0)
		    return;
		}
	    }
	  break;

	case SANE_FRAME_GRAY:
	  switch (p->params.depth)
	    {
	    case 1:
	      for (i = 0; i < len; ++i)
		{
		  u_char mask = buf[i];

		  for (j = 7; j >= 0; --j)
		    {
		      u_char gl = (mask & (1 << j)) ? 0x00 : 0xff;
		      p->image_data[p->image_offset++] = gl;
		      p->image_data[p->image_offset++] = gl;
		      p->image_data[p->image_offset++] = gl;
		      if (++p->image_x >= p->image_width)
			{
			  if (increment_image_y (p) < 0)
			    return;
			  break;	/* skip padding bits */
			}
		    }
		}
	      break;

	    case 8:
	      for (i = 0; i < len; ++i)
		{
		  u_char gl = buf[i];
		  p->image_data[p->image_offset++] = gl;
		  p->image_data[p->image_offset++] = gl;
		  p->image_data[p->image_offset++] = gl;
		  if (++p->image_x >= p->image_width
		      && increment_image_y (p) < 0)
		    return;
		}
	      break;

	    default:
	      goto bad_depth;
	    }
	  break;

	case SANE_FRAME_RED:
	case SANE_FRAME_GREEN:
	case SANE_FRAME_BLUE:
	  switch (p->params.depth)
	    {
	    case 1:
	      for (i = 0; i < len; ++i)
		{
		  u_char mask = buf[i];

		  for (j = 0; j < 8; ++j)
		    {
		      u_char gl = (mask & 1) ? 0xff : 0x00;
		      mask >>= 1;
		      p->image_data[p->image_offset++] = gl;
		      p->image_offset += 3;
		      if (++p->image_x >= p->image_width
			  && increment_image_y (p) < 0)
			return;
		    }
		}
	      break;

	    case 8:
	      for (i = 0; i < len; ++i)
		{
		  p->image_data[p->image_offset] = buf[i];
		  p->image_offset += 3;
		  if (++p->image_x >= p->image_width
		      && increment_image_y (p) < 0)
		    return;
		}
	      break;

	    default:
	      goto bad_depth;
	    }
	  break;

	default:
	  fprintf (stderr, "preview.input_available: bad frame format %d\n",
		   p->params.format);
	  scan_done (p);
	  return;
	}
      if (p->input_tag < 0)
	{
	  display_maybe (p);
	  while (gtk_events_pending ())
	    gtk_main_iteration ();
	}
    }
  display_maybe (p);
  return;

bad_depth:
  snprintf (buf, sizeof (buf), "Preview cannot handle depth %d.",
	    p->params.depth);
  gsg_error (buf);
  scan_done (p);
  return;
}

static void
scan_done (Preview *p)
{
  int i;

  p->scanning = FALSE;
  if (p->input_tag >= 0)
    {
      gdk_input_remove (p->input_tag);
      p->input_tag = -1;
    }
  sane_cancel (p->dialog->dev);

  restore_option (p, p->dialog->well_known.dpi,
		  p->saved_dpi, p->saved_dpi_valid);
  for (i = 0; i < 4; ++i)
    restore_option (p, p->dialog->well_known.coord[i],
		    p->saved_coord[i], p->saved_coord_valid[i]);
  set_option_bool (p, p->dialog->well_known.preview, SANE_FALSE);

  gtk_widget_set_sensitive (p->cancel, FALSE);
  gsg_set_sensitivity (p->dialog, TRUE);
}

static void
scan_start (Preview *p)
{
  SANE_Handle dev = p->dialog->dev;
  SANE_Status status;
  char buf[256];
  int fd, y;

  gtk_widget_set_sensitive (p->cancel, TRUE);
  gsg_set_sensitivity (p->dialog, FALSE);

  /* clear old preview: */
  memset (p->preview_row, 0xff, 3*p->preview_width);
  for (y = 0; y < p->preview_height; ++y)
    gtk_preview_draw_row (GTK_PREVIEW (p->window), p->preview_row,
			  0, y, p->preview_width);

  if (p->input_tag >= 0)
    {
      gdk_input_remove (p->input_tag);
      p->input_tag = -1;
    }

  gsg_sync (p->dialog);

  status = sane_start (dev);
  if (status != SANE_STATUS_GOOD)
    {
      snprintf (buf, sizeof (buf),
		"Failed to start scanner: %s.", sane_strstatus (status));
      gsg_error (buf);
      scan_done (p);
      return;
    }

  status = sane_get_parameters (dev, &p->params);
  if (status != SANE_STATUS_GOOD)
    {
      snprintf (buf, sizeof (buf),
		"Failed to obtain parameters: %s.", sane_strstatus (status));
      gsg_error (buf);
      scan_done (p);
      return;
    }

  p->image_offset = p->image_x = p->image_y = 0;

  if (p->params.format >= SANE_FRAME_RED
      && p->params.format <= SANE_FRAME_BLUE)
    p->image_offset = p->params.format - SANE_FRAME_RED;

  if (!p->image_data || p->params.pixels_per_line != p->image_width
      || (p->params.lines >= 0 && p->params.lines != p->image_height))
    {
      /* image size changed */
      if (p->image_data)
	free (p->image_data);

      p->image_width  = p->params.pixels_per_line;
      p->image_height = p->params.lines;
      if (p->image_height < 0)
	p->image_height = 32;	/* may have to adjust as we go... */

      p->image_data = malloc (3*p->image_width*p->image_height);
      if (!p->image_data)
	{
	  snprintf (buf, sizeof (buf),
		    "Failed to allocate image memory: %s.", strerror (errno));
	  gsg_error (buf);
	  scan_done (p);
	  return;
	}
      memset (p->image_data, 0xff, 3*p->image_width*p->image_height);
    }

  if (p->selection.active)
    {
      p->previous_selection = p->selection;
      p->selection.active = FALSE;
      draw_selection (p);
    }
  p->scanning = TRUE;

  if (sane_set_io_mode (dev, SANE_TRUE) == SANE_STATUS_GOOD
      && sane_get_select_fd (dev, &fd) == SANE_STATUS_GOOD)
    p->input_tag = gdk_input_add (fd, GDK_INPUT_READ | GDK_INPUT_EXCEPTION, input_available, p);
  else
    input_available (p, -1, GDK_INPUT_READ);
}

static void
establish_selection (Preview *p)
{
  float min, max, normal, dev_selection[4];
  int i;

  memcpy (dev_selection, p->surface, sizeof (dev_selection));
  if (p->selection.active)
    for (i = 0; i < 2; ++i)
      {
	min = p->surface[i];
	if (min <= -INF)
	  min = 0.0;
	max = p->surface[i + 2];
	if (max >= INF)
	  max = p->preview_width;

	normal = 1.0/(((i == 0) ? p->preview_width : p->preview_height) - 1);
	normal *= (max - min);
	dev_selection[i]     = p->selection.coord[i]*normal + min;
	dev_selection[i + 2] = p->selection.coord[i + 2]*normal + min;
      }
  for (i = 0; i < 4; ++i)
    set_option_float (p, p->dialog->well_known.coord[i], dev_selection[i]);
  gsg_update_scan_window (p->dialog);
  if (p->dialog->param_change_callback)
    (*p->dialog->param_change_callback) (p->dialog,
					 p->dialog->param_change_arg);
}

static int
make_preview_image_path (Preview *p, size_t filename_size, char *filename)
{
  return gsg_make_path (filename_size, filename, 0, "preview-",
			p->dialog->dev_name, ".ppm");
}

static void
restore_preview_image (Preview *p)
{
  u_int psurface_type, psurface_unit;
  char filename[PATH_MAX];
  int width, height;
  float psurface[4];
  size_t nread;
  FILE *in;

  /* See whether there is a saved preview and load it if present: */

  if (make_preview_image_path (p, sizeof (filename), filename) < 0)
    return;

  in = fopen (filename, "r");
  if (!in)
    return;

  /* Be careful about consuming too many bytes after the final newline
     (e.g., consider an image whose first image byte is 13 (`\r').  */
  if (fscanf (in, "P6\n# surface: %g %g %g %g %u %u\n%d %d\n255%*[\n]",
	      psurface + 0, psurface + 1, psurface + 2, psurface + 3,
	      &psurface_type, &psurface_unit,
	      &width, &height) != 8)
    return;

  if (GROSSLY_DIFFERENT (psurface[0], p->surface[0])
      || GROSSLY_DIFFERENT (psurface[1], p->surface[1])
      || GROSSLY_DIFFERENT (psurface[2], p->surface[2])
      || GROSSLY_DIFFERENT (psurface[3], p->surface[3])
      || psurface_type != p->surface_type || psurface_unit != p->surface_unit)
    /* ignore preview image that was acquired for/with a different surface */
    return;

  p->image_width = width;
  p->image_height = height;
  p->image_data = malloc (3*width*height);
  if (!p->image_data)
    return;

  nread = fread (p->image_data, 3, width*height, in);

  p->image_y = nread/width;
  p->image_x = nread%width;
}

/* This is executed _after_ the gtkpreview's expose routine.  */
static gint
expose_handler (GtkWidget *window, GdkEvent *event, gpointer data)
{
  Preview *p = data;

  p->selection.active = FALSE;
  update_selection (p);
  return FALSE;
}

static gint
event_handler (GtkWidget *window, GdkEvent *event, gpointer data)
{
  Preview *p = data;
  int i, tmp;

  if (event->type == GDK_EXPOSE)
    {
      if (!p->gc)
	{
	  p->gc = gdk_gc_new (p->window->window);
	  gdk_gc_set_function (p->gc, GDK_INVERT);
	  gdk_gc_set_line_attributes (p->gc, 1, GDK_LINE_ON_OFF_DASH,
				      GDK_CAP_BUTT, GDK_JOIN_MITER);
	  paint_image (p);
	}
      else
	{
	  p->selection.active = FALSE;
	  draw_selection (p);
	}
    }
  else if (!p->scanning)
    switch (event->type)
      {
      case GDK_UNMAP:
      case GDK_MAP:
	break;

      case GDK_BUTTON_PRESS:
	p->selection.coord[0] = event->button.x;
	p->selection.coord[1] = event->button.y;
	p->selection_drag = TRUE;
	break;

      case GDK_BUTTON_RELEASE:
	if (!p->selection_drag)
	  break;
	p->selection_drag = FALSE;

	p->selection.coord[2] = event->button.x;
	p->selection.coord[3] = event->button.y;
	p->selection.active =
	  (p->selection.coord[0] != p->selection.coord[2]
	   || p->selection.coord[1] != p->selection.coord[3]);

	if (p->selection.active)
	  {
	    for (i = 0; i < 2; i += 1)
	      if (p->selection.coord[i] > p->selection.coord[i + 2])
		{
		  tmp = p->selection.coord[i];
		  p->selection.coord[i] = p->selection.coord[i + 2];
		  p->selection.coord[i + 2] = tmp;
		}
	    if (p->selection.coord[0] < 0)
	      p->selection.coord[0] = 0;
	    if (p->selection.coord[1] < 0)
	      p->selection.coord[1] = 0;
	    if (p->selection.coord[2] >= p->preview_width)
	      p->selection.coord[2] = p->preview_width - 1;
	    if (p->selection.coord[3] >= p->preview_height)
	      p->selection.coord[3] = p->preview_height - 1;
	  }
	draw_selection (p);
	establish_selection (p);
	break;

      case GDK_MOTION_NOTIFY:
	if (p->selection_drag)
	  {
	    p->selection.active = TRUE;
	    p->selection.coord[2] = event->motion.x;
	    p->selection.coord[3] = event->motion.y;
	    draw_selection (p);
	  }
	break;

      default:
#if 0
	fprintf (stderr, "event_handler: unhandled event type %d\n",
		 event->type);
#endif
	break;
      }
  return FALSE;
}

static void
start_button_clicked (GtkWidget *widget, gpointer data)
{
  preview_scan (data);
}

static void
cancel_button_clicked (GtkWidget *widget, gpointer data)
{
  scan_done (data);
}

static void
top_destroyed (GtkWidget *widget, gpointer call_data)
{
  Preview *p = call_data;

  p->top = NULL;
}

Preview *
preview_new (GSGDialog *dialog)
{
  static int first_time = 1;
  GtkWidget *table, *frame, *button;
  GtkSignalFunc signal_func;
  GtkWidgetClass *class;
  GtkBox *vbox, *hbox;
  Preview *p;

  p = malloc (sizeof (*p));
  if (!p)
    return 0;
  memset (p, 0, sizeof (*p));

  p->dialog = dialog;
  p->input_tag = -1;

  if (first_time)
    {
      first_time = 0;
      gtk_preview_set_gamma (preferences.preview_gamma);
      gtk_preview_set_install_cmap (preferences.preview_own_cmap);
    }

#ifndef XSERVER_WITH_BUGGY_VISUALS
  gtk_widget_push_visual (gtk_preview_get_visual ());
#endif
  gtk_widget_push_colormap (gtk_preview_get_cmap ());

  p->top = gtk_dialog_new ();
  gtk_signal_connect (GTK_OBJECT (p->top), "destroy",
		      GTK_SIGNAL_FUNC (top_destroyed), p);
  gtk_window_set_title (GTK_WINDOW (p->top), "xscan preview");
  vbox = GTK_BOX (GTK_DIALOG (p->top)->vbox);
  hbox = GTK_BOX (GTK_DIALOG (p->top)->action_area);

  /* construct the preview area (table with sliders & preview window) */
  table = gtk_table_new (2, 2, /* homogeneous */ FALSE);
  gtk_table_set_col_spacing (GTK_TABLE (table), 0, 1);
  gtk_table_set_row_spacing (GTK_TABLE (table), 0, 1);
  gtk_container_border_width (GTK_CONTAINER (table), 2);
  gtk_box_pack_start (vbox, table, /* expand */ TRUE, /* fill */ TRUE,
		      /* padding */ 0);

  /* the empty box in the top-left corner */
  frame = gtk_frame_new (/* label */ 0);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
  gtk_table_attach (GTK_TABLE (table), frame, 0, 1, 0, 1,
		    GTK_FILL, GTK_FILL, 0, 0);

  /* the horizontal ruler */
  p->hruler = gtk_hruler_new ();
  gtk_table_attach (GTK_TABLE (table), p->hruler, 1, 2, 0, 1,
		    GTK_FILL, 0, 0, 0);

  /* the vertical ruler */
  p->vruler = gtk_vruler_new ();
  gtk_table_attach (GTK_TABLE (table), p->vruler, 0, 1, 1, 2, 0,
		    GTK_FILL, 0, 0);

  /* the preview area */

  p->window = gtk_preview_new (GTK_PREVIEW_COLOR);
  gtk_preview_set_expand (GTK_PREVIEW (p->window), TRUE);
  gtk_widget_set_events (p->window,
			 GDK_EXPOSURE_MASK |
			 GDK_POINTER_MOTION_MASK |
			 GDK_POINTER_MOTION_HINT_MASK |
			 GDK_BUTTON_PRESS_MASK |
			 GDK_BUTTON_RELEASE_MASK);
  gtk_signal_connect (GTK_OBJECT (p->window), "event",
		      (GtkSignalFunc) event_handler, p);
  gtk_signal_connect_after (GTK_OBJECT (p->window), "expose_event",
			    (GtkSignalFunc) expose_handler, p);
  gtk_signal_connect_after (GTK_OBJECT (p->window), "size_allocate",
			    (GtkSignalFunc) preview_area_resize, 0);
  gtk_object_set_data (GTK_OBJECT (p->window), "PreviewPointer", p);

  /* Connect the motion-notify events of the preview area with the
     rulers.  Nifty stuff!  */

  class = GTK_WIDGET_CLASS (GTK_OBJECT (p->hruler)->klass);
  signal_func = (GtkSignalFunc) class->motion_notify_event;
  gtk_signal_connect_object (GTK_OBJECT (p->window), "motion_notify_event",
			     signal_func, GTK_OBJECT (p->hruler));

  class = GTK_WIDGET_CLASS (GTK_OBJECT (p->vruler)->klass);
  signal_func = (GtkSignalFunc) class->motion_notify_event;
  gtk_signal_connect_object (GTK_OBJECT (p->window), "motion_notify_event",
			     signal_func, GTK_OBJECT (p->vruler));

  p->viewport = gtk_frame_new (/* label */ 0);
  gtk_frame_set_shadow_type (GTK_FRAME (p->viewport), GTK_SHADOW_IN);
  gtk_container_add (GTK_CONTAINER (p->viewport), p->window);

  gtk_table_attach (GTK_TABLE (table), p->viewport, 1, 2, 1, 2,
		    GTK_FILL | GTK_EXPAND | GTK_SHRINK,
		    GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);

  preview_update (p);

  /* fill in action area: */

  /* Start button */
  button = gtk_button_new_with_label ("Acquire Preview");
  gtk_signal_connect (GTK_OBJECT (button), "clicked",
		      (GtkSignalFunc) start_button_clicked, p);
  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
  gtk_widget_show (button);

  /* Cancel button */
  p->cancel = gtk_button_new_with_label ("Cancel Preview");
  gtk_signal_connect (GTK_OBJECT (p->cancel), "clicked",
		      (GtkSignalFunc) cancel_button_clicked, p);
  gtk_box_pack_start (GTK_BOX (hbox), p->cancel, TRUE, TRUE, 0);
  gtk_widget_set_sensitive (p->cancel, FALSE);

  gtk_widget_show (p->cancel);
  gtk_widget_show (p->viewport);
  gtk_widget_show (p->window);
  gtk_widget_show (p->hruler);
  gtk_widget_show (p->vruler);
  gtk_widget_show (frame);
  gtk_widget_show (table);
  gtk_widget_show (p->top);

  gtk_widget_pop_colormap ();
#ifndef XSERVER_WITH_BUGGY_VISUALS
  gtk_widget_pop_visual ();
#endif
  return p;
}

void
preview_update (Preview *p)
{
  float val, width, height, max_width, max_height;
  const SANE_Option_Descriptor *opt;
  int i, surface_changed;
  SANE_Value_Type type;
  SANE_Unit unit;
  float min, max;

  surface_changed = 0;
  unit = SANE_UNIT_PIXEL;
  type = SANE_TYPE_INT;
  for (i = 0; i < 4; ++i)
    {
      val = (i & 2) ? INF : -INF;
      if (p->dialog->well_known.coord[i] > 0)
	{
	  opt = sane_get_option_descriptor (p->dialog->dev,
					    p->dialog->well_known.coord[i]);
	  assert (opt->unit == SANE_UNIT_PIXEL || opt->unit == SANE_UNIT_MM);
	  unit = opt->unit;
	  type = opt->type;

	  get_bounds (opt, &min, &max);
	  if (i & 2)
	    val = max;
	  else
	    val = min;
	}
      if (p->surface[i] != val)
	{
	  surface_changed = 1;
	  p->surface[i] = val;
	}
    }
  if (p->surface_unit != unit)
    {
      surface_changed = 1;
      p->surface_unit = unit;
    }
  if (p->surface_type != type)
    {
      surface_changed = 1;
      p->surface_type = type;
    }
  if (surface_changed && p->image_data)
    {
      free (p->image_data);
      p->image_data = 0;
      p->image_width = 0;
      p->image_height = 0;
    }

  /* guess the initial preview window size: */

  width  = p->surface[GSG_BR_X] - p->surface[GSG_TL_X];
  height = p->surface[GSG_BR_Y] - p->surface[GSG_TL_Y];
  if (p->surface_type == SANE_TYPE_INT)
    {
      width  += 1.0;
      height += 1.0;
    }
  else
    {
      width  += SANE_UNFIX (1.0);
      height += SANE_UNFIX (1.0);
    }

  assert (width > 0.0 && height > 0.0);

  if (width >= INF || height >= INF)
    p->aspect = 1.0;
  else
    p->aspect = width/height;

  max_width  = 0.5*gdk_screen_width ();
  max_height = 0.5*gdk_screen_height ();

  if (p->surface_unit != SANE_UNIT_PIXEL)
    {
      width  = max_width;
      height = max_height;
    }
  else
    {
      if (width > max_width)
	width  = max_width;

      if (height > max_height)
	height = max_height;
    }

  /* re-adjust so we maintain aspect without exceeding max size: */
  if (width/height != p->aspect)
    {
      if (p->aspect > 1.0)
	height = width/p->aspect;
      else
	width  = height*p->aspect;
    }

  p->preview_width = width + 0.5;
  p->preview_height = height + 0.5;
  if (surface_changed)
    {
      gtk_widget_set_usize (GTK_WIDGET (p->window),
			    p->preview_width, p->preview_height);
      if (GTK_WIDGET_DRAWABLE (p->window))
	preview_area_resize (p->window);

      if (preferences.preserve_preview)
	restore_preview_image (p);
    }
  update_selection (p);
}

void
preview_scan (Preview *p)
{
  float min, max, swidth, sheight, width, height, dpi = 0;
  const SANE_Option_Descriptor *opt;
  gint gwidth, gheight;
  int i;

  save_option (p, p->dialog->well_known.dpi,
	       &p->saved_dpi, &p->saved_dpi_valid);
  for (i = 0; i < 4; ++i)
    save_option (p, p->dialog->well_known.coord[i],
		 &p->saved_coord[i], p->saved_coord_valid + i);

  /* determine dpi, if necessary: */

  if (p->dialog->well_known.dpi > 0)
    {
      opt = sane_get_option_descriptor (p->dialog->dev,
					p->dialog->well_known.dpi);

      gwidth  = p->preview_width;
      gheight = p->preview_height;

      height = gheight;
      width  = height*p->aspect;
      if (width > gwidth)
	{
	  width  = gwidth;
	  height = width/p->aspect;
	}

      swidth = (p->surface[GSG_BR_X] - p->surface[GSG_TL_X]);
      if (swidth < INF)
	dpi = MM_PER_INCH*width/swidth;
      else
	{
	  sheight = (p->surface[GSG_BR_Y] - p->surface[GSG_TL_Y]);
	  if (sheight < INF)
	    dpi = MM_PER_INCH*height/sheight;
	  else
	    dpi = 18.0;
	}
      get_bounds (opt, &min, &max);
      if (dpi < min)
	dpi = min;
      if (dpi > max)
	dpi = max;

      set_option_float (p, p->dialog->well_known.dpi, dpi);
    }

  /* set the scan window (necessary since backends may default to
     non-maximum size):  */
  for (i = 0; i < 4; ++i)
    set_option_float (p, p->dialog->well_known.coord[i], p->surface[i]);
  set_option_bool (p, p->dialog->well_known.preview, SANE_TRUE);

  /* OK, all set to go */
  scan_start (p);
}

void
preview_destroy (Preview *p)
{
  char filename[PATH_MAX];
  FILE *out;

  if (p->scanning)
    scan_done (p);		/* don't save partial window */
  else if (preferences.preserve_preview && p->image_data
	   && make_preview_image_path (p, sizeof (filename), filename) >= 0)
    {
      /* save preview image */
      out = fopen (filename, "w");
      if (out)
	{
	  /* always save it as a PPM image: */
	  fprintf (out, "P6\n# surface: %g %g %g %g %u %u\n%d %d\n255\n",
		   p->surface[0], p->surface[1], p->surface[2], p->surface[3],
		   p->surface_type, p->surface_unit,
		   p->image_width, p->image_height);
	  fwrite (p->image_data, 3, p->image_width*p->image_height, out);
	  fclose (out);
	}
    }
  if (p->image_data)
    free (p->image_data);
  if (p->preview_row)
    free (p->preview_row);
  if (p->gc)
    gdk_gc_destroy (p->gc);
  if (p->top)
    gtk_widget_destroy (p->top);
  free (p);
}
