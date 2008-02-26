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
    - M309x (Frederik Ramm, Randolph Bentson, Oliver Schirrmeister)
    - M409x (Oliver Schirrmeister)
    - fi-series (M. Allan Noah, Oliver Schirrmeister)

   The source code is divided in sections which you can easily find by
   searching for the tag "@@".

   Section 1 - Init & static stuff
   Section 2 - sane_init, _get_devices, _open & friends
   Section 3 - sane_*_option functions
   Section 4 - sane_start, _get_param, _read & friends
   Section 5 - sane_close functions
   Section 6 - misc functions

   Changes:
      V 1.0.2, 2002-05-05, OS
         - release memory allocated by sane_get_devices
         - several bugfixes
         - supports the M3097
         - get threshold, contrast and brightness from vpd
         - imprinter support
         - get_hardware_status now works before calling sane_start
         - avoid unnecessary reload of options when using source=fb
      V 1.0.3, 2002-08-08, OS
         - bugfix. Imprinter didn't print the first time after
           switching on the scanner
         - bugfix. reader_generic_passthrough ignored the number of bytes
           returned by the scanner
      V 1.0.4, 2002-09-13, OS
         - 3092 support (mgoppold a t tbz-pariv.de)
         - tested 4097 support
         - changed some functions to receive compressed data
      V 1.0.4, 2003-02-13, OS
         - fi-4220C support (ron a t roncemer.com)
         - SCSI over USB support (ron a t roncemer.com)
      V 1.0.5, 2003-02-20, OS
         - set availability of options THRESHOLD und VARIANCE
         - option RIF is available for 3091 and 3092
      V 1.0.6, 2003-03-04, OS
         - renamed some variables
         - bugfix: duplex scanning now works when disconnect is enabled
      V 1.0.7, 2003-03-10, OS
         - displays the offending byte in the window descriptor block
      V 1.0.8, 2003-03-28, OS
         - fi-4120C support, MAN
         - display information about gamma in vital_product_data
      V 1.0.9 2003-06-04, MAN
         - separated the 4120 and 4220 into another model
         - color support for the 4x20
      V 1.0.10 2003-06-04, MAN
         - removed SP15 code
         - sane_open actually opens the device you request
      V 1.0.11 2003-06-11, MAN
         - fixed bug in that code when a scanner is disconnected 
      V 1.0.12 2003-10-06, MAN
         - added code to support color modes of more recent scanners
      V 1.0.13 2003-11-07, OS
	 - Bugfix. If a scanner returned a color image
	   in format rr...r gg...g bb...b the reader process crashed
	 - Bugfix. Disable option gamma was for the fi-4120
      V 1.0.14 2003-12-15, OS
         - Bugfix: set default threshold range to 0..255 There is a problem
           with the M3093 when you are not allows to set the threshold to 0
         - Bugfix: set the allowable x- and y-DPI values from VPD. Scanning
           with x=100 and y=100 dpi with an fi4120 resulted in an image
           with 100,75 dpi
         - Bugfix: Set the default value of gamma to 0x80 for all scanners
           that don't have built in gamma patterns
         - Bugfix: fi-4530 and fi-4210 don't support standard paper size
      V 1.0.15 2003-12-16, OS
         - Bugfix: pagewidth and pageheight were disabled for the fi-4530C
      V 1.0.16 2004-02-20, OS
         - merged the 3092-routines with the 3091-routines
         - inverted the image in mode color and grayscale
         - jpg hardware compression support (fi-4530C)
      V 1.0.17 2004-03-04, OS
         - enabled option dropoutcolor for the fi-4530C, and fi-4x20C
      V 1.0.18 2004-06-02, OS
         - bugfix: can read duplex color now
      V 1.0.19 2004-06-28, MAN
         - 4220 use model code not strcmp (stan a t saticed.me.uk)
      V 1.0.20 2004-08-24, OS
         - bugfix: 3091 did not work since 15.12.2003 
         - M4099 supported (bw only)
      V 1.0.21 2006-05-01, MAN
         - Complete rewrite, half code size
         - better (read: correct) usb command support
         - basic support for most fi-series
         - most scanner capabilities read from VPD
         - reduced model-specific code
         - improved scanner detection/initialization
         - improved SANE_Option handling
         - basic button support
         - all IPC and Imprinter options removed temporarily
         - duplex broken temporarily
      V 1.0.22 2006-05-04, MAN
         - do_scsi_cmd gets basic looping capability
         - reverse now divided by mode
         - re-write sane_fix/unfix value handling
         - fix several bugs in options code
         - some options' ranges modified by other options vals
         - added advanced read-only options for all
           known hardware sensors and buttons
         - rewrote hw status function
         - initial testing with M3091dc- color mode broken
      V 1.0.23 2006-05-14, MAN
         - initial attempt to recover duplex mode
         - fix bad usb prodID when config file missing
      V 1.0.24 2006-05-17, MAN
         - sane_read must set len=0 when return != good
         - simplify do_cmd() calls by removing timeouts
         - lengthen most timeouts, shorten those for wait_scanner()
      V 1.0.25 2006-05-19, MAN
         - rename scsi-buffer-size to buffer-size, usb uses it too
         - default buffer-size increased to 64k
         - use sanei_scsi_open_extended() to set buffer size
         - fix some compiler warns: 32&64 bit gcc
      V 1.0.26 2006-05-23, MAN
         - dont send scanner control (F1) if unsupported
      V 1.0.27 2006-05-30, MAN
         - speed up hexdump (adeuring A T gmx D O T net)
         - duplex request same size block from both sides
         - dont #include or call sanei_thread
         - split usb/scsi command DBG into 25 and 30
      V 1.0.28 2006-06-01, MAN
         - sane_read() usleep if scanner is busy
         - do_*_cmd() no looping (only one caller used it),
           remove unneeded casts, cleanup/add error messages
         - scanner_control() look at correct has_cmd_* var,
           handles own looping on busy
      V 1.0.29 2006-06-04, MAN
         - M3091/2 Color mode support (duplex still broken)
         - all sensors option names start with 'button-'
         - rewrite sane_read and helpers to use buffers,
           currently an extreme waste of ram, but should
           work with saned and scanimage -T
         - merge color conversion funcs into read_from_buf()
         - compare bytes tx v/s rx instead of storing EOFs
         - remove scanner cmd buf, use buf per func instead
         - print color and duplex raster offsets (inquiry)
         - print EOM, ILI, and info bytes (request sense)
      V 1.0.30 2006-06-06, MAN
         - M3091/2 duplex support, color/gray/ht/lineart ok
         - sane_read helpers share code, report more errors
         - add error msg if VPD missing or non-extended
         - remove references to color_lineart and ht units
         - rework init_model to support more known models
         - dont send paper size data if using flatbed
      V 1.0.31 2006-06-13, MAN
         - add 5220C usb id
         - dont show ink level buttons if no imprinter
         - run ghs/rs every second instead of every other
      V 1.0.32 2006-06-14, MAN
         - add 4220C2 usb id
      V 1.0.33 2006-06-14, MAN (SANE v1.0.18)
         - add Fi-5900 usb id and init_model section
      V 1.0.34 2006-07-04, MAN
         - add S500 usb id
         - gather more data from inq and vpd
         - allow background color setting
      V 1.0.35 2006-07-05, MAN
         - allow double feed sensor settings
         - more consistent naming of global strings
      V 1.0.36 2006-07-06, MAN
         - deal with fi-5900 even bytes problem
         - less verbose calculateDerivedValues()
      V 1.0.37 2006-07-14, MAN
         - mode sense command support
         - detect mode page codes instead of hardcoding
         - send command support
         - brightness/contrast support via LUT
         - merge global mode page buffers
      V 1.0.38 2006-07-15, MAN
         - add 'useless noise' debug level (35)
         - move mode sense probe errors to DBG 35
      V 1.0.39 2006-07-17, MAN
         - rewrite contrast slope math for readability
      V 1.0.40 2006-08-26, MAN
         - rewrite brightness/contrast more like xsane
         - initial gamma support
         - add fi-5530 usb id
         - rewrite do_*_cmd functions to handle short reads
           and to use ptr to return read in length
         - new init_user function split from init_model
         - init_vpd allows short vpd block for older models
         - support MS buffer (s.scipioni AT harvardgroup DOT it)
         - support MS prepick
         - read only 1 byte of mode sense output
      V 1.0.41 2006-08-28, MAN
         - do_usb_cmd() returns io error on cmd/out/status/rs EOF
         - fix bug in MS buffer/prepick scsi data block
      V 1.0.42 2006-08-31, MAN
         - fix bug in get_hardware_status (#303798)
      V 1.0.43 2006-09-19, MAN
         - add model-specific code to init_vpd for M3099
      V 1.0.44 2007-01-26, MAN
         - set SANE_CAP_HARD_SELECT on all buttons/sensors
         - disable sending gamma LUT, seems wrong on some units?
         - support MS overscan
         - clamp the scan area to the pagesize on ADF
      V 1.0.45 2007-01-28, MAN
         - update overscan code to extend max scan area
      V 1.0.46 2007-03-08, MAN
         - tweak fi-4x20c2 and M3093 settings
	 - add fi-5110EOXM usb id
	 - add M3093 non-alternating duplex code
      V 1.0.47 2007-04-13, MAN
         - change window_gamma determination
         - add fi-5650C usb id and color mode
      V 1.0.48 2007-04-16, MAN
         - re-enable brightness/contrast for built-in models 
      V 1.0.49 2007-06-28, MAN
         - add fi-5750C usb id and color mode
      V 1.0.50 2007-07-10, MAN
         - updated overscan and bgcolor option descriptions
	 - added jpeg output support
         - restructured usb reading code to use RS len for short reads
         - combined calcDerivedValues with sane_get_params
      V 1.0.51 2007-07-26, MAN
	 - fix bug in jpeg output support
      V 1.0.52 2007-07-27, MAN
	 - remove unused jpeg function
	 - reactivate look-up-table based brightness and contrast options
	 - change range of hardware brightness/contrast to match LUT versions
	 - call send_lut() from sane_control_option instead of sane_start
      V 1.0.53 2007-11-18, MAN
         - add S510 usb id
	 - OPT_NUM_OPTS type is SANE_TYPE_INT (jblache)
      V 1.0.54 2007-12-29, MAN
	 - disable SANE_FRAME_JPEG support until SANE 1.1.0
      V 1.0.55 2007-12-29, MAN (SANE v1.0.19)
	 - add S500M usb id
      V 1.0.56 2008-02-14, MAN
	 - sanei_config_read has already cleaned string (#310597)
      V 1.0.57 2008-02-24, MAN
         - fi-5900 does not (initially) interlace colors
	 - add mode sense for color interlacing? (page code 32)
	 - more debug output in init_ms()

   SANE FLOW DIAGRAM

   - sane_init() : initialize backend
   . - sane_get_devices() : query list of scanner devices
   . - sane_open() : open a particular scanner device
   . . - sane_set_io_mode : set blocking mode
   . . - sane_get_select_fd : get scanner fd
   . .
   . . - sane_get_option_descriptor() : get option information
   . . - sane_control_option() : change option values
   . . - sane_get_parameters() : returns estimated scan parameters
   . . - (repeat previous 3 functions)
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

/*
 * @@ Section 1 - Init
 */

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
#include <math.h>

#include <sys/types.h>
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
#define BUILD 57 

/* values for SANE_DEBUG_FUJITSU env var:
 - errors           5
 - function trace  10
 - function detail 15
 - get/setopt cmds 20
 - scsi/usb trace  25 
 - scsi/usb detail 30
 - useless noise   35
*/

/* ------------------------------------------------------------------------- */
static const char string_Flatbed[] = "Flatbed";
static const char string_ADFFront[] = "ADF Front";
static const char string_ADFBack[] = "ADF Back";
static const char string_ADFDuplex[] = "ADF Duplex";

static const char string_Lineart[] = "Lineart";
static const char string_Halftone[] = "Halftone";
static const char string_Grayscale[] = "Gray";
static const char string_Color[] = "Color";

static const char string_Default[] = "Default";
static const char string_On[] = "On";
static const char string_Off[] = "Off";

static const char string_Red[] = "Red";
static const char string_Green[] = "Green";
static const char string_Blue[] = "Blue";
static const char string_White[] = "White";
static const char string_Black[] = "Black";

static const char string_None[] = "None";
static const char string_JPEG[] = "JPEG";

static const char string_Thickness[] = "Thickness";
static const char string_Length[] = "Length";
static const char string_Both[] = "Both";

static const char string_10mm[] = "10mm";
static const char string_15mm[] = "15mm";
static const char string_20mm[] = "20mm";

/* Also set via config file. */
static int global_buffer_size = 64 * 1024;

/*
 * used by attach* and sane_get_devices
 * a ptr to a null term array of ptrs to SANE_Device structs
 * a ptr to a single-linked list of fujitsu structs
 */
static const SANE_Device **sane_devArray = NULL;
static struct fujitsu *fujitsu_devList = NULL;

/*
 * @@ Section 2 - SANE & scanner init code
 */

/*
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
  authorize = authorize;        /* get rid of compiler warning */

  DBG_INIT ();
  DBG (10, "sane_init: start\n");

  sanei_usb_init();

  if (version_code)
    *version_code = SANE_VERSION_CODE (V_MAJOR, V_MINOR, BUILD);

  DBG (5, "sane_init: fujitsu backend %d.%d.%d, from %s\n",
    V_MAJOR, V_MINOR, BUILD, PACKAGE_STRING);

  DBG (10, "sane_init: finish\n");

  return SANE_STATUS_GOOD;
}

/*
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
  SANE_Status ret;

  local_only = local_only;        /* get rid of compiler warning */

  DBG (10, "sane_get_devices: start\n");

  /* load the two global lists of device structs */
  ret = find_scanners();

  *device_list = sane_devArray;

  DBG (10, "sane_get_devices: finish\n");

  return ret;
}

/*
 * Read the config file, find scanners with help from sanei_*
 * store in global device structs
 *
 * We place the complex init code here, so it can be called
 * from sane_get_devices() and sane_open()
 */
SANE_Status
find_scanners ()
{
  struct fujitsu *dev;
  char line[PATH_MAX];
  const char *lp;
  FILE *fp;
  int num_devices=0;
  int i=0;

  DBG (10, "find_scanners: start\n");

  /* set this to 64K before reading the file */
  global_buffer_size = 64 * 1024;

  fp = sanei_config_open (FUJITSU_CONFIG_FILE);

  if (fp) {

      DBG (15, "find_scanners: reading config file %s\n", FUJITSU_CONFIG_FILE);

      while (sanei_config_read (line, PATH_MAX, fp)) {
    
          lp = line;
    
          /* ignore comments */
          if (*lp == '#')
            continue;
    
          /* skip empty lines */
          if (*lp == 0)
            continue;
    
          if ((strncmp ("option", lp, 6) == 0) && isspace (lp[6])) {
    
              lp += 6;
              lp = sanei_config_skip_whitespace (lp);
    
              /* we allow setting buffersize too big */
              if ((strncmp (lp, "buffer-size", 11) == 0) && isspace (lp[11])) {
    
                  int buf;
                  lp += 11;
                  lp = sanei_config_skip_whitespace (lp);
                  buf = atoi (lp);
    
                  if (buf < 4096) {
                    DBG (5, "find_scanners: config option \"buffer-size\" (%d) is < 4096, ignoring!\n", buf);
                    continue;
                  }
    
                  if (buf > 64*1024) {
                    DBG (5, "find_scanners: config option \"buffer-size\" (%d) is > %d, warning!\n", buf, 64*1024);
                  }
    
                  DBG (15, "find_scanners: setting \"buffer-size\" to %d\n", buf);
                  global_buffer_size = buf;
              }
              else {
                  DBG (5, "find_scanners: config option \"%s\" unrecognized - ignored.\n", lp);
              }
          }
          else if ((strncmp ("usb", lp, 3) == 0) && isspace (lp[3])) {
              DBG (15, "find_scanners: looking for '%s'\n", lp);
              sanei_usb_attach_matching_devices(lp, attach_one_usb);
          }
          else if ((strncmp ("scsi", lp, 4) == 0) && isspace (lp[4])) {
              DBG (15, "find_scanners: looking for '%s'\n", lp);
              sanei_config_attach_matching_devices (lp, attach_one_scsi);
          }
          else{
              DBG (5, "find_scanners: config line \"%s\" unrecognized - ignored.\n", lp);
          }
      }
      fclose (fp);
  }

  else {
      DBG (5, "find_scanners: no config file '%s', using defaults\n", FUJITSU_CONFIG_FILE);

      DBG (15, "find_scanners: looking for 'scsi FUJITSU'\n");
      sanei_config_attach_matching_devices ("scsi FUJITSU", attach_one_scsi);

      DBG (15, "find_scanners: looking for 'usb 0x04c5 0x1041'\n");
      sanei_usb_attach_matching_devices("usb 0x04c5 0x1041", attach_one_usb);

      DBG (15, "find_scanners: looking for 'usb 0x04c5 0x1042'\n");
      sanei_usb_attach_matching_devices("usb 0x04c5 0x1042", attach_one_usb);

      DBG (15, "find_scanners: looking for 'usb 0x04c5 0x1095'\n");
      sanei_usb_attach_matching_devices("usb 0x04c5 0x1095", attach_one_usb);

      DBG (15, "find_scanners: looking for 'usb 0x04c5 0x1096'\n");
      sanei_usb_attach_matching_devices("usb 0x04c5 0x1096", attach_one_usb);

      DBG (15, "find_scanners: looking for 'usb 0x04c5 0x1097'\n");
      sanei_usb_attach_matching_devices("usb 0x04c5 0x1097", attach_one_usb);

      DBG (15, "find_scanners: looking for 'usb 0x04c5 0x10ad'\n");
      sanei_usb_attach_matching_devices("usb 0x04c5 0x10ad", attach_one_usb);

      DBG (15, "find_scanners: looking for 'usb 0x04c5 0x10ae'\n");
      sanei_usb_attach_matching_devices("usb 0x04c5 0x10ae", attach_one_usb);

      DBG (15, "find_scanners: looking for 'usb 0x04c5 0x10af'\n");
      sanei_usb_attach_matching_devices("usb 0x04c5 0x10af", attach_one_usb);

      DBG (15, "find_scanners: looking for 'usb 0x04c5 0x10e0'\n");
      sanei_usb_attach_matching_devices("usb 0x04c5 0x10e0", attach_one_usb);

      DBG (15, "find_scanners: looking for 'usb 0x04c5 0x10e1'\n");
      sanei_usb_attach_matching_devices("usb 0x04c5 0x10e1", attach_one_usb);

      DBG (15, "find_scanners: looking for 'usb 0x04c5 0x10e2'\n");
      sanei_usb_attach_matching_devices("usb 0x04c5 0x10e2", attach_one_usb);

      DBG (15, "find_scanners: looking for 'usb 0x04c5 0x10e7'\n");
      sanei_usb_attach_matching_devices("usb 0x04c5 0x10e7", attach_one_usb);

      DBG (15, "find_scanners: looking for 'usb 0x04c5 0x10f2'\n");
      sanei_usb_attach_matching_devices("usb 0x04c5 0x10f2", attach_one_usb);

      DBG (15, "find_scanners: looking for 'usb 0x04c5 0x10fe'\n");
      sanei_usb_attach_matching_devices("usb 0x04c5 0x10fe", attach_one_usb);

      DBG (15, "find_scanners: looking for 'usb 0x04c5 0x1135'\n");
      sanei_usb_attach_matching_devices("usb 0x04c5 0x1135", attach_one_usb);

      DBG (15, "find_scanners: looking for 'usb 0x04c5 0x1155'\n");
      sanei_usb_attach_matching_devices("usb 0x04c5 0x1155", attach_one_usb);
  }

  for (dev = fujitsu_devList; dev; dev=dev->next) {
    DBG (15, "find_scanners: found scanner %s\n",dev->device_name);
    num_devices++;
  }

  DBG (15, "find_scanners: found %d scanner(s)\n",num_devices);

  sane_devArray = calloc (num_devices + 1, sizeof (SANE_Device*));
  if (!sane_devArray)
    return SANE_STATUS_NO_MEM;

  for (dev = fujitsu_devList; dev; dev=dev->next) {
    sane_devArray[i++] = (SANE_Device *)&dev->sane;
  }

  sane_devArray[i] = 0;

  DBG (10, "find_scanners: finish\n");

  return SANE_STATUS_GOOD;
}

/* callbacks used by find_scanners */
static SANE_Status
attach_one_scsi (const char *device_name)
{
  return attach_one(device_name,CONNECTION_SCSI);
}

static SANE_Status
attach_one_usb (const char *device_name)
{
  return attach_one(device_name,CONNECTION_USB);
}

/* build the scanner struct and link to global list 
 * unless struct is already loaded, then pretend 
 */
static SANE_Status
attach_one (const char *device_name, int connType)
{
  struct fujitsu *s;
  int ret;

  DBG (10, "attach_one: start\n");
  DBG (15, "attach_one: looking for '%s'\n", device_name);

  for (s = fujitsu_devList; s; s = s->next) {
    if (strcmp (s->sane.name, device_name) == 0) {
      DBG (10, "attach_one: already attached!\n");
      return SANE_STATUS_GOOD;
    }
  }

  /* build a fujitsu struct to hold it */
  if ((s = calloc (sizeof (*s), 1)) == NULL)
    return SANE_STATUS_NO_MEM;

  /* scsi command/data buffer */
  s->buffer_size = global_buffer_size;

  /* copy the device name */
  s->device_name = strdup (device_name);
  if (!s->device_name){
    free (s);
    return SANE_STATUS_NO_MEM;
  }

  /* connect the fd */
  s->connection = connType;
  s->fd = -1;
  s->fds[0] = -1;
  s->fds[1] = -1;
  ret = connect_fd(s);
  if(ret != SANE_STATUS_GOOD){
    free (s->device_name);
    free (s);
    return ret;
  }

  /* Now query the device to load its vendor/model/version */
  ret = init_inquire (s);
  if (ret != SANE_STATUS_GOOD) {
    disconnect_fd(s);
    free (s->device_name);
    free (s);
    DBG (5, "attach_one: inquiry failed\n");
    return ret;
  }

  /* load detailed specs/capabilities from the device */
  ret = init_vpd (s);
  if (ret != SANE_STATUS_GOOD) {
    disconnect_fd(s);
    free (s->device_name);
    free (s);
    DBG (5, "attach_one: vpd failed\n");
    return ret;
  }

  /* see what mode pages device supports */
  ret = init_ms (s);
  if (ret != SANE_STATUS_GOOD) {
    disconnect_fd(s);
    free (s->device_name);
    free (s);
    DBG (5, "attach_one: ms failed\n");
    return ret;
  }

  /* clean up the scanner struct based on model */
  /* this is the only piece of model specific code */
  ret = init_model (s);
  if (ret != SANE_STATUS_GOOD) {
    disconnect_fd(s);
    free (s->device_name);
    free (s);
    DBG (5, "attach_one: model failed\n");
    return ret;
  }

  /* sets SANE option 'values' to good defaults */
  ret = init_user (s);
  if (ret != SANE_STATUS_GOOD) {
    disconnect_fd(s);
    free (s->device_name);
    free (s);
    DBG (5, "attach_one: user failed\n");
    return ret;
  }

  ret = init_options (s);
  if (ret != SANE_STATUS_GOOD) {
    disconnect_fd(s);
    free (s->device_name);
    free (s);
    DBG (5, "attach_one: options failed\n");
    return ret;
  }

  /* we close the connection, so that another backend can talk to scanner */
  disconnect_fd(s);

  /* load info into sane_device struct */
  s->sane.name = s->device_name;
  s->sane.vendor = s->vendor_name;
  s->sane.model = s->product_name;
  s->sane.type = "scanner";

  s->next = fujitsu_devList;
  fujitsu_devList = s;

  DBG (10, "attach_one: finish\n");

  return SANE_STATUS_GOOD;
}

/*
 * connect the fd in the scanner struct
 */
static SANE_Status
connect_fd (struct fujitsu *s)
{
  SANE_Status ret;
  int buffer_size = s->buffer_size;

  DBG (10, "connect_fd: start\n");

  if(s->fd > -1){
    DBG (5, "connect_fd: already open\n");
    ret = SANE_STATUS_GOOD;
  }
  else if (s->connection == CONNECTION_USB) {
    DBG (15, "connect_fd: opening USB device\n");
    ret = sanei_usb_open (s->device_name, &(s->fd));
  }
  else {
    DBG (15, "connect_fd: opening SCSI device\n");
    ret = sanei_scsi_open_extended (s->device_name, &(s->fd), sense_handler, s, &s->buffer_size);
    if(ret == SANE_STATUS_GOOD && buffer_size != s->buffer_size){
      DBG (5, "connect_fd: cannot get requested buffer size (%d/%d)\n", buffer_size, s->buffer_size);
      ret = SANE_STATUS_NO_MEM;
    }
  }

  if(ret == SANE_STATUS_GOOD){

    /* first generation usb scanners can get flaky if not closed 
     * properly after last use. very first commands sent to device 
     * must be prepared to correct this- see wait_scanner() */
    ret = wait_scanner(s);
    if (ret != SANE_STATUS_GOOD) {
      DBG (5, "connect_fd: could not wait_scanner\n");
      disconnect_fd(s);
    }

  }
  else{
    DBG (5, "connect_fd: could not open device: %d\n", ret);
  }

  DBG (10, "connect_fd: finish\n");

  return ret;
}

