/* sane - Scanner Access Now Easy.
   Copyright (C) 1997 Geoffrey T. Dairiki
   Support for HP PhotoSmart Photoscanner by Peter Kirchgessner
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

   This file is part of a SANE backend for HP Scanners supporting
   HP Scanner Control Language (SCL).
*/

static char *hp_backend_version = "0.82";
/* Changes:

   V 0.82, 28-Feb-99, Ewald de Witt <ewald@pobox.com>:
      - add options 'exposure time' and 'data width'

   V 0.81, 11-Jan-99, PK:
      - occasionally 'scan from ADF' was active for Photoscanner

   V 0.80, 10-Jan-99, PK:
      - fix problem with scan size for ADF-scan
        (thanks to Christop Biardzki <cbi@allgaeu.org> for tests)
      - add option "unload after scan" for HP PhotoScanner
      - no blanks in command line options
      - fix problem with segmentation fault for scanimage -d hp:/dev/sga
        with /dev/sga not included in hp.conf

   V 0.72, 25-Dec-98, PK:
      - add patches from mike@easysw.com to fix problems:
        - core dumps by memory alignment
        - config file to accept matching devices (scsi HP)
      - add simulation for brightness/contrast/custom gamma table
        if not supported by scanner
      - add configuration options for connect-...

   V 0.72c, 04-Dec-98, PK:
      - use sanei_pio
      - try ADF support

   V 0.72b, 29-Nov-98 James Carter <james@cs.york.ac.uk>, PK:
      - try to add parallel scanner support

   V 0.71, 14-Nov-98 PK:
      - add HP 6200 C
      - cleanup hp_scsi_s structure
      - show calibrate button on photoscanner only for print media
      - suppress halftone mode on photoscanner
      - add media selection for photoscanner

   V 0.70, 26-Jul-98 PK:
      - Rename global symbols to sanei_...
        Change filenames to hp-...
        Use backend name hp

   V 0.65, 18-Jul-98 PK:
      - Dont use pwd.h for VACPP-Compiler to get home-directory,
        check $SANE_HOME_XHP instead

   V 0.64, 12-Jul-98 PK:
      - only download calibration file for media = 1 (prints)
      - Changes for VACPP-Compiler (check macros __IBMC__, __IBMCPP__)

   V 0.63, 07-Jun-98 PK:
      - fix problem with custom gamma table
      - Add unload button

   V 0.62, 25-May-98 PK:
      - make it compilable under sane V 0.73

   V 0.61, 28-Mar-98, Peter Kirchgessner <pkirchg@aol.com>:
      - Add support for HP PhotoSmart Photoscanner
      - Use more inquiries to see what the scanner supports
      - Add options: calibrate/Mirror horizontal+vertical
      - Upload/download calibration data
*/

#define VERSIO                                8

#include <sane/config.h>
#include "hp.h"

#include <string.h>
/* #include <sys/types.h> */
/* #include <sane/sane.h> */
#include <sane/sanei_config.h>
#include <sane/sanei_backend.h>
/* #include <sane/sanei_debug.h> */
#include "hp-device.h"
#include "hp-handle.h"

#ifndef PATH_MAX
# define PATH_MAX	1024
#endif

#if (defined(__IBMC__) || defined(__IBMCPP__))
#ifndef _AIX
#define inline /* */
#endif
#endif

#ifndef NDEBUG
#include <ctype.h>
void
sanei_hp_dbgdump (const void * bufp, size_t len)
{
  const hp_byte_t *buf	= bufp;
  int		offset	= 0;
  int		i;
  FILE *	fp	= stderr;

  for (offset = 0; offset < len; offset += 16)
    {
      fprintf(fp, " 0x%04X ", offset);
      for (i = offset; i < offset + 16 && i < len; i++)
	  fprintf(fp, " %02X", buf[i]);
      while (i++ < offset + 16)
	  fputs("   ", fp);
      fputs("  ", fp);
      for (i = offset; i < offset + 16 && i < len; i++)
	  fprintf(fp, "%c", isprint(buf[i]) ? buf[i] : '.');
      fputs("\n", fp);
    }
}

