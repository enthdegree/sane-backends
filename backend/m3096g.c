static const char RCSid[] = "$Header$";
/* sane - Scanner Access Now Easy.

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

   This file implements a SANE backend for Fujitsu M3096G
   flatbed/ADF scanners.  It was derived from the COOLSCAN driver.
   Written by Randolph Bentson <bentson@holmsjoen.com> */

/* ------------------------------------------------------------------------- */
/*
 * $Log$
 * Revision 1.5  2001/10/10 21:50:22  hmg
 * Update (from Oliver Schirrmeister <oschirr@abm.de>). Added: Support for ipc2/3
 * and cmp2 options; support for duplex-scanners m3093DG, m4097DG; constraint checking
 * for m3093; support EVPD (virtual product data); support ADF paper size spezification.
 * Henning Meier-Geinitz <henning@meier-geinitz.de>
 *
 * Revision 1.4  2001/05/31 18:01:39  hmg
 * Fixed config_line[len-1] bug which could generate an access
 * violation if len==0.
 * Henning Meier-Geinitz <henning@meier-geinitz.de>
 *
 * Revision 1.3  2000/08/12 15:09:17  pere
 * Merge devel (v1.0.3) into head branch.
 *
 * Revision 1.1.2.6  2000/07/30 11:16:01  hmg
 * 2000-07-30  Henning Meier-Geinitz <hmg@gmx.de>
 *
 * 	* backend/mustek.*: Update to Mustek backend 1.0-95. Changed from
 * 	  wait() to waitpid() and removed unused code.
 * 	* configure configure.in backend/m3096g.c backend/sp15c.c: Reverted
 * 	  the V_REV patch. V_REV should not be used in backends.
 *
 * Revision 1.1.2.5  2000/07/29 21:38:12  hmg
 * 2000-07-29  Henning Meier-Geinitz <hmg@gmx.de>
 *
 * 	* backend/sp15.c backend/m3096g.c: Replace fgets with
 * 	  sanei_config_read, return V_REV as part of version_code string
 * 	  (patch from Randolph Bentson).
 *
 * Revision 1.1.2.4  2000/07/25 21:47:33  hmg
 * 2000-07-25  Henning Meier-Geinitz <hmg@gmx.de>
 *
 * 	* backend/snapscan.c: Use DBG(0, ...) instead of fprintf (stderr, ...).
 * 	* backend/abaton.c backend/agfafocus.c backend/apple.c backend/dc210.c
 *  	  backend/dll.c backend/dmc.c backend/microtek2.c backend/pint.c
 * 	  backend/qcam.c backend/ricoh.c backend/s9036.c backend/snapscan.c
 * 	  backend/tamarack.c: Use sanei_config_read instead of fgets.
 * 	* backend/dc210.c backend/microtek.c backend/pnm.c: Added
 * 	  #include <sane/config.h>.
 * 	* backend/dc25.c backend/m3096.c  backend/sp15.c
 *  	  backend/st400.c: Moved #include <sane/config.h> to the beginning.
 * 	* AUTHORS: Changed agfa to agfafocus.
 *
 * Revision 1.1.2.3  2000/03/14 17:47:07  abel
 * new version of the Sharp backend added.
 *
 * Revision 1.1.2.2  2000/01/26 03:51:46  pere
 * Updated backends sp15c (v1.12) and m3096g (v1.11).
 *
 * Revision 1.11  2000/01/25 16:24:15  bentson
 * expand tabs; add debug message; clean-up compiler warnings
 *
 * Revision 1.10  2000/01/05 05:25:19  bentson
 * indent to barfin' GNU style
 *
 * Revision 1.9  2000/01/05 05:24:06  bentson
 * fixin' boundary conditions on paper size
 *
 * Revision 1.8.1.1  1999/12/20 20:25:05  bentson
 * hack for preview resolution
 *
 * Revision 1.8  1999/12/16 16:08:56  bentson
 * fix problem with landscape ADF operation
 *
 * Revision 1.7  1999/12/04 00:48:36  bentson
 * cosmetic changes only
 *
 * Revision 1.6  1999/11/24 20:05:10  bentson
 * minor fix to size parameter controls
 *
 * Revision 1.5  1999/11/23 18:47:27  bentson
 * add some constraint checking
 *
 * Revision 1.4  1999/11/19 17:29:15  bentson
 * enhance control of device (works with xscanimage)
 *
 * Revision 1.3  1999/11/18 18:13:36  bentson
 * basic grayscale scanning works
 *
 * Revision 1.2  1999/11/17 00:36:19  bentson
 * basic lineart scanning works
 *
 * Revision 1.1  1999/11/12 05:41:07  bentson
 * can move paper, but not yet scan
 *
 */

/* SANE-FLOW-DIAGRAMM

   - sane_init() : initialize backend, attach scanners
   . - sane_get_devices() : query list of scanner-devices
   . - sane_open() : open a particular scanner-device
   . . - sane_set_io_mode : set blocking-mode
   . . - sane_get_select_fd : get scanner-fd
   . . - sane_get_option_descriptor() : get option informations
   . . - sane_control_option() : change option values
   . .
   . . - sane_start() : start image aquisition
   . .   - sane_get_parameters() : returns actual scan-parameters
   . .   - sane_read() : read image-data (from pipe)
   . .
   . . - sane_cancel() : cancel operation
   . - sane_close() : close opened scanner-device
   - sane_exit() : terminate use of backend
 */

/* ------------------------------------------------------------------------- */

#include "sane/config.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "sane/sanei_backend.h"
#include "sane/sanei_scsi.h"
#include "sane/saneopts.h"
#include "sane/sanei_config.h"


#include "m3096g-scsi.h"
#include "m3096g.h"

/* ------------------------------------------------------------------------- */

static const char negativeStr[] = "Negative";
static const char positiveStr[] = "Positive";
static SANE_String_Const type_list[] =
{positiveStr, negativeStr, 0};

static SANE_String_Const source_list[] =
{"ADF", "FB", NULL};

#ifdef no_preview_res
static const SANE_Int resolution_list[] =
{5, 0, 200, 240, 300, 400};
#endif


static SANE_Int res_list0[] = 
  {4, 0, 200, 300, 400};                     /* 3096GX gray */

static SANE_Int res_list1[] = 
  {5, 0, 200, 240, 300, 400};                /* 3096GX binary */

static SANE_Int res_list2[] = 
  {7, 0, 100, 150, 200, 240, 300, 400};      /* 3093DG gray */

static SANE_Int res_list3[] = 
  {8, 0, 100, 150, 200, 240, 300, 400, 600}; /* 3093DG binary */


static const char lineStr[] = "Lineart";
static const char halfStr[] = "Halftone";
static const char grayStr[] = "Gray";
static SANE_String_Const scan_mode_list[] =
{lineStr, halfStr, grayStr, NULL};


/* how do the following work? */
static const SANE_Range brightness_range =
{0, 255, 32};
static const SANE_Range threshold_range =
{0, 255, 4};
static const SANE_Range contrast_range =
{0, 255, 1};


static const char cmp_none[] = "None";
static const char cmp_mh[]   = "MH";  /* Fax Group 3 */
static const char cmp_mr[]   = "MR";  /* what's that??? */
static const char cmp_mmr[]  = "MMR"; /* Fax Groupp 4 */
static SANE_String_Const compression_mode_list[] = 
  {cmp_none, cmp_mh, cmp_mr, cmp_mmr, NULL};

static const char gamma_default[] = "Default";
static const char gamma_normal[] = "Normal";
static const char gamma_soft[] = "Soft";
static const char gamma_sharp[] = "Sharp";
static SANE_String_Const gamma_mode_list[] = 
  {gamma_default, gamma_normal, gamma_soft, gamma_sharp, NULL};

static const char emphasis_none[]   = "None";
static const char emphasis_low[]    = "Low";
static const char emphasis_medium[] = "Medium";
static const char emphasis_high[]   = "High";
static const char emphasis_smooth[] = "Smooth";
static SANE_String_Const emphasis_mode_list[] = 
  {emphasis_none, emphasis_low, emphasis_medium, 
   emphasis_high, emphasis_smooth, NULL};

static const SANE_Range variance_rate_range =
{0, 255, 1};

static const SANE_Range threshold_curve_range =
{0, 7, 1};

static const char gradiation_ordinary[] = "Ordinary";
static const char gradiation_high[] = "High";
static SANE_String_Const gradiation_mode_list[] = 
  {gradiation_ordinary, gradiation_high, NULL};

static const char smoothing_mode_ocr[] = "OCR";
static const char smoothing_mode_image[] = "Image";
static SANE_String_Const smoothing_mode_list[] = 
  {smoothing_mode_ocr, smoothing_mode_image, NULL};

static const char filtering_ballpoint[] = "Ballpoint";
static const char filtering_ordinary[] = "Ordinary";
static SANE_String_Const filtering_mode_list[] = 
  {filtering_ballpoint, filtering_ordinary, NULL};

static const char background_white[] = "White";
static const char background_black[] = "Black";
static SANE_String_Const background_mode_list[] = 
  {background_white, background_black, NULL};

static const char white_level_follow_default[] = "Default";
static const char white_level_follow_enabled[] = "Enabled";
static const char white_level_follow_disabled[] = "Disabled";
static SANE_String_Const white_level_follow_mode_list[] = 
  {white_level_follow_default, white_level_follow_enabled, 
   white_level_follow_disabled, NULL};

static const char dtc_selection_default[] = "Default";
static const char dtc_selection_simplified[] = "Simplified";
static const char dtc_selection_dynamic[] = "Dynamic";
static SANE_String_Const dtc_selection_mode_list[] = 
  {dtc_selection_default, dtc_selection_simplified, dtc_selection_dynamic, NULL};

static const char paper_size_a3[] = "A3";
static const char paper_size_a4[] = "A4";
static const char paper_size_a5[] = "A5";
static const char paper_size_double[] = "Double";
static const char paper_size_letter[] = "Letter";
static const char paper_size_b4[] = "B4";
static const char paper_size_b5[] = "B5";
static const char paper_size_legal[] = "Legal";
static const char paper_size_custom[] = "Custom";
static const char paper_size_detect[] = "Autodetect";

static SANE_String_Const paper_size_mode_list[] = 
  {paper_size_a3, paper_size_a4, paper_size_a5, 
   paper_size_double, paper_size_letter, paper_size_b4, paper_size_b5, 
   paper_size_legal, paper_size_custom, paper_size_detect, NULL};

static const char paper_orientation_portrait[] = "Portrait";
static const char paper_orientation_landscape[] = "Landscape";
static SANE_String_Const paper_orientation_mode_list[] = 
  {paper_orientation_portrait, paper_orientation_landscape, NULL};


#if 0
static void
wabbit (void)
{
  int i;
  DBG (10, "%s\n", "\twait a bit before quitting\n");
  for (i = 0; i < 5; i++)
    {
      sleep (1);
      DBG (10, "\ttick\n");
    }
}                               /* wabbit */
#endif

static struct m3096g *current_scanner;

/* ################# externally visible routines ################{ */

SANE_Status                     /* looks like frontend ignores results */
sane_init (SANE_Int * version_code, SANE_Auth_Callback authorize)
{
  char dev_name[PATH_MAX];
  size_t len;
  FILE *fp;
  DBG_INIT ();
  DBG (10, "sane_init %d\n", authorize);

  if (version_code)
    *version_code = SANE_VERSION_CODE (V_MAJOR, V_MINOR, 0);
  fp = sanei_config_open (M3096G_CONFIG_FILE);
  if (!fp)
    {
      attach_scanner ("/dev/scanner", 0);       /* no config-file: /dev/scanner */
      return SANE_STATUS_GOOD;
    }

  while (sanei_config_read (dev_name, sizeof (dev_name), fp))
    {
      if (dev_name[0] == '#')
        continue;
      len = strlen (dev_name);
      if (!len)
        continue;
      sanei_config_attach_matching_devices (dev_name, attach_one);
    }

  fclose (fp);


  return SANE_STATUS_GOOD;
}                               /* sane_init */


SANE_Status
sane_get_devices (const SANE_Device *** device_list, SANE_Bool local_only)
{
  static const SANE_Device **devlist = 0;
  struct m3096g *dev;
  int i;

  DBG (10, "sane_get_devices %d\n", local_only);

  if (devlist)
    free (devlist);
  devlist = calloc (num_devices + 1, sizeof (devlist[0]));
  if (!devlist)
    return SANE_STATUS_NO_MEM;

  for (dev = first_dev, i = 0; i < num_devices; dev = dev->next)
    devlist[i++] = &dev->sane;
  devlist[i++] = 0;

  *device_list = devlist;

  return SANE_STATUS_GOOD;
}                               /* sane_get_devices */


SANE_Status
sane_open (SANE_String_Const name, SANE_Handle * handle)
{
  struct m3096g *dev = first_dev;

  DBG (10, "sane_open %s\n", name);

  if (!dev)
    return SANE_STATUS_INVAL;

  init_options (dev);
  *handle = dev;

  if (dev->autofeeder)
  dev->use_adf = SANE_TRUE;
  else
    dev->use_adf = SANE_FALSE;

  dev->x_res = 200;
  dev->y_res = 200;
  dev->tl_x = 0;
  dev->tl_y = 0;
  dev->br_x = 1200 * 17 / 2;
  dev->br_y = 1200 * 11;
  dev->brightness = 128;
  dev->threshold = 0;
  dev->contrast = 0;
  dev->composition = WD_comp_LA;
  dev->opt[OPT_BRIGHTNESS].cap = SANE_CAP_INACTIVE;
  dev->opt[OPT_THRESHOLD].cap = SANE_CAP_SOFT_DETECT
    | SANE_CAP_SOFT_SELECT;
  dev->opt[OPT_CONTRAST].cap = SANE_CAP_SOFT_DETECT
    | SANE_CAP_SOFT_SELECT;
  dev->bitsperpixel = 1;
  dev->halftone = 0;
  dev->rif = 0;
  dev->bitorder = 0;
  dev->compress_type = 0;
  dev->compress_arg = 0;
  dev->vendor_id_code = 0;
  dev->gamma = WD_gamma_DEFAULT;
  dev->outline = 0;
  dev->emphasis = WD_emphasis_NONE;
  dev->auto_sep = 0;
  dev->mirroring = 0;
  dev->var_rate_dyn_thresh = 0;

  dev->dtc_threshold_curve = 0;
  dev->gradiation = WD_gradiation_ORDINARY;
  dev->smoothing_mode = WD_smoothing_IMAGE;
  dev->filtering = WD_filtering_ORDINARY;
  dev->background = WD_background_WHITE;
  dev->matrix2x2 = 0;
  dev->matrix3x3 = 0;
  dev->matrix4x4 = 0;
  dev->matrix5x5 = 0;
  dev->noise_removal = 0; 
  dev->opt[OPT_MATRIX2X2].cap = SANE_CAP_INACTIVE;
  dev->opt[OPT_MATRIX3X3].cap = SANE_CAP_INACTIVE;
  dev->opt[OPT_MATRIX4X4].cap = SANE_CAP_INACTIVE;
  dev->opt[OPT_MATRIX5X5].cap = SANE_CAP_INACTIVE;

  dev->white_level_follow = WD_white_level_follow_DEFAULT;

  dev->subwindow_list = 0;

  dev->paper_size        = WD_paper_A4;
  dev->paper_orientation = WD_paper_PORTRAIT;
  dev->paper_selection   = WD_paper_SEL_STANDARD;

  dev->paper_width_X = 0;
  dev->paper_length_Y = 0;

  dev->duplex = 0;

  dev->dtc_selection = WD_dtc_selection_DEFAULT;
  dev->opt[OPT_NOISE_REMOVAL].cap = SANE_CAP_INACTIVE;
  dev->opt[OPT_BACKGROUND].cap = SANE_CAP_INACTIVE;
  dev->opt[OPT_FILTERING].cap = SANE_CAP_INACTIVE;
  dev->opt[OPT_SMOOTHING_MODE].cap = SANE_CAP_INACTIVE;
  dev->opt[OPT_GRADIATION].cap = SANE_CAP_INACTIVE;
  dev->opt[OPT_THRESHOLD_CURVE].cap = SANE_CAP_INACTIVE;
  dev->opt[OPT_VARIANCE_RATE].cap = SANE_CAP_INACTIVE;


  return SANE_STATUS_GOOD;
}                               /* sane_open */


SANE_Status
sane_set_io_mode (SANE_Handle h, SANE_Bool non_blocking)
{
  DBG (10, "sane_set_io_mode \n");
  DBG (99, "%d %d\n", non_blocking, h); /* avoid compiler warning */

  return SANE_STATUS_UNSUPPORTED;
}                               /* sane_set_io_mode */


SANE_Status
sane_get_select_fd (SANE_Handle h, SANE_Int * fdp)
{
  DBG (10, "sane_get_select_fd\n");
  DBG (99, "%d %d\n", fdp, h); /* avoid compiler warning */
  return SANE_STATUS_UNSUPPORTED;
}                               /* sane_get_select_fd */


const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
  struct m3096g *scanner = handle;

  DBG (10, "sane_get_option_descriptor: \"%s\"\n",
       scanner->opt[option].name);

  if ((unsigned) option >= NUM_OPTIONS)
    return 0;
  return &scanner->opt[option];
}                               /* sane_get_option_descriptor */


SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option,
                     SANE_Action action, void *val,
                     SANE_Int * info)
{
  struct m3096g *scanner = handle;
  SANE_Status status;
  SANE_Word cap;

  if (info)
    *info = 0;

  if (scanner->scanning == SANE_TRUE)
    {
      DBG (5, "sane_control_option: device busy\n");
      return SANE_STATUS_DEVICE_BUSY;
    }

  if (option >= NUM_OPTIONS)
    return SANE_STATUS_INVAL;

  cap = scanner->opt[option].cap;

  if (action == SANE_ACTION_GET_VALUE)
    {
      DBG (10, "sane_control_option: get value \"%s\"\n",
           scanner->opt[option].name);
      DBG (11, "\tcap = %d\n", cap);

      if (!SANE_OPTION_IS_ACTIVE (cap))
        {
          DBG (10, "\tinactive\n");
          return SANE_STATUS_INVAL;
        }

      switch (option)
        {

        case OPT_NUM_OPTS:
          *(SANE_Word *) val = NUM_OPTIONS;
          return SANE_STATUS_GOOD;

        case OPT_SOURCE:
          if (scanner->use_adf == SANE_TRUE)
            {
              strcpy (val, "ADF");
            }
          else
            {
              strcpy (val, "FB");
            }
          return SANE_STATUS_GOOD;

        case OPT_MODE:
          switch (scanner->composition)
            {
            case WD_comp_LA:
              strcpy (val, lineStr);
              break;
            case WD_comp_HT:
              strcpy (val, halfStr);
              break;
            case WD_comp_GS:
              strcpy (val, grayStr);
              break;
            default:
              return SANE_STATUS_INVAL;
            }
          if (info)
            {
              *info |= SANE_INFO_RELOAD_PARAMS;
            }
          return SANE_STATUS_GOOD;

        case OPT_TYPE:
          return SANE_STATUS_INVAL;

        case OPT_PRESCAN:
          return SANE_STATUS_INVAL;

        case OPT_X_RES:
          *(SANE_Word *) val = scanner->x_res;
          return SANE_STATUS_GOOD;

        case OPT_Y_RES:
          *(SANE_Word *) val = scanner->y_res;
          return SANE_STATUS_GOOD;

#ifdef no_preview_res
        case OPT_PREVIEW_RES:
          return SANE_STATUS_INVAL;
#endif

        case OPT_TL_X:
          *(SANE_Word *) val = SANE_FIX (iluToMm (scanner->tl_x));
          if (info)
            *info |= SANE_INFO_RELOAD_PARAMS;
          return SANE_STATUS_GOOD;

        case OPT_TL_Y:
          *(SANE_Word *) val = SANE_FIX (iluToMm (scanner->tl_y));
          if (info)
            *info |= SANE_INFO_RELOAD_PARAMS;
          return SANE_STATUS_GOOD;

        case OPT_BR_X:
          *(SANE_Word *) val = SANE_FIX (iluToMm (scanner->br_x));
          if (info)
            *info |= SANE_INFO_RELOAD_PARAMS;
          return SANE_STATUS_GOOD;

        case OPT_BR_Y:
          *(SANE_Word *) val = SANE_FIX (iluToMm (scanner->br_y));
          if (info)
            *info |= SANE_INFO_RELOAD_PARAMS;
          return SANE_STATUS_GOOD;

        case OPT_AVERAGING:
          return SANE_STATUS_INVAL;

        case OPT_BRIGHTNESS:
          *(SANE_Word *) val = scanner->brightness;
          return SANE_STATUS_GOOD;

        case OPT_THRESHOLD:
          *(SANE_Word *) val = scanner->threshold;
          return SANE_STATUS_GOOD;

        case OPT_CONTRAST:
          *(SANE_Word *) val = scanner->contrast;
          return SANE_STATUS_GOOD;

        case OPT_RIF:
          *(SANE_Bool *) val = scanner->rif;
          return SANE_STATUS_GOOD;
          
        case OPT_COMPRESSION:
          switch (scanner->compress_type)
            {
            case WD_cmp_NONE:
              strcpy (val, cmp_none);
              break;
            case WD_cmp_MH:
              strcpy (val, cmp_mh);
              break;
            case WD_cmp_MR:
              strcpy (val, cmp_mr);
              break; 
            case WD_cmp_MMR:
              strcpy (val, cmp_mmr);
              break;
           default:
              return SANE_STATUS_INVAL;
            }
          return SANE_STATUS_GOOD;

        case OPT_GAMMA:
          switch (scanner->gamma) 
            { 
            case WD_gamma_DEFAULT:
              strcpy (val, gamma_default);
              break;
            case WD_gamma_NORMAL:
              strcpy (val, gamma_normal);
              break;
            case WD_gamma_SOFT:
              strcpy (val, gamma_soft);
              break;
            case WD_gamma_SHARP:
              strcpy (val, gamma_sharp);
              break;
            default:
              return SANE_STATUS_INVAL;
            }
          return SANE_STATUS_GOOD;

        case OPT_OUTLINE_EXTRACTION:
          *(SANE_Bool *) val = scanner->outline;
          return SANE_STATUS_GOOD;

        case OPT_EMPHASIS:
          switch (scanner->emphasis)
            {
            case WD_emphasis_NONE:
              strcpy (val, emphasis_none);
              break;
            case WD_emphasis_LOW:
              strcpy (val, emphasis_low);
              break;
            case WD_emphasis_MEDIUM:
              strcpy (val, emphasis_medium);
              break; 
            case WD_emphasis_HIGH:
              strcpy (val, emphasis_high);
              break;
            case WD_emphasis_SMOOTH:
              strcpy (val, emphasis_smooth);
              break;
            default:
              return SANE_STATUS_INVAL;
            }
          return SANE_STATUS_GOOD;

        case OPT_AUTOSEP:
          *(SANE_Bool *) val = scanner->auto_sep;
          return SANE_STATUS_GOOD;

        case OPT_MIRROR_IMAGE:
          *(SANE_Bool *) val = scanner->mirroring;
          return SANE_STATUS_GOOD;

        case OPT_VARIANCE_RATE:
          *(SANE_Word *) val = scanner->var_rate_dyn_thresh;
          return SANE_STATUS_GOOD;

        case OPT_THRESHOLD_CURVE:
          *(SANE_Word *) val = scanner->dtc_threshold_curve;
          return SANE_STATUS_GOOD;

        case OPT_GRADIATION:
          switch (scanner->gradiation)
            {
            case WD_gradiation_ORDINARY:
              strcpy (val, gradiation_ordinary);
              break;
            case WD_gradiation_HIGH:
              strcpy (val, gradiation_high);
              break;
           default:
              return SANE_STATUS_INVAL;
            }
          return SANE_STATUS_GOOD;

        case OPT_SMOOTHING_MODE:
          switch (scanner->smoothing_mode)
            {
            case WD_smoothing_OCR:
              strcpy (val, smoothing_mode_ocr);
              break;
            case WD_smoothing_IMAGE:
              strcpy (val, smoothing_mode_image);
              break;
           default:
              return SANE_STATUS_INVAL;
            }
          return SANE_STATUS_GOOD;

        case OPT_FILTERING:
          switch (scanner->filtering)
            {
            case WD_filtering_BALLPOINT:
              strcpy (val, filtering_ballpoint);
              break;
            case WD_filtering_ORDINARY:
              strcpy (val, filtering_ordinary);
              break;
           default:
              return SANE_STATUS_INVAL;
            }
          return SANE_STATUS_GOOD;

        case OPT_BACKGROUND:
          switch (scanner->background)
            {
            case WD_background_WHITE:
              strcpy (val, background_white);
              break;
            case WD_background_BLACK:
              strcpy (val, background_black);
              break;
           default:
              return SANE_STATUS_INVAL;
            }
          return SANE_STATUS_GOOD;

        case OPT_MATRIX2X2:
          *(SANE_Bool *) val = scanner->matrix2x2;
          return SANE_STATUS_GOOD;

        case OPT_MATRIX3X3:
          *(SANE_Bool *) val = scanner->matrix3x3;
          return SANE_STATUS_GOOD;

        case OPT_MATRIX4X4:
          *(SANE_Bool *) val = scanner->matrix4x4;
          return SANE_STATUS_GOOD;

        case OPT_MATRIX5X5:
          *(SANE_Bool *) val = scanner->matrix5x5;
          return SANE_STATUS_GOOD;

        case OPT_NOISE_REMOVAL:
          *(SANE_Bool *) val = scanner->noise_removal;
          return SANE_STATUS_GOOD;

        case OPT_WHITE_LEVEL_FOLLOW:
          switch (scanner->white_level_follow)
            {
            case WD_white_level_follow_DEFAULT:
              strcpy (val, white_level_follow_default);
              break;
            case WD_white_level_follow_ENABLED:
              strcpy (val, white_level_follow_enabled);
              break;
            case WD_white_level_follow_DISABLED:
              strcpy (val, white_level_follow_disabled);
              break;
           default:
              return SANE_STATUS_INVAL;
            }
          return SANE_STATUS_GOOD;

        case OPT_DTC_SELECTION:
          switch (scanner->dtc_selection)
            {
            case WD_dtc_selection_DEFAULT:
              strcpy (val, dtc_selection_default);
              break;
            case WD_dtc_selection_DYNAMIC:
              strcpy (val, dtc_selection_dynamic);
              break;
            case WD_dtc_selection_SIMPLIFIED:
              strcpy (val, dtc_selection_simplified);
              break;
           default:
              return SANE_STATUS_INVAL;
            }
          return SANE_STATUS_GOOD;

        case OPT_PREVIEW:
          return SANE_STATUS_INVAL;

        case OPT_DUPLEX:
          *(SANE_Bool *) val = scanner->duplex;
          return SANE_STATUS_GOOD;

	case OPT_PAPER_SIZE:
          switch (scanner->paper_size)
            {
            case WD_paper_A3:
              strcpy (val, paper_size_a3);
              break;
            case WD_paper_A4:
              strcpy (val, paper_size_a4);
              break;
            case WD_paper_A5:
              strcpy (val, paper_size_a5);
              break;
            case WD_paper_DOUBLE:
              strcpy (val, paper_size_double);
              break;
            case WD_paper_LETTER:
              strcpy (val, paper_size_letter);
              break;
            case WD_paper_B4:
              strcpy (val, paper_size_b4);
              break;
            case WD_paper_B5:
              strcpy (val, paper_size_b5);
              break;
            case WD_paper_LEGAL:
              strcpy (val, paper_size_legal);
              break;
	    case WD_paper_UNDEFINED:
	      if (scanner->paper_selection == WD_paper_UNDEFINED)
		{
		  strcpy(val, paper_size_detect);
		}
	      else
		{
		  strcpy(val, paper_size_custom);
		}
	      break;
           default:
              return SANE_STATUS_INVAL;
            }
	  return SANE_STATUS_GOOD;

	case OPT_PAPER_ORIENTATION:
          switch (scanner->paper_orientation)
            {
	    case WD_paper_PORTRAIT:
              strcpy (val, paper_orientation_portrait);
              break;
	    case WD_paper_LANDSCAPE:
              strcpy (val, paper_orientation_landscape);
              break;
	    default:
              return SANE_STATUS_INVAL;
            }
	  return SANE_STATUS_GOOD;

	case OPT_PAPER_WIDTH:
          *(SANE_Word *) val = SANE_FIX (iluToMm (scanner->paper_width_X));
          if (info)
            *info |= SANE_INFO_RELOAD_PARAMS;
	  return SANE_STATUS_GOOD;
	  
	case OPT_PAPER_HEIGHT:
          *(SANE_Word *) val = SANE_FIX (iluToMm (scanner->paper_length_Y));
          if (info)
            *info |= SANE_INFO_RELOAD_PARAMS;
	  return SANE_STATUS_GOOD;

        case OPT_START_BUTTON:
	  if ( SANE_STATUS_GOOD == m3096g_get_hardware_status(scanner) )
	    {
	      *(SANE_Bool *) val = get_HW_start_button(scanner->buffer);
	    } 
	  else
	    {
	      *(SANE_Bool *) val = SANE_FALSE;
	    }
          return SANE_STATUS_GOOD;

        }



    }
  else if (action == SANE_ACTION_SET_VALUE)
    {
      DBG (10, "sane_control_option: set value \"%s\"\n",
           scanner->opt[option].name);

      if (!SANE_OPTION_IS_ACTIVE (cap))
        {
          DBG (10, "\tinactive\n");
          return SANE_STATUS_INVAL;
        }

      if (!SANE_OPTION_IS_SETTABLE (cap))
        {
          DBG (10, "\tnot settable\n");
          return SANE_STATUS_INVAL;
        }

      status = sanei_constrain_value (scanner->opt + option, val, info);
      if (status != SANE_STATUS_GOOD)
        {
          DBG (10, "\tbad value\n");
          return status;
        }

      switch (option)
        {

        case OPT_NUM_OPTS:
          return SANE_STATUS_GOOD;

        case OPT_SOURCE:
          if (strcmp (val, "ADF") == 0)
            {
              if (scanner->use_adf == SANE_TRUE)
              return SANE_STATUS_GOOD;

              scanner->use_adf = SANE_TRUE;
              if (scanner->is_duplex) 
                scanner->opt[OPT_DUPLEX].cap = SANE_CAP_SOFT_SELECT | 
                  SANE_CAP_SOFT_DETECT;

	      scanner->opt[OPT_PAPER_SIZE].cap = SANE_CAP_SOFT_SELECT | 
		SANE_CAP_SOFT_DETECT;
	      scanner->opt[OPT_PAPER_ORIENTATION].cap = SANE_CAP_SOFT_SELECT | 
		SANE_CAP_SOFT_DETECT;

            }
          else if (strcmp (val, "FB") == 0)
            {
              if (scanner->use_adf == SANE_FALSE)
              return SANE_STATUS_GOOD;

              scanner->use_adf = SANE_FALSE;
              scanner->opt[OPT_DUPLEX].cap = SANE_CAP_INACTIVE;
              scanner->duplex = 0;
	      scanner->opt[OPT_PAPER_SIZE].cap = SANE_CAP_INACTIVE;
	      scanner->opt[OPT_PAPER_ORIENTATION].cap = SANE_CAP_INACTIVE;
            }
          else
            {
              return SANE_STATUS_INVAL;
            }

          if (info)
            {
              *info |= SANE_INFO_RELOAD_PARAMS |SANE_INFO_RELOAD_OPTIONS;
            }

          return SANE_STATUS_GOOD;

        case OPT_MODE:
          if (strcmp (val, lineStr) == 0)
            {
              if (scanner->composition == WD_comp_LA)
                return SANE_STATUS_GOOD;
              scanner->composition = WD_comp_LA;
              scanner->bitsperpixel = 1;
              scanner->threshold = 0;
	      scanner->opt[OPT_X_RES].constraint.word_list = scanner->x_res_list;
	      scanner->opt[OPT_Y_RES].constraint.word_list = scanner->y_res_list;
              scanner->opt[OPT_BRIGHTNESS].cap = SANE_CAP_INACTIVE;
              scanner->opt[OPT_THRESHOLD].cap = SANE_CAP_SOFT_DETECT
                | SANE_CAP_SOFT_SELECT;
              scanner->opt[OPT_CONTRAST].cap = SANE_CAP_SOFT_DETECT
                | SANE_CAP_SOFT_SELECT;
              if (scanner->cmp2)
                scanner->opt[OPT_COMPRESSION].cap = SANE_CAP_SOFT_DETECT
                  | SANE_CAP_SOFT_SELECT;;
              scanner->opt[OPT_GAMMA].cap = SANE_CAP_SOFT_DETECT
                | SANE_CAP_SOFT_SELECT;;

              if (!m3096g_valid_number (scanner->x_res,
                              scanner->opt[OPT_X_RES].constraint.word_list))
                {
                  scanner->x_res = 240;
                }
              if (!m3096g_valid_number (scanner->y_res,
                              scanner->opt[OPT_Y_RES].constraint.word_list))
                {
                  scanner->y_res = 240;
                }

              scanner->rif = 0;
            }
          else if (strcmp (val, halfStr) == 0)
            {
              if (scanner->composition == WD_comp_HT)
                return SANE_STATUS_GOOD;
              scanner->composition = WD_comp_HT;
              scanner->bitsperpixel = 1;
	      scanner->opt[OPT_X_RES].constraint.word_list = scanner->x_res_list;
	      scanner->opt[OPT_Y_RES].constraint.word_list = scanner->y_res_list;
              scanner->opt[OPT_BRIGHTNESS].cap = SANE_CAP_SOFT_DETECT
                | SANE_CAP_SOFT_SELECT;
              scanner->opt[OPT_THRESHOLD].cap = SANE_CAP_INACTIVE;
              scanner->opt[OPT_CONTRAST].cap = SANE_CAP_SOFT_DETECT
                | SANE_CAP_SOFT_SELECT;
              if (scanner->cmp2)
                scanner->opt[OPT_COMPRESSION].cap = SANE_CAP_SOFT_DETECT
                  | SANE_CAP_SOFT_SELECT;;
              scanner->opt[OPT_GAMMA].cap = SANE_CAP_SOFT_DETECT
                | SANE_CAP_SOFT_SELECT;;
              if (!m3096g_valid_number (scanner->x_res,
                              scanner->opt[OPT_X_RES].constraint.word_list))
                {
                  scanner->x_res = 200;
                }
              if (!m3096g_valid_number (scanner->y_res,
                              scanner->opt[OPT_Y_RES].constraint.word_list))
                {
                  scanner->y_res = 200;
                }
              scanner->rif = 0;
            }
          else if (strcmp (val, grayStr) == 0)
            {
              if (scanner->composition == WD_comp_GS)
                return SANE_STATUS_GOOD;
              scanner->composition = WD_comp_GS;
              scanner->bitsperpixel = 8;
              scanner->compress_type = WD_cmp_NONE;
	      scanner->opt[OPT_X_RES].constraint_type = SANE_CONSTRAINT_WORD_LIST;
              scanner->opt[OPT_X_RES].constraint.word_list = scanner->x_res_list_grey;
	      scanner->opt[OPT_Y_RES].constraint_type = SANE_CONSTRAINT_WORD_LIST;
              scanner->opt[OPT_Y_RES].constraint.word_list = scanner->y_res_list_grey;
              scanner->opt[OPT_COMPRESSION].cap = SANE_CAP_INACTIVE;
              scanner->opt[OPT_GAMMA].cap = SANE_CAP_INACTIVE;
              scanner->opt[OPT_BRIGHTNESS].cap = SANE_CAP_INACTIVE;
              scanner->opt[OPT_THRESHOLD].cap = SANE_CAP_INACTIVE;
              scanner->opt[OPT_CONTRAST].cap = SANE_CAP_INACTIVE;
              if (!m3096g_valid_number (scanner->x_res,
                              scanner->opt[OPT_X_RES].constraint.word_list))
                {
                  scanner->x_res = 400;
                }
              if (!m3096g_valid_number (scanner->y_res,
                              scanner->opt[OPT_Y_RES].constraint.word_list))
                {
                  scanner->y_res = 400;
                }
              scanner->rif = 1;
            }
          else
            {
              return SANE_STATUS_INVAL;
            }
          if (info)
            {
              *info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;
            }
          return SANE_STATUS_GOOD;

        case OPT_TYPE:
          return SANE_STATUS_INVAL;

        case OPT_PRESCAN:
          return SANE_STATUS_INVAL;

        case OPT_X_RES:
          scanner->x_res = (*(SANE_Word *) val);
          if (info)
            *info |= SANE_INFO_RELOAD_PARAMS;
          return SANE_STATUS_GOOD;

        case OPT_Y_RES:
          scanner->y_res = (*(SANE_Word *) val);
          if (info)
            *info |= SANE_INFO_RELOAD_PARAMS;
          return SANE_STATUS_GOOD;

#ifdef no_preview_res
        case OPT_PREVIEW_RES:
          return SANE_STATUS_INVAL;
#endif

        case OPT_TL_X:
          scanner->tl_x = mmToIlu (SANE_UNFIX (*(SANE_Word *) val));
          *(SANE_Word *) val = SANE_FIX (iluToMm (scanner->tl_x));
          if (info)
            *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_INEXACT;
          return SANE_STATUS_GOOD;

        case OPT_TL_Y:
          scanner->tl_y = mmToIlu (SANE_UNFIX (*(SANE_Word *) val));
          *(SANE_Word *) val = SANE_FIX (iluToMm (scanner->tl_y));
          if (info)
            *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_INEXACT;
          return SANE_STATUS_GOOD;

        case OPT_BR_X:
          scanner->br_x = mmToIlu (SANE_UNFIX (*(SANE_Word *) val));
          *(SANE_Word *) val = SANE_FIX (iluToMm (scanner->br_x));
          /*scanner->paper_width_X = mmToIlu (SANE_UNFIX (*(SANE_Word *) val));*/
          if (info)
            *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_INEXACT;
          return SANE_STATUS_GOOD;

        case OPT_BR_Y:
          scanner->br_y = mmToIlu (SANE_UNFIX (*(SANE_Word *) val));
          *(SANE_Word *) val = SANE_FIX (iluToMm (scanner->br_y));
          /*scanner->paper_length_Y = mmToIlu (SANE_UNFIX (*(SANE_Word *) val));*/
          if (info)
            *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_INEXACT;
          return SANE_STATUS_GOOD;

        case OPT_AVERAGING:
          return SANE_STATUS_INVAL;

        case OPT_MIRROR_IMAGE:
          scanner->mirroring = *(SANE_Bool *) val;
          return SANE_STATUS_GOOD;

        case OPT_NOISE_REMOVAL:

          if (scanner->noise_removal == (*(SANE_Bool *) val) )
            {
              return SANE_STATUS_GOOD;
            }

          scanner->noise_removal = *(SANE_Bool *) val;
          
          if (!scanner->noise_removal) 
            {
              scanner->opt[OPT_MATRIX2X2].cap = SANE_CAP_INACTIVE;
              scanner->opt[OPT_MATRIX3X3].cap = SANE_CAP_INACTIVE;
              scanner->opt[OPT_MATRIX4X4].cap = SANE_CAP_INACTIVE;
              scanner->opt[OPT_MATRIX5X5].cap = SANE_CAP_INACTIVE;
            } 
          else 
            {

              scanner->opt[OPT_MATRIX2X2].cap = SANE_CAP_SOFT_DETECT
                | SANE_CAP_SOFT_SELECT;
              scanner->opt[OPT_MATRIX3X3].cap = SANE_CAP_SOFT_DETECT
                | SANE_CAP_SOFT_SELECT;
              scanner->opt[OPT_MATRIX4X4].cap = SANE_CAP_SOFT_DETECT
                | SANE_CAP_SOFT_SELECT;
              scanner->opt[OPT_MATRIX5X5].cap = SANE_CAP_SOFT_DETECT
                | SANE_CAP_SOFT_SELECT;
            }

          if (info)
            *info |= SANE_INFO_RELOAD_OPTIONS;

          return SANE_STATUS_GOOD;

        case OPT_MATRIX2X2:
          scanner->matrix2x2 = *(SANE_Bool *) val;
          return SANE_STATUS_GOOD;

        case OPT_MATRIX3X3:
          scanner->matrix3x3 = *(SANE_Bool *) val;
          return SANE_STATUS_GOOD;

        case OPT_MATRIX4X4:
          scanner->matrix4x4 = *(SANE_Bool *) val;
          return SANE_STATUS_GOOD;

        case OPT_MATRIX5X5:
          scanner->matrix5x5 = *(SANE_Bool *) val;
          return SANE_STATUS_GOOD;

        case OPT_AUTOSEP:
          scanner->auto_sep = *(SANE_Bool *) val;
          return SANE_STATUS_GOOD;

        case OPT_RIF:
          scanner->rif = *(SANE_Bool *) val;
          return SANE_STATUS_GOOD;

        case OPT_OUTLINE_EXTRACTION:
          scanner->outline = *(SANE_Bool *) val;
          return SANE_STATUS_GOOD;

        case OPT_COMPRESSION:
          if (strcmp (val, cmp_none) == 0)
            scanner->compress_type = WD_cmp_NONE;
          else if (strcmp (val, cmp_mh) == 0)   
            scanner->compress_type = WD_cmp_MH;
          else if (strcmp (val, cmp_mr) == 0)   
            scanner->compress_type = WD_cmp_MR;
          else if (strcmp (val, cmp_mmr) == 0)  
            scanner->compress_type = WD_cmp_MMR;
          else
            {
              return SANE_STATUS_INVAL;
            }

          return SANE_STATUS_GOOD;

        case OPT_EMPHASIS:
          if (strcmp (val, emphasis_none) == 0)
            scanner->emphasis = WD_emphasis_NONE;
          else if (strcmp (val, emphasis_low) == 0)     
            scanner->emphasis = WD_emphasis_LOW;
          else if (strcmp (val, emphasis_medium) == 0)  
            scanner->emphasis = WD_emphasis_MEDIUM;
          else if (strcmp (val, emphasis_high) == 0)    
            scanner->emphasis = WD_emphasis_HIGH;
          else if (strcmp (val, emphasis_smooth) == 0)  
            scanner->emphasis = WD_emphasis_SMOOTH;
          else
            {
              return SANE_STATUS_INVAL;
            }

          return SANE_STATUS_GOOD;

        case OPT_GAMMA:
          if (strcmp (val, gamma_default) == 0)
            scanner->gamma = WD_gamma_DEFAULT;
          else if (strcmp (val, gamma_normal) == 0)     
            scanner->gamma = WD_gamma_NORMAL;
          else if (strcmp (val, gamma_soft) == 0)       
            scanner->gamma = WD_gamma_SOFT;
          else if (strcmp (val, gamma_sharp) == 0)      
            scanner->gamma = WD_gamma_SHARP;
          else
            {
              return SANE_STATUS_INVAL;
            }

          return SANE_STATUS_GOOD;

        case OPT_THRESHOLD_CURVE:
          scanner->dtc_threshold_curve = *(SANE_Word *) val;
          return SANE_STATUS_GOOD;

        case OPT_GRADIATION:
          if (strcmp (val, gradiation_ordinary) == 0)
            scanner->gradiation = WD_gradiation_ORDINARY;
          else if (strcmp (val, gradiation_high) == 0)  
            scanner->gradiation = WD_gradiation_HIGH;
          else
            {
              return SANE_STATUS_INVAL;
            }

          return SANE_STATUS_GOOD;

        case OPT_SMOOTHING_MODE:
          if (strcmp (val, smoothing_mode_ocr) == 0)
            scanner->smoothing_mode = WD_smoothing_OCR;
          else if (strcmp (val, smoothing_mode_image) == 0)     
            scanner->smoothing_mode = WD_smoothing_IMAGE;
          else
            {
              return SANE_STATUS_INVAL;
            }

          return SANE_STATUS_GOOD;

        case OPT_FILTERING:
          if (strcmp (val, filtering_ballpoint) == 0)
            scanner->filtering = WD_filtering_BALLPOINT;
          else if (strcmp (val, filtering_ordinary) == 0)       
            scanner->filtering = WD_filtering_ORDINARY;
          else
            {
              return SANE_STATUS_INVAL;
            }

          return SANE_STATUS_GOOD;

        case OPT_BACKGROUND:
          if (strcmp (val, background_white) == 0)
            scanner->background = WD_background_WHITE;
          else if (strcmp (val, background_black) == 0) 
            scanner->background = WD_background_BLACK;
          else
            {
              return SANE_STATUS_INVAL;
            }

          return SANE_STATUS_GOOD;

        case OPT_WHITE_LEVEL_FOLLOW:
          if (strcmp (val, white_level_follow_default) == 0)
            scanner->white_level_follow = WD_white_level_follow_DEFAULT;
          else if (strcmp (val, white_level_follow_enabled) == 0)
            scanner->white_level_follow = WD_white_level_follow_ENABLED;
          else if (strcmp (val, white_level_follow_disabled) == 0)
            scanner->white_level_follow = WD_white_level_follow_DISABLED;
          else
            {
              return SANE_STATUS_INVAL;
            }

          return SANE_STATUS_GOOD;

        case OPT_DTC_SELECTION:
          if (strcmp (val, dtc_selection_default) == 0)
            {
              if (scanner->dtc_selection == WD_dtc_selection_DEFAULT) 
                return SANE_STATUS_GOOD;

              scanner->dtc_selection = WD_dtc_selection_DEFAULT;
            }
          else if (strcmp (val, dtc_selection_dynamic) == 0)
            {
              if (scanner->dtc_selection == WD_dtc_selection_DYNAMIC) 
                return SANE_STATUS_GOOD;

              scanner->dtc_selection = WD_dtc_selection_DYNAMIC;
            }
          else if (strcmp (val, dtc_selection_simplified) == 0)
            {
              if (scanner->dtc_selection == WD_dtc_selection_SIMPLIFIED ) 
                return SANE_STATUS_GOOD;
              
              scanner->dtc_selection = WD_dtc_selection_SIMPLIFIED;
            }
          else
            {
              return SANE_STATUS_INVAL;
            }

          if (scanner->dtc_selection != WD_dtc_selection_DYNAMIC) 
            { 
              scanner->opt[OPT_NOISE_REMOVAL].cap = SANE_CAP_INACTIVE;
              scanner->opt[OPT_BACKGROUND].cap = SANE_CAP_INACTIVE;
              scanner->opt[OPT_FILTERING].cap = SANE_CAP_INACTIVE;
              scanner->opt[OPT_SMOOTHING_MODE].cap = SANE_CAP_INACTIVE;
              scanner->opt[OPT_GRADIATION].cap = SANE_CAP_INACTIVE;
              scanner->opt[OPT_THRESHOLD_CURVE].cap = SANE_CAP_INACTIVE;
              scanner->opt[OPT_VARIANCE_RATE].cap = SANE_CAP_INACTIVE;
              
              scanner->noise_removal = 0;
              if (info)
                *info |= SANE_INFO_RELOAD_PARAMS;
            } 
          else 
            {
              scanner->opt[OPT_NOISE_REMOVAL].cap = SANE_CAP_SOFT_DETECT
                | SANE_CAP_SOFT_SELECT;
              scanner->opt[OPT_BACKGROUND].cap = SANE_CAP_SOFT_DETECT
                | SANE_CAP_SOFT_SELECT;
              scanner->opt[OPT_FILTERING].cap = SANE_CAP_SOFT_DETECT
                | SANE_CAP_SOFT_SELECT;
              scanner->opt[OPT_SMOOTHING_MODE].cap = SANE_CAP_SOFT_DETECT
                | SANE_CAP_SOFT_SELECT;
              scanner->opt[OPT_GRADIATION].cap = SANE_CAP_SOFT_DETECT
                | SANE_CAP_SOFT_SELECT;
              scanner->opt[OPT_THRESHOLD_CURVE].cap = SANE_CAP_SOFT_DETECT
                | SANE_CAP_SOFT_SELECT;
              scanner->opt[OPT_VARIANCE_RATE].cap = SANE_CAP_SOFT_DETECT
                | SANE_CAP_SOFT_SELECT;
            }

          if (info)
            *info |= SANE_INFO_RELOAD_OPTIONS;


          return SANE_STATUS_GOOD;

        case OPT_PAPER_SIZE:


          if (strcmp (val, paper_size_a3) == 0)
	    {
	      if (scanner->paper_size == WD_paper_A3)
		return SANE_STATUS_GOOD;

	      scanner->paper_size = WD_paper_A3;
	      m3096g_set_standard_size(scanner);
	    }
          else if (strcmp (val, paper_size_a4) == 0)
	    {
	      if (scanner->paper_size == WD_paper_A4)
		return SANE_STATUS_GOOD;

	      scanner->paper_size = WD_paper_A4;
	      m3096g_set_standard_size(scanner);
	    }
          else if (strcmp (val, paper_size_a5) == 0)
	    {
	      if (scanner->paper_size == WD_paper_A5)
		return SANE_STATUS_GOOD;

	      scanner->paper_size = WD_paper_A5;
	      m3096g_set_standard_size(scanner);
	    }
          else if (strcmp (val, paper_size_double) == 0)
	    {
	      if (scanner->paper_size == WD_paper_DOUBLE)
		return SANE_STATUS_GOOD;

	      scanner->paper_size = WD_paper_DOUBLE;
	      m3096g_set_standard_size(scanner);
	    }
          else if (strcmp (val, paper_size_letter) == 0)
	    {
	      if (scanner->paper_size == WD_paper_LETTER)
		return SANE_STATUS_GOOD;

	      scanner->paper_size = WD_paper_LETTER;
	      m3096g_set_standard_size(scanner);
	    }
          else if (strcmp (val, paper_size_b4) == 0)
	    {
	      if (scanner->paper_size == WD_paper_B4)
		return SANE_STATUS_GOOD;

	      scanner->paper_size = WD_paper_B4;
	      m3096g_set_standard_size(scanner);
	    }
          else if (strcmp (val, paper_size_b5) == 0)
	    {
	      if (scanner->paper_size == WD_paper_B5)
		return SANE_STATUS_GOOD;

	      scanner->paper_size = WD_paper_B5;
	      m3096g_set_standard_size(scanner);
	    }
          else if (strcmp (val, paper_size_legal) == 0)
	    {
	      if (scanner->paper_size == WD_paper_LEGAL)
		return SANE_STATUS_GOOD;

	      scanner->paper_size = WD_paper_LEGAL;
	      m3096g_set_standard_size(scanner);
	    }
          else if (strcmp (val, paper_size_custom) == 0)
	    {
	      if (scanner->paper_size == WD_paper_UNDEFINED &&
		  scanner->paper_selection == WD_paper_SEL_NON_STANDARD)
		{
		  return SANE_STATUS_GOOD;
		}
	      scanner->paper_size = WD_paper_UNDEFINED;
	      scanner->paper_selection = WD_paper_SEL_NON_STANDARD;
              scanner->opt[OPT_PAPER_ORIENTATION].cap = SANE_CAP_INACTIVE;
	      scanner->opt[OPT_PAPER_WIDTH].cap = SANE_CAP_SOFT_SELECT | 
		SANE_CAP_SOFT_DETECT;
	      scanner->opt[OPT_PAPER_HEIGHT].cap = SANE_CAP_SOFT_SELECT | 
		SANE_CAP_SOFT_DETECT;
	      if (info)
		*info |= SANE_INFO_RELOAD_PARAMS;
	    }
          else if (strcmp (val, paper_size_detect) == 0) 
	    {
	      if (scanner->paper_size == WD_paper_UNDEFINED &&
		  scanner->paper_selection == WD_paper_SEL_UNDEFINED)
		return SANE_STATUS_GOOD;

	      scanner->paper_size = WD_paper_UNDEFINED;
	      scanner->paper_orientation = WD_paper_PORTRAIT;
	      scanner->paper_selection = WD_paper_SEL_UNDEFINED;
              scanner->opt[OPT_PAPER_ORIENTATION].cap = SANE_CAP_INACTIVE;
	      scanner->opt[OPT_PAPER_WIDTH].cap = SANE_CAP_INACTIVE;
	      scanner->opt[OPT_PAPER_HEIGHT].cap = SANE_CAP_INACTIVE;
	      if (info)
		*info |= SANE_INFO_RELOAD_PARAMS;
	    }
          else
            {
              return SANE_STATUS_INVAL;
            }


	  if (info)
	    *info |= SANE_INFO_RELOAD_OPTIONS;

          return SANE_STATUS_GOOD;


        case OPT_PAPER_ORIENTATION:
          if (strcmp (val, paper_orientation_portrait) == 0)
            scanner->paper_orientation = WD_paper_PORTRAIT;
          else if (strcmp (val, paper_orientation_landscape) == 0)
            scanner->paper_orientation = WD_paper_LANDSCAPE;
          else
            {
              return SANE_STATUS_INVAL;
            }

          return SANE_STATUS_GOOD;

        case OPT_PAPER_WIDTH:
          scanner->paper_width_X = mmToIlu (SANE_UNFIX (*(SANE_Word *) val));
          *(SANE_Word *) val = SANE_FIX (iluToMm (scanner->paper_width_X));
          if (info)
            *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_INEXACT;
          return SANE_STATUS_GOOD;

        case OPT_PAPER_HEIGHT:
          scanner->paper_length_Y = mmToIlu (SANE_UNFIX (*(SANE_Word *) val));
          *(SANE_Word *) val = SANE_FIX (iluToMm (scanner->paper_length_Y));
          if (info)
            *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_INEXACT;
          return SANE_STATUS_GOOD;

        case OPT_BRIGHTNESS:
          scanner->brightness = *(SANE_Word *) val;
          return SANE_STATUS_GOOD;

        case OPT_CONTRAST:
          scanner->contrast = *(SANE_Word *) val;
          return SANE_STATUS_GOOD;

        case OPT_VARIANCE_RATE:
          scanner->var_rate_dyn_thresh = *(SANE_Word *) val;
          return SANE_STATUS_GOOD;

        case OPT_THRESHOLD:
          scanner->threshold = *(SANE_Word *) val;
          return SANE_STATUS_GOOD;

        case OPT_DUPLEX:
          scanner->duplex = *(SANE_Bool *) val;
          return SANE_STATUS_GOOD;

        }                       /* switch */
    }                           /* else */
  return SANE_STATUS_INVAL;
}                               /* sane_control_option */


