/* sane - Scanner Access Now Easy.
   Copyright (C) 1999 Juergen G. Schimmer
   This file will (hopefully) become part of the SANE package.

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

   This file implements a SANE backend for v4l-Devices.  At
   present, only the Quickcam and bttv is supported though the driver
   should be able to easily accommodate other v4l-Devices.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
   NONINFRINGEMENT.  IN NO EVENT SHALL SCOTT LAIRD BE LIABLE FOR ANY
   CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
   CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
   WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.  */

#ifdef _AIX
# include "lalloca.h"   /* MUST come first for AIX! */
#endif

#include "sane/config.h"
#include "lalloca.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/mman.h>

#include <unistd.h>
#include <sys/time.h>
#include <sys/stat.h>


#include "sane/sane.h"
#include "sane/sanei.h"
#include "sane/saneopts.h"

#include <X11/Intrinsic.h>

#include "v4l-grab.h"
#if what_is_this
#  include "colorspace.h"
#endif
#include <sys/ioctl.h>
#include <asm/types.h>          /* XXX glibc */
#include <linux/videodev.h>

#define SYNC_TIMEOUT 1

#include "v4l.h"

#define BACKEND_NAME v4l
#include "sane/sanei_backend.h"

#ifndef PATH_MAX
# define PATH_MAX       1024
#endif

#include "sane/sanei_config.h"
#define V4L_CONFIG_FILE "v4l.conf"

static int num_devices;
static V4L_Device * first_dev;
static V4L_Scanner * first_handle;
static int v4ldev;
static char *buffer;
static int buffercount;


static const SANE_String_Const resolution_list[] =
  {
    "Low",      /* million-mode */
    "High",     /* billion-mode */
    0
  };


static const SANE_Int color_depth_list[] =
  {
    3 ,                 /* # of elements */
    6, 8, 24            /* "thousand" mode not implemented yet */
  };

static const SANE_Int xfer_scale_list[] =
  {
    4,                          /* # of elements */
    1, 2, 4 , 8
  };

static const SANE_Range u8_range =
  {
    /* min, max, quantization */
    0, 255, 0
  };

static const SANE_Range brightness_range =
  {
    /* min, max, quantization */
    0, 255, 0                   /* 255 is bulb mode! */
  };

SANE_Range x_range = {0,338,2};

SANE_Range odd_x_range = {1,339,2};

SANE_Range y_range = {0,249,1};

SANE_Range odd_y_range ={1,250,1};


static SANE_Parameters parms =
  {
     SANE_FRAME_RGB,
     1,                          /* 1 = Last Frame , 0 = More Frames to come */
     0,                          /* Number of bytes returned per scan line: */
     0,                          /* Number of pixels per scan line.  */
     0,                          /* Number of lines for the current scan.  */
     8,                          /* Number of bits per sample. */
   };


static struct video_capability  capability;
static struct video_picture     pict;
static struct video_window      window;
static struct video_mbuf        ov_mbuf;
static struct video_mmap        gb;

static SANE_Status
attach (const char *devname, V4L_Device **devp)
{
  V4L_Device * q;

  errno = 0;

  q = malloc (sizeof (*q));
  if (!q)
    return SANE_STATUS_NO_MEM;

  memset (q, 0, sizeof (*q));
  q->lock_fd = -1;
  q->version = V4L_COLOR;

  q->sane.name   = strdup (devname);
  q->sane.vendor = "V4L";
  q->sane.model  = capability.name;
  q->sane.type   = "virtual device";

  ++num_devices;
  q->next = first_dev;
  first_dev = q;


  if (devp)
    *devp = q;
  return SANE_STATUS_GOOD;

  free (q);
  return SANE_STATUS_INVAL;
}


