/*
  Copyright (C) 2001 Bertrik Sikken (bertrik@zonnet.nl)

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

  $Id$
*/

/*
    Concept for a backend for scanners based on the NIASH chipset,
    such as HP3300C, HP3400C, HP4300C, Agfa Touch.
    Parts of this source were inspired by other backends.
*/

#include "../include/sane/config.h"
#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"
#include "../include/sane/sanei_backend.h"
#include "../include/sane/sanei_config.h"
#include "../include/sane/saneopts.h"

#include <stdlib.h>		/* malloc, free */
#include <string.h>		/* memcpy */
#include <stdio.h>
#include <sys/time.h>
#include <sys/wait.h>

/* definitions for debug */
#define BACKEND_NAME niash
#define BUILD 1

#define DBG_ASSERT  1
#define DBG_ERR     16
#define DBG_MSG     32

/* Just to avoid conflicts between niash backend and testtool */
#define WITH_NIASH 1


/* (source) includes for data transfer methods */
#define STATIC static

#include "niash_core.c"
#include "niash_xfer.c"


#define ASSERT(cond) (!(cond) ? DBG(DBG_ASSERT, "!!! ASSERT(%S) FAILED!!!\n",STRINGIFY(cond));)


#define MM_TO_PIXEL(_mm_, _dpi_)    ((_mm_) * (_dpi_) / 25.4 )
#define PIXEL_TO_MM(_pixel_, _dpi_) ((_pixel_) * 25.4 / (_dpi_) )


/* options enumerator */
typedef enum
{
  optCount = 0,

  optGroupGeometry,
  optTLX, optTLY, optBRX, optBRY,
  optDPI,

  optGroupImage,
  optGammaTable,		/* gamma table */

  optLast,
/* put temporarily disabled options here after optLast */

  optGroupMisc,
  optLamp,

  optCalibrate,
  optGamma			/* analog gamma = single number */
} EOptionIndex;


typedef union
{
  SANE_Word w;
  SANE_Word *wa;		/* word array */
  SANE_String s;
} TOptionValue;


typedef struct
{
  SANE_Option_Descriptor aOptions[optLast];
  TOptionValue aValues[optLast];

  TScanParams ScanParams;
  THWParams HWParams;

  TDataPipe DataPipe;
  int iLinesLeft;

  SANE_Int aGammaTable[4096];	/* a 12-to-8 bit color lookup table */

  /* fCancelled needed to let sane issue the cancel message
     instead of an error message */
  SANE_Bool fCancelled;		/* SANE_TRUE if scanning cancelled */

  SANE_Bool fScanning;		/* SANE_TRUE if actively scanning */

  int WarmUpTime;		/* time to wait before a calibration starts */
  unsigned char CalWhite[3];	/* values for the last calibration of white */
  struct timeval WarmUpStarted;
  /* system type to trace the time elapsed */
} TScanner;


/* linked list of SANE_Device structures */
typedef struct TDevListEntry
{
  struct TDevListEntry *pNext;
  SANE_Device dev;
} TDevListEntry;


static TDevListEntry *_pFirstSaneDev = 0;
static int iNumSaneDev = 0;
static const SANE_Device **_pSaneDevList = 0;


/* option constraints */
static const SANE_Range rangeGammaTable = { 0, 255, 1 };

/* available scanner resolutions */
static const SANE_Int setResolutions[] = { 4, 75, 150, 300, 600 };

static const SANE_Range rangeGamma = { SANE_FIX (0.25), SANE_FIX (4.0),
  SANE_FIX (0.0)
};
static const SANE_Range rangeXmm = { 0, 220, 1 };
static const SANE_Range rangeYmm = { 0, 290, 1 };
static const SANE_Int startUpGamma = SANE_FIX (1.6);

#define WARMUP_AFTERSTART    1	/* flag for 1st warm up */
#define WARMUP_INSESSION     0
#define WARMUP_TESTINTERVAL 15	/* test every 15sec */
#define WARMUP_TIME         30	/* first wait is 30sec minimum */
#define WARMUP_MAXTIME      90	/* after one and a half minute start latest */

#define CAL_DEV_MAX         25
/* maximum deviation of cal values in pecent
  bewteen 2 tests */

/* different warm up after start and after automatic off */
static const int aiWarmUpTime[] = { WARMUP_TESTINTERVAL, WARMUP_TIME };

