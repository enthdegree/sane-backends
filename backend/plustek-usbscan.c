/*.............................................................................
 * Project : SANE library for Plustek USB flatbed scanners.
 *.............................................................................
 * File:	 plustek-usbscan.c
 *.............................................................................
 *
 * based on sources acquired from Plustek Inc.
 * Copyright (C) 2001-2002 Gerhard Jaeger <gerhard@gjaeger.de>
 *.............................................................................
 * History:
 * 0.40 - starting version of the USB support
 * 0.41 - minor fixes
 * 0.42 - added some stuff for CIS devices
 * 0.43 - no changes
 * 0.44 - added CIS specific settings and calculations
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

/** @file plustek-usbscan.c
 */
static u_char     bMaxITA;

static SANE_Bool  m_fAutoPark;
static SANE_Bool  m_fFirst;
static double     m_dHDPIDivider;
static double     m_dMCLKDivider;
static pScanParam m_pParam;
static u_char     m_bLineRateColor;
static u_char     m_bCM;
static u_char	  m_bIntTimeAdjust;
static u_char	  m_bOldScanData;
static u_short    m_wFastFeedStepSize;
static u_short    m_wLineLength;
static u_short	  m_wStepSize;
static u_long     m_dwPauseLimit;

static SANE_Bool  m_fStart = SANE_FALSE;

/* Prototype... */
static SANE_Bool usb_DownloadShadingData( pPlustek_Device, u_char );

/**
 * @param val1 -
 * @param val2 -
 * @return
 */
static u_long usb_min( u_long val1, u_long val2 )
{
	if( val1 > val2 )
		return val2;

	return val1;
}

/**
 * @param val1 -
 * @param val2 -
 * @return
 */
static u_long usb_max( u_long val1, u_long val2 )
{
	if( val1 > val2 )
		return val1;

	return val2;
}

/**
 * This function is used to detect a cancel condition,
 * our ESC key is the SIGUSR1 signal. It is sent by the backend when the
 * cancel button has been pressed
 *
 * @param - none
 * @return the function returns SANE_TRUE if a cancel condition has been
 *  detected, if not, it returns SANE_FALSE
 */
static SANE_Bool usb_IsEscPressed( void )
{
	sigset_t sigs;
	
	sigpending( &sigs );
	if( sigismember( &sigs, SIGUSR1 )) {
		DBG( _DBG_INFO, "SIGUSR1 is pending --> Cancel detected\n" );
		return SANE_TRUE;
	}

	return SANE_FALSE;
}

/**
 * Affected registers:<br>
 * 0x09 - Horizontal DPI divider HDPI_DIV<br>
 *
 * @param dev  - pointer to our device structure,
 *               it should contain all we need
 * @param xdpi - user specified horizontal resolution
 * @return -
 */
static u_short usb_SetAsicDpiX( pPlustek_Device dev, u_short xdpi )
{
    pScanDef  scanning = &dev->scanning;
	pDCapsDef scaps    = &dev->usbDev.Caps;

	/*
     * limit xdpi to lower value for certain devices...
     */
	if( scaps->OpticDpi.x == 1200 &&
		scanning->sParam.bDataType != SCANDATATYPE_Color &&
		xdpi < 150 &&
		scanning->sParam.bDataType == SCANDATATYPE_BW ) {
		xdpi = 150;
		DBG( _DBG_INFO, "LIMIT XDPI to %udpi\n", xdpi );
	}

	m_dHDPIDivider = (double)scaps->OpticDpi.x / xdpi;

	if (m_dHDPIDivider < 1.5)
	{
		m_dHDPIDivider = 1.0;
		a_bRegs[0x09]  = 0;
	}
	else if (m_dHDPIDivider < 2.0)
	{
		m_dHDPIDivider = 1.5;
		a_bRegs[0x09]  = 1;
	}
	else if (m_dHDPIDivider < 3.0)
	{
		m_dHDPIDivider = 2.0;
		a_bRegs[0x09]  = 2;
	}
	else if (m_dHDPIDivider < 4.0)
	{
		m_dHDPIDivider = 3.0;
		a_bRegs[0x09]  = 3;
	}
	else if (m_dHDPIDivider < 6.0)
	{
		m_dHDPIDivider = 4.0;
		a_bRegs[0x09]  = 4;
	}
	else if (m_dHDPIDivider < 8.0)
	{
		m_dHDPIDivider = 6.0;
		a_bRegs[0x09]  = 5;
	}
	else if (m_dHDPIDivider < 12.0)
	{
		m_dHDPIDivider = 8.0;
		a_bRegs[0x09]  = 6;
	}
	else
	{
		m_dHDPIDivider = 12.0;
		a_bRegs[0x09]  = 7;
	}

	/* adjust, if any turbo/preview mode is set, should be 0 here... */
	if( a_bRegs[0x0a] )
		a_bRegs[0x09] -= ((a_bRegs[0x0a] >> 2) + 2);

	DBG( _DBG_INFO, "HDPI: %.3f\n", m_dHDPIDivider );
	return (u_short)((double)scaps->OpticDpi.x / m_dHDPIDivider);
}

/**
 * @param dev  - pointer to our device structure,
 *               it should contain all we need
 * @param ydpi - user specified vertical resolution
 * @return -
 */
static u_short usb_SetAsicDpiY( pPlustek_Device dev, u_short ydpi )
{
    pScanDef  scanning = &dev->scanning;
	pDCapsDef sCaps    = &dev->usbDev.Caps;
	pHWDef    hw       = &dev->usbDev.HwSetting;

	u_short	wMinDpi, wDpi;

	wMinDpi = sCaps->OpticDpi.y / sCaps->bSensorDistance;
	
	/* Here we might have to check against the MinDpi value ! */
	
	wDpi = (ydpi + wMinDpi - 1) / wMinDpi * wMinDpi;
	/*
	 * HEINER: added '*2'
	 */
	if( wDpi > sCaps->OpticDpi.y * 2 )
		wDpi = sCaps->OpticDpi.y * 2;

	if( (hw->motorModel == MODEL_Tokyo600) ||
		!_IS_PLUSTEKMOTOR(hw->motorModel)) {
		/* return wDpi; */
	} else if( sCaps->wFlags & DEVCAPSFLAG_Adf && sCaps->OpticDpi.x == 600 ) {
		/* for ADF scanner color mode 300 dpi big noise */
		if( scanning->sParam.bDataType == SCANDATATYPE_Color &&
			scanning->sParam.bBitDepth > 8 && wDpi < 300 ) {
			wDpi = 300;
		}
	} else if( sCaps->OpticDpi.x == 1200 ) {
		if( scanning->sParam.bDataType != SCANDATATYPE_Color && wDpi < 200) {
			wDpi = 200;
		}
	}

	DBG( _DBG_INFO2, "YDPI=%u, MinDPIY=%u\n", wDpi, wMinDpi );
	return wDpi;
}

/**
 *
 * Affected registers:<br>
 * 0x26 - 0x27 - Color Mode settings<br>
 * 0x0f - 0x18 - Sensor Configuration - directly from the HwDefs<br>
 * 0x09        - add Data Mode and Pixel Packing<br>
 *
 * @param dev    - pointer to our device structure,
 *                 it should contain all we need
 * @param pParam - pointer to the current scan parameters
 * @return - Nothing
 */
