/*
   check-usb-chip.c -- Find out what USB scanner chipset is used
   
   Copyright (C) 2003 Henning Meier-Geinitz <henning@meier-geinitz.de>
   Copyright (C) 2003 Gerhard Jäger <gerhard@gjaeger.de>
                      for LM983x tests

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
*/


#include "../include/sane/config.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>

static int verbose = 0;

#define TIMEOUT 1000

#ifdef HAVE_LIBUSB
#include <usb.h>

extern char *check_usb_chip (struct usb_device *dev, int verbosity);


static int
prepare_interface (struct usb_device *dev, usb_dev_handle ** handle)
{
  int result;

  *handle = usb_open (dev);
  if (*handle == 0)
    {
      if (verbose > 1)
	printf ("    Couldn't open device: %s\n", usb_strerror ());
      return 0;
    }

  result =
    usb_set_configuration (*handle, dev->config[0].bConfigurationValue);
  if (result < 0)
    {
      if (verbose > 1)
	printf ("  Couldn't set configuration: %s\n", usb_strerror ());
      usb_close (*handle);
      return 0;
    }

  result = usb_claim_interface (*handle, 0);
  if (result < 0)
    {
      if (verbose > 1)
	printf ("    Couldn't claim interface: %s\n", usb_strerror ());
      usb_close (*handle);
      return 0;
    }
  return 1;
}

static void
finish_interface (usb_dev_handle * handle)
{
  usb_release_interface (handle, 0);
  usb_close (handle);
}

/* Check for Grandtech GT-6801 */
static char *
check_gt6801 (struct usb_device *dev)
{
  char req[64];
  usb_dev_handle *handle;
  int result;

  if (verbose > 2)
    printf ("    checking for GT-6801 ...\n");

  /* Check device descriptor */
  if (dev->descriptor.bDeviceClass != USB_CLASS_VENDOR_SPEC)
    {
      if (verbose > 2)
	printf ("    this is not a GT-6801 (bDeviceClass = %d)\n",
		dev->descriptor.bDeviceClass);
      return 0;
    }
  if (dev->descriptor.bcdUSB != 0x110)
    {
      if (verbose > 2)
	printf ("    this is not a GT-6801 (bcdUSB = 0x%x)\n",
		dev->descriptor.bcdUSB);
      return 0;
    }
  if (dev->descriptor.bDeviceSubClass != 0xff)
    {
      if (verbose > 2)
	printf ("    this is not a GT-6801 (bDeviceSubClass = 0x%x)\n",
		dev->descriptor.bDeviceSubClass);
      return 0;
    }
  if (dev->descriptor.bDeviceProtocol != 0xff)
    {
      if (verbose > 2)
	printf ("    this is not a GT-6801 (bDeviceProtocol = 0x%x)\n",
		dev->descriptor.bDeviceProtocol);
      return 0;
    }

  /* Check endpoints */
  if (dev->config[0].interface[0].altsetting[0].bNumEndpoints != 1)
    {
      if (verbose > 2)
	printf ("    this is not a GT-6801 (bNumEndpoints = %d)\n",
		dev->config[0].interface[0].altsetting[0].bNumEndpoints);
      return 0;
    }
  if ((dev->config[0].interface[0].altsetting[0].endpoint[0].
       bEndpointAddress != 0x81)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].
	  bmAttributes != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].
	  wMaxPacketSize != 0x40)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].bInterval !=
	  0x00))
    {
      if (verbose > 2)
	printf
	  ("    this is not a GT-6801 (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[0].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].bInterval);
      return 0;
    }

  /* Now we send a control message */
  result = prepare_interface (dev, &handle);
  if (!result)
    return "GT-6801?";

  memset (req, 0, 64);
  req[0] = 0x2e;		/* get identification information */
  req[1] = 0x01;

  result =
    usb_control_msg (handle, 0x40, 0x01, 0x2010, 0x3f40, req, 64, TIMEOUT);
  if (result <= 0)
    {
      if (verbose > 2)
	printf ("    Couldn't send write control message (%s)\n",
		strerror (errno));
      finish_interface (handle);
      return 0;
    }
  result =
    usb_control_msg (handle, 0xc0, 0x01, 0x2011, 0x3f00, req, 64, TIMEOUT);
  if (result <= 0)
    {
      if (verbose > 2)
	printf ("    Couldn't send read control message (%s)\n",
		strerror (errno));
      finish_interface (handle);
      return 0;
    }
  if (req[0] != 0 || (req[1] != 0x2e && req[1] != 0))
    {
      if (verbose > 2)
	printf ("    Unexpected result from control message (%0x/%0x)\n",
		req[0], req[1]);
      finish_interface (handle);
      return 0;
    }
  finish_interface (handle);
  return "GT-6801";
}

