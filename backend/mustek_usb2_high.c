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

#define BACKEND_NAME mustek_usb2
#define DEBUG_DECLARE_ONLY
#include "sane/config.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <pthread.h>	/* TODO: use sanei_thread functions instead, possibly require USE_PHTREAD */

#include "byteorder.h"
#include "sane/sane.h"
#include "sane/sanei_backend.h"
#include "sane/sanei_debug.h"

#include "mustek_usb2_high.h"

#ifdef DEBUG_SAVE_IMAGE
#include <stdio.h>
#endif


const Scanner_ModelParams paramsMustekBP2448TAPro = {
  &asicMustekBP2448TAPro,

  0,
  4550
};

const Scanner_ModelParams paramsMicrotek4800H48U = {
  &asicMicrotek4800H48U,

  719,
  0  /* TODO */
};


void
Scanner_Init (Scanner_State * st, const Scanner_ModelParams * params)
{
  DBG_ENTER ();

  st->params = params;
  Asic_Initialize (&st->chip, st->params->asic_params);

  st->bOpened = SANE_FALSE;
  st->bPrepared = SANE_FALSE;
  st->isCanceled = SANE_FALSE;
  st->bIsFirstReadBefData = SANE_TRUE;

  st->pReadImageHead = NULL;
  st->pGammaTable = NULL;

  pthread_mutex_init (&st->scannedLinesMutex, NULL);
  pthread_mutex_init (&st->readyLinesMutex, NULL);

  DBG_LEAVE ();
}

SANE_Status
Scanner_PowerControl (Scanner_State * st, SANE_Bool isLampOn,
		      SANE_Bool isTALampOn)
{
  SANE_Status status;
  SANE_Bool hasTA;
  DBG_ENTER ();

  status = Asic_Open (&st->chip);
  if (status != SANE_STATUS_GOOD)
    return status;

  status = Asic_TurnLamp (&st->chip, isLampOn);
  if (status != SANE_STATUS_GOOD)
    goto error;

  status = Asic_IsTAConnected (&st->chip, &hasTA);
  if (status != SANE_STATUS_GOOD)
    goto error;

  if (hasTA)
    {
      status = Asic_TurnTA (&st->chip, isTALampOn);
      if (status != SANE_STATUS_GOOD)
	goto error;
    }

  status = Asic_Close (&st->chip);

  DBG_LEAVE ();
  return status;

error:
  Asic_Close (&st->chip);
  return status;
}

SANE_Status
Scanner_BackHome (Scanner_State * st)
{
  SANE_Status status;
  DBG_ENTER ();

  status = Asic_Open (&st->chip);
  if (status != SANE_STATUS_GOOD)
    return status;

  status = Asic_CarriageHome (&st->chip);
  if (status != SANE_STATUS_GOOD)
    {
      Asic_Close (&st->chip);
      return status;
    }

  status = Asic_Close (&st->chip);

  DBG_LEAVE ();
  return status;
}

SANE_Status
Scanner_IsTAConnected (Scanner_State * st, SANE_Bool * pHasTA)
{
  SANE_Status status;
  DBG_ENTER ();

  status = Asic_Open (&st->chip);
  if (status != SANE_STATUS_GOOD)
    return status;

  status = Asic_IsTAConnected (&st->chip, pHasTA);
  if (status != SANE_STATUS_GOOD)
    {
      Asic_Close (&st->chip);
      return status;
    }

  status = Asic_Close (&st->chip);

  DBG_LEAVE ();
  return status;
}

