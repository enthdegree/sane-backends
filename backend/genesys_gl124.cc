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

#include "genesys_gl124.h"

#include <vector>

/****************************************************************************
 Mid level functions
 ****************************************************************************/

static SANE_Bool gl124_get_fast_feed_bit(Genesys_Register_Set* regs)
{
    return (bool)(regs->get8(REG02) & REG02_FASTFED);
}

static SANE_Bool gl124_get_filter_bit(Genesys_Register_Set* regs)
{
    return (bool)(regs->get8(REG04) & REG04_FILTER);
}

static SANE_Bool gl124_get_lineart_bit(Genesys_Register_Set* regs)
{
    return (bool)(regs->get8(REG04) & REG04_LINEART);
}

static SANE_Bool gl124_get_bitset_bit(Genesys_Register_Set* regs)
{
    return (bool)(regs->get8(REG04) & REG04_BITSET);
}

static SANE_Bool gl124_get_gain4_bit (Genesys_Register_Set * regs)
{
    return (bool)(regs->get8(REG06) & REG06_GAIN4);
}

static SANE_Bool
gl124_test_buffer_empty_bit (SANE_Byte val)
{
  if (val & BUFEMPTY)
    return SANE_TRUE;
  return SANE_FALSE;
}

static SANE_Bool
gl124_test_motor_flag_bit (SANE_Byte val)
{
  if (val & MOTORENB)
    return SANE_TRUE;
  return SANE_FALSE;
}

/** @brief sensor profile
 * search for the database of motor profiles and get the best one. Each
 * profile is at a specific dpihw. Use LiDE 110 table by default.
 * @param sensor_type sensor id
 * @param dpi hardware dpi for the scan
 * @param half_ccd flag to signal half ccd mode
 * @return a pointer to a Sensor_Profile struct
 */
static Sensor_Profile *get_sensor_profile(int sensor_type, int dpi, int half_ccd)
{
  unsigned int i;
  int idx;

  i=0;
  idx=-1;
  while(i<sizeof(sensors)/sizeof(Sensor_Profile))
    {
      /* exact match */
      if(sensors[i].sensor_type==sensor_type
         && sensors[i].dpi==dpi
         && sensors[i].half_ccd==half_ccd)
        {
          return &(sensors[i]);
        }

      /* closest match */
      if(sensors[i].sensor_type==sensor_type
        && sensors[i].half_ccd==half_ccd)
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


static SANE_Status
gl124_homsnr_gpio(Genesys_Device *dev)
{
uint8_t val;
SANE_Status status=SANE_STATUS_GOOD;

  RIE (sanei_genesys_read_register (dev, REG32, &val));
  val &= ~REG32_GPIO10;
  RIE (sanei_genesys_write_register (dev, REG32, val));
  return status;
}

/**@brief compute half ccd mode
 * Compute half CCD mode flag. Half CCD is on when dpiset it twice
 * the actual scanning resolution. Used for fast scans.
 * @param model pointer to device model
 * @param xres required horizontal resolution
 * @return SANE_TRUE if half CCD mode enabled
 */
static SANE_Bool compute_half_ccd(const Genesys_Sensor& sensor, int xres)
{
  /* we have 2 domains for ccd: xres below or above half ccd max dpi */
  if (xres<=300 && sensor.ccd_size_divisor > 1)
    {
      return SANE_TRUE;
    }
  return SANE_FALSE;
}

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
  DBGSTART;

  dev->reg.clear();

  /* default to LiDE 110 */
  SETREG (0x01,0xa2); /* + REG01_SHDAREA */
  SETREG (0x02,0x90);
  SETREG (0x03,0x50);
  SETREG (0x04,0x03);
  SETREG (0x05,0x00);
  if(dev->model->ccd_type==CIS_CANONLIDE120)
    {
      SETREG (0x06,0x50);
      SETREG (0x07,0x00);
    }
  else
    {
      SETREG (0x03,0x50 & ~REG03_AVEENB);
      SETREG (0x06,0x50 | REG06_GAIN4);
    }
  SETREG (0x09,0x00);
  SETREG (0x0a,0xc0);
  SETREG (0x0b,0x2a);
  SETREG (0x0c,0x12);
  SETREG (0x11,0x00);
  SETREG (0x12,0x00);
  SETREG (0x13,0x0f);
  SETREG (0x14,0x00);
  SETREG (0x15,0x80);
  SETREG (0x16,0x10);
  SETREG (0x17,0x04);
  SETREG (0x18,0x00);
  SETREG (0x19,0x01);
  SETREG (0x1a,0x30);
  SETREG (0x1b,0x00);
  SETREG (0x1c,0x00);
  SETREG (0x1d,0x01);
  SETREG (0x1e,0x10);
  SETREG (0x1f,0x00);
  SETREG (0x20,0x15);
  SETREG (0x21,0x00);
  if(dev->model->ccd_type!=CIS_CANONLIDE120)
    {
      SETREG (0x22,0x02);
    }
  else
    {
      SETREG (0x22,0x14);
    }
  SETREG (0x23,0x00);
  SETREG (0x24,0x00);
  SETREG (0x25,0x00);
  SETREG (0x26,0x0d);
  SETREG (0x27,0x48);
  SETREG (0x28,0x00);
  SETREG (0x29,0x56);
  SETREG (0x2a,0x5e);
  SETREG (0x2b,0x02);
  SETREG (0x2c,0x02);
  SETREG (0x2d,0x58);
  SETREG (0x3b,0x00);
  SETREG (0x3c,0x00);
  SETREG (0x3d,0x00);
  SETREG (0x3e,0x00);
  SETREG (0x3f,0x02);
  SETREG (0x40,0x00);
  SETREG (0x41,0x00);
  SETREG (0x42,0x00);
  SETREG (0x43,0x00);
  SETREG (0x44,0x00);
  SETREG (0x45,0x00);
  SETREG (0x46,0x00);
  SETREG (0x47,0x00);
  SETREG (0x48,0x00);
  SETREG (0x49,0x00);
  SETREG (0x4f,0x00);
  SETREG (0x52,0x00);
  SETREG (0x53,0x02);
  SETREG (0x54,0x04);
  SETREG (0x55,0x06);
  SETREG (0x56,0x04);
  SETREG (0x57,0x04);
  SETREG (0x58,0x04);
  SETREG (0x59,0x04);
  SETREG (0x5a,0x1a);
  SETREG (0x5b,0x00);
  SETREG (0x5c,0xc0);
  SETREG (0x5f,0x00);
  SETREG (0x60,0x02);
  SETREG (0x61,0x00);
  SETREG (0x62,0x00);
  SETREG (0x63,0x00);
  SETREG (0x64,0x00);
  SETREG (0x65,0x00);
  SETREG (0x66,0x00);
  SETREG (0x67,0x00);
  SETREG (0x68,0x00);
  SETREG (0x69,0x00);
  SETREG (0x6a,0x00);
  SETREG (0x6b,0x00);
  SETREG (0x6c,0x00);
  SETREG (0x6e,0x00);
  SETREG (0x6f,0x00);
  if(dev->model->ccd_type!=CIS_CANONLIDE120)
    {
      SETREG (0x6d,0xd0);
      SETREG (0x71,0x08);
    }
  else
    {
      SETREG (0x6d,0x00);
      SETREG (0x71,0x1f);
    }
  SETREG (0x70,0x00);
  SETREG (0x72,0x08);
  SETREG (0x73,0x0a);

  /* CKxMAP */
  SETREG (0x74,0x00);
  SETREG (0x75,0x00);
  SETREG (0x76,0x3c);
  SETREG (0x77,0x00);
  SETREG (0x78,0x00);
  SETREG (0x79,0x9f);
  SETREG (0x7a,0x00);
  SETREG (0x7b,0x00);
  SETREG (0x7c,0x55);

  SETREG (0x7d,0x00);
  SETREG (0x7e,0x08);
  SETREG (0x7f,0x58);
  if(dev->model->ccd_type!=CIS_CANONLIDE120)
    {
      SETREG (0x80,0x00);
      SETREG (0x81,0x14);
    }
  else
    {
      SETREG (0x80,0x00);
      SETREG (0x81,0x10);
    }

  /* STRPIXEL */
  SETREG (0x82,0x00);
  SETREG (0x83,0x00);
  SETREG (0x84,0x00);
  /* ENDPIXEL */
  SETREG (0x85,0x00);
  SETREG (0x86,0x00);
  SETREG (0x87,0x00);

  SETREG (0x88,0x00);
  SETREG (0x89,0x65);
  SETREG (0x8a,0x00);
  SETREG (0x8b,0x00);
  SETREG (0x8c,0x00);
  SETREG (0x8d,0x00);
  SETREG (0x8e,0x00);
  SETREG (0x8f,0x00);
  SETREG (0x90,0x00);
  SETREG (0x91,0x00);
  SETREG (0x92,0x00);
  SETREG (0x93,0x00);
  SETREG (0x94,0x14);
  SETREG (0x95,0x30);
  SETREG (0x96,0x00);
  SETREG (0x97,0x90);
  SETREG (0x98,0x01);
  SETREG (0x99,0x1f);
  SETREG (0x9a,0x00);
  SETREG (0x9b,0x80);
  SETREG (0x9c,0x80);
  SETREG (0x9d,0x3f);
  SETREG (0x9e,0x00);
  SETREG (0x9f,0x00);
  SETREG (0xa0,0x20);
  SETREG (0xa1,0x30);
  SETREG (0xa2,0x00);
  SETREG (0xa3,0x20);
  SETREG (0xa4,0x01);
  SETREG (0xa5,0x00);
  SETREG (0xa6,0x00);
  SETREG (0xa7,0x08);
  SETREG (0xa8,0x00);
  SETREG (0xa9,0x08);
  SETREG (0xaa,0x01);
  SETREG (0xab,0x00);
  SETREG (0xac,0x00);
  SETREG (0xad,0x40);
  SETREG (0xae,0x01);
  SETREG (0xaf,0x00);
  SETREG (0xb0,0x00);
  SETREG (0xb1,0x40);
  SETREG (0xb2,0x00);
  SETREG (0xb3,0x09);
  SETREG (0xb4,0x5b);
  SETREG (0xb5,0x00);
  SETREG (0xb6,0x10);
  SETREG (0xb7,0x3f);
  SETREG (0xb8,0x00);
  SETREG (0xbb,0x00);
  SETREG (0xbc,0xff);
  SETREG (0xbd,0x00);
  SETREG (0xbe,0x07);
  SETREG (0xc3,0x00);
  SETREG (0xc4,0x00);

  /* gamma
  SETREG (0xc5,0x00);
  SETREG (0xc6,0x00);
  SETREG (0xc7,0x00);
  SETREG (0xc8,0x00);
  SETREG (0xc9,0x00);
  SETREG (0xca,0x00);
  SETREG (0xcb,0x00);
  SETREG (0xcc,0x00);
  SETREG (0xcd,0x00);
  SETREG (0xce,0x00);
  */
  if(dev->model->ccd_type==CIS_CANONLIDE120)
    {
      SETREG (0xc5,0x20);
      SETREG (0xc6,0xeb);
      SETREG (0xc7,0x20);
      SETREG (0xc8,0xeb);
      SETREG (0xc9,0x20);
      SETREG (0xca,0xeb);
    }

  /* memory layout
  SETREG (0xd0,0x0a);
  SETREG (0xd1,0x1f);
  SETREG (0xd2,0x34); */
  SETREG (0xd3,0x00);
  SETREG (0xd4,0x00);
  SETREG (0xd5,0x00);
  SETREG (0xd6,0x00);
  SETREG (0xd7,0x00);
  SETREG (0xd8,0x00);
  SETREG (0xd9,0x00);

  /* memory layout
  SETREG (0xe0,0x00);
  SETREG (0xe1,0x48);
  SETREG (0xe2,0x15);
  SETREG (0xe3,0x90);
  SETREG (0xe4,0x15);
  SETREG (0xe5,0x91);
  SETREG (0xe6,0x2a);
  SETREG (0xe7,0xd9);
  SETREG (0xe8,0x2a);
  SETREG (0xe9,0xad);
  SETREG (0xea,0x40);
  SETREG (0xeb,0x22);
  SETREG (0xec,0x40);
  SETREG (0xed,0x23);
  SETREG (0xee,0x55);
  SETREG (0xef,0x6b);
  SETREG (0xf0,0x55);
  SETREG (0xf1,0x6c);
  SETREG (0xf2,0x6a);
  SETREG (0xf3,0xb4);
  SETREG (0xf4,0x6a);
  SETREG (0xf5,0xb5);
  SETREG (0xf6,0x7f);
  SETREG (0xf7,0xfd);*/

  SETREG (0xf8,0x01);   /* other value is 0x05 */
  SETREG (0xf9,0x00);
  SETREG (0xfa,0x00);
  SETREG (0xfb,0x00);
  SETREG (0xfc,0x00);
  SETREG (0xff,0x00);

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

  dev->calib_reg = dev->reg;

  DBGCOMPLETED;
}

/**@brief send slope table for motor movement
 * Send slope_table in machine byte order
 * @param dev device to send slope table
 * @param table_nr index of the slope table in ASIC memory
 * Must be in the [0-4] range.
 * @param slope_table pointer to 16 bit values array of the slope table
 * @param steps number of elemnts in the slope table
 */
static SANE_Status
gl124_send_slope_table (Genesys_Device * dev, int table_nr,
			uint16_t * slope_table, int steps)
{
  SANE_Status status;
  int i;
  char msg[10000];

  DBG (DBG_proc, "%s (table_nr = %d, steps = %d)\n", __func__,
       table_nr, steps);

  /* sanity check */
  if(table_nr<0 || table_nr>4)
    {
      DBG (DBG_error, "%s: invalid table number %d!\n", __func__, table_nr);
      return SANE_STATUS_INVAL;
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
	  sprintf (msg+strlen(msg), ",%d", slope_table[i]);
	}
      DBG (DBG_io, "%s: %s\n", __func__, msg);
    }

  /* slope table addresses are fixed */
  status = sanei_genesys_write_ahb(dev, 0x10000000 + 0x4000 * table_nr, steps * 2, table.data());
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "%s: write to AHB failed writing slope table %d (%s)\n",
	   __func__, table_nr, sane_strstatus (status));
    }

  DBGCOMPLETED;
  return status;
}

