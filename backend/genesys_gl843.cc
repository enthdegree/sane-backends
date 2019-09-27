/* sane - Scanner Access Now Easy.

   Copyright (C) 2010-2013 Stéphane Voltz <stef.dev@free.fr>


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

#define DEBUG_DECLARE_ONLY

#include "genesys_gl843.h"

#include <string>
#include <vector>

/****************************************************************************
 Low level function
 ****************************************************************************/

/* ------------------------------------------------------------------------ */
/*                  Read and write RAM, registers and AFE                   */
/* ------------------------------------------------------------------------ */

// Set address for writing data
static void gl843_set_buffer_address(Genesys_Device* dev, uint32_t addr)
{
    DBG_HELPER_ARGS(dbg, "setting address to 0x%05x", addr & 0xffff);

    dev->write_register(0x5b, ((addr >> 8) & 0xff));
    dev->write_register(0x5c, (addr & 0xff));
}

/**
 * writes a block of data to RAM
 * @param dev USB device
 * @param addr RAM address to write to
 * @param size size of the chunk of data
 * @param data pointer to the data to write
 */
static void write_data(Genesys_Device* dev, uint32_t addr, uint32_t size, uint8_t* data)
{
    DBG_HELPER(dbg);

    gl843_set_buffer_address(dev, addr);

    // write actual data
    sanei_genesys_bulk_write_data(dev, 0x28, data, size);

    // set back address to 0
    gl843_set_buffer_address(dev, 0);
}

/****************************************************************************
 Mid level functions
 ****************************************************************************/

static SANE_Bool
gl843_get_fast_feed_bit (Genesys_Register_Set * regs)
{
  GenesysRegister *r = NULL;

  r = sanei_genesys_get_address (regs, REG02);
  if (r && (r->value & REG02_FASTFED))
    return SANE_TRUE;
  return SANE_FALSE;
}

static SANE_Bool
gl843_get_filter_bit (Genesys_Register_Set * regs)
{
  GenesysRegister *r = NULL;

  r = sanei_genesys_get_address (regs, REG04);
  if (r && (r->value & REG04_FILTER))
    return SANE_TRUE;
  return SANE_FALSE;
}

static SANE_Bool
gl843_get_lineart_bit (Genesys_Register_Set * regs)
{
  GenesysRegister *r = NULL;

  r = sanei_genesys_get_address (regs, REG04);
  if (r && (r->value & REG04_LINEART))
    return SANE_TRUE;
  return SANE_FALSE;
}

static SANE_Bool
gl843_get_bitset_bit (Genesys_Register_Set * regs)
{
  GenesysRegister *r = NULL;

  r = sanei_genesys_get_address (regs, REG04);
  if (r && (r->value & REG04_BITSET))
    return SANE_TRUE;
  return SANE_FALSE;
}

static SANE_Bool
gl843_get_gain4_bit (Genesys_Register_Set * regs)
{
  GenesysRegister *r = NULL;

  r = sanei_genesys_get_address (regs, REG06);
  if (r && (r->value & REG06_GAIN4))
    return SANE_TRUE;
  return SANE_FALSE;
}

/**
 * compute the step multiplier used
 */
static int
gl843_get_step_multiplier (Genesys_Register_Set * regs)
{
  GenesysRegister *r = NULL;
  int value = 1;

  r = sanei_genesys_get_address (regs, REG9D);
  if (r != NULL)
    {
      switch (r->value & 0x0c)
	{
	case 0x04:
	  value = 2;
	  break;
	case 0x08:
	  value = 4;
	  break;
	default:
	  value = 1;
	}
    }
  DBG(DBG_io, "%s: step multiplier is %d\n", __func__, value);
  return value;
}

static SANE_Bool
gl843_test_buffer_empty_bit (SANE_Byte val)
{
  if (val & REG41_BUFEMPTY)
    return SANE_TRUE;
  return SANE_FALSE;
}

static SANE_Bool
gl843_test_motor_flag_bit (SANE_Byte val)
{
  if (val & REG41_MOTORENB)
    return SANE_TRUE;
  return SANE_FALSE;
}

/** copy sensor specific settings */
static void gl843_setup_sensor(Genesys_Device* dev, const Genesys_Sensor& sensor,
                               Genesys_Register_Set* regs)
{
    DBG_HELPER(dbg);
    for (const auto& custom_reg : sensor.custom_regs) {
        regs->set8(custom_reg.address, custom_reg.value);
    }
    if (!(dev->model->flags & GENESYS_FLAG_FULL_HWDPI_MODE)) {
        regs->set8(0x7d, 0x90);
    }

    dev->segment_order = sensor.segment_order;
}


/** @brief set all registers to default values .
 * This function is called only once at the beginning and
 * fills register startup values for registers reused across scans.
 * Those that are rarely modified or not modified are written
 * individually.
 * @param dev device structure holding register set to initialize
 */
static void
gl843_init_registers (Genesys_Device * dev)
{
  // Within this function SENSOR_DEF marker documents that a register is part
  // of the sensors definition and the actual value is set in
  // gl843_setup_sensor().

    // 0x6c, 0x6d, 0x6e, 0x6f, 0xa6, 0xa7, 0xa8, 0xa9 are defined in the Gpo sensor struct

    DBG_HELPER(dbg);

  dev->reg.clear();

  /* default to KV-SS080 */
  SETREG (0xa2, 0x0f);
  if (dev->model->model_id == MODEL_CANON_CANOSCAN_8600F)
    {
      SETREG(0xa2, 0x1f);
    }
  SETREG (0x01, 0x00);
  SETREG (0x02, 0x78);
  SETREG (0x03, 0x1f);
    if (dev->model->model_id == MODEL_HP_SCANJET_G4010 ||
        dev->model->model_id == MODEL_HP_SCANJET_G4050 ||
        dev->model->model_id == MODEL_HP_SCANJET_4850C)
    {
        SETREG(0x03, 0x1d);
    }
    if (dev->model->model_id == MODEL_CANON_CANOSCAN_8400F) {
        SETREG(0x03, 0x1c);
    }

    SETREG(0x04, 0x10);

  // fine tune upon device description
  SETREG (0x05, 0x80);
  if (dev->model->model_id == MODEL_HP_SCANJET_G4010 ||
      dev->model->model_id == MODEL_HP_SCANJET_G4050 ||
      dev->model->model_id == MODEL_HP_SCANJET_4850C)
    {
      SETREG (0x05, 0x08);
    }

    const auto& sensor = sanei_genesys_find_sensor_any(dev);
    sanei_genesys_set_dpihw(dev->reg, sensor, sensor.optical_res);

  // TODO: on 8600F the windows driver turns off GAIN4 which is recommended
  SETREG (0x06, 0xd8); /* SCANMOD=110, PWRBIT and GAIN4 */
    if (dev->model->model_id == MODEL_HP_SCANJET_G4010 ||
        dev->model->model_id == MODEL_HP_SCANJET_G4050 ||
        dev->model->model_id == MODEL_HP_SCANJET_4850C)
    {
        SETREG(0x06, 0xd8); /* SCANMOD=110, PWRBIT and GAIN4 */
    }
    if (dev->model->model_id == MODEL_CANON_CANOSCAN_4400F) {
        SETREG(0x06, 0xf0); /* SCANMOD=111, PWRBIT and no GAIN4 */
    }

  SETREG (0x08, 0x00);
  SETREG (0x09, 0x00);
  SETREG (0x0a, 0x00);
    if (dev->model->model_id == MODEL_HP_SCANJET_G4010 ||
        dev->model->model_id == MODEL_HP_SCANJET_G4050 ||
        dev->model->model_id == MODEL_HP_SCANJET_4850C)
    {
        SETREG(0x0a, 0x18);
    }
    if (dev->model->model_id == MODEL_CANON_CANOSCAN_8400F) {
        SETREG(0x0a, 0x10);
    }

  // This register controls clock and RAM settings and is further modified in
  // gl843_boot
  SETREG (0x0b, 0x6a);

    if (dev->model->model_id == MODEL_CANON_CANOSCAN_4400F) {
        SETREG(0x0b, 0x69); // 16M only
    }
    if (dev->model->model_id == MODEL_CANON_CANOSCAN_8600F) {
        SETREG(0x0b, 0x89);
    }
    if (dev->model->model_id == MODEL_HP_SCANJET_G4010 ||
        dev->model->model_id == MODEL_HP_SCANJET_G4050 ||
        dev->model->model_id == MODEL_HP_SCANJET_4850C)
    {
        SETREG(0x0b, 0x69);
    }

    if (dev->model->model_id != MODEL_CANON_CANOSCAN_8400F) {
        SETREG (0x0c, 0x00);
    }

  // EXPR[0:15], EXPG[0:15], EXPB[0:15]: Exposure time settings.
  SETREG(0x10, 0x00); // SENSOR_DEF
  SETREG(0x11, 0x00); // SENSOR_DEF
  SETREG(0x12, 0x00); // SENSOR_DEF
  SETREG(0x13, 0x00); // SENSOR_DEF
  SETREG(0x14, 0x00); // SENSOR_DEF
  SETREG(0x15, 0x00); // SENSOR_DEF
    if (dev->model->model_id == MODEL_CANON_CANOSCAN_4400F ||
        dev->model->model_id == MODEL_CANON_CANOSCAN_8600F)
    {
        dev->reg.set16(REG_EXPR, 0x9c40);
        dev->reg.set16(REG_EXPG, 0x9c40);
        dev->reg.set16(REG_EXPB, 0x9c40);
    }
    if (dev->model->model_id == MODEL_HP_SCANJET_G4010 ||
        dev->model->model_id == MODEL_HP_SCANJET_G4050 ||
        dev->model->model_id == MODEL_HP_SCANJET_4850C)
    {
        dev->reg.set16(REG_EXPR, 0x2c09);
        dev->reg.set16(REG_EXPG, 0x22b8);
        dev->reg.set16(REG_EXPB, 0x10f0);
    }

  // CCD signal settings.
  SETREG(0x16, 0x33); // SENSOR_DEF
  SETREG(0x17, 0x1c); // SENSOR_DEF
  SETREG(0x18, 0x10); // SENSOR_DEF

  // EXPDMY[0:7]: Exposure time of dummy lines.
  SETREG(0x19, 0x2a); // SENSOR_DEF

  // Various CCD clock settings.
  SETREG(0x1a, 0x04); // SENSOR_DEF
  SETREG(0x1b, 0x00); // SENSOR_DEF
  SETREG(0x1c, 0x20); // SENSOR_DEF
  SETREG(0x1d, 0x04); // SENSOR_DEF

    SETREG(0x1e, 0x10);
    if (dev->model->model_id == MODEL_CANON_CANOSCAN_4400F ||
        dev->model->model_id == MODEL_CANON_CANOSCAN_8600F)
    {
        SETREG(0x1e, 0x20);
    }
    if (dev->model->model_id == MODEL_CANON_CANOSCAN_8400F) {
        SETREG(0x1e, 0xa0);
    }

  SETREG (0x1f, 0x01);
  if (dev->model->model_id == MODEL_CANON_CANOSCAN_8600F)
    {
      SETREG(0x1f, 0xff);
    }

  SETREG (0x20, 0x10);
  SETREG (0x21, 0x04);

    SETREG(0x22, 0x01);
    SETREG(0x23, 0x01);
    if (dev->model->model_id == MODEL_CANON_CANOSCAN_4400F ||
        dev->model->model_id == MODEL_CANON_CANOSCAN_8600F)
    {
        SETREG(0x22, 0xc8);
        SETREG(0x23, 0xc8);
    }
    if (dev->model->model_id == MODEL_CANON_CANOSCAN_8400F) {
        SETREG(0x22, 0x50);
        SETREG(0x23, 0x50);
    }

  SETREG (0x24, 0x04);
  SETREG (0x25, 0x00);
  SETREG (0x26, 0x00);
  SETREG (0x27, 0x00);
  SETREG (0x2c, 0x02);
  SETREG (0x2d, 0x58);
  // BWHI[0:7]: high level of black and white threshold
  SETREG (0x2e, 0x80);
  // BWLOW[0:7]: low level of black and white threshold
  SETREG (0x2f, 0x80);
  SETREG (0x30, 0x00);
  SETREG (0x31, 0x14);
  SETREG (0x32, 0x27);
  SETREG (0x33, 0xec);

  // DUMMY: CCD dummy and optically black pixel count
  SETREG (0x34, 0x24);
  if (dev->model->model_id == MODEL_CANON_CANOSCAN_8600F)
    {
      SETREG(0x34, 0x14);
    }

  // MAXWD: If available buffer size is less than 2*MAXWD words, then
  // "buffer full" state will be set.
  SETREG (0x35, 0x00);
  SETREG (0x36, 0xff);
  SETREG (0x37, 0xff);

  // LPERIOD: Line period or exposure time for CCD or CIS.
  SETREG(0x38, 0x55); // SENSOR_DEF
  SETREG(0x39, 0xf0); // SENSOR_DEF

  // FEEDL[0:24]: The number of steps of motor movement.
  SETREG(0x3d, 0x00);
  SETREG (0x3e, 0x00);
  SETREG (0x3f, 0x01);

  // Latch points for high and low bytes of R, G and B channels of AFE. If
  // multiple clocks per pixel are consumed, then the setting defines during
  // which clock the corresponding value will be read.
  // RHI[0:4]: The latch point for high byte of R channel.
  // RLOW[0:4]: The latch point for low byte of R channel.
  // GHI[0:4]: The latch point for high byte of G channel.
  // GLOW[0:4]: The latch point for low byte of G channel.
  // BHI[0:4]: The latch point for high byte of B channel.
  // BLOW[0:4]: The latch point for low byte of B channel.
  SETREG(0x52, 0x01); // SENSOR_DEF
  SETREG(0x53, 0x04); // SENSOR_DEF
  SETREG(0x54, 0x07); // SENSOR_DEF
  SETREG(0x55, 0x0a); // SENSOR_DEF
  SETREG(0x56, 0x0d); // SENSOR_DEF
  SETREG(0x57, 0x10); // SENSOR_DEF

  // VSMP[0:4]: The position of the image sampling pulse for AFE in cycles.
  // VSMPW[0:2]: The length of the image sampling pulse for AFE in cycles.
  SETREG(0x58, 0x1b); // SENSOR_DEF

  SETREG(0x59, 0x00); // SENSOR_DEF
  SETREG(0x5a, 0x40); // SENSOR_DEF

  // 0x5b-0x5c: GMMADDR[0:15] address for gamma or motor tables download
  // SENSOR_DEF

    // DECSEL[0:2]: The number of deceleratino steps after touching home sensor
    // STOPTIM[0:4]: The stop duration between change of directions in
    // backtracking
    SETREG(0x5e, 0x23);
    if (dev->model->model_id == MODEL_CANON_CANOSCAN_4400F) {
        SETREG(0x5e, 0x3f);
    }
    if (dev->model->model_id == MODEL_CANON_CANOSCAN_8400F) {
        SETREG(0x5e, 0x85);
    }
    if (dev->model->model_id == MODEL_CANON_CANOSCAN_8600F) {
        SETREG(0x5e, 0x1f);
    }

    //FMOVDEC: The number of deceleration steps in table 5 for auto-go-home
    SETREG(0x5f, 0x01);
    if (dev->model->model_id == MODEL_CANON_CANOSCAN_4400F) {
        SETREG(0x5f, 0xf0);
    }
    if (dev->model->model_id == MODEL_CANON_CANOSCAN_8600F) {
        SETREG(0x5f, 0xf0);
    }

  // Z1MOD[0:20]
  SETREG (0x60, 0x00);
  SETREG (0x61, 0x00);
  SETREG (0x62, 0x00);

  // Z2MOD[0:20]
  SETREG (0x63, 0x00);
  SETREG (0x64, 0x00);
  SETREG (0x65, 0x00);

  // STEPSEL[0:1]. Motor movement step mode selection for tables 1-3 in
  // scanning mode.
  // MTRPWM[0:5]. Motor phase PWM duty cycle setting for tables 1-3
  SETREG (0x67, 0x7f);
  // FSTPSEL[0:1]: Motor movement step mode selection for tables 4-5 in
  // command mode.
  // FASTPWM[5:0]: Motor phase PWM duty cycle setting for tables 4-5
  SETREG (0x68, 0x7f);

  // FSHDEC[0:7]: The number of deceleration steps after scanning is finished
  // (table 3)
  SETREG (0x69, 0x01);
  if (dev->model->model_id == MODEL_CANON_CANOSCAN_8600F)
    {
      SETREG(0x69, 64);
    }

  // FMOVNO[0:7] The number of acceleration or deceleration steps for fast
  // moving (table 4)
  SETREG (0x6a, 0x04);
  if (dev->model->model_id == MODEL_CANON_CANOSCAN_8600F)
    {
      SETREG(0x69, 64);
    }

    // GPIO-related register bits
    SETREG(0x6b, 0x30);
    if (dev->model->model_id == MODEL_CANON_CANOSCAN_4400F ||
        dev->model->model_id == MODEL_CANON_CANOSCAN_8600F)
    {
        SETREG(0x6b, 0x72);
    }
    if (dev->model->model_id == MODEL_CANON_CANOSCAN_8400F) {
        SETREG(0x6b, 0xb1);
    }
    if (dev->model->model_id == MODEL_HP_SCANJET_G4010 ||
        dev->model->model_id == MODEL_HP_SCANJET_G4050 ||
        dev->model->model_id == MODEL_HP_SCANJET_4850C)
    {
        SETREG(0x6b, 0xf4);
    }

  // 0x6c, 0x6d, 0x6e, 0x6f are set according to gpio tables. See
  // gl843_init_gpio.

    // RSH[0:4]: The position of rising edge of CCD RS signal in cycles
    // RSL[0:4]: The position of falling edge of CCD RS signal in cycles
    // CPH[0:4]: The position of rising edge of CCD CP signal in cycles.
    // CPL[0:4]: The position of falling edge of CCD CP signal in cycles
    SETREG(0x70, 0x01); // SENSOR_DEF
    SETREG(0x71, 0x03); // SENSOR_DEF
    SETREG(0x72, 0x04); // SENSOR_DEF
    SETREG(0x73, 0x05); // SENSOR_DEF

    if (dev->model->model_id == MODEL_CANON_CANOSCAN_4400F) {
        SETREG(0x70, 0x01);
        SETREG(0x71, 0x03);
        SETREG(0x72, 0x01);
        SETREG(0x73, 0x03);
    }
    if (dev->model->model_id == MODEL_CANON_CANOSCAN_8400F) {
        SETREG(0x70, 0x01);
        SETREG(0x71, 0x03);
        SETREG(0x72, 0x03);
        SETREG(0x73, 0x04);
    }
    if (dev->model->model_id == MODEL_CANON_CANOSCAN_8600F) {
        SETREG(0x70, 0x00);
        SETREG(0x71, 0x02);
        SETREG(0x72, 0x02);
        SETREG(0x73, 0x04);
    }
    if (dev->model->model_id == MODEL_HP_SCANJET_G4010 ||
        dev->model->model_id == MODEL_HP_SCANJET_G4050 ||
        dev->model->model_id == MODEL_HP_SCANJET_4850C)
    {
        SETREG(0x70, 0x00);
        SETREG(0x71, 0x02);
        SETREG(0x72, 0x00);
        SETREG(0x73, 0x00);
    }

  // CK1MAP[0:17], CK3MAP[0:17], CK4MAP[0:17]: CCD clock bit mapping setting.
  SETREG(0x74, 0x00); // SENSOR_DEF
  SETREG(0x75, 0x00); // SENSOR_DEF
  SETREG(0x76, 0x3c); // SENSOR_DEF
  SETREG(0x77, 0x00); // SENSOR_DEF
  SETREG(0x78, 0x00); // SENSOR_DEF
  SETREG(0x79, 0x9f); // SENSOR_DEF
  SETREG(0x7a, 0x00); // SENSOR_DEF
  SETREG(0x7b, 0x00); // SENSOR_DEF
  SETREG(0x7c, 0x55); // SENSOR_DEF

    // various AFE settings
    SETREG(0x7d, 0x00);
    if (dev->model->model_id == MODEL_CANON_CANOSCAN_8400F) {
        SETREG(0x7d, 0x20);
    }

  // GPOLED[x]: LED vs GPIO settings
  SETREG(0x7e, 0x00);

  // BSMPDLY, VSMPDLY
  // LEDCNT[0:1]: Controls led blinking and its period
  SETREG (0x7f, 0x00);

    // VRHOME, VRMOVE, VRBACK, VRSCAN: Vref settings of the motor driver IC for
    // moving in various situations.
    SETREG(0x80, 0x00);
    if (dev->model->model_id == MODEL_CANON_CANOSCAN_4400F) {
        SETREG(0x80, 0x0c);
    }
    if (dev->model->model_id == MODEL_CANON_CANOSCAN_8400F) {
        SETREG(0x80, 0x28);
    }
    if (dev->model->model_id == MODEL_HP_SCANJET_G4010 ||
        dev->model->model_id == MODEL_HP_SCANJET_G4050 ||
        dev->model->model_id == MODEL_HP_SCANJET_4850C)
    {
        SETREG(0x80, 0x50);
    }

  if (dev->model->model_id != MODEL_CANON_CANOSCAN_4400F)
    {
      // NOTE: Historical code. None of the following 6 registers are
      // documented in the datasheet. Their default value is 0, so probably it's
      // not a bad idea to leave this here.
      SETREG (0x81, 0x00);
      SETREG (0x82, 0x00);
      SETREG (0x83, 0x00);
      SETREG (0x84, 0x00);
      SETREG (0x85, 0x00);
      SETREG (0x86, 0x00);
    }

    SETREG(0x87, 0x00);
    if (dev->model->model_id == MODEL_CANON_CANOSCAN_4400F ||
        dev->model->model_id == MODEL_CANON_CANOSCAN_8400F ||
        dev->model->model_id == MODEL_CANON_CANOSCAN_8600F)
    {
        SETREG(0x87, 0x02);
    }

    // MTRPLS[0:7]: The width of the ADF motor trigger signal pulse.
    if (dev->model->model_id != MODEL_CANON_CANOSCAN_8400F) {
        SETREG(0x94, 0xff);
    }

  // 0x95-0x97: SCANLEN[0:19]: Controls when paper jam bit is set in sheetfed
  // scanners.

  // ONDUR[0:15]: The duration of PWM ON phase for LAMP control
  // OFFDUR[0:15]: The duration of PWM OFF phase for LAMP control
  // both of the above are in system clocks
  if (dev->model->model_id == MODEL_CANON_CANOSCAN_8600F)
    {
      SETREG(0x98, 0x00);
      SETREG(0x99, 0x00);
      SETREG(0x9a, 0x00);
      SETREG(0x9b, 0x00);
    }
    if (dev->model->model_id == MODEL_HP_SCANJET_G4010 ||
        dev->model->model_id == MODEL_HP_SCANJET_G4050 ||
        dev->model->model_id == MODEL_HP_SCANJET_4850C)
    {
        // TODO: move to set for scan
        SETREG(0x98, 0x03);
        SETREG(0x99, 0x30);
        SETREG(0x9a, 0x01);
        SETREG(0x9b, 0x80);
    }

    // RMADLY[0:1], MOTLAG, CMODE, STEPTIM, MULDMYLN, IFRS
    SETREG(0x9d, 0x04);
    if (dev->model->model_id == MODEL_CANON_CANOSCAN_4400F ||
        dev->model->model_id == MODEL_CANON_CANOSCAN_8400F ||
        dev->model->model_id == MODEL_CANON_CANOSCAN_8600F ||
        dev->model->model_id == MODEL_HP_SCANJET_G4010 ||
        dev->model->model_id == MODEL_HP_SCANJET_G4050 ||
        dev->model->model_id == MODEL_HP_SCANJET_4850C)
    {
        SETREG(0x9d, 0x08); // sets the multiplier for slope tables
    }


  // SEL3INV, TGSTIME[0:2], TGWTIME[0:2]
  if (dev->model->model_id != MODEL_CANON_CANOSCAN_8400F)
    {
      SETREG(0x9e, 0x00); // SENSOR_DEF
    }

    // RFHSET[0:4]: Refresh time of SDRAM in units of 2us
    if (dev->model->model_id == MODEL_CANON_CANOSCAN_4400F ||
        dev->model->model_id == MODEL_CANON_CANOSCAN_8600F)
    {
        SETREG(0xa2, 0x1f);
    }

    // 0xa6-0xa9: controls gpio, see gl843_gpio_init

    // not documented
    if (dev->model->model_id != MODEL_CANON_CANOSCAN_4400F
     && dev->model->model_id != MODEL_CANON_CANOSCAN_8400F)
    {
        SETREG(0xaa, 0x00);
    }

    // GPOM9, MULSTOP[0-2], NODECEL, TB3TB1, TB5TB2, FIX16CLK.
    if (dev->model->model_id != MODEL_CANON_CANOSCAN_8400F) {
        SETREG(0xab, 0x50);
    }
    if (dev->model->model_id == MODEL_CANON_CANOSCAN_4400F) {
        SETREG(0xab, 0x00);
    }
    if (dev->model->model_id == MODEL_HP_SCANJET_G4010 ||
        dev->model->model_id == MODEL_HP_SCANJET_G4050 ||
        dev->model->model_id == MODEL_HP_SCANJET_4850C)
    {
        // BUG: this should apply to MODEL_CANON_CANOSCAN_8600F too, but due to previous bug
        // the 8400F case overwrote it
        SETREG(0xab, 0x40);
    }

  // VRHOME[3:2], VRMOVE[3:2], VRBACK[3:2]: Vref setting of the motor driver IC
  // for various situations.
    if (dev->model->model_id == MODEL_CANON_CANOSCAN_8600F ||
        dev->model->model_id == MODEL_HP_SCANJET_G4010 ||
        dev->model->model_id == MODEL_HP_SCANJET_G4050 ||
        dev->model->model_id == MODEL_HP_SCANJET_4850C)
    {
        SETREG(0xac, 0x00);
    }

  dev->calib_reg = dev->reg;
}

