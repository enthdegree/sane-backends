/* sane - Scanner Access Now Easy.

   Copyright (C) 1997, 1998 Franck Schnefra, Michel Roelofs,
   Emmanuel Blot, Mikko Tyolajarvi, David Mosberger-Tang, Wolfgang Goeller
   and Kevin Charter

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

   This file implements a backend for the AGFA SnapScan flatbed
   scanner. */


/* $Id$
   SANE SnapScan backend */

#include <sane/config.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <sane/sane.h>
#include <sane/sanei.h>
#include <sane/sanei_scsi.h>

#ifndef PATH_MAX
# define PATH_MAX	1024
#endif

#define EXPECTED_MAJOR 1
#define MINOR_VERSION 3
#define BUILD 0

#include "snapscan.h"

#define BACKEND_NAME snapscan

#include <sane/sanei_backend.h>
#include <sane/saneopts.h>

#define MIN(x,y) ((x)<(y) ? (x) : (y))
#define MAX(x,y) ((x)>(y) ? (x) : (y))

#ifdef INOPERATIVE
#define P_200_TO_255(per) SANE_UNFIX(255.0*((per + 100)/200.0))
#endif

#include <sane/sanei_config.h>
#define SNAPSCAN_CONFIG_FILE "snapscan.conf"

/* debug levels */
#define DL_INFO        1
#define DL_MINOR_INFO  2
#define DL_MAJOR_ERROR 1
#define DL_MINOR_ERROR 2
#define DL_DATA_TRACE  5
#define DL_CALL_TRACE  10
#define DL_VERBOSE     30

#define CHECK_STATUS(s,caller,cmd) \
if ((s) != SANE_STATUS_GOOD) { DBG(DL_MAJOR_ERROR, "%s: %s command failed: %s\n", caller, (cmd), sane_strstatus(s)); return s; }



/*----- internal scanner operations -----*/

/* hardware configuration byte masks */

#define HCFG_ADC  0x80		/* AD converter 1 ==> 10bit, 0 ==> 8bit */
#define HCFG_ADF  0x40		/* automatic document feeder */
#define HCFG_TPO  0x20		/* transparency option */
#define HCFG_RB   0x10		/* ring buffer */
#define HCFG_HT16 0x08		/* 16x16 halftone matrices */
#define HCFG_HT8  0x04		/* 8x8 halftone matrices */
#define HCFG_SRA  0x02		/* scanline row average (high-speed colour) */

#define HCFG_HT   0x0c		/* support halftone matrices at all */

#define MM_PER_IN 25.4		/* # millimetres per inch */
#define IN_PER_MM 0.03937	/* # inches per millimetre  */

/* default option values */

#define DEFAULT_RES  300
#define DEFAULT_PREVIEW SANE_FALSE

#ifdef INOPERATIVE
#define DEFAULT_BRIGHTNESS 0
#define AUTO_BRIGHTNESS -100
#define DEFAULT_CONTRAST 0
#define AUTO_CONTRAST -100
#endif
#define DEFAULT_GAMMA SANE_FIX(1.8)
#define DEFAULT_HALFTONE SANE_FALSE
#define DEFAULT_NEGATIVE SANE_FALSE
#define DEFAULT_THRESHOLD 50

static SANE_Int def_rgb_lpr = 4;
static SANE_Int def_gs_lpr = 12;

/* ranges */
static const SANE_Range x_range =
{SANE_FIX (0.0), SANE_FIX (215.0), SANE_FIX (1.0)};	/* mm */
static const SANE_Range y_range =
{SANE_FIX (0.0), SANE_FIX (290.0), SANE_FIX (1.0)};	/* mm */
static const SANE_Range gamma_range =
{SANE_FIX (0.0), SANE_FIX (4.0), SANE_FIX (0.1)};
static const SANE_Range lpr_range =
{1, 50, 1};

/* default scan area size is the maximum area */
#define DEFAULT_TLX  SANE_FIX(0.0)
#define DEFAULT_TLY  SANE_FIX(0.0)
#define DEFAULT_BRX  (x_range.max)
#define DEFAULT_BRY  (y_range.max)

#ifdef INOPERATIVE
static const SANE_Range percent_range =
{
  -100 << SANE_FIXED_SCALE_SHIFT,
  100 << SANE_FIXED_SCALE_SHIFT,
  1 << SANE_FIXED_SCALE_SHIFT
};
#endif

static const SANE_Range positive_percent_range =
{
  0 << SANE_FIXED_SCALE_SHIFT,
  100 << SANE_FIXED_SCALE_SHIFT,
  1 << SANE_FIXED_SCALE_SHIFT
};

/* predefined scan mode names */
static char md_colour[] = "Colour";
static char md_bilevelcolour[] = "BiLevelColour";
static char md_greyscale[] = "GreyScale";
static char md_lineart[] = "LineArt";

/* predefined scan window setting names */
static char pdw_none[] = "none";
static char pdw_6X4[] = "6x4";
static char pdw_8X10[] = "8x10";
static char pdw_85X11[] = "8.5x11";

/* predefined dither matrix names */
static char dm_none[] = "Halftoning Unsupported";
static char dm_dd8x8[] = "DispersedDot8x8";
static char dm_dd16x16[] = "DispersedDot16x16";

/* strings */
static char lpr_desc[] =
"Number of scan lines to request in a SCSI read. "
"Changing this parameter allows you to tune the speed at which "
"data is read from the scanner during scans. If this is set too "
"low, the scanner will have to stop periodically in the middle of "
"a scan; if it's set too high, X-based frontends may stop responding "
"to X events and your system could bog down.";


/* authorization stuff */
static SANE_Auth_Callback auth = NULL;
#if notyet
static SANE_Char username[SANE_MAX_USERNAME_LEN];
static SANE_Char password[SANE_MAX_PASSWORD_LEN];
#endif



/* bit depth tables */

static u_char depths8[MD_NUM_MODES] =
{8, 1, 8, 1};
static u_char depths10[MD_NUM_MODES] =
{10, 1, 10, 1};


/* external routines */
#include "snapscan-310.c"


/* init_options -- initialize the option set for a scanner; expects the
   scanner structure's hardware configuration byte (hconfig) to be valid.

   ARGS: a pointer to an existing scanner structure
   RET:  nothing
   SIDE: the option set of *ps is initialized; this includes both
   the option descriptors and the option values themselves */

