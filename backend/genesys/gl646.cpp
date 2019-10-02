/* sane - Scanner Access Now Easy.

   Copyright (C) 2003 Oliver Rauch
   Copyright (C) 2003, 2004 Henning Meier-Geinitz <henning@meier-geinitz.de>
   Copyright (C) 2004 Gerhard Jaeger <gerhard@gjaeger.de>
   Copyright (C) 2004-2013 Stéphane Voltz <stef.dev@free.fr>
   Copyright (C) 2005-2009 Pierre Willenbrock <pierre@pirsoft.dnsalias.org>
   Copyright (C) 2007 Luke <iceyfor@gmail.com>
   Copyright (C) 2011 Alexey Osipov <simba@lerlan.ru> for HP2400 description
                      and tuning

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

#include "gl646.h"
#include "gl646_registers.h"

#include <vector>

/**
 * reads value from gpio endpoint
 */
static void gl646_gpio_read(UsbDevice& usb_dev, uint8_t* value)
{
    DBG_HELPER(dbg);
    usb_dev.control_msg(REQUEST_TYPE_IN, REQUEST_REGISTER, GPIO_READ, INDEX, 1, value);
}

/**
 * writes the given value to gpio endpoint
 */
static void gl646_gpio_write(UsbDevice& usb_dev, uint8_t value)
{
    DBG_HELPER_ARGS(dbg, "(0x%02x)", value);
    usb_dev.control_msg(REQUEST_TYPE_OUT, REQUEST_REGISTER, GPIO_WRITE, INDEX, 1, &value);
}

/**
 * writes the given value to gpio output enable endpoint
 */
static void gl646_gpio_output_enable(UsbDevice& usb_dev, uint8_t value)
{
    DBG_HELPER_ARGS(dbg, "(0x%02x)", value);
    usb_dev.control_msg(REQUEST_TYPE_OUT, REQUEST_REGISTER, GPIO_OUTPUT_ENABLE, INDEX, 1, &value);
}

/* Read bulk data (e.g. scanned data) */
void CommandSetGl646::bulk_read_data(Genesys_Device* dev, uint8_t addr, uint8_t* data,
                                     size_t len) const
{
    DBG_HELPER(dbg);
    sanei_genesys_bulk_read_data(dev, addr, data, len);
    if (dev->model->is_sheetfed) {
        detect_document_end(dev);
    }
}

bool CommandSetGl646::get_gain4_bit(Genesys_Register_Set* regs) const
{
    GenesysRegister *r = sanei_genesys_get_address(regs, 0x06);
    return (r && (r->value & REG06_GAIN4));
}

bool CommandSetGl646::test_buffer_empty_bit(SANE_Byte val) const
{
    return (val & REG41_BUFEMPTY);
}

/**
 * decodes and prints content of status (0x41) register
 * @param val value read from reg41
 */
static void
print_status (uint8_t val)
{
  char msg[80];

  sprintf (msg, "%s%s%s%s%s%s%s%s",
	   val & REG41_PWRBIT ? "PWRBIT " : "",
	   val & REG41_BUFEMPTY ? "BUFEMPTY " : "",
	   val & REG41_FEEDFSH ? "FEEDFSH " : "",
	   val & REG41_SCANFSH ? "SCANFSH " : "",
	   val & REG41_HOMESNR ? "HOMESNR " : "",
	   val & REG41_LAMPSTS ? "LAMPSTS " : "",
	   val & REG41_FEBUSY ? "FEBUSY " : "",
	   val & REG41_MOTMFLG ? "MOTMFLG" : "");
  DBG(DBG_info, "status=%s\n", msg);
}

/**
 * start scanner's motor
 * @param dev scanner's device
 */
static void gl646_start_motor(Genesys_Device* dev)
{
    DBG_HELPER(dbg);
    dev->write_register(0x0f, 0x01);
}


/**
 * stop scanner's motor
 * @param dev scanner's device
 */
static void gl646_stop_motor(Genesys_Device* dev)
{
    DBG_HELPER(dbg);
    dev->write_register(0x0f, 0x00);
}

/**
 * find the lowest resolution for the sensor in the given mode.
 * @param sensor id of the sensor
 * @param channels the channel count
 * @return the minimum resolution for the sensor and mode
 */
static unsigned get_lowest_resolution(SensorId sensor_id, unsigned channels)
{
    unsigned min_res = 9600;
    for (const auto& sensor : *s_sensors) {
        // computes distance and keep mode if it is closer than previous
        if (sensor_id == sensor.sensor_id && sensor.matches_channel_count(channels)) {
            for (auto res : sensor.resolutions.resolutions()) {
                if (res < min_res)
                    min_res = res;
            }
        }
    }

    DBG(DBG_info, "%s: %d\n", __func__, min_res);
    return min_res;
}

/**
 * find the closest match in mode tables for the given resolution and scan mode.
 * @param sensor id of the sensor
 * @param required required resolution
 * @param color true is color mode
 * @return the closest resolution for the sensor and mode
 */
static unsigned get_closest_resolution(SensorId sensor_id, int required, unsigned channels)
{
    unsigned best_res = 0;
    unsigned best_diff = 9600;

    for (const auto& sensor : *s_sensors) {
        if (sensor_id != sensor.sensor_id)
            continue;

        // exit on perfect match
        if (sensor.resolutions.matches(required) && sensor.matches_channel_count(channels)) {
            DBG(DBG_info, "%s: match found for %d\n", __func__, required);
            return required;
        }

        // computes distance and keep mode if it is closer than previous
        if (sensor.matches_channel_count(channels)) {
            for (auto res : sensor.resolutions.resolutions()) {
                unsigned curr_diff = std::abs(static_cast<int>(res) - static_cast<int>(required));
                if (curr_diff < best_diff) {
                    best_res = res;
                    best_diff = curr_diff;
                }
            }
        }
    }

    DBG(DBG_info, "%s: closest match for %d is %d\n", __func__, required, best_res);
    return best_res;
}

/**
 * Returns the cksel values used by the required scan mode.
 * @param sensor id of the sensor
 * @param required required resolution
 * @param color true is color mode
 * @return cksel value for mode
 */
static int get_cksel(SensorId sensor_id, int required, unsigned channels)
{
    for (const auto& sensor : *s_sensors) {
        // exit on perfect match
        if (sensor.sensor_id == sensor_id && sensor.resolutions.matches(required) &&
            sensor.matches_channel_count(channels))
        {
            unsigned cksel = sensor.ccd_pixels_per_system_pixel();
            DBG(DBG_io, "%s: match found for %d (cksel=%d)\n", __func__, required, cksel);
            return cksel;
        }
    }
  DBG(DBG_error, "%s: failed to find match for %d dpi\n", __func__, required);
  /* fail safe fallback */
  return 1;
}

static void gl646_compute_session(Genesys_Device* dev, ScanSession& s,
                                  const Genesys_Sensor& sensor)
{
    DBG_HELPER(dbg);
    (void) dev;

    compute_session(dev, s, sensor);
    s.computed = true;

    DBG(DBG_info, "%s ", __func__);
    debug_dump(DBG_info, s);
}

/**
 * Setup register and motor tables for a scan at the
 * given resolution and color mode. TODO try to not use any filed from
 * the device.
 * @param dev          pointer to a struct describing the device
 * @param regs         register set to fill
 * @param slope_table1 first motor table to fill
 * @param slope_table2 second motor table to fill
 * @note No harcoded SENSOR or MOTOR 'names' should be present and
 * registers are set from settings tables and flags related
 * to the hardware capabilities.
 * */
static void gl646_setup_registers(Genesys_Device* dev,
                                  const Genesys_Sensor& sensor,
                                  Genesys_Register_Set* regs,
                                  const ScanSession& session,
                                  std::vector<uint16_t>& slope_table1,
                                  std::vector<uint16_t>& slope_table2)
{
    DBG_HELPER(dbg);
    session.assert_computed();

    debug_dump(DBG_info, sensor);

    uint32_t move = session.params.starty;

  int i, nb;
  Motor_Master *motor = nullptr;
  unsigned int used1, used2, vfinal;
  uint32_t z1, z2;
  int feedl;


  /* for the given resolution, search for master
   * motor mode setting */
  i = 0;
  nb = sizeof (motor_master) / sizeof (Motor_Master);
  while (i < nb)
    {
      if (dev->model->motor_id == motor_master[i].motor_id
          && motor_master[i].dpi == session.params.yres
          && motor_master[i].channels == session.params.channels)
	{
	  motor = &motor_master[i];
	}
      i++;
    }
  if (motor == nullptr)
    {
        throw SaneException("unable to find settings for motor %d at %d dpi, color=%d",
                            static_cast<unsigned>(dev->model->motor_id),
                            session.params.yres, session.params.channels);
    }

  /* now we can search for the specific sensor settings */
  i = 0;

    // now apply values from settings to registers
    regs->set16(REG_EXPR, sensor.exposure.red);
    regs->set16(REG_EXPG, sensor.exposure.green);
    regs->set16(REG_EXPB, sensor.exposure.blue);

    for (const auto& reg : sensor.custom_regs) {
        regs->set8(reg.address, reg.value);
    }

  /* now generate slope tables : we are not using generate_slope_table3 yet */
  sanei_genesys_generate_slope_table (slope_table1, motor->steps1,
				      motor->steps1 + 1, motor->vend1,
				      motor->vstart1, motor->vend1,
				      motor->steps1, motor->g1, &used1,
				      &vfinal);
  sanei_genesys_generate_slope_table (slope_table2, motor->steps2,
				      motor->steps2 + 1, motor->vend2,
				      motor->vstart2, motor->vend2,
				      motor->steps2, motor->g2, &used2,
				      &vfinal);

  /* R01 */
  /* now setup other registers for final scan (ie with shading enabled) */
  /* watch dog + shading + scan enable */
  regs->find_reg(0x01).value |= REG01_DOGENB | REG01_DVDSET | REG01_SCAN;
    if (dev->model->is_cis) {
        regs->find_reg(0x01).value |= REG01_CISSET;
    } else {
        regs->find_reg(0x01).value &= ~REG01_CISSET;
    }

  /* if device has no calibration, don't enable shading correction */
  if (dev->model->flags & GENESYS_FLAG_NO_CALIBRATION)
    {
      regs->find_reg(0x01).value &= ~REG01_DVDSET;
    }

  regs->find_reg(0x01).value &= ~REG01_FASTMOD;
  if (motor->fastmod)
    regs->find_reg(0x01).value |= REG01_FASTMOD;

  /* R02 */
  /* allow moving when buffer full by default */
    if (!dev->model->is_sheetfed) {
        dev->reg.find_reg(0x02).value &= ~REG02_ACDCDIS;
    } else {
        dev->reg.find_reg(0x02).value |= REG02_ACDCDIS;
    }

  /* setup motor power and direction */
  sanei_genesys_set_motor_power(*regs, true);
  regs->find_reg(0x02).value &= ~REG02_MTRREV;

  /* fastfed enabled (2 motor slope tables) */
  if (motor->fastfed)
    regs->find_reg(0x02).value |= REG02_FASTFED;
  else
    regs->find_reg(0x02).value &= ~REG02_FASTFED;

  /* step type */
  regs->find_reg(0x02).value &= ~REG02_STEPSEL;
  switch (motor->steptype)
    {
    case StepType::FULL:
      break;
    case StepType::HALF:
      regs->find_reg(0x02).value |= 1;
      break;
    case StepType::QUARTER:
      regs->find_reg(0x02).value |= 2;
      break;
    default:
      regs->find_reg(0x02).value |= 3;
      break;
    }

    if (dev->model->is_sheetfed) {
        regs->find_reg(0x02).value &= ~REG02_AGOHOME;
    } else {
        regs->find_reg(0x02).value |= REG02_AGOHOME;
    }

  /* R03 */
  regs->find_reg(0x03).value &= ~REG03_AVEENB;
  /* regs->find_reg(0x03).value |= REG03_AVEENB; */
  regs->find_reg(0x03).value &= ~REG03_LAMPDOG;

  /* select XPA */
  regs->find_reg(0x03).value &= ~REG03_XPASEL;
    if (session.params.flags & SCAN_FLAG_USE_XPA) {
      regs->find_reg(0x03).value |= REG03_XPASEL;
    }
    regs->state.is_xpa_on = session.params.flags & SCAN_FLAG_USE_XPA;

  /* R04 */
  /* monochrome / color scan */
    switch (session.params.depth) {
    case 8:
      regs->find_reg(0x04).value &= ~(REG04_LINEART | REG04_BITSET);
      break;
    case 16:
      regs->find_reg(0x04).value &= ~REG04_LINEART;
      regs->find_reg(0x04).value |= REG04_BITSET;
      break;
    }

    sanei_genesys_set_dpihw(*regs, sensor, sensor.optical_res);

  /* gamma enable for scans */
  if (dev->model->flags & GENESYS_FLAG_14BIT_GAMMA)
    regs->find_reg(0x05).value |= REG05_GMM14BIT;

  regs->find_reg(0x05).value &= ~REG05_GMMENB;

  /* true CIS gray if needed */
    if (dev->model->is_cis && session.params.channels == 1 && dev->settings.true_gray) {
      regs->find_reg(0x05).value |= REG05_LEDADD;
    }
  else
    {
      regs->find_reg(0x05).value &= ~REG05_LEDADD;
    }

  /* HP2400 1200dpi mode tuning */

    if (dev->model->sensor_id == SensorId::CCD_HP2400) {
      /* reset count of dummy lines to zero */
      regs->find_reg(0x1e).value &= ~REG1E_LINESEL;
        if (session.params.xres >= 1200) {
          /* there must be one dummy line */
          regs->find_reg(0x1e).value |= 1 & REG1E_LINESEL;

          /* GPO12 need to be set to zero */
          regs->find_reg(0x66).value &= ~0x20;
        }
        else
        {
          /* set GPO12 back to one */
          regs->find_reg(0x66).value |= 0x20;
        }
    }

  /* motor steps used */
  regs->find_reg(0x21).value = motor->steps1;
  regs->find_reg(0x22).value = motor->fwdbwd;
  regs->find_reg(0x23).value = motor->fwdbwd;
  regs->find_reg(0x24).value = motor->steps1;

  /* CIS scanners read one line per color channel
   * since gray mode use 'add' we also read 3 channels even not in
   * color mode */
    if (dev->model->is_cis) {
        regs->set24(REG_LINCNT, session.output_line_count * 3);
    } else {
        regs->set24(REG_LINCNT, session.output_line_count);
    }

    regs->set16(REG_STRPIXEL, session.pixel_startx);
    regs->set16(REG_ENDPIXEL, session.pixel_endx);

    regs->set24(REG_MAXWD, session.output_line_bytes);

    regs->set16(REG_DPISET, session.output_resolution * session.ccd_size_divisor *
                            sensor.ccd_pixels_per_system_pixel());
    regs->set16(REG_LPERIOD, sensor.exposure_lperiod);

  /* move distance must be adjusted to take into account the extra lines
   * read to reorder data */
  feedl = move;
    if (session.num_staggered_lines + session.max_color_shift_lines > 0 && feedl != 0) {
        int feed_offset = ((session.max_color_shift_lines + session.num_staggered_lines) * dev->motor.optical_ydpi) /
                motor->dpi;
        if (feedl > feed_offset) {
            feedl = feedl - feed_offset;
        }
    }

  /* we assume all scans are done with 2 tables */
  /*
     feedl = feed_steps - fast_slope_steps*2 -
     (slow_slope_steps >> scan_step_type); */
  /* but head has moved due to shading calibration => dev->scanhead_position_in_steps */
  if (feedl > 0)
    {
      /* take into account the distance moved during calibration */
      /* feedl -= dev->scanhead_position_in_steps; */
      DBG(DBG_info, "%s: initial move=%d\n", __func__, feedl);
      DBG(DBG_info, "%s: scanhead_position_in_steps=%d\n", __func__,
          dev->scanhead_position_in_steps);

      /* TODO clean up this when I'll fully understand.
       * for now, special casing each motor */
        switch (dev->model->motor_id) {
            case MotorId::MD_5345:
                    switch (motor->dpi) {
	    case 200:
	      feedl -= 70;
	      break;
	    case 300:
	      feedl -= 70;
	      break;
	    case 400:
	      feedl += 130;
	      break;
	    case 600:
	      feedl += 160;
	      break;
	    case 1200:
	      feedl += 160;
	      break;
	    case 2400:
	      feedl += 180;
	      break;
	    default:
	      break;
	    }
	  break;
            case MotorId::HP2300:
                    switch (motor->dpi) {
	    case 75:
	      feedl -= 180;
	      break;
	    case 150:
	      feedl += 0;
	      break;
	    case 300:
	      feedl += 30;
	      break;
	    case 600:
	      feedl += 35;
	      break;
	    case 1200:
	      feedl += 45;
	      break;
	    default:
	      break;
	    }
	  break;
            case MotorId::HP2400:
                    switch (motor->dpi) {
	    case 150:
	      feedl += 150;
	      break;
	    case 300:
	      feedl += 220;
	      break;
	    case 600:
	      feedl += 260;
	      break;
	    case 1200:
	      feedl += 280; /* 300 */
	      break;
	    case 50:
	      feedl += 0;
	      break;
	    case 100:
	      feedl += 100;
	      break;
	    default:
	      break;
	    }
	  break;

	  /* theorical value */
        default: {
            unsigned step_shift = static_cast<unsigned>(motor->steptype);

	  if (motor->fastfed)
        {
                feedl = feedl - 2 * motor->steps2 - (motor->steps1 >> step_shift);
	    }
	  else
	    {
                feedl = feedl - (motor->steps1 >> step_shift);
	    }
	  break;
        }
	}
      /* security */
      if (feedl < 0)
	feedl = 0;
    }

  DBG(DBG_info, "%s: final move=%d\n", __func__, feedl);
    regs->set24(REG_FEEDL, feedl);

  regs->find_reg(0x65).value = motor->mtrpwm;

    sanei_genesys_calculate_zmod(regs->find_reg(0x02).value & REG02_FASTFED,
                                 sensor.exposure_lperiod,
				  slope_table1,
				  motor->steps1,
                                  move, motor->fwdbwd, &z1, &z2);

  /* no z1/z2 for sheetfed scanners */
    if (dev->model->is_sheetfed) {
      z1 = 0;
      z2 = 0;
    }
    regs->set16(REG_Z1MOD, z1);
    regs->set16(REG_Z2MOD, z2);
  regs->find_reg(0x6b).value = motor->steps2;
  regs->find_reg(0x6c).value =
    (regs->find_reg(0x6c).value & REG6C_TGTIME) | ((z1 >> 13) & 0x38) | ((z2 >> 16)
								   & 0x07);

    write_control(dev, sensor, session.output_resolution);

    // setup analog frontend
    gl646_set_fe(dev, sensor, AFE_SET, session.output_resolution);

    dev->read_buffer.clear();
    dev->read_buffer.alloc(session.buffer_size_read);

    build_image_pipeline(dev, session);

  /* scan bytes to read */
    unsigned cis_channel_multiplier = dev->model->is_cis ? session.params.channels : 1;

    dev->read_active = true;

    dev->session = session;
    dev->current_setup.pixels = session.output_pixels;
    dev->current_setup.lines = session.output_line_count * cis_channel_multiplier;
    dev->current_setup.exposure_time = sensor.exposure_lperiod;
    dev->current_setup.xres = session.output_resolution;
  dev->current_setup.ccd_size_divisor = session.ccd_size_divisor;
    dev->current_setup.stagger = session.num_staggered_lines;
    dev->current_setup.max_shift = session.max_color_shift_lines + session.num_staggered_lines;

    dev->total_bytes_read = 0;
    dev->total_bytes_to_read = session.output_line_bytes_requested * session.params.lines;

    /* select color filter based on settings */
    regs->find_reg(0x04).value &= ~REG04_FILTER;
    if (session.params.channels == 1) {
        switch (session.params.color_filter) {
            case ColorFilter::RED:
                regs->find_reg(0x04).value |= 0x04;
                break;
            case ColorFilter::GREEN:
                regs->find_reg(0x04).value |= 0x08;
                break;
            case ColorFilter::BLUE:
                regs->find_reg(0x04).value |= 0x0c;
                break;
            default:
                break;
        }
    }
}


