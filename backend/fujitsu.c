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
   search for the tag "@@".

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
           returned by the scanner.
      V 1.0.4, 2002-09-13, OS
         - 3092 support (mgoppold@tbz-pariv.de)
         - tested 4097 support
         - changed some functions to receive compressed data
      V 1.0.4, 2003-02-13, OS
         - fi-4220C support (ron@roncemer.com)
         - SCSI over USB support (ron@roncemer.com)
      V 1.0.5, 2003-02-20, OS
         - set availability of options THRESHOLD und VARIANCE
         - option RIF is available for 3091 und 3092
      V 1.0.6, 2003-03-04, OS
         - renamed some variables
         - bugfix: duplex scanning now works when disconnect is enabled
           in the scsi controller.
      V 1.0.7, 2003-03-10, OS
         - displays the offending byte when something is wrong in the
           window descriptor block.
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
	   in format rr...r gg.g bb...b the reader process crashed.
	 - Bugfix. The option gamma was enabled for
	   the fi-4120. The result was an 'invalid field in parm list'-error.
      V 1.0.14 2003-12-15, OS
         - Bugfix: set default threshold range to 0..255 There is a problem
           with the M3093 when you are not allows to set the threshold to 0.
         - Bugfix: set the allowable x- and y-DPI values from VPD. Scanning
           with x=100 and y=100 dpi with an fi4120 resulted in an image
           with 100,75 dpi.
         - Bugfix: Set the default value of gamma to 0x80 for all scanners
           that don't have build in gamma patterns.
         - Bugfix: fi-4530 and fi-4210 don't support standard paper size
           spezification. Disable this option for these scanners.
      V 1.0.15 2003-12-16, OS
         - Bugfix: pagewidth and pageheight where disable for the fi-4530C.
      V 1.0.16 2004-02-20, OS
         - merged the 3092-routines with the 3091-routines.
         - inverted the image in mode color and grayscale
         - jpg hardware compression support (fi-4530C)
      V 1.0.17 2004-03-04, OS
         - enabled option dropoutcolor for the fi-4530C, and fi-4x20C
      V 1.0.18 2004-06-02, OS
         - bugfix: can read duplex color now
      V 1.0.19 2004-06-28, MAN
         - 4220 use model code not strcmp (stan@saticed.me.uk)
      V 1.0.20 2004-08-24, OS
         - bugfix: 3091 did not work since 15.12.2003 
         - M4099 supported (bw only)
      V 1.0.21 2006-05-01, MAN
         - Complete rewrite, half code size
         - better (read: correct) usb command support
         - basic support for most fi-series
         - most scanner capabilites read from VPD
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
#include "sane/sanei_thread.h"

#include "fujitsu-scsi.h"
#include "fujitsu.h"

#define DEBUG 1
#define FUJITSU_V_POINT 25

/* values for SANE_DEBUG_FUJITSU env var:
 - errors           5
 - function trace  10
 - function detail 15
 - get/setopt cmds 20
 - scsi/usb data   30
*/

/* ------------------------------------------------------------------------- */
static const char source_Flatbed[] = "Flatbed";
static const char source_ADFFront[] = "ADF Front";
static const char source_ADFBack[] = "ADF Back";
static const char source_ADFDuplex[] = "ADF Duplex";

static const char mode_Lineart[] = "Lineart";
static const char mode_Halftone[] = "Halftone";
static const char mode_Grayscale[] = "Gray";
static const char mode_Color_lineart[] = "Color Lineart";
static const char mode_Color_halftone[] = "Color Halftone";
static const char mode_Color[] = "Color";

static const char color_Red[] = "Red";
static const char color_Green[] = "Green";
static const char color_Blue[] = "Blue";
static const char color_Default[] = "Default";

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
  sanei_thread_init();

  /*FIXME*/
  if (version_code)
    *version_code = SANE_VERSION_CODE (V_MAJOR, V_MINOR, FUJITSU_V_POINT);

  DBG (5, "sane_init: backend version %d.%d.%d\n", V_MAJOR, V_MINOR, FUJITSU_V_POINT);

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
  size_t len;
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
    
          /* ignore comments */
          if (line[0] == '#')
            continue;
    
          /* delete newline characters at end */
          len = strlen (line);
          if (line[len - 1] == '\n')
            line[--len] = '\0';
    
          lp = sanei_config_skip_whitespace (line);
    
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

      DBG (15, "find_scanners: looking for 'usb 0x04c5 0x1096'\n");
      sanei_usb_attach_matching_devices("usb 0x04c5 0x1096", attach_one_usb);

      DBG (15, "find_scanners: looking for 'usb 0x04c5 0x1097'\n");
      sanei_usb_attach_matching_devices("usb 0x04c5 0x1097", attach_one_usb);

      DBG (15, "find_scanners: looking for 'usb 0x04c5 0x10e0'\n");
      sanei_usb_attach_matching_devices("usb 0x04c5 0x10e0", attach_one_usb);

      DBG (15, "find_scanners: looking for 'usb 0x04c5 0x10ae'\n");
      sanei_usb_attach_matching_devices("usb 0x04c5 0x10ae", attach_one_usb);
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
  if ((s->buffer = calloc (s->buffer_size, 1)) == NULL){
    free (s);
    return SANE_STATUS_NO_MEM;
  }

  /* copy the device name */
  s->device_name = strdup (device_name);
  if (!s->device_name){
    free (s->buffer);
    free (s);
    return SANE_STATUS_NO_MEM;
  }

  /* connect the fd */
  s->connection = connType;
  s->fd = -1;
  s->duplex_fd = -1;
  ret = connect_fd(s);
  if(ret != SANE_STATUS_GOOD){
    free (s->device_name);
    free (s->buffer);
    free (s);
    return ret;
  }

  /* Now query the device to load its vendor/model/version */
  ret = init_inquire (s);
  if (ret != SANE_STATUS_GOOD) {
    disconnect_fd(s);
    free (s->device_name);
    free (s->buffer);
    free (s);
    DBG (5, "attach_one: inquiry failed\n");
    return ret;
  }

  /* load detailed specs/capabilities from the device */
  ret = init_vpd (s);
  if (ret != SANE_STATUS_GOOD) {
    disconnect_fd(s);
    free (s->device_name);
    free (s->buffer);
    free (s);
    DBG (5, "attach_one: vpd failed\n");
    return ret;
  }

  /* clean up the scanner struct based on model */
  /* this is the only piece of model specific code */
  /* sets SANE option 'values' to good defaults */
  ret = init_model (s);
  if (ret != SANE_STATUS_GOOD) {
    disconnect_fd(s);
    free (s->device_name);
    free (s->buffer);
    free (s);
    DBG (5, "attach_one: model failed\n");
    return ret;
  }

  ret = init_options (s);
  if (ret != SANE_STATUS_GOOD) {
    disconnect_fd(s);
    free (s->buffer);
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

  DBG (10, "init_inquire: start\n");

  set_IN_return_size (inquiryB.cmd, 96);
  set_IN_evpd (inquiryB.cmd, 0);
  set_IN_page_code (inquiryB.cmd, 0);
 
  ret = do_cmd (
    s, 0, 0, 1, 0, 
    inquiryB.cmd, inquiryB.size,
    NULL, 0,
    s->buffer, 96
  );

  if (ret != SANE_STATUS_GOOD){
    return ret;
  }

  if (get_IN_periph_devtype (s->buffer) != IN_periph_devtype_scanner){
    DBG (5, "The device at '%s' is not a scanner.\n", s->device_name);
    return SANE_STATUS_INVAL;
  }

  get_IN_vendor ((char*) s->buffer, s->vendor_name);
  get_IN_product ((char*) s->buffer, s->product_name);
  get_IN_version ((char*) s->buffer, s->version_name);

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

  /*some scanners (3091/2) list raster offsets here*/
  s->color_raster_offset = get_IN_raster(s->buffer);
  s->duplex_raster_offset = get_IN_frontback(s->buffer);

  DBG (10, "init_inquire: finish\n");

  return 0;
}

/*
 * Use INQUIRY VPD to setup more detail about the scanner
 */
