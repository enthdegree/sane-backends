/*.............................................................................
 * Project : SANE library for Plustek flatbed scanners.
 *.............................................................................
 */

/** @file plustek-usbshading.c
 *  @brief Calibration routines.
 *
 * Based on sources acquired from Plustek Inc.<br>
 * Copyright (C) 2001-2003 Gerhard Jaeger <gerhard@gjaeger.de>
 *
 * History:
 * - 0.40 - starting version of the USB support
 * - 0.41 - minor fixes
 *        - added workaround stuff for EPSON1250
 * - 0.42 - added workaround stuff for UMAX 3400
 * - 0.43 - added call to usb_switchLamp before reading dark data
 * - 0.44 - added more debug output and bypass calibration
 *        - added dump of shading data
 * - 0.45 - added coarse calibration for CIS devices
 *        - added _WAF_SKIP_FINE to skip the results of fine calibration
 *        - CanoScan fixes
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
 
/************** global stuff - I hate it, but we need it... ******************/

#define _MAX_GAIN_LOOPS 10	 /**< max number of loops for coarse calibration */

#define _MAX_SHAD       0x4000
#define _SHADING_BUF	(_MAX_SHAD*3)	 /**< max size of the shading buffer */

#define _CIS_GAIN	1
#define _CIS_OFFS   4

static u_short a_wWhiteShading[_SHADING_BUF] = {0};
static u_short a_wDarkShading[_SHADING_BUF]  = {0};

/************************** static variables *********************************/

static RGBUShortDef Gain_Hilight;
static RGBUShortDef Gain_NegHilight;
static RGBByteDef   Gain_Reg;

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

static u_short m_wHilight = 4 /*0*/;  /* check the windows registry... */
static u_short m_wShadow  = 4 /*0*/;  /* check the windows registry... */

/**
 */
static void usb_line_statistics( char *cmt, u_short* buf,
						   		 u_long dim_x, SANE_Bool color )
{
	int          i, end;
	u_long       dw, imad, imid, alld, cld, cud;
	u_short      mid, mad, aved, lbd, ubd, tmp;
	pMonoWordDef pvd, pvd2;

	pvd = pvd2 = (pMonoWordDef)buf;

	if( color )
		end = 3;
	else
        end = 1;

	for( i = 0; i < end; i++ ) {

		mid  = 0xFFFF;
		mad  = 0;
		imid = 0;
		imad = 0;
		alld = 0;
		cld  = 0;
		cud  = 0;

		for( dw = 0; dw < dim_x; pvd++, dw++ ) {

			tmp = _LOBYTE(pvd->Mono) * 256 + _HIBYTE(pvd->Mono);

			if( tmp > mad ) {
				mad  = tmp;
				imad = dw;
			}

			if( tmp < mid ) {
				mid  = tmp;
				imid = dw;
			}

			alld += tmp;
       	}

		aved = (u_short)(alld/dim_x);
		lbd  = aved - 0.05*aved;
		ubd  = aved + 0.05*aved;

		for( dw = 0; dw < dim_x; pvd2++, dw++ ) {

			tmp = _LOBYTE(pvd2->Mono) * 256 + _HIBYTE(pvd2->Mono);

			if( tmp > ubd ) {
				cud++;
			} else if( tmp < lbd ) {
				cld++;
			}
		}

		DBG( _DBG_INFO2, "Color[%u] (%s) : "
						  "min=%u(%lu) max=%u(%lu) ave=%u\n",
										i, cmt, mid, imid, mad, imad, aved);
		DBG( _DBG_INFO2, "5%%: %u (%lu), %u (%lu)\n", lbd, cld,ubd,cud);
   	}
}

/** usb_SetMCLK
 * get the MCLK out of our table
 */
static void	usb_SetMCLK( pPlustek_Device dev, pScanParam pParam )
{
	int       	 idx, i;
	pClkMotorDef clk;
	pHWDef    	 hw = &dev->usbDev.HwSetting;

	clk = usb_GetMotorSet( hw->motorModel );
	idx = 0;
	for( i = 0; i < _MAX_CLK; i++ ) {
		if( pParam->PhyDpi.x <= dpi_ranges[i] )
  			break;
		idx++;
	}
	if( idx >= _MAX_CLK )
		idx = _MAX_CLK - 1;

	if( pParam->bDataType != SCANDATATYPE_Color ) {

		if( pParam->bBitDepth > 8 )
			dMCLK = clk->gray_mclk_16[idx];
		else
			dMCLK = clk->gray_mclk_8[idx];
	} else {
		if( pParam->bBitDepth > 8 )
			dMCLK = clk->color_mclk_16[idx];
		else
			dMCLK = clk->color_mclk_8[idx];
	}

 	pParam->dMCLK = dMCLK;

	DBG( _DBG_INFO, "SETMCLK[%u/%u], using entry %u: %f, %u\n",
			hw->motorModel,	pParam->bDataType, idx, dMCLK, pParam->PhyDpi.x );
}

/** usb_SetDarkShading
 * download the dark shading data to Merlins' DRAM
 */
