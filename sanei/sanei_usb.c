/* sane - Scanner Access Now Easy.
   Copyright (C) 2003 Rene Rebe (sanei_read_int)
   Copyright (C) 2001, 2002 Henning Meier-Geinitz
   Copyright (C) 2001 Frank Zago (sanei_usb_control_msg)
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

   This file provides a generic USB interface.  */

#include "../include/sane/config.h"

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <dirent.h>

#ifdef HAVE_LIBUSB
#include <usb.h>
#endif /* HAVE_LIBUSB */

#define BACKEND_NAME	sanei_usb
#include "../include/sane/sane.h"
#include "../include/sane/sanei_debug.h"
#include "../include/sane/sanei_usb.h"
#include "../include/sane/sanei_config.h"

typedef enum
{
  sanei_usb_method_scanner_driver = 0,	/* kernel scanner driver 
					   (Linux, BSD) */
  sanei_usb_method_libusb
}
sanei_usb_access_method_type;

typedef struct
{
  SANE_Bool open;
  sanei_usb_access_method_type method;
  int fd;
  SANE_String devname;
  SANE_Int vendor;
  SANE_Int product;
  SANE_Int bulk_in_ep;
  SANE_Int bulk_out_ep;
  SANE_Int int_in_ep;
  SANE_Int int_out_ep;
  SANE_Int interface_nr;
#ifdef HAVE_LIBUSB
  usb_dev_handle *libusb_handle;
  struct usb_device *libusb_device;
#endif				/* HAVE_LIBUSB */
}
device_list_type;

/* total number of devices that can be opened at the same time */
#define MAX_DEVICES 100

/* per-device information, using the functions' parameters dn as index */
static device_list_type devices[MAX_DEVICES];

#ifdef HAVE_LIBUSB
static int libusb_timeout = 30 * 1000;	/* 30 seconds */
#endif /* HAVE_LIBUSB */

#if defined (__linux__)
/* From /usr/src/linux/driver/usb/scanner.h */
#define SCANNER_IOCTL_VENDOR _IOR('U', 0x20, int)
#define SCANNER_IOCTL_PRODUCT _IOR('U', 0x21, int)
#define SCANNER_IOCTL_CTRLMSG _IOWR('U', 0x22, devrequest)
/* Older (unofficial) IOCTL numbers for Linux < v2.4.13 */
#define SCANNER_IOCTL_VENDOR_OLD _IOR('u', 0xa0, int)
#define SCANNER_IOCTL_PRODUCT_OLD _IOR('u', 0xa1, int)

/* From /usr/src/linux/include/linux/usb.h */
typedef struct
{
  unsigned char requesttype;
  unsigned char request;
  unsigned short value;
  unsigned short index;
  unsigned short length;
}
devrequest __attribute__ ((packed));

/* From /usr/src/linux/driver/usb/scanner.h */
struct ctrlmsg_ioctl
{
  devrequest req;
  void *data;
}
cmsg;
#endif /* __linux__ */

static SANE_Bool inited = SANE_FALSE;

static void
kernel_get_vendor_product (int fd, int *vendorID, int *productID)
{
#if defined (__linux__)
  /* read the vendor and product IDs via the IOCTLs */
  if (ioctl (fd, SCANNER_IOCTL_VENDOR, vendorID) == -1)
    {
      if (ioctl (fd, SCANNER_IOCTL_VENDOR_OLD, vendorID) == -1)
	DBG (3, "kernel_get_vendor_product: ioctl (vendor) "
	     "of device %d failed: %s\n", fd, strerror (errno));
    }
  if (ioctl (fd, SCANNER_IOCTL_PRODUCT, productID) == -1)
    {
      if (ioctl (fd, SCANNER_IOCTL_PRODUCT_OLD, productID) == -1)
	DBG (3, "sanei_usb_get_vendor_product: ioctl (product) "
	     "of device %d failed: %s\n", fd, strerror (errno));
    }
#endif /* defined (__linux__) */
      /* put more os-dependant stuff ... */
}