#if 0
static int m3096g_valid_x_resolution(SANE_Handle handle)
{
  struct m3096g *s = handle;

  DBG (5, "m3096g_valid_x_resolution\n");
  if (s->x_res_range.quant>0 && 0)
    {
      if (s->x_res >= s->x_res_range.min &&
	  s->x_res <= s->x_res_range.max)
	{
	  if ((s->x_res - s->x_res_range.min) % s->x_res_range.quant == 0) 
	    {
	      return 1;
	    }
	}
    } 
  else
    {
      return m3096g_valid_number(s->x_res, s->x_res_list);
    }

  return 0;
}


static int m3096g_valid_y_resolution(SANE_Handle handle)
{
  struct m3096g *s = handle;

  if (s->y_res_range.quant>0 && 0)
    {
      if (s->y_res >= s->y_res_range.min &&
	  s->y_res <= s->y_res_range.max)
	{
	  if ((s->y_res - s->y_res_range.min) % s->y_res_range.quant == 0) 
	    {
	      return 1;
	    }
	}
    } 
  else
    {
      return m3096g_valid_number(s->y_res, s->y_res_list);
    }

  return 0;
}

static void m3096g_set_binary_res(SANE_Handle handle)
{
  struct m3096g *scanner = handle;

  if (scanner->x_res_range.quant>0)
    {
      scanner->opt[OPT_X_RES].constraint_type = SANE_CONSTRAINT_RANGE;
      scanner->opt[OPT_X_RES].constraint.range = &scanner->x_res_range;
    }
  else
    {
      scanner->opt[OPT_X_RES].constraint_type = SANE_CONSTRAINT_WORD_LIST;
      scanner->opt[OPT_X_RES].constraint.word_list = scanner->x_res_list;
    }  

  if (scanner->y_res_range.quant>0)
    {
      scanner->opt[OPT_Y_RES].constraint_type = SANE_CONSTRAINT_RANGE;
      scanner->opt[OPT_Y_RES].constraint.range = &scanner->y_res_range;
    }
  else
    {
      scanner->opt[OPT_Y_RES].constraint_type = SANE_CONSTRAINT_WORD_LIST;
      scanner->opt[OPT_Y_RES].constraint.word_list = scanner->y_res_list;
    }
  return;
}
#endif