static SANE_Status
init_options (V4L_Scanner *s)
{
  int i;

  memset (s->opt, 0, sizeof (s->opt));
  memset (s->val, 0, sizeof (s->val));

  x_range.min   =       0;
  x_range.max   =       capability.maxwidth - capability.minwidth;
  x_range.quant =       1;

  y_range.min   =       0;
  y_range.max   =       capability.maxheight - capability.minheight;
  y_range.quant =       1;

  odd_x_range.min =     capability.minwidth;
  odd_x_range.max =     capability.maxwidth;
  if (odd_x_range.max > 767)
    {
        odd_x_range.max = 767;
        x_range.max     = 767 - capability.minwidth;
    };
  odd_x_range.quant =   1;

  odd_y_range.min =     capability.minheight;
  odd_y_range.max =     capability.maxheight;
  if (odd_y_range.max > 511)
    {
        odd_y_range.max = 511;
        y_range.max     = 511 - capability.minheight;
    };
  odd_y_range.quant =   1;

  parms.depth   =       pict.depth;
  if (pict.depth < 8)
     parms.depth        =       8;
  parms.depth   =       8;
  parms.lines           = window.height;
  parms.pixels_per_line = window.width;
  switch(pict.palette)
     {
        case VIDEO_PALETTE_GREY:        /* Linear greyscale */
           {
                parms.format    =       SANE_FRAME_GRAY;
                parms.bytes_per_line  = window.width;
                break;
           }
        case VIDEO_PALETTE_HI240:       /* High 240 cube (BT848) */
           {
                parms.format    =       SANE_FRAME_RGB;
                parms.bytes_per_line  = window.width * 3;
                break;
           }
        case VIDEO_PALETTE_RGB565:      /* 565 16 bit RGB */
           {
                parms.format    =       SANE_FRAME_RGB;
                parms.bytes_per_line  = window.width * 3;
                break;
           }
        case VIDEO_PALETTE_RGB24:       /* 24bit RGB */
           {
                parms.format    =       SANE_FRAME_RGB;
                parms.bytes_per_line  = window.width * 3;
                break;
           }
        case VIDEO_PALETTE_RGB32:       /* 32bit RGB */
           {
                parms.format    =       SANE_FRAME_RGB;
                parms.bytes_per_line  = window.width * 3;
                break;
           }
        case VIDEO_PALETTE_RGB555:      /* 555 15bit RGB */
           {
                parms.format    =       SANE_FRAME_RGB;
                parms.bytes_per_line  = window.width * 3;
                break;
           }
        case VIDEO_PALETTE_YUV422:      /* YUV422 capture */
           {
                parms.format    =       SANE_FRAME_RGB;
                parms.bytes_per_line  = window.width * 3;
                break;
           }
        case VIDEO_PALETTE_YUYV:
           {
                parms.format    =       SANE_FRAME_RGB;
                parms.bytes_per_line  = window.width * 3;
                break;
           }
        case VIDEO_PALETTE_UYVY:        /*The great thing about standards is..*/
           {
                parms.format    =       SANE_FRAME_RGB;
                parms.bytes_per_line  = window.width * 3;
                break;
           }
        case VIDEO_PALETTE_YUV420:
           {
                parms.format    =       SANE_FRAME_RGB;
                parms.bytes_per_line  = window.width * 3;
                break;
           }
        case VIDEO_PALETTE_YUV411:      /* YUV411 capture */
           {
                parms.format    =       SANE_FRAME_RGB;
                parms.bytes_per_line  = window.width * 3;
                break;
           }
        case VIDEO_PALETTE_RAW:         /* RAW capture (BT848) */
           {
                parms.format    =       SANE_FRAME_RGB;
                parms.bytes_per_line  = window.width * 3;
                break;
           }
        case VIDEO_PALETTE_YUV422P:     /* YUV 4:2:2 Planar */
           {
                parms.format    =       SANE_FRAME_RGB;
                parms.bytes_per_line  = window.width * 3;
                break;
           }
        case VIDEO_PALETTE_YUV411P:     /* YUV 4:1:1 Planar */
           {
                parms.format    =       SANE_FRAME_RGB;
                parms.bytes_per_line  = window.width * 3;
                break;
           }
        case VIDEO_PALETTE_YUV420P:     /* YUV 4:2:0 Planar */
           {
                parms.format    =       SANE_FRAME_RGB;
                parms.bytes_per_line  = window.width * 3;
                break;
           }
        case VIDEO_PALETTE_YUV410P:     /* YUV 4:1:0 Planar */
           {
                parms.format    =       SANE_FRAME_RGB;
                parms.bytes_per_line  = window.width * 3;
                break;
           }
        default:
           {
                parms.format    =       SANE_FRAME_GRAY;
                parms.bytes_per_line  = window.width;
                break;
           }
     }

  for (i = 0; i < NUM_OPTIONS; ++i)
    {
      s->opt[i].size = sizeof (SANE_Word);
      s->opt[i].cap = (SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT
                       | SANE_CAP_ALWAYS_SETTABLE);
    }

  s->opt[OPT_NUM_OPTS].title = SANE_TITLE_NUM_OPTIONS;
  s->opt[OPT_NUM_OPTS].desc = SANE_DESC_NUM_OPTIONS;
  s->opt[OPT_NUM_OPTS].type = SANE_TYPE_INT;
  s->opt[OPT_NUM_OPTS].cap = SANE_CAP_SOFT_DETECT;
  s->val[OPT_NUM_OPTS].w = NUM_OPTIONS;

  /* "Mode" group: */

  s->opt[OPT_MODE_GROUP].title = "Scan Mode";
  s->opt[OPT_MODE_GROUP].desc = "";
  s->opt[OPT_MODE_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_MODE_GROUP].cap = 0;
  s->opt[OPT_MODE_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* resolution */
  s->opt[OPT_RESOLUTION].name = SANE_NAME_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].type = SANE_TYPE_STRING;
  s->opt[OPT_RESOLUTION].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_RESOLUTION].size = 5;      /* sizeof("High") */
  s->opt[OPT_RESOLUTION].unit = SANE_UNIT_NONE;
  s->opt[OPT_RESOLUTION].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_RESOLUTION].constraint.string_list = resolution_list;
  s->val[OPT_RESOLUTION].s = strdup (resolution_list[V4L_RES_LOW]);
  s->val[OPT_RESOLUTION].w = 1;

  /* bit-depth */
  s->opt[OPT_DEPTH].name = SANE_NAME_BIT_DEPTH;
  s->opt[OPT_DEPTH].title = "Pixel depth";
  s->opt[OPT_DEPTH].desc = "Number of bits per pixel.";
  s->opt[OPT_DEPTH].type = SANE_TYPE_INT;
  s->opt[OPT_DEPTH].unit = SANE_UNIT_BIT;
  s->opt[OPT_DEPTH].constraint_type = SANE_CONSTRAINT_WORD_LIST;
  s->opt[OPT_DEPTH].constraint.word_list = color_depth_list;
  s->val[OPT_DEPTH].w = pict.depth;
  if (pict.depth == 0)
     s->val[OPT_DEPTH].w = 8;

  /* test */
  s->opt[OPT_TEST].name = "test-image";
  s->opt[OPT_TEST].title = "Force test image";
  s->opt[OPT_TEST].desc =
    "Acquire a test-image instead of the image seen by the camera. "
    "The test image consists of red, green, and blue squares of "
    "32x32 pixels each.  Use this to find out whether the "
    "camera is connected properly.";
  s->opt[OPT_TEST].type = SANE_TYPE_BOOL;
  s->val[OPT_TEST].w = SANE_FALSE;

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
  s->opt[OPT_TL_X].type = SANE_TYPE_INT;
  s->opt[OPT_TL_X].unit = SANE_UNIT_PIXEL;
  s->opt[OPT_TL_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TL_X].constraint.range = &x_range;
  s->val[OPT_TL_X].w = 0;

  /* top-left y */
  s->opt[OPT_TL_Y].name = SANE_NAME_SCAN_TL_Y;
  s->opt[OPT_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
  s->opt[OPT_TL_Y].desc = SANE_DESC_SCAN_TL_Y;
  s->opt[OPT_TL_Y].type = SANE_TYPE_INT;
  s->opt[OPT_TL_Y].unit = SANE_UNIT_PIXEL;
  s->opt[OPT_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TL_Y].constraint.range = &y_range;
  s->val[OPT_TL_Y].w = 0;

  /* bottom-right x */
  s->opt[OPT_BR_X].name = SANE_NAME_SCAN_BR_X;
  s->opt[OPT_BR_X].title = SANE_TITLE_SCAN_BR_X;
  s->opt[OPT_BR_X].desc = SANE_DESC_SCAN_BR_X;
  s->opt[OPT_BR_X].type = SANE_TYPE_INT;
  s->opt[OPT_BR_X].unit = SANE_UNIT_PIXEL;
  s->opt[OPT_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_X].constraint.range = &odd_x_range;
  s->val[OPT_BR_X].w = capability.maxwidth;
  if (s->val[OPT_BR_X].w > 767)
    s->val[OPT_BR_X].w = 767;

  /* bottom-right y */
  s->opt[OPT_BR_Y].name = SANE_NAME_SCAN_BR_Y;
  s->opt[OPT_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
  s->opt[OPT_BR_Y].desc = SANE_DESC_SCAN_BR_Y;
  s->opt[OPT_BR_Y].type = SANE_TYPE_INT;
  s->opt[OPT_BR_Y].unit = SANE_UNIT_PIXEL;
  s->opt[OPT_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_Y].constraint.range = &odd_y_range;
  s->val[OPT_BR_Y].w = capability.maxheight;
  if (s->val[OPT_BR_Y].w > 511)
    s->val[OPT_BR_Y].w = 511;

  /* xfer-scale */
  s->opt[OPT_XFER_SCALE].name = "transfer-scale";
  s->opt[OPT_XFER_SCALE].title = "Transfer scale";
  s->opt[OPT_XFER_SCALE].desc =
    "The transferscale determines how many of the acquired pixels actually "
    "get sent to the computer.  For example, a transfer scale of 2 would "
    "request that every other acquired pixel would be omitted.  That is, "
    "when scanning a 200 pixel wide and 100 pixel tall area, the resulting "
    "image would be only 100x50 pixels large.  Using a large transfer scale "
    "improves acquisition speed, but reduces resolution.";
  s->opt[OPT_XFER_SCALE].type = SANE_TYPE_INT;
  s->opt[OPT_XFER_SCALE].constraint_type = SANE_CONSTRAINT_WORD_LIST;
  s->opt[OPT_XFER_SCALE].constraint.word_list = xfer_scale_list;
  s->val[OPT_XFER_SCALE].w = xfer_scale_list[1];

  /* despeckle */
  s->opt[OPT_DESPECKLE].name = "despeckle";
  s->opt[OPT_DESPECKLE].title = "Speckle filter";
  s->opt[OPT_DESPECKLE].desc = "Turning on this filter will remove the "
    "christmas lights that are typically present in dark images.";
  s->opt[OPT_DESPECKLE].type = SANE_TYPE_BOOL;
  s->opt[OPT_DESPECKLE].constraint_type = SANE_CONSTRAINT_NONE;
  s->val[OPT_DESPECKLE].w = 0;


  /* "Enhancement" group: */

  s->opt[OPT_ENHANCEMENT_GROUP].title = "Enhancement";
  s->opt[OPT_ENHANCEMENT_GROUP].desc = "";
  s->opt[OPT_ENHANCEMENT_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_ENHANCEMENT_GROUP].cap = 0;
  s->opt[OPT_ENHANCEMENT_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* brightness */
  s->opt[OPT_BRIGHTNESS].name = SANE_NAME_BRIGHTNESS;
  s->opt[OPT_BRIGHTNESS].title = SANE_TITLE_BRIGHTNESS;
  s->opt[OPT_BRIGHTNESS].desc = SANE_DESC_BRIGHTNESS
    "This controls the brightness of the Picture returned from"
    "the video4linux device.";
  s->opt[OPT_BRIGHTNESS].type = SANE_TYPE_INT;
  s->opt[OPT_BRIGHTNESS].cap |= SANE_CAP_AUTOMATIC;
  s->opt[OPT_BRIGHTNESS].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BRIGHTNESS].constraint.range = &brightness_range;
  s->val[OPT_BRIGHTNESS].w = pict.brightness / 256;

  /* contrast */
  s->opt[OPT_CONTRAST].name = SANE_NAME_CONTRAST;
  s->opt[OPT_CONTRAST].title = SANE_TITLE_CONTRAST;
  s->opt[OPT_CONTRAST].desc = SANE_DESC_CONTRAST
    " This controls the contrast of the Picture returned from"
    "the video4linux device";
  s->opt[OPT_CONTRAST].type = SANE_TYPE_INT;
  s->opt[OPT_CONTRAST].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_CONTRAST].constraint.range = &u8_range;
  s->val[OPT_CONTRAST].w = pict.contrast /256;

  /* black-level */
  s->opt[OPT_BLACK_LEVEL].name = SANE_NAME_BLACK_LEVEL;
  s->opt[OPT_BLACK_LEVEL].title = SANE_TITLE_BLACK_LEVEL;
  s->opt[OPT_BLACK_LEVEL].desc = SANE_DESC_BLACK_LEVEL
    " This value should be selected so that black areas just start "
    "to look really black (not gray).";
  s->opt[OPT_BLACK_LEVEL].type = SANE_TYPE_INT;
  s->opt[OPT_BLACK_LEVEL].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BLACK_LEVEL].constraint.range = &u8_range;
  s->val[OPT_BLACK_LEVEL].w = 0;

  /* white-level */
  s->opt[OPT_WHITE_LEVEL].name = SANE_NAME_WHITE_LEVEL;
  s->opt[OPT_WHITE_LEVEL].title = SANE_TITLE_WHITE_LEVEL;
  s->opt[OPT_WHITE_LEVEL].desc = SANE_DESC_WHITE_LEVEL
    " This value should be selected so that white areas just start "
    "to look really white (not gray).";
  s->opt[OPT_WHITE_LEVEL].type = SANE_TYPE_INT;
  s->opt[OPT_WHITE_LEVEL].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_WHITE_LEVEL].constraint.range = &u8_range;
  s->val[OPT_WHITE_LEVEL].w = pict.whiteness /256;

  /* hue */
  s->opt[OPT_HUE].name = SANE_NAME_HUE;
  s->opt[OPT_HUE].title = SANE_TITLE_HUE;
  s->opt[OPT_HUE].desc = SANE_DESC_HUE
    "This value should be selected so that pink looks realy pink";
  s->opt[OPT_HUE].type = SANE_TYPE_INT;
  s->opt[OPT_HUE].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_HUE].constraint.range = &u8_range;
  s->val[OPT_HUE].w = pict.hue / 256;

  /* saturation */
  s->opt[OPT_SATURATION].name = SANE_NAME_SATURATION;
  s->opt[OPT_SATURATION].title = SANE_TITLE_SATURATION;
  s->opt[OPT_SATURATION].desc = SANE_DESC_SATURATION
    "This value controlls wether the Picture looks Black and White"
    "or shows to much color";
  s->opt[OPT_SATURATION].type = SANE_TYPE_INT;
  s->opt[OPT_SATURATION].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_SATURATION].constraint.range = &u8_range;
  s->val[OPT_SATURATION].w = pict.colour / 256 ;

  return SANE_STATUS_GOOD;
}

