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

#define DEBUG_DECLARE_ONLY
#include "../include/sane/config.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>		/* HOLD */

#include "../include/sane/sane.h"
#include "../include/sane/sanei_backend.h"

#include "mustek_usb2_high.h"

/* HOLD: these global variables should go to scanner structure */

static SANE_Bool g_bOpened;
static SANE_Bool g_bPrepared;
static SANE_Bool g_isCanceled;
static SANE_Bool g_bFirstReadImage;
SANE_Bool g_isScanning;

static SANE_Byte g_bScanBits;
SANE_Byte * g_lpReadImageHead;

static unsigned short g_X;
static unsigned short g_Y;
static unsigned short g_Width;
static unsigned short g_Height;
static unsigned short g_XDpi;
static unsigned short g_YDpi;
static unsigned short g_SWWidth;
unsigned short g_SWHeight;
static unsigned short g_wPixelDistance;		/* even & odd sensor problem */
static unsigned short g_wLineDistance;
static unsigned short g_wScanLinesPerBlock;
unsigned short g_wLineartThreshold;

static unsigned int g_wtheReadyLines;
static unsigned int g_wMaxScanLines;
static unsigned int g_dwScannedTotalLines;
static unsigned int g_dwImageBufferSize;
static unsigned int g_BytesPerRow;
static unsigned int g_SWBytesPerRow;
static unsigned int g_dwCalibrationSize;
static unsigned int g_dwBufferSize;

static unsigned int g_dwTotalTotalXferLines;

unsigned short * g_pGammaTable;

static pthread_t g_threadid_readimage;

static COLORMODE g_ScanMode;
SCANSOURCE g_ssScanSource;

Asic g_chip;

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


void
MustScanner_Init (void)
{
  DBG (DBG_FUNC, "MustScanner_Init: Call in\n");

  Asic_Initialize (&g_chip);

  g_dwImageBufferSize = 24 * 1024 * 1024;
  g_dwBufferSize = 64 * 1024;
  g_dwCalibrationSize = 64 * 1024;
  g_lpReadImageHead = NULL;

  g_isCanceled = SANE_FALSE;
  g_bOpened = SANE_FALSE;
  g_bPrepared = SANE_FALSE;

  g_isScanning = SANE_FALSE;
  g_pGammaTable = NULL;

  g_ssScanSource = SS_Reflective;

  DBG (DBG_FUNC, "MustScanner_Init: leave MustScanner_Init\n");
}

SANE_Bool
MustScanner_PowerControl (SANE_Bool isLampOn, SANE_Bool isTALampOn)
{
  SANE_Bool hasTA;

  DBG (DBG_FUNC, "MustScanner_PowerControl: Call in\n");

  if (Asic_Open (&g_chip) != SANE_STATUS_GOOD)
    {
      DBG (DBG_FUNC, "MustScanner_PowerControl: Asic_Open return error\n");
      return SANE_FALSE;
    }

  if (Asic_TurnLamp (&g_chip, isLampOn) != SANE_STATUS_GOOD)
    {
      DBG (DBG_FUNC,
	   "MustScanner_PowerControl: Asic_TurnLamp return error\n");
      goto error;
    }

  if (Asic_IsTAConnected (&g_chip, &hasTA) != SANE_STATUS_GOOD)
    {
      DBG (DBG_FUNC,
	   "MustScanner_PowerControl: Asic_IsTAConnected return error\n");
      goto error;
    }

  if (hasTA && (Asic_TurnTA (&g_chip, isTALampOn) != SANE_STATUS_GOOD))
    {
      DBG (DBG_FUNC, "MustScanner_PowerControl: Asic_TurnTA return error\n");
      goto error;
    }

  Asic_Close (&g_chip);

  DBG (DBG_FUNC,
       "MustScanner_PowerControl: leave MustScanner_PowerControl\n");
  return SANE_TRUE;

error:
  Asic_Close (&g_chip);
  return SANE_FALSE;
}

