/* sane - Scanner Access Now Easy.
   Copyright (C) 2001 Stéphane Voltz <svoltz@wanadoo.fr>
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

   This file implements a SANE backend for Umax PP flatbed scanners.  */

/* CREDITS:
   Started by being a mere copy of mustek_pp 
   by Jochen Eisinger <jochen.eisinger@gmx.net> 
   then evolved in its own thing                                       */


#include "../include/sane/config.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include <sys/time.h>
#include <sys/types.h>

#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"
#include "../include/sane/saneopts.h"

#include "umax_pp_mid.h"
#include "umax_pp.h"

#define BACKEND_NAME	umax_pp
#include "../include/sane/sanei_backend.h"

#include "../include/sane/sanei_config.h"
#define UMAX_PP_CONFIG_FILE "umax_pp.conf"

#define MIN(a,b)	((a) < (b) ? (a) : (b))


/* DEBUG
 * 	for debug output, set SANE_DEBUG_UMAX_PP to
 * 		0	for nothing
 * 		1	for errors
 * 		2	for warnings
 * 		3	for additional information
 * 		4	for debug information
 * 		5	for code flow protocol (there isn't any)
 * 		129	if you want to know which parameters are unused
 */

/* history:
 *  0.0.1-devel		SANE backend structure taken from mustek_pp backend
 */

/* if you change the source, please set UMAX_PP_STATE to "devel". Do *not*
 * change the UMAX_PP_BUILD. */
#define UMAX_PP_BUILD	5
#define UMAX_PP_STATE	"devel"

static int num_devices = 0;
static Umax_PP_Descriptor *devlist = NULL;
static const SANE_Device **devarray = NULL;

static Umax_PP_Device *first_dev = NULL;


/* 1 Meg scan buffer */
static long int buf_size = 2024 * 1024;


static int red_gain = 0;
static int green_gain = 0;
static int blue_gain = 0;

static int red_highlight = 0;
static int green_highlight = 0;
static int blue_highlight = 0;






static const SANE_String_Const mode_list[] = {
  "Gray", "Color", 0
};

static const SANE_Range u4_range = {
  0,				/* minimum */
  15,				/* maximum */
  0				/* quantization */
};

static const SANE_Range u8_range = {
  0,				/* minimum */
  255,				/* maximum */
  0				/* quantization */
};


#define UMAX_PP_CHANNEL_RED		0
#define UMAX_PP_CHANNEL_GREEN		1
#define UMAX_PP_CHANNEL_BLUE		2
#define UMAX_PP_CHANNEL_GRAY		1

#define UMAX_PP_STATE_SCANNING		2
#define UMAX_PP_STATE_CANCELLED		1
#define UMAX_PP_STATE_IDLE		0

#define UMAX_PP_MODE_LINEART		0
#define UMAX_PP_MODE_GRAYSCALE		1
#define UMAX_PP_MODE_COLOR		2

#define MM_PER_INCH			25.4
#define MM_TO_PIXEL(mm, res)	(SANE_UNFIX(mm) * (float )res / MM_PER_INCH)
#define PIXEL_TO_MM(px, res)	(SANE_FIX((float )(px * MM_PER_INCH / (res / 10)) / 10.0))

#define UMAX_PP_DEFAULT_PORT		0x378




static SANE_Status
attach (const char *devname)
{
  Umax_PP_Descriptor *dev;
  int i;
  SANE_Status status = SANE_STATUS_GOOD;
  int ret, prt, mdl;
  char model[32];


  if ((strlen (devname) < 3) || (strlen (devname) > 8))
    return SANE_STATUS_INVAL;

  if ((devname[0] == '0') && ((devname[1] == 'x') || (devname[1] == 'X')))
    prt = strtol (devname + 2, NULL, 16);
  else
    prt = atoi (devname);


  for (i = 0; i < num_devices; i++)
    if (strcmp (devlist[i].port, devname) == 0)
      return SANE_STATUS_GOOD;

  ret = sanei_umax_pp_attach (prt);
  switch (ret)
    {
    case UMAX1220P_OK:
      status = SANE_STATUS_GOOD;
      break;
    case UMAX1220P_BUSY:
      status = SANE_STATUS_DEVICE_BUSY;
      break;
    case UMAX1220P_TRANSPORT_FAILED:
      DBG (1, "attach: failed to init transport layer on port %s\n", devname);
      status = SANE_STATUS_IO_ERROR;
      break;
    case UMAX1220P_PROBE_FAILED:
      DBG (1, "attach: failed to probe scanner on port %s\n", devname);
      status = SANE_STATUS_IO_ERROR;
      break;
    }

  if (status != SANE_STATUS_GOOD)
    {
      DBG (2, "attach: couldn't attach to `%s' (%s)\n", devname,
	   sane_strstatus (status));
      DEBUG ();
      return status;
    }


  /* now look for the model */
  do
    {
      ret = sanei_umax_pp_model (prt, &mdl);
      if (ret != UMAX1220P_OK)
	{
	  DBG (1, "attach: waiting for busy scanner on port %s\n", devname);
	}
    }
  while (ret == UMAX1220P_BUSY);

  if (ret != UMAX1220P_OK)
    {
      DBG (1, "attach: failed to recognize scanner model on port %s\n",
	   devname);
      return SANE_STATUS_IO_ERROR;
    }
  sprintf (model, "Astra %dP", mdl);


  dev = malloc (sizeof (Umax_PP_Descriptor) * (num_devices + 1));

  if (dev == NULL)
    {
      DBG (2, "attach: not enough memory for device descriptor\n");
      DEBUG ();
      return SANE_STATUS_NO_MEM;
    }

  memset (dev, 0, sizeof (Umax_PP_Descriptor) * (num_devices + 1));

  if (num_devices > 0)
    {
      memcpy (dev + 1, devlist, sizeof (Umax_PP_Descriptor) * (num_devices));
      free (devlist);
    }

  devlist = dev;
  num_devices++;

  dev->sane.name = strdup (devname);
  dev->sane.vendor = strdup ("UMAX");
  dev->sane.type = "flatbed scanner";

  dev->port = strdup (devname);
  dev->buf_size = buf_size;

  if (mdl > 610)
    {				/* Astra 1220, 1600 and 2000 */
      dev->max_res = 1200;
      dev->max_h_size = 5100;
      dev->max_v_size = 7000;
    }
  else
    {				/* Astra 610 */
      dev->max_res = 600;
      dev->max_h_size = 2050;
      dev->max_v_size = 3500;
    }
  dev->sane.model = strdup (model);


  DBG (3, "attach: device %s attached\n", devname);

  return SANE_STATUS_GOOD;
}






