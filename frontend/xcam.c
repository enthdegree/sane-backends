/* xcam -- X-based camera frontend
   Uses the SANE library.
   Copyright (C) 1997 David Mosberger and Tristan Tarrant

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include <sane/config.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>

#include <gtkglue.h>
#include <preferences.h>
#include <sane/sane.h>
#include <sane/sanei.h>

#ifndef PATH_MAX
# define PATH_MAX	1024
#endif

#define MAX_LUM		64	/* how many graylevels for 8 bit displays */

#ifndef HAVE_ATEXIT
# define atexit(func)	on_exit(func, 0)	/* works for SunOS, at least */
#endif

typedef struct Canvas
  {
    GtkWidget *preview;
    GdkGC *gc;
    GdkImage *gdk_image;
    GdkColormap *graylevel_cmap;	/* for 8 bit displays */
    guint32 graylevel[MAX_LUM];		/* graylevel pixels */
    GdkColormap *cube_cmap;
    GdkColor cube_colors[5 * 6 * 5];
  }
Canvas;

Preferences preferences;

static const char *prog_name;
static const SANE_Device **device;
static GSGDialog *dialog;
static char settings_filename[1024] = "settings.dat";

static struct
  {
    GtkWidget *shell;
    GtkWidget *dialog_box;
    GtkWidget *play_stop_label;
    GtkWidget *device_info_label;
    struct
      {
	GtkWidget *item;	/* the menu bar item */
	GtkWidget *menu;	/* the associated menu */
      }
    devices;
    Canvas canvas;
    gint gdk_input_tag;		/* tag returned by gdk_input_add () */
    int playing;		/* are we playing video? */

    SANE_Byte buf[32768];
    SANE_Int remaining;
    SANE_Parameters params;
    gpointer data;	/* image data */
    int x;		/* x position */
  }
win;

/* forward declarations: */
static void rescan_devices (GtkWidget *widget, gpointer client_data,
			    gpointer call_data);
static void next_frame (void);


#define CANVAS_EVENT_MASK	GDK_BUTTON1_MOTION_MASK |	\
				GDK_EXPOSURE_MASK |		\
				GDK_BUTTON_PRESS_MASK |		\
				GDK_ENTER_NOTIFY_MASK

static void
display_image (Canvas *canvas)
{
  if (canvas->gdk_image)
    {
      gdk_draw_image (canvas->preview->window, canvas->gc, canvas->gdk_image,
		      0, 0, 0, 0,
		      canvas->gdk_image->width, canvas->gdk_image->height);
      gdk_flush ();
    }
}

static gint
canvas_events (GtkWidget *widget, GdkEvent *event)
{
  Canvas *canvas = &win.canvas;
  if (!canvas)
    return FALSE;

  switch (event->type)
    {
    case GDK_EXPOSE:
      if (!canvas->gc)
	canvas->gc = gdk_gc_new (canvas->preview->window);
      display_image (canvas);
      break;

    case GDK_BUTTON_PRESS:
      break;

    case GDK_BUTTON_RELEASE:
      break;

    case GDK_MOTION_NOTIFY:
      break;

    case GDK_ENTER_NOTIFY:
#if 0
      gdk_colors_store (win.canvas.cube_cmap, win.canvas.cube_colors,
			NELEMS(win.canvas.cube_colors));
#endif
      break;

    default:
      break;
    }
  return FALSE;
}

static void
stop_camera (void)
{
  if (dialog)
    sane_cancel (gsg_dialog_get_device (dialog));
  if (win.gdk_input_tag >= 0)
    gdk_input_remove (win.gdk_input_tag);
  else
    win.playing = FALSE;
  win.gdk_input_tag = -1;
  if (!win.playing)
    gtk_label_set (GTK_LABEL (win.play_stop_label), "Play");
}