/** @brief * Set register values of 'special' ti type frontend
 * Registers value are taken from the frontend register data
 * set.
 * @param dev device owning the AFE
 * @param set flag AFE_INIT to specify the AFE must be reset before writing data
 * */
static SANE_Status
gl124_set_ti_fe (Genesys_Device * dev, uint8_t set)
{
  SANE_Status status = SANE_STATUS_GOOD;
  int i;

  DBGSTART;
  if (set == AFE_INIT)
    {
      DBG (DBG_proc, "%s: setting DAC %u\n", __func__, dev->model->dac_type);

      dev->frontend = dev->frontend_initial;
    }

  /* start writing to DAC */
  status = sanei_genesys_fe_write_data (dev, 0x00, 0x80);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "%s: failed to write reg0: %s\n", __func__,
	   sane_strstatus (status));
      return status;
    }

  /* write values to analog frontend */
  for (uint16_t addr = 0x01; addr < 0x04; addr++)
    {
      status = sanei_genesys_fe_write_data(dev, addr, dev->frontend.regs.get_value(addr));
      if (status != SANE_STATUS_GOOD)
	{
          DBG(DBG_error, "%s: failed to write reg %d: %s\n", __func__, addr,
              sane_strstatus(status));
	  return status;
	}
    }

  status = sanei_genesys_fe_write_data (dev, 0x04, 0x00);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "%s: failed to write reg4: %s\n", __func__,
	   sane_strstatus (status));
      return status;
    }

  /* these are not really sign for this AFE */
  for (i = 0; i < 3; i++)
    {
      status = sanei_genesys_fe_write_data(dev, 0x05 + i, dev->frontend.regs.get_value(0x24 + i));
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "%s: failed to write reg %d: %s\n", __func__, i+5,
	       sane_strstatus (status));
	  return status;
	}
    }

  /* close writing to DAC */
  if(dev->model->dac_type == DAC_CANONLIDE120)
    {
      status = sanei_genesys_fe_write_data (dev, 0x00, 0x01);
    }
  else
    {
      status = sanei_genesys_fe_write_data (dev, 0x00, 0x11);
    }
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "%s: failed to write reg0: %s\n", __func__,
	   sane_strstatus (status));
      return status;
    }

  DBGCOMPLETED;

  return status;
}


/* Set values of analog frontend */
static SANE_Status
gl124_set_fe(Genesys_Device * dev, const Genesys_Sensor& sensor, uint8_t set)
{
    (void) sensor;
  SANE_Status status;
  uint8_t val;

  DBG(DBG_proc, "%s (%s)\n", __func__, set == AFE_INIT ? "init" : set == AFE_SET ? "set" : set ==
      AFE_POWER_SAVE ? "powersave" : "huh?");

  if (set == AFE_INIT)
    {
      DBG(DBG_proc, "%s(): setting DAC %u\n", __func__, dev->model->dac_type);
      dev->frontend = dev->frontend_initial;
    }

  RIE (sanei_genesys_read_register (dev, REG0A, &val));

  /* route to correct analog FE */
  switch ((val & REG0A_SIFSEL)>>REG0AS_SIFSEL)
    {
    case 3:
      status=gl124_set_ti_fe (dev, set);
      break;
    case 0:
    case 1:
    case 2:
    default:
      DBG(DBG_error, "%s: unsupported analog FE 0x%02x\n", __func__, val);
      status=SANE_STATUS_INVAL;
      break;
    }

  DBGCOMPLETED;
  return status;
}


/**@brief compute exposure to use
 * compute the sensor exposure based on target resolution
 * @param dev pointer to  device description
 * @param xres sensor's required resolution
 * @param half_ccd flag for half ccd mode
 */
static int gl124_compute_exposure(Genesys_Device *dev, int xres, int half_ccd)
{
  Sensor_Profile* sensor_profile = get_sensor_profile(dev->model->ccd_type, xres, half_ccd);
  return sensor_profile->exposure;
}


