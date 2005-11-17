/* lexmark.c: SANE backend for Lexmark scanners.

   (C) 2003-2004 Lexmark International, Inc. (Original Source code)
   (C) 2005 Fred Odendaal
   
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

   **************************************************************************/

#include "../include/sane/config.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/time.h>


#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"
#include "../include/sane/saneopts.h"
#include "../include/sane/sanei_config.h"
#include "../include/sane/sanei_usb.h"

#include "lexmark.h"
#include "../include/sane/sanei_backend.h"



#define LEXMARK_CONFIG_FILE "lexmark.conf"
#define BUILD 0
#define MAX_OPTION_STRING_SIZE 255
#define MM_PER_INCH 25.4

static Lexmark_Device *first_lexmark_device = 0;
static SANE_Int num_lexmark_device = 0;
static const SANE_Device **sane_device_list = NULL;

/* Program globals F.O - Should this be per device?*/
static SANE_Bool initialized = SANE_FALSE;

static SANE_String_Const mode_list[] = {
  SANE_VALUE_SCAN_MODE_COLOR,
  SANE_VALUE_SCAN_MODE_GRAY,
  SANE_VALUE_SCAN_MODE_LINEART, 
  NULL
};

/* possible resolutions are: 75x75, 150x150, 300x300, 600X600, 600X1200 */

static SANE_Int dpi_list[] = {
  5, 75, 150, 300, 600, 1200
};

static SANE_String_Const size_list[] = {
  "Wallet", "3x5", "4x6", "5x7", "8x10", "Letter", NULL
};

static SANE_Range threshold_range = {
  SANE_FIX(0.0),               /* minimum */
  SANE_FIX(100.0),             /* maximum */
  SANE_FIX(1.0)                /* quantization */
};


/* static functions */
static SANE_Status init_options (Lexmark_Device * lexmark_device);
static SANE_Status attachLexmark (SANE_String_Const devname);

