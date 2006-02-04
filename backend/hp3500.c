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

   This file implements a SANE backend for HP ScanJet 3500 series scanners.
   Currently supported:
    - HP ScanJet 3500C
    - HP ScanJet 3530C
    - HP ScanJet 3570C

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
   . .
   . . - sane_cancel() : cancel operation
   . - sane_close() : close opened scanner device
   - sane_exit() : terminate use of backend


   There are some device specific routines in this file that are in "#if 0"
   sections - these are left in place for documentation purposes in case
   somebody wants to implement features that use those routines.

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
#include <unistd.h>
#ifdef HAVE_LIBC_H
# include <libc.h>		/* NeXTStep/OpenStep */
#endif

#include "sane/sane.h"
#include "sane/sanei_usb.h"
#include "sane/saneopts.h"
#include "sane/sanei_config.h"
#include "sane/sanei_thread.h"
#include "sane/sanei_backend.h"

#define RTCMD_GETREG		0x80
#define	RTCMD_READSRAM		0x81

#define	RTCMD_SETREG		0x88
#define	RTCMD_WRITESRAM		0x89

#define	RTCMD_NVRAMCONTROL	0x8a

#define	RTCMD_BYTESAVAIL	0x90
#define	RTCMD_READBYTES		0x91

#define	RT_CHANNEL_ALL		0
#define	RT_CHANNEL_RED		1
#define	RT_CHANNEL_GREEN	2
#define	RT_CHANNEL_BLUE		3

typedef int (*rts8801_callback) (void *param, unsigned bytes, void *data);

#define	RTS8801_GREYSCALE	0
#define	RTS8801_COLOUR		1
#define	RTS8801_BW		2

#define DEBUG 1
#define MM_PER_INCH 25.4
#define SCANNER_UNIT_TO_FIXED_MM(number) SANE_FIX(number * MM_PER_INCH / 1200)
#define FIXED_MM_TO_SCANNER_UNIT(number) SANE_UNFIX(number) * 1200 / MM_PER_INCH

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

enum hp3500_option
{
  OPT_NUM_OPTS = 0,

  OPT_RESOLUTION,
  OPT_GEOMETRY_GROUP,
  OPT_TL_X,
  OPT_TL_Y,
  OPT_BR_X,
  OPT_BR_Y,

  NUM_OPTIONS
};

typedef struct
{
  int left;
  int top;
  int right;
  int bottom;
} hp3500_rect;

struct hp3500_data
{
  struct hp3500_data *next;
  char *devicename;

  int sfd;
  int pipe_r;
  int pipe_w;
  int reader_pid;

  int resolution;

  time_t last_scan;

  hp3500_rect request_mm;
  hp3500_rect actual_mm;
  hp3500_rect fullres_pixels;
  hp3500_rect actres_pixels;

  int rounded_left;
  int rounded_top;
  int rounded_right;
  int rounded_bottom;

  int bytes_per_scan_line;
  int scan_width_pixels;
  int scan_height_pixels;

  SANE_Option_Descriptor opt[NUM_OPTIONS];
  SANE_Device sane;
};

static struct hp3500_data *first_dev = 0;
static struct hp3500_data **new_dev = &first_dev;
static int num_devices = 0;
static SANE_Int res_list[] =
  { 10, 25, 50, 75, 100, 150, 200, 300, 400, 600, 1200 };
static const SANE_Range range_x =
  { 0, SANE_FIX (215.9), SANE_FIX (MM_PER_INCH / 1200) };
static const SANE_Range range_y =
  { 0, SANE_FIX (298.7), SANE_FIX (MM_PER_INCH / 1200) };


static SANE_Status attachScanner (const char *name);
static SANE_Status init_options (struct hp3500_data *scanner);
static int reader_process (void *);
static void calculateDerivedValues (struct hp3500_data *scanner);
static void do_reset (struct hp3500_data *scanner);
static void do_cancel (struct hp3500_data *scanner);

/*
 * used by sane_get_devices
 */
static const SANE_Device **devlist = 0;

/*
 * SANE Interface
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
  authorize = authorize;	/* get rid of compiler warning */

  DBG_INIT ();
  DBG (10, "sane_init\n");

  sanei_usb_init ();
  sanei_thread_init ();

  if (version_code)
    *version_code = SANE_VERSION_CODE (V_MAJOR, V_MINOR, 0);

  sanei_usb_find_devices (0x03f0, 0x2205, attachScanner);
  sanei_usb_find_devices (0x03f0, 0x2005, attachScanner);

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
  int i;
  struct hp3500_data *dev;

  DBG (10, "sane_get_devices %d\n", local_only);

  if (devlist)
    free (devlist);
  devlist = calloc (num_devices + 1, sizeof (SANE_Device *));
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
  struct hp3500_data *dev = NULL;
  struct hp3500_data *scanner = NULL;

  if (name[0] == 0)
    {
      DBG (10, "sane_open: no device requested, using default\n");
      if (first_dev)
	{
	  scanner = (struct hp3500_data *) first_dev;
	  DBG (10, "sane_open: device %s found\n", first_dev->sane.name);
	}
    }
  else
    {
      DBG (10, "sane_open: device %s requested\n", name);

      for (dev = first_dev; dev; dev = dev->next)
	{
	  if (strcmp (dev->sane.name, name) == 0)
	    {
	      DBG (10, "sane_open: device %s found\n", name);
	      scanner = (struct hp3500_data *) dev;
	    }
	}
    }

  if (!scanner)
    {
      DBG (10, "sane_open: no device found\n");
      return SANE_STATUS_INVAL;
    }

  *handle = scanner;

  init_options (scanner);

  scanner->resolution = 600;
  scanner->request_mm.left = 0;
  scanner->request_mm.top = 0;
  scanner->request_mm.right = SCANNER_UNIT_TO_FIXED_MM (10200);
  scanner->request_mm.bottom = SCANNER_UNIT_TO_FIXED_MM (14100);
  calculateDerivedValues (scanner);

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
sane_get_select_fd (SANE_Handle h, SANE_Int * fdp)
{
  struct hp3500_data *scanner = (struct hp3500_data *) h;
  DBG (10, "sane_get_select_fd\n");
  *fdp = scanner->pipe_r;
  DBG (99, "%p %d\n", h, *fdp);
  return SANE_STATUS_GOOD;
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
  struct hp3500_data *scanner = handle;

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
  struct hp3500_data *scanner = (struct hp3500_data *) handle;
  SANE_Status status;
  SANE_Word cap;
  SANE_Int dummy;

  /* Make sure that all those statements involving *info cannot break (better
   * than having to do "if (info) ..." everywhere!)
   */
  if (info == 0)
    info = &dummy;

  *info = 0;

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

	case OPT_RESOLUTION:
	  *(SANE_Word *) val = scanner->resolution;
	  return SANE_STATUS_GOOD;

	case OPT_TL_X:
	  *(SANE_Word *) val = scanner->request_mm.left;
	  return SANE_STATUS_GOOD;

	case OPT_TL_Y:
	  *(SANE_Word *) val = scanner->request_mm.top;
	  return SANE_STATUS_GOOD;

	case OPT_BR_X:
	  *(SANE_Word *) val = scanner->request_mm.right;
	  return SANE_STATUS_GOOD;

	case OPT_BR_Y:
	  *(SANE_Word *) val = scanner->request_mm.bottom;
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
	case OPT_RESOLUTION:
	  if (scanner->resolution == *(SANE_Word *) val)
	    {
	      return SANE_STATUS_GOOD;
	    }
	  scanner->resolution = (*(SANE_Word *) val);
	  calculateDerivedValues (scanner);
	  *info |= SANE_INFO_RELOAD_PARAMS;
	  return SANE_STATUS_GOOD;

	case OPT_TL_X:
	  if (scanner->request_mm.left == *(SANE_Word *) val)
	    return SANE_STATUS_GOOD;
	  scanner->request_mm.left = *(SANE_Word *) val;
	  calculateDerivedValues (scanner);
	  if (scanner->actual_mm.left != scanner->request_mm.left)
	    *info |= SANE_INFO_INEXACT;
	  *info |= SANE_INFO_RELOAD_PARAMS;
	  return SANE_STATUS_GOOD;

	case OPT_TL_Y:
	  if (scanner->request_mm.top == *(SANE_Word *) val)
	    return SANE_STATUS_GOOD;
	  scanner->request_mm.top = *(SANE_Word *) val;
	  calculateDerivedValues (scanner);
	  if (scanner->actual_mm.top != scanner->request_mm.top)
	    *info |= SANE_INFO_INEXACT;
	  *info |= SANE_INFO_RELOAD_PARAMS;
	  return SANE_STATUS_GOOD;

	case OPT_BR_X:
	  if (scanner->request_mm.right == *(SANE_Word *) val)
	    {
	      return SANE_STATUS_GOOD;
	    }
	  scanner->request_mm.right = *(SANE_Word *) val;
	  calculateDerivedValues (scanner);
	  if (scanner->actual_mm.right != scanner->request_mm.right)
	    *info |= SANE_INFO_INEXACT;
	  *info |= SANE_INFO_RELOAD_PARAMS;
	  return SANE_STATUS_GOOD;

	case OPT_BR_Y:
	  if (scanner->request_mm.bottom == *(SANE_Word *) val)
	    {
	      return SANE_STATUS_GOOD;
	    }
	  scanner->request_mm.bottom = *(SANE_Word *) val;
	  calculateDerivedValues (scanner);
	  if (scanner->actual_mm.bottom != scanner->request_mm.bottom)
	    *info |= SANE_INFO_INEXACT;
	  *info |= SANE_INFO_RELOAD_PARAMS;
	  return SANE_STATUS_GOOD;
	}			/* switch */
    }				/* else */
  return SANE_STATUS_INVAL;
}

