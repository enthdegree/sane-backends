/*.............................................................................
 * Project : SANE library for Plustek USB flatbed scanners.
 *.............................................................................
 * File:	 plustek-usb.c - the interface functions to the USB driver stuff
 *.............................................................................
 *
 * based on sources acquired from Plustek Inc.
 * Copyright (C) 2001 Gerhard Jaeger <g.jaeger@earthling.net>
 *.............................................................................
 * History:
 * 0.40 - starting version of the USB support
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

/* the other stuff is included by plustek.c ...*/
#include "sane/sanei_usb.h"

#define CHECK(func)                   \
    {                                 \
      SANE_Status status;             \
      status = func;                  \
      if (status != SANE_STATUS_GOOD) \
          return status;              \
    }

/*
 * to allow different vendors...
 */
static TabDef usbVendors[] = {

	{ 0x07B3, "Plustek"         },
	{ 0x0400, "Mustek"          },  /* this in fact is not correct */
                                    /* but used for the BearPaws   */
	{ 0x0458, "KYE/Genius"      },
	{ 0x03F0, "Hewlett-Packard" },
	{ 0x04B8, "Epson"           },
	{ 0xFFFF, NULL              }
};


/********************** the USB scanner interface ****************************/

/*.............................................................................
 * assign the values to the structures used by the currently found scanner
 */
static void usb_initDev( pPlustek_Device dev, int idx, int handle, int vendor )
{
	int       i;
	ScanParam sParam;

	DBG( _DBG_INFO, "usb_initDev()\n" );

	memset( &dev->usbDev, 0, sizeof(DeviceDef));

	memcpy( &dev->usbDev.Caps, Settings[idx].pDevCaps, sizeof(DCapsDef));
	memcpy( &dev->usbDev.HwSetting, Settings[idx].pHwDef, sizeof(HWDef));

	/*
	 * the following you normally get from the registry...
	 *usbVendors[i].desc 30 and 8 are for a UT12
	 */
    dev->usbDev.bStepsToReverse = 30;
    dev->usbDev.dwBufferSize    = 8 * 1024 * 1024; /*min val is 4MB!!!*/
		
	dev->usbDev.ModelStr = Settings[idx].pModelString;

	dev->fd = handle;
	
	/*
     * well now we patch the vendor string...
     * if not found, the default vendor will be Plustek
     */
	for( i = 0; usbVendors[i].desc != NULL; i++ ) {

		if( usbVendors[i].id == vendor ) {
			dev->sane.vendor = usbVendors[i].desc;
			DBG( _DBG_INFO, "Vendor adjusted to: >%s<\n", dev->sane.vendor );
			break;
		}
	}

	usb_ResetRegisters( dev );
    usbio_ResetLM983x ( dev );

	sParam.bBitDepth     = 8;
	sParam.bCalibration  = PARAM_Scan;
	sParam.bChannels     = 3;
	sParam.bDataType     = SCANDATATYPE_Color;
	sParam.bSource       = SOURCE_Reflection;
	sParam.Origin.x      = 0;
    sParam.Origin.y      = 0;
	sParam.siThreshold   = 0;
	sParam.UserDpi.x     = 150;
    sParam.UserDpi.y     = 150;
	sParam.dMCLK         = 4;
	sParam.Size.dwPixels = 0;

	/* initialize the ASIC registers */
	usb_SetScanParameters( dev, &sParam );

	/* check and move sensor to its home position */
	usb_ModuleToHome( dev, SANE_FALSE );
}

/*.............................................................................
 * will be used for retrieving a Plustek device
 */
