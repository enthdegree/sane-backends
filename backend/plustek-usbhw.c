/*.............................................................................
 * Project : SANE library for Plustek USB flatbed scanners.
 *.............................................................................
 * File:	 plustek-usbhw.c
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
#include <sys/time.h>

#define DEV_LampReflection      1
#define DEV_LampTPA             2
#define DEV_LampAll             3
#define DEV_LampPositive        4
#define DEV_LampNegative        5

/* HEINER: check the tick counts 'cause 1 tick on NT is about 10ms */

static u_long dwCrystalFrequency = 48000000UL;

static Bool   fModuleFirstHome;  /* HEINER: this has to be initialized */
static Bool   fLastScanIsAdf;
static u_char a_bRegs[0x80];

/*.............................................................................
 *
 */
static Bool usb_MotorOn( int handle, Bool fOn )
{
	if( fOn )
		a_bRegs[0x45] |= 0x10;
	else
		a_bRegs[0x45] &= ~0x10;

	usbio_WriteReg( handle, 0x45, a_bRegs[0x45] );

	return SANE_TRUE;
}

/*................................................................b.............
 *
 */
static Bool usb_IsScannerReady( pPlustek_Device dev )
{
	u_char value;

 	_UIO( usbio_ReadReg( dev->fd, 7, &value));

	if( value == 0 )
		_UIO( usbio_ResetLM983x( dev ));

	if(value == 0 || value >= 0x20) {

		if( usbio_WriteReg( dev->fd, 0x07, 0 )) {
			return SANE_TRUE;
		}
	}

	DBG( _DBG_ERROR, "Scanner not ready!!!\n" );
	return SANE_FALSE;
}

/*.............................................................................
 *
 */
static Bool usb_SensorAdf( int handle )
{
	u_char value;

	usbio_ReadReg( handle, 0x02, &value );

	return (value & 0x20);
}

/*.............................................................................
 *
 */
static Bool usb_SensorPaper( int handle )
{
	u_char value;

	usbio_ReadReg( handle, 0x02, &value );

	return (value & 0x02);
}

/*.............................................................................
 * Home sensor always on when backward move.
 * dwStep is steps to move and based on 300 dpi, but
 * if the action is MOVE_Both, it becomes the times
 * to repeatly move the module around the scanner and
 * 0 means forever.
 */