/**
 * Called by SANE when a page acquisition operation is to be started.
 *
 */
SANE_Status
sane_start (SANE_Handle handle)
{
  struct hp3500_data *scanner = handle;
  int defaultFds[2];
  int ret;

  DBG (10, "sane_start\n");

  if (scanner->sfd < 0)
    {
      /* first call */
      DBG (10, "sane_start opening USB device\n");
      if (sanei_usb_open (scanner->sane.name, &(scanner->sfd)) !=
	  SANE_STATUS_GOOD)
	{
	  DBG (MSG_ERR,
	       "sane_start: open of %s failed:\n", scanner->sane.name);
	  return SANE_STATUS_INVAL;
	}
    }

  calculateDerivedValues (scanner);

  DBG (10, "\tbytes per line = %d\n", scanner->bytes_per_scan_line);
  DBG (10, "\tpixels_per_line = %d\n", scanner->scan_width_pixels);
  DBG (10, "\tlines = %d\n", scanner->scan_height_pixels);


  /* create a pipe, fds[0]=read-fd, fds[1]=write-fd */
  if (pipe (defaultFds) < 0)
    {
      DBG (MSG_ERR, "ERROR: could not create pipe\n");
      do_cancel (scanner);
      return SANE_STATUS_IO_ERROR;
    }

  scanner->pipe_r = defaultFds[0];
  scanner->pipe_w = defaultFds[1];

  ret = SANE_STATUS_GOOD;

  scanner->reader_pid = sanei_thread_begin (reader_process, scanner);
  time (&scanner->last_scan);

  if (scanner->reader_pid == -1)
    {
      DBG (MSG_ERR, "cannot fork reader process.\n");
      DBG (MSG_ERR, "%s", strerror (errno));
      ret = SANE_STATUS_IO_ERROR;
    }

  if (sanei_thread_is_forked ())
    {
      close (scanner->pipe_w);
    }

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
  struct hp3500_data *scanner = (struct hp3500_data *) handle;


  DBG (10, "sane_get_parameters\n");

  calculateDerivedValues (scanner);

  params->format = SANE_FRAME_RGB;
  params->depth = 8;		/* internally we treat this as 24 */

  params->pixels_per_line = scanner->scan_width_pixels;
  params->lines = scanner->scan_height_pixels;

  params->bytes_per_line = scanner->bytes_per_scan_line;

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
  struct hp3500_data *scanner = (struct hp3500_data *) handle;
  ssize_t nread;
  int source = scanner->pipe_r;

  *len = 0;

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
      return SANE_STATUS_EOF;
    }

  return SANE_STATUS_GOOD;
}				/* sane_read */


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
  do_cancel ((struct hp3500_data *) h);
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
  do_reset (handle);
  do_cancel (handle);
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
  struct hp3500_data *dev, *next;

  DBG (10, "sane_exit\n");

  for (dev = first_dev; dev; dev = next)
    {
      next = dev->next;
      free (dev->devicename);
      free (dev);
    }

  if (devlist)
    free (devlist);
}

/*
 * The scanning code
 */

static SANE_Status
attachScanner (const char *devicename)
{
  struct hp3500_data *dev;

  DBG (15, "attach_scanner: %s\n", devicename);

  for (dev = first_dev; dev; dev = dev->next)
    {
      if (strcmp (dev->sane.name, devicename) == 0)
	{
	  DBG (5, "attach_scanner: scanner already attached (is ok)!\n");
	  return SANE_STATUS_GOOD;
	}
    }


  if (NULL == (dev = malloc (sizeof (*dev))))
    return SANE_STATUS_NO_MEM;
  memset (dev, 0, sizeof (*dev));

  dev->devicename = strdup (devicename);
  dev->sfd = -1;
  dev->last_scan = 0;
  dev->reader_pid = 0;
  dev->pipe_r = dev->pipe_w = -1;

  dev->sane.name = dev->devicename;
  dev->sane.vendor = "Hewlett-Packard";
  dev->sane.model = "ScanJet 3500";
  dev->sane.type = "scanner";

  ++num_devices;
  *new_dev = dev;

  DBG (15, "attach_scanner: done\n");

  return SANE_STATUS_GOOD;
}

static SANE_Status
init_options (struct hp3500_data *scanner)
{
  int i;
  SANE_Option_Descriptor *opt;

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

  opt = scanner->opt + OPT_RESOLUTION;
  opt->name = SANE_NAME_SCAN_RESOLUTION;
  opt->title = SANE_TITLE_SCAN_RESOLUTION;
  opt->desc = SANE_DESC_SCAN_RESOLUTION;
  opt->type = SANE_TYPE_INT;
  opt->constraint_type = SANE_CONSTRAINT_WORD_LIST;
  opt->constraint.word_list = res_list;
  opt->unit = SANE_UNIT_DPI;
  opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;

  opt = scanner->opt + OPT_GEOMETRY_GROUP;
  opt->title = "Geometry";
  opt->desc = "";
  opt->type = SANE_TYPE_GROUP;
  opt->constraint_type = SANE_CONSTRAINT_NONE;

  opt = scanner->opt + OPT_TL_X;
  opt->name = SANE_NAME_SCAN_TL_X;
  opt->title = SANE_TITLE_SCAN_TL_X;
  opt->desc = SANE_DESC_SCAN_TL_X;
  opt->type = SANE_TYPE_FIXED;
  opt->unit = SANE_UNIT_MM;
  opt->constraint_type = SANE_CONSTRAINT_RANGE;
  opt->constraint.range = &range_x;
  opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;

  opt = scanner->opt + OPT_TL_Y;
  opt->name = SANE_NAME_SCAN_TL_Y;
  opt->title = SANE_TITLE_SCAN_TL_Y;
  opt->desc = SANE_DESC_SCAN_TL_Y;
  opt->type = SANE_TYPE_FIXED;
  opt->unit = SANE_UNIT_MM;
  opt->constraint_type = SANE_CONSTRAINT_RANGE;
  opt->constraint.range = &range_y;
  opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;

  opt = scanner->opt + OPT_BR_X;
  opt->name = SANE_NAME_SCAN_BR_X;
  opt->title = SANE_TITLE_SCAN_BR_X;
  opt->desc = SANE_DESC_SCAN_BR_X;
  opt->type = SANE_TYPE_FIXED;
  opt->unit = SANE_UNIT_MM;
  opt->constraint_type = SANE_CONSTRAINT_RANGE;
  opt->constraint.range = &range_x;
  opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;

  opt = scanner->opt + OPT_BR_Y;
  opt->name = SANE_NAME_SCAN_BR_Y;
  opt->title = SANE_TITLE_SCAN_BR_Y;
  opt->desc = SANE_DESC_SCAN_BR_Y;
  opt->type = SANE_TYPE_FIXED;
  opt->unit = SANE_UNIT_MM;
  opt->constraint_type = SANE_CONSTRAINT_RANGE;
  opt->constraint.range = &range_y;
  opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;

  return SANE_STATUS_GOOD;
}

static void
do_reset (struct hp3500_data *scanner)
{
	scanner = scanner; /* kill warning */
}

static void
do_cancel (struct hp3500_data *scanner)
{
  if (scanner->reader_pid > 0)
    {

      if (sanei_thread_kill (scanner->reader_pid) == 0)
	{
          int exit_status;

          sanei_thread_waitpid(scanner->reader_pid, &exit_status);
	}
      scanner->reader_pid = 0;
    }
  if (scanner->pipe_r >= 0)
    {
      close(scanner->pipe_r);
      scanner->pipe_r = -1;
    }
}

static void
calculateDerivedValues (struct hp3500_data *scanner)
{

  DBG (12, "calculateDerivedValues\n");

  /* Convert the SANE_FIXED values for the scan area into 1/1200 inch 
   * scanner units */

  scanner->fullres_pixels.left =
    FIXED_MM_TO_SCANNER_UNIT (scanner->request_mm.left);
  scanner->fullres_pixels.top =
    FIXED_MM_TO_SCANNER_UNIT (scanner->request_mm.top);
  scanner->fullres_pixels.right =
    FIXED_MM_TO_SCANNER_UNIT (scanner->request_mm.right);
  scanner->fullres_pixels.bottom =
    FIXED_MM_TO_SCANNER_UNIT (scanner->request_mm.bottom);

  DBG (12, "\tleft margin: %u\n", scanner->fullres_pixels.left);
  DBG (12, "\ttop margin: %u\n", scanner->fullres_pixels.top);
  DBG (12, "\tright margin: %u\n", scanner->fullres_pixels.right);
  DBG (12, "\tbottom margin: %u\n", scanner->fullres_pixels.bottom);


  scanner->scan_width_pixels =
    scanner->resolution * (scanner->fullres_pixels.right -
			   scanner->fullres_pixels.left) / 1200;
  scanner->scan_height_pixels =
    scanner->resolution * (scanner->fullres_pixels.bottom -
			   scanner->fullres_pixels.top) / 1200;
  scanner->bytes_per_scan_line = scanner->scan_width_pixels * 3;

  if (scanner->scan_width_pixels < 1)
    scanner->scan_width_pixels = 1;
  if (scanner->scan_height_pixels < 1)
    scanner->scan_height_pixels = 1;

  scanner->actres_pixels.left =
    scanner->fullres_pixels.left * scanner->resolution / 1200;
  scanner->actres_pixels.top =
    scanner->fullres_pixels.top * scanner->resolution / 1200;
  scanner->actres_pixels.right =
    scanner->actres_pixels.left + scanner->scan_width_pixels;
  scanner->actres_pixels.bottom =
    scanner->actres_pixels.top + scanner->scan_height_pixels;

  scanner->actual_mm.left =
    SCANNER_UNIT_TO_FIXED_MM (scanner->fullres_pixels.left);
  scanner->actual_mm.top =
    SCANNER_UNIT_TO_FIXED_MM (scanner->fullres_pixels.top);
  scanner->actual_mm.bottom =
    SCANNER_UNIT_TO_FIXED_MM (scanner->scan_width_pixels * 1200 /
			      scanner->resolution);
  scanner->actual_mm.right =
    SCANNER_UNIT_TO_FIXED_MM (scanner->scan_height_pixels * 1200 /
			      scanner->resolution);

  DBG (12, "calculateDerivedValues: ok\n");
}

