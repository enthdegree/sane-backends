/* sane - Scanner Access Now Easy.
   Copyright (C) 2000-2003 Jochen Eisinger <jochen.eisinger@gmx.net>
   This file is part of the SANE package.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA.

   As a special exception, the authors of SANE give permission for
   additional uses of the libraries contained in this release of SANE.

   The exception is that, if you link a SANE library with other files
   to produce an executable, this does not by itself cause the
   resulting executable to be covered by the GNU General Public
   License.  Your use of that executable is in no way restricted on
   account of linking the SANE library code into it.

   This exception does not, however, invalidate any other reasons why
   the executable file might be covered by the GNU General Public
   License.

   If you submit changes to SANE to the maintainers to be included in
   a subsequent release, you agree by submitting the changes that
   those changes may be distributed with this exception intact.

   If you write modifications of your own for SANE, it is your choice
   whether to permit this exception to apply to your modifications.
   If you do not wish that, delete this exception notice.  
   
   This file implements the hardware driver scanners using a 300dpi CCD */
   
#include "../include/sane/config.h"

#if defined(HAVE_STDLIB_H)
# include <stdlib.h>
#endif
#include <stdio.h>
#include <ctype.h>
#if defined(HAVE_STRING_H)
# include <string.h>
#elif defined(HAVE_STRINGS_H)
# include <strings.h>
#endif
#include <time.h>
#if defined(HAVE_UNISTD_H)
# include <unistd.h>
#endif

#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"
#include "../include/sane/sanei_pa4s2.h"
#define DEBUG_DECLARE_ONLY
#include "mustek_pp.h"
#include "mustek_pp_decl.h"
#include "mustek_pp_ccd300.h"

/* A4SII300 driver */

unsigned char ChannelCode[] = { 0x82, 0x42, 0xc2 };
unsigned char ChannelCode_1015[] = { 0x80, 0x40, 0xc0 };
unsigned char MotorPhase[] = { 0x09, 0x0c, 0x06, 0x03 };
unsigned char MotorPhase_HalfStep[]
  = { 0x01, 0x09, 0x08, 0x0c, 0x04, 0x06, 0x02, 0x03 };

SANE_Status
ParRead (mustek_pp_ccd300_priv * dev)
{
  unsigned char ASICId;
  Switch_To_Scanner (dev);
  ASICId = ReadID1 (0, dev);
  Switch_To_Printer (dev);

  if (ASICId != 0xA8 && ASICId != 0xA5)
    return SANE_STATUS_INVAL;
  else
    return SANE_STATUS_GOOD;
}

void
Switch_To_Scanner (mustek_pp_ccd300_priv * dev)
{
  sanei_pa4s2_enable (dev->fd, SANE_TRUE);
}

void
Switch_To_Printer (mustek_pp_ccd300_priv * dev)
{
  sanei_pa4s2_enable (dev->fd, SANE_FALSE);
}

void
LampPowerOn (mustek_pp_ccd300_priv * dev)
{
  LampOnOP (dev);
}

void
LampOnOP (mustek_pp_ccd300_priv * dev)
{
  int btLoop;
  dev->m_wMotorStepNo = 1;
  SetLed_OnOff (dev);
  OutChar (6, 0xC3, dev);
  for (btLoop = 0; btLoop < 3; btLoop++)
    {
      OutChar (6, 0x47, dev);
      OutChar (6, 0x77, dev);
    }

}

void
LampPowerOff (mustek_pp_ccd300_priv * dev)
{
  LampOffOP (dev);
}

void
LampOffOP (mustek_pp_ccd300_priv * dev)
{
  int btLoop;
  dev->m_wMotorStepNo = 0;
  SetLed_OnOff (dev);
  OutChar (6, 0xC3, dev);
  for (btLoop = 0; btLoop < 3; btLoop++)
    {
      OutChar (6, 0x57, dev);
      OutChar (6, 0x77, dev);
    }
}

void
SetCCDInfo (mustek_pp_ccd300_priv * dev)
{
  dev->CCD_Type = CheckCCD_Kind (dev);
  SetCCDDPI (dev);

  if (dev->IsGa1013)
    {
      SetCCDMode (dev);
      SetCCDInvert (dev);
    }
  else
    {
      dev->ImageCtrl = 0;
      dev->DataValue = dev->ImageCtrl;
      SetCCDMode_1015 (dev);
      SetCCDInvert_1015 (dev);
      OutChar (6, 0xf6, dev);
      OutChar (6, 0x23, dev);
      OutChar (5, 0x00, dev);
      OutChar (6, 0x43, dev);

      if (dev->CCD_Type == 1)
	OutChar (5, 0x6b, dev);

      else if (dev->CCD_Type == 4)
	OutChar (5, 0x9f, dev);

      else

	OutChar (5, 0x92, dev);
      OutChar (6, 0x03, dev);
    }
  OutChar (6, 0x37, dev);
  ClearBankCount (dev);
  OutChar (6, 0x27, dev);
  OutChar (6, 0x67, dev);
  OutChar (6, 0x17, dev);
  OutChar (6, 0x77, dev);
  SetDummyCount (dev);
  OutChar (6, 0x81, dev);
  if (!dev->IsGa1013)
    {
      if (dev->CCD_Type == 1)
	OutChar (5, 0x90, dev);
      else if (dev->CCD_Type == 4)
	OutChar (5, 0xA8, dev);
      else
	OutChar (5, 0x8A, dev);
    }
  else
    OutChar (5, 0x70, dev);
  OutChar (6, 0x01, dev);
  SetScanByte (dev);
}

void
SetCCDDPI (mustek_pp_ccd300_priv * dev)
{
  unsigned char ucRes = 0x00;
  OutChar (6, 0x80, dev);
  switch (dev->ASICResolution)
    {
    case 100:

      if (dev->CCD_Type == 1)
	ucRes = 0x01;
      else

	ucRes = 0x00;
      break;
    case 200:

      if (dev->CCD_Type == 1)
	ucRes = 0x11;
      else

	ucRes = 0x10;
      break;
    case 300:

      if (dev->CCD_Type == 1)
	ucRes = 0x21;
      else

	ucRes = 0x20;
      break;
    }
  OutChar (5, ucRes, dev);
  OutChar (6, 0x00, dev);
}

void
SetCCDMode (mustek_pp_ccd300_priv * dev)
{
  unsigned char ucMode = 0x00;
  switch (dev->ScanMode)
    {
    case 0:
      ucMode = 0x15;
      dev->RGBChannel = 1;
      break;
    case 1:
      dev->RGBChannel = 1;
      ucMode = 0x05;
      break;
    case 2:
      dev->RGBChannel = 0;
      ucMode = 0x05;
      break;
    }
  OutChar (6, ucMode, dev);
  SetCCD_Channel (dev);
}

void
SetCCDMode_1015 (mustek_pp_ccd300_priv * dev)
{
  switch (dev->ScanMode)
    {
    case 0:
      dev->ImageCtrl = 0x24;
      dev->RGBChannel = 1;
      break;
    case 1:
      dev->ImageCtrl = 0x04;
      dev->RGBChannel = 1;
      break;
    case 2:
      dev->ImageCtrl = 0x04;
      dev->RGBChannel = 0;
      break;
    }
  dev->DataValue = dev->ImageCtrl;
  SetCCD_Channel (dev);
}

void
SetCCDInvert_1015 (mustek_pp_ccd300_priv * dev)
{
  dev->ImageCtrl &= 0xe4;
  dev->DataValue = dev->ImageCtrl;
  if (dev->PixelFlavor == 1)
    dev->ImageCtrl |= 0x14;
  dev->DataValue = dev->ImageCtrl;
  OutChar (6, dev->ImageCtrl, dev);
}

void
SetPixelAverage (mustek_pp_ccd300_priv * dev)
{
  OutChar (6, 0x15, dev);
}

void
SetCCD_Channel_WriteSRAM (mustek_pp_ccd300_priv * dev)
{
  unsigned char ucChannelCode;
  ucChannelCode = ChannelCode[dev->RGBChannel];
  ucChannelCode |= 0x22;
  OutChar (6, ucChannelCode, dev);
}

void
SetCCD_Channel (mustek_pp_ccd300_priv * dev)
{
  unsigned char ucChannelCode;
  if (dev->IsGa1013)
    {
      ucChannelCode = ChannelCode[dev->RGBChannel];
      OutChar (6, ucChannelCode, dev);
    }
  else
    {
      dev->ImageCtrl &= 0x34;
      dev->DataValue = dev->ImageCtrl;
      ucChannelCode = ChannelCode_1015[dev->RGBChannel];
      dev->DataValue = ucChannelCode;
      dev->ImageCtrl |= ucChannelCode;
      dev->DataValue = dev->ImageCtrl;
      OutChar (6, dev->ImageCtrl, dev);
    }
}

void
SetCCDInvert (mustek_pp_ccd300_priv * dev)
{
  if (dev->PixelFlavor == 1)
    OutChar (6, 0x14, dev);
  else
    OutChar (6, 0x04, dev);
}

void
ClearBankCount (mustek_pp_ccd300_priv * dev)
{
  OutChar (6, 0x07, dev);
}

