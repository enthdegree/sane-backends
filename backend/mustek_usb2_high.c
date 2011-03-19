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

#include <pthread.h>		/* HOLD */
#include <stdlib.h>

#include "mustek_usb2_asic.c"

#include "mustek_usb2_high.h"

/* HOLD: these global variables should go to scanner structure */

static SANE_Bool g_bOpened;
static SANE_Bool g_bPrepared;
static SANE_Bool g_isCanceled;
static SANE_Bool g_bFirstReadImage;
static SANE_Bool g_isScanning;
static SANE_Bool g_isSelfGamma;

static SANE_Byte g_bScanBits;
static SANE_Byte *g_lpReadImageHead;

static const unsigned short s_wOpticalDpi[] = { 1200, 600, 300, 150, 75, 0 };
static unsigned short g_X;
static unsigned short g_Y;
static unsigned short g_Width;
static unsigned short g_Height;
static unsigned short g_XDpi;
static unsigned short g_YDpi;
static unsigned short g_SWWidth;
static unsigned short g_SWHeight;
static unsigned short g_wPixelDistance;		/* even & odd sensor problem */
static unsigned short g_wLineDistance;
static unsigned short g_wScanLinesPerBlock;
static unsigned short g_wLineartThreshold;

static unsigned int g_wtheReadyLines;
static unsigned int g_wMaxScanLines;
static unsigned int g_dwScannedTotalLines;
static unsigned int g_dwImageBufferSize;
static unsigned int g_BytesPerRow;
static unsigned int g_SWBytesPerRow;
static unsigned int g_dwCalibrationSize;
static unsigned int g_dwBufferSize;

static unsigned int g_dwTotalTotalXferLines;

static unsigned short *g_pGammaTable;

static pthread_t g_threadid_readimage;

static COLORMODE g_ScanMode;
static TARGETIMAGE g_tiTarget;
static SCANTYPE g_ScanType = ST_Reflective;
static SCANSOURCE g_ssScanSource;

static SUGGESTSETTING g_ssSuggest;
static Asic g_chip;

static int g_nSecLength, g_nDarkSecLength;
static int g_nSecNum, g_nDarkSecNum;
static unsigned short g_wCalWidth;
static unsigned short g_wDarkCalWidth;
static int g_nPowerNum;
static unsigned short g_wStartPosition;

static pthread_mutex_t g_scannedLinesMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_readyLinesMutex = PTHREAD_MUTEX_INITIALIZER;

/* for modifying the last point */
static SANE_Bool g_bIsFirstReadBefData = SANE_TRUE;
static SANE_Byte * g_lpBefLineImageData;
static unsigned int g_dwAlreadyGetLines;

static SANE_Byte QBET4 (SANE_Byte A, SANE_Byte B);
static unsigned int GetScannedLines (void);
static unsigned int GetReadyLines (void);
static void AddScannedLines (unsigned short wAddLines);
static void AddReadyLines (void);
static void ModifyLinePoint (SANE_Byte * lpImageData,
			     SANE_Byte * lpImageDataBefore,
			     unsigned int dwBytesPerLine,
			     unsigned int dwLinesCount,
			     unsigned short wPixDistance,
			     unsigned short wModPtCount);

#include "mustek_usb2_reflective.c"
#include "mustek_usb2_transparent.c"


static void
MustScanner_Init (void)
{
  DBG (DBG_FUNC, "MustScanner_Init: Call in\n");

  Asic_Initialize (&g_chip);

  g_dwImageBufferSize = 24 * 1024 * 1024;
  g_dwBufferSize = 64 * 1024;
  g_dwCalibrationSize = 64 * 1024;
  g_lpReadImageHead = NULL;

  g_isCanceled = SANE_FALSE;
  g_bFirstReadImage = SANE_TRUE;
  g_bOpened = SANE_FALSE;
  g_bPrepared = SANE_FALSE;

  g_isScanning = SANE_FALSE;
  g_isSelfGamma = SANE_FALSE;
  g_pGammaTable = NULL;

  g_ssScanSource = SS_Reflective;

  DBG (DBG_FUNC, "MustScanner_Init: leave MustScanner_Init\n");
}

static SANE_Bool
MustScanner_GetScannerState (void)
{
  if (SANE_STATUS_GOOD != Asic_Open (&g_chip))
    {
      DBG (DBG_FUNC, "MustScanner_GetScannerState: Asic_Open return error\n");
      return SANE_FALSE;
    }

  Asic_Close (&g_chip);
  return SANE_TRUE;
}

static SANE_Bool
MustScanner_PowerControl (SANE_Bool isLampOn, SANE_Bool isTALampOn)
{
  SANE_Bool hasTA;

  DBG (DBG_FUNC, "MustScanner_PowerControl: Call in\n");

  if (SANE_STATUS_GOOD != Asic_Open (&g_chip))
    {
      DBG (DBG_FUNC, "MustScanner_PowerControl: Asic_Open return error\n");
      return SANE_FALSE;
    }

  if (SANE_STATUS_GOOD != Asic_TurnLamp (&g_chip, isLampOn))
    {
      DBG (DBG_FUNC,
	   "MustScanner_PowerControl: Asic_TurnLamp return error\n");
      return SANE_FALSE;
    }

  if (SANE_STATUS_GOOD != Asic_IsTAConnected (&g_chip, &hasTA))
    {
      DBG (DBG_FUNC,
	   "MustScanner_PowerControl: Asic_IsTAConnected return error\n");
      return SANE_FALSE;
    }

  if (hasTA && (SANE_STATUS_GOOD != Asic_TurnTA (&g_chip, isTALampOn)))
    {
      DBG (DBG_FUNC, "MustScanner_PowerControl: Asic_TurnTA return error\n");
      return SANE_FALSE;
    }

  Asic_Close (&g_chip);

  DBG (DBG_FUNC,
       "MustScanner_PowerControl: leave MustScanner_PowerControl\n");
  return SANE_TRUE;
}

static SANE_Bool
MustScanner_BackHome (void)
{
  DBG (DBG_FUNC, "MustScanner_BackHome: call in\n");

  if (SANE_STATUS_GOOD != Asic_Open (&g_chip))
    {
      DBG (DBG_FUNC, "MustScanner_BackHome: Asic_Open return error\n");
      return SANE_FALSE;
    }

  if (SANE_STATUS_GOOD != Asic_CarriageHome (&g_chip))
    {
      DBG (DBG_FUNC,
	   "MustScanner_BackHome: Asic_CarriageHome return error\n");
      return SANE_FALSE;
    }

  Asic_Close (&g_chip);

  DBG (DBG_FUNC, "MustScanner_BackHome: leave MustScanner_BackHome\n");
  return SANE_TRUE;
}

