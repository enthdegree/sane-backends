/* gtk/SANE-glue -- gtk interfacing routines for SANE
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

#ifdef _AIX
# include <lalloca.h>	/* MUST come first for AIX! */
#endif

#include <sane/config.h>
#include <lalloca.h>

#include <assert.h>
#include <errno.h>
#include <memory.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>	/* Apollo/DomainOS needs this _before_ sys/stat.h */
#include <sys/stat.h>

#include <gtkglue.h>

#include <preferences.h>
#include <sane/sane.h>
#include <sane/saneopts.h>

int gsg_message_dialog_active = 0;

/* forward declarations: */
static void panel_rebuild (GSGDialog * dialog);

static const char *
unit_string (SANE_Unit unit)
{
  double d;

  switch (unit)
    {
    case SANE_UNIT_NONE:	return "none";
    case SANE_UNIT_PIXEL:	return "pixel";
    case SANE_UNIT_BIT:		return "bit";
    case SANE_UNIT_DPI:		return "dpi";
    case SANE_UNIT_PERCENT:	return "%";
    case SANE_UNIT_MM:
      d = preferences.length_unit;
      if (d > 9.9 && d < 10.1)
	return "cm";
      else if (d > 25.3 && d < 25.5)
	return "in";
      return "mm";
    case SANE_UNIT_MICROSECOND:	return "\265s";
    }
  return 0;
}

static void
set_tooltip (GtkTooltips *tooltips, GtkWidget *widget, const char *desc)
{
  if (desc && desc[0])
#ifdef HAVE_GTK_TOOLTIPS_SET_TIPS
    /* pre 0.99.4: */
    gtk_tooltips_set_tips (tooltips, widget, (char *) desc);
#else
    gtk_tooltips_set_tip (tooltips, widget, desc, 0);
#endif
}

int
gsg_make_path (size_t buf_size, char *buf,
	       const char *prog_name,
	       const char *prefix, const char *dev_name,
	       const char *postfix)
{
  struct passwd *pw;
  size_t len, extra;
  int i;

  /* first, make sure ~/.sane exists: */
  pw = getpwuid (getuid ());
  if (!pw)
    {
      snprintf (buf, buf_size, "Failed to determine home directory: %s.",
		strerror (errno));
      gsg_error (buf);
      return -1;
    }
  snprintf (buf, buf_size, "%s/.sane", pw->pw_dir);
  mkdir (buf, 0777);	/* ensure ~/.sane directory exists */

  len = strlen (buf);

  if (prog_name)
    {
      extra = strlen (prog_name);
      if (len + extra + 1 >= buf_size)
	goto filename_too_long;

      buf[len++] = '/';
      memcpy (buf + len, prog_name, extra);
      len += extra;
      buf[len] = '\0';
      mkdir (buf, 0777);	/* ensure ~/.sane/PROG_NAME directory exists */
    }
  if (len >= buf_size)
    goto filename_too_long;

  buf[len++] = '/';

  if (prefix)
    {
      extra = strlen (prefix);
      if (len + extra >= buf_size)
	goto filename_too_long;

      memcpy (buf + len, prefix, extra);
      len += extra;
    }

  if (dev_name)
    {
      /* Turn devicename into valid filename by replacing slashes by
	 "+-".  A lonely `+' gets translated into "++" so we can tell
	 it from a substituted slash.  */

      for (i = 0; dev_name[i]; ++i)
	{
	  if (len + 2 >= buf_size)
	    goto filename_too_long;

	  switch (dev_name[i])
	    {
	    case '/':
	      buf[len++] = '+';
	      buf[len++] = '-';
	      break;

	    case '+':
	      buf[len++] = '+';
	    default:
	      buf[len++] = dev_name[i];
	      break;
	    }
	}
    }

  if (postfix)
    {
      extra = strlen (postfix);
      if (len + extra >= buf_size)
	goto filename_too_long;
      memcpy (buf + len, postfix, extra);
      len += extra;
    }
  if (len >= buf_size)
    goto filename_too_long;

  buf[len++] = '\0';
  return 0;

filename_too_long:
  gsg_error ("Filename too long.");
  errno = E2BIG;
  return -1;
}

static void
set_option (GSGDialog * dialog, int opt_num, void *val, SANE_Action action)
{
  SANE_Status status;
  SANE_Int info;
  char buf[256];

  status = sane_control_option (dialog->dev, opt_num, action, val, &info);
  if (status != SANE_STATUS_GOOD)
    {
      snprintf (buf, sizeof (buf), "Failed to set value of option %s: %s.",
	       sane_get_option_descriptor (dialog->dev, opt_num)->name,
	       sane_strstatus (status));
      gsg_error (buf);
      return;
    }
  if (info & SANE_INFO_RELOAD_OPTIONS)
    {
      panel_rebuild (dialog);
      if (dialog->option_reload_callback)
	(*dialog->option_reload_callback) (dialog, dialog->option_reload_arg);
    }
  if ((info & SANE_INFO_RELOAD_PARAMS) && dialog->param_change_callback)
    (*dialog->param_change_callback) (dialog, dialog->param_change_arg);
}

void
gsg_close_dialog_callback (GtkWidget * widget, gpointer data)
{
  gtk_widget_destroy (data);
  gsg_message_dialog_active = 0;
}