/*
 * This routine will check if a certain device is a Fujitsu scanner
 * It also copies interesting data from INQUIRY into the handle structure
 */
static SANE_Status
init_inquire (struct fujitsu *s)
{
  int i;
  SANE_Status ret;
  unsigned char buffer[96];
  size_t inLen = sizeof(buffer);

  DBG (10, "init_inquire: start\n");

  set_IN_return_size (inquiryB.cmd, inLen);
  set_IN_evpd (inquiryB.cmd, 0);
  set_IN_page_code (inquiryB.cmd, 0);
 
  ret = do_cmd (
    s, 1, 0, 
    inquiryB.cmd, inquiryB.size,
    NULL, 0,
    buffer, &inLen
  );

  if (ret != SANE_STATUS_GOOD){
    return ret;
  }

  if (get_IN_periph_devtype (buffer) != IN_periph_devtype_scanner){
    DBG (5, "The device at '%s' is not a scanner.\n", s->device_name);
    return SANE_STATUS_INVAL;
  }

  get_IN_vendor (buffer, s->vendor_name);
  get_IN_product (buffer, s->product_name);
  get_IN_version (buffer, s->version_name);

  s->vendor_name[8] = 0;
  s->product_name[16] = 0;
  s->version_name[4] = 0;

  /* gobble trailing spaces */
  for (i = 7; s->vendor_name[i] == ' ' && i >= 0; i--)
    s->vendor_name[i] = 0;
  for (i = 15; s->product_name[i] == ' ' && i >= 0; i--)
    s->product_name[i] = 0;
  for (i = 3; s->version_name[i] == ' ' && i >= 0; i--)
    s->version_name[i] = 0;

  if (strcmp ("FUJITSU", s->vendor_name)) {
    DBG (5, "The device at '%s' is reported to be made by '%s'\n", s->device_name, s->vendor_name);
    DBG (5, "This backend only supports Fujitsu products.\n");
    return SANE_STATUS_INVAL;
  }

  DBG (15, "init_inquire: Found %s scanner %s version %s at %s\n",
    s->vendor_name, s->product_name, s->version_name, s->device_name);

  /*some scanners list random data here*/
  DBG (15, "inquiry options\n");

  s->color_raster_offset = get_IN_color_offset(buffer);
  DBG (15, "  color offset: %d lines\n",s->color_raster_offset);

  /* FIXME: we dont store all of these? */
  DBG (15, "  long color scan: %d\n",get_IN_long_color(buffer));
  DBG (15, "  long gray scan: %d\n",get_IN_long_gray(buffer));
  DBG (15, "  3091 duplex: %d\n",get_IN_duplex_3091(buffer));

  s->has_bg_front = get_IN_bg_front(buffer);
  DBG (15, "  background front: %d\n",s->has_bg_front);

  s->has_bg_back = get_IN_bg_back(buffer);
  DBG (15, "  background back: %d\n",s->has_bg_back);

  DBG (15, "  emulation mode: %d\n",get_IN_emulation(buffer));

  s->duplex_raster_offset = get_IN_duplex_offset(buffer);
  DBG (15, "  duplex offset: %d lines\n",s->duplex_raster_offset);

  DBG (10, "init_inquire: finish\n");

  return SANE_STATUS_GOOD;
}

/*
 * Use INQUIRY VPD to setup more detail about the scanner
 */
static SANE_Status
init_vpd (struct fujitsu *s)
{
  SANE_Status ret;
  unsigned char buffer[0x68];
  size_t inLen = sizeof(buffer);

  DBG (10, "init_vpd: start\n");

  /* get EVPD */
  set_IN_return_size (inquiryB.cmd, inLen);
  set_IN_evpd (inquiryB.cmd, 1);
  set_IN_page_code (inquiryB.cmd, 0xf0);

  ret = do_cmd (
    s, 1, 0,
    inquiryB.cmd, inquiryB.size,
    NULL, 0,
    buffer, &inLen
  );

  /* M3099 gives all data, but wrong length */
  if (strstr (s->product_name, "M3099")
    && (ret == SANE_STATUS_GOOD || ret == SANE_STATUS_EOF)
    && get_IN_page_length (buffer) == 0x19){
      DBG (5, "init_vpd: 3099 repair\n");
      set_IN_page_length(buffer,0x5f);
  }

  /* some(all?) versions of 3097 dont have vpd?
  else if (strstr (s->product_name, "M3097")
    && (ret == SANE_STATUS_GOOD || ret == SANE_STATUS_EOF)
    && get_IN_page_length (buffer) == 0x19){
      DBG (5, "init_vpd: 3097 repair\n");
      memcpy(buffer+0x19,buff_VPD_M3097.cmd,buff_VPD_M3097.size);
  }*/

  DBG (15, "init_vpd: length=%0x\n",get_IN_page_length (buffer));

  /* This scanner supports vital product data.
   * Use this data to set dpi-lists etc. */
  if (ret == SANE_STATUS_GOOD || ret == SANE_STATUS_EOF) {

      DBG (15, "standard options\n");

      s->basic_x_res = get_IN_basic_x_res (buffer);
      DBG (15, "  basic x res: %d dpi\n",s->basic_x_res);

      s->basic_y_res = get_IN_basic_y_res (buffer);
      DBG (15, "  basic y res: %d dpi\n",s->basic_y_res);

      s->step_x_res = get_IN_step_x_res (buffer);
      DBG (15, "  step x res: %d dpi\n", s->step_x_res);

      s->step_y_res = get_IN_step_y_res (buffer);
      DBG (15, "  step y res: %d dpi\n", s->step_y_res);

      s->max_x_res = get_IN_max_x_res (buffer);
      DBG (15, "  max x res: %d dpi\n", s->max_x_res);

      s->max_y_res = get_IN_max_y_res (buffer);
      DBG (15, "  max y res: %d dpi\n", s->max_y_res);

      s->min_x_res = get_IN_min_x_res (buffer);
      DBG (15, "  min x res: %d dpi\n", s->min_x_res);

      s->min_y_res = get_IN_min_y_res (buffer);
      DBG (15, "  min y res: %d dpi\n", s->min_y_res);

      /* some scanners list B&W resolutions. */
      s->std_res_60 = get_IN_std_res_60 (buffer);
      DBG (15, "  60 dpi: %d\n", s->std_res_60);

      s->std_res_75 = get_IN_std_res_75 (buffer);
      DBG (15, "  75 dpi: %d\n", s->std_res_75);

      s->std_res_100 = get_IN_std_res_100 (buffer);
      DBG (15, "  100 dpi: %d\n", s->std_res_100);

      s->std_res_120 = get_IN_std_res_120 (buffer);
      DBG (15, "  120 dpi: %d\n", s->std_res_120);

      s->std_res_150 = get_IN_std_res_150 (buffer);
      DBG (15, "  150 dpi: %d\n", s->std_res_150);

      s->std_res_160 = get_IN_std_res_160 (buffer);
      DBG (15, "  160 dpi: %d\n", s->std_res_160);

      s->std_res_180 = get_IN_std_res_180 (buffer);
      DBG (15, "  180 dpi: %d\n", s->std_res_180);

      s->std_res_200 = get_IN_std_res_200 (buffer);
      DBG (15, "  200 dpi: %d\n", s->std_res_200);

      s->std_res_240 = get_IN_std_res_240 (buffer);
      DBG (15, "  240 dpi: %d\n", s->std_res_240);

      s->std_res_300 = get_IN_std_res_300 (buffer);
      DBG (15, "  300 dpi: %d\n", s->std_res_300);

      s->std_res_320 = get_IN_std_res_320 (buffer);
      DBG (15, "  320 dpi: %d\n", s->std_res_320);

      s->std_res_400 = get_IN_std_res_400 (buffer);
      DBG (15, "  400 dpi: %d\n", s->std_res_400);

      s->std_res_480 = get_IN_std_res_480 (buffer);
      DBG (15, "  480 dpi: %d\n", s->std_res_480);

      s->std_res_600 = get_IN_std_res_600 (buffer);
      DBG (15, "  600 dpi: %d\n", s->std_res_600);

      s->std_res_800 = get_IN_std_res_800 (buffer);
      DBG (15, "  800 dpi: %d\n", s->std_res_800);

      s->std_res_1200 = get_IN_std_res_1200 (buffer);
      DBG (15, "  1200 dpi: %d\n", s->std_res_1200);

      /* maximum window width and length are reported in basic units.*/
      s->max_x_basic = get_IN_window_width(buffer);
      s->max_x = s->max_x_basic * 1200 / s->basic_x_res;
      DBG(15, "  max width: %2.2f inches\n",(float)s->max_x_basic/s->basic_x_res);

      s->max_y_basic = get_IN_window_length(buffer);
      s->max_y = s->max_y_basic * 1200 / s->basic_y_res;
      DBG(15, "  max length: %2.2f inches\n",(float)s->max_y_basic/s->basic_y_res);

      /* known modes */
      s->can_overflow = get_IN_overflow(buffer);
      DBG (15, "  overflow: %d\n", s->can_overflow);

      s->can_monochrome = get_IN_monochrome (buffer);
      DBG (15, "  monochrome: %d\n", s->can_monochrome);

      s->can_halftone = get_IN_half_tone (buffer);
      DBG (15, "  halftone: %d\n", s->can_halftone);

      s->can_grayscale = get_IN_multilevel (buffer);
      DBG (15, "  grayscale: %d\n", s->can_grayscale);

      DBG (15, "  color_monochrome: %d\n", get_IN_monochrome_rgb(buffer));
      DBG (15, "  color_halftone: %d\n", get_IN_half_tone_rgb(buffer));

      s->can_color_grayscale = get_IN_multilevel_rgb (buffer);
      DBG (15, "  color_grayscale: %d\n", s->can_color_grayscale);

      /* now we look at vendor specific data */
      if (get_IN_page_length (buffer) >= 0x5f) {

          DBG (15, "vendor options\n");

          s->has_operator_panel = get_IN_operator_panel(buffer);
          DBG (15, "  operator panel: %d\n", s->has_operator_panel);

          s->has_barcode = get_IN_barcode(buffer);
          DBG (15, "  barcode: %d\n", s->has_barcode);

          s->has_imprinter = get_IN_imprinter(buffer);
          DBG (15, "  imprinter: %d\n", s->has_imprinter);

          s->has_duplex = get_IN_duplex(buffer);
          s->has_back = s->has_duplex;
          DBG (15, "  duplex: %d\n", s->has_duplex);

          s->has_transparency = get_IN_transparency(buffer);
          DBG (15, "  transparency: %d\n", s->has_transparency);

          s->has_flatbed = get_IN_flatbed(buffer);
          DBG (15, "  flatbed: %d\n", s->has_flatbed);

          s->has_adf = get_IN_adf(buffer);
          DBG (15, "  adf: %d\n", s->has_adf);

          s->adbits = get_IN_adbits(buffer);
          DBG (15, "  A/D bits: %d\n",s->adbits);

          s->buffer_bytes = get_IN_buffer_bytes(buffer);
          DBG (15, "  buffer bytes: %d\n",s->buffer_bytes);

          /* std scsi command support */
          s->has_cmd_msen = get_IN_has_cmd_msen(buffer);
          DBG (15, "  mode_sense cmd: %d\n", s->has_cmd_msen);

          /* vendor added scsi command support */
          /* FIXME: there are more of these... */
          s->has_cmd_subwindow = get_IN_has_subwindow(buffer);
          DBG (15, "  subwindow cmd: %d\n", s->has_cmd_subwindow);

          s->has_cmd_endorser = get_IN_has_endorser(buffer);
          DBG (15, "  endorser cmd: %d\n", s->has_cmd_endorser);

          s->has_cmd_hw_status = get_IN_has_hw_status (buffer);
          DBG (15, "  hardware status cmd: %d\n", s->has_cmd_hw_status);

          s->has_cmd_scanner_ctl = get_IN_has_scanner_ctl(buffer);
          DBG (15, "  scanner control cmd: %d\n", s->has_cmd_scanner_ctl);

          /* get threshold, brightness and contrast ranges. */
          s->brightness_steps = get_IN_brightness_steps(buffer);
          DBG (15, "  brightness steps: %d\n", s->brightness_steps);

          s->threshold_steps = get_IN_threshold_steps(buffer);
          DBG (15, "  threshold steps: %d\n", s->threshold_steps);

          s->contrast_steps = get_IN_contrast_steps(buffer);
          DBG (15, "  contrast steps: %d\n", s->contrast_steps);

          /* dither/gamma patterns */
	  s->num_internal_gamma = get_IN_num_gamma_internal (buffer);
          DBG (15, "  built in gamma patterns: %d\n", s->num_internal_gamma);

	  s->num_download_gamma = get_IN_num_gamma_download (buffer);
          DBG (15, "  download gamma patterns: %d\n", s->num_download_gamma);

	  s->num_internal_dither = get_IN_num_dither_internal (buffer);
          DBG (15, "  built in dither patterns: %d\n", s->num_internal_dither);

	  s->num_download_dither = get_IN_num_dither_download (buffer);
          DBG (15, "  download dither patterns: %d\n", s->num_download_dither);

          /* ipc functions */
	  s->has_rif = get_IN_ipc_bw_rif (buffer);
          DBG (15, "  black and white rif: %d\n", s->has_rif);

          s->has_auto1 = get_IN_ipc_auto1(buffer);
          DBG (15, "  automatic binary DTC: %d\n", s->has_auto1);

          s->has_auto2 = get_IN_ipc_auto2(buffer);
          DBG (15, "  simplified DTC: %d\n", s->has_auto2);

          s->has_outline = get_IN_ipc_outline_extraction (buffer);
          DBG (15, "  outline extraction: %d\n", s->has_outline);

          s->has_emphasis = get_IN_ipc_image_emphasis (buffer);
          DBG (15, "  image emphasis: %d\n", s->has_emphasis);

          s->has_autosep = get_IN_ipc_auto_separation (buffer);
          DBG (15, "  automatic separation: %d\n", s->has_autosep);

          s->has_mirroring = get_IN_ipc_mirroring (buffer);
          DBG (15, "  mirror image: %d\n", s->has_mirroring);

          s->has_white_level_follow = get_IN_ipc_white_level_follow (buffer);
          DBG (15, "  white level follower: %d\n", s->has_white_level_follow);

          s->has_subwindow = get_IN_ipc_subwindow (buffer);
          DBG (15, "  subwindow: %d\n", s->has_subwindow);

          /* compression modes */
          s->has_comp_MH = get_IN_compression_MH (buffer);
          DBG (15, "  compression MH: %d\n", s->has_comp_MH);

          s->has_comp_MR = get_IN_compression_MR (buffer);
          DBG (15, "  compression MR: %d\n", s->has_comp_MR);

          s->has_comp_MMR = get_IN_compression_MMR (buffer);
          DBG (15, "  compression MMR: %d\n", s->has_comp_MMR);

          s->has_comp_JBIG = get_IN_compression_JBIG (buffer);
          DBG (15, "  compression JBIG: %d\n", s->has_comp_JBIG);

          s->has_comp_JPG1 = get_IN_compression_JPG_BASE (buffer);
          DBG (15, "  compression JPG1: %d\n", s->has_comp_JPG1);

          s->has_comp_JPG2 = get_IN_compression_JPG_EXT (buffer);
          DBG (15, "  compression JPG2: %d\n", s->has_comp_JPG2);

          s->has_comp_JPG3 = get_IN_compression_JPG_INDEP (buffer);
          DBG (15, "  compression JPG3: %d\n", s->has_comp_JPG3);

          /* FIXME: we dont store these? */
          DBG (15, "  imprinter mech: %d\n", get_IN_imprinter_mechanical(buffer));
          DBG (15, "  imprinter stamp: %d\n", get_IN_imprinter_stamp(buffer));
          DBG (15, "  imprinter elec: %d\n", get_IN_imprinter_electrical(buffer));
          DBG (15, "  imprinter max id: %d\n", get_IN_imprinter_max_id(buffer));
          DBG (15, "  imprinter size: %d\n", get_IN_imprinter_size(buffer));

          /*not all scanners go this far*/
          if (get_IN_page_length (buffer) > 0x5f) {
              DBG (15, "  connection type: %d\n", get_IN_connection(buffer));
    
              s->os_x_basic = get_IN_x_overscan_size(buffer);
              DBG (15, "  horizontal overscan: %d\n", s->os_x_basic);
    
              s->os_y_basic = get_IN_y_overscan_size(buffer);
              DBG (15, "  vertical overscan: %d\n", s->os_y_basic);
          }

          ret = SANE_STATUS_GOOD;
      }
      /*FIXME no vendor vpd, set some defaults? */
      else{
        DBG (5, "init_vpd: Your scanner supports only partial VPD?\n");
        DBG (5, "init_vpd: Please contact kitno455 at gmail dot com\n");
        DBG (5, "init_vpd: with details of your scanner model.\n");
        ret = SANE_STATUS_INVAL;
      }
  }
  /*FIXME no vpd, set some defaults? */
  else{
    DBG (5, "init_vpd: Your scanner does not support VPD?\n");
    DBG (5, "init_vpd: Please contact kitno455 at gmail dot com\n");
    DBG (5, "init_vpd: with details of your scanner model.\n");
  }

  DBG (10, "init_vpd: finish\n");

  return ret;
}

static SANE_Status
init_ms(struct fujitsu *s) 
{
  int ret;
  int oldDbg=DBG_LEVEL;
  unsigned char buffer[12];
  size_t inLen = sizeof(buffer);

  DBG (10, "init_ms: start\n");

  if(!s->has_cmd_msen){
    DBG (10, "init_ms: unsupported\n");
    return SANE_STATUS_GOOD;
  }

  if(DBG_LEVEL < 35){
    DBG_LEVEL = 0;
  }

  set_MSEN_xfer_length (mode_senseB.cmd, inLen);

  DBG (35, "init_ms: color interlace?\n");
  set_MSEN_pc(mode_senseB.cmd, MS_pc_color);
  memset(buffer,0,inLen);
  ret = do_cmd (
    s, 1, 0,
    mode_senseB.cmd, mode_senseB.size,
    NULL, 0,
    buffer, &inLen
  );
  if(ret == SANE_STATUS_GOOD){
    s->has_MS_color=1;
  }

  DBG (35, "init_ms: prepick\n");
  set_MSEN_pc(mode_senseB.cmd, MS_pc_prepick);
  memset(buffer,0,inLen);
  ret = do_cmd (
    s, 1, 0,
    mode_senseB.cmd, mode_senseB.size,
    NULL, 0,
    buffer, &inLen
  );
  if(ret == SANE_STATUS_GOOD){
    s->has_MS_prepick=1;
  }

  DBG (35, "init_ms: sleep\n");
  set_MSEN_pc(mode_senseB.cmd, MS_pc_sleep);
  inLen = sizeof(buffer);
  memset(buffer,0,inLen);
  ret = do_cmd (
    s, 1, 0,
    mode_senseB.cmd, mode_senseB.size,
    NULL, 0,
    buffer, &inLen
  );
  if(ret == SANE_STATUS_GOOD){
    s->has_MS_sleep=1;
  }

  DBG (35, "init_ms: duplex\n");
  set_MSEN_pc(mode_senseB.cmd, MS_pc_duplex);
  inLen = sizeof(buffer);
  memset(buffer,0,inLen);
  ret = do_cmd (
    s, 1, 0,
    mode_senseB.cmd, mode_senseB.size,
    NULL, 0,
    buffer, &inLen
  );
  if(ret == SANE_STATUS_GOOD){
    s->has_MS_duplex=1;
  }

  DBG (35, "init_ms: rand\n");
  set_MSEN_pc(mode_senseB.cmd, MS_pc_rand);
  inLen = sizeof(buffer);
  memset(buffer,0,inLen);
  ret = do_cmd (
    s, 1, 0,
    mode_senseB.cmd, mode_senseB.size,
    NULL, 0,
    buffer, &inLen
  );
  if(ret == SANE_STATUS_GOOD){
    s->has_MS_rand=1;
  }

  DBG (35, "init_ms: bg\n");
  set_MSEN_pc(mode_senseB.cmd, MS_pc_bg);
  inLen = sizeof(buffer);
  memset(buffer,0,inLen);
  ret = do_cmd (
    s, 1, 0,
    mode_senseB.cmd, mode_senseB.size,
    NULL, 0,
    buffer, &inLen
  );
  if(ret == SANE_STATUS_GOOD){
    s->has_MS_bg=1;
  }

  DBG (35, "init_ms: df\n");
  set_MSEN_pc(mode_senseB.cmd, MS_pc_df);
  inLen = sizeof(buffer);
  memset(buffer,0,inLen);
  ret = do_cmd (
    s, 1, 0,
    mode_senseB.cmd, mode_senseB.size,
    NULL, 0,
    buffer, &inLen
  );
  if(ret == SANE_STATUS_GOOD){
    s->has_MS_df=1;
  }

  DBG (35, "init_ms: dropout\n");
  set_MSEN_pc(mode_senseB.cmd, MS_pc_dropout);
  inLen = sizeof(buffer);
  memset(buffer,0,inLen);
  ret = do_cmd (
    s, 1, 0,
    mode_senseB.cmd, mode_senseB.size,
    NULL, 0,
    buffer, &inLen
  );
  if(ret == SANE_STATUS_GOOD){
    s->has_MS_dropout=1;
  }

  DBG (35, "init_ms: buffer\n");
  set_MSEN_pc(mode_senseB.cmd, MS_pc_buff);
  inLen = sizeof(buffer);
  memset(buffer,0,inLen);
  ret = do_cmd (
    s, 1, 0,
    mode_senseB.cmd, mode_senseB.size,
    NULL, 0,
    buffer, &inLen
  );
  if(ret == SANE_STATUS_GOOD){
    s->has_MS_buff=1;
  }

  DBG (35, "init_ms: auto\n");
  set_MSEN_pc(mode_senseB.cmd, MS_pc_auto);
  inLen = sizeof(buffer);
  memset(buffer,0,inLen);
  ret = do_cmd (
    s, 1, 0,
    mode_senseB.cmd, mode_senseB.size,
    NULL, 0,
    buffer, &inLen
  );
  if(ret == SANE_STATUS_GOOD){
    s->has_MS_auto=1;
  }

  DBG (35, "init_ms: lamp\n");
  set_MSEN_pc(mode_senseB.cmd, MS_pc_lamp);
  inLen = sizeof(buffer);
  memset(buffer,0,inLen);
  ret = do_cmd (
    s, 1, 0,
    mode_senseB.cmd, mode_senseB.size,
    NULL, 0,
    buffer, &inLen
  );
  if(ret == SANE_STATUS_GOOD){
    s->has_MS_lamp=1;
  }

  DBG (35, "init_ms: jobsep\n");
  set_MSEN_pc(mode_senseB.cmd, MS_pc_jobsep);
  inLen = sizeof(buffer);
  memset(buffer,0,inLen);
  ret = do_cmd (
    s, 1, 0,
    mode_senseB.cmd, mode_senseB.size,
    NULL, 0,
    buffer, &inLen
  );
  if(ret == SANE_STATUS_GOOD){
    s->has_MS_jobsep=1;
  }

  DBG_LEVEL = oldDbg;

  DBG (15, "  prepick: %d\n", s->has_MS_prepick);
  DBG (15, "  sleep: %d\n", s->has_MS_sleep);
  DBG (15, "  duplex: %d\n", s->has_MS_duplex);
  DBG (15, "  rand: %d\n", s->has_MS_rand);
  DBG (15, "  bg: %d\n", s->has_MS_bg);
  DBG (15, "  df: %d\n", s->has_MS_df);
  DBG (15, "  dropout: %d\n", s->has_MS_dropout);
  DBG (15, "  buff: %d\n", s->has_MS_buff);
  DBG (15, "  auto: %d\n", s->has_MS_auto);
  DBG (15, "  lamp: %d\n", s->has_MS_lamp);
  DBG (15, "  jobsep: %d\n", s->has_MS_jobsep);

  DBG (10, "init_ms: finish\n");

  return SANE_STATUS_GOOD;
}

/*
 * get model specific info that is not in vpd, and correct
 * errors in vpd data. struct is already initialized to 0.
 */
