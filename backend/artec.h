/* sane - Scanner Access Now Easy.
   Copyright (C) 1996 David Mosberger-Tang
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

   This file implements a SANE backend for the Ultima/Artec AT3 and
   A6000C scanners.

   Copyright (C) 1998, Chris Pinkham
   Released under the terms of the GPL.
   *NO WARRANTY*

   *********************************************************************
   For feedback/information:

   cpinkham@sh001.infi.net
   http://www4.infi.net/~cpinkham/sane/sane-artec-doc.html
   *********************************************************************
 */

#ifndef artec_h
#define artec_h

#include <sys/types.h>

#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#define MAX(a,b) (((a) > (b)) ? (a) : (b))

typedef enum
  {
    OPT_NUM_OPTS = 0,

    OPT_MODE_GROUP,
    OPT_MODE,
    OPT_X_RESOLUTION,
    OPT_Y_RESOLUTION,
    OPT_RESOLUTION_BIND,
    OPT_PREVIEW,
    OPT_GRAY_PREVIEW,
    OPT_NEGATIVE,

    OPT_GEOMETRY_GROUP,
    OPT_TL_X,			/* top-left x */
    OPT_TL_Y,			/* top-left y */
    OPT_BR_X,			/* bottom-right x */
    OPT_BR_Y,			/* bottom-right y */

    OPT_ENHANCEMENT_GROUP,
    OPT_QUALITY_CAL,
    OPT_CONTRAST,
    OPT_THRESHOLD,
    OPT_BRIGHTNESS,
    OPT_HALFTONE_PATTERN,
    OPT_FILTER_TYPE,

    /* must come last */
    NUM_OPTIONS
  }
ARTEC_Option;

typedef enum
  {
    ARTEC_COMP_LINEART = 0,
    ARTEC_COMP_HALFTONE,
    ARTEC_COMP_GRAY,
    ARTEC_COMP_UNSUPP1,
    ARTEC_COMP_UNSUPP2,
    ARTEC_COMP_COLOR
  }
ARTEC_Image_Composition;

typedef enum
  {
    ARTEC_DATA_IMAGE = 0,
    ARTEC_DATA_UNSUPP1,
    ARTEC_DATA_HALFTONE_PATTERN,	/* 2 */
    ARTEC_DATA_UNSUPP3,
    ARTEC_DATA_RED_SHADING,	/* 4 */
    ARTEC_DATA_GREEN_SHADING,	/* 5 */
    ARTEC_DATA_BLUE_SHADING,	/* 6 */
    ARTEC_DATA_WHITE_SHADING_OPT,	/* 7 */
    ARTEC_DATA_WHITE_SHADING_TRANS,	/* 8 */
    ARTEC_DATA_CAPABILITY_DATA,	/* 9 */
    ARTEC_DATA_DARK_SHADING,	/* 10, 0xA */
    ARTEC_DATA_RED_GAMMA_CURVE,	/* 11, 0xB */
    ARTEC_DATA_GREEN_GAMMA_CURVE,	/* 12, 0xC */
    ARTEC_DATA_BLUE_GAMMA_CURVE,	/* 13, 0xD */
    ARTEC_DATA_ALL_GAMMA_CURVE	/* 14, 0xE */
  }
ARTEC_Read_Data_Type;

typedef enum
  {
    ARTEC_CALIB_RGB = 0,
    ARTEC_CALIB_DARK_WHITE
  }
ARTEC_Calibrate_Method;

typedef enum
  {
    ARTEC_FILTER_MONO = 0,
    ARTEC_FILTER_RED,
    ARTEC_FILTER_GREEN,
    ARTEC_FILTER_BLUE
  }
ARTEC_Filter_Type;

typedef union
  {
    SANE_Word w;
    SANE_Word *wa;		/* word array */
    SANE_String s;
  }
Option_Value;

typedef struct ARTEC_Device
  {
    struct ARTEC_Device *next;
    SANE_Device sane;
    double width;
    SANE_Range x_range;
    SANE_Range x_dpi_range;
    SANE_Word *horz_resolution_list;
    double height;
    SANE_Range y_range;
    SANE_Range y_dpi_range;
    SANE_Word *vert_resolution_list;
    SANE_Range threshold_range;
    SANE_Range contrast_range;
    SANE_Range brightness_range;
    SANE_Word setwindow_cmd_size;
    SANE_Word calibrate_method;

    SANE_Bool support_cap_data_retrieve;
    SANE_Bool req_shading_calibrate;
    SANE_Bool req_rgb_line_offset;
    SANE_Bool req_rgb_char_shift;
    SANE_Bool opt_brightness;
  }
ARTEC_Device;

typedef struct ARTEC_Scanner
  {
    /* all the state needed to define a scan request: */
    struct ARTEC_Scanner *next;

    SANE_Option_Descriptor opt[NUM_OPTIONS];
    Option_Value val[NUM_OPTIONS];

    int scanning;
    SANE_Parameters params;
    size_t bytes_to_read;
    size_t line_offset;

    /* scan parameters */
    char *mode;
    int x_resolution;
    int y_resolution;
    int tl_x;
    int tl_y;

    int fd;			/* SCSI filedescriptor */

    /* scanner dependent/low-level state: */
    ARTEC_Device *hw;
  }
ARTEC_Scanner;

#endif /* artec_h */