SANE_Bool
MustScanner_BackHome (void)
{
  DBG (DBG_FUNC, "MustScanner_BackHome: call in\n");

  if (Asic_Open (&g_chip) != SANE_STATUS_GOOD)
    {
      DBG (DBG_FUNC, "MustScanner_BackHome: Asic_Open return error\n");
      return SANE_FALSE;
    }

  if (Asic_CarriageHome (&g_chip) != SANE_STATUS_GOOD)
    {
      DBG (DBG_FUNC,
	   "MustScanner_BackHome: Asic_CarriageHome return error\n");
      Asic_Close (&g_chip);
      return SANE_FALSE;
    }

  Asic_Close (&g_chip);

  DBG (DBG_FUNC, "MustScanner_BackHome: leave MustScanner_BackHome\n");
  return SANE_TRUE;
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

  if (g_ssScanSource == SS_Reflective)
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

  if (g_ssScanSource == SS_Reflective)
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

  if (g_ssScanSource == SS_Reflective)
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

  if (g_ssScanSource == SS_Reflective)
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

  if (g_ssScanSource == SS_Reflective)
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

	  if (Asic_ReadImage (&g_chip, lpReadImage, wScanLinesThisBlock) !=
	      SANE_STATUS_GOOD)
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

SANE_Bool
MustScanner_GetRows (SANE_Byte * lpBlock, unsigned short * Rows,
		     SANE_Bool isOrderInvert)
{
  unsigned int dwLineIncrement = g_SWBytesPerRow;
  SANE_Bool fixEvenOdd = SANE_FALSE;
  void (*pFunc)(SANE_Byte *, SANE_Bool) = NULL;

  DBG (DBG_FUNC, "MustScanner_GetRows: call in\n");

  if (!g_bOpened || !g_bPrepared)
    {
      DBG (DBG_FUNC, "MustScanner_GetRows: invalid state\n");
      return SANE_FALSE;
    }

  switch (g_ScanMode)
    {
    case CM_RGB48:
      if (g_XDpi == SENSOR_DPI)
        pFunc = GetRgb48BitLineFull;
      else
        pFunc = GetRgb48BitLineHalf;
      break;

    case CM_RGB24:
      if (g_XDpi == SENSOR_DPI)
        pFunc = GetRgb24BitLineFull;
      else
        pFunc = GetRgb24BitLineHalf;
      break;

    case CM_GRAY16:
      if (g_XDpi == SENSOR_DPI)
        {
          fixEvenOdd = SANE_TRUE;
          pFunc = GetMono16BitLineFull;
        }
      else
        {
	  pFunc = GetMono16BitLineHalf;
	}
      break;

    case CM_GRAY8:
      if (g_XDpi == SENSOR_DPI)
        {
          fixEvenOdd = SANE_TRUE;
          pFunc = GetMono8BitLineFull;
        }
      else
        {
          pFunc = GetMono8BitLineHalf;
        }
      break;

    case CM_TEXT:
      memset (lpBlock, 0, *Rows * g_SWWidth / 8);
      dwLineIncrement /= 8;
      if (g_XDpi == SENSOR_DPI)
	pFunc = GetMono1BitLineFull;
      else
	pFunc = GetMono1BitLineHalf;
      break;
    }

  DBG (DBG_FUNC, "MustScanner_GetRows: leave MustScanner_GetRows\n");
  return MustScanner_GetLine (lpBlock, Rows, dwLineIncrement, pFunc,
			      isOrderInvert, fixEvenOdd);
}

SANE_Bool
MustScanner_ScanSuggest (PTARGETIMAGE pTarget)
{
  unsigned short wMaxWidth, wMaxHeight;

  DBG (DBG_FUNC, "MustScanner_ScanSuggest: call in\n");

  if (!pTarget)
    {
      DBG (DBG_FUNC, "MustScanner_ScanSuggest: parameters error\n");
      return SANE_FALSE;
    }

  /* check width and height */
  wMaxWidth = (MAX_SCANNING_WIDTH * pTarget->wDpi) / 300;
  wMaxHeight = (MAX_SCANNING_HEIGHT * pTarget->wDpi) / 300;

  DBG (DBG_FUNC, "MustScanner_ScanSuggest: wMaxWidth = %d\n", wMaxWidth);
  DBG (DBG_FUNC, "MustScanner_ScanSuggest: wMaxHeight = %d\n", wMaxHeight);

  pTarget->wWidth = _MIN (pTarget->wWidth, wMaxWidth);
  pTarget->wHeight = _MIN (pTarget->wHeight, wMaxHeight);

  g_Width = (pTarget->wWidth + 15) & ~15;	/* real scan width */
  g_Height = pTarget->wHeight;
  DBG (DBG_FUNC, "MustScanner_ScanSuggest: g_Width=%d\n", g_Width);

  switch (pTarget->cmColorMode)
    {
    case CM_RGB48:
      pTarget->dwBytesPerRow = (unsigned int) pTarget->wWidth * 6;
      break;
    case CM_RGB24:
      pTarget->dwBytesPerRow = (unsigned int) pTarget->wWidth * 3;
      break;
    case CM_GRAY16:
      pTarget->dwBytesPerRow = (unsigned int) pTarget->wWidth * 2;
      break;
    case CM_GRAY8:
      pTarget->dwBytesPerRow = (unsigned int) pTarget->wWidth;
      break;
    case CM_TEXT:
      pTarget->dwBytesPerRow = (unsigned int) pTarget->wWidth / 8;
      break;
    }
  DBG (DBG_FUNC, "MustScanner_ScanSuggest: pTarget->dwBytesPerRow = %d\n",
       pTarget->dwBytesPerRow);

  DBG (DBG_FUNC, "MustScanner_ScanSuggest: leave MustScanner_ScanSuggest\n");
  return SANE_TRUE;
}

SANE_Bool
MustScanner_StopScan (void)
{
  SANE_Bool result = SANE_TRUE;

  DBG (DBG_FUNC, "MustScanner_StopScan: call in\n");

  if (!g_bOpened || !g_bPrepared)
    {
      DBG (DBG_FUNC, "MustScanner_StopScan: invalid state\n");
      return SANE_FALSE;
    }

  g_isCanceled = SANE_TRUE;

  pthread_cancel (g_threadid_readimage);
  pthread_join (g_threadid_readimage, NULL);

  DBG (DBG_FUNC, "MustScanner_StopScan: thread exit\n");

  if (Asic_ScanStop (&g_chip) != SANE_STATUS_GOOD)
    result = SANE_FALSE;
  Asic_Close (&g_chip);

  g_bOpened = SANE_FALSE;

  DBG (DBG_FUNC, "MustScanner_StopScan: leave MustScanner_StopScan\n");
  return result;
}

SANE_Bool
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

  if (Asic_ScanStart (&g_chip) != SANE_STATUS_GOOD)
    {
      free(g_lpReadImageHead);
      return SANE_FALSE;
    }

  DBG (DBG_FUNC, "MustScanner_PrepareScan: leave MustScanner_PrepareScan\n");
  return SANE_TRUE;
}