#endif

typedef struct info_list_el_s * HpDeviceInfoList;
struct info_list_el_s
{
    HpDeviceInfoList    next;
    HpDeviceInfo        info;
};

typedef struct device_list_el_s * HpDeviceList;
struct device_list_el_s
{
    HpDeviceList	next;
    HpDevice	 	dev;
};

/* Global state */
static struct hp_global_s {
    hp_bool_t	is_up;
    hp_bool_t	config_read;

    const SANE_Device ** devlist;

    HpDeviceList	device_list;
    HpDeviceList	handle_list;
    HpDeviceInfoList    infolist;

    HpDeviceConfig      config;
} global;


/* Get the info structure for a device. If not available in global list */
/* add new entry and return it */
static HpDeviceInfo *
hp_device_info_create (const char *devname)

{
 HpDeviceInfoList  *infolist = &(global.infolist);
 HpDeviceInfoList  infolistelement;
 HpDeviceInfo *info;
 int k, found;

 if (!global.is_up) return 0;

 found = 0;
 infolistelement = 0;
 info = 0;
 while (*infolist)
 {
   infolistelement = *infolist;
   info = &(infolistelement->info);
   if (strcmp (info->devname, devname) == 0)  /* Already in list ? */
   {
     found = 1;
     break;
   }
   infolist = &(infolistelement->next);
 }

 if (found)  /* Clear old entry */
 {
   memset (infolistelement, 0, sizeof (*infolistelement));
 }
 else   /* New element */
 {
   infolistelement = (HpDeviceInfoList)
                        sanei_hp_allocz (sizeof (*infolistelement));
   if (!infolistelement) return 0;
   info = &(infolistelement->info);
   *infolist = infolistelement;
 }

 k = sizeof (info->devname);
 strncpy (info->devname, devname, k);
 info->devname[k-1] = '\0';

 return info;
}

static void
hp_init_config (HpDeviceConfig *config)

{
  if (config)
  {
    config->connect = HP_CONNECT_SCSI;
    config->use_scsi_request = 1;
  }
}

static HpDeviceConfig *
hp_global_config_get (void)

{
 if (!global.is_up) return 0;
 return &(global.config);
}

static SANE_Status
hp_device_config_add (const char *devname)

{
 HpDeviceInfo *info;
 HpDeviceConfig *config;

 info = hp_device_info_create (devname);
 if (!info) return SANE_STATUS_INVAL;

 config = hp_global_config_get ();

 if (config)
 {
   memcpy (&(info->config), config, sizeof (info->config));
   info->config_is_up = 1;
 }
 else     /* Initialize with default configuration */
 {
   DBG(3, "hp_device_config_add: No configuration found for device %s.\n\tUseing default\n",
       devname);
   hp_init_config (&(info->config));
   info->config_is_up = 1;
 }
 return SANE_STATUS_GOOD;
}

HpDeviceInfo *
sanei_hp_device_info_get (const char *devname)

{
 HpDeviceInfoList  *infolist;
 HpDeviceInfoList  infolistelement;
 HpDeviceInfo *info;
 int retries = 1;

 if (!global.is_up) return 0;

 do
 {
 infolist = &(global.infolist);
 while (*infolist)
 {
   infolistelement = *infolist;
   info = &(infolistelement->info);
   if (strcmp (info->devname, devname) == 0)  /* Found ? */
   {
     return info;
   }
   infolist = &(infolistelement->next);
 }

 /* No configuration found. Assume default */
 DBG(1, "hp_device_info_get: device %s not configured. Using default\n",
     devname);
 if (hp_device_config_add (devname) != SANE_STATUS_GOOD)
   return 0;
 }
 while (retries-- > 0);

 return 0;
}