SANE_Status
Scanner_GetKeyStatus (Scanner_State * st, SANE_Byte * pKey)
{
  SANE_Status status;
  DBG_ENTER ();

  status = Asic_Open (&st->chip);
  if (status != SANE_STATUS_GOOD)
    return status;

  status = Asic_CheckFunctionKey (&st->chip, pKey);
  if (status != SANE_STATUS_GOOD)
    {
      Asic_Close (&st->chip);
      return status;
    }

  status = Asic_Close (&st->chip);

  DBG_LEAVE ();
  return status;
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
SetPixel48Bit (unsigned short * pLine, unsigned short * pGammaTable,
	       unsigned short wRTempData, unsigned short wGTempData,
	       unsigned short wBTempData, SANE_Bool isOrderInvert)
{
  if (!isOrderInvert)
    {
      pLine[0] = pGammaTable[wRTempData];
      pLine[1] = pGammaTable[wGTempData + 0x10000];
      pLine[2] = pGammaTable[wBTempData + 0x20000];
    }
  else
    {
      pLine[2] = pGammaTable[wRTempData];
      pLine[1] = pGammaTable[wGTempData + 0x10000];
      pLine[0] = pGammaTable[wBTempData + 0x20000];
    }
}

static void
GetRgb48BitLineHalf (Scanner_State * st, SANE_Byte * pLine,
		     SANE_Bool isOrderInvert)
{
  unsigned int dwRLinePos, dwGLinePos, dwBLinePos;
  unsigned short wRTempData, wGTempData, wBTempData;
  unsigned short i;

  dwRLinePos = st->wtheReadyLines;
  dwGLinePos = st->wtheReadyLines - st->wLineDistance;
  dwBLinePos = st->wtheReadyLines - st->wLineDistance * 2;
  dwRLinePos = (dwRLinePos % st->wMaxScanLines) * st->BytesPerRow;
  dwGLinePos = (dwGLinePos % st->wMaxScanLines) * st->BytesPerRow;
  dwBLinePos = (dwBLinePos % st->wMaxScanLines) * st->BytesPerRow;

  for (i = 0; i < st->SWWidth; i++)
    {
      wRTempData = st->pReadImageHead[dwRLinePos + i * 6] |
	(st->pReadImageHead[dwRLinePos + i * 6 + 1] << 8);
      wGTempData = st->pReadImageHead[dwGLinePos + i * 6 + 2] |
	(st->pReadImageHead[dwGLinePos + i * 6 + 3] << 8);
      wBTempData = st->pReadImageHead[dwBLinePos + i * 6 + 4] |
	(st->pReadImageHead[dwBLinePos + i * 6 + 5] << 8);

      LE16TOH (wRTempData);
      LE16TOH (wGTempData);
      LE16TOH (wBTempData);

      SetPixel48Bit ((unsigned short *) (pLine + (i * 6)), st->pGammaTable,
		     wRTempData, wGTempData, wBTempData, isOrderInvert);
    }
}

static void
GetRgb48BitLineFull (Scanner_State * st, SANE_Byte * pLine,
		     SANE_Bool isOrderInvert)
{
  unsigned int dwRLinePosOdd, dwGLinePosOdd, dwBLinePosOdd;
  unsigned int dwRLinePosEven, dwGLinePosEven, dwBLinePosEven;
  unsigned int dwRTempData, dwGTempData, dwBTempData;
  unsigned short i = 0;

  if (st->Target.ssScanSource == SS_REFLECTIVE)
    {
      dwRLinePosOdd = st->wtheReadyLines - st->wPixelDistance;
      dwGLinePosOdd = st->wtheReadyLines - st->wLineDistance -
		      st->wPixelDistance;
      dwBLinePosOdd = st->wtheReadyLines - st->wLineDistance * 2 -
		      st->wPixelDistance;
      dwRLinePosEven = st->wtheReadyLines;
      dwGLinePosEven = st->wtheReadyLines - st->wLineDistance;
      dwBLinePosEven = st->wtheReadyLines - st->wLineDistance * 2;
    }
  else
    {
      dwRLinePosOdd = st->wtheReadyLines;
      dwGLinePosOdd = st->wtheReadyLines - st->wLineDistance;
      dwBLinePosOdd = st->wtheReadyLines - st->wLineDistance * 2;
      dwRLinePosEven = st->wtheReadyLines - st->wPixelDistance;
      dwGLinePosEven = st->wtheReadyLines - st->wLineDistance -
		       st->wPixelDistance;
      dwBLinePosEven = st->wtheReadyLines - st->wLineDistance * 2 -
		       st->wPixelDistance;
    }
  dwRLinePosOdd = (dwRLinePosOdd % st->wMaxScanLines) * st->BytesPerRow;
  dwGLinePosOdd = (dwGLinePosOdd % st->wMaxScanLines) * st->BytesPerRow;
  dwBLinePosOdd = (dwBLinePosOdd % st->wMaxScanLines) * st->BytesPerRow;
  dwRLinePosEven = (dwRLinePosEven % st->wMaxScanLines) * st->BytesPerRow;
  dwGLinePosEven = (dwGLinePosEven % st->wMaxScanLines) * st->BytesPerRow;
  dwBLinePosEven = (dwBLinePosEven % st->wMaxScanLines) * st->BytesPerRow;

  while (i < st->SWWidth)
    {
      if ((i + 1) >= st->SWWidth)
	break;

      dwRTempData = st->pReadImageHead[dwRLinePosOdd + i * 6] |
	(st->pReadImageHead[dwRLinePosOdd + i * 6 + 1] << 8);
      dwRTempData += st->pReadImageHead[dwRLinePosEven + (i + 1) * 6] |
	(st->pReadImageHead[dwRLinePosEven + (i + 1) * 6 + 1] << 8);
      dwRTempData /= 2;

      dwGTempData = st->pReadImageHead[dwGLinePosOdd + i * 6 + 2] |
	(st->pReadImageHead[dwGLinePosOdd + i * 6 + 3] << 8);
      dwGTempData += st->pReadImageHead[dwGLinePosEven + (i + 1) * 6 + 2] |
	(st->pReadImageHead[dwGLinePosEven + (i + 1) * 6 + 3] << 8);
      dwGTempData /= 2;

      dwBTempData = st->pReadImageHead[dwBLinePosOdd + i * 6 + 4] |
	(st->pReadImageHead[dwBLinePosOdd + i * 6 + 5] << 8);
      dwBTempData += st->pReadImageHead[dwBLinePosEven + (i + 1) * 6 + 4] |
	(st->pReadImageHead[dwBLinePosEven + (i + 1) * 6 + 5] << 8);
      dwBTempData /= 2;

      LE16TOH (dwRTempData);
      LE16TOH (dwGTempData);
      LE16TOH (dwBTempData);

      SetPixel48Bit ((unsigned short *) (pLine + (i * 6)), st->pGammaTable,
		     dwRTempData, dwGTempData, dwBTempData, isOrderInvert);
      i++;

      dwRTempData = st->pReadImageHead[dwRLinePosEven + i * 6] |
	(st->pReadImageHead[dwRLinePosEven + i * 6 + 1] << 8);
      dwRTempData += st->pReadImageHead[dwRLinePosOdd + (i + 1) * 6] |
	(st->pReadImageHead[dwRLinePosOdd + (i + 1) * 6 + 1] << 8);
      dwRTempData /= 2;

      dwGTempData = st->pReadImageHead[dwGLinePosEven + i * 6 + 2] |
	(st->pReadImageHead[dwGLinePosEven + i * 6 + 3] << 8);
      dwGTempData += st->pReadImageHead[dwGLinePosOdd + (i + 1) * 6 + 2] |
	(st->pReadImageHead[dwGLinePosOdd + (i + 1) * 6 + 3] << 8);
      dwGTempData /= 2;

      dwBTempData = st->pReadImageHead[dwBLinePosEven + i * 6 + 4] |
	(st->pReadImageHead[dwBLinePosEven + i * 6 + 5] << 8);
      dwBTempData += st->pReadImageHead[dwBLinePosOdd + (i + 1) * 6 + 4] |
	(st->pReadImageHead[dwBLinePosOdd + (i + 1) * 6 + 5] << 8);
      dwBTempData /= 2;

      LE16TOH (dwRTempData);
      LE16TOH (dwGTempData);
      LE16TOH (dwBTempData);

      SetPixel48Bit ((unsigned short *) (pLine + (i * 6)), st->pGammaTable,
		     dwRTempData, dwGTempData, dwBTempData, isOrderInvert);
      i++;
    }
}

static void
SetPixel24Bit (SANE_Byte * pLine, unsigned short * pGammaTable,
	       unsigned short tempR, unsigned short tempG, unsigned short tempB,
	       SANE_Bool isOrderInvert)
{
  if (!isOrderInvert)
    {
      pLine[0] = (SANE_Byte) pGammaTable[tempR];
      pLine[1] = (SANE_Byte) pGammaTable[4096 + tempG];
      pLine[2] = (SANE_Byte) pGammaTable[8192 + tempB];
    }
  else
    {
      pLine[2] = (SANE_Byte) pGammaTable[tempR];
      pLine[1] = (SANE_Byte) pGammaTable[4096 + tempG];
      pLine[0] = (SANE_Byte) pGammaTable[8192 + tempB];
    }
}

static void
GetRgb24BitLineHalf (Scanner_State * st, SANE_Byte * pLine,
		     SANE_Bool isOrderInvert)
{
  unsigned int dwRLinePos, dwGLinePos, dwBLinePos;
  unsigned short wRed, wGreen, wBlue;
  unsigned short tempR, tempG, tempB;
  unsigned short i;

  dwRLinePos = st->wtheReadyLines;
  dwGLinePos = st->wtheReadyLines - st->wLineDistance;
  dwBLinePos = st->wtheReadyLines - st->wLineDistance * 2;
  dwRLinePos = (dwRLinePos % st->wMaxScanLines) * st->BytesPerRow;
  dwGLinePos = (dwGLinePos % st->wMaxScanLines) * st->BytesPerRow;
  dwBLinePos = (dwBLinePos % st->wMaxScanLines) * st->BytesPerRow;  

  for (i = 0; i < st->SWWidth; i++)
    {
      wRed = st->pReadImageHead[dwRLinePos + i * 3];
      wRed += st->pReadImageHead[dwRLinePos + (i + 1) * 3];
      wRed /= 2;

      wGreen = st->pReadImageHead[dwGLinePos + i * 3 + 1];
      wGreen += st->pReadImageHead[dwGLinePos + (i + 1) * 3 + 1];
      wGreen /= 2;

      wBlue = st->pReadImageHead[dwBLinePos + i * 3 + 2];
      wBlue += st->pReadImageHead[dwBLinePos + (i + 1) * 3 + 2];
      wBlue /= 2;

      tempR = (wRed << 4) | QBET4 (wBlue, wGreen);
      tempG = (wGreen << 4) | QBET4 (wRed, wBlue);
      tempB = (wBlue << 4) | QBET4 (wGreen, wRed);

      SetPixel24Bit (pLine + (i * 3), st->pGammaTable, tempR, tempG, tempB,
		     isOrderInvert);
    }
}

static void
GetRgb24BitLineFull (Scanner_State * st, SANE_Byte * pLine,
		     SANE_Bool isOrderInvert)
{
  unsigned int dwRLinePosOdd, dwGLinePosOdd, dwBLinePosOdd;
  unsigned int dwRLinePosEven, dwGLinePosEven, dwBLinePosEven;
  unsigned short wRed, wGreen, wBlue;
  unsigned short tempR, tempG, tempB;
  unsigned short i = 0;

  if (st->Target.ssScanSource == SS_REFLECTIVE)
    {
      dwRLinePosOdd = st->wtheReadyLines - st->wPixelDistance;
      dwGLinePosOdd = st->wtheReadyLines - st->wLineDistance -
		      st->wPixelDistance;
      dwBLinePosOdd = st->wtheReadyLines - st->wLineDistance * 2 -
		      st->wPixelDistance;
      dwRLinePosEven = st->wtheReadyLines;
      dwGLinePosEven = st->wtheReadyLines - st->wLineDistance;
      dwBLinePosEven = st->wtheReadyLines - st->wLineDistance * 2;
    }
  else
    {
      dwRLinePosOdd = st->wtheReadyLines;
      dwGLinePosOdd = st->wtheReadyLines - st->wLineDistance;
      dwBLinePosOdd = st->wtheReadyLines - st->wLineDistance * 2;
      dwRLinePosEven = st->wtheReadyLines - st->wPixelDistance;
      dwGLinePosEven = st->wtheReadyLines - st->wLineDistance -
		       st->wPixelDistance;
      dwBLinePosEven = st->wtheReadyLines - st->wLineDistance * 2 -
		       st->wPixelDistance;
    }
  dwRLinePosOdd = (dwRLinePosOdd % st->wMaxScanLines) * st->BytesPerRow;
  dwGLinePosOdd = (dwGLinePosOdd % st->wMaxScanLines) * st->BytesPerRow;
  dwBLinePosOdd = (dwBLinePosOdd % st->wMaxScanLines) * st->BytesPerRow;
  dwRLinePosEven = (dwRLinePosEven % st->wMaxScanLines) * st->BytesPerRow;
  dwGLinePosEven = (dwGLinePosEven % st->wMaxScanLines) * st->BytesPerRow;
  dwBLinePosEven = (dwBLinePosEven % st->wMaxScanLines) * st->BytesPerRow;

  while (i < st->SWWidth)
    {
      if ((i + 1) >= st->SWWidth)
	break;

      wRed = st->pReadImageHead[dwRLinePosOdd + i * 3];
      wRed += st->pReadImageHead[dwRLinePosEven + (i + 1) * 3];
      wRed /= 2;

      wGreen = st->pReadImageHead[dwGLinePosOdd + i * 3 + 1];
      wGreen += st->pReadImageHead[dwGLinePosEven + (i + 1) * 3 + 1];
      wGreen /= 2;

      wBlue = st->pReadImageHead[dwBLinePosOdd + i * 3 + 2];
      wBlue += st->pReadImageHead[dwBLinePosEven + (i + 1) * 3 + 2];
      wBlue /= 2;

      tempR = (wRed << 4) | QBET4 (wBlue, wGreen);
      tempG = (wGreen << 4) | QBET4 (wRed, wBlue);
      tempB = (wBlue << 4) | QBET4 (wGreen, wRed);

      SetPixel24Bit (pLine + (i * 3), st->pGammaTable, tempR, tempG, tempB,
		     isOrderInvert);
      i++;

      wRed = st->pReadImageHead[dwRLinePosEven + i * 3];
      wRed += st->pReadImageHead[dwRLinePosOdd + (i + 1) * 3];
      wRed /= 2;

      wGreen = st->pReadImageHead[dwGLinePosEven + i * 3 + 1];
      wGreen += st->pReadImageHead[dwGLinePosOdd + (i + 1) * 3 + 1];
      wGreen /= 2;

      wBlue = st->pReadImageHead[dwBLinePosEven + i * 3 + 2];
      wBlue += st->pReadImageHead[dwBLinePosOdd + (i + 1) * 3 + 2];
      wBlue /= 2;

      tempR = (wRed << 4) | QBET4 (wBlue, wGreen);
      tempG = (wGreen << 4) | QBET4 (wRed, wBlue);
      tempB = (wBlue << 4) | QBET4 (wGreen, wRed);

      SetPixel24Bit (pLine + (i * 3), st->pGammaTable, tempR, tempG, tempB,
		     isOrderInvert);
      i++;
    }
}

static void
GetMono16BitLineHalf (Scanner_State * st, SANE_Byte * pLine,
		      SANE_Bool __sane_unused__ isOrderInvert)
{
  unsigned int dwLinePos;
  unsigned short wTempData;
  unsigned short i;

  dwLinePos = (st->wtheReadyLines % st->wMaxScanLines) * st->BytesPerRow;

  for (i = 0; i < st->SWWidth; i++)
    {
      wTempData = st->pReadImageHead[dwLinePos + i * 2] |
		  (st->pReadImageHead[dwLinePos + i * 2 + 1] << 8);
      LE16TOH (wTempData);
      *((unsigned short *) (pLine + (i * 2))) = st->pGammaTable[wTempData];
    }
}

static void
GetMono16BitLineFull (Scanner_State * st, SANE_Byte * pLine,
		      SANE_Bool __sane_unused__ isOrderInvert)
{
  unsigned int dwLinePosOdd;
  unsigned int dwLinePosEven;
  unsigned int dwTempData;
  unsigned short i = 0;

  if (st->Target.ssScanSource == SS_REFLECTIVE)
    {
      dwLinePosOdd = st->wtheReadyLines - st->wPixelDistance;
      dwLinePosEven = st->wtheReadyLines;
    }
  else
    {
      dwLinePosOdd = st->wtheReadyLines;
      dwLinePosEven = st->wtheReadyLines - st->wPixelDistance;
    }
  dwLinePosOdd = (dwLinePosOdd % st->wMaxScanLines) * st->BytesPerRow;
  dwLinePosEven = (dwLinePosEven % st->wMaxScanLines) * st->BytesPerRow;

  while (i < st->SWWidth)
    {
      if ((i + 1) >= st->SWWidth)
	break;

      dwTempData = (unsigned int) st->pReadImageHead[dwLinePosOdd + i * 2];
      dwTempData += (unsigned int)
		    st->pReadImageHead[dwLinePosOdd + i * 2 + 1] << 8;
      dwTempData += (unsigned int)
		    st->pReadImageHead[dwLinePosEven + (i + 1) * 2];
      dwTempData += (unsigned int)
		    st->pReadImageHead[dwLinePosEven + (i + 1) * 2 + 1] << 8;
      dwTempData /= 2;
      LE16TOH (dwTempData);

      *((unsigned short *) (pLine + (i * 2))) = st->pGammaTable[dwTempData];
      i++;

      dwTempData = (unsigned int) st->pReadImageHead[dwLinePosEven + i * 2];
      dwTempData += (unsigned int)
		    st->pReadImageHead[dwLinePosEven + i * 2 + 1] << 8;
      dwTempData += (unsigned int)
		    st->pReadImageHead[dwLinePosOdd + (i + 1) * 2];
      dwTempData += (unsigned int)
		    st->pReadImageHead[dwLinePosOdd + (i + 1) * 2 + 1] << 8;
      dwTempData /= 2;
      LE16TOH (dwTempData);

      *((unsigned short *) (pLine + (i * 2))) = st->pGammaTable[dwTempData];
      i++;
    }
}

static void
GetMono8BitLineHalf (Scanner_State * st, SANE_Byte * pLine,
		     SANE_Bool __sane_unused__ isOrderInvert)
{
  unsigned int dwLinePos;
  unsigned int dwTempData;
  unsigned short i;

  dwLinePos = (st->wtheReadyLines % st->wMaxScanLines) * st->BytesPerRow;

  for (i = 0; i < st->SWWidth; i++)
    {
      dwTempData = (st->pReadImageHead[dwLinePos + i] << 4) | (rand () & 0x0f);
      pLine[i] = (SANE_Byte) st->pGammaTable[dwTempData];
    }
}

static void
GetMono8BitLineFull (Scanner_State * st, SANE_Byte * pLine,
		     SANE_Bool __sane_unused__ isOrderInvert)
{
  unsigned int dwLinePosOdd;
  unsigned int dwLinePosEven;
  unsigned short wGray;
  unsigned short i = 0;

  if (st->Target.ssScanSource == SS_REFLECTIVE)
    {
      dwLinePosOdd = (st->wtheReadyLines - st->wPixelDistance) %
		     st->wMaxScanLines;
      dwLinePosEven = (st->wtheReadyLines) % st->wMaxScanLines;
    }
  else
    {
      dwLinePosOdd = (st->wtheReadyLines) % st->wMaxScanLines;
      dwLinePosEven = (st->wtheReadyLines - st->wPixelDistance) %
		      st->wMaxScanLines;
    }
  dwLinePosOdd = (dwLinePosOdd % st->wMaxScanLines) * st->BytesPerRow;
  dwLinePosEven = (dwLinePosEven % st->wMaxScanLines) * st->BytesPerRow;

  while (i < st->SWWidth)
    {
      if ((i + 1) >= st->SWWidth)
	break;

      wGray = st->pReadImageHead[dwLinePosOdd + i];
      wGray += st->pReadImageHead[dwLinePosEven + i + 1];
      wGray /= 2;

      pLine[i] = (SANE_Byte) st->pGammaTable[(wGray << 4) | (rand () & 0x0f)];
      i++;

      wGray = st->pReadImageHead[dwLinePosEven + i];
      wGray += st->pReadImageHead[dwLinePosOdd + i + 1];
      wGray /= 2;

      pLine[i] = (SANE_Byte) st->pGammaTable[(wGray << 4) | (rand () & 0x0f)];
      i++;
    }
}

static void
GetMono1BitLineHalf (Scanner_State * st, SANE_Byte * pLine,
		     SANE_Bool __sane_unused__ isOrderInvert)
{
  unsigned int dwLinePos;
  unsigned short i;

  dwLinePos = (st->wtheReadyLines % st->wMaxScanLines) * st->BytesPerRow;

  for (i = 0; i < st->SWWidth; i++)
    {
      if (st->pReadImageHead[dwLinePos + i] <= st->Target.wLineartThreshold)
	pLine[i / 8] |= 0x80 >> (i % 8);
    }
}

static void
GetMono1BitLineFull (Scanner_State * st, SANE_Byte * pLine,
		     SANE_Bool __sane_unused__ isOrderInvert)
{
  unsigned int dwLinePosOdd;
  unsigned int dwLinePosEven;
  unsigned short i = 0;

  if (st->Target.ssScanSource == SS_REFLECTIVE)
    {
      dwLinePosOdd = (st->wtheReadyLines - st->wPixelDistance) %
		     st->wMaxScanLines;
      dwLinePosEven = (st->wtheReadyLines) % st->wMaxScanLines;
    }
  else
    {
      dwLinePosOdd = (st->wtheReadyLines) % st->wMaxScanLines;
      dwLinePosEven = (st->wtheReadyLines - st->wPixelDistance) %
		      st->wMaxScanLines;
    }
  dwLinePosOdd = (dwLinePosOdd % st->wMaxScanLines) * st->BytesPerRow;
  dwLinePosEven = (dwLinePosEven % st->wMaxScanLines) * st->BytesPerRow;

  while (i < st->SWWidth)
    {
      if ((i + 1) >= st->SWWidth)
	break;

      if (st->pReadImageHead[dwLinePosOdd + i] <= st->Target.wLineartThreshold)
	pLine[i / 8] |= 0x80 >> (i % 8);
      i++;

      if (st->pReadImageHead[dwLinePosEven + i] <= st->Target.wLineartThreshold)
	pLine[i / 8] |= 0x80 >> (i % 8);
      i++;
    }
}

static unsigned int
GetScannedLines (Scanner_State * st)
{
  unsigned int dwScannedLines;

  pthread_mutex_lock (&st->scannedLinesMutex);
  dwScannedLines = st->dwScannedTotalLines;
  pthread_mutex_unlock (&st->scannedLinesMutex);

  return dwScannedLines;
}

static unsigned int
GetReadyLines (Scanner_State * st)
{
  unsigned int dwReadyLines;

  pthread_mutex_lock (&st->readyLinesMutex);
  dwReadyLines = st->wtheReadyLines;
  pthread_mutex_unlock (&st->readyLinesMutex);

  return dwReadyLines;
}

static void
AddScannedLines (Scanner_State * st, unsigned short wAddLines)
{
  pthread_mutex_lock (&st->scannedLinesMutex);
  st->dwScannedTotalLines += wAddLines;
  pthread_mutex_unlock (&st->scannedLinesMutex);
}

static void
AddReadyLines (Scanner_State * st)
{
  pthread_mutex_lock (&st->readyLinesMutex);
  st->wtheReadyLines++;
  pthread_mutex_unlock (&st->readyLinesMutex);
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
ReadDataFromScanner (void * param)
{
  Scanner_State * st = param;
  unsigned short wTotalReadImageLines = 0;
  unsigned short wWantedLines = st->Target.wHeight;
  SANE_Byte * pReadImage = st->pReadImageHead;
  SANE_Bool isWaitImageLineDiff = SANE_FALSE;
  unsigned int wMaxScanLines = st->wMaxScanLines;
  unsigned short wReadImageLines = 0;
  unsigned short wScanLinesThisBlock;
  unsigned short wBufferLines = st->wLineDistance * 2 + st->wPixelDistance;
  DBG_ENTER ();
  DBG (DBG_FUNC, "ReadDataFromScanner: new thread\n");

  while (wTotalReadImageLines < wWantedLines)
    {
      if (!isWaitImageLineDiff)
	{
	  wScanLinesThisBlock =
	    (wWantedLines - wTotalReadImageLines) < st->wScanLinesPerBlock ?
	    (wWantedLines - wTotalReadImageLines) : st->wScanLinesPerBlock;

	  DBG (DBG_FUNC, "ReadDataFromScanner: wWantedLines=%d\n",
	       wWantedLines);
	  DBG (DBG_FUNC, "ReadDataFromScanner: wScanLinesThisBlock=%d\n",
	       wScanLinesThisBlock);

	  /* This call is thread-safe under the assumption that no function that
	     might run concurrently will cause a register bank switch. */
	  if (Asic_ReadImage (&st->chip, pReadImage, wScanLinesThisBlock) !=
	      SANE_STATUS_GOOD)
	    {
	      DBG (DBG_FUNC, "ReadDataFromScanner: thread exit\n");
	      return NULL;
	    }

	  /* has read in memory buffer */
	  wReadImageLines += wScanLinesThisBlock;
	  AddScannedLines (st, wScanLinesThisBlock);
	  wTotalReadImageLines += wScanLinesThisBlock;
	  pReadImage += wScanLinesThisBlock * st->BytesPerRow;

	  /* buffer is full */
	  if (wReadImageLines >= wMaxScanLines)
	    {
	      pReadImage = st->pReadImageHead;
	      wReadImageLines = 0;
	    }

	  if (st->dwScannedTotalLines >= (GetReadyLines (st) +
	      (wMaxScanLines - (wBufferLines + st->wScanLinesPerBlock))))
	    {
	      isWaitImageLineDiff = SANE_TRUE;
	    }
	}
      else if (st->dwScannedTotalLines <=
	       GetReadyLines (st) + wBufferLines + st->wScanLinesPerBlock)
	{
	  isWaitImageLineDiff = SANE_FALSE;
	}

      pthread_testcancel ();
    }

  DBG (DBG_FUNC, "ReadDataFromScanner: read OK, exit thread\n");
  DBG_LEAVE ();
  return NULL;
}

static SANE_Status
GetLine (Scanner_State * st, SANE_Byte * pLine, unsigned short * wLinesCount,
	 void (* pFunc) (Scanner_State *, SANE_Byte *, SANE_Bool),
	 SANE_Bool isOrderInvert, SANE_Bool fixEvenOdd)
{
  unsigned short wWantedTotalLines;
  unsigned short TotalXferLines = 0;
  SANE_Byte * pFirstLine = pLine;
  unsigned int i;
  DBG_ENTER ();

  wWantedTotalLines = *wLinesCount;

  if (st->bFirstReadImage)
    {
      pthread_create (&st->threadid_readimage, NULL, ReadDataFromScanner, st);
      DBG (DBG_FUNC, "thread started\n");
      st->bFirstReadImage = SANE_FALSE;
    }

  while (TotalXferLines < wWantedTotalLines)
    {
      if (st->dwTotalTotalXferLines >= st->SWHeight)
	{
	  pthread_cancel (st->threadid_readimage);
	  pthread_join (st->threadid_readimage, NULL);
	  DBG (DBG_FUNC, "thread finished\n");

	  *wLinesCount = TotalXferLines;
	  return SANE_STATUS_GOOD;
	}

      if (GetScannedLines (st) > st->wtheReadyLines)
	{
	  pFunc (st, pLine, isOrderInvert);

	  TotalXferLines++;
	  st->dwTotalTotalXferLines++;
	  pLine += st->SWBytesPerRow;
	  AddReadyLines (st);
	}

      if (st->isCanceled)
	{
	  pthread_join (st->threadid_readimage, NULL);
	  DBG (DBG_FUNC, "thread finished\n");
	  break;
	}
    }

  *wLinesCount = TotalXferLines;

  if (fixEvenOdd)
    {
      /* modify the last point */
      if (st->bIsFirstReadBefData)
	{
	  st->pBefLineImageData = malloc (st->SWBytesPerRow);
	  if (!st->pBefLineImageData)
	    return SANE_STATUS_NO_MEM;
	  memcpy (st->pBefLineImageData, pFirstLine, st->SWBytesPerRow);
	  st->bIsFirstReadBefData = SANE_FALSE;
	  st->dwAlreadyGetLines = 0;
	}

      ModifyLinePoint (pFirstLine, st->pBefLineImageData, st->SWBytesPerRow,
		       wWantedTotalLines, 1, 4);

      memcpy (st->pBefLineImageData,
	      pFirstLine + (wWantedTotalLines - 1) * st->SWBytesPerRow,
	      st->SWBytesPerRow);
      st->dwAlreadyGetLines += wWantedTotalLines;
      if (st->dwAlreadyGetLines >= st->SWHeight)
	{
	  DBG (DBG_FUNC, "freeing pBefLineImageData\n");
	  free (st->pBefLineImageData);
	  st->bIsFirstReadBefData = SANE_TRUE;
	}
    }

  if (st->Target.ssScanSource == SS_NEGATIVE)
    {
      for (i = 0; i < (*wLinesCount * st->SWBytesPerRow); i++)
	pFirstLine[i] ^= 0xff;
    }

  DBG_LEAVE ();
  return SANE_STATUS_GOOD;
}

SANE_Status
Scanner_GetRows (Scanner_State * st, SANE_Byte * pBlock,
		 unsigned short * pNumRows, SANE_Bool isOrderInvert)
{
  SANE_Status status;
  SANE_Bool fixEvenOdd = SANE_FALSE;
  void (* pFunc) (Scanner_State *, SANE_Byte *, SANE_Bool) = NULL;
  DBG_ENTER ();

  if (!st->bOpened || !st->bPrepared)
    {
      DBG (DBG_FUNC, "invalid state\n");
      return SANE_STATUS_CANCELLED;
    }

  switch (st->Target.cmColorMode)
    {
    case CM_RGB48:
      if (st->Target.wXDpi == SENSOR_DPI)
	pFunc = GetRgb48BitLineFull;
      else
	pFunc = GetRgb48BitLineHalf;
      break;

    case CM_RGB24:
      if (st->Target.wXDpi == SENSOR_DPI)
	pFunc = GetRgb24BitLineFull;
      else
	pFunc = GetRgb24BitLineHalf;
      break;

    case CM_GRAY16:
      if (st->Target.wXDpi == SENSOR_DPI)
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
      if (st->Target.wXDpi == SENSOR_DPI)
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
      memset (pBlock, 0, *pNumRows * st->SWBytesPerRow);
      if (st->Target.wXDpi == SENSOR_DPI)
	pFunc = GetMono1BitLineFull;
      else
	pFunc = GetMono1BitLineHalf;
      break;
    }

  status = GetLine (st, pBlock, pNumRows, pFunc, isOrderInvert, fixEvenOdd);

  DBG_LEAVE ();
  return status;
}

SANE_Status
Scanner_StopScan (Scanner_State * st)
{
  SANE_Status status;
  DBG_ENTER ();

  if (!st->bOpened || !st->bPrepared)
    {
      DBG (DBG_FUNC, "invalid state\n");
      return SANE_STATUS_INVAL;
    }

  st->isCanceled = SANE_TRUE;

  pthread_cancel (st->threadid_readimage);
  pthread_join (st->threadid_readimage, NULL);
  DBG (DBG_FUNC, "thread finished\n");

  status = Asic_ScanStop (&st->chip);
  Asic_Close (&st->chip);

  st->bOpened = SANE_FALSE;

  free (st->pGammaTable);
  st->pGammaTable = NULL;
  free (st->pReadImageHead);
  st->pReadImageHead = NULL;

  DBG_LEAVE ();
  return status;
}

static SANE_Status
PrepareScan (Scanner_State * st)
{
  SANE_Status status;
  DBG_ENTER ();

  st->isCanceled = SANE_FALSE;

  st->wScanLinesPerBlock = BLOCK_SIZE / st->BytesPerRow;
  st->wMaxScanLines = ((IMAGE_BUFFER_SIZE / st->BytesPerRow) /
		       st->wScanLinesPerBlock) * st->wScanLinesPerBlock;
  st->dwScannedTotalLines = 0;

  switch (st->Target.cmColorMode)
    {
    case CM_RGB48:
    case CM_RGB24:
      st->wtheReadyLines = st->wLineDistance * 2 + st->wPixelDistance;
      break;
    case CM_GRAY16:
    case CM_GRAY8:
    case CM_TEXT:
      st->wtheReadyLines = st->wPixelDistance;
      break;
    }
  DBG (DBG_FUNC, "st->wtheReadyLines=%d\n", st->wtheReadyLines);

  DBG (DBG_FUNC, "allocate %d bytes for st->pReadImageHead\n",
       IMAGE_BUFFER_SIZE);
  st->pReadImageHead = malloc (IMAGE_BUFFER_SIZE);
  if (!st->pReadImageHead)
    {
      DBG (DBG_FUNC, "st->pReadImageHead == NULL\n");
      return SANE_STATUS_NO_MEM;
    }

  status = Asic_ScanStart (&st->chip);
  if (status != SANE_STATUS_GOOD)
    free (st->pReadImageHead);

  DBG_LEAVE ();
  return status;
}

SANE_Status
Scanner_Reset (Scanner_State * st)
{
  DBG_ENTER ();

  if (st->bOpened)
    {
      DBG (DBG_FUNC, "scanner already open\n");
      return SANE_STATUS_INVAL;
    }

  st->dwTotalTotalXferLines = 0;
  st->bFirstReadImage = SANE_TRUE;
  st->bPrepared = SANE_TRUE;

  DBG_LEAVE ();
  return SANE_STATUS_GOOD;
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

static SANE_Status
CalculateMaxMin (CALIBRATIONPARAM * pCalParam, SANE_Byte * pBuffer,
		 unsigned short * pMaxValue, unsigned short * pMinValue)
{
  unsigned short *wSecData, *wDarkSecData;
  int i, j;

  wSecData = malloc (pCalParam->nSecNum * sizeof (unsigned short));
  if (!wSecData)
    return SANE_STATUS_NO_MEM;
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
    return SANE_STATUS_NO_MEM;
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

  return SANE_STATUS_GOOD;
}

static SANE_Status
AdjustAD (Scanner_State * st)
{
  SANE_Status status;
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
  DBG_ENTER ();

  for (i = 0; i < 3; i++)
    {
      st->chip.AD.Direction[i] = DIR_POSITIVE;
      st->chip.AD.Gain[i] = 0;
    }
  if (st->Target.ssScanSource == SS_REFLECTIVE)
    {
      st->chip.AD.Offset[0] = 152;	/* red */
      st->chip.AD.Offset[1] = 56;	/* green */
      st->chip.AD.Offset[2] = 8;	/* blue */
    }
  else
    {
      st->chip.AD.Offset[0] = 159;
      st->chip.AD.Offset[1] = 50;
      st->chip.AD.Offset[2] = 45;
    }

  if (st->Target.wXDpi <= (SENSOR_DPI / 2))
    wAdjustADResolution = SENSOR_DPI / 2;
  else
    wAdjustADResolution = SENSOR_DPI;

  wCalWidth = 10240;
  pCalData = malloc (wCalWidth * 3);
  if (!pCalData)
    {
      DBG (DBG_FUNC, "pCalData == NULL\n");
      return SANE_STATUS_NO_MEM;
    }

  status = Asic_SetWindow (&st->chip, st->Target.ssScanSource,
			   SCAN_TYPE_CALIBRATE_DARK, 24,
			   wAdjustADResolution, wAdjustADResolution, 0, 0,
			   wCalWidth, 1);
  if (status != SANE_STATUS_GOOD)
    goto out;

  PrepareCalculateMaxMin (wAdjustADResolution, &calParam);

  for (i = 0; i < 10; i++)
    {
      DBG (DBG_FUNC, "first AD offset adjustment loop\n");
      status = Asic_ScanStart (&st->chip);
      if (status != SANE_STATUS_GOOD)
	goto out;
      status = Asic_ReadCalibrationData (&st->chip, pCalData, wCalWidth * 3,
					 SANE_TRUE);
      if (status != SANE_STATUS_GOOD)
	goto out;
      status = Asic_ScanStop (&st->chip);
      if (status != SANE_STATUS_GOOD)
	goto out;

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
	  status = CalculateMaxMin (&calParam, pCalData + (wCalWidth * j),
				    &wMaxValue[j], &wMinValue[j]);
	  if (status != SANE_STATUS_GOOD)
	    goto out;

	  if (st->chip.AD.Direction[j] == DIR_POSITIVE)
	    {
	      if (wMinValue[j] > 15)
		{
		  if (st->chip.AD.Offset[j] < 8)
		    st->chip.AD.Direction[j] = DIR_NEGATIVE;
		  else
		    st->chip.AD.Offset[j] -= 8;
		}
	      else if (wMinValue[j] < 5)
		st->chip.AD.Offset[j] += 8;
	    }
	  else
	    {
	      if (wMinValue[j] > 15)
		st->chip.AD.Offset[j] += 8;
	      else if (wMinValue[j] < 5)
		st->chip.AD.Offset[j] -= 8;
	    }
	}

      SetAFEGainOffset (&st->chip);

      if (!(wMinValue[0] > 15 || wMinValue[0] < 5 ||
	    wMinValue[1] > 15 || wMinValue[1] < 5 ||
	    wMinValue[2] > 15 || wMinValue[2] < 5))
	break;
    }

  DBG (DBG_FUNC, "OffsetR=%d,OffsetG=%d,OffsetB=%d\n",
       st->chip.AD.Offset[0], st->chip.AD.Offset[1], st->chip.AD.Offset[2]);

  for (i = 0; i < 3; i++)
    {
      range = 1.0 - (double) (wMaxValue[i] - wMinValue[i]) / MAX_LEVEL_RANGE;
      if (range > 0)
	{
	  st->chip.AD.Gain[i] = (SANE_Byte) (range * 63 * 6 / 5);
	  st->chip.AD.Gain[i] = _MIN (st->chip.AD.Gain[i], 63);
	}
      else
      	st->chip.AD.Gain[i] = 0;
    }

  DBG (DBG_FUNC, "GainR=%d,GainG=%d,GainB=%d\n",
       st->chip.AD.Gain[0], st->chip.AD.Gain[1], st->chip.AD.Gain[2]);

  for (i = 0; i < 10; i++)
    {
      status = Asic_ScanStart (&st->chip);
      if (status != SANE_STATUS_GOOD)
	goto out;
      status = Asic_ReadCalibrationData (&st->chip, pCalData, wCalWidth * 3,
					 SANE_TRUE);
      if (status != SANE_STATUS_GOOD)
	goto out;
      status = Asic_ScanStop (&st->chip);
      if (status != SANE_STATUS_GOOD)
	goto out;

      for (j = 0; j < 3; j++)
	{
	  status = CalculateMaxMin (&calParam, pCalData + (wCalWidth * j),
				    &wMaxValue[j], &wMinValue[j]);
	  if (status != SANE_STATUS_GOOD)
	    goto out;
	  DBG (DBG_FUNC, "%d: Max=%d, Min=%d\n", j, wMaxValue[j], wMinValue[j]);

	  if ((wMaxValue[j] - wMinValue[j]) > MAX_LEVEL_RANGE)
	    {
	      if (st->chip.AD.Gain[j] != 0)
		st->chip.AD.Gain[j]--;
	    }
	  else if ((wMaxValue[j] - wMinValue[j]) < MIN_LEVEL_RANGE)
	    {
	      if (wMaxValue[j] > WHITE_MAX_LEVEL)
		{
		  if (st->chip.AD.Gain[j] != 0)
		    st->chip.AD.Gain[j]--;
		}
	      else
		{
		  if (st->chip.AD.Gain[j] < 63)
		    st->chip.AD.Gain[j]++;
		}
	    }
	  else
	    {
	      if (wMaxValue[j] > WHITE_MAX_LEVEL)
		{
		  if (st->chip.AD.Gain[j] != 0)
		    st->chip.AD.Gain[j]--;
		}
	      else if (wMaxValue[j] < WHITE_MIN_LEVEL)
		{
		  if (st->chip.AD.Gain[j] < 63)
		    st->chip.AD.Gain[j]++;
		}
	    }

	  DBG (DBG_FUNC, "%d: Gain=%d,Offset=%d,Dir=%d\n",
	       j, st->chip.AD.Gain[j], st->chip.AD.Offset[j],
	       st->chip.AD.Direction[j]);
	}

      SetAFEGainOffset (&st->chip);

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
      status = Asic_ScanStart (&st->chip);
      if (status != SANE_STATUS_GOOD)
	goto out;
      status = Asic_ReadCalibrationData (&st->chip, pCalData, wCalWidth * 3,
					 SANE_TRUE);
      if (status != SANE_STATUS_GOOD)
	goto out;
      status = Asic_ScanStop (&st->chip);
      if (status != SANE_STATUS_GOOD)
	goto out;

      for (j = 0; j < 3; j++)
	{
	  status = CalculateMaxMin (&calParam, pCalData + (wCalWidth * j),
				    &wMaxValue[j], &wMinValue[j]);
	  if (status != SANE_STATUS_GOOD)
	    goto out;
	  DBG (DBG_FUNC, "%d: Max=%d, Min=%d\n", j, wMaxValue[j], wMinValue[j]);

	  if (st->chip.AD.Direction[j] == DIR_POSITIVE)
	    {
	      if (wMinValue[j] > 20)
		{
		  if (st->chip.AD.Offset[j] < 8)
		    st->chip.AD.Direction[j] = DIR_NEGATIVE;
		  else
		    st->chip.AD.Offset[j] -= 8;
		}
	      else if (wMinValue[j] < 10)
		{
		  st->chip.AD.Offset[j] += 8;
		}
	    }
	  else
	    {
	      if (wMinValue[j] > 20)
		st->chip.AD.Offset[j] += 8;
	      else if (wMinValue[j] < 10)
		st->chip.AD.Offset[j] -= 8;
	    }

	  DBG (DBG_FUNC, "%d: Gain=%d,Offset=%d,Dir=%d\n",
	       j, st->chip.AD.Gain[j], st->chip.AD.Offset[j],
	       st->chip.AD.Direction[j]);
	}

      SetAFEGainOffset (&st->chip);

      if (!(wMinValue[0] > 20 || wMinValue[0] < 10 ||
	    wMinValue[1] > 20 || wMinValue[1] < 10 ||
	    wMinValue[2] > 20 || wMinValue[2] < 10))
	break;
    }

out:
  free (pCalData);

  DBG_LEAVE ();
  return status;
}

static SANE_Status
FindTopLeft (Scanner_State * st, unsigned short * pwStartX,
	     unsigned short * pwStartY)
{
  SANE_Status status;
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
  DBG_ENTER ();

  if (st->Target.ssScanSource == SS_REFLECTIVE)
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
      return SANE_STATUS_NO_MEM;
    }
  nScanBlock = dwTotalSize / CALIBRATION_BLOCK_SIZE;

  status = Asic_SetWindow (&st->chip, st->Target.ssScanSource,
			   SCAN_TYPE_CALIBRATE_LIGHT, 8,
			   FIND_LEFT_TOP_CALIBRATE_RESOLUTION,
			   FIND_LEFT_TOP_CALIBRATE_RESOLUTION, 0, 0,
			   wCalWidth, wCalHeight);
  if (status != SANE_STATUS_GOOD)
    goto out;

  status = Asic_ScanStart (&st->chip);
  if (status != SANE_STATUS_GOOD)
    goto out;

  for (i = 0; i < nScanBlock; i++)
    {
      status = Asic_ReadCalibrationData (&st->chip,
	pCalData + (i * CALIBRATION_BLOCK_SIZE), CALIBRATION_BLOCK_SIZE,
	SANE_FALSE);
      if (status != SANE_STATUS_GOOD)
	goto out;
    }

  status = Asic_ReadCalibrationData (&st->chip,
    pCalData + (nScanBlock * CALIBRATION_BLOCK_SIZE),
    dwTotalSize - CALIBRATION_BLOCK_SIZE * nScanBlock, SANE_FALSE);
  if (status != SANE_STATUS_GOOD)
    goto out;

  status = Asic_ScanStop (&st->chip);
  if (status != SANE_STATUS_GOOD)
    goto out;

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

  if (st->Target.ssScanSource == SS_REFLECTIVE)
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

      status = Asic_MotorMove (&st->chip, SANE_FALSE,
	(wCalHeight - *pwStartY + LINE_CALIBRATION_HEIGHT) *
	SENSOR_DPI / FIND_LEFT_TOP_CALIBRATE_RESOLUTION);
      if (status != SANE_STATUS_GOOD)
	goto out;
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

      status = Asic_MotorMove (&st->chip, SANE_FALSE,
	(wCalHeight - *pwStartY) * SENSOR_DPI /
	FIND_LEFT_TOP_CALIBRATE_RESOLUTION + 300);
      if (status != SANE_STATUS_GOOD)
	goto out;
    }

  DBG (DBG_FUNC, "pwStartY=%d,pwStartX=%d\n", *pwStartY, *pwStartX);

out:
  free (pCalData);

  DBG_LEAVE ();
  return status;
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

static SANE_Status
LineCalibration16Bits (Scanner_State * st)
{
  SANE_Status status;
  unsigned short * pWhiteData, * pDarkData;
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
  DBG_ENTER ();

  wCalWidth = st->Target.wWidth;
  wCalHeight = LINE_CALIBRATION_HEIGHT;
  dwTotalSize = wCalWidth * wCalHeight * 3 * sizeof (unsigned short);
  DBG (DBG_FUNC, "wCalWidth=%d,wCalHeight=%d\n", wCalWidth, wCalHeight);

  pWhiteData = malloc (dwTotalSize);
  pDarkData = malloc (dwTotalSize);
  if (!pWhiteData || !pDarkData)
    {
      DBG (DBG_FUNC, "malloc failed\n");
      status = SANE_STATUS_NO_MEM;
      goto out1;
    }

  /* read white level data */
  status = Asic_SetWindow (&st->chip, st->Target.ssScanSource,
			   SCAN_TYPE_CALIBRATE_LIGHT, 48, st->Target.wXDpi, 600,
			   st->Target.wX, 0, wCalWidth, wCalHeight);
  if (status != SANE_STATUS_GOOD)
    goto out1;

  status = Asic_ScanStart (&st->chip);
  if (status != SANE_STATUS_GOOD)
    goto out1;

  status = Asic_ReadCalibrationData (&st->chip, (SANE_Byte *) pWhiteData,
				     dwTotalSize, SANE_FALSE);
  if (status != SANE_STATUS_GOOD)
    goto out1;

  status = Asic_ScanStop (&st->chip);
  if (status != SANE_STATUS_GOOD)
    goto out1;

  /* read dark level data */
  status = Asic_SetWindow (&st->chip, st->Target.ssScanSource,
			   SCAN_TYPE_CALIBRATE_DARK, 48, st->Target.wXDpi, 600,
			   st->Target.wX, 0, wCalWidth, wCalHeight);
  if (status != SANE_STATUS_GOOD)
    goto out1;

  if (st->Target.ssScanSource == SS_REFLECTIVE)
    status = Asic_TurnLamp (&st->chip, SANE_FALSE);
  else
    status = Asic_TurnTA (&st->chip, SANE_FALSE);
  if (status != SANE_STATUS_GOOD)
    goto out1;

  usleep (500000);

  status = Asic_ScanStart (&st->chip);
  if (status != SANE_STATUS_GOOD)
    goto out1;

  status = Asic_ReadCalibrationData (&st->chip, (SANE_Byte *) pDarkData,
				     dwTotalSize, SANE_FALSE);
  if (status != SANE_STATUS_GOOD)
    goto out1;

  status = Asic_ScanStop (&st->chip);
  if (status != SANE_STATUS_GOOD)
    goto out1;

  if (st->Target.ssScanSource == SS_REFLECTIVE)
    status = Asic_TurnLamp (&st->chip, SANE_TRUE);
  else
    status = Asic_TurnTA (&st->chip, SANE_TRUE);
  if (status != SANE_STATUS_GOOD)
    goto out1;

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
      status = SANE_STATUS_NO_MEM;
      goto out2;
    }

  /* compute dark level */
  for (i = 0; i < wCalWidth; i++)
    {
      for (j = 0; j < wCalHeight; j++)
	{
	  pRDarkSort[j] = le16toh (pDarkData[j * wCalWidth * 3 + i * 3]);
	  pGDarkSort[j] = le16toh (pDarkData[j * wCalWidth * 3 + i * 3 + 1]);
	  pBDarkSort[j] = le16toh (pDarkData[j * wCalWidth * 3 + i * 3 + 2]);
	}

      /* sum of dark level for all pixels */
      if ((st->Target.wXDpi == SENSOR_DPI) && ((i % 2) == 0))
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

  if (st->Target.wXDpi == SENSOR_DPI)
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
  if (st->Target.ssScanSource != SS_REFLECTIVE)
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
	  pRWhiteSort[j] = le16toh (pWhiteData[j * wCalWidth * 3 + i * 3]);
	  pGWhiteSort[j] = le16toh (pWhiteData[j * wCalWidth * 3 + i * 3 + 1]);
	  pBWhiteSort[j] = le16toh (pWhiteData[j * wCalWidth * 3 + i * 3 + 2]);
	}

      if ((st->Target.wXDpi == SENSOR_DPI) && ((i % 2) == 0))
	{
	  pDarkShading[i * 3] = dwREvenDarkLevel;
	  pDarkShading[i * 3 + 1] = dwGEvenDarkLevel;
	  if (st->Target.ssScanSource == SS_POSITIVE)
	    pDarkShading[i * 3 + 1] *= 0.78;
	  pDarkShading[i * 3 + 2] = dwBEvenDarkLevel;
	}
      else
	{
	  pDarkShading[i * 3] = dwRDarkLevel;
	  pDarkShading[i * 3 + 1] = dwGDarkLevel;
	  if (st->Target.ssScanSource == SS_POSITIVE)
	    pDarkShading[i * 3 + 1] *= 0.78;
	  pDarkShading[i * 3 + 2] = dwBDarkLevel;
	}

      wRWhiteLevel = FiltLower (pRWhiteSort, wCalHeight, 20, 30) -
		     pDarkShading[i * 3];
      wGWhiteLevel = FiltLower (pGWhiteSort, wCalHeight, 20, 30) -
		     pDarkShading[i * 3 + 1];
      wBWhiteLevel = FiltLower (pBWhiteSort, wCalHeight, 20, 30) -
		     pDarkShading[i * 3 + 2];

      switch (st->Target.ssScanSource)
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
	    			       ((65536 * 1.04) / wGWhiteLevel * 0x1000);
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

  status = Asic_SetShadingTable (&st->chip, pWhiteShading, pDarkShading,
				 st->Target.wXDpi, wCalWidth);

out2:
  free (pWhiteShading);
  free (pDarkShading);

  free (pRWhiteSort);
  free (pGWhiteSort);
  free (pBWhiteSort);
  free (pRDarkSort);
  free (pGDarkSort);
  free (pBDarkSort);

out1:
  free (pWhiteData);
  free (pDarkData);

  DBG_LEAVE ();
  return status;
}