static
void
init_options (SnapScan_Scanner * ps)
{
  SANE_Option_Descriptor *po = ps->options;

  po[OPT_COUNT].name = SANE_NAME_NUM_OPTIONS;
  po[OPT_COUNT].title = SANE_TITLE_NUM_OPTIONS;
  po[OPT_COUNT].desc = SANE_DESC_NUM_OPTIONS;
  po[OPT_COUNT].type = SANE_TYPE_INT;
  po[OPT_COUNT].unit = SANE_UNIT_NONE;
  po[OPT_COUNT].size = sizeof (SANE_Word);
  po[OPT_COUNT].cap = SANE_CAP_INACTIVE;
  {
    static SANE_Range count_range =
    {NUM_OPTS, NUM_OPTS, 0};
    po[OPT_COUNT].constraint_type = SANE_CONSTRAINT_RANGE;
    po[OPT_COUNT].constraint.range = &count_range;
  }

  po[OPT_SCANRES].name = SANE_NAME_SCAN_RESOLUTION;
  po[OPT_SCANRES].title = SANE_TITLE_SCAN_RESOLUTION;
  po[OPT_SCANRES].desc = SANE_DESC_SCAN_RESOLUTION;
  po[OPT_SCANRES].type = SANE_TYPE_INT;
  po[OPT_SCANRES].unit = SANE_UNIT_DPI;
  po[OPT_SCANRES].size = sizeof (SANE_Word);
  po[OPT_SCANRES].cap =
    SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_AUTOMATIC;
  {
    static SANE_Word resolutions_300[] =
    {7, 50, 75, 100, 150, 200, 300, 600};
    static SANE_Word resolutions_310[] =
    {6, 50, 75, 100, 150, 200, 300};
    po[OPT_SCANRES].constraint_type = SANE_CONSTRAINT_WORD_LIST;
    switch (ps->pdev->model)
      {
      case SNAPSCAN310:
      case VUEGO310S:		/* WG changed */
	po[OPT_SCANRES].constraint.word_list = resolutions_310;
	break;

      case SNAPSCAN600:
	DBG (DL_MINOR_INFO, "600 dpi mode untested on SnapScan 600\nPlease report bugs\n");
      default:
	po[OPT_SCANRES].constraint.word_list = resolutions_300;
	break;
      }
  }
  ps->res = DEFAULT_RES;

  po[OPT_PREVIEW].name = SANE_NAME_PREVIEW;
  po[OPT_PREVIEW].title = SANE_TITLE_PREVIEW;
  po[OPT_PREVIEW].desc = SANE_DESC_PREVIEW;
  po[OPT_PREVIEW].type = SANE_TYPE_BOOL;
  po[OPT_PREVIEW].unit = SANE_UNIT_NONE;
  po[OPT_PREVIEW].size = sizeof (SANE_Word);
  po[OPT_PREVIEW].cap =
    SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_AUTOMATIC;
  po[OPT_PREVIEW].constraint_type = SANE_CONSTRAINT_NONE;
  ps->preview = DEFAULT_PREVIEW;

#ifdef INOPERATIVE
  po[OPT_BRIGHTNESS].name = SANE_NAME_BRIGHTNESS;
  po[OPT_BRIGHTNESS].title = SANE_TITLE_BRIGHTNESS;
  po[OPT_BRIGHTNESS].desc = SANE_DESC_BRIGHTNESS;
  po[OPT_BRIGHTNESS].type = SANE_TYPE_FIXED;
  po[OPT_BRIGHTNESS].unit = SANE_UNIT_PERCENT;
  po[OPT_BRIGHTNESS].size = sizeof (int);
  po[OPT_BRIGHTNESS].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT |
    SANE_CAP_AUTOMATIC;
  po[OPT_BRIGHTNESS].constraint_type = SANE_CONSTRAINT_RANGE;
  po[OPT_BRIGHTNESS].constraint.range = &percent_range;
  ps->bright = DEFAULT_BRIGHTNESS;

  po[OPT_CONTRAST].name = SANE_NAME_CONTRAST;
  po[OPT_CONTRAST].title = SANE_TITLE_CONTRAST;
  po[OPT_CONTRAST].desc = SANE_DESC_CONTRAST;
  po[OPT_CONTRAST].type = SANE_TYPE_FIXED;
  po[OPT_CONTRAST].unit = SANE_UNIT_PERCENT;
  po[OPT_CONTRAST].size = sizeof (int);
  po[OPT_CONTRAST].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT |
    SANE_CAP_AUTOMATIC;
  po[OPT_CONTRAST].constraint_type = SANE_CONSTRAINT_RANGE;
  po[OPT_CONTRAST].constraint.range = &percent_range;
  ps->contrast = DEFAULT_CONTRAST;
#endif

  po[OPT_MODE].name = SANE_NAME_SCAN_MODE;
  po[OPT_MODE].title = SANE_TITLE_SCAN_MODE;
  po[OPT_MODE].desc = SANE_DESC_SCAN_MODE;
  po[OPT_MODE].type = SANE_TYPE_STRING;
  po[OPT_MODE].unit = SANE_UNIT_NONE;
  po[OPT_MODE].size = 32;
  po[OPT_MODE].cap =
    SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  {
    static SANE_String_Const names_300[] =
    {md_colour, md_bilevelcolour, md_greyscale, md_lineart, NULL};
    static SANE_String_Const names_310[] =
    {md_colour, md_greyscale, md_lineart, NULL};
    po[OPT_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
    switch (ps->pdev->model)
      {
      case SNAPSCAN310:
      case SNAPSCAN600:
      case SNAPSCAN1236S:
      case VUEGO310S:		/* WG changed */
	po[OPT_MODE].constraint.string_list = names_310;
	break;

      default:
	po[OPT_MODE].constraint.string_list = names_300;
	break;
      }
  }
  ps->mode_s = md_colour;
  ps->mode = MD_COLOUR;

  po[OPT_PREVIEW_MODE].name = "preview-mode";
  po[OPT_PREVIEW_MODE].title = "Preview mode";
  po[OPT_PREVIEW_MODE].desc =
    "Select the mode for previews. Greyscale previews usually give "
    "the best combination of speed and detail.";
  po[OPT_PREVIEW_MODE].type = SANE_TYPE_STRING;
  po[OPT_PREVIEW_MODE].unit = SANE_UNIT_NONE;
  po[OPT_PREVIEW_MODE].size = 32;
  po[OPT_PREVIEW_MODE].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_ADVANCED;
  {
    static SANE_String_Const names_300[] =
    {md_colour, md_bilevelcolour, md_greyscale, md_lineart, NULL};
    static SANE_String_Const names_310[] =
    {md_colour, md_greyscale, md_lineart, NULL};
    po[OPT_PREVIEW_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
    switch (ps->pdev->model)
      {
      case SNAPSCAN310:
      case SNAPSCAN600:
      case SNAPSCAN1236S:
      case VUEGO310S:		/* WG changed */
	po[OPT_PREVIEW_MODE].constraint.string_list = names_310;
	break;

      default:
	po[OPT_PREVIEW_MODE].constraint.string_list = names_300;
	break;
      }
  }
  ps->preview_mode_s = md_greyscale;
  ps->preview_mode = MD_GREYSCALE;

  po[OPT_TLX].name = SANE_NAME_SCAN_TL_X;
  po[OPT_TLX].title = SANE_TITLE_SCAN_TL_X;
  po[OPT_TLX].desc = SANE_DESC_SCAN_TL_X;
  po[OPT_TLX].type = SANE_TYPE_FIXED;
  po[OPT_TLX].unit = SANE_UNIT_MM;
  po[OPT_TLX].size = sizeof (SANE_Word);
  po[OPT_TLX].cap =
    SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  po[OPT_TLX].constraint_type = SANE_CONSTRAINT_RANGE;
  po[OPT_TLX].constraint.range = &x_range;
  ps->tlx = DEFAULT_TLX;

  po[OPT_TLY].name = SANE_NAME_SCAN_TL_Y;
  po[OPT_TLY].title = SANE_TITLE_SCAN_TL_Y;
  po[OPT_TLY].desc = SANE_DESC_SCAN_TL_Y;
  po[OPT_TLY].type = SANE_TYPE_FIXED;
  po[OPT_TLY].unit = SANE_UNIT_MM;
  po[OPT_TLY].size = sizeof (SANE_Word);
  po[OPT_TLY].cap =
    SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  po[OPT_TLY].constraint_type = SANE_CONSTRAINT_RANGE;
  po[OPT_TLY].constraint.range = &y_range;
  ps->tly = DEFAULT_TLY;

  po[OPT_BRX].name = SANE_NAME_SCAN_BR_X;
  po[OPT_BRX].title = SANE_TITLE_SCAN_BR_X;
  po[OPT_BRX].desc = SANE_DESC_SCAN_BR_X;
  po[OPT_BRX].type = SANE_TYPE_FIXED;
  po[OPT_BRX].unit = SANE_UNIT_MM;
  po[OPT_BRX].size = sizeof (SANE_Word);
  po[OPT_BRX].cap =
    SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  po[OPT_BRX].constraint_type = SANE_CONSTRAINT_RANGE;
  po[OPT_BRX].constraint.range = &x_range;
  ps->brx = DEFAULT_BRX;

  po[OPT_BRY].name = SANE_NAME_SCAN_BR_Y;
  po[OPT_BRY].title = SANE_TITLE_SCAN_BR_Y;
  po[OPT_BRY].desc = SANE_DESC_SCAN_BR_Y;
  po[OPT_BRY].type = SANE_TYPE_FIXED;
  po[OPT_BRY].unit = SANE_UNIT_MM;
  po[OPT_BRY].size = sizeof (SANE_Word);
  po[OPT_BRY].cap =
    SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  po[OPT_BRY].constraint_type = SANE_CONSTRAINT_RANGE;
  po[OPT_BRY].constraint.range = &y_range;
  ps->bry = DEFAULT_BRY;


  po[OPT_PREDEF_WINDOW].name = "predef-window";
  po[OPT_PREDEF_WINDOW].title = "Predefined settings";
  po[OPT_PREDEF_WINDOW].desc =
    "Provides standard scanning areas for photographs, printed pages "
    "and the like.";
  po[OPT_PREDEF_WINDOW].type = SANE_TYPE_STRING;
  po[OPT_PREDEF_WINDOW].unit = SANE_UNIT_NONE;
  po[OPT_PREDEF_WINDOW].size = 32;
  po[OPT_PREDEF_WINDOW].cap =
    SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  {
    static SANE_String_Const names[] =
    {pdw_none, pdw_6X4, pdw_8X10, pdw_85X11, NULL};
    po[OPT_PREDEF_WINDOW].constraint_type = SANE_CONSTRAINT_STRING_LIST;
    po[OPT_PREDEF_WINDOW].constraint.string_list = names;
  }
  ps->predef_window = pdw_none;

  po[OPT_GAMMA_GS].name = SANE_NAME_ANALOG_GAMMA;
  po[OPT_GAMMA_GS].title = SANE_TITLE_ANALOG_GAMMA;
  po[OPT_GAMMA_GS].desc = SANE_DESC_ANALOG_GAMMA;
  po[OPT_GAMMA_GS].type = SANE_TYPE_FIXED;
  po[OPT_GAMMA_GS].unit = SANE_UNIT_NONE;
  po[OPT_GAMMA_GS].size = sizeof (SANE_Word);
  po[OPT_GAMMA_GS].cap =
    SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_INACTIVE;
  po[OPT_GAMMA_GS].constraint_type = SANE_CONSTRAINT_RANGE;
  po[OPT_GAMMA_GS].constraint.range = &gamma_range;
  ps->gamma_gs = DEFAULT_GAMMA;

  po[OPT_GAMMA_R].name = SANE_NAME_ANALOG_GAMMA_R;
  po[OPT_GAMMA_R].title = SANE_TITLE_ANALOG_GAMMA_R;
  po[OPT_GAMMA_R].desc = SANE_DESC_ANALOG_GAMMA_R;
  po[OPT_GAMMA_R].type = SANE_TYPE_FIXED;
  po[OPT_GAMMA_R].unit = SANE_UNIT_NONE;
  po[OPT_GAMMA_R].size = sizeof (SANE_Word);
  po[OPT_GAMMA_R].cap =
    SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  po[OPT_GAMMA_R].constraint_type = SANE_CONSTRAINT_RANGE;
  po[OPT_GAMMA_R].constraint.range = &gamma_range;
  ps->gamma_r = DEFAULT_GAMMA;

  po[OPT_GAMMA_G].name = SANE_NAME_ANALOG_GAMMA_G;
  po[OPT_GAMMA_G].title = SANE_TITLE_ANALOG_GAMMA_G;
  po[OPT_GAMMA_G].desc = SANE_DESC_ANALOG_GAMMA_G;
  po[OPT_GAMMA_G].type = SANE_TYPE_FIXED;
  po[OPT_GAMMA_G].unit = SANE_UNIT_NONE;
  po[OPT_GAMMA_G].size = sizeof (SANE_Word);
  po[OPT_GAMMA_G].cap =
    SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  po[OPT_GAMMA_G].constraint_type = SANE_CONSTRAINT_RANGE;
  po[OPT_GAMMA_G].constraint.range = &gamma_range;
  ps->gamma_g = DEFAULT_GAMMA;

  po[OPT_GAMMA_B].name = SANE_NAME_ANALOG_GAMMA_B;
  po[OPT_GAMMA_B].title = SANE_TITLE_ANALOG_GAMMA_B;
  po[OPT_GAMMA_B].desc = SANE_DESC_ANALOG_GAMMA_B;
  po[OPT_GAMMA_B].type = SANE_TYPE_FIXED;
  po[OPT_GAMMA_B].unit = SANE_UNIT_NONE;
  po[OPT_GAMMA_B].size = sizeof (SANE_Word);
  po[OPT_GAMMA_B].cap =
    SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  po[OPT_GAMMA_B].constraint_type = SANE_CONSTRAINT_RANGE;
  po[OPT_GAMMA_B].constraint.range = &gamma_range;
  ps->gamma_b = DEFAULT_GAMMA;

  po[OPT_HALFTONE].name = SANE_NAME_HALFTONE;
  po[OPT_HALFTONE].title = SANE_TITLE_HALFTONE;
  po[OPT_HALFTONE].desc = SANE_DESC_HALFTONE;
  po[OPT_HALFTONE].type = SANE_TYPE_BOOL;
  po[OPT_HALFTONE].unit = SANE_UNIT_NONE;
  po[OPT_HALFTONE].size = sizeof (SANE_Bool);
  po[OPT_HALFTONE].cap =
    SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_INACTIVE;
  po[OPT_HALFTONE].constraint_type = SANE_CONSTRAINT_NONE;
  ps->halftone = DEFAULT_HALFTONE;

  po[OPT_HALFTONE_PATTERN].name = SANE_NAME_HALFTONE_PATTERN;
  po[OPT_HALFTONE_PATTERN].title = SANE_TITLE_HALFTONE_PATTERN;
  po[OPT_HALFTONE_PATTERN].desc = SANE_DESC_HALFTONE_PATTERN;
  po[OPT_HALFTONE_PATTERN].type = SANE_TYPE_STRING;
  po[OPT_HALFTONE_PATTERN].unit = SANE_UNIT_NONE;
  po[OPT_HALFTONE_PATTERN].size = 32;
  po[OPT_HALFTONE_PATTERN].cap =
    SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_INACTIVE;
  po[OPT_HALFTONE_PATTERN].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  switch (ps->hconfig & HCFG_HT)
    {
    case HCFG_HT:		/* both 16x16, 8x8 matrices */
      {
	static SANE_String_Const names[] =
	{dm_dd8x8, dm_dd16x16, NULL};
	po[OPT_HALFTONE_PATTERN].constraint.string_list = names;
	ps->dither_matrix = dm_dd8x8;
      }
      break;
    case HCFG_HT16:		/* 16x16 matrices only */
      {
	static SANE_String_Const names[] =
	{dm_dd16x16, NULL};
	po[OPT_HALFTONE_PATTERN].constraint.string_list = names;
	ps->dither_matrix = dm_dd16x16;
      }
      break;
    case HCFG_HT8:		/* 8x8 matrices only */
      {
	static SANE_String_Const names[] =
	{dm_dd8x8, NULL};
	po[OPT_HALFTONE_PATTERN].constraint.string_list = names;
	ps->dither_matrix = dm_dd8x8;
      }
      break;
    default:			/* no halftone matrices */
      {
	static SANE_String_Const names[] =
	{dm_none, NULL};
	po[OPT_HALFTONE_PATTERN].constraint.string_list = names;
	ps->dither_matrix = dm_none;
      }
    }

  po[OPT_NEGATIVE].name = SANE_NAME_NEGATIVE;
  po[OPT_NEGATIVE].title = SANE_TITLE_NEGATIVE;
  po[OPT_NEGATIVE].desc = SANE_DESC_NEGATIVE;
  po[OPT_NEGATIVE].type = SANE_TYPE_BOOL;
  po[OPT_NEGATIVE].unit = SANE_UNIT_NONE;
  po[OPT_NEGATIVE].size = sizeof (SANE_Bool);
  po[OPT_NEGATIVE].cap =
    SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_INACTIVE;
  po[OPT_NEGATIVE].constraint_type = SANE_CONSTRAINT_NONE;
  ps->negative = DEFAULT_NEGATIVE;

  po[OPT_THRESHOLD].name = SANE_NAME_THRESHOLD;
  po[OPT_THRESHOLD].title = SANE_TITLE_THRESHOLD;
  po[OPT_THRESHOLD].desc = SANE_DESC_THRESHOLD;
  po[OPT_THRESHOLD].type = SANE_TYPE_FIXED;
  po[OPT_THRESHOLD].unit = SANE_UNIT_PERCENT;
  po[OPT_THRESHOLD].size = sizeof (SANE_Int);
  po[OPT_THRESHOLD].cap =
    SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_INACTIVE;
  po[OPT_THRESHOLD].constraint_type = SANE_CONSTRAINT_RANGE;
  po[OPT_THRESHOLD].constraint.range = &positive_percent_range;
  ps->threshold = DEFAULT_THRESHOLD;

  po[OPT_RGB_LPR].name = "rgb-lpr";
  po[OPT_RGB_LPR].title = "Colour lines per read";
  po[OPT_RGB_LPR].desc = lpr_desc;
  po[OPT_RGB_LPR].type = SANE_TYPE_INT;
  po[OPT_RGB_LPR].unit = SANE_UNIT_NONE;
  po[OPT_RGB_LPR].size = sizeof (SANE_Word);
  po[OPT_RGB_LPR].cap =
    SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
  po[OPT_RGB_LPR].constraint_type = SANE_CONSTRAINT_RANGE;
  po[OPT_RGB_LPR].constraint.range = &lpr_range;
  ps->rgb_lpr = def_rgb_lpr;

  po[OPT_GS_LPR].name = "gs-lpr";
  po[OPT_GS_LPR].title = "Greyscale lines per read";
  po[OPT_GS_LPR].desc = lpr_desc;
  po[OPT_GS_LPR].type = SANE_TYPE_INT;
  po[OPT_GS_LPR].unit = SANE_UNIT_NONE;
  po[OPT_GS_LPR].size = sizeof (SANE_Word);
  po[OPT_GS_LPR].cap =
    SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED |
    SANE_CAP_INACTIVE;
  po[OPT_GS_LPR].constraint_type = SANE_CONSTRAINT_RANGE;
  po[OPT_GS_LPR].constraint.range = &lpr_range;
  ps->gs_lpr = def_gs_lpr;

  po[OPT_SCSI_CMDS].name = "scsi-cmds";
  po[OPT_SCSI_CMDS].title = "SCSI commands (for debugging)";
  po[OPT_SCSI_CMDS].type = SANE_TYPE_GROUP;

  po[OPT_INQUIRY].name = "do-inquiry";
  po[OPT_INQUIRY].title = "Inquiry";
  po[OPT_INQUIRY].desc =
    "Send an Inquiry command to the scanner and dump out some of "
    "the current settings.";
  po[OPT_INQUIRY].type = SANE_TYPE_BUTTON;
  po[OPT_INQUIRY].cap = SANE_CAP_ADVANCED;
  po[OPT_INQUIRY].constraint_type = SANE_CONSTRAINT_NONE;

  po[OPT_SELF_TEST].name = "do-self-test";
  po[OPT_SELF_TEST].title = "Self test";
  po[OPT_SELF_TEST].desc =
    "Send a Self Test command to the scanner and report the result.";
  po[OPT_SELF_TEST].type = SANE_TYPE_BUTTON;
  po[OPT_SELF_TEST].cap = SANE_CAP_ADVANCED;
  po[OPT_SELF_TEST].constraint_type = SANE_CONSTRAINT_NONE;

  po[OPT_REQ_SENSE].name = "do-req-sense";
  po[OPT_REQ_SENSE].title = "Request sense";
  po[OPT_REQ_SENSE].desc =
    "Send a Request Sense command to the scanner, and print out the sense "
    "report.";
  po[OPT_REQ_SENSE].type = SANE_TYPE_BUTTON;
  po[OPT_REQ_SENSE].cap = SANE_CAP_ADVANCED;
  po[OPT_REQ_SENSE].constraint_type = SANE_CONSTRAINT_NONE;

  po[OPT_REL_UNIT].name = "do-rel-unit";
  po[OPT_REL_UNIT].title = "Release unit (cancel)";
  po[OPT_REL_UNIT].desc =
    "Send a Release Unit command to the scanner. This is the same as "
    "a cancel command.";
  po[OPT_REL_UNIT].type = SANE_TYPE_BUTTON;
  po[OPT_REL_UNIT].cap = SANE_CAP_ADVANCED;
  po[OPT_REL_UNIT].constraint_type = SANE_CONSTRAINT_NONE;

}

/* gamma table computation */

static void
gamma_8 (double gamma, u_char * buf)
{
  int i;
  double i_gamma = 1.0 / gamma;
  for (i = 0; i < 256; i++)
    {
      buf[i] = (u_char) (255 * pow ((double) i / 255, i_gamma) + 0.5);;
    }
}


/* dispersed-dot dither matrices; this is discussed in Foley, Van Dam,
   Feiner and Hughes, 2nd ed., pp 570-571.

   The function mfDn computes the nth dispersed-dot dither matrix Dn
   given D(n/2) and n; n is presumed to be a power of 2. D8 and D16
   are the matrices of interest to us, since the SnapScan supports
   only 8x8 and 16x16 dither matrices. */

static u_char D2[] =
{0, 2, 3, 1};

static u_char D4[16], D8[64], D16[256];

static
void
mkDn (u_char * Dn, u_char * Dn2, unsigned n)
{
  static u_char tmp[256];
  unsigned n2 = n / 2;
  unsigned nsq = n * n;
  unsigned i, r, imin, f;

  /* compute 4*D(n/2) */
  for (i = 0; i < nsq; i++)
    tmp[i] = (u_char) (4 * Dn2[i]);

  /* now the dither matrix */
  for (r = 0, imin = 0, f = 0; r < 2; r++, imin += n2)
    {
      unsigned c, jmin;
      for (c = 0, jmin = 0; c < 2; c++, jmin += n2, f++)
	{
	  unsigned i, i2, j, j2;
	  for (i = imin, i2 = 0; i < imin + n2; i++, i2++)
	    {
	      for (j = jmin, j2 = 0; j < jmin + n2; j++, j2++)
		Dn[i * n + j] = (u_char) (tmp[i2 * n2 + j2] + D2[f]);
	    }
	}
    }
}

/* scanner scsi commands */


/* a sensible sense handler, courtesy of Franck;
   the last argument is expected to be a pointer to the associated
   SnapScan_Scanner structure */
static SANE_Status
sense_handler (int scsi_fd, u_char * result, void *arg)
{
  static char me[] = "sense_handler";
  SnapScan_Scanner *pss = (SnapScan_Scanner *) arg;
  u_char sense, asc, ascq;
  char *sense_str = NULL, *as_str = NULL;
  SANE_Status status = SANE_STATUS_GOOD;

  DBG (DL_CALL_TRACE, "%s(%ld, %p, %p)\n", me, (long) scsi_fd,
       (void *) result, (void *) arg);

  sense = result[2] & 0x0f;
  asc = result[12];
  ascq = result[13];
  if (pss)
    {
      pss->asi1 = result[18];
      pss->asi2 = result[19];
    }

  if ((result[0] & 0x80) == 0)
    {
      DBG (DL_DATA_TRACE, "%s: sense key is invalid.\n", me);
      return SANE_STATUS_GOOD;	/* sense key invalid */
    }

  switch (sense)
    {
    case 0x00:			/* no sense */
      sense_str = "No sense.";
      break;
    case 0x02:			/* not ready */
      sense_str = "Not ready.";
      if (asc == 0x04 && ascq == 0x01)
	/* warming up; byte 18 contains remaining seconds */
	{
	  as_str = "Logical unit is in process of becoming ready.";
	  status = SANE_STATUS_DEVICE_BUSY;
	}
      break;
    case 0x04:			/* hardware error */
      sense_str = "Hardware error.";
      /* byte 18 and 19 detail the hardware problems */
      status = SANE_STATUS_IO_ERROR;
      break;
    case 0x05:			/* illegal request */
      sense_str = "Illegal request.";
      if (asc == 0x25 && ascq == 0x00)
	{
	  as_str = "Logical unit not supported.";
	}
      status = SANE_STATUS_IO_ERROR;
      break;
    case 0x09:			/* process error */
      sense_str = "Process error.";
      if (asc == 0x00 && ascq == 0x05)
	/* no documents in ADF */
	{
	  as_str = "End of data detected.";
	  status = SANE_STATUS_NO_DOCS;
	}
      else if (asc == 0x3b && ascq == 0x05)
	/* paper jam in ADF */
	{
	  as_str = "Paper jam.";
	  status = SANE_STATUS_JAMMED;
	}
      else if (asc == 0x3b && ascq == 0x09)
	/* scanning area exceeds end of paper in ADF */
	{
	  as_str = "Read past end of medium.";
	  status = SANE_STATUS_EOF;
	}
      break;
    default:
      DBG (DL_MINOR_ERROR, "%s: no handling for sense %x.\n", me, sense);
      break;
    }

  if (pss)
    {
      pss->sense_str = sense_str;
      pss->as_str = as_str;
    }
  return status;
}


static
  SANE_Status
open_scanner (SnapScan_Scanner * pss)
{
  SANE_Status status;
  DBG (DL_CALL_TRACE, "open_scanner\n");
  if (!pss->opens)
    status = sanei_scsi_open (pss->devname, &(pss->fd),
			      sense_handler, (void *) pss);
  else				/* already open */
    status = SANE_STATUS_GOOD;
  if (status == SANE_STATUS_GOOD)
    pss->opens++;
  return status;
}

static
void
close_scanner (SnapScan_Scanner * pss)
{
  DBG (DL_CALL_TRACE, "close_scanner\n");
  if (pss->opens)
    {
      pss->opens--;
      if (!pss->opens)
	sanei_scsi_close (pss->fd);
    }
}


/* SCSI commands */
#define TEST_UNIT_READY        0x00
#define INQUIRY                0x12
#define SEND                   0x2a
#define SET_WINDOW             0x24
#define SCAN                   0x1b
#define READ                   0x28
#define REQUEST_SENSE          0x03
#define RESERVE_UNIT           0x16
#define RELEASE_UNIT           0x17
#define SEND_DIAGNOSTIC        0x1d
#define GET_DATA_BUFFER_STATUS 0x34


#define SCAN_LEN 6
#define READ_LEN 10

/*  buffer tools */

static void
zero_buf (u_char * buf, size_t len)
{
  size_t i;
  for (i = 0; i < len; i++)
    {
      buf[i] = 0x00;
    }
}


#define BYTE_SIZE 8

static u_short
u_char_to_u_short (u_char * pc)
{
  u_short r = 0;
  r |= pc[0];
  r = r << BYTE_SIZE;
  r |= pc[1];
  return r;
}

static void
u_short_to_u_charp (u_short x, u_char * pc)
{
  pc[0] = (0xff00 & x) >> BYTE_SIZE;
  pc[1] = (0x00ff & x);
}

static void
u_int_to_u_char3p (u_int x, u_char * pc)
{
  pc[0] = (0xff0000 & x) >> 2 * BYTE_SIZE;
  pc[1] = (0x00ff00 & x) >> BYTE_SIZE;
  pc[2] = (0x0000ff & x);
}

static void
u_int_to_u_char4p (u_int x, u_char * pc)
{
  pc[0] = (0xff000000 & x) >> 3 * BYTE_SIZE;
  pc[1] = (0x00ff0000 & x) >> 2 * BYTE_SIZE;
  pc[2] = (0x0000ff00 & x) >> BYTE_SIZE;
  pc[3] = (0x000000ff & x);
}

/* Convert 'STRING   ' to 'STRING' by adding 0 after last non-space */
static void
remove_trailing_space(char *s)
{
  int position;

  if (NULL == s)
    return;

  for (position = strlen(s); position > 0 && ' ' == s[position-1];
       position--);
  s[position] = 0;  
}

#define INQUIRY_LEN 6
#define INQUIRY_RET_LEN 120

#define INQUIRY_VENDOR  8	/* Offset in reply data to vendor name */
#define INQUIRY_PRODUCT 16	/* Offset in reply data to product id */
#define INQUIRY_REV 32		/* Offset in reply data to revision level */
#define INQUIRY_PRL2 36		/* Product Revision Level 2 (AGFA) */
#define INQUIRY_HCFG 37		/* Hardware Configuration (AGFA) */
#define INQUIRY_PIX_PER_LINE  42/* Pixels per scan line (AGFA) */
#define INQUIRY_BYTE_PER_LINE 44/* Bytes per scan line (AGFA) */
#define INQUIRY_NUM_LINES     46/* number of scan lines (AGFA) */
#define INQUIRY_OPT_RES       48/* optical resolution (AGFA) */
#define INQUIRY_SCAN_SPEED    51/* scan speed (AGFA) */
#define INQUIRY_EXPTIME1      52/* exposure time, first digit (AGFA) */
#define INQUIRY_EXPTIME2      53/* exposure time, second digit (AGFA) */
#define INQUIRY_G2R_DIFF      54/* green to red difference (AGFA)*/
#define INQUIRY_B2R_DIFF      55/* green to red difference (AGFA)*/
#define INQUIRY_FIRMWARE      96/* firmware date and time (AGFA) */


/* a mini-inquiry reads only the first 36 bytes of inquiry data, and
   returns the vendor(7 chars) and model(16 chars); vendor and model
   must point to character buffers of size at least 8 and 17
   respectively */

static
  SANE_Status
mini_inquiry (int fd, char *vendor, char *model)
{
  static const char *me = "mini_inquiry";
  size_t read_bytes;
  char cmd[] =
  {INQUIRY, 0, 0, 0, 36, 0};
  char data[36];
  SANE_Status status;

  read_bytes = 36;

  DBG (DL_CALL_TRACE, "%s\n", me);
  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), data, &read_bytes);
  CHECK_STATUS (status, me, "sanei_scsi_cmd");

  memcpy (vendor, data + 8, 7);
  vendor[7] = 0;
  memcpy (model, data + 16, 16);
  model[16] = 0;

  remove_trailing_space(vendor);
  remove_trailing_space(model);

  return SANE_STATUS_GOOD;
}