static void
hp_device_info_remove (void)
{
 HpDeviceInfoList  next, infolistelement = global.infolist;
 HpDeviceInfo *info;

 if (!global.is_up) return;

 while (infolistelement)
 {
   info = &(infolistelement->info);
   next = infolistelement->next;
   sanei_hp_free (infolistelement);
   infolistelement = next;
 }
}

static SANE_Status
hp_device_list_add (HpDeviceList * list, HpDevice dev)
{
  HpDeviceList new = sanei_hp_alloc(sizeof(*new));

  if (!new)
      return SANE_STATUS_NO_MEM;
  while (*list)
      list = &(*list)->next;

  *list = new;
  new->next = 0;
  new->dev = dev;
  return SANE_STATUS_GOOD;
}

static SANE_Status
hp_device_list_remove (HpDeviceList * list, HpDevice dev)
{
  HpDeviceList old;

  while (*list && (*list)->dev != dev)
      list = &(*list)->next;

  if (!*list)
      return SANE_STATUS_INVAL;

  old = *list;
  *list = (*list)->next;
  sanei_hp_free(old);
  return SANE_STATUS_GOOD;
}

static inline SANE_Status
hp_handle_list_add (HpDeviceList * list, HpHandle h)
{
  return hp_device_list_add(list, (HpDevice)h);
}

static inline SANE_Status
hp_handle_list_remove (HpDeviceList * list, HpHandle h)
{
  return hp_device_list_remove(list, (HpDevice)h);
}




static SANE_Status
hp_init (void)
{
  memset(&global, 0, sizeof(global));
  global.is_up++;
  return SANE_STATUS_GOOD;
}

static void
hp_destroy (void)
{
  if (global.is_up)
    {
      /* Close open handles */
      while (global.handle_list)
	  sane_close(global.handle_list->dev);

      /* Remove device infos */
      hp_device_info_remove ();

      sanei_hp_free_all();
      global.is_up = 0;
    }
}

static SANE_Status
hp_get_dev (const char *devname, HpDevice* devp)
{
  HpDeviceList  ptr;
  HpDevice	new;
  const HpDeviceInfo *info;
  char         *connect;
  HpConnect     hp_connect;
  SANE_Status   status;

  for (ptr = global.device_list; ptr; ptr = ptr->next)
      if (strcmp(sanei_hp_device_sanedevice(ptr->dev)->name, devname) == 0)
	{
	  if (devp)
	      *devp = ptr->dev;
	  return SANE_STATUS_GOOD;
	}

  info = sanei_hp_device_info_get (devname);
  hp_connect = info->config.connect;

  if (hp_connect == HP_CONNECT_SCSI) connect = "scsi";
  else if (hp_connect == HP_CONNECT_DEVICE) connect = "device";
  else if (hp_connect == HP_CONNECT_PIO) connect = "pio";
  else if (hp_connect == HP_CONNECT_USB) connect = "usb";
  else if (hp_connect == HP_CONNECT_RESERVE) connect = "reserve";
  else connect = "unknown";

  DBG(3, "hp_get_dev: New device %s, connect-%s, scsi-request=%lu\n",
      devname, connect, (unsigned long)info->config.use_scsi_request);

  if (!ptr)
  {
     status =  sanei_hp_device_new (&new, devname);

     if ( status != SANE_STATUS_GOOD )
       return status;
  }

  if (devp)
      *devp = new;

  RETURN_IF_FAIL( hp_device_list_add(&global.device_list, new) );

  return SANE_STATUS_GOOD;
}

static SANE_Status
hp_attach (const char *devname)
{
  hp_device_config_add (devname);
  return hp_get_dev (devname, 0);
}