SANE_Bool
MustScanner_Reset (SCANSOURCE ssScanSource)
{
  DBG (DBG_FUNC, "MustScanner_Reset: call in\n");

  if (g_bOpened)
    {
      DBG (DBG_FUNC, "MustScanner_Reset: scanner already open\n");
      return SANE_FALSE;
    }

  if (ssScanSource == SS_Reflective)
    {
      if (!MustScanner_PowerControl (SANE_TRUE, SANE_FALSE))
        return SANE_FALSE;
    }
  else
    {
      if (!MustScanner_PowerControl (SANE_FALSE, SANE_TRUE))
        return SANE_FALSE;
    }

  g_dwTotalTotalXferLines = 0;
  g_bFirstReadImage = SANE_TRUE;
  g_bPrepared = SANE_TRUE;

  DBG (DBG_FUNC, "MustScannert_Reset: leave\n");
  return SANE_TRUE;
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

/* TODO: error handling for Asic functions */

static SANE_Bool
MustScanner_AdjustAD (void)
{
  SANE_Byte * lpCalData;
  unsigned short wCalWidth;
  unsigned short wMaxValue[3], wMinValue[3];
  unsigned short wAdjustADResolution;
  int i, j;
  double range;
#if DEBUG_SAVE_IMAGE
  FILE * stream;
  char buf[16];
#endif

  DBG (DBG_FUNC, "MustScanner_AdjustAD: call in\n");

  if (!g_bOpened || !g_bPrepared)
    {
      DBG (DBG_FUNC, "MustScanner_AdjustAD: invalid state\n");
      return SANE_FALSE;
    }

  for (i = 0; i < 3; i++)
    {
      g_chip.AD.Direction[i] = DIR_POSITIVE;
      g_chip.AD.Gain[i] = 0;
    }
  if (g_ssScanSource == SS_Reflective)
    {
      g_chip.AD.Offset[0] = 152;	/* red */
      g_chip.AD.Offset[1] = 56;		/* green */
      g_chip.AD.Offset[2] = 8;		/* blue */
    }
  else
    {
      g_chip.AD.Offset[0] = 159;
      g_chip.AD.Offset[1] = 50;
      g_chip.AD.Offset[2] = 45;
    }

  if (g_XDpi <= (SENSOR_DPI / 2))
    wAdjustADResolution = SENSOR_DPI / 2;
  else
    wAdjustADResolution = SENSOR_DPI;

  wCalWidth = 10240;
  lpCalData = malloc (wCalWidth * 3);
  if (!lpCalData)
    {
      DBG (DBG_FUNC, "MustScanner_AdjustAD: lpCalData malloc error\n");
      return SANE_FALSE;
    }

  Asic_SetWindow (&g_chip, g_ssScanSource, SCAN_TYPE_CALIBRATE_DARK, 24,
		  wAdjustADResolution, wAdjustADResolution, 0, 0, wCalWidth, 1);
  MustScanner_PrepareCalculateMaxMin (wAdjustADResolution);

  for (i = 0; i < 10; i++)
    {
      DBG (DBG_FUNC, "MustScanner_AdjustAD: first offset adjustment loop\n");
      SetAFEGainOffset (&g_chip);
      Asic_ScanStart (&g_chip);
      Asic_ReadCalibrationData (&g_chip, lpCalData, wCalWidth * 3, 24);
      Asic_ScanStop (&g_chip);

#ifdef DEBUG_SAVE_IMAGE
      if (i == 0)
	{
	  stream = fopen ("AD.ppm", "wb");
	  snprintf (buf, sizeof (buf), "P6\n%u 1\n255\n", wCalWidth);
	  fwrite (buf, 1, strlen (buf), stream);
	  fwrite (lpCalData, 1, wCalWidth * 3, stream);
	  fclose (stream);
	}
#endif

      for (j = 0; j < 3; j++)
        {
          if (!MustScanner_CalculateMaxMin (&lpCalData[wCalWidth * j],
					    &wMaxValue[j], &wMinValue[j]))
	    return SANE_FALSE;

	  if (g_chip.AD.Direction[j] == DIR_POSITIVE)
	    {
	      if (wMinValue[j] > 15)
		{
		  if (g_chip.AD.Offset[j] < 8)
		    g_chip.AD.Direction[j] = DIR_NEGATIVE;
		  else
		    g_chip.AD.Offset[j] -= 8;
		}
	      else if (wMinValue[j] < 5)
		g_chip.AD.Offset[j] += 8;
	    }
	  else
	    {
	      if (wMinValue[j] > 15)
		g_chip.AD.Offset[j] += 8;
	      else if (wMinValue[j] < 5)
		g_chip.AD.Offset[j] -= 8;
	    }
        }

      if (!(wMinValue[0] > 15 || wMinValue[0] < 5 ||
	    wMinValue[1] > 15 || wMinValue[1] < 5 ||
	    wMinValue[2] > 15 || wMinValue[2] < 5))
	break;
    }

  DBG (DBG_FUNC, "MustScanner_AdjustAD: OffsetR=%d,OffsetG=%d,OffsetB=%d\n",
       g_chip.AD.Offset[0], g_chip.AD.Offset[1], g_chip.AD.Offset[2]);

  for (i = 0; i < 3; i++)
    {
      range = 1.0 - (double) (wMaxValue[i] - wMinValue[i]) / MAX_LEVEL_RANGE;
      if (range > 0)
        {
	  g_chip.AD.Gain[i] = (SANE_Byte) range * 63 * 6 / 5;
	  g_chip.AD.Gain[i] = _MIN (g_chip.AD.Gain[i], 63);
	}
      else
      	g_chip.AD.Gain[i] = 0;
    }

  DBG (DBG_FUNC, "MustScanner_AdjustAD: GainR=%d,GainG=%d,GainB=%d\n",
       g_chip.AD.Gain[0], g_chip.AD.Gain[1], g_chip.AD.Gain[2]);

  for (i = 0; i < 10; i++)
    {
      SetAFEGainOffset (&g_chip);
      Asic_ScanStart (&g_chip);
      Asic_ReadCalibrationData (&g_chip, lpCalData, wCalWidth * 3, 24);
      Asic_ScanStop (&g_chip);

      for (j = 0; j < 3; j++)
	{
	  if (!MustScanner_CalculateMaxMin (&lpCalData[wCalWidth * j],
					    &wMaxValue[j], &wMinValue[j]))
	    return SANE_FALSE;

	  DBG (DBG_FUNC, "MustScanner_AdjustAD: %d: Max=%d, Min=%d\n",
	       j, wMaxValue[j], wMinValue[j]);

	  if ((wMaxValue[j] - wMinValue[j]) > MAX_LEVEL_RANGE)
	    {
	      if (g_chip.AD.Gain[j] != 0)
		g_chip.AD.Gain[j]--;
	    }
	  else if ((wMaxValue[j] - wMinValue[j]) < MIN_LEVEL_RANGE)
	    {
	      if (wMaxValue[j] > WHITE_MAX_LEVEL)
		{
		  if (g_chip.AD.Gain[j] != 0)
		    g_chip.AD.Gain[j]--;
		}
	      else
		{
		  if (g_chip.AD.Gain[j] < 63)
		    g_chip.AD.Gain[j]++;
		}
	    }
	  else
	    {
	      if (wMaxValue[j] > WHITE_MAX_LEVEL)
		{
		  if (g_chip.AD.Gain[j] != 0)
		    g_chip.AD.Gain[j]--;
		}
	      else if (wMaxValue[j] < WHITE_MIN_LEVEL)
		{
		  if (g_chip.AD.Gain[j] < 63)
		    g_chip.AD.Gain[j]++;
		}
	    }

	  DBG (DBG_FUNC, "MustScanner_AdjustAD: %d: Gain=%d,Offset=%d,Dir=%d\n",
	       j, g_chip.AD.Gain[j], g_chip.AD.Offset[j],
	       g_chip.AD.Direction[j]);
	}

      if (!((wMaxValue[0] - wMinValue[0]) > MAX_LEVEL_RANGE ||
	    (wMaxValue[0] - wMinValue[0]) < MIN_LEVEL_RANGE ||
	    (wMaxValue[1] - wMinValue[1]) > MAX_LEVEL_RANGE ||
	    (wMaxValue[1] - wMinValue[1]) < MIN_LEVEL_RANGE ||
	    (wMaxValue[2] - wMinValue[2]) > MAX_LEVEL_RANGE ||
	    (wMaxValue[2] - wMinValue[2]) < MIN_LEVEL_RANGE))
	break;
    }

  for (i = 0; i < 8; i++)
    {
      DBG (DBG_FUNC, "MustScanner_AdjustAD: second offset adjustment loop\n");
      SetAFEGainOffset (&g_chip);
      Asic_ScanStart (&g_chip);
      Asic_ReadCalibrationData (&g_chip, lpCalData, wCalWidth * 3, 24);
      Asic_ScanStop (&g_chip);

      for (j = 0; j < 3; j++)
        {
	  if (!MustScanner_CalculateMaxMin (&lpCalData[wCalWidth * j],
					    &wMaxValue[j], &wMinValue[j]))
	    return SANE_FALSE;

	  DBG (DBG_FUNC, "MustScanner_AdjustAD: %d: Max=%d, Min=%d\n",
	       j, wMaxValue[j], wMinValue[j]);

	  if (g_chip.AD.Direction[j] == DIR_POSITIVE)
	    {
	      if (wMinValue[j] > 20)
		{
		  if (g_chip.AD.Offset[j] < 8)
		    g_chip.AD.Direction[j] = DIR_NEGATIVE;
		  else
		    g_chip.AD.Offset[j] -= 8;
		}
	      else if (wMinValue[j] < 10)
	        {
		  g_chip.AD.Offset[j] += 8;
		}
	    }
	  else
	    {
	      if (wMinValue[j] > 20)
		g_chip.AD.Offset[j] += 8;
	      else if (wMinValue[j] < 10)
		g_chip.AD.Offset[j] -= 8;
	    }

	  DBG (DBG_FUNC, "MustScanner_AdjustAD: %d: Gain=%d,Offset=%d,Dir=%d\n",
	       j, g_chip.AD.Gain[j], g_chip.AD.Offset[j],
	       g_chip.AD.Direction[j]);
        }

      if (!(wMinValue[0] > 20 || wMinValue[0] < 10 ||
	    wMinValue[1] > 20 || wMinValue[1] < 10 ||
	    wMinValue[2] > 20 || wMinValue[2] < 10))
	break;
    }

  free (lpCalData);

  DBG (DBG_FUNC, "MustScanner_AdjustAD: leave\n");
  return SANE_TRUE;
}

static SANE_Bool
MustScanner_FindTopLeft (unsigned short * lpwStartX, unsigned short * lpwStartY)
{
  unsigned short wCalWidth, wCalHeight;
  unsigned int dwTotalSize;
  int nScanBlock;
  SANE_Byte * lpCalData;
  unsigned short wLeftSide, wTopSide;
  int i, j;
#ifdef DEBUG_SAVE_IMAGE
  FILE * stream;
  char buf[20];
#endif

  DBG (DBG_FUNC, "MustScanner_FindTopLeft: call in\n");

  if (!g_bOpened || !g_bPrepared)
    {
      DBG (DBG_FUNC, "MustScanner_FindTopLeft: invalid state\n");
      return SANE_FALSE;
    }

  if (g_ssScanSource == SS_Reflective)
    {
      wCalWidth = FIND_LEFT_TOP_WIDTH_IN_DIP;
      wCalHeight = FIND_LEFT_TOP_HEIGHT_IN_DIP;
    }
  else
    {
      wCalWidth = TA_FIND_LEFT_TOP_WIDTH_IN_DIP;
      wCalHeight = TA_FIND_LEFT_TOP_HEIGHT_IN_DIP;
    }

  dwTotalSize = wCalWidth * wCalHeight;
  lpCalData = malloc (dwTotalSize);
  if (!lpCalData)
    {
      DBG (DBG_FUNC, "MustScanner_FindTopLeft: lpCalData malloc error\n");
      return SANE_FALSE;
    }
  nScanBlock = dwTotalSize / g_dwCalibrationSize;

  Asic_SetWindow (&g_chip, g_ssScanSource, SCAN_TYPE_CALIBRATE_LIGHT, 8,
		  FIND_LEFT_TOP_CALIBRATE_RESOLUTION,
		  FIND_LEFT_TOP_CALIBRATE_RESOLUTION, 0, 0,
		  wCalWidth, wCalHeight);
  SetAFEGainOffset (&g_chip);
  if (Asic_ScanStart (&g_chip) != SANE_STATUS_GOOD)
    {
      DBG (DBG_FUNC, "MustScanner_FindTopLeft: Asic_ScanStart return error\n");
      free (lpCalData);
      return SANE_FALSE;
    }

  for (i = 0; i < nScanBlock; i++)
    {
      if (Asic_ReadCalibrationData (&g_chip,
				    &lpCalData[i * g_dwCalibrationSize],
				    g_dwCalibrationSize, 8) != SANE_STATUS_GOOD)
	{
	  DBG (DBG_FUNC, "MustScanner_FindTopLeft: Asic_ReadCalibrationData " \
			 "return error\n");
	  free (lpCalData);
	  return SANE_FALSE;
	}
    }

  if (Asic_ReadCalibrationData (&g_chip,
				&lpCalData[nScanBlock * g_dwCalibrationSize],
				dwTotalSize - g_dwCalibrationSize * nScanBlock,
				8) != SANE_STATUS_GOOD)
    {
      DBG (DBG_FUNC, "MustScanner_FindTopLeft: Asic_ReadCalibrationData " \
		     "return error\n");
      free (lpCalData);
      return SANE_FALSE;
    }

  Asic_ScanStop (&g_chip);

#ifdef DEBUG_SAVE_IMAGE
  stream = fopen ("bounds.pgm", "wb");
  snprintf (buf, sizeof (buf), "P5\n%d %d\n255\n", wCalWidth, wCalHeight);
  fwrite (buf, 1, strlen (buf), stream);
  fwrite (lpCalData, 1, dwTotalSize, stream);
  fclose (stream);
#endif

  /* find left side */
  for (i = wCalWidth - 1; i > 0; i--)
    {
      wLeftSide = lpCalData[i];
      wLeftSide += lpCalData[wCalWidth * 2 + i];
      wLeftSide += lpCalData[wCalWidth * 4 + i];
      wLeftSide += lpCalData[wCalWidth * 6 + i];
      wLeftSide += lpCalData[wCalWidth * 8 + i];
      wLeftSide /= 5;

      if (wLeftSide < 60)
	{
	  if (i < (wCalWidth - 1))
	    *lpwStartX = i;
	  break;
	}
    }

  if (g_ssScanSource == SS_Reflective)
    {
      /* find top side, i = left side */
      for (j = 0; j < wCalHeight; j++)
        {
          wTopSide = lpCalData[wCalWidth * j + i - 2];
          wTopSide += lpCalData[wCalWidth * j + i - 4];
          wTopSide += lpCalData[wCalWidth * j + i - 6];
          wTopSide += lpCalData[wCalWidth * j + i - 8];
          wTopSide += lpCalData[wCalWidth * j + i - 10];
          wTopSide /= 5;

          if (wTopSide > 60)
	    {
	      if (j > 0)
	        *lpwStartY = j;
	      break;
	    }
        }

      if ((*lpwStartX < 100) || (*lpwStartX > 250))
        *lpwStartX = 187;
      if ((*lpwStartY < 10) || (*lpwStartY > 100))
        *lpwStartY = 43;

      Asic_MotorMove (&g_chip, SANE_FALSE,
		      (wCalHeight - *lpwStartY +
		       BEFORE_SCANNING_MOTOR_FORWARD_PIXEL) * SENSOR_DPI /
		      FIND_LEFT_TOP_CALIBRATE_RESOLUTION);
    }
  else
    {
      /* find top side, i = left side */
      for (j = 0; j < wCalHeight; j++)
        {
          wTopSide = lpCalData[wCalWidth * j + i + 2];
          wTopSide += lpCalData[wCalWidth * j + i + 4];
          wTopSide += lpCalData[wCalWidth * j + i + 6];
          wTopSide += lpCalData[wCalWidth * j + i + 8];
          wTopSide += lpCalData[wCalWidth * j + i + 10];
          wTopSide /= 5;

          if (wTopSide < 60)
	    {
	      if (j > 0)
	        *lpwStartY = j;
	      break;
	    }
        }

      if ((*lpwStartX < 2200) || (*lpwStartX > 2300))
        *lpwStartX = 2260;
      if ((*lpwStartY < 100) || (*lpwStartY > 200))
        *lpwStartY = 124;

      Asic_MotorMove (&g_chip, SANE_FALSE,
		      (wCalHeight - *lpwStartY) * SENSOR_DPI /
		      FIND_LEFT_TOP_CALIBRATE_RESOLUTION + 300);
    }

  free (lpCalData);

  DBG (DBG_FUNC, "MustScanner_FindTopLeft: *lpwStartY = %d, *lpwStartX = %d\n",
       *lpwStartY, *lpwStartX);
  DBG (DBG_FUNC, "MustScanner_FindTopLeft: leave\n");
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

static SANE_Bool
MustScanner_LineCalibration16Bits (void)
{
  SANE_Byte * lpWhiteData;
  SANE_Byte * lpDarkData;
  unsigned int dwTotalSize;
  unsigned short wCalWidth, wCalHeight;
  unsigned short * lpWhiteShading;
  unsigned short * lpDarkShading;
  double wRWhiteLevel;
  double wGWhiteLevel;
  double wBWhiteLevel;
  unsigned int dwRDarkLevel = 0;
  unsigned int dwGDarkLevel = 0;
  unsigned int dwBDarkLevel = 0;
  unsigned int dwREvenDarkLevel = 0;
  unsigned int dwGEvenDarkLevel = 0;
  unsigned int dwBEvenDarkLevel = 0;
  unsigned short * lpRWhiteSort;
  unsigned short * lpGWhiteSort;
  unsigned short * lpBWhiteSort;
  unsigned short * lpRDarkSort;
  unsigned short * lpGDarkSort;
  unsigned short * lpBDarkSort;
  int i, j;
#ifdef DEBUG_SAVE_IMAGE
  FILE * stream;
  char buf[22];
#endif

  DBG (DBG_FUNC, "MustScanner_LineCalibration16Bits: call in\n");

  if (!g_bOpened || !g_bPrepared)
    {
      DBG (DBG_FUNC, "MustScanner_LineCalibration16Bits: invalid state\n");
      return SANE_FALSE;
    }

  wCalWidth = g_Width;
  wCalHeight = LINE_CALIBRATION__16BITS_HEIGHT;
  dwTotalSize = wCalWidth * wCalHeight * 3 * 2;
  DBG (DBG_FUNC, "MustScanner_LineCalibration16Bits: wCalWidth = %d, " \
		 "wCalHeight = %d\n", wCalWidth, wCalHeight);

  lpWhiteData = malloc (dwTotalSize);
  lpDarkData = malloc (dwTotalSize);
  if (!lpWhiteData || !lpDarkData)
    {
      DBG (DBG_FUNC, "MustScanner_LineCalibration16Bits: malloc error\n");
      return SANE_FALSE;
    }

  /* read white level data */
  SetAFEGainOffset (&g_chip);
  if (Asic_SetWindow (&g_chip, g_ssScanSource, SCAN_TYPE_CALIBRATE_LIGHT, 48,
		      g_XDpi, 600, g_X, 0, wCalWidth, wCalHeight) !=
      SANE_STATUS_GOOD)
    goto error;

  if (Asic_ScanStart (&g_chip) != SANE_STATUS_GOOD)
    goto error;

  if (Asic_ReadCalibrationData (&g_chip, lpWhiteData, dwTotalSize, 8) != SANE_STATUS_GOOD)
    goto error;

  Asic_ScanStop (&g_chip);

  /* read dark level data */
  SetAFEGainOffset (&g_chip);
  if (Asic_SetWindow (&g_chip, g_ssScanSource, SCAN_TYPE_CALIBRATE_DARK, 48,
		      g_XDpi, 600, g_X, 0, wCalWidth, wCalHeight) !=
      SANE_STATUS_GOOD)
    goto error;

  if (g_ssScanSource == SS_Reflective)
    {
      if (Asic_TurnLamp (&g_chip, SANE_FALSE) != SANE_STATUS_GOOD)
	goto error;
    }
  else
    {
      if (Asic_TurnTA (&g_chip, SANE_FALSE) != SANE_STATUS_GOOD)
	goto error;
    }

  usleep (500000);

  if (Asic_ScanStart (&g_chip) != SANE_STATUS_GOOD)
    goto error;

  if (Asic_ReadCalibrationData (&g_chip, lpDarkData, dwTotalSize, 8) !=
      SANE_STATUS_GOOD)
    goto error;

  Asic_ScanStop (&g_chip);

  if (g_ssScanSource == SS_Reflective)
    {
      if (Asic_TurnLamp (&g_chip, SANE_TRUE) != SANE_STATUS_GOOD)
	goto error;
    }
  else
    {
      if (Asic_TurnTA (&g_chip, SANE_TRUE) != SANE_STATUS_GOOD)
	goto error;
    }

#ifdef DEBUG_SAVE_IMAGE
  stream = fopen ("whiteshading.ppm", "wb");
  snprintf (buf, sizeof (buf), "P6\n%d %d\n65535\n", wCalWidth, wCalHeight);
  fwrite (buf, 1, strlen (buf), stream);
  fwrite (lpWhiteData, 1, dwTotalSize, stream);
  fclose (stream);

  stream = fopen ("darkshading.ppm", "wb");
  snprintf (buf, sizeof (buf), "P6\n%d %d\n65535\n", wCalWidth, wCalHeight);
  fwrite (buf, 1, strlen (buf), stream);
  fwrite (lpDarkData, 1, dwTotalSize, stream);
  fclose (stream);
#endif

  sleep (1);

  lpWhiteShading = malloc (sizeof (unsigned short) * wCalWidth * 3);
  lpDarkShading = malloc (sizeof (unsigned short) * wCalWidth * 3);

  lpRWhiteSort = malloc (sizeof (unsigned short) * wCalHeight);
  lpGWhiteSort = malloc (sizeof (unsigned short) * wCalHeight);
  lpBWhiteSort = malloc (sizeof (unsigned short) * wCalHeight);
  lpRDarkSort = malloc (sizeof (unsigned short) * wCalHeight);
  lpGDarkSort = malloc (sizeof (unsigned short) * wCalHeight);
  lpBDarkSort = malloc (sizeof (unsigned short) * wCalHeight);

  if (!lpWhiteShading || !lpDarkShading ||
      !lpRWhiteSort || !lpGWhiteSort || !lpBWhiteSort ||
      !lpRDarkSort || !lpGDarkSort || !lpBDarkSort)
    {
      DBG (DBG_FUNC, "MustScanner_LineCalibration16Bits: malloc error\n");
      goto error;
    }

  /* compute dark level */
  for (i = 0; i < wCalWidth; i++)
    {
      for (j = 0; j < wCalHeight; j++)
	{
	  lpRDarkSort[j] = lpDarkData[j * wCalWidth * 6 + i * 6];
	  lpRDarkSort[j] += lpDarkData[j * wCalWidth * 6 + i * 6 + 1] << 8;

	  lpGDarkSort[j] = lpDarkData[j * wCalWidth * 6 + i * 6 + 2];
	  lpGDarkSort[j] += lpDarkData[j * wCalWidth * 6 + i * 6 + 3] << 8;

	  lpBDarkSort[j] = lpDarkData[j * wCalWidth * 6 + i * 6 + 4];
	  lpBDarkSort[j] += lpDarkData[j * wCalWidth * 6 + i * 6 + 5] << 8;
	}

      /* sum of dark level for all pixels */
      if ((g_XDpi == SENSOR_DPI) && ((i % 2) == 0))
	{
    	  /* compute dark shading table with mean */
	  dwREvenDarkLevel += MustScanner_FiltLower (lpRDarkSort, wCalHeight,
	  					     20, 30);
	  dwGEvenDarkLevel += MustScanner_FiltLower (lpGDarkSort, wCalHeight,
	  					     20, 30);
	  dwBEvenDarkLevel += MustScanner_FiltLower (lpBDarkSort, wCalHeight,
	  					     20, 30);
	}
      else
        {
          dwRDarkLevel += MustScanner_FiltLower (lpRDarkSort, wCalHeight,
						 20, 30);
	  dwGDarkLevel += MustScanner_FiltLower (lpGDarkSort, wCalHeight,
						 20, 30);
	  dwBDarkLevel += MustScanner_FiltLower (lpBDarkSort, wCalHeight,
						 20, 30);
	}
    }

  if (g_XDpi == SENSOR_DPI)
    {
      dwRDarkLevel /= wCalWidth / 2;
      dwGDarkLevel /= wCalWidth / 2;
      dwBDarkLevel /= wCalWidth / 2;
      dwREvenDarkLevel /= wCalWidth / 2;
      dwGEvenDarkLevel /= wCalWidth / 2;
      dwBEvenDarkLevel /= wCalWidth / 2;
    }
  else
    {
      dwRDarkLevel /= wCalWidth;
      dwGDarkLevel /= wCalWidth;
      dwBDarkLevel /= wCalWidth;
    }
  if (g_ssScanSource != SS_Reflective)
    {
      dwRDarkLevel -= 512;
      dwGDarkLevel -= 512;
      dwBDarkLevel -= 512;
      dwREvenDarkLevel -= 512;
      dwGEvenDarkLevel -= 512;
      dwBEvenDarkLevel -= 512;
    }

  /* compute white level and create shading tables */
  for (i = 0; i < wCalWidth; i++)
    {
      for (j = 0; j < wCalHeight; j++)
	{
	  lpRWhiteSort[j] = lpWhiteData[j * wCalWidth * 2 * 3 + i * 6];
	  lpRWhiteSort[j] += lpWhiteData[j * wCalWidth * 2 * 3 + i * 6 + 1] <<8;

	  lpGWhiteSort[j] = lpWhiteData[j * wCalWidth * 2 * 3 + i * 6 + 2];
	  lpGWhiteSort[j] += lpWhiteData[j * wCalWidth * 2 * 3 + i * 6 + 3] <<8;

	  lpBWhiteSort[j] = lpWhiteData[j * wCalWidth * 2 * 3 + i * 6 + 4];
	  lpBWhiteSort[j] += lpWhiteData[j * wCalWidth * 2 * 3 + i * 6 + 5] <<8;
	}

      if ((g_XDpi == SENSOR_DPI) && ((i % 2) == 0))
	{
	  lpDarkShading[i * 3] = dwREvenDarkLevel;
	  lpDarkShading[i * 3 + 1] = dwGEvenDarkLevel;
	  if (g_ssScanSource == SS_Positive)
	    lpDarkShading[i * 3 + 1] *= 0.78;
	  lpDarkShading[i * 3 + 2] = dwBEvenDarkLevel;
	}
      else
	{
	  lpDarkShading[i * 3] = dwRDarkLevel;
	  lpDarkShading[i * 3 + 1] = dwGDarkLevel;
	  if (g_ssScanSource == SS_Positive)
	    lpDarkShading[i * 3 + 1] *= 0.78;
	  lpDarkShading[i * 3 + 2] = dwBDarkLevel;
	}

      wRWhiteLevel = MustScanner_FiltLower (lpRWhiteSort, wCalHeight, 20, 30) -
		     lpDarkShading[i * 3];
      wGWhiteLevel = MustScanner_FiltLower (lpGWhiteSort, wCalHeight, 20, 30) -
		     lpDarkShading[i * 3 + 1];
      wBWhiteLevel = MustScanner_FiltLower (lpBWhiteSort, wCalHeight, 20, 30) -
		     lpDarkShading[i * 3 + 2];

      switch (g_ssScanSource)
        {
        case SS_Reflective:
          if (wRWhiteLevel > 0)
	    lpWhiteShading[i * 3] = (unsigned short)
				    (65535.0 / wRWhiteLevel * 0x2000);
          else
	    lpWhiteShading[i * 3] = 0x2000;

          if (wGWhiteLevel > 0)
	    lpWhiteShading[i * 3 + 1] = (unsigned short)
				        (65535.0 / wGWhiteLevel * 0x2000);
          else
	    lpWhiteShading[i * 3 + 1] = 0x2000;

          if (wBWhiteLevel > 0)
	    lpWhiteShading[i * 3 + 2] = (unsigned short)
				        (65535.0 / wBWhiteLevel * 0x2000);
          else
	    lpWhiteShading[i * 3 + 2] = 0x2000;
	  break;

	case SS_Negative:
	  if (wRWhiteLevel > 0)
	    lpWhiteShading[i * 3] = (unsigned short)
	    			    (65536.0 / wRWhiteLevel * 0x1000);
	  else
	    lpWhiteShading[i * 3] = 0x1000;

	  if (wGWhiteLevel > 0)
	    lpWhiteShading[i * 3 + 1] = (unsigned short)
	    				((65536 * 1.5) / wGWhiteLevel * 0x1000);
	  else
	    lpWhiteShading[i * 3 + 1] = 0x1000;

	  if (wBWhiteLevel > 0)
	    lpWhiteShading[i * 3 + 2] = (unsigned short)
	    				((65536 * 2.0) / wBWhiteLevel * 0x1000);
	  else
	    lpWhiteShading[i * 3 + 2] = 0x1000;
	  break;

	case SS_Positive:
	  if (wRWhiteLevel > 0)
	    lpWhiteShading[i * 3] = (unsigned short)
	    			    (65536.0 / wRWhiteLevel * 0x1000);
	  else
	    lpWhiteShading[i * 3] = 0x1000;

	  if (wGWhiteLevel > 0)
	    lpWhiteShading[i * 3 + 1] = (unsigned short)
	    				((65536 * 1.04) / wGWhiteLevel *0x1000);
	  else
	    lpWhiteShading[i * 3 + 1] = 0x1000;

	  if (wBWhiteLevel > 0)
	    lpWhiteShading[i * 3 + 2] = (unsigned short)
	    				(65536.0 / wBWhiteLevel * 0x1000);
	  else
	    lpWhiteShading[i * 3 + 2] = 0x1000;
	  break;
	}
    }

  free (lpWhiteData);
  free (lpDarkData);
  free (lpRWhiteSort);
  free (lpGWhiteSort);
  free (lpBWhiteSort);
  free (lpRDarkSort);
  free (lpGDarkSort);
  free (lpBDarkSort);

  Asic_SetShadingTable (&g_chip, lpWhiteShading, lpDarkShading, g_XDpi,
			wCalWidth);

  free (lpWhiteShading);
  free (lpDarkShading);

  DBG (DBG_FUNC, "MustScanner_LineCalibration16Bits: leave\n");
  return SANE_TRUE;

error:
  free (lpWhiteData);
  free (lpDarkData);
  return SANE_FALSE;
}

SANE_Bool
MustScanner_SetupScan (TARGETIMAGE *pTarget)
{
  unsigned short targetY;

  DBG (DBG_FUNC, "MustScanner_SetupScan: call in\n");

  if (g_bOpened || !g_bPrepared)
    {
      DBG (DBG_FUNC, "MustScanner_SetupScan: invalid state\n");
      return SANE_FALSE;
    }

  g_ScanMode = pTarget->cmColorMode;
  g_XDpi = pTarget->wDpi;
  g_YDpi = pTarget->wDpi;
  g_SWWidth = pTarget->wWidth;
  g_SWHeight = pTarget->wHeight;

  switch (g_YDpi)
    {
    case 1200:
      g_wPixelDistance = 4;	/* even & odd sensor problem */
      g_wLineDistance = 24;
      g_Height += g_wPixelDistance;
      break;
    case 600:
      g_wPixelDistance = 0;	/* no even & odd problem */
      g_wLineDistance = 12;
      break;
    case 300:
      g_wPixelDistance = 0;
      g_wLineDistance = 6;
      break;
    case 150:
      g_wPixelDistance = 0;
      g_wLineDistance = 3;
      break;
    case 75:
    case 50:
      g_wPixelDistance = 0;
      g_wLineDistance = 1;
      break;
    default:
      g_wPixelDistance = 0;
      g_wLineDistance = 0;
    }

  DBG (DBG_FUNC, "MustScanner_SetupScan: g_YDpi=%d\n", g_YDpi);
  DBG (DBG_FUNC, "MustScanner_SetupScan: g_wLineDistance=%d\n",
       g_wLineDistance);
  DBG (DBG_FUNC, "MustScanner_SetupScan: g_wPixelDistance=%d\n",
       g_wPixelDistance);

  switch (g_ScanMode)
    {
    case CM_RGB48:
      g_BytesPerRow = 6 * g_Width;	/* ASIC limit: width must be 8x */
      g_SWBytesPerRow = 6 * g_SWWidth;	/* ASIC limit: width must be 8x */
      g_bScanBits = 48;
      g_Height += g_wLineDistance * 2;
      break;
    case CM_RGB24:
      g_BytesPerRow = 3 * g_Width;
      g_SWBytesPerRow = 3 * g_SWWidth;
      g_bScanBits = 24;
      g_Height += g_wLineDistance * 2;
      break;
    case CM_GRAY16:
      g_BytesPerRow = 2 * g_Width;
      g_SWBytesPerRow = 2 * g_SWWidth;
      g_bScanBits = 16;
      break;
    case CM_GRAY8:
    case CM_TEXT:
      g_BytesPerRow = g_Width;
      g_SWBytesPerRow = g_SWWidth;
      g_bScanBits = 8;
      break;
    }

  if (Asic_Open (&g_chip) != SANE_STATUS_GOOD)
    {
      DBG (DBG_FUNC, "MustScanner_SetupScan: Asic_Open return error\n");
      return SANE_FALSE;
    }

  g_bOpened = SANE_TRUE;

  /* find left & top side */
  if (g_ssScanSource != SS_Reflective)
    Asic_MotorMove (&g_chip, SANE_TRUE, TRAN_START_POS);

  if (g_XDpi == SENSOR_DPI)
    {
      g_XDpi = SENSOR_DPI / 2;
      if (!MustScanner_AdjustAD ())
	return SANE_FALSE;
      if (!MustScanner_FindTopLeft (&g_X, &g_Y))
        {
	  g_X = 187;
	  g_Y = 43;
        }

      g_XDpi = SENSOR_DPI;
      if (!MustScanner_AdjustAD ())
        return SANE_FALSE;

      g_X = g_X * SENSOR_DPI / FIND_LEFT_TOP_CALIBRATE_RESOLUTION + pTarget->wX;
      if (g_ssScanSource == SS_Reflective)
        {
	  g_X += 47;
	  g_Y = g_Y * SENSOR_DPI / FIND_LEFT_TOP_CALIBRATE_RESOLUTION +
		pTarget->wY * SENSOR_DPI / g_YDpi + 47;
	}
    }
  else
    {
      if (!MustScanner_AdjustAD ())
        return SANE_FALSE;
      if (!MustScanner_FindTopLeft (&g_X, &g_Y))
        {
	  g_X = 187;
	  g_Y = 43;
        }

      g_X += pTarget->wX * (SENSOR_DPI / 2) / g_XDpi;
      if (g_ssScanSource == SS_Reflective)
        {
          if (g_XDpi != 75)
            g_X += 23;
	  g_Y = g_Y * SENSOR_DPI / FIND_LEFT_TOP_CALIBRATE_RESOLUTION +
		pTarget->wY * SENSOR_DPI / g_YDpi + 47;
         }
      else
        {
	  if (g_XDpi == 75)
	    g_X -= 23;
	}
    }

  DBG (DBG_FUNC, "MustScanner_SetupScan: before line calibration g_X=%d," \
		 "g_Y=%d\n", g_X, g_Y);

  if (!MustScanner_LineCalibration16Bits ())
    return SANE_FALSE;

  DBG (DBG_FUNC, "MustScanner_SetupScan: after " \
		 "MustScanner_LineCalibration16Bits g_X=%d,g_Y=%d\n", g_X, g_Y);

  DBG (DBG_FUNC, "MustScanner_SetupScan: g_bScanBits=%d, g_XDpi=%d, " \
		 "g_YDpi=%d, g_X=%d, g_Y=%d, g_Width=%d, g_Height=%d\n",
       g_bScanBits, g_XDpi, g_YDpi, g_X, g_Y, g_Width, g_Height);

  if (g_ssScanSource == SS_Reflective)
    {
      targetY = 300;
    }
  else
    {
      g_Y = pTarget->wY * SENSOR_DPI / g_YDpi + (300 - 40) + 189;
      targetY = 360;
    }

  if (g_Y > targetY)
    Asic_MotorMove (&g_chip, SANE_TRUE, g_Y - targetY);
  else
    Asic_MotorMove (&g_chip, SANE_FALSE, targetY - g_Y);
  g_Y = targetY;

  Asic_SetWindow (&g_chip, g_ssScanSource, SCAN_TYPE_NORMAL, g_bScanBits,
		  g_XDpi, g_YDpi, g_X, g_Y, g_Width, g_Height);

  DBG (DBG_FUNC, "MustScanner_SetupScan: leave\n");
  return MustScanner_PrepareScan ();
}
