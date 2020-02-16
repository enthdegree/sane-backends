/* sane - Scanner Access Now Easy.

   Copyright (C) 2012-2013 Stéphane Voltz <stef.dev@free.fr>


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

/** @file
 *
 * This file handles GL846 and GL845 ASICs since they are really close to each other.
 */

#define DEBUG_DECLARE_ONLY

#include "gl846.h"
#include "gl846_registers.h"
#include "test_settings.h"

#include <vector>

namespace genesys {
namespace gl846 {

/**
 * compute the step multiplier used
 */
static int
gl846_get_step_multiplier (Genesys_Register_Set * regs)
{
    GenesysRegister *r = sanei_genesys_get_address(regs, 0x9d);
    int value = 1;
    if (r != nullptr) {
      value = (r->value & 0x0f)>>1;
      value = 1 << value;
    }
  DBG (DBG_io, "%s: step multiplier is %d\n", __func__, value);
  return value;
}

/** @brief sensor specific settings
*/
static void gl846_setup_sensor(Genesys_Device * dev, const Genesys_Sensor& sensor,
                               Genesys_Register_Set* regs)
{
    DBG_HELPER(dbg);

    for (const auto& reg : sensor.custom_regs) {
        regs->set8(reg.address, reg.value);
    }

    regs->set16(REG_EXPR, sensor.exposure.red);
    regs->set16(REG_EXPG, sensor.exposure.green);
    regs->set16(REG_EXPB, sensor.exposure.blue);

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
gl846_init_registers (Genesys_Device * dev)
{
    DBG_HELPER(dbg);

    dev->reg.clear();

    dev->reg.init_reg(0x01, 0x60);
    dev->reg.init_reg(0x02, 0x38);
    dev->reg.init_reg(0x03, 0x03);
    dev->reg.init_reg(0x04, 0x22);
    dev->reg.init_reg(0x05, 0x60);
    dev->reg.init_reg(0x06, 0x10);
    dev->reg.init_reg(0x08, 0x60);
    dev->reg.init_reg(0x09, 0x00);
    dev->reg.init_reg(0x0a, 0x00);
    dev->reg.init_reg(0x0b, 0x8b);
    dev->reg.init_reg(0x0c, 0x00);
    dev->reg.init_reg(0x0d, 0x00);
    dev->reg.init_reg(0x10, 0x00);
    dev->reg.init_reg(0x11, 0x00);
    dev->reg.init_reg(0x12, 0x00);
    dev->reg.init_reg(0x13, 0x00);
    dev->reg.init_reg(0x14, 0x00);
    dev->reg.init_reg(0x15, 0x00);
    dev->reg.init_reg(0x16, 0xbb); // SENSOR_DEF
    dev->reg.init_reg(0x17, 0x13); // SENSOR_DEF
    dev->reg.init_reg(0x18, 0x10); // SENSOR_DEF
    dev->reg.init_reg(0x19, 0x2a); // SENSOR_DEF
    dev->reg.init_reg(0x1a, 0x34); // SENSOR_DEF
    dev->reg.init_reg(0x1b, 0x00); // SENSOR_DEF
    dev->reg.init_reg(0x1c, 0x20); // SENSOR_DEF
    dev->reg.init_reg(0x1d, 0x06); // SENSOR_DEF
    dev->reg.init_reg(0x1e, 0xf0);
    dev->reg.init_reg(0x1f, 0x01);
    dev->reg.init_reg(0x20, 0x03);
    dev->reg.init_reg(0x21, 0x10);
    dev->reg.init_reg(0x22, 0x60);
    dev->reg.init_reg(0x23, 0x60);
    dev->reg.init_reg(0x24, 0x60);
    dev->reg.init_reg(0x25, 0x00);
    dev->reg.init_reg(0x26, 0x00);
    dev->reg.init_reg(0x27, 0x00);
    dev->reg.init_reg(0x2c, 0x00);
    dev->reg.init_reg(0x2d, 0x00);
    dev->reg.init_reg(0x2e, 0x80);
    dev->reg.init_reg(0x2f, 0x80);
    dev->reg.init_reg(0x30, 0x00);
    dev->reg.init_reg(0x31, 0x00);
    dev->reg.init_reg(0x32, 0x00);
    dev->reg.init_reg(0x33, 0x00);
    dev->reg.init_reg(0x34, 0x1f);
    dev->reg.init_reg(0x35, 0x00);
    dev->reg.init_reg(0x36, 0x40);
    dev->reg.init_reg(0x37, 0x00);
    dev->reg.init_reg(0x38, 0x2a);
    dev->reg.init_reg(0x39, 0xf8);
    dev->reg.init_reg(0x3d, 0x00);
    dev->reg.init_reg(0x3e, 0x00);
    dev->reg.init_reg(0x3f, 0x01);
    dev->reg.init_reg(0x52, 0x02); // SENSOR_DEF
    dev->reg.init_reg(0x53, 0x04); // SENSOR_DEF
    dev->reg.init_reg(0x54, 0x06); // SENSOR_DEF
    dev->reg.init_reg(0x55, 0x08); // SENSOR_DEF
    dev->reg.init_reg(0x56, 0x0a); // SENSOR_DEF
    dev->reg.init_reg(0x57, 0x00); // SENSOR_DEF
    dev->reg.init_reg(0x58, 0x59); // SENSOR_DEF
    dev->reg.init_reg(0x59, 0x31); // SENSOR_DEF
    dev->reg.init_reg(0x5a, 0x40); // SENSOR_DEF
    dev->reg.init_reg(0x5e, 0x1f);
    dev->reg.init_reg(0x5f, 0x01);
    dev->reg.init_reg(0x60, 0x00);
    dev->reg.init_reg(0x61, 0x00);
    dev->reg.init_reg(0x62, 0x00);
    dev->reg.init_reg(0x63, 0x00);
    dev->reg.init_reg(0x64, 0x00);
    dev->reg.init_reg(0x65, 0x00);
    dev->reg.init_reg(0x67, 0x7f);
    dev->reg.init_reg(0x68, 0x7f);
    dev->reg.init_reg(0x69, 0x01);
    dev->reg.init_reg(0x6a, 0x01);
    dev->reg.init_reg(0x70, 0x01);
    dev->reg.init_reg(0x71, 0x00);
    dev->reg.init_reg(0x72, 0x02);
    dev->reg.init_reg(0x73, 0x01);
    dev->reg.init_reg(0x74, 0x00); // SENSOR_DEF
    dev->reg.init_reg(0x75, 0x00); // SENSOR_DEF
    dev->reg.init_reg(0x76, 0x00); // SENSOR_DEF
    dev->reg.init_reg(0x77, 0x00); // SENSOR_DEF
    dev->reg.init_reg(0x78, 0x00); // SENSOR_DEF
    dev->reg.init_reg(0x79, 0x3f); // SENSOR_DEF
    dev->reg.init_reg(0x7a, 0x00); // SENSOR_DEF
    dev->reg.init_reg(0x7b, 0x09); // SENSOR_DEF
    dev->reg.init_reg(0x7c, 0x99); // SENSOR_DEF
    dev->reg.init_reg(0x7d, 0x20);
    dev->reg.init_reg(0x7f, 0x05);
    dev->reg.init_reg(0x80, 0x4f);
    dev->reg.init_reg(0x87, 0x02);
    dev->reg.init_reg(0x94, 0xff);
    dev->reg.init_reg(0x9d, 0x04);
    dev->reg.init_reg(0x9e, 0x00);
    dev->reg.init_reg(0xa1, 0xe0);
    dev->reg.init_reg(0xa2, 0x1f);
    dev->reg.init_reg(0xab, 0xc0);
    dev->reg.init_reg(0xbb, 0x00);
    dev->reg.init_reg(0xbc, 0x0f);
    dev->reg.init_reg(0xdb, 0xff);
    dev->reg.init_reg(0xfe, 0x08);
    dev->reg.init_reg(0xff, 0x02);
    dev->reg.init_reg(0x98, 0x20);
    dev->reg.init_reg(0x99, 0x00);
    dev->reg.init_reg(0x9a, 0x90);
    dev->reg.init_reg(0x9b, 0x00);
    dev->reg.init_reg(0xf8, 0x05);

    const auto& sensor = sanei_genesys_find_sensor_any(dev);
    sanei_genesys_set_dpihw(dev->reg, sensor, sensor.optical_res);
}

/**@brief send slope table for motor movement
 * Send slope_table in machine byte order
 * @param dev device to send slope table
 * @param table_nr index of the slope table in ASIC memory
 * Must be in the [0-4] range.
 * @param slope_table pointer to 16 bit values array of the slope table
 * @param steps number of elements in the slope table
 */
static void gl846_send_slope_table(Genesys_Device* dev, int table_nr,
                                   const std::vector<uint16_t>& slope_table,
                                   int steps)
{
    DBG_HELPER_ARGS(dbg, "table_nr = %d, steps = %d", table_nr, steps);
  int i;
  char msg[10000];

  /* sanity check */
  if(table_nr<0 || table_nr>4)
    {
        throw SaneException("invalid table number %d", table_nr);
    }

  std::vector<uint8_t> table(steps * 2);
  for (i = 0; i < steps; i++)
    {
      table[i * 2] = slope_table[i] & 0xff;
      table[i * 2 + 1] = slope_table[i] >> 8;
    }

  if (DBG_LEVEL >= DBG_io)
    {
        std::sprintf(msg, "write slope %d (%d)=", table_nr, steps);
      for (i = 0; i < steps; i++)
        {
            std::sprintf(msg+strlen(msg), "%d", slope_table[i]);
        }
      DBG (DBG_io, "%s: %s\n", __func__, msg);
    }

    if (dev->interface->is_mock()) {
        dev->interface->record_slope_table(table_nr, slope_table);
    }
    // slope table addresses are fixed
    dev->interface->write_ahb(0x10000000 + 0x4000 * table_nr, steps * 2, table.data());
}

/**
 * Set register values of Analog Device type frontend
 * */
static void gl846_set_adi_fe(Genesys_Device* dev, uint8_t set)
{
    DBG_HELPER(dbg);
  int i;

    // wait for FE to be ready
    auto status = scanner_read_status(*dev);
    while (status.is_front_end_busy) {
        dev->interface->sleep_ms(10);
        status = scanner_read_status(*dev);
    };

  if (set == AFE_INIT)
    {
        DBG(DBG_proc, "%s(): setting DAC %u\n", __func__,
            static_cast<unsigned>(dev->model->adc_id));
      dev->frontend = dev->frontend_initial;
    }

    // write them to analog frontend
    dev->interface->write_fe_register(0x00, dev->frontend.regs.get_value(0x00));

    dev->interface->write_fe_register(0x01, dev->frontend.regs.get_value(0x01));

    for (i = 0; i < 3; i++) {
        dev->interface->write_fe_register(0x02 + i, dev->frontend.get_gain(i));
    }
    for (i = 0; i < 3; i++) {
        dev->interface->write_fe_register(0x05 + i, dev->frontend.get_offset(i));
    }
}

// Set values of analog frontend
void CommandSetGl846::set_fe(Genesys_Device* dev, const Genesys_Sensor& sensor, uint8_t set) const
{
    DBG_HELPER_ARGS(dbg, "%s", set == AFE_INIT ? "init" :
                               set == AFE_SET ? "set" :
                               set == AFE_POWER_SAVE ? "powersave" : "huh?");
    (void) sensor;

  /* route to specific analog frontend setup */
    uint8_t frontend_type = dev->reg.find_reg(0x04).value & REG_0x04_FESET;
    switch (frontend_type) {
      case 0x02: /* ADI FE */
        gl846_set_adi_fe(dev, set);
        break;
      default:
            throw SaneException("unsupported frontend type %d", frontend_type);
    }
}


// @brief set up motor related register for scan
static void gl846_init_motor_regs_scan(Genesys_Device* dev,
                                       const Genesys_Sensor& sensor,
                                       Genesys_Register_Set* reg,
                                       const MotorProfile& motor_profile,
                                       unsigned int scan_exposure_time,
                                       unsigned scan_yres,
                                       unsigned int scan_lines,
                                       unsigned int scan_dummy,
                                       unsigned int feed_steps,
                                       MotorFlag flags)
{
    DBG_HELPER_ARGS(dbg, "scan_exposure_time=%d, scan_yres=%d, step_type=%d, scan_lines=%d, "
                         "scan_dummy=%d, feed_steps=%d, flags=%x",
                    scan_exposure_time, scan_yres, static_cast<unsigned>(motor_profile.step_type),
                    scan_lines, scan_dummy, feed_steps, static_cast<unsigned>(flags));
  int use_fast_fed;
  unsigned int fast_dpi;
  unsigned int feedl, dist;
  GenesysRegister *r;
  uint32_t z1, z2;
  unsigned int min_restep = 0x20;
  uint8_t val;
  unsigned int ccdlmt,tgtime;

    unsigned step_multiplier = gl846_get_step_multiplier(reg);

  use_fast_fed=0;
  /* no fast fed since feed works well */
    if (dev->settings.yres == 4444 && feed_steps > 100 && !has_flag(flags, MotorFlag::FEED)) {
        use_fast_fed = 1;
    }
  DBG (DBG_io, "%s: use_fast_fed=%d\n", __func__, use_fast_fed);

    reg->set24(REG_LINCNT, scan_lines);
  DBG (DBG_io, "%s: lincnt=%d\n", __func__, scan_lines);

  /* compute register 02 value */
    r = sanei_genesys_get_address(reg, REG_0x02);
  r->value = 0x00;
  sanei_genesys_set_motor_power(*reg, true);

  if (use_fast_fed)
        r->value |= REG_0x02_FASTFED;
  else
        r->value &= ~REG_0x02_FASTFED;

    if (has_flag(flags, MotorFlag::AUTO_GO_HOME)) {
        r->value |= REG_0x02_AGOHOME | REG_0x02_NOTHOME;
    }

    if (has_flag(flags, MotorFlag::DISABLE_BUFFER_FULL_MOVE) ||(scan_yres>=sensor.optical_res)) {
        r->value |= REG_0x02_ACDCDIS;
    }
    if (has_flag(flags, MotorFlag::REVERSE)) {
        r->value |= REG_0x02_MTRREV;
    } else {
        r->value &= ~REG_0x02_MTRREV;
    }

  /* scan and backtracking slope table */
    auto scan_table = sanei_genesys_slope_table(dev->model->asic_type, scan_yres,
                                                scan_exposure_time, dev->motor.base_ydpi,
                                                step_multiplier, motor_profile);

    gl846_send_slope_table(dev, SCAN_TABLE, scan_table.table, scan_table.steps_count);
    gl846_send_slope_table(dev, BACKTRACK_TABLE, scan_table.table, scan_table.steps_count);

  /* fast table */
  fast_dpi=sanei_genesys_get_lowest_ydpi(dev);

    // BUG: looks like for fast moves we use inconsistent step type
    StepType fast_step_type = motor_profile.step_type;
    if (static_cast<unsigned>(motor_profile.step_type) >= static_cast<unsigned>(StepType::QUARTER)) {
        fast_step_type = StepType::QUARTER;
    }

    MotorProfile fast_motor_profile = motor_profile;
    fast_motor_profile.step_type = fast_step_type;

    auto fast_table = sanei_genesys_slope_table(dev->model->asic_type, fast_dpi,
                                                scan_exposure_time, dev->motor.base_ydpi,
                                                step_multiplier, fast_motor_profile);

    gl846_send_slope_table(dev, STOP_TABLE, fast_table.table, fast_table.steps_count);
    gl846_send_slope_table(dev, FAST_TABLE, fast_table.table, fast_table.steps_count);
    gl846_send_slope_table(dev, HOME_TABLE, fast_table.table, fast_table.steps_count);

  /* correct move distance by acceleration and deceleration amounts */
  feedl=feed_steps;
  if (use_fast_fed)
    {
        feedl <<= static_cast<unsigned>(fast_step_type);
        dist = (scan_table.steps_count + 2 * fast_table.steps_count);
        /* TODO read and decode REG_0xAB */
        r = sanei_genesys_get_address (reg, 0x5e);
        dist += (r->value & 31);
        /* FEDCNT */
        r = sanei_genesys_get_address(reg, REG_FEDCNT);
        dist += r->value;
    }
  else
    {
        feedl <<= static_cast<unsigned>(motor_profile.step_type);
        dist = scan_table.steps_count;
        if (has_flag(flags, MotorFlag::FEED)) {
            dist *= 2;
        }
    }
  DBG (DBG_io2, "%s: acceleration distance=%d\n", __func__, dist);

  /* check for overflow */
    if (dist < feedl) {
        feedl -= dist;
    } else {
        feedl = 0;
    }

    reg->set24(REG_FEEDL, feedl);
  DBG (DBG_io ,"%s: feedl=%d\n",__func__,feedl);

    r = sanei_genesys_get_address(reg, REG_0x0C);
    ccdlmt = (r->value & REG_0x0C_CCDLMT) + 1;

    r = sanei_genesys_get_address(reg, REG_0x1C);
    tgtime = 1 << (r->value & REG_0x1C_TGTIME);

  /* hi res motor speed GPIO */
  /*
    uint8_t effective = dev->interface->read_register(REG_0x6C);
  */

  /* if quarter step, bipolar Vref2 */
  /* XXX STEF XXX GPIO
  if (motor_profile.step_type > 1)
    {
      if (motor_profile.step_type < 3)
        {
            val = effective & ~REG_0x6C_GPIO13;
        }
      else
        {
            val = effective | REG_0x6C_GPIO13;
        }
    }
  else
    {
      val = effective;
    }
    dev->interface->write_register(REG_0x6C, val);
    */

  /* effective scan */
  /*
    effective = dev->interface->read_register(REG_0x6C);
    val = effective | REG_0x6C_GPIO10;
    dev->interface->write_register(REG_0x6C, val);
  */

    if(dev->model->gpio_id == GpioId::IMG101) {
        if (scan_yres == sensor.get_register_hwdpi(scan_yres)) {
          val=1;
        }
      else
        {
          val=0;
        }
        dev->interface->write_register(REG_0x7E, val);
    }

    min_restep = (scan_table.steps_count / step_multiplier) / 2 - 1;
    if (min_restep < 1) {
        min_restep = 1;
    }
    r = sanei_genesys_get_address(reg, REG_FWDSTEP);
  r->value = min_restep;
    r = sanei_genesys_get_address(reg, REG_BWDSTEP);
  r->value = min_restep;

    sanei_genesys_calculate_zmod(use_fast_fed,
                                 scan_exposure_time*ccdlmt*tgtime,
                                 scan_table.table,
                                 scan_table.steps_count,
                                 feedl,
                                 min_restep * step_multiplier,
                                 &z1,
                                 &z2);

  DBG(DBG_info, "%s: z1 = %d\n", __func__, z1);
    reg->set24(REG_0x60, z1 | (static_cast<unsigned>(motor_profile.step_type) << (16 + REG_0x60S_STEPSEL)));

  DBG(DBG_info, "%s: z2 = %d\n", __func__, z2);
    reg->set24(REG_0x63, z2 | (static_cast<unsigned>(motor_profile.step_type) << (16 + REG_0x63S_FSTPSEL)));

  r = sanei_genesys_get_address (reg, 0x1e);
  r->value &= 0xf0;		/* 0 dummy lines */
  r->value |= scan_dummy;	/* dummy lines */

    r = sanei_genesys_get_address(reg, REG_0x67);
  r->value = 0x7f;

    r = sanei_genesys_get_address(reg, REG_0x68);
  r->value = 0x7f;

    reg->set8(REG_STEPNO, scan_table.steps_count / step_multiplier);
    reg->set8(REG_FASTNO, scan_table.steps_count / step_multiplier);
    reg->set8(REG_FSHDEC, scan_table.steps_count / step_multiplier);
    reg->set8(REG_FMOVNO, fast_table.steps_count / step_multiplier);
    reg->set8(REG_FMOVDEC, fast_table.steps_count / step_multiplier);
}


/** @brief set up registers related to sensor
 * Set up the following registers
   0x01
   0x03
   0x10-0x015     R/G/B exposures
   0x19           EXPDMY
   0x2e           BWHI
   0x2f           BWLO
   0x04
   0x87
   0x05
   0x2c,0x2d      DPISET
   0x30,0x31      STRPIXEL
   0x32,0x33      ENDPIXEL
   0x35,0x36,0x37 MAXWD [25:2] (>>2)
   0x38,0x39      LPERIOD
   0x34           DUMMY
 */
static void gl846_init_optical_regs_scan(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                         Genesys_Register_Set* reg, unsigned int exposure_time,
                                         const ScanSession& session)
{
    DBG_HELPER_ARGS(dbg, "exposure_time=%d", exposure_time);
    unsigned int dpihw;
  GenesysRegister *r;

    // resolution is divided according to ccd_pixels_per_system_pixel()
    unsigned ccd_pixels_per_system_pixel = sensor.ccd_pixels_per_system_pixel();
    DBG(DBG_io2, "%s: ccd_pixels_per_system_pixel=%d\n", __func__, ccd_pixels_per_system_pixel);

    // to manage high resolution device while keeping good low resolution scanning speed,
    // we make hardware dpi vary
    dpihw = sensor.get_register_hwdpi(session.params.xres * ccd_pixels_per_system_pixel);
    DBG(DBG_io2, "%s: dpihw=%d\n", __func__, dpihw);

    gl846_setup_sensor(dev, sensor, reg);

    dev->cmd_set->set_fe(dev, sensor, AFE_SET);

  /* enable shading */
    regs_set_optical_off(dev->model->asic_type, *reg);
    r = sanei_genesys_get_address(reg, REG_0x01);
    r->value |= REG_0x01_SHDAREA;
    if (has_flag(session.params.flags, ScanFlag::DISABLE_SHADING) ||
        has_flag(dev->model->flags, ModelFlag::NO_CALIBRATION))
    {
        r->value &= ~REG_0x01_DVDSET;
    }
  else
    {
        r->value |= REG_0x01_DVDSET;
    }

    r = sanei_genesys_get_address(reg, REG_0x03);
    r->value &= ~REG_0x03_AVEENB;

    sanei_genesys_set_lamp_power(dev, sensor, *reg,
                                 !has_flag(session.params.flags, ScanFlag::DISABLE_LAMP));

  /* BW threshold */
  r = sanei_genesys_get_address (reg, 0x2e);
  r->value = dev->settings.threshold;
  r = sanei_genesys_get_address (reg, 0x2f);
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
                r->value |= 0x24;
                break;
            case ColorFilter::BLUE:
                r->value |= 0x2c;
                break;
            case ColorFilter::GREEN:
                r->value |= 0x28;
                break;
            default:
                break; // should not happen
        }
    } else {
        r->value |= 0x20; // mono
    }

