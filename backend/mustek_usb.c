/* sane - Scanner Access Now Easy.

   Copyright (C) 2000 Mustek.
   Maintained by Tom Wang <tom.wang@mustek.com.tw>

   Updates (C) 2001 by Henning Meier-Geinitz.

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

   This file implements a SANE backend for Mustek 1200UB flatbed scanners.  */

#define BUILD 5

#include "../include/sane/config.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"
#include "../include/sane/saneopts.h"
#include "mustek_usb.h"


#define BACKEND_NAME mustek_usb

#include "../include/sane/sanei_backend.h"
#include "../include/sane/sanei_config.h"
#include "../include/sane/sanei_usb.h"

#include "mustek_usb_high.c"

static int num_devices;
static Mustek_Usb_Device *first_dev;
static Mustek_Usb_Scanner *first_handle;

/* Array of newly attached devices */
static Mustek_Usb_Device **new_dev;	

/* Length of new_dev array */
static SANE_Int new_dev_len;

/* Number of entries alloced for new_dev */
static SANE_Int new_dev_alloced;

static SANE_String_Const mode_list[6];

static const SANE_Int resbit_list[] =
{
  7,
  50, 100, 150, 200, 300, 400, 600
};

static const SANE_Int resbit300_list[] =
{
  5,
  50, 100, 150, 200, 300
};

/* NOTE: This is used for Brightness, Contrast, Threshold, AutoBackAdj
   and 0 is the default value */
   
static const SANE_Range percentage_range =
{
	-100 << SANE_FIXED_SCALE_SHIFT,
	 100 << SANE_FIXED_SCALE_SHIFT,
	   0 << SANE_FIXED_SCALE_SHIFT
};

static const SANE_Range byte_range =
{
	1, 255, 1
};

static const SANE_Range char_range =
{
	-127, 127,1
};

static const SANE_Range u8_range =
{
	0,				/* minimum */
	255,				/* maximum */
	0				/* quantization */
};

static SANE_Status
create_mapping_table (SANE_Word* table, SANE_Word input_level,
		      SANE_Word output_level, SANE_Int brightness,
		      SANE_Int contrast)
{	
  SANE_Word i;	
  SANE_Int input_bits_per_pixel = 0;	
  SANE_Int output_bits_per_pixel = 0;	
  SANE_Int temp;	
  double offset, offset_temp;	
  SANE_Word start, end, x0, y0;	
  double* code;
  
  code = (double*) malloc (input_level * sizeof (double));
  if (!code)
    return SANE_STATUS_NO_MEM;

  temp = input_level;	
  while (temp >= 2)
    {		
      temp = temp / 2;		
      input_bits_per_pixel ++;	
    }	
  temp = output_level;	
  while (temp >= 2)
    {		
      temp = temp / 2;		
      output_bits_per_pixel ++;	
    }	
  x0 = (SANE_Word) (input_level-1);	
  y0 = (SANE_Word) (output_level-1);	
  
  /* first, maps brightness, contrast effects to the table	
     Contrast first...	*/
  if (contrast >= 0)
    {		
      if (contrast > 0)			
	offset = (((double) (contrast) + 1.0f) * input_level / 256) - 1.0f;
      else			
	offset = 0.0f;		
      start = (SANE_Word) offset;		
      end = (SANE_Word) (x0 - (SANE_Word) offset);		
      for (i=0; i<=x0; i++)
	{			
	  if (i<=start)				
	    code[i] = 0;			
	  else if ((i > start) && (i < end))				
	    code[i] = (double) ((double)x0 * ( i - offset ) 
				  / ( x0 - 2 * offset ));
	  else if (i >= end)				
	    code[i] = (double) x0;		
	}	
    }	
  else
    {		
      offset = (((double) (contrast) - 1.0f) * input_level / (-256)) - 1.0f;
      start = 0;		
      end = x0;		
      if (contrast == -127 )			
	for (i=0; i<=x0; i++)
	  code[i] = (double) offset;		
      else			
	for (i=0; i<=x0; i++)
	  code[i] = (double) (offset + (double) i * (x0 - 2 * offset)
				/ x0 ); 	
    }	
  /* apply brightness here */
  if (brightness > 0)		
    offset = ((double) (brightness+1) * input_level / 256) - 1;	
  else if (brightness < 0)		
    offset = ((double) (brightness-1) * input_level / 256) + 1;	
  else		
    offset = 0;	
  for (i=0; i<=x0; i++)
    {		
      offset_temp = offset + code[i];		
      if (offset_temp <= 0)
	{			
	  code[i] = 0;		
	} 
      else if (offset_temp >= x0)
	{			
	  code[i] = (double) x0;		
	} 
      else 			
	code[i] = (double) offset_temp;	
    }	/* end of brightness...	*/
		
  /* copy code to table..	*/
  for (i = 0; i < input_level; i++)
    {		
      table[i] = (SANE_Word) code[i];	
    }	
  free (code);	
  /* map to table range to output_level	*/
  if (input_bits_per_pixel < output_bits_per_pixel)
    {
      SANE_Word output_temp;
      output_temp = 1;		
      output_temp = output_temp << (output_bits_per_pixel 
				    - input_bits_per_pixel - 1);
      for (i = 0 ; i < input_level; i++) 
	{
	  table[i] = (table[i] << (output_bits_per_pixel 
				   - input_bits_per_pixel)) + output_temp;
	}
    } 	
  return SANE_STATUS_GOOD;
}

static size_t 
max_string_size (const SANE_String_Const strings[])
{  
  size_t size, max_size = 0;  
  SANE_Int i;  

  for (i = 0; strings[i]; ++i)
    {      
      size = strlen (strings[i]) + 1;      
      if (size > max_size)        
	max_size = size;    
    }  
  return max_size;
}

