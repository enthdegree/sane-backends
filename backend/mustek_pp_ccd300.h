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
#ifndef __MUSTEK_PP_CCD300_H
#define __MUSTEK_PP_CCD300_H


typedef struct
{
  int fd;

  unsigned char BubbleSortAry[32];
  unsigned char *lpTmpBuf_R, *lpTmpBuf_B;
  unsigned char *Calib_Gray_Buf, *Calib_Buffer_R;
  unsigned char *Calib_Buffer_G, *Calib_Buffer_B;
  unsigned char *lpFind_black_Buf;
  unsigned int ResolutionTmp, ScanModeTmp, Skip_ImageTmp;
  unsigned int m_wMotorStepNo;
  unsigned char *lpTmp;
  unsigned char DataValue;
  unsigned int m_wByteCount;
  unsigned long m_dwByteCount;
  unsigned char CCD_Type;
  unsigned long TimeOut;
  unsigned char *Controller;
  unsigned char *DataBuffer;
  unsigned char *CommandBuffer;
  unsigned char *ImageBuffer;
  int CreatedSymbolicLink;
  int CatchDataFirst;
  int AbortDataLine;
  int IsGa1013;
  unsigned char ImageCtrl;
  unsigned char PCstatus;
  unsigned char ErrorFlag;
  unsigned char PixelFlavor;
  unsigned char scan_source;
  unsigned char PM;
  unsigned char ScanSetting;
  unsigned char FirstBank;
  unsigned char ToggleFlag;
  unsigned char DelayModeFlag;
  unsigned char ChangeTimes;
  unsigned char ADF_Flag;
  unsigned char SlideKit_Flag;
  unsigned char FirstIn;
  unsigned char EppAddrOrg;
  unsigned char deviceData;
  unsigned char deviceStatus;
  unsigned char deviceControl;
  unsigned char bankcountValue;
  unsigned char MotorPhaseStatus;
  unsigned char RefBlack;
  unsigned char RefBlack_R;
  unsigned char RefBlack_G;
  unsigned char RefBlack_B;
  unsigned char ASICRes;
  unsigned char Data_Get_Multi;
  unsigned int BufferLine_Count;
  unsigned int reverse_rgb;
  unsigned int Frame_Left;
  unsigned int Frame_Top;
  unsigned int Frame_Right;
  unsigned int Frame_Bottom;
  unsigned int AtHomeSensor;
  unsigned int BlackPos;
  unsigned int SkipCount;
  unsigned int SkipCountTmp;
  unsigned int Skip_ImageBytes;
  unsigned int RGBChannel;
  unsigned int ScanMode;
  unsigned int ErrorCode;
  unsigned int Total_Scanlines;
  unsigned int Total_lineBytes;
  unsigned int Scan_Lines;
  unsigned int Scan_Linebytes;
  unsigned int Real_Scan_Bytes;
  unsigned int Scan_Bytes;
  unsigned int Model;
  unsigned int ScanResolution;
  unsigned int ASICResolution;
  unsigned int Catch_R_TmpC;
  unsigned int Catch_B_TmpC;
  unsigned int Catch_R_Count;
  unsigned int Catch_G_Count;
  unsigned int Catch_B_Count;
  unsigned int Catch_Count;
  unsigned int txf_lines;
  signed short RValue;
  signed short GValue;
  signed short BValue;
  int bw;
  time_t LampOnTime;
}
mustek_pp_ccd300_priv;

