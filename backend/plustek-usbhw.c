/*.............................................................................
 * Project : SANE library for Plustek flatbed scanners.
 *.............................................................................
 */

/** @file plustek-usbhw.c
 *  @brief Functions to control the scanner hardware.
 *
 * Based on sources acquired from Plustek Inc.<br>
 * Copyright (C) 2001-2004 Gerhard Jaeger <gerhard@gjaeger.de>
 *
 * History:
 * - 0.40 - starting version of the USB support
 * - 0.41 - added EPSON1250 specific stuff
 *        - added alternative usb_IsScannerReady function
 * - 0.42 - added warmup stuff
 *        - added UMAX 3400 stuff
 *        - fixed problem with minimum wait time...
 * - 0.43 - added usb_switchLamp for non-Plustek devices
 * - 0.44 - added bStepsToReverse and active Pixelstart values
 *        - to resetRegister function
 *        - modified getLampStatus function for CIS devices
 *        - added usb_Wait4Warmup()
 *        - moved usb_IsEscPressed to this file
 *        - added usb_switchLampX
 *        - do now not reinitialized MISC I/O pins upon reset registers
 * - 0.45 - added function usb_AdjustLamps() to tweak CIS lamp settings
 *        - fixed NULL pointer problem in lamp-off ISR
 *        - added usb_AdjustCISLampSettings()
 *        - skipping warmup for CIS devices 
 * - 0.46 - fixed problem in usb_GetLampStatus for CIS devices, as we
 *          read back reg[0x29] to wrong position
 *          made it compile without itimer definitions
 * - 0.47 - moved usb_HostSwap() and usb_Swap() to this file.
 *        - fixed lampOff timer for systems w/o setitimer
 *        - added lamp off adjustment for CIS devices
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
#ifdef HAVE_SYS_TIME_H 
#include <sys/time.h>
#endif

#define DEV_LampReflection      1
#define DEV_LampTPA             2
#define DEV_LampAll             3
#define DEV_LampPositive        4
#define DEV_LampNegative        5

/* HEINER: check the tick counts 'cause 1 tick on NT is about 10ms */

static u_long dwCrystalFrequency = 48000000UL;

static SANE_Bool fModuleFirstHome;  /* HEINER: this has to be initialized */
static SANE_Bool fLastScanIsAdf;
static u_char    a_bRegs[0x80];     /**< our global register file */


/** the NatSemi 983x is a big endian chip, and the line protocol data all
 *  arrives big-endian.  This determines if we need to swap to host-order
 */
static SANE_Bool usb_HostSwap( void )
{
	u_short        pattern  = 0xfeed; /* deadbeef */
	unsigned char *bytewise = (unsigned char *)&pattern;

	if ( bytewise[0] == 0xfe ) {
		DBG( _DBG_READ, "We're big-endian!  No need to swap!\n" );
		return 0;
	}
	DBG( _DBG_READ, "We're little-endian!  NatSemi LM983x is big!\n" );
	DBG( _DBG_READ, "--> Must swap calibration data!\n" );
	return 1;
}

/** as the name says..
 */
static void usb_Swap( u_short *pw, u_long dwBytes )
{
	for( dwBytes /= 2; dwBytes--; pw++ )
		_SWAP(((u_char*) pw)[0], ((u_char*)pw)[1]);
}

/** usb_GetMotorSet
 * according to the model, the function returns the address of
 * the corresponding entry of the Motor table
 */
static pClkMotorDef usb_GetMotorSet( eModelDef model )
{
	int i;

	for( i = 0; i < MODEL_LAST; i++ ) {
		if( model == Motors[i].motorModel ) {
			return &(Motors[i]);
		}
	}

	return NULL;
}

/** switch motor on or off
 * @param handle - handle to open USB device
 * @param fOn    - SANE_TRUE means motor on, SANE_FALSE means motor off
 * @return always SANE_TRUE
 */
static SANE_Bool usb_MotorOn( int handle, SANE_Bool fOn )
{
	if( fOn )
		a_bRegs[0x45] |= 0x10;
	else
		a_bRegs[0x45] &= ~0x10;

	usbio_WriteReg( handle, 0x45, a_bRegs[0x45] );

	return SANE_TRUE;
}

/** check if scanner is ready
 */
static SANE_Bool usb_IsScannerReady( pPlustek_Device dev )
{
	u_char value;
	double 		   len;	
	long           timeout;
    struct timeval t;

	/* time in s = 1000*scanner length in inches/max step speed/in */
	len = (dev->usbDev.Caps.Normal.Size.y/(double)_MEASURE_BASE) + 5;
	len = (1000.0 * len)/dev->usbDev.HwSetting.dMaxMoveSpeed;
	len /= 1000.0;

	/* wait at least 10 seconds... */
	if( len < 10 )
		len = 10;
	
	gettimeofday( &t, NULL);	
	timeout = t.tv_sec + len;

	do {	
		_UIO( usbio_ReadReg( dev->fd, 7, &value));

		if( value == 0 ) {
			_UIO( usbio_ResetLM983x( dev ));
			return SANE_TRUE;
		}

		if((value == 0) || (value >= 0x20) || (value == 0x03)) {

			if( !usbio_WriteReg( dev->fd, 0x07, 0 )) {
				DBG( _DBG_ERROR, "Scanner not ready!!!\n" );
				return SANE_FALSE;
			}
			else
				return SANE_TRUE;	
		}
	
		gettimeofday( &t, NULL);	
		
	} while( t.tv_sec < timeout );		
	
	DBG( _DBG_ERROR, "Scanner not ready!!!\n" );
	return SANE_FALSE;
}

/**
 */
static SANE_Bool usb_SensorAdf( int handle )
{
	u_char value;

	usbio_ReadReg( handle, 0x02, &value );

	return (value & 0x20);
}

/**
 */
static SANE_Bool usb_SensorPaper( int handle )
{
	u_char value;

	usbio_ReadReg( handle, 0x02, &value );

	return (value & 0x02);
}

/**
 * Home sensor always on when backward move.
 * dwStep is steps to move and based on 300 dpi, but
 * if the action is MOVE_Both, it becomes the times
 * to repeatly move the module around the scanner and
 * 0 means forever.
 */