void
sanei_usb_init (void)
{
  SANE_String *prefix;
  SANE_String prefixlist[] = {
#if defined(__linux__)
    "/dev/", "usbscanner",
    "/dev/usb/", "scanner",
#elif defined(__FreeBSD__)
    "/dev/", "uscanner",
#endif
    0, 0
  };
  SANE_Int vendor, product;
  SANE_Char devname[1024];
  SANE_Int dn = 0;
  int fd;
#ifdef HAVE_LIBUSB
  struct usb_bus *bus;
  struct usb_device *dev;
#endif /* HAVE_LIBUSB */

  if (inited)
    return SANE_STATUS_GOOD;

  inited = SANE_TRUE;

  DBG_INIT ();
  memset (devices, 0, sizeof (devices));

  /* Check for devices using the kernel scanner driver */

  for (prefix = prefixlist; *prefix; prefix += 2)
    {
      SANE_String dir_name = *prefix;
      SANE_String base_name = *(prefix + 1);
      struct stat stat_buf;
      DIR *dir;
      struct dirent *dir_entry;

      if (stat (dir_name, &stat_buf) < 0)
	{
	  DBG (5, "sanei_usb_init: can't stat %s: %s\n", dir_name, strerror (errno));
	  continue;
	}
      if (!S_ISDIR (stat_buf.st_mode))
	{
	  DBG (5, "sanei_usb_init: %s is not a directory\n", dir_name);
	  continue;
	}
      if ((dir = opendir (dir_name)) == 0)
	{
	  DBG (5, "sanei_usb_init: cannot read directory %s: %s\n", dir_name,
		    strerror (errno));
	  continue;
	}
      
      while ((dir_entry = readdir (dir)) != 0)
	{
	  if (strncmp (base_name, dir_entry->d_name, strlen (base_name)) == 0)
	    {
	      if (strlen (dir_name) + strlen (dir_entry->d_name) + 1 > sizeof (devname))
		  continue;
	      sprintf (devname, "%s%s", dir_name, dir_entry->d_name);
	      fd = open (devname, O_RDWR);
	      if (fd < 0)
		{
		  DBG (5, "sanei_usb_init: couldn't open %s: %s\n", devname,
		      strerror (errno));
		  continue;
		}
	      vendor = -1;
	      product = -1;
	      kernel_get_vendor_product (fd, &vendor, &product);
	      close (fd);
	      devices[dn].devname = strdup (devname);
	      if (!devices[dn].devname)
		return;
	      devices[dn].vendor = vendor;
	      devices[dn].product = product;
	      devices[dn].method = sanei_usb_method_scanner_driver;
	      devices[dn].open = SANE_FALSE;
	      DBG (4, "sanei_usb_init: found kernel scanner device (0x%04x/0x%04x) at %s\n",
		   vendor, product, devname);
	      dn++;
	      if (dn >= MAX_DEVICES)
		return;
	    }
	}
    }

  /* Check for devices using libusb */
#ifdef HAVE_LIBUSB
  usb_init ();
#ifdef DBG_LEVEL
  if (DBG_LEVEL > 4)
    usb_set_debug (255);
#endif /* DBG_LEVEL */
  if (!usb_get_busses ())
    {
      usb_find_busses ();
      usb_find_devices ();
    }

  /* Check for the matching device */
  for (bus = usb_get_busses (); bus; bus = bus->next)
    {
      for (dev = bus->devices; dev; dev = dev->next)
	{
	  int interface;
	  SANE_Bool found;

	  if (!dev->config)
	    {
	      DBG (1, "sanei_usb_init: device 0x%04x/0x%04x is not configured\n",
		   dev->descriptor.idVendor, dev->descriptor.idProduct);
	      continue;
	    }
	  if (dev->descriptor.idVendor == 0 || dev->descriptor.idProduct == 0)
	    {
	      DBG (5, "sanei_usb_init: device 0x%04x/0x%04x looks like a root hub\n",
		   dev->descriptor.idVendor, dev->descriptor.idProduct);
	      continue;
	    }
	  found = SANE_FALSE;
	  for (interface = 0; interface < dev->config[0].bNumInterfaces && !found; interface++)
	    {
	      switch (dev->descriptor.bDeviceClass)
		{
		case USB_CLASS_VENDOR_SPEC:
		  found = SANE_TRUE;
		  break;
		case USB_CLASS_PER_INTERFACE:
		  switch (dev->config[0].interface[interface].altsetting[0].bInterfaceClass)
		    {
		    case USB_CLASS_VENDOR_SPEC:
		    case USB_CLASS_PER_INTERFACE:
		    case 16:                /* data? */
		      found = SANE_TRUE;
		      break;
		    }
		  break;
		}
	      if (!found)
		DBG (5, "sanei_usb_init: device 0x%04x/0x%04x, interface %d doesn't look like a "
		     "scanner (%d/%d)\n", dev->descriptor.idVendor,
		     dev->descriptor.idProduct, interface, dev->descriptor.bDeviceClass, 
		     dev->config[0].interface[interface].altsetting[0].bInterfaceClass);
	    }
	  interface--;
	  if (!found)
	    {
	      DBG (5, "sanei_usb_init: device 0x%04x/0x%04x: no suitable interfaces\n",
		   dev->descriptor.idVendor, dev->descriptor.idProduct);
	      continue;
	    }
	  
	  devices[dn].libusb_device = dev;
	  snprintf (devname, sizeof (devname), "libusb:%s:%s",
		    dev->bus->dirname, dev->filename);
	  devices[dn].devname = strdup (devname);
	  if (!devices[dn].devname)
	    return;
	  devices[dn].vendor = dev->descriptor.idVendor;
	  devices[dn].product = dev->descriptor.idProduct;
	  devices[dn].method = sanei_usb_method_libusb;
	  devices[dn].open = SANE_FALSE;
	  devices[dn].interface_nr = interface;
	  DBG (4, "sanei_usb_init: found libusb device (0x%04x/0x%04x) interface %d  at %s\n",
	       dev->descriptor.idVendor, dev->descriptor.idProduct, interface, devname);
	  dn++;
	  if (dn >= MAX_DEVICES)
	    return;
	}
    }
#endif /* HAVE_LIBUSB */
  DBG (5, "sanei_usb_init: found %d devices\n", dn);
}