static SANE_Status
init_model (struct fujitsu *s)
{

  DBG (10, "init_model: start\n");

  /* for most scanners these are good defaults */
  s->color_interlace = COLOR_INTERLACE_BGR;

  s->reverse_by_mode[MODE_LINEART] = 0;
  s->reverse_by_mode[MODE_HALFTONE] = 0;
  s->reverse_by_mode[MODE_GRAYSCALE] = 1;
  s->reverse_by_mode[MODE_COLOR] = 1;

  /* if scanner has built-in gamma tables, we use the first one (0) */
  /* otherwise, we use the first downloaded one (0x80) */
  /* note that you may NOT need to send the table to use it? */
  if (!s->num_internal_gamma && s->num_download_gamma){
    s->window_gamma = 0x80;
  }

  /* these two scanners lie about their capabilities,
   * and/or differ significantly from most other models */
  if (strstr (s->product_name, "M3091")
   || strstr (s->product_name, "M3092")) {

    /* lies */
    s->has_rif = 1;
    s->has_back = 0;
    s->adbits = 8;

    /* weirdness */
    s->color_interlace = COLOR_INTERLACE_3091;
    s->duplex_interlace = DUPLEX_INTERLACE_3091;
    s->has_SW_dropout = 1;
    s->window_vid = 0xc0;
    s->ghs_in_rs = 1;
    s->window_gamma = 0;

    s->reverse_by_mode[MODE_LINEART] = 1;
    s->reverse_by_mode[MODE_HALFTONE] = 1;
    s->reverse_by_mode[MODE_GRAYSCALE] = 0;
    s->reverse_by_mode[MODE_COLOR] = 0;
  }
  else if (strstr (s->product_name, "M3093")){
    /* lies */
    s->has_back = 0;
    s->adbits = 8;

    /* weirdness */
    s->duplex_interlace = DUPLEX_INTERLACE_NONE;
  }
  else if ( strstr (s->product_name, "M309")
   || strstr (s->product_name, "M409")){

    /* lies */
    s->adbits = 8;

  }
  else if (strstr (s->product_name, "fi-4120C2")
   || strstr (s->product_name, "fi-4220C2") ) {

    /*s->max_x = 10488;*/

    /* missing from vpd */
    s->os_x_basic = 376;
    s->os_y_basic = 236;
  
  }
  else if ( strstr (s->product_name, "fi-4340")
   || strstr (s->product_name, "fi-4750")
   || strstr (s->product_name, "fi-5650")
   || strstr (s->product_name, "fi-5750")) {

    /* weirdness */
    s->color_interlace = COLOR_INTERLACE_RRGGBB;
  
  }
  /* some firmware versions use capital f? */
  else if (strstr (s->product_name, "Fi-5900")
   || strstr (s->product_name, "fi-5900") ) {

    /* weirdness */
    s->even_scan_line = 1;
    s->color_interlace = COLOR_INTERLACE_NONE;

  }

  DBG (10, "init_model: finish\n");

  return SANE_STATUS_GOOD;
}

/*
 * set good default user values.
 * struct is already initialized to 0.
 */
static SANE_Status
init_user (struct fujitsu *s)
{

  DBG (10, "init_user: start\n");

  /* source */
  if(s->has_flatbed)
    s->source = SOURCE_FLATBED;
  else if(s->has_adf)
    s->source = SOURCE_ADF_FRONT;

  /* scan mode */
  if(s->can_monochrome)
    s->mode = MODE_LINEART;
  else if(s->can_halftone)
    s->mode=MODE_HALFTONE;
  else if(s->can_grayscale)
    s->mode=MODE_GRAYSCALE;
  else if(s->can_color_grayscale)
    s->mode=MODE_COLOR;

  /*x res*/
  s->resolution_x = s->basic_x_res;

  /*y res*/
  s->resolution_y = s->basic_y_res;
  if(s->resolution_y > s->resolution_x){
    s->resolution_y = s->resolution_x;
  }

  /* page width US-Letter */
  s->page_width = 8.5 * 1200;
  if(s->page_width > s->max_x){
    s->page_width = s->max_x;
  }

  /* page height US-Letter */
  s->page_height = 11 * 1200;
  if(s->page_height > s->max_y){
    s->page_height = s->max_y;
  }

  /* bottom-right x */
  s->br_x = s->page_width;

  /* bottom-right y */
  s->br_y = s->page_height;

  /* gamma ramp exponent */
  s->gamma = 1;

  DBG (10, "init_user: finish\n");

  return SANE_STATUS_GOOD;
}

/*
 * This function presets the "option" array to blank
 */
static SANE_Status
init_options (struct fujitsu *s)
{
  int i;

  DBG (10, "init_options: start\n");

  memset (s->opt, 0, sizeof (s->opt));
  for (i = 0; i < NUM_OPTIONS; ++i) {
      s->opt[i].name = "filler";
      s->opt[i].size = sizeof (SANE_Word);
      s->opt[i].cap = SANE_CAP_INACTIVE;
  }

  /* go ahead and setup the first opt, because 
   * frontend may call control_option on it 
   * before calling get_option_descriptor 
   */
  s->opt[OPT_NUM_OPTS].name = SANE_NAME_NUM_OPTIONS;
  s->opt[OPT_NUM_OPTS].title = SANE_TITLE_NUM_OPTIONS;
  s->opt[OPT_NUM_OPTS].desc = SANE_DESC_NUM_OPTIONS;
  s->opt[OPT_NUM_OPTS].type = SANE_TYPE_INT;
  s->opt[OPT_NUM_OPTS].cap = SANE_CAP_SOFT_DETECT;

  DBG (10, "init_options: finish\n");

  return SANE_STATUS_GOOD;
}

/*
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
  struct fujitsu *s = NULL;
  SANE_Status ret;
 
  DBG (10, "sane_open: start\n");

  if(fujitsu_devList){
    DBG (15, "sane_open: searching currently attached scanners\n");
  }
  else{
    DBG (15, "sane_open: no scanners currently attached, attaching\n");

    ret = find_scanners();
    if(ret != SANE_STATUS_GOOD){
      return ret;
    }
  }

  if(name[0] == 0){
    DBG (15, "sane_open: no device requested, using default\n");
    s = fujitsu_devList;
  }
  else{
    DBG (15, "sane_open: device %s requested\n", name);
                                                                                
    for (dev = fujitsu_devList; dev; dev = dev->next) {
      if (strcmp (dev->sane.name, name) == 0) {
        s = dev;
        break;
      }
    }
  }

  if (!s) {
    DBG (5, "sane_open: no device found\n");
    return SANE_STATUS_INVAL;
  }

  DBG (15, "sane_open: device %s found\n", s->sane.name);

  *handle = s;

  /* connect the fd so we can talk to scanner */
  ret = connect_fd(s);
  if(ret != SANE_STATUS_GOOD){
    return ret;
  }

  DBG (10, "sane_open: finish\n");

  return SANE_STATUS_GOOD;
}

/*
 * @@ Section 3 - SANE Options functions
 */