static int usb_CheckForPlustekDevice( int handle, pPlustek_Device dev )
{
	char   tmp[50];
    char   pcbStr[10];
    u_char reg59[3], reg59s[3], pcbID;
    int    i, result;

	/*
	 * Plustek uses the misc IO 12 to get the PCB ID
	 * (PCB = printed circuit board), so it's possible to have one
	 * product ID and up to 7 different devices...
	 */
   	DBG( _DBG_INFO, "Trying to get the pcbID of a Plustek device...\n" );
		
    /*
   	 * get the PCB-ID
     */
	result = sanei_lm9831_read( handle, 0x59, reg59s, 3, SANE_TRUE );
   	if( SANE_STATUS_GOOD != result ) {
		sanei_usb_close( handle );
       	return -1;
	}

	reg59[0] = 0x22;	/* PIO1: Input, PIO2: Input         */
	reg59[1] = 0x02;	/* PIO3: Input, PIO4: Output as low */
	reg59[2] = 0x03;

	result = sanei_lm9831_write( handle, 0x59,  reg59, 3, SANE_TRUE );
    if( SANE_STATUS_GOOD != result ) {
		sanei_usb_close( handle );
       	return -1;
	}

	result = sanei_lm9831_read ( handle, 0x02, &pcbID, 1, SANE_TRUE );
    if( SANE_STATUS_GOOD != result ) {
		sanei_usb_close( handle );
       	return -1;
	}

	pcbID = (u_char)((pcbID >> 2) & 0x07);
	result = sanei_lm9831_read( handle, 0x59, reg59s, 3, SANE_TRUE );
    if( SANE_STATUS_GOOD != result ) {
		sanei_usb_close( handle );
       	return -1;
	}

   	DBG( _DBG_INFO, "pcbID=0x%02x\n", pcbID );

    /* now roam through the setting list... */
    strncpy( tmp, dev->usbId, 13 );
	tmp[13] = '\0';
	
	sprintf( pcbStr, "-%u", pcbID );
	strcat ( tmp, pcbStr );

    DBG( _DBG_INFO, "Checking for device >%s<\n", tmp );

	for( i = 0; NULL != Settings[i].pIDString; i++ ) {

		if( 0 == strcmp( Settings[i].pIDString, tmp )) {
	    	DBG(_DBG_INFO, "Device description for >%s< found.\n", tmp );
			usb_initDev( dev, i, handle, 0x07B3 );
	    	return handle;
		}
	}
	
	return -1;
}

/*.............................................................................
 * will be called upon sane_exit
 */
static void usbDev_shutdown( Plustek_Device *dev  )
{
	SANE_Int handle;

	DBG( _DBG_INFO, "Shutdown called (dev->fd=%d, %s)\n",
													dev->fd, dev->sane.name );
    if( SANE_STATUS_GOOD == sanei_usb_open( dev->sane.name, &handle )) {
		
    	dev->fd = handle;

		usb_LampOn( dev, SANE_FALSE, -1, SANE_FALSE );
		
		dev->fd = -1;
		
		sanei_usb_close( handle );
	}
}

/*.............................................................................
 *
 */
static int usbDev_open( const char *dev_name, void *misc )
{
    char            devStr[50];
    int             result;
	int             i;
	SANE_Int        handle;
	SANE_Byte       version;
	SANE_Word    	vendor, product;
    pPlustek_Device dev = (pPlustek_Device)misc;

    DBG( _DBG_INFO, "usbDev_open(%s,%s)\n", dev_name, dev->usbId );

    if( SANE_STATUS_GOOD != sanei_usb_open( dev_name, &handle )) {
        return -1;
	}

	result = sanei_usb_get_vendor_product( handle, &vendor, &product );
	if( SANE_STATUS_GOOD == result ) {

		sprintf( devStr, "0x%04X-0x%04X", vendor, product );

		DBG(_DBG_INFO,"Vendor ID=0x%04X, Product ID=0x%04X\n",vendor,product);

		if( dev->usbId[0] != '\0' ) {
		
			if( 0 != strcmp( dev->usbId, devStr )) {
				DBG( _DBG_ERROR, "Specified Vendor and Product ID "
								 "doesn't match with the ones\n"
								 "in the config file\n" );
				sanei_usb_close( handle );
		        return -1;
			}
		} else {
			sprintf( dev->usbId, "0x%04X-0x%04X", vendor, product );
		}			

	} else {

		DBG( _DBG_INFO, "Can't get vendor ID from driver...\n" );

		/* if the ioctl stuff is not supported by the kernel and we have
         * nothing specified, we have to give up...
         */
		if( dev->usbId[0] == '\0' ) {
			DBG( _DBG_ERROR, "Cannot autodetect Vendor an Product ID, "
							 "please specify in config file.\n" );
			sanei_usb_close( handle );
	        return -1;
		}
		
		vendor = strtol( dev->usbId, 0, 0 );
		DBG( _DBG_INFO, "... using the specified: 0x%04x\n", vendor );
	}

    if( SANE_STATUS_GOOD != usbio_DetectLM983x( handle, &version )) {
		sanei_usb_close( handle );
        return -1;
    }

    if ((version < 3) || (version > 4)) {
		DBG( _DBG_ERROR, "This is not a LM9831 or LM9832 chip based scanner.\n" );
		sanei_usb_close( handle );
		return -1;
    }

#if 0
	dev->fd = handle;
    usbio_ResetLM983x( dev );
	dev->fd = -1;
#endif	
	sanei_lm9831_reset( handle );
	
	/*
	 * Plustek uses the misc IO 1/2 to get the PCB ID
	 * (PCB = printed circuit board), so it's possible to have one
	 * product ID and up to 7 different devices...
	 */
	if( 0x07B3 == vendor ) {
	
		handle = usb_CheckForPlustekDevice( handle, dev );

		if( handle >= 0 )
			return handle;	
		
	} else {		

	    /* now roam through the setting list... */
	    strncpy( devStr, dev->usbId, 13 );
		devStr[13] = '\0';

	    /*
    	 * if we don't use the PCD ID extension...
	     */
		for( i = 0; NULL != Settings[i].pIDString; i++ ) {

			if( 0 == strncmp( Settings[i].pIDString, devStr, 13 )) {
		    	DBG( _DBG_INFO, "Device description for >%s< found.\n", devStr );
				usb_initDev( dev, i, handle, vendor );
			    return handle;
			}
		}			
	}

	sanei_usb_close( handle );
	DBG( _DBG_ERROR, "No matching device found >%s<\n", devStr );
	return -1;
}