    sanei_genesys_set_dpihw(*reg, sensor, dpihw);

    if (should_enable_gamma(session, sensor)) {
        reg->find_reg(REG_0x05).value |= REG_0x05_GMMENB;
    } else {
        reg->find_reg(REG_0x05).value &= ~REG_0x05_GMMENB;
    }

  /* CIS scanners can do true gray by setting LEDADD */
  /* we set up LEDADD only when asked */
    if (dev->model->is_cis) {
      r = sanei_genesys_get_address (reg, 0x87);
        r->value &= ~REG_0x87_LEDADD;
        if (session.enable_ledadd) {
            r->value |= REG_0x87_LEDADD;
        }
      /* RGB weighting
      r = sanei_genesys_get_address (reg, 0x01);
        r->value &= ~REG_0x01_TRUEGRAY;
      if (session.enable_ledadd))
        {
            r->value |= REG_0x01_TRUEGRAY;
        }*/
    }

    unsigned dpiset = session.params.xres * ccd_pixels_per_system_pixel;
    reg->set16(REG_DPISET, dpiset);
    DBG(DBG_io2, "%s: dpiset used=%d\n", __func__, dpiset);

    reg->set16(REG_STRPIXEL, session.pixel_startx);
    reg->set16(REG_ENDPIXEL, session.pixel_endx);

