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

#include "gl843_registers.h"
#include "gl843.h"

#include <string>
#include <vector>

namespace genesys {

// Set address for writing data
static void gl843_set_buffer_address(Genesys_Device* dev, uint32_t addr)
{
    DBG_HELPER_ARGS(dbg, "setting address to 0x%05x", addr & 0xffff);

    dev->write_register(0x5b, ((addr >> 8) & 0xff));
    dev->write_register(0x5c, (addr & 0xff));
}

static void gl843_set_ram_address(Genesys_Device* dev, uint32_t addr)
{
    DBG_HELPER_ARGS(dbg, "setting address to 0x%05x", addr & 0x1fffff);

    dev->write_register(0x29, ((addr >> 16) & 0x1f));
    dev->write_register(0x2a, ((addr >> 8) & 0xff));
    dev->write_register(0x2b, (addr & 0xff));
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

bool CommandSetGl843::get_gain4_bit(Genesys_Register_Set* regs) const
{
    GenesysRegister *r = sanei_genesys_get_address(regs, REG_0x06);
    return (r && (r->value & REG_0x06_GAIN4));
}

/**
 * compute the step multiplier used
 */
static int
gl843_get_step_multiplier (Genesys_Register_Set * regs)
{
    GenesysRegister *r = sanei_genesys_get_address(regs, REG_0x9D);
    int value = 1;
  if (r != nullptr)
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

bool CommandSetGl843::test_buffer_empty_bit(SANE_Byte val) const
{
    return (val & REG_0x41_BUFEMPTY);
}

/** copy sensor specific settings */
static void gl843_setup_sensor(Genesys_Device* dev, const Genesys_Sensor& sensor,
                               Genesys_Register_Set* regs)
{
    DBG_HELPER(dbg);
    for (const auto& custom_reg : sensor.custom_regs) {
        regs->set8(custom_reg.address, custom_reg.value);
    }
    if (!(dev->model->flags & GENESYS_FLAG_FULL_HWDPI_MODE) &&
        dev->model->model_id != ModelId::PLUSTEK_OPTICFILM_7200I &&
        dev->model->model_id != ModelId::PLUSTEK_OPTICFILM_7300 &&
        dev->model->model_id != ModelId::PLUSTEK_OPTICFILM_7500I)
    {
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

  SETREG(0x01, 0x00);
  SETREG (0x02, 0x78);
  SETREG (0x03, 0x1f);
    if (dev->model->model_id == ModelId::HP_SCANJET_G4010 ||
        dev->model->model_id == ModelId::HP_SCANJET_G4050 ||
        dev->model->model_id == ModelId::HP_SCANJET_4850C)
    {
        SETREG(0x03, 0x1d);
    }
    if (dev->model->model_id == ModelId::CANON_8400F) {
        SETREG(0x03, 0x1c);
    }

    SETREG(0x04, 0x10);
    if (dev->model->model_id == ModelId::PLUSTEK_OPTICFILM_7200I ||
        dev->model->model_id == ModelId::PLUSTEK_OPTICFILM_7300 ||
        dev->model->model_id == ModelId::PLUSTEK_OPTICFILM_7500I)
    {
        SETREG(0x04, 0x22);
    }

  // fine tune upon device description
  SETREG (0x05, 0x80);
    if (dev->model->model_id == ModelId::HP_SCANJET_G4010 ||
        dev->model->model_id == ModelId::HP_SCANJET_G4050 ||
        dev->model->model_id == ModelId::HP_SCANJET_4850C)
    {
      SETREG (0x05, 0x08);
    }

    const auto& sensor = sanei_genesys_find_sensor_any(dev);
    sanei_genesys_set_dpihw(dev->reg, sensor, sensor.optical_res);

  // TODO: on 8600F the windows driver turns off GAIN4 which is recommended
  SETREG (0x06, 0xd8); /* SCANMOD=110, PWRBIT and GAIN4 */
    if (dev->model->model_id == ModelId::HP_SCANJET_G4010 ||
        dev->model->model_id == ModelId::HP_SCANJET_G4050 ||
        dev->model->model_id == ModelId::HP_SCANJET_4850C)
    {
        SETREG(0x06, 0xd8); /* SCANMOD=110, PWRBIT and GAIN4 */
    }
    if (dev->model->model_id == ModelId::PLUSTEK_OPTICFILM_7200I) {
        SETREG(0x06, 0xd0);
    }
    if (dev->model->model_id == ModelId::CANON_4400F ||
        dev->model->model_id == ModelId::PLUSTEK_OPTICFILM_7300 ||
        dev->model->model_id == ModelId::PLUSTEK_OPTICFILM_7500I)
    {
        SETREG(0x06, 0xf0); /* SCANMOD=111, PWRBIT and no GAIN4 */
    }

  SETREG (0x08, 0x00);
  SETREG (0x09, 0x00);
  SETREG (0x0a, 0x00);
    if (dev->model->model_id == ModelId::HP_SCANJET_G4010 ||
        dev->model->model_id == ModelId::HP_SCANJET_G4050 ||
        dev->model->model_id == ModelId::HP_SCANJET_4850C)
    {
        SETREG(0x0a, 0x18);
    }
    if (dev->model->model_id == ModelId::CANON_8400F) {
        SETREG(0x0a, 0x10);
    }

  // This register controls clock and RAM settings and is further modified in
  // gl843_boot
  SETREG (0x0b, 0x6a);

    if (dev->model->model_id == ModelId::CANON_4400F) {
        SETREG(0x0b, 0x69); // 16M only
    }
    if (dev->model->model_id == ModelId::CANON_8600F) {
        SETREG(0x0b, 0x89);
    }
    if (dev->model->model_id == ModelId::PLUSTEK_OPTICFILM_7200I) {
        SETREG(0x0b, 0x2a);
    }
    if (dev->model->model_id == ModelId::PLUSTEK_OPTICFILM_7300 ||
        dev->model->model_id == ModelId::PLUSTEK_OPTICFILM_7500I) {
        SETREG(0x0b, 0x4a);
    }
    if (dev->model->model_id == ModelId::HP_SCANJET_G4010 ||
        dev->model->model_id == ModelId::HP_SCANJET_G4050 ||
        dev->model->model_id == ModelId::HP_SCANJET_4850C)
    {
        SETREG(0x0b, 0x69);
    }

    if (dev->model->model_id != ModelId::CANON_8400F &&
        dev->model->model_id != ModelId::PLUSTEK_OPTICFILM_7200I &&
        dev->model->model_id != ModelId::PLUSTEK_OPTICFILM_7300)
    {
        SETREG (0x0c, 0x00);
    }

  // EXPR[0:15], EXPG[0:15], EXPB[0:15]: Exposure time settings.
  SETREG(0x10, 0x00); // SENSOR_DEF
  SETREG(0x11, 0x00); // SENSOR_DEF
  SETREG(0x12, 0x00); // SENSOR_DEF
  SETREG(0x13, 0x00); // SENSOR_DEF
  SETREG(0x14, 0x00); // SENSOR_DEF
  SETREG(0x15, 0x00); // SENSOR_DEF
    if (dev->model->model_id == ModelId::CANON_4400F ||
        dev->model->model_id == ModelId::CANON_8600F)
    {
        dev->reg.set16(REG_EXPR, 0x9c40);
        dev->reg.set16(REG_EXPG, 0x9c40);
        dev->reg.set16(REG_EXPB, 0x9c40);
    }
    if (dev->model->model_id == ModelId::HP_SCANJET_G4010 ||
        dev->model->model_id == ModelId::HP_SCANJET_G4050 ||
        dev->model->model_id == ModelId::HP_SCANJET_4850C)
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
    if (dev->model->model_id == ModelId::CANON_4400F ||
        dev->model->model_id == ModelId::CANON_8600F)
    {
        SETREG(0x1e, 0x20);
    }
    if (dev->model->model_id == ModelId::CANON_8400F) {
        SETREG(0x1e, 0xa0);
    }

  SETREG (0x1f, 0x01);
    if (dev->model->model_id == ModelId::CANON_8600F) {
      SETREG(0x1f, 0xff);
    }

  SETREG (0x20, 0x10);
  SETREG (0x21, 0x04);

    SETREG(0x22, 0x01);
    SETREG(0x23, 0x01);
    if (dev->model->model_id == ModelId::CANON_4400F ||
        dev->model->model_id == ModelId::CANON_8600F)
    {
        SETREG(0x22, 0xc8);
        SETREG(0x23, 0xc8);
    }
    if (dev->model->model_id == ModelId::CANON_8400F) {
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
    if (dev->model->model_id == ModelId::CANON_8600F)
    {
      SETREG(0x34, 0x14);
    }
    if (dev->model->model_id == ModelId::PLUSTEK_OPTICFILM_7300 ||
        dev->model->model_id == ModelId::PLUSTEK_OPTICFILM_7500I)
    {
        SETREG(0x34, 0x3c);
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
    if (dev->model->model_id == ModelId::CANON_4400F) {
        SETREG(0x5e, 0x3f);
    }
    if (dev->model->model_id == ModelId::CANON_8400F) {
        SETREG(0x5e, 0x85);
    }
    if (dev->model->model_id == ModelId::CANON_8600F) {
        SETREG(0x5e, 0x1f);
    }
    if (dev->model->model_id == ModelId::PLUSTEK_OPTICFILM_7300 ||
        dev->model->model_id == ModelId::PLUSTEK_OPTICFILM_7500I)
    {
        SETREG(0x5e, 0x01);
    }

    //FMOVDEC: The number of deceleration steps in table 5 for auto-go-home
    SETREG(0x5f, 0x01);
    if (dev->model->model_id == ModelId::CANON_4400F) {
        SETREG(0x5f, 0xf0);
    }
    if (dev->model->model_id == ModelId::CANON_8600F) {
        SETREG(0x5f, 0xf0);
    }
    if (dev->model->model_id == ModelId::PLUSTEK_OPTICFILM_7300 ||
        dev->model->model_id == ModelId::PLUSTEK_OPTICFILM_7500I)
    {
        SETREG(0x5f, 0x01);
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

    if (dev->model->model_id == ModelId::PLUSTEK_OPTICFILM_7300) {
        SETREG(0x67, 0x80);
        SETREG(0x68, 0x80);
    }

  // FSHDEC[0:7]: The number of deceleration steps after scanning is finished
  // (table 3)
  SETREG (0x69, 0x01);
    if (dev->model->model_id == ModelId::CANON_8600F) {
      SETREG(0x69, 64);
    }

  // FMOVNO[0:7] The number of acceleration or deceleration steps for fast
  // moving (table 4)
  SETREG (0x6a, 0x04);
    if (dev->model->model_id == ModelId::CANON_8600F) {
      SETREG(0x69, 64);
    }

    // GPIO-related register bits
    SETREG(0x6b, 0x30);
    if (dev->model->model_id == ModelId::CANON_4400F ||
        dev->model->model_id == ModelId::CANON_8600F)
    {
        SETREG(0x6b, 0x72);
    }
    if (dev->model->model_id == ModelId::CANON_8400F) {
        SETREG(0x6b, 0xb1);
    }
    if (dev->model->model_id == ModelId::HP_SCANJET_G4010 ||
        dev->model->model_id == ModelId::HP_SCANJET_G4050 ||
        dev->model->model_id == ModelId::HP_SCANJET_4850C)
    {
        SETREG(0x6b, 0xf4);
    }
    if (dev->model->model_id == ModelId::PLUSTEK_OPTICFILM_7200I ||
        dev->model->model_id == ModelId::PLUSTEK_OPTICFILM_7300 ||
        dev->model->model_id == ModelId::PLUSTEK_OPTICFILM_7500I)
    {
        SETREG(0x6b, 0x31);
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

    if (dev->model->model_id == ModelId::CANON_4400F) {
        SETREG(0x70, 0x01);
        SETREG(0x71, 0x03);
        SETREG(0x72, 0x01);
        SETREG(0x73, 0x03);
    }
    if (dev->model->model_id == ModelId::CANON_8400F) {
        SETREG(0x70, 0x01);
        SETREG(0x71, 0x03);
        SETREG(0x72, 0x03);
        SETREG(0x73, 0x04);
    }
    if (dev->model->model_id == ModelId::CANON_8600F) {
        SETREG(0x70, 0x00);
        SETREG(0x71, 0x02);
        SETREG(0x72, 0x02);
        SETREG(0x73, 0x04);
    }
    if (dev->model->model_id == ModelId::HP_SCANJET_G4010 ||
        dev->model->model_id == ModelId::HP_SCANJET_G4050 ||
        dev->model->model_id == ModelId::HP_SCANJET_4850C)
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
    if (dev->model->model_id == ModelId::CANON_8400F) {
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
    if (dev->model->model_id == ModelId::CANON_4400F) {
        SETREG(0x80, 0x0c);
    }
    if (dev->model->model_id == ModelId::CANON_8400F) {
        SETREG(0x80, 0x28);
    }
    if (dev->model->model_id == ModelId::HP_SCANJET_G4010 ||
        dev->model->model_id == ModelId::HP_SCANJET_G4050 ||
        dev->model->model_id == ModelId::HP_SCANJET_4850C)
    {
        SETREG(0x80, 0x50);
    }
    if (dev->model->model_id == ModelId::PLUSTEK_OPTICFILM_7300 ||
        dev->model->model_id == ModelId::PLUSTEK_OPTICFILM_7500I)
    {
        SETREG(0x80, 0x0f);
    }

    if (dev->model->model_id != ModelId::CANON_4400F) {
      SETREG (0x81, 0x00);
      SETREG (0x82, 0x00);
      SETREG (0x83, 0x00);
      SETREG (0x84, 0x00);
      SETREG (0x85, 0x00);
      SETREG (0x86, 0x00);
    }

    SETREG(0x87, 0x00);
    if (dev->model->model_id == ModelId::CANON_4400F ||
        dev->model->model_id == ModelId::CANON_8400F ||
        dev->model->model_id == ModelId::CANON_8600F)
    {
        SETREG(0x87, 0x02);
    }

    // MTRPLS[0:7]: The width of the ADF motor trigger signal pulse.
    if (dev->model->model_id != ModelId::CANON_8400F &&
        dev->model->model_id != ModelId::PLUSTEK_OPTICFILM_7200I &&
        dev->model->model_id != ModelId::PLUSTEK_OPTICFILM_7300)
    {
        SETREG(0x94, 0xff);
    }

  // 0x95-0x97: SCANLEN[0:19]: Controls when paper jam bit is set in sheetfed
  // scanners.

  // ONDUR[0:15]: The duration of PWM ON phase for LAMP control
  // OFFDUR[0:15]: The duration of PWM OFF phase for LAMP control
  // both of the above are in system clocks
    if (dev->model->model_id == ModelId::CANON_8600F) {
      SETREG(0x98, 0x00);
      SETREG(0x99, 0x00);
      SETREG(0x9a, 0x00);
      SETREG(0x9b, 0x00);
    }
    if (dev->model->model_id == ModelId::HP_SCANJET_G4010 ||
        dev->model->model_id == ModelId::HP_SCANJET_G4050 ||
        dev->model->model_id == ModelId::HP_SCANJET_4850C)
    {
        // TODO: move to set for scan
        SETREG(0x98, 0x03);
        SETREG(0x99, 0x30);
        SETREG(0x9a, 0x01);
        SETREG(0x9b, 0x80);
    }

    // RMADLY[0:1], MOTLAG, CMODE, STEPTIM, MULDMYLN, IFRS
    SETREG(0x9d, 0x04);
    if (dev->model->model_id == ModelId::PLUSTEK_OPTICFILM_7300 ||
        dev->model->model_id == ModelId::PLUSTEK_OPTICFILM_7500I)
    {
        SETREG(0x9d, 0x00);
    }
    if (dev->model->model_id == ModelId::CANON_4400F ||
        dev->model->model_id == ModelId::CANON_8400F ||
        dev->model->model_id == ModelId::CANON_8600F ||
        dev->model->model_id == ModelId::PLUSTEK_OPTICFILM_7200I ||
        dev->model->model_id == ModelId::HP_SCANJET_G4010 ||
        dev->model->model_id == ModelId::HP_SCANJET_G4050 ||
        dev->model->model_id == ModelId::HP_SCANJET_4850C)
    {
        SETREG(0x9d, 0x08); // sets the multiplier for slope tables
    }


  // SEL3INV, TGSTIME[0:2], TGWTIME[0:2]
    if (dev->model->model_id != ModelId::CANON_8400F &&
        dev->model->model_id != ModelId::PLUSTEK_OPTICFILM_7200I &&
        dev->model->model_id != ModelId::PLUSTEK_OPTICFILM_7300)
    {
      SETREG(0x9e, 0x00); // SENSOR_DEF
    }

    if (dev->model->model_id != ModelId::PLUSTEK_OPTICFILM_7300) {
        SETREG(0xa2, 0x0f);
    }

    // RFHSET[0:4]: Refresh time of SDRAM in units of 2us
    if (dev->model->model_id == ModelId::CANON_4400F ||
        dev->model->model_id == ModelId::CANON_8600F)
    {
        SETREG(0xa2, 0x1f);
    }

    // 0xa6-0xa9: controls gpio, see gl843_gpio_init

    // not documented
    if (dev->model->model_id != ModelId::CANON_4400F &&
        dev->model->model_id != ModelId::CANON_8400F &&
        dev->model->model_id != ModelId::PLUSTEK_OPTICFILM_7200I &&
        dev->model->model_id != ModelId::PLUSTEK_OPTICFILM_7300)
    {
        SETREG(0xaa, 0x00);
    }

    // GPOM9, MULSTOP[0-2], NODECEL, TB3TB1, TB5TB2, FIX16CLK. Not documented
    if (dev->model->model_id != ModelId::CANON_8400F &&
        dev->model->model_id != ModelId::PLUSTEK_OPTICFILM_7200I &&
        dev->model->model_id != ModelId::PLUSTEK_OPTICFILM_7300) {
        SETREG(0xab, 0x50);
    }
    if (dev->model->model_id == ModelId::CANON_4400F) {
        SETREG(0xab, 0x00);
    }
    if (dev->model->model_id == ModelId::HP_SCANJET_G4010 ||
        dev->model->model_id == ModelId::HP_SCANJET_G4050 ||
        dev->model->model_id == ModelId::HP_SCANJET_4850C)
    {
        // BUG: this should apply to ModelId::CANON_CANOSCAN_8600F too, but due to previous bug
        // the 8400F case overwrote it
        SETREG(0xab, 0x40);
    }

  // VRHOME[3:2], VRMOVE[3:2], VRBACK[3:2]: Vref setting of the motor driver IC
  // for various situations.
    if (dev->model->model_id == ModelId::CANON_8600F ||
        dev->model->model_id == ModelId::HP_SCANJET_G4010 ||
        dev->model->model_id == ModelId::HP_SCANJET_G4050 ||
        dev->model->model_id == ModelId::HP_SCANJET_4850C)
    {
        SETREG(0xac, 0x00);
    }

  dev->calib_reg = dev->reg;

    if (dev->model->model_id == ModelId::PLUSTEK_OPTICFILM_7200I) {
        gl843_set_ram_address(dev, 0x03ff00);

        uint8_t data[32] = {
            0x8c, 0x8f, 0xc9, 0x00, 0x01, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x6a, 0x73, 0x63, 0x68, 0x69, 0x65, 0x6e, 0x00,
        };

        sanei_genesys_bulk_write_data(dev, 0x3c, data, 32);
    }
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
        std::sprintf(msg, "write slope %d (%d)=", table_nr, steps);
        for (i = 0; i < steps; i++) {
            std::sprintf (msg+strlen(msg), "%d", slope_table[i]);
	}
      DBG(DBG_io, "%s: %s\n", __func__, msg);
    }

    // slope table addresses are fixed : 0x4000,  0x4800,  0x5000,  0x5800,  0x6000
    // XXX STEF XXX USB 1.1 ? sanei_genesys_write_0x8c (dev, 0x0f, 0x14);
    write_data(dev, 0x4000 + 0x800 * table_nr, steps * 2, table.data());
}

static void gl843_set_ad_fe(Genesys_Device* dev)
{
    for (const auto& reg : dev->frontend.regs) {
        sanei_genesys_fe_write_data(dev, reg.address, reg.value);
    }
}

// Set values of analog frontend
void CommandSetGl843::set_fe(Genesys_Device* dev, const Genesys_Sensor& sensor, uint8_t set) const
{
    DBG_HELPER_ARGS(dbg, "%s", set == AFE_INIT ? "init" :
                               set == AFE_SET ? "set" :
                               set == AFE_POWER_SAVE ? "powersave" : "huh?");
    (void) sensor;
  int i;

  if (set == AFE_INIT)
    {
        DBG(DBG_proc, "%s(): setting DAC %u\n", __func__,
            static_cast<unsigned>(dev->model->adc_id));
      dev->frontend = dev->frontend_initial;
        dev->frontend_is_init = true;
    }

    // check analog frontend type
    // FIXME: looks like we write to that register with initial data
    uint8_t fe_type = dev->read_register(REG_0x04) & REG_0x04_FESET;
    if (fe_type == 2) {
        gl843_set_ad_fe(dev);
        return;
    }
    if (fe_type != 0) {
        throw SaneException(SANE_STATUS_UNSUPPORTED, "unsupported frontend type %d", fe_type);
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

    if (dev->model->sensor_id == SensorId::CCD_KVSS080) {
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
                                       unsigned scan_yres,
                                       StepType step_type,
                                       unsigned int scan_lines,
                                       unsigned int scan_dummy,
                                       unsigned int feed_steps,
                                       unsigned int flags)
{
    DBG_HELPER_ARGS(dbg, "exposure=%d, scan_yres=%d, step_type=%d, scan_lines=%d, scan_dummy=%d, "
                         "feed_steps=%d, flags=%x",
                    exposure, scan_yres, static_cast<unsigned>(step_type), scan_lines, scan_dummy,
                    feed_steps, flags);

  int use_fast_fed, coeff;
  unsigned int lincnt;
    std::vector<uint16_t> scan_table;
    std::vector<uint16_t> fast_table;
  int scan_steps,fast_steps;
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
    r = sanei_genesys_get_address(reg, REG_0x02);
  r->value = 0x00;
  sanei_genesys_set_motor_power(*reg, true);

    if (use_fast_fed) {
        r->value |= REG_0x02_FASTFED;
    } else {
        r->value &= ~REG_0x02_FASTFED;
    }

  /* in case of automatic go home, move until home sensor */
    if (flags & MOTOR_FLAG_AUTO_GO_HOME) {
        r->value |= REG_0x02_AGOHOME | REG_0x02_NOTHOME;
    }

  /* disable backtracking */
  if ((flags & MOTOR_FLAG_DISABLE_BUFFER_FULL_MOVE)
      ||(scan_yres>=2400)
      ||(scan_yres>=sensor.optical_res))
    {
        r->value |= REG_0x02_ACDCDIS;
    }

  /* scan and backtracking slope table */
  sanei_genesys_slope_table(scan_table,
                            &scan_steps,
                            scan_yres,
                            exposure,
                            dev->motor.base_ydpi,
                            step_type,
                            factor,
                            dev->model->motor_id,
                            gl843_motor_profiles);
    gl843_send_slope_table(dev, SCAN_TABLE, scan_table, scan_steps * factor);
    gl843_send_slope_table(dev, BACKTRACK_TABLE, scan_table, scan_steps * factor);

  /* STEPNO */
    r = sanei_genesys_get_address(reg, REG_STEPNO);
  r->value = scan_steps;

  /* FSHDEC */
    r = sanei_genesys_get_address(reg, REG_FSHDEC);
  r->value = scan_steps;

  /* fast table */
    StepType fast_step_type = StepType::FULL;
    if (static_cast<unsigned>(step_type) <= static_cast<unsigned>(fast_step_type)) {
        fast_step_type = step_type;
    }

  sanei_genesys_slope_table(fast_table,
                            &fast_steps,
                            sanei_genesys_get_lowest_ydpi(dev),
                            exposure,
                            dev->motor.base_ydpi,
                            fast_step_type,
                            factor,
                            dev->model->motor_id,
                            gl843_motor_profiles);
    gl843_send_slope_table(dev, STOP_TABLE, fast_table, fast_steps * factor);
    gl843_send_slope_table(dev, FAST_TABLE, fast_table, fast_steps * factor);
    gl843_send_slope_table(dev, HOME_TABLE, fast_table, fast_steps * factor);

  /* FASTNO */
    r = sanei_genesys_get_address(reg, REG_FASTNO);
  r->value = fast_steps;

  /* FMOVNO */
    r = sanei_genesys_get_address(reg, REG_FMOVNO);
  r->value = fast_steps;

  /* substract acceleration distance from feedl */
  feedl=feed_steps;
    feedl <<= static_cast<unsigned>(step_type);

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

    r = sanei_genesys_get_address(reg, REG_0x1E);
  r->value &= 0xf0;		/* 0 dummy lines */
  r->value |= scan_dummy;	/* dummy lines */

    reg->set8_mask(REG_0x67, static_cast<unsigned>(step_type) << REG_0x67S_STEPSEL, 0xc0);
    reg->set8_mask(REG_0x68, static_cast<unsigned>(step_type) << REG_0x68S_FSTPSEL, 0xc0);

  /* steps for STOP table */
    r = sanei_genesys_get_address(reg, REG_FMOVDEC);
  r->value = fast_steps;

  /* Vref XXX STEF XXX : optical divider or step type ? */
  r = sanei_genesys_get_address (reg, 0x80);
  if (!(dev->model->flags & GENESYS_FLAG_FULL_HWDPI_MODE))
    {
      r->value = 0x50;
        coeff = sensor.get_hwdpi_divisor_for_dpi(scan_yres);
        if (dev->model->motor_id == MotorId::KVSS080) {
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
 * @param ccd_size_divisor true specifies how much x coordinates must be shrunk
 * @param color_filter to choose the color channel used in gray scans
 * @param flags to drive specific settings such no calibration, XPA use ...
 */
static void gl843_init_optical_regs_scan(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                         Genesys_Register_Set* reg, unsigned int exposure,
                                         const ScanSession& session)
{
    DBG_HELPER_ARGS(dbg, "exposure=%d", exposure);
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

    dev->cmd_set->set_fe(dev, sensor, AFE_SET);

  /* enable shading */
    r = sanei_genesys_get_address (reg, REG_0x01);
    r->value &= ~REG_0x01_SCAN;
    if ((session.params.flags & SCAN_FLAG_DISABLE_SHADING) ||
        (dev->model->flags & GENESYS_FLAG_NO_CALIBRATION ||
        (dev->model->flags & GENESYS_FLAG_CALIBRATION_HOST_SIDE)))
    {
        r->value &= ~REG_0x01_DVDSET;
    } else {
        r->value |= REG_0x01_DVDSET;
    }

    bool use_shdarea = dpihw > 600;
    if (dev->model->model_id == ModelId::CANON_8400F) {
        use_shdarea = session.params.xres <= 400;
    }
    if (use_shdarea) {
        r->value |= REG_0x01_SHDAREA;
    } else {
        r->value &= ~REG_0x01_SHDAREA;
    }

    r = sanei_genesys_get_address (reg, REG_0x03);
    if (dev->model->model_id == ModelId::CANON_8600F) {
        r->value |= REG_0x03_AVEENB;
    } else {
        r->value &= ~REG_0x03_AVEENB;
  }

    // FIXME: we probably don't need to set exposure to registers at this point. It was this way
    // before a refactor.
    sanei_genesys_set_lamp_power(dev, sensor, *reg,
                                 !(session.params.flags & SCAN_FLAG_DISABLE_LAMP));

  /* select XPA */
    r->value &= ~REG_0x03_XPASEL;
    if (session.params.flags & SCAN_FLAG_USE_XPA) {
        r->value |= REG_0x03_XPASEL;
    }
    reg->state.is_xpa_on = session.params.flags & SCAN_FLAG_USE_XPA;

  /* BW threshold */
    r = sanei_genesys_get_address(reg, REG_0x2E);
  r->value = dev->settings.threshold;
    r = sanei_genesys_get_address(reg, REG_0x2F);
  r->value = dev->settings.threshold;

  /* monochrome / color scan */
    r = sanei_genesys_get_address(reg, REG_0x04);
    switch (session.params.depth) {
    case 8:
            r->value &= ~(REG_0x04_LINEART | REG_0x04_BITSET);
      break;
    case 16:
            r->value &= ~REG_0x04_LINEART;
            r->value |= REG_0x04_BITSET;
      break;
    }

    r->value &= ~(REG_0x04_FILTER | REG_0x04_AFEMOD);
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
        switch (dev->frontend.layout.type) {
            case FrontendType::WOLFSON:
                r->value |= 0x10; // pixel by pixel
                break;
            case FrontendType::ANALOG_DEVICES:
                r->value |= 0x20; // slow color pixel by pixel
                break;
            default:
                throw SaneException("Invalid frontend type %d",
                                    static_cast<unsigned>(dev->frontend.layout.type));
        }
    }

    sanei_genesys_set_dpihw(*reg, sensor, dpihw);

    if (should_enable_gamma(session, sensor)) {
        reg->find_reg(REG_0x05).value |= REG_0x05_GMMENB;
    } else {
        reg->find_reg(REG_0x05).value &= ~REG_0x05_GMMENB;
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

  /* MAXWD is expressed in 2 words unit */
  /* nousedspace = (mem_bank_range * 1024 / 256 -1 ) * 4; */
    // BUG: the division by ccd_size_divisor likely does not make sense
    reg->set24(REG_MAXWD, (session.output_line_bytes / session.ccd_size_divisor) >> 1);

    reg->set16(REG_LPERIOD, exposure / tgtime);
  DBG(DBG_io2, "%s: exposure used=%d\n", __func__, exposure/tgtime);

  r = sanei_genesys_get_address (reg, REG_DUMMY);
  r->value = sensor.dummy_pixel;
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
    StepType scan_step_type = sanei_genesys_compute_step_type(gl843_motor_profiles,
                                                              dev->model->motor_id,
                                                              exposure);

  DBG(DBG_info, "%s : exposure=%d pixels\n", __func__, exposure);
    DBG(DBG_info, "%s : scan_step_type=%d\n", __func__, static_cast<unsigned>(scan_step_type));

    // now _LOGICAL_ optical values used are known, setup registers
    gl843_init_optical_regs_scan(dev, sensor, reg, exposure, session);

  /*** motor parameters ***/
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

    build_image_pipeline(dev, session);

    dev->read_active = true;

    dev->session = session;

  dev->total_bytes_read = 0;
    dev->total_bytes_to_read = session.output_line_bytes_requested * session.params.lines;

    DBG(DBG_info, "%s: total bytes to send = %zu\n", __func__, dev->total_bytes_to_read);
}

ScanSession CommandSetGl843::calculate_scan_session(const Genesys_Device* dev,
                                                    const Genesys_Sensor& sensor,
                                                    const Genesys_Settings& settings) const
{
    DBG_HELPER(dbg);
    debug_dump(DBG_info, settings);

  int start;

  /* we have 2 domains for ccd: xres below or above half ccd max dpi */
  unsigned ccd_size_divisor = sensor.get_ccd_size_divisor_for_dpi(settings.xres);

    if (settings.scan_method == ScanMethod::TRANSPARENCY ||
        settings.scan_method == ScanMethod::TRANSPARENCY_INFRARED)
    {
        start = static_cast<int>(dev->model->x_offset_ta);
    } else {
        start = static_cast<int>(dev->model->x_offset);
    }

    if (dev->model->model_id == ModelId::CANON_8600F)
    {
        // FIXME: this is probably just an artifact of a bug elsewhere
        start /= ccd_size_divisor;
    }

    start += static_cast<int>(settings.tl_x);
    start = static_cast<int>((start * sensor.optical_res) / MM_PER_INCH);

    ScanSession session;
    session.params.xres = settings.xres;
    session.params.yres = settings.yres;
    session.params.startx = start; // not used
    session.params.starty = 0; // not used
    session.params.pixels = settings.pixels;
    session.params.requested_pixels = settings.requested_pixels;
    session.params.lines = settings.lines;
    session.params.depth = settings.depth;
    session.params.channels = settings.get_channels();
    session.params.scan_method = settings.scan_method;
    session.params.scan_mode = settings.scan_mode;
    session.params.color_filter = settings.color_filter;
    session.params.flags = 0;

    compute_session(dev, session, sensor);

    return session;
}

/**
 * for fast power saving methods only, like disabling certain amplifiers
 * @param dev device to use
 * @param enable true to set inot powersaving
 * */
void CommandSetGl843::save_power(Genesys_Device* dev, bool enable) const
{
    DBG_HELPER_ARGS(dbg, "enable = %d", enable);

    // switch KV-SS080 lamp off
    if (dev->model->gpio_id == GpioId::KVSS080) {
        uint8_t val = dev->read_register(REG_0x6C);
        if (enable) {
            val &= 0xef;
        } else {
            val |= 0x10;
        }
        dev->write_register(REG_0x6C, val);
    }
}

void CommandSetGl843::set_powersaving(Genesys_Device* dev, int delay /* in minutes */) const
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
    uint8_t val = reg->get8(REG_0x01);
    val &= ~REG_0x01_SCAN;
    reg->set8(REG_0x01, val);
    dev->write_register(REG_0x01, val);
    sanei_genesys_sleep_ms(100);
}

static void gl843_stop_action(Genesys_Device* dev)
{
    DBG_HELPER(dbg);
  uint8_t val;
  unsigned int loop;

    val = sanei_genesys_get_status(dev);
  if (DBG_LEVEL >= DBG_io)
    {
      sanei_genesys_print_status (val);
    }

    uint8_t val40 = dev->read_register(REG_0x40);

  /* only stop action if needed */
    if (!(val40 & REG_0x40_DATAENB) && !(val40 & REG_0x40_MOTMFLG))
    {
      DBG(DBG_info, "%s: already stopped\n", __func__);
      return;
    }

  /* ends scan 646  */
    val = dev->reg.get8(REG_0x01);
    val &= ~REG_0x01_SCAN;
    dev->reg.set8(REG_0x01, val);
    dev->write_register(REG_0x01, val);

  sanei_genesys_sleep_ms(100);

  loop = 10;
  while (loop > 0)
    {
        val = sanei_genesys_get_status(dev);
      if (DBG_LEVEL >= DBG_io)
	{
	  sanei_genesys_print_status (val);
	}
        val40 = dev->read_register(0x40);

      /* if scanner is in command mode, we are done */
        if (!(val40 & REG_0x40_DATAENB) && !(val40 & REG_0x40_MOTMFLG)
            && !(val & REG_0x41_MOTORENB))
	{
      return;
	}

      sanei_genesys_sleep_ms(100);
      loop--;
    }

  throw SaneException(SANE_STATUS_IO_ERROR, "could not stop motor");
}

static bool gl843_get_paper_sensor(Genesys_Device* dev)
{
    DBG_HELPER(dbg);

    uint8_t val = dev->read_register(REG_0x6D);

    return (val & 0x1) == 0;
}

void CommandSetGl843::eject_document(Genesys_Device* dev) const
{
    (void) dev;
    DBG_HELPER(dbg);
}


void CommandSetGl843::load_document(Genesys_Device* dev) const
{
    DBG_HELPER(dbg);
    (void) dev;
}

/**
 * detects end of document and adjust current scan
 * to take it into account
 * used by sheetfed scanners
 */
void CommandSetGl843::detect_document_end(Genesys_Device* dev) const
{
    DBG_HELPER(dbg);
    bool paper_loaded = gl843_get_paper_sensor(dev);

  /* sheetfed scanner uses home sensor as paper present */
    if (dev->document && !paper_loaded) {
      DBG(DBG_info, "%s: no more document\n", __func__);
        dev->document = false;

        unsigned scanned_lines = 0;
        catch_all_exceptions(__func__, [&](){ sanei_genesys_read_scancnt(dev, &scanned_lines); });

        std::size_t output_lines = dev->session.output_line_count;

        std::size_t offset_lines = static_cast<std::size_t>(
                (dev->model->post_scan * dev->session.params.yres) / MM_PER_INCH);

        std::size_t scan_end_lines = scanned_lines + offset_lines;

        std::size_t remaining_lines = dev->get_pipeline_source().remaining_bytes() /
                dev->session.output_line_bytes_raw;

        DBG(DBG_io, "%s: scanned_lines=%u\n", __func__, scanned_lines);
        DBG(DBG_io, "%s: scan_end_lines=%zu\n", __func__, scan_end_lines);
        DBG(DBG_io, "%s: output_lines=%zu\n", __func__, output_lines);
        DBG(DBG_io, "%s: remaining_lines=%zu\n", __func__, remaining_lines);

        if (scan_end_lines > output_lines) {
            auto skip_lines = scan_end_lines - output_lines;

            if (remaining_lines > skip_lines) {
                DBG(DBG_io, "%s: skip_lines=%zu\n", __func__, skip_lines);

                remaining_lines -= skip_lines;
                dev->get_pipeline_source().set_remaining_bytes(remaining_lines *
                                                               dev->session.output_line_bytes_raw);
                dev->total_bytes_to_read -= skip_lines * dev->session.output_line_bytes_requested;
            }
        }
    }
}

// enables or disables XPA slider motor
static void gl843_set_xpa_motor_power(Genesys_Device* dev, bool set)
{
    DBG_HELPER(dbg);
    uint8_t val;

    if (dev->model->model_id == ModelId::CANON_8400F) {

        if (set) {
            val = dev->read_register(0x6c);
            val &= ~(REG_0x6C_GPIO16 | REG_0x6C_GPIO13);
            if (dev->session.output_resolution >= 2400) {
                val &= ~REG_0x6C_GPIO10;
            }
            dev->write_register(0x6c, val);

            val = dev->read_register(0xa9);
            val |= REG_0xA9_GPO30;
            val &= ~REG_0xA9_GPO29;
            dev->write_register(0xa9, val);
        } else {
            val = dev->read_register(0x6c);
            val |= REG_0x6C_GPIO16 | REG_0x6C_GPIO13;
            dev->write_register(0x6c, val);

            val = dev->read_register(0xa9);
            val &= ~REG_0xA9_GPO30;
            val |= REG_0xA9_GPO29;
            dev->write_register(0xa9, val);
        }
    } else if (dev->model->model_id == ModelId::CANON_8600F) {
        if (set) {
            val = dev->read_register(REG_0x6C);
            val &= ~REG_0x6C_GPIO14;
            if (dev->session.output_resolution >= 2400) {
                val |= REG_0x6C_GPIO10;
            }
            dev->write_register(REG_0x6C, val);

            val = dev->read_register(REG_0xA6);
            val |= REG_0xA6_GPIO17;
            val &= ~REG_0xA6_GPIO23;
            dev->write_register(REG_0xA6, val);
        } else {
            val = dev->read_register(REG_0x6C);
            val |= REG_0x6C_GPIO14;
            val &= ~REG_0x6C_GPIO10;
            dev->write_register(REG_0x6C, val);

            val = dev->read_register(REG_0xA6);
            val &= ~REG_0xA6_GPIO17;
            val &= ~REG_0xA6_GPIO23;
            dev->write_register(REG_0xA6, val);
        }
    } else if (dev->model->model_id == ModelId::HP_SCANJET_G4050) {
        if (set) {
            // set MULTFILM et GPOADF
            val = dev->read_register(REG_0x6B);
            val |=REG_0x6B_MULTFILM|REG_0x6B_GPOADF;
            dev->write_register(REG_0x6B, val);

            val = dev->read_register(REG_0x6C);
            val &= ~REG_0x6C_GPIO15;
            dev->write_register(REG_0x6C, val);

            /* Motor power ? No move at all without this one */
            val = dev->read_register(REG_0xA6);
            val |= REG_0xA6_GPIO20;
            dev->write_register(REG_0xA6, val);

            val = dev->read_register(REG_0xA8);
            val &= ~REG_0xA8_GPO27;
            dev->write_register(REG_0xA8, val);

            val = dev->read_register(REG_0xA9);
            val |= REG_0xA9_GPO32|REG_0xA9_GPO31;
            dev->write_register(REG_0xA9, val);
        } else {
            // unset GPOADF
            val = dev->read_register(REG_0x6B);
            val &= ~REG_0x6B_GPOADF;
            dev->write_register(REG_0x6B, val);

            val = dev->read_register(REG_0xA8);
            val |= REG_0xA8_GPO27;
            dev->write_register(REG_0xA8, val);

            val = dev->read_register(REG_0xA9);
            val &= ~REG_0xA9_GPO31;
            dev->write_register(REG_0xA9, val);
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
        ModelId model_id;
        ScanMethod scan_method;
        GenesysRegisterSettingSet regs_on;
        GenesysRegisterSettingSet regs_off;
    };

    // FIXME: BUG: we're not clearing the registers to the previous state when returning back when
    // turning off the lamp
    LampSettings settings[] = {
        {   ModelId::CANON_8400F, ScanMethod::TRANSPARENCY, {
                { 0xa6, 0x34, 0xf4 },
            }, {
                { 0xa6, 0x40, 0x70 },
            }
        },
        {   ModelId::CANON_8400F, ScanMethod::TRANSPARENCY_INFRARED, {
                { 0x6c, 0x40, 0x40 },
                { 0xa6, 0x01, 0xff },
            }, {
                { 0x6c, 0x00, 0x40 },
                { 0xa6, 0x00, 0xff },
            }
        },
        {   ModelId::CANON_8600F, ScanMethod::TRANSPARENCY, {
                { 0xa6, 0x34, 0xf4 },
                { 0xa7, 0xe0, 0xe0 },
            }, {
                { 0xa6, 0x40, 0x70 },
            }
        },
        {   ModelId::CANON_8600F, ScanMethod::TRANSPARENCY_INFRARED, {
                { 0xa6, 0x00, 0xc0 },
                { 0xa7, 0xe0, 0xe0 },
                { 0x6c, 0x80, 0x80 },
            }, {
                { 0xa6, 0x00, 0xc0 },
                { 0x6c, 0x00, 0x80 },
            }
        },
        {   ModelId::PLUSTEK_OPTICFILM_7200I, ScanMethod::TRANSPARENCY, {
            }, {
                { 0xa6, 0x40, 0x70 }, // BUG: remove this cleanup write, it was enabled by accident
            }
        },
        {   ModelId::PLUSTEK_OPTICFILM_7200I, ScanMethod::TRANSPARENCY_INFRARED, {
                { 0xa8, 0x07, 0x07 },
            }, {
                { 0xa8, 0x00, 0x07 },
            }
        },
        {   ModelId::PLUSTEK_OPTICFILM_7300, ScanMethod::TRANSPARENCY, {}, {} },
        {   ModelId::PLUSTEK_OPTICFILM_7500I, ScanMethod::TRANSPARENCY, {}, {} },
        {   ModelId::PLUSTEK_OPTICFILM_7500I, ScanMethod::TRANSPARENCY_INFRARED, {
                { 0xa8, 0x07, 0x07 },
            }, {
                { 0xa8, 0x00, 0x07 },
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
void CommandSetGl843::begin_scan(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                 Genesys_Register_Set* reg, bool start_motor) const
{
    DBG_HELPER(dbg);
    (void) sensor;

  /* set up GPIO for scan */
    switch(dev->model->gpio_id) {
      /* KV case */
        case GpioId::KVSS080:
            dev->write_register(REG_0xA9, 0x00);
            dev->write_register(REG_0xA6, 0xf6);
            // blinking led
            dev->write_register(0x7e, 0x04);
            break;
        case GpioId::G4050:
            dev->write_register(REG_0xA7, 0xfe);
            dev->write_register(REG_0xA8, 0x3e);
            dev->write_register(REG_0xA9, 0x06);
            if ((reg->get8(0x05) & REG_0x05_DPIHW) == REG_0x05_DPIHW_600) {
                dev->write_register(REG_0x6C, 0x20);
                dev->write_register(REG_0xA6, 0x44);
            } else {
                dev->write_register(REG_0x6C, 0x60);
                dev->write_register(REG_0xA6, 0x46);
            }

            if (reg->state.is_xpa_on && reg->state.is_lamp_on) {
                gl843_set_xpa_lamp_power(dev, true);
            }

            if (reg->state.is_xpa_on) {
                dev->needs_home_ta = true;
                gl843_set_xpa_motor_power(dev, true);
            }

            // blinking led
            dev->write_register(REG_0x7E, 0x01);
            break;
        case GpioId::CANON_8400F:
        case GpioId::CANON_8600F:
            if (reg->state.is_xpa_on && reg->state.is_lamp_on) {
                gl843_set_xpa_lamp_power(dev, true);
            }
            if (reg->state.is_xpa_on) {
                dev->needs_home_ta = true;
                gl843_set_xpa_motor_power(dev, true);
            }
            break;
        case GpioId::PLUSTEK_OPTICFILM_7200I:
        case GpioId::PLUSTEK_OPTICFILM_7300:
        case GpioId::PLUSTEK_OPTICFILM_7500I: {
            if (reg->state.is_xpa_on && reg->state.is_lamp_on) {
                gl843_set_xpa_lamp_power(dev, true);
            }
            break;
        }
        case GpioId::CANON_4400F:
        default:
            break;
    }

    // clear scan and feed count
    dev->write_register(REG_0x0D, REG_0x0D_CLRLNCNT | REG_0x0D_CLRMCNT);

    // enable scan and motor
    uint8_t val = dev->read_register(REG_0x01);
    val |= REG_0x01_SCAN;
    dev->write_register(REG_0x01, val);

    if (start_motor) {
        dev->write_register(REG_0x0F, 1);
    } else {
        dev->write_register(REG_0x0F, 0);
    }
}


// Send the stop scan command
void CommandSetGl843::end_scan(Genesys_Device* dev, Genesys_Register_Set* reg,
                               bool check_stop) const
{
    DBG_HELPER_ARGS(dbg, "check_stop = %d", check_stop);

    // post scan gpio
    dev->write_register(0x7e, 0x00);

    // turn off XPA lamp if needed
    // BUG: the if condition below probably shouldn't be enabled when XPA is off
    if (reg->state.is_xpa_on || reg->state.is_lamp_on) {
        gl843_set_xpa_lamp_power(dev, false);
    }

    if (!dev->model->is_sheetfed) {
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
    dev->write_register(REG_0x0D, REG_0x0D_CLRLNCNT | REG_0x0D_CLRMCNT);

  /* set up for reverse and no scan */
    r = sanei_genesys_get_address (&local_reg, REG_0x02);
    r->value |= REG_0x02_MTRREV;
    r = sanei_genesys_get_address (&local_reg, REG_0x01);
    r->value &= ~REG_0x01_SCAN;

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
            val = sanei_genesys_get_status(dev);

          if (DBG_LEVEL >= DBG_io2)
            {
              sanei_genesys_print_status (val);
            }

        if (val & REG_0x41_HOMESNR)	/* home sensor */
	    {
	      DBG(DBG_info, "%s: reached home position\n", __func__);
	      DBG(DBG_proc, "%s: finished\n", __func__);

            gl843_set_xpa_motor_power(dev, false);
            dev->needs_home_ta = false;

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
void CommandSetGl843::slow_back_home(Genesys_Device* dev, bool wait_until_home) const
{
    DBG_HELPER_ARGS(dbg, "wait_until_home = %d", wait_until_home);
  Genesys_Register_Set local_reg;
  GenesysRegister *r;
  uint8_t val;
  int loop = 0;

    if (dev->needs_home_ta) {
        gl843_park_xpa_lamp(dev);
    }

  /* regular slow back home */
  dev->scanhead_position_in_steps = 0;

    // first read gives HOME_SENSOR true
    val = sanei_genesys_get_status(dev);

  sanei_genesys_sleep_ms(100);

    // second is reliable
    val = sanei_genesys_get_status(dev);

  if (DBG_LEVEL >= DBG_io)
    {
      sanei_genesys_print_status (val);
    }
  if (val & HOMESNR)	/* is sensor at home? */
    {
      return;
    }

  local_reg = dev->reg;
    unsigned resolution = sanei_genesys_get_lowest_ydpi(dev);

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
    compute_session(dev, session, sensor);

    gl843_init_scan_regs(dev, sensor, &local_reg, session);

    // clear scan and feed count
    dev->write_register(REG_0x0D, REG_0x0D_CLRLNCNT | REG_0x0D_CLRMCNT);

  /* set up for reverse and no scan */
    r = sanei_genesys_get_address(&local_reg, REG_0x02);
    r->value |= REG_0x02_MTRREV;
    r = sanei_genesys_get_address(&local_reg, REG_0x01);
    r->value &= ~REG_0x01_SCAN;

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
            val = sanei_genesys_get_status(dev);

          if (DBG_LEVEL >= DBG_io2)
            {
              sanei_genesys_print_status (val);
            }

            if (val & REG_0x41_HOMESNR)	/* home sensor */
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
void CommandSetGl843::search_start_position(Genesys_Device* dev) const
{
    DBG_HELPER(dbg);
  Genesys_Register_Set local_reg;

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
    compute_session(dev, session, sensor);

    gl843_init_scan_regs(dev, sensor, &local_reg, session);

    // send to scanner
    dev->write_registers(local_reg);

    dev->cmd_set->begin_scan(dev, sensor, &local_reg, true);

    wait_until_buffer_non_empty(dev);

    // now we're on target, we can read data
    Image image = read_unshuffled_image_from_scanner(dev, session, session.output_total_bytes_raw);

    gl843_stop_action_no_move(dev, &local_reg);

    if (DBG_LEVEL >= DBG_data) {
        sanei_genesys_write_pnm_file("gl843_search_position.pnm", image);
    }

    dev->cmd_set->end_scan(dev, &local_reg, true);

  /* update regs to copy ASIC internal state */
  dev->reg = local_reg;

    for (auto& sensor_update :
            sanei_genesys_find_sensors_all_for_write(dev, dev->model->default_method))
    {
        sanei_genesys_search_reference_point(dev, sensor_update, image.get_row_ptr(0), 0, dpi,
                                             pixels, dev->model->search_lines);
    }
}

// sets up register for coarse gain calibration
// todo: check it for scanners using it
void CommandSetGl843::init_regs_for_coarse_calibration(Genesys_Device* dev,
                                                       const Genesys_Sensor& sensor,
                                                       Genesys_Register_Set& regs) const
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
    compute_session(dev, session, sensor);

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
  uint8_t val;

  /* prepare local registers */
  local_reg = dev->reg;

    const auto& resolution_settings = dev->model->get_resolution_settings(dev->model->default_method);
    unsigned resolution = resolution_settings.get_min_resolution_y();

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
    compute_session(dev, session, sensor);

    gl843_init_scan_regs(dev, sensor, &local_reg, session);

    // clear scan and feed count
    dev->write_register(REG_0x0D, REG_0x0D_CLRLNCNT);
    dev->write_register(REG_0x0D, REG_0x0D_CLRMCNT);

  /* set up for no scan */
  r = sanei_genesys_get_address(&local_reg, REG_0x01);
    r->value &= ~REG_0x01_SCAN;

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
        val = sanei_genesys_get_status(dev);
    } while (!(val & FEEDFSH));

  // looks like the scanner locks up if we scan immediately after feeding
  sanei_genesys_sleep_ms(100);
}

// init registers for shading calibration shading calibration is done at dpihw
void CommandSetGl843::init_regs_for_shading(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                            Genesys_Register_Set& regs) const
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
        dev->model->model_id == ModelId::CANON_8600F &&
        dev->settings.xres == 4800)
    {
        float offset = static_cast<float>(dev->model->x_offset_ta);
        offset /= calib_sensor.get_ccd_size_divisor_for_dpi(resolution);
        offset = static_cast<float>((offset * calib_sensor.optical_res) / MM_PER_INCH);

        float size = static_cast<float>(dev->model->x_size_ta);
        size /= calib_sensor.get_ccd_size_divisor_for_dpi(resolution);
        size = static_cast<float>((size * calib_sensor.optical_res) / MM_PER_INCH);

        dev->calib_pixels_offset = static_cast<std::size_t>(offset);
        dev->calib_pixels = static_cast<std::size_t>(size);
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
        move = static_cast<int>(dev->model->y_offset_calib_white_ta - dev->model->y_offset_sensor_to_ta);
    flags |= SCAN_FLAG_USE_XPA;
    } else {
        move = static_cast<int>(dev->model->y_offset_calib_white);
    }

    move = static_cast<int>((move * resolution) / MM_PER_INCH);

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
    compute_session(dev, session, calib_sensor);

    gl843_init_scan_regs(dev, calib_sensor, &regs, session);

     // the pixel number may be updated to conform to scanner constraints
    dev->calib_pixels = session.output_pixels;

    dev->calib_session = session;
    dev->calib_total_bytes_to_read = session.output_total_bytes_raw;

  dev->scanhead_position_in_steps += dev->calib_lines + move;

    dev->write_registers(regs);
}

/** @brief set up registers for the actual scan
 */
void CommandSetGl843::init_regs_for_scan(Genesys_Device* dev, const Genesys_Sensor& sensor) const
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
            move = static_cast<float>(dev->model->y_offset_ta - dev->model->y_offset_sensor_to_ta);
        }
        flags |= SCAN_FLAG_USE_XPA;
    } else {
        if (dev->ignore_offsets) {
            move = 0;
        } else {
            move = static_cast<float>(dev->model->y_offset);
        }
    }

    move += static_cast<float>(dev->settings.tl_y);
    move = static_cast<float>((move * move_dpi) / MM_PER_INCH);
  DBG(DBG_info, "%s: move=%f steps\n", __func__, move);

  /* start */
    if (dev->settings.scan_method==ScanMethod::TRANSPARENCY ||
        dev->settings.scan_method == ScanMethod::TRANSPARENCY_INFRARED)
    {
        start = static_cast<float>(dev->model->x_offset_ta);
    } else {
        start = static_cast<float>(dev->model->x_offset);
    }

    if (dev->model->model_id == ModelId::CANON_8400F ||
        dev->model->model_id == ModelId::CANON_8600F)
    {
        // FIXME: this is probably just an artifact of a bug elsewhere
        start /= sensor.get_ccd_size_divisor_for_dpi(dev->settings.xres);
    }

    start = static_cast<float>(start + dev->settings.tl_x);
    start = static_cast<float>((start * sensor.optical_res) / MM_PER_INCH);

    ScanSession session;
    session.params.xres = dev->settings.xres;
    session.params.yres = dev->settings.yres;
    session.params.startx = static_cast<unsigned>(start);
    session.params.starty = static_cast<unsigned>(move);
    session.params.pixels = dev->settings.pixels;
    session.params.requested_pixels = dev->settings.requested_pixels;
    session.params.lines = dev->settings.lines;
    session.params.depth = dev->settings.depth;
    session.params.channels = dev->settings.get_channels();
    session.params.scan_method = dev->settings.scan_method;
    session.params.scan_mode = dev->settings.scan_mode;
    session.params.color_filter = dev->settings.color_filter;
    session.params.flags = flags;
    compute_session(dev, session, sensor);

    gl843_init_scan_regs(dev, sensor, &dev->reg, session);
}

/**
 * This function sends gamma tables to ASIC
 */
void CommandSetGl843::send_gamma_table(Genesys_Device* dev, const Genesys_Sensor& sensor) const
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
SensorExposure CommandSetGl843::led_calibration(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                                Genesys_Register_Set& regs) const
{
    DBG_HELPER(dbg);
  int num_pixels;
  int avg[3], avga, avge;
  int turn;
  uint16_t expr, expg, expb;

    // offset calibration is always done in color mode
    unsigned channels = 3;

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
    session.params.depth = 16;
    session.params.channels = channels;
    session.params.scan_method = dev->settings.scan_method;
    session.params.scan_mode = ScanColorMode::COLOR_SINGLE_PASS;
    session.params.color_filter = dev->settings.color_filter;
    session.params.flags =  SCAN_FLAG_DISABLE_SHADING |
                            SCAN_FLAG_DISABLE_GAMMA |
                            SCAN_FLAG_SINGLE_LINE |
                            SCAN_FLAG_IGNORE_LINE_DISTANCE;
    compute_session(dev, session, calib_sensor);

    gl843_init_scan_regs(dev, calib_sensor, &regs, session);

    dev->write_registers(regs);

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

    bool acceptable = false;
  do
    {

      calib_sensor.exposure.red = expr;
      calib_sensor.exposure.green = expg;
      calib_sensor.exposure.blue = expb;

      sanei_genesys_set_exposure(regs, calib_sensor.exposure);

        dev->write_registers(regs);

      DBG(DBG_info, "%s: starting first line reading\n", __func__);
        dev->cmd_set->begin_scan(dev, calib_sensor, &regs, true);
        auto image = read_unshuffled_image_from_scanner(dev, session,
                                                        session.output_total_bytes_raw);
        gl843_stop_action_no_move(dev, &regs);

      if (DBG_LEVEL >= DBG_data)
	{
          char fn[30];
            std::snprintf(fn, 30, "gl843_led_%02d.pnm", turn);
            sanei_genesys_write_pnm_file(fn, image);
	}

        acceptable = true;

        for (unsigned ch = 0; ch < channels; ch++) {
            avg[ch] = 0;
            for (std::size_t x = 0; x < image.get_width(); x++) {
                avg[ch] += image.get_raw_channel(x, 0, ch);
            }
            avg[ch] /= image.get_width();
        }

      DBG(DBG_info, "%s: average: %d,%d,%d\n", __func__, avg[0], avg[1], avg[2]);

        acceptable = true;

      if (avg[0] < avg[1] * 0.95 || avg[1] < avg[0] * 0.95 ||
	  avg[0] < avg[2] * 0.95 || avg[2] < avg[0] * 0.95 ||
	  avg[1] < avg[2] * 0.95 || avg[2] < avg[1] * 0.95)
        acceptable = false;

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

    slow_back_home(dev, true);

    return calib_sensor.exposure;
}



/**
 * average dark pixels of a 8 bits scan of a given channel
 */
static int dark_average_channel(const Image& image, unsigned black, unsigned channel)
{
    auto channels = get_pixel_channels(image.get_format());

    unsigned avg[3];

    // computes average values on black margin
    for (unsigned ch = 0; ch < channels; ch++) {
        avg[ch] = 0;
        unsigned count = 0;
        // FIXME: start with the second line because the black pixels often have noise on the first
        // line; the cause is probably incorrectly cleaned up previous scan
        for (std::size_t y = 1; y < image.get_height(); y++) {
            for (unsigned j = 0; j < black; j++) {
                avg[ch] += image.get_raw_channel(j, y, ch);
                count++;
            }
        }
        if (count > 0) {
            avg[ch] /= count;
        }
        DBG(DBG_info, "%s: avg[%d] = %d\n", __func__, ch, avg[ch]);
    }
    DBG(DBG_info, "%s: average = %d\n", __func__, avg[channel]);
    return avg[channel];
}

/** @brief calibrate AFE offset
 * Iterate doing scans at target dpi until AFE offset if correct. One
 * color line is scanned at a time. Scanning head doesn't move.
 * @param dev device to calibrate
 */
void CommandSetGl843::offset_calibration(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                         Genesys_Register_Set& regs) const
{
    DBG_HELPER(dbg);

    if (dev->frontend.layout.type != FrontendType::WOLFSON)
        return;

    unsigned channels;
    int pass, resolution, lines;
  int topavg[3], bottomavg[3], avg[3];
  int top[3], bottom[3], black_pixels, pixels, factor, dpihw;

  /* offset calibration is always done in color mode */
  channels = 3;
  lines = 8;

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
        dev->model->model_id == ModelId::CANON_8600F &&
        dev->settings.xres == 4800)
    {
        start_pixel = static_cast<int>(dev->model->x_offset_ta);
        start_pixel /= calib_sensor.get_ccd_size_divisor_for_dpi(resolution);
        start_pixel = static_cast<int>((start_pixel * calib_sensor.optical_res) / MM_PER_INCH);

        target_pixels = static_cast<int>(dev->model->x_size_ta);
        target_pixels /= calib_sensor.get_ccd_size_divisor_for_dpi(resolution);
        target_pixels = static_cast<int>((target_pixels * calib_sensor.optical_res) / MM_PER_INCH);
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
    session.params.depth = 8;
    session.params.channels = channels;
    session.params.scan_method = dev->settings.scan_method;
    session.params.scan_mode = ScanColorMode::COLOR_SINGLE_PASS;
    session.params.color_filter = ColorFilter::RED;
    session.params.flags = flags;
    compute_session(dev, session, calib_sensor);
    pixels = session.output_pixels;

    DBG(DBG_io, "%s: dpihw       =%d\n", __func__, dpihw);
    DBG(DBG_io, "%s: factor      =%d\n", __func__, factor);
    DBG(DBG_io, "%s: resolution  =%d\n", __func__, resolution);
    DBG(DBG_io, "%s: pixels      =%d\n", __func__, pixels);
    DBG(DBG_io, "%s: black_pixels=%d\n", __func__, black_pixels);
    gl843_init_scan_regs(dev, calib_sensor, &regs, session);

  sanei_genesys_set_motor_power(regs, false);

    // init gain and offset
    for (unsigned ch = 0; ch < 3; ch++)
    {
        bottom[ch] = 10;
        dev->frontend.set_offset(ch, bottom[ch]);
        dev->frontend.set_gain(ch, 0);
    }
    dev->cmd_set->set_fe(dev, calib_sensor, AFE_SET);

    // scan with bottom AFE settings
    dev->write_registers(regs);
    DBG(DBG_info, "%s: starting first line reading\n", __func__);

    dev->cmd_set->begin_scan(dev, calib_sensor, &regs, true);
    auto first_line = read_unshuffled_image_from_scanner(dev, session,
                                                         session.output_total_bytes_raw);
    gl843_stop_action_no_move(dev, &regs);

  if (DBG_LEVEL >= DBG_data)
    {
      char fn[40];
        std::snprintf(fn, 40, "gl843_bottom_offset_%03d_%03d_%03d.pnm",
                      bottom[0], bottom[1], bottom[2]);
        sanei_genesys_write_pnm_file(fn, first_line);
    }

    for (unsigned ch = 0; ch < 3; ch++) {
        bottomavg[ch] = dark_average_channel(first_line, black_pixels, ch);
        DBG(DBG_io2, "%s: bottom avg %d=%d\n", __func__, ch, bottomavg[ch]);
    }

    // now top value
    for (unsigned ch = 0; ch < 3; ch++) {
        top[ch] = 255;
        dev->frontend.set_offset(ch, top[ch]);
    }
    dev->cmd_set->set_fe(dev, calib_sensor, AFE_SET);

    // scan with top AFE values
    dev->write_registers(regs);
    DBG(DBG_info, "%s: starting second line reading\n", __func__);

    dev->cmd_set->begin_scan(dev, calib_sensor, &regs, true);
    auto second_line = read_unshuffled_image_from_scanner(dev, session,
                                                          session.output_total_bytes_raw);
    gl843_stop_action_no_move(dev, &regs);

    for (unsigned ch = 0; ch < 3; ch++){
        topavg[ch] = dark_average_channel(second_line, black_pixels, ch);
        DBG(DBG_io2, "%s: top avg %d=%d\n", __func__, ch, topavg[ch]);
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

        // settings for new scan
        for (unsigned ch = 0; ch < 3; ch++) {
            if (top[ch] - bottom[ch] > 1) {
                dev->frontend.set_offset(ch, (top[ch] + bottom[ch]) / 2);
            }
        }
        dev->cmd_set->set_fe(dev, calib_sensor, AFE_SET);

        // scan with no move
        dev->write_registers(regs);
      DBG(DBG_info, "%s: starting second line reading\n", __func__);
        dev->cmd_set->begin_scan(dev, calib_sensor, &regs, true);
        second_line = read_unshuffled_image_from_scanner(dev, session,
                                                         session.output_total_bytes_raw);
        gl843_stop_action_no_move(dev, &regs);

      if (DBG_LEVEL >= DBG_data)
	{
          char title[100];
          std::snprintf(title, 100, "lines: %d pixels_per_line: %d offsets[0..2]: %d %d %d\n",
                        lines, pixels,
                        dev->frontend.get_offset(0),
                        dev->frontend.get_offset(1),
                        dev->frontend.get_offset(2));
          debug_image_info += title;
          std::copy(second_line.get_row_ptr(0),
                    second_line.get_row_ptr(0) + second_line.get_row_bytes() * second_line.get_height(),
                    std::back_inserter(debug_image));
          debug_image_lines += lines;
	}

        for (unsigned ch = 0; ch < 3; ch++) {
            avg[ch] = dark_average_channel(second_line, black_pixels, ch);
            DBG(DBG_info, "%s: avg[%d]=%d offset=%d\n", __func__, ch, avg[ch],
                dev->frontend.get_offset(ch));
        }

        // compute new boundaries
        for (unsigned ch = 0; ch < 3; ch++) {
            if (topavg[ch] >= avg[ch]) {
                topavg[ch] = avg[ch];
                top[ch] = dev->frontend.get_offset(ch);
            } else {
                bottomavg[ch] = avg[ch];
                bottom[ch] = dev->frontend.get_offset(ch);
            }
        }
    }

  if (DBG_LEVEL >= DBG_data)
    {
      sanei_genesys_write_file("gl843_offset_all_desc.txt",
                               reinterpret_cast<const std::uint8_t*>(debug_image_info.data()),
                               debug_image_info.size());
      sanei_genesys_write_pnm_file("gl843_offset_all.pnm",
                                   debug_image.data(), session.params.depth, channels, pixels,
                                   debug_image_lines);
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
void CommandSetGl843::coarse_gain_calibration(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                              Genesys_Register_Set& regs, int dpi) const
{
    DBG_HELPER_ARGS(dbg, "dpi = %d", dpi);
    int factor, dpihw;
  float coeff;
    int lines;
  int resolution;

    if (dev->frontend.layout.type != FrontendType::WOLFSON)
        return;

    dpihw = sensor.get_logical_hwdpi(dpi);
  factor=sensor.optical_res/dpihw;

    // coarse gain calibration is always done in color mode
    unsigned channels = 3;

  /* follow CKSEL */
    if (dev->model->sensor_id == SensorId::CCD_KVSS080) {
      if(dev->settings.xres<sensor.optical_res)
        {
            coeff = 0.9f;
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
    session.params.depth = 8;
    session.params.channels = channels;
    session.params.scan_method = dev->settings.scan_method;
    session.params.scan_mode = ScanColorMode::COLOR_SINGLE_PASS;
    session.params.color_filter = dev->settings.color_filter;
    session.params.flags = flags;
    compute_session(dev, session, calib_sensor);
    std::size_t pixels = session.output_pixels;

    try {
        gl843_init_scan_regs(dev, calib_sensor, &regs, session);
    } catch (...) {
        catch_all_exceptions(__func__, [&](){ sanei_genesys_set_motor_power(regs, false); });
        throw;
    }

    sanei_genesys_set_motor_power(regs, false);

    dev->write_registers(regs);

    dev->cmd_set->set_fe(dev, calib_sensor, AFE_SET);
    dev->cmd_set->begin_scan(dev, calib_sensor, &regs, true);
    auto line = read_unshuffled_image_from_scanner(dev, session, session.output_total_bytes_raw);
    gl843_stop_action_no_move(dev, &regs);

    if (DBG_LEVEL >= DBG_data) {
        sanei_genesys_write_pnm_file("gl843_gain.pnm", line);
    }

    // average value on each channel
    for (unsigned ch = 0; ch < channels; ch++) {

        std::vector<uint16_t> values;
        // FIXME: start from the second line because the first line often has artifacts. Probably
        // caused by unclean cleanup of previous scan
        for (std::size_t x = pixels / 4; x < (pixels * 3 / 4); x++) {
            values.push_back(line.get_raw_channel(x, 1, ch));
        }

        // pick target value at 95th percentile of all values. There may be a lot of black values
        // in transparency scans for example
        std::sort(values.begin(), values.end());
        uint16_t curr_output = values[unsigned((values.size() - 1) * 0.95)];
        float target_value = calib_sensor.gain_white_ref * coeff;

        int code = compute_frontend_gain(curr_output, target_value, dev->frontend.layout.type);
      dev->frontend.set_gain(ch, code);

        DBG(DBG_proc, "%s: channel %d, max=%d, target=%d, setting:%d\n", __func__, ch, curr_output,
            static_cast<int>(target_value), code);
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

    slow_back_home(dev, true);
}

// wait for lamp warmup by scanning the same line until difference
// between 2 scans is below a threshold
void CommandSetGl843::init_regs_for_warmup(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                           Genesys_Register_Set* reg, int* channels,
                                           int* total_size) const
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
    compute_session(dev, session, calib_sensor);

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
void CommandSetGl843::asic_boot(Genesys_Device* dev, bool cold) const
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
    val = dev->read_register(REG_0x40);
    if (val & REG_0x40_CHKVER) {
        val = dev->read_register(0x00);
        DBG(DBG_info, "%s: reported version for genesys chip is 0x%02x\n", __func__, val);
    }

  /* Set default values for registers */
  gl843_init_registers (dev);

    if (dev->model->model_id == ModelId::CANON_8600F) {
        // turns on vref control for maximum current of the motor driver
        dev->write_register(REG_0x6B, 0x72);
    } else {
        dev->write_register(REG_0x6B, 0x02);
    }

    // Write initial registers
    dev->write_registers(dev->reg);

  // Enable DRAM by setting a rising edge on bit 3 of reg 0x0b
    val = dev->reg.find_reg(0x0b).value & REG_0x0B_DRAMSEL;
    val = (val | REG_0x0B_ENBDRAM);
    dev->write_register(REG_0x0B, val);
  dev->reg.find_reg(0x0b).value = val;

    if (dev->model->model_id == ModelId::CANON_8400F) {
        sanei_genesys_write_0x8c(dev, 0x1e, 0x01);
        sanei_genesys_write_0x8c(dev, 0x10, 0xb4);
        sanei_genesys_write_0x8c(dev, 0x0f, 0x02);
    }
    else if (dev->model->model_id == ModelId::CANON_8600F) {
        sanei_genesys_write_0x8c(dev, 0x10, 0xc8);
    } else if (dev->model->model_id == ModelId::PLUSTEK_OPTICFILM_7300 ||
               dev->model->model_id == ModelId::PLUSTEK_OPTICFILM_7500I)
    {
        sanei_genesys_write_0x8c(dev, 0x10, 0xd4);
    } else {
        sanei_genesys_write_0x8c(dev, 0x10, 0xb4);
    }

  /* CLKSET */
    int clock_freq = REG_0x0B_48MHZ;
    switch (dev->model->model_id) {
        case ModelId::CANON_8600F:
            clock_freq = REG_0x0B_60MHZ;
            break;
        case ModelId::PLUSTEK_OPTICFILM_7200I:
            clock_freq = REG_0x0B_30MHZ;
            break;
        case ModelId::PLUSTEK_OPTICFILM_7300:
        case ModelId::PLUSTEK_OPTICFILM_7500I:
            clock_freq = REG_0x0B_40MHZ;
            break;
        default:
            break;
    }

    val = (dev->reg.find_reg(0x0b).value & ~REG_0x0B_CLKSET) | clock_freq;

    dev->write_register(REG_0x0B, val);
  dev->reg.find_reg(0x0b).value = val;

  /* prevent further writings by bulk write register */
  dev->reg.remove_reg(0x0b);

    if (dev->model->model_id != ModelId::CANON_8600F) {
      // set up end access
      // FIXME: this is overwritten in gl843_init_gpio
        dev->write_register(REG_0xA7, 0x04);
        dev->write_register(REG_0xA9, 0x00);
    }

    // set RAM read address
    dev->write_register(REG_0x29, 0x00);
    dev->write_register(REG_0x2A, 0x00);
    dev->write_register(REG_0x2B, 0x00);

    // setup gpio
    gl843_init_gpio(dev);

  gl843_feed (dev, 300);
  sanei_genesys_sleep_ms(100);
}

/* *
 * initialize backend and ASIC : registers, motor tables, and gamma tables
 * then ensure scanner's head is at home
 */
void CommandSetGl843::init(Genesys_Device* dev) const
{
  DBG_INIT ();
    DBG_HELPER(dbg);

    sanei_genesys_asic_init(dev, 0);
}

void CommandSetGl843::update_hardware_sensors(Genesys_Scanner* s) const
{
    DBG_HELPER(dbg);
  /* do what is needed to get a new set of events, but try to not lose
     any of them.
   */

    uint8_t val = s->dev->read_register(REG_0x6D);

  switch (s->dev->model->gpio_id)
    {
        case GpioId::KVSS080:
            s->buttons[BUTTON_SCAN_SW].write((val & 0x04) == 0);
            break;
        case GpioId::G4050:
            s->buttons[BUTTON_SCAN_SW].write((val & 0x01) == 0);
            s->buttons[BUTTON_FILE_SW].write((val & 0x02) == 0);
            s->buttons[BUTTON_EMAIL_SW].write((val & 0x04) == 0);
            s->buttons[BUTTON_COPY_SW].write((val & 0x08) == 0);
            break;
        case GpioId::CANON_4400F:
        case GpioId::CANON_8400F:
        default:
            break;
    }
}

/** @brief move sensor to transparency adaptor
 * Move sensor to the calibration of the transparency adapator (XPA).
 * @param dev device to use
 */
void CommandSetGl843::move_to_ta(Genesys_Device* dev) const
{
    DBG_HELPER(dbg);

    const auto& resolution_settings = dev->model->get_resolution_settings(dev->model->default_method);
    float resolution = resolution_settings.get_min_resolution_y();

    unsigned multiplier = 16;
    if (dev->model->model_id == ModelId::CANON_8400F) {
        multiplier = 4;
    }
    unsigned feed = static_cast<unsigned>(multiplier * (dev->model->y_offset_sensor_to_ta * resolution) /
                                          MM_PER_INCH);
    gl843_feed(dev, feed);
}


/** @brief search for a full width black or white strip.
 * This function searches for a black or white stripe across the scanning area.
 * When searching backward, the searched area must completely be of the desired
 * color since this area will be used for calibration which scans forward.
 * @param dev scanner device
 * @param forward true if searching forward, false if searching backward
 * @param black true if searching for a black strip, false for a white strip
 */
void CommandSetGl843::search_strip(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                   bool forward, bool black) const
{
    DBG_HELPER_ARGS(dbg, "%s %s",  black ? "black" : "white", forward ? "forward" : "reverse");
  unsigned int pixels, lines, channels;
  Genesys_Register_Set local_reg;
    int dpi;
  unsigned int pass, count, found, x, y;
  GenesysRegister *r;

    dev->cmd_set->set_fe(dev, sensor, AFE_SET);
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
    session.params.depth = 8;
    session.params.channels = channels;
    session.params.scan_method = dev->settings.scan_method;
    session.params.scan_mode = ScanColorMode::GRAY;
    session.params.color_filter = ColorFilter::RED;
    session.params.flags = SCAN_FLAG_DISABLE_SHADING | SCAN_FLAG_DISABLE_SHADING;
    compute_session(dev, session, calib_sensor);

    gl843_init_scan_regs(dev, calib_sensor, &local_reg, session);

  /* set up for reverse or forward */
    r = sanei_genesys_get_address(&local_reg, REG_0x02);
    if (forward) {
        r->value &= ~REG_0x02_MTRREV;
    } else {
        r->value |= REG_0x02_MTRREV;
    }

    dev->write_registers(local_reg);

    dev->cmd_set->begin_scan(dev, calib_sensor, &local_reg, true);

    wait_until_buffer_non_empty(dev);

    // now we're on target, we can read data
    auto data = read_unshuffled_image_from_scanner(dev, session,
                                                   session.output_total_bytes_raw);

    gl843_stop_action(dev);

  pass = 0;
  if (DBG_LEVEL >= DBG_data)
    {
      char fn[40];
        std::snprintf(fn, 40, "gl843_search_strip_%s_%s%02d.pnm",
                      black ? "black" : "white", forward ? "fwd" : "bwd", pass);
        sanei_genesys_write_pnm_file(fn, data);
    }

  /* loop until strip is found or maximum pass number done */
  found = 0;
  while (pass < 20 && !found)
    {
        dev->write_registers(local_reg);

        // now start scan
        dev->cmd_set->begin_scan(dev, calib_sensor, &local_reg, true);

        wait_until_buffer_non_empty(dev);

        // now we're on target, we can read data
        data = read_unshuffled_image_from_scanner(dev, session, session.output_total_bytes_raw);

        gl843_stop_action(dev);

      if (DBG_LEVEL >= DBG_data)
	{
          char fn[40];
            std::snprintf(fn, 40, "gl843_search_strip_%s_%s%02d.pnm",
                          black ? "black" : "white", forward ? "fwd" : "bwd", pass);
            sanei_genesys_write_pnm_file(fn, data);
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
                    if (black && data.get_raw_channel(x, y, 0) > 90) {
		      count++;
		    }
		  /* when searching for white, detect black pixels */
                    if (!black && data.get_raw_channel(x, y, 0) < 60) {
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
                    // when searching for black, detect white pixels
                    if (black && data.get_raw_channel(x, y, 0) > 90) {
		      count++;
		    }
                    // when searching for white, detect black pixels
                    if (!black && data.get_raw_channel(x, y, 0) < 60) {
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
void CommandSetGl843::send_shading_data(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                        uint8_t* data, int size) const
{
    DBG_HELPER(dbg);
  uint32_t final_size, length, i;
  uint8_t *buffer;
  int count,offset;
  GenesysRegister *r;
    uint16_t strpixel, endpixel, startx;

  offset=0;
  length=size;
    r = sanei_genesys_get_address(&dev->reg, REG_0x01);
    if (r->value & REG_0x01_SHDAREA)
    {
      /* recompute STRPIXEL used shading calibration so we can
       * compute offset within data for SHDAREA case */

        // FIXME: the following is likely incorrect
        // start coordinate in optical dpi coordinates
        startx = (sensor.dummy_pixel / sensor.ccd_pixels_per_system_pixel()) / dev->session.hwdpi_divisor;
        startx *= dev->session.pixel_count_multiplier;

      /* current scan coordinates */
        strpixel = dev->session.pixel_startx;
        endpixel = dev->session.pixel_endx;

        if (dev->model->model_id == ModelId::CANON_8600F) {
            int half_ccd_factor = dev->session.optical_resolution /
                                  sensor.get_logical_hwdpi(dev->session.output_resolution);
            strpixel /= half_ccd_factor * sensor.ccd_pixels_per_system_pixel();
            endpixel /= half_ccd_factor * sensor.ccd_pixels_per_system_pixel();
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

    sanei_genesys_bulk_write_data(dev, 0x3c, final_data.data(), count);
}

bool CommandSetGl843::needs_home_before_init_regs_for_scan(Genesys_Device* dev) const
{
    (void) dev;
    return true;
}

void CommandSetGl843::wait_for_motor_stop(Genesys_Device* dev) const
{
    (void) dev;
}

void CommandSetGl843::rewind(Genesys_Device* dev) const
{
    (void) dev;
    throw SaneException("not implemented");
}

std::unique_ptr<CommandSet> create_gl843_cmd_set()
{
    return std::unique_ptr<CommandSet>(new CommandSetGl843{});
}

} // namespace genesys
