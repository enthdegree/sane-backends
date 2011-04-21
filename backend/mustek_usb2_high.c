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
#include <math.h>
#include <pthread.h>	/* TODO: use sanei_thread functions instead */

#include "../include/sane/sane.h"
#include "../include/sane/sanei_backend.h"

#include "mustek_usb2_high.h"


/* HOLD: these global variables should go to scanner structure */

static SANE_Bool g_bOpened;
static SANE_Bool g_bPrepared;
static SANE_Bool g_isCanceled;
static SANE_Bool g_bFirstReadImage;
static SANE_Byte * g_pReadImageHead;

static TARGETIMAGE g_Target;
static unsigned short g_SWWidth;
static unsigned short g_SWHeight;

/* even & odd sensor problem */
static unsigned short g_wPixelDistance;
static unsigned short g_wLineDistance;
static unsigned short g_wScanLinesPerBlock;

static unsigned int g_wtheReadyLines;
static unsigned int g_wMaxScanLines;
static unsigned int g_dwScannedTotalLines;
static unsigned int g_BytesPerRow;
static unsigned int g_SWBytesPerRow;
static unsigned int g_dwTotalTotalXferLines;

static pthread_t g_threadid_readimage;
static pthread_mutex_t g_scannedLinesMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_readyLinesMutex = PTHREAD_MUTEX_INITIALIZER;

static unsigned short * g_pGammaTable;
static ASIC g_chip;

/* for modifying the last point */
static SANE_Bool g_bIsFirstReadBefData = SANE_TRUE;
static SANE_Byte * g_pBefLineImageData;
static unsigned int g_dwAlreadyGetLines;


void
Scanner_Init (void)
{
  DBG_ENTER();

  Asic_Initialize (&g_chip);

  g_bOpened = SANE_FALSE;
  g_bPrepared = SANE_FALSE;
  g_isCanceled = SANE_FALSE;

  g_pReadImageHead = NULL;
  g_pGammaTable = NULL;

  DBG_LEAVE();
}

SANE_Bool
Scanner_IsPresent (void)
{
  DBG_ENTER();

  if (Asic_Open (&g_chip) != SANE_STATUS_GOOD)
    return SANE_FALSE;
  if (Asic_Close (&g_chip) != SANE_STATUS_GOOD)
    return SANE_FALSE;

  DBG_LEAVE();
  return SANE_TRUE;
}

SANE_Bool
Scanner_PowerControl (SANE_Bool isLampOn, SANE_Bool isTALampOn)
{
  SANE_Bool hasTA;
  DBG_ENTER();

  if (Asic_Open (&g_chip) != SANE_STATUS_GOOD)
    return SANE_FALSE;

  if (Asic_TurnLamp (&g_chip, isLampOn) != SANE_STATUS_GOOD)
    goto error;

  if (Asic_IsTAConnected (&g_chip, &hasTA) != SANE_STATUS_GOOD)
    goto error;

  if (hasTA && (Asic_TurnTA (&g_chip, isTALampOn) != SANE_STATUS_GOOD))
    goto error;

  if (Asic_Close (&g_chip) != SANE_STATUS_GOOD)
    return SANE_FALSE;

  DBG_LEAVE();
  return SANE_TRUE;

error:
  Asic_Close (&g_chip);
  return SANE_FALSE;
}

SANE_Bool
Scanner_BackHome (void)
{
  DBG_ENTER();

  if (Asic_Open (&g_chip) != SANE_STATUS_GOOD)
    return SANE_FALSE;

  if (Asic_CarriageHome (&g_chip) != SANE_STATUS_GOOD)
    {
      Asic_Close (&g_chip);
      return SANE_FALSE;
    }

  if (Asic_Close (&g_chip) != SANE_STATUS_GOOD)
    return SANE_FALSE;

  DBG_LEAVE();
  return SANE_TRUE;
}

SANE_Bool
Scanner_IsTAConnected (void)
{
  SANE_Bool hasTA;
  DBG_ENTER();

  if (Asic_Open (&g_chip) != SANE_STATUS_GOOD)
    return SANE_FALSE;

  if (Asic_IsTAConnected (&g_chip, &hasTA) != SANE_STATUS_GOOD)
    hasTA = SANE_FALSE;

  if (Asic_Close (&g_chip) != SANE_STATUS_GOOD)
    return SANE_FALSE;

  DBG_LEAVE();
  return hasTA;
}

#ifdef SANE_UNUSED
SANE_Bool
Scanner_GetKeyStatus (SANE_Byte * pKey)
{
  DBG_ENTER();

  if (Asic_Open (&g_chip) != SANE_STATUS_GOOD)
    return SANE_FALSE;

  if (Asic_CheckFunctionKey (&g_chip, pKey) != SANE_STATUS_GOOD)
    {
      Asic_Close (&g_chip);
      return SANE_FALSE;
    }

  if (Asic_Close (&g_chip) != SANE_STATUS_GOOD)
    return SANE_FALSE;

  DBG_LEAVE();
  return SANE_TRUE;
}
#endif

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
SetPixel48Bit (SANE_Byte * pLine, unsigned short wRTempData,
	       unsigned short wGTempData, unsigned short wBTempData,
	       SANE_Bool isOrderInvert)
{
  if (!isOrderInvert)
    {
      pLine[0] = LOBYTE (g_pGammaTable[wRTempData]);
      pLine[1] = HIBYTE (g_pGammaTable[wRTempData]);
      pLine[2] = LOBYTE (g_pGammaTable[wGTempData + 0x10000]);
      pLine[3] = HIBYTE (g_pGammaTable[wGTempData + 0x10000]);
      pLine[4] = LOBYTE (g_pGammaTable[wBTempData + 0x20000]);
      pLine[5] = HIBYTE (g_pGammaTable[wBTempData + 0x20000]);
    }
  else
    {
      pLine[4] = LOBYTE (g_pGammaTable[wRTempData]);
      pLine[5] = HIBYTE (g_pGammaTable[wRTempData]);
      pLine[2] = LOBYTE (g_pGammaTable[wGTempData + 0x10000]);
      pLine[3] = HIBYTE (g_pGammaTable[wGTempData + 0x10000]);
      pLine[0] = LOBYTE (g_pGammaTable[wBTempData + 0x20000]);
      pLine[1] = HIBYTE (g_pGammaTable[wBTempData + 0x20000]);
    }
}

static void
GetRgb48BitLineHalf (SANE_Byte * pLine, SANE_Bool isOrderInvert)
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
      wRTempData = g_pReadImageHead[wRLinePos + i * 6] |
	(g_pReadImageHead[wRLinePos + i * 6 + 1] << 8);
      wGTempData = g_pReadImageHead[wGLinePos + i * 6 + 2] |
	(g_pReadImageHead[wGLinePos + i * 6 + 3] << 8);
      wBTempData = g_pReadImageHead[wBLinePos + i * 6 + 4] |
	(g_pReadImageHead[wBLinePos + i * 6 + 5] << 8);

      SetPixel48Bit (pLine + (i * 6), wRTempData, wGTempData, wBTempData,
		     isOrderInvert);
    }
}