static void usb_SetColorAndBits( pPlustek_Device dev, pScanParam pParam )
{
	pHWDef hw = &dev->usbDev.HwSetting;

	/*
     * Set pixel packing, data mode and AFE operation
     */
	switch( pParam->bDataType ) {
		case SCANDATATYPE_Color:
			m_bCM = 3;
			a_bRegs[0x26] = hw->bReg_0x26 & 0x7;

			/* if set to one channel color, we select the blue channel
             * as input source, this is the default, but I don't know
             * what happens, if we deselect this
             */
			if( a_bRegs[0x26] & _ONE_CH_COLOR )
				a_bRegs[0x26] |= (_BLUE_CH | 0x01);

			memcpy( &a_bRegs[0x0f], hw->bReg_0x0f_Color, 10 );
			break;

		case SCANDATATYPE_Gray:
			m_bCM = 1;
			a_bRegs[0x26] = (hw->bReg_0x26 & 0x18) | 0x04;
			memcpy( &a_bRegs[0x0f], hw->bReg_0x0f_Mono, 10 );
			break;

		case SCANDATATYPE_BW:
			m_bCM = 1;
			a_bRegs[0x26] = (hw->bReg_0x26 & 0x18) | 0x04;
			memcpy( &a_bRegs [0x0f], hw->bReg_0x0f_Mono, 10 );
			break;
	}
			
	a_bRegs[0x27] = hw->bReg_0x27;

	if( pParam->bBitDepth > 8 ) {
		a_bRegs[0x09] |= 0x20;         /* 14/16bit image data */

	} else if( pParam->bBitDepth == 8 ) {
		a_bRegs[0x09] |= 0x18;        /* 8bits/per pixel */
	}
}

/**
 * Calculated basic image settings like the number of physical bytes per line
 * etc...
 * Affected registers:<br>
 * 0x22/0x23 - Data Pixels Start<br>
 * 0x24/0x25 - Data Pixels End<br>
 * 0x4a/0x4b - Full Steps to Skip at Start of Scan
 *
 * @param dev    - pointer to our device structure,
 *                 it should contain all we need
 * @param pParam - pointer to the current scan parameters
 * @return - Nothing
 */
static void usb_GetScanRect( pPlustek_Device dev, pScanParam pParam )
{
	u_short   wDataPixelStart, wLineEnd;

	pDCapsDef sCaps = &dev->usbDev.Caps;
	pHWDef    hw    = &dev->usbDev.HwSetting;

	/* Convert pixels to physical dpi based */
	pParam->Size.dwValidPixels = pParam->Size.dwPixels * pParam->PhyDpi.x / pParam->UserDpi.x;

/* HEINER: check ADF stuff... */
#if 0
	if(pParam->bCalibration != PARAM_Gain && pParam->bCalibration != PARAM_Offset && ScanInf.m_fADF)
		wDataPixelStart = 2550 * sCaps->OpticDpi.x / 300UL - (u_short)(m_dHDPIDivider * pParam->Size.dwValidPixels + 0.5);
	else
#endif
		wDataPixelStart = (u_short)((u_long) pParam->Origin.x * sCaps->OpticDpi.x / 300UL);

	/* Data output from NS983X should be times of 2-byte and every line
     * will append 2 status bytes
     */
	if (pParam->bBitDepth == 1)
	{
		/* Pixels should be times of 16 */
		pParam->Size.dwPhyPixels = (pParam->Size.dwValidPixels + 15UL) & 0xfffffff0UL;
		pParam->Size.dwPhyBytes = pParam->Size.dwPhyPixels / 8UL + 2UL;
	}
	else if (pParam->bBitDepth == 8)
	{
		/* Pixels should be times of 2 */
		pParam->Size.dwPhyPixels = (pParam->Size.dwValidPixels + 1UL) & 0xfffffffeUL;
		pParam->Size.dwPhyBytes = pParam->Size.dwPhyPixels * pParam->bChannels + 2UL;
	}
	else /* pParam->bBitDepth == 16 */
	{
		pParam->Size.dwPhyPixels = pParam->Size.dwValidPixels;
		pParam->Size.dwPhyBytes = pParam->Size.dwPhyPixels * 2 * pParam->bChannels + 2UL;
	}

	/* Compute data start pixel */
	wDataPixelStart = (u_short)((u_long) pParam->Origin.x * sCaps->OpticDpi.x / 300UL);
	if (pParam->bCalibration != PARAM_Gain && pParam->bCalibration != PARAM_Offset)
	{
/* HEINER: check ADF stuff... */
#if 0
		if(ScanInf.m_fADF)
			wDataPixelStart = 2550 * sCaps->OpticDpi.x / 300UL - (u_short)(m_dHDPIDivider * pParam->Size.dwValidPixels + 0.5);
#endif
		wDataPixelStart += hw->wActivePixelsStart;
	}
	wLineEnd = wDataPixelStart + (u_short)(m_dHDPIDivider * pParam->Size.dwPhyPixels + 0.5);

	DBG( _DBG_INFO, "DataPixelStart=%u, LineEnd=%u\n",
				     wDataPixelStart, wLineEnd );

	a_bRegs[0x22] = _HIBYTE( wDataPixelStart );
	a_bRegs[0x23] = _LOBYTE( wDataPixelStart );
	a_bRegs[0x24] = _HIBYTE( wLineEnd );
	a_bRegs[0x25] = _LOBYTE( wLineEnd );

	/* Y origin */
	if( pParam->bCalibration == PARAM_Scan ) {

		if( hw->motorModel == MODEL_Tokyo600 ) {

			if(pParam->PhyDpi.x <= 75)
				pParam->Origin.y += 20;
			else if(pParam->PhyDpi.x <= 100)
			{
				if (pParam->bDataType == SCANDATATYPE_Color)
					pParam->Origin.y += 0;
				else
					pParam->Origin.y -= 6;
			}
			else if(pParam->PhyDpi.x <= 150)
			{
				if (pParam->bDataType == SCANDATATYPE_Color)
					pParam->Origin.y -= 0;
			}
			else if(pParam->PhyDpi.x <= 200)
			{
				if (pParam->bDataType == SCANDATATYPE_Color)
					pParam->Origin.y -= 10;
				else
					pParam->Origin.y -= 4;
			}
			else if(pParam->PhyDpi.x <= 300)
			{
				if (pParam->bDataType == SCANDATATYPE_Color)
					pParam->Origin.y += 16;
				else
					pParam->Origin.y -= 18;
			}
			else if(pParam->PhyDpi.x <= 400)
			{
				if (pParam->bDataType == SCANDATATYPE_Color)
					pParam->Origin.y += 15;
				else if(pParam->bDataType == SCANDATATYPE_BW)
					pParam->Origin.y += 4;
			}
			else /* if(pParam->PhyDpi.x <= 600) */
			{
				if (pParam->bDataType == SCANDATATYPE_Gray)
					pParam->Origin.y += 4;
			}
		}

		/* Add gray mode offset (Green offset, we assume the CCD are
         * always be RGB or BGR order).
         */
		if (pParam->bDataType != SCANDATATYPE_Color)
			pParam->Origin.y += (u_long)(300UL * sCaps->bSensorDistance / sCaps->OpticDpi.y );
	}

	pParam->Origin.y = (u_short)((u_long)pParam->Origin.y * hw->wMotorDpi / 300UL);

	/* Something wrong, but I can not find it. */
	if( hw->motorModel == MODEL_HuaLien && sCaps->OpticDpi.x == 600)
		pParam->Origin.y = pParam->Origin.y * 297 / 298;

	DBG( _DBG_INFO, "Full Steps to Skip at Start = 0x%04x\n", pParam->Origin.y );

	a_bRegs[0x4a] = _HIBYTE( pParam->Origin.y );
	a_bRegs[0x4b] = _LOBYTE( pParam->Origin.y );
}

/** calculate default phase difference DPD
 *
 */