/*.............................................................................
 *
 */
static int usbDev_close( Plustek_Device *dev )
{
	sanei_usb_close( dev->fd );
	dev->fd = -1;
    return 0;
}

/*.............................................................................
 *
 */
static int usbDev_getCaps( Plustek_Device *dev )
{
	pDCapsDef scaps = &dev->usbDev.Caps;

	DBG( _DBG_INFO, "usbDev_getCaps()\n" );

	dev->caps.rDataType.wMin    = scaps->Normal.MinDpi.x;
	dev->caps.rDataType.wDef    = scaps->Normal.MinDpi.x;
	dev->caps.rDataType.wMax    = scaps->OpticDpi.y;
	dev->caps.rDataType.wPhyMax = scaps->OpticDpi.x;

	dev->caps.dwBits = 0;
	dev->caps.dwFlag = (SFLAG_SCANNERDEV + SFLAG_FLATBED);

	if(((DEVCAPSFLAG_Positive == scaps->wFlags)  &&
	    (DEVCAPSFLAG_Negative == scaps->wFlags)) ||
		(DEVCAPSFLAG_TPA      == scaps->wFlags)) {
		dev->caps.dwFlag |= SFLAG_TPA;
	}

	dev->caps.wIOBase = 0;
	dev->caps.wLens   = 1;

	dev->caps.wMaxExtentX = scaps->Normal.Size.x;
	dev->caps.wMaxExtentY = scaps->Normal.Size.y;
	dev->caps.AsicID      = _ASIC_IS_USB;
	dev->caps.Model       = MODEL_OP_USB;
	dev->caps.Version     = 0;

    return 0;
}

/*.............................................................................
 *
 */
static int usbDev_getLensInfo( Plustek_Device *dev, pLensInfo lens )
{
	DBG( _DBG_INFO, "usbDev_getLensInfo()\n" );

	lens->wBeginX = 0;
	lens->wBeginY = 0;

	lens->rExtentX.wMin	   = 150;
	lens->rExtentX.wDef    =
	lens->rExtentX.wMax    =
	lens->rExtentX.wPhyMax = dev->usbDev.Caps.Normal.Size.x;

	lens->rExtentY.wMin    = 150;
	lens->rExtentY.wDef    =
	lens->rExtentY.wMax    =
	lens->rExtentY.wPhyMax = dev->usbDev.Caps.Normal.Size.y;	

    /* set the DPI stuff */
	lens->rDpiX.wMin 	  = 16;
	lens->rDpiX.wDef 	  = 50;
	lens->rDpiX.wMax 	  = (dev->usbDev.Caps.OpticDpi.x *16);
	lens->rDpiX.wPhyMax   = dev->usbDev.Caps.OpticDpi.x;
	lens->rDpiY.wMin 	  = 16;
	lens->rDpiY.wDef 	  = 50;
	lens->rDpiY.wMax 	  = (dev->usbDev.Caps.OpticDpi.x *16);
	lens->rDpiY.wPhyMax   = (dev->usbDev.Caps.OpticDpi.x * 2);

    return 0;
}

/*
 * HEINER: workaround - remove it!!!!
 */
static ImgDef xxx;

/*.............................................................................
 * HEINER: No function, should be removed from backend and driver as well
 */
static int usbDev_putImgInfo( Plustek_Device *dev, pImgDef img )
{
	DBG( _DBG_INFO, "usbDev_putImgInfo()\n" );

    _VAR_NOT_USED(dev);

	memcpy( &xxx, img, sizeof(ImgDef));

    return 0;
}

/*.............................................................................
 *
 */