/* returns 1, when the warm up time "iTime" has elasped */
static int
_TimeElapsed (struct timeval *start, struct timeval *now, int iTime)
{

  /* this is a bit strange, but can cope with overflows */
  if (start->tv_sec > now->tv_sec)
    return (start->tv_sec / 2 - now->tv_sec / 2 > iTime / 2);
  else
    return (now->tv_sec - start->tv_sec >= iTime);
}

static void
_WarmUpLamp (TScanner * s, int iMode)
{
  SANE_Bool fLampOn;
  /* on startup don't care what was before
     assume lamp was off, and the previous
     cal values can never be reached */
  if (iMode == WARMUP_AFTERSTART)
    {
      fLampOn = SANE_FALSE;
      s->CalWhite[0] = s->CalWhite[1] = s->CalWhite[2] = (unsigned char) (-1);
    }
  else
    GetLamp (&s->HWParams, &fLampOn);

  if (!fLampOn)
    {
      /* get the current system time */
      gettimeofday (&s->WarmUpStarted, 0);
      /* determine the time to wait at least */
      s->WarmUpTime = aiWarmUpTime[iMode];
      /* switch on the lamp */
      SetLamp (&s->HWParams, SANE_TRUE);
    }
}

static void
_WaitForLamp (TScanner * s, unsigned char *pabCalibTable)
{
  struct timeval now[2];	/* toggling time holder */
  int i;			/* rgb loop */
  int iCal = 0;			/* counter */
  int iCurrent = 0;		/* buffer and time-holder swap flag */
  SANE_Bool fHasCal;
  unsigned char CalWhite[2][3];	/* toggling buffer */
  int iDelay = 0;		/* delay loop counter */
  _WarmUpLamp (s, SANE_FALSE);


  /* get the time stamp for the wait loops */
  if (s->WarmUpTime)
    gettimeofday (&now[iCurrent], 0);
  SimpleCalibExt (&s->HWParams, pabCalibTable, CalWhite[iCurrent]);
  fHasCal = SANE_TRUE;

  DBG (DBG_MSG, "_WaitForLamp: first calibration\n");


  /* wait until time has elapsed or for values to stabilze */
  while (s->WarmUpTime)
    {
      /* check if the last scan has lower calibration values than
         the current one would have */
      if (s->WarmUpTime && fHasCal)
	{
	  SANE_Bool fOver = SANE_TRUE;
	  for (i = 0; fOver && i < 3; ++i)
	    {
	      if (!s->CalWhite[i])
		fOver = SANE_FALSE;
	      else if (CalWhite[iCurrent][i] < s->CalWhite[i])
		fOver = SANE_FALSE;
	    }

	  /* warm up is not needed, when calibration data is above
	     the calibration data of the last scan */
	  if (fOver)
	    {
	      s->WarmUpTime = 0;
	      DBG (DBG_MSG,
		   "_WaitForLamp: Values seem stable, skipping next calibration cycle\n");
	    }
	}


      /* break the loop, when the longest wait time has expired
         to prevent a hanging application, 
         even if the values might not be good, yet */
      if (s->WarmUpTime && fHasCal && iCal)
	{
	  /* abort, when we have waited long enough */
	  if (_TimeElapsed
	      (&s->WarmUpStarted, &now[iCurrent], WARMUP_MAXTIME))
	    {
	      /* stop idling */
	      s->WarmUpTime = 0;
	      DBG (DBG_MSG, "_WaitForLamp: WARMUP_MAXTIME=%ds elapsed!\n",
		   WARMUP_MAXTIME);
	    }
	}


      /* enter a delay loop, when there is still time to wait */
      if (s->WarmUpTime)
	{
	  /* if the (too low) calibration values have just been acquired
	     we start waiting */
	  if (fHasCal)
	    DBG (DBG_MSG, "_WaitForLamp: entering delay loop\r");
	  else
	    DBG (DBG_MSG, "_WaitForLamp: delay loop %d        \r", ++iDelay);
	  sleep (1);
	  fHasCal = SANE_FALSE;
	  gettimeofday (&now[!iCurrent], 0);
	}


      /* look if we should check again */
      if (s->WarmUpTime		/* did we have to wait at all */
	  /* is the minimum time elapsed */
	  && _TimeElapsed (&s->WarmUpStarted, &now[!iCurrent], s->WarmUpTime)
	  /* has the minimum time elapsed since the last calibration */
	  && _TimeElapsed (&now[iCurrent], &now[!iCurrent],
			   WARMUP_TESTINTERVAL))
	{
	  int dev = 0;		/* 0 percent deviation in cal value as default */
	  iDelay = 0;		/* all delays processed */
	  /* new calibration */
	  ++iCal;
	  iCurrent = !iCurrent;	/* swap the test-buffer, and time-holder */
	  SimpleCalibExt (&s->HWParams, pabCalibTable, CalWhite[iCurrent]);
	  fHasCal = SANE_TRUE;

	  for (i = 0; i < 3; ++i)
	    {
	      /* copy for faster and clearer access */
	      int cwa;
	      int cwb;
	      int ldev;
	      cwa = CalWhite[!iCurrent][i];
	      cwb = CalWhite[iCurrent][i];
	      /* find the biggest deviation of one color */
	      if (cwa > cwb)
		ldev = 0;
	      else if (cwa && cwb)
		ldev = ((cwb - cwa) * 100) / cwb;
	      else
		ldev = 100;
	      dev = MAX (dev, ldev);
	    }

	  /* show the biggest deviation of the calibration values */
	  DBG (DBG_MSG, "_WaitForLamp: recalibration #%d, deviation = %d%%\n",
	       iCal, dev);

	  /* the deviation to the previous calibration is tolerable */
	  if (dev <= CAL_DEV_MAX)
	    s->WarmUpTime = 0;
	}
    }

  /* remember the values of this calibration
     for the next time */
  for (i = 0; i < 3; ++i)
    {
      s->CalWhite[i] = CalWhite[iCurrent][i];
    }
}