SANE_Status
init_options (Lexmark_Device * lexmark_device)
{

  SANE_Option_Descriptor *od;

  DBG (2, "init_options: lexmark_device = %p\n", (void *) lexmark_device);

  /* number of options */
  od = &(lexmark_device->opt[OPT_NUM_OPTS]);
  od->name = SANE_NAME_NUM_OPTIONS;
  od->title = SANE_TITLE_NUM_OPTIONS;
  od->desc = SANE_DESC_NUM_OPTIONS;
  od->type = SANE_TYPE_INT;
  od->unit = SANE_UNIT_NONE;
  od->size = sizeof (SANE_Word);
  od->cap = SANE_CAP_SOFT_DETECT;
  od->constraint_type = SANE_CONSTRAINT_NONE;
  od->constraint.range = 0;
  lexmark_device->val[OPT_NUM_OPTS].w = NUM_OPTIONS;

  /* mode - sets the scan mode: Color, Gray, or Line Art */
  od = &(lexmark_device->opt[OPT_MODE]);
  od->name = SANE_NAME_SCAN_MODE;
  od->title = SANE_TITLE_SCAN_MODE;
  od->desc = SANE_DESC_SCAN_MODE;;
  od->type = SANE_TYPE_STRING;
  od->unit = SANE_UNIT_NONE;
  od->size = MAX_OPTION_STRING_SIZE;
  od->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_SOFT_SELECT;
  od->constraint_type = SANE_CONSTRAINT_STRING_LIST;
  od->constraint.string_list = mode_list;
  lexmark_device->val[OPT_MODE].s = malloc (od->size);
  if (!lexmark_device->val[OPT_MODE].s)
    return SANE_STATUS_NO_MEM;
  strcpy (lexmark_device->val[OPT_MODE].s, SANE_VALUE_SCAN_MODE_COLOR);

  /* resolution */
  od = &(lexmark_device->opt[OPT_RESOLUTION]);
  od->name = SANE_NAME_SCAN_RESOLUTION;
  od->title = SANE_TITLE_SCAN_RESOLUTION;
  od->desc = SANE_DESC_SCAN_RESOLUTION;
  od->type = SANE_TYPE_INT;
  od->unit = SANE_UNIT_DPI;
  od->size = sizeof (SANE_Word);
  od->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_SOFT_SELECT;
  od->constraint_type = SANE_CONSTRAINT_WORD_LIST;
  od->constraint.word_list = dpi_list;
  lexmark_device->val[OPT_RESOLUTION].w = 150;

  /* preview mode */
  od = &(lexmark_device->opt[OPT_PREVIEW]);
  od->name = SANE_NAME_PREVIEW;
  od->title = SANE_TITLE_PREVIEW;
  od->desc = SANE_DESC_PREVIEW;
  od->size = sizeof (SANE_Word);
  od->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_SOFT_SELECT;
  od->type = SANE_TYPE_BOOL;
  od->constraint_type = SANE_CONSTRAINT_NONE;
  lexmark_device->val[OPT_PREVIEW].w = SANE_FALSE;

  /* scan size */
  od = &(lexmark_device->opt[OPT_SCAN_SIZE]);
  od->name = SANE_NAME_PAPER_SIZE;
  od->title = SANE_TITLE_PAPER_SIZE;
  od->desc = SANE_DESC_PAPER_SIZE;
  od->type = SANE_TYPE_STRING;
  od->unit = SANE_UNIT_NONE;
  od->size = MAX_OPTION_STRING_SIZE;
  od->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_SOFT_SELECT;
  od->constraint_type = SANE_CONSTRAINT_STRING_LIST;
  od->constraint.string_list = size_list;
  lexmark_device->val[OPT_SCAN_SIZE].s = malloc (od->size);
  if (!lexmark_device->val[OPT_SCAN_SIZE].s)
    return SANE_STATUS_NO_MEM;
  strcpy (lexmark_device->val[OPT_SCAN_SIZE].s, "3x5");

  /* threshold */
  od = &(lexmark_device->opt[OPT_THRESHOLD]);
  od->name = SANE_NAME_THRESHOLD;
  od->title = SANE_TITLE_THRESHOLD;
  od->desc = SANE_DESC_THRESHOLD;
  od->type = SANE_TYPE_FIXED;
  od->unit = SANE_UNIT_PERCENT;
  od->size = sizeof (SANE_Fixed);
  od->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_INACTIVE;
  od->constraint_type = SANE_CONSTRAINT_RANGE;
  od->constraint.range = &threshold_range;
  lexmark_device->val[OPT_THRESHOLD].w = SANE_FIX(50.0);

  return SANE_STATUS_GOOD;
}


/***************************** SANE API ****************************/

SANE_Status
attachLexmark (SANE_String_Const devname)
{
  Lexmark_Device *lexmark_device;

  DBG (2, "attachLexmark: devname=%s\n", devname);

  for (lexmark_device = first_lexmark_device; lexmark_device;
       lexmark_device = lexmark_device->next)
    {
      if (strcmp (lexmark_device->sane.name, devname) == 0)
	return SANE_STATUS_GOOD;
    }

  lexmark_device = (Lexmark_Device *) malloc (sizeof (Lexmark_Device));
  if (lexmark_device == NULL)
    return SANE_STATUS_NO_MEM;

  lexmark_device->sane.name = strdup (devname);
  lexmark_device->sane.vendor = "Lexmark";
  lexmark_device->sane.model = "X1100";
  lexmark_device->sane.type = "flatbed scanner";

  /* Set the default resolution here */
  lexmark_device->x_dpi = 75;
  lexmark_device->y_dpi = 75;

  /* Make the pointer to the read buffer null here */
  lexmark_device->read_buffer = NULL;

  /* Set the default threshold for lineart mode here */
  lexmark_device->threshold = 0x80;

  lexmark_device->next = first_lexmark_device;
  first_lexmark_device = lexmark_device;

  num_lexmark_device++;

  return SANE_STATUS_GOOD;
}