/** copy sensor specific settings */
/* *dev  : device infos
   *regs : regiters to be set
   extended : do extended set up
   ccd_size_divisor: set up for half ccd resolution
   all registers 08-0B, 10-1D, 52-5E are set up. They shouldn't
   appear anywhere else but in register init
*/
static void
gl646_setup_sensor (Genesys_Device * dev, const Genesys_Sensor& sensor, Genesys_Register_Set * regs)
{
    (void) dev;
    DBG(DBG_proc, "%s: start\n", __func__);

    for (const auto& reg_setting : sensor.custom_base_regs) {
        regs->set8(reg_setting.address, reg_setting.value);
    }
    // FIXME: all other drivers don't set exposure here
    sanei_genesys_set_exposure(*regs, sensor.exposure);

    DBG(DBG_proc, "%s: end\n", __func__);
}

/** Test if the ASIC works
 */
static void gl646_asic_test(Genesys_Device* dev)
{
    DBG_HELPER(dbg);
  size_t size, verify_size;
  unsigned int i;

    // set and read exposure time, compare if it's the same
    dev->write_register(0x38, 0xde);

    dev->write_register(0x39, 0xad);

    uint8_t val = dev->read_register(0x4e);

  if (val != 0xde)		/* value of register 0x38 */
    {
      throw SaneException("register contains invalid value");
    }

    val = dev->read_register(0x4f);

  if (val != 0xad)		/* value of register 0x39 */
    {
      throw SaneException("register contains invalid value");
    }

  /* ram test: */
  size = 0x40000;
  verify_size = size + 0x80;
  /* todo: looks like the read size must be a multiple of 128?
     otherwise the read doesn't succeed the second time after the scanner has
     been plugged in. Very strange. */

  std::vector<uint8_t> data(size);
  std::vector<uint8_t> verify_data(verify_size);

  for (i = 0; i < (size - 1); i += 2)
    {
      data[i] = i / 512;
      data[i + 1] = (i / 2) % 256;
    }

    sanei_genesys_set_buffer_address(dev, 0x0000);
    sanei_genesys_bulk_write_data(dev, 0x3c, data.data(), size);
    sanei_genesys_set_buffer_address(dev, 0x0000);

    dev->cmd_set->bulk_read_data(dev, 0x45, verify_data.data(), verify_size);

  /* i + 2 is needed as the changed address goes into effect only after one
     data word is sent. */
  for (i = 0; i < size; i++)
    {
      if (verify_data[i + 2] != data[i])
	{
            throw SaneException("data verification error");
	}
    }
}

/**
 * Set all registers to default values after init
 * @param dev scannerr's device to set
 */
static void
gl646_init_regs (Genesys_Device * dev)
{
  int addr;

  DBG(DBG_proc, "%s\n", __func__);

    dev->reg.clear();

    for (addr = 1; addr <= 0x0b; addr++)
        dev->reg.init_reg(addr, 0);
    for (addr = 0x10; addr <= 0x29; addr++)
        dev->reg.init_reg(addr, 0);
    for (addr = 0x2c; addr <= 0x39; addr++)
        dev->reg.init_reg(addr, 0);
    for (addr = 0x3d; addr <= 0x3f; addr++)
        dev->reg.init_reg(addr, 0);
    for (addr = 0x52; addr <= 0x5e; addr++)
        dev->reg.init_reg(addr, 0);
    for (addr = 0x60; addr <= 0x6d; addr++)
        dev->reg.init_reg(addr, 0);

  dev->reg.find_reg(0x01).value = 0x20 /*0x22 */ ;	/* enable shading, CCD, color, 1M */
  dev->reg.find_reg(0x02).value = 0x30 /*0x38 */ ;	/* auto home, one-table-move, full step */
    if (dev->model->motor_id == MotorId::MD_5345) {
        dev->reg.find_reg(0x02).value |= 0x01; // half-step
    }
    switch (dev->model->motor_id) {
        case MotorId::MD_5345:
      dev->reg.find_reg(0x02).value |= 0x01;	/* half-step */
      break;
        case MotorId::XP200:
      /* for this sheetfed scanner, no AGOHOME, nor backtracking */
      dev->reg.find_reg(0x02).value = 0x50;
      break;
        default:
      break;
    }
  dev->reg.find_reg(0x03).value = 0x1f /*0x17 */ ;	/* lamp on */
  dev->reg.find_reg(0x04).value = 0x13 /*0x03 */ ;	/* 8 bits data, 16 bits A/D, color, Wolfson fe *//* todo: according to spec, 0x0 is reserved? */
  switch (dev->model->adc_id)
    {
    case AdcId::AD_XP200:
      dev->reg.find_reg(0x04).value = 0x12;
      break;
    default:
      /* Wolfson frontend */
      dev->reg.find_reg(0x04).value = 0x13;
      break;
    }

  const auto& sensor = sanei_genesys_find_sensor_any(dev);

  dev->reg.find_reg(0x05).value = 0x00;	/* 12 bits gamma, disable gamma, 24 clocks/pixel */
    sanei_genesys_set_dpihw(dev->reg, sensor, sensor.optical_res);

    if (dev->model->flags & GENESYS_FLAG_14BIT_GAMMA) {
        dev->reg.find_reg(0x05).value |= REG05_GMM14BIT;
    }
    if (dev->model->adc_id == AdcId::AD_XP200) {
        dev->reg.find_reg(0x05).value |= 0x01;	/* 12 clocks/pixel */
    }

    if (dev->model->sensor_id == SensorId::CCD_HP2300) {
        dev->reg.find_reg(0x06).value = 0x00; // PWRBIT off, shading gain=4, normal AFE image capture
    } else {
        dev->reg.find_reg(0x06).value = 0x18; // PWRBIT on, shading gain=8, normal AFE image capture
    }


  gl646_setup_sensor(dev, sensor, &dev->reg);

  dev->reg.find_reg(0x1e).value = 0xf0;	/* watch-dog time */

  switch (dev->model->sensor_id)
    {
    case SensorId::CCD_HP2300:
      dev->reg.find_reg(0x1e).value = 0xf0;
      dev->reg.find_reg(0x1f).value = 0x10;
      dev->reg.find_reg(0x20).value = 0x20;
      break;
    case SensorId::CCD_HP2400:
      dev->reg.find_reg(0x1e).value = 0x80;
      dev->reg.find_reg(0x1f).value = 0x10;
      dev->reg.find_reg(0x20).value = 0x20;
      break;
    case SensorId::CCD_HP3670:
      dev->reg.find_reg(0x19).value = 0x2a;
      dev->reg.find_reg(0x1e).value = 0x80;
      dev->reg.find_reg(0x1f).value = 0x10;
      dev->reg.find_reg(0x20).value = 0x20;
      break;
    case SensorId::CIS_XP200:
      dev->reg.find_reg(0x1e).value = 0x10;
      dev->reg.find_reg(0x1f).value = 0x01;
      dev->reg.find_reg(0x20).value = 0x50;
      break;
    default:
      dev->reg.find_reg(0x1f).value = 0x01;
      dev->reg.find_reg(0x20).value = 0x50;
      break;
    }

  dev->reg.find_reg(0x21).value = 0x08 /*0x20 */ ;	/* table one steps number for forward slope curve of the acc/dec */
  dev->reg.find_reg(0x22).value = 0x10 /*0x08 */ ;	/* steps number of the forward steps for start/stop */
  dev->reg.find_reg(0x23).value = 0x10 /*0x08 */ ;	/* steps number of the backward steps for start/stop */
  dev->reg.find_reg(0x24).value = 0x08 /*0x20 */ ;	/* table one steps number backward slope curve of the acc/dec */
  dev->reg.find_reg(0x25).value = 0x00;	/* scan line numbers (7000) */
  dev->reg.find_reg(0x26).value = 0x00 /*0x1b */ ;
  dev->reg.find_reg(0x27).value = 0xd4 /*0x58 */ ;
  dev->reg.find_reg(0x28).value = 0x01;	/* PWM duty for lamp control */
  dev->reg.find_reg(0x29).value = 0xff;

  dev->reg.find_reg(0x2c).value = 0x02;	/* set resolution (600 DPI) */
  dev->reg.find_reg(0x2d).value = 0x58;
  dev->reg.find_reg(0x2e).value = 0x78;	/* set black&white threshold high level */
  dev->reg.find_reg(0x2f).value = 0x7f;	/* set black&white threshold low level */

  dev->reg.find_reg(0x30).value = 0x00;	/* begin pixel position (16) */
  dev->reg.find_reg(0x31).value = sensor.dummy_pixel /*0x10 */ ;	/* TGW + 2*TG_SHLD + x  */
  dev->reg.find_reg(0x32).value = 0x2a /*0x15 */ ;	/* end pixel position (5390) */
  dev->reg.find_reg(0x33).value = 0xf8 /*0x0e */ ;	/* TGW + 2*TG_SHLD + y   */
  dev->reg.find_reg(0x34).value = sensor.dummy_pixel;
  dev->reg.find_reg(0x35).value = 0x01 /*0x00 */ ;	/* set maximum word size per line, for buffer full control (10800) */
  dev->reg.find_reg(0x36).value = 0x00 /*0x2a */ ;
  dev->reg.find_reg(0x37).value = 0x00 /*0x30 */ ;
  dev->reg.find_reg(0x38).value = 0x2a; // line period (exposure time = 11000 pixels) */
  dev->reg.find_reg(0x39).value = 0xf8;
  dev->reg.find_reg(0x3d).value = 0x00;	/* set feed steps number of motor move */
  dev->reg.find_reg(0x3e).value = 0x00;
  dev->reg.find_reg(0x3f).value = 0x01 /*0x00 */ ;

  dev->reg.find_reg(0x60).value = 0x00;	/* Z1MOD, 60h:61h:(6D b5:b3), remainder for start/stop */
  dev->reg.find_reg(0x61).value = 0x00;	/* (21h+22h)/LPeriod */
  dev->reg.find_reg(0x62).value = 0x00;	/* Z2MODE, 62h:63h:(6D b2:b0), remainder for start scan */
  dev->reg.find_reg(0x63).value = 0x00;	/* (3Dh+3Eh+3Fh)/LPeriod for one-table mode,(21h+1Fh)/LPeriod */
  dev->reg.find_reg(0x64).value = 0x00;	/* motor PWM frequency */
  dev->reg.find_reg(0x65).value = 0x00;	/* PWM duty cycle for table one motor phase (63 = max) */
    if (dev->model->motor_id == MotorId::MD_5345) {
        // PWM duty cycle for table one motor phase (63 = max)
        dev->reg.find_reg(0x65).value = 0x02;
    }

    for (const auto& reg : dev->gpo.regs) {
        dev->reg.set8(reg.address, reg.value);
    }

    switch (dev->model->motor_id) {
        case MotorId::HP2300:
        case MotorId::HP2400:
      dev->reg.find_reg(0x6a).value = 0x7f;	/* table two steps number for acc/dec */
      dev->reg.find_reg(0x6b).value = 0x78;	/* table two steps number for acc/dec */
      dev->reg.find_reg(0x6d).value = 0x7f;
      break;
        case MotorId::MD_5345:
      dev->reg.find_reg(0x6a).value = 0x42;	/* table two fast moving step type, PWM duty for table two */
      dev->reg.find_reg(0x6b).value = 0xff;	/* table two steps number for acc/dec */
      dev->reg.find_reg(0x6d).value = 0x41;	/* select deceleration steps whenever go home (0), accel/decel stop time (31 * LPeriod) */
      break;
        case MotorId::XP200:
      dev->reg.find_reg(0x6a).value = 0x7f;	/* table two fast moving step type, PWM duty for table two */
      dev->reg.find_reg(0x6b).value = 0x08;	/* table two steps number for acc/dec */
      dev->reg.find_reg(0x6d).value = 0x01;	/* select deceleration steps whenever go home (0), accel/decel stop time (31 * LPeriod) */
      break;
        case MotorId::HP3670:
      dev->reg.find_reg(0x6a).value = 0x41;	/* table two steps number for acc/dec */
      dev->reg.find_reg(0x6b).value = 0xc8;	/* table two steps number for acc/dec */
      dev->reg.find_reg(0x6d).value = 0x7f;
      break;
        default:
      dev->reg.find_reg(0x6a).value = 0x40;	/* table two fast moving step type, PWM duty for table two */
      dev->reg.find_reg(0x6b).value = 0xff;	/* table two steps number for acc/dec */
      dev->reg.find_reg(0x6d).value = 0x01;	/* select deceleration steps whenever go home (0), accel/decel stop time (31 * LPeriod) */
      break;
    }
  dev->reg.find_reg(0x6c).value = 0x00;	/* peroid times for LPeriod, expR,expG,expB, Z1MODE, Z2MODE (one period time) */
}


