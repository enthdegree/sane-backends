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

   --------------------------------------------------------------------------

   This file implements a SANE backend for various Fujitsu scanners.
   Currently supported:
    - M3091 (Frederik Ramm)
    - M3096 (originally by Randolph Bentson, now by Oliver Schirrmeister)

   The source code is divided in sections which you can easily find by
   search for the tag "@@".

   Section 1 - Init & static stuff
   Section 2 - Routines called by SANE (externally visible)
   Section 3 - generic private routines with no or very little model-specific 
               code
   Section 4 - generic private routines with a lot of model-specific code
   Section 5 - private routines for the M3091
   Section 6 - private routines for the M3096
   Section 7 - private routines for the EVPD-capable scanners
   Section 8 - private routines for the M3092
   ...

   Changes:
      V 1.2, 05-Mai-2002, OS (oschirr@abm.de)
         - release memory allocated by sane_get_devices
         - several bugfixes
         - supports the M3097
         - get threshold, contrast and brightness from vpd
         - imprinter support
         - get_hardware_status now works before calling sane_start
         - avoid unnecessary reload of options when using source=fb
      V 1.3, 08-Aug-2002, OS (oschirr@abm.de)
         - bugfix. Imprinter didn't print the first time after
           switching on the scanner
         - bugfix. reader_generic_passthrough ignored the number of bytes
           returned by the scanner.
      V 1.4, 13-Sep-2002
         - 3092 support (mgoppold@tbz-pariv.de)
         - tested 4097 support
         - changed some functions to receive compressed data
      V 1.4, 13-Feb-2003
         - fi-4220C support (ron@roncemer.com)
         - USB support for scanners which send SCSI commands over usb
           (ron@roncemer.com)
      V 1.5, 20-Feb-2003 OS (oschirr@abm.de)
         - set availability of options THRESHOLD und VARIANCE
           correctly
         - option RIF is available for 3091 und 3092
      V 1.6, 04-Mar-2003 (oschirr@abm.de)
         - renamed some variables
         - bugfix: duplex scanning now works when disconnect is enabled
           in the scsi controller.
      V 1.7, 10-Mar-2003 (oschirr@abm.de)
         - displays the offending byte when something is wrong in the
           window descriptor block.
      V 1.8, 28-Mar-2003 (oschirr@abm.de)
         - fi-4120C support (anoah@pfeiffer.edu)
         - display information about gamma in vital_product_data
      V 1.9 04-Jun-2003 (anoah@pfeiffer.edu)
         - separated the 4120 and 4220 into another model
         - color support for the 4x20
      V 1.10 04-Jun-2003 (anoah@pfeiffer.edu)
         - removed SP15 code
         - sane_open actually opens the device you request
      V 1.11 11-Jun-2003 (anoah@pfeiffer.edu)
         - fixed bug in that code when a scanner is disconnected 
      V 1.12 06-Oct-2003 (anoah@pfeiffer.edu)
         - added code to support color modes of more recent scanners
      V 1.13 07-Nov-2003 (oschirr@abm.de)
	 - Bugfix. If a scanner returned a color image
	   in format rr...r gg.g bb...b the reader process crashed.
	 - Bugfix. The option gamma was enabled for
	   the fi-4120. The result was an 'invalid field in parm list'-error.
      V 1.14 15-Dec-2003 (oschirr@abm.de)
         - Bugfix: set default threshold range to 0..255 There is a problem
           with the M3093 when you are not allows to set the threshold to 0.
         - Bugfix: set the allowable x- and y-DPI values from VPD. Scanning
           with x=100 and y=100 dpi with an fi4120 resulted in an image
           with 100,75 dpi.
         - Bugfix: Set the default value of gamma to 0x80 for all scanners
           that don't have build in gamma patterns.
         - Bugfix: fi-4530 and fi-4210 don't support standard paper size
           spezification. Disable this option for these scanners.
      V 1.14 16-Dec-2003 (oschirr@abm.de)
         - Bugfix: pagewidth and pageheight where disable for the fi-4530C.
      20-Feb-2004 (oschirr@abm.de)
         - merged the 3092-routines with the 3091-routines.
         - inverted the image in mode color and grayscale
         - jpg hardware compression support (fi-4530C)
      04-Mrz-2004 (oschirr@abm.de)
         - enabled option dropoutcolor for the fi-4530C

   SANE FLOW DIAGRAM

   - sane_init() : initialize backend, attach scanners
   . - sane_get_devices() : query list of scanner devices
   . - sane_open() : open a particular scanner device
   . . - sane_set_io_mode : set blocking mode
   . . - sane_get_select_fd : get scanner fd
   . . - sane_get_option_descriptor() : get option information
   . . - sane_control_option() : change option values
   . .
   . . - sane_start() : start image acquisition
   . .   - sane_get_parameters() : returns actual scan parameters
   . .   - sane_read() : read image data (from pipe)
   . . (sane_read called multiple times; after sane_read returns EOF, 
   . . loop may continue with sane_start which may return a 2nd page
   . . when doing duplex scans, or load the next page from the ADF)
   . .
   . . - sane_cancel() : cancel operation
   . - sane_close() : close opened scanner device
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
#include <ctype.h>
#include <time.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef HAVE_LIBC_H
# include <libc.h>              /* NeXTStep/OpenStep */
#endif

#include "sane/sanei_backend.h"
#include "sane/sanei_scsi.h"
#include "sane/sanei_usb.h"
#include "sane/saneopts.h"
#include "sane/sanei_config.h"

#include "fujitsu-scsi.h"
#include "fujitsu.h"

#define DEBUG 1

#define MSG_ERR         1
#define MSG_USER        5
#define MSG_INFO        6
#define FLOW_CONTROL    10
#define MSG_IO          15
#define MSG_IO_READ     17
#define IO_CMD          20
#define IO_CMD_RES      20
#define MSG_GET         25
/* ------------------------------------------------------------------------- */

static const char sourceADF[] = "ADF";
static const char sourceFlatbed[] = "FB";
static SANE_String_Const sourceListWithADF[] =
  { sourceADF, sourceFlatbed, NULL };
static SANE_String_Const sourceListWithoutADF[] = { sourceFlatbed, NULL };

static const char duplexFront[] = "front";
static const char duplexBack[] = "back";
static const char duplexBoth[] = "both";
static SANE_String_Const duplexListWithDuplex[] =
  { duplexFront, duplexBack, duplexBoth, NULL };
static SANE_String_Const duplexListSpecial[] =
  { duplexFront, duplexBoth, NULL };
static SANE_String_Const duplexListWithoutDuplex[] = { duplexFront, NULL };

static const char mode_lineart[] = "Lineart";
static const char mode_halftone[] = "Halftone";
static const char mode_grayscale[] = "Gray";
static const char mode_color[] = "Color";
static SANE_String_Const modeList3091[] =
  { mode_lineart, mode_grayscale, mode_color, NULL };
static SANE_String_Const modeList3096[] =
  { mode_lineart, mode_halftone, mode_grayscale, mode_color, NULL };

static const char lampRed[] = "red";
static const char lampGreen[] = "green";
static const char lampBlue[] = "blue";
static const char lampDefault[] = "default";
static SANE_String_Const lamp_colorList[] =
  { lampRed, lampGreen, lampBlue, lampDefault, NULL };


static const char dropout_color_red[] = "Red";
static const char dropout_color_green[] = "Green";
static const char dropout_color_blue[] = "Blue";
static const char dropout_color_default[] = "Panel";
static SANE_String_Const dropout_color_list[] =
  { dropout_color_default, dropout_color_red,
  dropout_color_green, dropout_color_blue, NULL
};

static const SANE_Range default_threshold_range = { 0, 255, 1 };
static const SANE_Range default_brightness_range = { 0, 255, 1 };
static const SANE_Range default_contrast_range = { 0, 255, 1 };

static const SANE_Range rangeX3091 =
  { 0, SANE_FIX (215.9), SANE_FIX (MM_PER_INCH / 1200) };
static const SANE_Range rangeY3091 =
  { 0, SANE_FIX (355.6), SANE_FIX (MM_PER_INCH / 1200) };
static const SANE_Range rangeX3092 =
  { 0, SANE_FIX (215.5), SANE_FIX (MM_PER_INCH / 1200) };
static const SANE_Range rangeY3092 =
  { 0, SANE_FIX (305.0), SANE_FIX (MM_PER_INCH / 1200) };
static const SANE_Range rangeX3096 =
  { 0, SANE_FIX (308.8), SANE_FIX (MM_PER_INCH / 1200) };
static const SANE_Range rangeY3096 =
  { 0, SANE_FIX (438.9), SANE_FIX (MM_PER_INCH / 1200) };
static const SANE_Range rangeColorOffset = { -16, 16, 1 };
static const char smoothing_mode_ocr[] = "OCR";
static const char smoothing_mode_image[] = "Image";
static SANE_String_Const smoothing_mode_list[] =
  { smoothing_mode_ocr, smoothing_mode_image, NULL };

static const char filtering_ballpoint[] = "Ballpoint";
static const char filtering_ordinary[] = "Ordinary";
static SANE_String_Const filtering_mode_list[] =
  { filtering_ballpoint, filtering_ordinary, NULL };

static const char background_white[] = "White";
static const char background_black[] = "Black";
static SANE_String_Const background_mode_list[] =
  { background_white, background_black, NULL };

static const char white_level_follow_default[] = "Default";
static const char white_level_follow_enabled[] = "Enabled";
static const char white_level_follow_disabled[] = "Disabled";
static SANE_String_Const white_level_follow_mode_list[] =
  { white_level_follow_default, white_level_follow_enabled,
  white_level_follow_disabled, NULL
};

static const char dtc_selection_default[] = "Default";
static const char dtc_selection_simplified[] = "Simplified";
static const char dtc_selection_dynamic[] = "Dynamic";
static SANE_String_Const dtc_selection_mode_list[] =
  { dtc_selection_default, dtc_selection_simplified, dtc_selection_dynamic,
NULL }
 ;

static const char cmp_none[] = "None";
static const char cmp_mh[] = "MH";      /* Fax Group 3 */
static const char cmp_mr[] = "MR";      /* what's that??? */
static const char cmp_mmr[] = "MMR";    /* Fax Group 4 */
static const char cmp_jbig[] = "JBIG";
static const char cmp_jpg_base[] = "JPG_BASE_LINE";
static const char cmp_jpg_ext[] = "JPG_EXTENDED";
static const char cmp_jpg_indep[] = "JPG_INDEPENDENT";


static const char gamma_default[] = "Default";
static const char gamma_normal[] = "Normal";
static const char gamma_soft[] = "Soft";
static const char gamma_sharp[] = "Sharp";
static SANE_String_Const gamma_mode_list[] =
  { gamma_default, gamma_normal, gamma_soft, gamma_sharp, NULL };

static const char emphasis_none[] = "None";
static const char emphasis_low[] = "Low";
static const char emphasis_medium[] = "Medium";
static const char emphasis_high[] = "High";
static const char emphasis_smooth[] = "Smooth";
static SANE_String_Const emphasis_mode_list[] =
  { emphasis_none, emphasis_low, emphasis_medium,
  emphasis_high, emphasis_smooth, NULL
};

static const SANE_Range variance_rate_range = { 0, 255, 1 };

static const SANE_Range threshold_curve_range = { 0, 7, 1 };
static const SANE_Range jpg_quality_range = { 0, 7, 1};

static const char gradation_ordinary[] = "Ordinary";
static const char gradation_high[] = "High";
static SANE_String_Const gradation_mode_list[] =
  { gradation_ordinary, gradation_high, NULL };

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
  { paper_size_a3, paper_size_a4, paper_size_a5,
  paper_size_double, paper_size_letter, paper_size_b4, paper_size_b5,
  paper_size_legal, paper_size_custom, paper_size_detect, NULL
};

static const char paper_orientation_portrait[] = "Portrait";
static const char paper_orientation_landscape[] = "Landscape";
static SANE_String_Const paper_orientation_mode_list[] =
  { paper_orientation_portrait, paper_orientation_landscape, NULL };


static const char imprinter_dir_topbot[] = "Top to Bottom";
static const char imprinter_dir_bottop[] = "Bottom to Top";
static SANE_String_Const imprinter_dir_list[] = 
  { imprinter_dir_topbot, imprinter_dir_bottop, NULL };

static const char imprinter_ctr_dir_inc[] = "Increment";
static const char imprinter_ctr_dir_dec[] = "Decrement";
static SANE_String_Const imprinter_ctr_dir_list[] = 
  { imprinter_ctr_dir_inc, imprinter_ctr_dir_dec, NULL };

static SANE_Range imprinter_ctr_step_range = { 0, 2, 1 };
static const SANE_Range range_imprinter_y_offset =
  { SANE_FIX (7), SANE_FIX(140), 0};
/*SANE_FIX (MM_PER_INCH / 1200) };*/

static SANE_Range range_sleep_mode = {1, 254, 1};

static SANE_Int res_list0[] = { 4, 0, 200, 300, 400 };  /* 3096GX gray */

static SANE_Int res_list1[] = { 5, 0, 200, 240, 300, 400 };/* 3096GX binary */

static SANE_Int res_list2[] = { 7, 0, 100, 150, 200, 240, 300, 400 };/* 3093DG gray */

static SANE_Int res_list3[] = { 8, 0, 100, 150, 200, 240, 300, 400, 600 };      /* 3093DG binary */

/* This can be set via the config file if desired to force a non-recognized 
 * scanner to be treated like one of the known models (e.g. you have the 
 * new M1234-barf5 and believe it is M3096 compatible).
 */
static int forceModel = -1;

/* Also set via config file. */
static int scsiBuffer = 64 * 1024;

/* flaming hack to get USB scanners
   working without timeouts under linux */
static unsigned int cmd_count = 0;

/*
 * required for compressed data transfer. sense_handler has to tell
 * the caller the number of scanned bytes. 
 */
static struct fujitsu *current_scanner = NULL;

/*
 * used by sane_get_devices
 */
static const SANE_Device **devlist = 0;

/*
 * used by attachScanner and attachOne
 */
static Fujitsu_Connection_Type mostRecentConfigConnectionType = SANE_FUJITSU_SCSI;

/*
 * @@ Section 2 - SANE Interface
 */


/**
 * Called by SANE initially.
 * 
 * From the SANE spec:
 * This function must be called before any other SANE function can be
 * called. The behavior of a SANE backend is undefined if this
 * function is not called first. The version code of the backend is
 * returned in the value pointed to by version_code. If that pointer
 * is NULL, no version code is returned. Argument authorize is either
 * a pointer to a function that is invoked when the backend requires
 * authentication for a specific resource or NULL if the frontend does
 * not support authentication.
 */
SANE_Status
sane_init (SANE_Int * version_code, SANE_Auth_Callback authorize)
{
  char devName[PATH_MAX];
  char line[PATH_MAX];
  const char *lp;
  size_t len;
  FILE *fp;

  mostRecentConfigConnectionType = SANE_FUJITSU_SCSI;
  authorize = authorize;        /* get rid of compiler warning */

  DBG_INIT ();
  DBG (10, "sane_init\n");

  sanei_usb_init();

  if (version_code)
    *version_code = SANE_VERSION_CODE (V_MAJOR, V_MINOR, 0);
  fp = sanei_config_open (FUJITSU_CONFIG_FILE);

  /*
   * If we don't have a config file, the device name /dev/scanner is used.
   */
  if (!fp)
    {
      attachScanner ("/dev/scanner", 0);
      return SANE_STATUS_GOOD;
    }

  scsiBuffer = (sanei_scsi_max_request_size < (64 * 1024)) ?
    sanei_scsi_max_request_size : 64 * 1024;

  /*
   * Read the config file.  
   */

  DBG (10, "sane_init: reading config file %s\n", FUJITSU_CONFIG_FILE);

  while (sanei_config_read (line, PATH_MAX, fp))
    {
      int vendor, product;

      /* ignore comments */
      if (line[0] == '#')
        continue;
      len = strlen (line);

      /* delete newline characters at end */
      if (line[len - 1] == '\n')
        line[--len] = '\0';

      lp = sanei_config_skip_whitespace (line);

      /* skip empty lines */
      if (*lp == 0)
        continue;

      if ((strncmp ("option", lp, 6) == 0) && isspace (lp[6]))
        {
          lp += 6;
          lp = sanei_config_skip_whitespace (lp);

          if ((strncmp (lp, "force-model", 11) == 0) && isspace (lp[11]))
            {
              lp += 11;
              lp = sanei_config_skip_whitespace (lp);
              /*
                 if ((forceModel = modelMatch(lp)) == -1)
                 {
                 DBG(MSG_ERR, "sane_init: configuration option \"force-model\" "
                 "does not support value \"%s\" - ignored.\n", lp);
                 }
                 else
                 {
                 DBG (10, "sane_init: force model \"%s\" ok\n", lp);
                 }
               */
              forceModel = MODEL_FORCE;
            }
          else if ((strncmp (lp, "scsi-buffer-size", 16) == 0)
                   && isspace (lp[16]))
            {
              int buf;
              lp += 16;
              lp = sanei_config_skip_whitespace (lp);
              buf = atoi (lp);
              if ((buf >= 4096) && (buf <= sanei_scsi_max_request_size))
                {
                  scsiBuffer = buf;
                }
              else
                {
                  DBG (MSG_ERR, 
                       "sane_init: configuration option \"scsi-buffer-"
                       "size\" is outside allowable range of 4096..%d",
                       sanei_scsi_max_request_size);
                }
            }
          else
            {
              DBG (MSG_ERR,
                   "sane_init: configuration option \"%s\" unrecognized - ignored.\n",
                   lp);
            }
        }
        else if (sscanf(lp, "usb %i %i", &vendor, &product) == 2)
        {
            mostRecentConfigConnectionType = SANE_FUJITSU_USB;
            sanei_usb_attach_matching_devices(lp, attachOne);
            mostRecentConfigConnectionType = SANE_FUJITSU_SCSI;
        }
        else                      /* must be a device name if it's not an option */
        {
          if ((strncmp ("usb", lp, 3) == 0) && isspace (lp[3])) {
            lp += 3;
            lp = sanei_config_skip_whitespace (lp);
            mostRecentConfigConnectionType = SANE_FUJITSU_USB;
          }
          strncpy (devName, lp, sizeof (devName));
          devName[sizeof (devName) - 1] = '\0';
          sanei_config_attach_matching_devices (devName, attachOne);
          mostRecentConfigConnectionType = SANE_FUJITSU_SCSI;
        }
    }
  fclose (fp);
  return SANE_STATUS_GOOD;
}


/**
 * Called by SANE to find out about supported devices.
 * 
 * From the SANE spec:
 * This function can be used to query the list of devices that are
 * available. If the function executes successfully, it stores a
 * pointer to a NULL terminated array of pointers to SANE_Device
 * structures in *device_list. The returned list is guaranteed to
 * remain unchanged and valid until (a) another call to this function
 * is performed or (b) a call to sane_exit() is performed. This
 * function can be called repeatedly to detect when new devices become
 * available. If argument local_only is true, only local devices are
 * returned (devices directly attached to the machine that SANE is
 * running on). If it is false, the device list includes all remote
 * devices that are accessible to the SANE library.
 * 
 * SANE does not require that this function is called before a
 * sane_open() call is performed. A device name may be specified
 * explicitly by a user which would make it unnecessary and
 * undesirable to call this function first.
 */
SANE_Status
sane_get_devices (const SANE_Device *** device_list, SANE_Bool local_only)
{
  struct fujitsu *dev;
  int i;

  DBG (10, "sane_get_devices %d\n", local_only);

  if (devlist)
    free (devlist);
  devlist = calloc (num_devices + 1, sizeof (SANE_Device*));
  if (!devlist)
    return SANE_STATUS_NO_MEM;

  for (dev = first_dev, i = 0; i < num_devices; dev = dev->next)
    devlist[i++] = &dev->sane;
  devlist[i++] = 0;

  *device_list = devlist;

  return SANE_STATUS_GOOD;
}


/**
 * Called to establish connection with the scanner. This function will
 * also establish meaningful defauls and initialize the options.
 *
 * From the SANE spec:
 * This function is used to establish a connection to a particular
 * device. The name of the device to be opened is passed in argument
 * name. If the call completes successfully, a handle for the device
 * is returned in *h. As a special case, specifying a zero-length
 * string as the device requests opening the first available device
 * (if there is such a device).
 */
SANE_Status
sane_open (SANE_String_Const name, SANE_Handle * handle)
{
  struct fujitsu *dev = NULL;
  struct fujitsu *scanner = NULL;
 
  if(name[0] == 0){
    DBG (10, "sane_open: no device requested, using default\n");
    if(first_dev){
      scanner = (struct fujitsu *) first_dev;
      DBG (10, "sane_open: device %s found\n", first_dev->sane.name);
    }
  }
  else{
    DBG (10, "sane_open: device %s requested\n", name);
                                                                                
    for (dev = first_dev; dev; dev = dev->next)
      {
        if (strcmp (dev->sane.name, name) == 0)
          {
            DBG (10, "sane_open: device %s found\n", name);
            scanner = (struct fujitsu *) dev;
          }
      }
  }

  if (!scanner) {
    DBG (10, "sane_open: no device found\n");
    return SANE_STATUS_INVAL;
  }

  *handle = scanner;

  /*
   * Determine which SANE options are available with this scanner.
   * Since many options are identical for all models, handle them 
   * all in one function.
   */

  init_options (scanner);

  /*
   * Now we have to establish sensible defaults for this scanner model.
   * 
   * Setting the default values can be quite model-specific, so
   * we're using an extra routine for each.
   */

  if (!scanner->has_gamma && scanner->num_download_gamma > 0) 
    {
      
      scanner->gamma = 0x80;
    }
  else 
    {
      scanner->gamma = WD_gamma_DEFAULT;
    }

  if(!scanner->has_fixed_paper_size) 
    {
      scanner->paper_selection = WD_paper_SEL_NON_STANDARD;
      scanner->paper_size = WD_paper_UNDEFINED;
      scanner->paper_orientation = WD_paper_PORTRAIT;
    }
  else 
    {
      scanner->paper_size = WD_paper_A4;
      scanner->paper_orientation = WD_paper_PORTRAIT;
      scanner->paper_selection = WD_paper_SEL_STANDARD;
    }


  switch (scanner->model)
    {

    case MODEL_3091:
    case MODEL_3092:
    case MODEL_FI4x20:
      setDefaults3091 (scanner);
      break;

    case MODEL_FORCE:
    case MODEL_3096:
    case MODEL_3097:
    case MODEL_3093:
    case MODEL_4097:
    case MODEL_FI:
      setDefaults3096 (scanner);
      break;

    default:
      DBG (MSG_ERR, "sane_open: unknown model\n");
      return SANE_STATUS_INVAL;
    }

  return SANE_STATUS_GOOD;

}


/**
 * An advanced method we don't support but have to define.
 */
SANE_Status
sane_set_io_mode (SANE_Handle h, SANE_Bool non_blocking)
{
  DBG (10, "sane_set_io_mode\n");
  DBG (99, "%d %p\n", non_blocking, h);
  return SANE_STATUS_UNSUPPORTED;
}


/**
 * An advanced method we don't support but have to define.
 */
SANE_Status
sane_get_select_fd (SANE_Handle h, SANE_Int *fdp)
{
  DBG (10, "sane_get_select_fd\n");
  DBG (99, "%p %d\n", h, *fdp);
  return SANE_STATUS_UNSUPPORTED;
}


/**
 * Returns the options we know.
 *
 * From the SANE spec:
 * This function is used to access option descriptors. The function
 * returns the option descriptor for option number n of the device
 * represented by handle h. Option number 0 is guaranteed to be a
 * valid option. Its value is an integer that specifies the number of
 * options that are available for device handle h (the count includes
 * option 0). If n is not a valid option index, the function returns
 * NULL. The returned option descriptor is guaranteed to remain valid
 * (and at the returned address) until the device is closed.
 */
const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
  struct fujitsu *scanner = handle;

  DBG (MSG_GET, 
       "sane_get_option_descriptor: \"%s\"\n", scanner->opt[option].name);

  if ((unsigned) option >= NUM_OPTIONS)
    return NULL;
  return &scanner->opt[option];
}


/**
 * Gets or sets an option value.
 * 
 * From the SANE spec:
 * This function is used to set or inquire the current value of option
 * number n of the device represented by handle h. The manner in which
 * the option is controlled is specified by parameter action. The
 * possible values of this parameter are described in more detail
 * below.  The value of the option is passed through argument val. It
 * is a pointer to the memory that holds the option value. The memory
 * area pointed to by v must be big enough to hold the entire option
 * value (determined by member size in the corresponding option
 * descriptor).
 * 
 * The only exception to this rule is that when setting the value of a
 * string option, the string pointed to by argument v may be shorter
 * since the backend will stop reading the option value upon
 * encountering the first NUL terminator in the string. If argument i
 * is not NULL, the value of *i will be set to provide details on how
 * well the request has been met.
 */
SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option,
                     SANE_Action action, void *val, SANE_Int * info)
{
  struct fujitsu *scanner = (struct fujitsu *) handle;
  SANE_Status status;
  SANE_Word cap;
  SANE_Int dummy;
  SANE_Word *tmp;
  int newMode;

  /* Make sure that all those statements involving *info cannot break (better
   * than having to do "if (info) ..." everywhere!)
   */
  if (info == 0)
    info = &dummy;

  *info = 0;

  if ( (scanner->object_count != 0) && (scanner->eof == SANE_FALSE) )
    {
      DBG (5, "sane_control_option: device busy\n");
      return SANE_STATUS_DEVICE_BUSY;
    }

  if (option >= NUM_OPTIONS)
    return SANE_STATUS_INVAL;

  cap = scanner->opt[option].cap;

  /*
   * SANE_ACTION_GET_VALUE: We have to find out the current setting and
   * return it in a human-readable form (often, text).
   */
  if (action == SANE_ACTION_GET_VALUE)
    {
      DBG (MSG_GET, "sane_control_option: get value \"%s\"\n",
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
          strcpy (val, (scanner->use_adf == SANE_TRUE) ?
                  sourceADF : sourceFlatbed);
          return SANE_STATUS_GOOD;

        case OPT_DUPLEX:
          strcpy (val, (scanner->duplex_mode == DUPLEX_BOTH) ? duplexBoth :
                  (scanner->duplex_mode == DUPLEX_BACK) ?
                  duplexBack : duplexFront);
          return SANE_STATUS_GOOD;

        case OPT_MODE:
          strcpy (val, (scanner->color_mode == MODE_COLOR) ? mode_color :
                  (scanner->color_mode == MODE_GRAYSCALE) ? mode_grayscale :
                  (scanner->color_mode == MODE_HALFTONE) ? mode_halftone :
                  mode_lineart);
          return SANE_STATUS_GOOD;


        case OPT_LAMP_COLOR:
          strcpy (val, (scanner->lamp_color == LAMP_DEFAULT) ? lampDefault :
                  (scanner->lamp_color == LAMP_RED) ? lampRed :
                  (scanner->lamp_color == LAMP_GREEN) ? lampGreen : lampBlue);
          return SANE_STATUS_GOOD;


        case OPT_DROPOUT_COLOR:
          switch (scanner->dropout_color)
            {
            case MSEL_dropout_DEFAULT:
              strcpy (val, dropout_color_default);
              break;
            case MSEL_dropout_RED:
              strcpy (val, dropout_color_red);
              break;
            case MSEL_dropout_GREEN:
              strcpy (val, dropout_color_green);
              break;
            case MSEL_dropout_BLUE:
              strcpy (val, dropout_color_blue);
              break;
            default:
              return SANE_STATUS_INVAL;
            }
          return SANE_STATUS_GOOD;

        case OPT_X_RES:
          *(SANE_Word *) val = scanner->resolution_x;
          return SANE_STATUS_GOOD;

        case OPT_Y_RES:
          *(SANE_Word *) val = scanner->resolution_y;
          return SANE_STATUS_GOOD;

        case OPT_TL_X:
        case OPT_TL_Y:
          *(SANE_Word *) val = scanner->val[option].w;
          return SANE_STATUS_GOOD;

        case OPT_PAGE_WIDTH:
          *(SANE_Word *) val = SCANNER_UNIT_TO_FIXED_MM (scanner->page_width);
          return SANE_STATUS_GOOD;

        case OPT_PAGE_HEIGHT:
          *(SANE_Word *) val =
            SCANNER_UNIT_TO_FIXED_MM (scanner->page_height);
          return SANE_STATUS_GOOD;

        case OPT_BR_X:
          *(SANE_Word *) val = scanner->bottom_right_x;
          return SANE_STATUS_GOOD;

        case OPT_BR_Y:
          *(SANE_Word *) val = scanner->bottom_right_y;
          return SANE_STATUS_GOOD;

        case OPT_CONTRAST:
          *(SANE_Word *) val = scanner->contrast;
          return SANE_STATUS_GOOD;

        case OPT_RIF:
          *(SANE_Bool *) val = scanner->reverse;
          return SANE_STATUS_GOOD;

        case OPT_BRIGHTNESS:
          *(SANE_Word *) val = scanner->brightness;
          return SANE_STATUS_GOOD;

        case OPT_THRESHOLD:
          *(SANE_Word *) val = scanner->threshold;
          return SANE_STATUS_GOOD;

        case OPT_BLUE_OFFSET:
          *(SANE_Word *) val = scanner->blue_offset;
          return SANE_STATUS_GOOD;

        case OPT_GREEN_OFFSET:
          *(SANE_Word *) val = scanner->green_offset;
          return SANE_STATUS_GOOD;

        case OPT_USE_SWAPFILE:
          *(SANE_Word *) val = scanner->use_temp_file;
          return SANE_STATUS_GOOD;

        case OPT_COMPRESSION:
          strcpy (val, (scanner->compress_type == WD_cmp_NONE) ? cmp_none :
                  (scanner->compress_type == WD_cmp_MH) ? cmp_mh :
                  (scanner->compress_type == WD_cmp_MR) ? cmp_mr :
                  (scanner->compress_type == WD_cmp_MMR) ? cmp_mmr :
                  (scanner->compress_type == WD_cmp_JPG1) ? cmp_jpg_base :
                  (scanner->compress_type == WD_cmp_JPG2) ? cmp_jpg_ext :
                  (scanner->compress_type == WD_cmp_JPG3) ? cmp_jpg_indep :
                  cmp_jbig);
          return SANE_STATUS_GOOD;

	case OPT_COMPRESSION_ARG:
	  
	  *(SANE_Word *) val = scanner->compress_arg;
	  return SANE_STATUS_GOOD;

        case OPT_GAMMA:
          strcpy (val, (scanner->gamma == WD_gamma_DEFAULT) ? gamma_default :
                  (scanner->gamma == WD_gamma_NORMAL) ? gamma_normal :
                  (scanner->gamma == WD_gamma_SOFT) ? gamma_soft :
                  gamma_sharp);
          return SANE_STATUS_GOOD;

        case OPT_OUTLINE_EXTRACTION:
          if (scanner->outline == 0x80)
            {
              *(SANE_Bool *) val = 1;
            } 
          else 
            {
              *(SANE_Bool *) val = 0;
            }
          return SANE_STATUS_GOOD;

        case OPT_EMPHASIS:
          strcpy (val, (scanner->emphasis == WD_emphasis_NONE) ?
                  emphasis_none :
                  (scanner->emphasis == WD_emphasis_LOW) ?
                  emphasis_low :
                  (scanner->emphasis == WD_emphasis_MEDIUM) ?
                  emphasis_medium :
                  (scanner->emphasis == WD_emphasis_HIGH) ?
                  emphasis_high : emphasis_smooth);
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

        case OPT_GRADATION:
          switch (scanner->gradation)
            {
            case WD_gradation_ORDINARY:
              strcpy (val, gradation_ordinary);
              break;
            case WD_gradation_HIGH:
              strcpy (val, gradation_high);
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
                  strcpy (val, paper_size_detect);
                }
              else
                {
                  strcpy (val, paper_size_custom);
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



        case OPT_IMPRINTER:
          *(SANE_Bool *) val = scanner->use_imprinter;
          return SANE_STATUS_GOOD;

        case OPT_IMPRINTER_DIR:
          switch (scanner->imprinter_direction) 
            { 
            case S_im_dir_top_bottom:
              strcpy( val, imprinter_dir_topbot);
              break;
            case S_im_dir_bottom_top:
              strcpy( val, imprinter_dir_bottop);
              break;
            default:
              return SANE_STATUS_INVAL;
            }
          return SANE_STATUS_GOOD;

        case OPT_IMPRINTER_YOFFSET:
          *(SANE_Word *) val = scanner->imprinter_y_offset;
          return SANE_STATUS_GOOD;

        case OPT_IMPRINTER_STRING:
          strncpy((SANE_String) val, scanner->imprinter_string, 
                  max_imprinter_string_length);
          return SANE_STATUS_GOOD;

        case OPT_IMPRINTER_CTR_STEP:
          *(SANE_Word *) val = scanner->imprinter_ctr_step;
          return SANE_STATUS_GOOD;

        case OPT_IMPRINTER_CTR_INIT:
          *(SANE_Word *) val = scanner->imprinter_ctr_init;
          return SANE_STATUS_GOOD;

        case OPT_IMPRINTER_CTR_DIR:
          switch (scanner->imprinter_ctr_dir)
            { 
            case S_im_dir_inc:
              strcpy( val, imprinter_ctr_dir_inc);
              break;
            case S_im_dir_dec:
              strcpy( val, imprinter_ctr_dir_dec);
              break;
            default:
              return SANE_STATUS_INVAL;
            }
          return SANE_STATUS_GOOD;




        case OPT_START_BUTTON:
          if (SANE_STATUS_GOOD == get_hardware_status (scanner))
            {
              *(SANE_Bool *) val = get_HW_start_button (scanner->buffer);
            }
          else
            {
              *(SANE_Bool *) val = SANE_FALSE;
            }
          return SANE_STATUS_GOOD;


        case OPT_SLEEP_MODE:
          *(SANE_Word *) val = scanner->sleep_time;
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

      /*
       * Note - for those options which can assume one of a list of
       * valid values, we can safely assume that they will have
       * exactly one of those values because that's what
       * sanei_constrain_value does. Hence no "else: invalid" branches
       * below.
       */
      switch (option)
        {
        case OPT_SOURCE:
          if (!strcmp (val, sourceADF))
            {
              if (scanner->use_adf)
                {
                  return SANE_STATUS_GOOD;
                }
              else if (scanner->has_adf)
                {
                  scanner->use_adf = SANE_TRUE;
                  /* enable adf-related options. disregard the
                   * fact that not all scanners may understand these. */
                  if (scanner->duplex_present)
                    {
                      scanner->opt[OPT_DUPLEX].cap =
                        SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
                    }

		  /* enable adf paper size selection */
                  if (scanner->has_fixed_paper_size)
                    {
                      scanner->opt[OPT_PAPER_SIZE].cap =
                        SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
                      scanner->opt[OPT_PAPER_ORIENTATION].cap =
                        SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
                    } 
		  else 
		    {
		      scanner->opt[OPT_PAGE_WIDTH].cap = 
                        SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
		      scanner->opt[OPT_PAGE_HEIGHT].cap = 
                        SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
		    }
                }
              else
                {
                  return SANE_STATUS_INVAL;
                }
            }
          else
            {
              if (scanner->use_adf == SANE_FALSE)
                {
                  return SANE_STATUS_GOOD;
                }
              /* flatbed disables duplex, paper size, paper orientation */
              scanner->use_adf = SANE_FALSE;
              scanner->opt[OPT_DUPLEX].cap = SANE_CAP_INACTIVE;
              scanner->duplex_mode = DUPLEX_FRONT;
              scanner->opt[OPT_PAPER_SIZE].cap = SANE_CAP_INACTIVE;
              scanner->opt[OPT_PAPER_ORIENTATION].cap = SANE_CAP_INACTIVE;
              scanner->opt[OPT_PAGE_WIDTH].cap = SANE_CAP_INACTIVE;
              scanner->opt[OPT_PAGE_HEIGHT].cap = SANE_CAP_INACTIVE;
            }
          *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
          return SANE_STATUS_GOOD;

        case OPT_DUPLEX:
          scanner->duplex_mode = (!strcmp (val, duplexFront)) ? DUPLEX_FRONT :
            (!strcmp (val, duplexBack)) ? DUPLEX_BACK : DUPLEX_BOTH;
          if ((scanner->duplex_mode != DUPLEX_FRONT)
              && (!scanner->duplex_present))
            {
              scanner->duplex_mode = DUPLEX_FRONT;
              return SANE_STATUS_INVAL;
            }
          return SANE_STATUS_GOOD;

        case OPT_X_RES:
          if (scanner->resolution_x == *(SANE_Word *) val) 
            {
              return SANE_STATUS_GOOD;
            }
          scanner->resolution_x = (*(SANE_Word *) val);
          /* When x resolution is set and resolutions are linked,
           * set y resolution as well, making sure the value used 
           * is a valid y resolution.
           */
          if (scanner->resolution_linked)
            {
              SANE_Word tmp = (*(SANE_Word *) val);
              if (sanei_constrain_value
                  (scanner->opt + OPT_Y_RES, (void *) &tmp,
                   0) == SANE_STATUS_GOOD)
                {
                  scanner->resolution_y = tmp;
                }
            }
          calculateDerivedValues (scanner);
          *info |= SANE_INFO_RELOAD_PARAMS;
          return SANE_STATUS_GOOD;

        case OPT_Y_RES:
          if (scanner->resolution_y == *(SANE_Word *) val) 
            {
              return SANE_STATUS_GOOD;
            }
          scanner->resolution_y = *(SANE_Word *) val;
          scanner->resolution_linked = SANE_FALSE;
          calculateDerivedValues (scanner);
          *info |= SANE_INFO_RELOAD_PARAMS;
          return SANE_STATUS_GOOD;

        case OPT_TL_X:
          if (scanner->val[option].w == *(SANE_Word *) val) 
            {
              return SANE_STATUS_GOOD;
            }
          scanner->val[option].w = *(SANE_Word *) val;
          calculateDerivedValues (scanner);
          if (scanner->rounded_top_left_x != scanner->val[option].w)
            *info |= SANE_INFO_INEXACT;
          *info |= SANE_INFO_RELOAD_PARAMS;
          return SANE_STATUS_GOOD;

        case OPT_TL_Y:
          if (scanner->val[option].w == *(SANE_Word *) val) 
            {
              return SANE_STATUS_GOOD;
            }
          scanner->val[option].w = *(SANE_Word *) val;
          calculateDerivedValues (scanner);
          if (scanner->rounded_top_left_y != scanner->val[option].w)
            *info |= SANE_INFO_INEXACT;
          *info |= SANE_INFO_RELOAD_PARAMS;
          return SANE_STATUS_GOOD;

        case OPT_PAGE_WIDTH:
          tmp = (SANE_Word *) val;
          scanner->page_width = FIXED_MM_TO_SCANNER_UNIT (*tmp);
          /* FIXME          scanner->opt[OPT_TL_X].constraint.range->max = *tmp; */
          if (scanner->val[option].w > *tmp)
            scanner->val[option].w = *tmp;
          if (scanner->bottom_right_x > *tmp)
            scanner->bottom_right_x = *tmp;
          calculateDerivedValues (scanner);
          *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
          return SANE_STATUS_GOOD;

        case OPT_PAGE_HEIGHT:
          tmp = (SANE_Word *) val;
          scanner->page_height = FIXED_MM_TO_SCANNER_UNIT (*tmp);
          /* FIXME          scanner->opt[OPT_TL_Y].constraint.range->max = *tmp; */
          if (scanner->val[option].w > *tmp)
            scanner->val[option].w = *tmp;
          if (scanner->bottom_right_y > *tmp)
            scanner->bottom_right_y = *tmp;
          calculateDerivedValues (scanner);
          *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
          return SANE_STATUS_GOOD;

        case OPT_BR_X:
          if (scanner->bottom_right_x == *(SANE_Word *) val)
            {
              return SANE_STATUS_GOOD;
            }
          scanner->bottom_right_x = *(SANE_Word *) val;
          calculateDerivedValues (scanner);
          if (scanner->rounded_bottom_right_x != scanner->bottom_right_x)
            *info |= SANE_INFO_INEXACT;
          *info |= SANE_INFO_RELOAD_PARAMS;
          return SANE_STATUS_GOOD;

        case OPT_BR_Y:
          if (scanner->bottom_right_y == *(SANE_Word *) val)
            {
              return SANE_STATUS_GOOD;
            }
          scanner->bottom_right_y = *(SANE_Word *) val;
          calculateDerivedValues (scanner);
          if (scanner->rounded_bottom_right_y != scanner->bottom_right_y)
            *info |= SANE_INFO_INEXACT;
          *info |= SANE_INFO_RELOAD_PARAMS;
          return SANE_STATUS_GOOD;

        case OPT_BRIGHTNESS:
          scanner->brightness = *(SANE_Word *) val;
          return SANE_STATUS_GOOD;

        case OPT_THRESHOLD:
          scanner->threshold = *(SANE_Word *) val;
          return SANE_STATUS_GOOD;

        case OPT_LAMP_COLOR:
          scanner->lamp_color = (!strcmp (val, lampDefault)) ? LAMP_DEFAULT :
            (!strcmp (val, lampRed)) ? LAMP_RED :
            (!strcmp (val, lampGreen)) ? LAMP_GREEN : LAMP_BLUE;
          return SANE_STATUS_GOOD;

        case OPT_DROPOUT_COLOR:
          if (strcmp (val, dropout_color_default) == 0)
            scanner->dropout_color = MSEL_dropout_DEFAULT;
          else if (strcmp (val, dropout_color_red) == 0)
            scanner->dropout_color = MSEL_dropout_RED;
          else if (strcmp (val, dropout_color_green) == 0)
            scanner->dropout_color = MSEL_dropout_GREEN;
          else if (strcmp (val, dropout_color_blue) == 0)
            scanner->dropout_color = MSEL_dropout_BLUE;
          else
            return SANE_STATUS_INVAL;
          return SANE_STATUS_GOOD;

        case OPT_MODE:
          /* Since setting the color mode usually changes many other 
           * settings and available options and is very dependent on the 
           * scanner model, this is out-sourced... 
           */
          newMode = (!strcmp (val, mode_lineart)) ? MODE_LINEART :
            (!strcmp (val, mode_halftone)) ? MODE_HALFTONE :
            (!strcmp (val, mode_grayscale)) ? MODE_GRAYSCALE : MODE_COLOR;

          if (newMode == scanner->color_mode)
            {
              /* no changes - don't bother */
              return SANE_STATUS_GOOD;
            }

          *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;

          switch (scanner->model)
            {
            case MODEL_3091:
            case MODEL_3092:
            case MODEL_FI4x20:
              return (setMode3091 (scanner, newMode));
            case MODEL_FORCE:
            case MODEL_3093:
            case MODEL_3096:
            case MODEL_3097:
            case MODEL_4097:
            case MODEL_FI:
              return (setMode3096 (scanner, newMode));
            }
          return SANE_STATUS_INVAL;

        case OPT_BLUE_OFFSET:
          scanner->blue_offset = *(SANE_Word *) val;
          return SANE_STATUS_GOOD;

        case OPT_GREEN_OFFSET:
          scanner->green_offset = *(SANE_Word *) val;
          return SANE_STATUS_GOOD;

        case OPT_USE_SWAPFILE:
          scanner->use_temp_file = *(SANE_Word *) val;
          return SANE_STATUS_GOOD;

        case OPT_MIRROR_IMAGE:
          scanner->mirroring = *(SANE_Bool *) val;
          return SANE_STATUS_GOOD;

        case OPT_NOISE_REMOVAL:
          if (scanner->noise_removal == (*(SANE_Bool *) val))
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
          scanner->reverse = *(SANE_Bool *) val;
          return SANE_STATUS_GOOD;

        case OPT_OUTLINE_EXTRACTION:
          if ((*(SANE_Bool *) val)) 
            {
              scanner->outline = 0x80;
            } 
          else 
            {
              scanner->outline = 0x00;
            }
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
          else if (strcmp (val, cmp_jbig) == 0)
            scanner->compress_type = WD_cmp_JBIG;
          else if (strcmp (val, cmp_jpg_base) == 0)
            scanner->compress_type = WD_cmp_JPG1;
          else if (strcmp (val, cmp_jpg_ext) == 0)
            scanner->compress_type = WD_cmp_JPG2;
          else if (strcmp (val, cmp_jpg_indep) == 0)
            scanner->compress_type = WD_cmp_JPG3;
          else
            return SANE_STATUS_INVAL;

          return SANE_STATUS_GOOD;

	case OPT_COMPRESSION_ARG:
	  
	  scanner->compress_arg = *(SANE_Word *) val;
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
            return SANE_STATUS_INVAL;
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
            return SANE_STATUS_INVAL;
          return SANE_STATUS_GOOD;

        case OPT_THRESHOLD_CURVE:
          scanner->dtc_threshold_curve = *(SANE_Word *) val;
          return SANE_STATUS_GOOD;

        case OPT_GRADATION:
          if (strcmp (val, gradation_ordinary) == 0)
            scanner->gradation = WD_gradation_ORDINARY;
          else if (strcmp (val, gradation_high) == 0)
            scanner->gradation = WD_gradation_HIGH;
          else
            return SANE_STATUS_INVAL;
          return SANE_STATUS_GOOD;

        case OPT_SMOOTHING_MODE:
          if (strcmp (val, smoothing_mode_ocr) == 0)
            scanner->smoothing_mode = WD_smoothing_OCR;
          else if (strcmp (val, smoothing_mode_image) == 0)
            scanner->smoothing_mode = WD_smoothing_IMAGE;
          else
            return SANE_STATUS_INVAL;
          return SANE_STATUS_GOOD;

        case OPT_FILTERING:
          if (strcmp (val, filtering_ballpoint) == 0)
            scanner->filtering = WD_filtering_BALLPOINT;
          else if (strcmp (val, filtering_ordinary) == 0)
            scanner->filtering = WD_filtering_ORDINARY;
          else
            return SANE_STATUS_INVAL;
          return SANE_STATUS_GOOD;

        case OPT_BACKGROUND:
          if (strcmp (val, background_white) == 0)
            scanner->background = WD_background_WHITE;
          else if (strcmp (val, background_black) == 0)
            scanner->background = WD_background_BLACK;
          else
            return SANE_STATUS_INVAL;
          return SANE_STATUS_GOOD;

        case OPT_WHITE_LEVEL_FOLLOW:
          if (strcmp (val, white_level_follow_default) == 0)
            scanner->white_level_follow = WD_white_level_follow_DEFAULT;
          else if (strcmp (val, white_level_follow_enabled) == 0)
            scanner->white_level_follow = WD_white_level_follow_ENABLED;
          else if (strcmp (val, white_level_follow_disabled) == 0)
            scanner->white_level_follow = WD_white_level_follow_DISABLED;
          else
            return SANE_STATUS_INVAL;
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
              if (scanner->dtc_selection == WD_dtc_selection_SIMPLIFIED)
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
              scanner->opt[OPT_GRADATION].cap = SANE_CAP_INACTIVE;
              scanner->opt[OPT_THRESHOLD_CURVE].cap = SANE_CAP_INACTIVE;
              scanner->noise_removal = 0;
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
              scanner->opt[OPT_GRADATION].cap = SANE_CAP_SOFT_DETECT
                | SANE_CAP_SOFT_SELECT;
              scanner->opt[OPT_THRESHOLD_CURVE].cap = SANE_CAP_SOFT_DETECT
                | SANE_CAP_SOFT_SELECT;
            }

          switch (scanner->dtc_selection) 
            {
            case WD_dtc_selection_DYNAMIC:

              scanner->opt[OPT_THRESHOLD].cap = SANE_CAP_INACTIVE;
              scanner->opt[OPT_VARIANCE_RATE].cap = SANE_CAP_INACTIVE;
              break;

            case WD_dtc_selection_SIMPLIFIED:
              scanner->opt[OPT_VARIANCE_RATE].cap = SANE_CAP_SOFT_DETECT
                | SANE_CAP_SOFT_SELECT;
              scanner->opt[OPT_THRESHOLD].cap = SANE_CAP_INACTIVE;
              break;

            case WD_dtc_selection_DEFAULT: 
            default:
              scanner->opt[OPT_THRESHOLD].cap = SANE_CAP_SOFT_DETECT
                | SANE_CAP_SOFT_SELECT;
              scanner->opt[OPT_VARIANCE_RATE].cap = SANE_CAP_INACTIVE;
            }

          *info |= SANE_INFO_RELOAD_OPTIONS;
          return SANE_STATUS_GOOD;

        case OPT_PAPER_SIZE:
          if (strcmp (val, paper_size_a3) == 0)
            {
              if (scanner->paper_size == WD_paper_A3)
                return SANE_STATUS_GOOD;
              scanner->paper_size = WD_paper_A3;
              fujitsu_set_standard_size (scanner);
            }
          else if (strcmp (val, paper_size_a4) == 0)
            {
              if (scanner->paper_size == WD_paper_A4)
                return SANE_STATUS_GOOD;
              scanner->paper_size = WD_paper_A4;
              fujitsu_set_standard_size (scanner);
            }
          else if (strcmp (val, paper_size_a5) == 0)
            {
              if (scanner->paper_size == WD_paper_A5)
                return SANE_STATUS_GOOD;
              scanner->paper_size = WD_paper_A5;
              fujitsu_set_standard_size (scanner);
            }
          else if (strcmp (val, paper_size_double) == 0)
            {
              if (scanner->paper_size == WD_paper_DOUBLE)
                return SANE_STATUS_GOOD;
              scanner->paper_size = WD_paper_DOUBLE;
              fujitsu_set_standard_size (scanner);
            }
          else if (strcmp (val, paper_size_letter) == 0)
            {
              if (scanner->paper_size == WD_paper_LETTER)
                return SANE_STATUS_GOOD;
              scanner->paper_size = WD_paper_LETTER;
              fujitsu_set_standard_size (scanner);
            }
          else if (strcmp (val, paper_size_b4) == 0)
            {
              if (scanner->paper_size == WD_paper_B4)
                return SANE_STATUS_GOOD;
              scanner->paper_size = WD_paper_B4;
              fujitsu_set_standard_size (scanner);
            }
          else if (strcmp (val, paper_size_b5) == 0)
            {
              if (scanner->paper_size == WD_paper_B5)
                return SANE_STATUS_GOOD;
              scanner->paper_size = WD_paper_B5;
              fujitsu_set_standard_size (scanner);
            }
          else if (strcmp (val, paper_size_legal) == 0)
            {
              if (scanner->paper_size == WD_paper_LEGAL)
                return SANE_STATUS_GOOD;
              scanner->paper_size = WD_paper_LEGAL;
              fujitsu_set_standard_size (scanner);
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
              scanner->opt[OPT_PAGE_WIDTH].cap = SANE_CAP_SOFT_SELECT |
                SANE_CAP_SOFT_DETECT;
              scanner->opt[OPT_PAGE_HEIGHT].cap = SANE_CAP_SOFT_SELECT |
                SANE_CAP_SOFT_DETECT;
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
              scanner->opt[OPT_PAGE_WIDTH].cap = SANE_CAP_INACTIVE;
              scanner->opt[OPT_PAGE_HEIGHT].cap = SANE_CAP_INACTIVE;
              *info |= SANE_INFO_RELOAD_PARAMS;
            }
          else
            {
              return SANE_STATUS_INVAL;
            }

          *info |= SANE_INFO_RELOAD_OPTIONS;
          return SANE_STATUS_GOOD;


        case OPT_PAPER_ORIENTATION:
          if (strcmp (val, paper_orientation_portrait) == 0)
            scanner->paper_orientation = WD_paper_PORTRAIT;
          else if (strcmp (val, paper_orientation_landscape) == 0)
            scanner->paper_orientation = WD_paper_LANDSCAPE;
          else
            return SANE_STATUS_INVAL;
          return SANE_STATUS_GOOD;

        case OPT_CONTRAST:
          scanner->contrast = *(SANE_Word *) val;
          return SANE_STATUS_GOOD;

        case OPT_VARIANCE_RATE:
          scanner->var_rate_dyn_thresh = *(SANE_Word *) val;
          return SANE_STATUS_GOOD;




        case OPT_IMPRINTER:

          if (scanner->use_imprinter == *(SANE_Bool *) val)
            {
              return SANE_STATUS_GOOD;
            }

          scanner->use_imprinter = *(SANE_Bool *) val;

          imprinter(scanner);

          if (scanner->use_imprinter)
            {
              scanner->opt[OPT_IMPRINTER_DIR].cap = SANE_CAP_SOFT_SELECT |
                SANE_CAP_SOFT_DETECT;
              scanner->opt[OPT_IMPRINTER_YOFFSET].cap = SANE_CAP_SOFT_SELECT |
                SANE_CAP_SOFT_DETECT;
              scanner->opt[OPT_IMPRINTER_STRING].cap = SANE_CAP_SOFT_SELECT |
                SANE_CAP_SOFT_DETECT;
              scanner->opt[OPT_IMPRINTER_CTR_INIT].cap = SANE_CAP_SOFT_SELECT |
                SANE_CAP_SOFT_DETECT;
              scanner->opt[OPT_IMPRINTER_CTR_STEP].cap = SANE_CAP_SOFT_SELECT |
                SANE_CAP_SOFT_DETECT;
              scanner->opt[OPT_IMPRINTER_CTR_DIR].cap = SANE_CAP_SOFT_SELECT |
                SANE_CAP_SOFT_DETECT;
              *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
            }
          else
            {
              scanner->opt[OPT_IMPRINTER_DIR].cap = SANE_CAP_INACTIVE;
              scanner->opt[OPT_IMPRINTER_YOFFSET].cap = SANE_CAP_INACTIVE;
              scanner->opt[OPT_IMPRINTER_STRING].cap = SANE_CAP_INACTIVE;
              scanner->opt[OPT_IMPRINTER_CTR_INIT].cap = SANE_CAP_INACTIVE;
              scanner->opt[OPT_IMPRINTER_CTR_STEP].cap = SANE_CAP_INACTIVE;
              scanner->opt[OPT_IMPRINTER_CTR_DIR].cap = SANE_CAP_INACTIVE;
              *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
            }
          return SANE_STATUS_GOOD;

        case OPT_IMPRINTER_DIR:
          if (strcmp (val, imprinter_dir_topbot) == 0)
            scanner->imprinter_direction = S_im_dir_top_bottom;
          else if (strcmp (val, imprinter_dir_bottop) == 0)
            scanner->imprinter_direction = S_im_dir_bottom_top;
          else
            return SANE_STATUS_INVAL;
          return SANE_STATUS_GOOD;

        case OPT_IMPRINTER_YOFFSET:
          scanner->imprinter_y_offset = *(SANE_Word *) val;
          return SANE_STATUS_GOOD;

        case OPT_IMPRINTER_STRING:
          strncpy(scanner->imprinter_string, (SANE_String) val,
                  max_imprinter_string_length);
          
          return SANE_STATUS_GOOD;

        case OPT_IMPRINTER_CTR_STEP:
          scanner->imprinter_ctr_step = (*(SANE_Word *) val);
          return SANE_STATUS_GOOD;

        case OPT_IMPRINTER_CTR_INIT:
          if (scanner->imprinter_ctr_init == (*(SANE_Word *) val)) 
            {
              return SANE_STATUS_GOOD;
            }
          scanner->imprinter_ctr_init = (*(SANE_Word *) val);
          imprinter(scanner);

          return SANE_STATUS_GOOD;

        case OPT_IMPRINTER_CTR_DIR:
          if (strcmp (val, imprinter_ctr_dir_inc) == 0)
            scanner->imprinter_ctr_dir = S_im_dir_inc;
          else if (strcmp (val, imprinter_ctr_dir_dec) == 0)
            scanner->imprinter_ctr_dir = S_im_dir_dec;
          else
            return SANE_STATUS_INVAL;
          return SANE_STATUS_GOOD;

        case OPT_SLEEP_MODE:
          scanner->sleep_time = (*(SANE_Word *) val);
          /*fujitsu_set_sleep_mode(scanner);*/
          return SANE_STATUS_GOOD;

        }                       /* switch */
    }                           /* else */
  return SANE_STATUS_INVAL;
}

static void
fujitsu_set_standard_size (SANE_Handle handle)
{
  struct fujitsu *scanner = handle;

  scanner->paper_selection = WD_paper_SEL_STANDARD;
  scanner->opt[OPT_PAPER_ORIENTATION].cap = SANE_CAP_SOFT_DETECT
    | SANE_CAP_SOFT_SELECT;
  scanner->opt[OPT_PAGE_WIDTH].cap = SANE_CAP_INACTIVE;
  scanner->opt[OPT_PAGE_HEIGHT].cap = SANE_CAP_INACTIVE;

  return;
}


/**
 * Called by SANE when a page acquisition operation is to be started.
 *
 */
SANE_Status
sane_start (SANE_Handle handle)
{
  struct fujitsu *scanner = handle;
  int defaultFds[2];
  int duplexFds[2];
  int tempFile = -1;
  int ret;

  DBG (10, "sane_start\n");
  DBG (10, "\tobject_count = %d\n", scanner->object_count);
  DBG (10, "\tduplex_mode = %s\n",
       (scanner->duplex_mode ==
        DUPLEX_BOTH) ? "DUPLEX_BOTH" : (scanner->duplex_mode ==
                                        DUPLEX_BACK) ? "DUPLEX_BACK" :
       "DUPLEX_FRONT");
  DBG (10, "\tuse_temp_file = %s\n",
       (scanner->use_temp_file == SANE_TRUE) ? "yes" : "no");

  if ((scanner->object_count == 1) && (scanner->eof == SANE_TRUE) &&
      (scanner->duplex_mode == DUPLEX_BOTH))
    {
      /* Proceed with rear page. No action necessary; reader process has
       * already acquired the rear page and is only waiting to deliver it!
       */

      if (scanner->use_temp_file)
        {
          /* We're using a temp file for the duplex page, not a pipe.
           * That means we have to wait until the subprocess has
           * terminated - otherwise the file might be incomplete.
           */
          int exit_status;
          DBG (10, "sane_start: waiting for reader to terminate...\n");
          while (wait (&exit_status) != scanner->reader_pid);
          DBG (10, "sane_start: reader process has terminated.\n");
          lseek (scanner->duplex_pipe, 0, SEEK_SET);
        }

      scanner->object_count = 2;
      scanner->eof = SANE_FALSE;
      return SANE_STATUS_GOOD;
    }

  if (scanner->eof == SANE_TRUE)
    {
      scanner->object_count = 0;
    }

  if (scanner->object_count != 0)
    {
      DBG (5, "sane_start: device busy\n");
      return SANE_STATUS_DEVICE_BUSY;
    }

  if (scanner->sfd < 0)
    {
      /* first call */
      if (scanner->connection == SANE_FUJITSU_USB) {
            DBG (10, "sane_start opening USB device\n");
            if (sanei_usb_open (scanner->sane.name, &(scanner->sfd)) !=
                SANE_STATUS_GOOD) {
                DBG (MSG_ERR, 
                     "sane_start: open of %s failed:\n", scanner->sane.name);
                return SANE_STATUS_INVAL;
            }
      } else if (scanner->connection == SANE_FUJITSU_SCSI) {
            DBG (10, "sane_start opening SCSI device\n");
            if (sanei_scsi_open (scanner->sane.name, &(scanner->sfd),
                                 scsi_sense_handler, 0) != SANE_STATUS_GOOD) {
                DBG (MSG_ERR, 
                     "sane_start: open of %s failed:\n", scanner->sane.name);
                return SANE_STATUS_INVAL;
            }
      }
    }
  scanner->object_count = 1;
  scanner->eof = SANE_FALSE;

  if ((ret = grab_scanner (scanner)))
    {
      DBG (5, "sane_start: unable to reserve scanner\n");
      if (scanner->connection == SANE_FUJITSU_USB) {
            sanei_usb_close (scanner->sfd);
      } else if (scanner->connection == SANE_FUJITSU_SCSI) {
            sanei_scsi_close (scanner->sfd);
      }
      scanner->object_count = 0;
      scanner->sfd = -1;
      return ret;
    }

  fujitsu_set_sleep_mode(scanner);


  if (set_mode_params (scanner))
    {

      DBG (MSG_ERR, "sane_start: ERROR: failed to set mode\n");
      /* ignore it */
    }

  if ((ret = fujitsu_send(scanner)))
    {
      DBG (5, "sane_start: ERROR: failed to start send command\n");
      do_cancel(scanner);
      return ret;
    }
#if 0
  if ((ret = imprinter(scanner)))
    {
      DBG (5, "sane_start: ERROR: failed to start imprinter command\n");
      do_cancel(Scanner);
      return ret;
    }
#endif

  if (scanner->use_adf == SANE_TRUE && 
      (ret = object_position (scanner, SANE_TRUE)))
    {
      DBG (5, "sane_start: WARNING: ADF empty\n");
      do_cancel(scanner);
      return ret;
    }

  /* swap_res ?? */


  if ((ret = setWindowParam (scanner)))
    {
      DBG (5, "sane_start: ERROR: failed to set window\n");
      do_cancel(scanner);
      return ret;
    }

  calculateDerivedValues (scanner);

  DBG (10, "\tbytes per line = %d\n", scanner->bytes_per_scan_line);
  DBG (10, "\tpixels_per_line = %d\n", scanner->scan_width_pixels);
  DBG (10, "\tlines = %d\n", scanner->scan_height_pixels);
  DBG (10, "\tbrightness (halftone) = %d\n", scanner->brightness);
  DBG (10, "\tthreshold (line art) = %d\n", scanner->threshold);


  ret = start_scan (scanner);
  if (ret != SANE_STATUS_GOOD) 
    {
      DBG (MSG_ERR, "start_scan failed");
      return ret;
    }

  /* create a pipe, fds[0]=read-fd, fds[1]=write-fd */
  if (pipe (defaultFds) < 0)
    {
      DBG (MSG_ERR, "ERROR: could not create pipe\n");
      scanner->object_count = 0;
      do_cancel(scanner);
      return SANE_STATUS_IO_ERROR;
    }

  duplexFds[0] = duplexFds[1] = -1;
  if (scanner->duplex_mode == DUPLEX_BOTH)
    {
      if (scanner->use_temp_file)
        {
          if ((tempFile = makeTempFile ()) == -1)
            {
              DBG (MSG_ERR, "ERROR: could not create temporary file.\n");
              scanner->object_count = 0;
              do_cancel (scanner);
              return SANE_STATUS_IO_ERROR;
            }
        }
      else
        {
          if (pipe (duplexFds) < 0)
            {
              DBG (MSG_ERR, "ERROR: could not create duplex pipe.\n");
              scanner->object_count = 0;
              do_cancel (scanner);
              return SANE_STATUS_IO_ERROR;
            }
        }
    }

  ret = SANE_STATUS_GOOD;
  scanner->reader_pid = fork();
  if (scanner->reader_pid == 0)
    {
      /* reader_pid = 0 ===> child process */
      sigset_t ignore_set;
      struct SIGACTION act;

      close (defaultFds[0]);
      if (duplexFds[0] != -1)
        close (duplexFds[0]);

      sigfillset (&ignore_set);
      sigdelset (&ignore_set, SIGTERM);
      sigprocmask (SIG_SETMASK, &ignore_set, 0);

      memset (&act, 0, sizeof (act));
      sigaction (SIGTERM, &act, 0);

      /* don't use exit() since that would run the atexit() handlers... */
      _exit (reader_process
             (scanner, defaultFds[1],
              (tempFile != -1) ? tempFile : duplexFds[1]));
    } 
  else if (scanner->reader_pid == -1) 
    {
      DBG(MSG_ERR, "cannot fork reader process.\n");
      DBG(MSG_ERR, "%s", strerror(errno));
      ret = SANE_STATUS_IO_ERROR;

    }
  close (defaultFds[1]);
  if (duplexFds[1] != -1)
    close (duplexFds[1]);
  scanner->default_pipe = defaultFds[0];
  scanner->duplex_pipe = (tempFile != -1) ? tempFile : duplexFds[0];

  if (ret == SANE_STATUS_GOOD) 
    {
      DBG (10, "sane_start: ok\n");
    }

  return ret;
}


/**
 * Called by SANE to retrieve information about the type of data
 * that the current scan will return.
 *
 * From the SANE spec:
 * This function is used to obtain the current scan parameters. The
 * returned parameters are guaranteed to be accurate between the time
 * a scan has been started (sane_start() has been called) and the
 * completion of that request. Outside of that window, the returned
 * values are best-effort estimates of what the parameters will be
 * when sane_start() gets invoked.
 * 
 * Calling this function before a scan has actually started allows,
 * for example, to get an estimate of how big the scanned image will
 * be. The parameters passed to this function are the handle h of the
 * device for which the parameters should be obtained and a pointer p
 * to a parameter structure.
 */
SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters * params)
{
  struct fujitsu *scanner = (struct fujitsu *) handle;

  DBG (10, "sane_get_parameters\n");

  calculateDerivedValues (scanner);

  if (scanner->color_mode == MODE_COLOR)
    {
      params->format = SANE_FRAME_RGB;
      params->depth = 8;        /* internally we treat this as 24 */
    }
  else
    {
      params->format = SANE_FRAME_GRAY;
      params->depth = scanner->output_depth;
    }

  params->pixels_per_line = scanner->scan_width_pixels;
  params->lines = scanner->scan_height_pixels;

  /* The internal "bytes per scan line" value reflects what we get
   * from the scanner.  If the output depth differs from the scanner
   * depth, we have to modify the value for the frontend accordingly.
   *
   * This is currently ineffective since scanner_depth will always equal
   * output_depth.
   */
  params->bytes_per_line = scanner->bytes_per_scan_line *
    scanner->scanner_depth / scanner->output_depth;

  params->last_frame = 1;
  DBG (10, "\tdepth %d\n", params->depth);
  DBG (10, "\tlines %d\n", params->lines);
  DBG (10, "\tpixels_per_line %d\n", params->pixels_per_line);
  DBG (10, "\tbytes_per_line %d\n", params->bytes_per_line);
  return SANE_STATUS_GOOD;
}