SANE_Status
sane_init (SANE_Int * version_code, SANE_Auth_Callback authorize)
{
  FILE *fp;
  SANE_Char line[PATH_MAX];
  const char *lp;
  SANE_Int vendor, product;
  size_t len;
  SANE_Status status;
  SANE_Auth_Callback auth_callback;

  DBG_INIT ();

  DBG (1, "SANE Lexmark backend version %d.%d-%d\n", V_MAJOR, V_MINOR, BUILD);

  auth_callback = authorize;

  DBG (2, "sane_init: version_code=%p\n", (void *) version_code);

  if (version_code)
    *version_code = SANE_VERSION_CODE (V_MAJOR, V_MINOR, BUILD);

  status = sanei_lexmark_x1100_init ();

  if (status != SANE_STATUS_GOOD)
    return status;

  fp = sanei_config_open (LEXMARK_CONFIG_FILE);
  if (!fp)
    {
      /* sanei_lexmark_x1100_destroy (); */
      return SANE_STATUS_ACCESS_DENIED;
    }

  while (sanei_config_read (line, PATH_MAX, fp))
    {
      /* ignore comments */
      if (line[0] == '#')
	continue;
      len = strlen (line);

      /* delete newline characters at end */
      if (line[len - 1] == '\n')
	line[--len] = '\0';

      /* lp = (SANE_String) sanei_config_skip_whitespace (line); */
      lp = sanei_config_skip_whitespace (line);
      /* skip empty lines */
      if (*lp == 0)
	continue;

      if (sscanf (lp, "usb %i %i", &vendor, &product) == 2)
	;
      else if (strncmp ("libusb", lp, 6) == 0)
	;
      else if ((strncmp ("usb", lp, 3) == 0) && isspace (lp[3]))
	{
	  lp += 3;
	  /* lp = (SANE_String) sanei_config_skip_whitespace (lp); */
	  lp = sanei_config_skip_whitespace (lp);
	}
      else
	continue;

      sanei_usb_attach_matching_devices (lp, attachLexmark);

    }

  fclose (fp);

  initialized = SANE_TRUE;

  return SANE_STATUS_GOOD;
}

void
sane_exit (void)
{
  Lexmark_Device *lexmark_device, *next_lexmark_device;

  DBG (2, "sane_exit\n");

  if (!initialized)
    return;

  for (lexmark_device = first_lexmark_device; lexmark_device;
       lexmark_device = next_lexmark_device)
    {
      next_lexmark_device = lexmark_device->next;
      sanei_lexmark_x1100_destroy (lexmark_device);
      free (lexmark_device);
    }

  if (sane_device_list)
    free (sane_device_list);

  initialized = SANE_FALSE;

  return;
}

SANE_Status
sane_get_devices (const SANE_Device *** device_list, SANE_Bool local_only)
{
  Lexmark_Device *lexmark_device;
  SANE_Int index;

  DBG (2, "sane_get_devices: device_list=%p, local_only=%d\n",
       (void *) device_list, local_only);

  if (!initialized)
    return SANE_STATUS_INVAL;

  if (sane_device_list)
    free (sane_device_list);

  sane_device_list = malloc ((num_lexmark_device + 1) *
			     sizeof (sane_device_list[0]));

  if (!sane_device_list)
    return SANE_STATUS_NO_MEM;

  index = 0;
  for (lexmark_device = first_lexmark_device; lexmark_device;
       lexmark_device = lexmark_device->next)
    {
      sane_device_list[index] = &(lexmark_device->sane);
      index++;
    }
  sane_device_list[index] = 0;

  *device_list = sane_device_list;


  return SANE_STATUS_GOOD;
}