    build_image_pipeline(dev, sensor, session);

  /* MAXWD is expressed in 4 words unit */
    // BUG: we shouldn't multiply by channels here
    reg->set24(REG_MAXWD, (session.output_line_bytes_raw * session.params.channels >> 2));

    reg->set16(REG_LPERIOD, exposure_time);
  DBG (DBG_io2, "%s: exposure_time used=%d\n", __func__, exposure_time);

  r = sanei_genesys_get_address (reg, 0x34);
  r->value = sensor.dummy_pixel;
}

void CommandSetGl846::init_regs_for_scan_session(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                                 Genesys_Register_Set* reg,
                                                 const ScanSession& session) const
{
    DBG_HELPER(dbg);
    session.assert_computed();

  int move;
  int exposure_time;

  int slope_dpi = 0;
  int dummy = 0;

  dummy = 3-session.params.channels;

/* slope_dpi */
/* cis color scan is effectively a gray scan with 3 gray lines per color
   line and a FILTER of 0 */
    if (dev->model->is_cis) {
        slope_dpi = session.params.yres * session.params.channels;
    } else {
        slope_dpi = session.params.yres;
    }

  slope_dpi = slope_dpi * (1 + dummy);

    exposure_time = sensor.exposure_lperiod;
    const auto& motor_profile = get_motor_profile(dev->motor.profiles, exposure_time, session);

  DBG(DBG_info, "%s : exposure_time=%d pixels\n", __func__, exposure_time);
    DBG(DBG_info, "%s : scan_step_type=%d\n", __func__,
        static_cast<unsigned>(motor_profile.step_type));

  /* we enable true gray for cis scanners only, and just when doing
   * scan since color calibration is OK for this mode
   */
    gl846_init_optical_regs_scan(dev, sensor, reg, exposure_time, session);

/*** motor parameters ***/

  /* add tl_y to base movement */
  move = session.params.starty;
  DBG(DBG_info, "%s: move=%d steps\n", __func__, move);

    MotorFlag mflags = MotorFlag::NONE;
    if (has_flag(session.params.flags, ScanFlag::DISABLE_BUFFER_FULL_MOVE)) {
        mflags |= MotorFlag::DISABLE_BUFFER_FULL_MOVE;
    }
    if (has_flag(session.params.flags, ScanFlag::FEEDING)) {
        mflags |= MotorFlag::FEED;
    }
    if (has_flag(session.params.flags, ScanFlag::REVERSE)) {
        mflags |= MotorFlag::REVERSE;
    }

    gl846_init_motor_regs_scan(dev, sensor, reg, motor_profile, exposure_time, slope_dpi,
                               session.optical_line_count, dummy, move, mflags);

  /*** prepares data reordering ***/

    dev->read_buffer.clear();
    dev->read_buffer.alloc(session.buffer_size_read);

    dev->read_active = true;

    dev->session = session;

    dev->total_bytes_read = 0;
    dev->total_bytes_to_read = session.output_line_bytes_requested * session.params.lines;

    DBG(DBG_info, "%s: total bytes to send = %zu\n", __func__, dev->total_bytes_to_read);
}

ScanSession CommandSetGl846::calculate_scan_session(const Genesys_Device* dev,
                                                    const Genesys_Sensor& sensor,
                                                    const Genesys_Settings& settings) const
{
  int start;

    DBG(DBG_info, "%s ", __func__);
    debug_dump(DBG_info, settings);

  /* start */
    start = static_cast<int>(dev->model->x_offset);
    start += static_cast<int>(settings.tl_x);
    start = static_cast<int>((start * settings.xres) / MM_PER_INCH);

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
    session.params.flags = ScanFlag::NONE;

    compute_session(dev, session, sensor);

    return session;
}

// for fast power saving methods only, like disabling certain amplifiers
void CommandSetGl846::save_power(Genesys_Device* dev, bool enable) const
{
    (void) dev;
    DBG_HELPER_ARGS(dbg, "enable = %d", enable);
}

void CommandSetGl846::set_powersaving(Genesys_Device* dev, int delay /* in minutes */) const
{
    (void) dev;
    DBG_HELPER_ARGS(dbg, "delay = %d", delay);
}

// Send the low-level scan command
void CommandSetGl846::begin_scan(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                 Genesys_Register_Set* reg, bool start_motor) const
{
    DBG_HELPER(dbg);
    (void) sensor;
  uint8_t val;
  GenesysRegister *r;

  /* XXX STEF XXX SCAN GPIO */
  /*
    val = dev->interface->read_register(REG_0x6C);
    dev->interface->write_register(REG_0x6C, val);
  */

    val = REG_0x0D_CLRLNCNT;
    dev->interface->write_register(REG_0x0D, val);
    val = REG_0x0D_CLRMCNT;
    dev->interface->write_register(REG_0x0D, val);

    val = dev->interface->read_register(REG_0x01);
    val |= REG_0x01_SCAN;
    dev->interface->write_register(REG_0x01, val);
    r = sanei_genesys_get_address (reg, REG_0x01);
  r->value = val;

    scanner_start_action(*dev, start_motor);

    dev->advance_head_pos_by_session(ScanHeadId::PRIMARY);
}


// Send the stop scan command
void CommandSetGl846::end_scan(Genesys_Device* dev, Genesys_Register_Set* reg,
                               bool check_stop) const
{
    (void) reg;
    DBG_HELPER_ARGS(dbg, "check_stop = %d", check_stop);

    if (!dev->model->is_sheetfed) {
        scanner_stop_action(*dev);
    }
}

// Moves the slider to the home (top) postion slowly
void CommandSetGl846::move_back_home(Genesys_Device* dev, bool wait_until_home) const
{
    scanner_move_back_home(*dev, wait_until_home);
}

// Automatically set top-left edge of the scan area by scanning a 200x200 pixels area at 600 dpi
// from very top of scanner
void CommandSetGl846::search_start_position(Genesys_Device* dev) const
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
    session.params.yres =  dpi;
    session.params.startx = 0;
    session.params.starty =  0;	/*we should give a small offset here~60 steps */
    session.params.pixels = 600;
    session.params.lines = dev->model->search_lines;
    session.params.depth = 8;
    session.params.channels = 1;
    session.params.scan_method = dev->settings.scan_method;
    session.params.scan_mode = ScanColorMode::GRAY;
    session.params.color_filter = ColorFilter::GREEN;
    session.params.flags = ScanFlag::DISABLE_SHADING |
                           ScanFlag::DISABLE_GAMMA |
                           ScanFlag::IGNORE_LINE_DISTANCE;
    compute_session(dev, session, sensor);

