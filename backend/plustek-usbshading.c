/*.............................................................................
 * Project : SANE library for Plustek USB flatbed scanners.
 *.............................................................................
 * File:	 plustek-usbshading.c - calibration stuff
 *.............................................................................
 *
 * based on sources acquired from Plustek Inc.
 * Copyright (C) 2001-2002 Gerhard Jaeger <g.jaeger@earthling.net>
 *.............................................................................
 * History:
 * 0.40 - starting version of the USB support
 * 0.41 - minor fixes
 *      - added workaround stuff for EPSON1250
 * 0.42 - added workaround stuff for UMAX 3400
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

/************** global stuff - I hate it, but we need it... ******************/

static u_short a_wWhiteShading[10200 * 3] = {0};
static u_short a_wDarkShading[10200 * 3]  = {0};

/************************** static variables *********************************/

static RGBUShortDef Gain_Hilight;
static RGBUShortDef Gain_NegHilight;
static RGBByteDef   Gain_Reg;

static u_long     dwShadingLines = 64;
static u_long     m_dwPixels;
static u_long     dwPos;

static u_char    *pScanBuffer;
static pScanParam pParam;
static ScanParam  m_ScanParam;
static u_long	  m_dwIdealGain;

static pRGBUShortDef  m_pAvColor;
static u_short       *m_pAvMono;
static u_long        *m_pSum;

static double dMCLK, dExpect, dMax;
static double dMCLK_ADF;
static double dRed, dGreen, dBlue;

static u_short m_wHilight = 0;  /* check the windows registry... */
static u_short m_wShadow  = 0;  /* check the windows registry... */

/*.............................................................................
 *
 */
static Bool usb_SetDarkShading( int fd, u_char channel,
								void *lpCoeff, u_short wCount )
{
	int res;

	a_bRegs[0x03] = 0;
	if( channel == CHANNEL_green )
		a_bRegs[0x03] |= 4;
	else
		if( channel == CHANNEL_blue )
			a_bRegs[0x03] |= 8;

	if( usbio_WriteReg( fd, 0x03, a_bRegs[0x03] )) {

		/* Dataport address is always 0 for setting offset coefficient */
		a_bRegs[0x04] = 0;
		a_bRegs[0x05] = 0;

		res = sanei_lm983x_write( fd, 0x04, &a_bRegs[0x04], 2, SANE_TRUE );

		/* Download offset coefficients */
		if( SANE_STATUS_GOOD == res ) {
			
			res = sanei_lm983x_write(fd, 0x06,
										(u_char*)lpCoeff, wCount, SANE_FALSE );
			if( SANE_STATUS_GOOD == res )
				return SANE_TRUE;
		}
	}
	
	DBG( _DBG_ERROR, "usb_SetDarkShading() failed\n" );
	return SANE_FALSE;
}

/*.............................................................................
 *
 */
static Bool usb_SetWhiteShading( int fd, u_char channel,
								 void *lpData, u_short wCount )
{
	int res;

	a_bRegs [0x03] = 1;
	if (channel == CHANNEL_green)
		a_bRegs [0x03] |= 4;
	else
		if (channel == CHANNEL_blue)
			a_bRegs [0x03] |= 8;

	if( usbio_WriteReg( fd, 0x03, a_bRegs[0x03] )) {

		/* Dataport address is always 0 for setting offset coefficient */
		a_bRegs[0x04] = 0;
		a_bRegs[0x05] = 0;

		res = sanei_lm983x_write( fd, 0x04, &a_bRegs[0x04], 2, SANE_TRUE );

		/* Download offset coefficients */
		if( SANE_STATUS_GOOD == res ) {
			res = sanei_lm983x_write(fd, 0x06,
										(u_char*)lpData, wCount, SANE_FALSE );
			if( SANE_STATUS_GOOD == res )
				return SANE_TRUE;
		}
	}
	
	DBG( _DBG_ERROR, "usb_SetWhiteShading() failed\n" );
	return SANE_FALSE;
}

/*.............................................................................
 *
 */
static void usb_GetSoftwareOffsetGain( pPlustek_Device dev )
{
	pScanParam pParam = &dev->scanning.sParam;
	pDCapsDef  sCaps  = &dev->usbDev.Caps;
	pHWDef     hw     = &dev->usbDev.HwSetting;

	pParam->swOffset[0] = pParam->swOffset[1] = pParam->swOffset[2] = 0;
	pParam->swGain[0]   = pParam->swGain[1] = pParam->swGain[2] = 1000;

	if( pParam->bSource != SOURCE_Reflection )
		return;

	switch( sCaps->bCCD ) {

	case kNEC8861:
		break;

	case kNEC3799:
		if( sCaps->bPCB == 2 ) {
			if( pParam->PhyDpi.x <= 150 ) {
				pParam->swOffset[0] = 600;
				pParam->swOffset[1] = 500;
				pParam->swOffset[2] = 300;
				pParam->swGain[0] = 960;
				pParam->swGain[1] = 970;
				pParam->swGain[2] = 1000;
			} else if (pParam->PhyDpi.x <= 300)	{
				pParam->swOffset[0] = 700;
				pParam->swOffset[1] = 600;
				pParam->swOffset[2] = 400;
				pParam->swGain[0] = 967;
				pParam->swGain[1] = 980;
				pParam->swGain[2] = 1000;
			} else {
				pParam->swOffset[0] = 900;
				pParam->swOffset[1] = 850;
				pParam->swOffset[2] = 620;
				pParam->swGain[0] = 965;
				pParam->swGain[1] = 980;
				pParam->swGain[2] = 1000;
			}
		} else if( hw->ScannerModel == MODEL_KaoHsiung ) {
			pParam->swOffset[0] = 1950;
			pParam->swOffset[1] = 1700;
			pParam->swOffset[2] = 1250;
			pParam->swGain[0] = 955;
			pParam->swGain[1] = 950;
			pParam->swGain[2] = 1000;
		} else { /* MODEL_Hualien */
			if( pParam->PhyDpi.x <= 300 ) {
				if( pParam->bBitDepth > 8 ) {
					pParam->swOffset[0] = 0;
					pParam->swOffset[1] = 0;
					pParam->swOffset[2] = -300;
					pParam->swGain[0] = 970;
					pParam->swGain[1] = 985;
					pParam->swGain[2] = 1050;
				} else {
					pParam->swOffset[0] = -485;
					pParam->swOffset[1] = -375;
					pParam->swOffset[2] = -628;
					pParam->swGain[0] = 970;
					pParam->swGain[1] = 980;
					pParam->swGain[2] = 1050;
				}
			} else {
				if( pParam->bBitDepth > 8 ) {
					pParam->swOffset[0] = 1150;
					pParam->swOffset[1] = 1000;
					pParam->swOffset[2] = 700;
					pParam->swGain[0] = 990;
					pParam->swGain[1] = 1000;
					pParam->swGain[2] = 1050;
				} else {
					pParam->swOffset[0] = -30;
					pParam->swOffset[1] = 0;
					pParam->swOffset[2] = -250;
					pParam->swGain[0] = 985;
					pParam->swGain[1] = 995;
					pParam->swGain[2] = 1050;
				}
			}
		}
		break;

	case kSONY548:
		if(pParam->bDataType == SCANDATATYPE_Color)
		{
			if(pParam->PhyDpi.x <= 75)
			{
				pParam->swOffset[0] = 650;
				pParam->swOffset[1] = 850;
				pParam->swOffset[2] = 500;
				pParam->swGain[0] = 980;
				pParam->swGain[1] = 1004;
				pParam->swGain[2] = 1036;
			}
			else if(pParam->PhyDpi.x <= 300)
			{
				pParam->swOffset[0] = 700;
				pParam->swOffset[1] = 900;
				pParam->swOffset[2] = 550;
				pParam->swGain[0] = 970;
				pParam->swGain[1] = 995;
				pParam->swGain[2] = 1020;
			}
			else if(pParam->PhyDpi.x <= 400)
			{
				pParam->swOffset[0] = 770;
				pParam->swOffset[1] = 1010;
				pParam->swOffset[2] = 600;
				pParam->swGain[0] = 970;
				pParam->swGain[1] = 993;
				pParam->swGain[2] = 1023;
			}
			else
			{
				pParam->swOffset[0] = 380;
				pParam->swOffset[1] = 920;
				pParam->swOffset[2] = 450;
				pParam->swGain[0] = 957;
				pParam->swGain[1] = 980;
				pParam->swGain[2] = 1008;
			}
		}
		else
		{
			if(pParam->PhyDpi.x <= 75)
			{
				pParam->swOffset[1] = 1250;
				pParam->swGain[1] = 950;
			}
			else if(pParam->PhyDpi.x <= 300)
			{
				pParam->swOffset[1] = 1250;
				pParam->swGain[1] = 950;
			}
			else if(pParam->PhyDpi.x <= 400)
			{
				pParam->swOffset[1] = 1250;
				pParam->swGain[1] = 950;
			}
			else
			{
				pParam->swOffset[1] = 1250;
				pParam->swGain[1] = 950;
			}
		}
		break;

	case kNEC3778:
		if((_LM9831 == hw->chip) && (pParam->PhyDpi.x <= 300))
		{
			pParam->swOffset[0] = 0;
			pParam->swOffset[1] = 0;
			pParam->swOffset[2] = 0;
			pParam->swGain[0] = 900;
			pParam->swGain[1] = 920;
			pParam->swGain[2] = 980;
		}
		else if( hw->ScannerModel == MODEL_HuaLien && pParam->PhyDpi.x > 800)
		{
			pParam->swOffset[0] = 0;
			pParam->swOffset[1] = 0;
			pParam->swOffset[2] = -200;
			pParam->swGain[0] = 980;
			pParam->swGain[1] = 930;
			pParam->swGain[2] = 1080;
		}
		else
		{
			pParam->swOffset[0] = -304;
			pParam->swOffset[1] = -304;
			pParam->swOffset[2] = -304;
			pParam->swGain[0] = 910;	
			pParam->swGain[1] = 920;	
			pParam->swGain[2] = 975;
		}
		
		if(pParam->bDataType == SCANDATATYPE_BW && pParam->PhyDpi.x <= 300)
		{
			pParam->swOffset[1] = 1000;
			pParam->swGain[1] = 1000;
		}
		break;
	}
}