static SANE_Status
init_options (Umax_PP_Device * dev)
{
  int i;

  /* sets initial option value to zero */
  memset (dev->opt, 0, sizeof (dev->opt));
  memset (dev->val, 0, sizeof (dev->val));

  for (i = 0; i < NUM_OPTIONS; ++i)
    {
      dev->opt[i].size = sizeof (SANE_Word);
      dev->opt[i].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    }

  dev->opt[OPT_NUM_OPTS].name = SANE_NAME_NUM_OPTIONS;
  dev->opt[OPT_NUM_OPTS].title = SANE_TITLE_NUM_OPTIONS;
  dev->opt[OPT_NUM_OPTS].desc = SANE_DESC_NUM_OPTIONS;
  dev->opt[OPT_NUM_OPTS].type = SANE_TYPE_INT;
  dev->opt[OPT_NUM_OPTS].cap = SANE_CAP_SOFT_DETECT;
  dev->val[OPT_NUM_OPTS].w = NUM_OPTIONS;

  /* "Mode" group: */

  dev->opt[OPT_MODE_GROUP].title = "Scan Mode";
  dev->opt[OPT_MODE_GROUP].name = "";
  dev->opt[OPT_MODE_GROUP].desc = "";
  dev->opt[OPT_MODE_GROUP].type = SANE_TYPE_GROUP;
  dev->opt[OPT_MODE_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* scan mode */
  dev->opt[OPT_MODE].name = SANE_NAME_SCAN_MODE;
  dev->opt[OPT_MODE].title = SANE_TITLE_SCAN_MODE;
  dev->opt[OPT_MODE].desc = SANE_DESC_SCAN_MODE;
  dev->opt[OPT_MODE].type = SANE_TYPE_STRING;
  dev->opt[OPT_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  dev->opt[OPT_MODE].size = 10;
  dev->opt[OPT_MODE].constraint.string_list = mode_list;
  dev->val[OPT_MODE].s = strdup (mode_list[1]);

  /* resolution */
  dev->opt[OPT_RESOLUTION].name = SANE_NAME_SCAN_RESOLUTION;
  dev->opt[OPT_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
  dev->opt[OPT_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;
  dev->opt[OPT_RESOLUTION].type = SANE_TYPE_FIXED;
  dev->opt[OPT_RESOLUTION].unit = SANE_UNIT_DPI;
  dev->opt[OPT_RESOLUTION].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_RESOLUTION].constraint.range = &dev->dpi_range;
  dev->val[OPT_RESOLUTION].w = dev->dpi_range.min;


  /* preview */
  dev->opt[OPT_PREVIEW].name = SANE_NAME_PREVIEW;
  dev->opt[OPT_PREVIEW].title = SANE_TITLE_PREVIEW;
  dev->opt[OPT_PREVIEW].desc = SANE_DESC_PREVIEW;
  dev->opt[OPT_PREVIEW].type = SANE_TYPE_BOOL;
  dev->opt[OPT_PREVIEW].size = sizeof (SANE_Word);
  dev->opt[OPT_PREVIEW].unit = SANE_UNIT_NONE;
  dev->val[OPT_PREVIEW].w = SANE_FALSE;

  /* gray preview */
  dev->opt[OPT_GRAY_PREVIEW].name = SANE_NAME_GRAY_PREVIEW;
  dev->opt[OPT_GRAY_PREVIEW].title = SANE_TITLE_GRAY_PREVIEW;
  dev->opt[OPT_GRAY_PREVIEW].desc = SANE_DESC_GRAY_PREVIEW;
  dev->opt[OPT_GRAY_PREVIEW].type = SANE_TYPE_BOOL;
  dev->opt[OPT_GRAY_PREVIEW].size = sizeof (SANE_Word);
  dev->opt[OPT_GRAY_PREVIEW].unit = SANE_UNIT_NONE;
  dev->val[OPT_GRAY_PREVIEW].w = SANE_FALSE;

  /* "Geometry" group: */

  dev->opt[OPT_GEOMETRY_GROUP].title = "Geometry";
  dev->opt[OPT_GEOMETRY_GROUP].desc = "";
  dev->opt[OPT_GEOMETRY_GROUP].name = "";
  dev->opt[OPT_GEOMETRY_GROUP].type = SANE_TYPE_GROUP;
  dev->opt[OPT_GEOMETRY_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* top-left x */
  dev->opt[OPT_TL_X].name = SANE_NAME_SCAN_TL_X;
  dev->opt[OPT_TL_X].title = SANE_TITLE_SCAN_TL_X;
  dev->opt[OPT_TL_X].desc = SANE_DESC_SCAN_TL_X;
  dev->opt[OPT_TL_X].type = SANE_TYPE_INT;
  dev->opt[OPT_TL_X].unit = SANE_UNIT_PIXEL;
  dev->opt[OPT_TL_X].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_TL_X].constraint.range = &dev->x_range;
  dev->val[OPT_TL_X].w = 0;

  /* top-left y */
  dev->opt[OPT_TL_Y].name = SANE_NAME_SCAN_TL_Y;
  dev->opt[OPT_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
  dev->opt[OPT_TL_Y].desc = SANE_DESC_SCAN_TL_Y;
  dev->opt[OPT_TL_Y].type = SANE_TYPE_INT;
  dev->opt[OPT_TL_Y].unit = SANE_UNIT_PIXEL;
  dev->opt[OPT_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_TL_Y].constraint.range = &dev->y_range;
  dev->val[OPT_TL_Y].w = 0;

  /* bottom-right x */
  dev->opt[OPT_BR_X].name = SANE_NAME_SCAN_BR_X;
  dev->opt[OPT_BR_X].title = SANE_TITLE_SCAN_BR_X;
  dev->opt[OPT_BR_X].desc = SANE_DESC_SCAN_BR_X;
  dev->opt[OPT_BR_X].type = SANE_TYPE_INT;
  dev->opt[OPT_BR_X].unit = SANE_UNIT_PIXEL;
  dev->opt[OPT_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_BR_X].constraint.range = &dev->x_range;
  dev->val[OPT_BR_X].w = dev->x_range.max;

  /* bottom-right y */
  dev->opt[OPT_BR_Y].name = SANE_NAME_SCAN_BR_Y;
  dev->opt[OPT_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
  dev->opt[OPT_BR_Y].desc = SANE_DESC_SCAN_BR_Y;
  dev->opt[OPT_BR_Y].type = SANE_TYPE_INT;
  dev->opt[OPT_BR_Y].unit = SANE_UNIT_PIXEL;
  dev->opt[OPT_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_BR_Y].constraint.range = &dev->y_range;
  dev->val[OPT_BR_Y].w = dev->y_range.max;

  /* "Enhancement" group: */

  dev->opt[OPT_ENHANCEMENT_GROUP].title = "Enhancement";
  dev->opt[OPT_ENHANCEMENT_GROUP].desc = "";
  dev->opt[OPT_ENHANCEMENT_GROUP].name = "";
  dev->opt[OPT_ENHANCEMENT_GROUP].type = SANE_TYPE_GROUP;
  dev->opt[OPT_ENHANCEMENT_GROUP].cap |= SANE_CAP_ADVANCED;
  dev->opt[OPT_ENHANCEMENT_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* lamp control */
  dev->opt[OPT_LAMP_CONTROL].name = "lamp-control";
  dev->opt[OPT_LAMP_CONTROL].title = "Lamp on";
  dev->opt[OPT_LAMP_CONTROL].desc = "Sets lamp on/off";
  dev->opt[OPT_LAMP_CONTROL].type = SANE_TYPE_BOOL;
  dev->opt[OPT_LAMP_CONTROL].size = sizeof (SANE_Word);
  dev->opt[OPT_LAMP_CONTROL].unit = SANE_UNIT_NONE;
  dev->val[OPT_LAMP_CONTROL].w = SANE_TRUE;
  dev->opt[OPT_LAMP_CONTROL].cap |= SANE_CAP_ADVANCED;

  /* custom-gamma table */
  dev->opt[OPT_CUSTOM_GAMMA].name = SANE_NAME_CUSTOM_GAMMA;
  dev->opt[OPT_CUSTOM_GAMMA].title = SANE_TITLE_CUSTOM_GAMMA;
  dev->opt[OPT_CUSTOM_GAMMA].desc = SANE_DESC_CUSTOM_GAMMA;
  dev->opt[OPT_CUSTOM_GAMMA].type = SANE_TYPE_BOOL;
  dev->opt[OPT_CUSTOM_GAMMA].cap |= SANE_CAP_ADVANCED ;
  dev->val[OPT_CUSTOM_GAMMA].w = SANE_FALSE;

  /* grayscale gamma vector */
  dev->opt[OPT_GAMMA_VECTOR].name = SANE_NAME_GAMMA_VECTOR;
  dev->opt[OPT_GAMMA_VECTOR].title = SANE_TITLE_GAMMA_VECTOR;
  dev->opt[OPT_GAMMA_VECTOR].desc = SANE_DESC_GAMMA_VECTOR;
  dev->opt[OPT_GAMMA_VECTOR].type = SANE_TYPE_INT;
  dev->opt[OPT_GAMMA_VECTOR].cap |= SANE_CAP_INACTIVE;
  dev->opt[OPT_GAMMA_VECTOR].unit = SANE_UNIT_NONE;
  dev->opt[OPT_GAMMA_VECTOR].size = 256 * sizeof (SANE_Word);
  dev->opt[OPT_GAMMA_VECTOR].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_GAMMA_VECTOR].constraint.range = &u8_range;
  dev->val[OPT_GAMMA_VECTOR].wa = &dev->gamma_table[0][0];

  /* red gamma vector */
  dev->opt[OPT_GAMMA_VECTOR_R].name = SANE_NAME_GAMMA_VECTOR_R;
  dev->opt[OPT_GAMMA_VECTOR_R].title = SANE_TITLE_GAMMA_VECTOR_R;
  dev->opt[OPT_GAMMA_VECTOR_R].desc = SANE_DESC_GAMMA_VECTOR_R;
  dev->opt[OPT_GAMMA_VECTOR_R].type = SANE_TYPE_INT;
  dev->opt[OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
  dev->opt[OPT_GAMMA_VECTOR_R].unit = SANE_UNIT_NONE;
  dev->opt[OPT_GAMMA_VECTOR_R].size = 256 * sizeof (SANE_Word);
  dev->opt[OPT_GAMMA_VECTOR_R].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_GAMMA_VECTOR_R].constraint.range = &u8_range;
  dev->val[OPT_GAMMA_VECTOR_R].wa = &dev->gamma_table[1][0];

  /* green gamma vector */
  dev->opt[OPT_GAMMA_VECTOR_G].name = SANE_NAME_GAMMA_VECTOR_G;
  dev->opt[OPT_GAMMA_VECTOR_G].title = SANE_TITLE_GAMMA_VECTOR_G;
  dev->opt[OPT_GAMMA_VECTOR_G].desc = SANE_DESC_GAMMA_VECTOR_G;
  dev->opt[OPT_GAMMA_VECTOR_G].type = SANE_TYPE_INT;
  dev->opt[OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
  dev->opt[OPT_GAMMA_VECTOR_G].unit = SANE_UNIT_NONE;
  dev->opt[OPT_GAMMA_VECTOR_G].size = 256 * sizeof (SANE_Word);
  dev->opt[OPT_GAMMA_VECTOR_G].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_GAMMA_VECTOR_G].constraint.range = &u8_range;
  dev->val[OPT_GAMMA_VECTOR_G].wa = &dev->gamma_table[2][0];

  /* blue gamma vector */
  dev->opt[OPT_GAMMA_VECTOR_B].name = SANE_NAME_GAMMA_VECTOR_B;
  dev->opt[OPT_GAMMA_VECTOR_B].title = SANE_TITLE_GAMMA_VECTOR_B;
  dev->opt[OPT_GAMMA_VECTOR_B].desc = SANE_DESC_GAMMA_VECTOR_B;
  dev->opt[OPT_GAMMA_VECTOR_B].type = SANE_TYPE_INT;
  dev->opt[OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;
  dev->opt[OPT_GAMMA_VECTOR_B].unit = SANE_UNIT_NONE;
  dev->opt[OPT_GAMMA_VECTOR_B].size = 256 * sizeof (SANE_Word);
  dev->opt[OPT_GAMMA_VECTOR_B].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_GAMMA_VECTOR_B].constraint.range = &u8_range;
  dev->val[OPT_GAMMA_VECTOR_B].wa = &dev->gamma_table[3][0];

  /*  gain group */
  dev->opt[OPT_MANUAL_GAIN].name = "manual-channel-gain";
  dev->opt[OPT_MANUAL_GAIN].title = "Gain";
  dev->opt[OPT_MANUAL_GAIN].desc = "Color channels gain settings";
  dev->opt[OPT_MANUAL_GAIN].type = SANE_TYPE_BOOL;
  dev->opt[OPT_MANUAL_GAIN].cap |= SANE_CAP_ADVANCED;
  dev->val[OPT_MANUAL_GAIN].w = SANE_FALSE;

  /* gray gain */
  dev->opt[OPT_GRAY_GAIN].name = "gray-gain";
  dev->opt[OPT_GRAY_GAIN].title = "Gray gain";
  dev->opt[OPT_GRAY_GAIN].desc = "Sets gray channel gain";
  dev->opt[OPT_GRAY_GAIN].type = SANE_TYPE_INT;
  dev->opt[OPT_GRAY_GAIN].cap |= SANE_CAP_INACTIVE | SANE_CAP_ADVANCED;
  dev->opt[OPT_GRAY_GAIN].unit = SANE_UNIT_NONE;
  dev->opt[OPT_GRAY_GAIN].size = sizeof (SANE_Int);
  dev->opt[OPT_GRAY_GAIN].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_GRAY_GAIN].constraint.range = &u4_range;
  dev->val[OPT_GRAY_GAIN].w = dev->gray_gain;

  /* red gain */
  dev->opt[OPT_RED_GAIN].name = "red-gain";
  dev->opt[OPT_RED_GAIN].title = "Red gain";
  dev->opt[OPT_RED_GAIN].desc = "Sets red channel gain";
  dev->opt[OPT_RED_GAIN].type = SANE_TYPE_INT;
  dev->opt[OPT_RED_GAIN].cap |= SANE_CAP_INACTIVE | SANE_CAP_ADVANCED;
  dev->opt[OPT_RED_GAIN].unit = SANE_UNIT_NONE;
  dev->opt[OPT_RED_GAIN].size = sizeof (SANE_Int);
  dev->opt[OPT_RED_GAIN].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_RED_GAIN].constraint.range = &u4_range;
  dev->val[OPT_RED_GAIN].w = dev->red_gain;

  /* green gain */
  dev->opt[OPT_GREEN_GAIN].name = "green-gain";
  dev->opt[OPT_GREEN_GAIN].title = "Green gain";
  dev->opt[OPT_GREEN_GAIN].desc = "Sets green channel gain";
  dev->opt[OPT_GREEN_GAIN].type = SANE_TYPE_INT;
  dev->opt[OPT_GREEN_GAIN].cap |= SANE_CAP_INACTIVE | SANE_CAP_ADVANCED;
  dev->opt[OPT_GREEN_GAIN].unit = SANE_UNIT_NONE;
  dev->opt[OPT_GREEN_GAIN].size = sizeof (SANE_Int);
  dev->opt[OPT_GREEN_GAIN].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_GREEN_GAIN].constraint.range = &u4_range;
  dev->val[OPT_GREEN_GAIN].w = dev->green_gain;

  /* blue gain */
  dev->opt[OPT_BLUE_GAIN].name = "blue-gain";
  dev->opt[OPT_BLUE_GAIN].title = "Blue gain";
  dev->opt[OPT_BLUE_GAIN].desc = "Sets blue channel gain";
  dev->opt[OPT_BLUE_GAIN].type = SANE_TYPE_INT;
  dev->opt[OPT_BLUE_GAIN].cap |= SANE_CAP_INACTIVE | SANE_CAP_ADVANCED;
  dev->opt[OPT_BLUE_GAIN].unit = SANE_UNIT_NONE;
  dev->opt[OPT_BLUE_GAIN].size = sizeof (SANE_Int);
  dev->opt[OPT_BLUE_GAIN].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_BLUE_GAIN].constraint.range = &u4_range;
  dev->val[OPT_BLUE_GAIN].w = dev->blue_gain;

  /*  highlight group */
  dev->opt[OPT_MANUAL_HIGHLIGHT].name = "manual-highlight";
  dev->opt[OPT_MANUAL_HIGHLIGHT].title = "Highlight";
  dev->opt[OPT_MANUAL_HIGHLIGHT].desc = "Color channels highlight settings";
  dev->opt[OPT_MANUAL_HIGHLIGHT].type = SANE_TYPE_BOOL;
  dev->opt[OPT_MANUAL_HIGHLIGHT].cap |= SANE_CAP_ADVANCED;
  dev->val[OPT_MANUAL_HIGHLIGHT].w = SANE_FALSE;

  /* gray highlight */
  dev->opt[OPT_GRAY_HIGHLIGHT].name = "gray-highlight";
  dev->opt[OPT_GRAY_HIGHLIGHT].title = "Gray highlight";
  dev->opt[OPT_GRAY_HIGHLIGHT].desc = "Sets gray channel highlight";
  dev->opt[OPT_GRAY_HIGHLIGHT].type = SANE_TYPE_INT;
  dev->opt[OPT_GRAY_HIGHLIGHT].cap |= SANE_CAP_INACTIVE | SANE_CAP_ADVANCED;
  dev->opt[OPT_GRAY_HIGHLIGHT].unit = SANE_UNIT_NONE;
  dev->opt[OPT_GRAY_HIGHLIGHT].size = sizeof (SANE_Int);
  dev->opt[OPT_GRAY_HIGHLIGHT].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_GRAY_HIGHLIGHT].constraint.range = &u4_range;
  dev->val[OPT_GRAY_HIGHLIGHT].w = dev->gray_highlight;

  /* red highlight */
  dev->opt[OPT_RED_HIGHLIGHT].name = "red-highlight";
  dev->opt[OPT_RED_HIGHLIGHT].title = "Red highlight";
  dev->opt[OPT_RED_HIGHLIGHT].desc = "Sets red channel highlight";
  dev->opt[OPT_RED_HIGHLIGHT].type = SANE_TYPE_INT;
  dev->opt[OPT_RED_HIGHLIGHT].cap |= SANE_CAP_INACTIVE | SANE_CAP_ADVANCED;
  dev->opt[OPT_RED_HIGHLIGHT].unit = SANE_UNIT_NONE;
  dev->opt[OPT_RED_HIGHLIGHT].size = sizeof (SANE_Int);
  dev->opt[OPT_RED_HIGHLIGHT].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_RED_HIGHLIGHT].constraint.range = &u4_range;
  dev->val[OPT_RED_HIGHLIGHT].w = dev->red_highlight;

  /* green highlight */
  dev->opt[OPT_GREEN_HIGHLIGHT].name = "green-highlight";
  dev->opt[OPT_GREEN_HIGHLIGHT].title = "Green highlight";
  dev->opt[OPT_GREEN_HIGHLIGHT].desc = "Sets green channel highlight";
  dev->opt[OPT_GREEN_HIGHLIGHT].type = SANE_TYPE_INT;
  dev->opt[OPT_GREEN_HIGHLIGHT].cap |= SANE_CAP_INACTIVE | SANE_CAP_ADVANCED;
  dev->opt[OPT_GREEN_HIGHLIGHT].unit = SANE_UNIT_NONE;
  dev->opt[OPT_GREEN_HIGHLIGHT].size = sizeof (SANE_Int);
  dev->opt[OPT_GREEN_HIGHLIGHT].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_GREEN_HIGHLIGHT].constraint.range = &u4_range;
  dev->val[OPT_GREEN_HIGHLIGHT].w = dev->green_highlight;

  /* blue highlight */
  dev->opt[OPT_BLUE_HIGHLIGHT].name = "blue-highlight";
  dev->opt[OPT_BLUE_HIGHLIGHT].title = "Blue highlight";
  dev->opt[OPT_BLUE_HIGHLIGHT].desc = "Sets blue channel highlight";
  dev->opt[OPT_BLUE_HIGHLIGHT].type = SANE_TYPE_INT;
  dev->opt[OPT_BLUE_HIGHLIGHT].cap |= SANE_CAP_INACTIVE | SANE_CAP_ADVANCED;
  dev->opt[OPT_BLUE_HIGHLIGHT].unit = SANE_UNIT_NONE;
  dev->opt[OPT_BLUE_HIGHLIGHT].size = sizeof (SANE_Int);
  dev->opt[OPT_BLUE_HIGHLIGHT].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_BLUE_HIGHLIGHT].constraint.range = &u4_range;
  dev->val[OPT_BLUE_HIGHLIGHT].w = dev->blue_highlight;

  return SANE_STATUS_GOOD;
}