SANE_Status
sane_init (SANE_Int *version_code, SANE_Auth_Callback authorize)
{
  char dev_name[PATH_MAX], * str;
  size_t len;
  FILE *fp;

  DBG_INIT();

  if (version_code)
    *version_code = SANE_VERSION_CODE (V_MAJOR, V_MINOR, 0);

  fp = sanei_config_open (V4L_CONFIG_FILE);
  if (!fp)
    {
      DBG(1, "sane_init: file `%s' not accessible\n", V4L_CONFIG_FILE);
      return SANE_STATUS_INVAL;
    }

  while (sanei_config_read(dev_name, sizeof (dev_name), fp))
    {
      if (dev_name[0] == '#')           /* ignore line comments */
        continue;
      len = strlen (dev_name);

      if (!len)
        continue;                       /* ignore empty lines */

      /* Remove trailing space and trailing comments */
      for (str = dev_name; *str && !isspace (*str) && *str != '#'; ++str);
      *str = '\0';
      v4ldev = open ( dev_name,O_RDWR);
      if (-1 != v4ldev)
        {
          if (-1 != ioctl(v4ldev,VIDIOCGCAP,&capability))
            {
              DBG(1, "sane_init: found videodev on `%s'\n", dev_name);
              attach (dev_name, 0);
            }
          else
            DBG(1, "sane_init: ioctl(%d,VIDIOCGCAP,..) failed on `%s'\n",
                v4ldev, dev_name);
          close (v4ldev);
        }
      else
        DBG(1, "sane_init: failed to open device `%s'\n", dev_name);
    }
  fclose (fp);
  return SANE_STATUS_GOOD;
}