static SANE_Status
init_vpd (struct fujitsu *s)
{
  SANE_Status ret;

  DBG (10, "init_vpd: start\n");

  /* get EVPD */
  set_IN_return_size (inquiryB.cmd, 0x64);
  set_IN_evpd (inquiryB.cmd, 1);
  set_IN_page_code (inquiryB.cmd, 0xf0);

  ret = do_cmd (
    s, 0, 0, 1, 0,
    inquiryB.cmd, inquiryB.size,
    NULL, 0,
    s->buffer, 0x64
  );

  /* This scanner supports vital product data.
   * Use this data to set dpi-lists etc. */
  if (ret == SANE_STATUS_GOOD) {

      DBG (15, "standard options\n");

      s->basic_x_res = get_IN_basic_x_res (s->buffer);
      DBG (15, "  basic x res: %d dpi\n",s->basic_x_res);

      s->basic_y_res = get_IN_basic_y_res (s->buffer);
      DBG (15, "  basic y res: %d dpi\n",s->basic_y_res);

      s->step_x_res = get_IN_step_x_res (s->buffer);
      DBG (15, "  step x res: %d dpi\n", s->step_x_res);

      s->step_y_res = get_IN_step_y_res (s->buffer);
      DBG (15, "  step y res: %d dpi\n", s->step_y_res);

      s->max_x_res = get_IN_max_x_res (s->buffer);
      DBG (15, "  max x res: %d dpi\n", s->max_x_res);

      s->max_y_res = get_IN_max_y_res (s->buffer);
      DBG (15, "  max y res: %d dpi\n", s->max_y_res);

      s->min_x_res = get_IN_min_x_res (s->buffer);
      DBG (15, "  min x res: %d dpi\n", s->min_x_res);

      s->min_y_res = get_IN_min_y_res (s->buffer);
      DBG (15, "  min y res: %d dpi\n", s->min_y_res);

      /* some scanners list B&W resolutions. */
      s->std_res_60 = get_IN_std_res_60 (s->buffer);
      DBG (15, "  60 dpi: %d\n", s->std_res_60);

      s->std_res_75 = get_IN_std_res_75 (s->buffer);
      DBG (15, "  75 dpi: %d\n", s->std_res_75);

      s->std_res_100 = get_IN_std_res_100 (s->buffer);
      DBG (15, "  100 dpi: %d\n", s->std_res_100);

      s->std_res_120 = get_IN_std_res_120 (s->buffer);
      DBG (15, "  120 dpi: %d\n", s->std_res_120);

      s->std_res_150 = get_IN_std_res_150 (s->buffer);
      DBG (15, "  150 dpi: %d\n", s->std_res_150);

      s->std_res_160 = get_IN_std_res_160 (s->buffer);
      DBG (15, "  160 dpi: %d\n", s->std_res_160);

      s->std_res_180 = get_IN_std_res_180 (s->buffer);
      DBG (15, "  180 dpi: %d\n", s->std_res_180);

      s->std_res_200 = get_IN_std_res_200 (s->buffer);
      DBG (15, "  200 dpi: %d\n", s->std_res_200);

      s->std_res_240 = get_IN_std_res_240 (s->buffer);
      DBG (15, "  240 dpi: %d\n", s->std_res_240);

      s->std_res_300 = get_IN_std_res_300 (s->buffer);
      DBG (15, "  300 dpi: %d\n", s->std_res_300);

      s->std_res_320 = get_IN_std_res_320 (s->buffer);
      DBG (15, "  320 dpi: %d\n", s->std_res_320);

      s->std_res_400 = get_IN_std_res_400 (s->buffer);
      DBG (15, "  400 dpi: %d\n", s->std_res_400);

      s->std_res_480 = get_IN_std_res_480 (s->buffer);
      DBG (15, "  480 dpi: %d\n", s->std_res_480);

      s->std_res_600 = get_IN_std_res_600 (s->buffer);
      DBG (15, "  600 dpi: %d\n", s->std_res_600);

      s->std_res_800 = get_IN_std_res_800 (s->buffer);
      DBG (15, "  800 dpi: %d\n", s->std_res_800);

      s->std_res_1200 = get_IN_std_res_1200 (s->buffer);
      DBG (15, "  1200 dpi: %d\n", s->std_res_1200);

      /* maximum window width and length are reported in basic units.*/
      s->max_x_basic = get_IN_window_width(s->buffer);
      DBG(15, "  max width: %2.2f inches\n",(float)s->max_x_basic/s->basic_x_res);

      s->max_y_basic = get_IN_window_length(s->buffer);
      DBG(15, "  max length: %2.2f inches\n",(float)s->max_y_basic/s->basic_y_res);

      /* known modes */
      s->can_overflow = get_IN_overflow(s->buffer);
      DBG (15, "  overflow: %d\n", s->can_overflow);

      s->can_monochrome = get_IN_monochrome (s->buffer);
      DBG (15, "  monochrome: %d\n", s->can_monochrome);

      s->can_halftone = get_IN_half_tone (s->buffer);
      DBG (15, "  halftone: %d\n", s->can_halftone);

      s->can_grayscale = get_IN_multilevel (s->buffer);
      DBG (15, "  grayscale: %d\n", s->can_grayscale);

      s->can_color_monochrome = get_IN_monochrome_rgb(s->buffer);
      DBG (15, "  color_monochrome: %d\n", s->can_color_monochrome);

      s->can_color_halftone = get_IN_half_tone_rgb(s->buffer);
      DBG (15, "  color_halftone: %d\n", s->can_color_halftone);

      s->can_color_grayscale = get_IN_multilevel_rgb (s->buffer);
      DBG (15, "  color_grayscale: %d\n", s->can_color_grayscale);

      /* now we look at vendor specific data */
      if (get_IN_page_length (s->buffer) > 0x1C) {

          DBG (15, "vendor options\n");

          s->has_operator_panel = get_IN_operator_panel(s->buffer);
          DBG (15, "  operator panel: %d\n", s->has_operator_panel);

          s->has_barcode = get_IN_barcode(s->buffer);
          DBG (15, "  barcode: %d\n", s->has_barcode);

          s->has_imprinter = get_IN_imprinter(s->buffer);
          DBG (15, "  imprinter: %d\n", s->has_imprinter);

          s->has_duplex = get_IN_duplex(s->buffer);
          DBG (15, "  duplex: %d\n", s->has_duplex);

          s->has_transparency = get_IN_transparency(s->buffer);
          DBG (15, "  transparency: %d\n", s->has_transparency);

          s->has_flatbed = get_IN_flatbed(s->buffer);
          DBG (15, "  flatbed: %d\n", s->has_flatbed);

          s->has_adf = get_IN_adf(s->buffer);
          DBG (15, "  adf: %d\n", s->has_adf);

          s->adbits = get_IN_adbits(s->buffer);
          DBG (15, "  A/D bits: %d\n",s->adbits);

          s->buffer_bytes = get_IN_buffer_bytes(s->buffer);
          DBG (15, "  buffer bytes: %d\n",s->buffer_bytes);

          /* vendor added scsi command support */
          /* FIXME: there are more of these... */
          s->has_cmd_subwindow = get_IN_has_subwindow(s->buffer);
          DBG (15, "  subwindow cmd: %d\n", s->has_cmd_subwindow);

          s->has_cmd_endorser = get_IN_has_endorser(s->buffer);
          DBG (15, "  endorser cmd: %d\n", s->has_cmd_endorser);

          s->has_cmd_hw_status = get_IN_has_hw_status (s->buffer);
          DBG (15, "  hardware status cmd: %d\n", s->has_cmd_hw_status);

          s->has_cmd_scanner_ctl = get_IN_has_scanner_ctl(s->buffer);
          DBG (15, "  scanner control cmd: %d\n", s->has_cmd_scanner_ctl);

          /* get threshold, brightness and contrast ranges. */
          s->brightness_steps = get_IN_brightness_steps(s->buffer);
          DBG (15, "  brightness steps: %d\n", s->brightness_steps);

          s->threshold_steps = get_IN_threshold_steps(s->buffer);
          DBG (15, "  threshold steps: %d\n", s->threshold_steps);

          s->contrast_steps = get_IN_contrast_steps(s->buffer);
          DBG (15, "  contrast steps: %d\n", s->contrast_steps);

          /* dither/gamma patterns */
	  s->num_internal_gamma = get_IN_num_gamma_internal (s->buffer);
          DBG (15, "  built in gamma patterns: %d\n", s->num_internal_gamma);

	  s->num_download_gamma = get_IN_num_gamma_download (s->buffer);
          DBG (15, "  download gamma patterns: %d\n", s->num_download_gamma);

	  s->num_internal_dither = get_IN_num_dither_internal (s->buffer);
          DBG (15, "  built in dither patterns: %d\n", s->num_internal_dither);

	  s->num_download_dither = get_IN_num_dither_download (s->buffer);
          DBG (15, "  download dither patterns: %d\n", s->num_download_dither);

          /* ipc functions */
	  s->has_rif = get_IN_ipc_bw_rif (s->buffer);
          DBG (15, "  black and white rif: %d\n", s->has_rif);

          s->has_auto1 = get_IN_ipc_auto1(s->buffer);
          DBG (15, "  automatic binary DTC: %d\n", s->has_auto1);

          s->has_auto2 = get_IN_ipc_auto2(s->buffer);
          DBG (15, "  simplified DTC: %d\n", s->has_auto2);

          s->has_outline = get_IN_ipc_outline_extraction (s->buffer);
          DBG (15, "  outline extraction: %d\n", s->has_outline);

          s->has_emphasis = get_IN_ipc_image_emphasis (s->buffer);
          DBG (15, "  image emphasis: %d\n", s->has_emphasis);

          s->has_autosep = get_IN_ipc_auto_separation (s->buffer);
          DBG (15, "  automatic separation: %d\n", s->has_autosep);

          s->has_mirroring = get_IN_ipc_mirroring (s->buffer);
          DBG (15, "  mirror image: %d\n", s->has_mirroring);

          s->has_white_level_follow = get_IN_ipc_white_level_follow (s->buffer);
          DBG (15, "  white level follower: %d\n", s->has_white_level_follow);

          s->has_subwindow = get_IN_ipc_subwindow (s->buffer);
          DBG (15, "  subwindow: %d\n", s->has_subwindow);

          /* compression modes */
          s->has_comp_MH = get_IN_compression_MH (s->buffer);
          DBG (15, "  compression MH: %d\n", s->has_comp_MH);

          s->has_comp_MR = get_IN_compression_MR (s->buffer);
          DBG (15, "  compression MR: %d\n", s->has_comp_MR);

          s->has_comp_MMR = get_IN_compression_MMR (s->buffer);
          DBG (15, "  compression MMR: %d\n", s->has_comp_MMR);

          s->has_comp_JBIG = get_IN_compression_JBIG (s->buffer);
          DBG (15, "  compression JBIG: %d\n", s->has_comp_JBIG);

          s->has_comp_JPG1 = get_IN_compression_JPG_BASE (s->buffer);
          DBG (15, "  compression JPG1: %d\n", s->has_comp_JPG1);

          s->has_comp_JPG2 = get_IN_compression_JPG_EXT (s->buffer);
          DBG (15, "  compression JPG2: %d\n", s->has_comp_JPG2);

          s->has_comp_JPG3 = get_IN_compression_JPG_INDEP (s->buffer);
          DBG (15, "  compression JPG3: %d\n", s->has_comp_JPG3);
          
      }
      /*FIXME no vendor vpd, set some defaults? */
      else{
      }
  }
  /*FIXME no vpd, set some defaults? */
  else{
  }

  DBG (10, "init_vpd: finish\n");

  return ret;
}

/*
 * set model specific info and good default user values 
 * in scanner struct. struct is already initialized to 0,
 * so all the default 0 values dont need touching.
 */