static SANE_Status
calc_parameters (Mustek_Usb_Scanner * s)
{
  SANE_String val;
  SANE_Status status = SANE_STATUS_GOOD;
  SANE_Bool Protect = SANE_TRUE;
  SANE_Int max_x, max_y;

  DBG (5, "calc_parameters: start\n");
  val = s->val[OPT_MODE].s;
  
  if (!strcmp (val, "Lineart"))
    {
      s->params.last_frame = SANE_TRUE;
      s->params.format = SANE_FRAME_GRAY;
      s->params.depth = 1;
      s->channels = 1;
    }
  else if (!strcmp (val, "Gray"))
    {
      s->params.last_frame = SANE_TRUE;
      s->params.format = SANE_FRAME_GRAY;
      s->params.depth = 8;
      s->channels = 1;
    }
  else if (!strcmp (val, "Color"))
    {
      s->params.last_frame = SANE_TRUE;
      s->params.format = SANE_FRAME_RGB;
      s->params.depth = 8;
      s->channels = 3;
    }
  else
    {
      DBG (1, "calc_parameters: Invalid mode %s\n", (char *) val);
      status = SANE_STATUS_INVAL;
    }
  
  s->tl_x = SANE_UNFIX (s->val[OPT_TL_X].w) / MM_PER_INCH;
  s->tl_y = SANE_UNFIX (s->val[OPT_TL_Y].w) / MM_PER_INCH;
  s->br_x = SANE_UNFIX (s->val[OPT_BR_X].w) / MM_PER_INCH - s->tl_x;
  s->br_y = SANE_UNFIX (s->val[OPT_BR_Y].w) / MM_PER_INCH - s->tl_y;
  
  max_x = s->hw->max_width*s->val[OPT_RESOLUTION].w/300;
  max_y = s->hw->max_height*s->val[OPT_RESOLUTION].w/300;  
  
  s->tl_x_dots = s->tl_x * s->val[OPT_RESOLUTION].w;
  s->width_dots = s->br_x * s->val[OPT_RESOLUTION].w;
  s->tl_y_dots = s->tl_y * s->val[OPT_RESOLUTION].w;
  s->height_dots = s->br_y * s->val[OPT_RESOLUTION].w;
  
  if (s->width_dots > max_x) s->width_dots = max_x;
  if (s->height_dots > max_y) s->height_dots = max_y;
  if (!strcmp (val, "Lineart")) {
    s->width_dots = (s->width_dots / 8) * 8;
    if (s->width_dots == 8) s->width_dots = 8;
  }
  if (s->tl_x_dots < 0) s->tl_x_dots = 0;
  if (s->tl_y_dots < 0) s->tl_y_dots = 0;
  if (s->tl_x_dots + s->width_dots > max_x)
    s->tl_x_dots = max_x - s->width_dots;
  if (s->tl_y_dots + s->height_dots> max_y)
    s->tl_y_dots = max_y - s->height_dots;
  
  if (Protect)
    {
      s->val[OPT_TL_X].w = SANE_FIX (s->tl_x * MM_PER_INCH);
      s->val[OPT_TL_Y].w = SANE_FIX (s->tl_y * MM_PER_INCH);
      s->val[OPT_BR_X].w = SANE_FIX ((s->tl_x + s->br_x) * MM_PER_INCH);
      s->val[OPT_BR_Y].w = SANE_FIX ((s->tl_y + s->br_y) * MM_PER_INCH);
    }
  else
    DBG (4, "calc_parameters: Not adapted. Protecting\n");
  
  s->params.pixels_per_line = s->width_dots;
  s->params.lines = s->height_dots;
  s->params.bytes_per_line = s->params.pixels_per_line * s->params.depth/8 
    * s->channels;
  
  DBG (4, "calc_parameters: format=%d\n", s->params.format);
  DBG (4, "calc_parameters: last_frame=%d\n", s->params.last_frame);
  DBG (4, "calc_parameters: lines=%d\n", s->params.lines);
  DBG (4, "calc_parameters: pixels_per_line=%d\n", 
       s->params.pixels_per_line);
  DBG (4, "calc_parameters: bytes_per_line=%d\n", s->params.bytes_per_line);
  DBG (4, "calc_parameters: Pixels %dx%dx%d\n",
       s->params.pixels_per_line, s->params.lines, 1 << s->params.depth);
  
  DBG (5, "calc_parameters: exit\n");
  return status;
}