void
sane_exit (void)
{
  V4L_Device *dev, *next;

  for (dev = first_dev; dev; dev = next)
    {
      next = dev->next;
      free ((void *) dev->sane.name);
      free (dev);
    }
}

SANE_Status
sane_get_devices (const SANE_Device *** device_list, SANE_Bool local_only)
{
  static const SANE_Device ** devlist = 0;
  V4L_Device *dev;
  int i;

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
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_open (SANE_String_Const devicename, SANE_Handle *handle)
{
  SANE_Status status;
  V4L_Device *dev;
  V4L_Scanner *s;

  DBG(4, "open(%s)\n", devicename);

  if (devicename[0])
    {
      v4ldev = open ( devicename,O_RDWR);
      if (-1 == v4ldev)
        return SANE_STATUS_DEVICE_BUSY;
      if (-1 == ioctl(v4ldev,VIDIOCGCAP,&capability))
        return SANE_STATUS_INVAL;
      status = attach (devicename, &dev);
      if (status != SANE_STATUS_GOOD)
        return status;
      /*if (VID_TYPE_CAPTURE & capability.type)
        syslog (LOG_NOTICE,"V4L Device can Capture");
      if (VID_TYPE_TUNER & capability.type)
        syslog (LOG_NOTICE,"V4L Device has a Tuner");
      if (VID_TYPE_TELETEXT & capability.type)
        syslog (LOG_NOTICE,"V4L Device supports Teletext");
      if (VID_TYPE_OVERLAY & capability.type)
        syslog (LOG_NOTICE,"V4L Device supports Overlay");
      if (VID_TYPE_CHROMAKEY & capability.type)
        syslog (LOG_NOTICE,"V4L Device uses Chromakey");
      if (VID_TYPE_CLIPPING & capability.type)
        syslog (LOG_NOTICE,"V4L Device supports Clipping");
      if (VID_TYPE_FRAMERAM & capability.type)
        syslog (LOG_NOTICE,"V4L Device overwrites Framebuffermemory");
      if (VID_TYPE_SCALES & capability.type)
        syslog (LOG_NOTICE,"V4L Device supports Hardware-scalling");
      if (VID_TYPE_MONOCHROME & capability.type)
        syslog (LOG_NOTICE,"V4L Device is Monochrome");
      if (VID_TYPE_SUBCAPTURE & capability.type)
        syslog (LOG_NOTICE,"V4L Device can capture parts of the Image");
*/
      if (-1 == ioctl(v4ldev,VIDIOCGPICT,&pict))
        return SANE_STATUS_INVAL;

      if (-1 == ioctl(v4ldev,VIDIOCGWIN,&window))
        return SANE_STATUS_INVAL;

      if (-1 == ioctl(v4ldev,VIDIOCGMBUF,&ov_mbuf))
        DBG(1, "No Fbuffer\n");
    }
  else
    /* empty devicname -> use first device */
    dev = first_dev;

  if (!dev)
    return SANE_STATUS_INVAL;

  s = malloc (sizeof (*s));
  if (!s)
    return SANE_STATUS_NO_MEM;
  memset (s, 0, sizeof (*s));
  s->user_corner = 0;
  s->value_changed = ~0;        /* ensure all options get updated */
  s->devicename = devicename;
  s->fd = v4ldev;
  init_options (s);

  /* insert newly opened handle into list of open handles: */
  s->next = first_handle;
  first_handle = s;

  *handle = s;

  return SANE_STATUS_GOOD;
}

void
sane_close (SANE_Handle handle)
{
  V4L_Scanner *prev, *s;

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
      DBG(1, "sane_close: bad handle %p\n", handle);
      return;           /* oops, not a handle we know about */
    }
  if (prev)
    prev->next = s->next;
  else
    first_handle = s->next;

  if (s->scanning)
    sane_cancel (handle);
  close(s->fd);
  free (s);
}