static void
GetRgb48BitLineFull (SANE_Byte * pLine, SANE_Bool isOrderInvert)
{
  unsigned short wRLinePosOdd, wGLinePosOdd, wBLinePosOdd;
  unsigned short wRLinePosEven, wGLinePosEven, wBLinePosEven;
  unsigned int dwRTempData, dwGTempData, dwBTempData;
  unsigned short i = 0;

  if (g_Target.ssScanSource == SS_REFLECTIVE)
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

      dwRTempData = g_pReadImageHead[wRLinePosOdd + i * 6] |
	(g_pReadImageHead[wRLinePosOdd + i * 6 + 1] << 8);
      dwRTempData += g_pReadImageHead[wRLinePosEven + (i + 1) * 6] |
	(g_pReadImageHead[wRLinePosEven + (i + 1) * 6 + 1] << 8);
      dwRTempData /= 2;

      dwGTempData = g_pReadImageHead[wGLinePosOdd + i * 6 + 2] |
	(g_pReadImageHead[wGLinePosOdd + i * 6 + 3] << 8);
      dwGTempData += g_pReadImageHead[wGLinePosEven + (i + 1) * 6 + 2] |
	(g_pReadImageHead[wGLinePosEven + (i + 1) * 6 + 3] << 8);
      dwGTempData /= 2;

      dwBTempData = g_pReadImageHead[wBLinePosOdd + i * 6 + 4] |
	(g_pReadImageHead[wBLinePosOdd + i * 6 + 5] << 8);
      dwBTempData += g_pReadImageHead[wBLinePosEven + (i + 1) * 6 + 4] |
	(g_pReadImageHead[wBLinePosEven + (i + 1) * 6 + 5] << 8);
      dwBTempData /= 2;

      SetPixel48Bit (pLine + (i * 6), dwRTempData, dwGTempData, dwBTempData,
		     isOrderInvert);
      i++;

      dwRTempData = g_pReadImageHead[wRLinePosEven + i * 6] |
	(g_pReadImageHead[wRLinePosEven + i * 6 + 1] << 8);
      dwRTempData += g_pReadImageHead[wRLinePosOdd + (i + 1) * 6] |
	(g_pReadImageHead[wRLinePosOdd + (i + 1) * 6 + 1] << 8);
      dwRTempData /= 2;

      dwGTempData = g_pReadImageHead[wGLinePosEven + i * 6 + 2] |
	(g_pReadImageHead[wGLinePosEven + i * 6 + 3] << 8);
      dwGTempData += g_pReadImageHead[wGLinePosOdd + (i + 1) * 6 + 2] |
	(g_pReadImageHead[wGLinePosOdd + (i + 1) * 6 + 3] << 8);
      dwGTempData /= 2;

      dwBTempData = g_pReadImageHead[wBLinePosEven + i * 6 + 4] |
	(g_pReadImageHead[wBLinePosEven + i * 6 + 5] << 8);
      dwBTempData += g_pReadImageHead[wBLinePosOdd + (i + 1) * 6 + 4] |
	(g_pReadImageHead[wBLinePosOdd + (i + 1) * 6 + 5] << 8);
      dwBTempData /= 2;

      SetPixel48Bit (pLine + (i * 6), dwRTempData, dwGTempData, dwBTempData,
		     isOrderInvert);
      i++;
    }
}

static void
SetPixel24Bit (SANE_Byte * pLine, unsigned short tempR, unsigned short tempG,
	       unsigned short tempB, SANE_Bool isOrderInvert)
{
  if (!isOrderInvert)
    {
      pLine[0] = (SANE_Byte) g_pGammaTable[tempR];
      pLine[1] = (SANE_Byte) g_pGammaTable[4096 + tempG];
      pLine[2] = (SANE_Byte) g_pGammaTable[8192 + tempB];
    }
  else
    {
      pLine[2] = (SANE_Byte) g_pGammaTable[tempR];
      pLine[1] = (SANE_Byte) g_pGammaTable[4096 + tempG];
      pLine[0] = (SANE_Byte) g_pGammaTable[8192 + tempB];
    }
}

static void
GetRgb24BitLineHalf (SANE_Byte * pLine, SANE_Bool isOrderInvert)
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
      wRed = g_pReadImageHead[wRLinePos + i * 3];
      wRed += g_pReadImageHead[wRLinePos + (i + 1) * 3];
      wRed /= 2;

      wGreen = g_pReadImageHead[wGLinePos + i * 3 + 1];
      wGreen += g_pReadImageHead[wGLinePos + (i + 1) * 3 + 1];
      wGreen /= 2;

      wBlue = g_pReadImageHead[wBLinePos + i * 3 + 2];
      wBlue += g_pReadImageHead[wBLinePos + (i + 1) * 3 + 2];
      wBlue /= 2;

      tempR = (wRed << 4) | QBET4 (wBlue, wGreen);
      tempG = (wGreen << 4) | QBET4 (wRed, wBlue);
      tempB = (wBlue << 4) | QBET4 (wGreen, wRed);

      SetPixel24Bit (pLine + (i * 3), tempR, tempG, tempB, isOrderInvert);
    }
}

static void
GetRgb24BitLineFull (SANE_Byte * pLine, SANE_Bool isOrderInvert)
{
  unsigned short wRLinePosOdd, wGLinePosOdd, wBLinePosOdd;
  unsigned short wRLinePosEven, wGLinePosEven, wBLinePosEven;
  unsigned short wRed, wGreen, wBlue;
  unsigned short tempR, tempG, tempB;
  unsigned short i = 0;

  if (g_Target.ssScanSource == SS_REFLECTIVE)
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

      wRed = g_pReadImageHead[wRLinePosOdd + i * 3];
      wRed += g_pReadImageHead[wRLinePosEven + (i + 1) * 3];
      wRed /= 2;

      wGreen = g_pReadImageHead[wGLinePosOdd + i * 3 + 1];
      wGreen += g_pReadImageHead[wGLinePosEven + (i + 1) * 3 + 1];
      wGreen /= 2;

      wBlue = g_pReadImageHead[wBLinePosOdd + i * 3 + 2];
      wBlue += g_pReadImageHead[wBLinePosEven + (i + 1) * 3 + 2];
      wBlue /= 2;

      tempR = (wRed << 4) | QBET4 (wBlue, wGreen);
      tempG = (wGreen << 4) | QBET4 (wRed, wBlue);
      tempB = (wBlue << 4) | QBET4 (wGreen, wRed);

      SetPixel24Bit (pLine + (i * 3), tempR, tempG, tempB, isOrderInvert);
      i++;

      wRed = g_pReadImageHead[wRLinePosEven + i * 3];
      wRed += g_pReadImageHead[wRLinePosOdd + (i + 1) * 3];
      wRed /= 2;

      wGreen = g_pReadImageHead[wGLinePosEven + i * 3 + 1];
      wGreen += g_pReadImageHead[wGLinePosOdd + (i + 1) * 3 + 1];
      wGreen /= 2;

      wBlue = g_pReadImageHead[wBLinePosEven + i * 3 + 2];
      wBlue += g_pReadImageHead[wBLinePosOdd + (i + 1) * 3 + 2];
      wBlue /= 2;

      tempR = (wRed << 4) | QBET4 (wBlue, wGreen);
      tempG = (wGreen << 4) | QBET4 (wRed, wBlue);
      tempB = (wBlue << 4) | QBET4 (wGreen, wRed);

      SetPixel24Bit (pLine + (i * 3), tempR, tempG, tempB, isOrderInvert);
      i++;
    }
}

static void
GetMono16BitLineHalf (SANE_Byte * pLine,
		      SANE_Bool __sane_unused__ isOrderInvert)
{
  unsigned int dwTempData;
  unsigned short wLinePos;
  unsigned short i;

  wLinePos = (g_wtheReadyLines % g_wMaxScanLines) * g_BytesPerRow;

  for (i = 0; i < g_SWWidth; i++)
    {
      dwTempData = g_pReadImageHead[wLinePos + i * 2] |
		  (g_pReadImageHead[wLinePos + i * 2 + 1] << 8);
      pLine[i * 2] = LOBYTE (g_pGammaTable[dwTempData]);
      pLine[i * 2 + 1] = HIBYTE (g_pGammaTable[dwTempData]);
    }
}

static void
GetMono16BitLineFull (SANE_Byte * pLine,
		      SANE_Bool __sane_unused__ isOrderInvert)
{
  unsigned int dwTempData;
  unsigned short wLinePosOdd;
  unsigned short wLinePosEven;
  unsigned short i = 0;

  if (g_Target.ssScanSource == SS_REFLECTIVE)
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

      dwTempData = (unsigned int) g_pReadImageHead[wLinePosOdd + i * 2];
      dwTempData += (unsigned int)
		    g_pReadImageHead[wLinePosOdd + i * 2 + 1] << 8;
      dwTempData += (unsigned int)
		    g_pReadImageHead[wLinePosEven + (i + 1) * 2];
      dwTempData += (unsigned int)
		    g_pReadImageHead[wLinePosEven + (i + 1) * 2 + 1] << 8;
      dwTempData /= 2;

      pLine[i * 2] = LOBYTE (g_pGammaTable[dwTempData]);
      pLine[i * 2 + 1] = HIBYTE (g_pGammaTable[dwTempData]);
      i++;

      dwTempData = (unsigned int) g_pReadImageHead[wLinePosEven + i * 2];
      dwTempData += (unsigned int)
		    g_pReadImageHead[wLinePosEven + i * 2 + 1] << 8;
      dwTempData += (unsigned int)
		    g_pReadImageHead[wLinePosOdd + (i + 1) * 2];
      dwTempData += (unsigned int)
		    g_pReadImageHead[wLinePosOdd + (i + 1) * 2 + 1] << 8;
      dwTempData /= 2;

      pLine[i * 2] = LOBYTE (g_pGammaTable[dwTempData]);
      pLine[i * 2 + 1] = HIBYTE (g_pGammaTable[dwTempData]);
      i++;
    }
}

