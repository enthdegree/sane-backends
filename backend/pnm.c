/* sane - Scanner Access Now Easy.
   Copyright (C) 1996, 1997 Andreas Beck
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
   If you do not wish that, delete this exception notice.  */

#include "sane/config.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include "sane/sane.h"
#include "sane/sanei.h"
#include "sane/saneopts.h"

#define BACKEND_NAME	pnm
#include "sane/sanei_backend.h"

#ifndef PATH_MAX
# define PATH_MAX	1024
#endif

#define MAGIC	(void *)0xab730324

static int is_open = 0;
static int three_pass = 0;
static int hand_scanner = 0;
static int pass = 0;
static char filename[PATH_MAX] = "/tmp/input.ppm";
static SANE_Fixed bright = 0;
static SANE_Fixed contr = 0;
static SANE_Bool gray = SANE_FALSE;
static enum
  {
    ppm_bitmap,
    ppm_greyscale,
    ppm_color
  }
ppm_type = ppm_color;
static FILE *infile = NULL;

static const SANE_Range percentage_range =
  {
    -100 << SANE_FIXED_SCALE_SHIFT,	/* minimum */
     100 << SANE_FIXED_SCALE_SHIFT,	/* maximum */
       0 << SANE_FIXED_SCALE_SHIFT	/* quantization */
  };

static const SANE_Option_Descriptor sod[] =
  {
    {
      SANE_NAME_NUM_OPTIONS,
      SANE_TITLE_NUM_OPTIONS,
      SANE_DESC_NUM_OPTIONS,
      SANE_TYPE_INT,
      SANE_UNIT_NONE,
      sizeof (SANE_Word),
      SANE_CAP_SOFT_DETECT,
      SANE_CONSTRAINT_NONE,
      {NULL}
    },
    {
      "",
      "Source Selection",
      "Selection of the file to load.",
      SANE_TYPE_GROUP,
      SANE_UNIT_NONE,
      0,
      0,
      SANE_CONSTRAINT_NONE,
      {NULL}
    },
    {
      SANE_NAME_FILE,
      SANE_TITLE_FILE,
      SANE_DESC_FILE,
      SANE_TYPE_STRING,
      SANE_UNIT_NONE,
      sizeof (filename),
      SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT,
      SANE_CONSTRAINT_NONE,
      {NULL}
    },
    {
      "",
      "Image Enhancement",
      "A few controls to enhance image while loading",
      SANE_TYPE_GROUP,
      SANE_UNIT_NONE,
      0,
      0,
      SANE_CONSTRAINT_NONE,
      {NULL}
    },
    {
      SANE_NAME_BRIGHTNESS,
      SANE_TITLE_BRIGHTNESS,
      SANE_DESC_BRIGHTNESS,
      SANE_TYPE_FIXED,
      SANE_UNIT_PERCENT,
      sizeof (SANE_Word),
      SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT,
      SANE_CONSTRAINT_RANGE,
      {(SANE_String_Const *) &percentage_range}	/* this is ANSI conformant! */
    },
    {
      SANE_NAME_CONTRAST,
      SANE_TITLE_CONTRAST,
      SANE_DESC_CONTRAST,
      SANE_TYPE_FIXED,
      SANE_UNIT_PERCENT,
      sizeof (SANE_Word),
      SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT,
      SANE_CONSTRAINT_RANGE,
      {(SANE_String_Const *) &percentage_range}	/* this is ANSI conformant! */
    },
    {
      "grayify",
      "Grayify",
      "Load the image as grayscale.",
      SANE_TYPE_BOOL,
      SANE_UNIT_NONE,
      sizeof (SANE_Word),
      SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT,
      SANE_CONSTRAINT_NONE,
      {NULL}
    },
    {
      "three-pass",
      "Three-Pass Simulation",
      "Simulate a three-pass scanner by returning 3 separate frames.  "
      "For kicks, it returns green, then blue, then red.",
      SANE_TYPE_BOOL,
      SANE_UNIT_NONE,
      sizeof (SANE_Word),
      SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT,
      SANE_CONSTRAINT_NONE,
      {NULL}
    },
    {
      "hand-scanner",
      "Hand-Scanner Simulation",
      "Simulate a hand-scanner.  Hand-scanners often do not know the image "
      "height a priori.  Instead, they return a height of -1.  Setting this "
      "option allows to test whether a frontend can handle this correctly.",
      SANE_TYPE_BOOL,
      SANE_UNIT_NONE,
      sizeof (SANE_Word),
      SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT,
      SANE_CONSTRAINT_NONE,
      {NULL}
    },
    {
      "default-enhancements",
      "Defaults",
      "Set default values for enhancement controls (brightness & contrast).",
      SANE_TYPE_BUTTON,
      SANE_UNIT_NONE,
      0,
      SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT,
      SANE_CONSTRAINT_NONE,
      {NULL}
    }
  };