static SANE_Status
init_options (Mustek_Usb_Scanner * s)
{
  int i;
  SANE_Status status;
  
  DBG (5, "init_options: start\n");

  memset (s->opt, 0, sizeof (s->opt));
  memset (s->val, 0, sizeof (s->val));
  
  for (i = 0; i < NUM_OPTIONS; ++i)
    {
      s->opt[i].size = sizeof (SANE_Word);
      s->opt[i].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    }

  s->opt[OPT_NUM_OPTS].title = SANE_TITLE_NUM_OPTIONS;
  s->opt[OPT_NUM_OPTS].desc = SANE_DESC_NUM_OPTIONS;
  s->opt[OPT_NUM_OPTS].cap = SANE_CAP_SOFT_DETECT;
  s->val[OPT_NUM_OPTS].w = NUM_OPTIONS;

  /* "Mode" group: */
  s->opt[OPT_MODE_GROUP].title = "Scan Mode";
  s->opt[OPT_MODE_GROUP].desc = "";
  s->opt[OPT_MODE_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_MODE_GROUP].cap = 0;
  s->opt[OPT_MODE_GROUP].constraint_type = SANE_CONSTRAINT_NONE;
  
  mode_list[0]="Color";
  mode_list[1]="Gray";
  mode_list[2]="Lineart";
  mode_list[3]=NULL;
  
  /* scan mode */
  s->opt[OPT_MODE].name = SANE_NAME_SCAN_MODE;
  s->opt[OPT_MODE].title = SANE_TITLE_SCAN_MODE;
  s->opt[OPT_MODE].desc = SANE_DESC_SCAN_MODE;
  s->opt[OPT_MODE].type = SANE_TYPE_STRING;
  s->opt[OPT_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_MODE].size = max_string_size (mode_list);
  s->opt[OPT_MODE].constraint.string_list = mode_list;
  s->val[OPT_MODE].s = strdup (mode_list[0]);
  
  /* resolution */
  s->opt[OPT_RESOLUTION].name = SANE_NAME_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].type = SANE_TYPE_INT;
  s->opt[OPT_RESOLUTION].unit = SANE_UNIT_DPI;
  /* TODO: Build the constraints on resolution in a smart way */
  s->opt[OPT_RESOLUTION].constraint_type = SANE_CONSTRAINT_WORD_LIST;
  if (s->hw->chip->scanner_type == MT_600CU)
    s->opt[OPT_RESOLUTION].constraint.word_list = resbit300_list;
  else
    s->opt[OPT_RESOLUTION].constraint.word_list = resbit_list;
  s->val[OPT_RESOLUTION].w = resbit_list[1];

  /* preview */
  s->opt[OPT_PREVIEW].name = SANE_NAME_PREVIEW;
  s->opt[OPT_PREVIEW].title = SANE_TITLE_PREVIEW;
  s->opt[OPT_PREVIEW].desc = SANE_DESC_PREVIEW;
  s->opt[OPT_PREVIEW].cap = SANE_CAP_SOFT_DETECT | SANE_CAP_SOFT_SELECT;
  s->opt[OPT_PREVIEW].type = SANE_TYPE_BOOL;
  s->val[OPT_PREVIEW].w = SANE_FALSE;

  /* "Geometry" group: */
  s->opt[OPT_GEOMETRY_GROUP].title = "Geometry";
  s->opt[OPT_GEOMETRY_GROUP].desc = "";
  s->opt[OPT_GEOMETRY_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_GEOMETRY_GROUP].cap = SANE_CAP_ADVANCED;
  s->opt[OPT_GEOMETRY_GROUP].constraint_type = SANE_CONSTRAINT_NONE;
  
  /* top-left x */
  s->opt[OPT_TL_X].name = SANE_NAME_SCAN_TL_X;
  s->opt[OPT_TL_X].title = SANE_TITLE_SCAN_TL_X;
  s->opt[OPT_TL_X].desc = SANE_DESC_SCAN_TL_X;
  s->opt[OPT_TL_X].type = SANE_TYPE_FIXED;
  s->opt[OPT_TL_X].unit = SANE_UNIT_MM;
  s->opt[OPT_TL_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TL_X].constraint.range = &s->hw->x_range;
  s->val[OPT_TL_X].w = 0;
  
  /* top-left y */
  s->opt[OPT_TL_Y].name = SANE_NAME_SCAN_TL_Y;
  s->opt[OPT_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
  s->opt[OPT_TL_Y].desc = SANE_DESC_SCAN_TL_Y;
  s->opt[OPT_TL_Y].type = SANE_TYPE_FIXED;
  s->opt[OPT_TL_Y].unit = SANE_UNIT_MM;
  s->opt[OPT_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TL_Y].constraint.range = &s->hw->y_range;
  s->val[OPT_TL_Y].w = 0;
  
  /* bottom-right x */
  s->opt[OPT_BR_X].name = SANE_NAME_SCAN_BR_X;
  s->opt[OPT_BR_X].title = SANE_TITLE_SCAN_BR_X;
  s->opt[OPT_BR_X].desc = SANE_DESC_SCAN_BR_X;
  s->opt[OPT_BR_X].type = SANE_TYPE_FIXED;
  s->opt[OPT_BR_X].unit = SANE_UNIT_MM;
  s->opt[OPT_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_X].constraint.range = &s->hw->x_range;
  s->val[OPT_BR_X].w = s->hw->x_range.max;
  
  /* bottom-right y */
  s->opt[OPT_BR_Y].name = SANE_NAME_SCAN_BR_Y;
  s->opt[OPT_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
  s->opt[OPT_BR_Y].desc = SANE_DESC_SCAN_BR_Y;
  s->opt[OPT_BR_Y].type = SANE_TYPE_FIXED;
  s->opt[OPT_BR_Y].unit = SANE_UNIT_MM;
  s->opt[OPT_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_Y].constraint.range = &s->hw->y_range;
  s->val[OPT_BR_Y].w = s->hw->y_range.max;
  
  /* "Enhancement" group: */
  s->opt[OPT_ENHANCEMENT_GROUP].title = "Enhancement";
  s->opt[OPT_ENHANCEMENT_GROUP].desc = "";
  s->opt[OPT_ENHANCEMENT_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_ENHANCEMENT_GROUP].cap = 0;
  s->opt[OPT_ENHANCEMENT_GROUP].constraint_type = SANE_CONSTRAINT_NONE;
  
  /* brightness */
  s->opt[OPT_BRIGHTNESS].name = SANE_NAME_BRIGHTNESS;
  s->opt[OPT_BRIGHTNESS].title = SANE_TITLE_BRIGHTNESS;
  s->opt[OPT_BRIGHTNESS].desc = SANE_DESC_BRIGHTNESS;
  s->opt[OPT_BRIGHTNESS].type = SANE_TYPE_INT;
  s->opt[OPT_BRIGHTNESS].unit = SANE_UNIT_NONE;
  s->opt[OPT_BRIGHTNESS].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BRIGHTNESS].constraint.range = &char_range;
  s->val[OPT_BRIGHTNESS].w = 0;
  
  /* contrast */
  s->opt[OPT_CONTRAST].name = SANE_NAME_CONTRAST;
  s->opt[OPT_CONTRAST].title = SANE_TITLE_CONTRAST;
  s->opt[OPT_CONTRAST].desc = SANE_DESC_CONTRAST;
  s->opt[OPT_CONTRAST].type = SANE_TYPE_INT;
  s->opt[OPT_CONTRAST].unit = SANE_UNIT_NONE;
  s->opt[OPT_CONTRAST].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_CONTRAST].constraint.range = &char_range;
  s->val[OPT_CONTRAST].w = 0;
  
  RIE(calc_parameters (s));

  DBG (5, "init_options: exit\n");
  return SANE_STATUS_GOOD;
}