static void
switch_device (const SANE_Device *dev)
{
  char buf[512];

  if (win.playing)
    {
      win.playing = FALSE;
      stop_camera ();
    }

  if (dialog)
    gsg_destroy_dialog (dialog);

  dialog = gsg_create_dialog (GTK_WIDGET (win.dialog_box), dev->name,
			      0, 0, 0, 0);
  buf[0] = '\0';
  if (dialog)
    sprintf (buf, "%s %s %s", dev->vendor, dev->model, dev->type);
  gtk_label_set (GTK_LABEL (win.device_info_label), buf);
}

static void
switch_device_by_name (const char *device_name)
{
  SANE_Device dev_info;
  int i;

  for (i = 0; device[i]; ++i)
    if (strcmp (device[i]->name, device_name) == 0)
      {
	switch_device (device[i]);
	return;
      }

  /* the backends don't know about this device yet---make up an entry: */
  dev_info.name   = device_name;
  dev_info.vendor = "Unknown";
  dev_info.model  = "";
  dev_info.type   = "";
  switch_device (&dev_info);
}

static int
make_default_filename (size_t buf_size, char *buf, const char *dev_name)
{
  return gsg_make_path (buf_size, buf, "xcam", 0, dev_name, ".rc");
}

static void
save_settings (const char *filename)
{
  int fd;

  fd = open (filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if (fd < 0)
    {
      char buf[256];

      snprintf (buf, sizeof (buf), "Failed to create file: %s.",
		strerror (errno));
      gsg_error (buf);
      return;
    }
  write (fd, dialog->dev_name, strlen (dialog->dev_name));
  write (fd, "\n", 1);
  sanei_save_values (fd, dialog->dev);
  close (fd);
}

static void
load_settings (const char *filename, int silent)
{
  char buf[2*PATH_MAX];
  char *end;
  int fd;

  fd = open (filename, O_RDONLY);
  if (fd < 0)
    {
      if (!silent)
	{
	  snprintf (buf, sizeof (buf), "Failed to open file %s: %s.",
		    filename, strerror (errno));
	  gsg_error (buf);
	}
      return;	/* fail silently */
    }

  /* first, read off the devicename that these settings are for: */
  read (fd, buf, sizeof (buf));
  buf[sizeof (buf) - 1] = '\0';
  end = strchr (buf, '\n');
  if (!end)
    {
      if (!silent)
	{
	  snprintf (buf, sizeof (buf), "File %s is malformed.", filename);
	  gsg_error (buf);
	}
      return;
    }
  *end = '\0';
  if (strcmp (dialog->dev_name, buf) != 0)
    switch_device_by_name (buf);

  /* position right behind device name: */
  lseek (fd, strlen (buf) + 1, SEEK_SET);

  sanei_load_values (fd, dialog->dev);
  close (fd);

  gsg_refresh_dialog (dialog);
}

static void
load_defaults (int silent)
{
  char filename[PATH_MAX];

  if (make_default_filename (sizeof (filename), filename, dialog->dev_name)
      < 0)
    return;
  load_settings (filename, silent);
}

void
device_name_dialog_cancel (GtkWidget *widget, gpointer data)
{
  gtk_widget_destroy (data);
}

void
device_name_dialog_ok (GtkWidget *widget, gpointer data)
{
  GtkWidget *text = data;
  const char *name;

  name = gtk_entry_get_text (GTK_ENTRY (text));
  if (!name)
    return;				/* huh? how come? */
  switch_device_by_name (name);

  gtk_widget_destroy (gtk_widget_get_toplevel (text));
}

static void
prompt_for_device_name (GtkWidget *widget, gpointer data)
{
  GtkWidget *vbox, *hbox, *label, *text;
  GtkWidget *button, *dialog;

  dialog = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);
  gtk_window_set_title (GTK_WINDOW (dialog), "Device name");

  /* create the main vbox */
  vbox = gtk_vbox_new (TRUE, 5);
  gtk_container_border_width (GTK_CONTAINER (vbox), 5);
  gtk_container_add (GTK_CONTAINER (dialog), vbox);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (vbox), hbox);

  label = gtk_label_new ("Device name:");
  gtk_container_add (GTK_CONTAINER (hbox), label);
  gtk_widget_show (label);

  text = gtk_entry_new ();
  gtk_container_add (GTK_CONTAINER (hbox), text);

  gtk_widget_show (hbox);

  /* the confirmation button */
  hbox = gtk_hbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (vbox), hbox);

  button = gtk_button_new_with_label ("OK");
  GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
  gtk_signal_connect (GTK_OBJECT (button), "clicked",
		      (GtkSignalFunc) device_name_dialog_ok, text);
  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 5);
  gtk_widget_grab_default (button);
  gtk_widget_show (button);

  button = gtk_button_new_with_label ("Cancel");
  gtk_signal_connect (GTK_OBJECT (button), "clicked",
		      (GtkSignalFunc) device_name_dialog_cancel, dialog);
  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 5);
  gtk_widget_show (button);

  gtk_widget_show (hbox);
  gtk_widget_show (text);
  gtk_widget_show (vbox);
  gtk_widget_show (dialog);
}

