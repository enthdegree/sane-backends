/* sane - Scanner Access Now Easy.

   This file is part of the SANE package, and implements a SANE backend
   for various Canon DR-series scanners.

   Copyright (C) 2008 m. allan noah

   Development funded by Corcaribe Tecnología C.A. www.cc.com.ve
   and by EvriChart, Inc. www.evrichart.com

   --------------------------------------------------------------------------

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

   The source code is divided in sections which you can easily find by
   searching for the tag "@@".

   Section 1 - Init & static stuff
   Section 2 - sane_init, _get_devices, _open & friends
   Section 3 - sane_*_option functions
   Section 4 - sane_start, _get_param, _read & friends
   Section 5 - sane_close functions
   Section 6 - misc functions

   Changes:
      v1 2008-10-29, MAN
         - initial version
      v2 2008-11-04, MAN
         - round scanlines to even bytes
	 - spin RS and usb_clear_halt code into new function
	 - update various scsi payloads
	 - calloc out block so it gets set to 0 initially
      v3 2008-11-07, MAN
         - back window uses id 1
         - add option and functions to read/send page counter
         - add rif option
      v4 2008-11-11, MAN
         - eject document when sane_read() returns EOF
      v5 2008-11-25, MAN
         - remove EOF ejection code
         - add SSM and GSM commands
         - add dropout, doublefeed, and jpeg compression options
         - disable adf backside
         - fix adf duplex
         - read two extra lines (ignore errors) at end of image
         - only send scan command at beginning of batch
	 - fix bug in hexdump with 0 length string
         - DR-7580 support
      v6 2008-11-29, MAN
         - fix adf simplex
         - rename ssm_duplex to ssm_buffer
         - add --buffer option
         - reduce inter-page commands when buffering is enabled
         - improve sense_handler output
         - enable counter option
         - drop unused code
      v7 2008-11-29, MAN
         - jpeg support (size rounding and header overwrite)
         - call object_position(load) between pages even if buffering is on
         - use request sense info bytes on short scsi reads
         - byte swap color BGR to RGB
         - round image width down, not up
         - round image height down to even # of lines
         - always transfer even # of lines per block
         - scsi and jpeg don't require reading extra lines to reach EOF
         - rename buffer option to buffermode to avoid conflict with scanimage
         - send ssm_do and ssm_df during sane_start
         - improve sense_handler output
      v8 2008-12-07, MAN
         - rename read/send_counter to read/send_panel
         - enable control panel during init
         - add options for all buttons
         - call TUR twice in wait_scanner(), even if first succeeds
         - disable rif
         - enable brightness/contrast/threshold options
      v9 2008-12-07, MAN
         - add rollerdeskew and stapledetect options
         - add rollerdeskew and stapledetect bits to ssm_df()
      v10 2008-12-10, MAN
         - add all documented request sense codes to sense_handler()
         - fix color jpeg (remove unneeded BGR to RGB swapping code)
         - add macros for LUT data
      v11 2009-01-10, MAN
         - send_panel() can disable too
         - add cancel() to send d8 command
         - call cancel() only after final read from scanner
         - stop button reqests cancel
      v12 2009-01-21, MAN
         - dont export private symbols

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

#include "canon_dr-cmd.h"
#include "canon_dr.h"

#define DEBUG 1
#define BUILD 12

/* values for SANE_DEBUG_CANON_DR env var:
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

static const char string_Dither[] = "Dither";
static const char string_Diffusion[] = "Diffusion";

static const char string_Red[] = "Red";
static const char string_Green[] = "Green";
static const char string_Blue[] = "Blue";
static const char string_En_Red[] = "Enhance Red";
static const char string_En_Green[] = "Enhance Green";
static const char string_En_Blue[] = "Enhance Blue";

static const char string_White[] = "White";
static const char string_Black[] = "Black";

static const char string_None[] = "None";
static const char string_JPEG[] = "JPEG";

static const char string_Front[] = "Front";
static const char string_Back[] = "Back";

/* Also set via config file. */
static int global_buffer_size = 64 * 1024;

/*
 * used by attach* and sane_get_devices
 * a ptr to a null term array of ptrs to SANE_Device structs
 * a ptr to a single-linked list of scanner structs
 */
static const SANE_Device **sane_devArray = NULL;
static struct scanner *scanner_devList = NULL;

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

  if (version_code)
    *version_code = SANE_VERSION_CODE (SANE_CURRENT_MAJOR, V_MINOR, BUILD);

  DBG (5, "sane_init: canon_dr backend %d.%d.%d, from %s\n",
    SANE_CURRENT_MAJOR, V_MINOR, BUILD, PACKAGE_STRING);

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
/*
 * Read the config file, find scanners with help from sanei_*
 * and store in global device structs
 */
SANE_Status
sane_get_devices (const SANE_Device *** device_list, SANE_Bool local_only)
{
  SANE_Status ret = SANE_STATUS_GOOD;
  struct scanner * s;
  struct scanner * prev = NULL;
  char line[PATH_MAX];
  const char *lp;
  FILE *fp;
  int num_devices=0;
  int i=0;

  local_only = local_only;        /* get rid of compiler warning */

  DBG (10, "sane_get_devices: start\n");

  /* mark all existing scanners as missing, attach_one will remove mark */
  for (s = scanner_devList; s; s = s->next) {
    s->missing = 1;
  }

  sanei_usb_init();

  /* set this to 64K before reading the file */
  global_buffer_size = 64 * 1024;

  fp = sanei_config_open (CANON_DR_CONFIG_FILE);

  if (fp) {

      DBG (15, "sane_get_devices: reading config file %s\n",
        CANON_DR_CONFIG_FILE);

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
                    DBG (5, "sane_get_devices: config option \"buffer-size\" (%d) is < 4096, ignoring!\n", buf);
                    continue;
                  }
    
                  if (buf > 64*1024) {
                    DBG (5, "sane_get_devices: config option \"buffer-size\" (%d) is > %d, warning!\n", buf, 64*1024);
                  }
    
                  DBG (15, "sane_get_devices: setting \"buffer-size\" to %d\n", buf);
                  global_buffer_size = buf;
              }
              else {
                  DBG (5, "sane_get_devices: config option \"%s\" unrecognized - ignored.\n", lp);
              }
          }
          else if ((strncmp ("usb", lp, 3) == 0) && isspace (lp[3])) {
              DBG (15, "sane_get_devices: looking for '%s'\n", lp);
              sanei_usb_attach_matching_devices(lp, attach_one_usb);
          }
          else if ((strncmp ("scsi", lp, 4) == 0) && isspace (lp[4])) {
              DBG (15, "sane_get_devices: looking for '%s'\n", lp);
              sanei_config_attach_matching_devices (lp, attach_one_scsi);
          }
          else{
              DBG (5, "sane_get_devices: config line \"%s\" unrecognized - ignored.\n", lp);
          }
      }
      fclose (fp);
  }

  else {
      DBG (5, "sane_get_devices: no config file '%s', using defaults\n", CANON_DR_CONFIG_FILE);

      DBG (15, "sane_get_devices: looking for 'scsi CANON DR'\n");
      sanei_config_attach_matching_devices ("scsi CANON DR", attach_one_scsi);

      DBG (15, "sane_get_devices: looking for 'usb 0x04a9 0x1601'\n");
      sanei_usb_attach_matching_devices("usb 0x04a9 0x1601", attach_one_usb);

      DBG (15, "sane_get_devices: looking for 'usb 0x04a9 0x1603'\n");
      sanei_usb_attach_matching_devices("usb 0x04a9 0x1603", attach_one_usb);

      DBG (15, "sane_get_devices: looking for 'usb 0x04a9 0x1604'\n");
      sanei_usb_attach_matching_devices("usb 0x04a9 0x1604", attach_one_usb);

      DBG (15, "sane_get_devices: looking for 'usb 0x04a9 0x1606'\n");
      sanei_usb_attach_matching_devices("usb 0x04a9 0x1606", attach_one_usb);

      DBG (15, "sane_get_devices: looking for 'usb 0x04a9 0x1607'\n");
      sanei_usb_attach_matching_devices("usb 0x04a9 0x1607", attach_one_usb);

      DBG (15, "sane_get_devices: looking for 'usb 0x04a9 0x1608'\n");
      sanei_usb_attach_matching_devices("usb 0x04a9 0x1608", attach_one_usb);

      DBG (15, "sane_get_devices: looking for 'usb 0x04a9 0x1609'\n");
      sanei_usb_attach_matching_devices("usb 0x04a9 0x1609", attach_one_usb);

      DBG (15, "sane_get_devices: looking for 'usb 0x04a9 0x160b'\n");
      sanei_usb_attach_matching_devices("usb 0x04a9 0x160b", attach_one_usb);

      DBG (15, "sane_get_devices: looking for 'usb 0x04a9 0x1614'\n");
      sanei_usb_attach_matching_devices("usb 0x04a9 0x1614", attach_one_usb);

      DBG (15, "sane_get_devices: looking for 'usb 0x04a9 0x1618'\n");
      sanei_usb_attach_matching_devices("usb 0x04a9 0x1618", attach_one_usb);
  }

  /*delete missing scanners from list*/
  for (s = scanner_devList; s;) {
    if(s->missing){
      DBG (5, "sane_get_devices: missing scanner %s\n",s->device_name);

      /*splice s out of list by changing pointer in prev to next*/
      if(prev){
        prev->next = s->next;
        free(s);
        s=prev->next;
      }
      /*remove s from head of list, using prev to cache it*/
      else{
        prev = s;
        s = s->next;
        free(prev);
	prev=NULL;

	/*reset head to next s*/
	scanner_devList = s;
      }
    }
    else{
      prev = s;
      s=prev->next;
    }
  }

  for (s = scanner_devList; s; s=s->next) {
    DBG (15, "sane_get_devices: found scanner %s\n",s->device_name);
    num_devices++;
  }

  DBG (15, "sane_get_devices: found %d scanner(s)\n",num_devices);

  if (sane_devArray)
    free (sane_devArray);

  sane_devArray = calloc (num_devices + 1, sizeof (SANE_Device*));
  if (!sane_devArray)
    return SANE_STATUS_NO_MEM;

  for (s = scanner_devList; s; s=s->next) {
    sane_devArray[i++] = (SANE_Device *)&s->sane;
  }
  sane_devArray[i] = 0;

  if(device_list){
      *device_list = sane_devArray;
  }

  DBG (10, "sane_get_devices: finish\n");

  return ret;
}