static SANE_Status
attach (const char *devname, Mustek_Usb_Device ** devp, int may_wait)
{
  Mustek_Usb_Device *dev;
  SANE_Status status;
  Mustek_Type scanner_type;
  SANE_Int fd;

  DBG (5, "attach: start: devp %s NULL, may_wait = %d\n", devp ? "!=" : "==", 
       may_wait);
  if (!devname)
    {
      DBG (1, "attach: devname == NULL\n");
      return SANE_STATUS_INVAL;
    }

  for (dev = first_dev; dev; dev = dev->next)
    if (strcmp (dev->sane.name, devname) == 0)
      {
	if (devp)
	  *devp = dev;
	DBG (4, "attach: device `%s' was already in device list\n", devname);
	return SANE_STATUS_GOOD;
      }

  DBG(4, "attach: trying to open device `%s'\n", devname);
  status = sanei_usb_open (devname, &fd);
  if (status != SANE_STATUS_GOOD)
    {
      DBG(3, "attach: couldn't open device `%s': %s\n", devname,
	  sane_strstatus (status));
      return status;
    }
  DBG(4, "attach: device `%s' successfully opened\n", devname);

  /* Try to identify model */
  DBG(4, "attach: trying to identify device `%s'\n", devname);
  status = usb_low_identify_scanner (fd, &scanner_type);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "attach: device `%s' doesn't look like a supported scanner\n",
	   devname);
      sanei_usb_close (fd);
      return status;
    }
  sanei_usb_close (fd);
  if (scanner_type == MT_UNKNOWN)
    {
      DBG (3, "attach: warning: couldn't identify device `%s', must set "
	   "type manually\n", devname);
    }

  dev = malloc (sizeof (Mustek_Usb_Device));
  if (!dev)
    {
      DBG(1, "attach: couldn't malloc Mustek_Usb_Device\n");
      return SANE_STATUS_NO_MEM;
    }

  memset(dev, 0, sizeof (*dev));
  dev->name = strdup (devname);
  dev->sane.name = (SANE_String_Const) dev->name;
  dev->sane.vendor = "Mustek";
  switch (scanner_type)
    {
    case MT_1200CU:
      dev->sane.model = "1200 CU";
      break;
    case MT_1200CU_PLUS:
      dev->sane.model = "1200 CU Plus";
      break;
    case MT_1200UB:
      dev->sane.model = "1200 UB";
      break;
    case MT_600CU:
      dev->sane.model = "600 CU";
      break;
    default:
      dev->sane.model = "(unidentified)";
      break;
    }
  dev->sane.type = "flatbed scanner";

  dev->x_range.min = 0;
  dev->x_range.max = SANE_FIX (8.5 * MM_PER_INCH);
  dev->x_range.quant = 0;

  dev->y_range.min = 0;
  dev->y_range.max = SANE_FIX (11.9 * MM_PER_INCH);
  dev->y_range.quant = 0;

  dev->max_height = 11.9*300;
  dev->max_width = 8.5*300;
  dev->dpi_range.min = SANE_FIX (50);
  dev->dpi_range.max = SANE_FIX (600);
  dev->dpi_range.quant = SANE_FIX (1);
  
  status = usb_high_scan_init (dev);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "attach: usb_high_scan_init returned status: %s\n",
	  sane_strstatus (status));
      free (dev);
      return status;
    }
  dev->chip->scanner_type = scanner_type;

  DBG (2, "attach: found %s %s %s at %s\n", dev->sane.vendor, dev->sane.type,
       dev->sane.model, dev->sane.name);
  ++num_devices;
  dev->next = first_dev;
  first_dev = dev;

  if (devp)
    *devp = dev;

  DBG (5, "attach: exit\n");
  return SANE_STATUS_GOOD;
}

static SANE_Status
attach_one_device (SANE_String_Const devname)
{
  Mustek_Usb_Device *dev;
  SANE_Status status;
  
  RIE(attach (devname, &dev, SANE_FALSE));

  if (dev)
    {
      /* Keep track of newly attached devices so we can set options as
	 necessary.  */
      if (new_dev_len >= new_dev_alloced)
	{
	  new_dev_alloced += 4;
	  if (new_dev)
	    new_dev = realloc (new_dev, new_dev_alloced * sizeof (new_dev[0]));
	  else
	    new_dev = malloc (new_dev_alloced * sizeof (new_dev[0]));
	  if (!new_dev)
	    {
	      DBG (1, "attach_one_device: out of memory\n");
	      return SANE_STATUS_NO_MEM;
	    }
	}
      new_dev[new_dev_len++] = dev;
    }
  return SANE_STATUS_GOOD;
}

/* -------------------------- SANE API functions ------------------------- */