static SANE_Status
gl124_init_motor_regs_scan (Genesys_Device * dev,
                            const Genesys_Sensor& sensor,
                            Genesys_Register_Set * reg,
                            unsigned int scan_exposure_time,
			    float scan_yres,
			    int scan_step_type,
			    unsigned int scan_lines,
			    unsigned int scan_dummy,
			    unsigned int feed_steps,
                            ScanColorMode scan_mode,
                            unsigned int flags)
{
  SANE_Status status;
  int use_fast_fed;
  unsigned int lincnt, fast_dpi;
  uint16_t scan_table[SLOPE_TABLE_SIZE];
  uint16_t fast_table[SLOPE_TABLE_SIZE];
  int scan_steps,fast_steps,factor;
  unsigned int feedl,dist;
  uint32_t z1, z2;
  float yres;
  int min_speed;
  unsigned int linesel;

  DBGSTART;
  DBG(DBG_info, "%s : scan_exposure_time=%d, scan_yres=%g, scan_step_type=%d, scan_lines=%d, "
      "scan_dummy=%d, feed_steps=%d, scan_mode=%d, flags=%x\n", __func__, scan_exposure_time,
      scan_yres, scan_step_type, scan_lines, scan_dummy, feed_steps,
      static_cast<unsigned>(scan_mode), flags);

  /* we never use fast fed since we do manual feed for the scans */
  use_fast_fed=0;
  factor=1;

  /* enforce motor minimal scan speed
   * @TODO extend motor struct for this value */
  if (scan_mode == ScanColorMode::COLOR_SINGLE_PASS)
    {
      min_speed = 900;
    }
  else
    {
      switch(dev->model->motor_type)
        {
          case MOTOR_CANONLIDE110:
	    min_speed = 600;
            break;
          case MOTOR_CANONLIDE120:
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
      linesel=yres/scan_yres-1;
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

  DBG (DBG_io2, "%s: final yres=%f, linesel=%d\n", __func__, yres, linesel);

  lincnt=scan_lines*(linesel+1);
  sanei_genesys_set_triple(reg,REG_LINCNT,lincnt);
  DBG (DBG_io, "%s: lincnt=%d\n", __func__, lincnt);

  /* compute register 02 value */
    uint8_t r02 = REG02_NOTHOME;

    if (use_fast_fed) {
        r02 |= REG02_FASTFED;
    } else {
        r02 &= ~REG02_FASTFED;
    }

    if (flags & MOTOR_FLAG_AUTO_GO_HOME)
        r02 |= REG02_AGOHOME;

    if ((flags & MOTOR_FLAG_DISABLE_BUFFER_FULL_MOVE)
            ||(yres>=sensor.optical_res))
    {
        r02 |= REG02_ACDCDIS;
    }

    reg->set8(REG02, r02);
    sanei_genesys_set_motor_power(*reg, true);

  /* SCANFED */
  sanei_genesys_set_double(reg,REG_SCANFED,4);

  /* scan and backtracking slope table */
  sanei_genesys_slope_table(scan_table,
                            &scan_steps,
                            yres,
                            scan_exposure_time,
                            dev->motor.base_ydpi,
                            scan_step_type,
                            factor,
                            dev->model->motor_type,
                            motors);
  RIE(gl124_send_slope_table (dev, SCAN_TABLE, scan_table, scan_steps));
  RIE(gl124_send_slope_table (dev, BACKTRACK_TABLE, scan_table, scan_steps));

  /* STEPNO */
  sanei_genesys_set_double(reg,REG_STEPNO,scan_steps);

  /* fast table */
  fast_dpi=yres;

  /*
  if (scan_mode != ScanColorMode::COLOR_SINGLE_PASS)
    {
      fast_dpi*=3;
    }
    */
  sanei_genesys_slope_table(fast_table,
                            &fast_steps,
                            fast_dpi,
                            scan_exposure_time,
                            dev->motor.base_ydpi,
                            scan_step_type,
                            factor,
                            dev->model->motor_type,
                            motors);
  RIE(gl124_send_slope_table (dev, STOP_TABLE, fast_table, fast_steps));
  RIE(gl124_send_slope_table (dev, FAST_TABLE, fast_table, fast_steps));

  /* FASTNO */
  sanei_genesys_set_double(reg,REG_FASTNO,fast_steps);

  /* FSHDEC */
  sanei_genesys_set_double(reg,REG_FSHDEC,fast_steps);

  /* FMOVNO */
  sanei_genesys_set_double(reg,REG_FMOVNO,fast_steps);

  /* substract acceleration distance from feedl */
  feedl=feed_steps;
  feedl<<=scan_step_type;

  dist = scan_steps;
  if (flags & MOTOR_FLAG_FEED)
    dist *=2;
  if (use_fast_fed)
    {
        dist += fast_steps*2;
    }
  DBG (DBG_io2, "%s: acceleration distance=%d\n", __func__, dist);

  /* get sure we don't use insane value */
  if(dist<feedl)
    feedl -= dist;
  else
    feedl = 0;

  sanei_genesys_set_triple(reg,REG_FEEDL,feedl);
  DBG (DBG_io, "%s: feedl=%d\n", __func__, feedl);

  /* doesn't seem to matter that much */
  sanei_genesys_calculate_zmode2 (use_fast_fed,
				  scan_exposure_time,
				  scan_table,
				  scan_steps,
				  feedl,
                                  scan_steps,
                                  &z1,
                                  &z2);

  sanei_genesys_set_triple(reg, REG_Z1MOD,z1);
  DBG(DBG_info, "%s: z1 = %d\n", __func__, z1);

  sanei_genesys_set_triple(reg, REG_Z2MOD, z2);
  DBG(DBG_info, "%s: z2 = %d\n", __func__, z2);

  /* LINESEL */
  reg->set8_mask(REG1D, linesel, REG1D_LINESEL);
  reg->set8(REGA0, (scan_step_type << REGA0S_STEPSEL) | (scan_step_type << REGA0S_FSTPSEL));

  /* FMOVDEC */
  sanei_genesys_set_double(reg,REG_FMOVDEC,fast_steps);

  DBGCOMPLETED;
  return SANE_STATUS_GOOD;
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
 * @param half_ccd flag for half ccd mode
 * */
static void gl124_setup_sensor(Genesys_Device * dev,
                               const Genesys_Sensor& sensor,
                               Genesys_Register_Set * regs, int dpi, int half_ccd)
{
  int dpihw;
  uint32_t exp;

  DBGSTART;

    // we start at 6, 0-5 is a 16 bits cache for exposure
    for (uint16_t addr = 0x16; addr < 0x1e; addr++) {
        regs->set8(addr, sensor.custom_regs.get_value(addr));
    }

    // skip writing 5d,5e which is AFE address because
    // they are not defined in register set */
    for (uint16_t addr = 0x52; addr < 0x52 + 11; addr++) {
        regs->set8(addr, sensor.custom_regs.get_value(addr));
    }

  /* set EXPDUMMY and CKxMAP */
    dpihw = sanei_genesys_compute_dpihw(dev, sensor, dpi);
    Sensor_Profile* sensor_profile = get_sensor_profile(dev->model->ccd_type, dpihw, half_ccd);

    regs->set8(0x18, sensor_profile->reg18);
    regs->set8(0x20, sensor_profile->reg20);
    regs->set8(0x61, sensor_profile->reg61);
    regs->set8(0x98, sensor_profile->reg98);
    if (sensor_profile->reg16 != 0) {
        regs->set8(0x16, sensor_profile->reg16);
    }
    if (sensor_profile->reg70 != 0) {
        regs->set8(0x70, sensor_profile->reg70);
    }


  sanei_genesys_set_triple(regs,REG_SEGCNT,sensor_profile->segcnt);
  sanei_genesys_set_double(regs,REG_TG0CNT,sensor_profile->tg0cnt);
  sanei_genesys_set_double(regs,REG_EXPDMY,sensor_profile->expdummy);

  /* if no calibration has been done, set default values for exposures */
  exp = sensor.exposure.red;
  if(exp==0)
    {
      exp=sensor_profile->expr;
    }
  sanei_genesys_set_triple(regs,REG_EXPR,exp);

  exp =sensor.exposure.green;
  if(exp==0)
    {
      exp=sensor_profile->expg;
    }
  sanei_genesys_set_triple(regs,REG_EXPG,exp);

  exp = sensor.exposure.blue;
  if(exp==0)
    {
      exp=sensor_profile->expb;
    }
  sanei_genesys_set_triple(regs,REG_EXPB,exp);

  sanei_genesys_set_triple(regs,REG_CK1MAP,sensor_profile->ck1map);
  sanei_genesys_set_triple(regs,REG_CK3MAP,sensor_profile->ck3map);
  sanei_genesys_set_triple(regs,REG_CK4MAP,sensor_profile->ck4map);

  /* order of the sub-segments */
  dev->order=sensor_profile->order;

  DBGCOMPLETED;
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
 * @param half_ccd SANE_TRUE if sensor's timings are such that x coordinates
 *           must be halved
 * @param color_filter color channel to use as gray data
 * @param flags optical flags (@see )
 * @return SANE_STATUS_GOOD if OK
 */
static SANE_Status
gl124_init_optical_regs_scan (Genesys_Device * dev,
                              const Genesys_Sensor& sensor,
			      Genesys_Register_Set * reg,
			      unsigned int exposure_time,
			      int used_res,
			      unsigned int start,
			      unsigned int pixels,
			      int channels,
			      int depth,
			      SANE_Bool half_ccd,
                              ColorFilter color_filter,
                              int flags)
{
  unsigned int words_per_line, segcnt;
  unsigned int startx, endx, used_pixels, segnb;
  unsigned int dpiset, cksel, dpihw, factor;
  unsigned int bytes;
  GenesysRegister *r;
  SANE_Status status;
  uint32_t expmax, exp;

  DBG(DBG_proc, "%s :  exposure_time=%d, used_res=%d, start=%d, pixels=%d, channels=%d, depth=%d, "
      "half_ccd=%d, flags=%x\n", __func__, exposure_time, used_res, start, pixels, channels, depth,
      half_ccd, flags);

  /* resolution is divided according to CKSEL */
  r = sanei_genesys_get_address (reg, REG18);
  cksel= (r->value & REG18_CKSEL)+1;
  DBG (DBG_io2, "%s: cksel=%d\n", __func__, cksel);

  /* to manage high resolution device while keeping good
   * low resolution scanning speed, we make hardware dpi vary */
  dpihw=sanei_genesys_compute_dpihw(dev, sensor, used_res * cksel);
  factor=sensor.optical_res/dpihw;
  DBG (DBG_io2, "%s: dpihw=%d (factor=%d)\n", __func__, dpihw, factor);

  /* sensor parameters */
  gl124_setup_sensor(dev, sensor, reg, dpihw, half_ccd);
  dpiset = used_res * cksel;

  /* start and end coordinate in optical dpi coordinates */
  /* startx = start/cksel + sensor.dummy_pixel; XXX STEF XXX */
  startx = start/cksel;
  used_pixels=pixels/cksel;
  endx = startx + used_pixels;

  /* pixel coordinate factor correction when used dpihw is not maximal one */
  startx/=factor;
  endx/=factor;
  used_pixels=endx-startx;

  status = gl124_set_fe(dev, sensor, AFE_SET);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "%s: failed to set frontend: %s\n", __func__,
	   sane_strstatus (status));
      return status;
    }

  /* enable shading */
  r = sanei_genesys_get_address (reg, REG01);
  r->value &= ~REG01_SCAN;
  if ((flags & OPTICAL_FLAG_DISABLE_SHADING) ||
      (dev->model->flags & GENESYS_FLAG_NO_CALIBRATION))
    {
      r->value &= ~REG01_DVDSET;
    }
  else
    {
      r->value |= REG01_DVDSET;
    }
  r->value &= ~REG01_SCAN;

  r = sanei_genesys_get_address (reg, REG03);
  if((dev->model->ccd_type!=CIS_CANONLIDE120)&&(used_res>=600))
    {
      r->value &= ~REG03_AVEENB;
      DBG (DBG_io, "%s: disabling AVEENB\n", __func__);
    }
  else
    {
      r->value |= ~REG03_AVEENB;
      DBG (DBG_io, "%s: enabling AVEENB\n", __func__);
    }

    sanei_genesys_set_lamp_power(dev, sensor, *reg, !(flags & OPTICAL_FLAG_DISABLE_LAMP));

  /* BW threshold */
  RIE (sanei_genesys_write_register (dev, REG114, dev->settings.threshold));
  RIE (sanei_genesys_write_register (dev, REG115, dev->settings.threshold));

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

  r->value &= ~REG04_FILTER;
  if (channels == 1)
    {
      switch (color_filter)
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

  if(half_ccd)
    {
      sanei_genesys_set_double(reg,REG_DPISET,dpiset*2);
      DBG (DBG_io2, "%s: dpiset used=%d\n", __func__, dpiset*2);
    }
  else
    {
      sanei_genesys_set_double(reg,REG_DPISET,dpiset);
      DBG (DBG_io2, "%s: dpiset used=%d\n", __func__, dpiset);
    }

  r = sanei_genesys_get_address (reg, REG06);
  r->value |= REG06_GAIN4;

  /* CIS scanners can do true gray by setting LEDADD */
  /* we set up LEDADD only when asked */
  if (dev->model->is_cis == SANE_TRUE)
    {
      r = sanei_genesys_get_address (reg, REG60);
      r->value &= ~REG60_LEDADD;
      if (channels == 1 && (flags & OPTICAL_FLAG_ENABLE_LEDADD))
	{
	  r->value |= REG60_LEDADD;
          sanei_genesys_get_triple(reg,REG_EXPR,&expmax);
          sanei_genesys_get_triple(reg,REG_EXPG,&exp);
          if(exp>expmax)
            {
              expmax=exp;
            }
          sanei_genesys_get_triple(reg,REG_EXPB,&exp);
          if(exp>expmax)
            {
              expmax=exp;
            }
          sanei_genesys_set_triple(&dev->reg,REG_EXPR,expmax);
          sanei_genesys_set_triple(&dev->reg,REG_EXPG,expmax);
          sanei_genesys_set_triple(&dev->reg,REG_EXPB,expmax);
	}
      /* RGB weighting, REG_TRUER,G and B are to be set  */
      r = sanei_genesys_get_address (reg, 0x01);
      r->value &= ~REG01_TRUEGRAY;
      if (channels == 1 && (flags & OPTICAL_FLAG_ENABLE_LEDADD))
	{
	  r->value |= REG01_TRUEGRAY;
          sanei_genesys_write_register (dev, REG_TRUER, 0x80);
          sanei_genesys_write_register (dev, REG_TRUEG, 0x80);
          sanei_genesys_write_register (dev, REG_TRUEB, 0x80);
	}
    }

  /* segment number */
  r = sanei_genesys_get_address (reg, 0x98);
  segnb = r->value & 0x0f;

  sanei_genesys_set_triple(reg,REG_STRPIXEL,startx/segnb);
  DBG (DBG_io2, "%s: strpixel used=%d\n", __func__, startx/segnb);
  sanei_genesys_get_triple(reg,REG_SEGCNT,&segcnt);
  if(endx/segnb==segcnt)
    {
      endx=0;
    }
  sanei_genesys_set_triple(reg,REG_ENDPIXEL,endx/segnb);
  DBG (DBG_io2, "%s: endpixel used=%d\n", __func__, endx/segnb);

  /* words(16bit) before gamma, conversion to 8 bit or lineart */
  words_per_line = (used_pixels * dpiset) / dpihw;
  bytes = depth / 8;
  if (depth == 1)
    {
      words_per_line = (words_per_line >> 3) + ((words_per_line & 7) ? 1 : 0);
    }
  else
    {
      words_per_line *= bytes;
    }

  dev->bpl = words_per_line;
  dev->cur = 0;
  dev->skip = 0;
  dev->len = dev->bpl/segnb;
  dev->dist = dev->bpl/segnb;
  dev->segnb = segnb;
  dev->line_count = 0;
  dev->line_interp = 0;

  DBG (DBG_io2, "%s: used_pixels     =%d\n", __func__, used_pixels);
  DBG (DBG_io2, "%s: pixels          =%d\n", __func__, pixels);
  DBG (DBG_io2, "%s: depth           =%d\n", __func__, depth);
  DBG (DBG_io2, "%s: dev->bpl        =%lu\n", __func__, (unsigned long)dev->bpl);
  DBG (DBG_io2, "%s: dev->len        =%lu\n", __func__, (unsigned long)dev->len);
  DBG (DBG_io2, "%s: dev->dist       =%lu\n", __func__, (unsigned long)dev->dist);
  DBG (DBG_io2, "%s: dev->line_interp=%lu\n", __func__, (unsigned long)dev->line_interp);

  words_per_line *= channels;
  dev->wpl = words_per_line;

  /* allocate buffer for odd/even pixels handling */
    dev->oe_buffer.clear();
    dev->oe_buffer.alloc(dev->wpl);

  /* MAXWD is expressed in 2 words unit */
  sanei_genesys_set_triple(reg,REG_MAXWD,(words_per_line));
  DBG (DBG_io2, "%s: words_per_line used=%d\n", __func__, words_per_line);

  sanei_genesys_set_triple(reg,REG_LPERIOD,exposure_time);
  DBG (DBG_io2, "%s: exposure_time used=%d\n", __func__, exposure_time);

  sanei_genesys_set_double(reg,REG_DUMMY,sensor.dummy_pixel);

  DBGCOMPLETED;
  return SANE_STATUS_GOOD;
}

/** set up registers for an actual scan
 *
 * this function sets up the scanner to scan in normal or single line mode
 */
static SANE_Status
gl124_init_scan_regs(Genesys_Device * dev, const Genesys_Sensor& sensor, Genesys_Register_Set* reg,
                     SetupParams& params)
{
    params.assert_valid();

  int used_res;
  int start, used_pixels;
  int bytes_per_line;
  int move;
  unsigned int lincnt;
  unsigned int oflags, mflags; /**> optical and motor flags */
  int exposure_time;
  int stagger;

  int dummy = 0;
  int slope_dpi = 0;
  int scan_step_type = 1;
  int max_shift;
  size_t requested_buffer_size, read_buffer_size;

  SANE_Bool half_ccd;                /* false: full CCD res is used, true, half max CCD res is used */
  unsigned optical_res;
  SANE_Status status;

    DBG(DBG_info, "%s ", __func__);
    debug_dump(DBG_info, params);

  half_ccd=compute_half_ccd(sensor, params.xres);

  /* optical_res */
  optical_res = sensor.optical_res;
  if (half_ccd)
    optical_res /= 2;
  DBG (DBG_info, "%s: optical_res=%d\n", __func__, optical_res);

  /* stagger */
  if ((!half_ccd) && (dev->model->flags & GENESYS_FLAG_STAGGERED_LINE))
    stagger = (4 * params.yres) / dev->motor.base_ydpi;
  else
    stagger = 0;
  DBG (DBG_info, "gl124_init_scan_regs : stagger=%d lines\n", stagger);

  /** @brief compute used resolution */
  if (params.flags & SCAN_FLAG_USE_OPTICAL_RES)
    {
      used_res = optical_res;
    }
  else
    {
      /* resolution is choosen from a fixed list and can be used directly,
       * unless we have ydpi higher than sensor's maximum one */
      if(params.xres>optical_res)
        used_res=optical_res;
      else
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
  used_pixels = (params.pixels * optical_res) / params.xres;
  DBG (DBG_info, "%s: used_pixels=%d\n", __func__, used_pixels);

  /* round up pixels number if needed */
  if (used_pixels * params.xres < params.pixels * optical_res)
    used_pixels++;

  /* we want even number of pixels here */
  if(used_pixels & 1)
    used_pixels++;

  /* slope_dpi */
  /* cis color scan is effectively a gray scan with 3 gray lines per color line and a FILTER of 0 */
  if (dev->model->is_cis)
    slope_dpi = params.yres * params.channels;
  else
    slope_dpi = params.yres;

  /* scan_step_type */
  if(params.flags & SCAN_FLAG_FEEDING)
    {
      scan_step_type=0;
      exposure_time=MOVE_EXPOSURE;
    }
  else
    {
      exposure_time = gl124_compute_exposure (dev, used_res, half_ccd);
      scan_step_type = sanei_genesys_compute_step_type(motors, dev->model->motor_type, exposure_time);
    }

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
  if (params.flags & SCAN_FLAG_DISABLE_SHADING)
    oflags |= OPTICAL_FLAG_DISABLE_SHADING;
  if (params.flags & SCAN_FLAG_DISABLE_GAMMA)
    oflags |= OPTICAL_FLAG_DISABLE_GAMMA;
  if (params.flags & SCAN_FLAG_DISABLE_LAMP)
    oflags |= OPTICAL_FLAG_DISABLE_LAMP;
  if (params.flags & SCAN_FLAG_CALIBRATION)
    oflags |= OPTICAL_FLAG_DISABLE_DOUBLE;

  if (dev->model->is_cis && dev->settings.true_gray)
    {
      oflags |= OPTICAL_FLAG_ENABLE_LEDADD;
    }

  /* now _LOGICAL_ optical values used are known, setup registers */
  status = gl124_init_optical_regs_scan (dev,
                                         sensor,
					 reg,
					 exposure_time,
					 used_res,
					 start,
					 used_pixels,
                                         params.channels,
                                         params.depth,
					 half_ccd,
                                         params.color_filter,
                                         oflags);
  if (status != SANE_STATUS_GOOD)
    return status;

  /*** motor parameters ***/

  /* max_shift */
  max_shift=sanei_genesys_compute_max_shift(dev,params.channels,params.yres,params.flags);

  /* lines to scan */
  lincnt = params.lines + max_shift + stagger;

  /* add tl_y to base movement */
  move = params.starty;
  DBG(DBG_info, "%s: move=%d steps\n", __func__, move);

  mflags=0;
  if(params.flags & SCAN_FLAG_DISABLE_BUFFER_FULL_MOVE)
    mflags|=MOTOR_FLAG_DISABLE_BUFFER_FULL_MOVE;
  if(params.flags & SCAN_FLAG_FEEDING)
    mflags|=MOTOR_FLAG_FEED;

  status = gl124_init_motor_regs_scan (dev, sensor,
                                       reg,
                                       exposure_time,
                                       slope_dpi,
                                       scan_step_type,
                                       dev->model->is_cis ? lincnt * params.channels : lincnt,
                                       dummy,
                                       move,
                                       params.scan_mode,
                                       mflags);
  if (status != SANE_STATUS_GOOD)
    return status;

  /*** prepares data reordering ***/

  /* words_per_line */
  bytes_per_line = (used_pixels * used_res) / optical_res;
  bytes_per_line = (bytes_per_line * params.channels * params.depth) / 8;

  /* since we don't have sheetfed scanners to handle,
   * use huge read buffer */
  /* TODO find the best size according to settings */
  requested_buffer_size = 16 * bytes_per_line;

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
    dev->out_buffer.alloc((8 * dev->settings.pixels * params.channels * params.depth) / 8);

  dev->read_bytes_left = bytes_per_line * lincnt;

  DBG(DBG_info, "%s: physical bytes to read = %lu\n", __func__, (u_long) dev->read_bytes_left);
  dev->read_active = SANE_TRUE;


  dev->current_setup.params = params;
  dev->current_setup.pixels = (used_pixels * used_res) / optical_res;
  DBG(DBG_info, "%s: current_setup.pixels=%d\n", __func__, dev->current_setup.pixels);
  dev->current_setup.lines = lincnt;
  dev->current_setup.depth = params.depth;
  dev->current_setup.channels = params.channels;
  dev->current_setup.exposure_time = exposure_time;
  dev->current_setup.xres = used_res;
  dev->current_setup.yres = params.yres;
  dev->current_setup.ccd_size_divisor = half_ccd ? 2 : 1;
  dev->current_setup.stagger = stagger;
  dev->current_setup.max_shift = max_shift + stagger;

  dev->total_bytes_read = 0;
  if (params.depth == 1)
    dev->total_bytes_to_read =
      ((dev->settings.pixels * dev->settings.lines) / 8 +
       (((dev->settings.pixels * dev->settings.lines) % 8) ? 1 : 0)) *
      params.channels;
  else
    dev->total_bytes_to_read =
      dev->settings.pixels * dev->settings.lines * params.channels * (params.depth / 8);

  DBG(DBG_info, "%s: total bytes to send = %lu\n", __func__, (u_long) dev->total_bytes_to_read);

  DBGCOMPLETED;
  return SANE_STATUS_GOOD;
}

static void
gl124_calculate_current_setup (Genesys_Device * dev, const Genesys_Sensor& sensor)
{
  int channels;
  int depth;
  int start;

  int used_res;
  int used_pixels;
  unsigned int lincnt;
  int exposure_time;
  int stagger;
  SANE_Bool half_ccd;

  int max_shift, dpihw;

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
    params.startx = start;
    params.starty = 0; // not used
    params.pixels = dev->settings.pixels;
    params.lines = dev->settings.lines;
    params.depth = depth;
    params.channels = channels;
    params.scan_method = dev->settings.scan_method;
    params.scan_mode = dev->settings.scan_mode;
    params.color_filter = dev->settings.color_filter;
    params.flags = 0;

  half_ccd=compute_half_ccd(sensor, params.xres);

    DBG(DBG_info, "%s ", __func__);
    debug_dump(DBG_info, params);

  /* optical_res */
  optical_res = sensor.optical_res;

  if (params.xres <= (unsigned) optical_res)
    used_res = params.xres;
  else
    used_res=optical_res;

  /* compute scan parameters values */
  /* pixels are allways given at half or full CCD optical resolution */
  /* use detected left margin  and fixed value */

  /* compute correct pixels number */
  used_pixels = (params.pixels * optical_res) / params.xres;
  DBG (DBG_info, "%s: used_pixels=%d\n", __func__, used_pixels);

  /* exposure */
  exposure_time = gl124_compute_exposure (dev, params.xres, half_ccd);
  DBG (DBG_info, "%s : exposure_time=%d pixels\n", __func__, exposure_time);

  /* max_shift */
  max_shift=sanei_genesys_compute_max_shift(dev, params.channels, params.yres, 0);

  /* compute hw dpi for sensor */
  dpihw=sanei_genesys_compute_dpihw(dev, sensor,used_res);

  Sensor_Profile* sensor_profile = get_sensor_profile(dev->model->ccd_type, dpihw, half_ccd);
  dev->segnb=sensor_profile->reg98 & 0x0f;

  /* stagger */
  if ((!half_ccd) && (dev->model->flags & GENESYS_FLAG_STAGGERED_LINE))
    stagger = (4 * params.yres) / dev->motor.base_ydpi;
  else
    stagger = 0;
  DBG (DBG_info, "%s: stagger=%d lines\n", __func__, stagger);

  /* lincnt */
  lincnt = params.lines + max_shift + stagger;

  dev->current_setup.params = params;
  dev->current_setup.pixels = (used_pixels * used_res) / optical_res;
  DBG (DBG_info, "%s: current_setup.pixels=%d\n", __func__, dev->current_setup.pixels);
  dev->current_setup.lines = lincnt;
  dev->current_setup.depth = params.depth;
  dev->current_setup.channels = params.channels;
  dev->current_setup.exposure_time = exposure_time;
  dev->current_setup.xres = used_res;
  dev->current_setup.yres = params.yres;
  dev->current_setup.ccd_size_divisor = half_ccd ? 2 : 1;
  dev->current_setup.stagger = stagger;
  dev->current_setup.max_shift = max_shift + stagger;

  DBGCOMPLETED;
}

/**
 * for fast power saving methods only, like disabling certain amplifiers
 * @param dev device to use
 * @param enable true to set inot powersaving
 * */
static SANE_Status
gl124_save_power (Genesys_Device * dev, SANE_Bool enable)
{
  DBG(DBG_proc, "%s: enable = %d\n", __func__, enable);
  if (dev == NULL)
    return SANE_STATUS_INVAL;

  DBGCOMPLETED;
  return SANE_STATUS_GOOD;
}

static SANE_Status
gl124_set_powersaving (Genesys_Device * dev, int delay /* in minutes */ )
{
  GenesysRegister *r;

  DBG(DBG_proc, "%s (delay = %d)\n", __func__, delay);

  r = sanei_genesys_get_address (&dev->reg, REG03);
  r->value &= ~0xf0;
  if(delay<15)
    {
      r->value |= delay;
    }
  else
    {
      r->value |= 0x0f;
    }

  DBGCOMPLETED;
  return SANE_STATUS_GOOD;
}

static SANE_Status
gl124_start_action (Genesys_Device * dev)
{
  return sanei_genesys_write_register (dev, 0x0f, 0x01);
}

static SANE_Status
gl124_stop_action (Genesys_Device * dev)
{
  SANE_Status status;
  uint8_t val40, val;
  unsigned int loop;

  DBGSTART;

  /* post scan gpio : without that HOMSNR is unreliable */
  gl124_homsnr_gpio(dev);

  status = sanei_genesys_get_status (dev, &val);
  if (DBG_LEVEL >= DBG_io)
    {
      sanei_genesys_print_status (val);
    }

  status = sanei_genesys_read_register (dev, REG100, &val40);
  if (status != SANE_STATUS_GOOD)
    {
      DBG(DBG_error, "%s: failed to read reg100: %s\n", __func__, sane_strstatus(status));
      DBGCOMPLETED;
      return status;
    }

  /* only stop action if needed */
  if (!(val40 & REG100_DATAENB) && !(val40 & REG100_MOTMFLG))
    {
      DBG (DBG_info, "%s: already stopped\n", __func__);
      DBGCOMPLETED;
      return SANE_STATUS_GOOD;
    }

  /* ends scan */
  val = dev->reg.get8(REG01);
  val &= ~REG01_SCAN;
  dev->reg.set8(REG01, val);
  status = sanei_genesys_write_register (dev, REG01, val);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "%s: failed to write register 01: %s\n", __func__,
	   sane_strstatus (status));
      return status;
    }
  sanei_genesys_sleep_ms(100);

  loop = 10;
  while (loop > 0)
    {
      status = sanei_genesys_get_status (dev, &val);
      if (DBG_LEVEL >= DBG_io)
	{
	  sanei_genesys_print_status (val);
	}
      status = sanei_genesys_read_register (dev, REG100, &val40);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "%s: failed to read home sensor: %s\n", __func__,
	       sane_strstatus (status));
	  DBGCOMPLETED;
	  return status;
	}

      /* if scanner is in command mode, we are done */
      if (!(val40 & REG100_DATAENB) && !(val40 & REG100_MOTMFLG)
	  && !(val & MOTORENB))
	{
	  DBGCOMPLETED;
	  return SANE_STATUS_GOOD;
	}

      sanei_genesys_sleep_ms(100);
      loop--;
    }

  DBGCOMPLETED;
  return SANE_STATUS_IO_ERROR;
}


