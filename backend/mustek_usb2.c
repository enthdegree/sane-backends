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

#include "../include/sane/config.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"
#include "../include/sane/saneopts.h"

#define BACKEND_NAME mustek_usb2
#include "../include/sane/sanei_backend.h"

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

  {1200, 600, 300, 150, 75, 0}, /* possible resolutions */

  SANE_FIX (8.3 * MM_PER_INCH),	/* size of scan area in mm (x) */
  SANE_FIX (11.6 * MM_PER_INCH), /* size of scan area in mm (y) */

  SANE_FIX (1.46 * MM_PER_INCH), /* size of scan area in TA mode in mm (x) */
  SANE_FIX (6.45 * MM_PER_INCH), /* size of scan area in TA mode in mm (y) */

  RO_RGB  /* order of the CCD/CIS colors */
};


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

static void
calc_parameters (Mustek_Scanner * s)
{
  SANE_String val, val_source;
  float x1, y1, x2, y2;

  DBG (DBG_FUNC, "calc_parameters: start\n");

  if (s->val[OPT_PREVIEW].w)
    s->setpara.wDpi = 75;
  else
    s->setpara.wDpi = s->val[OPT_RESOLUTION].w;

  x1 = SANE_UNFIX (s->val[OPT_TL_X].w) / MM_PER_INCH;
  y1 = SANE_UNFIX (s->val[OPT_TL_Y].w) / MM_PER_INCH;
  x2 = SANE_UNFIX (s->val[OPT_BR_X].w) / MM_PER_INCH;
  y2 = SANE_UNFIX (s->val[OPT_BR_Y].w) / MM_PER_INCH;

  s->setpara.wX = (unsigned short) ((x1 * s->setpara.wDpi) + 0.5);
  s->setpara.wY = (unsigned short) ((y1 * s->setpara.wDpi) + 0.5);
  s->setpara.wWidth = (unsigned short) (((x2 - x1) * s->setpara.wDpi) + 0.5);
  s->setpara.wHeight = (unsigned short) (((y2 - y1) * s->setpara.wDpi) + 0.5);

  s->setpara.wLineartThreshold = s->val[OPT_THRESHOLD].w;

  val_source = s->val[OPT_SOURCE].s;
  DBG (DBG_DET, "calc_parameters: scan source = %s\n", val_source);
  if (strcmp (val_source, source_list[SS_POSITIVE]) == 0)
    s->setpara.ssScanSource = SS_POSITIVE;
  else if (strcmp (val_source, source_list[SS_NEGATIVE]) == 0)
    s->setpara.ssScanSource = SS_NEGATIVE;
  else
    s->setpara.ssScanSource = SS_REFLECTIVE;

  val = s->val[OPT_MODE].s;
  if (strcmp (val, mode_list[CM_RGB48]) == 0)
    {
      if (s->val[OPT_PREVIEW].w)
	{
	  DBG (DBG_DET, "calc_parameters: preview, set ColorMode CM_RGB24\n");
	  s->setpara.cmColorMode = CM_RGB24;
	}
      else
	{
	  s->setpara.cmColorMode = CM_RGB48;
	}
    }
  else if (strcmp (val, mode_list[CM_RGB24]) == 0)
    {
      s->setpara.cmColorMode = CM_RGB24;
    }
  else if (strcmp (val, mode_list[CM_GRAY16]) == 0)
    {
      if (s->val[OPT_PREVIEW].w)
	{
	  DBG (DBG_DET, "calc_parameters: preview, set ColorMode CM_GRAY8\n");
	  s->setpara.cmColorMode = CM_GRAY8;
	}
      else
	{
	  s->setpara.cmColorMode = CM_GRAY16;
	}
    }
  else if (strcmp (val, mode_list[CM_GRAY8]) == 0)
    {
      s->setpara.cmColorMode = CM_GRAY8;
    }
  else
    {
      s->setpara.cmColorMode = CM_TEXT;
    }

  /* provide an estimate for scan parameters returned by sane_get_parameters */
  s->params.pixels_per_line = s->setpara.wWidth;
  s->params.lines = s->setpara.wHeight;
  s->params.last_frame = SANE_TRUE;

  switch (s->setpara.cmColorMode)
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

  DBG (DBG_FUNC, "calc_parameters: exit\n");
}