void
gsg_message (gchar *title, gchar *message)
{
  GtkWidget *main_vbox, *label;
  GtkWidget *button, *message_dialog;

  if (gsg_message_dialog_active)
    {
      fprintf (stderr, "%s: %s\n", title, message);
      return;
    }
  gsg_message_dialog_active = 1;
  message_dialog = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_position (GTK_WINDOW (message_dialog), GTK_WIN_POS_MOUSE);
  gtk_window_set_title (GTK_WINDOW (message_dialog), title);

  /* create the main vbox */
  main_vbox = gtk_vbox_new (TRUE, 5);
  gtk_container_border_width (GTK_CONTAINER (main_vbox), 5);
  gtk_widget_show (main_vbox);

  gtk_container_add (GTK_CONTAINER (message_dialog), main_vbox);

  /* the message */
  label = gtk_label_new (message);
  gtk_container_add (GTK_CONTAINER (main_vbox), label);
  gtk_widget_show (label);

  /* the confirmation button */
  button = gtk_button_new_with_label ("OK");
  gtk_signal_connect (GTK_OBJECT (button), "clicked",
		      (GtkSignalFunc) gsg_close_dialog_callback,
		      message_dialog);
  gtk_container_add (GTK_CONTAINER (main_vbox), button);

  gtk_widget_show (button);
  gtk_widget_show (message_dialog);
}

void
gsg_error (gchar * error)
{
  gsg_message ("Error", error);
}

void
gsg_warning (gchar * warning)
{
  gsg_message ("Warning", warning);
}

static void
get_filename_button_clicked (GtkWidget *w, gpointer data)
{
  int *clicked = data;
  *clicked = 1;
}

int
gsg_get_filename (const char *label, const char *default_name,
		  size_t max_len, char *filename)
{
  int cancel = 0, ok = 0;
  GtkWidget *filesel;

  filesel = gtk_file_selection_new ((char *) label);
  gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (filesel)->cancel_button),
		      "clicked", (GtkSignalFunc) get_filename_button_clicked,
		      &cancel);
  gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (filesel)->ok_button),
		      "clicked", (GtkSignalFunc) get_filename_button_clicked,
		      &ok);
  if (default_name)
    gtk_file_selection_set_filename (GTK_FILE_SELECTION (filesel),
				     (char *) default_name);

  gtk_widget_show (filesel);

  while (!cancel && !ok)
    {
      if (!gtk_events_pending ())
	usleep (100000);
      gtk_main_iteration ();
    }

  if (ok)
    {
      size_t len, cwd_len;
      char *cwd;

      strncpy (filename,
	       gtk_file_selection_get_filename (GTK_FILE_SELECTION (filesel)),
	       max_len - 1);
      filename[max_len - 1] = '\0';

      len = strlen (filename);
      cwd = alloca (len + 2);
      getcwd (cwd, len + 1);
      cwd_len = strlen (cwd);
      cwd[cwd_len++] = '/';
      cwd[cwd_len] = '\0';
      if (strncmp (filename, cwd, cwd_len) == 0)
	memcpy (filename, filename + cwd_len, len - cwd_len + 1);
    }
  gtk_widget_destroy (filesel);
  return cancel ? -1 : 0;
}

static gint
autobutton_update (GtkWidget * widget, GSGDialogElement * elem)
{
  GSGDialog *dialog = elem->dialog;
  int opt_num = elem - dialog->element;
  const SANE_Option_Descriptor *opt;
  SANE_Status status;
  SANE_Word val;
  char buf[256];

  opt = sane_get_option_descriptor (dialog->dev, opt_num);
  if (GTK_TOGGLE_BUTTON (widget)->active)
    set_option (dialog, opt_num, 0, SANE_ACTION_SET_AUTO);
  else
    {
      status = sane_control_option (dialog->dev, opt_num,
				    SANE_ACTION_GET_VALUE, &val, 0);
      if (status != SANE_STATUS_GOOD)
	{
	  snprintf (buf, sizeof (buf),
		    "Failed to obtain value of option %s: %s.",
		    opt->name, sane_strstatus (status));
	  gsg_error (buf);
	}
      set_option (dialog, opt_num, &val, SANE_ACTION_SET_VALUE);
    }
  return FALSE;
}

static void
autobutton_new (GtkWidget *parent, GSGDialogElement *elem,
		GtkWidget *label, GtkTooltips *tooltips)
{
  GtkWidget *button, *alignment;

  button = gtk_check_button_new ();
  gtk_container_border_width (GTK_CONTAINER (button), 0);
  gtk_widget_set_usize (button, 20, 20);
  gtk_signal_connect (GTK_OBJECT (button), "toggled",
		      (GtkSignalFunc) autobutton_update,
		      elem);
  set_tooltip (tooltips, button, "Turns on automatic mode.");

  alignment = gtk_alignment_new (0.0, 1.0, 0.5, 0.5);
  gtk_container_add (GTK_CONTAINER (alignment), button);

  gtk_box_pack_end (GTK_BOX (parent), label, FALSE, FALSE, 0);
  gtk_box_pack_end (GTK_BOX (parent), alignment, FALSE, FALSE, 2);

  gtk_widget_show (alignment);
  gtk_widget_show (button);
}

static gint
button_update (GtkWidget * widget, GSGDialogElement * elem)
{
  GSGDialog *dialog = elem->dialog;
  int opt_num = elem - dialog->element;
  const SANE_Option_Descriptor *opt;
  SANE_Word val = SANE_FALSE;

  opt = sane_get_option_descriptor (dialog->dev, opt_num);
  if (GTK_TOGGLE_BUTTON (widget)->active)
    val = SANE_TRUE;
  set_option (dialog, opt_num, &val, SANE_ACTION_SET_VALUE);
  return FALSE;
}

static void
button_new (GtkWidget * parent, const char *name, SANE_Word val,
	    GSGDialogElement * elem, GtkTooltips *tooltips, const char *desc)
{
  GtkWidget *button;

  button = gtk_check_button_new_with_label ((char *) name);
  gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (button), val);
  gtk_signal_connect (GTK_OBJECT (button), "toggled",
		      (GtkSignalFunc) button_update,
		      elem);
  gtk_box_pack_start (GTK_BOX (parent), button, FALSE, TRUE, 0);
  gtk_widget_show (button);
  set_tooltip (tooltips, button, desc);

  elem->widget = button;
}