/* This logically belongs to sanei_config.c but not every backend that
   uses sanei_config() wants to depend on sanei_usb.  */
void
sanei_usb_attach_matching_devices (const char *name,
				   SANE_Status (*attach) (const char *dev))
{
  char *vendor, *product;

  if (strncmp (name, "usb", 3) == 0)
    {
      SANE_Word vendorID = 0, productID = 0;

      name += 3;

      name = sanei_config_skip_whitespace (name);
      if (*name)
	{
	  name = sanei_config_get_string (name, &vendor);
	  if (vendor)
	    {
	      vendorID = strtol (vendor, 0, 0);
	      free (vendor);
	    }
	  name = sanei_config_skip_whitespace (name);
	}

      name = sanei_config_skip_whitespace (name);
      if (*name)
	{
	  name = sanei_config_get_string (name, &product);
	  if (product)
	    {
	      productID = strtol (product, 0, 0);
	      free (product);
	    }
	}
      sanei_usb_find_devices (vendorID, productID, attach);
    }
  else
    (*attach) (name);
}

SANE_Status
sanei_usb_get_vendor_product (SANE_Int dn, SANE_Word * vendor,
			      SANE_Word * product)
{
  SANE_Word vendorID = 0;
  SANE_Word productID = 0;

  if (dn >= MAX_DEVICES || dn < 0)
    {
      DBG (1, "sanei_usb_get_vendor_product: dn >= MAX_DEVICES || dn < 0\n");
      return SANE_STATUS_INVAL;
    }

  if (devices[dn].method == sanei_usb_method_scanner_driver)
    kernel_get_vendor_product (devices[dn].fd, &vendorID, &productID);
  else if (devices[dn].method == sanei_usb_method_libusb)
    {
#ifdef HAVE_LIBUSB
      vendorID = usb_device (devices[dn].libusb_handle)->descriptor.idVendor;
      productID =
	usb_device (devices[dn].libusb_handle)->descriptor.idProduct;
#else
      DBG (1, "sanei_usb_get_vendor_product: libusb support missing\n");
      return SANE_STATUS_UNSUPPORTED;
#endif /* HAVE_LIBUSB */
    }
  else
    {
      DBG (1, "sanei_usb_get_vendor_product: access method %d not "
	   "implemented\n", devices[dn].method);
      return SANE_STATUS_INVAL;
    }

  if (vendor)
    *vendor = vendorID;
  if (product)
    *product = productID;

  if (!vendorID || !productID)
    {
      DBG (3, "sanei_usb_get_vendor_product: device %d: Your OS doesn't "
	   "seem to support detection of vendor+product ids\n", dn);
      return SANE_STATUS_UNSUPPORTED;
    }
  else
    {
      DBG (3, "sanei_usb_get_vendor_product: device %d: vendorID: 0x%04x, "
	   "productID: 0x%04x\n", dn, vendorID, productID);
      return SANE_STATUS_GOOD;
    }
}