static SANE_Bool usb_ModuleMove( pPlustek_Device dev,
								 u_char bAction, u_long dwStep )
{
	SANE_Status  res;
	u_char       bReg2, reg7, mclk_div;
	u_short      wFastFeedStepSize;
	double       dMaxMoveSpeed;
	pClkMotorDef clk;
	pHWDef       hw = &dev->usbDev.HwSetting;

	if( bAction != MOVE_ToPaperSensor   &&
		bAction != MOVE_EjectAllPapers  &&
		bAction != MOVE_SkipPaperSensor &&
		bAction != MOVE_ToShading       && !dwStep ) {

		return SANE_TRUE;
	}

	if( !usb_IsScannerReady( dev )) {
		DBG( _DBG_ERROR, "Sensor-position NOT reached\n" );
		return SANE_FALSE;
	}

	if( bAction == MOVE_EjectAllPapers ) {

		double d = hw->dMaxMoveSpeed;

		hw->dMaxMoveSpeed += 0.6;

		do {
			if( usb_SensorPaper(dev->fd) &&
							!usb_ModuleMove(dev,MOVE_SkipPaperSensor, 0 )) {
				return SANE_FALSE;
			}

			if( usb_SensorAdf(dev->fd) &&
							!usb_ModuleMove(dev,MOVE_ToPaperSensor, 0 )) {
				return SANE_FALSE;
            }

		} while( usb_SensorPaper(dev->fd));

		if(!usb_ModuleMove( dev, MOVE_Forward, 300 * 3))
			return SANE_FALSE;

		usbio_WriteReg( dev->fd, 0x07, 0);
		usbio_WriteReg( dev->fd, 0x58, a_bRegs[0x58]);

		usbio_ReadReg( dev->fd, 0x02, &bReg2 );
		hw->dMaxMoveSpeed = d;

		return SANE_TRUE;
	}

	usbio_WriteReg( dev->fd, 0x0a, 0 );

	/* Compute fast feed step size, use equation 3 and equation 8 */
	if( bAction == MOVE_ToShading )	{

		double dToShadingSpeed = 0.0; /* HEINER: check callRegistry.GetToShadingSpeed(); */

		if( dToShadingSpeed != 0.0 )
			dMaxMoveSpeed = dToShadingSpeed;
		else {

			if( hw->dMaxMoveSpeed > 0.5 )
				dMaxMoveSpeed = hw->dMaxMoveSpeed - 0.5;
			else
				dMaxMoveSpeed = hw->dMaxMoveSpeed;
		}
	} else {

		dMaxMoveSpeed = hw->dMaxMoveSpeed;
	}

	clk = usb_GetMotorSet( hw->motorModel );

	mclk_div = clk->mclk_fast;

	wFastFeedStepSize = (u_short)(dwCrystalFrequency /
						((u_long)mclk_div * 8UL * 1 *
						dMaxMoveSpeed * 4 * hw->wMotorDpi));

	a_bRegs[0x48] = (u_char)(wFastFeedStepSize >> 8);
	a_bRegs[0x49] = (u_char)(wFastFeedStepSize & 0xFF);

	dwStep = dwStep * hw->wMotorDpi / 300UL;
	a_bRegs[0x4a] = _HIBYTE(_LOWORD(dwStep));
	a_bRegs[0x4b] = _LOBYTE(_LOWORD(dwStep));

	a_bRegs[0x45] |= 0x10;

	DBG( _DBG_INFO2,"MotorDPI=%u, MaxMoveSpeed=%.3f, "
					"FFStepSize=%u, Steps=%lu\n", hw->wMotorDpi,
					hw->dMaxMoveSpeed, wFastFeedStepSize, dwStep );
	DBG( _DBG_INFO2, "MOTOR: "
					"PWM=0x%02x, PWM_DUTY=0x%02x 0x45=0x%02x "
                    "0x48=0x%02x, 0x49=0x%02x \n",
					a_bRegs[0x56], a_bRegs[0x57], a_bRegs[0x45],
					a_bRegs[0x48], a_bRegs[0x49] );

	DBG( _DBG_INFO2,"MCLK_FFW = %u --> 0x%02x\n", mclk_div, (mclk_div-1)*2 );

	/* The setting for chassis moving is:
	 * MCLK divider = 6, 8 bits/pixel, HDPI divider = 12,
	 * no integration time adjustment and 1 channel grayscale
	 */

	/* MCLK divider = 6 */
	if( !usbio_WriteReg(dev->fd, 0x08, (mclk_div-1)*2 /*0x0A*/))
		return SANE_FALSE;

	/* 8 bits/pixel, HDPI divider = 12 */
	if( !usbio_WriteReg(dev->fd, 0x09, 0x1F))
		return SANE_FALSE;

	/* Turn off integration time adjustment */
	if( !usbio_WriteReg(dev->fd, 0x19, 0))
		return SANE_FALSE;

	/* 1 channel grayscale, green channel */
	if( !usbio_WriteReg(dev->fd, 0x26, 0x0C))
		return SANE_FALSE;

	_UIO(sanei_lm983x_write(dev->fd, 0x48, &a_bRegs[0x48], 2, SANE_TRUE));
	_UIO(sanei_lm983x_write(dev->fd, 0x4A, &a_bRegs[0x4A], 2, SANE_TRUE));

	/* disable home */
	if( !usbio_WriteReg(dev->fd, 0x58, a_bRegs[0x58] & ~7))
		return SANE_FALSE;

	if( !usbio_WriteReg(dev->fd, 0x45, a_bRegs[0x45] ))
		return SANE_FALSE;

	if( bAction == MOVE_Forward || bAction == MOVE_ToShading )
		reg7 = 5;
	else if( bAction == MOVE_Backward )
		reg7 = 6;
	else if( bAction == MOVE_ToPaperSensor || bAction == MOVE_EjectAllPapers ||
			 bAction == MOVE_SkipPaperSensor ) {
		reg7 = 1;
	} else {
		return SANE_TRUE;
    }

	if( usbio_WriteReg( dev->fd, 0x07, reg7 )) {

		long           dwTicks;
	    struct timeval start_time, t2;

		res = gettimeofday( &start_time, NULL );	

		/* 20000 NT-Ticks means 200s */
	    dwTicks = start_time.tv_sec + 200;

		if( bAction == MOVE_ToPaperSensor ) {

			for(;;)	{

				if( usb_SensorPaper( dev->fd )) {
					usbio_WriteReg( dev->fd, 0x07, 0 );
					usbio_WriteReg( dev->fd, 0x58, a_bRegs[0x58] );
					usbio_ReadReg ( dev->fd, 0x02, &bReg2 );
					return SANE_TRUE;
				}

				gettimeofday(&t2, NULL);	
				if( t2.tv_sec > dwTicks )
					break;	
			}
		} else if( bAction == MOVE_SkipPaperSensor ) {

			for(;;)	{

				if( usb_SensorPaper( dev->fd )) {
					usbio_WriteReg( dev->fd, 0x07, 0 );
					usbio_WriteReg( dev->fd, 0x58, a_bRegs[0x58] );
					usbio_ReadReg ( dev->fd, 0x02, &bReg2 );
					return SANE_TRUE;
				}

				gettimeofday(&t2, NULL);	
				if( t2.tv_sec > dwTicks )
					break;	
			}
		} else {

			for(;;)	{

				usleep(10 * 1000);
				_UIO( usbio_ReadReg( dev->fd, 0x07, &reg7));

				if( !reg7 ) {
					usbio_WriteReg( dev->fd, 0x58, a_bRegs[0x58] );
					usbio_ReadReg( dev->fd, 0x02, &bReg2 );
					return SANE_TRUE;
				}

				gettimeofday(&t2, NULL);	
				if( t2.tv_sec > dwTicks )
					break;	
			}
		}

		usbio_WriteReg( dev->fd, 0x58, a_bRegs[0x58] );
		usbio_ReadReg ( dev->fd, 0x02, &bReg2 );
	}

	DBG( _DBG_ERROR, "Position NOT reached\n" );
	return SANE_FALSE;
}