/**
 * Called by SANE to read data.
 * 
 * In this implementation, sane_read does nothing much besides reading
 * data from a pipe and handing it back. On the other end of the pipe
 * there's the reader process which gets data from the scanner and
 * stuffs it into the pipe.
 * 
 * From the SANE spec:
 * This function is used to read image data from the device
 * represented by handle h.  Argument buf is a pointer to a memory
 * area that is at least maxlen bytes long.  The number of bytes
 * returned is stored in *len. A backend must set this to zero when
 * the call fails (i.e., when a status other than SANE_STATUS_GOOD is
 * returned).
 * 
 * When the call succeeds, the number of bytes returned can be
 * anywhere in the range from 0 to maxlen bytes.
 */
SANE_Status
sane_read (SANE_Handle handle, SANE_Byte * buf,
           SANE_Int max_len, SANE_Int * len)
{
  struct fujitsu *scanner = (struct fujitsu *) handle;
  ssize_t nread;
  int source;

  *len = 0;

  /* Since we may be operating duplex - check where we're supposed to
   * read the data from
   */
  switch (scanner->object_count)
    {
    case 1:
      source = scanner->default_pipe;   /* this is always a pipe */
      break;
    case 2:
      source = scanner->duplex_pipe;    /* this may be a pipe or a file */
      break;
    default:
      return do_cancel (scanner);
    }
  DBG (30, "sane_read, object_count=%d\n", scanner->object_count);

  nread = read (source, buf, max_len);
  DBG (30, "sane_read: read %ld bytes of %ld\n",
       (long) nread, (long) max_len);

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
    {
      close (source);
      DBG (10, "sane_read: pipe closed\n");
      scanner->eof = SANE_TRUE;
      return SANE_STATUS_EOF;
    }

  return SANE_STATUS_GOOD;
}                               /* sane_read */


/**
 * Cancels a scan. 
 *
 * It has been said on the mailing list that sane_cancel is a bit of a
 * misnomer because it is routinely called to signal the end of a
 * batch - quoting David Mosberger-Tang:
 * 
 * > In other words, the idea is to have sane_start() be called, and
 * > collect as many images as the frontend wants (which could in turn
 * > consist of multiple frames each as indicated by frame-type) and
 * > when the frontend is done, it should call sane_cancel(). 
 * > Sometimes it's better to think of sane_cancel() as "sane_stop()"
 * > but that name would have had some misleading connotations as
 * > well, that's why we stuck with "cancel".
 * 
 * The current consensus regarding duplex and ADF scans seems to be
 * the following call sequence: sane_start; sane_read (repeat until
 * EOF); sane_start; sane_read...  and then call sane_cancel if the
 * batch is at an end. I.e. do not call sane_cancel during the run but
 * as soon as you get a SANE_STATUS_NO_DOCS.
 * 
 * From the SANE spec:
 * This function is used to immediately or as quickly as possible
 * cancel the currently pending operation of the device represented by
 * handle h.  This function can be called at any time (as long as
 * handle h is a valid handle) but usually affects long-running
 * operations only (such as image is acquisition). It is safe to call
 * this function asynchronously (e.g., from within a signal handler).
 * It is important to note that completion of this operaton does not
 * imply that the currently pending operation has been cancelled. It
 * only guarantees that cancellation has been initiated. Cancellation
 * completes only when the cancelled call returns (typically with a
 * status value of SANE_STATUS_CANCELLED).  Since the SANE API does
 * not require any other operations to be re-entrant, this implies
 * that a frontend must not call any other operation until the
 * cancelled operation has returned.
 */
void
sane_cancel (SANE_Handle h)
{
  DBG (10, "sane_cancel\n");
  do_cancel ((struct fujitsu *) h);
}


/**
 * Ends use of the scanner.
 * 
 * From the SANE spec:
 * This function terminates the association between the device handle
 * passed in argument h and the device it represents. If the device is
 * presently active, a call to sane_cancel() is performed first. After
 * this function returns, handle h must not be used anymore.
 */
void
sane_close (SANE_Handle handle)
{
  DBG (10, "sane_close\n");
  if (((struct fujitsu *) handle)->object_count != 0)
                {
    do_reset (handle);
    do_cancel (handle);
                }
}


/**
 * Terminates the backend.
 * 
 * From the SANE spec:
 * This function must be called to terminate use of a backend. The
 * function will first close all device handles that still might be
 * open (it is recommended to close device handles explicitly through
 * a call to sane_clo-se(), but backends are required to release all
 * resources upon a call to this function). After this function
 * returns, no function other than sane_init() may be called
 * (regardless of the status value returned by sane_exit(). Neglecting
 * to call this function may result in some resources not being
 * released properly.
 */
void
sane_exit (void)
{
  struct fujitsu *dev, *next;

  DBG (10, "sane_exit\n");

  for (dev = first_dev; dev; dev = next)
    {
      next = dev->next;
      free (dev->devicename);
      free (dev->buffer);
      free (dev);
    }

  if (devlist)
    free (devlist);
}


/*
 * @@ Section 3 - generic routines which don't distinguish between models
 */


/**
 *
 *
 */
static SANE_Status
attachScanner (const char *devicename, struct fujitsu **devp)
{
  struct fujitsu *dev;
  SANE_Int sfd;

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
  if (mostRecentConfigConnectionType == SANE_FUJITSU_USB) {
    DBG (15, "attachScanner opening USB device\n");
    if (sanei_usb_open (devicename, &sfd) != SANE_STATUS_GOOD) {
        DBG (5, "attach_scanner: open failed\n");
        return SANE_STATUS_INVAL;
    }
  } else if (mostRecentConfigConnectionType == SANE_FUJITSU_SCSI) {
    DBG (15, "attachScanner opening SCSI device\n");
    if (sanei_scsi_open (devicename, &sfd, scsi_sense_handler, 0) != 0) {
        DBG (5, "attach_scanner: open failed\n");
        return SANE_STATUS_INVAL;
    }
  }

  if (NULL == (dev = malloc (sizeof (*dev))))
    return SANE_STATUS_NO_MEM;
  memset(dev,0,sizeof(*dev));

  dev->scsi_buf_size = scsiBuffer;

  if ((dev->buffer = malloc (dev->scsi_buf_size)) == NULL)
    return SANE_STATUS_NO_MEM;

  memset(dev->buffer,0,sizeof(dev->scsi_buf_size));

  dev->devicename = strdup (devicename);
  dev->connection = mostRecentConfigConnectionType;
  dev->sfd = sfd;

  /*
   * Now query the device and find out what it is, exactly.
   */
  if (identify_scanner (dev) != 0)
    {
      DBG (5, "attach_scanner: scanner identification failed\n");
      if (dev->connection == SANE_FUJITSU_USB) {
            sanei_usb_close (dev->sfd);
      } else if (dev->connection == SANE_FUJITSU_SCSI) {
            sanei_scsi_close (dev->sfd);
      }
      free (dev->buffer);
      free (dev);
      return SANE_STATUS_INVAL;
    }

  /* Why? */
  if (dev->connection == SANE_FUJITSU_USB) {
        sanei_usb_close (dev->sfd);
  } else if (dev->connection == SANE_FUJITSU_SCSI) {
        sanei_scsi_close (dev->sfd);
  }
  dev->sfd = -1;

  dev->sane.name = dev->devicename;
  dev->sane.vendor = dev->vendorName;
  dev->sane.model = dev->productName;
  dev->sane.type = "scanner";

  ++num_devices;
  dev->next = first_dev;
  first_dev = dev;

  if (devp)
    {
      *devp = dev;
    }

  DBG (15, "attach_scanner: done\n");

  return SANE_STATUS_GOOD;
}

/**
 * 
 *
 *
 */
static SANE_Status
attachOne (const char *name)
{
  return attachScanner (name, 0);
}

/**
 * Called from within the SANE SCSI core if there's trouble with the device.
 * Responsible for producing a meaningful debug message.
 */
static SANE_Status
scsi_sense_handler (int scsi_fd, u_char * sensed_data, void *arg)
{
  unsigned int ret = SANE_STATUS_IO_ERROR;
  unsigned int sense = get_RS_sense_key (sensed_data);
  unsigned int asc = get_RS_ASC (sensed_data);
  unsigned int ascq = get_RS_ASCQ (sensed_data);

  scsi_fd = scsi_fd;
  arg = arg;                    /* get rid of compiler warnings */

  switch (sense)
    {
    case 0x0:                   /* No Sense */
      DBG (5, "\t%d/%d/%d: Scanner ready\n", sense, asc, ascq);
      if ( (current_scanner != NULL) && (get_RS_EOM (sensed_data)) ) {
          current_scanner->i_transfer_length = get_RS_information (sensed_data);
          return SANE_STATUS_EOF;
      }
      return SANE_STATUS_GOOD;

    case 0x2:                   /* Not Ready */
      if ((0x00 == asc) && (0x00 == ascq))
        {
          DBG (MSG_ERR, "\t%d/%d/%d: Not Ready \n", sense, asc, ascq);
        }
      else
        {
          DBG (MSG_ERR, "\tUnknown - Sense=%d, ASC=%d, ASCQ=%d\n", 
               sense, asc, ascq);
        }
      break;

    case 0x3:                   /* Medium Error */
      if ((0x80 == asc) && (0x01 == ascq))
        {
          DBG (MSG_ERR, "\t%d/%d/%d: Jam \n", sense, asc, ascq);
          ret = SANE_STATUS_JAMMED;
        }
      else if ((0x80 == asc) && (0x02 == ascq))
        {
          DBG (MSG_ERR, "\t%d/%d/%d: ADF cover open \n", sense, asc, ascq);
          ret = SANE_STATUS_COVER_OPEN;
        }
      else if ((0x80 == asc) && (0x03 == ascq))
        {
          DBG (MSG_ERR, "\t%d/%d/%d: ADF empty \n", sense, asc, ascq);
          ret = SANE_STATUS_NO_DOCS;
        }
      else if ((0x80 == asc) && (0x04 == ascq))
        {
          DBG (MSG_ERR, "\t%d/%d/%d: Special-purpose paper detection\n", 
               sense, asc, ascq);
        }
      else if ((0x80 == asc) && (0x07 == ascq))
        {
          DBG (MSG_ERR, "\t%d/%d/%d: Double feed error\n", 
               sense, asc, ascq);
        }
      else if ((0x80 == asc) && (0x10 == ascq))
        {
          DBG (MSG_ERR, "\t%d/%d/%d: No Ink Cartridge is mounted\n", 
               sense, asc, ascq);
        }
      else if ((0x80 == asc) && (0x13 == ascq))
        {
          DBG (MSG_ERR, "\t%d/%d/%d: temporary lack of data\n", 
               sense, asc, ascq);
          ret = SANE_STATUS_DEVICE_BUSY;
        }
      else if ((0x80 == asc) && (0x14 == ascq))
        {
          DBG (MSG_ERR, "\t%d/%d/%d: Endorser paper detection failure\n", 
               sense, asc, ascq);
        }
      else
        {
          DBG (MSG_ERR, "\tUnknown - Sense=%d, ASC=%d, ASCQ=%d\n", 
               sense, asc, ascq);
        }
      break;

    case 0x4:                   /* Hardware Error */
      if ((0x80 == asc) && (0x01 == ascq))
        {
          DBG (MSG_ERR, "\t%d/%d/%d: FB motor fuse \n", sense, asc, ascq);
        }
      else if ((0x80 == asc) && (0x02 == ascq))
        {
          DBG (MSG_ERR, "\t%d/%d/%d: heater fuse \n", sense, asc, ascq);
        }
      else if ((0x80 == asc) && (0x04 == ascq))
        {
          DBG (MSG_ERR, "\t%d/%d/%d: ADF motor fuse \n", sense, asc, ascq);
        }
      else if ((0x80 == asc) && (0x05 == ascq))
        {
          DBG (MSG_ERR, "\t%d/%d/%d: mechanical alarm \n", sense, asc, ascq);
        }
      else if ((0x80 == asc) && (0x06 == ascq))
        {
          DBG (MSG_ERR, "\t%d/%d/%d: optical alarm \n", sense, asc, ascq);
        }
      else if ((0x80 == asc) && (0x07 == ascq))
        {
          DBG (MSG_ERR, "\t%d/%d/%d: FAN error \n", sense, asc, ascq);
        }
      else if ((0x80 == asc) && (0x08 == ascq))
        {
          DBG (MSG_ERR, "\t%d/%d/%d: abnormal option(IPC) \n", 
               sense, asc, ascq);
        }
      else if ((0x80 == asc) && (0x10 == ascq))
        {
          DBG (MSG_ERR, "\t%d/%d/%d: imprinter error \n", sense, asc, ascq);
        }
      else if ((0x44 == asc) && (0x00 == ascq))
        {
          DBG (MSG_ERR, "\t%d/%d/%d: abnormal internal target \n", 
               sense, asc, ascq);
        }
      else if ((0x47 == asc) && (0x00 == ascq))
        {
          DBG (MSG_ERR, "\t%d/%d/%d: SCSI parity error \n", 
               sense, asc, ascq);
        }
      else
        {
          DBG (MSG_ERR, "\tUnknown - Sense=%d, ASC=%d, ASCQ=%d\n", 
               sense, asc, ascq);
        }
      break;

    case 0x5:                   /* Illegal Request */
      if ((0x00 == asc) && (0x00 == ascq))
        {
          DBG (MSG_ERR, "\t%d/%d/%d: Page end is detected before reading\n",
               sense, asc, ascq);
          ret = SANE_STATUS_INVAL;
        }
      else if ((0x1a == asc) && (0x00 == ascq))
        {
          DBG (MSG_ERR, "\t%d/%d/%d: Parameter list error \n", 
               sense, asc, ascq);
          ret = SANE_STATUS_INVAL;
        }
      else if ((0x20 == asc) && (0x00 == ascq))
        {
          DBG (MSG_ERR, "\t%d/%d/%d: Invalid command \n", sense, asc, ascq);
          ret = SANE_STATUS_INVAL;
        }
      else if ((0x24 == asc) && (0x00 == ascq))
        {
          DBG (MSG_ERR, "\t%d/%d/%d: Invalid field in CDB \n", 
               sense, asc, ascq);
          ret = SANE_STATUS_INVAL;
        }
      else if ((0x25 == asc) && (0x00 == ascq))
        {
          DBG (MSG_ERR, "\t%d/%d/%d: Unsupported logical unit \n", 
               sense, asc, ascq);
          ret = SANE_STATUS_UNSUPPORTED;
        }
      else if ((0x26 == asc) && (0x00 == ascq))
        {

          DBG (MSG_ERR, "\t%d/%d/%d: Invalid field in parm list \n", 
               sense, asc, ascq);
	  /*hexdump (MSG_IO, "Sense", sensed_data, sensed_data[7]+8);*/
	  if (sensed_data[7]+8 >=17) 
	    {
	      
	      DBG(MSG_ERR, 
		  "offending byte is %x. (Byte %x in window descriptor block)\n", 
		  get_RS_offending_byte(sensed_data),
		  get_RS_offending_byte(sensed_data)-8);
	    }

          ret = SANE_STATUS_INVAL;
        }
      else if ((0x2C == asc) && (0x02 == ascq))
        {
          DBG (MSG_ERR, "\t%d/%d/%d: wrong window combination \n", 
               sense, asc, ascq);
          ret = SANE_STATUS_INVAL;
        }
      else
        {
          DBG (MSG_ERR, "\tUnknown - Sense=%d, ASC=%d, ASCQ=%d\n", 
               sense, asc, ascq);
        }
      break;

    case 0x6:                   /* Unit Attention */
      if ((0x00 == asc) && (0x00 == ascq))
        {
          DBG (MSG_ERR, "\t%d/%d/%d: UNIT ATTENTION \n", sense, asc, ascq);
        }
      else
        {
          DBG (MSG_ERR, "\tUnknown - Sense=%d, ASC=%d, ASCQ=%d\n", 
               sense, asc, ascq);
        }
      break;

    case 0xb:                   /* Aborted Command */
      if ((0x43 == asc) && (0x00 == ascq))
        {
          DBG (MSG_ERR, "\t%d/%d/%d: Message error \n", sense, asc, ascq);
        }
      else if ((0x80 == asc) && (0x01 == ascq))
        {
          DBG (MSG_ERR, "\t%d/%d/%d: Image transfer error \n", 
               sense, asc, ascq);
        }
      else
        {
          DBG (MSG_ERR, "\tUnknown - Sense=%d, ASC=%d, ASCQ=%d\n", 
               sense, asc, ascq);
        }
      break;

    default:
      DBG (MSG_ERR, "\tUnknown - Sense=%d, ASC=%d, ASCQ=%d\n", 
           sense, asc, ascq);
    }
  return ret;
}


/**
 * Sends an INQUIRY command to the device and stores the response in the
 * struct buffer.
 */
static void
do_inquiry (struct fujitsu *s)
{
  int i, tries = (s->connection == SANE_FUJITSU_USB) ? 5 : 1;
  size_t res;

  DBG (10, "do_inquiry\n");

  memset (s->buffer, '\0', 256);        /* clear buffer */
  set_IN_return_size (inquiryB.cmd, 96);
  set_IN_evpd (inquiryB.cmd, 0);
  set_IN_page_code (inquiryB.cmd, 0);
 
  hexdump (MSG_IO, "inquiry", inquiryB.cmd, inquiryB.size);

  for (i = 0; i < tries; i++) 
    {
      DBG(10, "try inquiry %d\n", i);
      if (   (do_cmd (s->connection, s->sfd, inquiryB.cmd, inquiryB.size,
		      s->buffer, 96, &res) == SANE_STATUS_GOOD)
	     && (res >= 96)   ) 
	{
	  break;
	}
      usleep(100000L);
    }
}

static SANE_Status
get_hardware_status (struct fujitsu *s)
{
  SANE_Status ret;
  DBG (10, "get_hardware_status\n");

  memset (s->buffer, '\0', 256);        /* clear buffer */
  set_HW_allocation_length (hw_statusB.cmd, 10);

  if (s->sfd < 0)
    {
      int sfd;

      if (s->connection == SANE_FUJITSU_USB) {
            DBG (10, "get_hardware_status opening USB device\n");
            if (sanei_usb_open (s->devicename, &sfd) != SANE_STATUS_GOOD) {
                DBG (5, "get_hardware_status: open failed\n");
                return SANE_STATUS_INVAL;
            }
      } else if (s->connection == SANE_FUJITSU_SCSI) {
            DBG (10, "get_hardware_status opening SCSI device\n");
            if (sanei_scsi_open (s->devicename, &sfd, scsi_sense_handler, 0)!=0) {
                DBG (5, "get_hardware_status: open failed\n");
                return SANE_STATUS_INVAL;
            }
      }      
      hexdump (MSG_IO, "get_hardware_status", 
               hw_statusB.cmd, hw_statusB.size);

      ret = do_cmd (s->connection, sfd, hw_statusB.cmd, hw_statusB.size,
                    s->buffer, 10, NULL);
      if (s->connection == SANE_FUJITSU_USB) {
            sanei_usb_close (sfd);
      } else if (s->connection == SANE_FUJITSU_SCSI) {
            sanei_scsi_close (sfd);
      }
    }
  else
    {
      hexdump (MSG_IO, "get_hardware_status", 
               hw_statusB.cmd, hw_statusB.size);

      ret = do_cmd (s->connection, s->sfd, hw_statusB.cmd, hw_statusB.size,
                    s->buffer, 10, NULL);
    }

  if (ret == SANE_STATUS_GOOD)
    {
      DBG (1, "B5 %d\n", get_HW_B5_present (s->buffer));
      DBG (1, "A4 %d \n", get_HW_A4_present (s->buffer));
      DBG (1, "B4 %d \n", get_HW_B4_present (s->buffer));
      DBG (1, "A3 %d \n", get_HW_A3_present (s->buffer));
      DBG (1, "HE %d\n", get_HW_adf_empty (s->buffer));
      DBG (1, "OMR %d\n", get_HW_omr (s->buffer));
      DBG (1, "ADFC %d\n", get_HW_adfc_open (s->buffer));
      DBG (1, "SLEEP %d\n", get_HW_sleep (s->buffer));
      DBG (1, "MF %d\n", get_HW_manual_feed (s->buffer));
      DBG (1, "Start %d\n", get_HW_start_button (s->buffer));
      DBG (1, "Ink empty %d\n", get_HW_ink_empty (s->buffer));
      DBG (1, "DFEED %d\n", get_HW_dfeed_detected (s->buffer));
      DBG (1, "SKEW %d\n", get_HW_skew_angle (s->buffer));
    }

  return ret;
}

static SANE_Status
getVitalProductData (struct fujitsu *s)
{
  SANE_Status ret;

  DBG (10, "get_vital_product_data\n");

  memset (s->buffer, '\0', 256);        /* clear buffer */
  set_IN_return_size (inquiryB.cmd, 0x64);
  set_IN_evpd (inquiryB.cmd, 1);
  set_IN_page_code (inquiryB.cmd, 0xf0);

  hexdump (MSG_IO, "get_vital_product_data", inquiryB.cmd, inquiryB.size);
  ret = do_cmd (s->connection, s->sfd, inquiryB.cmd, inquiryB.size, 
                s->buffer, 0x64, NULL);
  if (ret == SANE_STATUS_GOOD)
    {
      DBG (MSG_INFO, "standard options\n");
      DBG (MSG_INFO, "  basic x res: %d dpi\n",get_IN_basic_x_res (s->buffer));
      DBG (MSG_INFO, "  basic y res: %d dpi\n",get_IN_basic_y_res (s->buffer));
      DBG (MSG_INFO, "  step x res %d dpi\n", get_IN_step_x_res (s->buffer));
      DBG (MSG_INFO, "  step y res %d dpi\n", get_IN_step_y_res (s->buffer));
      DBG (MSG_INFO, "  max x res %d dpi\n", get_IN_max_x_res (s->buffer));
      DBG (MSG_INFO, "  max y res %d dpi\n", get_IN_max_y_res (s->buffer));
      DBG (MSG_INFO, "  min x res %d dpi\n", get_IN_min_x_res (s->buffer));
      DBG (MSG_INFO, "  max y res %d dpi\n", get_IN_min_y_res (s->buffer));
      DBG (MSG_INFO, "  window width %3.2f cm\n", 
           (double) get_IN_window_width (s->buffer)/
           (double) get_IN_basic_x_res(s->buffer) * 2.54);
      DBG (MSG_INFO, "  window length %3.2f cm\n",
           (double) get_IN_window_length (s->buffer)/
           (double) get_IN_basic_y_res(s->buffer) * 2.54);

      DBG (MSG_INFO, "functions:\n");
      DBG (MSG_INFO, "   binary scanning: %d\n", 
           get_IN_monochrome (s->buffer));
      DBG (MSG_INFO, "   gray scanning: %d\n", 
           get_IN_multilevel (s->buffer));
      DBG (MSG_INFO, "   half-tone scanning: %d\n", 
           get_IN_half_tone (s->buffer));
      DBG (MSG_INFO, "   color binary scanning: %d\n",
           get_IN_monochrome_rgb (s->buffer));
      DBG (MSG_INFO, "   color scanning: %d\n", 
           get_IN_multilevel_rgb (s->buffer));
      DBG (MSG_INFO, "   color half-tone scanning: %d\n",
           get_IN_half_tone_rgb (s->buffer));

      if (get_IN_page_length (s->buffer) > 0x19) 
        {
          DBG (MSG_INFO, "image memory: %d bytes\n",
               get_IN_buffer_bytes(s->buffer));
          DBG (MSG_INFO, "physical functions:\n");
          DBG (MSG_INFO, "   operator panel %d\n",
               get_IN_operator_panel (s->buffer));
          DBG (MSG_INFO, "   barcode %d\n", get_IN_barcode (s->buffer));
          DBG (MSG_INFO, "   endorser %d\n", get_IN_endorser (s->buffer));
          DBG (MSG_INFO, "   duplex %d\n", get_IN_duplex (s->buffer));
          DBG (MSG_INFO, "   flatbed %d\n", get_IN_flatbed (s->buffer));
          DBG (MSG_INFO, "   adf %d\n", get_IN_adf (s->buffer));

          DBG (MSG_INFO, "image control functions:\n");
          DBG (MSG_INFO, "   brightness steps: %d\n", 
               get_IN_brightness_steps (s->buffer));
          DBG (MSG_INFO, "   threshold steps: %d\n", 
               get_IN_threshold_steps (s->buffer));
          DBG (MSG_INFO, "   contrast steps: %d\n", 
               get_IN_contrast_steps (s->buffer));
          DBG (MSG_INFO, "   number of build in gamma patterns: %d\n", 
               get_IN_num_gamma (s->buffer));
          DBG (MSG_INFO, "   number of download gamma patterns: %d\n", 
               get_IN_num_gamma_download (s->buffer));

          DBG (MSG_INFO, "compression processing functions:\n");
          DBG (MSG_INFO, "   compression MR: %d\n", 
               get_IN_compression_MH (s->buffer));
          DBG (MSG_INFO, "   compression MR: %d\n", 
               get_IN_compression_MR (s->buffer));
          DBG (MSG_INFO, "   compression MMR: %d\n", 
               get_IN_compression_MMR (s->buffer));
          DBG (MSG_INFO, "   compression JBIG: %d\n", 
               get_IN_compression_JBIG (s->buffer));
          DBG (MSG_INFO, "   compression JPG1: %d\n",
               get_IN_compression_JPG_BASE (s->buffer));
          DBG (MSG_INFO, "   compression JPG2: %d\n",
               get_IN_compression_JPG_EXT (s->buffer));
          DBG (MSG_INFO, "   compression JPG3: %d\n",
               get_IN_compression_JPG_INDEP (s->buffer));
          
          DBG (MSG_INFO, "image processing functions:\n");
          DBG (MSG_INFO, "   black and white reverse: %d\n", 
               get_IN_ipc_bw_reverse(s->buffer));
          DBG (MSG_INFO, "   automatic binary DTC: %d\n", 
               get_IN_ipc_auto1(s->buffer));
          DBG (MSG_INFO, "   simplified DTC: %d\n", 
               get_IN_ipc_auto2(s->buffer));
          DBG (MSG_INFO, "   autline extraction: %d\n", 
               get_IN_ipc_outline_extraction(s->buffer));
          DBG (MSG_INFO, "   image emphasis: %d\n", 
               get_IN_ipc_image_emphasis(s->buffer));
          DBG (MSG_INFO, "   automatic separation: %d\n", 
               get_IN_ipc_auto_separation(s->buffer));
          DBG (MSG_INFO, "   mirror image: %d\n", 
               get_IN_ipc_mirroring(s->buffer));
          DBG (MSG_INFO, "   white level follower: %d\n", 
               get_IN_ipc_white_level_follow(s->buffer));
        }
      DBG (MSG_INFO, "\n\n");
    }
  return ret;
}

/**
 * Sends a command to the device. This calls do_scsi_cmd or do_usb_cmd.
 */
static int
do_cmd (Fujitsu_Connection_Type connection, int fd, unsigned char *cmd,
             int cmd_len, unsigned char *out, size_t req_out_len,
             size_t *res_out_len)
{
    if (connection == SANE_FUJITSU_SCSI) {
        return do_scsi_cmd(fd, cmd, cmd_len, out, req_out_len, res_out_len);
    }
    if (connection == SANE_FUJITSU_USB) {
        return do_usb_cmd(fd, cmd, cmd_len, out, req_out_len, res_out_len);
    }
    return SANE_STATUS_INVAL;
}

/**
 * Sends a SCSI command to the device. This is just a wrapper around 
 * sanei_scsi_cmd with some debug printing.
 */
static int
do_scsi_cmd (int fd, unsigned char *cmd,
             int cmd_len, unsigned char *out, size_t req_out_len,
             size_t *res_out_len)
{
  int ret;
  size_t ol = req_out_len;

  hexdump (IO_CMD, "<cmd<", cmd, cmd_len);

  ret = sanei_scsi_cmd (fd, cmd, cmd_len, out, &ol);
  if (res_out_len != NULL)
    {
      *res_out_len = ol;
    }

  if ((req_out_len != 0) && (req_out_len != ol))
    {
      DBG (MSG_ERR, "sanei_scsi_cmd: asked %lu bytes, got %lu\n",
           (u_long) req_out_len, (u_long) ol);
    }


  if (ret)
    {
      DBG (MSG_ERR, "sanei_scsi_cmd: returning 0x%08x\n", ret);
    }

  DBG (IO_CMD_RES, "sanei_scsi_cmd: returning %lu bytes:\n", (u_long) ol);

  if (out != NULL && ol != 0)
    {
      hexdump (IO_CMD_RES, ">rslt>", out, (ol > 0x60) ? 0x60 : ol);
    }

  return ret;
}

#define USB_CMD_HEADER_BYTES 19
#define USB_CMD_MIN_BYTES 31

/**
 * Sends a USB command to the device.
 */