static void
_SetAnalogGamma (SANE_Int * aiGamma, SANE_Int sfGamma)
{
  int j;
  double fGamma;
  fGamma = ((double) sfGamma) / (0x01 << SANE_FIXED_SCALE_SHIFT);
  for (j = 0; j < 4096; j++)
    {
      int iData;
      iData = floor (256.0 * pow (((double) j / 4096.0), 1.0 / fGamma));
      if (iData > 255)
	iData = 255;
      aiGamma[j] = iData;
    }
}

static void
_InitOptions (TScanner * s)
{
  int i;
  SANE_Option_Descriptor *pDesc;
  TOptionValue *pVal;
  _SetAnalogGamma (s->aGammaTable, startUpGamma);

  for (i = optCount; i < optLast; i++)
    {

      pDesc = &s->aOptions[i];
      pVal = &s->aValues[i];

      /* defaults */
      pDesc->name = "";
      pDesc->title = "";
      pDesc->desc = "";
      pDesc->type = SANE_TYPE_INT;
      pDesc->unit = SANE_UNIT_NONE;
      pDesc->size = sizeof (SANE_Word);
      pDesc->constraint_type = SANE_CONSTRAINT_NONE;
      pDesc->cap = 0;

      switch (i)
	{

	case optCount:
	  pDesc->title = SANE_TITLE_NUM_OPTIONS;
	  pDesc->desc = SANE_DESC_NUM_OPTIONS;
	  pDesc->cap = SANE_CAP_SOFT_DETECT;
	  pVal->w = (SANE_Word) optLast;
	  break;

	case optGroupGeometry:
	  pDesc->title = "Geometry";
	  pDesc->type = SANE_TYPE_GROUP;
	  pDesc->size = 0;
	  break;

	case optTLX:
	  pDesc->name = SANE_NAME_SCAN_TL_X;
	  pDesc->title = SANE_TITLE_SCAN_TL_X;
	  pDesc->desc = SANE_DESC_SCAN_TL_X;
	  pDesc->unit = SANE_UNIT_MM;
	  pDesc->constraint_type = SANE_CONSTRAINT_RANGE;
	  pDesc->constraint.range = &rangeXmm;
	  pDesc->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
	  pVal->w = rangeXmm.min;
	  break;

	case optTLY:
	  pDesc->name = SANE_NAME_SCAN_TL_Y;
	  pDesc->title = SANE_TITLE_SCAN_TL_Y;
	  pDesc->desc = SANE_DESC_SCAN_TL_Y;
	  pDesc->unit = SANE_UNIT_MM;
	  pDesc->constraint_type = SANE_CONSTRAINT_RANGE;
	  pDesc->constraint.range = &rangeYmm;
	  pDesc->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
	  pVal->w = rangeYmm.min;
	  break;

	case optBRX:
	  pDesc->name = SANE_NAME_SCAN_BR_X;
	  pDesc->title = SANE_TITLE_SCAN_BR_X;
	  pDesc->desc = SANE_DESC_SCAN_BR_X;
	  pDesc->unit = SANE_UNIT_MM;
	  pDesc->constraint_type = SANE_CONSTRAINT_RANGE;
	  pDesc->constraint.range = &rangeXmm;
	  pDesc->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
	  pVal->w = rangeXmm.max;
	  break;

	case optBRY:
	  pDesc->name = SANE_NAME_SCAN_BR_Y;
	  pDesc->title = SANE_TITLE_SCAN_BR_Y;
	  pDesc->desc = SANE_DESC_SCAN_BR_Y;
	  pDesc->unit = SANE_UNIT_MM;
	  pDesc->constraint_type = SANE_CONSTRAINT_RANGE;
	  pDesc->constraint.range = &rangeYmm;
	  pDesc->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
	  pVal->w = rangeYmm.max;
	  break;

	case optDPI:
	  pDesc->name = SANE_NAME_SCAN_RESOLUTION;
	  pDesc->title = SANE_TITLE_SCAN_RESOLUTION;
	  pDesc->desc = SANE_DESC_SCAN_RESOLUTION;
	  pDesc->unit = SANE_UNIT_DPI;
	  pDesc->constraint_type = SANE_CONSTRAINT_WORD_LIST;
	  pDesc->constraint.word_list = setResolutions;
	  pDesc->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
	  pVal->w = setResolutions[2];	/* default to 150dpi */
	  break;

	case optGroupImage:
	  pDesc->title = SANE_I18N ("Image");
	  pDesc->type = SANE_TYPE_GROUP;
	  pDesc->size = 0;
	  break;

	case optGamma:
	  pDesc->name = SANE_NAME_ANALOG_GAMMA;
	  pDesc->title = SANE_TITLE_ANALOG_GAMMA;
	  pDesc->desc = SANE_DESC_ANALOG_GAMMA;
	  pDesc->type = SANE_TYPE_FIXED;
	  pDesc->constraint_type = SANE_CONSTRAINT_RANGE;
	  pDesc->constraint.range = &rangeGamma;
	  pDesc->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
	  pVal->w = startUpGamma;
	  break;

	case optGammaTable:
	  pDesc->name = SANE_NAME_GAMMA_VECTOR;
	  pDesc->title = SANE_TITLE_GAMMA_VECTOR;
	  pDesc->desc = SANE_DESC_GAMMA_VECTOR;
	  pDesc->size = sizeof (s->aGammaTable);
	  pDesc->constraint_type = SANE_CONSTRAINT_RANGE;
	  pDesc->constraint.range = &rangeGammaTable;
	  pDesc->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
	  pVal->wa = s->aGammaTable;
	  break;

	case optGroupMisc:
	  pDesc->title = SANE_I18N ("Miscellaneous");
	  pDesc->type = SANE_TYPE_GROUP;
	  pDesc->size = 0;
	  break;

	case optLamp:
	  pDesc->name = "lamp";
	  pDesc->title = SANE_I18N ("Lamp status");
	  pDesc->desc = SANE_I18N ("Switches the lamp on or off.");
	  pDesc->type = SANE_TYPE_BOOL;
	  pDesc->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
	  /* switch the lamp on when starting for first the time */
	  pVal->w = SANE_TRUE;
	  break;

	case optCalibrate:
	  pDesc->name = "calibrate";
	  pDesc->title = SANE_I18N ("Calibrate");
	  pDesc->desc = SANE_I18N ("Calibrates for black and white level.");
	  pDesc->type = SANE_TYPE_BUTTON;
	  pDesc->cap = SANE_CAP_SOFT_SELECT;
	  pDesc->size = 0;
	  break;

	default:
	  DBG (DBG_ERR, "Uninitialised option %d\n", i);
	  break;
	}
    }
}