// Send slope table for motor movement slope_table in machine byte order
static void gl843_send_slope_table(Genesys_Device* dev, int table_nr,
                                   const std::vector<uint16_t>& slope_table,
                                   int steps)
{
    DBG_HELPER_ARGS(dbg, "table_nr = %d, steps = %d", table_nr, steps);

  int i;
  char msg[10000];

  std::vector<uint8_t> table(steps * 2);
  for (i = 0; i < steps; i++)
    {
      table[i * 2] = slope_table[i] & 0xff;
      table[i * 2 + 1] = slope_table[i] >> 8;
    }

  if (DBG_LEVEL >= DBG_io)
    {
      sprintf (msg, "write slope %d (%d)=", table_nr, steps);
      for (i = 0; i < steps; i++)
	{
	  sprintf (msg+strlen(msg), "%d", slope_table[i]);
	}
      DBG(DBG_io, "%s: %s\n", __func__, msg);
    }

    // slope table addresses are fixed : 0x4000,  0x4800,  0x5000,  0x5800,  0x6000
    // XXX STEF XXX USB 1.1 ? sanei_genesys_write_0x8c (dev, 0x0f, 0x14);
    write_data(dev, 0x4000 + 0x800 * table_nr, steps * 2, table.data());
}


// Set values of analog frontend
static void gl843_set_fe(Genesys_Device* dev, const Genesys_Sensor& sensor, uint8_t set)
{
    DBG_HELPER_ARGS(dbg, "%s", set == AFE_INIT ? "init" :
                               set == AFE_SET ? "set" :
                               set == AFE_POWER_SAVE ? "powersave" : "huh?");
    (void) sensor;
  int i;

  if (set == AFE_INIT)
    {
      DBG(DBG_proc, "%s(): setting DAC %u\n", __func__, dev->model->dac_type);
      dev->frontend = dev->frontend_initial;
        dev->frontend_is_init = true;
    }

    // check analog frontend type
    // FIXME: looks like we write to that register with initial data
    uint8_t val = dev->read_register(REG04);
  if ((val & REG04_FESET) != 0x00)
    {
        // for now there is no support for AD fe
        throw SaneException(SANE_STATUS_UNSUPPORTED, "unsupported frontend type %d",
                            val & REG04_FESET);
    }

  DBG(DBG_proc, "%s(): frontend reset complete\n", __func__);

  for (i = 1; i <= 3; i++)
    {
        // FIXME: the check below is just historical artifact, we can remove it when convenient
        if (!dev->frontend_is_init) {
            sanei_genesys_fe_write_data(dev, i, 0x00);
        } else {
            sanei_genesys_fe_write_data(dev, i, dev->frontend.regs.get_value(0x00 + i));
        }
    }
    for (const auto& reg : sensor.custom_fe_regs) {
        sanei_genesys_fe_write_data(dev, reg.address, reg.value);
    }

  for (i = 0; i < 3; i++)
    {
         // FIXME: the check below is just historical artifact, we can remove it when convenient
        if (!dev->frontend_is_init) {
            sanei_genesys_fe_write_data(dev, 0x20 + i, 0x00);
        } else {
            sanei_genesys_fe_write_data(dev, 0x20 + i, dev->frontend.get_offset(i));
        }
    }

  if (dev->model->ccd_type == CCD_KVSS080)
    {
      for (i = 0; i < 3; i++)
	{
            // FIXME: the check below is just historical artifact, we can remove it when convenient
            if (!dev->frontend_is_init) {
                sanei_genesys_fe_write_data(dev, 0x24 + i, 0x00);
            } else {
                sanei_genesys_fe_write_data(dev, 0x24 + i, dev->frontend.regs.get_value(0x24 + i));
            }
	}
    }

  for (i = 0; i < 3; i++)
    {
        // FIXME: the check below is just historical artifact, we can remove it when convenient
        if (!dev->frontend_is_init) {
            sanei_genesys_fe_write_data(dev, 0x28 + i, 0x00);
        } else {
            sanei_genesys_fe_write_data(dev, 0x28 + i, dev->frontend.get_gain(i));
        }
    }
}