SANE_Status
sane_init (SANE_Int * version_code, SANE_Auth_Callback authorize)
{
  char dev_name[512];
  const char *cp;
  size_t len;
  FILE *fp;
  SANE_Status ret;

  DBG_INIT ();

  if (authorize != NULL)
    {
      DBG (2, "init: SANE_Auth_Callback not supported (yet) ...\n");
    }

  if (version_code != NULL)
    *version_code = SANE_VERSION_CODE (V_MAJOR, V_MINOR, UMAX_PP_BUILD);

  DBG (3, "init: SANE v%s, backend v%d.%d.%d-%s\n", VERSION, V_MAJOR, V_MINOR,
       UMAX_PP_BUILD, UMAX_PP_STATE);

  fp = sanei_config_open (UMAX_PP_CONFIG_FILE);


  if (fp == NULL)
    {
      DBG (2, "init: no configuration file, using default `port 0x%03X'\n",
	   UMAX_PP_DEFAULT_PORT);

      ret = attach (STRINGIFY (UMAX_PP_DEFAULT_PORT));
      return ret;
    }

  while (sanei_config_read (dev_name, sizeof (dev_name), fp))
    {
      cp = sanei_config_skip_whitespace (dev_name);
      if (!*cp || *cp == '#')	/* ignore line comments & empty lines */
	continue;

      len = strlen (cp);

      if (!len)
	continue;		/* ignore empty lines */


      if (strncmp (cp, "option", 6) == 0 && isspace (cp[6]))
	{

	  cp += 7;
	  cp = sanei_config_skip_whitespace (cp);

	  if (strncmp (cp, "buffer", 6) == 0 && isspace (cp[6]))
	    {
	      char *end;
	      long int val;

	      cp += 7;

	      errno = 0;
	      val = strtol (cp, &end, 0);

	      if ((end == cp || errno) || (val < 0))
		{
		  DBG (2, "init: invalid value `%s`, falling back to %ld\n",
		       cp, buf_size);
		  val = buf_size;	/* safe fallback */
		}

	      DBG (3, "init: option buffer %ld\n", val);

	      if (num_devices == 0)
		{
		  DBG (3, "init: setting global option buffer to %ld\n", val);
		  buf_size = val;
		}
	      else
		{
		  DBG (3, "init: setting buffer to %ld for device %s\n",
		       val, devlist[0].sane.name);
		  devlist[0].buf_size = val;
		}
	    }
	  else if (strncmp (cp, "astra", 6) == 0)
	    {
	      char *end;
	      long int val;

	      cp += 7;

	      errno = 0;
	      val = strtol (cp, &end, 0);

	      if ((end == cp || errno))
		{
		  val = 1220;	/* safe fallback */
		  DBG (2,
		       "init: invalid astra value `%s`, falling back to %ld\n",
		       cp, val);
		}
	      if ((val != 610)
		  && (val != 1220) && (val != 1600) && (val != 2000))
		{
		  val = 1220;	/* safe fallback */
		  DBG (2,
		       "init: invalid astra value `%s`, falling back to %ld\n",
		       cp, val);
		}
	      sanei_umax_pp_setastra (val);
	      DBG (3, "init: option astra %ld P\n", val);
	    }
	  else if (strncmp (cp, "red-gain", 8) == 0)
	    {
	      char *end;
	      long int val;

	      cp += 9;

	      errno = 0;
	      val = strtol (cp, &end, 0);

	      if ((end == cp || errno) || (val < 0) || (val > 15))
		{
		  val = 8;	/* safe fallback */
		  DBG (2, "init: invalid value `%s`, falling back to %ld\n",
		       cp, val);
		}

	      DBG (3, "init: option buffer %ld\n", val);

	      DBG (3, "init: setting global option red-gain to %ld\n", val);
	      red_gain = val;
	    }
	  else if (strncmp (cp, "green-gain", 10) == 0 && isspace (cp[10]))
	    {
	      char *end;
	      long int val;

	      cp += 11;

	      errno = 0;
	      val = strtol (cp, &end, 0);

	      if ((end == cp || errno) || (val < 0) || (val > 15))
		{
		  val = 4;	/* safe fallback */
		  DBG (2, "init: invalid value `%s`, falling back to %ld\n",
		       cp, val);
		}

	      DBG (3, "init: option green-gain %ld\n", val);
	      DBG (3, "init: setting global option green-gain to %ld\n", val);
	      green_gain = val;
	    }
	  else if (strncmp (cp, "blue-gain", 9) == 0 && isspace (cp[9]))
	    {
	      char *end;
	      long int val;

	      cp += 10;

	      errno = 0;
	      val = strtol (cp, &end, 0);

	      if ((end == cp || errno) || (val < 0) || (val > 15))
		{
		  val = 8;	/* safe fallback */
		  DBG (2, "init: invalid value `%s`, falling back to %ld\n",
		       cp, val);
		}

	      DBG (3, "init: option blue-gain %ld\n", val);

	      DBG (3, "init: setting global option blue-gain to %ld\n", val);
	      blue_gain = val;
	    }
	  else if ((strncmp (cp, "red-highlight", 13) == 0)
		   && isspace (cp[13]))
	    {
	      char *end;
	      long int val;

	      cp += 14;

	      errno = 0;
	      val = strtol (cp, &end, 0);

	      if ((end == cp || errno) || (val < 0) || (val > 15))
		{
		  val = 8;	/* safe fallback */
		  DBG (2, "init: invalid value `%s`, falling back to %ld\n",
		       cp, val);
		}

	      DBG (3, "init: option buffer %ld\n", val);

	      DBG (3, "init: setting global option red-highlight to %ld\n",
		   val);
	      red_highlight = val;
	    }
	  else if (strncmp (cp, "green-highlight", 15) == 0
		   && isspace (cp[15]))
	    {
	      char *end;
	      long int val;

	      cp += 16;

	      errno = 0;
	      val = strtol (cp, &end, 0);

	      if ((end == cp || errno) || (val < 0) || (val > 15))
		{
		  val = 4;	/* safe fallback */
		  DBG (2, "init: invalid value `%s`, falling back to %ld\n",
		       cp, val);
		}

	      DBG (3, "init: option green-highlight %ld\n", val);
	      DBG (3, "init: setting global option green-highlight to %ld\n",
		   val);
	      green_highlight = val;
	    }
	  else if (strncmp (cp, "blue-highlight", 14) == 0 && isspace (cp[14]))
	    {
	      char *end;
	      long int val;

	      cp += 15;

	      errno = 0;
	      val = strtol (cp, &end, 0);

	      if ((end == cp || errno) || (val < 0) || (val > 15))
		{
		  val = 8;	/* safe fallback */
		  DBG (2, "init: invalid value `%s`, falling back to %ld\n",
		       cp, val);
		}

	      DBG (3, "init: option blue-highlight %ld\n", val);

	      DBG (3, "init: setting global option blue-highlight to %ld\n",
		   val);
	      blue_highlight = val;
	    }
	}
      else if ((strncmp (cp, "port", 4) == 0) && isspace (cp[4]))
	{

	  cp += 5;
	  cp = sanei_config_skip_whitespace (cp);

	  DBG (3, "init: trying port `%s'\n", cp);

	  if (*cp)
	    {
	      DBG (3, "attach(%s)\n", cp);
	      if (attach (cp) != SANE_STATUS_GOOD)
		DBG (2, "init: couldn't attach to port `%s'\n", cp);
	    }
	}
      else if ((strncmp (cp, "name", 4) == 0) && isspace (cp[4]))
	{
	  cp += 5;
	  cp = sanei_config_skip_whitespace (cp);

	  if (num_devices == 0)
	    DBG (2, "init: 'name' only allowed after 'port'\n");
	  else
	    {
	      DBG (3, "init: naming device %s '%s'\n", devlist[0].port, cp);
	      free (devlist[0].sane.name);
	      devlist[0].sane.name = strdup (cp);
	    }
	}
      else if ((strncmp (cp, "model", 5) == 0) && isspace (cp[5]))
	{
	  cp += 6;
	  cp = sanei_config_skip_whitespace (cp);

	  if (num_devices == 0)
	    DBG (2, "init: 'model' only allowed after 'port'\n");
	  else
	    {
	      DBG (3, "init: device %s is a '%s'\n", devlist[0].port, cp);
	      free (devlist[0].sane.model);
	      devlist[0].sane.model = strdup (cp);
	    }
	}
      else if ((strncmp (cp, "vendor", 6) == 0) && isspace (cp[6]))
	{
	  cp += 7;
	  cp = sanei_config_skip_whitespace (cp);

	  if (num_devices == 0)
	    DBG (2, "init: 'vendor' only allowed after 'port'\n");
	  else
	    {
	      DBG (3, "init: device %s is from '%s'\n", devlist[0].port, cp);
	      free (devlist[0].sane.vendor);
	      devlist[0].sane.vendor = strdup (cp);
	    }
	}
      else
	DBG (2, "init: don't know what to do with `%s'\n", cp);
    }

  fclose (fp);

  return SANE_STATUS_GOOD;
}