static int usbDev_getCropInfo( Plustek_Device *dev, pCropInfo ci )
{
	WinInfo size;

	DBG( _DBG_INFO, "usbDev_getCropInfo()\n" );

    _VAR_NOT_USED(dev);

	memcpy( &ci->ImgDef, &xxx, sizeof(ImgDef));

	usb_GetImageInfo( &ci->ImgDef, &size );

	ci->dwPixelsPerLine = size.dwPixels;
	ci->dwLinesPerArea  = size.dwLines;
	ci->dwBytesPerLine  = size.dwBytes;

	if( ci->ImgDef.dwFlag & SCANDEF_BoundaryDWORD )
		ci->dwBytesPerLine = (ci->dwBytesPerLine + 3UL) & 0xfffffffcUL;

	ci->dwOffsetX = (u_long)ci->ImgDef.crArea.x * ci->ImgDef.xyDpi.x / 300UL;
	ci->dwOffsetY = (u_long)ci->ImgDef.crArea.y * ci->ImgDef.xyDpi.y / 300UL;

	DBG( _DBG_INFO, "PPL = %u\n",  ci->dwPixelsPerLine );
	DBG( _DBG_INFO, "LPA = %u\n",  ci->dwLinesPerArea );
	DBG( _DBG_INFO, "BPL = %u\n",  ci->dwBytesPerLine );

    return 0;
}

/*.............................................................................
 *
 */
static int usbDev_setScanEnv( Plustek_Device *dev, pScanInfo si )
{
	DBG( _DBG_INFO, "usbDev_setScanEnv()\n" );

    /* clear all the stuff */
    memset( &dev->scanning, 0, sizeof(ScanDef));

	if( si->ImgDef.dwFlag & SCANDEF_Adf && si->ImgDef.dwFlag & SCANDEF_ContinuousScan)
		dev->scanning.sParam.dMCLK = dMCLK_ADF;

    /* Save necessary informations */
	dev->scanning.fGrayFromColor = 0;

	if( si->ImgDef.wDataType == COLOR_256GRAY ) {

#if 0
		if( dev->scanning.fGrayFromColor = Registry.GetGrayFromColor())
			si->ImgDef.wDataType = COLOR_TRUE24;
		else
#endif
		if( !(si->ImgDef.dwFlag & SCANDEF_Adf ) &&
 		  (dev->usbDev.Caps.OpticDpi.x == 1200 && si->ImgDef.xyDpi.x <= 300)) {
			dev->scanning.fGrayFromColor = 2;
			si->ImgDef.wDataType = COLOR_TRUE24;
		}
	}

    usb_SaveImageInfo( &si->ImgDef, &dev->scanning.sParam );
    usb_GetImageInfo ( &si->ImgDef, &dev->scanning.sParam.Size );

    /* Flags */
    dev->scanning.dwFlag = si->ImgDef.dwFlag & (SCANFLAG_bgr | SCANFLAG_BottomUp | SCANFLAG_Invert |
		    								    SCANFLAG_DWORDBoundary | SCANFLAG_RightAlign |
    								   SCANFLAG_StillModule | SCANDEF_Adf | SCANDEF_ContinuousScan);

    dev->scanning.sParam.siThreshold = si->siBrightness;
    dev->scanning.sParam.brightness  = si->siBrightness;
    dev->scanning.sParam.contrast    = si->siContrast;

    if( dev->scanning.sParam.bBitDepth <= 8 )
    	dev->scanning.dwFlag &= ~SCANFLAG_RightAlign;

    if( dev->scanning.dwFlag & SCANFLAG_DWORDBoundary ) {
		if( dev->scanning.fGrayFromColor )
			dev->scanning.dwBytesLine = (dev->scanning.sParam.Size.dwBytes / 3 + 3) & 0xfffffffcUL;
		else
			dev->scanning.dwBytesLine = (dev->scanning.sParam.Size.dwBytes + 3UL) & 0xfffffffcUL;

	} else {

		if( dev->scanning.fGrayFromColor )
			dev->scanning.dwBytesLine = dev->scanning.sParam.Size.dwBytes / 3;
		else
			dev->scanning.dwBytesLine = dev->scanning.sParam.Size.dwBytes;
	}

	if( dev->scanning.dwFlag & SCANFLAG_BottomUp)
		dev->scanning.lBufAdjust = -(long)dev->scanning.dwBytesLine;
	else
		dev->scanning.lBufAdjust = dev->scanning.dwBytesLine;

	/* LM9831 has a BUG in 16-bit mode,
     * so we generate pseudo 16-bit data from 8-bit
	 */
	if( dev->scanning.sParam.bBitDepth > 8 ) {

		if( dev->usbDev.HwSetting.fLM9831 ) {

			dev->scanning.sParam.bBitDepth = 8;
			dev->scanning.dwFlag |= SCANFLAG_Pseudo48;
			dev->scanning.sParam.Size.dwBytes >>= 1;
		}
	}

    /* Source selection */
    if( dev->scanning.sParam.bSource == SOURCE_Reflection ) {

    	dev->usbDev.pSource = &dev->usbDev.Caps.Normal;
    	dev->scanning.sParam.Origin.x += dev->usbDev.pSource->DataOrigin.x + (u_long)dev->usbDev.Normal.lLeft;
    	dev->scanning.sParam.Origin.y += dev->usbDev.pSource->DataOrigin.y + (u_long)dev->usbDev.Normal.lUp;

    } else if( dev->scanning.sParam.bSource == SOURCE_Transparency ) {

		dev->usbDev.pSource = &dev->usbDev.Caps.Positive;
    	dev->scanning.sParam.Origin.x += dev->usbDev.pSource->DataOrigin.x + (u_long)dev->usbDev.Positive.lLeft;
    	dev->scanning.sParam.Origin.y += dev->usbDev.pSource->DataOrigin.y + (u_long)dev->usbDev.Positive.lUp;

	} else if( dev->scanning.sParam.bSource == SOURCE_Negative ) {

		dev->usbDev.pSource = &dev->usbDev.Caps.Negative;
    	dev->scanning.sParam.Origin.x += dev->usbDev.pSource->DataOrigin.x + (u_long)dev->usbDev.Negative.lLeft;
    	dev->scanning.sParam.Origin.y += dev->usbDev.pSource->DataOrigin.y + (u_long)dev->usbDev.Negative.lUp;

	} else {

		dev->usbDev.pSource = &dev->usbDev.Caps.Adf;
    	dev->scanning.sParam.Origin.x += dev->usbDev.pSource->DataOrigin.x + (u_long)dev->usbDev.Normal.lLeft;
    	dev->scanning.sParam.Origin.y += dev->usbDev.pSource->DataOrigin.y + (u_long)dev->usbDev.Normal.lUp;
	}

	if( dev->scanning.sParam.bSource == SOURCE_ADF ) {

		if( dev->scanning.dwFlag & SCANDEF_ContinuousScan )
			fLastScanIsAdf = SANE_TRUE;
		else
			fLastScanIsAdf = SANE_FALSE;
	}

    return 0;
}