static int
do_usb_cmd (int fd, unsigned char *cmd,
             int cmd_len, unsigned char *out, size_t req_out_len,
             size_t *res_out_len)
{
    int ret = SANE_STATUS_GOOD;
    size_t cnt, ol;
    int op_code = 0;
    int i, j;
    int tries = 0;
    int status_byte = 0;
    unsigned char buf[1024];

retry:
    hexdump (IO_CMD, "<cmd<", cmd, cmd_len);

    cmd_count++;

    if (cmd_len > 0) op_code = ((int)cmd[0]) & 0xff;

    if ((cmd_len+USB_CMD_HEADER_BYTES) > (int)sizeof(buf)) {
            /* Command too long. */
        return SANE_STATUS_INVAL;
    }
    buf[0] = (unsigned char)'C';
    for (i = 1; i < USB_CMD_HEADER_BYTES; i++) buf[i] = (unsigned char)0;
    memcpy(&buf[USB_CMD_HEADER_BYTES], cmd, cmd_len);
    for (i = USB_CMD_HEADER_BYTES+cmd_len; i < USB_CMD_MIN_BYTES; i++) {
        buf[i] = (unsigned char)0;
    }
        /* The SCAN command must be at least 32 bytes long. */
    if ( (op_code == SCAN) && (i < 32) ) {
        for (; i < 32; i++) buf[i] = (unsigned char)0;
    }

    for (j = 0; j < i;) {
        cnt = i-j;
            /* First URB has to be 31 bytes. */
            /* All other URBs must be 64 bytes (max) per URB. */
        if ( (j == 0) && (cnt > 31) ) cnt = 31; else if (cnt > 64) cnt = 64;
        hexdump (IO_CMD, "*** URB going out:", &buf[j], cnt);
        DBG (10, "try to write %u bytes\n", cnt);
        ret = sanei_usb_write_bulk(fd, &buf[j], &cnt);
        DBG (10, "wrote %u bytes\n", cnt);
        if (ret != SANE_STATUS_GOOD) break;
        j += cnt;
    }
    if (ret != SANE_STATUS_GOOD) {
        DBG (MSG_ERR, "*** Got error %d trying to write\n", ret);
    }

    ol = 0;
    if (ret == SANE_STATUS_GOOD) {
        if ( (out != NULL) && (req_out_len > 0) ) {
/*            while (ol < req_out_len) {*/
                cnt = (size_t)(req_out_len-ol);
                DBG (10, "try to read %u bytes\n", cnt);
                ret = sanei_usb_read_bulk(fd, &out[ol], &cnt);
                DBG (10, "read %u bytes\n", cnt);
                if (cnt > 0) {
                    hexdump (IO_CMD, "*** Data read:", &out[ol], cnt);
                }
                if (ret != SANE_STATUS_GOOD) {
                    DBG(MSG_ERR, "*** Got error %d trying to read\n", ret);
                }
/*                if (ret != SANE_STATUS_GOOD) break;*/
                ol += cnt;
            }
/*        }*/

        DBG(10, "*** Try to read CSW\n");
        cnt = 13;
        sanei_usb_read_bulk(fd, buf, &cnt);
        hexdump (IO_CMD, "*** Read CSW", buf, cnt);

	status_byte = ((int)buf[9]) & 0xff;
	if (status_byte != 0) {
	  DBG
	    (MSG_ERR,
	     "Got bad status: %2.2x op_code=%2.2x ret=%d req_out_len=%u ol=%u\n",
	     status_byte,
	     op_code,
	     ret,
	     req_out_len,
	     ol);
       }    
    }

        /* Auto-retry failed data reads, in case the scanner is busy. */
    if ( (op_code == READ) && (tries < 100) && (ol == 0) ) {
        usleep(100000L);
        tries++;
	DBG(MSG_ERR, "read failed; retry %d\n", tries);
        goto retry;
    }

  if (res_out_len != NULL)
    {
      *res_out_len = ol;
    }

  if ((req_out_len != 0) && (req_out_len != ol))
    {
      DBG (MSG_ERR, "do_usb_cmd: asked %lu bytes, got %lu\n",
           (u_long) req_out_len, (u_long) ol);
    }

  if (ret)
    {
      DBG (MSG_ERR, "do_usb_cmd: returning 0x%08x\n", ret);
    }

  DBG (IO_CMD_RES, "do_usb_cmd: returning %lu bytes:\n", (u_long) ol);

  if (out != NULL && ol != 0)
    {
      hexdump (IO_CMD_RES, ">rslt>", out, (ol > 0x60) ? 0x60 : ol);
    }

/*  if ( (op_code == OBJECT_POSITION) || (op_code == TEST_UNIT_READY) ) { */
  if (status_byte == 0x02) {	/* check condition */
      /* Issue a REQUEST_SENSE command and pass the resulting sense data
         into the scsi_sense_handler routine.  */
    memset(buf, 0, 18);
    if (do_usb_cmd
	  (fd,
	   request_senseB.cmd,
	   request_senseB.size,
	   buf,
	   18,
	   NULL) == SANE_STATUS_GOOD) {
      ret = scsi_sense_handler(fd, buf, NULL);
    }
  }

  return ret;
}

/**
 * Prints a hex dump of the given buffer onto the debug output stream.
 */
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
          sprintf (ptr, "%3.3x:", i);
          ptr += 4;
        }
      sprintf (ptr, " %2.2x", *p);
      ptr += 3;
    }
  *ptr = '\0';
  DBG (level, "%s\n", line);
}


/**
 * Creates a temporary file, opens it, and returns a file pointer for it.
 * (There seems to be no platform-independent way of doing this with mktemp
 * and friends.)
 * 
 * If no file can be created, NULL is returned. Will only create a file that
 * doesn't exist already. The function will also unlink ("delete") the file
 * immediately after it is created. In any "sane" programming environment this
 * has the effect that the file can be used for reading and writing as normal
 * but vanishes as soon as it's closed - so no cleanup required if the 
 * process dies etc.
 */
static int
makeTempFile ()
{
#ifdef P_tmpdir
  static const char *tmpdir = P_tmpdir;
#else
  static const char *tmpdir = "/tmp";
#endif
  char filename[PATH_MAX];
  unsigned int suffix = time (NULL) % 256;
  int try = 0;
  int file;

  for (try = 0; try < 10; try++)
    {
      sprintf (filename, "%s%csane-fujitsu-%d-%d", tmpdir, PATH_SEP,
               getpid (), suffix + try);
#if defined(_POSIX_SOURCE)
      file = open (filename, O_RDWR | O_CREAT | O_EXCL, S_IWUSR | S_IRUSR);
#else
      file = open (filename, O_RDWR | O_CREAT | O_EXCL);
#endif
      if (file != -1)
        {
          unlink (filename);
          DBG (10, "makeTempFile: file \"%s\" created.\n", filename);
          return file;
        }
    }
  return -1;
}



/**
 * Releases the scanner device on the SCSI bus.
 *
 * Should go through the following sequence:
 * OBJECT POSITION DISCHARGE
 *     GOOD
 * RELEASE UNIT
 *     GOOD
 */
static int
free_scanner (struct fujitsu *s)
{
  int ret;
  DBG (10, "free_scanner\n");

#if 0
  ret = object_position (s, SANE_FALSE);
  if (ret)
    return ret;

  wait_scanner (s);
#endif

  hexdump (MSG_IO, "release_unit", release_unitB.cmd, release_unitB.size);
  ret = do_cmd (s->connection, s->sfd, release_unitB.cmd,
                release_unitB.size, NULL, 0, NULL);
  if (ret)
    return ret;

  /* flaming hack cause some usb scanners (fi-4x20) fail 
     to work properly on next connection if an odd number
      of commands are sent to the scanner. */
  if(s->connection == SANE_FUJITSU_USB && cmd_count % 2){
    ret = get_hardware_status(s);
    if (ret)
      return ret;
  }

  DBG (10, "free_scanner: ok\n");
  return ret;
}

/**
 * Reserves the scanner on the SCSI bus for our exclusive access until released
 * by free_scanner.
 * 
 * Should go through the following command sequence:
 * TEST UNIT READY
 *     CHECK CONDITION  \
 * REQUEST SENSE         > These should be handled automagically by
 *     UNIT ATTENTION   /  the kernel if they happen (powerup/reset)
 * TEST UNIT READY
 *     GOOD
 * RESERVE UNIT
 *     GOOD
 */
static int
grab_scanner (struct fujitsu *s)
{
  int ret;

  DBG (10, "grab_scanner\n");
  wait_scanner (s);

  hexdump (MSG_IO, "reserve_unit", reserve_unitB.cmd, reserve_unitB.size);
  ret = do_cmd (s->connection, s->sfd, reserve_unitB.cmd,
                reserve_unitB.size, NULL, 0, NULL);
  if (ret)
    return ret;

  DBG (10, "grab_scanner: ok\n");
  return 0;
}

static int fujitsu_wait_scanner(Fujitsu_Connection_Type connection, int fd) 
{
  int ret = -1;
  int cnt = 0;

  DBG (10, "wait_scanner\n");

  while (ret != SANE_STATUS_GOOD)
    {
      hexdump (MSG_IO, "test_unit_ready", test_unit_readyB.cmd,
               test_unit_readyB.size);
      ret = do_cmd (connection, fd, test_unit_readyB.cmd,
                    test_unit_readyB.size, 0, 0, NULL);
      if (ret == SANE_STATUS_DEVICE_BUSY)
        {
          usleep (500000);      /* wait 0.5 seconds */
          /* 20 sec. max (prescan takes up to 15 sec. */
          if (cnt++ > 40)
            {
              DBG (MSG_ERR, "wait_scanner: scanner does NOT get ready\n");
              return -1;
            }
        }
      else if (ret != SANE_STATUS_GOOD)
        {
          DBG (MSG_ERR, "wait_scanner: unit ready failed (%s)\n",
               sane_strstatus (ret));
        }
    }
  DBG (10, "wait_scanner: ok\n");
  return SANE_STATUS_GOOD;
}

/** 
 *  wait_scanner spins until TEST_UNIT_READY returns 0 (GOOD)
 *  returns 0 on success,
 *  returns -1 on error or timeout
 */
static int
wait_scanner (struct fujitsu *s)
{
  return fujitsu_wait_scanner(s->connection, s->sfd);
}

/**
 * Issues the SCSI OBJECT POSITION command if an ADF is installed.
 */
static int
object_position (struct fujitsu *s, int i_load)
{
  int ret;
  DBG (10, "object_position: %s \n", (i_load==SANE_TRUE)?"load":"discharge");
  if (s->use_adf != SANE_TRUE)
    {
      return SANE_STATUS_GOOD;
    }
  if (s->has_adf == SANE_FALSE)
    {
      DBG (10, "object_position: ADF not present.\n");
      return SANE_STATUS_UNSUPPORTED;
    }
  memcpy (s->buffer, object_positionB.cmd, object_positionB.size);

  if (i_load == SANE_TRUE) 
    {
      set_OP_autofeed (s->buffer, OP_Feed);
    } 
  else 
    {
      set_OP_autofeed (s->buffer, OP_Discharge);
    }

  hexdump (MSG_IO, "object_position", s->buffer, object_positionB.size);
  ret = do_cmd (s->connection, s->sfd, s->buffer, object_positionB.size,
                NULL, 0, NULL);
  if (ret != SANE_STATUS_GOOD)
    return ret;
  wait_scanner (s);
  DBG (10, "object_position: ok\n");
  return ret;
}


/**
 * Issues OBJECT DISCHARGE command to spit out the paper.
 */
/*
static int
objectDischarge (struct fujitsu *s)
{
  int ret;

  DBG (10, "objectDischarge\n");
  if (s->use_adf != SANE_TRUE)
    {
      return SANE_STATUS_GOOD;
    }

  memcpy (s->buffer, object_positionB.cmd, object_positionB.size);
  if (s->model == MODEL_3092) {
     set_OP_autofeed (s->buffer, OP_Feed);
  }
  else {
     set_OP_autofeed (s->buffer, OP_Discharge);
  }
  hexdump (MSG_IO, "object_position", s->buffer, object_positionB.size);
  ret = do_cmd (s->connection, s->sfd, s->buffer, object_positionB.size,
                NULL, 0, NULL);
  wait_scanner (s);
  DBG (10, "objectDischarge: ok\n");
  return ret;
}
*/

/**
 * Performs reset scanner and reset the Window
 */
static SANE_Status
do_reset (struct fujitsu *scanner)
{
  int ret=SANE_STATUS_GOOD;

  DBG (10, "doReset\n");
  if (scanner->model == MODEL_3092) {
     ret = do_cmd (scanner->connection, scanner->sfd, reset_unitB.cmd,
                   reset_unitB.size, NULL, 0, NULL);
     if (ret)
       return ret;
  }
  return ret;
}

/**
 * Performs cleanup.
 */
static SANE_Status
do_cancel (struct fujitsu *scanner)
{
  DBG (10, "do_cancel\n");
  scanner->object_count = 0;
  scanner->eof = SANE_TRUE;

  if (scanner->object_count != 0)
    {
      close (scanner->default_pipe);
      if (scanner->duplex_mode == DUPLEX_BOTH)
        {
          close (scanner->duplex_pipe);
        }
    }

  if (scanner->reader_pid > 0)
    {
      int exit_status;
      DBG (10, "do_cancel: kill reader_process\n");

      /* Tell reader process to stop, then wait for it to react. 
       * If kill fails because the process doesn't exist for some 
       * reason, don't wait. This will mostly happen if the process 
       * has already terminated and been waited for - for example, 
       * in the duplex-with-tempfile scenario.
       */
      if (kill (scanner->reader_pid, SIGTERM) == 0)
        {
          while (wait (&exit_status) != scanner->reader_pid)
            DBG (50, "wait for scanner to stop\n");
        }
      scanner->reader_pid = 0;
    }

  if (scanner->sfd >= 0)
    {
      free_scanner (scanner);
      DBG (10, "do_cancel: close filedescriptor\n");
      if (scanner->connection == SANE_FUJITSU_USB) {
            sanei_usb_close (scanner->sfd);
      } else if (scanner->connection == SANE_FUJITSU_SCSI) {
            sanei_scsi_close (scanner->sfd);
      }
      scanner->sfd = -1;
    }

  return SANE_STATUS_CANCELLED;
}



/**
 * Convenience method to determine longest string size in a list.
 */
static size_t
maxStringSize (const SANE_String_Const strings[])
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
}


/**
 * Issues SCAN command.
 * 
 * (This doesn't actually read anything, it just tells the scanner
 * to start scanning.)
 */
static int
start_scan (struct fujitsu *s)
{
  int ret;

  unsigned char *command;
  int cmdsize;

  DBG (10, "start_scan\n");
  command = malloc (scanB.size + 2);
  memcpy (command, scanB.cmd, scanB.size);

  /*
   * 3096 seems to work by sending just the start scan command. 
   * 3091 requires one or two extra bytes, telling it which windows
   * to scan (using the same window identifiers as in SET WINDOW).
   * 
   * Currently, this sends the one extra byte to 3096 as well - may
   * have to be changed if it misbehaves. 
   */
  if (s->duplex_mode == DUPLEX_BOTH)
    {
      cmdsize = scanB.size + 2;
      set_SC_xfer_length (command, 2);
      command[cmdsize - 2] = WD_wid_front;
      command[cmdsize - 1] = WD_wid_back;
    }
  else
    {
      cmdsize = scanB.size + 1;
      set_SC_xfer_length (command, 1);
      command[cmdsize - 1] =
        (s->duplex_mode == DUPLEX_BACK) ? WD_wid_back : WD_wid_front;
    }

  hexdump (MSG_IO, "start_scan", command, cmdsize);
  ret = do_cmd (s->connection, s->sfd, command, cmdsize, NULL, 0, NULL);

  free (command);

  if (ret)
    {
      /* possible reasons:
       * - duplex reading without object_position
       * - invalid field in parameter list
       */
      
      return ret;
    }

  DBG (10, "start_scan:ok\n");
  return ret;
}

static int
set_mode_params (struct fujitsu *s)
{

  int ret;
  unsigned char *command;
  int i_cmd_size;
  int i_param_size;

  DBG (10, "set_mode_params\n");

  ret = 0;

  if (s->has_dropout_color)
    {

      memcpy (s->buffer, mode_selectB.cmd, mode_selectB.size);
      memcpy (s->buffer + mode_selectB.size, mode_select_headerB.cmd,
              mode_select_headerB.size);
      memcpy (s->buffer + mode_selectB.size + mode_select_headerB.size,
              mode_select_parameter_blockB.cmd,
              mode_select_parameter_blockB.size);



      command = s->buffer + mode_selectB.size + mode_select_headerB.size;
      i_cmd_size = 8;
      set_MSEL_len (command, i_cmd_size);

      set_MSEL_pagecode (command, MSEL_dropout_color);
      set_MSEL_dropout_front (command, s->dropout_color);
      set_MSEL_dropout_back (command, s->dropout_color);

      i_param_size = mode_select_headerB.size + i_cmd_size + 2;
      set_MSEL_xfer_length (s->buffer, i_param_size);

      hexdump (MSG_IO, "mode_select", s->buffer, 
               i_param_size + mode_selectB.size);

      ret = do_cmd (s->connection, s->sfd, s->buffer,
                    i_param_size + mode_selectB.size, NULL, 0, NULL);

    }


  if (!ret)
    {

      DBG (10, "set_mode_params: ok\n");
    }

  return ret;
}

static int
fujitsu_set_sleep_mode(struct fujitsu *s) 
{
  int ret;
  unsigned char *command;
  int     i_cmd_size;
  int     i_param_size;

  ret = SANE_STATUS_GOOD;

  if (s->model == MODEL_FI4x20 || s->model == MODEL_FI) /*...and others */
    {
        /* With the fi-series scanners, we have to use a 10-byte header
         * instead of a 4-byte header when communicating via USB.  This
         * may be a firmware bug. */
      scsiblk *headerB;
      int xfer_length_pos_adj;
      if (s->connection == SANE_FUJITSU_USB) {
          headerB = &mode_select_usb_headerB;
          xfer_length_pos_adj = mode_select_headerB.size-mode_select_usb_headerB.size;
      } else {
          headerB = &mode_select_headerB;
          xfer_length_pos_adj = 0;
      }
      memcpy (s->buffer, mode_selectB.cmd, mode_selectB.size);
      memcpy (s->buffer + mode_selectB.size, headerB->cmd,
              headerB->size);
      memcpy (s->buffer + mode_selectB.size + headerB->size,
              mode_select_parameter_blockB.cmd,
              mode_select_parameter_blockB.size);
  
      command = s->buffer + mode_selectB.size + headerB->size;
      i_cmd_size = 6;
      set_MSEL_len (command, i_cmd_size);

      set_MSEL_pagecode (command, MSEL_sleep);
      set_MSEL_sleep_mode(command, s->sleep_time);
  
      i_param_size = headerB->size + i_cmd_size + 2;
      set_MSEL_xfer_length (s->buffer, i_param_size + xfer_length_pos_adj);

      hexdump (MSG_IO, "mode_select", s->buffer, 
               i_param_size + mode_selectB.size);
  
      ret = do_cmd (s->connection, s->sfd, s->buffer,
                    i_param_size + mode_selectB.size, NULL, 0, NULL);

      if (!ret)
        {
          DBG (10, "set_sleep_mode: ok\n");
        }
    }

  return ret;
}


/**
 * Handler for termination signal. 
 */
static void
sigtermHandler (int signal)
{
  signal = signal;              /* get rid of compiler warning */
  sanei_scsi_req_flush_all ();  /* flush SCSI queue */
  _exit (SANE_STATUS_GOOD);
}


/**
 * A quite generic reader routine that just passes the scanner's data
 * through (i.e. stuffs it into the pipe).
 */
static unsigned int
reader_generic_passthrough (struct fujitsu *scanner, FILE * fp, int i_window_id)
{
  int status;
  unsigned int total_data_size, i_data_left, data_to_read;
  unsigned char *large_buffer;
  unsigned int large_buffer_size = 0;
  unsigned int i_data_read;

  i_data_left = scanner->bytes_per_scan_line * scanner->scan_height_pixels;
  total_data_size = i_data_left;

  large_buffer = scanner->buffer;
  large_buffer_size =
    scanner->scsi_buf_size -
    (scanner->scsi_buf_size % scanner->bytes_per_scan_line);

  do
    {
      data_to_read = (i_data_left < large_buffer_size) ? i_data_left : large_buffer_size;

      status = read_large_data_block (scanner, large_buffer, data_to_read, 
                                      i_window_id, &i_data_read);

      switch (status)
        {
        case SANE_STATUS_GOOD:
          break;
        case SANE_STATUS_EOF:
          DBG (10, "reader_process: EOM (no more data) length = %d\n",
               i_data_read);
          data_to_read -= i_data_read;
          i_data_left = data_to_read;
          break;
        default:
          DBG (MSG_ERR, 
               "reader_process: unable to get image data from scanner!\n");
          fclose (fp);
          return (0);
        }

      fwrite (large_buffer, 1, i_data_read, fp);
      fflush (fp);

      i_data_left -= data_to_read;
      DBG (10, "reader_process(generic): buffer of %d bytes read; %d bytes to go\n",
           data_to_read, i_data_left);
    }
  while (i_data_left);
  fclose (fp);

  return total_data_size;
}


/**
 * Reads a large data block from the device. If the requested length is larger 
 * than the device's scsi_buf_size (which is assumed to be the maximum amount
 * transferrable within one SCSI command), multiple SCSI commands will be 
 * issued to retrieve the full length.
 * 
 * @param s the scanner object
 * @param buffer where the data should go
 * @param length how many bytes to read
 * @param i_window_id 0=read data from front side 128= read data from back.
 *        m3091: set i_window_id=0 for both sides.
 * @return i_data_read the number of bytes returned by the scanner, is not
 *         always euqal to length.
 */
static int
read_large_data_block (struct fujitsu *s, unsigned char *buffer,
                       unsigned int length, int i_window_id, 
                       unsigned int *i_data_read)
{
  unsigned int data_left = length;
  unsigned int data_to_read;
  int status;
  unsigned char *myBuffer = buffer;
  size_t data_read;
  unsigned int i;

  *i_data_read = 0;
  current_scanner = s;
  DBG (MSG_IO_READ, "read_large_data_block requested %u bytes\n", length);
  do
    {
      data_to_read = 
        (data_left <= s->scsi_buf_size) ? data_left : s->scsi_buf_size;

      set_R_datatype_code (readB.cmd, R_datatype_imagedata);
      set_R_window_id (readB.cmd, i_window_id);
      set_R_xfer_length (readB.cmd, data_to_read);

      status = do_cmd (s->connection, s->sfd, readB.cmd, readB.size,
                       myBuffer, data_to_read, &data_read);    

      if (status == SANE_STATUS_EOF)
        {
          /* get the real number of bytes */
          data_read -= s->i_transfer_length;
          data_left = 0;
        }
      else if (status == SANE_STATUS_DEVICE_BUSY) 
	{
	  /* temporary lack of data? 
	   * try again.
	   */
	  data_read = 0;
	  usleep(100000L);
	}
      else if (status != SANE_STATUS_GOOD)
        {
          /* denotes error */
          DBG(MSG_ERR, "error reading data block status = %d\n", status);
	  data_read = 0;
          data_left = 0;
        }
      else 
        {
	  if (s->compress_type == WD_cmp_NONE && 
	      (s->color_mode == MODE_COLOR || 
	       s->color_mode == MODE_GRAYSCALE))
	    {
	      /* invert image if mode == Color || Gray */

	      for ( i = 0; i < data_read; i++ ) 
		{
		  *myBuffer = *myBuffer ^ 0xff;
		  myBuffer ++;
		}
	    } 
	  else 
	    {

	      myBuffer += data_read;
	    }
          data_left -= data_read;
        }

      *i_data_read += data_read;
    }
  while (data_left);

  if (*i_data_read != length) 
    {
      DBG(10, "data read = %d data requested = %d\n", *i_data_read, length);
    }

  current_scanner = NULL;
  return status;
}



/*
 * @@ Section 4 - generic routines with limited amount of model specific code
 */



/**
 * Finds out details about a scanner.
 * 
 * This routine will check if a certain device is a Fujitsu scanner and
 * determine the model. It also copies interesting data from the INQUIRY
 * command into the handle structure (e.g. whether or not an ADF has been 
 * detected on the scanner).
 * 
 * We will return a failure code of 1 if the device is not a supported 
 * scanner.
 */
