/* sane - Scanner Access Now Easy.

   Copyright (C) 2002 Sergey Vlasov <vsu@altlinux.ru>
   Copyright (C) 2002 Henning Meier-Geinitz <henning@meier-geinitz.de>
   
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
  "mustek-gt6816",

  0x200c, 0x200b,
  0x2010, 0x3f40, 0x2011, 0x3f00,
  0x2012, 0x3f40, 0x2013, 0x3f00,

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

  0x200a, 0x2009,
  0x2010, 0x3f40, 0x2011, 0x3f00,
  0x2012, 0x3f40, 0x2013, 0x3f00,

  /* activate */ NULL,
  /* deactivate */ NULL,
  gt6801_check_firmware,
  gt6801_download_firmware,
  gt6801_get_power_status,
  /* get_ta_status (FIXME: implement this) */ NULL,
  gt6801_lamp_control,
  gt6801_is_moving,
  /*gt68xx_generic_move_relative *** to be tested */ NULL,
  gt6801_carriage_home,
  gt68xx_generic_start_scan,
  gt68xx_generic_read_scanned_data,
  gt6801_stop_scan,
  gt6801_setup_scan,
  gt68xx_generic_set_afe,
  gt68xx_generic_set_exposure_time,
  gt68xx_generic_get_id
};

static GT68xx_Model mustek_2400ta_model = {
  "mustek-bearpaw-2400-ta",
  "Mustek",
  "BearPaw 2400 TA",
  "A2fw.usb",
  SANE_FALSE,

  &mustek_gt6816_command_set,

  1200,
  2400,
  1200,				/* ??? */
  1200,
  1200,
  SANE_FALSE,
  {1200, 600, 300, 150, 100, 50, 0},
  {2400, 1200, 600, 300, 150, 100, 50, 0},
  {16, 12, 8, 0},
  {16, 12, 8, 0},
  SANE_FIX (3.67),
  SANE_FIX (7.4),
  SANE_FIX (219.0),
  SANE_FIX (298.0),
  SANE_FIX (0.0),
  SANE_FIX (0.635),

  SANE_FIX (94.0),
  SANE_FIX (107.0),
  SANE_FIX (37.0),
  SANE_FIX (165.0),
  SANE_FIX (95.0),

  0, 24, 48,			/* ??? */
  0,

  COLOR_ORDER_RGB,
  {0x2a, 0x0c, 0x2e, 0x06, 0x2d, 0x07},
  {0x157, 0x157, 0x157},
  SANE_FALSE,
  0
    /* flatbed values tested */
};

static GT68xx_Model mustek_2400taplus_model = {
  "mustek-bearpaw-2400-ta-plus",
  "Mustek",
  "BearPaw 2400 TA Plus",
  "A2Dfw.usb",
  SANE_FALSE,

  &mustek_gt6816_command_set,

  1200,
  2400,
  600,
  1200,
  1200,
  SANE_FALSE,
  {1200, 600, 300, 100, 50, 0},
  {2400, 1200, 600, 300, 100, 50, 0},
  {8, 0},
  {16, 12, 8, 0},
  SANE_FIX (7.41),
  SANE_FIX (7.4),
  SANE_FIX (217.5),
  SANE_FIX (298.0),
  SANE_FIX (0.0),
  SANE_FIX (5.0),

  SANE_FIX (94.0),
  SANE_FIX (107.0),
  SANE_FIX (37.0),
  SANE_FIX (165.0),
  SANE_FIX (95.0),

  0, 48, 96,
  8,

  COLOR_ORDER_RGB,
  {0x2a, 0x0c, 0x2e, 0x06, 0x2d, 0x07},
  {0x157, 0x157, 0x157},
  SANE_FALSE,
  0
    /* Setup and tested */
};