static SANE_Bool
MustScanner_Prepare (SCANSOURCE ssScanSource)
{
  DBG (DBG_FUNC, "MustScanner_Prepare: call in\n");

  if (SANE_STATUS_GOOD != Asic_Open (&g_chip))
    {
      DBG (DBG_FUNC, "MustScanner_Prepare: Asic_Open return error\n");
      return SANE_FALSE;
    }

  if (SS_Reflective == ssScanSource)
    {
      DBG (DBG_FUNC, "MustScanner_Prepare: ScanSource is SS_Reflective\n");
      if (SANE_STATUS_GOOD != Asic_TurnLamp (&g_chip, SANE_TRUE))
	{
	  DBG (DBG_FUNC, "MustScanner_Prepare: Asic_TurnLamp return error\n");
	  return SANE_FALSE;
	}

      g_chip.lsLightSource = LS_REFLECTIVE;
    }
  else if (SS_Positive == ssScanSource)
    {
      DBG (DBG_FUNC, "MustScanner_Prepare: ScanSource is SS_Positive\n");
      if (SANE_STATUS_GOOD != Asic_TurnTA (&g_chip, SANE_TRUE))
	{
	  DBG (DBG_FUNC, "MustScanner_Prepare: Asic_TurnTA return error\n");
	  return SANE_FALSE;
	}

      g_chip.lsLightSource = LS_POSITIVE;
    }
  else if (SS_Negative == ssScanSource)
    {
      DBG (DBG_FUNC, "MustScanner_Prepare: ScanSource is SS_Negative\n");
      if (SANE_STATUS_GOOD != Asic_TurnTA (&g_chip, SANE_TRUE))
	{
	  DBG (DBG_FUNC, "MustScanner_Prepare: Asic_TurnTA return error\n");
	  return SANE_FALSE;
	}

      g_chip.lsLightSource = LS_NEGATIVE;
    }

  Asic_Close (&g_chip);
  g_bPrepared = SANE_TRUE;

  DBG (DBG_FUNC, "MustScanner_Prepare: leave MustScanner_Prepare\n");
  return SANE_TRUE;
}

static unsigned short
MustScanner_FiltLower (unsigned short * pSort, unsigned short TotalCount,
		       unsigned short LowCount, unsigned short HighCount)
{
  unsigned short Bound = TotalCount - 1;
  unsigned short LeftCount = HighCount - LowCount;
  int Temp;
  unsigned int Sum = 0;
  unsigned short i, j;

  for (i = 0; i < Bound; i++)
    {
      for (j = 0; j < Bound - i; j++)
	{
	  if (pSort[j + 1] > pSort[j])
	    {
	      Temp = pSort[j];
	      pSort[j] = pSort[j + 1];
	      pSort[j + 1] = Temp;
	    }
	}
    }

  for (i = 0; i < LeftCount; i++)
    Sum += pSort[i + LowCount];
  return (unsigned short) (Sum / LeftCount);
}

static void
SetPixel48Bit (SANE_Byte * lpLine, unsigned short wRTempData,
	       unsigned short wGTempData, unsigned short wBTempData,
	       SANE_Bool isOrderInvert)
{
  if (!isOrderInvert)
    {
      lpLine[0] = LOBYTE (g_pGammaTable[wRTempData]);
      lpLine[1] = HIBYTE (g_pGammaTable[wRTempData]);
      lpLine[2] = LOBYTE (g_pGammaTable[wGTempData + 0x10000]);
      lpLine[3] = HIBYTE (g_pGammaTable[wGTempData + 0x10000]);
      lpLine[4] = LOBYTE (g_pGammaTable[wBTempData + 0x20000]);
      lpLine[5] = HIBYTE (g_pGammaTable[wBTempData + 0x20000]);
    }
  else
    {
      lpLine[4] = LOBYTE (g_pGammaTable[wRTempData]);
      lpLine[5] = HIBYTE (g_pGammaTable[wRTempData]);
      lpLine[2] = LOBYTE (g_pGammaTable[wGTempData + 0x10000]);
      lpLine[3] = HIBYTE (g_pGammaTable[wGTempData + 0x10000]);
      lpLine[0] = LOBYTE (g_pGammaTable[wBTempData + 0x20000]);
      lpLine[1] = HIBYTE (g_pGammaTable[wBTempData + 0x20000]);
    }
}

static void
GetRgb48BitLineHalf (SANE_Byte * lpLine, SANE_Bool isOrderInvert)
{
  unsigned short wRLinePos, wGLinePos, wBLinePos;
  unsigned short wRTempData, wGTempData, wBTempData;
  unsigned short i;

  wRLinePos = g_wtheReadyLines;
  wGLinePos = g_wtheReadyLines - g_wLineDistance;
  wBLinePos = g_wtheReadyLines - g_wLineDistance * 2;
  wRLinePos = (wRLinePos % g_wMaxScanLines) * g_BytesPerRow;
  wGLinePos = (wGLinePos % g_wMaxScanLines) * g_BytesPerRow;
  wBLinePos = (wBLinePos % g_wMaxScanLines) * g_BytesPerRow;

  for (i = 0; i < g_SWWidth; i++)
    {
      wRTempData = g_lpReadImageHead[wRLinePos + i * 6] |
	(g_lpReadImageHead[wRLinePos + i * 6 + 1] << 8);
      wGTempData = g_lpReadImageHead[wGLinePos + i * 6 + 2] |
	(g_lpReadImageHead[wGLinePos + i * 6 + 3] << 8);
      wBTempData = g_lpReadImageHead[wBLinePos + i * 6 + 4] |
	(g_lpReadImageHead[wBLinePos + i * 6 + 5] << 8);

      SetPixel48Bit (lpLine + (i * 6), wRTempData, wGTempData, wBTempData,
		     isOrderInvert);
    }
}