void
SetDummyCount (mustek_pp_ccd300_priv * dev)
{
  unsigned char ucTotalDummy;
  unsigned int wTotalSkipCount;
  OutChar (6, 0x41, dev);
  wTotalSkipCount = dev->SkipCount + dev->Skip_ImageBytes;
  dev->m_wByteCount = dev->Skip_ImageBytes;
  if (dev->IsGa1013)
    {
      ucTotalDummy = (unsigned char) (wTotalSkipCount / 16);
      dev->DataValue = ucTotalDummy;
      ucTotalDummy = ucTotalDummy + 2;
      dev->DataValue = ucTotalDummy;
      dev->SkipCount = wTotalSkipCount % 16;
      dev->m_wByteCount = dev->SkipCount;
      OutChar (5, ucTotalDummy, dev);

    }
  else
    {
      ucTotalDummy = (unsigned char) (wTotalSkipCount / 32);
      ucTotalDummy = (ucTotalDummy + 2) / 2;
      dev->SkipCount = wTotalSkipCount % 32;
      OutChar (5, ucTotalDummy, dev);

    }
  OutChar (6, 0x01, dev);
}

void
SetScanByte (mustek_pp_ccd300_priv * dev)
{
  unsigned int wASICScanBytes, wTmp;
  unsigned char ucSetRegBytes;
  dev->Scan_Bytes = CalScanParameter (dev->ASICResolution, dev->Scan_Bytes);
  dev->SkipCount = CalScanParameter (dev->ASICResolution, dev->SkipCount);
  wASICScanBytes = dev->Scan_Bytes + dev->SkipCount;
  OutChar (6, 0x11, dev);
  wTmp = wASICScanBytes;
  ucSetRegBytes = (unsigned char) (wTmp >> 8);
  OutChar (5, ucSetRegBytes, dev);
  OutChar (6, 0x21, dev);
  ucSetRegBytes = (unsigned char) wASICScanBytes;
  OutChar (5, ucSetRegBytes, dev);
  OutChar (6, 0x01, dev);
}

void
SetRGBRefVoltage (mustek_pp_ccd300_priv * dev)
{
  dev->CCD_Type = CheckCCD_Kind (dev);

  if (dev->CCD_Type == 4)
    {
      OutChar (6, 0x10, dev);

      OutChar (5, 180, dev);
      OutChar (6, 0x20, dev);

      OutChar (5, 180, dev);
      OutChar (6, 0x40, dev);

      OutChar (5, 180, dev);
    }

  else if (dev->CCD_Type == 1)
    {
      OutChar (6, 0x10, dev);
      OutChar (5, 230, dev);
      OutChar (6, 0x20, dev);
      OutChar (5, 180, dev);
      OutChar (6, 0x40, dev);
      OutChar (5, 190, dev);
    }

  else
    {
      OutChar (6, 0x10, dev);

      OutChar (5, 92, dev);
      OutChar (6, 0x20, dev);

      OutChar (5, 90, dev);
      OutChar (6, 0x40, dev);

      OutChar (5, 99, dev);
    }
  OutChar (6, 0x00, dev);
}

void
SetLed_OnOff (mustek_pp_ccd300_priv * dev)
{
  unsigned int wModNo;
  wModNo = dev->m_wMotorStepNo % 5;
  if (wModNo == 0)
    OutChar (6, 0x03, dev);
  else
    OutChar (6, 0x13, dev);
}

void
OutChar (unsigned char RegNo,
	 unsigned char OutData, mustek_pp_ccd300_priv * dev)
{
  sanei_pa4s2_writebyte (dev->fd, RegNo, OutData);
}

unsigned char
Read_a_Byte (mustek_pp_ccd300_priv * dev, unsigned char RegNo)
{
  unsigned char Data;

  InChar_Begin_Dispatch (dev->PM, dev, RegNo);
  Data = InChar_Do_Dispatch (dev->PM, dev);
  InChar_End_Dispatch (dev->PM, dev);
  return (Data);
}

/*ARGSUSED*/
void
InChar_Begin_Dispatch (unsigned char Mode __UNUSED__, 
		       mustek_pp_ccd300_priv * dev, unsigned char RegNo)
{
  sanei_pa4s2_readbegin (dev->fd, RegNo);
}

/*ARGSUSED*/
unsigned char
InChar_Do_Dispatch (unsigned char Mode __UNUSED__, mustek_pp_ccd300_priv * dev)
{
  unsigned char val;
  sanei_pa4s2_readbyte (dev->fd, &val);
  return val;
}

/*ARGSUSED*/
void
InChar_End_Dispatch (unsigned char Mode __UNUSED__, 
		     mustek_pp_ccd300_priv * dev)
{
  sanei_pa4s2_readend (dev->fd);
}

unsigned char
Change_Mode (mustek_pp_ccd300_priv * dev)
{
  unsigned char ID;
  ID = ReadID1 (0, dev);
  if (ID == 0xA8)
    dev->IsGa1013 = 1;
  if (ID == 0xA5)
    dev->IsGa1013 = 0;
  dev->PM = 0;
  return ID;
}

unsigned char
ReadID1 (unsigned char Mode, mustek_pp_ccd300_priv * dev)
{
  unsigned char ID;
  InChar_Begin_Dispatch (Mode, dev, 0);
  ID = InChar_Do_Dispatch (Mode, dev);
  InChar_End_Dispatch (Mode, dev);
  return (ID);
}

void
CheckMotorSatus (mustek_pp_ccd300_priv * dev)
{
  unsigned char ucDataValue;
  while (1)
    {
      ucDataValue = Read_a_Byte (dev, 2);
      ucDataValue &= 0x08;
      if (ucDataValue == 0)
	break;
    }
}

void
CheckPIPStatus (mustek_pp_ccd300_priv * dev)
{
  unsigned char ucOldPM, ucDataValue;
  ucOldPM = dev->PM;
  dev->PM = 0;
  ucDataValue = Read_a_Byte (dev, 2);
  if (dev->IsGa1013)
    {
      ucDataValue &= 0x01;
    }
  else
    {
      ucDataValue >>= 1;
      ucDataValue ^= 0xff;
      ucDataValue &= 0x01;
    }
  dev->AtHomeSensor = (unsigned int) ucDataValue;
  dev->PM = ucOldPM;
}

unsigned char
GetBankCount (mustek_pp_ccd300_priv * dev)
{
  unsigned char ucDataValue;
  ucDataValue = Read_a_Byte (dev, 3);
  ucDataValue &= 0x07;
  return ucDataValue;
}

unsigned char
CheckCCDBit (mustek_pp_ccd300_priv * dev)
{
  unsigned char ucDataValue;
  ucDataValue = Read_a_Byte (dev, 2);
  ucDataValue &= 0x04;
  if (ucDataValue == 0)
    ucDataValue = 8;
  else
    ucDataValue = 10;
  return ucDataValue;
}

unsigned char
CheckCCD_Kind (mustek_pp_ccd300_priv * dev)
{
  unsigned char ucDataValue;
  ucDataValue = Read_a_Byte (dev, 2);
  if (dev->IsGa1013)
    {
      ucDataValue &= 0x04;
    }
  else
    {
      ucDataValue &= 0x05;
    }
  return ucDataValue;
}

void
WaitBankCountChange (mustek_pp_ccd300_priv * dev)
{
  unsigned char ucASICBCValue;

  while (1)
    {
      ucASICBCValue = GetBankCount (dev);
      if (ucASICBCValue == dev->bankcountValue)
	break;

    }
}

void
GetDeviceInfo (mustek_pp_ccd300_priv * dev)
{
  unsigned char ucID1;
  ucID1 = Change_Mode (dev);
  if (ucID1 != 0xA8 && ucID1 != 0xA5)
    {
      dev->PM = 0;
      ucID1 = ReadID1 (dev->PM, dev);
      if (ucID1 == 0xA8)
	dev->IsGa1013 = 1;
      if (ucID1 == 0xA5)
	dev->IsGa1013 = 0;
    }
  if (ucID1 != 0xA8 && ucID1 != 0xA5)
    dev->ErrorCode = 1;
  else
    {
      dev->ErrorCode = 0;
    }
  dev->ScanMode = 2;
  dev->PixelFlavor = 1;
  dev->scan_source = 0;
  dev->Frame_Left = 0;
  dev->Skip_ImageBytes = dev->Frame_Left;
  dev->Frame_Right = 1200;
  dev->Frame_Top = 0;
  dev->Frame_Bottom = 100;
  dev->ScanResolution = 300;
  dev->Scan_Bytes = dev->Total_lineBytes;
  SetCCDInfo (dev);
}

/*ARGSUSED*/
void
SetScanParameter (int DeviceObject __UNUSED__, int Irp __UNUSED__, 
		  mustek_pp_ccd300_priv * dev)
{
  unsigned char ucDataValue;
  dev->ErrorCode = 0;
  dev->Skip_ImageBytes = dev->Frame_Left;
  if (0 != dev->scan_source && 99 != dev->scan_source)
    dev->Skip_ImageBytes += 560;
  SetASICRes (dev);
  dev->Total_lineBytes = dev->Frame_Right - dev->Frame_Left;
  dev->Scan_Bytes = dev->Total_lineBytes;
  dev->Total_Scanlines = dev->Frame_Bottom - dev->Frame_Top;
  SetCCDInfo (dev);
  SetRGBRefVoltage (dev);
  ucDataValue = GetBankCount (dev);
  dev->bankcountValue = ucDataValue;
  if (ucDataValue != 0)
    dev->ErrorCode = 1;
}