SANE_Status
sanei_usb_find_devices (SANE_Int vendor, SANE_Int product,
			SANE_Status (*attach) (SANE_String_Const dev))
{
  SANE_Int dn = 0;

  DBG (3,
       "sanei_usb_find_devices: vendor=0x%04x, product=0x%04x, attach=%p\n",
       vendor, product, attach);

  while (devices[dn].devname && dn < MAX_DEVICES)
    {
      if (devices[dn].vendor == vendor && devices[dn].product == product)
	if (attach)
	  attach (devices[dn].devname);
      dn++;
    }
  return SANE_STATUS_GOOD;
}

SANE_Status
sanei_usb_open (SANE_String_Const devname, SANE_Int * dn)
{
  int devcount;
  SANE_Bool found = SANE_FALSE;

  DBG (5, "sanei_usb_open: trying to open device `%s'\n", devname);
  if (!dn)
    {
      DBG (1, "sanei_usb_open: can't open `%s': dn == NULL\n", devname);
      return SANE_STATUS_INVAL;
    }

  for (devcount = 0; devcount < MAX_DEVICES && devices[devcount].devname != 0; devcount++)
    {
      if (strcmp (devices[devcount].devname, devname) == 0)
	{
	  if (devices[devcount].open)
	    {
	      DBG (1, "sanei_usb_open: device `%s' already open\n", devname);
	      return SANE_STATUS_INVAL;
	    }
	  found = SANE_TRUE;
	  break;
	}
    }

  if (!found)
    {
      DBG (1, "sanei_usb_open: can't find device `%s' in list\n", devname);
      return SANE_STATUS_INVAL;
    }

  if (devices[devcount].method == sanei_usb_method_libusb)
    {
#ifdef HAVE_LIBUSB
      struct usb_device *dev;
      struct usb_interface_descriptor *interface;
      int result, num;

      devices[devcount].libusb_handle = usb_open (devices[devcount].libusb_device);
      if (!devices[devcount].libusb_handle)
	{
	  SANE_Status status = SANE_STATUS_INVAL;

	  DBG (1, "sanei_usb_open: can't open device `%s': %s\n",
	       devname, strerror (errno));
	  if (errno == EPERM)
	    {
	      DBG (1, "Make sure you run as root or set appropriate "
		   "permissions\n");
	      status = SANE_STATUS_ACCESS_DENIED;
	    }
	  else if (errno == EBUSY)
	    {
	      DBG (1, "Maybe the kernel scanner driver claims the "
		   "scanner's interface?\n");
	      status = SANE_STATUS_DEVICE_BUSY;
	    }
	  return status;
	}

      dev = usb_device (devices[devcount].libusb_handle);

      /* Set the configuration */
      if (!dev->config)
	{
	  DBG (1, "sanei_usb_open: device `%s' not configured?\n", devname);
	  return SANE_STATUS_INVAL;
	}
      if (dev->descriptor.bNumConfigurations > 1)
	{
	  DBG (3, "sanei_usb_open: more than one "
	       "configuration (%d), choosing first config (%d)\n",
	       dev->descriptor.bNumConfigurations, 
	       dev->config[0].bConfigurationValue);
	}
      result = usb_set_configuration (devices[devcount].libusb_handle,
				      dev->config[0].bConfigurationValue);
      if (result < 0)
	{
	  SANE_Status status = SANE_STATUS_INVAL;

	  DBG (1, "sanei_usb_open: libusb complained: %s\n", usb_strerror ());
	  if (errno == EPERM)
	    {
	      DBG (1, "Make sure you run as root or set appropriate "
		   "permissions\n");
	      status = SANE_STATUS_ACCESS_DENIED;
	    }
	  else if (errno == EBUSY)
	    {
	      DBG (1, "Maybe the kernel scanner driver claims the "
		   "scanner's interface?\n");
	      status = SANE_STATUS_DEVICE_BUSY;
	    }
	  usb_close (devices[devcount].libusb_handle);
	  return status;
	}
      
      /* Claim the interface */
      result = usb_claim_interface (devices[devcount].libusb_handle, 
				    devices[devcount].interface_nr);
      if (result < 0)
	{
	  SANE_Status status = SANE_STATUS_INVAL;

	  DBG (1, "sanei_usb_open: libusb complained: %s\n", usb_strerror ());
	  if (errno == EPERM)
	    {
	      DBG (1, "Make sure you run as root or set appropriate "
		   "permissions\n");
	      status = SANE_STATUS_ACCESS_DENIED;
	    }
	  else if (errno == EBUSY)
	    {
	      DBG (1, "Maybe the kernel scanner driver claims the "
		   "scanner's interface?\n");
	      status = SANE_STATUS_DEVICE_BUSY;
	    }
	  usb_close (devices[devcount].libusb_handle);
	  return status;
	}
      interface = &dev->config[0].interface->altsetting[0];

#if 0
      /* Set alternative setting */
      /* Seems to break on Max OS X -> disabled */
      DBG (3, "sanei_usb_open: chosing first altsetting (%d) without "
	   "checking\n", interface->bAlternateSetting);

      result = usb_set_altinterface (devices[devcount].libusb_handle, 
				     interface->bAlternateSetting);
      if (result < 0)
	{
	  SANE_Status status = SANE_STATUS_INVAL;

	  DBG (1, "sanei_usb_open: libusb complained: %s\n", usb_strerror ());
	  if (errno == EPERM)
	    {
	      DBG (1, "Make sure you run as root or set appropriate "
		   "permissions\n");
	      status = SANE_STATUS_ACCESS_DENIED;
	    }
	  else if (errno == EBUSY)
	    {
	      DBG (1, "Maybe the kernel scanner driver claims the "
		   "scanner's interface?\n");
	      status = SANE_STATUS_DEVICE_BUSY;
	    }
	  usb_close (devices[devcount].libusb_handle);
	  return status;
	}
#endif

      /* Now we look for usable endpoints */
      for (num = 0; num < interface->bNumEndpoints; num++)
	{
	  struct usb_endpoint_descriptor *endpoint;
	  int address, direction, transfer_type;

	  endpoint = &interface->endpoint[num];
	  address = endpoint->bEndpointAddress & USB_ENDPOINT_ADDRESS_MASK;
	  direction = endpoint->bEndpointAddress & USB_ENDPOINT_DIR_MASK;
	  transfer_type = endpoint->bmAttributes & USB_ENDPOINT_TYPE_MASK;

	  /* save the endpoints we need later */
	  if (transfer_type == USB_ENDPOINT_TYPE_INTERRUPT)
	  {
	    DBG (5, "sanei_usb_open: found interupt-%s endpoint (address %d)\n",
	         direction ? "in" : "out", address);
	    if (direction)	/* in */
	    {
	      if (devices[devcount].int_in_ep)
		DBG (3, "sanei_usb_open: we already have a int-in endpoint "
		     "(address: %d), ignoring the new one\n",
		     devices[devcount].int_in_ep);
	      else
		devices[devcount].int_in_ep = endpoint->bEndpointAddress;
	    }
	    else
	      if (devices[devcount].int_out_ep)
		DBG (3, "sanei_usb_open: we already have a int-out endpoint "
		     "(address: %d), ignoring the new one\n",
		     devices[devcount].int_out_ep);
	      else
		devices[devcount].int_out_ep = endpoint->bEndpointAddress;
	  }
	  else if (transfer_type == USB_ENDPOINT_TYPE_BULK)
	  {
	    DBG (5, "sanei_usb_open: found bulk-%s endpoint (address %d)\n",
	         direction ? "in" : "out", address);
	    if (direction)	/* in */
	      {
		if (devices[devcount].bulk_in_ep)
		  DBG (3, "sanei_usb_open: we already have a bulk-in endpoint "
		       "(address: %d), ignoring the new one\n",
		       devices[devcount].bulk_in_ep);
		else
		  devices[devcount].bulk_in_ep = endpoint->bEndpointAddress;
	      }
	    else
	      {
	        if (devices[devcount].bulk_out_ep)
		  DBG (3, "sanei_usb_open: we already have a bulk-out endpoint "
		       "(address: %d), ignoring the new one\n",
		       devices[devcount].bulk_out_ep);
	        else
		  devices[devcount].bulk_out_ep = endpoint->bEndpointAddress;
	      }
	    }
	  /* ignore currently unsupported endpoints */
	  else {
	      DBG (5, "sanei_usb_open: ignoring %s-%s endpoint "
		   "(address: %d)\n",
		   transfer_type == USB_ENDPOINT_TYPE_CONTROL ? "control" :
		   transfer_type == USB_ENDPOINT_TYPE_ISOCHRONOUS
		   ? "isochronous" : "interrupt",
		   direction ? "in" : "out", address);
	      continue;
	    }
	}
#else /* not HAVE_LIBUSB */
      DBG (1, "sanei_usb_open: can't open device `%s': "
	   "libusb support missing\n", devname);
      return SANE_STATUS_UNSUPPORTED;
#endif /* not HAVE_LIBUSB */
    }
  else if (devices[devcount].method == sanei_usb_method_scanner_driver)
    {
      /* Using kernel scanner driver */
      devices[devcount].fd = open (devname, O_RDWR);
      if (devices[devcount].fd < 0)
	{
	  SANE_Status status = SANE_STATUS_INVAL;

	  if (errno == EACCES)
	    status = SANE_STATUS_ACCESS_DENIED;
	  else if (errno == ENOENT)
	    {
	      DBG (5, "sanei_usb_open: open of `%s' failed: %s\n",
		   devname, strerror (errno));
	      return status;
	    }
	  DBG (1, "sanei_usb_open: open of `%s' failed: %s\n",
	       devname, strerror (errno));
	  return status;
	}
    }
  else
    {
      DBG (1, "sanei_usb_open: access method %d not implemented\n", devices[devcount].method);
      return SANE_STATUS_INVAL;
    }

  devices[devcount].open = SANE_TRUE;
  *dn = devcount;
  DBG (3, "sanei_usb_open: opened usb device `%s' (*dn=%d)\n",
       devname, devcount);
  return SANE_STATUS_GOOD;
}

