/*.............................................................................
 * Project : SANE library for Plustek flatbed scanners; canoscan calibration
 *.............................................................................
 */

/** @file plustek-usbcal.c
 *  @brief Calibration routines for CanoScan CIS devices.
 *
 * Based on sources acquired from Plustek Inc.<br>
 * Copyright (C) 2001-2003 Gerhard Jaeger <gerhard@gjaeger.de><br>
 * Large parts Copyright (C) 2003 Monty <monty@xiph.org>
 *
 * Montys' comment:
 * The basic premise: The stock Plustek-usbshading.c in the plustek
 * driver is effectively nonfunctional for Canon CanoScan scanners.
 * These scanners rely heavily on all calibration steps, especially
 * fine white, to produce acceptible scan results.  However, to make
 * autocalibration work and make it work well involves some
 * substantial mucking aobut in code that supports thirty other
 * scanners with widely varying characteristics... none of which I own
 * or can test.
 *
 * Therefore, I'm splitting out a few calibration functions I need
 * to modify for the CanoScan which allows me to simplify things 
 * greatly for the CanoScan without worrying about breaking other
 * scanners, as well as reuse the vast majority of the Plustek
 * driver infrastructure without forking.
 *
 * History:
 * - 0.45m - birth of the file; tested extensively with the LiDE 20
 * - 0.46  - renamed to plustek-usbcal.c
 *         - fixed problems with LiDE30, works now with 650, 1220, 670, 1240
 *         - cleanup
 *         - added CCD calibration capability
 *         - added the usage of the swGain and swOffset values, to allow
 *           tweaking the calibration results on a sensor base
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
 * <hr>
 */

/** 0 for not ready,  1 pos white lamp on,  2 lamp off */ 
static int strip_state=0; 
  
/** the NatSemi 983x is a big endian chip, and the line protocol data all
 *  arrives big-endian.  This determines if we need to swap to host-order
 */
static SANE_Bool cano_HostSwap_p( void )
{
	u_short        pattern  = 0xfeed; /* deadbeef */
	unsigned char *bytewise = (unsigned char *)&pattern;
	
	if ( bytewise[0] == 0xfe ) {
		DBG( _DBG_READ, "We're big-endian!  No need to swap!\n" );
		return 0;
	}
	DBG( _DBG_READ, "We're little-endian!  NatSemi LM9833 is big!"
	                "Must swap calibration data!\n" );
	return 1;
}

/** depending on the strip state, the sensor is moved to the shading position
 *  and the lamp ist switched on
 */
static int cano_PrepareToReadWhiteCal( pPlustek_Device dev )
{
	pHWDef hw = &dev->usbDev.HwSetting;
	
	switch (strip_state) {
		case 0:
			if(!usb_ModuleToHome( dev, SANE_TRUE )) {
				DBG( _DBG_ERROR, "cano_PrepareToReadWhiteCal() failed\n" );
				return _E_LAMP_NOT_IN_POS;
			}
			if( !usb_ModuleMove(dev, MOVE_Forward,
				(u_long)dev->usbDev.pSource->ShadingOriginY)) {
				DBG( _DBG_ERROR, "cano_PrepareToReadWhiteCal() failed\n" );
				return _E_LAMP_NOT_IN_POS;
			}
	    	break;
		case 2:
			a_bRegs[0x29] = hw->bReg_0x29;
			usb_switchLamp( dev, SANE_TRUE );
			if( !usbio_WriteReg( dev->fd, 0x29, a_bRegs[0x29])) {
				DBG( _DBG_ERROR, "cano_PrepareToReadWhiteCal() failed\n" );
				return _E_LAMP_NOT_IN_POS;
			}
		break;
	}
  
	strip_state = 1;
	return 0;
}

/**
 */
static int cano_PrepareToReadBlackCal( pPlustek_Device dev )
{
	if( strip_state == 0 )
		if(cano_PrepareToReadWhiteCal(dev))
			return SANE_FALSE;
			
	if( strip_state != 2 ) {
	    /*
		 * if we have dark shading strip, there's no need to switch
	     * the lamp off
		 */
		if( dev->usbDev.pSource->DarkShadOrgY >= 0 ) {

			usb_ModuleToHome( dev, SANE_TRUE );
			usb_ModuleMove  ( dev, MOVE_Forward,
								(u_long)dev->usbDev.pSource->DarkShadOrgY );
			a_bRegs[0x45] &= ~0x10;
			strip_state = 0;

		} else {

		 	/* switch lamp off to read dark data... */
			a_bRegs[0x29] = 0;
			usb_switchLamp( dev, SANE_FALSE );
			strip_state = 2;
		}
	}
	return 0;
}

/**
 */
static int cano_LampOnAfterCalibration( pPlustek_Device dev )
{
	pHWDef hw = &dev->usbDev.HwSetting;

	switch (strip_state) {
		case 2:
			a_bRegs[0x29] = hw->bReg_0x29;
			usb_switchLamp( dev, SANE_TRUE );
			if( !usbio_WriteReg( dev->fd, 0x29, a_bRegs[0x29])) {
				DBG( _DBG_ERROR, "cano_LampOnAfterCalibration() failed\n" );
				return _E_LAMP_NOT_IN_POS;
			}
			strip_state = 1;
			break;
	}
	return 0;
}

/** function to adjust the...
 * returns 0 if the value is fine, 1, if we need to adjust and 2 if we ran
 * against a limit...
 */