/** @brief setup GPIOs for scan
 * Setup GPIO values to drive motor (or light) needed for the
 * target resolution
 * @param *dev device to set up
 * @param resolution dpi of the target scan
 * @return SANE_STATUS_GOOD unless REG32 cannot be read
 */
static SANE_Status
gl124_setup_scan_gpio(Genesys_Device *dev, int resolution)
{
SANE_Status status;
uint8_t val;

  DBGSTART;
  RIE (sanei_genesys_read_register (dev, REG32, &val));

  /* LiDE 110, 210 and 220 cases */
  if(dev->model->gpo_type != GPO_CANONLIDE120)
    {
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
  RIE (sanei_genesys_write_register (dev, REG32, val));
  DBGCOMPLETED;
  return SANE_STATUS_GOOD;
}

/* Send the low-level scan command */
/* todo : is this that useful ? */
static SANE_Status
gl124_begin_scan (Genesys_Device * dev, const Genesys_Sensor& sensor, Genesys_Register_Set * reg,
		  SANE_Bool start_motor)
{
    (void) sensor;

  SANE_Status status;
  uint8_t val;

  DBGSTART;
  if (reg == NULL)
    return SANE_STATUS_INVAL;

  /* set up GPIO for scan */
  RIE(gl124_setup_scan_gpio(dev,dev->settings.yres));

  /* clear scan and feed count */
  RIE (sanei_genesys_write_register (dev, REG0D, REG0D_CLRLNCNT | REG0D_CLRMCNT));

  /* enable scan and motor */
  RIE (sanei_genesys_read_register (dev, REG01, &val));
  val |= REG01_SCAN;
  RIE (sanei_genesys_write_register (dev, REG01, val));

  if (start_motor)
    {
      RIE (sanei_genesys_write_register (dev, REG0F, 1));
    }
  else
    {
      RIE (sanei_genesys_write_register (dev, REG0F, 0));
    }

  DBGCOMPLETED;
  return status;
}


/* Send the stop scan command */
static SANE_Status
gl124_end_scan (Genesys_Device * dev, Genesys_Register_Set * reg,
		SANE_Bool check_stop)
{
  SANE_Status status;

  DBG(DBG_proc, "%s (check_stop = %d)\n", __func__, check_stop);
  if (reg == NULL)
    return SANE_STATUS_INVAL;

  if (dev->model->is_sheetfed == SANE_TRUE)
    {
      status = SANE_STATUS_GOOD;
    }
  else                                /* flat bed scanners */
    {
      status = gl124_stop_action (dev);
      if (status != SANE_STATUS_GOOD)
	{
          DBG(DBG_error, "%s: failed to stop: %s\n", __func__, sane_strstatus(status));
	  return status;
	}
    }

  DBGCOMPLETED;
  return status;
}


/** rewind scan
 * Move back by the same amount of distance than previous scan.
 * @param dev device to rewind
 * @returns SANE_STATUS_GOOD on success
 */
static
SANE_Status gl124_rewind(Genesys_Device * dev)
{
  SANE_Status status;
  uint8_t byte;

  DBGSTART;

  /* set motor reverse */
  RIE (sanei_genesys_read_register (dev, 0x02, &byte));
  byte |= 0x04;
  RIE (sanei_genesys_write_register(dev, 0x02, byte));

  const auto& sensor = sanei_genesys_find_sensor_any(dev);

  /* and start scan, then wait completion */
  RIE (gl124_begin_scan (dev, sensor, &dev->reg, SANE_TRUE));
  do
    {
      sanei_genesys_sleep_ms(100);
      RIE (sanei_genesys_read_register (dev, REG100, &byte));
    }
  while(byte & REG100_MOTMFLG);
  RIE (gl124_end_scan (dev, &dev->reg, SANE_TRUE));

  /* restore direction */
  RIE (sanei_genesys_read_register (dev, 0x02, &byte));
  byte &= 0xfb;
  RIE (sanei_genesys_write_register(dev, 0x02, byte));
  DBGCOMPLETED;
  return SANE_STATUS_GOOD;
}


/** Park head
 * Moves the slider to the home (top) position slowly
 * @param dev device to park
 * @param wait_until_home true to make the function waiting for head
 * to be home before returning, if fals returne immediately
 * @returns SANE_STATUS_GOO on success */
static
SANE_Status
gl124_slow_back_home (Genesys_Device * dev, SANE_Bool wait_until_home)
{
  Genesys_Register_Set local_reg;
  SANE_Status status;
  GenesysRegister *r;
  uint8_t val;
  float resolution;
  int loop = 0;

  DBG(DBG_proc, "%s (wait_until_home = %d)\n", __func__, wait_until_home);

  /* post scan gpio : without that HOMSNR is unreliable */
  gl124_homsnr_gpio(dev);

  /* first read gives HOME_SENSOR true */
  status = sanei_genesys_get_status (dev, &val);
  if (status != SANE_STATUS_GOOD)
    {
      DBG(DBG_error, "%s: failed to read home sensor: %s\n", __func__, sane_strstatus(status));
      return status;
    }
  if (DBG_LEVEL >= DBG_io)
    {
      sanei_genesys_print_status (val);
    }
  sanei_genesys_sleep_ms(100);

  /* second is reliable */
  status = sanei_genesys_get_status (dev, &val);
  if (status != SANE_STATUS_GOOD)
    {
      DBG(DBG_error, "%s: failed to read home sensor: %s\n", __func__, sane_strstatus(status));
      return status;
    }
  if (DBG_LEVEL >= DBG_io)
    {
      sanei_genesys_print_status (val);
    }

  /* is sensor at home? */
  if (val & HOMESNR)
    {
      DBG (DBG_info, "%s: already at home, completed\n", __func__);
      dev->scanhead_position_in_steps = 0;
      DBGCOMPLETED;
      return SANE_STATUS_GOOD;
    }

  /* feed a little first */
  if (dev->model->model_id == MODEL_CANON_LIDE_210)
    {
      status = gl124_feed (dev, 20, SANE_TRUE);
      if (status != SANE_STATUS_GOOD)
        {
          DBG(DBG_error, "%s: failed to do initial feed: %s\n", __func__, sane_strstatus(status));
          return status;
        }
    }

  local_reg = dev->reg;
  resolution=sanei_genesys_get_lowest_dpi(dev);

  const auto& sensor = sanei_genesys_find_sensor_any(dev);

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

    status = gl124_init_scan_regs(dev, sensor, &local_reg, params);

  if (status != SANE_STATUS_GOOD)
    {
      DBG(DBG_error, "%s: failed to set up registers: %s\n", __func__, sane_strstatus(status));
      DBGCOMPLETED;
      return status;
    }

  /* clear scan and feed count */
  RIE (sanei_genesys_write_register (dev, REG0D, REG0D_CLRLNCNT | REG0D_CLRMCNT));

  /* set up for reverse and no scan */
  r = sanei_genesys_get_address(&local_reg, REG02);
  r->value |= REG02_MTRREV;

  RIE (dev->model->cmd_set->bulk_write_register(dev, local_reg));

  RIE(gl124_setup_scan_gpio(dev,resolution));

  status = gl124_start_action (dev);
  if (status != SANE_STATUS_GOOD)
    {
      DBG(DBG_error, "%s: failed to start motor: %s\n", __func__, sane_strstatus(status));
      gl124_stop_action (dev);
      /* restore original registers */
      dev->model->cmd_set->bulk_write_register(dev, dev->reg);
      return status;
    }

  /* post scan gpio : without that HOMSNR is unreliable */
  gl124_homsnr_gpio(dev);

  if (wait_until_home)
    {

      while (loop < 300)        /* do not wait longer then 30 seconds */
	{
	  status = sanei_genesys_get_status (dev, &val);
	  if (status != SANE_STATUS_GOOD)
	    {
	      DBG(DBG_error, "%s: failed to read home sensor: %s\n", __func__,
		  sane_strstatus(status));
	      return status;
	    }

	  if (val & HOMESNR)        /* home sensor */
	    {
	      DBG(DBG_info, "%s: reached home position\n", __func__);
	      DBGCOMPLETED;
              dev->scanhead_position_in_steps = 0;
	      return SANE_STATUS_GOOD;
	    }
          sanei_genesys_sleep_ms(100);
	  ++loop;
	}

      /* when we come here then the scanner needed too much time for this, so we better stop the motor */
      gl124_stop_action (dev);
      DBG(DBG_error, "%s: timeout while waiting for scanhead to go home\n", __func__);
      return SANE_STATUS_IO_ERROR;
    }

  DBG(DBG_info, "%s: scanhead is still moving\n", __func__);
  DBGCOMPLETED;
  return SANE_STATUS_GOOD;
}

/** @brief moves the slider to steps at motor base dpi
 * @param dev device to work on
 * @param steps number of steps to move
 * @param reverse true is moving backward
 * */
static SANE_Status
gl124_feed (Genesys_Device * dev, unsigned int steps, int reverse)
{
  Genesys_Register_Set local_reg;
  SANE_Status status;
  GenesysRegister *r;
  float resolution;
  uint8_t val;

  DBGSTART;
  DBG (DBG_io, "%s: steps=%d\n", __func__, steps);

  /* prepare local registers */
  local_reg = dev->reg;

  resolution=sanei_genesys_get_lowest_ydpi(dev);
  const auto& sensor = sanei_genesys_find_sensor(dev, resolution);

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
                   SCAN_FLAG_DISABLE_BUFFER_FULL_MOVE |
                   SCAN_FLAG_IGNORE_LINE_DISTANCE;

    status = gl124_init_scan_regs(dev, sensor, &local_reg, params);

  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "%s: failed to set up registers: %s\n", __func__, sane_strstatus (status));
      DBGCOMPLETED;
      return status;
    }

  /* set exposure to zero */
  sanei_genesys_set_triple(&local_reg,REG_EXPR,0);
  sanei_genesys_set_triple(&local_reg,REG_EXPG,0);
  sanei_genesys_set_triple(&local_reg,REG_EXPB,0);

  /* clear scan and feed count */
  RIE (sanei_genesys_write_register (dev, REG0D, REG0D_CLRLNCNT));
  RIE (sanei_genesys_write_register (dev, REG0D, REG0D_CLRMCNT));

  /* set up for no scan */
  r = sanei_genesys_get_address (&local_reg, REG01);
  r->value &= ~REG01_SCAN;

  /* set up for reverse if needed */
  if(reverse)
    {
      r = sanei_genesys_get_address (&local_reg, REG02);
      r->value |= REG02_MTRREV;
    }

  /* send registers */
  RIE (dev->model->cmd_set->bulk_write_register(dev, local_reg));

  status = gl124_start_action (dev);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "%s: failed to start motor: %s\n", __func__, sane_strstatus (status));
      gl124_stop_action (dev);

      /* restore original registers */
      dev->model->cmd_set->bulk_write_register(dev, dev->reg);
      return status;
    }

  /* wait until feed count reaches the required value, but do not
   * exceed 30s */
  do
    {
          status = sanei_genesys_get_status (dev, &val);
    }
  while (status == SANE_STATUS_GOOD && !(val & FEEDFSH));

  /* then stop scanning */
  RIE(gl124_stop_action (dev));

  DBGCOMPLETED;
  return SANE_STATUS_GOOD;
}