/**
 */
static SANE_Bool usb_ModuleToHome( pPlustek_Device dev, SANE_Bool fWait )
{
	u_char    mclk_div;
	u_char    value;
	pDCapsDef scaps = &dev->usbDev.Caps;
	pHWDef    hw    = &dev->usbDev.HwSetting;

	/* Check if merlin is ready for setting command */
	usbio_WriteReg( dev->fd, 0x58, hw->bReg_0x58 );
	usbio_ReadReg ( dev->fd, 2, &value );

	/* check current position... */
#if 0	
	_UIO(usbio_ReadReg( dev->fd, 2, &value ));
#endif	

	if( value & 1 ) {
		fModuleFirstHome = SANE_FALSE;
		return SANE_TRUE;
	}

	_UIO( usbio_ReadReg( dev->fd, 0x07, &value ));

	if( fModuleFirstHome ) {
		fModuleFirstHome = SANE_FALSE;
		if( hw->motorModel != MODEL_Tokyo600 )
			usb_ModuleMove( dev, MOVE_Forward, hw->wMotorDpi / 2);
	}

	/* if not homing, do it... */
	if( value != 2 ) {

		u_short wFastFeedStepSize;

		if( hw->motorModel == MODEL_Tokyo600 ) {
			usbio_WriteReg( dev->fd, 0x07, 0 );
		} else 	{
				
			_UIO( usbio_ResetLM983x( dev ));

			usleep(200*1000);
		}
		
		if(!_IS_PLUSTEKMOTOR(hw->motorModel)) {

			pClkMotorDef clk;

			clk = usb_GetMotorSet( hw->motorModel );

			a_bRegs[0x56] = clk->pwm_fast;
			a_bRegs[0x57] = clk->pwm_duty_fast;
			mclk_div      = clk->mclk_fast;

		} else {

			mclk_div = 6;

			if( scaps->OpticDpi.x == 1200 || scaps->bPCB == 2) {
				switch( hw->motorModel ) {

				case MODEL_KaoHsiung:
				case MODEL_HuaLien:
				default:
					a_bRegs[0x56] = 1;
					a_bRegs[0x57] = 63;
					break;
				}
			} else { /* if(Device.Caps.OpticDpi.x == 600) */

				switch( hw->motorModel ) {

				case MODEL_Tokyo600:
					a_bRegs[0x56] = 4;
					a_bRegs[0x57] = 4;	/* 2; */
					break;
				case MODEL_HuaLien:
					if( dev->caps.dwFlag & SFLAG_ADF ) {
						a_bRegs[0x56] = 64;	/* 32; */
						a_bRegs[0x57] = 4;	/* 16; */
					} else {
						a_bRegs[0x56] = 32;
						a_bRegs[0x57] = 16;
					}
					break;
				
				case MODEL_KaoHsiung:
				default:
					a_bRegs[0x56] = 64;
					a_bRegs[0x57] = 20;
					break;
				}
			}
		}

		/* Compute fast feed step size, use equation 3 and equation 8
		 * assumptions: MCLK = 6, Lineratemode (CM=1)
		 */
		wFastFeedStepSize = (u_short)(dwCrystalFrequency / (mclk_div * 8 * 1 *
									  hw->dMaxMotorSpeed * 4 * hw->wMotorDpi));
		a_bRegs[0x48] = (u_char)(wFastFeedStepSize >> 8);
		a_bRegs[0x49] = (u_char)(wFastFeedStepSize & 0xFF);
		a_bRegs[0x4a] = 0;
		a_bRegs[0x4b] = 0;

		a_bRegs[0x45] |= 0x10;

		DBG( _DBG_INFO2,"MotorDPI=%u, MaxMotorSpeed=%.3f, FFStepSize=%u\n",
						hw->wMotorDpi, hw->dMaxMotorSpeed, wFastFeedStepSize );
		DBG( _DBG_INFO, "MOTOR: "
						"PWM=0x%02x, PWM_DUTY=0x%02x 0x45=0x%02x "
                        "0x48=0x%02x, 0x49=0x%02x\n",
						a_bRegs[0x56], a_bRegs[0x57],
						a_bRegs[0x45], a_bRegs[0x48], a_bRegs[0x49] );

		/* The setting for chassis moving is:
		 * MCLK divider = 6, 8 bits/pixel, HDPI divider = 12,
		 * no integration time adjustment and 1 channel grayscale
	 	 */
		/* MCLK divider = 6 */
		value = (u_char)((mclk_div-1) * 2);

		DBG( _DBG_INFO, "MCLK_FFW = %u --> 0x%02x\n", mclk_div, value );

		if( !usbio_WriteReg(dev->fd, 0x08, value))
			return SANE_FALSE;

		/* 8 bits/pixel, HDPI divider = 12 */
		if( !usbio_WriteReg(dev->fd, 0x09, 0x1F))
			return SANE_FALSE;

		/* Turn off integration time adjustment */
		if( !usbio_WriteReg(dev->fd, 0x19, 0))
			return SANE_FALSE;

		/* 1 channel grayscale, green channel */
		if( !usbio_WriteReg(dev->fd, 0x26, 0x8C))
			return SANE_FALSE;
		
		_UIO(sanei_lm983x_write(dev->fd, 0x48, &a_bRegs[0x48], 4, SANE_TRUE));
		_UIO(sanei_lm983x_write(dev->fd, 0x56, &a_bRegs[0x56], 3, SANE_TRUE));

		if( !usbio_WriteReg(dev->fd, 0x45, a_bRegs[0x45]))
			return SANE_FALSE;

		usbio_WriteReg(dev->fd, 0x0a, 0);

		if( hw->motorModel == MODEL_HuaLien && scaps->OpticDpi.x == 600 )
			usleep(100 * 1000);

		if( !usbio_WriteReg(dev->fd, 0x07, 2))
			return SANE_FALSE;
		
#if 0
		if(Device.HwSetting.motorModel == MODEL_Tokyo600)
		{
			DWORD	dwSpeedUp = GetTickCount () + 250;
			
			//while(GetTickCount () < dwSpeedUp)
			while((int)(dwSpeedUp - GetTickCount ()) > 0)
			{
				Sleep (10);
				if (!ReadRegister (0x07, &value))
					return FALSE;
				if (!value)
					return TRUE;
			}
			wFastFeedStepSize = (WORD)(dwCrystalFrequency / (6UL * 8UL * 1 * Device.HwSetting.dMaxMotorSpeed * 4 *
				Device.HwSetting.wMotorDpi) * 60 / 78);	
			a_bRegs [0x48] = (BYTE)(wFastFeedStepSize >> 8);
			a_bRegs [0x49] = (BYTE)(wFastFeedStepSize & 0xFF);
			WriteRegisters(0x48, &a_bRegs[0x48], 2);
		}
#endif
	}

	if( fWait ) {

		long           dwTicks;
	    struct timeval start_time, t2;
		
		gettimeofday( &start_time, NULL );	

	    dwTicks =  start_time.tv_sec + 150;

		for(;;)	{

			usleep( 20 * 1000 );
			_UIO( usbio_ReadReg( dev->fd, 0x07, &value ));

			if (!value)
				return SANE_TRUE;

			gettimeofday(&t2, NULL);	
		    if( t2.tv_sec > dwTicks )
				break;
		}
		return SANE_FALSE;
	}

	return SANE_TRUE;
}