/*ARGSUSED*/
void
GetScanParameter (int DeviceObject __UNUSED__, int Irp __UNUSED__, 
		  mustek_pp_ccd300_priv * dev)
{
  dev->Scan_Lines = CalScanParameter (dev->ScanResolution,
				      dev->Total_Scanlines);
  dev->Scan_Linebytes = CalScanParameter (dev->ScanResolution,
					  dev->Total_lineBytes);
}

/*ARGSUSED*/
void
A4StartScan (int DeviceObject __UNUSED__, int Irp __UNUSED__, 
	     mustek_pp_ccd300_priv * dev)
{
  dev->Total_Scanlines = dev->Scan_Lines;
  dev->CatchDataFirst = 1;
  dev->SkipCount = 0;
  dev->Catch_R_TmpC = 0;
  dev->Catch_B_TmpC = 0;
  dev->MotorPhaseStatus = 0;
  dev->m_wMotorStepNo = 0;
  IO_FindBlack_Data (dev);
}

/*ARGSUSED*/
void
A4StopScan (int DeviceObject __UNUSED__, int Irp __UNUSED__, 
	    mustek_pp_ccd300_priv * dev)
{
  dev->Catch_R_TmpC = 0;
  dev->Catch_B_TmpC = 0;
  dev->SkipCount = 0;
  dev->Skip_ImageBytes = 0;
  dev->m_wMotorStepNo = 0;
  PullCarriage_ToHome (dev);
  if (dev->IsGa1013)
    Motor_Off (dev);
}

/*ARGSUSED*/
void
A4CheckScanner_HomeSensor (int DeviceObject __UNUSED__, int Irp __UNUSED__,
			   mustek_pp_ccd300_priv * dev)
{
  CheckPIPStatus (dev);
}

/*ARGSUSED*/
void
A4CarriageTo_Home (int DeviceObject __UNUSED__, int Irp __UNUSED__,
		   mustek_pp_ccd300_priv * dev)
{
  LampOnOP (dev);
  PullCarriage_ToHome (dev);
}

void
PullCarriage_ToHome (mustek_pp_ccd300_priv * dev)
{
  if (dev->IsGa1013)
    {
      Motor_BackHome (dev);
    }
  else
    {
      dev->AtHomeSensor = 0;
      SetCCDInfo (dev);
      Asic1015_Motor_Ctrl (dev, 0xc3);
    }
}

void
Motor_BackHome (mustek_pp_ccd300_priv * dev)
{
  while (1)
    {
      CheckPIPStatus (dev);
      if (dev->AtHomeSensor == 0)
	break;
      Backward_onestep (dev);
      WaitBankCountChange (dev);
      ClearBankCount (dev);
    }
}

/*ARGSUSED*/
void
A4GetImage (int DeviceObject __UNUSED__, int Irp __UNUSED__, 
	    mustek_pp_ccd300_priv * dev)
{
  if (dev->Total_Scanlines == 0)
    {
      dev->txf_lines = dev->Catch_G_Count;
      return;
    }
  switch (dev->ScanMode)
    {
    case 2:
      if (dev->ASICResolution > 100)
	IO_GetColorData (dev);
      else
	IO_GetColorData_100 (dev);
      break;
    case 1:
    case 0:
      if (dev->ASICResolution > 100)
	IO_GetGrayData (dev);
      else
	IO_GetGrayData_100 (dev);
      break;
    }
  dev->txf_lines = dev->Catch_G_Count;
  dev->Catch_G_Count = 0;
}

void
SetASICRes (mustek_pp_ccd300_priv * dev)
{
  if (dev->ScanResolution > 100)
    {
      if (dev->ScanResolution > 200)
	{
	  dev->ASICResolution = 300;
	}
      else
	{
	  dev->ASICResolution = 200;
	}
    }
  else
    {
      dev->ASICResolution = 100;
    }
}

int
CalScanParameter (int wResolution, int wPar)
{
  switch (wResolution)
    {
    case 50:
      wPar = wPar / 6;
      break;
    case 100:
      wPar = wPar / 3;
      break;
    case 150:
      wPar = wPar / 2;
      break;
    case 200:
      wPar = wPar * 2 / 3;
      break;
    case 250:
      wPar = wPar * 5 / 6;
      break;
    default:
      break;
    }
  return wPar;
}

void
Forward_onestep (mustek_pp_ccd300_priv * dev)
{
  unsigned char ucPhase;
  dev->m_wMotorStepNo++;
  SetLed_OnOff (dev);
  if (dev->IsGa1013)
    {

      SetCCD_Channel_WriteSRAM (dev);

      ucPhase = MotorPhase[dev->MotorPhaseStatus];
      if (dev->MotorPhaseStatus >= 3)
	dev->MotorPhaseStatus = 0;
      else
	dev->MotorPhaseStatus++;
      OutChar (5, ucPhase, dev);

    }
  else
    {
      Asic1015_Motor_Ctrl (dev, 0x1b);
    }
  SetCCD_Channel (dev);
  SetSTI (dev);

}

void
Backward_onestep (mustek_pp_ccd300_priv * dev)
{
  unsigned char ucPhase;
  dev->m_wMotorStepNo++;
  SetLed_OnOff (dev);
  if (dev->IsGa1013)
    {
      SetCCD_Channel_WriteSRAM (dev);

      ucPhase = MotorPhase[dev->MotorPhaseStatus];
      if (dev->MotorPhaseStatus == 0)
	dev->MotorPhaseStatus = 3;
      else
	dev->MotorPhaseStatus--;
      OutChar (5, ucPhase, dev);

      SetCCD_Channel (dev);
      SetSTI (dev);
    }
  else
    {
      Asic1015_Motor_Ctrl (dev, 0x43);
    }

}

void
Asic1015_Motor_Ctrl (mustek_pp_ccd300_priv * dev, unsigned char ucMotorCtrl)
{
  OutChar (6, 0xf6, dev);
  OutChar (6, 0x22, dev);
  OutChar (5, ucMotorCtrl, dev);
  OutChar (6, 0x02, dev);
  CheckMotorSatus (dev);
}

void
Delay_nTimes_mSec (unsigned int wTimes)
{
  unsigned int wmSec, wLoop;
  wmSec = wTimes * 100;
  for (wLoop = 0; wLoop < wTimes; wLoop++)
    usleep (100);
}

void
SetSTI (mustek_pp_ccd300_priv * dev)
{
  OutChar (3, 0x00, dev);
  dev->bankcountValue++;
  dev->bankcountValue &= 0x07;
}

void
Motor_StepLoop (mustek_pp_ccd300_priv * dev,
		unsigned char ucForBackFlag, unsigned int wStepNo)
{
  unsigned int wMotorStepNo;
  for (wMotorStepNo = 0; wMotorStepNo < wStepNo; wMotorStepNo++)
    {
      if (ucForBackFlag == 0)
	Forward_onestep (dev);
      else
	Backward_onestep (dev);
      WaitBankCountChange (dev);
      ClearBankCount (dev);
    }
}

void
Motor_Off (mustek_pp_ccd300_priv * dev)
{
  unsigned char ucDataValue;
  OutChar (6, 0x22, dev);
  OutChar (5, 0x00, dev);
  OutChar (6, 0x82, dev);
  ucDataValue = GetBankCount (dev);
  dev->bankcountValue = ucDataValue;
  SetSTI (dev);
  WaitBankCountChange (dev);
  ClearBankCount (dev);
  dev->m_wMotorStepNo = 1;
  SetLed_OnOff (dev);
}

void
Store_Tmp_Data (mustek_pp_ccd300_priv * dev)
{
  dev->ResolutionTmp = dev->ASICResolution;
  dev->ScanModeTmp = dev->ScanMode;
  dev->Skip_ImageTmp = dev->Skip_ImageBytes;
}

void
Restore_Tmp_Data (mustek_pp_ccd300_priv * dev)
{
  dev->ASICResolution = dev->ResolutionTmp;
  dev->ScanMode = dev->ScanModeTmp;
  dev->Skip_ImageBytes = dev->Skip_ImageTmp;
}

void
IO_FindBlack_Data (mustek_pp_ccd300_priv * dev)
{
  unsigned int MotrStep;
  Store_Tmp_Data (dev);
  dev->Real_Scan_Bytes = CalScanParameter (dev->ScanResolution,
					   dev->Total_lineBytes);
  Check_DataPar (dev);
  dev->Scan_Bytes = 2550;
  dev->ASICResolution = 300;
  dev->ScanMode = 1;
  dev->Skip_ImageBytes = 0;
  dev->SkipCount = 0;
  SetCCDInfo (dev);
  dev->bankcountValue = GetBankCount (dev);
  FindHorBlackPos (dev);
  Restore_Tmp_Data (dev);

  dev->Scan_Bytes = dev->Total_lineBytes;
  Motor_StepLoop (dev, 0, 4);
  dev->SkipCountTmp = dev->SkipCount;

  GetCalibData (dev);

  dev->SkipCount = dev->SkipCountTmp;
  FindVerBlackPos (dev);
  Restore_Tmp_Data (dev);
  MotrStep = 47;

  if (0 != dev->scan_source)
    {
      LampOffOP (dev);
      if (99 != dev->scan_source)
	{

	  MotrStep += 460;
	  Motor_StepLoop (dev, 0, MotrStep);
	  dev->Scan_Bytes = dev->Total_lineBytes;
	  GetCalibData (dev);
	  MotrStep = 240;
	}
    }

  if (dev->ScanMode != 2)
    MotrStep += 16;
  MotrStep += dev->Frame_Top;
  Motor_StepLoop (dev, 0, MotrStep);
  RestoreCCDInfo_Set (dev);

  if (CheckCCD_Kind (dev) == 1)
    SetPixelAverage (dev);

}

