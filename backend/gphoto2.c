/* Please note!  This is extremely alpha code, and is really intended as
 * a "proof of concept" since I don't yet know whether it's going to 
 * to be practical and/or possible to implement a the complete backend.
 * The current implemenation uses the gphoto2 command as the interface
 * to the camera - the longterm plan is access the cameras directly by
 * linking with the gphoto2 libraries.  It's also been tested with only
 * one camera model, the Kodak DC240 which happens to be the camera I
 * have.  I'm very interested in learning what it would take to support
 * more cameras.
 *  
 * However, having said that, I've already found it to be quite useful
 * even in its current form - one reason is that gphoto2 provides access
 * to the camera via USB which is not supported by the regular DC240 
 * backend and is dramatically faster than the serial port.
 */

/***************************************************************************
 * _S_A_N_E - Scanner Access Now Easy.

   gphoto2.c 

   03/12/01 - Peter Fales

   Based on the dc210 driver, (C) 1998 Brian J. Murrell (which is
	based on dc25 driver (C) 1998 by Peter Fales)
	
   This file (C) 2001 by Peter Fales

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

 ***************************************************************************

   This file implements a SANE backend for the Kodak DC-240
   digital camera.  THIS IS EXTREMELY ALPHA CODE!  USE AT YOUR OWN RISK!! 

   (feedback to:  peter@fales.com

   This backend is based somewhat on the dc25 backend included in this
   package by Peter Fales, and the dc210 backend by Brian J. Murrell

 ***************************************************************************/

#include "sane/config.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include "cdjpeg.h"
#include <sys/ioctl.h>

#include "sane/sane.h"
#include "sane/sanei.h"
#include "sane/saneopts.h"

#define BACKEND_NAME	gphoto2
#include "sane/sanei_backend.h"

#include "gphoto2.h"

#ifndef PATH_MAX
# define PATH_MAX	1024
#endif

#define MAGIC			(void *)0xab730324
#define GPHOTO2_CONFIG_FILE 	"gphoto2.conf"

# define DEFAULT_TTY		"usb:"

static SANE_Bool is_open = 0;

static SANE_Bool gphoto2_opt_thumbnails;
static SANE_Bool gphoto2_opt_snap;
static SANE_Bool gphoto2_opt_lowres;
static SANE_Bool gphoto2_opt_erase;
static SANE_Bool gphoto2_opt_autoinc;
static SANE_Bool dumpinquiry;

static struct jpeg_decompress_struct cinfo;
static djpeg_dest_ptr dest_mgr = NULL;

static SANE_Int highres_height = 960, highres_width = 1280;
static SANE_Int lowres_height = 480, lowres_width = 640;
static SANE_Int thumb_height = 120, thumb_width = 160;
static SANE_String TopFolder;
static SANE_String Gphoto2Path = "gphoto2";

static GPHOTO2 Camera;

static SANE_Range image_range = {
  0,
  0,
  0
};

static SANE_String **folder_list;
static SANE_Int current_folder = 0;

static SANE_Option_Descriptor sod[] = {
  {
   SANE_NAME_NUM_OPTIONS,
   SANE_TITLE_NUM_OPTIONS,
   SANE_DESC_NUM_OPTIONS,
   SANE_TYPE_INT,
   SANE_UNIT_NONE,
   sizeof (SANE_Word),
   SANE_CAP_SOFT_DETECT,
   SANE_CONSTRAINT_NONE,
   {NULL}
   }
  ,

#define GPHOTO2_OPT_IMAGE_SELECTION 1
  {
   "",
   "Image Selection",
   "Selection of the image to load.",
   SANE_TYPE_GROUP,
   SANE_UNIT_NONE,
   0,
   0,
   SANE_CONSTRAINT_NONE,
   {NULL}
   }
  ,

#define GPHOTO2_OPT_FOLDER 2
  {
   "folder",
   "Folder",
   "Select folder within camera",
   SANE_TYPE_STRING,
   SANE_UNIT_NONE,
   256,
   SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT,
   SANE_CONSTRAINT_STRING_LIST,
   {NULL}
   }
  ,

#define GPHOTO2_OPT_IMAGE_NUMBER 3
  {
   "image",
   "Image Number",
   "Select Image Number to load from camera",
   SANE_TYPE_INT,
   SANE_UNIT_NONE,
   4,
   SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT,
   SANE_CONSTRAINT_RANGE,
   {(SANE_String_Const *) & image_range}	/* this is ANSI conformant! */
   }
  ,

#define GPHOTO2_OPT_THUMBS 4
  {
   "thumbs",
   "Load Thumbnail",
   "Load the image as thumbnail.",
   SANE_TYPE_BOOL,
   SANE_UNIT_NONE,
   sizeof (SANE_Word),
   SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT,
   SANE_CONSTRAINT_NONE,
   {NULL}
   }
  ,

#define GPHOTO2_OPT_SNAP 5
  {
   "snap",
   "Snap new picture",
   "Take new picture and download it",
   SANE_TYPE_BOOL,
   SANE_UNIT_NONE,
   sizeof (SANE_Word),
   SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT /* | SANE_CAP_ADVANCED */ ,
   SANE_CONSTRAINT_NONE,
   {NULL}
   }
  ,

#define GPHOTO2_OPT_LOWRES 6
  {
   "lowres",
   "Low Resolution",
   "Resolution of new picture or selected image (must be manually specified)",
   SANE_TYPE_BOOL,
   SANE_UNIT_NONE,
   sizeof (SANE_Word),
   SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT
   /* | SANE_CAP_ADVANCED */ ,
   SANE_CONSTRAINT_NONE,
   {NULL}
   }
  ,

#define GPHOTO2_OPT_ERASE 7
  {
   "erase",
   "Erase",
   "Erase the picture after downloading",
   SANE_TYPE_BOOL,
   SANE_UNIT_NONE,
   sizeof (SANE_Word),
   SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT,
   SANE_CONSTRAINT_NONE,
   {NULL}
   }
  ,

#define GPHOTO2_OPT_DEFAULT 8
  {
   "default-enhancements",
   "Defaults",
   "Set default values for enhancement controls.",
   SANE_TYPE_BUTTON,
   SANE_UNIT_NONE,
   0,
   SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT,
   SANE_CONSTRAINT_NONE,
   {NULL}
   }
  ,

#define GPHOTO2_OPT_INIT_GPHOTO2 9
  {
   "camera-init",
   "Re-establish Communications",
   "Re-establish communications with camera (in case of timeout, etc.)",
   SANE_TYPE_BUTTON,
   SANE_UNIT_NONE,
   0,
   SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT,
   SANE_CONSTRAINT_NONE,
   {NULL}
   }
  ,

#define GPHOTO2_OPT_AUTOINC 10
  {
   "autoinc",
   "Auto Increment",
   "Increment image number after each scan",
   SANE_TYPE_BOOL,
   SANE_UNIT_NONE,
   sizeof (SANE_Word),
   SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED,
   SANE_CONSTRAINT_NONE,
   {NULL}
   }
  ,


};