void
sanei_usb_close (SANE_Int dn)
{
  DBG (5, "sanei_usb_close: closing device %d\n", dn);
  if (dn >= MAX_DEVICES || dn < 0)
    {
      DBG (1, "sanei_usb_close: dn >= MAX_DEVICES || dn < 0\n");
      return;
    }
  if (!devices[dn].open)
    {
      DBG (1, "sanei_usb_close: device %d already closed or never opened\n",
	   dn);
      return;
    }
  if (devices[dn].method == sanei_usb_method_scanner_driver)
    close (devices[dn].fd);
  else
#ifdef HAVE_LIBUSB
    {
#if 0
      /* Should only be done in case of a stall */
      usb_clear_halt (devices[dn].libusb_handle, devices[dn].bulk_in_ep);
      usb_clear_halt (devices[dn].libusb_handle, devices[dn].bulk_out_ep);
      /* be careful, we don't know if we are in DATA0 stage now */
      usb_resetep(devices[dn].libusb_handle, devices[dn].bulk_in_ep);
      usb_resetep(devices[dn].libusb_handle, devices[dn].bulk_out_ep);
#endif
      usb_release_interface (devices[dn].libusb_handle, 
			     devices[dn].interface_nr);
      usb_close (devices[dn].libusb_handle);
    }
#else
    DBG (1, "sanei_usb_close: libusb support missing\n");
#endif
  devices[dn].open = SANE_FALSE;
  return;
}

