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

#include "genesys_gl847.h"

#include <vector>

/****************************************************************************
 Mid level functions
 ****************************************************************************/

static SANE_Bool
gl847_get_fast_feed_bit (Genesys_Register_Set * regs)
{
  GenesysRegister *r = NULL;

  r = sanei_genesys_get_address (regs, REG02);
  if (r && (r->value & REG02_FASTFED))
    return SANE_TRUE;
  return SANE_FALSE;
}

static SANE_Bool
gl847_get_filter_bit (Genesys_Register_Set * regs)
{
  GenesysRegister *r = NULL;

  r = sanei_genesys_get_address (regs, REG04);
  if (r && (r->value & REG04_FILTER))
    return SANE_TRUE;
  return SANE_FALSE;
}

static SANE_Bool
gl847_get_lineart_bit (Genesys_Register_Set * regs)
{
  GenesysRegister *r = NULL;

  r = sanei_genesys_get_address (regs, REG04);
  if (r && (r->value & REG04_LINEART))
    return SANE_TRUE;
  return SANE_FALSE;
}

static SANE_Bool
gl847_get_bitset_bit (Genesys_Register_Set * regs)
{
  GenesysRegister *r = NULL;

  r = sanei_genesys_get_address (regs, REG04);
  if (r && (r->value & REG04_BITSET))
    return SANE_TRUE;
  return SANE_FALSE;
}

static SANE_Bool
gl847_get_gain4_bit (Genesys_Register_Set * regs)
{
  GenesysRegister *r = NULL;

  r = sanei_genesys_get_address (regs, 0x06);
  if (r && (r->value & REG06_GAIN4))
    return SANE_TRUE;
  return SANE_FALSE;
}

static SANE_Bool
gl847_test_buffer_empty_bit (SANE_Byte val)
{
  if (val & REG41_BUFEMPTY)
    return SANE_TRUE;
  return SANE_FALSE;
}

static SANE_Bool
gl847_test_motor_flag_bit (SANE_Byte val)
{
  if (val & REG41_MOTORENB)
    return SANE_TRUE;
  return SANE_FALSE;
}

/**
 * compute the step multiplier used
 */
static int
gl847_get_step_multiplier (Genesys_Register_Set * regs)
{
  GenesysRegister *r = NULL;
  int value = 1;

  r = sanei_genesys_get_address (regs, 0x9d);
  if (r != NULL)
    {
      value = (r->value & 0x0f)>>1;
      value = 1 << value;
    }
  DBG (DBG_io, "%s: step multiplier is %d\n", __func__, value);
  return value;
}

/** @brief sensor profile
 * search for the database of motor profiles and get the best one. Each
 * profile is at a specific dpihw. Use LiDE 110 table by default.
 * @param sensor_type sensor id
 * @param dpi hardware dpi for the scan
 * @return a pointer to a Sensor_Profile struct
 */
static Sensor_Profile *get_sensor_profile(int sensor_type, int dpi)
{
  unsigned int i;
  int idx;

  i=0;
  idx=-1;
  while(i<sizeof(sensors)/sizeof(Sensor_Profile))
    {
      /* exact match */
      if(sensors[i].sensor_type==sensor_type && sensors[i].dpi==dpi)
        {
          return &(sensors[i]);
        }

      /* closest match */
      if(sensors[i].sensor_type==sensor_type)
        {
          if(idx<0)
            {
              idx=i;
            }
          else
            {
              if(sensors[i].dpi>=dpi
              && sensors[i].dpi<sensors[idx].dpi)
                {
                  idx=i;
                }
            }
        }
      i++;
    }

  /* default fallback */
  if(idx<0)
    {
      DBG (DBG_warn,"%s: using default sensor profile\n",__func__);
      idx=0;
    }

  return &(sensors[idx]);
}

/**@brief compute exposure to use
 * compute the sensor exposure based on target resolution
 */
static int gl847_compute_exposure(Genesys_Device *dev, int xres)
{
    Sensor_Profile* sensor_profile=get_sensor_profile(dev->model->ccd_type, xres);
    return sensor_profile->exposure;
}


/** @brief sensor specific settings
*/
static void gl847_setup_sensor(Genesys_Device * dev, const Genesys_Sensor& sensor,
                               Genesys_Register_Set * regs, int dpi)
{
    DBG_HELPER(dbg);
  GenesysRegister *r;
  int dpihw;
  uint16_t exp;

  dpihw=sanei_genesys_compute_dpihw(dev, sensor, dpi);

    for (uint16_t addr = 0x16; addr < 0x1e; addr++) {
        regs->set8(addr, sensor.custom_regs.get_value(addr));
    }

    for (uint16_t addr = 0x52; addr < 0x52 + 9; addr++) {
        regs->set8(addr, sensor.custom_regs.get_value(addr));
    }

  /* set EXPDUMMY and CKxMAP */
  dpihw=sanei_genesys_compute_dpihw(dev, sensor, dpi);
  Sensor_Profile* sensor_profile=get_sensor_profile(dev->model->ccd_type, dpihw);

  sanei_genesys_set_reg_from_set(regs,REG_EXPDMY,(uint8_t)((sensor_profile->expdummy) & 0xff));

  /* if no calibration has been done, set default values for exposures */
  exp = sensor.exposure.red;
  if(exp==0)
    {
      exp=sensor_profile->expr;
    }
  sanei_genesys_set_double(regs,REG_EXPR,exp);

  exp = sensor.exposure.green;
  if(exp==0)
    {
      exp=sensor_profile->expg;
    }
  sanei_genesys_set_double(regs,REG_EXPG,exp);

  exp = sensor.exposure.blue;
  if(exp==0)
    {
      exp=sensor_profile->expb;
    }
  sanei_genesys_set_double(regs,REG_EXPB,exp);

  sanei_genesys_set_triple(regs,REG_CK1MAP,sensor_profile->ck1map);
  sanei_genesys_set_triple(regs,REG_CK3MAP,sensor_profile->ck3map);
  sanei_genesys_set_triple(regs,REG_CK4MAP,sensor_profile->ck4map);

  /* order of the sub-segments */
  dev->order=sensor_profile->order;

  r = sanei_genesys_get_address (regs, 0x17);
  r->value = sensor_profile->r17;
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
  if (dev->model->model_id == MODEL_CANON_LIDE_700F)
    {
       lide700 = 1;
    }

  dev->reg.clear();

  SETREG (0x01, 0x82);
  SETREG (0x02, 0x18);
  SETREG (0x03, 0x50);
  SETREG (0x04, 0x12);
  SETREG (0x05, 0x80);
  SETREG (0x06, 0x50);		/* FASTMODE + POWERBIT */
  SETREG (0x08, 0x10);
  SETREG (0x09, 0x01);
  SETREG (0x0a, 0x00);
  SETREG (0x0b, 0x01);
  SETREG (0x0c, 0x02);

  /* LED exposures */
  SETREG (0x10, 0x00);
  SETREG (0x11, 0x00);
  SETREG (0x12, 0x00);
  SETREG (0x13, 0x00);
  SETREG (0x14, 0x00);
  SETREG (0x15, 0x00);

  SETREG (0x16, 0x10);
  SETREG (0x17, 0x08);
  SETREG (0x18, 0x00);

  /* EXPDMY */
  SETREG (0x19, 0x50);

  SETREG (0x1a, 0x34);
  SETREG (0x1b, 0x00);
  SETREG (0x1c, 0x02);
  SETREG (0x1d, 0x04);
  SETREG (0x1e, 0x10);
  SETREG (0x1f, 0x04);
  SETREG (0x20, 0x02);
  SETREG (0x21, 0x10);
  SETREG (0x22, 0x7f);
  SETREG (0x23, 0x7f);
  SETREG (0x24, 0x10);
  SETREG (0x25, 0x00);
  SETREG (0x26, 0x00);
  SETREG (0x27, 0x00);
  SETREG (0x2c, 0x09);
  SETREG (0x2d, 0x60);
  SETREG (0x2e, 0x80);
  SETREG (0x2f, 0x80);
  SETREG (0x30, 0x00);
  SETREG (0x31, 0x10);
  SETREG (0x32, 0x15);
  SETREG (0x33, 0x0e);
  SETREG (0x34, 0x40);
  SETREG (0x35, 0x00);
  SETREG (0x36, 0x2a);
  SETREG (0x37, 0x30);
  SETREG (0x38, 0x2a);
  SETREG (0x39, 0xf8);
  SETREG (0x3d, 0x00);
  SETREG (0x3e, 0x00);
  SETREG (0x3f, 0x00);
  SETREG (0x52, 0x03);
  SETREG (0x53, 0x07);
  SETREG (0x54, 0x00);
  SETREG (0x55, 0x00);
  SETREG (0x56, 0x00);
  SETREG (0x57, 0x00);
  SETREG (0x58, 0x2a);
  SETREG (0x59, 0xe1);
  SETREG (0x5a, 0x55);
  SETREG (0x5e, 0x41);
  SETREG (0x5f, 0x40);
  SETREG (0x60, 0x00);
  SETREG (0x61, 0x21);
  SETREG (0x62, 0x40);
  SETREG (0x63, 0x00);
  SETREG (0x64, 0x21);
  SETREG (0x65, 0x40);
  SETREG (0x67, 0x80);
  SETREG (0x68, 0x80);
  SETREG (0x69, 0x20);
  SETREG (0x6a, 0x20);

  /* CK1MAP */
  SETREG (0x74, 0x00);
  SETREG (0x75, 0x00);
  SETREG (0x76, 0x3c);

  /* CK3MAP */
  SETREG (0x77, 0x00);
  SETREG (0x78, 0x00);
  SETREG (0x79, 0x9f);

  /* CK4MAP */
  SETREG (0x7a, 0x00);
  SETREG (0x7b, 0x00);
  SETREG (0x7c, 0x55);

  SETREG (0x7d, 0x00);

  /* NOTE: autoconf is a non working option */
  SETREG (0x87, 0x02);
  SETREG (0x9d, 0x06);
  SETREG (0xa2, 0x0f);
  SETREG (0xbd, 0x18);
  SETREG (0xfe, 0x08);

  /* gamma[0] and gamma[256] values */
  SETREG (0xbe, 0x00);
  SETREG (0xc5, 0x00);
  SETREG (0xc6, 0x00);
  SETREG (0xc7, 0x00);
  SETREG (0xc8, 0x00);
  SETREG (0xc9, 0x00);
  SETREG (0xca, 0x00);

  /* LiDE 700 fixups */
  if(lide700)
    {
      SETREG (0x5f, 0x04);
      SETREG (0x7d, 0x80);

      /* we write to these registers only once */
      val=0;
      sanei_genesys_write_register (dev, REG7E, val);
      sanei_genesys_write_register (dev, REG9E, val);
      sanei_genesys_write_register (dev, REG9F, val);
      sanei_genesys_write_register (dev, REGAB, val);
    }

  /* fine tune upon device description */
  dev->reg.find_reg(0x05).value &= ~REG05_DPIHW;
  switch (sanei_genesys_find_sensor_any(dev).optical_res)
    {
    case 600:
      dev->reg.find_reg(0x05).value |= REG05_DPIHW_600;
      break;
    case 1200:
      dev->reg.find_reg(0x05).value |= REG05_DPIHW_1200;
      break;
    case 2400:
      dev->reg.find_reg(0x05).value |= REG05_DPIHW_2400;
      break;
    case 4800:
      dev->reg.find_reg(0x05).value |= REG05_DPIHW_4800;
      break;
    }

  /* initalize calibration reg */
  dev->calib_reg = dev->reg;
}

