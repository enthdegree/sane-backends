/*.............................................................................
 * Project : linux driver for Plustek parallel-port scanners
 *.............................................................................
 * File:	 plustek-pp_procs.h
 *           here are the prototypes of all exported functions
 *.............................................................................
 *
 * based on sources acquired from Plustek Inc.
 * Copyright (C) 1998 Plustek Inc.
 * Copyright (C) 2000-2003 Gerhard Jaeger <gerhard@gjaeger.de>
 * also based on the work done by Rick Bronson <rick@efn.org>
 *.............................................................................
 * History:
 * 0.30 - initial version
 * 0.31 - no changes
 * 0.32 - no changes
 * 0.33 - no changes
 * 0.34 - added this history
 * 0.35 - added Kevins´ changes to MiscRestorePort
 *		  changed prototype of MiscReinitStruct
 *		  added prototype for function PtDrvLegalRequested()
 * 0.36 - added prototype for function MiscLongRand()
 *		  removed PtDrvLegalRequested()
 *		  changed prototype of function MiscInitPorts()
 * 0.37 - added io.c and procfs.c
 *        added MiscGetModelName()
 *        added ModelSetA3I()
 * 0.38 - added P12 stuff
 *        removed prototype of IOScannerIdleMode()
 *        removed prototype of IOSPPWrite()
 * 0.39 - moved prototypes for the user space stuff to plustek-share.h
 * 0.40 - no changes
 * 0.41 - no changes
 * 0.42 - added MapAdjust
 *
 *.............................................................................
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#ifndef __PROCS_H__
#define __PROCS_H__

/*
 * implementation in plustekpp-misc.c
 */
pScanData 	MiscAllocAndInitStruct( void );
int			MiscReinitStruct	  ( pScanData ps );

int		  	MiscInitPorts   ( pScanData ps, int port );
void	  	MiscRestorePort ( pScanData ps );
inline void MiscStartTimer  ( pTimerDef timer, unsigned long us );
inline int  MiscCheckTimer  ( pTimerDef timer );

int  MiscRegisterPort       ( pScanData ps, int portAddr );
void MiscUnregisterPort     ( pScanData ps );
int  MiscClaimPort	        ( pScanData ps );
void MiscReleasePort        ( pScanData ps );
Long MiscLongRand	        ( void );
const char *MiscGetModelName( UShort id );

#ifdef DEBUG
Bool MiscAllPointersSet( pScanData ps );
#endif

/*
 * implementation in plustekpp-detect.c
 */
int DetectScanner( pScanData ps, int mode );

/*
 * implementation in plustekpp-p48xx.c
 */
int P48xxInitAsic( pScanData ps );

/*
 * implementation in plustekpp-p9636.c
 */
int P9636InitAsic( pScanData ps );

/*
 * implementation in plustekpp-p12.c
 */
int  P12InitAsic          ( pScanData ps );
void P12SetGeneralRegister( pScanData ps );

/*
 * implementation in plustekpp-p12ccd.c
 */
void P12InitCCDandDAC( pScanData ps, Bool shading );

/*
 * implementation in plustekpp-models.c
 */
void ModelSet4800 ( pScanData ps );
void ModelSet4830 ( pScanData ps );
void ModelSet600  ( pScanData ps );
void ModelSet12000( pScanData ps );
void ModelSetA3I  ( pScanData ps );
void ModelSet9630 ( pScanData ps );
void ModelSet9636 ( pScanData ps );
void ModelSetP12  ( pScanData ps );

/*
 * implementation in plustekpp-dac.c
 */
int  DacInitialize( pScanData ps );

void DacP98AdjustDark					   ( pScanData ps );
void DacP98FillGainOutDirectPort		   ( pScanData ps );
void DacP98FillShadingDarkToShadingRegister( pScanData ps );

void DacP96WriteBackToGammaShadingRAM( pScanData ps );

void DacP98003FillToDAC ( pScanData ps, pRGBByteDef regs, pColorByte data );
void DacP98003AdjustGain( pScanData ps, ULong color, Byte hilight );
Byte DacP98003SumGains  ( pUChar pb, ULong pixelsLine );

/*
 * implementation in plustekpp-motor.c
 */