// Send slope table for motor movement slope_table in machine byte order
static void gl646_send_slope_table(Genesys_Device* dev, int table_nr,
                                   const std::vector<uint16_t>& slope_table,
                                   int steps)
{
    DBG_HELPER_ARGS(dbg, "table_nr = %d, steps = %d)=%d .. %d", table_nr, steps, slope_table[0],
                    slope_table[steps - 1]);
  int dpihw;
  int start_address;

  dpihw = dev->reg.find_reg(0x05).value >> 6;

  if (dpihw == 0)		/* 600 dpi */
    start_address = 0x08000;
  else if (dpihw == 1)		/* 1200 dpi */
    start_address = 0x10000;
  else if (dpihw == 2)		/* 2400 dpi */
    start_address = 0x1f800;
    else {
        throw SaneException("Unexpected dpihw");
    }

  std::vector<uint8_t> table(steps * 2);
  for (int i = 0; i < steps; i++)
    {
      table[i * 2] = slope_table[i] & 0xff;
      table[i * 2 + 1] = slope_table[i] >> 8;
    }

    sanei_genesys_set_buffer_address(dev, start_address + table_nr * 0x100);

    sanei_genesys_bulk_write_data(dev, 0x3c, table.data(), steps * 2);
}

// Set values of Analog Device type frontend
static void gl646_set_ad_fe(Genesys_Device* dev, uint8_t set)
{
    DBG_HELPER(dbg);
  int i;

  if (set == AFE_INIT)
    {
        DBG(DBG_proc, "%s(): setting DAC %u\n", __func__,
            static_cast<unsigned>(dev->model->adc_id));

      dev->frontend = dev->frontend_initial;

        // write them to analog frontend
        sanei_genesys_fe_write_data(dev, 0x00, dev->frontend.regs.get_value(0x00));
        sanei_genesys_fe_write_data(dev, 0x01, dev->frontend.regs.get_value(0x01));
    }
  if (set == AFE_SET)
    {
      for (i = 0; i < 3; i++)
	{
        sanei_genesys_fe_write_data(dev, 0x02 + i, dev->frontend.get_gain(i));
	}
      for (i = 0; i < 3; i++)
	{
        sanei_genesys_fe_write_data(dev, 0x05 + i, dev->frontend.get_offset(i));
	}
    }
  /*
     if (set == AFE_POWER_SAVE)
     {
        sanei_genesys_fe_write_data(dev, 0x00, dev->frontend.reg[0] | 0x04);
     } */
}

/** set up analog frontend
 * set up analog frontend
 * @param dev device to set up
 * @param set action from AFE_SET, AFE_INIT and AFE_POWERSAVE
 * @param dpi resolution of the scan since it affects settings
 */
static void gl646_wm_hp3670(Genesys_Device* dev, const Genesys_Sensor& sensor, uint8_t set, int dpi)
{
    DBG_HELPER(dbg);
  int i;

  switch (set)
    {
    case AFE_INIT:
        sanei_genesys_fe_write_data (dev, 0x04, 0x80);
      sanei_genesys_sleep_ms(200);
    dev->write_register(0x50, 0x00);
      dev->frontend = dev->frontend_initial;
        sanei_genesys_fe_write_data(dev, 0x01, dev->frontend.regs.get_value(0x01));
        sanei_genesys_fe_write_data(dev, 0x02, dev->frontend.regs.get_value(0x02));
        gl646_gpio_output_enable(dev->usb_dev, 0x07);
      break;
    case AFE_POWER_SAVE:
        sanei_genesys_fe_write_data(dev, 0x01, 0x06);
        sanei_genesys_fe_write_data(dev, 0x06, 0x0f);
            return;
      break;
    default:			/* AFE_SET */
      /* mode setup */
      i = dev->frontend.regs.get_value(0x03);
      if (dpi > sensor.optical_res / 2)
	{
      /* fe_reg_0x03 must be 0x12 for 1200 dpi in WOLFSON_HP3670.
       * WOLFSON_HP2400 in 1200 dpi mode works well with
	   * fe_reg_0x03 set to 0x32 or 0x12 but not to 0x02 */
	  i = 0x12;
	}
        sanei_genesys_fe_write_data(dev, 0x03, i);
      /* offset and sign (or msb/lsb ?) */
      for (i = 0; i < 3; i++)
	{
        sanei_genesys_fe_write_data(dev, 0x20 + i, dev->frontend.get_offset(i));
        sanei_genesys_fe_write_data(dev, 0x24 + i,
                                    dev->frontend.regs.get_value(0x24 + i));	/* MSB/LSB ? */
	}

        // gain
        for (i = 0; i < 3; i++) {
            sanei_genesys_fe_write_data(dev, 0x28 + i, dev->frontend.get_gain(i));
        }
    }
}

/** Set values of analog frontend
 * @param dev device to set
 * @param set action to execute
 * @param dpi dpi to setup the AFE
 */
static void gl646_set_fe(Genesys_Device* dev, const Genesys_Sensor& sensor, uint8_t set, int dpi)
{
    DBG_HELPER_ARGS(dbg, "%s,%d", set == AFE_INIT ? "init" :
                                  set == AFE_SET ? "set" :
                                  set == AFE_POWER_SAVE ? "powersave" : "huh?", dpi);
  int i;
  uint8_t val;

  /* Analog Device type frontend */
    uint8_t frontend_type = dev->reg.find_reg(0x04).value & REG04_FESET;
    if (frontend_type == 0x02) {
        gl646_set_ad_fe(dev, set);
        return;
    }

  /* Wolfson type frontend */
    if (frontend_type != 0x03) {
        throw SaneException("unsupported frontend type %d", frontend_type);
    }

  /* per frontend function to keep code clean */
  switch (dev->model->adc_id)
    {
    case AdcId::WOLFSON_HP3670:
    case AdcId::WOLFSON_HP2400:
            gl646_wm_hp3670(dev, sensor, set, dpi);
            return;
    default:
      DBG(DBG_proc, "%s(): using old method\n", __func__);
      break;
    }

  /* initialize analog frontend */
  if (set == AFE_INIT)
    {
        DBG(DBG_proc, "%s(): setting DAC %u\n", __func__,
            static_cast<unsigned>(dev->model->adc_id));
      dev->frontend = dev->frontend_initial;

        // reset only done on init
        sanei_genesys_fe_write_data(dev, 0x04, 0x80);

      /* enable GPIO for some models */
        if (dev->model->sensor_id == SensorId::CCD_HP2300) {
	  val = 0x07;
            gl646_gpio_output_enable(dev->usb_dev, val);
	}
        return;
    }

    // set fontend to power saving mode
    if (set == AFE_POWER_SAVE) {
        sanei_genesys_fe_write_data(dev, 0x01, 0x02);
        return;
    }

  /* here starts AFE_SET */
  /* TODO :  base this test on cfg reg3 or a CCD family flag to be created */
  /* if (dev->model->ccd_type != SensorId::CCD_HP2300
     && dev->model->ccd_type != SensorId::CCD_HP3670
     && dev->model->ccd_type != SensorId::CCD_HP2400) */
  {
        sanei_genesys_fe_write_data(dev, 0x00, dev->frontend.regs.get_value(0x00));
        sanei_genesys_fe_write_data(dev, 0x02, dev->frontend.regs.get_value(0x02));
  }

    // start with reg3
    sanei_genesys_fe_write_data(dev, 0x03, dev->frontend.regs.get_value(0x03));

  switch (dev->model->sensor_id)
    {
    default:
      for (i = 0; i < 3; i++)
	{
        sanei_genesys_fe_write_data(dev, 0x24 + i, dev->frontend.regs.get_value(0x24 + i));
        sanei_genesys_fe_write_data(dev, 0x28 + i, dev->frontend.get_gain(i));
        sanei_genesys_fe_write_data(dev, 0x20 + i, dev->frontend.get_offset(i));
	}
      break;
      /* just can't have it to work ....
         case SensorId::CCD_HP2300:
         case SensorId::CCD_HP2400:
         case SensorId::CCD_HP3670:

        sanei_genesys_fe_write_data(dev, 0x23, dev->frontend.get_offset(1));
        sanei_genesys_fe_write_data(dev, 0x28, dev->frontend.get_gain(1));
         break; */
    }

    // end with reg1
    sanei_genesys_fe_write_data(dev, 0x01, dev->frontend.regs.get_value(0x01));
}

/** Set values of analog frontend
 * this this the public interface, the gl646 as to use one more
 * parameter to work effectively, hence the redirection
 * @param dev device to set
 * @param set action to execute
 */
void CommandSetGl646::set_fe(Genesys_Device* dev, const Genesys_Sensor& sensor, uint8_t set) const
{
    gl646_set_fe(dev, sensor, set, dev->settings.yres);
}

/**
 * enters or leaves power saving mode
 * limited to AFE for now.
 * @param dev scanner's device
 * @param enable true to enable power saving, false to leave it
 */
void CommandSetGl646::save_power(Genesys_Device* dev, bool enable) const
{
    DBG_HELPER_ARGS(dbg, "enable = %d", enable);

  const auto& sensor = sanei_genesys_find_sensor_any(dev);

  if (enable)
    {
        // gl646_set_fe(dev, sensor, AFE_POWER_SAVE);
    }
  else
    {
      gl646_set_fe(dev, sensor, AFE_INIT, 0);
    }
}

void CommandSetGl646::set_powersaving(Genesys_Device* dev, int delay /* in minutes */) const
{
    DBG_HELPER_ARGS(dbg, "delay = %d", delay);
  Genesys_Register_Set local_reg(Genesys_Register_Set::SEQUENTIAL);
  int rate, exposure_time, tgtime, time;

  local_reg.init_reg(0x01, dev->reg.get8(0x01));	// disable fastmode
  local_reg.init_reg(0x03, dev->reg.get8(0x03));        // Lamp power control
  local_reg.init_reg(0x05, dev->reg.get8(0x05) & ~REG05_BASESEL);   // 24 clocks/pixel
  local_reg.init_reg(0x38, 0x00); // line period low
  local_reg.init_reg(0x39, 0x00); //line period high
  local_reg.init_reg(0x6c, 0x00); // period times for LPeriod, expR,expG,expB, Z1MODE, Z2MODE

  if (!delay)
    local_reg.find_reg(0x03).value &= 0xf0;	/* disable lampdog and set lamptime = 0 */
  else if (delay < 20)
    local_reg.find_reg(0x03).value = (local_reg.get8(0x03) & 0xf0) | 0x09;	/* enable lampdog and set lamptime = 1 */
  else
    local_reg.find_reg(0x03).value = (local_reg.get8(0x03) & 0xf0) | 0x0f;	/* enable lampdog and set lamptime = 7 */

  time = delay * 1000 * 60;	/* -> msec */
    exposure_time = static_cast<std::uint32_t>((time * 32000.0 /
                (24.0 * 64.0 * (local_reg.get8(0x03) & REG03_LAMPTIM) *
         1024.0) + 0.5));
  /* 32000 = system clock, 24 = clocks per pixel */
  rate = (exposure_time + 65536) / 65536;
  if (rate > 4)
    {
      rate = 8;
      tgtime = 3;
    }
  else if (rate > 2)
    {
      rate = 4;
      tgtime = 2;
    }
  else if (rate > 1)
    {
      rate = 2;
      tgtime = 1;
    }
  else
    {
      rate = 1;
      tgtime = 0;
    }

  local_reg.find_reg(0x6c).value |= tgtime << 6;
  exposure_time /= rate;

  if (exposure_time > 65535)
    exposure_time = 65535;

  local_reg.find_reg(0x38).value = exposure_time / 256;
  local_reg.find_reg(0x39).value = exposure_time & 255;

    dev->write_registers(local_reg);
}


/**
 * loads document into scanner
 * currently only used by XP200
 * bit2 (0x04) of gpio is paper event (document in/out) on XP200
 * HOMESNR is set if no document in front of sensor, the sequence of events is
 * paper event -> document is in the sheet feeder
 * HOMESNR becomes 0 -> document reach sensor
 * HOMESNR becomes 1 ->document left sensor
 * paper event -> document is out
 */