SANE_Status
sane_init (SANE_Int * version_code, SANE_Auth_Callback authorize)
{
  SANE_Char line[PATH_MAX];
  SANE_Char *word;
  SANE_String_Const cp;
  SANE_Int linenumber;
  FILE *fp;

  DBG_INIT ();
  DBG (2, "SANE Mustek USB backend version %d.%d build %d from %s\n", V_MAJOR,
       V_MINOR, BUILD, PACKAGE_VERSION);

  if (version_code)
    *version_code = SANE_VERSION_CODE (V_MAJOR, V_MINOR, BUILD);

  DBG(5, "sane_init: authorize %s null\n", authorize ? "!=" : "==");

  sanei_usb_init ();

  fp = sanei_config_open (MUSTEK_USB_CONFIG_FILE);
  if (!fp)
    {
      /* default to /dev/usb/scanner instead of insisting on config file */
      DBG(3, "sane_init: couldn't open config file `%s': %s. Using "
	  "/dev/usb/scanner directly\n", MUSTEK_USB_CONFIG_FILE,
	  strerror (errno));
      attach ("/dev/usb/scanner", 0, SANE_FALSE);
      return SANE_STATUS_GOOD;
    }
  linenumber = 0;
  DBG(4, "sane_init: reading config file `%s'\n",  MUSTEK_USB_CONFIG_FILE);

  while (sanei_config_read (line, sizeof (line), fp))
    {
      word = 0;
      linenumber++;

      cp = sanei_config_get_string (line, &word);
      if (!word || cp == line)
	{
	  DBG(5, "sane_init: config file line %d: ignoring empty line\n",
	      linenumber);
	  if (word)
	    free (word);
	  continue;
	}
      if (word[0] == '#')
	{
	  DBG(5, "sane_init: config file line %d: ignoring comment line\n",
	      linenumber);
	  free (word);
	  continue;
	}
                    
      if (strcmp (word, "option") == 0)
	{
	  free (word);
	  word = 0;
	  cp = sanei_config_get_string (cp, &word);

	  if (strcmp (word, "1200ub") == 0)
	    {
	      if (new_dev_len > 0)
		{
		  /* This is a 1200 UB */
		  new_dev[new_dev_len - 1]->chip->scanner_type 
		    = MT_1200UB;
		  new_dev[new_dev_len - 1]->sane.model 
		    = "1200 UB";
		  DBG(3, "sane_init: config file line %d: `%s' is a Mustek "
		      "1200 UB\n", linenumber, 
		      new_dev[new_dev_len - 1]->sane.name);
		}
	      else
		{
		  DBG(3, "sane_init: config file line %d: option "
		      "1200ub ignored, was set before any device "
		      "name\n", linenumber);
		}
	      if (word)
		free (word);
	      word = 0;
	    }
	  else if (strcmp (word, "1200cu") == 0)
	    {
	      if (new_dev_len > 0)
		{
		  /* This is a 1200 CU */
		  new_dev[new_dev_len - 1]->chip->scanner_type 
		    = MT_1200CU;
		  new_dev[new_dev_len - 1]->sane.model 
		    = "1200 CU";
		  DBG(3, "sane_init: config file line %d: `%s' is a Mustek "
		      "1200 CU\n", linenumber, 
		      new_dev[new_dev_len - 1]->sane.name);
		}
	      else
		{
		  DBG(3, "sane_init: config file line %d: option "
		      "1200cu ignored, was set before any device "
		      "name\n", linenumber);
		}
	      if (word)
		free (word);
	      word = 0;
	    }
	  else if (strcmp (word, "1200cu_plus") == 0)
	    {
	      if (new_dev_len > 0)
		{
		  /* This is a 1200 CU Plus*/
		  new_dev[new_dev_len - 1]->chip->scanner_type 
		    = MT_1200CU_PLUS;
		  new_dev[new_dev_len - 1]->sane.model 
		    = "1200 CU Plus";
		  DBG(3, "sane_init: config file line %d: `%s' is a Mustek "
		      "1200 CU Plus\n", linenumber, 
		      new_dev[new_dev_len - 1]->sane.name);
		}
	      else
		{
		  DBG(3, "sane_init: config file line %d: option "
		      "1200cu_plus ignored, was set before any device "
		      "name\n", linenumber);
		}
	      if (word)
		free (word);
	      word = 0;
	    }
	  else if (strcmp (word, "600cu") == 0)
	    {
	      if (new_dev_len > 0)
		{
		  /* This is a 600 CU */
		  new_dev[new_dev_len - 1]->chip->scanner_type 
		    = MT_600CU;
		  new_dev[new_dev_len - 1]->sane.model 
		    = "600 CU";
		  DBG(3, "sane_init: config file line %d: `%s' is a Mustek "
		      "600 CU\n", linenumber, 
		      new_dev[new_dev_len - 1]->sane.name);
		}
	      else
		{
		  DBG(3, "sane_init: config file line %d: option "
		      "600cu ignored, was set before any device "
		      "name\n", linenumber);
		}
	      if (word)
		free (word);
	      word = 0;
	    }
	}
      else
	{ 
	  new_dev_len = 0;
	  DBG(4, "sane_init: config file line %d: trying to attach `%s'\n",
	      linenumber, line);
	  sanei_usb_attach_matching_devices (line, attach_one_device);
	  if (word)
	    free (word);
	  word = 0;
	}
    }     

  if (new_dev_alloced > 0)
    {     
      new_dev_len = new_dev_alloced = 0;
      free (new_dev);
    }

  fclose (fp);
  DBG(5, "sane_init: exit\n");

  return SANE_STATUS_GOOD;
}

void
sane_exit (void)
{
  Mustek_Usb_Device *dev, *next;
  SANE_Status status;
  
  DBG(5, "sane_exit: start\n");
  for (dev = first_dev; dev; dev = next)
    {
      next = dev->next;
      status = usb_high_scan_exit (dev);
      if (status != SANE_STATUS_GOOD)
	DBG (3, "sane_exit: while closing %s, usb_high_scan_exit returned: "
	     "%s\n", dev->name, sane_strstatus (status));
      free ((void *) dev->name);
      free (dev);
    }
  DBG(5, "sane_exit: exit\n");
}