static int cano_adjLampSetting( u_short *min,
                                u_short *max, u_short *off, u_short val )
{
	u_long newoff  = *off;

	/* perfect value, no need to adjust
	 * val ¤ [53440..61440] is perfect
	 */  
	if((val < IDEAL_GainNormal) && (val > (IDEAL_GainNormal-8000)))
		return 0;

	if(val > (IDEAL_GainNormal-4000)) {

		DBG(_DBG_INFO2, "TOO BRIGHT --> reduce\n" );
		*max   = newoff;
		*off = ((newoff + *min)>>1);
		
	} else {

		u_short bisect = (newoff + *max)>>1;
		u_short twice  =  newoff*2;

		DBG(_DBG_INFO2, "TOO DARK --> up\n" );
		*min = newoff;
		*off = twice<bisect?twice:bisect;

		if( *off > 0x3FFF ) {
			DBG( _DBG_INFO2, "lamp off limited (0x%04x --> 0x3FFF)\n", *off );
			*off = 0x3FFF;
			return 2;
		}
	}

	if((*min+1) >= *max )
		return 0;
		
	return 1;
}

/** cano_AdjustLightsource
 * coarse calibration step 0
 * [Monty changes]: On the CanoScan at least, the default lamp
 * settings are several *hundred* percent too high and vary from
 * scanner-to-scanner by 20-50%. This is only for CIS devices 
 * where the lamp_off parameter is adjustable; I'd make it more general, 
 * but I only have the CIS hardware to test.
 */

static int cano_AdjustLightsource( pPlustek_Device dev)
{
	char         tmp[40];
	int          i, adj, warmup_limit, limit;
	u_long       dw, dwR, dwG, dwB, dwDiv, dwLoop1, dwLoop2;
	RGBUShortDef max_rgb, min_rgb, tmp_rgb;
	
	pDCapsDef    scaps = &dev->usbDev.Caps;
	pHWDef       hw    = &dev->usbDev.HwSetting;

	if( usb_IsEscPressed())
		return SANE_FALSE;

	DBG( _DBG_INFO2, "cano_AdjustLightsource()\n" );

	if( !(hw->bReg_0x26 & _ONE_CH_COLOR)) {
		DBG( _DBG_INFO2, "- function skipped\n" );
		return SANE_TRUE;	
	}

	/* define the strip to scan for coarse calibration;  done at 300dpi */
	m_ScanParam.Size.dwLines  = 1;
	m_ScanParam.Size.dwPixels = scaps->Normal.Size.x *
								scaps->OpticDpi.x / 300UL;
  
	m_ScanParam.Size.dwBytes  = m_ScanParam.Size.dwPixels * 2;

	if( m_ScanParam.bDataType == SCANDATATYPE_Color )
		m_ScanParam.Size.dwBytes *=3;

	m_ScanParam.Origin.x = (u_short)((u_long) hw->wActivePixelsStart *
													300UL / scaps->OpticDpi.x);
	m_ScanParam.bCalibration = PARAM_Gain;
  
	DBG( _DBG_INFO2, "Coarse Calibration Strip:\n" );
	DBG( _DBG_INFO2, "Lines    = %lu\n", m_ScanParam.Size.dwLines  );
	DBG( _DBG_INFO2, "Pixels   = %lu\n", m_ScanParam.Size.dwPixels );
	DBG( _DBG_INFO2, "Bytes    = %lu\n", m_ScanParam.Size.dwBytes  );
	DBG( _DBG_INFO2, "Origin.X = %u\n",  m_ScanParam.Origin.x );

	max_rgb.Red   = max_rgb.Green = max_rgb.Blue = 0xffff;
	min_rgb.Red   = hw->red_lamp_on;
	min_rgb.Green = hw->green_lamp_on;
	min_rgb.Blue  = hw->blue_lamp_on;

	for( i = 0; ; i++ ) {
  
		m_ScanParam.dMCLK = dMCLK;
		if( !usb_SetScanParameters( dev, &m_ScanParam )) {
			return SANE_FALSE;
		}
    
		if(	!usb_ScanBegin( dev, SANE_FALSE) ||
			!usb_ScanReadImage( dev, pScanBuffer, m_ScanParam.Size.dwPhyBytes ) ||
			!usb_ScanEnd( dev )) {
				DBG( _DBG_ERROR, "cano_AdjustLightsource() failed\n" );
				return SANE_FALSE;
		}
    
		DBG( _DBG_INFO2, "PhyBytes  = %lu\n",  m_ScanParam.Size.dwPhyBytes  );
		DBG( _DBG_INFO2, "PhyPixels = %lu\n",  m_ScanParam.Size.dwPhyPixels );
    
		sprintf( tmp, "coarse-lamp-%u.raw", i );
    
		dumpPicInit( &m_ScanParam, tmp );
		dumpPic( tmp, pScanBuffer, m_ScanParam.Size.dwPhyBytes );
    
		if(cano_HostSwap_p())
			usb_Swap((u_short *)pScanBuffer, m_ScanParam.Size.dwPhyBytes );

		sprintf( tmp, "coarse-lamp-swap%u.raw", i );
    
		dumpPicInit( &m_ScanParam, tmp );
		dumpPic( tmp, pScanBuffer, m_ScanParam.Size.dwPhyBytes );
    
		dwDiv   = 10;
		dwLoop1 = m_ScanParam.Size.dwPhyPixels/dwDiv;
		adj     = 0;
      
		tmp_rgb.Red = tmp_rgb.Green = tmp_rgb.Blue = 0;
      
		/* find out the max pixel value for R, G, B */
		for( dw = 0; dwLoop1; dwLoop1-- ) {
	
			/* do some averaging... */
			for (dwLoop2 = dwDiv, dwR = dwG = dwB = 0; dwLoop2; dwLoop2--, dw++) {

				if( m_ScanParam.bDataType == SCANDATATYPE_Color ) {
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
				} else {
					dwG += ((u_short*)pScanBuffer)[dw];
				}
			}

			dwR = dwR / dwDiv;
			dwG = dwG / dwDiv;
			dwB = dwB / dwDiv;
	
			if( tmp_rgb.Red < dwR )
				tmp_rgb.Red = dwR;
			if( tmp_rgb.Green < dwG )
				tmp_rgb.Green = dwG;
			if( tmp_rgb.Blue < dwB )
				tmp_rgb.Blue = dwB;
		}
      
		if( m_ScanParam.bDataType == SCANDATATYPE_Color ) {
			DBG( _DBG_INFO2, "red_lamp_off  = %u/%u/%u\n",
								min_rgb.Red ,hw->red_lamp_off, max_rgb.Red );
		}
			
		DBG( _DBG_INFO2, "green_lamp_off = %u/%u/%u\n",
							min_rgb.Green, hw->green_lamp_off, max_rgb.Green );
							
		if( m_ScanParam.bDataType == SCANDATATYPE_Color ) {
			DBG( _DBG_INFO2, "blue_lamp_off = %u/%u/%u\n",
							min_rgb.Blue, hw->blue_lamp_off, max_rgb.Blue );
		}
      
		DBG(_DBG_INFO2, "CUR(R,G,B)= 0x%04x(%u), 0x%04x(%u), 0x%04x(%u)\n",
			 				tmp_rgb.Red, tmp_rgb.Red, tmp_rgb.Green,
							 tmp_rgb.Green, tmp_rgb.Blue, tmp_rgb.Blue );
      
		/* bisect */
		adj          = 0;
		warmup_limit = 2;
		limit        = 2;

		if( m_ScanParam.bDataType == SCANDATATYPE_Color ) {
			adj += cano_adjLampSetting( &min_rgb.Red, &max_rgb.Red,
											  &hw->red_lamp_off, tmp_rgb.Red );
			adj += cano_adjLampSetting( &min_rgb.Blue, &max_rgb.Blue,
											 &hw->blue_lamp_off,tmp_rgb.Blue );
			warmup_limit = 6;
			limit        = 10;
		}

		adj += cano_adjLampSetting( &min_rgb.Green, &max_rgb.Green,
										  &hw->green_lamp_off, tmp_rgb.Green );
		if( 0 == adj )
			break;

		/* it might be, that we need some warmup, if every adjustment
		 * rans agaist the limit (cano_adjLampSetting returns 2!
		 * allow at least 10 loops... */
		if( adj == warmup_limit ) {
			if( i >= limit ) {
				DBG(_DBG_INFO, "10 times limit reached, still too dark!!!\n" );
				break;
			} else {
				DBG(_DBG_INFO2, "CIS-Warmup, 1s!!!\n" );
				sleep( 1 );
			}
		} else {

			/* not all channels ran against the limit... */
			if((adj % 2) == 0 ) {
				DBG( _DBG_INFO2,
					"Still %u channel(s) too dark, but proceeding\n", adj/2 );
				break;
			}
		}
			
		usb_AdjustLamps(dev);
	}

	DBG( _DBG_INFO2, "red_lamp_on    = %u\n", hw->red_lamp_on  );
	DBG( _DBG_INFO2, "red_lamp_off   = %u\n", hw->red_lamp_off );
	DBG( _DBG_INFO2, "green_lamp_on  = %u\n", hw->green_lamp_on  );
	DBG( _DBG_INFO2, "green_lamp_off = %u\n", hw->green_lamp_off );
	DBG( _DBG_INFO2, "blue_lamp_on   = %u\n", hw->blue_lamp_on   );
	DBG( _DBG_INFO2, "blue_lamp_off  = %u\n", hw->blue_lamp_off  );
    
	DBG( _DBG_INFO2, "cano_AdjustLightsource() done.\n" );

	return SANE_TRUE;
}

