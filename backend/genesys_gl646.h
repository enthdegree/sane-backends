/* sane - Scanner Access Now Easy.

   Copyright (C) 2003-2004 Henning Meier-Geinitz <henning@meier-geinitz.de>
   Copyright (C) 2004-2005 Gerhard Jaeger <gerhard@gjaeger.de>
   Copyright (C) 2004-2013 Stéphane Voltz <stef.dev@free.fr>
   Copyright (C) 2005-2009 Pierre Willenbrock <pierre@pirsoft.dnsalias.org>

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

#ifndef BACKEND_GENESYS_GL646_H
#define BACKEND_GENESYS_GL646_H

#include "genesys.h"

static void gl646_set_fe(Genesys_Device* dev, const Genesys_Sensor& sensor, uint8_t set, int dpi);

static void gl646_public_set_fe(Genesys_Device* dev, const Genesys_Sensor& sensor, uint8_t set);

static void gl646_save_power(Genesys_Device* dev, SANE_Bool enable);

static void gl646_slow_back_home(Genesys_Device* dev, SANE_Bool wait_until_home);

static void gl646_move_to_ta(Genesys_Device* dev);

/**
 * sets up the scanner for a scan, registers, gamma tables, shading tables
 * and slope tables, based on the parameter struct.
 * @param dev         device to set up
 * @param regs        registers to set up
 * @param settings    settings of the scan
 * @param split       true if move before scan has to be done
 * @param xcorrection true if scanner's X geometry must be taken into account to
 * 		     compute X, ie add left margins
 * @param ycorrection true if scanner's Y geometry must be taken into account to
 * 		     compute Y, ie add top margins
 */
static void setup_for_scan(Genesys_Device* device,
                           const Genesys_Sensor& sensor,
                           Genesys_Register_Set*regs,
                           Genesys_Settings settings,
                           SANE_Bool split,
                           SANE_Bool xcorrection,
                           SANE_Bool ycorrection);

/**
 * Does a simple move of the given distance by doing a scan at lowest resolution
 * shading correction. Memory for data is allocated in this function
 * and must be freed by caller.
 * @param dev device of the scanner
 * @param distance distance to move in MM
 */
static void simple_move(Genesys_Device* dev, SANE_Int distance);

/**
 * Does a simple scan of the area given by the settings. Scanned data
 * it put in an allocated area which must be freed by the caller.
 * and slope tables, based on the parameter struct. There is no shading
 * correction while gamma correction is active.
 * @param dev      device to set up
 * @param settings settings of the scan
 * @param move     flag to enable scanhead to move
 * @param forward  flag to tell movement direction
 * @param shading  flag to tell if shading correction should be done
 * @param data     pointer that will point to the scanned data
 */
static void simple_scan(Genesys_Device* dev, const Genesys_Sensor& sensor,
                        Genesys_Settings settings, SANE_Bool move, SANE_Bool forward,
                        SANE_Bool shading, std::vector<uint8_t>& data);

/**
 * Send the stop scan command
 * */
static void end_scan(Genesys_Device* dev, Genesys_Register_Set* reg, SANE_Bool check_stop,
                     SANE_Bool eject);
/**
 * writes control data to an area behind the last motor table.
 */
static void write_control(Genesys_Device* dev, const Genesys_Sensor& sensor, int resolution);


/**
 * initialize scanner's registers at SANE init time
 */
static void gl646_init_regs (Genesys_Device * dev);

static void gl646_load_document(Genesys_Device* dev);

static void gl646_detect_document_end(Genesys_Device* dev);

#define FULL_STEP   0
#define HALF_STEP   1
#define QUATER_STEP 2

#define CALIBRATION_LINES 10

/**
 * master motor settings table entry
 */