static int
_ReportDevice (TScannerModel * pModel, const char *pszDeviceName)
{
  TDevListEntry *pNew, *pDev;

  DBG (DBG_MSG, "niash: _ReportDevice '%s'\n", pszDeviceName);

  pNew = malloc (sizeof (TDevListEntry));
  if (!pNew)
    {
      DBG (DBG_ERR, "no mem\n");
      return -1;
    }

  /* add new element to the end of the list */
  if (_pFirstSaneDev == 0)
    {
      _pFirstSaneDev = pNew;
    }
  else
    {
      for (pDev = _pFirstSaneDev; pDev->pNext; pDev = pDev->pNext)
	{
	  ;
	}
      pDev->pNext = pNew;
    }

  /* fill in new element */
  pNew->pNext = 0;
  pNew->dev.name = strdup (pszDeviceName);
  pNew->dev.vendor = pModel->pszVendor;
  pNew->dev.model = pModel->pszName;
  pNew->dev.type = "flatbed scanner";

  iNumSaneDev++;

  return 0;
}

/*****************************************************************************/

SANE_Status
sane_init (SANE_Int * piVersion, SANE_Auth_Callback pfnAuth)
{
  /* prevent compiler from complaing about unused parameters */
  pfnAuth = pfnAuth;

  DBG_INIT ();
  DBG (DBG_MSG, "sane_init\n");

  if (piVersion != NULL)
    {
      *piVersion = SANE_VERSION_CODE (V_MAJOR, V_MINOR, BUILD);
    }

  /* initialise transfer methods */
  iNumSaneDev = 0;
  NiashXferInit (_ReportDevice);

  return SANE_STATUS_GOOD;
}