static Bool usb_ModuleMove(pPlustek_Device dev, u_char bAction, u_long dwStep)
{
	SANE_Status res;
	u_char      bReg2, reg7;
	u_short     wFastFeedStepSize;
	double      dMaxMoveSpeed;
	pHWDef      hw = &dev->usbDev.HwSetting;

	/* Check if LM9831 is ready for setting command */
	if( bAction != MOVE_ToPaperSensor   &&
		bAction != MOVE_EjectAllPapers  &&
		bAction != MOVE_SkipPaperSensor &&
		bAction != MOVE_ToShading       && !dwStep) {

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
		else
			dMaxMoveSpeed = hw->dMaxMoveSpeed - 0.5;
	} else {

		dMaxMoveSpeed = hw->dMaxMoveSpeed;
	}

	wFastFeedStepSize = (u_short)(dwCrystalFrequency /
						(6UL * 8UL * 1 * dMaxMoveSpeed * 4 * hw->wMotorDpi));

	a_bRegs[0x48] = (u_char)(wFastFeedStepSize >> 8);
	a_bRegs[0x49] = (u_char)(wFastFeedStepSize & 0xFF);

	dwStep = dwStep * hw->wMotorDpi / 300UL;
	a_bRegs[0x4a] = _HIBYTE(_LOWORD(dwStep));
	a_bRegs[0x4b] = _LOBYTE(_LOWORD(dwStep));

	a_bRegs[0x45] |= 0x10;

	/* The setting for chassis moving is:
	 * MCLK divider = 6, 8 bits/pixel, HDPI divider = 12,
	 * no integration time adjustment and 1 channel grayscale
	 */

	/* MCLK divider = 6 */
	if( !usbio_WriteReg(dev->fd, 0x08, 0x0A))
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

	_UIO(sanei_lm9831_write(dev->fd, 0x48, &a_bRegs[0x48], 2, SANE_TRUE));
	_UIO(sanei_lm9831_write(dev->fd, 0x4A, &a_bRegs[0x4A], 2, SANE_TRUE));

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

/*.............................................................................
 *
 */
static Bool usb_ModuleToHome( pPlustek_Device dev, Bool fWait )
{
	u_char    value;
	pDCapsDef scaps = &dev->usbDev.Caps;
	pHWDef    hw    = &dev->usbDev.HwSetting;

	/* Check if LM9831 is ready for setting command */
	usbio_WriteReg( dev->fd, 0x58, hw->bReg_0x58 );
	usbio_ReadReg ( dev->fd, 2, &value );

	/* check current position... */
	_UIO(usbio_ReadReg( dev->fd, 2, &value ));

	if( value & 1 ) {
		fModuleFirstHome = SANE_FALSE;
		return SANE_TRUE;
	}

	_UIO( usbio_ReadReg( dev->fd, 0x07, &value ));

	if( fModuleFirstHome ) {
		fModuleFirstHome = SANE_FALSE;
		if( hw->ScannerModel != MODEL_Tokyo600 )
			usb_ModuleMove( dev, MOVE_Forward, hw->wMotorDpi / 2);
	}

	/* if not homing, do it... */
	if( value != 2 ) {

		u_short wFastFeedStepSize;

		if( hw->ScannerModel == MODEL_Tokyo600 ) {
			usbio_WriteReg( dev->fd, 0x07, 0 );
		} else 	{
				
			_UIO( usbio_ResetLM983x( dev ));

			usleep(200*1000);
		}
		
		/* Compute fast feed step size, use equation 3 and equation 8 */
		wFastFeedStepSize = (u_short)(dwCrystalFrequency / (6UL * 8UL * 1 *
									  hw->dMaxMotorSpeed * 4 * hw->wMotorDpi));
		a_bRegs[0x48] = (u_char)(wFastFeedStepSize >> 8);
		a_bRegs[0x49] = (u_char)(wFastFeedStepSize & 0xFF);
		a_bRegs[0x4a] = 0;
		a_bRegs[0x4b] = 0;

		if( scaps->OpticDpi.x == 1200 || scaps->bPCB == 2) {
			switch( hw->ScannerModel ) {
			case MODEL_KaoHsiung:
			case MODEL_HuaLien:
			default:
				a_bRegs[0x56] = 1;
				a_bRegs[0x57] = 63;
				break;
			}
		} else { /* if(Device.Caps.OpticDpi.x == 600) */

			switch( hw->ScannerModel ) {

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

		a_bRegs[0x45] |= 0x10;

		/* The setting for chassis moving is:
		 * MCLK divider = 6, 8 bits/pixel, HDPI divider = 12,
		 * no integration time adjustment and 1 channel grayscale
	 	 */
		/* MCLK divider = 6 */
		if( !usbio_WriteReg(dev->fd, 0x08, 0x0A))
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
		
		_UIO(sanei_lm9831_write(dev->fd, 0x48, &a_bRegs[0x48], 4, SANE_TRUE));
		_UIO(sanei_lm9831_write(dev->fd, 0x56, &a_bRegs[0x56], 3, SANE_TRUE));

		if( !usbio_WriteReg(dev->fd, 0x45, a_bRegs[0x45]))
			return SANE_FALSE;

		usbio_WriteReg(dev->fd, 0x0a, 0);

		if( hw->ScannerModel == MODEL_HuaLien && scaps->OpticDpi.x == 600 )
			usleep(100 * 1000);

		if( !usbio_WriteReg(dev->fd, 0x07, 2))
			return SANE_FALSE;
		
#if 0
		if(Device.HwSetting.ScannerModel == MODEL_Tokyo600)
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

/*.............................................................................
 *
 */
static Bool usb_MotorSelect( pPlustek_Device dev, Bool fADF )
{
	pDCapsDef sCaps = &dev->usbDev.Caps;
	pHWDef    hw    = &dev->usbDev.HwSetting;

	if( fADF ) {

		if( sCaps->bCCD == 4 ) {
			hw->wMotorDpi = 300;
			hw->dMaxMotorSpeed = 1.5;
			hw->dMaxMoveSpeed = 1.5;
			sCaps->OpticDpi.y = 600;
		}
		a_bRegs[0x5b] |= 0x80;
	} else {
		if( sCaps->bCCD == 4 ) {
			hw->wMotorDpi = 600;
			hw->dMaxMotorSpeed = 1.1;
			hw->dMaxMoveSpeed = 0.9;
			sCaps->OpticDpi.y = 1200;
		}
		a_bRegs[0x5b] &= ~0x80;
	}

	/* To stop the motor moving */
	usbio_WriteReg( dev->fd, 0x07, 0 );		
	usleep(10 * 1000);

	usbio_WriteReg( dev->fd, 0x5b, a_bRegs[0x5b] );
	return SANE_TRUE;
}

/*.............................................................................
 *
 */
static int usb_GetLampStatus( pPlustek_Device dev )
{
	int    iLampStatus = 0;
	pHWDef hw          = &dev->usbDev.HwSetting;

	if( NULL == hw ) {
		DBG( _DBG_ERROR, "NULL-Pointer detected: usb_GetLampStatus()\n" );
		return -1;
	}	
	
	sanei_lm9831_read( dev->fd, 0x29, &a_bRegs[0x29], 0x37-0x29+1, SANE_TRUE );

	if((a_bRegs[0x29] & 3) == 1) {

		if((a_bRegs[0x2e] * 256 + a_bRegs[0x2f]) > hw->wLineEnd )
			iLampStatus |= DEV_LampReflection;

		if((a_bRegs[0x36] * 256 + a_bRegs[0x37]) > hw->wLineEnd )
			iLampStatus |= DEV_LampTPA;
	}

	return iLampStatus;
}

/*.............................................................................
 *
 */
static void usb_LedOn(  pPlustek_Device dev, Bool fOn )
{
	u_char value;

	if( dev->usbDev.HwSetting.ScannerModel != MODEL_HuaLien )
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

/*.............................................................................
 *
 */
static Bool usb_LampOn( pPlustek_Device dev,
						Bool fOn, int iLampID, Bool fResetTimer )
{
    pScanDef  scanning    = &dev->scanning;
	int       iLampStatus = usb_GetLampStatus( dev );

	if( NULL == scanning ) {
		DBG( _DBG_ERROR, "NULL-Pointer detected: usb_LampOn()\n" );
		return SANE_FALSE;
	}
	
	if( -1 == iLampID ) {

		switch( scanning->sParam.bSource ) {

		case SOURCE_Reflection:
		case SOURCE_ADF:
			iLampID = DEV_LampReflection;
			break;

		case SOURCE_Transparency:
		case SOURCE_Negative:
			iLampID = DEV_LampTPA;
			break;
		}
	}

	if( fOn ) {

		if(iLampStatus != iLampID) {

			memset( &a_bRegs[0x29], 0, (0x37-0x29+1));
			a_bRegs[0x29] = 1;	/* mode 1 */

			if( iLampID == DEV_LampReflection ) {
				a_bRegs[0x2e] = 16383 / 256;
				a_bRegs[0x2f] = 16383 % 256;
			}

			if( iLampID == DEV_LampTPA ) {
				a_bRegs[0x36] = 16383 / 256;
				a_bRegs[0x37] = 16383 % 256;
			}
			
			sanei_lm9831_write( dev->fd, 0x29,
								&a_bRegs[0x29], 0x37-0x29+1, SANE_TRUE );
#if 0
			if(!(iLampStatus & iLampID) && fResetTimer)
				Device.dwTicksLampOn = GetTickCount();
#else
			_VAR_NOT_USED(fResetTimer);
#endif
		}
	} else {

		int iStatusChange = iLampStatus & ~iLampID;
		
		if( iStatusChange != iLampStatus ) {

			memset( &a_bRegs[0x29], 0, 0x37-0x29+1 );
			a_bRegs[0x29] = 1;	/* mode 1 */

			if( iStatusChange & DEV_LampReflection ) {
				a_bRegs[0x2e] = 16383 / 256;
				a_bRegs[0x2f] = 16383 % 256;
			}

			if( iStatusChange & DEV_LampTPA ) {
				a_bRegs[0x36] = 16383 / 256;
				a_bRegs[0x37] = 16383 % 256;
			}
			
			sanei_lm9831_write( dev->fd, 0x29,
								&a_bRegs[0x29], 0x37-0x29+1, SANE_TRUE );
		}
	}

	if( usb_GetLampStatus(dev))
		usb_LedOn( dev, SANE_TRUE);
	else
		usb_LedOn( dev, SANE_FALSE);

	return SANE_TRUE;
}

/*.............................................................................
 *
 */
static void usb_ResetRegisters(  pPlustek_Device dev )
{
	pHWDef hw = &dev->usbDev.HwSetting;

	memset( a_bRegs, 0, sizeof(a_bRegs));

	memcpy( a_bRegs+0x0b, &hw->bSensorConfiguration, 4 );
	memcpy( a_bRegs+0x0f, &hw->bReg_0x0f_Color, 10 );
	a_bRegs[0x1a] = _HIBYTE( hw->StepperPhaseCorrection );
	a_bRegs[0x1b] = _LOBYTE( hw->StepperPhaseCorrection );
	a_bRegs[0x20] = _HIBYTE( hw->wLineEnd );
	a_bRegs[0x21] = _LOBYTE( hw->wLineEnd );
	a_bRegs[0x45] = hw->bReg_0x45;
	a_bRegs[0x4c] = _HIBYTE( hw->wStepsAfterPaperSensor2 );
	a_bRegs[0x4d] = _LOBYTE( hw->wStepsAfterPaperSensor2 );
	a_bRegs[0x51] = hw->bReg_0x51;

	memcpy( a_bRegs+0x54, &hw->bReg_0x54, 0x5e - 0x54 + 1 );
}

/*.............................................................................
 *
 */
static SANE_Bool usb_ModuleStatus( pPlustek_Device dev )
{
	u_char value;
	pHWDef hw = &dev->usbDev.HwSetting;

#if 0
	if(Calibration.fCalibrated)
		return SANE_TRUE;
#endif

	_UIO( usbio_ReadReg( dev->fd, 2, &value ));

	if( value & 1 )	{

		_UIO( usbio_ReadReg( dev->fd, 0x7, &value));

		if( value ) {

			usbio_WriteReg( dev->fd, 0x07, 0 );
			usbio_WriteReg( dev->fd, 0x07, 0x20 );
			usbio_WriteReg( dev->fd, 0x07, 0 );
			
			sanei_lm9831_write( dev->fd, 0x58,
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

/*****************************************************************************/

#if 0

BOOL CHardware::LampSetKeepOnInterval (BYTE bMinutes) // 0: Disable timer proc
{
	Device.bLampOnPeriod = bMinutes;
	Device.dwSecondsLampOn = (DWORD) bMinutes * 60UL;
	Registry.SetLampTurnOffTime (Device.bLampOnPeriod);
	
	return TRUE;
}
		
BOOL CHardware::Reset ()
{
	Scan.Reset();
	ResetRegisters();
	return TRUE;
}

#endif

/* END PLUSTEK-USBHW.C ......................................................*/