static
  SANE_Status
inquiry (SnapScan_Scanner * pss)
{
  static const char *me = "inquiry";

  SANE_Status status;
  pss->read_bytes = INQUIRY_RET_LEN;

  zero_buf (pss->cmd, MAX_SCSI_CMD_LEN);
  pss->cmd[0] = INQUIRY;
  pss->cmd[4] = INQUIRY_RET_LEN;

  DBG (DL_CALL_TRACE, "%s\n", me);
  status = sanei_scsi_cmd (pss->fd, pss->cmd, INQUIRY_LEN,
			   pss->buf, &pss->read_bytes);
  CHECK_STATUS (status, me, "sanei_scsi_cmd");

  /* record current parameters */

  pss->actual_res =
    u_char_to_u_short (pss->buf + INQUIRY_OPT_RES);
  pss->pixels_per_line =
    u_char_to_u_short (pss->buf + INQUIRY_PIX_PER_LINE);
  pss->bytes_per_line =
    u_char_to_u_short (pss->buf + INQUIRY_BYTE_PER_LINE);
  pss->lines =
    u_char_to_u_short (pss->buf + INQUIRY_NUM_LINES);
  /* effective buffer size must be a whole number of scan lines */
  if (pss->lines)
    {
      pss->buf_sz =
	(SCANNER_BUF_SZ / pss->bytes_per_line) * pss->bytes_per_line;
    }
  else
    {
      pss->buf_sz = 0;
    }
  pss->expected_data_len = pss->bytes_per_line * pss->lines;
  pss->expected_read_bytes = 0;
  pss->read_bytes = 0;
  pss->hconfig = pss->buf[INQUIRY_HCFG];

  {
    char exptime[4] =
    {' ', '.', ' ', 0};
    exptime[0] = (char) (pss->buf[INQUIRY_EXPTIME1] + '0');
    exptime[2] = (char) (pss->buf[INQUIRY_EXPTIME2] + '0');
    pss->ms_per_line = atof (exptime) * (float) pss->buf[INQUIRY_SCAN_SPEED];
  }

  switch (pss->pdev->model)
    {
    case SNAPSCAN310:
    case SNAPSCAN600:
    case SNAPSCAN1236S:
    case VUEGO310S:		/* WG changed */
      rgb_buf_set_diff (pss,
			pss->buf[INQUIRY_G2R_DIFF],
			pss->buf[INQUIRY_B2R_DIFF]);
      break;
    default:
      break;
    }

  DBG (DL_DATA_TRACE, "%s: pixels per scan line = %lu\n",
       me, (u_long) pss->pixels_per_line);
  DBG (DL_DATA_TRACE, "%s: bytes per scan line = %lu\n",
       me, (u_long) pss->bytes_per_line);
  DBG (DL_DATA_TRACE, "%s: number of scan lines = %lu\n",
       me, (u_long) pss->lines);
  DBG (DL_DATA_TRACE, "%s: effective buffer size = %lu bytes, %lu lines\n",
       me, (u_long) pss->buf_sz,
       (u_long) (pss->buf_sz ? pss->buf_sz / pss->lines : 0));
  DBG (DL_DATA_TRACE, "%s: expected total scan data: %lu bytes\n", me,
       (u_long) pss->expected_data_len);

  return status;
}

static
  SANE_Status
test_unit_ready (SnapScan_Scanner * pss)
{
  static const char *me = "test_unit_ready";
  char cmd[] =
  {TEST_UNIT_READY, 0, 0, 0, 0, 0};
  SANE_Status status;

  DBG (DL_CALL_TRACE, "%s\n", me);
  status = sanei_scsi_cmd (pss->fd, cmd, sizeof (cmd), NULL, NULL);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DL_MAJOR_ERROR, "%s: scsi command error: %s\n",
	   me, sane_strstatus (status));
    }
  return status;
}


static
void
reserve_unit (SnapScan_Scanner * pss)
{
  static const char *me = "reserve_unit";
  char cmd[] =
  {RESERVE_UNIT, 0, 0, 0, 0, 0};
  SANE_Status status;

  DBG (DL_CALL_TRACE, "%s\n", me);
  status = sanei_scsi_cmd (pss->fd, cmd, sizeof (cmd), NULL, NULL);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DL_MAJOR_ERROR, "%s: scsi command error: %s\n",
	   me, sane_strstatus (status));
    }
}

static
void
release_unit (SnapScan_Scanner * pss)
{
  static const char *me = "release_unit";
  char cmd[] =
  {RELEASE_UNIT, 0, 0, 0, 0, 0};
  SANE_Status status;

  DBG (DL_CALL_TRACE, "%s\n", me);
  status = sanei_scsi_cmd (pss->fd, cmd, sizeof (cmd), NULL, NULL);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DL_MAJOR_ERROR, "%s: scsi command error: %s\n",
	   me, sane_strstatus (status));
    }
}


#define SEND_LENGTH 10
#define DTC_HALFTONE 0x02
#define DTC_GAMMA 0x03
#define DTC_SPEED 0x81
#define DTCQ_HALFTONE_BW8 0x00
#define DTCQ_HALFTONE_COLOR8 0x01
#define DTCQ_HALFTONE_BW16 0x80
#define DTCQ_HALFTONE_COLOR16 0x81
#define DTCQ_GAMMA_GRAY8 0x00
#define DTCQ_GAMMA_RED8 0x01
#define DTCQ_GAMMA_GREEN8 0x02
#define DTCQ_GAMMA_BLUE8 0x03
#define DTCQ_GAMMA_GRAY10 0x80
#define DTCQ_GAMMA_RED10 0x81
#define DTCQ_GAMMA_GREEN10 0x82
#define DTCQ_GAMMA_BLUE10 0x83

static
  SANE_Status
send (SnapScan_Scanner * pss, u_char dtc, u_char dtcq)
{
  static char me[] = "send";
  SANE_Status status;
  u_short tl;			/* transfer length */

  DBG (DL_CALL_TRACE, "%s\n", me);

  zero_buf (pss->buf, SEND_LENGTH);

  switch (dtc)
    {
    case DTC_HALFTONE:		/* halftone mask */
      switch (dtcq)
	{
	case DTCQ_HALFTONE_BW8:
	  tl = 64;		/* bw 8x8 table */
	  break;
	case DTCQ_HALFTONE_COLOR8:
	  tl = 3 * 64;		/* rgb 8x8 tables */
	  break;
	case DTCQ_HALFTONE_BW16:
	  tl = 256;		/* bw 16x16 table */
	  break;
	case DTCQ_HALFTONE_COLOR16:
	  tl = 3 * 256;		/* rgb 16x16 tables */
	  break;
	default:
	  DBG (DL_MAJOR_ERROR, "%s: bad halftone data type qualifier 0x%x\n",
	       me, dtcq);
	  return SANE_STATUS_INVAL;
	}
      break;
    case DTC_GAMMA:		/* gamma function */
      switch (dtcq)
	{
	case DTCQ_GAMMA_GRAY8:	/* 8-bit tables */
	case DTCQ_GAMMA_RED8:
	case DTCQ_GAMMA_GREEN8:
	case DTCQ_GAMMA_BLUE8:
	  tl = 256;
	  break;
	case DTCQ_GAMMA_GRAY10:/* 10-bit tables */
	case DTCQ_GAMMA_RED10:
	case DTCQ_GAMMA_GREEN10:
	case DTCQ_GAMMA_BLUE10:
	  tl = 1024;
	  break;
	default:
	  DBG (DL_MAJOR_ERROR, "%s: bad gamma data type qualifier 0x%x\n",
	       me, dtcq);
	  return SANE_STATUS_INVAL;
	}
      break;
    case DTC_SPEED:		/* static transfer speed */
      tl = 2;
      break;
    default:
      DBG (DL_MAJOR_ERROR, "%s: unsupported data type code 0x%x\n",
	   me, (unsigned) dtc);
      return SANE_STATUS_INVAL;
    }

  pss->buf[0] = SEND;
  pss->buf[2] = dtc;
  pss->buf[5] = dtcq;
  pss->buf[7] = (tl >> 8) & 0xff;
  pss->buf[8] = tl & 0xff;

  status = sanei_scsi_cmd (pss->fd, pss->buf, SEND_LENGTH + tl,
			   NULL, NULL);
  CHECK_STATUS (status, me, "sane_scsi_cmd");
  return status;
}


#define SET_WINDOW_LEN 10
#define SET_WINDOW_HEADER 10	/* header starts */
#define SET_WINDOW_HEADER_LEN 8
#define SET_WINDOW_DESC 18	/* window descriptor starts */
#define SET_WINDOW_DESC_LEN 48
#define SET_WINDOW_TRANSFER_LEN 56
#define SET_WINDOW_TOTAL_LEN 66
#define SET_WINDOW_RET_LEN   0	/* no returned data */

#define SET_WINDOW_P_TRANSFER_LEN 6
#define SET_WINDOW_P_DESC_LEN 6

#define SET_WINDOW_P_WIN_ID 0
#define SET_WINDOW_P_XRES   2
#define SET_WINDOW_P_YRES   4
#define SET_WINDOW_P_TLX    6
#define SET_WINDOW_P_TLY    10
#define SET_WINDOW_P_WIDTH  14
#define SET_WINDOW_P_LENGTH 18
#define SET_WINDOW_P_BRIGHTNESS   22
#define SET_WINDOW_P_THRESHOLD    23
#define SET_WINDOW_P_CONTRAST     24
#define SET_WINDOW_P_COMPOSITION  25
#define SET_WINDOW_P_BITS_PER_PIX 26
#define SET_WINDOW_P_HALFTONE_PATTERN  27
#define SET_WINDOW_P_PADDING_TYPE      29
#define SET_WINDOW_P_BIT_ORDERING      30
#define SET_WINDOW_P_COMPRESSION_TYPE  32
#define SET_WINDOW_P_COMPRESSION_ARG   33
#define SET_WINDOW_P_HALFTONE_FLAG     35
#define SET_WINDOW_P_DEBUG_MODE        40
#define SET_WINDOW_P_GAMMA_NO          41
#define SET_WINDOW_P_OPERATION_MODE    42
#define SET_WINDOW_P_RED_UNDER_COLOR   43
#define SET_WINDOW_P_BLUE_UNDER_COLOR  45
#define SET_WINDOW_P_GREEN_UNDER_COLOR 44

static
  SANE_Status
set_window (SnapScan_Scanner * pss)
{
  static const char *me = "set_window";
  SANE_Status status;

  u_char *pc;

  DBG (DL_CALL_TRACE, "%s\n", me);
  zero_buf (pss->cmd, MAX_SCSI_CMD_LEN);

  /* basic command */
  pc = pss->cmd;
  pc[0] = SET_WINDOW;
  u_int_to_u_char3p ((u_int) SET_WINDOW_TRANSFER_LEN,
		     pc + SET_WINDOW_P_TRANSFER_LEN);

  /* header; we support only one window */
  pc += SET_WINDOW_LEN;
  u_short_to_u_charp (SET_WINDOW_DESC_LEN, pc + SET_WINDOW_P_DESC_LEN);

  /* the sole window descriptor */
  pc += SET_WINDOW_HEADER_LEN;
  pc[SET_WINDOW_P_WIN_ID] = 0;
  u_short_to_u_charp (pss->res, pc + SET_WINDOW_P_XRES);
  u_short_to_u_charp (pss->res, pc + SET_WINDOW_P_YRES);
  {
    unsigned tlxp =
    (unsigned) (pss->actual_res * IN_PER_MM * SANE_UNFIX (pss->tlx));
    unsigned tlyp =
    (unsigned) (pss->actual_res * IN_PER_MM * SANE_UNFIX (pss->tly));
    unsigned brxp =
    (unsigned) (pss->actual_res * IN_PER_MM * SANE_UNFIX (pss->brx));
    unsigned bryp =
    (unsigned) (pss->actual_res * IN_PER_MM * SANE_UNFIX (pss->bry));
    unsigned tmp;

    /* we don't guard against brx < tlx and bry < tly in the options */
    if (brxp < tlxp)
      {
	tmp = tlxp;
	tlxp = brxp;
	brxp = tmp;
      }

    if (bryp < tlyp)
      {
	tmp = tlyp;
	tlyp = bryp;
	bryp = tmp;
      }

    u_int_to_u_char4p (tlxp, pc + SET_WINDOW_P_TLX);
    u_int_to_u_char4p (tlyp, pc + SET_WINDOW_P_TLY);
    u_int_to_u_char4p (MAX (((unsigned) (brxp - tlxp)), 75),
		       pc + SET_WINDOW_P_WIDTH);
    u_int_to_u_char4p (MAX (((unsigned) (bryp - tlyp)), 75),
		       pc + SET_WINDOW_P_LENGTH);
  }
#ifdef INOPERATIVE
  pc[SET_WINDOW_P_BRIGHTNESS] =
    (u_char) (255.0 * ((pss->bright + 100) / 200.0));
#endif
  pc[SET_WINDOW_P_THRESHOLD] =
    (u_char) (255.0 * (pss->threshold / 100.0));
#ifdef INOPERATIVE
  pc[SET_WINDOW_P_CONTRAST] =
    (u_char) (255.0 * ((pss->contrast + 100) / 200.0));
#endif
  {
    SnapScan_Mode mode = pss->mode;
    u_char bpp;

    if (pss->preview)
      mode = pss->preview_mode;

    bpp = pss->pdev->depths[mode];

    switch (mode)
      {
      case MD_COLOUR:
	pc[SET_WINDOW_P_COMPOSITION] = 0x05;	/* multi-level RGB */
	bpp *= 3;
	break;
      case MD_BILEVELCOLOUR:
	if (pss->halftone)
	  pc[SET_WINDOW_P_COMPOSITION] = 0x04;	/* halftone RGB */
	else
	  pc[SET_WINDOW_P_COMPOSITION] = 0x03;	/* bi-level RGB */
	bpp *= 3;
	break;
      case MD_GREYSCALE:
	pc[SET_WINDOW_P_COMPOSITION] = 0x02;	/* grayscale */
	break;
      default:
	if (pss->halftone)
	  pc[SET_WINDOW_P_COMPOSITION] = 0x01;	/* b&w halftone */
	else
	  pc[SET_WINDOW_P_COMPOSITION] = 0x00;	/* b&w (lineart) */
	break;
      }

    pc[SET_WINDOW_P_BITS_PER_PIX] = bpp;
    DBG (DL_DATA_TRACE, "%s: bits-per-pixel set to %d\n", me, (int) bpp);
  }
  /* the RIF bit is the high bit of the padding type */
  pc[SET_WINDOW_P_PADDING_TYPE] = 0x00 /*| (pss->negative ? 0x00 : 0x80)*/ ;
  pc[SET_WINDOW_P_HALFTONE_PATTERN] = 0;
  pc[SET_WINDOW_P_HALFTONE_FLAG] = 0x80;	/* always set; image composition
                                            determines whether halftone is
                                            actually used */

  u_short_to_u_charp (0x0000, pc + SET_WINDOW_P_BIT_ORDERING);	/* used? */
  pc[SET_WINDOW_P_COMPRESSION_TYPE] = 0;	/* none */
  pc[SET_WINDOW_P_COMPRESSION_ARG] = 0;	/* none applicable */
  pc[SET_WINDOW_P_DEBUG_MODE] = 2;	/* use full 128k buffer */
  pc[SET_WINDOW_P_GAMMA_NO] = 0x01;	/* downloaded table */
  pc[SET_WINDOW_P_OPERATION_MODE] = (pss->preview) ? 0x20 : 0x60;
  pc[SET_WINDOW_P_RED_UNDER_COLOR] = 0xff;	/* defaults */
  pc[SET_WINDOW_P_BLUE_UNDER_COLOR] = 0xff;
  pc[SET_WINDOW_P_GREEN_UNDER_COLOR] = 0xff;

  DBG (DL_CALL_TRACE, "%s\n", me);

  status = sanei_scsi_cmd (pss->fd, pss->cmd, SET_WINDOW_TOTAL_LEN,
			   NULL, NULL);
  CHECK_STATUS (status, me, "sanei_scsi_cmd");
  return status;
}