/**
 */
static SANE_Bool usb_MotorSelect( pPlustek_Device dev, SANE_Bool fADF )
{
	pDCapsDef sCaps = &dev->usbDev.Caps;
	pHWDef    hw    = &dev->usbDev.HwSetting;

	if(!_IS_PLUSTEKMOTOR(hw->motorModel)) {
		return SANE_TRUE;
	}
	
	if( fADF ) {

		if( sCaps->bCCD == kNEC3778 ) {
		
			hw->wMotorDpi      = 300;
			hw->dMaxMotorSpeed = 1.5;
			hw->dMaxMoveSpeed  = 1.5;
			sCaps->OpticDpi.y  = 600;
		}
		a_bRegs[0x5b] |= 0x80;
		
	} else {
	
		if( sCaps->bCCD == kNEC3778 ) {
			
			hw->wMotorDpi      = 600;
			hw->dMaxMotorSpeed = 1.1;
			hw->dMaxMoveSpeed  = 0.9;
			sCaps->OpticDpi.y  = 1200;
		}
		a_bRegs[0x5b] &= ~0x80;
	}

	/* To stop the motor moving */
	usbio_WriteReg( dev->fd, 0x07, 0 );		
	usleep(10 * 1000);

	usbio_WriteReg( dev->fd, 0x5b, a_bRegs[0x5b] );
	return SANE_TRUE;
}

/** function to adjust the lamp settings of a device
 */
static SANE_Bool usb_AdjustLamps( pPlustek_Device dev )
{
	pHWDef hw = &dev->usbDev.HwSetting;

	a_bRegs[0x2c] = hw->red_lamp_on / 256;
	a_bRegs[0x2d] = hw->red_lamp_on & 0xFF;
	a_bRegs[0x2e] = hw->red_lamp_off / 256;
	a_bRegs[0x2f] = hw->red_lamp_off & 0xFF;

	a_bRegs[0x30] = hw->green_lamp_on / 256;
	a_bRegs[0x31] = hw->green_lamp_on & 0xFF;
	a_bRegs[0x32] = hw->green_lamp_off / 256;
	a_bRegs[0x33] = hw->green_lamp_off & 0xFF;

	a_bRegs[0x34] = hw->blue_lamp_on / 256;
	a_bRegs[0x35] = hw->blue_lamp_on & 0xFF;
	a_bRegs[0x36] = hw->blue_lamp_off / 256;
	a_bRegs[0x37] = hw->blue_lamp_off & 0xFF;

	return sanei_lm983x_write( dev->fd, 0x2c,
							   &a_bRegs[0x2c], 0x37-0x2c+1, SANE_TRUE );
}

/**
 */