SANE_Status
sane_get_devices (const SANE_Device *** device_list, SANE_Bool local_only)
{
  static const SANE_Device **devlist = 0;
  Mustek_Usb_Device *dev;
  int i;
  
  DBG(5, "sane_get_devices: start: local_only = %s\n", 
      local_only == SANE_TRUE ? "true" : "false");

  if (devlist)
    free (devlist);
  
  devlist = malloc ((num_devices + 1) * sizeof (devlist[0]));
  if (!devlist)
    return SANE_STATUS_NO_MEM;
  
  i = 0;
  for (dev = first_dev; i < num_devices; dev = dev->next) 
    devlist[i++] = &dev->sane;
  devlist[i++] = 0;
  
  *device_list = devlist;

  DBG(5, "sane_get_devices: exit\n");

  return SANE_STATUS_GOOD;
}

SANE_Status
sane_open (SANE_String_Const devicename, SANE_Handle * handle)
{
  Mustek_Usb_Device *dev;
  SANE_Status status;
  Mustek_Usb_Scanner *s;
  
  DBG(5, "sane_open: start\n");

  if (devicename[0])
    {
      for (dev = first_dev; dev; dev = dev->next)
	if (strcmp (dev->sane.name, devicename) == 0) 
	  break;
      
      if (!dev)
	{
	  RIE(attach (devicename, &dev, SANE_TRUE));
	}
    }
  else
    /* empty devicname -> use first device */
    dev = first_dev;
  
  if (!dev)
    return SANE_STATUS_INVAL;
  
  if (dev->chip->scanner_type == MT_UNKNOWN)
    {
      DBG(0, "sane_open: The type of your scanner is unknown. Edit "
	  "mustek_usb.conf before using the scanner.\n");
      return SANE_STATUS_INVAL;
    }
  s = malloc (sizeof (*s));
  if (!s)
    return SANE_STATUS_NO_MEM;
  memset (s, 0, sizeof (*s));
  s->hw = dev;
  
  RIE(init_options (s));
  
  /* insert newly opened handle into list of open handles: */
  s->next = first_handle;
  first_handle = s;
  
  *handle = s;
  RIE(usb_high_scan_load_private_profile (s->hw));
  strcpy (s->hw->device_name, devicename);

  RIE(usb_high_scan_turn_power (s->hw, SANE_TRUE));
  RIE(usb_high_scan_back_home (s->hw));

  s->hw->scan_buffer = (SANE_Byte *) malloc (SCAN_BUFFER_SIZE);
  if (!s->hw->scan_buffer)
    {
      DBG(5, "sane_open: couldn't malloc s->hw->scan_buffer (%d bytes)\n",
	  SCAN_BUFFER_SIZE);
      return SANE_STATUS_NO_MEM;
    }
  s->hw->scan_buffer_len = 0;
  s->hw->scan_buffer_start = s->hw->scan_buffer;
  DBG(5, "sane_open: exit\n");

  return SANE_STATUS_GOOD;
}

void
sane_close (SANE_Handle handle)
{
  Mustek_Usb_Scanner *prev, *s;
  SANE_Status status;
  
  DBG(5, "sane_close: start\n");

  /* remove handle from list of open handles: */
  prev = 0;
  for (s = first_handle; s; s = s->next)
    {
      if (s == handle)
	break;
      prev = s;
    }
  if (!s)
    {
      DBG (5, "close: invalid handle %p\n", handle);
      return;			/* oops, not a handle we know about */
    }
  
  if (prev)
    prev->next = s->next;
  else
    first_handle = s->next;
  
  if (s->hw->is_prepared)
    {
      status = usb_high_scan_clearup (s->hw);
      if (status != SANE_STATUS_GOOD)
	DBG (3, "sane_close: usb_high_scan_clearup returned %s\n",
	     sane_strstatus (status));
    }
  status = usb_high_scan_exit (s->hw);
  if (status != SANE_STATUS_GOOD)
    DBG (3, "sane_close: usb_high_scan_exit returned %s\n",
	 sane_strstatus (status));

  free (handle);

  if (s->hw->scan_buffer)
    {
      free (s->hw->scan_buffer);
      s->hw->scan_buffer = 0;
    }
  DBG(5, "sane_close: exit\n");
}

const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
  Mustek_Usb_Scanner *s = handle;

  if ((unsigned) option >= NUM_OPTIONS)
    return 0;
  DBG (5, "sane_get_option_descriptor: option = %s (%d)\n", 
       s->opt[option].name, option);
  return s->opt + option;
}

SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option,
		     SANE_Action action, void *val, SANE_Int * info)
{
  Mustek_Usb_Scanner *s = handle;
  SANE_Status status;
  SANE_Word cap;

  DBG (5, "sane_control_option: start: action = %s, option = %s (%d)\n",
       (action == SANE_ACTION_GET_VALUE) ? "get" : "set",
       s->opt[option].name, option);
  
  if (val || action == SANE_ACTION_GET_VALUE)
    switch (s->opt[option].type)
      {
      case SANE_TYPE_STRING:
	DBG (5, "sane_control_option: Value %s\n", s->val[option].s);
	break;
      case SANE_TYPE_FIXED:
	{
	  double v1, v2;
	  SANE_Fixed f;
	  v1 = SANE_UNFIX (s->val[option].w);
	  f = *(SANE_Fixed *) val;
	  v2 = SANE_UNFIX (f);
	  DBG (5, "sane_control_option: Value %g (Fixed)\n", v1);
	}
	break;
      default:
	DBG (5, "sane_control_option: Value %u (Int)\n", s->val[option].w);
	break;
      }
  
  if (info)
    *info = 0;
  
  if (s->scanning)
    return SANE_STATUS_DEVICE_BUSY;
  
  if (option >= NUM_OPTIONS)
    return SANE_STATUS_INVAL;
  
  cap = s->opt[option].cap;
  
  if (!SANE_OPTION_IS_ACTIVE (cap))
    return SANE_STATUS_INVAL;
  
  if (action == SANE_ACTION_GET_VALUE)
    {
      switch (option)
	{
	  /* word options: */
	case OPT_NUM_OPTS:
	case OPT_RESOLUTION:
	case OPT_PREVIEW:
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
	case OPT_BRIGHTNESS:
	case OPT_CONTRAST:
	  *(SANE_Word *) val = s->val[option].w;
	  return SANE_STATUS_GOOD;

	  /* string options: */
	case OPT_MODE:
	  strcpy(val, s->val[option].s);
	  return SANE_STATUS_GOOD;
	}
    }
  else if (action == SANE_ACTION_SET_VALUE)
    {
      if (!SANE_OPTION_IS_SETTABLE (cap))
	return SANE_STATUS_INVAL;
      
      status = sanei_constrain_value (s->opt + option, val, info);
      
      if (status != SANE_STATUS_GOOD)
	return status;
      
      switch (option)
	{
	  /* (mostly) side-effect-free word options: */
	case OPT_RESOLUTION:
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
	  s->val[option].w = *(SANE_Word *) val;
	  RIE(calc_parameters (s));
	  if (info)
	    *info |= SANE_INFO_INEXACT|SANE_INFO_RELOAD_PARAMS;
	  return SANE_STATUS_GOOD;
	  
	  /* fall through */
	case OPT_BRIGHTNESS:
	case OPT_CONTRAST:
	  s->val[option].w = *(SANE_Word *) val;
	  return SANE_STATUS_GOOD;
	  /* Boolean */
	case OPT_PREVIEW:
	  s->val[option].w = *(SANE_Bool *) val;
	  return SANE_STATUS_GOOD;
	  
	case OPT_MODE:
	  if (s->val[option].s)
	    free (s->val[option].s);
	  s->val[option].s = strdup (val);
	  
	  RIE(status = calc_parameters(s));
	  if (strcmp(val, "Lineart") == 0)
	    s->opt[OPT_CONTRAST].cap |= SANE_CAP_INACTIVE;
	  else
	    s->opt[OPT_CONTRAST].cap &= ~SANE_CAP_INACTIVE;
	  if (info)
	    *info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;
	  return SANE_STATUS_GOOD;
	} /* End of switch */
    } /* End of SET_VALUE */

  DBG(4, "sane_control_option: unknown action for option %s\n",
      s->opt[option].name);
  
  return SANE_STATUS_INVAL;
}

SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters * params)
{
  Mustek_Usb_Scanner *s = handle;
  SANE_Status status;
  
  DBG(5, "sane_get_parameters: start\n");

  RIE(calc_parameters (s));
  if (params)
    *params = s->params;

  DBG(5, "sane_get_parameters: exit\n");

  return SANE_STATUS_GOOD;
}

