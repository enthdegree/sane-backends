/* sane - Scanner Access Now Easy.

   Copyright (C) 2002 Sergey Vlasov <vsu@altlinux.ru>
   Copyright (C) 2002, 2003 Henning Meier-Geinitz <henning@meier-geinitz.de>
   
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

/* Scanner-specific data */

#include "gt68xx_low.h"
#include "gt68xx_generic.c"
#include "gt68xx_gt6801.c"
#include "gt68xx_gt6816.c"

static GT68xx_Command_Set mustek_gt6816_command_set = {
  "mustek-gt6816",		/* Name of this command set */

  0x40,				/* Request type */
  0x01,				/* Request */
  
  0x200c,			/* Memory read - wValue */
  0x200b,			/* Memory write - wValue */

  0x2010,			/* Send normal command - wValue */
  0x3f40,			/* Send normal command - wIndex */
  0x2011,			/* Receive normal result - wValue */
  0x3f00,			/* Receive normal result - wIndex */

  0x2012,			/* Send small command - wValue */
  0x3f40,			/* Send small command - wIndex */
  0x2013,			/* Receive small result - wValue */
  0x3f00,			/* Receive small result - wIndex */

  /* activate */ NULL,
  /* deactivate */ NULL,
  gt6816_check_firmware,
  gt6816_download_firmware,
  gt6816_get_power_status,
  gt6816_get_ta_status,
  gt6816_lamp_control,
  gt6816_is_moving,
  gt68xx_generic_move_relative,
  gt6816_carriage_home,
  gt68xx_generic_start_scan,
  gt68xx_generic_read_scanned_data,
  gt6816_stop_scan,
  gt6816_setup_scan,
  gt68xx_generic_set_afe,
  gt68xx_generic_set_exposure_time,
  gt68xx_generic_get_id
};

static GT68xx_Command_Set mustek_gt6801_command_set = {
  "mustek-gt6801",


  0x40,				/* Request type */
  0x01,				/* Request */

  0x200a,			/* Memory read - wValue */
  0x2009,			/* Memory write - wValue */

  0x2010,			/* Send normal command - wValue */
  0x3f40,			/* Send normal command - wIndex */
  0x2011,			/* Receive normal result - wValue */
  0x3f00,			/* Receive normal result - wIndex */

  0x2012,			/* Send small command - wValue */
  0x3f40,			/* Send small command - wIndex */
  0x2013,			/* Receive small result - wValue */
  0x3f00,			/* Receive small result - wIndex */

  /* activate */ NULL,
  /* deactivate */ NULL,
  gt6801_check_firmware,
  gt6801_download_firmware,
  gt6801_get_power_status,
  /* get_ta_status (FIXME: implement this) */ NULL,
  gt6801_lamp_control,
  gt6801_is_moving,
  /* gt68xx_generic_move_relative *** to be tested */ NULL,
  gt6801_carriage_home,
  gt68xx_generic_start_scan,
  gt68xx_generic_read_scanned_data,
  gt6801_stop_scan,
  gt6801_setup_scan,
  gt68xx_generic_set_afe,
  gt68xx_generic_set_exposure_time,
  gt68xx_generic_get_id
};

static GT68xx_Command_Set plustek_gt6801_command_set = {
  "plustek-gt6801",

  0x40,				/* Request type */
  0x04,				/* Request */

  0x200a,			/* Memory read - wValue */
  0x2009,			/* Memory write - wValue */

  0x2010,			/* Send normal command - wValue */
  0x3f40,			/* Send normal command - wIndex */
  0x2011,			/* Receive normal result - wValue */
  0x3f00,			/* Receive normal result - wIndex */

  0x2012,			/* Send small command - wValue */
  0x3f40,			/* Send small command - wIndex */
  0x2013,			/* Receive small result - wValue */
  0x3f00,			/* Receive small result - wIndex */

  /* activate */ NULL,
  /* deactivate */ NULL,
  gt6801_check_plustek_firmware,
  gt6801_download_firmware,
  gt6801_get_power_status,
  /* get_ta_status (FIXME: implement this) */ NULL,
  gt6801_lamp_control,
  gt6801_is_moving,
  /* gt68xx_generic_move_relative *** to be tested */ NULL,
  gt6801_carriage_home,
  gt68xx_generic_start_scan,
  gt68xx_generic_read_scanned_data,
  gt6801_stop_scan,
  gt6801_setup_scan,
  gt68xx_generic_set_afe,
  /* set_exposure_time */ NULL,
  gt68xx_generic_get_id
};