static void usb_AdjustCISLampSettings( Plustek_Device *dev, SANE_Bool on )
{
	pHWDef hw = &dev->usbDev.HwSetting;

	if( !(hw->bReg_0x26 & _ONE_CH_COLOR))
		return;

	DBG( _DBG_INFO2, "AdjustCISLamps(%u)\n", on );

	if((dev->scanning.sParam.bDataType == SCANDATATYPE_Gray) ||
	   (dev->scanning.sParam.bDataType == SCANDATATYPE_BW)) {

		DBG( _DBG_INFO2, " * setting mono mode\n" );
		hw->bReg_0x29 = hw->illu_mono.mode;

		memcpy( &hw->red_lamp_on,
				&hw->illu_mono.red_lamp_on, sizeof(u_short) * 6 );

	} else {

		DBG( _DBG_INFO2, " * setting color mode\n" );
		hw->bReg_0x29 = hw->illu_color.mode;

		memcpy( &hw->red_lamp_on,
				&hw->illu_color.red_lamp_on, sizeof(u_short) * 6 );
	}

	if( !on ) {

		hw->red_lamp_on    = 0x3fff;
		hw->red_lamp_off   = 0;
		hw->green_lamp_on  = 0x3fff;
		hw->green_lamp_off = 0;
		hw->blue_lamp_on   = 0x3fff;
		hw->blue_lamp_off  = 0;
	} else {

		if( dev->adj.rlampoff > 0 ) {
			hw->red_lamp_off = dev->adj.rlampoff;

			if( hw->red_lamp_off > 0x3fff )
				hw->red_lamp_off = 0x3fff;
			DBG( _DBG_INFO2,
			     " * red_lamp_off adjusted: %u\n", hw->red_lamp_off );
		}

		if( dev->adj.glampoff > 0 ) {
			hw->green_lamp_off = dev->adj.glampoff;

			if( hw->green_lamp_off > 0x3fff )
				hw->green_lamp_off = 0x3fff;
			DBG( _DBG_INFO2,
			     " * green_lamp_off adjusted: %u\n", hw->green_lamp_off );
		}

		if( dev->adj.blampoff > 0 ) {
			hw->blue_lamp_off = dev->adj.blampoff;

			if( hw->blue_lamp_off > 0x3fff )
				hw->blue_lamp_off = 0x3fff;
			DBG( _DBG_INFO2,
			     " * blue_lamp_off adjusted: %u\n", hw->blue_lamp_off );
		}
	}

	a_bRegs[0x29] = hw->bReg_0x29;
	usb_AdjustLamps( dev );
}

/** according to the flag field, we return the register and
 * it's maks to turn on/off the lamp.
 * @param flag - field to check
 * @param reg  - pointer to a var to receive the register value
 * @param msk  - pointer to a var to receive the mask value
 * @return Nothing
 */
static void usb_GetLampRegAndMask( u_long flag, SANE_Byte *reg, SANE_Byte *msk )
{
	if( _MIO6 == ( _MIO6 & flag )) {
		*reg = 0x5b;
		*msk = 0x80;

   	} else if( _MIO5 == ( _MIO5 & flag )) {
		*reg = 0x5b;
		*msk = 0x08;

   	} else if( _MIO4 == ( _MIO4 & flag )) {
		*reg = 0x5a;
		*msk = 0x80;

   	} else if( _MIO3 == ( _MIO3 & flag )) {
		*reg = 0x5a;
		*msk = 0x08;
   	
    } else if( _MIO2 == ( _MIO2 & flag )) {
		*reg = 0x59;
		*msk = 0x80;

   	} else if( _MIO1 == ( _MIO1 & flag )) {
		*reg = 0x59;
		*msk = 0x08;

   	} else {
       *reg = 0;
       *msk = 0;
	}
}

/** usb_Get
 * This function returns the current lamp in use.
 * For non Plustek devices, it always returns DEV_LampReflection.
 * @param dev  - pointer to our device structure,
 *               it should contain all we need
 * @return - 0 if the scanner hasn't been used before, DEV_LampReflection
 *           for the normal lamp, or DEV_LampTPA for negative/transparency
 *           lamp
 */
static int usb_GetLampStatus( pPlustek_Device dev )
{
	int    iLampStatus = 0;
	pHWDef hw          = &dev->usbDev.HwSetting;
	pDCapsDef sc       = &dev->usbDev.Caps;
	SANE_Byte reg, msk, val;


	if( NULL == hw ) {
		DBG( _DBG_ERROR, "NULL-Pointer detected: usb_GetLampStatus()\n" );
		return -1;
	}

 	/* do we use the misc I/O pins for switching the lamp ? */
	if( _WAF_MISC_IO_LAMPS & sc->workaroundFlag ) {

		usb_GetLampRegAndMask( sc->lamp, &reg, &msk );

		if( 0 == reg ) {
#if 0
			/* probably not correct, esp. when changing from color to gray...*/
			usbio_ReadReg( dev->fd, 0x29, &a_bRegs[0x29] );
			if( a_bRegs[0x29] & 3 )
#else
			usbio_ReadReg( dev->fd, 0x29, &reg );
			if( reg & 3 )
#endif
				iLampStatus |= DEV_LampReflection;
		} else {

			/* check if the lamp is on */
			usbio_ReadReg( dev->fd, reg, &val );

			DBG( _DBG_INFO, "REG[0x%02x] = 0x%02x (msk=0x%02x)\n",reg,val,msk);
			if( val & msk )
				iLampStatus |= DEV_LampReflection;

   			/* if the device supports a TPA, we check this here */
	  		if( sc->wFlags & DEVCAPSFLAG_TPA ) {

				usb_GetLampRegAndMask( _GET_TPALAMP(sc->lamp), &reg, &msk );
				usbio_ReadReg( dev->fd, reg, &val );
				DBG( _DBG_INFO, "REG[0x%02x] = 0x%02x (msk=0x%02x)\n",
																reg,val,msk);
				if( val & msk )
					iLampStatus |= DEV_LampTPA;
			}
		}

	} else {

		sanei_lm983x_read(dev->fd, 0x29,&a_bRegs[0x29],0x37-0x29+1,SANE_TRUE);

		if((a_bRegs[0x29] & 3) == 1) {

/* HEINER: BETTER define register to check ! */

			if(!_IS_PLUSTEKMOTOR(hw->motorModel)) {
				iLampStatus |= DEV_LampReflection;

            } else {

				if((a_bRegs[0x2e] * 256 + a_bRegs[0x2f]) > hw->wLineEnd )
					iLampStatus |= DEV_LampReflection;

				if((a_bRegs[0x36] * 256 + a_bRegs[0x37]) > hw->wLineEnd )
					iLampStatus |= DEV_LampTPA;
			}
		}
	}

	DBG( _DBG_INFO, "LAMP-STATUS: 0x%08x\n", iLampStatus );
	return iLampStatus;
}