static int
identify_scanner (struct fujitsu *s)
{
  char vendor[9];
  char product[17];
  char version[5];
  char *pp;

  DBG (1, "current version\n");
  DBG (10, "identify_scanner\n");

  vendor[8] = product[0x10] = version[4] = 0;

  do_inquiry (s);                /* get inquiry */
  if (get_IN_periph_devtype (s->buffer) != IN_periph_devtype_scanner)
    {
      DBG (5, "The device at \"%s\" is not a scanner.\n", s->devicename);
      return 1;
    }

  get_IN_vendor ((char*) s->buffer, vendor);
  get_IN_product ((char*) s->buffer, product);
  get_IN_version ((char*) s->buffer, version);

  vendor[8] = '\0';
  product[16] = '\0';
  version[4] = '\0';
  for (pp = vendor + 7; *pp == ' ' && pp >= vendor; pp--)
    *pp = '\0';
  for (pp = product + 15; *pp == ' ' && pp >= product; pp--)
    *pp = '\0';
  for (pp = version + 3; *pp == ' ' && pp >= version; pp--)
    *pp = '\0';

  if (strcmp ("FUJITSU", vendor))
    {
      DBG (5, "The device at \"%s\" is reported to be made by "
           "\"%s\" -\n", s->devicename, vendor);
      DBG (5, "this backend only supports Fujitsu products.\n");
      return 1;
    }

  /*
   * Ok, this is where we determine which type of scanner it is.
   * 
   * If a model has been defined in the config file, we just use that 
   * without looking. This is slightly inelegant at the moment since we 
   * should really allow to "force-model" a model _per_ _device_ instead 
   * of globally. 
   */
  if (forceModel != -1)
    {
      s->model = forceModel;
      DBG (5, "The device at \"%s\" says it's a model \"%s\" scanner.\n",
           s->devicename, product);
      DBG (5, "Automatic detection has been overridden by force-model "
           "option in the config file.\n");
    }
  else
    {
      s->model = modelMatch (product);
      if (s->model == -1)
        {
          DBG (5, "The device at \"%s\" says it's a model \"%s\" scanner -\n",
               s->devicename, product);
          DBG (5, "this backend does not support that model.\n");
          DBG (5,
               "You may try to use the \"force-model\" option in the config file\n");
          return 1;
        }
    }

  DBG (10, "Found %s scanner %s version %s on device %s, treating as %s\n",
       vendor, product, version, s->devicename,
       (s->model == MODEL_FORCE) ? "unknown" :
       (s->model == MODEL_3091) ? "3091" :
       (s->model == MODEL_3092) ? "3092" :
       (s->model == MODEL_3096) ? "3096" :
       (s->model == MODEL_3097) ? "3097" :
       (s->model == MODEL_3093) ? "3093" :
       (s->model == MODEL_4097) ? "4097" :
       (s->model == MODEL_FI4x20) ? "fi-4x20" :
       (s->model == MODEL_FI) ? "fi-series" : "unknown");

  strcpy (s->vendorName, vendor);
  strcpy (s->productName, product);
  strcpy (s->versionName, version);

  s->has_adf = SANE_FALSE;
  s->has_fb = SANE_FALSE;
  s->can_read_alternate = SANE_FALSE;
  s->read_mode = READ_MODE_PASS;
  s->ipc_present = SANE_FALSE;
  s->has_dropout_color = SANE_FALSE;
  s->cmp_present = SANE_FALSE;
  s->has_outline = SANE_FALSE;
  s->has_emphasis = SANE_FALSE;
  s->has_autosep = SANE_FALSE;
  s->has_mirroring = SANE_FALSE;
  s->has_imprinter = SANE_FALSE;
  s->has_threshold = SANE_TRUE;
  s->has_brightness = SANE_TRUE;
  s->has_contrast = SANE_TRUE;

  s->threshold_range = default_threshold_range;
  s->brightness_range = default_brightness_range;
  s->contrast_range = default_contrast_range;

  s->vpd_mode_list[0] = NULL;
  s->compression_mode_list[0] = cmp_none;


  if ((s->model == MODEL_3091) || (s->model == MODEL_3092))
    {
      /* Couldn't find a way to ask the scanner if it supports the ADF, 
       * so just hard-code it.
       */
      s->has_adf = SANE_TRUE;
      s->has_fb = SANE_FALSE;
      s->has_contrast = SANE_FALSE;
      s->has_reverse = SANE_TRUE;
      s->read_mode = READ_MODE_3091RGB;
      s->color_raster_offset = get_IN_raster (s->buffer);
      s->duplex_raster_offset = get_IN_frontback (s->buffer);
      s->duplex_present =
        (get_IN_duplex_3091 (s->buffer)) ? SANE_TRUE : SANE_FALSE;
    }
  else if ((s->model == MODEL_3096) || (s->model == MODEL_3093)
           || (s->model == MODEL_3097) || (s->model == MODEL_4097))
    {
      int i;
      s->has_adf = SANE_TRUE;
      s->has_fb = SANE_TRUE;
      s->color_raster_offset = 0;
      s->duplex_raster_offset = 0;

      /* no info about the 3093DG, seems to have trouble when trying to
       * alternate read in linear mode.
       */
      s->can_read_alternate = SANE_FALSE;
      s->read_mode = READ_MODE_PASS;

      s->ipc_present = (strchr (s->productName, 'i')) ? 1 : 0;
      s->cmp_present = (strchr (s->productName, 'm')) ? 1 : 0;
      s->duplex_present = (strchr (s->productName, 'd')) ? 1 : 0;
      if (s->ipc_present)
        {
          s->has_outline = 1;
          s->has_emphasis = 1;
          s->has_autosep = 1;
          s->has_mirroring = 1;
        }

      s->x_res_list[0] = 0;
      s->y_res_list[0] = 0;

      s->x_res_range.min = s->y_res_range.min = 50;
      s->x_res_range.quant = s->y_res_range.quant = 0;
      s->x_grey_res_range.min = s->y_grey_res_range.min = 50;
      s->x_grey_res_range.quant = s->y_grey_res_range.quant = 0;
      if (!strncmp (product, "M3093DG", 7) || !strncmp (product, "M4097D", 6))
        {

          if (!strncmp (product, "M4097D", 6))
            {
              s->can_read_alternate = SANE_TRUE;
              s->read_mode = READ_MODE_PASS;
            }

          for (i = 0; i <= res_list3[0]; i++)
            {
              s->y_res_list[i] = s->x_res_list[i] = res_list3[i];
            }
          for (i = 0; i <= res_list2[0]; i++)
            {
              s->x_res_list_grey[i] = s->y_res_list_grey[i] = res_list2[i];
            }

          if (s->cmp_present)
            {
              s->x_res_range.max = s->y_res_range.max = 800;
              s->x_res_range.quant = s->y_res_range.quant = 1;
              s->x_grey_res_range.max = s->y_grey_res_range.max = 400;
              s->x_grey_res_range.quant = s->y_grey_res_range.quant = 1;
            }
        }
      else
        {
          /* M3093GX/M3096GX */
          for (i = 0; i <= res_list1[0]; i++)
            {
              s->y_res_list[i] = s->x_res_list[i] = res_list1[i];
            }
          for (i = 0; i <= res_list0[0]; i++)
            {
              s->x_res_list_grey[i] = s->y_res_list_grey[i] = res_list0[i];
            }
          if (s->cmp_present)
            {
              s->x_res_range.max = s->y_res_range.max = 400;
              s->x_res_range.quant = s->y_res_range.quant = 1;
            }
        }
    }
  else if (s->model == MODEL_FI4x20)
    {
      s->can_read_alternate = SANE_TRUE;
      s->read_mode = READ_MODE_BGR;
      s->has_dropout_color = SANE_TRUE;
    }
  else if (s->model == MODEL_FI)
    {

      s->can_read_alternate = SANE_TRUE;
      s->read_mode = READ_MODE_RRGGBB;
      s->ipc_present = (strchr ((s->productName)+3, 'i')) ? 1 : 0;
      if (!strncmp (product, "fi-4340C", 8))
        {
          s->has_dropout_color = SANE_TRUE;
        }

      if (!strncmp (product, "fi-4530C", 8))
	{
          s->has_dropout_color = SANE_TRUE;
	  s->read_mode = READ_MODE_BGR;
	}
    }
  else
    {
      s->has_adf = SANE_FALSE;
    }


  /* initialize scanning- and adf paper size */

  s->x_range.min = SANE_FIX (0);
  s->x_range.quant = SANE_FIX (length_quant);

  s->y_range.min = SANE_FIX (0);
  s->y_range.quant = SANE_FIX (length_quant);

  s->adf_width_range.min = SANE_FIX (0);
  s->adf_width_range.quant = SANE_FIX (length_quant);

  s->adf_height_range.min = SANE_FIX (0);
  s->adf_height_range.quant = SANE_FIX (length_quant);


  if (0 == strncmp (product, "M3093", 5))
    {
      /* A4 scanner */
      if (0 == strncmp (product, "M3093DG", 7))
        {
          s->x_range.max = SANE_FIX (219);
          s->y_range.max = SANE_FIX (308);
          s->adf_width_range.max = SANE_FIX (219);
          s->adf_height_range.max = SANE_FIX (308);
        }
      else
        {
          s->x_range.max = SANE_FIX (215);
          s->y_range.max = SANE_FIX (308);
          s->adf_width_range.max = SANE_FIX (215);
          s->adf_height_range.max = SANE_FIX (308);
        }
    }
  else if (0 == strncmp (product, "M3092", 5)) 
                {
           s->x_range.max = SANE_FIX (215);
           s->y_range.max = SANE_FIX (305);
           s->adf_width_range.max = SANE_FIX (215);
           s->adf_height_range.max = SANE_FIX (305);
  }
  else
    {
      /* A3 scanner */
      s->x_range.max = SANE_FIX (308);
      s->y_range.max = SANE_FIX (438);
      s->adf_width_range.max = SANE_FIX (308);
      s->adf_height_range.max = SANE_FIX (438);
    }


  if (s->cmp_present)
    {

      s->compression_mode_list[1] = cmp_mh;
      s->compression_mode_list[2] = cmp_mr;
      s->compression_mode_list[3] = cmp_mmr;
      s->compression_mode_list[4] = NULL;
    }
  else
    {

      s->compression_mode_list[1] = NULL;
    }

  if (SANE_STATUS_GOOD == getVitalProductData (s))
    {
      /* This scanner supports vital product data.
       * Use this data to set dpi-lists etc.
       */
      int i, i_num_res;


      /* 
       * calculate maximum window with and window length.
       */
      s->x_range.max = SANE_FIX(get_IN_window_width(s->buffer) * MM_PER_INCH / 
                                get_IN_basic_x_res(s->buffer) );
      s->y_range.max = SANE_FIX(get_IN_window_length(s->buffer)* MM_PER_INCH / 
                                get_IN_basic_y_res(s->buffer) );


      DBG(DEBUG, "range: %d %d\n",s->x_range.max, s->y_range.max);
      /*
       * set possible resolutions.
       */
      s->x_res_range.quant = get_IN_step_x_res (s->buffer);
      s->y_res_range.quant = get_IN_step_y_res (s->buffer);
      s->x_res_range.max = get_IN_max_x_res (s->buffer);
      s->y_res_range.max = get_IN_max_y_res (s->buffer);
      s->x_res_range.min = get_IN_min_x_res (s->buffer);
      s->y_res_range.min = get_IN_min_y_res (s->buffer);

      i_num_res = 0;
      if (get_IN_std_res_60 (s->buffer))
        {
          i_num_res++;
          s->x_res_list[i_num_res] = 60;
        }
      if (get_IN_std_res_75 (s->buffer))
        {
          i_num_res++;
          s->x_res_list[i_num_res] = 75;
        }
      if (get_IN_std_res_100 (s->buffer))
        {
          i_num_res++;
          s->x_res_list[i_num_res] = 100;
        }
      if (get_IN_std_res_120 (s->buffer))
        {
          i_num_res++;
          s->x_res_list[i_num_res] = 120;
        }
      if (get_IN_std_res_150 (s->buffer))
        {
          i_num_res++;
          s->x_res_list[i_num_res] = 150;
        }
      if (get_IN_std_res_160 (s->buffer))
        {
          i_num_res++;
          s->x_res_list[i_num_res] = 160;
        }
      if (get_IN_std_res_180 (s->buffer))
        {
          i_num_res++;
          s->x_res_list[i_num_res] = 180;
        }
      if (get_IN_std_res_200 (s->buffer))
        {
          i_num_res++;
          s->x_res_list[i_num_res] = 200;
        }

      if (get_IN_std_res_240 (s->buffer))
        {
          i_num_res++;
          s->x_res_list[i_num_res] = 240;
        }
      if (get_IN_std_res_300 (s->buffer))
        {
          i_num_res++;
          s->x_res_list[i_num_res] = 300;
        }
      if (get_IN_std_res_320 (s->buffer))
        {
          i_num_res++;
          s->x_res_list[i_num_res] = 320;
        }
      if (get_IN_std_res_400 (s->buffer))
        {
          i_num_res++;
          s->x_res_list[i_num_res] = 400;
        }
      if (get_IN_std_res_480 (s->buffer))
        {
          i_num_res++;
          s->x_res_list[i_num_res] = 480;
        }
      if (get_IN_std_res_600 (s->buffer))
        {
          i_num_res++;
          s->x_res_list[i_num_res] = 600;
        }
      if (get_IN_std_res_800 (s->buffer))
        {
          i_num_res++;
          s->x_res_list[i_num_res] = 800;
        }
      if (get_IN_std_res_1200 (s->buffer))
        {
          i_num_res++;
          s->x_res_list[i_num_res] = 1200;
        }

      s->x_res_list[0] = i_num_res;

      for (i = 0; i <= i_num_res; i++)
        {
          s->y_res_list[i] = s->x_res_list[i];
          /* can't get resolutions for grey scale reading ???? */
        }


      if (get_IN_page_length (s->buffer) > 0x19)
        {
          /* VPD extended format present */
          s->has_adf = get_IN_adf (s->buffer);
          s->has_fb = get_IN_flatbed(s->buffer);
          s->duplex_present = get_IN_duplex (s->buffer);
          s->has_hw_status = get_IN_has_hw_status (s->buffer);
          s->has_outline = get_IN_ipc_outline_extraction (s->buffer);
          s->has_emphasis = get_IN_ipc_image_emphasis (s->buffer);
          s->has_autosep = get_IN_ipc_auto_separation (s->buffer);
          s->has_mirroring = get_IN_ipc_mirroring (s->buffer);
          s->has_white_level_follow =
            get_IN_ipc_white_level_follow (s->buffer);
          s->has_subwindow = get_IN_ipc_subwindow (s->buffer);
          s->has_reverse = get_IN_ipc_bw_reverse(s->buffer);

          /*
           * get threshold, brightness and contrast ranges.
           */
          s->has_threshold = (get_IN_threshold_steps(s->buffer) != 0);
          if (s->has_threshold) 
            {
              s->threshold_range.quant = (get_IN_threshold_steps(s->buffer) / 255);
            }
          s->has_contrast = (get_IN_contrast_steps(s->buffer) != 0);
          if (s->has_contrast) 
            {
              s->contrast_range.quant = (get_IN_contrast_steps(s->buffer) / 255);
            }
          s->has_brightness = (get_IN_brightness_steps(s->buffer) != 0);
          if (s->has_brightness) 
            {
              s->brightness_range.quant = (get_IN_brightness_steps(s->buffer)/255);
            }

          /*
           * setup mode list.
           * caution: vpd_mode_list has constant size (see header file)
           */
          i = 0;
          if (get_IN_monochrome (s->buffer))
            {
              
              s->vpd_mode_list[i] = mode_lineart;
              i++;
            }
          if (get_IN_half_tone (s->buffer))
            {
              
              s->vpd_mode_list[i] = mode_halftone;
              i++;
            }
          if (get_IN_multilevel (s->buffer))
            {
              
              s->vpd_mode_list[i] = mode_grayscale;
              i++;
            }
          if (get_IN_multilevel_rgb (s->buffer))
            {
              
              s->vpd_mode_list[i] = mode_color;
              i++;
            }
          s->vpd_mode_list[i] = NULL;
          

          /*
       * setup compression modes
       */
          i = 1;
	  /* compression_mode_list[0] = no compression */
          if (get_IN_compression_MH (s->buffer))
            {

              s->compression_mode_list[i] = cmp_mh;
	      s->cmp_present = SANE_TRUE;
              i++;
            }
          if (get_IN_compression_MR (s->buffer))
            {

              s->compression_mode_list[i] = cmp_mr;
	      s->cmp_present = SANE_TRUE;
              i++;
            }
          if (get_IN_compression_MMR (s->buffer))
            {

              s->compression_mode_list[i] = cmp_mmr;
	      s->cmp_present = SANE_TRUE;
              i++;
            }
          if (get_IN_compression_JBIG (s->buffer))
            {

              s->compression_mode_list[i] = cmp_jbig;
	      s->cmp_present = SANE_TRUE;
              i++;
            }

          if (get_IN_compression_JPG_BASE (s->buffer))
            {

              s->compression_mode_list[i] = cmp_jpg_base;
	      s->cmp_present = SANE_TRUE;
              i++;
            }
          if (get_IN_compression_JPG_EXT (s->buffer))
            {

              s->compression_mode_list[i] = cmp_jpg_ext;
	      s->cmp_present = SANE_TRUE;
              i++;
            }
          if (get_IN_compression_JPG_INDEP (s->buffer))
            {

              s->compression_mode_list[i] = cmp_jpg_indep;
	      s->cmp_present = SANE_TRUE;
              i++;
            }

          s->compression_mode_list[i] = NULL;
          s->cmp_present = (i > 1);

          s->has_imprinter = get_IN_imprinter(s->buffer);

	  s->has_gamma = get_IN_num_gamma(s->buffer);
	  s->num_download_gamma = get_IN_num_gamma_download (s->buffer);
        }
    }
  if (s->has_adf) 
    { 
      if (!strncmp (product, "fi-4530C", 8) ||
	  !strncmp (product, "M3091", 5) ||
	  !strncmp (product, "M3092", 5) ||
	  !strncmp (product, "fi-4120", 7) )
	{
	  /* These scanner don't support standard paper size specification
	   * in bye 0x35 of the window descriptor block
	   */
	  s->has_fixed_paper_size = SANE_FALSE;
	} 
      else
	{
	  s->has_fixed_paper_size = SANE_TRUE;
	}
      
    } 
  else 
    {
      s->has_fixed_paper_size = SANE_FALSE;
    }

  s->object_count = 0;

  DBG (10, "\tADF:%s present\n", s->has_adf ? "" : " not");
  DBG (10, "\tDuplex Unit:%s present\n", s->duplex_present ? "" : " not");
  DBG (10, "\tDuplex Raster Offset: %d\n", s->duplex_raster_offset);
  DBG (10, "\tColor Raster Offset: %d\n", s->color_raster_offset);

  return 0;

}


/**
 * Finds a matching MODEL_... code for a textual product name.
 *
 * Returns -1 if nothing can be found.
 */
static int
modelMatch (const char *product)
{
  if (strstr (product, "3091"))
    {
      return MODEL_3091;
    }
  else if (strstr (product, "3092"))
    {
      return MODEL_3092;
    }
  else if (strstr (product, "3096"))
    {
      return MODEL_3096;
    }
  else if (strstr (product, "3097"))
    {
      return MODEL_3097;
    }
  else if (strstr (product, "4097"))
    {
      return MODEL_4097;
    }
  else if (strstr (product, "3093"))
    {
      return MODEL_3093;
    }
  else if (strstr (product, "fi-4120") || strstr (product, "fi-4220"))
    {
      return MODEL_FI4x20;
    }
  else if (strstr (product, "fi-"))
    {
      return MODEL_FI;
    }

  return -1;
}

static int
imprinter(struct fujitsu *s) 
{
  int ret;
  unsigned char *command;
  int i_size;
  DBG (10, "imprinter\n");

  ret = 0;

  if (s->has_imprinter)
    {
      memcpy (s->buffer, imprinterB.cmd, imprinterB.size);
      memcpy (s->buffer + imprinterB.size,
              imprinter_descB.cmd, imprinter_descB.size);
      
      command = s->buffer + imprinterB.size;
      if (s->use_imprinter) {
        set_IMD_enable(command, IMD_enable);
      } else {
        set_IMD_enable(command, IMD_disable);
      }

      set_IMD_side(command, IMD_back);
      if (0) {

        /* 16 bit counter */
        set_IMD_format(command, IMD_16_bit);
        set_IMD_initial_count_16(command, s->imprinter_ctr_init);
        i_size = 4;
      } else {

        /* 24 bit counter */
        set_IMD_format(command, IMD_24_bit);
        set_IMD_initial_count_24(command, s->imprinter_ctr_init);
        i_size = 6;
      }
      
      set_IM_xfer_length(s->buffer, i_size);

      hexdump (MSG_IO, "imprinter", s->buffer, 
               imprinterB.size + i_size);
      

      if (s->sfd < 0) 
        {
          int sfd;

          if (s->connection == SANE_FUJITSU_USB) {
                DBG (10, "imprinter opening USB device\n");
                if (sanei_usb_open (s->devicename, &sfd) != SANE_STATUS_GOOD) {
                    DBG (5, "imprinter: open failed\n");
                    return SANE_STATUS_INVAL;
                }
          } else if (s->connection == SANE_FUJITSU_SCSI) {
                DBG (10, "imprinter opening SCSI device\n");
                if (sanei_scsi_open
                        (s->devicename, &sfd, scsi_sense_handler, 0) != 0) {
                    DBG (5, "imprinter: open failed\n");
                    return SANE_STATUS_INVAL;
                }
          }
      
          fujitsu_wait_scanner(s->connection, sfd);
          ret = do_cmd (s->connection, sfd, s->buffer, imprinterB.size + i_size,
                        NULL, 0, NULL);
          if (s->connection == SANE_FUJITSU_USB) {
                sanei_usb_close (sfd);
          } else if (s->connection == SANE_FUJITSU_SCSI) {
                sanei_scsi_close (sfd);
          }
        } 
      else 
        {
          ret = do_cmd (s->connection, s->sfd, s->buffer,
                        imprinterB.size + i_size,
                             NULL, 0, NULL);
        }

      if (!ret)
        {
          
          DBG (10, "imprinter: ok\n");
        }
    }

  return ret;
}

static int
fujitsu_send(struct fujitsu *s) 
{
  int ret;
  unsigned char *command;
  char *cp_string = "%05ud";
  int  i_strlen;
  int  i_yoffset;
  int  i_ymin;

  DBG (10, "send\n");

  ret = 0;

  if (s->has_imprinter && s->use_imprinter)
    {

      memcpy (s->buffer, sendB.cmd, sendB.size);
      memcpy (s->buffer + sendB.size,
              send_imprinterB.cmd,
              send_imprinterB.size);
      
      cp_string = s->imprinter_string;
      i_strlen = strlen(cp_string);
      command = s->buffer;
      set_S_datatype_code(command, S_datatype_imprinter_data);
      set_S_xfer_length(command, send_imprinterB.size + i_strlen);

      command += sendB.size;
      set_imprinter_cnt_dir(command, S_im_dir_inc);
      set_imprinter_lap24(command, S_im_ctr_16bit);
      set_imprinter_cstep(command, 1);

      /* minimum offset is 7mm. You have to set i_yoffset to 0 to
       * have 7mm offset.
       */
      i_ymin = FIXED_MM_TO_SCANNER_UNIT(7);
      i_yoffset = FIXED_MM_TO_SCANNER_UNIT(s->imprinter_y_offset);
      if (i_yoffset >= i_ymin) 
        {
          i_yoffset = i_yoffset - i_ymin;
        } 
      else 
        {
          i_yoffset = 0;
        }
      set_imprinter_uly(command, i_yoffset);

      set_imprinter_dirs(command, s->imprinter_direction);
      set_imprinter_string_length(command, i_strlen);

      command += send_imprinterB.size;
      memcpy(command, cp_string, i_strlen);

      hexdump (MSG_IO, "send", s->buffer, 
               i_strlen +  send_imprinterB.size + sendB.size);

      ret = do_cmd (s->connection, s->sfd, s->buffer,
                    i_strlen +  send_imprinterB.size + sendB.size,
                    NULL, 0, NULL);

    }

  if (!ret)
    {

      DBG (10, "send: ok\n");
    }

  return ret;
}

/**
 * This routine issues a SCSI SET WINDOW command to the scanner, using the
 * values currently in the scanner data structure.
 * 
 * The routine relies heavily on macros defined in fujitsu-scsi.h for manipulation
 * of the SCSI data block.
 */
static int
setWindowParam (struct fujitsu *s)
{
  unsigned char buffer[max_WDB_size];
  int ret;
  int scsiCommandLength = 0;
  scsiblk *setwinB;

  /* the amout of window data we're going to send. may have to be reduced 
   * depending on scanner model. */

  int windowDataSize;

  if ((s->model == MODEL_3091) || (s->model == MODEL_3092))
    {
      windowDataSize = 0x60;
    }
  else
    {
      windowDataSize = 0x40;
    }

  /* compiler was unhappy about me using "assert" ...? */
  if (windowDataSize > window_descriptor_blockB.size)
    {
      DBG (MSG_ERR,
           "backend is broken - cannot transfer larger window descriptor than defined in header file\n");
    }

  wait_scanner (s);
  DBG (10, "set_window_param\n");

  /* Initialize the buffer with pre-defined data from the header file. */

  memset (buffer, '\0', max_WDB_size);
  memcpy (buffer, window_descriptor_blockB.cmd,
          window_descriptor_blockB.size);

  set_WD_Xres (buffer, s->resolution_x);        /* x resolution in dpi */
  set_WD_Yres (buffer, s->resolution_y);        /* y resolution in dpi */

  set_WD_ULX (buffer, s->left_margin);
  set_WD_ULY (buffer, s->top_margin);
  set_WD_width (buffer, s->scan_width);
  set_WD_length (buffer, s->scan_height);

  set_WD_brightness (buffer, s->brightness);
  set_WD_threshold (buffer, s->threshold);
  set_WD_contrast (buffer, s->contrast);
  set_WD_composition (buffer, (s->color_mode == MODE_COLOR) ? WD_comp_RC :
                      (s->color_mode == MODE_GRAYSCALE) ? WD_comp_GS :
                      (s->color_mode ==
                       MODE_HALFTONE) ? WD_comp_HT : WD_comp_LA);

  if ((s->model == MODEL_3091) || (s->model == MODEL_3092))
    {
      set_WD_lamp_color (buffer, s->lamp_color);
    }
  else
    {
      set_WD_mirroring (buffer, s->mirroring);
    }

  /* bpp must be 8 even for RGB scans */
  set_WD_bitsperpixel (buffer,
                       (s->color_mode == MODE_COLOR) ? 8 : s->scanner_depth);

  set_WD_rif (buffer, s->reverse);

  set_WD_brightness (buffer, s->brightness);
  set_WD_threshold (buffer, s->threshold);
  set_WD_contrast (buffer, s->contrast);
  /* set_WD_halftone (buffer, s->halftone); */
  set_WD_bitorder (buffer, s->bitorder);
  set_WD_compress_type (buffer, s->compress_type);
  set_WD_compress_arg (buffer, s->compress_arg);
  set_WD_gamma (buffer, s->gamma);
  set_WD_outline (buffer, s->outline);
  set_WD_emphasis (buffer, s->emphasis);
  set_WD_auto_sep (buffer, s->auto_sep);
  set_WD_mirroring (buffer, s->mirroring);

  /* do not use these for 3091, it doesn't like non-zero in the
   * reserved fields */
  if ((s->model != MODEL_3091) && (s->model != MODEL_3092))
    {
      set_WD_var_rate_dyn_thresh (buffer, s->var_rate_dyn_thresh);
      set_WD_dtc_threshold_curve (buffer, s->dtc_threshold_curve);
      set_WD_dtc_selection (buffer, s->dtc_selection);
      set_WD_gradation (buffer, s->gradation);
      set_WD_smoothing_mode (buffer, s->smoothing_mode);
      set_WD_filtering (buffer, s->filtering);
      set_WD_background (buffer, s->background);
      set_WD_matrix2x2 (buffer, s->matrix2x2);
      set_WD_matrix3x3 (buffer, s->matrix3x3);
      set_WD_matrix4x4 (buffer, s->matrix4x4);
      set_WD_matrix5x5 (buffer, s->matrix5x5);
      set_WD_noise_removal (buffer, s->noise_removal);
      set_WD_white_level_follow (buffer, s->white_level_follow);
    }

  if ((s->model != MODEL_3091) && (s->model != MODEL_3092))
    {
      /* documentation says that 0xc0 is valid, but this isn't tru
       * for the M3093
       */
      set_WD_vendor_id_code (buffer, 0);
    }

  set_WD_paper_size (buffer, s->paper_size);
  set_WD_paper_orientation(buffer, s->paper_orientation);
  set_WD_paper_selection(buffer, s->paper_selection);
                
  if (s->paper_selection != WD_paper_SEL_STANDARD) {
    set_WD_paper_width_X (buffer, s->page_width);
    set_WD_paper_length_Y (buffer, s->page_height);
  } else {
    set_WD_paper_width_X (buffer, 0);
    set_WD_paper_length_Y (buffer, 0);
  }

  /* prepare SCSI buffer */

  /* The full command has the following format:
   * - the "command descriptor block" which includes the operation 
   *   code 24h (SET WINDOW) and the total amount of data following
   * - then, a window data header that basically only specifies the number  
   *   of bytes that make up one window description
   * - then, window data
   * that's it for simplex; for duplex, a second batch of window data 
   * follows without a header of its own. 
   */

    if ( ( (s->model == MODEL_FI4x20) || (s->model == MODEL_FI) ) && (s->connection == SANE_FUJITSU_USB) ) {
        setwinB = &set_usb_windowB;
    } else {
        setwinB = &set_windowB;
    }

  /* SET WINDOW command and command descriptor block. 
   * The transfer length will be filled in later using set_SW_xferlen.
   */
  memcpy (s->buffer, setwinB->cmd, setwinB->size);

  /* header for first set of window data */
  memcpy ((s->buffer + setwinB->size),
          window_parameter_data_blockB.cmd,
          window_parameter_data_blockB.size);

  /* set window data size */
  set_WPDB_wdblen ((s->buffer + setwinB->size), windowDataSize);

  if (s->duplex_mode == DUPLEX_BACK)
    {
      set_WD_wid (buffer, WD_wid_back);
    }
  else
    {
      set_WD_wid (buffer, WD_wid_front);
    }

  /* now copy window data itself */
  memcpy (s->buffer + setwinB->size + window_parameter_data_blockB.size,
          buffer, windowDataSize);

  /* when in duplex mode, add a second window */
  if (s->duplex_mode == DUPLEX_BOTH)
    {
      hexdump (MSG_IO, "Window set - front", buffer, windowDataSize);
      set_WD_wid (buffer, WD_wid_back);
      if ((s->model != MODEL_3091) && (s->model != MODEL_3092))
        {

          /* bytes 35 - 3D have to be 0 for back-side */
          set_WD_paper_size (buffer, WD_paper_UNDEFINED);
          set_WD_paper_orientation (buffer, WD_paper_PORTRAIT);
          set_WD_paper_selection (buffer, WD_paper_SEL_UNDEFINED);
          set_WD_paper_width_X (buffer, 0);
          set_WD_paper_length_Y (buffer, 0);
        }
      hexdump (MSG_IO, "Window set - back", buffer, windowDataSize);
      memcpy (s->buffer + setwinB->size +
              window_parameter_data_blockB.size + windowDataSize, buffer,
              windowDataSize);
      set_SW_xferlen (s->buffer,
                      (window_parameter_data_blockB.size +
                       2 * windowDataSize));
      scsiCommandLength =
        window_parameter_data_blockB.size + 2 * windowDataSize +
        setwinB->size;
    }
  else
    {
      hexdump (MSG_IO, "Window set", buffer, windowDataSize);
      set_SW_xferlen (s->buffer, (window_parameter_data_blockB.size +
                                  windowDataSize));
      scsiCommandLength =
        window_parameter_data_blockB.size + windowDataSize + setwinB->size;
    }

  ret = do_cmd (s->connection, s->sfd, s->buffer, scsiCommandLength,
                NULL, 0, NULL);
  if (ret)
    return ret;
  DBG (10, "set_window_param: ok\n");
  return ret;
}

/* 
 * Computes the scanner units used for the set_window command from the
 * millimeter units.
 */
void
calculateDerivedValues (struct fujitsu *scanner)
{

  DBG (12, "calculateDerivedValues\n");

  /* Convert the SANE_FIXED values for the scan area into 1/1200 inch 
   * scanner units */

  scanner->top_margin = FIXED_MM_TO_SCANNER_UNIT (scanner->val[OPT_TL_Y].w);
  scanner->left_margin = FIXED_MM_TO_SCANNER_UNIT (scanner->val[OPT_TL_X].w);
  scanner->scan_width = FIXED_MM_TO_SCANNER_UNIT (scanner->bottom_right_x) -
    scanner->left_margin;
  scanner->scan_height = FIXED_MM_TO_SCANNER_UNIT (scanner->bottom_right_y) -
    scanner->top_margin;

  DBG (12, "\ttop_margin: %u\n", scanner->top_margin);
  DBG (12, "\tleft_margin: %u\n", scanner->left_margin);
  DBG (12, "\tscan_width: %u\n", scanner->scan_width);
  DBG (12, "\tscan_height: %u\n", scanner->scan_height);

  /* increase scan width until there's a full number of bytes in a scan line
   * - only required if depth is not a multiple of 8.
   */
  if (scanner->scanner_depth % 8)
    {
      while ((scanner->scan_width * scanner->resolution_x *
              scanner->scanner_depth / 1200) % 8)
        {
          scanner->scan_width++;
        }
      DBG (12, "\tscan_width corrected: %u\n", scanner->scan_width);
    }


  /* sepcial relationship between X and Y must be maintained for 3096 */

  if (( (scanner->model == MODEL_3096) || (scanner->model == MODEL_3097) )&&
      ((scanner->left_margin + scanner->scan_width) >= 13200))
    {
      if (scanner->top_margin > 19830)
        scanner->top_margin = 19830;
      if ((scanner->top_margin + scanner->scan_height) > 19842)
        scanner->scan_height = 19842 - scanner->top_margin;
      DBG (12, "\ttop_margin corrected: %u\n", scanner->top_margin);
      DBG (12, "\tscan_height corrected: %u\n", scanner->scan_height);
    }

  scanner->scan_width_pixels =
    scanner->resolution_x * scanner->scan_width / 1200;
  scanner->scan_height_pixels =
    scanner->resolution_y * scanner->scan_height / 1200;
  scanner->bytes_per_scan_line =
    (scanner->scan_width_pixels * scanner->scanner_depth + 7) / 8;


  /* Now reverse our initial calculations and compute the "real" values so 
   * that the front-end has a chance to know. As an "added extra" we'll 
   * also round the height and width to the resolution that has been set; 
   * the scanner will accept height and width in 1/1200 units even at 1/75 
   * resolution but the front-end should know what it gets exactly.
   */
  scanner->rounded_top_left_x =
    SCANNER_UNIT_TO_FIXED_MM (scanner->left_margin);
  scanner->rounded_top_left_y =
    SCANNER_UNIT_TO_FIXED_MM (scanner->top_margin);
  scanner->rounded_bottom_right_x =
    SCANNER_UNIT_TO_FIXED_MM (scanner->left_margin +
                              scanner->scan_width_pixels * 1200 /
                              scanner->resolution_x);
  scanner->rounded_bottom_right_y =
    SCANNER_UNIT_TO_FIXED_MM (scanner->top_margin +
                              scanner->scan_height_pixels * 1200 /
                              scanner->resolution_y);

  /* And now something ugly. This is because the 3091 has this funny way 
   * of interspersing color values, you actually have to get more lines 
   * from the scanner than you want to process. Doesn't affect 
   * scan_height_pixels though!
   */
  if (((scanner->model == MODEL_3091)||(scanner->model == MODEL_3092)) && (scanner->color_mode == MODE_COLOR))
    {
      scanner->scan_height += 8 * scanner->color_raster_offset;
    }

  DBG (12, "calculateDerivedValues: ok\n");
}

/**
 * Coordinates the data acquisition from the scanner.
 *
 * This function is executed as a child process.
 */
static int
reader_process (struct fujitsu *scanner, int pipe_fd, int duplex_pipeFd)
{
  FILE *fp1, *fp2;
  sigset_t sigterm_set;
  struct SIGACTION act;
  time_t start_time, end_time;
  unsigned int total_data_size = 0;

  (void) time (&start_time);

  DBG (10, "reader_process started\n");

  sigemptyset (&sigterm_set);
  sigaddset (&sigterm_set, SIGTERM);
  memset (&act, 0, sizeof (act));
#ifdef _POSIX_SOURCE
  act.sa_handler = sigtermHandler;
#endif
  sigaction (SIGTERM, &act, 0);

  fp1 = fdopen (pipe_fd, "w");
  if (!fp1)
    {
      DBG (MSG_ERR, "reader_process: couldn't open pipe!\n");
      return 1;
    }
  if (scanner->duplex_mode == DUPLEX_BOTH)
    {
      fp2 = fdopen (duplex_pipeFd, "w");
      if (!fp2)
        {
          DBG (MSG_ERR, "reader_process: couldn't open pipe!\n");
          return 1;
        }
    }
  else
    {
      fp2 = 0;
    }

  DBG (10, "reader_process: starting to READ data\n");

  /**
   * Depending on the scanner model and mode, we select and call the appropiate
   * reader function.
   *
   * The main responsibility of the reader function is to accept raw SCSI data from the
   * scanner and process it in a way that yields a "plain" bit stream, containing the
   * values for pixels, row by row, from top-left to bottom-right, and if it's color,
   * blue after green after red value for each pixel.
   *
   * In gray and lineart modes, it is not unlikely that the scanners send data in
   * the right fashion already (so we can call the generic "passthrough" reader here), 
   * while color and/or duplex modes often require a bit of work ro get right.
   */
  switch (scanner->model)
    {
    case MODEL_3091:
    case MODEL_3092:
      if ((scanner->color_mode == MODE_COLOR)
          && (scanner->duplex_mode == DUPLEX_BOTH))
        total_data_size = reader3091ColorDuplex (scanner, fp1, fp2);
      else if (scanner->color_mode == MODE_COLOR)
        total_data_size = reader3091ColorSimplex (scanner, fp1);
      else if (scanner->duplex_mode == DUPLEX_BOTH)
        total_data_size = reader3091GrayDuplex (scanner, fp1, fp2);
      else
        total_data_size = reader_generic_passthrough (scanner, fp1, 0);
      break;

    case MODEL_FORCE:
    case MODEL_3096:
    case MODEL_3097:
    case MODEL_3093:
    case MODEL_4097:
    case MODEL_FI:
      if (scanner->duplex_mode == DUPLEX_BOTH)
        {
	  if (scanner->color_mode == MODE_COLOR)
	    {
	      if (scanner->can_read_alternate) {
		total_data_size = reader_duplex_alternate (scanner, fp1, fp2);
	      }
	      else {
		  total_data_size = reader_duplex_sequential (scanner, fp1, fp2);
	      }
	    }
	  else
	    {
	      if (scanner->can_read_alternate)
		{
		  total_data_size =
		    reader_gray_duplex_alternate (scanner, fp1, fp2);
		}
	      else
		{
		  total_data_size =
		    reader_gray_duplex_sequential (scanner, fp1, fp2);
		}
	    }
	}
      else
        {
	  if (scanner->color_mode == MODE_COLOR)
	    {
	      total_data_size = reader_simplex (scanner, fp1, 0);
	    }
	  else 
	    {
	      total_data_size = reader_generic_passthrough (scanner, fp1, 0);
	    }
        }
      break;


    case MODEL_FI4x20:
      if (scanner->duplex_mode == DUPLEX_BOTH) {

        if (scanner->color_mode == MODE_COLOR){
          if (scanner->can_read_alternate) {
            total_data_size = reader_duplex_alternate (scanner, fp1, fp2);
          }
          else {
            total_data_size = reader_duplex_sequential (scanner, fp1, fp2);
          }
        }
        else {
          if (scanner->can_read_alternate) {
            total_data_size = reader_gray_duplex_alternate (scanner, fp1, fp2);
          }
          else {
            total_data_size = reader_gray_duplex_sequential (scanner, fp1, fp2);
          }
        }

      }
      else { /*simplex*/

        if (scanner->color_mode == MODE_COLOR){
          total_data_size = reader_simplex (scanner, fp1, 0);
        }
        else {
          total_data_size = reader_generic_passthrough (scanner, fp1, 0);
        }

      }
      break;

    default:
      DBG (5, "reader_process: no implementation for this scanner model\n");
    }

  (void) time (&end_time);
  if (end_time == start_time)
    end_time++;

  DBG (10, "reader_process: finished, throughput was %lu bytes/second\n",
       (u_long) total_data_size / (end_time - start_time));

  return 0;

}

/**
 * This function presets the "option" array in the given data structure
 * according to the capabilities of the detected scanner model.
 */