SANE_Status
sanei_usb_read_bulk (SANE_Int dn, SANE_Byte * buffer, size_t * size)
{
  ssize_t read_size = 0;

  if (!size)
    {
      DBG (1, "sanei_usb_read_bulk: size == NULL\n");
      return SANE_STATUS_INVAL;
    }

  if (dn >= MAX_DEVICES || dn < 0)
    {
      DBG (1, "sanei_usb_read_bulk: dn >= MAX_DEVICES || dn < 0\n");
      return SANE_STATUS_INVAL;
    }
  if (devices[dn].method == sanei_usb_method_scanner_driver)
    read_size = read (devices[dn].fd, buffer, *size);
  else if (devices[dn].method == sanei_usb_method_libusb)
#ifdef HAVE_LIBUSB
    {
      if (devices[dn].bulk_in_ep)
	read_size = usb_bulk_read (devices[dn].libusb_handle,
				   devices[dn].bulk_in_ep, (char *) buffer,
				   (int) *size, libusb_timeout);
      else
	{
	  DBG (1, "sanei_usb_read_bulk: can't read without a bulk-in "
	       "endpoint\n");
	  return SANE_STATUS_INVAL;
	}
    }
#else /* not HAVE_LIBUSB */
    {
      DBG (1, "sanei_usb_read_bulk: libusb support missing\n");
      return SANE_STATUS_UNSUPPORTED;
    }
#endif /* not HAVE_LIBUSB */
  else
    {
      DBG (1, "sanei_usb_read_bulk: access method %d not implemented\n",
	   devices[dn].method);
      return SANE_STATUS_INVAL;
    }

  if (read_size < 0)
    {
      DBG (1, "sanei_usb_read_bulk: read failed: %s\n", strerror (errno));
#ifdef HAVE_LIBUSB
      if (devices[dn].method == sanei_usb_method_libusb)
	usb_clear_halt (devices[dn].libusb_handle, devices[dn].bulk_in_ep);
#endif
      *size = 0;
      return SANE_STATUS_IO_ERROR;
    }
  if (read_size == 0)
    {
      DBG (3, "sanei_usb_read_bulk: read returned EOF\n");
      *size = 0;
      return SANE_STATUS_EOF;
    }
  DBG (5, "sanei_usb_read_bulk: wanted %lu bytes, got %ld bytes\n",
       (unsigned long) *size, (unsigned long) read_size);
  *size = read_size;
  return SANE_STATUS_GOOD;
}