static void
GetRgb48BitLineFull (SANE_Byte * lpLine, SANE_Bool isOrderInvert)
{
  unsigned short wRLinePosOdd, wGLinePosOdd, wBLinePosOdd;
  unsigned short wRLinePosEven, wGLinePosEven, wBLinePosEven;
  unsigned int dwRTempData, dwGTempData, dwBTempData;
  unsigned short i = 0;

  if (ST_Reflective == g_ScanType)
    {
      wRLinePosOdd = g_wtheReadyLines - g_wPixelDistance;
      wGLinePosOdd = g_wtheReadyLines - g_wLineDistance - g_wPixelDistance;
      wBLinePosOdd = g_wtheReadyLines - g_wLineDistance * 2 - g_wPixelDistance;
      wRLinePosEven = g_wtheReadyLines;
      wGLinePosEven = g_wtheReadyLines - g_wLineDistance;
      wBLinePosEven = g_wtheReadyLines - g_wLineDistance * 2;
    }
  else
    {
      wRLinePosOdd = g_wtheReadyLines;
      wGLinePosOdd = g_wtheReadyLines - g_wLineDistance;
      wBLinePosOdd = g_wtheReadyLines - g_wLineDistance * 2;
      wRLinePosEven = g_wtheReadyLines - g_wPixelDistance;
      wGLinePosEven = g_wtheReadyLines - g_wLineDistance - g_wPixelDistance;
      wBLinePosEven = g_wtheReadyLines - g_wLineDistance * 2 - g_wPixelDistance;
    }
  wRLinePosOdd = (wRLinePosOdd % g_wMaxScanLines) * g_BytesPerRow;
  wGLinePosOdd = (wGLinePosOdd % g_wMaxScanLines) * g_BytesPerRow;
  wBLinePosOdd = (wBLinePosOdd % g_wMaxScanLines) * g_BytesPerRow;
  wRLinePosEven = (wRLinePosEven % g_wMaxScanLines) * g_BytesPerRow;
  wGLinePosEven = (wGLinePosEven % g_wMaxScanLines) * g_BytesPerRow;
  wBLinePosEven = (wBLinePosEven % g_wMaxScanLines) * g_BytesPerRow;

  while (i < g_SWWidth)
    {
      if ((i + 1) >= g_SWWidth)
        break;

      dwRTempData = g_lpReadImageHead[wRLinePosOdd + i * 6] |
	(g_lpReadImageHead[wRLinePosOdd + i * 6 + 1] << 8);
      dwRTempData += g_lpReadImageHead[wRLinePosEven + (i + 1) * 6] |
	(g_lpReadImageHead[wRLinePosEven + (i + 1) * 6 + 1] << 8);
      dwRTempData /= 2;

      dwGTempData = g_lpReadImageHead[wGLinePosOdd + i * 6 + 2] |
	(g_lpReadImageHead[wGLinePosOdd + i * 6 + 3] << 8);
      dwGTempData += g_lpReadImageHead[wGLinePosEven + (i + 1) * 6 + 2] |
	(g_lpReadImageHead[wGLinePosEven + (i + 1) * 6 + 3] << 8);
      dwGTempData /= 2;

      dwBTempData = g_lpReadImageHead[wBLinePosOdd + i * 6 + 4] |
	(g_lpReadImageHead[wBLinePosOdd + i * 6 + 5] << 8);
      dwBTempData += g_lpReadImageHead[wBLinePosEven + (i + 1) * 6 + 4] |
	(g_lpReadImageHead[wBLinePosEven + (i + 1) * 6 + 5] << 8);
      dwBTempData /= 2;

      SetPixel48Bit (lpLine + (i * 6), dwRTempData, dwGTempData, dwBTempData,
		     isOrderInvert);
      i++;

      dwRTempData = g_lpReadImageHead[wRLinePosEven + i * 6] |
	(g_lpReadImageHead[wRLinePosEven + i * 6 + 1] << 8);
      dwRTempData += g_lpReadImageHead[wRLinePosOdd + (i + 1) * 6] |
	(g_lpReadImageHead[wRLinePosOdd + (i + 1) * 6 + 1] << 8);
      dwRTempData /= 2;

      dwGTempData = g_lpReadImageHead[wGLinePosEven + i * 6 + 2] |
	(g_lpReadImageHead[wGLinePosEven + i * 6 + 3] << 8);
      dwGTempData += g_lpReadImageHead[wGLinePosOdd + (i + 1) * 6 + 2] |
	(g_lpReadImageHead[wGLinePosOdd + (i + 1) * 6 + 3] << 8);
      dwGTempData /= 2;

      dwBTempData = g_lpReadImageHead[wBLinePosEven + i * 6 + 4] |
	(g_lpReadImageHead[wBLinePosEven + i * 6 + 5] << 8);
      dwBTempData += g_lpReadImageHead[wBLinePosOdd + (i + 1) * 6 + 4] |
	(g_lpReadImageHead[wBLinePosOdd + (i + 1) * 6 + 5] << 8);
      dwBTempData /= 2;

      SetPixel48Bit (lpLine + (i * 6), dwRTempData, dwGTempData, dwBTempData,
		     isOrderInvert);
      i++;
    }
}

static void
SetPixel24Bit (SANE_Byte * lpLine, unsigned short tempR, unsigned short tempG,
	       unsigned short tempB, SANE_Bool isOrderInvert)
{
  if (!isOrderInvert)
    {
      lpLine[0] = (SANE_Byte) g_pGammaTable[tempR];
      lpLine[1] = (SANE_Byte) g_pGammaTable[4096 + tempG];
      lpLine[2] = (SANE_Byte) g_pGammaTable[8192 + tempB];
    }
  else
    {
      lpLine[2] = (SANE_Byte) g_pGammaTable[tempR];
      lpLine[1] = (SANE_Byte) g_pGammaTable[4096 + tempG];
      lpLine[0] = (SANE_Byte) g_pGammaTable[8192 + tempB];
    }
}

static void
GetRgb24BitLineHalf (SANE_Byte * lpLine, SANE_Bool isOrderInvert)
{
  unsigned short wRLinePos, wGLinePos, wBLinePos;
  unsigned short wRed, wGreen, wBlue;
  unsigned short tempR, tempG, tempB;
  unsigned short i;

  wRLinePos = g_wtheReadyLines;
  wGLinePos = g_wtheReadyLines - g_wLineDistance;
  wBLinePos = g_wtheReadyLines - g_wLineDistance * 2;
  wRLinePos = (wRLinePos % g_wMaxScanLines) * g_BytesPerRow;
  wGLinePos = (wGLinePos % g_wMaxScanLines) * g_BytesPerRow;
  wBLinePos = (wBLinePos % g_wMaxScanLines) * g_BytesPerRow;  

  for (i = 0; i < g_SWWidth; i++)
    {
      wRed = g_lpReadImageHead[wRLinePos + i * 3];
      wRed += g_lpReadImageHead[wRLinePos + (i + 1) * 3];
      wRed /= 2;

      wGreen = g_lpReadImageHead[wGLinePos + i * 3 + 1];
      wGreen += g_lpReadImageHead[wGLinePos + (i + 1) * 3 + 1];
      wGreen /= 2;

      wBlue = g_lpReadImageHead[wBLinePos + i * 3 + 2];
      wBlue += g_lpReadImageHead[wBLinePos + (i + 1) * 3 + 2];
      wBlue /= 2;

      tempR = (wRed << 4) | QBET4 (wBlue, wGreen);
      tempG = (wGreen << 4) | QBET4 (wRed, wBlue);
      tempB = (wBlue << 4) | QBET4 (wGreen, wRed);

      SetPixel24Bit (lpLine + (i * 3), tempR, tempG, tempB, isOrderInvert);
    }
}