static GT68xx_Model mustek_1200ta_model = {
  "mustek-bearpaw-1200-ta",
  "Mustek",
  "BearPaw 1200 TA",
  "A1fw.usb",
  SANE_FALSE,

  &mustek_gt6816_command_set,

  600,
  1200,
  600,
  600,
  1200,
  SANE_TRUE,
  {600, 300, 150, 75, 50, 0},
  {1200, 600, 300, 150, 100, 50, 0},
  {16, 12, 8, 0},
  {16, 12, 8, 0},
  SANE_FIX (8.4),
  SANE_FIX (7.5),
  SANE_FIX (220.0),
  SANE_FIX (299.0),
  SANE_FIX (0.0),
  SANE_FIX (7.0),

  SANE_FIX (98.0),
  SANE_FIX (108.0),		/* 109 */
  SANE_FIX (37.0),
  SANE_FIX (163.0),
  SANE_FIX (95.0),

  32, 16, 0,			/* ??? */
  0,

  COLOR_ORDER_RGB,
  {0x2a, 0x0c, 0x2e, 0x06, 0x2d, 0x07},
  {0x157, 0x157, 0x157},
  SANE_FALSE,
  0
    /* Everything untested */
};

static GT68xx_Model mustek_1200cuplus_model = {
  "mustek-bearpaw-1200-cu-plus",
  "Mustek",
  "Bearpaw 1200 CU Plus",
  "PS1Dfw.usb",
  SANE_FALSE,

  &mustek_gt6816_command_set,

  600,
  1200,
  600,
  600,
  1200,
  SANE_FALSE,
  {600, 300, 150, 75, 50, 0},
  {1200, 600, 300, 150, 75, 50, 0},
  {16, 12, 8, 0},
  {16, 12, 8, 0},
  SANE_FIX (0.0),
  SANE_FIX (13.0),
  SANE_FIX (217.0),
  SANE_FIX (299.0),
  SANE_FIX (5.0),
  SANE_FIX (0.0),		/* no black mark */

  SANE_FIX (0.0),		/* no TA */
  SANE_FIX (0.0),
  SANE_FIX (100.0),
  SANE_FIX (100.0),
  SANE_FIX (0.0),

  0, 0, 0,
  0,

  COLOR_ORDER_BGR,
  {0x14, 0x07, 0x14, 0x07, 0x14, 0x07},
  {0x157, 0x157, 0x157},
  SANE_TRUE,
  0
    /* Everything untested */
};

static GT68xx_Model mustek_2400cuplus_model = {
  "mustek-bearpaw-2400-cu-plus",
  "Mustek",
  "BearPaw 2400 CU Plus",
  "PS2Dfw.usb",
  SANE_FALSE,

  &mustek_gt6816_command_set,

  1200,
  2400,
  1200,
  1200,
  1200,
  SANE_FALSE,
  {1200, 600, 300, 200, 150, 100, 50, 0},
  {2400, 1200, 600, 300, 200, 150, 100, 50, 0},
  {16, 12, 8, 0},
  {16, 12, 8, 0},
  SANE_FIX (2.0),
  SANE_FIX (13.0),
  SANE_FIX (217.0),
  SANE_FIX (300.0),
  SANE_FIX (5.0),
  SANE_FIX (0.0),		/* no black mark */

  SANE_FIX (0.0),		/* no TA */
  SANE_FIX (0.0),
  SANE_FIX (100.0),
  SANE_FIX (100.0),
  SANE_FIX (0.0),

  0, 0, 0,			/* no LD correction */
  0,

  COLOR_ORDER_BGR,
  {0x1a, 0x16, 0x15, 0x08, 0x0e, 0x02},
  {0x157, 0x157, 0x157},
  SANE_TRUE,
  0
    /* Setup and tested */
};

/* Seems that Mustek ScanExpress 1200 UB Plus, the Mustek BearPaw 1200 CU
 * and lots of other scanners have the same USB identifier.
 */