static void
exit_callback (GtkWidget *widget, gpointer data)
{
  if (dialog)
    gsg_destroy_dialog (dialog);
  dialog = 0;
  exit (0);
}

static void
save_defaults_callback (GtkWidget *widget, gpointer data)
{
  char buf[PATH_MAX];

  if (make_default_filename (sizeof (buf), buf, dialog->dev_name) < 0)
    return;
  save_settings (buf);
}

static void
load_defaults_callback (GtkWidget *widget, gpointer data)
{
  load_defaults (0);
}

static void
save_as_callback (GtkWidget *widget, gpointer data)
{
  if (gsg_get_filename ("File to save settings to", settings_filename,
			sizeof (settings_filename), settings_filename) < 0)
    return;
  save_settings (settings_filename);
}

static void
load_from_callback (GtkWidget *widget, gpointer data)
{
  if (gsg_get_filename ("File to load settings from", settings_filename,
			sizeof (settings_filename), settings_filename) < 0)
    return;
  load_settings (settings_filename, 0);
}

static GtkWidget *
build_files_menu (void)
{
  GtkWidget *menu, *item;

  menu = gtk_menu_new ();

  item = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menu), item);
  gtk_widget_show (item);

  item = gtk_menu_item_new_with_label ("Exit");
  gtk_container_add (GTK_CONTAINER (menu), item);
  gtk_signal_connect (GTK_OBJECT (item), "activate",
		      (GtkSignalFunc) exit_callback, 0);
  gtk_widget_show (item);

  return menu;
}

static gint
delayed_switch (gpointer data)
{
  switch_device (data);
  load_defaults (1);
  return FALSE;
}

static void
device_activate_callback (GtkWidget *widget, gpointer data)
{
  gtk_idle_add (delayed_switch, data);
}

static GtkWidget *
build_device_menu (void)
{
  GtkWidget *menu, *item;
  SANE_Status result;
  int i;

  menu = gtk_menu_new ();

  result = sane_get_devices (&device, SANE_FALSE);
  if (result != SANE_STATUS_GOOD)
    {
      fprintf (stderr, "%s: %s\n", prog_name, sane_strstatus (result));
      exit (1);
    }
	
  for (i = 0; device[i]; ++i)
    {
      item = gtk_menu_item_new_with_label ((char *) device[i]->name);
      gtk_container_add (GTK_CONTAINER (menu), item);
      gtk_signal_connect (GTK_OBJECT (item), "activate",
			  (GtkSignalFunc) device_activate_callback,
			  (gpointer) device[i]);
      gtk_widget_show (item);
    }

  item = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menu), item);
  gtk_widget_show (item);

  item = gtk_menu_item_new_with_label ("Refresh device list...");
  gtk_signal_connect (GTK_OBJECT (item), "activate",
		      (GtkSignalFunc) rescan_devices, 0);
  gtk_container_add (GTK_CONTAINER (menu), item);
  gtk_widget_show (item);

  item = gtk_menu_item_new_with_label ("Specify device name...");
  gtk_signal_connect (GTK_OBJECT (item), "activate",
		      (GtkSignalFunc) prompt_for_device_name, 0);
  gtk_container_add (GTK_CONTAINER (menu), item);
  gtk_widget_show (item);

  return menu;
}