static void
GetRgb24BitLineFull (SANE_Byte * lpLine, SANE_Bool isOrderInvert)
{
  unsigned short wRLinePosOdd, wGLinePosOdd, wBLinePosOdd;
  unsigned short wRLinePosEven, wGLinePosEven, wBLinePosEven;
  unsigned short wRed, wGreen, wBlue;
  unsigned short tempR, tempG, tempB;
  unsigned short i = 0;

  if (ST_Reflective == g_ScanType)
    {
      wRLinePosOdd = g_wtheReadyLines - g_wPixelDistance;
      wGLinePosOdd = g_wtheReadyLines - g_wLineDistance - g_wPixelDistance;
      wBLinePosOdd = g_wtheReadyLines - g_wLineDistance * 2 - g_wPixelDistance;
      wRLinePosEven = g_wtheReadyLines;
      wGLinePosEven = g_wtheReadyLines - g_wLineDistance;
      wBLinePosEven = g_wtheReadyLines - g_wLineDistance * 2;
    }
  else
    {
      wRLinePosOdd = g_wtheReadyLines;
      wGLinePosOdd = g_wtheReadyLines - g_wLineDistance;
      wBLinePosOdd = g_wtheReadyLines - g_wLineDistance * 2;
      wRLinePosEven = g_wtheReadyLines - g_wPixelDistance;
      wGLinePosEven = g_wtheReadyLines - g_wLineDistance - g_wPixelDistance;
      wBLinePosEven = g_wtheReadyLines - g_wLineDistance * 2 - g_wPixelDistance;
    }
  wRLinePosOdd = (wRLinePosOdd % g_wMaxScanLines) * g_BytesPerRow;
  wGLinePosOdd = (wGLinePosOdd % g_wMaxScanLines) * g_BytesPerRow;
  wBLinePosOdd = (wBLinePosOdd % g_wMaxScanLines) * g_BytesPerRow;
  wRLinePosEven = (wRLinePosEven % g_wMaxScanLines) * g_BytesPerRow;
  wGLinePosEven = (wGLinePosEven % g_wMaxScanLines) * g_BytesPerRow;
  wBLinePosEven = (wBLinePosEven % g_wMaxScanLines) * g_BytesPerRow;

  while (i < g_SWWidth)
    {
      if ((i + 1) >= g_SWWidth)
        break;

      wRed = g_lpReadImageHead[wRLinePosOdd + i * 3];
      wRed += g_lpReadImageHead[wRLinePosEven + (i + 1) * 3];
      wRed /= 2;

      wGreen = g_lpReadImageHead[wGLinePosOdd + i * 3 + 1];
      wGreen += g_lpReadImageHead[wGLinePosEven + (i + 1) * 3 + 1];
      wGreen /= 2;

      wBlue = g_lpReadImageHead[wBLinePosOdd + i * 3 + 2];
      wBlue += g_lpReadImageHead[wBLinePosEven + (i + 1) * 3 + 2];
      wBlue /= 2;

      tempR = (wRed << 4) | QBET4 (wBlue, wGreen);
      tempG = (wGreen << 4) | QBET4 (wRed, wBlue);
      tempB = (wBlue << 4) | QBET4 (wGreen, wRed);

      SetPixel24Bit (lpLine + (i * 3), tempR, tempG, tempB, isOrderInvert);
      i++;

      wRed = g_lpReadImageHead[wRLinePosEven + i * 3];
      wRed += g_lpReadImageHead[wRLinePosOdd + (i + 1) * 3];
      wRed /= 2;

      wGreen = g_lpReadImageHead[wGLinePosEven + i * 3 + 1];
      wGreen += g_lpReadImageHead[wGLinePosOdd + (i + 1) * 3 + 1];
      wGreen /= 2;

      wBlue = g_lpReadImageHead[wBLinePosEven + i * 3 + 2];
      wBlue += g_lpReadImageHead[wBLinePosOdd + (i + 1) * 3 + 2];
      wBlue /= 2;

      tempR = (wRed << 4) | QBET4 (wBlue, wGreen);
      tempG = (wGreen << 4) | QBET4 (wRed, wBlue);
      tempB = (wBlue << 4) | QBET4 (wGreen, wRed);

      SetPixel24Bit (lpLine + (i * 3), tempR, tempG, tempB, isOrderInvert);
      i++;
    }
}

static void
GetMono16BitLineHalf (SANE_Byte * lpLine,
		      SANE_Bool __sane_unused__ isOrderInvert)
{
  unsigned int dwTempData;
  unsigned short wLinePos;
  unsigned short i;

  wLinePos = (g_wtheReadyLines % g_wMaxScanLines) * g_BytesPerRow;

  for (i = 0; i < g_SWWidth; i++)
    {
      dwTempData = g_lpReadImageHead[wLinePos + i * 2] |
		  (g_lpReadImageHead[wLinePos + i * 2 + 1] << 8);
      lpLine[i * 2] = LOBYTE (g_pGammaTable[dwTempData]);
      lpLine[i * 2 + 1] = HIBYTE (g_pGammaTable[dwTempData]);
    }
}

static void
GetMono16BitLineFull (SANE_Byte * lpLine,
		      SANE_Bool __sane_unused__ isOrderInvert)
{
  unsigned int dwTempData;
  unsigned short wLinePosOdd;
  unsigned short wLinePosEven;
  unsigned short i = 0;

  if (ST_Reflective == g_ScanType)
    {
      wLinePosOdd = g_wtheReadyLines - g_wPixelDistance;
      wLinePosEven = g_wtheReadyLines;
    }
  else
    {
      wLinePosOdd = g_wtheReadyLines;
      wLinePosEven = g_wtheReadyLines - g_wPixelDistance;
    }
  wLinePosOdd = (wLinePosOdd % g_wMaxScanLines) * g_BytesPerRow;
  wLinePosEven = (wLinePosEven % g_wMaxScanLines) * g_BytesPerRow;

  while (i < g_SWWidth)
    {
      if ((i + 1) >= g_SWWidth)
        break;

      dwTempData = (unsigned int) g_lpReadImageHead[wLinePosOdd + i * 2];
      dwTempData += (unsigned int)
		    g_lpReadImageHead[wLinePosOdd + i * 2 + 1] << 8;
      dwTempData += (unsigned int)
		    g_lpReadImageHead[wLinePosEven + (i + 1) * 2];
      dwTempData += (unsigned int)
		    g_lpReadImageHead[wLinePosEven + (i + 1) * 2 + 1] << 8;
      dwTempData /= 2;

      lpLine[i * 2] = LOBYTE (g_pGammaTable[dwTempData]);
      lpLine[i * 2 + 1] = HIBYTE (g_pGammaTable[dwTempData]);
      i++;

      dwTempData = (unsigned int) g_lpReadImageHead[wLinePosEven + i * 2];
      dwTempData += (unsigned int)
		    g_lpReadImageHead[wLinePosEven + i * 2 + 1] << 8;
      dwTempData += (unsigned int)
		    g_lpReadImageHead[wLinePosOdd + (i + 1) * 2];
      dwTempData += (unsigned int)
		    g_lpReadImageHead[wLinePosOdd + (i + 1) * 2 + 1] << 8;
      dwTempData /= 2;

      lpLine[i * 2] = LOBYTE (g_pGammaTable[dwTempData]);
      lpLine[i * 2 + 1] = HIBYTE (g_pGammaTable[dwTempData]);
      i++;
    }
}

static void
GetMono8BitLineHalf (SANE_Byte * lpLine,
		     SANE_Bool __sane_unused__ isOrderInvert)
{
  unsigned int dwTempData;
  unsigned short wLinePos;
  unsigned short i;

  wLinePos = (g_wtheReadyLines % g_wMaxScanLines) * g_BytesPerRow;

  for (i = 0; i < g_SWWidth; i++)
    {
      dwTempData = (g_lpReadImageHead[wLinePos + i] << 4) | (rand () & 0x0f);
      lpLine[i] = (SANE_Byte) g_pGammaTable[dwTempData];
    }
}