/* callbacks used by sane_get_devices */
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
  struct scanner *s;
  int ret;

  DBG (10, "attach_one: start\n");
  DBG (15, "attach_one: looking for '%s'\n", device_name);

  for (s = scanner_devList; s; s = s->next) {
    if (strcmp (s->device_name, device_name) == 0){
      DBG (10, "attach_one: already attached!\n");
      s->missing = 0;
      return SANE_STATUS_GOOD;
    }
  }

  /* build a scanner struct to hold it */
  if ((s = calloc (sizeof (*s), 1)) == NULL)
    return SANE_STATUS_NO_MEM;

  /* scsi command/data buffer */
  s->buffer_size = global_buffer_size;

  /* copy the device name */
  strcpy (s->device_name, device_name);

  /* connect the fd */
  s->connection = connType;
  s->fd = -1;
  s->fds[0] = -1;
  s->fds[1] = -1;
  ret = connect_fd(s);
  if(ret != SANE_STATUS_GOOD){
    free (s);
    return ret;
  }

  /* Now query the device to load its vendor/model/version */
  ret = init_inquire (s);
  if (ret != SANE_STATUS_GOOD) {
    disconnect_fd(s);
    free (s);
    DBG (5, "attach_one: inquiry failed\n");
    return ret;
  }

  /* load detailed specs/capabilities from the device */
  ret = init_vpd (s);
  if (ret != SANE_STATUS_GOOD) {
    disconnect_fd(s);
    free (s);
    DBG (5, "attach_one: vpd failed\n");
    return ret;
  }

  /* clean up the scanner struct based on model */
  /* this is the only piece of model specific code */
  ret = init_model (s);
  if (ret != SANE_STATUS_GOOD) {
    disconnect_fd(s);
    free (s);
    DBG (5, "attach_one: model failed\n");
    return ret;
  }

  /* enable/read the buttons */
  ret = init_panel (s);
  if (ret != SANE_STATUS_GOOD) {
    disconnect_fd(s);
    free (s);
    DBG (5, "attach_one: model failed\n");
    return ret;
  }

  /* sets SANE option 'values' to good defaults */
  ret = init_user (s);
  if (ret != SANE_STATUS_GOOD) {
    disconnect_fd(s);
    free (s);
    DBG (5, "attach_one: user failed\n");
    return ret;
  }

  ret = init_options (s);
  if (ret != SANE_STATUS_GOOD) {
    disconnect_fd(s);
    free (s);
    DBG (5, "attach_one: options failed\n");
    return ret;
  }

  /* load strings into sane_device struct */
  s->sane.name = s->device_name;
  s->sane.vendor = s->vendor_name;
  s->sane.model = s->model_name;
  s->sane.type = "scanner";

  /* change name in sane_device struct if scanner has serial number
  ret = init_serial (s);
  if (ret == SANE_STATUS_GOOD) {
    s->sane.name = s->serial_name;
  }
  else{
    DBG (5, "attach_one: serial number unsupported?\n");
  }
  */

  /* we close the connection, so that another backend can talk to scanner */
  disconnect_fd(s);

  /* store this scanner in global vars */
  s->next = scanner_devList;
  scanner_devList = s;

  DBG (10, "attach_one: finish\n");

  return SANE_STATUS_GOOD;
}

/*
 * connect the fd in the scanner struct
 */
static SANE_Status
connect_fd (struct scanner *s)
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
    ret = sanei_usb_clear_halt(s->fd);
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
 * This routine will check if a certain device is a Canon scanner
 * It also copies interesting data from INQUIRY into the handle structure
 */
static SANE_Status
init_inquire (struct scanner *s)
{
  int i;
  SANE_Status ret;

  unsigned char cmd[INQUIRY_len];
  size_t cmdLen = INQUIRY_len;

  unsigned char in[INQUIRY_std_len];
  size_t inLen = INQUIRY_std_len;

  DBG (10, "init_inquire: start\n");

  memset(cmd,0,cmdLen);
  set_SCSI_opcode(cmd, INQUIRY_code);
  set_IN_return_size (cmd, inLen);
  set_IN_evpd (cmd, 0);
  set_IN_page_code (cmd, 0);
 
  ret = do_cmd (
    s, 1, 0, 
    cmd, cmdLen,
    NULL, 0,
    in, &inLen
  );

  if (ret != SANE_STATUS_GOOD){
    return ret;
  }

  if (get_IN_periph_devtype (in) != IN_periph_devtype_scanner){
    DBG (5, "The device at '%s' is not a scanner.\n", s->device_name);
    return SANE_STATUS_INVAL;
  }

  get_IN_vendor (in, s->vendor_name);
  get_IN_product (in, s->model_name);
  get_IN_version (in, s->version_name);

  s->vendor_name[8] = 0;
  s->model_name[16] = 0;
  s->version_name[4] = 0;

  /* gobble trailing spaces */
  for (i = 7; s->vendor_name[i] == ' ' && i >= 0; i--)
    s->vendor_name[i] = 0;
  for (i = 15; s->model_name[i] == ' ' && i >= 0; i--)
    s->model_name[i] = 0;
  for (i = 3; s->version_name[i] == ' ' && i >= 0; i--)
    s->version_name[i] = 0;

  /*check for vendor name*/
  if (strcmp ("CANON", s->vendor_name)) {
    DBG (5, "The device at '%s' is reported to be made by '%s'\n", s->device_name, s->vendor_name);
    DBG (5, "This backend only supports Canon DR-series products.\n");
    return SANE_STATUS_INVAL;
  }

  /*check for model name*/
  if (strncmp ("DR", s->model_name, 2)) {
    DBG (5, "The device at '%s' is reported to be a '%s'\n", s->device_name, s->model_name);
    DBG (5, "This backend only supports Canon DR-series products.\n");
    return SANE_STATUS_INVAL;
  }

  DBG (15, "init_inquire: Found %s scanner %s version %s at %s\n",
    s->vendor_name, s->model_name, s->version_name, s->device_name);

  DBG (10, "init_inquire: finish\n");

  return SANE_STATUS_GOOD;
}

/*
 * Use INQUIRY VPD to setup more detail about the scanner
 */