void
CalRefBlack (mustek_pp_ccd300_priv * dev)
{
  dev->RefBlack = 0;
  dev->SkipCount = 0;
  dev->Skip_ImageBytes = 0;
  dev->Scan_Bytes = dev->Total_lineBytes;
  SetCCDInfo (dev);
  dev->bankcountValue = GetBankCount (dev);
  GetRefBlack (dev);
  dev->Skip_ImageBytes = dev->Skip_ImageTmp;
  dev->SkipCount = dev->SkipCountTmp;
  dev->Scan_Bytes = dev->Total_lineBytes;
  SetCCDInfo (dev);
  dev->bankcountValue = GetBankCount (dev);
}

void
GetRefBlack (mustek_pp_ccd300_priv * dev)
{
  unsigned char *pucBuf, *pucBufTmp;
  unsigned int wLoop, wTemp;
  pucBuf = malloc (2550);
  pucBufTmp = pucBuf;

  wTemp = 0;
  for (wLoop = 0; wLoop < 8; wLoop++)
    {
      dev->AbortDataLine = 0;
      dev->RGBChannel = 0;
      GetChannelData (dev, pucBuf);
      wTemp += (unsigned int) (*(pucBufTmp + 3));
    }
  wTemp /= (unsigned int) 8;

  dev->RefBlack_R = (unsigned char) wTemp;
  dev->DataValue = dev->RefBlack_R;
  wTemp = 0;
  for (wLoop = 0; wLoop < 8; wLoop++)
    {
      dev->AbortDataLine = 0;
      dev->RGBChannel = 1;
      GetChannelData (dev, pucBuf);
      wTemp += (unsigned int) (*(pucBufTmp + 3));
    }
  wTemp /= (unsigned int) 8;

  dev->RefBlack_G = (unsigned char) wTemp;
  dev->DataValue = dev->RefBlack_G;
  wTemp = 0;
  for (wLoop = 0; wLoop < 8; wLoop++)
    {
      dev->AbortDataLine = 0;
      dev->RGBChannel = 2;
      GetChannelData (dev, pucBuf);
      wTemp += (unsigned int) (*(pucBufTmp + 3));
    }
  wTemp /= (unsigned int) 8;

  dev->RefBlack_B = (unsigned char) wTemp;
  dev->DataValue = dev->RefBlack_B;
  free (pucBuf);
}

void
RestoreCCDInfo_Set (mustek_pp_ccd300_priv * dev)
{
  dev->Scan_Bytes = dev->Total_lineBytes;
  dev->Skip_ImageBytes = dev->Skip_ImageTmp;
  SetCCDInfo (dev);
  dev->bankcountValue = GetBankCount (dev);
}

void
FindHorBlackPos (mustek_pp_ccd300_priv * dev)
{
  unsigned int wLoop, wCount;
  unsigned int wByteCount;
  wCount = 0;
  wByteCount = sizeof (unsigned char) * 2550;
  dev->lpFind_black_Buf = malloc (wByteCount);
  memset (dev->lpFind_black_Buf, 0, wByteCount);
  for (wLoop = 0; wLoop < 20; wLoop++)
    {
      dev->RefBlack = 0;
      dev->SkipCount = 0;
      Forward_onestep (dev);
      WaitBankCountChange (dev);
      IO_GetData (dev, dev->lpFind_black_Buf);
      ClearBankCount (dev);
      FindHBlackPos (dev, dev->lpFind_black_Buf);
      if (dev->SkipCount > 2)
	{
	  dev->BubbleSortAry[wCount] = (unsigned char) (dev->SkipCount);
	  wCount++;
	}
      if (wCount >= 5)
	break;

    }
  Bubble_Sort_Arg (dev, 5);
  dev->SkipCount = (unsigned int) (dev->BubbleSortAry[0] + 11);
  dev->BlackPos = (unsigned int) (dev->BubbleSortAry[0]);
  free (dev->lpFind_black_Buf);
}

void
FindVerBlackPos (mustek_pp_ccd300_priv * dev)
{
  unsigned int wSkipTmp, wFindVerBkCount;
  unsigned int wBufSize;
  wFindVerBkCount = 0;
  wSkipTmp = dev->SkipCount;
  dev->Scan_Bytes = 2550;
  dev->ASICResolution = 300;
  dev->ScanMode = 1;
  dev->Skip_ImageBytes = 0;
  dev->SkipCount = 0;
  SetCCDInfo (dev);
  dev->bankcountValue = GetBankCount (dev);
  dev->DataValue = dev->bankcountValue;
  wBufSize = sizeof (unsigned char) * 2550;
  dev->lpFind_black_Buf = malloc (wBufSize);
  memset (dev->lpFind_black_Buf, 0, wBufSize);
  while (1)
    {
      Forward_onestep (dev);
      WaitBankCountChange (dev);
      IO_GetData (dev, dev->lpFind_black_Buf);
      ClearBankCount (dev);
      if (!FindVBlackPos (dev, dev->lpFind_black_Buf))
	break;
      wFindVerBkCount++;
      if (wFindVerBkCount > 76)
	break;

    }
  free (dev->lpFind_black_Buf);
  dev->SkipCount = wSkipTmp;
}

void
AllocBuffer (mustek_pp_ccd300_priv * dev)
{
  unsigned long wBufSize;
  wBufSize = 16 * sizeof (unsigned char) * 2550;
  dev->lpTmpBuf_R = malloc (wBufSize);
  memset (dev->lpTmpBuf_R, 0, wBufSize);
  wBufSize = 8 * sizeof (unsigned char) * 2550;
  dev->lpTmpBuf_B = malloc (wBufSize);
  memset (dev->lpTmpBuf_B, 0, wBufSize);

  wBufSize = 32 * sizeof (unsigned char) * 2550;
  dev->Calib_Buffer_R = malloc (wBufSize);
  memset (dev->Calib_Buffer_R, 0, wBufSize);
  dev->Calib_Buffer_G = malloc (wBufSize);
  memset (dev->Calib_Buffer_G, 0, wBufSize);
  dev->Calib_Buffer_B = malloc (wBufSize);
  memset (dev->Calib_Buffer_B, 0, wBufSize);
  dev->Calib_Gray_Buf = malloc (wBufSize);
  memset (dev->Calib_Gray_Buf, 0, wBufSize);
}

void
FreeBuf (mustek_pp_ccd300_priv * dev)
{
  free (dev->lpTmpBuf_R);
  free (dev->lpTmpBuf_B);
  free (dev->Calib_Buffer_R);
  free (dev->Calib_Buffer_G);
  free (dev->Calib_Buffer_B);
  free (dev->Calib_Gray_Buf);
}

void
IO_GetGrayData (mustek_pp_ccd300_priv * dev)
{
  unsigned int wLineCount;
  unsigned long dwPtrAddr;
  unsigned char *lpImagePtr;
  wLineCount = dev->BufferLine_Count;
  if (dev->BufferLine_Count > dev->Total_Scanlines)
    wLineCount = dev->Total_Scanlines;
  dev->Catch_Count = 0;
  dev->Catch_G_Count = 0;
  if (dev->CatchDataFirst)
    {
      Forward_onestep (dev);
      WaitBankCountChange (dev);
    }
  while (1)
    {
      dev->AbortDataLine = 0;
      Whether_Skip_One_Line (dev, dev->Catch_Count);
      Forward_onestep (dev);
      dev->Catch_Count++;
      if (!dev->AbortDataLine)
	{
	  lpImagePtr = dev->ImageBuffer;
	  dwPtrAddr = dev->Catch_G_Count;
	  dwPtrAddr *= dev->Real_Scan_Bytes;
	  lpImagePtr += dwPtrAddr;
	  if (dev->ASICRes == 0)
	    IO_GetData_SPEC (dev, lpImagePtr);
	  else
	    IO_GetData (dev, lpImagePtr);
	  dev->Catch_G_Count++;
	}
      ClearBankCount (dev);
      WaitBankCountChange (dev);
      if (dev->Catch_G_Count == wLineCount)
	break;

    }
  lpImagePtr = dev->ImageBuffer;
  CalibrationData_Gray (dev, lpImagePtr);
  dev->Total_Scanlines -= dev->Catch_G_Count;
  dev->Catch_Count = 0;
  dev->CatchDataFirst = 0;
}

void
IO_GetGrayData_100 (mustek_pp_ccd300_priv * dev)
{
  unsigned int wLineCount;
  unsigned long dwPtrAddr;
  unsigned char *lpImagePtr;
  wLineCount = dev->BufferLine_Count;
  if (dev->BufferLine_Count > dev->Total_Scanlines)
    wLineCount = dev->Total_Scanlines;
  dev->Catch_Count = 0;
  dev->Catch_G_Count = 0;
  while (1)
    {
      dev->AbortDataLine = 0;
      Whether_Skip_One_Line (dev, dev->Catch_Count);
      Forward_onestep (dev);
      WaitBankCountChange (dev);
      dev->Catch_Count++;
      if (!dev->AbortDataLine)
	{
	  lpImagePtr = dev->ImageBuffer;
	  dwPtrAddr = dev->Catch_G_Count;
	  dwPtrAddr *= dev->Real_Scan_Bytes;
	  lpImagePtr += dwPtrAddr;
	  IO_GetData (dev, lpImagePtr);
	  dev->Catch_G_Count++;
	}
      ClearBankCount (dev);
      if (dev->Catch_G_Count == wLineCount)
	break;
    }
  lpImagePtr = dev->ImageBuffer;
  CalibrationData_Gray (dev, lpImagePtr);
  dev->Total_Scanlines -= dev->Catch_G_Count;
  dev->Catch_Count = 0;
  dev->CatchDataFirst = 0;
}