/* From here on in we have the original code written for the scanner demo */

#define	MAX_COMMANDS_BYTES	131072
#define	MAX_READ_COMMANDS	1	/* Issuing more than one register
					 * read command in a single request
					 * seems to put the device in an
					 * unpredictable state.
					 */
#define	MAX_READ_BYTES		0xffc0

#define	REG_DESTINATION_POSITION 0x60
#define	REG_MOVE_CONTROL_TEST	0xb3

static int command_reads_outstanding = 0;
static int command_bytes_outstanding = 0;
static unsigned char command_buffer[MAX_COMMANDS_BYTES];
static int receive_bytes_outstanding = 0;
static char *command_readmem_outstanding[MAX_READ_COMMANDS];
static int command_readbytes_outstanding[MAX_READ_COMMANDS];
static unsigned char sram_access_method = 0;
static unsigned sram_size = 0;
static int udh;

static int
rt_execute_commands (void)
{
  SANE_Status result;
  size_t bytes;

  if (!command_bytes_outstanding)
    return 0;

  bytes = command_bytes_outstanding;

  result = sanei_usb_write_bulk (udh, /* 0x02, */ command_buffer, &bytes);

  if (result == SANE_STATUS_GOOD && receive_bytes_outstanding)
    {
      unsigned char readbuf[MAX_READ_BYTES];
      int total_read = 0;

      do
	{
	  bytes = receive_bytes_outstanding - total_read;
	  result = sanei_usb_read_bulk (udh,
					/* 0x81, */
					readbuf + total_read, &bytes);
	  if (result == SANE_STATUS_GOOD)
	    total_read += bytes;
	  else
	    break;
	}
      while (total_read < receive_bytes_outstanding);
      if (result == SANE_STATUS_GOOD)
	{
	  unsigned char *readptr;
	  int i;

	  for (i = 0, readptr = readbuf;
	       i < command_reads_outstanding;
	       readptr += command_readbytes_outstanding[i++])
	    {
	      memcpy (command_readmem_outstanding[i],
		      readptr, command_readbytes_outstanding[i]);
	    }
	}
    }
  receive_bytes_outstanding = command_reads_outstanding =
    command_bytes_outstanding = 0;
  return (result == SANE_STATUS_GOOD) ? 0 : -1;
}

static int
rt_queue_command (int command,
		  int reg,
		  int count,
		  int bytes, void *data, int readbytes, void *readdata)
{
  int len = 4 + bytes;
  unsigned char *buffer;

  if (command_bytes_outstanding + len > MAX_COMMANDS_BYTES ||
      (readbytes &&
       ((command_reads_outstanding >= MAX_READ_COMMANDS) ||
	(receive_bytes_outstanding >= MAX_READ_BYTES))))
    {
      if (rt_execute_commands () < 0)
	return -1;
    }

  buffer = command_buffer + command_bytes_outstanding;

  buffer[0] = command;
  buffer[1] = reg;
  buffer[2] = count >> 8;
  buffer[3] = count;
  memcpy (buffer + 4, data, bytes);
  command_bytes_outstanding += 4 + bytes;
  if (readbytes)
    {
      command_readbytes_outstanding[command_reads_outstanding] = readbytes;
      command_readmem_outstanding[command_reads_outstanding] = readdata;
      receive_bytes_outstanding += readbytes;
      ++command_reads_outstanding;
    }

  return 0;
}

static int
rt_send_command_immediate (int command,
			   int reg,
			   int count,
			   int bytes,
			   void *data, int readbytes, void *readdata)
{
  rt_queue_command (command, reg, count, bytes, data, readbytes, readdata);
  return rt_execute_commands ();
}

static int
rt_queue_read_register (int reg, int bytes, void *data)
{
  return rt_queue_command (RTCMD_GETREG, reg, bytes, 0, 0, bytes, data);
}

static int
rt_read_register_immediate (int reg, int bytes, void *data)
{
  if (rt_queue_read_register (reg, bytes, data) < 0)
    return -1;
  return rt_execute_commands ();
}

static int
rt_queue_set_register (int reg, int bytes, void *data)
{
  return rt_queue_command (RTCMD_SETREG, reg, bytes, bytes, data, 0, 0);
}

static int
rt_set_register_immediate (int reg, int bytes, void *data)
{
  if (reg < 0xb3 && reg + bytes > 0xb3)
    {
      int bytes_in_first_block = 0xb3 - reg;

      if (rt_set_register_immediate (reg, bytes_in_first_block, data) < 0 ||
	  rt_set_register_immediate (0xb4, bytes - bytes_in_first_block - 1,
				     (char *) data + bytes_in_first_block + 1) < 0)
	return -1;
      return 0;
    }
  if (rt_queue_set_register (reg, bytes, data) < 0)
    return -1;
  return rt_execute_commands ();
}

static int
rt_set_one_register (int reg, int val)
{
  char r = val;

  return rt_set_register_immediate (reg, 1, &r);
}

static int
rt_write_sram (int bytes, void *data)
{
  return rt_send_command_immediate (RTCMD_WRITESRAM, 0, bytes, bytes, data, 0,
				    0);
}

static int
rt_read_sram (int bytes, void *data)
{
  return rt_send_command_immediate (RTCMD_READSRAM, 0, bytes, 0, 0, bytes,
				    data);
}

static int
rt_set_sram_page (int page)
{
  unsigned char regs[2];

  regs[0] = page;
  regs[1] = page >> 8;

  return rt_set_register_immediate (0x91, 2, regs);
}

static int
rt_detect_sram (unsigned *totalbytes, unsigned char *r93setting)
{
  char data[0x818];
  char testbuf[0x818];
  unsigned i;
  int test_values[] = { 6, 2, 1, -1 };

  for (i = 0; i < sizeof (data); ++i)
    data[i] = i % 0x61;


  for (i = 0; test_values[i] != -1; ++i)
    {
      if (rt_set_one_register (0x93, test_values[i]) ||
	  rt_set_sram_page (0x81) ||
	  rt_write_sram (0x818, data) ||
	  rt_set_sram_page (0x81) || rt_read_sram (0x818, testbuf))
	return -1;
      if (!memcmp (testbuf, data, 0x818))
	{
	  sram_access_method = test_values[i];
	  if (r93setting)
	    *r93setting = sram_access_method;
	  break;
	}
    }
  if (!sram_access_method)
    return -1;

  for (i = 0; i < 16; ++i)
    {
      int j;
      char write_data[32];
      char read_data[32];
      int pagesetting;

      for (j = 0; j < 16; j++)
	{
	  write_data[j * 2] = j * 2;
	  write_data[j * 2 + 1] = i;
	}

      pagesetting = i * 4096;


      if (rt_set_sram_page (pagesetting) < 0 ||
	  rt_write_sram (32, write_data) < 0)
	return -1;
      if (i)
	{
	  if (rt_set_sram_page (0) < 0 || rt_read_sram (32, read_data) < 0)
	    return -1;
	  if (!memcmp (read_data, write_data, 32))
	    {
	      sram_size = i * 0x20000;
	      if (totalbytes)
		*totalbytes = sram_size;
	      return 0;
	    }
	}
    }
  return -1;
}

static int
rt_get_available_bytes (void)
{
  unsigned char data[3];

  if (rt_queue_command (RTCMD_BYTESAVAIL, 0, 3, 0, 0, 3, data) < 0 ||
      rt_execute_commands () < 0)
    return -1;
  return ((unsigned) data[0]) |
    ((unsigned) data[1] << 8) | ((unsigned) data[2] << 16);
}

static int
rt_get_data (int bytes, void *data)
{
  int total = 0;

  while (bytes)
    {
      int bytesnow = bytes;

      if (bytesnow > 0xffc0)
	bytesnow = 0xffc0;
      if (rt_queue_command
	  (RTCMD_READBYTES, 0, bytesnow, 0, 0, bytesnow, data) < 0
	  || rt_execute_commands () < 0)
	return -1;
      total += bytesnow;
      bytes -= bytesnow;
      data = (char *) data + bytesnow;
    }
  return 0;
}

static int
rt_is_moving (void)
{
  char r;

  if (rt_read_register_immediate (REG_MOVE_CONTROL_TEST, 1, &r) < 0)
    return -1;
  if (r == 0x08)
    return 1;
  return 0;
}

static int
rt_is_rewound (void)
{
  char r;

  if (rt_read_register_immediate (0x1d, 1, &r) < 0)
    return -1;
  if (r & 0x02)
    return 1;
  return 0;
}

static int
rt_set_direction_forwards (unsigned char *regs)
{
  regs[0xc6] |= 0x08;
  return 0;
}