static void
GetMono8BitLineHalf (SANE_Byte * pLine,
		     SANE_Bool __sane_unused__ isOrderInvert)
{
  unsigned int dwTempData;
  unsigned short wLinePos;
  unsigned short i;

  wLinePos = (g_wtheReadyLines % g_wMaxScanLines) * g_BytesPerRow;

  for (i = 0; i < g_SWWidth; i++)
    {
      dwTempData = (g_pReadImageHead[wLinePos + i] << 4) | (rand () & 0x0f);
      pLine[i] = (SANE_Byte) g_pGammaTable[dwTempData];
    }
}

static void
GetMono8BitLineFull (SANE_Byte * pLine,
		     SANE_Bool __sane_unused__ isOrderInvert)
{
  unsigned short wLinePosOdd;
  unsigned short wLinePosEven;
  unsigned short wGray;
  unsigned short i = 0;

  if (g_Target.ssScanSource == SS_REFLECTIVE)
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

      wGray = g_pReadImageHead[wLinePosOdd + i];
      wGray += g_pReadImageHead[wLinePosEven + i + 1];
      wGray /= 2;

      pLine[i] = (SANE_Byte) g_pGammaTable[(wGray << 4) | (rand () & 0x0f)];
      i++;

      wGray = g_pReadImageHead[wLinePosEven + i];
      wGray += g_pReadImageHead[wLinePosOdd + i + 1];
      wGray /= 2;

      pLine[i] = (SANE_Byte) g_pGammaTable[(wGray << 4) | (rand () & 0x0f)];
      i++;
    }
}

static void
GetMono1BitLineHalf (SANE_Byte * pLine,
		     SANE_Bool __sane_unused__ isOrderInvert)
{
  unsigned short wLinePos;
  unsigned short i;

  wLinePos = (g_wtheReadyLines % g_wMaxScanLines) * g_BytesPerRow;

  for (i = 0; i < g_SWWidth; i++)
    {
      if (g_pReadImageHead[wLinePos + i] > g_Target.wLineartThreshold)
	pLine[i / 8] |= 0x80 >> (i % 8);
    }
}

static void
GetMono1BitLineFull (SANE_Byte * pLine,
		     SANE_Bool __sane_unused__ isOrderInvert)
{
  unsigned short wLinePosOdd;
  unsigned short wLinePosEven;
  unsigned short i = 0;

  if (g_Target.ssScanSource == SS_REFLECTIVE)
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

      if (g_pReadImageHead[wLinePosOdd + i] > g_Target.wLineartThreshold)
	pLine[i / 8] |= 0x80 >> (i % 8);
      i++;

      if (g_pReadImageHead[wLinePosEven + i] > g_Target.wLineartThreshold)
	pLine[i / 8] |= 0x80 >> (i % 8);
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
ModifyLinePoint (SANE_Byte * pImageData, SANE_Byte * pImageDataBefore,
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
	  pImageData[lineOffset] =
	    (pImageData[prevLineOffset] + pImageDataBefore[prevLineOffset]) / 2;

	  /* modify other lines */
	  for (wLines = 1; wLines < dwLinesCount; wLines++)
	    {
	      unsigned int dwBytesBefore = (wLines - 1) * dwBytesPerLine;
	      unsigned int dwBytes = wLines * dwBytesPerLine;
	      pImageData[dwBytes + lineOffset] =
		(pImageData[dwBytes + prevLineOffset] + 
		 pImageData[dwBytesBefore + prevLineOffset]) / 2;
	    }
	}
    }
}