static SANE_Status
init_model (struct fujitsu *s)
{

  DBG (10, "init_model: start\n");

  /* converted to native scanner unit of 1200dpi */
  /* FIXME: put these elsewhere, make them variable? */
  s->max_x = s->max_x_basic * 1200 / s->basic_x_res;
  s->max_y = s->max_y_basic * 1200 / s->basic_y_res;
  s->min_x=0;
  s->min_y=0;

  /* give defaults to all the user settings */

  /* "Mode" group -------------------------------------------------------- */

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
  else if(s->can_color_monochrome)
    s->mode=MODE_COLOR_LINEART;
  else if(s->can_color_halftone)
    s->mode=MODE_COLOR_HALFTONE;
  else if(s->can_color_grayscale)
    s->mode=MODE_COLOR;

  /*x res*/
  s->resolution_x = s->basic_x_res;

  /*y res*/
  s->resolution_y = s->basic_y_res;
  if(s->resolution_y > s->resolution_x){
    s->resolution_y = s->resolution_x;
  }

  /* "Geometry" group ---------------------------------------------------- */

  /* bottom-right x */
  s->br_x = 8.5 * 1200;
  if(s->br_x > s->max_x){
    s->br_x = s->max_x;
  }

  /* bottom-right y */
  s->br_y = 11 * 1200;
  if(s->br_y > s->max_y){
    s->br_y = s->max_y;
  }

  /* page width */
  s->page_width = s->br_x;

  /* page height */
  s->page_height = s->br_y;

  /* "Enhancement" group --------------------------------------------------- */

  /* most scanners dont support gamma, but the default varies */
  /* newer models want 0x80, older want 0x00, override below */
  s->gamma = 0x80;

  /* ------------------------------------------------------ */
  /* load vars that cannot be gotten from vpd */
  s->has_back = s->has_duplex;
  s->color_interlace = COLOR_INTERLACE_NONE;
  s->duplex_interlace = DUPLEX_INTERLACE_NONE;
  s->has_MS_dropout = 1;
  s->has_SW_dropout = 0;
  s->window_vid = 0;
  s->ghs_in_rs = 0;

  s->reverse_by_mode[MODE_LINEART] = 0;
  s->reverse_by_mode[MODE_HALFTONE] = 0;
  s->reverse_by_mode[MODE_GRAYSCALE] = 1;
  s->reverse_by_mode[MODE_COLOR_LINEART] = 0;
  s->reverse_by_mode[MODE_COLOR_HALFTONE] = 0;
  s->reverse_by_mode[MODE_COLOR] = 1;

  /* now we override any of the above based on model */
  if (strstr (s->product_name, "3091")) {
    s->gamma = 0;
    s->has_rif=1;

    s->has_back = 0;
    s->color_interlace = COLOR_INTERLACE_3091;
    s->duplex_interlace = DUPLEX_INTERLACE_3091;
    s->has_MS_dropout = 0;
    s->has_SW_dropout = 1;
    s->window_vid = 0xc0;
    s->ghs_in_rs = 1;

    s->reverse_by_mode[MODE_LINEART] = 1;
    s->reverse_by_mode[MODE_HALFTONE] = 1;
    s->reverse_by_mode[MODE_GRAYSCALE] = 0;
    s->reverse_by_mode[MODE_COLOR_LINEART] = 1;
    s->reverse_by_mode[MODE_COLOR_HALFTONE] = 1;
    s->reverse_by_mode[MODE_COLOR] = 0;
  }
  else if (strstr (s->product_name, "3092")) {
    s->gamma = 0;
    s->has_rif=1;

    s->has_back = 0;
    s->has_MS_dropout = 0;
    s->has_SW_dropout = 1;
    s->color_interlace = COLOR_INTERLACE_3091;
    s->duplex_interlace = DUPLEX_INTERLACE_3091;
    s->window_vid = 0xc0;
    s->ghs_in_rs = 1;

    s->reverse_by_mode[MODE_LINEART] = 1;
    s->reverse_by_mode[MODE_HALFTONE] = 1;
    s->reverse_by_mode[MODE_GRAYSCALE] = 0;
    s->reverse_by_mode[MODE_COLOR_LINEART] = 1;
    s->reverse_by_mode[MODE_COLOR_HALFTONE] = 1;
    s->reverse_by_mode[MODE_COLOR] = 0;
  }
  else if (strstr (s->product_name, "3093")) {
  }
  else if (strstr (s->product_name, "3096")) {
    s->window_vid = 0xc0;
  }
  else if (strstr (s->product_name, "3097")) {
  }
  else if (strstr (s->product_name, "4097") || strstr (s->product_name, "4099")) {
  }
  else if (strstr (s->product_name, "fi-4120") || strstr (s->product_name, "fi-4220")) {
    s->color_interlace = COLOR_INTERLACE_BGR;
  }
  else if (strstr (s->product_name, "fi-")) {
    s->color_interlace = COLOR_INTERLACE_BGR;
  }

  DBG (10, "init_model: finish\n");

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
  struct fujitsu *scanner = NULL;
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
    scanner = fujitsu_devList;
  }
  else{
    DBG (15, "sane_open: device %s requested\n", name);
                                                                                
    for (dev = fujitsu_devList; dev; dev = dev->next) {
      if (strcmp (dev->sane.name, name) == 0) {
        scanner = dev;
        break;
      }
    }
  }

  if (!scanner) {
    DBG (5, "sane_open: no device found\n");
    return SANE_STATUS_INVAL;
  }

  DBG (15, "sane_open: device %s found\n", scanner->sane.name);

  *handle = scanner;

  /* connect the fd so we can talk to scanner */
  ret = connect_fd(scanner);
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
      s->source_list[i++]=source_Flatbed;
    }
    if(s->has_adf){
      s->source_list[i++]=source_ADFFront;
  
      if(s->has_back){
        s->source_list[i++]=source_ADFBack;
      }
      if(s->has_duplex){
        s->source_list[i++]=source_ADFDuplex;
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
      s->mode_list[i++]=mode_Lineart;
    }
    if(s->can_halftone){
      s->mode_list[i++]=mode_Halftone;
    }
    if(s->can_grayscale){
      s->mode_list[i++]=mode_Grayscale;
    }
    if(s->can_color_monochrome){
      s->mode_list[i++]=mode_Color_lineart;
    }
    if(s->can_color_halftone){
      s->mode_list[i++]=mode_Color_halftone;
    }
    if(s->can_color_grayscale){
      s->mode_list[i++]=mode_Color;
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
    s->tl_x_range.max = SCANNER_UNIT_TO_FIXED_MM(s->max_x);
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
    s->tl_y_range.max = SCANNER_UNIT_TO_FIXED_MM(s->max_y);
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
    s->br_x_range.max = SCANNER_UNIT_TO_FIXED_MM(s->max_x);
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
    s->br_y_range.max = SCANNER_UNIT_TO_FIXED_MM(s->max_y);
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
    s->brightness_range.min=0;
    s->brightness_range.max=s->brightness_steps;
    s->brightness_range.quant=1;
    if (s->brightness_steps){
      opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    }
    else{
      opt->cap = SANE_CAP_INACTIVE;
    }
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

  /* contrast */
  if(option==OPT_CONTRAST){
    opt->name = SANE_NAME_CONTRAST;
    opt->title = SANE_TITLE_CONTRAST;
    opt->desc = SANE_DESC_CONTRAST;
    opt->type = SANE_TYPE_INT;
    opt->unit = SANE_UNIT_NONE;
    opt->constraint_type = SANE_CONSTRAINT_RANGE;
    opt->constraint.range = &s->contrast_range;
    s->contrast_range.min=0;
    s->contrast_range.max=s->contrast_steps;
    s->contrast_range.quant=1;
    if (s->contrast_steps) {
      opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    } 
    else {
      opt->cap = SANE_CAP_INACTIVE;
    }
  }

  /*rif*/
  if(option==OPT_RIF){
    opt->name = "rif";
    opt->title = "rif";
    opt->desc = "reverse image format";
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

  /*dropout color*/
  if(option==OPT_DROPOUT_COLOR){
    s->do_color_list[0] = color_Default;
    s->do_color_list[1] = color_Red;
    s->do_color_list[2] = color_Green;
    s->do_color_list[3] = color_Blue;
    s->do_color_list[4] = NULL;
  
    opt->name = "dropoutcolor";
    opt->title = "dropout color";
    opt->desc = "One-pass color scanners read three colors; only one of them is used in non-color scanning. Sometimes useful for colored paper or ink.";
    opt->type = SANE_TYPE_STRING;
    opt->constraint_type = SANE_CONSTRAINT_STRING_LIST;
    opt->constraint.string_list = s->do_color_list;
    opt->size = maxStringSize (opt->constraint.string_list);
    if ((s->has_MS_dropout || s->has_SW_dropout) && s->mode != MODE_COLOR)
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
    opt->title = "sleep timer";
    opt->desc = "time in minutes until the internal power supply switches to sleep mode"; 
    opt->type = SANE_TYPE_INT;
    opt->unit = SANE_UNIT_NONE;
    opt->constraint_type = SANE_CONSTRAINT_RANGE;
    opt->constraint.range=&s->sleep_time_range;
    opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
  }

  /*duplex offset*/
  if(option==OPT_DUPLEX_OFFSET){
    s->duplex_offset_range.min = -16;
    s->duplex_offset_range.max = 16;
    s->duplex_offset_range.quant = 1;
  
    opt->name = "duplexoffset";
    opt->title = "back scan line offset from default";
    opt->desc = "Use small positive/negative values to correct for miscalibration.";
    opt->type = SANE_TYPE_INT;
    opt->unit = SANE_UNIT_NONE;
    opt->constraint_type = SANE_CONSTRAINT_RANGE;
    opt->constraint.range = &s->duplex_offset_range;
    if(s->duplex_interlace == DUPLEX_INTERLACE_3091)
      opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
    else
      opt->cap = SANE_CAP_INACTIVE;
  }

  if(option==OPT_BLUE_OFFSET){
    s->blue_offset_range.min = -16;
    s->blue_offset_range.max = 16;
    s->blue_offset_range.quant = 1;
  
    opt->name = "blueoffset";
    opt->title = "blue scan line offset from default";
    opt->desc = "Use small positive/negative values to correct for miscalibration.";
    opt->type = SANE_TYPE_INT;
    opt->unit = SANE_UNIT_NONE;
    opt->constraint_type = SANE_CONSTRAINT_RANGE;
    opt->constraint.range = &s->blue_offset_range;
    if(s->color_interlace == COLOR_INTERLACE_3091)
      opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
    else
      opt->cap = SANE_CAP_INACTIVE;
  }
  
  if(option==OPT_GREEN_OFFSET){
    s->green_offset_range.min = -16;
    s->green_offset_range.max = 16;
    s->green_offset_range.quant = 1;
  
    opt->name = "greenoffset";
    opt->title = "green scan line offset from default";
    opt->desc =
      "Use small positive/negative values to correct for miscalibration.";
    opt->type = SANE_TYPE_INT;
    opt->unit = SANE_UNIT_NONE;
    opt->constraint_type = SANE_CONSTRAINT_RANGE;
    opt->constraint.range = &s->green_offset_range;
    if(s->color_interlace == COLOR_INTERLACE_3091)
      opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
    else
      opt->cap = SANE_CAP_INACTIVE;
  }
  
  if(option==OPT_USE_SWAPFILE){
    opt->name = "swapfile";
    opt->title = "swap file";
    opt->desc =
      "When set, data is buffered in a temp file rather than in memory. Use this option if you are scanning at higher resolution and are low on memory.";
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
    opt->name = "top-edge";
    opt->title = "top edge";
    opt->desc = "paper is pulled partly into adf";
    opt->type = SANE_TYPE_BOOL;
    opt->unit = SANE_UNIT_NONE;
    if (s->has_cmd_hw_status || s->ghs_in_rs)
      opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
    else 
      opt->cap = SANE_CAP_INACTIVE;
  }

  if(option==OPT_A3){
    opt->name = "a3";
    opt->title = "A3 paper";
    opt->desc = "A3 paper detected";
    opt->type = SANE_TYPE_BOOL;
    opt->unit = SANE_UNIT_NONE;
    if (s->has_cmd_hw_status)
      opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
    else 
      opt->cap = SANE_CAP_INACTIVE;
  }

  if(option==OPT_B4){
    opt->name = "b4";
    opt->title = "B4 paper";
    opt->desc = "B4 paper detected";
    opt->type = SANE_TYPE_BOOL;
    opt->unit = SANE_UNIT_NONE;
    if (s->has_cmd_hw_status)
      opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
    else 
      opt->cap = SANE_CAP_INACTIVE;
  }

  if(option==OPT_A4){
    opt->name = "a4";
    opt->title = "A4 paper";
    opt->desc = "A4 paper detected";
    opt->type = SANE_TYPE_BOOL;
    opt->unit = SANE_UNIT_NONE;
    if (s->has_cmd_hw_status)
      opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
    else 
      opt->cap = SANE_CAP_INACTIVE;
  }

  if(option==OPT_B5){
    opt->name = "b5";
    opt->title = "B5 paper";
    opt->desc = "B5 paper detected";
    opt->type = SANE_TYPE_BOOL;
    opt->unit = SANE_UNIT_NONE;
    if (s->has_cmd_hw_status)
      opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
    else 
      opt->cap = SANE_CAP_INACTIVE;
  }

  if(option==OPT_HOPPER){
    opt->name = "adf-loaded";
    opt->title = "ADF loaded";
    opt->desc = "paper detected in adf hopper";
    opt->type = SANE_TYPE_BOOL;
    opt->unit = SANE_UNIT_NONE;
    if (s->has_cmd_hw_status || s->ghs_in_rs)
      opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
    else 
      opt->cap = SANE_CAP_INACTIVE;
  }

  if(option==OPT_OMR){
    opt->name = "omr-df";
    opt->title = "OMR or DF";
    opt->desc = "OMR or double feed detected";
    opt->type = SANE_TYPE_BOOL;
    opt->unit = SANE_UNIT_NONE;
    if (s->has_cmd_hw_status)
      opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
    else 
      opt->cap = SANE_CAP_INACTIVE;
  }

  if(option==OPT_ADF_OPEN){
    opt->name = "adf-open";
    opt->title = "ADF open";
    opt->desc = "ADF cover open";
    opt->type = SANE_TYPE_BOOL;
    opt->unit = SANE_UNIT_NONE;
    if (s->has_cmd_hw_status || s->ghs_in_rs)
      opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
    else 
      opt->cap = SANE_CAP_INACTIVE;
  }

  if(option==OPT_SLEEP){
    opt->name = "power-save";
    opt->title = "power saving";
    opt->desc = "scanner in power saving mode";
    opt->type = SANE_TYPE_BOOL;
    opt->unit = SANE_UNIT_NONE;
    if (s->has_cmd_hw_status)
      opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
    else 
      opt->cap = SANE_CAP_INACTIVE;
  }

  if(option==OPT_SEND_SW){
    opt->name = "button-send";
    opt->title = "Send to button";
    opt->desc = "button 'Send to' pressed";
    opt->type = SANE_TYPE_BOOL;
    opt->unit = SANE_UNIT_NONE;
    if (s->has_cmd_hw_status || s->ghs_in_rs)
      opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
    else 
      opt->cap = SANE_CAP_INACTIVE;
  }

  if(option==OPT_MANUAL_FEED){
    opt->name = "manual-feed";
    opt->title = "manual feed";
    opt->desc = "manual feed selected";
    opt->type = SANE_TYPE_BOOL;
    opt->unit = SANE_UNIT_NONE;
    if (s->has_cmd_hw_status)
      opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
    else 
      opt->cap = SANE_CAP_INACTIVE;
  }

  if(option==OPT_SCAN_SW){
    opt->name = "button-scan";
    opt->title = "Scan button";
    opt->desc = "button 'Scan' pressed";
    opt->type = SANE_TYPE_BOOL;
    opt->unit = SANE_UNIT_NONE;
    if (s->has_cmd_hw_status || s->ghs_in_rs)
      opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
    else 
      opt->cap = SANE_CAP_INACTIVE;
  }

  if(option==OPT_FUNCTION){
    opt->name = "function";
    opt->title = "function";
    opt->desc = "function character on lcd screen";
    opt->type = SANE_TYPE_INT;
    opt->unit = SANE_UNIT_NONE;
    if (s->has_cmd_hw_status || s->ghs_in_rs)
      opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
    else 
      opt->cap = SANE_CAP_INACTIVE;
  }

  if(option==OPT_INK_EMPTY){
    opt->name = "ink-empty";
    opt->title = "ink empty";
    opt->desc = "imprinter ink running low";
    opt->type = SANE_TYPE_BOOL;
    opt->unit = SANE_UNIT_NONE;
    if (s->has_cmd_hw_status)
      opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
    else 
      opt->cap = SANE_CAP_INACTIVE;
  }

  if(option==OPT_DOUBLE_FEED){
    opt->name = "double-feed";
    opt->title = "double feed";
    opt->desc = "double feed detected";
    opt->type = SANE_TYPE_BOOL;
    opt->unit = SANE_UNIT_NONE;
    if (s->has_cmd_hw_status || s->ghs_in_rs)
      opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
    else 
      opt->cap = SANE_CAP_INACTIVE;
  }

  if(option==OPT_ERROR_CODE){
    opt->name = "error-code";
    opt->title = "error code";
    opt->desc = "hardware error code";
    opt->type = SANE_TYPE_INT;
    opt->unit = SANE_UNIT_NONE;
    if (s->has_cmd_hw_status)
      opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
    else 
      opt->cap = SANE_CAP_INACTIVE;
  }

  if(option==OPT_SKEW_ANGLE){
    opt->name = "skew-angle";
    opt->title = "skew angle";
    opt->desc = "requires black background for scanning";
    opt->type = SANE_TYPE_INT;
    opt->unit = SANE_UNIT_NONE;
    if (s->has_cmd_hw_status)
      opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
    else 
      opt->cap = SANE_CAP_INACTIVE;
  }

  if(option==OPT_INK_REMAIN){
    opt->name = "ink-remain";
    opt->title = "ink remaining";
    opt->desc = "imprinter ink level";
    opt->type = SANE_TYPE_INT;
    opt->unit = SANE_UNIT_NONE;
    if (s->has_cmd_hw_status)
      opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
    else 
      opt->cap = SANE_CAP_INACTIVE;
  }

  if(option==OPT_DENSITY_SW){
    opt->name = "density";
    opt->title = "density";
    opt->desc = "density dial";
    opt->type = SANE_TYPE_INT;
    opt->unit = SANE_UNIT_NONE;
    if (s->ghs_in_rs)
      opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
    else 
      opt->cap = SANE_CAP_INACTIVE;
  }

  if(option==OPT_DUPLEX_SW){
    opt->name = "button-duplex";
    opt->title = "duplex switch";
    opt->desc = "duplex switch";
    opt->type = SANE_TYPE_BOOL;
    opt->unit = SANE_UNIT_NONE;
    if (s->ghs_in_rs)
      opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
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
            strcpy (val, source_Flatbed);
          }
          else if(s->source == SOURCE_ADF_FRONT){
            strcpy (val, source_ADFFront);
          }
          else if(s->source == SOURCE_ADF_BACK){
            strcpy (val, source_ADFBack);
          }
          else if(s->source == SOURCE_ADF_DUPLEX){
            strcpy (val, source_ADFDuplex);
          }
          else{
            DBG(5,"missing option val for source\n"); 
          }
          return SANE_STATUS_GOOD;

        case OPT_MODE:
          if(s->mode == MODE_LINEART){
            strcpy (val, mode_Lineart);
          }
          else if(s->mode == MODE_HALFTONE){
            strcpy (val, mode_Halftone);
          }
          else if(s->mode == MODE_GRAYSCALE){
            strcpy (val, mode_Grayscale);
          }
          else if(s->mode == MODE_COLOR_LINEART){
            strcpy (val, mode_Color_lineart);
          }
          else if(s->mode == MODE_COLOR_HALFTONE){
            strcpy (val, mode_Color_halftone);
          }
          else if(s->mode == MODE_COLOR){
            strcpy (val, mode_Color);
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

        case OPT_THRESHOLD:
          *val_p = s->threshold;
          return SANE_STATUS_GOOD;

        case OPT_CONTRAST:
          *val_p = s->contrast;
          return SANE_STATUS_GOOD;

        case OPT_RIF:
          *val_p = s->rif;
          return SANE_STATUS_GOOD;

        /* Advanced Group */
        case OPT_DROPOUT_COLOR:
          switch (s->dropout_color) {
            case COLOR_DEFAULT:
              strcpy (val, color_Default);
              break;
            case COLOR_RED:
              strcpy (val, color_Red);
              break;
            case COLOR_GREEN:
              strcpy (val, color_Green);
              break;
            case COLOR_BLUE:
              strcpy (val, color_Blue);
              break;
          }
          return SANE_STATUS_GOOD;

        case OPT_SLEEP_TIME:
          *val_p = s->sleep_time;
          return SANE_STATUS_GOOD;

        case OPT_DUPLEX_OFFSET:
          *val_p = s->duplex_offset;
          return SANE_STATUS_GOOD;

        case OPT_BLUE_OFFSET:
          *val_p = s->blue_offset;
          return SANE_STATUS_GOOD;

        case OPT_GREEN_OFFSET:
          *val_p = s->green_offset;
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
          if (!strcmp (val, source_ADFFront)) {
            tmp = SOURCE_ADF_FRONT;
          }
          else if (!strcmp (val, source_ADFBack)) {
            tmp = SOURCE_ADF_BACK;
          }
          else if (!strcmp (val, source_ADFDuplex)) {
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
          if (!strcmp (val, mode_Lineart)) {
            tmp = MODE_LINEART;
          }
          else if (!strcmp (val, mode_Halftone)) {
            tmp = MODE_HALFTONE;
          }
          else if (!strcmp (val, mode_Grayscale)) {
            tmp = MODE_GRAYSCALE;
          }
          else if (!strcmp (val, mode_Color_lineart)) {
            tmp = MODE_COLOR_LINEART;
          }
          else if (!strcmp (val, mode_Color_halftone)) {
            tmp = MODE_COLOR_HALFTONE;
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
          *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
          return SANE_STATUS_GOOD;

        case OPT_PAGE_HEIGHT:
          if (s->page_height == FIXED_MM_TO_SCANNER_UNIT(val_c))
              return SANE_STATUS_GOOD;

          s->page_height = FIXED_MM_TO_SCANNER_UNIT(val_c);
          *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
          return SANE_STATUS_GOOD;

        /* Enhancement Group */
        case OPT_BRIGHTNESS:
          s->brightness = val_c;
          return SANE_STATUS_GOOD;

        case OPT_THRESHOLD:
          s->threshold = val_c;
          return SANE_STATUS_GOOD;

        case OPT_CONTRAST:
          s->contrast = val_c;
          return SANE_STATUS_GOOD;

        case OPT_RIF:
          s->rif = val_c;
          return SANE_STATUS_GOOD;

        /* Advanced Group */
        case OPT_DROPOUT_COLOR:
          if (!strcmp(val, color_Default))
            s->dropout_color = COLOR_DEFAULT;
          else if (!strcmp(val, color_Red))
            s->dropout_color = COLOR_RED;
          else if (!strcmp(val, color_Green))
            s->dropout_color = COLOR_GREEN;
          else if (!strcmp(val, color_Blue))
            s->dropout_color = COLOR_BLUE;
          if (s->has_MS_dropout)
            return mode_select_dropout(s);
          else
            return SANE_STATUS_GOOD;

        case OPT_SLEEP_TIME:
          s->sleep_time = val_c;
          return set_sleep_mode(s);

        case OPT_DUPLEX_OFFSET:
          s->duplex_offset = val_c;
          return SANE_STATUS_GOOD;

        case OPT_BLUE_OFFSET:
          s->blue_offset = val_c;
          return SANE_STATUS_GOOD;

        case OPT_GREEN_OFFSET:
          s->green_offset = val_c;
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

  set_MSEL_xfer_length (mode_selectB.cmd, mode_select_sleepB.size);
  set_MSEL_sleep_mode(mode_select_sleepB.cmd, s->sleep_time);

  ret = do_cmd (
    s, 0, 0, 1, 0,
    mode_selectB.cmd, mode_selectB.size,
    mode_select_sleepB.cmd, mode_select_sleepB.size,
    NULL, 0
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
  if (s->last_ghs + GHS_TIME < time(NULL)) {

      if (s->has_cmd_hw_status){
  
          set_HW_allocation_length (hw_statusB.cmd, 10);
        
          ret = do_cmd (
            s, 0, 0, 1, 0,
            hw_statusB.cmd, hw_statusB.size,
            NULL, 0,
            s->buffer, 10
          );
        
          if (ret == SANE_STATUS_GOOD) {
      
              s->last_ghs = time(NULL);
    
              s->hw_top = get_HW_top(s->buffer);
              s->hw_A3 = get_HW_A3(s->buffer);
              s->hw_B4 = get_HW_B4(s->buffer);
              s->hw_A4 = get_HW_A4(s->buffer);
              s->hw_B5 = get_HW_B5(s->buffer);
      
              s->hw_hopper = get_HW_hopper(s->buffer);
              s->hw_omr = get_HW_omr(s->buffer);
              s->hw_adf_open = get_HW_adf_open(s->buffer);
      
              s->hw_sleep = get_HW_sleep(s->buffer);
              s->hw_send_sw = get_HW_send_sw(s->buffer);
              s->hw_manual_feed = get_HW_manual_feed(s->buffer);
              s->hw_scan_sw = get_HW_scan_sw(s->buffer);
      
              s->hw_function = get_HW_function(s->buffer);
      
              s->hw_ink_empty = get_HW_ink_empty(s->buffer);
              s->hw_double_feed = get_HW_double_feed(s->buffer);
      
              s->hw_error_code = get_HW_error_code(s->buffer);
      
              s->hw_skew_angle = get_HW_skew_angle(s->buffer);
        
              s->hw_ink_remain = get_HW_ink_remain(s->buffer);
      
          }
      }

      /* 3091/2 put hardware status in RS data */
      else if (s->ghs_in_rs){
          
          DBG(15,"get_hardware_status: calling rs\n");

          ret = do_cmd(
            s,0,0,0,0,
            request_senseB.cmd, request_senseB.size,
            NULL,0,
            (unsigned char *)s->buffer,RS_return_size
          );
    
          /* parse the rs data */
          if(ret == SANE_STATUS_GOOD && get_RS_sense_key(s->buffer)==0 && get_RS_ASC(s->buffer)==0x80){

              s->last_ghs = time(NULL);
    
              s->hw_adf_open = get_RS_adf_open(s->buffer);
              s->hw_send_sw = get_RS_send_sw(s->buffer);
              s->hw_scan_sw = get_RS_scan_sw(s->buffer);
              s->hw_duplex_sw = get_RS_duplex_sw(s->buffer);
              s->hw_top = get_RS_top(s->buffer);
              s->hw_hopper = get_RS_hopper(s->buffer);
              s->hw_function = get_RS_function(s->buffer);
              s->hw_density_sw = get_RS_density(s->buffer);
          }
          else{
            return SANE_STATUS_GOOD;
          }
      }
  }

  DBG (10, "get_hardware_status: finish\n");

  return ret;
}

static SANE_Status
mode_select_dropout (struct fujitsu *s)
{
  int ret;

  DBG (10, "mode_select_dropout: start\n");

  set_MSEL_xfer_length (mode_selectB.cmd, mode_select_dropoutB.size);
  set_MSEL_dropout_front (mode_select_dropoutB.cmd, s->dropout_color);
  set_MSEL_dropout_back (mode_select_dropoutB.cmd, s->dropout_color);
  
  ret = do_cmd (
      s, 0, 0, 1, 0,
      mode_selectB.cmd, mode_selectB.size,
      mode_select_dropoutB.cmd, mode_select_dropoutB.size,
      NULL, 0
  );

  DBG (10, "mode_select_dropout: finish\n");

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
SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters * params)
{
  struct fujitsu *s = (struct fujitsu *) handle;

  DBG (10, "sane_get_parameters: start\n");

  calculateDerivedValues (s);

  params->format = s->params.format;
  params->last_frame = s->params.last_frame;
  params->lines = s->params.lines;
  params->depth = s->params.depth;
  params->pixels_per_line = s->params.pixels_per_line;
  params->bytes_per_line = s->params.bytes_per_line;

  DBG (15, "\tdepth %d\n", params->depth);
  DBG (15, "\tlines %d\n", params->lines);
  DBG (15, "\tpixels_per_line %d\n", params->pixels_per_line);
  DBG (15, "\tbytes_per_line %d\n", params->bytes_per_line);

  DBG (10, "sane_get_parameters: finish\n");

  return SANE_STATUS_GOOD;
}

/* 
 * Computes the scanner units used for the set_window command
 */
void
calculateDerivedValues (struct fujitsu *s)
{

  DBG (10, "calculateDerivedValues: start\n");
  DBG (5, "xres=%d, tlx=%d, brx=%d, pw=%d, maxx=%d\n", s->resolution_x, s->tl_x, s->br_x, s->page_width, s->max_x);
  DBG (5, "yres=%d, tly=%d, bry=%d, ph=%d, maxy=%d\n", s->resolution_y, s->tl_y, s->br_y, s->page_height, s->max_y);

  s->params.pixels_per_line = s->resolution_x * (s->br_x - s->tl_x) / 1200;
  s->params.lines = s->resolution_y * (s->br_y - s->tl_y) / 1200;
  s->params.last_frame = 1;

  if (s->mode == MODE_COLOR) {
    s->params.format = SANE_FRAME_RGB;
    s->params.depth = 8;
    s->params.bytes_per_line = s->params.pixels_per_line * 3;
  }
  else if (s->mode == MODE_GRAYSCALE) {
    s->params.format = SANE_FRAME_GRAY;
    s->params.depth = 8;
    s->params.bytes_per_line = s->params.pixels_per_line;
  }
  else if (s->mode == MODE_LINEART || s->mode == MODE_HALFTONE) {

    /* increase scan width to full number of bytes in a scan line */
    while (s->params.pixels_per_line % 8) {
      s->br_x++;
      s->params.pixels_per_line = s->resolution_x * (s->br_x - s->tl_x) / 1200;
    }

    /* dont round up larger than scanners max width */
    while (s->br_x > s->max_x || s->params.pixels_per_line % 8){
      s->br_x--;
      s->params.pixels_per_line = s->resolution_x * (s->br_x - s->tl_x) / 1200;
    }

    s->params.format = SANE_FRAME_GRAY;
    s->params.depth = 1;
    s->params.bytes_per_line = s->params.pixels_per_line / 8;
  }
  /*FIXME what about the other modes*/
  else{
  }

  DBG (5, "xres=%d, tlx=%d, brx=%d, pw=%d, maxx=%d\n", s->resolution_x, s->tl_x, s->br_x, s->page_width, s->max_x);
  DBG (5, "yres=%d, tly=%d, bry=%d, ph=%d, maxy=%d\n", s->resolution_y, s->tl_y, s->br_y, s->page_height, s->max_y);

#if 0
  /* special relationship between X and Y must be maintained for 3096 */
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
#endif
  DBG (10, "calculateDerivedValues: finish\n");
}

/*
 * Called by SANE when a page acquisition operation is to be started.
 * commands: scanner control (lampon), send (lut), send (dither),
 * set window, object pos, and scan
 *
 * this will be called between sides of a duplex scan,
 * and at the start of each page of an adf batch.
 * hence, we spend alot of time playing with s->started and s->eof_*
 */
SANE_Status
sane_start (SANE_Handle handle)
{
  struct fujitsu *s = handle;
  SANE_Status ret;

  DBG (10, "sane_start: start\n");

  DBG (15, "started=%d, img_count=%d, source=%d\n", s->started, s->img_count, s->source);

  /* first page of batch */
  if(!s->started){

      /* set clean defaults */
      s->started=1;
      s->img_count=0;

      s->bytes_rx[0]=0;
      s->bytes_rx[1]=0;
      s->eof_rx[0]=0;
      s->eof_rx[1]=0;

      s->bytes_tx[0]=0;
      s->bytes_tx[1]=0;
      s->eof_tx[0]=0;
      s->eof_tx[1]=0;

      /* send batch setup commands */
      ret = scanner_control (s, SC_function_lamp_on);
      if (ret != SANE_STATUS_GOOD) {
        DBG (5, "sane_start: WARNING: cannot start lamp\n");
        do_cancel(s);
        return ret;
      }
    
      /*FIXME: send lut and dither ?*/
    
      calculateDerivedValues(s);
    
      /* now that we know the number of bytes, make a temp file
       * or a large buffer to hold the back side image */
      if (s->source == SOURCE_ADF_DUPLEX) {
        ret = setup_duplex_buffer(s);
        if (ret != SANE_STATUS_GOOD) {
          DBG (5, "sane_start: WARNING: cannot load duplex buffer\n");
          do_cancel(s);
          return ret;
        }
      }

      ret = set_window(s);
      if (ret != SANE_STATUS_GOOD) {
        DBG (5, "sane_start: ERROR: failed to set window\n");
        do_cancel(s);
        return ret;
      }

      ret = object_position (s, SANE_TRUE);
      if (ret != SANE_STATUS_GOOD) {
        DBG (5, "sane_start: WARNING: cannot load page\n");
        do_cancel(s);
        return ret;
      }

      ret = start_scan (s);
      if (ret != SANE_STATUS_GOOD) {
        DBG (5, "sane_start: start_scan failed");
        do_cancel(s);
        return ret;
      }
  }

  /* already in a batch */
  else {
    int side = get_current_side(s);

    /* not finished with current side, error */
    if (!s->eof_rx[side] || !s->eof_tx[side]) {
      DBG(5,"sane_start: previous transfer not finished?");
      return do_cancel(s);
    }

    /* Finished with previous img, jump to next */
    s->img_count++;
    side = get_current_side(s);

    /* dont reset the transfer vars on backside of duplex page */
    /* otherwise buffered back page will be lost */
    /* dont call object pos or scan on back side of duplex scan */
    if ( !(s->source == SOURCE_ADF_DUPLEX && side == SIDE_BACK) ) {
      s->bytes_rx[0]=0;
      s->bytes_rx[1]=0;
      s->eof_rx[0]=0;
      s->eof_rx[1]=0;

      s->bytes_tx[0]=0;
      s->bytes_tx[1]=0;
      s->eof_tx[0]=0;
      s->eof_tx[1]=0;

      ret = object_position (s, SANE_TRUE);
      if (ret != SANE_STATUS_GOOD) {
        DBG (5, "sane_start: WARNING: cannot load page\n");
        do_cancel(s);
        return ret;
      }

      ret = start_scan (s);
      if (ret != SANE_STATUS_GOOD) {
        DBG (5, "sane_start: start_scan failed");
        do_cancel(s);
        return ret;
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
  int ret;

  DBG (10, "scanner_control: start\n");

  set_SC_function (scanner_controlB.cmd, function);

  /* notice extremely long retry period */
  ret = do_cmd (
    s, 30, 10, 1, 0,
    scanner_controlB.cmd, scanner_controlB.size,
    NULL, 0,
    NULL, 0
  );

  DBG (10, "scanner_control: finish\n");

  return ret;
}

/*
 * Creates a temporary file, opens it, and returns a file pointer for it.
 * OR, callocs a buffer to hold the backside scan
 * 
 * Will only create a file that
 * doesn't exist already. The function will also unlink ("delete") the file
 * immediately after it is created. In any "sane" programming environment this
 * has the effect that the file can be used for reading and writing as normal
 * but vanishes as soon as it's closed - so no cleanup required if the 
 * process dies etc.
 */
static SANE_Status
setup_duplex_buffer (struct fujitsu *s)
{
  SANE_Status ret = SANE_STATUS_GOOD;

  DBG (10, "setup_duplex_buffer: start\n");

  /* close old file */
  if (s->duplex_fd != -1) {
    DBG (15, "setup_duplex_buffer: closing old tempfile.\n");
    if( (close(s->duplex_fd)) != 0){
      DBG (5, "setup_duplex_buffer: attempt to close tempfile returned %d.\n", errno);
    }
    s->duplex_fd = -1;
  }

  /* free old mem */
  if (s->duplex_buffer) {
    DBG (15, "setup_duplex_buffer: free old mem.\n");
    free(s->duplex_buffer);
    s->duplex_buffer = NULL;
  }

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

  /* big mem buffer */
  else {

    s->duplex_buffer = calloc (1,s->params.bytes_per_line * s->params.lines);
    if (!s->duplex_buffer) {
      DBG (5, "ERROR: could not create duplex buffer.\n");
      return SANE_STATUS_NO_MEM;
    }
  }

  DBG (10, "setup_duplex_buffer: finish\n");

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
  set_WD_length (window_descriptor_blockB.cmd, s->br_y - s->tl_y);

  set_WD_brightness (window_descriptor_blockB.cmd, s->brightness);
  set_WD_threshold (window_descriptor_blockB.cmd, s->threshold);
  set_WD_contrast (window_descriptor_blockB.cmd, s->contrast);
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

  set_WD_vendor_id_code (window_descriptor_blockB.cmd, s->window_vid);
  set_WD_gamma (window_descriptor_blockB.cmd, s->gamma);

  set_WD_paper_selection (window_descriptor_blockB.cmd, WD_paper_SEL_NON_STANDARD);
  set_WD_paper_width_X (window_descriptor_blockB.cmd, s->page_width);
  set_WD_paper_length_Y (window_descriptor_blockB.cmd, s->page_height);

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
    s, 3, 1, 1, 0,
    set_windowB.cmd, set_windowB.size,
    buffer, bufferLen,
    NULL, 0
  );

  DBG (10, "set_window: finish\n");

  return ret;
}

/*
 * Issues the SCSI OBJECT POSITION command if an ADF is installed.
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
    s, 0, 0, 1, 0,
    object_positionB.cmd, object_positionB.size,
    NULL, 0,
    NULL, 0
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
    s, 0, 0, 1, 0,
    scanB.cmd, scanB.size,
    (unsigned char *)&outBuff, outLen,
    NULL, 0
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
  SANE_Status ret=0, ret2=0;
  struct fujitsu *s = (struct fujitsu *) handle;
  int i, len2=0, side;

  DBG (10, "sane_read: start\n");

  *len=0;

  /* maybe cancelled? */
  if(!s->started){
      DBG (5, "sane_read: not started, call sane_start\n");
      return SANE_STATUS_CANCELLED;
  }

  side = get_current_side(s);

  /* sane_start required between sides */
  if(s->eof_rx[side] && s->eof_tx[side]){
      DBG (15, "sane_read: returning eof\n");
      return SANE_STATUS_EOF;
  }

  /* any front sides or simplex back sides are simply
   * read from device into frontends buffer */
  if( side == SIDE_FRONT || s->source == SOURCE_ADF_BACK ){
    ret = read_from_scanner(s,buf,max_len,len,side);
  }

  /* duplex front or back should also buffer a bit of back */
  if(s->source == SOURCE_ADF_DUPLEX){

    /* fill back side buffer */
    if(!s->eof_rx[SIDE_BACK]){
      ret2 = read_from_scanner(s, s->duplex_buffer+s->bytes_rx[SIDE_BACK], s->buffer_size, &len2, SIDE_BACK);
    }

    /* we are looking at back side, copy buffer */
    if( side == SIDE_BACK ){
      ret = read_from_buffer(s,buf,max_len,len,side);
    }
  }

  /* we are about to transmit last bytes for this side */
  /* but sane standard says status_eof be returned */
  /* only with 0 bytes, so we return that next pass */
  if(ret == SANE_STATUS_EOF){
    s->eof_tx[side] = 1;
    ret = SANE_STATUS_GOOD;
  }

  /* scanners interlace colors in many different ways */
  /* use separate functions to convert to regular rgb */
  if(s->mode == MODE_COLOR){
    switch (s->color_interlace) {
        case COLOR_INTERLACE_RRGGBB:
          convert_rrggbb_to_rgb(s,buf,*len);
          break;
        case COLOR_INTERLACE_BGR:
          convert_bgr_to_rgb(s,buf,*len);
          break;
        case COLOR_INTERLACE_3091:
          convert_3091rgb_to_rgb(s,buf,*len);
          break;
        case COLOR_INTERLACE_NONE:
          memcpy(buf,s->buffer,*len);
          break;
        default:
          DBG (5, "sane_read: cant convert buffer: %d\n",s->color_interlace);
          memcpy(buf,s->buffer,*len);
    }
  }

  /*if (s->compress_type == WD_cmp_NONE && */

  /* invert image if scanner needs it for this mode */
  if (s->reverse_by_mode[s->mode]){
      for ( i = 0; i < *len; i++ ) {
          buf[i] ^= 0xff;
      }
  }

  DBG (10, "sane_read: finish\n");

  return ret;
}

static SANE_Status
read_from_scanner(struct fujitsu *s, SANE_Byte * buf, SANE_Int max_len, SANE_Int * len, int side)
{
  SANE_Status ret=SANE_STATUS_GOOD;
  int bytes = s->buffer_size;
  int remain = (s->params.bytes_per_line * s->params.lines) - s->bytes_rx[side];

  DBG (10, "read_from_scanner: start\n");

  /* figure out the max amount to transfer */
  if(bytes > max_len){
    bytes = max_len;
  }
  if(bytes > remain){
    bytes = remain;
  }

  /* all requests must end on line boundary */
  bytes -= (bytes % s->params.bytes_per_line);

  /* this should never happen */
  if(bytes < 1){
    DBG(5, "read_from_scanner: ERROR side:%d want:%d room:%d/%d doing:%d done:%d\n", side, remain, max_len, s->buffer_size, bytes, s->bytes_rx[side]);
    return SANE_STATUS_INVAL;
  }

  DBG(15, "read_from_scanner: side:%d want:%d room:%d/%d doing:%d done:%d\n", side, remain, max_len, s->buffer_size, bytes, s->bytes_rx[side]);

  set_R_datatype_code (readB.cmd, R_datatype_imagedata);

  if (side == SIDE_BACK) {
      set_R_window_id (readB.cmd, WD_wid_back);
  }
  else{
      set_R_window_id (readB.cmd, WD_wid_front);
  }

  set_R_xfer_length (readB.cmd, bytes);

  ret = do_cmd (
    s, 15, 10, 1, 0,
    readB.cmd, readB.size,
    NULL, 0,
    buf, bytes
  );

  if (ret == SANE_STATUS_GOOD) {
    DBG(15, "read_from_scanner: got GOOD\n");
    *len = bytes;

    if (bytes == remain) {
      DBG(15, "read_from_scanner: done\n");
      ret = SANE_STATUS_EOF;
    }
  }
  /* FIXME get the real number of bytes */
  else if (ret == SANE_STATUS_EOF) {
    DBG(5, "read_from_scanner: got EOF\n");
    *len = bytes;
  }
  else {
    DBG(5, "read_from_scanner: error reading data block status = %d\n", ret);
  }

  s->bytes_rx[side] += *len;

  if (ret == SANE_STATUS_EOF) {
    s->eof_rx[side] = 1;
  }

  DBG (10, "read_from_scanner: finish\n");

  return ret;
}

static SANE_Status
read_from_buffer(struct fujitsu *s, SANE_Byte * buf, SANE_Int max_len, SANE_Int * len, int side)
{
  SANE_Status ret=SANE_STATUS_GOOD;
  int bytes = max_len;
  int remain = s->bytes_rx[side] - s->bytes_tx[side];

  DBG (10, "read_from_buffer: start\n");

  /* figure out the max amount to transfer */
  if(bytes > remain){
    bytes = remain;
  }

  /* all requests must end on line boundary */
  bytes -= (bytes % s->params.bytes_per_line);

  /* this should never happen */
  if(bytes < 1){
    DBG(5, "read_from_buffer: ERROR side:%d want:%d room:%d/%d doing:%d done:%d\n", side, remain, max_len, s->buffer_size, bytes, s->bytes_tx[side]);
    return SANE_STATUS_INVAL;
  }

  DBG(15, "read_from_buffer: side:%d want:%d room:%d/%d doing:%d done:%d\n", side, remain, max_len, s->buffer_size, bytes, s->bytes_tx[side]);

  memcpy(buf,s->duplex_buffer+s->bytes_tx[side],bytes);
  s->bytes_tx[side] += bytes;
  *len = bytes;

  if(s->eof_rx[side] && s->bytes_tx[side] == s->bytes_rx[side]){
    ret = SANE_STATUS_EOF;
  }

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
      free (dev->buffer);
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
  unsigned int sense = get_RS_sense_key (sensed_data);
  unsigned int asc = get_RS_ASC (sensed_data);
  unsigned int ascq = get_RS_ASCQ (sensed_data);
  struct fujitsu * s = arg;

  DBG (5, "sense_handler: start\n");

  /* kill compiler warning */
  fd = fd;

  /* copy the rs return data into the scanner struct
     so that the caller can use it if he wants */
  memcpy(&s->rs_buffer,sensed_data,RS_return_size);

  DBG (5, "Sense=%#02x, ASC=%#02x, ASCQ=%#02x\n", sense, asc, ascq);

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
      if (get_RS_EOM (sensed_data)) {
        /*s->i_transfer_length = get_RS_information (sensed_data);*/
        DBG  (5, "No sense: EOM\n");
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
        if (sensed_data[7] >= 0x0a) {
	  DBG (5, "Offending byte is %#02x. (Byte %#02x in window descriptor block)\n", 
	    get_RS_offending_byte(sensed_data), get_RS_offending_byte(sensed_data)-8);
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
 * if the status byte/rs data says 'busy' then we try again
 * we also dump lots of debug info to stderr
 */
static SANE_Status
do_cmd(struct fujitsu *s, int busyRetry, int busySleep, int runRS, int shortTime,
 unsigned char * cmdBuff, size_t cmdLen,
 unsigned char * outBuff, size_t outLen,
 unsigned char * inBuff, size_t inLen
)
{
    if (s->connection == CONNECTION_SCSI) {
        return do_scsi_cmd(s, busyRetry, busySleep, runRS, shortTime,
                 cmdBuff, cmdLen,
                 outBuff, outLen,
                 inBuff, inLen);
    }
    if (s->connection == CONNECTION_USB) {
        return do_usb_cmd(s, busyRetry, busySleep, runRS, shortTime,
                 cmdBuff, cmdLen,
                 outBuff, outLen,
                 inBuff, inLen);
    }
    return SANE_STATUS_INVAL;
}

SANE_Status
do_scsi_cmd(struct fujitsu *s, int busyRetry, int busySleep, int runRS, int shortTime,
 unsigned char * cmdBuff, size_t cmdLen,
 unsigned char * outBuff, size_t outLen,
 unsigned char * inBuff, size_t inLen
)
{
  int ret;
  size_t actLen = inLen;

  /*shut up compiler*/
  runRS=runRS;
  shortTime=shortTime;

  DBG(10, "do_scsi_cmd: start\n");

  hexdump(30, "cmd >>", cmdBuff, cmdLen);

  if(outBuff && outLen){
    hexdump(30, "out >>", outBuff, outLen);
  }
  if (inBuff && inLen){
    DBG(30, "in << want %lu bytes\n", (long unsigned int)inLen);
    memset(inBuff,0,inLen);
  }

  ret = sanei_scsi_cmd2 (s->fd, cmdBuff, cmdLen, outBuff, outLen, inBuff, &actLen);
  DBG(30, "do_scsi_cmd: sanei returned %d\n",ret);

  /* FIXME: what about 0 length reads?
  if(ret == SANE_STATUS_EOF){
    ret = SANE_STATUS_DEVICE_BUSY;
  }*/

  if(ret == SANE_STATUS_DEVICE_BUSY){
    if(busyRetry){
        DBG (30, "do_scsi_cmd: retrying\n");
        usleep(busySleep*100000);
        return do_cmd(s,busyRetry-1,busySleep,runRS,shortTime,
          cmdBuff,cmdLen,
          outBuff,outLen,
          inBuff,inLen
        );
    }

    /* out of retries, still busy? */
    DBG (30, "do_scsi_cmd: out of retries, but still busy\n");
    return SANE_STATUS_DEVICE_BUSY;
  }

  if (inBuff && inLen){
    DBG(30, "in << got %lu bytes\n", (long unsigned int)actLen);

    if (inLen != actLen) {
      DBG(30,"wrong size!\n");
    }

    hexdump(30, "in <<", inBuff, actLen);
  }

  DBG(10, "do_scsi_cmd: returning %d\n", ret);

  return ret;
}

SANE_Status
do_usb_cmd(struct fujitsu *s, int busyRetry, int busySleep, int runRS, int shortTime,
 unsigned char * cmdBuff, size_t cmdLen,
 unsigned char * outBuff, size_t outLen,
 unsigned char * inBuff, size_t inLen
)
{
    unsigned char usb_cmdBuff[USB_COMMAND_LEN];
    unsigned char usb_statBuff[USB_STATUS_LEN];
    unsigned char rsBuff[RS_return_size];
    size_t usb_cmdLen = USB_COMMAND_LEN;
    size_t usb_outLen = outLen;
    size_t usb_inLen = inLen;
    size_t usb_statLen = USB_STATUS_LEN;
    int cmdRetVal = 0;
    int outRetVal = 0;
    int inRetVal = 0;
    int statRetVal = 0;
    int rsRetVal = 0;
    SANE_Status ret = SANE_STATUS_GOOD;

    int cmdTime = USB_COMMAND_TIME;
    int outTime = USB_DATA_TIME;
    int inTime = USB_DATA_TIME;
    int statTime = USB_STATUS_TIME;

    DBG (10, "do_usb_cmd: start\n");

    if(shortTime){
        cmdTime = USB_COMMAND_TIME/20;
        outTime = USB_DATA_TIME/20;
        inTime = USB_DATA_TIME/20;
        statTime = USB_STATUS_TIME/20;
    }

    hexdump(30, "cmd >>", cmdBuff, cmdLen);

    /* build a USB packet around the SCSI command */
    memset(&usb_cmdBuff,0,USB_COMMAND_LEN);
    usb_cmdBuff[0] = USB_COMMAND_CODE;
    memcpy(&usb_cmdBuff[USB_COMMAND_OFFSET],cmdBuff,cmdLen);

    /* sanei_usb timeout is way too long */
    sanei_usb_set_timeout(cmdTime);

    /* write the command out */
    hexdump(30, "usb_cmd >>", (unsigned char *)&usb_cmdBuff, USB_COMMAND_LEN);
    DBG(30, "writing %u bytes\n", (unsigned int) usb_cmdLen);
    cmdRetVal = sanei_usb_write_bulk(s->fd, (unsigned char *)&usb_cmdBuff, &usb_cmdLen);
    DBG(30, "wrote %u bytes\n", (unsigned int) usb_cmdLen);
    DBG(30,"cmdRetVal: %d\n",cmdRetVal);

    if(cmdRetVal != SANE_STATUS_GOOD){
        DBG(30,"ERROR!\n");
        return cmdRetVal;
    }
    if(usb_cmdLen != USB_COMMAND_LEN){
        DBG(30,"wrong size!\n");
        return SANE_STATUS_IO_ERROR;
    }

    /* this command has a write component, and a place to get it */
    if(outBuff && outLen && outTime){

        /* sanei_usb timeout is way too long */
        sanei_usb_set_timeout(outTime);

        hexdump(30, "out >>", outBuff, outLen);
        DBG(30, "writing %u bytes\n", (unsigned int) outLen);
        outRetVal = sanei_usb_write_bulk(s->fd, outBuff, &usb_outLen);
        DBG(30, "wrote %u bytes\n", (unsigned int) usb_outLen);
        DBG(30,"outRetVal: %d\n",outRetVal);
    
        if(outRetVal != SANE_STATUS_GOOD){
            DBG(30,"ERROR!\n");
            return outRetVal;
        }
        if(usb_outLen != outLen){
            DBG(30,"wrong size!\n");
            return SANE_STATUS_IO_ERROR;
        }
    }

    /* this command has a read component, and a place to put it */
    if(inBuff && inLen && inTime){

        memset(inBuff,0,inLen);

        /* sanei_usb timeout is way too long */
        sanei_usb_set_timeout(inTime);

        DBG(30, "reading %u bytes\n", (unsigned int) inLen);
        inRetVal = sanei_usb_read_bulk(s->fd, inBuff, &usb_inLen);
        DBG(30, "read %u bytes\n", (unsigned int) usb_inLen);
        hexdump(30, "in <<", inBuff, usb_inLen);
        DBG(30,"inRetVal: %d\n",inRetVal);
    
        if(inRetVal == SANE_STATUS_EOF){
            ret = SANE_STATUS_DEVICE_BUSY;
        }

        else if(inRetVal != SANE_STATUS_GOOD){
            DBG(30,"ERROR!\n");
            return inRetVal;
        }

        else if(usb_inLen != inLen){
            DBG(30,"wrong size!\n");
            return SANE_STATUS_IO_ERROR;
        }
    }

    memset(&usb_statBuff,0,USB_STATUS_LEN);

    /* sanei_usb timeout is way too long */
    sanei_usb_set_timeout(statTime);

    DBG(30, "reading %u bytes\n", (unsigned int) USB_STATUS_LEN);
    statRetVal = sanei_usb_read_bulk(s->fd, (unsigned char *)&usb_statBuff, &usb_statLen);
    DBG(30, "read %u bytes\n", (unsigned int) usb_statLen);
    hexdump(30, "stat <<", usb_statBuff, usb_statLen);
    DBG(30,"statRetVal: %d\n",statRetVal);

    if(statRetVal != SANE_STATUS_GOOD){
        DBG(30,"ERROR!\n");
        return statRetVal;
    }
    if(usb_statLen != USB_STATUS_LEN){
        DBG(30,"wrong size!\n");
        return SANE_STATUS_IO_ERROR;
    }

    /* if there is status >0, figure out why, and store in ret */
    if(usb_statBuff[USB_STATUS_OFFSET] > 0){

      /* busy status */
      if(usb_statBuff[USB_STATUS_OFFSET] == 8){
        ret = SANE_STATUS_DEVICE_BUSY;
      }

      /* if caller is interested in having RS run on errors, */
      /* run RS for every error status other than busy */
      else if(runRS){
  
        DBG(30,"rs sub call >>\n");
        rsRetVal = do_cmd(
          s,0,0,0,0,
          request_senseB.cmd, request_senseB.size,
          NULL,0,
          (unsigned char *)&rsBuff,RS_return_size
        );
        DBG(30,"rs sub call <<\n");
  
        if(rsRetVal != SANE_STATUS_GOOD){
          return rsRetVal;
        }

        /* parse the rs data */
        ret = sense_handler( 0, (unsigned char *)&rsBuff, (void *)s );
      }
      else{
        DBG(30,"Not calling rs!\n");
        ret = SANE_STATUS_IO_ERROR;
      }
    }

    /* if device is busy, retry if requested.
     * notice that this will overwrite the in buffer. */
    if(ret == SANE_STATUS_DEVICE_BUSY){

      if(busyRetry){
        DBG (30, "do_usb_cmd: retrying\n");
        usleep(busySleep*100000);
        return do_cmd(s,busyRetry-1,busySleep,runRS,shortTime,
          cmdBuff,cmdLen,
          outBuff,outLen,
          inBuff,inLen
        );
      }

      /* out of retries, still busy? */
      DBG (30, "do_usb_cmd: out of retries, but still busy\n");
      return SANE_STATUS_DEVICE_BUSY;
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
    s, 0, 0, 0, 1,
    test_unit_readyB.cmd, test_unit_readyB.size,
    NULL, 0,
    NULL, 0
  );
  
  if (ret != SANE_STATUS_GOOD) {
    DBG(5,"WARNING: Brain-dead scanner. Hitting with stick\n");
    ret = do_cmd (
      s, 0, 0, 0, 1,
      test_unit_readyB.cmd, test_unit_readyB.size,
      NULL, 0,
      NULL, 0
    );
  }
  if (ret != SANE_STATUS_GOOD) {
    DBG(5,"WARNING: Brain-dead scanner. Hitting with stick again\n");
    ret = do_cmd (
      s, 0, 0, 0, 1,
      test_unit_readyB.cmd, test_unit_readyB.size,
      NULL, 0,
      NULL, 0
    );
  }

  if (ret != SANE_STATUS_GOOD) {
    DBG (5, "wait_scanner: error '%s'\n", sane_strstatus (ret));
  }

  DBG (10, "wait_scanner: finish\n");

  return ret;
}

/* one line has the following format:
 * rrr...rrrggg...gggbbb...bbbb 
 * ^-r      ^-g      ^-b
 * r/g/b pointers all slide right together
 */
static SANE_Status
convert_rrggbb_to_rgb(struct fujitsu *s, unsigned char * buff, int length)
{
    int i, j, k;
    int bytes_per_line = s->params.bytes_per_line;
    int pixels_per_line = s->params.pixels_per_line;
    unsigned char * tmp;

    /* make a buffer big enough for one line */
    if(!(tmp=malloc(bytes_per_line))){
      return SANE_STATUS_NO_MEM;
    }

    /* only byteswap for full lines*/
    for (i=0; i < length/bytes_per_line; i++){

      for (j=0; j < pixels_per_line; j++){
        for (k=0; k < 3; k++){
          *(tmp+j+k) = *(buff+(i*bytes_per_line)+(k*pixels_per_line)+j); 
        }
      }

      memcpy ( buff+(i*bytes_per_line), tmp, bytes_per_line );

    }

    free(tmp);

    return SANE_STATUS_GOOD;
}

/* scanner returns pixel data as bgrbgr
 * turn each pixel around to rgbrgb */
static SANE_Status
convert_bgr_to_rgb(struct fujitsu *s, unsigned char * buff, int length)
{

    int tmp, i;
 
    /* silence compiler */ 
    s=s;

    /* only byteswap full pixels*/
    for (i=0; i < length/3; i++){
        tmp = buff[i*3];         /* buffer b */
        buff[i*3] = buff[i*3+2]; /* write r */
        buff[i*3+2] = tmp;       /* write b */
    }

    return SANE_STATUS_GOOD;
}

/* scanner returns pixel data as foo
 * turn each pixel around to rgbrgb */
static SANE_Status
convert_3091rgb_to_rgb(struct fujitsu *s, unsigned char * buff, int length)
{

    /* silence compiler */ 
    s=s;
    buff=buff;
    length=length;

    return SANE_STATUS_GOOD;
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