static SANE_Status
CreateGammaTable (COLORMODE cmColorMode, unsigned short ** pGammaTable)
{
  unsigned short * pTable = NULL;
  DBG_ENTER ();

  if ((cmColorMode == CM_GRAY8) || (cmColorMode == CM_RGB24))
    {
      unsigned short i;
      SANE_Byte bGammaData;

      pTable = malloc (sizeof (unsigned short) * 4096 * 3);
      if (!pTable)
	{
	  DBG (DBG_ERR, "pTable == NULL\n");
	  return SANE_STATUS_NO_MEM;
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
	  return SANE_STATUS_NO_MEM;
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

  DBG_LEAVE ();
  return SANE_STATUS_GOOD;
}

SANE_Status
Scanner_SetupScan (Scanner_State * st, TARGETIMAGE * pTarget)
{
  SANE_Status status;
  unsigned int startPos;
  unsigned short finalY;
  SANE_Byte bScanBits;
  DBG_ENTER ();

  if (st->bOpened || !st->bPrepared)
    {
      DBG (DBG_FUNC, "invalid state\n");
      return SANE_STATUS_INVAL;
    }

  st->Target = *pTarget;
  st->SWWidth = st->Target.wWidth;
  st->SWHeight = st->Target.wHeight;
  /* set real scan width according to ASIC limit: width must be 8x */
  st->Target.wWidth = (st->Target.wWidth + 15) & ~15;

  status = CreateGammaTable (st->Target.cmColorMode, &st->pGammaTable);
  if (status != SANE_STATUS_GOOD)
    return status;

  switch (st->Target.wYDpi)
    {
    case 1200:
      st->wPixelDistance = 4;	/* even & odd sensor problem */
      st->wLineDistance = 24;
      st->Target.wHeight += st->wPixelDistance;
      break;
    case 600:
      st->wPixelDistance = 0;	/* no even & odd problem */
      st->wLineDistance = 12;
      break;
    case 300:
      st->wPixelDistance = 0;
      st->wLineDistance = 6;
      break;
    case 150:
      st->wPixelDistance = 0;
      st->wLineDistance = 3;
      break;
    case 75:
    case 50:
      st->wPixelDistance = 0;
      st->wLineDistance = 1;
      break;
    default:
      st->wPixelDistance = 0;
      st->wLineDistance = 0;
    }

  DBG (DBG_FUNC, "wYDpi=%d,st->wLineDistance=%d,st->wPixelDistance=%d\n",
       st->Target.wYDpi, st->wLineDistance, st->wPixelDistance);

  switch (st->Target.cmColorMode)
    {
    case CM_RGB48:
      st->BytesPerRow = 6 * st->Target.wWidth;
      st->SWBytesPerRow = 6 * st->SWWidth;
      st->Target.wHeight += st->wLineDistance * 2;
      bScanBits = 48;
      break;
    default:
    case CM_RGB24:
      st->BytesPerRow = 3 * st->Target.wWidth;
      st->SWBytesPerRow = 3 * st->SWWidth;
      st->Target.wHeight += st->wLineDistance * 2;
      bScanBits = 24;
      break;
    case CM_GRAY16:
      st->BytesPerRow = 2 * st->Target.wWidth;
      st->SWBytesPerRow = 2 * st->SWWidth;
      bScanBits = 16;
      break;
    case CM_GRAY8:
      st->BytesPerRow = st->Target.wWidth;
      st->SWBytesPerRow = st->SWWidth;
      bScanBits = 8;
      break;
    case CM_TEXT:
      /* 1-bit image data is generated from an 8-bit grayscale scan */
      st->BytesPerRow = st->Target.wWidth;
      st->SWBytesPerRow = st->SWWidth / 8;
      bScanBits = 8;
      break;
    }

  status = Asic_Open (&st->chip);
  if (status != SANE_STATUS_GOOD)
    return status;

  st->bOpened = SANE_TRUE;

  /* move carriage to start position for calibration */
  if (st->Target.ssScanSource == SS_REFLECTIVE)
    startPos = st->params->calibrationStartPos;
  else
    startPos = st->params->calibrationTaStartPos;

  if (startPos != 0)
    {
      status = Asic_MotorMove (&st->chip, SANE_TRUE, startPos);
      if (status != SANE_STATUS_GOOD)
	return status;
    }

  if (st->Target.wXDpi == SENSOR_DPI)
    {
      st->Target.wXDpi = SENSOR_DPI / 2;
      status = AdjustAD (st);
      if (status != SANE_STATUS_GOOD)
	return status;
      if (FindTopLeft (st, &st->Target.wX, &st->Target.wY) != SANE_STATUS_GOOD)
	{
	  st->Target.wX = 187;
	  st->Target.wY = 43;
	}

      st->Target.wXDpi = SENSOR_DPI;
      status = AdjustAD (st);
      if (status != SANE_STATUS_GOOD)
	return status;

      st->Target.wX = st->Target.wX * SENSOR_DPI /
		      FIND_LEFT_TOP_CALIBRATE_RESOLUTION + pTarget->wX;
      if (st->Target.ssScanSource == SS_REFLECTIVE)
	{
	  st->Target.wX += 47;
	  st->Target.wY = st->Target.wY * SENSOR_DPI /
			  FIND_LEFT_TOP_CALIBRATE_RESOLUTION +
			  pTarget->wY * SENSOR_DPI / st->Target.wYDpi + 47;
	}
    }
  else
    {
      status = AdjustAD (st);
      if (status != SANE_STATUS_GOOD)
	return status;
      if (FindTopLeft (st, &st->Target.wX, &st->Target.wY) != SANE_STATUS_GOOD)
	{
	  st->Target.wX = 187;
	  st->Target.wY = 43;
	}

      st->Target.wX += pTarget->wX * (SENSOR_DPI / 2) / st->Target.wXDpi;
      if (st->Target.ssScanSource == SS_REFLECTIVE)
	{
	  if (st->Target.wXDpi != 75)
	    st->Target.wX += 23;
	  st->Target.wY = st->Target.wY * SENSOR_DPI /
			  FIND_LEFT_TOP_CALIBRATE_RESOLUTION +
			  pTarget->wY * SENSOR_DPI / st->Target.wYDpi + 47;
	}
      else
	{
	  if (st->Target.wXDpi == 75)
	    st->Target.wX -= 23;
	}
    }

  DBG (DBG_FUNC, "before LineCalibration16Bits wX=%d,wY=%d\n",
       st->Target.wX, st->Target.wY);

  status = LineCalibration16Bits (st);
  if (status != SANE_STATUS_GOOD)
    return status;

  DBG (DBG_FUNC, "after LineCalibration16Bits wX=%d,wY=%d\n",
       st->Target.wX, st->Target.wY);

  DBG (DBG_FUNC, "bScanBits=%d,wXDpi=%d,wYDpi=%d,wWidth=%d,wHeight=%d\n",
       bScanBits, st->Target.wXDpi, st->Target.wYDpi,
       st->Target.wWidth, st->Target.wHeight);

  if (st->Target.ssScanSource == SS_REFLECTIVE)
    {
      finalY = 300;
    }
  else
    {
      st->Target.wY = pTarget->wY * SENSOR_DPI / st->Target.wYDpi +
		      (300 - 40) + 189;
      finalY = 360;
    }

  if (st->Target.wY > finalY)
    status = Asic_MotorMove (&st->chip, SANE_TRUE, st->Target.wY - finalY);
  else
    status = Asic_MotorMove (&st->chip, SANE_FALSE, finalY - st->Target.wY);
  if (status != SANE_STATUS_GOOD)
    return status;
  st->Target.wY = finalY;

  status = Asic_SetWindow (&st->chip, st->Target.ssScanSource, SCAN_TYPE_NORMAL,
			   bScanBits, st->Target.wXDpi, st->Target.wYDpi,
			   st->Target.wX, st->Target.wY,
			   st->Target.wWidth, st->Target.wHeight);
  if (status != SANE_STATUS_GOOD)
    return status;

  status = PrepareScan(st);

  DBG_LEAVE ();
  return status;
}