void
sane_exit (void)
{
  TDevListEntry *pDev, *pNext;

  DBG (DBG_MSG, "sane_exit\n");

  /* free device list memory */
  if (_pSaneDevList)
    {
      for (pDev = _pFirstSaneDev; pDev; pDev = pNext)
	{
	  pNext = pDev->pNext;
	  free ((void *) pDev->dev.name);
	  free (pDev);
	}
      _pFirstSaneDev = 0;
      free (_pSaneDevList);
      _pSaneDevList = 0;
    }
}


SANE_Status
sane_get_devices (const SANE_Device *** device_list, SANE_Bool local_only)
{
  TDevListEntry *pDev;
  int i;

  DBG (DBG_MSG, "sane_get_devices\n");

  local_only = local_only;

  if (_pSaneDevList)
    {
      free (_pSaneDevList);
    }

  _pSaneDevList = malloc (sizeof (*_pSaneDevList) * (iNumSaneDev + 1));
  if (!_pSaneDevList)
    {
      DBG (DBG_MSG, "no mem\n");
      return SANE_STATUS_NO_MEM;
    }
  i = 0;
  for (pDev = _pFirstSaneDev; pDev; pDev = pDev->pNext)
    {
      _pSaneDevList[i++] = &pDev->dev;
    }
  _pSaneDevList[i++] = 0;	/* last entry is 0 */

  *device_list = _pSaneDevList;

  return SANE_STATUS_GOOD;
}


SANE_Status
sane_open (SANE_String_Const name, SANE_Handle * h)
{
  TScanner *s;

  DBG (DBG_MSG, "sane_open: %s\n", name);

  /* check the name */
  if (strlen (name) == 0)
    {
      /* default to first available device */
      name = _pFirstSaneDev->dev.name;
    }

  s = malloc (sizeof (TScanner));
  if (!s)
    {
      DBG (DBG_MSG, "malloc failed\n");
      return SANE_STATUS_NO_MEM;
    }

  if (NiashOpen (&s->HWParams, name) < 0)
    {
      /* is this OK ? */
      DBG (DBG_ERR, "NiashOpen failed\n");
      free ((void *) s);
      return SANE_STATUS_DEVICE_BUSY;
    }
  _InitOptions (s);
  s->fScanning = SANE_FALSE;
  s->fCancelled = SANE_FALSE;
  *h = s;

  /* Turn on lamp by default at startup */
  _WarmUpLamp (s, WARMUP_AFTERSTART);

  return SANE_STATUS_GOOD;
}


void
sane_close (SANE_Handle h)
{
  TScanner *s;

  DBG (DBG_MSG, "sane_close\n");

  s = (TScanner *) h;

  /* turn off scanner lamp */
  SetLamp (&s->HWParams, SANE_FALSE);

  /* close scanner */
  NiashClose (&s->HWParams);

  /* free scanner object memory */
  free ((void *) s);
}


const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle h, SANE_Int n)
{
  TScanner *s;

  DBG (DBG_MSG, "sane_get_option_descriptor %d\n", n);

  if ((n < optCount) || (n >= optLast))
    {
      return NULL;
    }

  s = (TScanner *) h;
  return &s->aOptions[n];
}