static void gl843_init_motor_regs_scan(Genesys_Device* dev,
                                       const Genesys_Sensor& sensor,
                                       Genesys_Register_Set* reg,
                                       unsigned int exposure,
                                       float scan_yres,
                                       int scan_step_type,
                                       unsigned int scan_lines,
                                       unsigned int scan_dummy,
                                       unsigned int feed_steps,
                                       unsigned int flags)
{
    DBG_HELPER_ARGS(dbg, "exposure=%d, scan_yres=%g, scan_step_type=%d, scan_lines=%d, scan_dummy=%d, "
                         "feed_steps=%d, flags=%x",
                    exposure, scan_yres, scan_step_type, scan_lines, scan_dummy, feed_steps, flags);

  int use_fast_fed, coeff;
  unsigned int lincnt;
    std::vector<uint16_t> scan_table;
    std::vector<uint16_t> fast_table;
  int scan_steps,fast_steps, fast_step_type;
  unsigned int feedl,factor,dist;
  GenesysRegister *r;
  uint32_t z1, z2;

  /* get step multiplier */
  factor = gl843_get_step_multiplier (reg);

  use_fast_fed = 0;

  if((scan_yres>=300 && feed_steps>900) || (flags & MOTOR_FLAG_FEED))
    use_fast_fed=1;

  lincnt=scan_lines;
    reg->set24(REG_LINCNT, lincnt);
  DBG(DBG_io, "%s: lincnt=%d\n", __func__, lincnt);

  /* compute register 02 value */
  r = sanei_genesys_get_address (reg, REG02);
  r->value = 0x00;
  sanei_genesys_set_motor_power(*reg, true);

  if (use_fast_fed)
    r->value |= REG02_FASTFED;
  else
    r->value &= ~REG02_FASTFED;

  /* in case of automatic go home, move until home sensor */
  if (flags & MOTOR_FLAG_AUTO_GO_HOME)
    r->value |= REG02_AGOHOME | REG02_NOTHOME;

  /* disable backtracking */
  if ((flags & MOTOR_FLAG_DISABLE_BUFFER_FULL_MOVE)
      ||(scan_yres>=2400)
      ||(scan_yres>=sensor.optical_res))
    r->value |= REG02_ACDCDIS;

  /* scan and backtracking slope table */
  sanei_genesys_slope_table(scan_table,
                            &scan_steps,
                            scan_yres,
                            exposure,
                            dev->motor.base_ydpi,
                            scan_step_type,
                            factor,
                            dev->model->motor_type,
                            gl843_motor_profiles);
    gl843_send_slope_table(dev, SCAN_TABLE, scan_table, scan_steps * factor);
    gl843_send_slope_table(dev, BACKTRACK_TABLE, scan_table, scan_steps * factor);

  /* STEPNO */
  r = sanei_genesys_get_address (reg, REG_STEPNO);
  r->value = scan_steps;

  /* FSHDEC */
  r = sanei_genesys_get_address (reg, REG_FSHDEC);
  r->value = scan_steps;

  /* fast table */
  fast_step_type=0;
  if(scan_step_type<=fast_step_type)
    {
      fast_step_type=scan_step_type;
    }
  sanei_genesys_slope_table(fast_table,
                            &fast_steps,
                            sanei_genesys_get_lowest_ydpi(dev),
                            exposure,
                            dev->motor.base_ydpi,
                            fast_step_type,
                            factor,
                            dev->model->motor_type,
                            gl843_motor_profiles);
    gl843_send_slope_table(dev, STOP_TABLE, fast_table, fast_steps * factor);
    gl843_send_slope_table(dev, FAST_TABLE, fast_table, fast_steps * factor);
    gl843_send_slope_table(dev, HOME_TABLE, fast_table, fast_steps * factor);

  /* FASTNO */
  r = sanei_genesys_get_address (reg, REG_FASTNO);
  r->value = fast_steps;

  /* FMOVNO */
  r = sanei_genesys_get_address (reg, REG_FMOVNO);
  r->value = fast_steps;

  /* substract acceleration distance from feedl */
  feedl=feed_steps;
  feedl<<=scan_step_type;

  dist = scan_steps;
  if (use_fast_fed)
    {
        dist += fast_steps*2;
    }
  DBG(DBG_io2, "%s: acceleration distance=%d\n", __func__, dist);

  /* get sure when don't insane value : XXX STEF XXX in this case we should
   * fall back to single table move */
    if (dist < feedl) {
        feedl -= dist;
    } else {
        feedl = 1;
    }

    reg->set24(REG_FEEDL, feedl);
  DBG(DBG_io, "%s: feedl=%d\n", __func__, feedl);

  /* doesn't seem to matter that much */
  sanei_genesys_calculate_zmod (use_fast_fed,
				  exposure,
				  scan_table,
				  scan_steps,
				  feedl,
                                  scan_steps,
                                  &z1,
                                  &z2);
  if(scan_yres>600)
    {
      z1=0;
      z2=0;
    }

    reg->set24(REG_Z1MOD, z1);
  DBG(DBG_info, "%s: z1 = %d\n", __func__, z1);

    reg->set24(REG_Z2MOD, z2);
  DBG(DBG_info, "%s: z2 = %d\n", __func__, z2);

  r = sanei_genesys_get_address (reg, REG1E);
  r->value &= 0xf0;		/* 0 dummy lines */
  r->value |= scan_dummy;	/* dummy lines */

  r = sanei_genesys_get_address (reg, REG67);
  r->value = 0x3f | (scan_step_type << REG67S_STEPSEL);

  r = sanei_genesys_get_address (reg, REG68);
  r->value = 0x3f | (scan_step_type << REG68S_FSTPSEL);

  /* steps for STOP table */
  r = sanei_genesys_get_address (reg, REG_FMOVDEC);
  r->value = fast_steps;

  /* Vref XXX STEF XXX : optical divider or step type ? */
  r = sanei_genesys_get_address (reg, 0x80);
  if (!(dev->model->flags & GENESYS_FLAG_FULL_HWDPI_MODE))
    {
      r->value = 0x50;
        coeff = sensor.get_hwdpi_divisor_for_dpi(scan_yres);
      if (dev->model->motor_type == MOTOR_KVSS080)
        {
          if(coeff>=1)
            {
              r->value |= 0x05;
            }
        }
      else {
        switch(coeff)
          {
          case 4:
              r->value |= 0x0a;
              break;
          case 2:
              r->value |= 0x0f;
              break;
          case 1:
              r->value |= 0x0f;
              break;
          }
        }
    }
}


/** @brief setup optical related registers
 * start and pixels are expressed in optical sensor resolution coordinate
 * space.
 * @param dev device to use
 * @param reg registers to set up
 * @param exposure exposure time to use
 * @param used_res scanning resolution used, may differ from
 *        scan's one
 * @param start logical start pixel coordinate
 * @param pixels logical number of pixels to use
 * @param channels number of color channles used (1 or 3)
 * @param depth bit depth of the scan (1, 8 or 16 bits)
 * @param ccd_size_divisor SANE_TRUE specifies how much x coordinates must be shrunk
 * @param color_filter to choose the color channel used in gray scans
 * @param flags to drive specific settings such no calibration, XPA use ...
 */
static void gl843_init_optical_regs_scan(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                         Genesys_Register_Set* reg, unsigned int exposure,
                                         const ScanSession& session)
{
    DBG_HELPER_ARGS(dbg, "exposure=%d, used_res=%d, start=%f, pixels=%d, channels=%d, depth=%d, "
                         "ccd_size_divisor=%d",
                    exposure, session.output_resolution, session.params.startx,
                    session.optical_pixels, session.params.channels, session.params.depth,
                    session.ccd_size_divisor);
    unsigned int dpihw;
  unsigned int tgtime;          /**> exposure time multiplier */
  GenesysRegister *r;

  /* tgtime */
  tgtime = exposure / 65536 + 1;
  DBG(DBG_io2, "%s: tgtime=%d\n", __func__, tgtime);

    // to manage high resolution device while keeping good low resolution scanning speed, we make
    // hardware dpi vary
    dpihw = sensor.get_register_hwdpi(session.output_resolution);
    DBG(DBG_io2, "%s: dpihw=%d\n", __func__, dpihw);

  /* sensor parameters */
    gl843_setup_sensor(dev, sensor, reg);

    // resolution is divided according to CKSEL
    unsigned ccd_pixels_per_system_pixel = sensor.ccd_pixels_per_system_pixel();
    DBG(DBG_io2, "%s: ccd_pixels_per_system_pixel=%d\n", __func__, ccd_pixels_per_system_pixel);

    gl843_set_fe(dev, sensor, AFE_SET);

  /* enable shading */
  r = sanei_genesys_get_address (reg, REG01);
  r->value &= ~REG01_SCAN;
    if ((session.params.flags & SCAN_FLAG_DISABLE_SHADING) ||
        (dev->model->flags & GENESYS_FLAG_NO_CALIBRATION))
    {
      r->value &= ~REG01_DVDSET;
    }
  else
    {
      r->value |= REG01_DVDSET;
    }
  if(dpihw>600)
    {
      r->value |= REG01_SHDAREA;
    }
  else
    {
      r->value &= ~REG01_SHDAREA;
    }

  r = sanei_genesys_get_address (reg, REG03);
  if (dev->model->model_id == MODEL_CANON_CANOSCAN_8600F)
    r->value |= REG03_AVEENB;
  else {
    r->value &= ~REG03_AVEENB;
  }

    // FIXME: we probably don't need to set exposure to registers at this point. It was this way
    // before a refactor.
    sanei_genesys_set_lamp_power(dev, sensor, *reg,
                                 !(session.params.flags & SCAN_FLAG_DISABLE_LAMP));

  /* select XPA */
  r->value &= ~REG03_XPASEL;
    if (session.params.flags & SCAN_FLAG_USE_XPA) {
        r->value |= REG03_XPASEL;
    }
    reg->state.is_xpa_on = session.params.flags & SCAN_FLAG_USE_XPA;

  /* BW threshold */
  r = sanei_genesys_get_address (reg, REG2E);
  r->value = dev->settings.threshold;
  r = sanei_genesys_get_address (reg, REG2F);
  r->value = dev->settings.threshold;

  /* monochrome / color scan */
  r = sanei_genesys_get_address (reg, REG04);
  switch (session.params.depth)
    {
    case 1:
      r->value &= ~REG04_BITSET;
      r->value |= REG04_LINEART;
      break;
    case 8:
      r->value &= ~(REG04_LINEART | REG04_BITSET);
      break;
    case 16:
      r->value &= ~REG04_LINEART;
      r->value |= REG04_BITSET;
      break;
    }

  r->value &= ~(REG04_FILTER | REG04_AFEMOD);
  if (session.params.channels == 1)
    {
      switch (session.params.color_filter)
	{
            case ColorFilter::RED:
                r->value |= 0x14;
                break;
            case ColorFilter::BLUE:
                r->value |= 0x1c;
                break;
            case ColorFilter::GREEN:
                r->value |= 0x18;
                break;
            default:
                break; // should not happen
	}
    } else {
        r->value |= 0x10;		/* mono */
    }

    sanei_genesys_set_dpihw(*reg, sensor, dpihw);

    // enable gamma tables
    r = sanei_genesys_get_address(reg, REG05);
    if (session.params.flags & SCAN_FLAG_DISABLE_GAMMA) {
        r->value &= ~REG05_GMMENB;
    } else {
        r->value |= REG05_GMMENB;
    }

    unsigned dpiset = session.output_resolution * session.ccd_size_divisor *
            ccd_pixels_per_system_pixel;

    if (sensor.dpiset_override != 0) {
        dpiset = sensor.dpiset_override;
    }
    reg->set16(REG_DPISET, dpiset);
    DBG(DBG_io2, "%s: dpiset used=%d\n", __func__, dpiset);

    reg->set16(REG_STRPIXEL, session.pixel_startx);
    reg->set16(REG_ENDPIXEL, session.pixel_endx);

  DBG(DBG_io2, "%s: pixels     =%d\n", __func__, session.optical_pixels);
  DBG(DBG_io2, "%s: depth      =%d\n", __func__, session.params.depth);

  /* MAXWD is expressed in 2 words unit */
  /* nousedspace = (mem_bank_range * 1024 / 256 -1 ) * 4; */
    // BUG: the division by ccd_size_divisor likely does not make sense
    reg->set24(REG_MAXWD, (session.output_line_bytes / session.ccd_size_divisor) >> 1);

    reg->set16(REG_LPERIOD, exposure / tgtime);
  DBG(DBG_io2, "%s: exposure used=%d\n", __func__, exposure/tgtime);

  r = sanei_genesys_get_address (reg, REG_DUMMY);
  r->value = sensor.dummy_pixel;
}

// computes physical parameters for specific scan setup
static void gl843_compute_session(Genesys_Device* dev, ScanSession& s,
                                  const Genesys_Sensor& sensor)
{
    // in case of dynamic lineart, we use an internal 8 bit gray scan to generate 1 lineart data
    if (s.params.flags & SCAN_FLAG_DYNAMIC_LINEART) {
        s.params.depth = 8;
    }

    // no 16 bit gamma for this ASIC
    if (s.params.depth == 16) {
        s.params.flags |= SCAN_FLAG_DISABLE_GAMMA;
    }

    compute_session(dev, s, sensor);

    // compute physical pixel positions
    unsigned ccd_pixels_per_system_pixel = sensor.ccd_pixels_per_system_pixel();
    s.pixel_startx = (s.params.startx + sensor.dummy_pixel) / ccd_pixels_per_system_pixel;
    s.pixel_endx = s.pixel_startx + s.optical_pixels / ccd_pixels_per_system_pixel;

    s.pixel_startx /= s.hwdpi_divisor;
    s.pixel_endx /= s.hwdpi_divisor;

    // in case of stagger we have to start at an odd coordinate
    if (s.num_staggered_lines > 0 && (s.pixel_startx & 1) == 0) {
        s.pixel_startx++;
        s.pixel_endx++;
    }

    s.computed = true;

    DBG(DBG_info, "%s ", __func__);
    debug_dump(DBG_info, s);
}

// set up registers for an actual scan this function sets up the scanner to scan in normal or single
// line mode
static void gl843_init_scan_regs(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                 Genesys_Register_Set* reg, const ScanSession& session)
{
    DBG_HELPER(dbg);
    session.assert_computed();

  unsigned int mflags;
  int exposure;

  int slope_dpi = 0;
  int dummy = 0;
  int scan_step_type = 1;

  DBG(DBG_info, "%s : stagger=%d lines\n", __func__, session.num_staggered_lines);

  /* we enable true gray for cis scanners only, and just when doing
   * scan since color calibration is OK for this mode
   */

  dummy = 0;
  /* dummy = 1;  XXX STEF XXX */

  /* slope_dpi */
  /* cis color scan is effectively a gray scan with 3 gray lines per color line and a FILTER of 0 */
  if (dev->model->is_cis)
    slope_dpi = session.params.yres * session.params.channels;
  else
    slope_dpi = session.params.yres;
  slope_dpi = slope_dpi * (1 + dummy);

  /* scan_step_type */
  exposure = sensor.exposure_lperiod;
  if (exposure < 0) {
      throw std::runtime_error("Exposure not defined in sensor definition");
  }
    scan_step_type = sanei_genesys_compute_step_type(gl843_motor_profiles, dev->model->motor_type,
                                                     exposure);

  DBG(DBG_info, "%s : exposure=%d pixels\n", __func__, exposure);
  DBG(DBG_info, "%s : scan_step_type=%d\n", __func__, scan_step_type);

    // now _LOGICAL_ optical values used are known, setup registers
    gl843_init_optical_regs_scan(dev, sensor, reg, exposure, session);

  /*** motor parameters ***/

  /* it seems base_dpi of the G4050 motor is changed above 600 dpi*/
  if (dev->model->motor_type == MOTOR_G4050 && session.params.yres>600)
    {
      dev->ld_shift_r = (dev->model->ld_shift_r*3800)/dev->motor.base_ydpi;
      dev->ld_shift_g = (dev->model->ld_shift_g*3800)/dev->motor.base_ydpi;
      dev->ld_shift_b = (dev->model->ld_shift_b*3800)/dev->motor.base_ydpi;
    }
  else
    {
      dev->ld_shift_r = dev->model->ld_shift_r;
      dev->ld_shift_g = dev->model->ld_shift_g;
      dev->ld_shift_b = dev->model->ld_shift_b;
    }

    mflags = 0;
    if (session.params.flags & SCAN_FLAG_DISABLE_BUFFER_FULL_MOVE) {
        mflags |= MOTOR_FLAG_DISABLE_BUFFER_FULL_MOVE;
    }
    if (session.params.flags & SCAN_FLAG_FEEDING) {
        mflags |= MOTOR_FLAG_FEED;
    }
    if (session.params.flags & SCAN_FLAG_USE_XPA) {
        mflags |= MOTOR_FLAG_USE_XPA;
    }

    unsigned scan_lines = dev->model->is_cis ? session.output_line_count * session.params.channels
                                             : session.output_line_count;

    gl843_init_motor_regs_scan(dev, sensor, reg, exposure, slope_dpi, scan_step_type,
                               scan_lines, dummy, session.params.starty, mflags);

    dev->read_buffer.clear();
    dev->read_buffer.alloc(session.buffer_size_read);

    dev->lines_buffer.clear();
    dev->lines_buffer.alloc(session.buffer_size_lines);

    dev->shrink_buffer.clear();
    dev->shrink_buffer.alloc(session.buffer_size_shrink);

    dev->out_buffer.clear();
    dev->out_buffer.alloc(session.buffer_size_out);

    build_image_pipeline(dev, session);

  dev->read_bytes_left_after_deseg = session.output_line_bytes * session.output_line_count;

    DBG(DBG_info, "%s: physical bytes to read = %lu\n", __func__,
        (u_long) dev->read_bytes_left_after_deseg);
  dev->read_active = SANE_TRUE;

    dev->session = session;
  dev->current_setup.pixels = session.output_pixels;
  DBG(DBG_info, "%s: current_setup.pixels=%d\n", __func__, dev->current_setup.pixels);
  dev->current_setup.lines = session.output_line_count;
  dev->current_setup.exposure_time = exposure;
  dev->current_setup.xres = session.output_resolution;
  dev->current_setup.ccd_size_divisor = session.ccd_size_divisor;
  dev->current_setup.stagger = session.num_staggered_lines;
  dev->current_setup.max_shift = session.max_color_shift_lines + session.num_staggered_lines;

  dev->total_bytes_read = 0;
    dev->total_bytes_to_read =
            multiply_by_depth_ceil(session.params.get_requested_pixels() * session.params.lines,
                                   session.params.depth) * session.params.channels;

  DBG(DBG_info, "%s: total bytes to send = %lu\n", __func__, (u_long) dev->total_bytes_to_read);
}