/* Automatically set top-left edge of the scan area by scanning a 200x200 pixels
   area at 600 dpi from very top of scanner */
static SANE_Status
gl124_search_start_position (Genesys_Device * dev)
{
  int size;
  SANE_Status status;
  Genesys_Register_Set local_reg = dev->reg;
  int steps;

  int pixels = 600;
  int dpi = 300;

  DBGSTART;

  /* sets for a 200 lines * 600 pixels */
  /* normal scan with no shading */

  // FIXME: the current approach of doing search only for one resolution does not work on scanners
  // whith employ different sensors with potentially different settings.
  auto& sensor = sanei_genesys_find_sensor_for_write(dev, dpi);

    SetupParams params;
    params.xres = dpi;
    params.yres = dpi;
    params.startx = 0;
    params.starty = 0;        /*we should give a small offset here~60 steps */
    params.pixels = 600;
    params.lines = dev->model->search_lines;
    params.depth = 8;
    params.channels = 1;
    params.scan_method = dev->settings.scan_method;
    params.scan_mode = ScanColorMode::GRAY;
    params.color_filter = ColorFilter::GREEN;
    params.flags = SCAN_FLAG_DISABLE_SHADING |
                   SCAN_FLAG_DISABLE_GAMMA |
                   SCAN_FLAG_IGNORE_LINE_DISTANCE |
                   SCAN_FLAG_DISABLE_BUFFER_FULL_MOVE;

    status = gl124_init_scan_regs(dev, sensor, &local_reg, params);

  if (status!=SANE_STATUS_GOOD)
    {
      DBG(DBG_error, "%s: failed to init scan registers: %s\n", __func__, sane_strstatus(status));
      return status;
    }

  /* send to scanner */
  status = dev->model->cmd_set->bulk_write_register(dev, local_reg);
  if (status != SANE_STATUS_GOOD)
    {
      DBG(DBG_error, "%s: failed to bulk write registers: %s\n", __func__, sane_strstatus(status));
      return status;
    }

  size = pixels * dev->model->search_lines;

  std::vector<uint8_t> data(size);

  status = gl124_begin_scan (dev, sensor, &local_reg, SANE_TRUE);
  if (status != SANE_STATUS_GOOD)
    {
      DBG(DBG_error, "%s: failed to begin scan: %s\n", __func__, sane_strstatus(status));
      return status;
    }

  /* waits for valid data */
  do
    sanei_genesys_test_buffer_empty (dev, &steps);
  while (steps);

  /* now we're on target, we can read data */
  status = sanei_genesys_read_data_from_scanner (dev, data.data(), size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG(DBG_error, "%s: failed to read data: %s\n", __func__, sane_strstatus(status));
      return status;
    }

  if (DBG_LEVEL >= DBG_data)
    sanei_genesys_write_pnm_file("gl124_search_position.pnm", data.data(), 8, 1, pixels,
                                 dev->model->search_lines);

  status = gl124_end_scan (dev, &local_reg, SANE_TRUE);
  if (status != SANE_STATUS_GOOD)
    {
      DBG(DBG_error, "%s: failed to end scan: %s\n", __func__, sane_strstatus(status));
      return status;
    }

  /* update regs to copy ASIC internal state */
  dev->reg = local_reg;

  status =
    sanei_genesys_search_reference_point (dev, sensor, data.data(), 0, dpi, pixels,
					  dev->model->search_lines);
  if (status != SANE_STATUS_GOOD)
    {
      DBG(DBG_error, "%s: failed to set search reference point: %s\n", __func__,
          sane_strstatus(status));
      return status;
    }

  DBGCOMPLETED;
  return SANE_STATUS_GOOD;
}

