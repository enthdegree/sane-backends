/* sane - Scanner Access Now Easy.
   Copyright (C) 2000 Jochen Eisinger <jochen.eisinger@gmx.net>
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

#ifndef mustek_pp_h
#define mustek_pp_h

#include <sys/types.h>
#include <sys/time.h>
#include <sane/sanei_debug.h>

/* #define for user authentification */
/* #undef	HAVE_AUTHORIZATION */

/* #define for invert option */
/* #undef	HAVE_INVERSION */

/* #define if you want to patch for ASIC 1505 */
/* #undef	PATCH_MUSTEK_PP_1505 */

#ifdef HAVE_AUTHORIZATION

#define MAX_LINE_LEN	512

  /* Here are the usernames and passwords stored
   * the file name is relative to SANE_CONFIG_DIR */

#define PASSWD_FILE	".auth"
#define SEPARATOR	':'

#define SALT		"je"

#endif

enum Mustek_PP_Option
{
  OPT_NUM_OPTS = 0,

  OPT_MODE_GROUP,
  OPT_MODE,
  OPT_RESOLUTION,
  OPT_PREVIEW,
  OPT_GRAY_PREVIEW,

  OPT_GEOMETRY_GROUP,
  OPT_TL_X,			/* top-left x */
  OPT_TL_Y,			/* top-left y */
  OPT_BR_X,			/* bottom-right x */
  OPT_BR_Y,			/* bottom-right y */

  OPT_ENHANCEMENT_GROUP,

#ifdef HAVE_INVERSION

  OPT_INVERT,

#endif
  OPT_CUSTOM_GAMMA,		/* use custom gamma tables? */
  /* The gamma vectors MUST appear in the order gray, red, green,
     blue.  */
  OPT_GAMMA_VECTOR,
  OPT_GAMMA_VECTOR_R,
  OPT_GAMMA_VECTOR_G,
  OPT_GAMMA_VECTOR_B,

  /* must come last: */
  NUM_OPTIONS
};

typedef union
{
  SANE_Word w;
  SANE_Word *wa;		/* word array */
  SANE_String s;
}
Option_Value;

typedef struct CCD_info
{
  /* CCD mode (color/grayscale/lineart) */
  int mode;
  /* inversion (true/false) */
  int invert;
  /* how many positions to skip until scanable area starts */
  int skipcount;
  /* how many positions to skip until scan area starts */
  int skipimagebytes;
  /* total skip, adjusted to resolution */
  int adjustskip;
  /* current resolution */
  int res;
  /* current hardware resolution */
  int hwres;
  /* how many positions to scan for one pixel */
  int res_step;
  /* how many lines to scan for one scanline */
  int line_step;
  /* current CCD channel (red/green or gray/blue) */
  int channel;
}
CCD_Info;

typedef struct Mustek_PP_Descriptor
{
  /* SANE device */
  SANE_Device sane;
  /* port no (0x378,0x278,0x3bc) */
  SANE_String port;
  /* maximal resolution (300/600) */
  SANE_Int max_res;
  /* max horiz */
  SANE_Int max_h_size;
  /* max vert */
  SANE_Int max_v_size;
  /* msecs to wait for bank change */
  unsigned int wait_bank;
  /* lines to scan in one read */
  SANE_Int strip_height;
  /* bytesize of scan buffer */
  long int buf_size;
  /* ASIC id (0xa8 0xa5, 0xa2) */
  u_char asic;
  /* CCD type (0,1,4,5) */
  u_char ccd;
  /* devices requires authentification (yes/no) */
  int requires_auth;
  /* color index to divide black from white (0-255) */
  int bw;
  /* seconds to wait for the lamp */
  int wait_lamp;
  /* use 600 dpi code (yes/no) */
  int use600;
}
Mustek_PP_Descriptor;

typedef struct Mustek_PP_Device
{
  /* next device */
  struct Mustek_PP_Device *next;

  /* device descriptor */
  Mustek_PP_Descriptor *desc;

  /* fd for sanei_pa4s2 */
  int fd;

  /* CCD status */
  CCD_Info CCD;
  /* used during calibration & return_home */
  CCD_Info Saved_CCD;

  /* options and values */
  SANE_Option_Descriptor opt[NUM_OPTIONS];
  Option_Value val[NUM_OPTIONS];

  /* gamma tables (all, red, green, blue) */
  SANE_Int gamma_table[4][256];

  /* scanning, idle, cancelled */
  int state;

  /* scan area */
  int TopX;
  int TopY;
  int BottomX;
  int BottomY;

  /* ASIC & CCD (see mustek_pp_descritpor) */
  int asic_id;
  int ccd_type;

  /* when did we turn the lamp on? */
  time_t lamp_on;

  /* image control for ASIC 0xA5 */
  int image_control;

  /* bank count (for all ASICs) */
  int bank_count;

  /* motor state for ASIC 0xA8 */
  int motor_phase;
  int motor_step;

  /* those are used to count the hardware line the scanner is at, the
     line the current bank is at and the lines we've scanned */
  int line;
  int line_diff;
  int ccd_line;
  int lines_left;
  int max_lines;
  int redline;
  int blueline;

  /* result from calibration */
  int blackpos;

  /* line distances during scan */
  int rdiff, gdiff, bdiff;

  /* calibration buffers (high cut) */
  SANE_Byte *calib_r;
  SANE_Byte *calib_g;
  SANE_Byte *calib_b;

  /* calibration values (low cut) */
  SANE_Byte ref_black, ref_red, ref_green, ref_blue;

  /* line distance buffers */
  SANE_Byte *green;
  SANE_Byte **red;
  SANE_Byte **blue;

  /* line distances */
  int blue_offs;
  int green_offs;

  /* scan buffer */
  SANE_Byte *buf;
  long int bufsize, buflen;

  /* current parameters */
  SANE_Parameters params;
  SANE_Range dpi_range;
  SANE_Range x_range;
  SANE_Range y_range;


  /* these are additional parameters for 600 dpi scanners */

  SANE_Byte motor_ctrl;
  SANE_Bool first_time;
  SANE_Byte unknown_value;
  SANE_Byte expose_time;
  SANE_Byte voltages[3];
  SANE_Bool send_voltages;


}
Mustek_PP_Device;


#if (!defined __GNUC__ || __GNUC__ < 2 || \
     __GNUC_MINOR__ < (defined __cplusplus ? 6 : 4))

#define __PRETTY_FUNCTION__	"mustek_pp"

#endif

#define DEBUG()		DBG(4, "%s(v%d.%d.%d-%s): line %d: debug exception\n", \
			  __PRETTY_FUNCTION__, V_MAJOR, V_MINOR,	\
			  MUSTEK_PP_BUILD, MUSTEK_PP_STATE, __LINE__)

#define ASSERT(cond)	if (!(cond))					\
			  {						\
                            DEBUG();					\
			    DBG(1, "ASSERT(%s) failed\n", STRINGIFY(cond)); \
			    DBG(1, "expect disaster...\n");\
			  }

/* Please note: ASSERT won't go away if you define NDEBUG, it just won't
 * output a message when ASSERT failes. So if "cond" does anything, it will
 * be executed, even if NDEBUG is defined... 
 */


#endif /* mustek_pp_h */
