/* Please note!  This is extremely alpha code, and is really intended as
 * a "proof of concept" since I don't yet know whether it's going to 
 * to be practical and/or possible to implement a the complete backend.
 * It's also been tested with only  one camera model, the Kodak DC240,
 * which happens to be the only camera I  have.  I'm very interested 
 * in learning what it would take to support more cameras.  In 
 * particular, the current incarnation will only support cameras
 * that directly generate jpeg files.
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

   This file implements a SANE backend for digital cameras 
   supported by the gphoto2 libraries.
 
   THIS IS EXTREMELY ALPHA CODE!  USE AT YOUR OWN RISK!! 

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

#include <gphoto2-core.h>
#include <gphoto2-camera.h>
#include <gphoto2-debug.h>

#define CHECK_EXIT(f) {int res = f; if (res < 0) {DBG (0,"ERROR: %s\n", gp_result_as_string (res)); exit (1);}}
#define CHECK_RET(f) {int res = f; if (res < 0) {DBG (0,"ERROR: %s\n", gp_result_as_string (res)); return (SANE_STATUS_INVAL);}}

#ifndef PATH_MAX
# define PATH_MAX	1024
#endif

#define MAGIC			(void *)0xab730324
#define GPHOTO2_CONFIG_FILE 	"gphoto2.conf"

static SANE_Bool is_open = 0;

/* Options selected by frontend: */
static SANE_Bool gphoto2_opt_thumbnails;	/* Read thumbnails */
static SANE_Bool gphoto2_opt_snap;	/* Take new picture */
static SANE_Bool gphoto2_opt_lowres;	/* Set low resolution */
static SANE_Bool gphoto2_opt_erase;	/* Erase after downloading */
static SANE_Bool gphoto2_opt_autoinc;	/* Increment image number */
static SANE_Bool dumpinquiry;	/* Dump status info */

/* Used for jpeg decompression */
static struct jpeg_decompress_struct cinfo;
static djpeg_dest_ptr dest_mgr = NULL;

static SANE_Int highres_height = 960, highres_width = 1280;
static SANE_Int thumb_height = 120, thumb_width = 160;
static SANE_String TopFolder;	/* Fixed part of path strings */

static GPHOTO2 Cam_data;	/* Other camera data */

static SANE_Range image_range = {
  0,
  0,
  0
};

static SANE_String_Const *folder_list;
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
   SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_INACTIVE	/* Until we figure out how to support it */
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


CameraList *dir_list;
Camera *camera;

/* Buffer to hold line currently being processed by sane_read */
static SANE_Byte *linebuffer = NULL;
static SANE_Int linebuffer_size = 0;
static SANE_Int linebuffer_index = 0;

/* used for setting up commands */
static SANE_Char cmdbuf[256];

/* Structures used by gphoto2 API */
static CameraAbilities *abilities;
static CameraFile *data_file;
static const char *data_ptr;
static long data_file_total_size, data_file_current_index;

#include <sys/time.h>
#include <unistd.h>

/*
 * init_gphoto2() - Initialize interface to camera using gphoto2 API
 */