static int
rt_set_direction_rewind (unsigned char *regs)
{
  regs[0xc6] &= 0xf7;
  return 0;
}

static int
rt_set_stop_when_rewound (unsigned char *regs, int stop)
{
  if (stop)
    regs[0xb2] |= 0x10;
  else
    regs[0xb2] &= 0xef;
  return 0;
}

static int
rt_start_moving (void)
{
  if (rt_set_one_register (REG_MOVE_CONTROL_TEST, 2) < 0 ||
      rt_set_one_register (REG_MOVE_CONTROL_TEST, 2) < 0 ||
      rt_set_one_register (REG_MOVE_CONTROL_TEST, 0) < 0 ||
      rt_set_one_register (REG_MOVE_CONTROL_TEST, 0) < 0 ||
      rt_set_one_register (REG_MOVE_CONTROL_TEST, 8) < 0 ||
      rt_set_one_register (REG_MOVE_CONTROL_TEST, 8) < 0)
    return -1;
  return 0;
}

static int
rt_stop_moving (void)
{
  if (rt_set_one_register (REG_MOVE_CONTROL_TEST, 2) < 0 ||
      rt_set_one_register (REG_MOVE_CONTROL_TEST, 2) < 0 ||
      rt_set_one_register (REG_MOVE_CONTROL_TEST, 0) < 0 ||
      rt_set_one_register (REG_MOVE_CONTROL_TEST, 0) < 0)
    return -1;
  return 0;
}

static int
rt_set_powersave_mode (int enable)
{
  unsigned char r;

  if (rt_read_register_immediate (REG_MOVE_CONTROL_TEST, 1, &r) < 0)
    return -1;
  if (r & 0x04)
    {
      if (enable == 1)
	return 0;
      r &= ~0x04;
    }
  else
    {
      if (enable == 0)
	return 0;
      r |= 0x04;
    }
  if (rt_set_one_register (REG_MOVE_CONTROL_TEST, r) < 0 ||
      rt_set_one_register (REG_MOVE_CONTROL_TEST, r) < 0)
    return -1;
  return 0;
}

static int
rt_turn_off_lamp (void)
{
  return rt_set_one_register (0x3a, 0);
}

static int
rt_turn_on_lamp (void)
{
  char r3a;
  char r10;
  char r58;

  if (rt_read_register_immediate (0x3a, 1, &r3a) < 0 ||
      rt_read_register_immediate (0x10, 1, &r10) < 0 ||
      rt_read_register_immediate (0x58, 1, &r58) < 0)
    return -1;
  r3a |= 0x80;
  r10 |= 0x01;
  r58 &= 0x0f;
  if (rt_set_one_register (0x3a, r3a) < 0 ||
      rt_set_one_register (0x10, r10) < 0 ||
      rt_set_one_register (0x58, r58) < 0)
    return -1;
  return 0;
}

static int
rt_set_value_lsbfirst (unsigned char *regs,
		       int firstreg, int totalregs, unsigned value)
{
  while (totalregs--)
    {
      regs[firstreg++] = value & 0xff;
      value >>= 8;
    }
  return 0;
}

#if 0
static int
rt_set_value_msbfirst (unsigned char *regs,
		       int firstreg, int totalregs, unsigned value)
{
  while (totalregs--)
    {
      regs[firstreg + totalregs] = value & 0xff;
      value >>= 8;
    }
  return 0;
}
#endif

static int
rt_set_ccd_shift_clock_multiplier (unsigned char *regs, unsigned value)
{
  return rt_set_value_lsbfirst (regs, 0xf0, 3, value);
}

static int
rt_set_ccd_clock_reset_interval (unsigned char *regs, unsigned value)
{
  return rt_set_value_lsbfirst (regs, 0xf9, 3, value);
}

static int
rt_set_ccd_clamp_clock_multiplier (unsigned char *regs, unsigned value)
{
  return rt_set_value_lsbfirst (regs, 0xfc, 3, value);
}

static int
rt_set_movement_pattern (unsigned char *regs, unsigned value)
{
  return rt_set_value_lsbfirst (regs, 0xc0, 3, value);
}

static int
rt_set_motor_movement_clock_multiplier (unsigned char *regs, unsigned value)
{
  regs[0x40] = (regs[0x40] & ~0xc0) | (value << 6);
  return 0;
}

static int
rt_set_motor_type (unsigned char *regs, unsigned value)
{
  regs[0xc9] = (regs[0xc9] & 0xf8) | (value & 0x7);
  return 0;
}

static int
rt_set_noscan_distance (unsigned char *regs, unsigned value)
{
  DBG (10, "Setting distance without scanning to %d\n", value);
  return rt_set_value_lsbfirst (regs, 0x60, 2, value);
}

static int
rt_set_total_distance (unsigned char *regs, unsigned value)
{
  DBG (10, "Setting total distance to %d\n", value);
  return rt_set_value_lsbfirst (regs, 0x62, 2, value);
}

static int
rt_set_scanline_start (unsigned char *regs, unsigned value)
{
  return rt_set_value_lsbfirst (regs, 0x66, 2, value);
}

static int
rt_set_scanline_end (unsigned char *regs, unsigned value)
{
  return rt_set_value_lsbfirst (regs, 0x6c, 2, value);
}

static int
rt_set_basic_calibration (unsigned char *regs,
			  int redoffset1,
			  int redoffset2,
			  int redgain,
			  int greenoffset1,
			  int greenoffset2,
			  int greengain,
			  int blueoffset1, int blueoffset2, int bluegain)
{
  regs[0x05] = redoffset1;
  regs[0x02] = redoffset2;
  regs[0x08] = redgain;
  regs[0x06] = greenoffset1;
  regs[0x03] = greenoffset2;
  regs[0x09] = greengain;
  regs[0x07] = blueoffset1;
  regs[0x04] = blueoffset2;
  regs[0x0a] = bluegain;
  return 0;
}

static int
rt_set_calibration_addresses (unsigned char *regs,
			      unsigned redaddr,
			      unsigned blueaddr, unsigned greenaddr)
{
  regs[0x84] = redaddr;
  regs[0x8e] = (regs[0x8e] & 0x0f) | ((redaddr >> 4) & 0xf0);
  rt_set_value_lsbfirst (regs, 0x85, 2, blueaddr);
  rt_set_value_lsbfirst (regs, 0x87, 2, greenaddr);
  return 0;
}

static int
rt_set_lamp_duty_cycle (unsigned char *regs,
			int enable, int frequency, int offduty)
{
  if (enable)
    regs[0x3b] |= 0x80;
  else
    regs[0x3b] &= 0x7f;

  regs[0x3b] =
    (regs[0x3b] & 0x80) | ((frequency & 0x7) << 4) | (offduty & 0x0f);
  regs[0x3d] = (regs[0x3d] & 0x7f) | ((frequency & 0x8) << 4);
  return 0;
}

static int
rt_set_data_feed_on (unsigned char *regs)
{
  regs[0xb2] &= ~0x04;
  return 0;
}

static int
rt_enable_ccd (unsigned char *regs, int enable)
{
  if (enable)
    regs[0x00] &= ~0x10;
  else
    regs[0x00] |= 0x10;
  return 0;
}

static int
rt_set_cdss (unsigned char *regs, int val1, int val2)
{
  regs[0x28] = (regs[0x28] & 0xe0) | (val1 & 0x1f);
  regs[0x2a] = (regs[0x2a] & 0xe0) | (val2 & 0x1f);
  return 0;
}

static int
rt_set_cdsc (unsigned char *regs, int val1, int val2)
{
  regs[0x29] = (regs[0x29] & 0xe0) | (val1 & 0x1f);
  regs[0x2b] = (regs[0x2b] & 0xe0) | (val2 & 0x1f);
  return 0;
}

static int
rt_update_after_setting_cdss2 (unsigned char *regs)
{
  int fullcolour = (!(regs[0x2f] & 0xc0) && (regs[0x2f] & 0x04));
  int value = regs[0x2a] & 0x1f;

  regs[0x2a] = (regs[0x2a] & 0xe0) | (value & 0x1f);

  if (fullcolour)
    value *= 3;
  if ((regs[0x40] & 0xc0) == 0x40)
    value += 17;
  else
    value += 16;

  regs[0x2c] = (regs[0x2c] & 0xe0) | (value % 24);
  regs[0x2d] = (regs[0x2d] & 0xe0) | ((value + 2) % 24);
  return 0;
}

static int
rt_set_cph0s (unsigned char *regs, int on)
{
  if (on)
    regs[0x2d] |= 0x20;		/* 1200dpi horizontal coordinate space */
  else
    regs[0x2d] &= ~0x20;	/* 600dpi horizontal coordinate space */
  return 0;
}

static int
rt_set_cvtr_lm (unsigned char *regs, int val1, int val2, int val3)
{
  regs[0x28] = (regs[0x28] & ~0xe0) | (val1 << 5);
  regs[0x29] = (regs[0x29] & ~0xe0) | (val2 << 5);
  regs[0x2a] = (regs[0x2a] & ~0xe0) | (val3 << 5);
  return 0;
}

static int
rt_set_cvtr_mpt (unsigned char *regs, int val1, int val2, int val3)
{
  regs[0x3c] = (val1 & 0x0f) | (val2 << 4);
  regs[0x3d] = (regs[0x3d] & 0xf0) | (val3 & 0x0f);
  return 0;
}

static int
rt_set_cvtr_wparams (unsigned char *regs,
		     unsigned fpw, unsigned bpw, unsigned w)
{
  regs[0x31] = (w & 0x0f) | ((bpw << 4) & 0x30) | (fpw << 6);
  return 0;
}

