/* sane - Scanner Access Now Easy.

   Copyright (C) 2010-2016 Stéphane Voltz <stef.dev@free.fr>


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

#include "gl124.h"
#include "gl124_registers.h"
#include "test_settings.h"

#include <vector>

namespace genesys {
namespace gl124 {

/** @brief set all registers to default values .
 * This function is called only once at the beginning and
 * fills register startup values for registers reused across scans.
 * Those that are rarely modified or not modified are written
 * individually.
 * @param dev device structure holding register set to initialize
 */
static void
gl124_init_registers (Genesys_Device * dev)
{
    DBG_HELPER(dbg);

    dev->reg.clear();

    // default to LiDE 110
    dev->reg.init_reg(0x01, 0xa2); // + REG_0x01_SHDAREA
    dev->reg.init_reg(0x02, 0x90);
    dev->reg.init_reg(0x03, 0x50);
    dev->reg.init_reg(0x04, 0x03);
    dev->reg.init_reg(0x05, 0x00);

    if(dev->model->sensor_id == SensorId::CIS_CANON_LIDE_120) {
    dev->reg.init_reg(0x06, 0x50);
    dev->reg.init_reg(0x07, 0x00);
    } else {
        dev->reg.init_reg(0x03, 0x50 & ~REG_0x03_AVEENB);
        dev->reg.init_reg(0x06, 0x50 | REG_0x06_GAIN4);
    }
    dev->reg.init_reg(0x09, 0x00);
    dev->reg.init_reg(0x0a, 0xc0);
    dev->reg.init_reg(0x0b, 0x2a);
    dev->reg.init_reg(0x0c, 0x12);
    dev->reg.init_reg(0x11, 0x00);
    dev->reg.init_reg(0x12, 0x00);
    dev->reg.init_reg(0x13, 0x0f);
    dev->reg.init_reg(0x14, 0x00);
    dev->reg.init_reg(0x15, 0x80);
    dev->reg.init_reg(0x16, 0x10); // SENSOR_DEF
    dev->reg.init_reg(0x17, 0x04); // SENSOR_DEF
    dev->reg.init_reg(0x18, 0x00); // SENSOR_DEF
    dev->reg.init_reg(0x19, 0x01); // SENSOR_DEF
    dev->reg.init_reg(0x1a, 0x30); // SENSOR_DEF
    dev->reg.init_reg(0x1b, 0x00); // SENSOR_DEF
    dev->reg.init_reg(0x1c, 0x00); // SENSOR_DEF
    dev->reg.init_reg(0x1d, 0x01); // SENSOR_DEF
    dev->reg.init_reg(0x1e, 0x10);
    dev->reg.init_reg(0x1f, 0x00);
    dev->reg.init_reg(0x20, 0x15); // SENSOR_DEF
    dev->reg.init_reg(0x21, 0x00);
    if(dev->model->sensor_id != SensorId::CIS_CANON_LIDE_120) {
        dev->reg.init_reg(0x22, 0x02);
    } else {
        dev->reg.init_reg(0x22, 0x14);
    }
    dev->reg.init_reg(0x23, 0x00);
    dev->reg.init_reg(0x24, 0x00);
    dev->reg.init_reg(0x25, 0x00);
    dev->reg.init_reg(0x26, 0x0d);
    dev->reg.init_reg(0x27, 0x48);
    dev->reg.init_reg(0x28, 0x00);
    dev->reg.init_reg(0x29, 0x56);
    dev->reg.init_reg(0x2a, 0x5e);
    dev->reg.init_reg(0x2b, 0x02);
    dev->reg.init_reg(0x2c, 0x02);
    dev->reg.init_reg(0x2d, 0x58);
    dev->reg.init_reg(0x3b, 0x00);
    dev->reg.init_reg(0x3c, 0x00);
    dev->reg.init_reg(0x3d, 0x00);
    dev->reg.init_reg(0x3e, 0x00);
    dev->reg.init_reg(0x3f, 0x02);
    dev->reg.init_reg(0x40, 0x00);
    dev->reg.init_reg(0x41, 0x00);
    dev->reg.init_reg(0x42, 0x00);
    dev->reg.init_reg(0x43, 0x00);
    dev->reg.init_reg(0x44, 0x00);
    dev->reg.init_reg(0x45, 0x00);
    dev->reg.init_reg(0x46, 0x00);
    dev->reg.init_reg(0x47, 0x00);
    dev->reg.init_reg(0x48, 0x00);
    dev->reg.init_reg(0x49, 0x00);
    dev->reg.init_reg(0x4f, 0x00);
    dev->reg.init_reg(0x52, 0x00); // SENSOR_DEF
    dev->reg.init_reg(0x53, 0x02); // SENSOR_DEF
    dev->reg.init_reg(0x54, 0x04); // SENSOR_DEF
    dev->reg.init_reg(0x55, 0x06); // SENSOR_DEF
    dev->reg.init_reg(0x56, 0x04); // SENSOR_DEF
    dev->reg.init_reg(0x57, 0x04); // SENSOR_DEF
    dev->reg.init_reg(0x58, 0x04); // SENSOR_DEF
    dev->reg.init_reg(0x59, 0x04); // SENSOR_DEF
    dev->reg.init_reg(0x5a, 0x1a); // SENSOR_DEF
    dev->reg.init_reg(0x5b, 0x00); // SENSOR_DEF
    dev->reg.init_reg(0x5c, 0xc0); // SENSOR_DEF
    dev->reg.init_reg(0x5f, 0x00);
    dev->reg.init_reg(0x60, 0x02);
    dev->reg.init_reg(0x61, 0x00); // SENSOR_DEF
    dev->reg.init_reg(0x62, 0x00);
    dev->reg.init_reg(0x63, 0x00);
    dev->reg.init_reg(0x64, 0x00);
    dev->reg.init_reg(0x65, 0x00);
    dev->reg.init_reg(0x66, 0x00);
    dev->reg.init_reg(0x67, 0x00);
    dev->reg.init_reg(0x68, 0x00);
    dev->reg.init_reg(0x69, 0x00);
    dev->reg.init_reg(0x6a, 0x00);
    dev->reg.init_reg(0x6b, 0x00);
    dev->reg.init_reg(0x6c, 0x00);
    dev->reg.init_reg(0x6e, 0x00);
    dev->reg.init_reg(0x6f, 0x00);

    if (dev->model->sensor_id != SensorId::CIS_CANON_LIDE_120) {
        dev->reg.init_reg(0x6d, 0xd0);
        dev->reg.init_reg(0x71, 0x08);
    } else {
        dev->reg.init_reg(0x6d, 0x00);
        dev->reg.init_reg(0x71, 0x1f);
    }
    dev->reg.init_reg(0x70, 0x00); // SENSOR_DEF
    dev->reg.init_reg(0x71, 0x08); // SENSOR_DEF
    dev->reg.init_reg(0x72, 0x08); // SENSOR_DEF
    dev->reg.init_reg(0x73, 0x0a); // SENSOR_DEF

    // CKxMAP
    dev->reg.init_reg(0x74, 0x00); // SENSOR_DEF
    dev->reg.init_reg(0x75, 0x00); // SENSOR_DEF
    dev->reg.init_reg(0x76, 0x3c); // SENSOR_DEF
    dev->reg.init_reg(0x77, 0x00); // SENSOR_DEF
    dev->reg.init_reg(0x78, 0x00); // SENSOR_DEF
    dev->reg.init_reg(0x79, 0x9f); // SENSOR_DEF
    dev->reg.init_reg(0x7a, 0x00); // SENSOR_DEF
    dev->reg.init_reg(0x7b, 0x00); // SENSOR_DEF
    dev->reg.init_reg(0x7c, 0x55); // SENSOR_DEF

    dev->reg.init_reg(0x7d, 0x00);
    dev->reg.init_reg(0x7e, 0x08);
    dev->reg.init_reg(0x7f, 0x58);

    if (dev->model->sensor_id != SensorId::CIS_CANON_LIDE_120) {
        dev->reg.init_reg(0x80, 0x00);
        dev->reg.init_reg(0x81, 0x14);
    } else {
        dev->reg.init_reg(0x80, 0x00);
        dev->reg.init_reg(0x81, 0x10);
    }

    // STRPIXEL
    dev->reg.init_reg(0x82, 0x00);
    dev->reg.init_reg(0x83, 0x00);
    dev->reg.init_reg(0x84, 0x00);

    // ENDPIXEL
    dev->reg.init_reg(0x85, 0x00);
    dev->reg.init_reg(0x86, 0x00);
    dev->reg.init_reg(0x87, 0x00);

    dev->reg.init_reg(0x88, 0x00); // SENSOR_DEF
    dev->reg.init_reg(0x89, 0x65); // SENSOR_DEF
    dev->reg.init_reg(0x8a, 0x00);
    dev->reg.init_reg(0x8b, 0x00);
    dev->reg.init_reg(0x8c, 0x00);
    dev->reg.init_reg(0x8d, 0x00);
    dev->reg.init_reg(0x8e, 0x00);
    dev->reg.init_reg(0x8f, 0x00);
    dev->reg.init_reg(0x90, 0x00);
    dev->reg.init_reg(0x91, 0x00);
    dev->reg.init_reg(0x92, 0x00);
    dev->reg.init_reg(0x93, 0x00); // SENSOR_DEF
    dev->reg.init_reg(0x94, 0x14); // SENSOR_DEF
    dev->reg.init_reg(0x95, 0x30); // SENSOR_DEF
    dev->reg.init_reg(0x96, 0x00); // SENSOR_DEF
    dev->reg.init_reg(0x97, 0x90); // SENSOR_DEF
    dev->reg.init_reg(0x98, 0x01); // SENSOR_DEF
    dev->reg.init_reg(0x99, 0x1f);
    dev->reg.init_reg(0x9a, 0x00);
    dev->reg.init_reg(0x9b, 0x80);
    dev->reg.init_reg(0x9c, 0x80);
    dev->reg.init_reg(0x9d, 0x3f);
    dev->reg.init_reg(0x9e, 0x00);
    dev->reg.init_reg(0x9f, 0x00);
    dev->reg.init_reg(0xa0, 0x20);
    dev->reg.init_reg(0xa1, 0x30);
    dev->reg.init_reg(0xa2, 0x00);
    dev->reg.init_reg(0xa3, 0x20);
    dev->reg.init_reg(0xa4, 0x01);
    dev->reg.init_reg(0xa5, 0x00);
    dev->reg.init_reg(0xa6, 0x00);
    dev->reg.init_reg(0xa7, 0x08);
    dev->reg.init_reg(0xa8, 0x00);
    dev->reg.init_reg(0xa9, 0x08);
    dev->reg.init_reg(0xaa, 0x01);
    dev->reg.init_reg(0xab, 0x00);
    dev->reg.init_reg(0xac, 0x00);
    dev->reg.init_reg(0xad, 0x40);
    dev->reg.init_reg(0xae, 0x01);
    dev->reg.init_reg(0xaf, 0x00);
    dev->reg.init_reg(0xb0, 0x00);
    dev->reg.init_reg(0xb1, 0x40);
    dev->reg.init_reg(0xb2, 0x00);
    dev->reg.init_reg(0xb3, 0x09);
    dev->reg.init_reg(0xb4, 0x5b);
    dev->reg.init_reg(0xb5, 0x00);
    dev->reg.init_reg(0xb6, 0x10);
    dev->reg.init_reg(0xb7, 0x3f);
    dev->reg.init_reg(0xb8, 0x00);
    dev->reg.init_reg(0xbb, 0x00);
    dev->reg.init_reg(0xbc, 0xff);
    dev->reg.init_reg(0xbd, 0x00);
    dev->reg.init_reg(0xbe, 0x07);
    dev->reg.init_reg(0xc3, 0x00);
    dev->reg.init_reg(0xc4, 0x00);

    /* gamma
    dev->reg.init_reg(0xc5, 0x00);
    dev->reg.init_reg(0xc6, 0x00);
    dev->reg.init_reg(0xc7, 0x00);
    dev->reg.init_reg(0xc8, 0x00);
    dev->reg.init_reg(0xc9, 0x00);
    dev->reg.init_reg(0xca, 0x00);
    dev->reg.init_reg(0xcb, 0x00);
    dev->reg.init_reg(0xcc, 0x00);
    dev->reg.init_reg(0xcd, 0x00);
    dev->reg.init_reg(0xce, 0x00);
     */

    if (dev->model->sensor_id == SensorId::CIS_CANON_LIDE_120) {
        dev->reg.init_reg(0xc5, 0x20);
        dev->reg.init_reg(0xc6, 0xeb);
        dev->reg.init_reg(0xc7, 0x20);
        dev->reg.init_reg(0xc8, 0xeb);
        dev->reg.init_reg(0xc9, 0x20);
        dev->reg.init_reg(0xca, 0xeb);
    }

    // memory layout
    /*
    dev->reg.init_reg(0xd0, 0x0a);
    dev->reg.init_reg(0xd1, 0x1f);
    dev->reg.init_reg(0xd2, 0x34);
    */
    dev->reg.init_reg(0xd3, 0x00);
    dev->reg.init_reg(0xd4, 0x00);
    dev->reg.init_reg(0xd5, 0x00);
    dev->reg.init_reg(0xd6, 0x00);
    dev->reg.init_reg(0xd7, 0x00);
    dev->reg.init_reg(0xd8, 0x00);
    dev->reg.init_reg(0xd9, 0x00);

    // memory layout
    /*
    dev->reg.init_reg(0xe0, 0x00);
    dev->reg.init_reg(0xe1, 0x48);
    dev->reg.init_reg(0xe2, 0x15);
    dev->reg.init_reg(0xe3, 0x90);
    dev->reg.init_reg(0xe4, 0x15);
    dev->reg.init_reg(0xe5, 0x91);
    dev->reg.init_reg(0xe6, 0x2a);
    dev->reg.init_reg(0xe7, 0xd9);
    dev->reg.init_reg(0xe8, 0x2a);
    dev->reg.init_reg(0xe9, 0xad);
    dev->reg.init_reg(0xea, 0x40);
    dev->reg.init_reg(0xeb, 0x22);
    dev->reg.init_reg(0xec, 0x40);
    dev->reg.init_reg(0xed, 0x23);
    dev->reg.init_reg(0xee, 0x55);
    dev->reg.init_reg(0xef, 0x6b);
    dev->reg.init_reg(0xf0, 0x55);
    dev->reg.init_reg(0xf1, 0x6c);
    dev->reg.init_reg(0xf2, 0x6a);
    dev->reg.init_reg(0xf3, 0xb4);
    dev->reg.init_reg(0xf4, 0x6a);
    dev->reg.init_reg(0xf5, 0xb5);
    dev->reg.init_reg(0xf6, 0x7f);
    dev->reg.init_reg(0xf7, 0xfd);
    */

    dev->reg.init_reg(0xf8, 0x01);   // other value is 0x05
    dev->reg.init_reg(0xf9, 0x00);
    dev->reg.init_reg(0xfa, 0x00);
    dev->reg.init_reg(0xfb, 0x00);
    dev->reg.init_reg(0xfc, 0x00);
    dev->reg.init_reg(0xff, 0x00);

    // fine tune upon device description
    const auto& sensor = sanei_genesys_find_sensor_any(dev);
    sanei_genesys_set_dpihw(dev->reg, sensor, sensor.optical_res);

  dev->calib_reg = dev->reg;
}

