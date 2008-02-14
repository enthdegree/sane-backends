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

   This file implements a SANE backend for the Fujitsu fi-60F, the
   ScanSnap S300, and (hopefully) other Epson-based Fujitsu scanners. 
   This code is Copyright 2007 by m. allan noah <kitno455 at gmail dot com>
   and was funded by Microdea, Inc. and TrueCheck, Inc.

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
      V 1.0.0, 2007-08-08, MAN
        - initial alpha release, S300 raw data only
      V 1.0.1, 2007-09-03, MAN
        - only supports 300dpi duplex binary for S300
      V 1.0.2, 2007-09-05, MAN
        - add resolution option (only one choice)
	- add simplex option
      V 1.0.3, 2007-09-12, MAN
        - add support for 150 dpi resolution
      V 1.0.4, 2007-10-03, MAN
        - change binarization algo to use average of all channels
      V 1.0.5, 2007-10-10, MAN
        - move data blocks to separate file
        - add basic fi-60F support (600dpi color)
      V 1.0.6, 2007-11-12, MAN
        - move various data vars into transfer structs
        - move most of read_from_scanner to sane_read
	- add single line reads to calibration code
	- generate calibration buffer from above reads
      V 1.0.7, 2007-12-05, MAN
        - split calibration into fine and coarse functions
        - add S300 fine calibration code
        - add S300 color and grayscale support
      V 1.0.8, 2007-12-06, MAN
        - change sane_start to call ingest earlier
        - enable SOURCE_ADF_BACK
        - add if() around memcopy and better debugs in sane_read
        - shorten default scan sizes from 15.4 to 11.75 inches
      V 1.0.9, 2007-12-17, MAN
        - fi-60F 300 & 600 dpi support (150 is non-square?)
        - fi-60F gray & binary support
        - fi-60F improved calibration
      V 1.0.10, 2007-12-19, MAN (SANE v1.0.19)
        - fix missing function (and memory leak)
      V 1.0.11 2008-02-14, MAN
	 - sanei_config_read has already cleaned string (#310597)


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
#include "sane/sanei_usb.h"
#include "sane/saneopts.h"
#include "sane/sanei_config.h"

#include "epjitsu.h"
#include "epjitsu-cmd.h"

#define DEBUG 1
#define BUILD 11 

unsigned char global_firmware_filename[PATH_MAX];

/* values for SANE_DEBUG_EPJITSU env var:
 - errors           5
 - function trace  10
 - function detail 15
 - get/setopt cmds 20
 - usb cmd trace   25 
 - usb cmd detail  30
 - useless noise   35
*/

/* ------------------------------------------------------------------------- */
static const char string_Flatbed[] = "Flatbed";
static const char string_ADFFront[] = "ADF Front";
static const char string_ADFBack[] = "ADF Back";
static const char string_ADFDuplex[] = "ADF Duplex";

static const char string_Lineart[] = "Lineart";
static const char string_Grayscale[] = "Gray";
static const char string_Color[] = "Color";

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
    FILE *fp;
    int num_devices=0;
    int i=0;

    struct scanner *dev;
    char line[PATH_MAX];
    const char *lp;
  
    authorize = authorize;        /* get rid of compiler warning */
  
    DBG_INIT ();
    DBG (10, "sane_init: start\n");
  
    sanei_usb_init();
  
    if (version_code)
      *version_code = SANE_VERSION_CODE (V_MAJOR, V_MINOR, BUILD);
  
    DBG (5, "sane_init: epjitsu backend %d.%d.%d, from %s\n",
      V_MAJOR, V_MINOR, BUILD, PACKAGE_STRING);
  
    fp = sanei_config_open (CONFIG_FILE);
  
    if (fp) {
  
        DBG (15, "sane_init: reading config file %s\n", CONFIG_FILE);
  
        while (sanei_config_read (line, PATH_MAX, fp)) {
      
            lp = line;

            /* ignore comments */
            if (*lp == '#')
                continue;
      
            /* skip empty lines */
            if (*lp == 0)
                continue;
      
            if ((strncmp ("firmware", lp, 8) == 0) && isspace (lp[8])) {
                lp += 8;
                lp = sanei_config_skip_whitespace (lp);
                DBG (15, "sane_init: firmware '%s'\n", lp);
                strncpy((char *)global_firmware_filename,lp,PATH_MAX);
            }
            else if ((strncmp ("usb", lp, 3) == 0) && isspace (lp[3])) {
                DBG (15, "sane_init: looking for '%s'\n", lp);
                sanei_usb_attach_matching_devices(lp, attach_one);
            }
            else{
                DBG (5, "sane_init: config line \"%s\" ignored.\n", lp);
            }
        }
        fclose (fp);
    }
  
    else {
        DBG (5, "sane_init: no config file '%s'!\n",
          CONFIG_FILE);
    }
  
    for (dev = scanner_devList; dev; dev=dev->next) {
        DBG (15, "sane_init: found scanner %s\n",dev->sane.name);
        num_devices++;
    }
  
    DBG (15, "sane_init: found %d scanner(s)\n",num_devices);
  
    sane_devArray = calloc (num_devices + 1, sizeof (SANE_Device*));
    if (!sane_devArray)
        return SANE_STATUS_NO_MEM;
  
    for (dev = scanner_devList; dev; dev=dev->next) {
        sane_devArray[i++] = (SANE_Device *)&dev->sane;
    }
  
    sane_devArray[i] = 0;
 
    DBG (10, "sane_init: finish\n");
  
    return SANE_STATUS_GOOD;
}

/* callback used by sane_init
 * build the scanner struct and link to global list 
 * unless struct is already loaded, then pretend 
 */
static SANE_Status
attach_one (const char *name)
{
    struct scanner *s;
    int ret, i;
  
    DBG (10, "attach_one: start '%s'\n", name);
  
    for (s = scanner_devList; s; s = s->next) {
        if (strcmp (s->sane.name, name) == 0) {
            DBG (10, "attach_one: already attached!\n");
            return SANE_STATUS_GOOD;
        }
    }
  
    /* build a scanner struct to hold it */
    DBG (15, "attach_one: init struct\n");
  
    if ((s = calloc (sizeof (*s), 1)) == NULL)
        return SANE_STATUS_NO_MEM;
 
    /* copy the device name */
    s->sane.name = strdup (name);
    if (!s->sane.name){
        sane_close((SANE_Handle)s);
        return SANE_STATUS_NO_MEM;
    }
  
    /* connect the fd */
    DBG (15, "attach_one: connect fd\n");
  
    s->fd = -1;
    ret = connect_fd(s);
    if(ret != SANE_STATUS_GOOD){
        sane_close((SANE_Handle)s);
        return ret;
    }
 
    /* load the firmware file into scanner */
    ret = load_fw(s);
    if (ret != SANE_STATUS_GOOD) {
        sane_close((SANE_Handle)s);
        DBG (5, "attach_one: firmware load failed\n");
        return ret;
    }

    /* Now query the device to load its vendor/model/version */
    ret = get_ident(s);
    if (ret != SANE_STATUS_GOOD) {
        sane_close((SANE_Handle)s);
        DBG (5, "attach_one: identify failed\n");
        return ret;
    }

    DBG (15, "attach_one: Found %s scanner %s at %s\n",
      s->sane.vendor, s->sane.model, s->sane.name);
  
    if (strstr (s->sane.model, "S300")){
        DBG (15, "attach_one: Found S300\n");

        s->model = MODEL_S300;

        s->has_adf = 1;
        s->x_res_150 = 1;
        s->x_res_300 = 1;
        /*s->x_res_600 = 1;*/
        s->y_res_150 = 1;
        s->y_res_300 = 1;
        /*s->y_res_600 = 1;*/

        s->source = SOURCE_ADF_FRONT;
	s->mode = MODE_LINEART;
        s->resolution_x = 300;
        s->resolution_y = 300;
        s->threshold = 128;
    }

    else if (strstr (s->sane.model, "fi-60F")){
        DBG (15, "attach_one: Found fi-60F\n");

        s->model = MODEL_FI60F;

        s->has_fb = 1;
        s->x_res_150 = 0;
        s->x_res_300 = 1;
        s->x_res_600 = 1;
        s->y_res_150 = 0;
        s->y_res_300 = 1;
        s->y_res_600 = 1;

        s->source = SOURCE_FLATBED;
	s->mode = MODE_COLOR;
        s->resolution_x = 300;
        s->resolution_y = 300;
        s->threshold = 128;
    }

    else{
        DBG (15, "attach_one: Found other\n");
    }
     
    /* set SANE option 'values' to good defaults */
    DBG (15, "attach_one: init options\n");
  
    /* go ahead and setup the first opt, because 
     * frontend may call control_option on it 
     * before calling get_option_descriptor 
     */
    memset (s->opt, 0, sizeof (s->opt));
    for (i = 0; i < NUM_OPTIONS; ++i) {
        s->opt[i].name = "filler";
        s->opt[i].size = sizeof (SANE_Word);
        s->opt[i].cap = SANE_CAP_INACTIVE;
    }
  
    s->opt[OPT_NUM_OPTS].name = SANE_NAME_NUM_OPTIONS;
    s->opt[OPT_NUM_OPTS].title = SANE_TITLE_NUM_OPTIONS;
    s->opt[OPT_NUM_OPTS].desc = SANE_DESC_NUM_OPTIONS;
    s->opt[OPT_NUM_OPTS].cap = SANE_CAP_SOFT_DETECT;
  
    DBG (15, "attach_one: init settings\n");
    ret = change_params(s);

    /* we close the connection, so that another backend can talk to scanner */
    disconnect_fd(s);
  
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
  
    DBG (10, "connect_fd: start\n");
  
    if(s->fd > -1){
        DBG (5, "connect_fd: already open\n");
        ret = SANE_STATUS_GOOD;
    }
    else {
        DBG (15, "connect_fd: opening USB device\n");
        ret = sanei_usb_open (s->sane.name, &(s->fd));
    }
  
    if(ret != SANE_STATUS_GOOD){
        DBG (5, "connect_fd: could not open device: %d\n", ret);
    }
  
    DBG (10, "connect_fd: finish\n");
  
    return ret;
}

/*
 * try to load fw into scanner
 */