const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
  V4L_Scanner *s = handle;

  if ((unsigned) option >= NUM_OPTIONS)
    return 0;
  return s->opt + option;
}

SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option,
                     SANE_Action action, void *val, SANE_Int *info)
{
  V4L_Scanner *s = handle;
  SANE_Status status;
  SANE_Word cap;

  if (info)
    *info = 0;

  if (option >= NUM_OPTIONS)
    return SANE_STATUS_INVAL;

  cap = s->opt[option].cap;

  if (!SANE_OPTION_IS_ACTIVE (cap))
    return SANE_STATUS_INVAL;

  if (action == SANE_ACTION_GET_VALUE)
    {
      init_options(s);
      switch (option)
        {
          /* word options: */
        case OPT_NUM_OPTS:
        case OPT_DEPTH:
        case OPT_DESPECKLE:
        case OPT_TEST:
        case OPT_TL_X:
        case OPT_TL_Y:
        case OPT_BR_X:
        case OPT_BR_Y:
        case OPT_XFER_SCALE:
        case OPT_BRIGHTNESS:
        case OPT_CONTRAST:
        case OPT_BLACK_LEVEL:
        case OPT_WHITE_LEVEL:
        case OPT_HUE:
        case OPT_SATURATION:
          *(SANE_Word *) val = s->val[option].w;
          return SANE_STATUS_GOOD;
        default:
          DBG(1, "control_option: option %d unknown\n", option);
        }
    }
  else if (action == SANE_ACTION_SET_VALUE)
    {
      if (!SANE_OPTION_IS_SETTABLE (cap))
        {
        return SANE_STATUS_INVAL;
        }
      status = sanei_constrain_value (s->opt + option, val, info);
      if (status != SANE_STATUS_GOOD)
        return status;

      if (-1 == ioctl(s->fd,VIDIOCGWIN,&window))
        {
          DBG(1, "Can not get Window Geometry\n");
          /*return SANE_STATUS_INVAL;*/
        }
      window.clipcount  = 0;
      window.clips      = 0;
      window.height     = parms.lines;
      window.width      = parms.pixels_per_line;

      if (option >= OPT_TL_X && option <= OPT_BR_Y)
        {
        s->user_corner |= 1 << (option - OPT_TL_X);
        }
      assert (option <= 31);
      s->value_changed |= 1 << option;

      switch (option)
        {
          /* (mostly) side-effect-free word options: */
        case OPT_TL_X:
                break;
        case OPT_TL_Y:
                break;
        case OPT_BR_X:
                window.width    = *(SANE_Word *)val;
                parms.pixels_per_line   = *(SANE_Word *)val;
                if (info)
                  *info         |= SANE_INFO_RELOAD_PARAMS;
                break;
        case OPT_BR_Y:
                window.height   = *(SANE_Word *)val;
                parms.lines     = *(SANE_Word *)val;
                if (info)
                  *info         |= SANE_INFO_RELOAD_PARAMS;
                break;
        case OPT_XFER_SCALE:
                break;
        case OPT_DEPTH:
                pict.depth      = *(SANE_Word *)val;
                if (info)
                  *info         |= SANE_INFO_RELOAD_PARAMS;
                break;
          if (!s->scanning && info && s->val[option].w != *(SANE_Word *) val)
            /* only signal the reload params if we're not scanning---no point
               in creating the frontend useless work */
            if (info)
              *info |= SANE_INFO_RELOAD_PARAMS;
          /* fall through */
        case OPT_NUM_OPTS:
        case OPT_TEST:
        case OPT_DESPECKLE:
        case OPT_BRIGHTNESS:
                pict.brightness = *(SANE_Word *)val * 256;
                if (info)
                  *info         |= SANE_INFO_RELOAD_PARAMS;
                break;
        case OPT_CONTRAST:
                pict.contrast   = *(SANE_Word *)val * 256;
                /* if (info) *info         |= SANE_INFO_RELOAD_PARAMS;*/
                break;
        case OPT_BLACK_LEVEL:
        case OPT_WHITE_LEVEL:
                pict.whiteness  = *(SANE_Word *)val * 256;
                if (info)
                  *info         |= SANE_INFO_RELOAD_PARAMS;
                break;
        case OPT_HUE:
        case OPT_SATURATION:
          s->val[option].w = *(SANE_Word *) val;
        }
      if (-1 == ioctl(s->fd,VIDIOCSWIN,&window))
        {
          DBG(1, "Can not set Window Geometry\n");
          /*return SANE_STATUS_INVAL;*/
        }
      if (-1 == ioctl(s->fd,VIDIOCGWIN,&window))
        {
          DBG(1, "Can not get window geometry\n");
        }
      if (-1 == ioctl(s->fd,VIDIOCSPICT,&pict))
        {
          DBG(1, "Can not set Picture Parameters\n");
          /*return SANE_STATUS_INVAL;*/
        }
      return SANE_STATUS_GOOD;
    }
  else if (action == SANE_ACTION_SET_AUTO)
    {
      switch (option)
        {
        case OPT_BRIGHTNESS:
          /* not implemented yet */
          return SANE_STATUS_GOOD;

        default:
          break;
        }
    }
  return SANE_STATUS_INVAL;
}

SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters *params)
{
  V4L_Scanner *s = handle;

  if (-1 == ioctl(s->fd,VIDIOCGWIN,&window))
     return SANE_STATUS_INVAL;
  parms.pixels_per_line = window.width;
  parms.bytes_per_line  = window.width;
  if (parms.format == SANE_FRAME_RGB)
    parms.bytes_per_line = window.width * 3;
  parms.lines           = window.height;
  *params = parms;
  return SANE_STATUS_GOOD;

}

SANE_Status
sane_start (SANE_Handle handle)
{
  int len;
  V4L_Scanner *s;

  for (s = first_handle; s; s = s->next)
    {
      if (s == handle)
        break;
    }
  if (!s)
    {
      DBG(1, "sane_start: bad handle %p\n", handle);
      return SANE_STATUS_INVAL;         /* oops, not a handle we know about */
    }
  len = ioctl(v4ldev,VIDIOCGCAP,&capability);
  if (-1 == len)
    DBG(1, "Can not get capabilities\n");
  buffercount = 0;
  DBG(2, "sane_start\n");
  if (-1 == ioctl(s->fd,VIDIOCGMBUF,&ov_mbuf))
     {
        s->mmap = FALSE;
        buffer = malloc(capability.maxwidth * capability.maxheight * pict.depth);
        if (0 == buffer)
          return SANE_STATUS_NO_MEM;
        DBG(2, "V4L READING Frame\n");
        len = read (s->fd,buffer,parms.bytes_per_line * parms.lines);
        DBG(2, "V4L Frame read\n");
     }
  else
     {
        s->mmap = TRUE;
        DBG(2, "MMAP Frame\n");
        DBG(2, "Buffersize %d , Buffers %d , Offset %d\n",
            ov_mbuf.size,ov_mbuf.frames,ov_mbuf.offsets);
        buffer = mmap (0,ov_mbuf.size,PROT_READ|PROT_WRITE,MAP_SHARED,s->fd,0);
        DBG(2, "MMAPed Frame, Capture 1 Pict into %x\n",buffer);
        gb.frame = 0;
        gb.width = window.width;
        gb.width = parms.pixels_per_line;
        gb.height = window.height;
        gb.height = parms.lines;
        gb.format = pict.palette;
        DBG(2,"MMAPed Frame %d x %d with Palette %d\n",
            gb.width,gb.height,gb.format);
        len = ioctl(v4ldev,VIDIOCMCAPTURE,&gb);
        DBG(2, "mmap-result %d\n",len);
        DBG(2, "Frame %x\n",gb.frame);
#if 0
        len = ioctl(v4ldev,VIDIOCSYNC,&(gb.frame));
        if (-1 == len)
          {
            DBG(1, "Call to ioctl(%d, VIDIOCSYNC, ..) failed\n", v4ldev);
            return SANE_STATUS_INVAL;
          }
#endif
     }
  DBG(2, "sane_start: done\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_read (SANE_Handle handle, SANE_Byte *buf, SANE_Int max_len,
           SANE_Int *lenp)
{
  int i,min;
  V4L_Scanner *s = handle;

  if ((buffercount + 1) > (parms.lines * parms.bytes_per_line))
     {
        *lenp = 0;
        return SANE_STATUS_EOF;
     };
  min = parms.lines * parms.bytes_per_line;
  if (min > (max_len + buffercount))
     min = (max_len + buffercount);
  if (s->mmap == FALSE)
     {
          for (i=buffercount;i<(min+0);i++)
             {
                *(buf + i - buffercount) = *(buffer +i);
             };
          *lenp = (parms.lines * parms.bytes_per_line - buffercount);
          if (max_len < *lenp)
             *lenp = max_len;
          buffercount = i ;
     }
  else
     {
          for (i=buffercount;i<(min+0);i++)
             {
                *(buf + i - buffercount) = *(buffer +i);
                /*switch (i % 3)
                  {
                    case 0:
                        *(buf + i - buffercount) = *(buffer + i + 0);
                        break;
                    case 1:
                        *(buf + i - buffercount) = *(buffer + i);
                        break;
                    case 2:
                        *(buf + i - buffercount) = *(buffer + i - 0);
                  };*/
             };
          *lenp = (parms.lines * parms.bytes_per_line - buffercount);
          if ((i - buffercount) < *lenp)
             *lenp = (i-buffercount);
         /* syslog(LOG_NOTICE,"Transfering %d Bytes",*lenp);
          syslog(LOG_NOTICE,"Transfering from Byte %d to Byte %d",buffercount,i);*/
          buffercount = i;
     }
  return SANE_STATUS_GOOD;
}

void
sane_cancel (SANE_Handle handle)
{
  V4L_Scanner *s = handle;

  if ((buffer != 0) && (s->mmap == FALSE))
     free(buffer);
  buffer = 0;
}


SANE_Status
sane_set_io_mode (SANE_Handle handle, SANE_Bool non_blocking)
{
  return SANE_STATUS_UNSUPPORTED;
}

SANE_Status
sane_get_select_fd (SANE_Handle handle, SANE_Int * fd)
{
  return SANE_STATUS_UNSUPPORTED;
}
