/* sane - Scanner Access Now Easy.

   Copyright (C) 2003 Oliver Rauch
   Copyright (C) 2003-2005 Henning Meier-Geinitz <henning@meier-geinitz.de>
   Copyright (C) 2004, 2005 Gerhard Jaeger <gerhard@gjaeger.de>
   Copyright (C) 2004-2007 Stephane Voltz <stef.dev@free.fr>
   Copyright (C) 2005 Pierre Willenbrock <pierre@pirsoft.dnsalias.org>
   Copyright (C) 2007 Luke <iceyfor@gmail.com>
   
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
*/

/* ------------------------------------------------------------------------ */
/*                     Some setup DAC and CCD tables                        */
/* ------------------------------------------------------------------------ */

/** Setup table for various scanners using a Wolfson DAC
 */
static Genesys_Frontend Wolfson[] = {
  {{0x00, 0x03, 0x05, 0x11}
   , {0x00, 0x00, 0x00}
   , {0x80, 0x80, 0x80}
   , {0x02, 0x02, 0x02}
   , {0x00, 0x00, 0x00}
   }
  ,				/* UMAX */
  {{0x00, 0x03, 0x05, 0x03}
   , {0x00, 0x00, 0x00}
   , {0xc8, 0xc8, 0xc8}
   , {0x04, 0x04, 0x04}
   , {0x00, 0x00, 0x00}
   }
  ,				/* ST12 */
  {{0x00, 0x03, 0x05, 0x21}
   , {0x00, 0x00, 0x00}
   , {0xc8, 0xc8, 0xc8}
   , {0x06, 0x06, 0x06}
   , {0x00, 0x00, 0x00}
   }
  ,				/* ST24 */
  {{0x00, 0x03, 0x05, 0x12}
   , {0x00, 0x00, 0x00}
   , {0xc8, 0xc8, 0xc8}
   , {0x04, 0x04, 0x04}
   , {0x00, 0x00, 0x00}
   }
  ,				/* MD6228/MD6471 */
  {{0x00, 0x03, 0x05, 0x02}
   , {0x00, 0x00, 0x00}
   , {0xc0, 0xc0, 0xc0}
   , {0x07, 0x07, 0x07}
   , {0x00, 0x00, 0x00}
   }
  ,				/* HP2400c */
  {{0x00, 0x03, 0x04, 0x02}
   , {0x00, 0x00, 0x00}
   , {0xb0, 0xb0, 0xb0}
   , {0x04, 0x04, 0x04}
   , {0x00, 0x00, 0x00}
   }
  ,				/* HP2300c */
  {{0x00, 0x3d, 0x08, 0x00}
   , {0x00, 0x00, 0x00}
   , {0xe1, 0xe1, 0xe1}
   , {0x93, 0x93, 0x93}
   , {0x00, 0x19, 0x06}
   }
  ,				/* CANONLIDE35 */
};


/** for setting up the sensor-specific settings:
 * Optical Resolution, number of black pixels, number of dummy pixels, 
 * CCD_start_xoffset, and overall number of sensor pixels
 * registers 0x08-0x0b, 0x10-0x1d and 0x52-0x59
 */