/*.............................................................................
 *
 */
static int usbDev_stopScan( Plustek_Device *dev, int *mode )
{
	DBG( _DBG_INFO, "usbDev_stopScan(mode=%u)\n", *mode );

	/* in cancel-mode we first stop the motor */
	usb_ScanEnd( dev );

	dev->scanning.dwFlag = 0;

	if( NULL != dev->scanning.pScanBuffer ) {

		free( dev->scanning.pScanBuffer );
		dev->scanning.pScanBuffer = NULL;
	}

    return 0;
}

/*.............................................................................
 *
 */
static int usbDev_startScan( Plustek_Device *dev, pStartScan start )
{
    pScanDef   scanning = &dev->scanning;
	static int iSkipLinesForADF = 0;

	DBG( _DBG_INFO, "usbDev_startScan()\n" );

	a_bRegs[0x0a] = 0;

	/* Allocate shading buffer */
	if((dev->scanning.dwFlag & SCANDEF_Adf) &&
	   (dev->scanning.dwFlag & SCANDEF_ContinuousScan)) {
		dev->scanning.fCalibrated = SANE_TRUE;
	} else {
		dev->scanning.fCalibrated = SANE_FALSE;
		iSkipLinesForADF = 0;
	}

	scanning->sParam.PhyDpi.x = usb_SetAsicDpiX( dev, scanning->sParam.UserDpi.x );
	scanning->pScanBuffer = (u_char*)malloc( dev->usbDev.dwBufferSize );

	if( NULL != scanning->pScanBuffer ) {

		scanning->dwFlag |= SCANFLAG_StartScan;
		usb_LampOn( dev, SANE_TRUE, -1, SANE_TRUE );

		start->dwBytesPerLine = scanning->dwBytesLine;
		start->dwFlag 		  = scanning->dwFlag;
		return 0;
	}

    return _E_ALLOC;
}

/*.............................................................................
 * do the reading stuff here...
 * first we perform the calibration step, and the we read the image
 * line for line
 */