SANE_Status
sane_open (SANE_String_Const devicename, SANE_Handle * handle)
{
  Lexmark_Device *lexmark_device;
  SANE_Status status;

  DBG (2, "sane_open: devicename=\"%s\", handle=%p\n", devicename, 
       (void *) handle);

  if (!initialized)
    {
      DBG (2, "sane_open: not initialized\n");
      return SANE_STATUS_INVAL;
    }

  if (!handle)
    {
      DBG (2, "sane_open: no handle\n");
      return SANE_STATUS_INVAL;
    }

  for (lexmark_device = first_lexmark_device; lexmark_device;
       lexmark_device = lexmark_device->next)
    {
      DBG (2, "sane_open: devname from list: %s\n",
	   lexmark_device->sane.name);
      if (strcmp (devicename, lexmark_device->sane.name) == 0)
	break;
    }

  *handle = lexmark_device;

  if (!lexmark_device)
    {
      DBG (2, "sane_open: Not a Lexmark device\n");
      return SANE_STATUS_INVAL;
    }

  status = init_options (lexmark_device);
  if (status != SANE_STATUS_GOOD)
    return status;

/*   status = llpddk_open_device (lexmark_device->sane.name); */
  status =
    sanei_lexmark_x1100_open_device (lexmark_device->sane.name, &(lexmark_device->devnum));
  if (status != SANE_STATUS_GOOD)
    return status;

  return SANE_STATUS_GOOD;
}

void
sane_close (SANE_Handle handle)
{
  Lexmark_Device *lexmark_device;

  DBG (2, "sane_close: handle=%p\n", (void *) handle);

  if (!initialized)
    return;

  for (lexmark_device = first_lexmark_device; lexmark_device;
       lexmark_device = lexmark_device->next)
    {
      if (lexmark_device == handle)
	break;
    }

  if (!lexmark_device)
    return;

/*   llpddk_close_device (); */
  sanei_lexmark_x1100_close_device (lexmark_device->devnum);

  return;
}

const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
  Lexmark_Device *lexmark_device;

  DBG (2, "sane_get_option_descriptor: handle=%p, option = %d\n",
       (void *) handle, option);

  if (!initialized)
    return NULL;

  /* Check for valid option number */
  if ((option < 0) || (option >= NUM_OPTIONS))
    return NULL;

  for (lexmark_device = first_lexmark_device; lexmark_device;
       lexmark_device = lexmark_device->next)
    {
      if (lexmark_device == handle)
	break;
    }

  if (!lexmark_device)
    return NULL;

  return &(lexmark_device->opt[option]);
}

SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option, SANE_Action action,
		     void *value, SANE_Int * info)
{
  Lexmark_Device *lexmark_device;
  SANE_Status status;

  DBG (2, "sane_control_option: handle=%p, opt=%d, act=%d, val=%p, info=%p\n",
       (void *) handle, option, action, (void *) value, (void *) info);

  if (!initialized)
    return SANE_STATUS_INVAL;

  for (lexmark_device = first_lexmark_device; lexmark_device;
       lexmark_device = lexmark_device->next)
    {
      if (lexmark_device == handle)
	break;
    }

  if (!lexmark_device)
    return SANE_STATUS_INVAL;

  if (value == NULL)
    return SANE_STATUS_INVAL;

  if (info != NULL)
    *info = 0;

  if (option < 0 || option >= NUM_OPTIONS)
    return SANE_STATUS_INVAL;

  if (lexmark_device->opt[option].type == SANE_TYPE_GROUP)
    return SANE_STATUS_INVAL;

  switch (action)
    {
    case SANE_ACTION_SET_AUTO:

      if (!SANE_OPTION_IS_SETTABLE (lexmark_device->opt[option].cap))
	return SANE_STATUS_INVAL;
      if (!(lexmark_device->opt[option].cap & SANE_CAP_AUTOMATIC))
	return SANE_STATUS_INVAL;
      break;

    case SANE_ACTION_SET_VALUE:

      if (!SANE_OPTION_IS_SETTABLE (lexmark_device->opt[option].cap))
	return SANE_STATUS_INVAL;

      /* Make sure boolean values are only TRUE or FALSE */
      if (lexmark_device->opt[option].type == SANE_TYPE_BOOL)
	{
	  if (!
	      ((*(SANE_Bool *) value == SANE_FALSE)
	       || (*(SANE_Bool *) value == SANE_TRUE)))
	    return SANE_STATUS_INVAL;
	}

      /* Check range constraints */
      if (lexmark_device->opt[option].constraint_type ==
	  SANE_CONSTRAINT_RANGE)
	{
	  status =
	    sanei_constrain_value (&(lexmark_device->opt[option]), value,
				   info);
	  if (status != SANE_STATUS_GOOD)
	    {
	      DBG(2, "SANE_CONTROL_OPTION: Bad value for range\n");
	      return SANE_STATUS_INVAL;
	    }
	}

      switch (option)
	{
	case OPT_NUM_OPTS:
	case OPT_RESOLUTION:	
	  lexmark_device->val[option].w = *(SANE_Int *) value;
	  sane_get_parameters (handle, 0);
	  break;
	case OPT_THRESHOLD:
	  lexmark_device->val[option].w = *(SANE_Fixed *) value;
	  lexmark_device->threshold = 0xFF * (lexmark_device->val[option].w/100);
	  break;
	case OPT_SCAN_SIZE:
	  strcpy (lexmark_device->val[option].s, value);
	  break;
	case OPT_PREVIEW:
	  lexmark_device->val[option].w = *(SANE_Int *) value;
	  if (*(SANE_Word *) value)
	    {
	      lexmark_device->y_dpi = lexmark_device->val[OPT_RESOLUTION].w;
	      lexmark_device->val[OPT_RESOLUTION].w = 75;
	    }
	  else
	    {
	      lexmark_device->val[OPT_RESOLUTION].w = lexmark_device->y_dpi;
	    }
	  if (info)
	    *info |= SANE_INFO_RELOAD_OPTIONS;
	  sane_get_parameters (handle, 0);
	  if (info)
	    *info |= SANE_INFO_RELOAD_PARAMS;
	  return SANE_STATUS_GOOD;
	  lexmark_device->val[option].w = *(SANE_Int *) value;
	  break;
	case OPT_MODE:
	  strcpy (lexmark_device->val[option].s, value); 
	  if (strcmp (lexmark_device->val[option].s, 
		      SANE_VALUE_SCAN_MODE_LINEART) == 0)
	    {
	      lexmark_device->opt[OPT_THRESHOLD].cap &= ~SANE_CAP_INACTIVE;
	    }
	  else
	    {
	      lexmark_device->opt[OPT_THRESHOLD].cap |= SANE_CAP_INACTIVE;
	    }
	  if (info)
	    *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
	  break;
	}

      if (info != NULL)
	*info |= SANE_INFO_RELOAD_PARAMS;

      break;
     
    case SANE_ACTION_GET_VALUE:

      switch (option)
	{
	case OPT_NUM_OPTS:
	case OPT_RESOLUTION:
	case OPT_PREVIEW:
	  *(SANE_Int *) value = lexmark_device->val[option].w;
	  DBG(2,"Option value = %d\n", *(SANE_Int *) value);
	  break;
	case OPT_THRESHOLD:
	  *(SANE_Fixed *) value = lexmark_device->val[option].w;
	  DBG(2,"Option value = %f\n", SANE_UNFIX(*(SANE_Fixed *) value));
	  break;
	case OPT_MODE:
	case OPT_SCAN_SIZE:
	  strcpy (value, lexmark_device->val[option].s);
	  break;
	default:
	  return SANE_STATUS_INVAL;
	}
      break;

    default:
      return SANE_STATUS_INVAL;

    }

  return SANE_STATUS_GOOD;
}


SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters * params)
{
  Lexmark_Device *lexmark_device;
  SANE_Parameters *device_params;
  SANE_Int xres, yres, width_px, height_px;
  SANE_Int channels, bitsperchannel;
  double width, height;
  SANE_Bool isColourScan;

  DBG (2, "sane_get_parameters: handle=%p, params=%p\n", (void *) handle, 
       (void *) params);

  if (!initialized)
    return SANE_STATUS_INVAL;

  for (lexmark_device = first_lexmark_device; lexmark_device;
       lexmark_device = lexmark_device->next)
    {
      if (lexmark_device == handle)
	break;
    }

  if (!lexmark_device)
    return SANE_STATUS_INVAL;

  yres = lexmark_device->val[OPT_RESOLUTION].w;
  if (yres == 1200)
    xres = 600;
  else
    xres = yres;

  /* 24 bit colour = 8 bits/channel for each of the RGB channels */
  channels = 3;
  bitsperchannel = 8;
  isColourScan = SANE_TRUE;

  /* If not color there is only 1 channel */
  if (strcmp (lexmark_device->val[OPT_MODE].s, SANE_VALUE_SCAN_MODE_COLOR) 
      != 0)
    {
      channels = 1;
      bitsperchannel = 8;
      isColourScan = SANE_FALSE;
    }


  /* size in inches */
  if (strcmp (lexmark_device->val[OPT_SCAN_SIZE].s, "Wallet") == 0)
    {
      width = 2.50;
      height = 3.50;
    }
  else if (strcmp (lexmark_device->val[OPT_SCAN_SIZE].s, "3x5") == 0)
    {
      width = 3.00;
      height = 5.00;
    }
  else if (strcmp (lexmark_device->val[OPT_SCAN_SIZE].s, "4x6") == 0)
    {
      width = 4.00;
      height = 6.00;
    }
  else if (strcmp (lexmark_device->val[OPT_SCAN_SIZE].s, "5x7") == 0)
    {
      width = 5.00;
      height = 7.00;
    }
  else if (strcmp (lexmark_device->val[OPT_SCAN_SIZE].s, "8x10") == 0)
    {
      width = 8.00;
      height = 10.00;
    }
  else if (strcmp (lexmark_device->val[OPT_SCAN_SIZE].s, "Letter") == 0)
    {
      width = 8.50;
      height = 11.00;
    }
  else
    {
      DBG (2, "sane_get_parameters - ERROR: Unknown Scan Size option\n");
      return SANE_STATUS_INVAL;
    }

  /* in pixels */
  width_px = (SANE_Int) (width * xres);
  /* not sure why this is so - if its odd add one */
  if ((width_px & 0x01) == 1)
    width_px = width_px + 1;

  height_px = (SANE_Int) (height * yres);

  /* Stashed with device record for easy retrieval later */
  lexmark_device->pixel_width = width_px;
  lexmark_device->pixel_height = height_px;

  /* data_size is the size transferred from the scanner to the backend */
  /* therefor bitsperchannel is the same for gray and lineart */
  lexmark_device->data_size =
    width_px * height_px * channels * (bitsperchannel / 8);
  DBG (2, "sane_get_parameters: Data size determined as %lx\n",
       lexmark_device->data_size);

  /* we must tell the front end the bitsperchannel for lineart is really */
  /* only 1, so it can calculate the correct image size */
  /* If not color there is only 1 channel */
  if (strcmp (lexmark_device->val[OPT_MODE].s, SANE_VALUE_SCAN_MODE_LINEART) 
      == 0)
    {
      bitsperchannel = 1;
    }

  device_params = &(lexmark_device->params);
  device_params->format = SANE_FRAME_RGB;
  if (channels == 1)
    device_params->format = SANE_FRAME_GRAY;
  device_params->last_frame = SANE_TRUE;
  device_params->lines = height_px;
  device_params->depth = bitsperchannel;
  device_params->pixels_per_line = width_px;
  device_params->bytes_per_line =
    (SANE_Int) (channels * device_params->pixels_per_line *
		(bitsperchannel / 8));
  if ( bitsperchannel == 1 )
    {
      device_params->bytes_per_line =  
	(SANE_Int) (device_params->pixels_per_line / 8);
      if ((device_params->pixels_per_line % 8) != 0)
	device_params->bytes_per_line++;
    }
  


  DBG (2, "sane_get_parameters: \n");
  if (device_params->format == SANE_FRAME_GRAY)
    DBG (2, "  format: SANE_FRAME_GRAY\n");
  else if (device_params->format == SANE_FRAME_RGB)
    DBG (2, "  format: SANE_FRAME_RGB\n");
  else
    DBG (2, "  format: UNKNOWN\n");
  if (device_params->last_frame == SANE_TRUE)
    DBG (2, "  last_frame: TRUE\n");
  else
    DBG (2, "  last_frame: FALSE\n");
  DBG (2, "  lines %x\n", device_params->lines);
  DBG (2, "  depth %x\n", device_params->depth);
  DBG (2, "  pixels_per_line %x\n", device_params->pixels_per_line);
  DBG (2, "  bytes_per_line %x\n", device_params->bytes_per_line);

  if (params != 0)
    {
      params->format = device_params->format;
      params->last_frame = device_params->last_frame;
      params->lines = device_params->lines;
      params->depth = device_params->depth;
      params->pixels_per_line = device_params->pixels_per_line;
      params->bytes_per_line = device_params->bytes_per_line;
    }

  return SANE_STATUS_GOOD;
}