static Genesys_Sensor Sensor[] = {
  /* UMAX */
  {1200, 48, 64, 0, 10800, 210, 230,
   {0x01, 0x03, 0x05, 0x07}
   ,
   {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x33, 0x05, 0x31, 0x2a, 0x00, 0x00,
    0x00, 0x02}
   ,
   {0x13, 0x17, 0x03, 0x07, 0x0b, 0x0f, 0x23, 0x00, 0xc1, 0x00, 0x00, 0x00,
    0x00}
   ,
   1.0, 1.0, 1.0,
   NULL, NULL, NULL}
  ,
  /* Plustek OpticPro S12/ST12 */
  {600, 48, 85, 152, 5416, 210, 230,
   {0x02, 0x00, 0x06, 0x04}
   ,
   {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2b, 0x08, 0x20, 0x2a, 0x00, 0x00,
    0x0c, 0x03}
   ,
   {0x0f, 0x13, 0x17, 0x03, 0x07, 0x0b, 0x83, 0x00, 0xc1, 0x00, 0x00, 0x00,
    0x00}
   ,
   1.0, 1.0, 1.0,
   NULL, NULL, NULL}
  ,
  /* Plustek OpticPro S24/ST24 */
  {1200, 48, 64, 0, 10800, 210, 230,
   {0x0e, 0x0c, 0x00, 0x0c}
   ,
   {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x33, 0x08, 0x31, 0x2a, 0x00, 0x00,
    0x00, 0x02}
   ,
   {0x17, 0x03, 0x07, 0x0b, 0x0f, 0x13, 0x03, 0x00, 0xc1, 0x00, 0x00, 0x00,
    0x00}
   ,
   1.0, 1.0, 1.0,
   NULL, NULL, NULL}
  ,
  /* MD6471 */
  {1200,
   48,
   16, 0, 10872,
   210, 200,
   {0x0d, 0x0f, 0x11, 0x13}
   ,
   {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0b, 0x0a, 0x30, 0x2a, 0x00, 0x00,
    0x00, 0x03}
   ,
   {0x0f, 0x13, 0x17, 0x03, 0x07, 0x0b, 0x23, 0x00, 0xc1, 0x00, 0x00, 0x00,
    0x00}
   ,
   2.38, 2.35, 2.34,
   NULL, NULL, NULL}
  ,
  /* HP2400c */
  {1200,
   48,
   15, 0, 10872, 210, 200,
   {0x14, 0x15, 0x00, 0x00}
   ,
   {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xbf, 0x08, 0x3f, 0x2a, 0x00, 0x00,
    0x00, 0x02}
   ,
   {0x0b, 0x0f, 0x13, 0x17, 0x03, 0x07, 0x63, 0x00, 0xc1, 0x00, 0x00, 0x00,
    0x00}
   ,
   1.0, 1.0, 1.0,
   NULL, NULL, NULL}
  ,
  /* HP2300c */
  {600,
   48,
   20, 0, 5454, 210, 200,
   {0x16, 0x00, 0x01, 0x03}
   ,
   {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xb7, 0x0a, 0x20, 0x2a, 0x6a, 0x8a,
    0x00, 0x05}
   ,
   {0x0f, 0x13, 0x17, 0x03, 0x07, 0x0b, 0x83, 0x00, 0xc1, 0x06, 0x0b, 0x10,
    0x16}
   ,
   2.1, 2.1, 2.1,
   NULL, NULL, NULL}
  ,
  /* CANOLIDE35 */
  {1200,
/*TODO: find a good reason for keeping all three following variables*/
   87,				/*(black) */
   87,				/* (dummy) */
   0,				/* (startxoffset) */
   10400,			/*sensor_pixels */
   210,
   200,
   {0x00, 0x00, 0x00, 0x00},
   {0x04, 0x00, 0x04, 0x00, 0x04, 0x00, 0x00, 0x02, 0x00, 0x50,
    0x00, 0x00, 0x00, 0x00	/* TODO(these do no harm, but may be neccessery for CCD) */
    },
   {0x05, 0x07,
    0x00, 0x00, 0x00, 0x00,	/*[GB](HI|LOW) not needed for cis */
    0x3a, 0x03,
    0x40,			/*TODO: bit7 */
    0x00, 0x00, 0x00, 0x00	/*TODO (these do no harm, but may be neccessery for CCD) */
    }
   ,
   1.0, 1.0, 1.0,
   NULL, NULL, NULL}
};

/** for General Purpose Output specific settings:
 * initial GPO value (registers 0x66-0x67/0x6c-0x6d)
 * GPO enable mask (registers 0x68-0x69/0x6e-0x6f)
 */
