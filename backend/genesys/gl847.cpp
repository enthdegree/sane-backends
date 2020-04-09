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

#include "gl847.h"
#include "gl847_registers.h"
#include "test_settings.h"

#include <vector>

namespace genesys {
namespace gl847 {

/**
 * compute the step multiplier used
 */
static int
gl847_get_step_multiplier (Genesys_Register_Set * regs)
{
    GenesysRegister *r = sanei_genesys_get_address(regs, 0x9d);
    int value = 1;
    if (r != nullptr)
    {
      value = (r->value & 0x0f)>>1;
      value = 1 << value;
    }
  DBG (DBG_io, "%s: step multiplier is %d\n", __func__, value);
  return value;
}

/** @brief sensor specific settings
*/
static void gl847_setup_sensor(Genesys_Device * dev, const Genesys_Sensor& sensor,
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
gl847_init_registers (Genesys_Device * dev)
{
    DBG_HELPER(dbg);
  int lide700=0;
  uint8_t val;

  /* 700F class needs some different initial settings */
    if (dev->model->model_id == ModelId::CANON_LIDE_700F) {
       lide700 = 1;
    }

    dev->reg.clear();

    dev->reg.init_reg(0x01, 0x82);
    dev->reg.init_reg(0x02, 0x18);
    dev->reg.init_reg(0x03, 0x50);
    dev->reg.init_reg(0x04, 0x12);
    dev->reg.init_reg(0x05, 0x80);
    dev->reg.init_reg(0x06, 0x50); // FASTMODE + POWERBIT
    dev->reg.init_reg(0x08, 0x10);
    dev->reg.init_reg(0x09, 0x01);
    dev->reg.init_reg(0x0a, 0x00);
    dev->reg.init_reg(0x0b, 0x01);
    dev->reg.init_reg(0x0c, 0x02);

    // LED exposures
    dev->reg.init_reg(0x10, 0x00);
    dev->reg.init_reg(0x11, 0x00);
    dev->reg.init_reg(0x12, 0x00);
    dev->reg.init_reg(0x13, 0x00);
    dev->reg.init_reg(0x14, 0x00);
    dev->reg.init_reg(0x15, 0x00);

    dev->reg.init_reg(0x16, 0x10); // SENSOR_DEF
    dev->reg.init_reg(0x17, 0x08); // SENSOR_DEF
    dev->reg.init_reg(0x18, 0x00); // SENSOR_DEF

    // EXPDMY
    dev->reg.init_reg(0x19, 0x50); // SENSOR_DEF

    dev->reg.init_reg(0x1a, 0x34); // SENSOR_DEF
    dev->reg.init_reg(0x1b, 0x00); // SENSOR_DEF
    dev->reg.init_reg(0x1c, 0x02); // SENSOR_DEF
    dev->reg.init_reg(0x1d, 0x04); // SENSOR_DEF
    dev->reg.init_reg(0x1e, 0x10);
    dev->reg.init_reg(0x1f, 0x04);
    dev->reg.init_reg(0x20, 0x02);
    dev->reg.init_reg(0x21, 0x10);
    dev->reg.init_reg(0x22, 0x7f);
    dev->reg.init_reg(0x23, 0x7f);
    dev->reg.init_reg(0x24, 0x10);
    dev->reg.init_reg(0x25, 0x00);
    dev->reg.init_reg(0x26, 0x00);
    dev->reg.init_reg(0x27, 0x00);
    dev->reg.init_reg(0x2c, 0x09);
    dev->reg.init_reg(0x2d, 0x60);
    dev->reg.init_reg(0x2e, 0x80);
    dev->reg.init_reg(0x2f, 0x80);
    dev->reg.init_reg(0x30, 0x00);
    dev->reg.init_reg(0x31, 0x10);
    dev->reg.init_reg(0x32, 0x15);
    dev->reg.init_reg(0x33, 0x0e);
    dev->reg.init_reg(0x34, 0x40);
    dev->reg.init_reg(0x35, 0x00);
    dev->reg.init_reg(0x36, 0x2a);
    dev->reg.init_reg(0x37, 0x30);
    dev->reg.init_reg(0x38, 0x2a);
    dev->reg.init_reg(0x39, 0xf8);
    dev->reg.init_reg(0x3d, 0x00);
    dev->reg.init_reg(0x3e, 0x00);
    dev->reg.init_reg(0x3f, 0x00);
    dev->reg.init_reg(0x52, 0x03); // SENSOR_DEF
    dev->reg.init_reg(0x53, 0x07); // SENSOR_DEF
    dev->reg.init_reg(0x54, 0x00); // SENSOR_DEF
    dev->reg.init_reg(0x55, 0x00); // SENSOR_DEF
    dev->reg.init_reg(0x56, 0x00); // SENSOR_DEF
    dev->reg.init_reg(0x57, 0x00); // SENSOR_DEF
    dev->reg.init_reg(0x58, 0x2a); // SENSOR_DEF
    dev->reg.init_reg(0x59, 0xe1); // SENSOR_DEF
    dev->reg.init_reg(0x5a, 0x55); // SENSOR_DEF
    dev->reg.init_reg(0x5e, 0x41);
    dev->reg.init_reg(0x5f, 0x40);
    dev->reg.init_reg(0x60, 0x00);
    dev->reg.init_reg(0x61, 0x21);
    dev->reg.init_reg(0x62, 0x40);
    dev->reg.init_reg(0x63, 0x00);
    dev->reg.init_reg(0x64, 0x21);
    dev->reg.init_reg(0x65, 0x40);
    dev->reg.init_reg(0x67, 0x80);
    dev->reg.init_reg(0x68, 0x80);
    dev->reg.init_reg(0x69, 0x20);
    dev->reg.init_reg(0x6a, 0x20);

    // CK1MAP
    dev->reg.init_reg(0x74, 0x00); // SENSOR_DEF
    dev->reg.init_reg(0x75, 0x00); // SENSOR_DEF
    dev->reg.init_reg(0x76, 0x3c); // SENSOR_DEF

    // CK3MAP
    dev->reg.init_reg(0x77, 0x00); // SENSOR_DEF
    dev->reg.init_reg(0x78, 0x00); // SENSOR_DEF
    dev->reg.init_reg(0x79, 0x9f); // SENSOR_DEF

    // CK4MAP
    dev->reg.init_reg(0x7a, 0x00); // SENSOR_DEF
    dev->reg.init_reg(0x7b, 0x00); // SENSOR_DEF
    dev->reg.init_reg(0x7c, 0x55); // SENSOR_DEF

    dev->reg.init_reg(0x7d, 0x00);

    // NOTE: autoconf is a non working option
    dev->reg.init_reg(0x87, 0x02);
    dev->reg.init_reg(0x9d, 0x06);
    dev->reg.init_reg(0xa2, 0x0f);
    dev->reg.init_reg(0xbd, 0x18);
    dev->reg.init_reg(0xfe, 0x08);

    // gamma[0] and gamma[256] values
    dev->reg.init_reg(0xbe, 0x00);
    dev->reg.init_reg(0xc5, 0x00);
    dev->reg.init_reg(0xc6, 0x00);
    dev->reg.init_reg(0xc7, 0x00);
    dev->reg.init_reg(0xc8, 0x00);
    dev->reg.init_reg(0xc9, 0x00);
    dev->reg.init_reg(0xca, 0x00);

  /* LiDE 700 fixups */
    if (lide700) {
        dev->reg.init_reg(0x5f, 0x04);
        dev->reg.init_reg(0x7d, 0x80);

      /* we write to these registers only once */
      val=0;
        dev->interface->write_register(REG_0x7E, val);
        dev->interface->write_register(REG_0x9E, val);
        dev->interface->write_register(REG_0x9F, val);
        dev->interface->write_register(REG_0xAB, val);
    }

    const auto& sensor = sanei_genesys_find_sensor_any(dev);
    const auto& dpihw_sensor = sanei_genesys_find_sensor(dev, sensor.optical_res,
                                                         3, ScanMethod::FLATBED);
    sanei_genesys_set_dpihw(dev->reg, dpihw_sensor.register_dpihw);
}

/**@brief send slope table for motor movement
 * Send slope_table in machine byte order
 * @param dev device to send slope table
 * @param table_nr index of the slope table in ASIC memory
 * Must be in the [0-4] range.
 * @param slope_table pointer to 16 bit values array of the slope table
 * @param steps number of elements in the slope table
 */
static void gl847_send_slope_table(Genesys_Device* dev, int table_nr,
                                   const std::vector<uint16_t>& slope_table,
                                   int steps)
{
    DBG_HELPER_ARGS(dbg, "table_nr = %d, steps = %d", table_nr, steps);
  int i;

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

    if (dev->interface->is_mock()) {
        dev->interface->record_slope_table(table_nr, slope_table);
    }
    // slope table addresses are fixed
    dev->interface->write_ahb(0x10000000 + 0x4000 * table_nr, steps * 2, table.data());
}

/**
 * Set register values of Analog Device type frontend
 * */
static void gl847_set_ad_fe(Genesys_Device* dev, uint8_t set)
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

    // reset DAC
    dev->interface->write_fe_register(0x00, 0x80);

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
void CommandSetGl847::set_fe(Genesys_Device* dev, const Genesys_Sensor& sensor, uint8_t set) const
{
    DBG_HELPER_ARGS(dbg, "%s", set == AFE_INIT ? "init" :
                               set == AFE_SET ? "set" :
                               set == AFE_POWER_SAVE ? "powersave" : "huh?");

    (void) sensor;

    uint8_t val = dev->interface->read_register(REG_0x04);
    uint8_t frontend_type = val & REG_0x04_FESET;

    // route to AD devices
    if (frontend_type == 0x02) {
        gl847_set_ad_fe(dev, set);
        return;
    }

    throw SaneException("unsupported frontend type %d", frontend_type);
}


// @brief set up motor related register for scan
static void gl847_init_motor_regs_scan(Genesys_Device* dev,
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
    DBG_HELPER_ARGS(dbg, "scan_exposure_time=%d, can_yres=%d, step_type=%d, scan_lines=%d, "
                         "scan_dummy=%d, feed_steps=%d, flags=%x",
                    scan_exposure_time, scan_yres, static_cast<unsigned>(motor_profile.step_type),
                    scan_lines, scan_dummy, feed_steps, static_cast<unsigned>(flags));

    unsigned step_multiplier = gl847_get_step_multiplier (reg);

    bool use_fast_fed = false;
    if (dev->settings.yres == 4444 && feed_steps > 100 && !has_flag(flags, MotorFlag::FEED)) {
        use_fast_fed = true;
    }

    reg->set24(REG_LINCNT, scan_lines);
    DBG(DBG_io, "%s: lincnt=%d\n", __func__, scan_lines);

    reg->set8(REG_0x02, 0);
    sanei_genesys_set_motor_power(*reg, true);

    std::uint8_t reg02 = reg->get8(REG_0x02);
    if (use_fast_fed) {
        reg02 |= REG_0x02_FASTFED;
    } else {
        reg02 &= ~REG_0x02_FASTFED;
    }

    if (has_flag(flags, MotorFlag::AUTO_GO_HOME)) {
        reg02 |= REG_0x02_AGOHOME | REG_0x02_NOTHOME;
    }

    if (has_flag(flags, MotorFlag::DISABLE_BUFFER_FULL_MOVE) ||(scan_yres>=sensor.optical_res)) {
        reg02 |= REG_0x02_ACDCDIS;
    }
    if (has_flag(flags, MotorFlag::REVERSE)) {
        reg02 |= REG_0x02_MTRREV;
    } else {
        reg02 &= ~REG_0x02_MTRREV;
    }
    reg->set8(REG_0x02, reg02);

    // scan and backtracking slope table
    auto scan_table = sanei_genesys_slope_table(dev->model->asic_type, scan_yres,
                                                scan_exposure_time, dev->motor.base_ydpi,
                                                step_multiplier, motor_profile);
    gl847_send_slope_table(dev, SCAN_TABLE, scan_table.table, scan_table.steps_count);
    gl847_send_slope_table(dev, BACKTRACK_TABLE, scan_table.table, scan_table.steps_count);

    // fast table
    unsigned fast_dpi = sanei_genesys_get_lowest_ydpi(dev);

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

    gl847_send_slope_table(dev, STOP_TABLE, fast_table.table, fast_table.steps_count);
    gl847_send_slope_table(dev, FAST_TABLE, fast_table.table, fast_table.steps_count);
    gl847_send_slope_table(dev, HOME_TABLE, fast_table.table, fast_table.steps_count);

    // correct move distance by acceleration and deceleration amounts
    unsigned feedl = feed_steps;
    unsigned dist = 0;
    if (use_fast_fed)
    {
        feedl <<= static_cast<unsigned>(fast_step_type);
        dist = (scan_table.steps_count + 2 * fast_table.steps_count);
        // TODO read and decode REG_0xAB
        dist += (reg->get8(0x5e) & 31);
        dist += reg->get8(REG_FEDCNT);
    } else {
        feedl <<= static_cast<unsigned>(motor_profile.step_type);
        dist = scan_table.steps_count;
        if (has_flag(flags, MotorFlag::FEED)) {
            dist *= 2;
        }
    }
    DBG(DBG_io2, "%s: acceleration distance=%d\n", __func__, dist);

    // check for overflow
    if (dist < feedl) {
        feedl -= dist;
    } else {
        feedl = 0;
    }

    reg->set24(REG_FEEDL, feedl);
    DBG(DBG_io ,"%s: feedl=%d\n", __func__, feedl);

    unsigned ccdlmt = (reg->get8(REG_0x0C) & REG_0x0C_CCDLMT) + 1;
    unsigned tgtime = 1 << (reg->get8(REG_0x1C) & REG_0x1C_TGTIME);

    // hi res motor speed GPIO
    uint8_t effective = dev->interface->read_register(REG_0x6C);

    // if quarter step, bipolar Vref2

    std::uint8_t val = effective;
    if (motor_profile.step_type == StepType::QUARTER) {
        val = effective & ~REG_0x6C_GPIO13;
    } else if (static_cast<unsigned>(motor_profile.step_type) > static_cast<unsigned>(StepType::QUARTER)) {
        val = effective | REG_0x6C_GPIO13;
    }
    dev->interface->write_register(REG_0x6C, val);

    // effective scan
    effective = dev->interface->read_register(REG_0x6C);
    val = effective | REG_0x6C_GPIO10;
    dev->interface->write_register(REG_0x6C, val);

    unsigned min_restep = scan_table.steps_count / (2 * step_multiplier) - 1;
    if (min_restep < 1) {
        min_restep = 1;
    }

    reg->set8(REG_FWDSTEP, min_restep);
    reg->set8(REG_BWDSTEP, min_restep);

    std::uint32_t z1, z2;
    sanei_genesys_calculate_zmod(use_fast_fed,
                                 scan_exposure_time * ccdlmt * tgtime,
                                 scan_table.table,
                                 scan_table.steps_count,
                                 feedl,
                                 min_restep * step_multiplier,
                                 &z1,
                                 &z2);

    DBG(DBG_info, "%s: z1 = %d\n", __func__, z1);
    reg->set24(REG_0x60, z1 | (static_cast<unsigned>(motor_profile.step_type) << (16+REG_0x60S_STEPSEL)));

    DBG(DBG_info, "%s: z2 = %d\n", __func__, z2);
    reg->set24(REG_0x63, z2 | (static_cast<unsigned>(motor_profile.step_type) << (16+REG_0x63S_FSTPSEL)));

    reg->set8_mask(REG_0x1E, scan_dummy, 0x0f);

    reg->set8(REG_0x67, REG_0x67_MTRPWM);
    reg->set8(REG_0x68, REG_0x68_FASTPWM);

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
static void gl847_init_optical_regs_scan(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                         Genesys_Register_Set* reg, unsigned int exposure_time,
                                         const ScanSession& session)
{
    DBG_HELPER_ARGS(dbg, "exposure_time=%d", exposure_time);
  GenesysRegister *r;

    gl847_setup_sensor(dev, sensor, reg);

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

  r = sanei_genesys_get_address (reg, REG_0x03);
  r->value &= ~REG_0x03_AVEENB;

    sanei_genesys_set_lamp_power(dev, sensor, *reg,
                                 !has_flag(session.params.flags, ScanFlag::DISABLE_LAMP));

  /* BW threshold */
    r = sanei_genesys_get_address (reg, 0x2e);
  r->value = dev->settings.threshold;
    r = sanei_genesys_get_address (reg, 0x2f);
  r->value = dev->settings.threshold;

  /* monochrome / color scan */
    r = sanei_genesys_get_address (reg, REG_0x04);
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
        r->value |= 0x10; // mono
    }

    const auto& dpihw_sensor = sanei_genesys_find_sensor(dev, session.output_resolution,
                                                         session.params.channels,
                                                         session.params.scan_method);
    sanei_genesys_set_dpihw(*reg, dpihw_sensor.register_dpihw);

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
        if (session.enable_ledadd) {
            r->value |= REG_0x01_TRUEGRAY;
        }
        */
    }

    reg->set16(REG_DPISET, sensor.register_dpiset);
    reg->set16(REG_STRPIXEL, session.pixel_startx);
    reg->set16(REG_ENDPIXEL, session.pixel_endx);

    build_image_pipeline(dev, sensor, session);

  /* MAXWD is expressed in 4 words unit */
    // BUG: we shouldn't multiply by channels here
    reg->set24(REG_MAXWD, (session.output_line_bytes_raw * session.params.channels >> 2));

    reg->set16(REG_LPERIOD, exposure_time);
  DBG(DBG_io2, "%s: exposure_time used=%d\n", __func__, exposure_time);

  r = sanei_genesys_get_address (reg, 0x34);
  r->value = sensor.dummy_pixel;
}

void CommandSetGl847::init_regs_for_scan_session(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                                 Genesys_Register_Set* reg,
                                                 const ScanSession& session) const
{
    DBG_HELPER(dbg);
    session.assert_computed();

  int move;
  int exposure_time;

  int slope_dpi = 0;
  int dummy = 0;

    dummy = 3 - session.params.channels;

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
    gl847_init_optical_regs_scan(dev, sensor, reg, exposure_time, session);

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

    gl847_init_motor_regs_scan(dev, sensor, reg, motor_profile, exposure_time, slope_dpi,
                               session.optical_line_count, dummy, move, mflags);

    dev->read_buffer.clear();
    dev->read_buffer.alloc(session.buffer_size_read);

    dev->read_active = true;

    dev->session = session;

    dev->total_bytes_read = 0;
    dev->total_bytes_to_read = session.output_line_bytes_requested * session.params.lines;

    DBG(DBG_info, "%s: total bytes to send = %zu\n", __func__, dev->total_bytes_to_read);
}

ScanSession CommandSetGl847::calculate_scan_session(const Genesys_Device* dev,
                                                    const Genesys_Sensor& sensor,
                                                    const Genesys_Settings& settings) const
{
    DBG(DBG_info, "%s ", __func__);
    debug_dump(DBG_info, settings);

    /*  Steps to move to reach scanning area:

        - first we move to physical start of scanning either by a fixed steps amount from the
          black strip or by a fixed amount from parking position, minus the steps done during
          shading calibration.

        - then we move by the needed offset whitin physical scanning area
    */
    unsigned move_dpi = dev->motor.base_ydpi;

    float move = dev->model->y_offset;
    move = move + settings.tl_y;
    move = static_cast<float>((move * move_dpi) / MM_PER_INCH);
    move -= dev->head_pos(ScanHeadId::PRIMARY);

    float start = dev->model->x_offset;
    start = start + dev->settings.tl_x;
    start = static_cast<float>((start * settings.xres) / MM_PER_INCH);

    ScanSession session;
    session.params.xres = settings.xres;
    session.params.yres = settings.yres;
    session.params.startx = static_cast<unsigned>(start);
    session.params.starty = static_cast<unsigned>(move);
    session.params.pixels = settings.pixels;
    session.params.requested_pixels = settings.requested_pixels;
    session.params.lines = settings.lines;
    session.params.depth = settings.depth;
    session.params.channels = settings.get_channels();
    session.params.scan_method = settings.scan_method;
    session.params.scan_mode = settings.scan_mode;
    session.params.color_filter = settings.color_filter;
    // backtracking isn't handled well, so don't enable it
    session.params.flags = ScanFlag::DISABLE_BUFFER_FULL_MOVE;

    compute_session(dev, session, sensor);

    return session;
}

// for fast power saving methods only, like disabling certain amplifiers
void CommandSetGl847::save_power(Genesys_Device* dev, bool enable) const
{
    DBG_HELPER_ARGS(dbg, "enable = %d", enable);
    (void) dev;
}

void CommandSetGl847::set_powersaving(Genesys_Device* dev, int delay /* in minutes */) const
{
    (void) dev;
    DBG_HELPER_ARGS(dbg, "delay = %d", delay);
}

// Send the low-level scan command
void CommandSetGl847::begin_scan(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                 Genesys_Register_Set* reg, bool start_motor) const
{
    DBG_HELPER(dbg);
    (void) sensor;
  uint8_t val;
  GenesysRegister *r;

    // clear GPIO 10
    if (dev->model->gpio_id != GpioId::CANON_LIDE_700F) {
        val = dev->interface->read_register(REG_0x6C);
        val &= ~REG_0x6C_GPIO10;
        dev->interface->write_register(REG_0x6C, val);
    }

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
void CommandSetGl847::end_scan(Genesys_Device* dev, Genesys_Register_Set* reg,
                               bool check_stop) const
{
    (void) reg;
    DBG_HELPER_ARGS(dbg, "check_stop = %d", check_stop);

    if (!dev->model->is_sheetfed) {
        scanner_stop_action(*dev);
    }
}

/** Park head
 * Moves the slider to the home (top) position slowly
 * @param dev device to park
 * @param wait_until_home true to make the function waiting for head
 * to be home before returning, if fals returne immediately
*/
void CommandSetGl847::move_back_home(Genesys_Device* dev, bool wait_until_home) const
{
    scanner_move_back_home(*dev, wait_until_home);
}

// init registers for shading calibration
void CommandSetGl847::init_regs_for_shading(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                            Genesys_Register_Set& regs) const
{
    DBG_HELPER(dbg);

    unsigned channels = 3;

    unsigned resolution = sensor.shading_resolution;

    const auto& calib_sensor = sanei_genesys_find_sensor(dev, resolution, channels,
                                                         dev->settings.scan_method);

    unsigned calib_lines =
            static_cast<unsigned>(dev->model->y_size_calib_mm * resolution / MM_PER_INCH);

    ScanSession session;
    session.params.xres = resolution;
    session.params.yres = resolution;
    session.params.startx = 0;
    session.params.starty = 20;
    session.params.pixels = dev->model->x_size_calib_mm * resolution / MM_PER_INCH;;
    session.params.lines = calib_lines;
    session.params.depth = 16;
    session.params.channels = channels;
    session.params.scan_method = dev->settings.scan_method;
    session.params.scan_mode = ScanColorMode::COLOR_SINGLE_PASS;
    session.params.color_filter = dev->settings.color_filter;
    session.params.flags = ScanFlag::DISABLE_SHADING |
                           ScanFlag::DISABLE_GAMMA |
                           ScanFlag::DISABLE_BUFFER_FULL_MOVE;
    compute_session(dev, session, calib_sensor);

    init_regs_for_scan_session(dev, calib_sensor, &regs, session);

  /* we use ModelFlag::SHADING_REPARK */
    dev->set_head_pos_zero(ScanHeadId::PRIMARY);

    dev->calib_session = session;
}

/** @brief set up registers for the actual scan
 */
void CommandSetGl847::init_regs_for_scan(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                         Genesys_Register_Set& regs) const
{
    DBG_HELPER(dbg);

    auto session = calculate_scan_session(dev, sensor, dev->settings);

    /*  Fast move to scan area:

        We don't move fast the whole distance since it would involve computing
        acceleration/deceleration distance for scan resolution. So leave a remainder for it so
        scan makes the final move tuning
    */
    if (dev->settings.get_channels() * dev->settings.yres >= 600 && session.params.starty > 700) {
        scanner_move(*dev, dev->model->default_method,
                     static_cast<unsigned>(session.params.starty - 500),
                     Direction::FORWARD);
        session.params.starty = 500;
    }
    compute_session(dev, session, sensor);

    init_regs_for_scan_session(dev, sensor, &regs, session);
}


/**
 * Send shading calibration data. The buffer is considered to always hold values
 * for all the channels.
 */
void CommandSetGl847::send_shading_data(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                        uint8_t* data, int size) const
{
    DBG_HELPER_ARGS(dbg, "writing %d bytes of shading data", size);
    std::uint32_t addr, length, i, pixels;
  uint8_t val,*ptr,*src;

  /* shading data is plit in 3 (up to 5 with IR) areas
     write(0x10014000,0x00000dd8)
     URB 23429  bulk_out len  3544  wrote 0x33 0x10 0x....
     write(0x1003e000,0x00000dd8)
     write(0x10068000,0x00000dd8)
   */
    length = static_cast<std::uint32_t>(size / 3);
    std::uint32_t strpixel = dev->session.pixel_startx;
    std::uint32_t endpixel = dev->session.pixel_endx;

  pixels=endpixel-strpixel;

  /* since we're using SHDAREA, substract startx coordinate from shading */
    strpixel -= (sensor.ccd_start_xoffset * 600) / sensor.optical_res;

  /* turn pixel value into bytes 2x16 bits words */
  strpixel*=2*2;
  pixels*=2*2;

    dev->interface->record_key_value("shading_offset", std::to_string(strpixel));
    dev->interface->record_key_value("shading_pixels", std::to_string(pixels));
    dev->interface->record_key_value("shading_length", std::to_string(length));
    dev->interface->record_key_value("shading_factor", std::to_string(sensor.shading_factor));

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

        // iterate on both sensor segment
        for (unsigned x = 0; x < pixels; x += 4 * sensor.shading_factor) {
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
SensorExposure CommandSetGl847::led_calibration(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                                Genesys_Register_Set& regs) const
{
    DBG_HELPER(dbg);
    int i;
  int avg[3], top[3], bottom[3];
  int turn;
  uint16_t exp[3];
  float move;

    move = dev->model->y_offset_calib_white;
    move = static_cast<float>((move * (dev->motor.base_ydpi / 4)) / MM_PER_INCH);
    if (move > 20) {
        scanner_move(*dev, dev->model->default_method, static_cast<unsigned>(move),
                     Direction::FORWARD);
    }
  DBG(DBG_io, "%s: move=%f steps\n", __func__, move);

  /* offset calibration is always done in color mode */
    unsigned channels = 3;
    unsigned resolution = sensor.shading_resolution;
    const auto& calib_sensor = sanei_genesys_find_sensor(dev, resolution, channels,
                                                         dev->settings.scan_method);

  /* initial calibration reg values */
  regs = dev->reg;

    ScanSession session;
    session.params.xres = resolution;
    session.params.yres = resolution;
    session.params.startx = 0;
    session.params.starty = 0;
    session.params.pixels = dev->model->x_size_calib_mm * resolution / MM_PER_INCH;
    session.params.lines = 1;
    session.params.depth = 16;
    session.params.channels = channels;
    session.params.scan_method = dev->settings.scan_method;
    session.params.scan_mode = ScanColorMode::COLOR_SINGLE_PASS;
    session.params.color_filter = dev->settings.color_filter;
    session.params.flags = ScanFlag::DISABLE_SHADING |
                           ScanFlag::DISABLE_GAMMA |
                           ScanFlag::SINGLE_LINE |
                           ScanFlag::IGNORE_STAGGER_OFFSET |
                           ScanFlag::IGNORE_COLOR_OFFSET;
    compute_session(dev, session, calib_sensor);

    init_regs_for_scan_session(dev, calib_sensor, &regs, session);

    // initial loop values and boundaries
    exp[0] = calib_sensor.exposure.red;
    exp[1] = calib_sensor.exposure.green;
    exp[2] = calib_sensor.exposure.blue;

    bottom[0] = 28000;
    bottom[1] = 28000;
    bottom[2] = 28000;

    top[0] = 32000;
    top[1] = 32000;
    top[2] = 32000;

  turn = 0;

  /* no move during led calibration */
    bool acceptable = false;
  sanei_genesys_set_motor_power(regs, false);
  do
    {
        // set up exposure
        regs.set16(REG_EXPR,exp[0]);
        regs.set16(REG_EXPG,exp[1]);
        regs.set16(REG_EXPB,exp[2]);

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
            std::snprintf(fn, 30, "gl847_led_%02d.pnm", turn);
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
            if (avg[i] < bottom[i] || avg[i] > top[i]) {
                auto target = (bottom[i] + top[i]) / 2;
                exp[i] = (exp[i] * target) / avg[i];
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

    // go back home
    if (move>20) {
        move_back_home(dev, true);
    }

    return { exp[0], exp[1], exp[2] };
}

/**
 * set up GPIO/GPOE for idle state
 */
static void gl847_init_gpio(Genesys_Device* dev)
{
    DBG_HELPER(dbg);
  int idx=0;

  /* search GPIO profile */
    while(gpios[idx].gpio_id != GpioId::UNKNOWN && dev->model->gpio_id != gpios[idx].gpio_id) {
      idx++;
    }
    if (gpios[idx].gpio_id == GpioId::UNKNOWN) {
        throw SaneException("failed to find GPIO profile for sensor_id=%d",
                            static_cast<unsigned>(dev->model->sensor_id));
    }

    dev->interface->write_register(REG_0xA7, gpios[idx].ra7);
    dev->interface->write_register(REG_0xA6, gpios[idx].ra6);

    dev->interface->write_register(REG_0x6E, gpios[idx].r6e);
    dev->interface->write_register(REG_0x6C, 0x00);

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
static void gl847_init_memory_layout(Genesys_Device* dev)
{
    DBG_HELPER(dbg);

    // TODO: move to initial register list
    switch (dev->model->model_id) {
        case ModelId::CANON_LIDE_100:
        case ModelId::CANON_LIDE_200:
        case ModelId::CANON_5600F:
            dev->interface->write_register(REG_0x0B, 0x29);
            break;
        case ModelId::CANON_LIDE_700F:
            dev->interface->write_register(REG_0x0B, 0x2a);
            break;
        default:
            throw SaneException("Unknown device");
    }

    // prevent further writings by bulk write register
    dev->reg.remove_reg(0x0b);

    apply_reg_settings_to_device_write_only(*dev, dev->memory_layout.regs);
}

/* *
 * initialize ASIC from power on condition
 */
void CommandSetGl847::asic_boot(Genesys_Device* dev, bool cold) const
{
    DBG_HELPER(dbg);

    // reset ASIC if cold boot
    if (cold) {
        dev->interface->write_register(0x0e, 0x01);
        dev->interface->write_register(0x0e, 0x00);
    }

    // test CHKVER
    uint8_t val = dev->interface->read_register(REG_0x40);
    if (val & REG_0x40_CHKVER) {
        val = dev->interface->read_register(0x00);
        DBG(DBG_info, "%s: reported version for genesys chip is 0x%02x\n", __func__, val);
    }

  /* Set default values for registers */
  gl847_init_registers (dev);

    // Write initial registers
    dev->interface->write_registers(dev->reg);

  /* Enable DRAM by setting a rising edge on bit 3 of reg 0x0b */
    val = dev->reg.find_reg(0x0b).value & REG_0x0B_DRAMSEL;
    val = (val | REG_0x0B_ENBDRAM);
    dev->interface->write_register(REG_0x0B, val);
    dev->reg.find_reg(0x0b).value = val;

  /* CIS_LINE */
    dev->reg.init_reg(0x08, REG_0x08_CIS_LINE);
    dev->interface->write_register(0x08, dev->reg.find_reg(0x08).value);

    // set up end access
    dev->interface->write_0x8c(0x10, 0x0b);
    dev->interface->write_0x8c(0x13, 0x0e);

    // setup gpio
    gl847_init_gpio(dev);

    // setup internal memory layout
    gl847_init_memory_layout (dev);

    dev->reg.init_reg(0xf8, 0x01);
    dev->interface->write_register(0xf8, dev->reg.find_reg(0xf8).value);
}

/**
 * initialize backend and ASIC : registers, motor tables, and gamma tables
 * then ensure scanner's head is at home
 */
void CommandSetGl847::init(Genesys_Device* dev) const
{
  DBG_INIT ();
    DBG_HELPER(dbg);

    sanei_genesys_asic_init(dev);
}

void CommandSetGl847::update_hardware_sensors(Genesys_Scanner* s) const
{
    DBG_HELPER(dbg);
  /* do what is needed to get a new set of events, but try to not lose
     any of them.
   */
  uint8_t val;
  uint8_t scan, file, email, copy;
    switch(s->dev->model->gpio_id) {
    case GpioId::CANON_LIDE_700F:
        scan=0x04;
        file=0x02;
        email=0x01;
        copy=0x08;
        break;
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

void CommandSetGl847::update_home_sensor_gpio(Genesys_Device& dev) const
{
    DBG_HELPER(dbg);

    if (dev.model->gpio_id == GpioId::CANON_LIDE_700F) {
        std::uint8_t val = dev.interface->read_register(REG_0x6C);
        val &= ~REG_0x6C_GPIO10;
        dev.interface->write_register(REG_0x6C, val);
    } else {
        std::uint8_t val = dev.interface->read_register(REG_0x6C);
        val |= REG_0x6C_GPIO10;
        dev.interface->write_register(REG_0x6C, val);
    }
}

void CommandSetGl847::offset_calibration(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                         Genesys_Register_Set& regs) const
{
    scanner_offset_calibration(*dev, sensor, regs);
}

void CommandSetGl847::coarse_gain_calibration(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                              Genesys_Register_Set& regs, int dpi) const
{
    scanner_coarse_gain_calibration(*dev, sensor, regs, dpi);
}

bool CommandSetGl847::needs_home_before_init_regs_for_scan(Genesys_Device* dev) const
{
    (void) dev;
    return false;
}

void CommandSetGl847::init_regs_for_warmup(Genesys_Device* dev, const Genesys_Sensor& sensor,
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

void CommandSetGl847::send_gamma_table(Genesys_Device* dev, const Genesys_Sensor& sensor) const
{
    sanei_genesys_send_gamma_table(dev, sensor);
}

void CommandSetGl847::wait_for_motor_stop(Genesys_Device* dev) const
{
    (void) dev;
}

void CommandSetGl847::load_document(Genesys_Device* dev) const
{
    (void) dev;
    throw SaneException("not implemented");
}

void CommandSetGl847::detect_document_end(Genesys_Device* dev) const
{
    (void) dev;
    throw SaneException("not implemented");
}

void CommandSetGl847::eject_document(Genesys_Device* dev) const
{
    (void) dev;
    throw SaneException("not implemented");
}

void CommandSetGl847::move_to_ta(Genesys_Device* dev) const
{
    (void) dev;
    throw SaneException("not implemented");
}

std::unique_ptr<CommandSet> create_gl847_cmd_set()
{
    return std::unique_ptr<CommandSet>(new CommandSetGl847{});
}

} // namespace gl847
} // namespace genesys
