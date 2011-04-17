/* sane - Scanner Access Now Easy.

   Copyright (C) 2005 Mustek.
   Originally maintained by Mustek
   Author:Jack Roy 2005.5.24

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

   This file implements a SANE backend for the Mustek BearPaw 2448 TA Pro 
   and similar USB2 scanners. */

#ifndef MUSTEK_USB2_HIGH_H
#define MUSTEK_USB2_HIGH_H

#include "mustek_usb2_asic.h"


typedef enum
{
  CM_RGB48,
  CM_RGB24,
  CM_GRAY16,
  CM_GRAY8,
  CM_TEXT
} COLORMODE;


typedef struct
{
  COLORMODE cmColorMode;
  SCANSOURCE ssScanSource;
  unsigned short wDpi;
  unsigned short wX;
  unsigned short wY;
  unsigned short wWidth;
  unsigned short wHeight;
  unsigned short wLineartThreshold;
  unsigned int dwBytesPerRow;
} TARGETIMAGE, *PTARGETIMAGE;


#define _MAX(a,b) ((a)>(b)?(a):(b))
#define _MIN(a,b) ((a)<(b)?(a):(b))

/* used for adjusting the AD offset */
#define WHITE_MAX_LEVEL            220
#define WHITE_MIN_LEVEL            210
#define MAX_LEVEL_RANGE            210
#define MIN_LEVEL_RANGE            190

/* 600 dpi */
#define FIND_LEFT_TOP_WIDTH_IN_DIP          512
#define FIND_LEFT_TOP_HEIGHT_IN_DIP         180
#define FIND_LEFT_TOP_CALIBRATE_RESOLUTION  600

#define TA_FIND_LEFT_TOP_WIDTH_IN_DIP       2668
#define TA_FIND_LEFT_TOP_HEIGHT_IN_DIP      300

/* must be a multiple of 8 */
#define LINE_CALIBRATION__16BITS_HEIGHT     40

/* the length from block bar to start calibration position */
#define BEFORE_SCANNING_MOTOR_FORWARD_PIXEL 40

#define TRAN_START_POS                      4550

/* 300 dpi */
#define MAX_SCANNING_WIDTH                  2550	/* just for A4 */
#define MAX_SCANNING_HEIGHT                 3540	/* just for A4 */

/*#define DEBUG_SAVE_IMAGE*/


extern SANE_Bool g_isScanning;
extern unsigned short g_SWHeight;
extern SANE_Byte * g_lpReadImageHead;
extern unsigned short g_wLineartThreshold;
extern unsigned short * g_pGammaTable;
extern SCANSOURCE g_ssScanSource;
extern Asic g_chip;

void MustScanner_Init (void);
SANE_Bool MustScanner_PowerControl (SANE_Bool isLampOn, SANE_Bool isTALampOn);
SANE_Bool MustScanner_BackHome (void);
SANE_Bool MustScanner_GetRows (SANE_Byte * lpBlock, unsigned short * Rows,
			       SANE_Bool isOrderInvert);
SANE_Bool MustScanner_ScanSuggest (PTARGETIMAGE pTarget);
SANE_Bool MustScanner_StopScan (void);
SANE_Bool MustScanner_PrepareScan (void);
SANE_Bool MustScanner_Reset (SCANSOURCE ssScanSource);
SANE_Bool MustScanner_SetupScan (TARGETIMAGE *pTarget);


#endif