/*
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
  struct fujitsu *s = handle;
  int i;
  SANE_Option_Descriptor *opt = &s->opt[option];

  DBG (20, "sane_get_option_descriptor: %d\n", option);

  if ((unsigned) option >= NUM_OPTIONS)
    return NULL;

  /* "Mode" group -------------------------------------------------------- */
  if(option==OPT_MODE_GROUP){
    opt->title = "Scan Mode";
    opt->desc = "";
    opt->type = SANE_TYPE_GROUP;
    opt->constraint_type = SANE_CONSTRAINT_NONE;
  }

  /* source */
  if(option==OPT_SOURCE){
    i=0;
    if(s->has_flatbed){
      s->source_list[i++]=string_Flatbed;
    }
    if(s->has_adf){
      s->source_list[i++]=string_ADFFront;
  
      if(s->has_back){
        s->source_list[i++]=string_ADFBack;
      }
      if(s->has_duplex){
        s->source_list[i++]=string_ADFDuplex;
      }
    }
    s->source_list[i]=NULL;

    opt->name = SANE_NAME_SCAN_SOURCE;
    opt->title = SANE_TITLE_SCAN_SOURCE;
    opt->desc = SANE_DESC_SCAN_SOURCE;
    opt->type = SANE_TYPE_STRING;
    opt->constraint_type = SANE_CONSTRAINT_STRING_LIST;
    opt->constraint.string_list = s->source_list;
    opt->size = maxStringSize (opt->constraint.string_list);
    opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  }

  /* scan mode */
  if(option==OPT_MODE){
    i=0;
    if(s->can_monochrome){
      s->mode_list[i++]=string_Lineart;
    }
    if(s->can_halftone){
      s->mode_list[i++]=string_Halftone;
    }
    if(s->can_grayscale){
      s->mode_list[i++]=string_Grayscale;
    }
    if(s->can_color_grayscale){
      s->mode_list[i++]=string_Color;
    }
    s->mode_list[i]=NULL;
  
    opt->name = SANE_NAME_SCAN_MODE;
    opt->title = SANE_TITLE_SCAN_MODE;
    opt->desc = SANE_DESC_SCAN_MODE;
    opt->type = SANE_TYPE_STRING;
    opt->constraint_type = SANE_CONSTRAINT_STRING_LIST;
    opt->constraint.string_list = s->mode_list;
    opt->size = maxStringSize (opt->constraint.string_list);
    opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  }

  /* x resolution */
  /* some scanners only support fixed res
   * build a list of possible choices */
  if(option==OPT_X_RES){
    i=0;
    if(s->std_res_60 && s->max_x_res >= 60 && s->min_x_res <= 60){
      s->x_res_list[++i] = 60;
    }
    if(s->std_res_75 && s->max_x_res >= 75 && s->min_x_res <= 75){
      s->x_res_list[++i] = 75;
    }
    if(s->std_res_100 && s->max_x_res >= 100 && s->min_x_res <= 100){
      s->x_res_list[++i] = 100;
    }
    if(s->std_res_120 && s->max_x_res >= 120 && s->min_x_res <= 120){
      s->x_res_list[++i] = 120;
    }
    if(s->std_res_150 && s->max_x_res >= 150 && s->min_x_res <= 150){
      s->x_res_list[++i] = 150;
    }
    if(s->std_res_160 && s->max_x_res >= 160 && s->min_x_res <= 160){
      s->x_res_list[++i] = 160;
    }
    if(s->std_res_180 && s->max_x_res >= 180 && s->min_x_res <= 180){
      s->x_res_list[++i] = 180;
    }
    if(s->std_res_200 && s->max_x_res >= 200 && s->min_x_res <= 200){
      s->x_res_list[++i] = 200;
    }
    if(s->std_res_240 && s->max_x_res >= 240 && s->min_x_res <= 240){
      s->x_res_list[++i] = 240;
    }
    if(s->std_res_300 && s->max_x_res >= 300 && s->min_x_res <= 300){
      s->x_res_list[++i] = 300;
    }
    if(s->std_res_320 && s->max_x_res >= 320 && s->min_x_res <= 320){
      s->x_res_list[++i] = 320;
    }
    if(s->std_res_400 && s->max_x_res >= 400 && s->min_x_res <= 400){
      s->x_res_list[++i] = 400;
    }
    if(s->std_res_480 && s->max_x_res >= 480 && s->min_x_res <= 480){
      s->x_res_list[++i] = 480;
    }
    if(s->std_res_600 && s->max_x_res >= 600 && s->min_x_res <= 600){
      s->x_res_list[++i] = 600;
    }
    if(s->std_res_800 && s->max_x_res >= 800 && s->min_x_res <= 800){
      s->x_res_list[++i] = 800;
    }
    if(s->std_res_1200 && s->max_x_res >= 1200 && s->min_x_res <= 1200){
      s->x_res_list[++i] = 1200;
    }
    s->x_res_list[0] = i;
  
    opt->name = SANE_NAME_SCAN_X_RESOLUTION;
    opt->title = SANE_TITLE_SCAN_X_RESOLUTION;
    opt->desc = SANE_DESC_SCAN_X_RESOLUTION;
    opt->type = SANE_TYPE_INT;
    opt->unit = SANE_UNIT_DPI;
    opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  
    if(s->step_x_res){
      s->x_res_range.min = s->min_x_res;
      s->x_res_range.max = s->max_x_res;
      s->x_res_range.quant = s->step_x_res;
      opt->constraint_type = SANE_CONSTRAINT_RANGE;
      opt->constraint.range = &s->x_res_range;
    }
    else{
      opt->constraint_type = SANE_CONSTRAINT_WORD_LIST;
      opt->constraint.word_list = s->x_res_list;
    }
  }

  /* y resolution */
  if(option==OPT_Y_RES){
    i=0;
    if(s->std_res_60 && s->max_y_res >= 60 && s->min_y_res <= 60){
      s->y_res_list[++i] = 60;
    }
    if(s->std_res_75 && s->max_y_res >= 75 && s->min_y_res <= 75){
      s->y_res_list[++i] = 75;
    }
    if(s->std_res_100 && s->max_y_res >= 100 && s->min_y_res <= 100){
      s->y_res_list[++i] = 100;
    }
    if(s->std_res_120 && s->max_y_res >= 120 && s->min_y_res <= 120){
      s->y_res_list[++i] = 120;
    }
    if(s->std_res_150 && s->max_y_res >= 150 && s->min_y_res <= 150){
      s->y_res_list[++i] = 150;
    }
    if(s->std_res_160 && s->max_y_res >= 160 && s->min_y_res <= 160){
      s->y_res_list[++i] = 160;
    }
    if(s->std_res_180 && s->max_y_res >= 180 && s->min_y_res <= 180){
      s->y_res_list[++i] = 180;
    }
    if(s->std_res_200 && s->max_y_res >= 200 && s->min_y_res <= 200){
      s->y_res_list[++i] = 200;
    }
    if(s->std_res_240 && s->max_y_res >= 240 && s->min_y_res <= 240){
      s->y_res_list[++i] = 240;
    }
    if(s->std_res_300 && s->max_y_res >= 300 && s->min_y_res <= 300){
      s->y_res_list[++i] = 300;
    }
    if(s->std_res_320 && s->max_y_res >= 320 && s->min_y_res <= 320){
      s->y_res_list[++i] = 320;
    }
    if(s->std_res_400 && s->max_y_res >= 400 && s->min_y_res <= 400){
      s->y_res_list[++i] = 400;
    }
    if(s->std_res_480 && s->max_y_res >= 480 && s->min_y_res <= 480){
      s->y_res_list[++i] = 480;
    }
    if(s->std_res_600 && s->max_y_res >= 600 && s->min_y_res <= 600){
      s->y_res_list[++i] = 600;
    }
    if(s->std_res_800 && s->max_y_res >= 800 && s->min_y_res <= 800){
      s->y_res_list[++i] = 800;
    }
    if(s->std_res_1200 && s->max_y_res >= 1200 && s->min_y_res <= 1200){
      s->y_res_list[++i] = 1200;
    }
    s->y_res_list[0] = i;
  
    opt->name = SANE_NAME_SCAN_Y_RESOLUTION;
    opt->title = SANE_TITLE_SCAN_Y_RESOLUTION;
    opt->desc = SANE_DESC_SCAN_Y_RESOLUTION;
    opt->type = SANE_TYPE_INT;
    opt->unit = SANE_UNIT_DPI;
    opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  
    if(s->step_y_res){
      s->y_res_range.min = s->min_y_res;
      s->y_res_range.max = s->max_y_res;
      s->y_res_range.quant = s->step_y_res;
      opt->constraint_type = SANE_CONSTRAINT_RANGE;
      opt->constraint.range = &s->y_res_range;
    }
    else{
      opt->constraint_type = SANE_CONSTRAINT_WORD_LIST;
      opt->constraint.word_list = s->y_res_list;
    }
  }

  /* "Geometry" group ---------------------------------------------------- */
  if(option==OPT_GEOMETRY_GROUP){
    opt->title = "Geometry";
    opt->desc = "";
    opt->type = SANE_TYPE_GROUP;
    opt->constraint_type = SANE_CONSTRAINT_NONE;
  }

  /* top-left x */
  if(option==OPT_TL_X){
    /* values stored in 1200 dpi units */
    /* must be converted to MM for sane */
    s->tl_x_range.min = SCANNER_UNIT_TO_FIXED_MM(s->min_x);
    s->tl_x_range.max = SCANNER_UNIT_TO_FIXED_MM(get_page_width(s));
    s->tl_x_range.quant = MM_PER_UNIT_FIX;
  
    opt->name = SANE_NAME_SCAN_TL_X;
    opt->title = SANE_TITLE_SCAN_TL_X;
    opt->desc = SANE_DESC_SCAN_TL_X;
    opt->type = SANE_TYPE_FIXED;
    opt->unit = SANE_UNIT_MM;
    opt->constraint_type = SANE_CONSTRAINT_RANGE;
    opt->constraint.range = &(s->tl_x_range);
    opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  }

  /* top-left y */
  if(option==OPT_TL_Y){
    /* values stored in 1200 dpi units */
    /* must be converted to MM for sane */
    s->tl_y_range.min = SCANNER_UNIT_TO_FIXED_MM(s->min_y);
    s->tl_y_range.max = SCANNER_UNIT_TO_FIXED_MM(get_page_height(s));
    s->tl_y_range.quant = MM_PER_UNIT_FIX;
  
    opt->name = SANE_NAME_SCAN_TL_Y;
    opt->title = SANE_TITLE_SCAN_TL_Y;
    opt->desc = SANE_DESC_SCAN_TL_Y;
    opt->type = SANE_TYPE_FIXED;
    opt->unit = SANE_UNIT_MM;
    opt->constraint_type = SANE_CONSTRAINT_RANGE;
    opt->constraint.range = &(s->tl_y_range);
    opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  }

  /* bottom-right x */
  if(option==OPT_BR_X){
    /* values stored in 1200 dpi units */
    /* must be converted to MM for sane */
    s->br_x_range.min = SCANNER_UNIT_TO_FIXED_MM(s->min_x);
    s->br_x_range.max = SCANNER_UNIT_TO_FIXED_MM(get_page_width(s));
    s->br_x_range.quant = MM_PER_UNIT_FIX;
  
    opt->name = SANE_NAME_SCAN_BR_X;
    opt->title = SANE_TITLE_SCAN_BR_X;
    opt->desc = SANE_DESC_SCAN_BR_X;
    opt->type = SANE_TYPE_FIXED;
    opt->unit = SANE_UNIT_MM;
    opt->constraint_type = SANE_CONSTRAINT_RANGE;
    opt->constraint.range = &(s->br_x_range);
    opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  }

  /* bottom-right y */
  if(option==OPT_BR_Y){
    /* values stored in 1200 dpi units */
    /* must be converted to MM for sane */
    s->br_y_range.min = SCANNER_UNIT_TO_FIXED_MM(s->min_y);
    s->br_y_range.max = SCANNER_UNIT_TO_FIXED_MM(get_page_height(s));
    s->br_y_range.quant = MM_PER_UNIT_FIX;
  
    opt->name = SANE_NAME_SCAN_BR_Y;
    opt->title = SANE_TITLE_SCAN_BR_Y;
    opt->desc = SANE_DESC_SCAN_BR_Y;
    opt->type = SANE_TYPE_FIXED;
    opt->unit = SANE_UNIT_MM;
    opt->constraint_type = SANE_CONSTRAINT_RANGE;
    opt->constraint.range = &(s->br_y_range);
    opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  }

  /* page width */
  if(option==OPT_PAGE_WIDTH){
    /* values stored in 1200 dpi units */
    /* must be converted to MM for sane */
    s->paper_x_range.min = SCANNER_UNIT_TO_FIXED_MM(s->min_x);
    s->paper_x_range.max = SCANNER_UNIT_TO_FIXED_MM(s->max_x);
    s->paper_x_range.quant = MM_PER_UNIT_FIX;

    opt->name = "pagewidth";
    opt->title = "ADF paper width";
    opt->desc = "Must be set properly to align scanning window";
    opt->type = SANE_TYPE_FIXED;
    opt->unit = SANE_UNIT_MM;
    opt->constraint_type = SANE_CONSTRAINT_RANGE;
    opt->constraint.range = &s->paper_x_range;
    if(s->source == SOURCE_FLATBED){
      opt->cap = SANE_CAP_INACTIVE;
    }
    else{
      opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    }
  }

  /* page height */
  if(option==OPT_PAGE_HEIGHT){
    /* values stored in 1200 dpi units */
    /* must be converted to MM for sane */
    s->paper_y_range.min = SCANNER_UNIT_TO_FIXED_MM(s->min_y);
    s->paper_y_range.max = SCANNER_UNIT_TO_FIXED_MM(s->max_y);
    s->paper_y_range.quant = MM_PER_UNIT_FIX;

    opt->name = "pageheight";
    opt->title = "ADF paper length";
    opt->desc = "Must be set properly to eject pages";
    opt->type = SANE_TYPE_FIXED;
    opt->unit = SANE_UNIT_MM;
    opt->constraint_type = SANE_CONSTRAINT_RANGE;
    opt->constraint.range = &s->paper_y_range;
    if(s->source == SOURCE_FLATBED){
      opt->cap = SANE_CAP_INACTIVE;
    }
    else{
      opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    }
  }

  /* "Enhancement" group ------------------------------------------------- */
  if(option==OPT_ENHANCEMENT_GROUP){
    opt->title = "Enhancement";
    opt->desc = "";
    opt->type = SANE_TYPE_GROUP;
    opt->constraint_type = SANE_CONSTRAINT_NONE;
  }

  /* brightness */
  if(option==OPT_BRIGHTNESS){
    opt->name = SANE_NAME_BRIGHTNESS;
    opt->title = SANE_TITLE_BRIGHTNESS;
    opt->desc = SANE_DESC_BRIGHTNESS;
    opt->type = SANE_TYPE_INT;
    opt->unit = SANE_UNIT_NONE;
    opt->constraint_type = SANE_CONSTRAINT_RANGE;
    opt->constraint.range = &s->brightness_range;
    s->brightness_range.quant=1;

    /* some have hardware brightness (always 0 to 255?) */
    /* some use LUT or GT (-127 to +127)*/
    if (s->brightness_steps || s->num_download_gamma){
      s->brightness_range.min=-127;
      s->brightness_range.max=127;
      opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    }
    else{
      opt->cap = SANE_CAP_INACTIVE;
    }
  }

  /* contrast */
  if(option==OPT_CONTRAST){
    opt->name = SANE_NAME_CONTRAST;
    opt->title = SANE_TITLE_CONTRAST;
    opt->desc = SANE_DESC_CONTRAST;
    opt->type = SANE_TYPE_INT;
    opt->unit = SANE_UNIT_NONE;
    opt->constraint_type = SANE_CONSTRAINT_RANGE;
    opt->constraint.range = &s->contrast_range;
    s->contrast_range.quant=1;

    /* some have hardware contrast (always 0 to 255?) */
    /* some use LUT or GT (-127 to +127)*/
    if (s->contrast_steps || s->num_download_gamma){
      s->contrast_range.min=-127;
      s->contrast_range.max=127;
      opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    }
    else {
      opt->cap = SANE_CAP_INACTIVE;
    }
  }

  /* gamma */
  if(option==OPT_GAMMA){
    opt->name = "gamma";
    opt->title = "Gamma function exponent";
    opt->desc = "Changes intensity of midtones";
    opt->type = SANE_TYPE_FIXED;
    opt->unit = SANE_UNIT_NONE;
    opt->constraint_type = SANE_CONSTRAINT_RANGE;
    opt->constraint.range = &s->gamma_range;

    /* value ranges from .3 to 5, should be log scale? */
    s->gamma_range.quant=SANE_FIX(0.01);
    s->gamma_range.min=SANE_FIX(0.3);
    s->gamma_range.max=SANE_FIX(5);

    /* scanner has gamma via LUT or GT */
    /*if (s->num_download_gamma){
      opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    }
    else {
      opt->cap = SANE_CAP_INACTIVE;
    }*/

    opt->cap = SANE_CAP_INACTIVE;
  }

  /*threshold*/
  if(option==OPT_THRESHOLD){
    opt->name = SANE_NAME_THRESHOLD;
    opt->title = SANE_TITLE_THRESHOLD;
    opt->desc = SANE_DESC_THRESHOLD;
    opt->type = SANE_TYPE_INT;
    opt->unit = SANE_UNIT_NONE;
    opt->constraint_type = SANE_CONSTRAINT_RANGE;
    opt->constraint.range = &s->threshold_range;
    s->threshold_range.min=0;
    s->threshold_range.max=s->threshold_steps;
    s->threshold_range.quant=1;
    if (s->threshold_steps && s->mode == MODE_LINEART) {
      opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    } 
    else {
      opt->cap = SANE_CAP_INACTIVE;
    }
  }

  /*rif*/
  if(option==OPT_RIF){
    opt->name = "rif";
    opt->title = "RIF";
    opt->desc = "Reverse image format";
    opt->type = SANE_TYPE_BOOL;
    opt->unit = SANE_UNIT_NONE;
    if (s->has_rif)
      opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    else
      opt->cap = SANE_CAP_INACTIVE;
  }

  /* "Advanced" group ------------------------------------------------------ */
  if(option==OPT_ADVANCED_GROUP){
    opt->title = "Advanced";
    opt->desc = "";
    opt->type = SANE_TYPE_GROUP;
    opt->constraint_type = SANE_CONSTRAINT_NONE;
  }

  /*image compression*/
  if(option==OPT_COMPRESS){
    i=0;
    s->compress_list[i++]=string_None;

    if(s->has_comp_JPG1){
      s->compress_list[i++]=string_JPEG;
    }

    s->compress_list[i]=NULL;

    opt->name = "compression";
    opt->title = "Compression";
    opt->desc = "Enable compressed data. May crash your front-end program";
    opt->type = SANE_TYPE_STRING;
    opt->constraint_type = SANE_CONSTRAINT_STRING_LIST;
    opt->constraint.string_list = s->compress_list;
    opt->size = maxStringSize (opt->constraint.string_list);

    /*if (i > 1)
      opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    else*/
      opt->cap = SANE_CAP_INACTIVE;
  }

  /*image compression arg*/
  if(option==OPT_COMPRESS_ARG){

    opt->name = "compression-arg";
    opt->title = "Compression argument";
    opt->desc = "Level of JPEG compression. 1 is small file, 7 is large file. 0 (default) is same as 4";
    opt->type = SANE_TYPE_INT;
    opt->unit = SANE_UNIT_NONE;
    opt->constraint_type = SANE_CONSTRAINT_RANGE;
    opt->constraint.range = &s->compress_arg_range;
    s->compress_arg_range.quant=1;

    /*if(s->has_comp_JPG1){
      s->compress_arg_range.min=0;
      s->compress_arg_range.max=7;
      opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    }
    else*/
      opt->cap = SANE_CAP_INACTIVE;
  }

  /*double feed detection*/
  if(option==OPT_DF_DETECT){
    s->df_detect_list[0] = string_Default;
    s->df_detect_list[1] = string_None;
    s->df_detect_list[2] = string_Thickness;
    s->df_detect_list[3] = string_Length;
    s->df_detect_list[4] = string_Both;
    s->df_detect_list[5] = NULL;
  
    opt->name = "dfdetect";
    opt->title = "DF detection";
    opt->desc = "Enable double feed sensors";
    opt->type = SANE_TYPE_STRING;
    opt->constraint_type = SANE_CONSTRAINT_STRING_LIST;
    opt->constraint.string_list = s->df_detect_list;
    opt->size = maxStringSize (opt->constraint.string_list);
    if (s->has_MS_df)
      opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
    else
      opt->cap = SANE_CAP_INACTIVE;
  }

  /*double feed length difference*/
  if(option==OPT_DF_DIFF){
    s->df_diff_list[0] = string_Default;
    s->df_diff_list[1] = string_10mm;
    s->df_diff_list[2] = string_15mm;
    s->df_diff_list[3] = string_20mm;
    s->df_diff_list[4] = NULL;
  
    opt->name = "dfdiff";
    opt->title = "DF length difference";
    opt->desc = "Difference in page length to trigger double feed sensor";
    opt->type = SANE_TYPE_STRING;
    opt->constraint_type = SANE_CONSTRAINT_STRING_LIST;
    opt->constraint.string_list = s->df_diff_list;
    opt->size = maxStringSize (opt->constraint.string_list);
    if (s->has_MS_df)
      opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
    else
      opt->cap = SANE_CAP_INACTIVE;
  }

  /*background color*/
  if(option==OPT_BG_COLOR){
    s->bg_color_list[0] = string_Default;
    s->bg_color_list[1] = string_White;
    s->bg_color_list[2] = string_Black;
    s->bg_color_list[3] = NULL;
  
    opt->name = "bgcolor";
    opt->title = "Background color";
    opt->desc = "Set color of background for scans. May conflict with overscan option";
    opt->type = SANE_TYPE_STRING;
    opt->constraint_type = SANE_CONSTRAINT_STRING_LIST;
    opt->constraint.string_list = s->bg_color_list;
    opt->size = maxStringSize (opt->constraint.string_list);
    if (s->has_bg_front || s->has_bg_back)
      opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
    else
      opt->cap = SANE_CAP_INACTIVE;
  }

  /*dropout color*/
  if(option==OPT_DROPOUT_COLOR){
    s->do_color_list[0] = string_Default;
    s->do_color_list[1] = string_Red;
    s->do_color_list[2] = string_Green;
    s->do_color_list[3] = string_Blue;
    s->do_color_list[4] = NULL;
  
    opt->name = "dropoutcolor";
    opt->title = "Dropout color";
    opt->desc = "One-pass scanners use only one color during gray or binary scanning, useful for colored paper or ink";
    opt->type = SANE_TYPE_STRING;
    opt->constraint_type = SANE_CONSTRAINT_STRING_LIST;
    opt->constraint.string_list = s->do_color_list;
    opt->size = maxStringSize (opt->constraint.string_list);
    if ((s->has_MS_dropout || s->has_SW_dropout) && s->mode != MODE_COLOR)
      opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
    else
      opt->cap = SANE_CAP_INACTIVE;
  }

  /*buffer mode*/
  if(option==OPT_BUFF_MODE){
    s->buff_mode_list[0] = string_Default;
    s->buff_mode_list[1] = string_Off;
    s->buff_mode_list[2] = string_On;
    s->buff_mode_list[3] = NULL;
  
    opt->name = "buffermode";
    opt->title = "Buffer mode";
    opt->desc = "Request scanner to read pages quickly from ADF into internal memory";
    opt->type = SANE_TYPE_STRING;
    opt->constraint_type = SANE_CONSTRAINT_STRING_LIST;
    opt->constraint.string_list = s->buff_mode_list;
    opt->size = maxStringSize (opt->constraint.string_list);
    if (s->has_MS_buff)
      opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
    else
      opt->cap = SANE_CAP_INACTIVE;
  }

  /*prepick*/
  if(option==OPT_PREPICK){
    s->prepick_list[0] = string_Default;
    s->prepick_list[1] = string_Off;
    s->prepick_list[2] = string_On;
    s->prepick_list[3] = NULL;
  
    opt->name = "prepick";
    opt->title = "Prepick";
    opt->desc = "Request scanner to grab next page from ADF";
    opt->type = SANE_TYPE_STRING;
    opt->constraint_type = SANE_CONSTRAINT_STRING_LIST;
    opt->constraint.string_list = s->prepick_list;
    opt->size = maxStringSize (opt->constraint.string_list);
    if (s->has_MS_prepick)
      opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
    else
      opt->cap = SANE_CAP_INACTIVE;
  }

  /*overscan*/
  if(option==OPT_OVERSCAN){
    s->overscan_list[0] = string_Default;
    s->overscan_list[1] = string_Off;
    s->overscan_list[2] = string_On;
    s->overscan_list[3] = NULL;
  
    opt->name = "overscan";
    opt->title = "Overscan";
    opt->desc = "Collect a few mm of background on top side of scan, before paper enters ADF, and increase maximum scan area beyond paper size, to allow collection on remaining sides. May conflict with bgcolor option";
    opt->type = SANE_TYPE_STRING;
    opt->constraint_type = SANE_CONSTRAINT_STRING_LIST;
    opt->constraint.string_list = s->overscan_list;
    opt->size = maxStringSize (opt->constraint.string_list);
    if (s->has_MS_auto)
      opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
    else
      opt->cap = SANE_CAP_INACTIVE;
  }

  /*sleep time*/
  if(option==OPT_SLEEP_TIME){
    s->sleep_time_range.min = 0;
    s->sleep_time_range.max = 60;
    s->sleep_time_range.quant = 1;
  
    opt->name = "sleeptimer";
    opt->title = "Sleep timer";
    opt->desc = "Time in minutes until the internal power supply switches to sleep mode"; 
    opt->type = SANE_TYPE_INT;
    opt->unit = SANE_UNIT_NONE;
    opt->constraint_type = SANE_CONSTRAINT_RANGE;
    opt->constraint.range=&s->sleep_time_range;
    if(s->has_MS_sleep)
      opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
    else
      opt->cap = SANE_CAP_INACTIVE;
  }

  /*duplex offset*/
  if(option==OPT_DUPLEX_OFFSET){
    s->duplex_offset_range.min = -16;
    s->duplex_offset_range.max = 16;
    s->duplex_offset_range.quant = 1;
  
    opt->name = "duplexoffset";
    opt->title = "Duplex offset";
    opt->desc = "Adjust front/back offset";
    opt->type = SANE_TYPE_INT;
    opt->unit = SANE_UNIT_NONE;
    opt->constraint_type = SANE_CONSTRAINT_RANGE;
    opt->constraint.range = &s->duplex_offset_range;
    if(s->duplex_interlace == DUPLEX_INTERLACE_3091)
      opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
    else
      opt->cap = SANE_CAP_INACTIVE;
  }

  if(option==OPT_GREEN_OFFSET){
    s->green_offset_range.min = -16;
    s->green_offset_range.max = 16;
    s->green_offset_range.quant = 1;
  
    opt->name = "greenoffset";
    opt->title = "Green offset";
    opt->desc = "Adjust green/red offset";
    opt->type = SANE_TYPE_INT;
    opt->unit = SANE_UNIT_NONE;
    opt->constraint_type = SANE_CONSTRAINT_RANGE;
    opt->constraint.range = &s->green_offset_range;
    if(s->color_interlace == COLOR_INTERLACE_3091)
      opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
    else
      opt->cap = SANE_CAP_INACTIVE;
  }
  
  if(option==OPT_BLUE_OFFSET){
    s->blue_offset_range.min = -16;
    s->blue_offset_range.max = 16;
    s->blue_offset_range.quant = 1;
  
    opt->name = "blueoffset";
    opt->title = "Blue offset";
    opt->desc = "Adjust blue/red offset";
    opt->type = SANE_TYPE_INT;
    opt->unit = SANE_UNIT_NONE;
    opt->constraint_type = SANE_CONSTRAINT_RANGE;
    opt->constraint.range = &s->blue_offset_range;
    if(s->color_interlace == COLOR_INTERLACE_3091)
      opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
    else
      opt->cap = SANE_CAP_INACTIVE;
  }
  
  if(option==OPT_USE_SWAPFILE){
    opt->name = "swapfile";
    opt->title = "Swap file";
    opt->desc = "Save memory by buffering data in a temp file";
    opt->type = SANE_TYPE_BOOL;
    opt->unit = SANE_UNIT_NONE;
    opt->constraint_type = SANE_CONSTRAINT_NONE;
    /*if(s->has_duplex)
      opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
    else*/
      opt->cap = SANE_CAP_INACTIVE;
  }

  /* "Sensor" group ------------------------------------------------------ */
  if(option==OPT_SENSOR_GROUP){
    opt->title = "Sensors and Buttons";
    opt->desc = "";
    opt->type = SANE_TYPE_GROUP;
    opt->constraint_type = SANE_CONSTRAINT_NONE;
  }

  if(option==OPT_TOP){
    opt->name = "button-topedge";
    opt->title = "Top edge";
    opt->desc = "Paper is pulled partly into adf";
    opt->type = SANE_TYPE_BOOL;
    opt->unit = SANE_UNIT_NONE;
    if (s->has_cmd_hw_status || s->ghs_in_rs)
      opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_HARD_SELECT | SANE_CAP_ADVANCED;
    else 
      opt->cap = SANE_CAP_INACTIVE;
  }

  if(option==OPT_A3){
    opt->name = "button-a3";
    opt->title = "A3 paper";
    opt->desc = "A3 paper detected";
    opt->type = SANE_TYPE_BOOL;
    opt->unit = SANE_UNIT_NONE;
    if (s->has_cmd_hw_status)
      opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_HARD_SELECT | SANE_CAP_ADVANCED;
    else 
      opt->cap = SANE_CAP_INACTIVE;
  }

  if(option==OPT_B4){
    opt->name = "button-b4";
    opt->title = "B4 paper";
    opt->desc = "B4 paper detected";
    opt->type = SANE_TYPE_BOOL;
    opt->unit = SANE_UNIT_NONE;
    if (s->has_cmd_hw_status)
      opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_HARD_SELECT | SANE_CAP_ADVANCED;
    else 
      opt->cap = SANE_CAP_INACTIVE;
  }

  if(option==OPT_A4){
    opt->name = "button-a4";
    opt->title = "A4 paper";
    opt->desc = "A4 paper detected";
    opt->type = SANE_TYPE_BOOL;
    opt->unit = SANE_UNIT_NONE;
    if (s->has_cmd_hw_status)
      opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_HARD_SELECT | SANE_CAP_ADVANCED;
    else 
      opt->cap = SANE_CAP_INACTIVE;
  }

  if(option==OPT_B5){
    opt->name = "button-b5";
    opt->title = "B5 paper";
    opt->desc = "B5 paper detected";
    opt->type = SANE_TYPE_BOOL;
    opt->unit = SANE_UNIT_NONE;
    if (s->has_cmd_hw_status)
      opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_HARD_SELECT | SANE_CAP_ADVANCED;
    else 
      opt->cap = SANE_CAP_INACTIVE;
  }

  if(option==OPT_HOPPER){
    opt->name = "button-adfloaded";
    opt->title = "ADF loaded";
    opt->desc = "Paper in adf hopper";
    opt->type = SANE_TYPE_BOOL;
    opt->unit = SANE_UNIT_NONE;
    if (s->has_cmd_hw_status || s->ghs_in_rs)
      opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_HARD_SELECT | SANE_CAP_ADVANCED;
    else 
      opt->cap = SANE_CAP_INACTIVE;
  }

  if(option==OPT_OMR){
    opt->name = "button-omrdf";
    opt->title = "OMR or DF";
    opt->desc = "OMR or double feed detected";
    opt->type = SANE_TYPE_BOOL;
    opt->unit = SANE_UNIT_NONE;
    if (s->has_cmd_hw_status)
      opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_HARD_SELECT | SANE_CAP_ADVANCED;
    else 
      opt->cap = SANE_CAP_INACTIVE;
  }

  if(option==OPT_ADF_OPEN){
    opt->name = "button-adfopen";
    opt->title = "ADF open";
    opt->desc = "ADF cover open";
    opt->type = SANE_TYPE_BOOL;
    opt->unit = SANE_UNIT_NONE;
    if (s->has_cmd_hw_status || s->ghs_in_rs)
      opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_HARD_SELECT | SANE_CAP_ADVANCED;
    else 
      opt->cap = SANE_CAP_INACTIVE;
  }

  if(option==OPT_SLEEP){
    opt->name = "button-powersave";
    opt->title = "Power saving";
    opt->desc = "Scanner in power saving mode";
    opt->type = SANE_TYPE_BOOL;
    opt->unit = SANE_UNIT_NONE;
    if (s->has_cmd_hw_status)
      opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_HARD_SELECT | SANE_CAP_ADVANCED;
    else 
      opt->cap = SANE_CAP_INACTIVE;
  }

  if(option==OPT_SEND_SW){
    opt->name = "button-send";
    opt->title = "'Send to' button";
    opt->desc = "'Send to' button pressed";
    opt->type = SANE_TYPE_BOOL;
    opt->unit = SANE_UNIT_NONE;
    if (s->has_cmd_hw_status || s->ghs_in_rs)
      opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_HARD_SELECT | SANE_CAP_ADVANCED;
    else 
      opt->cap = SANE_CAP_INACTIVE;
  }

  if(option==OPT_MANUAL_FEED){
    opt->name = "button-manualfeed";
    opt->title = "Manual feed";
    opt->desc = "Manual feed selected";
    opt->type = SANE_TYPE_BOOL;
    opt->unit = SANE_UNIT_NONE;
    if (s->has_cmd_hw_status)
      opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_HARD_SELECT | SANE_CAP_ADVANCED;
    else 
      opt->cap = SANE_CAP_INACTIVE;
  }

  if(option==OPT_SCAN_SW){
    opt->name = "button-scan";
    opt->title = "'Scan' button";
    opt->desc = "'Scan' button pressed";
    opt->type = SANE_TYPE_BOOL;
    opt->unit = SANE_UNIT_NONE;
    if (s->has_cmd_hw_status || s->ghs_in_rs)
      opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_HARD_SELECT | SANE_CAP_ADVANCED;
    else 
      opt->cap = SANE_CAP_INACTIVE;
  }

  if(option==OPT_FUNCTION){
    opt->name = "button-function";
    opt->title = "Function";
    opt->desc = "Function character on screen";
    opt->type = SANE_TYPE_INT;
    opt->unit = SANE_UNIT_NONE;
    if (s->has_cmd_hw_status || s->ghs_in_rs)
      opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_HARD_SELECT | SANE_CAP_ADVANCED;
    else 
      opt->cap = SANE_CAP_INACTIVE;
  }

  if(option==OPT_INK_EMPTY){
    opt->name = "button-inklow";
    opt->title = "Ink low";
    opt->desc = "Imprinter ink running low";
    opt->type = SANE_TYPE_BOOL;
    opt->unit = SANE_UNIT_NONE;
    if (s->has_cmd_hw_status && s->has_imprinter)
      opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_HARD_SELECT | SANE_CAP_ADVANCED;
    else 
      opt->cap = SANE_CAP_INACTIVE;
  }

  if(option==OPT_DOUBLE_FEED){
    opt->name = "button-doublefeed";
    opt->title = "Double feed";
    opt->desc = "Double feed detected";
    opt->type = SANE_TYPE_BOOL;
    opt->unit = SANE_UNIT_NONE;
    if (s->has_cmd_hw_status || s->ghs_in_rs)
      opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_HARD_SELECT | SANE_CAP_ADVANCED;
    else 
      opt->cap = SANE_CAP_INACTIVE;
  }

  if(option==OPT_ERROR_CODE){
    opt->name = "button-errorcode";
    opt->title = "Error code";
    opt->desc = "Hardware error code";
    opt->type = SANE_TYPE_INT;
    opt->unit = SANE_UNIT_NONE;
    if (s->has_cmd_hw_status)
      opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_HARD_SELECT | SANE_CAP_ADVANCED;
    else 
      opt->cap = SANE_CAP_INACTIVE;
  }

  if(option==OPT_SKEW_ANGLE){
    opt->name = "button-skewangle";
    opt->title = "Skew angle";
    opt->desc = "Requires black background for scanning";
    opt->type = SANE_TYPE_INT;
    opt->unit = SANE_UNIT_NONE;
    if (s->has_cmd_hw_status)
      opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_HARD_SELECT | SANE_CAP_ADVANCED;
    else 
      opt->cap = SANE_CAP_INACTIVE;
  }

  if(option==OPT_INK_REMAIN){
    opt->name = "button-inkremain";
    opt->title = "Ink remaining";
    opt->desc = "Imprinter ink level";
    opt->type = SANE_TYPE_INT;
    opt->unit = SANE_UNIT_NONE;
    if (s->has_cmd_hw_status && s->has_imprinter)
      opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_HARD_SELECT | SANE_CAP_ADVANCED;
    else 
      opt->cap = SANE_CAP_INACTIVE;
  }

  if(option==OPT_DENSITY_SW){
    opt->name = "button-density";
    opt->title = "Density";
    opt->desc = "Density dial";
    opt->type = SANE_TYPE_INT;
    opt->unit = SANE_UNIT_NONE;
    if (s->ghs_in_rs)
      opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_HARD_SELECT | SANE_CAP_ADVANCED;
    else 
      opt->cap = SANE_CAP_INACTIVE;
  }

  if(option==OPT_DUPLEX_SW){
    opt->name = "button-duplex";
    opt->title = "Duplex switch";
    opt->desc = "Duplex switch";
    opt->type = SANE_TYPE_BOOL;
    opt->unit = SANE_UNIT_NONE;
    if (s->ghs_in_rs)
      opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_HARD_SELECT | SANE_CAP_ADVANCED;
    else 
      opt->cap = SANE_CAP_INACTIVE;
  }

  return opt;
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
  struct fujitsu *s = (struct fujitsu *) handle;
  SANE_Int dummy = 0;

  /* Make sure that all those statements involving *info cannot break (better
   * than having to do "if (info) ..." everywhere!)
   */
  if (info == 0)
    info = &dummy;

  if (option >= NUM_OPTIONS) {
    DBG (5, "sane_control_option: %d too big\n", option);
    return SANE_STATUS_INVAL;
  }

  if (!SANE_OPTION_IS_ACTIVE (s->opt[option].cap)) {
    DBG (5, "sane_control_option: %d inactive\n", option);
    return SANE_STATUS_INVAL;
  }

  /*
   * SANE_ACTION_GET_VALUE: We have to find out the current setting and
   * return it in a human-readable form (often, text).
   */
  if (action == SANE_ACTION_GET_VALUE) {
      SANE_Word * val_p = (SANE_Word *) val;

      DBG (20, "sane_control_option: get value for '%s' (%d)\n", s->opt[option].name,option);

      switch (option) {

        case OPT_NUM_OPTS:
          *val_p = NUM_OPTIONS;
          return SANE_STATUS_GOOD;

        case OPT_SOURCE:
          if(s->source == SOURCE_FLATBED){
            strcpy (val, string_Flatbed);
          }
          else if(s->source == SOURCE_ADF_FRONT){
            strcpy (val, string_ADFFront);
          }
          else if(s->source == SOURCE_ADF_BACK){
            strcpy (val, string_ADFBack);
          }
          else if(s->source == SOURCE_ADF_DUPLEX){
            strcpy (val, string_ADFDuplex);
          }
          else{
            DBG(5,"missing option val for source\n"); 
          }
          return SANE_STATUS_GOOD;

        case OPT_MODE:
          if(s->mode == MODE_LINEART){
            strcpy (val, string_Lineart);
          }
          else if(s->mode == MODE_HALFTONE){
            strcpy (val, string_Halftone);
          }
          else if(s->mode == MODE_GRAYSCALE){
            strcpy (val, string_Grayscale);
          }
          else if(s->mode == MODE_COLOR){
            strcpy (val, string_Color);
          }
          return SANE_STATUS_GOOD;

        case OPT_X_RES:
          *val_p = s->resolution_x;
          return SANE_STATUS_GOOD;

        case OPT_Y_RES:
          *val_p = s->resolution_y;
          return SANE_STATUS_GOOD;

        case OPT_TL_X:
          *val_p = SCANNER_UNIT_TO_FIXED_MM(s->tl_x);
          return SANE_STATUS_GOOD;

        case OPT_TL_Y:
          *val_p = SCANNER_UNIT_TO_FIXED_MM(s->tl_y);
          return SANE_STATUS_GOOD;

        case OPT_BR_X:
          *val_p = SCANNER_UNIT_TO_FIXED_MM(s->br_x);
          return SANE_STATUS_GOOD;

        case OPT_BR_Y:
          *val_p = SCANNER_UNIT_TO_FIXED_MM(s->br_y);
          return SANE_STATUS_GOOD;

        case OPT_PAGE_WIDTH:
          *val_p = SCANNER_UNIT_TO_FIXED_MM(s->page_width);
          return SANE_STATUS_GOOD;

        case OPT_PAGE_HEIGHT:
          *val_p = SCANNER_UNIT_TO_FIXED_MM(s->page_height);
          return SANE_STATUS_GOOD;

        case OPT_BRIGHTNESS:
          *val_p = s->brightness;
          return SANE_STATUS_GOOD;

        case OPT_CONTRAST:
          *val_p = s->contrast;
          return SANE_STATUS_GOOD;

        case OPT_GAMMA:
          *val_p = SANE_FIX(s->gamma);
          return SANE_STATUS_GOOD;

        case OPT_THRESHOLD:
          *val_p = s->threshold;
          return SANE_STATUS_GOOD;

        case OPT_RIF:
          *val_p = s->rif;
          return SANE_STATUS_GOOD;

        /* Advanced Group */
        case OPT_COMPRESS:
          if(s->compress == COMP_JPEG){
            strcpy (val, string_JPEG);
          }
          else{
            strcpy (val, string_None);
          }
          return SANE_STATUS_GOOD;

        case OPT_COMPRESS_ARG:
          *val_p = s->compress_arg;
          return SANE_STATUS_GOOD;

        case OPT_DF_DETECT:
          switch (s->df_detect) {
            case DF_DEFAULT:
              strcpy (val, string_Default);
              break;
            case DF_NONE:
              strcpy (val, string_None);
              break;
            case DF_THICKNESS:
              strcpy (val, string_Thickness);
              break;
            case DF_LENGTH:
              strcpy (val, string_Length);
              break;
            case DF_BOTH:
              strcpy (val, string_Both);
              break;
          }
          return SANE_STATUS_GOOD;

        case OPT_DF_DIFF:
          switch (s->df_diff) {
            case MSEL_df_diff_DEFAULT:
              strcpy (val, string_Default);
              break;
            case MSEL_df_diff_10MM:
              strcpy (val, string_10mm);
              break;
            case MSEL_df_diff_15MM:
              strcpy (val, string_15mm);
              break;
            case MSEL_df_diff_20MM:
              strcpy (val, string_20mm);
              break;
          }
          return SANE_STATUS_GOOD;

        case OPT_BG_COLOR:
          switch (s->bg_color) {
            case COLOR_DEFAULT:
              strcpy (val, string_Default);
              break;
            case COLOR_WHITE:
              strcpy (val, string_White);
              break;
            case COLOR_BLACK:
              strcpy (val, string_Black);
              break;
          }
          return SANE_STATUS_GOOD;

        case OPT_DROPOUT_COLOR:
          switch (s->dropout_color) {
            case COLOR_DEFAULT:
              strcpy (val, string_Default);
              break;
            case COLOR_RED:
              strcpy (val, string_Red);
              break;
            case COLOR_GREEN:
              strcpy (val, string_Green);
              break;
            case COLOR_BLUE:
              strcpy (val, string_Blue);
              break;
          }
          return SANE_STATUS_GOOD;

        case OPT_BUFF_MODE:
          switch (s->buff_mode) {
            case MSEL_DEFAULT:
              strcpy (val, string_Default);
              break;
            case MSEL_ON:
              strcpy (val, string_On);
              break;
            case MSEL_OFF:
              strcpy (val, string_Off);
              break;
          }
          return SANE_STATUS_GOOD;

        case OPT_PREPICK:
          switch (s->prepick) {
            case MSEL_DEFAULT:
              strcpy (val, string_Default);
              break;
            case MSEL_ON:
              strcpy (val, string_On);
              break;
            case MSEL_OFF:
              strcpy (val, string_Off);
              break;
          }
          return SANE_STATUS_GOOD;

        case OPT_OVERSCAN:
          switch (s->overscan) {
            case MSEL_DEFAULT:
              strcpy (val, string_Default);
              break;
            case MSEL_ON:
              strcpy (val, string_On);
              break;
            case MSEL_OFF:
              strcpy (val, string_Off);
              break;
          }
          return SANE_STATUS_GOOD;

        case OPT_SLEEP_TIME:
          *val_p = s->sleep_time;
          return SANE_STATUS_GOOD;

        case OPT_DUPLEX_OFFSET:
          *val_p = s->duplex_offset;
          return SANE_STATUS_GOOD;

        case OPT_GREEN_OFFSET:
          *val_p = s->green_offset;
          return SANE_STATUS_GOOD;

        case OPT_BLUE_OFFSET:
          *val_p = s->blue_offset;
          return SANE_STATUS_GOOD;

        case OPT_USE_SWAPFILE:
          *val_p = s->use_temp_file;
          return SANE_STATUS_GOOD;

        /* Sensor Group */
        case OPT_TOP:
          get_hardware_status(s);
          *val_p = s->hw_top;
          return SANE_STATUS_GOOD;
          
        case OPT_A3:
          get_hardware_status(s);
          *val_p = s->hw_A3;
          return SANE_STATUS_GOOD;
          
        case OPT_B4:
          get_hardware_status(s);
          *val_p = s->hw_B4;
          return SANE_STATUS_GOOD;
          
        case OPT_A4:
          get_hardware_status(s);
          *val_p = s->hw_A4;
          return SANE_STATUS_GOOD;
          
        case OPT_B5:
          get_hardware_status(s);
          *val_p = s->hw_B5;
          return SANE_STATUS_GOOD;
          
        case OPT_HOPPER:
          get_hardware_status(s);
          *val_p = s->hw_hopper;
          return SANE_STATUS_GOOD;
          
        case OPT_OMR:
          get_hardware_status(s);
          *val_p = s->hw_omr;
          return SANE_STATUS_GOOD;
          
        case OPT_ADF_OPEN:
          get_hardware_status(s);
          *val_p = s->hw_adf_open;
          return SANE_STATUS_GOOD;
          
        case OPT_SLEEP:
          get_hardware_status(s);
          *val_p = s->hw_sleep;
          return SANE_STATUS_GOOD;
          
        case OPT_SEND_SW:
          get_hardware_status(s);
          *val_p = s->hw_send_sw;
          return SANE_STATUS_GOOD;
          
        case OPT_MANUAL_FEED:
          get_hardware_status(s);
          *val_p = s->hw_manual_feed;
          return SANE_STATUS_GOOD;
          
        case OPT_SCAN_SW:
          get_hardware_status(s);
          *val_p = s->hw_scan_sw;
          return SANE_STATUS_GOOD;
          
        case OPT_FUNCTION:
          get_hardware_status(s);
          *val_p = s->hw_function;
          return SANE_STATUS_GOOD;
          
        case OPT_INK_EMPTY:
          get_hardware_status(s);
          *val_p = s->hw_ink_empty;
          return SANE_STATUS_GOOD;
          
        case OPT_DOUBLE_FEED:
          get_hardware_status(s);
          *val_p = s->hw_double_feed;
          return SANE_STATUS_GOOD;
          
        case OPT_ERROR_CODE:
          get_hardware_status(s);
          *val_p = s->hw_error_code;
          return SANE_STATUS_GOOD;
          
        case OPT_SKEW_ANGLE:
          get_hardware_status(s);
          *val_p = s->hw_skew_angle;
          return SANE_STATUS_GOOD;
          
        case OPT_INK_REMAIN:
          get_hardware_status(s);
          *val_p = s->hw_ink_remain;
          return SANE_STATUS_GOOD;
          
        case OPT_DENSITY_SW:
          get_hardware_status(s);
          *val_p = s->hw_density_sw;
          return SANE_STATUS_GOOD;
          
        case OPT_DUPLEX_SW:
          get_hardware_status(s);
          *val_p = s->hw_duplex_sw;
          return SANE_STATUS_GOOD;
          
      }
  }
  else if (action == SANE_ACTION_SET_VALUE) {
      int tmp;
      SANE_Word val_c;
      SANE_Status status;

      DBG (20, "sane_control_option: set value for '%s' (%d)\n", s->opt[option].name,option);

      if ( s->started ) {
        DBG (5, "sane_control_option: cant set, device busy\n");
        return SANE_STATUS_DEVICE_BUSY;
      }

      if (!SANE_OPTION_IS_SETTABLE (s->opt[option].cap)) {
        DBG (5, "sane_control_option: not settable\n");
        return SANE_STATUS_INVAL;
      }

      status = sanei_constrain_value (s->opt + option, val, info);
      if (status != SANE_STATUS_GOOD) {
        DBG (5, "sane_control_option: bad value\n");
        return status;
      }

      /* may have been changed by constrain, so dont copy until now */
      val_c = *(SANE_Word *)val;

      /*
       * Note - for those options which can assume one of a list of
       * valid values, we can safely assume that they will have
       * exactly one of those values because that's what
       * sanei_constrain_value does. Hence no "else: invalid" branches
       * below.
       */
      switch (option) {
 
        /* Mode Group */
        case OPT_SOURCE:
          if (!strcmp (val, string_ADFFront)) {
            tmp = SOURCE_ADF_FRONT;
          }
          else if (!strcmp (val, string_ADFBack)) {
            tmp = SOURCE_ADF_BACK;
          }
          else if (!strcmp (val, string_ADFDuplex)) {
            tmp = SOURCE_ADF_DUPLEX;
          }
          else{
            tmp = SOURCE_FLATBED;
          }

          if (s->source == tmp) 
              return SANE_STATUS_GOOD;

          s->source = tmp;
          *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
          return SANE_STATUS_GOOD;

        case OPT_MODE:
          if (!strcmp (val, string_Lineart)) {
            tmp = MODE_LINEART;
          }
          else if (!strcmp (val, string_Halftone)) {
            tmp = MODE_HALFTONE;
          }
          else if (!strcmp (val, string_Grayscale)) {
            tmp = MODE_GRAYSCALE;
          }
          else{
            tmp = MODE_COLOR;
          }

          if (tmp == s->mode)
              return SANE_STATUS_GOOD;

          s->mode = tmp;
          *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
          return SANE_STATUS_GOOD;

        case OPT_X_RES:

          if (s->resolution_x == val_c) 
              return SANE_STATUS_GOOD;

          /* currently the same? move y too */
          if (s->resolution_x == s->resolution_y){
            s->resolution_y = val_c;
            /*sanei_constrain_value (s->opt + OPT_Y_RES, (void *) &val_c, 0) == SANE_STATUS_GOOD*/
          } 

          s->resolution_x = val_c;

          *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
          return SANE_STATUS_GOOD;

        case OPT_Y_RES:

          if (s->resolution_y == val_c) 
              return SANE_STATUS_GOOD;

          s->resolution_y = val_c;

          *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
          return SANE_STATUS_GOOD;

        /* Geometry Group */
        case OPT_TL_X:
          if (s->tl_x == FIXED_MM_TO_SCANNER_UNIT(val_c))
              return SANE_STATUS_GOOD;

          s->tl_x = FIXED_MM_TO_SCANNER_UNIT(val_c);

          *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
          return SANE_STATUS_GOOD;

        case OPT_TL_Y:
          if (s->tl_y == FIXED_MM_TO_SCANNER_UNIT(val_c))
              return SANE_STATUS_GOOD;

          s->tl_y = FIXED_MM_TO_SCANNER_UNIT(val_c);

          *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
          return SANE_STATUS_GOOD;

        case OPT_BR_X:
          if (s->br_x == FIXED_MM_TO_SCANNER_UNIT(val_c))
              return SANE_STATUS_GOOD;

          s->br_x = FIXED_MM_TO_SCANNER_UNIT(val_c);

          *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
          return SANE_STATUS_GOOD;

        case OPT_BR_Y:
          if (s->br_y == FIXED_MM_TO_SCANNER_UNIT(val_c))
              return SANE_STATUS_GOOD;

          s->br_y = FIXED_MM_TO_SCANNER_UNIT(val_c);

          *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
          return SANE_STATUS_GOOD;

        case OPT_PAGE_WIDTH:
          if (s->page_width == FIXED_MM_TO_SCANNER_UNIT(val_c))
              return SANE_STATUS_GOOD;

          s->page_width = FIXED_MM_TO_SCANNER_UNIT(val_c);
          *info |= SANE_INFO_RELOAD_OPTIONS;
          return SANE_STATUS_GOOD;

        case OPT_PAGE_HEIGHT:
          if (s->page_height == FIXED_MM_TO_SCANNER_UNIT(val_c))
              return SANE_STATUS_GOOD;

          s->page_height = FIXED_MM_TO_SCANNER_UNIT(val_c);
          *info |= SANE_INFO_RELOAD_OPTIONS;
          return SANE_STATUS_GOOD;

        /* Enhancement Group */
        case OPT_BRIGHTNESS:
          s->brightness = val_c;

          /* send lut if scanner has no hardware brightness */
          if(!s->brightness_steps && s->num_download_gamma && s->adbits){
            return send_lut(s);
          }
    
          return SANE_STATUS_GOOD;

        case OPT_CONTRAST:
          s->contrast = val_c;

          /* send lut if scanner has no hardware contrast */
          if(!s->contrast_steps && s->num_download_gamma && s->adbits){
            return send_lut(s);
          }
    
          return SANE_STATUS_GOOD;

        case OPT_GAMMA:
          s->gamma = SANE_UNFIX(val_c);
          return SANE_STATUS_GOOD;

        case OPT_THRESHOLD:
          s->threshold = val_c;
          return SANE_STATUS_GOOD;

        case OPT_RIF:
          s->rif = val_c;
          return SANE_STATUS_GOOD;

        /* Advanced Group */
        case OPT_COMPRESS:
          if (!strcmp (val, string_JPEG)) {
            tmp = COMP_JPEG;
          }
          else{
            tmp = COMP_NONE;
          }

          if (tmp == s->compress)
              return SANE_STATUS_GOOD;

          s->compress = tmp;
          return SANE_STATUS_GOOD;

        case OPT_COMPRESS_ARG:
          s->compress_arg = val_c;
          return SANE_STATUS_GOOD;

        case OPT_DF_DETECT:
          if (!strcmp(val, string_Default))
            s->df_detect = DF_DEFAULT;
          else if (!strcmp(val, string_None))
            s->df_detect = DF_NONE;
          else if (!strcmp(val, string_Thickness))
            s->df_detect = DF_THICKNESS;
          else if (!strcmp(val, string_Length))
            s->df_detect = DF_LENGTH;
          else if (!strcmp(val, string_Both))
            s->df_detect = DF_BOTH;
          return mode_select_df(s);

        case OPT_DF_DIFF:
          if (!strcmp(val, string_Default))
            s->df_diff = MSEL_df_diff_DEFAULT;
          else if (!strcmp(val, string_10mm))
            s->df_diff = MSEL_df_diff_10MM;
          else if (!strcmp(val, string_15mm))
            s->df_diff = MSEL_df_diff_15MM;
          else if (!strcmp(val, string_20mm))
            s->df_diff = MSEL_df_diff_20MM;
          return mode_select_df(s);

        case OPT_BG_COLOR:
          if (!strcmp(val, string_Default))
            s->bg_color = COLOR_DEFAULT;
          else if (!strcmp(val, string_White))
            s->bg_color = COLOR_WHITE;
          else if (!strcmp(val, string_Black))
            s->bg_color = COLOR_BLACK;
          return mode_select_bg(s);

        case OPT_DROPOUT_COLOR:
          if (!strcmp(val, string_Default))
            s->dropout_color = COLOR_DEFAULT;
          else if (!strcmp(val, string_Red))
            s->dropout_color = COLOR_RED;
          else if (!strcmp(val, string_Green))
            s->dropout_color = COLOR_GREEN;
          else if (!strcmp(val, string_Blue))
            s->dropout_color = COLOR_BLUE;
          if (s->has_MS_dropout)
            return mode_select_dropout(s);
          else
            return SANE_STATUS_GOOD;

        case OPT_BUFF_MODE:
          if (!strcmp(val, string_Default))
            s->buff_mode = MSEL_DEFAULT;
          else if (!strcmp(val, string_On))
            s->buff_mode= MSEL_ON;
          else if (!strcmp(val, string_Off))
            s->buff_mode= MSEL_OFF;
          if (s->has_MS_buff)
            return mode_select_buff(s);
          else
            return SANE_STATUS_GOOD;

        case OPT_PREPICK:
          if (!strcmp(val, string_Default))
            s->prepick = MSEL_DEFAULT;
          else if (!strcmp(val, string_On))
            s->prepick = MSEL_ON;
          else if (!strcmp(val, string_Off))
            s->prepick = MSEL_OFF;
          if (s->has_MS_buff)
            return mode_select_prepick(s);
          else
            return SANE_STATUS_GOOD;

        case OPT_OVERSCAN:
          if (!strcmp(val, string_Default))
            s->overscan = MSEL_DEFAULT;
          else if (!strcmp(val, string_On))
            s->overscan = MSEL_ON;
          else if (!strcmp(val, string_Off))
            s->overscan = MSEL_OFF;
          if (s->has_MS_auto){
            *info |= SANE_INFO_RELOAD_OPTIONS;
            return mode_select_overscan(s);
          }
          else
            return SANE_STATUS_GOOD;

        case OPT_SLEEP_TIME:
          s->sleep_time = val_c;
          return set_sleep_mode(s);

        case OPT_DUPLEX_OFFSET:
          s->duplex_offset = val_c;
          return SANE_STATUS_GOOD;

        case OPT_GREEN_OFFSET:
          s->green_offset = val_c;
          return SANE_STATUS_GOOD;

        case OPT_BLUE_OFFSET:
          s->blue_offset = val_c;
          return SANE_STATUS_GOOD;

        case OPT_USE_SWAPFILE:
          s->use_temp_file = val_c;
          return SANE_STATUS_GOOD;

      }                       /* switch */
  }                           /* else */

  return SANE_STATUS_INVAL;
}