static void
scale_update (GtkAdjustment * adj_data, GSGDialogElement * elem)
{
  const SANE_Option_Descriptor *opt;
  GSGDialog *dialog = elem->dialog;
  SANE_Word val, new_val;
  int opt_num;
  double d;

  opt_num = elem - dialog->element;
  opt = sane_get_option_descriptor (dialog->dev, opt_num);
  switch (opt->type)
    {
    case SANE_TYPE_INT:
      val = adj_data->value + 0.5;
      break;

    case SANE_TYPE_FIXED:
      d = adj_data->value;
      if (opt->unit == SANE_UNIT_MM)
	d *= preferences.length_unit;
      val = SANE_FIX (d);
      break;

    default:
      fprintf (stderr, "scale_update: unknown type %d\n", opt->type);
      return;
    }
  set_option (dialog, opt_num, &val, SANE_ACTION_SET_VALUE);
  sane_control_option (dialog->dev, opt_num, SANE_ACTION_GET_VALUE, &new_val,
		       0);
  if (new_val != val)
    {
      val = new_val;
      goto value_changed;
    }
  return;			/* value didn't change */

value_changed:
  switch (opt->type)
    {
    case SANE_TYPE_INT:
      adj_data->value = val;
      break;

    case SANE_TYPE_FIXED:
      d = SANE_UNFIX (val);
      if (opt->unit == SANE_UNIT_MM)
	d /= preferences.length_unit;
      adj_data->value = d;
      break;

    default:
      break;
    }
  /* Let widget know that value changed _again_.  This must converge
     quickly---otherwise things would get very slow very quickly (as
     in "infinite recursion"): */
  gtk_signal_emit_by_name (GTK_OBJECT (adj_data), "value_changed");
  return;
}

static void
scale_new (GtkWidget * parent, const char *name, gfloat val,
	   gfloat min, gfloat max, gfloat quant, int automatic,
	   GSGDialogElement * elem, GtkTooltips *tooltips, const char *desc)
{
  GtkWidget *hbox, *label, *scale;

  hbox = gtk_hbox_new (FALSE, 2);
  gtk_container_border_width (GTK_CONTAINER (hbox), 0);
  gtk_box_pack_start (GTK_BOX (parent), hbox, FALSE, FALSE, 0);

  label = gtk_label_new ((char *) name);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 2);

  elem->data = gtk_adjustment_new (val, min, max, quant, 1.0, 0.0);
  scale = gtk_hscale_new (GTK_ADJUSTMENT (elem->data));
  set_tooltip (tooltips, scale, desc);
  gtk_widget_set_usize (scale, 200, 0);

  if (automatic)
    autobutton_new (hbox, elem, scale, tooltips);
  else
    gtk_box_pack_end (GTK_BOX (hbox), scale, FALSE, FALSE, 0);

  gtk_range_set_update_policy (GTK_RANGE (scale), GTK_UPDATE_CONTINUOUS);
  gtk_scale_set_value_pos (GTK_SCALE (scale), GTK_POS_TOP);
  if (quant - (int) quant == 0.0)
    gtk_scale_set_digits (GTK_SCALE (scale), 0);
  else
    /* one place behind decimal point */
    gtk_scale_set_digits (GTK_SCALE (scale), 1);

  gtk_signal_connect (elem->data, "value_changed",
		      (GtkSignalFunc) scale_update, elem);

  gtk_widget_show (label);
  gtk_widget_show (scale);
  gtk_widget_show (hbox);

  elem->widget = scale;
}

static void
push_button_callback (GtkWidget * widget, gpointer data)
{
  GSGDialogElement *elem = data;
  GSGDialog *dialog = elem->dialog;
  int opt_num;

  opt_num = elem - dialog->element;
  set_option (dialog, opt_num, 0, SANE_ACTION_SET_VALUE);
}

static int
option_menu_lookup (GSGMenuItem menu_items[], const char *string)
{
  int i;

  for (i = 0; strcmp (menu_items[i].label, string) != 0; ++i);
  return i;
}

static void
option_menu_callback (GtkWidget * widget, gpointer data)
{
  GSGMenuItem *menu_item = data;
  GSGDialogElement *elem = menu_item->elem;
  const SANE_Option_Descriptor *opt;
  GSGDialog *dialog = elem->dialog;
  int opt_num;
  double dval;
  SANE_Word val;
  void *valp = &val;

  opt_num = elem - dialog->element;
  opt = sane_get_option_descriptor (dialog->dev, opt_num);
  switch (opt->type)
    {
    case SANE_TYPE_INT:
      sscanf (menu_item->label, "%d", &val);
      break;

    case SANE_TYPE_FIXED:
      sscanf (menu_item->label, "%lg", &dval);
      val = SANE_FIX (dval);
      break;

    case SANE_TYPE_STRING:
      valp = menu_item->label;
      break;

    default:
      fprintf (stderr, "option_menu_callback: unexpected type %d\n",
	       opt->type);
      break;
    }
  set_option (dialog, opt_num, valp, SANE_ACTION_SET_VALUE);
}