/**@brief send slope table for motor movement
 * Send slope_table in machine byte order
 * @param dev device to send slope table
 * @param table_nr index of the slope table in ASIC memory
 * Must be in the [0-4] range.
 * @param slope_table pointer to 16 bit values array of the slope table
 * @param steps number of elements in the slope table
 */
static void gl847_send_slope_table(Genesys_Device* dev, int table_nr, uint16_t* slope_table,
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
      sprintf (msg, "write slope %d (%d)=", table_nr, steps);
      for (i = 0; i < steps; i++)
	{
	  sprintf (msg+strlen(msg), "%d", slope_table[i]);
	}
      DBG (DBG_io, "%s: %s\n", __func__, msg);
    }

    // slope table addresses are fixed
    sanei_genesys_write_ahb(dev, 0x10000000 + 0x4000 * table_nr, steps * 2, table.data());
}

/**
 * Set register values of Analog Device type frontend
 * */
static void gl847_set_ad_fe(Genesys_Device* dev, uint8_t set)
{
    DBG_HELPER(dbg);
  int i;
  uint8_t val8;

    // wait for FE to be ready
    sanei_genesys_get_status(dev, &val8);
  while (val8 & REG41_FEBUSY)
    {
      sanei_genesys_sleep_ms(10);
        sanei_genesys_get_status(dev, &val8);
    };

  if (set == AFE_INIT)
    {
      DBG(DBG_proc, "%s(): setting DAC %u\n", __func__, dev->model->dac_type);

      dev->frontend = dev->frontend_initial;
    }

    // reset DAC
    sanei_genesys_fe_write_data(dev, 0x00, 0x80);

    // write them to analog frontend
    sanei_genesys_fe_write_data(dev, 0x00, dev->frontend.regs.get_value(0x00));

    sanei_genesys_fe_write_data(dev, 0x01, dev->frontend.regs.get_value(0x01));

    for (i = 0; i < 3; i++) {
        sanei_genesys_fe_write_data(dev, 0x02 + i, dev->frontend.get_gain(i));
    }
    for (i = 0; i < 3; i++) {
        sanei_genesys_fe_write_data(dev, 0x05 + i, dev->frontend.get_offset(i));
    }
}

static void gl847_homsnr_gpio(Genesys_Device* dev)
{
    DBG_HELPER(dbg);
uint8_t val;

    if (dev->model->gpo_type == GPO_CANONLIDE700) {
        sanei_genesys_read_register(dev, REG6C, &val);
        val &= ~REG6C_GPIO10;
        sanei_genesys_write_register(dev, REG6C, val);
    } else {
        sanei_genesys_read_register(dev, REG6C, &val);
        val |= REG6C_GPIO10;
        sanei_genesys_write_register(dev, REG6C, val);
    }
}

// Set values of analog frontend
static void gl847_set_fe(Genesys_Device* dev, const Genesys_Sensor& sensor, uint8_t set)
{
    DBG_HELPER_ARGS(dbg, "%s", set == AFE_INIT ? "init" :
                               set == AFE_SET ? "set" :
                               set == AFE_POWER_SAVE ? "powersave" : "huh?");

    (void) sensor;

    uint8_t val;
    sanei_genesys_read_register(dev, REG04, &val);
    uint8_t frontend_type = val & REG04_FESET;

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
                                       unsigned int scan_exposure_time,
                                       float scan_yres,
                                       int scan_step_type,
                                       unsigned int scan_lines,
                                       unsigned int scan_dummy,
                                       unsigned int feed_steps,
                                       int scan_power_mode,
                                       unsigned int flags)
{
    DBG_HELPER_ARGS(dbg, "scan_exposure_time=%d, can_yres=%g, scan_step_type=%d, scan_lines=%d, "
                         "scan_dummy=%d, feed_steps=%d, scan_power_mode=%d, flags=%x",
                    scan_exposure_time, scan_yres, scan_step_type, scan_lines, scan_dummy,
                    feed_steps, scan_power_mode, flags);
  int use_fast_fed;
  unsigned int fast_dpi;
  uint16_t scan_table[SLOPE_TABLE_SIZE];
  uint16_t fast_table[SLOPE_TABLE_SIZE];
  int scan_steps, fast_steps, factor;
  unsigned int feedl, dist;
  GenesysRegister *r;
  uint32_t z1, z2;
  unsigned int min_restep = 0x20;
  uint8_t val, effective;
  int fast_step_type;
  unsigned int ccdlmt,tgtime;

  /* get step multiplier */
  factor = gl847_get_step_multiplier (reg);

  use_fast_fed=0;
  /* no fast fed since feed works well */
  if(dev->settings.yres==4444 && feed_steps>100
     && ((flags & MOTOR_FLAG_FEED)==0))
    {
      use_fast_fed=1;
    }
  DBG(DBG_io, "%s: use_fast_fed=%d\n", __func__, use_fast_fed);

  sanei_genesys_set_triple(reg, REG_LINCNT, scan_lines);
  DBG(DBG_io, "%s: lincnt=%d\n", __func__, scan_lines);

  /* compute register 02 value */
  r = sanei_genesys_get_address (reg, REG02);
  r->value = 0x00;
  sanei_genesys_set_motor_power(*reg, true);

  if (use_fast_fed)
    r->value |= REG02_FASTFED;
  else
    r->value &= ~REG02_FASTFED;

  if (flags & MOTOR_FLAG_AUTO_GO_HOME)
    r->value |= REG02_AGOHOME | REG02_NOTHOME;

  if ((flags & MOTOR_FLAG_DISABLE_BUFFER_FULL_MOVE)
      ||(scan_yres>=sensor.optical_res))
    {
      r->value |= REG02_ACDCDIS;
    }

  /* scan and backtracking slope table */
  sanei_genesys_slope_table(scan_table,
                            &scan_steps,
                            scan_yres,
                            scan_exposure_time,
                            dev->motor.base_ydpi,
                            scan_step_type,
                            factor,
                            dev->model->motor_type,
                            gl847_motors);
    gl847_send_slope_table(dev, SCAN_TABLE, scan_table, scan_steps * factor);
    gl847_send_slope_table(dev, BACKTRACK_TABLE, scan_table, scan_steps * factor);

  /* fast table */
  fast_dpi=sanei_genesys_get_lowest_ydpi(dev);
  fast_step_type=scan_step_type;
  if(scan_step_type>=2)
    {
      fast_step_type=2;
    }

  sanei_genesys_slope_table(fast_table,
                            &fast_steps,
                            fast_dpi,
                            scan_exposure_time,
                            dev->motor.base_ydpi,
                            fast_step_type,
                            factor,
                            dev->model->motor_type,
                            gl847_motors);

  /* manual override of high start value */
  fast_table[0]=fast_table[1];

    gl847_send_slope_table(dev, STOP_TABLE, fast_table, fast_steps * factor);
    gl847_send_slope_table(dev, FAST_TABLE, fast_table, fast_steps * factor);
    gl847_send_slope_table(dev, HOME_TABLE, fast_table, fast_steps * factor);

  /* correct move distance by acceleration and deceleration amounts */
  feedl=feed_steps;
  if (use_fast_fed)
    {
        feedl<<=fast_step_type;
        dist=(scan_steps+2*fast_steps)*factor;
        /* TODO read and decode REGAB */
        r = sanei_genesys_get_address (reg, 0x5e);
        dist += (r->value & 31);
        /* FEDCNT */
        r = sanei_genesys_get_address (reg, REG_FEDCNT);
        dist += r->value;
    }
  else
    {
      feedl<<=scan_step_type;
      dist=scan_steps*factor;
      if (flags & MOTOR_FLAG_FEED)
        dist *=2;
    }
  DBG(DBG_io2, "%s: scan steps=%d\n", __func__, scan_steps);
  DBG(DBG_io2, "%s: acceleration distance=%d\n", __func__, dist);

  /* check for overflow */
  if(dist<feedl)
    feedl -= dist;
  else
    feedl = 0;

  sanei_genesys_set_triple(reg,REG_FEEDL,feedl);
  DBG(DBG_io ,"%s: feedl=%d\n", __func__, feedl);

  r = sanei_genesys_get_address (reg, REG0C);
  ccdlmt=(r->value & REG0C_CCDLMT)+1;

  r = sanei_genesys_get_address (reg, REG1C);
  tgtime=1<<(r->value & REG1C_TGTIME);

    // hi res motor speed GPIO
    sanei_genesys_read_register(dev, REG6C, &effective);

  /* if quarter step, bipolar Vref2 */
  if (scan_step_type > 1)
    {
      if (scan_step_type < 3)
        {
          val = effective & ~REG6C_GPIO13;
        }
      else
        {
          val = effective | REG6C_GPIO13;
        }
    }
  else
    {
      val = effective;
    }
    sanei_genesys_write_register(dev, REG6C, val);

    // effective scan
    sanei_genesys_read_register(dev, REG6C, &effective);
  val = effective | REG6C_GPIO10;
    sanei_genesys_write_register(dev, REG6C, val);

  min_restep=scan_steps/2-1;
  if (min_restep < 1)
    min_restep = 1;
  r = sanei_genesys_get_address (reg, REG_FWDSTEP);
  r->value = min_restep;
  r = sanei_genesys_get_address (reg, REG_BWDSTEP);
  r->value = min_restep;

  sanei_genesys_calculate_zmode2(use_fast_fed,
			         scan_exposure_time*ccdlmt*tgtime,
				 scan_table,
				 scan_steps*factor,
				 feedl,
                                 min_restep*factor,
                                 &z1,
                                 &z2);

  DBG(DBG_info, "%s: z1 = %d\n", __func__, z1);
  sanei_genesys_set_triple(reg, REG60, z1 | (scan_step_type << (16+REG60S_STEPSEL)));

  DBG(DBG_info, "%s: z2 = %d\n", __func__, z2);
  sanei_genesys_set_triple(reg, REG63, z2 | (scan_step_type << (16+REG63S_FSTPSEL)));

  r = sanei_genesys_get_address (reg, 0x1e);
  r->value &= 0xf0;		/* 0 dummy lines */
  r->value |= scan_dummy;	/* dummy lines */

  r = sanei_genesys_get_address (reg, REG67);
  r->value = REG67_MTRPWM;

  r = sanei_genesys_get_address (reg, REG68);
  r->value = REG68_FASTPWM;

  r = sanei_genesys_get_address (reg, REG_STEPNO);
  r->value = scan_steps;

  r = sanei_genesys_get_address (reg, REG_FASTNO);
  r->value = scan_steps;

  r = sanei_genesys_get_address (reg, REG_FSHDEC);
  r->value = scan_steps;

  r = sanei_genesys_get_address (reg, REG_FMOVNO);
  r->value = fast_steps;

  r = sanei_genesys_get_address (reg, REG_FMOVDEC);
  r->value = fast_steps;
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
                                         int used_res, unsigned int start, unsigned int pixels,
                                         int channels, int depth, SANE_Bool half_ccd,
                                         ColorFilter color_filter, int flags)
{
    DBG_HELPER_ARGS(dbg, "exposure_time=%d, used_res=%d, start=%d, pixels=%d, channels=%d, "
                         "depth=%d, half_ccd=%d, flags=%x",
                    exposure_time, used_res, start, pixels, channels, depth, half_ccd, flags);
  unsigned int words_per_line;
    unsigned dpiset, dpihw, segnb, factor;
  unsigned int bytes;
  GenesysRegister *r;

    // resolution is divided according to ccd_pixels_per_system_pixel()
    unsigned ccd_pixels_per_system_pixel = sensor.ccd_pixels_per_system_pixel();
    DBG(DBG_io2, "%s: ccd_pixels_per_system_pixel=%d\n", __func__, ccd_pixels_per_system_pixel);

    // to manage high resolution device while keeping good low resolution scanning speed, we make
    // hardware dpi vary
    dpihw = sanei_genesys_compute_dpihw(dev, sensor, used_res * ccd_pixels_per_system_pixel);
  factor=sensor.optical_res/dpihw;
  DBG(DBG_io2, "%s: dpihw=%d (factor=%d)\n", __func__, dpihw, factor);

  /* sensor parameters */
  Sensor_Profile* sensor_profile=get_sensor_profile(dev->model->ccd_type, dpihw);
  gl847_setup_sensor(dev, sensor, reg, dpihw);
    dpiset = used_res * ccd_pixels_per_system_pixel;

    // start and end coordinate in optical dpi coordinates
    unsigned startx = start / ccd_pixels_per_system_pixel + sensor.CCD_start_xoffset;
    unsigned endx = startx + pixels / ccd_pixels_per_system_pixel;

  /* sensors are built from 600 dpi segments for LiDE 100/200
   * and 1200 dpi for the 700F */
  if (dev->model->flags & GENESYS_FLAG_SIS_SENSOR)
    {
      segnb=dpihw/600;
    }
  else
    {
      segnb=1;
    }

    // compute pixel coordinate in the given dpihw space, taking segments into account
    startx /= factor*segnb;
    endx /= factor*segnb;
  dev->len=endx-startx;
  dev->dist=0;
  dev->skip=0;

  /* in cas of multi-segments sensor, we have to add the witdh
   * of the sensor crossed by the scan area */
  if (dev->model->flags & GENESYS_FLAG_SIS_SENSOR && segnb>1)
    {
      dev->dist = sensor_profile->segcnt;
    }

  /* use a segcnt rounded to next even number */
  endx += ((dev->dist+1)&0xfffe)*(segnb-1);
    unsigned used_pixels = endx - startx;

    gl847_set_fe(dev, sensor, AFE_SET);

  /* enable shading */
  r = sanei_genesys_get_address (reg, REG01);
  r->value &= ~REG01_SCAN;
  r->value |= REG01_SHDAREA;
  if ((flags & OPTICAL_FLAG_DISABLE_SHADING) ||
      (dev->model->flags & GENESYS_FLAG_NO_CALIBRATION))
    {
      r->value &= ~REG01_DVDSET;
    }
  else
    {
      r->value |= REG01_DVDSET;
    }

  r = sanei_genesys_get_address (reg, REG03);
  r->value &= ~REG03_AVEENB;

    sanei_genesys_set_lamp_power(dev, sensor, *reg, !(flags & OPTICAL_FLAG_DISABLE_LAMP));

  /* BW threshold */
  r = sanei_genesys_get_address (reg, 0x2e);
  r->value = dev->settings.threshold;
  r = sanei_genesys_get_address (reg, 0x2f);
  r->value = dev->settings.threshold;

  /* monochrome / color scan */
  r = sanei_genesys_get_address (reg, REG04);
  switch (depth)
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
  if (channels == 1)
    {
      switch (color_filter)
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
    }
  else
    r->value |= 0x10;		/* mono */

  /* register 05 */
  r = sanei_genesys_get_address (reg, REG05);

  /* set up dpihw */
  r->value &= ~REG05_DPIHW;
  switch(dpihw)
    {
      case 600:
        r->value |= REG05_DPIHW_600;
        break;
      case 1200:
        r->value |= REG05_DPIHW_1200;
        break;
      case 2400:
        r->value |= REG05_DPIHW_2400;
        break;
      case 4800:
        r->value |= REG05_DPIHW_4800;
        break;
    }

  /* enable gamma tables */
  if (flags & OPTICAL_FLAG_DISABLE_GAMMA)
    r->value &= ~REG05_GMMENB;
  else
    r->value |= REG05_GMMENB;

  /* CIS scanners can do true gray by setting LEDADD */
  /* we set up LEDADD only when asked */
  if (dev->model->is_cis == SANE_TRUE)
    {
      r = sanei_genesys_get_address (reg, 0x87);
      r->value &= ~REG87_LEDADD;
      if (channels == 1 && (flags & OPTICAL_FLAG_ENABLE_LEDADD))
	{
	  r->value |= REG87_LEDADD;
	}
      /* RGB weighting
      r = sanei_genesys_get_address (reg, 0x01);
      r->value &= ~REG01_TRUEGRAY;
      if (channels == 1 && (flags & OPTICAL_FLAG_ENABLE_LEDADD))
	{
	  r->value |= REG01_TRUEGRAY;
	}*/
    }

  /* words(16bit) before gamma, conversion to 8 bit or lineart*/
  words_per_line = (used_pixels * dpiset) / dpihw;
  bytes=depth/8;
  if (depth == 1)
    {
      words_per_line = (words_per_line+7)/8 ;
      dev->len = (dev->len >> 3) + ((dev->len & 7) ? 1 : 0);
      dev->dist = (dev->dist >> 3) + ((dev->dist & 7) ? 1 : 0);
    }
  else
    {
      words_per_line *= bytes;
      dev->dist *= bytes;
      dev->len *= bytes;
    }

  dev->bpl = words_per_line;
  dev->cur=0;
  dev->segnb=segnb;
  dev->line_interp = 0;

  sanei_genesys_set_double(reg,REG_DPISET,dpiset);
  DBG (DBG_io2, "%s: dpiset used=%d\n", __func__, dpiset);

  sanei_genesys_set_double(reg,REG_STRPIXEL,startx);
  sanei_genesys_set_double(reg,REG_ENDPIXEL,endx);
  DBG (DBG_io2, "%s: startx=%d\n", __func__, startx);
  DBG (DBG_io2, "%s: endx  =%d\n", __func__, endx);

  DBG (DBG_io2, "%s: used_pixels=%d\n", __func__, used_pixels);
  DBG (DBG_io2, "%s: pixels     =%d\n", __func__, pixels);
  DBG (DBG_io2, "%s: depth      =%d\n", __func__, depth);
  DBG (DBG_io2, "%s: dev->bpl   =%lu\n", __func__, (unsigned long)dev->bpl);
  DBG (DBG_io2, "%s: dev->len   =%lu\n", __func__, (unsigned long)dev->len);
  DBG (DBG_io2, "%s: dev->dist  =%lu\n", __func__, (unsigned long)dev->dist);
  DBG (DBG_io2, "%s: dev->segnb =%lu\n", __func__, (unsigned long)dev->segnb);

  words_per_line *= channels;
  dev->wpl = words_per_line;

    dev->oe_buffer.clear();
    dev->oe_buffer.alloc(dev->wpl);

  /* MAXWD is expressed in 4 words unit */
  sanei_genesys_set_triple(reg, REG_MAXWD, (words_per_line >> 2));
  DBG(DBG_io2, "%s: words_per_line used=%d\n", __func__, words_per_line);

  sanei_genesys_set_double(reg, REG_LPERIOD, exposure_time);
  DBG(DBG_io2, "%s: exposure_time used=%d\n", __func__, exposure_time);

  r = sanei_genesys_get_address (reg, 0x34);
  r->value = sensor.dummy_pixel;
}