void
IO_GetColorData (mustek_pp_ccd300_priv * dev)
{
  unsigned int wLineCount;
  unsigned char *lpImagePtr;
  wLineCount = dev->BufferLine_Count;
  if (dev->BufferLine_Count > dev->Total_Scanlines)
    wLineCount = dev->Total_Scanlines;
  dev->Catch_Count = 0;
  dev->Catch_R_Count = 0;
  dev->Catch_G_Count = 0;
  dev->Catch_B_Count = 0;
  if (dev->CatchDataFirst)
    {
      Forward_onestep (dev);
      WaitBankCountChange (dev);
    }
  else
    {
      lpImagePtr = dev->ImageBuffer;
      MoveR_Tmp_Image_Buffer (dev, lpImagePtr, dev->lpTmpBuf_R);
      lpImagePtr = dev->ImageBuffer;
      MoveB_Tmp_Image_Buffer (dev, lpImagePtr, dev->lpTmpBuf_B);
    }
  while (1)
    {
      dev->RGBChannel = 2;
      SetCCD_Channel (dev);
      SetSTI (dev);
      dev->RGBChannel = 0;
      Catch_Red_Line (dev);
      ClearBankCount (dev);
      WaitBankCountChange (dev);
      dev->RGBChannel = 1;
      SetCCD_Channel (dev);
      SetSTI (dev);
      dev->RGBChannel = 2;
      Catch_Blue_Line (dev);
      ClearBankCount (dev);
      WaitBankCountChange (dev);
      dev->RGBChannel = 0;
      Forward_onestep (dev);
      dev->RGBChannel = 1;
      Catch_Green_Line (dev);
      ClearBankCount (dev);
      WaitBankCountChange (dev);
      dev->Catch_Count++;
      if (dev->Catch_G_Count == wLineCount)
	break;

    }
  lpImagePtr = dev->ImageBuffer;
  CalibrationData_Color (dev, lpImagePtr);
  dev->Total_Scanlines -= dev->Catch_G_Count;
  dev->Catch_Count = 0;
  dev->CatchDataFirst = 0;
}

void
IO_GetColorData_100 (mustek_pp_ccd300_priv * dev)
{
  unsigned int wLineCount;
  unsigned char *lpImagePtr;
  wLineCount = dev->BufferLine_Count;
  if (dev->BufferLine_Count > dev->Total_Scanlines)
    wLineCount = dev->Total_Scanlines;
  dev->Catch_Count = 0;
  dev->Catch_R_Count = 0;
  dev->Catch_G_Count = 0;
  dev->Catch_B_Count = 0;
  if (dev->CatchDataFirst)
    {
      Forward_onestep (dev);
      WaitBankCountChange (dev);
    }
  else
    {
      lpImagePtr = dev->ImageBuffer;
      MoveR_Tmp_Image_Buffer (dev, lpImagePtr, dev->lpTmpBuf_R);
      lpImagePtr = dev->ImageBuffer;
      MoveB_Tmp_Image_Buffer (dev, lpImagePtr, dev->lpTmpBuf_B);
    }
  while (1)
    {
      dev->RGBChannel = 2;
      Forward_onestep (dev);
      dev->RGBChannel = 0;
      Catch_Red_Line (dev);
      ClearBankCount (dev);
      dev->Catch_Count++;
      WaitBankCountChange (dev);
      dev->RGBChannel = 1;
      Forward_onestep (dev);
      dev->RGBChannel = 2;
      Catch_Blue_Line (dev);
      ClearBankCount (dev);
      dev->Catch_Count++;
      WaitBankCountChange (dev);
      dev->RGBChannel = 0;
      Forward_onestep (dev);
      dev->RGBChannel = 1;
      Catch_Green_Line (dev);
      ClearBankCount (dev);
      dev->Catch_Count++;
      WaitBankCountChange (dev);
      if (dev->ScanResolution == 50)
	Res50_Go_3_step (dev);
      if (dev->Catch_G_Count == wLineCount)
	break;
    }
  lpImagePtr = dev->ImageBuffer;
  CalibrationData_Color (dev, lpImagePtr);
  dev->Total_Scanlines -= dev->Catch_G_Count;
  dev->Catch_Count = 0;
  dev->CatchDataFirst = 0;
}

void
Res50_Go_3_step (mustek_pp_ccd300_priv * dev)
{
  unsigned int wLoop;
  dev->RGBChannel = 0;
  for (wLoop = 0; wLoop < 3; wLoop++)
    {
      ClearBankCount (dev);
      dev->Catch_Count++;
      Forward_onestep (dev);
      WaitBankCountChange (dev);
    }
}
void
MoveR_Tmp_Image_Buffer (mustek_pp_ccd300_priv * dev,
			unsigned char * pImagePtr, unsigned char * pBufferPtr)
{
  unsigned int wLoop, wLoop1;
  if (dev->reverse_rgb == 1)
    pImagePtr += 2;
  dev->m_wByteCount = dev->Catch_R_TmpC;
  for (wLoop = 0; wLoop < dev->Catch_R_TmpC; wLoop++)
    {
      for (wLoop1 = 0; wLoop1 < dev->Real_Scan_Bytes; wLoop1++)
	{
	  *pImagePtr = *pBufferPtr;
	  pImagePtr += 3;
	  pBufferPtr++;
	}
      dev->Catch_R_Count++;
    }
  dev->Catch_R_TmpC = 0;
}

void
MoveB_Tmp_Image_Buffer (mustek_pp_ccd300_priv * dev,
			unsigned char * pImagePtr, unsigned char * pBufferPtr)
{
  unsigned int wLoop, wLoop1;
  if (dev->reverse_rgb == 0)
    pImagePtr += 2;
  dev->m_wByteCount = dev->Catch_B_TmpC;
  for (wLoop = 0; wLoop < dev->Catch_B_TmpC; wLoop++)
    {
      for (wLoop1 = 0; wLoop1 < dev->Real_Scan_Bytes; wLoop1++)
	{
	  *pImagePtr = *pBufferPtr;
	  pImagePtr += 3;
	  pBufferPtr++;
	}
      dev->Catch_B_Count++;
    }
  dev->Catch_B_TmpC = 0;
}

void
Catch_Red_Line (mustek_pp_ccd300_priv * dev)
{
  unsigned int wBuf_Lines;
  unsigned long dwPtrAddr;
  unsigned char *lpImagePtr;
  dev->AbortDataLine = 0;
  Whether_Skip_One_Line (dev, dev->Catch_Count);
  if (dev->AbortDataLine)
    return;
  wBuf_Lines = dev->BufferLine_Count - 1;

  dev->RefBlack = dev->RefBlack_R;
  if (dev->Catch_R_Count <= wBuf_Lines)
    {
      dwPtrAddr = dev->Catch_R_Count * 3;
      dwPtrAddr = dwPtrAddr * dev->Real_Scan_Bytes;
      lpImagePtr = dev->ImageBuffer + dwPtrAddr;
      if (dev->ASICRes == 1)
	IO_Color_Line (dev, lpImagePtr);
      else
	IO_Color_Line_SPEC (dev, lpImagePtr);
      dev->Catch_R_Count++;
    }
  else
    {
      dwPtrAddr = dev->Catch_R_TmpC;
      dwPtrAddr *= dev->Real_Scan_Bytes;
      lpImagePtr = dev->lpTmpBuf_R + dwPtrAddr;
      if (dev->ASICRes == 0)
	IO_GetData_SPEC (dev, lpImagePtr);
      else
	IO_GetData (dev, lpImagePtr);
      dev->Catch_R_TmpC++;
    }
}