static SANE_Status
load_fw (struct scanner *s)
{
    SANE_Status ret = SANE_STATUS_GOOD;
    int file, i;
    int len = 0;
    unsigned char * buf;

    unsigned char cmd[4];
    size_t cmdLen;
    unsigned char stat[2];
    size_t statLen;
  
    DBG (10, "load_fw: start\n");

    /*check status*/
    cmd[0] = 0x1b;
    cmd[1] = 0x03;
    cmdLen = 2;
    statLen = 2;
    
    ret = do_cmd(
      s, 0,
      cmd, cmdLen,
      NULL, 0,
      stat, &statLen
    );
    if(ret){
        DBG (5, "load_fw: error checking status\n");
        return ret;
    }
    if(stat[0] & 0x10){
        DBG (5, "load_fw: firmware already loaded?\n");
        return SANE_STATUS_GOOD;
    }
    
    if(!global_firmware_filename[0]){
        DBG (5, "load_fw: missing filename\n");
        return SANE_STATUS_NO_DOCS;
    }

    file = open((char *)global_firmware_filename,O_RDONLY);
    if(!file){
        DBG (5, "load_fw: failed to open file %s\n",global_firmware_filename);
        return SANE_STATUS_NO_DOCS;
    }

    if(lseek(file,0x100,SEEK_SET) != 0x100){
        DBG (5, "load_fw: failed to lseek file %s\n",global_firmware_filename);
	close(file);
        return SANE_STATUS_NO_DOCS;
    }

    buf = malloc(FIRMWARE_LENGTH);
    if(!buf){
        DBG (5, "load_fw: failed to alloc mem\n");
	close(file);
        return SANE_STATUS_NO_MEM;
    }

    len = read(file,buf,FIRMWARE_LENGTH);
    close(file);

    if(len != FIRMWARE_LENGTH){
        DBG (5, "load_fw: firmware file %s wrong length\n",
          global_firmware_filename);
        free(buf);
        return SANE_STATUS_NO_DOCS;
    }

    DBG (15, "load_fw: read firmware file %s ok\n", global_firmware_filename);

    /* firmware upload is in three commands */

    /*start/status*/
    cmd[0] = 0x1b;
    cmd[1] = 0x06;
    cmdLen = 2;
    statLen = 1;
    
    ret = do_cmd(
      s, 0,
      cmd, cmdLen,
      NULL, 0,
      stat, &statLen
    );
    if(ret){
        DBG (5, "load_fw: error on cmd 1\n");
        free(buf);
        return ret;
    }
    if(stat[0] != 6){
        DBG (5, "load_fw: bad stat on cmd 1\n");
        free(buf);
        return SANE_STATUS_IO_ERROR;
    }
    
    /*length/data*/
    cmd[0] = 0x01;
    cmd[1] = 0x00;
    cmd[2] = 0x01;
    cmd[3] = 0x00;
    cmdLen = 4;
    
    ret = do_cmd(
      s, 0,
      cmd, cmdLen,
      buf, FIRMWARE_LENGTH,
      NULL, 0
    );
    if(ret){
        DBG (5, "load_fw: error on cmd 2\n");
        free(buf);
        return ret;
    }

    /*checksum/status*/
    cmd[0] = 0;
    for(i=0;i<FIRMWARE_LENGTH;i++){
        cmd[0] += buf[i];
    }
    free(buf);

    cmdLen = 1;
    statLen = 1;
    
    ret = do_cmd(
      s, 0,
      cmd, cmdLen,
      NULL, 0,
      stat, &statLen
    );
    if(ret){
        DBG (5, "load_fw: error on cmd 3\n");
        return ret;
    }
    if(stat[0] != 6){
        DBG (5, "load_fw: bad stat on cmd 3\n");
        return SANE_STATUS_IO_ERROR;
    }
    
    /*reinit*/
    cmd[0] = 0x1b;
    cmd[1] = 0x16;
    cmdLen = 2;
    statLen = 1;
    
    ret = do_cmd(
      s, 0,
      cmd, cmdLen,
      NULL, 0,
      stat, &statLen
    );
    if(ret){
        DBG (5, "load_fw: error reinit cmd\n");
        return ret;
    }
    if(stat[0] != 6){
        DBG (5, "load_fw: reinit cmd bad status?\n");
        return SANE_STATUS_IO_ERROR;
    }

    cmd[0] = 0x80;
    cmdLen = 1;
    statLen = 1;
    
    ret = do_cmd(
      s, 0,
      cmd, cmdLen,
      NULL, 0,
      stat, &statLen
    );
    if(ret){
        DBG (5, "load_fw: error reinit payload\n");
        return ret;
    }
    if(stat[0] != 6){
        DBG (5, "load_fw: reinit payload bad status?\n");
        return SANE_STATUS_IO_ERROR;
    }

    /*check status*/
    cmd[0] = 0x1b;
    cmd[1] = 0x03;
    cmdLen = 2;
    statLen = 2;
    
    ret = do_cmd(
      s, 0,
      cmd, cmdLen,
      NULL, 0,
      stat, &statLen
    );
    if(ret){
        DBG (5, "load_fw: error rechecking status\n");
        return ret;
    }
    if(!(stat[0] & 0x10)){
        DBG (5, "load_fw: firmware not loaded? %#x\n",stat[0]);
        return SANE_STATUS_IO_ERROR;
    }
    
    return ret;
}