static void m3096g_set_standard_size(SANE_Handle handle)
{
  struct m3096g *scanner = handle;

  scanner->paper_selection = WD_paper_SEL_STANDARD;
  scanner->opt[OPT_PAPER_ORIENTATION].cap = SANE_CAP_SOFT_DETECT
    | SANE_CAP_SOFT_SELECT;
  scanner->opt[OPT_PAPER_WIDTH].cap = SANE_CAP_INACTIVE;
  scanner->opt[OPT_PAPER_HEIGHT].cap = SANE_CAP_INACTIVE;
  
  return;
}


SANE_Status
sane_start (SANE_Handle handle)
{
  struct m3096g *scanner = handle;
  int fds[2];
  int ret;
  int i_window_id;

  DBG (10, "sane_start\n");

  /*
  if (scanner->scanning == SANE_TRUE)
    {
      DBG (5, "sane_start: device busy\n");
      return SANE_STATUS_DEVICE_BUSY;
    }
  */

  if (scanner->sfd < 0)
    {                           /* first call */
      if (sanei_scsi_open (scanner->sane.name, &(scanner->sfd),
                           sense_handler, 0) != SANE_STATUS_GOOD)
        {
          DBG (1, "sane_start: open of %s failed:\n",
               scanner->sane.name);
          return SANE_STATUS_INVAL;
        }

  scanner->scanning = SANE_TRUE;


  if ((ret = m3096g_check_values (scanner)) != 0)
    {                           /* Verify values */
      DBG (1, "sane_start: ERROR: invalid scan-values\n");
      sanei_scsi_close (scanner->sfd);
      scanner->scanning = SANE_FALSE;
      scanner->sfd = -1;
      return SANE_STATUS_INVAL;
    }

  if ((ret = m3096g_grab_scanner (scanner)))
    {
      DBG (5, "sane_start: unable to reserve scanner\n");
      sanei_scsi_close (scanner->sfd);
      scanner->scanning = SANE_FALSE;
      scanner->sfd = -1;
      return ret;
    }

  if (scanner->use_adf == SANE_TRUE
      && (ret = m3096g_object_position (scanner)))
    {
      DBG (5, "sane_start: WARNING: ADF empty\n");
      m3096g_free_scanner (scanner);
      sanei_scsi_close (scanner->sfd);
      scanner->scanning = SANE_FALSE;
      scanner->sfd = -1;
      return ret;
    }

  swap_res (scanner);

  if ((ret = m3096g_set_window_param (scanner, 0)))
    {
      DBG (5, "sane_start: ERROR: failed to set window\n");
      m3096g_free_scanner (scanner);
      sanei_scsi_close (scanner->sfd);
      scanner->scanning = SANE_FALSE;
      scanner->sfd = -1;
      return ret;
    }


  DBG (10, "\tbytes per line = %d\n", bytes_per_line (scanner));
  DBG (10, "\tpixels_per_line = %d\n", pixels_per_line (scanner));
  DBG (10, "\tlines = %d\n", lines_per_scan (scanner));
  DBG (10, "\tbrightness (halftone) = %d\n", scanner->brightness);
  DBG (10, "\tthreshold (line art) = %d\n", scanner->threshold);

      /*
      if (( ret = m3096g_read_pixel_size(scanner, 28, 0))) 
	{
          DBG (5, "sane_start: unable to read pixel size\n");
          sanei_scsi_close (scanner->sfd);
          scanner->scanning = SANE_FALSE;
          scanner->sfd = -1;
          return ret;
	}
      else 
	{
	  DBG(1, "numx: %d\n", get_PSIZE_num_x(scanner->buffer));
	  DBG(1, "numy: %d\n", get_PSIZE_num_y(scanner->buffer));
	}
	 
      */

      
      if ((ret = m3096g_start_scan (scanner)))
        {
          DBG (5, "sane_start: unable to set reading method\n");
          sanei_scsi_close (scanner->sfd);
          scanner->scanning = SANE_FALSE;
          scanner->sfd = -1;
          return ret;
        }
      

      i_window_id = 0x00;
    } 
  else 
    {
      i_window_id = 0x80;
    }

  /* create a pipe, fds[0]=read-fd, fds[1]=write-fd */
  if (pipe (fds) < 0)
    {
      DBG (1, "ERROR: could not create pipe\n");
      swap_res (scanner);
      scanner->scanning = SANE_FALSE;
      m3096g_free_scanner (scanner);
      sanei_scsi_close (scanner->sfd);
      scanner->sfd = -1;
      return SANE_STATUS_IO_ERROR;
    }

  scanner->reader_pid = fork ();
  if (scanner->reader_pid == 0)
    {
      /* reader_pid = 0 ===> child process */
      sigset_t ignore_set;
      struct SIGACTION act;

      close (fds[0]);

      sigfillset (&ignore_set);
      sigdelset (&ignore_set, SIGTERM);
      sigprocmask (SIG_SETMASK, &ignore_set, 0);

      memset (&act, 0, sizeof (act));
      sigaction (SIGTERM, &act, 0);

      /* don't use exit() since that would run the atexit() handlers... */
      _exit (reader_process (scanner, fds[1], i_window_id));
    }
  close (fds[1]);
  scanner->pipe = fds[0];

  DBG (10, "sane_start: ok\n");
  return SANE_STATUS_GOOD;
}                               /* sane_start */


SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters * params)
{
  struct m3096g *scanner = handle;

  DBG (10, "sane_get_parameters\n");
  params->format = SANE_FRAME_GRAY;

  params->depth = scanner->bitsperpixel;
  params->pixels_per_line = pixels_per_line (scanner);
  params->lines = lines_per_scan (scanner);
  params->bytes_per_line = bytes_per_line (scanner);
  if ( scanner->i_num_frames > 1 ) 
    {
      params->last_frame = 0;
    } 
  else 
    {
  params->last_frame = 1;
    }
  DBG (10, "\tdepth %d\n", params->depth);
  DBG (10, "\tlines %d\n", params->lines);
  DBG (10, "\tpixels_per_line %d\n", params->pixels_per_line);
  DBG (10, "\tbytes_per_line %d\n", params->bytes_per_line);
  return SANE_STATUS_GOOD;
}                               /* sane_get_parameters */


SANE_Status
sane_read (SANE_Handle handle, SANE_Byte * buf,
           SANE_Int max_len, SANE_Int * len)
{
  struct m3096g *scanner = handle;
  ssize_t nread;

  DBG (10, "sane_read\n");
  *len = 0;

  scanner->i_num_frames = scanner->i_num_frames - 1;

  nread = read (scanner->pipe, buf, max_len);
  DBG (10, "sane_read: read %ld bytes of %ld\n",
       (long) nread, (long) max_len);

  if (scanner->scanning == SANE_FALSE)
    {
      /* PREDICATE WAS (!(scanner->scanning))  */
      return do_cancel (scanner);
    }

  if (nread < 0)
    {
      if (errno == EAGAIN)
        {
          return SANE_STATUS_GOOD;
        }
      else
        {
          do_cancel (scanner);
          return SANE_STATUS_IO_ERROR;
        }
    }

  *len = nread;

  if (nread == 0)
    return do_eof (scanner);    /* close pipe */

  return SANE_STATUS_GOOD;
}                               /* sane_read */


void
sane_cancel (SANE_Handle h)
{
  DBG (10, "sane_cancel\n");
  do_cancel ((struct m3096g *) h);
}                               /* sane_cancel */


void
sane_close (SANE_Handle handle)
{
  DBG (10, "sane_close\n");
  if (((struct m3096g *) handle)->scanning == SANE_TRUE)
    do_cancel (handle);
}                               /* sane_close */


void
sane_exit (void)
{
  struct m3096g *dev, *next;

  DBG (10, "sane_exit\n");

  for (dev = first_dev; dev; dev = next)
    {
      next = dev->next;
      free (dev->devicename);
      free (dev->buffer);
      free (dev);
    }
}                               /* sane_exit */

/* }################ internal (support) routines ################{ */

static SANE_Status
attach_scanner (const char *devicename, struct m3096g **devp)
{
  struct m3096g *dev;
  int sfd;

  DBG (15, "attach_scanner: %s\n", devicename);

  for (dev = first_dev; dev; dev = dev->next)
    {
      if (strcmp (dev->sane.name, devicename) == 0)
        {
          if (devp)
            {
              *devp = dev;
            }
          DBG (5, "attach_scanner: scanner already attached (is ok)!\n");
          return SANE_STATUS_GOOD;
        }
    }

  DBG (15, "attach_scanner: opening %s\n", devicename);
  if (sanei_scsi_open (devicename, &sfd, sense_handler, 0) != 0)
    {
      DBG (5, "attach_scanner: open failed\n");
      return SANE_STATUS_INVAL;
    }

  if (NULL == (dev = malloc (sizeof (*dev))))
    return SANE_STATUS_NO_MEM;

  dev->row_bufsize = (sanei_scsi_max_request_size < (64 * 1024))
    ? sanei_scsi_max_request_size
    : 64 * 1024;

  if ((dev->buffer = malloc (dev->row_bufsize)) == NULL)
    return SANE_STATUS_NO_MEM;

  dev->devicename = strdup (devicename);
  dev->sfd = sfd;

  if (m3096g_identify_scanner (dev) != 0)
    {
      DBG (5, "attach_scanner: scanner-identification failed\n");
      sanei_scsi_close (dev->sfd);
      free (dev->buffer);
      free (dev);
      return SANE_STATUS_INVAL;
    }

#if 0
  /* Get MUD (via mode_sense), internal info (via get_internal_info), and
     * initialize values */
  coolscan_initialize_values (dev);
#endif

  /* Why? */
  sanei_scsi_close (dev->sfd);
  dev->sfd = -1;

  dev->sane.name = dev->devicename;
  dev->sane.vendor = dev->vendor;
  dev->sane.model = dev->product;
  dev->sane.type = "scanner";

#if 0
  dev->x_range.min = SANE_FIX (0);
  dev->x_range.quant = SANE_FIX (length_quant);
  dev->x_range.max = SANE_FIX ((double) ((dev->xmaxpix) * length_quant));

  dev->y_range.min = SANE_FIX (0.0);
  dev->y_range.quant = SANE_FIX (length_quant);
  dev->y_range.max = SANE_FIX ((double) ((dev->ymaxpix) * length_quant));

  /* ...and this?? */
  dev->dpi_range.min = SANE_FIX (108);
  dev->dpi_range.quant = SANE_FIX (0);
  dev->dpi_range.max = SANE_FIX (dev->maxres);
  DBG (15, "attach: dev->dpi_range.max = %f\n",
       SANE_UNFIX (dev->dpi_range.max));
#endif

  ++num_devices;
  dev->next = first_dev;
  first_dev = dev;

  if (devp)
    {
      *devp = dev;
    }

  DBG (15, "attach_scanner: done\n");

  return SANE_STATUS_GOOD;
}                               /* attach_scanner */

static SANE_Status
attach_one (const char *name)
{
  return attach_scanner (name, 0);
}                               /* attach_one */

static SANE_Status
sense_handler (int scsi_fd, u_char * result, void *arg)
{
  DBG (99, "%d %s\n", scsi_fd, arg); /* avoid compiler warning */

  return request_sense_parse (result);
}                               /* sense_handler */