int  MotorInitialize	 ( pScanData ps );
void MotorSetConstantMove( pScanData ps, Byte bMovePerStep );
void MotorToHomePosition ( pScanData ps );

void MotorP98GoFullStep  ( pScanData ps, ULong dwStep );

void MotorP96SetSpeedToStopProc( pScanData ps );
void MotorP96ConstantMoveProc  ( pScanData ps, ULong dwLines );
Bool MotorP96AheadToDarkArea   ( pScanData ps );
void MotorP96AdjustCurrentSpeed( pScanData ps, Byte bSpeed );

void MotorP98003BackToHomeSensor     ( pScanData ps );
void MotorP98003ModuleForwardBackward( pScanData ps );
void MotorP98003ForceToLeaveHomePos  ( pScanData ps );
void MotorP98003PositionYProc        ( pScanData ps, ULong steps);

/*
 * implementation in plustekpp-map.c
 */
void MapInitialize ( pScanData ps );
void MapSetupDither( pScanData ps );
void MapAdjust     ( pScanData ps, int which );

/*
 * implementation in plustekpp-image.c
 */
int ImageInitialize( pScanData ps );

/*
 * implementation in plustekpp-genericio.c
 */
int  IOFuncInitialize	   ( pScanData ps );
Byte IOSetToMotorRegister  ( pScanData ps );
Byte IOGetScanState		   ( pScanData ps, Bool fOpenned );
Byte IOGetExtendedStatus   ( pScanData ps );
void IOGetCurrentStateCount( pScanData, pScanState pScanStep);
int  IOIsReadyForScan	   ( pScanData ps );

void  IOSetXStepLineScanTime( pScanData ps, Byte b );
void  IOSetToMotorStepCount ( pScanData ps );
void  IOSelectLampSource	( pScanData ps );
Bool  IOReadOneShadingLine  ( pScanData ps, pUChar pBuf, ULong len );
ULong IOReadFifoLength	    ( pScanData ps );
void  IOPutOnAllRegisters   ( pScanData ps );
void  IOReadColorData       ( pScanData ps, pUChar pBuf, ULong len );

/*
 * implementation in plustekpp-io.c
 */
int  IOInitialize		 ( pScanData ps );
void IOMoveDataToScanner ( pScanData ps, pUChar pBuffer, ULong size );
void IODownloadScanStates( pScanData ps );
void IODataToScanner     ( pScanData, Byte bValue );
void IODataToRegister    ( pScanData ps, Byte bReg, Byte bData );
Byte IODataFromRegister  ( pScanData ps, Byte bReg );
void IORegisterToScanner ( pScanData ps, Byte bReg );
void IODataRegisterToDAC ( pScanData ps, Byte bReg, Byte bData );

Byte IODataRegisterFromScanner( pScanData ps, Byte bReg );
void IOCmdRegisterToScanner   ( pScanData ps, Byte bReg, Byte bData );
void IORegisterDirectToScanner( pScanData, Byte bReg );
void IOSoftwareReset          ( pScanData ps );
void IOReadScannerImageData   ( pScanData ps, pUChar pBuf, ULong size );
void IOFill97003Register      ( pScanData ps, Byte bDecode1, Byte bDecode2 );

void IOOut       ( Byte data, UShort port );
void IOOutDelayed( Byte data, UShort port );
Byte IOIn        ( UShort port );
Byte IOInDelayed ( UShort port );


/*
 * implementation in plustekpp-tpa.c
 */
void TPAP98001AverageShadingData( pScanData ps );
void TPAP98003FindCenterPointer ( pScanData ps );
void TPAP98003Reshading         ( pScanData ps );

/*
 * implementation in plustekpp-scale.c
 */
void ScaleX( pScanData ps, pUChar inBuf, pUChar outBuf );

/*
 * implementation in plustekpp-procfs.c (Kernel-mode only)
 */
#ifdef __KERNEL__
int  ProcFsInitialize	   ( void );
void ProcFsShutdown  	   ( void );
void ProcFsRegisterDevice  ( pScanData ps );
void ProcFsUnregisterDevice( pScanData ps );
#endif

#endif	/* guard __PROCS_H__ */

/* END PLUSTEK-PP_PROCS.H ...................................................*/