static void
gl843_calculate_current_setup(Genesys_Device * dev, const Genesys_Sensor& sensor)
{
  int start;

  int exposure;

    DBG(DBG_info, "%s ", __func__);
    debug_dump(DBG_info, dev->settings);

  /* we have 2 domains for ccd: xres below or above half ccd max dpi */
  unsigned ccd_size_divisor = sensor.get_ccd_size_divisor_for_dpi(dev->settings.xres);

    if (dev->settings.scan_method == ScanMethod::TRANSPARENCY ||
        dev->settings.scan_method == ScanMethod::TRANSPARENCY_INFRARED)
    {
        start = SANE_UNFIX(dev->model->x_offset_ta);
    } else {
        start = SANE_UNFIX(dev->model->x_offset);
    }

    if (dev->model->model_id == MODEL_CANON_CANOSCAN_8400F ||
        dev->model->model_id == MODEL_CANON_CANOSCAN_8600F)
    {
        // FIXME: this is probably just an artifact of a bug elsewhere
        start /= ccd_size_divisor;
    }

  start += dev->settings.tl_x;
  start = (start * sensor.optical_res) / MM_PER_INCH;

    ScanSession session;
    session.params.xres = dev->settings.xres;
    session.params.yres = dev->settings.yres;
    session.params.startx = start; // not used
    session.params.starty = 0; // not used
    session.params.pixels = dev->settings.pixels;
    session.params.requested_pixels = dev->settings.requested_pixels;
    session.params.lines = dev->settings.lines;
    session.params.depth = dev->settings.get_depth();
    session.params.channels = dev->settings.get_channels();
    session.params.scan_method = dev->settings.scan_method;
    session.params.scan_mode = dev->settings.scan_mode;
    session.params.color_filter = dev->settings.color_filter;
    session.params.flags = 0;

    gl843_compute_session(dev, session, sensor);

  /* compute scan parameters values */
  /* pixels are allways given at half or full CCD optical resolution */
  /* use detected left margin  and fixed value */

  /* exposure */
  exposure = sensor.exposure_lperiod;
  if (exposure < 0) {
      throw std::runtime_error("Exposure not defined in sensor definition");
  }
  DBG(DBG_info, "%s : exposure=%d pixels\n", __func__, exposure);

  /* it seems base_dpi of the G4050 motor is changed above 600 dpi*/
    if (dev->model->motor_type == MOTOR_G4050 && session.params.yres>600) {
      dev->ld_shift_r = (dev->model->ld_shift_r*3800)/dev->motor.base_ydpi;
      dev->ld_shift_g = (dev->model->ld_shift_g*3800)/dev->motor.base_ydpi;
      dev->ld_shift_b = (dev->model->ld_shift_b*3800)/dev->motor.base_ydpi;
    } else {
      dev->ld_shift_r = dev->model->ld_shift_r;
      dev->ld_shift_g = dev->model->ld_shift_g;
      dev->ld_shift_b = dev->model->ld_shift_b;
    }

    dev->session = session;
    dev->current_setup.pixels = session.output_pixels;
  DBG(DBG_info, "%s: current_setup.pixels=%d\n", __func__, dev->current_setup.pixels);
    dev->current_setup.lines = session.output_line_count;
  dev->current_setup.exposure_time = exposure;
    dev->current_setup.xres = session.params.xres;
  dev->current_setup.ccd_size_divisor = session.ccd_size_divisor;
    dev->current_setup.stagger = session.num_staggered_lines;
    dev->current_setup.max_shift = session.max_color_shift_lines + session.num_staggered_lines;

  DBG(DBG_proc, "%s: completed\n", __func__);
}

/**
 * for fast power saving methods only, like disabling certain amplifiers
 * @param dev device to use
 * @param enable true to set inot powersaving
 * */
static void gl843_save_power(Genesys_Device* dev, SANE_Bool enable)
{
    DBG_HELPER_ARGS(dbg, "enable = %d", enable);

    // switch KV-SS080 lamp off
    if (dev->model->gpo_type == GPO_KVSS080) {
        uint8_t val = dev->read_register(REG6C);
        if (enable) {
            val &= 0xef;
        } else {
            val |= 0x10;
        }
        dev->write_register(REG6C, val);
    }
}

static void gl843_set_powersaving(Genesys_Device* dev, int delay /* in minutes */)
{
    (void) dev;
    DBG_HELPER_ARGS(dbg, "delay = %d", delay);
}

static void gl843_start_action(Genesys_Device* dev)
{
    DBG_HELPER(dbg);
    dev->write_register(0x0f, 0x01);
}

static void gl843_stop_action_no_move(Genesys_Device* dev, Genesys_Register_Set* reg)
{
    DBG_HELPER(dbg);
    uint8_t val = reg->get8(REG01);
    val &= ~REG01_SCAN;
    reg->set8(REG01, val);
    dev->write_register(REG01, val);
    sanei_genesys_sleep_ms(100);
}

static void gl843_stop_action(Genesys_Device* dev)
{
    DBG_HELPER(dbg);
  uint8_t val;
  unsigned int loop;

    sanei_genesys_get_status(dev, &val);
  if (DBG_LEVEL >= DBG_io)
    {
      sanei_genesys_print_status (val);
    }

    uint8_t val40 = dev->read_register(REG40);

  /* only stop action if needed */
  if (!(val40 & REG40_DATAENB) && !(val40 & REG40_MOTMFLG))
    {
      DBG(DBG_info, "%s: already stopped\n", __func__);
      return;
    }

  /* ends scan 646  */
  val = dev->reg.get8(REG01);
  val &= ~REG01_SCAN;
  dev->reg.set8(REG01, val);
    dev->write_register(REG01, val);

  sanei_genesys_sleep_ms(100);

  loop = 10;
  while (loop > 0)
    {
        sanei_genesys_get_status(dev, &val);
      if (DBG_LEVEL >= DBG_io)
	{
	  sanei_genesys_print_status (val);
	}
        val40 = dev->read_register(0x40);

      /* if scanner is in command mode, we are done */
      if (!(val40 & REG40_DATAENB) && !(val40 & REG40_MOTMFLG)
	  && !(val & REG41_MOTORENB))
	{
      return;
	}

      sanei_genesys_sleep_ms(100);
      loop--;
    }

  throw SaneException(SANE_STATUS_IO_ERROR, "could not stop motor");
}

static void gl843_get_paper_sensor(Genesys_Device* dev, SANE_Bool * paper_loaded)
{
    DBG_HELPER(dbg);

    uint8_t val = dev->read_register(REG6D);

    *paper_loaded = (val & 0x1) == 0;
}

static void gl843_eject_document(Genesys_Device* dev)
{
    (void) dev;
    DBG_HELPER(dbg);
}


static void gl843_load_document(Genesys_Device* dev)
{
    DBG_HELPER(dbg);
    (void) dev;
}

/**
 * detects end of document and adjust current scan
 * to take it into account
 * used by sheetfed scanners
 */
static void gl843_detect_document_end(Genesys_Device* dev)
{
    DBG_HELPER(dbg);
  SANE_Bool paper_loaded;
  unsigned int scancnt = 0;
  int flines, bytes_remain, sublines,
    bytes_to_flush, lines, sub_bytes, tmp, read_bytes_left;

    gl843_get_paper_sensor(dev, &paper_loaded);

  /* sheetfed scanner uses home sensor as paper present */
  if ((dev->document == SANE_TRUE) && !paper_loaded)
    {
      DBG(DBG_info, "%s: no more document\n", __func__);
      dev->document = SANE_FALSE;

        unsigned channels = dev->session.params.channels;
        unsigned depth = dev->session.params.depth;
      read_bytes_left = (int) dev->read_bytes_left_after_deseg;
      DBG(DBG_io, "%s: read_bytes_left=%d\n", __func__, read_bytes_left);

        // get lines read
        try {
            sanei_genesys_read_scancnt(dev, &scancnt);
        } catch (...) {
            flines = 0;
        }

	  /* compute number of line read */
	  tmp = (int) dev->total_bytes_read;
          if (depth == 1 || dev->settings.scan_mode == ScanColorMode::LINEART)
	    flines = tmp * 8 / dev->settings.pixels / channels;
	  else
	    flines = tmp / (depth / 8) / dev->settings.pixels / channels;

	  /* number of scanned lines, but no read yet */
	  flines = scancnt - flines;

	  DBG(DBG_io, "%s: %d scanned but not read lines\n", __func__, flines);

        // Adjust number of bytes to read. We need to read the final bytes which are word per
        // line times number of last lines to have doc leaving feeder
        lines = (SANE_UNFIX(dev->model->post_scan) * dev->session.params.yres) / MM_PER_INCH +
            flines;

      DBG(DBG_io, "%s: adding %d line to flush\n", __func__, lines);

        // number of bytes to read from scanner to get document out of it after
        // end of document dectected by hardware sensor */
        bytes_to_flush = lines * dev->session.output_line_bytes_raw;

      /* if we are already close to end of scan, flushing isn't needed */
      if (bytes_to_flush < read_bytes_left)
	{
	  /* we take all these step to work around an overflow on some plateforms */
	  tmp = (int) dev->total_bytes_read;
	  DBG (DBG_io, "%s: tmp=%d\n", __func__, tmp);
	  bytes_remain = (int) dev->total_bytes_to_read;
	  DBG(DBG_io, "%s: bytes_remain=%d\n", __func__, bytes_remain);
	  bytes_remain = bytes_remain - tmp;
	  DBG(DBG_io, "%s: bytes_remain=%d\n", __func__, bytes_remain);

	  /* remaining lines to read by frontend for the current scan */
          if (depth == 1 || dev->settings.scan_mode == ScanColorMode::LINEART)
	    {
	      flines = bytes_remain * 8 / dev->settings.pixels / channels;
	    }
	  else
	    flines = bytes_remain / (depth / 8)
	      / dev->settings.pixels / channels;
	  DBG(DBG_io, "%s: flines=%d\n", __func__, flines);

	  if (flines > lines)
	    {
	      /* change the value controlling communication with the frontend :
	       * total bytes to read is current value plus the number of remaining lines
	       * multiplied by bytes per line */
	      sublines = flines - lines;

              if (depth == 1 || dev->settings.scan_mode == ScanColorMode::LINEART)
		sub_bytes =
		  ((dev->settings.pixels * sublines) / 8 +
		   (((dev->settings.pixels * sublines) % 8) ? 1 : 0)) *
		  channels;
	      else
		sub_bytes =
		  dev->settings.pixels * sublines * channels * (depth / 8);

	      dev->total_bytes_to_read -= sub_bytes;

          /* then adjust the desegmented bytes to read */
            if (read_bytes_left > sub_bytes) {
                dev->read_bytes_left_after_deseg -= sub_bytes;
            } else {
                dev->total_bytes_to_read = dev->total_bytes_read;
                dev->read_bytes_left_after_deseg = 0;
            }

	      DBG(DBG_io, "%s: sublines=%d\n", __func__, sublines);
	      DBG(DBG_io, "%s: subbytes=%d\n", __func__, sub_bytes);
	      DBG(DBG_io, "%s: total_bytes_to_read=%lu\n", __func__,
		  (unsigned long) dev->total_bytes_to_read);
	      DBG(DBG_io, "%s: read_bytes_left=%d\n", __func__, read_bytes_left);
	    }
	}
      else
	{
	  DBG(DBG_io, "%s: no flushing needed\n", __func__);
	}
    }
}

// enables or disables XPA slider motor
static void gl843_set_xpa_motor_power(Genesys_Device* dev, bool set)
{
    DBG_HELPER(dbg);
    uint8_t val;

    if (dev->model->model_id == MODEL_CANON_CANOSCAN_8400F) {

        if (set) {
            val = dev->read_register(0x6c);
            val &= ~(REG6C_GPIO16 | REG6C_GPIO13);
            if (dev->current_setup.xres >= 2400) {
                val &= ~REG6C_GPIO10;
            }
            dev->write_register(0x6c, val);

            val = dev->read_register(0xa9);
            val |= REGA9_GPO30;
            val &= ~REGA9_GPO29;
            dev->write_register(0xa9, val);
        } else {
            val = dev->read_register(0x6c);
            val |= REG6C_GPIO16 | REG6C_GPIO13;
            dev->write_register(0x6c, val);

            val = dev->read_register(0xa9);
            val &= ~REGA9_GPO30;
            val |= REGA9_GPO29;
            dev->write_register(0xa9, val);
        }
    } else if (dev->model->model_id == MODEL_CANON_CANOSCAN_8600F) {
        if (set) {
            val = dev->read_register(REG6C);
            val &= ~REG6C_GPIO14;
            if (dev->current_setup.xres >= 2400) {
                val |= REG6C_GPIO10;
            }
            dev->write_register(REG6C, val);

            val = dev->read_register(REGA6);
            val |= REGA6_GPIO17;
            val &= ~REGA6_GPIO23;
            dev->write_register(REGA6, val);
        } else {
            val = dev->read_register(REG6C);
            val |= REG6C_GPIO14;
            val &= ~REG6C_GPIO10;
            dev->write_register(REG6C, val);

            val = dev->read_register(REGA6);
            val &= ~REGA6_GPIO17;
            val &= ~REGA6_GPIO23;
            dev->write_register(REGA6, val);
        }
    } else if (dev->model->model_id == MODEL_HP_SCANJET_G4050) {
        if (set) {
            // set MULTFILM et GPOADF
            val = dev->read_register(REG6B);
            val |=REG6B_MULTFILM|REG6B_GPOADF;
            dev->write_register(REG6B, val);

            val = dev->read_register(REG6C);
            val &= ~REG6C_GPIO15;
            dev->write_register(REG6C, val);

            /* Motor power ? No move at all without this one */
            val = dev->read_register(REGA6);
            val |= REGA6_GPIO20;
            dev->write_register(REGA6, val);

            val = dev->read_register(REGA8);
            val &= ~REGA8_GPO27;
            dev->write_register(REGA8, val);

            val = dev->read_register(REGA9);
            val |= REGA9_GPO32|REGA9_GPO31;
            dev->write_register(REGA9, val);
        } else {
            // unset GPOADF
            val = dev->read_register(REG6B);
            val &= ~REG6B_GPOADF;
            dev->write_register(REG6B, val);

            val = dev->read_register(REGA8);
            val |= REGA8_GPO27;
            dev->write_register(REGA8, val);

            val = dev->read_register(REGA9);
            val &= ~REGA9_GPO31;
            dev->write_register(REGA9, val);
        }
    }
}


/** @brief light XPA lamp
 * toggle gpios to switch off regular lamp and light on the
 * XPA light
 * @param dev device to set up
 */