static GT68xx_Model mustek_1200cu_model = {
  "mustek-bearpaw-1200-cu",
  "Mustek",
  "BearPaw 1200 CU",
  "PS1fw.usb",
  SANE_FALSE,

  &mustek_gt6801_command_set,

  600,
  1200,
  600,
  600,
  1200,
  SANE_TRUE,
  {600, 300, 150, 75, 50, 0},
  {1200, 600, 300, 150, 75, 50, 0},
  {12, 8, 0},
  {12, 8, 0},
  SANE_FIX (0.0),
  SANE_FIX (13.0),
  SANE_FIX (217.0),
  SANE_FIX (299.0),
  SANE_FIX (3.5),
  SANE_FIX (0.0),		/* no black mark */

  SANE_FIX (0.0),		/* no TA */
  SANE_FIX (0.0),
  SANE_FIX (100.0),
  SANE_FIX (100.0),
  SANE_FIX (0.0),

  0, 0, 0,			/* no LD correction */
  0,

  COLOR_ORDER_BGR,
  {0x13, 0x04, 0x15, 0x06, 0x0f, 0x02},
  {0x157, 0x157, 0x157},
  SANE_TRUE,
  0
    /* Setup and tested */
};

static GT68xx_Model mustek_scanexpress1200ubplus_model = {
  "mustek-scanexpress-1200-ub-plus",
  "Mustek",
  "ScanExpress 1200 UB Plus",
  "SBfw.usb",
  SANE_FALSE,

  &mustek_gt6801_command_set,

  600,
  1200,
  600,
  600,
  1200,
  SANE_TRUE,
  {600, 300, 150, 75, 50, 0},
  {1200, 600, 300, 150, 75, 50, 0},
  {12, 8, 0},
  {12, 8, 0},
  SANE_FIX (0.0),
  SANE_FIX (15.5),
  SANE_FIX (217.0),
  SANE_FIX (299.0),
  SANE_FIX (6.5),
  SANE_FIX (0.0),		/* no black mark */

  SANE_FIX (0.0),		/* no TA */
  SANE_FIX (0.0),
  SANE_FIX (100.0),
  SANE_FIX (100.0),
  SANE_FIX (0.0),

  0, 0, 0,			/* no LD correction */
  0,

  COLOR_ORDER_BGR,
  {0x0f, 0x01, 0x15, 0x06, 0x13, 0x04},
  {0x157, 0x157, 0x157},
  SANE_TRUE,
  0
    /* Setup and tested */
};

static GT68xx_Model artec_ultima2000_model = {
  "artec-ultima-2000",
  "Artec",
  "Ultima 2000",
  "Gt680xfw.usb",
  SANE_FALSE,

  &mustek_gt6801_command_set,

  600,
  600,
  600,
  600,
  600,
  SANE_TRUE,
  {600, 300, 200, 150, 100, 75, 50, 0},
  {600, 300, 200, 150, 100, 75, 50, 0},
  {12, 8, 0},
  {12, 8, 0},
  SANE_FIX (2.0),
  SANE_FIX (7.0),
  SANE_FIX (218.0),
  SANE_FIX (299.0),
  SANE_FIX (0.0),
  SANE_FIX (0.0),		/* no black mark */
  SANE_FIX (0.0),		/* no TA */
  SANE_FIX (0.0),
  SANE_FIX (100.0),
  SANE_FIX (100.0),
  SANE_FIX (0.0),

  0, 0, 0,			/* no LD correction */
  0,

  COLOR_ORDER_BGR,
  {0x0f, 0x01, 0x15, 0x06, 0x13, 0x04},
  {0x157, 0x157, 0x157},
  SANE_TRUE,
  GT68XX_FLAG_MIRROR_X | GT68XX_FLAG_MOTOR_HOME | GT68XX_FLAG_OFFSET_INV
    /* Setup for Cytron TCM MD 9385 */
};