SANE_Status
sane_control_option (SANE_Handle h, SANE_Int n, SANE_Action Action,
		     void *pVal, SANE_Int * pInfo)
{
  TScanner *s;
  SANE_Bool fVal;
  static char szTable[100];
  char szTemp[16];
  int *pi;
  int i;
  SANE_Int info;
  SANE_Bool fLampIsOn;
  SANE_Status status;
  SANE_Bool fSame;

  DBG (DBG_MSG, "sane_control_option: option %d, action %d\n", n, Action);

  s = (TScanner *) h;
  info = 0;

  switch (Action)
    {
    case SANE_ACTION_GET_VALUE:
      switch (n)
	{

	  /* Get options of type SANE_Word */
	case optCount:
	case optDPI:
	case optGamma:
	case optTLX:
	case optTLY:
	case optBRX:
	case optBRY:
	  DBG (DBG_MSG,
	       "sane_control_option: SANE_ACTION_GET_VALUE %d = %d\n", n,
	       (int) s->aValues[n].w);
	  *(SANE_Word *) pVal = s->aValues[n].w;
	  break;

	  /* Get options of type SANE_Word array */
	case optGammaTable:
	  DBG (DBG_MSG, "Reading gamma table\n");
	  memcpy (pVal, s->aValues[n].wa, s->aOptions[n].size);
	  break;

	  /* Get options of type SANE_Bool */
	case optLamp:
	  GetLamp (&s->HWParams, &fLampIsOn);
	  *(SANE_Bool *) pVal = fLampIsOn;
	  break;

	case optCalibrate:
	  /*  although this option has nothing to read,
	     it's added here to avoid a warning when running scanimage --help */
	  break;

	default:
	  DBG (DBG_MSG, "SANE_ACTION_GET_VALUE: Invalid option (%d)\n", n);
	}
      break;


    case SANE_ACTION_SET_VALUE:
      if (s->fScanning)
	{
	  DBG (DBG_ERR,
	       "sane_control_option: SANE_ACTION_SET_VALUE not allowed during scan\n");
	  return SANE_STATUS_INVAL;
	}
      switch (n)
	{

	case optCount:
	  return SANE_STATUS_INVAL;

	case optDPI:
	case optTLX:
	case optTLY:
	case optBRX:
	case optBRY:
	case optGamma:
	  info |= SANE_INFO_RELOAD_PARAMS;
	  status = sanei_constrain_value (&s->aOptions[n], pVal, &info);
	  if (status != SANE_STATUS_GOOD)
	    {
	      DBG (DBG_ERR, "Failed to constrain option %d (%s)\n", n,
		   s->aOptions[n].title);
	      return status;
	    }

	  /* check values if they are equal */
	  fSame = s->aValues[n].w == *(SANE_Word *) pVal;

	  /* set the values */
	  s->aValues[n].w = *(SANE_Word *) pVal;
	  DBG (DBG_MSG,
	       "sane_control_option: SANE_ACTION_SET_VALUE %d = %d\n", n,
	       (int) s->aValues[n].w);
	  if (n == optGamma)
	    {
	      if (!fSame && optLast > optGammaTable)
		{
		  info |= SANE_INFO_RELOAD_OPTIONS;
		}
	      _SetAnalogGamma (s->aGammaTable, s->aValues[n].w);
	    }
	  break;

	case optGammaTable:
	  DBG (DBG_MSG, "Writing gamma table\n");
	  pi = (SANE_Int *) pVal;
	  memcpy (s->aValues[n].wa, pVal, s->aOptions[n].size);

	  /* prepare table for debug */
	  strcpy (szTable, "Gamma table summary:");
	  for (i = 0; i < 4096; i++)
	    {
	      if ((i % 256) == 0)
		{
		  strcat (szTable, "\n");
		  DBG (DBG_MSG, szTable);
		  strcpy (szTable, "");
		}
	      /* test for number print */
	      if ((i % 32) == 0)
		{
		  sprintf (szTemp, " %04X", pi[i]);
		  strcat (szTable, szTemp);
		}
	    }
	  if (strlen (szTable))
	    {
	      strcat (szTable, "\n");
	      DBG (DBG_MSG, szTable);
	    }
	  break;

	case optLamp:
	  fVal = *(SANE_Bool *) pVal;
	  DBG (DBG_MSG, "lamp %s\n", fVal ? "on" : "off");
	  if (fVal)
	    _WarmUpLamp (s, WARMUP_INSESSION);
	  else
	    SetLamp (&s->HWParams, SANE_FALSE);
	  break;

	case optCalibrate:
/*       SimpleCalib(&s->HWParams); */
	  break;

	default:
	  DBG (DBG_ERR, "SANE_ACTION_SET_VALUE: Invalid option (%d)\n", n);
	}
      if (pInfo != NULL)
	{
	  *pInfo = info;
	}
      break;


    case SANE_ACTION_SET_AUTO:
      return SANE_STATUS_UNSUPPORTED;


    default:
      DBG (DBG_ERR, "Invalid action (%d)\n", Action);
      return SANE_STATUS_INVAL;
    }

  return SANE_STATUS_GOOD;
}



