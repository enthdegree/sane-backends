/* sane - Scanner Access Now Easy.
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
  SANE_Int bulk_in_ep;
  SANE_Int bulk_out_ep;
#ifdef HAVE_LIBUSB
  usb_dev_handle *libusb_handle;
#endif				/* HAVE_LIBUSB */
}
device_list_type;

#define MAX_DEVICES 100

/* per-device information, using the functions' parameters dn as index */
static device_list_type devices[MAX_DEVICES];

#ifdef HAVE_LIBUSB
int libusb_timeout = 30 * 1000;	/* 30 seconds */
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


void
sanei_usb_init (void)
{
  DBG_INIT ();

  memset (devices, 0, sizeof (devices));

#ifdef HAVE_LIBUSB
  usb_init ();
#ifdef DBG_LEVEL
  if (DBG_LEVEL > 4)
    usb_set_debug (255);
#endif /* DBG_LEVEL */
  usb_find_busses ();
  usb_find_devices ();
#endif /* HAVE_LIBUSB */
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
    {
#if defined (__linux__)
      /* read the vendor and product IDs via the IOCTLs */
      if (ioctl (devices[dn].fd, SCANNER_IOCTL_VENDOR, &vendorID) == -1)
	{
	  if (ioctl (devices[dn].fd, SCANNER_IOCTL_VENDOR_OLD, &vendorID)
	      == -1)
	    DBG (3, "sanei_usb_get_vendor_product: ioctl (vendor) "
		 "of device %d failed: %s\n", dn, strerror (errno));
	}
      if (ioctl (devices[dn].fd, SCANNER_IOCTL_PRODUCT, &productID) == -1)
	{
	  if (ioctl (devices[dn].fd, SCANNER_IOCTL_PRODUCT_OLD,
		     &productID) == -1)
	    DBG (3, "sanei_usb_get_vendor_product: ioctl (product) "
		 "of device %d failed: %s\n", dn, strerror (errno));
	}
#endif /* defined (__linux__) */
      /* put more os-dependant stuff ... */
    }
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


static SANE_Status
check_vendor_product (SANE_String_Const devname, SANE_Word vendor,
		      SANE_Word product,
		      SANE_Status (*attach) (SANE_String_Const dev))
{
  SANE_Status status;
  SANE_Word devvendor, devproduct;
  SANE_Int dn;

  status = sanei_usb_open (devname, &dn);
  if (status != SANE_STATUS_GOOD)
    return status;

  status = sanei_usb_get_vendor_product (dn, &devvendor, &devproduct);
  sanei_usb_close (dn);
  if (status == SANE_STATUS_GOOD)
    {
      if (devvendor == vendor && devproduct == product)
	{
	  if (attach)
	    attach (devname);
	}
    }
  return status;
}

SANE_Status
sanei_usb_find_devices (SANE_Int vendor, SANE_Int product,
			SANE_Status (*attach) (SANE_String_Const dev))
{
  SANE_String *prefix;
  SANE_String prefixlist[] = {
#if defined(__linux__)
    "/dev/usbscanner",
    "/dev/usb/scanner",
#elif defined(__FreeBSD__)
    "/dev/uscanner",
#endif
    0
  };
  SANE_Char devname[1024];
  int devcount;
  SANE_Status status;

#define SANEI_USB_MAX_DEVICE_FILES 16

  DBG (3,
       "sanei_usb_find_devices: vendor=0x%04x, product=0x%04x, attach=%p\n",
       vendor, product, attach);

  /* First we check the device files for access over the kernel scanner
     drivers */
  for (prefix = prefixlist; *prefix; prefix++)
    {
      status = check_vendor_product (*prefix, vendor, product, attach);
      if (status == SANE_STATUS_UNSUPPORTED)
	break;			/* give up if we can't detect vendor/product */
      for (devcount = 0; devcount < SANEI_USB_MAX_DEVICE_FILES; devcount++)
	{
	  snprintf (devname, sizeof (devname), "%s%d", *prefix, devcount);
	  status = check_vendor_product (devname, vendor, product, attach);
	  if (status == SANE_STATUS_UNSUPPORTED)
	    break;		/* give up if we can't detect vendor/product */
	}
    }
  /* Now we will check all the devices libusb finds (if available) */
#ifdef HAVE_LIBUSB
  {
    struct usb_bus *bus;
    struct usb_device *dev;

    for (bus = usb_get_busses (); bus; bus = bus->next)
      {
	for (dev = bus->devices; dev; dev = dev->next)
	  {
	    if (dev->descriptor.idVendor == vendor
		&& dev->descriptor.idProduct == product)
	      {
		DBG (3,
		     "sanei_usb_find_devices: found matching libusb device "
		     "(bus=%s, filename=%s, vendor=0x%04x, product=0x%04x)\n",
		     bus->dirname, dev->filename, dev->descriptor.idVendor,
		     dev->descriptor.idProduct);
		snprintf (devname, sizeof (devname), "libusb:%s:%s",
			  bus->dirname, dev->filename);
		attach (devname);
	      }
	    else
	      DBG (5, "sanei_usb_find_devices: found libusb device "
		   "(bus=%s, filename=%s, vendor=0x%04x, product=0x%04x)\n",
		   bus->dirname, dev->filename, dev->descriptor.idVendor,
		   dev->descriptor.idProduct);
	  }
      }
  }
#endif /* HAVE_LIBUSB */
  return SANE_STATUS_GOOD;
}

SANE_Status
sanei_usb_open (SANE_String_Const devname, SANE_Int * dn)
{
  int devcount;

  if (!dn)
    {
      DBG (1, "sanei_usb_open: can't open `%s': dn == NULL\n", devname);
      return SANE_STATUS_INVAL;
    }

  for (devcount = 0;
       devcount < MAX_DEVICES && devices[devcount].open == SANE_TRUE;
       devcount++)
    ;

  if (devices[devcount].open)
    {
      DBG (1, "sanei_usb_open: can't open `%s': "
	   "maximum amount of devices (%d) reached\n", devname, MAX_DEVICES);
      return SANE_STATUS_NO_MEM;
    }

  memset (&devices[devcount], 0, sizeof (devices[devcount]));

  if (strncmp (devname, "libusb:", 7) == 0)
    {
      /* Using libusb */
#ifdef HAVE_LIBUSB
      SANE_String start, end, bus_string, dev_string;
      struct usb_bus *bus;
      struct usb_device *dev;
      struct usb_interface_descriptor *interface;
      SANE_Int result;
      int num;


      DBG (5, "sanei_usb_open: trying to open device `%s'\n", devname);
      start = strchr (devname, ':');
      if (!start)
	return SANE_STATUS_INVAL;
      start++;
      end = strchr (start, ':');
      if (!end)
	return SANE_STATUS_INVAL;
      bus_string = strndup (start, end - start);
      if (!bus_string)
	return SANE_STATUS_NO_MEM;

      start = strchr (start, ':');
      if (!start)
	return SANE_STATUS_INVAL;
      start++;
      dev_string = strdup (start);
      if (!dev_string)
	return SANE_STATUS_NO_MEM;

      for (bus = usb_get_busses (); bus; bus = bus->next)
	{
	  if (strcmp (bus->dirname, bus_string) == 0)
	    {
	      for (dev = bus->devices; dev; dev = dev->next)
		{
		  if (strcmp (dev->filename, dev_string) == 0)
		    devices[devcount].libusb_handle = usb_open (dev);
		}
	    }
	}

      if (!devices[devcount].libusb_handle)
	{
	  DBG (1, "sanei_usb_open: can't open device `%s': "
	       "not found or libusb error (%s)\n", devname, strerror (errno));
	  return SANE_STATUS_INVAL;
	}
      devices[devcount].method = sanei_usb_method_libusb;

      dev = usb_device (devices[devcount].libusb_handle);

      if (dev->descriptor.bNumConfigurations > 1)
	{
	  DBG (3, "sanei_usb_open: more than one "
	       "configuration (%d), choosing config 0\n",
	       dev->descriptor.bNumConfigurations);
	}

      if (dev->config[0].interface->num_altsetting > 1)
	{
	  DBG (3, "sanei_usb_open: more than one "
	       "alternate setting (%d), choosing interface 0\n",
	       dev->config[0].interface->num_altsetting);
	}

      result = usb_claim_interface (devices[devcount].libusb_handle, 0);
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

      /* Now we look for usable endpoints */
      for (num = 0; num < interface->bNumEndpoints; num++)
	{
	  struct usb_endpoint_descriptor *endpoint;
	  int address, direction, transfer_type;

	  endpoint = &interface->endpoint[num];
	  address = endpoint->bEndpointAddress & USB_ENDPOINT_ADDRESS_MASK;
	  direction = endpoint->bEndpointAddress & USB_ENDPOINT_DIR_MASK;
	  transfer_type = endpoint->bmAttributes & USB_ENDPOINT_TYPE_MASK;

	  if (transfer_type != USB_ENDPOINT_TYPE_BULK)
	    {
	      DBG (5, "sanei_usb_open: ignoring %s-%s endpoint "
		   "(address: %d)\n",
		   transfer_type == USB_ENDPOINT_TYPE_CONTROL ? "control" :
		   transfer_type == USB_ENDPOINT_TYPE_ISOCHRONOUS
		   ? "isochronous" : "interrupt",
		   direction ? "in" : "out", address);
	      continue;
	    }
	  DBG (5, "sanei_usb_open: found bulk-%s endpoint (address %d)\n",
	       direction ? "in" : "out", address);
	  if (direction)	/* in */
	    {
	      if (devices[devcount].bulk_in_ep)
		DBG (3, "sanei_usb_open: we already have a bulk-in endpoint "
		     "(address: %d), ignoring the new one\n",
		     devices[devcount].bulk_in_ep);
	      else
		devices[devcount].bulk_in_ep = address;
	    }
	  else
	    {
	      if (devices[devcount].bulk_out_ep)
		DBG (3, "sanei_usb_open: we already have a bulk-out endpoint "
		     "(address: %d), ignoring the new one\n",
		     devices[devcount].bulk_out_ep);
	      else
		devices[devcount].bulk_out_ep = address;
	    }
	}
#else /* not HAVE_LIBUSB */
      DBG (1, "sanei_usb_open: can't open device `%s': "
	   "libusb support missing\n", devname);
      return SANE_STATUS_UNSUPPORTED;
#endif /* not HAVE_LIBUSB */
    }
  else
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
      devices[devcount].method = sanei_usb_method_scanner_driver;
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
  if (devices[dn].method == sanei_usb_method_scanner_driver)
    close (devices[dn].fd);
  else
#ifdef HAVE_LIBUSB
    usb_close (devices[dn].libusb_handle);
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
sanei_usb_write_bulk (SANE_Int dn, SANE_Byte * buffer, size_t * size)
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
      if (devices[dn].bulk_in_ep)
	write_size = usb_bulk_write (devices[dn].libusb_handle,
				     devices[dn].bulk_out_ep, (char *) buffer,
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