static SANE_Status
set_sleep_mode(struct fujitsu *s) 
{
  int ret;

  DBG (10, "set_sleep_mode: start\n");

  set_MSEL_xfer_length (mode_selectB.cmd, mode_select_8byteB.size);
  set_MSEL_pc(mode_select_8byteB.cmd, MS_pc_sleep);
  set_MSEL_sleep_mode(mode_select_8byteB.cmd, s->sleep_time);

  ret = do_cmd (
    s, 1, 0,
    mode_selectB.cmd, mode_selectB.size,
    mode_select_8byteB.cmd, mode_select_8byteB.size,
    NULL, NULL
  );

  DBG (10, "set_sleep_mode: finish\n");

  return ret;
}

static SANE_Status
get_hardware_status (struct fujitsu *s)
{
  SANE_Status ret = SANE_STATUS_GOOD;

  DBG (10, "get_hardware_status: start\n");

  /* only run this once every couple seconds */
  if (s->last_ghs < time(NULL)) {

      DBG (15, "get_hardware_status: running\n");

      if (s->has_cmd_hw_status){
          unsigned char buffer[12];
          size_t inLen = sizeof(buffer);

          DBG (15, "get_hardware_status: calling ghs\n");
  
          set_HW_allocation_length (hw_statusB.cmd, inLen);
        
          ret = do_cmd (
            s, 1, 0,
            hw_statusB.cmd, hw_statusB.size,
            NULL, 0,
            buffer, &inLen
          );
        
          if (ret == SANE_STATUS_GOOD || ret == SANE_STATUS_EOF) {

              s->last_ghs = time(NULL);

              s->hw_top = get_HW_top(buffer);
              s->hw_A3 = get_HW_A3(buffer);
              s->hw_B4 = get_HW_B4(buffer);
              s->hw_A4 = get_HW_A4(buffer);
              s->hw_B5 = get_HW_B5(buffer);
      
              s->hw_hopper = get_HW_hopper(buffer);
              s->hw_omr = get_HW_omr(buffer);
              s->hw_adf_open = get_HW_adf_open(buffer);
      
              s->hw_sleep = get_HW_sleep(buffer);
              s->hw_send_sw = get_HW_send_sw(buffer);
              s->hw_manual_feed = get_HW_manual_feed(buffer);
              s->hw_scan_sw = get_HW_scan_sw(buffer);
      
              s->hw_function = get_HW_function(buffer);
              s->hw_ink_empty = get_HW_ink_empty(buffer);

              s->hw_double_feed = get_HW_double_feed(buffer);
      
              s->hw_error_code = get_HW_error_code(buffer);
      
              s->hw_skew_angle = get_HW_skew_angle(buffer);

              if(inLen > 9){
                s->hw_ink_remain = get_HW_ink_remain(buffer);
              }
          }
      }

      /* 3091/2 put hardware status in RS data */
      else if (s->ghs_in_rs){
          unsigned char buffer[RS_return_size];
          size_t inLen = sizeof(buffer);
          
          DBG(15,"get_hardware_status: calling rs\n");

          ret = do_cmd(
            s,0,0,
            request_senseB.cmd, request_senseB.size,
            NULL,0,
            buffer, &inLen
          );
    
          /* parse the rs data */
          if(ret == SANE_STATUS_GOOD && get_RS_sense_key(buffer)==0 && get_RS_ASC(buffer)==0x80){

              s->last_ghs = time(NULL);
    
              s->hw_adf_open = get_RS_adf_open(buffer);
              s->hw_send_sw = get_RS_send_sw(buffer);
              s->hw_scan_sw = get_RS_scan_sw(buffer);
              s->hw_duplex_sw = get_RS_duplex_sw(buffer);
              s->hw_top = get_RS_top(buffer);
              s->hw_hopper = get_RS_hopper(buffer);
              s->hw_function = get_RS_function(buffer);
              s->hw_density_sw = get_RS_density(buffer);
          }
          else{
            return SANE_STATUS_GOOD;
          }
      }
  }

  DBG (10, "get_hardware_status: finish\n");

  return ret;
}

/* instead of internal brightness/contrast/gamma
   most scanners use a 256x256 or 1024x256 LUT
   default is linear table of slope 1 or 1/4 resp.
   brightness and contrast inputs are -127 to +127 

   contrast rotates slope of line around central input val

       high           low
       .       x      .
       .      x       .         xx
   out .     x        . xxxxxxxx
       .    x         xx
       ....x.......   ............
            in             in

   then brightness moves line vertically, and clamps to 8bit

       bright         dark
       .   xxxxxxxx   .
       . x            . 
   out x              .          x
       .              .        x
       ............   xxxxxxxx....
            in             in
  */
static SANE_Status
send_lut (struct fujitsu *s)
{
  int i, j, ret=0, bytes = 1 << s->adbits;
  unsigned char * p = send_lutC+S_lut_data_offset;
  double b, slope, offset;

  DBG (10, "send_lut: start\n");

  /* contrast is converted to a slope [0,90] degrees:
   * first [-127,127] to [0,254] then to [0,1]
   * then multiply by PI/2 to convert to radians
   * then take the tangent to get slope (T.O.A)
   * then multiply by the normal linear slope 
   * because the table may not be square, i.e. 1024x256*/
  slope = tan(((double)s->contrast+127)/254 * M_PI/2) * 256/bytes;

  /* contrast slope must stay centered, so figure
   * out vertical offset at central input value */
  offset = 127.5-(slope*bytes/2);

  /* convert the user brightness setting (-127 to +127)
   * into a scale that covers the range required
   * to slide the contrast curve entirely off the table */
  b = ((double)s->brightness/127) * (256 - offset);

  DBG (15, "send_lut: %d %f %d %f %f\n", s->brightness, b,
    s->contrast, slope, offset);

  set_S_xfer_datatype (sendB.cmd, S_datatype_lut_data);
  set_S_xfer_length (sendB.cmd, S_lut_data_offset+bytes);

  set_S_lut_order (send_lutC, S_lut_order_single);
  set_S_lut_ssize (send_lutC, bytes);
  set_S_lut_dsize (send_lutC, 256);
 
  for(i=0;i<bytes;i++){
    j=slope*i + offset + b;

    if(j<0){
      j=0;
    }

    if(j>255){
      j=255;
    }

    *p=j;
    p++;
  }

  hexdump(15,"LUT:",send_lutC+S_lut_data_offset,bytes);
 
  DBG (10,"send_lut: skipping\n");
  ret = do_cmd (
      s, 1, 0,
      sendB.cmd, sendB.size,
      send_lutC, S_lut_data_offset+bytes,
      NULL, NULL
  );

  DBG (10, "send_lut: finish\n");

  return ret;
}

static SANE_Status
mode_select_df (struct fujitsu *s)
{
  int ret;

  DBG (10, "mode_select_df: start\n");

  set_MSEL_xfer_length (mode_selectB.cmd, mode_select_8byteB.size);
  set_MSEL_pc(mode_select_8byteB.cmd, MS_pc_df);

  /* clear everything for defaults */
  if(s->df_detect == DF_DEFAULT){
    set_MSEL_df_enable (mode_select_8byteB.cmd, 0);
    set_MSEL_df_continue (mode_select_8byteB.cmd, 0);
    set_MSEL_df_thickness (mode_select_8byteB.cmd, 0);
    set_MSEL_df_length (mode_select_8byteB.cmd, 0);
    set_MSEL_df_diff (mode_select_8byteB.cmd, 0);
  }
  /* none, still have to enable */
  else if(s->df_detect == DF_NONE){
    set_MSEL_df_enable (mode_select_8byteB.cmd, 1);
    set_MSEL_df_continue (mode_select_8byteB.cmd, 1);
    set_MSEL_df_thickness (mode_select_8byteB.cmd, 0);
    set_MSEL_df_length (mode_select_8byteB.cmd, 0);
    set_MSEL_df_diff (mode_select_8byteB.cmd, 0);
  }
  /* thickness only */
  else if(s->df_detect == DF_THICKNESS){
    set_MSEL_df_enable (mode_select_8byteB.cmd, 1);
    set_MSEL_df_continue (mode_select_8byteB.cmd, 0);
    set_MSEL_df_thickness (mode_select_8byteB.cmd, 1);
    set_MSEL_df_length (mode_select_8byteB.cmd, 0);
    set_MSEL_df_diff (mode_select_8byteB.cmd, 0);
  }
  /* length only */
  else if(s->df_detect == DF_LENGTH){
    set_MSEL_df_enable (mode_select_8byteB.cmd, 1);
    set_MSEL_df_continue (mode_select_8byteB.cmd, 0);
    set_MSEL_df_thickness (mode_select_8byteB.cmd, 0);
    set_MSEL_df_length (mode_select_8byteB.cmd, 1);
    set_MSEL_df_diff (mode_select_8byteB.cmd, s->df_diff);
  }
  /* thickness and length */
  else{
    set_MSEL_df_enable (mode_select_8byteB.cmd, 1);
    set_MSEL_df_continue (mode_select_8byteB.cmd, 0);
    set_MSEL_df_thickness (mode_select_8byteB.cmd, 1);
    set_MSEL_df_length (mode_select_8byteB.cmd, 1);
    set_MSEL_df_diff (mode_select_8byteB.cmd, s->df_diff);
  }
  
  ret = do_cmd (
      s, 1, 0,
      mode_selectB.cmd, mode_selectB.size,
      mode_select_8byteB.cmd, mode_select_8byteB.size,
      NULL, NULL
  );

  DBG (10, "mode_select_df: finish\n");

  return ret;
}

static SANE_Status
mode_select_bg (struct fujitsu *s)
{
  int ret;

  DBG (10, "mode_select_bg: start\n");

  set_MSEL_xfer_length (mode_selectB.cmd, mode_select_8byteB.size);
  set_MSEL_pc(mode_select_8byteB.cmd, MS_pc_bg);

  /* clear everything for defaults */
  if(s->bg_color == COLOR_DEFAULT){
    set_MSEL_bg_enable (mode_select_8byteB.cmd, 0);
    set_MSEL_bg_front (mode_select_8byteB.cmd, 0);
    set_MSEL_bg_back (mode_select_8byteB.cmd, 0);
    set_MSEL_bg_fb (mode_select_8byteB.cmd, 0);
  }
  else{
    set_MSEL_bg_enable (mode_select_8byteB.cmd, 1);

    if(s->bg_color == COLOR_BLACK){
      set_MSEL_bg_front (mode_select_8byteB.cmd, 1);
      set_MSEL_bg_back (mode_select_8byteB.cmd, 1);
      set_MSEL_bg_fb (mode_select_8byteB.cmd, 1);
    }
    else{
      set_MSEL_bg_front (mode_select_8byteB.cmd, 0);
      set_MSEL_bg_back (mode_select_8byteB.cmd, 0);
      set_MSEL_bg_fb (mode_select_8byteB.cmd, 0);
    }
  }
  
  ret = do_cmd (
      s, 1, 0,
      mode_selectB.cmd, mode_selectB.size,
      mode_select_8byteB.cmd, mode_select_8byteB.size,
      NULL, NULL
  );

  DBG (10, "mode_select_bg: finish\n");

  return ret;
}