void
sane_exit (void)
{
  int i;
  Umax_PP_Device *dev;

  DBG (3, "sane_exit: (...)\n");
  if (first_dev)
    DBG (3, "exit: closing open devices\n");

  while (first_dev)
    {
      dev = first_dev;
      sane_close (dev);
    }

  for (i = 0; i < num_devices; i++)
    {
      free (devlist[i].port);
      free (devlist[i].sane.name);
      free (devlist[i].sane.model);
      free (devlist[i].sane.vendor);
    }

  if (devlist != NULL)
    free (devlist);

  if (devarray != NULL)
    free (devarray);


  num_devices = 0;
}

SANE_Status
sane_get_devices (const SANE_Device *** device_list, SANE_Bool local_only)
{
  int i;

  DBG (3, "get_devices\n");
  DBG (129, "unused arg: local_only = %d\n", (int) local_only);

  if (devarray != NULL)
    free (devarray);

  devarray = malloc ((num_devices + 1) * sizeof (devarray[0]));

  if (devarray == NULL)
    {
      DBG (2, "get_devices: not enough memory for device list\n");
      DEBUG ();
      return SANE_STATUS_NO_MEM;
    }

  for (i = 0; i < num_devices; i++)
    devarray[i] = &devlist[i].sane;

  devarray[num_devices] = NULL;
  *device_list = devarray;

  return SANE_STATUS_GOOD;
}