static void usb_GetDPD( pPlustek_Device dev  )
{
	int    qtcnt;	/* quarter speed count count reg 51 b2..3 */
	int    hfcnt;	/* half speed count reg 51 b0..1          */
	int    strev;	/* steps to reverse reg 50                */
	int    dpd;	    /* calculated dpd reg 52:53               */
	int    st;		/* step size reg 46:47                    */

	pHWDef hw = &dev->usbDev.HwSetting;

	qtcnt = (a_bRegs [0x51] & 0x30) >> 4;	/* quarter speed count */
	hfcnt = (a_bRegs [0x51] & 0xc0) >> 6;	/* half speed count    */

	if( _LM9831 == hw->chip )
		strev = a_bRegs [0x50] & 0x3f;		/* steps to reverse */
	else /* LM9832/3 */
	{
		if (qtcnt == 3)
			qtcnt = 8;
		if (hfcnt == 3)
			hfcnt = 8;
		strev = a_bRegs[0x50]; /* steps to reverse */
	}

	st = a_bRegs[0x46] * 256 + a_bRegs[0x47]; /* scan step size */

	if (m_wLineLength == 0)
		dpd = 0;
	else
	{
		dpd = (((qtcnt * 4) + (hfcnt * 2) + strev) * 4 * st) %
					 (m_wLineLength * m_bLineRateColor);
		dpd = m_wLineLength * m_bLineRateColor - dpd;
	}

	DBG( _DBG_INFO2, "DPD =%u, step size=%u, steps2rev=%u\n", dpd, st, strev );
	DBG( _DBG_INFO2, "llen=%u, lineRateColor=%u, qtcnt=%u, hfcnt=%u\n",
						m_wLineLength, m_bLineRateColor, qtcnt, hfcnt );

	a_bRegs[0x51] |= (u_char)((dpd >> 16) & 0x03);
	a_bRegs[0x52] = (u_char)(dpd >> 8);
	a_bRegs[0x53] = (u_char)(dpd & 0xFF);
}

/**
 * Plusteks' poor-man MCLK calculation...
 *
 */
