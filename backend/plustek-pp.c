/*.............................................................................
 * Project : SANE library for Plustek flatbed scanners.
 *.............................................................................
 * File:	 plustek_pp.c - the interface to the parport driver...
 *.............................................................................
 *
 * based on Kazuhiro Sasayama previous work on
 * plustek.[ch] file from the SANE package.
 * original code taken from sane-0.71
 * Copyright (C) 1997 Hypercore Software Design, Ltd.
 * also based on the work done by Rick Bronson
 * Copyright (C) 2000/2001 Gerhard Jaeger <g.jaeger@earthling.net>
 *.............................................................................
 * History:
 * 0.40 - initial version
 * 0.41 - added _PTDRV_ADJUST call
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

/******************* wrapper functions for parport device ********************/

/*.............................................................................
 *
 */
static int ppDev_open( const char *dev_name, void *misc )
{
	int 		    result;
	int			    handle;
	unsigned short  version = _PTDRV_IOCTL_VERSION;
	Plustek_Device *dev     = (Plustek_Device *)misc;

	_INIT(0x378,190,15);

	if ((handle = _OPEN(dev_name)) < 0) {
	    DBG(_DBG_ERROR, "open: can't open %s as a device\n", dev_name);
    	return handle;
	}
	
	result = _IOCTL( handle, _PTDRV_OPEN_DEVICE, &version );
	if( result < 0 ) {
		_CLOSE( handle );
		DBG( _DBG_ERROR, "ioctl PT_DRV_OPEN_DEVICE failed(%d)\n", result );
        if( -9019 == result )
    		DBG( _DBG_ERROR,"Version problem, please recompile driver!\n" );
		return result;
    }

	_IOCTL( handle, _PTDRV_ADJUST, &dev->adj );

	return handle;
}

/*.............................................................................
 *
 */
static int ppDev_close( Plustek_Device *dev )
{
	return _CLOSE( dev->fd );
}

/*.............................................................................
 *
 */
static int ppDev_getCaps( Plustek_Device *dev )
{
	return _IOCTL( dev->fd, _PTDRV_GET_CAPABILITIES, &dev->caps );
}

/*.............................................................................
 *
 */
static int ppDev_getLensInfo( Plustek_Device *dev, pLensInfo lens )
{
	return _IOCTL( dev->fd, _PTDRV_GET_LENSINFO, lens );
}

/*.............................................................................
 *
 */
static int ppDev_getCropInfo( Plustek_Device *dev, pCropInfo crop )
{
	return _IOCTL( dev->fd, _PTDRV_GET_CROPINFO, crop );
}

/*.............................................................................
 *
 */
static int ppDev_putImgInfo( Plustek_Device *dev, pImgDef img )
{
	return _IOCTL( dev->fd, _PTDRV_PUT_IMAGEINFO, img );
}

/*.............................................................................
 *
 */
static int ppDev_setScanEnv( Plustek_Device *dev, pScanInfo sinfo )
{
	return _IOCTL( dev->fd, _PTDRV_SET_ENV, sinfo );
}

/*.............................................................................
 *
 */
static int ppDev_startScan( Plustek_Device *dev, pStartScan start )
{
	return _IOCTL( dev->fd, _PTDRV_START_SCAN, start );
}

/*.............................................................................
 *
 */
static int ppDev_stopScan( Plustek_Device *dev, int *mode )
{
	int retval;

	retval = _IOCTL( dev->fd, _PTDRV_STOP_SCAN, mode );

	if( 0 == *mode )
		_IOCTL( dev->fd, _PTDRV_CLOSE_DEVICE, 0);

	return retval;
}

/*.............................................................................
 *
 */
static int ppDev_readImage( Plustek_Device *dev,
                            SANE_Byte *buf, unsigned long data_length )
{
	return _READ( dev->fd, buf, data_length );
}

/* END PLUSTEK_PP.C .........................................................*/
