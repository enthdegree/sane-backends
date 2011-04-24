/* sane - Scanner Access Now Easy.

   Copyright (C) 2000-2005 Mustek.
   Originally maintained by Mustek

   Copyright (C) 2001-2005 by Henning Meier-Geinitz.

   This file is part of the SANE package.

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA.

   As a special exception, the authors of SANE give permission for
   additional uses of the libraries contained in this release of SANE.

   The exception is that, if you link a SANE library with other files
   to produce an executable, this does not by itself cause the
   resulting executable to be covered by the GNU General Public
   License.  Your use of that executable is in no way restricted on
   account of linking the SANE library code into it.

   This exception does not, however, invalidate any other reasons why
   the executable file might be covered by the GNU General Public
   License.

   If you submit changes to SANE to the maintainers to be included in
   a subsequent release, you agree by submitting the changes that
   those changes may be distributed with this exception intact.

   If you write modifications of your own for SANE, it is your choice
   whether to permit this exception to apply to your modifications.
   If you do not wish that, delete this exception notice.

   This file implements a SANE backend for the Mustek BearPaw 2448 TA Pro 
   and similar USB2 scanners. */

#define BUILD 11

#include "sane/config.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "sane/sane.h"
#include "sane/sanei.h"
#include "sane/saneopts.h"

#define BACKEND_NAME mustek_usb2
#include "sane/sanei_backend.h"

#include "mustek_usb2.h"


static const SANE_Device **devlist = NULL;

static const SANE_Range u8_range = {
  0,	/* minimum */
  255,	/* maximum */
  0	/* quantization */
};

static SANE_Range x_range = {
  SANE_FIX (0.0),
  SANE_FIX (8.3 * MM_PER_INCH),
  SANE_FIX (0.0)
};

static SANE_Range y_range = {
  SANE_FIX (0.0),
  SANE_FIX (11.6 * MM_PER_INCH),
  SANE_FIX (0.0)
};

static SANE_String_Const mode_list[] = {
  SANE_I18N ("Color48"),
  SANE_I18N ("Color24"),
  SANE_I18N ("Gray16"),
  SANE_I18N ("Gray8"),
  SANE_VALUE_SCAN_MODE_LINEART,
  NULL
};

static SANE_String_Const negative_mode_list[] = {
  SANE_I18N ("Color24"),
  NULL
};

static SANE_String_Const source_list[] = {
  SANE_I18N ("Reflective"),
  SANE_I18N ("Positive"),
  SANE_I18N ("Negative"),
  NULL
};

static const Scanner_Model mustek_A2nu2_model = {
  "Mustek", /* device vendor string */
  "BearPaw 2448TA Pro", /* device model name */

  {5 /* count */, 1200, 600, 300, 150, 75}, /* possible resolutions */

  SANE_FIX (8.5 * MM_PER_INCH),	/* size of scan area in mm (x) */
  SANE_FIX (11.8 * MM_PER_INCH), /* size of scan area in mm (y) */

  SANE_FIX (1.46 * MM_PER_INCH), /* size of scan area in TA mode in mm (x) */
  SANE_FIX (6.45 * MM_PER_INCH), /* size of scan area in TA mode in mm (y) */

  SANE_FALSE,  /* invert order of the CCD/CIS colors? */

  5,  /* number of buttons */

  /* button names */
  {SANE_NAME_SCAN, SANE_NAME_COPY, SANE_NAME_FAX, SANE_NAME_EMAIL,
   "panel"},

  /* button titles */
  {SANE_TITLE_SCAN, SANE_TITLE_COPY, SANE_TITLE_FAX, SANE_TITLE_EMAIL,
   "Panel button"},

  /* button descriptions */
  {SANE_DESC_SCAN, SANE_DESC_COPY, SANE_DESC_FAX, SANE_DESC_EMAIL,
   SANE_I18N ("Panel button")}
};