static SANE_Parameters parms = {
  SANE_FRAME_RGB,
  0,
  0,				/* Number of bytes returned per scan line: */
  0,				/* Number of pixels per scan line.  */
  0,				/* Number of lines for the current scan.  */
  8,				/* Number of bits per sample. */
};

/* Linked list of file names in the current folder */
static struct cam_dirlist *dir_head = NULL;

/* Buffer to hold line currently being processed by sane_read */
static SANE_Byte *linebuffer = NULL;
static SANE_Int linebuffer_size = 0;
static SANE_Int linebuffer_index = 0;

/* gphoto2 command currently being executed */
static SANE_Char cmdbuf[256];
/* The pipe corresponding to cmdbuf */
static FILE *cmdpipe;
/* String read from cmdpipe */
static SANE_Byte tmpstr[256];

static SANE_Byte name_buf[60];

#include <sys/time.h>
#include <unistd.h>

static SANE_Int
init_gphoto2 (void)
{
  SANE_Int entries, n;

  DBG (1, "GPHOTO2 Backend 05/16/01\n");

  sprintf ((char *) cmdbuf, "%s --camera \"%s\" --port %s --list-folders -q",
	   (char *) Gphoto2Path, (char *) Camera.camera_name,
	   (char *) Camera.tty_name);

  cmdpipe = popen ((char *) cmdbuf, "r");
  if (cmdpipe == NULL)
    {
      DBG (0, "%s: error: couldn't open gphoto2", "init_gphoto2");
      exit (1);
    }

  n = fscanf (cmdpipe, "%d", &entries);
  if (n == 0)
    {
      return -1;
    }
  fclose (cmdpipe);

  return SANE_STATUS_GOOD;
}

static void
close_gphoto2 (SANE_Int fd)
{
  /*
   *    Put the camera back to 9600 baud
   */

  if (close (fd) == -1)
    {
      DBG (1, "close_gphoto2: error: could not close device\n");
    }
}

int
get_info (void)
{

  SANE_Int n;
  struct cam_dirlist *e;

  if (Camera.pic_taken == 0)
    {
      sod[GPHOTO2_OPT_IMAGE_NUMBER].cap |= SANE_CAP_INACTIVE;
      image_range.min = 0;
      image_range.max = 0;
    }
  else
    {
      sod[GPHOTO2_OPT_IMAGE_NUMBER].cap &= ~SANE_CAP_INACTIVE;
      image_range.min = 1;
      image_range.max = Camera.pic_taken;
    }

  n = read_dir (TopFolder, 0);

  /* If we've already got a folder_list, free it up before starting
   * the new one 
   */
  if (folder_list != NULL)
    {
      int tmp;
      for (tmp = 0; folder_list[tmp]; tmp++)
	{
	  free (folder_list[tmp]);
	}
      free (folder_list);
    }

  folder_list = (SANE_String * *)malloc ((n + 1) * sizeof (SANE_String *));
  for (e = dir_head, n = 0; e; e = e->next, n++)
    {
      folder_list[n] = (SANE_String *) strdup (e->name);
      if (strchr ((char *) folder_list[n], ' '))
	{
	  *strchr ((char *) folder_list[n], ' ') = '\0';
	}
    }
  if (n == 0)
    {
      folder_list[n++] = (SANE_String *) strdup ("");
    }

  folder_list[n] = NULL;
  sod[GPHOTO2_OPT_FOLDER].constraint.string_list =
    (SANE_String_Const *) folder_list;

  Camera.pic_taken = 0;
  Camera.pic_left = 1;		/* Just a guess! */

  return 0;

}

static SANE_Int
erase (void)
{
  sprintf ((char *) cmdbuf,
	   "%s --camera \"%s\" --port %s --folder %s/%s  --stdout -d %d",
	   (char *) Gphoto2Path, (char *) Camera.camera_name,
	   (char *) Camera.tty_name, (char *) TopFolder,
	   (char *) folder_list[current_folder],
	   Camera.current_picture_number);
  cmdpipe = popen ((char *) cmdbuf, "r");
  if (cmdpipe == NULL)
    {
      return -1;
    }

  fclose (cmdpipe);

  return SANE_STATUS_GOOD;
}