/**@brief send slope table for motor movement
 * Send slope_table in machine byte order
 * @param dev device to send slope table
 * @param table_nr index of the slope table in ASIC memory
 * Must be in the [0-4] range.
 * @param slope_table pointer to 16 bit values array of the slope table
 * @param steps number of elemnts in the slope table
 */
static void gl124_send_slope_table(Genesys_Device* dev, int table_nr,
                                   const std::vector<uint16_t>& slope_table,
                                   int steps)
{
    DBG_HELPER_ARGS(dbg, "table_nr = %d, steps = %d", table_nr, steps);
  int i;
  char msg[10000];

  /* sanity check */
  if(table_nr<0 || table_nr>4)
    {
        throw SaneException("invalid table number");
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
        for (i = 0; i < steps; i++) {
            std::sprintf(msg + std::strlen(msg), ",%d", slope_table[i]);
        }
      DBG (DBG_io, "%s: %s\n", __func__, msg);
    }

    if (dev->interface->is_mock()) {
        dev->interface->record_slope_table(table_nr, slope_table);
    }
    // slope table addresses are fixed
    dev->interface->write_ahb(0x10000000 + 0x4000 * table_nr, steps * 2, table.data());
}

/** @brief * Set register values of 'special' ti type frontend
 * Registers value are taken from the frontend register data
 * set.
 * @param dev device owning the AFE
 * @param set flag AFE_INIT to specify the AFE must be reset before writing data
 * */