static void
GetMono8BitLineFull (SANE_Byte * lpLine,
		     SANE_Bool __sane_unused__ isOrderInvert)
{
  unsigned short wLinePosOdd;
  unsigned short wLinePosEven;
  unsigned short wGray;
  unsigned short i = 0;

  if (ST_Reflective == g_ScanType)
    {
      wLinePosOdd = (g_wtheReadyLines - g_wPixelDistance) % g_wMaxScanLines;
      wLinePosEven = (g_wtheReadyLines) % g_wMaxScanLines;
    }
  else
    {
      wLinePosOdd = (g_wtheReadyLines) % g_wMaxScanLines;
      wLinePosEven = (g_wtheReadyLines - g_wPixelDistance) % g_wMaxScanLines;
    }
  wLinePosOdd = (wLinePosOdd % g_wMaxScanLines) * g_BytesPerRow;
  wLinePosEven = (wLinePosEven % g_wMaxScanLines) * g_BytesPerRow;

  while (i < g_SWWidth)
    {
      if ((i + 1) >= g_SWWidth)
        break;

      wGray = g_lpReadImageHead[wLinePosOdd + i];
      wGray += g_lpReadImageHead[wLinePosEven + i + 1];
      wGray /= 2;

      lpLine[i] = (SANE_Byte) g_pGammaTable[(wGray << 4) | (rand () & 0x0f)];
      i++;

      wGray = g_lpReadImageHead[wLinePosEven + i];
      wGray += g_lpReadImageHead[wLinePosOdd + i + 1];
      wGray /= 2;

      lpLine[i] = (SANE_Byte) g_pGammaTable[(wGray << 4) | (rand () & 0x0f)];
      i++;
    }
}

static void
GetMono1BitLineHalf (SANE_Byte * lpLine,
		     SANE_Bool __sane_unused__ isOrderInvert)
{
  unsigned short wLinePos;
  unsigned short i;

  wLinePos = (g_wtheReadyLines % g_wMaxScanLines) * g_BytesPerRow;

  for (i = 0; i < g_SWWidth; i++)
    {
      if (g_lpReadImageHead[wLinePos + i] > g_wLineartThreshold)
	lpLine[i / 8] |= 0x80 >> (i % 8);
    }
}

static void
GetMono1BitLineFull (SANE_Byte * lpLine,
		     SANE_Bool __sane_unused__ isOrderInvert)
{
  unsigned short wLinePosOdd;
  unsigned short wLinePosEven;
  unsigned short i = 0;

  if (ST_Reflective == g_ScanType)
    {
      wLinePosOdd = (g_wtheReadyLines - g_wPixelDistance) % g_wMaxScanLines;
      wLinePosEven = (g_wtheReadyLines) % g_wMaxScanLines;
    }
  else
    {
      wLinePosOdd = (g_wtheReadyLines) % g_wMaxScanLines;
      wLinePosEven = (g_wtheReadyLines - g_wPixelDistance) % g_wMaxScanLines;
    }
  wLinePosOdd = (wLinePosOdd % g_wMaxScanLines) * g_BytesPerRow;
  wLinePosEven = (wLinePosEven % g_wMaxScanLines) * g_BytesPerRow;

  while (i < g_SWWidth)
    {
      if ((i + 1) >= g_SWWidth)
        break;

      if (g_lpReadImageHead[wLinePosOdd + i] > g_wLineartThreshold)
	lpLine[i / 8] |= 0x80 >> (i % 8);
      i++;

      if (g_lpReadImageHead[wLinePosEven + i] > g_wLineartThreshold)
	lpLine[i / 8] |= 0x80 >> (i % 8);
      i++;
    }
}

static SANE_Bool
MustScanner_GetLine (SANE_Byte * lpLine, unsigned short * wLinesCount,
		     unsigned int dwLineIncrement,
		     void (*pFunc)(SANE_Byte *, SANE_Bool),
		     SANE_Bool isOrderInvert, SANE_Bool fixEvenOdd)
{
  unsigned short wWantedTotalLines;
  unsigned short TotalXferLines = 0;
  SANE_Byte *lpFirstLine = lpLine;

  DBG (DBG_FUNC, "MustScanner_GetLine: call in\n");

  g_isCanceled = SANE_FALSE;
  g_isScanning = SANE_TRUE;
  wWantedTotalLines = *wLinesCount;

  if (g_bFirstReadImage)
    {
      pthread_create (&g_threadid_readimage, NULL,
		      MustScanner_ReadDataFromScanner, NULL);
      DBG (DBG_FUNC, "MustScanner_GetLine: thread create\n");
      g_bFirstReadImage = SANE_FALSE;
    }

  while (TotalXferLines < wWantedTotalLines)
    {
      if (g_dwTotalTotalXferLines >= g_SWHeight)
	{
	  pthread_cancel (g_threadid_readimage);
	  pthread_join (g_threadid_readimage, NULL);
	  DBG (DBG_FUNC, "MustScanner_GetLine: thread exit\n");

	  *wLinesCount = TotalXferLines;
	  g_isScanning = SANE_FALSE;
	  return SANE_TRUE;
	}

      if (GetScannedLines () > g_wtheReadyLines)
	{
	  pFunc (lpLine, isOrderInvert);

	  TotalXferLines++;
	  g_dwTotalTotalXferLines++;
	  lpLine += dwLineIncrement;
	  AddReadyLines ();
	}

      if (g_isCanceled)
	{
	  pthread_cancel (g_threadid_readimage);
	  pthread_join (g_threadid_readimage, NULL);
	  DBG (DBG_FUNC, "MustScanner_GetLine: thread exit\n");
	  break;
	}
    }

  *wLinesCount = TotalXferLines;
  g_isScanning = SANE_FALSE;

  if (fixEvenOdd)
    {
      /* modify the last point */
      if (g_bIsFirstReadBefData)
	{
	  g_lpBefLineImageData = malloc (g_SWBytesPerRow);
	  if (!g_lpBefLineImageData)
	    return SANE_FALSE;
	  memset (g_lpBefLineImageData, 0, g_SWBytesPerRow);
	  memcpy (g_lpBefLineImageData, lpFirstLine, g_SWBytesPerRow);
	  g_bIsFirstReadBefData = SANE_FALSE;
	  g_dwAlreadyGetLines = 0;
	}

      ModifyLinePoint (lpFirstLine, g_lpBefLineImageData, g_SWBytesPerRow,
		       wWantedTotalLines, 1, 4);

      memcpy (g_lpBefLineImageData,
	      lpFirstLine + (wWantedTotalLines - 1) * g_SWBytesPerRow,
	      g_SWBytesPerRow);
      g_dwAlreadyGetLines += wWantedTotalLines;
      if (g_dwAlreadyGetLines >= g_SWHeight)
	{
	  DBG (DBG_FUNC, "MustScanner_GetLine: freeing g_lpBefLineImageData\n");
	  free (g_lpBefLineImageData);
	  g_bIsFirstReadBefData = SANE_TRUE;
	}
    }

  DBG (DBG_FUNC, "MustScanner_GetLine: leave MustScanner_GetLine\n");
  return SANE_TRUE;
}