static void
calc_parameters (Mustek_Scanner * s, TARGETIMAGE * pTarget)
{
  SANE_String val;
  float x1, y1, x2, y2;
  DBG_ENTER ();

  if (s->val[OPT_PREVIEW].b)
    pTarget->wXDpi = 75;
  else
    pTarget->wXDpi = s->val[OPT_RESOLUTION].w;
  pTarget->wYDpi = pTarget->wXDpi;

  x1 = SANE_UNFIX (s->val[OPT_TL_X].w) / MM_PER_INCH;
  y1 = SANE_UNFIX (s->val[OPT_TL_Y].w) / MM_PER_INCH;
  x2 = SANE_UNFIX (s->val[OPT_BR_X].w) / MM_PER_INCH;
  y2 = SANE_UNFIX (s->val[OPT_BR_Y].w) / MM_PER_INCH;

  pTarget->wX = (unsigned short) ((x1 * pTarget->wXDpi) + 0.5);
  pTarget->wY = (unsigned short) ((y1 * pTarget->wYDpi) + 0.5);
  pTarget->wWidth = (unsigned short) (((x2 - x1) * pTarget->wXDpi) + 0.5);
  pTarget->wHeight = (unsigned short) (((y2 - y1) * pTarget->wYDpi) + 0.5);

  pTarget->wLineartThreshold = s->val[OPT_THRESHOLD].w;

  val = s->val[OPT_SOURCE].s;
  DBG (DBG_DET, "scan source = %s\n", val);
  if (strcmp (val, source_list[SS_POSITIVE]) == 0)
    pTarget->ssScanSource = SS_POSITIVE;
  else if (strcmp (val, source_list[SS_NEGATIVE]) == 0)
    pTarget->ssScanSource = SS_NEGATIVE;
  else
    pTarget->ssScanSource = SS_REFLECTIVE;

  val = s->val[OPT_MODE].s;
  if (strcmp (val, mode_list[CM_RGB48]) == 0)
    {
      if (s->val[OPT_PREVIEW].b)
	{
	  DBG (DBG_DET, "preview, set color mode CM_RGB24\n");
	  pTarget->cmColorMode = CM_RGB24;
	}
      else
	pTarget->cmColorMode = CM_RGB48;
    }
  else if (strcmp (val, mode_list[CM_RGB24]) == 0)
    {
      pTarget->cmColorMode = CM_RGB24;
    }
  else if (strcmp (val, mode_list[CM_GRAY16]) == 0)
    {
      if (s->val[OPT_PREVIEW].b)
	{
	  DBG (DBG_DET, "preview, set color mode CM_GRAY8\n");
	  pTarget->cmColorMode = CM_GRAY8;
	}
      else
	pTarget->cmColorMode = CM_GRAY16;
    }
  else if (strcmp (val, mode_list[CM_GRAY8]) == 0)
    {
      pTarget->cmColorMode = CM_GRAY8;
    }
  else
    {
      pTarget->cmColorMode = CM_TEXT;
    }

  s->params.pixels_per_line = pTarget->wWidth;
  s->params.lines = pTarget->wHeight;
  s->params.last_frame = SANE_TRUE;

  switch (pTarget->cmColorMode)
    {
    case CM_RGB48:
      s->params.format = SANE_FRAME_RGB;
      s->params.depth = 16;
      s->params.bytes_per_line = s->params.pixels_per_line * 6;
      break;
    case CM_RGB24:
      s->params.format = SANE_FRAME_RGB;
      s->params.depth = 8;
      s->params.bytes_per_line = s->params.pixels_per_line * 3;
      break;
    case CM_GRAY16:
      s->params.format = SANE_FRAME_GRAY;
      s->params.depth = 16;
      s->params.bytes_per_line = s->params.pixels_per_line * 2;
      break;
    case CM_GRAY8:
      s->params.format = SANE_FRAME_GRAY;
      s->params.depth = 8;
      s->params.bytes_per_line = s->params.pixels_per_line;
      break;
    case CM_TEXT:
      s->params.format = SANE_FRAME_GRAY;
      s->params.depth = 1;
      s->params.bytes_per_line = s->params.pixels_per_line / 8;
      break;
    }

  DBG_LEAVE ();
}

static void
update_button_status (Mustek_Scanner * s)
{
  SANE_Byte key;
  if (Scanner_GetKeyStatus (&s->state, &key) == SANE_STATUS_GOOD)
    s->val[OPT_BUTTON_1 + key].b = SANE_TRUE;
}

static size_t
max_string_size (SANE_String_Const *strings)
{
  size_t size, max_size = 0;
  SANE_Int i;

  for (i = 0; strings[i]; i++)
    {
      size = strlen (strings[i]) + 1;
      if (size > max_size)
	max_size = size;
    }
  return max_size;
}