static void gl124_set_ti_fe(Genesys_Device* dev, uint8_t set)
{
    DBG_HELPER(dbg);
  int i;

  if (set == AFE_INIT)
    {
        DBG(DBG_proc, "%s: setting DAC %u\n", __func__,
            static_cast<unsigned>(dev->model->adc_id));

      dev->frontend = dev->frontend_initial;
    }

    // start writing to DAC
    dev->interface->write_fe_register(0x00, 0x80);

  /* write values to analog frontend */
  for (uint16_t addr = 0x01; addr < 0x04; addr++)
    {
        dev->interface->write_fe_register(addr, dev->frontend.regs.get_value(addr));
    }

    dev->interface->write_fe_register(0x04, 0x00);

  /* these are not really sign for this AFE */
  for (i = 0; i < 3; i++)
    {
        dev->interface->write_fe_register(0x05 + i, dev->frontend.regs.get_value(0x24 + i));
    }

    if (dev->model->adc_id == AdcId::CANON_LIDE_120) {
        dev->interface->write_fe_register(0x00, 0x01);
    }
  else
    {
        dev->interface->write_fe_register(0x00, 0x11);
    }
}


// Set values of analog frontend
void CommandSetGl124::set_fe(Genesys_Device* dev, const Genesys_Sensor& sensor, uint8_t set) const
{
    DBG_HELPER_ARGS(dbg, "%s", set == AFE_INIT ? "init" :
                               set == AFE_SET ? "set" :
                               set == AFE_POWER_SAVE ? "powersave" : "huh?");
    (void) sensor;
  uint8_t val;

  if (set == AFE_INIT)
    {
        DBG(DBG_proc, "%s(): setting DAC %u\n", __func__,
            static_cast<unsigned>(dev->model->adc_id));
      dev->frontend = dev->frontend_initial;
    }

    val = dev->interface->read_register(REG_0x0A);

  /* route to correct analog FE */
    switch ((val & REG_0x0A_SIFSEL) >> REG_0x0AS_SIFSEL) {
    case 3:
            gl124_set_ti_fe(dev, set);
      break;
    case 0:
    case 1:
    case 2:
    default:
            throw SaneException("unsupported analog FE 0x%02x", val);
    }
}

static void gl124_init_motor_regs_scan(Genesys_Device* dev,
                                       const Genesys_Sensor& sensor,
                                       Genesys_Register_Set* reg,
                                       const MotorProfile& motor_profile,
                                       unsigned int scan_exposure_time,
                                       unsigned scan_yres,
                                       unsigned int scan_lines,
                                       unsigned int scan_dummy,
                                       unsigned int feed_steps,
                                       ScanColorMode scan_mode,
                                       MotorFlag flags)
{
    DBG_HELPER(dbg);
  int use_fast_fed;
  unsigned int lincnt, fast_dpi;
  unsigned int feedl,dist;
  uint32_t z1, z2;
    unsigned yres;
    unsigned min_speed;
  unsigned int linesel;

    DBG(DBG_info, "%s : scan_exposure_time=%d, scan_yres=%d, step_type=%d, scan_lines=%d, "
      "scan_dummy=%d, feed_steps=%d, scan_mode=%d, flags=%x\n", __func__, scan_exposure_time,
        scan_yres, static_cast<unsigned>(motor_profile.step_type), scan_lines, scan_dummy,
        feed_steps, static_cast<unsigned>(scan_mode),
        static_cast<unsigned>(flags));

  /* we never use fast fed since we do manual feed for the scans */
  use_fast_fed=0;

  /* enforce motor minimal scan speed
   * @TODO extend motor struct for this value */
  if (scan_mode == ScanColorMode::COLOR_SINGLE_PASS)
    {
      min_speed = 900;
    }
  else
    {
      switch(dev->model->motor_id)
        {
          case MotorId::CANON_LIDE_110:
	    min_speed = 600;
            break;
          case MotorId::CANON_LIDE_120:
            min_speed = 900;
            break;
          default:
            min_speed = 900;
            break;
        }
    }

  /* compute min_speed and linesel */
  if(scan_yres<min_speed)
    {
      yres=min_speed;
        linesel = yres / scan_yres - 1;
      /* limit case, we need a linesel > 0 */
      if(linesel==0)
        {
          linesel=1;
          yres=scan_yres*2;
        }
    }
  else
    {
      yres=scan_yres;
      linesel=0;
    }

    DBG(DBG_io2, "%s: final yres=%d, linesel=%d\n", __func__, yres, linesel);

  lincnt=scan_lines*(linesel+1);
    reg->set24(REG_LINCNT, lincnt);
  DBG (DBG_io, "%s: lincnt=%d\n", __func__, lincnt);

  /* compute register 02 value */
    uint8_t r02 = REG_0x02_NOTHOME;

    if (use_fast_fed) {
        r02 |= REG_0x02_FASTFED;
    } else {
        r02 &= ~REG_0x02_FASTFED;
    }

    if (has_flag(flags, MotorFlag::AUTO_GO_HOME)) {
        r02 |= REG_0x02_AGOHOME;
    }

    if (has_flag(flags, MotorFlag::DISABLE_BUFFER_FULL_MOVE) || (yres >= sensor.optical_res))
    {
        r02 |= REG_0x02_ACDCDIS;
    }
    if (has_flag(flags, MotorFlag::REVERSE)) {
        r02 |= REG_0x02_MTRREV;
    }

    reg->set8(REG_0x02, r02);
    sanei_genesys_set_motor_power(*reg, true);

    reg->set16(REG_SCANFED, 4);

  /* scan and backtracking slope table */
    auto scan_table = sanei_genesys_slope_table(dev->model->asic_type, yres, scan_exposure_time,
                                                dev->motor.base_ydpi, 1,
                                                motor_profile);
    gl124_send_slope_table(dev, SCAN_TABLE, scan_table.table, scan_table.steps_count);
    gl124_send_slope_table(dev, BACKTRACK_TABLE, scan_table.table, scan_table.steps_count);

    reg->set16(REG_STEPNO, scan_table.steps_count);

  /* fast table */
  fast_dpi=yres;

  /*
  if (scan_mode != ScanColorMode::COLOR_SINGLE_PASS)
    {
      fast_dpi*=3;
    }
    */
    auto fast_table = sanei_genesys_slope_table(dev->model->asic_type, fast_dpi,
                                                scan_exposure_time, dev->motor.base_ydpi,
                                                1, motor_profile);
    gl124_send_slope_table(dev, STOP_TABLE, fast_table.table, fast_table.steps_count);
    gl124_send_slope_table(dev, FAST_TABLE, fast_table.table, fast_table.steps_count);

    reg->set16(REG_FASTNO, fast_table.steps_count);
    reg->set16(REG_FSHDEC, fast_table.steps_count);
    reg->set16(REG_FMOVNO, fast_table.steps_count);

  /* substract acceleration distance from feedl */
  feedl=feed_steps;
    feedl <<= static_cast<unsigned>(motor_profile.step_type);

    dist = scan_table.steps_count;
    if (has_flag(flags, MotorFlag::FEED)) {
        dist *= 2;
    }
    if (use_fast_fed) {
        dist += fast_table.steps_count * 2;
    }
  DBG (DBG_io2, "%s: acceleration distance=%d\n", __func__, dist);

  /* get sure we don't use insane value */
    if (dist < feedl) {
        feedl -= dist;
    } else {
        feedl = 0;
    }

    reg->set24(REG_FEEDL, feedl);
  DBG (DBG_io, "%s: feedl=%d\n", __func__, feedl);

  /* doesn't seem to matter that much */
    sanei_genesys_calculate_zmod(use_fast_fed,
				  scan_exposure_time,
                                 scan_table.table,
                                 scan_table.steps_count,
				  feedl,
                                 scan_table.steps_count,
                                  &z1,
                                  &z2);

    reg->set24(REG_Z1MOD, z1);
  DBG(DBG_info, "%s: z1 = %d\n", __func__, z1);

    reg->set24(REG_Z2MOD, z2);
  DBG(DBG_info, "%s: z2 = %d\n", __func__, z2);

  /* LINESEL */
    reg->set8_mask(REG_0x1D, linesel, REG_0x1D_LINESEL);
    reg->set8(REG_0xA0, (static_cast<unsigned>(motor_profile.step_type) << REG_0xA0S_STEPSEL) |
                        (static_cast<unsigned>(motor_profile.step_type) << REG_0xA0S_FSTPSEL));

    reg->set16(REG_FMOVDEC, fast_table.steps_count);
}


/** @brief copy sensor specific settings
 * Set up register set for the given sensor resolution. Values are from the device table
 * in genesys_devices.c for registers:
 *       [0x16 ... 0x1d]
 *       [0x52 ... 0x5e]
 * Other come from the specific device sensor table in genesys_gl124.h:
 *      0x18, 0x20, 0x61, 0x98 and
 * @param dev device to set up
 * @param regs register set to modify
 * @param dpi resolution of the sensor during scan
 * @param ccd_size_divisor flag for half ccd mode
 * */