    init_regs_for_scan_session(dev, sensor, &local_reg, session);

    // send to scanner
    dev->interface->write_registers(local_reg);

    begin_scan(dev, sensor, &local_reg, true);

    if (is_testing_mode()) {
        dev->interface->test_checkpoint("search_start_position");
        end_scan(dev, &local_reg, true);
        dev->reg = local_reg;
        return;
    }

    wait_until_buffer_non_empty(dev);

    // now we're on target, we can read data
    auto image = read_unshuffled_image_from_scanner(dev, session, session.output_total_bytes);

    if (DBG_LEVEL >= DBG_data) {
        sanei_genesys_write_pnm_file("gl846_search_position.pnm", image);
    }

    end_scan(dev, &local_reg, true);

  /* update regs to copy ASIC internal state */
  dev->reg = local_reg;

    // TODO: find out where sanei_genesys_search_reference_point stores information,
    // and use that correctly
    for (auto& sensor_update :
            sanei_genesys_find_sensors_all_for_write(dev, dev->model->default_method))
    {
        sanei_genesys_search_reference_point(dev, sensor_update, image.get_row_ptr(0), 0, dpi,
                                             pixels, dev->model->search_lines);
    }
}

// sets up register for coarse gain calibration
// todo: check it for scanners using it
void CommandSetGl846::init_regs_for_coarse_calibration(Genesys_Device* dev,
                                                       const Genesys_Sensor& sensor,
                                                       Genesys_Register_Set& regs) const
{
    DBG_HELPER(dbg);

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
    session.params.flags = ScanFlag::DISABLE_SHADING |
                           ScanFlag::DISABLE_GAMMA |
                           ScanFlag::SINGLE_LINE |
                           ScanFlag::IGNORE_LINE_DISTANCE;
    compute_session(dev, session, sensor);

    init_regs_for_scan_session(dev, sensor, &regs, session);

    DBG(DBG_info, "%s: optical sensor res: %d dpi, actual res: %d\n", __func__,
        sensor.optical_res / sensor.ccd_pixels_per_system_pixel(), dev->settings.xres);
}