void
Catch_Blue_Line (mustek_pp_ccd300_priv * dev)
{
  unsigned int wCatch_Count, wBuf_Lines;
  unsigned long dwPtrAddr;
  unsigned char *lpImagePtr;

  unsigned int wLineDiff;
  if (dev->CCD_Type == 1)
    wLineDiff = 4;
  else
    wLineDiff = 8;

  dev->AbortDataLine = 1;
  wCatch_Count = dev->Catch_Count;
  if (!dev->CatchDataFirst)
    {
      dev->AbortDataLine = 0;
    }
  else
    {

      if (wCatch_Count >= wLineDiff)
	{
	  dev->AbortDataLine = 0;

	  wCatch_Count -= wLineDiff;
	}
    }
  Whether_Skip_One_Line (dev, wCatch_Count);
  if (dev->AbortDataLine)
    return;
  dev->RefBlack = dev->RefBlack_B;
  wBuf_Lines = dev->BufferLine_Count - 1;
  if (dev->Catch_B_Count <= wBuf_Lines)
    {
      dwPtrAddr = dev->Catch_B_Count * 3;
      dwPtrAddr = dwPtrAddr * dev->Real_Scan_Bytes;
      lpImagePtr = dev->ImageBuffer + dwPtrAddr;
      if (dev->ASICRes == 1)
	IO_Color_Line (dev, lpImagePtr);
      else
	IO_Color_Line_SPEC (dev, lpImagePtr);
      dev->Catch_B_Count++;
    }
  else
    {
      dwPtrAddr = dev->Catch_B_TmpC;
      dwPtrAddr *= dev->Real_Scan_Bytes;
      lpImagePtr = dev->lpTmpBuf_B + dwPtrAddr;
      if (dev->ASICRes == 0)
	IO_GetData_SPEC (dev, lpImagePtr);
      else
	IO_GetData (dev, lpImagePtr);
      dev->Catch_B_TmpC++;
    }
}
void
Catch_Green_Line (mustek_pp_ccd300_priv * dev)
{
  unsigned int wCatch_Count;
  unsigned long dwPtrAddr;
  unsigned char *lpImagePtr;

  unsigned int wLineDiff;
  if (dev->CCD_Type == 1)
    wLineDiff = 8;
  else
    wLineDiff = 16;

  dev->AbortDataLine = 1;
  wCatch_Count = dev->Catch_Count;
  if (!dev->CatchDataFirst)
    {
      dev->AbortDataLine = 0;
    }
  else
    {

      if (wCatch_Count >= wLineDiff)
	{
	  dev->AbortDataLine = 0;

	  wCatch_Count -= wLineDiff;
	}
    }
  Whether_Skip_One_Line (dev, wCatch_Count);
  if (dev->AbortDataLine)
    return;
  dev->RefBlack = dev->RefBlack_G;
  dwPtrAddr = dev->Catch_G_Count * 3;
  dwPtrAddr = dwPtrAddr * dev->Real_Scan_Bytes;
  lpImagePtr = dev->ImageBuffer + dwPtrAddr;
  if (dev->ASICRes == 1)
    IO_Color_Line (dev, lpImagePtr);
  else
    IO_Color_Line_SPEC (dev, lpImagePtr);
  dev->Catch_G_Count++;
}

void
IO_GetData (mustek_pp_ccd300_priv * dev, unsigned char * pImagePtr)
{
  unsigned int wByteCount;
  InChar_Begin_Dispatch (dev->PM, dev, 1);
  IO_SkipData (dev);
  for (wByteCount = 0; wByteCount < dev->Scan_Bytes; wByteCount++)
    {
      dev->DataValue = InChar_Do_Dispatch (dev->PM, dev);
      usleep (100);
      dev->DataValue = SubRefBlack (dev, dev->DataValue);
      *pImagePtr = dev->DataValue;
      pImagePtr++;
    }
  InChar_End_Dispatch (dev->PM, dev);
}

void
IO_GetData_SPEC (mustek_pp_ccd300_priv * dev, unsigned char *pImagePtr)
{
  unsigned int wByteCount;
  InChar_Begin_Dispatch (dev->PM, dev, 1);
  IO_SkipData (dev);
  for (wByteCount = 0; wByteCount < dev->Scan_Bytes; wByteCount++)
    {
      dev->DataValue = InChar_Do_Dispatch (dev->PM, dev);
      usleep (100);
      if ((wByteCount % dev->Data_Get_Multi) != 0)
	{
	  dev->DataValue = SubRefBlack (dev, dev->DataValue);
	  *pImagePtr = dev->DataValue;
	  pImagePtr++;
	}
    }
  InChar_End_Dispatch (dev->PM, dev);
}

void
IO_Color_Line (mustek_pp_ccd300_priv * dev, unsigned char * pImagePtr)
{
  unsigned int wLoop;
  switch (dev->RGBChannel)
    {
    case 0:
      if (dev->reverse_rgb == 1)
	pImagePtr += 2;
      break;
    case 1:
      pImagePtr++;
      break;
    case 2:
      if (dev->reverse_rgb == 0)
	pImagePtr += 2;
      break;
    }
  InChar_Begin_Dispatch (dev->PM, dev, 1);
  IO_SkipData (dev);
  for (wLoop = 0; wLoop < dev->Scan_Bytes; wLoop++)
    {
      dev->DataValue = InChar_Do_Dispatch (dev->PM, dev);
      usleep (100);
      dev->DataValue = SubRefBlack (dev, dev->DataValue);
      *pImagePtr = dev->DataValue;
      pImagePtr += 3;
    }
  InChar_End_Dispatch (dev->PM, dev);
}

void
IO_Color_Line_SPEC (mustek_pp_ccd300_priv * dev, unsigned char * pImagePtr)
{
  unsigned int wByteCount;
  switch (dev->RGBChannel)
    {
    case 0:
      if (dev->reverse_rgb == 1)
	pImagePtr += 2;
      break;
    case 1:
      pImagePtr++;
      break;
    case 2:
      if (dev->reverse_rgb == 0)
	pImagePtr += 2;
      break;
    }
  InChar_Begin_Dispatch (dev->PM, dev, 1);
  IO_SkipData (dev);
  for (wByteCount = 0; wByteCount < dev->Scan_Bytes; wByteCount++)
    {
      dev->DataValue = InChar_Do_Dispatch (dev->PM, dev);
      usleep (100);
      if ((wByteCount % dev->Data_Get_Multi) != 0)
	{
	  dev->DataValue = SubRefBlack (dev, dev->DataValue);
	  *pImagePtr = dev->DataValue;
	  pImagePtr += 3;
	}
    }
  InChar_End_Dispatch (dev->PM, dev);
}

void
IO_SkipData (mustek_pp_ccd300_priv * dev)
{
  unsigned int wLoop, wSkipCount;
  wSkipCount = dev->SkipCount + 1;
  for (wLoop = 0; wLoop < wSkipCount; wLoop++)
    {
      InChar_Do_Dispatch (dev->PM, dev);
    }
}

void
Check_DataPar (mustek_pp_ccd300_priv * dev)
{
  dev->ASICRes = 1;
  switch (dev->ScanResolution)
    {
    case 50:
      dev->Data_Get_Multi = 2;
      dev->ASICRes = 0;
      break;
    case 150:
      dev->Data_Get_Multi = 4;
      dev->ASICRes = 0;
      break;
    case 250:
      dev->Data_Get_Multi = 6;
      dev->ASICRes = 0;
      break;
    default:
      break;
    }
}

void
Whether_Skip_One_Line (mustek_pp_ccd300_priv * dev, int wCatch_Count)
{
  switch (dev->ScanResolution)
    {
    case 100:
      if (dev->ScanMode == 2)
	{

	  if (dev->CCD_Type == 1)
	    Chk_Color_100_Abort_3794 (dev, wCatch_Count);
	  else

	    Chk_Color_100_Abort (dev, wCatch_Count);
	}
      else
	{
	  if ((wCatch_Count % 3) != 0)
	    dev->AbortDataLine = 1;
	}
      break;
    case 150:
      if ((wCatch_Count % 2) == 1)
	dev->AbortDataLine = 1;
      break;
    case 200:
      if ((wCatch_Count % 3) == 2)
	dev->AbortDataLine = 1;
      break;
    case 250:
      if ((wCatch_Count % 6) == 2)
	dev->AbortDataLine = 1;
      break;
    default:
      break;
    }
}

void
Chk_Color_100_Abort (mustek_pp_ccd300_priv * dev, int wCatch_Count)
{
  unsigned int wModNo;
  wModNo = wCatch_Count % 3;
  switch (dev->RGBChannel)
    {
    case 0:
      if (wModNo != 0)
	dev->AbortDataLine = 1;
      break;
    case 1:
      if (dev->CatchDataFirst)
	{
	  if (wModNo != 1)
	    dev->AbortDataLine = 1;
	}
      else
	{
	  if (wModNo != 2)
	    dev->AbortDataLine = 1;
	}
      break;
    case 2:
      if (dev->CatchDataFirst)
	{
	  if (wModNo != 2)
	    dev->AbortDataLine = 1;
	}
      else
	{
	  if (wModNo != 1)
	    dev->AbortDataLine = 1;
	}
      break;
    }
}

void
Chk_Color_100_Abort_3794 (mustek_pp_ccd300_priv * dev, int wCatch_Count)
{
  unsigned int wModNo;
  wModNo = wCatch_Count % 3;
  switch (dev->RGBChannel)
    {
    case 0:
      if (wModNo != 0)
	dev->AbortDataLine = 1;
      break;
    case 1:
      if (dev->CatchDataFirst)
	{
	  if (wModNo != 0)
	    dev->AbortDataLine = 1;
	}
      else
	{
	  if (wModNo != 2)
	    dev->AbortDataLine = 1;
	}
      break;
    case 2:
      if (dev->CatchDataFirst)
	{
	  if (wModNo != 0)
	    dev->AbortDataLine = 1;
	}
      else
	{
	  if (wModNo != 1)
	    dev->AbortDataLine = 1;
	}
      break;
    }
}

/*ARGSUSED*/
void
Delay_Motor_Times (mustek_pp_ccd300_priv * dev __UNUSED__, 
		   unsigned long lgScanTime)
{
  unsigned long lgTimes;
  unsigned long dwScanTime;
  unsigned int wScanTime;
  lgTimes = lgScanTime / 10000;
  dwScanTime = lgTimes;
  if (dwScanTime < 2)
    return;
  if (dwScanTime < 5)
    {

      return;
    }
  if (dwScanTime < 12)
    {
      wScanTime = (unsigned int) (12 - dwScanTime);

    }
}