static double usb_GetMCLKDivider( pPlustek_Device dev, pScanParam pParam )
{
	double dMaxIntegrationTime;
	double dMaxMCLKDivider;

	pDCapsDef sCaps = &dev->usbDev.Caps;
	pHWDef    hw    = &dev->usbDev.HwSetting;

	DBG( _DBG_INFO, "usb_GetMCLKDivider()\n" );

	m_dMCLKDivider = pParam->dMCLK;

	if (m_dHDPIDivider*m_dMCLKDivider >= 5.3/*6*/)
		m_bIntTimeAdjust = 0;
	else
		m_bIntTimeAdjust = ceil( 5.3/*6.0*/ / (m_dHDPIDivider*m_dMCLKDivider));

	DBG( _DBG_INFO, "Integration Time Adjust = %u (HDPI=%.3f,MCLKD=%.3f)\n",
							m_bIntTimeAdjust, m_dHDPIDivider, m_dMCLKDivider );

	if( pParam->bCalibration == PARAM_Scan ) {

		/*  Compare Integration with USB speed to find the best ITA value */
		if( pParam->bBitDepth > 8 )	{

			while( pParam->Size.dwPhyBytes >
					(m_dMCLKDivider * m_bCM * m_wLineLength / 6 * 9 / 10) *
 													  (1 + m_bIntTimeAdjust)) {
				m_bIntTimeAdjust++;
			}	

			if( hw->motorModel == MODEL_HuaLien &&
				sCaps->bCCD == kNEC3799 && m_bIntTimeAdjust > bMaxITA) {
				m_bIntTimeAdjust = bMaxITA;
			}
		}
	}

	a_bRegs[0x08] = (u_char)((m_dMCLKDivider - 1) * 2);
	a_bRegs[0x19] = m_bIntTimeAdjust;

	if( m_bIntTimeAdjust != 0 ) {

		m_wStepSize = (u_short)((u_long) m_wStepSize *
									(m_bIntTimeAdjust + 1) / m_bIntTimeAdjust);
		if( m_wStepSize < 2 )
			m_wStepSize = 2;

		a_bRegs[0x46] = _HIBYTE(m_wStepSize);
		a_bRegs[0x47] = _LOBYTE(m_wStepSize);

		DBG( _DBG_INFO2, "Stepsize = %u, 0x46=0x%02x 0x47=0x%02x\n",
						  m_wStepSize, a_bRegs[0x46], a_bRegs[0x47] );
	    usb_GetDPD( dev );
	}
	
	/* Compute maximum MCLK divider base on maximum integration time for
     * high lamp PWM, use equation 4
     */
	dMaxIntegrationTime = hw->dIntegrationTimeHighLamp;
	dMaxMCLKDivider = (double)dwCrystalFrequency * dMaxIntegrationTime /
					   (1000 * 8 * m_bCM * m_wLineLength);

	/* Determine PWM setting */
	if( m_dMCLKDivider > dMaxMCLKDivider ) {

		a_bRegs[0x2A] = _HIBYTE( hw->wGreenPWMDutyCycleLow );
		a_bRegs[0x2B] = _LOBYTE( hw->wGreenPWMDutyCycleLow );

	} else {

		a_bRegs[0x2A] = _HIBYTE( hw->wGreenPWMDutyCycleHigh );
		a_bRegs[0x2B] = _LOBYTE( hw->wGreenPWMDutyCycleHigh );
	}

	DBG( _DBG_INFO, "Current MCLK Divider = %f\n", m_dMCLKDivider );
	
/*	return m_dMCLKDivider; */

/*
 * this is the original NS code adapted to the backends' needs
 */
/*-------------------------------------------------------
 * FUNCTION	:	ComputeCalibrationMClk(int *regs)
 *
 * DESCRIPTION	:	calculate the mclk for calibration and scanning
 *
 * if the pctransfer rate has not been computed, compute it
 *compute ideal_step_size
 *calculate ideal mclk divider from average pc transfer rate
 *calculate mclk divider for max integration time
 *calculate mclk divider for max motor speed
 *check if stepsize must be adjusted because max integration time
 *gives too fast motor speed
 * else limit mclk divider between max integration time and max motor speed
 *
 * scaninfo.yres is the vertical res that may be changed
 *
 * RETURNS	:	calculated mclk
 * registers set: 19
 *			46:47 step size
 *			4e pause limit
 *			52:53 dpd
*/
#if 1
	{

/*#define PCTR            2000000*/
#define PCTR            1000000 /* --> typical transfer rate... */
#define MCLKDIV_SCALING 2

		int    minmclk, maxmclk, mclkdiv, max_mclkdiv, min_mclkdiv;
		int    stepsize;
		int    j;
		int    pixelbits, pixelsperline;
		unsigned long PCTransferRate = PCTR;
		double dpi;
		double min_integration_time;

/*	Hardware.merlininfo.timeadj = regs[0x19] = 0; already set !*/

		/* the first calibration needs to have the pc transfer rate
		 * 0. if the pctransfer rate has not been computed
		 * 1. compute ideal_step_size
	     * step size = tr*vres/(4*motor steps/in)
    	 * tr is from the dpd calculation but does include the cm component
	     * vres is the vertical resolution and is stored in scaninfo.yres
    	 * motor steps/in is from ini file
	     */
		stepsize = 256 * a_bRegs[0x46] + a_bRegs[0x47];

		/* 1.5, calculate the min mclk divider based on min integration time
		 * save mclk divider as int*2 value
		 * calculate the max mclk divider and save as *2
		 * r=24 for pixel rate, 8 for line rate
		 */

		/* use high or low res min integration time */
		min_integration_time = ((a_bRegs[0x9]&7) > 2 ?
								hw->dMinIntegrationTimeLowres:
								hw->dMinIntegrationTimeHighres);

		minmclk = (int) ceil((double)MCLKDIV_SCALING *
					dwCrystalFrequency *
					min_integration_time
					/((double) 1000. * 8 * m_bCM * m_wLineLength));

		minmclk = usb_max(minmclk, MCLKDIV_SCALING);

		maxmclk = (int) (32.5*MCLKDIV_SCALING + .5); /*round to be sure proper int*/

		DBG( _DBG_INFO2, "lower mclkdiv limit=%f\n",(double)minmclk/MCLKDIV_SCALING );
		DBG( _DBG_INFO2, "upper mclkdiv limit=%f\n",(double)maxmclk/MCLKDIV_SCALING );

       /*
        * 2. calculate ideal mclk divider from average pc transfer rate
        * ideal mclkdiv = pixelsinline*clock in/(8*line width*data rate)
        * *2 and round up
        * byte in line from calculation
        * line width = tr from dpd calculation
        */
       /* get the bits per pixel */
		switch( a_bRegs[0x9] & 0x38 ) {
			case 0:	    pixelbits=1; break;
			case 0x8:   pixelbits=2; break;
			case 0x10:  pixelbits=4; break;
			case 0x18:  pixelbits=8; break;
			default:	pixelbits=16;break;
		}

		/* compute the horizontal dpi (pixels per inch) */
		j   = a_bRegs[0x9] & 0x7;
		dpi = ((j&1)*.5+1)*(j&2?2:1)*(j&4?4:1);	

		pixelsperline = (int)((256 * a_bRegs[0x24] + a_bRegs[0x25] -
							   256 * a_bRegs[0x22] - a_bRegs[0x23])
						     *pixelbits/(dpi * 8));

		mclkdiv = (int) ceil ((double) MCLKDIV_SCALING * pixelsperline *
					dwCrystalFrequency
					/((double) 8. * m_wLineLength * PCTransferRate));

		DBG( _DBG_INFO2, "mclkdiv before limit=%f\n", (double)mclkdiv/MCLKDIV_SCALING);

		mclkdiv = usb_max( mclkdiv,minmclk );
		mclkdiv = usb_min( mclkdiv,maxmclk );

		DBG( _DBG_INFO2, "mclkdiv after limit=%f\n", (double)mclkdiv/MCLKDIV_SCALING);

#if 1
		/* limited by the transfer speed... */
    	if (PCTransferRate == PCTR)
    	{
    		{
    			int mult,timeadj;
    			mult = timeadj = m_bIntTimeAdjust;
    			if (!mult) mult++;
    			while (mclkdiv*dpi*mult < 6.*MCLKDIV_SCALING)
    			{
    				mclkdiv++; /* for now */
    			}
    	/*		Hardware.merlininfo.timeadj = regs[0x19] = timeadj;		*/
    		}
    		DBG( _DBG_INFO, "PC-Rate mclkdiv=%f\n", (double)mclkdiv/MCLKDIV_SCALING );
/*    		return mclkdiv; */
			
			return m_dMCLKDivider; /* = ((double)mclkdiv/MCLKDIV_SCALING); */
    	}
#endif
		DBG( _DBG_INFO2, "mclkdiv=%f\n",(double)mclkdiv/MCLKDIV_SCALING );
		DBG( _DBG_INFO2, "pixel bytes per line=%d\n",pixelsperline*m_bCM);
		DBG( _DBG_INFO2, "linewidth=%d\n", m_wLineLength);

        /*
		 * 3. calculate max mclk divider based on max integration time
		 * max mclkdiv = (clock in * max integration time)/(r * line width)
		 * *2 and round down
		 * limit to min,max
		 */

		max_mclkdiv = (int) floor((double) MCLKDIV_SCALING *
								dwCrystalFrequency * hw->dIntegrationTimeLowLamp
								/((double) 1000. * 8 * m_bCM * m_wLineLength));

		DBG( _DBG_INFO2, "max mclkdiv before limit=%f\n", (double)max_mclkdiv/MCLKDIV_SCALING);

		max_mclkdiv = usb_max( max_mclkdiv,minmclk );
		max_mclkdiv = usb_min( max_mclkdiv,maxmclk );

		DBG( _DBG_INFO2, "max mclkdiv after limit=%f\n", (double)max_mclkdiv/MCLKDIV_SCALING);

        /*
         * 4. calculate min mclk divider based on max motor speed
         *    min mclkdiv = clock in/(stepsize*max motor speed*r*step_per_inch*4)
         *   *2 and round up
         *    limit to min,max
         */
		min_mclkdiv = (int) ceil((double) MCLKDIV_SCALING * dwCrystalFrequency /
				    ((double) stepsize * hw->dMaxMotorSpeed *8 * m_bCM *
					 hw->wMotorDpi * 4.));

		DBG( _DBG_INFO2, "min mclkdiv before limit=%f\n", (double)min_mclkdiv/MCLKDIV_SCALING);

		min_mclkdiv = usb_max( min_mclkdiv,minmclk );
		min_mclkdiv = usb_min( min_mclkdiv,maxmclk );

		DBG( _DBG_INFO2, "min mclkdiv after limit=%f\n", (double)min_mclkdiv/MCLKDIV_SCALING);

		/* 5.Check if stepsize must be adjusted because max integration
		 * time gives too fast motor speed
		 */
		if( min_mclkdiv > max_mclkdiv )	{

			double x;

			mclkdiv     = max_mclkdiv;
			min_mclkdiv = mclkdiv;				

		    x = ((double)dwCrystalFrequency /
						((double)hw->wMotorDpi * 4. * hw->dMaxMotorSpeed *
								 8 * m_bCM * mclkdiv/MCLKDIV_SCALING));

			stepsize = (int)ceil(x);

			DBG( _DBG_INFO2, "step size for mclk calc=%d\n", stepsize );

#if 0
			Hardware.LimitStepSize(stepsize,regs);
	    	Hardware.NSCStatusOut("5. step size for mclk calc=%d\n",stepsize);
			int tr = Hardware.ComputeTR(regs); // recompute for this step size
	    	pParam->PhyDpi.y = stepsize * 4 * Hardware.merlininfo.Motor_Steps_Per_Inch/tr;
	    	ScanParms.yres = pParam->PhyDpi.y;

		    Hardware.NSCStatusOut("5. vres=%d\n",pParam->PhyDpi.y);
#endif
		}
    	/* else limited by motor speed and integration time */
    	{
    		int    org_mclkdiv = mclkdiv;
    		int    timeadj     = 0;
    		int    org_timeadj = timeadj;
    		int    mult        = timeadj;
    		double x;

    		if( !mult )
                mult++;

    		while ((mclkdiv*dpi*mult < 6.*MCLKDIV_SCALING) ||
    			   (mclkdiv > max_mclkdiv) ||
    			   (mclkdiv*(timeadj+1) < org_mclkdiv*(org_timeadj+1)))
    		{
    			if (!timeadj && (mclkdiv < max_mclkdiv))
    				mclkdiv++;
    			else
    			{
    				mclkdiv = min_mclkdiv;
    				timeadj = ++mult;
    			}
    		}
    		/*Hardware.merlininfo.timeadj = a_bRegs[0x19] = timeadj; */
			DBG( _DBG_INFO2, "time adj=%d\n", timeadj);

    		x = stepsize;
    		if( timeadj )
    			x = x * (timeadj+1)/(double)timeadj;
    		stepsize = (int) ceil(x);
    	}

		mclkdiv = usb_max( mclkdiv,min_mclkdiv );
		DBG( _DBG_INFO, "final mclkdiv=%f\n", (double)mclkdiv/MCLKDIV_SCALING);

#if 0
		Hardware.LimitStepSize(stepsize,regs);
		Hardware.NSCStatusOut("5. step size for mclk calc=%d",stepsize);

		if( Hardware.merlininfo.timeadj > bMaxITA ) {
			if (Hardware.merlininfo.Color_Mode == 0) {
				Hardware.merlininfo.Color_Mode = 1;
				ScanParms.linerate = 1;
				regs[0x26] |= 1;
				Hardware.merlininfo.timeadj = regs[0x19] = 0;		
				return -1; //force calculation
			}
		}
		m_dMCLKDivider = mclkdiv/MCLKDIV_SCALING;
		
		/* set the registers !!!! */
#endif
}
#endif
	return m_dMCLKDivider;
}

/*.............................................................................
 *
 */