static void gl843_set_xpa_lamp_power(Genesys_Device* dev, bool set)
{
    DBG_HELPER(dbg);

    struct LampSettings {
        unsigned model_id;
        ScanMethod scan_method;
        GenesysRegisterSettingSet regs_on;
        GenesysRegisterSettingSet regs_off;
    };

    // FIXME: BUG: we're not clearing the registers to the previous state when returning back when
    // turning off the lamp
    LampSettings settings[] = {
        {   MODEL_CANON_CANOSCAN_8400F, ScanMethod::TRANSPARENCY, {
                { 0xa6, 0x34, 0xf4 },
                { 0xa7, 0xe0, 0xe0 }, // BUG: should be 0x03
            }, {
                { 0xa6, 0x40, 0x70 },
            }
        },
        {   MODEL_CANON_CANOSCAN_8400F, ScanMethod::TRANSPARENCY_INFRARED, {
                { 0x6c, 0x40, 0x40 },
                { 0xa6, 0x01, 0xff },
                { 0xa7, 0x03, 0x07 },
            }, {
                { 0x6c, 0x00, 0x40 },
                { 0xa6, 0x00, 0xff },
                { 0xa7, 0x07, 0x07 },
            }
        },
        {   MODEL_CANON_CANOSCAN_8600F, ScanMethod::TRANSPARENCY, {
                { 0xa6, 0x34, 0xf4 },
                { 0xa7, 0xe0, 0xe0 },
            }, {
                { 0xa6, 0x40, 0x70 },
            }
        },
        {   MODEL_CANON_CANOSCAN_8600F, ScanMethod::TRANSPARENCY_INFRARED, {
                { 0xa6, 0x00, 0xc0 },
                { 0xa7, 0xe0, 0xe0 },
                { 0x6c, 0x80, 0x80 },
            }, {
                { 0xa6, 0x00, 0xc0 },
                { 0x6c, 0x00, 0x80 },
            }
        },
    };

    for (const auto& setting : settings) {
        if (setting.model_id == dev->model->model_id &&
            setting.scan_method == dev->settings.scan_method)
        {
            apply_reg_settings_to_device(*dev, set ? setting.regs_on : setting.regs_off);
            return;
        }
    }

    // BUG: we're currently calling the function in shut down path of regular lamp
    if (set) {
        throw SaneException("Unexpected code path entered");
    }

    GenesysRegisterSettingSet regs = {
        { 0xa6, 0x40, 0x70 },
    };
    apply_reg_settings_to_device(*dev, regs);
    // TODO: throw exception when we're only calling this function in error return path
    // throw SaneException("Could not find XPA lamp settings");
}

// Send the low-level scan command
static void gl843_begin_scan(Genesys_Device* dev, const Genesys_Sensor& sensor,
                             Genesys_Register_Set* reg, SANE_Bool start_motor)
{
    DBG_HELPER(dbg);
    (void) sensor;

  /* set up GPIO for scan */
  switch(dev->model->gpo_type)
    {
      /* KV case */
      case GPO_KVSS080:
            dev->write_register(REGA9, 0x00);
            dev->write_register(REGA6, 0xf6);
            // blinking led
            dev->write_register(0x7e, 0x04);
            break;
      case GPO_G4050:
            dev->write_register(REGA7, 0xfe);
            dev->write_register(REGA8, 0x3e);
            dev->write_register(REGA9, 0x06);
            if ((reg->get8(0x05) & REG05_DPIHW) == REG05_DPIHW_600) {
                dev->write_register(REG6C, 0x20);
                dev->write_register(REGA6, 0x44);
            } else {
                dev->write_register(REG6C, 0x60);
                dev->write_register(REGA6, 0x46);
            }

            if (reg->state.is_xpa_on && reg->state.is_lamp_on) {
                gl843_set_xpa_lamp_power(dev, true);
            }

            if (reg->state.is_xpa_on) {
                dev->needs_home_ta = SANE_TRUE;
                gl843_set_xpa_motor_power(dev, true);
            }

            // blinking led
            dev->write_register(REG7E, 0x01);
        break;
      case GPO_CS8400F:
      case GPO_CS8600F:
            if (reg->state.is_xpa_on && reg->state.is_lamp_on) {
                gl843_set_xpa_lamp_power(dev, true);
            }
            if (reg->state.is_xpa_on) {
                dev->needs_home_ta = SANE_TRUE;
                gl843_set_xpa_motor_power(dev, true);
            }
        break;
      case GPO_CS4400F:
      default:
        break;
    }

    // clear scan and feed count
    dev->write_register(REG0D, REG0D_CLRLNCNT | REG0D_CLRMCNT);

    // enable scan and motor
    uint8_t val = dev->read_register(REG01);
    val |= REG01_SCAN;
    dev->write_register(REG01, val);

    if (start_motor) {
        dev->write_register(REG0F, 1);
    } else {
        dev->write_register(REG0F, 0);
    }
}


// Send the stop scan command
static void gl843_end_scan(Genesys_Device* dev, Genesys_Register_Set* reg, SANE_Bool check_stop)
{
    DBG_HELPER_ARGS(dbg, "check_stop = %d", check_stop);

    // post scan gpio
    dev->write_register(0x7e, 0x00);

    // turn off XPA lamp if needed
    // BUG: the if condition below probably shouldn't be enabled when XPA is off
    if (reg->state.is_xpa_on || reg->state.is_lamp_on) {
        gl843_set_xpa_lamp_power(dev, false);
    }

    if (dev->model->is_sheetfed != SANE_TRUE) {
        gl843_stop_action(dev);
    }
}

/** @brief park XPA lamp
 * park the XPA lamp if needed
 */
static void gl843_park_xpa_lamp(Genesys_Device* dev)
{
    DBG_HELPER(dbg);
  Genesys_Register_Set local_reg;
  GenesysRegister *r;
  uint8_t val;
  int loop = 0;

  /* copy scan settings */
  local_reg = dev->reg;

  /* set a huge feedl and reverse direction */
    local_reg.set24(REG_FEEDL, 0xbdcd);

    // clear scan and feed count
    dev->write_register(REG0D, REG0D_CLRLNCNT | REG0D_CLRMCNT);

  /* set up for reverse and no scan */
  r = sanei_genesys_get_address (&local_reg, REG02);
  r->value |= REG02_MTRREV;
  r = sanei_genesys_get_address (&local_reg, REG01);
  r->value &= ~REG01_SCAN;

    // write to scanner and start action
    dev->write_registers(local_reg);
    gl843_set_xpa_motor_power(dev, true);
    try {
        gl843_start_action(dev);
    } catch (...) {
        catch_all_exceptions(__func__, [&]() { gl843_stop_action(dev); });
        // restore original registers
        catch_all_exceptions(__func__, [&]()
        {
            dev->write_registers(dev->reg);
        });
        throw;
    }

      while (loop < 600)	/* do not wait longer then 60 seconds */
	{
            sanei_genesys_get_status(dev, &val);

          if (DBG_LEVEL >= DBG_io2)
            {
              sanei_genesys_print_status (val);
            }

	  if (val & REG41_HOMESNR)	/* home sensor */
	    {
	      DBG(DBG_info, "%s: reached home position\n", __func__);
	      DBG(DBG_proc, "%s: finished\n", __func__);

            gl843_set_xpa_motor_power(dev, false);
            dev->needs_home_ta = SANE_FALSE;

            return;
	    }
          sanei_genesys_sleep_ms(100);
	  ++loop;
	}

  /* we are not parked here.... should we fail ? */
  DBG(DBG_info, "%s: XPA lamp is not parked\n", __func__);
}

/** @brief Moves the slider to the home (top) position slowly
 * */
static void gl843_slow_back_home(Genesys_Device* dev, SANE_Bool wait_until_home)
{
    DBG_HELPER_ARGS(dbg, "wait_until_home = %d", wait_until_home);
  Genesys_Register_Set local_reg;
  GenesysRegister *r;
  uint8_t val;
  float resolution;
  int loop = 0;

    if (dev->needs_home_ta) {
        gl843_park_xpa_lamp(dev);
    }

  /* regular slow back home */
  dev->scanhead_position_in_steps = 0;

    // first read gives HOME_SENSOR true
    sanei_genesys_get_status(dev, &val);

  sanei_genesys_sleep_ms(100);

    // second is reliable
    sanei_genesys_get_status(dev, &val);

  if (DBG_LEVEL >= DBG_io)
    {
      sanei_genesys_print_status (val);
    }
  if (val & HOMESNR)	/* is sensor at home? */
    {
      return;
    }

  local_reg = dev->reg;
  resolution=sanei_genesys_get_lowest_ydpi(dev);

    const auto& sensor = sanei_genesys_find_sensor(dev, resolution, 1, dev->model->default_method);

    ScanSession session;
    session.params.xres = resolution;
    session.params.yres = resolution;
    session.params.startx = 100;
    session.params.starty = 40000;
    session.params.pixels = 100;
    session.params.lines = 100;
    session.params.depth = 8;
    session.params.channels = 1;
    session.params.scan_method = dev->settings.scan_method;
    session.params.scan_mode = ScanColorMode::LINEART;
    session.params.color_filter = dev->settings.color_filter;
    session.params.flags =  SCAN_FLAG_DISABLE_SHADING |
                            SCAN_FLAG_DISABLE_GAMMA |
                            SCAN_FLAG_DISABLE_BUFFER_FULL_MOVE |
                            SCAN_FLAG_IGNORE_LINE_DISTANCE;
    gl843_compute_session(dev, session, sensor);

    gl843_init_scan_regs(dev, sensor, &local_reg, session);

    // clear scan and feed count
    dev->write_register(REG0D, REG0D_CLRLNCNT | REG0D_CLRMCNT);

  /* set up for reverse and no scan */
  r = sanei_genesys_get_address(&local_reg, REG02);
  r->value |= REG02_MTRREV;
  r = sanei_genesys_get_address(&local_reg, REG01);
  r->value &= ~REG01_SCAN;

    dev->write_registers(local_reg);

    try {
        gl843_start_action(dev);
    } catch (...) {
        catch_all_exceptions(__func__, [&]() { gl843_stop_action(dev); });
        // restore original registers
        catch_all_exceptions(__func__, [&]()
        {
            dev->write_registers(dev->reg);
        });
        throw;
    }

  if (wait_until_home)
    {

      while (loop < 300)	/* do not wait longer then 30 seconds */
	{
            sanei_genesys_get_status(dev, &val);

          if (DBG_LEVEL >= DBG_io2)
            {
              sanei_genesys_print_status (val);
            }

	  if (val & REG41_HOMESNR)	/* home sensor */
	    {
	      DBG(DBG_info, "%s: reached home position\n", __func__);
	      DBG(DBG_proc, "%s: finished\n", __func__);
          return;
	    }
          sanei_genesys_sleep_ms(100);
	  ++loop;
	}

        // when we come here then the scanner needed too much time for this, so we better stop
        // the motor
        catch_all_exceptions(__func__, [&](){ gl843_stop_action(dev); });
        throw SaneException(SANE_STATUS_IO_ERROR, "timeout while waiting for scanhead to go home");
    }

  DBG(DBG_info, "%s: scanhead is still moving\n", __func__);
}

// Automatically set top-left edge of the scan area by scanning a 200x200 pixels area at 600 dpi
// from very top of scanner
static void gl843_search_start_position(Genesys_Device* dev)
{
    DBG_HELPER(dbg);
  int size;
  Genesys_Register_Set local_reg;
  int steps;

  int pixels = 600;
  int dpi = 300;

  local_reg = dev->reg;

  /* sets for a 200 lines * 600 pixels */
  /* normal scan with no shading */

    // FIXME: the current approach of doing search only for one resolution does not work on scanners
    // whith employ different sensors with potentially different settings.
    const auto& sensor = sanei_genesys_find_sensor(dev, dpi, 1, dev->model->default_method);

    ScanSession session;
    session.params.xres = dpi;
    session.params.yres = dpi;
    session.params.startx = 0;
    session.params.starty = 0; // we should give a small offset here - ~60 steps
    session.params.pixels = 600;
    session.params.lines = dev->model->search_lines;
    session.params.depth = 8;
    session.params.channels = 1;
    session.params.scan_method = dev->settings.scan_method;
    session.params.scan_mode = ScanColorMode::GRAY;
    session.params.color_filter = ColorFilter::GREEN;
    session.params.flags =  SCAN_FLAG_DISABLE_SHADING |
                            SCAN_FLAG_DISABLE_GAMMA |
                            SCAN_FLAG_IGNORE_LINE_DISTANCE |
                            SCAN_FLAG_DISABLE_BUFFER_FULL_MOVE;
    gl843_compute_session(dev, session, sensor);

    gl843_init_scan_regs(dev, sensor, &local_reg, session);

    // send to scanner
    dev->write_registers(local_reg);

    size = dev->read_bytes_left_after_deseg;

  std::vector<uint8_t> data(size);

    gl843_begin_scan(dev, sensor, &local_reg, SANE_TRUE);

        // waits for valid data
        do {
            sanei_genesys_test_buffer_empty(dev, &steps);
        } while (steps);

    // now we're on target, we can read data
    sanei_genesys_read_data_from_scanner(dev, data.data(), size);

    gl843_stop_action_no_move(dev, &local_reg);

    if (DBG_LEVEL >= DBG_data) {
        sanei_genesys_write_pnm_file("gl843_search_position.pnm", data.data(), 8, 1, pixels,
                                     dev->model->search_lines);
    }

    gl843_end_scan(dev, &local_reg, SANE_TRUE);

  /* update regs to copy ASIC internal state */
  dev->reg = local_reg;

    for (auto& sensor_update :
            sanei_genesys_find_sensors_all_for_write(dev, dev->model->default_method))
    {
        sanei_genesys_search_reference_point(dev, sensor_update, data.data(), 0, dpi, pixels,
                                             dev->model->search_lines);
    }
}

// sets up register for coarse gain calibration
// todo: check it for scanners using it
static void gl843_init_regs_for_coarse_calibration(Genesys_Device* dev,
                                                   const Genesys_Sensor& sensor,
                                                   Genesys_Register_Set& regs)
{
    DBG_HELPER(dbg);

  int flags = SCAN_FLAG_DISABLE_SHADING |
              SCAN_FLAG_DISABLE_GAMMA |
              SCAN_FLAG_SINGLE_LINE |
              SCAN_FLAG_IGNORE_LINE_DISTANCE;

    if (dev->settings.scan_method == ScanMethod::TRANSPARENCY ||
        dev->settings.scan_method == ScanMethod::TRANSPARENCY_INFRARED) {
        flags |= SCAN_FLAG_USE_XPA;
    }

    ScanSession session;
    session.params.xres = dev->settings.xres;
    session.params.yres = dev->settings.yres;
    session.params.startx = 0;
    session.params.starty = 0;
    session.params.pixels = sensor.optical_res / sensor.ccd_pixels_per_system_pixel();
    session.params.lines = 20;
    session.params.depth = 16;
    session.params.channels = dev->settings.get_channels();
    session.params.scan_method = dev->settings.scan_method;
    session.params.scan_mode = dev->settings.scan_mode;
    session.params.color_filter = dev->settings.color_filter;
    session.params.flags = flags;
    gl843_compute_session(dev, session, sensor);

    gl843_init_scan_regs(dev, sensor, &regs, session);

  sanei_genesys_set_motor_power(regs, false);

  DBG(DBG_info, "%s: optical sensor res: %d dpi, actual res: %d\n", __func__,
      sensor.optical_res / sensor.ccd_pixels_per_system_pixel(), dev->settings.xres);

    dev->write_registers(regs);
}

/** @brief moves the slider to steps at motor base dpi
 * @param dev device to work on
 * @param steps number of steps to move
 * */