static int
rt_enable_movement (unsigned char *regs, int enable)
{
  if (enable)
    regs[0xc3] |= 0x80;
  else
    regs[0xc3] &= ~0x80;
  return 0;
}

static int
rt_set_scan_frequency (unsigned char *regs, int frequency)
{
  regs[0x64] = (regs[0x64] & 0xf0) | (frequency & 0x0f);
  return 0;
}

static int
rt_set_merge_channels (unsigned char *regs, int on)
{
  regs[0x2f] &= ~0x14;
  regs[0x2f] |= on ? 0x04 : 0x10;
  return 0;
}

static int
rt_set_channel (unsigned char *regs, int channel)
{
  regs[0x2f] = (regs[0x2f] & ~0xc0) | (channel << 6);
  return 0;
}

static int
rt_set_single_channel_scanning (unsigned char *regs, int on)
{
  if (on)
    regs[0x2f] |= 0x20;
  else
    regs[0x2f] &= ~0x20;
  return 0;
}

static int
rt_set_colour_mode (unsigned char *regs, int on)
{
  if (on)
    regs[0x2f] |= 0x02;
  else
    regs[0x2f] &= ~0x02;
  return 0;
}

static int
rt_set_horizontal_resolution (unsigned char *regs, int resolution)
{
  if (regs[0x2d] & 0x20)
    regs[0x7a] = 1200 / resolution;
  else
    regs[0x7a] = 600 / resolution;
  return 0;
}

static int
rt_set_last_sram_page (unsigned char *regs, int pagenum)
{
  rt_set_value_lsbfirst (regs, 0x8b, 2, pagenum);
  return 0;
}

static int
rt_set_step_size (unsigned char *regs, int stepsize)
{
  rt_set_value_lsbfirst (regs, 0xe2, 2, stepsize);
  rt_set_value_lsbfirst (regs, 0xe0, 2, 0);
  return 0;
}

static int
rt_set_all_registers (void const *regs_)
{
  char regs[255];

  memcpy (regs, regs_, 255);
  regs[0x32] &= ~0x40;

  if (rt_set_one_register (0x32, regs[0x32]) < 0 ||
      rt_set_register_immediate (0, 255, regs) < 0 ||
      rt_set_one_register (0x32, regs[0x32] | 0x40) < 0)
    return -1;
  return 0;
}

static int
rt_adjust_misc_registers (unsigned char *regs)
{
  /* Mostly unknown purposes - probably no need to adjust */
  regs[0xc6] = (regs[0xc6] & 0x0f) | 0x20;	/* Purpose unknown - appears to do nothing */
  regs[0x2e] = 0x86;		/* ???? - Always has this value */
  regs[0x30] = 2;		/* CCPL = 1 */
  regs[0xc9] |= 0x38;		/* Doesn't have any obvious effect, but the Windows driver does this */
  return 0;
}


#define NVR_MAX_ADDRESS_SIZE	11
#define NVR_MAX_OPCODE_SIZE	3
#define NVR_DATA_SIZE		8
#define	NVR_MAX_COMMAND_SIZE	((NVR_MAX_ADDRESS_SIZE + \
				  NVR_MAX_OPCODE_SIZE + \
				  NVR_DATA_SIZE) * 2 + 1)

static int
rt_nvram_enable_controller (int enable)
{
  unsigned char r;

  if (rt_read_register_immediate (0x1d, 1, &r) < 0)
    return -1;
  if (enable)
    r |= 1;
  else
    r &= ~1;
  return rt_set_one_register (0x1d, r);

}

static int
rt_nvram_init_command (void)
{
  unsigned char regs[13];

  if (rt_read_register_immediate (0x10, 13, regs) < 0)
    return -1;
  regs[2] |= 0xf0;
  regs[4] = (regs[4] & 0x1f) | 0x60;
  return rt_set_register_immediate (0x10, 13, regs);
}

static int
rt_nvram_init_stdvars (int block, int *addrbits, unsigned char *basereg)
{
  int bitsneeded;
  int capacity;

  switch (block)
    {
    case 0:
      bitsneeded = 7;
      break;

    case 1:
      bitsneeded = 9;
      break;

    case 2:
      bitsneeded = 11;
      break;

    default:
      bitsneeded = 0;
      capacity = 1;
      while (capacity < block)
	capacity <<= 1, ++bitsneeded;
      break;
    }

  *addrbits = bitsneeded;

  if (rt_read_register_immediate (0x10, 1, basereg) < 0)
    return -1;

  *basereg &= ~0x60;
  return 0;
}

static void
rt_nvram_set_half_bit (unsigned char *buffer,
		       int value, unsigned char stdbits, int whichhalf)
{
  *buffer = stdbits | (value ? 0x40 : 0) | (whichhalf ? 0x20 : 0);
}

static void
rt_nvram_set_command_bit (unsigned char *buffer,
			  int value, unsigned char stdbits)
{
  rt_nvram_set_half_bit (buffer, value, stdbits, 0);
  rt_nvram_set_half_bit (buffer + 1, value, stdbits, 1);
}

static void
rt_nvram_set_addressing_bits (unsigned char *buffer,
			      int location,
			      int addressingbits, unsigned char stdbits)
{
  int currentbit = 1 << (addressingbits - 1);

  while (addressingbits--)
    {
      rt_nvram_set_command_bit (buffer,
				(location & currentbit) ? 1 : 0, stdbits);
      buffer += 2;
      currentbit >>= 1;
    }
}

#if 0
static int
rt_nvram_enable_write (int addressingbits, int enable, unsigned char stdbits)
{
  unsigned char cmdbuffer[NVR_MAX_COMMAND_SIZE];
  int cmdsize = 6 + addressingbits * 2;

  rt_nvram_set_command_bit (cmdbuffer, 1, stdbits);
  rt_nvram_set_command_bit (cmdbuffer + 2, 0, stdbits);
  rt_nvram_set_command_bit (cmdbuffer + 4, 0, stdbits);
  rt_nvram_set_command_bit (cmdbuffer + 6, enable, stdbits);
  if (addressingbits > 1)
    rt_nvram_set_addressing_bits (cmdbuffer + 8, 0, addressingbits - 1,
				  stdbits);

  if (rt_nvram_enable_controller (1) < 0 ||
      rt_send_command_immediate (RTCMD_NVRAMCONTROL, 0, cmdsize, cmdsize,
				 cmdbuffer, 0, 0) < 0
      || rt_nvram_enable_controller (0) < 0)
    {
      return -1;
    }
  return 0;
}

static int
rt_nvram_write (int block, int location, char const *data, int bytes)
{
  int addressingbits;
  unsigned char stdbits;
  unsigned char cmdbuffer[NVR_MAX_COMMAND_SIZE];
  unsigned char *address_bits;
  unsigned char *data_bits;
  int cmdsize;

  /* This routine doesn't appear to work, but I can't see anything wrong with it */
  if (rt_nvram_init_stdvars (block, &addressingbits, &stdbits) < 0)
    return -1;

  cmdsize = (addressingbits + 8) * 2 + 6;
  address_bits = cmdbuffer + 6;
  data_bits = address_bits + (addressingbits * 2);

  rt_nvram_set_command_bit (cmdbuffer, 1, stdbits);
  rt_nvram_set_command_bit (cmdbuffer + 2, 0, stdbits);
  rt_nvram_set_command_bit (cmdbuffer + 4, 1, stdbits);

  if (rt_nvram_init_command () < 0 ||
      rt_nvram_enable_write (addressingbits, 1, stdbits) < 0)
    return -1;

  while (bytes--)
    {
      int i;

      rt_nvram_set_addressing_bits (address_bits, location, addressingbits,
				    stdbits);
      rt_nvram_set_addressing_bits (data_bits, *data++, 8, stdbits);

      if (rt_nvram_enable_controller (1) < 0 ||
	  rt_send_command_immediate (RTCMD_NVRAMCONTROL, 0, cmdsize, cmdsize,
				     cmdbuffer, 0, 0) < 0
	  || rt_nvram_enable_controller (0) < 0)
	return -1;

      if (rt_nvram_enable_controller (1) < 0)
	return -1;
      for (i = 0; i < cmdsize; ++i)
	{
	  unsigned char r;
	  unsigned char cmd;

	  rt_nvram_set_half_bit (&cmd, 0, stdbits, i & 1);
	  if (rt_send_command_immediate
	      (RTCMD_NVRAMCONTROL, 0, 1, 1, &cmd, 0, 0) < 0
	      || rt_read_register_immediate (0x10, 1, &r) < 0)
	    {
	      return -1;
	    }
	  else if (r & 0x80)
	    {
	      break;
	    }
	}
      if (rt_nvram_enable_controller (0) < 0)
	return -1;

      ++location;
    }

  if (rt_nvram_enable_write (addressingbits, 0, stdbits) < 0)
    return -1;
  return 0;
}
#endif