static void
option_menu_new (GtkWidget *parent, const char *name, char *str_list[],
		 const char *val, GSGDialogElement * elem,
		 GtkTooltips *tooltips, const char *desc)
{
  GtkWidget *hbox, *label, *option_menu, *menu, *item;
  GSGMenuItem *menu_items;
  int i, num_items;

  hbox = gtk_hbox_new (FALSE, 2);
  gtk_container_border_width (GTK_CONTAINER (hbox), 0);
  gtk_box_pack_start (GTK_BOX (parent), hbox, FALSE, FALSE, 0);

  label = gtk_label_new ((char *) name);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 2);

  for (num_items = 0; str_list[num_items]; ++num_items);
  menu_items = malloc (num_items * sizeof (menu_items[0]));

  menu = gtk_menu_new ();
  for (i = 0; i < num_items; ++i)
    {
      item = gtk_menu_item_new_with_label (str_list[i]);
      gtk_container_add (GTK_CONTAINER (menu), item);
      gtk_signal_connect (GTK_OBJECT (item), "activate",
			  (GtkSignalFunc) option_menu_callback,
			  menu_items + i);

      gtk_widget_show (item);

      menu_items[i].label = str_list[i];
      menu_items[i].elem = elem;
      menu_items[i].index = i;
    }

  option_menu = gtk_option_menu_new ();
  gtk_box_pack_end (GTK_BOX (hbox), option_menu, FALSE, FALSE, 2);
  gtk_option_menu_set_menu (GTK_OPTION_MENU (option_menu), menu);
  gtk_option_menu_set_history (GTK_OPTION_MENU (option_menu),
			       option_menu_lookup (menu_items, val));
  set_tooltip (tooltips, option_menu, desc);

  gtk_widget_show (label);
  gtk_widget_show (option_menu);
  gtk_widget_show (hbox);

  elem->widget = option_menu;
  elem->menu_size = num_items;
  elem->menu = menu_items;
}

static void
text_entry_callback (GtkWidget *w, gpointer data)
{
  GSGDialogElement *elem = data;
  const SANE_Option_Descriptor *opt;
  GSGDialog *dialog = elem->dialog;
  gchar *text;
  int opt_num;
  char *buf;

  opt_num = elem - dialog->element;
  opt = sane_get_option_descriptor (dialog->dev, opt_num);

  buf = alloca (opt->size);
  buf[0] = '\0';

  text = gtk_entry_get_text (GTK_ENTRY (elem->widget));
  if (text)
    strncpy (buf, text, opt->size);
  buf[opt->size - 1] = '\0';

  set_option (dialog, opt_num, buf, SANE_ACTION_SET_VALUE);

  if (strcmp (buf, text) != 0)
    /* the backend modified the option value; update widget: */
    gtk_entry_set_text (GTK_ENTRY (elem->widget), buf);
}

static void
text_entry_new (GtkWidget * parent, const char *name, const char *val,
		GSGDialogElement * elem,
		GtkTooltips *tooltips, const char *desc)
{
  GtkWidget *hbox, *text, *label;

  hbox = gtk_hbox_new (FALSE, 2);
  gtk_container_border_width (GTK_CONTAINER (hbox), 0);
  gtk_box_pack_start (GTK_BOX (parent), hbox, FALSE, FALSE, 0);

  label = gtk_label_new ((char *) name);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 2);

  text = gtk_entry_new ();
  gtk_entry_set_text (GTK_ENTRY (text), (char *) val);
  gtk_box_pack_start (GTK_BOX (hbox), text, FALSE, TRUE, 0);
  gtk_signal_connect (GTK_OBJECT (text), "changed",
		      (GtkSignalFunc) text_entry_callback, elem);
  set_tooltip (tooltips, text, desc);

  gtk_widget_show (hbox);
  gtk_widget_show (label);
  gtk_widget_show (text);

  elem->widget = text;
}

static GtkWidget *
group_new (GtkWidget *parent, const char * title)
{
  GtkWidget * frame, * vbox;

  frame = gtk_frame_new ((char *) title);
  gtk_container_border_width (GTK_CONTAINER (frame), 4);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_ETCHED_IN);
  gtk_box_pack_start (GTK_BOX (parent), frame, FALSE, FALSE, 0);

  vbox = gtk_vbox_new (FALSE, 4);
  gtk_container_border_width (GTK_CONTAINER (vbox), 2);
  gtk_container_add (GTK_CONTAINER (frame), vbox);
  gtk_widget_show (vbox);
  return vbox;
}

static GtkWidget*
curve_new (GSGDialog *dialog, int optnum)
{
  const SANE_Option_Descriptor * opt;
  gfloat fmin, fmax, val, *vector;
  SANE_Word *optval, min, max;
  GtkWidget *curve, *gamma;
  SANE_Status status;
  SANE_Handle dev;
  int i, optlen;

  gamma = gtk_gamma_curve_new ();
  curve = GTK_GAMMA_CURVE (gamma)->curve;
  dev = dialog->dev;

  opt    = sane_get_option_descriptor (dev, optnum);
  optlen = opt->size / sizeof (SANE_Word);
  vector = alloca (optlen * (sizeof (vector[0]) + sizeof (optval[0])));
  optval = (SANE_Word *) (vector + optlen);

  min = max = 0;
  switch (opt->constraint_type)
    {
    case SANE_CONSTRAINT_RANGE:
      min = opt->constraint.range->min;
      max = opt->constraint.range->max;
      break;

    case SANE_CONSTRAINT_WORD_LIST:
      if (opt->constraint.word_list[0] > 1)
	{
	  min = max = opt->constraint.word_list[1];
	  for (i = 2; i < opt->constraint.word_list[0]; ++i)
	    {
	      if (opt->constraint.word_list[i] < min)
		min = opt->constraint.word_list[i];
	      if (opt->constraint.word_list[i] > max)
		max = opt->constraint.word_list[i];
	    }
	}
      break;

    default:
      break;
    }
  if (min == max)
    {
      fprintf (stderr,
	       "curve_new: warning: option `%s' has no value constraint\n",
	       opt->name);
      fmin = 0;
      fmax = 255;
    }
  else if (opt->type == SANE_TYPE_FIXED)
    {
      fmin = SANE_UNFIX (min);
      fmax = SANE_UNFIX (max);
    }
  else
    {
      fmin = min;
      fmax = max;
    }
  gtk_curve_set_range (GTK_CURVE (curve), 0, optlen - 1, fmin, fmax);

  status = sane_control_option (dev, optnum, SANE_ACTION_GET_VALUE,
				optval, 0);
  if (status == SANE_STATUS_GOOD)
    {
      for (i = 0; i < optlen; ++i)
	{
	  if (opt->type == SANE_TYPE_FIXED)
	    val = SANE_UNFIX (optval[i]);
	  else
	    val = optval[i];
	  vector[i] = val;
	}
      gtk_curve_set_vector (GTK_CURVE (curve), optlen, vector);
    }
  else
    gtk_widget_set_sensitive (gamma, FALSE);

  return gamma;
}