// set up registers for an actual scan this function sets up the scanner to scan in normal or single
// line mode
static void gl847_init_scan_regs(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                 Genesys_Register_Set* reg, SetupParams& params)
{
    DBG_HELPER(dbg);
    params.assert_valid();

  int used_res;
  int start, used_pixels;
  int bytes_per_line;
  int move;
  unsigned int lincnt;
  unsigned int oflags; /**> optical flags */
  unsigned int mflags; /**> motor flags */
  int exposure_time;
  int stagger;

  int slope_dpi = 0;
  int dummy = 0;
  int scan_step_type = 1;
  int scan_power_mode = 0;
  int max_shift;
  size_t requested_buffer_size, read_buffer_size;

  SANE_Bool half_ccd;		/* false: full CCD res is used, true, half max CCD res is used */
  int optical_res;

    debug_dump(DBG_info, params);

  /* we may have 2 domains for ccd: xres below or above half ccd max dpi */
  if (sensor.get_ccd_size_divisor_for_dpi(params.xres) > 1)
    {
      half_ccd = SANE_TRUE;
    }
  else
    {
      half_ccd = SANE_FALSE;
    }

  /* optical_res */
  optical_res = sensor.optical_res;
  if (half_ccd)
    optical_res /= 2;

  /* stagger */
  if ((!half_ccd) && (dev->model->flags & GENESYS_FLAG_STAGGERED_LINE))
    stagger = (4 * params.yres) / dev->motor.base_ydpi;
  else
    stagger = 0;
  DBG(DBG_info, "%s : stagger=%d lines\n", __func__, stagger);

  /* used_res */
  if (params.flags & SCAN_FLAG_USE_OPTICAL_RES)
    {
      used_res = optical_res;
    }
  else
    {
      /* resolution is choosen from a list */
      used_res = params.xres;
    }

  /* compute scan parameters values */
  /* pixels are allways given at full optical resolution */
  /* use detected left margin and fixed value */
  /* start */
  /* add x coordinates */
  start = params.startx;

  if (stagger > 0)
    start |= 1;

  /* compute correct pixels number */
  /* pixels */
  used_pixels = (params.pixels * optical_res) / params.xres;

  /* round up pixels number if needed */
  if (used_pixels * params.xres < params.pixels * optical_res)
    used_pixels++;

  dummy = 3-params.channels;

/* slope_dpi */
/* cis color scan is effectively a gray scan with 3 gray lines per color
   line and a FILTER of 0 */
  if (dev->model->is_cis)
    slope_dpi = params.yres * params.channels;
  else
    slope_dpi = params.yres;

  slope_dpi = slope_dpi * (1 + dummy);

  exposure_time = gl847_compute_exposure (dev, used_res);
  scan_step_type = sanei_genesys_compute_step_type(gl847_motors, dev->model->motor_type, exposure_time);

  DBG(DBG_info, "%s : exposure_time=%d pixels\n", __func__, exposure_time);
  DBG(DBG_info, "%s : scan_step_type=%d\n", __func__, scan_step_type);

/*** optical parameters ***/
  /* in case of dynamic lineart, we use an internal 8 bit gray scan
   * to generate 1 lineart data */
    if (params.flags & SCAN_FLAG_DYNAMIC_LINEART) {
        params.depth = 8;
    }

  /* we enable true gray for cis scanners only, and just when doing
   * scan since color calibration is OK for this mode
   */
  oflags = 0;
  if(params.flags & SCAN_FLAG_DISABLE_SHADING)
    oflags |= OPTICAL_FLAG_DISABLE_SHADING;
  if(params.flags & SCAN_FLAG_DISABLE_GAMMA)
    oflags |= OPTICAL_FLAG_DISABLE_GAMMA;
  if(params.flags & SCAN_FLAG_DISABLE_LAMP)
    oflags |= OPTICAL_FLAG_DISABLE_LAMP;

  if (dev->model->is_cis && dev->settings.true_gray)
    {
      oflags |= OPTICAL_FLAG_ENABLE_LEDADD;
    }

    gl847_init_optical_regs_scan(dev, sensor, reg, exposure_time, used_res, start, used_pixels,
                                 params.channels, params.depth, half_ccd, params.color_filter,
                                 oflags);

/*** motor parameters ***/

  /* max_shift */
  max_shift=sanei_genesys_compute_max_shift(dev,params.channels,params.yres,params.flags);

  /* lincnt */
  lincnt = params.lines + max_shift + stagger;

  /* add tl_y to base movement */
  move = params.starty;
  DBG(DBG_info, "%s: move=%d steps\n", __func__, move);

  mflags=0;
    if (params.flags & SCAN_FLAG_DISABLE_BUFFER_FULL_MOVE) {
        mflags |= MOTOR_FLAG_DISABLE_BUFFER_FULL_MOVE;
    }
    if (params.flags & SCAN_FLAG_FEEDING) {
        mflags |= MOTOR_FLAG_FEED;
  }

    gl847_init_motor_regs_scan(dev, sensor, reg, exposure_time, slope_dpi, scan_step_type,
                               dev->model->is_cis ? lincnt * params.channels : lincnt, dummy, move,
                               scan_power_mode, mflags);

  /*** prepares data reordering ***/

/* words_per_line */
  bytes_per_line = (used_pixels * used_res) / optical_res;
  bytes_per_line = (bytes_per_line * params.channels * params.depth) / 8;

  requested_buffer_size = 8 * bytes_per_line;

  read_buffer_size =
    2 * requested_buffer_size +
    ((max_shift + stagger) * used_pixels * params.channels * params.depth) / 8;

    dev->read_buffer.clear();
    dev->read_buffer.alloc(read_buffer_size);

    dev->lines_buffer.clear();
    dev->lines_buffer.alloc(read_buffer_size);

    dev->shrink_buffer.clear();
    dev->shrink_buffer.alloc(requested_buffer_size);

    dev->out_buffer.clear();
    dev->out_buffer.alloc((8 * params.pixels * params.channels * params.depth) / 8);

  dev->read_bytes_left = bytes_per_line * lincnt;

  DBG(DBG_info, "%s: physical bytes to read = %lu\n", __func__, (u_long) dev->read_bytes_left);
  dev->read_active = SANE_TRUE;

  dev->current_setup.params = params;
  dev->current_setup.pixels = (used_pixels * used_res) / optical_res;
  dev->current_setup.lines = lincnt;
  dev->current_setup.depth = params.depth;
  dev->current_setup.channels = params.channels;
  dev->current_setup.exposure_time = exposure_time;
  dev->current_setup.xres = used_res;
  dev->current_setup.yres = params.yres;
  dev->current_setup.ccd_size_divisor = half_ccd ? 2 : 1;
  dev->current_setup.stagger = stagger;
  dev->current_setup.max_shift = max_shift + stagger;

/* TODO: should this be done elsewhere? */
  /* scan bytes to send to the frontend */
  /* theory :
     target_size =
     (params.pixels * params.lines * channels * depth) / 8;
     but it suffers from integer overflow so we do the following:

     1 bit color images store color data byte-wise, eg byte 0 contains
     8 bits of red data, byte 1 contains 8 bits of green, byte 2 contains
     8 bits of blue.
     This does not fix the overflow, though.
     644mp*16 = 10gp, leading to an overflow
     -- pierre
   */

  dev->total_bytes_read = 0;
  if (params.depth == 1)
    dev->total_bytes_to_read =
      ((params.pixels * params.lines) / 8 +
       (((params.pixels * params.lines) % 8) ? 1 : 0)) *
      params.channels;
  else
    dev->total_bytes_to_read =
      params.pixels * params.lines * params.channels * (params.depth / 8);

  DBG(DBG_info, "%s: total bytes to send = %lu\n", __func__, (u_long) dev->total_bytes_to_read);
/* END TODO */
}