/* Check for Grandtech GT-6816 */
static char *
check_gt6816 (struct usb_device *dev)
{
  char req[64];
  usb_dev_handle *handle;
  int result;
  int i;

  if (verbose > 2)
    printf ("    checking for GT-6816 ...\n");

  /* Check device descriptor */
  if ((dev->descriptor.bDeviceClass != USB_CLASS_PER_INTERFACE)
      || (dev->config[0].interface[0].altsetting[0].bInterfaceClass !=
	  USB_CLASS_VENDOR_SPEC))
    {
      if (verbose > 2)
	printf
	  ("    this is not a GT-6816 (bDeviceClass = %d, bInterfaceClass = %d)\n",
	   dev->descriptor.bDeviceClass,
	   dev->config[0].interface[0].altsetting[0].bInterfaceClass);
      return 0;
    }
  if (dev->descriptor.bcdUSB != 0x110)
    {
      if (verbose > 2)
	printf ("    this is not a GT-6816 (bcdUSB = 0x%x)\n",
		dev->descriptor.bcdUSB);
      return 0;
    }
  if (dev->descriptor.bDeviceSubClass != 0x00)
    {
      if (verbose > 2)
	printf ("    this is not a GT-6816 (bDeviceSubClass = 0x%x)\n",
		dev->descriptor.bDeviceSubClass);
      return 0;
    }
  if (dev->descriptor.bDeviceProtocol != 0x00)
    {
      if (verbose > 2)
	printf ("    this is not a GT-6816 (bDeviceProtocol = 0x%x)\n",
		dev->descriptor.bDeviceProtocol);
      return 0;
    }

  /* Check endpoints */
  if (dev->config[0].interface[0].altsetting[0].bNumEndpoints != 2)
    {
      if (verbose > 2)
	printf ("    this is not a GT-6816 (bNumEndpoints = %d)\n",
		dev->config[0].interface[0].altsetting[0].bNumEndpoints);
      return 0;
    }
  if ((dev->config[0].interface[0].altsetting[0].endpoint[0].
       bEndpointAddress != 0x81)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].
	  bmAttributes != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].
	  wMaxPacketSize != 0x40)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].bInterval !=
	  0x00))
    {
      if (verbose > 2)
	printf
	  ("    this is not a GT-6816 (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[0].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].bInterval);
      return 0;
    }
  if ((dev->config[0].interface[0].altsetting[0].endpoint[1].
       bEndpointAddress != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].
	  bmAttributes != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].
	  wMaxPacketSize != 0x40)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].bInterval !=
	  0x00))
    {
      if (verbose > 2)
	printf
	  ("    this is not a GT-6816 (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[1].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].bInterval);
      return 0;

    }

  /* Now we send a control message */
  result = prepare_interface (dev, &handle);
  if (!result)
    return "GT-6816?";

  memset (req, 0, 64);
  for (i = 0; i < 8; i++)
    {
      req[8 * i + 0] = 0x73;	/* check firmware */
      req[8 * i + 1] = 0x01;
    }

  result =
    usb_control_msg (handle, 0x40, 0x04, 0x2012, 0x3f40, req, 64, TIMEOUT);
  if (result <= 0)
    {
      if (verbose > 2)
	printf ("    Couldn't send write control message (%s)\n",
		strerror (errno));
      finish_interface (handle);
      return 0;
    }
  result =
    usb_control_msg (handle, 0xc0, 0x01, 0x2013, 0x3f00, req, 64, TIMEOUT);
  if (result <= 0)
    {
      if (verbose > 2)
	printf ("    Couldn't send read control message (%s)\n",
		strerror (errno));
      finish_interface (handle);
      return 0;
    }

  if (req[0] != 0)
    {
      if (verbose > 2)
	printf ("    Unexpected result from control message (%0x/%0x)\n",
		req[0], req[1]);
      finish_interface (handle);
      return 0;
    }
  finish_interface (handle);
  return "GT-6816";
}