static void gl843_feed(Genesys_Device* dev, unsigned int steps)
{
    DBG_HELPER(dbg);
  Genesys_Register_Set local_reg;
  GenesysRegister *r;
  float resolution;
  uint8_t val;

  /* prepare local registers */
  local_reg = dev->reg;

  resolution=sanei_genesys_get_lowest_ydpi(dev);

    const auto& sensor = sanei_genesys_find_sensor(dev, resolution, 3, dev->model->default_method);

    ScanSession session;
    session.params.xres = resolution;
    session.params.yres = resolution;
    session.params.startx = 0;
    session.params.starty = steps;
    session.params.pixels = 100;
    session.params.lines = 3;
    session.params.depth = 8;
    session.params.channels = 3;
    session.params.scan_method = dev->settings.scan_method;
    session.params.scan_mode = ScanColorMode::COLOR_SINGLE_PASS;
    session.params.color_filter = ColorFilter::RED;
    session.params.flags =  SCAN_FLAG_DISABLE_SHADING |
                            SCAN_FLAG_DISABLE_GAMMA |
                            SCAN_FLAG_FEEDING |
                            SCAN_FLAG_IGNORE_LINE_DISTANCE;
    gl843_compute_session(dev, session, sensor);

    gl843_init_scan_regs(dev, sensor, &local_reg, session);

    // clear scan and feed count
    dev->write_register(REG0D, REG0D_CLRLNCNT);
    dev->write_register(REG0D, REG0D_CLRMCNT);

  /* set up for no scan */
  r = sanei_genesys_get_address(&local_reg, REG01);
  r->value &= ~REG01_SCAN;

    // send registers
    dev->write_registers(local_reg);

    try {
        gl843_start_action(dev);
    } catch (...) {
        catch_all_exceptions(__func__, [&]() { gl843_stop_action(dev); });
        // restore original registers
        catch_all_exceptions(__func__, [&]()
        {
            dev->write_registers(dev->reg);
        });
        throw;
    }

    // wait until feed count reaches the required value, but do not exceed 30s
    do {
        sanei_genesys_get_status(dev, &val);
    } while (!(val & FEEDFSH));

  // looks like the scanner locks up if we scan immediately after feeding
  sanei_genesys_sleep_ms(100);
}

static void gl843_move_to_ta(Genesys_Device* dev);

// init registers for shading calibration shading calibration is done at dpihw
static void gl843_init_regs_for_shading(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                        Genesys_Register_Set& regs)
{
    DBG_HELPER(dbg);
  int move, resolution, dpihw, factor;

  /* initial calibration reg values */
  regs = dev->reg;

  dev->calib_channels = 3;

    if (dev->settings.scan_method == ScanMethod::TRANSPARENCY ||
        dev->settings.scan_method == ScanMethod::TRANSPARENCY_INFRARED)
    {
        dev->calib_lines = dev->model->shading_ta_lines;
    } else {
        dev->calib_lines = dev->model->shading_lines;
    }

    dpihw = sensor.get_logical_hwdpi(dev->settings.xres);
  factor=sensor.optical_res/dpihw;
  resolution=dpihw;

  const auto& calib_sensor = sanei_genesys_find_sensor(dev, resolution, dev->calib_channels,
                                                       dev->settings.scan_method);

    if ((dev->settings.scan_method == ScanMethod::TRANSPARENCY ||
         dev->settings.scan_method == ScanMethod::TRANSPARENCY_INFRARED) &&
        dev->model->model_id == MODEL_CANON_CANOSCAN_8600F &&
        dev->settings.xres == 4800)
    {
        float offset = SANE_UNFIX(dev->model->x_offset_ta);
        offset /= calib_sensor.get_ccd_size_divisor_for_dpi(resolution);
        offset = (offset * calib_sensor.optical_res) / MM_PER_INCH;

        float size = SANE_UNFIX(dev->model->x_size_ta);
        size /= calib_sensor.get_ccd_size_divisor_for_dpi(resolution);
        size = (size * calib_sensor.optical_res) / MM_PER_INCH;

        dev->calib_pixels_offset = offset;
        dev->calib_pixels = size;
    }
    else
    {
        dev->calib_pixels_offset = 0;
        dev->calib_pixels = calib_sensor.sensor_pixels / factor;
    }

  dev->calib_resolution = resolution;

  int flags = SCAN_FLAG_DISABLE_SHADING |
              SCAN_FLAG_DISABLE_GAMMA |
              SCAN_FLAG_DISABLE_BUFFER_FULL_MOVE |
              SCAN_FLAG_IGNORE_LINE_DISTANCE;

    if (dev->settings.scan_method == ScanMethod::TRANSPARENCY ||
        dev->settings.scan_method == ScanMethod::TRANSPARENCY_INFRARED)
    {
        // note: move_to_ta() function has already been called and the sensor is at the
        // transparency adapter
        move = SANE_UNFIX(dev->model->y_offset_calib_ta) -
               SANE_UNFIX(dev->model->y_offset_sensor_to_ta);
    flags |= SCAN_FLAG_USE_XPA;
  }
  else
    move = SANE_UNFIX(dev->model->y_offset_calib);

  move = (move * resolution) / MM_PER_INCH;

    ScanSession session;
    session.params.xres = resolution;
    session.params.yres = resolution;
    session.params.startx = dev->calib_pixels_offset;
    session.params.starty = move;
    session.params.pixels = dev->calib_pixels;
    session.params.lines = dev->calib_lines;
    session.params.depth = 16;
    session.params.channels = dev->calib_channels;
    session.params.scan_method = dev->settings.scan_method;
    session.params.scan_mode = dev->settings.scan_mode;
    session.params.color_filter = dev->settings.color_filter;
    session.params.flags = flags;
    gl843_compute_session(dev, session, calib_sensor);

    gl843_init_scan_regs(dev, calib_sensor, &regs, session);

  // the pixel number may be updated to conform to scanner constraints
  dev->calib_pixels = dev->current_setup.pixels;

    dev->calib_total_bytes_to_read = dev->read_bytes_left_after_deseg;

  dev->scanhead_position_in_steps += dev->calib_lines + move;

    dev->write_registers(regs);
}

/** @brief set up registers for the actual scan
 */
static void gl843_init_regs_for_scan(Genesys_Device* dev, const Genesys_Sensor& sensor)
{
    DBG_HELPER(dbg);
  int flags;
  float move;
  int move_dpi;
  float start;

    debug_dump(DBG_info, dev->settings);

  move_dpi = dev->motor.base_ydpi;

  flags = 0;

    if (dev->settings.scan_method == ScanMethod::TRANSPARENCY ||
        dev->settings.scan_method == ScanMethod::TRANSPARENCY_INFRARED)
    {
        // note: move_to_ta() function has already been called and the sensor is at the
        // transparency adapter
        if (dev->ignore_offsets) {
            move = 0;
        } else {
            move = SANE_UNFIX(dev->model->y_offset_ta) -
                   SANE_UNFIX(dev->model->y_offset_sensor_to_ta);
        }
        flags |= SCAN_FLAG_USE_XPA;
    } else {
        if (dev->ignore_offsets) {
            move = 0;
        } else {
            move = SANE_UNFIX(dev->model->y_offset);
        }
    }

  move += dev->settings.tl_y;
  move = (move * move_dpi) / MM_PER_INCH;
  DBG(DBG_info, "%s: move=%f steps\n", __func__, move);

  /* start */
    if (dev->settings.scan_method==ScanMethod::TRANSPARENCY ||
        dev->settings.scan_method == ScanMethod::TRANSPARENCY_INFRARED)
    {
        start = SANE_UNFIX(dev->model->x_offset_ta);
    } else {
        start = SANE_UNFIX(dev->model->x_offset);
    }

    if (dev->model->model_id == MODEL_CANON_CANOSCAN_8400F ||
        dev->model->model_id == MODEL_CANON_CANOSCAN_8600F)
    {
        // FIXME: this is probably just an artifact of a bug elsewhere
        start /= sensor.get_ccd_size_divisor_for_dpi(dev->settings.xres);
    }

  start += dev->settings.tl_x;
  start = (start * sensor.optical_res) / MM_PER_INCH;

  /* enable emulated lineart from gray data */
  if(dev->settings.scan_mode == ScanColorMode::LINEART
     && dev->settings.dynamic_lineart)
    {
      flags |= SCAN_FLAG_DYNAMIC_LINEART;
    }

    ScanSession session;
    session.params.xres = dev->settings.xres;
    session.params.yres = dev->settings.yres;
    session.params.startx = start;
    session.params.starty = move;
    session.params.pixels = dev->settings.pixels;
    session.params.requested_pixels = dev->settings.requested_pixels;
    session.params.lines = dev->settings.lines;
    session.params.depth = dev->settings.get_depth();
    session.params.channels = dev->settings.get_channels();
    session.params.scan_method = dev->settings.scan_method;
    session.params.scan_mode = dev->settings.scan_mode;
    session.params.color_filter = dev->settings.color_filter;
    session.params.flags = flags;
    gl843_compute_session(dev, session, sensor);

    gl843_init_scan_regs(dev, sensor, &dev->reg, session);
}

/**
 * This function sends gamma tables to ASIC
 */
static void gl843_send_gamma_table(Genesys_Device* dev, const Genesys_Sensor& sensor)
{
    DBG_HELPER(dbg);
  int size;
  int i;

  size = 256;

  /* allocate temporary gamma tables: 16 bits words, 3 channels */
  std::vector<uint8_t> gamma(size * 2 * 3);

    std::vector<uint16_t> rgamma = get_gamma_table(dev, sensor, GENESYS_RED);
    std::vector<uint16_t> ggamma = get_gamma_table(dev, sensor, GENESYS_GREEN);
    std::vector<uint16_t> bgamma = get_gamma_table(dev, sensor, GENESYS_BLUE);

    // copy sensor specific's gamma tables
    for (i = 0; i < size; i++) {
        gamma[i * 2 + size * 0 + 0] = rgamma[i] & 0xff;
        gamma[i * 2 + size * 0 + 1] = (rgamma[i] >> 8) & 0xff;
        gamma[i * 2 + size * 2 + 0] = ggamma[i] & 0xff;
        gamma[i * 2 + size * 2 + 1] = (ggamma[i] >> 8) & 0xff;
        gamma[i * 2 + size * 4 + 0] = bgamma[i] & 0xff;
        gamma[i * 2 + size * 4 + 1] = (bgamma[i] >> 8) & 0xff;
    }

    // send address
    gl843_set_buffer_address(dev, 0x0000);

    // send data
    sanei_genesys_bulk_write_data(dev, 0x28, gamma.data(), size * 2 * 3);
}

/* this function does the led calibration by scanning one line of the calibration
   area below scanner's top on white strip.

-needs working coarse/gain
*/
static SensorExposure gl843_led_calibration(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                            Genesys_Register_Set& regs)
{
    DBG_HELPER(dbg);
  int num_pixels;
  int total_size;
  int i, j;
  int val;
  int channels, depth;
  int avg[3], avga, avge;
  int turn;
  uint16_t expr, expg, expb;

  SANE_Bool acceptable = SANE_FALSE;

  /* offset calibration is always done in color mode */
  channels = 3;
  depth = 16;

    // take a copy, as we're going to modify exposure
    auto calib_sensor = sanei_genesys_find_sensor(dev, sensor.optical_res, channels,
                                                  dev->settings.scan_method);

    num_pixels = (calib_sensor.sensor_pixels * calib_sensor.optical_res) / calib_sensor.optical_res;

  /* initial calibration reg values */
  regs = dev->reg;

    ScanSession session;
    session.params.xres = calib_sensor.sensor_pixels;
    session.params.yres = dev->motor.base_ydpi;
    session.params.startx = 0;
    session.params.starty = 0;
    session.params.pixels = num_pixels;
    session.params.lines = 1;
    session.params.depth = depth;
    session.params.channels = channels;
    session.params.scan_method = dev->settings.scan_method;
    session.params.scan_mode = ScanColorMode::COLOR_SINGLE_PASS;
    session.params.color_filter = dev->settings.color_filter;
    session.params.flags =  SCAN_FLAG_DISABLE_SHADING |
                            SCAN_FLAG_DISABLE_GAMMA |
                            SCAN_FLAG_SINGLE_LINE |
                            SCAN_FLAG_IGNORE_LINE_DISTANCE;
    gl843_compute_session(dev, session, calib_sensor);

    gl843_init_scan_regs(dev, calib_sensor, &regs, session);

    dev->write_registers(regs);

    total_size = dev->read_bytes_left_after_deseg;

  std::vector<uint8_t> line(total_size);

/*
   we try to get equal bright leds here:

   loop:
     average per color
     adjust exposure times
 */

  expr = calib_sensor.exposure.red;
  expg = calib_sensor.exposure.green;
  expb = calib_sensor.exposure.blue;

  turn = 0;

  do
    {

      calib_sensor.exposure.red = expr;
      calib_sensor.exposure.green = expg;
      calib_sensor.exposure.blue = expb;

      sanei_genesys_set_exposure(regs, calib_sensor.exposure);

        dev->write_registers(regs);

      DBG(DBG_info, "%s: starting first line reading\n", __func__);
        gl843_begin_scan(dev, calib_sensor, &regs, SANE_TRUE);
        sanei_genesys_read_data_from_scanner(dev, line.data(), total_size);
        gl843_stop_action_no_move(dev, &regs);

      if (DBG_LEVEL >= DBG_data)
	{
          char fn[30];
          snprintf(fn, 30, "gl843_led_%02d.pnm", turn);
          sanei_genesys_write_pnm_file(fn, line.data(), depth, channels, num_pixels, 1);
	}

      acceptable = SANE_TRUE;

      for (j = 0; j < channels; j++)
	{
	  avg[j] = 0;
	  for (i = 0; i < num_pixels; i++)
	    {
	      if (dev->model->is_cis)
		val =
		  line[i * 2 + j * 2 * num_pixels + 1] * 256 +
		  line[i * 2 + j * 2 * num_pixels];
	      else
		val =
		  line[i * 2 * channels + 2 * j + 1] * 256 +
		  line[i * 2 * channels + 2 * j];
	      avg[j] += val;
	    }

	  avg[j] /= num_pixels;
	}

      DBG(DBG_info, "%s: average: %d,%d,%d\n", __func__, avg[0], avg[1], avg[2]);

      acceptable = SANE_TRUE;

      if (avg[0] < avg[1] * 0.95 || avg[1] < avg[0] * 0.95 ||
	  avg[0] < avg[2] * 0.95 || avg[2] < avg[0] * 0.95 ||
	  avg[1] < avg[2] * 0.95 || avg[2] < avg[1] * 0.95)
	acceptable = SANE_FALSE;

      if (!acceptable)
	{
	  avga = (avg[0] + avg[1] + avg[2]) / 3;
	  expr = (expr * avga) / avg[0];
	  expg = (expg * avga) / avg[1];
	  expb = (expb * avga) / avg[2];
/*
  keep the resulting exposures below this value.
  too long exposure drives the ccd into saturation.
  we may fix this by relying on the fact that
  we get a striped scan without shading, by means of
  statistical calculation
*/
	  avge = (expr + expg + expb) / 3;

	  /* don't overflow max exposure */
	  if (avge > 3000)
	    {
	      expr = (expr * 2000) / avge;
	      expg = (expg * 2000) / avge;
	      expb = (expb * 2000) / avge;
	    }
	  if (avge < 50)
	    {
	      expr = (expr * 50) / avge;
	      expg = (expg * 50) / avge;
	      expb = (expb * 50) / avge;
	    }

	}

        gl843_stop_action (dev);

      turn++;

    }
  while (!acceptable && turn < 100);

  DBG(DBG_info, "%s: acceptable exposure: %d,%d,%d\n", __func__, expr, expg, expb);

  gl843_slow_back_home (dev, SANE_TRUE);

    return calib_sensor.exposure;
}



/**
 * average dark pixels of a 8 bits scan of a given channel
 */
static int
dark_average_channel (uint8_t * data, unsigned int pixels, unsigned int lines,
	      unsigned int channels, unsigned int black, int channel)
{
  unsigned int i, j, k, count;
  unsigned int avg[3];
  uint8_t val;

  /* computes average values on black margin */
  for (k = 0; k < channels; k++)
    {
      avg[k] = 0;
      count = 0;
      // FIXME: start with the second line because the black pixels often have noise on the first
      // line; the cause is probably incorrectly cleaned up previous scan
      for (i = 1; i < lines; i++)
	{
	  for (j = 0; j < black; j++)
	    {
	      val = data[i * channels * pixels + j*channels + k];
	      avg[k] += val;
	      count++;
	    }
	}
      if (count)
	avg[k] /= count;
      DBG(DBG_info, "%s: avg[%d] = %d\n", __func__, k, avg[k]);
    }
  DBG(DBG_info, "%s: average = %d\n", __func__, avg[channel]);
  return avg[channel];
}

