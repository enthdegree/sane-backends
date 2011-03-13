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

typedef SANE_Byte SCANSOURCE;
#define SS_Reflective	0x00
#define SS_Positive	0x01
#define SS_Negative	0x02

typedef unsigned short RGBORDER;
#define RO_RGB	0x00
#define RO_BGR	0x01

typedef unsigned char SCANTYPE;
#define ST_Reflective	0x00
#define ST_Transparent	0x01

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
  unsigned int dwLineByteWidth;
  unsigned int dwLength;
} GETPARAMETERS, *LPGETPARAMETERS;

typedef struct
{
  unsigned short x1;
  unsigned short y1;
  unsigned short x2;
  unsigned short y2;
} FRAME, *LPFRAME;

typedef struct
{
  FRAME fmArea;
  unsigned short wTargetDPI;
  COLORMODE cmColorMode;
  unsigned short wLinearThreshold;	/* threshold for line art mode */
  SCANSOURCE ssScanSource;
  unsigned short * pGammaTable;
} SETPARAMETERS, *LPSETPARAMETERS;

typedef struct
{
  RGBORDER roRgbOrder;
  unsigned short wWantedLineNum;
  unsigned short wXferedLineNum;
  SANE_Byte * pBuffer;
} IMAGEROWS, *LPIMAGEROWS;

typedef struct
{
  COLORMODE cmColorMode;
  unsigned short wDpi;
  unsigned short wX;
  unsigned short wY;
  unsigned short wWidth;
  unsigned short wHeight;
  SCANSOURCE ssScanSource;
} TARGETIMAGE, *PTARGETIMAGE;

typedef struct
{
  COLORMODE cmScanMode;
  unsigned short wXDpi;
  unsigned short wYDpi;
  unsigned short wX;
  unsigned short wY;
  unsigned short wWidth;
  unsigned short wHeight;
  unsigned int dwBytesPerRow;
} SUGGESTSETTING, *PSUGGESTSETTING;


#define R_GAIN                          0
#define G_GAIN                          0
#define B_GAIN                          0
#define R_DIRECTION                     DIR_POSITIVE
#define G_DIRECTION                     DIR_POSITIVE
#define B_DIRECTION                     DIR_POSITIVE

/* used for adjusting the AD offset */

/* for Reflective */
#define REFL_WHITE_MAX_LEVEL            220
#define REFL_WHITE_MIN_LEVEL            210
#define REFL_MAX_LEVEL_RANGE            210
#define REFL_MIN_LEVEL_RANGE            190

/* for Transparent */
#define TRAN_WHITE_MAX_LEVEL            220
#define TRAN_WHITE_MIN_LEVEL            210
#define TRAN_MAX_LEVEL_RANGE            210
#define TRAN_MIN_LEVEL_RANGE            190


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

#define ENABLE_GAMMA
/*#define DEBUG_SAVE_IMAGE*/


static void MustScanner_Init (void);
static SANE_Bool MustScanner_GetScannerState (void);
static SANE_Bool MustScanner_PowerControl (SANE_Bool isLampOn,
					   SANE_Bool isTALampOn);
static SANE_Bool MustScanner_BackHome (void);
static SANE_Bool MustScanner_Prepare (SCANSOURCE ssScanSource);
static unsigned short MustScanner_FiltLower (unsigned short * pSort,
					     unsigned short TotalCount,
					     unsigned short LowCount,
					     unsigned short HighCount);
static SANE_Bool MustScanner_GetRgb48BitLine (SANE_Byte * lpLine,
	SANE_Bool isOrderInvert, unsigned short * wLinesCount);
static SANE_Bool MustScanner_GetRgb48BitLine1200DPI (SANE_Byte * lpLine,
	SANE_Bool isOrderInvert, unsigned short * wLinesCount);
static SANE_Bool MustScanner_GetRgb24BitLine (SANE_Byte * lpLine,
	SANE_Bool isOrderInvert, unsigned short * wLinesCount);
static SANE_Bool MustScanner_GetRgb24BitLine1200DPI (SANE_Byte * lpLine,
	SANE_Bool isOrderInvert, unsigned short * wLinesCount);
static SANE_Bool MustScanner_GetMono16BitLine (SANE_Byte * lpLine,
	unsigned short * wLinesCount);
static SANE_Bool MustScanner_GetMono16BitLine1200DPI (SANE_Byte * lpLine,
	unsigned short * wLinesCount);
static SANE_Bool MustScanner_GetMono8BitLine (SANE_Byte * lpLine,
	unsigned short * wLinesCount);
static SANE_Bool MustScanner_GetMono8BitLine1200DPI (SANE_Byte * lpLine,
	unsigned short * wLinesCount);
static SANE_Bool MustScanner_GetMono1BitLine (SANE_Byte * lpLine,
	unsigned short * wLinesCount);
static SANE_Bool MustScanner_GetMono1BitLine1200DPI (SANE_Byte * lpLine,
	unsigned short * wLinesCount);
static void *MustScanner_ReadDataFromScanner (void * dummy);
static void MustScanner_PrepareCalculateMaxMin (unsigned short wResolution);
static SANE_Bool MustScanner_CalculateMaxMin (SANE_Byte * pBuffer,
					      unsigned short * lpMaxValue,
					      unsigned short * lpMinValue);
static SANE_Bool MustScanner_ScanSuggest (PTARGETIMAGE pTarget,
					  PSUGGESTSETTING pSuggest);
static SANE_Bool MustScanner_StopScan (void);
static SANE_Bool MustScanner_PrepareScan (void);
static SANE_Bool MustScanner_GetRows (SANE_Byte * lpBlock,
				      unsigned short * Rows,
				      SANE_Bool isOrderInvert);


#endif