/* Check for Mustek MA-1017 */
static char *
check_ma1017 (struct usb_device *dev)
{
  char req[2];
  usb_dev_handle *handle;
  int result;
  char res;

  if (verbose > 2)
    printf ("    checking for MA-1017 ...\n");

  /* Check device descriptor */
  if ((dev->descriptor.bDeviceClass != USB_CLASS_PER_INTERFACE)
      || (dev->config[0].interface[0].altsetting[0].bInterfaceClass !=
	  USB_CLASS_PER_INTERFACE))
    {
      if (verbose > 2)
	printf
	  ("    this is not a MA-1017 (bDeviceClass = %d, bInterfaceClass = %d)\n",
	   dev->descriptor.bDeviceClass,
	   dev->config[0].interface[0].altsetting[0].bInterfaceClass);
      return 0;
    }
  if (dev->descriptor.bcdUSB != 0x100)
    {
      if (verbose > 2)
	printf ("    this is not a MA-1017 (bcdUSB = 0x%x)\n",
		dev->descriptor.bcdUSB);
      return 0;
    }
  if (dev->descriptor.bDeviceSubClass != 0x00)
    {
      if (verbose > 2)
	printf ("    this is not a MA-1017 (bDeviceSubClass = 0x%x)\n",
		dev->descriptor.bDeviceSubClass);
      return 0;
    }
  if (dev->descriptor.bDeviceProtocol != 0x00)
    {
      if (verbose > 2)
	printf ("    this is not a MA-1017 (bDeviceProtocol = 0x%x)\n",
		dev->descriptor.bDeviceProtocol);
      return 0;
    }

  /* Check endpoints */
  if (dev->config[0].interface[0].altsetting[0].bNumEndpoints != 3)
    {
      if (verbose > 2)
	printf ("    this is not a MA-1017 (bNumEndpoints = %d)\n",
		dev->config[0].interface[0].altsetting[0].bNumEndpoints);
      return 0;
    }
  if ((dev->config[0].interface[0].altsetting[0].endpoint[0].
       bEndpointAddress != 0x01)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].
	  bmAttributes != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].
	  wMaxPacketSize != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].bInterval !=
	  0x00))
    {
      if (verbose > 2)
	printf
	  ("    this is not a MA-1017 (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[0].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].bInterval);
      return 0;
    }
  if ((dev->config[0].interface[0].altsetting[0].endpoint[1].
       bEndpointAddress != 0x82)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].
	  bmAttributes != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].
	  wMaxPacketSize != 0x40)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].bInterval !=
	  0x00))
    {
      if (verbose > 2)
	printf
	  ("    this is not a MA-1017 (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[1].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].bInterval);
      return 0;
    }
  if ((dev->config[0].interface[0].altsetting[0].endpoint[2].
       bEndpointAddress != 0x83)
      || (dev->config[0].interface[0].altsetting[0].endpoint[2].
	  bmAttributes != 0x03)
      || (dev->config[0].interface[0].altsetting[0].endpoint[2].
	  wMaxPacketSize != 0x01)
      || (dev->config[0].interface[0].altsetting[0].endpoint[2].bInterval !=
	  0x01))
    {
      if (verbose > 2)
	printf
	  ("    this is not a MA-1017 (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[2].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[2].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[2].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[2].bInterval);
      return 0;
    }

  /* read a register value */
  result = prepare_interface (dev, &handle);
  if (!result)
    return "MA-1017?";

  req[0] = 0x00;
  req[1] = 0x02 | 0x20;
  result = usb_bulk_write (handle, 0x01, req, 2, 1000);
  if (result <= 0)
    {
      if (verbose > 2)
	printf ("    this is not a MA-1017 (Error during bulk write)\n");
      finish_interface (handle);
      return 0;
    }
  result = usb_bulk_read (handle, 0x82, &res, 1, 1000);
  if (result <= 0)
    {
      if (verbose > 2)
	printf ("    this is not a MA-1017 (Error during bulk read)\n");
      finish_interface (handle);
      return 0;
    }
  finish_interface (handle);
  return "MA-1017";
}

/* Check for Mustek MA-1015 */
static char *
check_ma1015 (struct usb_device *dev)
{
  char req[8];
  usb_dev_handle *handle;
  int result;
  unsigned char res;

  if (verbose > 2)
    printf ("    checking for MA-1015 ...\n");

  /* Check device descriptor */
  if (dev->descriptor.bDeviceClass != USB_CLASS_VENDOR_SPEC)
    {
      if (verbose > 2)
	printf ("    this is not a MA-1015 (bDeviceClass = %d)\n",
		dev->descriptor.bDeviceClass);
      return 0;
    }
  if (dev->descriptor.bcdUSB != 0x100)
    {
      if (verbose > 2)
	printf ("    this is not a MA-1015 (bcdUSB = 0x%x)\n",
		dev->descriptor.bcdUSB);
      return 0;
    }
  if (dev->descriptor.bDeviceSubClass != 0xff)
    {
      if (verbose > 2)
	printf ("    this is not a MA-1015 (bDeviceSubClass = 0x%x)\n",
		dev->descriptor.bDeviceSubClass);
      return 0;
    }
  if (dev->descriptor.bDeviceProtocol != 0xff)
    {
      if (verbose > 2)
	printf ("    this is not a MA-1015 (bDeviceProtocol = 0x%x)\n",
		dev->descriptor.bDeviceProtocol);
      return 0;
    }

  /* Check endpoints */
  if (dev->config[0].interface[0].altsetting[0].bNumEndpoints != 2)
    {
      if (verbose > 2)
	printf ("    this is not a MA-1015 (bNumEndpoints = %d)\n",
		dev->config[0].interface[0].altsetting[0].bNumEndpoints);
      return 0;
    }
  if ((dev->config[0].interface[0].altsetting[0].endpoint[0].
       bEndpointAddress != 0x81)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].
	  bmAttributes != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].
	  wMaxPacketSize != 0x40)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].bInterval !=
	  0x00))
    {
      if (verbose > 2)
	printf
	  ("    this is not a MA-1015 (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[0].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].bInterval);
      return 0;
    }
  if ((dev->config[0].interface[0].altsetting[0].endpoint[1].
       bEndpointAddress != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].
	  bmAttributes != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].
	  wMaxPacketSize != 0x08)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].bInterval !=
	  0x00))
    {
      if (verbose > 2)
	printf
	  ("    this is not a MA-1015 (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[1].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].bInterval);
      return 0;
    }

  /* Now we read register 0 to find out if this is really a MA-1015 */
  result = prepare_interface (dev, &handle);
  if (!result)
    return 0;

  memset (req, 0, 8);
  req[0] = 33;
  req[1] = 0x00;
  result = usb_bulk_write (handle, 0x02, req, 8, TIMEOUT);
  if (result <= 0)
    {
      if (verbose > 2)
	printf ("    this is not a MA-1015 (Error during bulk write)\n");
      finish_interface (handle);
      return 0;
    }
  result = usb_bulk_read (handle, 0x81, (char *) &res, 1, TIMEOUT);
  if (result <= 0)
    {
      if (verbose > 2)
	printf ("    this is not a MA-1015 (Error during bulk read)\n");
      finish_interface (handle);
      return 0;
    }
  if (res != 0xa5)
    {
      if (verbose > 2)
	printf ("    this is not a MA-1015 (got 0x%x, expected 0xa5)\n", res);
      finish_interface (handle);
      return 0;
    }
  finish_interface (handle);
  return "MA-1015";
}