static SANE_Status
mode_select_dropout (struct fujitsu *s)
{
  int ret;

  DBG (10, "mode_select_dropout: start\n");

  set_MSEL_xfer_length (mode_selectB.cmd, mode_select_10byteB.size);
  set_MSEL_pc(mode_select_10byteB.cmd, MS_pc_dropout);
  set_MSEL_dropout_front (mode_select_10byteB.cmd, s->dropout_color);
  set_MSEL_dropout_back (mode_select_10byteB.cmd, s->dropout_color);
  
  ret = do_cmd (
      s, 1, 0,
      mode_selectB.cmd, mode_selectB.size,
      mode_select_10byteB.cmd, mode_select_10byteB.size,
      NULL, NULL
  );

  DBG (10, "mode_select_dropout: finish\n");

  return ret;
}

static SANE_Status
mode_select_buff (struct fujitsu *s)
{
  int ret;

  DBG (10, "mode_select_buff: start\n");

  set_MSEL_xfer_length (mode_selectB.cmd, mode_select_8byteB.size);
  set_MSEL_pc(mode_select_8byteB.cmd, MS_pc_buff);
  set_MSEL_buff_mode(mode_select_8byteB.cmd, s->buff_mode);
  
  ret = do_cmd (
      s, 1, 0,
      mode_selectB.cmd, mode_selectB.size,
      mode_select_8byteB.cmd, mode_select_8byteB.size,
      NULL, NULL
  );

  DBG (10, "mode_select_buff: finish\n");

  return ret;
}

static SANE_Status
mode_select_prepick (struct fujitsu *s)
{
  int ret;

  DBG (10, "mode_select_prepick: start\n");

  set_MSEL_xfer_length (mode_selectB.cmd, mode_select_8byteB.size);
  set_MSEL_pc(mode_select_8byteB.cmd, MS_pc_prepick);
  set_MSEL_prepick(mode_select_8byteB.cmd, s->prepick);
  
  ret = do_cmd (
      s, 1, 0,
      mode_selectB.cmd, mode_selectB.size,
      mode_select_8byteB.cmd, mode_select_8byteB.size,
      NULL, NULL
  );

  DBG (10, "mode_select_prepick: finish\n");

  return ret;
}

static SANE_Status
mode_select_overscan (struct fujitsu *s)
{
  int ret;

  DBG (10, "mode_select_overscan: start\n");

  set_MSEL_xfer_length (mode_selectB.cmd, mode_select_8byteB.size);
  set_MSEL_pc(mode_select_8byteB.cmd, MS_pc_auto);
  set_MSEL_overscan(mode_select_8byteB.cmd, s->overscan);
  
  ret = do_cmd (
      s, 1, 0,
      mode_selectB.cmd, mode_selectB.size,
      mode_select_8byteB.cmd, mode_select_8byteB.size,
      NULL, NULL
  );

  DBG (10, "mode_select_overscan: finish\n");

  return ret;
}


/*
 * @@ Section 4 - SANE scanning functions
 */
/*
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
#if 0

  /* some scanners need even number of bytes per line */
  /* increase scan width to even number of bytes */
  /* but dont round up larger than scanners max width */
  while(s->even_scan_line && s->params.bytes_per_line % 2) {

    if(s->br_x == s->max_x){
      dir = -1;
    }
    s->br_x += dir;
    s->params.pixels_per_line = s->resolution_x * (s->br_x - s->tl_x) / 1200;

    if (s->mode == MODE_COLOR) {
      s->params.bytes_per_line = s->params.pixels_per_line * 3;
    }
    else if (s->mode == MODE_GRAYSCALE) {
      s->params.bytes_per_line = s->params.pixels_per_line;
    }
    else if (s->mode == MODE_LINEART || s->mode == MODE_HALFTONE) {
      s->params.bytes_per_line = s->params.pixels_per_line / 8;
    }
  }
#endif

/* we should be able to do this instead of all the math, but 
 * this does not seem to work in duplex mode? */
#if 0
    int x=0,y=0,px=0,py=0;

    /* call set window to send user settings to scanner */
    ret = set_window(s);
    if(ret){
        DBG (5, "sane_get_parameters: error setting window\n");
        return ret;
    }

    /* read scanner's modified version of x and y */
    ret = get_pixelsize(s,&x,&y,&px,&py);
    if(ret){
        DBG (5, "sane_get_parameters: error reading size\n");
        return ret;
    }

    /* update x data from scanner data */
    s->params.pixels_per_line = x;

    /* update y data from scanner data */
    s->params.lines = y;
#endif

SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters * params)
{
    SANE_Status ret = SANE_STATUS_GOOD;
    struct fujitsu *s = (struct fujitsu *) handle;
  
    DBG (10, "sane_get_parameters: start\n");
  
    DBG (15, "sane_get_parameters: xres=%d, tlx=%d, brx=%d, pw=%d, maxx=%d\n",
      s->resolution_x, s->tl_x, s->br_x, s->page_width, s->max_x);
    DBG (15, "sane_get_parameters: yres=%d, tly=%d, bry=%d, ph=%d, maxy=%d\n",
      s->resolution_y, s->tl_y, s->br_y, s->page_height, s->max_y);
    DBG (15, "sane_get_parameters: user_x=%d, user_y=%d\n", 
      (s->resolution_x * (s->br_x - s->tl_x) / 1200),
      (s->resolution_y * (s->br_y - s->tl_y) / 1200));

    /* not started? update param data */
    if(!s->started){
        int dir = 1;

        DBG (15, "sane_get_parameters: updating\n");

        /* this backend only sends single frame images */
        s->params.last_frame = 1;
      
        /* update params struct from user settings */
        if (s->mode == MODE_COLOR) {
            s->params.format = SANE_FRAME_RGB;
            s->params.depth = 8;
            if(s->compress == COMP_JPEG){
                s->params.format = SANE_FRAME_JPEG;
            }
        }
        else if (s->mode == MODE_GRAYSCALE) {
            s->params.format = SANE_FRAME_GRAY;
            s->params.depth = 8;
            if(s->compress == COMP_JPEG){
                s->params.format = SANE_FRAME_JPEG;
            }
        }
        else {
            s->params.format = SANE_FRAME_GRAY;
            s->params.depth = 1;
        }

        /* adjust x data in a loop */
        while(1){

            s->params.pixels_per_line =
              s->resolution_x * (s->br_x - s->tl_x) / 1200;
    
            /* bytes per line differs by mode */
            if (s->mode == MODE_COLOR) {
                s->params.bytes_per_line = s->params.pixels_per_line * 3;
            }
            else if (s->mode == MODE_GRAYSCALE) {
                s->params.bytes_per_line = s->params.pixels_per_line;
            }
            else {
                s->params.bytes_per_line = s->params.pixels_per_line / 8;
            }

            /* binary and jpeg must have width in multiple of 8 pixels */
            /* plus, some larger scanners require even bytes per line */
            /* so change the user's scan width and try again */
	    /* FIXME: should change a 'hidden' copy instead? */
            if(
              ((s->params.depth == 1 || s->params.format == SANE_FRAME_JPEG)
                && s->params.pixels_per_line % 8)
              || (s->even_scan_line && s->params.bytes_per_line % 2)
            ){

                /* dont round up larger than scanners max width */
                if(s->br_x == s->max_x){
                    dir = -1;
                }
                s->br_x += dir;
            }
            else{
                break;
            }
        }

        dir = 1;

        /* adjust y data in a loop */
        while(1){

            s->params.lines =
              s->resolution_y * (s->br_y - s->tl_y) / 1200;
    
            /* jpeg must have length in multiple of 8 pixels */
            /* so change the user's scan length and try again */
            if( s->params.format == SANE_FRAME_JPEG && s->params.lines % 8 ){

                /* dont round up larger than scanners max length */
                if(s->br_y == s->max_y){
                    dir = -1;
                }
                s->br_y += dir;
            }
            else{
                break;
            }
        }
    }
  
    DBG (15, "sane_get_parameters: scan_x=%d, Bpl=%d, depth=%d\n", 
      s->params.pixels_per_line, s->params.bytes_per_line, s->params.depth );
      
    DBG (15, "sane_get_parameters: scan_y=%d, frame=%d, last=%d\n", 
      s->params.lines, s->params.format, s->params.last_frame );

    if(params){
        DBG (15, "sane_get_parameters: copying to caller\n");
        params->format = s->params.format;
        params->last_frame = s->params.last_frame;
        params->lines = s->params.lines;
        params->depth = s->params.depth;
        params->pixels_per_line = s->params.pixels_per_line;
        params->bytes_per_line = s->params.bytes_per_line;
    }

    DBG (10, "sane_get_parameters: finish\n");
  
    return ret;
}

static SANE_Status
get_pixelsize(struct fujitsu *s, int * x, int * y, int * px, int * py)
{
    SANE_Status ret;
    unsigned char buf[0x18];
    size_t inLen = sizeof(buf);

    DBG (10, "get_pixelsize: start\n");

    set_R_datatype_code (readB.cmd, R_datatype_pixelsize);
    set_R_window_id (readB.cmd, WD_wid_front);
    if(s->source == SOURCE_ADF_DUPLEX || s->source == SOURCE_ADF_BACK){
      set_R_window_id (readB.cmd, WD_wid_back);
    }
    set_R_xfer_length (readB.cmd, inLen);
      
    ret = do_cmd (
      s, 1, 0,
      readB.cmd, readB.size,
      NULL, 0,
      buf, &inLen
    );

    if(ret == SANE_STATUS_GOOD){
      *x = get_PSIZE_num_x(buf);
      *y = get_PSIZE_num_y(buf);
      *px = get_PSIZE_paper_w(buf);
      *py = get_PSIZE_paper_l(buf);
      DBG (15, "get_pixelsize: x=%d, y=%d, px=%d, py=%d\n", *x, *y, *px, *py);
    }

    DBG (10, "get_pixelsize: finish\n");
    return ret;
}

/*
 * Called by SANE when a page acquisition operation is to be started.
 * commands: scanner control (lampon), send (lut), send (dither),
 * set window, object pos, and scan
 *
 * this will be called between sides of a duplex scan,
 * and at the start of each page of an adf batch.
 * hence, we spend alot of time playing with s->started, etc.
 */
SANE_Status
sane_start (SANE_Handle handle)
{
  struct fujitsu *s = handle;
  SANE_Status ret;

  DBG (10, "sane_start: start\n");

  DBG (15, "started=%d, img_count=%d, source=%d\n", s->started,
    s->img_count, s->source);

  /* first page of batch */
  if(!s->started){

      /* set clean defaults */
      s->img_count=0;

      s->bytes_tot[0]=0;
      s->bytes_tot[1]=0;

      s->bytes_rx[0]=0;
      s->bytes_rx[1]=0;
      s->lines_rx[0]=0;
      s->lines_rx[1]=0;

      s->bytes_tx[0]=0;
      s->bytes_tx[1]=0;

      /* reset jpeg just in case... */
      s->jpeg_stage = JPEG_STAGE_HEAD;
      s->jpeg_ff_offset = 0;
      s->jpeg_front_rst = 0;
      s->jpeg_back_rst = 0;

      /* call this, in case frontend has not already */
      ret = sane_get_parameters ((SANE_Handle) s, NULL);
      if (ret != SANE_STATUS_GOOD) {
        DBG (5, "sane_start: ERROR: cannot get params\n");
        do_cancel(s);
        return ret;
      }

      /* set window command */
      ret = set_window(s);
      if (ret != SANE_STATUS_GOOD) {
        DBG (5, "sane_start: ERROR: cannot set window\n");
        do_cancel(s);
        return ret;
      }
    
      /* send batch setup commands */
      ret = scanner_control(s, SC_function_lamp_on);
      if (ret != SANE_STATUS_GOOD) {
        DBG (5, "sane_start: ERROR: cannot start lamp\n");
        do_cancel(s);
        return ret;
      }
    
      /* store the number of front bytes */ 
      if ( s->source != SOURCE_ADF_BACK ){
        s->bytes_tot[SIDE_FRONT] = s->params.bytes_per_line * s->params.lines;
      }

      /* store the number of back bytes */ 
      if ( s->source == SOURCE_ADF_DUPLEX || s->source == SOURCE_ADF_BACK ){
        s->bytes_tot[SIDE_BACK] = s->params.bytes_per_line * s->params.lines;
      }

      /* make temp file/large buffer to hold the image */
      ret = setup_buffers(s);
      if (ret != SANE_STATUS_GOOD) {
          DBG (5, "sane_start: ERROR: cannot load buffers\n");
          do_cancel(s);
          return ret;
      }

      ret = object_position (s, SANE_TRUE);
      if (ret != SANE_STATUS_GOOD) {
        DBG (5, "sane_start: ERROR: cannot load page\n");
        do_cancel(s);
        return ret;
      }

      ret = start_scan (s);
      if (ret != SANE_STATUS_GOOD) {
        DBG (5, "sane_start: ERROR: cannot start_scan\n");
        do_cancel(s);
        return ret;
      }

      s->started=1;
  }

  /* already in a batch */
  else {
    int side = get_current_side(s);

    /* not finished with current side, error */
    if (s->bytes_tx[side] != s->bytes_tot[side]) {
      DBG(5,"sane_start: previous transfer not finished?");
      return do_cancel(s);
    }

    /* Finished with previous img, jump to next */
    s->img_count++;
    side = get_current_side(s);

    /* dont reset the transfer vars on backside of duplex page */
    /* otherwise buffered back page will be lost */
    /* dont call object pos or scan on back side of duplex scan */
    if (s->source == SOURCE_ADF_DUPLEX && side == SIDE_BACK) {
        DBG (15, "sane_start: using buffered duplex backside\n");
    }
    else{
      s->bytes_tot[0]=0;
      s->bytes_tot[1]=0;

      s->bytes_rx[0]=0;
      s->bytes_rx[1]=0;
      s->lines_rx[0]=0;
      s->lines_rx[1]=0;

      s->bytes_tx[0]=0;
      s->bytes_tx[1]=0;

      /* reset jpeg just in case... */
      s->jpeg_stage = JPEG_STAGE_HEAD;
      s->jpeg_ff_offset = 0;
      s->jpeg_front_rst = 0;
      s->jpeg_back_rst = 0;

      ret = object_position (s, SANE_TRUE);
      if (ret != SANE_STATUS_GOOD) {
        DBG (5, "sane_start: ERROR: cannot load page\n");
        do_cancel(s);
        return ret;
      }

      ret = start_scan (s);
      if (ret != SANE_STATUS_GOOD) {
        DBG (5, "sane_start: ERROR: cannot start_scan\n");
        do_cancel(s);
        return ret;
      }

      /* store the number of front bytes */ 
      if ( s->source != SOURCE_ADF_BACK ){
        s->bytes_tot[SIDE_FRONT] = s->params.bytes_per_line * s->params.lines;
      }

      /* store the number of back bytes */ 
      if ( s->source == SOURCE_ADF_DUPLEX || s->source == SOURCE_ADF_BACK ){
        s->bytes_tot[SIDE_BACK] = s->params.bytes_per_line * s->params.lines;
      }

    }
  }

  DBG (15, "started=%d, img_count=%d, source=%d\n", s->started, s->img_count, s->source);

  DBG (10, "sane_start: finish\n");

  return SANE_STATUS_GOOD;
}

/* figure out what side we are looking at currently */
int
get_current_side (struct fujitsu * s){

    int side = SIDE_FRONT;

    if ( s->source == SOURCE_ADF_BACK || (s->source == SOURCE_ADF_DUPLEX && s->img_count % 2) ){
      side = SIDE_BACK;
    }

    return side;
}

static SANE_Status
scanner_control (struct fujitsu *s, int function) 
{
  int ret = SANE_STATUS_GOOD;
  int tries = 0;

  DBG (10, "scanner_control: start\n");

  if(s->has_cmd_scanner_ctl){

    DBG (15, "scanner_control: power up lamp...\n");

    set_SC_function (scanner_controlB.cmd, function);
 
    /* extremely long retry period */
    while(tries++ < 120){

      ret = do_cmd (
        s, 1, 0,
        scanner_controlB.cmd, scanner_controlB.size,
        NULL, 0,
        NULL, NULL
      );

      if(ret == SANE_STATUS_GOOD){
        break;
      }

      usleep(500000);

    } 

    if(ret == SANE_STATUS_GOOD){
      DBG (15, "scanner_control: lamp on, tries %d, ret %d\n",tries,ret);
    }
    else{
      DBG (5, "scanner_control: lamp error, tries %d, ret %d\n",tries,ret);
    }

  }

  DBG (10, "scanner_control: finish\n");

  return ret;
}

/*
 * Creates a temporary file, opens it, and stores file pointer for it.
 * OR, callocs a buffer to hold the scan data
 * 
 * Will only create a file that
 * doesn't exist already. The function will also unlink ("delete") the file
 * immediately after it is created. In any "sane" programming environment this
 * has the effect that the file can be used for reading and writing as normal
 * but vanishes as soon as it's closed - so no cleanup required if the 
 * process dies etc.
 */
static SANE_Status
setup_buffers (struct fujitsu *s)
{
  SANE_Status ret = SANE_STATUS_GOOD;
  int side;

  DBG (10, "setup_buffers: start\n");

  /* cleanup existing first */
  for(side=0;side<2;side++){

      /* close old file */
      if (s->fds[side] != -1) {
        DBG (15, "setup_buffers: closing old tempfile %d.\n",side);
        if(close(s->fds[side])){
          DBG (5, "setup_buffers: attempt to close tempfile %d returned %d.\n", side, errno);
        }
        s->fds[side] = -1;
      }
    
      /* free old mem */
      if (s->buffers[side]) {
        DBG (15, "setup_buffers: free buffer %d.\n",side);
        free(s->buffers[side]);
        s->buffers[side] = NULL;
      }

      if(s->bytes_tot[side]){
        s->buffers[side] = calloc (1,s->bytes_tot[side]);
        if (!s->buffers[side]) {
          DBG (5, "setup_buffers: Error, no buffer %d.\n",side);
          return SANE_STATUS_NO_MEM;
        }
      }
  }

/*
  if (s->use_temp_file) {

#ifdef P_tmpdir
    static const char *tmpdir = P_tmpdir;
#else
    static const char *tmpdir = "/tmp";
#endif
    char filename[PATH_MAX];
    unsigned int suffix = time (NULL) % 256;
    int try = 0;

    while (try++ < 10) {

      sprintf (filename, "%s%csane-fujitsu-%d-%d", tmpdir, PATH_SEP, getpid (), suffix + try);

#if defined(_POSIX_SOURCE)
      s->duplex_fd = open (filename, O_RDWR | O_CREAT | O_EXCL, S_IWUSR | S_IRUSR);
#else
      s->duplex_fd = open (filename, O_RDWR | O_CREAT | O_EXCL);
#endif

      if (s->duplex_fd == -1) {
        DBG (5, "setup_duplex_buffer: attempt to create '%s' returned %d.\n", filename, errno);
      }
      else{
        DBG (15, "setup_duplex_buffer: file '%s' created.\n", filename);
        unlink (filename);
        break;
      }
    }

    if (s->duplex_fd == -1) {
      DBG (5, "setup_duplex_buffer: attempt to create tempfile failed.\n");
      ret = SANE_STATUS_IO_ERROR;
    }
  }
*/

  DBG (10, "setup_buffers: finish\n");

  return ret;
}

/*
 * This routine issues a SCSI SET WINDOW command to the scanner, using the
 * values currently in the scanner data structure.
 */
static int
set_window (struct fujitsu *s)
{
  unsigned char buffer[max_WDB_size];
  int ret, bufferLen;
  int length = s->br_y - s->tl_y;

  DBG (10, "set_window: start\n");

  /* The command specifies the number of bytes in the data phase 
   * the data phase has a header, followed by 1 or 2 window desc blocks 
   * the header specifies the number of bytes in 1 window desc block
   */

  /* set window desc size in header */
  set_WPDB_wdblen (window_descriptor_headerB.cmd, window_descriptor_blockB.size);

  /* copy header into local buffer */
  memcpy (buffer, window_descriptor_headerB.cmd, window_descriptor_headerB.size);
  bufferLen = window_descriptor_headerB.size;

  /* init the window block */
  set_WD_Xres (window_descriptor_blockB.cmd, s->resolution_x);
  set_WD_Yres (window_descriptor_blockB.cmd, s->resolution_y);

  set_WD_ULX (window_descriptor_blockB.cmd, s->tl_x);
  set_WD_ULY (window_descriptor_blockB.cmd, s->tl_y);
  set_WD_width (window_descriptor_blockB.cmd, s->br_x - s->tl_x);

  /* stupid trick. 3091/2 require reading extra lines,
   * because they have a gap between R G and B */
  if(s->mode == MODE_COLOR && s->color_interlace == COLOR_INTERLACE_3091){
    length += (s->color_raster_offset+s->green_offset) * 1200/300 * 2;
    DBG(5,"set_window: Increasing length to %d\n",length);
  }
  set_WD_length (window_descriptor_blockB.cmd, length);

  set_WD_brightness (window_descriptor_blockB.cmd, 0);
  if(s->brightness_steps){
    /*convert our common -127 to +127 range into HW's range
     *FIXME: this code assumes hardware range of 0-255 */

    set_WD_brightness (window_descriptor_blockB.cmd, s->brightness+128);
  }

  set_WD_threshold (window_descriptor_blockB.cmd, s->threshold);

  set_WD_contrast (window_descriptor_blockB.cmd, 0);
  if(s->contrast_steps){
    /*convert our common -127 to +127 range into HW's range
     *FIXME: this code assumes hardware range of 0-255 */

    set_WD_contrast (window_descriptor_blockB.cmd, s->contrast+128);
  }

  set_WD_composition (window_descriptor_blockB.cmd, s->mode);

  /* FIXME: is this something else on new scanners? */
  set_WD_lamp_color (window_descriptor_blockB.cmd, 0);

  /* older scanners use set window to indicate dropout color */
  if (s->has_SW_dropout && s->mode != MODE_COLOR){
      switch (s->dropout_color) {
        case COLOR_RED:
          set_WD_lamp_color (window_descriptor_blockB.cmd, WD_LAMP_RED);
          break;
        case COLOR_GREEN:
          set_WD_lamp_color (window_descriptor_blockB.cmd, WD_LAMP_GREEN);
          break;
        case COLOR_BLUE:
          set_WD_lamp_color (window_descriptor_blockB.cmd, WD_LAMP_BLUE);
          break;
        default:
          set_WD_lamp_color (window_descriptor_blockB.cmd, WD_LAMP_DEFAULT);
          break;
      }
  }

  set_WD_bitsperpixel (window_descriptor_blockB.cmd, s->params.depth);

  set_WD_rif (window_descriptor_blockB.cmd, s->rif);

  set_WD_compress_type(window_descriptor_blockB.cmd, COMP_NONE);
  set_WD_compress_arg(window_descriptor_blockB.cmd, 0);

  /* some scanners support jpeg image compression, for color/gs only */
  if(s->params.format == SANE_FRAME_JPEG){
      set_WD_compress_type(window_descriptor_blockB.cmd, COMP_JPEG);
      set_WD_compress_arg(window_descriptor_blockB.cmd, s->compress_arg);
  }

  set_WD_vendor_id_code (window_descriptor_blockB.cmd, s->window_vid);

  set_WD_gamma (window_descriptor_blockB.cmd, s->window_gamma);

  if(s->source == SOURCE_FLATBED){
    set_WD_paper_selection (window_descriptor_blockB.cmd, WD_paper_SEL_UNDEFINED);
  }
  else{
    set_WD_paper_selection (window_descriptor_blockB.cmd, WD_paper_SEL_NON_STANDARD);

    /* call helper function, scanner wants lies about paper width */
    set_WD_paper_width_X (window_descriptor_blockB.cmd, get_page_width(s));

    /* dont call helper function, scanner wants actual length?  */
    set_WD_paper_length_Y (window_descriptor_blockB.cmd, s->page_height);
  }

  if (s->source == SOURCE_ADF_BACK) {
      set_WD_wid (window_descriptor_blockB.cmd, WD_wid_back);
  }
  else{
      set_WD_wid (window_descriptor_blockB.cmd, WD_wid_front);
  }

  /* copy first desc block into local buffer */
  memcpy (buffer + bufferLen, window_descriptor_blockB.cmd, window_descriptor_blockB.size);
  bufferLen += window_descriptor_blockB.size;

  /* when in duplex mode, add a second window */
  if (s->source == SOURCE_ADF_DUPLEX) {

      set_WD_wid (window_descriptor_blockB.cmd, WD_wid_back);

      /* FIXME: do we really need these on back of page? */
      set_WD_paper_selection (window_descriptor_blockB.cmd, WD_paper_SEL_UNDEFINED);
      set_WD_paper_width_X (window_descriptor_blockB.cmd, 0);
      set_WD_paper_length_Y (window_descriptor_blockB.cmd, 0);

      /* copy second desc block into local buffer */
      memcpy (buffer + bufferLen, window_descriptor_blockB.cmd, window_descriptor_blockB.size);
      bufferLen += window_descriptor_blockB.size;
  }

  /* cmd has data phase byte count */
  set_SW_xferlen(set_windowB.cmd,bufferLen);

  ret = do_cmd (
    s, 1, 0,
    set_windowB.cmd, set_windowB.size,
    buffer, bufferLen,
    NULL, NULL
  );

  DBG (10, "set_window: finish\n");

  return ret;
}