static
  SANE_Status
scan (SnapScan_Scanner * pss)
{
  static const char *me = "scan";
  SANE_Status status;

  DBG (DL_CALL_TRACE, "%s\n", me);
  zero_buf (pss->cmd, MAX_SCSI_CMD_LEN);
  pss->cmd[0] = SCAN;

  DBG (DL_CALL_TRACE, "%s\n", me);

  status = sanei_scsi_cmd (pss->fd, pss->cmd, SCAN_LEN, NULL, NULL);
  CHECK_STATUS (status, me, "sanei_scsi_cmd");
  return status;
}

/* supported read operations */

#define READ_IMAGE     0x00
#define READ_TRANSTIME 0x80

/* number of bytes expected must be in pss->expected_read_bytes */
static
  SANE_Status
scsi_read (SnapScan_Scanner * pss, u_char read_type)
{
  static const char *me = "scsi_read";
  SANE_Status status;

  DBG (DL_CALL_TRACE, "%s\n", me);
  zero_buf (pss->cmd, MAX_SCSI_CMD_LEN);
  pss->cmd[0] = READ;
  pss->cmd[2] = read_type;
  u_int_to_u_char3p (pss->expected_read_bytes, pss->cmd + 6);

  pss->read_bytes = pss->expected_read_bytes;

  status = sanei_scsi_cmd (pss->fd, pss->cmd, READ_LEN,
			   pss->buf, &pss->read_bytes);
  CHECK_STATUS (status, me, "sanei_scsi_cmd");
  return status;
}


static
  SANE_Status
request_sense (SnapScan_Scanner * pss)
{
  static const char *me = "request_sense";
  size_t read_bytes = 0;
  u_char cmd[] =
  {REQUEST_SENSE, 0, 0, 0, 20, 0};
  u_char data[20];
  SANE_Status status;

  read_bytes = 20;

  DBG (DL_CALL_TRACE, "%s\n", me);
  status = sanei_scsi_cmd (pss->fd, cmd, sizeof (cmd), data, &read_bytes);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DL_MAJOR_ERROR, "%s: scsi command error: %s\n",
	   me, sane_strstatus (status));
    }
  else
    {
      status = sense_handler (pss->fd, data, (void *) pss);
    }
  return status;
}


static
  SANE_Status
send_diagnostic (SnapScan_Scanner * pss)
{
  static const char *me = "send_diagnostic";
  u_char cmd[] =
  {SEND_DIAGNOSTIC, 0x04, 0, 0, 0, 0};	/* self-test */
  SANE_Status status;

  DBG (DL_CALL_TRACE, "%s\n", me);
  status = sanei_scsi_cmd (pss->fd, cmd, sizeof (cmd), NULL, NULL);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DL_MAJOR_ERROR, "%s: scsi command error: %s\n",
	   me, sane_strstatus (status));
    }
  return status;
}


#if 0

/* get_data_buffer_status: fetch the scanner's current data buffer
   status. If wait == 0, the scanner must respond immediately;
   otherwise it will wait until there is data available in the buffer
   before reporting. */

#define GET_DATA_BUFFER_STATUS_LEN 10
#define DESCRIPTOR_LENGTH 12

static
  SANE_Status
get_data_buffer_status (SnapScan_Scanner * pss, int wait)
{
  static const char *me = "get_data_buffer_status";
  SANE_Status status;

  pss->read_bytes = DESCRIPTOR_LENGTH;	/* one status descriptor only */

  DBG (DL_CALL_TRACE, "%s\n", me);

  zero_buf (pss->cmd, MAX_SCSI_CMD_LEN);
  pss->cmd[0] = GET_DATA_BUFFER_STATUS;


  if (wait)
    {
      pss->cmd[1] = 0x01;
    }
  u_short_to_u_charp (DESCRIPTOR_LENGTH, pss->cmd + 7);

  status = sanei_scsi_cmd (pss->fd, pss->cmd, GET_DATA_BUFFER_STATUS_LEN,
			   pss->buf, &pss->read_bytes);
  CHECK_STATUS (status, me, "sanei_scsi_cmd");
  return status;
}

#endif


static
  SANE_Status
wait_scanner_ready (SnapScan_Scanner * pss)
{
  static char me[] = "wait_scanner_ready";
  SANE_Status status;
  int retries;

  DBG (DL_CALL_TRACE, "%s\n", me);

  for (retries = 5; retries; retries--)
    {
      status = test_unit_ready (pss);
      if (status == SANE_STATUS_GOOD)
	{
	  status = request_sense (pss);
	  switch (status)
	    {
	    case SANE_STATUS_GOOD:
	      return status;
	      break;
	    case SANE_STATUS_DEVICE_BUSY:
	      /* first additional sense byte contains time to wait */
	      {
		int delay = pss->asi1 + 1;
		DBG (DL_INFO, "%s: scanner warming up. Waiting %ld seconds.\n",
		     me, (long) delay);
		sleep (delay);
	      }
	      break;
	    case SANE_STATUS_IO_ERROR:
	      /* hardware error; bail */
	      DBG (DL_MAJOR_ERROR, "%s: hardware error detected.\n", me);
	      return status;
	      break;		/* not reached */
	    default:
	      DBG (DL_MAJOR_ERROR,
		   "%s: unhandled request_sense result; trying again.\n",
		   me);
	      break;
	    }
	}
    }

  return status;
}

/*----- global data structures and access utilities -----*/


/* available device list */

static SnapScan_Device *first_device = NULL;	/* device list head */
static int n_devices = 0;	/* the device count */

/* list returned from sane_get_devices() */
static const SANE_Device **get_devices_list = NULL;

static SANE_Bool
device_already_in_list(SnapScan_Device *current, SANE_String_Const name)
{
  for ( ;NULL != current; current = current->pnext )
    if (0 == strcmp(name, current->dev.name))
      return SANE_TRUE;
  return SANE_FALSE;
}

static SANE_Status
add_device (SANE_String_Const name)
{
  int fd;
  static const char me[] = "add_device";
  SANE_Status status;
  SnapScan_Device *pd;
  SnapScan_Model model_num = UNKNOWN;
  int i, supported_vendor = 0;
  char vendor[8], model[17];

  DBG (DL_CALL_TRACE, "%s(%s)\n", me, name);

  /* Avoid adding the same device more then once */
  if (device_already_in_list(first_device, name)) {
    return SANE_STATUS_GOOD;
  }

  vendor[0] = model[0] = '\0';

  status = sanei_scsi_open (name, &fd, sense_handler, NULL);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DL_MAJOR_ERROR, "%s: error opening device %s: %s\n",
	   me, name, sane_strstatus (status));
      return status;
    }

  /* check that the device is legitimate */
  if ((status = mini_inquiry (fd, vendor, model)) != SANE_STATUS_GOOD)
    {
      DBG (DL_MAJOR_ERROR, "%s: mini_inquiry failed with %s.\n",
	   me, sane_strstatus (status));
      sanei_scsi_close (fd);
      return status;
    }

  DBG (DL_VERBOSE, "%s: Is vendor \"%s\" model \"%s\" a supported scanner?\n",
       me, vendor, model);

  /* check if this is one of our supported vendors */
  for (i = 0; i < known_vendors; i++)
    if (0 == strcasecmp (vendor, vendors[i]))
      {
	supported_vendor = 1;
	break;
      }
  if (!supported_vendor)
    {
      DBG (DL_MINOR_ERROR, "%s: \"%s %s\" is not an %s\n",
	   me, vendor, model,
	   "AGFA SnapScan model 300, 310, 600 and 1236s"
	   " or VUEGO model 310S"); /* WG changed */
      sanei_scsi_close (fd);
      return SANE_STATUS_INVAL;
    }

  /* Known vendor.  Check if it is one of our supported models */
  for (i = 0; i < known_scanners; i++)
    {
      if (0 == strcasecmp (model, scanners[i].scsi_name))
	{
	  model_num = scanners[i].id;
	  break;
	}
    }
  if (UNKNOWN == model_num)
    {
      DBG (DL_INFO, "%s: sorry, model %s is not supported.\n"
	   "Currently supported models are AGFA SnapScan model 300, 310, 600\n"
	   "and 1236s and VUEGO model 310S\n",
	   me, model);
      sanei_scsi_close (fd);
      return SANE_STATUS_INVAL;
    }

  sanei_scsi_close (fd);

  pd = (SnapScan_Device *) malloc (sizeof (SnapScan_Device));
  if (!pd)
    {
      DBG (DL_MAJOR_ERROR, "%s: out of memory allocating device.", me);
      return SANE_STATUS_NO_MEM;
    }
  pd->dev.name = strdup (name);
  pd->dev.vendor = strdup (vendor);
  pd->dev.model = strdup (model);
  pd->dev.type = strdup (SNAPSCAN_TYPE);
  pd->model = model_num;
  switch (model_num)
    {
    case SNAPSCAN300:
      pd->depths = depths8;
      break;
    default:
      pd->depths = depths10;
      break;
    }

  if (!pd->dev.name || !pd->dev.vendor || !pd->dev.model || !pd->dev.type)
    {
      DBG (DL_MAJOR_ERROR,
	   "%s: out of memory allocating device descriptor strings.\n", me);
      free (pd);
      return SANE_STATUS_NO_MEM;
    }

  pd->pnext = first_device;
  first_device = pd;
  n_devices++;

  return SANE_STATUS_GOOD;
}

/* find_device: find a device in the available list by name

   ARG: the device name

   RET: a pointer to the corresponding device record, or NULL if there
   is no such device */

static SnapScan_Device *
find_device (SANE_String_Const name)
{
  static char me[] = "find_device";
  SnapScan_Device *psd;

  DBG (DL_CALL_TRACE, "%s\n", me);

  for (psd = first_device; psd; psd = psd->pnext)
    {
      if (strcmp (psd->dev.name, name) == 0)
	{
	  return psd;
	}
    }
  return NULL;
}



/*----- functions in the scanner interface -----*/


SANE_Status
sane_init (SANE_Int * version_code,
		    SANE_Auth_Callback authorize)
{
  static const char me[] = "sane_snapscan_init";
  char dev_name[PATH_MAX];
  size_t len;
  FILE *fp;
  SANE_Status status;

  DBG_INIT ();

  DBG (DL_CALL_TRACE, "%s\n", me);

  /* version check */
  if (V_MAJOR != EXPECTED_MAJOR)
    {
      DBG (DL_MAJOR_ERROR,
	   "%s: this version of the SnapScan backend is intended for use\n"
	   "with SANE major version %ld, but the major version of this SANE\n"
	   "release is %ld. Sorry, but you need a different version of\n"
	   "this backend.\n\n",
	   me, (long) /*SANE_CURRENT_MAJOR*/V_MAJOR, (long) EXPECTED_MAJOR);
      return SANE_STATUS_INVAL;
    }

  if (version_code != NULL)
    {
      *version_code =
	SANE_VERSION_CODE (V_MAJOR, MINOR_VERSION, BUILD);
    }

  auth = authorize;

  /* build a device list here; lifted out of hp backend with minor
     modifications */

  fp = sanei_config_open (SNAPSCAN_CONFIG_FILE);
  if (!fp)
    {
      /* default to DEFAULT_DEVICE instead of insisting on config file */
      DBG (DL_INFO,
	   "%s: configuration file not found, defaulting to %s.\n",
	   me, DEFAULT_DEVICE);
      status = add_device (DEFAULT_DEVICE);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DL_MINOR_ERROR, "%s: failed to add device \"%s\"\n",
	       me, dev_name);
	}
    }
  else
    {
      while (fgets (dev_name, sizeof (dev_name), fp))
	{
	  if (dev_name[0] == '#')	/* ignore line comments */
	    continue;
	  len = strlen (dev_name);
	  if (dev_name[len - 1] == '\n')
	    dev_name[--len] = '\0';

	  if (!len)
	    continue;		/* ignore empty lines */

	  sanei_config_attach_matching_devices (dev_name, add_device);
	}
      fclose (fp);
    }

  /* compute the dither matrices */

  mkDn (D4, D2, 4);
  mkDn (D8, D4, 8);
  mkDn (D16, D8, 16);
  /* scale the D8 matrix from 0..63 to 0..255 */
  {
    u_char i;
    for (i = 0; i < 64; i++)
      {
	D8[i] = (u_char) (4 * D8[i] + 3);
      }
  }

  return SANE_STATUS_GOOD;
}

void
sane_exit (void)
{
  DBG (DL_CALL_TRACE, "sane_snapscan_exit\n");

  if (NULL != get_devices_list)
    free(get_devices_list);
  get_devices_list = NULL;

  /* just for safety, reset things to known values */
  auth = NULL;
}

SANE_Status
sane_get_devices (const SANE_Device *** device_list,
			   SANE_Bool local_only)
{
  static const char *me = "sane_snapscan_get_devices";
  DBG (DL_CALL_TRACE, "%s (%p, %ld)\n", me, (void *) device_list,
       (long) local_only);

  /* Waste the last list returned from this function */
  if (NULL != get_devices_list)
    free(get_devices_list);

  *device_list = (const SANE_Device **)
    malloc ((n_devices + 1) * sizeof (SANE_Device *));

  if (*device_list)
    {
      int i;
      SnapScan_Device *pdev;
      for (i = 0, pdev = first_device; pdev; i++, pdev = pdev->pnext)
	{
	  (*device_list)[i] = &(pdev->dev);
	}
      (*device_list)[i] = 0x0000 /*NULL*/ ;
    }
  else
    {
      DBG (DL_MAJOR_ERROR, "%s: out of memory\n", me);
      return SANE_STATUS_NO_MEM;
    }

  get_devices_list = *device_list;

  return SANE_STATUS_GOOD;
}

SANE_Status
sane_open (SANE_String_Const name, SANE_Handle * h)
{
  static const char *me = "sane_snapscan_open";
  SnapScan_Device *psd;
  SANE_Status status;

  DBG (DL_CALL_TRACE, "%s (%s, %p)\n", me, name, (void *) h);

  /* possible authorization required */

  /* device exists? */
  psd = find_device (name);
  if (!psd)
    {
      DBG (DL_MINOR_ERROR, "%s: device \"%s\" not in current device list.\n",
	   me, name);
      return SANE_STATUS_INVAL;
    }

  /* create and initialize the scanner structure */

  *h = (SnapScan_Scanner *) malloc (sizeof (SnapScan_Scanner));
  if (!*h)
    {
      DBG (DL_MAJOR_ERROR, "%s: out of memory creating scanner structure.\n",
	   me);
      return SANE_STATUS_NO_MEM;
    }

  {
    SnapScan_Scanner *pss = *(SnapScan_Scanner **) h;

    {
      pss->devname = strdup (name);
      if (!name)
	{
	  free (*h);
	  DBG (DL_MAJOR_ERROR, "%s: out of memory copying device name.\n", me);
	  return SANE_STATUS_NO_MEM;
	}
      pss->pdev = psd;
      pss->opens = 0;
      pss->sense_str = NULL;
      pss->as_str = NULL;
      pss->rgb_buf.data = NULL;

      /* temp file name and the temp file */
      {
	char tname[128];
	snprintf (tname, sizeof(tname), TMP_FILE_PREFIX "-%p", (void *) pss);
	if ((pss->tfd = open (tname, O_CREAT | O_RDWR | O_TRUNC, 0600)) == -1)
	  {
	    char str[200];
	    snprintf(str, sizeof(str), "Can't open temp file %s", tname);
	    DBG (DL_MAJOR_ERROR, "%s: %s\n", me, str);
	    perror (str);
	    free (*h);
	    return SANE_STATUS_ACCESS_DENIED;
	  }
	unlink (tname);
	pss->tmpfname = strdup (tname);
	if (!pss->tmpfname)
	  {
	    DBG (DL_MAJOR_ERROR, "%s: can't duplicate temp file name\n", me);
	    free (*h);
	    return SANE_STATUS_NO_MEM;
	  }
      }

      DBG (DL_VERBOSE, "%s: allocated scanner structure at %p\n",
	   me, (void *) pss);
    }

    status = open_scanner (pss);
    if (status != SANE_STATUS_GOOD)
      {
	DBG (DL_MAJOR_ERROR, "%s: open_scanner failed, status: %s\n",
	     me, sane_strstatus (status));
	free (pss);
	return SANE_STATUS_ACCESS_DENIED;
      }

    DBG (DL_MINOR_INFO, "%s: performing scanner self test.\n", me);
    status = send_diagnostic (pss);
    if (status != SANE_STATUS_GOOD)
      {
	DBG (DL_MINOR_INFO, "%s: send_diagnostic reports %s\n",
	     me, sane_strstatus (status));
	free (pss);
	return status;
      }
    DBG (DL_MINOR_INFO, "%s: self test passed.\n", me);

    /* option initialization depends on getting the hardware configuration
       byte */
    status = inquiry (pss);
    if (status != SANE_STATUS_GOOD)
      {
	DBG (DL_MAJOR_ERROR, "%s: error in inquiry command: %s\n",
	     me, sane_strstatus (status));
	free (pss);
	return status;
      }

    close_scanner (pss);
    init_options (pss);
    pss->state = ST_IDLE;
  }


  return SANE_STATUS_GOOD;
}