/* Check for Mustek MA-1509 */
static char *
check_ma1509 (struct usb_device *dev)
{
  char req[8];
  unsigned char inquiry[0x60];
  usb_dev_handle *handle;
  int result;

  if (verbose > 2)
    printf ("    checking for MA-1509 ...\n");

  /* Check device descriptor */
  if (dev->descriptor.bDeviceClass != USB_CLASS_VENDOR_SPEC)
    {
      if (verbose > 2)
	printf ("    this is not a MA-1509 (bDeviceClass = %d)\n",
		dev->descriptor.bDeviceClass);
      return 0;
    }
  if (dev->descriptor.bcdUSB != 0x110)
    {
      if (verbose > 2)
	printf ("    this is not a MA-1509 (bcdUSB = 0x%x)\n",
		dev->descriptor.bcdUSB);
      return 0;
    }
  if (dev->descriptor.bDeviceSubClass != 0xff)
    {
      if (verbose > 2)
	printf ("    this is not a MA-1509 (bDeviceSubClass = 0x%x)\n",
		dev->descriptor.bDeviceSubClass);
      return 0;
    }
  if (dev->descriptor.bDeviceProtocol != 0xff)
    {
      if (verbose > 2)
	printf ("    this is not a MA-1509 (bDeviceProtocol = 0x%x)\n",
		dev->descriptor.bDeviceProtocol);
      return 0;
    }

  /* Check endpoints */
  if (dev->config[0].interface[0].altsetting[0].bNumEndpoints != 3)
    {
      if (verbose > 2)
	printf ("    this is not a MA-1509 (bNumEndpoints = %d)\n",
		dev->config[0].interface[0].altsetting[0].bNumEndpoints);
      return 0;
    }
  if ((dev->config[0].interface[0].altsetting[0].endpoint[0].
       bEndpointAddress != 0x81)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].
	  bmAttributes != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].
	  wMaxPacketSize != 0x40)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].bInterval !=
	  0xff))
    {
      if (verbose > 2)
	printf
	  ("    this is not a MA-1509 (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[0].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].bInterval);
      return 0;
    }
  if ((dev->config[0].interface[0].altsetting[0].endpoint[1].
       bEndpointAddress != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].
	  bmAttributes != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].
	  wMaxPacketSize != 0x08)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].bInterval !=
	  0x00))
    {
      if (verbose > 2)
	printf
	  ("    this is not a MA-1509 (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[1].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].bInterval);
      return 0;
    }
  if ((dev->config[0].interface[0].altsetting[0].endpoint[2].
       bEndpointAddress != 0x83)
      || (dev->config[0].interface[0].altsetting[0].endpoint[2].
	  bmAttributes != 0x03)
      || (dev->config[0].interface[0].altsetting[0].endpoint[2].
	  wMaxPacketSize != 0x08)
      || (dev->config[0].interface[0].altsetting[0].endpoint[2].bInterval !=
	  0x10))
    {
      if (verbose > 2)
	printf
	  ("    this is not a MA-1509 (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[2].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[2].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[2].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[2].bInterval);
      return 0;
    }

  /* This is a SCSI-over-USB chip, we'll read the inquiry */
  result = prepare_interface (dev, &handle);
  if (!result)
    return "MA-1509?";

  memset (req, 0, 8);
  req[0] = 0x12;
  req[1] = 1;
  req[6] = 0x60;

  result = usb_bulk_write (handle, 0x02, req, 8, TIMEOUT);
  if (result <= 0)
    {
      if (verbose > 2)
	printf ("    this is not a MA-1509 (Error during bulk write)\n");
      finish_interface (handle);
      return 0;
    }
  memset (inquiry, 0, 0x60);
  result = usb_bulk_read (handle, 0x81, (char *) inquiry, 0x60, TIMEOUT);
  if (result != 0x60)
    {
      if (verbose > 2)
	printf ("    this is not a MA-1509 (Error during bulk read: %d)\n",
		result);
      finish_interface (handle);
      return 0;
    }
  if ((inquiry[0] & 0x1f) != 0x06)
    {
      if (verbose > 2)
	printf ("    this is not a MA-1509 (inquiry [0] = %d)\n", inquiry[0]);
      finish_interface (handle);
      return 0;
    }
  if (strncmp (inquiry + 8, "SCANNER ", 8) != 0)
    {
      inquiry[16] = 0;
      if (verbose > 2)
	printf ("    this is not a MA-1509 (vendor=%s)\n", inquiry + 8);
      finish_interface (handle);
      return 0;
    }
  inquiry[36] = 0;
  if (verbose > 2)
    printf ("    MA-1509 version %s\n", inquiry + 32);

  finish_interface (handle);
  return "MA-1509";
}

/********** the lm983x section **********/

static int
lm983x_wb (usb_dev_handle * handle, unsigned char reg, unsigned char val)
{
  unsigned char buf[5];
  int result;

  buf[0] = 0;
  buf[1] = reg;
  buf[2] = 0;
  buf[3] = 1;
  buf[4] = val;

  result = usb_bulk_write (handle, 3, (char *) buf, 5, TIMEOUT);
  if (result != 5)
    return 0;

  return 1;
}

static int
lm983x_rb (usb_dev_handle * handle, unsigned char reg, unsigned char *val)
{
  unsigned char buf[5];
  int result;

  buf[0] = 1;
  buf[1] = reg;
  buf[2] = 0;
  buf[3] = 1;

  result = usb_bulk_write (handle, 3, (char *) buf, 4, TIMEOUT);
  if (result != 4)
    return 0;


  result = usb_bulk_read (handle, 2, (char *) val, 1, TIMEOUT);
  if (result != 1)
    return 0;

  return 1;
}

static char *
check_merlin (struct usb_device *dev)
{
  unsigned char val;
  int result;
  usb_dev_handle *handle;

  if (verbose > 2)
    printf ("    checking for LM983[1,2,3] ...\n");

  /* Check device descriptor */
  if (((dev->descriptor.bDeviceClass != USB_CLASS_VENDOR_SPEC)
       && (dev->descriptor.bDeviceClass != 0))
      || (dev->config[0].interface[0].altsetting[0].bInterfaceClass !=
	  USB_CLASS_VENDOR_SPEC))
    {
      if (verbose > 2)
	printf
	  ("    this is not a LM983x (bDeviceClass = %d, bInterfaceClass = %d)\n",
	   dev->descriptor.bDeviceClass,
	   dev->config[0].interface[0].altsetting[0].bInterfaceClass);
      return 0;
    }
  if ((dev->descriptor.bcdUSB != 0x110)
      && (dev->descriptor.bcdUSB != 0x101)
      && (dev->descriptor.bcdUSB != 0x100))
    {
      if (verbose > 2)
	printf ("    this is not a LM983x (bcdUSB = 0x%x)\n",
		dev->descriptor.bcdUSB);
      return 0;
    }
  if (dev->descriptor.bDeviceSubClass != 0x00)
    {
      if (verbose > 2)
	printf ("    this is not a LM983x (bDeviceSubClass = 0x%x)\n",
		dev->descriptor.bDeviceSubClass);
      return 0;
    }
  if ((dev->descriptor.bDeviceProtocol != 0) &&
      (dev->descriptor.bDeviceProtocol != 0xff))
    {
      if (verbose > 2)
	printf ("    this is not a LM983x (bDeviceProtocol = 0x%x)\n",
		dev->descriptor.bDeviceProtocol);
      return 0;
    }

  /* Check endpoints */
  if (dev->config[0].interface[0].altsetting[0].bNumEndpoints != 3)
    {
      if (verbose > 2)
	printf ("    this is not a LM983x (bNumEndpoints = %d)\n",
		dev->config[0].interface[0].altsetting[0].bNumEndpoints);
      return 0;
    }

  if ((dev->config[0].interface[0].altsetting[0].endpoint[0].
       bEndpointAddress != 0x81)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].
	  bmAttributes != 0x03)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].
	  wMaxPacketSize != 0x1)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].bInterval !=
	  0x10))
    {
      if (verbose > 2)
	printf
	  ("    this is not a LM983x (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[0].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].bInterval);
      return 0;
    }

  if ((dev->config[0].interface[0].altsetting[0].endpoint[1].
       bEndpointAddress != 0x82)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].
	  bmAttributes != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].
	  wMaxPacketSize != 0x40)
      /* Currently disabled as we have some problems in detection here ! */
      /*|| (dev->config[0].interface[0].altsetting[0].endpoint[1].bInterval != 0) */
    )
    {
      if (verbose > 2)
	printf
	  ("    this is not a LM983x (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[1].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].bInterval);
      return 0;
    }

  if ((dev->config[0].interface[0].altsetting[0].endpoint[2].
       bEndpointAddress != 0x03)
      || (dev->config[0].interface[0].altsetting[0].endpoint[2].
	  bmAttributes != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[2].
	  wMaxPacketSize != 0x40)
      /* Currently disabled as we have some problems in detection here ! */
      /* || (dev->config[0].interface[0].altsetting[0].endpoint[2].bInterval != 0) */
    )
    {
      if (verbose > 2)
	printf
	  ("    this is not a LM983x (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[2].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[2].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[2].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[2].bInterval);
      return 0;
    }

  result = prepare_interface (dev, &handle);
  if (!result)
    return "LM983x?";

  result = lm983x_wb (handle, 0x07, 0x00);
  if (1 == result)
    result = lm983x_wb (handle, 0x08, 0x02);
  if (1 == result)
    result = lm983x_rb (handle, 0x07, &val);
  if (1 == result)
    result = lm983x_rb (handle, 0x08, &val);
  if (1 == result)
    result = lm983x_rb (handle, 0x69, &val);

  if (0 == result)
    {
      if (verbose > 2)
	printf ("  Couldn't access LM983x registers.\n");
      finish_interface (handle);
      return 0;
    }

  finish_interface (handle);

  switch (val)
    {
    case 4:
      return "LM9832/3";
      break;
    case 3:
      return "LM9831";
      break;
    case 2:
      return "LM9830";
      break;
    default:
      return "LM983x?";
      break;
    }
}

/********** the gl646 section **********/


static int
gl646_write_reg (usb_dev_handle * handle, unsigned char reg,
		 unsigned char val)
{
  int result;

  result =
    usb_control_msg (handle, 0x00, 0x00, 0x83, 0x00, (char *) &reg, 0x01,
		     TIMEOUT);
  if (result < 0)
    return 0;

  result =
    usb_control_msg (handle, 0x00, 0x00, 0x85, 0x00, (char *) &val, 0x01,
		     TIMEOUT);
  if (result < 0)
    return 0;

  return 1;
}

static int
gl646_read_reg (usb_dev_handle * handle, unsigned char reg,
		unsigned char *val)
{
  int result;

  result =
    usb_control_msg (handle, 0x00, 0x00, 0x83, 0x00, (char *) &reg, 0x01,
		     TIMEOUT);
  if (result < 0)
    return 0;

  result =
    usb_control_msg (handle, 0x80, 0x00, 0x84, 0x00, (char *) val, 0x01,
		     TIMEOUT);
  if (result < 0)
    return 0;

  return 1;
}


static char *
check_gl646 (struct usb_device *dev)
{
  unsigned char val;
  int result;
  usb_dev_handle *handle;

  if (verbose > 2)
    printf ("    checking for GL646 ...\n");

  /* Check device descriptor */
  if ((dev->descriptor.bDeviceClass != USB_CLASS_PER_INTERFACE)
      || (dev->config[0].interface[0].altsetting[0].bInterfaceClass != 0x10))
    {
      if (verbose > 2)
	printf
	  ("    this is not a GL646 (bDeviceClass = %d, bInterfaceClass = %d)\n",
	   dev->descriptor.bDeviceClass,
	   dev->config[0].interface[0].altsetting[0].bInterfaceClass);
      return 0;
    }
  if (dev->descriptor.bcdUSB != 0x110)
    {
      if (verbose > 2)
	printf ("    this is not a GL646 (bcdUSB = 0x%x)\n",
		dev->descriptor.bcdUSB);
      return 0;
    }
  if (dev->descriptor.bDeviceSubClass != 0x00)
    {
      if (verbose > 2)
	printf ("    this is not a GL646 (bDeviceSubClass = 0x%x)\n",
		dev->descriptor.bDeviceSubClass);
      return 0;
    }
  if (dev->descriptor.bDeviceProtocol != 0)
    {
      if (verbose > 2)
	printf ("    this is not a GL646 (bDeviceProtocol = 0x%x)\n",
		dev->descriptor.bDeviceProtocol);
      return 0;
    }

  /* Check endpoints */
  if (dev->config[0].interface[0].altsetting[0].bNumEndpoints != 3)
    {
      if (verbose > 2)
	printf ("    this is not a GL646 (bNumEndpoints = %d)\n",
		dev->config[0].interface[0].altsetting[0].bNumEndpoints);
      return 0;
    }

  if ((dev->config[0].interface[0].altsetting[0].endpoint[0].
       bEndpointAddress != 0x81)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].
	  bmAttributes != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].
	  wMaxPacketSize != 0x40)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].bInterval !=
	  0x0))
    {
      if (verbose > 2)
	printf
	  ("    this is not a GL646 (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[0].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].bInterval);
      return 0;
    }

  if ((dev->config[0].interface[0].altsetting[0].endpoint[1].
       bEndpointAddress != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].
	  bmAttributes != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].
	  wMaxPacketSize != 0x40)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].bInterval !=
	  0))
    {
      if (verbose > 2)
	printf
	  ("    this is not a GL646 (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[1].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].bInterval);
      return 0;
    }

  if ((dev->config[0].interface[0].altsetting[0].endpoint[2].
       bEndpointAddress != 0x83)
      || (dev->config[0].interface[0].altsetting[0].endpoint[2].
	  bmAttributes != 0x03)
      || (dev->config[0].interface[0].altsetting[0].endpoint[2].
	  wMaxPacketSize != 0x1)
      || (dev->config[0].interface[0].altsetting[0].endpoint[2].bInterval !=
	  8))
    {
      if (verbose > 2)
	printf
	  ("    this is not a GL646 (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[2].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[2].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[2].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[2].bInterval);
      return 0;
    }

  result = prepare_interface (dev, &handle);
  if (!result)
    return "GL646?";

  result = gl646_write_reg (handle, 0x38, 0x15);
  if (!result)
    {
      if (verbose > 2)
	printf ("    this is not a GL646 (writing register failed)\n");
      finish_interface (handle);
      return 0;
    }

  result = gl646_read_reg (handle, 0x4e, &val);
  if (!result)
    {
      if (verbose > 2)
	printf ("    this is not a GL646 (reading register failed)\n");
      finish_interface (handle);
      return 0;
    }

  if (val != 0x15)
    {
      if (verbose > 2)
	printf ("    this is not a GL646 (reg 0x4e != reg 0x38)\n");
      finish_interface (handle);
      return 0;
    }
  finish_interface (handle);
  return "GL646";
}

/* check for the combination of gl660 and gl646 */
static char *
check_gl660_gl646 (struct usb_device *dev)
{
  unsigned char val;
  int result;
  usb_dev_handle *handle;

  if (verbose > 2)
    printf ("    checking for GL660+GL646 ...\n");

  /* Check device descriptor */
  if ((dev->descriptor.bDeviceClass != USB_CLASS_VENDOR_SPEC)
      || (dev->config[0].interface[0].altsetting[0].bInterfaceClass != USB_CLASS_PER_INTERFACE))
    {
      if (verbose > 2)
	printf
	  ("    this is not a GL660+GL646 (bDeviceClass = %d, bInterfaceClass = %d)\n",
	   dev->descriptor.bDeviceClass,
	   dev->config[0].interface[0].altsetting[0].bInterfaceClass);
      return 0;
    }
  if (dev->descriptor.bcdUSB != 0x200)
    {
      if (verbose > 2)
	printf ("    this is not a GL660+GL646 (bcdUSB = 0x%x)\n",
		dev->descriptor.bcdUSB);
      return 0;
    }
  if (dev->descriptor.bDeviceSubClass != 0xff)
    {
      if (verbose > 2)
	printf ("    this is not a GL660+GL646 (bDeviceSubClass = 0x%x)\n",
		dev->descriptor.bDeviceSubClass);
      return 0;
    }
  if (dev->descriptor.bDeviceProtocol != 0xff)
    {
      if (verbose > 2)
	printf ("    this is not a GL660+GL646 (bDeviceProtocol = 0x%x)\n",
		dev->descriptor.bDeviceProtocol);
      return 0;
    }

  /* Check endpoints */
  if (dev->config[0].interface[0].altsetting[0].bNumEndpoints != 3)
    {
      if (verbose > 2)
	printf ("    this is not a GL660+GL646 (bNumEndpoints = %d)\n",
		dev->config[0].interface[0].altsetting[0].bNumEndpoints);
      return 0;
    }

  if ((dev->config[0].interface[0].altsetting[0].endpoint[0].
       bEndpointAddress != 0x81)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].
	  bmAttributes != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].
	  wMaxPacketSize != 0x40)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].bInterval !=
	  0x0))
    {
      if (verbose > 2)
	printf
	  ("    this is not a GL660+GL646 (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[0].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].bInterval);
      return 0;
    }

  if ((dev->config[0].interface[0].altsetting[0].endpoint[1].
       bEndpointAddress != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].
	  bmAttributes != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].
	  wMaxPacketSize != 0x40)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].bInterval !=
	  0))
    {
      if (verbose > 2)
	printf
	  ("    this is not a GL660+GL646 (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[1].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].bInterval);
      return 0;
    }

  if ((dev->config[0].interface[0].altsetting[0].endpoint[2].
       bEndpointAddress != 0x83)
      || (dev->config[0].interface[0].altsetting[0].endpoint[2].
	  bmAttributes != 0x03)
      || (dev->config[0].interface[0].altsetting[0].endpoint[2].
	  wMaxPacketSize != 0x1)
      || (dev->config[0].interface[0].altsetting[0].endpoint[2].bInterval !=
	  8))
    {
      if (verbose > 2)
	printf
	  ("    this is not a GL660+GL646 (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[2].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[2].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[2].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[2].bInterval);
      return 0;
    }

  result = prepare_interface (dev, &handle);
  if (!result)
    return "GL660+GL646?";

  result = gl646_write_reg (handle, 0x38, 0x15);
  if (!result)
    {
      if (verbose > 2)
	printf ("    this is not a GL660+GL646 (writing register failed)\n");
      finish_interface (handle);
      return 0;
    }

  result = gl646_read_reg (handle, 0x4e, &val);
  if (!result)
    {
      if (verbose > 2)
	printf ("    this is not a GL660+GL646 (reading register failed)\n");
      finish_interface (handle);
      return 0;
    }

  if (val != 0x15)
    {
      if (verbose > 2)
	printf ("    this is not a GL660+GL646 (reg 0x4e != reg 0x38)\n");
      finish_interface (handle);
      return 0;
    }
  finish_interface (handle);
  return "GL660+GL646";
}



char *
check_usb_chip (struct usb_device *dev, int verbosity)
{
  char *chip_name = 0;

  verbose = verbosity;

  if (verbose > 2)
    printf ("\n<trying to find out which USB chip is used>\n");

  chip_name = check_gt6801 (dev);

  if (!chip_name)
    chip_name = check_gt6816 (dev);

  if (!chip_name)
    chip_name = check_ma1017 (dev);

  if (!chip_name)
    chip_name = check_ma1015 (dev);

  if (!chip_name)
    chip_name = check_ma1509 (dev);

  if (!chip_name)
    chip_name = check_merlin (dev);

  if (!chip_name)
    chip_name = check_gl646 (dev);

  if (!chip_name)
    chip_name = check_gl660_gl646 (dev);

  if (verbose > 2)
    {
      if (chip_name)
	printf ("<This USB chip looks like a %s>\n\n", chip_name);
      else
	printf ("<Couldn't determine the type of the USB chip>\n\n");
    }

  return chip_name;
}

#endif /* HAVE_LIBUSB */