static SANE_Bool usb_SetDarkShading( int fd, u_char channel,
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

/** usb_SetWhiteShading
 * download the white shading data to Merlins' DRAM
 */
static SANE_Bool usb_SetWhiteShading( int fd, u_char channel,
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

/** usb_GetSoftwareOffsetGain
 * preset the offset and gain parameter for fine calibration.
 * Adjustment on these defaults is done for negative and transparency
 * scanning...
 */
static void usb_GetSoftwareOffsetGain( pPlustek_Device dev )
{
	pScanParam pParam = &dev->scanning.sParam;
	pDCapsDef  sCaps  = &dev->usbDev.Caps;
	pHWDef     hw     = &dev->usbDev.HwSetting;

	pParam->swOffset[0] = 0;
	pParam->swOffset[1] = 0;
	pParam->swOffset[2] = 0;

	pParam->swGain[0] = 1000;
	pParam->swGain[1] = 1000;
	pParam->swGain[2] = 1000;

	if( pParam->bSource != SOURCE_Reflection )
		return;

	switch( sCaps->bCCD ) {

	case kNECSLIM:
		DBG( _DBG_INFO2, "kNECSLIM adjustments\n" );
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
		break;

	case kNEC8861:
		DBG( _DBG_INFO2, "kNEC8861 adjustments\n" );
		break;

	case kNEC3799:
		DBG( _DBG_INFO2, "kNEC3799 adjustments\n" );
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
		} else if( hw->motorModel == MODEL_KaoHsiung ) {
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
		DBG( _DBG_INFO2, "kSONY548 adjustments\n" );
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
		DBG( _DBG_INFO2, "kNEC3778 adjustments\n" );
		if((_LM9831 == hw->chip) && (pParam->PhyDpi.x <= 300))
		{
			pParam->swOffset[0] = 0;
			pParam->swOffset[1] = 0;
			pParam->swOffset[2] = 0;
			pParam->swGain[0] = 900;
			pParam->swGain[1] = 920;
			pParam->swGain[2] = 980;
		}
		else if( hw->motorModel == MODEL_HuaLien && pParam->PhyDpi.x > 800)
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

/** as the name says..
 *
 */
static void usb_Swap( u_short *pw, u_long dwBytes )
{
	for( dwBytes /= 2; dwBytes--; pw++ )
		_SWAP(((u_char*) pw)[0], ((u_char*)pw)[1]);
}

/** according to the pixel values,
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

		if( dAmp > 31 ) {
			DBG( _DBG_INFO, "Gain internally limited! (%.3f-> 31)\n", dAmp );
			dAmp = 31;
		}

		bGain = (u_char)dAmp + 32;
		return bGain;
	}
}

/** limit and set register given by address
 * @param gain -
 * @param reg  -
 */
static void setAdjGain( int gain, u_char *reg )
{
	if( gain >= 0 ) {

		if( gain > 0x3f )
			*reg = 0x3f;
		else
			*reg = gain;
	}
}

/**
 * @param channel - 0 = red, 1 = green, 2 = blue
 * @param max     -
 * @param ideal   -
 * @param l_on    -
 * @param l_off   -
 * @return
 */
static SANE_Bool adjLampSetting( int channel, u_long  max,  u_long   ideal,
											  u_short l_on, u_short *l_off )
{
	SANE_Bool adj = SANE_FALSE;
	u_long    lamp_on;

	/* so if the image was too bright, we dim the lamps by 3% */
	if( max > ideal ) {

		lamp_on = (*l_off) - l_on;
	    lamp_on = (lamp_on * 97)/100;
		*l_off  = l_on + lamp_on;
        DBG( _DBG_INFO2,
	        		"lamp(%u) adjust (-3%%): %i %i\n", channel, l_on, *l_off );
		adj = SANE_TRUE;	          
	}	          

    /* if the image was too dull, increase lamp by 1% */
	if( a_bRegs[0x3b + channel] == 63 ) {

		lamp_on  = (*l_off) - l_on;
	    lamp_on += (lamp_on/100);
		*l_off   = l_on + lamp_on;
        DBG( _DBG_INFO2,
	        		"lamp(%u) adjust (+1%%): %i %i\n", channel, l_on, *l_off );
		adj = SANE_TRUE;
    }

    return adj;
}    

/** usb_AdjustGain
 * function to perform the "coarse calibration step" part 1.
 * We scan reference image pixels to determine the optimum coarse gain settings
 * for R, G, B. (Analog gain and offset prior to ADC). These coefficients are
 * applied at the line rate during normal scanning.
 * The scanned line should contain a white strip with some black at the
 * beginning. The function searches for the maximum value which corresponds to
 * the maximum white value.
 * Affects register 0x3b, 0x3c and 0x3d
 *
 */
static SANE_Bool usb_AdjustGain( pPlustek_Device dev, int fNegative )
{
	char      tmp[40];
	int       i;
    pScanDef  scanning = &dev->scanning;
	pDCapsDef scaps    = &dev->usbDev.Caps;
	pHWDef    hw       = &dev->usbDev.HwSetting;
	u_long    dw;
	SANE_Bool fRepeatITA = SANE_TRUE;

	if( usb_IsEscPressed())
		return SANE_FALSE;

	bMaxITA = 0xff;

	DBG( _DBG_INFO2, "usb_AdjustGain()\n" );
	if( hw->bReg_0x26 & _ONE_CH_COLOR ) {
		a_bRegs[0x3b] =
		a_bRegs[0x3c] =
		a_bRegs[0x3d] = _CIS_GAIN;

		/* don't blame on me - I know it's shitty... */
		goto show_sets;
	}
	
	/*
     * define the strip to scan for coarse calibration
     * done at 300dpi
     */
	m_ScanParam.Size.dwLines  = 1;				/* for gain */
	m_ScanParam.Size.dwPixels = scaps->Normal.Size.x *
								scaps->OpticDpi.x / 300UL;

	m_ScanParam.Size.dwBytes  = m_ScanParam.Size.dwPixels *
								2 * m_ScanParam.bChannels;

	if( hw->bReg_0x26 & _ONE_CH_COLOR &&
		m_ScanParam.bDataType == SCANDATATYPE_Color ) {
		m_ScanParam.Size.dwBytes *= 3;
	}

	m_ScanParam.Origin.x = (u_short)((u_long) hw->wActivePixelsStart *
													300UL / scaps->OpticDpi.x);
	m_ScanParam.bCalibration = PARAM_Gain;

	DBG( _DBG_INFO2, "Coarse Calibration Strip:\n" );
	DBG( _DBG_INFO2, "Lines    = %lu\n", m_ScanParam.Size.dwLines  );
	DBG( _DBG_INFO2, "Pixels   = %lu\n", m_ScanParam.Size.dwPixels );
	DBG( _DBG_INFO2, "Bytes    = %lu\n", m_ScanParam.Size.dwBytes  );
	DBG( _DBG_INFO2, "Origin.X = %u\n",  m_ScanParam.Origin.x );

	i = 0;
TOGAIN:
 	m_ScanParam.dMCLK = dMCLK;

	if( !usb_SetScanParameters( dev, &m_ScanParam ))
 		return SANE_FALSE;

	if( !usb_Wait4Warmup( dev )) {
		DBG( _DBG_ERROR, "usb_AdjustGain() - CANCEL detected\n" );
		return SANE_FALSE;
	}

	if(	!usb_ScanBegin( dev, SANE_FALSE) ||
		!usb_ScanReadImage( dev, pScanBuffer, m_ScanParam.Size.dwPhyBytes ) ||
		!usb_ScanEnd( dev )) {
		DBG( _DBG_ERROR, "usb_AdjustGain() failed\n" );
		return SANE_FALSE;
	}

	DBG( _DBG_INFO2, "PhyBytes  = %lu\n",  m_ScanParam.Size.dwPhyBytes  );
	DBG( _DBG_INFO2, "PhyPixels = %lu\n",  m_ScanParam.Size.dwPhyPixels );

	sprintf( tmp, "coarse-gain-%u.raw", i++ );

	dumpPicInit( &m_ScanParam, tmp );
	dumpPic( tmp, pScanBuffer, m_ScanParam.Size.dwPhyBytes );
		
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

			RGBUShortDef max_rgb, min_rgb, tmp_rgb;
			u_long       dwR, dwG, dwB;
			u_long       dwDiv = 10;
			u_long       dwLoop1 = m_ScanParam.Size.dwPhyPixels / dwDiv, dwLoop2;

			max_rgb.Red = max_rgb.Green = max_rgb.Blue = 0;
			min_rgb.Red = min_rgb.Green = min_rgb.Blue = 0xffff;

			/* find out the max pixel value for R, G, B */
			for( dw = 0; dwLoop1; dwLoop1-- ) {

				/* do some averaging... */
				for (dwLoop2 = dwDiv, dwR = dwG = dwB = 0; dwLoop2; dwLoop2--, dw++)
				{
					if( hw->bReg_0x26 & _ONE_CH_COLOR ) {
						dwR += ((u_short*)pScanBuffer)[dw];
						dwG += ((u_short*)pScanBuffer)
										[dw+m_ScanParam.Size.dwPhyPixels+1];
						dwB += ((u_short*)pScanBuffer)
									[dw+(m_ScanParam.Size.dwPhyPixels+1)*2];
            		} else {
						dwR += ((pRGBUShortDef)pScanBuffer)[dw].Red;
						dwG += ((pRGBUShortDef)pScanBuffer)[dw].Green;
						dwB += ((pRGBUShortDef)pScanBuffer)[dw].Blue;
					}
				}
				dwR = dwR / dwDiv;
				dwG = dwG / dwDiv;
				dwB = dwB / dwDiv;

				if(max_rgb.Red < dwR)
					max_rgb.Red = dwR;
				if(max_rgb.Green < dwG)
					max_rgb.Green = dwG;
				if(max_rgb.Blue < dwB)
					max_rgb.Blue = dwB;

				if(min_rgb.Red > dwR)
					min_rgb.Red = dwR;
				if(min_rgb.Green > dwG)
					min_rgb.Green = dwG;
				if(min_rgb.Blue > dwB)
					min_rgb.Blue = dwB;
			}

			DBG(_DBG_INFO2, "MAX(R,G,B)= 0x%04x(%u), 0x%04x(%u), 0x%04x(%u)\n",
				  	max_rgb.Red, max_rgb.Red, max_rgb.Green,
					max_rgb.Green, max_rgb.Blue, max_rgb.Blue );
			DBG(_DBG_INFO2, "MIN(R,G,B)= 0x%04x(%u), 0x%04x(%u), 0x%04x(%u)\n",
				  	min_rgb.Red, min_rgb.Red, min_rgb.Green,
					min_rgb.Green, min_rgb.Blue, min_rgb.Blue );

			/* on CIS scanner, we use the min value, on CCD the max value
			 * for adjusting the gain
			 */
			tmp_rgb = max_rgb;
			if( hw->bReg_0x26 & _ONE_CH_COLOR ) 
				tmp_rgb = min_rgb;

			DBG(_DBG_INFO2, "CUR(R,G,B)= 0x%04x(%u), 0x%04x(%u), 0x%04x(%u)\n",
				  	tmp_rgb.Red, tmp_rgb.Red, tmp_rgb.Green,
				    tmp_rgb.Green, tmp_rgb.Blue, tmp_rgb.Blue);

			m_dwIdealGain = IDEAL_GainNormal;
						 /* min(min(rgb.wRed, rgb.wGreen), rgb.wBlue) */

			a_bRegs[0x3b] = usb_GetNewGain( tmp_rgb.Red   );
			a_bRegs[0x3c] = usb_GetNewGain( tmp_rgb.Green );
			a_bRegs[0x3d] = usb_GetNewGain( tmp_rgb.Blue  );

			if( !_IS_PLUSTEKMOTOR(hw->motorModel)) {

				SANE_Bool adj = SANE_FALSE;

				/* on CIS devices, we can control the lamp off settings */
				if( hw->bReg_0x26 & _ONE_CH_COLOR ) {

					m_dwIdealGain = IDEAL_GainNormal;

					if( adjLampSetting( CHANNEL_red, tmp_rgb.Red, m_dwIdealGain,
									    hw->red_lamp_on, &hw->red_lamp_off )) {
   						adj = SANE_TRUE;
					}

					if( adjLampSetting( CHANNEL_green, tmp_rgb.Green, m_dwIdealGain,
								    hw->green_lamp_on, &hw->green_lamp_off )) {
   						adj = SANE_TRUE;
					}

					if( adjLampSetting( CHANNEL_blue, tmp_rgb.Blue, m_dwIdealGain,
									    hw->blue_lamp_on, &hw->blue_lamp_off)){
   						adj = SANE_TRUE;
					}

		            /* on any adjustment, set the registers... */
	       			if( adj ) {
						usb_AdjustLamps( dev );

						if( i < _MAX_GAIN_LOOPS )
		       				goto TOGAIN;
	           		}
	             
    			} else {

        			if((!a_bRegs[0x3b] ||
        			    !a_bRegs[0x3c] || !a_bRegs[0x3d]) && dMCLK > 3.0) {

        				scanning->sParam.dMCLK = dMCLK = dMCLK - 0.5;
        				a_bRegs[0x3b] = a_bRegs[0x3c] = a_bRegs[0x3d] = 1;

   						adj = SANE_TRUE;

        			} else if(((a_bRegs[0x3b] == 63) || (a_bRegs[0x3c] == 63) ||
       								  (a_bRegs[0x3d] == 63)) && (dMCLK < 10)) {

        				scanning->sParam.dMCLK = dMCLK = dMCLK + 0.5;
        				a_bRegs[0x3b] = a_bRegs[0x3c] = a_bRegs[0x3d] = 1;

   						adj = SANE_TRUE;
                	}

	       			if( adj ) {
						if( i < _MAX_GAIN_LOOPS )
		       				goto TOGAIN;
					}
#if 0
				/* on CCD devices, we can adjust the dutycyles of the lamp */
					Hardware.merlininfo.Green_PWM_Duty_Cycle =
						(Hardware.merlininfo.Green_PWM_Duty_Cycle*90)/100;
					if (Hardware.merlininfo.Green_PWM_Duty_Cycle <
						Hardware.merlininfo.Green_PWM_Duty_Cycle_Low)
					{
						Hardware.NSCAfxMessageBox("Image too bright!");
						return FALSE;
					}
#endif       	
				}
				
			} else {

        		/* for MODEL KaoHsiung 1200 scanner multi-straight-line bug at
                 * 1200 dpi color mode
                 */
        		if( hw->motorModel == MODEL_KaoHsiung &&
					scaps->bCCD == kNEC3778 && 	dMCLK>= 5.5 && !a_bRegs[0x3c]){

        			a_bRegs[0x3b] = a_bRegs[0x3c] = a_bRegs[0x3d] = 1;
        			scanning->sParam.dMCLK = dMCLK = dMCLK - 1.5;
        			goto TOGAIN;

        		} else if( hw->motorModel == MODEL_HuaLien &&
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
			}

		} else {

			u_short	w_max = 0, w_min = 0xffff, w_tmp;

			for( dw = 0; dw < m_ScanParam.Size.dwPhyPixels; dw++ ) {
				if( w_max < ((u_short*)pScanBuffer)[dw])
					w_max = ((u_short*)pScanBuffer)[dw];
				if( w_min > ((u_short*)pScanBuffer)[dw])
					w_min = ((u_short*)pScanBuffer)[dw];
			}

			w_tmp = w_max;
			if( hw->bReg_0x26 & _ONE_CH_COLOR )
				w_tmp = w_min;

			a_bRegs[0x3b] =
			a_bRegs[0x3c] =
			a_bRegs[0x3d] = usb_GetNewGain(w_tmp);

			DBG(_DBG_INFO2, "MAX(G)= 0x%04x(%u)\n", w_max, w_max );
			DBG(_DBG_INFO2, "MIN(G)= 0x%04x(%u)\n", w_min, w_min );
			DBG(_DBG_INFO2, "CUR(G)= 0x%04x(%u)\n", w_tmp, w_tmp );

			m_dwIdealGain = IDEAL_GainNormal;

			if( !_IS_PLUSTEKMOTOR(hw->motorModel)) {

				SANE_Bool adj = SANE_FALSE;

				/* on CIS devices, we can control the lamp off settings */
				if( hw->bReg_0x26 & _ONE_CH_COLOR ) {

					if( adjLampSetting( CHANNEL_green, w_tmp, m_dwIdealGain,
								    hw->green_lamp_on, &hw->green_lamp_off )) {
   						adj = SANE_TRUE;
					}

		            /* on any adjustment, set the registers... */
	       			if( adj ) {
						usb_AdjustLamps( dev );

						if( i < _MAX_GAIN_LOOPS )
		       				goto TOGAIN;
	           		}

    			} else {

        			if( !a_bRegs[0x3b] && (dMCLK > 6.0)) {

        				scanning->sParam.dMCLK = dMCLK = dMCLK - 0.5;
        				a_bRegs[0x3b] = a_bRegs[0x3c] = a_bRegs[0x3d] = 1;

   						adj = SANE_TRUE;

        			} else if((a_bRegs[0x3b] == 63) && (dMCLK < 20)) {

        				scanning->sParam.dMCLK = dMCLK = dMCLK + 0.5;
        				a_bRegs[0x3b] = a_bRegs[0x3c] = a_bRegs[0x3d] = 1;

   						adj = SANE_TRUE;
                	}

	       			if( adj ) {
						if( i < _MAX_GAIN_LOOPS )
		       				goto TOGAIN;
					}
				}
            }
		}
	}

show_sets:		
	DBG( _DBG_INFO2, "REG[0x3b] = %u\n", a_bRegs[0x3b] );
	DBG( _DBG_INFO2, "REG[0x3c] = %u\n", a_bRegs[0x3c] );
	DBG( _DBG_INFO2, "REG[0x3d] = %u\n", a_bRegs[0x3d] );

	/* adjust gain here if the user has set some values >= 0 */
	setAdjGain( dev->adj.rgain, &a_bRegs[0x3b] );
	setAdjGain( dev->adj.ggain, &a_bRegs[0x3c] );
	setAdjGain( dev->adj.bgain, &a_bRegs[0x3d] );

	DBG( _DBG_INFO2, "after tweaking:\n" );
	DBG( _DBG_INFO2, "REG[0x3b] = %u\n", a_bRegs[0x3b] );
	DBG( _DBG_INFO2, "REG[0x3c] = %u\n", a_bRegs[0x3c] );
	DBG( _DBG_INFO2, "REG[0x3d] = %u\n", a_bRegs[0x3d] );

	DBG( _DBG_INFO2, "red_lamp_on    = %u\n", hw->red_lamp_on  );
	DBG( _DBG_INFO2, "red_lamp_off   = %u\n", hw->red_lamp_off );
	DBG( _DBG_INFO2, "green_lamp_on  = %u\n", hw->green_lamp_on  );
	DBG( _DBG_INFO2, "green_lamp_off = %u\n", hw->green_lamp_off );
	DBG( _DBG_INFO2, "blue_lamp_on   = %u\n", hw->blue_lamp_on   );
	DBG( _DBG_INFO2, "blue_lamp_off  = %u\n", hw->blue_lamp_off  );

	DBG( _DBG_INFO2, "usb_AdjustGain() done.\n" );

	return SANE_TRUE;
}

/** usb_GetNewOffset		
 * @param pdwSum   -
 * @param pdwDiff  -
 * @param pcOffset -
 * @param pIdeal   -
 * @param channel  -
 * @param cAdjust  -
 */
static void usb_GetNewOffset( u_long *pdwSum, u_long *pdwDiff,
								signed char *pcOffset, u_char *pIdeal,
										u_long channel, signed char cAdjust )
{
	/* IDEAL_Offset is currently set to 0x1000 = 4096 */
	u_long dwIdealOffset = IDEAL_Offset;

	if( pdwSum[channel] > dwIdealOffset ) {

		/* Over ideal value */
		pdwSum[channel] -= dwIdealOffset;
		if( pdwSum[channel] < pdwDiff[channel] ) {
			/* New offset is better than old one */
			pdwDiff[channel] = pdwSum[channel];
			pIdeal[channel]  = a_bRegs[0x38 + channel];
		}
		pcOffset[channel] -= cAdjust;

	} else 	{

		/* Below than ideal value */
		pdwSum[channel] = dwIdealOffset - pdwSum [channel];
		if( pdwSum[channel] < pdwDiff[channel] ) {
			/* New offset is better than old one */
			pdwDiff[channel] = pdwSum[channel];
			pIdeal[channel]  = a_bRegs[0x38 + channel];
		}
		pcOffset[channel] += cAdjust;
	}

	if( pcOffset[channel] >= 0 )
		a_bRegs[0x38 + channel] = pcOffset[channel];
	else
		a_bRegs[0x38 + channel] = (u_char)(32 - pcOffset[channel]);
}

/** usb_AdjustOffset
 * function to perform the "coarse calibration step" part 2.
 * We scan reference image pixels to determine the optimum coarse offset settings
 * for R, G, B. (Analog gain and offset prior to ADC). These coefficients are
 * applied at the line rate during normal scanning.
 * On CIS based devices, we switch the light off, on CCD devices, we use the optical
 * black pixels.
 * Affects register 0x38, 0x39 and 0x3a
 */
static SANE_Bool usb_AdjustOffset( pPlustek_Device dev )
{
	char          tmp[40];
	signed char   cAdjust = 16;
	signed char   cOffset[3];
	u_char        bExpect[3];
	int           i;
	u_long        dw, dwPixels;
    u_long        dwDiff[3], dwSum[3];

	pHWDef hw = &dev->usbDev.HwSetting;

	if( usb_IsEscPressed())
		return SANE_FALSE;

	DBG( _DBG_INFO2, "usb_AdjustOffset()\n" );
	if( hw->bReg_0x26 & _ONE_CH_COLOR ) {
		a_bRegs[0x38] =
		a_bRegs[0x39] =
		a_bRegs[0x3a] = _CIS_OFFS;
		return SANE_TRUE;
	}

	m_ScanParam.Size.dwLines  = 1;				/* for gain */
	m_ScanParam.Size.dwPixels = 2550;

	if( hw->bReg_0x26 & _ONE_CH_COLOR ) 
		dwPixels = m_ScanParam.Size.dwPixels;
	else
		dwPixels = (u_long)(hw->bOpticBlackEnd - hw->bOpticBlackStart );

	m_ScanParam.Size.dwBytes  = m_ScanParam.Size.dwPixels * 2 *
														m_ScanParam.bChannels;
	if( hw->bReg_0x26 & _ONE_CH_COLOR &&
		m_ScanParam.bDataType == SCANDATATYPE_Color ) {
		m_ScanParam.Size.dwBytes *= 3;
	}

	m_ScanParam.Origin.x = (u_short)((u_long)hw->bOpticBlackStart * 300UL /
												  dev->usbDev.Caps.OpticDpi.x);
	m_ScanParam.bCalibration = PARAM_Offset;
 	m_ScanParam.dMCLK        = dMCLK;

	dwDiff[0]  = dwDiff[1]  = dwDiff[2]  = 0xffff;
	cOffset[0] = cOffset[1] = cOffset[2] = 0;
	bExpect[0] = bExpect[1] = bExpect[2] = 0;

	a_bRegs[0x38] = a_bRegs [0x39] = a_bRegs [0x3a] = 0;

	if( hw->bReg_0x26 & _ONE_CH_COLOR ) {
	    /*
		 * if we have dark shading strip, there's no need to switch
	     * the lamp off
		 */
		if( dev->usbDev.pSource->DarkShadOrgY >= 0 ) {

			usb_ModuleToHome( dev, SANE_TRUE );
			usb_ModuleMove  ( dev, MOVE_Forward,
								(u_long)dev->usbDev.pSource->DarkShadOrgY );

			a_bRegs[0x45] &= ~0x10;

		} else {

		 	/* switch lamp off to read dark data... */
			a_bRegs[0x29] = 0;
			usb_switchLamp( dev, SANE_FALSE );
		}
	}

	if( 0 == dwPixels ) {
		DBG( _DBG_ERROR, "OpticBlackEnd = OpticBlackStart!!!\n" );
		return SANE_FALSE;
	}

	if( !usb_SetScanParameters( dev, &m_ScanParam )) {
		DBG( _DBG_ERROR, "usb_AdjustOffset() failed\n" );
		return SANE_FALSE;
	}
		
	i = 0;

	DBG( _DBG_INFO2, "S.dwPixels  = %lu\n", m_ScanParam.Size.dwPixels );		
	DBG( _DBG_INFO2, "dwPixels    = %lu\n", dwPixels );
	DBG( _DBG_INFO2, "dwPhyBytes  = %lu\n", m_ScanParam.Size.dwPhyBytes );
	DBG( _DBG_INFO2, "dwPhyPixels = %lu\n", m_ScanParam.Size.dwPhyPixels );

	while( cAdjust ) {

		/*
		 * read data (a white calibration strip - hopefully ;-)
		 */
		if((!usb_ScanBegin(dev, SANE_FALSE)) ||
		   (!usb_ScanReadImage(dev,pScanBuffer,m_ScanParam.Size.dwPhyBytes)) ||
			!usb_ScanEnd( dev )) {
			DBG( _DBG_ERROR, "usb_AdjustOffset() failed\n" );
			return SANE_FALSE;
		}

		sprintf( tmp, "coarse-off-%u.raw", i++ );

		dumpPicInit( &m_ScanParam, tmp );
		dumpPic( tmp, pScanBuffer, m_ScanParam.Size.dwPhyBytes );

		if( m_ScanParam.bDataType == SCANDATATYPE_Color ) {

			dwSum[0] = dwSum[1] = dwSum[2] = 0;

			for (dw = 0; dw < dwPixels; dw++) {

				dwSum[0] += (u_long)_HILO2WORD(((pColorWordDef)pScanBuffer)[dw].HiLo[0]);
				dwSum[1] += (u_long)_HILO2WORD(((pColorWordDef)pScanBuffer)[dw].HiLo[1]);
				dwSum[2] += (u_long)_HILO2WORD(((pColorWordDef)pScanBuffer)[dw].HiLo[2]);
			}

            DBG( _DBG_INFO2, "RedSum   = %lu, ave = %lu\n",
												dwSum[0], dwSum[0] /dwPixels );
            DBG( _DBG_INFO2, "GreenSum = %lu, ave = %lu\n",
												dwSum[1], dwSum[1] /dwPixels );
            DBG( _DBG_INFO2, "BlueSum  = %lu, ave = %lu\n",
												dwSum[2], dwSum[2] /dwPixels );
			
			/* do averaging for each channel */
			dwSum[0] /= dwPixels;
			dwSum[1] /= dwPixels;
			dwSum[2] /= dwPixels;

			usb_GetNewOffset( dwSum, dwDiff, cOffset, bExpect, 0, cAdjust );
			usb_GetNewOffset( dwSum, dwDiff, cOffset, bExpect, 1, cAdjust );
			usb_GetNewOffset( dwSum, dwDiff, cOffset, bExpect, 2, cAdjust );

            DBG( _DBG_INFO2, "RedExpect   = %u\n", bExpect[0] );
            DBG( _DBG_INFO2, "GreenExpect = %u\n", bExpect[1] );
            DBG( _DBG_INFO2, "BlueExpect  = %u\n", bExpect[2] );

		} else {
			dwSum[0] = 0;

			for( dw = 0; dw < dwPixels; dw++ )
				dwSum[0] += (u_long)_HILO2WORD(((pHiLoDef)pScanBuffer)[dw]);
			dwSum [0] /= dwPixels;
			usb_GetNewOffset( dwSum, dwDiff, cOffset, bExpect, 0, cAdjust );
			a_bRegs[0x3a] = a_bRegs[0x39] = a_bRegs[0x38];

            DBG( _DBG_INFO2, "Sum = %lu, ave = %lu\n",
												dwSum[0], dwSum[0] /dwPixels );

            DBG( _DBG_INFO2, "Expect = %u\n", bExpect[0] );
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

	DBG( _DBG_INFO2, "REG[0x38] = %u\n", a_bRegs[0x38] );
	DBG( _DBG_INFO2, "REG[0x39] = %u\n", a_bRegs[0x39] );
	DBG( _DBG_INFO2, "REG[0x3a] = %u\n", a_bRegs[0x3a] );
	DBG( _DBG_INFO2, "usb_AdjustOffset() done.\n" );

	/* switch it on again on CIS based scanners */
	if( hw->bReg_0x26 & _ONE_CH_COLOR ) {

		if( dev->usbDev.pSource->DarkShadOrgY < 0 ) {
			a_bRegs[0x29] = hw->bReg_0x29;
  			usb_switchLamp( dev, SANE_TRUE );
			usbio_WriteReg( dev->fd, 0x29, a_bRegs[0x29]);
		}
	}

	return SANE_TRUE;
}

/** this function tries to find out some suitable values for the dark
 *  fine calibration. If the device owns a black calibration strip
 *  (never saw one yet - _WAF_BLACKFINE is set then), the data is simply
 *  copied. If not, then the white strip is read with the lamp switched
 *  off...
 */
static void usb_GetDarkShading( pPlustek_Device dev, u_short *pwDest,
								pHiLoDef pSrce, u_long dwPixels,
								u_long dwAdd, int iOffset )
{
	u_long    dw;
	u_long    dwSum[2];
	pDCapsDef scaps = &dev->usbDev.Caps;
	pHWDef    hw    = &dev->usbDev.HwSetting;

	if( scaps->workaroundFlag & _WAF_BLACKFINE )
	{
		u_short w;

		/* here we use the source  buffer + a static offset */
		for (dw = 0; dw < dwPixels; dw++, pSrce += dwAdd)
		{
			w = (u_short)((int)_PHILO2WORD(pSrce) + iOffset);
			pwDest[dw] = _LOBYTE(w) * 256 + _HIBYTE(w);
		}
	}
	else
	{
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

			/* do some averaging on the line */
			for( dw = dwEnd; dw < dwPixels; dw++, pSrce += dwAdd )
				dwSum[0] += (u_long)_PHILO2WORD(pSrce);

			dwSum[0] /= (dwPixels - dwEnd);

			/* add our offset... */
			dwSum[0] = (int)dwSum[0] + iOffset;
			if((int)dwSum[0] < 0)
				dwSum [0] = 0;

			dwSum[0] = (u_long)_LOBYTE(_LOWORD(dwSum[0])) * 256UL +
													_HIBYTE(_LOWORD(dwSum[0]));

			/* fill the shading data */
			for( dw = dwEnd; dw < dwPixels; dw++ )
				pwDest[dw] = (u_short)dwSum[0];
		}
	}
}

/** usb_AdjustDarkShading
 * fine calibration part 1 - read the black calibration area and write
 * the black line data to the offset coefficient data in Merlins' DRAM
 * If there's no black line available, we can use the min pixel value
 * from coarse calibration...
 */
static SANE_Bool usb_AdjustDarkShading( pPlustek_Device dev )
{
	char      tmp[40];
    pScanDef  scanning = &dev->scanning;
	pDCapsDef scaps    = &dev->usbDev.Caps;
	pHWDef    hw       = &dev->usbDev.HwSetting;

	if( usb_IsEscPressed())
		return SANE_FALSE;

	if( scaps->workaroundFlag & _WAF_SKIP_FINE )
		return SANE_TRUE;

	DBG( _DBG_INFO2, "usb_AdjustDarkShading()\n" );
	DBG( _DBG_INFO2, "MCLK = %f (scanparam-MCLK=%f)\n",
						dMCLK, scanning->sParam.dMCLK );

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

	if( hw->bReg_0x26 & _ONE_CH_COLOR &&
		m_ScanParam.bDataType == SCANDATATYPE_Color ) {
		m_ScanParam.Size.dwBytes *= 3;
    }

    /*
	 * if we have dark shading strip, there's no need to switch
     * the lamp off
	 */
	if( dev->usbDev.pSource->DarkShadOrgY >= 0 ) {

		usb_ModuleToHome( dev, SANE_TRUE );
		usb_ModuleMove  ( dev, MOVE_Forward,
								(u_long)dev->usbDev.pSource->DarkShadOrgY );
	} else {

	 	/* switch lamp off to read dark data... */
		a_bRegs[0x29] = 0;
		usb_switchLamp( dev, SANE_FALSE );
	}

	usb_SetScanParameters( dev, &m_ScanParam );

	if((!usb_ScanBegin(dev, SANE_FALSE)) ||
	   (!usb_ScanReadImage(dev,pScanBuffer, m_ScanParam.Size.dwPhyBytes)) ||
	   (!usb_ScanEnd( dev ))) {

      	/* on error, reset the lamp settings*/
		
		a_bRegs[0x29] = hw->bReg_0x29;
  		usb_switchLamp( dev, SANE_TRUE );
		usbio_WriteReg( dev->fd, 0x29, a_bRegs[0x29] );
		
		DBG( _DBG_ERROR, "usb_AdjustDarkShading() failed\n" );
		return SANE_FALSE;
	}
	
	/*
	 * set illumination mode and switch lamp on again
	 */
	a_bRegs[0x29] = hw->bReg_0x29;
	usb_switchLamp( dev, SANE_TRUE );

	if( !usbio_WriteReg( dev->fd, 0x29, a_bRegs[0x29])) {
		DBG( _DBG_ERROR, "usb_AdjustDarkShading() failed\n" );
		return SANE_FALSE;
	}	

	sprintf( tmp, "fine-black.raw" );

	dumpPicInit( &m_ScanParam, tmp );
	dumpPic( tmp, pScanBuffer, m_ScanParam.Size.dwPhyBytes );

	usleep(500 * 1000);			/* Warm up lamp again */

	if( m_ScanParam.bDataType == SCANDATATYPE_Color ) {

		if( hw->bReg_0x26 & _ONE_CH_COLOR ) {

			usb_GetDarkShading( dev, a_wDarkShading, (pHiLoDef)pScanBuffer,
								m_ScanParam.Size.dwPhyPixels, 1,
												scanning->sParam.swOffset[0]);

			usb_GetDarkShading( dev, a_wDarkShading + m_ScanParam.Size.dwPhyPixels,
							(pHiLoDef)pScanBuffer + m_ScanParam.Size.dwPhyPixels,
					m_ScanParam.Size.dwPhyPixels, 1, scanning->sParam.swOffset[1]);

			usb_GetDarkShading( dev, a_wDarkShading + m_ScanParam.Size.dwPhyPixels * 2,
			               (pHiLoDef)pScanBuffer + m_ScanParam.Size.dwPhyPixels * 2,
							m_ScanParam.Size.dwPhyPixels, 1, scanning->sParam.swOffset[2]);

		} else {

			usb_GetDarkShading( dev, a_wDarkShading, (pHiLoDef)pScanBuffer,
							m_ScanParam.Size.dwPhyPixels, 3,
												scanning->sParam.swOffset[0]);

			usb_GetDarkShading( dev, a_wDarkShading + m_ScanParam.Size.dwPhyPixels,
							(pHiLoDef)pScanBuffer + 1, m_ScanParam.Size.dwPhyPixels,
							3, scanning->sParam.swOffset[1]);
			usb_GetDarkShading( dev, a_wDarkShading + m_ScanParam.Size.dwPhyPixels * 2,
			               (pHiLoDef)pScanBuffer + 2, m_ScanParam.Size.dwPhyPixels,
							3, scanning->sParam.swOffset[2]);
		}
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

	usb_line_statistics( "Dark", a_wDarkShading, m_ScanParam.Size.dwPhyPixels,
                          scanning->sParam.bDataType == SCANDATATYPE_Color?1:0);
	return SANE_TRUE;		
}

/** usb_AdjustWhiteShading
 * fine calibration part 2 - read the white calibration area and calculate
 * the gain coefficient for each pixel
 *
 */
static SANE_Bool usb_AdjustWhiteShading( pPlustek_Device dev )
{
	char         tmp[40];
    pScanDef     scanning = &dev->scanning;
	pDCapsDef    scaps    = &dev->usbDev.Caps;
	pHWDef       hw       = &dev->usbDev.HwSetting;
	u_char      *pBuf     = pScanBuffer;
	u_long       dw, dwLines, dwRead;
	u_long       dwShadingLines;
	pMonoWordDef pValue;
	u_long*      pdw;
	int          i;

	if( scaps->workaroundFlag & _WAF_SKIP_FINE )
		return SANE_TRUE;
	
	m_pAvColor = (pRGBUShortDef)pScanBuffer;
	m_pAvMono  = (u_short*)pScanBuffer;

	if( usb_IsEscPressed())
		return SANE_FALSE;

	if( m_ScanParam.PhyDpi.x > 75)
		dwShadingLines = 64;
	else
		dwShadingLines = 32;

	/* NOTE: m_wHilight + m_wShadow < dwShadingLines */
	m_wHilight = 4;
	m_wShadow  = 4;

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
	m_ScanParam.Size.dwBytes = m_ScanParam.Size.dwPixels * 2 *
														m_ScanParam.bChannels;

	if( hw->bReg_0x26 & _ONE_CH_COLOR &&
		m_ScanParam.bDataType == SCANDATATYPE_Color ) {
		m_ScanParam.Size.dwBytes *= 3;
	}

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
		if( hw->bReg_0x26 & _ONE_CH_COLOR &&
			m_ScanParam.bDataType == SCANDATATYPE_Color ) {
			m_ScanParam.Size.dwBytes *= 3;
		}
		m_dwPixels = scanning->sParam.Size.dwPixels * m_ScanParam.UserDpi.x / scanning->sParam.UserDpi.x;

		dw = (u_long)(hw->wDRAMSize - DRAM_UsedByAsic16BitMode) * 1024UL;
		m_ScanParam.Size.dwLines = dwShadingLines;	/* SHADING_Lines */
		for( dwLines = dw / m_ScanParam.Size.dwBytes;
			 dwLines < m_ScanParam.Size.dwLines; m_ScanParam.Size.dwLines>>=1);
	} else {
		m_ScanParam.Size.dwBytes = m_ScanParam.Size.dwPixels * 2 * m_ScanParam.bChannels;
		if( hw->bReg_0x26 & _ONE_CH_COLOR &&
			m_ScanParam.bDataType == SCANDATATYPE_Color ) {
			m_ScanParam.Size.dwBytes *= 3;
		}
		m_ScanParam.Size.dwLines = dwShadingLines;	/* SHADING_Lines */
	}

	/* goto the correct position again... */
	if( dev->usbDev.pSource->DarkShadOrgY >= 0 ) {

		usb_ModuleToHome( dev, SANE_TRUE );
		usb_ModuleMove  ( dev, MOVE_Forward,
								(u_long)dev->usbDev.pSource->ShadingOriginY);
	}

	sprintf( tmp, "fine-white.raw" );
	DBG( _DBG_INFO2, "FINE WHITE Calibration Strip: %s\n", tmp );
	DBG( _DBG_INFO2, "Shad.-Lines = %lu\n", dwShadingLines  );
	DBG( _DBG_INFO2, "Lines       = %lu\n", m_ScanParam.Size.dwLines  );
	DBG( _DBG_INFO2, "Pixels      = %lu\n", m_ScanParam.Size.dwPixels );
	DBG( _DBG_INFO2, "Bytes       = %lu\n", m_ScanParam.Size.dwBytes  );
	DBG( _DBG_INFO2, "Origin.X    = %u\n",  m_ScanParam.Origin.x );

	for( dw = dwShadingLines /*SHADING_Lines*/,
							 dwRead = 0; dw; dw -= m_ScanParam.Size.dwLines ) {

		if( usb_SetScanParameters( dev, &m_ScanParam ) &&
											usb_ScanBegin( dev, SANE_FALSE )) {

			DBG( _DBG_INFO2, "TotalBytes = %lu\n",
											m_ScanParam.Size.dwTotalBytes );

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

				if( 0 == dwRead ) {
					dumpPicInit( &m_ScanParam, tmp );
				}
				
				dumpPic( tmp, pBuf + dwRead, m_ScanParam.Size.dwTotalBytes );

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
	/*
	 * do some reordering on CIS based devices:
     * from RRRRRRR.... GGGGGGGG.... BBBBBBBBB, create RGB RGB RGB ...
     * to use the following code, originally written for CCD devices...
	 */
	if( hw->bReg_0x26 & _ONE_CH_COLOR ) {

    	u_short *dest, *src;
		u_long   dww;

		src = (u_short*)pBuf;

		DBG( _DBG_INFO2, "PhyBytes  = %lu\n", m_ScanParam.Size.dwPhyBytes );
		DBG( _DBG_INFO2, "PhyPixels = %lu\n", m_ScanParam.Size.dwPhyPixels );
		DBG( _DBG_INFO2, "Pixels    = %lu\n", m_ScanParam.Size.dwPixels );
		DBG( _DBG_INFO2, "Bytes     = %lu\n", m_ScanParam.Size.dwBytes  );
		DBG( _DBG_INFO2, "Channels  = %u\n",  m_ScanParam.bChannels );

		for( dwLines = dwShadingLines; dwLines; dwLines-- ) {

			dest = a_wWhiteShading;

			for( dw=dww=0; dw < m_ScanParam.Size.dwPhyPixels; dw++, dww+=3 ) {

				dest[dww]     = src[dw];
				dest[dww + 1] = src[m_ScanParam.Size.dwPhyPixels + dw];
				dest[dww + 2] = src[m_ScanParam.Size.dwPhyPixels * 2 + dw];
			}

			/* copy line back ... */
			memcpy( src, dest, m_ScanParam.Size.dwPhyPixels * 3 * 2 );
			src = &src[m_ScanParam.Size.dwPhyPixels * 3];
		}

		m_ScanParam.bChannels = 3;
	}

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
				pValue->Mono = (u_short)(*pdw / (dwShadingLines - m_wHilight - m_wShadow));
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
  				   (dwShadingLines - m_wHilight - m_wShadow));
		}
	}

	usb_line_statistics( "White", a_wWhiteShading, m_ScanParam.Size.dwPhyPixels,
                          scanning->sParam.bDataType == SCANDATATYPE_Color?1:0);
	return SANE_TRUE;
}

/** for negative film only
 * we need to resize the gain to obtain bright white...
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

/**
 */
static void usb_PrepareCalibration( pPlustek_Device dev )
{
    pScanDef  scanning = &dev->scanning;
	pDCapsDef scaps    = &dev->usbDev.Caps;

	usb_GetSoftwareOffsetGain( dev );

	pScanBuffer = scanning->pScanBuffer;
	pParam      = &scanning->sParam;

	memset( &m_ScanParam, 0, sizeof(ScanParam));

	m_ScanParam.UserDpi   = scaps->OpticDpi;
	m_ScanParam.PhyDpi    = scaps->OpticDpi;
	m_ScanParam.bChannels = scanning->sParam.bChannels;
	m_ScanParam.bBitDepth = 16;
	m_ScanParam.bSource   = scanning->sParam.bSource;
	m_ScanParam.Origin.y  = 0;

	if( scanning->sParam.bDataType == SCANDATATYPE_Color )
		m_ScanParam.bDataType = SCANDATATYPE_Color;
	else
		m_ScanParam.bDataType = SCANDATATYPE_Gray;

	usb_SetMCLK( dev, &m_ScanParam );
 
	/* preset these registers offset/gain */
	a_bRegs[0x38] = a_bRegs[0x39] = a_bRegs[0x3a] = 0;
	a_bRegs[0x3b] = a_bRegs[0x3c] = a_bRegs[0x3d] = 1;
	a_bRegs[0x45] &= ~0x10;

	memset( a_wWhiteShading, 0, _SHADING_BUF );
	memset( a_wDarkShading,  0, _SHADING_BUF );
}

/** usb_DoCalibration
 *
 */
static int usb_DoCalibration( pPlustek_Device dev )
{
    pScanDef  scanning = &dev->scanning;
	pDCapsDef scaps    = &dev->usbDev.Caps;
	pHWDef    hw       = &dev->usbDev.HwSetting;

	DBG( _DBG_INFO, "usb_DoCalibration()\n" );

	if( SANE_TRUE == scanning->fCalibrated )
		return SANE_TRUE;

	/* Go to shading position
     */
	if( !(hw->bReg_0x26 & _ONE_CH_COLOR)) {
     
		DBG( _DBG_INFO, "goto shading position\n" );

		/* HEINER: Currently not clear why Plustek didn't use the ShadingOriginY
		 *         for all modes
		 * It should be okay to remove this and reference to the ShadingOriginY
		 */	
#if 0	
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

			if((hw->motorModel == MODEL_HuaLien) && (scaps->OpticDpi.x==600)) {
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
/*		}*/
	}

	DBG( _DBG_INFO, "shading position reached\n" );

	usb_PrepareCalibration( dev );

	/*
	 * this won't work for Plustek devices!!!
	 */
#if 0
	if( scaps->workaroundFlag & _WAF_BYPASS_CALIBRATION ||
		!(SCANDEF_QualityScan & dev->scanning.dwFlag)) {
#else
	if( scaps->workaroundFlag & _WAF_BYPASS_CALIBRATION ) {
#endif

		DBG( _DBG_INFO, "--> BYPASS\n" );
		a_bRegs[0x38] = a_bRegs[0x39] = a_bRegs[0x3a] = 0;
		a_bRegs[0x3b] = a_bRegs[0x3c] = a_bRegs[0x3d] = 1;

		setAdjGain( dev->adj.rgain, &a_bRegs[0x3b] );
		setAdjGain( dev->adj.ggain, &a_bRegs[0x3c] );
		setAdjGain( dev->adj.bgain, &a_bRegs[0x3d] );

		a_bRegs[0x45] |= 0x10;
		usb_SetMCLK( dev, &scanning->sParam );

	    dumpregs( dev->fd, a_bRegs );
		DBG( _DBG_INFO, "<-- BYPASS\n" );

	} else {

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

    			DBG( _DBG_INFO, "MCLK      = %.3f\n", dMCLK );
    			DBG( _DBG_INFO, "GainRed   = %u\n", a_bRegs[0x3b] );
    			DBG( _DBG_INFO, "GainGreen = %u\n", a_bRegs[0x3c] );
    			DBG( _DBG_INFO, "GainBlue  = %u\n", a_bRegs[0x3d] );

    #if 0
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
    				usb_ResizeWhiteShading( dRed,  a_wWhiteShading,
    												scanning->sParam.swGain[0]);
    				usb_ResizeWhiteShading( dGreen, a_wWhiteShading +
    												m_ScanParam.Size.dwPhyPixels,
    												scanning->sParam.swGain[1]);
    				usb_ResizeWhiteShading( dBlue,  a_wWhiteShading +
    												m_ScanParam.Size.dwPhyPixels*2,
    												scanning->sParam.swGain[2]);
    			}
    			break;

    		case SOURCE_ADF:
    			DBG( _DBG_INFO, "ADF Shading\n" );
    			m_dwIdealGain = IDEAL_GainPositive;
    			if( scanning->sParam.bDataType == SCANDATATYPE_BW ) {
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

    			if( !_IS_PLUSTEKMOTOR(hw->motorModel)) {
    				DBG( _DBG_INFO, "No Plustek model: %udpi\n",
    												scanning->sParam.PhyDpi.x );
					usb_SetMCLK( dev, &scanning->sParam );

    			} else {
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
    			}

    			DBG( _DBG_INFO, "Settings done, so start...\n" );
    			if( !usb_AdjustGain(dev, 0)     || !usb_AdjustOffset(dev) ||
    				!usb_AdjustDarkShading(dev) || !usb_AdjustWhiteShading(dev)) {
    				return _E_INTERNAL;
    			}
    			break;

    		default:
    			if( !_IS_PLUSTEKMOTOR(hw->motorModel)) {
    				DBG( _DBG_INFO, "No Plustek model: %udpi\n",
    												scanning->sParam.PhyDpi.x );
    				usb_SetMCLK( dev, &scanning->sParam );

    			} else 	if( dev->usbDev.Caps.OpticDpi.x == 600 ) {

    				DBG( _DBG_INFO, "Default Shading (600dpi)\n" );

    				if( dev->usbDev.Caps.bCCD == kSONY548 )	{

    					DBG( _DBG_INFO, "CCD - SONY548\n" );
    					if( scanning->sParam.PhyDpi.x <= 75 ) {
    						if( scanning->sParam.bDataType  == SCANDATATYPE_Color )
    							scanning->sParam.dMCLK = dMCLK = 2.5;
    						else if(scanning->sParam.bDataType == SCANDATATYPE_Gray)
    							scanning->sParam.dMCLK = dMCLK = 7.0;
    						else
    							scanning->sParam.dMCLK = dMCLK = 7.0;

    					} else if( scanning->sParam.PhyDpi.x <= 300 ) {
    						if( scanning->sParam.bDataType  == SCANDATATYPE_Color )
    							scanning->sParam.dMCLK = dMCLK = 3.0;
    						else if(scanning->sParam.bDataType == SCANDATATYPE_Gray)
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
    						else if( scanning->sParam.bDataType == SCANDATATYPE_Gray )
    							scanning->sParam.dMCLK = dMCLK = 6.0;
    						else
    							scanning->sParam.dMCLK = dMCLK = 4.0;
    					} else {
    						if(scanning->sParam.bDataType  == SCANDATATYPE_Color)
    							scanning->sParam.dMCLK = dMCLK = 6.0;
    						else if(scanning->sParam.bDataType == SCANDATATYPE_Gray)
    							scanning->sParam.dMCLK = dMCLK = 7.0;
    						else
    							scanning->sParam.dMCLK = dMCLK = 6.0;
    					}

    				} else if( dev->usbDev.Caps.bPCB == 0x02 ) {
    					DBG( _DBG_INFO, "PCB - 0x02\n" );
    					if( scanning->sParam.PhyDpi.x > 300 )
    						scanning->sParam.dMCLK = dMCLK = ((scanning->sParam.bDataType == SCANDATATYPE_Color)? 6: 16);
    					else if( scanning->sParam.PhyDpi.x > 150 )
    						scanning->sParam.dMCLK = dMCLK = ((scanning->sParam.bDataType == SCANDATATYPE_Color)?4.5: 13.5);
    					else
    						scanning->sParam.dMCLK = dMCLK = ((scanning->sParam.bDataType == SCANDATATYPE_Color)?3: 8);

    				}  else if( dev->usbDev.Caps.bButtons ) { /* with lens Shading piece (with gobo) */

    					DBG( _DBG_INFO, "CAPS - Buttons\n" );
    					scanning->sParam.dMCLK = dMCLK = ((scanning->sParam.bDataType == SCANDATATYPE_Color)?3: 6);
    					if( dev->usbDev.HwSetting.motorModel == MODEL_KaoHsiung )	{
    						if( dev->usbDev.Caps.bCCD == kNEC3799 ) {
    							if( scanning->sParam.PhyDpi.x > 300 )
    								scanning->sParam.dMCLK = dMCLK = ((scanning->sParam.bDataType == SCANDATATYPE_Color)? 6: 13);
    							else if(scanning->sParam.PhyDpi.x > 150)
    								scanning->sParam.dMCLK = dMCLK = ((scanning->sParam.bDataType == SCANDATATYPE_Color)?4.5: 13.5);
    							else
    								scanning->sParam.dMCLK = dMCLK = ((scanning->sParam.bDataType == SCANDATATYPE_Color)?3: 6);
    						} else {
    							scanning->sParam.dMCLK = dMCLK = ((scanning->sParam.bDataType == SCANDATATYPE_Color)?3: 6);
    						}
    					} else { /* motorModel == MODEL_Hualien */
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
    			DBG( _DBG_INFO2, "###### ADJUST GAIN (COARSE)#######\n" );

    			if( !usb_AdjustGain(dev, 0)) {
    				DBG( _DBG_ERROR, "Coarse Calibration failed!!!\n" );
    				return _E_INTERNAL;
    			}
    			DBG( _DBG_INFO2, "###### ADJUST OFFSET (COARSE) ####\n" );
    			if( !usb_AdjustOffset(dev)) {
    				DBG( _DBG_ERROR, "Coarse Calibration failed!!!\n" );
    				return _E_INTERNAL;
    			}
    			DBG( _DBG_INFO2, "###### ADJUST DARK (FINE) ########\n" );
    			if( !usb_AdjustDarkShading(dev)) {
    				DBG( _DBG_ERROR, "Fine Calibration failed!!!\n" );
    				return _E_INTERNAL;
    			}
    			DBG( _DBG_INFO2, "###### ADJUST WHITE (FINE) #######\n" );
				if( !usb_AdjustWhiteShading(dev)) {
    				DBG( _DBG_ERROR, "Fine Calibration failed!!!\n" );
    				return _E_INTERNAL;
    			}
    			break;
    	}
	}	

	/*
	 * home the sensor after calibration
	 */
	if( _IS_PLUSTEKMOTOR(hw->motorModel)) {

		if( hw->motorModel != MODEL_Tokyo600 ) {
			usb_ModuleMove  ( dev, MOVE_Forward, hw->wMotorDpi / 5 );
			usb_ModuleToHome( dev, SANE_TRUE );
		}
	} else {

		usb_ModuleMove( dev, MOVE_Forward, 1 );
		usleep( 150 );
		usb_ModuleToHome( dev, SANE_TRUE );
	}

	if( scanning->sParam.bSource == SOURCE_ADF ) {

		if( scaps->bCCD == kNEC3778 )
			usb_ModuleMove( dev, MOVE_Forward, 1000 );

		else /* if( scaps->bCCD == kNEC3799) */
			usb_ModuleMove( dev, MOVE_Forward, 3 * 300 + 38 );

		usb_MotorOn( dev->fd, SANE_FALSE );
	}

	scanning->fCalibrated = SANE_TRUE;
	DBG( _DBG_INFO, "Calibration done\n-----------------------\n" );
	DBG( _DBG_INFO, "Static Gain:\n" );
	DBG( _DBG_INFO, "REG[0x3b] = %u\n", a_bRegs[0x3b] );
	DBG( _DBG_INFO, "REG[0x3c] = %u\n", a_bRegs[0x3c] );
	DBG( _DBG_INFO, "REG[0x3d] = %u\n", a_bRegs[0x3d] );
	DBG( _DBG_INFO, "Static Offset:\n" );
	DBG( _DBG_INFO, "REG[0x38] = %u\n", a_bRegs[0x38] );
	DBG( _DBG_INFO, "REG[0x39] = %u\n", a_bRegs[0x39] );
	DBG( _DBG_INFO, "REG[0x3a] = %u\n", a_bRegs[0x3a] );
	DBG( _DBG_INFO, "-----------------------\n" );

	return SANE_TRUE;
}

/** usb_DownloadShadingData
 * according to the job id, different registers or DRAM areas are set
 * in preparation for calibration or scanning
 */
static SANE_Bool usb_DownloadShadingData( pPlustek_Device dev, u_char bJobID )
{
	pDCapsDef scaps = &dev->usbDev.Caps;
	pHWDef    hw    = &dev->usbDev.HwSetting;

	DBG( _DBG_INFO2, "usb_DownloadShadingData(%u)\n", bJobID );

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

			/* set RAM configuration AND
             * Gain = Multiplier Coefficient/16384
             * CFG Register 0x40/0x41 for Multiplier Coefficient Source
             * External DRAM for Offset Coefficient Source
             */
			a_bRegs[0x42] = (u_char)(( hw->wDRAMSize > 512)? 0x64: 0x24);

			_UIO(sanei_lm983x_write( dev->fd, 0x40,
									 &a_bRegs[0x40], 0x42-0x40+1, SANE_TRUE ));
			break;

		case PARAM_Scan:
			{
#if 0
				if( scaps->workaroundFlag & _WAF_BYPASS_CALIBRATION ||
					!(SCANDEF_QualityScan & dev->scanning.dwFlag)) {
#else
				if( scaps->workaroundFlag & _WAF_BYPASS_CALIBRATION ) {
#endif
					DBG( _DBG_INFO, "--> BYPASS\n" );

					/* set RAM configuration AND
		             * Bypass Multiplier
		             * CFG Register 0x40/0x41 for Multiplier Coefficient Source
        		     * CFG Register 0x3e/0x3f for Offset Coefficient Source
		             */
					a_bRegs[0x03] = 0;
					a_bRegs[0x40] = 0x40;
					a_bRegs[0x41] = 0x00;
					a_bRegs[0x42] = (u_char)((hw->wDRAMSize > 512)? 0x61:0x21);
					if( !usbio_WriteReg( dev->fd, 0x03, a_bRegs[0x03]))
						return SANE_FALSE;

					_UIO(sanei_lm983x_write( dev->fd, 0x40,
											 &a_bRegs[0x40], 3, SANE_TRUE));
					break;
				}

				if( _LM9831 != hw->chip )
					m_dwPixels = m_ScanParam.Size.dwPhyPixels;

				if( scaps->workaroundFlag & _WAF_SKIP_FINE ) {
					DBG( _DBG_INFO, "Skipping fine calibration\n" );
    				a_bRegs[0x42] = (u_char)(( hw->wDRAMSize > 512)? 0x60: 0x20);
        
					if( !usbio_WriteReg( dev->fd, 0x42, a_bRegs[0x42]))
						return SANE_FALSE;

					break;						
				}						
										
				DBG( _DBG_INFO, "Downloading %lu pixels\n", m_dwPixels );

				/* Download the dark & white shadings to LM983x */
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
				
				/* set RAM configuration AND
	             * Gain = Multiplier Coefficient/16384
                 * External DRAM for Multiplier Coefficient Source
                 * External DRAM for Offset Coefficient Source
                 */
				a_bRegs[0x42] = (u_char)((hw->wDRAMSize > 512)? 0x66: 0x26);

				if( scaps->workaroundFlag & _WAF_SKIP_WHITEFINE ) {
					DBG( _DBG_INFO,"Skipping fine white calibration result\n");
					a_bRegs[0x42] = (u_char)(( hw->wDRAMSize > 512)? 0x64: 0x24);
				}

				if( !usbio_WriteReg( dev->fd, 0x42, a_bRegs[0x42]))
					return SANE_FALSE;
			}
			break;

		default:
			/* for coarse calibration and "black fine" */
			a_bRegs[0x3e] = 0;
			a_bRegs[0x3f] = 0;
			a_bRegs[0x40] = 0x40;
			a_bRegs[0x41] = 0x00;

			/* set RAM configuration AND
             * GAIN = Multiplier Coefficient/16384
             * CFG Register 0x40/0x41 for Multiplier Coefficient Source
             * CFG Register 0x3e/0x3f for Offset Coefficient Source
             */
			a_bRegs[0x42] = (u_char)((hw->wDRAMSize > 512)? 0x60: 0x20);

			_UIO(sanei_lm983x_write( dev->fd, 0x3e, &a_bRegs[0x3e],
									  0x42 - 0x3e + 1, SANE_TRUE ));
			break;									
	}

	return SANE_TRUE;
}

/* END PLUSTEK-USBSHADING.C .................................................*/