static int
rt_nvram_read (int block, int location, unsigned char *data, int bytes)
{
  int addressingbits;
  unsigned char stdbits;
  unsigned char cmdbuffer[NVR_MAX_COMMAND_SIZE];
  unsigned char *address_bits;
  unsigned char readbit_command[2];
  int cmdsize;

  if (rt_nvram_init_stdvars (block, &addressingbits, &stdbits) < 0)
    return -1;

  cmdsize = addressingbits * 2 + 7;
  address_bits = cmdbuffer + 6;

  rt_nvram_set_command_bit (cmdbuffer, 1, stdbits);
  rt_nvram_set_command_bit (cmdbuffer + 2, 1, stdbits);
  rt_nvram_set_command_bit (cmdbuffer + 4, 0, stdbits);
  rt_nvram_set_half_bit (cmdbuffer + cmdsize - 1, 0, stdbits, 0);

  rt_nvram_set_half_bit (readbit_command, 0, stdbits, 1);
  rt_nvram_set_half_bit (readbit_command + 1, 0, stdbits, 0);

  if (rt_nvram_init_command () < 0)
    return -1;

  while (bytes--)
    {
      char c = 0;
      unsigned char r;
      int i;

      rt_nvram_set_addressing_bits (address_bits, location, addressingbits,
				    stdbits);

      if (rt_nvram_enable_controller (1) < 0 ||
	  rt_send_command_immediate (RTCMD_NVRAMCONTROL, 0x1d, cmdsize,
				     cmdsize, cmdbuffer, 0, 0) < 0)
	return -1;

      for (i = 0; i < 8; ++i)
	{
	  c <<= 1;

	  if (rt_send_command_immediate
	      (RTCMD_NVRAMCONTROL, 0x1d, 2, 2, readbit_command, 0, 0) < 0
	      || rt_read_register_immediate (0x10, 1, &r) < 0)
	    return -1;
	  if (r & 0x80)
	    c |= 1;
	}
      if (rt_nvram_enable_controller (0) < 0)
	return -1;

      *data++ = c;
      ++location;
    }
  return 0;
}

static unsigned char initial_regs[] = {
  0xf5, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x70,
    0x00, 0x00, 0x00, 0x00,
  0xe1, 0xfc, 0xff, 0xff, 0x00, 0x00, 0x00, 0xfc, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x02, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x06, 0x19,
  0xd0, 0x7a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0, 0x37,
    0xff, 0x0f, 0x00, 0x00,
  0x80, 0x00, 0x00, 0x00, 0x8c, 0x76, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
  0x20, 0xbc, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1d, 0x1f, 0x00, 0x1f,
    0x00, 0x00, 0x00, 0x00,
  0x5e, 0xea, 0x5f, 0xea, 0x00, 0x80, 0x64, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x84, 0x04, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
  0x0f, 0x02, 0x4b, 0x02, 0x00, 0xec, 0x19, 0xd8, 0x2d, 0x87, 0x02, 0xff,
    0x3f, 0x78, 0x60, 0x00,
  0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x0c, 0x27, 0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
  0x12, 0x08, 0x06, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
  0xff, 0xbf, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00
};

static struct tg_info__
{
  int tg_cph0p;
  int tg_crsp;
  int tg_cclpp;
  int tg_cdss1;
  int tg_cdsc1;
  int tg_cdss2;
  int tg_cdsc2;
} tg_info[] =
{
  /*      CPH0P           CRSP            CCLPP           CDSS1   CDSC1   CDSS2   CDSC2   */
  {
  0x01FFE0, 0x3c0000, 0x003000, 0xb, 0xd, 0x00, 0x01},	/* NORMAL */
  {
  0x7ff800, 0xf00000, 0x01c000, 0xb, 0xc, 0x14, 0x15}	/* DOUBLE */
};

struct resolution_parameters
{
  unsigned resolution;
  int reg_39_value;
  int reg_c3_value;
  int reg_c6_value;
  int scan_frequency;
  int cph0s;
  int red_green_offset;
  int green_blue_offset;
  int intra_channel_offset;
  int motor_movement_clock_multiplier;
  int tg;
  int step_size;
};

static struct resolution_parameters resparms[] = {
  /* My values - all work */
  {1200, 3, 6, 4, 2, 1, 22, 22, 4, 2, 0, 0x157b},
  {600, 3, 3, 1, 1, 0, 9, 10, 0, 2, 0, 0x157b},
  {400, 1, 1, 1, 1, 1, 6, 6, 1, 2, 0, 0x157b},
  {300, 3, 3, 3, 1, 0, 5, 4, 0, 2, 1, 0x157b},
  {200, 3, 1, 1, 1, 0, 3, 3, 0, 2, 1, 0x157b},
  {150, 3, 3, 3, 2, 0, 2, 2, 0, 2, 1, 0x157b},
  {100, 3, 1, 3, 1, 0, 1, 1, 0, 2, 1, 0x157b},
  {75, 3, 3, 3, 4, 0, 1, 1, 0, 2, 1, 0x157b},
  {50, 3, 1, 3, 2, 0, 0, 0, 0, 2, 1, 0x157b},
  {25, 3, 1, 3, 4, 0, 0, 0, 0, 2, 1, 0x157b},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x157b}
};

struct dcalibdata
{
  unsigned *buffers[3];
  int pixelsperrow;
  int pixelnow;
  int channelnow;
  int rowsdone;
};

static int
rts8801_rewind (void)
{
  unsigned char regs[255];
  int n;

  rt_read_register_immediate (0, 255, regs);

  rt_set_noscan_distance (regs, 59998);
  rt_set_total_distance (regs, 59999);

  rt_set_stop_when_rewound (regs, 0);

  rt_set_one_register (0xc6, 0);
  rt_set_one_register (0xc6, 0);

  rt_set_step_size (regs, 0x0abd);

  rt_set_direction_rewind (regs);

  regs[0x39] = 15;
  regs[0xc3] = (regs[0xc3] & 0xf8) | 0x81;
  regs[0xc6] = (regs[0xc6] & 0xf8) | 3;

  rt_set_all_registers (regs);
  rt_start_moving ();

  while (!rt_is_rewound () &&
	 ((n = rt_get_available_bytes ()) > 0 || rt_is_moving () > 0))
    {
      if (n)
	{
	  char buffer[0xffc0];

	  if (n > (int) sizeof (buffer))
	    n = sizeof (buffer);
	  rt_get_data (n, buffer);
	}
      else
	{
	  usleep (10000);
	}
    }

  rt_stop_moving ();
  return 0;
}

static int cancelled_scan = 0;

static int
rts8801_doscan (unsigned width,
		unsigned colour,
		unsigned red_green_offset,
		unsigned green_blue_offset,
		unsigned intra_channel_offset,
		rts8801_callback cbfunc,
		void *params,
		int oddfirst,
		unsigned char const *calib_info, struct dcalibdata *pdcd)
{
  unsigned rowbytes = 0;
  unsigned channels = 0;
  unsigned total_rows = 0;
  unsigned bytesperchannel;
  char *row_buffer;
  char *output_buffer;
  unsigned buffered_rows;
  int rows_to_begin;
  int rowbuffer_bytes;
  int n;
  unsigned rownow = 0;
  unsigned bytenow = 0;
  char *channel_data[3][2];
  unsigned i;
  unsigned j;
  int result = 0;

  calib_info = calib_info; /* Kill warning */
  if (cancelled_scan)
    return -1;
  rt_start_moving ();

  switch (colour)
    {
    case RTS8801_GREYSCALE:
      channels = 1;
      rowbytes = width;
      bytesperchannel = rowbytes;
      break;

    case RTS8801_COLOUR:
      channels = 3;
      rowbytes = width * 3;
      bytesperchannel = width;
      break;

    case RTS8801_BW:
      channels = 1;
      rowbytes = width / 8;
      bytesperchannel = rowbytes;
      break;
    }

  buffered_rows =
    red_green_offset + green_blue_offset + intra_channel_offset + 1;
  rows_to_begin = buffered_rows;
  rowbuffer_bytes = buffered_rows * rowbytes;
  row_buffer = (char *) malloc (rowbuffer_bytes);
  output_buffer = (char *) malloc (rowbytes);

  for (i = j = 0; i < channels; ++i)
    {
      if (i == 1)
	j += red_green_offset;
      else if (i == 2)
	j += green_blue_offset;
      channel_data[i][1 - oddfirst] = row_buffer + rowbytes * j + width * i;
      channel_data[i][oddfirst] =
	channel_data[i][1 - oddfirst] + rowbytes * intra_channel_offset;
    }

  while (((n = rt_get_available_bytes ()) > 0 || rt_is_moving () > 0) && !cancelled_scan)
    {
      if (n == 1 && (rt_is_moving () || rt_get_available_bytes () != 1))
	n = 0;
      if (n > 0)
	{
	  char buffer[0xffc0];

	  if (n > 0xffc0)
	    n = 0xffc0;
	  else if ((n > 1) && (n & 1))
	    --n;
	  if (rt_get_data (n, buffer) >= 0)
	    {
	      char *bufnow = buffer;

	      while (n)
		{
		  int numcopy = rowbytes - bytenow;

		  if (numcopy > n)
		    numcopy = n;
		  memcpy (row_buffer + rownow * rowbytes + bytenow, bufnow,
			  numcopy);
		  bytenow += numcopy;
		  bufnow += numcopy;
		  n -= numcopy;

		  if (bytenow == rowbytes)
		    {
		      if (!rows_to_begin || !--rows_to_begin)
			{
			  char *outnow = output_buffer;

			  for (i = 0; i < width; ++i)
			    {
			      for (j = 0; j < channels; ++j)
				{
				  unsigned pix =
				    (unsigned char) channel_data[j][i & 1][i];

				  if (pdcd)
				    {
				      /* 5400 is "magic" - chosen because it works */
				      pix = pix * 5400 / pdcd->buffers[j][i];
				      if (pix > 255)
					pix = 255;
				    }

				  *outnow++ = pix;
				}
			    }

			  if (!((*cbfunc) (params, rowbytes, output_buffer)))
			    break;

			  for (i = 0; i < channels; ++i)
			    {
			      for (j = 0; j < 2; ++j)
				{
				  channel_data[i][j] += rowbytes;
				  if (channel_data[i][j] - row_buffer >=
				      rowbuffer_bytes)
				    channel_data[i][j] -= rowbuffer_bytes;
				}
			    }
			}
		      ++total_rows;
		      if (++rownow == buffered_rows)
			rownow = 0;
		      bytenow = 0;
		    }
		}
	    }
	  DBG (10, "\rtotal_rows = %d", total_rows);
	}
      else
	{
	  usleep (10000);
	}
    }
  DBG (10, "\n");
  if (n < 0)
    result = -1;

  free (output_buffer);
  free (row_buffer);

  rt_stop_moving ();
  return result;
}