void
sane_close (SANE_Handle h)
{
  SnapScan_Scanner *pss = (SnapScan_Scanner *) h;
  DBG (DL_CALL_TRACE, "sane_snapscan_close (%p)\n", (void *) h);
  switch (pss->state)
    {
    case ST_SCAN_INIT:
    case ST_SCANNING:
      release_unit (pss);
      break;
    default:
      break;
    }
  close_scanner (pss);
  close (pss->tfd);
  free (pss->tmpfname);
  free (pss);
}

const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle h, SANE_Int n)
{
  DBG (DL_CALL_TRACE, "sane_snapscan_get_option_descriptor (%p, %ld)\n",
       (void *) h, (long) n);

  if (n < NUM_OPTS)
    {
      return ((SnapScan_Scanner *) h)->options + n;
    }
  return NULL;
}

SANE_Status
sane_control_option (SANE_Handle h, SANE_Int n,
			      SANE_Action a, void *v,
			      SANE_Int * i)
{
  static const char *me = "sane_snapscan_control_option";
  SnapScan_Scanner *pss = h;
  static SANE_Status status;

  DBG (DL_CALL_TRACE, "%s (%p, %ld, %ld, %p, %p)\n",
       me, (void *) h, (long) n, (long) a, v, (void *) i);

  status = open_scanner (pss);
  CHECK_STATUS (status, me, "open_scanner");

  /* possible authorization required */

  switch (a)
    {
    case SANE_ACTION_GET_VALUE:
      switch (n)
	{
	case OPT_COUNT:
	  *(SANE_Int *) v = NUM_OPTS;
	  break;
	case OPT_SCANRES:
	  *(SANE_Int *) v = pss->res;
	  break;
	case OPT_PREVIEW:
	  *(SANE_Bool *) v = pss->preview;
	  break;
	case OPT_MODE:
	  DBG (DL_VERBOSE, "%s: writing \"%s\" to location %p\n",
	       me, pss->mode_s, (SANE_String) v);
	  strcpy ((SANE_String) v, pss->mode_s);
	  break;
	case OPT_PREVIEW_MODE:
	  DBG (DL_VERBOSE, "%s: writing \"%s\" to location %p\n",
	       me, pss->preview_mode_s, (SANE_String) v);
	  strcpy ((SANE_String) v, pss->preview_mode_s);
	  break;
	case OPT_TLX:
	  *(SANE_Fixed *) v = pss->tlx;
	  break;
	case OPT_TLY:
	  *(SANE_Fixed *) v = pss->tly;
	  break;
	case OPT_BRX:
	  *(SANE_Fixed *) v = pss->brx;
	  break;
	case OPT_BRY:
	  *(SANE_Fixed *) v = pss->bry;
	  break;
#ifdef INOPERATIVE
	case OPT_BRIGHTNESS:
	  *(SANE_Int *) v = pss->bright << SANE_FIXED_SCALE_SHIFT;
	  break;
	case OPT_CONTRAST:
	  *(SANE_Int *) v = pss->contrast << SANE_FIXED_SCALE_SHIFT;
	  break;
#endif
	case OPT_PREDEF_WINDOW:
	  DBG (DL_VERBOSE, "%s: writing \"%s\" to location %p\n",
	       me, pss->predef_window, (SANE_String) v);
	  strcpy ((SANE_String) v, pss->predef_window);
	  break;
	case OPT_GAMMA_GS:
	  *(SANE_Fixed *) v = pss->gamma_gs;
	  break;
	case OPT_GAMMA_R:
	  *(SANE_Fixed *) v = pss->gamma_r;
	  break;
	case OPT_GAMMA_G:
	  *(SANE_Fixed *) v = pss->gamma_g;
	  break;
	case OPT_GAMMA_B:
	  *(SANE_Fixed *) v = pss->gamma_b;
	  break;
	case OPT_HALFTONE:
	  *(SANE_Bool *) v = pss->halftone;
	  break;
	case OPT_HALFTONE_PATTERN:
	  DBG (DL_VERBOSE, "%s: writing \"%s\" to location %p\n",
	       me, pss->dither_matrix, (SANE_String) v);
	  strcpy ((SANE_String) v, pss->dither_matrix);
	  break;
	case OPT_NEGATIVE:
	  *(SANE_Bool *) v = pss->negative;
	  break;
	case OPT_THRESHOLD:
	  *(SANE_Int *) v = pss->threshold << SANE_FIXED_SCALE_SHIFT;
	  break;
	case OPT_RGB_LPR:
	  *(SANE_Int *) v = pss->rgb_lpr;
	  break;
	case OPT_GS_LPR:
	  *(SANE_Int *) v = pss->gs_lpr;
	  break;
	default:
	  DBG (DL_MAJOR_ERROR, "%s: invalid option number %ld\n",
	       me, (long) n);
	  return SANE_STATUS_UNSUPPORTED;
	  break;		/* not reached */
	}
      break;
    case SANE_ACTION_SET_VALUE:
      switch (n)
	{
	case OPT_COUNT:
	  return SANE_STATUS_UNSUPPORTED;
	  break;
	case OPT_SCANRES:
	  pss->res = *(SANE_Int *) v;
	  if (i)
	    *i = SANE_INFO_RELOAD_PARAMS;
	  break;
	case OPT_PREVIEW:
	  pss->preview = *(SANE_Bool *) v;
	  if (i)
	    *i = SANE_INFO_RELOAD_PARAMS;
	  break;
	case OPT_MODE:
	  {
	    char *s = (SANE_String) v;
	    if (strcmp (s, md_colour) == 0)
	      {
		pss->mode_s = md_colour;
		pss->mode = MD_COLOUR;
		pss->options[OPT_GAMMA_GS].cap |= SANE_CAP_INACTIVE;
		pss->options[OPT_GAMMA_R].cap &= ~SANE_CAP_INACTIVE;
		pss->options[OPT_GAMMA_G].cap &= ~SANE_CAP_INACTIVE;
		pss->options[OPT_GAMMA_B].cap &= ~SANE_CAP_INACTIVE;
		pss->options[OPT_HALFTONE].cap |= SANE_CAP_INACTIVE;
		pss->options[OPT_HALFTONE_PATTERN].cap |= SANE_CAP_INACTIVE;
		pss->options[OPT_NEGATIVE].cap |= SANE_CAP_INACTIVE;
		pss->options[OPT_THRESHOLD].cap |= SANE_CAP_INACTIVE;
		pss->options[OPT_GS_LPR].cap |= SANE_CAP_INACTIVE;
		pss->options[OPT_RGB_LPR].cap &= ~SANE_CAP_INACTIVE;
	      }
	    else if (strcmp (s, md_bilevelcolour) == 0)
	      {
		int ht_cap = pss->hconfig & HCFG_HT;
		pss->mode_s = md_bilevelcolour;
		pss->mode = MD_BILEVELCOLOUR;
		pss->options[OPT_GAMMA_GS].cap |= SANE_CAP_INACTIVE;
		if (ht_cap)
		  pss->options[OPT_HALFTONE].cap &= ~SANE_CAP_INACTIVE;
		if (ht_cap && pss->halftone)
		  {
		    pss->options[OPT_GAMMA_R].cap &= ~SANE_CAP_INACTIVE;
		    pss->options[OPT_GAMMA_G].cap &= ~SANE_CAP_INACTIVE;
		    pss->options[OPT_GAMMA_B].cap &= ~SANE_CAP_INACTIVE;
		    pss->options[OPT_HALFTONE_PATTERN].cap &=
		      ~SANE_CAP_INACTIVE;
		  }
		else
		  {
		    pss->options[OPT_GAMMA_R].cap |= SANE_CAP_INACTIVE;
		    pss->options[OPT_GAMMA_G].cap |= SANE_CAP_INACTIVE;
		    pss->options[OPT_GAMMA_B].cap |= SANE_CAP_INACTIVE;
		    pss->options[OPT_HALFTONE_PATTERN].cap |=
		      SANE_CAP_INACTIVE;
		  }
		pss->options[OPT_NEGATIVE].cap |= SANE_CAP_INACTIVE;
		pss->options[OPT_THRESHOLD].cap |= SANE_CAP_INACTIVE;
		pss->options[OPT_GS_LPR].cap |= SANE_CAP_INACTIVE;
		pss->options[OPT_RGB_LPR].cap &= ~SANE_CAP_INACTIVE;
	      }
	    else if (strcmp (s, md_greyscale) == 0)
	      {
		pss->mode_s = md_greyscale;
		pss->mode = MD_GREYSCALE;
		pss->options[OPT_GAMMA_GS].cap &= ~SANE_CAP_INACTIVE;
		pss->options[OPT_GAMMA_R].cap |= SANE_CAP_INACTIVE;
		pss->options[OPT_GAMMA_G].cap |= SANE_CAP_INACTIVE;
		pss->options[OPT_GAMMA_B].cap |= SANE_CAP_INACTIVE;
		pss->options[OPT_HALFTONE].cap |= SANE_CAP_INACTIVE;
		pss->options[OPT_HALFTONE_PATTERN].cap |= SANE_CAP_INACTIVE;
		pss->options[OPT_NEGATIVE].cap |= SANE_CAP_INACTIVE;
		pss->options[OPT_THRESHOLD].cap |= SANE_CAP_INACTIVE;
		pss->options[OPT_GS_LPR].cap &= ~SANE_CAP_INACTIVE;
		pss->options[OPT_RGB_LPR].cap |= SANE_CAP_INACTIVE;
	      }
	    else if (strcmp (s, md_lineart) == 0)
	      {
		int ht_cap = pss->hconfig & HCFG_HT;
		pss->mode_s = md_lineart;
		pss->mode = MD_LINEART;
		if (ht_cap)
		  pss->options[OPT_HALFTONE].cap &= ~SANE_CAP_INACTIVE;
		if (ht_cap && pss->halftone)
		  {
		    pss->options[OPT_GAMMA_GS].cap &= ~SANE_CAP_INACTIVE;
		    pss->options[OPT_HALFTONE_PATTERN].cap &=
		      ~SANE_CAP_INACTIVE;
		    pss->options[OPT_THRESHOLD].cap |= SANE_CAP_INACTIVE;
		  }
		else
		  {
		    pss->options[OPT_GAMMA_GS].cap |= SANE_CAP_INACTIVE;
		    pss->options[OPT_HALFTONE_PATTERN].cap |=
		      SANE_CAP_INACTIVE;
		    pss->options[OPT_THRESHOLD].cap &= ~SANE_CAP_INACTIVE;
		  }
		pss->options[OPT_GAMMA_R].cap |= SANE_CAP_INACTIVE;
		pss->options[OPT_GAMMA_G].cap |= SANE_CAP_INACTIVE;
		pss->options[OPT_GAMMA_B].cap |= SANE_CAP_INACTIVE;
		pss->options[OPT_NEGATIVE].cap &= ~SANE_CAP_INACTIVE;
		pss->options[OPT_GS_LPR].cap &= ~SANE_CAP_INACTIVE;
		pss->options[OPT_RGB_LPR].cap |= SANE_CAP_INACTIVE;
	      }
	    else
	      {
		DBG (DL_MAJOR_ERROR, "%s: internal error: given illegal mode "
		     "string \"%s\"\n", me, s);
	      }
	  }
	  if (i)
	    *i = SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
	  break;
	case OPT_PREVIEW_MODE:
	  {
	    char *s = (SANE_String) v;
	    if (strcmp (s, md_colour) == 0)
	      {
		pss->preview_mode_s = md_colour;
		pss->preview_mode = MD_COLOUR;
	      }
	    else if (strcmp (s, md_bilevelcolour) == 0)
	      {
		pss->preview_mode_s = md_bilevelcolour;
		pss->preview_mode = MD_BILEVELCOLOUR;
	      }
	    else if (strcmp (s, md_greyscale) == 0)
	      {
		pss->preview_mode_s = md_greyscale;
		pss->preview_mode = MD_GREYSCALE;
	      }
	    else if (strcmp (s, md_lineart) == 0)
	      {
		pss->preview_mode_s = md_lineart;
		pss->preview_mode = MD_LINEART;
	      }
	    else
	      DBG (DL_MAJOR_ERROR,
		   "%s: internal error: given illegal mode string "
		   "\"%s\"\n", me, s);
	    if (i)
	      *i = 0;
	    break;
	  }
	case OPT_TLX:
	  pss->tlx = *(SANE_Fixed *) v;
	  pss->predef_window = pdw_none;
	  if (i)
	    *i = SANE_INFO_RELOAD_PARAMS;
	  break;
	case OPT_TLY:
	  pss->tly = *(SANE_Fixed *) v;
	  pss->predef_window = pdw_none;
	  if (i)
	    *i = SANE_INFO_RELOAD_PARAMS;
	  break;
	case OPT_BRX:
	  pss->brx = *(SANE_Fixed *) v;
	  pss->predef_window = pdw_none;
	  if (i)
	    *i = SANE_INFO_RELOAD_PARAMS;
	  break;
	case OPT_BRY:
	  pss->bry = *(SANE_Fixed *) v;
	  pss->predef_window = pdw_none;
	  if (i)
	    *i = SANE_INFO_RELOAD_PARAMS;
	  break;
#ifdef INOPERATIVE
	case OPT_BRIGHTNESS:
	  pss->bright = *(SANE_Int *) v >> SANE_FIXED_SCALE_SHIFT;
	  if (i)
	    *i = 0;
	  break;
	case OPT_CONTRAST:
	  pss->contrast = *(SANE_Int *) v >> SANE_FIXED_SCALE_SHIFT;
	  if (i)
	    *i = 0;
	  break;
#endif
	case OPT_PREDEF_WINDOW:
	  {
	    char *s = (SANE_String) v;
	    if (strcmp (s, pdw_none) != 0)
	      {
		pss->tlx = 0;
		pss->tly = 0;

		if (strcmp (s, pdw_6X4) == 0)
		  {
		    pss->predef_window = pdw_6X4;
		    pss->brx = SANE_FIX (6.0 * MM_PER_IN);
		    pss->bry = SANE_FIX (4.0 * MM_PER_IN);
		  }
		else if (strcmp (s, pdw_8X10) == 0)
		  {
		    pss->predef_window = pdw_8X10;
		    pss->brx = SANE_FIX (8.0 * MM_PER_IN);
		    pss->bry = SANE_FIX (10.0 * MM_PER_IN);
		  }
		else if (strcmp (s, pdw_85X11) == 0)
		  {
		    pss->predef_window = pdw_85X11;
		    pss->brx = SANE_FIX (8.5 * MM_PER_IN);
		    pss->bry = SANE_FIX (11.0 * MM_PER_IN);
		  }
		else
		  {
		    DBG (DL_MAJOR_ERROR,
			 "%s: trying to set predef window with "
			 "garbage value.", me);
		    pss->predef_window = pdw_none;
		    pss->brx = SANE_FIX (6.0 * MM_PER_IN);
		    pss->bry = SANE_FIX (4.0 * MM_PER_IN);
		  }
	      }
	    else
	      {
		pss->predef_window = pdw_none;
	      }
	  }
	  if (i)
	    *i = SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
	  break;
	case OPT_GAMMA_GS:
	  pss->gamma_gs = *(SANE_Fixed *) v;
	  if (i)
	    *i = 0;
	  break;
	case OPT_GAMMA_R:
	  pss->gamma_r = *(SANE_Fixed *) v;
	  if (i)
	    *i = 0;
	  break;
	case OPT_GAMMA_G:
	  pss->gamma_g = *(SANE_Fixed *) v;
	  if (i)
	    *i = 0;
	  break;
	case OPT_GAMMA_B:
	  pss->gamma_b = *(SANE_Fixed *) v;
	  if (i)
	    *i = 0;
	  break;
	case OPT_HALFTONE:
	  pss->halftone = *(SANE_Bool *) v;
	  if (pss->halftone)
	    {
	      switch (pss->mode)
		{
		case MD_BILEVELCOLOUR:
		  pss->options[OPT_GAMMA_R].cap &= ~SANE_CAP_INACTIVE;
		  pss->options[OPT_GAMMA_G].cap &= ~SANE_CAP_INACTIVE;
		  pss->options[OPT_GAMMA_B].cap &= ~SANE_CAP_INACTIVE;
		  pss->options[OPT_GAMMA_GS].cap |= SANE_CAP_INACTIVE;
		  break;
		case MD_LINEART:
		  pss->options[OPT_GAMMA_R].cap |= SANE_CAP_INACTIVE;
		  pss->options[OPT_GAMMA_G].cap |= SANE_CAP_INACTIVE;
		  pss->options[OPT_GAMMA_B].cap |= SANE_CAP_INACTIVE;
		  pss->options[OPT_GAMMA_GS].cap &= ~SANE_CAP_INACTIVE;
		  pss->options[OPT_THRESHOLD].cap |= SANE_CAP_INACTIVE;
		  break;
		default:
		  break;
		}
	      pss->options[OPT_HALFTONE_PATTERN].cap &= ~SANE_CAP_INACTIVE;
	    }
	  else
	    {
	      pss->options[OPT_GAMMA_R].cap |= SANE_CAP_INACTIVE;
	      pss->options[OPT_GAMMA_G].cap |= SANE_CAP_INACTIVE;
	      pss->options[OPT_GAMMA_B].cap |= SANE_CAP_INACTIVE;
	      pss->options[OPT_GAMMA_GS].cap |= SANE_CAP_INACTIVE;
	      pss->options[OPT_HALFTONE_PATTERN].cap |= SANE_CAP_INACTIVE;
	      if (pss->mode == MD_LINEART)
		pss->options[OPT_THRESHOLD].cap &= ~SANE_CAP_INACTIVE;
	    }
	  if (i)
	    *i = SANE_INFO_RELOAD_OPTIONS;
	  break;
	case OPT_HALFTONE_PATTERN:
	  {
	    char *s = (SANE_String) v;
	    if (strcmp (s, dm_dd8x8) == 0)
	      pss->dither_matrix = dm_dd8x8;
	    else if (strcmp (s, dm_dd16x16) == 0)
	      pss->dither_matrix = dm_dd16x16;
	    else
	      {
		DBG (DL_MAJOR_ERROR,
		     "%s: internal error: given illegal halftone pattern "
		     "string \"%s\"\n", me, s);
	      }
	  }
	  if (i)
	    *i = 0;
	  break;
	case OPT_NEGATIVE:
	  pss->negative = *(SANE_Bool *) v;
	  if (i)
	    *i = 0;
	  break;
	case OPT_THRESHOLD:
	  pss->threshold = *(SANE_Int *) v >> SANE_FIXED_SCALE_SHIFT;
	  if (i)
	    *i = 0;
	  break;
	case OPT_INQUIRY:
	  {
	    static char *hcfg_str[][2] =
	    {
	      {"8-bit AD converter", "10-bit AD converter"},
	      {"No auto-document feeder", "Auto-document feeder installed"},
	      {"No transparency option", "Transparency option installed"},
	      {"No ring buffer", "Ring buffer installed"},
	    {"No 16x16 halftone matrices", "16x16 halftone matrix support"},
	      {"No 8x8 halftone matrices", "8x8 halftone matrix support"}
	    };
	    SANE_Status status = inquiry (pss);
	    CHECK_STATUS (status, me, "inquiry");
	    DBG (DL_INFO, "\nInquiry results:\n"
		 "\tScanner:            %s\n"
		 "\thardware config:    0x%x\n"
		 "\t\t%s\n"
		 "\t\t%s\n"
		 "\t\t%s\n"
		 "\t\t%s\n"
		 "\t\t%s\n"
		 "\t\t%s\n"
		 "\toptical resolution: %lu\n"
		 "\tscan resolution:    %lu\n"
		 "\tnumber of lines:    %lu\n"
		 "\tbytes per line:     %lu\n"
		 "\tpixels per line:    %lu\n"
		 "\tms per line:        %f\n"
		 "\texposure time:      %c.%c ms\n"
		 "\tgreen offset:       %ld\n"
		 "\tblue offset:        %ld\n"
		 "\tred offset:         %ld\n"
		 "\tfirmware:           %s\n\n",
		 pss->buf + INQUIRY_VENDOR,
		 pss->hconfig,
		 hcfg_str[0][pss->hconfig & HCFG_ADC ? 1 : 0],
		 hcfg_str[1][pss->hconfig & HCFG_ADF ? 1 : 0],
		 hcfg_str[2][pss->hconfig & HCFG_TPO ? 1 : 0],
		 hcfg_str[3][pss->hconfig & HCFG_RB ? 1 : 0],
		 hcfg_str[4][pss->hconfig & HCFG_HT16 ? 1 : 0],
		 hcfg_str[5][pss->hconfig & HCFG_HT8 ? 1 : 0],
		 (u_long) pss->actual_res,
		 (u_long) pss->res,
		 (u_long) pss->lines,
		 (u_long) pss->bytes_per_line,
		 (u_long) pss->pixels_per_line,
		 (double) pss->ms_per_line,
		 pss->buf[INQUIRY_EXPTIME1] + '0',
		 pss->buf[INQUIRY_EXPTIME2] + '0',
		 (long) pss->rgb_buf.g_offset,
		 (long) pss->rgb_buf.b_offset,
		 (long) pss->rgb_buf.r_offset,
		 pss->buf + INQUIRY_FIRMWARE);
	  }
	  break;
	case OPT_SELF_TEST:
	  {
	    SANE_Status status = send_diagnostic (pss);
	    if (status == SANE_STATUS_GOOD)
	      DBG (DL_INFO, "Passes self-test.\n");
	    CHECK_STATUS (status, me, "self_test");
	    break;
	  }
	case OPT_REQ_SENSE:
	  {
	    SANE_Status status = request_sense (pss);
	    CHECK_STATUS (status, me, "request_sense");
	    if (pss->sense_str)
	      DBG (DL_INFO, "Scanner sense: %s\n", pss->sense_str);
	    if (pss->as_str)
	      DBG (DL_INFO, "Scanner ASC/ASCQ: %s\n", pss->as_str);
	    break;
	  }
	case OPT_REL_UNIT:
	  release_unit (pss);
	  DBG (DL_INFO, "Release unit sent.\n");
	  break;
	case OPT_RGB_LPR:
	  pss->rgb_lpr = *(SANE_Int *) v;
	  if (i)
	    *i = 0;
	  break;
	case OPT_GS_LPR:
	  pss->gs_lpr = *(SANE_Int *) v;
	  if (i)
	    *i = 0;
	  break;
	default:
	  DBG (DL_MAJOR_ERROR,
	       "%s: invalid option number %ld\n", me, (long) n);
	  return SANE_STATUS_UNSUPPORTED;
	  break;		/* not reached */
	}
      DBG (DL_VERBOSE, "%s: option %s set to value ",
	   me, pss->options[n].name);
      switch (pss->options[n].type)
	{
	case SANE_TYPE_INT:
	  DBG (DL_VERBOSE, "%ld\n", (long) (*(SANE_Int *) v));
	  break;
	case SANE_TYPE_BOOL:
	  {
	    char *valstr = *(SANE_Bool *) v == SANE_TRUE ? "TRUE" : "FALSE";
	    DBG (DL_VERBOSE, "%s\n", valstr);
	  }
	  break;
	default:
	  DBG (DL_VERBOSE, "other than an integer or boolean.\n");
	  break;
	}
      break;
    case SANE_ACTION_SET_AUTO:
      switch (n)
	{
	case OPT_COUNT:
	  break;
	case OPT_SCANRES:
	  pss->res = 300;
	  if (i)
	    *i = SANE_INFO_RELOAD_PARAMS;
	  break;
	case OPT_PREVIEW:
	  pss->preview = SANE_FALSE;
	  if (i)
	    *i = SANE_INFO_RELOAD_PARAMS;
	  break;
	case OPT_MODE:
	  pss->mode_s = md_colour;
	  pss->mode = MD_COLOUR;
	  pss->options[OPT_GAMMA_GS].cap |= SANE_CAP_INACTIVE;
	  pss->options[OPT_GAMMA_R].cap &= ~SANE_CAP_INACTIVE;
	  pss->options[OPT_GAMMA_G].cap &= ~SANE_CAP_INACTIVE;
	  pss->options[OPT_GAMMA_B].cap &= ~SANE_CAP_INACTIVE;
	  pss->options[OPT_HALFTONE].cap |= SANE_CAP_INACTIVE;
	  pss->options[OPT_HALFTONE_PATTERN].cap |= SANE_CAP_INACTIVE;
	  pss->options[OPT_NEGATIVE].cap |= SANE_CAP_INACTIVE;
	  pss->options[OPT_THRESHOLD].cap |= SANE_CAP_INACTIVE;
	  pss->options[OPT_GS_LPR].cap |= SANE_CAP_INACTIVE;
	  pss->options[OPT_RGB_LPR].cap &= ~SANE_CAP_INACTIVE;
	  if (i)
	    *i = SANE_INFO_RELOAD_OPTIONS;
	  break;
	case OPT_PREVIEW_MODE:
	  pss->preview_mode_s = md_greyscale;
	  pss->preview_mode = MD_GREYSCALE;
	  if (i)
	    *i = 0;
	  break;
	case OPT_TLX:
	  pss->tlx = DEFAULT_TLX;
	  if (i)
	    *i = SANE_INFO_RELOAD_PARAMS;
	  break;
	case OPT_TLY:
	  pss->tly = DEFAULT_TLY;
	  if (i)
	    *i = SANE_INFO_RELOAD_PARAMS;
	  break;
	case OPT_BRX:
	  pss->brx = DEFAULT_BRX;
	  if (i)
	    *i = SANE_INFO_RELOAD_PARAMS;
	  break;
	case OPT_BRY:
	  pss->bry = DEFAULT_BRY;
	  if (i)
	    *i = SANE_INFO_RELOAD_PARAMS;
	  break;
	case OPT_PREDEF_WINDOW:
	  pss->predef_window = pdw_none;
	  if (i)
	    *i = SANE_INFO_RELOAD_PARAMS;
	  break;
#ifdef INOPERATIVE
	case OPT_BRIGHTNESS:
	  pss->bright = AUTO_BRIGHTNESS;
	  if (i)
	    *i = SANE_INFO_RELOAD_PARAMS;
	  break;
	case OPT_CONTRAST:
	  pss->contrast = AUTO_CONTRAST;
	  if (i)
	    *i = SANE_INFO_RELOAD_PARAMS;
	  break;
#endif
	case OPT_GAMMA_GS:
	  pss->gamma_gs = DEFAULT_GAMMA;
	  if (i)
	    *i = 0;
	  break;
	case OPT_GAMMA_R:
	  pss->gamma_r = DEFAULT_GAMMA;
	  if (i)
	    *i = 0;
	  break;
	case OPT_GAMMA_G:
	  pss->gamma_g = DEFAULT_GAMMA;
	  if (i)
	    *i = 0;
	  break;
	case OPT_GAMMA_B:
	  pss->gamma_b = DEFAULT_GAMMA;
	  if (i)
	    *i = 0;
	  break;
	case OPT_HALFTONE:
	  pss->halftone = DEFAULT_HALFTONE;
	  if (pss->halftone)
	    {
	      pss->options[OPT_GAMMA_GS].cap &= ~SANE_CAP_INACTIVE;
	      pss->options[OPT_HALFTONE_PATTERN].cap &= ~SANE_CAP_INACTIVE;
	    }
	  else
	    {
	      pss->options[OPT_GAMMA_GS].cap |= SANE_CAP_INACTIVE;
	      pss->options[OPT_HALFTONE_PATTERN].cap |= SANE_CAP_INACTIVE;
	    }
	  if (i)
	    *i = SANE_INFO_RELOAD_OPTIONS;
	  break;
	case OPT_HALFTONE_PATTERN:
	  pss->dither_matrix = dm_dd8x8;
	  if (i)
	    *i = 0;
	  break;
	case OPT_NEGATIVE:
	  pss->halftone = DEFAULT_NEGATIVE;
	  if (i)
	    *i = 0;
	  break;
	case OPT_THRESHOLD:
	  pss->threshold = DEFAULT_THRESHOLD;
	  if (i)
	    *i = 0;
	  break;
	case OPT_RGB_LPR:
	  pss->rgb_lpr = def_rgb_lpr;
	  if (i)
	    *i = 0;
	  break;
	case OPT_GS_LPR:
	  pss->gs_lpr = def_gs_lpr;
	  if (i)
	    *i = 0;
	  break;
	default:
	  DBG (DL_MAJOR_ERROR, "%s: invalid option number %ld\n",
	       me, (long) n);
	  return SANE_STATUS_UNSUPPORTED;
	  break;		/* not reached */
	}
      break;
    default:
      DBG (DL_MAJOR_ERROR, "%s: invalid action code %ld\n", me, (long) a);
      return SANE_STATUS_UNSUPPORTED;
      break;			/* not reached */
    }

  if (a != SANE_ACTION_GET_VALUE)
    {
      SANE_Status status = set_window (pss);
      CHECK_STATUS (status, me, "set_window");
    }

  close_scanner (pss);
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_get_parameters (SANE_Handle h,
			      SANE_Parameters * p)
{
  static const char *me = "sane_snapscan_get_parameters";
  SnapScan_Scanner *pss = (SnapScan_Scanner *) h;
  SANE_Status status;

  DBG (DL_CALL_TRACE, "%s (%p, %p)\n", me, (void *) h, (void *) p);

  status = open_scanner (pss);
  CHECK_STATUS (status, me, "open_scanner");

  /* fetch the latest parameters */
  status = inquiry (pss);
  CHECK_STATUS (status, me, "inquiry");

  close_scanner (pss);

  p->last_frame = SANE_TRUE;	/* we always do only one frame */
  p->pixels_per_line = pss->pixels_per_line;
  p->bytes_per_line = pss->bytes_per_line;
  p->lines = pss->lines;

  /* mode-specific modifications */
  {
    SnapScan_Mode mode = pss->mode;
    size_t line_offset = rgb_buf_get_size (pss);

    if (pss->preview)
      mode = pss->preview_mode;

    /* The SANE API supports bit-depths of 1 and integer multiples of
       8 only.  10 bit/channel is _not_ supported.  */
#if 0
    /* the frontend only supports depths 1, 8, and depth 1 only
       for black and white */
    p->depth = pss->pdev->depths[mode];

    if (pss->preview && p->depth > 8)
      p->depth = 8;
#else
    p->depth = 8;
#endif

    switch (mode)
      {
      case MD_COLOUR:
	p->format = SANE_FRAME_RGB;
	switch (pss->pdev->model)
	  {
	  case SNAPSCAN310:
	  case SNAPSCAN600:
	  case SNAPSCAN1236S:
	  case VUEGO310S:	/* WG changed */
	    if (!pss->preview)
	      {
		pss->lines += line_offset;
		p->lines -= line_offset;
	      }
	    break;
	  default:
	    break;
	  }
	break;
      case MD_BILEVELCOLOUR:
	p->format = SANE_FRAME_RGB;
	/* the scanner uses depth 1, but the frontend will use 8 */
	p->bytes_per_line = 3 * p->pixels_per_line;
	p->depth = 8;
	break;
      case MD_LINEART:		/* WG changed - added the case-switch */
	{
	  p->depth = 1;
	  p->bytes_per_line = (3 * p->pixels_per_line) / 8;
	}
	switch (pss->pdev->model)
	  {
	  case SNAPSCAN310:
	  case SNAPSCAN600:
	  case SNAPSCAN1236S:
	  case VUEGO310S:	/* WG changed */
	    pss->lines += line_offset;
	    p->lines -= line_offset;
	    break;
	  default:
	    break;
	  }
	break;
      default:
	p->format = SANE_FRAME_GRAY;
	break;
      }
  }

  DBG (DL_DATA_TRACE, "%s: depth = %ld\n", me, (long) p->depth);
  DBG (DL_DATA_TRACE, "%s: lines = %ld\n", me, (long) p->lines);
  DBG (DL_DATA_TRACE, "%s: pixels per line = %ld\n", me,
       (long) p->pixels_per_line);
  DBG (DL_DATA_TRACE, "%s: bytes per line = %ld\n", me,
       (long) p->bytes_per_line);

  return status;
}


/* scan data reader routine for child process */

static SnapScan_Scanner *reader_pss;

static void
handler (int signo)
{
  signal (signo, handler);
  DBG (DL_MINOR_INFO, "child process: received signal %ld\n", (long) signo);
  reader_pss->state = ST_CANCEL_INIT;
}

static void
reader (SnapScan_Scanner * pss)
{
  static char me[] = "Child reader process";
  SANE_Status status;
  int max_lines_per_read, max_bytes_per_read;

  /* cap on time per read */
  SnapScan_Mode mode = pss->mode;

  DBG (DL_CALL_TRACE, "%s\n", me);

  if (pss->preview)
    mode = pss->preview_mode;

  if (mode == MD_COLOUR || mode == MD_BILEVELCOLOUR)
    max_lines_per_read = pss->rgb_lpr;
  else
    max_lines_per_read = pss->gs_lpr;

/* WG  ändern : Bei Lineart geteilt durch 8 ? */

  max_bytes_per_read = pss->bytes_per_line * max_lines_per_read;

  while (pss->expected_data_len)
    {
      pss->expected_read_bytes =
	MIN (MIN (pss->buf_sz, pss->expected_data_len), max_bytes_per_read);
      while (1)
	{
	  status = scsi_read (pss, READ_IMAGE);
	  if (status == SANE_STATUS_GOOD)
	    break;
	  else
	    {
	      DBG (DL_MAJOR_ERROR, "%s: %s on read.\n", me,
		   sane_strstatus (status));
	      _exit (1);
	    }
	}

      if (pss->state == ST_CANCEL_INIT)
	{
	  return;		/* aborted before or between writes */
	}
      else
	{
	  int to_write = pss->read_bytes;
	  unsigned char *buf = pss->buf;
	  DBG (1, "READ_BYTES %lu\n", (u_long) pss->read_bytes);
	  while (to_write)
	    {
	      int written = write (STDOUT_FILENO, buf, to_write);
	      DBG (1, "WRITTEN %lu\n", (u_long) written);
	      if (pss->state == ST_CANCEL_INIT)
		{
		  return;	/* aborted during write */
		}
	      if (written == -1)
		{
		  DBG (DL_MAJOR_ERROR,
		       "%s: error writing scan data on parent pipe.\n", me);
		  perror ("pipe error: ");
		}
	      else
		{
		  to_write -= written;
		  buf += written;
		}
	    }
	}
      pss->expected_data_len -= MIN (pss->read_bytes, pss->expected_data_len);
    }
}

static void
start_reader (SnapScan_Scanner * pss)
{
  static char me[] = "start_reader";

  DBG (DL_CALL_TRACE, "%s\n", me);

  /* We can implement nonblocking mode by forking a separate reader
     process. It doesn't seem that we can poll the scsi descriptor. */

  pss->nonblocking = SANE_FALSE;
  pss->rpipe[0] = pss->rpipe[1] = -1;
  pss->child = -1;
  /*return;*/

  if (pipe (pss->rpipe) != -1)
    {
      pss->orig_rpipe_flags = fcntl (pss->rpipe[0], F_GETFL, 0);
      switch (pss->child = fork ())
	{
	case -1:
	  /* we'll have to read in blocking mode */
	  DBG (DL_MAJOR_ERROR, "%s: can't fork; must read in blocking mode.\n",
	       me);
	  close (pss->rpipe[0]);
	  close (pss->rpipe[1]);
	  break;
	case 0:
	  /* child; close read side, make stdin the scsi file descriptor
         and stdout the pipe */
	  signal (SIGTERM, handler);
	  reader_pss = pss;
	  dup2 (pss->rpipe[1], STDOUT_FILENO);
	  close (pss->rpipe[0]);
	  reader (pss);
	  /* regular exit will cause a SIGPIPE */
	  DBG (DL_MINOR_INFO, "Reader process terminating.\n");
	  _exit (0);
	  break;		/* not reached */
	default:
	  /* parent; close write side */
	  close (pss->rpipe[1]);
	  pss->nonblocking = SANE_TRUE;
	  break;
	}
    }
}


/* transfer_data_8: transfer snapscan raw multilevel RGB data, depth
   8, to a frontend buffer.

   The snapscan arranges the bytes in a scan line so that the R, G and
   B bands are contiguous and follow one another; the frontend expects
   the bands byte-interleaved for format SANE_FRAME_RGB. */

static void
transfer_data_8 (u_char * dest, u_char * src,
		 size_t n_lines, size_t bytes_per_line)
{
  static char me[] = "transfer_data_8";
  size_t bytes_per_band = bytes_per_line / 3;
  size_t R = 0;
  size_t G = bytes_per_band;
  size_t B = 2 * bytes_per_band;

  int i, j;

  DBG (DL_CALL_TRACE, "%s\n", me);

  for (i = 0; i < n_lines; i++)
    {
      for (j = 0; j < bytes_per_band; j++)
	{
	  dest[0] = src[R];
	  dest[1] = src[G];
	  dest[2] = src[B];
	  dest += 3;
	  src++;
	}
      src += B;
    }
  DBG (DL_VERBOSE, "%s: transferred %lu lines (%lu bytes)\n",
       me, (u_long) n_lines, (u_long) n_lines * bytes_per_line);
}


#define BIT_TX(n,b) (((n >> b) & 0x01) ? 0xff : 0x00)

static size_t
transfer_data_1 (u_char * dest, u_char * src,
		 size_t n_lines, size_t bytes_per_line,
		 size_t pixels_per_line)
{
  static char me[] = "transfer_data_1";
  size_t bytes_per_band = bytes_per_line / 3;
  size_t R = 0;
  size_t G = bytes_per_band;
  size_t B = 2 * bytes_per_band;

  int i, j;

  int lastbit = pixels_per_line % 8;

  DBG (DL_CALL_TRACE, "%s\n", me);

  if (!lastbit)
    lastbit = 7;

  if (bytes_per_line % 3)
    DBG (DL_MINOR_ERROR, "%s: bytes_per_line not a factor of 3!!!\n", me);

  for (i = 0; i < n_lines; i++)
    {
      int bit;
      for (j = 0; j < bytes_per_band - 1; j++)
	{
	  for (bit = 7; bit >= 0; bit--)
	    {
	      dest[0] = BIT_TX (src[R], bit);
	      dest[1] = BIT_TX (src[G], bit);
	      dest[2] = BIT_TX (src[B], bit);
	      dest += 3;
	    }
	  src++;
	}
      /* handle last byte specially */
      {
	for (bit = 7; bit >= 7 - lastbit; bit--)
	  {
	    dest[0] = BIT_TX (src[R], bit);
	    dest[1] = BIT_TX (src[G], bit);
	    dest[2] = BIT_TX (src[B], bit);
	    dest += 3;
	  }
	src++;
      }
      src += B;
    }
  DBG (DL_VERBOSE,
       "%s: transferred %lu lines (%lu bytes src, %lu bytes dest)\n",
       me, (u_long) n_lines, (u_long) n_lines * bytes_per_line,
       (u_long) n_lines * pixels_per_line * 3);
  return (n_lines * 3 * pixels_per_line);
}

SANE_Status
sane_start (SANE_Handle h)
{
  static const char *me = "sane_snapscan_start";
  SANE_Status status;
  SnapScan_Scanner *pss = (SnapScan_Scanner *) h;
  SnapScan_Mode mode = pss->mode;
  SANE_Bool colour = SANE_FALSE;

  DBG (DL_CALL_TRACE, "%s (%p)\n", me, (void *) h);

  rgb_buf_clean (pss);		/* really required ? */

  /* possible authorization required */

  status = open_scanner (pss);
  CHECK_STATUS (status, me, "open_scanner");

  status = wait_scanner_ready (pss);
  CHECK_STATUS (status, me, "wait_scanner_ready");

  if (pss->preview)
    mode = pss->preview_mode;	/* actual mode */

  /* download the gamma and halftone tables */

  {
    float gamma_gs = SANE_UNFIX (pss->gamma_gs);
    float gamma_r = SANE_UNFIX (pss->gamma_r);
    float gamma_g = SANE_UNFIX (pss->gamma_g);
    float gamma_b = SANE_UNFIX (pss->gamma_r);

    switch (mode)
      {
      case MD_COLOUR:
	colour = SANE_TRUE;
	break;
      case MD_BILEVELCOLOUR:
	colour = SANE_TRUE;
	if (!pss->halftone)
	  gamma_r = gamma_g = gamma_b = 1.0;
	break;
      case MD_LINEART:
	if (!pss->halftone)
	  gamma_gs = 1.0;
	break;
      default:
	/* no further action for greyscale */
	break;
      }


    if (colour)
      {
	gamma_8 (gamma_r, pss->buf + SEND_LENGTH);
	status = send (pss, DTC_GAMMA, DTCQ_GAMMA_RED8);
	CHECK_STATUS (status, me, "send");

	gamma_8 (gamma_g, pss->buf + SEND_LENGTH);
	status = send (pss, DTC_GAMMA, DTCQ_GAMMA_GREEN8);
	CHECK_STATUS (status, me, "send");

	gamma_8 (gamma_b, pss->buf + SEND_LENGTH);
	status = send (pss, DTC_GAMMA, DTCQ_GAMMA_BLUE8);
	CHECK_STATUS (status, me, "send");
      }
    else
      {
	gamma_8 (gamma_gs, pss->buf + SEND_LENGTH);
	status = send (pss, DTC_GAMMA, DTCQ_GAMMA_GRAY8);
	CHECK_STATUS (status, me, "send");
      }

    if (pss->halftone)
      {
	u_char *matrix;
	size_t matrix_sz;
	u_char dtcq;

	if (pss->dither_matrix == dm_dd8x8)
	  {
	    matrix = D8;
	    matrix_sz = sizeof (D8);
	  }
	else
	  {
	    matrix = D16;
	    matrix_sz = sizeof (D16);
	  }

	memcpy (pss->buf + SEND_LENGTH, matrix, matrix_sz);

	if (colour)
	  {
	    if (matrix_sz == sizeof (D8))
	      dtcq = DTCQ_HALFTONE_COLOR8;
	    else
	      dtcq = DTCQ_HALFTONE_COLOR16;

	    /* need copies for green and blue bands */
	    memcpy (pss->buf + SEND_LENGTH + matrix_sz,
		    matrix, matrix_sz);
	    memcpy (pss->buf + SEND_LENGTH + 2 * matrix_sz,
		    matrix, matrix_sz);
	  }
	else
	  {
	    if (matrix_sz == sizeof (D8))
	      dtcq = DTCQ_HALFTONE_BW8;
	    else
	      dtcq = DTCQ_HALFTONE_BW16;
	  }

	status = send (pss, DTC_HALFTONE, dtcq);
	CHECK_STATUS (status, me, "send");
      }
  }

  /* set up the window and fetch the resulting scanner parameters */
  status = set_window (pss);
  CHECK_STATUS (status, me, "set_window");

  status = inquiry (pss);
  CHECK_STATUS (status, me, "inquiry");

  /* we must measure the data transfer rate between the host and the
     scanner, and the method varies depending on whether there is a
     ring buffer or not. */

  if (pss->hconfig & 0x10)
    {
      /* We have a ring buffer. We simulate one round of a read-store
       cycle on the size of buffer we will be using. For this read only,
       the buffer size must be rounded to a 128-byte boundary. */

      DBG (DL_VERBOSE, "%s: have ring buffer\n", me);
      pss->expected_read_bytes =
	pss->buf_sz % 128 ? (pss->buf_sz / 128 + 1) * 128 : pss->buf_sz;

      {
	u_char *other_buf = (u_char *) malloc (pss->expected_read_bytes);

	if (!other_buf)
	  {
	    DBG (DL_MAJOR_ERROR,
		 "%s: failed to allocate second test buffer.\n", me);
	    return SANE_STATUS_NO_MEM;
	  }

	if (mode == MD_COLOUR || mode == MD_BILEVELCOLOUR)
	  {
	    switch (pss->pdev->model)
	      {
	      case SNAPSCAN310:
	      case SNAPSCAN600:
	      case SNAPSCAN1236S:
	      case VUEGO310S:	/* WG changed */
		if (SANE_STATUS_GOOD != rgb_buf_init (pss))
		  return SANE_STATUS_NO_MEM;
		break;
	      default:
		break;
	      }
	  }

	status = scsi_read (pss, READ_TRANSTIME);
	if (status != SANE_STATUS_GOOD)
	  {
	    DBG (DL_MAJOR_ERROR, "%s: test read failed.\n", me);
	    free (other_buf);
	    if (mode == MD_COLOUR || mode == MD_BILEVELCOLOUR)
	      {
		switch (pss->pdev->model)
		  {
		  case SNAPSCAN310:
		  case SNAPSCAN600:
	          case SNAPSCAN1236S:
		  case VUEGO310S:	/* WG changed */
		    rgb_buf_clean (pss);
		    break;
		  default:
		    break;
		  }
	      }
	    return status;
	  }

	/* presumably, after copying, the front-end will be storing the
         data in a file */

	if (mode == MD_COLOUR || mode == MD_BILEVELCOLOUR)
	  {
	    switch (pss->pdev->model)
	      {
	      case SNAPSCAN310:
	      case SNAPSCAN600:
	      case SNAPSCAN1236S:
	      case VUEGO310S:	/* WG changed */
		transfer_data_diff (other_buf, pss);
		break;

	      default:
		transfer_data_8 (other_buf, pss->buf,
				 pss->read_bytes / pss->bytes_per_line,
				 pss->bytes_per_line);
		break;
	      }
	  }
	else
	  {
	    memcpy (other_buf, pss->buf, pss->read_bytes);
	  }

	{
	  int result = write (pss->tfd, other_buf, pss->read_bytes);
	  if (result < 0 || result == INT_MAX)
	    {
	      DBG (DL_MAJOR_ERROR, "%s: write of test data to file failed.\n",
		   me);
	      perror ("");
	      return SANE_STATUS_UNSUPPORTED;
	    }
	}
	free (other_buf);
	if (mode == MD_COLOUR || mode == MD_BILEVELCOLOUR)
	  {
	    switch (pss->pdev->model)
	      {
	      case SNAPSCAN310:
	      case SNAPSCAN600:
	      case SNAPSCAN1236S:
	      case VUEGO310S:	/* WG changed */
		rgb_buf_clean (pss);
		break;
	      default:
		break;
	      }
	  }
      }

      pss->expected_read_bytes = 0;
      status = scsi_read (pss, READ_TRANSTIME);
      CHECK_STATUS (status, me, "scsi_read");
    }
  else
    {
      /* we don't have a ring buffer. The test requires transferring one
       scan line of data (rounded up to next 128 byte boundary). */

      DBG (DL_VERBOSE, "%s: we don't have a ring buffer.\n", me);
      pss->expected_read_bytes = pss->bytes_per_line;


      if (pss->expected_read_bytes % 128)
	{
	  pss->expected_read_bytes =
	    (pss->expected_read_bytes / 128 + 1) * 128;
	}
      status = scsi_read (pss, READ_TRANSTIME);
      CHECK_STATUS (status, me, "scsi_read");
      DBG (DL_VERBOSE, "%s: read %ld bytes.\n", me, (long) pss->read_bytes);
    }

  DBG (DL_VERBOSE, "%s: successfully calibrated transfer rate.\n", me);

  /* now perform an inquiry again to get the scan speed */
  status = inquiry (pss);
  CHECK_STATUS (status, me, "inquiry");

  DBG (DL_DATA_TRACE,
       "%s: after measuring speed:\n\t%lu bytes per scan line\n"
       "\t%f milliseconds per scan line.\n\t==>%f bytes per millisecond\n",
       me, (u_long) pss->bytes_per_line, pss->ms_per_line,
       pss->bytes_per_line / pss->ms_per_line);

  /* allocate and initialize rgb ring buffer if the device is
     a snapscan 310, 600 or 1236s model, in colour mode               */
  if (colour)
    {
      switch (pss->pdev->model)
	{
	case SNAPSCAN310:
	case SNAPSCAN600:
	case SNAPSCAN1236S:
	case VUEGO310S:	/* WG changed */
	  rgb_buf_init (pss);
	  break;

	default:
	  break;
	}
    }

  /* start scanning; reserve the unit first, because a release_unit is
     necessary to abort a scan in progress */

  pss->state = ST_SCAN_INIT;

  reserve_unit (pss);
  status = scan (pss);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DL_MAJOR_ERROR, "%s: scan command failed.\n", me);
      release_unit (pss);
      return status;
    }
  DBG (DL_MINOR_INFO, "%s: starting the reader process.\n", me);
  start_reader (pss);

  return SANE_STATUS_GOOD;
}