// init registers for shading calibration
void CommandSetGl846::init_regs_for_shading(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                            Genesys_Register_Set& regs) const
{
    DBG_HELPER(dbg);
  float move;

    unsigned channels = 3;
    unsigned resolution = sensor.get_register_hwdpi(dev->settings.xres);

    const auto& calib_sensor = sanei_genesys_find_sensor(dev, resolution, channels,
                                                         dev->settings.scan_method);
    unsigned calib_lines = dev->model->shading_lines;
    if (resolution == 4800) {
        calib_lines *= 2;
    }
    unsigned calib_pixels = (calib_sensor.sensor_pixels * resolution) /
                             calib_sensor.optical_res;

  /* this is aworkaround insufficent distance for slope
   * motor acceleration TODO special motor slope for shading  */
  move=1;
    if (resolution < 1200) {
      move=40;
    }

    ScanSession session;
    session.params.xres = resolution;
    session.params.yres = resolution;
    session.params.startx = 0;
    session.params.starty = static_cast<unsigned>(move);
    session.params.pixels = calib_pixels;
    session.params.lines = calib_lines;
    session.params.depth = 16;
    session.params.channels = channels;
    session.params.scan_method = dev->settings.scan_method;
    session.params.scan_mode = ScanColorMode::COLOR_SINGLE_PASS;
    session.params.color_filter = dev->settings.color_filter;
    session.params.flags = ScanFlag::DISABLE_SHADING |
                   ScanFlag::DISABLE_GAMMA |
                   ScanFlag::DISABLE_BUFFER_FULL_MOVE |
                   ScanFlag::IGNORE_LINE_DISTANCE;
    compute_session(dev, session, calib_sensor);

    init_regs_for_scan_session(dev, calib_sensor, &regs, session);

  /* we use ModelFlag::SHADING_REPARK */
    dev->set_head_pos_zero(ScanHeadId::PRIMARY);

    dev->calib_session = session;
}

/** @brief set up registers for the actual scan
 */
void CommandSetGl846::init_regs_for_scan(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                         Genesys_Register_Set& regs) const
{
    DBG_HELPER(dbg);
  float move;
  int move_dpi;
  float start;

    debug_dump(DBG_info, dev->settings);

  /* steps to move to reach scanning area:
     - first we move to physical start of scanning
     either by a fixed steps amount from the black strip
     or by a fixed amount from parking position,
     minus the steps done during shading calibration
     - then we move by the needed offset whitin physical
     scanning area

     assumption: steps are expressed at maximum motor resolution

     we need:
     float y_offset;
     float y_size;
     float y_offset_calib;
     mm_to_steps()=motor dpi / 2.54 / 10=motor dpi / MM_PER_INCH */

  /* if scanner uses ModelFlag::SEARCH_START y_offset is
     relative from origin, else, it is from parking position */

  move_dpi = dev->motor.base_ydpi;

    move = dev->model->y_offset;
    move = move + dev->settings.tl_y;
    move = static_cast<float>((move * move_dpi) / MM_PER_INCH);
    move -= dev->head_pos(ScanHeadId::PRIMARY);
  DBG(DBG_info, "%s: move=%f steps\n", __func__, move);

  /* fast move to scan area */
  /* we don't move fast the whole distance since it would involve
   * computing acceleration/deceleration distance for scan
   * resolution. So leave a remainder for it so scan makes the final
   * move tuning */
    if (dev->settings.get_channels() * dev->settings.yres >= 600 && move > 700) {
        scanner_move(*dev, dev->model->default_method, static_cast<unsigned>(move - 500),
                     Direction::FORWARD);
      move=500;
    }

  DBG(DBG_info, "%s: move=%f steps\n", __func__, move);
  DBG(DBG_info, "%s: move=%f steps\n", __func__, move);

  /* start */
    start = dev->model->x_offset;
    start = start + dev->settings.tl_x;
    start = static_cast<float>((start * dev->settings.xres) / MM_PER_INCH);

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
    // backtracking isn't handled well, so don't enable it
    session.params.flags = ScanFlag::DISABLE_BUFFER_FULL_MOVE;
    compute_session(dev, session, sensor);

    init_regs_for_scan_session(dev, sensor, &regs, session);
}


/**
 * Send shading calibration data. The buffer is considered to always hold values
 * for all the channels.
 */
void CommandSetGl846::send_shading_data(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                        uint8_t* data, int size) const
{
    DBG_HELPER_ARGS(dbg, "writing %d bytes of shading data", size);
  uint32_t addr, length, i, x, factor, pixels;
    uint32_t dpiset, dpihw;
  uint8_t val,*ptr,*src;

  /* shading data is plit in 3 (up to 5 with IR) areas
     write(0x10014000,0x00000dd8)
     URB 23429  bulk_out len  3544  wrote 0x33 0x10 0x....
     write(0x1003e000,0x00000dd8)
     write(0x10068000,0x00000dd8)
   */
    length = static_cast<uint32_t>(size / 3);
    unsigned strpixel = dev->session.pixel_startx;
    unsigned endpixel = dev->session.pixel_endx;

  /* compute deletion factor */
    dpiset = dev->reg.get16(REG_DPISET);
    dpihw = sensor.get_register_hwdpi(dpiset);
  factor=dpihw/dpiset;
  DBG(DBG_io2, "%s: factor=%d\n", __func__, factor);

  pixels=endpixel-strpixel;

  /* since we're using SHDAREA, substract startx coordinate from shading */
    strpixel -= (sensor.ccd_start_xoffset * 600) / sensor.optical_res;

  /* turn pixel value into bytes 2x16 bits words */
  strpixel*=2*2;
  pixels*=2*2;

    dev->interface->record_key_value("shading_offset", std::to_string(strpixel));
    dev->interface->record_key_value("shading_pixels", std::to_string(pixels));
    dev->interface->record_key_value("shading_length", std::to_string(length));
    dev->interface->record_key_value("shading_factor", std::to_string(factor));

  std::vector<uint8_t> buffer(pixels, 0);

  DBG(DBG_io2, "%s: using chunks of %d (0x%04x) bytes\n", __func__, pixels, pixels);

  /* base addr of data has been written in reg D0-D4 in 4K word, so AHB address
   * is 8192*reg value */

  /* write actual color channel data */
  for(i=0;i<3;i++)
    {
      /* build up actual shading data by copying the part from the full width one
       * to the one corresponding to SHDAREA */
      ptr = buffer.data();

      /* iterate on both sensor segment */
      for(x=0;x<pixels;x+=4*factor)
        {
          /* coefficient source */
          src=(data+strpixel+i*length)+x;

          /* coefficient copy */
          ptr[0]=src[0];
          ptr[1]=src[1];
          ptr[2]=src[2];
          ptr[3]=src[3];

          /* next shading coefficient */
          ptr+=4;
        }

        val = dev->interface->read_register(0xd0+i);
        addr = val * 8192 + 0x10000000;
        dev->interface->write_ahb(addr, pixels, buffer.data());
    }
}

/** @brief calibrates led exposure
 * Calibrate exposure by scanning a white area until the used exposure gives
 * data white enough.
 * @param dev device to calibrate
 */