static SANE_Status
get_ident(struct scanner *s)
{
    int i;
    SANE_Status ret;

    unsigned char cmd[] = {0x1b,0x13};
    size_t cmdLen = 2;
    unsigned char in[0x20];
    size_t inLen = sizeof(in);

    DBG (10, "get_ident: start\n");

    ret = do_cmd (
      s, 0,
      cmd, cmdLen,
      NULL, 0,
      in, &inLen
    );
  
    if (ret != SANE_STATUS_GOOD){
      return ret;
    }

    /*hmm, similar to scsi?*/
    for (i = 7; (in[i] == ' ' || in[i] == 0xff) && i >= 0; i--){
        in[i] = 0;
    }
    s->sane.vendor = strndup((char *)in, 8);

    for (i = 23; (in[i] == ' ' || in[i] == 0xff) && i >= 8; i--){
        in[i] = 0;
    }
    s->sane.model= strndup((char *)in+8, 24);

    s->sane.type = "scanner";
  
    DBG (10, "get_ident: finish\n");
    return ret;
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
 *
 * Read the config file, find scanners with help from sanei_*
 * store in global device structs
 */
SANE_Status
sane_get_devices (const SANE_Device *** device_list, SANE_Bool local_only)
{
    local_only = local_only;        /* get rid of compiler warning */
  
    DBG (10, "sane_get_devices: start\n");
  
    /*FIXME: rebuild this list every time*/
    *device_list = sane_devArray;
  
    DBG (10, "sane_get_devices: finish\n");
  
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
  
    if(name[0] == 0){
        if(scanner_devList){
            DBG (15, "sane_open: no device requested, using first\n");
            s = scanner_devList;
        }
        else{
            DBG (15, "sane_open: no device requested, none found\n");
        }
    }
    else{
        DBG (15, "sane_open: device %s requested, attaching\n", name);

        for (dev = scanner_devList; dev; dev = dev->next) {
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
  struct scanner *s = handle;
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
  else if(option==OPT_SOURCE){
    i=0;
    if(s->has_fb){
      s->source_list[i++]=string_Flatbed;
    }
    if(s->has_adf){
      s->source_list[i++]=string_ADFFront;
      s->source_list[i++]=string_ADFBack;
      s->source_list[i++]=string_ADFDuplex;
    }
    s->source_list[i]=NULL;

    opt->name = SANE_NAME_SCAN_SOURCE;
    opt->title = SANE_TITLE_SCAN_SOURCE;
    opt->desc = SANE_DESC_SCAN_SOURCE;
    opt->type = SANE_TYPE_STRING;
    opt->constraint_type = SANE_CONSTRAINT_STRING_LIST;
    opt->constraint.string_list = s->source_list;
    opt->size = maxStringSize (opt->constraint.string_list);
    if(i > 1){
      opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    }
  }

  /* scan mode */
  else if(option==OPT_MODE){
    i=0;
    s->mode_list[i++]=string_Lineart;
    s->mode_list[i++]=string_Grayscale;
    s->mode_list[i++]=string_Color;
    s->mode_list[i]=NULL;
  
    opt->name = SANE_NAME_SCAN_MODE;
    opt->title = SANE_TITLE_SCAN_MODE;
    opt->desc = SANE_DESC_SCAN_MODE;
    opt->type = SANE_TYPE_STRING;
    opt->constraint_type = SANE_CONSTRAINT_STRING_LIST;
    opt->constraint.string_list = s->mode_list;
    opt->size = maxStringSize (opt->constraint.string_list);
    if(i > 1){
      opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    }
  }

  else if(option==OPT_X_RES){
    i=0;
    if(s->x_res_150){
      s->x_res_list[++i] = 150;
    }
    if(s->x_res_300){
      s->x_res_list[++i] = 300;
    }
    if(s->x_res_600){
      s->x_res_list[++i] = 600;
    }
    s->x_res_list[0] = i;

    opt->name = SANE_NAME_SCAN_X_RESOLUTION;
    opt->title = SANE_TITLE_SCAN_X_RESOLUTION;
    opt->desc = SANE_DESC_SCAN_X_RESOLUTION;
    opt->type = SANE_TYPE_INT;
    opt->unit = SANE_UNIT_DPI;
    if(i > 1){
      opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    }

    opt->constraint_type = SANE_CONSTRAINT_WORD_LIST;
    opt->constraint.word_list = s->x_res_list;
  }

  else if(option==OPT_Y_RES){
    i=0;
    if(s->y_res_150){
      s->y_res_list[++i] = 150;
    }
    if(s->y_res_300){
      s->y_res_list[++i] = 300;
    }
    if(s->y_res_600){
      s->y_res_list[++i] = 600;
    }
    s->y_res_list[0] = i;

    opt->name = SANE_NAME_SCAN_Y_RESOLUTION;
    opt->title = SANE_TITLE_SCAN_Y_RESOLUTION;
    opt->desc = SANE_DESC_SCAN_Y_RESOLUTION;
    opt->type = SANE_TYPE_INT;
    opt->unit = SANE_UNIT_DPI;
    opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;

    opt->constraint_type = SANE_CONSTRAINT_WORD_LIST;
    opt->constraint.word_list = s->y_res_list;
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
          else{
            DBG(5,"missing option val for source\n");
          }
          return SANE_STATUS_GOOD;

        case OPT_MODE:
          if(s->mode == MODE_LINEART){
            strcpy (val, string_Lineart);
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
          return change_params(s);

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
          return change_params(s);

        case OPT_Y_RES:

          if (s->resolution_y == val_c)
              return SANE_STATUS_GOOD;

          s->resolution_y = val_c;

          *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
          return change_params(s);

      }                       /* switch */
  }                           /* else */

  return SANE_STATUS_INVAL;
}

/* add extra bytes to total because of block trailer */
void update_block_totals(struct scanner * s)
{
    s->block.total_pix = s->block.width_pix * s->block.height;
    s->block.total_bytes = s->block.width_bytes * s->block.height + 8;
}

/*
 * clean up scanner struct vals when user changes mode, res, etc
 */
static SANE_Status
change_params(struct scanner *s)
{
    SANE_Status ret = SANE_STATUS_GOOD;
  
    DBG (10, "change_params: start\n");
  
    if (s->model == MODEL_S300){

        if(s->resolution_x == 150){
            /*s->scan.height = 2317;*/
            s->scan.height = 1762;
            s->scan.width_pix = 4256;
            s->block.height = 41;
            s->front.width_pix = 1296;

            s->req_width = 1480;
            s->head_width = 1296;
            s->pad_width = 184;

            /* cal reads at 300 dpi? */
            s->coarsecal.width_pix = 8512;

	    s->setWindowCoarseCal = setWindowCoarseCal_S300_150;
	    s->setWindowCoarseCalLen = sizeof(setWindowCoarseCal_S300_150);
	    
	    s->setWindowFineCal = setWindowFineCal_S300_150;
	    s->setWindowFineCalLen = sizeof(setWindowFineCal_S300_150);
	    
	    s->setWindowSendCal = setWindowSendCal_S300_150;
	    s->setWindowSendCalLen = sizeof(setWindowSendCal_S300_150);
	    
	    s->sendCal1Header = sendCal1Header_S300_150;
	    s->sendCal1HeaderLen = sizeof(sendCal1Header_S300_150);

	    s->sendCal2Header = sendCal2Header_S300_150;
	    s->sendCal2HeaderLen = sizeof(sendCal2Header_S300_150);

	    s->setWindowScan = setWindowScan_S300_150;
	    s->setWindowScanLen = sizeof(setWindowScan_S300_150);
        }
        else if(s->resolution_x == 300){
            /*s->scan.height = 4632;*/
            s->scan.height = 3524;
            s->scan.width_pix = 8192;
            s->block.height = 21;
            s->front.width_pix = 2592;

            s->req_width = 2800;
            s->head_width = 2592;
            s->pad_width = 208;

            s->coarsecal.width_pix = 0x2000;

	    s->setWindowCoarseCal = setWindowCoarseCal_S300_300;
	    s->setWindowCoarseCalLen = sizeof(setWindowCoarseCal_S300_300);
	    
	    s->setWindowFineCal = setWindowFineCal_S300_300;
	    s->setWindowFineCalLen = sizeof(setWindowFineCal_S300_300);
	    
	    s->setWindowSendCal = setWindowSendCal_S300_300;
	    s->setWindowSendCalLen = sizeof(setWindowSendCal_S300_300);
	    
	    s->sendCal1Header = sendCal1Header_S300_300;
	    s->sendCal1HeaderLen = sizeof(sendCal1Header_S300_300);

	    s->sendCal2Header = sendCal2Header_S300_300;
	    s->sendCal2HeaderLen = sizeof(sendCal2Header_S300_300);

	    s->setWindowScan = setWindowScan_S300_300;
	    s->setWindowScanLen = sizeof(setWindowScan_S300_300);
        }
#if 0
no actual 600 dpi support?
        else if(s->resolution_x == 600){
            s->scan.height = 9249;
            s->scan.width_pix = 16064;
            s->block.height = 10;
            s->front.width_pix = 5000;

            s->req_width = 5440;
            s->head_width = 5000;
            s->pad_width = 440;

	    s->setWindowCoarseCal = setWindowCoarseCal_S300_600;
	    s->setWindowCoarseCalLen = sizeof(setWindowCoarseCal_S300_600);
	    
	    s->setWindowFineCal = setWindowFineCal_S300_600;
	    s->setWindowFineCalLen = sizeof(setWindowFineCal_S300_600);
	    
	    s->setWindowSendCal = setWindowSendCal_S300_600;
	    s->setWindowSendCalLen = sizeof(setWindowSendCal_S300_600);
	    
	    s->sendCal1Header = sendCal1Header_S300_600;
	    s->sendCal1HeaderLen = sizeof(sendCal1Header_S300_600);

	    s->sendCal2Header = sendCal2Header_S300_600;
	    s->sendCal2HeaderLen = sizeof(sendCal2Header_S300_600);

	    s->setWindowScan = setWindowScan_S300_600;
	    s->setWindowScanLen = sizeof(setWindowScan_S300_600);
        }
#endif
	else{
            DBG (5, "change_params: unsupported res\n");
            ret = SANE_STATUS_INVAL;
	}

        /* fill in scan settings */
        /* S300 always scans in color? */
        s->scan.width_bytes = s->scan.width_pix * 3;

        /* cal is always color? */
        s->coarsecal.height = 1;
        s->coarsecal.width_bytes = s->coarsecal.width_pix * 3;

        s->darkcal.height = 16;
        s->darkcal.width_pix = s->coarsecal.width_pix;
        s->darkcal.width_bytes = s->coarsecal.width_bytes;

        s->lightcal.height = 16;
        s->lightcal.width_pix = s->coarsecal.width_pix;
        s->lightcal.width_bytes = s->coarsecal.width_bytes;

        /* 2 bytes per channel (gain&offset) */
        s->sendcal.height = 1;
        s->sendcal.width_pix = s->coarsecal.width_pix;
        s->sendcal.width_bytes = s->coarsecal.width_bytes * 2;
    }

    else if (s->model == MODEL_FI60F){

        if(s->resolution_x == 150){
            s->scan.height = 1750;
            s->scan.width_pix = 1200;
            s->block.height = 41;
            s->front.width_pix = 1296;

            s->req_width = 1480;
            /*
            s->head_width = 864;
            s->pad_width = 114;
            */

            s->coarsecal.width_pix = 1200;

	    s->setWindowCoarseCal = setWindowCoarseCal_FI60F_150;
	    s->setWindowCoarseCalLen = sizeof(setWindowCoarseCal_FI60F_150);
	    
	    s->setWindowFineCal = setWindowFineCal_FI60F_150;
	    s->setWindowFineCalLen = sizeof(setWindowFineCal_FI60F_150);
	    
	    s->setWindowSendCal = setWindowSendCal_FI60F_150;
	    s->setWindowSendCalLen = sizeof(setWindowSendCal_FI60F_150);
	    
	    s->sendCal1Header = sendCal1Header_FI60F_150;
	    s->sendCal1HeaderLen = sizeof(sendCal1Header_FI60F_150);

	    s->sendCal2Header = sendCal2Header_FI60F_150;
	    s->sendCal2HeaderLen = sizeof(sendCal2Header_FI60F_150);

	    s->setWindowScan = setWindowScan_FI60F_150;
	    s->setWindowScanLen = sizeof(setWindowScan_FI60F_150);
        }
	/* FIXME: too short? */
        else if(s->resolution_x == 300){
            s->scan.height = 1749;
            s->scan.width_pix = 2400;
            s->block.height = 72;
            s->front.width_pix = 1296;

            s->req_width = 2400;
            s->head_width = 432;
            s->pad_width = 526;

            s->coarsecal.width_pix = 2400;

	    s->setWindowCoarseCal = setWindowCoarseCal_FI60F_300;
	    s->setWindowCoarseCalLen = sizeof(setWindowCoarseCal_FI60F_300);
	    
	    s->setWindowFineCal = setWindowFineCal_FI60F_300;
	    s->setWindowFineCalLen = sizeof(setWindowFineCal_FI60F_300);
	    
	    s->setWindowSendCal = setWindowSendCal_FI60F_300;
	    s->setWindowSendCalLen = sizeof(setWindowSendCal_FI60F_300);
	    
	    s->sendCal1Header = sendCal1Header_FI60F_300;
	    s->sendCal1HeaderLen = sizeof(sendCal1Header_FI60F_300);

	    s->sendCal2Header = sendCal2Header_FI60F_300;
	    s->sendCal2HeaderLen = sizeof(sendCal2Header_FI60F_300);

	    s->setWindowScan = setWindowScan_FI60F_300;
	    s->setWindowScanLen = sizeof(setWindowScan_FI60F_300);
        }
	/* FIXME: too short? */
        else if(s->resolution_x == 600){
            s->scan.height = 3498;
            s->scan.width_pix = 2848;
            s->block.height = 61;
            s->front.width_pix = 2592;

            s->req_width = 2848;
            s->head_width = 864;
            s->pad_width = 114;

            s->coarsecal.width_pix = 2848;

	    s->setWindowCoarseCal = setWindowCoarseCal_FI60F_600;
	    s->setWindowCoarseCalLen = sizeof(setWindowCoarseCal_FI60F_600);
	    
	    s->setWindowFineCal = setWindowFineCal_FI60F_600;
	    s->setWindowFineCalLen = sizeof(setWindowFineCal_FI60F_600);
	    
	    s->setWindowSendCal = setWindowSendCal_FI60F_600;
	    s->setWindowSendCalLen = sizeof(setWindowSendCal_FI60F_600);
	    
	    s->sendCal1Header = sendCal1Header_FI60F_600;
	    s->sendCal1HeaderLen = sizeof(sendCal1Header_FI60F_600);

	    s->sendCal2Header = sendCal2Header_FI60F_600;
	    s->sendCal2HeaderLen = sizeof(sendCal2Header_FI60F_600);

	    s->setWindowScan = setWindowScan_FI60F_600;
	    s->setWindowScanLen = sizeof(setWindowScan_FI60F_600);
        }
	else{
            DBG (5, "change_params: unsupported res\n");
            ret = SANE_STATUS_INVAL;
	}

	/*gray or binary scans in 8 bit gray*/
        if (s->mode == MODE_COLOR) {
            s->scan.width_bytes = s->scan.width_pix*3;
	}
        else{
            s->scan.width_bytes = s->scan.width_pix*3;
        }

	/* FIXME: is this always in color? */
        s->coarsecal.height = 1;
        s->coarsecal.width_pix = s->scan.width_pix;
        s->coarsecal.width_bytes = s->scan.width_bytes;

        s->darkcal.height = 16;
        s->darkcal.width_pix = s->scan.width_pix;
        s->darkcal.width_bytes = s->scan.width_bytes;

        s->lightcal.height = 16;
        s->lightcal.width_pix = s->scan.width_pix;
        s->lightcal.width_bytes = s->scan.width_bytes;

        /* 2 bytes per channel (gain&offset) */
        s->sendcal.height = 1;
        s->sendcal.width_pix = s->scan.width_pix * 2;
        s->sendcal.width_bytes = s->scan.width_bytes * 2;
    }
    else{
        DBG (5, "change_params: cant handle this scanner\n");
        ret = SANE_STATUS_INVAL;
    }
     
    /* fill in scan settings */
    /* add extra bytes to total because of block trailers */
    s->scan.total_pix = s->scan.width_pix * s->scan.height;
    s->scan.total_bytes = s->scan.width_bytes * s->scan.height;
    s->scan.total_bytes += (s->scan.height/s->block.height*8);
    if(s->scan.height % s->block.height){
        s->scan.total_bytes += 8;
    }

    /* fill in cal settings */
    /* add extra bytes to total because of block trailer */
    s->coarsecal.total_pix = s->coarsecal.width_pix * s->coarsecal.height;
    s->coarsecal.total_bytes = s->coarsecal.width_bytes * s->coarsecal.height + 8;

    s->darkcal.total_pix = s->darkcal.width_pix * s->darkcal.height;
    s->darkcal.total_bytes = s->darkcal.width_bytes * s->darkcal.height + 8;

    s->lightcal.total_pix = s->lightcal.width_pix * s->lightcal.height;
    s->lightcal.total_bytes = s->lightcal.width_bytes * s->lightcal.height + 8;

    s->sendcal.total_pix = s->sendcal.width_pix * s->sendcal.height;
    s->sendcal.total_bytes = s->sendcal.width_bytes * s->sendcal.height;

    /* fill in block settings, function so we can call it elsewhere */
    s->block.width_bytes = s->scan.width_bytes;
    s->block.width_pix = s->scan.width_pix;
    update_block_totals(s);

    /* fill in front settings */
    switch (s->mode) {
      case MODE_COLOR:
        s->front.width_bytes = s->front.width_pix*3;
        break;
      case MODE_GRAYSCALE:
        s->front.width_bytes = s->front.width_pix;
        break;
      default: /*binary*/
        s->front.width_bytes = s->front.width_pix/8;
        break;
    }
    s->front.height = s->scan.height;
    s->front.total_pix = s->front.width_pix * s->front.height;
    s->front.total_bytes = s->front.width_bytes * s->front.height;

    /* back settings always same as front settings */
    if(s->source == SOURCE_ADF_DUPLEX || s->source == SOURCE_ADF_BACK){
        s->back.height = s->front.height;
        s->back.width_pix = s->front.width_pix;
        s->back.width_bytes = s->front.width_bytes;
        s->back.total_pix = s->front.total_pix;
        s->back.total_bytes = s->front.total_bytes;
    }

    DBG (10, "change_params: finish\n");
  
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
  struct scanner *s = (struct scanner *) handle;

  DBG (10, "sane_get_parameters: start\n");

  params->pixels_per_line = s->front.width_pix;
  params->bytes_per_line = s->front.width_bytes;
  params->lines = s->scan.height;
  params->last_frame = 1;

  if (s->mode == MODE_COLOR) {
    params->format = SANE_FRAME_RGB;
    params->depth = 8;
  }
  else if (s->mode == MODE_GRAYSCALE) {
    params->format = SANE_FRAME_GRAY;
    params->depth = 8;
  }
  else if (s->mode == MODE_LINEART) {
    params->format = SANE_FRAME_GRAY;
    params->depth = 1;
  }

  DBG (15, "\tdepth %d\n", params->depth);
  DBG (15, "\tlines %d\n", params->lines);
  DBG (15, "\tpixels_per_line %d\n", params->pixels_per_line);
  DBG (15, "\tbytes_per_line %d\n", params->bytes_per_line);

  DBG (10, "sane_get_parameters: finish\n");

  return SANE_STATUS_GOOD;
}

/*
 * Called by SANE when a page acquisition operation is to be started.
 * FIXME: wont handle SOURCE_ADF_BACK
 */
SANE_Status
sane_start (SANE_Handle handle)
{
    struct scanner *s = handle;
    SANE_Status ret;
  
    DBG (10, "sane_start: start\n");
  
    /* start next image */
    s->send_eof=0;

    /* set side marker on first page */
    if(!s->started){
      if(s->source == SOURCE_ADF_BACK){
        s->side = SIDE_BACK;
      }
      else{
        s->side = SIDE_FRONT;
      }
    }
    /* if already running, duplex needs to switch sides */
    else if(s->source == SOURCE_ADF_DUPLEX){
        s->side = !s->side;
    }

    /* ingest paper with adf */
    if( s->source == SOURCE_ADF_BACK || s->source == SOURCE_ADF_FRONT
     || (s->source == SOURCE_ADF_DUPLEX && s->side == SIDE_FRONT) ){
        ret = ingest(s);
        if (ret != SANE_STATUS_GOOD) {
            DBG (5, "sane_start: ERROR: failed to ingest\n");
            sane_cancel((SANE_Handle)s);
            return ret;
        }
    }

    /* first page requires buffers, etc */
    if(!s->started){

        DBG(15,"sane_start: first page\n");

        s->started=1;

        ret = teardown_buffers(s);
        if (ret != SANE_STATUS_GOOD) {
            DBG (5, "sane_start: ERROR: failed to teardown buffers\n");
            sane_cancel((SANE_Handle)s);
            return SANE_STATUS_NO_MEM;
        }

        ret = change_params(s);
        if (ret != SANE_STATUS_GOOD) {
            DBG (5, "sane_start: ERROR: failed to change_params\n");
            sane_cancel((SANE_Handle)s);
            return SANE_STATUS_NO_MEM;
        }

        ret = setup_buffers(s);
        if (ret != SANE_STATUS_GOOD) {
            DBG (5, "sane_start: ERROR: failed to setup buffers\n");
            sane_cancel((SANE_Handle)s);
            return SANE_STATUS_NO_MEM;
        }

        ret = coarsecal(s);
        if (ret != SANE_STATUS_GOOD) {
            DBG (5, "sane_start: ERROR: failed to coarsecal\n");
            sane_cancel((SANE_Handle)s);
            return ret;
        }
      
        ret = finecal(s);
        if (ret != SANE_STATUS_GOOD) {
            DBG (5, "sane_start: ERROR: failed to finecal\n");
            sane_cancel((SANE_Handle)s);
            return ret;
        }
      
        ret = lamp(s,1);
        if (ret != SANE_STATUS_GOOD) {
            DBG (5, "sane_start: ERROR: failed to heat lamp\n");
            sane_cancel((SANE_Handle)s);
            return ret;
        }
      
        /*should this be between each page*/
        ret = set_window(s,WINDOW_SCAN);
        if (ret != SANE_STATUS_GOOD) {
            DBG (5, "sane_start: ERROR: failed to set window\n");
            sane_cancel((SANE_Handle)s);
            return ret;
        }
  
    }

    /* reset everything when starting any front, or just back */
    if(s->side == SIDE_FRONT || s->source == SOURCE_ADF_BACK){

        DBG(15,"sane_start: reset counters\n");

        /* reset scan */
	s->scan.rx_bytes = 0;
	s->scan.tx_bytes = 0;
    
        /* reset block */
	s->block.rx_bytes = 0;
	s->block.tx_bytes = 0;

        /* reset front */
	s->front.rx_bytes = 0;
	s->front.tx_bytes = 0;

        /* reset back */
	s->back.rx_bytes = 0;
	s->back.tx_bytes = 0;

        ret = scan(s);
        if (ret != SANE_STATUS_GOOD) {
            DBG (5, "sane_start: ERROR: failed to start scan\n");
            sane_cancel((SANE_Handle)s);
            return ret;
        }
    }
    else{
        DBG(15,"sane_start: back side\n");
    }

    DBG (10, "sane_start: finish\n");
  
    return SANE_STATUS_GOOD;
}

static SANE_Status
setup_buffers(struct scanner *s)
{
    SANE_Status ret = SANE_STATUS_GOOD;

    DBG (10, "setup_buffers: start\n");

    /* temporary cal data */
    s->coarsecal.buffer = calloc (1,s->coarsecal.total_bytes);
    if(!s->coarsecal.buffer){
        DBG (5, "setup_buffers: ERROR: failed to setup coarse cal buffer\n");
        return SANE_STATUS_NO_MEM;
    }

    s->darkcal.buffer = calloc (1,s->darkcal.total_bytes);
    if(!s->darkcal.buffer){
        DBG (5, "setup_buffers: ERROR: failed to setup fine cal buffer\n");
        return SANE_STATUS_NO_MEM;
    }

    s->lightcal.buffer = calloc (1,s->lightcal.total_bytes);
    if(!s->lightcal.buffer){
        DBG (5, "setup_buffers: ERROR: failed to setup fine cal buffer\n");
        return SANE_STATUS_NO_MEM;
    }

    s->sendcal.buffer = calloc (1,s->sendcal.total_bytes);
    if(!s->sendcal.buffer){
        DBG (5, "setup_buffers: ERROR: failed to setup send cal buffer\n");
        return SANE_STATUS_NO_MEM;
    }

    /* grab up to 512K at a time */
    s->block.buffer = calloc (1,s->block.total_bytes);
    if(!s->block.buffer){
        DBG (5, "setup_buffers: ERROR: failed to setup block buffer\n");
        return SANE_STATUS_NO_MEM;
    }

    /* make image buffer to hold frontside data */
    if(s->source != SOURCE_ADF_BACK){
        s->front.buffer = calloc (1,s->front.total_bytes);
        if(!s->front.buffer){
            DBG (5, "setup_buffers: ERROR: failed to setup front buffer\n");
            return SANE_STATUS_NO_MEM;
        }
    }

    /* make image buffer to hold backside data */
    if(s->source == SOURCE_ADF_DUPLEX || s->source == SOURCE_ADF_BACK){
        s->back.buffer = calloc (1,s->back.total_bytes);
        if(!s->back.buffer){
            DBG (5, "setup_buffers: ERROR: failed to setup back buffer\n");
            return SANE_STATUS_NO_MEM;
        }
    }

    DBG (10, "setup_buffers: finish\n");
    return ret;
}

/*
 coarse calibration consists of:
 1. turn lamp off (d0)
 2. set window for single line of data (d1)
 3. get line (d2)
 4. update dark coarse cal (c6)
 5. return to #3 if not dark enough
 6. turn lamp on (d0)
 7. get line (d2)
 8. update light coarse cal (c6)
 9. return to #7 if not light enough
*/

static SANE_Status
coarsecal(struct scanner *s)
{
    SANE_Status ret = SANE_STATUS_GOOD;

    size_t cmdLen = 2;
    unsigned char cmd[2];

    size_t statLen = 1;
    unsigned char stat[1];

    size_t payLen = 28;
    unsigned char pay[28];

    /*size_t i;*/
    int try = 0, direction = -1;

    DBG (10, "coarsecal: start\n");

    if(s->model == MODEL_S300){
        memcpy(pay,coarseCalData_S300,payLen);
    }
    else{
        memcpy(pay,coarseCalData_FI60F,payLen);
    }

    /* ask for 1 line */
    ret = set_window(s, WINDOW_COARSECAL);
    if(ret){
        DBG (5, "coarsecal: error sending setwindow\n");
        return ret;
    }

    /* dark cal, lamp off */
    lamp(s,0);

    while(try++ < 1){

        /* send coarse cal (c6) */
        cmd[0] = 0x1b;
        cmd[1] = 0xc6;
        stat[0] = 0;
        statLen = 1;
    
        ret = do_cmd(
          s, 0,
          cmd, cmdLen,
          NULL, 0,
          stat, &statLen
        );
        if(ret){
            DBG (5, "coarsecal: error sending c6 cmd\n");
            return ret;
        }
        if(stat[0] != 6){
            DBG (5, "coarsecal: cmd bad c6 status?\n");
            return SANE_STATUS_IO_ERROR;
        }
    
        /*send coarse cal payload*/
        stat[0] = 0;
        statLen = 1;
    
        ret = do_cmd(
          s, 0,
          pay, payLen,
          NULL, 0,
          stat, &statLen
        );
        if(ret){
            DBG (5, "coarsecal: error sending c6 payload\n");
            return ret;
        }
        if(stat[0] != 6){
            DBG (5, "coarsecal: c6 payload bad status?\n");
            return SANE_STATUS_IO_ERROR;
        }

        /* send scan d2 command */
        cmd[0] = 0x1b;
        cmd[1] = 0xd2;
        stat[0] = 0;
        statLen = 1;
    
        ret = do_cmd(
          s, 0,
          cmd, cmdLen,
          NULL, 0,
          stat, &statLen
        );
        if(ret){
            DBG (5, "coarsecal: error sending d2 cmd\n");
            return ret;
        }
        if(stat[0] != 6){
            DBG (5, "coarsecal: cmd bad d2 status?\n");
            return SANE_STATUS_IO_ERROR;
        }

        s->coarsecal.rx_bytes = 0;
        s->coarsecal.tx_bytes = 0;
    
        while(s->coarsecal.rx_bytes < s->coarsecal.total_bytes){
            ret = read_from_scanner(s,&s->coarsecal);
            if(ret){
                DBG (5, "coarsecal: cant read from scanner\n");
                return ret;
            }
        }

        /* FIXME inspect data, change coarse cal data */
        if(s->model == MODEL_S300){
            pay[5] += direction;
            pay[7] += direction;
        }
        else{
            pay[5] += direction;
            pay[7] += direction;
            pay[9] += direction;
        }

        hexdump(15, "c6 payload: ", pay, payLen);
    }

    /* light cal, lamp on */
    lamp(s,1);

    try = 0;
    direction = -1;

    while(try++ < 1){

        /* send coarse cal (c6) */
        cmd[0] = 0x1b;
        cmd[1] = 0xc6;
        stat[0] = 0;
        statLen = 1;
    
        ret = do_cmd(
          s, 0,
          cmd, cmdLen,
          NULL, 0,
          stat, &statLen
        );
        if(ret){
            DBG (5, "coarsecal: error sending c6 cmd\n");
            return ret;
        }
        if(stat[0] != 6){
            DBG (5, "coarsecal: cmd bad c6 status?\n");
            return SANE_STATUS_IO_ERROR;
        }
    
        /*send coarse cal payload*/
        stat[0] = 0;
        statLen = 1;
    
        ret = do_cmd(
          s, 0,
          pay, payLen,
          NULL, 0,
          stat, &statLen
        );
        if(ret){
            DBG (5, "coarsecal: error sending c6 payload\n");
            return ret;
        }
        if(stat[0] != 6){
            DBG (5, "coarsecal: c6 payload bad status?\n");
            return SANE_STATUS_IO_ERROR;
        }

        /* send scan d2 command */
        cmd[0] = 0x1b;
        cmd[1] = 0xd2;
        stat[0] = 0;
        statLen = 1;
    
        ret = do_cmd(
          s, 0,
          cmd, cmdLen,
          NULL, 0,
          stat, &statLen
        );
        if(ret){
            DBG (5, "coarsecal: error sending d2 cmd\n");
            return ret;
        }
        if(stat[0] != 6){
            DBG (5, "coarsecal: cmd bad d2 status?\n");
            return SANE_STATUS_IO_ERROR;
        }

        s->coarsecal.rx_bytes = 0;
        s->coarsecal.tx_bytes = 0;
    
        while(s->coarsecal.rx_bytes < s->coarsecal.total_bytes){
            ret = read_from_scanner(s,&s->coarsecal);
            if(ret){
                DBG (5, "coarsecal: cant read from scanner\n");
                return ret;
            }
        }

        /* FIXME inspect data, change coarse cal data */
        if(s->model == MODEL_S300){
            pay[11] += direction;
            pay[13] += direction;
        }
        else{
            pay[11] += direction;
            pay[13] += direction;
            pay[15] += direction;
        }

        hexdump(15, "c6 payload: ", pay, payLen);
    }

    DBG (10, "coarsecal: finish\n");
    return ret;
}

static SANE_Status
finecal(struct scanner *s)
{
    SANE_Status ret = SANE_STATUS_GOOD;

    size_t cmdLen = 2;
    unsigned char cmd[2];

    size_t statLen = 1;
    unsigned char stat[2];

    int i,j;

    DBG (10, "finecal: start\n");

    /* ask for 16 lines */
    ret = set_window(s, WINDOW_FINECAL);
    if(ret){
        DBG (5, "finecal: error sending setwindowcal\n");
        return ret;
    }

    /* dark cal, lamp off */
    lamp(s,0);

    /* send scan d2 command */
    cmd[0] = 0x1b;
    cmd[1] = 0xd2;
    stat[0] = 0;
    statLen = 1;

    ret = do_cmd(
      s, 0,
      cmd, cmdLen,
      NULL, 0,
      stat, &statLen
    );
    if(ret){
        DBG (5, "finecal: error sending d2 cmd\n");
        return ret;
    }
    if(stat[0] != 6){
        DBG (5, "finecal: cmd bad d2 status?\n");
        return SANE_STATUS_IO_ERROR;
    }

    s->darkcal.rx_bytes = 0;
    s->darkcal.tx_bytes = 0;

    while(s->darkcal.rx_bytes < s->darkcal.total_bytes){
        ret = read_from_scanner(s,&s->darkcal);
        if(ret){
            DBG (5, "finecal: cant read from scanner\n");
            return ret;
        }
    }

    /* grab rows with lamp on */
    lamp(s,1);

    /* send scan d2 command */
    cmd[0] = 0x1b;
    cmd[1] = 0xd2;
    stat[0] = 0;
    statLen = 1;

    ret = do_cmd(
      s, 0,
      cmd, cmdLen,
      NULL, 0,
      stat, &statLen
    );
    if(ret){
        DBG (5, "finecal: error sending d2 cmd\n");
        return ret;
    }
    if(stat[0] != 6){
        DBG (5, "finecal: cmd bad d2 status?\n");
        return SANE_STATUS_IO_ERROR;
    }

    s->lightcal.rx_bytes = 0;
    s->lightcal.tx_bytes = 0;

    while(s->lightcal.rx_bytes < s->lightcal.total_bytes){
        ret = read_from_scanner(s,&s->lightcal);
        if(ret){
            DBG (5, "finecal: cant read from scanner\n");
            return ret;
        }
    }

    /* sum up light and dark scans */
    for(j=0; j<s->darkcal.width_bytes; j++){

        int dtotal=0, ltotal=0, gain=0, offset=0;

        /*s300 has two bytes of NULL out of every 6*/
        if(s->model == MODEL_S300 && (j%3) == 2){
          s->sendcal.buffer[j*2] = 0;
          s->sendcal.buffer[j*2+1] = 0;
          continue;
        }

        for(i=0; i<s->darkcal.height; i++){
            dtotal += s->darkcal.buffer[i*s->darkcal.width_bytes+j];
        }

        for(i=0; i<s->lightcal.height; i++){
            ltotal += s->lightcal.buffer[i*s->lightcal.width_bytes+j];
        }

        /* even bytes of payload (offset) */
        /* larger numbers - greater dark offset? */

        if(s->model == MODEL_S300){
          /* S300 front side 150dpi color empirically determined*/
          offset = dtotal - ltotal/93 - 0xa4;
        }
        else{
          /* fi-60F 600dpi color empirically determined*/
          offset = dtotal - ltotal/100 + 8;
        }

        if(offset < 1){
          s->sendcal.buffer[j*2] = 0;
	}
        else if(offset > 0xfe){
          s->sendcal.buffer[j*2] = 0xff;
	}
        else{
          s->sendcal.buffer[j*2] = offset;
	}

        /* odd bytes of payload (gain) */
        /* smaller numbers increase gain (contrast) */

        if(s->model == MODEL_S300){
          if(j%3 == 0){
            /* S300 front side 150dpi color empirically determined */
            gain = ltotal * 17/100 - dtotal/5 - 0x38;
          }
          else{
            /* S300 back side 150dpi color empirically determined */
            gain = ltotal * 21/100 - dtotal/5 - 0x4f;
          }
        }
        else{
          /* fi-60F 600dpi color empirically determined*/
          /* FIXME: too much gain? */
          gain = ltotal * 2/19 - dtotal/9 - 50;
        }

        if(gain < 1){
          s->sendcal.buffer[j*2+1] = 0;
	}
        else if(gain > 0xfe){
          s->sendcal.buffer[j*2+1] = 0xff;
	}
        else{
          s->sendcal.buffer[j*2+1] = gain;
	}

    }

    if(DBG_LEVEL >= 15){
        FILE * foo = fopen("epjitsu_finecal.pnm","w");
	fprintf(foo,"P5\n%d\n%d\n255\n",s->darkcal.width_bytes,s->darkcal.height*2+2);
        fwrite(s->darkcal.buffer,s->darkcal.total_bytes-8,1,foo);
        fwrite(s->lightcal.buffer,s->lightcal.total_bytes-8,1,foo);

        /* write out even (offset) and odd (gain) bytes on separate lines */
        for(i=0;i<s->sendcal.total_bytes;i+=2){
	    fwrite(s->sendcal.buffer+i,1,1,foo);
	}
        for(i=1;i<s->sendcal.total_bytes;i+=2){
	    fwrite(s->sendcal.buffer+i,1,1,foo);
	}
	fclose(foo);
    }

    ret = set_window(s, WINDOW_SENDCAL);
    if(ret){
        DBG (5, "finecal: error sending setwindow\n");
        return ret;
    }

    /*first unknown cal block*/
    cmd[1] = 0xc3;
    statLen = 1;
    
    ret = do_cmd(
      s, 0,
      cmd, cmdLen,
      NULL, 0,
      stat, &statLen
    );
    if(ret){
        DBG (5, "finecal: error sending c3 cmd\n");
        return ret;
    }
    if(stat[0] != 6){
        DBG (5, "finecal: cmd bad c3 status?\n");
        return SANE_STATUS_IO_ERROR;
    }

    /*send header*/
    /*send payload*/
    statLen = 1;

    ret = do_cmd(
      s, 0,
      s->sendCal1Header, s->sendCal1HeaderLen,
      s->sendcal.buffer, s->sendcal.total_bytes,
      stat, &statLen
    );

    if(ret){
        DBG (5, "finecal: error sending c3 payload\n");
        return ret;
    }
    if(stat[0] != 6){
        DBG (5, "finecal: payload bad c3 status?\n");
        return SANE_STATUS_IO_ERROR;
    }

    /*second unknown cal block*/
    cmd[1] = 0xc4;
    statLen = 1;
    
    ret = do_cmd(
      s, 0,
      cmd, cmdLen,
      NULL, 0,
      stat, &statLen
    );

    if(ret){
        DBG (5, "finecal: error sending c4 cmd\n");
        return ret;
    }
    if(stat[0] != 6){
        DBG (5, "finecal: cmd bad c4 status?\n");
        return SANE_STATUS_IO_ERROR;
    }

    /*send header*/
    /*send payload*/
    statLen = 1;

    ret = do_cmd(
      s, 0,
      s->sendCal2Header, s->sendCal2HeaderLen,
      s->sendcal.buffer, s->sendcal.total_bytes,
      stat, &statLen
    );

    if(ret){
        DBG (5, "finecal: error sending c4 payload\n");
        return ret;
    }
    if(stat[0] != 6){
        DBG (5, "finecal: payload bad c4 status?\n");
        return SANE_STATUS_IO_ERROR;
    }

    DBG (10, "cal: finish\n");
    return ret;
}

static SANE_Status
lamp(struct scanner *s, unsigned char set)
{
    SANE_Status ret = SANE_STATUS_GOOD;
    unsigned char cmd[2];
    size_t cmdLen = 2;
    unsigned char stat[1];
    size_t statLen = 1;
  
    DBG (10, "lamp: start (%d)\n", set);

    /*send cmd*/
    cmd[0] = 0x1b;
    cmd[1] = 0xd0;
    
    ret = do_cmd(
      s, 0,
      cmd, cmdLen,
      NULL, 0,
      stat, &statLen
    );
    if(ret){
        DBG (5, "lamp: error sending cmd\n");
        return ret;
    }
    if(stat[0] != 6){
        DBG (5, "lamp: cmd bad status?\n");
        return SANE_STATUS_IO_ERROR;
    }

    /*send payload*/
    cmd[0] = set;
    cmdLen = 1;
    statLen = 1;
    
    ret = do_cmd(
      s, 0,
      cmd, cmdLen,
      NULL, 0,
      stat, &statLen
    );
    if(ret){
        DBG (5, "lamp: error sending payload\n");
        return ret;
    }
    if(stat[0] != 6){
        DBG (5, "lamp: payload bad status?\n");
        return SANE_STATUS_IO_ERROR;
    }

    DBG (10, "lamp: finish\n");
    return ret;
}

static SANE_Status
set_window(struct scanner *s, int window)
{
    SANE_Status ret = SANE_STATUS_GOOD;
    unsigned char cmd[] = {0x1b, 0xd1};
    size_t cmdLen = sizeof(cmd);
    unsigned char stat[] = {0};
    size_t statLen = sizeof(stat);
    unsigned char * payload;
    size_t paylen;

    DBG (10, "set_window: start, window %d\n",window);

    switch (window) {
      case WINDOW_COARSECAL:
        payload = s->setWindowCoarseCal;
	paylen  = s->setWindowCoarseCalLen;
	break;
      case WINDOW_FINECAL:
        payload = s->setWindowFineCal;
	paylen  = s->setWindowFineCalLen;
	break;
      case WINDOW_SENDCAL:
        payload = s->setWindowSendCal;
	paylen  = s->setWindowSendCalLen;
	break;
      case WINDOW_SCAN:
        payload = s->setWindowScan;
	paylen  = s->setWindowScanLen;
	break;
      default:
        DBG (5, "set_window: unknown window\n");
        return SANE_STATUS_INVAL;
    }

    /*send cmd*/
    ret = do_cmd(
      s, 0,
      cmd, cmdLen,
      NULL, 0,
      stat, &statLen
    );
    if(ret){
        DBG (5, "set_window: error sending cmd\n");
        return ret;
    }
    if(stat[0] != 6){
        DBG (5, "set_window: cmd bad status?\n");
        return SANE_STATUS_IO_ERROR;
    }

    /*send payload*/
    statLen = 1;
    
    ret = do_cmd(
      s, 0,
      payload, paylen,
      NULL, 0,
      stat, &statLen
    );
    if(ret){
        DBG (5, "set_window: error sending payload\n");
        return ret;
    }
    if(stat[0] != 6){
        DBG (5, "set_window: payload bad status?\n");
        return SANE_STATUS_IO_ERROR;
    }

    DBG (10, "set_window: finish\n");
    return ret;
}

static SANE_Status
ingest(struct scanner *s)
{
    SANE_Status ret = SANE_STATUS_GOOD;
    int i;

    unsigned char cmd[2];
    size_t cmdLen = sizeof(cmd);
    unsigned char stat[1];
    size_t statLen = sizeof(stat);
    unsigned char pay[2];
    size_t payLen = sizeof(pay);

    DBG (10, "ingest: start\n");

    for(i=0;i<5;i++){
    
        /*send paper load cmd*/
        cmd[0] = 0x1b;
        cmd[1] = 0xd4;
        statLen = 1;
        
        ret = do_cmd(
          s, 0,
          cmd, cmdLen,
          NULL, 0,
          stat, &statLen
        );
        if(ret){
            DBG (5, "ingest: error sending cmd\n");
            return ret;
        }
        if(stat[0] != 6){
            DBG (5, "ingest: cmd bad status?\n");
            return SANE_STATUS_IO_ERROR;
        }
    
        /*send payload*/
        statLen = 1;
        payLen = 1;
        pay[0] = 1;
        
        ret = do_cmd(
          s, 0,
          pay, payLen,
          NULL, 0,
          stat, &statLen
        );
        if(ret){
            DBG (5, "ingest: error sending payload\n");
            return ret;
        }
        if(stat[0] == 6){
            DBG (5, "ingest: found paper?\n");
            break;
        }
        if(stat[0] == 0x15){
            DBG (5, "ingest: no paper?\n");
            ret=SANE_STATUS_NO_DOCS;
	    continue;
        }
        if(stat[0] != 6){
            DBG (5, "ingest: payload bad status?\n");
            return SANE_STATUS_IO_ERROR;
        }
    }

    DBG (10, "ingest: finish\n");
    return ret;
}

static SANE_Status
scan(struct scanner *s)
{
    SANE_Status ret=SANE_STATUS_GOOD;
    unsigned char cmd[] = {0x1b, 0xd2};
    size_t cmdLen = 2;
    unsigned char stat[1];
    size_t statLen = 1;
    
    DBG (10, "scan: start\n");

    if(s->model == MODEL_S300){
        cmd[1] = 0xd6;
    }

    ret = do_cmd(
      s, 0,
      cmd, cmdLen,
      NULL, 0,
      stat, &statLen
    );
    if(ret){
        DBG (5, "scan: error sending cmd\n");
        return ret;
    }
    if(stat[0] != 6){
        DBG (5, "scan: cmd bad status?\n");
        return SANE_STATUS_IO_ERROR;
    }

    DBG (10, "scan: finish\n");
  
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
    struct scanner *s = (struct scanner *) handle;
    SANE_Status ret=SANE_STATUS_GOOD;
    struct transfer * tp;
  
    DBG (10, "sane_read: start si:%d len:%d max:%d\n",s->side,*len,max_len);

    *len = 0;

    /* cancelled? */
    if(!s->started){
        DBG (5, "sane_read: call sane_start first\n");
        return SANE_STATUS_CANCELLED;
    }
  
    /* have sent all of current buffer */
    if(s->send_eof){
        DBG (10, "sane_read: returning eof\n");
        return SANE_STATUS_EOF;
    } 

    /* scan not finished, get more into block buffer */
    if(s->scan.rx_bytes != s->scan.total_bytes){

        /* block buffer currently empty, clean up */ 
	if(!s->block.rx_bytes){

            /* block buffer bigger than remainder of scan, shrink block */
            int remainTotal = s->scan.total_bytes - s->scan.rx_bytes;
	    if(remainTotal < s->block.total_bytes){
                DBG (15, "sane_read: shrinking block to %lu\n",
                  (unsigned long)remainTotal);
	        s->block.total_bytes = remainTotal;
	    }

            /* send d3 cmd for S300 */
            if(s->model == MODEL_S300){
        
                unsigned char cmd[] = {0x1b, 0xd3};
                size_t cmdLen = 2;
                unsigned char stat[1];
                size_t statLen = 1;
                
                DBG (15, "sane_read: d3\n");
              
                ret = do_cmd(
                  s, 0,
                  cmd, cmdLen,
                  NULL, 0,
                  stat, &statLen
                );
                if(ret){
                    DBG (5, "sane_read: error sending d3 cmd\n");
                    return ret;
                }
                if(stat[0] != 6){
                    DBG (5, "sane_read: cmd bad status?\n");
                    return SANE_STATUS_IO_ERROR;
                }
            }
	}

        ret = read_from_scanner(s, &s->block);
        if(ret){
            DBG (5, "sane_read: cant read from scanner\n");
            return ret;
        }

        /* block filled, copy to front/back */
	if(s->block.rx_bytes == s->block.total_bytes){

            DBG (15, "sane_read: block buffer full\n");

            /* get the 0x43 cmd for the S300 */
            if(s->model == MODEL_S300){

                unsigned char cmd[] = {0x1b, 0x43};
                size_t cmdLen = 2;
                unsigned char in[10];
                size_t inLen = 10;
      
                ret = do_cmd(
                  s, 0,
                  cmd, cmdLen,
                  NULL, 0,
                  in, &inLen
                );
                hexdump(30, "cmd 43: ", in, inLen);
    
                if(ret){
                    DBG (5, "sane_read: error sending 43 cmd\n");
                    return ret;
                }

                ret = fill_frontback_buffers_S300(s);
                if(ret){
                    DBG (5, "sane_read: cant copy to front/back\n");
                    return ret;
                }
            }

            else { /*fi-60f*/
                ret = fill_frontback_buffers_FI60F(s);
                if(ret){
                    DBG (5, "sane_read: cant copy to front/back\n");
                    return ret;
                }
	    }

            /* count block_rx_bytes in scan_rx_bytes */
            s->scan.rx_bytes += s->block.rx_bytes;
            s->scan.tx_bytes += s->block.rx_bytes;

            /* reset for next pass */
            s->block.rx_bytes = 0;
            s->block.tx_bytes = 0;

            /* scan now finished, reset size of block buffer */
            if(s->scan.rx_bytes == s->scan.total_bytes){
                DBG (15, "sane_read: growing block\n");
                update_block_totals(s);
            }
	}
    }

    if(s->side == SIDE_FRONT){
        tp = &s->front;
    }
    else{
        tp = &s->back;
    }

    *len = tp->rx_bytes - tp->tx_bytes;
    if(*len > max_len){
        *len = max_len;
    }

    if(*len){
        DBG (10, "sane_read: copy rx:%d tx:%d tot:%d len:%d\n",
          tp->rx_bytes,tp->tx_bytes,tp->total_bytes,*len);
    
        memcpy(buf,tp->buffer+tp->tx_bytes,*len);
        tp->tx_bytes += *len;
    
        /* sent it all, return eof on next read */
        if(tp->tx_bytes == tp->total_bytes){
            DBG (10, "sane_read: side done\n");
            s->send_eof=1;
        }
    }  

    DBG (10, "sane_read: finish si:%d len:%d max:%d\n",s->side,*len,max_len);
  
    return ret;
}

/* fills block buffer a little per pass */
static SANE_Status
read_from_scanner(struct scanner *s, struct transfer * tp)
{
    SANE_Status ret=SANE_STATUS_GOOD;
    size_t bytes = MAX_IMG_PASS;
    size_t remainBlock = tp->total_bytes - tp->rx_bytes;
  
    /* determine amount to ask for */
    if(bytes > remainBlock){
        bytes = remainBlock;
    }

    DBG (10, "read_from_scanner: start rB:%lu len:%lu\n",
      (unsigned long)remainBlock, (unsigned long)bytes);

    if(!bytes){
        DBG(10, "read_from_scanner: no bytes!\n");
        return SANE_STATUS_INVAL;
    }

    ret = do_cmd(
      s, 0,
      NULL, 0,
      NULL, 0,
      tp->buffer+tp->rx_bytes, &bytes
    );
  
    /* full read or short read */
    if (ret == SANE_STATUS_GOOD || (ret == SANE_STATUS_EOF && bytes) ) {

        DBG(15,"read_from_scanner: got GOOD/EOF (%lu)\n",(unsigned long)bytes);
        ret = SANE_STATUS_GOOD;
        tp->rx_bytes += bytes;
    }
    else {
        DBG(5, "read_from_scanner: error reading status = %d\n", ret);
    }
  
    DBG (10, "read_from_scanner: finish rB:%lu len:%lu\n",
      (unsigned long)(tp->total_bytes-tp->rx_bytes), (unsigned long)bytes);
  
    return ret;
}

/* copies block buffer into front and back buffers */
/* moves front/back rx_bytes forward */
static SANE_Status
fill_frontback_buffers_S300(struct scanner *s)
{
    SANE_Status ret=SANE_STATUS_GOOD;
    int i,j;
    int thresh = s->threshold * 3;

    DBG (10, "fill_frontback_buffers_S300: start\n");

    switch (s->mode) {
      case MODE_COLOR:
        /* put frontside data into buffer */
        if(s->source != SOURCE_ADF_BACK){

            for(i=0; i<s->block.rx_bytes-8; i+=s->block.width_bytes){
                for(j=0; j<s->front.width_pix; j++){

                    /*red*/
                    s->front.buffer[s->front.rx_bytes++] =
                      s->block.buffer[i + s->req_width*3 + j*3];

                    /*green*/
                    s->front.buffer[s->front.rx_bytes++] =
                      s->block.buffer[i + s->req_width*6 + j*3];

                    /*blue*/
                    s->front.buffer[s->front.rx_bytes++] =
                      s->block.buffer[i + j*3];
                }
            }
        }

        /* put backside data into buffer */
        if(s->source == SOURCE_ADF_DUPLEX || s->source == SOURCE_ADF_BACK){
            for(i=0; i<s->block.rx_bytes-8; i+=s->block.width_bytes){
                for(j=0; j<s->back.width_pix; j++){

                    /*red*/
                    s->back.buffer[s->back.rx_bytes++] =
                      s->block.buffer[i + s->req_width*3 + (s->back.width_pix-1)*3 - j*3 + 1];

                    /*green*/
                    s->back.buffer[s->back.rx_bytes++] =
                      s->block.buffer[i + s->req_width*6 + (s->back.width_pix-1)*3 - j*3 + 1];

                    /*blue*/
                    s->back.buffer[s->back.rx_bytes++] =
                      s->block.buffer[i + (s->back.width_pix-1)*3 - j*3 + 1];
                }
            }
        }

        break;

      case MODE_GRAYSCALE:

        /* put frontside data into buffer */
        if(s->source != SOURCE_ADF_BACK){
            for(i=0; i<s->block.rx_bytes-8; i+=s->block.width_bytes){
                for(j=0; j<s->front.width_pix; j++){

                    /* GS is (red+green+blue)/3 */
                    s->front.buffer[s->front.rx_bytes++] =
                      ( s->block.buffer[i + s->req_width*3 + j*3]
                      + s->block.buffer[i + s->req_width*6 + j*3]
                      + s->block.buffer[i + j*3]) / 3;
                }
            }
        }

        /* put backside data into buffer */
        if(s->source == SOURCE_ADF_DUPLEX || s->source == SOURCE_ADF_BACK){
            for(i=0; i<s->block.rx_bytes-8; i+=s->block.width_bytes){
                for(j=0; j<s->back.width_pix; j++){

                    /* GS is (red+green+blue)/3 */
                    s->back.buffer[s->back.rx_bytes++] =
                      ( (int)s->block.buffer[i + s->req_width*3 + (s->back.width_pix-1)*3 - j*3 + 1]
                      + s->block.buffer[i + s->req_width*6 + (s->back.width_pix-1)*3 - j*3 + 1]
                      + s->block.buffer[i + (s->back.width_pix-1)*3 - j*3 + 1]) / 3;
                }
            }
        }
        break;

      default: /* binary */

        /* put frontside data into buffer */
        if(s->source != SOURCE_ADF_BACK){

            for(i=0; i<s->block.rx_bytes-8; i+=s->block.width_bytes){
                for(j=0; j<s->front.width_pix; j++){
        
                    int offset = j%8;
                    unsigned char mask = 0x80 >> offset;
                    int curr = s->block.buffer[i+j*3] +  /*blue*/
                    s->block.buffer[i+(s->req_width+j)*3] + /*green*/
                    s->block.buffer[i+(s->req_width*2+j)*3];  /*red*/
        
                    /* looks white */
                    if(curr > thresh){
                        s->front.buffer[s->front.rx_bytes] &= ~mask;
                    }
                    else{
                        s->front.buffer[s->front.rx_bytes] |= mask;
                    }
                    
                    if(offset == 7){
                        s->front.rx_bytes++;
                    }
                }
            }
        }
    
        /* put backside data into buffer */
        if(s->source == SOURCE_ADF_DUPLEX || s->source == SOURCE_ADF_BACK){
    
            for(i=0; i<s->block.rx_bytes-8; i+=s->block.width_bytes){
              for(j=0; j<s->back.width_pix; j++){
  
                int offset = j%8;
                unsigned char mask = 0x80 >> offset;
                int curr = s->block.buffer[i+(s->back.width_pix-1-j)*3+1] +
                s->block.buffer[i+(s->req_width+s->back.width_pix-1-j)*3+1] +
                s->block.buffer[i+(s->req_width*2+s->back.width_pix-1-j)*3+1];
 
         	/* looks white */
                if(curr > thresh){
                    s->back.buffer[s->back.rx_bytes] &= ~mask;
                }
                else{
                    s->back.buffer[s->back.rx_bytes] |= mask;
                }
                 
                if(offset == 7){
                    s->back.rx_bytes++;
                }
      	      }
            }
        }
        break;
    }

    DBG (10, "fill_frontback_buffers_S300: finish\n");
  
    return ret;
}

/* copies block_buffer into front and back buffers */
/* moves front/back rx_bytes forward */
static SANE_Status
fill_frontback_buffers_FI60F(struct scanner *s)
{
    SANE_Status ret=SANE_STATUS_GOOD;
    int i,j,k;

    DBG (10, "fill_frontback_buffers_FI60F: start\n");

    switch (s->mode) {
      case MODE_COLOR:

        /* reorder the img data into the struct's buffer */
        for(i=0; i<s->block.rx_bytes-8; i+=s->block.width_bytes){
    
            DBG (15, "fill_frontback_buffers_FI60F: offset %d\n", i);

            for(k=0; k<3; k++){

                for(j=0; j<s->head_width; j++){

                    /* red */
                    s->front.buffer[s->front.rx_bytes++] = 
                      s->block.buffer[i+(s->head_width-1-j)*3+2-k];
    
                    /* green */
                    s->front.buffer[s->front.rx_bytes++] = 
                      s->block.buffer[i+(s->head_width*2+s->pad_width-1-j)*3+2-k];
    
                    /* blue */
                    s->front.buffer[s->front.rx_bytes++] = 
                      s->block.buffer[i+(s->head_width*3+s->pad_width*2-1-j)*3+2-k];
                }
            }
        }

        break;

      case MODE_GRAYSCALE:

        /* reorder the img data into the struct's buffer */
        for(i=0; i<s->block.rx_bytes-8; i+=s->block.width_bytes){
    
            DBG (15, "fill_frontback_buffers_FI60F: offset %d\n", i);

            for(k=0; k<3; k++){

                for(j=0; j<s->head_width; j++){

                    /* GS = red green blue / 3 */
                    s->front.buffer[s->front.rx_bytes++] = 
                      ( s->block.buffer[i+(s->head_width-1-j)*3+2-k]
                      + s->block.buffer[i+(s->head_width*2+s->pad_width-1-j)*3+2-k]
                      + s->block.buffer[i+(s->head_width*3+s->pad_width*2-1-j)*3+2-k]
                      )/3;
                }
            }
        }
        break;

      default: /* binary */

        /* put binary data into buffer */
        for(i=0; i<s->block.rx_bytes-8; i+=s->block.width_bytes){

            DBG (15, "fill_frontback_buffers_FI60F: offset %d\n", i);

            for(k=0; k<3; k++){

                for(j=0; j<s->head_width; j++){

                    int offset = j%8;
                    unsigned char mask = 0x80 >> offset;
                    int curr = s->block.buffer[i+(s->head_width-1-j)*3+2-k]
                      + s->block.buffer[i+(s->head_width*2+s->pad_width-1-j)*3+2-k]
                      + s->block.buffer[i+(s->head_width*3+s->pad_width*2-1-j)*3+2-k];

                    /* looks white */
                    if(curr > s->threshold){
                        s->front.buffer[s->front.rx_bytes] &= ~mask;
                    }
                    else{
                        s->front.buffer[s->front.rx_bytes] |= mask;
                    }
                    
                    if(offset == 7){
                        s->front.rx_bytes++;
                    }
                }
            }
        }
    
        break;
    }

    DBG (10, "fill_frontback_buffers_FI60F: finish\n");
  
    return ret;
}

/*
 * @@ Section 4 - SANE cleanup functions
 */
/*
 * Cancels a scan. 
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
  /*FIXME: actually ask the scanner to stop?*/
  struct scanner * s = (struct scanner *) handle;
  DBG (10, "sane_cancel: start\n");
  s->started = 0;
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

  /* still connected- drop it */
  if(s->fd >= 0){
      sane_cancel(handle);
      lamp(s, 0);
      disconnect_fd(s);
  }

  if(s->sane.name){
    free(s->sane.name);
  }
  if(s->sane.model){
    free(s->sane.model);
  }
  if(s->sane.vendor){
    free(s->sane.vendor);
  }

  teardown_buffers(s);
  free(s);

  DBG (10, "sane_close: finish\n");
}

static SANE_Status
disconnect_fd (struct scanner *s)
{
  DBG (10, "disconnect_fd: start\n");

  if(s->fd > -1){
    DBG (15, "disconnecting usb device\n");
    sanei_usb_close (s->fd);
    s->fd = -1;
  }

  DBG (10, "disconnect_fd: finish\n");

  return SANE_STATUS_GOOD;
}

static SANE_Status
teardown_buffers(struct scanner *s)
{
    SANE_Status ret = SANE_STATUS_GOOD;

    DBG (10, "teardown_buffers: start\n");

    /* temporary cal data */
    if(s->coarsecal.buffer){
        free(s->coarsecal.buffer);
	s->coarsecal.buffer = NULL;
    }

    if(s->darkcal.buffer){
        free(s->darkcal.buffer);
	s->darkcal.buffer = NULL;
    }

    if(s->sendcal.buffer){
        free(s->sendcal.buffer);
	s->sendcal.buffer = NULL;
    }

    /* image slice */
    if(s->block.buffer){
        free(s->block.buffer);
	s->block.buffer = NULL;
    }

    /* make image buffer to hold frontside data */
    if(s->front.buffer){
        free(s->front.buffer);
	s->front.buffer = NULL;
    }

    /* make image buffer to hold backside data */
    if(s->back.buffer){
        free(s->back.buffer);
	s->back.buffer = NULL;
    }

    DBG (10, "teardown_buffers: finish\n");
    return ret;
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
      next = dev->next;
      free(dev);
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
 * take a bunch of pointers, send commands to scanner
 */
static SANE_Status
do_cmd(struct scanner *s, int shortTime,
 unsigned char * cmdBuff, size_t cmdLen,
 unsigned char * outBuff, size_t outLen,
 unsigned char * inBuff, size_t * inLen
)
{
    /* sanei_usb overwrites the transfer size, so make some local copies */
    size_t loc_cmdLen = cmdLen;
    size_t loc_outLen = outLen;
    size_t loc_inLen = 0;

    int cmdTime = USB_COMMAND_TIME;
    int outTime = USB_DATA_TIME;
    int inTime = USB_DATA_TIME;

    int ret = 0;

    DBG (10, "do_cmd: start\n");

    if(shortTime){
        cmdTime /= 20;
        outTime /= 20;
        inTime /= 20;
    }

    /* this command has a cmd component, and a place to get it */
    if(cmdBuff && cmdLen && cmdTime){

        /* change timeout */
        sanei_usb_set_timeout(cmdTime);
    
        /* write the command out */
        DBG(25, "cmd: writing %ld bytes, timeout %d\n", (long)cmdLen, cmdTime);
        hexdump(30, "cmd: >>", cmdBuff, cmdLen);
        ret = sanei_usb_write_bulk(s->fd, cmdBuff, &cmdLen);
        DBG(25, "cmd: wrote %ld bytes, retVal %d\n", (long)cmdLen, ret);
    
        if(ret == SANE_STATUS_EOF){
            DBG(5,"cmd: got EOF, returning IO_ERROR\n");
            return SANE_STATUS_IO_ERROR;
        }
        if(ret != SANE_STATUS_GOOD){
            DBG(5,"cmd: return error '%s'\n",sane_strstatus(ret));
            return ret;
        }
        if(loc_cmdLen != cmdLen){
            DBG(5,"cmd: wrong size %ld/%ld\n", (long)loc_cmdLen, (long)cmdLen);
            return SANE_STATUS_IO_ERROR;
        }
    }

    /* this command has a write component, and a place to get it */
    if(outBuff && outLen && outTime){

        /* change timeout */
        sanei_usb_set_timeout(outTime);

        DBG(25, "out: writing %ld bytes, timeout %d\n", (long)outLen, outTime);
        hexdump(30, "out: >>", outBuff, outLen);
        ret = sanei_usb_write_bulk(s->fd, outBuff, &outLen);
        DBG(25, "out: wrote %ld bytes, retVal %d\n", (long)outLen, ret);

        if(ret == SANE_STATUS_EOF){
            DBG(5,"out: got EOF, returning IO_ERROR\n");
            return SANE_STATUS_IO_ERROR;
        }
        if(ret != SANE_STATUS_GOOD){
            DBG(5,"out: return error '%s'\n",sane_strstatus(ret));
            return ret;
        }
        if(loc_outLen != outLen){
            DBG(5,"out: wrong size %ld/%ld\n", (long)loc_outLen, (long)outLen);
            return SANE_STATUS_IO_ERROR;
        }
    }

    /* this command has a read component, and a place to put it */
    if(inBuff && inLen && inTime){

        loc_inLen = *inLen;
        DBG(25, "in: memset %ld bytes\n", (long)*inLen);
        memset(inBuff,0,*inLen);

        /* change timeout */
        sanei_usb_set_timeout(inTime);

        DBG(25, "in: reading %ld bytes, timeout %d\n", (long)*inLen, inTime);
        ret = sanei_usb_read_bulk(s->fd, inBuff, inLen);
        DBG(25, "in: retVal %d\n", ret);

        if(ret == SANE_STATUS_EOF){
            DBG(5,"in: got EOF, continuing\n");
        }
        else if(ret != SANE_STATUS_GOOD){
            DBG(5,"in: return error '%s'\n",sane_strstatus(ret));
            return ret;
        }

        DBG(25, "in: read %ld bytes\n", (long)*inLen);
        if(*inLen){
            hexdump(30, "in: <<", inBuff, *inLen);
        }

        if(loc_inLen != *inLen){
            ret = SANE_STATUS_EOF;
            DBG(5,"in: short read %ld/%ld\n", (long)loc_inLen, (long)*inLen);
        }
    }

    DBG (10, "do_cmd: finish\n");

    return ret;
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