static SANE_Status
init_options (struct fujitsu *scanner)
{
  int i;
  SANE_Option_Descriptor *opt;
  SANE_String_Const *valueList;

  DBG (10, "init_options\n");

  /*
   * set defaults for everything
   */
  memset (scanner->opt, 0, sizeof (scanner->opt));
  for (i = 0; i < NUM_OPTIONS; ++i)
    {
      scanner->opt[i].name = "filler";
      scanner->opt[i].size = sizeof (SANE_Word);
      scanner->opt[i].cap = SANE_CAP_INACTIVE;
    }

  opt = scanner->opt + OPT_NUM_OPTS;
  opt->title = SANE_TITLE_NUM_OPTIONS;
  opt->desc = SANE_DESC_NUM_OPTIONS;
  opt->cap = SANE_CAP_SOFT_DETECT;

  /* "Mode" group -------------------------------------------------------- */

  opt = scanner->opt + OPT_MODE_GROUP;
  opt->title = "Scan Mode";
  opt->desc = "";
  opt->type = SANE_TYPE_GROUP;
  opt->constraint_type = SANE_CONSTRAINT_NONE;

  /* source */
  opt = scanner->opt + OPT_SOURCE;
  opt->name = SANE_NAME_SCAN_SOURCE;
  opt->title = SANE_TITLE_SCAN_SOURCE;
  opt->desc = SANE_DESC_SCAN_SOURCE;
  opt->type = SANE_TYPE_STRING;
  valueList =
    (scanner->has_adf) ? sourceListWithADF : sourceListWithoutADF;
  opt->size = maxStringSize (valueList);
  opt->constraint_type = SANE_CONSTRAINT_STRING_LIST;
  opt->constraint.string_list = valueList;
  opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;

  /* scan mode */
  opt = scanner->opt + OPT_MODE;
  opt->name = SANE_NAME_SCAN_MODE;
  opt->title = SANE_TITLE_SCAN_MODE;
  opt->desc = SANE_DESC_SCAN_MODE;
  opt->type = SANE_TYPE_STRING;
  if (scanner->vpd_mode_list[0] == NULL)
    {

      valueList = ((scanner->model == MODEL_3091)||(scanner->model == MODEL_3092)) ? modeList3091 :
        ((scanner->model == MODEL_3096) ||
         (scanner->model == MODEL_3093) ||
         (scanner->model == MODEL_3097) ||
         (scanner->model ==
          MODEL_4097)) ? modeList3096 : scanner->vpd_mode_list;
      /* add others */
      if (valueList[0] == NULL)
        {

          DBG (MSG_ERR, "no modes found for this scanner\n");
        }
    }
  else
    {

      valueList = scanner->vpd_mode_list;
    }
  opt->size = maxStringSize (valueList);
  opt->constraint_type = SANE_CONSTRAINT_STRING_LIST;
  opt->constraint.string_list = valueList;
  opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;


  /* duplex mode */
  opt = scanner->opt + OPT_DUPLEX;
  opt->name = "duplex";
  opt->title = "Duplex Mode";
  opt->desc = "Select if you want the front or back to be scanned, or both.";
  opt->type = SANE_TYPE_STRING;
  /* 3091 doesn't support "back side only" */
  valueList = (!scanner->duplex_present) ? duplexListWithoutDuplex :
    ((scanner->model == MODEL_3091)||(scanner->model == MODEL_3092)) ? duplexListSpecial : duplexListWithDuplex;
  opt->size = maxStringSize (valueList);
  opt->constraint_type = SANE_CONSTRAINT_STRING_LIST;
  opt->constraint.string_list = valueList;
  opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;

  /* resolution */
  opt = scanner->opt + OPT_X_RES;
  opt->name = SANE_NAME_SCAN_X_RESOLUTION;
  opt->title = SANE_TITLE_SCAN_X_RESOLUTION;
  opt->desc = SANE_DESC_SCAN_X_RESOLUTION;
  opt->type = SANE_TYPE_INT;
  opt->constraint_type = SANE_CONSTRAINT_WORD_LIST;
  opt->constraint.word_list = scanner->x_res_list;
  opt->unit = SANE_UNIT_DPI;
  opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;

  opt = scanner->opt + OPT_Y_RES;
  opt->name = SANE_NAME_SCAN_Y_RESOLUTION;
  opt->title = SANE_TITLE_SCAN_Y_RESOLUTION;
  opt->desc = SANE_DESC_SCAN_Y_RESOLUTION;
  opt->type = SANE_TYPE_INT;
  opt->constraint_type = SANE_CONSTRAINT_WORD_LIST;
  opt->constraint.word_list = scanner->y_res_list;
  opt->unit = SANE_UNIT_DPI;
  opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;

  /* the constraints for the resolution options are set in the "setMode..."
   * functions because they depend on the selected scan mode.
   * The "setDefaults..." function is called during initialization and will
   * make sure that "setMode..." ist called.
   */

  /* "Geometry" group ---------------------------------------------------- */

  opt = scanner->opt + OPT_GEOMETRY_GROUP;
  opt->title = "Geometry";
  opt->desc = "";
  opt->type = SANE_TYPE_GROUP;
  opt->constraint_type = SANE_CONSTRAINT_NONE;

  /* top-left x */
  opt = scanner->opt + OPT_TL_X;
  opt->name = SANE_NAME_SCAN_TL_X;
  opt->title = SANE_TITLE_SCAN_TL_X;
  opt->desc = SANE_DESC_SCAN_TL_X;
  opt->type = SANE_TYPE_FIXED;
  opt->unit = SANE_UNIT_MM;
  opt->constraint_type = SANE_CONSTRAINT_RANGE;
  switch (scanner->model)
    {
    case MODEL_3091:

      opt->constraint.range = &rangeX3091;
      break;
    case MODEL_3092:

      opt->constraint.range = &rangeX3092;
      break;
    case MODEL_3096:

      opt->constraint.range = &rangeX3096;
      break;
    default:

      opt->constraint.range = &(scanner->x_range);
    }
  /*
     opt->constraint.range = (scanner->model == MODEL_3091) ? &rangeX3091 : 
     (scanner->model == MODEL_3096) ? &rangeX3096 : NULL; 
   */

  opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;

  /* top-left y */
  opt = scanner->opt + OPT_TL_Y;
  opt->name = SANE_NAME_SCAN_TL_Y;
  opt->title = SANE_TITLE_SCAN_TL_Y;
  opt->desc = SANE_DESC_SCAN_TL_Y;
  opt->type = SANE_TYPE_FIXED;
  opt->unit = SANE_UNIT_MM;
  opt->constraint_type = SANE_CONSTRAINT_RANGE;
  switch (scanner->model)
    {
    case MODEL_3091:

      opt->constraint.range = &rangeY3091;
      break;
    case MODEL_3092:

      opt->constraint.range = &rangeY3092;
      break;
    case MODEL_3096:

      opt->constraint.range = &rangeY3096;
      break;
    default:

      opt->constraint.range = &(scanner->y_range);
    }
  /*
     opt->constraint.range = (scanner->model == MODEL_3091) ? &rangeY3091 :
     (scanner->model == MODEL_3096) ? &rangeY3096 : NULL;
   */
  opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;

  /* bottom-right x */
  opt = scanner->opt + OPT_BR_X;
  opt->name = SANE_NAME_SCAN_BR_X;
  opt->title = SANE_TITLE_SCAN_BR_X;
  opt->desc = SANE_DESC_SCAN_BR_X;
  opt->type = SANE_TYPE_FIXED;
  opt->unit = SANE_UNIT_MM;
  opt->constraint_type = SANE_CONSTRAINT_RANGE;
  switch (scanner->model)
    {
    case MODEL_3091:

      opt->constraint.range = &rangeX3091;
      break;
    case MODEL_3092:

      opt->constraint.range = &rangeX3092;
      break;
    case MODEL_3096:

      opt->constraint.range = &rangeX3096;
      break;
    default:

      opt->constraint.range = &(scanner->x_range);
    }

  /*
     opt->constraint.range = (scanner->model == MODEL_3091) ? &rangeX3091 :
     (scanner->model == MODEL_3096) ? &rangeX3096 : NULL; 
   */

  opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;

  /* bottom-right y */
  opt = scanner->opt + OPT_BR_Y;
  opt->name = SANE_NAME_SCAN_BR_Y;
  opt->title = SANE_TITLE_SCAN_BR_Y;
  opt->desc = SANE_DESC_SCAN_BR_Y;
  opt->type = SANE_TYPE_FIXED;
  opt->unit = SANE_UNIT_MM;
  opt->constraint_type = SANE_CONSTRAINT_RANGE;
  switch (scanner->model)
    {
    case MODEL_3091:

      opt->constraint.range = &rangeY3091;
      break;
    case MODEL_3092:

      opt->constraint.range = &rangeY3092;
      break;
    case MODEL_3096:

      opt->constraint.range = &rangeY3096;
      break;
    default:

      opt->constraint.range = &(scanner->y_range);
    }
  /*
     opt->constraint.range = (scanner->model == MODEL_3091) ? &rangeY3091 :
     (scanner->model == MODEL_3096) ? &rangeY3096 : NULL; 
   */
  opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;

  /* page width */
  opt = scanner->opt + OPT_PAGE_WIDTH;
  opt->name = "pagewidth";
  opt->title = "Width of the paper in the scanner";
  opt->desc =
    "Usually it's ok to just leave it at the maximum value all the time, but the ADF behavior may change if this value is modified.";
  opt->type = SANE_TYPE_FIXED;
  opt->unit = SANE_UNIT_MM;
  opt->constraint_type = SANE_CONSTRAINT_RANGE;
  opt->constraint.range = &scanner->adf_width_range;
  opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;

  /* page height */
  opt = scanner->opt + OPT_PAGE_HEIGHT;
  opt->name = "pageheight";
  opt->title = "Height of the paper in the scanner";
  opt->desc =
    "Usually it's ok to just leave it at the maximum value all the time, but the ADF behavior may change if this value is modified.";
  opt->type = SANE_TYPE_FIXED;
  opt->unit = SANE_UNIT_MM;
  opt->constraint_type = SANE_CONSTRAINT_RANGE;
  opt->constraint.range = &scanner->adf_height_range;
  opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;

  /* "Enhancement" group ------------------------------------------------- */

  opt = scanner->opt + OPT_ENHANCEMENT_GROUP;
  opt->title = "Enhancement";
  opt->desc = "";
  opt->type = SANE_TYPE_GROUP;
  opt->constraint_type = SANE_CONSTRAINT_NONE;

  opt = scanner->opt + OPT_BRIGHTNESS;
  opt->name = SANE_NAME_BRIGHTNESS;
  opt->title = SANE_TITLE_BRIGHTNESS;
  opt->desc = SANE_DESC_BRIGHTNESS;
  opt->type = SANE_TYPE_INT;
  opt->unit = SANE_UNIT_NONE;
  opt->constraint_type = SANE_CONSTRAINT_RANGE;
  opt->constraint.range = &scanner->brightness_range;
  if (scanner->has_brightness)
    {
      opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    } 
  else
    {
      opt->cap = SANE_CAP_INACTIVE;
    }

  opt = scanner->opt + OPT_AVERAGING;
  opt->name = "averaging";
  opt->title = "Averaging";
  opt->desc = "Averaging";
  opt->type = SANE_TYPE_BOOL;
  opt->unit = SANE_UNIT_NONE;

  opt = scanner->opt + OPT_CONTRAST;
  opt->name = SANE_NAME_CONTRAST;
  opt->title = SANE_TITLE_CONTRAST;
  opt->desc = SANE_DESC_CONTRAST;
  opt->type = SANE_TYPE_INT;
  opt->unit = SANE_UNIT_NONE;
  opt->constraint_type = SANE_CONSTRAINT_RANGE;
  opt->constraint.range = &scanner->contrast_range;
  if (scanner->has_contrast)
    {
      opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    } 
  else
    {
      opt->cap = SANE_CAP_INACTIVE;
    }

  opt = scanner->opt + OPT_THRESHOLD;
  opt->name = SANE_NAME_THRESHOLD;
  opt->title = SANE_TITLE_THRESHOLD;
  opt->desc = SANE_DESC_THRESHOLD;
  opt->type = SANE_TYPE_INT;
  opt->unit = SANE_UNIT_NONE;
  opt->constraint_type = SANE_CONSTRAINT_RANGE;
  opt->constraint.range = &scanner->threshold_range;
  if (scanner->has_threshold)
    {
      opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    } 
  else
    {
      opt->cap = SANE_CAP_INACTIVE;
    }


  opt = scanner->opt + OPT_MIRROR_IMAGE;
  opt->name = "mirroring";
  opt->title = "mirror image";
  opt->desc = "mirror image";
  opt->type = SANE_TYPE_BOOL;
  opt->unit = SANE_UNIT_NONE;
  if (scanner->has_mirroring)
    opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  else
    opt->cap = SANE_CAP_INACTIVE;

  scanner->opt[OPT_RIF].name = "rif";
  scanner->opt[OPT_RIF].title = "rif";
  scanner->opt[OPT_RIF].desc = "reverse image format";
  scanner->opt[OPT_RIF].type = SANE_TYPE_BOOL;
  scanner->opt[OPT_RIF].unit = SANE_UNIT_NONE;
  if (scanner->has_reverse)
    scanner->opt[OPT_RIF].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  else
    opt->cap = SANE_CAP_INACTIVE;

  scanner->opt[OPT_AUTOSEP].name = "autoseparation";
  scanner->opt[OPT_AUTOSEP].title = "automatic separation";
  scanner->opt[OPT_AUTOSEP].desc = "automatic separation";
  scanner->opt[OPT_AUTOSEP].type = SANE_TYPE_BOOL;
  scanner->opt[OPT_AUTOSEP].unit = SANE_UNIT_NONE;
  if (scanner->has_autosep)
    scanner->opt[OPT_AUTOSEP].cap =
      SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;

  scanner->opt[OPT_OUTLINE_EXTRACTION].name = "outline";
  scanner->opt[OPT_OUTLINE_EXTRACTION].title = "outline extraction";
  scanner->opt[OPT_OUTLINE_EXTRACTION].desc = "enable outline extraction";
  scanner->opt[OPT_OUTLINE_EXTRACTION].type = SANE_TYPE_BOOL;
  scanner->opt[OPT_OUTLINE_EXTRACTION].unit = SANE_UNIT_NONE;
  if (scanner->has_outline)
    scanner->opt[OPT_OUTLINE_EXTRACTION].cap = SANE_CAP_SOFT_SELECT |
      SANE_CAP_SOFT_DETECT;

  scanner->opt[OPT_COMPRESSION].name = "compression";
  scanner->opt[OPT_COMPRESSION].title = "compress image";
  scanner->opt[OPT_COMPRESSION].desc = "use hardware compression of scanner";
  scanner->opt[OPT_COMPRESSION].type = SANE_TYPE_STRING;
  scanner->opt[OPT_COMPRESSION].size =
    maxStringSize (scanner->compression_mode_list);
  scanner->opt[OPT_COMPRESSION].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  scanner->opt[OPT_COMPRESSION].constraint.string_list =
    scanner->compression_mode_list;
  DBG(1, "init_options: set compression %d\n", scanner->cmp_present);
  if (scanner->cmp_present) 
    {
  DBG(1, "ok compression %d\n", scanner->cmp_present);
      scanner->opt[OPT_COMPRESSION].cap = SANE_CAP_SOFT_SELECT |
	SANE_CAP_SOFT_DETECT;
    }

  scanner->opt[OPT_COMPRESSION_ARG].name = "compressionarg";
  scanner->opt[OPT_COMPRESSION_ARG].title = "compression argument";
  scanner->opt[OPT_COMPRESSION_ARG].desc = "compression quality (JPG)";
  scanner->opt[OPT_COMPRESSION_ARG].type = SANE_TYPE_INT;
  scanner->opt[OPT_COMPRESSION_ARG].unit = SANE_UNIT_NONE;
  scanner->opt[OPT_COMPRESSION_ARG].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_COMPRESSION_ARG].constraint.range = &jpg_quality_range;
  if (scanner->cmp_present)
    scanner->opt[OPT_COMPRESSION_ARG].cap = SANE_CAP_SOFT_SELECT |
      SANE_CAP_SOFT_DETECT;
  

  scanner->opt[OPT_GAMMA].name = "gamma";
  scanner->opt[OPT_GAMMA].title = "gamma";
  scanner->opt[OPT_GAMMA].desc =
    "specifies the gamma pattern number for the line art or the halftone";
  scanner->opt[OPT_GAMMA].type = SANE_TYPE_STRING;
  scanner->opt[OPT_GAMMA].size = maxStringSize (gamma_mode_list);
  scanner->opt[OPT_GAMMA].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  scanner->opt[OPT_GAMMA].constraint.string_list = gamma_mode_list;
  if (scanner -> has_gamma)
    scanner->opt[OPT_GAMMA].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  else 
    scanner->opt[OPT_GAMMA].cap = SANE_CAP_INACTIVE;

  scanner->opt[OPT_EMPHASIS].name = "emphasis";
  scanner->opt[OPT_EMPHASIS].title = "emphasis";
  scanner->opt[OPT_EMPHASIS].desc = "image emphasis";
  scanner->opt[OPT_EMPHASIS].type = SANE_TYPE_STRING;
  scanner->opt[OPT_EMPHASIS].size = maxStringSize (emphasis_mode_list);
  scanner->opt[OPT_EMPHASIS].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  scanner->opt[OPT_EMPHASIS].constraint.string_list = emphasis_mode_list;
  if (scanner->has_emphasis)
    scanner->opt[OPT_EMPHASIS].cap =
      SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;


  scanner->opt[OPT_VARIANCE_RATE].name = "variancerate";
  scanner->opt[OPT_VARIANCE_RATE].title = "variance rate";
  scanner->opt[OPT_VARIANCE_RATE].desc =
    "variance rate for simplified dynamic threshold";
  scanner->opt[OPT_VARIANCE_RATE].type = SANE_TYPE_INT;
  scanner->opt[OPT_VARIANCE_RATE].unit = SANE_UNIT_NONE;
  scanner->opt[OPT_VARIANCE_RATE].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_VARIANCE_RATE].constraint.range = &variance_rate_range;
    scanner->opt[OPT_VARIANCE_RATE].cap =
      SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;

  scanner->opt[OPT_THRESHOLD_CURVE].name = "thresholdcurve";
  scanner->opt[OPT_THRESHOLD_CURVE].title = "threshold curve";
  scanner->opt[OPT_THRESHOLD_CURVE].desc = "DTC threshold_curve";
  scanner->opt[OPT_THRESHOLD_CURVE].type = SANE_TYPE_INT;
  scanner->opt[OPT_THRESHOLD_CURVE].unit = SANE_UNIT_NONE;
  scanner->opt[OPT_THRESHOLD_CURVE].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_THRESHOLD_CURVE].constraint.range = &threshold_curve_range;
  if (scanner->ipc_present)
    scanner->opt[OPT_THRESHOLD_CURVE].cap = SANE_CAP_SOFT_SELECT |
      SANE_CAP_SOFT_DETECT;

  scanner->opt[OPT_GRADATION].name = "gradation";
  scanner->opt[OPT_GRADATION].title = "gradation";
  scanner->opt[OPT_GRADATION].desc = "gradation";
  scanner->opt[OPT_GRADATION].type = SANE_TYPE_STRING;
  scanner->opt[OPT_GRADATION].size = maxStringSize (gradation_mode_list);
  scanner->opt[OPT_GRADATION].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  scanner->opt[OPT_GRADATION].constraint.string_list = gradation_mode_list;
  scanner->opt[OPT_GRADATION].cap =
    SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;

  scanner->opt[OPT_SMOOTHING_MODE].name = "smoothingmode";
  scanner->opt[OPT_SMOOTHING_MODE].title = "smoothing mode";
  scanner->opt[OPT_SMOOTHING_MODE].desc = "smoothing mode";
  scanner->opt[OPT_SMOOTHING_MODE].type = SANE_TYPE_STRING;
  scanner->opt[OPT_SMOOTHING_MODE].size = maxStringSize (smoothing_mode_list);
  scanner->opt[OPT_SMOOTHING_MODE].constraint_type =
    SANE_CONSTRAINT_STRING_LIST;
  scanner->opt[OPT_SMOOTHING_MODE].constraint.string_list =
    smoothing_mode_list;
  if (scanner->ipc_present)
    scanner->opt[OPT_SMOOTHING_MODE].cap = SANE_CAP_SOFT_SELECT |
      SANE_CAP_SOFT_DETECT;

  scanner->opt[OPT_FILTERING].name = "filtering";
  scanner->opt[OPT_FILTERING].title = "filtering";
  scanner->opt[OPT_FILTERING].desc = "filtering";
  scanner->opt[OPT_FILTERING].type = SANE_TYPE_STRING;
  scanner->opt[OPT_FILTERING].size = maxStringSize (filtering_mode_list);
  scanner->opt[OPT_FILTERING].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  scanner->opt[OPT_FILTERING].constraint.string_list = filtering_mode_list;
  if (scanner->ipc_present)
    scanner->opt[OPT_FILTERING].cap =
      SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;

  scanner->opt[OPT_BACKGROUND].name = "background";
  scanner->opt[OPT_BACKGROUND].title = "background";
  scanner->opt[OPT_BACKGROUND].desc =
    "output if gradation of the video data is equal to or larger than threshold";
  scanner->opt[OPT_BACKGROUND].type = SANE_TYPE_STRING;
  scanner->opt[OPT_BACKGROUND].size = maxStringSize (background_mode_list);
  scanner->opt[OPT_BACKGROUND].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  scanner->opt[OPT_BACKGROUND].constraint.string_list = background_mode_list;
  if (scanner->ipc_present)
    scanner->opt[OPT_BACKGROUND].cap =
      SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;


  scanner->opt[OPT_NOISE_REMOVAL].name = "noiseremoval";
  scanner->opt[OPT_NOISE_REMOVAL].title = "noise removal";
  scanner->opt[OPT_NOISE_REMOVAL].desc = "enables the noise removal matrixes";
  scanner->opt[OPT_NOISE_REMOVAL].type = SANE_TYPE_BOOL;
  scanner->opt[OPT_NOISE_REMOVAL].unit = SANE_UNIT_NONE;
  if (scanner->ipc_present)
    scanner->opt[OPT_NOISE_REMOVAL].cap = SANE_CAP_SOFT_SELECT |
      SANE_CAP_SOFT_DETECT;

  scanner->opt[OPT_MATRIX2X2].name = "matrix2x2";
  scanner->opt[OPT_MATRIX2X2].title = "2x2 noise removal matrix";
  scanner->opt[OPT_MATRIX2X2].desc = "2x2 noise removal matrix";
  scanner->opt[OPT_MATRIX2X2].type = SANE_TYPE_BOOL;
  scanner->opt[OPT_MATRIX2X2].unit = SANE_UNIT_NONE;
  if (scanner->ipc_present)
    scanner->opt[OPT_MATRIX2X2].cap = SANE_CAP_SOFT_SELECT |
      SANE_CAP_SOFT_DETECT;

  scanner->opt[OPT_MATRIX3X3].name = "matrix3x3";
  scanner->opt[OPT_MATRIX3X3].title = "3x3 noise removal matrix";
  scanner->opt[OPT_MATRIX3X3].desc = "3x3 noise removal matrix";
  scanner->opt[OPT_MATRIX3X3].type = SANE_TYPE_BOOL;
  scanner->opt[OPT_MATRIX3X3].unit = SANE_UNIT_NONE;
  if (scanner->ipc_present)
    scanner->opt[OPT_MATRIX3X3].cap = SANE_CAP_SOFT_SELECT |
      SANE_CAP_SOFT_DETECT;

  scanner->opt[OPT_MATRIX4X4].name = "matrix4x4";
  scanner->opt[OPT_MATRIX4X4].title = "4x4 noise removal matrix";
  scanner->opt[OPT_MATRIX4X4].desc = "4x4 noise removal matrix";
  scanner->opt[OPT_MATRIX4X4].type = SANE_TYPE_BOOL;
  scanner->opt[OPT_MATRIX4X4].unit = SANE_UNIT_NONE;
  if (scanner->ipc_present)
    scanner->opt[OPT_MATRIX4X4].cap = SANE_CAP_SOFT_SELECT |
      SANE_CAP_SOFT_DETECT;

  scanner->opt[OPT_MATRIX5X5].name = "matrix5x5";
  scanner->opt[OPT_MATRIX5X5].title = "5x5 noise removal matrix";
  scanner->opt[OPT_MATRIX5X5].desc = "5x5 noise removal matrix";
  scanner->opt[OPT_MATRIX5X5].type = SANE_TYPE_BOOL;
  scanner->opt[OPT_MATRIX5X5].unit = SANE_UNIT_NONE;
  if (scanner->ipc_present)
    scanner->opt[OPT_MATRIX5X5].cap = SANE_CAP_SOFT_SELECT |
      SANE_CAP_SOFT_DETECT;

  scanner->opt[OPT_WHITE_LEVEL_FOLLOW].name = "whitelevelfollow";
  scanner->opt[OPT_WHITE_LEVEL_FOLLOW].title = "white level follow";
  scanner->opt[OPT_WHITE_LEVEL_FOLLOW].desc = "white level follow";
  scanner->opt[OPT_WHITE_LEVEL_FOLLOW].type = SANE_TYPE_STRING;
  scanner->opt[OPT_WHITE_LEVEL_FOLLOW].size =
    maxStringSize (white_level_follow_mode_list);
  scanner->opt[OPT_WHITE_LEVEL_FOLLOW].constraint_type =
    SANE_CONSTRAINT_STRING_LIST;
  scanner->opt[OPT_WHITE_LEVEL_FOLLOW].constraint.string_list =
    white_level_follow_mode_list;
  if (scanner->has_white_level_follow)
    scanner->opt[OPT_WHITE_LEVEL_FOLLOW].cap = SANE_CAP_SOFT_SELECT |
      SANE_CAP_SOFT_DETECT;

  scanner->opt[OPT_DTC_SELECTION].name = "dtcselection";
  scanner->opt[OPT_DTC_SELECTION].title = "DTC selection";
  scanner->opt[OPT_DTC_SELECTION].desc = "DTC selection";
  scanner->opt[OPT_DTC_SELECTION].type = SANE_TYPE_STRING;
  scanner->opt[OPT_DTC_SELECTION].size =
    maxStringSize (dtc_selection_mode_list);
  scanner->opt[OPT_DTC_SELECTION].constraint_type =
    SANE_CONSTRAINT_STRING_LIST;
  scanner->opt[OPT_DTC_SELECTION].constraint.string_list =
    dtc_selection_mode_list;
  if (scanner->ipc_present)
    scanner->opt[OPT_DTC_SELECTION].cap = SANE_CAP_SOFT_SELECT |
      SANE_CAP_SOFT_DETECT;

  scanner->opt[OPT_PAPER_SIZE].name = "papersize";
  scanner->opt[OPT_PAPER_SIZE].title = "paper size";
  scanner->opt[OPT_PAPER_SIZE].desc = "Physical size of the paper in the ADF";
  scanner->opt[OPT_PAPER_SIZE].type = SANE_TYPE_STRING;
  scanner->opt[OPT_PAPER_SIZE].size = maxStringSize (paper_size_mode_list);
  scanner->opt[OPT_PAPER_SIZE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  scanner->opt[OPT_PAPER_SIZE].constraint.string_list = paper_size_mode_list;
  if (scanner->has_fixed_paper_size) 
    {
      scanner->opt[OPT_PAPER_SIZE].cap = 
	(SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT);
    } 
  else 
    {
      scanner->opt[OPT_PAPER_SIZE].cap = SANE_CAP_INACTIVE;
    }

  scanner->opt[OPT_PAPER_ORIENTATION].name = "orientation";
  scanner->opt[OPT_PAPER_ORIENTATION].title = "orientation";
  scanner->opt[OPT_PAPER_ORIENTATION].desc =
    "Orientation of the paper in the ADF";
  scanner->opt[OPT_PAPER_ORIENTATION].type = SANE_TYPE_STRING;
  scanner->opt[OPT_PAPER_ORIENTATION].size =
    maxStringSize (paper_orientation_mode_list);
  scanner->opt[OPT_PAPER_ORIENTATION].constraint_type =
    SANE_CONSTRAINT_STRING_LIST;
  scanner->opt[OPT_PAPER_ORIENTATION].constraint.string_list =
    paper_orientation_mode_list;
  if (scanner->has_fixed_paper_size) 
    {
      scanner->opt[OPT_PAPER_ORIENTATION].cap = 
	(SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT);
    } 
  else 
    {
      scanner->opt[OPT_PAPER_ORIENTATION].cap = SANE_CAP_INACTIVE;
    }


  scanner->opt[OPT_DROPOUT_COLOR].name = "dropoutcolor";
  scanner->opt[OPT_DROPOUT_COLOR].title = "dropout color";
  scanner->opt[OPT_DROPOUT_COLOR].desc =
    "select dropout color";
  scanner->opt[OPT_DROPOUT_COLOR].type = SANE_TYPE_STRING;
  scanner->opt[OPT_DROPOUT_COLOR].size = maxStringSize (dropout_color_list);
  scanner->opt[OPT_DROPOUT_COLOR].constraint_type =
    SANE_CONSTRAINT_STRING_LIST;
  scanner->opt[OPT_DROPOUT_COLOR].constraint.string_list = dropout_color_list;
  if (scanner->has_dropout_color)
    {
      scanner->opt[OPT_DROPOUT_COLOR].cap = SANE_CAP_SOFT_SELECT |
        SANE_CAP_SOFT_DETECT;
    }
  else
    {
      scanner->opt[OPT_DROPOUT_COLOR].cap = SANE_CAP_INACTIVE;
    }

  /* ------------------------------ */

  if (scanner->has_hw_status)
    {
      scanner->opt[OPT_START_BUTTON].name = "startbutton";
      scanner->opt[OPT_START_BUTTON].title = "start button";
      scanner->opt[OPT_START_BUTTON].desc =
        "is the start button of the scanner pressed";
      scanner->opt[OPT_START_BUTTON].type = SANE_TYPE_BOOL;
      scanner->opt[OPT_START_BUTTON].unit = SANE_UNIT_NONE;
      scanner->opt[OPT_START_BUTTON].cap = SANE_CAP_SOFT_DETECT;
    }


  if (scanner->has_imprinter) 
    {

      opt = scanner->opt + OPT_IMPRINTER_GROUP;
      opt->title = "imprinter control";
      opt->desc = "";
      opt->type = SANE_TYPE_GROUP;
      opt->constraint_type = SANE_CONSTRAINT_NONE;

      opt = scanner->opt + OPT_IMPRINTER;
      opt->name = "imprinter";
      opt->title = "enable imprinter";
      opt->desc = "enables the imprinter";
      opt->type = SANE_TYPE_BOOL;
      opt->unit = SANE_UNIT_NONE;
      opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;

      opt = scanner->opt + OPT_IMPRINTER_DIR;
      opt->name = "imprinterdirection";
      opt->title = "imprinter direction";
      opt->desc = "writing direction of the imprinter";
      opt->type = SANE_TYPE_STRING;
      opt->size = maxStringSize(imprinter_dir_list);
      opt->constraint_type = SANE_CONSTRAINT_STRING_LIST;
      opt->constraint.string_list = imprinter_dir_list;
      opt->cap = SANE_CAP_INACTIVE;

      opt = scanner->opt + OPT_IMPRINTER_CTR_DIR;
      opt->name = "imprinterctrdir";
      opt->title = "imprinter counter direction";
      opt->desc = "imprinter counter direction";
      opt->type = SANE_TYPE_STRING;
      opt->constraint_type = SANE_CONSTRAINT_STRING_LIST;
      opt->size = maxStringSize(imprinter_ctr_dir_list);
      opt->constraint.string_list = imprinter_ctr_dir_list;
      opt->cap = SANE_CAP_INACTIVE;

      opt = scanner->opt + OPT_IMPRINTER_YOFFSET;
      opt->name = "imprinteryoffset";
      opt->title = "imprinter Y-Offset";
      opt->desc = "imprinter Y-Offset";
      opt->type = SANE_TYPE_FIXED;
      opt->unit = SANE_UNIT_MM;
      opt->constraint_type = SANE_CONSTRAINT_RANGE;
      opt->constraint.range = &range_imprinter_y_offset;
      opt->cap = SANE_CAP_INACTIVE;

      opt = scanner->opt + OPT_IMPRINTER_STRING;
      opt->name = "imprinterstring";
      opt->title = "imprinter string";
      opt->desc = "The string the imprinter prints";
      opt->type = SANE_TYPE_STRING;
      opt->size = max_imprinter_string_length;
      opt->cap = SANE_CAP_INACTIVE;

      opt = scanner->opt + OPT_IMPRINTER_CTR_INIT;
      opt->name = "imprinterctrinit";
      opt->title = "imprinter counter init value";
      opt->desc = "imprinter counter initial value";
      opt->type = SANE_TYPE_INT;
      opt->unit = SANE_UNIT_NONE;
      opt->cap = SANE_CAP_INACTIVE;

      opt = scanner->opt + OPT_IMPRINTER_CTR_STEP;
      opt->name = "imprinterctrstep";
      opt->title = "imprinter counter step";
      opt->desc = "imprinter counter step";
      opt->type = SANE_TYPE_INT;
      opt->unit = SANE_UNIT_NONE;
      opt->constraint_type = SANE_CONSTRAINT_RANGE;
      opt->constraint.range = &imprinter_ctr_step_range;
      scanner->opt[OPT_IMPRINTER_CTR_STEP].cap = SANE_CAP_INACTIVE;

    } 
  else 
    {
      scanner->opt[OPT_IMPRINTER].cap = SANE_CAP_INACTIVE;
      scanner->opt[OPT_IMPRINTER_DIR].cap = SANE_CAP_INACTIVE;
      scanner->opt[OPT_IMPRINTER_YOFFSET].cap = SANE_CAP_INACTIVE;
      scanner->opt[OPT_IMPRINTER_STRING].cap = SANE_CAP_INACTIVE;
      scanner->opt[OPT_IMPRINTER_CTR_INIT].cap = SANE_CAP_INACTIVE;
      scanner->opt[OPT_IMPRINTER_CTR_STEP].cap = SANE_CAP_INACTIVE;
      scanner->opt[OPT_IMPRINTER_CTR_DIR].cap = SANE_CAP_INACTIVE;
    }


  /* "Tuning" group ------------------------------------------------------ */

  opt = scanner->opt + OPT_TUNING_GROUP;
  opt->title = "Fine tuning and debugging";
  opt->desc = "";
  opt->type = SANE_TYPE_GROUP;
  opt->constraint_type = SANE_CONSTRAINT_NONE;

  opt = scanner->opt + OPT_LAMP_COLOR;
  opt->name = "lampcolor";
  opt->title = "lamp used for grayscale acquisition";
  opt->desc =
    "One-pass color scanners have three lamp colors; only one of them is used in grayscale scanning. Scanning from nonstandard media (e.g. colored paper) can sometimes be improved by changing this setting.";
  opt->type = SANE_TYPE_STRING;
  opt->size = maxStringSize (lamp_colorList);
  opt->constraint_type = SANE_CONSTRAINT_STRING_LIST;
  opt->constraint.string_list = lamp_colorList;
  /* the opt->cap member is set to active in the setMode3091 subroutine
   * if monochrome scanning is selected
   */
  opt->cap = SANE_CAP_INACTIVE;

  opt = scanner->opt + OPT_BLUE_OFFSET;
  opt->name = "blueoffset";
  opt->title = "blue scan line offset from default";
  opt->desc =
    "Use small positive/negative values to correct for miscalibration.";
  opt->type = SANE_TYPE_INT;
  opt->unit = SANE_UNIT_NONE;
  opt->constraint_type = SANE_CONSTRAINT_RANGE;
  opt->constraint.range = &rangeColorOffset;
  opt->cap = ((scanner->model != MODEL_3091)&&(scanner->model != MODEL_3092)) ? SANE_CAP_INACTIVE :
    SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;

  opt = scanner->opt + OPT_GREEN_OFFSET;
  opt->name = "greenoffset";
  opt->title = "green scan line offset from default";
  opt->desc =
    "Use small positive/negative values to correct for miscalibration.";
  opt->type = SANE_TYPE_INT;
  opt->unit = SANE_UNIT_NONE;
  opt->constraint_type = SANE_CONSTRAINT_RANGE;
  opt->constraint.range = &rangeColorOffset;
  opt->cap = ((scanner->model != MODEL_3091)&&(scanner->model != MODEL_3092)) ? SANE_CAP_INACTIVE :
    SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;

  opt = scanner->opt + OPT_USE_SWAPFILE;
  opt->name = "swapfile";
  opt->title = "use swap file for 2nd page during duplex scans";
  opt->desc =
    "When set, data for the rear page during a duplex scan is buffered in a temp file rather than in memory. Use this option if you are scanning duplex and are low on memory.";
  opt->type = SANE_TYPE_BOOL;
  opt->unit = SANE_UNIT_NONE;
  opt->constraint_type = SANE_CONSTRAINT_NONE;
  opt->cap = ((scanner->model != MODEL_3091)&&(scanner->model != MODEL_3092)) ? SANE_CAP_INACTIVE :
    SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;

  opt = scanner->opt + OPT_SLEEP_MODE;
  opt->name = "sleeptimer";
  opt->title = "sleep timer";
  opt->desc = "time in minutes until the internal power supply switches to sleep mode"; 
  opt->type = SANE_TYPE_INT;
  opt->unit = SANE_UNIT_NONE;
  opt->constraint_type = SANE_CONSTRAINT_RANGE;
  opt->constraint.range=&range_sleep_mode;
  if (scanner->model == MODEL_FI || scanner->model == MODEL_FI4x20) /*...and others */
    {
      opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    } 
  else 
    {
      opt->cap = SANE_CAP_INACTIVE;
    }

  DBG (10, "init_options:ok\n");
  return SANE_STATUS_GOOD;
}                               /* init_options */