static void
pref_toggle_advanced (GtkWidget *widget, gpointer data)
{
  preferences.advanced = (GTK_CHECK_MENU_ITEM (widget)->active != 0);
  gsg_set_advanced (dialog, preferences.advanced);
}

static void
pref_toggle_tooltips (GtkWidget *widget, gpointer data)
{
  preferences.tooltips_enabled = (GTK_CHECK_MENU_ITEM (widget)->active != 0);
  gsg_set_tooltips (dialog, preferences.tooltips_enabled);
}

static GtkWidget *
build_preferences_menu (GSGDialog *dialog)
{
  GtkWidget *menu, *item;

  menu = gtk_menu_new ();

  /* advanced user option: */
  item = gtk_check_menu_item_new_with_label ("Show advanced options");
  gtk_check_menu_item_set_state (GTK_CHECK_MENU_ITEM (item),
				 preferences.advanced);
  gtk_menu_append (GTK_MENU (menu), item);
  gtk_widget_show (item);
  gtk_signal_connect (GTK_OBJECT (item), "toggled",
		      (GtkSignalFunc) pref_toggle_advanced, 0);

  /* tooltips submenu: */

  item = gtk_check_menu_item_new_with_label ("Show tooltips");
  gtk_check_menu_item_set_state (GTK_CHECK_MENU_ITEM (item),
				 preferences.tooltips_enabled);
  gtk_menu_append (GTK_MENU (menu), item);
  gtk_widget_show (item);
  gtk_signal_connect (GTK_OBJECT (item), "toggled",
		      (GtkSignalFunc) pref_toggle_tooltips, 0);

  item = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menu), item);
  gtk_widget_show (item);

  item = gtk_menu_item_new_with_label ("Save as default settings");
  gtk_container_add (GTK_CONTAINER (menu), item);
  gtk_signal_connect (GTK_OBJECT (item), "activate",
		      (GtkSignalFunc) save_defaults_callback, 0);
  gtk_widget_show (item);

  item = gtk_menu_item_new_with_label ("Load default settings");
  gtk_container_add (GTK_CONTAINER (menu), item);
  gtk_signal_connect (GTK_OBJECT (item), "activate",
		      (GtkSignalFunc) load_defaults_callback, 0);
  gtk_widget_show (item);

  item = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menu), item);
  gtk_widget_show (item);

  item = gtk_menu_item_new_with_label ("Save settings as...");
  gtk_container_add (GTK_CONTAINER (menu), item);
  gtk_signal_connect (GTK_OBJECT (item), "activate",
		      (GtkSignalFunc) save_as_callback, 0);
  gtk_widget_show (item);

  item = gtk_menu_item_new_with_label ("Load settings from...");
  gtk_container_add (GTK_CONTAINER (menu), item);
  gtk_signal_connect (GTK_OBJECT (item), "activate",
		      (GtkSignalFunc) load_from_callback, 0);
  gtk_widget_show (item);

  return menu;
}

static void
rescan_devices (GtkWidget *widget, gpointer client_data, gpointer call_data)
{
  gtk_widget_destroy (GTK_WIDGET (win.devices.menu));
  win.devices.menu = build_device_menu ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (win.devices.item),
			     win.devices.menu);
}