static SANE_Int
init_gphoto2 (void)
{
  SANE_Int n;
  CameraList *list;

  DBG (1, "GPHOTO2 Backend 05/16/01\n");

  CHECK_RET (gp_camera_new (&camera));

  if (!Cam_data.camera_name)
    {
      DBG (0, "Camera name not specified in config file\n");
      exit (1);
    }
  CHECK_RET (gp_camera_set_model (camera, (char *) Cam_data.camera_name));

  if (!Cam_data.port)
    {
      DBG (0, "Camera port not specified in config file\n");
      exit (1);
    }
  CHECK_RET (gp_camera_set_port_path (camera, (char *) Cam_data.port));
  if (Cam_data.speed)
    {
      CHECK_RET (gp_camera_set_port_speed (camera, Cam_data.speed));
    }
  CHECK_RET (gp_camera_init (camera));
  CHECK_RET (gp_list_new (&list));

  CHECK_RET (gp_abilities_new (&abilities));
  CHECK_RET (gp_camera_abilities_by_name (Cam_data.camera_name, abilities));

  if (!(abilities->operations & GP_OPERATION_CAPTURE_IMAGE))
    {
      DBG (0, "Camera does not support image capture\n");
      return SANE_STATUS_INVAL;
    }

  for (n = 0; abilities->speed[n]; n++)
    {
      if (abilities->speed[n] == Cam_data.speed)
	{
	  break;
	}
      if (abilities->speed[n] == 0)
	{
	  DBG (0,
	       "%s: error: %d is not a valid speed for this camers.  Use \"gphoto2 --camera \"%s\" --abilities\" for list.\n",
	       "init_gphoto2", Cam_data.camera_name, Cam_data.speed);
	  return SANE_STATUS_INVAL;
	}
    }

  CHECK_RET (gp_camera_folder_list_folders (camera, "/PCCARD/DCIM", list));
  n = gp_list_count (list);
  if (n < 0)
    {
      DBG (0, "Unable to get file list\n");
      return SANE_STATUS_INVAL;
    }


  return SANE_STATUS_GOOD;
}

/*
 * close_gphoto2() - Shutdown camera interface 
 */
static void
close_gphoto2 (void)
{
  /*
   *    Put the camera back to 9600 baud
   */

  if (gp_camera_unref (camera))
    {
      DBG (1, "close_gphoto2: error: could not close device\n");
    }
}

/*
 * get_info() - Get overall information about camera: folder names,
 *	number of pictures, etc.
 */