static int
request_sense_parse (u_char * sensed_data)
{
  unsigned int ret, sense, asc, ascq;

  sense = get_RS_sense_key (sensed_data);
  asc = get_RS_ASC (sensed_data);
  ascq = get_RS_ASCQ (sensed_data);

  ret = SANE_STATUS_IO_ERROR;

  switch (sense)
    {
    case 0x0:                   /* No Sense */
      DBG (5, "\t%d/%d/%d: Scanner ready\n", sense, asc, ascq);
      if ( get_RS_EOM(sensed_data) ) {
        
        current_scanner->i_transfer_length = get_RS_information(sensed_data);
        
        return SANE_STATUS_EOF;
      }
      return SANE_STATUS_GOOD;

    case 0x2:                   /* Not Ready */
      if ((0x00 == asc) && (0x00 == ascq))
        {
          DBG (1, "\t%d/%d/%d: Not Ready \n", sense, asc, ascq);
        }
      else
        {
          DBG (1, "\tUnknown - Sense=%d, ASC=%d, ASCQ=%d\n", sense, asc, ascq);
        }
      break;

    case 0x3:                   /* Medium Error */
      if ((0x80 == asc) && (0x01 == ascq))
        {
          DBG (1, "\t%d/%d/%d: Jam \n", sense, asc, ascq);
          ret = SANE_STATUS_JAMMED;
        }
      else if ((0x80 == asc) && (0x02 == ascq))
        {
          DBG (1, "\t%d/%d/%d: ADF cover open \n", sense, asc, ascq);
          ret = SANE_STATUS_COVER_OPEN;
        }
      else if ((0x80 == asc) && (0x03 == ascq))
        {
          DBG (1, "\t%d/%d/%d: ADF empty \n", sense, asc, ascq);
          ret = SANE_STATUS_NO_DOCS;
        }
      else
        {
          DBG (1, "\tUnknown - Sense=%d, ASC=%d, ASCQ=%d\n", sense, asc, ascq);
        }
      break;

    case 0x4:                   /* Hardware Error */
      if ((0x80 == asc) && (0x01 == ascq))
        {
          DBG (1, "\t%d/%d/%d: FB motor fuse \n", sense, asc, ascq);
        }
      else if ((0x80 == asc) && (0x02 == ascq))
        {
          DBG (1, "\t%d/%d/%d: heater fuse \n", sense, asc, ascq);
        }
      else if ((0x80 == asc) && (0x04 == ascq))
        {
          DBG (1, "\t%d/%d/%d: ADF motor fuse \n", sense, asc, ascq);
        }
      else if ((0x80 == asc) && (0x05 == ascq))
        {
          DBG (1, "\t%d/%d/%d: mechanical alarm \n", sense, asc, ascq);
        }
      else if ((0x80 == asc) && (0x06 == ascq))
        {
          DBG (1, "\t%d/%d/%d: optical alarm \n", sense, asc, ascq);
        }
      else if ((0x44 == asc) && (0x00 == ascq))
        {
          DBG (1, "\t%d/%d/%d: abnormal internal target \n", sense, asc, ascq);
        }
      else if ((0x47 == asc) && (0x00 == ascq))
        {
          DBG (1, "\t%d/%d/%d: SCSI parity error \n", sense, asc, ascq);
        }
      else
        {
          DBG (1, "\tUnknown - Sense=%d, ASC=%d, ASCQ=%d\n", sense, asc, ascq);
        }
      break;

    case 0x5:                   /* Illegal Request */
      if ((0x20 == asc) && (0x00 == ascq))
        {
          DBG (1, "\t%d/%d/%d: Invalid command \n", sense, asc, ascq);
          ret = SANE_STATUS_INVAL;
        }
      else if ((0x24 == asc) && (0x00 == ascq))
        {
          DBG (1, "\t%d/%d/%d: Invalid field in CDB \n", sense, asc, ascq);
          ret = SANE_STATUS_INVAL;
        }
      else if ((0x25 == asc) && (0x00 == ascq))
        {
          DBG (1, "\t%d/%d/%d: Unsupported logical unit \n", sense, asc, ascq);
          ret = SANE_STATUS_UNSUPPORTED;
        }
      else if ((0x26 == asc) && (0x00 == ascq))
        {
          DBG (1, "\t%d/%d/%d: Invalid field in parm list \n", sense, asc, ascq);
          ret = SANE_STATUS_INVAL;
        }
      else if ((0x2C == asc) && (0x02 == ascq))
        {
          DBG (1, "\t%d/%d/%d: wrong window combination \n", sense, asc, ascq);
          ret = SANE_STATUS_INVAL;
        }
      else
        {
          DBG (1, "\tUnknown - Sense=%d, ASC=%d, ASCQ=%d\n", sense, asc, ascq);
        }
      break;

    case 0x6:                   /* Unit Attention */
      if ((0x00 == asc) && (0x00 == ascq))
        {
          DBG (1, "\t%d/%d/%d: UNIT ATTENTION \n", sense, asc, ascq);
        }
      else
        {
          DBG (1, "\tUnknown - Sense=%d, ASC=%d, ASCQ=%d\n", sense, asc, ascq);
        }
      break;

    case 0xb:                   /* Aborted Command */
      if ((0x43 == asc) && (0x00 == ascq))
        {
          DBG (1, "\t%d/%d/%d: Message error \n", sense, asc, ascq);
        }
      else if ((0x80 == asc) && (0x01 == ascq))
        {
          DBG (1, "\t%d/%d/%d: Image transfer error \n", sense, asc, ascq);
        }
      else
        {
          DBG (1, "\tUnknown - Sense=%d, ASC=%d, ASCQ=%d\n", sense, asc, ascq);
        }
      break;

    default:
      DBG (1, "\tUnknown - Sense=%d, ASC=%d, ASCQ=%d\n", sense, asc, ascq);
    }
  return ret;
}                               /* request_sense_parse */

static int
m3096g_identify_scanner (struct m3096g *s)
{
  char vendor[9];
  char product[0x11];
  char version[5];
  char *pp;
  int  i;

  DBG (10, "identify_scanner\n");

  vendor[8] = product[0x10] = version[4] = 0;


  m3096g_do_inquiry (s);        /* get inquiry */
  if (get_IN_periph_devtype (s->buffer) != IN_periph_devtype_scanner)
    {
      DBG (5, "identify_scanner: not a scanner\n");
      return 1;
    }

  get_IN_vendor (s->buffer, vendor);
  get_IN_product (s->buffer, product);
  get_IN_version (s->buffer, version);

  if (strncmp ("FUJITSU ", vendor, 8))
    {
      DBG (5, "identify_scanner: \"%s\" isn't a Fujitsu product\n", vendor);
      return 1;
    }

  pp = &vendor[8];
  vendor[8] = ' ';
  while (*pp == ' ')
    {
      *pp-- = '\0';
    }

  pp = &product[0x10];
  product[0x10] = ' ';
  while (*(pp - 1) == ' ')
    {
      *pp-- = '\0';
    }                           /* leave one blank at the end! */

  pp = &version[4];
  version[4] = ' ';
  while (*pp == ' ')
    {
      *pp-- = '\0';
    }

  DBG (10, "Found %s scanner %s version %s on device %s\n",
       vendor, product, version, s->devicename);

  vendor[8] = '\0';
  product[16] = '\0';
  version[4] = '\0';

  strncpy (s->vendor, vendor, 9);
  strncpy (s->product, product, 17);
  strncpy (s->version, version, 5);


  s->autofeeder = 1;            /* inferred by product title M3096G */
  s->flatbed = 1;
  s->is_duplex = 0;

  s->has_hw_status = 0;
  s->has_outline = 0;
  s->has_emphasis = 0;
  s->has_autosep = 0;
  s->has_mirroring = 0;
  s->has_white_level_follow = 1;
  s->has_subwindow = 1;

  s->ipc = 0;
  s->cmp2 = 0;
  s->x_res_list[0] = 0;
  s->y_res_list[0] = 0;

  s->x_res_range.min   = s->y_res_range.min = 50;
  s->x_res_range.quant = s->y_res_range.quant = 0;
  s->x_grey_res_range.min   = s->y_grey_res_range.min = 50;
  s->x_grey_res_range.quant = s->y_grey_res_range.quant = 0;


  /* examples of product names: 3093GXim, 3097Gim, 3093GXm, ...
   * i == ipc present
   * m == cmp2 present
   */
  for ( i = 5; i < 16; i++ ) 
    {
      if (product[i]=='i')
        {
          s->ipc = 1;
        }
      else if (product[i]=='m')
        {
          s->cmp2 = 1;
        }
      else if (product[i]=='d')
        {
          s->is_duplex = 1;
        }

    }

  if (s->ipc)
    {
      s->has_outline = 1;
      s->has_emphasis = 1;
      s->has_autosep = 1;
      s->has_mirroring = 1;
    }

  /* initialize dpi-lists */
  if (!strncmp(product, "M3093DG", 6) ||
      !strncmp(product, "M4097D",  5)   )
    {
      for ( i = 0; i <= res_list3[0]; i++ )
	{
	  s->y_res_list[i] = s->x_res_list[i] = res_list3[i];
	}
      for ( i = 0; i <= res_list2[0]; i++ )
	{
	  s->x_res_list_grey[i] = s->y_res_list_grey[i] = res_list2[i];
	}

      if (s->cmp2) 
	{
	  s->x_res_range.max   = s->y_res_range.max = 800;
	  s->x_res_range.quant = s->y_res_range.quant = 1;
	  s->x_grey_res_range.max = s->y_grey_res_range.max = 400;
	  s->x_grey_res_range.quant = s->y_grey_res_range.quant = 1;
	}
    }
  else 
    {
      /* M3093GX/M3096GX */
      
      for ( i = 0; i <= res_list1[0]; i++ )
	{
	  s->y_res_list[i] = s->x_res_list[i] = res_list1[i];
	}
      for ( i = 0; i <= res_list0[0]; i++ )
	{
	  s->x_res_list_grey[i] = s->y_res_list_grey[i] = res_list0[i];
	}

      
      if (s->cmp2) 
	{
	  s->x_res_range.max   = s->y_res_range.max = 400;
	  s->x_res_range.quant = s->y_res_range.quant = 1;
	}
    }

  /* initialize scanning- and adf paper size*/

  s->x_range.min   = SANE_FIX(0);
  s->x_range.quant = SANE_FIX(length_quant);

  s->y_range.min   = SANE_FIX(0);
  s->y_range.quant = SANE_FIX(length_quant);

  s->adf_width_range.min   = SANE_FIX(0);
  s->adf_width_range.quant = SANE_FIX(length_quant);

  s->adf_height_range.min   = SANE_FIX(0);
  s->adf_height_range.quant = SANE_FIX(length_quant);

  if ( 0 == strncmp(product, "M3093", 5) )
    {
      /* A4 scanner */
      if ( 0 == strncmp(product, "M3093DG", 7) )
	{
	  s->x_range.max          = SANE_FIX(219);
	  s->y_range.max          = SANE_FIX(308);
	  s->adf_width_range.max  = SANE_FIX(219);
	  s->adf_height_range.max = SANE_FIX(308);
	} 
      else
	{
	  s->x_range.max          = SANE_FIX(215);
	  s->y_range.max          = SANE_FIX(308);
	  s->adf_width_range.max  = SANE_FIX(215);
	  s->adf_height_range.max = SANE_FIX(308);
	}
    }
  else
    {
      /* A3 scanner */
      s->x_range.max          = SANE_FIX(308);
      s->y_range.max          = SANE_FIX(438);
      s->adf_width_range.max  = SANE_FIX(308);
      s->adf_height_range.max = SANE_FIX(438);
    }

  
  /* Here's where to add code to fetch the "vital product data"!!! */
  if ( SANE_STATUS_GOOD == m3096g_get_vital_product_data(s) ) 
    {
      /* This scanner supports vital product data.
       * Use this data to set dpi-lists etc.
       */
      int   i_num_res;

      if (get_IN_page_length(s->buffer) > 0x19) 
	{
	  /* VPD extended format present */
	  s->autofeeder    = get_IN_adf(s->buffer);
	  s->is_duplex     = get_IN_duplex(s->buffer);
	  s->flatbed       = get_IN_flatbed(s->buffer);
	  s->has_hw_status = get_IN_has_hw_status(s->buffer);
	  s->has_outline   = get_IN_ipc_outline_extraction(s->buffer);
	  s->has_emphasis  = get_IN_ipc_image_emphasis(s->buffer);
	  s->has_autosep   = get_IN_ipc_auto_separation(s->buffer);
	  s->has_mirroring = get_IN_ipc_mirroring(s->buffer);
	  s->has_white_level_follow = get_IN_ipc_white_level_follow(s->buffer);
	  s->has_subwindow = get_IN_ipc_subwindow(s->buffer);
	}


      s->x_res_range.quant = get_IN_step_x_res(s->buffer);
      s->y_res_range.quant = get_IN_step_y_res(s->buffer);
      s->x_res_range.max   = get_IN_max_x_res(s->buffer);
      s->y_res_range.max   = get_IN_max_y_res(s->buffer);
      s->x_res_range.min   = get_IN_min_x_res(s->buffer);
      s->y_res_range.min   = get_IN_min_y_res(s->buffer);
  
      i_num_res = 0;
      if (get_IN_std_res_60(s->buffer)) 
	{
	  i_num_res ++;
	  s->x_res_list[i_num_res] = 60;
	}
      if (get_IN_std_res_75(s->buffer)) 
	{
	  i_num_res ++;
	  s->x_res_list[i_num_res] = 75;
	}
      if (get_IN_std_res_100(s->buffer)) 
	{
	  i_num_res ++;
	  s->x_res_list[i_num_res] = 100;
	}
      if (get_IN_std_res_120(s->buffer)) 
	{
	  i_num_res ++;
	  s->x_res_list[i_num_res] = 120;
	}
      if (get_IN_std_res_150(s->buffer)) 
	{
	  i_num_res ++;
	  s->x_res_list[i_num_res] = 150;
	}
      if (get_IN_std_res_160(s->buffer)) 
	{
	  i_num_res ++;
	  s->x_res_list[i_num_res] = 160;
	}
      if (get_IN_std_res_180(s->buffer)) 
	{
	  i_num_res ++;
	  s->x_res_list[i_num_res] = 180;
	}
      if (get_IN_std_res_200(s->buffer)) 
	{
	  i_num_res ++;
	  s->x_res_list[i_num_res] = 200;
	}

      if (get_IN_std_res_240(s->buffer)) 
	{
	  i_num_res ++;
	  s->x_res_list[i_num_res] = 240;
	}
      if (get_IN_std_res_300(s->buffer)) 
	{
	  i_num_res ++;
	  s->x_res_list[i_num_res] = 300;
	}
      if (get_IN_std_res_320(s->buffer)) 
	{
	  i_num_res ++;
	  s->x_res_list[i_num_res] = 320;
	}
      if (get_IN_std_res_400(s->buffer)) 
	{
	  i_num_res ++;
	  s->x_res_list[i_num_res] = 400;
	}
      if (get_IN_std_res_480(s->buffer)) 
	{
	  i_num_res ++;
	  s->x_res_list[i_num_res] = 480;
	}
      if (get_IN_std_res_600(s->buffer)) 
	{
	  i_num_res ++;
	  s->x_res_list[i_num_res] = 600;
	}
      if (get_IN_std_res_800(s->buffer)) 
	{
	  i_num_res ++;
	  s->x_res_list[i_num_res] = 800;
	}
      if (get_IN_std_res_1200(s->buffer)) 
	{
	  i_num_res ++;
	  s->x_res_list[i_num_res] = 1200;
	}
	  
      s->x_res_list[0] = i_num_res;

      for ( i = 0; i <= i_num_res; i++ )
	{
	  s->y_res_list[i]      = s->x_res_list[i];
	  /* can't get resolutions for grey scale reading ????*/
	}
    } 

  return 0;
}                               /* m3096g_identify_scanner */

static void
m3096g_do_inquiry (struct m3096g *s)
{
  DBG (10, "do_inquiry\n");

  memset (s->buffer, '\0', 256);        /* clear buffer */
  set_IN_return_size (inquiryB.cmd, 96);
  set_IN_evpd(inquiryB.cmd, 0);
  set_IN_page_code(inquiryB.cmd, 0);

  do_scsi_cmd (s->sfd, inquiryB.cmd, inquiryB.size, s->buffer, 96);
}                               /* m3096g_do_inquiry */


static SANE_Status
m3096g_get_hardware_status(struct m3096g *s)
{
  SANE_Status   ret;
  DBG (10, "get_hardware_status\n");

  memset (s->buffer, '\0', 256);        /* clear buffer */
  set_HW_allocation_length (hw_statusB.cmd, 10);

  if (s->sfd < 0) 
    {
      return SANE_STATUS_INVAL;
    }
  else
    {
      ret = do_scsi_cmd (s->sfd, hw_statusB.cmd, hw_statusB.size, s->buffer, 10);
    }

  if (ret == SANE_STATUS_GOOD) 
    {
      DBG(1, "B5 %d\n", get_HW_B5_present(s->buffer));
      DBG(1, "A4 %d \n", get_HW_A4_present(s->buffer));
      DBG(1, "B4 %d \n", get_HW_B4_present(s->buffer));
      DBG(1, "A3 %d \n", get_HW_A3_present(s->buffer));
      DBG(1, "HE %d\n", get_HW_adf_empty(s->buffer));
      DBG(1, "OMR %d\n", get_HW_omr(s->buffer));
      DBG(1, "ADFC %d\n", get_HW_adfc_open(s->buffer));
      DBG(1, "SLEEP %d\n", get_HW_sleep(s->buffer));
      DBG(1, "MF %d\n", get_HW_manual_feed(s->buffer));
      DBG(1, "Start %d\n", get_HW_start_button(s->buffer));
      DBG(1, "Ink empty %d\n", get_HW_ink_empty(s->buffer));
      DBG(1, "DFEED %d\n", get_HW_dfeed_detected(s->buffer));
      DBG(1, "SKEW %d\n",   get_HW_skew_angle(s->buffer));
      
    }

  return ret;
}