SANE_Status
sanei_usb_write_bulk (SANE_Int dn, const SANE_Byte * buffer, size_t * size)
{
  ssize_t write_size = 0;

  if (!size)
    {
      DBG (1, "sanei_usb_write_bulk: size == NULL\n");
      return SANE_STATUS_INVAL;
    }

  if (dn >= MAX_DEVICES || dn < 0)
    {
      DBG (1, "sanei_usb_write_bulk: dn >= MAX_DEVICES || dn < 0\n");
      return SANE_STATUS_INVAL;
    }

  if (devices[dn].method == sanei_usb_method_scanner_driver)
    write_size = write (devices[dn].fd, buffer, *size);
  else if (devices[dn].method == sanei_usb_method_libusb)
#ifdef HAVE_LIBUSB
    {
      if (devices[dn].bulk_out_ep)
	write_size = usb_bulk_write (devices[dn].libusb_handle,
				     devices[dn].bulk_out_ep,
				     (const char *) buffer,
				     (int) *size, libusb_timeout);
      else
	{
	  DBG (1, "sanei_usb_write_bulk: can't write without a bulk-out "
	       "endpoint\n");
	  return SANE_STATUS_INVAL;
	}
    }
#else /* not HAVE_LIBUSB */
    {
      DBG (1, "sanei_usb_write_bulk: libusb support missing\n");
      return SANE_STATUS_UNSUPPORTED;
    }
#endif /* not HAVE_LIBUSB */
  else
    {
      DBG (1, "sanei_usb_read_bulk: access method %d not implemented\n",
	   devices[dn].method);
      return SANE_STATUS_INVAL;
    }

  if (write_size < 0)
    {
      DBG (1, "sanei_usb_write_bulk: write failed: %s\n", strerror (errno));
      *size = 0;
#ifdef HAVE_LIBUSB
      if (devices[dn].method == sanei_usb_method_libusb)
	usb_clear_halt (devices[dn].libusb_handle, devices[dn].bulk_out_ep);
#endif
      return SANE_STATUS_IO_ERROR;
    }
  DBG (5, "sanei_usb_write_bulk: wanted %lu bytes, wrote %ld bytes\n",
       (unsigned long) *size, (unsigned long) write_size);
  *size = write_size;
  return SANE_STATUS_GOOD;
}