static SANE_Int
change_res (SANE_Byte res)
{

  return (res - res);

}

SANE_Status
sane_init (SANE_Int * version_code, SANE_Auth_Callback UNUSEDARG authorize)
{

  SANE_Char f[] = "sane_init";
  SANE_Char dev_name[PATH_MAX], *p;
  SANE_Char buf[256];
  size_t len;
  FILE *fp;

  DBG_INIT ();

  if (version_code)
    *version_code = SANE_VERSION_CODE (V_MAJOR, V_MINOR, 0);

  fp = sanei_config_open (GPHOTO2_CONFIG_FILE);

  /* defaults */
  Camera.tty_name = DEFAULT_TTY;

  if (!fp)
    {
      /* default to /dev/whatever instead of insisting on config file */
      DBG (1, "warning: %s:  missing config file '%s'\n", f,
	   GPHOTO2_CONFIG_FILE);
    }
  else
    {
      while (sanei_config_read (dev_name, sizeof (dev_name), fp))
	{
	  dev_name[sizeof (dev_name) - 1] = '\0';
	  DBG (20, "%s:  config- %s\n", f, dev_name);

	  if (dev_name[0] == '#')
	    continue;		/* ignore line comments */
	  len = strlen (dev_name);
	  if (!len)
	    continue;		/* ignore empty lines */
	  if (strncmp (dev_name, "port=", 5) == 0)
	    {
	      SANE_Int n, entries;

	      p = dev_name + 5;
	      if (p)
		Camera.tty_name = strdup (p);
	      DBG (20, "Config file port=%s\n", Camera.tty_name);
	      sprintf ((char *) cmdbuf, "%s -q --list-ports",
		       (char *) Gphoto2Path);
	      cmdpipe = popen ((char *) cmdbuf, "r");
	      if (cmdpipe == NULL)
		{
		  return -1;
		}
	      fgets ((char *) tmpstr, sizeof (tmpstr), cmdpipe);
	      n = sscanf ((char *) tmpstr, "%d", &entries);
	      if (n == 0 || entries < 1)
		{
		  DBG (0, "%s: error: Can't list available ports\n", f);
		  exit (1);
		}
	      for (n = 0; n < entries; n++)
		{
		  fgets ((char *) tmpstr, sizeof (tmpstr), cmdpipe);
		  if (strchr (tmpstr, ' '))
		    {
		      *strchr (tmpstr, ' ') = '\0';
		    }
		  if (strcmp (Camera.tty_name, tmpstr) == 0)
		    {
		      break;
		    }
		}
	      if (n == entries)
		{
		  DBG (0,
		       "%s: error: %s is not a valid gphoto2 port.  Use \"gphoto2 --list-ports\" for list.\n",
		       f, Camera.tty_name);
		  exit (1);
		}
	      fclose (cmdpipe);
	    }
	  else if (strncmp (dev_name, "camera=", 7) == 0)
	    {
	      SANE_Int n, entries;
	      Camera.camera_name = strdup (dev_name + 7);
	      DBG (20, "Config file camera=%s\n", Camera.camera_name);
	      sprintf (buf, "Image selection - %s", Camera.camera_name);

	      sprintf ((char *) cmdbuf, "%s -q --list-cameras",
		       (char *) Gphoto2Path);
	      cmdpipe = popen ((char *) cmdbuf, "r");
	      if (cmdpipe == NULL)
		{
		  return -1;
		}
	      fgets ((char *) tmpstr, sizeof (tmpstr), cmdpipe);
	      n = sscanf ((char *) tmpstr, "%d", &entries);
	      if (n == 0 || entries < 1)
		{
		  DBG (0, "%s: error: Can't list available cameras\n", f);
		  return SANE_STATUS_INVAL;
		}
	      for (n = 0; n < entries; n++)
		{
		  fgets ((char *) tmpstr, sizeof (tmpstr), cmdpipe);
		  if (strchr (tmpstr, '\n'))
		    {
		      *strchr (tmpstr, '\n') = '\0';
		    }
		  if (strcmp (Camera.camera_name, tmpstr) == 0)
		    {
		      break;
		    }
		}
	      if (n == entries)
		{
		  DBG (0,
		       "%s: error: %s is not a valid camera type.  Use \"gphoto2 --list-cameras\" for list.\n",
		       f, Camera.camera_name);
		  return SANE_STATUS_INVAL;
		}

	      fclose (cmdpipe);

	      sod[GPHOTO2_OPT_IMAGE_SELECTION].title = strdup (buf);
	    }
	  else if (strcmp (dev_name, "dumpinquiry") == 0)
	    {
	      dumpinquiry = SANE_TRUE;
	    }
	  else if (strncmp (dev_name, "high_resolution=", 16) == 0)
	    {
	      sscanf (&dev_name[16], "%dx%d", &highres_width,
		      &highres_height);
	      DBG (20, "Config file high_resolution=%ux%u\n", highres_width,
		   highres_height);
	    }
	  else if (strncmp (dev_name, "low_resolution=", 15) == 0)
	    {
	      sscanf (&dev_name[15], "%dx%d", &lowres_width, &lowres_height);
	      DBG (20, "Config file low_resolution=%ux%u\n", lowres_width,
		   lowres_height);
	    }
	  else if (strncmp (dev_name, "thumb_resolution=", 17) == 0)
	    {
	      sscanf (&dev_name[17], "%dx%d", &thumb_width, &thumb_height);
	      DBG (20, "Config file thumb_resolution=%ux%u\n", thumb_width,
		   thumb_height);
	    }
	  else if (strncmp (dev_name, "topfolder=", 10) == 0)
	    {
	      TopFolder = strdup (&dev_name[10]);
	      DBG (20, "Config file topfolder=%s\n", TopFolder);
	    }
	  else if (strncmp (dev_name, "gphoto2path=", 12) == 0)
	    {
	      Gphoto2Path = strdup (&dev_name[12]);
	      DBG (20, "Config file gphoto2path=%s\n", Gphoto2Path);
	    }
	}
      fclose (fp);
    }

  if (init_gphoto2 () == -1)
    return SANE_STATUS_INVAL;

  if (get_info () == -1)
    {
      DBG (1, "error: could not get info\n");
      close_gphoto2 (Camera.fd);
      return SANE_STATUS_INVAL;
    }

  /* load the current images array */
  get_pictures_info ();

  if (Camera.pic_taken == 0)
    {
      Camera.current_picture_number = 0;
      parms.bytes_per_line = 0;
      parms.pixels_per_line = 0;
      parms.lines = 0;
    }
  else
    {
      Camera.current_picture_number = 1;
/* OLD:
      set_res (Camera.Pictures[Camera.current_picture_number - 1].low_res);
*/
	set_res( gphoto2_opt_lowres );  
  }

  if (dumpinquiry)
    {
      DBG (0, "\nCamera information:\n~~~~~~~~~~~~~~~~~\n\n");
      DBG (0, "Model...........: DC%s\n", "240");
      DBG (0, "Firmware version: %d.%d\n", Camera.ver_major,
	   Camera.ver_minor);
      DBG (0, "Pictures........: %d/%d\n", Camera.pic_taken,
	   Camera.pic_taken + Camera.pic_left);
      DBG (0, "Battery state...: %s\n",
	   Camera.flags.low_batt == 0 ? "good" : (Camera.flags.low_batt ==
						  1 ? "weak" : "empty"));
    }

  return SANE_STATUS_GOOD;
}