static void gl124_setup_sensor(Genesys_Device* dev, const Genesys_Sensor& sensor,
                               Genesys_Register_Set* regs)
{
    DBG_HELPER(dbg);

    for (const auto& reg : sensor.custom_regs) {
        regs->set8(reg.address, reg.value);
    }

    regs->set24(REG_EXPR, sensor.exposure.red);
    regs->set24(REG_EXPG, sensor.exposure.green);
    regs->set24(REG_EXPB, sensor.exposure.blue);

    dev->segment_order = sensor.segment_order;
}

/** @brief setup optical related registers
 * start and pixels are expressed in optical sensor resolution coordinate
 * space.
 * @param dev scanner device to use
 * @param reg registers to set up
 * @param exposure_time exposure time to use
 * @param used_res scanning resolution used, may differ from
 *        scan's one
 * @param start logical start pixel coordinate
 * @param pixels logical number of pixels to use
 * @param channels number of color channels (currently 1 or 3)
 * @param depth bit depth of the scan (1, 8 or 16)
 * @param ccd_size_divisor whether sensor's timings are such that x coordinates must be halved
 * @param color_filter color channel to use as gray data
 * @param flags optical flags (@see )
 */
static void gl124_init_optical_regs_scan(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                         Genesys_Register_Set* reg, unsigned int exposure_time,
                                         const ScanSession& session)
{
    DBG_HELPER_ARGS(dbg, "exposure_time=%d", exposure_time);
    unsigned int dpihw;
  GenesysRegister *r;
  uint32_t expmax;

    // resolution is divided according to ccd_pixels_per_system_pixel
    unsigned ccd_pixels_per_system_pixel = sensor.ccd_pixels_per_system_pixel();
    DBG(DBG_io2, "%s: ccd_pixels_per_system_pixel=%d\n", __func__, ccd_pixels_per_system_pixel);

    // to manage high resolution device while keeping good low resolution scanning speed, we
    // make hardware dpi vary
    dpihw = sensor.get_register_hwdpi(session.output_resolution * ccd_pixels_per_system_pixel);
    DBG(DBG_io2, "%s: dpihw=%d\n", __func__, dpihw);

    gl124_setup_sensor(dev, sensor, reg);

    dev->cmd_set->set_fe(dev, sensor, AFE_SET);

  /* enable shading */
    regs_set_optical_off(dev->model->asic_type, *reg);
    r = sanei_genesys_get_address (reg, REG_0x01);
    if (has_flag(session.params.flags, ScanFlag::DISABLE_SHADING) ||
        has_flag(dev->model->flags, ModelFlag::NO_CALIBRATION))
    {
        r->value &= ~REG_0x01_DVDSET;
    } else {
        r->value |= REG_0x01_DVDSET;
    }

    r = sanei_genesys_get_address(reg, REG_0x03);
    if ((dev->model->sensor_id != SensorId::CIS_CANON_LIDE_120) && (session.params.xres>=600)) {
        r->value &= ~REG_0x03_AVEENB;
      DBG (DBG_io, "%s: disabling AVEENB\n", __func__);
    }
  else
    {
        r->value |= ~REG_0x03_AVEENB;
      DBG (DBG_io, "%s: enabling AVEENB\n", __func__);
    }

    sanei_genesys_set_lamp_power(dev, sensor, *reg,
                                 !has_flag(session.params.flags, ScanFlag::DISABLE_LAMP));

    // BW threshold
    dev->interface->write_register(REG_0x114, dev->settings.threshold);
    dev->interface->write_register(REG_0x115, dev->settings.threshold);

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

    r->value &= ~REG_0x04_FILTER;
  if (session.params.channels == 1)
    {
      switch (session.params.color_filter)
	{
            case ColorFilter::RED:
                r->value |= 0x10;
                break;
            case ColorFilter::BLUE:
                r->value |= 0x30;
                break;
            case ColorFilter::GREEN:
                r->value |= 0x20;
                break;
            default:
                break; // should not happen
	}
    }

    sanei_genesys_set_dpihw(*reg, sensor, dpihw);

    if (should_enable_gamma(session, sensor)) {
        reg->find_reg(REG_0x05).value |= REG_0x05_GMMENB;
    } else {
        reg->find_reg(REG_0x05).value &= ~REG_0x05_GMMENB;
    }

    unsigned dpiset_reg = session.output_resolution * ccd_pixels_per_system_pixel *
            session.ccd_size_divisor;
    if (sensor.dpiset_override != 0) {
        dpiset_reg = sensor.dpiset_override;
    }

    reg->set16(REG_DPISET, dpiset_reg);
    DBG (DBG_io2, "%s: dpiset used=%d\n", __func__, dpiset_reg);

    r = sanei_genesys_get_address(reg, REG_0x06);
    r->value |= REG_0x06_GAIN4;

  /* CIS scanners can do true gray by setting LEDADD */
  /* we set up LEDADD only when asked */
    if (dev->model->is_cis) {
        r = sanei_genesys_get_address (reg, REG_0x60);
        r->value &= ~REG_0x60_LEDADD;
        if (session.enable_ledadd) {
            r->value |= REG_0x60_LEDADD;
            expmax = reg->get24(REG_EXPR);
            expmax = std::max(expmax, reg->get24(REG_EXPG));
            expmax = std::max(expmax, reg->get24(REG_EXPB));

            dev->reg.set24(REG_EXPR, expmax);
            dev->reg.set24(REG_EXPG, expmax);
            dev->reg.set24(REG_EXPB, expmax);
        }
      /* RGB weighting, REG_TRUER,G and B are to be set  */
        r = sanei_genesys_get_address (reg, 0x01);
        r->value &= ~REG_0x01_TRUEGRAY;
        if (session.enable_ledadd) {
            r->value |= REG_0x01_TRUEGRAY;
            dev->interface->write_register(REG_TRUER, 0x80);
            dev->interface->write_register(REG_TRUEG, 0x80);
            dev->interface->write_register(REG_TRUEB, 0x80);
        }
    }

    reg->set24(REG_STRPIXEL, session.pixel_startx);
    reg->set24(REG_ENDPIXEL, session.pixel_endx);

  dev->line_count = 0;

    build_image_pipeline(dev, session);

    // MAXWD is expressed in 2 words unit

    // BUG: we shouldn't multiply by channels here
    reg->set24(REG_MAXWD, session.output_line_bytes_raw / session.ccd_size_divisor * session.params.channels);

    reg->set24(REG_LPERIOD, exposure_time);
  DBG (DBG_io2, "%s: exposure_time used=%d\n", __func__, exposure_time);

    reg->set16(REG_DUMMY, sensor.dummy_pixel);
}

void CommandSetGl124::init_regs_for_scan_session(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                                 Genesys_Register_Set* reg,
                                                 const ScanSession& session) const
{
    DBG_HELPER(dbg);
    session.assert_computed();

  int move;
  int exposure_time;

  int dummy = 0;
  int slope_dpi = 0;

    /* cis color scan is effectively a gray scan with 3 gray lines per color line and a FILTER of 0 */
    if (dev->model->is_cis) {
        slope_dpi = session.params.yres * session.params.channels;
    } else {
        slope_dpi = session.params.yres;
    }

    if (has_flag(session.params.flags, ScanFlag::FEEDING)) {
        exposure_time = 2304;
    } else {
        exposure_time = sensor.exposure_lperiod;
    }
    const auto& motor_profile = get_motor_profile(dev->motor.profiles, exposure_time, session);

  DBG(DBG_info, "%s : exposure_time=%d pixels\n", __func__, exposure_time);
  DBG(DBG_info, "%s : scan_step_type=%d\n", __func__, static_cast<unsigned>(motor_profile.step_type));

  /* we enable true gray for cis scanners only, and just when doing
   * scan since color calibration is OK for this mode
   */

    // now _LOGICAL_ optical values used are known, setup registers
    gl124_init_optical_regs_scan(dev, sensor, reg, exposure_time, session);

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
    gl124_init_motor_regs_scan(dev, sensor, reg, motor_profile, exposure_time, slope_dpi,
                               session.optical_line_count,
                               dummy, move, session.params.scan_mode, mflags);

  /*** prepares data reordering ***/

    dev->read_buffer.clear();
    dev->read_buffer.alloc(session.buffer_size_read);

    dev->read_active = true;

    dev->session = session;

    dev->total_bytes_read = 0;
    dev->total_bytes_to_read = session.output_line_bytes_requested * session.params.lines;

    DBG(DBG_info, "%s: total bytes to send to frontend = %zu\n", __func__,
        dev->total_bytes_to_read);
}