SensorExposure CommandSetGl846::led_calibration(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                                Genesys_Register_Set& regs) const
{
    DBG_HELPER(dbg);
  int num_pixels;
  int used_res;
    int i;
  int avg[3], top[3], bottom[3];
  int turn;
  uint16_t exp[3];

    float move = dev->model->y_offset_calib_white;
     move = static_cast<float>((move * (dev->motor.base_ydpi / 4)) / MM_PER_INCH);
  if(move>20)
    {
        scanner_move(*dev, dev->model->default_method, static_cast<unsigned>(move),
                     Direction::FORWARD);
    }
  DBG(DBG_io, "%s: move=%f steps\n", __func__, move);

  /* offset calibration is always done in color mode */
    unsigned channels = 3;
    used_res = sensor.get_register_hwdpi(dev->settings.xres);
    const auto& calib_sensor = sanei_genesys_find_sensor(dev, used_res, channels,
                                                         dev->settings.scan_method);
    num_pixels = (calib_sensor.sensor_pixels * used_res) / calib_sensor.optical_res;

  /* initial calibration reg values */
  regs = dev->reg;

    ScanSession session;
    session.params.xres = used_res;
    session.params.yres = used_res;
    session.params.startx = 0;
    session.params.starty = 0;
    session.params.pixels = num_pixels;
    session.params.lines = 1;
    session.params.depth = 16;
    session.params.channels = channels;
    session.params.scan_method = dev->settings.scan_method;
    session.params.scan_mode = ScanColorMode::COLOR_SINGLE_PASS;
    session.params.color_filter = dev->settings.color_filter;
    session.params.flags = ScanFlag::DISABLE_SHADING |
                           ScanFlag::DISABLE_GAMMA |
                           ScanFlag::SINGLE_LINE |
                           ScanFlag::IGNORE_LINE_DISTANCE;
    compute_session(dev, session, calib_sensor);

    init_regs_for_scan_session(dev, calib_sensor, &regs, session);

  /* initial loop values and boundaries */
    exp[0] = calib_sensor.exposure.red;
    exp[1] = calib_sensor.exposure.green;
    exp[2] = calib_sensor.exposure.blue;

  bottom[0]=29000;
  bottom[1]=29000;
  bottom[2]=29000;

  top[0]=41000;
  top[1]=51000;
  top[2]=51000;

  turn = 0;

  /* no move during led calibration */
  sanei_genesys_set_motor_power(regs, false);
    bool acceptable = false;
  do
    {
        // set up exposure
        regs.set16(REG_EXPR, exp[0]);
        regs.set16(REG_EXPG, exp[1]);
        regs.set16(REG_EXPB, exp[2]);

        // write registers and scan data
        dev->interface->write_registers(regs);

      DBG(DBG_info, "%s: starting line reading\n", __func__);
        begin_scan(dev, calib_sensor, &regs, true);

        if (is_testing_mode()) {
            dev->interface->test_checkpoint("led_calibration");
            scanner_stop_action(*dev);
            move_back_home(dev, true);
            return calib_sensor.exposure;
        }

        auto image = read_unshuffled_image_from_scanner(dev, session, session.output_line_bytes);

        // stop scanning
        scanner_stop_action(*dev);

        if (DBG_LEVEL >= DBG_data) {
            char fn[30];
            std::snprintf(fn, 30, "gl846_led_%02d.pnm", turn);
            sanei_genesys_write_pnm_file(fn, image);
        }

        // compute average
        for (unsigned ch = 0; ch < channels; ch++) {
            avg[ch] = 0;
            for (std::size_t x = 0; x < image.get_width(); x++) {
                avg[ch] += image.get_raw_channel(x, 0, ch);
            }
            avg[ch] /= image.get_width();
        }

      DBG(DBG_info, "%s: average: %d,%d,%d\n", __func__, avg[0], avg[1], avg[2]);

      /* check if exposure gives average within the boundaries */
        acceptable = true;
      for(i=0;i<3;i++)
        {
          if(avg[i]<bottom[i])
            {
              exp[i]=(exp[i]*bottom[i])/avg[i];
                acceptable = false;
            }
          if(avg[i]>top[i])
            {
              exp[i]=(exp[i]*top[i])/avg[i];
                acceptable = false;
            }
        }

      turn++;
    }
  while (!acceptable && turn < 100);

  DBG(DBG_info, "%s: acceptable exposure: %d,%d,%d\n", __func__, exp[0], exp[1], exp[2]);

    // set these values as final ones for scan
    dev->reg.set16(REG_EXPR, exp[0]);
    dev->reg.set16(REG_EXPG, exp[1]);
    dev->reg.set16(REG_EXPB, exp[2]);

  /* go back home */
  if(move>20)
    {
        move_back_home(dev, true);
    }

    return { exp[0], exp[1], exp[2] };
}

/**
 * set up GPIO/GPOE for idle state
 */
static void gl846_init_gpio(Genesys_Device* dev)
{
    DBG_HELPER(dbg);
  int idx=0;

  /* search GPIO profile */
    while (gpios[idx].gpio_id != GpioId::UNKNOWN && dev->model->gpio_id != gpios[idx].gpio_id) {
      idx++;
    }
    if (gpios[idx].gpio_id == GpioId::UNKNOWN)
    {
        throw SaneException("failed to find GPIO profile for sensor_id=%d",
                            static_cast<unsigned>(dev->model->sensor_id));
    }

    dev->interface->write_register(REG_0xA7, gpios[idx].ra7);
    dev->interface->write_register(REG_0xA6, gpios[idx].ra6);

    dev->interface->write_register(REG_0x6B, gpios[idx].r6b);
    dev->interface->write_register(REG_0x6C, gpios[idx].r6c);
    dev->interface->write_register(REG_0x6D, gpios[idx].r6d);
    dev->interface->write_register(REG_0x6E, gpios[idx].r6e);
    dev->interface->write_register(REG_0x6F, gpios[idx].r6f);

    dev->interface->write_register(REG_0xA8, gpios[idx].ra8);
    dev->interface->write_register(REG_0xA9, gpios[idx].ra9);
}

/**
 * set memory layout by filling values in dedicated registers
 */
static void gl846_init_memory_layout(Genesys_Device* dev)
{
    DBG_HELPER(dbg);
  int idx = 0, i;
  uint8_t val;

  /* point to per model memory layout */
  idx = 0;
    while (layouts[idx].model != nullptr && strcmp(dev->model->name,layouts[idx].model)!=0) {
      if(strcmp(dev->model->name,layouts[idx].model)!=0)
        idx++;
    }
    if (layouts[idx].model == nullptr) {
        throw SaneException("failed to find memory layout for model %s", dev->model->name);
    }

  /* CLKSET and DRAMSEL */
  val = layouts[idx].dramsel;
    dev->interface->write_register(REG_0x0B, val);
  dev->reg.find_reg(0x0b).value = val;

  /* prevent further writings by bulk write register */
  dev->reg.remove_reg(0x0b);

  /* setup base address for shading and scanned data. */
  for(i=0;i<10;i++)
    {
      dev->interface->write_register(0xe0+i, layouts[idx].rx[i]);
    }
}

/* *
 * initialize ASIC from power on condition
 */
void CommandSetGl846::asic_boot(Genesys_Device* dev, bool cold) const
{
    DBG_HELPER(dbg);
  uint8_t val;

    // reset ASIC if cold boot
    if (cold) {
        dev->interface->write_register(0x0e, 0x01);
        dev->interface->write_register(0x0e, 0x00);
    }

  if(dev->usb_mode == 1)
    {
      val = 0x14;
    }
  else
    {
      val = 0x11;
    }
    dev->interface->write_0x8c(0x0f, val);

    // test CHKVER
    val = dev->interface->read_register(REG_0x40);
    if (val & REG_0x40_CHKVER) {
        val = dev->interface->read_register(0x00);
        DBG(DBG_info, "%s: reported version for genesys chip is 0x%02x\n", __func__, val);
    }

  /* Set default values for registers */
  gl846_init_registers (dev);

    // Write initial registers
    dev->interface->write_registers(dev->reg);

  /* Enable DRAM by setting a rising edge on bit 3 of reg 0x0b */
    val = dev->reg.find_reg(0x0b).value & REG_0x0B_DRAMSEL;
    val = (val | REG_0x0B_ENBDRAM);
    dev->interface->write_register(REG_0x0B, val);
  dev->reg.find_reg(0x0b).value = val;

  /* CIS_LINE */
  if (dev->model->is_cis)
    {
        dev->reg.init_reg(0x08, REG_0x08_CIS_LINE);
        dev->interface->write_register(0x08, dev->reg.find_reg(0x08).value);
    }

    // set up clocks
    dev->interface->write_0x8c(0x10, 0x0e);
    dev->interface->write_0x8c(0x13, 0x0e);

    // setup gpio
    gl846_init_gpio(dev);

    // setup internal memory layout
    gl846_init_memory_layout(dev);

  dev->reg.init_reg(0xf8, 0x05);
    dev->interface->write_register(0xf8, dev->reg.find_reg(0xf8).value);
}

/**
 * initialize backend and ASIC : registers, motor tables, and gamma tables
 * then ensure scanner's head is at home
 */
void CommandSetGl846::init(Genesys_Device* dev) const
{
  DBG_INIT ();
    DBG_HELPER(dbg);

    sanei_genesys_asic_init(dev, 0);
}

void CommandSetGl846::update_hardware_sensors(Genesys_Scanner* s) const
{
    DBG_HELPER(dbg);
  /* do what is needed to get a new set of events, but try to not lose
     any of them.
   */
  uint8_t val;
  uint8_t scan, file, email, copy;
  switch(s->dev->model->gpio_id)
    {
      default:
        scan=0x01;
        file=0x02;
        email=0x04;
        copy=0x08;
    }
    val = s->dev->interface->read_register(REG_0x6D);

    s->buttons[BUTTON_SCAN_SW].write((val & scan) == 0);
    s->buttons[BUTTON_FILE_SW].write((val & file) == 0);
    s->buttons[BUTTON_EMAIL_SW].write((val & email) == 0);
    s->buttons[BUTTON_COPY_SW].write((val & copy) == 0);
}


void CommandSetGl846::update_home_sensor_gpio(Genesys_Device& dev) const
{
    DBG_HELPER(dbg);

    std::uint8_t val = dev.interface->read_register(REG_0x6C);
    val |= 0x41;
    dev.interface->write_register(REG_0x6C, val);
}