SANE_Status
sane_get_parameters (SANE_Handle h, SANE_Parameters * p)
{
  TScanner *s;
  DBG (DBG_MSG, "sane_get_parameters\n");

  s = (TScanner *) h;

  /* first do some checks */
  if (s->aValues[optTLX].w >= s->aValues[optBRX].w)
    {
      DBG (DBG_ERR, "TLX should be smaller than BRX\n");
      return SANE_STATUS_INVAL;	/* proper error code? */
    }
  if (s->aValues[optTLY].w >= s->aValues[optBRY].w)
    {
      DBG (DBG_ERR, "TLY should be smaller than BRY\n");
      return SANE_STATUS_INVAL;	/* proper error code? */
    }

  /* return the data */
  p->format = SANE_FRAME_RGB;
  p->last_frame = SANE_TRUE;

  p->lines = MM_TO_PIXEL (s->aValues[optBRY].w - s->aValues[optTLY].w,
			  s->aValues[optDPI].w);
  p->depth = 8;
  p->pixels_per_line =
    MM_TO_PIXEL (s->aValues[optBRX].w - s->aValues[optTLX].w,
		 s->aValues[optDPI].w);
  p->bytes_per_line = p->pixels_per_line * 3;

  return SANE_STATUS_GOOD;
}

/* get the scale don factor for a resolution that is 
  not supported by hardware */
static int
_SaneEmulateScaling (int iDpi)
{
  if (iDpi == 75)
    return 2;
  else
    return 1;
}


SANE_Status
sane_start (SANE_Handle h)
{
  TScanner *s;
  SANE_Parameters par;
  int i, iLineCorr;
  int iScaleDown;
  static unsigned char abGamma[4096];
  static unsigned char abCalibTable[HW_PIXELS * 6];

  DBG (DBG_MSG, "sane_start\n");

  s = (TScanner *) h;

  if (sane_get_parameters (h, &par) != SANE_STATUS_GOOD)
    {
      DBG (DBG_MSG, "Invalid scan parameters\n");
      return SANE_STATUS_INVAL;
    }
  iScaleDown = _SaneEmulateScaling (s->aValues[optDPI].w);
  s->iLinesLeft = par.lines;

  /* fill in the scanparams using the option values */
  s->ScanParams.iDpi = s->aValues[optDPI].w * iScaleDown;
  s->ScanParams.iLpi = s->aValues[optDPI].w * iScaleDown;

  /* calculate correction for filling of circular buffer */
  iLineCorr = 3 * s->HWParams.iSensorSkew;	/* usually 16 motor steps */
  /* calculate correction for garbage lines */
  iLineCorr += s->HWParams.iSkipLines * (HW_LPI / s->ScanParams.iLpi);

  s->ScanParams.iTop =
    MM_TO_PIXEL (s->aValues[optTLY].w + s->HWParams.iTopLeftY,
		 HW_LPI) - iLineCorr;
  s->ScanParams.iLeft =
    MM_TO_PIXEL (s->aValues[optTLX].w + s->HWParams.iTopLeftX, HW_DPI);

  s->ScanParams.iWidth = par.pixels_per_line * iScaleDown;
  s->ScanParams.iHeight = par.lines * iScaleDown;
  s->ScanParams.iBottom = 14200UL;
  s->ScanParams.fCalib = SANE_FALSE;

  /* perform a simple calibration just before scanning */
  _WaitForLamp (s, abCalibTable);

  /* copy gamma table */
  for (i = 0; i < 4096; i++)
    {
      abGamma[i] = s->aGammaTable[i];
    }

  WriteGammaCalibTable (abGamma, abGamma, abGamma, abCalibTable, 0, 0,
			&s->HWParams);

  /* prepare the actual scan */
  if (!InitScan (&s->ScanParams, &s->HWParams))
    {
      DBG (DBG_MSG, "Invalid scan parameters\n");
      return SANE_STATUS_INVAL;
    }

  /* init data pipe */
  s->DataPipe.iSkipLines = s->HWParams.iSkipLines;
  /* on the hp3400 and hp4300 we cannot set the top of the scan area (yet),
     so instead we just scan and throw away the data until the top */
  if (s->HWParams.fReg07)
    {
      s->DataPipe.iSkipLines +=
	MM_TO_PIXEL (s->aValues[optTLY].w + s->HWParams.iTopLeftY,
		     s->aValues[optDPI].w * iScaleDown);
    }
  s->DataPipe.iBytesLeft = 0;
  /* hack */
  s->DataPipe.pabLineBuf = (unsigned char *) malloc (HW_PIXELS * 3);
  CircBufferInit (s->HWParams.iXferHandle, &s->DataPipe,
		  par.pixels_per_line, s->ScanParams.iHeight,
		  s->ScanParams.iLpi * s->HWParams.iSensorSkew / HW_LPI,
		  s->HWParams.iReversedHead, iScaleDown, iScaleDown);

  s->fScanning = SANE_TRUE;
  s->fCancelled = SANE_FALSE;
  return SANE_STATUS_GOOD;
}