static SANE_Status
hp_read_config (void)
{
  FILE *	fp;
  char		 buf[PATH_MAX], arg1[PATH_MAX], arg2[PATH_MAX], arg3[PATH_MAX];
  int           nl, nargs;
  HpDeviceConfig *config, df_config, dev_config;
  hp_bool_t     is_df_config;
  char          cu_device[PATH_MAX];

  if (!global.is_up)
      return SANE_STATUS_INVAL;
  if (global.config_read)
      return SANE_STATUS_GOOD;

  /* The default config will keep options set up until the first device is specified */
  hp_init_config (&df_config);
  config = &df_config;
  is_df_config = 1;
  cu_device[0] = '\0';

  DBG(1, "hp_read_config: hp backend v%s starts reading config file\n",
      hp_backend_version);

  if ((fp = sanei_config_open(HP_CONFIG_FILE)) != 0)
    {
      while (fgets(buf, sizeof(buf), fp))
	{
	  char *dev_name;

          nl = strlen (buf);
          while (nl > 0)
          {
            nl--;
            if (   (buf[nl] == ' ') || (buf[nl] == '\t')
                || (buf[nl] == '\r') || (buf[nl] == '\n'))
              buf[nl] = '\0';
            else
              break;
          }

          DBG(1, "hp_read_config: processing line <%s>\n", buf);

          nargs = sscanf (buf, "%s%s%s", arg1, arg2, arg3);
          if ((nargs <= 0) || (arg1[0] == '#')) continue;

          /* Option to process ? */
          if ((strcmp (arg1, "option") == 0) && (nargs >= 2))
          {
            if (strcmp (arg2, "connect-scsi") == 0)
            {
              config->connect = HP_CONNECT_SCSI;
            }
            else if (strcmp (arg2, "connect-device") == 0)
            {
              config->connect = HP_CONNECT_DEVICE;
              config->use_scsi_request = 0;
            }
            else if (strcmp (arg2, "connect-pio") == 0)
            {
              config->connect = HP_CONNECT_PIO;
              config->use_scsi_request = 0;
            }
            else if (strcmp (arg2, "connect-usb") == 0)
            {
              config->connect = HP_CONNECT_USB;
              config->use_scsi_request = 0;
            }
            else if (strcmp (arg2, "connect-reserve") == 0)
            {
              config->connect = HP_CONNECT_RESERVE;
              config->use_scsi_request = 0;
            }
            else if (strcmp (arg2, "disable-scsi-request") == 0)
            {
              config->use_scsi_request = 0;
            }
            else
            {
              DBG(1,"hp_read_config: Invalid option %s\n", arg2);
            }
          }
          else   /* No option. This is the start of a new device */
          {
            if (is_df_config) /* Did we only read default configurations ? */
            {
              is_df_config = 0;
              config = &dev_config;
            }
            if (cu_device[0] != '\0')  /* Did we work on a device ? */
            {
              memcpy (hp_global_config_get (), &dev_config, sizeof (dev_config));
              DBG(1, "hp_read_config: attach %s\n", cu_device);
              sanei_config_attach_matching_devices (cu_device, hp_attach);
              cu_device[0] = '\0';
            }

            /* Initialize new device with default config */
            memcpy (&dev_config, &df_config, sizeof (dev_config));

            /* Cut off leading blanks of device name */
            dev_name = buf+strspn (buf, " \t\n\r");
            strcpy (cu_device, dev_name);    /* Save the device name */
          }
        }
        if (cu_device[0] != '\0')  /* Did we work on a device ? */
        {
          memcpy (hp_global_config_get (), &dev_config, sizeof (dev_config));
          DBG(1, "hp_read_config: attach %s\n", cu_device);
          sanei_config_attach_matching_devices (cu_device, hp_attach);
          cu_device[0] = '\0';
        }
      fclose (fp);
    }
  else
    {
      /* default to /dev/scanner instead of insisting on config file */
      char *dev_name = "/dev/scanner";

      memcpy (hp_global_config_get (), &df_config, sizeof (df_config));
      sanei_config_attach_matching_devices (dev_name, hp_attach);
    }

  global.config_read++;
  return SANE_STATUS_GOOD;
}