/*.............................................................................
 *
 */
static void usb_Swap( u_short *pw, u_long dwBytes )
{
	for( dwBytes /= 2; dwBytes--; pw++ )
		_SWAP(((u_char*) pw)[0], ((u_char*)pw)[1]);
}

/*.............................................................................
 *
 */
static u_char usb_GetNewGain( u_short wMax )
{
	double dRatio, dAmp;
	u_long dwInc, dwDec;
	u_char bGain;

	if( !wMax )
		wMax = 1;

	dAmp = 0.93 + 0.067 * a_bRegs[0x3d];

	if((m_dwIdealGain / (wMax / dAmp)) < 3) {

		dRatio = ((double) m_dwIdealGain * dAmp / wMax - 0.93) / 0.067;
		if(ceil(dRatio) > 0x3f)
			return 0x3f;

		dwInc = (u_long)((0.93 + ceil (dRatio) * 0.067) * wMax / dAmp);
		dwDec = (u_long)((0.93 + floor (dRatio) * 0.067) * wMax / dAmp);
		if((dwInc >= 0xff00) ||
		   (labs (dwInc - m_dwIdealGain) > labs(dwDec - m_dwIdealGain))) {
			bGain = (u_char)floor (dRatio);
		} else {
			bGain = ceil(dRatio);
        }

		if( bGain > 0x3f )
			bGain = 0x3f;

		return bGain;

	} else {

		dRatio = m_dwIdealGain / (wMax / dAmp);
		dAmp   = floor((dRatio / 3 - 0.93)/0.067);

		if( dAmp > 31 )
			dAmp = 31;

		bGain = (u_char)dAmp + 32;
		return bGain;
	}
}

/*.............................................................................
 *
 */
static Bool usb_AdjustGain( pPlustek_Device dev, int fNegative )
{
    pScanDef  scanning = &dev->scanning;
	pDCapsDef scaps    = &dev->usbDev.Caps;
	pHWDef    hw       = &dev->usbDev.HwSetting;
	u_long    dw;
	Bool      fRepeatITA = SANE_TRUE;

	if( usb_IsEscPressed())
		return SANE_FALSE;

	bMaxITA = 0xff;

	m_ScanParam.Size.dwLines  = 1;				/* for gain */
	m_ScanParam.Size.dwPixels = scaps->Normal.Size.x *
								scaps->OpticDpi.x / 300UL;
	m_ScanParam.Size.dwBytes  = m_ScanParam.Size.dwPixels *
								2 * m_ScanParam.bChannels;
	m_ScanParam.Origin.x      = (u_short)((u_long) hw->wActivePixelsStart *
													300UL / scaps->OpticDpi.x);
	m_ScanParam.bCalibration  = PARAM_Gain;

TOGAIN:
	m_ScanParam.dMCLK = dMCLK;

	if( !usb_SetScanParameters( dev, &m_ScanParam ) || (usleep(50*1000),
		!usb_ScanBegin( dev, SANE_FALSE)) ||
		!usb_ScanReadImage( dev, pScanBuffer, m_ScanParam.Size.dwPhyBytes ) ||
		!usb_ScanEnd( dev )) {
		DBG( _DBG_ERROR, "usb_AdjustGain() failed\n" );
		return SANE_FALSE;
	}

	usb_Swap((u_short *)pScanBuffer, m_ScanParam.Size.dwPhyBytes );

	if( fNegative ) {

		if( m_ScanParam.bDataType == SCANDATATYPE_Color ) {

			RGBULongDef rgb, rgbSum;
			u_long		dwLoop = m_ScanParam.Size.dwPhyPixels / 20 * 20;
			u_long		dw10, dwGray, dwGrayMax;

			rgb.Red = rgb.Green = rgb.Blue = dwGrayMax = 0;

			for( dw = 0; dw < dwLoop;) {

				rgbSum.Red = rgbSum.Green = rgbSum.Blue = 0;
				for( dw10 = 20; dw10--; dw++ ) {
					rgbSum.Red   += (u_long)(((pRGBULongDef)pScanBuffer)[dw].Red);
					rgbSum.Green += (u_long)(((pRGBULongDef)pScanBuffer)[dw].Green);
					rgbSum.Blue  += (u_long)(((pRGBULongDef)pScanBuffer)[dw].Blue);
				}					
				dwGray = (rgbSum.Red * 30UL + rgbSum.Green * 59UL + rgbSum.Blue * 11UL) / 100UL;

				if( fNegative == 1 || rgbSum.Red > rgbSum.Green) {
					if( dwGray > dwGrayMax ) {
						dwPos = (u_long)((pRGBUShortDef)pScanBuffer + dw - 20);
						dwGrayMax = dwGray;
						rgb.Red   = rgbSum.Red;
						rgb.Green = rgbSum.Green;
						rgb.Blue  = rgbSum.Blue;
					}
				}
			}

			Gain_Hilight.Red   = (u_short)(rgb.Red / 20UL);
			Gain_Hilight.Green = (u_short)(rgb.Green / 20UL);
			Gain_Hilight.Blue  = (u_short)(rgb.Blue / 20UL);
			a_bRegs[0x3b] = usb_GetNewGain(Gain_Hilight.Red);
			a_bRegs[0x3c] = usb_GetNewGain(Gain_Hilight.Green);
			a_bRegs[0x3d] = usb_GetNewGain(Gain_Hilight.Blue);

		} else {

			u_long dwMax  = 0, dwSum;
			u_long dwLoop = m_ScanParam.Size.dwPhyPixels / 20 * 20;
			u_long dw10;

			for( dw = 0; dw < dwLoop;) {

				dwSum = 0;
				for( dw10 = 20; dw10--; dw++ )
					dwSum += (u_long)((u_short*)pScanBuffer)[dw];

				if((fNegative == 1) || (dwSum < 0x6000 * 20)) {
					if( dwMax < dwSum )
						dwMax = dwSum;
				}
			}
			Gain_Hilight.Red  = Gain_Hilight.Green =
			Gain_Hilight.Blue = (u_short)(dwMax / 20UL);

			Gain_Reg.Red  = Gain_Reg.Green = Gain_Reg.Blue =
			a_bRegs[0x3b] = a_bRegs[0x3c]  = a_bRegs[0x3d] = usb_GetNewGain(Gain_Hilight.Green);
		}
	} else {

		if( m_ScanParam.bDataType == SCANDATATYPE_Color ) {

			RGBUShortDef rgb;
			u_long       dwR, dwG, dwB;
			u_long       dwDiv = 10;
			u_long       dwLoop1 = m_ScanParam.Size.dwPhyPixels / dwDiv, dwLoop2;

			rgb.Red = rgb.Green = rgb.Blue = 0;

			for( dw = 0; dwLoop1; dwLoop1-- ) {

				for (dwLoop2 = dwDiv, dwR = dwG = dwB = 0; dwLoop2; dwLoop2--, dw++)
				{
					dwR += ((pRGBUShortDef)pScanBuffer)[dw].Red;
					dwG += ((pRGBUShortDef)pScanBuffer)[dw].Green;
					dwB += ((pRGBUShortDef)pScanBuffer)[dw].Blue;
				}
				dwR = dwR / dwDiv;
				dwG = dwG / dwDiv;
				dwB = dwB / dwDiv;

				if(rgb.Red < dwR)
					rgb.Red = dwR;
				if(rgb.Green < dwG)
					rgb.Green = dwG;
				if(rgb.Blue < dwB)
					rgb.Blue = dwB;
			}

			m_dwIdealGain = 0xf000; /* min(min(rgb.wRed, rgb.wGreen), rgb.wBlue) */

			a_bRegs[0x3b] = usb_GetNewGain(rgb.Red);
			a_bRegs[0x3c] = usb_GetNewGain(rgb.Green);
			a_bRegs[0x3d] = usb_GetNewGain(rgb.Blue);

			/* for MODEL KaoHsiung 1200 scanner multi-straight-line bug at
             * 1200 dpi color mode
             */
			if( hw->ScannerModel == MODEL_KaoHsiung && scaps->bCCD==kNEC3778 &&
				dMCLK >= 5.5 && !a_bRegs[0x3c] ) {

				a_bRegs[0x3b] = a_bRegs[0x3c] = a_bRegs[0x3d] = 1;
				scanning->sParam.dMCLK = dMCLK = dMCLK - 1.5;
				goto TOGAIN;

			} else if( hw->ScannerModel == MODEL_HuaLien &&
						scaps->bCCD == kNEC3799 && fRepeatITA )	{

				if((!a_bRegs[0x3b] ||
				    !a_bRegs[0x3c] || !a_bRegs[0x3d]) && dMCLK > 3.0) {

					scanning->sParam.dMCLK = dMCLK = dMCLK - 0.5;
					a_bRegs[0x3b] = a_bRegs[0x3c] = a_bRegs[0x3d] = 1;
					goto TOGAIN;

				} else if(((a_bRegs[0x3b] == 63) || (a_bRegs[0x3c] == 63) ||
									  (a_bRegs[0x3d] == 63)) && (dMCLK < 10)) {

					scanning->sParam.dMCLK = dMCLK = dMCLK + 0.5;
					a_bRegs[0x3b] = a_bRegs[0x3c] = a_bRegs[0x3d] = 1;
					goto TOGAIN;
				}
				bMaxITA = (u_char)floor((dMCLK + 1) / 2);
				fRepeatITA = SANE_FALSE;
			}

		} else {
			u_short	w = 0;

			for( dw = 0; dw < m_ScanParam.Size.dwPhyPixels; dw++ ) {
				if( w < ((u_short*)pScanBuffer)[dw])
					w = ((u_short*)pScanBuffer)[dw];
			}
			a_bRegs[0x3b] = a_bRegs[0x3c] = a_bRegs[0x3d] = usb_GetNewGain(w);
		}
	}

	return SANE_TRUE;
}