static void usb_GetStepSize( pPlustek_Device dev, pScanParam pParam )
{
	pHWDef hw = &dev->usbDev.HwSetting;

	/* Compute step size using equation 1 */
	if (m_bIntTimeAdjust != 0)
		m_wStepSize = (u_short)(((u_long) pParam->PhyDpi.y * m_wLineLength * m_bLineRateColor *
									 (m_bIntTimeAdjust + 1)) /
									 (4 * hw->wMotorDpi * m_bIntTimeAdjust));
	else
		m_wStepSize = (u_short)(((u_long) pParam->PhyDpi.y * m_wLineLength * m_bLineRateColor) /
									 (4 * hw->wMotorDpi));
	if (m_wStepSize < 2)
		m_wStepSize = 2;

	m_wStepSize = m_wStepSize * 298 / 297;

	a_bRegs[0x46] = _HIBYTE( m_wStepSize );
	a_bRegs[0x47] = _LOBYTE( m_wStepSize );

	DBG( _DBG_INFO2, "Stepsize = %u, 0x46=0x%02x 0x47=0x%02x\n",
					  m_wStepSize, a_bRegs[0x46], a_bRegs[0x47] );
}

/*.............................................................................
 *
 */
static void usb_GetLineLength( pPlustek_Device dev )
{
/* [note]
 *	The ITA in this moment is always 0, it will be changed later when we
 *  calculate MCLK. This is very strange why this routine will not call
 *  again to get all new value after ITA was changed? If this routine
 *  never call again, maybe we remove all factor with ITA here.
 */
	int tr;
	int tpspd;	/* turbo/preview mode speed reg 0a b2..3                 */
	int tpsel;	/* turbo/preview mode select reg 0a b0..1                */
	int gbnd;	/* guardband duration reg 0e b4..7                       */
	int dur;	/* pulse duration reg 0e b0..3                           */
	int ntr;	/* number of tr pulses reg 0d b7                         */
	int afeop;	/* scan mode, 0=pixel rate, 1=line rate,                 */
				/* 4=1 channel mode a, 5=1 channel mode b, reg 26 b0..2  */
	int ctmode;	/* CIS tr timing mode reg 19 b0..1                       */
	int tp;		/* tpspd or 1 if tpsel=0                                 */
	int b;		/* if ctmode=0, (ntr+1)*((2*gbnd)+dur+1), otherwise 1    */
	int tradj;	/* ITA                                                   */
	int en_tradj;

	pHWDef hw = &dev->usbDev.HwSetting;

	tpspd = (a_bRegs[0x0a] & 0x0c) >> 2; /* turbo/preview mode speed  */
	tpsel = a_bRegs[0x0a] & 3; 			 /* turbo/preview mode select */

	gbnd = (a_bRegs[0x0e] & 0xf0) >> 4;	 /* TR fi1 guardband duration */
	dur = (a_bRegs[0x0e] & 0xf);		 /* TR pulse duration         */

	ntr = a_bRegs[0x0d] / 128;			 /* number of tr pulses       */

	afeop = a_bRegs[0x26] & 7;			 /* afe op - 3 channel or 1 channel */

	tradj = a_bRegs[0x19] & 0x7f;		 /* integration time adjust */
	en_tradj = (tradj) ? 1 : 0;

	ctmode = (a_bRegs[0x0b] >> 3) & 3;	 /* cis tr timing mode */

	m_bLineRateColor = 1;	
	if (afeop == 1 || afeop == 5) /* if 3 channel line or 1 channel mode b */
		m_bLineRateColor = 3;	

	/* according to turbo/preview mode to set value */
	if( tpsel == 0 ) {
		tp = 1;
	} else {
		tp = tpspd + 2;
		if( tp == 5 )
			tp++;
	}

	b = 1;
	if( ctmode == 0 ) { /* CCD mode scanner*/
	
		b  = (ntr + 1) * ((2 * gbnd) + dur + 1);
		b += (1 - ntr) * en_tradj;
	}
	if( ctmode == 2 )   /* CIS mode scanner */
	    b = 3;
	
	tr = m_bLineRateColor * (hw->wLineEnd + tp * (b + 3 - ntr));

	if( tradj == 0 ) {
		if( ctmode == 0 )
			tr += m_bLineRateColor;
	} else {
	
		int le_phi, num_byteclk, num_mclkf, tr_fast_pix, extra_pix;
			
		/* Line color or gray mode */
		if( afeop != 0 ) {
		
			le_phi      = (tradj + 1) / 2 + 1 + 6;
			num_byteclk = ((le_phi + 8 * hw->wLineEnd + 8 * b + 4) /
						   (8 * tradj)) + 1;
			num_mclkf   = 8 * tradj * num_byteclk;
			tr_fast_pix = num_byteclk;
			extra_pix   = (num_mclkf - le_phi) % 8;
		}
		else /* 3 channel pixel rate color */
		{
			le_phi      = (tradj + 1) / 2 + 1 + 10 + 12;
			num_byteclk = ((le_phi + 3 * 8 * hw->wLineEnd + 3 * 8 * b + 3 * 4) /
						   (3 * 8 * tradj)) + 1;
			num_mclkf   = 3 * 8 * tradj * num_byteclk;
			tr_fast_pix = num_byteclk;
			extra_pix   = (num_mclkf - le_phi) % (3 * 8);
		}
		
		tr = b + hw->wLineEnd + 4 + tr_fast_pix;
		if (extra_pix == 0)
			tr++;
		tr *= m_bLineRateColor;
	}
	m_wLineLength = tr / m_bLineRateColor;

	DBG( _DBG_INFO2, "LineLength=%d, LineRateColor=%u\n",
						m_wLineLength, m_bLineRateColor );
}

/** usb_GetMotorParam
 * registers 0x56, 0x57
 */
static void usb_GetMotorParam( pPlustek_Device dev, pScanParam pParam )
{
	int       	 idx, i;
	pClkMotorDef clk;
	pMDef     	 md;
	pDCapsDef 	 sCaps = &dev->usbDev.Caps;
	pHWDef    	 hw    = &dev->usbDev.HwSetting;

	if( !_IS_PLUSTEKMOTOR(hw->motorModel)) {
	
		clk = usb_GetMotorSet( hw->motorModel );
		md  = clk->motor_sets;
 		idx = 0;
		for( i = 0; i < _MAX_CLK; i++ ) {
			if( pParam->PhyDpi.x <= dpi_ranges[i] )
  				break;
			idx++;
		}
		if( idx >= _MAX_CLK )
			idx = _MAX_CLK - 1;

		a_bRegs[0x56] = md[idx].pwm;
		a_bRegs[0x57] = md[idx].pwm_duty;

    } else {
        if( sCaps->OpticDpi.x == 1200 ) {

        	switch( hw->motorModel ) {

        	case MODEL_HuaLien:
        	case MODEL_KaoHsiung:
        	default:
        		if(pParam->PhyDpi.x <= 200)
        		{
        			a_bRegs[0x56] = 1;
        			a_bRegs[0x57] = 48;	/* 63; */
        		}
        		else if(pParam->PhyDpi.x <= 300)
        		{
        			a_bRegs[0x56] = 2;	/* 8;  */
        			a_bRegs[0x57] = 48;	/* 56; */
        		}
        		else if(pParam->PhyDpi.x <= 400)
        		{
        			a_bRegs[0x56] = 8;	
        			a_bRegs[0x57] = 48;	
        		}
        		else if(pParam->PhyDpi.x <= 600)
        		{
        			a_bRegs[0x56] = 2;	/* 10; */
        			a_bRegs[0x57] = 48;	/* 56; */
        		}
        		else /* pParam->PhyDpi.x == 1200) */
        		{
        			a_bRegs[0x56] = 1;	/* 8;  */
        			a_bRegs[0x57] = 48;	/* 56; */
        		}
        		break;
        	}
        } else {
        	switch ( hw->motorModel ) {

        	case MODEL_Tokyo600:
        		a_bRegs[0x56] = 16;
        		a_bRegs[0x57] = 4;	/* 2; */
        		break;
        	case MODEL_HuaLien:
        		{
        			if(pParam->PhyDpi.x <= 200)
        			{
        				a_bRegs[0x56] = 64;	/* 24; */
        				a_bRegs[0x57] = 4;	/* 16; */
        			}
        			else if(pParam->PhyDpi.x <= 300)
        			{
        				a_bRegs[0x56] = 64;	/* 16; */
        				a_bRegs[0x57] = 4;	/* 16; */
        			}
        			else if(pParam->PhyDpi.x <= 400)
        			{
        				a_bRegs[0x56] = 64;	/* 16; */
        				a_bRegs[0x57] = 4;	/* 16; */
        			}
        			else /* if(pParam->PhyDpi.x <= 600) */
        			{
     /* HEINER: check ADF stuff... */
     #if 0
        				if(ScanInf.m_fADF)
        				{
        					a_bRegs[0x56] = 8;
        					a_bRegs[0x57] = 48;
        				}
        				else
     #endif
        				{
        					a_bRegs[0x56] = 64;	/* 2;  */
        					a_bRegs[0x57] = 4;	/* 48; */
        				}
        			}
        		}
        		break;
        	case MODEL_KaoHsiung:
        	default:
        		if(pParam->PhyDpi.x <= 200)
        		{
        			a_bRegs[0x56] = 24;
        			a_bRegs[0x57] = 16;
        		}
        		else if(pParam->PhyDpi.x <= 300)
        		{
        			a_bRegs[0x56] = 16;
        			a_bRegs[0x57] = 16;
        		}
        		else if(pParam->PhyDpi.x <= 400)
        		{
        			a_bRegs[0x56] = 16;
        			a_bRegs[0x57] = 16;
        		}
        		else /* if(pParam->PhyDpi.x <= 600) */
        		{
        			a_bRegs[0x56] = 2;
        			a_bRegs[0x57] = 48;
        		}
        		break;
        	}
        }
	}
	
	DBG( _DBG_INFO, "MOTOR-Settings: PWM=0x%02x, PWM_DUTY=0x%02x\n",
	  											a_bRegs[0x56], a_bRegs[0x57] );
}