static Genesys_Gpo Gpo[] = {
  /* UMAX */
  {
   {0x11, 0x00}
   ,
   {0x51, 0x20}
   ,
   }
  ,
  /* Plustek OpticPro S12/ST12 */
  {
   {0x11, 0x00}
   ,
   {0x51, 0x20}
   ,
   }
  ,
  /* Plustek OpticPro S24/ST24 */
  {
   {0x00, 0x00}
   ,
   {0x51, 0x20}
   ,
   }
  ,
  /* MD5345/MD6471 */
  {
   {0x30, 0x00}
   ,				/* bits 11-12 are for bipolar V-ref input voltage */
   {0xa0, 0x18}
   ,
   }
  ,
  /* HP2400C */
  {
   {0x30, 0x00}
   ,
   {0x31, 0x00}
   ,
   }
  ,
  /* HP2300C */
  {
   {0x00, 0x00}
   ,
   {0x00, 0x00}
   ,
   }
  ,
  /* CANONLIDE35 */
  {
   {0x81, 0x80}
   ,
   {0xef, 0x80}
   ,
   }
};

#define MOTOR_ST24       2
static Genesys_Motor Motor[] = {
  /* UMAX */
  {
   1200,			/* motor base steps */
   2400,			/* maximum motor resolution */
   1,				/* maximum step mode */
   1,                           /* number of power modes*/
   {{{   
     11000,			/* maximum start speed */
     3000,			/* maximum end speed */
     128,			/* step count */
     1.0,			/* nonlinearity */
     },
    {
     11000,
     3000,
     128,
     1.0,
   },},},
  },
  {				/* MD5345/6228/6471 */
   1200,
   2400,
   1,
   1,
   {{{   
     2000,
     1375,
     128,
     0.5,
     },
    {
     2000,
     1375,
     128,
     0.5,
    },},},
  },
  {				/* ST24 */
   2400,
   2400,
   1,
   1,
   {{{
     2289,
     2100,
     128,
     0.3,
     },
    {
     2289,
     2100,
     128,
     0.3,
    },},}, 
  },
  {				/* HP 2400c */
   1200,
   2400,
   1,
   1,
   {{{
     11000,
     3000,
     128,
     1.0,
     },
    {
     11000,
     3000,
     128,
     1.0,
    },},},
  },
  {				/* HP 2300c */
   600,
   1200,
   1,
   1,
   {{{
     3200,
     1200,
     128,
     0.5,
     },
    {
     3200,
     1200,
     128,
     0.5,
   },},},
  },
  {				/* Canon LiDE 35 */
   1200,
   2400,
   1,
   1,
   {{{
     3500,
     1300,
     60,
     0.8,
     },
    {
     3500,
     1400,
     60,
     0.8,
    },},},
  },
};

/* here we have the various device settings...
 */
static Genesys_Model umax_astra_4500_model = {
  "umax-astra-4500",		/* Name */
  "UMAX",			/* Device vendor string */
  "Astra 4500",			/* Device model name */
  GENESYS_GL646,
  NULL,

  {1200, 600, 300, 150, 75, 0},	/* possible x-resolutions */
  {2400, 1200, 600, 300, 150, 75, 0},	/* possible y-resolutions */
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 8, 0},			/* possible depths in color mode */

  SANE_FIX (3.5),		/* Start of scan area in mm  (x) */
  SANE_FIX (7.5),		/* Start of scan area in mm (y) */
  SANE_FIX (218.0),		/* Size of scan area in mm (x) */
  SANE_FIX (299.0),		/* Size of scan area in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in mm (y) */
  SANE_FIX (1.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  0, 8, 16,			/* RGB CCD Line-distance correction in pixel */

  COLOR_ORDER_BGR,		/* Order of the CCD/CIS colors */

  SANE_FALSE,			/* Is this a CIS scanner? */
  CCD_UMAX,
  DAC_WOLFSON_UMAX,
  GPO_UMAX,
  MOTOR_UMAX,
  GENESYS_FLAG_UNTESTED,	/* Which flags are needed for this scanner? */
  /* untested, values set by hmg */
  20,
  200
};