#define READ_SANE_PIXEL(buf, buf_end, format, depth, r, g, b)		\
{									\
  switch (format)							\
    {									\
    case SANE_FRAME_GRAY:						\
      switch (depth)							\
	{								\
	case 1:								\
	  if (buf + 1 > buf_end)					\
	    goto end_of_buffer;						\
	  (r) = (g) = (b) = (*buf & src_mask) ? 0x0000 : 0xffff;	\
	  src_mask >>= 1;						\
	  if (src_mask == 0x00)						\
	    {								\
	      ++buf;							\
	      src_mask = 0x80;						\
	    }								\
	  break;							\
									\
	case 8:								\
	  if (buf + 1 > buf_end)					\
	    goto end_of_buffer;						\
	  (r) = (g) = (b) = (*buf++ << 8);				\
	  break;							\
									\
	case 16:							\
	  if (buf + 2 > buf_end)					\
	    goto end_of_buffer;						\
	  (r) = (g) = (b) = *((guint16 *) buf);			\
	  buf += 2;							\
	  break;							\
	}								\
      break;								\
									\
    case SANE_FRAME_RGB:						\
      switch (depth)							\
	{								\
	case 1:								\
	case 8:								\
	  if (buf + 3 > buf_end)					\
	    goto end_of_buffer;						\
	  (r) = buf[0] << 8; (g) = buf[1] << 8; (b) = buf[2] << 8;	\
	  buf += 3;							\
	  break;							\
									\
	case 16:							\
	  if (buf + 3 > buf_end)					\
	    goto end_of_buffer;						\
	  (r) = ((guint16 *)buf)[0];					\
	  (g) = ((guint16 *)buf)[1];					\
	  (b) = ((guint16 *)buf)[2];					\
	  buf += 6;							\
	  break;							\
	}								\
      break;								\
									\
    case SANE_FRAME_RED:						\
    case SANE_FRAME_GREEN:						\
    case SANE_FRAME_BLUE:						\
    default:								\
      fprintf (stderr, "%s: format %d not yet supported\n",		\
	       prog_name, (format));					\
      goto end_of_buffer;						\
    }									\
}

#define PUT_X11_PIXEL(buf, endian, depth, bpp, r, g, b, gl_map)		\
{									\
  switch (depth)							\
    {									\
    case  1: /* duh?  A Sun3 or what?? */				\
      lum = 3*(r) + 5*(g) + 2*(b);					\
      if (lum >= 5*0x8000)						\
	*buf |= dst_mask;						\
      dst_mask <<= 1;							\
      if (dst_mask > 0xff)						\
	{								\
	  buf += (bpp);							\
	  dst_mask = 0x01;						\
	}								\
      break;								\
									\
    case  8:								\
      lum = ((3*(r) + 5*(g) + 2*(b)) / (10 * 256/MAX_LUM)) >> 8;	\
      if (lum >= MAX_LUM)						\
	lum = MAX_LUM;							\
      buf[0] = (gl_map)[lum];						\
      buf += (bpp);							\
      break;								\
									\
    case 15:								\
      rgb = (  (((r) >> 11) << 10)	/* 5 bits of red */		\
	     | (((g) >> 11) <<  5)	/* 5 bits of green */		\
	     | (((b) >> 11) <<  0));	/* 5 bits of blue */		\
      ((guint16 *)buf)[0] = rgb;					\
      buf += (bpp);							\
      break;								\
									\
    case 16:								\
      rgb = (  (((r) >> 11) << 11)	/* 5 bits of red */		\
	     | (((g) >> 10) <<  5)	/* 6 bits of green */		\
	     | (((b) >> 11) <<  0));	/* 5 bits of blue */		\
      ((guint16 *)buf)[0] = rgb;					\
      buf += (bpp);							\
      break;								\
									\
    case 24:								\
      /* Is this correctly handling all byte order cases? */		\
      if ((endian) == GDK_LSB_FIRST)					\
	{								\
	  buf[0] = (b) >> 8; buf[1] = (g) >> 8; buf[2] = (r) >> 8;	\
	}								\
      else								\
	{								\
	  buf[0] = (r) >> 8; buf[1] = (g) >> 8; buf[2] = (b) >> 8;	\
	}								\
      buf += (bpp);							\
      break;								\
									\
    case 32:								\
      /* Is this correctly handling all byte order cases?  It assumes	\
	 the byte order of the host is the same as that of the		\
	 pixmap. */							\
      rgb = (((r) >> 8) << 16) | (((g) >> 8) << 8) | ((b) >> 8);	\
      ((guint32 *)buf)[0] = rgb;					\
      buf += (bpp);							\
      break;								\
    }									\
}