SANE_Status
sane_start (SANE_Handle handle)
{
  Lexmark_Device *lexmark_device;
  SANE_Int offset;

  DBG (2, "sane_start: handle=%p\n", (void *) handle);

  if (!initialized)
    return SANE_STATUS_INVAL;

  for (lexmark_device = first_lexmark_device; lexmark_device;
       lexmark_device = lexmark_device->next)
    {
      if (lexmark_device == handle)
	break;
    }

  sane_get_parameters (handle, 0);

  if ((lexmark_device->params.lines == 0) ||
      (lexmark_device->params.pixels_per_line == 0) ||
      (lexmark_device->params.bytes_per_line == 0))
    {
      DBG(2, "sane_start: \n");
      DBG(2, "  ERROR: Zero size encountered in:\n");
      DBG(2, "         number of lines, bytes per line, or pixels per line\n");
      return SANE_STATUS_INVAL;
    }

  lexmark_device->device_cancelled = SANE_FALSE;
  lexmark_device->data_ctr = 0;
  lexmark_device->eof = SANE_FALSE;


  /* Need this cancel_ctr to determine how many times sane_cancel is called 
     since it is called more than once. */
  lexmark_device->cancel_ctr = 0;

  /* Find Home */
  if (sanei_lexmark_x1100_search_home_fwd (lexmark_device))
    {
      DBG (2, "sane_start: Scan head initially at home position\n");
    }
  else
    {
      /* We may have been rewound too far, so move forward the distance from
         the edge to the home position */
      /* sanei_lexmark_x1100_move_fwd_a_bit(lexmark_device); */
      sanei_lexmark_x1100_move_fwd (0x01a8, lexmark_device);
      /* Scan backwards until we find home */
      sanei_lexmark_x1100_search_home_bwd (lexmark_device);
    }

  /* At this point we're somewhere in the dot. We need to read a number of 
     lines greater than the diameter of the dot and determine how many lines 
     past the dot we've gone. We then use this information to see how far the 
     scan head must move before starting the scan. */
  offset = sanei_lexmark_x1100_find_start_line (lexmark_device->devnum);

  /* Set the shadow registers for scan with the options (resolution, mode, 
     size) set in the front end. Pass the offset so we can get the vert.
     start. */
  sanei_lexmark_x1100_set_scan_regs (lexmark_device, offset);

  if (sanei_lexmark_x1100_start_scan (lexmark_device) == SANE_STATUS_GOOD)
    {
      DBG (2, "sane_start: scan started\n");
      return SANE_STATUS_GOOD;
    }
  else
    {
      lexmark_device->device_cancelled = SANE_TRUE;
      return SANE_STATUS_INVAL;
    }
}