static Genesys_Model canon_lide_50_model = {
  "canon-lide-50",		/* Name */
  "Canon",			/* Device vendor string */
  "LiDE 35/40/50",		/* Device model name */
  GENESYS_GL841,
  NULL,

  {1200, 600, 300, 150, 75, 0},	/* possible x-resolutions */
  {2400, 1200, 600, 300, 150, 75, 0},	/* possible y-resolutions */
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 8, 0},			/* possible depths in color mode */

  SANE_FIX (0.42),		/* Start of scan area in mm  (x) */
  SANE_FIX (7.9),		/* Start of scan area in mm (y) */
  SANE_FIX (218.0),		/* Size of scan area in mm (x) */
  SANE_FIX (299.0),		/* Size of scan area in mm (y) */

  SANE_FIX (3.0),		/* Start of white strip in mm (y) */
  SANE_FIX (0.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  0, 0, 0,			/* RGB CCD Line-distance correction in pixel */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */

  SANE_TRUE,			/* Is this a CIS scanner? */
  CCD_CANONLIDE35,
  DAC_CANONLIDE35,
  GPO_CANONLIDE35,
  MOTOR_CANONLIDE35,
  GENESYS_FLAG_LAZY_INIT | GENESYS_FLAG_SKIP_WARMUP | GENESYS_FLAG_OFFSET_CALIBRATION | GENESYS_FLAG_DARK_WHITE_CALIBRATION,	/* Which flags are needed for this scanner? */
  280,
  400
};

static Genesys_Model canon_lide_60_model = {
  "canon-lide-60",		/* Name */
  "Canon",			/* Device vendor string */
  "LiDE 60",			/* Device model name */
  GENESYS_GL841,
  NULL,

  {1200, 600, 300, 150, 75, 0},	/* possible x-resolutions */
  {2400, 1200, 600, 300, 150, 75, 0},	/* possible y-resolutions */
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 8, 0},			/* possible depths in color mode */

  SANE_FIX (0.42),		/* Start of scan area in mm  (x) */
  SANE_FIX (7.9),		/* Start of scan area in mm (y) */
  SANE_FIX (218.0),		/* Size of scan area in mm (x) */
  SANE_FIX (299.0),		/* Size of scan area in mm (y) */

  SANE_FIX (3.0),		/* Start of white strip in mm (y) */
  SANE_FIX (0.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  0, 0, 0,			/* RGB CCD Line-distance correction in pixel */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */

  SANE_TRUE,			/* Is this a CIS scanner? */
  CCD_CANONLIDE35,
  DAC_CANONLIDE35,
  GPO_CANONLIDE35,
  MOTOR_CANONLIDE35,
  GENESYS_FLAG_LAZY_INIT | GENESYS_FLAG_SKIP_WARMUP | GENESYS_FLAG_OFFSET_CALIBRATION | GENESYS_FLAG_DARK_WHITE_CALIBRATION,	/* Which flags are needed for this scanner? */
  300,
  400
};				/* this is completely untested -- hmg */

static Genesys_Model hp2300c_model = {
  "hewlett-packard-scanjet-2300c",	/* Name */
  "Hewlett Packard",		/* Device vendor string */
  "ScanJet 2300c",		/* Device model name */
  GENESYS_GL646,
  NULL,

  {600, 300, 150, 75, 0},	/* possible x-resolutions */
  {1200, 600, 300, 150, 75, 0},	/* possible y-resolutions, motor can go up to 1200 dpi */
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 8, 0},			/* possible depths in color mode */

  SANE_FIX (-2.0),		/* Start of scan area in mm (x_offset) */
  SANE_FIX (0.0),		/* Start of scan area in mm (y_offset) */
  SANE_FIX (215.9),		/* Size of scan area in mm (x) */
  SANE_FIX (295.0),		/* Size of scan area in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in mm (y) */
  SANE_FIX (1.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  16, 8, 0,			/* RGB CCD Line-distance correction in pixel */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */

  SANE_FALSE,			/* Is this a CIS scanner? */
  CCD_HP2300,
  DAC_WOLFSON_HP2300,
  GPO_HP2300,
  MOTOR_HP2300,
  GENESYS_FLAG_REPARK
    | GENESYS_FLAG_14BIT_GAMMA
    | GENESYS_FLAG_SEARCH_START
    | GENESYS_FLAG_MUST_WAIT
    | GENESYS_FLAG_DARK_CALIBRATION | GENESYS_FLAG_OFFSET_CALIBRATION,
  9,
  132
};