static void
gl847_calculate_current_setup(Genesys_Device * dev, const Genesys_Sensor& sensor)
{
  int channels;
  int depth;
  int start;

  int used_res;
  int used_pixels;
  unsigned int lincnt;
  int exposure_time;
  int stagger;

  int slope_dpi = 0;
  int dummy = 0;
  int max_shift;

  SANE_Bool half_ccd;		/* false: full CCD res is used, true, half max CCD res is used */
  int optical_res;

    DBG(DBG_info, "%s ", __func__);
    debug_dump(DBG_info, dev->settings);

  /* channels */
  if (dev->settings.scan_mode == ScanColorMode::COLOR_SINGLE_PASS)
    channels = 3;
  else
    channels = 1;

  /* depth */
  depth = dev->settings.depth;
  if (dev->settings.scan_mode == ScanColorMode::LINEART)
    depth = 1;

  /* start */
  start = SANE_UNFIX (dev->model->x_offset);
  start += dev->settings.tl_x;
  start = (start * sensor.optical_res) / MM_PER_INCH;

    SetupParams params;
    params.xres = dev->settings.xres;
    params.yres = dev->settings.yres;
    params.startx = start; // not used
    params.starty = 0; // not used
    params.pixels = dev->settings.pixels;
    params.lines = dev->settings.lines;
    params.depth = depth;
    params.channels = channels;
    params.scan_method = dev->settings.scan_method;
    params.scan_mode = dev->settings.scan_mode;
    params.color_filter = dev->settings.color_filter;
    params.flags = 0;

    DBG(DBG_info, "%s ", __func__);
    debug_dump(DBG_info, params);

/* half_ccd */
  /* we have 2 domains for ccd: xres below or above half ccd max dpi */
    if (sensor.get_ccd_size_divisor_for_dpi(params.xres) > 1) {
        half_ccd = SANE_TRUE;
    } else {
        half_ccd = SANE_FALSE;
    }

  /* optical_res */
  optical_res = sensor.optical_res;

  /* stagger */
  if (dev->model->flags & GENESYS_FLAG_STAGGERED_LINE)
    stagger = (4 * params.yres) / dev->motor.base_ydpi;
  else
    stagger = 0;
  DBG(DBG_info, "%s: stagger=%d lines\n", __func__, stagger);

  /* resolution is choosen from a fixed list */
  used_res = params.xres;

  /* compute scan parameters values */
  /* pixels are allways given at half or full CCD optical resolution */
  /* use detected left margin  and fixed value */

  /* compute correct pixels number */
  used_pixels = (params.pixels * optical_res) / used_res;
  dummy = 3 - params.channels;

  /* slope_dpi */
  /* cis color scan is effectively a gray scan with 3 gray lines per color
   line and a FILTER of 0 */
    if (dev->model->is_cis) {
        slope_dpi = params.yres * params.channels;
    } else {
        slope_dpi = params.yres;
    }

  slope_dpi = slope_dpi * (1 + dummy);

  exposure_time = gl847_compute_exposure (dev, used_res);
  DBG(DBG_info, "%s : exposure_time=%d pixels\n", __func__, exposure_time);

  /* max_shift */
  max_shift = sanei_genesys_compute_max_shift(dev, params.channels, params.yres, 0);

  /* lincnt */
  lincnt = params.lines + max_shift + stagger;

  dev->current_setup.params = params;
  dev->current_setup.pixels = (used_pixels * used_res) / optical_res;
  dev->current_setup.lines = lincnt;
  dev->current_setup.depth = params.depth;
  dev->current_setup.channels = params.channels;
  dev->current_setup.exposure_time = exposure_time;
  dev->current_setup.xres = used_res;
  dev->current_setup.yres = params.yres;
  dev->current_setup.ccd_size_divisor = half_ccd ? 2 : 1;
  dev->current_setup.stagger = stagger;
  dev->current_setup.max_shift = max_shift + stagger;
}

// for fast power saving methods only, like disabling certain amplifiers
static void gl847_save_power(Genesys_Device* dev, SANE_Bool enable)
{
    DBG_HELPER_ARGS(dbg, "enable = %d", enable);
    (void) dev;
}

static void gl847_set_powersaving(Genesys_Device* dev, int delay /* in minutes */)
{
    (void) dev;
    DBG_HELPER_ARGS(dbg, "delay = %d", delay);
}

static void gl847_start_action(Genesys_Device* dev)
{
    DBG_HELPER(dbg);
    sanei_genesys_write_register(dev, 0x0f, 0x01);
}