/* prototypes */
SANE_Status ParRead (mustek_pp_ccd300_priv * dev);
void Switch_To_Scanner (mustek_pp_ccd300_priv * dev);
void Switch_To_Printer (mustek_pp_ccd300_priv * dev);
void LampPowerOn (mustek_pp_ccd300_priv * dev);
void LampOnOP (mustek_pp_ccd300_priv * dev);
void LampPowerOff (mustek_pp_ccd300_priv * dev);
void LampOffOP (mustek_pp_ccd300_priv * dev);
void SetCCDInfo (mustek_pp_ccd300_priv * dev);
void SetCCDDPI (mustek_pp_ccd300_priv * dev);
void SetCCDMode (mustek_pp_ccd300_priv * dev);
void SetCCDMode_1015 (mustek_pp_ccd300_priv * dev);
void SetCCDInvert_1015 (mustek_pp_ccd300_priv * dev);
void SetPixelAverage (mustek_pp_ccd300_priv * dev);
void SetCCD_Channel_WriteSRAM (mustek_pp_ccd300_priv * dev);
void SetCCD_Channel (mustek_pp_ccd300_priv * dev);
void SetCCDInvert (mustek_pp_ccd300_priv * dev);
void ClearBankCount (mustek_pp_ccd300_priv * dev);
void SetDummyCount (mustek_pp_ccd300_priv * dev);
void SetScanByte (mustek_pp_ccd300_priv * dev);
void SetRGBRefVoltage (mustek_pp_ccd300_priv * dev);
void SetLed_OnOff (mustek_pp_ccd300_priv * dev);
void OutChar (unsigned char RegNo,
	      unsigned char OutData, mustek_pp_ccd300_priv * dev);
unsigned char Read_a_Byte (mustek_pp_ccd300_priv * dev, unsigned char RegNo);
void InChar_Begin_Dispatch (unsigned char Mode, mustek_pp_ccd300_priv * dev, 
			    unsigned char RegNo);
unsigned char InChar_Do_Dispatch (unsigned char Mode, 
				  mustek_pp_ccd300_priv * dev);
void InChar_End_Dispatch (unsigned char Mode, mustek_pp_ccd300_priv * dev);
unsigned char Change_Mode (mustek_pp_ccd300_priv * dev);
unsigned char ReadID1 (unsigned char Mode, mustek_pp_ccd300_priv * dev);
void CheckMotorSatus (mustek_pp_ccd300_priv * dev);
void CheckPIPStatus (mustek_pp_ccd300_priv * dev);
unsigned char GetBankCount (mustek_pp_ccd300_priv * dev);
unsigned char CheckCCDBit (mustek_pp_ccd300_priv * dev);
unsigned char CheckCCD_Kind (mustek_pp_ccd300_priv * dev);
void WaitBankCountChange (mustek_pp_ccd300_priv * dev);
void GetDeviceInfo (mustek_pp_ccd300_priv * dev);
void SetScanParameter (int DeviceObject, int Irp, mustek_pp_ccd300_priv * dev);
void GetScanParameter (int DeviceObject, int Irp, mustek_pp_ccd300_priv * dev);
void A4StartScan (int DeviceObject, int Irp, mustek_pp_ccd300_priv * dev);
void A4StopScan (int DeviceObject, int Irp, mustek_pp_ccd300_priv * dev);
void A4CheckScanner_HomeSensor (int DeviceObject, int Irp,
			        mustek_pp_ccd300_priv * dev);
void A4CarriageTo_Home (int DeviceObject, int Irp, 
			mustek_pp_ccd300_priv * dev);
void PullCarriage_ToHome (mustek_pp_ccd300_priv * dev);
void Motor_BackHome (mustek_pp_ccd300_priv * dev);
void A4GetImage (int DeviceObject, int Irp, mustek_pp_ccd300_priv * dev);
void SetASICRes (mustek_pp_ccd300_priv * dev);
int CalScanParameter (int wResolution, int wPar);
void Forward_onestep (mustek_pp_ccd300_priv * dev);
void Backward_onestep (mustek_pp_ccd300_priv * dev);
void Asic1015_Motor_Ctrl (mustek_pp_ccd300_priv * dev,
			  unsigned char ucMotorCtrl);
void Delay_nTimes_mSec (unsigned int wTimes);
void SetSTI (mustek_pp_ccd300_priv * dev);
void Motor_StepLoop (mustek_pp_ccd300_priv * dev,
		     unsigned char ucForBackFlag, unsigned int wStepNo);