/*
 * sets up register for coarse gain calibration
 * todo: check it for scanners using it */
static SANE_Status
gl124_init_regs_for_coarse_calibration(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                       Genesys_Register_Set& regs)
{
  SANE_Status status;
  uint8_t channels;
  uint8_t cksel;

  DBGSTART;
  cksel = (regs.find_reg(0x18).value & REG18_CKSEL) + 1;        /* clock speed = 1..4 clocks */

  /* set line size */
    if (dev->settings.scan_mode == ScanColorMode::COLOR_SINGLE_PASS) {
        channels = 3;
    } else {
        channels = 1;
    }

    SetupParams params;
    params.xres = dev->settings.xres;
    params.yres = dev->settings.yres;
    params.startx = 0;
    params.starty = 0;
    params.pixels = sensor.optical_res / cksel;
    params.lines = 20;
    params.depth = 16;
    params.channels = channels;
    params.scan_method = dev->settings.scan_method;
    params.scan_mode = dev->settings.scan_mode;
    params.color_filter = dev->settings.color_filter;
    params.flags = SCAN_FLAG_DISABLE_SHADING |
                   SCAN_FLAG_DISABLE_GAMMA |
                   SCAN_FLAG_SINGLE_LINE |
                   SCAN_FLAG_FEEDING |
                   SCAN_FLAG_IGNORE_LINE_DISTANCE;

    status = gl124_init_scan_regs(dev, sensor, &regs, params);

  if (status != SANE_STATUS_GOOD)
    {
      DBG(DBG_error, "%s: failed to setup scan: %s\n", __func__, sane_strstatus(status));
      return status;
    }
  sanei_genesys_set_motor_power(regs, false);

  DBG(DBG_info, "%s: optical sensor res: %d dpi, actual res: %d\n", __func__,
      sensor.optical_res / cksel, dev->settings.xres);

  status = dev->model->cmd_set->bulk_write_register(dev, regs);
  if (status != SANE_STATUS_GOOD)
    {
      DBG(DBG_error, "%s: failed to bulk write registers: %s\n", __func__, sane_strstatus(status));
      return status;
    }

  DBGCOMPLETED;
  return SANE_STATUS_GOOD;
}


/* init registers for shading calibration */
/* shading calibration is done at dpihw */
static SANE_Status
gl124_init_regs_for_shading(Genesys_Device * dev, const Genesys_Sensor& sensor,
                            Genesys_Register_Set& regs)
{
  SANE_Status status;
  int move, resolution, dpihw, factor;

  DBGSTART;

  /* initial calibration reg values */
  regs = dev->reg;

  dev->calib_channels = 3;
  dev->calib_lines = dev->model->shading_lines;
  dpihw=sanei_genesys_compute_dpihw(dev, sensor, dev->settings.xres);
  if(dpihw>=2400)
    {
      dev->calib_lines *= 2;
    }
  resolution=dpihw;

  /* if half CCD mode, use half resolution */
  if(compute_half_ccd(sensor, dev->settings.xres)==SANE_TRUE)
    {
      resolution /= 2;
      dev->calib_lines /= 2;
    }
  dev->calib_resolution = resolution;
  dev->calib_total_bytes_to_read = 0;
  factor=sensor.optical_res/resolution;
  dev->calib_pixels = sensor.sensor_pixels/factor;

  /* distance to move to reach white target at high resolution */
  move=0;
  if(dev->settings.yres>=1200)
    {
      move = SANE_UNFIX (dev->model->y_offset_calib);
      move = (move * (dev->motor.base_ydpi/4)) / MM_PER_INCH;
    }
  DBG (DBG_io, "%s: move=%d steps\n", __func__, move);

    SetupParams params;
    params.xres = resolution;
    params.yres = resolution;
    params.startx = 0;
    params.starty = move;
    params.pixels = dev->calib_pixels;
    params.lines = dev->calib_lines;
    params.depth = 16;
    params.channels = dev->calib_channels;
    params.scan_method = dev->settings.scan_method;
    params.scan_mode = ScanColorMode::COLOR_SINGLE_PASS;
    params.color_filter = ColorFilter::RED;
    params.flags = SCAN_FLAG_DISABLE_SHADING |
                   SCAN_FLAG_DISABLE_GAMMA |
                   SCAN_FLAG_DISABLE_BUFFER_FULL_MOVE |
                   SCAN_FLAG_IGNORE_LINE_DISTANCE;

    status = gl124_init_scan_regs(dev, sensor, &regs, params);

    sanei_genesys_set_motor_power(regs, false);

  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "%s: failed to setup scan: %s\n", __func__,
	   sane_strstatus (status));
      return status;
    }

  dev->scanhead_position_in_steps += dev->calib_lines + move;

  status = dev->model->cmd_set->bulk_write_register(dev, regs);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "%s: failed to bulk write registers: %s\n", __func__,
	   sane_strstatus (status));
      return status;
    }

  DBGCOMPLETED;
  return SANE_STATUS_GOOD;
}

/** @brief set up registers for the actual scan
 */
static SANE_Status
gl124_init_regs_for_scan (Genesys_Device * dev, const Genesys_Sensor& sensor)
{
  int channels;
  int flags;
  int depth;
  float move;
  int move_dpi;
  float start;
  uint8_t val40,val;

  SANE_Status status;

    DBG(DBG_info, "%s ", __func__);
    debug_dump(DBG_info, dev->settings);

  /* wait for motor to stop first */
  status = sanei_genesys_get_status (dev, &val);
  if (status != SANE_STATUS_GOOD)
   {
     DBG (DBG_error, "%s: failed to read status: %s\n", __func__, sane_strstatus (status));
     DBGCOMPLETED;
     return status;
   }
  status = sanei_genesys_read_register (dev, REG100, &val40);
  if (status != SANE_STATUS_GOOD)
   {
     DBG (DBG_error, "%s: failed to read reg100: %s\n", __func__, sane_strstatus (status));
     DBGCOMPLETED;
     return status;
   }
  if((val & MOTORENB) || (val40 & REG100_MOTMFLG))
    {
      do
        {
          sanei_genesys_sleep_ms(10);
          status = sanei_genesys_get_status (dev, &val);
          if (status != SANE_STATUS_GOOD)
           {
             DBG (DBG_error, "%s: failed to read status: %s\n", __func__, sane_strstatus (status));
             DBGCOMPLETED;
             return status;
           }
          status = sanei_genesys_read_register (dev, REG100, &val40);
          if (status != SANE_STATUS_GOOD)
           {
             DBG (DBG_error, "%s: failed to read reg100: %s\n", __func__, sane_strstatus (status));
             DBGCOMPLETED;
             return status;
           }
        } while ((val & MOTORENB) || (val40 & REG100_MOTMFLG));
        sanei_genesys_sleep_ms(50);
    }

  /* ensure head is parked in case of calibration */
  RIE (gl124_slow_back_home (dev, SANE_TRUE));

  /* channels */
  if (dev->settings.scan_mode == ScanColorMode::COLOR_SINGLE_PASS)
    channels = 3;
  else
    channels = 1;

  /* depth */
  depth = dev->settings.depth;
  if (dev->settings.scan_mode == ScanColorMode::LINEART)
    depth = 1;

  /* y (motor) distance to move to reach scanned area */
  move_dpi = dev->motor.base_ydpi/4;
  move = SANE_UNFIX (dev->model->y_offset);
  move += dev->settings.tl_y;
  move = (move * move_dpi) / MM_PER_INCH;
  DBG (DBG_info, "%s: move=%f steps\n", __func__, move);

  if(channels*dev->settings.yres>=600 && move>700)
    {
      status = gl124_feed (dev, move-500, SANE_FALSE);
      if (status != SANE_STATUS_GOOD)
        {
          DBG (DBG_error, "%s: failed to move to scan area\n",__func__);
          return status;
        }
      move=500;
    }
  DBG(DBG_info, "%s: move=%f steps\n", __func__, move);

  /* start */
  start = SANE_UNFIX (dev->model->x_offset);
  start += dev->settings.tl_x;
  if(compute_half_ccd(sensor, dev->settings.xres)==SANE_TRUE)
    {
      start /=2;
    }
  start = (start * sensor.optical_res) / MM_PER_INCH;

  flags = 0;

  /* enable emulated lineart from gray data */
  if(dev->settings.scan_mode == ScanColorMode::LINEART
     && dev->settings.dynamic_lineart)
    {
      flags |= SCAN_FLAG_DYNAMIC_LINEART;
    }

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

    status = gl124_init_scan_regs(dev, sensor, &dev->reg, params);

  if (status != SANE_STATUS_GOOD)
    return status;

  DBGCOMPLETED;
  return SANE_STATUS_GOOD;
}