static SANE_Status
init_options (Mustek_Scanner * s)
{
  SANE_Int option, count = 0;
  SANE_Word *dpi_list;

  DBG (DBG_FUNC, "init_options: start\n");

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

  if (!MustScanner_IsTAConnected ())
    s->opt[OPT_SOURCE].cap |= SANE_CAP_INACTIVE;

  /* resolution */
  while (s->model.dpi_values[count] != 0)
    count++;
  dpi_list = malloc ((count + 1) * sizeof (SANE_Word));
  if (!dpi_list)
    return SANE_STATUS_NO_MEM;
  dpi_list[0] = count;
  memcpy (&dpi_list[1], s->model.dpi_values, count * sizeof (SANE_Word));

  s->opt[OPT_RESOLUTION].name = SANE_NAME_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].type = SANE_TYPE_INT;
  s->opt[OPT_RESOLUTION].unit = SANE_UNIT_DPI;
  s->opt[OPT_RESOLUTION].size = sizeof (SANE_Word);
  s->opt[OPT_RESOLUTION].constraint_type = SANE_CONSTRAINT_WORD_LIST;
  s->opt[OPT_RESOLUTION].constraint.word_list = dpi_list;
  s->val[OPT_RESOLUTION].w = dpi_list[1];

  /* preview */
  s->opt[OPT_PREVIEW].name = SANE_NAME_PREVIEW;
  s->opt[OPT_PREVIEW].title = SANE_TITLE_PREVIEW;
  s->opt[OPT_PREVIEW].desc = SANE_DESC_PREVIEW;
  s->opt[OPT_PREVIEW].type = SANE_TYPE_BOOL;
  s->opt[OPT_PREVIEW].size = sizeof (SANE_Word);
  s->val[OPT_PREVIEW].w = SANE_FALSE;

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

  calc_parameters (s);

  DBG (DBG_FUNC, "init_options: exit\n");
  return SANE_STATUS_GOOD;
}

static SANE_Bool
ReadScannedData (IMAGEROWS * pImageRows, TARGETIMAGE * pTarget)
{
  SANE_Bool isRGBInvert;
  unsigned short Rows;
  unsigned int i;

  DBG (DBG_FUNC, "ReadScannedData: start\n");

  isRGBInvert = (pImageRows->roRgbOrder == RO_RGB) ? SANE_FALSE : SANE_TRUE;
  Rows = pImageRows->wWantedLineNum;
  DBG (DBG_INFO, "ReadScannedData: wanted rows = %d\n", Rows);

  if (!MustScanner_GetRows (pImageRows->pBuffer, &Rows, isRGBInvert))
    return SANE_FALSE;
  pImageRows->wXferedLineNum = Rows;

  if ((pTarget->cmColorMode == CM_TEXT) ||
      (pTarget->ssScanSource == SS_NEGATIVE))
    {
      for (i = 0; i < (Rows * pTarget->dwBytesPerRow); i++)
	pImageRows->pBuffer[i] ^= 0xff;
    }

  DBG (DBG_FUNC, "ReadScannedData: exit\n");
  return SANE_TRUE;
}


/****************************** SANE API functions ****************************/

SANE_Status
sane_init (SANE_Int * version_code,
	   SANE_Auth_Callback __sane_unused__ authorize)
{
  DBG_INIT ();
  DBG (DBG_FUNC, "sane_init: start\n");
  DBG (DBG_ERR, "SANE Mustek USB2 backend version %d.%d build %d from %s\n",
       SANE_CURRENT_MAJOR, V_MINOR, BUILD, PACKAGE_STRING);

  if (version_code)
    *version_code = SANE_VERSION_CODE (SANE_CURRENT_MAJOR, V_MINOR, BUILD);

  DBG (DBG_FUNC, "sane_init: exit\n");
  return SANE_STATUS_GOOD;
}