static unsigned local_sram_size;
static unsigned char r93setting;

#define RTS8801_F_SUPPRESS_MOVEMENT	1

static int
rts8801_fullscan (unsigned x,
		  unsigned y,
		  unsigned w,
		  unsigned h,
		  unsigned xresolution,
		  unsigned yresolution,
		  unsigned colour,
		  rts8801_callback cbfunc,
		  void *param,
		  unsigned char *calib_info,
		  int flags,
		  unsigned red_calib_offset,
		  unsigned green_calib_offset,
		  unsigned blue_calib_offset, struct dcalibdata *pdcd)
{
  int ires, jres;
  int tg_setting;
  unsigned char regs[256];
  unsigned char offdutytime;
  int result;

  /* Kill warnings */
  red_calib_offset = red_calib_offset;
  blue_calib_offset = blue_calib_offset;
  green_calib_offset = green_calib_offset;

  for (ires = 0;
       resparms[ires].resolution && resparms[ires].resolution != xresolution;
       ++ires);
  if (resparms[ires].resolution == 0)
    return -1;
  for (jres = 0;
       resparms[jres].resolution && resparms[jres].resolution != yresolution;
       ++jres);
  if (resparms[jres].resolution == 0)
    return -1;

  /* Set scan parameters */

  rt_read_register_immediate (0, 255, regs);
  regs[255] = 0;

  rt_enable_ccd (regs, 1);
  rt_enable_movement (regs, 1);
  rt_set_scan_frequency (regs, 1);

  rt_adjust_misc_registers (regs);

  rt_set_cvtr_wparams (regs, 3, 0, 6);
  rt_set_cvtr_mpt (regs, 15, 15, 15);
  rt_set_cvtr_lm (regs, 7, 7, 7);
  rt_set_motor_type (regs, 2);

  if (rt_nvram_read (0, 0x7b, &offdutytime, 1) < 0 || offdutytime >= 15)
    {
      offdutytime = 6;
    }
  rt_set_lamp_duty_cycle (regs, 1,	/* On */
			  10,	/* Frequency */
			  offdutytime);	/* Off duty time */

  rt_set_movement_pattern (regs, 0x800000);


  tg_setting = resparms[jres].tg;
  rt_set_ccd_shift_clock_multiplier (regs, tg_info[tg_setting].tg_cph0p);
  rt_set_ccd_clock_reset_interval (regs, tg_info[tg_setting].tg_crsp);
  rt_set_ccd_clamp_clock_multiplier (regs, tg_info[tg_setting].tg_cclpp);


  rt_set_one_register (0xc6, 0);
  rt_set_one_register (0xc6, 0);

  rt_set_step_size (regs, resparms[jres].step_size);

  rt_set_direction_forwards (regs);

  rt_set_stop_when_rewound (regs, 0);
  rt_set_data_feed_on (regs);

  rt_set_calibration_addresses (regs, 0, 0, 0);

  rt_set_basic_calibration (regs,
			    calib_info[0], calib_info[1], calib_info[2],
			    calib_info[3], calib_info[4], calib_info[5],
			    calib_info[6], calib_info[7], calib_info[8]);
  regs[0x0b] = 0x70;		/* If set to 0x71, the alternative, all values are low */

#if 0
  if (red_calib_offset >= 0 && green_calib_offset >= 0
      && blue_calib_offset >= 0)
    {
      rt_set_calibration_addresses (regs, red_calib_offset, blue_calib_offset,
				    green_calib_offset);
      regs[0x40] = 0x3d;
      pdcd = 0;
    }
#endif

  rt_set_channel (regs, RT_CHANNEL_ALL);
  rt_set_single_channel_scanning (regs, 0);
  rt_set_merge_channels (regs, 0);
  rt_set_colour_mode (regs, 1);

  rt_set_motor_movement_clock_multiplier (regs,
					  resparms[jres].
					  motor_movement_clock_multiplier);

  rt_set_cdss (regs, tg_info[tg_setting].tg_cdss1,
	       tg_info[tg_setting].tg_cdss2);
  rt_set_cdsc (regs, tg_info[tg_setting].tg_cdsc1,
	       tg_info[tg_setting].tg_cdsc2);

  rt_update_after_setting_cdss2 (regs);

  rt_set_last_sram_page (regs, (local_sram_size - 1) >> 5);

  regs[0x39] = resparms[jres].reg_39_value;
  regs[0xc3] = (regs[0xc3] & 0xf8) | resparms[jres].reg_c3_value;
  regs[0xc6] = (regs[0xc6] & 0xf8) | resparms[jres].reg_c6_value;
  rt_set_scan_frequency (regs, resparms[jres].scan_frequency);
  rt_set_cph0s (regs, resparms[ires].cph0s);

  if (flags & RTS8801_F_SUPPRESS_MOVEMENT)
    regs[0xc3] &= 0x7f;
  rt_set_horizontal_resolution (regs, xresolution);

  rt_set_noscan_distance (regs, y * resparms[jres].scan_frequency - 1);
  rt_set_total_distance (regs, resparms[jres].scan_frequency *
			 (y +
			  h +
			  ((colour ==
			    RTS8801_COLOUR) ? (resparms[jres].
					       red_green_offset +
					       resparms[jres].
					       green_blue_offset) : 0) +
			  resparms[jres].intra_channel_offset) - 1);

  rt_set_scanline_start (regs,
			 x * (1200 / xresolution) /
			 (resparms[ires].cph0s ? 1 : 2));
  rt_set_scanline_end (regs,
		       (x +
			w) * (1200 / xresolution) /
		       (resparms[ires].cph0s ? 1 : 2));

  rt_set_all_registers (regs);

  rt_set_one_register (0x2c, regs[0x2c]);

  result = rts8801_doscan (w,
			   colour,
			   resparms[jres].red_green_offset,
			   resparms[jres].green_blue_offset,
			   resparms[jres].intra_channel_offset,
			   cbfunc, param, (x & 1), calib_info, pdcd);

  return result;
}

static int
sumfunc (struct dcalibdata *dcd, int bytes, char *data)
{
  unsigned char *c = (unsigned char *) data;

  while (bytes > 0)
    {
      if (dcd->rowsdone)
	dcd->buffers[dcd->channelnow][dcd->pixelnow] += *c;
      if (++dcd->channelnow >= 3)
	{
	  dcd->channelnow = 0;
	  if (++dcd->pixelnow >= dcd->pixelsperrow)
	    {
	      dcd->pixelnow = 0;
	      ++dcd->rowsdone;
	    }
	}
      c++;
      bytes--;
    }
  return 1;
}

struct calibdata
{
  unsigned char *buffer;
  int space;
};

static int
storefunc (struct calibdata *cd, int bytes, char *data)
{
  if (cd->space > 0)
    {
      if (bytes > cd->space)
	bytes = cd->space;
      memcpy (cd->buffer, data, bytes);
      cd->buffer += bytes;
      cd->space -= bytes;
    }
  return 1;
}

#if 0
static void
show_calib_results (unsigned char const *buffer, int n)
{
  int i = 0;

  while (n > 0)
    {
      int j;

      DBG (10, "%02x: ", i);
      for (j = 0; j < 8; ++j)
	{
	  if (j < n)
	    DBG (10, "%02x ", buffer[j]);
	  else
	    DBG (10, "   ");
	}
      DBG (10, "-");
      for (; j < 16; ++j)
	{
	  if (j < n)
	    DBG (10, " %02x", buffer[j]);
	  else
	    DBG (10, "   ");
	}
      DBG (10, "\n");
      i += 16;
      n -= 16;
      buffer += 16;
    }

}
#endif

static unsigned
sum_channel (unsigned char *p, int n, int bytwo)
{
  unsigned v = 0;

  while (n-- > 0)
    {
      v += *p;
      p += 3;
      if (bytwo)
	p += 3;
    }
  return v;
}

static int
constrain (int val, int min, int max)
{
  if (val < min)
    {
      DBG (10, "Clipped %d to %d\n", val, min);
      val = min;
    }
  else if (val > max)
    {
      DBG (10, "Clipped %d to %d\n", val, max);
      val = max;
    }
  return val;
}

static int do_warmup = 1;