static void
input_available (gpointer ignore, gint source, GdkInputCondition cond)
{
  int x, pixels_per_line, bytes_per_line, dst_depth, src_depth;
  guint32 r = 0, g = 0, b = 0, lum, rgb, src_mask, dst_mask;
  size_t buf_size, remaining = win.remaining;
  SANE_Byte *src, *src_end;
  GdkByteOrder byte_order;
  u_long bytes_per_pixel;
  SANE_Frame format;
  SANE_Status result;
  SANE_Int len;
  u_char *dst;

  if (!win.playing)
    /* looks like we got cancelled */
    goto stop_and_exit;

  buf_size = sizeof (win.buf);
  format = win.params.format;
  src_depth = win.params.depth;
  dst_depth = win.canvas.gdk_image->depth;
  pixels_per_line = win.params.pixels_per_line;
  bytes_per_line = win.canvas.gdk_image->bpl;
  bytes_per_pixel = win.canvas.gdk_image->bpp;
  byte_order = win.canvas.gdk_image->byte_order;

  x = win.x;
  dst = win.data;
  src_mask = 0x80;	/* SANE has left most bit is most significant bit */
  dst_mask = 0x01;

  while (1)
    {
      result = sane_read (gsg_dialog_get_device (dialog),
			  win.buf + remaining, buf_size - remaining, &len);
      if (result != SANE_STATUS_GOOD)
	{
	  if (result == SANE_STATUS_EOF)
	    {
	      display_image (&win.canvas);
	      stop_camera ();
	      if (win.playing)
		{
		  next_frame ();		/* arrange for next frame */
		  return;
		}
	    }
	  else
	    {
	      char buf[256];
	      sprintf (buf, "Error during read: %s.", sane_strstatus (result));
	      gsg_error (buf);
	    }
	  win.playing = FALSE;
	  stop_camera ();
	  return;
	}
      if (!len)
	break;

      src = win.buf;
      src_end = src + len + remaining;
      while (1)
	{
	  READ_SANE_PIXEL(src, src_end, format, src_depth, r, g, b);
	  PUT_X11_PIXEL(dst, byte_order, dst_depth, bytes_per_pixel, r, g, b,
			win.canvas.graylevel);
	  if (++x >= pixels_per_line)
	    {
	      x = 0;
	      dst += bytes_per_line - pixels_per_line * bytes_per_pixel;
	    }
	}
    end_of_buffer:
      remaining = src_end - src;
    }
  win.data = dst;
  win.x = x;
  win.remaining = remaining;
  return;

stop_and_exit:
  win.playing = FALSE;
  stop_camera ();
  return;
}