/*.............................................................................
 *
 */
static void usb_GetPauseLimit( pPlustek_Device dev, pScanParam pParam )
{
	int    coeffsize, scaler;
	pHWDef hw = &dev->usbDev.HwSetting;

	scaler = 1;
	if( hw->bReg_0x26 & _ONE_CH_COLOR ) {
   		if( pParam->bDataType == SCANDATATYPE_Color ) {
			scaler = 3;
		}
	}

	/* compute size of coefficient ram */
	coeffsize = 4 + 16 + 16;	/* gamma and shading and offset */

	/* if 16 bit, then not all is used */
	if( a_bRegs[0x09] & 0x20 ) {
		coeffsize = 16 + 16;	/* no gamma */
	}
	coeffsize *= (2*3); /* 3 colors and 2 bytes/word */


	/* Get available buffer size in KB
	 * for 512kb this will be 296
	 * for 2Mb   this will be 1832
	 */
	m_dwPauseLimit = (u_long)(hw->wDRAMSize - (u_long)(coeffsize));
	m_dwPauseLimit -= ((pParam->Size.dwPhyBytes*scaler) / 1024 + 1);

	/* If not reversing, take into account the steps to reverse */
	if( a_bRegs[0x50] == 0 )
		m_dwPauseLimit -= ((a_bRegs[0x54] & 7) *
							(pParam->Size.dwPhyBytes * scaler) + 1023) / 1024;

	DBG( _DBG_INFO, "PL=%lu, coeffsize=%u, scaler=%u\n",
										m_dwPauseLimit, coeffsize, scaler );

	m_dwPauseLimit = usb_max( usb_min(m_dwPauseLimit,
						(u_long)ceil(pParam->Size.dwTotalBytes / 1024.0)), 2);

	a_bRegs[0x4e] = (u_char)floor((m_dwPauseLimit*512.0) / (2*hw->wDRAMSize));

	if( a_bRegs[0x4e] > 1 )	{
		a_bRegs[0x4e]--;
		if(a_bRegs[0x4e] > 1)
			a_bRegs[0x4e]--;
	} else
		a_bRegs[0x4e] = 1;

	/*
	 * resume, when buffer is 2/8 kbytes full (512k/2M memory)
	 */
	a_bRegs[0x4f] = 1;

	DBG( _DBG_INFO, "PauseLimit = %lu, [0x4e] = 0x%02x, [0x4f] = 0x%02x\n",
					m_dwPauseLimit, a_bRegs[0x4e], a_bRegs[0x4f] );
}

/** usb_GetScanLinesAndSize
 *
 */
static void usb_GetScanLinesAndSize( pPlustek_Device dev, pScanParam pParam )
{
	pDCapsDef sCaps = &dev->usbDev.Caps;
	pHWDef    hw    = &dev->usbDev.HwSetting;

	pParam->Size.dwPhyLines = (u_long)ceil((double) pParam->Size.dwLines *
										pParam->PhyDpi.y / pParam->UserDpi.y);

	/* Calculate color offset */
	if (pParam->bCalibration == PARAM_Scan && pParam->bChannels == 3) {

		dev->scanning.bLineDistance = sCaps->bSensorDistance *
								pParam->PhyDpi.y / sCaps->OpticDpi.x;
		pParam->Size.dwPhyLines += (dev->scanning.bLineDistance << 1);
	}
	else
		dev->scanning.bLineDistance = 0;

	pParam->Size.dwTotalBytes = pParam->Size.dwPhyBytes * pParam->Size.dwPhyLines;

	if( hw->bReg_0x26 & _ONE_CH_COLOR ) {

		if( pParam->bDataType == SCANDATATYPE_Color ) {
		
			pParam->Size.dwTotalBytes *= 3;
		}
	}
}

/** function to preset/reset the merlin registers
 *
 */