void
sane_exit (void)
{
}

/* Device select/open/close */

static const SANE_Device dev[] = {
  {
   "0",
   "Kodak",
   "DC-240",
   "still camera"},
};

static const SANE_Device *devlist[] = {
  dev + 0, 0
};

SANE_Status
sane_get_devices (const SANE_Device *** device_list, SANE_Bool
		  UNUSEDARG local_only)
{

  DBG (127, "sane_get_devices called\n");

  *device_list = devlist;
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_open (SANE_String_Const devicename, SANE_Handle * handle)
{
  SANE_Int i;

  DBG (127, "sane_open for device %s\n", devicename);
  if (!devicename[0])
    {
      i = 0;
    }
  else
    {
      for (i = 0; i < NELEMS (dev); ++i)
	{
	  if (strcmp (devicename, dev[i].name) == 0)
	    {
	      break;
	    }
	}
    }

  if (i >= NELEMS (dev))
    {
      return SANE_STATUS_INVAL;
    }

  if (is_open)
    {
      return SANE_STATUS_DEVICE_BUSY;
    }

  is_open = 1;
  *handle = MAGIC;

  DBG (4, "sane_open: pictures taken=%d\n", Camera.pic_taken);

  return SANE_STATUS_GOOD;
}

void
sane_close (SANE_Handle handle)
{
  DBG (127, "sane_close called\n");
  if (handle == MAGIC)
    is_open = 0;

  DBG (127, "sane_close returning\n");
}

const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
  if (handle != MAGIC || !is_open)
    return NULL;		/* wrong device */
  if (option < 0 || option >= NELEMS (sod))
    return NULL;
  return &sod[option];
}

static SANE_Int myinfo = 0;

SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option,
		     SANE_Action action, void *value, SANE_Int * info)
{
  SANE_Status status;

  DBG (127, "control_option(handle=%p,opt=%s,act=%s,val=%p,info=%p)\n",
       handle, sod[option].title,
       (action ==
	SANE_ACTION_SET_VALUE ? "SET" : (action ==
					 SANE_ACTION_GET_VALUE ? "GET" :
					 "SETAUTO")), value, info);

  if (handle != MAGIC || !is_open)
    return SANE_STATUS_INVAL;	/* Unknown handle ... */

  if (option < 0 || option >= NELEMS (sod))
    return SANE_STATUS_INVAL;	/* Unknown option ... */

  switch (action)
    {
    case SANE_ACTION_SET_VALUE:
      status = sanei_constrain_value (sod + option, value, &myinfo);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (2, "Constraint error in control_option\n");
	  return status;
	}

      switch (option)
	{
	case GPHOTO2_OPT_IMAGE_NUMBER:
	  Camera.current_picture_number = *(SANE_Word *) value;
	  myinfo |= SANE_INFO_RELOAD_PARAMS;

	  /* get the image's resolution, unless the camera has no 
	   * pictures yet 
	   */
	  if (Camera.pic_taken != 0)
	    {
/* OLD:
	      set_res (Camera.
		       Pictures[Camera.current_picture_number - 1].low_res);
*/
	      set_res (gphoto2_opt_lowres); 
	    }
	  break;

	case GPHOTO2_OPT_THUMBS:
	  gphoto2_opt_thumbnails = !!*(SANE_Word *) value;
	  myinfo |= SANE_INFO_RELOAD_PARAMS;

	  if (Camera.pic_taken != 0)
	    {
/* OLD:
	      set_res (Camera.
		       Pictures[Camera.current_picture_number - 1].low_res);
*/
	      set_res (gphoto2_opt_lowres); 
	    }
	  break;

	case GPHOTO2_OPT_SNAP:
	  gphoto2_opt_snap = !!*(SANE_Word *) value;
	  myinfo |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
	  /* if we are snapping a new one */
	  if (gphoto2_opt_snap)
	    {
	      /* activate the resolution setting */
	      sod[GPHOTO2_OPT_LOWRES].cap &= ~SANE_CAP_INACTIVE;
	      /* and de-activate the image number selector */
	      sod[GPHOTO2_OPT_IMAGE_NUMBER].cap |= SANE_CAP_INACTIVE;
	    }
	  else
	    {
	      /* deactivate the resolution setting */
	      /*  sod [GPHOTO2_OPT_LOWRES].cap |= SANE_CAP_INACTIVE; */
	      /* and activate the image number selector, if there are 
	       * pictures available */
	      if (Camera.current_picture_number)
		{
		  sod[GPHOTO2_OPT_IMAGE_NUMBER].cap &= ~SANE_CAP_INACTIVE;
		}
	    }
	  /* set params according to resolution settings */
	  set_res (gphoto2_opt_lowres);

	  break;

	case GPHOTO2_OPT_LOWRES:
	  gphoto2_opt_lowres = !!*(SANE_Word *) value;
	  myinfo |= SANE_INFO_RELOAD_PARAMS;

/* XXX - change the number of pictures left depending on resolution
   perhaps just call get_info again?
*/
	  set_res (gphoto2_opt_lowres);

	  break;

	case GPHOTO2_OPT_ERASE:
	  gphoto2_opt_erase = !!*(SANE_Word *) value;
	  break;

	case GPHOTO2_OPT_AUTOINC:
	  gphoto2_opt_autoinc = !!*(SANE_Word *) value;
	  break;

	case GPHOTO2_OPT_FOLDER:
	  printf ("FIXME set folder not implemented yet\n");
	  break;

	case GPHOTO2_OPT_DEFAULT:
	  gphoto2_opt_thumbnails = 0;
	  gphoto2_opt_snap = 0;

	  /* deactivate the resolution setting */
/*	  sod[GPHOTO2_OPT_LOWRES].cap |= SANE_CAP_INACTIVE; */
	  /* and activate the image number selector */
	  sod[GPHOTO2_OPT_IMAGE_NUMBER].cap &= ~SANE_CAP_INACTIVE;

	  myinfo |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;

	  DBG (1, "Fixme: Set all defaults here!\n");
	  break;

	case GPHOTO2_OPT_INIT_GPHOTO2:
	  if ((Camera.fd = init_gphoto2 ()) == -1)
	    {
	      return SANE_STATUS_INVAL;
	    }
	  if (get_info () == -1)
	    {
	      DBG (1, "error: could not get info\n");
	      close_gphoto2 (Camera.fd);
	      return SANE_STATUS_INVAL;
	    }

	  /* load the current images array */
	  get_pictures_info ();

	  myinfo |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;
	  break;

	default:
	  return SANE_STATUS_INVAL;
	}
      break;

    case SANE_ACTION_GET_VALUE:
      switch (option)
	{
	case 0:
	  *(SANE_Word *) value = NELEMS (sod);
	  break;

	case GPHOTO2_OPT_IMAGE_NUMBER:
	  *(SANE_Word *) value = Camera.current_picture_number;
	  break;

	case GPHOTO2_OPT_THUMBS:
	  *(SANE_Word *) value = gphoto2_opt_thumbnails;
	  break;

	case GPHOTO2_OPT_SNAP:
	  *(SANE_Word *) value = gphoto2_opt_snap;
	  break;

	case GPHOTO2_OPT_LOWRES:
	  *(SANE_Word *) value = gphoto2_opt_lowres;
	  break;

	case GPHOTO2_OPT_ERASE:
	  *(SANE_Word *) value = gphoto2_opt_erase;
	  break;

	case GPHOTO2_OPT_AUTOINC:
	  *(SANE_Word *) value = gphoto2_opt_autoinc;
	  break;

	case GPHOTO2_OPT_FOLDER:
	  if (folder_list == NULL)
	    {
	      return SANE_STATUS_INVAL;
	    }
	  strncpy ((char *) value, (char *) folder_list[current_folder], 256);
	  break;


	default:
	  return SANE_STATUS_INVAL;
	}
      break;

    case SANE_ACTION_SET_AUTO:
      switch (option)
	{
	default:
	  return SANE_STATUS_UNSUPPORTED;	/* We are DUMB */
	}
    }

  if (info)
    {
      *info = myinfo;
      myinfo = 0;
    }
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters * params)
{
  SANE_Int rc = SANE_STATUS_GOOD;

  DBG (127, "sane_get_params called, wid=%d,height=%d\n",
       parms.pixels_per_line, parms.lines);

  if (handle != MAGIC || !is_open)
    rc = SANE_STATUS_INVAL;	/* Unknown handle ... */

  parms.last_frame = SANE_TRUE;	/* Have no idea what this does */
  *params = parms;
  DBG (127, "sane_get_params return %d\n", rc);
  return rc;
}

typedef struct
{
  struct jpeg_source_mgr pub;
  JOCTET *buffer;
}
my_source_mgr;
typedef my_source_mgr *my_src_ptr;

METHODDEF (void)
jpeg_init_source (j_decompress_ptr UNUSEDARG cinfo)
{
  /* nothing to do */
}

METHODDEF (boolean) jpeg_fill_input_buffer (j_decompress_ptr cinfo)
{
  int n;

  my_src_ptr src = (my_src_ptr) cinfo->src;

  if ((n = fread (src->buffer, 1, 512, cmdpipe)) == -1)
    {
      DBG (5, "sane_start: read_data failed\n");
      src->buffer[0] = (JOCTET) 0xFF;
      src->buffer[1] = (JOCTET) JPEG_EOI;
      return FALSE;
    }
  src->pub.next_input_byte = src->buffer;
  src->pub.bytes_in_buffer = n;

  return TRUE;
}