/**
 */
static int cano_adjGainSetting( u_char *min, u_char *max, 
								u_char *gain,u_long val )
{
	u_long newgain = *gain;

	if((val < IDEAL_GainNormal) && (val > (IDEAL_GainNormal-8000)))
		return 0;

	if(val > (IDEAL_GainNormal-4000)) {
		*max   = newgain;
		*gain  = (newgain + *min)>>1;
	} else {
		*min   = newgain;
		*gain  = (newgain + *max)>>1;
	}

	if((*min+1) >= *max)
		return 0;
		
	return 1;
}

/** cano_AdjustGain
 * function to perform the "coarse calibration step" part 1.
 * We scan reference image pixels to determine the optimum coarse gain settings
 * for R, G, B. (Analog gain and offset prior to ADC). These coefficients are
 * applied at the line rate during normal scanning.
 * The scanned line should contain a white strip with some black at the
 * beginning. The function searches for the maximum value which corresponds to
 * the maximum white value.
 * Affects register 0x3b, 0x3c and 0x3d
 *
 * adjLightsource, above, steals most of this function's thunder.
 */
static SANE_Bool cano_AdjustGain( pPlustek_Device dev )
{
	char      tmp[40];
	int       i = 0, adj = 1;
	pDCapsDef scaps = &dev->usbDev.Caps;
	pHWDef    hw    = &dev->usbDev.HwSetting;
	u_long    dw;

	unsigned char max[3], min[3];
  
	if( usb_IsEscPressed())
		return SANE_FALSE;
  
	bMaxITA = 0xff;

	max[0]  = max[1] = max[2] = 0x3f;
	min[0]  = min[1] = min[2] = 1;

	DBG( _DBG_INFO2, "cano_AdjustGain()\n" );
  
	/*
	 * define the strip to scan for coarse calibration
	 * done at 300dpi
	 */

	m_ScanParam.Size.dwLines  = 1;				/* for gain */
	m_ScanParam.Size.dwPixels = scaps->Normal.Size.x *
				    								scaps->OpticDpi.x / 300UL;
  
	m_ScanParam.Size.dwBytes  = m_ScanParam.Size.dwPixels * 2;

	if( hw->bReg_0x26 & _ONE_CH_COLOR &&
    	m_ScanParam.bDataType == SCANDATATYPE_Color ) {
		m_ScanParam.Size.dwBytes *=3;
	}
  
	m_ScanParam.Origin.x = (u_short)((u_long) hw->wActivePixelsStart *
													300UL / scaps->OpticDpi.x);
	m_ScanParam.bCalibration = PARAM_Gain;
  
	DBG( _DBG_INFO2, "Coarse Calibration Strip:\n" );
	DBG( _DBG_INFO2, "Lines    = %lu\n", m_ScanParam.Size.dwLines  );
	DBG( _DBG_INFO2, "Pixels   = %lu\n", m_ScanParam.Size.dwPixels );
	DBG( _DBG_INFO2, "Bytes    = %lu\n", m_ScanParam.Size.dwBytes  );
	DBG( _DBG_INFO2, "Origin.X = %u\n",  m_ScanParam.Origin.x );
  
	while( adj ) {

		m_ScanParam.dMCLK = dMCLK;
    
		if( !usb_SetScanParameters( dev, &m_ScanParam ))
			return SANE_FALSE;
    
   
		if(	!usb_ScanBegin( dev, SANE_FALSE) ||
			!usb_ScanReadImage(dev,pScanBuffer,m_ScanParam.Size.dwPhyBytes) ||
			!usb_ScanEnd( dev )) {
			DBG( _DBG_ERROR, "cano_AdjustGain() failed\n" );
			return SANE_FALSE;
		}
    
		DBG( _DBG_INFO2, "PhyBytes  = %lu\n",  m_ScanParam.Size.dwPhyBytes  );
		DBG( _DBG_INFO2, "PhyPixels = %lu\n",  m_ScanParam.Size.dwPhyPixels );
    
		sprintf( tmp, "coarse-gain-%u.raw", i++ );
    
		dumpPicInit( &m_ScanParam, tmp );
		dumpPic( tmp, pScanBuffer, m_ScanParam.Size.dwPhyBytes );
    
		if(cano_HostSwap_p())
      		usb_Swap((u_short *)pScanBuffer, m_ScanParam.Size.dwPhyBytes );
    
		if( m_ScanParam.bDataType == SCANDATATYPE_Color ) {
      
			RGBUShortDef max_rgb;
			u_long       dwR, dwG, dwB;
			u_long       dwDiv = 10;
			u_long       dwLoop1 = m_ScanParam.Size.dwPhyPixels/dwDiv, dwLoop2;
      
			max_rgb.Red = max_rgb.Green = max_rgb.Blue = 0;
      
			/* find out the max pixel value for R, G, B */
			for( dw = 0; dwLoop1; dwLoop1-- ) {

				/* do some averaging... */
				for (dwLoop2 = dwDiv, dwR=dwG=dwB=0; dwLoop2; dwLoop2--, dw++) {

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
			}
      
			DBG(_DBG_INFO2, "MAX(R,G,B)= 0x%04x(%u), 0x%04x(%u), 0x%04x(%u)\n",
									max_rgb.Red, max_rgb.Red, max_rgb.Green,
									max_rgb.Green, max_rgb.Blue, max_rgb.Blue );
      
			adj  = cano_adjGainSetting(min  , max  ,a_bRegs+0x3b,max_rgb.Red  );
			adj += cano_adjGainSetting(min+1, max+1,a_bRegs+0x3c,max_rgb.Green);
			adj += cano_adjGainSetting(min+2, max+2,a_bRegs+0x3d,max_rgb.Blue );
      
	    } else {
      
			u_short w_max = 0;
      
			for( dw = 0; dw < m_ScanParam.Size.dwPhyPixels; dw++ ) {
				if( w_max < ((u_short*)pScanBuffer)[dw])
					w_max = ((u_short*)pScanBuffer)[dw];
			}
      
			adj = cano_adjGainSetting(min,max,a_bRegs+0x3c,w_max);
			a_bRegs[0x3b] = (a_bRegs[0x3d] = a_bRegs[0x3c]);
      
			DBG(_DBG_INFO2, "MAX(G)= 0x%04x(%u)\n", w_max, w_max );
      
		}
		DBG( _DBG_INFO2, "REG[0x3b] = %u\n", a_bRegs[0x3b] );
		DBG( _DBG_INFO2, "REG[0x3c] = %u\n", a_bRegs[0x3c] );
		DBG( _DBG_INFO2, "REG[0x3d] = %u\n", a_bRegs[0x3d] );
  
	}
  
	DBG( _DBG_INFO2, "cano_AdjustGain() done.\n" );

	return SANE_TRUE;
}

/**
 */
static int cano_GetNewOffset( u_long *val, int channel, signed char *low, 
							  signed char *now, signed char *high )
{
	/* if we're too black, we're likely off the low end */
	if(val[channel]<=16) {
		low[channel]=now[channel];
		now[channel]=(now[channel]+high[channel])/2;

		a_bRegs[0x38+channel]= (now[channel]&0x3f);

		if(low[channel]+1>=high[channel])
			return 0;
		return 1;
		
	} else if (val[channel]>=2048) {
		high[channel]=now[channel];
		now[channel]=(now[channel]+low[channel])/2;

		a_bRegs[0x38+channel]= (now[channel]&0x3f);

		if(low[channel]+1>=high[channel])
			return 0;
		return 1;
	}
	return 0;
}

/** cano_AdjustOffset
 * function to perform the "coarse calibration step" part 2.
 * We scan reference image pixels to determine the optimum coarse offset settings
 * for R, G, B. (Analog gain and offset prior to ADC). These coefficients are
 * applied at the line rate during normal scanning.
 * On CIS based devices, we switch the light off, on CCD devices, we use the optical
 * black pixels.
 * Affects register 0x38, 0x39 and 0x3a
 */

/* Move this to a bisection-based algo and correct some fenceposts;
   Plustek's example code disagrees with NatSemi's docs; going by the
   docs works better, I will assume the docs are correct. --Monty */

static int cano_AdjustOffset( pPlustek_Device dev )
{
	char   tmp[40];
	int    i, adj;
	u_long dw, dwPixels;
	u_long dwSum[3];
  
	signed char low[3]  = {-32,-32,-32 };
	signed char now[3]  = {  0,  0,  0 };
	signed char high[3] = { 31, 31, 31 };

	pHWDef    hw    = &dev->usbDev.HwSetting;
	pDCapsDef scaps = &dev->usbDev.Caps;
	
	if( usb_IsEscPressed())
		return SANE_FALSE;
  
	DBG( _DBG_INFO2, "cano_AdjustOffset()\n" );

	m_ScanParam.Size.dwLines  = 1;				/* for gain */
	m_ScanParam.Size.dwPixels = scaps->Normal.Size.x*scaps->OpticDpi.x/300UL;

	if( hw->bReg_0x26 & _ONE_CH_COLOR )
		dwPixels = m_ScanParam.Size.dwPixels;
	else
		dwPixels = (u_long)(hw->bOpticBlackEnd - hw->bOpticBlackStart );
  
	m_ScanParam.Size.dwBytes = m_ScanParam.Size.dwPixels * 2;

	if( hw->bReg_0x26 & _ONE_CH_COLOR &&
		m_ScanParam.bDataType == SCANDATATYPE_Color ) {
    	m_ScanParam.Size.dwBytes *= 3;
	}     
  
	m_ScanParam.Origin.x = (u_short)((u_long)hw->bOpticBlackStart * 300UL /
												dev->usbDev.Caps.OpticDpi.x);
	m_ScanParam.bCalibration = PARAM_Offset;
	m_ScanParam.dMCLK        = dMCLK;
 
	if( !usb_SetScanParameters( dev, &m_ScanParam )) {
		DBG( _DBG_ERROR, "cano_AdjustOffset() failed\n" );
		return SANE_FALSE;
	}
  
	DBG( _DBG_INFO2, "S.dwPixels  = %lu\n", m_ScanParam.Size.dwPixels );		
	DBG( _DBG_INFO2, "dwPixels    = %lu\n", dwPixels );
	DBG( _DBG_INFO2, "dwPhyBytes  = %lu\n", m_ScanParam.Size.dwPhyBytes );
	DBG( _DBG_INFO2, "dwPhyPixels = %lu\n", m_ScanParam.Size.dwPhyPixels );
  
	for( i = 0, adj = 1; adj != 0; i++ ) {
    
		if((!usb_ScanBegin(dev, SANE_FALSE)) ||
			(!usb_ScanReadImage(dev,pScanBuffer,m_ScanParam.Size.dwPhyBytes)) ||
			!usb_ScanEnd( dev )) {
			DBG( _DBG_ERROR, "cano_AdjustOffset() failed\n" );
			return SANE_FALSE;
		}
    
		sprintf( tmp, "coarse-off-%u.raw", i );
    
		dumpPicInit( &m_ScanParam, tmp );
		dumpPic( tmp, pScanBuffer, m_ScanParam.Size.dwPhyBytes );
    
		if(cano_HostSwap_p())
			usb_Swap((u_short *)pScanBuffer, m_ScanParam.Size.dwPhyBytes );
    
		if( m_ScanParam.bDataType == SCANDATATYPE_Color ) {
      
			dwSum[0] = dwSum[1] = dwSum[2] = 0;
      
			for (dw = 0; dw < dwPixels; dw++) {

				if( hw->bReg_0x26 & _ONE_CH_COLOR ) {

					dwSum[0] += ((u_short*)pScanBuffer)[dw];
					dwSum[1] += ((u_short*)
							pScanBuffer)[dw+m_ScanParam.Size.dwPhyPixels+1];
					dwSum[2] += ((u_short*)
							pScanBuffer)[dw+(m_ScanParam.Size.dwPhyPixels+1)*2];
					
				} else {
					dwSum[0] += ((pRGBUShortDef)pScanBuffer)[dw].Red;
					dwSum[1] += ((pRGBUShortDef)pScanBuffer)[dw].Green;
					dwSum[2] += ((pRGBUShortDef)pScanBuffer)[dw].Blue;
				}
			}
      
			DBG( _DBG_INFO2, "RedSum   = %lu, ave = %lu\n",
												dwSum[0], dwSum[0]/dwPixels );
			DBG( _DBG_INFO2, "GreenSum = %lu, ave = %lu\n",
												dwSum[1], dwSum[1] /dwPixels );
			DBG( _DBG_INFO2, "BlueSum  = %lu, ave = %lu\n",
												dwSum[2], dwSum[2] /dwPixels );
      
			/* do averaging for each channel */
			dwSum[0] /= dwPixels;
			dwSum[1] /= dwPixels;
			dwSum[2] /= dwPixels;
      
			adj  = cano_GetNewOffset( dwSum, 0, low, now, high );
			adj |= cano_GetNewOffset( dwSum, 1, low, now, high );
			adj |= cano_GetNewOffset( dwSum, 2, low, now, high );
      
			DBG( _DBG_INFO2, "RedOff   = %d/%d/%d\n",
										(int)low[0],(int)now[0],(int)high[0]);
			DBG( _DBG_INFO2, "GreenOff = %d/%d/%d\n",
										(int)low[1],(int)now[1],(int)high[1]);
			DBG( _DBG_INFO2, "BlueOff  = %d/%d/%d\n",
										(int)low[2],(int)now[2],(int)high[2]);
      
		} else {
			dwSum[0] = 0;
      
			for( dw = 0; dw < dwPixels; dw++ )
				dwSum[0] += ((u_short*)pScanBuffer)[dw];
			dwSum [0] /= dwPixels;
      
			DBG( _DBG_INFO2, "Sum = %lu, ave = %lu\n",
												dwSum[0], dwSum[0] /dwPixels );
      
			adj = cano_GetNewOffset( dwSum, 0, low, now, high );

			a_bRegs[0x3a] = a_bRegs[0x39] = a_bRegs[0x38];
      
			DBG( _DBG_INFO2, "GrayOff = %d/%d/%d\n",
										(int)low[0],(int)now[0],(int)high[0]);
		}
    
		DBG( _DBG_INFO2, "REG[0x38] = %u\n", a_bRegs[0x38] );
		DBG( _DBG_INFO2, "REG[0x39] = %u\n", a_bRegs[0x39] );
		DBG( _DBG_INFO2, "REG[0x3a] = %u\n", a_bRegs[0x3a] );

		_UIO(sanei_lm983x_write(dev->fd, 0x38, &a_bRegs[0x38], 3, SANE_TRUE));
	}
  
	if( m_ScanParam.bDataType == SCANDATATYPE_Color ) {
    	a_bRegs[0x38] = now[0];	
	    a_bRegs[0x39] = now[1];	
		a_bRegs[0x3a] = now[2];
	} else {
    
		a_bRegs[0x38] = a_bRegs[0x39] = a_bRegs[0x3a] = now[0];	
	}
  
	DBG( _DBG_INFO2, "cano_AdjustOffset() done.\n" );
	return SANE_TRUE;
}

/** usb_AdjustDarkShading
 * fine calibration part 1
 *
 */
static SANE_Bool cano_AdjustDarkShading( pPlustek_Device dev )
{
	char         tmp[40];
	pScanParam   pParam   = &dev->scanning.sParam;
	pScanDef     scanning = &dev->scanning;
	pDCapsDef    scaps    = &dev->usbDev.Caps;
	pHWDef       hw       = &dev->usbDev.HwSetting;
	u_short     *bufp;
	unsigned int i, j;
	int          step, stepW;
	u_long       red, green, blue, gray;
  
	DBG( _DBG_INFO2, "cano_AdjustDarkShading()\n" );
	if( usb_IsEscPressed())
		return SANE_FALSE;
   
	m_ScanParam = scanning->sParam;

	if( m_ScanParam.PhyDpi.x > 75)
		m_ScanParam.Size.dwLines = 64;
	else
		m_ScanParam.Size.dwLines = 32;

	m_ScanParam.Origin.y  = 0;
	m_ScanParam.bBitDepth = 16;
  
	m_ScanParam.UserDpi.y    = scaps->OpticDpi.y;
	m_ScanParam.Size.dwBytes = m_ScanParam.Size.dwPixels * 2;
  
	if( hw->bReg_0x26 & _ONE_CH_COLOR &&
		m_ScanParam.bDataType == SCANDATATYPE_Color ) {
		m_ScanParam.Size.dwBytes *= 3;
	}
  
	m_ScanParam.bCalibration = PARAM_DarkShading;
	m_ScanParam.dMCLK        = dMCLK;
  
	sprintf( tmp, "fine-dark.raw" );
	dumpPicInit( &m_ScanParam, tmp );
  
	usb_SetScanParameters( dev, &m_ScanParam );
	if( usb_ScanBegin( dev, SANE_FALSE ) &&
		usb_ScanReadImage( dev, pScanBuffer, m_ScanParam.Size.dwTotalBytes)) {
    
		dumpPic( tmp, pScanBuffer, m_ScanParam.Size.dwTotalBytes );
    
		if(cano_HostSwap_p())
			usb_Swap((u_short *)pScanBuffer, m_ScanParam.Size.dwTotalBytes);
	}
	if (!usb_ScanEnd( dev )){
		DBG( _DBG_ERROR, "cano_AdjustDarkShading() failed\n" );
		return SANE_FALSE;
	}
  
	/* average the n lines, compute reg values */
	if( scanning->sParam.bDataType == SCANDATATYPE_Color ) {
		
		stepW = m_ScanParam.Size.dwPhyPixels;
		step  = m_ScanParam.Size.dwPhyPixels + 1;
		
		for( i=0; i<m_ScanParam.Size.dwPhyPixels; i++ ) {

			red   = 0;
			green = 0;
			blue  = 0;
			bufp  = ((u_short *)pScanBuffer)+i;

			for( j=0; j<m_ScanParam.Size.dwPhyLines; j++ ) {

				if( hw->bReg_0x26 & _ONE_CH_COLOR ) {
					red   += *bufp;   bufp+=step;
					green += *bufp;   bufp+=step;
					blue  += *bufp;   bufp+=step;
				} else {

					red   += bufp[0];
					green += bufp[1];
					blue  += bufp[2];

					bufp += step;
				}
			}

			a_wDarkShading[i]         = red/j   + pParam->swOffset[0];
			a_wDarkShading[i+stepW]   = green/j + pParam->swOffset[1];
			a_wDarkShading[i+stepW*2] = blue/j  + pParam->swOffset[2];
		}
    
		if(cano_HostSwap_p())
			usb_Swap(a_wDarkShading, m_ScanParam.Size.dwPhyPixels * 2 * 3 );

	} else {
		step = m_ScanParam.Size.dwPhyPixels + 1;
		
		for( i=0; i<m_ScanParam.Size.dwPhyPixels; i++ ) {
			
			gray = 0;
			bufp = ((u_short *)pScanBuffer)+i;

			for( j=0; j<m_ScanParam.Size.dwPhyLines; j++) {
				gray += *bufp;   bufp+=step;
			}
			a_wDarkShading[i]= gray/j + pParam->swOffset[0];
		}
		if(cano_HostSwap_p())
			usb_Swap(a_wDarkShading, m_ScanParam.Size.dwPhyPixels * 2 );
			
		memcpy( a_wDarkShading+ m_ScanParam.Size.dwPhyPixels * 2,
				   			a_wDarkShading, m_ScanParam.Size.dwPhyPixels * 2);
		memcpy(a_wDarkShading+ m_ScanParam.Size.dwPhyPixels * 4,
							a_wDarkShading, m_ScanParam.Size.dwPhyPixels * 2);
	}
    
	DBG( _DBG_INFO2, "cano_AdjustDarkShading() done\n" );
	return SANE_TRUE;
}

/** usb_AdjustWhiteShading
 * fine calibration part 2 - read the white calibration area and calculate
 * the gain coefficient for each pixel
 */
static SANE_Bool cano_AdjustWhiteShading( pPlustek_Device dev )
{
	char         tmp[40];
	pScanParam   pParam   = &dev->scanning.sParam;
	pScanDef     scanning = &dev->scanning;
	pDCapsDef    scaps    = &dev->usbDev.Caps;
	pHWDef       hw       = &dev->usbDev.HwSetting;
	u_short     *bufp;
	unsigned int i, j;
	int          step, stepW;
	u_long       red, green, blue, gray;
  
	DBG( _DBG_INFO2, "cano_AdjustWhiteShading()\n" );
	if( usb_IsEscPressed())
		return SANE_FALSE;

	m_ScanParam = scanning->sParam;
	if( m_ScanParam.PhyDpi.x > 75)
		m_ScanParam.Size.dwLines = 64;
	else
		m_ScanParam.Size.dwLines = 32;

	m_ScanParam.Origin.y  = 0;
	m_ScanParam.bBitDepth = 16;

	m_ScanParam.UserDpi.y = scaps->OpticDpi.y;
	m_ScanParam.Size.dwBytes = m_ScanParam.Size.dwPixels * 2;

	if( hw->bReg_0x26 & _ONE_CH_COLOR &&
		m_ScanParam.bDataType == SCANDATATYPE_Color ) {
		m_ScanParam.Size.dwBytes *= 3;
	}

	m_ScanParam.bCalibration = PARAM_WhiteShading;
	m_ScanParam.dMCLK        = dMCLK;

	sprintf( tmp, "fine-white.raw" );
	dumpPicInit( &m_ScanParam, tmp );
  
	if( usb_SetScanParameters( dev, &m_ScanParam ) &&
		usb_ScanBegin( dev, SANE_FALSE ) &&
		usb_ScanReadImage( dev, pScanBuffer, m_ScanParam.Size.dwTotalBytes)) {
    
		dumpPic( tmp, pScanBuffer, m_ScanParam.Size.dwTotalBytes );
    
		if(cano_HostSwap_p())
			usb_Swap((u_short *)pScanBuffer, m_ScanParam.Size.dwTotalBytes);

		if (!usb_ScanEnd( dev )){
			DBG( _DBG_ERROR, "cano_AdjustWhiteShading() failed\n" );
			return SANE_FALSE;
		}
	} else {
		DBG( _DBG_ERROR, "cano_AdjustWhiteShading() failed\n" );
		return SANE_FALSE;
	}
  
	/* average the n lines, compute reg values */
	if( scanning->sParam.bDataType == SCANDATATYPE_Color ) {

		stepW = m_ScanParam.Size.dwPhyPixels;
		step  = m_ScanParam.Size.dwPhyPixels + 1;

		for( i=0; i<m_ScanParam.Size.dwPhyPixels; i++) {

			red   = 0;
			green = 0;
			blue  = 0;
			bufp  = ((u_short *)pScanBuffer)+i;

			for( j=0; j<m_ScanParam.Size.dwPhyLines; j++) {

				if( hw->bReg_0x26 & _ONE_CH_COLOR ) {
					red   += *bufp;   bufp+=step;
					green += *bufp;   bufp+=step;
					blue  += *bufp;   bufp+=step;
				} else {

					red   += bufp[0];
					green += bufp[1];
					blue  += bufp[2];

					bufp += step;
				}
			}

			/* tweaked by the settings in swGain --> 1000/swGain[r,g,b] */
			red   = (65535.*1000./pParam->swGain[0]) * 16384.*j/red;
			green = (65535.*1000./pParam->swGain[1]) * 16384.*j/green;
			blue  = (65535.*1000./pParam->swGain[2]) * 16384.*j/blue;

			a_wWhiteShading[i]         = (red   > 65535? 65535:red  );
			a_wWhiteShading[i+stepW]   = (green > 65535? 65535:green);
			a_wWhiteShading[i+stepW*2] = (blue  > 65535? 65535:blue );
	    }

		if(cano_HostSwap_p())
			usb_Swap(a_wWhiteShading, m_ScanParam.Size.dwPhyPixels * 2 * 3 );
	} else {

		step = m_ScanParam.Size.dwPhyPixels + 1;
		for( i=0; i<m_ScanParam.Size.dwPhyPixels; i++ ){
			gray = 0;
			bufp = ((u_short *)pScanBuffer)+i;

			for( j=0; j<m_ScanParam.Size.dwPhyLines; j++ ) {
				gray += *bufp;   bufp+=step;
			}
     
			gray = (65535.*1000./pParam->swGain[0]) * 16384.*j/gray;
			
			a_wWhiteShading[i]= (gray > 65535? 65535:gray);
		}
		if(cano_HostSwap_p())
			usb_Swap(a_wWhiteShading, m_ScanParam.Size.dwPhyPixels * 2 );

		memcpy(a_wWhiteShading+ m_ScanParam.Size.dwPhyPixels * 2,
							a_wWhiteShading, m_ScanParam.Size.dwPhyPixels * 2);
		memcpy(a_wWhiteShading+ m_ScanParam.Size.dwPhyPixels * 4,
							a_wWhiteShading, m_ScanParam.Size.dwPhyPixels * 2);
	}
    
	DBG( _DBG_INFO2, "cano_AdjustWhiteShading() done\n" );
	return SANE_TRUE;
}

/**
 */
static int cano_DoCalibration( pPlustek_Device dev )
{
	pScanDef  scanning = &dev->scanning;
	pHWDef    hw       = &dev->usbDev.HwSetting;
	pDCapsDef scaps    = &dev->usbDev.Caps;

	if( SANE_TRUE == scanning->fCalibrated )
		return SANE_TRUE;

	DBG( _DBG_INFO2, "cano_DoCalibration()\n" );

	if( _IS_PLUSTEKMOTOR(hw->motorModel)){
		DBG( _DBG_ERROR, "altCalibration can't work with this Plustek motor control setup\n" );
		return SANE_FALSE; /* can't cal this  */
	}

	/* Don't allow calibration settings from the other driver to confuse our use of
	   a few of its functions */
	scaps->workaroundFlag &= ~_WAF_SKIP_WHITEFINE; 
	scaps->workaroundFlag &= ~_WAF_SKIP_FINE; 
	scaps->workaroundFlag &= ~_WAF_BYPASS_CALIBRATION; 

	/* Set the shading position to undefined */
	strip_state = 0;
	usb_PrepareCalibration( dev );
    
	usb_SetMCLK( dev, &scanning->sParam );

	if( !scanning->skipCoarseCalib ) {
		DBG( _DBG_INFO2, "###### ADJUST LAMP (COARSE)#######\n" );
		if( cano_PrepareToReadWhiteCal(dev))
			return SANE_FALSE;
     
		a_bRegs[0x45] &= ~0x10;
		if( !cano_AdjustLightsource(dev)) {
			DBG( _DBG_ERROR, "Coarse Calibration failed!!!\n" );
			return _E_INTERNAL;
		}

	    DBG( _DBG_INFO2, "###### ADJUST OFFSET (COARSE) ####\n" );
		if(cano_PrepareToReadBlackCal(dev))
			return SANE_FALSE;
		
		if( !cano_AdjustOffset(dev)) {
			DBG( _DBG_ERROR, "Coarse Calibration failed!!!\n" );
			return _E_INTERNAL;
		}

		DBG( _DBG_INFO2, "###### ADJUST GAIN (COARSE)#######\n" );
		if(cano_PrepareToReadWhiteCal(dev))
			return SANE_FALSE;
		
		if( !cano_AdjustGain(dev)) {
			DBG( _DBG_ERROR, "Coarse Calibration failed!!!\n" );
			return _E_INTERNAL;
		}
	} else {
		strip_state = 1;
		DBG( _DBG_INFO2, "###### COARSE calibration skipped #######\n" );
	}

	DBG( _DBG_INFO2, "###### ADJUST DARK (FINE) ########\n" );
	if(cano_PrepareToReadBlackCal(dev))
		return SANE_FALSE;

	a_bRegs[0x45] |= 0x10;
	if( !cano_AdjustDarkShading(dev)) {
		DBG( _DBG_ERROR, "Fine Calibration failed!!!\n" );
		return _E_INTERNAL;
	}

	DBG( _DBG_INFO2, "###### ADJUST WHITE (FINE) #######\n" );
	if(cano_PrepareToReadWhiteCal(dev))
		return SANE_FALSE;
    
	if(!usb_ModuleToHome( dev, SANE_TRUE ))
		return SANE_FALSE;
		
	if( !usb_ModuleMove(dev, MOVE_Forward,
			(u_long)dev->usbDev.pSource->ShadingOriginY)) {
		return _E_INTERNAL;
	}
	if( !cano_AdjustWhiteShading(dev)) {
		DBG( _DBG_ERROR, "Fine Calibration failed!!!\n" );
		return _E_INTERNAL;
	}

	/* Lamp on if it's not */
	cano_LampOnAfterCalibration(dev);
	strip_state=0;
    
	/*
	 * home the sensor after calibration
	 */
	usb_ModuleToHome( dev, SANE_TRUE );
	scanning->fCalibrated = SANE_TRUE;
	
	DBG( _DBG_INFO, "cano_DoCalibration() done\n" );
	DBG( _DBG_INFO, "-------------------------\n" );
	DBG( _DBG_INFO, "Static Gain:\n" );
	DBG( _DBG_INFO, "REG[0x3b] = %u\n", a_bRegs[0x3b] );
	DBG( _DBG_INFO, "REG[0x3c] = %u\n", a_bRegs[0x3c] );
	DBG( _DBG_INFO, "REG[0x3d] = %u\n", a_bRegs[0x3d] );
	DBG( _DBG_INFO, "Static Offset:\n" );
	DBG( _DBG_INFO, "REG[0x38] = %u\n", a_bRegs[0x38] );
	DBG( _DBG_INFO, "REG[0x39] = %u\n", a_bRegs[0x39] );
	DBG( _DBG_INFO, "REG[0x3a] = %u\n", a_bRegs[0x3a] );
	DBG( _DBG_INFO, "-------------------------\n" );

	return SANE_TRUE;
}

/* END PLUSTEK-USBCAL.C_.....................................................*/