static GT68xx_Model mustek_2400cu_model = {
  "mustek-bearpaw-2400-cu",
  "Mustek",
  "BearPaw 2400 CU",
  "PS2fw.usb",
  SANE_FALSE,

  &mustek_gt6801_command_set,

  1200,
  2400,
  1200,
  1200,
  2400,
  SANE_TRUE,
  {1200, 600, 300, 150, 100, 50, 0},
  {2400, 1200, 600, 300, 150, 100, 50, 0},
  {12, 8, 0},
  {12, 8, 0},
  SANE_FIX (0.0),		/* 0 */
  SANE_FIX (13.5),		/* 1280, */
  SANE_FIX (215.9),		/* 10240, */
  SANE_FIX (296.7),		/* 28032, */
  SANE_FIX (5.5),
  SANE_FIX (0.0),		/* no black mark */

  SANE_FIX (0.0),		/* no TA */
  SANE_FIX (0.0),
  SANE_FIX (100.0),
  SANE_FIX (100.0),
  SANE_FIX (0.0),

  0, 0, 0,			/* no LD correction */
  0,

  COLOR_ORDER_BGR,
  {0x0f, 0x01, 0x15, 0x06, 0x13, 0x04},
  {0x157, 0x157, 0x157},
  SANE_TRUE,
  0
    /* basically tested, works up to 300 dpi (?) */
};

static GT68xx_Model mustek_a3usb_model = {
  "mustek-scanexpress-a3-usb",
  "Mustek",
  "ScanExpress A3 USB",
  "A32fw.usb",
  SANE_FALSE,

  &mustek_gt6816_command_set,

  300,
  600,
  300,
  300,
  300,
  SANE_FALSE,
  {300, 150, 95, 50, 0},
  {600, 300, 150, 75, 50, 0},
  {12, 8, 0},
  {12, 8, 0},
  SANE_FIX (2.0),
  SANE_FIX (13.0),
  SANE_FIX (300.0),
  SANE_FIX (420.0),
  SANE_FIX (5.0),
  SANE_FIX (0.0),		/* no black mark */

  SANE_FIX (0.0),		/* no TA */
  SANE_FIX (0.0),
  SANE_FIX (100.0),
  SANE_FIX (100.0),
  SANE_FIX (0.0),

  0, 0, 0,			/* no LD correction */
  0,

  COLOR_ORDER_BGR,
  {0x14, 0x07, 0x14, 0x07, 0x14, 0x07},
  {0x157, 0x157, 0x157},
  SANE_TRUE,
  0
    /* Completely untested */
};

static GT68xx_Model lexmark_x73_model = {
  "lexmark-x73",
  "Lexmark",
  "X73",
  "OSLO3071b2.usb",
  SANE_FALSE,

  &mustek_gt6816_command_set,

  600,
  1200,
  600,
  600,				/* ??? */
  1200,				/* ??? */
  SANE_TRUE,

  {600, 300, 150, 75, 50, 0},
  {1200, 600, 300, 150, 75, 50, 0},
  {16, 12, 8, 0},
  {16, 12, 8, 0},
  SANE_FIX (6.519),
  SANE_FIX (12.615),
  SANE_FIX (220.0),		/* physical windowsize: 220 mm  */
  SANE_FIX (300.0),		/* physical windowsize: 300 mm  */
  SANE_FIX (1.0),
  SANE_FIX (0.0),

  SANE_FIX (0.0),		/* no TA */
  SANE_FIX (0.0),
  SANE_FIX (100.0),
  SANE_FIX (100.0),
  SANE_FIX (0.0),		/* ld_shift */

  32, 16, 0,
  0,

  COLOR_ORDER_RGB,
  {0x14, 0x07, 0x14, 0x07, 0x14, 0x07},
  {0x157, 0x157, 0x157},
  SANE_FALSE,
  0
    /* 50 dpi and 1200 dpi scan does not work (for lexmark x70); when using
       automatic gain pictures are too dark. Only some ad hoc tests for
       lexmark x70 were done so far. WARNING: Don't use the Full scan option
       with the above settings, otherwise the sensor may bump at the end of
       the sledge and the scanner may be damaged!  */
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
  {0x055f, 0x0210, &mustek_a3usb_model},
  {0x043d, 0x002d, &lexmark_x73_model},
  {0, 0, NULL}
};

/* vim: set sw=2 cino=>2se-1sn-1s{s^-1st0(0u0 smarttab expandtab: */