METHODDEF (void) jpeg_skip_input_data (j_decompress_ptr cinfo, long num_bytes)
{

  my_src_ptr src = (my_src_ptr) cinfo->src;

  if (num_bytes > 0)
    {
      while (num_bytes > (long) src->pub.bytes_in_buffer)
	{
	  num_bytes -= (long) src->pub.bytes_in_buffer;
	  (void) jpeg_fill_input_buffer (cinfo);
	}
    }
  src->pub.next_input_byte += (size_t) num_bytes;
  src->pub.bytes_in_buffer -= (size_t) num_bytes;
}

METHODDEF (void)
jpeg_term_source (j_decompress_ptr UNUSEDARG cinfo)
{
  /* no work necessary here */
}

SANE_Status
sane_start (SANE_Handle handle)
{
  my_src_ptr src;

  struct jpeg_error_mgr jerr;
  SANE_Int row_stride;

  DBG (127, "sane_start called\n");
  if (handle != MAGIC || !is_open ||
      (Camera.current_picture_number == 0 && gphoto2_opt_snap == SANE_FALSE))
    return SANE_STATUS_INVAL;	/* Unknown handle ... */

  if (Camera.scanning)
    return SANE_STATUS_EOF;

/*
 * This shouldn't normally happen, but we allow it as a special case
 * when batch/autoinc are in effect.  The first illegal picture number
 * terminates the scan
 */
  if (Camera.current_picture_number > Camera.pic_taken)
    {
      return SANE_STATUS_INVAL;
    }

  if (gphoto2_opt_snap)
    {
      /*
       * Don't allow picture unless there is room in the 
       * camera.
       */
      if (Camera.pic_left == 0)
	{
	  DBG (3, "No room to store new picture\n");
	  return SANE_STATUS_INVAL;
	}


      if (snap_pic () == SANE_STATUS_INVAL)
	{
	  DBG (1, "Failed to snap new picture\n");
	  return SANE_STATUS_INVAL;
	}
    }

  sprintf ((char *) cmdbuf,
	   "%s --camera \"%s\" --port %s --folder %s/%s  --stdout -%s %d",
	   (char *) Gphoto2Path, (char *) Camera.camera_name,
	   (char *) Camera.tty_name, (char *) TopFolder,
	   (char *) folder_list[current_folder],
	   gphoto2_opt_thumbnails ? "t" : "p", Camera.current_picture_number);
  cmdpipe = popen ((char *) cmdbuf, "r");

  if (cmdpipe == NULL)
    {
      return SANE_STATUS_INVAL;
    }
  cinfo.err = jpeg_std_error (&jerr);
  jpeg_create_decompress (&cinfo);

  cinfo.src =
    (struct jpeg_source_mgr *) (*cinfo.mem->
				alloc_small) ((j_common_ptr) & cinfo,
					      JPOOL_PERMANENT,
					      sizeof (my_source_mgr));
  src = (my_src_ptr) cinfo.src;

  src->buffer = (JOCTET *) (*cinfo.mem->alloc_small) ((j_common_ptr) &
						      cinfo,
						      JPOOL_PERMANENT,
						      1024 * sizeof (JOCTET));
  src->pub.init_source = jpeg_init_source;
  src->pub.fill_input_buffer = jpeg_fill_input_buffer;
  src->pub.skip_input_data = jpeg_skip_input_data;
  src->pub.resync_to_restart = jpeg_resync_to_restart;	/* default */
  src->pub.term_source = jpeg_term_source;
  src->pub.bytes_in_buffer = 0;
  src->pub.next_input_byte = NULL;

  (void) jpeg_read_header (&cinfo, TRUE);
  dest_mgr = sanei_jpeg_jinit_write_ppm (&cinfo);
  (void) jpeg_start_decompress (&cinfo);
  row_stride = cinfo.output_width * cinfo.output_components;


  /* Check if a linebuffer has been allocated.  Assumes that highres_width
   * is also large enough to hold a lowres or thumbnail image 
   */
  if (linebuffer == NULL)
    {
      linebuffer = malloc (highres_width * 3);
    }
  if (linebuffer == NULL)
    {
      return SANE_STATUS_INVAL;
    }

  Camera.scanning = SANE_TRUE;	/* don't overlap scan requests */

  return SANE_STATUS_GOOD;
}