static Genesys_Model hp2400c_model = {
  "hewlett-packard-scanjet-2400c",	/* Name */
  "Hewlett Packard",		/* Device vendor string */
  "ScanJet 2400c",		/* Device model name */
  GENESYS_GL646,
  NULL,

  {1200, 600, 300, 150, 75, 0},	/* possible x-resolutions */
  {2400, 1200, 600, 300, 150, 75, 0},	/* possible y-resolutions */
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 8, 0},			/* possible depths in color mode */

  SANE_FIX (3.5),		/* Start of scan area in mm  (x) */
  SANE_FIX (7.5),		/* Start of scan area in mm (y) */
  SANE_FIX (215.9),		/* Size of scan area in mm (x) */
  SANE_FIX (297.2),		/* Size of scan area in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in mm (y) */
  SANE_FIX (1.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  0, 8, 16,			/* RGB CCD Line-distance correction in pixel */

  COLOR_ORDER_BGR,		/* Order of the CCD/CIS colors */

  SANE_FALSE,			/* Is this a CIS scanner? */
  CCD_HP2400,
  DAC_WOLFSON_HP2400,
  GPO_HP2400,
  MOTOR_HP2400,
  GENESYS_FLAG_UNTESTED		/* not fully working yet */
    | GENESYS_FLAG_REPARK
    | GENESYS_FLAG_14BIT_GAMMA
    | GENESYS_FLAG_SEARCH_START
    | GENESYS_FLAG_MUST_WAIT
    | GENESYS_FLAG_DARK_CALIBRATION | GENESYS_FLAG_OFFSET_CALIBRATION,
  20,
  132
};

static Genesys_Model hp3670c_model = {
  "hewlett-packard-scanjet-3670c",	/* Name */
  "Hewlett Packard",		/* Device vendor string */
  "ScanJet 3670c",		/* Device model name */
  GENESYS_GL646,
  NULL,

  {1200, 600, 300, 150, 75, 0},	/* possible x-resolutions */
  {2400, 1200, 600, 300, 150, 75, 0},	/* possible y-resolutions */
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 8, 0},			/* possible depths in color mode */

  SANE_FIX (3.5),		/* Start of scan area in mm  (x) */
  SANE_FIX (7.5),		/* Start of scan area in mm (y) */
  SANE_FIX (215.9),		/* Size of scan area in mm (x) */
  SANE_FIX (297.2),		/* Size of scan area in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in mm (y) */
  SANE_FIX (1.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  0, 8, 16,			/* RGB CCD Line-distance correction in pixel */

  COLOR_ORDER_BGR,		/* Order of the CCD/CIS colors */

  SANE_FALSE,			/* Is this a CIS scanner? */
  CCD_UMAX,
  DAC_WOLFSON_UMAX,
  GPO_UMAX,
  MOTOR_UMAX,
  GENESYS_FLAG_UNTESTED,	/* Which flags are needed for this scanner? */
  /* untested, values set by mike p. according to vendor's datasheet. */
  20,
  200
};

static Genesys_Model plustek_st12_model = {
  "plustek-opticpro-st12",	/* Name */
  "Plustek",			/* Device vendor string */
  "OpticPro ST12",		/* Device model name */
  GENESYS_GL646,
  NULL,

  {600, 300, 150, 75, 0},	/* possible x-resolutions */
  {1200, 600, 300, 150, 75, 0},	/* possible y-resolutions */
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 8, 0},			/* possible depths in color mode */

  SANE_FIX (3.5),		/* Start of scan area in mm  (x) */
  SANE_FIX (7.5),		/* Start of scan area in mm (y) */
  SANE_FIX (218.0),		/* Size of scan area in mm (x) */
  SANE_FIX (299.0),		/* Size of scan area in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in mm (y) */
  SANE_FIX (1.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  0, 8, 16,			/* RGB CCD Line-distance correction in pixel */

  COLOR_ORDER_BGR,		/* Order of the CCD/CIS colors */

  SANE_FALSE,			/* Is this a CIS scanner? */
  CCD_ST12,
  DAC_WOLFSON_ST12,
  GPO_ST12,
  MOTOR_UMAX,
  GENESYS_FLAG_UNTESTED | GENESYS_FLAG_14BIT_GAMMA,	/* Which flags are needed for this scanner? */
  20,
  200
};