void
GetCalibData (mustek_pp_ccd300_priv * dev)
{
  unsigned long dwLoop;
  unsigned char *pCalibPtr;
  SetCCDInfo (dev);
  dev->bankcountValue = GetBankCount (dev);
  if ((dev->ScanMode == 2) && (CheckCCD_Kind (dev) != 0))
    CalRefBlack (dev);
  for (dwLoop = 0; dwLoop < 32; dwLoop++)
    {
      if (dev->ScanMode != 2)
	{
	  pCalibPtr = dev->Calib_Gray_Buf + dev->Real_Scan_Bytes * dwLoop;
	  Forward_onestep (dev);
	  Get_Line_ntimes (dev, pCalibPtr);
	}
      else
	{
	  pCalibPtr = dev->Calib_Buffer_R + dev->Real_Scan_Bytes * dwLoop;
	  dev->RGBChannel = 0;
	  Forward_onestep (dev);
	  dev->RefBlack = dev->RefBlack_R;
	  Get_Line_ntimes (dev, pCalibPtr);
	  pCalibPtr = dev->Calib_Buffer_G + dev->Real_Scan_Bytes * dwLoop;
	  dev->RGBChannel = 1;
	  SetCCD_Channel (dev);
	  SetSTI (dev);
	  dev->RefBlack = dev->RefBlack_G;
	  Get_Line_ntimes (dev, pCalibPtr);
	  pCalibPtr = dev->Calib_Buffer_B + dev->Real_Scan_Bytes * dwLoop;
	  dev->RGBChannel = 2;
	  SetCCD_Channel (dev);
	  SetSTI (dev);
	  dev->RefBlack = dev->RefBlack_B;
	  Get_Line_ntimes (dev, pCalibPtr);
	}
    }
  if (dev->ScanMode != 2)
    {
      pCalibPtr = dev->Calib_Gray_Buf;
      GetMaxData (dev, pCalibPtr);
    }
  else
    {
      pCalibPtr = dev->Calib_Buffer_R;
      GetMaxData (dev, pCalibPtr);
      pCalibPtr = dev->Calib_Buffer_G;
      GetMaxData (dev, pCalibPtr);
      pCalibPtr = dev->Calib_Buffer_B;
      GetMaxData (dev, pCalibPtr);
    }
}

void
GetChannelData (mustek_pp_ccd300_priv * dev, unsigned char * pucBuf)
{
  SetCCD_Channel (dev);
  SetSTI (dev);
  WaitBankCountChange (dev);
  if (dev->ASICRes == 0)
    IO_GetData_SPEC (dev, pucBuf);
  else
    IO_GetData (dev, pucBuf);
  ClearBankCount (dev);
}

void
GetMaxData (mustek_pp_ccd300_priv * dev, unsigned char * pImagePtr)
{
  unsigned int wLoop, wLoop1;
  unsigned long wJumpPointer;
  unsigned int wTemp;
  for (wLoop = 0; wLoop < dev->Real_Scan_Bytes; wLoop++)
    {
      for (wLoop1 = 0; wLoop1 < 32; wLoop1++)
	{
	  wJumpPointer = (unsigned long) wLoop1 *dev->Real_Scan_Bytes;
	  dev->BubbleSortAry[wLoop1] = *(pImagePtr + wJumpPointer);
	}
      Bubble_Sort_Arg (dev, 32);

      wTemp = (unsigned int) dev->BubbleSortAry[4];
      wTemp += (unsigned int) dev->BubbleSortAry[5];
      wTemp += (unsigned int) dev->BubbleSortAry[6];
      wTemp += (unsigned int) dev->BubbleSortAry[7];
      wTemp /= (unsigned int) 4;
      *pImagePtr = (unsigned char) wTemp;

      pImagePtr++;
    }
}

void
Get_Line_ntimes (mustek_pp_ccd300_priv * dev, unsigned char * pImagePtr)
{
  unsigned char *pucCalib_Buf, *pCalibPtr;
  unsigned long dwLoop;
  unsigned long wBufSize;
  pCalibPtr = 0;
  wBufSize = 4 * sizeof (unsigned char) * dev->Real_Scan_Bytes;
  pucCalib_Buf = malloc (wBufSize);
  memset (pucCalib_Buf, 0, wBufSize);
  for (dwLoop = 0; dwLoop < 4; dwLoop++)
    {
      pCalibPtr = pucCalib_Buf + dev->Real_Scan_Bytes * dwLoop;
      WaitBankCountChange (dev);
      if (dev->ASICRes == 0)
	IO_GetData_SPEC (dev, pCalibPtr);
      else
	IO_GetData (dev, pCalibPtr);
      ClearBankCount (dev);
      if (dwLoop < 3)
	SetSTI (dev);
    }
  Average_Data (dev, pucCalib_Buf, pImagePtr);
  free (pucCalib_Buf);
}

void
Average_Data (mustek_pp_ccd300_priv * dev, unsigned char * pInImagePtr,
	      unsigned char * pOutImagePtr)
{
  unsigned int wLoop, wJumpPointer;
  unsigned int wAvgSum;
  for (wLoop = 0; wLoop < dev->Real_Scan_Bytes; wLoop++)
    {
      wJumpPointer = 0;
      wAvgSum = *pInImagePtr;
      wJumpPointer += dev->Real_Scan_Bytes;
      wAvgSum += *(pInImagePtr + wJumpPointer);
      wJumpPointer += dev->Real_Scan_Bytes;
      wAvgSum += *(pInImagePtr + wJumpPointer);
      wJumpPointer += dev->Real_Scan_Bytes;
      wAvgSum += *(pInImagePtr + wJumpPointer);
      wAvgSum /= 4;
      *pOutImagePtr = (unsigned char) wAvgSum;
      pInImagePtr++;
      pOutImagePtr++;
    }
}

void
FindHBlackPos (mustek_pp_ccd300_priv * dev, unsigned char * pImagePtr)
{
  unsigned char *pImagePtrTmp;
  unsigned int wByteCount, wLoop;
  wByteCount = dev->Scan_Bytes / 4;
  dev->RefBlack = *pImagePtr;
  dev->RefBlack_R = *pImagePtr;
  dev->RefBlack_G = *pImagePtr;
  dev->RefBlack_B = *pImagePtr;
  pImagePtrTmp = pImagePtr;
  pImagePtr = pImagePtr + wByteCount;
  for (wLoop = wByteCount; wLoop > 0; wLoop--)
    {
      if (ABSCompute (*pImagePtr, dev->RefBlack) < 15)
	break;
      pImagePtr--;
    }
  dev->SkipCount = wLoop;
  pImagePtr = pImagePtrTmp;
}

int
FindVBlackPos (mustek_pp_ccd300_priv * dev, unsigned char * pImagePtr)
{
  unsigned int wBkPos, wFindBkCount;
  pImagePtr = pImagePtr + dev->BlackPos;
  wFindBkCount = 0;
  for (wBkPos = 0; wBkPos < 10; wBkPos++)
    {
      if (*pImagePtr < 15)
	wFindBkCount++;
      pImagePtr--;
    }
  if (wFindBkCount >= 7)
    return 1;
  else
    return 0;
}

unsigned char
ABSCompute (unsigned char ucData1, unsigned char ucData2)
{
  unsigned char ucData;
  if (ucData1 > ucData2)
    ucData = ucData1 - ucData2;
  else
    ucData = ucData2 - ucData1;
  return ucData;
}

unsigned char
SubRefBlack (mustek_pp_ccd300_priv * dev, unsigned char ucData)
{

  if (ucData > dev->RefBlack)
    ucData -= dev->RefBlack;
  else
    ucData = 0;
  return ucData;
}

void
Bubble_Sort_Arg (mustek_pp_ccd300_priv * dev, unsigned int wCount)
{
  unsigned int wLoop, wLoop1;
  unsigned char ucTmp;
  for (wLoop = 0; wLoop < wCount; wLoop++)
    {
      for (wLoop1 = 0; wLoop1 < wCount - 1; wLoop1++)
	{
	  if (dev->BubbleSortAry[wLoop1] < dev->BubbleSortAry[wLoop1 + 1])
	    {
	      ucTmp = dev->BubbleSortAry[wLoop1];
	      dev->BubbleSortAry[wLoop1] = dev->BubbleSortAry[wLoop1 + 1];
	      dev->BubbleSortAry[wLoop1 + 1] = ucTmp;
	    }
	}
    }
}

void
CalibrationData_Gray (mustek_pp_ccd300_priv * dev, unsigned char * pImagePtr)
{
  unsigned int wLoop;
  unsigned long dwPtrAddr;
  unsigned char *pImageTmp;
  pImageTmp = pImagePtr;
  for (wLoop = 0; wLoop < dev->Catch_G_Count; wLoop++)
    {
      dwPtrAddr = wLoop;
      dwPtrAddr *= dev->Real_Scan_Bytes;
      pImagePtr = pImageTmp + dwPtrAddr;
      CalibrationData (dev, pImagePtr, dev->Calib_Gray_Buf);
    }
}

void CalibrationData_Color (mustek_pp_ccd300_priv * dev, 
			    unsigned char * pImagePtr)
{
  unsigned char *lpImageTmp;
  lpImageTmp = pImagePtr;
  if (dev->reverse_rgb == 1)
      lpImageTmp += 2;
    dev->RefBlack = dev->RefBlack_R;
    CalibrationData_R (dev, lpImageTmp);
    lpImageTmp = pImagePtr;
    lpImageTmp++;
    dev->RefBlack = dev->RefBlack_G;
    CalibrationData_G (dev, lpImageTmp);
    lpImageTmp = pImagePtr;
  if (dev->reverse_rgb == 0)
      lpImageTmp += 2;
    dev->RefBlack = dev->RefBlack_B;
    CalibrationData_B (dev, lpImageTmp);
}

