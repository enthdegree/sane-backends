/* sane - Scanner Access Now Easy.
   Copyright (C) 2001 Henning Meier-Geinitz
   This file is part of the SANE package.

   SANE is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   SANE is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License
   along with sane; see the file COPYING.  If not, write to the Free
   Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   This file provides a generic USB interface.  */

#ifndef sanei_usb_h
#define sanei_usb_h

#include "../include/sane/config.h"
#include "../include/sane/sane.h"

/* Initialize sanei_usb. Call this before any other sanei_usb function */
extern void
sanei_usb_init (void);

/* Get the vendor and product numbers for a given file descriptor. Currently,
   only scanners supported by the Linux USB scanner module can be found. Linux
   version must be 2.4.8 or higher. Returns SANE_STATUS_GOOD if the ids have
   been found, otherwise SANE_STATUS_UNSUPPORTED. */
extern SANE_Status
sanei_usb_get_vendor_product (SANE_Int fd, SANE_Word * vendor,
			      SANE_Word * product);

/* Find device names for given vendor and product ids. Above mentioned 
   limitations apply. The function attach is called for every device which
   has been found.*/
extern SANE_Status
sanei_usb_find_devices (SANE_Int vendor, SANE_Int product,
			SANE_Status (*attach) (SANE_String_Const devname));

/* Open an USB device using it's device file. Return a file descriptor. On
   success, SANE_STATUS_GOOD is returned. If the file couldn't be accessed
   due to permissions, SANE_STATUS_ACCESS_DENIED is returned. For every
   other error, the return value is SANE_STATUS_INVAL. */
extern SANE_Status
sanei_usb_open (SANE_String_Const devname, SANE_Int *fd);

/* Close an USB device represented by its file desciptor. */
extern void
sanei_usb_close (SANE_Int fd);

/* Initiate a bulk transfer read of up to zize bytes from the device to
   buffer. After the read, size contains the number of bytes actually
   read. Returns SANE_STATUS_GOOD on succes, SANE_STATUS_EOF if zero bytes
   have been read, SANE_STATUS_IO_ERROR if an error occured during the read,
   and SANE_STATUS_INVAL on every other error. */
extern SANE_Status
sanei_usb_read_bulk (SANE_Int fd, SANE_Byte * buffer, size_t *size);

/* Initiate a bulk transfer write of up to size bytes from buffer to the
   device. After the write size contains the number of bytes actually
   written. Returns SANE_STATUS_GOOD on succes, SANE_STATUS_IO_ERROR if an
   error occured during the read, and SANE_STATUS_INVAL on every other
   error. */
extern SANE_Status
sanei_usb_write_bulk (SANE_Int fd, SANE_Byte * buffer, size_t *size);

/* A convenience function to support expanding device name patterns
   into a list of devices.  Apart from a normal device name
   (such as /dev/usb/scanner0), this function currently supports USB
   device specifications of the form:

	usb VENDOR PRODUCT

   VENDOR and PRODUCT are non-negative integer numbers in decimal or
   hexadecimal format. A similar function for SCSI devices can be found
   in include/sane/config.h.
 */
extern void 
sanei_usb_attach_matching_devices (const char *name,
				   SANE_Status (*attach) (const char *dev));

/* Sends/Receives a control message to/from the USB device.
   data is the buffer to send/receive the data. len is the length of
   that data, or the length of the data to receive. The other arguments
   are passed directly to usb_control_msg (see the Linux USB project for
   documentation).

   The function returns 
     SANE_STATUS_IO_ERROR on error, 
     SANE_STATUS_GOOD on success,
	 SANE_STATUS_UNSUPPORTED if the feature is not supported by the
	                         OS or SANE.
*/

extern SANE_Status
sanei_usb_control_msg (SANE_Int fd, SANE_Int rtype, SANE_Int req,
		       SANE_Int value, SANE_Int index, SANE_Int len,
		       SANE_Byte *data);

#endif /* sanei_usb_h */