static SANE_Bool usb_SetScanParameters( pPlustek_Device dev, pScanParam pParam )
{
	static u_char reg8, reg38[6], reg48[2];

	pScanParam pdParam = &dev->scanning.sParam;
	pHWDef     hw      = &dev->usbDev.HwSetting;

	m_pParam = pParam;

	DBG( _DBG_INFO, "usb_SetScanParameters()\n" );

	if( !usb_IsScannerReady(dev))
		return SANE_FALSE;

	if(pParam->bCalibration == PARAM_Scan && pParam->bSource == SOURCE_ADF) {
/* HEINER: dSaveMoveSpeed is only used in func EjectPaper!!!
		dSaveMoveSpeed = hw->dMaxMoveSpeed;
*/
		hw->dMaxMoveSpeed = 1.0;
		usb_MotorSelect( dev, SANE_TRUE );
		usb_MotorOn( dev->fd, SANE_TRUE );
	}

	/*
     * calculate the basic settings...
     */
	pParam->PhyDpi.x = usb_SetAsicDpiX( dev, pParam->UserDpi.x );
	pParam->PhyDpi.y = usb_SetAsicDpiY( dev, pParam->UserDpi.y );
	usb_SetColorAndBits( dev, pParam );
	usb_GetScanRect    ( dev, pParam );

	if( dev->caps.dwFlag & SFLAG_ADF ) {

		if( pParam->bCalibration == PARAM_Scan ) {

			if( pdParam->bSource == SOURCE_ADF ) {
				a_bRegs[0x50] = 0;
				a_bRegs[0x51] = 0x40;
				if( pParam->PhyDpi.x <= 300)
					a_bRegs[0x54] = (a_bRegs[0x54] & ~7) | 4;	/* 3; */
				else
					a_bRegs[0x54] = (a_bRegs[0x54] & ~7) | 5;	/* 4; */
			} else {
				a_bRegs[0x50] = hw->bStepsToReverse;
				a_bRegs[0x51] = hw->bReg_0x51;
				a_bRegs[0x54] &= ~7;
			}
		} else
			a_bRegs[0x50] = 0;
	} else {
		if( pParam->bCalibration == PARAM_Scan )
			a_bRegs[0x50] = hw->bStepsToReverse;
		else
			a_bRegs[0x50] = 0;
	}

	/* Assume we will not use ITA */
	a_bRegs[0x19] = m_bIntTimeAdjust = 0;

	/* Initiate variables */

	/* Get variables from calculation algorithms */
	if(!(pParam->bCalibration == PARAM_Scan &&
         pParam->bSource == SOURCE_ADF && fLastScanIsAdf )) {

		DBG( _DBG_INFO2, "Scan calculations...\n" );
		usb_GetLineLength ( dev );
		usb_GetStepSize   ( dev, pParam );
		usb_GetDPD        ( dev );
		usb_GetMCLKDivider( dev, pParam );
		usb_GetMotorParam ( dev, pParam );
	}

	/* Compute fast feed step size, use equation 3 and equation 8 */
	if( m_dMCLKDivider < 1.0)
		m_dMCLKDivider = 1.0;

	m_wFastFeedStepSize = (u_short)(dwCrystalFrequency /
							(m_dMCLKDivider * 8 * m_bCM * hw->dMaxMoveSpeed *
							 4 * hw->wMotorDpi));
	if( m_bIntTimeAdjust != 0 )
		m_wFastFeedStepSize /= m_bIntTimeAdjust;

	if(a_bRegs[0x0a])
		m_wFastFeedStepSize *= ((a_bRegs[0x0a] >> 2) + 2);
	a_bRegs[0x48] = _HIBYTE( m_wFastFeedStepSize );
	a_bRegs[0x49] = _LOBYTE( m_wFastFeedStepSize );

	/* Compute the number of lines to scan using actual Y resolution */
	usb_GetScanLinesAndSize( dev, pParam );
	
	/* Pause limit should be bounded by total bytes to read
     * so that the chassis will not move too far.
     */
	usb_GetPauseLimit( dev, pParam );

	/* For ADF .... */
	if(pParam->bCalibration == PARAM_Scan && pParam->bSource == SOURCE_ADF)	{

		if( fLastScanIsAdf ) {

			a_bRegs[0x08] = reg8;
			memcpy( &a_bRegs[0x38], reg38, sizeof(reg38));
			memcpy( &a_bRegs[0x48], reg48, sizeof(reg48));

		} else {

			reg8 = a_bRegs[0x08];
			memcpy( reg38, &a_bRegs[0x38], sizeof(reg38));
			memcpy( reg48, &a_bRegs[0x48], sizeof(reg48));
		}
		usb_MotorSelect( dev, SANE_TRUE );
	}

	/* Reset LM983x's state machine before setting register values */
	if( !usbio_WriteReg( dev->fd, 0x18, 0x18 ))
		return SANE_FALSE;

	usleep(200 * 1000);	/* Need to delay at least xxx microseconds */

	if( !usbio_WriteReg( dev->fd, 0x07, 0x20 ))
		return SANE_FALSE;

	if( !usbio_WriteReg( dev->fd, 0x19, 6 ))
		return SANE_FALSE;

	a_bRegs[0x07] = 0;

	/* Set register values */
	memset( &a_bRegs[0x03], 0, 3 );
	memset( &a_bRegs[0x5C], 0, 0x7F-0x5C+1 );

	/* 0x08 - 0x5E */
	_UIO(sanei_lm983x_write( dev->fd, 0x08, &a_bRegs[0x08], 0x5a - 0x08+1, SANE_TRUE));

	/* 0x03 - 0x05 */
	_UIO(sanei_lm983x_write( dev->fd, 0x03, &a_bRegs[0x03], 3, SANE_TRUE));

	/* special setting for CANON... */
	if( dev->usbDev.vendor == 0x4a9 )
		a_bRegs[0x70] = 0x73;

	/* 0x5C - 0x7F */
	_UIO(sanei_lm983x_write( dev->fd, 0x5c, &a_bRegs[0x5c], 0x7f - 0x5c +1, SANE_TRUE));

	usbio_WriteReg( dev->fd, 0x5a, a_bRegs[0x5a] );
	usbio_WriteReg( dev->fd, 0x5b, a_bRegs[0x5b] );

	if( !usbio_WriteReg( dev->fd, 0x07, 0 ))
		return SANE_FALSE;

	/* special procedure for CANON... */
	if( dev->usbDev.vendor == 0x4a9 ) {

		SANE_Byte tmp;

		usbio_WriteReg( dev->fd, 0x5b, 0x11 );
		usbio_WriteReg( dev->fd, 0x5b, 0x91 );
		usbio_ReadReg ( dev->fd, 0x5a, &tmp );
		usbio_WriteReg( dev->fd, 0x5a, 0x14 );

		usbio_WriteReg( dev->fd, 0x59, a_bRegs[0x59] );
		usbio_WriteReg( dev->fd, 0x5a, a_bRegs[0x5a] );
	}

	DBG( _DBG_INFO, "usb_SetScanParameters() done.\n" );

	return SANE_TRUE;
}

/*.............................................................................
 *
 */
static SANE_Bool usb_ScanBegin( pPlustek_Device dev, SANE_Bool fAutoPark )
{
	u_char value;	

	pHWDef hw = &dev->usbDev.HwSetting;

	DBG( _DBG_INFO, "usb_ScanBegin()\n" );

	/* save the request for usb_ScanEnd () */
	m_fAutoPark = fAutoPark;

	/* Disable home sensor during scan, or the chassis cannot move */
	value = ((m_pParam->bCalibration == PARAM_Scan &&
			  m_pParam->bSource == SOURCE_ADF)? (a_bRegs[0x58] & ~7): 0);

	if(!usbio_WriteReg( dev->fd, 0x58, value ))
		return SANE_FALSE;

	/* Check if scanner is ready for receiving command */
	if( !usb_IsScannerReady(dev))
		return SANE_FALSE;

	/* Flush cache - only LM9831 (Source: National Semiconductors */
	if( _LM9831 == hw->chip ) {

        for(;;) {

        	if( SANE_TRUE == cancelRead ) {
        		DBG( _DBG_INFO, "ScanBegin() - Cancel detected...\n" );
        		return SANE_FALSE;
        	}
      	
        	_UIO(usbio_ReadReg( dev->fd, 0x01, &m_bOldScanData ));

        	if( m_bOldScanData ) {

        		u_long dwBytesToRead = m_bOldScanData * hw->wDRAMSize * 4;
        		u_char *pBuffer      = malloc( sizeof(u_char) * dwBytesToRead );

        		DBG( _DBG_INFO, "Flushing cache - %lu bytes (bOldScanData=%u)\n",
        						dwBytesToRead, m_bOldScanData );

        		_UIO(sanei_lm983x_read( dev->fd, 0x00, pBuffer, dwBytesToRead, SANE_FALSE ));

        		free( pBuffer );

        	} else
        		break;
        }
	}

	/* Download map & Shading data */
	if(( m_pParam->bCalibration == PARAM_Scan &&
		!usb_MapDownload( dev, m_pParam->bDataType)) ||
		!usb_DownloadShadingData( dev, m_pParam->bCalibration ))
		return SANE_FALSE;

	/* Move chassis and start to read image data */
	if (!usbio_WriteReg( dev->fd, 0x07, 3 ))
		return SANE_FALSE;

	usbio_ReadReg( dev->fd, 0x01, &m_bOldScanData );

	m_bOldScanData = 0;						/* No data at all  */
	m_fStart = m_fFirst = SANE_TRUE;		/* Prepare to read */

	DBG( _DBG_INFO2, "Register Dump before reading data:\n" );
	dumpregs( dev->fd, NULL );

	return SANE_TRUE;
}

/** usb_ScanEnd
 * stop all the processing stuff and reposition sensor back home
 */