SANE_Status
sane_read (SANE_Handle h, SANE_Byte * buf, SANE_Int maxlen, SANE_Int * len)
{
  TScanner *s;
  TDataPipe *p;

  DBG (DBG_MSG, "sane_read: buf=%p, maxlen=%d, ", buf, maxlen);

  s = (TScanner *) h;

  /* sane_read only allowed after sane_start */
  if (!s->fScanning)
    {
      if (s->fCancelled)
	{
	  DBG (DBG_MSG, "\n");
	  DBG (DBG_MSG, "sane_read: sane_read cancelled\n");
	  s->fCancelled = SANE_FALSE;
	  return SANE_STATUS_CANCELLED;
	}
      else
	{
	  DBG (DBG_ERR,
	       "sane_read: sane_read only allowed after sane_start\n");
	  return SANE_STATUS_INVAL;
	}
    }

  p = &s->DataPipe;

  /* anything left to read? */
  if ((s->iLinesLeft == 0) && (p->iBytesLeft == 0))
    {
      CircBufferExit (&s->DataPipe);
      free (p->pabLineBuf);
      FinishScan (&s->HWParams);
      *len = 0;
      DBG (DBG_MSG, "\n");
      DBG (DBG_MSG, "sane_read: end of scan\n");
      s->fCancelled = SANE_FALSE;
      s->fScanning = SANE_FALSE;
      return SANE_STATUS_EOF;
    }

  /* time to read the next line? */
  if (p->iBytesLeft == 0)
    {
      /* read a line from the transfer buffer */
      if (CircBufferGetLine (s->HWParams.iXferHandle, p, p->pabLineBuf,
			     s->HWParams.iReversedHead))
	{
	  p->iBytesLeft = p->iSaneBytesPerLine;
	  s->iLinesLeft--;
	}
      /* stop scanning further, when the read action fails
         because we try read after the end of the buffer */
      else
	{
	  CircBufferExit (&s->DataPipe);
	  free (p->pabLineBuf);
	  FinishScan (&s->HWParams);
	  *len = 0;
	  DBG (DBG_MSG, "\n");
	  DBG (DBG_MSG, "sane_read: read after end of buffer\n");
	  s->fCancelled = SANE_FALSE;
	  s->fScanning = SANE_FALSE;
	  return SANE_STATUS_EOF;
	}

    }

  /* copy (part of) a line */
  *len = MIN (maxlen, p->iBytesLeft);
  memcpy (buf, &p->pabLineBuf[p->iSaneBytesPerLine - p->iBytesLeft], *len);
  p->iBytesLeft -= *len;

  DBG (DBG_MSG, " read=%d\n", *len);

  return SANE_STATUS_GOOD;
}


void
sane_cancel (SANE_Handle h)
{
  TScanner *s;

  DBG (DBG_MSG, "sane_cancel\n");

  s = (TScanner *) h;

  /* to be implemented more thoroughly */

  /* Make sure the scanner head returns home */
  FinishScan (&s->HWParams);
  s->fCancelled = SANE_TRUE;
  s->fScanning = SANE_FALSE;
}


SANE_Status
sane_set_io_mode (SANE_Handle h, SANE_Bool m)
{
  DBG (DBG_MSG, "sane_set_io_mode %s\n", m ? "non-blocking" : "blocking");

  /* prevent compiler from complaining about unused parameters */
  h = h;

  if (m)
    {
      return SANE_STATUS_UNSUPPORTED;
    }
  return SANE_STATUS_GOOD;
}


SANE_Status
sane_get_select_fd (SANE_Handle h, SANE_Int * fd)
{
  DBG (DBG_MSG, "sane_select_fd\n");

  /* prevent compiler from complaining about unused parameters */
  h = h;
  fd = fd;

  return SANE_STATUS_UNSUPPORTED;
}