static GT68xx_Model mustek_2400ta_model = {
  "mustek-bearpaw-2400-ta",	/* Name */
  "Mustek",			/* Device vendor string */
  "BearPaw 2400 TA",		/* Device model name */
  "A2fw.usb",			/* Name of the firmware file */
  SANE_FALSE,			/* Dynamic allocation flag */

  &mustek_gt6816_command_set,	/* Command set used by this scanner */

  1200,				/* maximum optical sensor resolution */
  2400,				/* maximum motor resolution */
  1200,				/* base x-res used to calculate geometry */
  1200,				/* base y-res used to calculate geometry */
  1200,				/* if ydpi is equal or higher, use linemode */
  SANE_FALSE,			/* Use base_ydpi for all resolutions */

  {1200, 600, 300, 200, 100, 50, 0},	/* possible x-resolutions */
  {2400, 1200, 600, 300, 200, 100, 50, 0}, /* possible y-resolutions */
  {16, 12, 8, 0},		/* possible depths in gray mode */
  {16, 12, 8, 0},		/* possible depths in color mode */

  SANE_FIX (3.67),		/* Start of scan area in mm  (x) */
  SANE_FIX (7.4),		/* Start of scan area in mm (y) */
  SANE_FIX (219.0),		/* Size of scan area in mm (x) */
  SANE_FIX (298.0),		/* Size of scan area in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in mm (y) */
  SANE_FIX (0.635),		/* Start of black mark in mm (x) */

  SANE_FIX (94.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (107.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (37.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (165.0),		/* Size of scan area in TA mode in mm (y) */
  SANE_FIX (95.0),		/* Start of white strip in TA mode in mm (y) */

  0, 24, 48,			/* RGB CCD Line-distance correction in pixel */
  0,				/* CCD distcance for CCD with 6 lines) */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */
  {0x2a, 0x0c, 0x2e, 0x06, 0x2d, 0x07},	/* Default offset/gain */
  {0x157, 0x157, 0x157},	/* Default exposure parameters */
  SANE_FIX (2.0),		/* Default gamma value */

  SANE_FALSE,			/* Is this a CIS scanner? */
  0				/* Which flags are needed for this scanner? */
    /* flatbed values tested */
};

static GT68xx_Model mustek_2400taplus_model = {
  "mustek-bearpaw-2400-ta-plus",	/* Name */
  "Mustek",			/* Device vendor string */
  "BearPaw 2400 TA Plus",	/* Device model name */
  "A2Dfw.usb",			/* Name of the firmware file */
  SANE_FALSE,			/* Dynamic allocation flag */

  &mustek_gt6816_command_set,	/* Command set used by this scanner */

  1200,				/* maximum optical sensor resolution */
  2400,				/* maximum motor resolution */
  600,				/* base x-res used to calculate geometry */
  1200,				/* base y-res used to calculate geometry */
  1200,				/* if ydpi is equal or higher, use linemode */
  SANE_FALSE,			/* Use base_ydpi for all resolutions */

  {1200, 600, 300, 100, 50, 0},	/* possible x-resolutions */
  {2400, 1200, 600, 300, 100, 50, 0},	/* possible y-resolutions */
  {8, 0},			/* possible depths in gray mode */
  {16, 12, 8, 0},		/* possible depths in color mode */

  SANE_FIX (7.41),		/* Start of scan area in mm  (x) */
  SANE_FIX (7.4),		/* Start of scan area in mm (y) */
  SANE_FIX (217.5),		/* Size of scan area in mm (x) */
  SANE_FIX (298.0),		/* Size of scan area in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in mm (y) */
  SANE_FIX (5.0),		/* Start of black mark in mm (x) */

  SANE_FIX (94.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (107.0) /* Start of scan area in TA mode in mm (y) */ ,
  SANE_FIX (37.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (165.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (95.0),		/* Start of white strip in TA mode in mm (y) */

  0, 48, 96,			/* RGB CCD Line-distance correction in pixel */
  8,				/* CCD distcance for CCD with 6 lines) */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */
  {0x2a, 0x0c, 0x2e, 0x06, 0x2d, 0x07},	/* Default offset/gain */
  {0x157, 0x157, 0x157},	/* Default exposure parameters */
  SANE_FIX (2.0),		/* Default gamma value */

  SANE_FALSE,			/* Is this a CIS scanner? */
  0				/* Which flags are needed for this scanner? */
    /* Setup and tested */
};


static GT68xx_Model mustek_1200ta_model = {
  "mustek-bearpaw-1200-ta",	/* Name */
  "Mustek",			/* Device vendor string */
  "BearPaw 1200 TA",		/* Device model name */
  "A1fw.usb",			/* Name of the firmware file */
  SANE_FALSE,			/* Dynamic allocation flag */

  &mustek_gt6816_command_set,	/* Command set used by this scanner */

  600,				/* maximum optical sensor resolution */
  1200,				/* maximum motor resolution */
  600,				/* base x-res used to calculate geometry */
  600,				/* base y-res used to calculate geometry */
  1200,				/* if ydpi is equal or higher, use linemode */
  SANE_TRUE,			/* Use base_ydpi for all resolutions */

  {600, 300, 150, 75, 50, 0},	/* possible x-resolutions */
  {1200, 600, 300, 150, 75, 50, 0},	/* possible y-resolutions */
  {16, 12, 8, 0},		/* possible depths in gray mode */
  {16, 12, 8, 0},		/* possible depths in color mode */

  SANE_FIX (8.4),		/* Start of scan area in mm  (x) */
  SANE_FIX (7.5),		/* Start of scan area in mm (y) */
  SANE_FIX (220.0),		/* Size of scan area in mm (x) */
  SANE_FIX (299.0),		/* Size of scan area in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in mm (y) */
  SANE_FIX (7.0),		/* Start of black mark in mm (x) */

  SANE_FIX (98.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (108.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (37.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (163.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (95.0),		/* Start of white strip in TA mode in mm (y) */

  32, 16, 0,			/* RGB CCD Line-distance correction in pixel */
  0,				/* CCD distcance for CCD with 6 lines) */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */
  {0x2a, 0x0c, 0x2e, 0x06, 0x2d, 0x07},	/* Default offset/gain */
  {0x157, 0x157, 0x157},	/* Default exposure parameters */
  SANE_FIX (2.0),		/* Default gamma value */

  SANE_FALSE,			/* Is this a CIS scanner? */
  0				/* Which flags are needed for this scanner? */
    /* Setup for 1200 TA */
};

static GT68xx_Model mustek_1200cuplus_model = {
  "mustek-bearpaw-1200-cu-plus",	/* Name */
  "Mustek",			/* Device vendor string */
  "Bearpaw 1200 CU Plus",	/* Device model name */
  "PS1Dfw.usb",			/* Name of the firmware file */
  SANE_FALSE,			/* Dynamic allocation flag */

  &mustek_gt6816_command_set,	/* Command set used by this scanner */

  600,				/* maximum optical sensor resolution */
  1200,				/* maximum motor resolution */
  600,				/* base x-res used to calculate geometry */
  600,				/* base y-res used to calculate geometry */
  1200,				/* if ydpi is equal or higher, use linemode */
  SANE_FALSE,			/* Use base_ydpi for all resolutions */

  {600, 300, 150, 75, 50, 0},	/* possible x-resolutions */
  {1200, 600, 300, 150, 75, 50, 0},	/* possible y-resolutions */
  {16, 12, 8, 0},		/* possible depths in gray mode */
  {16, 12, 8, 0},		/* possible depths in color mode */

  SANE_FIX (0.0),		/* Start of scan area in mm  (x) */
  SANE_FIX (13.0),		/* Start of scan area in mm (y) */
  SANE_FIX (217.0),		/* Size of scan area in mm (x) */
  SANE_FIX (299.0),		/* Size of scan area in mm (y) */

  SANE_FIX (5.0),		/* Start of white strip in mm (y) */
  SANE_FIX (0.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  0, 0, 0,			/* RGB CCD Line-distance correction in pixel */
  0,				/* CCD distcance for CCD with 6 lines) */

  COLOR_ORDER_BGR,		/* Order of the CCD/CIS colors */
  {0x14, 0x07, 0x14, 0x07, 0x14, 0x07},	/* Default offset/gain */
  {0x157, 0x157, 0x157},	/* Default exposure parameters */
  SANE_FIX (2.0),		/* Default gamma value */

  SANE_TRUE,			/* Is this a CIS scanner? */
  0				/* Which flags are needed for this scanner? */
    /* Everything untested */
};

static GT68xx_Model mustek_2400cuplus_model = {
  "mustek-bearpaw-2400-cu-plus",	/* Name */
  "Mustek",			/* Device vendor string */
  "BearPaw 2400 CU Plus",	/* Device model name */
  "PS2Dfw.usb",			/* Name of the firmware file */
  SANE_FALSE,			/* Dynamic allocation flag */

  &mustek_gt6816_command_set,	/* Command set used by this scanner */

  1200,				/* maximum optical sensor resolution */
  2400,				/* maximum motor resolution */
  1200,				/* base x-res used to calculate geometry */
  1200,				/* base y-res used to calculate geometry */
  1200,				/* if ydpi is equal or higher, use linemode */
  SANE_FALSE,			/* Use base_ydpi for all resolutions */

  {1200, 600, 300, 200, 150, 100, 50, 0},	/* possible x-resolutions */
  {2400, 1200, 600, 300, 200, 150, 100, 50, 0},	/* possible y-resolutions */
  {16, 12, 8, 0},		/* possible depths in gray mode */
  {16, 12, 8, 0},		/* possible depths in color mode */

  SANE_FIX (2.0),		/* Start of scan area in mm  (x) */
  SANE_FIX (13.0),		/* Start of scan area in mm (y) */
  SANE_FIX (217.0),		/* Size of scan area in mm (x) */
  SANE_FIX (300.0),		/* Size of scan area in mm (y) */

  SANE_FIX (5.0),		/* Start of white strip in mm (y) */
  SANE_FIX (0.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  0, 0, 0,			/* RGB CCD Line-distance correction in pixel */
  0,				/* CCD distcance for CCD with 6 lines) */

  COLOR_ORDER_BGR,		/* Order of the CCD/CIS colors */
  {0x1a, 0x16, 0x15, 0x08, 0x0e, 0x02},	/* Default offset/gain */
  {0x157, 0x157, 0x157},	/* Default exposure parameters */
  SANE_FIX (2.0),		/* Default gamma value */

  SANE_TRUE,			/* Is this a CIS scanner? */
  0				/* Which flags are needed for this scanner? */
    /* Setup and tested */
};

/* Seems that Mustek ScanExpress 1200 UB Plus, the Mustek BearPaw 1200 CU
 * and lots of other scanners have the same USB identifier.
 */
static GT68xx_Model mustek_1200cu_model = {
  "mustek-bearpaw-1200-cu",	/* Name */
  "Mustek",			/* Device vendor string */
  "BearPaw 1200 CU",		/* Device model name */
  "PS1fw.usb",			/* Name of the firmware file */
  SANE_FALSE,			/* Dynamic allocation flag */

  &mustek_gt6801_command_set,	/* Command set used by this scanner */

  600,				/* maximum optical sensor resolution */
  1200,				/* maximum motor resolution */
  600,				/* base x-res used to calculate geometry */
  600,				/* base y-res used to calculate geometry */
  1200,				/* if ydpi is equal or higher, use linemode */
  SANE_TRUE,			/* Use base_ydpi for all resolutions */

  {600, 300, 150, 75, 50, 0},	/* possible x-resolutions */
  {1200, 600, 300, 150, 75, 50, 0},	/* possible y-resolutions */
  {12, 8, 0},			/* possible depths in gray mode */
  {12, 8, 0},			/* possible depths in color mode */

  SANE_FIX (0.0),		/* Start of scan area in mm  (x) */
  SANE_FIX (13.0),		/* Start of scan area in mm (y) */
  SANE_FIX (217.0),		/* Size of scan area in mm (x) */
  SANE_FIX (299.0),		/* Size of scan area in mm (y) */

  SANE_FIX (3.5),		/* Start of white strip in mm (y) */
  SANE_FIX (0.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  0, 0, 0,			/* RGB CCD Line-distance correction in pixel */
  0,				/* CCD distcance for CCD with 6 lines) */

  COLOR_ORDER_BGR,		/* Order of the CCD/CIS colors */
  {0x13, 0x04, 0x15, 0x06, 0x0f, 0x02},	/* Default offset/gain */
  {0x157, 0x157, 0x157},	/* Default exposure parameters */
  SANE_FIX (2.0),		/* Default gamma value */

  SANE_TRUE,			/* Is this a CIS scanner? */
  0				/* Which flags are needed for this scanner? */
    /* Setup and tested */
};

static GT68xx_Model mustek_scanexpress1200ubplus_model = {
  "mustek-scanexpress-1200-ub-plus",	/* Name */
  "Mustek",			/* Device vendor string */
  "ScanExpress 1200 UB Plus",	/* Device model name */
  "SBfw.usb",			/* Name of the firmware file */
  SANE_FALSE,			/* Dynamic allocation flag */

  &mustek_gt6801_command_set,	/* Command set used by this scanner */

  600,				/* maximum optical sensor resolution */
  1200,				/* maximum motor resolution */
  600,				/* base x-res used to calculate geometry */
  600,				/* base y-res used to calculate geometry */
  1200,				/* if ydpi is equal or higher, use linemode */
  SANE_TRUE,			/* Use base_ydpi for all resolutions */

  {600, 300, 150, 75, 50, 0},	/* possible x-resolutions */
  {1200, 600, 300, 150, 75, 50, 0},	/* possible y-resolutions */
  {12, 8, 0},			/* possible depths in gray mode */
  {12, 8, 0},			/* possible depths in color mode */

  SANE_FIX (0.0),		/* Start of scan area in mm  (x) */
  SANE_FIX (15.5),		/* Start of scan area in mm (y) */
  SANE_FIX (217.0),		/* Size of scan area in mm (x) */
  SANE_FIX (299.0),		/* Size of scan area in mm (y) */

  SANE_FIX (6.5),		/* Start of white strip in mm (y) */
  SANE_FIX (0.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  0, 0, 0,			/* RGB CCD Line-distance correction in pixel */
  0,				/* CCD distcance for CCD with 6 lines) */

  COLOR_ORDER_BGR,		/* Order of the CCD/CIS colors */
  {0x0f, 0x01, 0x15, 0x06, 0x13, 0x04},	/* Default offset/gain */
  {0x157, 0x157, 0x157},	/* Default exposure parameters */
  SANE_FIX (2.0),		/* Default gamma value */

  SANE_TRUE,			/* Is this a CIS scanner? */
  0				/* Which flags are needed for this scanner? */
    /* Setup and tested */
};

static GT68xx_Model artec_ultima2000_model = {
  "artec-ultima-2000",		/* Name */
  "Artec",			/* Device vendor string */
  "Ultima 2000",		/* Device model name */
  "Gt680xfw.usb",		/* Name of the firmware file */
  SANE_FALSE,			/* Dynamic allocation flag */

  &mustek_gt6801_command_set,	/* Command set used by this scanner */

  600,				/* maximum optical sensor resolution */
  600,				/* maximum motor resolution */
  600,				/* base x-res used to calculate geometry */
  600,				/* base y-res used to calculate geometry */
  600,				/* if ydpi is equal or higher, use linemode */
  SANE_TRUE,			/* Use base_ydpi for all resolutions */

  {600, 300, 200, 150, 100, 75, 50, 0},	/* possible x-resolutions */
  {600, 300, 200, 150, 100, 75, 50, 0},	/* possible y-resolutions */
  {12, 8, 0},			/* possible depths in gray mode */
  {12, 8, 0},			/* possible depths in color mode */

  SANE_FIX (2.0),		/* Start of scan area in mm  (x) */
  SANE_FIX (7.0),		/* Start of scan area in mm (y) */
  SANE_FIX (218.0),		/* Size of scan area in mm (x) */
  SANE_FIX (299.0),		/* Size of scan area in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in mm (y) */
  SANE_FIX (0.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  0, 0, 0,			/* RGB CCD Line-distance correction in pixel */
  0,				/* CCD distcance for CCD with 6 lines) */

  COLOR_ORDER_BGR,		/* Order of the CCD/CIS colors */
  {0x0f, 0x01, 0x15, 0x06, 0x13, 0x04},	/* Default offset/gain */
  {0x157, 0x157, 0x157},	/* Default exposure parameters */
  SANE_FIX (2.0),		/* Default gamma value */

  SANE_TRUE,			/* Is this a CIS scanner? */
  GT68XX_FLAG_MIRROR_X | GT68XX_FLAG_MOTOR_HOME | GT68XX_FLAG_OFFSET_INV	/* Which flags are needed for this scanner? */
    /* Setup for Cytron TCM MD 9385 */
};

static GT68xx_Model mustek_2400cu_model = {
  "mustek-bearpaw-2400-cu",	/* Name */
  "Mustek",			/* Device vendor string */
  "BearPaw 2400 CU",		/* Device model name */
  "PS2fw.usb",			/* Name of the firmware file */
  SANE_FALSE,			/* Dynamic allocation flag */

  &mustek_gt6801_command_set,	/* Command set used by this scanner */

  1200,				/* maximum optical sensor resolution */
  2400,				/* maximum motor resolution */
  1200,				/* base x-res used to calculate geometry */
  1200,				/* base y-res used to calculate geometry */
  2400,				/* if ydpi is equal or higher, use linemode */
  SANE_TRUE,			/* Use base_ydpi for all resolutions */

  {1200, 600, 300, 150, 100, 50, 0},	/* possible x-resolutions */
  {2400, 1200, 600, 300, 150, 100, 50, 0},	/* possible y-resolutions */
  {12, 8, 0},			/* possible depths in gray mode */
  {12, 8, 0},			/* possible depths in color mode */

  SANE_FIX (0.0),		/* Start of scan area in mm  (x) */
  SANE_FIX (13.5),		/* Start of scan area in mm (y) */
  SANE_FIX (215.0),		/* Size of scan area in mm (x) */
  SANE_FIX (296.9),		/* Size of scan area in mm (y) */

  SANE_FIX (5.5),		/* Start of white strip in mm (y) */
  SANE_FIX (0.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.9),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  0, 0, 0,			/* RGB CCD Line-distance correction in pixel */
  0,				/* CCD distcance for CCD with 6 lines) */

  COLOR_ORDER_BGR,		/* Order of the CCD/CIS colors */
  {0x0f, 0x01, 0x15, 0x06, 0x13, 0x04},	/* Default offset/gain */
  {0x157, 0x157, 0x157},	/* Default exposure parameters */
  SANE_FIX (2.0),		/* Default gamma value */

  SANE_TRUE,			/* Is this a CIS scanner? */
  0				/* Which flags are needed for this scanner? */
    /* basically tested, details may need tweaking */
};

static GT68xx_Model mustek_scanexpress2400usb_model = {
  "mustek-scanexpress-2400-usb",	/* Name */
  "Mustek",			/* Device vendor string */
  "ScanExpress 2400 USB",		/* Device model name */
  "P9fw.usb",			/* Name of the firmware file */
  SANE_FALSE,			/* Dynamic allocation flag */

  &mustek_gt6801_command_set,	/* Command set used by this scanner */

  1200,				/* maximum optical sensor resolution */
  1200,				/* maximum motor resolution */
  1200,				/* base x-res used to calculate geometry */
  1200,				/* base y-res used to calculate geometry */
  1200,				/* if ydpi is equal or higher, use linemode */
  SANE_TRUE,			/* Use base_ydpi for all resolutions */

  {600, 300, 150, 100, 50, 0},	/* possible x-resolutions */
  {1200, 600, 300, 150, 100, 50, 0},	/* possible y-resolutions */
  {12, 8, 0},			/* possible depths in gray mode */
  {12, 8, 0},			/* possible depths in color mode */

  SANE_FIX (5.0),		/* Start of scan area in mm  (x) */
  SANE_FIX (12.0),		/* Start of scan area in mm (y) */
  SANE_FIX (224.0),		/* Size of scan area in mm (x) */
  SANE_FIX (300.0),		/* Size of scan area in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in mm (y) */
  SANE_FIX (7.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  24, 12, 0,			/* RGB CCD Line-distance correction in pixel */
  0,				/* CCD distcance for CCD with 6 lines) */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */
  {0x12, 0x06, 0x0e, 0x03, 0x19, 0x25},	/* Default offset/gain */
  {0x157, 0x157, 0x157},	/* Default exposure parameters */
  SANE_FIX (2.0),		/* Default gamma value */

  SANE_FALSE,			/* Is this a CIS scanner? */
  GT68XX_FLAG_UNTESTED | GT68XX_FLAG_SE_2400	/* Which flags are needed for this scanner? */
    /* only partly tested, from "Fan Dan" <dan_fancn@hotmail.com> */
};

static GT68xx_Model mustek_a3usb_model = {
  "mustek-scanexpress-a3-usb",	/* Name */
  "Mustek",			/* Device vendor string */
  "ScanExpress A3 USB",		/* Device model name */
  "A32fw.usb",			/* Name of the firmware file */
  SANE_FALSE,			/* Dynamic allocation flag */

  &mustek_gt6816_command_set,	/* Command set used by this scanner */

  300,				/* maximum optical sensor resolution */
  600,				/* maximum motor resolution */
  300,				/* base x-res used to calculate geometry */
  300,				/* base y-res used to calculate geometry */
  300,				/* if ydpi is equal or higher, use linemode */
  SANE_FALSE,			/* Use base_ydpi for all resolutions */

  {300, 150, 75, 50, 0},	/* possible x-resolutions */
  {600, 300, 150, 75, 50, 0},	/* possible y-resolutions */
  {12, 8, 0},			/* possible depths in gray mode */
  {12, 8, 0},			/* possible depths in color mode */

  SANE_FIX (6.0),		/* Start of scan area in mm  (x) */
  SANE_FIX (14.0),		/* Start of scan area in mm (y) */
  SANE_FIX (297.0),		/* Size of scan area in mm (x) */
  SANE_FIX (431.0),		/* Size of scan area in mm (y) */

  SANE_FIX (5.0),		/* Start of white strip in mm (y) */
  SANE_FIX (0.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  0, 0, 0,			/* RGB CCD Line-distance correction in pixel */
  0,				/* CCD distcance for CCD with 6 lines) */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */
  {0x14, 0x07, 0x14, 0x07, 0x14, 0x07},	/* Default offset/gain */
  {0x157, 0x157, 0x157},	/* Default exposure parameters */
  SANE_FIX (2.0),		/* Default gamma value */

  SANE_TRUE,			/* Is this a CIS scanner? */
  0				/* Which flags are needed for this scanner? */
    /* Tested by Pedro Morais <morais@inocam.com> */
};

static GT68xx_Model lexmark_x73_model = {
  "lexmark-x73",		/* Name */
  "Lexmark",			/* Device vendor string */
  "X73",			/* Device model name */
  "OSLO3071b2.usb",		/* Name of the firmware file */
  SANE_FALSE,			/* Dynamic allocation flag */

  &mustek_gt6816_command_set,	/* Command set used by this scanner */

  600,				/* maximum optical sensor resolution */
  1200,				/* maximum motor resolution */
  600,				/* base x-res used to calculate geometry */
  600,				/* base y-res used to calculate geometry */
  1200,				/* if ydpi is equal or higher, use linemode */
  SANE_TRUE,			/* Use base_ydpi for all resolutions */

  {600, 300, 150, 75, 0},	/* possible x-resolutions */
  {600, 300, 150, 75, 0},	/* possible y-resolutions */
  {16, 12, 8, 0},		/* possible depths in gray mode */
  {16, 12, 8, 0},		/* possible depths in color mode */

  SANE_FIX (6.519),		/* Start of scan area in mm  (x) */
  SANE_FIX (12.615),		/* Start of scan area in mm (y) */
  SANE_FIX (220.0),		/* Size of scan area in mm (x) */
  SANE_FIX (297.0),		/* Size of scan area in mm (y) */

  SANE_FIX (1.0),		/* Start of white strip in mm (y) */
  SANE_FIX (0.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  32, 16, 0,			/* RGB CCD Line-distance correction in pixel */
  0,				/* CCD distcance for CCD with 6 lines) */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */
  {0x14, 0x07, 0x14, 0x07, 0x14, 0x07},	/* Default offset/gain */
  {0x157, 0x157, 0x157},	/* Default exposure parameters */
  SANE_FIX (2.0),		/* Default gamma value */

  SANE_FALSE,			/* Is this a CIS scanner? */
  0				/* Which flags are needed for this scanner? */
    /* When using automatic gain pictures are too dark. Only some ad hoc tests for
       lexmark x70 were done so far. WARNING: Don't use the Full scan option
       with the above settings, otherwise the sensor may bump at the end of
       the sledge and the scanner may be damaged!  */
};

static GT68xx_Model plustek_op1248u_model = {
  "plustek-op1248u",		/* Name */
  "Plustek",			/* Device vendor string */
  "OpticPro 1248U",		/* Device model name */
  "ccd548.fw",			/* Name of the firmware file */
  SANE_FALSE,			/* Dynamic allocation flag */

  &plustek_gt6801_command_set,	/* Command set used by this scanner */

  600,				/* maximum optical sensor resolution */
  600,				/* maximum motor resolution */
  600,				/* base x-res used to calculate geometry */
  600,				/* base y-res used to calculate geometry */
  50,				/* if ydpi is equal or higher, use linemode */
  SANE_TRUE,			/* Use base_ydpi for all resolutions */

  {600, 300, 150, 75, 0},	/* possible x-resolutions */
  {600, 300, 150, 75, 0},	/* possible y-resolutions */
  {12, 8, 0},			/* possible depths in gray mode */
  {12, 8, 0},			/* possible depths in color mode */

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
  0,				/* CCD distcance for CCD with 6 lines) */

  COLOR_ORDER_BGR,		/* Order of the CCD/CIS colors */
  {0x1c, 0x29, 0x1c, 0x2c, 0x1c, 0x2b},	/* Default offset/gain */
  {0x157, 0x157, 0x157},	/* Default exposure parameters */
  SANE_FIX (2.0),		/* Default gamma value */

  SANE_FALSE,			/* Is this a CIS scanner? */
  GT68XX_FLAG_OFFSET_INV	/* Which flags are needed for this scanner? */
  /* tested */
};

static GT68xx_Model genius_vivid3x_model  = {
  "genius-colorpage-vivid3x",	/* Name */
  "Genius",			/* Device vendor string */
  "Colorpage Vivid3x",		/* Device model name */
  "ccd548.fw",			/* Name of the firmware file */
  SANE_FALSE,			/* Dynamic allocation flag */

  &plustek_gt6801_command_set,	/* Command set used by this scanner */

  600,				/* maximum optical sensor resolution */
  600,				/* maximum motor resolution */
  600,				/* base x-res used to calculate geometry */
  600,				/* base y-res used to calculate geometry */
  50,				/* if ydpi is equal or higher, use linemode */
  SANE_TRUE,			/* Use base_ydpi for all resolutions */

  {600, 300, 150, 75, 0},	/* possible x-resolutions */
  {600, 300, 150, 75, 0},	/* possible y-resolutions */
  {12, 8, 0},			/* possible depths in gray mode */
  {12, 8, 0},			/* possible depths in color mode */

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
  0,				/* CCD distcance for CCD with 6 lines) */

  COLOR_ORDER_BGR,		/* Order of the CCD/CIS colors */
  {0x1c, 0x29, 0x1c, 0x2c, 0x1c, 0x2b},	/* Default offset/gain */
  {0x157, 0x157, 0x157},	/* Default exposure parameters */
  SANE_FIX (2.0),		/* Default gamma value */

  SANE_FALSE,			/* Is this a CIS scanner? */
  GT68XX_FLAG_OFFSET_INV |  GT68XX_FLAG_UNTESTED /* Which flags are needed for this scanner? */
  /* completely untested, based on the Plustek OpticPro 1248U*/
};

static GT68xx_Model genius_vivid3xe_model  = {
  "genius-colorpage-vivid3xe",	/* Name */
  "Genius",			/* Device vendor string */
  "Colorpage Vivid3xe",		/* Device model name */
  "ccd548.fw",			/* Name of the firmware file */
  SANE_FALSE,			/* Dynamic allocation flag */

  &plustek_gt6801_command_set,	/* Command set used by this scanner */

  600,				/* maximum optical sensor resolution */
  600,				/* maximum motor resolution */
  600,				/* base x-res used to calculate geometry */
  600,				/* base y-res used to calculate geometry */
  50,				/* if ydpi is equal or higher, use linemode */
  SANE_TRUE,			/* Use base_ydpi for all resolutions */

  {600, 300, 150, 75, 0},	/* possible x-resolutions */
  {600, 300, 150, 75, 0},	/* possible y-resolutions */
  {12, 8, 0},			/* possible depths in gray mode */
  {12, 8, 0},			/* possible depths in color mode */

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
  0,				/* CCD distcance for CCD with 6 lines) */

  COLOR_ORDER_BGR,		/* Order of the CCD/CIS colors */
  {0x1c, 0x29, 0x1c, 0x2c, 0x1c, 0x2b},	/* Default offset/gain */
  {0x157, 0x157, 0x157},	/* Default exposure parameters */
  SANE_FIX (2.0),		/* Default gamma value */

  SANE_FALSE,			/* Is this a CIS scanner? */
  GT68XX_FLAG_OFFSET_INV /* Which flags are needed for this scanner? */
  /* mostly untested, based on the Genius Vivid3x */
};

static GT68xx_USB_Device_Entry gt68xx_usb_device_list[] = {
  {0x055f, 0x0218, &mustek_2400ta_model},
  {0x055f, 0x0219, &mustek_2400taplus_model},
  {0x055f, 0x021c, &mustek_1200cuplus_model},
  {0x055f, 0x021d, &mustek_2400cuplus_model},
  {0x055f, 0x021e, &mustek_1200ta_model},
  {0x05d8, 0x4002, &mustek_1200cu_model},
  {0x05d8, 0x4002, &mustek_scanexpress1200ubplus_model},	/* manual override */
  {0x05d8, 0x4002, &artec_ultima2000_model},	/* manual override */
  {0x05d8, 0x4002, &mustek_2400cu_model},	/* manual override */
  {0x05d8, 0x4002, &mustek_scanexpress2400usb_model}, /* manual override */
  {0x055f, 0x0210, &mustek_a3usb_model},
  {0x043d, 0x002d, &lexmark_x73_model},
  {0x07b3, 0x0400, &plustek_op1248u_model},
  {0x07b3, 0x0401, &plustek_op1248u_model}, /* Same scanner, different id? */
  {0x0458, 0x2011, &genius_vivid3x_model},
  {0x0458, 0x2017, &genius_vivid3xe_model},
  {0, 0, NULL}
};