static int
rts8801_scan (unsigned x,
	      unsigned y,
	      unsigned w,
	      unsigned h,
	      unsigned resolution,
	      unsigned colour, rts8801_callback cbfunc, void *param)
{
  unsigned char calib_info[9];
  unsigned char calibbuf[2400];
  struct dcalibdata dcd;
  struct calibdata cd;
  unsigned *piSums;
  int iCalibOffset;
  int iCalibX;
  int iCalibY;
  int iCalibWidth;
  int iCalibTarget;
  int iCalibPixels;
  int iMoveFlags = 0;
#if 0
  unsigned char *pDetailedCalib;
  unsigned aiBestYet[3];
  int j, n;
#endif
  unsigned int aiLow[3] = { 0, 0, 0 };
  unsigned int aiHigh[3] = { 256, 256, 256 };
#if 0
  unsigned aiLowTotals[3];
  unsigned aiLowOffset[3];
#endif
  unsigned aiBestOffset[3];
  int i;
  int anychanged;

  /* Initialise and power up */

  rt_set_all_registers (initial_regs);
  rt_set_powersave_mode (0);

  /* Initial rewind in case scanner is stuck away from home position */

  rts8801_rewind ();

  /* Detect SRAM */

  rt_detect_sram (&local_sram_size, &r93setting);

  /* Warm up the lamp */

  DBG (10, "Warming up the lamp\n");

  rt_turn_on_lamp ();
  if (do_warmup)
    sleep (20);

  /* Basic calibration */

  DBG (10, "Calibrating (stage 1)\n");

  calib_info[2] = calib_info[5] = calib_info[8] = 1;

  calib_info[0] = calib_info[1] = calib_info[3] = calib_info[4] =
    calib_info[6] = calib_info[7] = 0xb4;

  iCalibOffset = 0;		/* Note that horizontal resolution is always 600dpi for calibration. 330 is 110 dots in (for R,G,B channels) */
  iCalibX = 1;
  iCalibPixels = 50;
  iCalibY = (resolution == 25) ? 1 : 2;	/* Was 1200 / resolution, which would take us past the calibration area for 50dpi */
  iCalibWidth = 100;
  iCalibTarget = 550;

  for (i = 0; i < 3; ++i)
    aiBestOffset[i] = 0xb4;

  do
    {
      anychanged = 0;

      for (i = 0; i < 3; ++i)
	{
	  aiBestOffset[i] = (aiHigh[i] + aiLow[i] + 1) / 2;
	}

      for (i = 0; i < 3; ++i)
	calib_info[i * 3] = calib_info[i * 3 + 1] = aiBestOffset[i];

      cd.buffer = calibbuf;
      cd.space = sizeof (calibbuf);
      rts8801_fullscan (iCalibX, iCalibY, iCalibWidth, 2, 600, resolution,
			RTS8801_COLOUR, (rts8801_callback) storefunc, &cd,
			calib_info, iMoveFlags, -1, -1, -1, 0);
      iMoveFlags = RTS8801_F_SUPPRESS_MOVEMENT;

      for (i = 0; i < 3; ++i)
	{
	  int sum;

	  if (aiBestOffset[i] >= 255)
	    continue;
	  sum = sum_channel (calibbuf + iCalibOffset + i, iCalibPixels, 0);
	  DBG (10, "channel[%d] sum = %d (target %d)\n", i, sum,
	       iCalibTarget);

	  if (sum >= iCalibTarget)
	    aiHigh[i] = aiBestOffset[i];
	  else
	    aiLow[i] = aiBestOffset[i];
	}
    }
  while (aiLow[0] < aiHigh[0] - 1 && aiLow[1] < aiHigh[1] - 1
	 && aiLow[1] < aiHigh[1] + 1);

  cd.buffer = calibbuf;
  cd.space = sizeof (calibbuf);
  rts8801_fullscan (iCalibX + 2100, iCalibY, iCalibWidth, 2, 600, resolution,
		    RTS8801_COLOUR, (rts8801_callback) storefunc, &cd,
		    calib_info, RTS8801_F_SUPPRESS_MOVEMENT, -1, -1, -1, 0);

  for (i = 0; i < 3; ++i)
    calib_info[i * 3 + 2] =
      constrain (60000 / sum_channel (calibbuf + i, 50, 0), 0, 255);

  for (i = 0; i < 3; ++i)
    {
      DBG (10, "Channel [%d] gain=%02x  offset=%02x\n",
	   i, calib_info[i * 3] + 2, calib_info[i * 3]);
    }

  /* Stage 2 calibration */

  DBG (10, "Calibrating (stage 2)\n");

  piSums = (unsigned *) malloc (sizeof (unsigned) * w * 3);
  memset (piSums, 0, sizeof (unsigned) * w * 3);

  dcd.buffers[0] = piSums;
  dcd.buffers[1] = piSums + w;
  dcd.buffers[2] = dcd.buffers[1] + w;
  dcd.pixelsperrow = w;
  dcd.pixelnow = dcd.channelnow = dcd.rowsdone = 0;

  DBG (10, "Performing detailed calibration scan\n");
  rts8801_fullscan (x, iCalibY, w, 21, resolution, resolution, colour,
		    (rts8801_callback) sumfunc, &dcd, calib_info,
		    RTS8801_F_SUPPRESS_MOVEMENT, -1, -1, -1, 0);

  DBG (10, "Detailed calibration scan completed\n");
#if 0
/* I haven't been able to get the scanner's per-element calibration to work at all yet, and when attempting to do
 * so I fequently cause the scanner to lock up.
 */
/*	pDetailedCalib = (unsigned char *) malloc(w * 6 + 1536); */
  pDetailedCalib = (unsigned char *) malloc (0x41e0);
  memset (pDetailedCalib, 0, 0x41e0);
  for (i = 0; i < 3; ++i)
    {
      for (j = 0; j < 256; ++j)
	{
	  x = j * 2;
	  pDetailedCalib[x] = pDetailedCalib[x + 512] =
	    pDetailedCalib[x + 1024] = j;
	  pDetailedCalib[x + 1] = pDetailedCalib[x + 513] =
	    pDetailedCalib[x + 1025] = j + 1;
	}
      pDetailedCalib[511] = pDetailedCalib[1023] = pDetailedCalib[1535] = -1;
      for (j = 0; j < w; ++j)
	{
	  unsigned avnow = (dcd.buffers[i][j] + 39) / 40;
	  unsigned valnow = 0xe000 / (avnow ? avnow : 1);
	  int idx = (i * w + j) * 2 + 1536;

/*	if (j & 0x02) valnow = 0xe000; else valnow = 1;
	valnow = 0;
*/
	  if (i == 1 && j == w / 2)
	    valnow = 0xe000;
	  else
	    valnow = 1;
	  pDetailedCalib[idx] = valnow;
	  pDetailedCalib[idx + 1] = valnow >> 8;
	}
    }

/*	rt_set_sram_page(0); */
#if 0
  DBG (10, "Calibrations calculated, writing\n");

  n = 0x41e0;
  i = j = 0;
  rt_set_one_register (0x93, r93setting);
  DBG (10, "Register 0x93 should have been set\n");
  while (n > 0)
    {
      int w = n;

      if (w > 32)
	w = 32;
      DBG (10, "%d", ++j);
      fflush (stdout);
      rt_write_sram (w, pDetailedCalib + i);
      DBG (10, "...");
      n -= w;
      i += w;
    }
  DBG (10, "\n");

/*	show_calib_results(pDetailedCalib, 6 * w);
	rt_write_sram(1536 + 6 * w, pDetailedCalib); */
  rt_get_available_bytes ();
#endif
  rt_set_sram_page (0);
  rt_set_one_register (0x93, r93setting);
  rt_write_sram (0x41e0, pDetailedCalib);
  rt_get_available_bytes ();
  free (pDetailedCalib);
#endif


  /* And finally, perform the scan */

  DBG (10, "Scanning\n");

  rts8801_rewind ();

  rts8801_fullscan (x, y, w, h, resolution, resolution, colour, cbfunc, param,
		    calib_info, 0, 1536, 1536 + w * 2, 1536 + w * 4, &dcd);

  rt_turn_off_lamp ();
  rts8801_rewind ();
  rt_set_powersave_mode (1);

  free (piSums);
  return 0;
}

static int
writefunc (struct hp3500_data *scanner, int bytes, char *data)
{
  return write (scanner->pipe_w, data, bytes) == bytes;
}

static void
sigtermHandler (int signal)
{
  signal = signal;              /* get rid of compiler warning */
  cancelled_scan = 1;
}

static int
reader_process (void *pv)
{
  struct hp3500_data *scanner = pv;
  time_t t;
  sigset_t ignore_set;
  sigset_t sigterm_set;
  struct SIGACTION act;
 
  if (sanei_thread_is_forked())
   {
     close (scanner->pipe_r);
   }

  sigfillset (&ignore_set);
  sigdelset (&ignore_set, SIGTERM);
#if defined (__APPLE__) && defined (__MACH__)
  sigdelset (&ignore_set, SIGUSR2);
#endif
  sigprocmask (SIG_SETMASK, &ignore_set, 0);

  sigemptyset (&sigterm_set);
  sigaddset (&sigterm_set, SIGTERM);

  memset (&act, 0, sizeof (act));
#ifdef _POSIX_SOURCE
  act.sa_handler = sigtermHandler;
#endif
  sigaction (SIGTERM, &act, 0);


  /* Warm up the lamp again if our last scan ended more than 5 minutes ago. */
  time (&t);
  do_warmup = (t - scanner->last_scan) > 300;

  udh = scanner->sfd;

  cancelled_scan = 0;

  DBG (10, "Scanning at %ddpi\n", scanner->resolution);
  if (rts8801_scan
      (scanner->actres_pixels.left + 250 * scanner->resolution / 1200,
       scanner->actres_pixels.top + 599 * scanner->resolution / 1200,
       scanner->actres_pixels.right - scanner->actres_pixels.left,
       scanner->actres_pixels.bottom - scanner->actres_pixels.top,
       scanner->resolution, RTS8801_COLOUR, (rts8801_callback) writefunc,
       scanner) >= 0)
    exit (SANE_STATUS_GOOD);
  exit (SANE_STATUS_IO_ERROR);
}