static SANE_Bool
MustScanner_GetRows (SANE_Byte * lpBlock, unsigned short * Rows,
		     SANE_Bool isOrderInvert)
{
  DBG (DBG_FUNC, "MustScanner_GetRows: call in\n");

  if (!g_bOpened || !g_bPrepared)
    {
      DBG (DBG_FUNC, "MustScanner_GetRows: scanner not opened or prepared\n");
      return SANE_FALSE;
    }

  switch (g_ScanMode)
    {
    case CM_RGB48:
      if (g_XDpi == SENSOR_DPI)
	return MustScanner_GetLine (lpBlock, Rows, g_SWBytesPerRow,
				    GetRgb48BitLineFull, isOrderInvert,
				    SANE_FALSE);
      else
	return MustScanner_GetLine (lpBlock, Rows, g_SWBytesPerRow,
				    GetRgb48BitLineHalf, isOrderInvert,
				    SANE_FALSE);

    case CM_RGB24:
      if (g_XDpi == SENSOR_DPI)
	return MustScanner_GetLine (lpBlock, Rows, g_SWBytesPerRow,
				    GetRgb24BitLineFull, isOrderInvert,
				    SANE_FALSE);
      else
	return MustScanner_GetLine (lpBlock, Rows, g_SWBytesPerRow,
				    GetRgb24BitLineHalf, isOrderInvert,
				    SANE_FALSE);

    case CM_GRAY16:
      if (g_XDpi == SENSOR_DPI)
	return MustScanner_GetLine (lpBlock, Rows, g_SWBytesPerRow,
				    GetMono16BitLineFull, isOrderInvert,
				    SANE_TRUE);
      else
	return MustScanner_GetLine (lpBlock, Rows, g_SWBytesPerRow,
				    GetMono16BitLineHalf, isOrderInvert,
				    SANE_FALSE);

    case CM_GRAY8:
      if (g_XDpi == SENSOR_DPI)
	return MustScanner_GetLine (lpBlock, Rows, g_SWBytesPerRow,
				    GetMono8BitLineFull, isOrderInvert,
				    SANE_TRUE);
      else
	return MustScanner_GetLine (lpBlock, Rows, g_SWBytesPerRow,
				    GetMono8BitLineHalf, isOrderInvert,
				    SANE_FALSE);

    case CM_TEXT:
      memset (lpBlock, 0, *Rows * g_SWWidth / 8);
      if (g_XDpi == SENSOR_DPI)
	return MustScanner_GetLine (lpBlock, Rows, g_SWBytesPerRow / 8,
				    GetMono1BitLineFull, isOrderInvert,
				    SANE_FALSE);
      else
	return MustScanner_GetLine (lpBlock, Rows, g_SWBytesPerRow / 8,
				    GetMono1BitLineHalf, isOrderInvert,
				    SANE_FALSE);
    }

  DBG (DBG_FUNC, "MustScanner_GetRows: leave MustScanner_GetRows\n");
  return SANE_FALSE;
}

static void
MustScanner_PrepareCalculateMaxMin (unsigned short wResolution)
{
  g_wDarkCalWidth = 52;
  if (wResolution <= (SENSOR_DPI / 2))
    {
      g_wCalWidth = (5120 * wResolution / (SENSOR_DPI / 2) + 511) & ~511;
      g_wDarkCalWidth *= wResolution / SENSOR_DPI;

      if (wResolution < 200)
	{
	  g_nPowerNum = 3;
	  g_nSecLength = 8;	/* 2^nPowerNum */
	  /* Dark has at least 2 sections */
	  g_nDarkSecLength = g_wDarkCalWidth / 2;
	}
      else
	{
	  g_nPowerNum = 6;
	  g_nSecLength = 64;	/* 2^nPowerNum */
	  g_nDarkSecLength = g_wDarkCalWidth / 3;
	}
    }
  else
    {
      g_nPowerNum = 6;
      g_nSecLength = 64;	/* 2^nPowerNum */
      g_wCalWidth = 10240;
      g_nDarkSecLength = g_wDarkCalWidth / 5;
    }

  if (g_nDarkSecLength <= 0)
    g_nDarkSecLength = 1;

  g_wStartPosition = 13 * wResolution / SENSOR_DPI;
  g_wCalWidth -= g_wStartPosition;

  /* start of find max value */
  g_nSecNum = (int) (g_wCalWidth / g_nSecLength);

  /* start of find min value */
  g_nDarkSecNum = (int) (g_wDarkCalWidth / g_nDarkSecLength);
}

static SANE_Bool
MustScanner_CalculateMaxMin (SANE_Byte * pBuffer, unsigned short * lpMaxValue,
			     unsigned short * lpMinValue)
{
  unsigned short *wSecData, *wDarkSecData;
  int i, j;

  wSecData = malloc (g_nSecNum * sizeof (unsigned short));
  if (!wSecData)
    return SANE_FALSE;
  memset (wSecData, 0, g_nSecNum * sizeof (unsigned short));

  for (i = 0; i < g_nSecNum; i++)
    {
      for (j = 0; j < g_nSecLength; j++)
	wSecData[i] += pBuffer[g_wStartPosition + i * g_nSecLength + j];
      wSecData[i] >>= g_nPowerNum;
    }

  *lpMaxValue = wSecData[0];
  for (i = 0; i < g_nSecNum; i++)
    {
      if (*lpMaxValue < wSecData[i])
	*lpMaxValue = wSecData[i];
    }
  free (wSecData);

  wDarkSecData = malloc (g_nDarkSecNum * sizeof (unsigned short));
  if (!wDarkSecData)
    return SANE_FALSE;
  memset (wDarkSecData, 0, g_nDarkSecNum * sizeof (unsigned short));

  for (i = 0; i < g_nDarkSecNum; i++)
    {
      for (j = 0; j < g_nDarkSecLength; j++)
	wDarkSecData[i] += pBuffer[g_wStartPosition + i * g_nDarkSecLength + j];
      wDarkSecData[i] /= g_nDarkSecLength;
    }

  *lpMinValue = wDarkSecData[0];
  for (i = 0; i < g_nDarkSecNum; i++)
    {
      if (*lpMinValue > wDarkSecData[i])
	*lpMinValue = wDarkSecData[i];
    }
  free (wDarkSecData);

  return SANE_TRUE;
}