static SANE_Status
m3096g_get_vital_product_data(struct m3096g *s) 
{
  SANE_Status   ret;

  DBG (10, "get_vital_product_data\n");


  memset (s->buffer, '\0', 256);        /* clear buffer */
  set_IN_return_size (inquiryB.cmd, 0x64);
  set_IN_evpd(inquiryB.cmd, 1);
  set_IN_page_code(inquiryB.cmd, 0xf0);

  ret = do_scsi_cmd (s->sfd, inquiryB.cmd, inquiryB.size, s->buffer, 0x64);
  if (ret == SANE_STATUS_GOOD) 
    {
      
      DBG(1, "basic x res: %d\n",get_IN_basic_x_res(s->buffer));
      DBG(1, "basic y res %d\n", get_IN_basic_y_res(s->buffer));
      DBG(1, "step x res %d\n", get_IN_step_x_res(s->buffer));
      DBG(1, "step y res %d\n", get_IN_step_y_res(s->buffer));
      DBG(1, "max x res %d\n", get_IN_max_x_res(s->buffer));
      DBG(1, "max y res %d\n", get_IN_max_y_res(s->buffer));
      DBG(1, "min x res %d\n", get_IN_min_x_res(s->buffer));
      DBG(1, "max y res %d\n", get_IN_min_y_res(s->buffer));
      DBG(1, "window width %d\n", get_IN_window_width(s->buffer));
      DBG(1, "window length %d\n", get_IN_window_length(s->buffer));
      DBG(1, "has operator panel %d\n", get_IN_operator_panel(s->buffer));
      DBG(1, "has barcode %d\n", get_IN_barcode(s->buffer));
      DBG(1, "has endorser %d\n", get_IN_endorser(s->buffer));
      DBG(1, "is duplex %d\n", get_IN_duplex(s->buffer));
      DBG(1, "has flatbed %d\n", get_IN_flatbed(s->buffer));
      DBG(1, "has adf %d\n", get_IN_adf(s->buffer));
    }

  return ret;
}


static SANE_Status
do_scsi_cmd (int fd, char *cmd, int cmd_len, char *out, size_t out_len)
{
  SANE_Status ret;
  size_t ol = out_len;

  hexdump (20, "<cmd<", cmd, cmd_len);

  ret = sanei_scsi_cmd (fd, cmd, cmd_len, out, &ol);
  if ((out_len != 0) && (out_len != ol))
    {
      DBG (1, "sanei_scsi_cmd: asked %lu bytes, got %lu\n",
           (u_long) out_len, (u_long) ol);
    }
  if (ret != SANE_STATUS_GOOD)
    {
      DBG (1, "sanei_scsi_cmd: returning 0x%08x\n", ret);
    }
  DBG (10, "sanei_scsi_cmd: returning %lu bytes:\n", (u_long) ol);
  if (out != NULL && out_len != 0)
    hexdump (15, ">rslt>", out, (out_len > 0x60) ? 0x60 : out_len);

  return ret;
}                               /* do_scsi_cmd */

static void
hexdump (int level, char *comment, unsigned char *p, int l)
{
  int i;
  char line[128];
  char *ptr;

  DBG (level, "%s\n", comment);
  ptr = line;
  for (i = 0; i < l; i++, p++)
    {
      if ((i % 16) == 0)
        {
          if (ptr != line)
            {
              *ptr = '\0';
              DBG (level, "%s\n", line);
              ptr = line;
            }
          sprintf (ptr, "%3.3d:", i);
          ptr += 4;
        }
      sprintf (ptr, " %2.2x", *p);
      ptr += 3;
    }
  *ptr = '\0';
  DBG (level, "%s\n", line);
}                               /* hexdump */