static SANE_Parameters parms =
  {
    SANE_FRAME_RGB,
    0,
    0,				/* Number of bytes returned per scan line: */
    0,				/* Number of pixels per scan line.  */
    0,				/* Number of lines for the current scan.  */
    8,				/* Number of bits per sample. */
  };

/* This library is a demo implementation of a SANE backend.  It
   implements a virtual device, a PNM file-filter. */
SANE_Status
sane_init (SANE_Int * version_code, SANE_Auth_Callback authorize)
{
  DBG_INIT();
  DBG(2, "sane_init: version_code %s 0, authorize %s 0\n",
      version_code == 0 ? "=" : "!=", authorize == 0 ? "=" : "!="); 
  if (version_code)
    *version_code = SANE_VERSION_CODE (V_MAJOR, V_MINOR, 0);
  return SANE_STATUS_GOOD;
}

void
sane_exit (void)
{
  DBG(2, "sane_exit\n");
}

/* Device select/open/close */

static const SANE_Device dev[] =
{
  {
    "0",
    "Noname",
    "PNM file reader",
    "virtual device"
  },
  {
    "1",
    "Noname",
    "PNM file reader",
    "virtual device"
  },
};

SANE_Status
sane_get_devices (const SANE_Device *** device_list, SANE_Bool local_only)
{
  static const SANE_Device * devlist[] =
  {
    dev + 0, dev + 1, 0
  };

  DBG(2, "sane_get_devices: local_only = %d\n", local_only);
  *device_list = devlist;
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_open (SANE_String_Const devicename, SANE_Handle * handle)
{
  int i;

  if (!devicename)
    return SANE_STATUS_INVAL;
  DBG(2, "sane_open: devicename = \"%s\"\n", devicename);  

  if (!devicename[0])
    i = 0;
  else
    for (i = 0; i < NELEMS(dev); ++i)
      if (strcmp (devicename, dev[i].name) == 0)
	break;
  if (i >= NELEMS(dev))
    return SANE_STATUS_INVAL;

  if (is_open)
    return SANE_STATUS_DEVICE_BUSY;

  is_open = 1;
  *handle = MAGIC;
  return SANE_STATUS_GOOD;
}

void
sane_close (SANE_Handle handle)
{
  DBG(2, "sane_close\n");
  if (handle == MAGIC)
    is_open = 0;
}

const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
  DBG(2, "sane_get_option_descriptor: option = %d\n", option);
  if (handle != MAGIC || !is_open)
    return NULL;		/* wrong device */
  if (option < 0 || option >= NELEMS(sod))
    return NULL;
  return &sod[option];
}

SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option,
		     SANE_Action action, void *value,
		     SANE_Int * info)
{
  SANE_Int myinfo = 0;
  SANE_Status status;

  DBG(2, "sane_control_option: handle=%p, opt=%d, act=%d, val=%p, info=%p\n",
      handle, option, action, value, info);

  if (handle != MAGIC || !is_open)
    return SANE_STATUS_INVAL;	/* Unknown handle ... */

  if (option < 0 || option >= NELEMS(sod))
    return SANE_STATUS_INVAL;	/* Unknown option ... */

  switch (action)
    {
    case SANE_ACTION_SET_VALUE:
      status = sanei_constrain_value (sod + option, value, &myinfo);
      if (status != SANE_STATUS_GOOD)
	return status;

      switch (option)
	{
	case 2:
	  if ((strlen (value) + 1) > sizeof (filename))
	    return SANE_STATUS_NO_MEM;
	  strcpy (filename, value);
	  if (access (filename, R_OK) == 0)
	    myinfo |= SANE_INFO_RELOAD_PARAMS;
	  break;
	case 4:
	  bright = *(SANE_Word *) value;
	  break;
	case 5:
	  contr = *(SANE_Word *) value;
	  break;
	case 6:
	  gray = !!*(SANE_Word *) value;
	  myinfo |= SANE_INFO_RELOAD_PARAMS;
	  break;
	case 7:
	  three_pass = !!*(SANE_Word *) value;
	  break;
	case 8:
	  hand_scanner = !!*(SANE_Word *) value;
	  break;
	case 9:
	  bright = contr = 0;
	  myinfo |= SANE_INFO_RELOAD_OPTIONS;
	  break;
	default:
	  return SANE_STATUS_INVAL;
	}
      break;

    case SANE_ACTION_GET_VALUE:
      switch (option)
	{
	case 0:
	  *(SANE_Word *) value = NELEMS(sod);
	  break;
	case 2:
	  strcpy (value, filename);
	  break;
	case 4:
	  *(SANE_Word *) value = bright;
	  break;
	case 5:
	  *(SANE_Word *) value = contr;
	  break;
	case 6:
	  *(SANE_Word *) value = gray;
	  break;
	case 7:
	  *(SANE_Word *) value = three_pass;
	  break;
	case 8:
	  *(SANE_Word *) value = hand_scanner;
	  break;
	case 9:
	  break;
	default:
	  return SANE_STATUS_INVAL;
	}
      break;

    case SANE_ACTION_SET_AUTO:
      return SANE_STATUS_UNSUPPORTED;	/* We are DUMB */
    }
  if (info)
    *info = myinfo;
  return SANE_STATUS_GOOD;
}