/** usb_switchLampX
 * used for all devices that use some misc I/O pins to switch the lamp
 */
static SANE_Bool usb_switchLampX( pPlustek_Device dev,
                                  SANE_Bool on, SANE_Bool tpa )
{
	SANE_Byte reg, msk;
	pDCapsDef sc = &dev->usbDev.Caps;

	if( tpa )
		usb_GetLampRegAndMask( _GET_TPALAMP(sc->lamp), &reg, &msk );
	else
		usb_GetLampRegAndMask( sc->lamp, &reg, &msk );

	if( 0 == reg )
		return SANE_FALSE; /* no need to switch something */

	DBG( _DBG_INFO, "usb_switchLampX(ON=%u,TPA=%u)\n", on, tpa );

	if( on )
		a_bRegs[reg] |= msk;
	else
		a_bRegs[reg] &= ~msk;

	DBG( _DBG_INFO, "Switch Lamp: %u, regs[0x%02x] = 0x%02x\n",
	                on, reg, a_bRegs[reg] );
	usbio_WriteReg( dev->fd, reg, a_bRegs[reg] );
	return SANE_TRUE;
}

/** usb_switchLamp
 * used for all devices that use some misc I/O pins to switch the lamp
 */
static SANE_Bool usb_switchLamp( pPlustek_Device dev, SANE_Bool on )
{
	SANE_Bool result;

	if((dev->scanning.sParam.bSource == SOURCE_Negative) ||
	   (dev->scanning.sParam.bSource == SOURCE_Transparency)) {
		result = usb_switchLampX( dev, on, SANE_TRUE );
	} else {
		result = usb_switchLampX( dev, on, SANE_FALSE );
	}
	return result;
}

/** usb_LedOn
 *
 */
static void usb_LedOn( pPlustek_Device dev, SANE_Bool fOn )
{
	u_char value;

	if( dev->usbDev.HwSetting.motorModel != MODEL_HuaLien )
		return;

	value = a_bRegs[0x0d];

	/* if(ReadRegister(0x0d, &value)) */
	{
		if( fOn )
			value |= 0x10;
		else
			value &= ~0x10;

		a_bRegs[0x0d] = value;
		usbio_WriteReg( dev->fd, 0x0d, value );
	}
}

/** usb_LampOn
 */
static SANE_Bool usb_LampOn( pPlustek_Device dev,
                             SANE_Bool fOn, SANE_Bool fResetTimer )
{
	pDCapsDef      sc          = &dev->usbDev.Caps;
	pScanDef       scanning    = &dev->scanning;
	pHWDef         hw          = &dev->usbDev.HwSetting;
	int            iLampStatus = usb_GetLampStatus( dev );
	int            lampId      = -1;
	struct timeval t;

	if( NULL == scanning ) {
		DBG( _DBG_ERROR, "NULL-Pointer detected: usb_LampOn()\n" );
		return SANE_FALSE;
	}

	switch( scanning->sParam.bSource ) {

	case SOURCE_Reflection:
	case SOURCE_ADF:
		lampId = DEV_LampReflection;
		break;

	case SOURCE_Transparency:
	case SOURCE_Negative:
		lampId = DEV_LampTPA;
		break;
	}

	if( fOn ) {
	
		if( iLampStatus != lampId ) {
			
			DBG( _DBG_INFO, "Switching Lamp on\n" );

/* here we might have to switch off the TPA/Main lamp before
 * using the other one
 */
			if( lampId != dev->usbDev.currentLamp ) {

				if( dev->usbDev.currentLamp == DEV_LampReflection )
					usb_switchLampX( dev, SANE_FALSE, SANE_FALSE );
				else
					usb_switchLampX( dev, SANE_FALSE, SANE_TRUE );
			}

			memset( &a_bRegs[0x29], 0, (0x37-0x29+1));
			
			a_bRegs[0x29] = hw->bReg_0x29;

			if( !usb_switchLamp(dev, SANE_TRUE )) {

				if( lampId == DEV_LampReflection ) {
					a_bRegs[0x2e] = 16383 / 256;
					a_bRegs[0x2f] = 16383 % 256;
				}

				if( lampId == DEV_LampTPA ) {
					a_bRegs[0x36] = 16383 / 256;
					a_bRegs[0x37] = 16383 % 256;
				}
			}

			if( _WAF_MISC_IO_LAMPS & sc->workaroundFlag ) {
			
				a_bRegs[0x2c] = hw->red_lamp_on / 256;
				a_bRegs[0x2d] = hw->red_lamp_on & 0xFF;
				a_bRegs[0x2e] = hw->red_lamp_off / 256;
				a_bRegs[0x2f] = hw->red_lamp_off & 0xFF;

				a_bRegs[0x30] = hw->green_lamp_on / 256;
				a_bRegs[0x31] = hw->green_lamp_on & 0xFF;
				a_bRegs[0x32] = hw->green_lamp_off / 256;
				a_bRegs[0x33] = hw->green_lamp_off & 0xFF;

				a_bRegs[0x34] = hw->blue_lamp_on / 256;
				a_bRegs[0x35] = hw->blue_lamp_on & 0xFF;
				a_bRegs[0x36] = hw->blue_lamp_off / 256;
				a_bRegs[0x37] = hw->blue_lamp_off & 0xFF;
			}
			
			sanei_lm983x_write( dev->fd, 0x29,
			                    &a_bRegs[0x29], 0x37-0x29+1, SANE_TRUE );

			if( lampId != dev->usbDev.currentLamp ) {
			
				dev->usbDev.currentLamp = lampId;
				
				if( fResetTimer ) {
			
					gettimeofday( &t, NULL );	
					dev->usbDev.dwTicksLampOn = t.tv_sec;
					DBG( _DBG_INFO, "Warmup-Timer started\n" );
				}
			}
		}

	} else {

		int iStatusChange = iLampStatus & ~lampId;
		
		if( iStatusChange != iLampStatus ) {

			DBG( _DBG_INFO, "Switching Lamp off\n" );
		
			memset( &a_bRegs[0x29], 0, 0x37-0x29+1 );

			if( !usb_switchLamp(dev, SANE_FALSE )) {
			
				if( iStatusChange & DEV_LampReflection ) {
					a_bRegs[0x2e] = 16383 / 256;
					a_bRegs[0x2f] = 16383 % 256;
				}
    	
				if( iStatusChange & DEV_LampTPA ) {
					a_bRegs[0x36] = 16383 / 256;
					a_bRegs[0x37] = 16383 % 256;
				}
			}

			if( _WAF_MISC_IO_LAMPS & sc->workaroundFlag ) {
			
				a_bRegs[0x2c] = hw->red_lamp_on / 256;
				a_bRegs[0x2d] = hw->red_lamp_on & 0xFF;
				a_bRegs[0x2e] = hw->red_lamp_off / 256;
				a_bRegs[0x2f] = hw->red_lamp_off & 0xFF;

				a_bRegs[0x30] = hw->green_lamp_on / 256;
				a_bRegs[0x31] = hw->green_lamp_on & 0xFF;
				a_bRegs[0x32] = hw->green_lamp_off / 256;
				a_bRegs[0x33] = hw->green_lamp_off & 0xFF;

				a_bRegs[0x34] = hw->blue_lamp_on / 256;
				a_bRegs[0x35] = hw->blue_lamp_on & 0xFF;
				a_bRegs[0x36] = hw->blue_lamp_off / 256;
				a_bRegs[0x37] = hw->blue_lamp_off & 0xFF;
			}
			
			sanei_lm983x_write( dev->fd, 0x29,
			                    &a_bRegs[0x29], 0x37-0x29+1, SANE_TRUE );
		}
	}

	if( usb_GetLampStatus(dev))
		usb_LedOn( dev, SANE_TRUE );
	else
		usb_LedOn( dev, SANE_FALSE );

	return SANE_TRUE;
}