/** @brief search for a full width black or white strip.
 * This function searches for a black or white stripe across the scanning area.
 * When searching backward, the searched area must completely be of the desired
 * color since this area will be used for calibration which scans forward.
 * @param dev scanner device
 * @param forward true if searching forward, false if searching backward
 * @param black true if searching for a black strip, false for a white strip
 */
void CommandSetGl846::search_strip(Genesys_Device* dev, const Genesys_Sensor& sensor, bool forward,
                                   bool black) const
{
    DBG_HELPER_ARGS(dbg, "%s %s", black ? "black" : "white", forward ? "forward" : "reverse");
  unsigned int pixels, lines, channels;
  Genesys_Register_Set local_reg;
    unsigned int pass, count, found;
  char title[80];

    set_fe(dev, sensor, AFE_SET);

    scanner_stop_action(*dev);

    // set up for a gray scan at lowest dpi
    const auto& resolution_settings = dev->model->get_resolution_settings(dev->settings.scan_method);
    unsigned dpi = resolution_settings.get_min_resolution_x();
  channels = 1;
  /* 10 MM */
  /* lines = (10 * dpi) / MM_PER_INCH; */
  /* shading calibation is done with dev->motor.base_ydpi */
  lines = (dev->model->shading_lines * dpi) / dev->motor.base_ydpi;
  pixels = (sensor.sensor_pixels * dpi) / sensor.optical_res;

    dev->set_head_pos_zero(ScanHeadId::PRIMARY);

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
    session.params.scan_mode = ScanColorMode::GRAY;
    session.params.color_filter = ColorFilter::RED;
    session.params.flags = ScanFlag::DISABLE_SHADING |
                           ScanFlag::DISABLE_GAMMA;
    if (!forward) {
        session.params.flags |= ScanFlag::REVERSE;
    }
    compute_session(dev, session, sensor);

    init_regs_for_scan_session(dev, sensor, &local_reg, session);

    dev->interface->write_registers(local_reg);

    begin_scan(dev, sensor, &local_reg, true);

    if (is_testing_mode()) {
        dev->interface->test_checkpoint("search_strip");
        scanner_stop_action(*dev);
        return;
    }

    wait_until_buffer_non_empty(dev);

    // now we're on target, we can read data
    auto image = read_unshuffled_image_from_scanner(dev, session, session.output_total_bytes);

    scanner_stop_action(*dev);

  pass = 0;
  if (DBG_LEVEL >= DBG_data)
    {
        std::sprintf(title, "gl846_search_strip_%s_%s%02d.pnm",
                     black ? "black" : "white", forward ? "fwd" : "bwd", pass);
        sanei_genesys_write_pnm_file(title, image);
    }

  /* loop until strip is found or maximum pass number done */
  found = 0;
  while (pass < 20 && !found)
    {
        dev->interface->write_registers(local_reg);

        // now start scan
        begin_scan(dev, sensor, &local_reg, true);

        wait_until_buffer_non_empty(dev);

        // now we're on target, we can read data
        image = read_unshuffled_image_from_scanner(dev, session, session.output_total_bytes);

        scanner_stop_action(*dev);

      if (DBG_LEVEL >= DBG_data)
        {
            std::sprintf(title, "gl846_search_strip_%s_%s%02d.pnm",
                         black ? "black" : "white", forward ? "fwd" : "bwd", pass);
            sanei_genesys_write_pnm_file(title, image);
        }

        unsigned white_level = 90;
        unsigned black_level = 60;

      /* search data to find black strip */
      /* when searching forward, we only need one line of the searched color since we
       * will scan forward. But when doing backward search, we need all the area of the
       * same color */
        if (forward) {

            for (std::size_t y = 0; y < image.get_height() && !found; y++) {
                count = 0;

                // count of white/black pixels depending on the color searched
                for (std::size_t x = 0; x < image.get_width(); x++) {

                    // when searching for black, detect white pixels
                    if (black && image.get_raw_channel(x, y, 0) > white_level) {
                        count++;
                    }

                    // when searching for white, detect black pixels
                    if (!black && image.get_raw_channel(x, y, 0) < black_level) {
                        count++;
                    }
                }

              /* at end of line, if count >= 3%, line is not fully of the desired color
               * so we must go to next line of the buffer */
                auto found_percentage = (count * 100 / image.get_width());
                if (found_percentage < 3) {
                    found = 1;
                    DBG(DBG_data, "%s: strip found forward during pass %d at line %zu\n", __func__,
                        pass, y);
                }
              else
                {
                    DBG(DBG_data, "%s: pixels=%zu, count=%d (%zu%%)\n", __func__, image.get_width(),
                        count, found_percentage);
                }
            }
        } else {
            /* since calibration scans are done forward, we need the whole area
               to be of the required color when searching backward
            */

            count = 0;
            for (std::size_t y = 0; y < image.get_height(); y++) {
                // count of white/black pixels depending on the color searched
                for (std::size_t x = 0; x < image.get_width(); x++) {
                    // when searching for black, detect white pixels
                    if (black && image.get_raw_channel(x, y, 0) > white_level) {
                        count++;
                    }
                    // when searching for white, detect black pixels
                    if (!black && image.get_raw_channel(x, y, 0) < black_level) {
                        count++;
                    }
                }
            }

          /* at end of area, if count >= 3%, area is not fully of the desired color
           * so we must go to next buffer */
            auto found_percentage = count * 100 / (image.get_width() * image.get_height());
            if (found_percentage < 3) {
                found = 1;
                 DBG(DBG_data, "%s: strip found backward during pass %d \n", __func__, pass);
            } else {
                DBG(DBG_data, "%s: pixels=%zu, count=%d (%zu%%)\n", __func__, image.get_width(),
                    count, found_percentage);
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
 * average dark pixels of a 8 bits scan
 */
static int
dark_average (uint8_t * data, unsigned int pixels, unsigned int lines,
              unsigned int channels, unsigned int black)
{
  unsigned int i, j, k, average, count;
  unsigned int avg[3];
  uint8_t val;

  /* computes average value on black margin */
  for (k = 0; k < channels; k++)
    {
      avg[k] = 0;
      count = 0;
      for (i = 0; i < lines; i++)
        {
          for (j = 0; j < black; j++)
            {
              val = data[i * channels * pixels + j + k];
              avg[k] += val;
              count++;
            }
        }
      if (count)
        avg[k] /= count;
      DBG(DBG_info, "%s: avg[%d] = %d\n", __func__, k, avg[k]);
    }
  average = 0;
  for (i = 0; i < channels; i++)
    average += avg[i];
  average /= channels;
  DBG(DBG_info, "%s: average = %d\n", __func__, average);
  return average;
}

void CommandSetGl846::offset_calibration(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                         Genesys_Register_Set& regs) const
{
    DBG_HELPER(dbg);
    unsigned channels;
    int pass = 0, avg;
    int topavg, bottomavg, lines;
  int top, bottom, black_pixels, pixels;

    // no gain nor offset for AKM AFE
    uint8_t reg04 = dev->interface->read_register(REG_0x04);
    if ((reg04 & REG_0x04_FESET) == 0x02) {
      return;
    }

  /* offset calibration is always done in color mode */
  channels = 3;
  lines=1;
    pixels = (sensor.sensor_pixels * sensor.optical_res) / sensor.optical_res;
    black_pixels = (sensor.black_pixels * sensor.optical_res) / sensor.optical_res;
  DBG(DBG_io2, "%s: black_pixels=%d\n", __func__, black_pixels);

    ScanSession session;
    session.params.xres = sensor.optical_res;
    session.params.yres = sensor.optical_res;
    session.params.startx = 0;
    session.params.starty = 0;
    session.params.pixels = pixels;
    session.params.lines = lines;
    session.params.depth = 8;
    session.params.channels = channels;
    session.params.scan_method = dev->settings.scan_method;
    session.params.scan_mode = ScanColorMode::COLOR_SINGLE_PASS;
    session.params.color_filter = dev->settings.color_filter;
    session.params.flags = ScanFlag::DISABLE_SHADING |
                           ScanFlag::DISABLE_GAMMA |
                           ScanFlag::SINGLE_LINE |
                           ScanFlag::IGNORE_LINE_DISTANCE;
    compute_session(dev, session, sensor);

    init_regs_for_scan_session(dev, sensor, &regs, session);

  sanei_genesys_set_motor_power(regs, false);

  /* init gain */
  dev->frontend.set_gain(0, 0);
  dev->frontend.set_gain(1, 0);
  dev->frontend.set_gain(2, 0);

  /* scan with no move */
  bottom = 10;
  dev->frontend.set_offset(0, bottom);
  dev->frontend.set_offset(1, bottom);
  dev->frontend.set_offset(2, bottom);

    set_fe(dev, sensor, AFE_SET);
    dev->interface->write_registers(regs);
  DBG(DBG_info, "%s: starting first line reading\n", __func__);
    begin_scan(dev, sensor, &regs, true);

    if (is_testing_mode()) {
        dev->interface->test_checkpoint("offset_calibration");
        return;
    }

    auto first_line = read_unshuffled_image_from_scanner(dev, session, session.output_total_bytes);
  if (DBG_LEVEL >= DBG_data)
   {
      char fn[30];
        std::snprintf(fn, 30, "gl846_offset%03d.pnm", bottom);
        sanei_genesys_write_pnm_file(fn, first_line);
   }

    bottomavg = dark_average(first_line.get_row_ptr(0), pixels, lines, channels, black_pixels);
  DBG(DBG_io2, "%s: bottom avg=%d\n", __func__, bottomavg);

  /* now top value */
  top = 255;
  dev->frontend.set_offset(0, top);
  dev->frontend.set_offset(1, top);
  dev->frontend.set_offset(2, top);
    set_fe(dev, sensor, AFE_SET);
    dev->interface->write_registers(regs);
  DBG(DBG_info, "%s: starting second line reading\n", __func__);
    begin_scan(dev, sensor, &regs, true);
    auto second_line = read_unshuffled_image_from_scanner(dev, session, session.output_total_bytes);

    topavg = dark_average(second_line.get_row_ptr(0), pixels, lines, channels, black_pixels);
  DBG(DBG_io2, "%s: top avg=%d\n", __func__, topavg);

  /* loop until acceptable level */
  while ((pass < 32) && (top - bottom > 1))
    {
      pass++;

      /* settings for new scan */
      dev->frontend.set_offset(0, (top + bottom) / 2);
      dev->frontend.set_offset(1, (top + bottom) / 2);
      dev->frontend.set_offset(2, (top + bottom) / 2);

        // scan with no move
        set_fe(dev, sensor, AFE_SET);
        dev->interface->write_registers(regs);
      DBG(DBG_info, "%s: starting second line reading\n", __func__);
        begin_scan(dev, sensor, &regs, true);
        second_line = read_unshuffled_image_from_scanner(dev, session, session.output_total_bytes);

        if (DBG_LEVEL >= DBG_data) {
            char fn[30];
            std::snprintf(fn, 30, "gl846_offset%03d.pnm", dev->frontend.get_offset(1));
            sanei_genesys_write_pnm_file(fn, second_line);
        }

        avg = dark_average(second_line.get_row_ptr(0), pixels, lines, channels, black_pixels);
      DBG(DBG_info, "%s: avg=%d offset=%d\n", __func__, avg, dev->frontend.get_offset(1));

      /* compute new boundaries */
      if (topavg == avg)
        {
          topavg = avg;
          top = dev->frontend.get_offset(1);
        }
      else
        {
          bottomavg = avg;
          bottom = dev->frontend.get_offset(1);
        }
    }
  DBG(DBG_info, "%s: offset=(%d,%d,%d)\n", __func__,
      dev->frontend.get_offset(0),
      dev->frontend.get_offset(1),
      dev->frontend.get_offset(2));
}

void CommandSetGl846::coarse_gain_calibration(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                              Genesys_Register_Set& regs, int dpi) const
{
    DBG_HELPER(dbg);
  int pixels;
  float gain[3],coeff;
    int code, lines;

  DBG(DBG_proc, "%s: dpi = %d\n", __func__, dpi);

    // no gain nor offset for AKM AFE
    uint8_t reg04 = dev->interface->read_register(REG_0x04);
    if ((reg04 & REG_0x04_FESET) == 0x02) {
      return;
    }

  /* coarse gain calibration is always done in color mode */
    unsigned channels = 3;

  /* follow CKSEL */
  if(dev->settings.xres<sensor.optical_res)
    {
        coeff = 0.9f;
    }
  else
    {
      coeff=1.0;
    }
  lines=10;
    pixels = (sensor.sensor_pixels * sensor.optical_res) / sensor.optical_res;

    ScanSession session;
    session.params.xres = sensor.optical_res;
    session.params.yres = sensor.optical_res;
    session.params.startx = 0;
    session.params.starty = 0;
    session.params.pixels = pixels;
    session.params.lines = lines;
    session.params.depth = 8;
    session.params.channels = channels;
    session.params.scan_method = dev->settings.scan_method;
    session.params.scan_mode = ScanColorMode::COLOR_SINGLE_PASS;
    session.params.color_filter = dev->settings.color_filter;
    session.params.flags = ScanFlag::DISABLE_SHADING |
                           ScanFlag::DISABLE_GAMMA |
                           ScanFlag::SINGLE_LINE |
                           ScanFlag::IGNORE_LINE_DISTANCE;
    compute_session(dev, session, sensor);

    try {
        init_regs_for_scan_session(dev, sensor, &regs, session);
    } catch (...) {
        catch_all_exceptions(__func__, [&](){ sanei_genesys_set_motor_power(regs, false); });
        throw;
    }

    sanei_genesys_set_motor_power(regs, false);

    dev->interface->write_registers(regs);

    set_fe(dev, sensor, AFE_SET);
    begin_scan(dev, sensor, &regs, true);

    if (is_testing_mode()) {
        dev->interface->test_checkpoint("coarse_gain_calibration");
        scanner_stop_action(*dev);
        move_back_home(dev, true);
        return;
    }

    auto image = read_unshuffled_image_from_scanner(dev, session, session.output_total_bytes);

    if (DBG_LEVEL >= DBG_data) {
        sanei_genesys_write_pnm_file("gl846_gain.pnm", image);
    }

  /* average value on each channel */
    for (unsigned ch = 0; ch < channels; ch++) {

        auto width = image.get_width();

        std::uint64_t total = 0;
        for (std::size_t x = width / 4; x < (width * 3 / 4); x++) {
            total += image.get_raw_channel(x, 0, ch);
        }

        total /= width / 2;

        gain[ch] = (static_cast<float>(sensor.gain_white_ref) * coeff) / total;

      /* turn logical gain value into gain code, checking for overflow */
        code = static_cast<int>(283 - 208 / gain[ch]);
        code = clamp(code, 0, 255);
        dev->frontend.set_gain(ch, code);

        DBG(DBG_proc, "%s: channel %d, total=%d, gain = %f, setting:%d\n", __func__, ch,
            static_cast<unsigned>(total), gain[ch], dev->frontend.get_gain(ch));
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

    scanner_stop_action(*dev);

    move_back_home(dev, true);
}

bool CommandSetGl846::needs_home_before_init_regs_for_scan(Genesys_Device* dev) const
{
    (void) dev;
    return false;
}

void CommandSetGl846::init_regs_for_warmup(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                           Genesys_Register_Set* regs, int* channels,
                                           int* total_size) const
{
    (void) dev;
    (void) sensor;
    (void) regs;
    (void) channels;
    (void) total_size;
    throw SaneException("not implemented");
}

void CommandSetGl846::send_gamma_table(Genesys_Device* dev, const Genesys_Sensor& sensor) const
{
    sanei_genesys_send_gamma_table(dev, sensor);
}

void CommandSetGl846::wait_for_motor_stop(Genesys_Device* dev) const
{
    (void) dev;
}

void CommandSetGl846::load_document(Genesys_Device* dev) const
{
    (void) dev;
    throw SaneException("not implemented");
}

void CommandSetGl846::detect_document_end(Genesys_Device* dev) const
{
    (void) dev;
    throw SaneException("not implemented");
}

void CommandSetGl846::eject_document(Genesys_Device* dev) const
{
    (void) dev;
    throw SaneException("not implemented");
}

void CommandSetGl846::move_to_ta(Genesys_Device* dev) const
{
    (void) dev;
    throw SaneException("not implemented");
}

std::unique_ptr<CommandSet> create_gl846_cmd_set()
{
    return std::unique_ptr<CommandSet>(new CommandSetGl846{});
}

} // namespace gl846
} // namespace genesys