static void
next_frame (void)
{
  char buf[256];
  SANE_Status result;
  int fd;

  gsg_sync (dialog);

  result = sane_start (gsg_dialog_get_device (dialog));
  if (result != SANE_STATUS_GOOD)
    {
      sprintf (buf, "Failed to start scanner: %s.", sane_strstatus (result));
      gsg_error (buf);
      win.playing = FALSE;
      stop_camera ();
      return;
    }

  result = sane_get_parameters (gsg_dialog_get_device (dialog), &win.params);
  if (result != SANE_STATUS_GOOD)
    {
      sprintf (buf, "Failed to get parameters: %s.", sane_strstatus (result));
      gsg_error (buf);
      win.playing = FALSE;
      stop_camera ();
      return;
    }

  if (!win.canvas.gdk_image
      || win.canvas.gdk_image->width != win.params.pixels_per_line
      || win.canvas.gdk_image->height != win.params.lines)
    {
      GdkImageType image_type = GDK_IMAGE_FASTEST;

      if (win.canvas.gdk_image)
	gdk_image_destroy (win.canvas.gdk_image);
#ifdef __alpha__
      /* Some X servers seem to have a problem with shared images that
	 have a width that is not a multiple of 8.  Duh... ;-( */
      if (win.params.pixels_per_line % 8)
	image_type = GDK_IMAGE_NORMAL;
#endif
      win.canvas.gdk_image =
	  gdk_image_new (image_type,
			 gdk_window_get_visual (win.canvas.preview->window),
			 win.params.pixels_per_line,
			 win.params.lines);
      gtk_widget_set_usize (win.canvas.preview,
			    win.params.pixels_per_line, win.params.lines);
    }

  win.data = win.canvas.gdk_image->mem;
  win.x = 0;
  win.remaining = 0;

  if (sane_set_io_mode (gsg_dialog_get_device (dialog), SANE_TRUE)
      == SANE_STATUS_GOOD
      && sane_get_select_fd (gsg_dialog_get_device (dialog), &fd)
      == SANE_STATUS_GOOD)
    win.gdk_input_tag = gdk_input_add (fd, GDK_INPUT_READ, input_available, 0);
  else
    input_available (0, -1, 0);
}

static void
play_stop_button (GtkWidget *widget, gpointer client_data, gpointer call_data)
{
  if (!dialog)
    return;

  if (win.playing)
    win.playing = FALSE;
  else if (win.gdk_input_tag < 0)
    {
      win.playing = TRUE;
      gtk_label_set (GTK_LABEL (win.play_stop_label), "Stop");

      next_frame ();
    }
}

static void
xcam_exit (void)
{
  static int active = 0;

  if (active)
    return;

  active = 1;
  sane_exit ();
  /* this has the habit of calling exit itself: */
  gtk_exit (0);
}

int
main (int argc, char **argv)
{
  GtkWidget *menu, *menu_bar, *menu_bar_item, *preview_vbox;
  GtkWidget *hbox, *vbox, *button, *alignment, *frame;
  int i;

  prog_name = strrchr (argv[0], '/');
  if (prog_name)
    ++prog_name;
  else
    prog_name = argv[0];

  /* turn on by default as we don't support graphical geometry selection */
  preferences.advanced = 1;

  sane_init (NULL, 0);

  gdk_set_show_events (0);
  gtk_init (&argc, &argv);

  atexit (xcam_exit);

  win.gdk_input_tag = -1;
  win.shell = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (win.shell), (char *) prog_name);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_border_width (GTK_CONTAINER (vbox), 0);      
  gtk_container_add (GTK_CONTAINER (win.shell), vbox);
  gtk_widget_show (vbox);

  menu_bar = gtk_menu_bar_new ();

  win.devices.menu = build_device_menu ();

  /* "Files" entry: */
  menu_bar_item = gtk_menu_item_new_with_label ("File");
  gtk_container_add (GTK_CONTAINER (menu_bar), menu_bar_item);
  menu = build_files_menu ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_bar_item), menu);
  gtk_widget_show (menu_bar_item);

  /* "Devices" entry: */
  win.devices.item = gtk_menu_item_new_with_label ("Devices");
  gtk_container_add (GTK_CONTAINER (menu_bar), win.devices.item);
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (win.devices.item),
			     win.devices.menu);
  gtk_widget_show (win.devices.item);

  /* "Preferences" entry: */
  menu_bar_item = gtk_menu_item_new_with_label ("Preferences");
  gtk_container_add (GTK_CONTAINER (menu_bar), menu_bar_item);
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_bar_item),
			     build_preferences_menu (dialog));
  gtk_widget_show (menu_bar_item);

  gtk_container_add (GTK_CONTAINER (vbox), menu_bar);
  gtk_widget_show (menu_bar);

  /* add device info at top: */
  frame = gtk_frame_new (0);
  gtk_container_border_width (GTK_CONTAINER (frame), 8);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_ETCHED_IN);
  gtk_container_add (GTK_CONTAINER (vbox), frame);
  gtk_widget_show (frame);

  win.device_info_label = gtk_label_new ("");
  gtk_widget_show (win.device_info_label);
  gtk_container_add (GTK_CONTAINER (frame), win.device_info_label);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (vbox), hbox);
  gtk_widget_show (hbox);

  /* create the device dialog box: */
  win.dialog_box = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (win.dialog_box);

  /* the preview vbox on the left hand side: */
  preview_vbox = gtk_vbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), preview_vbox, TRUE, TRUE, 0);

  frame = gtk_frame_new ("Preview");
  gtk_container_border_width (GTK_CONTAINER (frame), 8);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_ETCHED_IN);
  gtk_container_add (GTK_CONTAINER (preview_vbox), frame);

  alignment = gtk_alignment_new (0, 0.5, 1.0, 1.0);
  gtk_box_pack_start (GTK_BOX (preview_vbox), alignment, TRUE, TRUE, 0);

  win.canvas.preview = gtk_drawing_area_new ();
  gtk_drawing_area_size (GTK_DRAWING_AREA (win.canvas.preview),
			 320, 200);
  gtk_widget_set_events (win.canvas.preview, CANVAS_EVENT_MASK);
  gtk_signal_connect (GTK_OBJECT (win.canvas.preview), "event",
		      (GtkSignalFunc) canvas_events, 0);
  gtk_container_add (GTK_CONTAINER (frame), win.canvas.preview);

  gtk_widget_show (win.canvas.preview);
  gtk_widget_show (alignment);
  gtk_widget_show (frame);
  gtk_widget_show (preview_vbox);

  win.canvas.graylevel_cmap = gdk_colormap_get_system ();
  for (i = 0; i < NELEMS(win.canvas.graylevel); ++i)
    {
      GdkColor color;
      color.red = color.green = color.blue =
	i * 0xffff / (NELEMS(win.canvas.graylevel) - 1);
      gdk_color_alloc (win.canvas.graylevel_cmap, &color);
      win.canvas.graylevel[i] = color.pixel;
    }