static void *
MustScanner_ReadDataFromScanner (void __sane_unused__ * dummy)
{
  unsigned short wTotalReadImageLines = 0;
  unsigned short wWantedLines = g_Height;
  SANE_Byte * lpReadImage = g_lpReadImageHead;
  SANE_Bool isWaitImageLineDiff = SANE_FALSE;
  unsigned int wMaxScanLines = g_wMaxScanLines;
  unsigned short wReadImageLines = 0;
  unsigned short wScanLinesThisBlock;
  unsigned short wBufferLines = g_wLineDistance * 2 + g_wPixelDistance;

  DBG (DBG_FUNC,
       "MustScanner_ReadDataFromScanner: call in, and in new thread\n");

  while (wTotalReadImageLines < wWantedLines)
    {
      if (!isWaitImageLineDiff)
	{
	  wScanLinesThisBlock =
	    (wWantedLines - wTotalReadImageLines) < g_wScanLinesPerBlock ?
	    (wWantedLines - wTotalReadImageLines) : g_wScanLinesPerBlock;

	  DBG (DBG_FUNC,
	       "MustScanner_ReadDataFromScanner: wWantedLines=%d\n",
	       wWantedLines);
	  DBG (DBG_FUNC,
	       "MustScanner_ReadDataFromScanner: wScanLinesThisBlock=%d\n",
	       wScanLinesThisBlock);

	  if (SANE_STATUS_GOOD !=
	      Asic_ReadImage (&g_chip, lpReadImage, wScanLinesThisBlock))
	    {
	      DBG (DBG_FUNC, "MustScanner_ReadDataFromScanner: Asic_ReadImage" \
			     " return error\n");
	      DBG (DBG_FUNC, "MustScanner_ReadDataFromScanner: thread exit\n");
	      return NULL;
	    }

	  /* has read in memory buffer */
	  wReadImageLines += wScanLinesThisBlock;
	  AddScannedLines (wScanLinesThisBlock);
	  wTotalReadImageLines += wScanLinesThisBlock;
	  lpReadImage += wScanLinesThisBlock * g_BytesPerRow;

	  /* buffer is full */
	  if (wReadImageLines >= wMaxScanLines)
	    {
	      lpReadImage = g_lpReadImageHead;
	      wReadImageLines = 0;
	    }

	  if ((g_dwScannedTotalLines - GetReadyLines ()) >=
	      (wMaxScanLines - (wBufferLines + g_wScanLinesPerBlock)) &&
	      g_dwScannedTotalLines > GetReadyLines ())
	    {
	      isWaitImageLineDiff = SANE_TRUE;
	    }
	}
      else if (g_dwScannedTotalLines <=
	       GetReadyLines () + wBufferLines + g_wScanLinesPerBlock)
	{
	  isWaitImageLineDiff = SANE_FALSE;
	}

      pthread_testcancel ();
    }

  DBG (DBG_FUNC, "MustScanner_ReadDataFromScanner: Read image ok\n");
  DBG (DBG_FUNC, "MustScanner_ReadDataFromScanner: thread exit\n");
  DBG (DBG_FUNC, "MustScanner_ReadDataFromScanner: " \
		 "leave MustScanner_ReadDataFromScanner\n");
  return NULL;
}

static unsigned int
GetScannedLines (void)
{
  unsigned int dwScannedLines;

  pthread_mutex_lock (&g_scannedLinesMutex);
  dwScannedLines = g_dwScannedTotalLines;
  pthread_mutex_unlock (&g_scannedLinesMutex);

  return dwScannedLines;
}

static unsigned int
GetReadyLines (void)
{
  unsigned int dwReadyLines;

  pthread_mutex_lock (&g_readyLinesMutex);
  dwReadyLines = g_wtheReadyLines;
  pthread_mutex_unlock (&g_readyLinesMutex);

  return dwReadyLines;
}

static void
AddScannedLines (unsigned short wAddLines)
{
  pthread_mutex_lock (&g_scannedLinesMutex);
  g_dwScannedTotalLines += wAddLines;
  pthread_mutex_unlock (&g_scannedLinesMutex);
}

static void
AddReadyLines (void)
{
  pthread_mutex_lock (&g_readyLinesMutex);
  g_wtheReadyLines++;
  pthread_mutex_unlock (&g_readyLinesMutex);
}

static void
ModifyLinePoint (SANE_Byte * lpImageData, SANE_Byte * lpImageDataBefore,
		 unsigned int dwBytesPerLine, unsigned int dwLinesCount,
		 unsigned short wPixDistance, unsigned short wModPtCount)
{
  unsigned short i, j;
  unsigned short wLines;
  unsigned int dwWidth = dwBytesPerLine / wPixDistance;
  for (i = wModPtCount; i > 0; i--)
    {
      for (j = 0; j < wPixDistance; j++)
	{
	  unsigned int lineOffset = (dwWidth - i) * wPixDistance + j;
	  unsigned int prevLineOffset = (dwWidth - i - 1) * wPixDistance + j;

	  /* modify the first line */
	  lpImageData[lineOffset] =
	    (lpImageData[prevLineOffset] +
	     lpImageDataBefore[prevLineOffset]) / 2;

	  /* modify other lines */
	  for (wLines = 1; wLines < dwLinesCount; wLines++)
	    {
	      unsigned int dwBytesBefore = (wLines - 1) * dwBytesPerLine;
	      unsigned int dwBytes = wLines * dwBytesPerLine;
	      lpImageData[dwBytes + lineOffset] =
		(lpImageData[dwBytes + prevLineOffset] + 
		 lpImageData[dwBytesBefore + prevLineOffset]) / 2;
	    }
	}
    }
}

static SANE_Byte
QBET4 (SANE_Byte A, SANE_Byte B)
{
  static const SANE_Byte bQBET[16][16] = {
    {0, 0, 0, 0,  1,  1,  2,  2,  4,  4,  5,  5,  8,  8,  9,  9},
    {0, 0, 0, 0,  1,  1,  2,  2,  4,  4,  5,  5,  8,  8,  9,  9},
    {0, 0, 0, 0,  1,  1,  2,  2,  4,  4,  5,  5,  8,  8,  9,  9},
    {0, 0, 0, 0,  1,  1,  2,  2,  4,  4,  5,  5,  8,  8,  9,  9},
    {1, 1, 1, 1,  3,  3,  3,  3,  6,  6,  6,  6, 10, 10, 11, 11},
    {1, 1, 1, 1,  3,  3,  3,  3,  6,  6,  6,  6, 10, 10, 11, 11},
    {2, 2, 2, 2,  3,  3,  3,  3,  7,  7,  7,  7, 10, 10, 11, 11},
    {2, 2, 2, 2,  3,  3,  3,  3,  7,  7,  7,  7, 10, 10, 11, 11},
    {4, 4, 4, 4,  6,  6,  7,  7, 12, 12, 12, 12, 13, 13, 14, 14},
    {4, 4, 4, 4,  6,  6,  7,  7, 12, 12, 12, 12, 13, 13, 14, 14},
    {5, 5, 5, 5,  6,  6,  7,  7, 12, 12, 12, 12, 13, 13, 14, 14},
    {5, 5, 5, 5,  6,  6,  7,  7, 12, 12, 12, 12, 13, 13, 14, 14},
    {8, 8, 8, 8, 10, 10, 10, 10, 13, 13, 13, 13, 15, 15, 15, 15},
    {8, 8, 8, 8, 10, 10, 10, 10, 13, 13, 13, 13, 15, 15, 15, 15},
    {9, 9, 9, 9, 11, 11, 11, 11, 14, 14, 14, 14, 15, 15, 15, 15},
    {9, 9, 9, 9, 11, 11, 11, 11, 14, 14, 14, 14, 15, 15, 15, 15}
  };

  return bQBET[A & 0x0f][B & 0x0f];
}