void CommandSetGl646::load_document(Genesys_Device* dev) const
{
    DBG_HELPER(dbg);

  // FIXME: sequential not really needed in this case
  Genesys_Register_Set regs(Genesys_Register_Set::SEQUENTIAL);
  unsigned int used, vfinal, count;
    std::vector<uint16_t> slope_table;
  uint8_t val;

  /* no need to load document is flatbed scanner */
    if (!dev->model->is_sheetfed) {
      DBG(DBG_proc, "%s: nothing to load\n", __func__);
      DBG(DBG_proc, "%s: end\n", __func__);
      return;
    }

    val = sanei_genesys_get_status(dev);

  /* HOMSNR is set if a document is inserted */
  if ((val & REG41_HOMESNR))
    {
      /* if no document, waits for a paper event to start loading */
      /* with a 60 seconde minutes timeout                        */
      count = 0;
      do
	{
            gl646_gpio_read(dev->usb_dev, &val);

	  DBG(DBG_info, "%s: GPIO=0x%02x\n", __func__, val);
	  if ((val & 0x04) != 0x04)
	    {
              DBG(DBG_warn, "%s: no paper detected\n", __func__);
	    }
          sanei_genesys_sleep_ms(200);
	  count++;
	}
      while (((val & 0x04) != 0x04) && (count < 300));	/* 1 min time out */
      if (count == 300)
	{
        throw SaneException(SANE_STATUS_NO_DOCS, "timeout waiting for document");
    }
    }

  /* set up to fast move before scan then move until document is detected */
  regs.init_reg(0x01, 0x90);

  /* AGOME, 2 slopes motor moving */
  regs.init_reg(0x02, 0x79);

  /* motor feeding steps to 0 */
  regs.init_reg(0x3d, 0);
  regs.init_reg(0x3e, 0);
  regs.init_reg(0x3f, 0);

  /* 50 fast moving steps */
  regs.init_reg(0x6b, 50);

  /* set GPO */
  regs.init_reg(0x66, 0x30);

  /* stesp NO */
  regs.init_reg(0x21, 4);
  regs.init_reg(0x22, 1);
  regs.init_reg(0x23, 1);
  regs.init_reg(0x24, 4);

  /* generate slope table 2 */
  sanei_genesys_generate_slope_table (slope_table,
				      50,
				      51,
				      2400,
				      6000, 2400, 50, 0.25, &used, &vfinal);
    // document loading:
    // send regs
    // start motor
    // wait e1 status to become e0
    gl646_send_slope_table(dev, 1, slope_table, 50);

    dev->write_registers(regs);

    gl646_start_motor(dev);


  count = 0;
  do
    {
        val = sanei_genesys_get_status(dev);
      sanei_genesys_sleep_ms(200);
      count++;
    }
  while ((val & REG41_MOTMFLG) && (count < 300));
  if (count == 300)
    {
      throw SaneException(SANE_STATUS_JAMMED, "can't load document");
    }

  /* when loading OK, document is here */
    dev->document = true;

  /* set up to idle */
  regs.set8(0x02, 0x71);
  regs.set8(0x3f, 1);
  regs.set8(0x6b, 8);
    dev->write_registers(regs);
}

/**
 * detects end of document and adjust current scan
 * to take it into account
 * used by sheetfed scanners
 */
void CommandSetGl646::detect_document_end(Genesys_Device* dev) const
{
    DBG_HELPER(dbg);
  uint8_t val, gpio;
    unsigned int bytes_left;

    // test for document presence
    val = sanei_genesys_get_status(dev);
  if (DBG_LEVEL > DBG_info)
    {
      print_status (val);
    }
    gl646_gpio_read(dev->usb_dev, &gpio);
  DBG(DBG_info, "%s: GPIO=0x%02x\n", __func__, gpio);

  /* detect document event. There one event when the document go in,
   * then another when it leaves */
    if (dev->document && (gpio & 0x04) && (dev->total_bytes_read > 0)) {
      DBG(DBG_info, "%s: no more document\n", __func__);
        dev->document = false;

      /* adjust number of bytes to read:
       * total_bytes_to_read is the number of byte to send to frontend
       * total_bytes_read is the number of bytes sent to frontend
       * read_bytes_left is the number of bytes to read from the scanner
       */
      DBG(DBG_io, "%s: total_bytes_to_read=%zu\n", __func__, dev->total_bytes_to_read);
      DBG(DBG_io, "%s: total_bytes_read   =%zu\n", __func__, dev->total_bytes_read);

        // amount of data available from scanner is what to scan
        sanei_genesys_read_valid_words(dev, &bytes_left);

        unsigned lines_in_buffer = bytes_left / dev->session.output_line_bytes_raw;

        // we add the number of lines needed to read the last part of the document in
        unsigned lines_offset = (dev->model->y_offset * dev->session.params.yres) /
                MM_PER_INCH;

        unsigned remaining_lines = lines_in_buffer + lines_offset;

        bytes_left = remaining_lines * dev->session.output_line_bytes_raw;

        if (bytes_left < dev->get_pipeline_source().remaining_bytes()) {
            dev->get_pipeline_source().set_remaining_bytes(bytes_left);
            dev->total_bytes_to_read = dev->total_bytes_read + bytes_left;
        }
      DBG(DBG_io, "%s: total_bytes_to_read=%zu\n", __func__, dev->total_bytes_to_read);
      DBG(DBG_io, "%s: total_bytes_read   =%zu\n", __func__, dev->total_bytes_read);
    }
}

/**
 * eject document from the feeder
 * currently only used by XP200
 * TODO we currently rely on AGOHOME not being set for sheetfed scanners,
 * maybe check this flag in eject to let the document being eject automaticaly
 */
void CommandSetGl646::eject_document(Genesys_Device* dev) const
{
    DBG_HELPER(dbg);

  // FIXME: SEQUENTIAL not really needed in this case
  Genesys_Register_Set regs((Genesys_Register_Set::SEQUENTIAL));
  unsigned int used, vfinal, count;
    std::vector<uint16_t> slope_table;
  uint8_t gpio, state;

  /* at the end there will be noe more document */
    dev->document = false;

    // first check for document event
    gl646_gpio_read(dev->usb_dev, &gpio);

  DBG(DBG_info, "%s: GPIO=0x%02x\n", __func__, gpio);

    // test status : paper event + HOMESNR -> no more doc ?
    state = sanei_genesys_get_status(dev);

  DBG(DBG_info, "%s: state=0x%02x\n", __func__, state);
  if (DBG_LEVEL > DBG_info)
    {
      print_status (state);
    }

  /* HOMSNR=0 if no document inserted */
  if ((state & REG41_HOMESNR) != 0)
    {
        dev->document = false;
      DBG(DBG_info, "%s: no more document to eject\n", __func__);
      DBG(DBG_proc, "%s: end\n", __func__);
      return;
    }

    // there is a document inserted, eject it
    dev->write_register(0x01, 0xb0);

  /* wait for motor to stop */
  do
    {
      sanei_genesys_sleep_ms(200);
        state = sanei_genesys_get_status(dev);
    }
  while (state & REG41_MOTMFLG);

  /* set up to fast move before scan then move until document is detected */
  regs.init_reg(0x01, 0xb0);

  /* AGOME, 2 slopes motor moving , eject 'backward' */
  regs.init_reg(0x02, 0x5d);

  /* motor feeding steps to 119880 */
  regs.init_reg(0x3d, 1);
  regs.init_reg(0x3e, 0xd4);
  regs.init_reg(0x3f, 0x48);

  /* 60 fast moving steps */
  regs.init_reg(0x6b, 60);

  /* set GPO */
  regs.init_reg(0x66, 0x30);

  /* stesp NO */
  regs.init_reg(0x21, 4);
  regs.init_reg(0x22, 1);
  regs.init_reg(0x23, 1);
  regs.init_reg(0x24, 4);

  /* generate slope table 2 */
  sanei_genesys_generate_slope_table (slope_table,
				      60,
				      61,
				      1600,
				      10000, 1600, 60, 0.25, &used, &vfinal);
    // document eject:
    // send regs
    // start motor
    // wait c1 status to become c8 : HOMESNR and ~MOTFLAG
    gl646_send_slope_table(dev, 1, slope_table, 60);

    dev->write_registers(regs);

    gl646_start_motor(dev);

  /* loop until paper sensor tells paper is out, and till motor is running */
  /* use a 30 timeout */
  count = 0;
  do
    {
        state = sanei_genesys_get_status(dev);

      print_status (state);
      sanei_genesys_sleep_ms(200);
      count++;
    }
  while (((state & REG41_HOMESNR) == 0) && (count < 150));

    // read GPIO on exit
    gl646_gpio_read(dev->usb_dev, &gpio);

  DBG(DBG_info, "%s: GPIO=0x%02x\n", __func__, gpio);
}

// Send the low-level scan command
void CommandSetGl646::begin_scan(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                 Genesys_Register_Set* reg, bool start_motor) const
{
    DBG_HELPER(dbg);
    (void) sensor;
  // FIXME: SEQUENTIAL not really needed in this case
  Genesys_Register_Set local_reg(Genesys_Register_Set::SEQUENTIAL);

    local_reg.init_reg(0x03, reg->get8(0x03));
    local_reg.init_reg(0x01, reg->get8(0x01) | REG01_SCAN);

    if (start_motor) {
        local_reg.init_reg(0x0f, 0x01);
    } else {
        local_reg.init_reg(0x0f, 0x00); // do not start motor yet
    }

    dev->write_registers(local_reg);
}


// Send the stop scan command
static void end_scan_impl(Genesys_Device* dev, Genesys_Register_Set* reg, bool check_stop,
                          bool eject)
{
    DBG_HELPER_ARGS(dbg, "check_stop = %d, eject = %d", check_stop, eject);
  int i = 0;
  uint8_t val, scanfsh = 0;

  /* we need to compute scanfsh before cancelling scan */
    if (dev->model->is_sheetfed) {
        val = sanei_genesys_get_status(dev);

      if (val & REG41_SCANFSH)
	scanfsh = 1;
      if (DBG_LEVEL > DBG_io2)
	{
	  print_status (val);
	}
    }

  /* ends scan */
    val = reg->get8(0x01);
    val &= ~REG01_SCAN;
    reg->set8(0x01, val);
    dev->write_register(0x01, val);

  /* for sheetfed scanners, we may have to eject document */
    if (dev->model->is_sheetfed) {
        if (eject && dev->document)
	{
            dev->cmd_set->eject_document(dev);
	}
      if (check_stop)
	{
	  for (i = 0; i < 30; i++)	/* do not wait longer than wait 3 seconds */
	    {
            val = sanei_genesys_get_status(dev);

	      if (val & REG41_SCANFSH)
		scanfsh = 1;
	      if (DBG_LEVEL > DBG_io2)
		{
		  print_status (val);
		}

	      if (!(val & REG41_MOTMFLG) && (val & REG41_FEEDFSH) && scanfsh)
		{
		  DBG(DBG_proc, "%s: scanfeed finished\n", __func__);
		  break;	/* leave for loop */
		}

              sanei_genesys_sleep_ms(100);
	    }
	}
    }
  else				/* flat bed scanners */
    {
      if (check_stop)
	{
	  for (i = 0; i < 300; i++)	/* do not wait longer than wait 30 seconds */
	    {
            val = sanei_genesys_get_status(dev);

	      if (val & REG41_SCANFSH)
		scanfsh = 1;
	      if (DBG_LEVEL > DBG_io)
		{
		  print_status (val);
		}

	      if (!(val & REG41_MOTMFLG) && (val & REG41_FEEDFSH) && scanfsh)
		{
		  DBG(DBG_proc, "%s: scanfeed finished\n", __func__);
		  break;	/* leave while loop */
		}

	      if ((!(val & REG41_MOTMFLG)) && (val & REG41_HOMESNR))
		{
		  DBG(DBG_proc, "%s: head at home\n", __func__);
		  break;	/* leave while loop */
		}

              sanei_genesys_sleep_ms(100);
	    }
	}
    }

  DBG(DBG_proc, "%s: end (i=%u)\n", __func__, i);
}

// Send the stop scan command
void CommandSetGl646::end_scan(Genesys_Device* dev, Genesys_Register_Set* reg,
                               bool check_stop) const
{
    end_scan_impl(dev, reg, check_stop, false);
}

/**
 * parks head
 * @param dev scanner's device
 * @param wait_until_home true if the function waits until head parked
 */
void CommandSetGl646::slow_back_home(Genesys_Device* dev, bool wait_until_home) const
{
    DBG_HELPER_ARGS(dbg, "wait_until_home = %d\n", wait_until_home);
  Genesys_Settings settings;
  uint8_t val;
  int i;
  int loop = 0;

    val = sanei_genesys_get_status(dev);

  if (DBG_LEVEL > DBG_io)
    {
      print_status (val);
    }

  dev->scanhead_position_in_steps = 0;

  if (val & REG41_HOMESNR)	/* is sensor at home? */
    {
      DBG(DBG_info, "%s: end since already at home\n", __func__);
      return;
    }

  /* stop motor if needed */
  if (val & REG41_MOTMFLG)
    {
        gl646_stop_motor(dev);
      sanei_genesys_sleep_ms(200);
    }

  /* when scanhead is moving then wait until scanhead stops or timeout */
  DBG(DBG_info, "%s: ensuring that motor is off\n", __func__);
  val = REG41_MOTMFLG;
  for (i = 400; i > 0 && (val & REG41_MOTMFLG); i--)	/* do not wait longer than 40 seconds, count down to get i = 0 when busy */
    {
        val = sanei_genesys_get_status(dev);

      if (((val & (REG41_MOTMFLG | REG41_HOMESNR)) == REG41_HOMESNR))	/* at home and motor is off */
	{
	  DBG(DBG_info, "%s: already at home and not moving\n", __func__);
      return;
	}
      sanei_genesys_sleep_ms(100);
    }

  if (!i)			/* the loop counted down to 0, scanner still is busy */
    {
        throw SaneException(SANE_STATUS_DEVICE_BUSY, "motor is still on: device busy");
    }

    // setup for a backward scan of 65535 steps, with no actual data reading
    settings.scan_method = dev->model->default_method;
  settings.scan_mode = ScanColorMode::COLOR_SINGLE_PASS;
  settings.xres = get_lowest_resolution(dev->model->sensor_id, 1);
  settings.yres = settings.xres;
  settings.tl_x = 0;
  settings.tl_y = 0;
  settings.pixels = 600;
    settings.requested_pixels = settings.pixels;
  settings.lines = 1;
  settings.depth = 8;
  settings.color_filter = ColorFilter::RED;

  settings.disable_interpolation = 0;
  settings.threshold = 0;

    const auto& sensor = sanei_genesys_find_sensor(dev, settings.xres, 3,
                                                   dev->model->default_method);

    setup_for_scan(dev, sensor, &dev->reg, settings, true, true, true);

  /* backward , no actual data scanned TODO more setup flags to avoid this register manipulations ? */
    dev->reg.find_reg(0x02).value |= REG02_MTRREV;
    dev->reg.find_reg(0x01).value &= ~REG01_SCAN;
    dev->reg.set24(REG_FEEDL, 65535);

    // sets frontend
    gl646_set_fe(dev, sensor, AFE_SET, settings.xres);

  /* write scan registers */
    try {
        dev->write_registers(dev->reg);
    } catch (...) {
        DBG(DBG_error, "%s: failed to bulk write registers\n", __func__);
    }

  /* registers are restored to an iddl state, give up if no head to park */
    if (dev->model->is_sheetfed) {
      DBG(DBG_proc, "%s: end \n", __func__);
      return;
    }

    // starts scan
    dev->cmd_set->begin_scan(dev, sensor, &dev->reg, true);

  /* loop until head parked */
  if (wait_until_home)
    {
      while (loop < 300)		/* do not wait longer then 30 seconds */
	{
        val = sanei_genesys_get_status(dev);

	  if (val & 0x08)	/* home sensor */
	    {
	      DBG(DBG_info, "%s: reached home position\n", __func__);
	      DBG(DBG_proc, "%s: end\n", __func__);
              sanei_genesys_sleep_ms(500);
            return;
	    }
          sanei_genesys_sleep_ms(100);
          ++loop;
	}

        // when we come here then the scanner needed too much time for this, so we better
        // stop the motor
        catch_all_exceptions(__func__, [&](){ gl646_stop_motor (dev); });
        catch_all_exceptions(__func__, [&](){ end_scan_impl(dev, &dev->reg, true, false); });
        throw SaneException(SANE_STATUS_IO_ERROR, "timeout while waiting for scanhead to go home");
    }


  DBG(DBG_info, "%s: scanhead is still moving\n", __func__);
}