SANE_Status
sane_read (SANE_Handle h, SANE_Byte * buf,
		    SANE_Int maxlen, SANE_Int * plen)
{
  static const char *me = "sane_snapscan_read";
  SnapScan_Scanner *pss = (SnapScan_Scanner *) h;
  SANE_Status status;
  SnapScan_Mode mode = pss->mode;

  DBG (DL_CALL_TRACE, "%s (%p, %p, %ld, %p)\n",
       me, (void *) h, (void *) buf, (long) maxlen, (void *) plen);

  *plen = 0;

  if (!pss->expected_data_len)
    {
      if (pss->child > 0)
	{
	  int status;
	  wait (&status);	/* ensure no zombies */
	}
      pss->state = ST_IDLE;
      release_unit (pss);
      close_scanner (pss);
      return SANE_STATUS_EOF;
    }

  if (pss->preview)
    mode = pss->preview_mode;

  if (mode == MD_BILEVELCOLOUR)
    /* max data read must be 8 times less, so adjust */
    maxlen /= 8;

  /* reset maxlen to a scan line boundary */
  /* XXX Why is this here? The non-blocking client should have no
     buffer limits */
  maxlen = (maxlen / pss->bytes_per_line) * pss->bytes_per_line;

  /* expected data per read is the minimum of the scanner effective
     buffer length , the frontend effective buffer length, and the
     total remaining data in the scan */
  pss->expected_read_bytes =
    MIN (MIN (pss->buf_sz, maxlen), pss->expected_data_len);

  /* since a cancellation happens asynchronously, it seems in practice
     that we need to check for cancellation both before and after IO
     operations or we could get stuck */

  if (pss->child == -1)
    {
      if (pss->state != ST_CANCEL_INIT)
	{
	  status = scsi_read (pss, READ_IMAGE);
	  if (status != SANE_STATUS_GOOD && pss->state != ST_CANCEL_INIT)
	    {
	      DBG (DL_MAJOR_ERROR, "%s: read failed.\n", me);
	      return status;
	    }
	}
    }
  else
    {
      /* reading off the pipe we usually get a small amount of data */
      unsigned char *buf = pss->buf;
      int expected_read_bytes = pss->expected_read_bytes;
      pss->read_bytes = 0;
      while (pss->state != ST_CANCEL_INIT && expected_read_bytes)
	{
	  /* note pss->read_bytes may be unsigned */
	  int read_bytes = read (pss->rpipe[0], buf, expected_read_bytes);
	  if (!read_bytes || pss->state == ST_CANCEL_INIT)
	    break;
	  if (read_bytes == -1)
	    {
	      if (errno == EAGAIN)
		{
		  break;
		}
	      else
		{
		  DBG (DL_MAJOR_ERROR, "%s: read from pipe failed.\n", me);
		  perror ("File error");
		  return SANE_STATUS_IO_ERROR;
		}
	    }
	  else
	    {
	      pss->read_bytes += read_bytes;
	      expected_read_bytes -= read_bytes;
	      if (expected_read_bytes)
		buf += read_bytes;
	    }
	}

    }

  switch (pss->state)
    {
    case ST_IDLE:
      DBG (DL_MAJOR_ERROR,
	   "%s: weird error: scanner state should not be idle on call to "
	   "sane_read.\n", me);
      break;
    case ST_SCAN_INIT:
      /* we've read some data */
      pss->state = ST_SCANNING;
      break;
    case ST_CANCEL_INIT:
      /* stop scanning */
      if (pss->child > 0)
	{
	  int result;
	  if ((result = kill (pss->child, SIGTERM)) < 0)
	    {
	      DBG (DL_VERBOSE, "%s: error: kill returns %ld\n",
		   me, (long) result);
	    }
	  else
	    {
	      int status;
	      DBG (DL_VERBOSE, "%s: waiting on child reader.\n", me);
	      wait (&status);
	      DBG (DL_VERBOSE, "%s: child has terminated.\n", me);
	    }
	}
      release_unit (pss);
      close_scanner (pss);
      pss->state = ST_IDLE;
      return SANE_STATUS_CANCELLED;
      break;
    default:
      break;
    }

  {
    size_t transferred_bytes = pss->read_bytes;

    if (pss->read_bytes > 0)
      {
	DBG (1, "SNAP_TRANSFERRED %lu\n", (u_long) pss->read_bytes);
	switch (mode)
	  {
	  case MD_COLOUR:
	    switch (pss->pdev->model)
	      {
	      case SNAPSCAN310:
	      case SNAPSCAN600:
	      case SNAPSCAN1236S:
	      case VUEGO310S:	/* WG changed */
		transferred_bytes = transfer_data_diff (buf, pss);
		break;

	      default:
		transfer_data_8 (buf, pss->buf, pss->read_bytes / pss->bytes_per_line,
				 pss->bytes_per_line);
		break;
	      }			/* switch model */
	    break;

	  case MD_BILEVELCOLOUR:
	    transferred_bytes =
	      transfer_data_1 (buf, pss->buf,
			       pss->read_bytes / pss->bytes_per_line,
			       pss->bytes_per_line, pss->pixels_per_line);
	    break;

	  case MD_GREYSCALE:
	    memcpy (buf, pss->buf, pss->read_bytes);
	    break;

	  default:
	    memcpy (buf, pss->buf, pss->read_bytes);
	    if (!pss->negative)
	      /* the snapscan creates a negative image by default... so for the
             user interface to make sense, the internal meaning of "negative"
             is reversed */
	      {
		unsigned i;
		for (i = 0; i < pss->read_bytes; i++)
		  buf[i] ^= 0xff;
	      }
	    break;
	  }			/* switch mode */

      }				/* if (read_bytes > 0) */

    *plen = transferred_bytes;
  }

  pss->expected_read_bytes -= MIN (pss->expected_read_bytes, pss->read_bytes);
  pss->expected_data_len -= MIN (pss->expected_data_len, pss->read_bytes);

  /* we must close the pipe when it's exhausted because it is the select
     file descriptor supplied by sane_snapscan_select_fd */
  if (!pss->expected_data_len)
    close (pss->rpipe[0]);

  return SANE_STATUS_GOOD;
}