/* one line has the following format:
 * rrr...rrrggg...gggbbb...bbbb 
 * ^        ^        ^
 * \-r      \-g      \-b
 * The pointers r,g,b point to the start of the
 * r/g/b part. */
static void
convert_rrggbb_to_rgb(struct fujitsu *scanner, unsigned char * buffptr, unsigned int length)
{

    unsigned char * outptr = buffptr;
    int i_num_lines, i, j, bytes_per_line, pix_per_line;

    bytes_per_line = scanner->bytes_per_scan_line;
    pix_per_line =  bytes_per_line/3; 
    i_num_lines = (length - (length % bytes_per_line)) / bytes_per_line;

    /* only byteswap for full lines*/
    for (i=0; i < i_num_lines; i++){
     for (j=0; j < pix_per_line; j++){

       /* r */
       memcpy (outptr,
	       &scanner->buffer[(i*bytes_per_line)+j], 1); 
       outptr++;

       /* g */
       memcpy (outptr,
	       &scanner->buffer[(i*bytes_per_line)+j+pix_per_line], 1);
       outptr++;
       
       /* b */
       memcpy (outptr,
	       &scanner->buffer[(i*bytes_per_line)+j+2*pix_per_line], 1);
       outptr++;
     }
    }

    /* a partial line? just copy. this should never happen.
     * if it does, would be better to save this spare data,
     * and prepend it to the next buffer --FIXME--*/
    for (i=i_num_lines*bytes_per_line; i < (int) length; i++){
        memcpy (outptr,&scanner->buffer[i], 1);
        outptr++;
    }
    
}

/* scanner returns pixel data as bgrbgrbgr
 * turn each pixel around to rgbrgb */
static void
convert_bgr_to_rgb(struct fujitsu *scanner, unsigned char * buffptr, unsigned int length)
{

    unsigned char * outptr = buffptr;
    int i_num_pixels, i, j;

    /* number of full (3byte) pixels */
    i_num_pixels = (length - (length % 3)) / 3;

    /* only byteswap for full pixels*/
    for (i=0; i < i_num_pixels; i++){
        memcpy (outptr,&scanner->buffer[i*3+2], 1); /*r*/
        outptr++;
        memcpy (outptr,&scanner->buffer[i*3+1], 1); /*g*/
        outptr++;
        memcpy (outptr,&scanner->buffer[i*3], 1);   /*b*/
        outptr++;
    }

    /* a partial pixel? just copy. this should never happen.
     * if it does, would be better to save this spare data,
     * and prepend it to the next buffer --FIXME--*/
    for (j=i_num_pixels*3; j < (int) length; j++){
        memcpy (outptr,&scanner->buffer[j], 1);
        outptr++;
    }
    
}

static unsigned int
reader_duplex_alternate (struct fujitsu *scanner,
                              FILE * fp_front, FILE * fp_back)
{

  int status;
  unsigned int i_data_read;

  unsigned int data_to_read = 0;
  unsigned int total_data_size = 0;
  unsigned int i_left_front, i_left_back;

  unsigned int largeBufferSize = 0;
  unsigned char *largeBuffer;

  unsigned int duplexBufferSize = 0;
  unsigned char *duplexBuffer = NULL;
  unsigned char *duplexPointer = NULL;

  /*total data bytes we expect per side*/
  i_left_front = scanner->bytes_per_scan_line * scanner->scan_height_pixels;
  i_left_back = i_left_front;

  /* Only allocate memory if we're not using a temp file. */
  if (!scanner->use_temp_file)
    {
      /*
       * allocate a buffer for the back side because sane_read will 
       * first read the front side and then the back side. If I write
       * to the pipe directly sane_read will not read from that pipe
       * and the write will block.
       */
      duplexBufferSize = i_left_back;
      duplexBuffer = malloc (duplexBufferSize);
      if (duplexBuffer == NULL)
        {
          DBG (MSG_ERR,
               "reader_process: out of memory for duplex buffer (try option --swapfile)\n");
          return (0);
        }
      duplexPointer = duplexBuffer;
    }

  /* alloc mem to talk to the convert routines */
  /* make buffer aligned to size of a scanline */
  largeBufferSize = scanner->scsi_buf_size - (scanner->scsi_buf_size % scanner->bytes_per_scan_line);
  largeBuffer = malloc (largeBufferSize);
  if (largeBuffer == NULL) {
      DBG (MSG_ERR, "reader_process: out of memory for scan buffer (try option --swapfile)\n");
      return (0);
  }

  /* read the front side */
  do {
      data_to_read = (i_left_front < largeBufferSize) ? i_left_front : largeBufferSize;

      DBG (MSG_IO_READ, 
	   "reader_process: read %d bytes from front side\n", data_to_read);
      status = read_large_data_block (scanner, scanner->buffer, data_to_read, 
                                      0x0, &i_data_read);

      switch (status)
        {
        case SANE_STATUS_GOOD:
          total_data_size += i_data_read;
          i_left_front -= i_data_read;
          break;
        case SANE_STATUS_EOF:
          DBG (5, "reader_process: EOM (no more data) length = %d\n", i_data_read);
          total_data_size += i_data_read;
          i_left_front = 0;
          break;
        case SANE_STATUS_DEVICE_BUSY:
          DBG (5, "device busy");
          break;
        default:
          DBG (MSG_ERR, "reader_process: unable to get image data from scanner!\n");
          fclose (fp_front);
          fclose (fp_back);
          /*FIXME should we free some buffers here?*/
          return (0);
        }

      /* clear rgb buffer*/
      memset(largeBuffer,0,largeBufferSize);

      /* call model specific conversion routine */
      switch (scanner->read_mode) {
        case READ_MODE_RRGGBB:
          convert_rrggbb_to_rgb(scanner,largeBuffer,i_data_read);
          break;
        case READ_MODE_BGR:
          convert_bgr_to_rgb(scanner,largeBuffer,i_data_read);
          break;
        case READ_MODE_PASS:
          largeBuffer=scanner->buffer;
          break;
        /*case READ_MODE_3091RGB:
          convert_3091rgb_to_rgb(scanner,largeBuffer,i_data_read);
          break;*/
        default:
          DBG (5, "reader_process: cant convert buffer, unsupported read_mode %d\n",scanner->read_mode);
          return (0);
      }

      /* write data to file */
      fwrite (largeBuffer, 1, i_data_read, fp_front);

      DBG (MSG_IO_READ, "reader_process_front: buffer of %d bytes read; %d bytes to go\n",
           i_data_read, i_left_front);


      /*
       * read back side
       */
      data_to_read = (i_left_back < largeBufferSize) ? i_left_back : largeBufferSize;

      DBG (MSG_IO_READ, 
	   "reader_process: read %d bytes from back side\n", data_to_read);
      status = read_large_data_block (scanner, scanner->buffer, data_to_read, 
                                      0x80, &i_data_read);

      switch (status)
        {
        case SANE_STATUS_GOOD:
          total_data_size += i_data_read;
          i_left_back -= i_data_read;
          break;
        case SANE_STATUS_EOF:
          DBG (5, "reader_process: EOM (no more data) length = %d\n", i_data_read);
          total_data_size += i_data_read;
          i_left_back = 0;
          break;
        case SANE_STATUS_DEVICE_BUSY:
          DBG (5, "device busy");
          break;
        default:
          DBG (MSG_ERR, "reader_process: unable to get image data from scanner!\n");
          fclose (fp_front);
          fclose (fp_back);
          /*FIXME should we free some buffers here?*/
          return (0);
        }

      /* clear rgb buffer*/
      memset(largeBuffer,0,largeBufferSize);

      /* call model specific conversion routine */
      switch (scanner->read_mode) {
        case READ_MODE_RRGGBB:
          convert_rrggbb_to_rgb(scanner,largeBuffer,i_data_read);
          break;
        case READ_MODE_BGR:
          convert_bgr_to_rgb(scanner,largeBuffer,i_data_read);
          break;
        case READ_MODE_PASS:
          largeBuffer=scanner->buffer;
          break;
        /*case READ_MODE_3091RGB:
          convert_3091rgb_to_rgb(scanner,largeBuffer,i_data_read);
          break;*/
        default:
          DBG (5, "reader_process: cant convert buffer, unsupported read_mode %d\n",scanner->read_mode);
          return (0);
      }

      if (scanner->use_temp_file) {

          /* write data to file */
          if((unsigned int) fwrite (largeBuffer, 1, i_data_read, fp_back) != 1){
            fclose (fp_back);
            DBG (MSG_ERR, "reader_process: out of disk space while writing temp file\n");
            return (0);
          }

      }

      else {
          /* write data to buffer */
          memcpy (duplexPointer, largeBuffer, i_data_read);
      }

      DBG (MSG_IO_READ,
	   "reader_process_back: buffer of %d bytes read; %d bytes to go\n",
           data_to_read, i_left_back);

  } while (i_left_front > 0 || i_left_back > 0);

  fflush (fp_front);
  fclose (fp_front);

  if (scanner->use_temp_file)
    {
      fflush (fp_back);
    }
  else
    {
      fwrite (duplexBuffer, 1, duplexBufferSize, fp_back);
      fflush(fp_back);
      fclose(fp_back);
      free (duplexBuffer);
    }

  return total_data_size;
}

static unsigned int
reader_duplex_sequential (struct fujitsu *scanner,
                               FILE * fp_front, FILE * fp_back)
{
  unsigned int i_total_size;

  i_total_size = 0;

  i_total_size += reader_simplex (scanner, fp_front, 0);
  i_total_size += reader_simplex (scanner, fp_back, 0x80);

  return i_total_size;
}

static unsigned int
reader_simplex (struct fujitsu *scanner, FILE * fp_front, int i_window_id)
{

  int status;
  unsigned int i_data_read;

  unsigned int data_to_read = 0;
  unsigned int total_data_size = 0;
  unsigned int i_left_front;

  unsigned int largeBufferSize = 0;
  unsigned char *largeBuffer;

  /*total data bytes we expect per side*/
  i_left_front = scanner->bytes_per_scan_line * scanner->scan_height_pixels;

  /* alloc mem to talk to the convert routines */
  /* make buffer aligned to size of a scanline */
  largeBufferSize = scanner->scsi_buf_size - (scanner->scsi_buf_size % scanner->bytes_per_scan_line);
  largeBuffer = malloc (largeBufferSize);
  if (largeBuffer == NULL) {
      DBG (MSG_ERR, "reader_process: out of memory for scan buffer (try option --swapfile)\n");
      return (0);
  }

  /* read the data */
  do {
      data_to_read = (i_left_front < largeBufferSize) ? i_left_front : largeBufferSize;

      DBG (MSG_IO_READ, 
	   "reader_process: read %d bytes from front side\n", data_to_read);
      status = read_large_data_block (scanner, scanner->buffer, data_to_read, 
                                      i_window_id, &i_data_read);

      switch (status)
        {
        case SANE_STATUS_GOOD:
          total_data_size += i_data_read;
          i_left_front -= i_data_read;
          break;
        case SANE_STATUS_EOF:
          DBG (5, "reader_process: EOM (no more data) length = %d\n", i_data_read);
          total_data_size += i_data_read;
          i_left_front = 0;
          break;
        case SANE_STATUS_DEVICE_BUSY:
          DBG (5, "device busy");
          break;
        default:
          DBG (MSG_ERR, "reader_process: unable to get image data from scanner!\n");
          fclose (fp_front);
          /*FIXME should we free some buffers here?*/
          return (0);
        }

      /* clear rgb buffer*/
      memset(largeBuffer,0,largeBufferSize);

      /* call model specific conversion routine */
      switch (scanner->read_mode) {
        case READ_MODE_RRGGBB:
          convert_rrggbb_to_rgb(scanner,largeBuffer,i_data_read);
          break;
        case READ_MODE_BGR:
          convert_bgr_to_rgb(scanner,largeBuffer,i_data_read);
          break;
        case READ_MODE_PASS:
          largeBuffer=scanner->buffer;
          break;
        /*case READ_MODE_3091RGB:
          convert_3091rgb_to_rgb(scanner,largeBuffer,i_data_read);
          break;*/
        default:
          DBG (5, "reader_process: cant convert buffer, unsupported read_mode %d\n",scanner->read_mode);
          return (0);
      }

      /* write data to file */
      fwrite (largeBuffer, 1, i_data_read, fp_front);

      DBG (MSG_IO_READ, "reader_process_front: buffer of %d bytes read; %d bytes to go\n",
           i_data_read, i_left_front);

  } while (i_left_front > 0);

  fflush (fp_front);
  fclose (fp_front);

  return total_data_size;
}

static unsigned int
reader_gray_duplex_alternate (struct fujitsu *scanner,
                              FILE * fp_front, FILE * fp_back)
{

  int status;
  unsigned int i_left_front, i_left_back;
  unsigned int total_data_size, data_to_read;
  unsigned int duplexBufferSize;
  unsigned char *duplexBuffer;
  unsigned char *duplexPointer;
  unsigned int largeBufferSize = 0;
  unsigned int i_data_read;

  /*
  time_t start_time, end_time;
  (void) time (&start_time);
  */

  i_left_front = scanner->bytes_per_scan_line * scanner->scan_height_pixels;
  i_left_back = i_left_front;

  /* Only allocate memory if we're not using a temp file. */
  if (scanner->use_temp_file)
    {
      duplexBuffer = duplexPointer = NULL;
      duplexBufferSize = 0;
    }
  else
    {
      /*
       * allocate a buffer for the back side because sane_read will 
       * first read the front side and than the back side. If I write
       * to the pipe directly sane_read will not read from that pipe
       * and the write will block.
       */
      duplexBuffer = malloc (duplexBufferSize = i_left_back);
      if (duplexBuffer == NULL)
        {
          DBG (MSG_ERR,
               "reader_process: out of memory for duplex buffer (try option --swapfile)\n");
          return (0);
        }
      duplexPointer = duplexBuffer;
    }

  largeBufferSize =
    scanner->scsi_buf_size -
    (scanner->scsi_buf_size % scanner->bytes_per_scan_line);

  total_data_size = 0;

  do
    {
      data_to_read = (i_left_front < largeBufferSize)
        ? i_left_front : largeBufferSize;

      DBG (MSG_IO_READ, 
	   "reader_process: read %d bytes from front side\n", data_to_read);
      status = read_large_data_block (scanner, scanner->buffer, data_to_read, 
                                      0x0, &i_data_read);

      switch (status)
        {
        case SANE_STATUS_GOOD:
          break;
        case SANE_STATUS_EOF:
          DBG (5, "reader_process: EOM (no more data) length = %d\n",
               scanner->i_transfer_length);
          data_to_read -= scanner->i_transfer_length;
          i_left_front = data_to_read;
          break;
        case SANE_STATUS_DEVICE_BUSY:
          DBG (5, "device busy");
          data_to_read = 0;
          break;
        default:
          DBG (MSG_ERR, 
               "reader_process: unable to get image data from scanner!\n");
          fclose (fp_front);
          fclose (fp_back);
          return (0);
        }

      total_data_size += data_to_read;
      fwrite (scanner->buffer, 1, data_to_read, fp_front);
      i_left_front -= data_to_read;
      DBG (MSG_IO_READ,
           "reader_process_front: buffer of %d bytes read; %d bytes to go\n",
           data_to_read, i_left_front);


      /*
       * read back side
       */
      data_to_read = (i_left_back < largeBufferSize)
        ? i_left_back : largeBufferSize;

      DBG (MSG_IO_READ, 
	   "reader_process: read %d bytes from back side\n", data_to_read);
      status = read_large_data_block (scanner, scanner->buffer, data_to_read, 
                                      0x80, &i_data_read);

      switch (status)
        {
        case SANE_STATUS_GOOD:
          break;
        case SANE_STATUS_EOF:
          DBG (5, "reader_process: EOM (no more data) length = %d\n",
               scanner->i_transfer_length);
          data_to_read -= scanner->i_transfer_length;
          i_left_back = data_to_read;
          break;
        case SANE_STATUS_DEVICE_BUSY:
          DBG (5, "device busy");
          data_to_read = 0;
          break;
        default:
          DBG (MSG_ERR, 
               "reader_process: unable to get image data from scanner!\n");
          fclose (fp_front);
          fclose (fp_back);
          return (0);
        }

      total_data_size += data_to_read;
      if (scanner->use_temp_file)
        {
          if ((unsigned int) fwrite (scanner->buffer, 1, data_to_read, fp_back)
              != data_to_read)
            {
              fclose (fp_back);
              DBG (MSG_ERR,
                   "reader_process: out of disk space while writing temp file\n");
              return (0);
            }
        }
      else
        {
          memcpy (duplexPointer, scanner->buffer, data_to_read);
          duplexPointer += data_to_read;
        }

      i_left_back -= data_to_read;
      DBG (MSG_IO_READ,
           "reader_process_back: buffer of %d bytes read; %d bytes to go\n",
           data_to_read, i_left_back);
    }
  while (i_left_front > 0 || i_left_back > 0);

  fflush (fp_front);
  fclose (fp_front);

  /*
  (void) time (&end_time);
  if (end_time == start_time)
    end_time++;

  DBG (1, "time to read from scanner: %lu seconds\n", (end_time - start_time));
  */

  if (scanner->use_temp_file)
    {
      fflush (fp_back);
    }
  else
    {
      fwrite (duplexBuffer, 1, duplexBufferSize, fp_back);
      fflush(fp_back);
      fclose(fp_back);
      free (duplexBuffer);
    }

  return total_data_size;
}

static unsigned int
reader_gray_duplex_sequential (struct fujitsu *scanner,
                               FILE * fp_front, FILE * fp_back)
{
  unsigned int i_total_size;

  i_total_size = 0;

  i_total_size += reader_generic_passthrough (scanner, fp_front, 0);
  i_total_size += reader_generic_passthrough (scanner, fp_back, 0x80);

  return i_total_size;
}

/*
 * @@ Section 5 - M3091 specific internal routines
 */

static unsigned int
reader3091ColorSimplex (struct fujitsu *scanner, FILE * fp)
{
  int status;
  unsigned int data_left;
  unsigned int total_data_size, data_to_read, dataToProcess;
  unsigned int green_offset, blue_offset, lookAheadSize;
  unsigned readOffset;
  unsigned char *linebuffer = 0;
  unsigned char *largeBuffer;
  unsigned int largeBufferSize = 0;
  unsigned int colorLineGap = 0;
  unsigned char *redSource, *greenSource, *blueSource, *target;
  unsigned int i_data_read;

#ifdef DEBUG
  unsigned int lineCount = 0;
  int redSum, greenSum, blueSum;
  unsigned int redStart, greenStart, blueStart;
#endif

  linebuffer = malloc (scanner->bytes_per_scan_line);
  data_left = scanner->bytes_per_scan_line * scanner->scan_height_pixels;

  colorLineGap = scanner->color_raster_offset * scanner->resolution_y / 300;
  green_offset =
    (2 * colorLineGap + scanner->green_offset) * scanner->bytes_per_scan_line;
  blue_offset =
    (colorLineGap + scanner->blue_offset) * scanner->bytes_per_scan_line;

  lookAheadSize = green_offset;
  DBG (10, "colorLineGap=%u, green_offset=%u, blue_offset =%u\n",
       colorLineGap, green_offset / scanner->bytes_per_scan_line,
       blue_offset / scanner->bytes_per_scan_line);

  largeBuffer = scanner->buffer;
  largeBufferSize =
    scanner->scsi_buf_size -
    scanner->scsi_buf_size % scanner->bytes_per_scan_line;

  if (largeBufferSize < 2 * lookAheadSize)
    {
      /* 
       * We're in a bit of trouble here. To decode colors correctly, we have to look
       * ahead in the data stream, but obviously the pre-allocated buffer will not
       * hold enough lines to do this in an effective fashion (or do it at all).
       * Get a larger buffer.
       */
      largeBuffer = malloc (largeBufferSize = 4 * lookAheadSize);
    }

  DBG (MSG_IO_READ,
       "reader_process: reading %u+%u bytes in large blocks of %u bytes\n",
       data_left, lookAheadSize, largeBufferSize);

  readOffset = 0;
  data_left += lookAheadSize;
  total_data_size = data_left;

  do
    {
      data_to_read = (data_left < largeBufferSize - readOffset)
        ? data_left : largeBufferSize - readOffset;

      dataToProcess = data_to_read + readOffset - lookAheadSize;
      /*      if (data_to_read == data_left) dataToProcess += lookAheadSize / 2;  */

      status =
        read_large_data_block (scanner, largeBuffer + readOffset, data_to_read,
                               0, &i_data_read);

      switch (status)
        {
        case SANE_STATUS_GOOD:
          break;
        case SANE_STATUS_EOF:
          DBG (5, "reader_process: EOM (no more data) length = %d\n",
               scanner->i_transfer_length);
          data_to_read -= scanner->i_transfer_length;
          data_left = data_to_read;
          break;
        default:
          DBG (MSG_ERR, 
               "reader_process: unable to get image data from scanner!\n");
          fclose (fp);
          return (0);
        }

      /* 
       * This scanner will send you one line of R values, one line of G
       * values, and one line of B values in succession. 
       * The pitfall, however, is that those are *not* corresponding to
       * one and the same "line" on the paper. 
       * Instead, the "green" values matching the first line of "red"
       * values appear 8 lines further "down" in the data stream, and
       * the "blue" values are 4 lines further "down". (That's for 300dpi;
       * for 75dpi it's 2 and 1 line, for 150dpi it's 4 and 2 lines, 
       * for 600dpi 16 and 8 lines.)
       * The scanner will send you a total of 8 lines (at 300dpi) more
       * than you asked for so that you can build the complete image
       * from that.
       *
       * This code basically works by retaining the last "lookAheadSize"
       * bytes of the buffer just read and copying them onto the beginning
       * of the next data block, in effect creating a "sliding window"
       * over the data stream with an overlap of "lookAheadSize".
       */

      redSource = largeBuffer;
      greenSource = redSource + green_offset + scanner->scan_width_pixels;
      blueSource = redSource + blue_offset + 2 * scanner->scan_width_pixels;


      while (redSource < (largeBuffer + dataToProcess))
        {

#ifdef DEBUG
          redStart =
            ((unsigned int) redSource -
             (unsigned int) largeBuffer) / scanner->bytes_per_scan_line;
          greenStart =
            ((unsigned int) greenSource -
             (unsigned int) largeBuffer) / scanner->bytes_per_scan_line;
          blueStart =
            ((unsigned int) blueSource -
             (unsigned int) largeBuffer) / scanner->bytes_per_scan_line;
          redSum = 0;
          greenSum = 0;
          blueSum = 0;
#endif

          target = linebuffer;
          while (target < linebuffer + scanner->bytes_per_scan_line)
            {
#ifdef DEBUG
              redSum += *redSource;
              greenSum += *greenSource;
              blueSum += *blueSource;
#endif
              *(target++) = *(redSource++);
              *(target++) = *(greenSource++);
              *(target++) = *(blueSource++);
            }
          redSource += 2 * scanner->scan_width_pixels;
          greenSource += 2 * scanner->scan_width_pixels;
          blueSource += 2 * scanner->scan_width_pixels;
#ifdef DEBUG
          redSum = redSum / scanner->scan_width_pixels / 26;
          greenSum = greenSum / scanner->scan_width_pixels / 26;
          blueSum = blueSum / scanner->scan_width_pixels / 26;
          DBG (10, "line %4u: source lines %3u/%3u/%3u colors %u/%u/%u\n",
               lineCount++, redStart, greenStart, blueStart, redSum, greenSum,
               blueSum);
#endif
          fwrite (linebuffer, 1, scanner->bytes_per_scan_line, fp);
        }
      fflush (fp);

      data_left -= data_to_read;
      DBG (10, "reader_process(color, simplex): buffer of %d bytes read; %d bytes to go\n",
           data_to_read, data_left);
      memcpy (largeBuffer, largeBuffer + dataToProcess, lookAheadSize);
      readOffset = lookAheadSize;
    }
  while (data_left);
  free (linebuffer);
  if (largeBuffer != scanner->buffer)
    free (largeBuffer);
  fclose (fp);

  return total_data_size;
}


