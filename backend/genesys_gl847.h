/* sane - Scanner Access Now Easy.

   Copyright (C) 2010-2013 Stéphane Voltz <stef.dev@free.fr>

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
*/

#ifndef BACKEND_GENESYS_GL847_H
#define BACKEND_GENESYS_GL847_H

#include "genesys.h"

/** set up registers for an actual scan
 *
 * this function sets up the scanner to scan in normal or single line mode
 */
// Send the low-level scan command
static void gl847_begin_scan(Genesys_Device* dev, const Genesys_Sensor& sensor,
                             Genesys_Register_Set* reg, SANE_Bool start_motor);

// Send the stop scan command
static void gl847_end_scan(Genesys_Device* dev, Genesys_Register_Set* reg, SANE_Bool check_stop);

static void gl847_init(Genesys_Device* dev);

/** @brief moves the slider to steps at motor base dpi
 * @param dev device to work on
 * @param steps number of steps to move
 * */
static void gl847_feed(Genesys_Device* dev, unsigned int steps);

typedef struct
{
  uint8_t sensor_id;
  uint8_t r6b;
  uint8_t r6c;
  uint8_t r6d;
  uint8_t r6e;
  uint8_t r6f;
  uint8_t ra6;
  uint8_t ra7;
  uint8_t ra8;
  uint8_t ra9;
} Gpio_Profile;

static Gpio_Profile gpios[]={
    { GPO_CANONLIDE200, 0x02, 0xf9, 0x20, 0xff, 0x00, 0x04, 0x04, 0x00, 0x00},
    { GPO_CANONLIDE700, 0x06, 0xdb, 0xff, 0xff, 0x80, 0x15, 0x07, 0x20, 0x10},
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

typedef struct
{
  uint8_t dramsel;
  uint8_t rd0;
  uint8_t rd1;
  uint8_t rd2;
  uint8_t re0;
  uint8_t re1;
  uint8_t re2;
  uint8_t re3;
  uint8_t re4;
  uint8_t re5;
  uint8_t re6;
  uint8_t re7;
} Memory_layout;

static Memory_layout layouts[]={
	/* LIDE 100 */
	{
                0x29,
		0x0a, 0x15, 0x20,
		0x00, 0xac, 0x02, 0x55, 0x02, 0x56, 0x03, 0xff
	},
	/* LIDE 200 */
	{
                0x29,
		0x0a, 0x1f, 0x34,
		0x01, 0x24, 0x02, 0x91, 0x02, 0x92, 0x03, 0xff
	},
	/* 5600F */
	{
                0x29,
		0x0a, 0x1f, 0x34,
		0x01, 0x24, 0x02, 0x91, 0x02, 0x92, 0x03, 0xff
	},
	/* LIDE 700F */
	{
                0x2a,
		0x0a, 0x33, 0x5c,
		0x02, 0x14, 0x09, 0x09, 0x09, 0x0a, 0x0f, 0xff
	}
};


#endif // BACKEND_GENESYS_GL847_H