/** @brief calibrate AFE offset
 * Iterate doing scans at target dpi until AFE offset if correct. One
 * color line is scanned at a time. Scanning head doesn't move.
 * @param dev device to calibrate
 */
static void gl843_offset_calibration(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                     Genesys_Register_Set& regs)
{
    DBG_HELPER(dbg);

    if (dev->frontend.layout.type == FrontendType::UNKNOWN)
        return;

  unsigned int channels, bpp;
  int pass, total_size, i, resolution, lines;
  int topavg[3], bottomavg[3], avg[3];
  int top[3], bottom[3], black_pixels, pixels, factor, dpihw;

  /* offset calibration is always done in color mode */
  channels = 3;
  lines = 8;
  bpp = 8;

    // compute divider factor to compute final pixels number
    dpihw = sensor.get_logical_hwdpi(dev->settings.xres);
  factor = sensor.optical_res / dpihw;
  resolution = dpihw;

  const auto& calib_sensor = sanei_genesys_find_sensor(dev, resolution, channels,
                                                       dev->settings.scan_method);

  int target_pixels = calib_sensor.sensor_pixels / factor;
  int start_pixel = 0;
  black_pixels = calib_sensor.black_pixels / factor;

    if ((dev->settings.scan_method == ScanMethod::TRANSPARENCY ||
         dev->settings.scan_method == ScanMethod::TRANSPARENCY_INFRARED) &&
        dev->model->model_id == MODEL_CANON_CANOSCAN_8600F &&
        dev->settings.xres == 4800)
    {
        start_pixel = SANE_UNFIX(dev->model->x_offset_ta);
        start_pixel /= calib_sensor.get_ccd_size_divisor_for_dpi(resolution);
        start_pixel = (start_pixel * calib_sensor.optical_res) / MM_PER_INCH;

        target_pixels = SANE_UNFIX(dev->model->x_size_ta);
        target_pixels /= calib_sensor.get_ccd_size_divisor_for_dpi(resolution);
        target_pixels = (target_pixels * calib_sensor.optical_res) / MM_PER_INCH;
    }

  int flags = SCAN_FLAG_DISABLE_SHADING |
              SCAN_FLAG_DISABLE_GAMMA |
              SCAN_FLAG_SINGLE_LINE |
              SCAN_FLAG_IGNORE_LINE_DISTANCE;

    if (dev->settings.scan_method == ScanMethod::TRANSPARENCY ||
        dev->settings.scan_method == ScanMethod::TRANSPARENCY_INFRARED)
    {
        flags |= SCAN_FLAG_USE_XPA;
    }

    ScanSession session;
    session.params.xres = resolution;
    session.params.yres = resolution;
    session.params.startx = start_pixel;
    session.params.starty = 0;
    session.params.pixels = target_pixels;
    session.params.lines = lines;
    session.params.depth = bpp;
    session.params.channels = channels;
    session.params.scan_method = dev->settings.scan_method;
    session.params.scan_mode = ScanColorMode::COLOR_SINGLE_PASS;
    session.params.color_filter = ColorFilter::RED;
    session.params.flags = flags;
    gl843_compute_session(dev, session, calib_sensor);
    pixels = session.output_pixels;

    DBG(DBG_io, "%s: dpihw       =%d\n", __func__, dpihw);
    DBG(DBG_io, "%s: factor      =%d\n", __func__, factor);
    DBG(DBG_io, "%s: resolution  =%d\n", __func__, resolution);
    DBG(DBG_io, "%s: pixels      =%d\n", __func__, pixels);
    DBG(DBG_io, "%s: black_pixels=%d\n", __func__, black_pixels);
    gl843_init_scan_regs(dev, calib_sensor, &regs, session);

  sanei_genesys_set_motor_power(regs, false);

    // allocate memory for scans
    total_size = dev->read_bytes_left_after_deseg;

  std::vector<uint8_t> first_line(total_size);
  std::vector<uint8_t> second_line(total_size);

  /* init gain and offset */
  for (i = 0; i < 3; i++)
    {
      bottom[i] = 10;
      dev->frontend.set_offset(i, bottom[i]);
      dev->frontend.set_gain(i, 0);
    }
    gl843_set_fe(dev, calib_sensor, AFE_SET);

    // scan with obttom AFE settings
    dev->write_registers(regs);
  DBG(DBG_info, "%s: starting first line reading\n", __func__);
    gl843_begin_scan(dev, calib_sensor, &regs, SANE_TRUE);
    sanei_genesys_read_data_from_scanner(dev, first_line.data(), total_size);
    gl843_stop_action_no_move(dev, &regs);

  if (DBG_LEVEL >= DBG_data)
    {
      char fn[40];
      snprintf(fn, 40, "gl843_bottom_offset_%03d_%03d_%03d.pnm",
               bottom[0], bottom[1], bottom[2]);
      sanei_genesys_write_pnm_file(fn, first_line.data(), bpp, channels, pixels, lines);
    }

  for (i = 0; i < 3; i++)
    {
      bottomavg[i] = dark_average_channel(first_line.data(), pixels, lines, channels, black_pixels, i);
      DBG(DBG_io2, "%s: bottom avg %d=%d\n", __func__, i, bottomavg[i]);
    }

  /* now top value */
  for (i = 0; i < 3; i++)
    {
      top[i] = 255;
      dev->frontend.set_offset(i, top[i]);
    }
    gl843_set_fe(dev, calib_sensor, AFE_SET);

    // scan with top AFE values
    dev->write_registers(regs);
  DBG(DBG_info, "%s: starting second line reading\n", __func__);
    gl843_begin_scan(dev, calib_sensor, &regs, SANE_TRUE);
    sanei_genesys_read_data_from_scanner(dev, second_line.data(), total_size);
    gl843_stop_action_no_move(dev, &regs);

  for (i = 0; i < 3; i++)
    {
      topavg[i] = dark_average_channel(second_line.data(), pixels, lines, channels, black_pixels, i);
      DBG(DBG_io2, "%s: top avg %d=%d\n", __func__, i, topavg[i]);
    }

  pass = 0;

  std::vector<uint8_t> debug_image;
  size_t debug_image_lines = 0;
  std::string debug_image_info;

  /* loop until acceptable level */
  while ((pass < 32)
	 && ((top[0] - bottom[0] > 1)
	     || (top[1] - bottom[1] > 1) || (top[2] - bottom[2] > 1)))
    {
      pass++;

      /* settings for new scan */
      for (i = 0; i < 3; i++)
	{
	  if (top[i] - bottom[i] > 1)
	    {
              dev->frontend.set_offset(i, (top[i] + bottom[i]) / 2);
	    }
	}
        gl843_set_fe(dev, calib_sensor, AFE_SET);

        // scan with no move
        dev->write_registers(regs);
      DBG(DBG_info, "%s: starting second line reading\n", __func__);
        gl843_begin_scan(dev, calib_sensor, &regs, SANE_TRUE);
        sanei_genesys_read_data_from_scanner(dev, second_line.data(), total_size);
        gl843_stop_action_no_move(dev, &regs);

      if (DBG_LEVEL >= DBG_data)
	{
          char title[100];
          snprintf(title, 100, "lines: %d pixels_per_line: %d offsets[0..2]: %d %d %d\n",
                   lines, pixels,
                   dev->frontend.get_offset(0),
                   dev->frontend.get_offset(1),
                   dev->frontend.get_offset(2));
          debug_image_info += title;
          std::copy(second_line.begin(), second_line.end(), std::back_inserter(debug_image));
          debug_image_lines += lines;
	}

      for (i = 0; i < 3; i++)
	{
          avg[i] = dark_average_channel(second_line.data(), pixels, lines, channels, black_pixels, i);
          DBG(DBG_info, "%s: avg[%d]=%d offset=%d\n", __func__, i, avg[i],
              dev->frontend.get_offset(i));
	}

      /* compute new boundaries */
      for (i = 0; i < 3; i++)
	{
	  if (topavg[i] >= avg[i])
	    {
	      topavg[i] = avg[i];
              top[i] = dev->frontend.get_offset(i);
	    }
	  else
	    {
	      bottomavg[i] = avg[i];
              bottom[i] = dev->frontend.get_offset(i);
	    }
	}
    }

  if (DBG_LEVEL >= DBG_data)
    {
      sanei_genesys_write_file("gl843_offset_all_desc.txt",
                               (uint8_t*) debug_image_info.data(), debug_image_info.size());
      sanei_genesys_write_pnm_file("gl843_offset_all.pnm",
                                   debug_image.data(), bpp, channels, pixels, debug_image_lines);
    }

  DBG(DBG_info, "%s: offset=(%d,%d,%d)\n", __func__,
      dev->frontend.get_offset(0),
      dev->frontend.get_offset(1),
      dev->frontend.get_offset(2));
}


/* alternative coarse gain calibration
   this on uses the settings from offset_calibration and
   uses only one scanline
 */
/*
  with offset and coarse calibration we only want to get our input range into
  a reasonable shape. the fine calibration of the upper and lower bounds will
  be done with shading.
 */
static void gl843_coarse_gain_calibration(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                          Genesys_Register_Set& regs, int dpi)
{
    DBG_HELPER_ARGS(dbg, "dpi = %d", dpi);
  int pixels, factor, dpihw;
  int total_size;
  int i, j, channels;
  float coeff;
  int val, lines;
  int resolution;
  int bpp;

    if (dev->frontend.layout.type == FrontendType::UNKNOWN)
        return;

    dpihw = sensor.get_logical_hwdpi(dpi);
  factor=sensor.optical_res/dpihw;

  /* coarse gain calibration is always done in color mode */
  channels = 3;

  /* follow CKSEL */
  if (dev->model->ccd_type == CCD_KVSS080)
    {
      if(dev->settings.xres<sensor.optical_res)
        {
          coeff=0.9;
        }
      else
        {
          coeff=1.0;
        }
    }
  else
    {
      coeff=1.0;
    }
  resolution=dpihw;
  lines=10;
  bpp=8;
  int target_pixels = sensor.sensor_pixels / factor;

  int flags = SCAN_FLAG_DISABLE_SHADING |
              SCAN_FLAG_DISABLE_GAMMA |
              SCAN_FLAG_SINGLE_LINE |
              SCAN_FLAG_IGNORE_LINE_DISTANCE;

    if (dev->settings.scan_method == ScanMethod::TRANSPARENCY ||
        dev->settings.scan_method == ScanMethod::TRANSPARENCY_INFRARED)
    {
        flags |= SCAN_FLAG_USE_XPA;
    }

    const auto& calib_sensor = sanei_genesys_find_sensor(dev, resolution, channels,
                                                         dev->settings.scan_method);

    ScanSession session;
    session.params.xres = resolution;
    session.params.yres = resolution;
    session.params.startx = 0;
    session.params.starty = 0;
    session.params.pixels = target_pixels;
    session.params.lines = lines;
    session.params.depth = bpp;
    session.params.channels = channels;
    session.params.scan_method = dev->settings.scan_method;
    session.params.scan_mode = ScanColorMode::COLOR_SINGLE_PASS;
    session.params.color_filter = dev->settings.color_filter;
    session.params.flags = flags;
    gl843_compute_session(dev, session, calib_sensor);
    pixels = session.output_pixels;

    try {
        gl843_init_scan_regs(dev, calib_sensor, &regs, session);
    } catch (...) {
        catch_all_exceptions(__func__, [&](){ sanei_genesys_set_motor_power(regs, false); });
        throw;
    }

    sanei_genesys_set_motor_power(regs, false);

    dev->write_registers(regs);

    total_size = dev->read_bytes_left_after_deseg;

  std::vector<uint8_t> line(total_size);

    gl843_set_fe(dev, calib_sensor, AFE_SET);
    gl843_begin_scan(dev, calib_sensor, &regs, SANE_TRUE);
    sanei_genesys_read_data_from_scanner(dev, line.data(), total_size);
    gl843_stop_action_no_move(dev, &regs);

  if (DBG_LEVEL >= DBG_data)
    sanei_genesys_write_pnm_file("gl843_gain.pnm", line.data(), bpp, channels, pixels, lines);

  /* average value on each channel */
  for (j = 0; j < channels; j++)
    {
      std::vector<uint16_t> values;
      // FIXME: start from the second line because the first line often has artifacts. Probably
      // caused by unclean cleanup of previous scans
      for (i = pixels/4 + pixels; i < (pixels*3/4) + pixels; i++)
	{
          if(bpp==16)
            {
	  if (dev->model->is_cis)
	    val =
	      line[i * 2 + j * 2 * pixels + 1] * 256 +
	      line[i * 2 + j * 2 * pixels];
	  else
	    val =
	      line[i * 2 * channels + 2 * j + 1] * 256 +
	      line[i * 2 * channels + 2 * j];
            }
          else
            {
	  if (dev->model->is_cis)
	    val = line[i + j * pixels];
	  else
	    val = line[i * channels + j];
            }

            values.push_back(val);
	}

        // pick target value at 95th percentile of all values. There may be a lot of black values
        // in transparency scans for example
        std::sort(values.begin(), values.end());
        uint16_t curr_output = values[unsigned((values.size() - 1) * 0.95)];
        if (bpp == 16) {
            curr_output /= 256;
        }
        float target_value = calib_sensor.gain_white_ref * coeff;

        int code = compute_frontend_gain(curr_output, target_value, dev->frontend.layout.type);
      dev->frontend.set_gain(j, code);

        DBG(DBG_proc, "%s: channel %d, max=%d, target=%d, setting:%d\n", __func__, j, curr_output,
            (int) target_value, code);
    }

    if (dev->model->is_cis) {
        uint8_t gain0 = dev->frontend.get_gain(0);
        if (gain0 > dev->frontend.get_gain(1)) {
            gain0 = dev->frontend.get_gain(1);
        }
        if (gain0 > dev->frontend.get_gain(2)) {
            gain0 = dev->frontend.get_gain(2);
        }
        dev->frontend.set_gain(0, gain0);
        dev->frontend.set_gain(1, gain0);
        dev->frontend.set_gain(2, gain0);
    }

    if (channels == 1) {
        dev->frontend.set_gain(0, dev->frontend.get_gain(1));
        dev->frontend.set_gain(2, dev->frontend.get_gain(1));
    }

    gl843_stop_action(dev);

    gl843_slow_back_home(dev, SANE_TRUE);
}

// wait for lamp warmup by scanning the same line until difference
// between 2 scans is below a threshold
static void gl843_init_regs_for_warmup(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                       Genesys_Register_Set* reg, int* channels, int* total_size)
{
    DBG_HELPER(dbg);
  int num_pixels;
  int dpihw;
  int resolution;
  int factor;

  /* setup scan */
  *channels=3;
  resolution=600;
    dpihw = sensor.get_logical_hwdpi(resolution);
  resolution=dpihw;

  const auto& calib_sensor = sanei_genesys_find_sensor(dev, resolution, *channels,
                                                       dev->settings.scan_method);
  factor = calib_sensor.optical_res/dpihw;
  num_pixels = calib_sensor.sensor_pixels/(factor*2);
  *total_size = num_pixels * 3 * 1;

  *reg = dev->reg;

    ScanSession session;
    session.params.xres = resolution;
    session.params.yres = resolution;
    session.params.startx = num_pixels/2;
    session.params.starty = 0;
    session.params.pixels = num_pixels;
    session.params.lines = 1;
    session.params.depth = 8;
    session.params.channels = *channels;
    session.params.scan_method = dev->settings.scan_method;
    session.params.scan_mode = ScanColorMode::COLOR_SINGLE_PASS;
    session.params.color_filter = dev->settings.color_filter;
    session.params.flags =  SCAN_FLAG_DISABLE_SHADING |
                            SCAN_FLAG_DISABLE_GAMMA |
                            SCAN_FLAG_SINGLE_LINE |
                            SCAN_FLAG_IGNORE_LINE_DISTANCE;
    gl843_compute_session(dev, session, calib_sensor);

    gl843_init_scan_regs(dev, calib_sensor, reg, session);

  sanei_genesys_set_motor_power(*reg, false);
    dev->write_registers(*reg);
}