static void gl847_stop_action(Genesys_Device* dev)
{
    DBG_HELPER(dbg);
  uint8_t val40, val;
  unsigned int loop;

    // post scan gpio : without that HOMSNR is unreliable
    gl847_homsnr_gpio(dev);
  sanei_genesys_get_status(dev, &val);
  if (DBG_LEVEL >= DBG_io)
    {
      sanei_genesys_print_status (val);
    }

    sanei_genesys_read_register(dev, REG40, &val40);

  /* only stop action if needed */
  if (!(val40 & REG40_DATAENB) && !(val40 & REG40_MOTMFLG))
    {
      DBG(DBG_info, "%s: already stopped\n", __func__);
      return;
    }

  /* ends scan */
  val = dev->reg.get8(REG01);
  val &= ~REG01_SCAN;
  sanei_genesys_set_reg_from_set(&dev->reg, REG01, val);
    sanei_genesys_write_register(dev, REG01, val);

  sanei_genesys_sleep_ms(100);

  loop = 10;
  while (loop > 0)
    {
        sanei_genesys_get_status(dev, &val);
      if (DBG_LEVEL >= DBG_io)
	{
	  sanei_genesys_print_status (val);
	}
            sanei_genesys_read_register(dev, REG40, &val40);

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

// Send the low-level scan command
static void gl847_begin_scan(Genesys_Device* dev, const Genesys_Sensor& sensor,
                             Genesys_Register_Set* reg, SANE_Bool start_motor)
{
    DBG_HELPER(dbg);
    (void) sensor;
  uint8_t val;
  GenesysRegister *r;

    // clear GPIO 10
    if (dev->model->gpo_type != GPO_CANONLIDE700) {
        sanei_genesys_read_register(dev, REG6C, &val);
      val &= ~REG6C_GPIO10;
        sanei_genesys_write_register(dev, REG6C, val);
    }

  val = REG0D_CLRLNCNT;
    sanei_genesys_write_register(dev, REG0D, val);
  val = REG0D_CLRMCNT;
    sanei_genesys_write_register(dev, REG0D, val);

    sanei_genesys_read_register(dev, REG01, &val);
  val |= REG01_SCAN;
    sanei_genesys_write_register(dev, REG01, val);
  r = sanei_genesys_get_address (reg, REG01);
  r->value = val;

    if (start_motor) {
        sanei_genesys_write_register(dev, REG0F, 1);
    } else {
        sanei_genesys_write_register(dev, REG0F, 0);
    }
}


// Send the stop scan command
static void gl847_end_scan(Genesys_Device* dev, Genesys_Register_Set* reg, SANE_Bool check_stop)
{
    (void) reg;
    DBG_HELPER_ARGS(dbg, "check_stop = %d", check_stop);

    if (dev->model->is_sheetfed != SANE_TRUE) {
        gl847_stop_action(dev);
    }
}

/** rewind scan
 * Move back by the same amount of distance than previous scan.
 * @param dev device to rewind
 * @returns SANE_STATUS_GOOD on success
 */
#if 0                           /* disabled to fix #7 */
static
SANE_Status gl847_rewind(Genesys_Device * dev)
{
    DBG_HELPER(dbg);
  SANE_Status status = SANE_STATUS_GOOD;
  uint8_t byte;


    // set motor reverse
    sanei_genesys_read_register(dev, 0x02, &byte);
  byte |= 0x04;
    sanei_genesys_write_register(dev, 0x02, byte);

    // and start scan, then wait completion
    gl847_begin_scan(dev, dev->reg, SANE_TRUE);
  do
    {
      sanei_genesys_sleep_ms(100);
        sanei_genesys_read_register(dev, REG40, &byte);
    }
  while(byte & REG40_MOTMFLG);
    gl847_end_scan(dev, dev->reg, SANE_TRUE);

    // restore direction
    sanei_genesys_read_register(dev, 0x02, &byte);
  byte &= 0xfb;
    sanei_genesys_write_register(dev, 0x02, byte);

  return SANE_STATUS_GOOD;
}
#endif

/** Park head
 * Moves the slider to the home (top) position slowly
 * @param dev device to park
 * @param wait_until_home true to make the function waiting for head
 * to be home before returning, if fals returne immediately
 * @returns SANE_STATUS_GOO on success */
static void gl847_slow_back_home(Genesys_Device* dev, SANE_Bool wait_until_home)
{
    DBG_HELPER_ARGS(dbg, "wait_until_home = %d", wait_until_home);
  Genesys_Register_Set local_reg;
  GenesysRegister *r;
  float resolution;
  uint8_t val;
  int loop = 0;
  ScanColorMode scan_mode;

    // post scan gpio : without that HOMSNR is unreliable
    gl847_homsnr_gpio(dev);

    // first read gives HOME_SENSOR true
    sanei_genesys_get_status(dev, &val);

  if (DBG_LEVEL >= DBG_io)
    {
      sanei_genesys_print_status (val);
    }
  sanei_genesys_sleep_ms(100);

    // second is reliable
    sanei_genesys_get_status(dev, &val);

  if (DBG_LEVEL >= DBG_io)
    {
      sanei_genesys_print_status (val);
    }

  /* is sensor at home? */
  if (val & HOMESNR)
    {
      DBG(DBG_info, "%s: already at home, completed\n", __func__);
      dev->scanhead_position_in_steps = 0;
        return;
    }

  local_reg = dev->reg;

  resolution=sanei_genesys_get_lowest_ydpi(dev);

  const auto& sensor = sanei_genesys_find_sensor_any(dev);

  /* TODO add scan_mode to the API */
  scan_mode = dev->settings.scan_mode;
  dev->settings.scan_mode = ScanColorMode::LINEART;

    SetupParams params;
    params.xres = resolution;
    params.yres = resolution;
    params.startx = 100;
    params.starty = 30000;
    params.pixels = 100;
    params.lines = 100;
    params.depth = 8;
    params.channels = 1;
    params.scan_method = dev->settings.scan_method;
    params.scan_mode = ScanColorMode::GRAY;
    params.color_filter = ColorFilter::RED;
    params.flags = SCAN_FLAG_DISABLE_SHADING |
                   SCAN_FLAG_DISABLE_GAMMA |
                   SCAN_FLAG_IGNORE_LINE_DISTANCE;

    gl847_init_scan_regs(dev, sensor, &local_reg, params);

  dev->settings.scan_mode = scan_mode;

    // clear scan and feed count
    sanei_genesys_write_register(dev, REG0D, REG0D_CLRLNCNT | REG0D_CLRMCNT);

  /* set up for reverse */
  r = sanei_genesys_get_address (&local_reg, REG02);
  r->value |= REG02_MTRREV;

    dev->model->cmd_set->bulk_write_register(dev, local_reg);

    try {
        gl847_start_action(dev);
    } catch (...) {
        try {
            gl847_stop_action(dev);
        } catch (...) {}
        // restore original registers
        catch_all_exceptions(__func__, [&]()
        {
            dev->model->cmd_set->bulk_write_register(dev, dev->reg);
        });
        throw;
    }

    // post scan gpio : without that HOMSNR is unreliable
    gl847_homsnr_gpio(dev);

  if (wait_until_home)
    {
      while (loop < 300)	/* do not wait longer then 30 seconds */
	{
        sanei_genesys_get_status(dev, &val);

	  if (val & HOMESNR)	/* home sensor */
	    {
	      DBG(DBG_info, "%s: reached home position\n", __func__);
              gl847_stop_action (dev);
              dev->scanhead_position_in_steps = 0;
            return;
	    }
          sanei_genesys_sleep_ms(100);
	  ++loop;
	}

        // when we come here then the scanner needed too much time for this, so we better stop
        // the motor
        catch_all_exceptions(__func__, [&](){ gl847_stop_action(dev); });
        throw SaneException(SANE_STATUS_IO_ERROR, "timeout while waiting for scanhead to go home");
    }

  DBG(DBG_info, "%s: scanhead is still moving\n", __func__);
}

/* Automatically set top-left edge of the scan area by scanning a 200x200 pixels
   area at 600 dpi from very top of scanner */
static SANE_Status
gl847_search_start_position (Genesys_Device * dev)
{
    DBG_HELPER(dbg);
  int size;
  SANE_Status status = SANE_STATUS_GOOD;
  Genesys_Register_Set local_reg;
  int steps;

  int pixels = 600;
  int dpi = 300;

  local_reg = dev->reg;

  /* sets for a 200 lines * 600 pixels */
  /* normal scan with no shading */

  // FIXME: the current approach of doing search only for one resolution does not work on scanners
  // whith employ different sensors with potentially different settings.
    auto& sensor = sanei_genesys_find_sensor_for_write(dev, dpi, ScanMethod::FLATBED);

    SetupParams params;
    params.xres = dpi;
    params.yres = dpi;
    params.startx =  0;
    params.starty =  0; /*we should give a small offset here~60 steps */
    params.pixels = 600;
    params.lines = dev->model->search_lines;
    params.depth = 8;
    params.channels =  1;
    params.scan_method = dev->settings.scan_method;
    params.scan_mode = ScanColorMode::GRAY;
    params.color_filter = ColorFilter::GREEN;
    params.flags = SCAN_FLAG_DISABLE_SHADING |
                   SCAN_FLAG_DISABLE_GAMMA |
                   SCAN_FLAG_IGNORE_LINE_DISTANCE;

    gl847_init_scan_regs(dev, sensor, &local_reg, params);

    // send to scanner
    dev->model->cmd_set->bulk_write_register(dev, local_reg);

  size = pixels * dev->model->search_lines;

  std::vector<uint8_t> data(size);

    gl847_begin_scan(dev, sensor, &local_reg, SANE_TRUE);

        // waits for valid data
        do {
            sanei_genesys_test_buffer_empty(dev, &steps);
        } while (steps);

  /* now we're on target, we can read data */
  status = sanei_genesys_read_data_from_scanner(dev, data.data(), size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG(DBG_error, "%s: failed to read data: %s\n", __func__, sane_strstatus(status));
      return status;
    }

    if (DBG_LEVEL >= DBG_data) {
        sanei_genesys_write_pnm_file("gl847_search_position.pnm", data.data(), 8, 1, pixels,
                                     dev->model->search_lines);
    }

    gl847_end_scan(dev, &local_reg, SANE_TRUE);

  /* update regs to copy ASIC internal state */
  dev->reg = local_reg;

    // TODO: find out where sanei_genesys_search_reference_point stores information,
    // and use that correctly
    sanei_genesys_search_reference_point(dev, sensor, data.data(), 0, dpi, pixels,
                                         dev->model->search_lines);

  return SANE_STATUS_GOOD;
}

/*
 * sets up register for coarse gain calibration
 * todo: check it for scanners using it */
static SANE_Status
gl847_init_regs_for_coarse_calibration(Genesys_Device * dev, const Genesys_Sensor& sensor,
                                       Genesys_Register_Set& regs)
{
    DBG_HELPER(dbg);
  uint8_t channels;

  /* set line size */
  if (dev->settings.scan_mode == ScanColorMode::COLOR_SINGLE_PASS)
    channels = 3;
  else {
    channels = 1;
  }

    SetupParams params;
    params.xres = dev->settings.xres;
    params.yres = dev->settings.yres;
    params.startx = 0;
    params.starty = 0;
    params.pixels = sensor.optical_res / sensor.ccd_pixels_per_system_pixel();
    params.lines = 20;
    params.depth = 16;
    params.channels = channels;
    params.scan_method = dev->settings.scan_method;
    params.scan_mode = dev->settings.scan_mode;
    params.color_filter = dev->settings.color_filter;
    params.flags = SCAN_FLAG_DISABLE_SHADING |
                   SCAN_FLAG_DISABLE_GAMMA |
                   SCAN_FLAG_SINGLE_LINE |
                   SCAN_FLAG_IGNORE_LINE_DISTANCE;

    gl847_init_scan_regs(dev, sensor, &regs, params);

  DBG(DBG_info, "%s: optical sensor res: %d dpi, actual res: %d\n", __func__,
      sensor.optical_res / sensor.ccd_pixels_per_system_pixel(), dev->settings.xres);

    dev->model->cmd_set->bulk_write_register(dev, regs);

  return SANE_STATUS_GOOD;
}

/** @brief moves the slider to steps at motor base dpi
 * @param dev device to work on
 * @param steps number of steps to move in base_dpi line count
 * */
static void gl847_feed(Genesys_Device* dev, unsigned int steps)
{
    DBG_HELPER_ARGS(dbg, "steps=%d", steps);
  Genesys_Register_Set local_reg;
  GenesysRegister *r;
  float resolution;
  uint8_t val;

  local_reg = dev->reg;

  resolution=sanei_genesys_get_lowest_ydpi(dev);
    const auto& sensor = sanei_genesys_find_sensor(dev, resolution, ScanMethod::FLATBED);

    SetupParams params;
    params.xres = resolution;
    params.yres = resolution;
    params.startx = 0;
    params.starty = steps;
    params.pixels = 100;
    params.lines = 3;
    params.depth = 8;
    params.channels = 3;
    params.scan_method = dev->settings.scan_method;
    params.scan_mode = ScanColorMode::COLOR_SINGLE_PASS;
    params.color_filter = dev->settings.color_filter;
    params.flags = SCAN_FLAG_DISABLE_SHADING |
                   SCAN_FLAG_DISABLE_GAMMA |
                   SCAN_FLAG_FEEDING |
                   SCAN_FLAG_IGNORE_LINE_DISTANCE;

    gl847_init_scan_regs(dev, sensor, &local_reg, params);

  /* set exposure to zero */
  sanei_genesys_set_triple(&local_reg,REG_EXPR,0);
  sanei_genesys_set_triple(&local_reg,REG_EXPG,0);
  sanei_genesys_set_triple(&local_reg,REG_EXPB,0);

    // clear scan and feed count
    sanei_genesys_write_register(dev, REG0D, REG0D_CLRLNCNT);
    sanei_genesys_write_register(dev, REG0D, REG0D_CLRMCNT);

  /* set up for no scan */
  r = sanei_genesys_get_address(&local_reg, REG01);
  r->value &= ~REG01_SCAN;

    // send registers
    dev->model->cmd_set->bulk_write_register(dev, local_reg);

    try {
        gl847_start_action(dev);
    } catch (...) {
        try {
            gl847_stop_action(dev);
        } catch (...) {}
        try {
            // restore original registers
            dev->model->cmd_set->bulk_write_register(dev, dev->reg);
        } catch (...) {}
        throw;
    }

    // wait until feed count reaches the required value, but do not exceed 30s
    do {
        sanei_genesys_get_status(dev, &val);
    }
  while (!(val & FEEDFSH));

    // then stop scanning
    gl847_stop_action(dev);
}


/* init registers for shading calibration */
static SANE_Status
gl847_init_regs_for_shading(Genesys_Device * dev, const Genesys_Sensor& sensor,
                            Genesys_Register_Set& regs)
{
    DBG_HELPER(dbg);
  float move;

  dev->calib_channels = 3;

  /* initial calibration reg values */
  regs = dev->reg;

  dev->calib_resolution = sanei_genesys_compute_dpihw(dev, sensor, dev->settings.xres);
  dev->calib_total_bytes_to_read = 0;
  dev->calib_lines = dev->model->shading_lines;
  if(dev->calib_resolution==4800)
    dev->calib_lines *= 2;
  dev->calib_pixels = (sensor.sensor_pixels*dev->calib_resolution)/sensor.optical_res;
  DBG(DBG_io, "%s: calib_lines  = %d\n", __func__, (int)dev->calib_lines);
  DBG(DBG_io, "%s: calib_pixels = %d\n", __func__, (int)dev->calib_pixels);

  /* this is aworkaround insufficent distance for slope
   * motor acceleration TODO special motor slope for shading  */
  move=1;
  if(dev->calib_resolution<1200)
    {
      move=40;
    }

    SetupParams params;
    params.xres = dev->calib_resolution;
    params.yres = dev->calib_resolution;
    params.startx = 0;
    params.starty = move;
    params.pixels = dev->calib_pixels;
    params.lines = dev->calib_lines;
    params.depth = 16;
    params.channels = dev->calib_channels;
    params.scan_method = dev->settings.scan_method;
    params.scan_mode = ScanColorMode::COLOR_SINGLE_PASS;
    params.color_filter = dev->settings.color_filter;
    params.flags = SCAN_FLAG_DISABLE_SHADING |
                   SCAN_FLAG_DISABLE_GAMMA |
                   SCAN_FLAG_DISABLE_BUFFER_FULL_MOVE |
                   SCAN_FLAG_IGNORE_LINE_DISTANCE;

    gl847_init_scan_regs(dev, sensor, &regs, params);

    dev->model->cmd_set->bulk_write_register(dev, regs);

  /* we use GENESYS_FLAG_SHADING_REPARK */
  dev->scanhead_position_in_steps = 0;

  return SANE_STATUS_GOOD;
}

/** @brief set up registers for the actual scan
 */
static SANE_Status
gl847_init_regs_for_scan (Genesys_Device * dev, const Genesys_Sensor& sensor)
{
    DBG_HELPER(dbg);
  int channels;
  int flags;
  int depth;
  float move;
  int move_dpi;
  float start;

    debug_dump(DBG_info, dev->settings);

 /* channels */
  if (dev->settings.scan_mode == ScanColorMode::COLOR_SINGLE_PASS)
    channels = 3;
  else
    channels = 1;

  /* depth */
  depth = dev->settings.depth;
  if (dev->settings.scan_mode == ScanColorMode::LINEART)
    depth = 1;


  /* steps to move to reach scanning area:
     - first we move to physical start of scanning
     either by a fixed steps amount from the black strip
     or by a fixed amount from parking position,
     minus the steps done during shading calibration
     - then we move by the needed offset whitin physical
     scanning area

     assumption: steps are expressed at maximum motor resolution

     we need:
     SANE_Fixed y_offset;
     SANE_Fixed y_size;
     SANE_Fixed y_offset_calib;
     mm_to_steps()=motor dpi / 2.54 / 10=motor dpi / MM_PER_INCH */

  /* if scanner uses GENESYS_FLAG_SEARCH_START y_offset is
     relative from origin, else, it is from parking position */

  move_dpi = dev->motor.base_ydpi;

  move = SANE_UNFIX (dev->model->y_offset);
  move += dev->settings.tl_y;
  move = (move * move_dpi) / MM_PER_INCH;
  move -= dev->scanhead_position_in_steps;
  DBG(DBG_info, "%s: move=%f steps\n", __func__, move);

  /* fast move to scan area */
  /* we don't move fast the whole distance since it would involve
   * computing acceleration/deceleration distance for scan
   * resolution. So leave a remainder for it so scan makes the final
   * move tuning */
  if(channels*dev->settings.yres>=600 && move>700)
    {
        gl847_feed(dev, move-500);
      move=500;
    }

  DBG(DBG_info, "%s: move=%f steps\n", __func__, move);
  DBG(DBG_info, "%s: move=%f steps\n", __func__, move);

  /* start */
  start = SANE_UNFIX (dev->model->x_offset);
  start += dev->settings.tl_x;
  start = (start * sensor.optical_res) / MM_PER_INCH;

  flags = 0;

  /* emulated lineart from gray data is required for now */
  if(dev->settings.scan_mode == ScanColorMode::LINEART
     && dev->settings.dynamic_lineart)
    {
      flags |= SCAN_FLAG_DYNAMIC_LINEART;
    }

  /* backtracking isn't handled well, so don't enable it */
  flags |= SCAN_FLAG_DISABLE_BUFFER_FULL_MOVE;

    SetupParams params;
    params.xres = dev->settings.xres;
    params.yres = dev->settings.yres;
    params.startx = start;
    params.starty = move;
    params.pixels = dev->settings.pixels;
    params.lines = dev->settings.lines;
    params.depth = depth;
    params.channels = channels;
    params.scan_method = dev->settings.scan_method;
    params.scan_mode = dev->settings.scan_mode;
    params.color_filter = dev->settings.color_filter;
    params.flags = flags;

    gl847_init_scan_regs(dev, sensor, &dev->reg, params);

  return SANE_STATUS_GOOD;
}


/**
 * Send shading calibration data. The buffer is considered to always hold values
 * for all the channels.
 */
static void gl847_send_shading_data(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                    uint8_t* data, int size)
{
    DBG_HELPER_ARGS(dbg, "writing %d bytes of shading data", size);
  uint32_t addr, length, i, x, factor, pixels;
  uint32_t dpiset, dpihw, strpixel, endpixel;
  uint16_t tempo;
  uint32_t lines, channels;
  uint8_t val,*ptr,*src;

  /* shading data is plit in 3 (up to 5 with IR) areas
     write(0x10014000,0x00000dd8)
     URB 23429  bulk_out len  3544  wrote 0x33 0x10 0x....
     write(0x1003e000,0x00000dd8)
     write(0x10068000,0x00000dd8)
   */
  length = (uint32_t) (size / 3);
  sanei_genesys_get_double(&dev->reg,REG_STRPIXEL,&tempo);
  strpixel=tempo;
  sanei_genesys_get_double(&dev->reg,REG_ENDPIXEL,&tempo);
  endpixel=tempo;

  /* compute deletion factor */
  sanei_genesys_get_double(&dev->reg,REG_DPISET,&tempo);
  dpiset=tempo;
  DBG(DBG_io2, "%s: STRPIXEL=%d, ENDPIXEL=%d, PIXELS=%d, DPISET=%d\n", __func__, strpixel, endpixel,
      endpixel-strpixel, dpiset);
  dpihw=sanei_genesys_compute_dpihw(dev, sensor, dpiset);
  factor=dpihw/dpiset;
  DBG(DBG_io2, "%s: factor=%d\n", __func__, factor);

  if(DBG_LEVEL>=DBG_data)
    {
      dev->binary=fopen("binary.pnm","wb");
      sanei_genesys_get_triple(&dev->reg, REG_LINCNT, &lines);
      channels=dev->current_setup.channels;
      if(dev->binary!=NULL)
        {
          fprintf(dev->binary,"P5\n%d %d\n%d\n",(endpixel-strpixel)/factor*channels,lines/channels,255);
        }
    }

  pixels=endpixel-strpixel;

  /* since we're using SHDAREA, substract startx coordinate from shading */
  strpixel-=((sensor.CCD_start_xoffset*600)/sensor.optical_res);

  /* turn pixel value into bytes 2x16 bits words */
  strpixel*=2*2;
  pixels*=2*2;

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

            sanei_genesys_read_register(dev, 0xd0+i, &val);
      addr = val * 8192 + 0x10000000;
        sanei_genesys_write_ahb(dev, addr, pixels, buffer.data());
    }
}

/** @brief calibrates led exposure
 * Calibrate exposure by scanning a white area until the used exposure gives
 * data white enough.
 * @param dev device to calibrate
 */
static SANE_Status
gl847_led_calibration (Genesys_Device * dev, Genesys_Sensor& sensor, Genesys_Register_Set& regs)
{
    DBG_HELPER(dbg);
  int num_pixels;
  int total_size;
  int used_res;
  int i, j;
  SANE_Status status = SANE_STATUS_GOOD;
  int val;
  int channels, depth;
  int avg[3], top[3], bottom[3];
  int turn;
  uint16_t exp[3];
  float move;
  SANE_Bool acceptable;

  move = SANE_UNFIX (dev->model->y_offset_calib);
  move = (move * (dev->motor.base_ydpi/4)) / MM_PER_INCH;
  if(move>20)
    {
        gl847_feed(dev, move);
    }
  DBG(DBG_io, "%s: move=%f steps\n", __func__, move);

  /* offset calibration is always done in color mode */
  channels = 3;
  depth=16;
  used_res=sanei_genesys_compute_dpihw(dev, sensor, dev->settings.xres);
  Sensor_Profile* sensor_profile=get_sensor_profile(dev->model->ccd_type, used_res);
  num_pixels = (sensor.sensor_pixels*used_res)/sensor.optical_res;

  /* initial calibration reg values */
  regs = dev->reg;

    SetupParams params;
    params.xres = used_res;
    params.yres = used_res;
    params.startx = 0;
    params.starty = 0;
    params.pixels = num_pixels;
    params.lines = 1;
    params.depth = depth;
    params.channels = channels;
    params.scan_method = dev->settings.scan_method;
    params.scan_mode = ScanColorMode::COLOR_SINGLE_PASS;
    params.color_filter = dev->settings.color_filter;
    params.flags = SCAN_FLAG_DISABLE_SHADING |
                   SCAN_FLAG_DISABLE_GAMMA |
                   SCAN_FLAG_SINGLE_LINE |
                   SCAN_FLAG_IGNORE_LINE_DISTANCE;

    gl847_init_scan_regs(dev, sensor, &regs, params);

  total_size = num_pixels * channels * (depth/8) * 1;	/* colors * bytes_per_color * scan lines */
  std::vector<uint8_t> line(total_size);

  /* initial loop values and boundaries */
  exp[0]=sensor_profile->expr;
  exp[1]=sensor_profile->expg;
  exp[2]=sensor_profile->expb;

  bottom[0]=29000;
  bottom[1]=29000;
  bottom[2]=29000;

  top[0]=41000;
  top[1]=51000;
  top[2]=51000;

  turn = 0;

  /* no move during led calibration */
  sanei_genesys_set_motor_power(regs, false);
  do
    {
      /* set up exposure */
      sanei_genesys_set_double(&regs,REG_EXPR,exp[0]);
      sanei_genesys_set_double(&regs,REG_EXPG,exp[1]);
      sanei_genesys_set_double(&regs,REG_EXPB,exp[2]);

        // write registers and scan data
        dev->model->cmd_set->bulk_write_register(dev, regs);

      DBG(DBG_info, "%s: starting line reading\n", __func__);
        gl847_begin_scan(dev, sensor, &regs, SANE_TRUE);
      RIE(sanei_genesys_read_data_from_scanner(dev, line.data(), total_size));

        // stop scanning
        gl847_stop_action(dev);

      if (DBG_LEVEL >= DBG_data)
	{
          char fn[30];
          snprintf(fn, 30, "gl847_led_%02d.pnm", turn);
          sanei_genesys_write_pnm_file(fn, line.data(), depth, channels, num_pixels, 1);
	}

      /* compute average */
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

      /* check if exposure gives average within the boundaries */
      acceptable = SANE_TRUE;
      for(i=0;i<3;i++)
        {
          if(avg[i]<bottom[i])
            {
              exp[i]=(exp[i]*bottom[i])/avg[i];
              acceptable = SANE_FALSE;
            }
          if(avg[i]>top[i])
            {
              exp[i]=(exp[i]*top[i])/avg[i];
              acceptable = SANE_FALSE;
            }
        }

      turn++;
    }
  while (!acceptable && turn < 100);

  DBG(DBG_info, "%s: acceptable exposure: %d,%d,%d\n", __func__, exp[0], exp[1], exp[2]);

  /* set these values as final ones for scan */
  sanei_genesys_set_double(&dev->reg,REG_EXPR,exp[0]);
  sanei_genesys_set_double(&dev->reg,REG_EXPG,exp[1]);
  sanei_genesys_set_double(&dev->reg,REG_EXPB,exp[2]);

  /* store in this struct since it is the one used by cache calibration */
  sensor.exposure.red = exp[0];
  sensor.exposure.green = exp[1];
  sensor.exposure.blue = exp[2];

    // go back home
    if (move>20) {
        gl847_slow_back_home(dev, SANE_TRUE);
    }

  return status;
}

/**
 * set up GPIO/GPOE for idle state
 */
static void gl847_init_gpio(Genesys_Device* dev)
{
    DBG_HELPER(dbg);
  int idx=0;

  /* search GPIO profile */
  while(gpios[idx].sensor_id!=0 && dev->model->gpo_type!=gpios[idx].sensor_id)
    {
      idx++;
    }
  if(gpios[idx].sensor_id==0)
    {
        throw SaneException("failed to find GPIO profile for sensor_id=%d", dev->model->ccd_type);
    }

    sanei_genesys_write_register(dev, REGA7, gpios[idx].ra7);
    sanei_genesys_write_register(dev, REGA6, gpios[idx].ra6);

    sanei_genesys_write_register(dev, REG6E, gpios[idx].r6e);
    sanei_genesys_write_register(dev, REG6C, 0x00);

    sanei_genesys_write_register(dev, REG6B, gpios[idx].r6b);
    sanei_genesys_write_register(dev, REG6C, gpios[idx].r6c);
    sanei_genesys_write_register(dev, REG6D, gpios[idx].r6d);
    sanei_genesys_write_register(dev, REG6E, gpios[idx].r6e);
    sanei_genesys_write_register(dev, REG6F, gpios[idx].r6f);

    sanei_genesys_write_register(dev, REGA8, gpios[idx].ra8);
    sanei_genesys_write_register(dev, REGA9, gpios[idx].ra9);
}

/**
 * set memory layout by filling values in dedicated registers
 */
static SANE_Status
gl847_init_memory_layout (Genesys_Device * dev)
{
    DBG_HELPER(dbg);
  SANE_Status status = SANE_STATUS_GOOD;
  int idx = 0;
  uint8_t val;

  /* point to per model memory layout */
  idx = 0;
  if (dev->model->model_id == MODEL_CANON_LIDE_100)
    {
      idx = 0;
    }
  if (dev->model->model_id == MODEL_CANON_LIDE_200)
    {
      idx = 1;
    }
  if (dev->model->model_id == MODEL_CANON_CANOSCAN_5600F)
    {
      idx = 2;
    }
  if (dev->model->model_id == MODEL_CANON_LIDE_700F)
    {
      idx = 3;
    }

  /* CLKSET nd DRAMSEL */
  val = layouts[idx].dramsel;
    sanei_genesys_write_register(dev, REG0B, val);
  dev->reg.find_reg(0x0b).value = val;

  /* prevent further writings by bulk write register */
  dev->reg.remove_reg(0x0b);

  /* setup base address for shading data. */
  /* values must be multiplied by 8192=0x4000 to give address on AHB */
  /* R-Channel shading bank0 address setting for CIS */
  sanei_genesys_write_register (dev, 0xd0, layouts[idx].rd0);
  /* G-Channel shading bank0 address setting for CIS */
  sanei_genesys_write_register (dev, 0xd1, layouts[idx].rd1);
  /* B-Channel shading bank0 address setting for CIS */
  sanei_genesys_write_register (dev, 0xd2, layouts[idx].rd2);

  /* setup base address for scanned data. */
  /* values must be multiplied by 1024*2=0x0800 to give address on AHB */
  /* R-Channel ODD image buffer 0x0124->0x92000 */
  /* size for each buffer is 0x16d*1k word */
  sanei_genesys_write_register (dev, 0xe0, layouts[idx].re0);
  sanei_genesys_write_register (dev, 0xe1, layouts[idx].re1);
  /* R-Channel ODD image buffer end-address 0x0291->0x148800 => size=0xB6800*/
  sanei_genesys_write_register (dev, 0xe2, layouts[idx].re2);
  sanei_genesys_write_register (dev, 0xe3, layouts[idx].re3);

  /* R-Channel EVEN image buffer 0x0292 */
  sanei_genesys_write_register (dev, 0xe4, layouts[idx].re4);
  sanei_genesys_write_register (dev, 0xe5, layouts[idx].re5);
  /* R-Channel EVEN image buffer end-address 0x03ff*/
  sanei_genesys_write_register (dev, 0xe6, layouts[idx].re6);
  sanei_genesys_write_register (dev, 0xe7, layouts[idx].re7);

  /* same for green, since CIS, same addresses */
  sanei_genesys_write_register (dev, 0xe8, layouts[idx].re0);
  sanei_genesys_write_register (dev, 0xe9, layouts[idx].re1);
  sanei_genesys_write_register (dev, 0xea, layouts[idx].re2);
  sanei_genesys_write_register (dev, 0xeb, layouts[idx].re3);
  sanei_genesys_write_register (dev, 0xec, layouts[idx].re4);
  sanei_genesys_write_register (dev, 0xed, layouts[idx].re5);
  sanei_genesys_write_register (dev, 0xee, layouts[idx].re6);
  sanei_genesys_write_register (dev, 0xef, layouts[idx].re7);

/* same for blue, since CIS, same addresses */
  sanei_genesys_write_register (dev, 0xf0, layouts[idx].re0);
  sanei_genesys_write_register (dev, 0xf1, layouts[idx].re1);
  sanei_genesys_write_register (dev, 0xf2, layouts[idx].re2);
  sanei_genesys_write_register (dev, 0xf3, layouts[idx].re3);
  sanei_genesys_write_register (dev, 0xf4, layouts[idx].re4);
  sanei_genesys_write_register (dev, 0xf5, layouts[idx].re5);
  sanei_genesys_write_register (dev, 0xf6, layouts[idx].re6);
  sanei_genesys_write_register (dev, 0xf7, layouts[idx].re7);

  return status;
}

/* *
 * initialize ASIC from power on condition
 */
static void gl847_boot(Genesys_Device* dev, SANE_Bool cold)
{
    DBG_HELPER(dbg);
  uint8_t val;

    // reset ASIC if cold boot
    if (cold) {
        sanei_genesys_write_register(dev, 0x0e, 0x01);
        sanei_genesys_write_register(dev, 0x0e, 0x00);
    }

    // test CHKVER
    sanei_genesys_read_register(dev, REG40, &val);
    if (val & REG40_CHKVER) {
        sanei_genesys_read_register(dev, 0x00, &val);
        DBG(DBG_info, "%s: reported version for genesys chip is 0x%02x\n", __func__, val);
    }

  /* Set default values for registers */
  gl847_init_registers (dev);

    // Write initial registers
    dev->model->cmd_set->bulk_write_register(dev, dev->reg);

  /* Enable DRAM by setting a rising edge on bit 3 of reg 0x0b */
  val = dev->reg.find_reg(0x0b).value & REG0B_DRAMSEL;
  val = (val | REG0B_ENBDRAM);
    sanei_genesys_write_register(dev, REG0B, val);
  dev->reg.find_reg(0x0b).value = val;

  /* CIS_LINE */
  SETREG (0x08, REG08_CIS_LINE);
    sanei_genesys_write_register(dev, 0x08, dev->reg.find_reg(0x08).value);

    // set up end access
    sanei_genesys_write_0x8c(dev, 0x10, 0x0b);
    sanei_genesys_write_0x8c(dev, 0x13, 0x0e);

    // setup gpio
    gl847_init_gpio(dev);

    // setup internal memory layout
    gl847_init_memory_layout (dev);

  SETREG (0xf8, 0x01);
    sanei_genesys_write_register(dev, 0xf8, dev->reg.find_reg(0xf8).value);
}

/**
 * initialize backend and ASIC : registers, motor tables, and gamma tables
 * then ensure scanner's head is at home
 */
static SANE_Status gl847_init (Genesys_Device * dev)
{
  SANE_Status status = SANE_STATUS_GOOD;

  DBG_INIT ();
    DBG_HELPER(dbg);

  status=sanei_genesys_asic_init(dev, 0);

  return status;
}

static SANE_Status
gl847_update_hardware_sensors (Genesys_Scanner * s)
{
    DBG_HELPER(dbg);
  /* do what is needed to get a new set of events, but try to not lose
     any of them.
   */
  SANE_Status status = SANE_STATUS_GOOD;
  uint8_t val;
  uint8_t scan, file, email, copy;
  switch(s->dev->model->gpo_type)
    {
      case GPO_CANONLIDE700:
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
    sanei_genesys_read_register(s->dev, REG6D, &val);

    s->buttons[BUTTON_SCAN_SW].write((val & scan) == 0);
    s->buttons[BUTTON_FILE_SW].write((val & file) == 0);
    s->buttons[BUTTON_EMAIL_SW].write((val & email) == 0);
    s->buttons[BUTTON_COPY_SW].write((val & copy) == 0);

  return status;
}

/** @brief search for a full width black or white strip.
 * This function searches for a black or white stripe across the scanning area.
 * When searching backward, the searched area must completely be of the desired
 * color since this area will be used for calibration which scans forward.
 * @param dev scanner device
 * @param forward SANE_TRUE if searching forward, SANE_FALSE if searching backward
 * @param black SANE_TRUE if searching for a black strip, SANE_FALSE for a white strip
 * @return SANE_STATUS_GOOD if a matching strip is found, SANE_STATUS_UNSUPPORTED if not
 */
static SANE_Status
gl847_search_strip (Genesys_Device * dev, const Genesys_Sensor& sensor,
                    SANE_Bool forward, SANE_Bool black)
{
    DBG_HELPER_ARGS(dbg, "%s %s", black ? "black" : "white", forward ? "forward" : "reverse");
  unsigned int pixels, lines, channels;
  SANE_Status status = SANE_STATUS_GOOD;
  Genesys_Register_Set local_reg;
  size_t size;
  int steps, depth;
  unsigned int pass, count, found, x, y;
  char title[80];
  GenesysRegister *r;

  gl847_set_fe(dev, sensor, AFE_SET);
    gl847_stop_action(dev);

    // set up for a gray scan at lowest dpi
    unsigned dpi = *std::min_element(dev->model->xdpi_values.begin(),
                                     dev->model->xdpi_values.end());
  channels = 1;
  /* 10 MM */
  /* lines = (10 * dpi) / MM_PER_INCH; */
  /* shading calibation is done with dev->motor.base_ydpi */
  lines = (dev->model->shading_lines * dpi) / dev->motor.base_ydpi;
  depth = 8;
  pixels = (sensor.sensor_pixels * dpi) / sensor.optical_res;
  size = pixels * channels * lines * (depth / 8);
  std::vector<uint8_t> data(size);
  dev->scanhead_position_in_steps = 0;

  local_reg = dev->reg;

    SetupParams params;
    params.xres = dpi;
    params.yres = dpi;
    params.startx = 0;
    params.starty = 0;
    params.pixels = pixels;
    params.lines = lines;
    params.depth = depth;
    params.channels = channels;
    params.scan_method = dev->settings.scan_method;
    params.scan_mode = ScanColorMode::GRAY;
    params.color_filter = ColorFilter::RED;
    params.flags = SCAN_FLAG_DISABLE_SHADING |
                   SCAN_FLAG_DISABLE_GAMMA;

    gl847_init_scan_regs(dev, sensor, &local_reg, params);

  /* set up for reverse or forward */
  r = sanei_genesys_get_address(&local_reg, REG02);
    if (forward) {
        r->value &= ~REG02_MTRREV;
    } else {
        r->value |= REG02_MTRREV;
    }

    dev->model->cmd_set->bulk_write_register(dev, local_reg);

    gl847_begin_scan(dev, sensor, &local_reg, SANE_TRUE);

        // waits for valid data
        do {
            sanei_genesys_test_buffer_empty(dev, &steps);
        } while (steps);

  /* now we're on target, we can read data */
  status = sanei_genesys_read_data_from_scanner(dev, data.data(), size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG(DBG_error, "%s: failed to read data: %s\n", __func__, sane_strstatus(status));
      return status;
    }

    gl847_stop_action(dev);

  pass = 0;
  if (DBG_LEVEL >= DBG_data)
    {
      sprintf(title, "gl847_search_strip_%s_%s%02d.pnm",
              black ? "black" : "white", forward ? "fwd" : "bwd", (int)pass);
      sanei_genesys_write_pnm_file(title, data.data(), depth, channels, pixels, lines);
    }

  /* loop until strip is found or maximum pass number done */
  found = 0;
  while (pass < 20 && !found)
    {
        dev->model->cmd_set->bulk_write_register(dev, local_reg);

            // now start scan
            gl847_begin_scan(dev, sensor, &local_reg, SANE_TRUE);

        // waits for valid data
        do {
            sanei_genesys_test_buffer_empty(dev, &steps);
        } while (steps);

      /* now we're on target, we can read data */
      status = sanei_genesys_read_data_from_scanner(dev, data.data(), size);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG(DBG_error, "%s: failed to read data: %s\n", __func__, sane_strstatus(status));
	  return status;
	}

    gl847_stop_action(dev);

      if (DBG_LEVEL >= DBG_data)
	{
          sprintf(title, "gl847_search_strip_%s_%s%02d.pnm",
                  black ? "black" : "white", forward ? "fwd" : "bwd", (int)pass);
          sanei_genesys_write_pnm_file(title, data.data(), depth, channels, pixels, lines);
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
      status = SANE_STATUS_GOOD;
      DBG(DBG_info, "%s: %s strip found\n", __func__, black ? "black" : "white");
    }
  else
    {
      status = SANE_STATUS_UNSUPPORTED;
      DBG(DBG_info, "%s: %s strip not found\n", __func__, black ? "black" : "white");
    }

  return status;
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

static SANE_Status
gl847_offset_calibration(Genesys_Device * dev, const Genesys_Sensor& sensor,
                         Genesys_Register_Set& regs)
{
    DBG_HELPER(dbg);
  SANE_Status status = SANE_STATUS_GOOD;
  uint8_t reg04;
  unsigned int channels, bpp;
  int pass = 0, avg, total_size;
  int topavg, bottomavg, resolution, lines;
  int top, bottom, black_pixels, pixels;

    // no gain nor offset for AKM AFE
    sanei_genesys_read_register(dev, REG04, &reg04);
  if ((reg04 & REG04_FESET) == 0x02)
    {
      return status;
    }

  /* offset calibration is always done in color mode */
  channels = 3;
  resolution=sensor.optical_res;
  dev->calib_pixels = sensor.sensor_pixels;
  lines=1;
  bpp=8;
  pixels= (sensor.sensor_pixels*resolution) / sensor.optical_res;
  black_pixels = (sensor.black_pixels * resolution) / sensor.optical_res;
  DBG(DBG_io2, "%s: black_pixels=%d\n", __func__, black_pixels);

    SetupParams params;
    params.xres = resolution;
    params.yres = resolution;
    params.startx = 0;
    params.starty = 0;
    params.pixels = pixels;
    params.lines = lines;
    params.depth = bpp;
    params.channels = channels;
    params.scan_method = dev->settings.scan_method;
    params.scan_mode = ScanColorMode::COLOR_SINGLE_PASS;
    params.color_filter = dev->settings.color_filter;
    params.flags = SCAN_FLAG_DISABLE_SHADING |
                   SCAN_FLAG_DISABLE_GAMMA |
                   SCAN_FLAG_SINGLE_LINE |
                   SCAN_FLAG_IGNORE_LINE_DISTANCE;

    gl847_init_scan_regs(dev, sensor, &regs, params);

  sanei_genesys_set_motor_power(regs, false);

  /* allocate memory for scans */
  total_size = pixels * channels * lines * (bpp/8);	/* colors * bytes_per_color * scan lines */

  std::vector<uint8_t> first_line(total_size);
  std::vector<uint8_t> second_line(total_size);

  /* init gain */
  dev->frontend.set_gain(0, 0);
  dev->frontend.set_gain(1, 0);
  dev->frontend.set_gain(2, 0);

  /* scan with no move */
  bottom = 10;
  dev->frontend.set_offset(0, bottom);
  dev->frontend.set_offset(1, bottom);
  dev->frontend.set_offset(2, bottom);

    gl847_set_fe(dev, sensor, AFE_SET);
    dev->model->cmd_set->bulk_write_register(dev, regs);
  DBG(DBG_info, "%s: starting first line reading\n", __func__);
    gl847_begin_scan(dev, sensor, &regs, SANE_TRUE);
  RIE(sanei_genesys_read_data_from_scanner(dev, first_line.data(), total_size));
  if (DBG_LEVEL >= DBG_data)
   {
      char fn[30];
      snprintf(fn, 30, "gl847_offset%03d.pnm", bottom);
      sanei_genesys_write_pnm_file(fn, first_line.data(), bpp, channels, pixels, lines);
   }

  bottomavg = dark_average (first_line.data(), pixels, lines, channels, black_pixels);
  DBG(DBG_io2, "%s: bottom avg=%d\n", __func__, bottomavg);

  /* now top value */
  top = 255;
  dev->frontend.set_offset(0, top);
  dev->frontend.set_offset(1, top);
  dev->frontend.set_offset(2, top);
    gl847_set_fe(dev, sensor, AFE_SET);
    dev->model->cmd_set->bulk_write_register(dev, regs);
  DBG(DBG_info, "%s: starting second line reading\n", __func__);
    gl847_begin_scan(dev, sensor, &regs, SANE_TRUE);
  RIE(sanei_genesys_read_data_from_scanner (dev, second_line.data(), total_size));

  topavg = dark_average(second_line.data(), pixels, lines, channels, black_pixels);
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
        gl847_set_fe(dev, sensor, AFE_SET);
        dev->model->cmd_set->bulk_write_register(dev, regs);
      DBG(DBG_info, "%s: starting second line reading\n", __func__);
        gl847_begin_scan(dev, sensor, &regs, SANE_TRUE);
      RIE(sanei_genesys_read_data_from_scanner (dev, second_line.data(), total_size));

      if (DBG_LEVEL >= DBG_data)
	{
          char fn[30];
          snprintf(fn, 30, "gl847_offset%03d.pnm", dev->frontend.get_offset(1));
          sanei_genesys_write_pnm_file(fn, second_line.data(), bpp, channels, pixels, lines);
	}

      avg = dark_average(second_line.data(), pixels, lines, channels, black_pixels);
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

  return SANE_STATUS_GOOD;
}

static SANE_Status
gl847_coarse_gain_calibration(Genesys_Device * dev, const Genesys_Sensor& sensor,
                              Genesys_Register_Set& regs, int dpi)
{
    DBG_HELPER_ARGS(dbg, "dpi = %d", dpi);
  int pixels;
  int total_size;
  uint8_t reg04;
  int i, j, channels;
  SANE_Status status = SANE_STATUS_GOOD;
  int max[3];
  float gain[3],coeff;
  int val, code, lines;
  int resolution;
  int bpp;

    // no gain nor offset for AKM AFE
    sanei_genesys_read_register(dev, REG04, &reg04);
  if ((reg04 & REG04_FESET) == 0x02)
    {
      return status;
    }

  /* coarse gain calibration is always done in color mode */
  channels = 3;

  /* follow CKSEL */
  if(dev->settings.xres<sensor.optical_res)
    {
      coeff=0.9;
      /*resolution=sensor.optical_res/2; */
      resolution=sensor.optical_res;
    }
  else
    {
      resolution=sensor.optical_res;
      coeff=1.0;
    }
  lines=10;
  bpp=8;
  pixels = (sensor.sensor_pixels * resolution) / sensor.optical_res;

    SetupParams params;
    params.xres = resolution;
    params.yres = resolution;
    params.startx = 0;
    params.starty = 0;
    params.pixels = pixels;
    params.lines = lines;
    params.depth = bpp;
    params.channels = channels;
    params.scan_method = dev->settings.scan_method;
    params.scan_mode = ScanColorMode::COLOR_SINGLE_PASS;
    params.color_filter = dev->settings.color_filter;
    params.flags = SCAN_FLAG_DISABLE_SHADING |
                   SCAN_FLAG_DISABLE_GAMMA |
                   SCAN_FLAG_SINGLE_LINE |
                   SCAN_FLAG_IGNORE_LINE_DISTANCE;

    try {
        gl847_init_scan_regs(dev, sensor, &regs, params);
    } catch (...) {
        catch_all_exceptions(__func__, [&](){ sanei_genesys_set_motor_power(regs, false); });
        throw;
    }

    sanei_genesys_set_motor_power(regs, false);

    dev->model->cmd_set->bulk_write_register(dev, regs);

  total_size = pixels * channels * (16/bpp) * lines;

  std::vector<uint8_t> line(total_size);

    gl847_set_fe(dev, sensor, AFE_SET);
    gl847_begin_scan(dev, sensor, &regs, SANE_TRUE);
  RIE(sanei_genesys_read_data_from_scanner(dev, line.data(), total_size));

  if (DBG_LEVEL >= DBG_data)
    sanei_genesys_write_pnm_file("gl847_gain.pnm", line.data(), bpp, channels, pixels, lines);

  /* average value on each channel */
  for (j = 0; j < channels; j++)
    {
      max[j] = 0;
      for (i = pixels/4; i < (pixels*3/4); i++)
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

	    max[j] += val;
	}
      max[j] = max[j] / (pixels/2);

      gain[j] = ((float) sensor.gain_white_ref*coeff) / max[j];

      /* turn logical gain value into gain code, checking for overflow */
      code = 283 - 208 / gain[j];
      if (code > 255)
	code = 255;
      else if (code < 0)
	code = 0;
      dev->frontend.set_gain(j, code);

      DBG(DBG_proc, "%s: channel %d, max=%d, gain = %f, setting:%d\n", __func__, j, max[j], gain[j],
          dev->frontend.get_gain(j));
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

    gl847_stop_action(dev);

    gl847_slow_back_home(dev, SANE_TRUE);

  return status;
}


/** the gl847 command set */
static Genesys_Command_Set gl847_cmd_set = {
  "gl847-generic",		/* the name of this set */

  nullptr,

  gl847_init,
  NULL, /*gl847_init_regs_for_warmup*/
  gl847_init_regs_for_coarse_calibration,
  gl847_init_regs_for_shading,
  gl847_init_regs_for_scan,

  gl847_get_filter_bit,
  gl847_get_lineart_bit,
  gl847_get_bitset_bit,
  gl847_get_gain4_bit,
  gl847_get_fast_feed_bit,
  gl847_test_buffer_empty_bit,
  gl847_test_motor_flag_bit,

  gl847_set_fe,
  gl847_set_powersaving,
  gl847_save_power,

  gl847_begin_scan,
  gl847_end_scan,

  sanei_genesys_send_gamma_table,

  gl847_search_start_position,

  gl847_offset_calibration,
  gl847_coarse_gain_calibration,
  gl847_led_calibration,

  NULL,
  gl847_slow_back_home,
  NULL, /* disable gl847_rewind, see #7 */

  sanei_genesys_bulk_write_register,
  NULL,
  sanei_genesys_bulk_read_data,

  gl847_update_hardware_sensors,

  NULL, /* no known gl847 sheetfed scanner */
  NULL, /* no known gl847 sheetfed scanner */
  NULL, /* no known gl847 sheetfed scanner */
  gl847_search_strip,

  sanei_genesys_is_compatible_calibration,
  NULL,
  gl847_send_shading_data,
  gl847_calculate_current_setup,
  gl847_boot
};

void sanei_gl847_init_cmd_set(Genesys_Device* dev)
{
  dev->model->cmd_set = &gl847_cmd_set;
}