static SANE_Bool
MustScanner_ScanSuggest (PTARGETIMAGE pTarget, PSUGGESTSETTING pSuggest)
{
  int i;
  unsigned short wMaxWidth, wMaxHeight;

  DBG (DBG_FUNC, "MustScanner_ScanSuggest: call in\n");

  if (!pTarget || !pSuggest)
    {
      DBG (DBG_FUNC, "MustScanner_ScanSuggest: parameters error\n");
      return SANE_FALSE;
    }

  /* Look up optical X and Y resolution. */
  for (i = 0; s_wOpticalDpi[i] != 0; i++)
    {
      if (s_wOpticalDpi[i] <= pTarget->wDpi)
	{
	  pSuggest->wXDpi = s_wOpticalDpi[i];
	  break;
	}
    }
  if (s_wOpticalDpi[i] == 0)
    pSuggest->wXDpi = s_wOpticalDpi[--i];
  pSuggest->wYDpi = pSuggest->wXDpi;

  DBG (DBG_FUNC, "MustScanner_ScanSuggest: pTarget->wDpi = %d\n",
       pTarget->wDpi);
  DBG (DBG_FUNC, "MustScanner_ScanSuggest: pSuggest->w(X|Y)Dpi = %d\n",
       pSuggest->wXDpi);

  /* suggest scan area */
  pSuggest->wX = (unsigned short) (((unsigned int) pTarget->wX *
	    pSuggest->wXDpi) / pTarget->wDpi);
  pSuggest->wY = (unsigned short) (((unsigned int) pTarget->wY *
	    pSuggest->wYDpi) / pTarget->wDpi);
  pSuggest->wWidth = (unsigned short) (((unsigned int) pTarget->wWidth *
	    pSuggest->wXDpi) / pTarget->wDpi);
  pSuggest->wHeight = (unsigned short) (((unsigned int) pTarget->wHeight *
	    pSuggest->wYDpi) / pTarget->wDpi);

  pSuggest->wWidth &= ~1;

  DBG (DBG_FUNC, "MustScanner_ScanSuggest: pTarget->wX = %d\n", pTarget->wX);
  DBG (DBG_FUNC, "MustScanner_ScanSuggest: pTarget->wY = %d\n", pTarget->wY);
  DBG (DBG_FUNC, "MustScanner_ScanSuggest: pTarget->wWidth = %d\n",
       pTarget->wWidth);
  DBG (DBG_FUNC, "MustScanner_ScanSuggest: pTarget->wHeight = %d\n",
       pTarget->wHeight);

  DBG (DBG_FUNC, "MustScanner_ScanSuggest: pSuggest->wX = %d\n", pSuggest->wX);
  DBG (DBG_FUNC, "MustScanner_ScanSuggest: pSuggest->wY = %d\n", pSuggest->wY);
  DBG (DBG_FUNC, "MustScanner_ScanSuggest: pSuggest->wWidth = %d\n",
       pSuggest->wWidth);
  DBG (DBG_FUNC, "MustScanner_ScanSuggest: pSuggest->wHeight = %d\n",
       pSuggest->wHeight);

  if (pTarget->cmColorMode == CM_TEXT)
    {
      pSuggest->wWidth = (pSuggest->wWidth + 7) & ~7;
      if (pSuggest->wWidth < 8)
	pSuggest->wWidth = 8;
    }

  /* check width and height */
  wMaxWidth = (MAX_SCANNING_WIDTH * pSuggest->wXDpi) / 300;
  wMaxHeight = (MAX_SCANNING_HEIGHT * pSuggest->wYDpi) / 300;

  DBG (DBG_FUNC, "MustScanner_ScanSuggest: wMaxWidth = %d\n", wMaxWidth);
  DBG (DBG_FUNC, "MustScanner_ScanSuggest: wMaxHeight = %d\n", wMaxHeight);

  if (CM_TEXT == pTarget->cmColorMode)
    wMaxWidth &= ~7;

  if (pSuggest->wWidth > wMaxWidth)
    pSuggest->wWidth = wMaxWidth;
  if (pSuggest->wHeight > wMaxHeight)
    pSuggest->wHeight = wMaxHeight;

  g_Width = (pSuggest->wWidth + 15) & ~15;	/* real scan width */
  g_Height = pSuggest->wHeight;

  DBG (DBG_FUNC, "MustScanner_ScanSuggest: g_Width=%d\n", g_Width);

  switch (pTarget->cmColorMode)
    {
    case CM_RGB48:
      pSuggest->dwBytesPerRow = (unsigned int) pSuggest->wWidth * 6;
      break;
    case CM_RGB24:
      pSuggest->dwBytesPerRow = (unsigned int) pSuggest->wWidth * 3;
      break;
    case CM_GRAY16:
      pSuggest->dwBytesPerRow = (unsigned int) pSuggest->wWidth * 2;
      break;
    case CM_GRAY8:
      pSuggest->dwBytesPerRow = (unsigned int) pSuggest->wWidth;
      break;
    case CM_TEXT:
      pSuggest->dwBytesPerRow = (unsigned int) pSuggest->wWidth / 8;
      break;
    }
  pSuggest->cmScanMode = pTarget->cmColorMode;

  DBG (DBG_FUNC, "MustScanner_ScanSuggest: pSuggest->dwBytesPerRow = %d\n",
       pSuggest->dwBytesPerRow);
  DBG (DBG_FUNC, "MustScanner_ScanSuggest: leave MustScanner_ScanSuggest\n");
  return SANE_TRUE;
}

static SANE_Bool
MustScanner_StopScan (void)
{
  SANE_Bool result = SANE_TRUE;

  DBG (DBG_FUNC, "MustScanner_StopScan: call in\n");

  if (!g_bOpened || !g_bPrepared)
    {
      DBG (DBG_FUNC, "MustScanner_StopScan: scanner not opened or prepared\n");
      return SANE_FALSE;
    }

  g_isCanceled = SANE_TRUE;

  pthread_cancel (g_threadid_readimage);
  pthread_join (g_threadid_readimage, NULL);

  DBG (DBG_FUNC, "MustScanner_StopScan: thread exit\n");

  if (SANE_STATUS_GOOD != Asic_ScanStop (&g_chip))
    result = SANE_FALSE;
  Asic_Close (&g_chip);

  g_bOpened = SANE_FALSE;

  DBG (DBG_FUNC, "MustScanner_StopScan: leave MustScanner_StopScan\n");
  return result;
}

static SANE_Bool
MustScanner_PrepareScan (void)
{
  DBG (DBG_FUNC, "MustScanner_PrepareScan: call in\n");

  g_isCanceled = SANE_FALSE;

  g_wScanLinesPerBlock = g_dwBufferSize / g_BytesPerRow;
  g_wMaxScanLines = ((g_dwImageBufferSize / g_BytesPerRow) /
		     g_wScanLinesPerBlock) * g_wScanLinesPerBlock;
  g_dwScannedTotalLines = 0;

  switch (g_ScanMode)
    {
    case CM_RGB48:
    case CM_RGB24:
      g_wtheReadyLines = g_wLineDistance * 2 + g_wPixelDistance;
      break;
    case CM_GRAY16:
    case CM_GRAY8:
    case CM_TEXT:
      g_wtheReadyLines = g_wPixelDistance;
      break;
    }
  DBG (DBG_FUNC, "MustScanner_PrepareScan: g_wtheReadyLines=%d\n",
       g_wtheReadyLines);

  DBG (DBG_FUNC, "MustScanner_PrepareScan: g_lpReadImageHead malloc %d bytes\n",
       g_dwImageBufferSize);
  g_lpReadImageHead = malloc (g_dwImageBufferSize);
  if (!g_lpReadImageHead)
    {
      DBG (DBG_FUNC, "MustScanner_PrepareScan: g_lpReadImageHead malloc " \
           "error\n");
      return SANE_FALSE;
    }

  if (SANE_STATUS_GOOD != Asic_ScanStart (&g_chip))
    {
      free(g_lpReadImageHead);
      return SANE_FALSE;
    }

  DBG (DBG_FUNC, "MustScanner_PrepareScan: leave MustScanner_PrepareScan\n");
  return SANE_TRUE;
}