/** Function to preset the registers for the specific device, which
 * should never change during the whole operation
 * Affected registers:<br>
 * 0x0b - 0x0e - Sensor settings - directly from the HWDef<br>
 * 0x0f - 0x18 - Sensor Configuration - directly from the HwDef<br>
 * 0x1a - 0x1b - Stepper Phase Correction<br>
 * 0x20 - 0x21 - Line End<br>
 * 0x21 - 0x22 - Data Pixel start<br>
 * 0x23 - 0x24 - Data Pixel end<br>
 * 0x45        - Stepper Motor Mode<br>
 * 0x4c - 0x4d - Full Steps to Scan after PAPER SENSE 2 trips<br>
 * 0x50        - Steps to reverse when buffer is full<br>
 * 0x51        - Acceleration Profile<br>
 * 0x54 - 0x5e - Motor Settings, Paper-Sense Settings and Misc I/O<br>
 *
 * @param dev    - pointer to our device structure,
 *                 it should contain all we need
 * @return - Nothing
 */
static void usb_ResetRegisters( pPlustek_Device dev )
{
	int linend;

	pHWDef hw = &dev->usbDev.HwSetting;

	DBG( _DBG_INFO, "RESETTING REGISTERS(%i)\n", dev->initialized );
	memset( a_bRegs, 0, sizeof(a_bRegs));

	memcpy( a_bRegs+0x0b, &hw->bSensorConfiguration, 4 );
	memcpy( a_bRegs+0x0f, &hw->bReg_0x0f_Color, 10 );
	a_bRegs[0x1a] = _HIBYTE( hw->StepperPhaseCorrection );
	a_bRegs[0x1b] = _LOBYTE( hw->StepperPhaseCorrection );
#if 0
	a_bRegs[0x1e] = _HIBYTE( hw->wActivePixelsStart );
	a_bRegs[0x1f] = _LOBYTE( hw->wActivePixelsStart );
#endif
	a_bRegs[0x20] = _HIBYTE( hw->wLineEnd );
	a_bRegs[0x21] = _LOBYTE( hw->wLineEnd );

	a_bRegs[0x22] = _HIBYTE( hw->bOpticBlackStart );
	a_bRegs[0x22] = _LOBYTE( hw->bOpticBlackStart );

	linend = hw->bOpticBlackStart + hw->wLineEnd;
	if( linend < (hw->wLineEnd-20))
		linend = hw->wLineEnd-20;

	a_bRegs[0x24] = _HIBYTE( linend );
	a_bRegs[0x25] = _LOBYTE( linend );

	a_bRegs[0x2a] = _HIBYTE( hw->wGreenPWMDutyCycleHigh );
	a_bRegs[0x2b] = _LOBYTE( hw->wGreenPWMDutyCycleHigh );

	a_bRegs[0x45] = hw->bReg_0x45;
	a_bRegs[0x4c] = _HIBYTE( hw->wStepsAfterPaperSensor2 );
	a_bRegs[0x4d] = _LOBYTE( hw->wStepsAfterPaperSensor2 );
	a_bRegs[0x50] = hw->bStepsToReverse;
	a_bRegs[0x51] = hw->bReg_0x51;

	/* if already initialized, we ignore the MISC I/O settings as
     * they are used to determine the current lamp settings...
     */
	if( dev->initialized >= 0 ) {

		DBG( _DBG_INFO2, "USING MISC I/O settings\n" );
		memcpy( a_bRegs+0x54, &hw->bReg_0x54, 0x58 - 0x54 + 1 );
		a_bRegs[0x5c] = hw->bReg_0x5c;
		a_bRegs[0x5d] = hw->bReg_0x5d;
		a_bRegs[0x5e] = hw->bReg_0x5e;
		sanei_lm983x_read( dev->fd, 0x59, &a_bRegs[0x59], 3, SANE_TRUE );
	} else {

		DBG( _DBG_INFO2, "SETTING THE MISC I/Os\n" );
		memcpy( a_bRegs+0x54, &hw->bReg_0x54, 0x5e - 0x54 + 1 );
		sanei_lm983x_write( dev->fd, 0x59, &a_bRegs[0x59], 3, SANE_TRUE );
	}
	DBG( _DBG_INFO, "MISC I/O after RESET: 0x%02x, 0x%02x, 0x%02x\n",
								a_bRegs[0x59], a_bRegs[0x5a], a_bRegs[0x5b] );
}