static unsigned int
reader3091ColorDuplex (struct fujitsu *scanner, FILE * fp_front, FILE * fp_back)
{
  int status;
  unsigned int data_left;
  unsigned int total_data_size, data_to_read, dataToProcess;
  unsigned int green_offset, blue_offset, lookAheadSize;
  unsigned readOffset;
  unsigned int duplexStartLine, duplexEndLine;
  unsigned char *linebuffer = 0;
  unsigned char *duplexBuffer = 0;
  unsigned char *duplexPointer = 0;
  unsigned char *largeBuffer;
  unsigned int largeBufferSize = 0;
  unsigned int duplexBufferSize = 0;
  unsigned int lineCount = 0;
  unsigned int colorLineGap = 0;
  unsigned char *redSource, *greenSource, *blueSource, *target;
  unsigned int i_data_read;

#ifdef DEBUG
  unsigned int frontLineCount = 0;
  unsigned int backLineCount = 0;
  int redSum, greenSum, blueSum;
  unsigned int redStart, greenStart, blueStart;
#endif

  linebuffer = malloc (scanner->bytes_per_scan_line);
  if (linebuffer == NULL)
    {
      DBG (MSG_ERR, "reader_process: out of memory for line buffer\n");
      return (0);
    }
  data_left = scanner->bytes_per_scan_line * scanner->scan_height_pixels;

  colorLineGap = scanner->color_raster_offset * scanner->resolution_y / 300;
  green_offset = 2 * colorLineGap * scanner->bytes_per_scan_line;
  blue_offset = colorLineGap * scanner->bytes_per_scan_line;

  lookAheadSize = green_offset * 2;

  /*
   * In duplex mode, the scanner will first send some lines
   * belonging to the first page, then, after a certain number of
   * lines (duplexStartLine) it will send lines from the front and
   * back page in an alternating fashion, and after duplexEndLine
   * lines there will be only lines from the back page.
   * 
   * The total number of lines will be lines_per_scan * 2.
   * 
   * Please also read the comments in reader3091ColorSimplex which
   * explain the color encoding. They are valid for duplex color
   * scans as well, with the exception that at the two critical
   * points within the page where "front only" changes into
   * "front/back alternating" and "front/back alternating" changes
   * into "back only" the calculation of the offset between red,
   * green and blue is a bit tricky.
   *
   * For example, at 300 dpi the green value matching the red value
   * I'm just reading will lie 8 lines ahead (simplex); if I'm in
   * the middle of a duplex scan, it's going to be 16 lines (because
   * there will be 8 lines belonging to the reverse side and thus
   * not being counted); but if the red pixel I'm reading is before
   * one of the mentioned critical boundaries and the matching green
   * pixel is beyond, this number may be anything in the range 8-16.
   * Took me some time to figure it out but I hope it works now.
   */
  duplexStartLine =
    scanner->duplex_raster_offset * scanner->resolution_y / 300 + 1;
  duplexEndLine =
    scanner->scan_height_pixels * 2 + 4 * colorLineGap -
    scanner->duplex_raster_offset * scanner->resolution_y / 300;

  DBG (5, "duplex start line %u, end line %u, color gap %u\n",
       duplexStartLine, duplexEndLine, colorLineGap);

  /* Only allocate memory if we're not using a temp file. */
  if (scanner->use_temp_file)
    {
      duplexBuffer = duplexPointer = NULL;
      duplexBufferSize = 0;
    }
  else
    {
      duplexBuffer = malloc (duplexBufferSize = data_left);
      if (duplexBuffer == NULL)
        {
          DBG (MSG_ERR,
               "reader_process: out of memory for duplex buffer (try option --swapfile)\n");
          return (0);
        }
      duplexPointer = duplexBuffer;
    }
  data_left *= 2;

  largeBuffer = scanner->buffer;
  largeBufferSize =
    scanner->scsi_buf_size -
    scanner->scsi_buf_size % scanner->bytes_per_scan_line;

  if (largeBufferSize < 2 * lookAheadSize)
    {
      /* 
       * We're in a bit of trouble here. To decode colors correctly,
       * we have to look ahead in the data stream, but obviously the
       * pre-allocated buffer will not hold enough lines to do this
       * in an effective fashion (or do it at all).  Get a larger
       * buffer.
       */
      largeBuffer = malloc (largeBufferSize = 4 * lookAheadSize);
      if (largeBuffer == NULL)
        {
          DBG (MSG_ERR,
               "reader_process: out of memory for SCSI read buffer, try smaller image\n");
          return (0);
        }
    }

  DBG (MSG_IO_READ,
       "reader_process: reading %u bytes in blocks of %u bytes\n",
       data_left, scanner->scsi_buf_size);


  readOffset = 0;
  data_left += lookAheadSize;
  total_data_size = data_left;

  do
    {
      data_to_read = (data_left < largeBufferSize - readOffset)
        ? data_left : largeBufferSize - readOffset;

      dataToProcess = data_to_read + readOffset - lookAheadSize;
      if (data_to_read == data_left)
        dataToProcess += lookAheadSize / 2;

      status =
        read_large_data_block (scanner, largeBuffer + readOffset, data_to_read,
                               0, &i_data_read);
      switch (status)
        {
        case SANE_STATUS_GOOD:
          break;
        case SANE_STATUS_EOF:
          DBG (5, "reader_process: EOM (no more data) length = %d\n",
               scanner->i_transfer_length);
          data_to_read -= scanner->i_transfer_length;
          data_left = data_to_read;
          break;
        default:
          DBG (MSG_ERR, 
               "reader_process: unable to get image data from scanner!\n");
          fclose (fp_front);
          fclose (fp_back);
          return (0);
        }

      /* 
       * This scanner will send you one line of R values, one line of G
       * values, and one line of B values in succession. 
       * The pitfall, however, is that those are *not* corresponding to
       * one and the same "line" on the paper. 
       * Instead, the "green" values matching the first line of "red"
       * values appear 8 lines further "down" in the data stream, and
       * the "blue" values are 4 lines further "down". (That's for 300dpi;
       * for 75dpi it's 2 and 1 line, for 150dpi it's 4 and 2 lines, 
       * for 600dpi 16 and 8 lines.)
       * The scanner will send you a total of 8 lines (at 300dpi) more
       * than you asked for so that you can build the complete image
       * from that.
       *
       * This code basically works by retaining the last "lookAheadSize"
       * bytes of the buffer just read and copying them onto the beginning
       * of the next data block, in effect creating a "sliding window"
       * over the data stream with an overlap of "lookAheadSize".
       */

      redSource = largeBuffer;
      greenSource = redSource + green_offset + scanner->scan_width_pixels;
      blueSource = redSource + blue_offset + 2 * scanner->scan_width_pixels;

      while (redSource < largeBuffer + dataToProcess)
        {

#ifdef DEBUG
          redStart =
            ((unsigned int) redSource -
             (unsigned int) largeBuffer) / scanner->bytes_per_scan_line;
          greenStart =
            ((unsigned int) greenSource -
             (unsigned int) largeBuffer) / scanner->bytes_per_scan_line;
          blueStart =
            ((unsigned int) blueSource -
             (unsigned int) largeBuffer) / scanner->bytes_per_scan_line;
          redSum = 0;
          greenSum = 0;
          blueSum = 0;
#endif

          target = linebuffer;
          while (target < linebuffer + scanner->bytes_per_scan_line)
            {
#ifdef DEBUG
              redSum += *redSource;
              greenSum += *greenSource;
              blueSum += *blueSource;
#endif
              *(target++) = *(redSource++);
              *(target++) = *(greenSource++);
              *(target++) = *(blueSource++);
            }
          redSource += 2 * scanner->scan_width_pixels;
          greenSource += 2 * scanner->scan_width_pixels;
          blueSource += 2 * scanner->scan_width_pixels;
#ifdef DEBUG
          redSum = redSum / scanner->scan_width_pixels / 26;
          greenSum = greenSum / scanner->scan_width_pixels / 26;
          blueSum = blueSum / scanner->scan_width_pixels / 26;
#endif

          if ((lineCount < duplexStartLine) ||
              (((lineCount - duplexStartLine) % 2 == 1)
               && (lineCount < duplexEndLine)))
            {
              /* we want to ignore a certain number of invalid lines at the end */
              if (lineCount < duplexEndLine - 4 * colorLineGap)
                {
                  fwrite (linebuffer, 1, scanner->bytes_per_scan_line,
                          fp_front);
#ifdef DEBUG
                  DBG (10,
                       "line %4u to front line %4u source lines %3u/%3u/%3u colors %u/%u/%u\n",
                       lineCount, frontLineCount++, redStart, greenStart,
                       blueStart, redSum, greenSum, blueSum);
                }
              else
                {
                  DBG (10,
                       "line %4u (front/ignored)    source lines %3u/%3u/%3u colors %u/%u/%u\n",
                       lineCount, redStart, greenStart, blueStart, redSum,
                       greenSum, blueSum);
#endif
                }
            }
          else
            {
              if (scanner->use_temp_file)
                {
                  if ((int)
                      fwrite (linebuffer, 1, scanner->bytes_per_scan_line,
                              fp_back) != scanner->bytes_per_scan_line)
                    {
                      fclose (fp_back);
                      DBG (MSG_ERR,
                           "reader_process: out of disk space while writing temp file\n");
                      return (0);
                    }
                }
              else
                {
                  memcpy (duplexPointer, linebuffer,
                          scanner->bytes_per_scan_line);
                  duplexPointer += scanner->bytes_per_scan_line;
                }
#ifdef DEBUG
              DBG (10,
                   "line %4u to back  line %4u source lines %3u/%3u/%3u colors %u/%u/%u\n",
                   lineCount, backLineCount++, redStart, greenStart,
                   blueStart, redSum, greenSum, blueSum);
#endif
            }

          lineCount++;

          if ((lineCount >= duplexStartLine - 2 * colorLineGap)
              && (lineCount < duplexStartLine))
            {
              greenSource += scanner->bytes_per_scan_line;
              green_offset += scanner->bytes_per_scan_line;
            }
          else if ((lineCount > duplexEndLine - 4 * colorLineGap)
                   && (lineCount <= duplexEndLine)
                   && ((duplexEndLine - lineCount) % 2 == 1))
            {
              greenSource -= scanner->bytes_per_scan_line;
              green_offset -= scanner->bytes_per_scan_line;
            }
          if ((lineCount >= duplexStartLine - colorLineGap)
              && (lineCount < duplexStartLine))
            {
              blueSource += scanner->bytes_per_scan_line;
              blue_offset += scanner->bytes_per_scan_line;
            }
          else if ((lineCount > duplexEndLine - 2 * colorLineGap)
                   && (lineCount <= duplexEndLine)
                   && ((duplexEndLine - lineCount) % 2 == 1))
            {
              blueSource -= scanner->bytes_per_scan_line;
              blue_offset -= scanner->bytes_per_scan_line;
            }

        }

      fflush (fp_front);

      data_left -= data_to_read;
      DBG (10, "reader_process(color, duplex): buffer of %d bytes read; %d bytes to go\n",
           data_to_read, data_left);
      /* FIXME: simon lai reported an overflow here! */
      memcpy (largeBuffer, largeBuffer + dataToProcess, lookAheadSize);
      readOffset = lookAheadSize;
    }
  while (data_left);
  free (linebuffer);
  if (largeBuffer != scanner->buffer)
    free (largeBuffer);
  fclose (fp_front);

  /*
   * If we're using a temp file, the data has already been written,
   * and the file MUST NOT be closed since the parent process still
   * needs to read from it. If we're using a pipe, stuff the data
   * from duplexBuffer into the pipe and release the duplex buffer.
   * 
   * When using a temp file, this subprocess will usually terminate
   * before the first byte of the duplex page is read; when using a
   * pipe, this subprocess will live until it has flushed the duplex
   * buffer into the pipe. Since pipes usually only hold about 8 KB
   * of data, the process will live until the front-end has fetched
   * almost everything.
   */
  if (scanner->use_temp_file)
    {
      fflush (fp_back);
    }
  else
    {
      fwrite (duplexBuffer, 1, duplexBufferSize, fp_back);
      fclose (fp_back);
      free (duplexBuffer);
    }

  return total_data_size;

}

static unsigned int
reader3091GrayDuplex (struct fujitsu *scanner, FILE * fp_front, FILE * fp_back)
{
  int status;
  unsigned int total_data_size, data_left, data_to_read;
  unsigned int duplexStartLine, duplexEndLine;
  unsigned int duplexBufferSize;
  unsigned char *duplexBuffer;
  unsigned char *duplexPointer;
  unsigned char *largeBuffer;
  unsigned int largeBufferSize = 0;
  unsigned char *source;
  unsigned int lineCount = 0;
  unsigned int i_data_read;

  data_left = scanner->bytes_per_scan_line * scanner->scan_height_pixels;

  /* Only allocate memory if we're not using a temp file. */
  if (scanner->use_temp_file)
    {
      duplexBuffer = duplexPointer = NULL;
      duplexBufferSize = 0;
    }
  else
    {
      duplexBuffer = malloc (duplexBufferSize = data_left);
      if (duplexBuffer == NULL)
        {
          DBG (MSG_ERR,
               "reader_process: out of memory for duplex buffer (try option --swapfile)\n");
          return (0);
        }
      duplexPointer = duplexBuffer;
    }

  data_left *= 2;
  largeBuffer = scanner->buffer;
  largeBufferSize =
    scanner->scsi_buf_size -
    (scanner->scsi_buf_size % scanner->bytes_per_scan_line);

  duplexStartLine =
    scanner->duplex_raster_offset * scanner->resolution_y / 300 + 1;
  duplexEndLine =
    scanner->scan_height_pixels * 2 -
    scanner->duplex_raster_offset * scanner->resolution_y / 300;

  total_data_size = data_left;

  do
    {
      data_to_read = (data_left < largeBufferSize) ? data_left : largeBufferSize;

      status = read_large_data_block (scanner, largeBuffer, data_to_read, 
                                      0, &i_data_read);

      switch (status)
        {
        case SANE_STATUS_GOOD:
          break;
        case SANE_STATUS_EOF:
          DBG (5, "reader_process: EOM (no more data) length = %d\n",
               scanner->i_transfer_length);
          data_to_read -= scanner->i_transfer_length;
          data_left = data_to_read;
          break;
        default:
          DBG (MSG_ERR, 
               "reader_process: unable to get image data from scanner!\n");
          fclose (fp_front);
          fclose (fp_back);
          return (0);
        }

      for (source = scanner->buffer; source < scanner->buffer + data_to_read;
           source += scanner->bytes_per_scan_line)
        {
          if ((lineCount < duplexStartLine) ||
              (((lineCount - duplexStartLine) % 2 == 1)
               && (lineCount < duplexEndLine)))
            {
              fwrite (source, 1, scanner->bytes_per_scan_line, fp_front);
            }
          else
            {
              if (scanner->use_temp_file)
                {
                  if ((int)
                      fwrite (source, 1, scanner->bytes_per_scan_line,
                              fp_back) != scanner->bytes_per_scan_line)
                    {
                      fclose (fp_back);
                      DBG (MSG_ERR,
                           "reader_process: out of disk space while writing temp file\n");
                      return (0);
                    }
                }
              else
                {
                  memcpy (duplexPointer, source,
                          scanner->bytes_per_scan_line);
                  duplexPointer += scanner->bytes_per_scan_line;
                }
            }
          lineCount++;
        }
      fflush (fp_front);

      data_left -= data_to_read;
      DBG (10, "reader_process(gray duplex): buffer of %d bytes read; %d bytes to go\n",
           data_to_read, data_left);
    }
  while (data_left);
  fclose (fp_front);

  /* see comment in reader3091ColorDuplex about this */
  if (scanner->use_temp_file)
    {
      fflush (fp_back);
    }
  else
    {
      fwrite (duplexBuffer, 1, duplexBufferSize, fp_back);
      fclose (fp_back);
      free (duplexBuffer);
    }

  return total_data_size;
}

/**
 * This method is called from within sane_control_option whenever the scan mode
 * (color, grayscale, ...) is changed. It has to change the members of the
 * "struct fujitsu" structure accordingly and also update the SANE option 
 * structures to reflect the fact that some options may now be newly available
 * or others unavailable.
 * 
 * This is the incarnation for the M3091DCd scanner.
 */
static SANE_Status
setMode3091 (struct fujitsu *scanner, int mode)
{
  /*
  static const SANE_Int allowableY3091Color[] = { 4, 75, 150, 300, 600 };
  static const SANE_Range allowableY3091Mono = { 50, 600, 1 };
  static const SANE_Range allowableX3091 = { 50, 300, 1 };
  */
  static const SANE_Range allowableThreshold = { 0, 255, 1 };

  SANE_Word tmp;

  scanner->color_mode = mode;

  switch (mode)
    {
    case MODE_LINEART:

      /* depth is fixed */
      scanner->scanner_depth = 1;
      scanner->output_depth = 1;

      /* threshold is available, default is 128 */
      scanner->threshold = 128;
      if (scanner->has_threshold) 
        {
          scanner->opt[OPT_THRESHOLD].cap =
            SANE_CAP_SOFT_DETECT | SANE_CAP_SOFT_SELECT;
        }
      scanner->opt[OPT_THRESHOLD].constraint_type = SANE_CONSTRAINT_RANGE;
      scanner->opt[OPT_THRESHOLD].constraint.range = &allowableThreshold;

      /* brightness is unavailable */
      scanner->brightness = 0;
      scanner->opt[OPT_BRIGHTNESS].cap = SANE_CAP_INACTIVE;

      /* lamp color is selectable */
      scanner->lamp_color = 0;
      scanner->opt[OPT_LAMP_COLOR].cap =
        SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;

      /* X resolution free from 50-300, Y resolution free from 50-600 */
      /*
      scanner->opt[OPT_X_RES].constraint_type = SANE_CONSTRAINT_RANGE;
      scanner->opt[OPT_X_RES].constraint.range = &allowableX3091;
      scanner->opt[OPT_Y_RES].constraint_type = SANE_CONSTRAINT_RANGE;
      scanner->opt[OPT_Y_RES].constraint.range = &allowableY3091Mono;
      */

      /* verify that the currently set resolutions aren't out of order */
      /* - for this specific scanner that's not necessary since */
      /* monochrome mode supports the  superset of resolutions  */
      calculateDerivedValues (scanner);
      return (SANE_STATUS_GOOD);

    case MODE_GRAYSCALE:

      /* 4bpp mode not yet implemented so depth is fixed at 8 */
      scanner->scanner_depth = 8;
      scanner->output_depth = 8;

      /* threshold is unavailable */
      scanner->threshold = 0;
      scanner->opt[OPT_THRESHOLD].cap = SANE_CAP_INACTIVE;

      /* brightness is unavailable */
      scanner->brightness = 0;
      scanner->opt[OPT_BRIGHTNESS].cap = SANE_CAP_INACTIVE;

      /* lamp color is selectable */
      scanner->lamp_color = 0;
      scanner->opt[OPT_LAMP_COLOR].cap =
        SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;

      /* X resolution free from 50-300, Y resolution free from 50-600 */
      /*
      scanner->opt[OPT_X_RES].constraint_type = SANE_CONSTRAINT_RANGE;
      scanner->opt[OPT_X_RES].constraint.range = &allowableX3091;
      scanner->opt[OPT_Y_RES].constraint_type = SANE_CONSTRAINT_RANGE;
      scanner->opt[OPT_Y_RES].constraint.range = &allowableY3091Mono;
      */

      /* verify that the currently set resolutions aren't out of order */
      /* - for this specific scanner that's not necessary since */
      /* monochrome mode supports the  superset of resolutions  */
      calculateDerivedValues (scanner);
      return (SANE_STATUS_GOOD);

    case MODE_COLOR:

      /* depth is fixed at 24 */
      scanner->scanner_depth = 24;
      scanner->output_depth = 24;

      /* threshold is unavailable */
      scanner->threshold = 0;
      scanner->opt[OPT_THRESHOLD].cap = SANE_CAP_INACTIVE;

      /* brightness is unavailable */
      scanner->brightness = 0;
      scanner->opt[OPT_BRIGHTNESS].cap = SANE_CAP_INACTIVE;

      /* lamp color is unavailable */
      scanner->lamp_color = 0;
      scanner->opt[OPT_LAMP_COLOR].cap = SANE_CAP_INACTIVE;

      /* X resolution free from 50-300, Y one out of 75, 150, 300, 600 */
      /*
      scanner->opt[OPT_X_RES].constraint_type = SANE_CONSTRAINT_RANGE;
      scanner->opt[OPT_X_RES].constraint.range = &allowableX3091;
      scanner->opt[OPT_Y_RES].constraint_type = SANE_CONSTRAINT_WORD_LIST;
      scanner->opt[OPT_Y_RES].constraint.word_list = allowableY3091Color;
      */

      /* verify that the currently set resolutions aren't out of order */

      /* x is uncritical but check y */

      tmp = scanner->resolution_y;
      if (sanei_constrain_value (scanner->opt + OPT_Y_RES, &tmp, NULL) ==
          SANE_STATUS_GOOD)
        {
          scanner->resolution_y = tmp;
        }
      else
        {
          scanner->resolution_y = 300;
        }
      calculateDerivedValues (scanner);
      return (SANE_STATUS_GOOD);

    }                           /* end of SELECT */

  return SANE_STATUS_INVAL;
}

/*
 * Initializes the "struct fujitsu" structure with meaningful values for the
 * M3091DCd scanner.
 */
static void
setDefaults3091 (struct fujitsu *scanner)
{

  scanner->use_adf = scanner->has_adf;
  scanner->duplex_mode = DUPLEX_FRONT;
  scanner->resolution_x = 300;
  scanner->resolution_y = 300;
  scanner->resolution_linked = SANE_TRUE;
  scanner->brightness = 0;
  scanner->threshold = 0;
  scanner->contrast = 0;
  scanner->reverse = 0;
  scanner->bitorder = 0;
  scanner->compress_type = 0;
  scanner->compress_arg = 0;
  /*scanner->scanning_order = 0;*/
  setMode3091 (scanner, MODE_COLOR);

  scanner->val[OPT_TL_X].w = 0;
  scanner->val[OPT_TL_Y].w = 0;
  /* this size is just big enough to match letter and A4 formats */
  if (scanner->model == MODEL_3092) 
    {
      scanner->bottom_right_x = SANE_FIX (215.9);
    } 
  else
    {
      scanner->bottom_right_x = SANE_FIX (215.0);
    }
  scanner->bottom_right_y = SANE_FIX (297.0);
  scanner->page_width = FIXED_MM_TO_SCANNER_UNIT (scanner->bottom_right_x);
  scanner->page_height = FIXED_MM_TO_SCANNER_UNIT (scanner->bottom_right_y);
  scanner->mirror = SANE_FALSE;
  scanner->use_temp_file = SANE_FALSE;

  /* required according to manual */

  scanner->opt[OPT_NOISE_REMOVAL].cap = SANE_CAP_INACTIVE;
  scanner->opt[OPT_BACKGROUND].cap = SANE_CAP_INACTIVE;
  scanner->opt[OPT_FILTERING].cap = SANE_CAP_INACTIVE;
  scanner->opt[OPT_SMOOTHING_MODE].cap = SANE_CAP_INACTIVE;
  scanner->opt[OPT_GRADATION].cap = SANE_CAP_INACTIVE;
  scanner->opt[OPT_THRESHOLD_CURVE].cap = SANE_CAP_INACTIVE;
  scanner->opt[OPT_VARIANCE_RATE].cap = SANE_CAP_INACTIVE;
  scanner->opt[OPT_MATRIX2X2].cap = SANE_CAP_INACTIVE;
  scanner->opt[OPT_MATRIX3X3].cap = SANE_CAP_INACTIVE;
  scanner->opt[OPT_MATRIX4X4].cap = SANE_CAP_INACTIVE;
  scanner->opt[OPT_MATRIX5X5].cap = SANE_CAP_INACTIVE;

  scanner->opt[OPT_DROPOUT_COLOR].cap = SANE_CAP_INACTIVE;
  scanner->dropout_color = MSEL_dropout_DEFAULT;

  scanner->sleep_time = 15;
  scanner->use_imprinter = SANE_FALSE;
}


/*
 * @@ Section 6 - M3096 specific internal routines
 */

/**
 * Initializes the "struct fujitsu" structure with meaningful values for the
 * M3096 scanner.
 */
static void
setDefaults3096 (struct fujitsu *scanner)
{

  scanner->use_adf = scanner->has_adf;
  scanner->duplex_mode = DUPLEX_FRONT;
  scanner->resolution_x = 300;
  scanner->resolution_y = 300;
  scanner->resolution_linked = SANE_TRUE;
  scanner->brightness = 0;
  scanner->threshold = 0;
  scanner->contrast = 0;
  scanner->reverse = 0;
  setMode3096 (scanner, MODE_GRAYSCALE);

  scanner->val[OPT_TL_X].w = 0;
  scanner->val[OPT_TL_Y].w = 0;
  /* this size is just big enough to match letter and A4 formats */
  scanner->bottom_right_x = SANE_FIX (215.9);
  scanner->bottom_right_y = SANE_FIX (297.0);

  if(!scanner->has_fixed_paper_size) 
    {

      scanner->page_width = FIXED_MM_TO_SCANNER_UNIT(scanner->bottom_right_x);
      scanner->page_height = FIXED_MM_TO_SCANNER_UNIT(scanner->bottom_right_y);
    }

  scanner->mirror = SANE_FALSE;
  scanner->use_temp_file = SANE_FALSE;

  scanner->bitorder = 0;
  scanner->compress_type = 0;
  scanner->compress_arg = 0;
  scanner->vendor_id_code = 0;
  scanner->outline = 0;
  scanner->emphasis = WD_emphasis_NONE;
  scanner->auto_sep = 0;
  scanner->mirroring = 0;
  scanner->var_rate_dyn_thresh = 0;
  scanner->dtc_threshold_curve = 0;

  scanner->gradation = WD_gradation_ORDINARY;
  scanner->smoothing_mode = WD_smoothing_IMAGE;
  scanner->filtering = WD_filtering_ORDINARY;
  scanner->background = WD_background_WHITE;
  scanner->matrix2x2 = 0;
  scanner->matrix3x3 = 0;
  scanner->matrix4x4 = 0;
  scanner->matrix5x5 = 0;
  scanner->noise_removal = 0;

  scanner->opt[OPT_MATRIX2X2].cap = SANE_CAP_INACTIVE;
  scanner->opt[OPT_MATRIX3X3].cap = SANE_CAP_INACTIVE;
  scanner->opt[OPT_MATRIX4X4].cap = SANE_CAP_INACTIVE;
  scanner->opt[OPT_MATRIX5X5].cap = SANE_CAP_INACTIVE;

  scanner->white_level_follow = WD_white_level_follow_DEFAULT;

  if (scanner->has_fixed_paper_size) 
    {
      scanner->page_width=FIXED_MM_TO_SCANNER_UNIT (scanner->bottom_right_x);
      scanner->page_height=FIXED_MM_TO_SCANNER_UNIT (scanner->bottom_right_y);
      scanner->opt[OPT_PAGE_HEIGHT].cap = SANE_CAP_INACTIVE;
      scanner->opt[OPT_PAGE_WIDTH].cap = SANE_CAP_INACTIVE;
    }

  scanner->dtc_selection = WD_dtc_selection_DEFAULT;
  scanner->opt[OPT_NOISE_REMOVAL].cap = SANE_CAP_INACTIVE;
  scanner->opt[OPT_BACKGROUND].cap = SANE_CAP_INACTIVE;
  scanner->opt[OPT_FILTERING].cap = SANE_CAP_INACTIVE;
  scanner->opt[OPT_SMOOTHING_MODE].cap = SANE_CAP_INACTIVE;
  scanner->opt[OPT_GRADATION].cap = SANE_CAP_INACTIVE;
  scanner->opt[OPT_THRESHOLD_CURVE].cap = SANE_CAP_INACTIVE;

  scanner->dropout_color = MSEL_dropout_DEFAULT;
  /* 3091 only option */
  scanner->opt[OPT_LAMP_COLOR].cap = SANE_CAP_INACTIVE;

  scanner->imprinter_direction = S_im_dir_top_bottom;
  scanner->imprinter_y_offset = 7;
  memcpy(scanner->imprinter_string, "%05ud", max_imprinter_string_length);
  scanner->imprinter_ctr_init = 0;
  scanner->imprinter_ctr_step = 1;
  scanner->imprinter_ctr_dir = S_im_dir_inc;

  scanner->sleep_time = 15;
  scanner->use_imprinter = SANE_FALSE;
}

/**
 * This method is called from within sane_control_option whenever the
 * scan mode (color, grayscale, ...) is changed. It has to change the
 * members of the "struct fujitsu" structure accordingly and also
 * update the SANE option structures to reflect the fact that some
 * options may now be newly available or others unavailable.
 * 
 * This is the incarnation for the M3096 scanner.
 */
static SANE_Status
setMode3096 (struct fujitsu *scanner, int mode)
{
  /*
  static const SANE_Int allowableResolutionsBW[] = { 4, 200, 240, 300, 400 };
  static const SANE_Int allowableResolutionsGray[] = { 3, 200, 300, 400 };
  */

  scanner->color_mode = mode;

  switch (mode)
    {
    case MODE_LINEART:

      scanner->reverse = SANE_FALSE;

      /* depth is fixed */
      scanner->scanner_depth = 1;
      scanner->output_depth = 1;

      /* threshold is available, default is 128 */
      scanner->threshold = 128;
      if (scanner->has_threshold)
        {
          scanner->opt[OPT_THRESHOLD].cap =
            SANE_CAP_SOFT_DETECT | SANE_CAP_SOFT_SELECT;
        }

      /* brightness is unavailable */
      scanner->brightness = 0;
      scanner->opt[OPT_BRIGHTNESS].cap = SANE_CAP_INACTIVE;

      if (scanner->has_gamma)
	{
	  scanner->opt[OPT_GAMMA].cap =
	    SANE_CAP_SOFT_DETECT | SANE_CAP_SOFT_SELECT;;
	}

      /* X resolution and Y resolution in steps including 240 
      scanner->opt[OPT_X_RES].constraint_type = SANE_CONSTRAINT_WORD_LIST;
      scanner->opt[OPT_X_RES].constraint.word_list = allowableResolutionsBW;
      scanner->opt[OPT_Y_RES].constraint_type = SANE_CONSTRAINT_WORD_LIST;
      scanner->opt[OPT_Y_RES].constraint.word_list = allowableResolutionsBW;
      */
      if (scanner->has_dropout_color)
        {
          scanner->opt[OPT_DROPOUT_COLOR].cap =
            SANE_CAP_SOFT_DETECT | SANE_CAP_SOFT_SELECT;
        }

      /* verify that the currently set resolutions aren't out of order */
      /* - for this specific scanner that's not necessary since */
      /* monochrome mode supports the superset of resolutions  */
      calculateDerivedValues (scanner);
      return (SANE_STATUS_GOOD);

    case MODE_HALFTONE:

      scanner->reverse = SANE_FALSE;

      /* depth is fixed */
      scanner->scanner_depth = 1;
      scanner->output_depth = 1;

      /* threshold is only for line art mode. */
      scanner->threshold = 0;
      scanner->opt[OPT_THRESHOLD].cap = SANE_CAP_INACTIVE;

      /* brightness is available, 8 steps */
      scanner->brightness = 0x80;
      if (scanner->has_brightness) 
        {
          scanner->opt[OPT_BRIGHTNESS].cap =
            SANE_CAP_SOFT_DETECT | SANE_CAP_SOFT_SELECT;
        }

      if (scanner->has_gamma)
	{
	  scanner->opt[OPT_GAMMA].cap =
	    SANE_CAP_SOFT_DETECT | SANE_CAP_SOFT_SELECT;
	}

      /* X resolution and Y resolution in steps including 240 
      scanner->opt[OPT_X_RES].constraint_type = SANE_CONSTRAINT_WORD_LIST;
      scanner->opt[OPT_X_RES].constraint.word_list = allowableResolutionsBW;
      scanner->opt[OPT_Y_RES].constraint_type = SANE_CONSTRAINT_WORD_LIST;
      scanner->opt[OPT_Y_RES].constraint.word_list = allowableResolutionsBW;
      */
      if (scanner->has_dropout_color)
        {
          scanner->opt[OPT_DROPOUT_COLOR].cap =
            SANE_CAP_SOFT_DETECT | SANE_CAP_SOFT_SELECT;
        }
      /* verify that the currently set resolutions aren't out of order */
      /* - for this specific scanner that's not necessary since */
      /* monochrome mode supports the superset of resolutions  */
      calculateDerivedValues (scanner);
      return (SANE_STATUS_GOOD);

    case MODE_GRAYSCALE:

      scanner->reverse = SANE_FALSE;

      /* depth fixed at 8 */
      scanner->scanner_depth = 8;
      scanner->output_depth = 8;

      /* threshold is unavailable */
      scanner->threshold = 0;
      scanner->opt[OPT_THRESHOLD].cap = SANE_CAP_INACTIVE;

      /* brightness is unavailable */
      scanner->brightness = 0;
      scanner->opt[OPT_BRIGHTNESS].cap = SANE_CAP_INACTIVE;

      /* Gamma unavailable. */
      if (scanner->has_gamma) 
	{
	  scanner->opt[OPT_GAMMA].cap = SANE_CAP_INACTIVE;
	}
      
      switch (scanner->compress_type) 
	{
	case WD_cmp_MH:
	case WD_cmp_MR:
	case WD_cmp_MMR:
	  scanner->compress_type = WD_cmp_NONE;
	  break;
	}

      /* X and y in steps without the 240 
      scanner->opt[OPT_X_RES].constraint_type = SANE_CONSTRAINT_WORD_LIST;
      scanner->opt[OPT_X_RES].constraint.word_list = allowableResolutionsGray;
      scanner->opt[OPT_Y_RES].constraint_type = SANE_CONSTRAINT_WORD_LIST;
      scanner->opt[OPT_Y_RES].constraint.word_list = allowableResolutionsGray;
      */

      if (scanner->has_dropout_color)
        {
          scanner->opt[OPT_DROPOUT_COLOR].cap =
            SANE_CAP_SOFT_DETECT | SANE_CAP_SOFT_SELECT;
        }
      /* verify that the currently set resolutions aren't out of order */
      /*
      if (scanner->resolution_x == 240)
        scanner->resolution_x = 300;
      if (scanner->resolution_y == 240)
        scanner->resolution_y = 300;
      */
      calculateDerivedValues (scanner);
      return (SANE_STATUS_GOOD);

    case MODE_COLOR:

      scanner->scanner_depth = 24;
      scanner->output_depth = 24;
      calculateDerivedValues (scanner);
      return (SANE_STATUS_GOOD);
      
    }                           /* end of SELECT */

  return (SANE_STATUS_INVAL);
}

/*
 * @@ Section 7 - EVPD specific internal routines
 */

/*
 * @@ Section 8 - M3092 specific internal routines
 */