SANE_Status
sane_start (SANE_Handle handle)
{
  Mustek_Usb_Scanner *s = handle;
  SANE_Status status;
  Target_Image target;
  SANE_String val;
  
  DBG(5, "sane_start: start\n");

  /* First make sure we have a current parameter set.  Some of the
     parameters will be overwritten below, but that's OK.  */
  
  RIE(calc_parameters (s));
  RIE(create_mapping_table (s->hw->mapping_table, 256, 256, 
			    s->val[OPT_BRIGHTNESS].w, s->val[OPT_CONTRAST].w));

  val = s->val[OPT_MODE].s;
  if (!strcmp (val, "Lineart"))
    {
      target.color_mode = GRAY8;
    }
  else if (!strcmp (val, "Gray"))
    {
      target.color_mode = GRAY8;
    }
  else /* Color */
    {
      target.color_mode = RGB24;
    }
  
  target.is_optimal_speed = SANE_TRUE;
  target.dpi = s->val[OPT_RESOLUTION].w;
  target.x = (SANE_Word)(s->tl_x_dots);
  target.y = (SANE_Word)(s->tl_y_dots);
  target.width = (SANE_Word)(s->width_dots);
  target.height = (SANE_Word)(s->height_dots);

  if (!s->hw->is_prepared)
    {
      RIE(usb_high_scan_prepare (s->hw));
      RIE(usb_high_scan_reset (s->hw));
    }
  RIE(usb_high_scan_set_threshold (s->hw, 128));
  RIE(usb_high_scan_embed_gamma (s->hw, NULL));
  RIE(usb_high_scan_suggest_parameters (s->hw, &target, &s->hw->suggest));
  RIE(usb_high_scan_setup_scan (s->hw, s->hw->suggest.scan_mode,
				s->hw->suggest.x_dpi, s->hw->suggest.y_dpi, 0,
				s->hw->suggest.x, s->hw->suggest.y,
				s->hw->suggest.width));
  s->read_rows = s->height_dots; 

  DBG(5, "sane_start: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_read (SANE_Handle handle, SANE_Byte * buf, SANE_Int max_len,
	   SANE_Int * len)
{
  Mustek_Usb_Scanner *s = handle;
  int lines_read;
  int i,j;
  SANE_String val = s->val[OPT_MODE].s;
  int bpp;
  int bright, contra;
  SANE_Byte * i_buf;
  SANE_Status status;
  
  DBG(5, "sane_read: start\n");

  if (!s)
    {
      DBG(1, "sane_read: handle is null!\n");
      return SANE_STATUS_INVAL;
    }

  if (!buf)
    {
      DBG(1, "sane_read: buf is null!\n");
      return SANE_STATUS_INVAL;
    }

  if (!len)
    {
      DBG(1, "sane_read: len is null!\n");
      return SANE_STATUS_INVAL;
    }

  if (s->hw->scan_buffer_len == 0)
    {
      bright = s->val[OPT_BRIGHTNESS].w;
      contra = s->val[OPT_CONTRAST].w;
      
      if (!strcmp (val, "Lineart"))
	{
	  bpp = 8;
	}
      else if (!strcmp (val, "Gray"))
	{
	  bpp = 8;
	}
      else /* Color */
	{
	  bpp = 24;
	}
      
      *len = 0;
      if (s->read_rows > 0)
	{
	  /* We don't need to BackTrack... too much waste of time  */
	  lines_read = max_len / (s->width_dots*bpp/8);
	  if (lines_read > 0)
	    {
	      i_buf = buf;
	      if (lines_read > s->read_rows)
		lines_read = s->read_rows;
	    }
	  else
	    {
	      DBG (5, "sane_read: using scan buffer\n");
	      lines_read = SCAN_BUFFER_SIZE / (s->width_dots*bpp/8);
	      if (lines_read > s->read_rows)
		lines_read = s->read_rows;
	      i_buf = s->hw->scan_buffer;
	      s->hw->scan_buffer_start = s->hw->scan_buffer;
	      s->hw->scan_buffer_len = (s->width_dots*bpp/8) * lines_read;
	    }
	  if (lines_read > s->read_rows)
	    lines_read = s->read_rows;
	  RIE(usb_high_scan_get_rows (s->hw, i_buf, lines_read, SANE_FALSE));
	  if (!strcmp (val, "Lineart"))
	    {
	      long int pointer;
	      pointer = 0;
	      bright = 128 - bright;
	      for (i = 0; i < lines_read; i++)
		for (j = 0; j<s->width_dots/8; j++) 
		  {
		    i_buf[pointer] = 
		      (((i_buf[i * s->width_dots + j * 8 + 0] > bright) 
			? 0 : 1) << 7) +
		      (((i_buf[i * s->width_dots + j * 8 + 1] > bright) 
			? 0 : 1) << 6) +
		      (((i_buf[i * s->width_dots + j * 8 + 2] > bright) 
			? 0 : 1) << 5) +
		      (((i_buf[i * s->width_dots + j * 8 + 3] > bright) 
			? 0 : 1) << 4) +
		      (((i_buf[i * s->width_dots + j * 8 + 4] > bright) 
			? 0 : 1) << 3) +
		      (((i_buf[i * s->width_dots + j * 8 + 5] > bright) 
			? 0 : 1) << 2) +
		      (((i_buf[i * s->width_dots + j * 8 + 6] > bright) 
			? 0 : 1) << 1) +
		      (((i_buf[i * s->width_dots + j * 8 + 7] > bright) 
			? 0 : 1));
		    pointer++;
		  }
	      *len = pointer;
	      s->read_rows -= lines_read;
	    } 
	  else
	    {
	      *len = (s->width_dots*bpp/8)*lines_read;
	      s->read_rows -= lines_read;
	      for (i = 0; i < *len; i++)
		{
		  i_buf[i] = s->hw->mapping_table[i_buf[i]];
		}
	    }
	} 
      else
	{
	  DBG(5, "sane_read: scan finished -- exit\n");
	  return SANE_STATUS_EOF;
	}
    }

  if (s->hw->scan_buffer_len > 0)
    {
      *len = MIN (max_len, (SANE_Int) s->hw->scan_buffer_len);
      memcpy (buf, s->hw->scan_buffer_start, *len);
      DBG (5, "sane_read: read %d bytes from scan_buffer; "
	   "%d bytes remaining\n", *len, s->hw->scan_buffer_len - *len);
      s->hw->scan_buffer_len -= (*len);
      s->hw->scan_buffer_start += (*len);
    }
  else
    {
      DBG(5, "sane_read: read %d bytes without scan buffer\n", *len);
    }

  return SANE_STATUS_GOOD;
}

void
sane_cancel (SANE_Handle handle)
{
  Mustek_Usb_Scanner *s = handle;
  SANE_Status status;
  
  DBG (5, "sane_cancel: start\n");

  status = usb_high_scan_stop_scan (s->hw);
  if (status != SANE_STATUS_GOOD)
    DBG (3, "sane_cancel: usb_high_scan_stop_scan returned `%s' for `%s'\n", 
	 sane_strstatus (status), s->hw->name);
  usb_high_scan_back_home (s->hw);
  if (status != SANE_STATUS_GOOD)
    DBG (3, "sane_cancel: usb_high_scan_back_home returned `%s' for `%s'\n", 
	 sane_strstatus (status), s->hw->name);

  if (s->scanning)
    {
      if (s->aborted_by_user)
	{
	  DBG (3,"sane_cancel: Allready Aborted. Please Wait...\n");
	}
      else
	{
	  s->scanning = SANE_FALSE;
	  s->aborted_by_user = SANE_TRUE;
	  DBG (3, "sane_cancel: Signal Caught! Aborting...\n");
	}
    }
  else
    {
      if (s->aborted_by_user)
	{
	  DBG (3, "sane_cancel: Scan has not been Initiated yet, "
	       "or it is allready aborted.\n");
	  s->aborted_by_user = SANE_FALSE;
	}
      else
	{
	  DBG (3, "sane_cancel: Scan has not been Initiated "
	       "yet (or it's over).\n");
	}
    }
  return;
}

SANE_Status
sane_set_io_mode (SANE_Handle handle, SANE_Bool non_blocking)
{
  DBG (5, "sane_set_io_mode: handle = %p, non_blockeing = %s\n", 
       handle, non_blocking == SANE_TRUE ? "true" : "false");
  return SANE_STATUS_UNSUPPORTED;
}

SANE_Status
sane_get_select_fd (SANE_Handle handle, SANE_Int * fd)
{
  DBG (5, "sane_get_select_fd: handle = %p, fd = %p\n", handle, fd);
  return SANE_STATUS_UNSUPPORTED;
}