SANE_Status
sanei_usb_control_msg (SANE_Int dn, SANE_Int rtype, SANE_Int req,
		       SANE_Int value, SANE_Int index, SANE_Int len,
		       SANE_Byte * data)
{
  if (dn >= MAX_DEVICES || dn < 0)
    {
      DBG (1, "sanei_usb_control_msg: dn >= MAX_DEVICES || dn < 0\n");
      return SANE_STATUS_INVAL;
    }

  if (devices[dn].method == sanei_usb_method_scanner_driver)
    {
#if defined(__linux__)
      struct ctrlmsg_ioctl c;

      c.req.requesttype = rtype;
      c.req.request = req;
      c.req.value = value;
      c.req.index = index;
      c.req.length = len;
      c.data = data;

      DBG (5, "sanei_usb_control_msg: rtype = 0x%02x, req = %d, value = %d, "
	   "index = %d, len = %d\n", rtype, req, value, index, len);

      if (ioctl (devices[dn].fd, SCANNER_IOCTL_CTRLMSG, &c) < 0)
	{
	  DBG (5, "sanei_usb_control_msg: SCANNER_IOCTL_CTRLMSG error - %s\n",
	       strerror (errno));
	  return SANE_STATUS_IO_ERROR;
	}
      return SANE_STATUS_GOOD;
#else /* not __linux__ */
      DBG (5, "sanei_usb_control_msg: not supported on this OS\n");
      return SANE_STATUS_UNSUPPORTED;
#endif /* not __linux__ */
    }
  else if (devices[dn].method == sanei_usb_method_libusb)
#ifdef HAVE_LIBUSB
    {
      int result;

      result = usb_control_msg (devices[dn].libusb_handle, rtype, req,
				value, index, (char *) data, len,
				libusb_timeout);
      if (result < 0)
	{
	  DBG (1, "sanei_usb_control_msg: libusb complained: %s\n",
	       usb_strerror ());
	  return SANE_STATUS_INVAL;
	}
      return SANE_STATUS_GOOD;
    }
#else /* not HAVE_LIBUSB */
    {
      DBG (1, "sanei_usb_control_msg: libusb support missing\n");
      return SANE_STATUS_UNSUPPORTED;
    }
#endif /* not HAVE_LIBUSB */
  else
    {
      DBG (1, "sanei_usb_read_bulk: access method %d not implemented\n",
	   devices[dn].method);
      return SANE_STATUS_UNSUPPORTED;
    }
}

SANE_Status
sanei_usb_read_int (SANE_Int dn, SANE_Byte * buffer, size_t * size)
{
  ssize_t read_size = 0;

  if (!size)
    {
      DBG (1, "sanei_usb_read_int: size == NULL\n");
      return SANE_STATUS_INVAL;
    }

  if (dn >= MAX_DEVICES || dn < 0)
    {
      DBG (1, "sanei_usb_read_int: dn >= MAX_DEVICES || dn < 0\n");
      return SANE_STATUS_INVAL;
    }
  if (devices[dn].method == sanei_usb_method_scanner_driver) {
     DBG (1, "sanei_usb_read_int: access method %d not implemented\n",
	   devices[dn].method);
     return SANE_STATUS_INVAL;
  }
  else if (devices[dn].method == sanei_usb_method_libusb)
#ifdef HAVE_LIBUSB
    {
      if (devices[dn].int_in_ep)
	read_size = usb_bulk_read (devices[dn].libusb_handle,
				   devices[dn].int_in_ep, (char *) buffer,
				   (int) *size, libusb_timeout);
      else
	{
	  DBG (1, "sanei_usb_read_int: can't read without an int "
	       "endpoint\n");
	  return SANE_STATUS_INVAL;
	}
    }
#else /* not HAVE_LIBUSB */
    {
      DBG (1, "sanei_usb_read_int: libusb support missing\n");
      return SANE_STATUS_UNSUPPORTED;
    }
#endif /* not HAVE_LIBUSB */
  else
    {
      DBG (1, "sanei_usb_read_int: access method %d not implemented\n",
	   devices[dn].method);
      return SANE_STATUS_INVAL;
    }

  if (read_size < 0)
    {
      DBG (1, "sanei_usb_read_int: read failed: %s\n", strerror (errno));
#ifdef HAVE_LIBUSB
      if (devices[dn].method == sanei_usb_method_libusb)
	usb_clear_halt (devices[dn].libusb_handle, devices[dn].int_in_ep);
#endif
      *size = 0;
      return SANE_STATUS_IO_ERROR;
    }
  if (read_size == 0)
    {
      DBG (3, "sanei_usb_read_int: read returned EOF\n");
      *size = 0;
      return SANE_STATUS_EOF;
    }
  DBG (5, "sanei_usb_read_int: wanted %lu bytes, got %ld bytes\n",
       (unsigned long) *size, (unsigned long) read_size);
  *size = read_size;
  return SANE_STATUS_GOOD;
}