void
CalibrationData_R (mustek_pp_ccd300_priv * dev, unsigned char * pImagePtr)
{
  unsigned int wLoop;
  unsigned long dwPtrAddr;
  unsigned char *pImageTmp;
  pImageTmp = pImagePtr;
  for (wLoop = 0; wLoop < dev->Catch_G_Count; wLoop++)
    {
      dwPtrAddr = wLoop * 3;
      dwPtrAddr *= dev->Real_Scan_Bytes;
      pImagePtr = pImageTmp + dwPtrAddr;
      CalibrationData (dev, pImagePtr, dev->Calib_Buffer_R);
    }
}

void
CalibrationData_G (mustek_pp_ccd300_priv * dev, unsigned char * pImagePtr)
{
  unsigned int wLoop;
  unsigned long dwPtrAddr;
  unsigned char *pImageTmp;
  pImageTmp = pImagePtr;
  for (wLoop = 0; wLoop < dev->Catch_G_Count; wLoop++)
    {
      dwPtrAddr = wLoop * 3;
      dwPtrAddr *= dev->Real_Scan_Bytes;
      pImagePtr = pImageTmp + dwPtrAddr;
      CalibrationData (dev, pImagePtr, dev->Calib_Buffer_G);
    }
}

void
CalibrationData_B (mustek_pp_ccd300_priv * dev, unsigned char * pImagePtr)
{
  unsigned int wLoop;
  unsigned long dwPtrAddr;
  unsigned char *pImageTmp;
  pImageTmp = pImagePtr;
  for (wLoop = 0; wLoop < dev->Catch_G_Count; wLoop++)
    {
      dwPtrAddr = wLoop * 3;
      dwPtrAddr *= dev->Real_Scan_Bytes;
      pImagePtr = pImageTmp + dwPtrAddr;
      CalibrationData (dev, pImagePtr, dev->Calib_Buffer_B);
    }
}

void
CalibrationData (mustek_pp_ccd300_priv * dev, unsigned char * pImagePtr,
		 unsigned char * pCaliBufPtr)
{
  unsigned int wLoop, wImage;
  unsigned char ucCalib;
  for (wLoop = 0; wLoop < dev->Real_Scan_Bytes; wLoop++)
    {
      wImage = (unsigned int) (*pImagePtr);
      ucCalib = *pCaliBufPtr;
      dev->DataValue = ucCalib;
      dev->DataValue = *pImagePtr;
      if (*pImagePtr > ucCalib)
	{
	  *pImagePtr = 255;
	}
      else
	{
	  if (ucCalib == 0)
	    wImage = 255;
	  else
	    wImage = (wImage * 255) / ((unsigned int) (ucCalib));
	  *pImagePtr = (unsigned char) wImage;
	}

      pImagePtr++;
      pCaliBufPtr++;
      if (dev->ScanMode == 2)
	pImagePtr += 2;
    }
}

/* mustek_pp interface */

#define MUSTEK_PP_CCD300	4

SANE_Status
ccd300_init (SANE_Int options, SANE_String_Const port,
	     SANE_String_Const name, SANE_Attach_Callback attach)
{
  SANE_Status status;
  mustek_pp_ccd300_priv *dev;

  if ((options != CAP_NOTHING) && (options != CAP_TA))
    {
      DBG (1, "ccd300_init: called with unkown options (0x%02x)\n", options);
      return SANE_STATUS_INVAL;
    }
  
  if ((dev = malloc (sizeof (mustek_pp_ccd300_priv))) == NULL)
    {
      DBG (2, "ccd300_init: not enough free memory\n");
      return SANE_STATUS_NO_MEM;
    }

  /* try to attach to the supplied port */
  status = sanei_pa4s2_open (port, &dev->fd);

  if (status != SANE_STATUS_GOOD)
    {
      DBG (2, "ccd300_init: couldn't attach to port ``%s'' (%s)\n",
	port, sane_strstatus(status));
      free (dev);
      return status;
    }

  status = ParRead (dev);

  sanei_pa4s2_close (dev->fd);
  free (dev);
  
  if (status != SANE_STATUS_GOOD)
    {

      DBG (2, "ccd300_init: scanner not recognized (%s)\n",
	   sane_strstatus (status));
    return status;

    }

  DBG (3, "ccd300_init: found scanner on port ``%s''\n", port);

  return attach (port, name, MUSTEK_PP_CCD300, options);

}


void
ccd300_capabilities (SANE_Int info, SANE_String * model,
		     SANE_String * vendor, SANE_String * type,
		     SANE_Int * maxres, SANE_Int * minres,
		     SANE_Int * maxhsize, SANE_Int * maxvsize,
		     SANE_Int * caps)
{

  *model = strdup ("600 III EP Plus");
  *vendor = strdup ("Mustek");
  if (info & CAP_TA)
    {
      *type = strdup ("flatbed with TA (CCD 300 dpi)");
      DBG (3, "ccd300_capabilities: 600 III EP Plus with TA\n");
    }
  else
    {
      *type = strdup ("flatbed (CCD 300 dpi)");
      DBG (3, "ccd300_capabilities: 600 III EP Plus\n");
    }
  
  *maxres = 300;
  *minres = 50;
  *maxhsize = 2550; /* FIXME: validate these constants */
  *maxvsize = 3500;
  *caps = info | CAP_INVERT | CAP_LAMP_OFF;

}

SANE_Status
ccd300_open (SANE_String port, SANE_Int caps, SANE_Int * fd)
{
  SANE_Status status;

  if ((caps != CAP_NOTHING) && (caps != CAP_TA))
    {
      DBG (1, "ccd300_open: called with unknown capabilities (0x%02X)\n",
	   caps);
      return SANE_STATUS_INVAL;
    }

  DBG (3, "ccd300_open: called for port %s\n", port);

  status = sanei_pa4s2_open (port, fd);

  if (status != SANE_STATUS_GOOD)
    DBG (2, "ccd300_open: open failed (%s)\n", sane_strstatus (status));

  return status;
}

void
ccd300_setup (SANE_Handle hndl)
{

  Mustek_pp_Handle *handle = hndl;
  mustek_pp_ccd300_priv *dev;
  
  handle->lamp_on = 0;

  handle->priv = NULL;

  if ((dev = malloc (sizeof (mustek_pp_ccd300_priv))) == NULL)
    {
      DBG (2, "ccd300_setup: not enough memory");
      return;
    }

  handle->priv = dev;

  dev->fd = handle->fd;
  dev->bw = 127;

  Switch_To_Scanner(dev);

  GetDeviceInfo(dev);
  LampPowerOn(dev);
  PullCarriage_ToHome(dev);

  Switch_To_Printer(dev);

  dev->LampOnTime = time(NULL);
}

SANE_Status
ccd300_config (SANE_Handle handle, SANE_String_Const optname,
		SANE_String_Const optval)
{
	Mustek_pp_Handle *hndl = handle;
	mustek_pp_ccd300_priv *dev = hndl->priv;
	int value;

	DBG (4, "ccd300_config: %s %s %s",
		optname, (optval ? "=" : ""), (optval ? optval : ""));

	if (!strcmp(optname,"bw")) {
		if (!optval) {
			DBG (1, "ccd300_config: missing value for option bw");
			return SANE_STATUS_INVAL;
		}

		value = atoi(optval);

		if ((value < 0) || (value > 255)) {
			DBG (1, "ccd300_config: value %d for option bw out of range (0 <= bw <= 255)\n", value);
			return SANE_STATUS_INVAL;
		}

		dev->bw = value;

	} else {
		DBG (1, "ccd300_config: unkown option %s", optname);
		return SANE_STATUS_INVAL;
	}

	return SANE_STATUS_GOOD;
			
}

void ccd300_close(SANE_Handle handle)
{
	Mustek_pp_Handle *hndl = handle;
	mustek_pp_ccd300_priv *dev = hndl->priv;
  
  Switch_To_Scanner(dev);
  A4StopScan(0, 0, dev);
  LampPowerOff(dev);
  Switch_To_Printer(dev);

}

SANE_Status ccd300_start (SANE_Handle handle)
{
  Mustek_pp_Handle *hndl = handle;
  mustek_pp_ccd300_priv *dev = hndl->priv;
  int bitsperpixel, samplesperpixel;
  
  Switch_To_Scanner(dev);
  PullCarriage_ToHome (dev);
  dev->ErrorCode = 0;
  dev->ScanResolution = hndl->res;
  dev->Skip_ImageBytes = hndl->topX;

  switch (hndl->mode) {

    case MODE_BW:
      dev->ScanMode = 0;
      bitsperpixel = 1;
      samplesperpixel = 1;
      break;

    case MODE_GRAYSCALE:
      dev->ScanMode = 1;
      bitsperpixel = 8;
      samplesperpixel = 1;
      break;

    case MODE_COLOR:
      dev->ScanMode = 2;
      bitsperpixel = 24;
      samplesperpixel = 3;
      break;
  }

  A4StartScan(0, 0, dev);
  Switch_To_Printer(dev);

  return SANE_STATUS_GOOD;
}