/**
 * Automatically set top-left edge of the scan area by scanning an
 * area at 300 dpi from very top of scanner
 * @param dev  device stucture describing the scanner
 */
void CommandSetGl646::search_start_position(Genesys_Device* dev) const
{
    DBG_HELPER(dbg);
  Genesys_Settings settings;
  unsigned int resolution, x, y;

  /* we scan at 300 dpi */
  resolution = get_closest_resolution(dev->model->sensor_id, 300, 1);

    // FIXME: the current approach of doing search only for one resolution does not work on scanners
    // whith employ different sensors with potentially different settings.
    const auto& sensor = sanei_genesys_find_sensor(dev, resolution, 1,
                                                   dev->model->default_method);

  /* fill settings for a gray level scan */
  settings.scan_method = dev->model->default_method;
  settings.scan_mode = ScanColorMode::GRAY;
  settings.xres = resolution;
  settings.yres = resolution;
  settings.tl_x = 0;
  settings.tl_y = 0;
  settings.pixels = 600;
    settings.requested_pixels = settings.pixels;
  settings.lines = dev->model->search_lines;
  settings.depth = 8;
  settings.color_filter = ColorFilter::RED;

  settings.disable_interpolation = 0;
  settings.threshold = 0;

    // scan the desired area
    std::vector<uint8_t> data;
    simple_scan(dev, sensor, settings, true, true, false, data);

    /* handle stagger case : reorder gray data and thus loose some lines */
    if (dev->current_setup.stagger > 0)
      {
        DBG(DBG_proc, "%s: 'un-staggering'\n", __func__);
        for (y = 0; y < settings.lines - dev->current_setup.stagger; y++)
          {
            /* one point out of 2 is 'unaligned' */
            for (x = 0; x < settings.pixels; x += 2)
        {
          data[y * settings.pixels + x] =
            data[(y + dev->current_setup.stagger) * settings.pixels +
                 x];
        }
          }
        /* correct line number */
        settings.lines -= dev->current_setup.stagger;
      }
    if (DBG_LEVEL >= DBG_data)
      {
        sanei_genesys_write_pnm_file("gl646_search_position.pnm", data.data(), settings.depth, 1,
                                     settings.pixels, settings.lines);
      }

    // now search reference points on the data
    for (auto& sensor_update :
            sanei_genesys_find_sensors_all_for_write(dev, dev->model->default_method))
    {
        sanei_genesys_search_reference_point(dev, sensor_update, data.data(), 0,
                                             resolution, settings.pixels, settings.lines);
    }
}

/**
 * internally overriden during effective calibration
 * sets up register for coarse gain calibration
 */
void CommandSetGl646::init_regs_for_coarse_calibration(Genesys_Device* dev,
                                                       const Genesys_Sensor& sensor,
                                                       Genesys_Register_Set& regs) const
{
    DBG_HELPER(dbg);
    (void) dev;
    (void) sensor;
    (void) regs;
}


/**
 * init registers for shading calibration
 * we assume that scanner's head is on an area suiting shading calibration.
 * We scan a full scan width area by the shading line number for the device
 * at either at full sensor's resolution or half depending upon ccd_size_divisor
 * @param dev scanner's device
 */
void CommandSetGl646::init_regs_for_shading(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                            Genesys_Register_Set& regs) const
{
    DBG_HELPER(dbg);
    (void) regs;
  Genesys_Settings settings;
  int cksel = 1;

  /* fill settings for scan : always a color scan */
  int channels = 3;

    const auto& calib_sensor = sanei_genesys_find_sensor(dev, dev->settings.xres, channels,
                                                         dev->settings.scan_method);

    unsigned ccd_size_divisor = calib_sensor.get_ccd_size_divisor_for_dpi(dev->settings.xres);

  settings.scan_method = dev->settings.scan_method;
  settings.scan_mode = dev->settings.scan_mode;
    if (!dev->model->is_cis) {
      // FIXME: always a color scan, but why don't we set scan_mode to COLOR_SINGLE_PASS always?
      settings.scan_mode = ScanColorMode::COLOR_SINGLE_PASS;
    }
  settings.xres = sensor.optical_res / ccd_size_divisor;
  cksel = get_cksel(dev->model->sensor_id, dev->settings.xres, channels);
  settings.xres = settings.xres / cksel;
  settings.yres = settings.xres;
  settings.tl_x = 0;
  settings.tl_y = 0;
    settings.pixels = (calib_sensor.sensor_pixels * settings.xres) / calib_sensor.optical_res;
    settings.requested_pixels = settings.pixels;
  dev->calib_lines = dev->model->shading_lines;
  settings.lines = dev->calib_lines * (3 - ccd_size_divisor);
  settings.depth = 16;
  settings.color_filter = dev->settings.color_filter;

  settings.disable_interpolation = dev->settings.disable_interpolation;
  settings.threshold = dev->settings.threshold;

  /* keep account of the movement for final scan move */
  dev->scanhead_position_in_steps += settings.lines;

    // we don't want top offset, but we need right margin to be the same than the one for the final
    // scan
    setup_for_scan(dev, calib_sensor, &dev->reg, settings, true, false, false);

  /* used when sending shading calibration data */
  dev->calib_pixels = settings.pixels;
    dev->calib_channels = dev->session.params.channels;
    if (!dev->model->is_cis) {
      dev->calib_channels = 3;
    }

  /* no shading */
  dev->reg.find_reg(0x01).value &= ~REG01_DVDSET;
  dev->reg.find_reg(0x02).value |= REG02_ACDCDIS;	/* ease backtracking */
  dev->reg.find_reg(0x02).value &= ~(REG02_FASTFED | REG02_AGOHOME);
  dev->reg.find_reg(0x05).value &= ~REG05_GMMENB;
  sanei_genesys_set_motor_power(dev->reg, false);

  /* TODO another flag to setup regs ? */
  /* enforce needed LINCNT, getting rid of extra lines for color reordering */
    if (!dev->model->is_cis) {
        dev->reg.set24(REG_LINCNT, dev->calib_lines);
    }
  else
    {
        dev->reg.set24(REG_LINCNT, dev->calib_lines * 3);
    }

  /* copy reg to calib_reg */
  dev->calib_reg = dev->reg;

  /* this is an hack to make calibration cache working .... */
  /* if we don't do this, cache will be identified at the shading calibration
   * dpi which is different from calibration one */
  dev->current_setup.xres = dev->settings.xres;
  DBG(DBG_info, "%s:\n\tdev->settings.xres=%d\n\tdev->settings.yres=%d\n", __func__,
      dev->settings.xres, dev->settings.yres);
}

bool CommandSetGl646::needs_home_before_init_regs_for_scan(Genesys_Device* dev) const
{
    return (dev->scanhead_position_in_steps > 0 &&
            dev->settings.scan_method == ScanMethod::FLATBED);
}

/**
 * set up registers for the actual scan. The scan's parameters are given
 * through the device settings. It allocates the scan buffers.
 */
void CommandSetGl646::init_regs_for_scan(Genesys_Device* dev, const Genesys_Sensor& sensor) const
{
    DBG_HELPER(dbg);

    setup_for_scan(dev, sensor, &dev->reg, dev->settings, false, true, true);

  /* gamma is only enabled at final scan time */
  if (dev->settings.depth < 16)
    dev->reg.find_reg(0x05).value |= REG05_GMMENB;
}

/**
 * set up registers for the actual scan. The scan's parameters are given
 * through the device settings. It allocates the scan buffers.
 * @param dev scanner's device
 * @param regs     registers to set up
 * @param settings settings of scan
 * @param split true if move to scan area is split from scan, false is
 *              scan first moves to area
 * @param xcorrection take x geometry correction into account (fixed and detected offsets)
 * @param ycorrection take y geometry correction into account
 */
static void setup_for_scan(Genesys_Device* dev,
                           const Genesys_Sensor& sensor,
                           Genesys_Register_Set*regs,
                           Genesys_Settings settings,
                           bool split,
                           bool xcorrection,
                           bool ycorrection)
{
    DBG_HELPER(dbg);

    debug_dump(DBG_info, dev->settings);

    // compute distance to move
    float move = 0;
    // XXX STEF XXX MD5345 -> optical_ydpi, other base_ydpi => half/full step ? */
    if (!split) {
        if (!dev->model->is_sheetfed) {
            if (ycorrection) {
                move = dev->model->y_offset;
            }

            // add tl_y to base movement
        }
        move += settings.tl_y;

        if (move < 0) {
            DBG(DBG_error, "%s: overriding negative move value %f\n", __func__, move);
            move = 0;
        }
    }
    move = (move * dev->motor.optical_ydpi) / MM_PER_INCH;
    DBG(DBG_info, "%s: move=%f steps\n", __func__, move);

    float start = settings.tl_x;
    if (xcorrection) {
        if (settings.scan_method == ScanMethod::FLATBED) {
            start += dev->model->x_offset;
        } else {
            start += dev->model->x_offset_ta;
        }
    }
    start = (start * sensor.optical_res) / MM_PER_INCH;

    ScanSession session;
    session.params.xres = settings.xres;
    session.params.yres = settings.yres;
    session.params.startx = start;
    session.params.starty = move;
    session.params.pixels = settings.pixels;
    session.params.requested_pixels = settings.requested_pixels;
    session.params.lines = settings.lines;
    session.params.depth = settings.depth;
    session.params.channels = settings.get_channels();
    session.params.scan_method = dev->settings.scan_method;
    session.params.scan_mode = settings.scan_mode;
    session.params.color_filter = settings.color_filter;
    session.params.flags = 0;
    if (settings.scan_method == ScanMethod::TRANSPARENCY) {
        session.params.flags |= SCAN_FLAG_USE_XPA;
    }
    if (xcorrection) {
        session.params.flags |= SCAN_FLAG_USE_XCORRECTION;
    }
    gl646_compute_session(dev, session, sensor);

    std::vector<uint16_t> slope_table0;
    std::vector<uint16_t> slope_table1;

    // set up correct values for scan (gamma and shading enabled)
    gl646_setup_registers(dev, sensor, regs, session, slope_table0, slope_table1);

    // send computed slope tables
    gl646_send_slope_table(dev, 0, slope_table0, regs->get8(0x21));
    gl646_send_slope_table(dev, 1, slope_table1, regs->get8(0x6b));
}

/**
 * this function send gamma table to ASIC
 */
void CommandSetGl646::send_gamma_table(Genesys_Device* dev, const Genesys_Sensor& sensor) const
{
    DBG_HELPER(dbg);
  int size;
  int address;
  int bits;

  /* gamma table size */
  if (dev->model->flags & GENESYS_FLAG_14BIT_GAMMA)
    {
      size = 16384;
      bits = 14;
    }
  else
    {
      size = 4096;
      bits = 12;
    }

  /* allocate temporary gamma tables: 16 bits words, 3 channels */
  std::vector<uint8_t> gamma(size * 2 * 3);

    sanei_genesys_generate_gamma_buffer(dev, sensor, bits, size-1, size, gamma.data());

  /* table address */
  switch (dev->reg.find_reg(0x05).value >> 6)
    {
    case 0:			/* 600 dpi */
      address = 0x09000;
      break;
    case 1:			/* 1200 dpi */
      address = 0x11000;
      break;
    case 2:			/* 2400 dpi */
      address = 0x20000;
      break;
    default:
            throw SaneException("invalid dpi");
    }

    // send address
    sanei_genesys_set_buffer_address(dev, address);

    // send data
    sanei_genesys_bulk_write_data(dev, 0x3c, gamma.data(), size * 2 * 3);
}

/** @brief this function does the led calibration.
 * this function does the led calibration by scanning one line of the calibration
 * area below scanner's top on white strip. The scope of this function is
 * currently limited to the XP200
 */
SensorExposure CommandSetGl646::led_calibration(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                                Genesys_Register_Set& regs) const
{
    DBG_HELPER(dbg);
    (void) regs;
  int total_size;
  unsigned int i, j;
  int val;
  int avg[3], avga, avge;
  int turn;
  uint16_t expr, expg, expb;
  Genesys_Settings settings;
  SANE_Int resolution;

    unsigned channels = dev->settings.get_channels();

  /* get led calibration resolution */
  if (dev->settings.scan_mode == ScanColorMode::COLOR_SINGLE_PASS)
    {
      settings.scan_mode = ScanColorMode::COLOR_SINGLE_PASS;
    }
  else
    {
      settings.scan_mode = ScanColorMode::GRAY;
    }
  resolution = get_closest_resolution(dev->model->sensor_id, sensor.optical_res, channels);

  /* offset calibration is always done in color mode */
    settings.scan_method = dev->model->default_method;
  settings.xres = resolution;
  settings.yres = resolution;
  settings.tl_x = 0;
  settings.tl_y = 0;
    settings.pixels = (sensor.sensor_pixels * resolution) / sensor.optical_res;
    settings.requested_pixels = settings.pixels;
  settings.lines = 1;
  settings.depth = 16;
  settings.color_filter = ColorFilter::RED;

  settings.disable_interpolation = 0;
  settings.threshold = 0;

  /* colors * bytes_per_color * scan lines */
  total_size = settings.pixels * channels * 2 * 1;

  std::vector<uint8_t> line(total_size);

/*
   we try to get equal bright leds here:

   loop:
     average per color
     adjust exposure times
 */
  expr = sensor.exposure.red;
  expg = sensor.exposure.green;
  expb = sensor.exposure.blue;

  turn = 0;

    auto calib_sensor = sensor;

    bool acceptable = false;
    do {
        calib_sensor.exposure.red = expr;
        calib_sensor.exposure.green = expg;
        calib_sensor.exposure.blue = expb;

      DBG(DBG_info, "%s: starting first line reading\n", __func__);

        simple_scan(dev, calib_sensor, settings, false, true, false, line);

      if (DBG_LEVEL >= DBG_data)
	{
          char fn[30];
          snprintf(fn, 30, "gl646_led_%02d.pnm", turn);
          sanei_genesys_write_pnm_file(fn, line.data(), 16, channels, settings.pixels, 1);
	}

        acceptable = true;

      for (j = 0; j < channels; j++)
	{
	  avg[j] = 0;
	  for (i = 0; i < settings.pixels; i++)
	    {
	      if (dev->model->is_cis)
		val =
		  line[i * 2 + j * 2 * settings.pixels + 1] * 256 +
		  line[i * 2 + j * 2 * settings.pixels];
	      else
		val =
		  line[i * 2 * channels + 2 * j + 1] * 256 +
		  line[i * 2 * channels + 2 * j];
	      avg[j] += val;
	    }

	  avg[j] /= settings.pixels;
	}

      DBG(DBG_info, "%s: average: %d,%d,%d\n", __func__, avg[0], avg[1], avg[2]);

        acceptable = true;

      if (!acceptable)
	{
	  avga = (avg[0] + avg[1] + avg[2]) / 3;
	  expr = (expr * avga) / avg[0];
	  expg = (expg * avga) / avg[1];
	  expb = (expb * avga) / avg[2];

	  /* keep exposure time in a working window */
	  avge = (expr + expg + expb) / 3;
	  if (avge > 0x2000)
	    {
	      expr = (expr * 0x2000) / avge;
	      expg = (expg * 0x2000) / avge;
	      expb = (expb * 0x2000) / avge;
	    }
	  if (avge < 0x400)
	    {
	      expr = (expr * 0x400) / avge;
	      expg = (expg * 0x400) / avge;
	      expb = (expb * 0x400) / avge;
	    }
	}

      turn++;

    }
  while (!acceptable && turn < 100);

  DBG(DBG_info,"%s: acceptable exposure: 0x%04x,0x%04x,0x%04x\n", __func__, expr, expg, expb);
    // BUG: we don't store the result of the last iteration to the sensor
    return calib_sensor.exposure;
}