/*.............................................................................
 *
 */
static void usb_GetNewOffset( u_long *pdwSum, u_long *pdwDiff, char *pcOffset,
					          u_char *pIdeal, u_long dw, char cAdjust )
{
	u_long dwIdealOffset = IDEAL_Offset;

	if( pdwSum[dw] > dwIdealOffset ) {

		/* Over ideal value */
		pdwSum[dw] -= dwIdealOffset;
		if( pdwSum[dw] < pdwDiff[dw] ) {
			/* New offset is better than old one */
			pdwDiff[dw] = pdwSum[dw];
			pIdeal[dw]  = a_bRegs[0x38 + dw];
		}
		pcOffset[dw] -= cAdjust;

	} else 	{

		/* Below than ideal value */
		pdwSum[dw] = dwIdealOffset - pdwSum [dw];
		if( pdwSum[dw] < pdwDiff[dw] ) {
			/* New offset is better than old one */
			pdwDiff[dw] = pdwSum[dw];
			pIdeal[dw]  = a_bRegs[0x38 + dw];
		}
		pcOffset[dw] += cAdjust;
	}

	if( pcOffset[dw] >= 0 )
		a_bRegs[0x38 + dw] = pcOffset[dw];
	else
		a_bRegs[0x38 + dw] = (u_char)(32 - pcOffset[dw]);
}

/*.............................................................................
 *
 */
static Bool usb_AdjustOffset( pPlustek_Device dev )
{
	char   cAdjust = 16;
	char   cOffset[3];
	u_char bExpect[3];
	u_long dw, dwPixels;
    u_long dwDiff[3], dwSum[3];

	pHWDef hw = &dev->usbDev.HwSetting;

	if( usb_IsEscPressed())
		return SANE_FALSE;

	m_ScanParam.Size.dwLines = 1;				/* for gain */
	dwPixels = (u_long)(hw->bOpticBlackEnd - hw->bOpticBlackStart );
	m_ScanParam.Size.dwPixels = 2550;
	m_ScanParam.Size.dwBytes  = m_ScanParam.Size.dwPixels * 2 *
														m_ScanParam.bChannels;
	m_ScanParam.Origin.x = (u_short)((u_long)hw->bOpticBlackStart * 300UL /
												  dev->usbDev.Caps.OpticDpi.x);
	m_ScanParam.bCalibration = PARAM_Offset;
	m_ScanParam.dMCLK        = dMCLK;

	dwDiff[0]  = dwDiff[1]  = dwDiff[2]  = 0xffff;
	cOffset[0] = cOffset[1] = cOffset[2] = 0;
	bExpect[0] = bExpect[1] = bExpect[2] = 0;

	a_bRegs[0x38] = a_bRegs [0x39] = a_bRegs [0x3a] = 0;

	if( !usb_SetScanParameters( dev, &m_ScanParam )) {
		DBG( _DBG_ERROR, "usb_AdjustOffset() failed\n" );
		return SANE_FALSE;
	}
		
	while( cAdjust ) {

		if((!usb_ScanBegin(dev, SANE_FALSE)) ||
		   (!usb_ScanReadImage(dev,pScanBuffer,m_ScanParam.Size.dwPhyBytes)) ||
			!usb_ScanEnd( dev )) {
			DBG( _DBG_ERROR, "usb_AdjustOffset() failed\n" );
			return SANE_FALSE;
		}

		if( m_ScanParam.bDataType == SCANDATATYPE_Color ) {

			dwSum [0] = dwSum [1] = dwSum [2] = 0;

			for (dw = 0; dw < dwPixels; dw++)
			{
				dwSum[0] += (u_long)_HILO2WORD(((pColorWordDef)pScanBuffer)[dw].HiLo[0]);
				dwSum[1] += (u_long)_HILO2WORD(((pColorWordDef)pScanBuffer)[dw].HiLo[1]);
				dwSum[2] += (u_long)_HILO2WORD(((pColorWordDef)pScanBuffer)[dw].HiLo[2]);
			}
			dwSum[0] /= dwPixels;
			dwSum[1] /= dwPixels;
			dwSum[2] /= dwPixels;
			usb_GetNewOffset( dwSum, dwDiff, cOffset, bExpect, 0, cAdjust );
			usb_GetNewOffset( dwSum, dwDiff, cOffset, bExpect, 1, cAdjust );
			usb_GetNewOffset( dwSum, dwDiff, cOffset, bExpect, 2, cAdjust );
		} else {
			dwSum[0] = 0;

			for( dw = 0; dw < dwPixels; dw++ )
				dwSum[0] += (u_long)_HILO2WORD(((pHiLoDef)pScanBuffer)[dw]);
			dwSum [0] /= dwPixels;
			usb_GetNewOffset( dwSum, dwDiff, cOffset, bExpect, 0, cAdjust );
			a_bRegs[0x3a] = a_bRegs[0x39] = a_bRegs[0x38];
		}

		_UIO(sanei_lm983x_write(dev->fd, 0x38, &a_bRegs[0x38], 3, SANE_TRUE));
		cAdjust >>= 1;
	}

	if( m_ScanParam.bDataType == SCANDATATYPE_Color ) {
		a_bRegs[0x38] = bExpect[0];	
		a_bRegs[0x39] = bExpect[1];	
		a_bRegs[0x3a] = bExpect[2];
	} else {
		a_bRegs[0x38] = a_bRegs[0x39] = a_bRegs[0x3a] = bExpect[0];	
	}

	return SANE_TRUE;
}

/*.............................................................................
 *
 */