void
sane_exit (void)
{
  DBG (DBG_FUNC, "sane_exit: start\n");

  if (devlist != NULL)
    {
      free (devlist);
      devlist = NULL;
    }

  DBG (DBG_FUNC, "sane_exit: exit\n");
}

SANE_Status
sane_get_devices (const SANE_Device *** device_list, SANE_Bool local_only)
{
  DBG (DBG_FUNC, "sane_get_devices: start: local_only = %s\n",
       local_only ? "true" : "false");

  if (devlist != NULL)
    free (devlist);
  devlist = calloc (2, sizeof (devlist[0]));
  if (!devlist)
    return SANE_STATUS_NO_MEM;

  /* HOLD: This is ugly (only one scanner!) and should go to sane_init */
  if (MustScanner_IsPresent ())
    {
      SANE_Device *sane_device = malloc (sizeof (*sane_device));
      if (!sane_device)
	return SANE_STATUS_NO_MEM;
      sane_device->name = strdup (device_name);
      sane_device->vendor = strdup ("Mustek");
      sane_device->model = strdup ("BearPaw 2448 TA Pro");
      sane_device->type = strdup ("flatbed scanner");
      devlist[0] = sane_device;
    }
  *device_list = devlist;

  DBG (DBG_FUNC, "sane_get_devices: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_open (SANE_String_Const devicename, SANE_Handle * handle)
{
  Mustek_Scanner *s;

  DBG (DBG_FUNC, "sane_open: start: devicename = %s\n", devicename);

  MustScanner_Init ();

  if (!MustScanner_PowerControl (SANE_FALSE, SANE_FALSE))
    return SANE_STATUS_INVAL;
  if (!MustScanner_BackHome ())
    return SANE_STATUS_INVAL;

  s = malloc (sizeof (*s));
  if (!s)
    return SANE_STATUS_NO_MEM;
  memset (s, 0, sizeof (*s));
  s->model = mustek_A2nu2_model;
  init_options (s);
  *handle = s;

  DBG (DBG_FUNC, "sane_open: exit\n");
  return SANE_STATUS_GOOD;
}

void
sane_close (SANE_Handle handle)
{
  Mustek_Scanner *s = handle;

  DBG (DBG_FUNC, "sane_close: start\n");

  MustScanner_PowerControl (SANE_FALSE, SANE_FALSE);
  MustScanner_BackHome ();

  if (s->scan_buf)
    free (s->scan_buf);
  s->scan_buf = NULL;

  free (handle);

  DBG (DBG_FUNC, "sane_close: exit\n");
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
  Mustek_Scanner *s = handle;
  SANE_Status status;
  SANE_Word cap;
  SANE_Int myinfo = 0;

  DBG (DBG_FUNC, "sane_control_option: start: action = %s, option = %s (%d)\n",
       (action == SANE_ACTION_GET_VALUE) ? "get" :
         (action == SANE_ACTION_SET_VALUE) ? "set" :
           (action == SANE_ACTION_SET_AUTO) ? "set_auto" : "unknown",
       s->opt[option].name, option);

  if (info)
    *info = 0;

  if (s->bIsScanning)
    {
      DBG (DBG_ERR, "sane_control_option: scanner is busy\n");
      return SANE_STATUS_DEVICE_BUSY;
    }
  if ((option >= NUM_OPTIONS) || (option < 0))
    {
      DBG (DBG_ERR, "sane_control_option: option index out of range\n");
      return SANE_STATUS_INVAL;
    }

  cap = s->opt[option].cap;
  if (!SANE_OPTION_IS_ACTIVE (cap))
    {
      DBG (DBG_ERR, "sane_control_option: option %d is inactive\n", option);
      return SANE_STATUS_INVAL;
    }

  if (action == SANE_ACTION_GET_VALUE)
    {
      switch (option)
	{
	  /* word options: */
	case OPT_NUM_OPTS:
	case OPT_RESOLUTION:
	case OPT_PREVIEW:
	case OPT_THRESHOLD:
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
	  *(SANE_Word *) val = s->val[option].w;
	  break;
	  /* string options: */
	case OPT_MODE:
	  strcpy (val, s->val[option].s);
	  break;
	case OPT_SOURCE:
	  strcpy (val, s->val[option].s);
	  break;
	default:
	  DBG (DBG_ERR, "sane_control_option: unknown option %d\n", option);
	}
    }
  else if (action == SANE_ACTION_SET_VALUE)
    {
      if (!SANE_OPTION_IS_SETTABLE (cap))
	{
	  DBG (DBG_ERR, "sane_control_option: option %d is not settable\n",
	       option);
	  return SANE_STATUS_INVAL;
	}

      status = sanei_constrain_value (s->opt + option, val, &myinfo);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_WARN, "sane_control_option: sanei_constrain_value returned" \
	       " %s\n", sane_strstatus (status));
	  return status;
	}

      switch (option)
	{
	  /* (mostly) side-effect-free word options: */
	case OPT_RESOLUTION:
	case OPT_PREVIEW:
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
	  s->val[option].w = *(SANE_Word *) val;
	  calc_parameters (s);
	  myinfo |= SANE_INFO_RELOAD_PARAMS;
	  break;
	case OPT_THRESHOLD:
	  s->val[option].w = *(SANE_Word *) val;
	  break;
	  /* side-effect-free word-array options: */
	case OPT_MODE:
	  if (s->val[option].s)
	    free (s->val[option].s);
	  s->val[option].s = strdup (val);
	  if (strcmp (s->val[option].s, SANE_VALUE_SCAN_MODE_LINEART) == 0)
	    s->opt[OPT_THRESHOLD].cap &= ~SANE_CAP_INACTIVE;
	  else
	    s->opt[OPT_THRESHOLD].cap |= SANE_CAP_INACTIVE;
	  calc_parameters (s);
	  myinfo |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
	  break;
	case OPT_SOURCE:
	  if (strcmp (s->val[option].s, val) != 0)
	    {	/* something changed */
	      if (s->val[option].s)
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
	  DBG (DBG_ERR, "sane_control_option: unknown option %d\n", option);
	}
    }
  else
    {
      DBG (DBG_ERR, "sane_control_option: unknown action %d for option %d\n",
	   action, option);
      return SANE_STATUS_INVAL;
    }

  if (info)
    *info = myinfo;

  DBG (DBG_FUNC, "sane_control_option: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters * params)
{
  Mustek_Scanner *s = handle;

  DBG (DBG_FUNC, "sane_get_parameters: start\n");

  DBG (DBG_INFO, "sane_get_parameters: params.format = %d\n", s->params.format);
  DBG (DBG_INFO, "sane_get_parameters: params.depth = %d\n", s->params.depth);
  DBG (DBG_INFO, "sane_get_parameters: params.pixels_per_line = %d\n",
       s->params.pixels_per_line);
  DBG (DBG_INFO, "sane_get_parameters: params.bytes_per_line = %d\n",
       s->params.bytes_per_line);
  DBG (DBG_INFO, "sane_get_parameters: params.lines = %d\n", s->params.lines);

  if (params)
    *params = s->params;

  DBG (DBG_FUNC, "sane_get_parameters: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_start (SANE_Handle handle)
{
  Mustek_Scanner *s = handle;

  DBG (DBG_FUNC, "sane_start: start\n");
 
  if ((s->val[OPT_TL_X].w >= s->val[OPT_BR_X].w) ||
      (s->val[OPT_TL_Y].w >= s->val[OPT_BR_Y].w))
    {
      DBG (DBG_CRIT, "sane_start: top left >= bottom right -- exiting\n");
      return SANE_FALSE;
    }
 
  calc_parameters (s);

  DBG (DBG_INFO, "sane_start: setpara.wX=%d\n", s->setpara.wX);
  DBG (DBG_INFO, "sane_start: setpara.wY=%d\n", s->setpara.wY);
  DBG (DBG_INFO, "sane_start: setpara.wWidth=%d\n", s->setpara.wWidth);
  DBG (DBG_INFO, "sane_start: setpara.wHeight=%d\n", s->setpara.wHeight);
  DBG (DBG_INFO, "sane_start: setpara.wLineartThreshold=%d\n",
       s->setpara.wLineartThreshold);
  DBG (DBG_INFO, "sane_start: setpara.wDpi=%d\n", s->setpara.wDpi);
  DBG (DBG_INFO, "sane_start: setpara.cmColorMode=%d\n",
       s->setpara.cmColorMode);
  DBG (DBG_INFO, "sane_start: setpara.ssScanSource=%d\n",
       s->setpara.ssScanSource);

  MustScanner_Reset (s->setpara.ssScanSource);

  /* adjust parameters to the scanner's requirements */
  if (!MustScanner_ScanSuggest (&s->setpara))
    {
      DBG (DBG_ERR, "sane_start: MustScanner_ScanSuggest error\n");
      return SANE_STATUS_INVAL;
    }

  /* update the scan parameters returned by sane_get_parameters */
  s->params.pixels_per_line = s->setpara.wWidth;
  s->params.lines = s->setpara.wHeight;
  s->params.bytes_per_line = s->setpara.dwBytesPerRow;

  s->read_rows = s->params.lines;
  DBG (DBG_INFO, "sane_start: read_rows = %d\n", s->read_rows);

  DBG (DBG_INFO, "SCANNING...\n");
  s->bIsScanning = SANE_TRUE;

  if (s->scan_buf)
    free (s->scan_buf);
  s->scan_buf = malloc (SCAN_BUFFER_SIZE);
  if (!s->scan_buf)
    return SANE_STATUS_NO_MEM;
  s->scan_buf_len = 0;

  if (!MustScanner_SetupScan (&s->setpara))
    return SANE_STATUS_INVAL;

  DBG (DBG_FUNC, "sane_start: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_read (SANE_Handle handle, SANE_Byte * buf, SANE_Int max_len,
	   SANE_Int * len)
{
  Mustek_Scanner *s = handle;
  SANE_Byte *tempbuf;
  long int tempbuf_size;
  SANE_Int lines_to_read, lines_read;
  IMAGEROWS image_row;

  DBG (DBG_FUNC, "sane_read: start: max_len=%d\n", max_len);

  if (!buf || !len)
    {
      DBG (DBG_ERR, "sane_read: output parameter is null!\n");
      return SANE_STATUS_INVAL;
    }

  *len = 0;

  if (!s->bIsScanning)
    {
      DBG (DBG_WARN, "sane_read: scan was cancelled, is over or has not been " \
	   "initiated yet\n");
      return SANE_STATUS_CANCELLED;
    }

  DBG (DBG_DBG, "sane_read: before read data read_row=%d\n", s->read_rows);
  if (s->scan_buf_len == 0)
    {
      if (s->read_rows > 0)
	{
	  lines_to_read = SCAN_BUFFER_SIZE / s->setpara.dwBytesPerRow;
	  if (lines_to_read > s->read_rows)
	    lines_to_read = s->read_rows;

	  tempbuf_size = lines_to_read * s->setpara.dwBytesPerRow +
			 3 * 1024 + 1;
	  tempbuf = malloc (tempbuf_size);
	  if (!tempbuf)
	    return SANE_STATUS_NO_MEM;
	  memset (tempbuf, 0, tempbuf_size);
	  DBG (DBG_INFO, "sane_read: buffer size is %ld\n", tempbuf_size);

	  image_row.roRgbOrder = s->model.line_mode_color_order;
	  image_row.wWantedLineNum = lines_to_read;
	  image_row.pBuffer = tempbuf;
	  s->bIsReading = SANE_TRUE;

	  if (!ReadScannedData (&image_row, &s->setpara))
	    {
	      DBG (DBG_ERR, "sane_read: ReadScannedData error\n");
	      s->bIsReading = SANE_FALSE;
	      return SANE_STATUS_INVAL;
	    }

	  DBG (DBG_DBG, "sane_read: Finish ReadScanedData\n");
	  s->scan_buf_len =
	    image_row.wXferedLineNum * s->setpara.dwBytesPerRow;
	  DBG (DBG_INFO, "sane_read : s->scan_buf_len = %ld\n",
	       (long int) s->scan_buf_len);

	  memcpy (s->scan_buf, tempbuf, s->scan_buf_len);
	  if (s->scan_buf_len < SCAN_BUFFER_SIZE)
	    {
	      memset (s->scan_buf + s->scan_buf_len, 0,
		      SCAN_BUFFER_SIZE - s->scan_buf_len);
	    }

	  DBG (DBG_DBG, "sane_read: after memcpy\n");
	  free (tempbuf);
	  s->scan_buf_start = s->scan_buf;
	  s->read_rows -= image_row.wXferedLineNum;
	  s->bIsReading = SANE_FALSE;
	}
      else
	{
	  s->scan_buf_len = 0;
	}

      if (s->scan_buf_len == 0)
	{
	  DBG (DBG_FUNC, "sane_read: scan finished -- exit\n");
	  sane_cancel (handle);
	  return SANE_STATUS_EOF;
	}
    }

  lines_read = _MIN (max_len, (SANE_Int) s->scan_buf_len);
  DBG (DBG_DBG, "sane_read: after %d\n", lines_read);

  *len = (SANE_Int) lines_read;

  DBG (DBG_INFO, "sane_read: get lines_read = %d\n", lines_read);
  DBG (DBG_INFO, "sane_read: get *len = %d\n", *len);
  memcpy (buf, s->scan_buf_start, lines_read);

  s->scan_buf_len -= lines_read;
  s->scan_buf_start += lines_read;
  DBG (DBG_FUNC, "sane_read: exit\n");
  return SANE_STATUS_GOOD;
}

void
sane_cancel (SANE_Handle handle)
{
  Mustek_Scanner *s = handle;
  int i;

  DBG (DBG_FUNC, "sane_cancel: start\n");

  if (s->bIsScanning)
    {
      s->bIsScanning = SANE_FALSE;
      if (s->read_rows > 0)
	DBG (DBG_INFO, "sane_cancel: warning: is scanning\n");
      else
	DBG (DBG_INFO, "sane_cancel: scan finished\n");

      MustScanner_StopScan ();
      MustScanner_BackHome ();

      for (i = 0; i < 20; i++)
	{
	  if (!s->bIsReading)
	    break;
	  sleep (1);
	}

      if (s->scan_buf)
	{
	  free (s->scan_buf);
	  s->scan_buf = NULL;
	  s->scan_buf_start = NULL;
	}

      s->read_rows = 0;
      s->scan_buf_len = 0;
      memset (&s->setpara, 0, sizeof (s->setpara));
    }
  else
    {
      DBG (DBG_INFO, "sane_cancel: do nothing\n");
    }

  DBG (DBG_FUNC, "sane_cancel: exit\n");
}

SANE_Status
sane_set_io_mode (SANE_Handle __sane_unused__ handle,
		  SANE_Bool __sane_unused__ non_blocking)
{
  return SANE_STATUS_UNSUPPORTED;
}

SANE_Status
sane_get_select_fd (SANE_Handle __sane_unused__ handle,
		    SANE_Int __sane_unused__ * fd)
{
  return SANE_STATUS_UNSUPPORTED;
}