void Motor_Off (mustek_pp_ccd300_priv * dev);
void Store_Tmp_Data (mustek_pp_ccd300_priv * dev);
void Restore_Tmp_Data (mustek_pp_ccd300_priv * dev);
void IO_FindBlack_Data (mustek_pp_ccd300_priv * dev);
void CalRefBlack (mustek_pp_ccd300_priv * dev);
void GetRefBlack (mustek_pp_ccd300_priv * dev);
void RestoreCCDInfo_Set (mustek_pp_ccd300_priv * dev);
void FindHorBlackPos (mustek_pp_ccd300_priv * dev);
void FindVerBlackPos (mustek_pp_ccd300_priv * dev);
void AllocBuffer (mustek_pp_ccd300_priv * dev);
void FreeBuf (mustek_pp_ccd300_priv * dev);
void IO_GetGrayData (mustek_pp_ccd300_priv * dev);
void IO_GetGrayData_100 (mustek_pp_ccd300_priv * dev);
void IO_GetColorData (mustek_pp_ccd300_priv * dev);
void IO_GetColorData_100 (mustek_pp_ccd300_priv * dev);
void Res50_Go_3_step (mustek_pp_ccd300_priv * dev);
void MoveR_Tmp_Image_Buffer (mustek_pp_ccd300_priv * dev,
			     unsigned char * pImagePtr, 
			     unsigned char * pBufferPtr);
void MoveB_Tmp_Image_Buffer (mustek_pp_ccd300_priv * dev,
			     unsigned char * pImagePtr, 
			     unsigned char * pBufferPtr);
void Catch_Red_Line (mustek_pp_ccd300_priv * dev);
void Catch_Blue_Line (mustek_pp_ccd300_priv * dev);
void Catch_Green_Line (mustek_pp_ccd300_priv * dev);
void IO_GetData (mustek_pp_ccd300_priv * dev, unsigned char * pImagePtr);
void IO_GetData_SPEC (mustek_pp_ccd300_priv * dev, unsigned char *pImagePtr);
void IO_Color_Line (mustek_pp_ccd300_priv * dev, unsigned char * pImagePtr);
void IO_Color_Line_SPEC (mustek_pp_ccd300_priv * dev, 
			 unsigned char * pImagePtr);
void IO_SkipData (mustek_pp_ccd300_priv * dev);
void Check_DataPar (mustek_pp_ccd300_priv * dev);
void Whether_Skip_One_Line (mustek_pp_ccd300_priv * dev, int wCatch_Count);
void Chk_Color_100_Abort (mustek_pp_ccd300_priv * dev, int wCatch_Count);
void Chk_Color_100_Abort_3794 (mustek_pp_ccd300_priv * dev, int wCatch_Count);
void Delay_Motor_Times (mustek_pp_ccd300_priv * dev, unsigned long lgScanTime);
void GetCalibData (mustek_pp_ccd300_priv * dev);
void GetChannelData (mustek_pp_ccd300_priv * dev, unsigned char * pucBuf);
void GetMaxData (mustek_pp_ccd300_priv * dev, unsigned char * pImagePtr);
void Get_Line_ntimes (mustek_pp_ccd300_priv * dev, unsigned char * pImagePtr);
void Average_Data (mustek_pp_ccd300_priv * dev, unsigned char * pInImagePtr,
		   unsigned char * pOutImagePtr);
void FindHBlackPos (mustek_pp_ccd300_priv * dev, unsigned char * pImagePtr);
int FindVBlackPos (mustek_pp_ccd300_priv * dev, unsigned char * pImagePtr);
unsigned char ABSCompute (unsigned char ucData1, unsigned char ucData2);
unsigned char SubRefBlack (mustek_pp_ccd300_priv * dev, unsigned char ucData);
void Bubble_Sort_Arg (mustek_pp_ccd300_priv * dev, unsigned int wCount);
void CalibrationData_Gray (mustek_pp_ccd300_priv * dev, 
			   unsigned char * pImagePtr);
void CalibrationData_Color (mustek_pp_ccd300_priv * dev, 
			    unsigned char * pImagePtr);
void CalibrationData_R (mustek_pp_ccd300_priv * dev, 
			unsigned char * pImagePtr);
void CalibrationData_G (mustek_pp_ccd300_priv * dev, 
			unsigned char * pImagePtr);
void CalibrationData_B (mustek_pp_ccd300_priv * dev, 
			unsigned char * pImagePtr);
void CalibrationData (mustek_pp_ccd300_priv * dev, unsigned char * pImagePtr,
		      unsigned char * pCaliBufPtr);
#endif