SANE_Status
sane_read (SANE_Handle UNUSEDARG handle, SANE_Byte * data,
	   SANE_Int max_length, SANE_Int * length)
{
  SANE_Int lines = 0;

  /* If there is anything in the buffer, satisfy the read from there */
  if (linebuffer_size && linebuffer_index < linebuffer_size)
    {
      *length = linebuffer_size - linebuffer_index;

      if (*length > max_length)
	{
	  *length = max_length;
	}
      memcpy (data, linebuffer + linebuffer_index, *length);
      linebuffer_index += *length;

      return SANE_STATUS_GOOD;
    }

  if (cinfo.output_scanline >= cinfo.output_height)
    {
      fclose (cmdpipe);

      if (gphoto2_opt_erase)
	{
	  DBG (127, "sane_read bp%d, erase image\n", __LINE__);
	  if (erase () == -1)
	    {
	      DBG (1, "Failed to erase memory\n");
	      return SANE_STATUS_INVAL;
	    }
	  Camera.pic_taken--;
	  Camera.pic_left++;
	  Camera.current_picture_number = Camera.pic_taken;
	  image_range.max--;

	  myinfo |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;
	  dir_delete ((SANE_String) & name_buf[1]);

	}
      if (gphoto2_opt_autoinc)
	{
	  if (Camera.current_picture_number <= Camera.pic_taken)
	    {
	      Camera.current_picture_number++;

	      myinfo |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;

	      /* get the image's resolution */
/* OLD:
	      set_res (Camera.Pictures[Camera.current_picture_number - 1].
		       low_res);
*/
	      set_res (gphoto2_opt_lowres); 
	    }
	  DBG (4, "Increment count to %d (total %d)\n",
	       Camera.current_picture_number, Camera.pic_taken);
	}
      return SANE_STATUS_EOF;
    }

/* XXX - we should read more than 1 line at a time here */
  lines = 1;
  (void) jpeg_read_scanlines (&cinfo, dest_mgr->buffer, lines);
  (*dest_mgr->put_pixel_rows) (&cinfo, dest_mgr, lines, (char *) linebuffer);

  *length = cinfo.output_width * cinfo.output_components * lines;
  linebuffer_size = *length;
  linebuffer_index = 0;

  if (*length > max_length)
    {
      *length = max_length;
    }
  memcpy (data, linebuffer + linebuffer_index, *length);
  linebuffer_index += *length;

  return SANE_STATUS_GOOD;
}

void
sane_cancel (SANE_Handle UNUSEDARG handle)
{
  if (Camera.scanning)
    {
      Camera.scanning = SANE_FALSE;	/* done with scan */
    }
  else
    DBG (4, "sane_cancel: not scanning - nothing to do\n");
}

SANE_Status
sane_set_io_mode (SANE_Handle UNUSEDARG handle, SANE_Bool
		  UNUSEDARG non_blocking)
{
  return SANE_STATUS_UNSUPPORTED;
}

SANE_Status
sane_get_select_fd (SANE_Handle UNUSEDARG handle, SANE_Int * UNUSEDARG fd)
{
  return SANE_STATUS_UNSUPPORTED;
}

/*
 * get_pictures_info - load information about all pictures currently in
 *			camera:  Mainly the mapping of picture number
 *			to picture name, and the resolution of each picture.
 */
static PictureInfo *
get_pictures_info (void)
{
  SANE_Char f[] = "get_pictures_info";
  SANE_Char path[256];
  SANE_Int num_pictures;
  SANE_Int p;
  PictureInfo *pics;

  if (Camera.Pictures)
    {
      free (Camera.Pictures);
      Camera.Pictures = NULL;
    }

  strcpy (path, TopFolder);
  if (folder_list[current_folder] != NULL)
    {
      strcat (path, "/");
      strcat (path, (char *) folder_list[current_folder]);
    }

  num_pictures = read_dir (path, 1);
  Camera.pic_taken = num_pictures;
  if (num_pictures > 0)
    {
      sod[GPHOTO2_OPT_IMAGE_NUMBER].cap &= ~SANE_CAP_INACTIVE;
      image_range.min = 1;
      image_range.max = num_pictures;
    }

  if ((pics = (PictureInfo *) malloc (Camera.pic_taken *
				      sizeof (PictureInfo))) == NULL)
    {
      DBG (1, "%s: error: allocate memory for pictures array\n", f);
      return NULL;
    }

  for (p = 0; p < Camera.pic_taken; p++)
    {
      if (get_picture_info (pics + p, p) == -1)
	{
	  free (pics);
	  return NULL;
	}
    }

  Camera.Pictures = pics;
  return pics;
}

static SANE_Int
get_picture_info (PictureInfo * pic, SANE_Int p)
{

  SANE_Char f[] = "get_picture_info";
  SANE_Int n;
  struct cam_dirlist *e;

  DBG (4, "%s: info for pic #%d\n", f, p);

  for (n = 0, e = dir_head; e && n < p; n++, e = e->next)
    ;

  DBG (4, "Name is %s\n", e->name);

  read_info (e->name);

  pic->low_res = SANE_FALSE;

  return 0;
}

/*
 * snap_pic - take a picture (and call get_pictures_info to re-create
 *		the directory related data structures)
 */
static SANE_Status
snap_pic (void)
{
  SANE_Bool found = SANE_FALSE;
  SANE_Char f[] = "snap_pic";
  SANE_Int linecount = 0;

  /* make sure camera is set to our settings state */
  if (change_res (gphoto2_opt_lowres) == -1)
    {
      DBG (1, "%s: Failed to set resolution\n", f);
      return SANE_STATUS_INVAL;
    }

  sprintf ((char *) cmdbuf,
	   "%s --camera \"%s\" --port %s --folder %s/%s -q  --capture-image",
	   (char *) Gphoto2Path, (char *) Camera.camera_name,
	   (char *) Camera.tty_name, (char *) TopFolder,
	   (char *) folder_list[current_folder]);

  cmdpipe = popen ((char *) cmdbuf, "r");

  if (cmdpipe == NULL)
    {
      return SANE_STATUS_INVAL;
    }

  while (fgets ((char *) tmpstr, sizeof (tmpstr), cmdpipe)
	 && linecount++ < 100)
    {
      if (strncmp (tmpstr, "/DCIM/", 6) == 0)
	{
	  found = SANE_TRUE;
	}
    }
  fclose (cmdpipe);

  if (!found)
    {
      return SANE_STATUS_INVAL;
    }

  /* Can't just increment picture count, because if the camera has
   * zero pictures we may not know the folder name.  Start over
   * with get_info and get_pictures_info
   */
  if (get_info () == -1)
    {
      DBG (1, "error: could not get info\n");
      close_gphoto2 (Camera.fd);
      return SANE_STATUS_INVAL;
    }

  if (get_pictures_info () == NULL)
    {
      DBG (1, "%s: Failed to get new picture info\n", f);
      /* XXX - I guess we should try to erase the image here */
      return SANE_STATUS_INVAL;
    }


  sod[GPHOTO2_OPT_IMAGE_NUMBER].cap |= SANE_CAP_INACTIVE;
  Camera.current_picture_number = Camera.pic_taken;
  myinfo |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;

  return SANE_STATUS_GOOD;
}