/**
 * Send shading calibration data. The buffer is considered to always hold values
 * for all the channels.
 */
static SANE_Status
gl124_send_shading_data (Genesys_Device * dev, const Genesys_Sensor& sensor,
                         uint8_t * data, int size)
{
  SANE_Status status = SANE_STATUS_GOOD;
  uint32_t addr, length, strpixel ,endpixel, x, factor, segcnt, pixels, i;
  uint32_t lines, channels;
  uint16_t dpiset,dpihw;
  uint8_t val,*ptr,*src;

  DBGSTART;
  DBG( DBG_io2, "%s: writing %d bytes of shading data\n",__func__,size);

  /* logical size of a color as seen by generic code of the frontend */
  length = (uint32_t) (size / 3);
  sanei_genesys_get_triple(&dev->reg,REG_STRPIXEL,&strpixel);
  sanei_genesys_get_triple(&dev->reg,REG_ENDPIXEL,&endpixel);
  sanei_genesys_get_triple(&dev->reg,REG_SEGCNT,&segcnt);
  if(endpixel==0)
    {
      endpixel=segcnt;
    }
  DBG( DBG_io2, "%s: STRPIXEL=%d, ENDPIXEL=%d, PIXELS=%d, SEGCNT=%d\n",__func__,strpixel,endpixel,endpixel-strpixel,segcnt);

  /* compute deletion factor */
  sanei_genesys_get_double(&dev->reg,REG_DPISET,&dpiset);
  dpihw=sanei_genesys_compute_dpihw(dev, sensor, dpiset);
  factor=dpihw/dpiset;
  DBG( DBG_io2, "%s: factor=%d\n",__func__,factor);

  /* binary data logging */
  if(DBG_LEVEL>=DBG_data)
    {
      dev->binary=fopen("binary.pnm","wb");
      sanei_genesys_get_triple(&dev->reg, REG_LINCNT, &lines);
      channels=dev->current_setup.channels;
      if(dev->binary!=NULL)
        {
          fprintf(dev->binary,"P5\n%d %d\n%d\n",(endpixel-strpixel)/factor*channels*dev->segnb,lines/channels,255);
        }
    }

  /* turn pixel value into bytes 2x16 bits words */
  strpixel*=2*2; /* 2 words of 2 bytes */
  endpixel*=2*2;
  segcnt*=2*2;
  pixels=endpixel-strpixel;

  DBG( DBG_io2, "%s: using chunks of %d bytes (%d shading data pixels)\n",__func__,length, length/4);
  std::vector<uint8_t> buffer(pixels * dev->segnb, 0);

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
          switch(dev->segnb)
            {
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
      RIE (sanei_genesys_read_register (dev, 0xd0+i, &val));
      addr = val * 8192 + 0x10000000;
      status = sanei_genesys_write_ahb(dev, addr, pixels*dev->segnb, buffer.data());
      if (status != SANE_STATUS_GOOD)
        {
          DBG(DBG_error, "%s; write to AHB failed (%s)\n", __func__, sane_strstatus(status));
          return status;
        }
    }

  DBGCOMPLETED;

  return status;
}


/** @brief move to calibration area
 * This functions moves scanning head to calibration area
 * by doing a 600 dpi scan
 * @param dev scanner device
 * @return SANE_STATUS_GOOD on success, else the error code
 */
static SANE_Status
move_to_calibration_area (Genesys_Device * dev, const Genesys_Sensor& sensor,
                          Genesys_Register_Set& regs)
{
  int pixels;
  int size;
  SANE_Status status = SANE_STATUS_GOOD;

  DBGSTART;

  pixels = (sensor.sensor_pixels*600)/sensor.optical_res;

  /* initial calibration reg values */
  regs = dev->reg;

    SetupParams params;
    params.xres = 600;
    params.yres = 600;
    params.startx = 0;
    params.starty = 0;
    params.pixels = pixels;
    params.lines = 1;
    params.depth = 8;
    params.channels = 3;
    params.scan_method = dev->settings.scan_method;
    params.scan_mode = ScanColorMode::COLOR_SINGLE_PASS;
    params.color_filter = dev->settings.color_filter;
    params.flags = SCAN_FLAG_DISABLE_SHADING |
                   SCAN_FLAG_DISABLE_GAMMA |
                   SCAN_FLAG_SINGLE_LINE |
                   SCAN_FLAG_IGNORE_LINE_DISTANCE;

    status = gl124_init_scan_regs(dev, sensor, &regs, params);

  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "%s: failed to setup scan: %s\n", __func__, sane_strstatus (status));
      return status;
    }

  size = pixels * 3;
  std::vector<uint8_t> line(size);

  /* write registers and scan data */
  RIE(dev->model->cmd_set->bulk_write_register(dev, regs));

  DBG (DBG_info, "%s: starting line reading\n", __func__);
  RIE(gl124_begin_scan (dev, sensor, &regs, SANE_TRUE));
  RIE(sanei_genesys_read_data_from_scanner(dev, line.data(), size));

  /* stop scanning */
  RIE(gl124_stop_action (dev));

  if (DBG_LEVEL >= DBG_data)
    {
      sanei_genesys_write_pnm_file("gl124_movetocalarea.pnm", line.data(), 8, 3, pixels, 1);
    }

  DBGCOMPLETED;
  return status;
}