static void
get_line (char *buf, int len, FILE * f)
{
  do
    fgets (buf, len, f);
  while (*buf == '#');
}

static int
getparmfromfile (void)
{
  FILE *fn;
  int x, y;
  char buf[1024];

  parms.bytes_per_line = parms.pixels_per_line = parms.lines = 0;
  if ((fn = fopen (filename, "rb")) == NULL)
    return -1;

  /* Skip comments. */
  do
    get_line (buf, sizeof (buf), fn);
  while (*buf == '#');
  if (!strncmp (buf, "P4", 2))
    {
      /* Binary monochrome. */
      parms.depth = 1;
      ppm_type = ppm_bitmap;
    }
  else if (!strncmp (buf, "P5", 2))
    {
      /* Grayscale. */
      parms.depth = 8;
      ppm_type = ppm_greyscale;
    }
  else if (!strncmp (buf, "P6", 2))
    {
      /* Color. */
      parms.depth = 8;
      ppm_type = ppm_color;
    }
  else
    {
      DBG(1, "getparmfromfile: %s is not a recognized PPM\n", filename);
      fclose (fn);
      return -1;
    }

  /* Skip comments. */
  do
    get_line (buf, sizeof (buf), fn);
  while (*buf == '#');
  sscanf (buf, "%d %d", &x, &y);

  parms.last_frame = SANE_TRUE;
  parms.bytes_per_line = (ppm_type == ppm_bitmap) ? (x + 7) / 8 : x;
  parms.pixels_per_line = x;
  if (hand_scanner)
    parms.lines = -1;
  else
    parms.lines = y;
  if ((ppm_type == ppm_greyscale) || (ppm_type == ppm_bitmap) || gray)
    parms.format = SANE_FRAME_GRAY;
  else
    {
      if (three_pass)
	{
	  parms.format = SANE_FRAME_RED + (pass + 1) % 3;
	  parms.last_frame = (pass >= 2);
	}
      else
	{
	  parms.format = SANE_FRAME_RGB;
	  parms.bytes_per_line *= 3;
	}
    }
  fclose (fn);
  return 0;
}

SANE_Status
sane_get_parameters (SANE_Handle handle,
		     SANE_Parameters * params)
{
  int rc = SANE_STATUS_GOOD;
  DBG(2, "sane_get_parameters\n");
  if (handle != MAGIC || !is_open)
    rc = SANE_STATUS_INVAL;	/* Unknown handle ... */
  else if (getparmfromfile ())
    rc = SANE_STATUS_INVAL;
  *params = parms;
  return rc;
}

SANE_Status
sane_start (SANE_Handle handle)
{
  char buf[1024];
  int nlines;
  
  DBG(2, "sane_start\n");
  if (handle != MAGIC || !is_open)
    return SANE_STATUS_INVAL;	/* Unknown handle ... */

  if (infile != NULL)
    {
      fclose (infile);
      infile = NULL;
      if (!three_pass || ++pass >= 3)
	return SANE_STATUS_EOF;
    }

  if (getparmfromfile ())
    return SANE_STATUS_INVAL;

  if ((infile = fopen (filename, "rb")) == NULL)
    return SANE_STATUS_INVAL;

  /* Skip the header (only two lines for a bitmap). */
  nlines = (ppm_type == ppm_bitmap) ? 1 : 0;
  while (nlines < 3)
    {
      /* Skip comments. */
      get_line (buf, sizeof (buf), infile);
      if (*buf != '#')
	nlines++;
    }

  return SANE_STATUS_GOOD;
}