static Genesys_Model plustek_st24_model = {
  "plustek-opticpro-st24",	/* Name */
  "Plustek",			/* Device vendor string */
  "OpticPro ST24",		/* Device model name */
  GENESYS_GL646,
  NULL,

  {1200, 600, 300, 150, 75, 0},	/* possible x-resolutions */
  {2400, 1200, 600, 300, 150, 75, 0},	/* possible y-resolutions */
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 8, 0},			/* possible depths in color mode */

  SANE_FIX (3.5),		/* Start of scan area in mm  (x) */
  SANE_FIX (7.5),		/* Start of scan area in mm (y) */
  SANE_FIX (218.0),		/* Size of scan area in mm (x) */
  SANE_FIX (299.0),		/* Size of scan area in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in mm (y) */
  SANE_FIX (1.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  0, 8, 16,			/* RGB CCD Line-distance correction in pixel */

  COLOR_ORDER_BGR,		/* Order of the CCD/CIS colors */

  SANE_FALSE,			/* Is this a CIS scanner? */
  CCD_ST24,
  DAC_WOLFSON_ST24,
  GPO_ST24,
  MOTOR_ST24,
  GENESYS_FLAG_UNTESTED
    | GENESYS_FLAG_14BIT_GAMMA
    | GENESYS_FLAG_LAZY_INIT
    | GENESYS_FLAG_USE_PARK
    | GENESYS_FLAG_SKIP_WARMUP
    | GENESYS_FLAG_SEARCH_START | GENESYS_FLAG_OFFSET_CALIBRATION,
  20,
  200
};

static Genesys_Model medion_md5345_model = {
  "medion-md5345-model",	/* Name */
  "Medion",			/* Device vendor string */
  "MD5345/MD6228/MD6471",	/* Device model name */
  GENESYS_GL646,
  NULL,

  {1200, 600, 300, 200, 150, 100, 75, 50, 0},	/* possible x-resolutions */
  {2400, 1200, 600, 500, 400, 300, 250, 200, 150, 100, 50, 0},	/* possible y-resolutions */
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 8, 0},			/* possible depths in color mode */

  SANE_FIX (1.00),		/* Start of scan area in mm  (x) */
  SANE_FIX (5.00),		/* 2.79 < Start of scan area in mm (y) */
  SANE_FIX (215.9),		/* Size of scan area in mm (x) */
  SANE_FIX (296.4),		/* Size of scan area in mm (y) */

  SANE_FIX (0.00),		/* Start of white strip in mm (y) */
  SANE_FIX (0.00),		/* Start of black mark in mm (x) */

  SANE_FIX (0.00),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.00),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (0.00),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (0.00),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.00),		/* Start of white strip in TA mode in mm (y) */

  48, 24, 0,			/* RGB CCD Line-distance correction in pixel */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */

  SANE_FALSE,			/* Is this a CIS scanner? */
  CCD_5345,
  DAC_WOLFSON_5345,
  GPO_5345,
  MOTOR_5345,
  GENESYS_FLAG_14BIT_GAMMA
    | GENESYS_FLAG_LAZY_INIT
    | GENESYS_FLAG_USE_PARK
    | GENESYS_FLAG_SKIP_WARMUP
    | GENESYS_FLAG_SEARCH_START
    | GENESYS_FLAG_DARK_CALIBRATION
    | GENESYS_FLAG_STAGGERED_LINE | GENESYS_FLAG_OFFSET_CALIBRATION,
  32,
  200
};

static Genesys_USB_Device_Entry genesys_usb_device_list[] = {
  {0x0638, 0x0a10, &umax_astra_4500_model},
  {0x04a9, 0x2213, &canon_lide_50_model},
  {0x04a9, 0x221c, &canon_lide_60_model},
  {0x03f0, 0x0901, &hp2300c_model},
  {0x03f0, 0x0a01, &hp2400c_model},
  {0x03f0, 0x1405, &hp3670c_model},
  {0x07b3, 0x0600, &plustek_st12_model},
  {0x07b3, 0x0601, &plustek_st24_model},
  {0x0461, 0x0377, &medion_md5345_model},
  {0, 0, NULL}
};