void
sane_cancel (SANE_Handle h)
{
  char *me = "sane_snapscan_cancel";
  SnapScan_Scanner *pss = (SnapScan_Scanner *) h;

  DBG (DL_CALL_TRACE, "%s\n", me);
  switch (pss->state)
    {
    case ST_IDLE:
      break;
    case ST_SCAN_INIT:
    case ST_SCANNING:
      /* initiate the cancellation */
      pss->state = ST_CANCEL_INIT;
      if (pss->mode == MD_COLOUR)
	{
	  switch (pss->pdev->model)
	    {
	    case SNAPSCAN310:
	    case SNAPSCAN600:
	    case SNAPSCAN1236S:
	    case VUEGO310S:	/* WG changed */
	      rgb_buf_clean (pss);
	      break;

	    default:
	      break;
	    }
	}
      break;
    case ST_CANCEL_INIT:
      DBG (DL_INFO, "%s: cancellation already initiated.\n", me);
      break;
    default:
      DBG (DL_MAJOR_ERROR, "%s: weird error: invalid scanner state (%ld).\n",
	   me, (long) pss->state);
      break;
    }
}

SANE_Status
sane_set_io_mode (SANE_Handle h, SANE_Bool m)
{
  static char me[] = "sane_snapscan_set_io_mode";
  SnapScan_Scanner *pss = (SnapScan_Scanner *) h;
  char *op;

  DBG (DL_CALL_TRACE, "%s\n", me);

  if (pss->state != ST_SCAN_INIT)
    {
      return SANE_STATUS_INVAL;
    }

  if (m)
    {
      if (pss->child == -1)
	{
	  DBG (DL_MINOR_INFO,
	       "%s: no reader child; must use blocking mode.\n", me);
	  return SANE_STATUS_UNSUPPORTED;
	}
      op = "ON";
      fcntl (pss->rpipe[0], F_SETFL, O_NONBLOCK | pss->orig_rpipe_flags);
    }
  else
    {
      op = "OFF";
      fcntl (pss->rpipe[0], F_SETFL, pss->orig_rpipe_flags);
    }
  DBG (DL_MINOR_INFO, "%s: turning nonblocking mode %s.\n", me, op);
  pss->nonblocking = m;
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_get_select_fd (SANE_Handle h, SANE_Int * fd)
{
  static char me[] = "sane_snapscan_get_select_fd";
  SnapScan_Scanner *pss = (SnapScan_Scanner *) h;

  DBG (DL_CALL_TRACE, "%s\n", me);

  if (pss->state != ST_SCAN_INIT)
    {
      return SANE_STATUS_INVAL;
    }

  if (pss->child == -1)
    {
      DBG (DL_MINOR_INFO,
	   "%s: no reader child; cannot provide select file descriptor.\n",
	   me);
      return SANE_STATUS_UNSUPPORTED;
    }
  *fd = pss->rpipe[0];
  return SANE_STATUS_GOOD;
}


/* $Log$
 * Revision 1.1  1999/08/09 18:05:49  pere
 * Initial revision
 *
 * Revision 1.40  1998/12/16  18:43:06  charter
 * Fixed major version problem precipitated by release of SANE-1.00.
 *
 * Revision 1.39  1998/09/07  06:09:26  charter
 * Formatting (whitespace) changes.
 *
 * Revision 1.38  1998/09/07  06:06:01  charter
 * Merged in Wolfgang Goeller's changes (Vuego 310S, bugfixes).
 *
 * Revision 1.37  1998/08/06  06:16:39  charter
 * Now using sane_config_attach_matching_devices() in sane_snapscan_init().
 * Change contributed by David Mosberger-Tang.
 *
 * Revision 1.36  1998/05/11  17:02:53  charter
 * Added Mikko's threshold stuff.
 *
 * Revision 1.35  1998/03/10 23:43:23  eblot
 * Bug correction
 *
 * Revision 0.72  1998/03/10 23:40:42  eblot
 * More support for 310/600 models: color preview, large window
 *
 * Revision 1.35  1998/03/10  21:32:07  eblot
 * Debugging
 *
 * Revision 1.34  1998/02/15  21:55:53  charter
 * From Emmanuel Blot:
 * First routines to support SnapScan 310 scanned data.
 *
 * Revision 1.33  1998/02/06  02:30:28  charter
 * Now using a mode enum (instead of the static string pointers directly).
 * Now check for the SnapScan 310 and 600 explicitly (start of support
 * for these models).
 *
 * Revision 1.32  1998/02/01  21:56:48  charter
 * Patches to fix compilation problems on Solaris supplied by
 * Jim McBeath.
 *
 * Revision 1.31  1998/02/01  03:36:40  charter
 * Now check for BRX < TLX and BRY < TLY and whether the area of the
 * scanning window is approaching zero in set_window. I'm setting a
 * minimum window size of 75x75 hardware pixels (0.25 inches a side).
 * If the area falls to zero, the scanner seems to hang in the middle
 * of the set_window command.
 *
 * Revision 1.30  1998/02/01  00:00:33  charter
 * TLX, TLY, BRX and BRY are now lengths expressed in mm. The frontends
 * can now allow changes in the units, and units that are more user-
 * friendly.
 *
 * Revision 1.29  1998/01/31  21:09:19  charter
 * Fixed another problem with add_device(): if mini_inquiry ends
 * up indirectly invoking the sense handler, there'll be a segfault
 * because the sense_handler isn't set. Had to fix sense_handler so
 * it can handle a NULL pss pointer and then use the sanei_scsi stuff
 * everywhere. This error is most likely to occur if the scanner is
 * turned off.
 *
 * Revision 1.28  1998/01/31  18:45:22  charter
 * Last fix botched, produced a compile error. Thought I'd already
 * compiled successfully.
 *
 * Revision 1.27  1998/01/31  18:32:42  charter
 * Fixed stupid bug in add_device that causes segfault when no snapscan
 * found: closing a scsi fd opened with open() using sanei_scsi_close().
 *
 * Revision 1.26  1998/01/30  21:19:02  charter
 * sane_snapscan_init() handles failure of add_device() in the same
 * way when there is no snapscan.conf file available as when there is
 * one.
 *
 * Revision 1.25  1998/01/30  19:41:11  charter
 * Waiting for child process termination at regular end of scan (not
 * just on cancellation); before I was getting zombies.
 *
 * Revision 1.24  1998/01/30  19:19:27  charter
 * Changed from strncmp() to strncasecmp() to do vendor and model
 * comparisons in sane_snapscan_init. There are some snapcsan models
 * that use lower case.
 * Now have debug level defines instead of raw numbers, and better debug
 * information categories.
 * Don't complain at debug level 0 when a snapscan isn't found on a
 * requested device.
 * Changed CHECK_STATUS to take caller parameter instead of always
 * assuming an available string "me".
 *
 * Revision 1.23  1998/01/30  11:03:04  charter
 * Fixed * vs [] operator precedence screwup in sane_snapscan_get_devices()
 * that caused a segfault in scanimage -h.
 * Fixed problem with not closing the scsi fd between certain commands
 * that caused scanimage to hang; now using open_scanner() and close_scanner().
 *
 * Revision 1.22  1998/01/28  09:02:55  charter
 * Fixed bug: zero allocation length in request sense command buffer
 * was preventing sense information from being received. The
 * backend now correctly waits for the scanner to warm up.
 * Now using the hardware configuration byte to check whether
 * both 8x8 and 16x16 halftoning should be made available.
 *
 * Revision 1.21  1998/01/25  09:57:57  charter
 * Added more SCSI command buttons (and a group for them).
 * Made the output of the Inquiry command a bit nicer.
 *
 * Revision 1.20  1998/01/25  08:53:14  charter
 * Have added bi-level colour mode, with halftones too.
 * Can now select preview mode (but it's an advanced option, since
 * you usually don't want to do it).
 *
 * Revision 1.19  1998/01/25  02:25:02  charter
 * Fixed bug: preview mode gives blank image at initial startup.
 * Fixed bug: lineart mode goes weird after a preview or gs image.
 * More changes to option relationships;
 * now using test_unit_ready and send_diagnostic in sane_snapscan_open().
 * Added negative option.
 *
 * Revision 1.18  1998/01/24  05:15:32  charter
 * Now have RGB gamma correction and dispersed-dot dither halftoning
 * for BW images. Cleaned up some spots in the code and have set up
 * option interactions a bit better (e.g. halftoning and GS gamma
 * correction made inactive in colour mode, etc). TL_[XY] and BR_[XY]
 * now change in ten-pixel increments (I had problems with screwed-up
 * scan lines when the dimensions were weird at low res... could be the
 * problem).
 *
 * Revision 1.17  1998/01/23  13:03:17  charter
 * Several changes, all aimed at getting scanning performance working
 * correctly, and the progress/cancel window functioning. Cleaned up
 * a few nasty things as well.
 *
 * Revision 1.16  1998/01/23  07:40:23  charter
 * Reindented using GNU convention at David Mosberger-Tang's request.
 * Also applied David's patch fixing problems on 64-bit architectures.
 * Now using scanner's reported speed to guage amount of data to request
 * in a read on the scsi fd---nonblocking mode operates better now.
 * Fixed stupid bug I introduced in preview mode data transfer.
 *
 * Revision 1.15  1998/01/22  06:18:57  charter
 * Raised the priority of a couple of DBG messages in reserve_unit()
 * and release_unit(), and got rid of some unecessary ones.
 *
 * Revision 1.14  1998/01/22  05:15:35  charter
 * Have replaced the bit depth option with a mode option; various
 * changes associated with that.
 * Also, I again close the STDERR_FILENO in the reader child and
 * dup the STDOUT file descriptor onto it. This prevents an "X io"
 * error when the child exits, while still allowing the use of
 * DBG.
 *
 * Revision 1.13  1998/01/21  20:41:22  charter
 * Added copyright info.
 * Also now seem to have cancellation working. This requires using a
 * new scanner state variable and checking in all the right places
 * in the reader child and the sane_snapscan_read function. I've
 * tested it using both blocking and nonblocking I/O and it seems
 * to work both ways.
 * I've also switched to GTK+-0.99.2 and sane-0.69, and the
 * mysterious problems with the preview window have disappeared.
 * Problems with scanimage doing weird things to options have also
 * gone away and the frontends seem more stable.
 *
 * Revision 1.12  1998/01/21  11:05:53  charter
 * Inoperative code largely #defined out; I had the preview window
 * working correctly by having the window coordinates properly
 * constrained, but now the preview window bombs with a floating-
 * point error each time... I'm not sure yet what happened.
 * I've also figured out that we need to use reserve_unit and
 * release_unit in order to cancel scans in progress. This works
 * under scanimage, but I can't seem to find a way to fit cancellation
 * into xscanimage properly.
 *
 * Revision 1.11  1998/01/20  22:42:08  charter
 * Applied Franck's patch from Dec 17; preview mode is now grayscale.
 *
 * Revision 1.10  1997/12/10  23:33:12  charter
 * Slight change to some floating-point computations in the brightness
 * and contrast stuff. The controls don't seem to do anything to the
 * scanner though (I think these aren't actually supported in the
 * SnapScan).
 *
 * Revision 1.9  1997/11/26  15:40:50  charter
 * Brightness and contrast added by Michel.
 *
 * Revision 1.8  1997/11/12  12:55:40  charter
 * No longer exec after forking to do nonblocking scanning; found how
 * to fix the problems with SIGPIPEs from before.
 * Now support a config file like the other scanner drivers, and
 * can check whether a given device is an AGFA SnapScan (mini_inquiry()).
 *
 * Revision 1.7  1997/11/10  05:52:08  charter
 * Now have the child reader process and pipe stuff working, and
 * nonblocking mode. For large scans the nonblocking mode actually
 * seems to cut down on cpu hogging (though there is still a hit).
 *
 * Revision 1.6  1997/11/03  07:45:54  charter
 * Added the predef_window stuff. I've tried it with 6x4, and it seems
 * to work; I think something gets inconsistent if a preview is
 * performed though.
 *
 * Revision 1.5  1997/11/03  03:15:27  charter
 * Global static variables have now become part of the scanner structure;
 * the inquiry command automatically retrieves window parameters into
 * scanner structure members. Things are a bit cleaned up.
 *
 * Revision 1.4  1997/11/02  23:35:28  charter
 * After much grief.... I can finally scan reliably. Now it's a matter
 * of getting the band arrangement sorted out.
 *
 * Revision 1.3  1997/10/30  07:36:37  charter
 * Fixed a stupid bug in the #defines for the inquiry command, pointed out
 * by Franck.
 *
 * Revision 1.2  1997/10/14  06:00:11  charter
 * Option manipulation and some basic SCSI commands done; the basics
 * for scanning are written but there are bugs. A full scan always hangs
 * the SCSI driver, and preview mode scans complete but it isn't clear
 * whether any meaningful data is received.
 *
 * Revision 1.1  1997/10/13  02:25:54  charter
 * Initial revision
 * */