SANE_Status
sane_open (SANE_String_Const devicename, SANE_Handle * handle)
{
  Umax_PP_Device *dev;
  Umax_PP_Descriptor *desc;
  int i, j;
  int rc, prt;

  DBG (3, "open: device `%s'\n", devicename);

  if (devicename[0])
    {
      for (i = 0; i < num_devices; i++)
	if (strcmp (devlist[i].sane.name, devicename) == 0)
	  break;

      if (i >= num_devices)
	for (i = 0; i < num_devices; i++)
	  if (strcmp (devlist[i].port, devicename) == 0)
	    break;

      if (i >= num_devices)
	{
	  DBG (2, "open: device doesn't exist\n");
	  DEBUG ();
	  return SANE_STATUS_INVAL;
	}

      desc = &devlist[i];

      if ((devlist[i].port[0] == '0')
	  && ((devlist[i].port[1] == 'x') || (devlist[i].port[1] == 'X')))
	prt = strtol (devlist[i].port + 2, NULL, 16);
      else
	prt = atoi (devlist[i].port);
      DBG (64, "open: devlist[i].port='%s' -> port=0x%X\n", devlist[i].port,
	   prt);
      rc = sanei_umax_pp_open (prt);
    }
  else
    {

      if (num_devices == 0)
	{
	  DBG (1, "open: no devices present\n");
	  return SANE_STATUS_INVAL;
	}

      DBG (3, "open: trying default device %s, port=%s\n",
	   devlist[0].sane.name, devlist[0].port);

      rc = sanei_umax_pp_open (atoi (devlist[0].port));

      desc = &devlist[0];
    }
  switch (rc)
    {
    case UMAX1220P_TRANSPORT_FAILED:
      DBG (1, "failed to init transport layer on port 0x%03X\n",
	   atoi (desc->port));
      return SANE_STATUS_IO_ERROR;
    case UMAX1220P_SCANNER_FAILED:
      DBG (1, "failed to initialize scanner on port 0x%03X\n",
	   atoi (desc->port));
      return SANE_STATUS_IO_ERROR;
    case UMAX1220P_BUSY:
      DBG (1, "busy scanner on port 0x%03X\n", atoi (desc->port));
      return SANE_STATUS_DEVICE_BUSY;
    }


  dev = (Umax_PP_Device *) malloc (sizeof (*dev));

  if (dev == NULL)
    {
      DBG (2, "open: not enough memory for device descriptor\n");
      DEBUG ();
      return SANE_STATUS_NO_MEM;
    }

  memset (dev, 0, sizeof (*dev));

  dev->desc = desc;

  for (i = 0; i < 4; ++i)
    for (j = 0; j < 256; ++j)
      dev->gamma_table[i][j] = j;

  dev->buf = malloc (dev->desc->buf_size);
  dev->bufsize = dev->desc->buf_size;

  dev->dpi_range.min = SANE_FIX (75);
  dev->dpi_range.max = SANE_FIX (dev->desc->max_res);
  dev->dpi_range.quant = 0;

  dev->x_range.min = 0;
  dev->x_range.max = dev->desc->max_h_size;
  dev->x_range.quant = 0;

  dev->y_range.min = 0;
  dev->y_range.max = dev->desc->max_v_size;
  dev->y_range.quant = 0;

  dev->gray_gain = 0;

  /* use pre defined settings read from umax_pp.conf */
  dev->red_gain = red_gain;
  dev->green_gain = green_gain;
  dev->blue_gain = blue_gain;
  dev->red_highlight = red_highlight;
  dev->green_highlight = green_highlight;
  dev->blue_highlight = blue_highlight;


  if (dev->buf == NULL)
    {
      DBG (2, "open: not enough memory for scan buffer (%lu bytes)\n",
	   (long int) dev->desc->buf_size);
      DEBUG ();
      free (dev);
      return SANE_STATUS_NO_MEM;
    }

  init_options (dev);

  dev->next = first_dev;
  first_dev = dev;


  *handle = dev;

  DBG (3, "open: success\n");

  return SANE_STATUS_GOOD;
}

void
sane_close (SANE_Handle handle)
{
  Umax_PP_Device *prev, *dev;
  int rc;

  DBG (3, "sane_close: ...\n");
  /* remove handle from list of open handles: */
  prev = NULL;

  for (dev = first_dev; dev; dev = dev->next)
    {
      if (dev == handle)
	break;
      prev = dev;
    }

  if (dev == NULL)
    {
      DBG (2, "close: unknown device\n");
      DEBUG ();
      return;			/* oops, not a handle we know about */
    }

  if (dev->state == UMAX_PP_STATE_SCANNING)
    sane_cancel (handle);	/* remember: sane_cancel is a macro and
				   expands to sane_umax_pp_cancel ()... */


  /* if the scanner is parking head, we wait it to finish */
  while (dev->state == UMAX_PP_STATE_CANCELLED)
    {
      DBG (2, "close: waiting scanner to park head\n");
      rc = sanei_umax_pp_status ();

      /* check if scanner busy parking */
      if (rc != UMAX1220P_BUSY)
	{
	  DBG (2, "close: scanner head parked\n");
	  dev->state = UMAX_PP_STATE_IDLE;
	}
    }

  /* then we switch off light if needed */
  if (dev->val[OPT_LAMP_CONTROL].w == SANE_TRUE)
    {
      rc = sanei_umax_pp_lamp (0);
      if (rc == UMAX1220P_TRANSPORT_FAILED)
	{
	  DBG (1, "close: switch off light failed (ignored....)\n");
	}
    }

      sanei_umax_pp_close ();



  if (prev != NULL)
    prev->next = dev->next;
  else
    first_dev = dev->next;

  free (dev->buf);
  DBG (3, "close: device closed\n");

  free (handle);

}

const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
  Umax_PP_Device *dev = handle;

  if ((unsigned) option >= NUM_OPTIONS)
    {
      DBG (2, "get_option_descriptor: option %d doesn't exist\n", option);
      DEBUG ();
      return NULL;
    }

  DBG (6, "get_option_descriptor: requested option %d (%s)\n",
       option, dev->opt[option].name);

  return dev->opt + option;
}

SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option,
		     SANE_Action action, void *val, SANE_Int * info)
{
  Umax_PP_Device *dev = handle;
  SANE_Status status;
  SANE_Word w, cap;
  int dpi, rc;

  DBG (6, "control_option: option %d, action %d\n", option, action);

  if (info)
    *info = 0;

  if (dev->state == UMAX_PP_STATE_SCANNING)
    {
      DBG (2, "control_option: device is scanning\n");
      DEBUG ();
      return SANE_STATUS_DEVICE_BUSY;
    }

  if ((unsigned int) option >= NUM_OPTIONS)
    {
      DBG (2, "control_option: option doesn't exist\n");
      DEBUG ();
      return SANE_STATUS_INVAL;
    }

  cap = dev->opt[option].cap;

  if (!SANE_OPTION_IS_ACTIVE (cap))
    {
      DBG (2, "control_option: option isn't active\n");
      DEBUG ();
      return SANE_STATUS_INVAL;
    }

  DBG (6, "control_option: option <%s>, action ... ", dev->opt[option].name,
       action);

  if (action == SANE_ACTION_GET_VALUE)
    {
      DBG (6, " get value\n");
      switch (option)
	{
	  /* word options: */
	case OPT_PREVIEW:
	case OPT_GRAY_PREVIEW:
	case OPT_LAMP_CONTROL:
	case OPT_RESOLUTION:
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
	case OPT_NUM_OPTS:
	case OPT_CUSTOM_GAMMA:
	case OPT_MANUAL_GAIN:
	case OPT_GRAY_GAIN:
	case OPT_GREEN_GAIN:
	case OPT_RED_GAIN:
	case OPT_BLUE_GAIN:
	case OPT_MANUAL_HIGHLIGHT:
	case OPT_GRAY_HIGHLIGHT:
	case OPT_GREEN_HIGHLIGHT:
	case OPT_RED_HIGHLIGHT:
	case OPT_BLUE_HIGHLIGHT:

	  *(SANE_Word *) val = dev->val[option].w;
	  return SANE_STATUS_GOOD;

	  /* word-array options: */
	case OPT_GAMMA_VECTOR:
	case OPT_GAMMA_VECTOR_R:
	case OPT_GAMMA_VECTOR_G:
	case OPT_GAMMA_VECTOR_B:

	  memcpy (val, dev->val[option].wa, dev->opt[option].size);
	  return SANE_STATUS_GOOD;

	  /* string options: */
	case OPT_MODE:

	  strcpy (val, dev->val[option].s);
	  return SANE_STATUS_GOOD;
	}
    }
  else if (action == SANE_ACTION_SET_VALUE)
    {
      DBG (6, " set value\n");

      if (!SANE_OPTION_IS_SETTABLE (cap))
	{
	  DBG (2, "control_option: option can't be set\n");
	  DEBUG ();
	  return SANE_STATUS_INVAL;
	}

      status = sanei_constrain_value (dev->opt + option, val, info);

      if (status != SANE_STATUS_GOOD)
	{
	  DBG (2, "control_option: constrain_value failed (%s)\n",
	       sane_strstatus (status));
	  DEBUG ();
	  return status;
	}

      if (option == OPT_RESOLUTION)
	{
	  DBG (16, "control_option: setting resolution to %d\n",
	       *(SANE_Int *) val);
	}
      if (option == OPT_PREVIEW)
	{
	  DBG (16, "control_option: setting preview to %d\n",
	       *(SANE_Word *) val);
	}

      switch (option)
	{
	  /* (mostly) side-effect-free word options: */
	case OPT_PREVIEW:
	case OPT_GRAY_PREVIEW:
	case OPT_TL_Y:
	case OPT_BR_Y:

	  if (info)
	    *info |= SANE_INFO_RELOAD_PARAMS;

	case OPT_GRAY_GAIN:
	case OPT_GREEN_GAIN:
	case OPT_RED_GAIN:
	case OPT_BLUE_GAIN:
	case OPT_GRAY_HIGHLIGHT:
	case OPT_GREEN_HIGHLIGHT:
	case OPT_RED_HIGHLIGHT:
	case OPT_BLUE_HIGHLIGHT:

	  dev->val[option].w = *(SANE_Word *) val;
	  return SANE_STATUS_GOOD;

	  /* side-effect-free word-array options: */
	case OPT_GAMMA_VECTOR:
	case OPT_GAMMA_VECTOR_R:
	case OPT_GAMMA_VECTOR_G:
	case OPT_GAMMA_VECTOR_B:

	  memcpy (dev->val[option].wa, val, dev->opt[option].size);
	  return SANE_STATUS_GOOD;


	  /* options with side-effects: */
	case OPT_LAMP_CONTROL:
	  if (dev->state != UMAX_PP_STATE_IDLE)
	    {
	      rc = sanei_umax_pp_status ();

	      /* check if scanner busy parking */
	      if (rc == UMAX1220P_BUSY)
		{
		  DBG (2, "control_option: scanner busy\n");
		  if (info)
		    *info |= SANE_INFO_RELOAD_PARAMS;
		  return SANE_STATUS_DEVICE_BUSY;
		}
	      dev->state = UMAX_PP_STATE_IDLE;
	    }
	  dev->val[option].w = *(SANE_Word *) val;
	  if (dev->val[option].w == SANE_TRUE)
	    rc = sanei_umax_pp_lamp (1);
	  else
	    rc = sanei_umax_pp_lamp (0);
	  if (rc == UMAX1220P_TRANSPORT_FAILED)
	    return SANE_STATUS_IO_ERROR;
	  return SANE_STATUS_GOOD;

	case OPT_TL_X:
	case OPT_BR_X:
	  if (info)
	    *info |= SANE_INFO_RELOAD_PARAMS;
	  dpi = (int) (SANE_UNFIX (dev->val[OPT_RESOLUTION].w));
	  dev->val[option].w = *(SANE_Word *) val;
	  /* coords rounded to allow 32 bit IO/transfer */
	  /* at high resolution                         */
	  if (dpi >= 600)
	    {
	      if (dev->val[option].w & 0x03)
		{
		  if (info)
		    *info |= SANE_INFO_INEXACT;
		  dev->val[option].w = dev->val[option].w & 0xFFFC;
		  *(SANE_Word *) val = dev->val[option].w;
		  DBG (16, "control_option: rounding X to %d\n",
		       *(SANE_Word *) val);
		}
	    }
	  return SANE_STATUS_GOOD;



	case OPT_RESOLUTION:
	  if (info)
	    *info |= SANE_INFO_RELOAD_PARAMS;
	  /* resolution : only have 75, 150, 300, 600 and 1200 */
	  dpi = (int) (SANE_UNFIX (*(SANE_Word *) val));
	  if ((dpi != 75)
	      && (dpi != 150)
	      && (dpi != 300) && (dpi != 600) && (dpi != 1200))
	    {
	      if (dpi <= 75)
		dpi = 75;
	      else if (dpi <= 150)
		dpi = 150;
	      else if (dpi <= 300)
		dpi = 300;
	      else if (dpi <= 600)
		dpi = 600;
	      else
		dpi = 1200;
	      if (info)
		*info |= SANE_INFO_INEXACT;
	      *(SANE_Word *) val = SANE_FIX ((SANE_Word) dpi);
	    }
	  dev->val[option].w = *(SANE_Word *) val;

	  /* correct top x and bottom x if needed */
	  if (dpi >= 600)
	    {
	      dev->val[OPT_TL_X].w = dev->val[OPT_TL_X].w & 0xFFFC;
	      dev->val[OPT_BR_X].w = dev->val[OPT_BR_X].w & 0xFFF8;
	    }
	  return SANE_STATUS_GOOD;

	case OPT_MANUAL_HIGHLIGHT:
	  w = *(SANE_Word *) val;

	  if (w == dev->val[OPT_MANUAL_HIGHLIGHT].w)
	    return SANE_STATUS_GOOD;	/* no change */

	  if (info)
	    *info |= SANE_INFO_RELOAD_OPTIONS;

	  dev->val[OPT_MANUAL_HIGHLIGHT].w = w;

	  if (w == SANE_TRUE)
	    {
	      const char *mode = dev->val[OPT_MODE].s;

	      if (strcmp (mode, "Gray") == 0)
		dev->opt[OPT_GRAY_HIGHLIGHT].cap &= ~SANE_CAP_INACTIVE;
	      else if (strcmp (mode, "Color") == 0)
		{
		  dev->opt[OPT_GRAY_HIGHLIGHT].cap |= SANE_CAP_INACTIVE;
		  dev->opt[OPT_RED_HIGHLIGHT].cap &= ~SANE_CAP_INACTIVE;
		  dev->opt[OPT_GREEN_HIGHLIGHT].cap &= ~SANE_CAP_INACTIVE;
		  dev->opt[OPT_BLUE_HIGHLIGHT].cap &= ~SANE_CAP_INACTIVE;
		}
	    }
	  else
	    {
	      dev->opt[OPT_GRAY_HIGHLIGHT].cap |= SANE_CAP_INACTIVE;
	      dev->opt[OPT_RED_HIGHLIGHT].cap |= SANE_CAP_INACTIVE;
	      dev->opt[OPT_GREEN_HIGHLIGHT].cap |= SANE_CAP_INACTIVE;
	      dev->opt[OPT_BLUE_HIGHLIGHT].cap |= SANE_CAP_INACTIVE;
	    }
	  return SANE_STATUS_GOOD;



	case OPT_MANUAL_GAIN:
	  w = *(SANE_Word *) val;

	  if (w == dev->val[OPT_MANUAL_GAIN].w)
	    return SANE_STATUS_GOOD;	/* no change */

	  if (info)
	    *info |= SANE_INFO_RELOAD_OPTIONS;

	  dev->val[OPT_MANUAL_GAIN].w = w;

	  if (w == SANE_TRUE)
	    {
	      const char *mode = dev->val[OPT_MODE].s;

	      if (strcmp (mode, "Gray") == 0)
		dev->opt[OPT_GRAY_GAIN].cap &= ~SANE_CAP_INACTIVE;
	      else if (strcmp (mode, "Color") == 0)
		{
		  dev->opt[OPT_GRAY_GAIN].cap |= SANE_CAP_INACTIVE;
		  dev->opt[OPT_RED_GAIN].cap &= ~SANE_CAP_INACTIVE;
		  dev->opt[OPT_GREEN_GAIN].cap &= ~SANE_CAP_INACTIVE;
		  dev->opt[OPT_BLUE_GAIN].cap &= ~SANE_CAP_INACTIVE;
		}
	    }
	  else
	    {
	      dev->opt[OPT_GRAY_GAIN].cap |= SANE_CAP_INACTIVE;
	      dev->opt[OPT_RED_GAIN].cap |= SANE_CAP_INACTIVE;
	      dev->opt[OPT_GREEN_GAIN].cap |= SANE_CAP_INACTIVE;
	      dev->opt[OPT_BLUE_GAIN].cap |= SANE_CAP_INACTIVE;
	    }
	  return SANE_STATUS_GOOD;




	case OPT_CUSTOM_GAMMA:
	  w = *(SANE_Word *) val;

	  if (w == dev->val[OPT_CUSTOM_GAMMA].w)
	    return SANE_STATUS_GOOD;	/* no change */

	  if (info)
	    *info |= SANE_INFO_RELOAD_OPTIONS;

	  dev->val[OPT_CUSTOM_GAMMA].w = w;

	  if (w == SANE_TRUE)
	    {
	      const char *mode = dev->val[OPT_MODE].s;

	      if (strcmp (mode, "Gray") == 0)
		{
		  dev->opt[OPT_GAMMA_VECTOR].cap &= ~SANE_CAP_INACTIVE;
		  sanei_umax_pp_gamma (NULL, dev->val[OPT_GAMMA_VECTOR].wa,
				       NULL);
		}
	      else if (strcmp (mode, "Color") == 0)
		{
		  dev->opt[OPT_GAMMA_VECTOR].cap &= ~SANE_CAP_INACTIVE;
		  dev->opt[OPT_GAMMA_VECTOR_R].cap &= ~SANE_CAP_INACTIVE;
		  dev->opt[OPT_GAMMA_VECTOR_G].cap &= ~SANE_CAP_INACTIVE;
		  dev->opt[OPT_GAMMA_VECTOR_B].cap &= ~SANE_CAP_INACTIVE;
		  sanei_umax_pp_gamma (dev->val[OPT_GAMMA_VECTOR_R].wa,
				       dev->val[OPT_GAMMA_VECTOR_G].wa,
				       dev->val[OPT_GAMMA_VECTOR_B].wa);
		}
	    }
	  else
	    {
	      dev->opt[OPT_GAMMA_VECTOR].cap |= SANE_CAP_INACTIVE;
	      dev->opt[OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
	      dev->opt[OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
	      dev->opt[OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;
	      sanei_umax_pp_gamma (NULL, NULL, NULL);
	    }

	  return SANE_STATUS_GOOD;

	case OPT_MODE:
	  {
	    char *old_val = dev->val[option].s;

	    if (old_val)
	      {
		if (strcmp (old_val, val) == 0)
		  return SANE_STATUS_GOOD;	/* no change */

		free (old_val);
	      }

	    if (info)
	      *info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;

	    dev->val[option].s = strdup (val);

	    dev->opt[OPT_CUSTOM_GAMMA].cap |= SANE_CAP_INACTIVE;
	    dev->opt[OPT_GAMMA_VECTOR].cap |= SANE_CAP_INACTIVE;
	    dev->opt[OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
	    dev->opt[OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
	    dev->opt[OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;
	    sanei_umax_pp_gamma (NULL, NULL, NULL);


	    if (dev->val[OPT_CUSTOM_GAMMA].w == SANE_TRUE)
	      {
		if (strcmp (val, "Gray") == 0)
		  {
		    dev->opt[OPT_GAMMA_VECTOR].cap &= ~SANE_CAP_INACTIVE;
		    sanei_umax_pp_gamma (NULL, dev->val[OPT_GAMMA_VECTOR].wa,
					 NULL);
		  }
		else if (strcmp (val, "Color") == 0)
		  {
		    dev->opt[OPT_GAMMA_VECTOR].cap &= ~SANE_CAP_INACTIVE;
		    dev->opt[OPT_GAMMA_VECTOR_R].cap &= ~SANE_CAP_INACTIVE;
		    dev->opt[OPT_GAMMA_VECTOR_G].cap &= ~SANE_CAP_INACTIVE;
		    dev->opt[OPT_GAMMA_VECTOR_B].cap &= ~SANE_CAP_INACTIVE;
		    sanei_umax_pp_gamma (dev->val[OPT_GAMMA_VECTOR_R].wa,
					 dev->val[OPT_GAMMA_VECTOR_G].wa,
					 dev->val[OPT_GAMMA_VECTOR_B].wa);
		  }
	      }

	    /* rebuild OPT HIGHLIGHT */
	    dev->opt[OPT_GRAY_HIGHLIGHT].cap |= SANE_CAP_INACTIVE;
	    dev->opt[OPT_RED_HIGHLIGHT].cap |= SANE_CAP_INACTIVE;
	    dev->opt[OPT_GREEN_HIGHLIGHT].cap |= SANE_CAP_INACTIVE;
	    dev->opt[OPT_BLUE_HIGHLIGHT].cap |= SANE_CAP_INACTIVE;


	    if (dev->val[OPT_MANUAL_HIGHLIGHT].w == SANE_TRUE)
	      {
		if (strcmp (val, "Gray") == 0)
		  dev->opt[OPT_GRAY_HIGHLIGHT].cap &= ~SANE_CAP_INACTIVE;
		else if (strcmp (val, "Color") == 0)
		  {
		    dev->opt[OPT_RED_HIGHLIGHT].cap &= ~SANE_CAP_INACTIVE;
		    dev->opt[OPT_GREEN_HIGHLIGHT].cap &= ~SANE_CAP_INACTIVE;
		    dev->opt[OPT_BLUE_HIGHLIGHT].cap &= ~SANE_CAP_INACTIVE;
		  }
	      }

	    /* rebuild OPT GAIN */
	    dev->opt[OPT_GRAY_GAIN].cap |= SANE_CAP_INACTIVE;
	    dev->opt[OPT_RED_GAIN].cap |= SANE_CAP_INACTIVE;
	    dev->opt[OPT_GREEN_GAIN].cap |= SANE_CAP_INACTIVE;
	    dev->opt[OPT_BLUE_GAIN].cap |= SANE_CAP_INACTIVE;


	    if (dev->val[OPT_MANUAL_GAIN].w == SANE_TRUE)
	      {
		if (strcmp (val, "Gray") == 0)
		  dev->opt[OPT_GRAY_GAIN].cap &= ~SANE_CAP_INACTIVE;
		else if (strcmp (val, "Color") == 0)
		  {
		    dev->opt[OPT_RED_GAIN].cap &= ~SANE_CAP_INACTIVE;
		    dev->opt[OPT_GREEN_GAIN].cap &= ~SANE_CAP_INACTIVE;
		    dev->opt[OPT_BLUE_GAIN].cap &= ~SANE_CAP_INACTIVE;
		  }
	      }

	    return SANE_STATUS_GOOD;
	  }
	}
    }


  DBG (2, "control_option: unknown action %d \n", action);
  DEBUG ();
  return SANE_STATUS_INVAL;
}


SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters * params)
{
  Umax_PP_Device *dev = handle;
  int dpi, remain;

  memset (&(dev->params), 0, sizeof (dev->params));
  DBG (64, "sane_get_parameters\n");

  /* color/gray */
  if (strcmp (dev->val[OPT_MODE].s, "Gray") == 0)
    dev->color = UMAX_PP_MODE_GRAYSCALE;
  else
    dev->color = UMAX_PP_MODE_COLOR;

  /* highlight control */
  if (dev->val[OPT_MANUAL_HIGHLIGHT].w == SANE_TRUE)
    {
      if (dev->color == UMAX_PP_MODE_GRAYSCALE)
	{
	  dev->red_highlight = 0;
	  dev->green_highlight = (int) (dev->val[OPT_GRAY_HIGHLIGHT].w);
	  dev->blue_highlight = 0;
	}
      else
	{
	  dev->red_highlight = (int) (dev->val[OPT_RED_HIGHLIGHT].w);
	  dev->green_highlight = (int) (dev->val[OPT_GREEN_HIGHLIGHT].w);
	  dev->blue_highlight = (int) (dev->val[OPT_BLUE_HIGHLIGHT].w);
	}
    }
  else
    {
      dev->red_highlight = 2;
      dev->green_highlight = 2;
      dev->blue_highlight = 2;
    }

  /* gain control */
  if (dev->val[OPT_MANUAL_GAIN].w == SANE_TRUE)
    {
      if (dev->color == UMAX_PP_MODE_GRAYSCALE)
	{
	  dev->red_gain = 0;
	  dev->green_gain = (int) (dev->val[OPT_GRAY_GAIN].w);
	  dev->blue_gain = 0;
	}
      else
	{
	  dev->red_gain = (int) (dev->val[OPT_RED_GAIN].w);
	  dev->green_gain = (int) (dev->val[OPT_GREEN_GAIN].w);
	  dev->blue_gain = (int) (dev->val[OPT_BLUE_GAIN].w);
	}
    }
  else
    {
      dev->red_gain = red_gain;
      dev->green_gain = green_gain;
      dev->blue_gain = blue_gain;
    }

  /* geometry */
  dev->TopX = dev->val[OPT_TL_X].w;
  dev->TopY = dev->val[OPT_TL_Y].w;
  dev->BottomX = dev->val[OPT_BR_X].w;
  dev->BottomY = dev->val[OPT_BR_Y].w;

  /* resolution : only have 75, 150, 300, 600 and 1200 */
  dpi = (int) (SANE_UNFIX (dev->val[OPT_RESOLUTION].w));
  if (dpi <= 75)
    dpi = 75;
  else if (dpi <= 150)
    dpi = 150;
  else if (dpi <= 300)
    dpi = 300;
  else if (dpi <= 600)
    dpi = 600;
  else
    dpi = 1200;
  dev->dpi = dpi;

  DBG (16, "sane_get_parameters: dpi set to %d\n", dpi);

  /* for highest resolutions , width must be aligned on 32 bit word */
  if (dpi >= 600)
    {
      remain = (dev->BottomX - dev->TopX) & 0x03;
      if (remain)
	{
	  DBG (64, "sane_get_parameters: %d-%d -> remain is %d\n",
	       dev->BottomX, dev->TopX, remain);
	  if (dev->BottomX + remain < 5100)
	    dev->BottomX += remain;
	  else
	    {
	      remain -= (5100 - dev->BottomX);
	      dev->BottomX = 5100;
	      dev->TopX -= remain;
	    }
	}
    }

  if (dev->val[OPT_PREVIEW].w == SANE_TRUE)
    {

      if (dev->val[OPT_GRAY_PREVIEW].w == SANE_TRUE)
	{
	  DBG (16, "sane_get_parameters: gray preview\n");
	  dev->color = UMAX_PP_MODE_GRAYSCALE;
	  dev->params.format = SANE_FRAME_GRAY;
	}
      else
	{
	  DBG (16, "sane_get_parameters: color preview\n");
	  dev->color = UMAX_PP_MODE_COLOR;
	  dev->params.format = SANE_FRAME_RGB;
	}

      dev->dpi = 75;
      dev->TopX = 0;
      dev->TopY = 0;
      dev->BottomX = 5100;
      dev->BottomY = 7000;
    }


  /* fill params */
  dev->params.last_frame = SANE_TRUE;
  dev->params.lines = ((dev->BottomY - dev->TopY) * dev->dpi) / 600;
  if (dev->dpi == 1200)
    dpi = 600;
  else
    dpi = dev->dpi;
  dev->params.pixels_per_line = ((dev->BottomX - dev->TopX) * dpi) / 600;
  dev->params.bytes_per_line = dev->params.pixels_per_line;
  if (dev->color == UMAX_PP_MODE_COLOR)
    {
      dev->params.bytes_per_line *= 3;
      dev->params.format = SANE_FRAME_RGB;
    }
  else
    {
      dev->params.format = SANE_FRAME_GRAY;
    }
  dev->params.depth = 8;



  /* success */
  if (params != NULL)
    memcpy (params, &(dev->params), sizeof (dev->params));
  return SANE_STATUS_GOOD;

}


SANE_Status
sane_start (SANE_Handle handle)
{
  Umax_PP_Device *dev = handle;
  int rc;

  /* sanity check */
  if (dev->state == UMAX_PP_STATE_SCANNING)
    {
      DBG (2, "start: device is already scanning\n");
      DEBUG ();

      return SANE_STATUS_DEVICE_BUSY;
    }

  /* if cancelled, check if head is back home */
  if (dev->state == UMAX_PP_STATE_CANCELLED)
    {
      DBG (2, "start: checking if scanner is parking head .... \n");

      rc = sanei_umax_pp_status ();

      /* check if scanner busy parking */
      if (rc == UMAX1220P_BUSY)
	{
	  DBG (2, "start: scanner busy\n");
	  return SANE_STATUS_DEVICE_BUSY;
	}
      dev->state = UMAX_PP_STATE_IDLE;
    }


  /* get values from options */
  sane_get_parameters (handle, NULL);

  /* sets lamp flag to TRUE */
  dev->val[OPT_LAMP_CONTROL].w = SANE_TRUE;

  /* call start scan */
  if (dev->color == UMAX_PP_MODE_COLOR)
    {
      DBG (64, "start:umax_pp_start(%d,%d,%d,%d,%d,1,%X,%X)\n",
	   dev->TopX,
	   dev->TopY,
	   dev->BottomX - dev->TopX,
	   dev->BottomY - dev->TopY,
	   dev->dpi,
	   (dev->red_gain << 8) + (dev->green_gain << 4) + dev->blue_gain,
	   (dev->red_highlight << 8) + (dev->green_highlight << 4) +
	   dev->blue_highlight);

      rc = sanei_umax_pp_start (dev->TopX,
				dev->TopY,
				dev->BottomX - dev->TopX,
				dev->BottomY - dev->TopY,
				dev->dpi,
				1,
				(dev->red_gain << 8) +
				(dev->green_gain << 4) + dev->blue_gain,
				(dev->red_highlight << 8) +
				(dev->green_highlight << 4) +
				dev->blue_highlight, &(dev->bpp), &(dev->tw),
				&(dev->th));
    }
  else
    {
      DBG (64, "start:umax_pp_start(%d,%d,%d,%d,%d,0,%X,%X)\n",
	   dev->TopX,
	   dev->TopY,
	   dev->BottomX - dev->TopX,
	   dev->BottomY - dev->TopY, dev->dpi, dev->green_gain << 4,
	   dev->BottomY - dev->TopY, dev->dpi, dev->green_highlight << 4);
      rc = sanei_umax_pp_start (dev->TopX,
				dev->TopY,
				dev->BottomX - dev->TopX,
				dev->BottomY - dev->TopY,
				dev->dpi,
				0,
				dev->gray_gain << 4,
				dev->gray_highlight << 4,
				&(dev->bpp), &(dev->tw), &(dev->th));
    }

  if (rc != UMAX1220P_OK)
    {
      DBG (2, "start: failure\n");
      DEBUG ();

      return SANE_STATUS_IO_ERROR;
    }

  /* scan started, no bytes read */
  dev->state = UMAX_PP_STATE_SCANNING;
  dev->buflen = 0;
  dev->bufread = 0;
  dev->read = 0;

  /* OK .... */
  return SANE_STATUS_GOOD;

}


SANE_Status
sane_read (SANE_Handle handle, SANE_Byte * buf, SANE_Int max_len,
	   SANE_Int * len)
{
  Umax_PP_Device *dev = handle;
  long int length;
  int last, rc;
  int x, y, nl, ll;
  SANE_Byte *lbuf;


  /* no data until further notice */
  *len = 0;
  DBG (64, "sane_read(max_len=%d)\n", max_len);
  ll = dev->tw * dev->bpp;

  /* sanity check */
  if (dev->state == UMAX_PP_STATE_CANCELLED)
    {
      DBG (2, "read: scan cancelled\n");
      DEBUG ();

      return SANE_STATUS_CANCELLED;
    }

  /* eof test */
  if (dev->read >= dev->th * ll)
    {
      DBG (2, "read: end of scan reached\n");
      return SANE_STATUS_EOF;
    }

  /* read data from scanner if needed */
  if ((dev->buflen == 0) || (dev->bufread >= dev->buflen))
    {
      DBG (64, "sane_read: reading data from scanner\n");
      /* absolute number of bytes needed */
      length = ll * dev->th - dev->read;

      /* does all fit in a single last read ? */
      if (length <= dev->bufsize)
	{
	  last = 1;
	}
      else
	{
	  last = 0;
	  /* round number of scan lines */
	  length = (dev->bufsize / ll) * ll;
	}

      /* now do the read */
      rc = sanei_umax_pp_read (length, dev->tw, dev->dpi, last, dev->buf);
      if (rc != UMAX1220P_OK)
	return SANE_STATUS_IO_ERROR;
      dev->bufread = 0;
      dev->buflen = length;
      DBG (64, "sane_read: got %ld bytes of data from scanner\n", length);

      /* rounding to line size: needed by data reordering */
      nl = length / ll;

      /* re order data into RGB */
      if (dev->color == UMAX_PP_MODE_COLOR)
	{
	  DBG (64, "sane_read: reordering %d bytes of data (lines=%d)\n",
	       length, nl);
	  lbuf = (SANE_Byte *) malloc (dev->bufsize);
	  if (lbuf == NULL)
	    {
	      DBG (1, "sane_read: couldn't allocate %ld bytes\n",
		   dev->bufsize);
	      return SANE_STATUS_NO_MEM;
	    }
	  /* this loop will be optimized when everything else will be working */
	  for (y = 0; y < nl; y++)
	    {
	      for (x = 0; x < dev->tw; x++)
		{
		  lbuf[x * dev->bpp + y * ll] =
		    dev->buf[dev->bufread + x + y * ll + 2 * dev->tw];
		  lbuf[x * dev->bpp + y * ll + 1] =
		    dev->buf[dev->bufread + x + y * ll + dev->tw];
		  lbuf[x * dev->bpp + y * ll + 2] =
		    dev->buf[dev->bufread + x + y * ll];
		}
	    }
	  /* avoids memcopy */
	  free (dev->buf);
	  dev->buf = lbuf;
	}
    }

  /* how much get data we can get from memory buffer */
  length = dev->buflen - dev->bufread;
  DBG (64, "sane_read: %ld bytes of data available\n", length);
  if (length > max_len)
    length = max_len;



  memcpy (buf, dev->buf + dev->bufread, length);
  *len = length;
  dev->bufread += length;
  dev->read += length;
  DBG (64, "sane_read %d bytes read\n", length);

  return SANE_STATUS_GOOD;

}

void
sane_cancel (SANE_Handle handle)
{
  Umax_PP_Device *dev = handle;
  int rc;

  DBG (64, "sane_cancel\n");
  if (dev->state == UMAX_PP_STATE_IDLE)
    {
      DBG (3, "cancel: cancelling idle \n");
      return;
    }
  if (dev->state == UMAX_PP_STATE_SCANNING)
    {
      DBG (3, "cancel: stopping current scan\n");

      dev->buflen = 0;

      dev->state = UMAX_PP_STATE_CANCELLED;
      sanei_umax_pp_cancel ();
    }
  else
    {
      /* STATE_CANCELLED */
      DBG (2, "cancel: checking if scanner is still parking head .... \n");

      rc = sanei_umax_pp_status ();

      /* check if scanner busy parking */
      if (rc == UMAX1220P_BUSY)
	{
	  DBG (2, "cancel: scanner busy\n");
	  return;
	}
      dev->state = UMAX_PP_STATE_IDLE;
    }
}

SANE_Status
sane_set_io_mode (SANE_Handle handle, SANE_Bool non_blocking)
{
  DBG (129, "unused arg: handle = %p, non_blocking = %d\n",
       handle, (int) non_blocking);

  DBG (2, "set_io_mode: not supported\n");

  return SANE_STATUS_UNSUPPORTED;
}

SANE_Status
sane_get_select_fd (SANE_Handle handle, SANE_Int * fd)
{

  DBG (129, "unused arg: handle = %p, fd = %p\n", handle, fd);

  DBG (2, "get_select_fd: not supported\n");

  return SANE_STATUS_UNSUPPORTED;
}