SANE_Int
get_info (void)
{
  SANE_String_Const val;
  SANE_Int n;

  if (Cam_data.pic_taken == 0)
    {
      sod[GPHOTO2_OPT_IMAGE_NUMBER].cap |= SANE_CAP_INACTIVE;
      image_range.min = 0;
      image_range.max = 0;
    }
  else
    {
      sod[GPHOTO2_OPT_IMAGE_NUMBER].cap &= ~SANE_CAP_INACTIVE;
      image_range.min = 1;
      image_range.max = Cam_data.pic_taken;
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

  folder_list =
    (SANE_String_Const *) malloc ((n + 1) * sizeof (SANE_String_Const *));
  for (n = 0; n < gp_list_count (dir_list); n++)
    {
      gp_list_get_name (dir_list, n, &val);
      folder_list[n] = strdup (val);
      if (strchr ((const char *) folder_list[n], ' '))
	{
	  *strchr ((const char *) folder_list[n], ' ') = '\0';
	}
    }
  if (n == 0)
    {
      folder_list[n++] = (SANE_String) strdup ("");
    }

  folder_list[n] = NULL;
  sod[GPHOTO2_OPT_FOLDER].constraint.string_list =
    (SANE_String_Const *) folder_list;

  Cam_data.pic_taken = 0;
  Cam_data.pic_left = 1;	/* Just a guess! */

  return SANE_STATUS_GOOD;

}

/* 
 * erase() - erase file from camera corresponding to 
 *	current picture number.  Does not update any of the other
 *	backend data structures.
 */
static SANE_Int
erase (void)
{
  SANE_String_Const filename;

  sprintf (cmdbuf, "%s/%s", (char *) TopFolder,
	   (const char *) folder_list[current_folder]);
  CHECK_RET (gp_list_get_name
	     (dir_list, Cam_data.current_picture_number - 1, &filename));

  CHECK_RET (gp_camera_file_delete (camera, cmdbuf, filename));

  return SANE_STATUS_GOOD;
}

/*
 * change_res() - FIXME:  Would like to set resolution, but haven't figure
 * 	out how to control that yet.
 */
static SANE_Int
change_res (SANE_Byte res)
{

  return (res - res);

}

/*
 * sane_init() - Initialization function from SANE API.  Initialize some
 *	data structures, verify that all the necessary config information
 *	is present, and initialize gphoto2
 */
SANE_Status
sane_init (SANE_Int * version_code, SANE_Auth_Callback UNUSEDARG authorize)
{
  SANE_Int n, entries;
  SANE_Char f[] = "sane_init";
  SANE_Char dev_name[PATH_MAX], *p;
  SANE_Char buf[256];
  size_t len;
  FILE *fp;

  DBG_INIT ();

  if (getenv ("GP_DEBUG"))
    {
      gp_debug_set_level (atoi (getenv ("GP_DEBUG")));
    }
  else
    {
      /* NOTE:  One side effect of gp_debug_set_level() is that
       * it initializes the gphoto2 library, so we need it even
       * when the level is 0.
       */
      gp_debug_set_level (0);
    }
  if (version_code)
    *version_code = SANE_VERSION_CODE (V_MAJOR, V_MINOR, 0);

  fp = sanei_config_open (GPHOTO2_CONFIG_FILE);

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
	      p = dev_name + 5;
	      if (p)
		Cam_data.port = strdup (p);
	      DBG (20, "Config file port=%s\n", Cam_data.port);

	      /* Validate port */
	      CHECK_RET (entries = gp_port_count_get ());
	      for (n = 0; n < entries; n++)
		{
		  gp_port_info info;
		  CHECK_RET (gp_port_info_get (n, &info));
		  if (strcmp (Cam_data.port, info.path) == 0)
		    {
		      break;
		    }
		}
	      if (n == entries)
		{
		  DBG (0,
		       "%s: error: %s is not a valid gphoto2 port.  Use \"gphoto2 --list-ports\" for list.\n",
		       "init_gphoto2", Cam_data.port);
		  exit (1);
		}
	    }
	  else if (strncmp (dev_name, "camera=", 7) == 0)
	    {
	      Cam_data.camera_name = strdup (dev_name + 7);
	      DBG (20, "Config file camera=%s\n", Cam_data.camera_name);
	      sprintf (buf, "Image selection - %s", Cam_data.camera_name);

	      CHECK_RET (entries = gp_camera_count ());
	      for (n = 0; n < entries; n++)
		{
		  const char *buf;
		  CHECK_RET (gp_camera_name (n, &buf));
		  if (strcmp (Cam_data.camera_name, buf) == 0)
		    {
		      break;
		    }
		}
	      if (n == entries)
		{
		  DBG (0,
		       "%s: error: %s is not a valid camera type.  Use \"gphoto2 --list-cameras\" for list.\n",
		       f, Cam_data.camera_name);
		  return SANE_STATUS_INVAL;
		}

	      sod[GPHOTO2_OPT_IMAGE_SELECTION].title = strdup (buf);
	    }
	  else if (strcmp (dev_name, "dumpinquiry") == 0)
	    {
	      dumpinquiry = SANE_TRUE;
	    }
	  else if (strncmp (dev_name, "speed=", 6) == 0)
	    {
	      sscanf (&dev_name[6], "%d", &Cam_data.speed);

	      DBG (20, "Config file speed=%u\n", Cam_data.speed);

	    }
	  else if (strncmp (dev_name, "resolution=", 11) == 0)
	    {
	      sscanf (&dev_name[11], "%dx%d", &highres_width,
		      &highres_height);
	      DBG (20, "Config file resolution=%ux%u\n", highres_width,
		   highres_height);
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
	}
      fclose (fp);
    }

  if (init_gphoto2 () != SANE_STATUS_GOOD)
    return SANE_STATUS_INVAL;

  if (get_info () != SANE_STATUS_GOOD)
    {
      DBG (1, "error: could not get info\n");
      close_gphoto2 ();
      return SANE_STATUS_INVAL;
    }

  /* load the current images array */
  get_pictures_info ();

  if (Cam_data.pic_taken == 0)
    {
      Cam_data.current_picture_number = 0;
      parms.bytes_per_line = 0;
      parms.pixels_per_line = 0;
      parms.lines = 0;
    }
  else
    {
      Cam_data.current_picture_number = 1;
/* OLD:
      set_res (Cam_data.Pictures[Cam_data.current_picture_number - 1].low_res);
*/
      set_res (gphoto2_opt_lowres);
    }

  if (dumpinquiry)
    {
      SANE_Int x = 0;
      DBG (0, "\nCamera information:\n~~~~~~~~~~~~~~~~~\n\n");
      DBG (0, "Model                            : %s\n", abilities->model);
      DBG (0, "Pictures                         : %d\n", Cam_data.pic_taken);
      DBG (0, "Serial port support              : %s\n",
	   SERIAL_SUPPORTED (abilities->port) ? "yes" : "no");
      DBG (0, "Parallel port support            : %s\n",
	   PARALLEL_SUPPORTED (abilities->port) ? "yes" : "no");
      DBG (0, "USB support                      : %s\n",
	   USB_SUPPORTED (abilities->port) ? "yes" : "no");
      DBG (0, "IEEE1394 support                 : %s\n",
	   IEEE1394_SUPPORTED (abilities->port) ? "yes" : "no");
      DBG (0, "Network support                  : %s\n",
	   NETWORK_SUPPORTED (abilities->port) ? "yes" : "no");

      if (abilities->speed[0] != 0)
	{
	  DBG (0, "Transfer speeds supported        :\n");
	  do
	    {
	      DBG (0, "                                 : %i\n",
		   abilities->speed[x]);
	      x++;
	    }
	  while (abilities->speed[x] != 0);
	}
      DBG (0, "Capture choices                  :\n");
      if (abilities->operations & GP_OPERATION_CAPTURE_IMAGE)
	DBG (0, "                                 : Image\n");
      if (abilities->operations & GP_OPERATION_CAPTURE_VIDEO)
	DBG (0, "                                 : Video\n");
      if (abilities->operations & GP_OPERATION_CAPTURE_AUDIO)
	DBG (0, "                                 : Audio\n");
      if (abilities->operations & GP_OPERATION_CAPTURE_PREVIEW)
	DBG (0, "                                 : Preview\n");
      DBG (0, "Configuration support            : %s\n",
	   abilities->operations & GP_OPERATION_CONFIG ? "yes" : "no");

      DBG (0, "Delete files on camera support   : %s\n",
	   abilities->
	   file_operations & GP_FILE_OPERATION_DELETE ? "yes" : "no");
      DBG (0, "File preview (thumbnail) support : %s\n",
	   abilities->
	   file_operations & GP_FILE_OPERATION_PREVIEW ? "yes" : "no");
      DBG (0, "File upload support              : %s\n",
	   abilities->
	   folder_operations & GP_FOLDER_OPERATION_PUT_FILE ? "yes" : "no");


    }

  return SANE_STATUS_GOOD;
}