typedef struct
{
  /* key */
  SANE_Int motor;
    unsigned dpi;
  unsigned channels;

  /* settings */
  SANE_Int steptype;		/* 0=full, 1=half, 2=quarter */
  SANE_Bool fastmod;		/* fast scanning 0/1 */
  SANE_Bool fastfed;		/* fast fed slope tables */
  SANE_Int mtrpwm;
  SANE_Int steps1;		/* table 1 informations */
  SANE_Int vstart1;
  SANE_Int vend1;
  SANE_Int steps2;		/* table 2 informations */
  SANE_Int vstart2;
  SANE_Int vend2;
  float g1;
  float g2;
  SANE_Int fwdbwd;		/* forward/backward steps */
} Motor_Master;

/**
 * master motor settings, for a given motor and dpi,
 * it gives steps and speed informations
 */
static Motor_Master motor_master[] = {
  /* HP3670 motor settings */
  {MOTOR_HP3670,  75, 3, FULL_STEP, SANE_FALSE, SANE_TRUE , 1, 200,  3429,  305, 192, 3399, 337, 0.3, 0.4, 192},
  {MOTOR_HP3670, 100, 3, HALF_STEP, SANE_FALSE, SANE_TRUE , 1, 143,  2905,  187, 192, 3399, 337, 0.3, 0.4, 192},
  {MOTOR_HP3670, 150, 3, HALF_STEP, SANE_FALSE, SANE_TRUE , 1,  73,  3429,  305, 192, 3399, 337, 0.3, 0.4, 192},
  {MOTOR_HP3670, 300, 3, HALF_STEP, SANE_FALSE, SANE_TRUE , 1,  11,  1055,  563, 192, 3399, 337, 0.3, 0.4, 192},
  {MOTOR_HP3670, 600, 3, FULL_STEP, SANE_FALSE, SANE_TRUE , 0,   3, 10687, 5126, 192, 3399, 337, 0.3, 0.4, 192},
  {MOTOR_HP3670,1200, 3, HALF_STEP, SANE_FALSE, SANE_TRUE , 0,   3, 15937, 6375, 192, 3399, 337, 0.3, 0.4, 192},
  {MOTOR_HP3670,2400, 3, HALF_STEP, SANE_FALSE, SANE_TRUE , 0,   3, 15937, 12750, 192, 3399, 337, 0.3, 0.4, 192},
  {MOTOR_HP3670,  75, 1, FULL_STEP, SANE_FALSE, SANE_TRUE , 1, 200,  3429,  305, 192, 3399, 337, 0.3, 0.4, 192},
  {MOTOR_HP3670, 100, 1, HALF_STEP, SANE_FALSE, SANE_TRUE , 1, 143,  2905,  187, 192, 3399, 337, 0.3, 0.4, 192},
  {MOTOR_HP3670, 150, 1, HALF_STEP, SANE_FALSE, SANE_TRUE , 1,  73,  3429,  305, 192, 3399, 337, 0.3, 0.4, 192},
  {MOTOR_HP3670, 300, 1, HALF_STEP, SANE_FALSE, SANE_TRUE , 1,  11,  1055,  563, 192, 3399, 337, 0.3, 0.4, 192},
  {MOTOR_HP3670, 600, 1, FULL_STEP, SANE_FALSE, SANE_TRUE , 0,   3, 10687, 5126, 192, 3399, 337, 0.3, 0.4, 192},
  {MOTOR_HP3670,1200, 1, HALF_STEP, SANE_FALSE, SANE_TRUE , 0,   3, 15937, 6375, 192, 3399, 337, 0.3, 0.4, 192},
  {MOTOR_HP3670,2400, 3, HALF_STEP, SANE_FALSE, SANE_TRUE , 0,   3, 15937, 12750, 192, 3399, 337, 0.3, 0.4, 192},

  /* HP2400/G2410 motor settings base motor dpi = 600 */
  {MOTOR_HP2400,  50, 3, FULL_STEP, SANE_FALSE, SANE_TRUE , 63, 120, 8736,   601, 192, 4905,  337, 0.30, 0.4, 192},
  {MOTOR_HP2400, 100, 3, HALF_STEP, SANE_FALSE, SANE_TRUE,  63, 120, 8736,   601, 192, 4905,  337, 0.30, 0.4, 192},
  {MOTOR_HP2400, 150, 3, HALF_STEP, SANE_FALSE, SANE_TRUE , 63, 67, 15902,   902, 192, 4905,  337, 0.30, 0.4, 192},
  {MOTOR_HP2400, 300, 3, HALF_STEP, SANE_FALSE, SANE_TRUE , 63, 32, 16703,  2188, 192, 4905,  337, 0.30, 0.4, 192},
  {MOTOR_HP2400, 600, 3, FULL_STEP, SANE_FALSE, SANE_TRUE , 63,  3, 18761, 18761, 192, 4905,  627, 0.30, 0.4, 192},
  {MOTOR_HP2400,1200, 3, HALF_STEP, SANE_FALSE, SANE_TRUE , 63,  3, 43501, 43501, 192, 4905,  627, 0.30, 0.4, 192},
  {MOTOR_HP2400,  50, 1, FULL_STEP, SANE_FALSE, SANE_TRUE , 63, 120, 8736,   601, 192, 4905,  337, 0.30, 0.4, 192},
  {MOTOR_HP2400, 100, 1, HALF_STEP, SANE_FALSE, SANE_TRUE,  63, 120, 8736,   601, 192, 4905,  337, 0.30, 0.4, 192},
  {MOTOR_HP2400, 150, 1, HALF_STEP, SANE_FALSE, SANE_TRUE , 63, 67, 15902,   902, 192, 4905,  337, 0.30, 0.4, 192},
  {MOTOR_HP2400, 300, 1, HALF_STEP, SANE_FALSE, SANE_TRUE , 63, 32, 16703,  2188, 192, 4905,  337, 0.30, 0.4, 192},
  {MOTOR_HP2400, 600, 1, FULL_STEP, SANE_FALSE, SANE_TRUE , 63,  3, 18761, 18761, 192, 4905,  337, 0.30, 0.4, 192},
  {MOTOR_HP2400,1200, 1, HALF_STEP, SANE_FALSE, SANE_TRUE , 63,  3, 43501, 43501, 192, 4905,  337, 0.30, 0.4, 192},

  /* XP 200 motor settings */
  {MOTOR_XP200,  75, 3, HALF_STEP, SANE_TRUE , SANE_FALSE, 0, 4,  6000,  2136, 8, 12000, 1200, 0.3, 0.5, 1},
  {MOTOR_XP200, 100, 3, HALF_STEP, SANE_TRUE , SANE_FALSE, 0, 4,  6000,  2850, 8, 12000, 1200, 0.3, 0.5, 1},
  {MOTOR_XP200, 200, 3, HALF_STEP, SANE_TRUE , SANE_FALSE, 0, 4,  6999,  5700, 8, 12000, 1200, 0.3, 0.5, 1},
  {MOTOR_XP200, 250, 3, HALF_STEP, SANE_TRUE , SANE_FALSE, 0, 4,  6999,  6999, 8, 12000, 1200, 0.3, 0.5, 1},
  {MOTOR_XP200, 300, 3, HALF_STEP, SANE_TRUE , SANE_FALSE, 0, 4, 13500, 13500, 8, 12000, 1200, 0.3, 0.5, 1},
  {MOTOR_XP200, 600, 3, HALF_STEP, SANE_TRUE ,  SANE_TRUE, 0, 4, 31998, 31998, 2, 12000, 1200, 0.3, 0.5, 1},
  {MOTOR_XP200,  75, 1, HALF_STEP, SANE_TRUE , SANE_FALSE, 0, 4,  6000,  2000, 8, 12000, 1200, 0.3, 0.5, 1},
  {MOTOR_XP200, 100, 1, HALF_STEP, SANE_TRUE , SANE_FALSE, 0, 4,  6000,  1300, 8, 12000, 1200, 0.3, 0.5, 1},
  {MOTOR_XP200, 200, 1, HALF_STEP, SANE_TRUE ,  SANE_TRUE, 0, 4,  6000,  3666, 8, 12000, 1200, 0.3, 0.5, 1},
  {MOTOR_XP200, 300, 1, HALF_STEP, SANE_TRUE , SANE_FALSE, 0, 4,  6500,  6500, 8, 12000, 1200, 0.3, 0.5, 1},
  {MOTOR_XP200, 600, 1, HALF_STEP, SANE_TRUE ,  SANE_TRUE, 0, 4, 24000, 24000, 2, 12000, 1200, 0.3, 0.5, 1},

  /* HP scanjet 2300c */
  {MOTOR_HP2300,  75, 3, FULL_STEP, SANE_FALSE, SANE_TRUE , 63, 120,  8139,   560, 120, 4905,  337, 0.3, 0.4, 16},
  {MOTOR_HP2300, 150, 3, HALF_STEP, SANE_FALSE, SANE_TRUE , 63,  67,  7903,   543, 120, 4905,  337, 0.3, 0.4, 16},
  {MOTOR_HP2300, 300, 3, HALF_STEP, SANE_FALSE, SANE_TRUE , 63,   3,  2175,  1087, 120, 4905,  337, 0.3, 0.4, 16},
  {MOTOR_HP2300, 600, 3, HALF_STEP, SANE_FALSE, SANE_TRUE , 63,   3,  8700,  4350, 120, 4905,  337, 0.3, 0.4, 16},
  {MOTOR_HP2300,1200, 3, HALF_STEP, SANE_FALSE, SANE_TRUE , 63,   3, 17400,  8700, 120, 4905,  337, 0.3, 0.4, 16},
  {MOTOR_HP2300,  75, 1, FULL_STEP, SANE_FALSE, SANE_TRUE , 63, 120,  8139,   560, 120, 4905,  337, 0.3, 0.4, 16},
  {MOTOR_HP2300, 150, 1, HALF_STEP, SANE_FALSE, SANE_TRUE , 63,  67,  7903,   543, 120, 4905,  337, 0.3, 0.4, 16},
  {MOTOR_HP2300, 300, 1, HALF_STEP, SANE_FALSE, SANE_TRUE , 63,   3,  2175,  1087, 120, 4905,  337, 0.3, 0.4, 16},
  {MOTOR_HP2300, 600, 1, HALF_STEP, SANE_FALSE, SANE_TRUE , 63,   3,  8700,  4350, 120, 4905,  337, 0.3, 0.4, 16},
  {MOTOR_HP2300,1200, 1, HALF_STEP, SANE_FALSE, SANE_TRUE , 63,   3, 17400,  8700, 120, 4905,  337, 0.3, 0.4, 16},
  /* non half ccd settings for 300 dpi
  {MOTOR_HP2300, 300, 3, HALF_STEP, SANE_FALSE, SANE_TRUE , 63,  44,  5386,  2175, 120, 4905,  337, 0.3, 0.4, 16},
  {MOTOR_HP2300, 300, 1, HALF_STEP, SANE_FALSE, SANE_TRUE , 63,  44,  5386,  2175, 120, 4905,  337, 0.3, 0.4, 16},
  */

  /* MD5345/6471 motor settings */
  /* vfinal=(exposure/(1200/dpi))/step_type */
  {MOTOR_5345,    50, 3, HALF_STEP  , SANE_FALSE, SANE_TRUE , 2, 255,  2500,   250, 255, 2000,  300, 0.3, 0.4, 64},
  {MOTOR_5345,    75, 3, HALF_STEP  , SANE_FALSE, SANE_TRUE , 2, 255,  2500,   343, 255, 2000,  300, 0.3, 0.4, 64},
  {MOTOR_5345,   100, 3, HALF_STEP  , SANE_FALSE, SANE_TRUE , 2, 255,  2500,   458, 255, 2000,  300, 0.3, 0.4, 64},
  {MOTOR_5345,   150, 3, HALF_STEP  , SANE_FALSE, SANE_TRUE , 2, 255,  2500,   687, 255, 2000,  300, 0.3, 0.4, 64},
  {MOTOR_5345,   200, 3, HALF_STEP  , SANE_FALSE, SANE_TRUE , 2, 255,  2500,   916, 255, 2000,  300, 0.3, 0.4, 64},
  {MOTOR_5345,   300, 3, HALF_STEP  , SANE_FALSE, SANE_TRUE , 2, 255,  2500,  1375, 255, 2000,  300, 0.3, 0.4, 64},
  {MOTOR_5345,   400, 3, HALF_STEP  , SANE_FALSE, SANE_TRUE , 0,  32,  2000,  1833, 255, 2000,  300, 0.3, 0.4, 32},
  {MOTOR_5345,   500, 3, HALF_STEP  , SANE_FALSE, SANE_TRUE , 0,  32,  2291,  2291, 255, 2000,  300, 0.3, 0.4, 32},
  {MOTOR_5345,   600, 3, HALF_STEP  , SANE_FALSE, SANE_TRUE , 0,  32,  2750,  2750, 255, 2000,  300, 0.3, 0.4, 32},
  {MOTOR_5345,  1200, 3, QUATER_STEP, SANE_FALSE, SANE_TRUE , 0,  16,  2750,  2750, 255, 2000,  300, 0.3, 0.4, 146},
  {MOTOR_5345,  2400, 3, QUATER_STEP, SANE_FALSE, SANE_TRUE , 0,  16,  5500,  5500, 255, 2000,  300, 0.3, 0.4, 146},
  {MOTOR_5345,    50, 1, HALF_STEP  , SANE_FALSE, SANE_TRUE , 2, 255,  2500,   250, 255, 2000,  300, 0.3, 0.4, 64},
  {MOTOR_5345,    75, 1, HALF_STEP  , SANE_FALSE, SANE_TRUE , 2, 255,  2500,   343, 255, 2000,  300, 0.3, 0.4, 64},
  {MOTOR_5345,   100, 1, HALF_STEP  , SANE_FALSE, SANE_TRUE , 2, 255,  2500,   458, 255, 2000,  300, 0.3, 0.4, 64},
  {MOTOR_5345,   150, 1, HALF_STEP  , SANE_FALSE, SANE_TRUE , 2, 255,  2500,   687, 255, 2000,  300, 0.3, 0.4, 64},
  {MOTOR_5345,   200, 1, HALF_STEP  , SANE_FALSE, SANE_TRUE , 2, 255,  2500,   916, 255, 2000,  300, 0.3, 0.4, 64},
  {MOTOR_5345,   300, 1, HALF_STEP  , SANE_FALSE, SANE_TRUE , 2, 255,  2500,  1375, 255, 2000,  300, 0.3, 0.4, 64},
  {MOTOR_5345,   400, 1, HALF_STEP  , SANE_FALSE, SANE_TRUE , 0,  32,  2000,  1833, 255, 2000,  300, 0.3, 0.4, 32},
  {MOTOR_5345,   500, 1, HALF_STEP  , SANE_FALSE, SANE_TRUE , 0,  32,  2291,  2291, 255, 2000,  300, 0.3, 0.4, 32},
  {MOTOR_5345,   600, 1, HALF_STEP  , SANE_FALSE, SANE_TRUE , 0,  32,  2750,  2750, 255, 2000,  300, 0.3, 0.4, 32},
  {MOTOR_5345,  1200, 1, QUATER_STEP, SANE_FALSE, SANE_TRUE , 0,  16,  2750,  2750, 255, 2000,  300, 0.3, 0.4, 146},
  {MOTOR_5345,  2400, 1, QUATER_STEP, SANE_FALSE, SANE_TRUE , 0,  16,  5500,  5500, 255, 2000,  300, 0.3, 0.4, 146}, /* 5500 guessed */
};

#endif // BACKEND_GENESYS_GL646_H
