/* sane - Scanner Access Now Easy.

   Copyright (C) 1998, 1999 Kazuya Fukuda, Abel Deuring

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
   If you do not wish that, delete this exception notice. */

#ifndef sharp_h
#define sharp_h 1

#include <sys/types.h>

typedef enum
  {
    OPT_NUM_OPTS = 0,

    OPT_MODE_GROUP,
    XOPT_MODE,
    OPT_HALFTONE,
    OPT_PAPER,
    OPT_GAMMA,

    OPT_SPEED,

    OPT_RESOLUTION_GROUP,
    OPT_RESOLUTION,
    OPT_X_RESOLUTION,
    OPT_Y_RESOLUTION,

    OPT_GEOMETRY_GROUP,
    OPT_TL_X,			/* top-left x */
    OPT_TL_Y,			/* top-left y */
    OPT_BR_X,			/* bottom-right x */
    OPT_BR_Y,			/* bottom-right y */

    OPT_ENHANCEMENT_GROUP,
    OPT_EDGE_EMPHASIS,
    XOPT_THRESHOLD,
    OPT_LIGHTCOLOR,
#ifdef USE_CUSTOM_GAMMA
    OPT_CUSTOM_GAMMA,
    OPT_GAMMA_VECTOR,
    OPT_GAMMA_VECTOR_R,
    OPT_GAMMA_VECTOR_G,
    OPT_GAMMA_VECTOR_B,
#endif
    /* must come last: */
    NUM_OPTIONS
  }
SHARP_Option;

typedef union
  {
    SANE_Word w;
    SANE_Word *wa;		/* word array */
    SANE_String s;
  }
Option_Value;

typedef struct SHARP_Info
  {
    SANE_Range xres_range;
    SANE_Range yres_range;
    SANE_Range x_range;
    SANE_Range y_range;
    SANE_Range threshold_range;

    SANE_Int xres_default;
    SANE_Int yres_default;
    SANE_Int x_default;
    SANE_Int y_default;
    SANE_Int bmu;
    SANE_Int mud;
  }
SHARP_Info;

typedef struct SHARP_Device
  {
    struct SHARP_Device *next;
    SANE_Device sane;
    SHARP_Info info;
  }
SHARP_Device;

typedef struct SHARP_Scanner
  {
    struct SHARP_Scanner *next;
    int fd;
    SHARP_Device *dev;
    SANE_Option_Descriptor opt[NUM_OPTIONS];
    Option_Value val[NUM_OPTIONS];
    SANE_Parameters params;

    SANE_Byte *buffer;
    size_t buf_used;
    size_t buf_pos;
    SANE_Byte *outbuffer;
    size_t outbuf_start;
    size_t outbuf_used;
    size_t outbuf_size;
    size_t outbuf_remain_read;
    SANE_Int modes;
    SANE_Int xres;
    SANE_Int yres;
    SANE_Int ulx;
    SANE_Int uly;
    SANE_Int width;
    SANE_Int length;
    SANE_Int threshold;
    SANE_Int image_composition;
    SANE_Int bpp;
    SANE_Int halftone;
    SANE_Bool reverse;
    SANE_Bool speed;
    SANE_Int gamma;
    SANE_Int edge;
    SANE_Int lightcolor;

    size_t bytes_to_read;
    size_t max_lines_to_read;
    size_t unscanned_lines;
    SANE_Bool scanning;
    SANE_Bool busy;
    SANE_Bool cancel;
#ifdef USE_CUSTOM_GAMMA
    SANE_Int gamma_table[4][256];
#endif
  }
SHARP_Scanner;

typedef struct SHARP_Send
{
    SANE_Int dtc;
    SANE_Int dtq;
    SANE_Int length;
    SANE_Byte *data;
}
SHARP_Send;

typedef struct WPDH
{
    u_char wpdh[6];
    u_char wdl[2];
} 
WPDH;

typedef struct WDB
{
    SANE_Byte wid;
    SANE_Byte autobit;
    SANE_Byte x_res[2];
    SANE_Byte y_res[2];

    SANE_Byte x_ul[4];
    SANE_Byte y_ul[4];
    SANE_Byte width[4];
    SANE_Byte length[4];

    SANE_Byte brightness;
    SANE_Byte threshold;
    SANE_Byte null_1;

    SANE_Byte image_composition;
    SANE_Byte bpp;

    SANE_Byte ht_pattern[2];
    SANE_Byte rif_padding;
    SANE_Byte null_2[4];
    SANE_Byte null_3[6];
    SANE_Byte eletu;
    SANE_Byte zooming_x[2];
    SANE_Byte zooming_y[2];
    SANE_Byte lightness_r[2];
    SANE_Byte lightness_g[2];
    SANE_Byte lightness_b[2];
    SANE_Byte lightness_bw[2];
    
}
WDB;

/* "extension" off the window descriptor block for the JX 250 */
typedef struct WDBX250
  {
    SANE_Byte moire_reduction[2];
    SANE_Byte threshold_red;
    SANE_Byte threshold_green;
    SANE_Byte threshold_blue;
    SANE_Byte draft;
    SANE_Byte scanning_time[4];
    SANE_Byte fixed_gamma;
    SANE_Byte x_axis_res_qualifier[2];
    SANE_Byte y_axis_res_qualifier[2];
  }
WDBX250;

typedef struct window_param
{
    WPDH wpdh;
    WDB wdb;
    WDBX250 wdbx250;
}
window_param;

typedef struct mode_sense_param
{
    SANE_Byte mode_data_length;
    SANE_Byte mode_param_header2;
    SANE_Byte mode_param_header3;
    SANE_Byte mode_desciptor_length;
    SANE_Byte resereved[5];
    SANE_Byte blocklength[3];
    SANE_Byte page_code;
    SANE_Byte constant6;
    SANE_Byte bmu;
    SANE_Byte res2;
    SANE_Byte mud[2];
    SANE_Byte res3;
    SANE_Byte res4;
}
mode_sense_param;

typedef struct mode_select_param
{
    SANE_Byte mode_param_header1;
    SANE_Byte mode_param_header2;
    SANE_Byte mode_param_header3;
    SANE_Byte mode_param_header4;
    SANE_Byte page_code;
    SANE_Byte constant6;
    SANE_Byte res1;
    SANE_Byte res2;
    SANE_Byte mud[2];
    SANE_Byte res3;
    SANE_Byte res4;
}
mode_select_param;

/* SCSI commands */
#define TEST_UNIT_READY        0x00
#define REQUEST_SENSE          0x03
#define INQUIRY                0x12
#define MODE_SELECT6           0x15
#define RESERVE_UNIT           0x16
#define RELEASE_UNIT           0x17
#define MODE_SENSE6            0x1a
#define SCAN                   0x1b
#define SEND_DIAGNOSTIC        0x1d
#define SET_WINDOW             0x24
#define GET_WINDOW             0x25
#define READ                   0x28
#define SEND                   0x2a

#define SENSE_LEN              18
#define INQUIRY_LEN            36
#define MODEPARAM_LEN          12
#define WINDOW_LEN             76

#define DEFAULT_BUFSIZE 128

#endif /* not sharp_h */