static SANE_Int rgblength = 0;
static SANE_Byte *rgbbuf = 0;
static SANE_Byte rgbleftover[3] = {0, 0, 0};

SANE_Status
sane_read (SANE_Handle handle, SANE_Byte * data,
	   SANE_Int max_length, SANE_Int * length)
{
  int len, x, hlp;

  DBG(2,"sane_read: max_length = %d, rgbleftover = {%d, %d, %d}\n",
      max_length, rgbleftover[0], rgbleftover[1], rgbleftover[2]);
  if (handle != MAGIC || !is_open || !infile || !data || !length)
    return SANE_STATUS_INVAL;	/* Unknown handle or no file to read... */
    
  if (feof (infile))
    return SANE_STATUS_EOF;

  /* Allocate a buffer for the RGB values. */
  if (ppm_type == ppm_color && (gray || three_pass))
    {
      SANE_Byte *p, *q, *rgbend;

      if (rgbbuf == 0 || rgblength < 3 * max_length)
	{
	  /* Allocate a new rgbbuf. */
	  free (rgbbuf);
	  rgblength = 3 * max_length;
	  rgbbuf = malloc (rgblength);
	  if (rgbbuf == 0)
	    return SANE_STATUS_NO_MEM;
	}
      else rgblength = 3 * max_length;

      /* Copy any leftovers into the buffer. */
      q = rgbbuf;
      p = rgbleftover + 1;
      while (p - rgbleftover <= rgbleftover[0])
	*q++ = *p++;

      /* Slurp in the RGB buffer. */
      len = fread (q, 1, rgblength - rgbleftover[0], infile);
      rgbend = rgbbuf + len;

      q = data;
      if (gray)
	{
	  /* Zip through the buffer, converting color data to grayscale. */
	  for (p = rgbbuf; p < rgbend; p += 3)
	    *q++ = ((long) p[0] + p[1] + p[2]) / 3;
	}
      else
	{
	  /* Zip through the buffer, extracting data for this pass. */
	  for (p = (rgbbuf + (pass + 1) % 3); p < rgbend; p += 3)
	    *q++ = *p;
	}

      /* Save any leftovers in the array. */
      rgbleftover[0] = len % 3;
      p = rgbbuf + (len - rgbleftover[0]);
      q = rgbleftover + 1;
      while (p < rgbend)
	*q++ = *p++;

      len /= 3;
    }
  else
    /* Suck in as much of the file as possible, since it's already in the
       correct format. */
    len = fread (data, 1, max_length, infile);

  if (parms.depth == 8)
    /* Do the transformations ... DEMO ONLY ! THIS MAKES NO SENSE ! */
    for (x = 0; x < len; x++)
      {
	hlp = *((unsigned char *) data + x) - 128;
	hlp *= (contr + (100 << SANE_FIXED_SCALE_SHIFT));
	hlp /= 100 << SANE_FIXED_SCALE_SHIFT;
	hlp += (bright >> SANE_FIXED_SCALE_SHIFT) + 128;
	if (hlp < 0)
	  hlp = 0;
	if (hlp > 255)
	  hlp = 255;
	*(data + x) = hlp;
      }

  *length = len;
  return SANE_STATUS_GOOD;
}

void
sane_cancel (SANE_Handle handle)
{
  DBG(2, "sane_cancel: handle = %p\n", handle);
  pass = 0;
  if (infile != NULL)
    {
      fclose (infile);
      infile = NULL;
    }
}

SANE_Status
sane_set_io_mode (SANE_Handle handle, SANE_Bool non_blocking)
{
  DBG(2, "sane_set_io_mode: handle = %p, non_blocking = %d\n", handle,
      non_blocking);
  return SANE_STATUS_UNSUPPORTED;
}

SANE_Status
sane_get_select_fd (SANE_Handle handle, SANE_Int * fd)
{
  DBG(2, "sane_get_select_fd: handle = %p, fd %s 0\n", handle,
      fd ? "!=" : "=");
  return SANE_STATUS_UNSUPPORTED;
}