static SANE_Bool usb_ScanEnd( pPlustek_Device dev )
{
	u_char value;

	DBG( _DBG_INFO, "usbDev_ScanEnd(), start=%u, park=%u\n",
										m_fStart, m_fAutoPark );

	usbio_ReadReg( dev->fd, 0x07, &value );
	if( value == 3 || value != 2 )
		usbio_WriteReg( dev->fd, 0x07, 0 );

	if( m_fStart ) {
		m_fStart = SANE_FALSE;

		if( m_fAutoPark ) {
		
			usb_ModuleToHome( dev, SANE_FALSE );
		}			
	}
	else if( SANE_TRUE == cancelRead ) {
	
		usb_ModuleToHome( dev, SANE_FALSE );
	}

	return SANE_TRUE;
}

/*.............................................................................
 *
 */
static SANE_Bool usb_IsDataAvailableInDRAM( pPlustek_Device dev )
{
	/* Compute polling timeout
	 *	Height (Inches) / MaxScanSpeed (Inches/Second) = Seconds to move the
     *  module from top to bottom. Then convert the seconds to miliseconds
     *  by multiply 1000. We add extra 2 seconds to get some tolerance.
     */
	u_char         a_bBand[3];
	long           dwTicks;
    struct timeval t;

	DBG( _DBG_INFO, "usb_IsDataAvailableInDRAM()\n" );

	gettimeofday( &t, NULL);	
	dwTicks = t.tv_sec + 30;

	for(;;)	{

		_UIO( sanei_lm983x_read( dev->fd, 0x01, a_bBand, 3, SANE_FALSE ));

		gettimeofday( &t, NULL);	
	    if( t.tv_sec > dwTicks )
			break;

		if( usb_IsEscPressed()) {
			DBG(_DBG_INFO,"usb_IsDataAvailableInDRAM() - Cancel detected...\n");
			return SANE_FALSE;
		}
			
		/* It is not stable for read */
		if((a_bBand[0] != a_bBand[1]) && (a_bBand[1] != a_bBand[2]))
			continue;

		if( a_bBand[0] > m_bOldScanData ) {

			if( m_pParam->bSource != SOURCE_Reflection )

				usleep(1000*(30 * a_bRegs[0x08] * dev->usbDev.Caps.OpticDpi.x / 600));
			else
				usleep(1000*(20 * a_bRegs[0x08] * dev->usbDev.Caps.OpticDpi.x / 600));

			DBG( _DBG_INFO, "Data is available\n" );
			return SANE_TRUE;
		}
	}

	DBG( _DBG_INFO, "NO Data available\n" );
	return SANE_FALSE;
}

/*.............................................................................
 *
 */
static SANE_Bool usb_ScanReadImage( pPlustek_Device dev,
									void *pBuf, u_long dwSize )
{
	static u_long dwBytes = 0;
	SANE_Status res;

	DBG( _DBG_READ, "usb_ScanReadImage(%lu)\n", dwSize );

	if( m_fFirst ) {

		dwBytes  = 0;
		m_fFirst = SANE_FALSE;

		/* Wait for data band ready */
		if (!usb_IsDataAvailableInDRAM( dev )) {
			DBG( _DBG_ERROR, "Nothing to read...\n" );
			return SANE_FALSE;
		}			
	}
/* HEINER: ADF */
#if 0
	if(ScanInf.m_fADF == 1 && Scanning.sParam.bCalibration == PARAM_Scan)
	{
		if(dwBytes)
		{
			DWORD dw;
			BOOL fRet;

			if(dwSize < dwBytes)
				dw = dwSize;
			else
				dw = dwBytes;

			fRet = ReadData(0x00, (PBYTE)pBuf, dw);

			dwBytes -= dw;

			if(!dwBytes)
			{
				WriteRegister(0x07, 0);	// To stop scanning
				ScanInf.m_fADF++;
				if(dwSize > dw)
					ScanReadImage((PBYTE)pBuf + dw, dwSize - dw);
			}
			return fRet;
		}
		else if(!Hardware.SensorPaper())
		{
			dwBytes = (Scanning.sParam.PhyDpi.y * 18 / 25) * Scanning.sParam.Size.dwPhyBytes;
			return ScanReadImage(pBuf, dwSize);
		}
	}
	else if(ScanInf.m_fADF > 1)
	{
		DWORD dw;
		if(Scanning.sParam.bBitDepth > 8)
		{
			for(dw = 0; dw < dwSize; dw += 2)
				*((PWORD)pBuf + dw) = 0xfffc;
		}
		else
			FillMemory(pBuf, dwSize, 0xff);
		return TRUE;
	}
#endif

	res = sanei_lm983x_read(dev->fd, 0x00, (u_char *)pBuf, dwSize, SANE_FALSE);

	DBG( _DBG_READ, "usb_ScanReadImage() done, result: %d\n", res );

	if( SANE_STATUS_GOOD == res ) {
		return SANE_TRUE;
	}

	DBG( _DBG_ERROR, "usb_ScanReadImage() failed\n" );
	
	return SANE_FALSE;
}

/*.............................................................................
 *
 */
static void usb_GetImageInfo( pImgDef pInfo, pWinInfo pSize )
{
	DBG( _DBG_INFO, "usb_GetImageInfo()\n" );

	pSize->dwPixels = (u_long)pInfo->crArea.cx * pInfo->xyDpi.x / 300UL;
	pSize->dwLines  = (u_long)pInfo->crArea.cy * pInfo->xyDpi.y / 300UL;

	switch( pInfo->wDataType ) {

		case COLOR_TRUE48:
			pSize->dwBytes = pSize->dwPixels * 6UL;
			break;
			
		case COLOR_TRUE24:
			pSize->dwBytes = pSize->dwPixels * 3UL;
			break;

		case COLOR_GRAY16:
			pSize->dwBytes = pSize->dwPixels << 1;
			break;

		case COLOR_256GRAY:
			pSize->dwBytes = pSize->dwPixels;
			break;

		default:
			pSize->dwBytes = (pSize->dwPixels + 7UL) >> 3;
			break;
	}
}

/*.............................................................................
 *
 */
static void usb_SaveImageInfo( pPlustek_Device dev, pImgDef pInfo )
{
	pHWDef     hw     = &dev->usbDev.HwSetting;
	pScanParam pParam = &dev->scanning.sParam;

	DBG( _DBG_INFO, "usb_SaveImageInfo()\n" );

	/* Dpi & Origins */
	pParam->UserDpi  = pInfo->xyDpi;
	pParam->Origin.x = pInfo->crArea.x;
	pParam->Origin.y = pInfo->crArea.y;

	/* Source & Bits */
	pParam->bBitDepth = 8;

	switch( pInfo->wDataType ) {

		case COLOR_TRUE48:
			pParam->bBitDepth = 16;
			/* fall through... */
			
		case COLOR_TRUE24:
			pParam->bDataType = SCANDATATYPE_Color;

			/* AFE operation: one or 3 channels ! */
			if( hw->bReg_0x26 & _ONE_CH_COLOR )
				pParam->bChannels = 1;
			else
				pParam->bChannels = 3;
			break;

		case COLOR_GRAY16:
			pParam->bBitDepth = 16;

		case COLOR_256GRAY:
			pParam->bDataType = SCANDATATYPE_Gray;
			pParam->bChannels = 1;
			break;

		default:
			pParam->bBitDepth = 1;
			pParam->bDataType = SCANDATATYPE_BW;
			pParam->bChannels = 1;
	}

	DBG( _DBG_INFO, "dwFlag = 0x%08lx\n", pInfo->dwFlag );

	if( pInfo->dwFlag & SCANDEF_Transparency )
		pParam->bSource = SOURCE_Transparency;
	else if( pInfo->dwFlag & SCANDEF_Negative )
		pParam->bSource = SOURCE_Negative;
	else if( pInfo->dwFlag & SCANDEF_Adf )
		pParam->bSource = SOURCE_ADF;
	else
		pParam->bSource = SOURCE_Reflection;
}

/* END PLUSTEK-USBSCAN.C ....................................................*/
