/*.............................................................................
 * Project : SANE library for Plustek flatbed scanners.
 *.............................................................................
 */

/** @file plustek-usbmap.c
 *  @brief Creating and manipulating lookup tables.
 *
 * Based on sources acquired from Plustek Inc.<br>
 * Copyright (C) 2001-2003 Gerhard Jaeger <gerhard@gjaeger.de>
 *
 * History:
 * - 0.40 - starting version of the USB support
 * - 0.41 - fixed brightness problem for lineart mode
 * - 0.42 - removed preset of linear gamma tables
 * - 0.43 - no changes
 * - 0.44 - map inversion for negatatives now only upon user request
 * - 0.45 - no changes
 * - 0.46 - no changes
 * .
 * <hr>
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
 * <hr>
 */

#define _MAP_SIZE		4096U

static SANE_Byte a_bMap[_MAP_SIZE * 3];

/*.............................................................................
 * adjust acording to brightness and contrast
 */
static void usb_MapAdjust( pPlustek_Device dev )
{
	u_long i, tabLen;
	double b, c, tmp;
	
	tabLen = _MAP_SIZE;

	/*
	 * adjust brightness (b) and contrast (c) using the function:
	 *
	 * s(x,y) = (s(x,y) + b) * c
	 * b = [-127, 127]
	 * c = [0,2]
	 */

	/*
	 * scale brightness and contrast...
	 */
	b = ((double)dev->scanning.sParam.brightness * 192.0)/100.0;
	c = ((double)dev->scanning.sParam.contrast   + 100.0)/100.0;

	DBG( _DBG_INFO, "brightness   = %i -> %i\n",
					dev->scanning.sParam.brightness, (u_char)b);
	DBG( _DBG_INFO, "contrast*100 = %i -> %i\n",
					dev->scanning.sParam.contrast, (int)(c*100));

	for( i = 0; i < tabLen; i++ ) {

		tmp = ((double)(a_bMap[i] + b)) * c;
		if( tmp < 0 )   tmp = 0;
		if( tmp > 255 ) tmp = 255;
		a_bMap[i] = (u_char)tmp;

		tmp = ((double)(a_bMap[tabLen+i] + b)) * c;
		if( tmp < 0 )   tmp = 0;
		if( tmp > 255 ) tmp = 255;
		a_bMap[tabLen+i] = (u_char)tmp;

		tmp = ((double)(a_bMap[tabLen*2+i] + b)) * c;
		if( tmp < 0 )   tmp = 0;
		if( tmp > 255 ) tmp = 255;
		a_bMap[tabLen*2+i] = (u_char)tmp;
	}
}

/*.............................................................................
 *
 */
static SANE_Bool usb_MapDownload( pPlustek_Device dev, u_char bDataType )
{
    pScanDef  scanning = &dev->scanning;
	pDCapsDef sc       = &dev->usbDev.Caps;

	int       color, maxColor;			/* loop counters             */
	int       i, iThreshold;
	SANE_Byte value;					/* value transmitted to port */
	SANE_Bool fInverse = 0;
	
	DBG( _DBG_INFO, "usb_MapDownload()\n" );

	/* the maps are have been already set */
	
	/* do the brightness and contrast adjustment ... */			
	if( scanning->sParam.bDataType != SCANDATATYPE_BW )	
		usb_MapAdjust( dev );
			
	if( !usbio_WriteReg( dev->fd, 7, 0))
		return SANE_FALSE;

	/* we download all the time all three color maps, as we run
     * into trouble elsewhere on CanoScan models using gray mode
     */
#if 0
	if( bDataType == SCANDATATYPE_Color ) {
		color    = 0;
		maxColor = 3;
	} else {
		color    = 1;
		maxColor = 2;
	}
#else
	_VAR_NOT_USED( bDataType );

	color    = 0;
	maxColor = 3;
#endif

	for( ; color < maxColor; color++) {
	
		/* select color */
		value = (color << 2)+2;
		
		/* set gamma color selector */
		usbio_WriteReg( dev->fd, 0x03, value );
		usbio_WriteReg( dev->fd, 0x04, 0 );
		usbio_WriteReg( dev->fd, 0x05, 0 );
		
		/* write the gamma table entry to merlin */
		if( scanning->sParam.bDataType == SCANDATATYPE_BW )	{
		
			iThreshold = (int)((double)scanning->sParam.siThreshold *
											(_MAP_SIZE/200.0)) + (_MAP_SIZE/2);
			iThreshold = _MAP_SIZE - iThreshold;
			if(iThreshold < 0)
				iThreshold = 0;
			if(iThreshold > (int)_MAP_SIZE)
				iThreshold = _MAP_SIZE;
	
			DBG(_DBG_INFO, "Threshold is at %u siThresh=%i\n",
								iThreshold, scanning->sParam.siThreshold );

			for(i = 0; i < iThreshold; i++)
				a_bMap[color*_MAP_SIZE + i] = 0;
				
			for(i = iThreshold; i < (int)_MAP_SIZE; i++)
				a_bMap[color*_MAP_SIZE + i] = 255;

			fInverse = 1;
			
		} else {
			fInverse = 0;
		}

		if( /*scanning->dwFlag & SCANFLAG_Pseudo48 && */
			(scanning->sParam.bSource == SOURCE_Negative) &&
			(sc->workaroundFlag &_WAF_INV_NEGATIVE_MAP)) {
			fInverse ^= 1;
		}
		
		if((scanning->dwFlag & SCANFLAG_Invert) &&
									!(scanning->dwFlag & SCANFLAG_Pseudo48)) {
			fInverse ^= 1;
		}	

		if( fInverse ) {
		
			u_char  map[_MAP_SIZE];
			u_char *pMap = a_bMap+color*_MAP_SIZE;
			
			DBG( _DBG_INFO, "Inverting Map\n" );
			
			for( i = 0; i < (int)_MAP_SIZE; i++, pMap++ )
				map[i] = ~*pMap;
			
			sanei_lm983x_write( dev->fd,  0x06, map, _MAP_SIZE, SANE_FALSE );
			
		} else {
			sanei_lm983x_write( dev->fd,  0x06, a_bMap+color*_MAP_SIZE,
								 _MAP_SIZE, SANE_FALSE );
		}								
	
	} /* for each color */

	DBG( _DBG_INFO, "usb_MapDownload() done.\n" );
	
	return SANE_TRUE;
}

/* END PLUSTEK-USBMAP.C .....................................................*/