static void
vector_new (GSGDialog * dialog, GtkWidget *vbox, int num_vopts, int *vopts)
{
  GtkWidget *notebook, *label, *curve;
  const SANE_Option_Descriptor *opt;
  int i;

  notebook = gtk_notebook_new ();
  gtk_container_border_width (GTK_CONTAINER (notebook), 4);
  gtk_box_pack_start (GTK_BOX (vbox), notebook, TRUE, TRUE, 0);

  for (i = 0; i < num_vopts; ++i)
    {
      opt = sane_get_option_descriptor (dialog->dev, vopts[i]);

      label = gtk_label_new ((char *) opt->title);
      vbox = gtk_vbox_new (/* homogeneous */ FALSE, 0);
      gtk_notebook_append_page (GTK_NOTEBOOK (notebook), vbox, label);
      gtk_widget_show (vbox);
      gtk_widget_show (label);

      curve = curve_new (dialog, vopts[i]);
      gtk_container_border_width (GTK_CONTAINER (curve), 4);
      gtk_box_pack_start (GTK_BOX (vbox), curve, TRUE, TRUE, 0);
      gtk_widget_show (curve);

      dialog->element[vopts[i]].widget = curve;
    }
  gtk_widget_show (notebook);
}

static void
panel_destroy (GSGDialog * dialog)
{
  const SANE_Option_Descriptor *opt;
  GSGDialogElement *elem;
  int i, j;

#ifdef HAVE_GTK_TOOLTIPS_SET_TIPS
  /* pre 0.99.4: */
  gtk_tooltips_unref (dialog->tooltips);
#else
  gtk_object_unref (GTK_OBJECT (dialog->tooltips));
#endif
  gtk_widget_destroy (dialog->main_hbox);

  /* free the menu labels of integer/fix-point word-lists: */
  for (i = 0; i < dialog->num_elements; ++i)
    {
      if (dialog->element[i].menu)
	{
	  opt = sane_get_option_descriptor (dialog->dev, i);
	  elem = dialog->element + i;
	  if (opt->type != SANE_TYPE_STRING)
	    for (j = 0; j < elem->menu_size; ++j)
	      if (elem->menu[j].label)
		{
		  free (elem->menu[j].label);
		  elem->menu[j].label = 0;
		}
	  free (elem->menu);
	  elem->menu = 0;
	}
    }
  memset (dialog->element, 0,
	  dialog->num_elements*sizeof (dialog->element[0]));
}