static SANE_Status
init_options (Mustek_Scanner * s)
{
  SANE_Status status;
  SANE_Bool hasTA;
  SANE_Int i, option;
  TARGETIMAGE target;
  DBG_ENTER ();

  memset (s->opt, 0, sizeof (s->opt));
  memset (s->val, 0, sizeof (s->val));

  for (option = 0; option < NUM_OPTIONS; option++)
    s->opt[option].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;

  /* number of options */
  s->opt[OPT_NUM_OPTS].name = SANE_NAME_NUM_OPTIONS;
  s->opt[OPT_NUM_OPTS].title = SANE_TITLE_NUM_OPTIONS;
  s->opt[OPT_NUM_OPTS].desc = SANE_DESC_NUM_OPTIONS;
  s->opt[OPT_NUM_OPTS].type = SANE_TYPE_INT;
  s->opt[OPT_NUM_OPTS].size = sizeof (SANE_Word);
  s->opt[OPT_NUM_OPTS].cap = SANE_CAP_SOFT_DETECT;
  s->val[OPT_NUM_OPTS].w = NUM_OPTIONS;

  /* "standard" group */
  s->opt[OPT_MODE_GROUP].name = SANE_NAME_STANDARD;
  s->opt[OPT_MODE_GROUP].title = SANE_TITLE_STANDARD;
  s->opt[OPT_MODE_GROUP].desc = SANE_DESC_STANDARD;
  s->opt[OPT_MODE_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_MODE_GROUP].cap = 0;
  s->opt[OPT_MODE_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* scan mode */
  s->opt[OPT_MODE].name = SANE_NAME_SCAN_MODE;
  s->opt[OPT_MODE].title = SANE_TITLE_SCAN_MODE;
  s->opt[OPT_MODE].desc = SANE_DESC_SCAN_MODE;
  s->opt[OPT_MODE].type = SANE_TYPE_STRING;
  s->opt[OPT_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_MODE].size = max_string_size (mode_list);
  s->opt[OPT_MODE].constraint.string_list = mode_list;
  s->val[OPT_MODE].s = strdup (mode_list[CM_RGB24]);

  /* scan source */
  s->opt[OPT_SOURCE].name = SANE_NAME_SCAN_SOURCE;
  s->opt[OPT_SOURCE].title = SANE_TITLE_SCAN_SOURCE;
  s->opt[OPT_SOURCE].desc = SANE_DESC_SCAN_SOURCE;
  s->opt[OPT_SOURCE].type = SANE_TYPE_STRING;
  s->opt[OPT_SOURCE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_SOURCE].size = max_string_size (source_list);
  s->opt[OPT_SOURCE].constraint.string_list = source_list;
  s->val[OPT_SOURCE].s = strdup (source_list[SS_REFLECTIVE]);

  status = Scanner_IsTAConnected (&s->state, &hasTA);
  if ((status != SANE_STATUS_GOOD) || !hasTA)
    s->opt[OPT_SOURCE].cap |= SANE_CAP_INACTIVE;

  /* resolution */
  s->opt[OPT_RESOLUTION].name = SANE_NAME_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].type = SANE_TYPE_INT;
  s->opt[OPT_RESOLUTION].unit = SANE_UNIT_DPI;
  s->opt[OPT_RESOLUTION].size = sizeof (SANE_Word);
  s->opt[OPT_RESOLUTION].constraint_type = SANE_CONSTRAINT_WORD_LIST;
  s->opt[OPT_RESOLUTION].constraint.word_list = s->model.dpi_values;
  s->val[OPT_RESOLUTION].w = s->model.dpi_values[1];

  /* preview */
  s->opt[OPT_PREVIEW].name = SANE_NAME_PREVIEW;
  s->opt[OPT_PREVIEW].title = SANE_TITLE_PREVIEW;
  s->opt[OPT_PREVIEW].desc = SANE_DESC_PREVIEW;
  s->opt[OPT_PREVIEW].type = SANE_TYPE_BOOL;
  s->opt[OPT_PREVIEW].size = sizeof (SANE_Word);
  s->val[OPT_PREVIEW].b = SANE_FALSE;

  /* "enhancement" group */
  s->opt[OPT_ENHANCEMENT_GROUP].name = SANE_NAME_ENHANCEMENT;
  s->opt[OPT_ENHANCEMENT_GROUP].title = SANE_TITLE_ENHANCEMENT;
  s->opt[OPT_ENHANCEMENT_GROUP].desc = SANE_DESC_ENHANCEMENT;
  s->opt[OPT_ENHANCEMENT_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_ENHANCEMENT_GROUP].cap = 0;
  s->opt[OPT_ENHANCEMENT_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* threshold */
  s->opt[OPT_THRESHOLD].name = SANE_NAME_THRESHOLD;
  s->opt[OPT_THRESHOLD].title = SANE_TITLE_THRESHOLD;
  s->opt[OPT_THRESHOLD].desc = SANE_DESC_THRESHOLD;
  s->opt[OPT_THRESHOLD].type = SANE_TYPE_INT;
  s->opt[OPT_THRESHOLD].unit = SANE_UNIT_NONE;
  s->opt[OPT_THRESHOLD].size = sizeof (SANE_Word);
  s->opt[OPT_THRESHOLD].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_THRESHOLD].constraint.range = &u8_range;
  s->opt[OPT_THRESHOLD].cap |= SANE_CAP_INACTIVE;
  s->val[OPT_THRESHOLD].w = DEF_LINEARTTHRESHOLD;

  /* "geometry" group: */
  s->opt[OPT_GEOMETRY_GROUP].name = SANE_NAME_GEOMETRY;
  s->opt[OPT_GEOMETRY_GROUP].title = SANE_TITLE_GEOMETRY;
  s->opt[OPT_GEOMETRY_GROUP].desc = SANE_DESC_GEOMETRY;
  s->opt[OPT_GEOMETRY_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_GEOMETRY_GROUP].cap = SANE_CAP_ADVANCED;
  s->opt[OPT_GEOMETRY_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  x_range.max = s->model.x_size;
  y_range.max = s->model.y_size;

  /* top-left x */
  s->opt[OPT_TL_X].name = SANE_NAME_SCAN_TL_X;
  s->opt[OPT_TL_X].title = SANE_TITLE_SCAN_TL_X;
  s->opt[OPT_TL_X].desc = SANE_DESC_SCAN_TL_X;
  s->opt[OPT_TL_X].type = SANE_TYPE_FIXED;
  s->opt[OPT_TL_X].unit = SANE_UNIT_MM;
  s->opt[OPT_TL_X].size = sizeof (SANE_Word);
  s->opt[OPT_TL_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TL_X].constraint.range = &x_range;
  s->val[OPT_TL_X].w = 0;

  /* top-left y */
  s->opt[OPT_TL_Y].name = SANE_NAME_SCAN_TL_Y;
  s->opt[OPT_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
  s->opt[OPT_TL_Y].desc = SANE_DESC_SCAN_TL_Y;
  s->opt[OPT_TL_Y].type = SANE_TYPE_FIXED;
  s->opt[OPT_TL_Y].unit = SANE_UNIT_MM;
  s->opt[OPT_TL_Y].size = sizeof (SANE_Word);
  s->opt[OPT_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TL_Y].constraint.range = &y_range;
  s->val[OPT_TL_Y].w = 0;

  /* bottom-right x */
  s->opt[OPT_BR_X].name = SANE_NAME_SCAN_BR_X;
  s->opt[OPT_BR_X].title = SANE_TITLE_SCAN_BR_X;
  s->opt[OPT_BR_X].desc = SANE_DESC_SCAN_BR_X;
  s->opt[OPT_BR_X].type = SANE_TYPE_FIXED;
  s->opt[OPT_BR_X].unit = SANE_UNIT_MM;
  s->opt[OPT_BR_X].size = sizeof (SANE_Word);
  s->opt[OPT_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_X].constraint.range = &x_range;
  s->val[OPT_BR_X].w = x_range.max;

  /* bottom-right y */
  s->opt[OPT_BR_Y].name = SANE_NAME_SCAN_BR_Y;
  s->opt[OPT_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
  s->opt[OPT_BR_Y].desc = SANE_DESC_SCAN_BR_Y;
  s->opt[OPT_BR_Y].type = SANE_TYPE_FIXED;
  s->opt[OPT_BR_Y].unit = SANE_UNIT_MM;
  s->opt[OPT_BR_Y].size = sizeof (SANE_Word);
  s->opt[OPT_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_Y].constraint.range = &y_range;
  s->val[OPT_BR_Y].w = y_range.max;

  /* "sensors" group */
  s->opt[OPT_SENSORS_GROUP].title = SANE_TITLE_SENSORS;
  s->opt[OPT_SENSORS_GROUP].name = SANE_NAME_SENSORS;
  s->opt[OPT_SENSORS_GROUP].desc = SANE_DESC_SENSORS;
  s->opt[OPT_SENSORS_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_SENSORS_GROUP].cap = SANE_CAP_ADVANCED;
  s->opt[OPT_SENSORS_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* scanner buttons */
  for (i = OPT_BUTTON_1; i <= OPT_BUTTON_5; i++)
    {
      SANE_Int idx = i - OPT_BUTTON_1;
      if (idx < s->model.buttons)
	{
	  s->opt[i].name = s->model.button_name[idx];
	  s->opt[i].title = s->model.button_title[idx];
	  s->opt[i].desc = s->model.button_desc[idx];
	  s->opt[i].cap = SANE_CAP_HARD_SELECT | SANE_CAP_SOFT_DETECT |
			  SANE_CAP_ADVANCED;
	}
      else
	{
	  s->opt[i].name = SANE_I18N ("unused");
	  s->opt[i].title = SANE_I18N ("unused button");
	  s->opt[i].cap |= SANE_CAP_INACTIVE;
	}

      s->opt[i].type = SANE_TYPE_BOOL;
      s->opt[i].unit = SANE_UNIT_NONE;
      s->opt[i].size = sizeof (SANE_Word);
      s->opt[i].constraint_type = SANE_CONSTRAINT_NONE;
      s->val[i].b = SANE_FALSE;
    }

  calc_parameters (s, &target);
  update_button_status (s);

  DBG_LEAVE ();
  return SANE_STATUS_GOOD;
}


/****************************** SANE API functions ****************************/

SANE_Status
sane_init (SANE_Int * version_code,
	   SANE_Auth_Callback __sane_unused__ authorize)
{
  DBG_INIT ();
  DBG_ENTER ();
  DBG (DBG_ERR, "SANE Mustek USB2 backend version %d.%d build %d from %s\n",
       SANE_CURRENT_MAJOR, V_MINOR, BUILD, PACKAGE_STRING);

  if (version_code)
    *version_code = SANE_VERSION_CODE (SANE_CURRENT_MAJOR, V_MINOR, BUILD);

  DBG_LEAVE ();
  return SANE_STATUS_GOOD;
}

void
sane_exit (void)
{
  DBG_ENTER ();

  free (devlist);
  devlist = NULL;

  DBG_LEAVE ();
}

SANE_Status
sane_get_devices (const SANE_Device *** device_list,
		  SANE_Bool __sane_unused__ local_only)
{
  DBG_ENTER ();

  free (devlist);
  devlist = calloc (2, sizeof (devlist[0]));
  if (!devlist)
    return SANE_STATUS_NO_MEM;

  /* HOLD: This is ugly (only one scanner!) and should go to sane_init */
  if (Scanner_IsPresent ())
    {
      SANE_Device *sane_device = malloc (sizeof (*sane_device));
      if (!sane_device)
	return SANE_STATUS_NO_MEM;
      sane_device->name = strdup (device_name);
      sane_device->vendor = "Mustek";
      sane_device->model = "BearPaw 2448 TA Pro";
      sane_device->type = "flatbed scanner";
      devlist[0] = sane_device;
    }
  *device_list = devlist;

  DBG_LEAVE ();
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_open (SANE_String_Const devicename, SANE_Handle * handle)
{
  SANE_Status status;
  Mustek_Scanner *s;
  DBG_ENTER ();
  DBG (DBG_FUNC, "devicename=%s\n", devicename);

  s = malloc (sizeof (*s));
  if (!s)
    return SANE_STATUS_NO_MEM;
  memset (s, 0, sizeof (*s));
  s->model = mustek_A2nu2_model;

  Scanner_Init (&s->state);

  status = Scanner_PowerControl (&s->state, SANE_FALSE, SANE_FALSE);
  if (status != SANE_STATUS_GOOD)
    return status;
  status = Scanner_BackHome (&s->state);
  if (status != SANE_STATUS_GOOD)
    return status;

  init_options (s);
  *handle = s;

  DBG_LEAVE ();
  return SANE_STATUS_GOOD;
}

void
sane_close (SANE_Handle handle)
{
  Mustek_Scanner *s = handle;
  DBG_ENTER ();

  Scanner_PowerControl (&s->state, SANE_FALSE, SANE_FALSE);
  Scanner_BackHome (&s->state);

  free (s->scan_buf);
  s->scan_buf = NULL;

  free (s->val[OPT_MODE].s);
  free (s->val[OPT_SOURCE].s);

  free (handle);

  DBG_LEAVE ();
}

const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
  Mustek_Scanner *s = handle;

  if ((option >= NUM_OPTIONS) || (option < 0))
    return NULL;

  DBG (DBG_FUNC, "sane_get_option_descriptor: option = %s (%d)\n",
       s->opt[option].name, option);
  return &s->opt[option];
}

SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option,
		     SANE_Action action, void *val, SANE_Int * info)
{
  SANE_Status status;
  Mustek_Scanner *s = handle;
  SANE_Word cap;
  SANE_Int myinfo = 0;
  TARGETIMAGE target;
  DBG_ENTER ();
  DBG (DBG_FUNC, "action = %s, option = %s (%d)\n",
       (action == SANE_ACTION_GET_VALUE) ? "get" :
	 (action == SANE_ACTION_SET_VALUE) ? "set" :
	   (action == SANE_ACTION_SET_AUTO) ? "set_auto" : "unknown",
       s->opt[option].name, option);

  if (s->bIsScanning)
    {
      DBG (DBG_ERR, "scanner is busy\n");
      return SANE_STATUS_DEVICE_BUSY;
    }
  if ((option >= NUM_OPTIONS) || (option < 0))
    {
      DBG (DBG_ERR, "option index out of range\n");
      return SANE_STATUS_INVAL;
    }

  cap = s->opt[option].cap;
  if (!SANE_OPTION_IS_ACTIVE (cap))
    {
      DBG (DBG_ERR, "option %d is inactive\n", option);
      return SANE_STATUS_INVAL;
    }

  if (action == SANE_ACTION_GET_VALUE)
    {
      switch (option)
	{
	  /* word options: */
	case OPT_NUM_OPTS:
	case OPT_RESOLUTION:
	case OPT_THRESHOLD:
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
	  *(SANE_Word *) val = s->val[option].w;
	  break;
	  /* boolean options: */
	case OPT_PREVIEW:
	  *(SANE_Bool *) val = s->val[option].b;
	  break;
	  /* string options: */
	case OPT_MODE:
	case OPT_SOURCE:
	  strcpy (val, s->val[option].s);
	  break;
	  /* buttons: */
	case OPT_BUTTON_1:
	case OPT_BUTTON_2:
	case OPT_BUTTON_3:
	case OPT_BUTTON_4:
	case OPT_BUTTON_5:
	  update_button_status (s);
	  *(SANE_Bool *) val = s->val[option].b;
	  s->val[option].b = SANE_FALSE;
	  break;
	default:
	  DBG (DBG_ERR, "unknown option %d\n", option);
	}
    }
  else if (action == SANE_ACTION_SET_VALUE)
    {
      if (!SANE_OPTION_IS_SETTABLE (cap))
	{
	  DBG (DBG_ERR, "option %d is not settable\n", option);
	  return SANE_STATUS_INVAL;
	}

      status = sanei_constrain_value (s->opt + option, val, &myinfo);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_WARN, "sanei_constrain_value returned error: %s\n",
	       sane_strstatus (status));
	  return status;
	}

      switch (option)
	{
	  /* side-effect-free word options: */
	case OPT_THRESHOLD:
	  s->val[option].w = *(SANE_Word *) val;
	  break;
	  /* word options with side effects: */
	case OPT_RESOLUTION:
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
	  s->val[option].w = *(SANE_Word *) val;
	  calc_parameters (s, &target);
	  myinfo |= SANE_INFO_RELOAD_PARAMS;
	  break;
	  /* boolean options with side effects: */
	case OPT_PREVIEW:
	  s->val[option].b = *(SANE_Bool *) val;
	  calc_parameters (s, &target);
	  myinfo |= SANE_INFO_RELOAD_PARAMS;
	  break;
	  /* string array options with side effects: */
	case OPT_MODE:
	  free (s->val[option].s);
	  s->val[option].s = strdup (val);
	  if (strcmp (s->val[option].s, SANE_VALUE_SCAN_MODE_LINEART) == 0)
	    s->opt[OPT_THRESHOLD].cap &= ~SANE_CAP_INACTIVE;
	  else
	    s->opt[OPT_THRESHOLD].cap |= SANE_CAP_INACTIVE;
	  calc_parameters (s, &target);
	  myinfo |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
	  break;
	case OPT_SOURCE:
	  if (strcmp (s->val[option].s, val) != 0)
	    {	/* something changed */
	      free (s->val[option].s);
	      s->val[option].s = strdup (val);
	      if (strcmp (s->val[option].s, source_list[SS_REFLECTIVE]) == 0)
		{
		  s->opt[OPT_MODE].size = max_string_size (mode_list);
		  s->opt[OPT_MODE].constraint.string_list = mode_list;
		  s->val[OPT_MODE].s = strdup (mode_list[CM_RGB24]);
		  x_range.max = s->model.x_size;
		  y_range.max = s->model.y_size;
		}
	      else if (strcmp (s->val[option].s, source_list[SS_NEGATIVE]) == 0)
		{
		  s->opt[OPT_MODE].size = max_string_size (negative_mode_list);
		  s->opt[OPT_MODE].constraint.string_list = negative_mode_list;
		  s->val[OPT_MODE].s = strdup (mode_list[CM_RGB24]);
		  x_range.max = s->model.x_size_ta;
		  y_range.max = s->model.y_size_ta;
		}
	      else if (strcmp (s->val[option].s, source_list[SS_POSITIVE]) == 0)
		{
		  s->opt[OPT_MODE].size = max_string_size (mode_list);
		  s->opt[OPT_MODE].constraint.string_list = mode_list;
		  s->val[OPT_MODE].s = strdup (mode_list[CM_RGB24]);
		  x_range.max = s->model.x_size_ta;
		  y_range.max = s->model.y_size_ta;
		}
	    }
	  myinfo |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
	  break;
	default:
	  DBG (DBG_ERR, "unknown option %d\n", option);
	}

      if (info)
	*info = myinfo;
    }
  else
    {
      DBG (DBG_ERR, "unknown action %d for option %d\n", action, option);
      return SANE_STATUS_INVAL;
    }

  DBG_LEAVE ();
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters * params)
{
  Mustek_Scanner *s = handle;
  DBG_ENTER ();

  DBG (DBG_INFO, "params.format = %d\n", s->params.format);
  DBG (DBG_INFO, "params.depth = %d\n", s->params.depth);
  DBG (DBG_INFO, "params.pixels_per_line = %d\n", s->params.pixels_per_line);
  DBG (DBG_INFO, "params.bytes_per_line = %d\n", s->params.bytes_per_line);
  DBG (DBG_INFO, "params.lines = %d\n", s->params.lines);

  if (params)
    *params = s->params;

  DBG_LEAVE ();
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_start (SANE_Handle handle)
{
  SANE_Status status;
  Mustek_Scanner *s = handle;
  TARGETIMAGE target;
  DBG_ENTER ();
 
  if ((s->val[OPT_TL_X].w >= s->val[OPT_BR_X].w) ||
      (s->val[OPT_TL_Y].w >= s->val[OPT_BR_Y].w))
    {
      DBG (DBG_CRIT, "top left >= bottom right -- exiting\n");
      return SANE_FALSE;
    }
 
  calc_parameters (s, &target);
  DBG (DBG_INFO, "target.wX=%d\n", target.wX);
  DBG (DBG_INFO, "target.wY=%d\n", target.wY);
  DBG (DBG_INFO, "target.wWidth=%d\n", target.wWidth);
  DBG (DBG_INFO, "target.wHeight=%d\n", target.wHeight);
  DBG (DBG_INFO, "target.wLineartThreshold=%d\n", target.wLineartThreshold);
  DBG (DBG_INFO, "target.wXDpi=%d\n", target.wXDpi);
  DBG (DBG_INFO, "target.wYDpi=%d\n", target.wYDpi);
  DBG (DBG_INFO, "target.cmColorMode=%d\n", target.cmColorMode);
  DBG (DBG_INFO, "target.ssScanSource=%d\n", target.ssScanSource);

  s->read_rows = s->params.lines;
  DBG (DBG_INFO, "read_rows=%d\n", s->read_rows);

  DBG (DBG_INFO, "SCANNING...\n");
  s->bIsScanning = SANE_TRUE;

  free (s->scan_buf);
  s->scan_buf = malloc (SCAN_BUFFER_SIZE);
  if (!s->scan_buf)
    return SANE_STATUS_NO_MEM;
  s->scan_buf_len = 0;

  status = Scanner_Reset (&s->state);
  if (status != SANE_STATUS_GOOD)
    return status;

  if (target.ssScanSource == SS_REFLECTIVE)
    status = Scanner_PowerControl (&s->state, SANE_TRUE, SANE_FALSE);
  else
    status = Scanner_PowerControl (&s->state, SANE_FALSE, SANE_TRUE);
  if (status != SANE_STATUS_GOOD)
    return status;

  status = Scanner_SetupScan (&s->state, &target);

  DBG_LEAVE ();
  return status;
}

SANE_Status
sane_read (SANE_Handle handle, SANE_Byte * buf, SANE_Int max_len,
	   SANE_Int * len)
{
  SANE_Status status;
  Mustek_Scanner *s = handle;
  unsigned short lines;
  SANE_Int bytes_read;
  DBG_ENTER ();
  DBG (DBG_FUNC, "max_len=%d\n", max_len);

  *len = 0;

  if (!s->bIsScanning)
    {
      DBG (DBG_WARN, "scan was cancelled, is over or has not been " \
	   "initiated yet\n");
      return SANE_STATUS_CANCELLED;
    }

  DBG (DBG_DBG, "before read data, read_rows=%d\n", s->read_rows);
  if (s->scan_buf_len == 0)
    {
      if (s->read_rows > 0)
	{
	  lines = SCAN_BUFFER_SIZE / s->params.bytes_per_line;
	  lines = _MIN (lines, s->read_rows);

	  s->bIsReading = SANE_TRUE;
	  status = Scanner_GetRows (&s->state, s->scan_buf, &lines,
				    s->model.isRGBInvert);
	  if (status != SANE_STATUS_GOOD)
	    {
	      s->bIsReading = SANE_FALSE;
	      return status;
	    }

	  s->scan_buf_len = lines * s->params.bytes_per_line;
	  DBG (DBG_INFO, "scan_buf_len=%d\n", s->scan_buf_len);

	  if (s->scan_buf_len < SCAN_BUFFER_SIZE)
	    {
	      memset (s->scan_buf + s->scan_buf_len, 0,
		      SCAN_BUFFER_SIZE - s->scan_buf_len);
	    }

	  s->scan_buf_start = s->scan_buf;
	  s->read_rows -= lines;
	  s->bIsReading = SANE_FALSE;
	}
      else
	{
	  s->scan_buf_len = 0;
	}

      if (s->scan_buf_len == 0)
	{
	  DBG (DBG_FUNC, "scan finished -- exit\n");
	  sane_cancel (handle);
	  return SANE_STATUS_EOF;
	}
    }

  bytes_read = _MIN (max_len, s->scan_buf_len);
  DBG (DBG_DBG, "bytes_read=%d\n", bytes_read);
  *len = bytes_read;

  memcpy (buf, s->scan_buf_start, bytes_read);
  s->scan_buf_len -= bytes_read;
  s->scan_buf_start += bytes_read;

  DBG_LEAVE ();
  return SANE_STATUS_GOOD;
}

void
sane_cancel (SANE_Handle handle)
{
  Mustek_Scanner *s = handle;
  int i;
  DBG_ENTER ();

  if (s->bIsScanning)
    {
      s->bIsScanning = SANE_FALSE;
      if (s->read_rows > 0)
	DBG (DBG_INFO, "warning: is scanning\n");
      else
	DBG (DBG_INFO, "scan finished\n");

      Scanner_StopScan (&s->state);
      Scanner_BackHome (&s->state);

      for (i = 0; i < 20; i++)
	{
	  if (!s->bIsReading)
	    break;
	  sleep (1);
	}

      free (s->scan_buf);
      s->scan_buf = NULL;
      s->read_rows = 0;
      s->scan_buf_len = 0;
    }
  else
    {
      DBG (DBG_INFO, "not scanning\n");
    }

  DBG_LEAVE ();
}

SANE_Status
sane_set_io_mode (SANE_Handle __sane_unused__ handle, SANE_Bool non_blocking)
{
  if (non_blocking)
    return SANE_STATUS_UNSUPPORTED;
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_get_select_fd (SANE_Handle __sane_unused__ handle,
		    SANE_Int __sane_unused__ * fd)
{
  return SANE_STATUS_UNSUPPORTED;
}
