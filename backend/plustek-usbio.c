/*.............................................................................
 * Project : SANE library for Plustek USB flatbed scanners.
 *.............................................................................
 * File:	 plustek-usbio.c - some I/O stuff
 *.............................................................................
 *
 * based on sources acquired from Plustek Inc.
 * Copyright (C) 2001-2002 Gerhard Jaeger <gerhard@gjaeger.de>
 *.............................................................................
 * History:
 * 0.40 - starting version of the USB support
 * 0.41 - moved some functions to a sane library (sanei_lm983x.c)
 * 0.42 - no changes
 *
 *.............................................................................
 *
 * This file is part of the SANE package.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 * MA 02111-1307, USA.
 *
 * As a special exception, the authors of SANE give permission for
 * additional uses of the libraries contained in this release of SANE.
 *
 * The exception is that, if you link a SANE library with other files
 * to produce an executable, this does not by itself cause the
 * resulting executable to be covered by the GNU General Public
 * License.  Your use of that executable is in no way restricted on
 * account of linking the SANE library code into it.
 *
 * This exception does not, however, invalidate any other reasons why
 * the executable file might be covered by the GNU General Public
 * License.
 *
 * If you submit changes to SANE to the maintainers to be included in
 * a subsequent release, you agree by submitting the changes that
 * those changes may be distributed with this exception intact.
 *
 * If you write modifications of your own for SANE, it is your choice
 * whether to permit this exception to apply to your modifications.
 * If you do not wish that, delete this exception notice.
 */

#include "sane/sanei_usb.h"
#include "sane/sanei_lm983x.h"

#define _UIO(func)                        \
    {                                     \
      SANE_Status status;                 \
      status = func;                      \
      if (status != SANE_STATUS_GOOD) {   \
		DBG( _DBG_ERROR, "UIO error\n" ); \
        return SANE_FALSE;                \
	  }                                   \
    }

#define usbio_ReadReg(fd, reg, value) \
  sanei_lm983x_read (fd, reg, value, 1, 0)

/**
 * function to read the contents of a LM983x register and regarding some
 * extra stuff, like flushing register 2 when writing register 0x58, etc
 *
 * @param handle -
 * @param reg    -
 * @param value  -
 * @return
 */
static Bool usbio_WriteReg( SANE_Int handle, SANE_Byte reg, SANE_Byte value )
{
	int       i;
	SANE_Byte data;

	/* retry loop... */
	for( i = 0; i < 100; i++ ) {

		_UIO( sanei_lm983x_write_byte( handle, reg, value ));
	
		/* Flush register 0x02 when register 0x58 is written */
 		if( 0x58 == reg ) {

			_UIO( usbio_ReadReg( handle, 2, &data ));
			_UIO( usbio_ReadReg( handle, 2, &data ));
 		
 		}
 		
		if( reg != 7 ) 	
			return SANE_TRUE;
			
		/* verify register 7 */
		_UIO( usbio_ReadReg( handle, 7, &data ));
	
		if( data == value ) {
			return SANE_TRUE;
		}
	}

	return SANE_FALSE;
}

/*.............................................................................
 *
 */
static SANE_Status usbio_DetectLM983x( SANE_Int fd, SANE_Byte *version )
{
	SANE_Byte value;

	DBG( _DBG_INFO, "usbio_DetectLM983x\n");

	_UIO( sanei_lm983x_write_byte(fd, 0x07, 0x00));
	_UIO( sanei_lm983x_write_byte(fd, 0x08, 0x02));
	_UIO( usbio_ReadReg(fd, 0x07, &value));
	_UIO( usbio_ReadReg(fd, 0x08, &value));
	_UIO( usbio_ReadReg(fd, 0x69, &value));

	value &= 7;
	if (version)
		*version = value;

	DBG( _DBG_INFO, "usbio_DetectLM983x: found " );

	switch((SANE_Int)value ) {
		
		case 4:	 DBG( _DBG_INFO, "LM9832/3\n" ); break;
		case 3:	 DBG( _DBG_INFO, "LM9831\n" );   break;
		case 2:	 DBG( _DBG_INFO, "LM9830 --> unsupported!!!\n" );
				 return SANE_STATUS_INVAL;
				 break;
		default: DBG( _DBG_INFO, "Unknown chip v%d\n", value );
				 return SANE_STATUS_INVAL;
				 break;
	}
  	
	return SANE_STATUS_GOOD;
}

/*.............................................................................
 *
 */
static SANE_Status usbio_ResetLM983x( pPlustek_Device dev )
{
	SANE_Byte value;
	pHWDef    hw = &dev->usbDev.HwSetting;

	if( _LM9831 == hw->chip ) {

		_UIO( sanei_lm983x_write_byte( dev->fd, 0x07, 0));
		_UIO( sanei_lm983x_write_byte( dev->fd, 0x07,0x20));
		_UIO( sanei_lm983x_write_byte( dev->fd, 0x07, 0));
		_UIO( usbio_ReadReg( dev->fd, 0x07, &value));
		if (value != 0) {
    		DBG( _DBG_ERROR, "usbio_ResetLM983x: "
							 "reset wasn't successful, status=%d\n", value );
			return SANE_STATUS_INVAL;
    	}
	} else {
		_UIO( sanei_lm983x_write_byte( dev->fd, 0x07, 0));
	}

	return SANE_STATUS_GOOD;
}

/* END PLUSTEK-USBIO.C ......................................................*/