static void
panel_build (GSGDialog * dialog)
{
  GtkWidget *main_hbox, *standard_vbox, *advanced_vbox, *option_vbox;
  GtkWidget *parent, *vbox, *button, *label;
  const SANE_Option_Descriptor *opt;
  SANE_Handle dev = dialog->dev;
  double dval, dmin, dmax, dquant;
  char *buf, str[16], title[256];
  GSGDialogElement *elem;
  SANE_Word quant, val;
  SANE_Status status;
  SANE_Int num_words;
  char **str_list;
  int i, j;
  int num_vector_opts = 0, *vector_opts;

  main_hbox = gtk_hbox_new (FALSE, 2);

  option_vbox = gtk_vbox_new (FALSE, 2);
  gtk_box_pack_start (GTK_BOX (main_hbox), option_vbox, FALSE, FALSE, 0);
  gtk_widget_show (option_vbox);

  /* standard options vbox */

  standard_vbox = gtk_vbox_new (/* homogeneous */ FALSE, 0);
  gtk_widget_show (standard_vbox);
  gtk_box_pack_start (GTK_BOX (option_vbox), standard_vbox, FALSE, FALSE, 0);

  /* advanced options page */

  advanced_vbox = gtk_vbox_new (/* homogeneous */ FALSE, 0);
  gtk_box_pack_start (GTK_BOX (option_vbox), advanced_vbox, TRUE, TRUE, 0);

  /* use black as foreground: */
  dialog->tooltips = gtk_tooltips_new ();
  dialog->tooltips_fg.red   = 0;
  dialog->tooltips_fg.green = 0;
  dialog->tooltips_fg.blue  = 0;
  /* postit yellow (khaki) as background: */
  gdk_color_alloc (gtk_widget_get_colormap (main_hbox), &dialog->tooltips_fg);
  dialog->tooltips_bg.red   = 61669;
  dialog->tooltips_bg.green = 59113;
  dialog->tooltips_bg.blue  = 35979;
  gdk_color_alloc (gtk_widget_get_colormap (main_hbox), &dialog->tooltips_bg);
  gtk_tooltips_set_colors (dialog->tooltips,
			   &dialog->tooltips_bg, &dialog->tooltips_fg);
  gsg_set_tooltips (dialog, preferences.tooltips_enabled);

  gtk_container_add (GTK_CONTAINER (dialog->window), main_hbox);
  dialog->main_hbox = main_hbox;
  dialog->advanced_vbox = advanced_vbox;

  /* reset well-known options: */
  dialog->well_known.preview = -1;
  dialog->well_known.dpi = -1;
  dialog->well_known.coord[GSG_TL_X] = -1;
  dialog->well_known.coord[GSG_TL_Y] = -1;
  dialog->well_known.coord[GSG_BR_X] = -1;
  dialog->well_known.coord[GSG_BR_Y] = -1;

  vector_opts = alloca (dialog->num_elements * sizeof (int));

  parent = standard_vbox;
  for (i = 1; i < dialog->num_elements; ++i)
    {
      opt = sane_get_option_descriptor (dev, i);
      if (!SANE_OPTION_IS_ACTIVE (opt->cap))
	continue;

      /* pick up well-known options as we go: */
      if (opt->name)
	{
	  if (strcmp (opt->name, SANE_NAME_PREVIEW) == 0
	      && opt->type == SANE_TYPE_BOOL)
	    {
	      dialog->well_known.preview = i;
	      continue;
	    }
	  else if (strcmp (opt->name, SANE_NAME_SCAN_RESOLUTION) == 0
		   && opt->unit == SANE_UNIT_DPI
		   && (opt->type == SANE_TYPE_INT
		       || opt->type == SANE_TYPE_FIXED))
	    dialog->well_known.dpi = i;
	  else if (strcmp (opt->name, SANE_NAME_SCAN_TL_X) == 0)
	    dialog->well_known.coord[GSG_TL_X] = i;
	  else if (strcmp (opt->name, SANE_NAME_SCAN_TL_Y) == 0)
	    dialog->well_known.coord[GSG_TL_Y] = i;
	  else if (strcmp (opt->name, SANE_NAME_SCAN_BR_X) == 0)
	    dialog->well_known.coord[GSG_BR_X] = i;
	  else if (strcmp (opt->name, SANE_NAME_SCAN_BR_Y) == 0)
	    dialog->well_known.coord[GSG_BR_Y] = i;
	}

      elem = dialog->element + i;
      elem->dialog = dialog;

      if (opt->unit == SANE_UNIT_NONE)
	strncpy (title, opt->title, sizeof (title));
      else
	snprintf (title, sizeof (title),
		  "%s [%s]", opt->title, unit_string (opt->unit));

      switch (opt->type)
	{
	case SANE_TYPE_GROUP:
	  /* group a set of options */
	  vbox = standard_vbox;
	  if (opt->cap & SANE_CAP_ADVANCED)
	    vbox = advanced_vbox;
	  parent = group_new (vbox, title);
	  elem->widget = parent;
	  break;

	case SANE_TYPE_BOOL:
	  if ((opt->cap & SANE_CAP_ADVANCED) && !dialog->advanced)
	    break;
	  assert (opt->size == sizeof (SANE_Word));
	  status = sane_control_option (dialog->dev, i, SANE_ACTION_GET_VALUE,
					&val, 0);
	  if (status != SANE_STATUS_GOOD)
	    goto get_value_failed;

	  button_new (parent, title, val, elem, dialog->tooltips, opt->desc);
	  gtk_widget_show (parent->parent);
	  break;

	case SANE_TYPE_INT:
	  if ((opt->cap & SANE_CAP_ADVANCED) && !dialog->advanced)
	    break;
	  if (opt->size != sizeof (SANE_Word))
	    {
	      vector_opts[num_vector_opts++] = i;
	      break;
	    }
	  status = sane_control_option (dialog->dev, i, SANE_ACTION_GET_VALUE,
					&val, 0);
	  if (status != SANE_STATUS_GOOD)
	    goto get_value_failed;

	  switch (opt->constraint_type)
	    {
	    case SANE_CONSTRAINT_RANGE:
	      /* use a scale */
	      quant = opt->constraint.range->quant;
	      if (quant == 0)
		quant = 1;
	      scale_new (parent, title, val,
			 opt->constraint.range->min,
			 opt->constraint.range->max, quant,
			 (opt->cap & SANE_CAP_AUTOMATIC), elem,
			 dialog->tooltips, opt->desc);
	      gtk_widget_show (parent->parent);
	      break;

	    case SANE_CONSTRAINT_WORD_LIST:
	      /* use a "list-selection" widget */
	      num_words = opt->constraint.word_list[0];
	      str_list = malloc ((num_words + 1) * sizeof (str_list[0]));
	      for (j = 0; j < num_words; ++j)
		{
		  sprintf (str, "%d", opt->constraint.word_list[j + 1]);
		  str_list[j] = strdup (str);
		}
	      str_list[j] = 0;
	      sprintf (str, "%d", val);
	      option_menu_new (parent, title, str_list, str, elem,
			       dialog->tooltips, opt->desc);
	      free (str_list);
	      gtk_widget_show (parent->parent);
	      break;

	    default:
	      fprintf (stderr, "panel_build: unknown constraint %d!\n",
		       opt->constraint_type);
	      break;
	    }
	  break;

	case SANE_TYPE_FIXED:
	  if ((opt->cap & SANE_CAP_ADVANCED) && !dialog->advanced)
	    break;
	  if (opt->size != sizeof (SANE_Word))
	    {
	      vector_opts[num_vector_opts++] = i;
	      break;
	    }
	  status = sane_control_option (dialog->dev, i, SANE_ACTION_GET_VALUE,
					&val, 0);
	  if (status != SANE_STATUS_GOOD)
	    goto get_value_failed;

	  switch (opt->constraint_type)
	    {
	    case SANE_CONSTRAINT_RANGE:
	      /* use a scale */
	      quant = opt->constraint.range->quant;
	      if (quant == 0)
		quant = 1;
	      dval = SANE_UNFIX (val);
	      dmin = SANE_UNFIX (opt->constraint.range->min);
	      dmax = SANE_UNFIX (opt->constraint.range->max);
	      dquant = SANE_UNFIX (quant);
	      if (opt->unit == SANE_UNIT_MM)
		{
		  dval /= preferences.length_unit;
		  dmin /= preferences.length_unit;
		  dmax /= preferences.length_unit;
		  dquant /= preferences.length_unit;
		}
	      scale_new (parent, title, dval, dmin, dmax, dquant,
			 (opt->cap & SANE_CAP_AUTOMATIC), elem,
			 dialog->tooltips, opt->desc);
	      gtk_widget_show (parent->parent);
	      break;

	    case SANE_CONSTRAINT_WORD_LIST:
	      /* use a "list-selection" widget */
	      num_words = opt->constraint.word_list[0];
	      str_list = malloc ((num_words + 1) * sizeof (str_list[0]));
	      for (j = 0; j < num_words; ++j)
		{
		  sprintf (str, "%g",
			   SANE_UNFIX (opt->constraint.word_list[j + 1]));
		  str_list[j] = strdup (str);
		}
	      str_list[j] = 0;
	      sprintf (str, "%g", SANE_UNFIX (val));
	      option_menu_new (parent, title, str_list, str, elem,
			       dialog->tooltips, opt->desc);
	      free (str_list);
	      gtk_widget_show (parent->parent);
	      break;

	    default:
	      fprintf (stderr, "panel_build: unknown constraint %d!\n",
		       opt->constraint_type);
	      break;
	    }
	  break;

	case SANE_TYPE_STRING:
	  if ((opt->cap & SANE_CAP_ADVANCED) && !dialog->advanced)
	    break;
	  buf = malloc (opt->size);
	  status = sane_control_option (dialog->dev, i, SANE_ACTION_GET_VALUE,
					buf, 0);
	  if (status != SANE_STATUS_GOOD)
	    {
	      free (buf);
	      goto get_value_failed;
	    }

	  switch (opt->constraint_type)
	    {
	    case SANE_CONSTRAINT_STRING_LIST:
	      /* use a "list-selection" widget */
	      option_menu_new (parent, title,
			       (char **) opt->constraint.string_list, buf,
			       elem, dialog->tooltips, opt->desc);
	      gtk_widget_show (parent->parent);
	      break;

	    case SANE_CONSTRAINT_NONE:
	      text_entry_new (parent, title, buf, elem,
			      dialog->tooltips, opt->desc);
	      gtk_widget_show (parent->parent);
	      break;

	    default:
	      fprintf (stderr, "panel_build: unknown constraint %d!\n",
		       opt->constraint_type);
	      break;
	    }
	  free (buf);
	  break;

	case SANE_TYPE_BUTTON:
	  if ((opt->cap & SANE_CAP_ADVANCED) && !dialog->advanced)
	    break;
	  button = gtk_button_new ();
	  gtk_signal_connect (GTK_OBJECT (button), "clicked",
			      (GtkSignalFunc) push_button_callback, elem);
	  set_tooltip (dialog->tooltips, button, opt->desc);

	  label = gtk_label_new (title);
	  gtk_container_add (GTK_CONTAINER (button), label);

	  gtk_box_pack_start (GTK_BOX (parent), button, FALSE, TRUE, 0);

	  gtk_widget_show (label);
	  gtk_widget_show (button);

	  elem->widget = button;
	  gtk_widget_show (parent->parent);
	  break;

	default:
	  fprintf (stderr, "panel_build: Unknown type %d\n", opt->type);
	  break;
	}
      continue;

    get_value_failed:
      {
	char msg[256];

	sprintf (msg, "Failed to obtain value of option %s: %s.",
		 opt->name, sane_strstatus (status));
	gsg_error (msg);
      }
    }

  /* now add in vector editor, if necessary: */

  if (num_vector_opts)
    vector_new (dialog, main_hbox, num_vector_opts, vector_opts);

  if (dialog->advanced)
    gtk_widget_show (dialog->advanced_vbox);
  else
    gtk_widget_hide (dialog->advanced_vbox);
  gtk_widget_show (main_hbox);
}