SANE_Status
sane_read (SANE_Handle handle, SANE_Byte * data,
	   SANE_Int max_length, SANE_Int * length)
{
  Lexmark_Device *lexmark_device;
  long bytes_read;

  DBG (2, "sane_read: handle=%p, data=%p, max_length = %d, length=%p\n",
       (void *) handle, (void *) data, max_length, (void *) length);

  if (!initialized)
    {
      DBG (2, "sane_read: Not initialized\n");
      return SANE_STATUS_INVAL;
    }

  for (lexmark_device = first_lexmark_device; lexmark_device;
       lexmark_device = lexmark_device->next)
    {
      if (lexmark_device == handle)
	break;
    }

  if (lexmark_device->device_cancelled)
    {
      DBG (2, "sane_read: Device was cancelled\n");
      /* We don't know how far we've gone, so search for home. */
      sanei_lexmark_x1100_search_home_bwd (lexmark_device);
      return SANE_STATUS_EOF;
    }

  if (!length)
    {
      DBG (2, "sane_read: NULL length pointer\n");
      return SANE_STATUS_INVAL;
    }

  *length = 0;

  if (lexmark_device->eof)
    {
      DBG (2, "sane_read: Trying to read past EOF\n");
      return SANE_STATUS_EOF;
    }

  if (!data)
    return SANE_STATUS_INVAL;

  bytes_read = sanei_lexmark_x1100_read_scan_data (data, max_length, 
						   lexmark_device);
  if (bytes_read < 0)
    return SANE_STATUS_IO_ERROR;
  else if (bytes_read == 0)
    return SANE_STATUS_EOF;
  else
    {
      *length = bytes_read;
      lexmark_device->data_ctr += bytes_read;
    }

  return SANE_STATUS_GOOD;
}

void
sane_cancel (SANE_Handle handle)
{
  Lexmark_Device *lexmark_device;
/*   ssize_t bytes_read; */
  DBG (2, "sane_cancel: handle = %p\n", (void *) handle);

  if (!initialized)
    return;


  for (lexmark_device = first_lexmark_device; lexmark_device;
       lexmark_device = lexmark_device->next)
    {
      if (lexmark_device == handle)
	break;
    }

  /*If sane_cancel called more than once, return */
  if (++lexmark_device->cancel_ctr > 1)	   
    return;

  /* Set the device flag so the next call to sane_read() can stop the scan. */
  lexmark_device->device_cancelled = SANE_TRUE;

  return;
}

SANE_Status
sane_set_io_mode (SANE_Handle handle, SANE_Bool non_blocking)
{
  Lexmark_Device *lexmark_device;

  DBG (2, "sane_set_io_mode: handle = %p, non_blocking = %d\n", (void *) handle,
       non_blocking);

  if (!initialized)
    return SANE_STATUS_INVAL;

  for (lexmark_device = first_lexmark_device; lexmark_device;
       lexmark_device = lexmark_device->next)
    {
      if (lexmark_device == handle)
	break;
    }

  if (non_blocking)
    return SANE_STATUS_UNSUPPORTED;

  return SANE_STATUS_GOOD;
}

SANE_Status
sane_get_select_fd (SANE_Handle handle, SANE_Int * fd)
{
  Lexmark_Device *lexmark_device;

  DBG (2, "sane_get_select_fd: handle = %p, fd %s 0\n", (void *) handle,
       fd ? "!=" : "=");

  if (!initialized)
    return SANE_STATUS_INVAL;

  for (lexmark_device = first_lexmark_device; lexmark_device;
       lexmark_device = lexmark_device->next)
    {
      if (lexmark_device == handle)
	break;
    }

  return SANE_STATUS_UNSUPPORTED;
}

/***************************** END OF SANE API ****************************/
