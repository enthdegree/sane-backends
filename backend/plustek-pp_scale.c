/*.............................................................................
 * Project : linux driver for Plustek parallel-port scanners
 *.............................................................................
 * File:	plustek-pp_scale.c
 *.............................................................................
 *
 * Scaling functionality
 * Copyright (C) 2000-2003 Gerhard Jaeger <gerhard@gjaeger.de>
 *.............................................................................
 * History:
 * 0.32 - initial version
 * 0.33 - no changes
 * 0.34 - no changes
 * 0.35 - no changes
 * 0.36 - no changes
 * 0.37 - no changes
 * 0.38 - no changes
 * 0.39 - no changes
 * 0.40 - no changes
 * 0.41 - no changes
 * 0.42 - changed include names
 *
 *.............................................................................
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include "plustek-pp_scan.h"

/************************ exported functions *********************************/

/*.............................................................................
 * scaling picture data in x-direction, using a DDA algo
 * (digital differential analyzer).
 */
void ScaleX( pScanData ps, pUChar inBuf, pUChar outBuf )
{
	UChar tmp;
	int   step;
	int   izoom;
	int   ddax;
	register ULong i, j, x;

#ifdef DEBUG
	_VAR_NOT_USED( dbg_level );
#endif

	/* scale... */
	izoom = (int)(1.0/ps->DataInf.XYRatio * 1000);

	switch( ps->DataInf.wAppDataType ) {

	case COLOR_BW      : step = 0;  break;
	case COLOR_HALFTONE: step = 0;  break;
	case COLOR_256GRAY : step = 1;  break;
	case COLOR_TRUE24  : step = 3;  break;	/*NOTE: COLOR_TRUE32 is the same !*/
	case COLOR_TRUE48  : step = 6;  break;
	default			   : step = 99; break;
	}

	/*
	 * when not supported, only copy the data
	 */
	if( 99 == step ) {
		memcpy( outBuf, inBuf, ps->DataInf.dwAppBytesPerLine );
		return;
	}

	/*
	 * now scale...
	 */
	if( 0 == step ) {
		/*
		 * binary scaling
		 */
		ddax = 0;
		x 	 = 0;
		memset( outBuf, 0, ps->DataInf.dwAppBytesPerLine );

		for( i = 0; i < ps->DataInf.dwPhysBytesPerLine*8; i++ ) {

			ddax -= 1000;

			while( ddax < 0 ) {

				tmp = inBuf[(i>>3)];

				if((x>>3) < ps->DataInf.dwAppBytesPerLine ) {
					if( 0 != (tmp &= (1 << ((~(i & 0x7))&0x7))))
						outBuf[x>>3] |= (1 << ((~(x & 0x7))&0x7));
				}
				x++;
				ddax += izoom;
			}
		}

	} else {

		/*
		 * color and gray scaling
		 */
		ddax = 0;
		x 	 = 0;
		for( i = 0; i < ps->DataInf.dwPhysBytesPerLine*step; i+=step ) {

			ddax -= 1000;

			while( ddax < 0 ) {

				for( j = 0; j < (ULong)step; j++ ) {
        	
					if((x+j) < ps->DataInf.dwAppBytesPerLine ) {
						outBuf[x+j] = inBuf[i+j];
					}
				}
				x    += step;
				ddax += izoom;
			}
		}
	}
}

/* END PLUSTEK-PP_SCALE.C ...................................................*/