/* When an setting an option changes the dialog, everything may
   change: the option titles, the activity-status of the option, its
   constraints or what not.  Thus, rather than trying to be clever in
   detecting what exactly changed, we use a brute-force method of
   rebuilding the entire dialog.  */
static void
panel_rebuild (GSGDialog * dialog)
{
  panel_destroy (dialog);
  panel_build (dialog);
}

GSGDialog *
gsg_create_dialog (GtkWidget * window, const char *device_name,
		   GSGCallback option_reload_callback, void *option_reload_arg,
		   GSGCallback param_change_callback, void *param_change_arg)
{
  SANE_Int num_elements;
  GSGDialog *dialog;
  SANE_Status status;
  SANE_Handle dev;
  char buf[256];

  status = sane_open (device_name, &dev);
  if (status != SANE_STATUS_GOOD)
    {
      sprintf (buf, "Failed to open device `%s': %s.",
	       device_name, sane_strstatus (status));
      gsg_error (buf);
      return 0;
    }

  if (sane_control_option (dev, 0, SANE_ACTION_GET_VALUE, &num_elements, 0)
      != SANE_STATUS_GOOD)
    {
      gsg_error ("Error obtaining option count.");
      sane_close (dev);
      return 0;
    }

  dialog = malloc (sizeof (*dialog));
  memset (dialog, 0, sizeof (*dialog));

  dialog->window = window;
  dialog->dev = dev;
  dialog->dev_name = strdup (device_name);
  dialog->num_elements = num_elements;
  dialog->option_reload_callback = option_reload_callback;
  dialog->option_reload_arg = option_reload_arg;
  dialog->param_change_callback = param_change_callback;
  dialog->param_change_arg = param_change_arg;
  dialog->advanced = preferences.advanced;

  dialog->element = malloc (num_elements * sizeof (dialog->element[0]));
  memset (dialog->element, 0, num_elements * sizeof (dialog->element[0]));

  panel_build (dialog);
  return dialog;
}