ScanSession CommandSetGl124::calculate_scan_session(const Genesys_Device* dev,
                                                    const Genesys_Sensor& sensor,
                                                    const Genesys_Settings& settings) const
{
  int start;

    DBG(DBG_info, "%s ", __func__);
    debug_dump(DBG_info, settings);

  /* start */
    start = static_cast<int>(dev->model->x_offset);
    start += static_cast<int>(settings.tl_x);
    start = static_cast<int>((start * sensor.optical_res) / MM_PER_INCH);

    ScanSession session;
    session.params.xres = settings.xres;
    session.params.yres = settings.yres;
    session.params.startx = start;
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

/**
 * for fast power saving methods only, like disabling certain amplifiers
 * @param dev device to use
 * @param enable true to set inot powersaving
 * */
void CommandSetGl124::save_power(Genesys_Device* dev, bool enable) const
{
    (void) dev;
    DBG_HELPER_ARGS(dbg, "enable = %d", enable);
}

void CommandSetGl124::set_powersaving(Genesys_Device* dev, int delay /* in minutes */) const
{
    DBG_HELPER_ARGS(dbg,  "delay = %d",  delay);
  GenesysRegister *r;

    r = sanei_genesys_get_address(&dev->reg, REG_0x03);
  r->value &= ~0xf0;
  if(delay<15)
    {
      r->value |= delay;
    }
  else
    {
      r->value |= 0x0f;
    }
}

/** @brief setup GPIOs for scan
 * Setup GPIO values to drive motor (or light) needed for the
 * target resolution
 * @param *dev device to set up
 * @param resolution dpi of the target scan
 */
void gl124_setup_scan_gpio(Genesys_Device* dev, int resolution)
{
    DBG_HELPER(dbg);

    uint8_t val = dev->interface->read_register(REG_0x32);

  /* LiDE 110, 210 and 220 cases */
    if(dev->model->gpio_id != GpioId::CANON_LIDE_120) {
      if(resolution>=dev->motor.base_ydpi/2)
	{
	  val &= 0xf7;
	}
      else if(resolution>=dev->motor.base_ydpi/4)
	{
	  val &= 0xef;
	}
      else
	{
	  val |= 0x10;
	}
    }
  /* 120 : <=300 => 0x53 */
  else
    { /* base_ydpi is 4800 */
      if(resolution<=300)
	{
	  val &= 0xf7;
	}
      else if(resolution<=600)
	{
	  val |= 0x08;
	}
      else if(resolution<=1200)
	{
	  val &= 0xef;
	  val |= 0x08;
	}
      else
	{
	  val &= 0xf7;
	}
    }
  val |= 0x02;
    dev->interface->write_register(REG_0x32, val);
}

// Send the low-level scan command
// todo: is this that useful ?
void CommandSetGl124::begin_scan(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                 Genesys_Register_Set* reg, bool start_motor) const
{
    DBG_HELPER(dbg);
    (void) sensor;
    (void) reg;

    // set up GPIO for scan
    gl124_setup_scan_gpio(dev,dev->settings.yres);

    // clear scan and feed count
    dev->interface->write_register(REG_0x0D, REG_0x0D_CLRLNCNT | REG_0x0D_CLRMCNT);

    // enable scan and motor
    uint8_t val = dev->interface->read_register(REG_0x01);
    val |= REG_0x01_SCAN;
    dev->interface->write_register(REG_0x01, val);

    scanner_start_action(*dev, start_motor);

    dev->advance_head_pos_by_session(ScanHeadId::PRIMARY);
}


// Send the stop scan command
void CommandSetGl124::end_scan(Genesys_Device* dev, Genesys_Register_Set* reg,
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
void CommandSetGl124::move_back_home(Genesys_Device* dev, bool wait_until_home) const
{
    scanner_move_back_home(*dev, wait_until_home);
}

// Automatically set top-left edge of the scan area by scanning a 200x200 pixels area at 600 dpi
// from very top of scanner
void CommandSetGl124::search_start_position(Genesys_Device* dev) const
{
    DBG_HELPER(dbg);
  Genesys_Register_Set local_reg = dev->reg;

  int pixels = 600;
  int dpi = 300;

  /* sets for a 200 lines * 600 pixels */
  /* normal scan with no shading */

    // FIXME: the current approach of doing search only for one resolution does not work on scanners
    // whith employ different sensors with potentially different settings.
    const auto& sensor = sanei_genesys_find_sensor(dev, dpi, 1, ScanMethod::FLATBED);

    ScanSession session;
    session.params.xres = dpi;
    session.params.yres = dpi;
    session.params.startx = 0;
    session.params.starty = 0;        /*we should give a small offset here~60 steps */
    session.params.pixels = 600;
    session.params.lines = dev->model->search_lines;
    session.params.depth = 8;
    session.params.channels = 1;
    session.params.scan_method = dev->settings.scan_method;
    session.params.scan_mode = ScanColorMode::GRAY;
    session.params.color_filter = ColorFilter::GREEN;
    session.params.flags = ScanFlag::DISABLE_SHADING |
                           ScanFlag::DISABLE_GAMMA |
                           ScanFlag::IGNORE_LINE_DISTANCE |
                           ScanFlag::DISABLE_BUFFER_FULL_MOVE;
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
        sanei_genesys_write_pnm_file("gl124_search_position.pnm", image);
    }

    end_scan(dev, &local_reg, true);

  /* update regs to copy ASIC internal state */
  dev->reg = local_reg;

    for (auto& sensor_update :
             sanei_genesys_find_sensors_all_for_write(dev, dev->model->default_method))
    {
        sanei_genesys_search_reference_point(dev, sensor_update, image.get_row_ptr(0), 0, dpi, pixels,
                                             dev->model->search_lines);
    }
}

// sets up register for coarse gain calibration
// todo: check it for scanners using it
void CommandSetGl124::init_regs_for_coarse_calibration(Genesys_Device* dev,
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
                           ScanFlag::FEEDING |
                           ScanFlag::IGNORE_LINE_DISTANCE;
    compute_session(dev, session, sensor);

    init_regs_for_scan_session(dev, sensor, &regs, session);

  sanei_genesys_set_motor_power(regs, false);

  DBG(DBG_info, "%s: optical sensor res: %d dpi, actual res: %d\n", __func__,
      sensor.optical_res / sensor.ccd_pixels_per_system_pixel(), dev->settings.xres);

    dev->interface->write_registers(regs);
}


// init registers for shading calibration shading calibration is done at dpihw
void CommandSetGl124::init_regs_for_shading(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                            Genesys_Register_Set& regs) const
{
    DBG_HELPER(dbg);
  int move, resolution, dpihw, factor;

  /* initial calibration reg values */
  regs = dev->reg;

  dev->calib_channels = 3;
  dev->calib_lines = dev->model->shading_lines;
    dpihw = sensor.get_register_hwdpi(dev->settings.xres);
  if(dpihw>=2400)
    {
      dev->calib_lines *= 2;
    }
  resolution=dpihw;

    unsigned ccd_size_divisor = sensor.get_ccd_size_divisor_for_dpi(dev->settings.xres);

    resolution /= ccd_size_divisor;
    dev->calib_lines /= ccd_size_divisor; // reducing just because we reduced the resolution

    const auto& calib_sensor = sanei_genesys_find_sensor(dev, resolution,
                                                         dev->calib_channels,
                                                         dev->settings.scan_method);
  dev->calib_resolution = resolution;
  dev->calib_total_bytes_to_read = 0;
    factor = calib_sensor.optical_res / resolution;
    dev->calib_pixels = calib_sensor.sensor_pixels / factor;

  /* distance to move to reach white target at high resolution */
  move=0;
    if (dev->settings.yres >= 1200) {
        move = static_cast<int>(dev->model->y_offset_calib_white);
        move = static_cast<int>((move * (dev->motor.base_ydpi/4)) / MM_PER_INCH);
    }
  DBG (DBG_io, "%s: move=%d steps\n", __func__, move);

    ScanSession session;
    session.params.xres = resolution;
    session.params.yres = resolution;
    session.params.startx = 0;
    session.params.starty = move;
    session.params.pixels = dev->calib_pixels;
    session.params.lines = dev->calib_lines;
    session.params.depth = 16;
    session.params.channels = dev->calib_channels;
    session.params.scan_method = dev->settings.scan_method;
    session.params.scan_mode = ScanColorMode::COLOR_SINGLE_PASS;
    session.params.color_filter = ColorFilter::RED;
    session.params.flags = ScanFlag::DISABLE_SHADING |
                           ScanFlag::DISABLE_GAMMA |
                           ScanFlag::DISABLE_BUFFER_FULL_MOVE |
                           ScanFlag::IGNORE_LINE_DISTANCE;
    compute_session(dev, session, calib_sensor);

    try {
        init_regs_for_scan_session(dev, calib_sensor, &regs, session);
    } catch (...) {
        catch_all_exceptions(__func__, [&](){ sanei_genesys_set_motor_power(regs, false); });
        throw;
    }
    sanei_genesys_set_motor_power(regs, false);
}

void CommandSetGl124::wait_for_motor_stop(Genesys_Device* dev) const
{
    DBG_HELPER(dbg);

    auto status = scanner_read_status(*dev);
    uint8_t val40 = dev->interface->read_register(REG_0x100);

    if (!status.is_motor_enabled && (val40 & REG_0x100_MOTMFLG) == 0) {
        return;
    }

    do {
        dev->interface->sleep_ms(10);
        status = scanner_read_status(*dev);
        val40 = dev->interface->read_register(REG_0x100);
    } while (status.is_motor_enabled ||(val40 & REG_0x100_MOTMFLG));
    dev->interface->sleep_ms(50);
}

/** @brief set up registers for the actual scan
 */
void CommandSetGl124::init_regs_for_scan(Genesys_Device* dev, const Genesys_Sensor& sensor) const
{
    DBG_HELPER(dbg);
  float move;
  int move_dpi;
  float start;

    debug_dump(DBG_info, dev->settings);

  /* y (motor) distance to move to reach scanned area */
  move_dpi = dev->motor.base_ydpi/4;
    move = static_cast<float>(dev->model->y_offset);
    move += static_cast<float>(dev->settings.tl_y);
    move = static_cast<float>((move * move_dpi) / MM_PER_INCH);
  DBG (DBG_info, "%s: move=%f steps\n", __func__, move);

    if (dev->settings.get_channels() * dev->settings.yres >= 600 && move > 700) {
        scanner_move(*dev, dev->model->default_method, static_cast<unsigned>(move - 500),
                     Direction::FORWARD);
      move=500;
    }
  DBG(DBG_info, "%s: move=%f steps\n", __func__, move);

  /* start */
    start = static_cast<float>(dev->model->x_offset);
    start += static_cast<float>(dev->settings.tl_x);
    start /= sensor.get_ccd_size_divisor_for_dpi(dev->settings.xres);
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
    session.params.flags = ScanFlag::NONE;
    compute_session(dev, session, sensor);

    init_regs_for_scan_session(dev, sensor, &dev->reg, session);
}

/**
 * Send shading calibration data. The buffer is considered to always hold values
 * for all the channels.
 */
void CommandSetGl124::send_shading_data(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                        std::uint8_t* data, int size) const
{
    DBG_HELPER_ARGS(dbg, "writing %d bytes of shading data", size);
    uint32_t addr, length, x, factor, segcnt, pixels, i;
  uint16_t dpiset,dpihw;
    uint8_t *ptr, *src;

  /* logical size of a color as seen by generic code of the frontend */
    length = size / 3;
    std::uint32_t strpixel = dev->session.pixel_startx;
    std::uint32_t endpixel = dev->session.pixel_endx;
    segcnt = dev->reg.get24(REG_SEGCNT);
  if(endpixel==0)
    {
      endpixel=segcnt;
    }

  /* compute deletion factor */
    dpiset = dev->reg.get16(REG_DPISET);
    dpihw = sensor.get_register_hwdpi(dpiset);
  factor=dpihw/dpiset;
  DBG( DBG_io2, "%s: factor=%d\n",__func__,factor);

  /* turn pixel value into bytes 2x16 bits words */
  strpixel*=2*2; /* 2 words of 2 bytes */
  endpixel*=2*2;
  segcnt*=2*2;
  pixels=endpixel-strpixel;

    dev->interface->record_key_value("shading_start_pixel", std::to_string(strpixel));
    dev->interface->record_key_value("shading_pixels", std::to_string(pixels));
    dev->interface->record_key_value("shading_length", std::to_string(length));
    dev->interface->record_key_value("shading_factor", std::to_string(factor));
    dev->interface->record_key_value("shading_segcnt", std::to_string(segcnt));
    dev->interface->record_key_value("shading_segment_count",
                                     std::to_string(dev->session.segment_count));

  DBG( DBG_io2, "%s: using chunks of %d bytes (%d shading data pixels)\n",__func__,length, length/4);
    std::vector<uint8_t> buffer(pixels * dev->session.segment_count, 0);

  /* write actual red data */
  for(i=0;i<3;i++)
    {
      /* copy data to work buffer and process it */
          /* coefficent destination */
      ptr = buffer.data();

      /* iterate on both sensor segment */
      for(x=0;x<pixels;x+=4*factor)
        {
          /* coefficient source */
          src=data+x+strpixel+i*length;

          /* iterate over all the segments */
            switch (dev->session.segment_count) {
            case 1:
              ptr[0+pixels*0]=src[0+segcnt*0];
              ptr[1+pixels*0]=src[1+segcnt*0];
              ptr[2+pixels*0]=src[2+segcnt*0];
              ptr[3+pixels*0]=src[3+segcnt*0];
              break;
            case 2:
              ptr[0+pixels*0]=src[0+segcnt*0];
              ptr[1+pixels*0]=src[1+segcnt*0];
              ptr[2+pixels*0]=src[2+segcnt*0];
              ptr[3+pixels*0]=src[3+segcnt*0];
              ptr[0+pixels*1]=src[0+segcnt*1];
              ptr[1+pixels*1]=src[1+segcnt*1];
              ptr[2+pixels*1]=src[2+segcnt*1];
              ptr[3+pixels*1]=src[3+segcnt*1];
              break;
            case 4:
              ptr[0+pixels*0]=src[0+segcnt*0];
              ptr[1+pixels*0]=src[1+segcnt*0];
              ptr[2+pixels*0]=src[2+segcnt*0];
              ptr[3+pixels*0]=src[3+segcnt*0];
              ptr[0+pixels*1]=src[0+segcnt*2];
              ptr[1+pixels*1]=src[1+segcnt*2];
              ptr[2+pixels*1]=src[2+segcnt*2];
              ptr[3+pixels*1]=src[3+segcnt*2];
              ptr[0+pixels*2]=src[0+segcnt*1];
              ptr[1+pixels*2]=src[1+segcnt*1];
              ptr[2+pixels*2]=src[2+segcnt*1];
              ptr[3+pixels*2]=src[3+segcnt*1];
              ptr[0+pixels*3]=src[0+segcnt*3];
              ptr[1+pixels*3]=src[1+segcnt*3];
              ptr[2+pixels*3]=src[2+segcnt*3];
              ptr[3+pixels*3]=src[3+segcnt*3];
              break;
            }

          /* next shading coefficient */
          ptr+=4;
        }
        uint8_t val = dev->interface->read_register(0xd0+i);
      addr = val * 8192 + 0x10000000;
        dev->interface->write_ahb(addr, pixels * dev->session.segment_count, buffer.data());
    }
}


/** @brief move to calibration area
 * This functions moves scanning head to calibration area
 * by doing a 600 dpi scan
 * @param dev scanner device
 */
static void move_to_calibration_area(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                     Genesys_Register_Set& regs)
{
    (void) sensor;

    DBG_HELPER(dbg);
  int pixels;

    unsigned resolution = 600;
    unsigned channels = 3;
    const auto& move_sensor = sanei_genesys_find_sensor(dev, resolution, channels,
                                                         dev->settings.scan_method);
    pixels = (move_sensor.sensor_pixels * 600) / move_sensor.optical_res;

  /* initial calibration reg values */
  regs = dev->reg;

    ScanSession session;
    session.params.xres = resolution;
    session.params.yres = resolution;
    session.params.startx = 0;
    session.params.starty = 0;
    session.params.pixels = pixels;
    session.params.lines = 1;
    session.params.depth = 8;
    session.params.channels = channels;
    session.params.scan_method = dev->settings.scan_method;
    session.params.scan_mode = ScanColorMode::COLOR_SINGLE_PASS;
    session.params.color_filter = dev->settings.color_filter;
    session.params.flags = ScanFlag::DISABLE_SHADING |
                           ScanFlag::DISABLE_GAMMA |
                           ScanFlag::SINGLE_LINE |
                           ScanFlag::IGNORE_LINE_DISTANCE;
    compute_session(dev, session, move_sensor);

    dev->cmd_set->init_regs_for_scan_session(dev, move_sensor, &regs, session);

    // write registers and scan data
    dev->interface->write_registers(regs);

  DBG (DBG_info, "%s: starting line reading\n", __func__);
    dev->cmd_set->begin_scan(dev, move_sensor, &regs, true);

    if (is_testing_mode()) {
        dev->interface->test_checkpoint("move_to_calibration_area");
        scanner_stop_action(*dev);
        return;
    }

    auto image = read_unshuffled_image_from_scanner(dev, session, session.output_line_bytes);

    // stop scanning
    scanner_stop_action(*dev);

    if (DBG_LEVEL >= DBG_data) {
        sanei_genesys_write_pnm_file("gl124_movetocalarea.pnm", image);
    }
}

/* this function does the led calibration by scanning one line of the calibration
   area below scanner's top on white strip.

-needs working coarse/gain
*/
SensorExposure CommandSetGl124::led_calibration(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                                Genesys_Register_Set& regs) const
{
    DBG_HELPER(dbg);
  int num_pixels;
  int resolution;
  int dpihw;
    int i;
  int avg[3];
  int turn;
  uint16_t exp[3],target;

  /* move to calibration area */
  move_to_calibration_area(dev, sensor, regs);

  /* offset calibration is always done in 16 bit depth color mode */
    unsigned channels = 3;
    dpihw = sensor.get_register_hwdpi(dev->settings.xres);
    resolution = dpihw;
    unsigned ccd_size_divisor = sensor.get_ccd_size_divisor_for_dpi(dev->settings.xres);
    resolution /= ccd_size_divisor;

    const auto& calib_sensor = sanei_genesys_find_sensor(dev, resolution, channels,
                                                         dev->settings.scan_method);
    num_pixels = (calib_sensor.sensor_pixels * resolution) / calib_sensor.optical_res;

  /* initial calibration reg values */
  regs = dev->reg;

    ScanSession session;
    session.params.xres = resolution;
    session.params.yres = resolution;
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

    // initial loop values and boundaries
    exp[0] = calib_sensor.exposure.red;
    exp[1] = calib_sensor.exposure.green;
    exp[2] = calib_sensor.exposure.blue;
  target=sensor.gain_white_ref*256;

  turn = 0;

  /* no move during led calibration */
  sanei_genesys_set_motor_power(regs, false);
    bool acceptable = false;
  do
    {
        // set up exposure
        regs.set24(REG_EXPR, exp[0]);
        regs.set24(REG_EXPG, exp[1]);
        regs.set24(REG_EXPB, exp[2]);

        // write registers and scan data
        dev->interface->write_registers(regs);

      DBG(DBG_info, "%s: starting line reading\n", __func__);
        begin_scan(dev, calib_sensor, &regs, true);

        if (is_testing_mode()) {
            dev->interface->test_checkpoint("led_calibration");
            scanner_stop_action(*dev);
            return calib_sensor.exposure;
        }

        auto image = read_unshuffled_image_from_scanner(dev, session, session.output_line_bytes);

        // stop scanning
        scanner_stop_action(*dev);

      if (DBG_LEVEL >= DBG_data)
	{
          char fn[30];
          std::snprintf(fn, 30, "gl124_led_%02d.pnm", turn);
            sanei_genesys_write_pnm_file(fn, image);
        }

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
          /* we accept +- 2% delta from target */
          if(abs(avg[i]-target)>target/50)
            {
                float prev_weight = 0.5;
                exp[i] = exp[i] * prev_weight + ((exp[i] * target) / avg[i]) * (1 - prev_weight);
                acceptable = false;
            }
        }

      turn++;
    }
  while (!acceptable && turn < 100);

  DBG(DBG_info, "%s: acceptable exposure: %d,%d,%d\n", __func__, exp[0], exp[1], exp[2]);

    // set these values as final ones for scan
    dev->reg.set24(REG_EXPR, exp[0]);
    dev->reg.set24(REG_EXPG, exp[1]);
    dev->reg.set24(REG_EXPB, exp[2]);

    return { exp[0], exp[1], exp[2] };
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


void CommandSetGl124::offset_calibration(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                         Genesys_Register_Set& regs) const
{
    DBG_HELPER(dbg);
    unsigned channels;
    int pass = 0, avg;
    int topavg, bottomavg, lines;
  int top, bottom, black_pixels, pixels;

    // no gain nor offset for TI AFE
    uint8_t reg0a = dev->interface->read_register(REG_0x0A);
    if (((reg0a & REG_0x0A_SIFSEL) >> REG_0x0AS_SIFSEL) == 3) {
      return;
    }

  /* offset calibration is always done in color mode */
  channels = 3;
  dev->calib_pixels = sensor.sensor_pixels;
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

    auto first_line = read_unshuffled_image_from_scanner(dev, session, session.output_line_bytes);

  if (DBG_LEVEL >= DBG_data)
   {
      char title[30];
        std::snprintf(title, 30, "gl124_offset%03d.pnm", bottom);
        sanei_genesys_write_pnm_file(title, first_line);
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

    auto second_line = read_unshuffled_image_from_scanner(dev, session, session.output_line_bytes);

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
        second_line = read_unshuffled_image_from_scanner(dev, session, session.output_line_bytes);

        if (DBG_LEVEL >= DBG_data) {
          char title[30];
          std::snprintf(title, 30, "gl124_offset%03d.pnm", dev->frontend.get_offset(1));
            sanei_genesys_write_pnm_file(title, second_line);
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


/* alternative coarse gain calibration
   this on uses the settings from offset_calibration and
   uses only one scanline
 */
/*
  with offset and coarse calibration we only want to get our input range into
  a reasonable shape. the fine calibration of the upper and lower bounds will
  be done with shading.
 */
void CommandSetGl124::coarse_gain_calibration(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                              Genesys_Register_Set& regs, int dpi) const
{
    DBG_HELPER_ARGS(dbg, "dpi = %d", dpi);
  int pixels;
  float gain[3],coeff;
    int code, lines;

    // no gain nor offset for TI AFE
    uint8_t reg0a = dev->interface->read_register(REG_0x0A);
    if (((reg0a & REG_0x0A_SIFSEL) >> REG_0x0AS_SIFSEL) == 3) {
      return;
    }

  /* coarse gain calibration is always done in color mode */
    unsigned channels = 3;

  if(dev->settings.xres<sensor.optical_res)
    {
        coeff = 0.9f;
    } else {
        coeff = 1.0f;
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

    std::vector<uint8_t> line(session.output_line_bytes);

    set_fe(dev, sensor, AFE_SET);
    begin_scan(dev, sensor, &regs, true);

    if (is_testing_mode()) {
        dev->interface->test_checkpoint("coarse_gain_calibration");
        scanner_stop_action(*dev);
        move_back_home(dev, true);
        return;
    }

    // BUG: we probably want to read whole image, not just first line
    auto image = read_unshuffled_image_from_scanner(dev, session, session.output_line_bytes);

    if (DBG_LEVEL >= DBG_data) {
        sanei_genesys_write_pnm_file("gl124_gain.pnm", image);
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
            static_cast<unsigned>(total),
            gain[ch], dev->frontend.get_gain(ch));
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

    scanner_stop_action(*dev);

    move_back_home(dev, true);
}

// wait for lamp warmup by scanning the same line until difference
// between 2 scans is below a threshold
void CommandSetGl124::init_regs_for_warmup(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                           Genesys_Register_Set* reg, int* channels,
                                           int* total_size) const
{
    DBG_HELPER(dbg);
  int num_pixels;

  *channels=3;

  *reg = dev->reg;

    ScanSession session;
    session.params.xres = sensor.optical_res;
    session.params.yres = dev->motor.base_ydpi;
    session.params.startx = sensor.sensor_pixels / 4;
    session.params.starty = 0;
    session.params.pixels = sensor.sensor_pixels / 2;
    session.params.lines = 1;
    session.params.depth = 8;
    session.params.channels = *channels;
    session.params.scan_method = dev->settings.scan_method;
    session.params.scan_mode = ScanColorMode::COLOR_SINGLE_PASS;
    session.params.color_filter = dev->settings.color_filter;
    session.params.flags = ScanFlag::DISABLE_SHADING |
                           ScanFlag::DISABLE_GAMMA |
                           ScanFlag::SINGLE_LINE |
                           ScanFlag::IGNORE_LINE_DISTANCE;
    compute_session(dev, session, sensor);

    init_regs_for_scan_session(dev, sensor, reg, session);

    num_pixels = session.output_pixels;

  *total_size = num_pixels * 3 * 1;        /* colors * bytes_per_color * scan lines */

  sanei_genesys_set_motor_power(*reg, false);
    dev->interface->write_registers(*reg);
}

/** @brief default GPIO values
 * set up GPIO/GPOE for idle state
 * @param dev device to set up
 */
static void gl124_init_gpio(Genesys_Device* dev)
{
    DBG_HELPER(dbg);
  int idx;

  /* per model GPIO layout */
    if (dev->model->model_id == ModelId::CANON_LIDE_110) {
      idx = 0;
    } else if (dev->model->model_id == ModelId::CANON_LIDE_120) {
      idx = 2;
    }
  else
    {                                /* canon LiDE 210 and 220 case */
      idx = 1;
    }

    dev->interface->write_register(REG_0x31, gpios[idx].r31);
    dev->interface->write_register(REG_0x32, gpios[idx].r32);
    dev->interface->write_register(REG_0x33, gpios[idx].r33);
    dev->interface->write_register(REG_0x34, gpios[idx].r34);
    dev->interface->write_register(REG_0x35, gpios[idx].r35);
    dev->interface->write_register(REG_0x36, gpios[idx].r36);
    dev->interface->write_register(REG_0x38, gpios[idx].r38);
}

/**
 * set memory layout by filling values in dedicated registers
 */
static void gl124_init_memory_layout(Genesys_Device* dev)
{
    DBG_HELPER(dbg);
  int idx = 0;

  /* point to per model memory layout */
    if (dev->model->model_id == ModelId::CANON_LIDE_110 ||
        dev->model->model_id == ModelId::CANON_LIDE_120)
    {
      idx = 0;
    }
  else
    {                                /* canon LiDE 210 and 220 case */
      idx = 1;
    }

  /* setup base address for shading data. */
  /* values must be multiplied by 8192=0x4000 to give address on AHB */
  /* R-Channel shading bank0 address setting for CIS */
    dev->interface->write_register(0xd0, layouts[idx].rd0);
  /* G-Channel shading bank0 address setting for CIS */
    dev->interface->write_register(0xd1, layouts[idx].rd1);
  /* B-Channel shading bank0 address setting for CIS */
    dev->interface->write_register(0xd2, layouts[idx].rd2);

  /* setup base address for scanned data. */
  /* values must be multiplied by 1024*2=0x0800 to give address on AHB */
  /* R-Channel ODD image buffer 0x0124->0x92000 */
  /* size for each buffer is 0x16d*1k word */
    dev->interface->write_register(0xe0, layouts[idx].re0);
    dev->interface->write_register(0xe1, layouts[idx].re1);
  /* R-Channel ODD image buffer end-address 0x0291->0x148800 => size=0xB6800*/
    dev->interface->write_register(0xe2, layouts[idx].re2);
    dev->interface->write_register(0xe3, layouts[idx].re3);

  /* R-Channel EVEN image buffer 0x0292 */
    dev->interface->write_register(0xe4, layouts[idx].re4);
    dev->interface->write_register(0xe5, layouts[idx].re5);
  /* R-Channel EVEN image buffer end-address 0x03ff*/
    dev->interface->write_register(0xe6, layouts[idx].re6);
    dev->interface->write_register(0xe7, layouts[idx].re7);

  /* same for green, since CIS, same addresses */
    dev->interface->write_register(0xe8, layouts[idx].re0);
    dev->interface->write_register(0xe9, layouts[idx].re1);
    dev->interface->write_register(0xea, layouts[idx].re2);
    dev->interface->write_register(0xeb, layouts[idx].re3);
    dev->interface->write_register(0xec, layouts[idx].re4);
    dev->interface->write_register(0xed, layouts[idx].re5);
    dev->interface->write_register(0xee, layouts[idx].re6);
    dev->interface->write_register(0xef, layouts[idx].re7);

/* same for blue, since CIS, same addresses */
    dev->interface->write_register(0xf0, layouts[idx].re0);
    dev->interface->write_register(0xf1, layouts[idx].re1);
    dev->interface->write_register(0xf2, layouts[idx].re2);
    dev->interface->write_register(0xf3, layouts[idx].re3);
    dev->interface->write_register(0xf4, layouts[idx].re4);
    dev->interface->write_register(0xf5, layouts[idx].re5);
    dev->interface->write_register(0xf6, layouts[idx].re6);
    dev->interface->write_register(0xf7, layouts[idx].re7);
}

/**
 * initialize backend and ASIC : registers, motor tables, and gamma tables
 * then ensure scanner's head is at home
 */
void CommandSetGl124::init(Genesys_Device* dev) const
{
  DBG_INIT ();
    DBG_HELPER(dbg);

    sanei_genesys_asic_init(dev, 0);
}


/* *
 * initialize ASIC from power on condition
 */
void CommandSetGl124::asic_boot(Genesys_Device* dev, bool cold) const
{
    DBG_HELPER(dbg);

    // reset ASIC in case of cold boot
    if (cold) {
        dev->interface->write_register(0x0e, 0x01);
        dev->interface->write_register(0x0e, 0x00);
    }

    // enable GPOE 17
    dev->interface->write_register(0x36, 0x01);

    // set GPIO 17
    uint8_t val = dev->interface->read_register(0x33);
    val |= 0x01;
    dev->interface->write_register(0x33, val);

    // test CHKVER
    val = dev->interface->read_register(REG_0x100);
    if (val & REG_0x100_CHKVER) {
        val = dev->interface->read_register(0x00);
        DBG(DBG_info, "%s: reported version for genesys chip is 0x%02x\n", __func__, val);
    }

  /* Set default values for registers */
  gl124_init_registers (dev);

    // Write initial registers
    dev->interface->write_registers(dev->reg);

    // tune reg 0B
    dev->interface->write_register(REG_0x0B, REG_0x0B_30MHZ | REG_0x0B_ENBDRAM | REG_0x0B_64M);
  dev->reg.remove_reg(0x0b);

    //set up end access
    dev->interface->write_0x8c(0x10, 0x0b);
    dev->interface->write_0x8c(0x13, 0x0e);

  /* CIS_LINE */
    dev->reg.init_reg(0x08, REG_0x08_CIS_LINE);
    dev->interface->write_register(0x08, dev->reg.find_reg(0x08).value);

    // setup gpio
    gl124_init_gpio(dev);

    // setup internal memory layout
    gl124_init_memory_layout(dev);
}


void CommandSetGl124::update_hardware_sensors(Genesys_Scanner* s) const
{
  /* do what is needed to get a new set of events, but try to not loose
     any of them.
   */
    DBG_HELPER(dbg);
    uint8_t val = s->dev->interface->read_register(REG_0x31);

  /* TODO : for the next scanner special case,
   * add another per scanner button profile struct to avoid growing
   * hard-coded button mapping here.
   */
    if ((s->dev->model->gpio_id == GpioId::CANON_LIDE_110) ||
        (s->dev->model->gpio_id == GpioId::CANON_LIDE_120))
    {
        s->buttons[BUTTON_SCAN_SW].write((val & 0x01) == 0);
        s->buttons[BUTTON_FILE_SW].write((val & 0x08) == 0);
        s->buttons[BUTTON_EMAIL_SW].write((val & 0x04) == 0);
        s->buttons[BUTTON_COPY_SW].write((val & 0x02) == 0);
    }
  else
    { /* LiDE 210 case */
        s->buttons[BUTTON_EXTRA_SW].write((val & 0x01) == 0);
        s->buttons[BUTTON_SCAN_SW].write((val & 0x02) == 0);
        s->buttons[BUTTON_COPY_SW].write((val & 0x04) == 0);
        s->buttons[BUTTON_EMAIL_SW].write((val & 0x08) == 0);
        s->buttons[BUTTON_FILE_SW].write((val & 0x10) == 0);
    }
}

void CommandSetGl124::update_home_sensor_gpio(Genesys_Device& dev) const
{
    DBG_HELPER(dbg);

    std::uint8_t val = dev.interface->read_register(REG_0x32);
    val &= ~REG_0x32_GPIO10;
    dev.interface->write_register(REG_0x32, val);
}

bool CommandSetGl124::needs_home_before_init_regs_for_scan(Genesys_Device* dev) const
{
    (void) dev;
    return true;
}

void CommandSetGl124::send_gamma_table(Genesys_Device* dev, const Genesys_Sensor& sensor) const
{
    sanei_genesys_send_gamma_table(dev, sensor);
}

void CommandSetGl124::load_document(Genesys_Device* dev) const
{
    (void) dev;
    throw SaneException("not implemented");
}

void CommandSetGl124::detect_document_end(Genesys_Device* dev) const
{
    (void) dev;
    throw SaneException("not implemented");
}

void CommandSetGl124::eject_document(Genesys_Device* dev) const
{
    (void) dev;
    throw SaneException("not implemented");
}

void CommandSetGl124::search_strip(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                   bool forward, bool black) const
{
    (void) dev;
    (void) sensor;
    (void) forward;
    (void) black;
    throw SaneException("not implemented");
}

void CommandSetGl124::move_to_ta(Genesys_Device* dev) const
{
    (void) dev;
    throw SaneException("not implemented");
}

std::unique_ptr<CommandSet> create_gl124_cmd_set()
{
    return std::unique_ptr<CommandSet>(new CommandSetGl124{});
}

} // namespace gl124
} // namespace genesys