/** usb_ModuleStatus
 */
static SANE_Bool usb_ModuleStatus( pPlustek_Device dev )
{
	u_char value;
	pHWDef hw = &dev->usbDev.HwSetting;

/* HEINER: Maybe needed to avoid recalibration!!! */
#if 0
	if( dev->scanning.fCalibrated )
		return SANE_TRUE;
#endif

	_UIO( usbio_ReadReg( dev->fd, 2, &value ));

	if( value & 1 )	{

		_UIO( usbio_ReadReg( dev->fd, 0x7, &value));

		if( value ) {

			usbio_WriteReg( dev->fd, 0x07, 0 );
			usbio_WriteReg( dev->fd, 0x07, 0x20 );
			usbio_WriteReg( dev->fd, 0x07, 0 );
			
			sanei_lm983x_write( dev->fd, 0x58,
								&hw->bReg_0x58, 0x5b-0x58+1, SANE_TRUE );
			usbio_ReadReg( dev->fd, 2, &value );
			usbio_ReadReg( dev->fd, 2, &value );
		}

		usb_MotorOn( dev->fd, SANE_FALSE );
		return SANE_TRUE;
	}

	_UIO( usbio_ReadReg( dev->fd, 0x7, &value ));

	if( !(value & 2))
		usb_ModuleToHome( dev, SANE_FALSE );

	return SANE_FALSE;
}

/**
 */
static void usb_LampSwitch( Plustek_Device *dev, SANE_Bool sw )
{
	int handle = -1;

	if( -1 == dev->fd ) {

		if( SANE_STATUS_GOOD == sanei_usb_open(dev->sane.name, &handle)) {
			dev->fd = handle;
		}
	}

	/* needs to be recalibrated */
	dev->scanning.fCalibrated = SANE_FALSE;

	if( -1 != dev->fd )
		usb_LampOn( dev, sw, SANE_FALSE );

	if( -1 != handle ) {
		dev->fd = -1;
		sanei_usb_close( handle );
	}
}


/* HEINER: replace!!! */
static pPlustek_Device dev_xxx = NULL;

/** ISR to switch lamp off after time has elapsed
 */
static void usb_LampTimerIrq( int sig )
{
	if( NULL == dev_xxx )
		return;                             

	_VAR_NOT_USED( sig );
	DBG( _DBG_INFO, "LAMP OFF!!!\n" );

	usb_LampSwitch( dev_xxx, SANE_FALSE );
}

/** usb_StartLampTimer
 */
static void usb_StartLampTimer( pPlustek_Device dev )
{
	sigset_t         block, pause_mask;
	struct sigaction s;
#ifdef HAVE_SETITIMER
	struct itimerval interval;
#endif
	/* block SIGALRM */
	sigemptyset( &block );
	sigaddset  ( &block, SIGALRM );
	sigprocmask( SIG_BLOCK, &block, &pause_mask );

	/* setup handler */
	sigemptyset( &s.sa_mask );
	sigaddset  ( &s.sa_mask, SIGALRM );
	s.sa_flags   = 0;
	s.sa_handler = usb_LampTimerIrq;

	if( sigaction( SIGALRM, &s, NULL ) < 0 )
		DBG( _DBG_ERROR, "Can't setup timer-irq handler\n" );

	sigprocmask( SIG_UNBLOCK, &block, &pause_mask );

#ifdef HAVE_SETITIMER
	/* define a one-shot timer */
	interval.it_value.tv_usec    = 0;
	interval.it_value.tv_sec     = dev->usbDev.dwLampOnPeriod;
	interval.it_interval.tv_usec = 0;
	interval.it_interval.tv_sec  = 0;

	if( 0 != dev->usbDev.dwLampOnPeriod ) {
		dev_xxx = dev;
		setitimer( ITIMER_REAL, &interval, &dev->saveSettings );
		DBG( _DBG_INFO, "Lamp-Timer started (using ITIMER)\n" );
	}
#else
	if( 0 != dev->usbDev.dwLampOnPeriod ) {
		dev_xxx = dev;
		alarm( dev->usbDev.dwLampOnPeriod );
		DBG( _DBG_INFO, "Lamp-Timer started (using ALARM)\n" );
	}
#endif
}

/** usb_StopLampTimer
 */
static void usb_StopLampTimer( pPlustek_Device dev )
{
	sigset_t block, pause_mask;

	/* block SIGALRM */
	sigemptyset( &block );
	sigaddset  ( &block, SIGALRM );
	sigprocmask( SIG_BLOCK, &block, &pause_mask );

	dev_xxx = NULL;

#ifdef HAVE_SETITIMER
	if( 0 != dev->usbDev.dwLampOnPeriod )
		setitimer( ITIMER_REAL, &dev->saveSettings, NULL );
#else
	_VAR_NOT_USED( dev );
	alarm( 0 );
#endif
	DBG( _DBG_INFO, "Lamp-Timer stopped\n" );
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
 * wait until warmup has been done
 */
static SANE_Bool usb_Wait4Warmup( pPlustek_Device dev )
{
	u_long         dw;
	struct timeval t;

	pHWDef hw = &dev->usbDev.HwSetting;

	if( hw->bReg_0x26 & _ONE_CH_COLOR ) {
		DBG(_DBG_INFO,"Warmup: skipped for CIS devices\n" );
		return SANE_TRUE;
	}

	/*
	 * wait until warmup period has been elapsed
	 */
	gettimeofday( &t, NULL);
	dw = t.tv_sec - dev->usbDev.dwTicksLampOn;
	if( dw < dev->usbDev.dwWarmup )
		DBG(_DBG_INFO,"Warmup: Waiting %lu seconds\n",dev->usbDev.dwWarmup );

	do {

		gettimeofday( &t, NULL);

		dw = t.tv_sec - dev->usbDev.dwTicksLampOn;

		if( usb_IsEscPressed()) {
			return SANE_FALSE;
		}

	} while( dw < dev->usbDev.dwWarmup );

	return SANE_TRUE;
}
/* END PLUSTEK-USBHW.C ......................................................*/