/**
 * set up GPIO/GPOE for idle state
WRITE GPIO[17-21]= GPIO19
WRITE GPOE[17-21]= GPOE21 GPOE20 GPOE19 GPOE18
genesys_write_register(0xa8,0x3e)
GPIO(0xa8)=0x3e
 */
static void gl843_init_gpio(Genesys_Device* dev)
{
    DBG_HELPER(dbg);
    apply_registers_ordered(dev->gpo.regs, { 0x6e, 0x6f }, [&](const GenesysRegisterSetting& reg)
    {
        dev->write_register(reg.address, reg.value);
    });
}


/* *
 * initialize ASIC from power on condition
 */
static void gl843_boot(Genesys_Device* dev, SANE_Bool cold)
{
    DBG_HELPER(dbg);
  uint8_t val;

    if (cold) {
        dev->write_register(0x0e, 0x01);
        dev->write_register(0x0e, 0x00);
    }

  if(dev->usb_mode == 1)
    {
      val = 0x14;
    }
  else
    {
      val = 0x11;
    }
    sanei_genesys_write_0x8c(dev, 0x0f, val);

    // test CHKVER
    val = dev->read_register(REG40);
    if (val & REG40_CHKVER) {
        val = dev->read_register(0x00);
        DBG(DBG_info, "%s: reported version for genesys chip is 0x%02x\n", __func__, val);
    }

  /* Set default values for registers */
  gl843_init_registers (dev);

    if (dev->model->model_id == MODEL_CANON_CANOSCAN_8600F) {
        // turns on vref control for maximum current of the motor driver
        dev->write_register(REG6B, 0x72);
    } else {
        dev->write_register(REG6B, 0x02);
    }

    // Write initial registers
    dev->write_registers(dev->reg);

  // Enable DRAM by setting a rising edge on bit 3 of reg 0x0b
  val = dev->reg.find_reg(0x0b).value & REG0B_DRAMSEL;
  val = (val | REG0B_ENBDRAM);
    dev->write_register(REG0B, val);
  dev->reg.find_reg(0x0b).value = val;

    if (dev->model->model_id == MODEL_CANON_CANOSCAN_8400F) {
        sanei_genesys_write_0x8c(dev, 0x1e, 0x01);
        sanei_genesys_write_0x8c(dev, 0x10, 0xb4);
        sanei_genesys_write_0x8c(dev, 0x0f, 0x02);
    }
    else if (dev->model->model_id == MODEL_CANON_CANOSCAN_8600F) {
        sanei_genesys_write_0x8c(dev, 0x10, 0xc8);
    } else {
        sanei_genesys_write_0x8c(dev, 0x10, 0xb4);
    }

  /* CLKSET */
  int clock_freq = REG0B_48MHZ;
  if (dev->model->model_id == MODEL_CANON_CANOSCAN_8600F)
    clock_freq = REG0B_60MHZ;

  val = (dev->reg.find_reg(0x0b).value & ~REG0B_CLKSET) | clock_freq;

    dev->write_register(REG0B, val);
  dev->reg.find_reg(0x0b).value = val;

  /* prevent further writings by bulk write register */
  dev->reg.remove_reg(0x0b);

  if (dev->model->model_id != MODEL_CANON_CANOSCAN_8600F)
    {
      // set up end access
      // FIXME: this is overwritten in gl843_init_gpio
        dev->write_register(REGA7, 0x04);
        dev->write_register(REGA9, 0x00);
    }

    // set RAM read address
    dev->write_register(REG29, 0x00);
    dev->write_register(REG2A, 0x00);
    dev->write_register(REG2B, 0x00);

    // setup gpio
    gl843_init_gpio(dev);

  gl843_feed (dev, 300);
  sanei_genesys_sleep_ms(100);
}

/* *
 * initialize backend and ASIC : registers, motor tables, and gamma tables
 * then ensure scanner's head is at home
 */
static void gl843_init(Genesys_Device* dev)
{
  DBG_INIT ();
    DBG_HELPER(dbg);

    sanei_genesys_asic_init(dev, 0);
}

static void gl843_update_hardware_sensors(Genesys_Scanner* s)
{
    DBG_HELPER(dbg);
  /* do what is needed to get a new set of events, but try to not lose
     any of them.
   */

    uint8_t val = s->dev->read_register(REG6D);

  switch (s->dev->model->gpo_type)
    {
        case GPO_KVSS080:
            s->buttons[BUTTON_SCAN_SW].write((val & 0x04) == 0);
            break;
        case GPO_G4050:
            s->buttons[BUTTON_SCAN_SW].write((val & 0x01) == 0);
            s->buttons[BUTTON_FILE_SW].write((val & 0x02) == 0);
            s->buttons[BUTTON_EMAIL_SW].write((val & 0x04) == 0);
            s->buttons[BUTTON_COPY_SW].write((val & 0x08) == 0);
            break;
        case GPO_CS4400F:
        case GPO_CS8400F:
        default:
            break;
    }
}

/** @brief move sensor to transparency adaptor
 * Move sensor to the calibration of the transparency adapator (XPA).
 * @param dev device to use
 */
static void gl843_move_to_ta(Genesys_Device* dev)
{
    DBG_HELPER(dbg);
  float resolution;
  unsigned int feed;

  resolution=sanei_genesys_get_lowest_ydpi(dev);
  feed = 16*(SANE_UNFIX (dev->model->y_offset_sensor_to_ta) * resolution) / MM_PER_INCH;
    gl843_feed(dev, feed);
}


/** @brief search for a full width black or white strip.
 * This function searches for a black or white stripe across the scanning area.
 * When searching backward, the searched area must completely be of the desired
 * color since this area will be used for calibration which scans forward.
 * @param dev scanner device
 * @param forward SANE_TRUE if searching forward, SANE_FALSE if searching backward
 * @param black SANE_TRUE if searching for a black strip, SANE_FALSE for a white strip
 */
static void gl843_search_strip(Genesys_Device* dev, const Genesys_Sensor& sensor, SANE_Bool forward,
                               SANE_Bool black)
{
    DBG_HELPER_ARGS(dbg, "%s %s",  black ? "black" : "white", forward ? "forward" : "reverse");
  unsigned int pixels, lines, channels;
  Genesys_Register_Set local_reg;
  size_t size;
  int steps, depth, dpi;
  unsigned int pass, count, found, x, y;
  GenesysRegister *r;

  gl843_set_fe(dev, sensor, AFE_SET);
    gl843_stop_action(dev);

  /* set up for a gray scan at lowest dpi */
  dpi = sanei_genesys_get_lowest_dpi(dev);
  channels = 1;

  const auto& calib_sensor = sanei_genesys_find_sensor(dev, dpi, channels,
                                                       dev->settings.scan_method);

  /* 10 MM */
  /* lines = (10 * dpi) / MM_PER_INCH; */
  /* shading calibation is done with dev->motor.base_ydpi */
  lines = (dev->model->shading_lines * dpi) / dev->motor.base_ydpi;
  depth = 8;
  pixels = (calib_sensor.sensor_pixels * dpi) / calib_sensor.optical_res;

  dev->scanhead_position_in_steps = 0;

  local_reg = dev->reg;

    ScanSession session;
    session.params.xres = dpi;
    session.params.yres = dpi;
    session.params.startx = 0;
    session.params.starty = 0;
    session.params.pixels = pixels;
    session.params.lines = lines;
    session.params.depth = depth;
    session.params.channels = channels;
    session.params.scan_method = dev->settings.scan_method;
    session.params.scan_mode = ScanColorMode::GRAY;
    session.params.color_filter = ColorFilter::RED;
    session.params.flags = SCAN_FLAG_DISABLE_SHADING | SCAN_FLAG_DISABLE_SHADING;
    gl843_compute_session(dev, session, calib_sensor);

    gl843_init_scan_regs(dev, calib_sensor, &local_reg, session);

    size = dev->read_bytes_left_after_deseg;
  std::vector<uint8_t> data(size);

  /* set up for reverse or forward */
  r = sanei_genesys_get_address(&local_reg, REG02);
    if (forward) {
        r->value &= ~REG02_MTRREV;
    } else {
        r->value |= REG02_MTRREV;
    }

    dev->write_registers(local_reg);

    gl843_begin_scan(dev, calib_sensor, &local_reg, SANE_TRUE);

        // waits for valid data
        do {
            sanei_genesys_test_buffer_empty(dev, &steps);
        } while (steps);

    // now we're on target, we can read data
    sanei_genesys_read_data_from_scanner(dev, data.data(), size);

    gl843_stop_action(dev);

  pass = 0;
  if (DBG_LEVEL >= DBG_data)
    {
      char fn[40];
      snprintf(fn, 40, "gl843_search_strip_%s_%s%02d.pnm",
               black ? "black" : "white", forward ? "fwd" : "bwd", (int)pass);
      sanei_genesys_write_pnm_file(fn, data.data(), depth, channels, pixels, lines);
    }

  /* loop until strip is found or maximum pass number done */
  found = 0;
  while (pass < 20 && !found)
    {
        dev->write_registers(local_reg);

        // now start scan
        gl843_begin_scan(dev, calib_sensor, &local_reg, SANE_TRUE);

        // waits for valid data
        do {
            sanei_genesys_test_buffer_empty(dev, &steps);
        } while (steps);

        // now we're on target, we can read data
        sanei_genesys_read_data_from_scanner(dev, data.data(), size);

        gl843_stop_action(dev);

      if (DBG_LEVEL >= DBG_data)
	{
          char fn[40];
          snprintf(fn, 40, "gl843_search_strip_%s_%s%02d.pnm",
                   black ? "black" : "white", forward ? "fwd" : "bwd", (int)pass);
          sanei_genesys_write_pnm_file(fn, data.data(), depth, channels, pixels, lines);
	}

      /* search data to find black strip */
      /* when searching forward, we only need one line of the searched color since we
       * will scan forward. But when doing backward search, we need all the area of the
       * same color */
      if (forward)
	{
	  for (y = 0; y < lines && !found; y++)
	    {
	      count = 0;
	      /* count of white/black pixels depending on the color searched */
	      for (x = 0; x < pixels; x++)
		{
		  /* when searching for black, detect white pixels */
		  if (black && data[y * pixels + x] > 90)
		    {
		      count++;
		    }
		  /* when searching for white, detect black pixels */
		  if (!black && data[y * pixels + x] < 60)
		    {
		      count++;
		    }
		}

	      /* at end of line, if count >= 3%, line is not fully of the desired color
	       * so we must go to next line of the buffer */
	      /* count*100/pixels < 3 */
	      if ((count * 100) / pixels < 3)
		{
		  found = 1;
		  DBG(DBG_data, "%s: strip found forward during pass %d at line %d\n", __func__,
		      pass, y);
		}
	      else
		{
		  DBG(DBG_data, "%s: pixels=%d, count=%d (%d%%)\n", __func__, pixels, count,
		      (100 * count) / pixels);
		}
	    }
	}
      else			/* since calibration scans are done forward, we need the whole area
				   to be of the required color when searching backward */
	{
	  count = 0;
	  for (y = 0; y < lines; y++)
	    {
	      /* count of white/black pixels depending on the color searched */
	      for (x = 0; x < pixels; x++)
		{
		  /* when searching for black, detect white pixels */
		  if (black && data[y * pixels + x] > 90)
		    {
		      count++;
		    }
		  /* when searching for white, detect black pixels */
		  if (!black && data[y * pixels + x] < 60)
		    {
		      count++;
		    }
		}
	    }

	  /* at end of area, if count >= 3%, area is not fully of the desired color
	   * so we must go to next buffer */
	  if ((count * 100) / (pixels * lines) < 3)
	    {
	      found = 1;
	      DBG(DBG_data, "%s: strip found backward during pass %d \n", __func__, pass);
	    }
	  else
	    {
	      DBG(DBG_data, "%s: pixels=%d, count=%d (%d%%)\n", __func__, pixels, count,
		  (100 * count) / pixels);
	    }
	}
      pass++;
    }
  if (found)
    {
      DBG(DBG_info, "%s: %s strip found\n", __func__, black ? "black" : "white");
    }
  else
    {
        throw SaneException(SANE_STATUS_UNSUPPORTED, "%s strip not found", black ? "black" : "white");
    }
}

/**
 * Send shading calibration data. The buffer is considered to always hold values
 * for all the channels.
 */
static void gl843_send_shading_data(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                    uint8_t* data, int size)
{
    DBG_HELPER(dbg);
  uint32_t final_size, length, i;
  uint8_t *buffer;
  int count,offset;
  GenesysRegister *r;
    uint16_t strpixel, endpixel, startx;

  offset=0;
  length=size;
  r = sanei_genesys_get_address(&dev->reg, REG01);
  if(r->value & REG01_SHDAREA)
    {
      /* recompute STRPIXEL used shading calibration so we can
       * compute offset within data for SHDAREA case */

        // FIXME: the following is likely incorrect
        // start coordinate in optical dpi coordinates
        startx = (sensor.dummy_pixel / sensor.ccd_pixels_per_system_pixel()) / dev->session.hwdpi_divisor;

      /* current scan coordinates */
        strpixel = dev->session.pixel_startx;
        endpixel = dev->session.pixel_endx;

      if (dev->model->model_id == MODEL_CANON_CANOSCAN_8600F)
        {
            int dpiset_real = dev->session.output_resolution * sensor.ccd_pixels_per_system_pixel();
            int half_ccd_factor = dev->session.optical_resolution / sensor.get_logical_hwdpi(dpiset_real);
          strpixel /= half_ccd_factor;
          endpixel /= half_ccd_factor;
        }

      /* 16 bit words, 2 words per color, 3 color channels */
      offset=(strpixel-startx)*2*2*3;
      length=(endpixel-strpixel)*2*2*3;
      DBG(DBG_info, "%s: STRPIXEL=%d, ENDPIXEL=%d, startx=%d\n", __func__, strpixel, endpixel,
          startx);
    }

  /* compute and allocate size for final data */
  final_size = ((length+251) / 252) * 256;
  DBG(DBG_io, "%s: final shading size=%04x (length=%d)\n", __func__, final_size, length);
  std::vector<uint8_t> final_data(final_size, 0);

  /* copy regular shading data to the expected layout */
  buffer = final_data.data();
  count = 0;

  /* loop over calibration data */
  for (i = 0; i < length; i++)
    {
      buffer[count] = data[offset+i];
      count++;
      if ((count % (256*2)) == (252*2))
	{
	  count += 4*2;
	}
    }

    // send data
    sanei_genesys_set_buffer_address(dev, 0);

    dev->cmd_set->bulk_write_data (dev, 0x3c, final_data.data(), count);
}


/** the gl843 command set */
Genesys_Command_Set gl843_cmd_set = {
  "gl843-generic",		/* the name of this set */

  [](Genesys_Device* dev) -> bool { (void) dev; return true; },

  gl843_init,
  gl843_init_regs_for_warmup,
  gl843_init_regs_for_coarse_calibration,
  gl843_init_regs_for_shading,
  gl843_init_regs_for_scan,

  gl843_get_filter_bit,
  gl843_get_lineart_bit,
  gl843_get_bitset_bit,
  gl843_get_gain4_bit,
  gl843_get_fast_feed_bit,
  gl843_test_buffer_empty_bit,
  gl843_test_motor_flag_bit,

  gl843_set_fe,
  gl843_set_powersaving,
  gl843_save_power,

  gl843_begin_scan,
  gl843_end_scan,

  gl843_send_gamma_table,

  gl843_search_start_position,

  gl843_offset_calibration,
  gl843_coarse_gain_calibration,
  gl843_led_calibration,

  NULL,
  gl843_slow_back_home,
  NULL,

  sanei_genesys_bulk_write_data,
  sanei_genesys_bulk_read_data,

  gl843_update_hardware_sensors,

  gl843_load_document,
  gl843_detect_document_end,
  gl843_eject_document,
  gl843_search_strip,

  sanei_genesys_is_compatible_calibration,
  gl843_move_to_ta,
  gl843_send_shading_data,
  gl843_calculate_current_setup,
  gl843_boot
};
