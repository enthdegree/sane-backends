/* sane - Scanner Access Now Easy.
   Copyright (C) 1997 Jeffrey S. Freedman
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

   This file provides generic configuration support.  */

#ifndef sanei_config_h

#include <stdio.h>

extern FILE *sanei_config_open (const char *name);
extern char *sanei_config_read (char *str, int n, FILE *stream);
extern const char *sanei_config_skip_whitespace (const char *str);
/* Scan a string constant from the string pointed to by STR and return
   a malloced copy of it in *STRING_CONST (it's the responsibility of
   the caller to free the returned string at an appropriate time).
   Whitespace in front of the string constant is ignored.  Whitespace
   can be included in the string constant by enclosing it in
   double-quotes.  A return val of NULL indicates that no string
   constant was found. */
extern const char *sanei_config_get_string (const char *str,
					    char **string_const);

/* A convenience function to support expanding device name patterns
   into a list of devices.  Apart from a normal device name
   (such as /dev/sdb), this function currently supports SCSI
   device specifications of the form:

	scsi VENDOR MODEL TYPE BUS CHANNEL ID LUN

   Where VENDOR is the desired vendor name.  MODEL is the desired
   MODEL name.  TYPE is the desired device type.  All of these can be
   set to * to match anything.  To include whitespace in these
   strings, enclose them in double-quotes (").  BUS, ID, and LUN are
   the desired SCSI bus, id, and logical-unit numbers.  These can be
   set to * or simply omitted to match anything. 

   Also USB device specifications of the form:

        usb VENDOR PRODUCT

   are supported. VENDOR and PRODUCT are non-negative integer numbers in
   decimal or hexadecimal format.
 */
extern void sanei_config_attach_matching_devices (const char *name,
						  SANE_Status (*attach)
						    (const char *dev));

#endif	/* sanei_config_h */