/*
 * Issues the SCSI OBJECT POSITION command if an ADF is in use.
 */
static int
object_position (struct fujitsu *s, int i_load)
{
  int ret;

  DBG (10, "object_position: start\n");

  DBG (15, "object_position: %s\n", (i_load==SANE_TRUE)?"load":"discharge");

  if (s->source == SOURCE_FLATBED) {
    return SANE_STATUS_GOOD;
  }

  if (i_load) {
    set_OP_autofeed (object_positionB.cmd, OP_Feed);
  }
  else {
    set_OP_autofeed (object_positionB.cmd, OP_Discharge);
  }

  ret = do_cmd (
    s, 1, 0,
    object_positionB.cmd, object_positionB.size,
    NULL, 0,
    NULL, NULL
  );
  if (ret != SANE_STATUS_GOOD)
    return ret;

  wait_scanner (s);

  DBG (10, "object_position: finish\n");

  return ret;
}

/*
 * Issues SCAN command.
 * 
 * (This doesn't actually read anything, it just tells the scanner
 * to start scanning.)
 */
static int
start_scan (struct fujitsu *s)
{
  int ret;
  unsigned char outBuff[2];
  int outLen=1;

  DBG (10, "start_scan: start\n");

  outBuff[0] = WD_wid_front;

  if(s->source == SOURCE_ADF_BACK) {
      outBuff[0] = WD_wid_back;
  }
  else if (s->source == SOURCE_ADF_DUPLEX) {
      outBuff[1] = WD_wid_back;
      outLen++;
  }

  set_SC_xfer_length (scanB.cmd, outLen);

  ret = do_cmd (
    s, 1, 0,
    scanB.cmd, scanB.size,
    outBuff, outLen,
    NULL, NULL
  );

  DBG (10, "start_scan: finish\n");

  return ret;
}

/*
 * Called by SANE to read data.
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
sane_read (SANE_Handle handle, SANE_Byte * buf, SANE_Int max_len, SANE_Int * len)
{
  struct fujitsu *s = (struct fujitsu *) handle;
  int side;
  SANE_Status ret=0;

  DBG (10, "sane_read: start\n");

  *len=0;

  /* maybe cancelled? */
  if(!s->started){
      DBG (5, "sane_read: not started, call sane_start\n");
      return SANE_STATUS_CANCELLED;
  }

  side = get_current_side(s);

  /* sane_start required between sides */
  if(s->bytes_tx[side] == s->bytes_tot[side]){
      DBG (15, "sane_read: returning eof\n");
      return SANE_STATUS_EOF;
  }

  /* jpeg (only color/gray) duplex: fixed interlacing */
  if(s->source == SOURCE_ADF_DUPLEX && s->params.format == SANE_FRAME_JPEG){

    /* read from front side if either side has remaining */
    if ( s->bytes_tot[SIDE_FRONT] > s->bytes_rx[SIDE_FRONT]
      || s->bytes_tot[SIDE_BACK] > s->bytes_rx[SIDE_BACK] ){

        ret = read_from_JPEGduplex(s);
        if(ret){
          DBG(5,"sane_read: jpeg duplex returning %d\n",ret);
          return ret;
        }
    }

  }

  /* 3091/2 are on crack, get their own duplex reader function */
  else if(s->source == SOURCE_ADF_DUPLEX
    && s->duplex_interlace == DUPLEX_INTERLACE_3091
  ){

    if(s->bytes_tot[SIDE_FRONT] > s->bytes_rx[SIDE_FRONT]
      || s->bytes_tot[SIDE_BACK] > s->bytes_rx[SIDE_BACK] ){

        ret = read_from_3091duplex(s);
        if(ret){
          DBG(5,"sane_read: 3091 returning %d\n",ret);
          return ret;
        }

    }
  }

  /* 3093 cant alternate? */
  else if(s->source == SOURCE_ADF_DUPLEX
    && s->duplex_interlace == DUPLEX_INTERLACE_NONE
  ){

      if(s->bytes_tot[side] > s->bytes_rx[side] ){

        ret = read_from_scanner(s, side);
        if(ret){
          DBG(5,"sane_read: side %d returning %d\n",side,ret);
          return ret;
        }

      }
  }

  else{
    /* buffer front side */
    if( side == SIDE_FRONT){
      if(s->bytes_tot[SIDE_FRONT] > s->bytes_rx[SIDE_FRONT] ){

        ret = read_from_scanner(s, SIDE_FRONT);
        if(ret){
          DBG(5,"sane_read: front returning %d\n",ret);
          return ret;
        }

      }
    }
  
    /* buffer back side */
    if( side == SIDE_BACK || s->source == SOURCE_ADF_DUPLEX ){
      if(s->bytes_tot[SIDE_BACK] > s->bytes_rx[SIDE_BACK] ){

        ret = read_from_scanner(s, SIDE_BACK);
        if(ret){
          DBG(5,"sane_read: back returning %d\n",ret);
          return ret;
        }

      }
    }
  }

  /* copy a block from buffer to frontend */
  ret = read_from_buffer(s,buf,max_len,len,side);

  DBG (10, "sane_read: finish\n");

  return ret;
}

static SANE_Status
read_from_JPEGduplex(struct fujitsu *s)
{
    SANE_Status ret=SANE_STATUS_GOOD;
    int bytes = s->buffer_size;
    int remain = (s->bytes_tot[SIDE_FRONT] - s->bytes_rx[SIDE_FRONT])
      + (s->bytes_tot[SIDE_BACK] - s->bytes_rx[SIDE_BACK]);
    unsigned char * buf;
    size_t inLen = 0;
    int i=0;
  
    DBG (10, "read_from_JPEGduplex: start\n");
  
    /* figure out the max amount to transfer */
    if(bytes > remain){
        bytes = remain;
    }
  
    /* this should never happen */
    if(bytes < 1){
        DBG(5, "read_from_JPEGduplex: ERROR: no bytes this pass\n");
        ret = SANE_STATUS_INVAL;
    }
  
    DBG(15, "read_from_JPEGduplex: fto:%d frx:%d bto:%d brx:%d re:%d pa:%d\n",
      s->bytes_tot[SIDE_FRONT], s->bytes_rx[SIDE_FRONT],
      s->bytes_tot[SIDE_BACK], s->bytes_rx[SIDE_BACK],
      remain, bytes);

    if(ret){
        return ret;
    }
  
    inLen = bytes;
  
    buf = malloc(bytes);
    if(!buf){
        DBG(5, "read_from_JPEGduplex: not enough mem for buffer: %d\n",bytes);
        return SANE_STATUS_NO_MEM;
    }
  
    set_R_datatype_code (readB.cmd, R_datatype_imagedata);

    /* jpeg duplex always reads from front */
    set_R_window_id (readB.cmd, WD_wid_front);
  
    set_R_xfer_length (readB.cmd, bytes);
  
    ret = do_cmd (
      s, 1, 0,
      readB.cmd, readB.size,
      NULL, 0,
      buf, &inLen
    );
  
    if (ret == SANE_STATUS_GOOD || ret == SANE_STATUS_EOF) {
        DBG(15, "read_from_JPEGduplex: got GOOD/EOF, returning GOOD\n");
        ret = SANE_STATUS_GOOD;
    }
    else if (ret == SANE_STATUS_DEVICE_BUSY) {
        DBG(5, "read_from_JPEGduplex: got BUSY, returning GOOD\n");
        inLen = 0;
        ret = SANE_STATUS_GOOD;
    }
    else {
        DBG(5, "read_from_JPEGduplex: error reading data block status = %d\n",
	  ret);
        inLen = 0;
    }
  
    for(i=0;i<(int)inLen;i++){

        /* about to change stage */
        if(buf[i] == 0xff){
            s->jpeg_ff_offset=0;
            continue;
        }

        /* last byte was an ff, this byte will change stage */
        if(s->jpeg_ff_offset == 0){

            /* headers (SOI/HuffTab/QTab/DRI), in both sides */
            if(buf[i] == 0xd8 || buf[i] == 0xc4
              || buf[i] == 0xdb || buf[i] == 0xdd){
                s->jpeg_stage = JPEG_STAGE_HEAD;
                DBG(15, "read_from_JPEGduplex: stage head\n");
            }

            /* start of frame, in both sides, update x first */
            else if(buf[i]==0xc0){
                s->jpeg_stage = JPEG_STAGE_SOF;
                DBG(15, "read_from_JPEGduplex: stage sof\n");
            }

            /* start of scan, first few bytes of marker in both sides
             * but rest in front */
            else if(buf[i]==0xda){
                s->jpeg_stage = JPEG_STAGE_SOS;
                DBG(15, "read_from_JPEGduplex: stage sos\n");
            }

            /* finished front image block, switch to back */
            /* also change from even RST to proper one */
            else if(buf[i] == 0xd0 || buf[i] == 0xd2
              || buf[i] == 0xd4 || buf[i] == 0xd6){
                s->jpeg_stage = JPEG_STAGE_BACK;
                DBG(35, "read_from_JPEGduplex: stage back\n");

                /* skip first RST for back side*/
                if(!s->jpeg_back_rst){
                  DBG(15, "read_from_JPEGduplex: stage back jump\n");
                  s->jpeg_ff_offset++;
                  s->jpeg_back_rst++;
                  continue;
                }

                buf[i] = 0xd0 + (s->jpeg_back_rst-1) % 8;
                s->jpeg_back_rst++;
            }

            /* finished back image block, switch to front */
            else if(buf[i] == 0xd1 || buf[i] == 0xd3
              || buf[i] == 0xd5 || buf[i] == 0xd7){
                s->jpeg_stage = JPEG_STAGE_FRONT;
                DBG(35, "read_from_JPEGduplex: stage front\n");
                buf[i] = 0xd0 + (s->jpeg_front_rst % 8);
                s->jpeg_front_rst++;
            }

            /* finished image, in both, update totals */
            else if(buf[i]==0xd9){
                s->jpeg_stage = JPEG_STAGE_EOI;
                DBG(15, "read_from_JPEGduplex: stage eoi %d %d\n",(int)inLen,i);
            }
        }
        s->jpeg_ff_offset++;

        /* first x byte in start of frame */
        if(s->jpeg_stage == JPEG_STAGE_SOF && s->jpeg_ff_offset == 7){
          s->jpeg_x_bit = buf[i] & 0x01;
          buf[i] = buf[i] >> 1;
        }

        /* second x byte in start of frame */
        if(s->jpeg_stage == JPEG_STAGE_SOF && s->jpeg_ff_offset == 8){
          buf[i] = (s->jpeg_x_bit << 7) | (buf[i] >> 1);
        }

        /* copy these stages to front */
        if(s->jpeg_stage == JPEG_STAGE_HEAD
          || s->jpeg_stage == JPEG_STAGE_SOF
          || s->jpeg_stage == JPEG_STAGE_SOS
          || s->jpeg_stage == JPEG_STAGE_EOI
          || s->jpeg_stage == JPEG_STAGE_FRONT
        ){
            /* first byte after ff, send the ff first */
            if(s->jpeg_ff_offset == 1){
              s->buffers[SIDE_FRONT][ s->bytes_rx[SIDE_FRONT] ] = 0xff;
              s->bytes_rx[SIDE_FRONT]++;
            }
            s->buffers[SIDE_FRONT][ s->bytes_rx[SIDE_FRONT] ] = buf[i];
            s->bytes_rx[SIDE_FRONT]++;
        }

        /* copy these stages to back */
        if(s->jpeg_stage == JPEG_STAGE_HEAD
          || s->jpeg_stage == JPEG_STAGE_SOF
          || s->jpeg_stage == JPEG_STAGE_SOS
          || s->jpeg_stage == JPEG_STAGE_EOI
          || s->jpeg_stage == JPEG_STAGE_BACK
        ){
            /* first byte after ff, send the ff first */
            if(s->jpeg_ff_offset == 1){
              s->buffers[SIDE_BACK][ s->bytes_rx[SIDE_BACK] ] = 0xff;
              s->bytes_rx[SIDE_BACK]++;
            }
            s->buffers[SIDE_BACK][ s->bytes_rx[SIDE_BACK] ] = buf[i];
            s->bytes_rx[SIDE_BACK]++;
        }

        /* reached last byte of SOS section, next byte front */
        if(s->jpeg_stage == JPEG_STAGE_SOS && s->jpeg_ff_offset == 0x0d){
            s->jpeg_stage = JPEG_STAGE_FRONT;
        }

        /* last byte of file, update totals, bail out */
        if(s->jpeg_stage == JPEG_STAGE_EOI){
            s->bytes_tot[SIDE_FRONT] = s->bytes_rx[SIDE_FRONT];
            s->bytes_tot[SIDE_BACK] = s->bytes_rx[SIDE_BACK];
        }
    }
      
    free(buf);
  
    DBG (10, "read_from_JPEGduplex: finish\n");
  
    return ret;
}

static SANE_Status
read_from_3091duplex(struct fujitsu *s)
{
  SANE_Status ret=SANE_STATUS_GOOD;
  int side = SIDE_FRONT;
  int bytes = s->buffer_size;
  int remain = (s->bytes_tot[SIDE_FRONT] - s->bytes_rx[SIDE_FRONT]) + (s->bytes_tot[SIDE_BACK] - s->bytes_rx[SIDE_BACK]);
  int off = (s->duplex_raster_offset+s->duplex_offset) * s->resolution_y/300;
  unsigned int i;
  unsigned char * buf;
  size_t inLen = 0;

  DBG (10, "read_from_3091duplex: start\n");

  /* figure out the max amount to transfer */
  if(bytes > remain){
    bytes = remain;
  }

  /* all requests must end on line boundary */
  bytes -= (bytes % s->params.bytes_per_line);

  /* this should never happen */
  if(bytes < 1){
    DBG(5, "read_from_3091duplex: ERROR: no bytes this pass\n");
    ret = SANE_STATUS_INVAL;
  }

  DBG(15, "read_from_3091duplex: to:%d rx:%d li:%d re:%d bu:%d pa:%d of:%d\n",
      s->bytes_tot[SIDE_FRONT] + s->bytes_tot[SIDE_BACK],
      s->bytes_rx[SIDE_FRONT] + s->bytes_rx[SIDE_BACK],
      s->lines_rx[SIDE_FRONT] + s->lines_rx[SIDE_BACK],
      remain, s->buffer_size, bytes, off);

  if(ret){
    return ret;
  }

  inLen = bytes;

  buf = malloc(bytes);
  if(!buf){
    DBG(5, "read_from_3091duplex: not enough mem for buffer: %d\n",bytes);
    return SANE_STATUS_NO_MEM;
  }

  set_R_datatype_code (readB.cmd, R_datatype_imagedata);
  set_R_window_id (readB.cmd, WD_wid_front);
  set_R_xfer_length (readB.cmd, bytes);

  ret = do_cmd (
    s, 1, 0,
    readB.cmd, readB.size,
    NULL, 0,
    buf, &inLen
  );

  if (ret == SANE_STATUS_GOOD || ret == SANE_STATUS_EOF) {
    DBG(15, "read_from_3091duplex: got GOOD/EOF, returning GOOD\n");
    ret = SANE_STATUS_GOOD;
  }
  else if (ret == SANE_STATUS_DEVICE_BUSY) {
    DBG(5, "read_from_3091duplex: got BUSY, returning GOOD\n");
    inLen = 0;
    ret = SANE_STATUS_GOOD;
  }
  else {
    DBG(5, "read_from_3091duplex: error reading data block status = %d\n", ret);
    inLen = 0;
  }

  /* loop thru all lines in read buffer */
  for(i=0;i<inLen/s->params.bytes_per_line;i++){

      /* start is front */
      if(s->lines_rx[SIDE_FRONT] < off){
        side=SIDE_FRONT;
      }

      /* end is back */
      else if(s->bytes_rx[SIDE_FRONT] == s->bytes_tot[SIDE_FRONT]){
        side=SIDE_BACK;
      }

      /* odd are back */
      else if( ((s->lines_rx[SIDE_FRONT] + s->lines_rx[SIDE_BACK] - off) % 2) ){
        side=SIDE_BACK;
      }

      /* even are front */
      else{
        side=SIDE_FRONT;
      }

      if(s->mode == MODE_COLOR && s->color_interlace == COLOR_INTERLACE_3091){
        copy_3091 (s, buf + i*s->params.bytes_per_line, s->params.bytes_per_line, side);
      }
      else{
        copy_buffer (s, buf + i*s->params.bytes_per_line, s->params.bytes_per_line, side);
      }
  }

  free(buf);

  DBG (10, "read_from_3091duplex: finish\n");

  return ret;
}

static SANE_Status
read_from_scanner(struct fujitsu *s, int side)
{
    SANE_Status ret=SANE_STATUS_GOOD;
    int bytes = s->buffer_size;
    int remain = s->bytes_tot[side] - s->bytes_rx[side];
    unsigned char * buf;
    size_t inLen = 0;
  
    DBG (10, "read_from_scanner: start\n");
  
    /* figure out the max amount to transfer */
    if(bytes > remain){
        bytes = remain;
    }
  
    /* all requests must end on line boundary */
    bytes -= (bytes % s->params.bytes_per_line);
  
    /* this should never happen */
    if(bytes < 1){
        DBG(5, "read_from_scanner: ERROR: no bytes this pass\n");
        ret = SANE_STATUS_INVAL;
    }
  
    DBG(15, "read_from_scanner: si:%d to:%d rx:%d re:%d bu:%d pa:%d\n", side,
      s->bytes_tot[side], s->bytes_rx[side], remain, s->buffer_size, bytes);
  
    if(ret){
        return ret;
    }
  
    inLen = bytes;
  
    buf = malloc(bytes);
    if(!buf){
        DBG(5, "read_from_scanner: not enough mem for buffer: %d\n",bytes);
        return SANE_STATUS_NO_MEM;
    }
  
    set_R_datatype_code (readB.cmd, R_datatype_imagedata);
  
    if (side == SIDE_BACK) {
        set_R_window_id (readB.cmd, WD_wid_back);
    }
    else{
        set_R_window_id (readB.cmd, WD_wid_front);
    }
  
    set_R_xfer_length (readB.cmd, bytes);
  
    ret = do_cmd (
      s, 1, 0,
      readB.cmd, readB.size,
      NULL, 0,
      buf, &inLen
    );
  
    if (ret == SANE_STATUS_GOOD) {
        DBG(15, "read_from_scanner: got GOOD, returning GOOD\n");
    }
    else if (ret == SANE_STATUS_EOF) {
        DBG(15, "read_from_scanner: got EOF, finishing\n");
    }
    else if (ret == SANE_STATUS_DEVICE_BUSY) {
        DBG(5, "read_from_scanner: got BUSY, returning GOOD\n");
        inLen = 0;
        ret = SANE_STATUS_GOOD;
    }
    else {
        DBG(5, "read_from_scanner: error reading data block status = %d\n",ret);
        inLen = 0;
    }
  
    if(inLen){
        if(s->mode==MODE_COLOR && s->color_interlace == COLOR_INTERLACE_3091){
            copy_3091 (s, buf, inLen, side);
        }
        else{
            copy_buffer (s, buf, inLen, side);
        }
    }
  
    free(buf);
  
    if(ret == SANE_STATUS_EOF){
      s->bytes_tot[side] = s->bytes_rx[side];
      ret = SANE_STATUS_GOOD;
    }

    DBG (10, "read_from_scanner: finish\n");
  
    return ret;
}

static SANE_Status
copy_3091(struct fujitsu *s, unsigned char * buf, int len, int side)
{
  SANE_Status ret=SANE_STATUS_GOOD;
  int i, dest, boff, goff;

  DBG (10, "copy_3091: start\n");

  /* Data is RR...GG...BB... on each line,
   * green is back 8 lines from red at 300 dpi
   * blue is back 4 lines from red at 300 dpi. 
   * Here, we just get things on correct line,
   * interlacing to make RGBRGB comes later.
   * We add the user-supplied offsets before we scale
   * so that they are independent of scanning resolution.
   */
  goff = (s->color_raster_offset+s->green_offset) * s->resolution_y/150;
  boff = (s->color_raster_offset+s->blue_offset) * s->resolution_y/300;

  /* loop thru all lines in read buffer */
  for(i=0;i<len/s->params.bytes_per_line;i++){

      /* red at start of line */
      dest = s->lines_rx[side] * s->params.bytes_per_line;
      if(dest >= 0 && dest < s->bytes_tot[side]){
        memcpy(s->buffers[side] + dest,
               buf + i*s->params.bytes_per_line,
               s->params.pixels_per_line);
      }

      /* green is in middle of line */
      dest = (s->lines_rx[side] - goff) * s->params.bytes_per_line + s->params.pixels_per_line;
      if(dest >= 0 && dest < s->bytes_tot[side]){
        memcpy(s->buffers[side] + dest,
               buf + i*s->params.bytes_per_line + s->params.pixels_per_line,
               s->params.pixels_per_line);
      }

      /* blue is at end of line */
      dest = (s->lines_rx[side] - boff) * s->params.bytes_per_line + s->params.pixels_per_line*2;
      if(dest >= 0 && dest < s->bytes_tot[side]){
        memcpy(s->buffers[side] + dest,
               buf + i*s->params.bytes_per_line + s->params.pixels_per_line*2,
               s->params.pixels_per_line);
      }

      s->lines_rx[side]++;
  }

  /* even if we have read data, we may not have any 
   * full lines loaded yet, so we may have to lie */
  i = (s->lines_rx[side]-goff) * s->params.bytes_per_line;
  if(i < 0){
    i = 0;
  } 
  s->bytes_rx[side] = i;

  DBG (10, "copy_3091: finish\n");

  return ret;
}

static SANE_Status
copy_buffer(struct fujitsu *s, unsigned char * buf, int len, int side)
{
  SANE_Status ret=SANE_STATUS_GOOD;

  DBG (10, "copy_buffer: start\n");

  memcpy(s->buffers[side]+s->bytes_rx[side],buf,len);
  s->bytes_rx[side] += len;

  DBG (10, "copy_buffer: finish\n");

  return ret;
}

static SANE_Status
read_from_buffer(struct fujitsu *s, SANE_Byte * buf,
  SANE_Int max_len, SANE_Int * len, int side)
{
    SANE_Status ret=SANE_STATUS_GOOD;
    int bytes = max_len, i=0;
    int remain = s->bytes_rx[side] - s->bytes_tx[side];
  
    DBG (10, "read_from_buffer: start\n");
  
    /* figure out the max amount to transfer */
    if(bytes > remain){
        bytes = remain;
    }
  
    *len = bytes;
  
    DBG(15, "read_from_buffer: si:%d to:%d tx:%d re:%d bu:%d pa:%d\n", side,
      s->bytes_tot[side], s->bytes_tx[side], remain, max_len, bytes);
  
    /*FIXME this needs to timeout eventually */
    if(!bytes){
        DBG(5,"read_from_buffer: nothing to do\n");
        return SANE_STATUS_GOOD;
    }
  
    /* jpeg data does not use typical interlacing or inverting, just copy */
    if(s->compress == COMP_JPEG &&
      (s->mode == MODE_COLOR || s->mode == MODE_GRAYSCALE)){

        memcpy(buf,s->buffers[side]+s->bytes_tx[side],bytes);
    }
  
    /* not using jpeg, colors interlaced, pixels inverted */
    else {

        /* scanners interlace colors in many different ways */
        /* use separate code to convert to regular rgb */
        if(s->mode == MODE_COLOR){
            int byteOff, lineOff;
        
            switch (s->color_interlace) {
        
                /* scanner returns pixel data as bgrbgr... */
                case COLOR_INTERLACE_BGR:
                    for (i=0; i < bytes; i++){
                        byteOff = s->bytes_tx[side] + i;
                        buf[i] = s->buffers[side][ byteOff-((byteOff%3)-1)*2 ];
                    }
                    break;
        
                /* one line has the following format:
                 * rrr...rrrggg...gggbbb...bbb */
                case COLOR_INTERLACE_3091:
                case COLOR_INTERLACE_RRGGBB:
                    for (i=0; i < bytes; i++){
                        byteOff = s->bytes_tx[side] + i;
                        lineOff = byteOff % s->params.bytes_per_line;

                        buf[i] = s->buffers[side][
                          byteOff - lineOff                       /* line  */
                          + (lineOff%3)*s->params.pixels_per_line /* color */
                          + (lineOff/3)                           /* pixel */
                        ];
                    }
                    break;
        
                default:
                    memcpy(buf,s->buffers[side]+s->bytes_tx[side],bytes);
                    break;
            }
        }
        /* gray/ht/binary */
        else{
            memcpy(buf,s->buffers[side]+s->bytes_tx[side],bytes);
        }
      
        /* invert image if scanner needs it for this mode */
        if (s->reverse_by_mode[s->mode]){
            for ( i = 0; i < *len; i++ ) {
                buf[i] ^= 0xff;
            }
        }
    }
  
    s->bytes_tx[side] += *len;
      
    DBG (10, "read_from_buffer: finish\n");
  
    return ret;
}


/*
 * @@ Section 4 - SANE cleanup functions
 */
/*
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
sane_cancel (SANE_Handle handle)
{
  DBG (10, "sane_cancel: start\n");
  do_cancel ((struct fujitsu *) handle);
  DBG (10, "sane_cancel: finish\n");
}

/*
 * Performs cleanup.
 * FIXME: do better cleanup if scanning is ongoing...
 */