/**
 * average dark pixels of a scan
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


/** @brief calibration for AD frontend devices
 * we do simple scan until all black_pixels are higher than 0,
 * raising offset at each turn.
 */
static void ad_fe_offset_calibration(Genesys_Device* dev, const Genesys_Sensor& sensor)
{
    DBG_HELPER(dbg);
    (void) sensor;

  unsigned int channels;
  int pass = 0;
  SANE_Int resolution;
  Genesys_Settings settings;
  unsigned int x, y, adr, min;
  unsigned int bottom, black_pixels;

  channels = 3;
  resolution = get_closest_resolution(dev->model->sensor_id, sensor.optical_res, channels);
    const auto& calib_sensor = sanei_genesys_find_sensor(dev, resolution, 3, ScanMethod::FLATBED);
    black_pixels = (calib_sensor.black_pixels * resolution) / calib_sensor.optical_res;
  DBG(DBG_io2, "%s: black_pixels=%d\n", __func__, black_pixels);

    settings.scan_method = dev->model->default_method;
  settings.scan_mode = ScanColorMode::COLOR_SINGLE_PASS;
  settings.xres = resolution;
  settings.yres = resolution;
  settings.tl_x = 0;
  settings.tl_y = 0;
    settings.pixels = (calib_sensor.sensor_pixels * resolution) / calib_sensor.optical_res;
    settings.requested_pixels = settings.pixels;
  settings.lines = CALIBRATION_LINES;
  settings.depth = 8;
  settings.color_filter = ColorFilter::RED;

  settings.disable_interpolation = 0;
  settings.threshold = 0;

  /* scan first line of data with no gain */
  dev->frontend.set_gain(0, 0);
  dev->frontend.set_gain(1, 0);
  dev->frontend.set_gain(2, 0);

  std::vector<uint8_t> line;

  /* scan with no move */
  bottom = 1;
  do
    {
      pass++;
      dev->frontend.set_offset(0, bottom);
      dev->frontend.set_offset(1, bottom);
      dev->frontend.set_offset(2, bottom);
        simple_scan(dev, calib_sensor, settings, false, true, false, line);

      if (DBG_LEVEL >= DBG_data)
	{
          char title[30];
          std::snprintf(title, 30, "gl646_offset%03d.pnm", static_cast<int>(bottom));
          sanei_genesys_write_pnm_file (title, line.data(), 8, channels,
					settings.pixels, settings.lines);
	}

      min = 0;
      for (y = 0; y < settings.lines; y++)
	{
	  for (x = 0; x < black_pixels; x++)
	    {
	      adr = (x + y * settings.pixels) * channels;
	      if (line[adr] > min)
		min = line[adr];
	      if (line[adr + 1] > min)
		min = line[adr + 1];
	      if (line[adr + 2] > min)
		min = line[adr + 2];
	    }
	}

      DBG(DBG_io2, "%s: pass=%d, min=%d\n", __func__, pass, min);
      bottom++;
    }
  while (pass < 128 && min == 0);
  if (pass == 128)
    {
        throw SaneException(SANE_STATUS_INVAL, "failed to find correct offset");
    }

  DBG(DBG_info, "%s: offset=(%d,%d,%d)\n", __func__,
      dev->frontend.get_offset(0),
      dev->frontend.get_offset(1),
      dev->frontend.get_offset(2));
}