/*
 * sane_exit() - Required by SANE API, but otherwise not used
 */
void
sane_exit (void)
{
}

/* Device select/open/close */

static const SANE_Device dev[] = {
  {
   "0",
   "Gphoto2",
   "Supported",
   "still camera"},
};

static const SANE_Device *devlist[] = {
  dev + 0, 0
};

/*
 * sane_get_devices() - From SANE API
 */
SANE_Status
sane_get_devices (const SANE_Device *** device_list, SANE_Bool
		  UNUSEDARG local_only)
{

  DBG (127, "sane_get_devices called\n");

  *device_list = devlist;
  return SANE_STATUS_GOOD;
}

/*
 * sane_open() - From SANE API
 */

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

  DBG (4, "sane_open: pictures taken=%d\n", Cam_data.pic_taken);

  return SANE_STATUS_GOOD;
}

/*
 * sane_close() - From SANE API
 */

void
sane_close (SANE_Handle handle)
{
  DBG (127, "sane_close called\n");
  if (handle == MAGIC)
    is_open = 0;

  DBG (127, "sane_close returning\n");
}

/*
 * sane_get_option_descriptor() - From SANE API
 */

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

/*
 * sane_control_option() - From SANE API
 */

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
	  if (*(SANE_Word *) value <= Cam_data.pic_taken)
	    Cam_data.current_picture_number = *(SANE_Word *) value;
	  else
	    Cam_data.current_picture_number = Cam_data.pic_taken;

	  myinfo |= SANE_INFO_RELOAD_PARAMS;

	  /* get the image's resolution, unless the camera has no 
	   * pictures yet 
	   */
	  if (Cam_data.pic_taken != 0)
	    {
/* OLD:
	      set_res (Cam_data.
		       Pictures[Cam_data.current_picture_number - 1].low_res);
*/
	      set_res (gphoto2_opt_lowres);
	    }
	  break;

	case GPHOTO2_OPT_THUMBS:
	  gphoto2_opt_thumbnails = !!*(SANE_Word *) value;
	  myinfo |= SANE_INFO_RELOAD_PARAMS;

	  if (Cam_data.pic_taken != 0)
	    {
/* OLD:
	      set_res (Cam_data.
		       Pictures[Cam_data.current_picture_number - 1].low_res);
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
	      sod[GPHOTO2_OPT_LOWRES].cap |= SANE_CAP_INACTIVE;
	      /* and activate the image number selector, if there are 
	       * pictures available */
	      if (Cam_data.current_picture_number)
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

/* FIXME - change the number of pictures left depending on resolution
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
	  sod[GPHOTO2_OPT_LOWRES].cap |= SANE_CAP_INACTIVE;
	  /* and activate the image number selector */
	  sod[GPHOTO2_OPT_IMAGE_NUMBER].cap &= ~SANE_CAP_INACTIVE;

	  myinfo |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;

	  DBG (1, "FIXME: Set all defaults here!\n");
	  break;

	case GPHOTO2_OPT_INIT_GPHOTO2:
	  if (init_gphoto2 () != SANE_STATUS_GOOD)
	    {
	      return SANE_STATUS_INVAL;
	    }
	  if (get_info () != SANE_STATUS_GOOD)
	    {
	      DBG (1, "error: could not get info\n");
	      close_gphoto2 ();
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
	  *(SANE_Word *) value = Cam_data.current_picture_number;
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
	  strncpy ((char *) value, (const char *) folder_list[current_folder],
		   256);
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

/*
 * sane_get_parameters() - From SANE API
 */
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

  if (data_file_current_index + 512 > data_file_total_size)
    {
      n = data_file_total_size - data_file_current_index;
    }
  else
    {
      n = 512;
    }

  memcpy (src->buffer, data_ptr + data_file_current_index, n);
  data_file_current_index += n;

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

/*
 * sane_start() - From SANE API
 */
SANE_Status
sane_start (SANE_Handle handle)
{
  SANE_String_Const filename, mime_type;

  DBG (127, "sane_start called\n");
  if (handle != MAGIC || !is_open ||
      (Cam_data.current_picture_number == 0
       && gphoto2_opt_snap == SANE_FALSE))
    return SANE_STATUS_INVAL;	/* Unknown handle ... */

  if (Cam_data.scanning)
    return SANE_STATUS_EOF;

/*
 * This shouldn't normally happen, but we allow it as a special case
 * when batch/autoinc are in effect.  The first illegal picture number
 * terminates the scan
 */
  if (Cam_data.current_picture_number > Cam_data.pic_taken)
    {
      return SANE_STATUS_INVAL;
    }

  if (gphoto2_opt_snap)
    {
      /*
       * Don't allow picture unless there is room in the 
       * camera.
       */
      if (Cam_data.pic_left == 0)
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

  CHECK_RET (gp_file_new (&data_file));
  sprintf (cmdbuf, "%s/%s", (char *) TopFolder,
	   (const char *) folder_list[current_folder]);
  CHECK_RET (gp_list_get_name
	     (dir_list, Cam_data.current_picture_number - 1, &filename));

  CHECK_RET (gp_camera_file_get (camera, cmdbuf, filename,
				 gphoto2_opt_thumbnails ? GP_FILE_TYPE_PREVIEW
				 : GP_FILE_TYPE_NORMAL, data_file));

  CHECK_RET (gp_file_get_mime_type (data_file, &mime_type));
  if (strcmp (GP_MIME_JPEG, mime_type) != 0)
    {
      DBG (0, "FIXME - Only jpeg files currently supported\n");
      exit (1);
    }

  CHECK_RET (gp_file_get_data_and_size
	     (data_file, &data_ptr, &data_file_total_size));

  converter_init ();

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

  Cam_data.scanning = SANE_TRUE;	/* don't overlap scan requests */

  return SANE_STATUS_GOOD;
}

/*
 * sane_read() - From SANE API
 */
SANE_Status
sane_read (SANE_Handle UNUSEDARG handle, SANE_Byte * data,
	   SANE_Int max_length, SANE_Int * length)
{
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

  if (converter_scan_complete ())
    {
      SANE_Status retval = converter_do_scan_complete_cleanup ();
      if (retval != SANE_STATUS_GOOD)
	{
	  return retval;
	}
    }

  *length = converter_fill_buffer ();
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

/*
 * sane_cancel() - From SANE API
 */
void
sane_cancel (SANE_Handle UNUSEDARG handle)
{
  if (Cam_data.scanning)
    {
      Cam_data.scanning = SANE_FALSE;	/* done with scan */
    }
  else
    DBG (4, "sane_cancel: not scanning - nothing to do\n");
}

/*
 * sane_set_io_mode() - From SANE API
 */
SANE_Status
sane_set_io_mode (SANE_Handle UNUSEDARG handle, SANE_Bool
		  UNUSEDARG non_blocking)
{
  return SANE_STATUS_UNSUPPORTED;
}

/*
 * sane_get_select_fd() - From SANE API
 */
SANE_Status
sane_get_select_fd (SANE_Handle UNUSEDARG handle, SANE_Int * UNUSEDARG fd)
{
  return SANE_STATUS_UNSUPPORTED;
}

/*
 * get_pictures_info - load information about all pictures currently in
 *			camera:  Mainly the mapping of picture number
 *			to picture name.  We'ld like to get other
 *			information such as image size, but the API 
 *			doesn't provide any support for that.
 */
static PictureInfo *
get_pictures_info (void)
{
  SANE_Char f[] = "get_pictures_info";
  SANE_Char path[256];
  SANE_Int num_pictures;
  SANE_Int p;
  PictureInfo *pics;

  if (Cam_data.Pictures)
    {
      free (Cam_data.Pictures);
      Cam_data.Pictures = NULL;
    }

  strcpy (path, TopFolder);
  if (folder_list[current_folder] != NULL)
    {
      strcat (path, "/");
      strcat (path, (const char *) folder_list[current_folder]);
    }

  num_pictures = read_dir (path, 1);
  Cam_data.pic_taken = num_pictures;
  if (num_pictures > 0)
    {
      sod[GPHOTO2_OPT_IMAGE_NUMBER].cap &= ~SANE_CAP_INACTIVE;
      image_range.min = 1;
      image_range.max = num_pictures;
    }

  if ((pics = (PictureInfo *) malloc (Cam_data.pic_taken *
				      sizeof (PictureInfo))) == NULL)
    {
      DBG (1, "%s: error: allocate memory for pictures array\n", f);
      return NULL;
    }

  for (p = 0; p < Cam_data.pic_taken; p++)
    {
      if (get_picture_info (pics + p, p) == -1)
	{
	  free (pics);
	  return NULL;
	}
    }

  Cam_data.Pictures = pics;
  return pics;
}

/*
 * sane_picture_info() - get info about picture p.  Currently we have no
 *	way to get information about a picture beyond it's name.
 */
static SANE_Int
get_picture_info (PictureInfo * pic, SANE_Int p)
{

  SANE_Char f[] = "get_picture_info";
  const char *name;

  DBG (4, "%s: info for pic #%d\n", f, p);

  gp_list_get_name (dir_list, p, &name);
  DBG (4, "Name is %s\n", name);

  read_info (name);

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
  SANE_Char f[] = "snap_pic";
  CameraFilePath path;

  /* make sure camera is set to our settings state */
  if (change_res (gphoto2_opt_lowres) == -1)
    {
      DBG (1, "%s: Failed to set resolution\n", f);
      return SANE_STATUS_INVAL;
    }

  CHECK_RET (gp_camera_capture (camera, GP_OPERATION_CAPTURE_IMAGE, &path));

  /* Can't just increment picture count, because if the camera has
   * zero pictures we may not know the folder name.  Start over
   * with get_info and get_pictures_info
   */
  if (get_info () != SANE_STATUS_GOOD)
    {
      DBG (1, "error: could not get info\n");
      close_gphoto2 ();
      return SANE_STATUS_INVAL;
    }

  if (get_pictures_info () == NULL)
    {
      DBG (1, "%s: Failed to get new picture info\n", f);
      /* FIXME - I guess we should try to erase the image here */
      return SANE_STATUS_INVAL;
    }


  sod[GPHOTO2_OPT_IMAGE_NUMBER].cap |= SANE_CAP_INACTIVE;
  Cam_data.current_picture_number = Cam_data.pic_taken;
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
  SANE_Char f[] = "read_dir";

  /* Free up current list */
  if (dir_list != NULL)
    {
      if (gp_list_free (dir_list) < 0)
	{
	  DBG (0, "%s: errror: gp_list_free failed\n", f);
	}
      dir_list = NULL;
    }
  if (gp_list_new (&dir_list) < 0)
    {
      DBG (0, "%s: errror: gp_list_new failed\n", f);
    }

  if (read_files)
    {
      CHECK_RET (gp_camera_folder_list_files (camera, dir, dir_list));
    }
  else
    {
      CHECK_RET (gp_camera_folder_list_folders (camera, dir, dir_list));
    }

  retval = gp_list_count (dir_list);

  return retval;
}

/*
 * read_info - read the info block from camera for the specified file
 *	NOT YET SUPPORTED - If it were we could use it to do things 
 *	like update the image size parameters displayed by the GUI
 */
static SANE_Int
read_info (SANE_String_Const fname)
{
  SANE_Char path[256];

  strcpy (path, "\\PCCARD\\DCIM\\");
  strcat (path, (const char *) folder_list[current_folder]);
  strcat (path, "\\");
  strcat (path, fname);

  return 0;
}

/*
 *  set_res - set picture size depending on resolution settings 
 */
static void
set_res (SANE_Int lowres)
{
  lowres - lowres;

  if (gphoto2_opt_thumbnails)
    {
      parms.bytes_per_line = THUMB_WIDTH * 3;
      parms.pixels_per_line = THUMB_WIDTH;
      parms.lines = THUMB_HEIGHT;
    }
  else
    {
      parms.bytes_per_line = HIGHRES_WIDTH * 3;
      parms.pixels_per_line = HIGHRES_WIDTH;
      parms.lines = HIGHRES_HEIGHT;
    }
}

/*
 * converter_do_scan_complete_cleanup - do everything that needs to be 
 *      once a "scan" has been completed:  Unref the file, Erase the image, 
 *      and increment image number to point to next picture.
 */
static SANE_Status
converter_do_scan_complete_cleanup (void)
{
  CameraList *tmp_list;
  SANE_Int i;
  SANE_String_Const filename;

  gp_file_unref (data_file);

  if (gphoto2_opt_erase)
    {
      DBG (127, "sane_read bp%d, erase image\n", __LINE__);
      if (erase () == -1)
	{
	  DBG (1, "Failed to erase memory\n");
	  return SANE_STATUS_INVAL;
	}


      sprintf (cmdbuf, "%s/%s", (char *) TopFolder,
	       (const char *) folder_list[current_folder]);
      CHECK_RET (gp_list_get_name
		 (dir_list, Cam_data.current_picture_number - 1, &filename));

      Cam_data.pic_taken--;
      Cam_data.pic_left++;
      if (Cam_data.current_picture_number > Cam_data.pic_taken)
	{
	  Cam_data.current_picture_number = Cam_data.pic_taken;
	}
      image_range.max--;
      if (image_range.max == 0)
	{
	  sod[GPHOTO2_OPT_IMAGE_NUMBER].cap |= SANE_CAP_INACTIVE;
	}
      myinfo |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;

      /* Too bad we don't have an API function for deleting a
       * list item.  Instead, we copy all the entries in the
       * current list, skipping over the deleted entry, and then
       * replace the current list with the new list.
       */
      gp_list_new (&tmp_list);

      for (i = 0; i < gp_list_count (dir_list); i++)
	{
	  SANE_String_Const tfilename;

	  CHECK_RET (gp_list_get_name (dir_list, i, &tfilename));
	  /* If not the one to delete, copy to the new list */
	  if (strcmp (tfilename, filename) != 0)
	    {
	      CHECK_RET (gp_list_append (tmp_list, tfilename, NULL));
	    }
	}
      gp_list_free (dir_list);
      dir_list = tmp_list;

    }
  if (gphoto2_opt_autoinc)
    {
      if (Cam_data.current_picture_number <= Cam_data.pic_taken)
	{
	  Cam_data.current_picture_number++;

	  myinfo |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;

	  /* get the image's resolution */
/* OLD:
	      set_res (Cam_data.Pictures[Cam_data.current_picture_number - 1].
		       low_res);
*/
	  set_res (gphoto2_opt_lowres);
	}
      DBG (4, "Increment count to %d (total %d)\n",
	   Cam_data.current_picture_number, Cam_data.pic_taken);
    }
  return SANE_STATUS_EOF;
}

/*
 * converter_fill_buffer - Fill line buffer with next input line from image.  
 * 	Currently assumes jpeg, but this is where we would put the switch 
 * 	to handle other image types.
 */
static SANE_Int
converter_fill_buffer (void)
{

/* 
 * FIXME:  Current implementation reads one scan line at a time.  Part
 * of the reason for this is in the original code is to give the frontend 
 * a chance to update  * the progress marker periodically.  Since the gphoto2
 * driver sucks in the whole image before decoding it, perhaps we could
 * come up with a simpler implementation.
 */

  SANE_Int lines = 1;

  (void) jpeg_read_scanlines (&cinfo, dest_mgr->buffer, lines);
  (*dest_mgr->put_pixel_rows) (&cinfo, dest_mgr, lines, (char *) linebuffer);

  return cinfo.output_width * cinfo.output_components * lines;
}

/*
 * converter_scan_complete  - Check if all the data for the image has been read. 
 *	Currently assumes jpeg, but this is where we would put the 
 *	switch to handle other image types.
 */
static SANE_Bool
converter_scan_complete (void)
{
  if (cinfo.output_scanline >= cinfo.output_height)
    {
      return SANE_TRUE;
    }
  else
    {
      return SANE_FALSE;
    }
}

/*
 * converter_init  - Initialize image conversion data.
 *	Currently assumes jpeg, but this is where we would put the 
 *	switch to handle other image types.
 */
static void
converter_init (void)
{
  SANE_Int row_stride;
  struct jpeg_error_mgr jerr;
  my_src_ptr src;

  data_file_current_index = 0;

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

  parms.bytes_per_line = cinfo.output_width * 3;	/* 3 colors */
  parms.pixels_per_line = cinfo.output_width;
  parms.lines = cinfo.output_height;

}