static SANE_Status
init_options (struct m3096g *scanner)
{
  int i;

  DBG (10, "init_options\n");

  memset (scanner->opt, 0, sizeof (scanner->opt));

  for (i = 0; i < NUM_OPTIONS; ++i)
    {
      scanner->opt[i].name = "filler";
      scanner->opt[i].size = sizeof (SANE_Word);
      scanner->opt[i].cap = SANE_CAP_INACTIVE;
    }

  scanner->opt[OPT_NUM_OPTS].title = SANE_TITLE_NUM_OPTIONS;
  scanner->opt[OPT_NUM_OPTS].desc = SANE_DESC_NUM_OPTIONS;
  scanner->opt[OPT_NUM_OPTS].cap = SANE_CAP_SOFT_DETECT;

/************** "Mode" group: **************/
  scanner->opt[OPT_MODE_GROUP].title = "Scan Mode";
  scanner->opt[OPT_MODE_GROUP].desc = "";
  scanner->opt[OPT_MODE_GROUP].type = SANE_TYPE_GROUP;
  scanner->opt[OPT_MODE_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* source */
  scanner->opt[OPT_SOURCE].name = SANE_NAME_SCAN_SOURCE;
  scanner->opt[OPT_SOURCE].title = SANE_TITLE_SCAN_SOURCE;
  scanner->opt[OPT_SOURCE].desc = SANE_DESC_SCAN_SOURCE;
  scanner->opt[OPT_SOURCE].type = SANE_TYPE_STRING;
  scanner->opt[OPT_SOURCE].size = max_string_size (source_list);
  scanner->opt[OPT_SOURCE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  scanner->opt[OPT_SOURCE].constraint.string_list = source_list;
  if (scanner->autofeeder && scanner->flatbed)
    {
      scanner->opt[OPT_SOURCE].cap = SANE_CAP_SOFT_SELECT
        | SANE_CAP_SOFT_DETECT;
    }

  /* scan mode */
  scanner->opt[OPT_MODE].name = SANE_NAME_SCAN_MODE;
  scanner->opt[OPT_MODE].title = SANE_TITLE_SCAN_MODE;
  scanner->opt[OPT_MODE].desc = SANE_DESC_SCAN_MODE;
  scanner->opt[OPT_MODE].type = SANE_TYPE_STRING;
  scanner->opt[OPT_MODE].size = max_string_size (scan_mode_list);
  scanner->opt[OPT_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  scanner->opt[OPT_MODE].constraint.string_list = scan_mode_list;
  scanner->opt[OPT_MODE].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;

  /* negative */
  scanner->opt[OPT_TYPE].name = "type";
  scanner->opt[OPT_TYPE].title = "Film type";
  scanner->opt[OPT_TYPE].desc = "positive or negative image";
  scanner->opt[OPT_TYPE].type = SANE_TYPE_STRING;
  scanner->opt[OPT_TYPE].size = max_string_size (type_list);
  scanner->opt[OPT_TYPE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  scanner->opt[OPT_TYPE].constraint.string_list = type_list;

  scanner->opt[OPT_PRESCAN].name = "prescan";
  scanner->opt[OPT_PRESCAN].title = "Prescan";
  scanner->opt[OPT_PRESCAN].desc = "Perform a prescan during preview";
  scanner->opt[OPT_PRESCAN].type = SANE_TYPE_BOOL;
  scanner->opt[OPT_PRESCAN].unit = SANE_UNIT_NONE;

  /* resolution */
  scanner->opt[OPT_X_RES].name = SANE_NAME_SCAN_X_RESOLUTION;
  scanner->opt[OPT_X_RES].title = SANE_TITLE_SCAN_X_RESOLUTION;
  scanner->opt[OPT_X_RES].desc = SANE_DESC_SCAN_X_RESOLUTION;
  scanner->opt[OPT_X_RES].type = SANE_TYPE_INT;
  scanner->opt[OPT_X_RES].unit = SANE_UNIT_DPI;
  scanner->opt[OPT_X_RES].constraint_type = SANE_CONSTRAINT_WORD_LIST;
  scanner->opt[OPT_X_RES].constraint.word_list = scanner->x_res_list;
  scanner->opt[OPT_X_RES].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;

  scanner->opt[OPT_Y_RES].name = SANE_NAME_SCAN_Y_RESOLUTION;
  scanner->opt[OPT_Y_RES].title = SANE_TITLE_SCAN_Y_RESOLUTION;
  scanner->opt[OPT_Y_RES].desc = SANE_DESC_SCAN_Y_RESOLUTION;
  scanner->opt[OPT_Y_RES].type = SANE_TYPE_INT;
  scanner->opt[OPT_Y_RES].unit = SANE_UNIT_DPI;
  scanner->opt[OPT_Y_RES].constraint_type = SANE_CONSTRAINT_WORD_LIST;
  scanner->opt[OPT_Y_RES].constraint.word_list = scanner->y_res_list;
  scanner->opt[OPT_Y_RES].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;


#ifdef no_preview_res
  scanner->opt[OPT_PREVIEW_RES].name = "preview-resolution";
  scanner->opt[OPT_PREVIEW_RES].title = "Preview resolution";
  scanner->opt[OPT_PREVIEW_RES].desc = SANE_DESC_SCAN_RESOLUTION;
  scanner->opt[OPT_PREVIEW_RES].type = SANE_TYPE_INT;
  scanner->opt[OPT_PREVIEW_RES].unit = SANE_UNIT_DPI;
  scanner->opt[OPT_PREVIEW_RES].constraint_type = SANE_CONSTRAINT_WORD_LIST;
  scanner->opt[OPT_PREVIEW_RES].constraint.word_list = resolution_list;
#endif

/************** "Geometry" group: **************/
  scanner->opt[OPT_GEOMETRY_GROUP].title = "Geometry";
  scanner->opt[OPT_GEOMETRY_GROUP].desc = "";
  scanner->opt[OPT_GEOMETRY_GROUP].type = SANE_TYPE_GROUP;
  scanner->opt[OPT_GEOMETRY_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* top-left x */
  scanner->opt[OPT_TL_X].name = SANE_NAME_SCAN_TL_X;
  scanner->opt[OPT_TL_X].title = SANE_TITLE_SCAN_TL_X;
  scanner->opt[OPT_TL_X].desc = SANE_DESC_SCAN_TL_X;
  scanner->opt[OPT_TL_X].type = SANE_TYPE_FIXED;
  scanner->opt[OPT_TL_X].unit = SANE_UNIT_MM;
  scanner->opt[OPT_TL_X].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_TL_X].constraint.range = &scanner->x_range;
  scanner->opt[OPT_TL_X].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;

  /* top-left y */
  scanner->opt[OPT_TL_Y].name = SANE_NAME_SCAN_TL_Y;
  scanner->opt[OPT_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
  scanner->opt[OPT_TL_Y].desc = SANE_DESC_SCAN_TL_Y;
  scanner->opt[OPT_TL_Y].type = SANE_TYPE_FIXED;
  scanner->opt[OPT_TL_Y].unit = SANE_UNIT_MM;
  scanner->opt[OPT_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_TL_Y].constraint.range = &scanner->y_range;
  scanner->opt[OPT_TL_Y].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;

  /* bottom-right x */
  scanner->opt[OPT_BR_X].name = SANE_NAME_SCAN_BR_X;
  scanner->opt[OPT_BR_X].title = SANE_TITLE_SCAN_BR_X;
  scanner->opt[OPT_BR_X].desc = SANE_DESC_SCAN_BR_X;
  scanner->opt[OPT_BR_X].type = SANE_TYPE_FIXED;
  scanner->opt[OPT_BR_X].unit = SANE_UNIT_MM;
  scanner->opt[OPT_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_BR_X].constraint.range = &scanner->x_range;
  scanner->opt[OPT_BR_X].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;

  /* bottom-right y */
  scanner->opt[OPT_BR_Y].name = SANE_NAME_SCAN_BR_Y;
  scanner->opt[OPT_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
  scanner->opt[OPT_BR_Y].desc = SANE_DESC_SCAN_BR_Y;
  scanner->opt[OPT_BR_Y].type = SANE_TYPE_FIXED;
  scanner->opt[OPT_BR_Y].unit = SANE_UNIT_MM;
  scanner->opt[OPT_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_BR_Y].constraint.range = &scanner->y_range;
  scanner->opt[OPT_BR_Y].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;


  /* ------------------------------ */

/************** "Enhancement" group: **************/
  scanner->opt[OPT_ENHANCEMENT_GROUP].title = "Enhancement";
  scanner->opt[OPT_ENHANCEMENT_GROUP].desc = "";
  scanner->opt[OPT_ENHANCEMENT_GROUP].type = SANE_TYPE_GROUP;
  scanner->opt[OPT_ENHANCEMENT_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  scanner->opt[OPT_AVERAGING].name = "averaging";
  scanner->opt[OPT_AVERAGING].title = "Averaging";
  scanner->opt[OPT_AVERAGING].desc = "Averaging";
  scanner->opt[OPT_AVERAGING].type = SANE_TYPE_BOOL;
  scanner->opt[OPT_AVERAGING].unit = SANE_UNIT_NONE;

  scanner->opt[OPT_BRIGHTNESS].name = SANE_NAME_BRIGHTNESS;
  scanner->opt[OPT_BRIGHTNESS].title = SANE_TITLE_BRIGHTNESS;
  scanner->opt[OPT_BRIGHTNESS].desc = SANE_DESC_BRIGHTNESS;
  scanner->opt[OPT_BRIGHTNESS].type = SANE_TYPE_INT;
  scanner->opt[OPT_BRIGHTNESS].unit = SANE_UNIT_NONE;
  scanner->opt[OPT_BRIGHTNESS].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_BRIGHTNESS].constraint.range = &brightness_range;
  scanner->opt[OPT_BRIGHTNESS].cap = SANE_CAP_SOFT_DETECT;

  scanner->opt[OPT_CONTRAST].name = SANE_NAME_CONTRAST;
  scanner->opt[OPT_CONTRAST].title = SANE_TITLE_CONTRAST;
  scanner->opt[OPT_CONTRAST].desc = SANE_DESC_CONTRAST;
  scanner->opt[OPT_CONTRAST].type = SANE_TYPE_INT;
  scanner->opt[OPT_CONTRAST].unit = SANE_UNIT_NONE;
  scanner->opt[OPT_CONTRAST].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_CONTRAST].constraint.range = &threshold_range;
  scanner->opt[OPT_CONTRAST].cap = SANE_CAP_SOFT_DETECT;

  scanner->opt[OPT_THRESHOLD].name = SANE_NAME_THRESHOLD;
  scanner->opt[OPT_THRESHOLD].title = SANE_TITLE_THRESHOLD;
  scanner->opt[OPT_THRESHOLD].desc = SANE_DESC_THRESHOLD;
  scanner->opt[OPT_THRESHOLD].type = SANE_TYPE_INT;
  scanner->opt[OPT_THRESHOLD].unit = SANE_UNIT_NONE;
  scanner->opt[OPT_THRESHOLD].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_THRESHOLD].constraint.range = &contrast_range;
  scanner->opt[OPT_THRESHOLD].cap = SANE_CAP_SOFT_DETECT;

  scanner->opt[OPT_MIRROR_IMAGE].name  = "mirroring";
  scanner->opt[OPT_MIRROR_IMAGE].title = "mirror image";
  scanner->opt[OPT_MIRROR_IMAGE].desc = "mirror image";
  scanner->opt[OPT_MIRROR_IMAGE].type = SANE_TYPE_BOOL;
  scanner->opt[OPT_MIRROR_IMAGE].unit = SANE_UNIT_NONE;
  if (scanner->has_mirroring) 
    scanner->opt[OPT_MIRROR_IMAGE].cap  = SANE_CAP_SOFT_SELECT |
      SANE_CAP_SOFT_DETECT;


  scanner->opt[OPT_RIF].name  = "rif";
  scanner->opt[OPT_RIF].title = "rif";
  scanner->opt[OPT_RIF].desc = "reverse image format";
  scanner->opt[OPT_RIF].type = SANE_TYPE_BOOL;
  scanner->opt[OPT_RIF].unit = SANE_UNIT_NONE;
  if (scanner->ipc) 
    scanner->opt[OPT_RIF].cap  = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;

  scanner->opt[OPT_AUTOSEP].name  = "autoseparation";
  scanner->opt[OPT_AUTOSEP].title = "automatic separation";
  scanner->opt[OPT_AUTOSEP].desc = "automatic separation";
  scanner->opt[OPT_AUTOSEP].type = SANE_TYPE_BOOL;
  scanner->opt[OPT_AUTOSEP].unit = SANE_UNIT_NONE;
  if (scanner->has_autosep) 
    scanner->opt[OPT_AUTOSEP].cap  = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;

  scanner->opt[OPT_OUTLINE_EXTRACTION].name  = "outline";
  scanner->opt[OPT_OUTLINE_EXTRACTION].title = "outline extraction";
  scanner->opt[OPT_OUTLINE_EXTRACTION].desc = "enable outline extraction";
  scanner->opt[OPT_OUTLINE_EXTRACTION].type = SANE_TYPE_BOOL;
  scanner->opt[OPT_OUTLINE_EXTRACTION].unit = SANE_UNIT_NONE;
  if (scanner->has_outline) 
    scanner->opt[OPT_OUTLINE_EXTRACTION].cap  = SANE_CAP_SOFT_SELECT |
      SANE_CAP_SOFT_DETECT;

  scanner->opt[OPT_COMPRESSION].name  = "compression";
  scanner->opt[OPT_COMPRESSION].title = "compress image";
  scanner->opt[OPT_COMPRESSION].desc = "use hardware compression of scanner";
  scanner->opt[OPT_COMPRESSION].type = SANE_TYPE_STRING;
  scanner->opt[OPT_COMPRESSION].size = max_string_size (compression_mode_list);
  scanner->opt[OPT_COMPRESSION].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  scanner->opt[OPT_COMPRESSION].constraint.string_list = compression_mode_list;
  if (scanner->cmp2) 
    scanner->opt[OPT_COMPRESSION].cap = SANE_CAP_SOFT_SELECT | 
      SANE_CAP_SOFT_DETECT;

  scanner->opt[OPT_GAMMA].name  = "gamma";
  scanner->opt[OPT_GAMMA].title = "gamma";
  scanner->opt[OPT_GAMMA].desc = "specifies the gamma pattern number for the line art or the halftone";
  scanner->opt[OPT_GAMMA].type = SANE_TYPE_STRING;
  scanner->opt[OPT_GAMMA].size = max_string_size (scan_mode_list);
  scanner->opt[OPT_GAMMA].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  scanner->opt[OPT_GAMMA].constraint.string_list = gamma_mode_list;
  scanner->opt[OPT_GAMMA].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    
  scanner->opt[OPT_EMPHASIS].name  = "emphasis";
  scanner->opt[OPT_EMPHASIS].title = "emphasis";
  scanner->opt[OPT_EMPHASIS].desc = "image emphasis";
  scanner->opt[OPT_EMPHASIS].type = SANE_TYPE_STRING;
  scanner->opt[OPT_EMPHASIS].size = max_string_size (scan_mode_list);
  scanner->opt[OPT_EMPHASIS].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  scanner->opt[OPT_EMPHASIS].constraint.string_list = emphasis_mode_list;
  if (scanner->has_emphasis) 
    scanner->opt[OPT_EMPHASIS].cap  = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;


  scanner->opt[OPT_VARIANCE_RATE].name = "variance_rate";
  scanner->opt[OPT_VARIANCE_RATE].title = "variance rate";
  scanner->opt[OPT_VARIANCE_RATE].desc = "variance rate for simplified dynamic threshold";
  scanner->opt[OPT_VARIANCE_RATE].type = SANE_TYPE_INT;
  scanner->opt[OPT_VARIANCE_RATE].unit = SANE_UNIT_NONE;
  scanner->opt[OPT_VARIANCE_RATE].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_VARIANCE_RATE].constraint.range = &variance_rate_range;
  scanner->opt[OPT_VARIANCE_RATE].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;

  scanner->opt[OPT_THRESHOLD_CURVE].name = "threshold_curve";
  scanner->opt[OPT_THRESHOLD_CURVE].title = "threshold curve";
  scanner->opt[OPT_THRESHOLD_CURVE].desc = "DTC threshold_curve";
  scanner->opt[OPT_THRESHOLD_CURVE].type = SANE_TYPE_INT;
  scanner->opt[OPT_THRESHOLD_CURVE].unit = SANE_UNIT_NONE;
  scanner->opt[OPT_THRESHOLD_CURVE].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_THRESHOLD_CURVE].constraint.range = &threshold_curve_range;
  if (scanner->ipc) 
    scanner->opt[OPT_THRESHOLD_CURVE].cap = SANE_CAP_SOFT_SELECT | 
      SANE_CAP_SOFT_DETECT;

  scanner->opt[OPT_GRADIATION].name  = "gradiation";
  scanner->opt[OPT_GRADIATION].title = "gradiation";
  scanner->opt[OPT_GRADIATION].desc = "gradiation";
  scanner->opt[OPT_GRADIATION].type = SANE_TYPE_STRING;
  scanner->opt[OPT_GRADIATION].size = max_string_size (gradiation_mode_list);
  scanner->opt[OPT_GRADIATION].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  scanner->opt[OPT_GRADIATION].constraint.string_list = gradiation_mode_list;
  scanner->opt[OPT_GRADIATION].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;

  scanner->opt[OPT_SMOOTHING_MODE].name  = "smoothing_mode";
  scanner->opt[OPT_SMOOTHING_MODE].title = "smoothing mode";
  scanner->opt[OPT_SMOOTHING_MODE].desc = "smoothing mode";
  scanner->opt[OPT_SMOOTHING_MODE].type = SANE_TYPE_STRING;
  scanner->opt[OPT_SMOOTHING_MODE].size = max_string_size (smoothing_mode_list);
  scanner->opt[OPT_SMOOTHING_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  scanner->opt[OPT_SMOOTHING_MODE].constraint.string_list = smoothing_mode_list;
  if (scanner->ipc) 
    scanner->opt[OPT_SMOOTHING_MODE].cap = SANE_CAP_SOFT_SELECT | 
      SANE_CAP_SOFT_DETECT;

  scanner->opt[OPT_FILTERING].name  = "filtering";
  scanner->opt[OPT_FILTERING].title = "filtering";
  scanner->opt[OPT_FILTERING].desc = "filtering";
  scanner->opt[OPT_FILTERING].type = SANE_TYPE_STRING;
  scanner->opt[OPT_FILTERING].size = max_string_size (filtering_mode_list);
  scanner->opt[OPT_FILTERING].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  scanner->opt[OPT_FILTERING].constraint.string_list = filtering_mode_list;
  if (scanner->ipc) 
    scanner->opt[OPT_FILTERING].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;

  scanner->opt[OPT_BACKGROUND].name  = "background";
  scanner->opt[OPT_BACKGROUND].title = "background";
  scanner->opt[OPT_BACKGROUND].desc = "output if gradiation of the video data is equal to or larger than threshold";
  scanner->opt[OPT_BACKGROUND].type = SANE_TYPE_STRING;
  scanner->opt[OPT_BACKGROUND].size = max_string_size (background_mode_list);
  scanner->opt[OPT_BACKGROUND].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  scanner->opt[OPT_BACKGROUND].constraint.string_list = background_mode_list;
  if (scanner->ipc) 
    scanner->opt[OPT_BACKGROUND].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;


  scanner->opt[OPT_NOISE_REMOVAL].name  = "noise_removal";
  scanner->opt[OPT_NOISE_REMOVAL].title = "noise removal";
  scanner->opt[OPT_NOISE_REMOVAL].desc = "enables the noise removal matrixes";
  scanner->opt[OPT_NOISE_REMOVAL].type = SANE_TYPE_BOOL;
  scanner->opt[OPT_NOISE_REMOVAL].unit = SANE_UNIT_NONE;
  if (scanner->ipc) 
    scanner->opt[OPT_NOISE_REMOVAL].cap  = SANE_CAP_SOFT_SELECT | 
      SANE_CAP_SOFT_DETECT;

  scanner->opt[OPT_MATRIX2X2].name  = "matrix_2x2";
  scanner->opt[OPT_MATRIX2X2].title = "2x2 noise removal matrix";
  scanner->opt[OPT_MATRIX2X2].desc = "2x2 noise removal matrix";
  scanner->opt[OPT_MATRIX2X2].type = SANE_TYPE_BOOL;
  scanner->opt[OPT_MATRIX2X2].unit = SANE_UNIT_NONE;
  if (scanner->ipc) 
    scanner->opt[OPT_MATRIX2X2].cap  = SANE_CAP_SOFT_SELECT | 
      SANE_CAP_SOFT_DETECT;

  scanner->opt[OPT_MATRIX3X3].name  = "matrix_3x3";
  scanner->opt[OPT_MATRIX3X3].title = "3x3 noise removal matrix";
  scanner->opt[OPT_MATRIX3X3].desc = "3x3 noise removal matrix";
  scanner->opt[OPT_MATRIX3X3].type = SANE_TYPE_BOOL;
  scanner->opt[OPT_MATRIX3X3].unit = SANE_UNIT_NONE;
  if (scanner->ipc) 
    scanner->opt[OPT_MATRIX3X3].cap  = SANE_CAP_SOFT_SELECT | 
      SANE_CAP_SOFT_DETECT;

  scanner->opt[OPT_MATRIX4X4].name  = "matrix_4x4";
  scanner->opt[OPT_MATRIX4X4].title = "4x4 noise removal matrix";
  scanner->opt[OPT_MATRIX4X4].desc = "4x4 noise removal matrix";
  scanner->opt[OPT_MATRIX4X4].type = SANE_TYPE_BOOL;
  scanner->opt[OPT_MATRIX4X4].unit = SANE_UNIT_NONE;
  if (scanner->ipc) 
    scanner->opt[OPT_MATRIX4X4].cap  = SANE_CAP_SOFT_SELECT | 
      SANE_CAP_SOFT_DETECT;

  scanner->opt[OPT_MATRIX5X5].name  = "matrix_5x5";
  scanner->opt[OPT_MATRIX5X5].title = "5x5 noise removal matrix";
  scanner->opt[OPT_MATRIX5X5].desc = "5x5 noise removal matrix";
  scanner->opt[OPT_MATRIX5X5].type = SANE_TYPE_BOOL;
  scanner->opt[OPT_MATRIX5X5].unit = SANE_UNIT_NONE;
  if (scanner->ipc) 
    scanner->opt[OPT_MATRIX5X5].cap  = SANE_CAP_SOFT_SELECT | 
      SANE_CAP_SOFT_DETECT;

  scanner->opt[OPT_WHITE_LEVEL_FOLLOW].name  = "white_level_follow";
  scanner->opt[OPT_WHITE_LEVEL_FOLLOW].title = "white level follow";
  scanner->opt[OPT_WHITE_LEVEL_FOLLOW].desc = "white level follow";
  scanner->opt[OPT_WHITE_LEVEL_FOLLOW].type = SANE_TYPE_STRING;
  scanner->opt[OPT_WHITE_LEVEL_FOLLOW].size = 
    max_string_size (white_level_follow_mode_list);
  scanner->opt[OPT_WHITE_LEVEL_FOLLOW].constraint_type = 
    SANE_CONSTRAINT_STRING_LIST;
  scanner->opt[OPT_WHITE_LEVEL_FOLLOW].constraint.string_list = 
    white_level_follow_mode_list;
  if (scanner->has_white_level_follow)
    scanner->opt[OPT_WHITE_LEVEL_FOLLOW].cap = SANE_CAP_SOFT_SELECT | 
      SANE_CAP_SOFT_DETECT;

  scanner->opt[OPT_DTC_SELECTION].name  = "dtc_selection";
  scanner->opt[OPT_DTC_SELECTION].title = "DTC selection";
  scanner->opt[OPT_DTC_SELECTION].desc = "DTC selection";
  scanner->opt[OPT_DTC_SELECTION].type = SANE_TYPE_STRING;
  scanner->opt[OPT_DTC_SELECTION].size = max_string_size (dtc_selection_mode_list);
  scanner->opt[OPT_DTC_SELECTION].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  scanner->opt[OPT_DTC_SELECTION].constraint.string_list = dtc_selection_mode_list;
  if (scanner->ipc) 
    scanner->opt[OPT_DTC_SELECTION].cap = SANE_CAP_SOFT_SELECT | 
      SANE_CAP_SOFT_DETECT;


  scanner->opt[OPT_DUPLEX].name  = "duplex";
  scanner->opt[OPT_DUPLEX].title = "duplex";
  scanner->opt[OPT_DUPLEX].desc = "Scan front and back side";
  scanner->opt[OPT_DUPLEX].type = SANE_TYPE_BOOL;
  scanner->opt[OPT_DUPLEX].unit = SANE_UNIT_NONE;
  if (scanner->is_duplex) 
    scanner->opt[OPT_DUPLEX].cap  = SANE_CAP_SOFT_SELECT |
      SANE_CAP_SOFT_DETECT;



  scanner->opt[OPT_PAPER_SIZE].name  = "paper_size";
  scanner->opt[OPT_PAPER_SIZE].title = "paper size";
  scanner->opt[OPT_PAPER_SIZE].desc = "Physical size of the paper in the ADF";
  scanner->opt[OPT_PAPER_SIZE].type = SANE_TYPE_STRING;
  scanner->opt[OPT_PAPER_SIZE].size = max_string_size (paper_size_mode_list);
  scanner->opt[OPT_PAPER_SIZE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  scanner->opt[OPT_PAPER_SIZE].constraint.string_list = paper_size_mode_list;
  scanner->opt[OPT_PAPER_SIZE].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;

  scanner->opt[OPT_PAPER_ORIENTATION].name  = "orientation";
  scanner->opt[OPT_PAPER_ORIENTATION].title = "orientation";
  scanner->opt[OPT_PAPER_ORIENTATION].desc = "Orientation of the paper in the ADF";
  scanner->opt[OPT_PAPER_ORIENTATION].type = SANE_TYPE_STRING;
  scanner->opt[OPT_PAPER_ORIENTATION].size = max_string_size (paper_orientation_mode_list);
  scanner->opt[OPT_PAPER_ORIENTATION].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  scanner->opt[OPT_PAPER_ORIENTATION].constraint.string_list = paper_orientation_mode_list;
  scanner->opt[OPT_PAPER_ORIENTATION].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;

  scanner->opt[OPT_PAPER_WIDTH].name = "paper_width";
  scanner->opt[OPT_PAPER_WIDTH].title = "paper width";
  scanner->opt[OPT_PAPER_WIDTH].desc = "Physical width of the paper in the ADF";
  scanner->opt[OPT_PAPER_WIDTH].type = SANE_TYPE_FIXED;
  scanner->opt[OPT_PAPER_WIDTH].unit = SANE_UNIT_MM;
  scanner->opt[OPT_PAPER_WIDTH].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_PAPER_WIDTH].constraint.range = &scanner->adf_width_range;
  scanner->opt[OPT_PAPER_WIDTH].cap = SANE_CAP_INACTIVE;

  scanner->opt[OPT_PAPER_HEIGHT].name = "paper_height";
  scanner->opt[OPT_PAPER_HEIGHT].title = "paper height";
  scanner->opt[OPT_PAPER_HEIGHT].desc = "Physical height of the paper in the ADF";
  scanner->opt[OPT_PAPER_HEIGHT].type = SANE_TYPE_FIXED;
  scanner->opt[OPT_PAPER_HEIGHT].unit = SANE_UNIT_MM;
  scanner->opt[OPT_PAPER_HEIGHT].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_PAPER_HEIGHT].constraint.range = &scanner->adf_height_range;
  scanner->opt[OPT_PAPER_HEIGHT].cap = SANE_CAP_INACTIVE;

  /* ------------------------------ */

  if (scanner->has_hw_status)
    {
      scanner->opt[OPT_START_BUTTON].name  = "start_button";
      scanner->opt[OPT_START_BUTTON].title = "start button";
      scanner->opt[OPT_START_BUTTON].desc = "is the start button of the scanner pressed";
      scanner->opt[OPT_START_BUTTON].type = SANE_TYPE_BOOL;
      scanner->opt[OPT_START_BUTTON].unit = SANE_UNIT_NONE;
      scanner->opt[OPT_START_BUTTON].cap = SANE_CAP_SOFT_DETECT;
    }


/************** "Advanced" group: **************/
  scanner->opt[OPT_ADVANCED_GROUP].title = "Advanced";
  scanner->opt[OPT_ADVANCED_GROUP].desc = "";
  scanner->opt[OPT_ADVANCED_GROUP].type = SANE_TYPE_GROUP;
  scanner->opt[OPT_ADVANCED_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* preview */
  scanner->opt[OPT_PREVIEW].name = SANE_NAME_PREVIEW;
  scanner->opt[OPT_PREVIEW].title = SANE_TITLE_PREVIEW;
  scanner->opt[OPT_PREVIEW].desc = SANE_DESC_PREVIEW;
  scanner->opt[OPT_PREVIEW].type = SANE_TYPE_BOOL;

  DBG (10, "init_options:ok\n");
  return SANE_STATUS_GOOD;
}                               /* init_options */

static int
m3096g_check_values (struct m3096g *s)
{
  if (s->use_adf == SANE_TRUE && s->autofeeder == 0)
    {
      DBG (1, "m3096g_check_values: %s\n",
           "ERROR: ADF-MODE NOT SUPPORTED BY SCANNER, ABORTING");
      return (1);
    }
  return (0);
}                               /* m3096g_check_values */

/* m3096g_free_scanner should go through the following sequence:
 * OBJECT POSITION DISCHARGE
 *     GOOD
 * RELEASE UNIT
 *     GOOD
 */
static int
m3096g_free_scanner (struct m3096g *s)
{
  int ret;
  DBG (10, "m3096g_free_scanner\n");
  ret = m3096g_object_discharge (s);
  if (ret)
    return ret;

  wait_scanner (s);

  ret = do_scsi_cmd (s->sfd, release_unitB.cmd, release_unitB.size, NULL, 0);
  if (ret)
    return ret;

  DBG (10, "m3096g_free_scanner: ok\n");
  return ret;
}                               /* m3096g_free_scanner */

/* m3096g_grab_scanner should go through the following command sequence:
 * TEST UNIT READY
 *     CHECK CONDITION  \
 * REQUEST SENSE         > These should be handled automagically by
 *     UNIT ATTENTION   /  the kernel if they happen (powerup/reset)
 * TEST UNIT READY
 *     GOOD
 * RESERVE UNIT
 *     GOOD
 * 
 * It is then responsible for installing appropriate signal handlers
 * to call emergency_give_scanner() if user aborts.
 */

static int
m3096g_grab_scanner (struct m3096g *s)
{
  int ret;

  DBG (10, "m3096g_grab_scanner\n");
  wait_scanner (s);

  ret = do_scsi_cmd (s->sfd, reserve_unitB.cmd, reserve_unitB.size, NULL, 0);
  if (ret)
    return ret;

  DBG (10, "m3096g_grab_scanner: ok\n");
  return 0;
}                               /* m3096g_grab_scanner */

/* 
 *  wait_scanner spins until TEST_UNIT_READY returns 0 (GOOD)
 *  returns 0 on success,
 *  returns -1 on error or timeout
 */
static int
wait_scanner (struct m3096g *s)
{
  int ret = -1;
  int cnt = 0;

  DBG (10, "wait_scanner\n");

  while (ret != 0)
    {
      ret = do_scsi_cmd (s->sfd, test_unit_readyB.cmd,
                         test_unit_readyB.size, 0, 0);
      if (ret == SANE_STATUS_DEVICE_BUSY)
        {
          usleep (500000);      /* wait 0.5 seconds */
          /* 20 sec. max (prescan takes up to 15 sec. */
          if (cnt++ > 40)
            {
              DBG (1, "wait_scanner: scanner does NOT get ready\n");
              return -1;
            }
        }
      else if (ret == SANE_STATUS_GOOD)
        {
          DBG (10, "wait_scanner: ok\n");
          return ret;
        }
      else
        {
          DBG (1, "wait_scanner: unit ready failed (%s)\n",
               sane_strstatus (ret));
        }
    }
  DBG (10, "wait_scanner: ok\n");
  return 0;
}                               /* wait_scanner */

static int
m3096g_object_position (struct m3096g *s)
{
  int ret;
  DBG (10, "m3096g_object_position\n");
  if (s->use_adf != SANE_TRUE)
    {
      return SANE_STATUS_GOOD;
    }
  if (s->autofeeder == 0)
    {
      DBG (10, "m3096g_object_position: Autofeeder not present.\n");
      return SANE_STATUS_UNSUPPORTED;
    }
  memcpy (s->buffer, object_positionB.cmd, object_positionB.size);
  set_OP_autofeed (s->buffer, OP_Feed);
  ret = do_scsi_cmd (s->sfd, s->buffer,
                     object_positionB.size, NULL, 0);
  if (ret != SANE_STATUS_GOOD)
    {
      return ret;
    }
  wait_scanner (s);
  DBG (10, "m3096g_object_position: ok\n");
  return ret;
}                               /* m3096g_object_position */



static SANE_Status
do_cancel (struct m3096g *scanner)
{
  DBG (10, "do_cancel\n");
  swap_res (scanner);
  scanner->scanning = SANE_FALSE;

  do_eof (scanner);             /* close pipe and reposition scanner */

  if (scanner->reader_pid > 0)
    {
      int exit_status;
      DBG (10, "do_cancel: kill reader_process\n");
      /* ensure child knows it's time to stop: */
      kill (scanner->reader_pid, SIGTERM);
      while (wait (&exit_status) != scanner->reader_pid)
        DBG (50, "wait for scanner to stop\n");
      ;
      scanner->reader_pid = 0;
    }

  if (scanner->sfd >= 0)
    {
      m3096g_free_scanner (scanner);
      DBG (10, "do_cancel: close filedescriptor\n");
      sanei_scsi_close (scanner->sfd);
      scanner->sfd = -1;
    }

  return SANE_STATUS_CANCELLED;
}                               /* do_cancel */

static void
swap_res (struct m3096g *s)
{                               /* for the time being, do nothing */
  DBG (99, "%d\n", s);
}                               /* swap_res */

static int
m3096g_object_discharge (struct m3096g *s)
{
  int ret;

  DBG (10, "m3096g_object_discharge\n");
  if (s->use_adf != SANE_TRUE)
    {
      return SANE_STATUS_GOOD;
    }

  memcpy (s->buffer, object_positionB.cmd, object_positionB.size);
  set_OP_autofeed (s->buffer, OP_Discharge);
  ret = do_scsi_cmd (s->sfd, s->buffer,
                     object_positionB.size, NULL, 0);
  wait_scanner (s);
  DBG (10, "m3096g_object_discharge: ok\n");
  return ret;
}                               /* m3096g_object_discharge */

static int
m3096g_set_window_param (struct m3096g *s, int prescan)
{
  unsigned char buffer_r[max_WDB_size];
  unsigned char *cp_buffer;
  int width, length, pixels;
  int ret;
  int b_finished, b_front;
  int i_cmd_len, i_xfer_len;

  wait_scanner (s);
  DBG (10, "set_window_param\n");
  DBG (99, "%d\n", prescan); /* avoid compiler warning */

  memset (buffer_r, '\0', max_WDB_size);        /* clear buffer */
  memcpy (buffer_r, window_descriptor_blockB.cmd,
          window_descriptor_blockB.size);       /* copy preset data */

  b_front = 1;
  cp_buffer = buffer_r;

  do 
    {

      if ( b_front ) 
        {
          set_WD_wid (cp_buffer, WD_wid_front);    /* window identifier */
        } 
      else 
        {
          set_WD_wid (cp_buffer, WD_wid_back);    /* window identifier */
        }

      set_WD_Xres (cp_buffer, s->x_res);     /* x resolution in dpi */
      set_WD_Yres (cp_buffer, s->y_res);     /* y resolution in dpi */

      set_WD_ULX (cp_buffer, s->tl_x);       /* top left x */
      set_WD_ULY (cp_buffer, s->tl_y);       /* top left y */

  width = s->br_x - s->tl_x;
      /* increase initial width until we've a full number of bytes in line */
  if (s->x_res == 0)
    {
      while (pixels = 400 * width / 1200,
             (s->bitsperpixel * pixels) % 8)
        {
          width++;
        }
    }
  else
    {
      while (pixels = s->x_res * width / 1200,
             (s->bitsperpixel * pixels) % 8)
        {
          width++;
        }
    }
  length = s->br_y - s->tl_y;

  if (13200 < width
      && width <= 14592)
    {
      if (length > 19842
          && length < (19842 + 600))
        {
          length = 19840;
        }
    }

      /* todo: constraint checking for M3093GX DG GXm DGm */

      set_WD_width (cp_buffer, width);
      set_WD_length (cp_buffer, length);

      set_WD_brightness (cp_buffer, s->brightness);
      set_WD_threshold (cp_buffer, s->threshold);
      set_WD_contrast (cp_buffer, s->contrast);
      set_WD_composition (cp_buffer, s->composition);
      set_WD_bitsperpixel (cp_buffer, s->bitsperpixel);
      set_WD_halftone (cp_buffer, s->halftone);
      set_WD_rif (cp_buffer, s->rif);
      set_WD_bitorder (cp_buffer, s->bitorder);
      set_WD_compress_type (cp_buffer, s->compress_type);
      set_WD_compress_arg (cp_buffer, s->compress_arg);
      set_WD_vendor_id_code (cp_buffer, s->vendor_id_code);
      set_WD_gamma (cp_buffer, s->gamma);
      set_WD_outline (cp_buffer, s->outline);
      set_WD_emphasis (cp_buffer, s->emphasis);
      set_WD_auto_sep (cp_buffer, s->auto_sep);
      set_WD_mirroring (cp_buffer, s->mirroring);
      set_WD_var_rate_dyn_thresh (cp_buffer, s->var_rate_dyn_thresh);
      set_WD_dtc_threshold_curve (cp_buffer, s->dtc_threshold_curve);
      set_WD_gradiation(cp_buffer, s->gradiation);
      set_WD_smoothing_mode(cp_buffer, s->smoothing_mode);
      set_WD_filtering(cp_buffer, s->filtering);
      set_WD_background(cp_buffer, s->background);
      set_WD_matrix2x2(cp_buffer, s->matrix2x2);
      set_WD_matrix3x3(cp_buffer, s->matrix3x3);
      set_WD_matrix4x4(cp_buffer, s->matrix4x4);
      set_WD_matrix5x5(cp_buffer, s->matrix5x5);
      set_WD_noise_removal(cp_buffer, s->noise_removal);
      set_WD_white_level_follow (cp_buffer, s->white_level_follow);
      set_WD_subwindow_list (cp_buffer, s->subwindow_list);

      if ( b_front ) 
	{
	  set_WD_paper_size (cp_buffer, s->paper_size);
	  set_WD_paper_orientation(cp_buffer, s->paper_orientation);
	  set_WD_paper_selection(cp_buffer, s->paper_selection);
	}
      else
	{
	  /* bytes 35 - 3D has to be 0 for back-side */
	  set_WD_paper_size (cp_buffer, WD_paper_UNDEFINED);
	  set_WD_paper_orientation(cp_buffer, WD_paper_PORTRAIT);
	  set_WD_paper_selection(cp_buffer, WD_paper_SEL_UNDEFINED);
	  set_WD_paper_width_X (cp_buffer, 0);
	  set_WD_paper_length_Y (cp_buffer, 0);
	}

      set_WD_paper_width_X (cp_buffer, s->paper_width_X);
      set_WD_paper_length_Y (cp_buffer, s->paper_length_Y);

      set_WD_dtc_selection (cp_buffer, s->dtc_selection);

  DBG (10, "\tx_res=%d, y_res=%d\n",
       s->x_res, s->y_res);
  DBG (10, "\tupper left-x=%d, upper left-y=%d\n",
       s->tl_x, s->tl_y);
  DBG (10, "\twindow width=%d, length=%d\n",
       width, s->br_y - s->tl_y);

      DBG (10, "paper_size %d\n", s->paper_size);
      DBG (10, "paper_orient %d\n", s->paper_orientation);
      DBG (10, "paper_select %d\n", s->paper_selection);

      if (s->duplex && b_front) 
        {
          /* if we are scanning duplex and just set the front side
           * switch to back side and repeat the steps above.
           */
          b_front = 0;
          b_finished = 0;
          cp_buffer = cp_buffer + used_WDB_size;
        } 
      else
        {
          b_finished = 1;
        }
    }
  while (!b_finished);


  if (s->duplex) 
    {
      s->i_num_frames = 2;
    } 
  else 
    {
      s->i_num_frames = 1;
    }

  /* prepare SCSI-BUFFER */
  memcpy (s->buffer, set_windowB.cmd, set_windowB.size);        /* SET-WINDOW cmd */
  memcpy ((s->buffer + set_windowB.size),       /* add WPDB */
          window_parameter_data_blockB.cmd,
          window_parameter_data_blockB.size);

  /* set size of one window descriptor block */
  set_WPDB_wdblen ((s->buffer + set_windowB.size), used_WDB_size);

  
  if ( s->duplex ) 
    {
      memcpy (s->buffer + set_windowB.size + window_parameter_data_blockB.size,
              buffer_r, used_WDB_size * 2);
      
      i_xfer_len = window_parameter_data_blockB.size + used_WDB_size * 2;
      set_SW_xferlen (s->buffer, i_xfer_len);
      i_cmd_len = set_windowB.size + i_xfer_len;
    } 
  else 
    {
  memcpy (s->buffer + set_windowB.size + window_parameter_data_blockB.size,
              buffer_r, used_WDB_size);

      i_xfer_len = window_parameter_data_blockB.size + used_WDB_size;
      set_SW_xferlen (s->buffer, i_xfer_len);
      i_cmd_len = set_windowB.size + i_xfer_len;
    }

  hexdump (15, "Window front", buffer_r, used_WDB_size);
  if ( s->duplex ) 
    hexdump (15, "Window back", buffer_r + used_WDB_size, used_WDB_size);


  ret = do_scsi_cmd (s->sfd, s->buffer, i_cmd_len, NULL, 0);
  if (ret)
    return ret;
  DBG (10, "set_window_param: ok\n");
  return ret;
}                               /* m3096g_set_window_param */

static size_t
max_string_size (const SANE_String_Const strings[])
{
  size_t size, max_size = 0;
  int i;

  for (i = 0; strings[i]; ++i)
    {
      size = strlen (strings[i]) + 1;
      if (size > max_size)
        max_size = size;
    }
  return max_size;
}                               /* max_string_size */

static int
m3096g_start_scan (struct m3096g *s)
{
  unsigned char   cp_buffer[2];
  int ret;
  int             i_xfer_len;

  DBG (10, "m3096g_start_scan\n");

  cp_buffer[0] = 0x00;
  cp_buffer[1] = 0x80;

  memcpy (s->buffer, scanB.cmd, scanB.size);

  if (s->duplex) 
    {
      i_xfer_len = 2;
    }
  else
    {
      i_xfer_len = 1;
    }
  
  memcpy (s->buffer + scanB.size, cp_buffer, i_xfer_len);
  set_SC_xfer_length(s->buffer, i_xfer_len);

  DBG(1, "cmd_len = %d\n",scanB.size + i_xfer_len);
  ret = do_scsi_cmd (s->sfd, s->buffer, scanB.size + i_xfer_len, NULL, 0);

  if (ret)
    return ret;
  DBG (10, "m3096g_start_scan:ok\n");

  return ret;
}                               /* m3096g_start_scan */

static void
sigterm_handler (int signal)
{

  DBG (99, "%d\n", signal); /* avoid compiler warning */

  sanei_scsi_req_flush_all ();  /* flush SCSI queue */
  _exit (SANE_STATUS_GOOD);
}                               /* sigterm_handler */

/* This function is executed as a child process. */
static int
reader_process (struct m3096g *scanner, int pipe_fd, int i_window_id)
{
  SANE_Status  status;
  unsigned int data_left;
  unsigned int data_to_read;
  FILE *fp;
  sigset_t sigterm_set;
  struct SIGACTION act;

  DBG (10, "reader_process started\n");

  sigemptyset (&sigterm_set);
  sigaddset (&sigterm_set, SIGTERM);

  fp = fdopen (pipe_fd, "w");
  if (!fp)
    {
      DBG (1, "reader_process: couldn't open pipe!\n");
      return 1;
    }

  DBG (10, "reader_process: starting to READ data\n");

  data_left = bytes_per_line (scanner) *
    lines_per_scan (scanner);

  m3096g_trim_rowbufsize (scanner);     /* trim bufsize */

  DBG (10, "reader_process: reading %u bytes in blocks of %u bytes\n",
       data_left, scanner->row_bufsize);

  memset (&act, 0, sizeof (act));

  act.sa_handler = sigterm_handler;
  sigaction (SIGTERM, &act, 0);
  /* wait_scanner(scanner); */
  do
    {
      data_to_read = (data_left < scanner->row_bufsize)
        ? data_left
        : scanner->row_bufsize;

      current_scanner = scanner;

      status = m3096g_read_data_block (scanner, data_to_read, i_window_id);

      switch (status) 
        {
        case SANE_STATUS_GOOD:

          break;
        case SANE_STATUS_EOF:

          DBG (5, "reader_process: EOM (no more data) length = %d\n", 
	       current_scanner->i_transfer_length);
          
          data_to_read = data_to_read - current_scanner->i_transfer_length;
          data_left    = data_to_read;
          
          break;
        default:
          DBG (1, "reader_process: unable to get image data from scanner!\n");
          DBG (1, "status = %s\n",sane_strstatus(status));
          fclose (fp);
          return (-1);
          break;
        }

      fwrite (scanner->buffer, 1, data_to_read, fp);
      fflush (fp);

      data_left -= data_to_read;
      DBG (10, "reader_process: buffer of %d bytes read; %d bytes to go\n",
           data_to_read, data_left);
    }
  while (data_left);

  fclose (fp);

  DBG (10, "reader_process: finished\n");

  return 0;
}                               /* reader_process */

static SANE_Status
do_eof (struct m3096g *scanner)
{
  DBG (10, "do_eof\n");

  if (scanner->pipe >= 0)
    {
      close (scanner->pipe);
      scanner->pipe = -1;
    }
  return SANE_STATUS_EOF;
}                               /* do_eof */

static int
pixels_per_line (struct m3096g *s)
{
  int dots;
  if (s->x_res == 0)
    {
      dots = 400 * (s->br_x - s->tl_x) / 1200;
    }
  else
    {
      dots = s->x_res * (s->br_x - s->tl_x) / 1200;
    }

  if (s->compress_type != 0) 
    {
      dots = ((dots * s->bitsperpixel + 7) / 8) * 8;
    }

  return dots;
}                               /* pixels_per_line */

static int
lines_per_scan (struct m3096g *s)
{
  int lines;
  lines = s->y_res * (s->br_y - s->tl_y) / 1200;
  return lines;
}                               /* lines_per_scan */

static int
bytes_per_line (struct m3096g *s)
{
  return (pixels_per_line (s) * s->bitsperpixel + 7) / 8;
}                               /* bytes_per_line */

static void
m3096g_trim_rowbufsize (struct m3096g *s)
{
  unsigned int row_len;
  row_len = (unsigned int) bytes_per_line (s);
  if (s->row_bufsize >= row_len)
    {
      s->row_bufsize = s->row_bufsize - (s->row_bufsize % row_len);
      DBG (10, "trim_rowbufsize to %d (%d lines)\n",
           s->row_bufsize, s->row_bufsize / row_len);
    }
}                               /* m3096g_trim_rowbufsize */


static SANE_Status
m3096g_read_pixel_size (struct m3096g *s, unsigned int length, int i_window_id)
{
  SANE_Status r;

  DBG (10, "m3096g_read_pixel_size (length = %d)\n", length);

  set_R_datatype_code (readB.cmd, R_pixel_size);
  set_R_window_id(readB.cmd, i_window_id);
  set_R_xfer_length (readB.cmd, length);

  r = do_scsi_cmd (s->sfd, readB.cmd, readB.size, s->buffer, length);

  return r;
}



static SANE_Status
m3096g_read_data_block (struct m3096g *s, unsigned int length, int i_window_id)
{
  SANE_Status r;

  DBG (10, "m3096g_read_data_block (length = %d)\n", length);
  /*wait_scanner(s); */

  set_R_datatype_code (readB.cmd, R_datatype_imagedata);
  set_R_window_id(readB.cmd, i_window_id);
  set_R_xfer_length (readB.cmd, length);

  r = do_scsi_cmd (s->sfd, readB.cmd, readB.size, s->buffer, length);

  return r;
#if 0
#if 0
  return ((r != 0) ? -1 : length);
#else
  if (r)
    {
      return -1;
    }
  else
    {
      return length;
    }
#endif
#endif
}                               /* m3096g_read_data_block */

static int
m3096g_valid_number (int value, const int *acceptable)
{
  int index, max = acceptable[0];

  for (index = 1; index < max + 1; index++)
    {
      if (value == acceptable[index])
        return 1;
    }
  return 0;
}                               /* m3096g_valid_number */

/******************************************************************************
}#############################################################################
******************************************************************************/