#if 0
  {
      win.canvas.cube_cmap
	  = gdk_colormap_new (win.canvas.preview->window->visual, 1);
      for (i = 0; i < NELEMS(win.canvas.cube_colors); ++i)
	{
	  win.canvas.cube_colors[i].pixel = i;
	  win.canvas.cube_colors[i].red   = ((i / 30) % 5) * 0xffff / 4;
	  win.canvas.cube_colors[i].green = ((i / 5) % 6)  * 0xffff / 5;
	  win.canvas.cube_colors[i].blue  = ((i % 5))      * 0xffff / 4;
	}
      gdk_colors_store (win.canvas.cube_cmap, win.canvas.cube_colors,
			NELEMS(win.canvas.cube_colors));
      gdk_window_set_colormap (win.shell->window, win.canvas.cube_cmap);
  }
#endif

  gtk_container_add (GTK_CONTAINER (hbox), win.dialog_box);
  if (device[0])
    {
      switch_device (device[0]);
      load_defaults (1);
    }

  if (dialog && gsg_dialog_get_device (dialog)
      && (sane_get_parameters (gsg_dialog_get_device (dialog), &win.params)
	  == SANE_STATUS_GOOD))
    gtk_widget_set_usize (win.canvas.preview,
			  win.params.pixels_per_line, win.params.lines);

  /* buttons at the bottom: */
  button = gtk_button_new ();
  gtk_signal_connect (GTK_OBJECT (button), "clicked",
		      (GtkSignalFunc) play_stop_button, dialog);
  win.play_stop_label = gtk_label_new ("Play");
  gtk_container_add (GTK_CONTAINER (button), win.play_stop_label);
  gtk_container_add (GTK_CONTAINER (vbox), button);
  gtk_widget_show (win.play_stop_label);
  gtk_widget_show (button);

  gtk_widget_show (win.shell);
  gtk_main ();

  return 0;
}