static SANE_Status
do_cancel (struct fujitsu *s)
{
  DBG (10, "do_cancel: start\n");

  s->started = 0;

  DBG (10, "do_cancel: finish\n");

  return SANE_STATUS_CANCELLED;
}

/*
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
  DBG (10, "sane_close: start\n");

  do_cancel((struct fujitsu *) handle);
  disconnect_fd((struct fujitsu *) handle);

  DBG (10, "sane_close: finish\n");
}

static SANE_Status
disconnect_fd (struct fujitsu *s)
{
  DBG (10, "disconnect_fd: start\n");

  if(s->fd > -1){
    if (s->connection == CONNECTION_USB) {
      DBG (15, "disconnecting usb device\n");
      sanei_usb_close (s->fd);
    }
    else if (s->connection == CONNECTION_SCSI) {
      DBG (15, "disconnecting scsi device\n");
      sanei_scsi_close (s->fd);
    }
    s->fd = -1;
  }

  DBG (10, "disconnect_fd: finish\n");

  return SANE_STATUS_GOOD;
}

/*
 * Terminates the backend.
 * 
 * From the SANE spec:
 * This function must be called to terminate use of a backend. The
 * function will first close all device handles that still might be
 * open (it is recommended to close device handles explicitly through
 * a call to sane_close(), but backends are required to release all
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

  DBG (10, "sane_exit: start\n");

  for (dev = fujitsu_devList; dev; dev = next) {
      disconnect_fd(dev);
      next = dev->next;
      free (dev->device_name);
      free (dev);
  }

  if (sane_devArray)
    free (sane_devArray);

  fujitsu_devList = NULL;
  sane_devArray = NULL;

  DBG (10, "sane_exit: finish\n");
}


/*
 * @@ Section 5 - misc helper functions
 */
/*
 * Called by the SANE SCSI core and our usb code on device errors
 * parses the request sense return data buffer,
 * decides the best SANE_Status for the problem, produces debug msgs,
 * and copies the sense buffer into the scanner struct
 */
static SANE_Status
sense_handler (int fd, unsigned char * sensed_data, void *arg)
{
  struct fujitsu *s = arg;
  unsigned int sense = get_RS_sense_key (sensed_data);
  unsigned int asc = get_RS_ASC (sensed_data);
  unsigned int ascq = get_RS_ASCQ (sensed_data);
  unsigned int eom = get_RS_EOM (sensed_data);
  unsigned int ili = get_RS_ILI (sensed_data);
  unsigned int info = get_RS_information (sensed_data);

  DBG (5, "sense_handler: start\n");

  /* kill compiler warning */
  fd = fd;

  /* copy the rs return data into the scanner struct
     so that the caller can use it if he wants
  memcpy(&s->rs_buffer,sensed_data,RS_return_size);
  */

  DBG (5, "Sense=%#02x, ASC=%#02x, ASCQ=%#02x, EOM=%d, ILI=%d, info=%#08x\n", sense, asc, ascq, eom, ili, info);

  switch (sense) {
    case 0x0:
      if (0x80 == asc) {
        DBG  (5, "No sense: hardware status bits?\n");
        return SANE_STATUS_GOOD;
      }
      if (0x00 != asc) {
        DBG  (5, "No sense: unknown asc\n");
        return SANE_STATUS_IO_ERROR;
      }
      if (0x00 != ascq) {
        DBG  (5, "No sense: unknown ascq\n");
        return SANE_STATUS_IO_ERROR;
      }
      if (eom == 1 && ili == 1) {
        s->rs_info = get_RS_information (sensed_data);
        DBG  (5, "No sense: EOM remainder:%lu\n",(unsigned long)s->rs_info);
        return SANE_STATUS_EOF;
      }
      DBG  (5, "No sense: ready\n");
      return SANE_STATUS_GOOD;

    case 0x2:
      if (0x00 != asc) {
        DBG  (5, "Not ready: unknown asc\n");
        return SANE_STATUS_IO_ERROR;
      }
      if (0x00 != ascq) {
        DBG  (5, "Not ready: unknown ascq\n");
        return SANE_STATUS_IO_ERROR;
      }
      DBG  (5, "Not ready: busy\n");
      return SANE_STATUS_DEVICE_BUSY;
      break;

    case 0x3:
      if (0x80 != asc) {
        DBG  (5, "Medium error: unknown asc\n");
        return SANE_STATUS_IO_ERROR;
      }
      if (0x01 == ascq) {
        DBG  (5, "Medium error: paper jam\n");
        return SANE_STATUS_JAMMED;
      }
      if (0x02 == ascq) {
        DBG  (5, "Medium error: cover open\n");
        return SANE_STATUS_COVER_OPEN;
      }
      if (0x03 == ascq) {
        DBG  (5, "Medium error: hopper empty\n");
        return SANE_STATUS_NO_DOCS;
      }
      if (0x04 == ascq) {
        DBG  (5, "Medium error: unusual paper\n");
        return SANE_STATUS_JAMMED;
      }
      if (0x07 == ascq) {
        DBG  (5, "Medium error: double feed\n");
        return SANE_STATUS_JAMMED;
      }
      if (0x10 == ascq) {
        DBG  (5, "Medium error: no ink cartridge\n");
        return SANE_STATUS_IO_ERROR;
      }
      if (0x13 == ascq) {
        DBG  (5, "Medium error: temporary no data\n");
        return SANE_STATUS_DEVICE_BUSY;
      }
      if (0x14 == ascq) {
        DBG  (5, "Medium error: imprinter error\n");
        return SANE_STATUS_IO_ERROR;
      }
      DBG  (5, "Medium error: unknown ascq\n");
      return SANE_STATUS_IO_ERROR;
      break;

    case 0x4:
      if (0x80 != asc && 0x44 != asc && 0x47 != asc) {
        DBG  (5, "Hardware error: unknown asc\n");
        return SANE_STATUS_IO_ERROR;
      }
      if ((0x44 == asc) && (0x00 == ascq)) {
        DBG  (5, "Hardware error: EEPROM error\n");
        return SANE_STATUS_IO_ERROR;
      }
      if ((0x80 == asc) && (0x01 == ascq)) {
        DBG  (5, "Hardware error: FB motor fuse\n");
        return SANE_STATUS_IO_ERROR;
      }
      if ((0x80 == asc) && (0x02 == ascq)) {
        DBG  (5, "Hardware error: heater fuse\n");
        return SANE_STATUS_IO_ERROR;
      }
      if ((0x80 == asc) && (0x04 == ascq)) {
        DBG  (5, "Hardware error: ADF motor fuse\n");
        return SANE_STATUS_IO_ERROR;
      }
      if ((0x80 == asc) && (0x05 == ascq)) {
        DBG  (5, "Hardware error: mechanical error\n");
        return SANE_STATUS_IO_ERROR;
      }
      if ((0x80 == asc) && (0x06 == ascq)) {
        DBG  (5, "Hardware error: optical error\n");
        return SANE_STATUS_IO_ERROR;
      }
      if ((0x80 == asc) && (0x07 == ascq)) {
        DBG  (5, "Hardware error: Fan error\n");
        return SANE_STATUS_IO_ERROR;
      }
      if ((0x80 == asc) && (0x08 == ascq)) {
        DBG  (5, "Hardware error: IPC option error\n");
        return SANE_STATUS_IO_ERROR;
      }
      if ((0x80 == asc) && (0x10 == ascq)) {
        DBG  (5, "Hardware error: imprinter error\n");
        return SANE_STATUS_IO_ERROR;
      }
      DBG  (5, "Hardware error: unknown asc/ascq\n");
      return SANE_STATUS_IO_ERROR;
      break;

    case 0x5:
      if ((0x00 == asc) && (0x00 == ascq)) {
        DBG  (5, "Illegal request: paper edge detected too soon\n");
        return SANE_STATUS_INVAL;
      }
      if ((0x1a == asc) && (0x00 == ascq)) {
        DBG  (5, "Illegal request: Parameter list error\n");
        return SANE_STATUS_INVAL;
      }
      if ((0x20 == asc) && (0x00 == ascq)) {
        DBG  (5, "Illegal request: invalid command\n");
        return SANE_STATUS_INVAL;
      }
      if ((0x24 == asc) && (0x00 == ascq)) {
        DBG  (5, "Illegal request: invalid CDB field\n");
        return SANE_STATUS_INVAL;
      }
      if ((0x25 == asc) && (0x00 == ascq)) {
        DBG  (5, "Illegal request: unsupported logical unit\n");
        return SANE_STATUS_UNSUPPORTED;
      }
      if ((0x26 == asc) && (0x00 == ascq)) {
        DBG  (5, "Illegal request: invalid field in parm list\n");
        if (get_RS_additional_length(sensed_data) >= 0x0a) {
          DBG (5, "Offending byte is %#02x\n", get_RS_offending_byte(sensed_data));

          /* move this to set_window() ? */
          if (get_RS_offending_byte(sensed_data) >= 8) {
            DBG (5, "Window desc block? byte %#02x\n",get_RS_offending_byte(sensed_data)-8);
          }
        }
        return SANE_STATUS_INVAL;
      }
      if ((0x2C == asc) && (0x00 == ascq)) {
        DBG  (5, "Illegal request: command sequence error\n");
        return SANE_STATUS_INVAL;
      }
      if ((0x2C == asc) && (0x02 == ascq)) {
        DBG  (5, "Illegal request: wrong window combination \n");
        return SANE_STATUS_INVAL;
      }
      DBG  (5, "Illegal request: unknown asc/ascq\n");
      return SANE_STATUS_IO_ERROR;
      break;

    case 0x6:
      if ((0x00 == asc) && (0x00 == ascq)) {
        DBG  (5, "Unit attention: device reset\n");
        return SANE_STATUS_GOOD;
      }
      if ((0x80 == asc) && (0x01 == ascq)) {
        DBG  (5, "Unit attention: power saving\n");
        return SANE_STATUS_GOOD;
      }
      DBG  (5, "Unit attention: unknown asc/ascq\n");
      return SANE_STATUS_IO_ERROR;
      break;

    case 0xb:
      if ((0x43 == asc) && (0x00 == ascq)) {
        DBG  (5, "Aborted command: message error\n");
        return SANE_STATUS_IO_ERROR;
      }
      if ((0x45 == asc) && (0x00 == ascq)) {
        DBG  (5, "Aborted command: select failure\n");
        return SANE_STATUS_IO_ERROR;
      }
      if ((0x47 == asc) && (0x00 == ascq)) {
        DBG  (5, "Aborted command: SCSI parity error\n");
        return SANE_STATUS_IO_ERROR;
      }
      if ((0x48 == asc) && (0x00 == ascq)) {
        DBG  (5, "Aborted command: initiator error message\n");
        return SANE_STATUS_IO_ERROR;
      }
      if ((0x4e == asc) && (0x00 == ascq)) {
        DBG  (5, "Aborted command: overlapped commands\n");
        return SANE_STATUS_IO_ERROR;
      }
      if ((0x80 == asc) && (0x01 == ascq)) {
        DBG  (5, "Aborted command: image transfer error\n");
        return SANE_STATUS_IO_ERROR;
      }
      if ((0x80 == asc) && (0x03 == ascq)) {
        DBG  (5, "Aborted command: JPEG overflow error\n");
        return SANE_STATUS_NO_MEM;
      }
      DBG  (5, "Aborted command: unknown asc/ascq\n");
      return SANE_STATUS_IO_ERROR;
      break;

    default:
      DBG (5, "Unknown Sense Code\n");
      return SANE_STATUS_IO_ERROR;
  }

  DBG (5, "sense_handler: should never happen!\n");

  return SANE_STATUS_IO_ERROR;
}

/*
 * take a bunch of pointers, send commands to scanner
 */
static SANE_Status
do_cmd(struct fujitsu *s, int runRS, int shortTime,
 unsigned char * cmdBuff, size_t cmdLen,
 unsigned char * outBuff, size_t outLen,
 unsigned char * inBuff, size_t * inLen
)
{
    if (s->connection == CONNECTION_SCSI) {
        return do_scsi_cmd(s, runRS, shortTime,
                 cmdBuff, cmdLen,
                 outBuff, outLen,
                 inBuff, inLen
        );
    }
    if (s->connection == CONNECTION_USB) {
        return do_usb_cmd(s, runRS, shortTime,
                 cmdBuff, cmdLen,
                 outBuff, outLen,
                 inBuff, inLen
        );
    }
    return SANE_STATUS_INVAL;
}

SANE_Status
do_scsi_cmd(struct fujitsu *s, int runRS, int shortTime,
 unsigned char * cmdBuff, size_t cmdLen,
 unsigned char * outBuff, size_t outLen,
 unsigned char * inBuff, size_t * inLen
)
{
  int ret;
  size_t actLen = 0;

  /*shut up compiler*/
  runRS=runRS;
  shortTime=shortTime;

  DBG(10, "do_scsi_cmd: start\n");

  DBG(25, "cmd: writing %d bytes\n", (int)cmdLen);
  hexdump(30, "cmd: >>", cmdBuff, cmdLen);

  if(outBuff && outLen){
    DBG(25, "out: writing %d bytes\n", (int)outLen);
    hexdump(30, "out: >>", outBuff, outLen);
  }
  if (inBuff && inLen){
    DBG(25, "in: reading %d bytes\n", (int)*inLen);
    memset(inBuff,0,*inLen);
    actLen = *inLen;
  }

  ret = sanei_scsi_cmd2(s->fd, cmdBuff, cmdLen, outBuff, outLen, inBuff, inLen);

  if(ret != SANE_STATUS_GOOD && ret != SANE_STATUS_EOF){
    DBG(5,"do_scsi_cmd: return '%s'\n",sane_strstatus(ret));
    return ret;
  }

  /* FIXME: should we look at s->rs_info here? */
  if (inBuff && inLen){
    hexdump(30, "in: <<", inBuff, *inLen);
    DBG(25, "in: read %d bytes\n", (int)*inLen);
  }

  DBG(10, "do_scsi_cmd: finish\n");

  return ret;
}

SANE_Status
do_usb_cmd(struct fujitsu *s, int runRS, int shortTime,
 unsigned char * cmdBuff, size_t cmdLen,
 unsigned char * outBuff, size_t outLen,
 unsigned char * inBuff, size_t * inLen
)
{
    /*sanei_usb overwrites the transfer size,
     * so make some local copies */
    size_t usb_cmdLen = USB_COMMAND_LEN;
    size_t usb_outLen = outLen;
    size_t usb_statLen = USB_STATUS_LEN;
    size_t askLen = 0;

    /*copy the callers buffs into larger, padded ones*/
    unsigned char usb_cmdBuff[USB_COMMAND_LEN];
    unsigned char usb_statBuff[USB_STATUS_LEN];

    int cmdTime = USB_COMMAND_TIME;
    int outTime = USB_DATA_TIME;
    int inTime = USB_DATA_TIME;
    int statTime = USB_STATUS_TIME;

    int ret = 0;
    int ret2 = 0;

    DBG (10, "do_usb_cmd: start\n");

    if(shortTime){
        cmdTime = USB_COMMAND_TIME/20;
        outTime = USB_DATA_TIME/20;
        inTime = USB_DATA_TIME/20;
        statTime = USB_STATUS_TIME/20;
    }

    /* build a USB packet around the SCSI command */
    memset(&usb_cmdBuff,0,USB_COMMAND_LEN);
    usb_cmdBuff[0] = USB_COMMAND_CODE;
    memcpy(&usb_cmdBuff[USB_COMMAND_OFFSET],cmdBuff,cmdLen);

    /* change timeout */
    sanei_usb_set_timeout(cmdTime);

    /* write the command out */
    DBG(25, "cmd: writing %d bytes, timeout %d\n", USB_COMMAND_LEN, cmdTime);
    hexdump(30, "cmd: >>", usb_cmdBuff, USB_COMMAND_LEN);
    ret = sanei_usb_write_bulk(s->fd, usb_cmdBuff, &usb_cmdLen);
    DBG(25, "cmd: wrote %d bytes, retVal %d\n", (int)usb_cmdLen, ret);

    if(ret == SANE_STATUS_EOF){
        DBG(5,"cmd: got EOF, returning IO_ERROR\n");
        return SANE_STATUS_IO_ERROR;
    }
    if(ret != SANE_STATUS_GOOD){
        DBG(5,"cmd: return error '%s'\n",sane_strstatus(ret));
        return ret;
    }
    if(usb_cmdLen != USB_COMMAND_LEN){
        DBG(5,"cmd: wrong size %d/%d\n", USB_COMMAND_LEN, (int)usb_cmdLen);
        return SANE_STATUS_IO_ERROR;
    }

    /* this command has a write component, and a place to get it */
    if(outBuff && outLen && outTime){

        /* change timeout */
        sanei_usb_set_timeout(outTime);

        DBG(25, "out: writing %d bytes, timeout %d\n", (int)outLen, outTime);
        hexdump(30, "out: >>", outBuff, outLen);
        ret = sanei_usb_write_bulk(s->fd, outBuff, &usb_outLen);
        DBG(25, "out: wrote %d bytes, retVal %d\n", (int)usb_outLen, ret);

        if(ret == SANE_STATUS_EOF){
            DBG(5,"out: got EOF, returning IO_ERROR\n");
            return SANE_STATUS_IO_ERROR;
        }
        if(ret != SANE_STATUS_GOOD){
            DBG(5,"out: return error '%s'\n",sane_strstatus(ret));
            return ret;
        }
        if(usb_outLen != outLen){
            DBG(5,"out: wrong size %d/%d\n", (int)outLen, (int)usb_outLen);
            return SANE_STATUS_IO_ERROR;
        }
    }

    /* this command has a read component, and a place to put it */
    if(inBuff && inLen && inTime){

        askLen = *inLen;
        memset(inBuff,0,askLen);

        /* change timeout */
        sanei_usb_set_timeout(inTime);

        DBG(25, "in: reading %lu bytes, timeout %d\n",
          (unsigned long)askLen, inTime);

        ret = sanei_usb_read_bulk(s->fd, inBuff, inLen);
        DBG(25, "in: retVal %d\n", ret);

        if(ret == SANE_STATUS_EOF){
            DBG(5,"in: got EOF, continuing\n");
            ret = SANE_STATUS_GOOD;
        }

        if(ret != SANE_STATUS_GOOD){
            DBG(5,"in: return error '%s'\n",sane_strstatus(ret));
            return ret;
        }

        DBG(25, "in: read %lu bytes\n", (unsigned long)*inLen);
        if(*inLen){
            hexdump(30, "in: <<", inBuff, *inLen);
        }

        if(*inLen && *inLen != askLen){
            ret = SANE_STATUS_EOF;
            DBG(5,"in: short read, %lu/%lu\n",
              (unsigned long)*inLen,(unsigned long)askLen);
        }
    }

    /*gather the scsi status byte. use ret2 instead of ret for status*/

    memset(&usb_statBuff,0,USB_STATUS_LEN);

    /* change timeout */
    sanei_usb_set_timeout(statTime);

    DBG(25, "stat: reading %d bytes, timeout %d\n", USB_STATUS_LEN, statTime);
    ret2 = sanei_usb_read_bulk(s->fd, usb_statBuff, &usb_statLen);
    hexdump(30, "stat: <<", usb_statBuff, usb_statLen);
    DBG(25, "stat: read %d bytes, retVal %d\n", (int)usb_statLen, ret2);

    if(ret2 == SANE_STATUS_EOF){
        DBG(5,"stat: got EOF, returning IO_ERROR\n");
        return SANE_STATUS_IO_ERROR;
    }
    if(ret2 != SANE_STATUS_GOOD){
        DBG(5,"stat: return error '%s'\n",sane_strstatus(ret2));
        return ret2;
    }
    if(usb_statLen != USB_STATUS_LEN){
        DBG(5,"stat: wrong size %d/%d\n", USB_STATUS_LEN, (int)usb_statLen);
        return SANE_STATUS_IO_ERROR;
    }

    /* busy status */
    if(usb_statBuff[USB_STATUS_OFFSET] == 8){
        DBG(25,"stat: busy\n");
        return SANE_STATUS_DEVICE_BUSY;
    }

    /* if there is a non-busy status >0, try to figure out why */
    if(usb_statBuff[USB_STATUS_OFFSET] > 0){
      DBG(25,"stat: value %d\n", usb_statBuff[USB_STATUS_OFFSET]);

      /* caller is interested in having RS run on errors */
      if(runRS){
        unsigned char rsBuff[RS_return_size];
        size_t rs_datLen = RS_return_size;
  
        DBG(25,"rs sub call >>\n");
        ret2 = do_cmd(
          s,0,0,
          request_senseB.cmd, request_senseB.size,
          NULL,0,
          rsBuff, &rs_datLen
        );
        DBG(25,"rs sub call <<\n");
  
        if(ret2 == SANE_STATUS_EOF){
          DBG(5,"rs: got EOF, returning IO_ERROR\n");
          return SANE_STATUS_IO_ERROR;
        }
        if(ret2 != SANE_STATUS_GOOD){
          DBG(5,"rs: return error '%s'\n",sane_strstatus(ret2));
          return ret2;
        }

        /* parse the rs data */
        ret2 = sense_handler( 0, rsBuff, (void *)s );

        /* this was a short read, but the usb layer did not know */
        if(ret2 == SANE_STATUS_EOF && s->rs_info
          && inBuff && inLen && inTime){
            *inLen = askLen - s->rs_info;
            s->rs_info = 0;
            DBG(5,"do_usb_cmd: short read via rs, %lu/%lu\n",
              (unsigned long)*inLen,(unsigned long)askLen);
        }
        return ret2;
      }
      else{
        DBG(5,"do_usb_cmd: Not calling rs!\n");
        return SANE_STATUS_IO_ERROR;
      }
    }

    DBG (10, "do_usb_cmd: finish\n");

    return ret;
}

static int
wait_scanner(struct fujitsu *s) 
{
  int ret;

  DBG (10, "wait_scanner: start\n");

  ret = do_cmd (
    s, 0, 1,
    test_unit_readyB.cmd, test_unit_readyB.size,
    NULL, 0,
    NULL, NULL
  );
  
  if (ret != SANE_STATUS_GOOD) {
    DBG(5,"WARNING: Brain-dead scanner. Hitting with stick\n");
    ret = do_cmd (
      s, 0, 1,
      test_unit_readyB.cmd, test_unit_readyB.size,
      NULL, 0,
      NULL, NULL
    );
  }
  if (ret != SANE_STATUS_GOOD) {
    DBG(5,"WARNING: Brain-dead scanner. Hitting with stick again\n");
    ret = do_cmd (
      s, 0, 1,
      test_unit_readyB.cmd, test_unit_readyB.size,
      NULL, 0,
      NULL, NULL
    );
  }

  if (ret != SANE_STATUS_GOOD) {
    DBG (5, "wait_scanner: error '%s'\n", sane_strstatus (ret));
  }

  DBG (10, "wait_scanner: finish\n");

  return ret;
}

/* s->page_width stores the user setting
 * for the paper width in adf. sometimes,
 * we need a value that differs from this
 * due to using FB or overscan.
 */
int
get_page_width(struct fujitsu *s) 
{

  /* scanner max for fb */
  if(s->source == SOURCE_FLATBED){
      return s->max_x;
  }

  /* current paper size for adf not overscan */
  if(s->overscan != MSEL_ON){
      return s->page_width;
  }

  /* cant overscan larger than scanner max */
  if((s->page_width + s->os_x_basic*2) > s->max_x){
      return s->max_x;
  }

  /* overscan adds a margin to both sides */
  return (s->page_width + s->os_x_basic*2);
}

/* s->page_height stores the user setting
 * for the paper height in adf. sometimes,
 * we need a value that differs from this
 * due to using FB or overscan.
 */
int
get_page_height(struct fujitsu *s) 
{

  /* scanner max for fb */
  if(s->source == SOURCE_FLATBED){
      return s->max_y;
  }

  /* current paper size for adf not overscan */
  if(s->overscan != MSEL_ON){
      return s->page_height;
  }

  /* cant overscan larger than scanner max */
  if((s->page_height + s->os_y_basic*2) > s->max_y){
      return s->max_y;
  }

  /* overscan adds a margin to both sides */
  return (s->page_height + s->os_y_basic*2);
}


/**
 * Convenience method to determine longest string size in a list.
 */
static size_t
maxStringSize (const SANE_String_Const strings[])
{
  size_t size, max_size = 0;
  int i;

  for (i = 0; strings[i]; ++i) {
    size = strlen (strings[i]) + 1;
    if (size > max_size)
      max_size = size;
  }

  return max_size;
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

  if(DBG_LEVEL < level)
    return;

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
 * An advanced method we don't support but have to define.
 */
SANE_Status
sane_set_io_mode (SANE_Handle h, SANE_Bool non_blocking)
{
  DBG (10, "sane_set_io_mode\n");
  DBG (15, "%d %p\n", non_blocking, h);
  return SANE_STATUS_UNSUPPORTED;
}

/**
 * An advanced method we don't support but have to define.
 */
SANE_Status
sane_get_select_fd (SANE_Handle h, SANE_Int *fdp)
{
  DBG (10, "sane_get_select_fd\n");
  DBG (15, "%p %d\n", h, *fdp);
  return SANE_STATUS_UNSUPPORTED;
}
