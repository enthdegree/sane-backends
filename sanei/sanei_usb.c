/* sane - Scanner Access Now Easy.
   Copyright (C) 2001 Henning Meier-Geinitz
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

   This file provides a generic (?) USB interface.  */

#include "../include/sane/config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdio.h>

#define BACKEND_NAME	sanei_usb
#include "../include/sane/sane.h"
#include "../include/sane/sanei_debug.h"
#include "../include/sane/sanei_usb.h"


void
sanei_usb_init (void)
{
  DBG_INIT();
}

SANE_Status
sanei_usb_get_vendor_product (SANE_Int fd, SANE_Word * vendor,
			      SANE_Word * product)
{
  SANE_Word vendorID, productID;

#if defined (__linux__)
#define IOCTL_SCANNER_VENDOR _IOR('u', 0xa0, int)
#define IOCTL_SCANNER_PRODUCT _IOR('u', 0xa1, int)
  /* read the vendor and product IDs via the IOCTLs */
  if (ioctl (fd, IOCTL_SCANNER_VENDOR , &vendorID) == -1)
    {
      DBG (3, "sanei_usb_get_vendor_product: ioctl (vendor) of fd %d failed: "
	   "%s\n", fd, strerror (errno));
      /* just set the vendor ID to 0 */
      vendorID = 0;
    }
  if (ioctl (fd, IOCTL_SCANNER_PRODUCT , &productID) == -1)
    {
      DBG (3, "sanei_usb_get_vendor_product: ioctl (product) of ds %d failed: "
	   "%s\n", fd, strerror (errno));
      /* just set the product ID to 0 */
      productID = 0;
    }
  if (vendor)
    *vendor = vendorID;
  if (product)
    *product = productID;
#else /* not defined (__linux__) */
  vendorID = 0;
  productID = 0;
#endif /* not defined (__linux__) */

  if (!vendorID || !productID)
    {
      DBG (3, "sanei_usb_get_vendor_product: fd %d: couldn't get "
	   "vendor+product ids. ioctl unsupported?\n", fd);

      return SANE_STATUS_UNSUPPORTED;
    }
  else
    {
      DBG (3, "sanei_usb_get_vendor_product: fd %d: vendorID: 0x%x, "
	   "productID: 0x%x\n", fd, vendorID, productID);
      return SANE_STATUS_GOOD;
    }
}


static void
check_vendor_product (SANE_String_Const devname, SANE_Word vendor,
		      SANE_Word product,
		      SANE_Status (*attach) (SANE_String_Const dev))
{
  SANE_Status status;
  SANE_Word devvendor, devproduct;
  SANE_Int fd;

  status = sanei_usb_open (devname, &fd);
  if (status != SANE_STATUS_GOOD)
    return;

  status = sanei_usb_get_vendor_product (fd, &devvendor, &devproduct);
  sanei_usb_close (fd);
  if (status == SANE_STATUS_GOOD)
    {
      if (devvendor == vendor && devproduct == product)
	{
	  if (attach)
	    attach (devname);
	}
    }
  return;
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
    0};
  SANE_Char devname[30];
  int devcount;

#define SANEI_USB_MAX_DEVICES 16

  DBG (3, "sanei_usb_find_devices: vendor=0x%x, product=0x%x, attach=%p\n",
       vendor, product, attach);

  for (prefix = prefixlist; *prefix; prefix++)
    {
      check_vendor_product (*prefix, vendor, product, attach);
      for (devcount = 0; devcount < SANEI_USB_MAX_DEVICES; 
	   devcount++)
	{
	  snprintf (devname, sizeof (devname), "%s%d", *prefix,
		    devcount);
	  check_vendor_product (devname, vendor, product, attach);
	}
    }
  return SANE_STATUS_GOOD;
}

SANE_Status
sanei_usb_open (SANE_String_Const devname, SANE_Int *fd)
{
  if (!fd)
    {
      DBG (1, "sanei_usb_open: fd == NULL\n", fd);
      return SANE_STATUS_INVAL;
    }
  *fd = open (devname, O_RDWR);
  if (*fd < 0)
    {
      SANE_Status status = SANE_STATUS_INVAL;

      DBG (1, "sanei_usb_open: open failed: %s\n", strerror (errno));
      if (errno == EACCES)
	status = SANE_STATUS_ACCESS_DENIED;
      return status;
    }
  DBG (5, "sanei_usb_open: fd %d opened\n", *fd);
  return SANE_STATUS_GOOD;
}

void
sanei_usb_close (SANE_Int fd)
{
  DBG (5, "sanei_usb_close: closing fd %d\n", fd);
  close (fd);
  return;
}

SANE_Status
sanei_usb_read_bulk (SANE_Int fd, SANE_Byte * buffer, size_t *size)
{
  ssize_t read_size;

  if (!size)
    {
      DBG (1, "sanei_usb_read_bulk: size == NULL\n");
      return SANE_STATUS_INVAL;
    }

  read_size = read (fd, buffer, *size);
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
sanei_usb_write_bulk (SANE_Int fd, SANE_Byte * buffer, size_t *size)
{
  ssize_t write_size;

  if (!size)
    {
      DBG (1, "sanei_usb_write_bulk: size == NULL\n");
      return SANE_STATUS_INVAL;
    }

  write_size = write (fd, buffer, *size);
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