static int usbDev_readImage( struct Plustek_Device *dev,
                             SANE_Byte *buf, unsigned long data_length )
{
    int       result, lines;
	u_long    dw;
    pScanDef  scanning = &dev->scanning;
	pDCapsDef scaps    = &dev->usbDev.Caps;
	pHWDef    hw       = &dev->usbDev.HwSetting;

	DBG( _DBG_INFO, "usbDev_readImage()\n" );

	/* check the current position of the sensor and move it back
	 * to it's home position if necessary...
	 */
	usb_ModuleStatus( dev );

/*
 * speed test...
 */	
#if 0
{
	ScanParam  m_ScanParam;

	m_ScanParam.Size.dwLines  = 1;				/* for gain */
	m_ScanParam.Size.dwPixels = scaps->Normal.Size.x *
								scaps->OpticDpi.x / 300UL;
	m_ScanParam.Size.dwBytes  = m_ScanParam.Size.dwPixels *
								2 * m_ScanParam.bChannels;
	m_ScanParam.Origin.x      = (u_short)((u_long) hw->wActivePixelsStart *
													300UL / scaps->OpticDpi.x);
	m_ScanParam.bCalibration  = PARAM_Gain;

	m_ScanParam.dMCLK = dMCLK;

	usb_SetScanParameters( dev, &m_ScanParam );
	usleep(50*1000);
	usb_ScanBegin( dev, SANE_FALSE );
	DBG(_DBG_INFO,"reading %u bytes\n", m_ScanParam.Size.dwPhyBytes);
	usb_ScanReadImage( dev, scanning->pScanBuffer, m_ScanParam.Size.dwPhyBytes );
	usb_ScanEnd( dev );
}
#endif	
	
    result = usb_DoCalibration( dev );
    if( SANE_TRUE != result ) {
		DBG( _DBG_INFO, "calibration failed!!!\n" );
        return result;
	}

	DBG( _DBG_INFO, "calibration done.\n" );

	if( !( scanning->dwFlag & SCANFLAG_Scanning )) {

		usleep( 10 * 1000 );

		if( usb_SetScanParameters( dev, &scanning->sParam )) {

			scanning->pbScanBufBegin = scanning->pScanBuffer;

			if((dev->caps.dwFlag & SFLAG_ADF) && (scaps->OpticDpi.x == 600))
				scanning->dwLinesScanBuf = 8;
			else
				scanning->dwLinesScanBuf = 32;

			scanning->dwBytesScanBuf     = scanning->dwLinesScanBuf *
										   scanning->sParam.Size.dwPhyBytes;
			scanning->dwNumberOfScanBufs = dev->usbDev.dwBufferSize /
										   scanning->dwBytesScanBuf;
			scanning->dwLinesPerScanBufs = scanning->dwNumberOfScanBufs *
										   scanning->dwLinesScanBuf;
			scanning->pbScanBufEnd       = scanning->pbScanBufBegin +
										   scanning->dwLinesPerScanBufs *
										   scanning->sParam.Size.dwPhyBytes;

			if( scanning->sParam.bChannels == 3 ) {

				scanning->dwLinesDiscard = (u_long)scaps->bSensorDistance *
								scanning->sParam.PhyDpi.y / scaps->OpticDpi.y;

				switch( scaps->bSensorOrder ) {

				case SENSORORDER_rgb:
					scanning->Red.pb   = scanning->pbScanBufBegin;
					scanning->Green.pb = scanning->pbScanBufBegin +
										 scanning->dwLinesDiscard *
										 scanning->sParam.Size.dwPhyBytes;
					scanning->Blue.pb  = scanning->pbScanBufBegin +
										 scanning->dwLinesDiscard *
										 scanning->sParam.Size.dwPhyBytes * 2UL;
					break;

				case SENSORORDER_rbg:
					scanning->Red.pb   = scanning->pbScanBufBegin;
					scanning->Blue.pb  = scanning->pbScanBufBegin +
										 scanning->dwLinesDiscard *
										 scanning->sParam.Size.dwPhyBytes;
					scanning->Green.pb = scanning->pbScanBufBegin +
										 scanning->dwLinesDiscard *
										 scanning->sParam.Size.dwPhyBytes * 2UL;
					break;
					
				case SENSORORDER_gbr:
					scanning->Green.pb = scanning->pbScanBufBegin;
					scanning->Blue.pb  = scanning->pbScanBufBegin +
						                 scanning->dwLinesDiscard *
                                         scanning->sParam.Size.dwPhyBytes;
					scanning->Red.pb   = scanning->pbScanBufBegin +
						                 scanning->dwLinesDiscard *
                                         scanning->sParam.Size.dwPhyBytes * 2UL;
					break;
					
				case SENSORORDER_grb:
					scanning->Green.pb = scanning->pbScanBufBegin;
					scanning->Red.pb   = scanning->pbScanBufBegin +
						                 scanning->dwLinesDiscard *
                                         scanning->sParam.Size.dwPhyBytes;
					scanning->Blue.pb  = scanning->pbScanBufBegin +
						                 scanning->dwLinesDiscard *
                                         scanning->sParam.Size.dwPhyBytes * 2UL;
					break;
					
				case SENSORORDER_brg:
					scanning->Blue.pb  = scanning->pbScanBufBegin;
					scanning->Red.pb   = scanning->pbScanBufBegin +
						                 scanning->dwLinesDiscard *
                                         scanning->sParam.Size.dwPhyBytes;
					scanning->Green.pb = scanning->pbScanBufBegin +
						                 scanning->dwLinesDiscard *
                                         scanning->sParam.Size.dwPhyBytes * 2UL;
					break;
					
				case SENSORORDER_bgr:
					if( hw->ScannerModel == MODEL_Tokyo600 ) {
						scanning->Red.pb   = scanning->pbScanBufBegin;
						scanning->Green.pb = scanning->pbScanBufBegin +
							                 scanning->dwLinesDiscard *
                                             scanning->sParam.Size.dwPhyBytes;
						scanning->Blue.pb  = scanning->pbScanBufBegin +
							                 scanning->dwLinesDiscard *
                                        scanning->sParam.Size.dwPhyBytes * 2UL;
					} else {
						scanning->Blue.pb  = scanning->pbScanBufBegin;
						scanning->Green.pb = scanning->pbScanBufBegin +
							                 scanning->dwLinesDiscard *
                                             scanning->sParam.Size.dwPhyBytes;
						scanning->Red.pb   = scanning->pbScanBufBegin +
							                 scanning->dwLinesDiscard *
                                        scanning->sParam.Size.dwPhyBytes * 2UL;
					}
				}

				/* double it for last channel */
				scanning->dwLinesDiscard <<= 1;
				scanning->dwGreenShift = (7UL+scanning->sParam.bBitDepth) >> 3;
				scanning->Green.pb += scanning->dwGreenShift;
				scanning->Blue.pb  += (scanning->dwGreenShift * 2);

				if( scanning->dwFlag & SCANFLAG_bgr) {

					u_char *pb = scanning->Blue.pb;

					scanning->Blue.pb = scanning->Red.pb;
					scanning->Red.pb  = pb;
					scanning->dwBlueShift = 0;
					scanning->dwRedShift  = scanning->dwGreenShift << 1;
				} else {
					scanning->dwRedShift  = 0;
					scanning->dwBlueShift = scanning->dwGreenShift << 1;
				}

			} else {
				scanning->dwLinesDiscard = 0;
				scanning->Green.pb       = scanning->pbScanBufBegin;
			}
		}

		/* set a funtion to process the RAW data... */
		usb_GetImageProc( dev );

		scanning->bLinesToSkip = (u_char)(scanning->sParam.PhyDpi.y / 50);
		
		if( scanning->sParam.bSource == SOURCE_ADF )
			scanning->dwFlag |= SCANFLAG_StillModule;

		if( !usb_ScanBegin( dev,
			(scanning->dwFlag&SCANFLAG_StillModule) ? SANE_FALSE:SANE_TRUE)) {
	
			return _E_INTERNAL;
		}

		scanning->dwFlag |= SCANFLAG_Scanning;

		if( scanning->sParam.UserDpi.y != scanning->sParam.PhyDpi.y ) {
		
			if( scanning->sParam.UserDpi.y < scanning->sParam.PhyDpi.y ) {
			
				scanning->wSumY = scanning->sParam.PhyDpi.y -
								  scanning->sParam.UserDpi.y;
				scanning->dwFlag |= SCANFLAG_SampleY;
				DBG( _DBG_INFO, "SampleY Flag set (%u != %u, wSumY=%u)\n",
								scanning->sParam.UserDpi.y,
								scanning->sParam.PhyDpi.y, scanning->wSumY );
			}								
		}
	}

	/* here the NT driver uses an extra reading thread...
     * as the SANE stuff already forked the driver to read data, I think
     * we should only read data by using a function...
     */
/* HEINER: CHECK THIS!!! */
	scanning->dwLinesUser = scanning->sParam.Size.dwLines; /*lpCB->dwIn /  scanning->dwBytesLine*/;
	if( !scanning->dwLinesUser )
		return _E_BUFFER_TOO_SMALL;

	if( !scanning->sParam.Size.dwLines )
		return _E_NODATA;
	
	if( scanning->sParam.Size.dwLines < scanning->dwLinesUser )
		scanning->dwLinesUser = scanning->sParam.Size.dwLines;

	scanning->sParam.Size.dwLines -= scanning->dwLinesUser;
	if( scanning->dwFlag & SCANFLAG_BottomUp )
		scanning->UserBuf.pb = buf + (scanning->dwLinesUser - 1) *
									  scanning->dwBytesLine;
	else
		scanning->UserBuf.pb = buf;

	DBG(_DBG_INFO,"Reading the data now!\n" );
	DBG(_DBG_INFO,"PhyDpi.x         = %u\n", scanning->sParam.PhyDpi.x  );	
	DBG(_DBG_INFO,"PhyDpi.y         = %u\n", scanning->sParam.PhyDpi.y  );	
	DBG(_DBG_INFO,"UserDpi.x        = %u\n", scanning->sParam.UserDpi.x );	
	DBG(_DBG_INFO,"UserDpi.y        = %u\n", scanning->sParam.UserDpi.y );	
	DBG(_DBG_INFO,"NumberOfScanBufs = %lu\n",scanning->dwNumberOfScanBufs);
	DBG(_DBG_INFO,"LinesPerScanBufs = %lu\n",scanning->dwLinesPerScanBufs);
	DBG(_DBG_INFO,"dwPhyBytes       = %lu\n",scanning->sParam.Size.dwPhyBytes);
	DBG(_DBG_INFO,"dwTotalBytes     = %lu\n",scanning->sParam.Size.dwTotalBytes);
	DBG(_DBG_INFO,"dwPixels         = %lu\n",scanning->sParam.Size.dwPixels);
	DBG(_DBG_INFO,"dwValidPixels    = %lu\n",scanning->sParam.Size.dwValidPixels);
	DBG(_DBG_INFO,"dwBytesScanBuf   = %lu\n",scanning->dwBytesScanBuf );
	DBG(_DBG_INFO,"dwLinesDiscard   = %lu\n",scanning->dwLinesDiscard );
	DBG(_DBG_INFO,"dwLinesToSkip    = %u\n", scanning->bLinesToSkip );
	DBG(_DBG_INFO,"dwLinesUser      = %lu\n",scanning->dwLinesUser  );
	DBG(_DBG_INFO,"dwBytesLine      = %lu\n",scanning->dwBytesLine  );

	scanning->pbGetDataBuf = scanning->pbScanBufBegin;
	
	lines = usb_ReadData( dev  );
	if( 0 == lines )
		return _E_DATAREAD;
		
	/*
     * all settings done, so go ahead and read the data
     */
	for( dw = 0; scanning->dwLinesUser; ) {

		if( usb_IsEscPressed()) {
			DBG( _DBG_INFO, "ReadImage() - Cancel detected...\n" );
			return 0;
		}

		if( !(scanning->dwFlag & SCANFLAG_SampleY))	{

			scanning->pfnProcess( dev );

			/* Adjust user buffer pointer */
			scanning->UserBuf.pb += scanning->lBufAdjust;
			scanning->dwLinesUser--;
			dw++;

		} else {

			scanning->wSumY += scanning->sParam.UserDpi.y;

			if( scanning->wSumY >= scanning->sParam.PhyDpi.y ) {
				scanning->wSumY -= scanning->sParam.PhyDpi.y;
				
				scanning->pfnProcess( dev );
		
				/* Adjust user buffer pointer */
				scanning->UserBuf.pb += scanning->lBufAdjust;
				scanning->dwLinesUser--;
				dw++;
			}
		}

		/* Adjust get buffer pointers */
		if( scanning->sParam.bDataType == SCANDATATYPE_Color ) {

			scanning->Red.pb += scanning->sParam.Size.dwPhyBytes;
			if( scanning->Red.pb >= scanning->pbScanBufEnd )
				scanning->Red.pb = scanning->pbScanBufBegin +
								   scanning->dwRedShift;

			scanning->Green.pb += scanning->sParam.Size.dwPhyBytes;
			if( scanning->Green.pb >= scanning->pbScanBufEnd )
				scanning->Green.pb = scanning->pbScanBufBegin +
									 scanning->dwGreenShift;

			scanning->Blue.pb += scanning->sParam.Size.dwPhyBytes;
			if( scanning->Blue.pb >= scanning->pbScanBufEnd )
				scanning->Blue.pb = scanning->pbScanBufBegin +
									scanning->dwBlueShift;
		} else {
			scanning->Green.pb += scanning->sParam.Size.dwPhyBytes;
			if( scanning->Green.pb >= scanning->pbScanBufEnd )
				scanning->Green.pb = scanning->pbScanBufBegin +
									 scanning->dwGreenShift;
		}

		/*
		 * line processed, check if we have to get more...
		 */		
		lines--;

		if( 0 == lines ) {
		
			lines = usb_ReadData( dev );
			if(( 0 == lines ) && !usb_IsEscPressed())
				break;
		}
	}
	
	DBG( _DBG_INFO, "We've got %lu bytes (%lu lines, needed: %lu bytes)\n",
							 (dw * scanning->dwBytesLine), dw, data_length );
	usb_ScanEnd( dev );

    return data_length; /* (dw * scanning->dwBytesLine);*/
}

/* END PLUSTEK-USB.C ........................................................*/