static void *
ReadDataFromScanner (void __sane_unused__ * dummy)
{
  unsigned short wTotalReadImageLines = 0;
  unsigned short wWantedLines = g_Target.wHeight;
  SANE_Byte * pReadImage = g_pReadImageHead;
  SANE_Bool isWaitImageLineDiff = SANE_FALSE;
  unsigned int wMaxScanLines = g_wMaxScanLines;
  unsigned short wReadImageLines = 0;
  unsigned short wScanLinesThisBlock;
  unsigned short wBufferLines = g_wLineDistance * 2 + g_wPixelDistance;
  DBG_ENTER();
  DBG (DBG_FUNC, "ReadDataFromScanner: new thread\n");

  while (wTotalReadImageLines < wWantedLines)
    {
      if (!isWaitImageLineDiff)
	{
	  wScanLinesThisBlock =
	    (wWantedLines - wTotalReadImageLines) < g_wScanLinesPerBlock ?
	    (wWantedLines - wTotalReadImageLines) : g_wScanLinesPerBlock;

	  DBG (DBG_FUNC, "ReadDataFromScanner: wWantedLines=%d\n",
	       wWantedLines);
	  DBG (DBG_FUNC, "ReadDataFromScanner: wScanLinesThisBlock=%d\n",
	       wScanLinesThisBlock);

	  if (Asic_ReadImage (&g_chip, pReadImage, wScanLinesThisBlock) !=
	      SANE_STATUS_GOOD)
	    {
	      DBG (DBG_FUNC, "ReadDataFromScanner: thread exit\n");
	      return NULL;
	    }

	  /* has read in memory buffer */
	  wReadImageLines += wScanLinesThisBlock;
	  AddScannedLines (wScanLinesThisBlock);
	  wTotalReadImageLines += wScanLinesThisBlock;
	  pReadImage += wScanLinesThisBlock * g_BytesPerRow;

	  /* buffer is full */
	  if (wReadImageLines >= wMaxScanLines)
	    {
	      pReadImage = g_pReadImageHead;
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

  DBG (DBG_FUNC, "ReadDataFromScanner: read OK, exit thread\n");
  DBG_LEAVE();
  return NULL;
}

static SANE_Bool
GetLine (SANE_Byte * pLine, unsigned short * wLinesCount,
	 unsigned int dwLineIncrement, void (* pFunc)(SANE_Byte *, SANE_Bool),
	 SANE_Bool isOrderInvert, SANE_Bool fixEvenOdd)
{
  unsigned short wWantedTotalLines;
  unsigned short TotalXferLines = 0;
  SANE_Byte * pFirstLine = pLine;
  DBG_ENTER();

  wWantedTotalLines = *wLinesCount;

  if (g_bFirstReadImage)
    {
      pthread_create (&g_threadid_readimage, NULL, ReadDataFromScanner, NULL);
      DBG (DBG_FUNC, "thread started\n");
      g_bFirstReadImage = SANE_FALSE;
    }

  while (TotalXferLines < wWantedTotalLines)
    {
      if (g_dwTotalTotalXferLines >= g_SWHeight)
	{
	  pthread_cancel (g_threadid_readimage);
	  pthread_join (g_threadid_readimage, NULL);
	  DBG (DBG_FUNC, "thread finished\n");

	  *wLinesCount = TotalXferLines;
	  return SANE_TRUE;
	}

      if (GetScannedLines () > g_wtheReadyLines)
	{
	  pFunc (pLine, isOrderInvert);

	  TotalXferLines++;
	  g_dwTotalTotalXferLines++;
	  pLine += dwLineIncrement;
	  AddReadyLines ();
	}

      if (g_isCanceled)
	{
	  pthread_join (g_threadid_readimage, NULL);
	  DBG (DBG_FUNC, "thread finished\n");
	  break;
	}
    }

  *wLinesCount = TotalXferLines;

  if (fixEvenOdd)
    {
      /* modify the last point */
      if (g_bIsFirstReadBefData)
	{
	  g_pBefLineImageData = malloc (g_SWBytesPerRow);
	  if (!g_pBefLineImageData)
	    return SANE_FALSE;
	  memset (g_pBefLineImageData, 0, g_SWBytesPerRow);
	  memcpy (g_pBefLineImageData, pFirstLine, g_SWBytesPerRow);
	  g_bIsFirstReadBefData = SANE_FALSE;
	  g_dwAlreadyGetLines = 0;
	}

      ModifyLinePoint (pFirstLine, g_pBefLineImageData, g_SWBytesPerRow,
		       wWantedTotalLines, 1, 4);

      memcpy (g_pBefLineImageData,
	      pFirstLine + (wWantedTotalLines - 1) * g_SWBytesPerRow,
	      g_SWBytesPerRow);
      g_dwAlreadyGetLines += wWantedTotalLines;
      if (g_dwAlreadyGetLines >= g_SWHeight)
	{
	  DBG (DBG_FUNC, "freeing g_pBefLineImageData\n");
	  free (g_pBefLineImageData);
	  g_bIsFirstReadBefData = SANE_TRUE;
	}
    }

  DBG_LEAVE();
  return SANE_TRUE;
}

SANE_Bool
Scanner_GetRows (SANE_Byte * pBlock, unsigned short * Rows,
		 SANE_Bool isOrderInvert)
{
  unsigned int dwLineIncrement = g_SWBytesPerRow;
  SANE_Bool fixEvenOdd = SANE_FALSE;
  void (* pFunc)(SANE_Byte *, SANE_Bool) = NULL;
  DBG_ENTER();

  if (!g_bOpened || !g_bPrepared)
    {
      DBG (DBG_FUNC, "invalid state\n");
      return SANE_FALSE;
    }

  switch (g_Target.cmColorMode)
    {
    case CM_RGB48:
      if (g_Target.wXDpi == SENSOR_DPI)
	pFunc = GetRgb48BitLineFull;
      else
	pFunc = GetRgb48BitLineHalf;
      break;

    case CM_RGB24:
      if (g_Target.wXDpi == SENSOR_DPI)
	pFunc = GetRgb24BitLineFull;
      else
	pFunc = GetRgb24BitLineHalf;
      break;

    case CM_GRAY16:
      if (g_Target.wXDpi == SENSOR_DPI)
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
      if (g_Target.wXDpi == SENSOR_DPI)
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
      memset (pBlock, 0, *Rows * g_SWWidth / 8);
      dwLineIncrement /= 8;
      if (g_Target.wXDpi == SENSOR_DPI)
	pFunc = GetMono1BitLineFull;
      else
	pFunc = GetMono1BitLineHalf;
      break;
    }

  DBG_LEAVE();
  return GetLine (pBlock, Rows, dwLineIncrement, pFunc, isOrderInvert,
		  fixEvenOdd);
}

void
Scanner_ScanSuggest (TARGETIMAGE * pTarget)
{
  unsigned short wMaxWidth, wMaxHeight;
  DBG_ENTER();

  /* check width and height */
  wMaxWidth = (MAX_SCANNING_WIDTH * pTarget->wXDpi) / 300;
  wMaxHeight = (MAX_SCANNING_HEIGHT * pTarget->wYDpi) / 300;
  DBG (DBG_FUNC, "wMaxWidth=%d,wMaxHeight=%d\n", wMaxWidth, wMaxHeight);

  pTarget->wWidth = _MIN (pTarget->wWidth, wMaxWidth);
  pTarget->wHeight = _MIN (pTarget->wHeight, wMaxHeight);

  DBG_LEAVE();
}

SANE_Bool
Scanner_StopScan (void)
{
  SANE_Bool result = SANE_TRUE;
  DBG_ENTER();

  if (!g_bOpened || !g_bPrepared)
    {
      DBG (DBG_FUNC, "invalid state\n");
      return SANE_FALSE;
    }

  g_isCanceled = SANE_TRUE;

  pthread_cancel (g_threadid_readimage);
  pthread_join (g_threadid_readimage, NULL);
  DBG (DBG_FUNC, "thread finished\n");

  if (Asic_ScanStop (&g_chip) != SANE_STATUS_GOOD)
    result = SANE_FALSE;
  if (Asic_Close (&g_chip) != SANE_STATUS_GOOD)
    result = SANE_FALSE;

  g_bOpened = SANE_FALSE;

  if (g_pGammaTable)
    {
      free (g_pGammaTable);
      g_pGammaTable = NULL;
    }

  if (g_pReadImageHead)
    {
      free (g_pReadImageHead);
      g_pReadImageHead = NULL;
    }

  DBG_LEAVE();
  return result;
}

static SANE_Bool
PrepareScan (void)
{
  DBG_ENTER();

  g_isCanceled = SANE_FALSE;

  g_wScanLinesPerBlock = BLOCK_SIZE / g_BytesPerRow;
  g_wMaxScanLines = ((IMAGE_BUFFER_SIZE / g_BytesPerRow) /
		     g_wScanLinesPerBlock) * g_wScanLinesPerBlock;
  g_dwScannedTotalLines = 0;

  switch (g_Target.cmColorMode)
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
  DBG (DBG_FUNC, "g_wtheReadyLines=%d\n", g_wtheReadyLines);

  DBG (DBG_FUNC, "allocate %d bytes for g_pReadImageHead\n", IMAGE_BUFFER_SIZE);
  g_pReadImageHead = malloc (IMAGE_BUFFER_SIZE);
  if (!g_pReadImageHead)
    {
      DBG (DBG_FUNC, "g_pReadImageHead == NULL\n");
      return SANE_FALSE;
    }

  if (Asic_ScanStart (&g_chip) != SANE_STATUS_GOOD)
    {
      free (g_pReadImageHead);
      return SANE_FALSE;
    }

  DBG_LEAVE();
  return SANE_TRUE;
}

SANE_Bool
Scanner_Reset (void)
{
  DBG_ENTER();

  if (g_bOpened)
    {
      DBG (DBG_FUNC, "scanner already open\n");
      return SANE_FALSE;
    }

  g_dwTotalTotalXferLines = 0;
  g_bFirstReadImage = SANE_TRUE;
  g_bPrepared = SANE_TRUE;

  DBG_LEAVE();
  return SANE_TRUE;
}

static void
PrepareCalculateMaxMin (unsigned short wResolution,
			CALIBRATIONPARAM * pCalParam)
{
  unsigned short wCalWidth, wDarkCalWidth;

  wDarkCalWidth = 52;
  if (wResolution <= (SENSOR_DPI / 2))
    {
      wCalWidth = (5120 * wResolution / (SENSOR_DPI / 2) + 511) & ~511;
      wDarkCalWidth *= wResolution / SENSOR_DPI;

      if (wResolution < 200)
	{
	  pCalParam->nPowerNum = 3;
	  pCalParam->nSecLength = 8;	/* 2^nPowerNum */
	  /* dark has at least 2 sections */
	  pCalParam->nDarkSecLength = wDarkCalWidth / 2;
	}
      else
	{
	  pCalParam->nPowerNum = 6;
	  pCalParam->nSecLength = 64;	/* 2^nPowerNum */
	  pCalParam->nDarkSecLength = wDarkCalWidth / 3;
	}
    }
  else
    {
      pCalParam->nPowerNum = 6;
      pCalParam->nSecLength = 64;	/* 2^nPowerNum */
      wCalWidth = 10240;
      pCalParam->nDarkSecLength = wDarkCalWidth / 5;
    }

  if (pCalParam->nDarkSecLength <= 0)
    pCalParam->nDarkSecLength = 1;

  pCalParam->wStartPosition = 13 * wResolution / SENSOR_DPI;
  wCalWidth -= pCalParam->wStartPosition;

  /* start of find max value */
  pCalParam->nSecNum = wCalWidth / pCalParam->nSecLength;

  /* start of find min value */
  pCalParam->nDarkSecNum = wDarkCalWidth / pCalParam->nDarkSecLength;
}

static SANE_Bool
CalculateMaxMin (CALIBRATIONPARAM * pCalParam, SANE_Byte * pBuffer,
		 unsigned short * pMaxValue, unsigned short * pMinValue)
{
  unsigned short *wSecData, *wDarkSecData;
  int i, j;

  wSecData = malloc (pCalParam->nSecNum * sizeof (unsigned short));
  if (!wSecData)
    return SANE_FALSE;
  memset (wSecData, 0, pCalParam->nSecNum * sizeof (unsigned short));

  for (i = 0; i < pCalParam->nSecNum; i++)
    {
      for (j = 0; j < pCalParam->nSecLength; j++)
	wSecData[i] += pBuffer[pCalParam->wStartPosition +
			       i * pCalParam->nSecLength + j];
      wSecData[i] >>= pCalParam->nPowerNum;
    }

  *pMaxValue = wSecData[0];
  for (i = 0; i < pCalParam->nSecNum; i++)
    {
      if (*pMaxValue < wSecData[i])
	*pMaxValue = wSecData[i];
    }
  free (wSecData);

  wDarkSecData = malloc (pCalParam->nDarkSecNum * sizeof (unsigned short));
  if (!wDarkSecData)
    return SANE_FALSE;
  memset (wDarkSecData, 0, pCalParam->nDarkSecNum * sizeof (unsigned short));

  for (i = 0; i < pCalParam->nDarkSecNum; i++)
    {
      for (j = 0; j < pCalParam->nDarkSecLength; j++)
	wDarkSecData[i] += pBuffer[pCalParam->wStartPosition +
				   i * pCalParam->nDarkSecLength + j];
      wDarkSecData[i] /= pCalParam->nDarkSecLength;
    }

  *pMinValue = wDarkSecData[0];
  for (i = 0; i < pCalParam->nDarkSecNum; i++)
    {
      if (*pMinValue > wDarkSecData[i])
	*pMinValue = wDarkSecData[i];
    }
  free (wDarkSecData);

  return SANE_TRUE;
}

static SANE_Bool
AdjustAD (void)
{
  CALIBRATIONPARAM calParam;
  SANE_Byte * pCalData;
  unsigned short wCalWidth;
  unsigned short wMaxValue[3], wMinValue[3];
  unsigned short wAdjustADResolution;
  int i, j;
  double range;
#if DEBUG_SAVE_IMAGE
  FILE * stream;
  char buf[16];
#endif
  DBG_ENTER();

  for (i = 0; i < 3; i++)
    {
      g_chip.AD.Direction[i] = DIR_POSITIVE;
      g_chip.AD.Gain[i] = 0;
    }
  if (g_Target.ssScanSource == SS_REFLECTIVE)
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

  if (g_Target.wXDpi <= (SENSOR_DPI / 2))
    wAdjustADResolution = SENSOR_DPI / 2;
  else
    wAdjustADResolution = SENSOR_DPI;

  wCalWidth = 10240;
  pCalData = malloc (wCalWidth * 3);
  if (!pCalData)
    {
      DBG (DBG_FUNC, "pCalData == NULL\n");
      return SANE_FALSE;
    }

  if (Asic_SetWindow (&g_chip, g_Target.ssScanSource, SCAN_TYPE_CALIBRATE_DARK,
		      24, wAdjustADResolution, wAdjustADResolution, 0, 0,
		      wCalWidth, 1) != SANE_STATUS_GOOD)
    goto error;
  PrepareCalculateMaxMin (wAdjustADResolution, &calParam);

  for (i = 0; i < 10; i++)
    {
      DBG (DBG_FUNC, "first AD offset adjustment loop\n");
      SetAFEGainOffset (&g_chip);
      if (Asic_ScanStart (&g_chip) != SANE_STATUS_GOOD)
	goto error;
      if (Asic_ReadCalibrationData (&g_chip, pCalData, wCalWidth * 3, 24) !=
	  SANE_STATUS_GOOD)
	goto error;
      if (Asic_ScanStop (&g_chip) != SANE_STATUS_GOOD)
	goto error;

#ifdef DEBUG_SAVE_IMAGE
      if (i == 0)
	{
	  stream = fopen ("AD.ppm", "wb");
	  snprintf (buf, sizeof (buf), "P6\n%u 1\n255\n", wCalWidth);
	  fwrite (buf, 1, strlen (buf), stream);
	  fwrite (pCalData, 1, wCalWidth * 3, stream);
	  fclose (stream);
	}
#endif

      for (j = 0; j < 3; j++)
	{
	  if (!CalculateMaxMin (&calParam, pCalData + (wCalWidth * j),
				&wMaxValue[j], &wMinValue[j]))
	    goto error;

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

  DBG (DBG_FUNC, "OffsetR=%d,OffsetG=%d,OffsetB=%d\n",
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

  DBG (DBG_FUNC, "GainR=%d,GainG=%d,GainB=%d\n",
       g_chip.AD.Gain[0], g_chip.AD.Gain[1], g_chip.AD.Gain[2]);

  for (i = 0; i < 10; i++)
    {
      SetAFEGainOffset (&g_chip);
      if (Asic_ScanStart (&g_chip) != SANE_STATUS_GOOD)
	goto error;
      if (Asic_ReadCalibrationData (&g_chip, pCalData, wCalWidth * 3, 24) !=
	  SANE_STATUS_GOOD)
	goto error;
      if (Asic_ScanStop (&g_chip) != SANE_STATUS_GOOD)
	goto error;

      for (j = 0; j < 3; j++)
	{
	  if (!CalculateMaxMin (&calParam, pCalData + (wCalWidth * j),
				&wMaxValue[j], &wMinValue[j]))
	    goto error;

	  DBG (DBG_FUNC, "%d: Max=%d, Min=%d\n", j, wMaxValue[j], wMinValue[j]);

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

	  DBG (DBG_FUNC, "%d: Gain=%d,Offset=%d,Dir=%d\n",
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
      DBG (DBG_FUNC, "second AD offset adjustment loop\n");
      SetAFEGainOffset (&g_chip);
      if (Asic_ScanStart (&g_chip) != SANE_STATUS_GOOD)
	goto error;
      if (Asic_ReadCalibrationData (&g_chip, pCalData, wCalWidth * 3, 24) !=
	  SANE_STATUS_GOOD)
	goto error;
      if (Asic_ScanStop (&g_chip) != SANE_STATUS_GOOD)
	goto error;

      for (j = 0; j < 3; j++)
	{
	  if (!CalculateMaxMin (&calParam, pCalData + (wCalWidth * j),
				&wMaxValue[j], &wMinValue[j]))
	    goto error;

	  DBG (DBG_FUNC, "%d: Max=%d, Min=%d\n", j, wMaxValue[j], wMinValue[j]);

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

	  DBG (DBG_FUNC, "%d: Gain=%d,Offset=%d,Dir=%d\n",
	       j, g_chip.AD.Gain[j], g_chip.AD.Offset[j],
	       g_chip.AD.Direction[j]);
	}

      if (!(wMinValue[0] > 20 || wMinValue[0] < 10 ||
	    wMinValue[1] > 20 || wMinValue[1] < 10 ||
	    wMinValue[2] > 20 || wMinValue[2] < 10))
	break;
    }

  free (pCalData);

  DBG_LEAVE();
  return SANE_TRUE;

error:
  free (pCalData);
  return SANE_FALSE;
}

static SANE_Bool
FindTopLeft (unsigned short * pwStartX, unsigned short * pwStartY)
{
  unsigned short wCalWidth, wCalHeight;
  unsigned int dwTotalSize;
  int nScanBlock;
  SANE_Byte * pCalData;
  unsigned short wLeftSide, wTopSide;
  int i, j;
#ifdef DEBUG_SAVE_IMAGE
  FILE * stream;
  char buf[20];
#endif
  DBG_ENTER();

  if (g_Target.ssScanSource == SS_REFLECTIVE)
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
  pCalData = malloc (dwTotalSize);
  if (!pCalData)
    {
      DBG (DBG_FUNC, "pCalData == NULL\n");
      return SANE_FALSE;
    }
  nScanBlock = dwTotalSize / CALIBRATION_BLOCK_SIZE;

  if (Asic_SetWindow (&g_chip, g_Target.ssScanSource, SCAN_TYPE_CALIBRATE_LIGHT,
		      8, FIND_LEFT_TOP_CALIBRATE_RESOLUTION,
		      FIND_LEFT_TOP_CALIBRATE_RESOLUTION, 0, 0,
		      wCalWidth, wCalHeight) != SANE_STATUS_GOOD)
    goto error;

  SetAFEGainOffset (&g_chip);
  if (Asic_ScanStart (&g_chip) != SANE_STATUS_GOOD)
    goto error;

  for (i = 0; i < nScanBlock; i++)
    {
      if (Asic_ReadCalibrationData (&g_chip,
	    pCalData + (i * CALIBRATION_BLOCK_SIZE), CALIBRATION_BLOCK_SIZE,
	    8) != SANE_STATUS_GOOD)
	goto error;
    }

  if (Asic_ReadCalibrationData (&g_chip,
	pCalData + (nScanBlock * CALIBRATION_BLOCK_SIZE),
	dwTotalSize - CALIBRATION_BLOCK_SIZE * nScanBlock, 8) !=
      SANE_STATUS_GOOD)
    goto error;

  if (Asic_ScanStop (&g_chip) != SANE_STATUS_GOOD)
    goto error;

#ifdef DEBUG_SAVE_IMAGE
  stream = fopen ("bounds.pgm", "wb");
  snprintf (buf, sizeof (buf), "P5\n%d %d\n255\n", wCalWidth, wCalHeight);
  fwrite (buf, 1, strlen (buf), stream);
  fwrite (pCalData, 1, dwTotalSize, stream);
  fclose (stream);
#endif

  /* find left side */
  for (i = wCalWidth - 1; i > 0; i--)
    {
      wLeftSide = pCalData[i];
      wLeftSide += pCalData[wCalWidth * 2 + i];
      wLeftSide += pCalData[wCalWidth * 4 + i];
      wLeftSide += pCalData[wCalWidth * 6 + i];
      wLeftSide += pCalData[wCalWidth * 8 + i];
      wLeftSide /= 5;

      if (wLeftSide < 60)
	{
	  if (i < (wCalWidth - 1))
	    *pwStartX = i;
	  break;
	}
    }

  if (g_Target.ssScanSource == SS_REFLECTIVE)
    {
      /* find top side, i = left side */
      for (j = 0; j < wCalHeight; j++)
	{
	  wTopSide = pCalData[wCalWidth * j + i - 2];
	  wTopSide += pCalData[wCalWidth * j + i - 4];
	  wTopSide += pCalData[wCalWidth * j + i - 6];
	  wTopSide += pCalData[wCalWidth * j + i - 8];
	  wTopSide += pCalData[wCalWidth * j + i - 10];
	  wTopSide /= 5;

	  if (wTopSide > 60)
	    {
	      if (j > 0)
		*pwStartY = j;
	      break;
	    }
	}

      if ((*pwStartX < 100) || (*pwStartX > 250))
	*pwStartX = 187;
      if ((*pwStartY < 10) || (*pwStartY > 100))
	*pwStartY = 43;

      if (Asic_MotorMove (&g_chip, SANE_FALSE,
			  (wCalHeight - *pwStartY + LINE_CALIBRATION_HEIGHT) *
			  SENSOR_DPI / FIND_LEFT_TOP_CALIBRATE_RESOLUTION) !=
	  SANE_STATUS_GOOD)
	goto error;
    }
  else
    {
      /* find top side, i = left side */
      for (j = 0; j < wCalHeight; j++)
	{
	  wTopSide = pCalData[wCalWidth * j + i + 2];
	  wTopSide += pCalData[wCalWidth * j + i + 4];
	  wTopSide += pCalData[wCalWidth * j + i + 6];
	  wTopSide += pCalData[wCalWidth * j + i + 8];
	  wTopSide += pCalData[wCalWidth * j + i + 10];
	  wTopSide /= 5;

	  if (wTopSide < 60)
	    {
	      if (j > 0)
		*pwStartY = j;
	      break;
	    }
	}

      if ((*pwStartX < 2200) || (*pwStartX > 2300))
	*pwStartX = 2260;
      if ((*pwStartY < 100) || (*pwStartY > 200))
	*pwStartY = 124;

      if (Asic_MotorMove (&g_chip, SANE_FALSE,
			  (wCalHeight - *pwStartY) * SENSOR_DPI /
			  FIND_LEFT_TOP_CALIBRATE_RESOLUTION + 300) !=
	  SANE_STATUS_GOOD)
	goto error;
    }

  free (pCalData);

  DBG (DBG_FUNC, "pwStartY=%d,pwStartX=%d\n", *pwStartY, *pwStartX);
  DBG_LEAVE();
  return SANE_TRUE;

error:
  free (pCalData);
  return SANE_FALSE;
}

static unsigned short
FiltLower (unsigned short * pSort, unsigned short TotalCount,
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
LineCalibration16Bits (void)
{
  SANE_Byte * pWhiteData, * pDarkData;
  unsigned int dwTotalSize;
  unsigned short wCalWidth, wCalHeight;
  unsigned short * pWhiteShading, * pDarkShading;
  double wRWhiteLevel, wGWhiteLevel, wBWhiteLevel;
  unsigned int dwRDarkLevel = 0, dwGDarkLevel = 0, dwBDarkLevel = 0;
  unsigned int dwREvenDarkLevel = 0, dwGEvenDarkLevel = 0, dwBEvenDarkLevel = 0;
  unsigned short * pRWhiteSort, * pGWhiteSort, * pBWhiteSort;
  unsigned short * pRDarkSort, * pGDarkSort, * pBDarkSort;
  int i, j;
#ifdef DEBUG_SAVE_IMAGE
  FILE * stream;
  char buf[22];
#endif
  DBG_ENTER();

  wCalWidth = g_Target.wWidth;
  wCalHeight = LINE_CALIBRATION_HEIGHT;
  dwTotalSize = wCalWidth * wCalHeight * 3 * 2;
  DBG (DBG_FUNC, "wCalWidth=%d,wCalHeight=%d\n", wCalWidth, wCalHeight);

  pWhiteData = malloc (dwTotalSize);
  pDarkData = malloc (dwTotalSize);
  if (!pWhiteData || !pDarkData)
    {
      DBG (DBG_FUNC, "malloc failed\n");
      return SANE_FALSE;
    }

  /* read white level data */
  SetAFEGainOffset (&g_chip);
  if (Asic_SetWindow (&g_chip, g_Target.ssScanSource, SCAN_TYPE_CALIBRATE_LIGHT,
		      48, g_Target.wXDpi, 600, g_Target.wX, 0,
		      wCalWidth, wCalHeight) != SANE_STATUS_GOOD)
    goto error;

  if (Asic_ScanStart (&g_chip) != SANE_STATUS_GOOD)
    goto error;

  if (Asic_ReadCalibrationData (&g_chip, pWhiteData, dwTotalSize, 8) !=
      SANE_STATUS_GOOD)
    goto error;

  if (Asic_ScanStop (&g_chip) != SANE_STATUS_GOOD)
    goto error;

  /* read dark level data */
  SetAFEGainOffset (&g_chip);
  if (Asic_SetWindow (&g_chip, g_Target.ssScanSource, SCAN_TYPE_CALIBRATE_DARK,
		      48, g_Target.wXDpi, 600, g_Target.wX, 0, wCalWidth,
		      wCalHeight) != SANE_STATUS_GOOD)
    goto error;

  if (g_Target.ssScanSource == SS_REFLECTIVE)
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

  if (Asic_ReadCalibrationData (&g_chip, pDarkData, dwTotalSize, 8) !=
      SANE_STATUS_GOOD)
    goto error;

  if (Asic_ScanStop (&g_chip) != SANE_STATUS_GOOD)
    goto error;

  if (g_Target.ssScanSource == SS_REFLECTIVE)
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
  fwrite (pWhiteData, 1, dwTotalSize, stream);
  fclose (stream);

  stream = fopen ("darkshading.ppm", "wb");
  snprintf (buf, sizeof (buf), "P6\n%d %d\n65535\n", wCalWidth, wCalHeight);
  fwrite (buf, 1, strlen (buf), stream);
  fwrite (pDarkData, 1, dwTotalSize, stream);
  fclose (stream);
#endif

  sleep (1);

  pWhiteShading = malloc (sizeof (unsigned short) * wCalWidth * 3);
  pDarkShading = malloc (sizeof (unsigned short) * wCalWidth * 3);

  pRWhiteSort = malloc (sizeof (unsigned short) * wCalHeight);
  pGWhiteSort = malloc (sizeof (unsigned short) * wCalHeight);
  pBWhiteSort = malloc (sizeof (unsigned short) * wCalHeight);
  pRDarkSort = malloc (sizeof (unsigned short) * wCalHeight);
  pGDarkSort = malloc (sizeof (unsigned short) * wCalHeight);
  pBDarkSort = malloc (sizeof (unsigned short) * wCalHeight);

  if (!pWhiteShading || !pDarkShading ||
      !pRWhiteSort || !pGWhiteSort || !pBWhiteSort ||
      !pRDarkSort || !pGDarkSort || !pBDarkSort)
    {
      DBG (DBG_FUNC, "malloc failed\n");
      goto error;
    }

  /* compute dark level */
  for (i = 0; i < wCalWidth; i++)
    {
      for (j = 0; j < wCalHeight; j++)
	{
	  pRDarkSort[j] = pDarkData[j * wCalWidth * 6 + i * 6];
	  pRDarkSort[j] += pDarkData[j * wCalWidth * 6 + i * 6 + 1] << 8;

	  pGDarkSort[j] = pDarkData[j * wCalWidth * 6 + i * 6 + 2];
	  pGDarkSort[j] += pDarkData[j * wCalWidth * 6 + i * 6 + 3] << 8;

	  pBDarkSort[j] = pDarkData[j * wCalWidth * 6 + i * 6 + 4];
	  pBDarkSort[j] += pDarkData[j * wCalWidth * 6 + i * 6 + 5] << 8;
	}

      /* sum of dark level for all pixels */
      if ((g_Target.wXDpi == SENSOR_DPI) && ((i % 2) == 0))
	{
    	  /* compute dark shading table with mean */
	  dwREvenDarkLevel += FiltLower (pRDarkSort, wCalHeight, 20, 30);
	  dwGEvenDarkLevel += FiltLower (pGDarkSort, wCalHeight, 20, 30);
	  dwBEvenDarkLevel += FiltLower (pBDarkSort, wCalHeight, 20, 30);
	}
      else
	{
	  dwRDarkLevel += FiltLower (pRDarkSort, wCalHeight, 20, 30);
	  dwGDarkLevel += FiltLower (pGDarkSort, wCalHeight, 20, 30);
	  dwBDarkLevel += FiltLower (pBDarkSort, wCalHeight, 20, 30);
	}
    }

  if (g_Target.wXDpi == SENSOR_DPI)
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
  if (g_Target.ssScanSource != SS_REFLECTIVE)
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
	  pRWhiteSort[j] = pWhiteData[j * wCalWidth * 2 * 3 + i * 6];
	  pRWhiteSort[j] += pWhiteData[j * wCalWidth * 2 * 3 + i * 6 + 1] << 8;

	  pGWhiteSort[j] = pWhiteData[j * wCalWidth * 2 * 3 + i * 6 + 2];
	  pGWhiteSort[j] += pWhiteData[j * wCalWidth * 2 * 3 + i * 6 + 3] << 8;

	  pBWhiteSort[j] = pWhiteData[j * wCalWidth * 2 * 3 + i * 6 + 4];
	  pBWhiteSort[j] += pWhiteData[j * wCalWidth * 2 * 3 + i * 6 + 5] << 8;
	}

      if ((g_Target.wXDpi == SENSOR_DPI) && ((i % 2) == 0))
	{
	  pDarkShading[i * 3] = dwREvenDarkLevel;
	  pDarkShading[i * 3 + 1] = dwGEvenDarkLevel;
	  if (g_Target.ssScanSource == SS_POSITIVE)
	    pDarkShading[i * 3 + 1] *= 0.78;
	  pDarkShading[i * 3 + 2] = dwBEvenDarkLevel;
	}
      else
	{
	  pDarkShading[i * 3] = dwRDarkLevel;
	  pDarkShading[i * 3 + 1] = dwGDarkLevel;
	  if (g_Target.ssScanSource == SS_POSITIVE)
	    pDarkShading[i * 3 + 1] *= 0.78;
	  pDarkShading[i * 3 + 2] = dwBDarkLevel;
	}

      wRWhiteLevel = FiltLower (pRWhiteSort, wCalHeight, 20, 30) -
		     pDarkShading[i * 3];
      wGWhiteLevel = FiltLower (pGWhiteSort, wCalHeight, 20, 30) -
		     pDarkShading[i * 3 + 1];
      wBWhiteLevel = FiltLower (pBWhiteSort, wCalHeight, 20, 30) -
		     pDarkShading[i * 3 + 2];

      switch (g_Target.ssScanSource)
	{
	case SS_REFLECTIVE:
	  if (wRWhiteLevel > 0)
	    pWhiteShading[i * 3] = (unsigned short)
				   (65535.0 / wRWhiteLevel * 0x2000);
	  else
	    pWhiteShading[i * 3] = 0x2000;

	  if (wGWhiteLevel > 0)
	    pWhiteShading[i * 3 + 1] = (unsigned short)
				       (65535.0 / wGWhiteLevel * 0x2000);
	  else
	    pWhiteShading[i * 3 + 1] = 0x2000;

	  if (wBWhiteLevel > 0)
	    pWhiteShading[i * 3 + 2] = (unsigned short)
				       (65535.0 / wBWhiteLevel * 0x2000);
	  else
	    pWhiteShading[i * 3 + 2] = 0x2000;
	  break;

	case SS_NEGATIVE:
	  if (wRWhiteLevel > 0)
	    pWhiteShading[i * 3] = (unsigned short)
	    			   (65536.0 / wRWhiteLevel * 0x1000);
	  else
	    pWhiteShading[i * 3] = 0x1000;

	  if (wGWhiteLevel > 0)
	    pWhiteShading[i * 3 + 1] = (unsigned short)
	    			       ((65536 * 1.5) / wGWhiteLevel * 0x1000);
	  else
	    pWhiteShading[i * 3 + 1] = 0x1000;

	  if (wBWhiteLevel > 0)
	    pWhiteShading[i * 3 + 2] = (unsigned short)
	    			       ((65536 * 2.0) / wBWhiteLevel * 0x1000);
	  else
	    pWhiteShading[i * 3 + 2] = 0x1000;
	  break;

	case SS_POSITIVE:
	  if (wRWhiteLevel > 0)
	    pWhiteShading[i * 3] = (unsigned short)
	    			   (65536.0 / wRWhiteLevel * 0x1000);
	  else
	    pWhiteShading[i * 3] = 0x1000;

	  if (wGWhiteLevel > 0)
	    pWhiteShading[i * 3 + 1] = (unsigned short)
	    			       ((65536 * 1.04) / wGWhiteLevel *0x1000);
	  else
	    pWhiteShading[i * 3 + 1] = 0x1000;

	  if (wBWhiteLevel > 0)
	    pWhiteShading[i * 3 + 2] = (unsigned short)
	    			       (65536.0 / wBWhiteLevel * 0x1000);
	  else
	    pWhiteShading[i * 3 + 2] = 0x1000;
	  break;
	}
    }

  free (pRWhiteSort);
  free (pGWhiteSort);
  free (pBWhiteSort);
  free (pRDarkSort);
  free (pGDarkSort);
  free (pBDarkSort);

  if (Asic_SetShadingTable (&g_chip, pWhiteShading, pDarkShading,
			    g_Target.wXDpi, wCalWidth) != SANE_STATUS_GOOD)
    goto error;  /* TODO: memory leak */

  free (pWhiteShading);
  free (pDarkShading);

  free (pWhiteData);
  free (pDarkData);

  DBG_LEAVE();
  return SANE_TRUE;

error:
  free (pWhiteData);
  free (pDarkData);
  return SANE_FALSE;
}

static SANE_Bool
CreateGammaTable (COLORMODE cmColorMode, unsigned short ** pGammaTable)
{
  unsigned short * pTable = NULL;
  DBG_ENTER();

  if ((cmColorMode == CM_GRAY8) || (cmColorMode == CM_RGB24))
    {
      unsigned short i;
      SANE_Byte bGammaData;

      pTable = malloc (sizeof (unsigned short) * 4096 * 3);
      if (!pTable)
	{
	  DBG (DBG_ERR, "pTable == NULL\n");
	  return SANE_FALSE;
	}

      for (i = 0; i < 4096; i++)
	{
	  bGammaData = (SANE_Byte) (pow ((double) i / 4095, 0.625) * 255);
	  pTable[i] = bGammaData;
	  pTable[i + 4096] = bGammaData;
	  pTable[i + 8192] = bGammaData;
	}
    }
  else if ((cmColorMode == CM_GRAY16) || (cmColorMode == CM_RGB48))
    {
      unsigned int i;
      unsigned short wGammaData;

      pTable = malloc (sizeof (unsigned short) * 65536 * 3);
      if (!pTable)
	{
	  DBG (DBG_ERR, "pTable == NULL\n");
	  return SANE_FALSE;
	}

      for (i = 0; i < 65536; i++)
	{
	  wGammaData = (unsigned short)
	    (pow ((double) i / 65535, 0.625) * 65535);
	  pTable[i] = wGammaData;
	  pTable[i + 65536] = wGammaData;
	  pTable[i + 65536 * 2] = wGammaData;
	}
    }
  else
    {
      DBG (DBG_INFO, "no gamma table for this color mode\n");
    }

  *pGammaTable = pTable;

  DBG_LEAVE();
  return SANE_TRUE;
}

SANE_Bool
Scanner_SetupScan (TARGETIMAGE * pTarget)
{
  unsigned short finalY;
  SANE_Byte bScanBits;
  DBG_ENTER();

  if (g_bOpened || !g_bPrepared)
    {
      DBG (DBG_FUNC, "invalid state\n");
      return SANE_FALSE;
    }

  g_Target = *pTarget;
  g_SWWidth = g_Target.wWidth;
  g_SWHeight = g_Target.wHeight;
  /* set real scan width according to ASIC limit: width must be 8x */
  g_Target.wWidth = (g_Target.wWidth + 15) & ~15;

  if (!CreateGammaTable (g_Target.cmColorMode, &g_pGammaTable))
    return SANE_FALSE;

  switch (g_Target.wYDpi)
    {
    case 1200:
      g_wPixelDistance = 4;	/* even & odd sensor problem */
      g_wLineDistance = 24;
      g_Target.wHeight += g_wPixelDistance;
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

  DBG (DBG_FUNC, "wYDpi=%d\n", g_Target.wYDpi);
  DBG (DBG_FUNC, "g_wLineDistance=%d\n", g_wLineDistance);
  DBG (DBG_FUNC, "g_wPixelDistance=%d\n", g_wPixelDistance);

  switch (g_Target.cmColorMode)
    {
    case CM_RGB48:
      g_BytesPerRow = 6 * g_Target.wWidth;
      g_SWBytesPerRow = 6 * g_SWWidth;
      g_Target.wHeight += g_wLineDistance * 2;
      bScanBits = 48;
      break;
    case CM_RGB24:
      g_BytesPerRow = 3 * g_Target.wWidth;
      g_SWBytesPerRow = 3 * g_SWWidth;
      g_Target.wHeight += g_wLineDistance * 2;
      bScanBits = 24;
      break;
    case CM_GRAY16:
      g_BytesPerRow = 2 * g_Target.wWidth;
      g_SWBytesPerRow = 2 * g_SWWidth;
      bScanBits = 16;
      break;
    case CM_GRAY8:
    case CM_TEXT:
    default:
      g_BytesPerRow = g_Target.wWidth;
      g_SWBytesPerRow = g_SWWidth;
      bScanBits = 8;
      break;
    }

  if (g_Target.ssScanSource == SS_REFLECTIVE)
    {
      if (!Scanner_PowerControl (SANE_TRUE, SANE_FALSE))
	return SANE_FALSE;
    }
  else
    {
      if (!Scanner_PowerControl (SANE_FALSE, SANE_TRUE))
	return SANE_FALSE;
    }

  if (Asic_Open (&g_chip) != SANE_STATUS_GOOD)
    return SANE_FALSE;

  g_bOpened = SANE_TRUE;

  /* find left & top side */
  if (g_Target.ssScanSource != SS_REFLECTIVE)
    if (Asic_MotorMove (&g_chip, SANE_TRUE, TRAN_START_POS) != SANE_STATUS_GOOD)
      return SANE_FALSE;

  if (g_Target.wXDpi == SENSOR_DPI)
    {
      g_Target.wXDpi = SENSOR_DPI / 2;
      if (!AdjustAD ())
	return SANE_FALSE;
      if (!FindTopLeft (&g_Target.wX, &g_Target.wY))
	{
	  g_Target.wX = 187;
	  g_Target.wY = 43;
	}

      g_Target.wXDpi = SENSOR_DPI;
      if (!AdjustAD ())
	return SANE_FALSE;

      g_Target.wX = g_Target.wX * SENSOR_DPI /
		    FIND_LEFT_TOP_CALIBRATE_RESOLUTION + pTarget->wX;
      if (g_Target.ssScanSource == SS_REFLECTIVE)
	{
	  g_Target.wX += 47;
	  g_Target.wY = g_Target.wY * SENSOR_DPI /
			FIND_LEFT_TOP_CALIBRATE_RESOLUTION +
			pTarget->wY * SENSOR_DPI / g_Target.wYDpi + 47;
	}
    }
  else
    {
      if (!AdjustAD ())
	return SANE_FALSE;
      if (!FindTopLeft (&g_Target.wX, &g_Target.wY))
	{
	  g_Target.wX = 187;
	  g_Target.wY = 43;
	}

      g_Target.wX += pTarget->wX * (SENSOR_DPI / 2) / g_Target.wXDpi;
      if (g_Target.ssScanSource == SS_REFLECTIVE)
	{
	  if (g_Target.wXDpi != 75)
	    g_Target.wX += 23;
	  g_Target.wY = g_Target.wY * SENSOR_DPI /
			FIND_LEFT_TOP_CALIBRATE_RESOLUTION +
			pTarget->wY * SENSOR_DPI / g_Target.wYDpi + 47;
	}
      else
	{
	  if (g_Target.wXDpi == 75)
	    g_Target.wX -= 23;
	}
    }

  DBG (DBG_FUNC, "before LineCalibration16Bits wX=%d,wY=%d\n",
       g_Target.wX, g_Target.wY);

  if (!LineCalibration16Bits ())
    return SANE_FALSE;

  DBG (DBG_FUNC, "after LineCalibration16Bits wX=%d,wY=%d\n",
       g_Target.wX, g_Target.wY);

  DBG (DBG_FUNC, "bScanBits=%d, wXDpi=%d, wYDpi=%d, " \
		 "wX=%d, wY=%d, wWidth=%d, wHeight=%d\n",
       bScanBits, g_Target.wXDpi, g_Target.wYDpi, g_Target.wX, g_Target.wY,
       g_Target.wWidth, g_Target.wHeight);

  if (g_Target.ssScanSource == SS_REFLECTIVE)
    {
      finalY = 300;
    }
  else
    {
      g_Target.wY = pTarget->wY * SENSOR_DPI / g_Target.wYDpi + (300 - 40) +189;
      finalY = 360;
    }

  if (g_Target.wY > finalY)
    {
      if (Asic_MotorMove (&g_chip, SANE_TRUE, g_Target.wY - finalY) !=
	  SANE_STATUS_GOOD)
	return SANE_FALSE;
    }
  else
    {
      if (Asic_MotorMove (&g_chip, SANE_FALSE, finalY - g_Target.wY) !=
	  SANE_STATUS_GOOD)
	return SANE_FALSE;
    }
  g_Target.wY = finalY;

  if (Asic_SetWindow (&g_chip, g_Target.ssScanSource, SCAN_TYPE_NORMAL,
		      bScanBits, g_Target.wXDpi, g_Target.wYDpi, g_Target.wX,
		      g_Target.wY, g_Target.wWidth, g_Target.wHeight) !=
      SANE_STATUS_GOOD)
    return SANE_FALSE;

  DBG_LEAVE();
  return PrepareScan ();
}