/* this function does the led calibration by scanning one line of the calibration
   area below scanner's top on white strip.

-needs working coarse/gain
*/
static SANE_Status
gl124_led_calibration (Genesys_Device * dev, Genesys_Sensor& sensor, Genesys_Register_Set& regs)
{
  int num_pixels;
  int total_size;
  int resolution;
  int dpihw;
  int i, j;
  SANE_Status status = SANE_STATUS_GOOD;
  int val;
  int channels, depth;
  int avg[3];
  int turn;
  uint16_t exp[3],target;
  SANE_Bool acceptable;
  SANE_Bool half_ccd;

  DBGSTART;

  /* move to calibration area */
  move_to_calibration_area(dev, sensor, regs);

  /* offset calibration is always done in 16 bit depth color mode */
  channels = 3;
  depth=16;
  dpihw=sanei_genesys_compute_dpihw(dev, sensor, dev->settings.xres);
  half_ccd=compute_half_ccd(sensor, dev->settings.xres);
  if(half_ccd==SANE_TRUE)
    {
      resolution = dpihw/2;
    }
  else
    {
      resolution = dpihw;
    }
  Sensor_Profile* sensor_profile = get_sensor_profile(dev->model->ccd_type, dpihw, half_ccd);
  num_pixels = (sensor.sensor_pixels*resolution)/sensor.optical_res;

  /* initial calibration reg values */
  regs = dev->reg;

    SetupParams params;
    params.xres = resolution;
    params.yres = resolution;
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

    status = gl124_init_scan_regs(dev, sensor, &regs, params);

  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "%s: failed to setup scan: %s\n", __func__, sane_strstatus (status));
      return status;
    }

  total_size = num_pixels * channels * (depth/8) * 1;        /* colors * bytes_per_color * scan lines */
  std::vector<uint8_t> line(total_size);

  /* initial loop values and boundaries */
  exp[0]=sensor_profile->expr;
  exp[1]=sensor_profile->expg;
  exp[2]=sensor_profile->expb;
  target=sensor.gain_white_ref*256;

  turn = 0;

  /* no move during led calibration */
  sanei_genesys_set_motor_power(regs, false);
  do
    {
      /* set up exposure */
      sanei_genesys_set_triple(&regs,REG_EXPR,exp[0]);
      sanei_genesys_set_triple(&regs,REG_EXPG,exp[1]);
      sanei_genesys_set_triple(&regs,REG_EXPB,exp[2]);

      /* write registers and scan data */
      RIE(dev->model->cmd_set->bulk_write_register(dev, regs));

      DBG(DBG_info, "%s: starting line reading\n", __func__);
      RIE(gl124_begin_scan (dev, sensor, &regs, SANE_TRUE));
      RIE(sanei_genesys_read_data_from_scanner (dev, line.data(), total_size));

      /* stop scanning */
      RIE(gl124_stop_action (dev));

      if (DBG_LEVEL >= DBG_data)
	{
          char fn[30];
          snprintf(fn, 30, "gl124_led_%02d.pnm", turn);
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
          /* we accept +- 2% delta from target */
          if(abs(avg[i]-target)>target/50)
            {
              exp[i]=(exp[i]*target)/avg[i];
              acceptable = SANE_FALSE;
            }
        }

      turn++;
    }
  while (!acceptable && turn < 100);

  DBG(DBG_info, "%s: acceptable exposure: %d,%d,%d\n", __func__, exp[0], exp[1], exp[2]);

  /* set these values as final ones for scan */
  sanei_genesys_set_triple(&dev->reg,REG_EXPR,exp[0]);
  sanei_genesys_set_triple(&dev->reg,REG_EXPG,exp[1]);
  sanei_genesys_set_triple(&dev->reg,REG_EXPB,exp[2]);

  /* store in this struct since it is the one used by cache calibration */
  sensor.exposure.red = exp[0];
  sensor.exposure.green = exp[1];
  sensor.exposure.blue = exp[2];

  DBGCOMPLETED;
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
gl124_offset_calibration(Genesys_Device * dev, const Genesys_Sensor& sensor,
                         Genesys_Register_Set& regs)
{
  SANE_Status status = SANE_STATUS_GOOD;
  uint8_t reg0a;
  unsigned int channels, bpp;
  int pass = 0, avg, total_size;
  int topavg, bottomavg, resolution, lines;
  int top, bottom, black_pixels, pixels;

  DBGSTART;

  /* no gain nor offset for TI AFE */
  RIE (sanei_genesys_read_register (dev, REG0A, &reg0a));
  if(((reg0a & REG0A_SIFSEL)>>REG0AS_SIFSEL)==3)
    {
      DBGCOMPLETED;
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

    status = gl124_init_scan_regs(dev, sensor, &regs, params);

  if (status != SANE_STATUS_GOOD)
    {
      DBG(DBG_error, "%s: failed to setup scan: %s\n", __func__, sane_strstatus(status));
      return status;
    }
  sanei_genesys_set_motor_power(regs, false);

  /* allocate memory for scans */
  total_size = pixels * channels * lines * (bpp/8);        /* colors * bytes_per_color * scan lines */

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

  RIE(gl124_set_fe(dev, sensor, AFE_SET));
  RIE(dev->model->cmd_set->bulk_write_register(dev, regs));
  DBG(DBG_info, "%s: starting first line reading\n", __func__);
  RIE(gl124_begin_scan(dev, sensor, &regs, SANE_TRUE));
  RIE(sanei_genesys_read_data_from_scanner(dev, first_line.data(), total_size));
  if (DBG_LEVEL >= DBG_data)
   {
      char title[30];
      snprintf(title, 30, "gl124_offset%03d.pnm", bottom);
      sanei_genesys_write_pnm_file(title, first_line.data(), bpp, channels, pixels, lines);
   }

  bottomavg = dark_average(first_line.data(), pixels, lines, channels, black_pixels);
  DBG(DBG_io2, "%s: bottom avg=%d\n", __func__, bottomavg);

  /* now top value */
  top = 255;
  dev->frontend.set_offset(0, top);
  dev->frontend.set_offset(1, top);
  dev->frontend.set_offset(2, top);
  RIE(gl124_set_fe(dev, sensor, AFE_SET));
  RIE(dev->model->cmd_set->bulk_write_register(dev, regs));
  DBG(DBG_info, "%s: starting second line reading\n", __func__);
  RIE(gl124_begin_scan(dev, sensor, &regs, SANE_TRUE));
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

      /* scan with no move */
      RIE(gl124_set_fe(dev, sensor, AFE_SET));
      RIE(dev->model->cmd_set->bulk_write_register(dev, regs));
      DBG(DBG_info, "%s: starting second line reading\n", __func__);
      RIE(gl124_begin_scan(dev, sensor, &regs, SANE_TRUE));
      RIE(sanei_genesys_read_data_from_scanner(dev, second_line.data(), total_size));

      if (DBG_LEVEL >= DBG_data)
	{
          char title[30];
          snprintf(title, 30, "gl124_offset%03d.pnm", dev->frontend.get_offset(1));
          sanei_genesys_write_pnm_file(title, second_line.data(), bpp, channels, pixels, lines);
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

  DBGCOMPLETED;
  return SANE_STATUS_GOOD;
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
static SANE_Status
gl124_coarse_gain_calibration(Genesys_Device * dev, const Genesys_Sensor& sensor,
                              Genesys_Register_Set& regs, int dpi)
{
  int pixels;
  int total_size;
  uint8_t reg0a;
  int i, j, channels;
  SANE_Status status = SANE_STATUS_GOOD;
  int max[3];
  float gain[3],coeff;
  int val, code, lines;
  int resolution;
  int bpp;

  DBG(DBG_proc, "%s: dpi = %d\n", __func__, dpi);

  /* no gain nor offset for TI AFE */
  RIE (sanei_genesys_read_register (dev, REG0A, &reg0a));
  if(((reg0a & REG0A_SIFSEL)>>REG0AS_SIFSEL)==3)
    {
      DBGCOMPLETED;
      return status;
    }

  /* coarse gain calibration is always done in color mode */
  channels = 3;

  /* follow CKSEL */
  if(dev->settings.xres<sensor.optical_res)
    {
      coeff=0.9;
      /*resolution=sensor.optical_res/2;*/
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

    status = gl124_init_scan_regs(dev, sensor, &regs, params);

    sanei_genesys_set_motor_power(regs, false);

  if (status != SANE_STATUS_GOOD)
    {
      DBG(DBG_error, "%s: failed to setup scan: %s\n", __func__, sane_strstatus(status));
      return status;
    }

  RIE (dev->model->cmd_set->bulk_write_register(dev, regs));

  total_size = pixels * channels * (16/bpp) * lines;

  std::vector<uint8_t> line(total_size);

  RIE(gl124_set_fe(dev, sensor, AFE_SET));
  RIE(gl124_begin_scan(dev, sensor, &regs, SANE_TRUE));
  RIE(sanei_genesys_read_data_from_scanner(dev, line.data(), total_size));

  if (DBG_LEVEL >= DBG_data)
    sanei_genesys_write_pnm_file("gl124_coarse.pnm", line.data(), bpp, channels, pixels, lines);

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

      DBG(DBG_proc, "%s: channel %d, max=%d, gain = %f, setting:%d\n", __func__, j, max[j],
          gain[j], dev->frontend.get_gain(j));
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

  RIE (gl124_stop_action (dev));

  status = gl124_slow_back_home (dev, SANE_TRUE);

  DBGCOMPLETED;
  return status;
}

/*
 * wait for lamp warmup by scanning the same line until difference
 * between 2 scans is below a threshold
 */
static SANE_Status
gl124_init_regs_for_warmup (Genesys_Device * dev,
                            const Genesys_Sensor& sensor,
			    Genesys_Register_Set * reg,
			    int *channels, int *total_size)
{
  int num_pixels;
  SANE_Status status = SANE_STATUS_GOOD;

  DBGSTART;
  if (dev == NULL || reg == NULL || channels == NULL || total_size == NULL)
    return SANE_STATUS_INVAL;

  *channels=3;

  *reg = dev->reg;

    SetupParams params;
    params.xres = sensor.optical_res;
    params.yres = dev->motor.base_ydpi;
    params.startx = sensor.sensor_pixels / 4;
    params.starty = 0;
    params.pixels = sensor.sensor_pixels / 2;
    params.lines = 1;
    params.depth = 8;
    params.channels = *channels;
    params.scan_method = dev->settings.scan_method;
    params.scan_mode = ScanColorMode::COLOR_SINGLE_PASS;
    params.color_filter = dev->settings.color_filter;
    params.flags = SCAN_FLAG_DISABLE_SHADING |
                   SCAN_FLAG_DISABLE_GAMMA |
                   SCAN_FLAG_SINGLE_LINE |
                   SCAN_FLAG_IGNORE_LINE_DISTANCE;

    status = gl124_init_scan_regs(dev, sensor, reg, params);

  if (status != SANE_STATUS_GOOD)
    {
      DBG(DBG_error, "%s: failed to setup scan: %s\n", __func__, sane_strstatus(status));
      return status;
    }

  num_pixels = dev->current_setup.pixels;

  *total_size = num_pixels * 3 * 1;        /* colors * bytes_per_color * scan lines */

  sanei_genesys_set_motor_power(*reg, false);
  RIE (dev->model->cmd_set->bulk_write_register(dev, *reg));

  DBGCOMPLETED;
  return SANE_STATUS_GOOD;
}

/** @brief default GPIO values
 * set up GPIO/GPOE for idle state
 * @param dev device to set up
 * @return SANE_STATUS_GOOD unless a GPIO register cannot be written
 */
static SANE_Status
gl124_init_gpio (Genesys_Device * dev)
{
  SANE_Status status = SANE_STATUS_GOOD;
  int idx;

  DBGSTART;

  /* per model GPIO layout */
  if (dev->model->model_id == MODEL_CANON_LIDE_110)
    {
      idx = 0;
    }
  else if (dev->model->model_id == MODEL_CANON_LIDE_120)
    {
      idx = 2;
    }
  else
    {                                /* canon LiDE 210 and 220 case */
      idx = 1;
    }

  RIE (sanei_genesys_write_register (dev, REG31, gpios[idx].r31));
  RIE (sanei_genesys_write_register (dev, REG32, gpios[idx].r32));
  RIE (sanei_genesys_write_register (dev, REG33, gpios[idx].r33));
  RIE (sanei_genesys_write_register (dev, REG34, gpios[idx].r34));
  RIE (sanei_genesys_write_register (dev, REG35, gpios[idx].r35));
  RIE (sanei_genesys_write_register (dev, REG36, gpios[idx].r36));
  RIE (sanei_genesys_write_register (dev, REG38, gpios[idx].r38));

  DBGCOMPLETED;
  return status;
}

/**
 * set memory layout by filling values in dedicated registers
 */
static SANE_Status
gl124_init_memory_layout (Genesys_Device * dev)
{
  SANE_Status status = SANE_STATUS_GOOD;
  int idx = 0;

  DBGSTART;

  /* point to per model memory layout */
  if (dev->model->model_id == MODEL_CANON_LIDE_110 ||dev->model->model_id == MODEL_CANON_LIDE_120)
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

  DBGCOMPLETED;
  return status;
}

/**
 * initialize backend and ASIC : registers, motor tables, and gamma tables
 * then ensure scanner's head is at home
 */
static SANE_Status
gl124_init(Genesys_Device * dev)
{
  SANE_Status status;

  DBG_INIT ();
  DBGSTART;

  status=sanei_genesys_asic_init(dev, 0);

  DBGCOMPLETED;
  return status;
}


/* *
 * initialize ASIC from power on condition
 */
static SANE_Status
gl124_boot (Genesys_Device * dev, SANE_Bool cold)
{
  SANE_Status status;
  uint8_t val;

  DBGSTART;

  /* reset ASIC in case of cold boot */
  if(cold)
    {
      RIE (sanei_genesys_write_register (dev, 0x0e, 0x01));
      RIE (sanei_genesys_write_register (dev, 0x0e, 0x00));
    }

  /* enable GPOE 17 */
  RIE (sanei_genesys_write_register (dev, 0x36, 0x01));

  /* set GPIO 17 */
  RIE (sanei_genesys_read_register (dev, 0x33, &val));
  val |= 0x01;
  RIE (sanei_genesys_write_register (dev, 0x33, val));

  /* test CHKVER */
  RIE (sanei_genesys_read_register (dev, REG100, &val));
  if (val & REG100_CHKVER)
    {
      RIE (sanei_genesys_read_register (dev, 0x00, &val));
      DBG(DBG_info, "%s: reported version for genesys chip is 0x%02x\n", __func__, val);
    }

  /* Set default values for registers */
  gl124_init_registers (dev);

  /* Write initial registers */
  RIE (dev->model->cmd_set->bulk_write_register(dev, dev->reg));

  /* tune reg 0B */
  val = REG0B_30MHZ | REG0B_ENBDRAM | REG0B_64M;
  RIE (sanei_genesys_write_register (dev, REG0B, val));
  dev->reg.remove_reg(0x0b);

  /* set up end access */
  RIE (sanei_genesys_write_0x8c (dev, 0x10, 0x0b));
  RIE (sanei_genesys_write_0x8c (dev, 0x13, 0x0e));

  /* CIS_LINE */
  SETREG (0x08, REG08_CIS_LINE);
  RIE (sanei_genesys_write_register (dev, 0x08, dev->reg.find_reg(0x08).value));

  /* setup gpio */
  RIE (gl124_init_gpio (dev));

  /* setup internal memory layout */
  RIE (gl124_init_memory_layout (dev));

  DBGCOMPLETED;
  return SANE_STATUS_GOOD;
}


static SANE_Status
gl124_update_hardware_sensors (Genesys_Scanner * s)
{
  /* do what is needed to get a new set of events, but try to not loose
     any of them.
   */
  SANE_Status status = SANE_STATUS_GOOD;
  uint8_t val=0;

  RIE (sanei_genesys_read_register (s->dev, REG31, &val));

  /* TODO : for the next scanner special case,
   * add another per scanner button profile struct to avoid growing
   * hard-coded button mapping here.
   */
  if((s->dev->model->gpo_type == GPO_CANONLIDE110)
    ||(s->dev->model->gpo_type == GPO_CANONLIDE120))
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
  return status;
}


/** the gl124 command set */
static Genesys_Command_Set gl124_cmd_set = {
  "gl124-generic",                /* the name of this set */

  gl124_init,
  gl124_init_regs_for_warmup,
  gl124_init_regs_for_coarse_calibration,
  gl124_init_regs_for_shading,
  gl124_init_regs_for_scan,

  gl124_get_filter_bit,
  gl124_get_lineart_bit,
  gl124_get_bitset_bit,
  gl124_get_gain4_bit,
  gl124_get_fast_feed_bit,
  gl124_test_buffer_empty_bit,
  gl124_test_motor_flag_bit,

  gl124_set_fe,
  gl124_set_powersaving,
  gl124_save_power,

  gl124_begin_scan,
  gl124_end_scan,

  sanei_genesys_send_gamma_table,

  gl124_search_start_position,

  gl124_offset_calibration,
  gl124_coarse_gain_calibration,
  gl124_led_calibration,

  gl124_slow_back_home,
  gl124_rewind,

  sanei_genesys_bulk_write_register,
  NULL,
  sanei_genesys_bulk_read_data,

  gl124_update_hardware_sensors,

  /* no sheetfed support for now */
  NULL,
  NULL,
  NULL,
  NULL,

  sanei_genesys_is_compatible_calibration,
  NULL,
  gl124_send_shading_data,
  gl124_calculate_current_setup,
  gl124_boot
};

SANE_Status
sanei_gl124_init_cmd_set (Genesys_Device * dev)
{
  dev->model->cmd_set = &gl124_cmd_set;
  return SANE_STATUS_GOOD;
}