/*
 * read_dir - read a list of file names from the specified directory
 * 		and create a linked list of file name entries in
 *		alphabetical order.  The first entry in the list will
 *		be "picture #1", etc.
 */
static SANE_Int
read_dir (SANE_String dir, SANE_Bool read_files)
{
  SANE_Int retval = 0;
  SANE_Int i, entries;
  SANE_Char f[] = "read_dir";
  struct cam_dirlist *e, *next;

  /* Free up current list */
  for (e = dir_head; e; e = next)
    {
      DBG (127, "%s: free entry %s\n", f, e->name);
      next = e->next;
      free (e);
    }
  dir_head = NULL;

  if (read_files)
    {
      sprintf ((char *) cmdbuf,
	       "%s --camera \"%s\" --port %s --folder %s --list-files --stdout",
	       (char *) Gphoto2Path, (char *) Camera.camera_name,
	       (char *) Camera.tty_name, dir);
    }
  else
    {
      sprintf ((char *) cmdbuf,
	       "%s --camera \"%s\" --port %s --folder %s --list-folders -q",
	       (char *) Gphoto2Path, (char *) Camera.camera_name,
	       (char *) Camera.tty_name, dir);
    }

  cmdpipe = popen ((char *) cmdbuf, "r");
  if (cmdpipe == NULL)
    {
      DBG (0, "%s: error: couldn't open gphoto2", f);
      exit (1);
    }

  fgets ((char *) tmpstr, sizeof (tmpstr), cmdpipe);
  sscanf ((char *) tmpstr, "%d", &entries);

  for (i = 0; i < entries; i++)

    {
      fgets ((char *) tmpstr, sizeof (tmpstr), cmdpipe);
      *strrchr ((char *) tmpstr, '\"') = '\0';

      if (dir_insert ((char *) (tmpstr + 1)))
	{
	  DBG (1, "%s: error: failed to insert dir entry\n");
	  return -1;
	}
      retval++;
    }
  fclose (cmdpipe);

  return retval;
}

/*
 * read_info - read the info block from camera for the specified file
 */
static SANE_Int
read_info (SANE_String fname)
{
  SANE_Char path[256];

  strcpy (path, "\\PCCARD\\DCIM\\");
  strcat (path, (char *) folder_list[current_folder]);
  strcat (path, "\\");
  strcat (path, fname);



  return 0;
}

/*
 * dir_insert - Add (in alphabetical order) a directory entry to the
 * 		current list of entries.
 */
static SANE_Int
dir_insert (SANE_String s)
{
  struct cam_dirlist *cur, *e;

  cur = (struct cam_dirlist *) malloc (sizeof (struct cam_dirlist));
  if (cur == NULL)
    {
      DBG (1, "dir_insert: error: could not malloc entry\n");
      return -1;
    }

  strcpy (cur->name, s);
  DBG (127, "dir_insert: name is %s\n", cur->name);

  cur->next = NULL;

  if (dir_head == NULL)
    {
      dir_head = cur;
    }
  else if (strcmp (cur->name, dir_head->name) < 0)
    {
      cur->next = dir_head;
      dir_head = cur;
      return 0;
    }
  else
    {
      for (e = dir_head; e->next; e = e->next)
	{
	  if (strcmp (e->next->name, cur->name) > 0)
	    {
	      cur->next = e->next;
	      e->next = cur;
	      return 0;
	    }
	}
      e->next = cur;
    }
  return 0;
}

/*
 *  dir_delete - Delete a directory entry from the linked list of file 
 * 		names
 */
static SANE_Int
dir_delete (SANE_String fname)
{
  struct cam_dirlist *cur, *e;

  DBG (127, "dir_delete:  %s\n", fname);

  if (strcmp (fname, dir_head->name) == 0)
    {
      cur = dir_head;
      dir_head = dir_head->next;
      free (cur);
      return 0;
    }

  for (e = dir_head; e->next; e = e->next)
    {

      if (strcmp (fname, e->next->name) == 0)
	{
	  cur = e->next;
	  e->next = e->next->next;
	  free (cur);
	  return (0);
	}
    }
  DBG (1, "dir_delete: Couldn't find entry %s in dir list\n", fname);
  return -1;
}

/*
 *  set_res - set picture size depending on resolution settings 
 */
static void
set_res (SANE_Int lowres)
{
  if (gphoto2_opt_thumbnails)
    {
      parms.bytes_per_line = THUMB_WIDTH * 3;
      parms.pixels_per_line = THUMB_WIDTH;
      parms.lines = THUMB_HEIGHT;
    }
  else if (lowres)
    {
      parms.bytes_per_line = LOWRES_WIDTH * 3;
      parms.pixels_per_line = LOWRES_WIDTH;
      parms.lines = LOWRES_HEIGHT;
    }
  else
    {
      parms.bytes_per_line = HIGHRES_WIDTH * 3;
      parms.pixels_per_line = HIGHRES_WIDTH;
      parms.lines = HIGHRES_HEIGHT;
    }
}