#define DARK_TARGET 8
/**
 * This function does the offset calibration by scanning one line of the calibration
 * area below scanner's top. There is a black margin and the remaining is white.
 * genesys_search_start() must have been called so that the offsets and margins
 * are already known.
 * @param dev scanner's device
*/
void CommandSetGl646::offset_calibration(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                         Genesys_Register_Set& regs) const
{
    DBG_HELPER(dbg);
    (void) regs;

  unsigned int channels;
  int pass = 0, avg;
  Genesys_Settings settings;
  int topavg, bottomavg;
  int top, bottom, black_pixels;

    if (dev->model->adc_id == AdcId::AD_XP200) {
        ad_fe_offset_calibration(dev, sensor);
        return;
    }

  DBG(DBG_proc, "%s: start\n", __func__); // TODO

  /* setup for a RGB scan, one full sensor's width line */
  /* resolution is the one from the final scan          */
    channels = 3;
    int resolution = get_closest_resolution(dev->model->sensor_id, dev->settings.xres, channels);

    const auto& calib_sensor = sanei_genesys_find_sensor(dev, resolution, 3, ScanMethod::FLATBED);
    black_pixels = (calib_sensor.black_pixels * resolution) / calib_sensor.optical_res;

  DBG(DBG_io2, "%s: black_pixels=%d\n", __func__, black_pixels);

    settings.scan_method = dev->model->default_method;
  settings.scan_mode = ScanColorMode::COLOR_SINGLE_PASS;
  settings.xres = resolution;
  settings.yres = resolution;
  settings.tl_x = 0;
  settings.tl_y = 0;
    settings.pixels = (calib_sensor.sensor_pixels * resolution) / calib_sensor.optical_res;
    settings.requested_pixels = settings.pixels;
  settings.lines = CALIBRATION_LINES;
  settings.depth = 8;
  settings.color_filter = ColorFilter::RED;

  settings.disable_interpolation = 0;
  settings.threshold = 0;

  /* scan first line of data with no gain, but with offset from
   * last calibration */
  dev->frontend.set_gain(0, 0);
  dev->frontend.set_gain(1, 0);
  dev->frontend.set_gain(2, 0);

  /* scan with no move */
  bottom = 90;
  dev->frontend.set_offset(0, bottom);
  dev->frontend.set_offset(1, bottom);
  dev->frontend.set_offset(2, bottom);

  std::vector<uint8_t> first_line, second_line;

    simple_scan(dev, calib_sensor, settings, false, true, false, first_line);

  if (DBG_LEVEL >= DBG_data)
    {
      char title[30];
      snprintf(title, 30, "gl646_offset%03d.pnm", bottom);
      sanei_genesys_write_pnm_file(title, first_line.data(), 8, channels,
                                   settings.pixels, settings.lines);
    }
  bottomavg = dark_average(first_line.data(), settings.pixels, settings.lines, channels,
                           black_pixels);
  DBG(DBG_io2, "%s: bottom avg=%d\n", __func__, bottomavg);

  /* now top value */
  top = 231;
  dev->frontend.set_offset(0, top);
  dev->frontend.set_offset(1, top);
  dev->frontend.set_offset(2, top);
    simple_scan(dev, calib_sensor, settings, false, true, false, second_line);

  if (DBG_LEVEL >= DBG_data)
    {
      char title[30];
      snprintf(title, 30, "gl646_offset%03d.pnm", top);
      sanei_genesys_write_pnm_file (title, second_line.data(), 8, channels,
				    settings.pixels, settings.lines);
    }
  topavg = dark_average(second_line.data(), settings.pixels, settings.lines, channels,
                        black_pixels);
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
        simple_scan(dev, calib_sensor, settings, false, true, false, second_line);

      if (DBG_LEVEL >= DBG_data)
	{
          char title[30];
          snprintf(title, 30, "gl646_offset%03d.pnm", dev->frontend.get_offset(1));
          sanei_genesys_write_pnm_file (title, second_line.data(), 8, channels,
					settings.pixels, settings.lines);
	}

      avg =
        dark_average (second_line.data(), settings.pixels, settings.lines, channels,
		      black_pixels);
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

/** @brief gain calibration for Analog Device frontends
 * Alternative coarse gain calibration
 */
static void ad_fe_coarse_gain_calibration(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                          Genesys_Register_Set& regs, int dpi)
{
    DBG_HELPER(dbg);
    (void) sensor;
    (void) regs;

  unsigned int i, channels, val;
  unsigned int size, count, resolution, pass;
  float average;
  Genesys_Settings settings;
  char title[32];

  /* setup for a RGB scan, one full sensor's width line */
  /* resolution is the one from the final scan          */
  channels = 3;
  resolution = get_closest_resolution(dev->model->sensor_id, dpi, channels);

    const auto& calib_sensor = sanei_genesys_find_sensor(dev, resolution, 3, ScanMethod::FLATBED);

  settings.scan_mode = ScanColorMode::COLOR_SINGLE_PASS;

    settings.scan_method = dev->model->default_method;
  settings.xres = resolution;
  settings.yres = resolution;
  settings.tl_x = 0;
  settings.tl_y = 0;
    settings.pixels = (calib_sensor.sensor_pixels * resolution) / calib_sensor.optical_res;
    settings.requested_pixels = settings.pixels;
  settings.lines = CALIBRATION_LINES;
  settings.depth = 8;
  settings.color_filter = ColorFilter::RED;

  settings.disable_interpolation = 0;
  settings.threshold = 0;

  size = channels * settings.pixels * settings.lines;

  /* start gain value */
  dev->frontend.set_gain(0, 1);
  dev->frontend.set_gain(1, 1);
  dev->frontend.set_gain(2, 1);

  average = 0;
  pass = 0;

  std::vector<uint8_t> line;

    // loop until each channel raises to acceptable level
    while ((average < calib_sensor.gain_white_ref) && (pass < 30)) {
        // scan with no move
        simple_scan(dev, calib_sensor, settings, false, true, false, line);

      /* log scanning data */
      if (DBG_LEVEL >= DBG_data)
	{
          sprintf (title, "gl646_alternative_gain%02d.pnm", pass);
          sanei_genesys_write_pnm_file(title, line.data(), 8, channels, settings.pixels,
                                       settings.lines);
	}
      pass++;

      /* computes white average */
      average = 0;
      count = 0;
      for (i = 0; i < size; i++)
	{
	  val = line[i];
	  average += val;
	  count++;
	}
      average = average / count;

        uint8_t gain0 = dev->frontend.get_gain(0);
        // adjusts gain for the channel
        if (average < calib_sensor.gain_white_ref) {
            gain0 += 1;
        }

        dev->frontend.set_gain(0, gain0);
        dev->frontend.set_gain(1, gain0);
        dev->frontend.set_gain(2, gain0);

      DBG(DBG_proc, "%s: average = %.2f, gain = %d\n", __func__, average, gain0);
    }

  DBG(DBG_info, "%s: gains=(%d,%d,%d)\n", __func__,
      dev->frontend.get_gain(0),
      dev->frontend.get_gain(1),
      dev->frontend.get_gain(2));
}

/**
 * Alternative coarse gain calibration
 * this on uses the settings from offset_calibration. First scan moves so
 * we can go to calibration area for XPA.
 * @param dev device for scan
 * @param dpi resolutnio to calibrate at
 */
void CommandSetGl646::coarse_gain_calibration(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                              Genesys_Register_Set& regs, int dpi) const
{
    DBG_HELPER(dbg);
    (void) dpi;

  unsigned int i, j, k, channels, val, maximum, idx;
  unsigned int count, resolution, pass;
  float average[3];
  Genesys_Settings settings;
  char title[32];

    if (dev->model->sensor_id == SensorId::CIS_XP200) {
      return ad_fe_coarse_gain_calibration(dev, sensor, regs, sensor.optical_res);
    }

  /* setup for a RGB scan, one full sensor's width line */
  /* resolution is the one from the final scan          */
  channels = 3;

  /* we are searching a sensor resolution */
    resolution = get_closest_resolution(dev->model->sensor_id, dev->settings.xres, channels);

    const auto& calib_sensor = sanei_genesys_find_sensor(dev, resolution, channels,
                                                         ScanMethod::FLATBED);

  settings.scan_method = dev->settings.scan_method;
  settings.scan_mode = ScanColorMode::COLOR_SINGLE_PASS;
  settings.xres = resolution;
  settings.yres = resolution;
  settings.tl_y = 0;
  if (settings.scan_method == ScanMethod::FLATBED)
    {
      settings.tl_x = 0;
        settings.pixels = (calib_sensor.sensor_pixels * resolution) / calib_sensor.optical_res;
    }
  else
    {
        settings.tl_x = dev->model->x_offset_ta;
        settings.pixels = (dev->model->x_size_ta * resolution) / MM_PER_INCH;
    }
    settings.requested_pixels = settings.pixels;
  settings.lines = CALIBRATION_LINES;
  settings.depth = 8;
  settings.color_filter = ColorFilter::RED;

  settings.disable_interpolation = 0;
  settings.threshold = 0;

  /* start gain value */
  dev->frontend.set_gain(0, 1);
  dev->frontend.set_gain(1, 1);
  dev->frontend.set_gain(2, 1);

  if (channels > 1)
    {
      average[0] = 0;
      average[1] = 0;
      average[2] = 0;
      idx = 0;
    }
  else
    {
      average[0] = 255;
      average[1] = 255;
      average[2] = 255;
        switch (dev->settings.color_filter) {
            case ColorFilter::RED: idx = 0; break;
            case ColorFilter::GREEN: idx = 1; break;
            case ColorFilter::BLUE: idx = 2; break;
            default: idx = 0; break; // should not happen
        }
      average[idx] = 0;
    }
  pass = 0;

  std::vector<uint8_t> line;

  /* loop until each channel raises to acceptable level */
    while (((average[0] < calib_sensor.gain_white_ref) ||
            (average[1] < calib_sensor.gain_white_ref) ||
            (average[2] < calib_sensor.gain_white_ref)) && (pass < 30))
    {
        // scan with no move
        simple_scan(dev, calib_sensor, settings, false, true, false, line);

      /* log scanning data */
      if (DBG_LEVEL >= DBG_data)
	{
          std::sprintf(title, "gl646_gain%02d.pnm", pass);
          sanei_genesys_write_pnm_file(title, line.data(), 8, channels, settings.pixels,
                                       settings.lines);
	}
      pass++;

      /* average high level for each channel and compute gain
         to reach the target code
         we only use the central half of the CCD data         */
      for (k = idx; k < idx + channels; k++)
	{
	  /* we find the maximum white value, so we can deduce a threshold
	     to average white values */
	  maximum = 0;
	  for (i = 0; i < settings.lines; i++)
	    {
	      for (j = 0; j < settings.pixels; j++)
		{
		  val = line[i * channels * settings.pixels + j + k];
		  if (val > maximum)
		    maximum = val;
		}
	    }

	  /* threshold */
	  maximum *= 0.9;

	  /* computes white average */
	  average[k] = 0;
	  count = 0;
	  for (i = 0; i < settings.lines; i++)
	    {
	      for (j = 0; j < settings.pixels; j++)
		{
		  /* averaging only white points allow us not to care about dark margins */
		  val = line[i * channels * settings.pixels + j + k];
		  if (val > maximum)
		    {
		      average[k] += val;
		      count++;
		    }
		}
	    }
	  average[k] = average[k] / count;

	  /* adjusts gain for the channel */
          if (average[k] < calib_sensor.gain_white_ref)
            dev->frontend.set_gain(k, dev->frontend.get_gain(k) + 1);

	  DBG(DBG_proc, "%s: channel %d, average = %.2f, gain = %d\n", __func__, k, average[k],
              dev->frontend.get_gain(k));
	}
    }

    if (channels < 3) {
        dev->frontend.set_gain(1, dev->frontend.get_gain(0));
        dev->frontend.set_gain(2, dev->frontend.get_gain(0));
    }

  DBG(DBG_info, "%s: gains=(%d,%d,%d)\n", __func__,
      dev->frontend.get_gain(0),
      dev->frontend.get_gain(1),
      dev->frontend.get_gain(2));
}

/**
 * sets up the scanner's register for warming up. We scan 2 lines without moving.
 *
 */
void CommandSetGl646::init_regs_for_warmup(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                           Genesys_Register_Set* local_reg, int* channels,
                                           int* total_size) const
{
    DBG_HELPER(dbg);
    (void) sensor;

  Genesys_Settings settings;
  int resolution, lines;

  dev->frontend = dev->frontend_initial;

  resolution = get_closest_resolution(dev->model->sensor_id, 300, 1);

    const auto& local_sensor = sanei_genesys_find_sensor(dev, resolution, 1,
                                                         dev->settings.scan_method);

  /* set up for a half width 2 lines gray scan without moving */
    settings.scan_method = dev->model->default_method;
  settings.scan_mode = ScanColorMode::GRAY;
  settings.xres = resolution;
  settings.yres = resolution;
  settings.tl_x = 0;
  settings.tl_y = 0;
    settings.pixels = (local_sensor.sensor_pixels * resolution) / local_sensor.optical_res;
    settings.requested_pixels = settings.pixels;
  settings.lines = 2;
  settings.depth = 8;
  settings.color_filter = ColorFilter::RED;

  settings.disable_interpolation = 0;
  settings.threshold = 0;

    // setup for scan
    setup_for_scan(dev, local_sensor, &dev->reg, settings, true, false, false);

  /* we are not going to move, so clear these bits */
  dev->reg.find_reg(0x02).value &= ~(REG02_FASTFED | REG02_AGOHOME);

  /* don't enable any correction for this scan */
  dev->reg.find_reg(0x01).value &= ~REG01_DVDSET;

  /* copy to local_reg */
  *local_reg = dev->reg;

  /* turn off motor during this scan */
  sanei_genesys_set_motor_power(*local_reg, false);

  /* returned value to higher level warmup function */
  *channels = 1;
    lines = local_reg->get24(REG_LINCNT) + 1;
  *total_size = lines * settings.pixels;

    // now registers are ok, write them to scanner
    gl646_set_fe(dev, local_sensor, AFE_SET, settings.xres);
    dev->write_registers(*local_reg);
}


/*
 * this function moves head without scanning, forward, then backward
 * so that the head goes to park position.
 * as a by-product, also check for lock
 */
static void gl646_repark_head(Genesys_Device* dev)
{
    DBG_HELPER(dbg);
  Genesys_Settings settings;
  unsigned int expected, steps;

    settings.scan_method = dev->model->default_method;
  settings.scan_mode = ScanColorMode::COLOR_SINGLE_PASS;
  settings.xres = get_closest_resolution(dev->model->sensor_id, 75, 1);
  settings.yres = settings.xres;
  settings.tl_x = 0;
  settings.tl_y = 5;
  settings.pixels = 600;
    settings.requested_pixels = settings.pixels;
  settings.lines = 4;
  settings.depth = 8;
  settings.color_filter = ColorFilter::RED;

  settings.disable_interpolation = 0;
  settings.threshold = 0;

    const auto& sensor = sanei_genesys_find_sensor(dev, settings.xres, 3,
                                                   dev->model->default_method);

    setup_for_scan(dev, sensor, &dev->reg, settings, false, false, false);

  /* TODO seems wrong ... no effective scan */
  dev->reg.find_reg(0x01).value &= ~REG01_SCAN;

    dev->write_registers(dev->reg);

    // start scan
    dev->cmd_set->begin_scan(dev, sensor, &dev->reg, true);

    expected = dev->reg.get24(REG_FEEDL);
  do
    {
      sanei_genesys_sleep_ms(100);
        sanei_genesys_read_feed_steps (dev, &steps);
    }
  while (steps < expected);

    // toggle motor flag, put an huge step number and redo move backward
    dev->cmd_set->slow_back_home(dev, 1);
}

/* *
 * initialize ASIC : registers, motor tables, and gamma tables
 * then ensure scanner's head is at home
 * @param dev device description of the scanner to initailize
 */
void CommandSetGl646::init(Genesys_Device* dev) const
{
    DBG_INIT();
    DBG_HELPER(dbg);

    uint8_t val = 0;
  uint32_t addr = 0xdead;
  size_t len;

    // to detect real power up condition, we write to REG41 with pwrbit set, then read it back. When
    // scanner is cold (just replugged) PWRBIT will be set in the returned value
    std::uint8_t cold = sanei_genesys_get_status(dev);
  DBG(DBG_info, "%s: status=0x%02x\n", __func__, cold);
  cold = !(cold & REG41_PWRBIT);
  if (cold)
    {
      DBG(DBG_info, "%s: device is cold\n", __func__);
    }
  else
    {
      DBG(DBG_info, "%s: device is hot\n", __func__);
    }

  const auto& sensor = sanei_genesys_find_sensor_any(dev);

  /* if scanning session hasn't been initialized, set it up */
  if (!dev->already_initialized)
    {
      dev->dark_average_data.clear();
      dev->white_average_data.clear();

      dev->settings.color_filter = ColorFilter::GREEN;

      /* Set default values for registers */
      gl646_init_regs (dev);

        // Init shading data
        sanei_genesys_init_shading_data(dev, sensor, sensor.sensor_pixels);

      /* initial calibration reg values */
      dev->calib_reg = dev->reg;
    }

  /* execute physical unit init only if cold */
  if (cold)
    {
      DBG(DBG_info, "%s: device is cold\n", __func__);

        val = 0x04;
        dev->usb_dev.control_msg(REQUEST_TYPE_OUT, REQUEST_REGISTER, VALUE_INIT, INDEX, 1, &val);

        // ASIC reset
        dev->write_register(0x0e, 0x00);
      sanei_genesys_sleep_ms(100);

        // Write initial registers
        dev->write_registers(dev->reg);

        if (dev->model->flags & GENESYS_FLAG_TEST_ON_INIT) {
            gl646_asic_test(dev);
        }

        // send gamma tables if needed
        dev->cmd_set->send_gamma_table(dev, sensor);

        // Set powersaving(default = 15 minutes)
        dev->cmd_set->set_powersaving(dev, 15);
    }				/* end if cold */

    // Set analog frontend
    gl646_set_fe(dev, sensor, AFE_INIT, 0);

  /* GPO enabling for XP200 */
    if (dev->model->sensor_id == SensorId::CIS_XP200) {
        dev->write_register(0x68, dev->gpo.regs.get_value(0x68));
        dev->write_register(0x69, dev->gpo.regs.get_value(0x69));

        // enable GPIO
        gl646_gpio_output_enable(dev->usb_dev, 6);

        // writes 0 to GPIO
        gl646_gpio_write(dev->usb_dev, 0);

        // clear GPIO enable
        gl646_gpio_output_enable(dev->usb_dev, 0);

        dev->write_register(0x66, 0x10);
        dev->write_register(0x66, 0x00);
        dev->write_register(0x66, 0x10);
    }

  /* MD6471/G2410 and XP200 read/write data from an undocumented memory area which
   * is after the second slope table */
    if (dev->model->gpio_id != GpioId::HP3670 &&
        dev->model->gpio_id != GpioId::HP2400)
    {
      switch (sensor.optical_res)
	{
	case 600:
	  addr = 0x08200;
	  break;
	case 1200:
	  addr = 0x10200;
	  break;
	case 2400:
	  addr = 0x1fa00;
	  break;
	}
    sanei_genesys_set_buffer_address(dev, addr);

      sanei_usb_set_timeout (2 * 1000);
      len = 6;
        // for some reason, read fails here for MD6471, HP2300 and XP200 one time out of
        // 2 scanimage launches
        try {
            bulk_read_data(dev, 0x45, dev->control, len);
        } catch (...) {
            bulk_read_data(dev, 0x45, dev->control, len);
        }
        DBG(DBG_info, "%s: control read=0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n", __func__,
            dev->control[0], dev->control[1], dev->control[2], dev->control[3], dev->control[4],
            dev->control[5]);
      sanei_usb_set_timeout (30 * 1000);
    }
  else
    /* HP2400 and HP3670 case */
    {
      dev->control[0] = 0x00;
      dev->control[1] = 0x00;
      dev->control[2] = 0x01;
      dev->control[3] = 0x00;
      dev->control[4] = 0x00;
      dev->control[5] = 0x00;
    }

  /* ensure head is correctly parked, and check lock */
    if (!dev->model->is_sheetfed) {
      if (dev->model->flags & GENESYS_FLAG_REPARK)
	{
            // FIXME: if repark fails, we should print an error message that the scanner is locked and
            // the user should unlock the lock. We should also rethrow with SANE_STATUS_JAMMED
            gl646_repark_head(dev);
	}
      else
    {
            slow_back_home(dev, true);
	}
    }

  /* here session and device are initialized */
    dev->already_initialized = true;
}

void CommandSetGl646::move_to_ta(Genesys_Device* dev) const
{
    DBG_HELPER(dbg);

    simple_move(dev, dev->model->y_offset_sensor_to_ta);
}


/**
 * Does a simple scan: ie no line reordering and avanced data buffering and
 * shading correction. Memory for data is allocated in this function
 * and must be freed by caller.
 * @param dev device of the scanner
 * @param settings parameters of the scan
 * @param move true if moving during scan
 * @param forward true if moving forward during scan
 * @param shading true to enable shading correction
 * @param data pointer for the data
 */
static void simple_scan(Genesys_Device* dev, const Genesys_Sensor& sensor,
                        Genesys_Settings settings, bool move, bool forward,
                        bool shading, std::vector<uint8_t>& data)
{
    DBG_HELPER_ARGS(dbg, "move=%d, forward=%d, shading=%d", move, forward, shading);
  unsigned int size, lines, x, y, bpp;
    bool split;

  /* round up to multiple of 3 in case of CIS scanner */
    if (dev->model->is_cis) {
      settings.lines = ((settings.lines + 2) / 3) * 3;
    }

  /* setup for move then scan */
    split = !(move && settings.tl_y > 0);
    setup_for_scan(dev, sensor, &dev->reg, settings, split, false, false);

  /* allocate memory fo scan : LINCNT may have been adjusted for CCD reordering */
    if (dev->model->is_cis) {
        lines = dev->reg.get24(REG_LINCNT) / 3;
    }
  else
    {
        lines = dev->reg.get24(REG_LINCNT) + 1;
    }
  size = lines * settings.pixels;
    if (settings.depth == 16) {
        bpp = 2;
    } else {
        bpp = 1;
    }
    size *= bpp * settings.get_channels();
  data.clear();
  data.resize(size);

  DBG(DBG_io, "%s: allocated %d bytes of memory for %d lines\n", __func__, size, lines);

  /* put back real line number in settings */
  settings.lines = lines;

    // initialize frontend
    gl646_set_fe(dev, sensor, AFE_SET, settings.xres);

  /* no shading correction and not watch dog for simple scan */
  dev->reg.find_reg(0x01).value &= ~(REG01_DVDSET | REG01_DOGENB);
    if (shading) {
      dev->reg.find_reg(0x01).value |= REG01_DVDSET;
    }

  /* enable gamma table for the scan */
  dev->reg.find_reg(0x05).value |= REG05_GMMENB;

  /* one table movement for simple scan */
  dev->reg.find_reg(0x02).value &= ~REG02_FASTFED;

    if (!move) {
      sanei_genesys_set_motor_power(dev->reg, false);

      /* no automatic go home if no movement */
      dev->reg.find_reg(0x02).value &= ~REG02_AGOHOME;
    }
    if (!forward) {
      dev->reg.find_reg(0x02).value |= REG02_MTRREV;
    }
  else
    {
      dev->reg.find_reg(0x02).value &= ~REG02_MTRREV;
    }

  /* no automatic go home when using XPA */
  if (settings.scan_method == ScanMethod::TRANSPARENCY)
    {
      dev->reg.find_reg(0x02).value &= ~REG02_AGOHOME;
    }

    // write scan registers
    dev->write_registers(dev->reg);

    // starts scan
    dev->cmd_set->begin_scan(dev, sensor, &dev->reg, move);

    wait_until_buffer_non_empty(dev, true);

    // now we're on target, we can read data
    sanei_genesys_read_data_from_scanner(dev, data.data(), size);

  /* in case of CIS scanner, we must reorder data */
    if (dev->model->is_cis && settings.scan_mode == ScanColorMode::COLOR_SINGLE_PASS) {
      /* alloc one line sized working buffer */
      std::vector<uint8_t> buffer(settings.pixels * 3 * bpp);

      /* reorder one line of data and put it back to buffer */
      if (bpp == 1)
	{
	  for (y = 0; y < lines; y++)
	    {
	      /* reorder line */
	      for (x = 0; x < settings.pixels; x++)
		{
                  buffer[x * 3] = data[y * settings.pixels * 3 + x];
                  buffer[x * 3 + 1] = data[y * settings.pixels * 3 + settings.pixels + x];
                  buffer[x * 3 + 2] = data[y * settings.pixels * 3 + 2 * settings.pixels + x];
		}
	      /* copy line back */
              memcpy (data.data() + settings.pixels * 3 * y, buffer.data(),
		      settings.pixels * 3);
	    }
	}
      else
	{
	  for (y = 0; y < lines; y++)
	    {
	      /* reorder line */
	      for (x = 0; x < settings.pixels; x++)
		{
                  buffer[x * 6] = data[y * settings.pixels * 6 + x * 2];
                  buffer[x * 6 + 1] = data[y * settings.pixels * 6 + x * 2 + 1];
                  buffer[x * 6 + 2] = data[y * settings.pixels * 6 + 2 * settings.pixels + x * 2];
                  buffer[x * 6 + 3] = data[y * settings.pixels * 6 + 2 * settings.pixels + x * 2 + 1];
                  buffer[x * 6 + 4] = data[y * settings.pixels * 6 + 4 * settings.pixels + x * 2];
                  buffer[x * 6 + 5] = data[y * settings.pixels * 6 + 4 * settings.pixels + x * 2 + 1];
		}
	      /* copy line back */
              memcpy (data.data() + settings.pixels * 6 * y, buffer.data(),
		      settings.pixels * 6);
	    }
	}
    }

    // end scan , waiting the motor to stop if needed (if moving), but without ejecting doc
    end_scan_impl(dev, &dev->reg, true, false);
}

/**
 * Does a simple move of the given distance by doing a scan at lowest resolution
 * shading correction. Memory for data is allocated in this function
 * and must be freed by caller.
 * @param dev device of the scanner
 * @param distance distance to move in MM
 */
static void simple_move(Genesys_Device* dev, SANE_Int distance)
{
    DBG_HELPER_ARGS(dbg, "%d mm", distance);
  Genesys_Settings settings;

  int resolution = get_lowest_resolution(dev->model->sensor_id, 3);

  const auto& sensor = sanei_genesys_find_sensor(dev, resolution, 3, dev->model->default_method);

  /* TODO give a no AGOHOME flag */
    settings.scan_method = dev->model->default_method;
  settings.scan_mode = ScanColorMode::COLOR_SINGLE_PASS;
  settings.xres = resolution;
  settings.yres = resolution;
  settings.tl_y = 0;
  settings.tl_x = 0;
  settings.pixels = (sensor.sensor_pixels * settings.xres) / sensor.optical_res;
    settings.requested_pixels = settings.pixels;
  settings.lines = (distance * settings.xres) / MM_PER_INCH;
  settings.depth = 8;
  settings.color_filter = ColorFilter::RED;

  settings.disable_interpolation = 0;
  settings.threshold = 0;

  std::vector<uint8_t> data;
    simple_scan(dev, sensor, settings, true, true, false, data);
}

/**
 * update the status of the required sensor in the scanner session
 * the button fileds are used to make events 'sticky'
 */
void CommandSetGl646::update_hardware_sensors(Genesys_Scanner* session) const
{
    DBG_HELPER(dbg);
  Genesys_Device *dev = session->dev;
  uint8_t value;

    // do what is needed to get a new set of events, but try to not loose any of them.
    gl646_gpio_read(dev->usb_dev, &value);
    DBG(DBG_io, "%s: GPIO=0x%02x\n", __func__, value);

    // scan button
    if (dev->model->buttons & GENESYS_HAS_SCAN_SW) {
        switch (dev->model->gpio_id) {
        case GpioId::XP200:
            session->buttons[BUTTON_SCAN_SW].write((value & 0x02) != 0);
            break;
        case GpioId::MD_5345:
            session->buttons[BUTTON_SCAN_SW].write(value == 0x16);
            break;
        case GpioId::HP2300:
            session->buttons[BUTTON_SCAN_SW].write(value == 0x6c);
            break;
        case GpioId::HP3670:
        case GpioId::HP2400:
            session->buttons[BUTTON_SCAN_SW].write((value & 0x20) == 0);
            break;
        default:
                throw SaneException(SANE_STATUS_UNSUPPORTED, "unknown gpo type");
	}
    }

    // email button
    if (dev->model->buttons & GENESYS_HAS_EMAIL_SW) {
        switch (dev->model->gpio_id) {
        case GpioId::MD_5345:
            session->buttons[BUTTON_EMAIL_SW].write(value == 0x12);
            break;
        case GpioId::HP3670:
        case GpioId::HP2400:
            session->buttons[BUTTON_EMAIL_SW].write((value & 0x08) == 0);
            break;
        default:
                throw SaneException(SANE_STATUS_UNSUPPORTED, "unknown gpo type");
    }
    }

    // copy button
    if (dev->model->buttons & GENESYS_HAS_COPY_SW) {
        switch (dev->model->gpio_id) {
        case GpioId::MD_5345:
            session->buttons[BUTTON_COPY_SW].write(value == 0x11);
            break;
        case GpioId::HP2300:
            session->buttons[BUTTON_COPY_SW].write(value == 0x5c);
            break;
        case GpioId::HP3670:
        case GpioId::HP2400:
            session->buttons[BUTTON_COPY_SW].write((value & 0x10) == 0);
            break;
        default:
                throw SaneException(SANE_STATUS_UNSUPPORTED, "unknown gpo type");
    }
    }

    // power button
    if (dev->model->buttons & GENESYS_HAS_POWER_SW) {
        switch (dev->model->gpio_id) {
        case GpioId::MD_5345:
            session->buttons[BUTTON_POWER_SW].write(value == 0x14);
            break;
        default:
                throw SaneException(SANE_STATUS_UNSUPPORTED, "unknown gpo type");
    }
    }

    // ocr button
    if (dev->model->buttons & GENESYS_HAS_OCR_SW) {
        switch (dev->model->gpio_id) {
    case GpioId::MD_5345:
            session->buttons[BUTTON_OCR_SW].write(value == 0x13);
            break;
	default:
                throw SaneException(SANE_STATUS_UNSUPPORTED, "unknown gpo type");
    }
    }

    // document detection
    if (dev->model->buttons & GENESYS_HAS_PAGE_LOADED_SW) {
        switch (dev->model->gpio_id) {
        case GpioId::XP200:
            session->buttons[BUTTON_PAGE_LOADED_SW].write((value & 0x04) != 0);
            break;
        default:
                throw SaneException(SANE_STATUS_UNSUPPORTED, "unknown gpo type");
    }
    }

  /* XPA detection */
  if (dev->model->flags & GENESYS_FLAG_XPA)
    {
        switch (dev->model->gpio_id) {
            case GpioId::HP3670:
            case GpioId::HP2400:
	  /* test if XPA is plugged-in */
	  if ((value & 0x40) == 0)
	    {
	      DBG(DBG_io, "%s: enabling XPA\n", __func__);
	      session->opt[OPT_SOURCE].cap &= ~SANE_CAP_INACTIVE;
	    }
	  else
	    {
	      DBG(DBG_io, "%s: disabling XPA\n", __func__);
	      session->opt[OPT_SOURCE].cap |= SANE_CAP_INACTIVE;
	    }
      break;
            default:
                throw SaneException(SANE_STATUS_UNSUPPORTED, "unknown gpo type");
    }
    }
}


static void write_control(Genesys_Device* dev, const Genesys_Sensor& sensor, int resolution)
{
    DBG_HELPER(dbg);
  uint8_t control[4];
  uint32_t addr = 0xdead;

  /* 2300 does not write to 'control' */
    if (dev->model->motor_id == MotorId::HP2300) {
        return;
    }

  /* MD6471/G2410/HP2300 and XP200 read/write data from an undocumented memory area which
   * is after the second slope table */
  switch (sensor.optical_res)
    {
    case 600:
      addr = 0x08200;
      break;
    case 1200:
      addr = 0x10200;
      break;
    case 2400:
      addr = 0x1fa00;
      break;
    default:
        throw SaneException("failed to compute control address");
    }

  /* XP200 sets dpi, what other scanner put is unknown yet */
  switch (dev->model->motor_id)
    {
        case MotorId::XP200:
      /* we put scan's dpi, not motor one */
            control[0] = resolution & 0xff;
            control[1] = (resolution >> 8) & 0xff;
      control[2] = dev->control[4];
      control[3] = dev->control[5];
      break;
        case MotorId::HP3670:
        case MotorId::HP2400:
        case MotorId::MD_5345:
        default:
      control[0] = dev->control[2];
      control[1] = dev->control[3];
      control[2] = dev->control[4];
      control[3] = dev->control[5];
      break;
    }

  DBG(DBG_info, "%s: control write=0x%02x 0x%02x 0x%02x 0x%02x\n", __func__, control[0], control[1],
      control[2], control[3]);
    sanei_genesys_set_buffer_address(dev, addr);
    sanei_genesys_bulk_write_data(dev, 0x3c, control, 4);
}

/**
 * check if a stored calibration is compatible with requested scan.
 * @return true if compatible, false if not.
 * Whenever an error is met, it is returned.
 * @param dev scanner device
 * @param cache cache entry to test
 * @param for_overwrite reserved for future use ...
 */
bool CommandSetGl646::is_compatible_calibration(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                                Genesys_Calibration_Cache* cache,
                                                bool for_overwrite) const
{
    (void) sensor;
#ifdef HAVE_SYS_TIME_H
  struct timeval time;
#endif
  int compatible = 1;

  DBG(DBG_proc, "%s: start (for_overwrite=%d)\n", __func__, for_overwrite);

  if (cache == nullptr)
    return false;

  /* build minimal current_setup for calibration cache use only, it will be better
   * computed when during setup for scan
   */
    dev->session.params.channels = dev->settings.get_channels();
  dev->current_setup.xres = dev->settings.xres;

    DBG(DBG_io, "%s: requested=(%d,%f), tested=(%d,%f)\n", __func__,
        dev->session.params.channels, dev->current_setup.xres,
        cache->params.channels, cache->used_setup.xres);

  /* a calibration cache is compatible if color mode and x dpi match the user
   * requested scan. In the case of CIS scanners, dpi isn't a criteria */
    if (!dev->model->is_cis) {
        compatible = (dev->session.params.channels == cache->params.channels) &&
                     (static_cast<int>(dev->current_setup.xres) ==
                          static_cast<int>(cache->used_setup.xres));
    } else {
        compatible = dev->session.params.channels == cache->params.channels;
    }

  if (dev->session.params.scan_method != cache->params.scan_method)
    {
      DBG(DBG_io, "%s: current method=%d, used=%d\n", __func__,
          static_cast<unsigned>(dev->session.params.scan_method),
          static_cast<unsigned>(cache->params.scan_method));
      compatible = 0;
    }
  if (!compatible)
    {
      DBG(DBG_proc, "%s: completed, non compatible cache\n", __func__);
      return false;
    }

  /* a cache entry expires after 30 minutes for non sheetfed scanners */
  /* this is not taken into account when overwriting cache entries    */
#ifdef HAVE_SYS_TIME_H
    if (!for_overwrite) {
      gettimeofday (&time, nullptr);
        if ((time.tv_sec - cache->last_calibration > 30 * 60) && !dev->model->is_sheetfed) {
          DBG(DBG_proc, "%s: expired entry, non compatible cache\n", __func__);
          return false;
        }
    }
#endif

  DBG(DBG_proc, "%s: completed, cache compatible\n", __func__);
  return true;
}

/**
 * search for a full width black or white strip.
 * @param dev scanner device
 * @param forward true if searching forward, false if searching backward
 * @param black true if searching for a black strip, false for a white strip
 */
void CommandSetGl646::search_strip(Genesys_Device* dev, const Genesys_Sensor& sensor, bool forward,
                                   bool black) const
{
    DBG_HELPER(dbg);
    (void) sensor;

  Genesys_Settings settings;
  int res = get_closest_resolution(dev->model->sensor_id, 75, 1);
  unsigned int pass, count, found, x, y;
  char title[80];

    const auto& calib_sensor = sanei_genesys_find_sensor(dev, res, 1, ScanMethod::FLATBED);

  /* we set up for a lowest available resolution color grey scan, full width */
    settings.scan_method = dev->model->default_method;
  settings.scan_mode = ScanColorMode::GRAY;
  settings.xres = res;
  settings.yres = res;
  settings.tl_x = 0;
  settings.tl_y = 0;
    settings.pixels = (dev->model->x_size * res) / MM_PER_INCH;
    settings.pixels /= calib_sensor.get_ccd_size_divisor_for_dpi(res);
    settings.requested_pixels = settings.pixels;

  /* 15 mm at at time */
  settings.lines = (15 * settings.yres) / MM_PER_INCH;	/* may become a parameter from genesys_devices.c */
  settings.depth = 8;
  settings.color_filter = ColorFilter::RED;

  settings.disable_interpolation = 0;
  settings.threshold = 0;

  /* signals if a strip of the given color has been found */
  found = 0;

  /* detection pass done */
  pass = 0;

  std::vector<uint8_t> data;

  /* loop until strip is found or maximum pass number done */
  while (pass < 20 && !found)
    {
        // scan a full width strip
        simple_scan(dev, calib_sensor, settings, true, forward, false, data);

      if (DBG_LEVEL >= DBG_data)
	{
            std::sprintf(title, "gl646_search_strip_%s%02d.pnm", forward ? "fwd" : "bwd", pass);
          sanei_genesys_write_pnm_file (title, data.data(), settings.depth, 1,
					settings.pixels, settings.lines);
	}

      /* search data to find black strip */
      /* when searching forward, we only need one line of the searched color since we
       * will scan forward. But when doing backward search, we need all the area of the
       * same color */
      if (forward)
	{
	  for (y = 0; y < settings.lines && !found; y++)
	    {
	      count = 0;
	      /* count of white/black pixels depending on the color searched */
	      for (x = 0; x < settings.pixels; x++)
		{
		  /* when searching for black, detect white pixels */
		  if (black && data[y * settings.pixels + x] > 90)
		    {
		      count++;
		    }
		  /* when searching for white, detect black pixels */
		  if (!black && data[y * settings.pixels + x] < 60)
		    {
		      count++;
		    }
		}

	      /* at end of line, if count >= 3%, line is not fully of the desired color
	       * so we must go to next line of the buffer */
	      /* count*100/pixels < 3 */
	      if ((count * 100) / settings.pixels < 3)
		{
		  found = 1;
		  DBG(DBG_data, "%s: strip found forward during pass %d at line %d\n", __func__,
		      pass, y);
		}
	      else
		{
		  DBG(DBG_data, "%s: pixels=%d, count=%d\n", __func__, settings.pixels, count);
		}
	    }
	}
      else			/* since calibration scans are done forward, we need the whole area
				   to be of the required color when searching backward */
	{
	  count = 0;
	  for (y = 0; y < settings.lines; y++)
	    {
	      /* count of white/black pixels depending on the color searched */
	      for (x = 0; x < settings.pixels; x++)
		{
		  /* when searching for black, detect white pixels */
		  if (black && data[y * settings.pixels + x] > 60)
		    {
		      count++;
		    }
		  /* when searching for white, detect black pixels */
		  if (!black && data[y * settings.pixels + x] < 60)
		    {
		      count++;
		    }
		}
	    }

	  /* at end of area, if count >= 3%, area is not fully of the desired color
	   * so we must go to next buffer */
	  if ((count * 100) / (settings.pixels * settings.lines) < 3)
	    {
	      found = 1;
	      DBG(DBG_data, "%s: strip found backward during pass %d \n", __func__, pass);
	    }
	  else
	    {
	      DBG(DBG_data, "%s: pixels=%d, count=%d\n", __func__, settings.pixels, count);
	    }
	}
      pass++;
    }
  if (found)
    {
      DBG(DBG_info, "%s: strip found\n", __func__);
    }
  else
    {
        throw SaneException(SANE_STATUS_UNSUPPORTED, "%s strip not found", black ? "black" : "white");
    }
}

void CommandSetGl646::wait_for_motor_stop(Genesys_Device* dev) const
{
    (void) dev;
}

void CommandSetGl646::rewind(Genesys_Device* dev) const
{
    (void) dev;
    throw SaneException("not implemented");
}

void CommandSetGl646::bulk_write_data(Genesys_Device* dev, uint8_t addr, uint8_t* data,
                                      size_t len) const
{
    sanei_genesys_bulk_write_data(dev, addr, data, len);
}

void CommandSetGl646::send_shading_data(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                        std::uint8_t* data, int size) const
{
    (void) dev;
    (void) sensor;
    (void) data;
    (void) size;
    throw SaneException("not implemented");
}

void CommandSetGl646::calculate_current_setup(Genesys_Device* dev,
                                              const Genesys_Sensor& sensor) const
{
    (void) dev;
    (void) sensor;
    throw SaneException("not implemented");
}

void CommandSetGl646::asic_boot(Genesys_Device *dev, bool cold) const
{
    (void) dev;
    (void) cold;
    throw SaneException("not implemented");
}

std::unique_ptr<CommandSet> create_gl646_cmd_set()
{
    return std::unique_ptr<CommandSet>(new CommandSetGl646{});
}