static SANE_Status
init_vpd (struct scanner *s)
{
  SANE_Status ret;

  unsigned char cmd[INQUIRY_len];
  size_t cmdLen = INQUIRY_len;

  unsigned char in[INQUIRY_vpd_len];
  size_t inLen = INQUIRY_vpd_len;

  DBG (10, "init_vpd: start\n");

  /* get EVPD */
  memset(cmd,0,cmdLen);
  set_SCSI_opcode(cmd, INQUIRY_code);
  set_IN_return_size (cmd, inLen);
  set_IN_evpd (cmd, 1);
  set_IN_page_code (cmd, 0xf0);

  ret = do_cmd (
    s, 1, 0,
    cmd, cmdLen,
    NULL, 0,
    in, &inLen
  );

  DBG (15, "init_vpd: length=%0x\n",get_IN_page_length (in));

  /* This scanner supports vital product data.
   * Use this data to set dpi-lists etc. */
  if (ret == SANE_STATUS_GOOD || ret == SANE_STATUS_EOF) {

      DBG (15, "standard options\n");

      s->basic_x_res = get_IN_basic_x_res (in);
      DBG (15, "  basic x res: %d dpi\n",s->basic_x_res);

      s->basic_y_res = get_IN_basic_y_res (in);
      DBG (15, "  basic y res: %d dpi\n",s->basic_y_res);

      s->step_x_res = get_IN_step_x_res (in);
      DBG (15, "  step x res: %d dpi\n", s->step_x_res);

      s->step_y_res = get_IN_step_y_res (in);
      DBG (15, "  step y res: %d dpi\n", s->step_y_res);

      s->max_x_res = get_IN_max_x_res (in);
      DBG (15, "  max x res: %d dpi\n", s->max_x_res);

      s->max_y_res = get_IN_max_y_res (in);
      DBG (15, "  max y res: %d dpi\n", s->max_y_res);

      s->min_x_res = get_IN_min_x_res (in);
      DBG (15, "  min x res: %d dpi\n", s->min_x_res);

      s->min_y_res = get_IN_min_y_res (in);
      DBG (15, "  min y res: %d dpi\n", s->min_y_res);

      /* some scanners list B&W resolutions. */
      s->std_res_60 = get_IN_std_res_60 (in);
      DBG (15, "  60 dpi: %d\n", s->std_res_60);

      s->std_res_75 = get_IN_std_res_75 (in);
      DBG (15, "  75 dpi: %d\n", s->std_res_75);

      s->std_res_100 = get_IN_std_res_100 (in);
      DBG (15, "  100 dpi: %d\n", s->std_res_100);

      s->std_res_120 = get_IN_std_res_120 (in);
      DBG (15, "  120 dpi: %d\n", s->std_res_120);

      s->std_res_150 = get_IN_std_res_150 (in);
      DBG (15, "  150 dpi: %d\n", s->std_res_150);

      s->std_res_160 = get_IN_std_res_160 (in);
      DBG (15, "  160 dpi: %d\n", s->std_res_160);

      s->std_res_180 = get_IN_std_res_180 (in);
      DBG (15, "  180 dpi: %d\n", s->std_res_180);

      s->std_res_200 = get_IN_std_res_200 (in);
      DBG (15, "  200 dpi: %d\n", s->std_res_200);

      s->std_res_240 = get_IN_std_res_240 (in);
      DBG (15, "  240 dpi: %d\n", s->std_res_240);

      s->std_res_300 = get_IN_std_res_300 (in);
      DBG (15, "  300 dpi: %d\n", s->std_res_300);

      s->std_res_320 = get_IN_std_res_320 (in);
      DBG (15, "  320 dpi: %d\n", s->std_res_320);

      s->std_res_400 = get_IN_std_res_400 (in);
      DBG (15, "  400 dpi: %d\n", s->std_res_400);

      s->std_res_480 = get_IN_std_res_480 (in);
      DBG (15, "  480 dpi: %d\n", s->std_res_480);

      s->std_res_600 = get_IN_std_res_600 (in);
      DBG (15, "  600 dpi: %d\n", s->std_res_600);

      s->std_res_800 = get_IN_std_res_800 (in);
      DBG (15, "  800 dpi: %d\n", s->std_res_800);

      s->std_res_1200 = get_IN_std_res_1200 (in);
      DBG (15, "  1200 dpi: %d\n", s->std_res_1200);

      /* maximum window width and length are reported in basic units.*/
      s->max_x_basic = get_IN_window_width(in);
      DBG(15, "  max width: %2.2f inches\n",(float)s->max_x_basic/s->basic_x_res);

      s->max_y_basic = get_IN_window_length(in);
      DBG(15, "  max length: %2.2f inches\n",(float)s->max_y_basic/s->basic_y_res);

      DBG (15, "  unknown7: %d\n", get_IN_unknown7(in));
      DBG (15, "  unknown6: %d\n", get_IN_unknown6(in));
      DBG (15, "  unknown5: %d\n", get_IN_unknown5(in));
      DBG (15, "  unknown4: %d\n", get_IN_unknown4(in));

      /* known modes */
      s->can_grayscale = get_IN_multilevel (in);
      DBG (15, "  grayscale: %d\n", s->can_grayscale);

      s->can_halftone = get_IN_half_tone (in);
      DBG (15, "  halftone: %d\n", s->can_halftone);

      s->can_monochrome = get_IN_monochrome (in);
      DBG (15, "  monochrome: %d\n", s->can_monochrome);

      s->can_overflow = get_IN_overflow(in);
      DBG (15, "  overflow: %d\n", s->can_overflow);
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

/*
 * get model specific info that is not in vpd, and correct
 * errors in vpd data. struct is already initialized to 0.
 */
static SANE_Status
init_model (struct scanner *s)
{

  DBG (10, "init_model: start\n");

  s->reverse_by_mode[MODE_LINEART] = 1;
  s->reverse_by_mode[MODE_HALFTONE] = 1;
  s->reverse_by_mode[MODE_GRAYSCALE] = 0;
  s->reverse_by_mode[MODE_COLOR] = 0;

  s->can_color = 1;
  s->has_rif = 0;
  s->has_adf = 1;
  s->has_duplex = 1;
  s->has_buffer = 1;
  s->has_back = 0;
  s->has_comp_JPEG = 0;

  s->brightness_steps = 255;
  s->contrast_steps = 255;
  s->threshold_steps = 255;

  /* convert to 1200dpi units */
  s->max_x = s->max_x_basic * 1200 / s->basic_x_res;
  s->max_y = s->max_y_basic * 1200 / s->basic_y_res;

  /* assume these are same as adf, override below */
  s->max_x_fb = s->max_x;
  s->max_y_fb = s->max_y;

  /* any settings missing from vpd */
  if (strstr (s->model_name,"DR-9080")){
    s->has_comp_JPEG = 1;
    s->has_counter = 1;
  }

  if (strstr (s->model_name,"DR-7580")){
    s->can_color = 0;
    s->has_comp_JPEG = 1;
    s->has_counter = 1;
  }

  DBG (10, "init_model: finish\n");

  return SANE_STATUS_GOOD;
}

/*
 * This function enables the buttons and preloads the current panel values
 */
static SANE_Status
init_panel (struct scanner *s)
{
  SANE_Status ret = SANE_STATUS_GOOD;

  DBG (10, "init_panel: start\n");

  ret = read_panel(s);
  s->panel_enable_led = 1;
  ret = send_panel(s);

  DBG (10, "init_panel: finish\n");

  return ret;
}

/*
 * set good default user values.
 * struct is already initialized to 0.
 */
static SANE_Status
init_user (struct scanner *s)
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
  else if(s->can_color)
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

  s->threshold = 0x80;
  s->compress_arg = 50;

  DBG (10, "init_user: finish\n");

  return SANE_STATUS_GOOD;
}

/*
 * This function presets the "option" array to blank
 */
static SANE_Status
init_options (struct scanner *s)
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
  struct scanner *dev = NULL;
  struct scanner *s = NULL;
  SANE_Status ret;
 
  DBG (10, "sane_open: start\n");

  if(scanner_devList){
    DBG (15, "sane_open: searching currently attached scanners\n");
  }
  else{
    DBG (15, "sane_open: no scanners currently attached, attaching\n");

    ret = sane_get_devices(NULL,0);
    if(ret != SANE_STATUS_GOOD){
      return ret;
    }
  }

  if(name[0] == 0){
    DBG (15, "sane_open: no device requested, using default\n");
    s = scanner_devList;
  }
  else{
    DBG (15, "sane_open: device %s requested\n", name);
                                                                                
    for (dev = scanner_devList; dev; dev = dev->next) {
      if (strcmp (dev->sane.name, name) == 0
       || strcmp (dev->device_name, name) == 0) { /*always allow sanei devname*/
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
  struct scanner *s = handle;
  int i;
  SANE_Option_Descriptor *opt = &s->opt[option];

  DBG (20, "sane_get_option_descriptor: %d\n", option);

  if ((unsigned) option >= NUM_OPTIONS)
    return NULL;

  /* "Mode" group -------------------------------------------------------- */
  if(option==OPT_STANDARD_GROUP){
    opt->name = SANE_NAME_STANDARD;
    opt->title = SANE_TITLE_STANDARD;
    opt->desc = SANE_DESC_STANDARD;
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
    if(s->can_color){
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
  
    opt->name = SANE_NAME_SCAN_RESOLUTION;
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
    opt->name = SANE_NAME_GEOMETRY;
    opt->title = SANE_TITLE_GEOMETRY;
    opt->desc = SANE_DESC_GEOMETRY;
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

    opt->name = SANE_NAME_PAGE_WIDTH;
    opt->title = SANE_TITLE_PAGE_WIDTH;
    opt->desc = SANE_DESC_PAGE_WIDTH;
    opt->type = SANE_TYPE_FIXED;
    opt->unit = SANE_UNIT_MM;
    opt->constraint_type = SANE_CONSTRAINT_RANGE;
    opt->constraint.range = &s->paper_x_range;

    if(s->has_adf){
      opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
      if(s->source == SOURCE_FLATBED){
        opt->cap |= SANE_CAP_INACTIVE;
      }
    }
    else{
      opt->cap = SANE_CAP_INACTIVE;
    }
  }

  /* page height */
  if(option==OPT_PAGE_HEIGHT){
    /* values stored in 1200 dpi units */
    /* must be converted to MM for sane */
    s->paper_y_range.min = SCANNER_UNIT_TO_FIXED_MM(s->min_y);
    s->paper_y_range.max = SCANNER_UNIT_TO_FIXED_MM(s->max_y);
    s->paper_y_range.quant = MM_PER_UNIT_FIX;

    opt->name = SANE_NAME_PAGE_HEIGHT;
    opt->title = SANE_TITLE_PAGE_HEIGHT;
    opt->desc = SANE_DESC_PAGE_HEIGHT;
    opt->type = SANE_TYPE_FIXED;
    opt->unit = SANE_UNIT_MM;
    opt->constraint_type = SANE_CONSTRAINT_RANGE;
    opt->constraint.range = &s->paper_y_range;

    if(s->has_adf){
      opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
      if(s->source == SOURCE_FLATBED){
        opt->cap |= SANE_CAP_INACTIVE;
      }
    }
    else{
      opt->cap = SANE_CAP_INACTIVE;
    }
  }

  /* "Enhancement" group ------------------------------------------------- */
  if(option==OPT_ENHANCEMENT_GROUP){
    opt->name = SANE_NAME_ENHANCEMENT;
    opt->title = SANE_TITLE_ENHANCEMENT;
    opt->desc = SANE_DESC_ENHANCEMENT;
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
    if (s->brightness_steps){
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
    if (s->contrast_steps){
      s->contrast_range.min=-127;
      s->contrast_range.max=127;
      opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    }
    else {
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

    if (s->threshold_steps){
      opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
      if(s->mode != MODE_LINEART){
        opt->cap |= SANE_CAP_INACTIVE;
      }
    }
    else {
      opt->cap = SANE_CAP_INACTIVE;
    }
  }

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
    opt->name = SANE_NAME_ADVANCED;
    opt->title = SANE_TITLE_ADVANCED;
    opt->desc = SANE_DESC_ADVANCED;
    opt->type = SANE_TYPE_GROUP;
    opt->constraint_type = SANE_CONSTRAINT_NONE;
  }

  /*image compression*/
  if(option==OPT_COMPRESS){
    i=0;
    s->compress_list[i++]=string_None;

    if(s->has_comp_JPEG){
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

    if (i > 1){
      opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
      if (s->mode != MODE_COLOR && s->mode != MODE_GRAYSCALE){
        opt->cap |= SANE_CAP_INACTIVE;
      }
    }
    else
      opt->cap = SANE_CAP_INACTIVE;
  }

  /*image compression arg*/
  if(option==OPT_COMPRESS_ARG){

    opt->name = "compression-arg";
    opt->title = "Compression argument";
    opt->desc = "Level of JPEG compression. 1 is small file, 100 is large file.";
    opt->type = SANE_TYPE_INT;
    opt->unit = SANE_UNIT_NONE;
    opt->constraint_type = SANE_CONSTRAINT_RANGE;
    opt->constraint.range = &s->compress_arg_range;
    s->compress_arg_range.quant=1;

    if(s->has_comp_JPEG){
      s->compress_arg_range.min=0;
      s->compress_arg_range.max=100;
      opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;

      if(s->compress != COMP_JPEG){
        opt->cap |= SANE_CAP_INACTIVE;
      }
    }
    else
      opt->cap = SANE_CAP_INACTIVE;
  }

  /*double feed by length*/
  if(option==OPT_DF_LENGTH){
    opt->name = "df-length";
    opt->title = "DF by length";
    opt->desc = "Detect double feeds by comparing document lengths";
    opt->type = SANE_TYPE_BOOL;
    opt->unit = SANE_UNIT_NONE;
    opt->constraint_type = SANE_CONSTRAINT_NONE;

    if (1)
     opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
    else
     opt->cap = SANE_CAP_INACTIVE;
  }

  /*double feed by thickness */
  if(option==OPT_DF_THICKNESS){
  
    opt->name = "df-thickness";
    opt->title = "DF by thickness";
    opt->desc = "Detect double feeds using thickness sensor";
    opt->type = SANE_TYPE_BOOL;
    opt->unit = SANE_UNIT_NONE;
    opt->constraint_type = SANE_CONSTRAINT_NONE;

    if (1){
     opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
    }
    else
     opt->cap = SANE_CAP_INACTIVE;
  }

  /*deskew by roller*/
  if(option==OPT_ROLLERDESKEW){
    opt->name = "rollerdeskew";
    opt->title = "Roller deskew";
    opt->desc = "Request scanner to correct skewed pages mechanically";
    opt->type = SANE_TYPE_BOOL;
    if (1)
     opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
    else
     opt->cap = SANE_CAP_INACTIVE;
  }

  /*staple detection*/
  if(option==OPT_STAPLEDETECT){
    opt->name = "stapledetect";
    opt->title = "Staple detect";
    opt->desc = "Request scanner to halt if stapled pages are detected";
    opt->type = SANE_TYPE_BOOL;
    if (1)
     opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
    else
     opt->cap = SANE_CAP_INACTIVE;
  }

  /*dropout color front*/
  if(option==OPT_DROPOUT_COLOR_F){
    s->do_color_list[0] = string_None;
    s->do_color_list[1] = string_Red;
    s->do_color_list[2] = string_Green;
    s->do_color_list[3] = string_Blue;
    s->do_color_list[4] = string_En_Red;
    s->do_color_list[5] = string_En_Green;
    s->do_color_list[6] = string_En_Blue;
    s->do_color_list[7] = NULL;
  
    opt->name = "dropout-front";
    opt->title = "Dropout color front";
    opt->desc = "One-pass scanners use only one color during gray or binary scanning, useful for colored paper or ink";
    opt->type = SANE_TYPE_STRING;
    opt->constraint_type = SANE_CONSTRAINT_STRING_LIST;
    opt->constraint.string_list = s->do_color_list;
    opt->size = maxStringSize (opt->constraint.string_list);

    if (1){
      opt->cap = SANE_CAP_SOFT_SELECT|SANE_CAP_SOFT_DETECT|SANE_CAP_ADVANCED;
      if(s->mode == MODE_COLOR)
        opt->cap |= SANE_CAP_INACTIVE;
    }
    else
      opt->cap = SANE_CAP_INACTIVE;
  }

  /*dropout color back*/
  if(option==OPT_DROPOUT_COLOR_B){
    s->do_color_list[0] = string_None;
    s->do_color_list[1] = string_Red;
    s->do_color_list[2] = string_Green;
    s->do_color_list[3] = string_Blue;
    s->do_color_list[4] = string_En_Red;
    s->do_color_list[5] = string_En_Green;
    s->do_color_list[6] = string_En_Blue;
    s->do_color_list[7] = NULL;
  
    opt->name = "dropout-back";
    opt->title = "Dropout color back";
    opt->desc = "One-pass scanners use only one color during gray or binary scanning, useful for colored paper or ink";
    opt->type = SANE_TYPE_STRING;
    opt->constraint_type = SANE_CONSTRAINT_STRING_LIST;
    opt->constraint.string_list = s->do_color_list;
    opt->size = maxStringSize (opt->constraint.string_list);

    if (1){
      opt->cap = SANE_CAP_SOFT_SELECT|SANE_CAP_SOFT_DETECT|SANE_CAP_ADVANCED;
      if(s->mode == MODE_COLOR)
        opt->cap |= SANE_CAP_INACTIVE;
    }
    else
      opt->cap = SANE_CAP_INACTIVE;
  }

  /*buffer mode*/
  if(option==OPT_BUFFERMODE){
    opt->name = "buffermode";
    opt->title = "Buffer mode";
    opt->desc = "Request scanner to read pages async into internal memory";
    opt->type = SANE_TYPE_BOOL;
    if (s->has_buffer)
     opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
    else
     opt->cap = SANE_CAP_INACTIVE;
  }

  /* "Sensor" group ------------------------------------------------------ */
  if(option==OPT_SENSOR_GROUP){
    opt->name = SANE_NAME_SENSORS;
    opt->title = SANE_TITLE_SENSORS;
    opt->desc = SANE_DESC_SENSORS;
    opt->type = SANE_TYPE_GROUP;
    opt->constraint_type = SANE_CONSTRAINT_NONE;
  }

  if(option==OPT_START){
    opt->name = "start";
    opt->title = "Start button";
    opt->desc = "Big green button";
    opt->type = SANE_TYPE_BOOL;
    opt->unit = SANE_UNIT_NONE;
    opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_HARD_SELECT | SANE_CAP_ADVANCED;
  }

  if(option==OPT_STOP){
    opt->name = "stop";
    opt->title = "Stop button";
    opt->desc = "Little orange button";
    opt->type = SANE_TYPE_BOOL;
    opt->unit = SANE_UNIT_NONE;
    opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_HARD_SELECT | SANE_CAP_ADVANCED;
  }

  if(option==OPT_NEWFILE){
    opt->name = "newfile";
    opt->title = "New File button";
    opt->desc = "New File button";
    opt->type = SANE_TYPE_BOOL;
    opt->unit = SANE_UNIT_NONE;
    opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_HARD_SELECT | SANE_CAP_ADVANCED;
  }

  if(option==OPT_COUNTONLY){
    opt->name = "countonly";
    opt->title = "Count Only button";
    opt->desc = "Count Only button";
    opt->type = SANE_TYPE_BOOL;
    opt->unit = SANE_UNIT_NONE;
    opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_HARD_SELECT | SANE_CAP_ADVANCED;
  }

  if(option==OPT_BYPASSMODE){
    opt->name = "bypassmode";
    opt->title = "Bypass Mode button";
    opt->desc = "Bypass Mode button";
    opt->type = SANE_TYPE_BOOL;
    opt->unit = SANE_UNIT_NONE;
    opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_HARD_SELECT | SANE_CAP_ADVANCED;
  }

  if(option==OPT_COUNTER){
    opt->name = "counter";
    opt->title = "Counter";
    opt->desc = "Scan counter";
    opt->type = SANE_TYPE_INT;
    opt->unit = SANE_UNIT_NONE;
    opt->constraint_type = SANE_CONSTRAINT_RANGE;
    opt->constraint.range = &s->counter_range;
    s->counter_range.min=0;
    s->counter_range.max=500;
    s->counter_range.quant=1;

    if (s->has_counter)
      opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_HARD_SELECT;
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
  struct scanner *s = (struct scanner *) handle;
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

        case OPT_DF_LENGTH:
          *val_p = s->df_length;
          return SANE_STATUS_GOOD;

        case OPT_DF_THICKNESS:
          *val_p = s->df_thickness;
          return SANE_STATUS_GOOD;

        case OPT_ROLLERDESKEW:
          *val_p = s->rollerdeskew;
          return SANE_STATUS_GOOD;

        case OPT_STAPLEDETECT:
          *val_p = s->stapledetect;
          return SANE_STATUS_GOOD;

        case OPT_DROPOUT_COLOR_F:
          switch (s->dropout_color_f) {
            case COLOR_NONE:
              strcpy (val, string_None);
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
            case COLOR_EN_RED:
              strcpy (val, string_En_Red);
              break;
            case COLOR_EN_GREEN:
              strcpy (val, string_En_Green);
              break;
            case COLOR_EN_BLUE:
              strcpy (val, string_En_Blue);
              break;
          }
          return SANE_STATUS_GOOD;

        case OPT_DROPOUT_COLOR_B:
          switch (s->dropout_color_b) {
            case COLOR_NONE:
              strcpy (val, string_None);
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
            case COLOR_EN_RED:
              strcpy (val, string_En_Red);
              break;
            case COLOR_EN_GREEN:
              strcpy (val, string_En_Green);
              break;
            case COLOR_EN_BLUE:
              strcpy (val, string_En_Blue);
              break;
          }
          return SANE_STATUS_GOOD;

        case OPT_BUFFERMODE:
          *val_p = s->buffermode;
          return SANE_STATUS_GOOD;

        /* Sensor Group */
        case OPT_START:
          read_panel(s);
          *val_p = s->panel_start;
          return SANE_STATUS_GOOD;

        case OPT_STOP:
          read_panel(s);
          *val_p = s->panel_stop;
          return SANE_STATUS_GOOD;

        case OPT_NEWFILE:
          read_panel(s);
          *val_p = s->panel_new_file;
          return SANE_STATUS_GOOD;

        case OPT_COUNTONLY:
          read_panel(s);
          *val_p = s->panel_count_only;
          return SANE_STATUS_GOOD;

        case OPT_BYPASSMODE:
          read_panel(s);
          *val_p = s->panel_bypass_mode;
          return SANE_STATUS_GOOD;

        case OPT_COUNTER:
          read_panel(s);
          *val_p = s->panel_counter;
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
          return ssm_buffer(s);

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
          return SANE_STATUS_GOOD;

        case OPT_CONTRAST:
          s->contrast = val_c;
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
            s->compress = COMP_JPEG;
          }
          else{
            s->compress = COMP_NONE;
          }
          return SANE_STATUS_GOOD;

        case OPT_COMPRESS_ARG:
          s->compress_arg = val_c;
          return SANE_STATUS_GOOD;

        case OPT_DF_LENGTH:
          s->df_length = val_c;
          return ssm_df(s);

        case OPT_DF_THICKNESS:
          s->df_thickness = val_c;
          return ssm_df(s);

        case OPT_ROLLERDESKEW:
          s->rollerdeskew = val_c;
          return ssm_df(s);

        case OPT_STAPLEDETECT:
          s->stapledetect = val_c;
          return ssm_df(s);

        case OPT_DROPOUT_COLOR_F:
          if (!strcmp(val, string_None))
            s->dropout_color_f = COLOR_NONE;
          else if (!strcmp(val, string_Red))
            s->dropout_color_f = COLOR_RED;
          else if (!strcmp(val, string_Green))
            s->dropout_color_f = COLOR_GREEN;
          else if (!strcmp(val, string_Blue))
            s->dropout_color_f = COLOR_BLUE;
          else if (!strcmp(val, string_En_Red))
            s->dropout_color_f = COLOR_EN_RED;
          else if (!strcmp(val, string_En_Green))
            s->dropout_color_f = COLOR_EN_GREEN;
          else if (!strcmp(val, string_En_Blue))
            s->dropout_color_f = COLOR_EN_BLUE;
          return ssm_do(s);

        case OPT_DROPOUT_COLOR_B:
          if (!strcmp(val, string_None))
            s->dropout_color_b = COLOR_NONE;
          else if (!strcmp(val, string_Red))
            s->dropout_color_b = COLOR_RED;
          else if (!strcmp(val, string_Green))
            s->dropout_color_b = COLOR_GREEN;
          else if (!strcmp(val, string_Blue))
            s->dropout_color_b = COLOR_BLUE;
          else if (!strcmp(val, string_En_Red))
            s->dropout_color_b = COLOR_EN_RED;
          else if (!strcmp(val, string_En_Green))
            s->dropout_color_b = COLOR_EN_GREEN;
          else if (!strcmp(val, string_En_Blue))
            s->dropout_color_b = COLOR_EN_BLUE;
          return ssm_do(s);

        case OPT_BUFFERMODE:
          s->buffermode = val_c;
          return ssm_buffer(s);

        /* Sensor Group */
        case OPT_COUNTER:
          s->panel_counter = val_c;
          return send_panel(s);
      }
  }                           /* else */

  return SANE_STATUS_INVAL;
}

static SANE_Status
ssm_buffer (struct scanner *s)
{
  SANE_Status ret = SANE_STATUS_GOOD;

  unsigned char cmd[SET_SCAN_MODE_len];
  size_t cmdLen = SET_SCAN_MODE_len;

  unsigned char out[SSM_PAY_len];
  size_t outLen = SSM_PAY_len;

  DBG (10, "ssm_buffer: start\n");

  memset(cmd,0,cmdLen);
  set_SCSI_opcode(cmd, SET_SCAN_MODE_code);
  set_SSM_pf(cmd, 1);
  set_SSM_pay_len(cmd, outLen);

  memset(out,0,outLen);
  set_SSM_page_code(out, SM_pc_buffer);
  set_SSM_page_len(out, SSM_PAGE_len);

  if(s->source == SOURCE_ADF_DUPLEX){
    set_SSM_BUFF_duplex(out, 0x02);
  }
  if(s->buffermode){
    set_SSM_BUFF_async(out, 0x40);
  }

  ret = do_cmd (
      s, 1, 0,
      cmd, cmdLen,
      out, outLen,
      NULL, NULL
  );

  DBG (10, "ssm_buffer: finish\n");

  return ret;
}

static SANE_Status
ssm_df (struct scanner *s)
{
  SANE_Status ret = SANE_STATUS_GOOD;

  unsigned char cmd[SET_SCAN_MODE_len];
  size_t cmdLen = SET_SCAN_MODE_len;

  unsigned char out[SSM_PAY_len];
  size_t outLen = SSM_PAY_len;

  DBG (10, "ssm_df: start\n");

  memset(cmd,0,cmdLen);
  set_SCSI_opcode(cmd, SET_SCAN_MODE_code);
  set_SSM_pf(cmd, 1);
  set_SSM_pay_len(cmd, outLen);

  memset(out,0,outLen);
  set_SSM_page_code(out, SM_pc_df);
  set_SSM_page_len(out, SSM_PAGE_len);

  /* deskew by roller */
  if(s->rollerdeskew){
    set_SSM_DF_deskew_roll(out, 1);
  }
  
  /* staple detection */
  if(s->stapledetect){
    set_SSM_DF_staple(out, 1);
  }

  /* thickness */
  if(s->df_thickness){
    set_SSM_DF_thick(out, 1);
  }
  
  /* length */
  if(s->df_length){
    set_SSM_DF_len(out, 1);
  }

  ret = do_cmd (
      s, 1, 0,
      cmd, cmdLen,
      out, outLen,
      NULL, NULL
  );

  DBG (10, "ssm_df: finish\n");

  return ret;
}

static SANE_Status
ssm_do (struct scanner *s)
{
  SANE_Status ret = SANE_STATUS_GOOD;

  unsigned char cmd[SET_SCAN_MODE_len];
  size_t cmdLen = SET_SCAN_MODE_len;

  unsigned char out[SSM_PAY_len];
  size_t outLen = SSM_PAY_len;

  DBG (10, "ssm_do: start\n");

  memset(cmd,0,cmdLen);
  set_SCSI_opcode(cmd, SET_SCAN_MODE_code);
  set_SSM_pf(cmd, 1);
  set_SSM_pay_len(cmd, outLen);

  memset(out,0,outLen);
  set_SSM_page_code(out, SM_pc_dropout);
  set_SSM_page_len(out, SSM_PAGE_len);

  set_SSM_DO_unk1(out, 0x03);

  switch(s->dropout_color_f){
    case COLOR_RED:
      set_SSM_DO_unk2(out, 0x05);
      set_SSM_DO_f_do(out,SSM_DO_red);
      break;
    case COLOR_GREEN:
      set_SSM_DO_unk2(out, 0x05);
      set_SSM_DO_f_do(out,SSM_DO_green);
      break;
    case COLOR_BLUE:
      set_SSM_DO_unk2(out, 0x05);
      set_SSM_DO_f_do(out,SSM_DO_blue);
      break;
    case COLOR_EN_RED:
      set_SSM_DO_unk2(out, 0x05);
      set_SSM_DO_f_en(out,SSM_DO_red);
      break;
    case COLOR_EN_GREEN:
      set_SSM_DO_unk2(out, 0x05);
      set_SSM_DO_f_en(out,SSM_DO_green);
      break;
    case COLOR_EN_BLUE:
      set_SSM_DO_unk2(out, 0x05);
      set_SSM_DO_f_en(out,SSM_DO_blue);
      break;
  }

  switch(s->dropout_color_b){
    case COLOR_RED:
      set_SSM_DO_unk2(out, 0x05);
      set_SSM_DO_b_do(out,SSM_DO_red);
      break;
    case COLOR_GREEN:
      set_SSM_DO_unk2(out, 0x05);
      set_SSM_DO_b_do(out,SSM_DO_green);
      break;
    case COLOR_BLUE:
      set_SSM_DO_unk2(out, 0x05);
      set_SSM_DO_b_do(out,SSM_DO_blue);
      break;
    case COLOR_EN_RED:
      set_SSM_DO_unk2(out, 0x05);
      set_SSM_DO_b_en(out,SSM_DO_red);
      break;
    case COLOR_EN_GREEN:
      set_SSM_DO_unk2(out, 0x05);
      set_SSM_DO_b_en(out,SSM_DO_green);
      break;
    case COLOR_EN_BLUE:
      set_SSM_DO_unk2(out, 0x05);
      set_SSM_DO_b_en(out,SSM_DO_blue);
      break;
  }

  ret = do_cmd (
      s, 1, 0,
      cmd, cmdLen,
      out, outLen,
      NULL, NULL
  );

  DBG (10, "ssm_do: finish\n");

  return ret;
}

static SANE_Status
read_panel(struct scanner *s)
{
    SANE_Status ret=SANE_STATUS_GOOD;

    unsigned char cmd[READ_len];
    size_t cmdLen = READ_len;

    unsigned char in[R_PANEL_len];
    size_t inLen = R_PANEL_len;

    DBG (10, "read_panel: start\n");
 
    /* only run this once every second */
    if (s->last_panel < time(NULL)) {

        DBG (15, "read_panel: running\n");
        memset(cmd,0,cmdLen);
        set_SCSI_opcode(cmd, READ_code);
        set_R_datatype_code (cmd, SR_datatype_panel);
        set_R_xfer_length (cmd, inLen);
      
        ret = do_cmd (
          s, 1, 0,
          cmd, cmdLen,
          NULL, 0,
          in, &inLen
        );
      
        if (ret == SANE_STATUS_GOOD || ret == SANE_STATUS_EOF) {
            s->last_panel = time(NULL);
            s->panel_start = get_R_PANEL_start(in);
            s->panel_stop = get_R_PANEL_stop(in);
            s->panel_new_file = get_R_PANEL_new_file(in);
            s->panel_count_only = get_R_PANEL_count_only(in);
            s->panel_bypass_mode = get_R_PANEL_bypass_mode(in);
            s->panel_enable_led = get_R_PANEL_enable_led(in);
            s->panel_counter = get_R_PANEL_counter(in);
            ret = SANE_STATUS_GOOD;
        }
    }
  
    DBG (10, "read_panel: finish\n");
  
    return ret;
}

static SANE_Status
send_panel(struct scanner *s)
{
    SANE_Status ret=SANE_STATUS_GOOD;

    unsigned char cmd[SEND_len];
    size_t cmdLen = SEND_len;

    unsigned char out[S_PANEL_len];
    size_t outLen = S_PANEL_len;

    DBG (10, "send_panel: start\n");
  
    memset(cmd,0,cmdLen);
    set_SCSI_opcode(cmd, SEND_code);
    set_S_xfer_datatype (cmd, SR_datatype_panel);
    set_S_xfer_length (cmd, outLen);

    memset(out,0,outLen);
    set_S_PANEL_enable_led(out,s->panel_enable_led);
    set_S_PANEL_counter(out,s->panel_counter);
  
    ret = do_cmd (
      s, 1, 0,
      cmd, cmdLen,
      out, outLen,
      NULL, NULL
    );
  
    if (ret == SANE_STATUS_EOF) {
        ret = SANE_STATUS_GOOD;
    }
  
    DBG (10, "send_panel: finish %d\n", ret);
  
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
    SANE_Status ret = SANE_STATUS_GOOD;
    struct scanner *s = (struct scanner *) handle;
  
    DBG (10, "sane_get_parameters: start\n");
  
    /* started? get param data from struct */
    if(s->started){
        DBG (15, "sane_get_parameters: started, copying to caller\n");
        params->format = s->params.format;
        params->last_frame = s->params.last_frame;
        params->lines = s->params.lines;
        params->depth = s->params.depth;
        params->pixels_per_line = s->params.pixels_per_line;
        params->bytes_per_line = s->params.bytes_per_line;
    }

    /* not started? get param data from user settings */
    else {
        DBG (15, "sane_get_parameters: not started, updating\n");

        /* this backend only sends single frame images */
        params->last_frame = 1;

        params->pixels_per_line
	  = s->resolution_x * (s->br_x - s->tl_x) / 1200;
  
        params->lines = s->resolution_y * (s->br_y - s->tl_y) / 1200;

        /* round lines down to even number */
        params->lines -= params->lines % 2;
      
        if (s->mode == MODE_COLOR) {
            params->format = SANE_FRAME_RGB;
            params->depth = 8;

            /* jpeg requires 8x8 squares */
            if(s->compress == COMP_JPEG){
              params->format = SANE_FRAME_JPEG;
              params->pixels_per_line -= params->pixels_per_line % 8;
              params->lines -= params->lines % 8;
            }

            params->bytes_per_line = params->pixels_per_line * 3;
        }
        else if (s->mode == MODE_GRAYSCALE) {
            params->format = SANE_FRAME_GRAY;
            params->depth = 8;

            /* jpeg requires 8x8 squares */
            if(s->compress == COMP_JPEG){
              params->format = SANE_FRAME_JPEG;
              params->pixels_per_line -= params->pixels_per_line % 8;
              params->lines -= params->lines % 8;
            }

            params->bytes_per_line = params->pixels_per_line;
        }
        else {
            params->format = SANE_FRAME_GRAY;
            params->depth = 1;

            /* round down to byte boundary */
            params->pixels_per_line -= params->pixels_per_line % 8;
            params->bytes_per_line = params->pixels_per_line / 8;
        }
    }

    DBG(15,"sane_get_parameters: x: max=%d, page=%d, gpw=%d, res=%d\n",
      s->max_x, s->page_width, get_page_width(s), s->resolution_x);

    DBG(15,"sane_get_parameters: y: max=%d, page=%d, gph=%d, res=%d\n",
      s->max_y, s->page_height, get_page_height(s), s->resolution_y);

    DBG(15,"sane_get_parameters: area: tlx=%d, brx=%d, tly=%d, bry=%d\n",
      s->tl_x, s->br_x, s->tl_y, s->br_y);

    DBG (15, "sane_get_parameters: params: ppl=%d, Bpl=%d, lines=%d\n", 
      params->pixels_per_line, params->bytes_per_line, params->lines);

    DBG (15, "sane_get_parameters: params: format=%d, depth=%d, last=%d\n", 
      params->format, params->depth, params->last_frame);

    DBG (10, "sane_get_parameters: finish\n");
  
    return ret;
}

/*
 * Called by SANE when a page acquisition operation is to be started.
 * commands: set window, object pos, and scan
 *
 * this will be called between sides of a duplex scan,
 * and at the start of each page of an adf batch.
 * hence, we spend alot of time playing with s->started, etc.
 */
SANE_Status
sane_start (SANE_Handle handle)
{
  struct scanner *s = handle;
  SANE_Status ret = SANE_STATUS_GOOD;

  DBG (10, "sane_start: start\n");
  DBG (15, "started=%d, side=%d, source=%d\n", s->started, s->side, s->source);

  /* undo any prior sane_cancel calls */
  s->cancelled=0;

  /* not finished with current side, error */
  if (s->started && s->bytes_tx[s->side] != s->bytes_tot[s->side]) {
      DBG(5,"sane_start: previous transfer not finished?");
      return SANE_STATUS_INVAL;
  }

  /* batch start? inititalize struct and scanner */
  if(!s->started){

      /* load side marker */
      if(s->source == SOURCE_ADF_BACK){
        s->side = SIDE_BACK;
      }
      else{
        s->side = SIDE_FRONT;
      }

      /* load our own private copy of scan params */
      ret = sane_get_parameters ((SANE_Handle) s, &s->params);
      if (ret != SANE_STATUS_GOOD) {
        DBG (5, "sane_start: ERROR: cannot get params\n");
        return ret;
      }

      /* switch source
      if(s->source == SOURCE_FLATBED){
        ret = scanner_control(s, SC_function_fb);
        if (ret != SANE_STATUS_GOOD) {
          DBG (5, "sane_start: ERROR: cannot control fb\n");
          return ret;
        }
      }
      else{
        ret = scanner_control(s, SC_function_adf);
        if (ret != SANE_STATUS_GOOD) {
          DBG (5, "sane_start: ERROR: cannot control adf\n");
          return ret;
        }
      }
       * */

      /* eject paper leftover*/
      if(object_position (s, SANE_FALSE)){
        DBG (5, "sane_read: ERROR: cannot eject page\n");
      }

      /* set window command */
      ret = set_window(s);
      if (ret != SANE_STATUS_GOOD) {
        DBG (5, "sane_start: ERROR: cannot set window\n");
        return ret;
      }
    
      /* buffer command */
      ret = ssm_buffer(s);
      if (ret != SANE_STATUS_GOOD) {
        DBG (5, "sane_start: ERROR: cannot ssm buffer\n");
        return ret;
      }

      ret = ssm_do(s);
      if (ret != SANE_STATUS_GOOD) {
        DBG (5, "sane_start: ERROR: cannot ssm do\n");
        return ret;
      }

      ret = ssm_df(s);
      if (ret != SANE_STATUS_GOOD) {
        DBG (5, "sane_start: ERROR: cannot ssm df\n");
        return ret;
      }
  }
  /* if already running, duplex needs to switch sides */
  else if(s->source == SOURCE_ADF_DUPLEX){
      s->side = !s->side;
  }

  /* set clean defaults with new sheet of paper */
  /* dont reset the transfer vars on backside of duplex page */
  /* otherwise buffered back page will be lost */
  /* ingest paper with adf (no-op for fb) */
  /* dont call object pos or scan on back side of duplex scan */
  if(s->side == SIDE_FRONT || s->source == SOURCE_ADF_BACK){

      ret = object_position (s, SANE_TRUE);
      if (ret != SANE_STATUS_GOOD) {
        DBG (5, "sane_start: ERROR: cannot load page\n");
        s->started=0;
        return ret;
      }

      s->bytes_rx[0]=0;
      s->bytes_rx[1]=0;
      s->lines_rx[0]=0;
      s->lines_rx[1]=0;

      s->bytes_tx[0]=0;
      s->bytes_tx[1]=0;

      /* store the number of front bytes */ 
      if ( s->source != SOURCE_ADF_BACK ){
        s->bytes_tot[SIDE_FRONT] = s->params.bytes_per_line * s->params.lines;
      }
      else{
        s->bytes_tot[SIDE_FRONT] = 0;
      }

      /* store the number of back bytes */ 
      if ( s->source == SOURCE_ADF_DUPLEX || s->source == SOURCE_ADF_BACK ){
        s->bytes_tot[SIDE_BACK] = s->params.bytes_per_line * s->params.lines;
      }
      else{
        s->bytes_tot[SIDE_BACK] = 0;
      }

      /* first page of batch: make large buffer to hold the images */
      if(!s->started){
          ret = setup_buffers(s);
          if (ret != SANE_STATUS_GOOD) {
              DBG (5, "sane_start: ERROR: cannot load buffers\n");
              return ret;
          }
      }
    
      /* first page of batch or user wants unbuffered scans */
      /* send scan command */
      if(!s->started || !s->buffermode){
          ret = start_scan (s);
          if (ret != SANE_STATUS_GOOD) {
              DBG (5, "sane_start: ERROR: cannot start_scan\n");
              return ret;
          }
      }

      /* set started flag */
      s->started=1;
  }

  /* reset jpeg params on each page */
  s->jpeg_stage=JPEG_STAGE_NONE;
  s->jpeg_ff_offset=0;

  DBG (15, "started=%d, side=%d, source=%d\n", s->started, s->side, s->source);

  DBG (10, "sane_start: finish %d\n", ret);

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
setup_buffers (struct scanner *s)
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

  DBG (10, "setup_buffers: finish\n");

  return ret;
}

/*
 * This routine issues a SCSI SET WINDOW command to the scanner, using the
 * values currently in the scanner data structure.
 */
static SANE_Status
set_window (struct scanner *s)
{
  SANE_Status ret = SANE_STATUS_GOOD;

  /* The command specifies the number of bytes in the data phase 
   * the data phase has a header, followed by 1 or 2 window desc blocks 
   * the header specifies the number of bytes in 1 window desc block
   */

  unsigned char cmd[SET_WINDOW_len];
  size_t cmdLen = SET_WINDOW_len;
 
  unsigned char out[SW_header_len + SW_desc_len];
  size_t outLen = SW_header_len + SW_desc_len;

  unsigned char * header = out;                       /*header*/
  unsigned char * desc1 = out + SW_header_len;        /*descriptor*/

  DBG (10, "set_window: start\n");

  /*build the payload*/
  memset(out,0,outLen);

  /* set window desc size in header */
  set_WPDB_wdblen(header, SW_desc_len);

  /* init the window block */
  if (s->source == SOURCE_ADF_BACK) {
    set_WD_wid (desc1, WD_wid_back);
  }
  else{
    set_WD_wid (desc1, WD_wid_front);
  }

  set_WD_Xres (desc1, s->resolution_x);
  set_WD_Yres (desc1, s->resolution_y);

  /* we have to center the window ourselves */
  set_WD_ULX (desc1, s->tl_x + (s->max_x - s->page_width) / 2);

  set_WD_ULY (desc1, s->tl_y);
  set_WD_width (desc1, s->params.pixels_per_line * 1200/s->resolution_x);
  set_WD_length (desc1, s->params.lines * 1200/s->resolution_y);

  /*convert our common -127 to +127 range into HW's range
   *FIXME: this code assumes hardware range of 0-255 */
  set_WD_brightness (desc1, s->brightness+128);

  set_WD_threshold (desc1, s->threshold);

  /*convert our common -127 to +127 range into HW's range
   *FIXME: this code assumes hardware range of 0-255 */
  set_WD_contrast (desc1, s->contrast+128);

  set_WD_composition (desc1, s->mode);

  set_WD_bitsperpixel (desc1, s->params.depth);

  if(s->mode == MODE_HALFTONE){
    /*set_WD_ht_type(desc1, s->ht_type);
    set_WD_ht_pattern(desc1, s->ht_pattern);*/
  }

  set_WD_rif (desc1, s->rif);

  /*FIXME: what is this? */
  set_WD_reserved(desc1, 1);

  set_WD_compress_type(desc1, COMP_NONE);
  set_WD_compress_arg(desc1, 0);
  /* some scanners support jpeg image compression, for color/gs only */
  if(s->params.format == SANE_FRAME_JPEG){
      set_WD_compress_type(desc1, COMP_JPEG);
      set_WD_compress_arg(desc1, s->compress_arg);
  }

  /*build the command*/
  memset(cmd,0,cmdLen);
  set_SCSI_opcode(cmd, SET_WINDOW_code);
  set_SW_xferlen(cmd, outLen);

  ret = do_cmd (
    s, 1, 0,
    cmd, cmdLen,
    out, outLen,
    NULL, NULL
  );

  if (!ret && s->source == SOURCE_ADF_DUPLEX) {
      set_WD_wid (desc1, WD_wid_back);
      ret = do_cmd (
        s, 1, 0,
        cmd, cmdLen,
        out, outLen,
        NULL, NULL
      );
  }

  DBG (10, "set_window: finish\n");

  return ret;
}

/*
 * Issues the SCSI OBJECT POSITION command if an ADF is in use.
 */
static SANE_Status
object_position (struct scanner *s, int i_load)
{
  SANE_Status ret = SANE_STATUS_GOOD;

  unsigned char cmd[OBJECT_POSITION_len];
  size_t cmdLen = OBJECT_POSITION_len;

  DBG (10, "object_position: start\n");

  if (s->source == SOURCE_FLATBED) {
    DBG (10, "object_position: flatbed no-op\n");
    return SANE_STATUS_GOOD;
  }

  memset(cmd,0,cmdLen);
  set_SCSI_opcode(cmd, OBJECT_POSITION_code);

  if (i_load) {
    DBG (15, "object_position: load\n");
    set_OP_autofeed (cmd, OP_Feed);
  }
  else {
    DBG (15, "object_position: eject\n");
    set_OP_autofeed (cmd, OP_Discharge);
  }

  ret = do_cmd (
    s, 1, 0,
    cmd, cmdLen,
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
static SANE_Status
start_scan (struct scanner *s)
{
  SANE_Status ret = SANE_STATUS_GOOD;

  unsigned char cmd[SCAN_len];
  size_t cmdLen = SCAN_len;

  unsigned char out[] = {WD_wid_front, WD_wid_back};
  size_t outLen = 2;

  DBG (10, "start_scan: start\n");

  if (s->source != SOURCE_ADF_DUPLEX) {
    outLen--;
    if(s->source == SOURCE_ADF_BACK) {
      out[0] = WD_wid_back;
    }
  }

  memset(cmd,0,cmdLen);
  set_SCSI_opcode(cmd, SCAN_code);
  set_SC_xfer_length (cmd, outLen);

  ret = do_cmd (
    s, 1, 0,
    cmd, cmdLen,
    out, outLen,
    NULL, NULL
  );

  DBG (10, "start_scan: finish\n");

  return ret;
}

/* sends cancel command to scanner, clears s->started. don't call
 * this function asyncronously, wait for scan to complete */
static SANE_Status
cancel(struct scanner *s)
{
  SANE_Status ret = SANE_STATUS_GOOD;

  unsigned char cmd[CANCEL_len];
  size_t cmdLen = CANCEL_len;

  DBG (10, "cancel: start\n");

  memset(cmd,0,cmdLen);
  set_SCSI_opcode(cmd, CANCEL_code);

  ret = do_cmd (
      s, 1, 0,
      cmd, cmdLen,
      NULL, 0,
      NULL, NULL
  );

  if(!object_position(s,SANE_FALSE)){
    DBG (5, "cancel: ignoring bad eject\n");
  }

  s->started = 0;

  DBG (10, "cancel: finish\n");

  return SANE_STATUS_CANCELLED;
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
  struct scanner *s = (struct scanner *) handle;
  SANE_Status ret=SANE_STATUS_GOOD;

  DBG (10, "sane_read: start\n");

  *len=0;

  /* maybe cancelled? */
  if(!s->started){
      DBG (5, "sane_read: not started, call sane_start\n");
      return SANE_STATUS_CANCELLED;
  }

  /* sane_start required between sides */
  if(s->bytes_tx[s->side] == s->bytes_tot[s->side]){
      DBG (15, "sane_read: returning eof\n");
      return SANE_STATUS_EOF;
  }

  /* alternating pnm interlacing */
  if(s->source == SOURCE_ADF_DUPLEX
    && s->duplex_interlace == DUPLEX_INTERLACE_ALT){

      /* buffer front side */
      if(s->bytes_tot[SIDE_FRONT] > s->bytes_rx[SIDE_FRONT]){
          ret = read_from_scanner(s, SIDE_FRONT);
          if(ret){
            DBG(5,"sane_read: front returning %d\n",ret);
            return ret;
          }
      }
    
      /* buffer back side */
      if(s->bytes_tot[SIDE_BACK] > s->bytes_rx[SIDE_BACK] ){
          ret = read_from_scanner(s, SIDE_BACK);
          if(ret){
            DBG(5,"sane_read: back returning %d\n",ret);
            return ret;
          }
      }

  } /* end alt pnm */

  /* simplex or non-alternating duplex */
  else{

    if(s->bytes_tot[s->side] > s->bytes_rx[s->side] ){
      ret = read_from_scanner(s, s->side);
      if(ret){
        DBG(5,"sane_read: side %d returning %d\n",s->side,ret);
        return ret;
      }

      /* we have finished reading, clean up */
      /* grab a bit more so scanner will send eof */
      if(s->connection == CONNECTION_USB 
        && s->bytes_rx[s->side] == s->bytes_tot[s->side]
        && (s->compress != COMP_JPEG
          || s->mode == MODE_HALFTONE
          || s->mode == MODE_LINEART)
      ){
        read_from_scanner(s, s->side);
      }
    }
  }

  /* copy a block from buffer to frontend */
  ret = read_from_buffer(s,buf,max_len,len,s->side);

  /* we've read everything, and user cancelled */
  /* tell scanner to stop */
  if(s->bytes_rx[s->side] == s->bytes_tot[s->side]
    && 
    (s->cancelled || (!read_panel(s) && s->panel_stop))
  ){
      DBG(5,"sane_read: user cancelled\n");
      return cancel(s);
  }

  DBG (10, "sane_read: finish %d\n", ret);
  return ret;
}

static SANE_Status
read_from_scanner(struct scanner *s, int side)
{
    SANE_Status ret=SANE_STATUS_GOOD;

    unsigned char cmd[READ_len];
    size_t cmdLen = READ_len;

    unsigned char * in;
    size_t inLen = 0;

    int bytes = s->buffer_size;
    int remain = s->bytes_tot[side] - s->bytes_rx[side];
    int extra = 0;
    size_t i;
  
    DBG (10, "read_from_scanner: start\n");
  
    /* figure out the max amount to transfer */
    if(bytes > remain){
        bytes = remain;
    }
  
    /* all requests must end on line boundary */
    bytes -= (bytes % s->params.bytes_per_line);

    /* some larger scanners require even bytes per block */
    if(bytes % 2){
       bytes -= s->params.bytes_per_line;
    }
  
    /* these machines always send 1 extra line, and they need to send EOF too
     * so we ask for two full lines extra, and throw them away */
    if(!bytes){
        DBG(5, "read_from_scanner: no bytes, asking for two lines\n");
        bytes = 2 * s->params.bytes_per_line;
	extra = 1;
    }
  
    DBG(15, "read_from_scanner: si:%d to:%d rx:%d re:%d bu:%d pa:%d\n", side,
      s->bytes_tot[side], s->bytes_rx[side], remain, s->buffer_size, bytes);
  
    inLen = bytes;
    in = malloc(inLen);
    if(!in){
        DBG(5, "read_from_scanner: not enough mem for buffer: %d\n",(int)inLen);
        return SANE_STATUS_NO_MEM;
    }
  
    memset(cmd,0,cmdLen);
    set_SCSI_opcode(cmd, READ_code);
    set_R_datatype_code (cmd, SR_datatype_image);
  
    set_R_xfer_length (cmd, inLen);
  
    ret = do_cmd (
      s, 1, 0,
      cmd, cmdLen,
      NULL, 0,
      in, &inLen
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
  
    /* this is jpeg data, we need to fix the missing image size */
    if(s->compress == COMP_JPEG
      && (s->mode == MODE_GRAYSCALE || s->mode == MODE_COLOR)
    ){

      /* look for the SOF header near the beginning */
      if(s->jpeg_stage == JPEG_STAGE_NONE || s->jpeg_ff_offset < 0x0d){

        for(i=0;i<inLen;i++){
    
            /* about to change stage */
            if(s->jpeg_stage == JPEG_STAGE_NONE && in[i] == 0xff){
                s->jpeg_ff_offset=0;
                continue;
            }
    
            s->jpeg_ff_offset++;

            /* last byte was an ff, this byte is SOF */
            if(s->jpeg_ff_offset == 1 && in[i] == 0xc0){
                s->jpeg_stage = JPEG_STAGE_SOF;
                continue;
            }
    
            if(s->jpeg_stage == JPEG_STAGE_SOF){

                /* lines in start of frame, overwrite it */
                if(s->jpeg_ff_offset == 5){
                    in[i] = (s->params.lines >> 8) & 0xff;
        	    continue;
                }
                if(s->jpeg_ff_offset == 6){
                    in[i] = s->params.lines & 0xff;
        	    continue;
                }
        
                /* width in start of frame, overwrite it */
                if(s->jpeg_ff_offset == 7){
                    in[i] = (s->params.pixels_per_line >> 8) & 0xff;
        	    continue;
                }
                if(s->jpeg_ff_offset == 8){
                    in[i] = s->params.pixels_per_line & 0xff;
        	    continue;
                }
            }
        }
      }
    }
    /* non-jpeg color, swap bgr to rgb, assumes full lines
    else if (s->mode == MODE_COLOR){
        unsigned char temp;
        for (i=0; i < inLen/3; i++){
            temp = in[i*3];
            in[i*3] = in[i*3+2];
            in[i*3+2] = temp;
        }
    }
		*/

    if(inLen && !extra){
        copy_buffer (s, in, inLen, side);
    }
  
    free(in);
  
    if(ret == SANE_STATUS_EOF){
      s->bytes_tot[side] = s->bytes_rx[side];
      ret = SANE_STATUS_GOOD;
    }

    DBG (10, "read_from_scanner: finish\n");
  
    return ret;
}

static SANE_Status
copy_buffer(struct scanner *s, unsigned char * buf, int len, int side)
{
  SANE_Status ret=SANE_STATUS_GOOD;

  DBG (10, "copy_buffer: start\n");

  memcpy(s->buffers[side]+s->bytes_rx[side],buf,len);
  s->bytes_rx[side] += len;

  DBG (10, "copy_buffer: finish\n");

  return ret;
}

static SANE_Status
read_from_buffer(struct scanner *s, SANE_Byte * buf,
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
  
    memcpy(buf,s->buffers[side]+s->bytes_tx[side],bytes);
    
    /* invert image if scanner needs it for this mode */
    if (s->reverse_by_mode[s->mode]){
        for ( i = 0; i < *len; i++ ) {
            buf[i] ^= 0xff;
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
  struct scanner * s = (struct scanner *) handle;

  DBG (10, "sane_cancel: start\n");
  s->cancelled = 1;
  DBG (10, "sane_cancel: finish\n");
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
  struct scanner * s = (struct scanner *) handle;

  DBG (10, "sane_close: start\n");
  /*clears any held scans*/
  disconnect_fd(s);
  DBG (10, "sane_close: finish\n");
}

static SANE_Status
disconnect_fd (struct scanner *s)
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
  struct scanner *dev, *next;

  DBG (10, "sane_exit: start\n");

  for (dev = scanner_devList; dev; dev = next) {
      disconnect_fd(dev);
      next = dev->next;
      free (dev);
  }

  if (sane_devArray)
    free (sane_devArray);

  scanner_devList = NULL;
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
  struct scanner *s = arg;
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
    case 0:
      if (ili == 1) {
        s->rs_info = get_RS_information (sensed_data);
        DBG  (5, "No sense: EOM remainder:%lu\n",(unsigned long)s->rs_info);
        return SANE_STATUS_EOF;
      }
      DBG  (5, "No sense: unknown asc/ascq\n");
      return SANE_STATUS_GOOD;

    case 1:
      if (asc == 0x37 && ascq == 0x00) {
        DBG  (5, "Recovered error: parameter rounded\n");
        return SANE_STATUS_GOOD;
      }
      DBG  (5, "Recovered error: unknown asc/ascq\n");
      return SANE_STATUS_GOOD;

    case 2:
      if (asc == 0x04 && ascq == 0x01) {
        DBG  (5, "Not ready: previous command unfinished\n");
        return SANE_STATUS_DEVICE_BUSY;
      }
      DBG  (5, "Not ready: unknown asc/ascq\n");
      return SANE_STATUS_DEVICE_BUSY;

    case 3:
      if (asc == 0x36 && ascq == 0x00) {
        DBG  (5, "Medium error: no cartridge\n");
        return SANE_STATUS_IO_ERROR;
      }
      if (asc == 0x3a && ascq == 0x00) {
        DBG  (5, "Medium error: hopper empty\n");
        return SANE_STATUS_NO_DOCS;
      }
      if (asc == 0x80 && ascq == 0x00) {
        DBG  (5, "Medium error: paper jam\n");
        return SANE_STATUS_JAMMED;
      }
      if (asc == 0x80 && ascq == 0x01) {
        DBG  (5, "Medium error: cover open\n");
        return SANE_STATUS_COVER_OPEN;
      }
      if (asc == 0x81 && ascq == 0x01) {
        DBG  (5, "Medium error: double feed\n");
        return SANE_STATUS_JAMMED;
      }
      if (asc == 0x81 && ascq == 0x02) {
        DBG  (5, "Medium error: skew detected\n");
        return SANE_STATUS_JAMMED;
      }
      if (asc == 0x81 && ascq == 0x04) {
        DBG  (5, "Medium error: staple detected\n");
        return SANE_STATUS_JAMMED;
      }
      DBG  (5, "Medium error: unknown asc/ascq\n");
      return SANE_STATUS_IO_ERROR;

    case 4:
      if (asc == 0x60 && ascq == 0x00) {
        DBG  (5, "Hardware error: lamp error\n");
        return SANE_STATUS_IO_ERROR;
      }
      if (asc == 0x80 && ascq == 0x01) {
        DBG  (5, "Hardware error: CPU check error\n");
        return SANE_STATUS_IO_ERROR;
      }
      if (asc == 0x80 && ascq == 0x02) {
        DBG  (5, "Hardware error: RAM check error\n");
        return SANE_STATUS_IO_ERROR;
      }
      if (asc == 0x80 && ascq == 0x03) {
        DBG  (5, "Hardware error: ROM check error\n");
        return SANE_STATUS_IO_ERROR;
      }
      if (asc == 0x80 && ascq == 0x04) {
        DBG  (5, "Hardware error: hardware check error\n");
        return SANE_STATUS_IO_ERROR;
      }
      DBG  (5, "Hardware error: unknown asc/ascq\n");
      return SANE_STATUS_IO_ERROR;

    case 5:
      if (asc == 0x1a && ascq == 0x00) {
        DBG  (5, "Illegal request: Parameter list error\n");
        return SANE_STATUS_INVAL;
      }
      if (asc == 0x20 && ascq == 0x00) {
        DBG  (5, "Illegal request: invalid command\n");
        return SANE_STATUS_INVAL;
      }
      if (asc == 0x24 && ascq == 0x00) {
        DBG  (5, "Illegal request: invalid CDB field\n");
        return SANE_STATUS_INVAL;
      }
      if (asc == 0x25 && ascq == 0x00) {
        DBG  (5, "Illegal request: unsupported logical unit\n");
        return SANE_STATUS_UNSUPPORTED;
      }
      if (asc == 0x26 && ascq == 0x00) {
        DBG  (5, "Illegal request: invalid field in parm list\n");
        return SANE_STATUS_INVAL;
      }
      if (asc == 0x2c && ascq == 0x00) {
        DBG  (5, "Illegal request: command sequence error\n");
        return SANE_STATUS_INVAL;
      }
      if (asc == 0x2c && ascq == 0x01) {
        DBG  (5, "Illegal request: too many windows\n");
        return SANE_STATUS_INVAL;
      }
      if (asc == 0x3a && ascq == 0x00) {
        DBG  (5, "Illegal request: no paper\n");
        return SANE_STATUS_NO_DOCS;
      }
      if (asc == 0x3d && ascq == 0x00) {
        DBG  (5, "Illegal request: invalid IDENTIFY\n");
        return SANE_STATUS_INVAL;
      }
      if (asc == 0x55 && ascq == 0x00) {
        DBG  (5, "Illegal request: scanner out of memory\n");
        return SANE_STATUS_NO_MEM;
      }
      DBG  (5, "Illegal request: unknown asc/ascq\n");
      return SANE_STATUS_IO_ERROR;
      break;

    case 6:
      if (asc == 0x29 && ascq == 0x00) {
        DBG  (5, "Unit attention: device reset\n");
        return SANE_STATUS_GOOD;
      }
      if (asc == 0x2a && ascq == 0x00) {
        DBG  (5, "Unit attention: param changed by 2nd initiator\n");
        return SANE_STATUS_GOOD;
      }
      DBG  (5, "Unit attention: unknown asc/ascq\n");
      return SANE_STATUS_IO_ERROR;
      break;

    case 7:
      DBG  (5, "Data protect: unknown asc/ascq\n");
      return SANE_STATUS_IO_ERROR;

    case 8:
      DBG  (5, "Blank check: unknown asc/ascq\n");
      return SANE_STATUS_IO_ERROR;

    case 9:
      DBG  (5, "Vendor defined: unknown asc/ascq\n");
      return SANE_STATUS_IO_ERROR;

    case 0xa:
      DBG  (5, "Copy aborted: unknown asc/ascq\n");
      return SANE_STATUS_IO_ERROR;

    case 0xb:
      if (asc == 0x00 && ascq == 0x00) {
        DBG  (5, "Aborted command: no sense/cancelled\n");
        return SANE_STATUS_CANCELLED;
      }
      if (asc == 0x45 && ascq == 0x00) {
        DBG  (5, "Aborted command: reselect failure\n");
        return SANE_STATUS_IO_ERROR;
      }
      if (asc == 0x47 && ascq == 0x00) {
        DBG  (5, "Aborted command: SCSI parity error\n");
        return SANE_STATUS_IO_ERROR;
      }
      if (asc == 0x48 && ascq == 0x00) {
        DBG  (5, "Aborted command: initiator error message\n");
        return SANE_STATUS_IO_ERROR;
      }
      if (asc == 0x49 && ascq == 0x00) {
        DBG  (5, "Aborted command: invalid message\n");
        return SANE_STATUS_IO_ERROR;
      }
      if (asc == 0x80 && ascq == 0x00) {
        DBG  (5, "Aborted command: timeout\n");
        return SANE_STATUS_IO_ERROR;
      }
      DBG  (5, "Aborted command: unknown asc/ascq\n");
      return SANE_STATUS_IO_ERROR;
      break;

    case 0xc:
      DBG  (5, "Equal: unknown asc/ascq\n");
      return SANE_STATUS_IO_ERROR;

    case 0xd:
      DBG  (5, "Volume overflow: unknown asc/ascq\n");
      return SANE_STATUS_IO_ERROR;

    case 0xe:
      if (asc == 0x3b && ascq == 0x0d) {
        DBG  (5, "Miscompare: too many docs\n");
        return SANE_STATUS_IO_ERROR;
      }
      if (asc == 0x3b && ascq == 0x0e) {
        DBG  (5, "Miscompare: too few docs\n");
        return SANE_STATUS_IO_ERROR;
      }
      DBG  (5, "Miscompare: unknown asc/ascq\n");
      return SANE_STATUS_IO_ERROR;

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
do_cmd(struct scanner *s, int runRS, int shortTime,
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
do_scsi_cmd(struct scanner *s, int runRS, int shortTime,
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

  if (inBuff && inLen){
    if(ret == SANE_STATUS_EOF){
      DBG(25, "in: short read, remainder %lu bytes\n", (u_long)s->rs_info);
      *inLen -= s->rs_info;
    }
    hexdump(30, "in: <<", inBuff, *inLen);
    DBG(25, "in: read %d bytes\n", (int)*inLen);
  }

  DBG(10, "do_scsi_cmd: finish\n");

  return ret;
}

SANE_Status
do_usb_cmd(struct scanner *s, int runRS, int shortTime,
 unsigned char * cmdBuff, size_t cmdLen,
 unsigned char * outBuff, size_t outLen,
 unsigned char * inBuff, size_t * inLen
)
{
    /*sanei_usb overwrites the transfer size,
     * so make some local copies */
    size_t usb_cmdLen = USB_HEADER_LEN + USB_COMMAND_LEN;
    size_t usb_outLen = USB_HEADER_LEN + outLen;
    size_t usb_statLen = USB_STATUS_LEN;
    size_t askLen = 0;

    /*copy the callers buffs into larger, padded ones*/
    unsigned char usb_cmdBuff[USB_HEADER_LEN + USB_COMMAND_LEN];
    unsigned char * usb_outBuff;
    unsigned char usb_statBuff[USB_STATUS_LEN];

    int cmdTime = USB_COMMAND_TIME;
    int outTime = USB_DATA_TIME;
    int inTime = USB_DATA_TIME;
    int statTime = USB_STATUS_TIME;

    int ret = 0;
    int ret2 = 0;

    DBG (10, "do_usb_cmd: start\n");

    if(shortTime){
        cmdTime = USB_COMMAND_TIME/60;
        outTime = USB_DATA_TIME/60;
        inTime = USB_DATA_TIME/60;
        statTime = USB_STATUS_TIME/60;
    }

    /* build a USB packet around the SCSI command */
    memset(&usb_cmdBuff,0,usb_cmdLen);
    usb_cmdBuff[3] = usb_cmdLen-4;
    usb_cmdBuff[5] = 1;
    usb_cmdBuff[6] = 0x90;
    memcpy(usb_cmdBuff+USB_HEADER_LEN,cmdBuff,cmdLen);

    /* change timeout */
    sanei_usb_set_timeout(cmdTime);

    /* write the command out */
    DBG(25, "cmd: writing %lu bytes, timeout %d\n", (u_long)usb_cmdLen,
        cmdTime);
    hexdump(30, "cmd: >>", usb_cmdBuff, usb_cmdLen);
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
    if(usb_cmdLen != USB_HEADER_LEN + USB_COMMAND_LEN){
        DBG(5,"cmd: wrong size %d/%d\n", USB_COMMAND_LEN, (int)usb_cmdLen);
        return SANE_STATUS_IO_ERROR;
    }

    /* this command has a write component, and a place to get it */
    if(outBuff && outLen && outTime){

        usb_outBuff = calloc(usb_outLen,1);
        if(!usb_outBuff){
            DBG(5,"out: no mem\n");
            return SANE_STATUS_IO_ERROR;
        }

        usb_outBuff[3] = usb_outLen-4;
        usb_outBuff[5] = 2;
        usb_outBuff[6] = 0xb0;
        memcpy(usb_outBuff+USB_HEADER_LEN,outBuff,outLen);

        /* change timeout */
        sanei_usb_set_timeout(outTime);

        DBG(25, "out: writing %d bytes, timeout %d\n", (int)usb_outLen, outTime);
        hexdump(30, "out: >>", usb_outBuff, usb_outLen);
        ret = sanei_usb_write_bulk(s->fd, usb_outBuff, &usb_outLen);
        DBG(25, "out: wrote %d bytes, retVal %d\n", (int)usb_outLen, ret);

        if(ret == SANE_STATUS_EOF){
            DBG(5,"out: got EOF, returning IO_ERROR\n");
            free(usb_outBuff);
            return SANE_STATUS_IO_ERROR;
        }
        if(ret != SANE_STATUS_GOOD){
            DBG(5,"out: return error '%s'\n",sane_strstatus(ret));
            free(usb_outBuff);
            return ret;
        }
        if(usb_outLen != outLen + USB_HEADER_LEN){
            DBG(5,"out: wrong size %d/%d\n", (int)outLen, (int)usb_outLen);
            free(usb_outBuff);
            return SANE_STATUS_IO_ERROR;
        }
        free(usb_outBuff);
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
            DBG(5,"in: got EOF, clearing\n");
	    return do_usb_reset(s,runRS);
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
        DBG(5,"in: got EOF, clearing\n");
	return do_usb_reset(s,runRS);
    }
    if(ret2 != SANE_STATUS_GOOD){
        DBG(5,"stat: return error '%s'\n",sane_strstatus(ret2));
        return ret2;
    }
    if(usb_statLen != USB_STATUS_LEN){
        DBG(5,"stat: wrong size %d/%d\n", USB_STATUS_LEN, (int)usb_statLen);
        return SANE_STATUS_IO_ERROR;
    }

    /* busy status
    if(usb_statBuff[USB_STATUS_OFFSET] == 8){
        DBG(25,"stat: busy\n");
        return SANE_STATUS_DEVICE_BUSY;
    }
    */

    /* if there is a non-busy status >0, try to figure out why */
    if(usb_statBuff[USB_STATUS_OFFSET] > 0
     || usb_statBuff[1] > 0
     || usb_statBuff[2] > 0
     || usb_statBuff[3] > 0){
      DBG(25,"stat: bad stat?\n");
      return SANE_STATUS_IO_ERROR;
    }

    DBG (10, "do_usb_cmd: finish\n");

    return ret;
}

SANE_Status
do_usb_reset(struct scanner *s, int runRS)
{
    SANE_Status ret, ret2;

    ret = sanei_usb_clear_halt(s->fd);
    if(ret != SANE_STATUS_GOOD){
        DBG(5,"do_usb_reset: cant clear halt, returning %d\n", ret);
        return ret;
    }

    /* caller is interested in having RS run on errors */
    if(runRS){

        unsigned char rs_cmd[REQUEST_SENSE_len];
        size_t rs_cmdLen = REQUEST_SENSE_len;

        unsigned char rs_in[RS_return_size];
        size_t rs_inLen = RS_return_size;

        memset(rs_cmd,0,rs_cmdLen);
        set_SCSI_opcode(rs_cmd, REQUEST_SENSE_code);
	set_RS_return_size(rs_cmd, rs_inLen);

        DBG(25,"rs sub call >>\n");
        ret2 = do_cmd(
          s,0,0,
          rs_cmd, rs_cmdLen,
          NULL,0,
          rs_in, &rs_inLen
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
        ret2 = sense_handler( 0, rs_in, (void *)s );

        /* this was a short read, but the usb layer did not know
        if(ret2 == SANE_STATUS_EOF && s->rs_info
          && inBuff && inLen && inTime){
            *inLen = askLen - s->rs_info;
            s->rs_info = 0;
            DBG(5,"do_usb_cmd: short read via rs, %lu/%lu\n",
              (unsigned long)*inLen,(unsigned long)askLen);
        }
	 */
        return ret2;
    }

    DBG (10, "do_usb_reset: finish\n");

    return ret;
}

static SANE_Status
wait_scanner(struct scanner *s) 
{
  SANE_Status ret = SANE_STATUS_GOOD;

  unsigned char cmd[TEST_UNIT_READY_len];
  size_t cmdLen = TEST_UNIT_READY_len;

  DBG (10, "wait_scanner: start\n");

  memset(cmd,0,cmdLen);
  set_SCSI_opcode(cmd,TEST_UNIT_READY_code);

  ret = do_cmd (
    s, 0, 1,
    cmd, cmdLen,
    NULL, 0,
    NULL, NULL
  );
  
  ret = do_cmd (
    s, 0, 1,
    cmd, cmdLen,
    NULL, 0,
    NULL, NULL
  );
  
  if (ret != SANE_STATUS_GOOD) {
    DBG(5,"WARNING: Brain-dead scanner. Hitting with stick\n");
    ret = do_cmd (
      s, 0, 1,
      cmd, cmdLen,
      NULL, 0,
      NULL, NULL
    );
  }
  if (ret != SANE_STATUS_GOOD) {
    DBG(5,"WARNING: Brain-dead scanner. Hitting with stick again\n");
    ret = do_cmd (
      s, 0, 1,
      cmd, cmdLen,
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
static int
get_page_width(struct scanner *s) 
{
  int width = s->page_width;

  /* scanner max for fb */
  if(s->source == SOURCE_FLATBED){
      return s->max_x_fb;
  }

  /* cant overscan larger than scanner max */
  if(width > s->max_x){
      return s->max_x;
  }

  /* overscan adds a margin to both sides */
  return width;
}

/* s->page_height stores the user setting
 * for the paper height in adf. sometimes,
 * we need a value that differs from this
 * due to using FB or overscan.
 */
static int
get_page_height(struct scanner *s) 
{
  int height = s->page_height;

  /* scanner max for fb */
  if(s->source == SOURCE_FLATBED){
      return s->max_y_fb;
  }

  /* cant overscan larger than scanner max */
  if(height > s->max_y){
      return s->max_y;
  }

  /* overscan adds a margin to both sides */
  return height;
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

/*
 * Prints a hex dump of the given buffer onto the debug output stream.
 */
static void
hexdump (int level, char *comment, unsigned char *p, int l)
{
  int i;
  char line[70]; /* 'xxx: xx xx ... xx xx abc */
  char *hex = line+4;
  char *bin = line+53;

  if(DBG_LEVEL < level)
    return;

  line[0] = 0;

  DBG (level, "%s\n", comment);

  for (i = 0; i < l; i++, p++) {

    /* at start of line */
    if ((i % 16) == 0) {

      /* not at start of first line, print current, reset */
      if (i) {
        DBG (level, "%s\n", line);
      }

      memset(line,0x20,69);
      line[69] = 0;
      hex = line + 4;
      bin = line + 53;

      sprintf (line, "%3.3x:", i);
    }

    /* the hex section */
    sprintf (hex, " %2.2x", *p);
    hex += 3;
    *hex = ' ';

    /* the char section */
    if(*p >= 0x20 && *p <= 0x7e){
      *bin=*p;
    }
    else{
      *bin='.';
    }
    bin++;
  }

  /* print last (partial) line */
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