static SANE_Status
hp_update_devlist (void)
{
  HpDeviceList	devp;
  const SANE_Device **devlist;
  int		count	= 0;

  RETURN_IF_FAIL( hp_read_config() );

  if (global.devlist)
      sanei_hp_free(global.devlist);

  for (devp = global.device_list; devp; devp = devp->next)
      count++;

  if (!(devlist = sanei_hp_alloc((count + 1) * sizeof(*devlist))))
      return SANE_STATUS_NO_MEM;

  global.devlist = devlist;

  for (devp = global.device_list; devp; devp = devp->next)
      *devlist++ = sanei_hp_device_sanedevice(devp->dev);
  *devlist = 0;

  return SANE_STATUS_GOOD;
}


/*
 *
 */

SANE_Status
sane_init (SANE_Int *version_code, SANE_Auth_Callback authorize)
{
  DBG_INIT();
  DBG(3, "init called\n");

  hp_destroy();

  if (version_code)
    *version_code = SANE_VERSION_CODE (V_MAJOR, V_MINOR, VERSIO);

  return hp_init();
}

void
sane_exit (void)
{
  DBG(3, "exit called\n");
  hp_destroy();
}

SANE_Status
sane_get_devices (const SANE_Device ***device_list, SANE_Bool local_only)
{
  DBG(3, "get_devices called\n");

  RETURN_IF_FAIL( hp_update_devlist() );
  *device_list = global.devlist;
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_open (SANE_String_Const devicename, SANE_Handle *handle)
{
  HpDevice	dev	= 0;
  HpHandle	h;

  RETURN_IF_FAIL( hp_read_config() );

  if (devicename[0])
      RETURN_IF_FAIL( hp_get_dev(devicename, &dev) );
  else
    {
      /* empty devicname -> use first device */
      if (global.device_list)
	  dev = global.device_list->dev;
    }
  if (!dev)
      return SANE_STATUS_INVAL;

  if (!(h = sanei_hp_handle_new(dev)))
      return SANE_STATUS_NO_MEM;

  RETURN_IF_FAIL( hp_handle_list_add(&global.handle_list, h) );

  *handle = h;
  return SANE_STATUS_GOOD;
}

void
sane_close (SANE_Handle handle)
{
  HpHandle	h  = handle;

  if (!FAILED( hp_handle_list_remove(&global.handle_list, h) ))
      sanei_hp_handle_destroy(h);
}

const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int optnum)
{
  HpHandle 	h = handle;
  return sanei_hp_handle_saneoption(h, optnum);
}

SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int optnum,
		     SANE_Action action, void *valp, SANE_Int *info)
{
  HpHandle h = handle;
  return sanei_hp_handle_control(h, optnum, action, valp, info);
}

SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters *params)
{
  HpHandle h = handle;
  return sanei_hp_handle_getParameters(h, params);
}

SANE_Status
sane_start (SANE_Handle handle)
{
  HpHandle h = handle;
  return sanei_hp_handle_startScan(h);
}

SANE_Status
sane_read (SANE_Handle handle, SANE_Byte *buf, SANE_Int max_len, SANE_Int *len)
{
  HpHandle	h 	= handle;
  size_t	length	= max_len;
  SANE_Status	status;

  status =  sanei_hp_handle_read(h, buf, &length);
  *len = length;
  return status;
}

void
sane_cancel (SANE_Handle handle)
{
  HpHandle h = handle;
  sanei_hp_handle_cancel(h);
}

SANE_Status
sane_set_io_mode (SANE_Handle handle, SANE_Bool non_blocking)
{
  HpHandle h = handle;
  return sanei_hp_handle_setNonblocking(h, non_blocking);
}

SANE_Status
sane_get_select_fd (SANE_Handle handle, SANE_Int *fd)
{
  HpHandle h = handle;
  return sanei_hp_handle_getPipefd(h, fd);
}