void
gsg_refresh_dialog (GSGDialog *dialog)
{
  panel_rebuild (dialog);
  if (dialog->param_change_callback)
    (*dialog->param_change_callback) (dialog, dialog->param_change_arg);
}

void
gsg_update_scan_window (GSGDialog *dialog)
{
  const SANE_Option_Descriptor *opt;
  double old_val, new_val;
  GSGDialogElement *elem;
  SANE_Status status;
  SANE_Word word;
  int i, optnum;
  char str[64];

  for (i = 0; i < 4; ++i)
    if (dialog->well_known.coord[i] > 0)
      {
	optnum = dialog->well_known.coord[i];
	elem = dialog->element + optnum;
	opt = sane_get_option_descriptor (dialog->dev, optnum);

	status = sane_control_option (dialog->dev, optnum,
				      SANE_ACTION_GET_VALUE,
				      &word, 0);
	if (status != SANE_STATUS_GOOD)
	  continue;	/* sliently ignore errors */

	switch (opt->constraint_type)
	  {
	  case SANE_CONSTRAINT_RANGE:
	    if (opt->type == SANE_TYPE_INT)
	      {
		old_val = GTK_ADJUSTMENT (elem->data)->value;
		new_val = word;
		GTK_ADJUSTMENT (elem->data)->value = new_val;
	      }
	    else
	      {
		old_val = GTK_ADJUSTMENT (elem->data)->value;
		new_val = SANE_UNFIX (word);
		if (opt->unit == SANE_UNIT_MM)
		  new_val /= preferences.length_unit;
		GTK_ADJUSTMENT (elem->data)->value = new_val;
	      }
	    if (old_val != new_val)
	      gtk_signal_emit_by_name (GTK_OBJECT (elem->data),
				       "value_changed");
	    break;

	  case SANE_CONSTRAINT_WORD_LIST:
	    if (opt->type == SANE_TYPE_INT)
	      sprintf (str, "%d", word);
	    else
	      sprintf (str, "%g", SANE_UNFIX (word));
	    /* XXX maybe we should call this only when the value changes... */
	    gtk_option_menu_set_history (GTK_OPTION_MENU (elem->widget),
					 option_menu_lookup (elem->menu, str));
	    break;

	  default:
	    break;
	  }
      }
}

/* Ensure sure the device has up-to-date option values.  Except for
   vectors, all option values are kept current.  Vectors are
   downloaded into the device during this call.  */
void
gsg_sync (GSGDialog *dialog)
{
  const SANE_Option_Descriptor *opt;
  gfloat val, *vector;
  SANE_Word *optval;
  int i, j, optlen;
  GtkWidget *curve;

  for (i = 1; i < dialog->num_elements; ++i)
    {
      opt = sane_get_option_descriptor (dialog->dev, i);
      if (!SANE_OPTION_IS_ACTIVE (opt->cap))
	continue;

      if (opt->type != SANE_TYPE_INT &&
	  opt->type != SANE_TYPE_FIXED)
	continue;

      if (opt->size == sizeof (SANE_Word))
	continue;

      /* ok, we're dealing with an active vector */

      optlen = opt->size / sizeof (SANE_Word);
      optval = alloca (optlen * sizeof (optval[0]));
      vector = alloca (optlen * sizeof (vector[0]));

      curve = GTK_GAMMA_CURVE (dialog->element[i].widget)->curve;
      gtk_curve_get_vector (GTK_CURVE (curve), optlen, vector);
      for (j = 0; j < optlen; ++j)
	{
	  val = vector[j];
	  if (opt->type == SANE_TYPE_FIXED)
	    optval[j] = SANE_FIX (val);
	  else
	    optval[j] = val + 0.5;
	}

      set_option (dialog, i, optval, SANE_ACTION_SET_VALUE);
    }
}

void
gsg_set_advanced (GSGDialog *dialog, int advanced)
{
  dialog->advanced = advanced;
  panel_rebuild (dialog);
}

void
gsg_set_tooltips (GSGDialog *dialog, int enable)
{
  if (!dialog->tooltips)
    return;

  if (enable)
    gtk_tooltips_enable (dialog->tooltips);
  else
    gtk_tooltips_disable (dialog->tooltips);
}

void
gsg_set_sensitivity (GSGDialog *dialog, int sensitive)
{
  const SANE_Option_Descriptor *opt;
  int i;

  for (i = 0; i < dialog->num_elements; ++i)
    {
      opt = sane_get_option_descriptor (dialog->dev, i);

      if (!SANE_OPTION_IS_ACTIVE (opt->cap)
	  || opt->type == SANE_TYPE_GROUP
	  || !dialog->element[i].widget)
	continue;

      if (!(opt->cap & SANE_CAP_ALWAYS_SETTABLE))
	gtk_widget_set_sensitive (dialog->element[i].widget, sensitive);
    }
}

void
gsg_destroy_dialog (GSGDialog * dialog)
{
  SANE_Handle dev = dialog->dev;

  panel_destroy (dialog);
  free ((void *) dialog->dev_name);
  free (dialog->element);
  free (dialog);

  sane_close (dev);
}