static void usb_GetDarkShading( pPlustek_Device dev, u_short *pwDest,
								pHiLoDef pSrce, u_long dwPixels,
								u_long dwAdd, int iOffset )
{
	u_long    dw;
	u_long    dwSum[2];
	pDCapsDef scaps = &dev->usbDev.Caps;
	pHWDef    hw    = &dev->usbDev.HwSetting;

/* HEINER: check that */
#if 0
	if(Registry.GetEvenOdd() == 2)
	{
		u_short   w;

		for (dw = 0; dw < dwPixels; dw++, pSrce += dwAdd)
		{
			w = (u_short)((int)_PHILO2WORD(pSrce) + iOffset);
			pwDest[dw] = _LOBYTE(w) * 256 + _HIBYTE(w);
		}
	}
	else
	{
#endif
		dwSum [0] = dwSum [1] = 0;
		if( hw->bSensorConfiguration & 0x04 ) {

			/* Even/Odd CCD */
			for( dw = 0; dw < dwPixels; dw++, pSrce += dwAdd )
				dwSum [dw & 1] += (u_long)_PHILO2WORD(pSrce);
			dwSum [0] /= ((dwPixels + 1UL) >> 1);
			dwSum [1] /= (dwPixels >> 1);

			if( /*Registry.GetEvenOdd() == 1 ||*/ scaps->bPCB == 2)
			{
				dwSum[0] = dwSum[1] = (dwSum[0] + dwSum[1]) / 2;
			}

			dwSum[0] = (int)dwSum[0] + iOffset;
			dwSum[1] = (int)dwSum[1] + iOffset;

			if((int)dwSum[0] < 0)
				dwSum[0] = 0;

			if((int)dwSum[1] < 0)
				dwSum[1] = 0;

			dwSum[0] = (u_long)_LOBYTE(_LOWORD(dwSum[0])) * 256UL +
													_HIBYTE(_LOWORD(dwSum[0]));
			dwSum[1] = (u_long)_LOBYTE(_LOWORD (dwSum[1])) * 256UL +
													_HIBYTE(_LOWORD(dwSum[1]));

			for( dw = 0; dw < dwPixels; dw++ )
				pwDest [dw] = (u_short)dwSum[dw & 1];
		} else {
			
			u_long dwEnd = 0;

			/* Standard CCD */
			for( dw = dwEnd; dw < dwPixels; dw++, pSrce += dwAdd )
				dwSum[0] += (u_long)_PHILO2WORD(pSrce);

			dwSum [0] /= (dwPixels - dwEnd);

			dwSum [0] = (int)dwSum[0] + iOffset;
			if((int)dwSum[0] < 0)
				dwSum [0] = 0;

			dwSum[0] = (u_long)_LOBYTE(_LOWORD(dwSum[0])) * 256UL +
													_HIBYTE(_LOWORD(dwSum[0]));

			for( dw = dwEnd; dw < dwPixels; dw++ )
				pwDest[dw] = (u_short)dwSum [0];
		}
/*	} */
}

/*.............................................................................
 *
 */
static Bool usb_AdjustDarkShading( pPlustek_Device dev )
{
    pScanDef  scanning = &dev->scanning;
	pDCapsDef scaps    = &dev->usbDev.Caps;
	pHWDef    hw       = &dev->usbDev.HwSetting;

	if( usb_IsEscPressed())
		return SANE_FALSE;

	m_ScanParam              = scanning->sParam;
	m_ScanParam.Origin.y     = 0;
	m_ScanParam.UserDpi.y    = scaps->OpticDpi.y;
	m_ScanParam.Size.dwLines = 1;				/* for gain */
	m_ScanParam.bBitDepth    = 16;
	m_ScanParam.bCalibration = PARAM_DarkShading;
	m_ScanParam.dMCLK        = dMCLK;

	if( _LM9831 == hw->chip ) {
		m_ScanParam.UserDpi.x = usb_SetAsicDpiX( dev, m_ScanParam.UserDpi.x);
	 	if( m_ScanParam.UserDpi.x < 100)
			m_ScanParam.UserDpi.x = 150;
			
		/* Now DPI X is physical */
		m_ScanParam.Origin.x      = m_ScanParam.Origin.x %
		                            (u_short)m_dHDPIDivider;
		m_ScanParam.Size.dwPixels = (u_long)scaps->Normal.Size.x *
		                                    m_ScanParam.UserDpi.x / 300UL;
		m_ScanParam.Size.dwBytes  = m_ScanParam.Size.dwPixels *
		                            2UL * m_ScanParam.bChannels;
		
		m_dwPixels = scanning->sParam.Size.dwPixels *
		             m_ScanParam.UserDpi.x / scanning->sParam.UserDpi.x;
	} else {
		m_ScanParam.Size.dwBytes = m_ScanParam.Size.dwPixels *
		                           2 * m_ScanParam.bChannels;
	}

	a_bRegs[0x29] = 0;

	if((!usb_SetScanParameters( dev, &m_ScanParam)) ||
	   (!usb_ScanBegin(dev, SANE_FALSE)) ||
	   (!usb_ScanReadImage(dev,pScanBuffer,m_ScanParam.Size.dwPhyBytes)) ||
	   (!usb_ScanEnd( dev ))) {
		
		if(_WAF_MISC_IO6_LAMP==(_WAF_MISC_IO6_LAMP & scaps->workaroundFlag)) {
			a_bRegs[0x29] = 3;
			a_bRegs[0x5b] = 0x94;
			usbio_WriteReg( dev->fd, 0x5b, a_bRegs[0x5b] );
			
 		} else if( _WAF_MISC_IO3_LAMP ==
 								(_WAF_MISC_IO3_LAMP & scaps->workaroundFlag)) {
 			a_bRegs[0x29] = 3;
 			a_bRegs[0x5a] |= 0x08;
 			usbio_WriteReg( dev->fd, 0x5a, a_bRegs[0x5a] );
		
		} else {
			a_bRegs[0x29] = 1;
		}			
		usbio_WriteReg( dev->fd, 0x29, a_bRegs[0x29] );
		
		DBG( _DBG_ERROR, "usb_AdjustDarkShading() failed\n" );
		return SANE_FALSE;
	}
	
	/*
	 * set illumination mode to 1 or 3 on EPSON
	 */
	if( _WAF_MISC_IO6_LAMP == (_WAF_MISC_IO6_LAMP & scaps->workaroundFlag)) {
		
		a_bRegs[0x29] = 3;
		a_bRegs[0x5b] = 0x94;
		usbio_WriteReg( dev->fd, 0x5b, a_bRegs[0x5b] );

 	} else if( _WAF_MISC_IO3_LAMP ==
	 							(_WAF_MISC_IO3_LAMP & scaps->workaroundFlag)) {
 		a_bRegs[0x29] = 3;
 		a_bRegs[0x5a] |= 0x08;
 		usbio_WriteReg( dev->fd, 0x5a, a_bRegs[0x5a] );
 		
	} else {
		a_bRegs[0x29] = 1;
	}		

	if( !usbio_WriteReg( dev->fd, 0x29, a_bRegs[0x29])) {
		DBG( _DBG_ERROR, "usb_AdjustDarkShading() failed\n" );
		return SANE_FALSE;
	}	

	usleep(500 * 1000);			/* Warm up lamp again */

	if (m_ScanParam.bDataType == SCANDATATYPE_Color)
	{
		usb_GetDarkShading( dev, a_wDarkShading, (pHiLoDef)pScanBuffer,
							m_ScanParam.Size.dwPhyPixels, 3,
												scanning->sParam.swOffset[0]);

		usb_GetDarkShading( dev, a_wDarkShading + m_ScanParam.Size.dwPhyPixels,
							(pHiLoDef)pScanBuffer + 1, m_ScanParam.Size.dwPhyPixels,
							3, scanning->sParam.swOffset[1]);
		usb_GetDarkShading( dev, a_wDarkShading + m_ScanParam.Size.dwPhyPixels * 2,
			               (pHiLoDef)pScanBuffer + 2, m_ScanParam.Size.dwPhyPixels,
							3, scanning->sParam.swOffset[2]);
	} else {

		usb_GetDarkShading( dev, a_wDarkShading, (pHiLoDef)pScanBuffer,
                            m_ScanParam.Size.dwPhyPixels, 1,
						    scanning->sParam.swOffset[1]);

		memcpy( a_wDarkShading + m_ScanParam.Size.dwPhyPixels,
				a_wDarkShading, m_ScanParam.Size.dwPhyPixels * 2 );
		memcpy( a_wDarkShading + m_ScanParam.Size.dwPhyPixels * 2,
				a_wDarkShading, m_ScanParam.Size.dwPhyPixels * 2 );
	}

	a_bRegs[0x45] |= 0x10;

	return SANE_TRUE;		
}

/*.............................................................................
 *
 */
static Bool usb_AdjustWhiteShading( pPlustek_Device dev )
{
    pScanDef     scanning = &dev->scanning;
	pDCapsDef    scaps    = &dev->usbDev.Caps;
	pHWDef       hw       = &dev->usbDev.HwSetting;
	u_char      *pBuf     = pScanBuffer;
	u_long       dw, dwLines, dwRead;
	pMonoWordDef pValue;
	u_long*      pdw;

	m_pAvColor = (pRGBUShortDef)pScanBuffer;
	m_pAvMono  = (u_short*)pScanBuffer;

	if( usb_IsEscPressed())
		return SANE_FALSE;

	if( m_ScanParam.PhyDpi.x > 75)
		dwShadingLines = 64;
	else
		dwShadingLines = 32;

	m_ScanParam           = scanning->sParam;
	m_ScanParam.Origin.y  = 0;
	m_ScanParam.bBitDepth = 16;

/* HEINER: check ADF stuff... */
#if 0
	if( ScanInf.m_fADF ) {
		/* avoid ADF WhiteShading big noise */
		m_ScanParam.UserDpi.y = Device.Caps.OpticDpi.y - 200;
	} else {
#endif
		m_ScanParam.UserDpi.y = scaps->OpticDpi.y;
/*	} */
	m_ScanParam.Size.dwBytes = m_ScanParam.Size.dwPixels * 2 * m_ScanParam.bChannels;
	m_ScanParam.bCalibration = PARAM_WhiteShading;
	m_ScanParam.dMCLK        = dMCLK;


	if( _LM9831 == hw->chip ) {

		m_ScanParam.UserDpi.x = usb_SetAsicDpiX( dev, m_ScanParam.UserDpi.x);
	 	if( m_ScanParam.UserDpi.x < 100 )
			m_ScanParam.UserDpi.x = 150;

		/* Now DPI X is physical */
		m_ScanParam.Origin.x      = m_ScanParam.Origin.x % (u_short)m_dHDPIDivider;
		m_ScanParam.Size.dwPixels = (u_long)scaps->Normal.Size.x * m_ScanParam.UserDpi.x / 300UL;
		m_ScanParam.Size.dwBytes  = m_ScanParam.Size.dwPixels * 2UL * m_ScanParam.bChannels;
		m_dwPixels = scanning->sParam.Size.dwPixels * m_ScanParam.UserDpi.x / scanning->sParam.UserDpi.x;

		dw = (u_long)(hw->wDRAMSize - DRAM_UsedByAsic16BitMode) * 1024UL;
		m_ScanParam.Size.dwLines = dwShadingLines;	/* SHADING_Lines */
		for( dwLines = dw / m_ScanParam.Size.dwBytes;
			 dwLines < m_ScanParam.Size.dwLines; m_ScanParam.Size.dwLines>>=1);
	} else {
		m_ScanParam.Size.dwBytes = m_ScanParam.Size.dwPixels * 2 * m_ScanParam.bChannels;
		m_ScanParam.Size.dwLines = dwShadingLines;	/* SHADING_Lines */
	}


	for( dw = dwShadingLines /*SHADING_Lines*/,
		 dwRead = 0; dw; dw -= m_ScanParam.Size.dwLines ) {

		if( usb_SetScanParameters( dev, &m_ScanParam) &&
			usb_ScanBegin( dev, SANE_FALSE )) {

			if( _LM9831 == hw->chip ) {
				/* Delay for white shading hold for 9831-1200 scanner */
				usleep(250 * 10000);	
			}

			if( usb_ScanReadImage( dev, pBuf + dwRead,
								   m_ScanParam.Size.dwTotalBytes)) {

				if( _LM9831 == hw->chip ) {
					/* Delay for white shading hold for 9831-1200 scanner */
					usleep(10 * 1000);	
				}

				if( usb_ScanEnd( dev )) {
					dwRead += m_ScanParam.Size.dwTotalBytes;
					continue;
				}
			}
		}
		
		DBG( _DBG_ERROR, "usb_AdjustWhiteShading() failed\n" );
		return SANE_FALSE;
	}

	/* Simulate the old program */
	m_pSum = (u_long*)(pBuf + m_ScanParam.Size.dwPhyBytes *
											dwShadingLines/*SHADING_Lines*/);

	if( _LM9831 == hw->chip ) {
		
		u_short *pwDest = (u_short*)pBuf;
		pHiLoDef pwSrce = (pHiLoDef)pBuf;

		pwSrce  += ((u_long)(scanning->sParam.Origin.x-m_ScanParam.Origin.x) /
					(u_short)m_dHDPIDivider) *
						(scaps->OpticDpi.x / 300UL) * m_ScanParam.bChannels;

		for( dwLines = dwShadingLines /*SHADING_Lines*/; dwLines; dwLines--) {

			for( dw = 0; dw < m_dwPixels * m_ScanParam.bChannels; dw++ )
				pwDest[dw] = _HILO2WORD( pwSrce[dw] );

			pwDest += (u_long)m_dwPixels * m_ScanParam.bChannels;
			pwSrce  = (pHiLoDef)((u_char*)pwSrce + m_ScanParam.Size.dwPhyBytes);
		}

		_SWAP(m_ScanParam.Size.dwPhyPixels, m_dwPixels);
	} else {
		/* Discard the status word and conv. the hi-lo order to intel format */
		u_short *pwDest = (u_short*)pBuf;
		pHiLoDef pwSrce = (pHiLoDef)pBuf;

		for( dwLines = dwShadingLines/*SHADING_Lines*/; dwLines; dwLines-- ) {

			for( dw = 0; dw < m_ScanParam.Size.dwPhyPixels * m_ScanParam.bChannels; dw++)
				pwDest[dw] = _HILO2WORD( pwSrce[dw] );

			pwDest += m_ScanParam.Size.dwPhyPixels * m_ScanParam.bChannels;
			pwSrce = (pHiLoDef)((u_char*)pwSrce + m_ScanParam.Size.dwPhyBytes);
		}
	}

	if( scanning->sParam.bDataType == SCANDATATYPE_Color ) {

		u_long*       pdwR = (u_long*)m_pSum;
		u_long*       pdwG = pdwR + m_ScanParam.Size.dwPhyPixels;
		u_long*       pdwB = pdwG + m_ScanParam.Size.dwPhyPixels;
		pRGBUShortDef pw, pwRGB;
		u_short       w, wR, wG, wB;

		memset(m_pSum, 0, m_ScanParam.Size.dwPhyPixels * 4UL * 3UL);

		/* Sort hilight */
		if( m_wHilight ) {

			for( dwLines = m_wHilight,
				 pwRGB = m_pAvColor + m_ScanParam.Size.dwPhyPixels * dwLines;
				 dwLines < dwShadingLines/*SHADING_Lines*/;
				 		   dwLines++, pwRGB += m_ScanParam.Size.dwPhyPixels ) {
				for( dw = 0; dw < m_ScanParam.Size.dwPhyPixels; dw++ ) {
					pw = m_pAvColor;
					wR = pwRGB[dw].Red;
					wG = pwRGB[dw].Green;
					wB = pwRGB[dw].Blue;

					for( w = 0; w < m_wHilight;
						 w++, pw += m_ScanParam.Size.dwPhyPixels ) {
						if( wR > pw[dw].Red )
							_SWAP( wR, pw[dw].Red );

						if( wG > pw[dw].Green )
							_SWAP( wG, pw[dw].Green );

						if( wB > pw[dw].Blue )
							_SWAP( wB, pw[dw].Blue );
					}
					pwRGB[dw].Red   = wR;
					pwRGB[dw].Green = wG;
					pwRGB[dw].Blue  = wB;
				}
			}
		}

		/* Sort shadow */
		if( m_wShadow )	{

			for( dwLines = m_wHilight,
				 pwRGB = m_pAvColor + m_ScanParam.Size.dwPhyPixels * dwLines;
			 	 dwLines < (dwShadingLines/*SHADING_Lines*/ - m_wShadow);
							dwLines++, pwRGB += m_ScanParam.Size.dwPhyPixels) {

				for (dw = 0; dw < m_ScanParam.Size.dwPhyPixels; dw++) {
				
					pw = m_pAvColor + (dwShadingLines/*SHADING_Lines*/ -
									m_wShadow) * m_ScanParam.Size.dwPhyPixels;
					wR = pwRGB[dw].Red;
					wG = pwRGB[dw].Green;
					wB = pwRGB[dw].Blue;
					
					for( w = 0; w < m_wShadow;
						 w++, pw += m_ScanParam.Size.dwPhyPixels ) {
						if( wR < pw[dw].Red )
							_SWAP( wR, pw[dw].Red );
						if( wG < pw[dw].Green )
							_SWAP( wG, pw [dw].Green );
						if( wB > pw[dw].Blue )
							_SWAP( wB, pw[dw].Blue );
					}
					pwRGB [dw].Red   = wR;
					pwRGB [dw].Green = wG;
					pwRGB [dw].Blue  = wB;
				}
			}
		}

		/* Sum */
		for( dwLines = m_wHilight,
			 pwRGB = (pRGBUShortDef)
						(m_pAvColor + m_ScanParam.Size.dwPhyPixels * dwLines);
			 dwLines < (dwShadingLines/*SHADING_Lines*/ - m_wShadow);
			 dwLines++, pwRGB += m_ScanParam.Size.dwPhyPixels ) {

			for( dw = 0; dw < m_ScanParam.Size.dwPhyPixels; dw++ ) {
				pdwR[dw] += pwRGB[dw].Red;
				pdwG[dw] += pwRGB[dw].Green;
				pdwB[dw] += pwRGB[dw].Blue;
			}
		}

		/* simulate old program */
		pValue = (pMonoWordDef)a_wWhiteShading;
		pdw    = pdwR;

		/* Software gain */
		if( scanning->sParam.bSource != SOURCE_Negative ) {
			
			int i;

			for( i = 0; i < 3; i++ ) {

				for( dw = m_ScanParam.Size.dwPhyPixels;
												dw; dw--, pValue++, pdw++) {
					*pdw = *pdw * 1000 / ((dwShadingLines/*SHADING_Lines*/ -
							m_wHilight-m_wShadow) * scanning->sParam.swGain[i]);
					if(*pdw > 65535U)
						pValue->Mono = 65535U;
					else
						pValue->Mono = (u_short)*pdw;
					
					if (pValue->Mono > 16384U)
						pValue->Mono = (u_short)(GAIN_Target * 16384U / pValue->Mono);
					else
						pValue->Mono = GAIN_Target;
					_SWAP(pValue->HiLo.bHi, pValue->HiLo.bLo);
				}
			}
		} else {
			for( dw = m_ScanParam.Size.dwPhyPixels * 3; dw; dw--, pValue++, pdw++)
				pValue->Mono = (u_short)(*pdw / (dwShadingLines/*SHADING_Lines*/ - m_wHilight - m_wShadow));
		}			
	} else {

		/* gray mode */
		u_short *pwAv, *pw;
		u_short  w, wV;

		memset( m_pSum, 0, m_ScanParam.Size.dwPhyPixels << 2 );
		if( m_wHilight ) {
			for( dwLines = m_wHilight,
				 pwAv = m_pAvMono + m_ScanParam.Size.dwPhyPixels * dwLines;
				 dwLines < dwShadingLines/*SHADING_Lines*/;
              				 dwLines++, pwAv += m_ScanParam.Size.dwPhyPixels) {

				for( dw = 0; dw < m_ScanParam.Size.dwPhyPixels; dw++ ) {

					pw = m_pAvMono;
					wV = pwAv [dw];
					for( w = 0; w < m_wHilight; w++,
						 pw += m_ScanParam.Size.dwPhyPixels ) {
						if( wV > pw[dw] )
							_SWAP( wV, pw[dw] );
					}
					pwAv[dw] = wV;
				}
			}
		}

		/* Sort shadow */
		if (m_wShadow)
		{
			for (dwLines = m_wHilight, pwAv = m_pAvMono + m_ScanParam.Size.dwPhyPixels * dwLines;
			 	 dwLines < (dwShadingLines/*SHADING_Lines*/ - m_wShadow); dwLines++, pwAv += m_ScanParam.Size.dwPhyPixels)
				for (dw = 0; dw < m_ScanParam.Size.dwPhyPixels; dw++)
				{
					pw = m_pAvMono + (dwShadingLines/*SHADING_Lines*/ - m_wShadow) * m_ScanParam.Size.dwPhyPixels;
					wV = pwAv [dw];
					for (w = 0; w < m_wShadow; w++, pw += m_ScanParam.Size.dwPhyPixels)
						if (wV < pw [dw])
							_SWAP (wV, pw [dw]);
					pwAv [dw] = wV;
				}
		}

		/* Sum */
		pdw = (u_long*)m_pSum; /* HEINER: check this */

		for (dwLines = m_wHilight, pwAv = m_pAvMono + m_ScanParam.Size.dwPhyPixels * dwLines;
			 dwLines < (dwShadingLines/*SHADING_Lines*/ - m_wShadow); dwLines++, pwAv += m_ScanParam.Size.dwPhyPixels)
			for (dw = 0; dw < m_ScanParam.Size.dwPhyPixels; dw++)
				pdw[dw] += pwAv [dw];

		/* Software gain */
		pValue = (pMonoWordDef)a_wWhiteShading;
		if( scanning->sParam.bSource != SOURCE_Negative ) {

			for( dw = 0; dw < m_ScanParam.Size.dwPhyPixels; dw++) {
				pdw[dw] = pdw [dw] * 1000 / ((dwShadingLines/*SHADING_Lines*/ -
						 m_wHilight - m_wShadow) * scanning->sParam.swGain[1]);
				if( pdw[dw] > 65535U )
					pValue[dw].Mono = 65535;
				else
					pValue[dw].Mono = (u_short)pdw[dw];

				if( pValue[dw].Mono > 16384U ) {
					pValue[dw].Mono = (u_short)(GAIN_Target * 16384U / pValue[dw].Mono);
					_SWAP(pValue[dw].HiLo.bHi, pValue[dw].HiLo.bLo);
				} else
					pValue[dw].Mono = GAIN_Target;
			}
		} else{
			for( dw = 0; dw < m_ScanParam.Size.dwPhyPixels; dw++ )
				pValue[dw].Mono = (u_short)(pdw[dw] /
  				   (dwShadingLines/*SHADING_Lines*/ - m_wHilight - m_wShadow));
		}
	}

	if( hw->ScannerModel != MODEL_Tokyo600 ) {
		usb_ModuleMove( dev, MOVE_Forward, hw->wMotorDpi / 5 );
		usb_ModuleToHome( dev, SANE_TRUE );
	}


	if( scanning->sParam.bSource == SOURCE_ADF ) {

		if( scaps->bCCD == kNEC3778 )
			usb_ModuleMove( dev, MOVE_Forward, 1000 );

		else /* if( scaps->bCCD == kNEC3799) */
			usb_ModuleMove( dev, MOVE_Forward, 3 * 300 + 38 );

		usb_MotorOn( dev->fd, SANE_FALSE );
	}

	return SANE_TRUE;
}

/*.............................................................................
 * for negative film only
 */
static void usb_ResizeWhiteShading( double dAmp, u_short *pwShading, int iGain )
{
	u_long  dw, dwAmp;
	u_short	w;

	for( dw = 0; dw < m_ScanParam.Size.dwPhyPixels; dw++ ) {
		dwAmp = (u_long)(GAIN_Target * 0x4000 /
									(pwShading[dw] + 1) * dAmp) * iGain / 1000;
		if( dwAmp <= GAIN_Target)
			w = (u_short)dwAmp;
		else
			w = GAIN_Target;
		pwShading[dw] = (u_short)_LOBYTE(w) * 256 + _HIBYTE(w);
	}
}

/*.............................................................................
 *
 */
static int usb_DoCalibration( pPlustek_Device dev )
{
    pScanDef  scanning = &dev->scanning;
	pDCapsDef scaps    = &dev->usbDev.Caps;
	pHWDef    hw       = &dev->usbDev.HwSetting;

	DBG( _DBG_INFO, "usb_DoCalibration()\n" );

	if( SANE_TRUE == scanning->fCalibrated )
		return 0;

	usb_GetSoftwareOffsetGain( dev );

	pScanBuffer = scanning->pScanBuffer;
	pParam      = &scanning->sParam;

	memset( &m_ScanParam, 0, sizeof(ScanParam));

	m_ScanParam.UserDpi   = scaps->OpticDpi;
	m_ScanParam.bChannels = scanning->sParam.bChannels;
	m_ScanParam.bBitDepth = 16;
	m_ScanParam.bSource   = scanning->sParam.bSource;
	m_ScanParam.Origin.y = 0;
	if( scanning->sParam.bDataType == SCANDATATYPE_Color )
		m_ScanParam.bDataType = SCANDATATYPE_Color;
	else
		m_ScanParam.bDataType = SCANDATATYPE_Gray;

	a_bRegs[0x38] = a_bRegs[0x39] = a_bRegs[0x3a] = 0;
	a_bRegs[0x3b] = a_bRegs[0x3c] = a_bRegs[0x3d] = 1;
	a_bRegs[0x45] &= ~0x10;

	/* Go to shading position
	 * Wait for function implemented
     */
	DBG( _DBG_INFO, "goto shading position\n" );

	/* HEINER: Currently not clear why Plustek didn't use the ShadingOriginY
	 *         for all modes
	 */	
#if 1	
	if( scanning->sParam.bSource == SOURCE_Negative ) {

		DBG( _DBG_INFO, "DataOrigin.x=%u, DataOrigin.y=%u\n",
		 dev->usbDev.pSource->DataOrigin.x, dev->usbDev.pSource->DataOrigin.y);
		if(!usb_ModuleMove( dev, MOVE_Forward,
							              (dev->usbDev.pSource->DataOrigin.y +
  										   dev->usbDev.pSource->Size.y / 2))) {
			return _E_LAMP_NOT_IN_POS;
		}

	} else {
#endif
		DBG( _DBG_INFO, "ShadingOriginY=%lu\n",
					(u_long)dev->usbDev.pSource->ShadingOriginY );

		if((hw->ScannerModel == MODEL_HuaLien) && (scaps->OpticDpi.x == 600)) {
			if (!usb_ModuleMove(dev, MOVE_ToShading,
								(u_long)dev->usbDev.pSource->ShadingOriginY)) {
				return _E_LAMP_NOT_IN_POS;
			}
		} else {
			if( !usb_ModuleMove(dev, MOVE_Forward,
								(u_long)dev->usbDev.pSource->ShadingOriginY)) {
				return _E_LAMP_NOT_IN_POS;
			}
		}
	}

	DBG( _DBG_INFO, "shading position reached\n" );

	switch( scanning->sParam.bSource ) {

		case SOURCE_Negative:
			DBG( _DBG_INFO, "NEGATIVE Shading\n" );
			m_dwIdealGain = IDEAL_GainNormal;
			if( dev->usbDev.Caps.OpticDpi.x == 600 )
				dMCLK = 7;
			else
				dMCLK = 8;
				
			for(;;) {
				if( usb_AdjustGain( dev, 2)) {
					if( a_bRegs[0x3b] && a_bRegs[0x3c] && a_bRegs[0x3d]) {
						break;
					} else {
						a_bRegs[0x3b] = a_bRegs[0x3c] = a_bRegs[0x3d] = 1;
						dMCLK--;
					}
				} else {
					return _E_LAMP_NOT_STABLE;
				}
			}
			scanning->sParam.dMCLK = dMCLK;
			Gain_Reg.Red    = a_bRegs[0x3b];
			Gain_Reg.Green  = a_bRegs[0x3c];
			Gain_Reg.Blue   = a_bRegs[0x3d];
			Gain_NegHilight = Gain_Hilight;
#if 1
			if( !usb_ModuleMove( dev, MOVE_Backward,
								 dev->usbDev.pSource->DataOrigin.y +
								 dev->usbDev.pSource->Size.y / 2 -
								 dev->usbDev.pSource->ShadingOriginY)) {
				return _E_LAMP_NOT_IN_POS;
			}
#endif
			a_bRegs[0x45] &= ~0x10;
		
			a_bRegs[0x3b] = a_bRegs[0x3c] = a_bRegs[0x3d] = 1;
			
			if(!usb_AdjustGain( dev, 1 ))
				return _E_INTERNAL;
				
			a_bRegs[0x3b] = a_bRegs[0x3c] = a_bRegs[0x3d] = 1;

			DBG( _DBG_INFO, "Settings done, so start...\n" );
			if( !usb_AdjustOffset(dev) || !usb_AdjustDarkShading(dev) ||
										  !usb_AdjustWhiteShading(dev)) {
				return _E_INTERNAL;
			}

			dRed    = 0.93 + 0.067 * Gain_Reg.Red;
			dGreen  = 0.93 + 0.067 * Gain_Reg.Green;
			dBlue   = 0.93 + 0.067 * Gain_Reg.Blue;
			dExpect = 2.85;
			if( dBlue >= dGreen && dBlue >= dRed )
				dMax = dBlue;
			else
				if( dGreen >= dRed && dGreen >= dBlue )
					dMax = dGreen;
				else
					dMax = dRed;

			dMax = dExpect / dMax;
			dRed   *= dMax;
			dGreen *= dMax;
			dBlue  *= dMax;

			if( m_ScanParam.bDataType == SCANDATATYPE_Color ) {
				usb_ResizeWhiteShading(dRed, a_wWhiteShading, scanning->sParam.swGain[0]);
				usb_ResizeWhiteShading(dGreen, a_wWhiteShading + m_ScanParam.Size.dwPhyPixels, scanning->sParam.swGain[1]);
				usb_ResizeWhiteShading(dBlue, a_wWhiteShading + m_ScanParam.Size.dwPhyPixels * 2, scanning->sParam.swGain[2]);
			}
			break;

		case SOURCE_ADF:
			DBG( _DBG_INFO, "ADF Shading\n" );
			m_dwIdealGain = IDEAL_GainPositive;
			if( scanning->sParam.bDataType  == SCANDATATYPE_BW ) {
				if( scanning->sParam.PhyDpi.x <= 200 ) {
					scanning->sParam.dMCLK = 4.5;
					dMCLK = 4.0;
				} else if ( scanning->sParam.PhyDpi.x <= 300 ) {
					scanning->sParam.dMCLK = 4.0;
					dMCLK = 3.5;
				} else if( scanning->sParam.PhyDpi.x <= 400 ) {
					scanning->sParam.dMCLK = 5.0;
					dMCLK = 4.0;
				} else {
					scanning->sParam.dMCLK = 6.0;
					dMCLK = 4.0;
				}
			} else { /* Gray */

				if( scanning->sParam.PhyDpi.x <= 400 ) {
					scanning->sParam.dMCLK = 6.0;
					dMCLK = 4.5;
				} else {
					scanning->sParam.dMCLK = 9.0;
					dMCLK = 7.0;
				}
			}

			dMCLK_ADF = dMCLK;

			DBG( _DBG_INFO, "Settings done, so start...\n" );
			if( !usb_AdjustGain(dev, 0)     || !usb_AdjustOffset(dev) ||
				!usb_AdjustDarkShading(dev) || !usb_AdjustWhiteShading(dev)) {
				return _E_INTERNAL;
			}
			break;

		case SOURCE_Transparency:
			DBG( _DBG_INFO, "TRANSPARENCY Shading\n" );
			m_dwIdealGain = IDEAL_GainPositive;
			if( dev->usbDev.Caps.OpticDpi.x == 600 ) {
				scanning->sParam.dMCLK = 8;	
				dMCLK = 4;
			} else {
				if( dev->usbDev.Caps.bPCB == 0x06 )
					scanning->sParam.dMCLK = 8;
				else
					scanning->sParam.dMCLK = 8;
				dMCLK = 6;
			}

			DBG( _DBG_INFO, "Settings done, so start...\n" );
			if( !usb_AdjustGain(dev, 0)     || !usb_AdjustOffset(dev) ||
				!usb_AdjustDarkShading(dev) || !usb_AdjustWhiteShading(dev)) {
				return _E_INTERNAL;
			}
			break;

		default:
			if( dev->usbDev.Caps.OpticDpi.x == 600 ) {
				DBG( _DBG_INFO, "Default Shading (600dpi)\n" );
				
				if( MODEL_NOPLUSTEK == hw->ScannerModel ) {
					DBG( _DBG_INFO, "No Plustek model\n" );
					
					if( scanning->sParam.PhyDpi.x > 300 )
						scanning->sParam.dMCLK = dMCLK = ((scanning->sParam.bDataType == SCANDATATYPE_Color)? 3: 9);
					else
						scanning->sParam.dMCLK = dMCLK = ((scanning->sParam.bDataType == SCANDATATYPE_Color)? 2: 6);
						
				} else if( dev->usbDev.Caps.bCCD == kSONY548 )	{

					DBG( _DBG_INFO, "CCD - SONY548\n" );
					if( scanning->sParam.PhyDpi.x <= 75 ) {
						if( scanning->sParam.bDataType  == SCANDATATYPE_Color )
							scanning->sParam.dMCLK = dMCLK = 2.5;
						else if(scanning->sParam.bDataType  == SCANDATATYPE_Gray)
							scanning->sParam.dMCLK = dMCLK = 7.0;
						else
							scanning->sParam.dMCLK = dMCLK = 7.0;

					} else if( scanning->sParam.PhyDpi.x <= 300 ) {
						if( scanning->sParam.bDataType  == SCANDATATYPE_Color )
							scanning->sParam.dMCLK = dMCLK = 3.0;
						else if(scanning->sParam.bDataType  == SCANDATATYPE_Gray)
							scanning->sParam.dMCLK = dMCLK = 6.0;
						else  {
							if( scanning->sParam.PhyDpi.x <= 100 )
								scanning->sParam.dMCLK = dMCLK = 6.0;
							else if( scanning->sParam.PhyDpi.x <= 200 )
								scanning->sParam.dMCLK = dMCLK = 5.0;
							else
								scanning->sParam.dMCLK = dMCLK = 4.5;
						}
					} else if( scanning->sParam.PhyDpi.x <= 400 ) {
						if( scanning->sParam.bDataType  == SCANDATATYPE_Color )
							scanning->sParam.dMCLK = dMCLK = 4.0;
						else if( scanning->sParam.bDataType  == SCANDATATYPE_Gray )
							scanning->sParam.dMCLK = dMCLK = 6.0;
						else
							scanning->sParam.dMCLK = dMCLK = 4.0;
					} else {
						if(scanning->sParam.bDataType  == SCANDATATYPE_Color)
							scanning->sParam.dMCLK = dMCLK = 6.0;
						else if(scanning->sParam.bDataType  == SCANDATATYPE_Gray)
							scanning->sParam.dMCLK = dMCLK = 7.0;
						else
							scanning->sParam.dMCLK = dMCLK = 6.0;
					}

				} else if( dev->usbDev.Caps.bPCB == 0x02 ) {
					DBG( _DBG_INFO, "PCB - 0x02\n" );
					if( scanning->sParam.PhyDpi.x > 300 )
						scanning->sParam.dMCLK = dMCLK = ((scanning->sParam.bDataType  == SCANDATATYPE_Color)? 6: 16);
					else if( scanning->sParam.PhyDpi.x > 150 )
						scanning->sParam.dMCLK = dMCLK = ((scanning->sParam.bDataType  == SCANDATATYPE_Color)?4.5: 13.5);
					else
						scanning->sParam.dMCLK = dMCLK = ((scanning->sParam.bDataType  == SCANDATATYPE_Color)?3: 8);

				}  else if( dev->usbDev.Caps.bButtons ) { /* with lens Shading piece (with gobo) */

					DBG( _DBG_INFO, "CAPS - Buttons\n" );
					scanning->sParam.dMCLK = dMCLK = ((scanning->sParam.bDataType  == SCANDATATYPE_Color)?3: 6);
					if( dev->usbDev.HwSetting.ScannerModel == MODEL_KaoHsiung )	{
						if( dev->usbDev.Caps.bCCD == kNEC3799 ) {
							if( scanning->sParam.PhyDpi.x > 300 )
								scanning->sParam.dMCLK = dMCLK = ((scanning->sParam.bDataType  == SCANDATATYPE_Color)? 6: 13);
							else if(scanning->sParam.PhyDpi.x > 150)
								scanning->sParam.dMCLK = dMCLK = ((scanning->sParam.bDataType  == SCANDATATYPE_Color)?4.5: 13.5);
							else
								scanning->sParam.dMCLK = dMCLK = ((scanning->sParam.bDataType  == SCANDATATYPE_Color)?3: 6);
						} else {
							scanning->sParam.dMCLK = dMCLK = ((scanning->sParam.bDataType  == SCANDATATYPE_Color)?3: 6);
						}
					} else { /* ScannerModel == MODEL_Hualien */
						/*  IMPORTANT !!!!
						 * for Hualien 600 dpi scanner big noise
                         */
						hw->wLineEnd = 5384;
						if(scanning->sParam.bDataType  == SCANDATATYPE_Color &&
							((scanning->sParam.bBitDepth == 8 && (scanning->sParam.PhyDpi.x == 200 || scanning->sParam.PhyDpi.x == 300))))
							hw->wLineEnd = 7000;
						a_bRegs[0x20] = _HIBYTE(hw->wLineEnd);
						a_bRegs[0x21] = _LOBYTE(hw->wLineEnd);

						if( scanning->sParam.PhyDpi.x > 300 ) {
							if (scanning->sParam.bBitDepth > 8)
								scanning->sParam.dMCLK = dMCLK = ((scanning->sParam.bDataType  == SCANDATATYPE_Color)? 5: 13);
							else
								scanning->sParam.dMCLK = dMCLK = ((scanning->sParam.bDataType == SCANDATATYPE_Color)? 6: 13);
						} else {
							if( scanning->sParam.bBitDepth > 8 )
								scanning->sParam.dMCLK = dMCLK = ((scanning->sParam.bDataType == SCANDATATYPE_Color)? 5: 13);
							else
								scanning->sParam.dMCLK = dMCLK = ((scanning->sParam.bDataType == SCANDATATYPE_Color)?3: 6);
						}
					}
				} else  { /* without lens Shading piece (without gobo) - Model U12 only */
					DBG( _DBG_INFO, "Default trunc (U12)\n" );
					if( scanning->sParam.PhyDpi.x > 300 )
						scanning->sParam.dMCLK = dMCLK = ((scanning->sParam.bDataType == SCANDATATYPE_Color)? 3: 9);
					else
						scanning->sParam.dMCLK = dMCLK = ((scanning->sParam.bDataType == SCANDATATYPE_Color)? 2: 6);
				}
			} else {	/* Device.Caps.OpticDpi.x == 1200 */

				DBG( _DBG_INFO, "Default Shading (1200dpi)\n" );
				if( scanning->sParam.bDataType != SCANDATATYPE_Color ) {
					if( scanning->sParam.PhyDpi.x > 300 )
						scanning->sParam.dMCLK = dMCLK = 6.0;
					else {
						scanning->sParam.dMCLK = dMCLK = 5.0;
						a_bRegs[0x0a] = 1;
					}
				} else {
					if( scanning->sParam.PhyDpi.x <= 300)
						scanning->sParam.dMCLK = dMCLK = 2.0;
					else if( scanning->sParam.PhyDpi.x <= 800 )
						scanning->sParam.dMCLK = dMCLK = 4.0;
					else
						scanning->sParam.dMCLK = dMCLK = 5.5;
				}
			}

 			if (m_ScanParam.bSource == SOURCE_ADF)
				m_dwIdealGain = IDEAL_GainPositive;
			else
				m_dwIdealGain = IDEAL_GainNormal;

			DBG( _DBG_INFO, "Settings done, so start...\n" );
			if( !usb_AdjustGain(dev, 0)     || !usb_AdjustOffset(dev) ||
				!usb_AdjustDarkShading(dev) || !usb_AdjustWhiteShading(dev)) {
				DBG( _DBG_ERROR, "Shading failed!!!\n" );
				return _E_INTERNAL;
			}
			break;
	}

	scanning->fCalibrated = SANE_TRUE;
	DBG( _DBG_INFO, "Calibration done\n-----------------------\n" );

	return SANE_TRUE;
}

/*.............................................................................
 *
 */
static Bool usb_DownloadShadingData( pPlustek_Device dev, u_char bJobID )
{
	pHWDef hw = &dev->usbDev.HwSetting;

	switch( bJobID ) {

		case PARAM_WhiteShading:
			if( m_ScanParam.bDataType == SCANDATATYPE_Color ) {

				usb_SetDarkShading( dev->fd, CHANNEL_red, a_wDarkShading,
									(u_short)m_ScanParam.Size.dwPhyPixels * 2);
				usb_SetDarkShading( dev->fd, CHANNEL_green, a_wDarkShading +
									m_ScanParam.Size.dwPhyPixels,
							 	    (u_short)m_ScanParam.Size.dwPhyPixels * 2);
				usb_SetDarkShading( dev->fd, CHANNEL_blue, a_wDarkShading +
									m_ScanParam.Size.dwPhyPixels * 2,
								    (u_short)m_ScanParam.Size.dwPhyPixels * 2);
			} else {
				usb_SetDarkShading( dev->fd, CHANNEL_green, a_wDarkShading +
									m_ScanParam.Size.dwPhyPixels,
								    (u_short)m_ScanParam.Size.dwPhyPixels * 2);
			}
			a_bRegs[0x40] = 0x40;
			a_bRegs[0x41] = 0x00;
			a_bRegs[0x42] = (u_char)(( hw->wDRAMSize > 512)? 0x64: 0x24);

			_UIO(sanei_lm983x_write( dev->fd, 0x40,
									 &a_bRegs[0x40], 0x42-0x40+1, SANE_TRUE ));
			break;

		case PARAM_Scan:
			{
				if( _LM9831 != hw->chip )
					m_dwPixels = m_ScanParam.Size.dwPhyPixels;

				/* Download the dark & white shadings to LM9831 */
				if( pParam->bDataType == SCANDATATYPE_Color ) {
					usb_SetDarkShading( dev->fd, CHANNEL_red, a_wDarkShading,
									(u_short)m_ScanParam.Size.dwPhyPixels * 2);
					usb_SetDarkShading( dev->fd, CHANNEL_green,
									a_wDarkShading + m_dwPixels,
									(u_short)m_ScanParam.Size.dwPhyPixels * 2);
					usb_SetDarkShading( dev->fd, CHANNEL_blue,
										a_wDarkShading + m_dwPixels * 2,
									(u_short)m_ScanParam.Size.dwPhyPixels * 2);
				} else {
					usb_SetDarkShading( dev->fd, CHANNEL_green,
										a_wDarkShading + m_dwPixels,
									(u_short)m_ScanParam.Size.dwPhyPixels * 2);
				}

				if( pParam->bDataType == SCANDATATYPE_Color ) {
					usb_SetWhiteShading( dev->fd, CHANNEL_red, a_wWhiteShading,
									(u_short)m_ScanParam.Size.dwPhyPixels * 2);
					usb_SetWhiteShading( dev->fd, CHANNEL_green,
										 a_wWhiteShading +
										 m_ScanParam.Size.dwPhyPixels,
									 (u_short)m_ScanParam.Size.dwPhyPixels * 2);
					usb_SetWhiteShading( dev->fd, CHANNEL_blue,
									     a_wWhiteShading +
										 m_ScanParam.Size.dwPhyPixels * 2,
									 (u_short)m_ScanParam.Size.dwPhyPixels * 2);
				} else {
					usb_SetWhiteShading( dev->fd,CHANNEL_green,a_wWhiteShading,
									(u_short)m_ScanParam.Size.dwPhyPixels * 2);
				}

				a_bRegs[0x42] = (u_char)((hw->wDRAMSize > 512)? 0x66: 0x26);

				if( !usbio_WriteReg( dev->fd, 0x42, a_bRegs[0x42]))
					return SANE_FALSE;
			}
			break;

		default:
			a_bRegs[0x3E] = 0;
			a_bRegs[0x3F] = 0;
			a_bRegs[0x40] = 0x40;
			a_bRegs[0x41] = 0x00;
			a_bRegs[0x42] = (u_char)((hw->wDRAMSize > 512)? 0x60: 0x20);

			_UIO(sanei_lm983x_write( dev->fd, 0x3e, &a_bRegs[0x3e],
									  0x42 - 0x3e + 1, SANE_TRUE ));
			break;									
	}

	return SANE_TRUE;
}

/* END PLUSTEK-USBSHADING.C .................................................*/
