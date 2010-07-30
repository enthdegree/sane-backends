/* sane - Scanner Access Now Easy.

   Copyright (C) 2010 Stéphane Voltz <stef.dev@free.fr>
   
    
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

#include "genesys_gl843.h"

/****************************************************************************
 Low level function
 ****************************************************************************/

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
	   val & REG41_MOTORENB ? "MOTORENB" : "");
  DBG (DBG_info, "status=%s\n", msg);
}

/* ------------------------------------------------------------------------ */
/*                  Read and write RAM, registers and AFE                   */
/* ------------------------------------------------------------------------ */

/**
 *
 */
static SANE_Status
write_end_access (Genesys_Device * dev, uint8_t index, uint8_t val)
{
  SANE_Status status;

  DBG (DBG_io, "write_end_access: 0x%02x,0x%02x\n", index, val);

  status =
    sanei_usb_control_msg (dev->dn, REQUEST_TYPE_OUT, REQUEST_REGISTER,
			   VALUE_BUF_ENDACCESS, index, 1, &val);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "write_end_access: failed %s\n", sane_strstatus (status));
    }
  return status;
}

/* Set address for writing data */
static SANE_Status
gl843_set_buffer_address (Genesys_Device * dev, uint32_t addr)
{
  SANE_Status status;

  DBG (DBG_io, "gl843_set_buffer_address: setting address to 0x%05x\n",
       addr & 0xffff);

  status = sanei_genesys_write_register (dev, 0x5c, (addr & 0xff));
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl843_set_buffer_address: failed while writing low byte: %s\n",
	   sane_strstatus (status));
      return status;
    }

  addr = addr >> 8;
  status = sanei_genesys_write_register (dev, 0x5b, (addr & 0xff));
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl843_set_buffer_address: failed while writing high byte: %s\n",
	   sane_strstatus (status));
      return status;
    }

  DBG (DBG_io, "gl843_set_buffer_address: completed\n");

  return status;
}

/**
 * writes a block of data to RAM
 * @param dev USB device
 * @param addr RAM address to write to
 * @param size size of the chunk of data
 * @param data pointer to the data to write
 */
static SANE_Status
write_data (Genesys_Device * dev, uint32_t addr, uint32_t size, uint8_t * data)
{
  size_t written;
  SANE_Status status = SANE_STATUS_GOOD;
  uint8_t val;

  status = gl843_set_buffer_address (dev, addr);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "write_data: failed while setting address for bulk write data: %s\n",
	   sane_strstatus (status));
      return status;
    }

  /* URB    19  control  0x40 0x0c 0x83 0x00 len     1 wrote 0x28 */
  val = 0x28;
  status = sanei_usb_control_msg (dev->dn, REQUEST_TYPE_OUT, REQUEST_REGISTER, VALUE_SET_REGISTER, 0, 1, &val);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "write_data: failed to set register address %s\n", sane_strstatus (status));
      return status;
    }

  /* write actual data */
  written = size;
  status = sanei_usb_write_bulk (dev->dn, data, &written);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "write_data: failed while writing bulk write data: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status = gl843_set_buffer_address (dev, 0);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "write_data: failed setting to default RAM address: %s\n",
	   sane_strstatus (status));
      return status;
    }

  return status;
}

/**
 * Write to many GL843 registers at once
 */
#ifndef UNIT_TESTING
static
#endif
  SANE_Status
gl843_bulk_write_register (Genesys_Device * dev, Genesys_Register_Set * reg, size_t elems)
{
  SANE_Status status = SANE_STATUS_GOOD;
  size_t i;

  for (i = 0; i < elems && status == SANE_STATUS_GOOD; i++)
    {
      if (reg[i].address != 0)
	{
	  status =
	    sanei_genesys_write_register (dev, reg[i].address, reg[i].value);
	}
    }

  DBG (DBG_io, "gl843_bulk_write_register: wrote %lu registers\n",
       (u_long) elems);
  return status;
}

static SANE_Status
gl843_bulk_read_data (Genesys_Device * dev, uint8_t addr,
			      uint8_t * data, size_t len)
{
  SANE_Status status;
  size_t size;
  uint8_t outdata[8];

  DBG (DBG_io, "gl843_bulk_read_data: requesting %lu bytes\n",
       (u_long) len);

  if (len == 0) 
      return SANE_STATUS_GOOD;

  status =
    sanei_usb_control_msg (dev->dn, REQUEST_TYPE_OUT, REQUEST_REGISTER,
			   VALUE_SET_REGISTER, INDEX, 1, &addr);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl843_bulk_read_data failed while setting register: %s\n",
	   sane_strstatus (status));
      return status;
    }

  outdata[0] = BULK_IN;
  outdata[1] = BULK_RAM;
  outdata[2] = VALUE_BUFFER & 0xff;
  outdata[3] = (VALUE_BUFFER >> 8) & 0xff;
  outdata[4] = (len & 0xff);
  outdata[5] = ((len >> 8) & 0xff);
  outdata[6] = ((len >> 16) & 0xff);
  outdata[7] = ((len >> 24) & 0xff);

  status =
    sanei_usb_control_msg (dev->dn, REQUEST_TYPE_OUT, REQUEST_BUFFER,
			   VALUE_BUFFER, INDEX, sizeof (outdata), outdata);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl843_bulk_read_data failed while writing command: %s\n",
	   sane_strstatus (status));
      return status;
    }

  while (len)
    {
      if (len > BULKIN_MAXSIZE)
	size = BULKIN_MAXSIZE;
      else
	size = len;

      DBG (DBG_io2,
	   "gl843_bulk_read_data: trying to read %lu bytes of data\n",
	   (u_long) size);

      status = sanei_usb_read_bulk (dev->dn, data, &size);

      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "gl843_bulk_read_data failed while reading bulk data: %s\n",
	       sane_strstatus (status));
	  return status;
	}

      DBG (DBG_io2,
	   "gl843_bulk_read_data read %lu bytes, %lu remaining\n",
	   (u_long) size, (u_long) (len - size));

      len -= size;
      data += size;
    }

  DBG (DBG_io, "gl843_bulk_read_data: completed\n");

  return SANE_STATUS_GOOD;
}

/****************************************************************************
 Mid level functions 
 ****************************************************************************/

static SANE_Bool
gl843_get_fast_feed_bit (Genesys_Register_Set * regs)
{
  Genesys_Register_Set *r = NULL;

  r = sanei_genesys_get_address (regs, REG02);
  if (r && (r->value & REG02_FASTFED))
    return SANE_TRUE;
  return SANE_FALSE;
}

static SANE_Bool
gl843_get_filter_bit (Genesys_Register_Set * regs)
{
  Genesys_Register_Set *r = NULL;

  r = sanei_genesys_get_address (regs, REG04);
  if (r && (r->value & REG04_FILTER))
    return SANE_TRUE;
  return SANE_FALSE;
}

static SANE_Bool
gl843_get_lineart_bit (Genesys_Register_Set * regs)
{
  Genesys_Register_Set *r = NULL;

  r = sanei_genesys_get_address (regs, REG04);
  if (r && (r->value & REG04_LINEART))
    return SANE_TRUE;
  return SANE_FALSE;
}

static SANE_Bool
gl843_get_bitset_bit (Genesys_Register_Set * regs)
{
  Genesys_Register_Set *r = NULL;

  r = sanei_genesys_get_address (regs, REG04);
  if (r && (r->value & REG04_BITSET))
    return SANE_TRUE;
  return SANE_FALSE;
}

static SANE_Bool
gl843_get_gain4_bit (Genesys_Register_Set * regs)
{
  Genesys_Register_Set *r = NULL;

  r = sanei_genesys_get_address (regs, 0x06);
  if (r && (r->value & REG06_GAIN4))
    return SANE_TRUE;
  return SANE_FALSE;
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
/* *dev  : device infos
   *regs : registers to be set
   half_ccd: set up for half ccd resolution
   all registers 08-0B, 10-1D, 52-59 are set up. They shouldn't
   appear anywhere else but in register_ini

Responsible for signals to CCD/CIS:
  CCD_CK1X (CK1INV(0x16),CKDIS(0x16),CKTOGGLE(0x18),CKDELAY(0x18),MANUAL1(0x1A),CK1MTGL(0x1C),CK1LOW(0x1D),CK1MAP(0x74,0x75,0x76),CK1NEG(0x7D))
  CCD_CK2X (CK2INV(0x16),CKDIS(0x16),CKTOGGLE(0x18),CKDELAY(0x18),MANUAL1(0x1A),CK1LOW(0x1D),CK1NEG(0x7D))
  CCD_CK3X (MANUAL3(0x1A),CK3INV(0x1A),CK3MTGL(0x1C),CK3LOW(0x1D),CK3MAP(0x77,0x78,0x79),CK3NEG(0x7D))
  CCD_CK4X (MANUAL3(0x1A),CK4INV(0x1A),CK4MTGL(0x1C),CK4LOW(0x1D),CK4MAP(0x7A,0x7B,0x7C),CK4NEG(0x7D))
  CCD_CPX  (CTRLHI(0x16),CTRLINV(0x16),CTRLDIS(0x16),CPH(0x72),CPL(0x73),CPNEG(0x7D))
  CCD_RSX  (CTRLHI(0x16),CTRLINV(0x16),CTRLDIS(0x16),RSH(0x70),RSL(0x71),RSNEG(0x7D))
  CCD_TGX  (TGINV(0x16),TGMODE(0x17),TGW(0x17),EXPR(0x10,0x11),TGSHLD(0x1D))
  CCD_TGG  (TGINV(0x16),TGMODE(0x17),TGW(0x17),EXPG(0x12,0x13),TGSHLD(0x1D))
  CCD_TGB  (TGINV(0x16),TGMODE(0x17),TGW(0x17),EXPB(0x14,0x15),TGSHLD(0x1D))
  LAMP_SW  (EXPR(0x10,0x11),XPA_SEL(0x03),LAMP_PWR(0x03),LAMPTIM(0x03),MTLLAMP(0x04),LAMPPWM(0x29))
  XPA_SW   (EXPG(0x12,0x13),XPA_SEL(0x03),LAMP_PWR(0x03),LAMPTIM(0x03),MTLLAMP(0x04),LAMPPWM(0x29))
  LAMP_B   (EXPB(0x14,0x15),LAMP_PWR(0x03))

other registers:
  CISSET(0x01),CNSET(0x18),DCKSEL(0x18),SCANMOD(0x18),EXPDMY(0x19),LINECLP(0x1A),CKAREA(0x1C),TGTIME(0x1C),LINESEL(0x1E),DUMMY(0x34)

Responsible for signals to AFE:
  VSMP  (VSMP(0x58),VSMPW(0x58))
  BSMP  (BSMP(0x59),BSMPW(0x59))

other register settings depending on this:
  RHI(0x52),RLOW(0x53),GHI(0x54),GLOW(0x55),BHI(0x56),BLOW(0x57),

*/
static void
gl843_setup_sensor (Genesys_Device * dev, Genesys_Register_Set * regs)
{
  Genesys_Register_Set *r;
  int i;

  DBG (DBG_proc, "gl843_setup_sensor\n");

  for (i = 0x06; i < 0x0e; i++)
    {
      r = sanei_genesys_get_address (regs, 0x10 + i);
      if (r)
	r->value = dev->sensor.regs_0x10_0x1d[i];
    }

  for (i = 0; i < 9; i++)
    {
      r = sanei_genesys_get_address (regs, 0x52 + i);
      if (r)
	r->value = dev->sensor.regs_0x52_0x5e[i];
    }

  DBG (DBG_proc, "gl843_setup_sensor: completed \n");
}


/* returns the max register bulk size */
static int
gl843_bulk_full_size (void)
{
  return GENESYS_GL843_MAX_REGS;
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
  DBGSTART;

  memset (dev->reg, 0,
	  GENESYS_GL843_MAX_REGS * sizeof (Genesys_Register_Set));

        /* default to KV-SS080 */
       	SETREG (0xa2, 0x0f);
        SETREG (0x01, 0x00);
        SETREG (0x02, 0x78);
        SETREG (0x03, 0x1f);
        SETREG (0x04, 0x10);
        SETREG (0x05, 0x80);
        SETREG (0x06, 0xd0);
        SETREG (0x08, 0x00);
        SETREG (0x09, 0x00);
        SETREG (0x0a, 0x00);
        SETREG (0x0b, 0x6a);
        SETREG (0x0d, 0x00);
        SETREG (0x10, 0x00); 
        SETREG (0x11, 0x00);
        SETREG (0x12, 0x00); 
        SETREG (0x13, 0x00); 
        SETREG (0x14, 0x00); 
        SETREG (0x15, 0x00); 
        SETREG (0x16, 0x33); 
        SETREG (0x17, 0x1c);
        SETREG (0x18, 0x10);
        SETREG (0x19, 0x2a);
        SETREG (0x1a, 0x04);
        SETREG (0x1b, 0x00);
        SETREG (0x1c, 0x20);
        SETREG (0x1d, 0x04);
        SETREG (0x1e, 0x10);
        SETREG (0x1f, 0x01);
        SETREG (0x20, 0x10);
        SETREG (0x21, 0x04);
        SETREG (0x22, 0x01);
        SETREG (0x23, 0x01);
        SETREG (0x24, 0x04);
        SETREG (0x25, 0x00);
        SETREG (0x26, 0x00);
        SETREG (0x27, 0x00);
        SETREG (0x2c, 0x02);
        SETREG (0x2d, 0x58);
        SETREG (0x2e, 0x80);
        SETREG (0x2f, 0x80);
        SETREG (0x30, 0x00);
        SETREG (0x31, 0x14);
        SETREG (0x32, 0x27);
        SETREG (0x33, 0xec);
        SETREG (0x34, 0x24);
        SETREG (0x35, 0x00);
        SETREG (0x36, 0xff);
        SETREG (0x37, 0xff);
        SETREG (0x38, 0x55);
        SETREG (0x39, 0xf0);
        SETREG (0x3d, 0x00);
        SETREG (0x3e, 0x00);
        SETREG (0x3f, 0x01);
        SETREG (0x52, 0x01);
        SETREG (0x53, 0x04);
        SETREG (0x54, 0x07);
        SETREG (0x55, 0x0a);
        SETREG (0x56, 0x0d);
        SETREG (0x57, 0x10);
        SETREG (0x58, 0x1b);
        SETREG (0x59, 0x00);
        SETREG (0x5a, 0x40);
        SETREG (0x5e, 0x23);
        SETREG (0x5f, 0x01);
        SETREG (0x60, 0x00);
        SETREG (0x61, 0x00);
        SETREG (0x62, 0x00);
        SETREG (0x63, 0x00);
        SETREG (0x64, 0x00);
        SETREG (0x65, 0x00);
        SETREG (0x67, 0x7f);
        SETREG (0x68, 0x7f);
        SETREG (0x69, 0x01);
        SETREG (0x6a, 0x04);
        SETREG (0x6b, 0x30);
        SETREG (0x6c, 0xe5);
        SETREG (0x6d, 0x20);
        SETREG (0x6e, 0x7e);
        SETREG (0x6f, 0xA1);
        SETREG (0x70, 0x08);
        SETREG (0x71, 0x0a);
        SETREG (0x72, 0x0b);
        SETREG (0x73, 0x0d);
        SETREG (0x74, 0x00);
        SETREG (0x75, 0x00);
        SETREG (0x76, 0x00);
        SETREG (0x77, 0x00);
        SETREG (0x78, 0x00);
        SETREG (0x79, 0x00);
        SETREG (0x7a, 0x00);
        SETREG (0x7f, 0x00);
        SETREG (0x80, 0x00);
        SETREG (0x81, 0x00);
        SETREG (0x82, 0x00);
        SETREG (0x83, 0x00);
        SETREG (0x84, 0x00);
        SETREG (0x87, 0x00);
        SETREG (0x9d, 0x08);
        SETREG (0xa6, 0x06);
        SETREG (0xa7, 0x0f);
        SETREG (0xa8, 0x00);
        SETREG (0xa9, 0x08);
        SETREG (0x94, 0xff);
        SETREG (0xab, 0x50);

        /* G4050 values */
        if(strcmp(dev->model->name,"hewlett-packard-scanjet-g4050")==0)
          {
          }

  /* fine tune upon device description */
  dev->reg[reg_0x05].value &= ~REG05_DPIHW;
  switch (dev->sensor.optical_res)
    {
    case 600:
      dev->reg[reg_0x05].value |= REG05_DPIHW_600;
      break;
    case 1200:
      dev->reg[reg_0x05].value |= REG05_DPIHW_1200;
      break;
    case 2400:
      dev->reg[reg_0x05].value |= REG05_DPIHW_2400;
      break;
    case 4800:
      dev->reg[reg_0x05].value |= REG05_DPIHW_4800;
      break;
    }

  /* initalize calibration reg */
  memcpy (dev->calib_reg, dev->reg,
	  GENESYS_GL843_MAX_REGS * sizeof (Genesys_Register_Set));

  DBGCOMPLETED;
}

/* Send slope table for motor movement 
   slope_table in machine byte order
 */
static SANE_Status
gl843_send_slope_table (Genesys_Device * dev, int table_nr,
			uint16_t * slope_table, int steps)
{
  SANE_Status status;
  uint8_t *table;
  int i;
  char msg[2048];

  DBG (DBG_proc, "%s (table_nr = %d, steps = %d)\n", __FUNCTION__,
       table_nr, steps);

  table = (uint8_t *) malloc (steps * 2);
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
	  sprintf (msg, "%s,%d", msg, slope_table[i]);
	}
      DBG (DBG_io, "%s: %s\n", __FUNCTION__, msg);
    }

  /* slope table addresses are fixed : 0x4000,  0x4800,  0x5000,  0x5800,  0x6000 */
  status = write_data (dev, 0x4000 + 0x800 * table_nr, steps * 2, table);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "%s: write data failed writing slope table %d (%s)\n",
	   __FUNCTION__, table_nr, sane_strstatus (status));
    }

  free (table);
  DBG (DBG_proc, "%s: completed\n", __FUNCTION__);
  return status;
}


/* Set values of analog frontend */
static SANE_Status
gl843_set_fe (Genesys_Device * dev, uint8_t set)
{
  SANE_Status status;
  uint8_t val;
  int i;

  DBG (DBG_proc, "gl843_set_fe (%s)\n",
       set == AFE_INIT ? "init" : set == AFE_SET ? "set" : set ==
       AFE_POWER_SAVE ? "powersave" : "huh?");
  
  if (set == AFE_INIT)
    {
      DBG (DBG_proc, "gl843_set_fe(): setting DAC %u\n",
	   dev->model->dac_type);
      sanei_genesys_init_fe (dev);
    }

  /* check analog frontend type */
  RIE (sanei_genesys_read_register (dev, REG04, &val));
  if ((val & REG04_FESET) != 0x00)
    {
      /* for now there is no support for AD fe */
      DBG (DBG_proc, "gl843_set_fe(): unsupported frontend type %d\n", dev->reg[reg_0x04].value & REG04_FESET);
      return SANE_STATUS_UNSUPPORTED;
    }

  DBG (DBG_proc, "gl843_set_fe(): frontend reset complete\n");

  for (i = 1; i <= 3; i++)
      {
	status =
	  sanei_genesys_fe_write_data (dev, i, dev->frontend.reg[i]);
	if (status != SANE_STATUS_GOOD)
	  {
	    DBG (DBG_error,
		 "gl843_set_fe: writing reg[%d] failed: %s\n", i,
		 sane_strstatus (status));
	    return status;
	  }
      }

  for (i = 0; i < 3; i++)
      {
	status =
	  sanei_genesys_fe_write_data (dev, 0x20 + i, dev->frontend.offset[i]);
	if (status != SANE_STATUS_GOOD)
	  {
	    DBG (DBG_error,
		 "gl843_set_fe: writing offset[%d] failed: %s\n", i,
		 sane_strstatus (status));
	    return status;
	  }
      }

  for (i = 0; i < 3; i++)
      {
	status =
	  sanei_genesys_fe_write_data (dev, 0x24 + i, dev->frontend.sign[i]);
	if (status != SANE_STATUS_GOOD)
	  {
	    DBG (DBG_error,
		 "gl843_set_fe: writing sign[%d] failed: %s\n", i,
		 sane_strstatus (status));
	    return status;
	  }
      }

  for (i = 0; i < 3; i++)
      {
	status =
	  sanei_genesys_fe_write_data (dev, 0x28 + i, dev->frontend.gain[i]);
	if (status != SANE_STATUS_GOOD)
	  {
	    DBG (DBG_error,
		 "gl843_set_fe: writing gain[%d] failed: %s\n", i,
		 sane_strstatus (status));
	    return status;
	  }
      }

  DBGCOMPLETED;
  return SANE_STATUS_GOOD;
}

#define MOTOR_FLAG_AUTO_GO_HOME             1
#define MOTOR_FLAG_DISABLE_BUFFER_FULL_MOVE 2

#define MOTOR_ACTION_FEED       1
#define MOTOR_ACTION_GO_HOME    2
#define MOTOR_ACTION_HOME_FREE  3

/** @brief setup motor for off mode
 * 
 */
static SANE_Status
gl843_init_motor_regs_off (Genesys_Register_Set * reg,
			   unsigned int scan_lines)
{
  unsigned int feedl;
  Genesys_Register_Set *r;

  DBG (DBG_proc, "gl843_init_motor_regs_off : scan_lines=%d\n", scan_lines);

  feedl = 2;

/* all needed slopes available. we did even decide which mode to use. 
   what next?
   - transfer slopes
SCAN:
flags \ use_fast_fed    ! 0         1
------------------------\--------------------
                      0 ! 0,1,2     0,1,2,3
MOTOR_FLAG_AUTO_GO_HOME ! 0,1,2,4   0,1,2,3,4
OFF:       none
FEED:      3
GO_HOME:   3
HOME_FREE: 3
   - setup registers
     * slope specific registers (already done)
     * DECSEL for HOME_FREE/GO_HOME/SCAN
     * FEEDL
     * MTRREV
     * MTRPWR
     * FASTFED
     * STEPSEL
     * MTRPWM
     * FSTPSEL
     * FASTPWM
     * HOMENEG
     * BWDSTEP
     * FWDSTEP
     * Z1
     * Z2
 */
  r = sanei_genesys_get_address (reg, 0x3d);
  r->value = (feedl >> 16) & 0xf;
  r = sanei_genesys_get_address (reg, 0x3e);
  r->value = (feedl >> 8) & 0xff;
  r = sanei_genesys_get_address (reg, 0x3f);
  r->value = feedl & 0xff;
  DBG (DBG_io ,"%s: feedl=%d\n",__FUNCTION__,feedl);

  r = sanei_genesys_get_address (reg, 0x25);
  r->value = (scan_lines >> 16) & 0xf;
  r = sanei_genesys_get_address (reg, 0x26);
  r->value = (scan_lines >> 8) & 0xff;
  r = sanei_genesys_get_address (reg, 0x27);
  r->value = scan_lines & 0xff;

  r = sanei_genesys_get_address (reg, REG02);
  r->value &= ~0x01;		/*LONGCURV OFF */
  r->value &= ~0x80;		/*NOT_HOME OFF */

  r->value &= ~0x10;

  r->value &= ~0x06;

  r->value &= ~0x08;

  r->value &= ~0x20;

  r->value &= ~0x40;

  r = sanei_genesys_get_address (reg, REG67);
  r->value = REG67_MTRPWM;

  r = sanei_genesys_get_address (reg, REG68);
  r->value = REG68_FASTPWM;

  r = sanei_genesys_get_address (reg, 0x21);
  r->value = 1;

  r = sanei_genesys_get_address (reg, 0x24);
  r->value = 1;

  r = sanei_genesys_get_address (reg, 0x69);
  r->value = 1;

  r = sanei_genesys_get_address (reg, 0x6a);
  r->value = 1;

  r = sanei_genesys_get_address (reg, 0x5f);
  r->value = 1;


  DBGCOMPLETED;
  return SANE_STATUS_GOOD;
}

static SANE_Status
gl843_init_motor_regs (Genesys_Device * dev, Genesys_Register_Set * reg, unsigned int feed_steps,	/*1/base_ydpi */
/*maybe float for half/quarter step resolution?*/
		       unsigned int action, unsigned int flags)
{
  SANE_Status status;
  unsigned int fast_exposure;
  int use_fast_fed = 0;
  uint16_t fast_slope_table[256];
  uint8_t val;
  unsigned int fast_slope_time;
  unsigned int fast_slope_steps = 32;
  unsigned int feedl;
  Genesys_Register_Set *r;
/*number of scan lines to add in a scan_lines line*/

  DBG (DBG_proc,
       "gl843_init_motor_regs : feed_steps=%d, action=%d, flags=%x\n",
       feed_steps, action, flags);

  if (action == MOTOR_ACTION_FEED || action == MOTOR_ACTION_GO_HOME || action == MOTOR_ACTION_HOME_FREE)
    {
      /* FEED and GO_HOME can use fastest slopes available */
      fast_slope_steps = 256;
      fast_exposure = sanei_genesys_exposure_time2 (dev,
                                                    dev->motor.base_ydpi / 4,
                                                    0,	/*step_type */
						    0,	/*last used pixel */
						    0,
                                                    0);

      DBG (DBG_info, "gl843_init_motor_regs : fast_exposure=%d pixels\n",
	   fast_exposure);
    }

/* HOME_FREE must be able to stop in one step, so do not try to get faster */
  /* XXX STEF XXX
  if (action == MOTOR_ACTION_HOME_FREE)
    {
      fast_slope_steps = 256;
      fast_exposure = dev->motor.slopes[0][0].maximum_start_speed;
    }
    */

  fast_slope_time = sanei_genesys_create_slope_table3 (dev,
						       fast_slope_table,
						       256,
						       fast_slope_steps,
						       0,
						       fast_exposure,
						       dev->motor.base_ydpi / 4,
                                                       &fast_slope_steps,
						       &fast_exposure,
                                                       0);

  feedl = feed_steps - fast_slope_steps * 2;
  use_fast_fed = 1;

/* all needed slopes available. we did even decide which mode to use. 
   what next?
   - transfer slopes
SCAN:
flags \ use_fast_fed    ! 0         1
------------------------\--------------------
                      0 ! 0,1,2     0,1,2,3
MOTOR_FLAG_AUTO_GO_HOME ! 0,1,2,4   0,1,2,3,4
OFF:       none
FEED:      3
GO_HOME:   3
HOME_FREE: 3
   - setup registers
     * slope specific registers (already done)
     * DECSEL for HOME_FREE/GO_HOME/SCAN
     * FEEDL
     * MTRREV
     * MTRPWR
     * FASTFED
     * STEPSEL
     * MTRPWM
     * FSTPSEL
     * FASTPWM
     * HOMENEG
     * BWDSTEP
     * FWDSTEP
     * Z1
     * Z2
 */

  r = sanei_genesys_get_address (reg, 0x3d);
  r->value = (feedl >> 16) & 0xf;
  r = sanei_genesys_get_address (reg, 0x3e);
  r->value = (feedl >> 8) & 0xff;
  r = sanei_genesys_get_address (reg, 0x3f);
  r->value = feedl & 0xff;
  DBG (DBG_io ,"%s: feedl=%d\n",__FUNCTION__,feedl);

  r = sanei_genesys_get_address (reg, 0x25);
  r->value = 0;
  r = sanei_genesys_get_address (reg, 0x26);
  r->value = 0;
  r = sanei_genesys_get_address (reg, 0x27);
  r->value = 0;

  /* set REG 02 */
  r = sanei_genesys_get_address (reg, REG02);
  r->value &= ~REG02_LONGCURV;
  r->value &= ~REG02_HOMENEG;
  r->value &= ~REG02_AGOHOME;
  r->value &= ~REG02_ACDCDIS;

  if (flags & MOTOR_FLAG_DISABLE_BUFFER_FULL_MOVE)
    r->value |= REG02_ACDCDIS;

  r->value |= REG02_MTRPWR;

  if (action == MOTOR_ACTION_GO_HOME)
    r->value |= (REG02_MTRREV | REG02_NOTHOME);
  else
    r->value &= ~(REG02_MTRREV | REG02_HOMENEG);

  if (use_fast_fed)
    r->value |= REG02_FASTFED;
  else
    r->value &= ~REG02_FASTFED;

  if (flags & MOTOR_FLAG_AUTO_GO_HOME)
    r->value |= REG02_AGOHOME;

  /* reset gpio pin */
  RIE (sanei_genesys_read_register (dev, REG6C, &val));
  val |= REG6C_GPIO13;
  RIE (sanei_genesys_write_register (dev, REG6C, val));
  RIE (sanei_genesys_read_register (dev, REG6C, &val));
  val |= REG6C_GPIO12;
  RIE (sanei_genesys_write_register (dev, REG6C, val));

  status = gl843_send_slope_table (dev, 0, fast_slope_table, 256);
  status = gl843_send_slope_table (dev, 1, fast_slope_table, 256);
  /* XXX STEF XXX status = gl843_send_slope_table (dev, 2, fast_slope_table, 256); */
  status = gl843_send_slope_table (dev, 3, fast_slope_table, 256);
  status = gl843_send_slope_table (dev, 4, fast_slope_table, 256);

  if (status != SANE_STATUS_GOOD)
    return status;

  r = sanei_genesys_get_address (reg, REG67);
  r->value = REG67_MTRPWM;

  r = sanei_genesys_get_address (reg, REG68);
  r->value = REG68_FASTPWM;

  r = sanei_genesys_get_address (reg, 0x21);
  r->value = 1;

  r = sanei_genesys_get_address (reg, 0x24);
  r->value = 1;

  r = sanei_genesys_get_address (reg, REG67);
  r->value = 0 << REG67S_STEPSEL;

  r = sanei_genesys_get_address (reg, REG68);
  r->value = 0 << REG68S_FSTPSEL;

  r = sanei_genesys_get_address (reg, 0x69);
  r->value = 1;

  r = sanei_genesys_get_address (reg, 0x6a);
  r->value = fast_slope_steps;

  r = sanei_genesys_get_address (reg, 0x5f);
  r->value = fast_slope_steps;

  DBG (DBG_proc, "gl843_init_motor_regs : completed. \n");

  return SANE_STATUS_GOOD;
}

/** @brief set up motor related register for scan
 * the following registers are modified:
 * 0x02 
 *
   - transfer slopes
SCAN:
flags \ use_fast_fed    ! 0         1
------------------------\--------------------
                      0 ! 0,1,2     0,1,2,3
MOTOR_FLAG_AUTO_GO_HOME ! 0,1,2,4   0,1,2,3,4
OFF:       none
FEED:      3
GO_HOME:   3
HOME_FREE: 3
   - setup registers
     * slope specific registers (already done)
     * DECSEL for HOME_FREE/GO_HOME/SCAN
     * FEEDL
     * MTRREV
     * MTRPWR
     * FASTFED
     * STEPSEL
     * MTRPWM
     * FSTPSEL
     * FASTPWM
     * HOMENEG
     * BWDSTEP
     * FWDSTEP
     * Z1
     * Z2
     */
static SANE_Status
gl843_init_motor_regs_scan (Genesys_Device * dev,
                            Genesys_Register_Set * reg,
                            unsigned int scan_exposure_time,	/*pixel */
			    float scan_yres,	/*dpi, motor resolution */
			    int scan_step_type,	/*0: full, 1: half, 2: quarter */
			    unsigned int scan_lines,	/*lines, scan resolution */
			    unsigned int scan_dummy,
/*number of scan lines to add in a scan_lines line*/
			    unsigned int feed_steps,	/*1/base_ydpi */
/*maybe float for half/quarter step resolution?*/
			    int scan_power_mode, unsigned int flags)
{
  SANE_Status status;
  unsigned int fast_exposure;
  int use_fast_fed = 0;
  unsigned int fast_time;
  unsigned int slow_time;
  uint16_t slow_slope_table[256];
  uint16_t fast_slope_table[256];
  uint16_t back_slope_table[256];
  unsigned int slow_slope_time;
  unsigned int fast_slope_time;
  unsigned int back_slope_time;
  unsigned int slow_slope_steps = 0;
  unsigned int fast_slope_steps = 32;
  unsigned int back_slope_steps = 0;
  unsigned int feedl;
  Genesys_Register_Set *r;
  unsigned int min_restep = 0x20;
  uint32_t z1, z2;
  uint8_t val, effective;

  DBGSTART;
  DBG (DBG_proc, "gl843_init_motor_regs_scan : scan_exposure_time=%d, "
       "scan_yres=%g, scan_step_type=%d, scan_lines=%d, scan_dummy=%d, "
       "feed_steps=%d, scan_power_mode=%d, flags=%x\n",
       scan_exposure_time,
       scan_yres,
       scan_step_type,
       scan_lines, scan_dummy, feed_steps, scan_power_mode, flags);

  fast_exposure =
    sanei_genesys_exposure_time2 (dev, dev->motor.base_ydpi / 4,
				  0, 0, 0, scan_power_mode);

  DBG (DBG_info, "gl843_init_motor_regs_scan : fast_exposure=%d pixels\n",
       fast_exposure);

/*
  we calculate both tables for SCAN. the fast slope step count depends on
  how many steps we need for slow acceleration and how much steps we are
  allowed to use.
 */
  slow_slope_time = sanei_genesys_create_slope_table3 (dev,
						       slow_slope_table,
						       256,
						       256,
						       scan_step_type,
						       scan_exposure_time,
						       scan_yres,
						       &slow_slope_steps,
						       NULL, scan_power_mode);

  back_slope_time = sanei_genesys_create_slope_table3 (dev,
						       back_slope_table,
						       256,
						       256,
						       scan_step_type,
						       scan_exposure_time,
						       scan_yres,
						       &back_slope_steps,
						       NULL, scan_power_mode);

  if (feed_steps < (slow_slope_steps >> scan_step_type))
    {
      /*TODO: what should we do here?? go back to exposure calculation? */
      feed_steps = slow_slope_steps >> scan_step_type;
    }

  if (feed_steps > fast_slope_steps * 2 -
      (slow_slope_steps >> scan_step_type))
    fast_slope_steps = 256;
  else
    {
      /* we need to shorten fast_slope_steps here. */
      fast_slope_steps = (feed_steps - (slow_slope_steps >> scan_step_type)) / 2;
    }
  if(fast_slope_steps>256)
    fast_slope_steps=256;

  DBG (DBG_info,
       "gl843_init_motor_regs_scan: Maximum allowed slope steps for fast slope: %d\n",
       fast_slope_steps);

  fast_slope_time = sanei_genesys_create_slope_table3 (dev,
						       fast_slope_table,
						       256,
						       fast_slope_steps,
						       0,
						       fast_exposure,
						       dev->motor.base_ydpi / 4,
                                                       &fast_slope_steps,
						       &fast_exposure,
						       scan_power_mode);

  if (feed_steps <
      fast_slope_steps * 2 + (slow_slope_steps >> scan_step_type))
    {
      use_fast_fed = 0;
      DBG (DBG_info,
	   "gl843_init_motor_regs_scan: feed too short, slow move forced.\n");
    }
  else
    {
/* for deciding whether we should use fast mode we need to check how long we 
   need for (fast)accelerating, moving, decelerating, (TODO: stopping?) 
   (slow)accelerating again versus (slow)accelerating and moving. we need 
   fast and slow tables here.
*/
/*NOTE: scan_exposure_time is per scan_yres*/
/*NOTE: fast_exposure is per base_ydpi/4*/
/*we use full steps as base unit here*/
      fast_time =
	fast_exposure / 4 *
	(feed_steps - fast_slope_steps * 2 -
	 (slow_slope_steps >> scan_step_type))
	+ fast_slope_time * 2 + slow_slope_time;
      slow_time =
	(scan_exposure_time * scan_yres) / dev->motor.base_ydpi *
	(feed_steps - (slow_slope_steps >> scan_step_type)) + slow_slope_time;

      DBG (DBG_info, "gl843_init_motor_regs_scan: Time for slow move: %d\n",
	   slow_time);
      DBG (DBG_info, "gl843_init_motor_regs_scan: Time for fast move: %d\n",
	   fast_time);

      use_fast_fed = fast_time < slow_time;
    }
  
  DBG (DBG_info, "gl843_init_motor_regs_scan: decided to use %s mode\n",
       use_fast_fed ? "fast feed" : "slow feed");

  if (use_fast_fed)
    feedl = feed_steps - fast_slope_steps * 2 -
      (slow_slope_steps >> scan_step_type);
  else if ((feed_steps << scan_step_type) < slow_slope_steps)
    feedl = 0;
  else
    feedl = (feed_steps << scan_step_type) - slow_slope_steps;


/* all needed slopes available. we did even decide which mode to use. 
   what next?  */
  r = sanei_genesys_get_address (reg, 0x3d);
  r->value = (feedl >> 16) & 0xf;
  r = sanei_genesys_get_address (reg, 0x3e);
  r->value = (feedl >> 8) & 0xff;
  r = sanei_genesys_get_address (reg, 0x3f);
  r->value = feedl & 0xff;
  DBG (DBG_io ,"%s: feedl=%d\n",__FUNCTION__,feedl);

  r = sanei_genesys_get_address (reg, 0x25);
  r->value = (scan_lines >> 16) & 0xf;
  r = sanei_genesys_get_address (reg, 0x26);
  r->value = (scan_lines >> 8) & 0xff;
  r = sanei_genesys_get_address (reg, 0x27);
  r->value = scan_lines & 0xff;

  /* compute register 02 value */
  r = sanei_genesys_get_address (reg, REG02);
  r->value = 0x00;
  r->value |= REG02_NOTHOME | REG02_MTRPWR;

  if (use_fast_fed)
    r->value |= REG02_FASTFED;
  else
    r->value &= ~REG02_FASTFED;

  if (flags & MOTOR_FLAG_AUTO_GO_HOME)
    r->value |= REG02_AGOHOME;

  if (flags & MOTOR_FLAG_DISABLE_BUFFER_FULL_MOVE)
    r->value |= REG02_ACDCDIS;

  /* hi res motor speed */
  RIE (sanei_genesys_read_register (dev, REG6C, &effective));

  /* if quarter step, bipolar Vref2 */
  if (scan_step_type > 1)
    {
      val = effective & ~REG6C_GPIO13;
    }
  else
    {
      val = effective;
    }
  RIE (sanei_genesys_write_register (dev, REG6C, val));

  /* effective scan */
  RIE (sanei_genesys_read_register (dev, REG6C, &effective));
  val = effective | REG6C_GPIO10;
  RIE (sanei_genesys_write_register (dev, REG6C, val));

  status = gl843_send_slope_table (dev, 0, slow_slope_table, 256);

  if (status != SANE_STATUS_GOOD)
    return status;

  status = gl843_send_slope_table (dev, 1, back_slope_table, 256);

  if (status != SANE_STATUS_GOOD)
    return status;

  /* XXX STEF XXX status = gl843_send_slope_table (dev, 2, slow_slope_table, 256); */

  if (status != SANE_STATUS_GOOD)
    return status;

  if (use_fast_fed)
    {
      status = gl843_send_slope_table (dev, 3, fast_slope_table, 256);

      if (status != SANE_STATUS_GOOD)
	return status;
    }

  if (flags & MOTOR_FLAG_AUTO_GO_HOME)
    {
      status = gl843_send_slope_table (dev, 4, fast_slope_table, 256);

      if (status != SANE_STATUS_GOOD)
	return status;
    }


/* now reg 0x21 and 0x24 are available, we can calculate reg 0x22 and 0x23,
   reg 0x60-0x62 and reg 0x63-0x65
   rule:
   2*STEPNO+FWDSTEP=2*FASTNO+BWDSTEP
*/
/* steps of table 0*/
  if (min_restep < slow_slope_steps * 2 + 2)
    min_restep = slow_slope_steps * 2 + 2;
/* steps of table 1*/
  if (min_restep < back_slope_steps * 2 + 2)
    min_restep = back_slope_steps * 2 + 2;
/* steps of table 0*/
  r = sanei_genesys_get_address (reg, 0x22);
  r->value = min_restep - slow_slope_steps * 2;

/* steps of table 1*/
  r = sanei_genesys_get_address (reg, 0x23);
  r->value = min_restep - back_slope_steps * 2;


/*
  for z1/z2:
  in documentation mentioned variables a-d:
  a = time needed for acceleration, table 1
  b = time needed for reg 0x1f... wouldn't that be reg0x1f*exposure_time?
  c = time needed for acceleration, table 1
  d = time needed for reg 0x22... wouldn't that be reg0x22*exposure_time?
  z1 = (c+d-1) % exposure_time 
  z2 = (a+b-1) % exposure_time
*/
/* i don't see any effect of this. i can only guess that this will enhance 
   sub-pixel accuracy
   z1 = (slope_0_time-1) % exposure_time;
   z2 = (slope_0_time-1) % exposure_time;
*/

  sanei_genesys_calculate_zmode2 (use_fast_fed,
				  scan_exposure_time,
				  slow_slope_table,
				  slow_slope_steps,
				  feedl,
                                  back_slope_steps,
                                  &z1,
                                  &z2);

  DBG (DBG_info, "gl843_init_motor_regs_scan: z1 = %d\n", z1);
  r = sanei_genesys_get_address (reg, REG60);
  r->value = ((z1 >> 16) & REG60_Z1MOD);
  r = sanei_genesys_get_address (reg, REG61);
  r->value = ((z1 >> 8) & REG61_Z1MOD);
  r = sanei_genesys_get_address (reg, REG62);
  r->value = (z1 & REG62_Z1MOD);

  DBG (DBG_info, "gl843_init_motor_regs_scan: z2 = %d\n", z2);
  r = sanei_genesys_get_address (reg, REG63);
  r->value = ((z2 >> 16) & REG63_Z2MOD);
  r = sanei_genesys_get_address (reg, REG64);
  r->value = ((z2 >> 8) & REG64_Z2MOD);
  r = sanei_genesys_get_address (reg, REG65);
  r->value = (z2 & REG65_Z2MOD);

  r = sanei_genesys_get_address (reg, REG1E);
  r->value &= 0xf0;		/* 0 dummy lines */
  r->value |= scan_dummy;	/* dummy lines */

  r = sanei_genesys_get_address (reg, REG67);
  r->value = REG67_MTRPWM | (scan_step_type << REG67S_STEPSEL);

  r = sanei_genesys_get_address (reg, REG68);
  r->value = REG68_FASTPWM | (scan_step_type << REG68S_FSTPSEL);

  r = sanei_genesys_get_address (reg, 0x21);
  r->value = slow_slope_steps;

  r = sanei_genesys_get_address (reg, 0x24);
  r->value = back_slope_steps;

  r = sanei_genesys_get_address (reg, 0x69);
  r->value = slow_slope_steps;

  r = sanei_genesys_get_address (reg, 0x6a);
  r->value = fast_slope_steps;

  r = sanei_genesys_get_address (reg, 0x5f);
  r->value = fast_slope_steps;

  DBGCOMPLETED;
  return SANE_STATUS_GOOD;
}

static int
gl843_get_dpihw (Genesys_Device * dev)
{
  Genesys_Register_Set *r;
  r = sanei_genesys_get_address (dev->reg, REG05);
  if ((r->value & REG05_DPIHW) == REG05_DPIHW_600)
    return 600;
  if ((r->value & REG05_DPIHW) == REG05_DPIHW_1200)
    return 1200;
  if ((r->value & REG05_DPIHW) == REG05_DPIHW_2400)
    return 2400;
  if ((r->value & REG05_DPIHW) == REG05_DPIHW_4800)
    return 4800;
  return 0;
}

#define OPTICAL_FLAG_DISABLE_GAMMA   1
#define OPTICAL_FLAG_DISABLE_SHADING 2
#define OPTICAL_FLAG_DISABLE_LAMP    4
#define OPTICAL_FLAG_ENABLE_LEDADD   8

static SANE_Status
gl843_init_optical_regs_off (Genesys_Register_Set * reg)
{
  Genesys_Register_Set *r;

  DBG (DBG_proc, "gl843_init_optical_regs_off : start\n");

  r = sanei_genesys_get_address (reg, REG01);
  r->value &= ~REG01_SCAN;

  DBG (DBG_proc, "gl843_init_optical_regs_off : completed. \n");
  return SANE_STATUS_GOOD;
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
static SANE_Status
gl843_init_optical_regs_scan (Genesys_Device * dev,
			      Genesys_Register_Set * reg,
			      unsigned int exposure_time,
			      int used_res,
			      unsigned int start,
			      unsigned int pixels,
			      int channels,
			      int depth,
			      SANE_Bool half_ccd, int color_filter, int flags)
{
  unsigned int words_per_line;
  unsigned int startx,endx, used_pixels,max_pixels;
  unsigned int dpiset;
  unsigned int i,bytes;
  Genesys_Register_Set *r;
  SANE_Status status;
  int double_xres;

  DBG (DBG_proc, "gl843_init_optical_regs_scan :  exposure_time=%d, "
       "used_res=%d, start=%d, pixels=%d, channels=%d, depth=%d, "
       "half_ccd=%d, flags=%x\n",
       exposure_time,
       used_res, start, pixels, channels, depth, half_ccd, flags);

  /* during calibration , we don't want double xres */
  if(dev->settings.double_xres==SANE_TRUE&&used_res<dev->sensor.optical_res)
    {
      double_xres=SANE_TRUE;
    }
  else
    {
      double_xres=SANE_FALSE;
    }

  startx = dev->sensor.dummy_pixel + 1 + dev->sensor.CCD_start_xoffset;
  if(double_xres==SANE_TRUE)
    {
      max_pixels = dev->sensor.sensor_pixels/2;
    }
  else
    {
      max_pixels = dev->sensor.sensor_pixels;
    }
  if(pixels<max_pixels)
    {
      used_pixels = max_pixels;
    }
  else
    {
      used_pixels = pixels;
    }
  endx = startx + used_pixels;

  status = gl843_set_fe (dev, AFE_SET);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl843_init_optical_regs_scan: failed to set frontend: %s\n",
	   sane_strstatus (status));
      return status;
    }

  /* adjust used_res for chosen dpihw */
  used_res = used_res * gl843_get_dpihw (dev) / dev->sensor.optical_res;
  if(double_xres==SANE_TRUE)
    {
      used_res *=2;
    }
  dpiset = used_res;

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
  if (flags & OPTICAL_FLAG_DISABLE_LAMP)
    r->value &= ~REG03_LAMPPWR;
  else
    r->value |= REG03_LAMPPWR;

  /* exposure times */
  for (i = 0; i < 6; i++)
    {
      r = sanei_genesys_get_address (reg, 0x10 + i);
      if (flags & OPTICAL_FLAG_DISABLE_LAMP)
	r->value = 0x01;	/* 0x0101 is as off as possible */
      else
	r->value = dev->sensor.regs_0x10_0x1d[i];
    }

  r = sanei_genesys_get_address (reg, 0x19);
  r->value = 0x50;

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
	case 0:
	  r->value |= 0x14;	/* red filter */
	  break;
	case 2:
	  r->value |= 0x1c;	/* blue filter */
	  break;
	default:
	  r->value |= 0x18;	/* green filter */
	  break;
	}
    }
  else
    r->value |= 0x10;		/* mono */

  /* enable gamma tables */
  r = sanei_genesys_get_address (reg, REG05);
  if (flags & OPTICAL_FLAG_DISABLE_GAMMA)
    r->value &= ~REG05_GMMENB;
  else
    r->value |= REG05_GMMENB;

  /* sensor parameters */
  gl843_setup_sensor (dev, dev->reg);

  r = sanei_genesys_get_address (reg, 0x2c);
  r->value = HIBYTE (dpiset);
  r = sanei_genesys_get_address (reg, 0x2d);
  r->value = LOBYTE (dpiset);
  DBG (DBG_io2, "%s: dpiset used=%d\n", __FUNCTION__, dpiset);

  r = sanei_genesys_get_address (reg, 0x30);
  r->value = HIBYTE (startx);
  r = sanei_genesys_get_address (reg, 0x31);
  r->value = LOBYTE (startx);
  r = sanei_genesys_get_address (reg, 0x32);
  r->value = HIBYTE (endx);
  r = sanei_genesys_get_address (reg, 0x33);
  r->value = LOBYTE (endx);

  /* words(16bit) before gamma, conversion to 8 bit or lineart*/
  words_per_line = (used_pixels * dpiset) / gl843_get_dpihw (dev);
  bytes=depth/8;
  if (depth == 1)
    {
      words_per_line = (words_per_line >> 3) + ((words_per_line & 7) ? 1 : 0);
    }
  else
    {
      words_per_line *= bytes;
    }

  dev->bpl = words_per_line;
  dev->cur=0;
  dev->len=((pixels*dpiset)/gl843_get_dpihw (dev))/2*bytes;
  dev->dist=dev->bpl/2;
  dev->skip=((start*dpiset)/gl843_get_dpihw (dev))/2*bytes;
  if(dev->skip>=dev->dist && double_xres==SANE_FALSE)
    {
      dev->skip-=dev->dist;
    }
  DBG (DBG_io2, "%s: used_pixels=%d\n", __FUNCTION__, used_pixels);
  DBG (DBG_io2, "%s: pixels     =%d\n", __FUNCTION__, pixels);
  DBG (DBG_io2, "%s: depth      =%d\n", __FUNCTION__, depth);
  DBG (DBG_io2, "%s: dev->bpl   =%lu\n", __FUNCTION__, (unsigned long)dev->bpl);
  DBG (DBG_io2, "%s: dev->len   =%lu\n", __FUNCTION__, (unsigned long)dev->len);
  DBG (DBG_io2, "%s: dev->dist  =%lu\n", __FUNCTION__, (unsigned long)dev->dist);
  DBG (DBG_io2, "%s: dev->skip  =%lu\n", __FUNCTION__, (unsigned long)dev->skip);
  
  words_per_line *= channels;
  dev->wpl = words_per_line;

  if(dev->oe_buffer.buffer!=NULL)
    {
      sanei_genesys_buffer_free (&(dev->oe_buffer));
    }
  RIE (sanei_genesys_buffer_alloc (&(dev->oe_buffer), dev->wpl));


  /* MAXWD is expressed in 4 words unit */
  r = sanei_genesys_get_address (reg, 0x35);
  r->value = LOBYTE (HIWORD (words_per_line >> 2));
  r = sanei_genesys_get_address (reg, 0x36);
  r->value = HIBYTE (LOWORD (words_per_line >> 2));
  r = sanei_genesys_get_address (reg, 0x37);
  r->value = LOBYTE (LOWORD (words_per_line >> 2));
  DBG (DBG_io2, "%s: words_per_line used=%d\n", __FUNCTION__, words_per_line);

  r = sanei_genesys_get_address (reg, 0x38);
  r->value = HIBYTE (exposure_time);
  r = sanei_genesys_get_address (reg, 0x39);
  r->value = LOBYTE (exposure_time);
  DBG (DBG_io2, "%s: exposure_time used=%d\n", __FUNCTION__, exposure_time);

  r = sanei_genesys_get_address (reg, 0x34);
  r->value = dev->sensor.dummy_pixel;
  
  DBG (DBG_proc, "gl843_init_optical_regs_scan : completed. \n");
  return SANE_STATUS_GOOD;
}


static int
gl843_get_led_exposure (Genesys_Device * dev)
{
  int d, r, g, b, m;
  if (!dev->model->is_cis)
    return 0;
  d = dev->reg[reg_0x19].value;
  r = dev->sensor.regs_0x10_0x1d[1] | (dev->sensor.regs_0x10_0x1d[0] << 8);
  g = dev->sensor.regs_0x10_0x1d[3] | (dev->sensor.regs_0x10_0x1d[2] << 8);
  b = dev->sensor.regs_0x10_0x1d[5] | (dev->sensor.regs_0x10_0x1d[4] << 8);

  m = r;
  if (m < g)
    m = g;
  if (m < b)
    m = b;

  return m + d;
}

/* set up registers for an actual scan
 *
 * this function sets up the scanner to scan in normal or single line mode
 */
#ifndef UNIT_TESTING
static
#endif
  SANE_Status
gl843_init_scan_regs (Genesys_Device * dev,
                      Genesys_Register_Set * reg,
                      float xres,	/*dpi */
		      float yres,	/*dpi */
		      float startx,	/*optical_res, from dummy_pixel+1 */
		      float starty,	/*base_ydpi, from home! */
		      float pixels,
		      float lines,
		      unsigned int depth,
		      unsigned int channels,
		      int color_filter, unsigned int flags)
{
  int used_res;
  int start, used_pixels;
  int bytes_per_line;
  int move;
  unsigned int lincnt;
  unsigned int oflags; /**> optical flags */
  int exposure_time, exposure_time2, led_exposure;
  int i;
  int stagger;

  int slope_dpi = 0;
  int pixels_exposure;
  int dummy = 0;
  int scan_step_type = 1;
  int scan_power_mode = 0;
  int max_shift;
  size_t requested_buffer_size, read_buffer_size;

  SANE_Bool half_ccd;		/* false: full CCD res is used, true, half max CCD res is used */
  int optical_res;
  SANE_Status status;

  DBG (DBG_info,
       "gl843_init_scan_regs settings:\n"
       "Resolution    : %gDPI/%gDPI\n"
       "Lines         : %g\n"
       "PPL           : %g\n"
       "Startpos      : %g/%g\n"
       "Depth/Channels: %u/%u\n"
       "Flags         : %x\n\n",
       xres, yres, lines, pixels, startx, starty, depth, channels, flags);

/*
results:

for scanner:
half_ccd
start
end
dpiset
exposure_time
dummy
z1
z2

for ordered_read:
  dev->words_per_line
  dev->read_factor
  dev->requested_buffer_size
  dev->read_buffer_size
  dev->read_pos
  dev->read_bytes_in_buffer
  dev->read_bytes_left
  dev->max_shift
  dev->stagger

independent of our calculated values:
  dev->total_bytes_read
  dev->bytes_to_read
 */

/* half_ccd */
  /* we have 2 domains for ccd: xres below or above half ccd max dpi */
  if (dev->sensor.optical_res < 2 * xres ||
      !(dev->model->flags & GENESYS_FLAG_HALF_CCD_MODE))
    {
      half_ccd = SANE_FALSE;
    }
  else
    {
      half_ccd = SANE_TRUE;
    }

/* optical_res */

  optical_res = dev->sensor.optical_res;
  if (half_ccd)
    optical_res /= 2;

/* stagger */

  if ((!half_ccd) && (dev->model->flags & GENESYS_FLAG_STAGGERED_LINE))
    stagger = (4 * yres) / dev->motor.base_ydpi;
  else
    stagger = 0;
  DBG (DBG_info, "gl843_init_scan_regs : stagger=%d lines\n", stagger);

  /* used_res */
  i = optical_res / xres;
#if 0
/* gl843 supports 1/2,1/3,1/4,1/5,1/6,1/8,1/10,1/12,1/15 */

  if (i < 2 || (flags & SCAN_FLAG_USE_OPTICAL_RES))	/* optical_res >= xres > optical_res/2 */
    used_res = optical_res;
  else if (i < 3)		/* optical_res/2 >= xres > optical_res/3 */
    used_res = optical_res / 2;
  else if (i < 4)		/* optical_res/3 >= xres > optical_res/4 */
    used_res = optical_res / 3;
  else if (i < 5)		/* optical_res/4 >= xres > optical_res/5 */
    used_res = optical_res / 4;
  else if (i < 6)		/* optical_res/5 >= xres > optical_res/6 */
    used_res = optical_res / 5;
  else if (i < 8)		/* optical_res/6 >= xres > optical_res/8 */
    used_res = optical_res / 6;
  else if (i < 10)		/* optical_res/8 >= xres > optical_res/10 */
    used_res = optical_res / 8;
  else if (i < 12)		/* optical_res/10 >= xres > optical_res/12 */
    used_res = optical_res / 10;
  else if (i < 15)		/* optical_res/12 >= xres > optical_res/15 */
    used_res = optical_res / 12;
  else
    used_res = optical_res / 15;
#endif
  if (flags & SCAN_FLAG_USE_OPTICAL_RES)
    {
      used_res = optical_res;
    }
  else
    {
      /* resolution is choosen from a list */
      used_res = xres;
    }

  /* compute scan parameters values */
  /* pixels are allways given at full optical resolution */
  /* use detected left margin and fixed value */
  /* start */
  /* add x coordinates */
  start = startx;

  if (stagger > 0)
    start |= 1;

  /* compute correct pixels number */
  /* pixels */
  used_pixels = (pixels * optical_res) / xres;

  /* round up pixels number if needed */
  if (used_pixels * xres < pixels * optical_res)
    used_pixels++;

  dummy = 3-channels;

/* slope_dpi */
/* cis color scan is effectively a gray scan with 3 gray lines per color
   line and a FILTER of 0 */
  if (dev->model->is_cis)
    slope_dpi = yres * channels;
  else
    slope_dpi = yres;

  slope_dpi = slope_dpi * (1 + dummy);

  /* scan_step_type */
  switch((int)yres)
    {
    case 75:
    case 100:
    case 150:
      scan_step_type = 0;
      break;
    case 200:
    case 300:
      scan_step_type = 1;
      break;
    default:
      scan_step_type = 2;
    }

  /* exposure_time , CCD case not handled */
  led_exposure = gl843_get_led_exposure (dev);

  pixels_exposure=dev->sensor.sensor_pixels+572;
  if(xres<dev->sensor.optical_res)
    pixels_exposure=(pixels_exposure*xres)/dev->sensor.optical_res-32;
  else
    pixels_exposure=0;

  exposure_time = sanei_genesys_exposure_time2 (dev,
						slope_dpi,
						scan_step_type,
						pixels_exposure,
						led_exposure,
						scan_power_mode);

  while (scan_power_mode + 1 < dev->motor.power_mode_count)
    {
      exposure_time2 = sanei_genesys_exposure_time2 (dev,
						     slope_dpi,
						     scan_step_type,
						     pixels_exposure,
						     led_exposure,
						     scan_power_mode + 1);
      if (exposure_time < exposure_time2)
	break;
      exposure_time = exposure_time2;
      scan_power_mode++;
    }

  DBG (DBG_info, "gl843_init_scan_regs : exposure_time=%d pixels\n",
       exposure_time);
  DBG (DBG_info, "gl843_init_scan_regs : scan_step_type=%d\n",
       scan_step_type);

/*** optical parameters ***/
  /* in case of dynamic lineart, we use an internal 8 bit gray scan
   * to generate 1 lineart data */
  if ((flags & SCAN_FLAG_DYNAMIC_LINEART) && (dev->settings.scan_mode == SCAN_MODE_LINEART))
    {
      depth = 8;
    }
  if (depth == 16)
    flags |= SCAN_FLAG_DISABLE_GAMMA;

  /* we enable true gray for cis scanners only, and just when doing 
   * scan since color calibration is OK for this mode
   */
  oflags = 0;
  if(flags & SCAN_FLAG_DISABLE_SHADING)
    oflags |= OPTICAL_FLAG_DISABLE_SHADING;
  if(flags & SCAN_FLAG_DISABLE_GAMMA)
    oflags |= OPTICAL_FLAG_DISABLE_GAMMA;
  if(flags & SCAN_FLAG_DISABLE_LAMP)
    oflags |= OPTICAL_FLAG_DISABLE_LAMP;
  
  if (dev->model->is_cis && dev->settings.true_gray)
    {
      oflags |= OPTICAL_FLAG_ENABLE_LEDADD;
    }

  status = gl843_init_optical_regs_scan (dev,
					 reg,
					 exposure_time,
					 used_res,
					 start,
					 used_pixels,
					 channels,
					 depth,
					 half_ccd,
					 color_filter,
                                         oflags);

  if (status != SANE_STATUS_GOOD)
    return status;

/*** motor parameters ***/

/* max_shift */
  /* scanned area must be enlarged by max color shift needed */
  /* all values are assumed >= 0 */
  if (channels > 1 && !(flags & SCAN_FLAG_IGNORE_LINE_DISTANCE))
    {
      max_shift = dev->model->ld_shift_r;
      if (dev->model->ld_shift_b > max_shift)
	max_shift = dev->model->ld_shift_b;
      if (dev->model->ld_shift_g > max_shift)
	max_shift = dev->model->ld_shift_g;
      max_shift = (max_shift * yres) / dev->motor.base_ydpi;
    }
  else
    {
      max_shift = 0;
    }

/* lincnt */
  lincnt = lines + max_shift + stagger;

  /* add tl_y to base movement */
  move = starty;
  DBG (DBG_info, "gl843_init_scan_regs: move=%d steps\n", move);

  /* subtract current head position */
  /* XXX STEF XXX
  move -= dev->scanhead_position_in_steps;
  DBG (DBG_info, "gl843_init_scan_regs: move=%d steps\n", move);

  if (move < 0)
    move = 0; */

  /* round it */
/* the move is not affected by dummy -- pierre */
/*  move = ((move + dummy) / (dummy + 1)) * (dummy + 1);
    DBG (DBG_info, "gl843_init_scan_regs: move=%d steps\n", move);*/

  if (flags & SCAN_FLAG_SINGLE_LINE)
    status = gl843_init_motor_regs_off (reg,
					dev->model->is_cis ? lincnt *
					channels : lincnt);
  else
    status = gl843_init_motor_regs_scan (dev,
					 reg,
					 exposure_time,
					 slope_dpi,
					 scan_step_type,
					 dev->model->is_cis ? lincnt *
					 channels : lincnt, dummy, move,
					 scan_power_mode,
					 (flags &
					  SCAN_FLAG_DISABLE_BUFFER_FULL_MOVE)
					 ?
					 MOTOR_FLAG_DISABLE_BUFFER_FULL_MOVE
					 : 0);

  if (status != SANE_STATUS_GOOD)
    return status;


  /*** prepares data reordering ***/

/* words_per_line */
  bytes_per_line = (used_pixels * used_res) / optical_res;
  bytes_per_line = (bytes_per_line * channels * depth) / 8;

  requested_buffer_size = 8 * bytes_per_line;
  /* we must use a round number of bytes_per_line */
  if (requested_buffer_size > BULKIN_MAXSIZE)
    requested_buffer_size =
      (BULKIN_MAXSIZE / bytes_per_line) * bytes_per_line;

  read_buffer_size =
    2 * requested_buffer_size +
    ((max_shift + stagger) * used_pixels * channels * depth) / 8;

  RIE (sanei_genesys_buffer_free (&(dev->read_buffer)));
  RIE (sanei_genesys_buffer_alloc (&(dev->read_buffer), read_buffer_size));

  RIE (sanei_genesys_buffer_free (&(dev->lines_buffer)));
  RIE (sanei_genesys_buffer_alloc (&(dev->lines_buffer), read_buffer_size));

  RIE (sanei_genesys_buffer_free (&(dev->shrink_buffer)));
  RIE (sanei_genesys_buffer_alloc (&(dev->shrink_buffer),
				   requested_buffer_size));

  RIE (sanei_genesys_buffer_free (&(dev->out_buffer)));
  RIE (sanei_genesys_buffer_alloc (&(dev->out_buffer),
				   (8 * dev->settings.pixels * channels *
				    depth) / 8));


  dev->read_bytes_left = bytes_per_line * lincnt;

  DBG (DBG_info,
       "gl843_init_scan_regs: physical bytes to read = %lu\n",
       (u_long) dev->read_bytes_left);
  dev->read_active = SANE_TRUE;


  dev->current_setup.pixels = (used_pixels * used_res) / optical_res;
  dev->current_setup.lines = lincnt;
  dev->current_setup.depth = depth;
  dev->current_setup.channels = channels;
  dev->current_setup.exposure_time = exposure_time;
  dev->current_setup.xres = used_res;
  dev->current_setup.yres = yres;
  dev->current_setup.half_ccd = half_ccd;
  dev->current_setup.stagger = stagger;
  dev->current_setup.max_shift = max_shift + stagger;

/* TODO: should this be done elsewhere? */
  /* scan bytes to send to the frontend */
  /* theory :
     target_size =
     (dev->settings.pixels * dev->settings.lines * channels * depth) / 8;
     but it suffers from integer overflow so we do the following: 

     1 bit color images store color data byte-wise, eg byte 0 contains 
     8 bits of red data, byte 1 contains 8 bits of green, byte 2 contains 
     8 bits of blue.
     This does not fix the overflow, though. 
     644mp*16 = 10gp, leading to an overflow
     -- pierre
   */

  dev->total_bytes_read = 0;
  if (depth == 1 || dev->settings.scan_mode == SCAN_MODE_LINEART)
    dev->total_bytes_to_read =
      ((dev->settings.pixels * dev->settings.lines) / 8 +
       (((dev->settings.pixels * dev->settings.lines) % 8) ? 1 : 0)) *
      channels;
  else
    dev->total_bytes_to_read =
      dev->settings.pixels * dev->settings.lines * channels * (depth / 8);

  DBG (DBG_info, "gl843_init_scan_regs: total bytes to send = %lu\n",
       (u_long) dev->total_bytes_to_read);
/* END TODO */

  DBG (DBG_proc, "gl843_init_scan_regs: completed\n");
  return SANE_STATUS_GOOD;
}

static SANE_Status
gl843_calculate_current_setup (Genesys_Device * dev)
{
  int channels;
  int depth;
  int start;

  float xres;			/*dpi */
  float yres;			/*dpi */
  float startx;			/*optical_res, from dummy_pixel+1 */
  float pixels;
  float lines;
  int color_filter;

  int used_res;
  int used_pixels;
  unsigned int lincnt;
  int exposure_time, exposure_time2, led_exposure;
  int i;
  int stagger;

  int slope_dpi = 0;
  int dummy = 0;
  int scan_step_type = 1;
  int scan_power_mode = 0;
  int max_shift;
  int pixels_exposure;

  SANE_Bool half_ccd;		/* false: full CCD res is used, true, half max CCD res is used */
  int optical_res;

  DBG (DBG_info,
       "gl843_calculate_current_setup settings:\n"
       "Resolution: %uDPI\n"
       "Lines     : %u\n"
       "PPL       : %u\n"
       "Startpos  : %.3f/%.3f\n"
       "Scan mode : %d\n\n",
       dev->settings.yres, dev->settings.lines, dev->settings.pixels,
       dev->settings.tl_x, dev->settings.tl_y, dev->settings.scan_mode);

  /* channels */
  if (dev->settings.scan_mode == 4)	/* single pass color */
    channels = 3;
  else
    channels = 1;

  /* depth */
  depth = dev->settings.depth;
  if (dev->settings.scan_mode == 0)
    depth = 1;

  /* start */
  start = SANE_UNFIX (dev->model->x_offset);
  start += dev->settings.tl_x;
  start = (start * dev->sensor.optical_res) / MM_PER_INCH;


  xres = dev->settings.xres;	/*dpi */
  yres = dev->settings.yres;	/*dpi */
  startx = start;		/*optical_res, from dummy_pixel+1 */
  pixels = dev->settings.pixels;
  lines = dev->settings.lines;
  color_filter = dev->settings.color_filter;


  DBG (DBG_info,
       "gl843_calculate_current_setup settings:\n"
       "Resolution    : %gDPI/%gDPI\n"
       "Lines         : %g\n"
       "PPL           : %g\n"
       "Startpos      : %g\n"
       "Depth/Channels: %u/%u\n\n",
       xres, yres, lines, pixels, startx, depth, channels);

/* half_ccd */
  /* we have 2 domains for ccd: xres below or above half ccd max dpi */
  if ((dev->sensor.optical_res < 2 * xres) ||
      !(dev->model->flags & GENESYS_FLAG_HALF_CCD_MODE))
    {
      half_ccd = SANE_FALSE;
    }
  else
    {
      half_ccd = SANE_TRUE;
    }

/* optical_res */

  optical_res = dev->sensor.optical_res;
  if (half_ccd)
    optical_res /= 2;

/* stagger */

  if ((!half_ccd) && (dev->model->flags & GENESYS_FLAG_STAGGERED_LINE))
    stagger = (4 * yres) / dev->motor.base_ydpi;
  else
    stagger = 0;
  DBG (DBG_info, "gl843_calculate_current_setup: stagger=%d lines\n",
       stagger);

/* used_res */
  i = optical_res / xres;

#if 0
/* gl843 supports 1/1 1/2 1/3 1/4 1/5 1/6 1/8 1/10 1/12 1/15 averaging */
  if (i < 2)			/* optical_res >= xres > optical_res/2 */
    used_res = optical_res;
  else if (i < 3)		/* optical_res/2 >= xres > optical_res/3 */
    used_res = optical_res / 2;
  else if (i < 4)		/* optical_res/3 >= xres > optical_res/4 */
    used_res = optical_res / 3;
  else if (i < 5)		/* optical_res/4 >= xres > optical_res/5 */
    used_res = optical_res / 4;
  else if (i < 6)		/* optical_res/5 >= xres > optical_res/6 */
    used_res = optical_res / 5;
  else if (i < 8)		/* optical_res/6 >= xres > optical_res/8 */
    used_res = optical_res / 6;
  else if (i < 10)		/* optical_res/8 >= xres > optical_res/10 */
    used_res = optical_res / 8;
  else if (i < 12)		/* optical_res/10 >= xres > optical_res/12 */
    used_res = optical_res / 10;
  else if (i < 15)		/* optical_res/12 >= xres > optical_res/15 */
    used_res = optical_res / 12;
  else
    used_res = optical_res / 15;
#endif
  /* resolution is choosen from a fixed list */
  used_res = xres;

  /* compute scan parameters values */
  /* pixels are allways given at half or full CCD optical resolution */
  /* use detected left margin  and fixed value */

  /* compute correct pixels number */
  used_pixels = (pixels * optical_res) / used_res;

/* dummy */
  /* dummy lines: may not be usefull, for instance 250 dpi works with 0 or 1
     dummy line. Maybe the dummy line adds correctness since the motor runs 
     slower (higher dpi) 
   */
/* for cis this creates better aligned color lines:
dummy \ scanned lines
   0: R           G           B           R ...
   1: R        G        B        -        R ...
   2: R      G      B       -      -      R ...
   3: R     G     B     -     -     -     R ...
   4: R    G    B     -   -     -    -    R ...
   5: R    G   B    -   -   -    -   -    R ...
   6: R   G   B   -   -   -   -   -   -   R ...
   7: R   G  B   -  -   -   -  -   -  -   R ...
   8: R  G  B   -  -  -   -  -  -   -  -  R ...
   9: R  G  B  -  -  -  -  -  -  -  -  -  R ...
  10: R  G B  -  -  -  - -  -  -  -  - -  R ...
  11: R  G B  - -  - -  -  - -  - -  - -  R ...
  12: R G  B - -  - -  - -  - -  - - -  - R ...
  13: R G B  - - - -  - - -  - - - -  - - R ...
  14: R G B - - -  - - - - - -  - - - - - R ...
  15: R G B - - - - - - - - - - - - - - - R ...
 -- pierre
 */
  dummy = 3-channels;

/* slope_dpi */
/* cis color scan is effectively a gray scan with 3 gray lines per color
   line and a FILTER of 0 */
  if (dev->model->is_cis)
    slope_dpi = yres * channels;
  else
    slope_dpi = yres;

  slope_dpi = slope_dpi * (1 + dummy);

  /* scan_step_type */
  switch((int)yres)
    {
    case 75:
    case 100:
    case 150:
      scan_step_type = 0;
      break;
    case 200:
    case 300:
      scan_step_type = 1;
      break;
    default:
      scan_step_type = 2;
    }

  led_exposure = gl843_get_led_exposure (dev);

  pixels_exposure=dev->sensor.sensor_pixels+572;
  if(xres<dev->sensor.optical_res)
    pixels_exposure=(pixels_exposure*xres)/dev->sensor.optical_res-32;
  else
    pixels_exposure=0;

/* exposure_time */
  exposure_time = sanei_genesys_exposure_time2 (dev,
						slope_dpi,
						scan_step_type,
						pixels_exposure,
						led_exposure,
						scan_power_mode);

  while (scan_power_mode + 1 < dev->motor.power_mode_count)
    {
      exposure_time2 = sanei_genesys_exposure_time2 (dev,
						     slope_dpi,
						     scan_step_type,
						     pixels_exposure,
						     led_exposure,
						     scan_power_mode + 1);
      if (exposure_time < exposure_time2)
	break;
      exposure_time = exposure_time2;
      scan_power_mode++;
    }

  DBG (DBG_info,
       "gl843_calculate_current_setup : exposure_time=%d pixels\n",
       exposure_time);

/* max_shift */
  /* scanned area must be enlarged by max color shift needed */
  /* all values are assumed >= 0 */
  if (channels > 1)
    {
      max_shift = dev->model->ld_shift_r;
      if (dev->model->ld_shift_b > max_shift)
	max_shift = dev->model->ld_shift_b;
      if (dev->model->ld_shift_g > max_shift)
	max_shift = dev->model->ld_shift_g;
      max_shift = (max_shift * yres) / dev->motor.base_ydpi;
    }
  else
    {
      max_shift = 0;
    }

/* lincnt */
  lincnt = lines + max_shift + stagger;

  dev->current_setup.pixels = (used_pixels * used_res) / optical_res;
  dev->current_setup.lines = lincnt;
  dev->current_setup.depth = depth;
  dev->current_setup.channels = channels;
  dev->current_setup.exposure_time = exposure_time;
  dev->current_setup.xres = used_res;
  dev->current_setup.yres = yres;
  dev->current_setup.half_ccd = half_ccd;
  dev->current_setup.stagger = stagger;
  dev->current_setup.max_shift = max_shift + stagger;

  DBG (DBG_proc, "gl843_calculate_current_setup: completed\n");
  return SANE_STATUS_GOOD;
}

static void
gl843_set_motor_power (Genesys_Register_Set * regs, SANE_Bool set)
{

  DBG (DBG_proc, "gl843_set_motor_power\n");

  if (set)
    {
      sanei_genesys_set_reg_from_set (regs, REG02,
				      sanei_genesys_read_reg_from_set (regs,
								       REG02)
				      | REG02_MTRPWR);
    }
  else
    {
      sanei_genesys_set_reg_from_set (regs, REG02,
				      sanei_genesys_read_reg_from_set (regs,
								       REG02)
				      & ~REG02_MTRPWR);
    }
}

static void
gl843_set_lamp_power (Genesys_Device * dev,
		      Genesys_Register_Set * regs, SANE_Bool set)
{
  Genesys_Register_Set *r;
  int i;

  if (set)
    {
      sanei_genesys_set_reg_from_set (regs, 0x03,
				      sanei_genesys_read_reg_from_set (regs,
								       0x03)
				      | REG03_LAMPPWR);

      for (i = 0; i < 6; i++)
	{
          r = sanei_genesys_get_address (dev->calib_reg, 0x10+i);
	  r->value = dev->sensor.regs_0x10_0x1d[i];
	}
      r = sanei_genesys_get_address (regs, 0x19);
      r->value = 0x50;
    }
  else
    {
      sanei_genesys_set_reg_from_set (regs, 0x03,
				      sanei_genesys_read_reg_from_set (regs,
								       0x03)
				      & ~REG03_LAMPPWR);

      for (i = 0; i < 6; i++)
	{
          r = sanei_genesys_get_address (dev->calib_reg, 0x10+i);
	  r->value = 0x00;
	}
      r = sanei_genesys_get_address (regs, 0x19);
      r->value = 0xff;
    }
}

/*for fast power saving methods only, like disabling certain amplifiers*/
static SANE_Status
gl843_save_power (Genesys_Device * dev, SANE_Bool enable)
{
  DBG (DBG_proc, "gl843_save_power: enable = %d\n", enable);
  if(dev==NULL)
    return SANE_STATUS_INVAL;

  DBGCOMPLETED;
  return SANE_STATUS_GOOD;
}

static SANE_Status
gl843_set_powersaving (Genesys_Device * dev, int delay /* in minutes */ )
{
  SANE_Status status = SANE_STATUS_GOOD;

  DBG (DBG_proc, "gl843_set_powersaving (delay = %d)\n", delay);
  if(dev==NULL)
    return SANE_STATUS_INVAL;
  DBGCOMPLETED;
  return status;
}

static SANE_Status
gl843_start_action (Genesys_Device * dev)
{
  return sanei_genesys_write_register (dev, 0x0f, 0x01);
}

static SANE_Status
gl843_stop_action (Genesys_Device * dev)
{
  Genesys_Register_Set local_reg[GENESYS_GL843_MAX_REGS];
  SANE_Status status;
  uint8_t val40, val;
  unsigned int loop;

  DBG (DBG_proc, "%s\n", __FUNCTION__);

  status = sanei_genesys_get_status (dev, &val);
  if (DBG_LEVEL >= DBG_io)
    {
      print_status (val);
    }

  val40 = 0;
  status = sanei_genesys_read_register (dev, REG40, &val40);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "%s: failed to read home sensor: %s\n", __FUNCTION__,
	   sane_strstatus (status));
      DBG (DBG_proc, "%s: completed\n", __FUNCTION__);
      return status;
    }

  /* only stop action if needed */
  if (!(val40 & REG40_DATAENB) && !(val40 & REG40_MOTMFLG))
    {
      DBG (DBG_info, "%s: already stopped\n", __FUNCTION__);
      DBG (DBG_proc, "%s: completed\n", __FUNCTION__);
      return SANE_STATUS_GOOD;
    }

  memset (local_reg, 0, sizeof (local_reg));

  memcpy (local_reg, dev->reg,
	  GENESYS_GL843_MAX_REGS * sizeof (Genesys_Register_Set));

  gl843_init_optical_regs_off (local_reg);

  gl843_init_motor_regs_off (local_reg, 0);
  status = gl843_bulk_write_register (dev, local_reg, GENESYS_GL843_MAX_REGS);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "%s: failed to bulk write registers: %s\n",
	   __FUNCTION__, sane_strstatus (status));
      return status;
    }

  /* looks like writing the right registers to zero is enough to get the chip 
     out of scan mode into command mode, actually triggering(writing to 
     register 0x0f) seems to be unnecessary */

  loop = 10;
  while (loop > 0)
    {
      status = sanei_genesys_get_status (dev, &val);
      if (DBG_LEVEL >= DBG_io)
	{
	  print_status (val);
	}
      val40 = 0;
      status = sanei_genesys_read_register (dev, 0x40, &val40);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "%s: failed to read home sensor: %s\n", __FUNCTION__,
	       sane_strstatus (status));
	  DBG (DBG_proc, "%s: completed\n", __FUNCTION__);
	  return status;
	}

      /* if scanner is in command mode, we are done */
      if (!(val40 & REG40_DATAENB) && !(val40 & REG40_MOTMFLG)
	  && !(val & REG41_MOTORENB))
	{
	  DBG (DBG_proc, "%s: completed\n", __FUNCTION__);
	  return SANE_STATUS_GOOD;
	}

      usleep (100 * 1000);
      loop--;
    }

  DBG (DBG_proc, "%s: completed\n", __FUNCTION__);

  return SANE_STATUS_IO_ERROR;
}

static SANE_Status
gl843_get_paper_sensor (Genesys_Device * dev, SANE_Bool * paper_loaded)
{
  SANE_Status status;
  uint8_t val;

  status = sanei_genesys_read_register (dev, REG6D, &val);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl843_get_paper_sensor: failed to read gpio: %s\n",
	   sane_strstatus (status));
      return status;
    }
  *paper_loaded = (val & 0x1) == 0;
  return SANE_STATUS_GOOD;

  return SANE_STATUS_INVAL;
}

static SANE_Status
gl843_eject_document (Genesys_Device * dev)
{
  Genesys_Register_Set local_reg[GENESYS_GL843_MAX_REGS];
  SANE_Status status;
  uint8_t val;
  SANE_Bool paper_loaded;
  unsigned int init_steps;
  float feed_mm;
  int loop;

  DBG (DBG_proc, "gl843_eject_document\n");

  if (!dev->model->is_sheetfed == SANE_TRUE)
    {
      DBG (DBG_proc,
	   "gl843_eject_document: there is no \"eject sheet\"-concept for non sheet fed\n");
      DBG (DBG_proc, "gl843_eject_document: finished\n");
      return SANE_STATUS_GOOD;
    }


  memset (local_reg, 0, sizeof (local_reg));
  val = 0;

  status = sanei_genesys_get_status (dev, &val);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl843_eject_document: Failed to read status register: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status = gl843_stop_action (dev);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl843_eject_document: failed to stop motor: %s\n",
	   sane_strstatus (status));
      return SANE_STATUS_IO_ERROR;
    }

  memcpy (local_reg, dev->reg,
	  GENESYS_GL843_MAX_REGS * sizeof (Genesys_Register_Set));

  gl843_init_optical_regs_off (local_reg);

  gl843_init_motor_regs (dev, local_reg, 65536, MOTOR_ACTION_FEED, 0);

  status = gl843_bulk_write_register (dev, local_reg, GENESYS_GL843_MAX_REGS);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl843_eject_document: failed to bulk write registers: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status = gl843_start_action (dev);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl843_eject_document: failed to start motor: %s\n",
	   sane_strstatus (status));
      gl843_stop_action (dev);
      /* send original registers */
      gl843_bulk_write_register (dev, dev->reg, GENESYS_GL843_MAX_REGS);
      return status;
    }

  RIE (gl843_get_paper_sensor (dev, &paper_loaded));
  if (paper_loaded)
    {
      DBG (DBG_info, "gl843_eject_document: paper still loaded\n");
      /* force document TRUE, because it is definitely present */
      dev->document = SANE_TRUE;
      dev->scanhead_position_in_steps = 0;

      loop = 300;
      while (loop > 0)		/* do not wait longer then 30 seconds */
	{

	  RIE (gl843_get_paper_sensor (dev, &paper_loaded));

	  if (!paper_loaded)
	    {
	      DBG (DBG_info, "gl843_eject_document: reached home position\n");
	      DBG (DBG_proc, "gl843_eject_document: finished\n");
	      break;
	    }
	  usleep (100000);	/* sleep 100 ms */
	  --loop;
	}

      if (loop == 0)
	{
	  /* when we come here then the scanner needed too much time for this, so we better stop the motor */
	  gl843_stop_action (dev);
	  DBG (DBG_error,
	       "gl843_eject_document: timeout while waiting for scanhead to go home\n");
	  return SANE_STATUS_IO_ERROR;
	}
    }

  feed_mm = SANE_UNFIX (dev->model->eject_feed);
  if (dev->document)
    {
      feed_mm += SANE_UNFIX (dev->model->post_scan);
    }

  status = sanei_genesys_read_feed_steps (dev, &init_steps);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl843_eject_document: Failed to read feed steps: %s\n",
	   sane_strstatus (status));
      return status;
    }

  /* now feed for extra <number> steps */
  loop = 0;
  while (loop < 300)		/* do not wait longer then 30 seconds */
    {
      unsigned int steps;

      status = sanei_genesys_read_feed_steps (dev, &steps);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "gl843_eject_document: Failed to read feed steps: %s\n",
	       sane_strstatus (status));
	  return status;
	}

      DBG (DBG_info, "gl843_eject_document: init_steps: %d, steps: %d\n",
	   init_steps, steps);

      if (steps > init_steps + (feed_mm * dev->motor.base_ydpi) / MM_PER_INCH)
	{
	  break;
	}

      usleep (100000);		/* sleep 100 ms */
      ++loop;
    }

  status = gl843_stop_action (dev);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl843_eject_document: Failed to stop motor: %s\n",
	   sane_strstatus (status));
      return status;
    }

  dev->document = SANE_FALSE;

  DBG (DBG_proc, "gl843_eject_document: finished\n");
  return SANE_STATUS_GOOD;
}


static SANE_Status
gl843_load_document (Genesys_Device * dev)
{
  SANE_Status status;
  SANE_Bool paper_loaded;
  int loop = 300;
  DBG (DBG_proc, "gl843_load_document\n");
  while (loop > 0)		/* do not wait longer then 30 seconds */
    {

      RIE (gl843_get_paper_sensor (dev, &paper_loaded));

      if (paper_loaded)
	{
	  DBG (DBG_info, "gl843_load_document: document inserted\n");

	  /* when loading OK, document is here */
	  dev->document = SANE_TRUE;

	  usleep (1000000);	/* give user 1000ms to place document correctly */
	  break;
	}
      usleep (100000);		/* sleep 100 ms */
      --loop;
    }

  if (loop == 0)
    {
      /* when we come here then the user needed to much time for this */
      DBG (DBG_error,
	   "gl843_load_document: timeout while waiting for document\n");
      return SANE_STATUS_IO_ERROR;
    }

  DBG (DBG_proc, "gl843_load_document: finished\n");
  return SANE_STATUS_GOOD;
}

/**
 * detects end of document and adjust current scan
 * to take it into account
 * used by sheetfed scanners
 */
static SANE_Status
gl843_detect_document_end (Genesys_Device * dev)
{
  SANE_Status status = SANE_STATUS_GOOD;
  SANE_Bool paper_loaded;
  unsigned int scancnt = 0;
  int flines, channels, depth, bytes_remain, sublines,
    bytes_to_flush, lines, sub_bytes, tmp, read_bytes_left;
  DBG (DBG_proc, "%s: begin\n", __FUNCTION__);

  RIE (gl843_get_paper_sensor (dev, &paper_loaded));

  /* sheetfed scanner uses home sensor as paper present */
  if ((dev->document == SANE_TRUE) && !paper_loaded)
    {
      DBG (DBG_info, "%s: no more document\n", __FUNCTION__);
      dev->document = SANE_FALSE;

      channels = dev->current_setup.channels;
      depth = dev->current_setup.depth;
      read_bytes_left = (int) dev->read_bytes_left;
      DBG (DBG_io, "gl843_detect_document_end: read_bytes_left=%d\n",
	   read_bytes_left);

      /* get lines read */
      status = sanei_genesys_read_scancnt (dev, &scancnt);
      if (status != SANE_STATUS_GOOD)
	{
	  flines = 0;
	}
      else
	{
	  /* compute number of line read */
	  tmp = (int) dev->total_bytes_read;
	  if (depth == 1 || dev->settings.scan_mode == SCAN_MODE_LINEART)
	    flines = tmp * 8 / dev->settings.pixels / channels;
	  else
	    flines = tmp / (depth / 8) / dev->settings.pixels / channels;

	  /* number of scanned lines, but no read yet */
	  flines = scancnt - flines;

	  DBG (DBG_io,
	       "gl843_detect_document_end: %d scanned but not read lines\n",
	       flines);
	}

      /* adjust number of bytes to read 
       * we need to read the final bytes which are word per line * number of last lines
       * to have doc leaving feeder */
      lines =
	(SANE_UNFIX (dev->model->post_scan) * dev->current_setup.yres) /
	MM_PER_INCH + flines;
      DBG (DBG_io, "gl843_detect_document_end: adding %d line to flush\n",
	   lines);

      /* number of bytes to read from scanner to get document out of it after
       * end of document dectected by hardware sensor */
      bytes_to_flush = lines * dev->wpl;

      /* if we are already close to end of scan, flushing isn't needed */
      if (bytes_to_flush < read_bytes_left)
	{
	  /* we take all these step to work around an overflow on some plateforms */
	  tmp = (int) dev->total_bytes_read;
	  DBG (DBG_io, "gl843_detect_document_end: tmp=%d\n", tmp);
	  bytes_remain = (int) dev->total_bytes_to_read;
	  DBG (DBG_io, "gl843_detect_document_end: bytes_remain=%d\n",
	       bytes_remain);
	  bytes_remain = bytes_remain - tmp;
	  DBG (DBG_io, "gl843_detect_document_end: bytes_remain=%d\n",
	       bytes_remain);

	  /* remaining lines to read by frontend for the current scan */
	  if (depth == 1 || dev->settings.scan_mode == SCAN_MODE_LINEART)
	    {
	      flines = bytes_remain * 8 / dev->settings.pixels / channels;
	    }
	  else
	    flines = bytes_remain / (depth / 8)
	      / dev->settings.pixels / channels;
	  DBG (DBG_io, "gl843_detect_document_end: flines=%d\n", flines);

	  if (flines > lines)
	    {
	      /* change the value controlling communication with the frontend :
	       * total bytes to read is current value plus the number of remaining lines 
	       * multiplied by bytes per line */
	      sublines = flines - lines;

	      if (depth == 1 || dev->settings.scan_mode == SCAN_MODE_LINEART)
		sub_bytes =
		  ((dev->settings.pixels * sublines) / 8 +
		   (((dev->settings.pixels * sublines) % 8) ? 1 : 0)) *
		  channels;
	      else
		sub_bytes =
		  dev->settings.pixels * sublines * channels * (depth / 8);

	      dev->total_bytes_to_read -= sub_bytes;

	      /* then adjust the physical bytes to read */
	      if (read_bytes_left > sub_bytes)
		{
		  dev->read_bytes_left -= sub_bytes;
		}
	      else
		{
		  dev->total_bytes_to_read = dev->total_bytes_read;
		  dev->read_bytes_left = 0;
		}

	      DBG (DBG_io, "gl843_detect_document_end: sublines=%d\n",
		   sublines);
	      DBG (DBG_io, "gl843_detect_document_end: subbytes=%d\n",
		   sub_bytes);
	      DBG (DBG_io,
		   "gl843_detect_document_end: total_bytes_to_read=%lu\n",
		   (unsigned long)dev->total_bytes_to_read);
	      DBG (DBG_io,
		   "gl843_detect_document_end: read_bytes_left=%d\n",
		   read_bytes_left);
	    }
	}
      else
	{
	  DBG (DBG_io, "gl843_detect_document_end: no flushing needed\n");
	}
    }

  DBG (DBG_proc, "%s: finished\n", __FUNCTION__);
  return SANE_STATUS_GOOD;
}

/* Send the low-level scan command */
/* todo : is this that useful ? */
#ifndef UNIT_TESTING
static
#endif
  SANE_Status
gl843_begin_scan (Genesys_Device * dev, Genesys_Register_Set * reg,
		  SANE_Bool start_motor)
{
  SANE_Status status;
  uint8_t val;

  DBGSTART;
  if(reg==NULL)
    return SANE_STATUS_INVAL;

  /* clear GPIO 10 */
  RIE (sanei_genesys_read_register (dev, REG6C, &val));
  val &= ~REG6C_GPIO10;
  RIE (sanei_genesys_write_register (dev, REG6C, val));

  val = REG0D_CLRLNCNT;
  RIE (sanei_genesys_write_register (dev, REG0D, val));
  val = REG0D_CLRMCNT;
  RIE (sanei_genesys_write_register (dev, REG0D, val));

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
#ifndef UNIT_TESTING
static
#endif
  SANE_Status
gl843_end_scan (Genesys_Device * dev, Genesys_Register_Set * reg,
		SANE_Bool check_stop)
{
  SANE_Status status;

  DBG (DBG_proc, "gl843_end_scan (check_stop = %d)\n", check_stop);
  if(reg==NULL)
    return SANE_STATUS_INVAL;

  if (dev->model->is_sheetfed == SANE_TRUE)
    {
      status = SANE_STATUS_GOOD;
    }
  else				/* flat bed scanners */
    {
      status = gl843_stop_action (dev);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "gl843_end_scan: Failed to stop: %s\n",
	       sane_strstatus (status));
	  return status;
	}
    }

  DBG (DBG_proc, "gl843_end_scan: completed\n");

  return status;
}

/* Moves the slider to steps */
static SANE_Status
gl843_feed (Genesys_Device * dev, int steps)
{
  Genesys_Register_Set local_reg[GENESYS_GL843_MAX_REGS];
  SANE_Status status;
  uint8_t val;
  int loop;

  DBG (DBG_proc, "gl843_feed (steps = %d)\n", steps);

  status = gl843_stop_action (dev);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl843_feed: failed to stop action: %s\n",
	   sane_strstatus (status));
      return status;
    }

  memset (local_reg, 0, sizeof (local_reg));

  memcpy (local_reg, dev->reg,
	  GENESYS_GL843_MAX_REGS * sizeof (Genesys_Register_Set));

  gl843_init_optical_regs_off (local_reg);

  gl843_init_motor_regs (dev, local_reg, steps, MOTOR_ACTION_FEED, 0);

  status = gl843_bulk_write_register (dev, local_reg, GENESYS_GL843_MAX_REGS);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl843_feed: failed to bulk write registers: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status = gl843_start_action (dev);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl843_feed: failed to start motor: %s\n",
	   sane_strstatus (status));
      gl843_stop_action (dev);
      /* send original registers */
      gl843_bulk_write_register (dev, dev->reg, GENESYS_GL843_MAX_REGS);
      return status;
    }

  loop = 0;
  while (loop < 300)		/* do not wait longer then 30 seconds */
    {
      status = sanei_genesys_get_status (dev, &val);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "gl843_feed: failed to read home sensor: %s\n",
	       sane_strstatus (status));
	  return status;
	}

      if (!(val & REG41_MOTORENB))	/* motor enabled */
	{
	  DBG (DBG_proc, "gl843_feed: finished\n");
	  dev->scanhead_position_in_steps += steps;
	  return SANE_STATUS_GOOD;
	}
      usleep (100000);		/* sleep 100 ms */
      ++loop;
    }

  /* when we come here then the scanner needed too much time for this, so we better stop the motor */
  gl843_stop_action (dev);

  DBG (DBG_error,
       "gl843_feed: timeout while feeding\n");
  return SANE_STATUS_IO_ERROR;
}

/* Moves the slider to the home (top) postion slowly */
static SANE_Status
gl843_slow_back_home (Genesys_Device * dev, SANE_Bool wait_until_home)
{
  Genesys_Register_Set local_reg[GENESYS_GL843_MAX_REGS];
  SANE_Status status;
  uint8_t val;

  DBG (DBG_proc, "gl843_slow_back_home (wait_until_home = %d)\n",
       wait_until_home);

  if (dev->model->is_sheetfed == SANE_TRUE)
    {
      DBG (DBG_proc,
	   "gl843_slow_back_home: there is no \"home\"-concept for sheet fed\n");
      DBG (DBG_proc, "gl843_slow_back_home: finished\n");
      return SANE_STATUS_GOOD;
    }

  memset (local_reg, 0, sizeof (local_reg));

  /* reset gpio pin */
  RIE (sanei_genesys_read_register (dev, REG6C, &val));
  val = dev->gpo.value[0];
  RIE (sanei_genesys_write_register (dev, REG6C, val));

  /* first read gives HOME_SENSOR true */
  status = sanei_genesys_get_status (dev, &val);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl843_slow_back_home: failed to read home sensor: %s\n",
	   sane_strstatus (status));
      return status;
    }
  if (DBG_LEVEL >= DBG_io)
    {
      print_status (val);
    }
  usleep (100000);		/* sleep 100 ms */

  /* second is reliable */
  status = sanei_genesys_get_status (dev, &val);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl843_slow_back_home: failed to read home sensor: %s\n",
	   sane_strstatus (status));
      return status;
    }
  if (DBG_LEVEL >= DBG_io)
    {
      print_status (val);
    }

  dev->scanhead_position_in_steps = 0;

  if (val & REG41_HOMESNR)	/* is sensor at home? */
    {
      DBG (DBG_info, "gl843_slow_back_home: already at home, completed\n");
      dev->scanhead_position_in_steps = 0;
      return SANE_STATUS_GOOD;
    }

  /* if motor is on, stop current action */
  if (val & REG41_MOTORENB)
    {
      status = gl843_stop_action (dev);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "gl843_slow_back_home: failed to stop motor: %s\n",
	       sane_strstatus (status));
	  return SANE_STATUS_IO_ERROR;
	}
    }

  memcpy (local_reg, dev->reg,
	  GENESYS_GL843_MAX_REGS * sizeof (Genesys_Register_Set));

  gl843_init_optical_regs_off (local_reg);

  gl843_init_motor_regs (dev, local_reg, 65536, MOTOR_ACTION_GO_HOME, 0);

  status = gl843_bulk_write_register (dev, local_reg, GENESYS_GL843_MAX_REGS);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl843_slow_back_home: failed to bulk write registers: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status = gl843_start_action (dev);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl843_slow_back_home: failed to start motor: %s\n",
	   sane_strstatus (status));
      gl843_stop_action (dev);
      /* send original registers */
      gl843_bulk_write_register (dev, dev->reg, GENESYS_GL843_MAX_REGS);
      return status;
    }

  if (wait_until_home)
    {
      int loop = 0;

      while (loop < 300)	/* do not wait longer then 30 seconds */
	{
	  status = sanei_genesys_get_status (dev, &val);
	  if (status != SANE_STATUS_GOOD)
	    {
	      DBG (DBG_error,
		   "gl843_slow_back_home: failed to read home sensor: %s\n",
		   sane_strstatus (status));
	      return status;
	    }

	  if (val & REG41_HOMESNR)	/* home sensor */
	    {
	      DBG (DBG_info, "gl843_slow_back_home: reached home position\n");
	      DBG (DBG_proc, "gl843_slow_back_home: finished\n");
	      return SANE_STATUS_GOOD;
	    }
	  usleep (100000);	/* sleep 100 ms */
	  ++loop;
	}

      /* when we come here then the scanner needed too much time for this, so we better stop the motor */
      gl843_stop_action (dev);
      DBG (DBG_error,
	   "gl843_slow_back_home: timeout while waiting for scanhead to go home\n");
      return SANE_STATUS_IO_ERROR;
    }

  DBG (DBG_info, "gl843_slow_back_home: scanhead is still moving\n");
  DBG (DBG_proc, "gl843_slow_back_home: finished\n");
  return SANE_STATUS_GOOD;
}

/* Automatically set top-left edge of the scan area by scanning a 200x200 pixels
   area at 600 dpi from very top of scanner */
static SANE_Status
gl843_search_start_position (Genesys_Device * dev)
{
  int size;
  SANE_Status status;
  uint8_t *data;
  Genesys_Register_Set local_reg[GENESYS_GL843_MAX_REGS];
  int steps;

  int pixels = 600;
  int dpi = 300;

  DBG (DBG_proc, "gl843_search_start_position\n");

  memset (local_reg, 0, sizeof (local_reg));
  memcpy (local_reg, dev->reg,
	  GENESYS_GL843_MAX_REGS * sizeof (Genesys_Register_Set));

  /* sets for a 200 lines * 600 pixels */
  /* normal scan with no shading */

  status = gl843_init_scan_regs (dev, local_reg, dpi, dpi, 0, 0,	/*we should give a small offset here~60 steps */
				 600, dev->model->search_lines, 8, 1, 1,	/*green */
				 SCAN_FLAG_DISABLE_SHADING |
				 SCAN_FLAG_DISABLE_GAMMA |
				 SCAN_FLAG_IGNORE_LINE_DISTANCE |
				 SCAN_FLAG_DISABLE_BUFFER_FULL_MOVE);

  /* send to scanner */
  status = gl843_bulk_write_register (dev, local_reg, GENESYS_GL843_MAX_REGS);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl843_search_start_position: failed to bulk write registers: %s\n",
	   sane_strstatus (status));
      return status;
    }

  size = pixels * dev->model->search_lines;

  data = malloc (size);
  if (!data)
    {
      DBG (DBG_error,
	   "gl843_search_start_position: failed to allocate memory\n");
      return SANE_STATUS_NO_MEM;
    }

  status = gl843_begin_scan (dev, local_reg, SANE_TRUE);
  if (status != SANE_STATUS_GOOD)
    {
      free (data);
      DBG (DBG_error,
	   "gl843_search_start_position: failed to begin scan: %s\n",
	   sane_strstatus (status));
      return status;
    }

  /* waits for valid data */
  do
    sanei_genesys_test_buffer_empty (dev, &steps);
  while (steps);

  /* now we're on target, we can read data */
  status = sanei_genesys_read_data_from_scanner (dev, data, size);
  if (status != SANE_STATUS_GOOD)
    {
      free (data);
      DBG (DBG_error,
	   "gl843_search_start_position: failed to read data: %s\n",
	   sane_strstatus (status));
      return status;
    }

  if (DBG_LEVEL >= DBG_data)
    sanei_genesys_write_pnm_file ("search_position.pnm", data, 8, 1, pixels,
				  dev->model->search_lines);

  status = gl843_end_scan (dev, local_reg, SANE_TRUE);
  if (status != SANE_STATUS_GOOD)
    {
      free (data);
      DBG (DBG_error,
	   "gl843_search_start_position: failed to end scan: %s\n",
	   sane_strstatus (status));
      return status;
    }

  /* update regs to copy ASIC internal state */
  memcpy (dev->reg, local_reg,
	  GENESYS_GL843_MAX_REGS * sizeof (Genesys_Register_Set));

/*TODO: find out where sanei_genesys_search_reference_point 
  stores information, and use that correctly*/
  status =
    sanei_genesys_search_reference_point (dev, data, 0, dpi, pixels,
					  dev->model->search_lines);
  if (status != SANE_STATUS_GOOD)
    {
      free (data);
      DBG (DBG_error,
	   "gl843_search_start_position: failed to set search reference point: %s\n",
	   sane_strstatus (status));
      return status;
    }

  free (data);
  return SANE_STATUS_GOOD;
}

/* 
 * sets up register for coarse gain calibration
 * todo: check it for scanners using it */
static SANE_Status
gl843_init_regs_for_coarse_calibration (Genesys_Device * dev)
{
  SANE_Status status;
  uint8_t channels;
  uint8_t cksel;

  DBG (DBG_proc, "gl843_init_regs_for_coarse_calibration\n");


  cksel = (dev->calib_reg[reg_0x18].value & REG18_CKSEL) + 1;	/* clock speed = 1..4 clocks */

  /* set line size */
  if (dev->settings.scan_mode == SCAN_MODE_COLOR)	/* single pass color */
    channels = 3;
  else
    channels = 1;

  status = gl843_init_scan_regs (dev,
				 dev->calib_reg,
				 dev->settings.xres,
				 dev->settings.yres,
				 0,
				 0,
				 dev->sensor.optical_res / cksel,
				 20,
				 16,
				 channels,
				 dev->settings.color_filter,
				 SCAN_FLAG_DISABLE_SHADING |
				 SCAN_FLAG_DISABLE_GAMMA |
				 SCAN_FLAG_SINGLE_LINE |
				 SCAN_FLAG_IGNORE_LINE_DISTANCE);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl843_init_register_for_coarse_calibration: Failed to setup scan: %s\n",
	   sane_strstatus (status));
      return status;
    }

  DBG (DBG_info,
       "gl843_init_register_for_coarse_calibration: optical sensor res: %d dpi, actual res: %d\n",
       dev->sensor.optical_res / cksel, dev->settings.xres);

  status =
    gl843_bulk_write_register (dev, dev->calib_reg, GENESYS_GL843_MAX_REGS);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl843_init_register_for_coarse_calibration: Failed to bulk write registers: %s\n",
	   sane_strstatus (status));
      return status;
    }

  DBG (DBG_proc, "gl843_init_register_for_coarse_calibration: completed\n");

/*  if (DBG_LEVEL >= DBG_info)
    sanei_gl843_print_registers (dev->calib_reg);*/


  return SANE_STATUS_GOOD;
}


/* init registers for shading calibration */
static SANE_Status
gl843_init_regs_for_shading (Genesys_Device * dev)
{
  SANE_Status status;

  DBG (DBG_proc, "gl843_init_regs_for_shading: lines = %d\n",
       dev->model->shading_lines);

  dev->calib_channels = 3;

  /* initial calibration reg values */
  memcpy (dev->calib_reg, dev->reg,
	  GENESYS_GL843_MAX_REGS * sizeof (Genesys_Register_Set));

  dev->calib_pixels = dev->sensor.sensor_pixels;

  status = gl843_init_scan_regs (dev,
				 dev->calib_reg,
				 dev->sensor.optical_res,
				 dev->motor.base_ydpi,
				 0,
				 0,
				 dev->calib_pixels,
				 dev->model->shading_lines,
                                 16,
				 dev->calib_channels,
				 dev->settings.color_filter,
				 SCAN_FLAG_DISABLE_SHADING |
				 SCAN_FLAG_DISABLE_GAMMA |
  				 SCAN_FLAG_DISABLE_BUFFER_FULL_MOVE |
				 SCAN_FLAG_IGNORE_LINE_DISTANCE |
				 SCAN_FLAG_USE_OPTICAL_RES);


  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl843_init_registers_for_shading: Failed to setup scan: %s\n",
	   sane_strstatus (status));
      return status;
    }
  
  dev->scanhead_position_in_steps += dev->model->shading_lines;

  status =
    gl843_bulk_write_register (dev, dev->calib_reg, GENESYS_GL843_MAX_REGS);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl843_init_registers_for_shading: Failed to bulk write registers: %s\n",
	   sane_strstatus (status));
      return status;
    }

  DBG (DBG_proc, "gl843_init_regs_for_shading: completed\n");

  return SANE_STATUS_GOOD;
}

/** @brief set up registers for the actual scan
 */
static SANE_Status
gl843_init_regs_for_scan (Genesys_Device * dev)
{
  int channels;
  int flags;
  int depth;
  float move;
  int move_dpi;
  float start;
  uint8_t val;

  SANE_Status status;

  DBG (DBG_info,
       "gl843_init_regs_for_scan settings:\nResolution: %uDPI\n"
       "Lines     : %u\nPPL       : %u\nStartpos  : %.3f/%.3f\nScan mode : %d\n\n",
       dev->settings.yres, dev->settings.lines, dev->settings.pixels,
       dev->settings.tl_x, dev->settings.tl_y, dev->settings.scan_mode);

  gl843_slow_back_home (dev, 1);

 /* channels */
  if (dev->settings.scan_mode == SCAN_MODE_COLOR)	/* single pass color */
    channels = 3;
  else
    channels = 1;

  /* depth */
  depth = dev->settings.depth;
  if (dev->settings.scan_mode == SCAN_MODE_LINEART)
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
  DBG (DBG_info, "gl843_init_regs_for_scan: move=%f steps\n", move);

  /* at high res we do fast move to scan area */
  if(dev->settings.xres>150)
    {
      status = gl843_feed (dev, move);
      if (status != SANE_STATUS_GOOD)
        {
          DBG (DBG_error, "%s: failed to move to scan area\n",__FUNCTION__);
          return status;
        }
      move=0;
    }
 
  /* clear scancnt and fedcnt */
  val = REG0D_CLRLNCNT;
  RIE (sanei_genesys_write_register (dev, REG0D, val));
  val = REG0D_CLRMCNT;
  RIE (sanei_genesys_write_register (dev, REG0D, val));

  /* start */
  start = SANE_UNFIX (dev->model->x_offset);
  start += dev->settings.tl_x;
  start = (start * dev->sensor.optical_res) / MM_PER_INCH;

  flags = 0;

  /* emulated lineart from gray data is required for now */
  flags |= SCAN_FLAG_DYNAMIC_LINEART;

  status = gl843_init_scan_regs (dev,
				 dev->reg,
				 dev->settings.xres,
				 dev->settings.yres,
				 start,
				 move,
				 dev->settings.pixels,
				 dev->settings.lines,
				 depth,
				 channels,
                                 dev->settings.color_filter,
                                 flags);

  if (status != SANE_STATUS_GOOD)
    return status;

  DBG (DBG_proc, "gl843_init_register_for_scan: completed\n");
  return SANE_STATUS_GOOD;
}

/* Write bulk data (e.g. gamma) */
static SANE_Status
gl843_bulk_write_data_gamma (Genesys_Device * dev, uint8_t addr,
			 uint8_t * data, size_t len)
{
  SANE_Status status;
  size_t size;
  uint8_t outdata[8];

  DBG (DBG_io, "gl843_bulk_write_data_gamma writing %lu bytes\n",
       (u_long) len);

  status =
    sanei_usb_control_msg (dev->dn, REQUEST_TYPE_OUT, REQUEST_REGISTER,
			   VALUE_SET_REGISTER, INDEX, 1, &addr);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl843_bulk_write_data_gamma failed while setting register: %s\n",
	   sane_strstatus (status));
      return status;
    }

  while (len)
    {
      if (len > BULKOUT_MAXSIZE)
	size = BULKOUT_MAXSIZE;
      else
	size = len;

      outdata[0] = BULK_OUT;
      outdata[1] = BULK_RAM;
      outdata[2] = 0x00;/* 0x82 works, too */
      outdata[3] = 0x00;
      outdata[4] = (size & 0xff);
      outdata[5] = ((size >> 8) & 0xff);
      outdata[6] = ((size >> 16) & 0xff);
      outdata[7] = ((size >> 24) & 0xff);

      status =
	sanei_usb_control_msg (dev->dn, REQUEST_TYPE_OUT, REQUEST_BUFFER,
			       VALUE_BUFFER, INDEX, sizeof (outdata),
			       outdata);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "gl843_bulk_write_data_gamma failed while writing command: %s\n",
	       sane_strstatus (status));
	  return status;
	}

      status = sanei_usb_write_bulk (dev->dn, data, &size);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "gl843_bulk_write_data_gamma failed while writing bulk data: %s\n",
	       sane_strstatus (status));
	  return status;
	}

      DBG (DBG_io2,
	   "gl843_bulk_write_data:gamma wrote %lu bytes, %lu remaining\n",
	   (u_long) size, (u_long) (len - size));

      len -= size;
      data += size;
    }

  DBG (DBG_io, "gl843_bulk_write_data_gamma: completed\n");

  return status;
}

/*
 * this function sends generic gamma table (ie linear ones)
 * or the Sensor specific one if provided
 */
static SANE_Status
gl843_send_gamma_table (Genesys_Device * dev, SANE_Bool generic)
{
  int size;
  int status;
  uint8_t *gamma;
  int i,gmmval;

  DBG (DBG_proc, "gl843_send_gamma_table\n");

  /* don't send anything if no specific gamma table defined */
  if (!generic
      && (dev->sensor.red_gamma_table == NULL
	  || dev->sensor.green_gamma_table == NULL
	  || dev->sensor.blue_gamma_table == NULL))
    {
      DBG (DBG_proc,
	   "gl843_send_gamma_table: nothing to send, skipping\n");
      return SANE_STATUS_GOOD;
    }

  size = 256;

  /* allocate temporary gamma tables: 16 bits words, 3 channels */
  gamma = (uint8_t *) malloc (size * 2 * 3);
  if (!gamma)
    return SANE_STATUS_NO_MEM;

  /* take care off generic/specific data */
  if (generic)
    {
      /* fill with default values */
      for (i = 0; i < size; i++)
	{
	  gmmval = i* 256;
	  gamma[i*2 + size * 0 + 0] = gmmval & 0xff;
	  gamma[i*2 + size * 0 + 1] = (gmmval >> 8) & 0xff;
	  gamma[i*2 + size * 2 + 0] = gmmval & 0xff;
	  gamma[i*2 + size * 2 + 1] = (gmmval >> 8) & 0xff;
	  gamma[i*2 + size * 4 + 0] = gmmval & 0xff;
	  gamma[i*2 + size * 4 + 1] = (gmmval >> 8) & 0xff;
	}
    }
  else
    {
      /* copy sensor specific's gamma tables */
      for (i = 0; i < size; i++)
	{
	  gamma[i*2 + size * 0 + 0] = 
	      dev->sensor.red_gamma_table[i] & 0xff;
	  gamma[i*2 + size * 0 + 1] = 
	      (dev->sensor.red_gamma_table[i] >> 8) & 0xff;
	  gamma[i*2 + size * 2 + 0] = 
	      dev->sensor.green_gamma_table[i] & 0xff;
	  gamma[i*2 + size * 2 + 1] = 
	      (dev->sensor.green_gamma_table[i] >> 8) & 0xff;
	  gamma[i*2 + size * 4 + 0] = 
	      dev->sensor.blue_gamma_table[i] & 0xff;
	  gamma[i*2 + size * 4 + 1] = 
	      (dev->sensor.blue_gamma_table[i] >> 8) & 0xff;
	}
    }

  /* send address */
  status = gl843_set_buffer_address (dev, 0x0000);
  if (status != SANE_STATUS_GOOD)
    {
      free (gamma);
      DBG (DBG_error,
	   "gl843_send_gamma_table: failed to set buffer address: %s\n",
	   sane_strstatus (status));
      return status;
    }

  /* send data */
  status = gl843_bulk_write_data_gamma (dev, 0x28, (uint8_t *) gamma, size * 2 * 3);
  if (status != SANE_STATUS_GOOD)
    {
      free (gamma);
      DBG (DBG_error,
	   "gl843_send_gamma_table: failed to send gamma table: %s\n",
	   sane_strstatus (status));
      return status;
    }

  DBG (DBG_proc, "gl843_send_gamma_table: completed\n");
  free (gamma);
  return SANE_STATUS_GOOD;
}

/**
 * Send shading calibration data. The buffer is considered to always hold values
 * for all the channels.
 */
static SANE_Status
gl843_send_shading_data (Genesys_Device * dev, uint8_t * data, int size)
{
  SANE_Status status = SANE_STATUS_GOOD;
  uint32_t addr, length;
  uint8_t val;

  DBGSTART;
  DBG( DBG_io2, "%s: writing %d bytes of shading data\n",__FUNCTION__,size);

  length = (uint32_t) (size / 3);
  DBG( DBG_io2, "%s: using chunks of %d (0x%04x) bytes\n",__FUNCTION__,length,length);

  /* base addr of data has been written in reg D0-D4 in 4K word, so AHB address
   * is 8192*reg value */

  /* write actual red data */
  RIE (sanei_genesys_read_register (dev, 0xd0, &val));
  addr = val * 8192 + 0x10000000;
  status = write_data (dev, addr, length, data);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "gl843_send_shading_data; write to AHB failed (%s)\n",
	   sane_strstatus (status));
      return status;
    }

  /* write actual green data */
  RIE (sanei_genesys_read_register (dev, 0xd1, &val));
  addr = val * 8192 + 0x10000000;
  status = write_data (dev, addr, length, data + length);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "gl843_send_shading_data; write to AHB failed (%s)\n",
	   sane_strstatus (status));
      return status;
    }

  /* write actual blue data */
  RIE (sanei_genesys_read_register (dev, 0xd2, &val));
  addr = val * 8192 + 0x10000000;
  status = write_data (dev, addr, length, data + 2 * length);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "gl843_send_shading_data; write to AHB failed (%s)\n",
	   sane_strstatus (status));
      return status;
    }

  DBGCOMPLETED;

  return status;
}

/* this function does the led calibration by scanning one line of the calibration
   area below scanner's top on white strip.

-needs working coarse/gain
*/
static SANE_Status
gl843_led_calibration (Genesys_Device * dev)
{
  int num_pixels;
  int total_size;
  int used_res;
  uint8_t *line;
  int i, j;
  SANE_Status status = SANE_STATUS_GOOD;
  int val;
  int channels, depth;
  int avg[3], avga, avge;
  int turn;
  char fn[20];
  uint16_t expr, expg, expb;
  Genesys_Register_Set *r;

  SANE_Bool acceptable = SANE_FALSE;

  DBG (DBG_proc, "gl843_led_calibration\n");

  /* offset calibration is always done in color mode */
  channels = 3;
  depth=16;
  used_res = dev->sensor.optical_res;
  num_pixels = (dev->sensor.sensor_pixels*used_res)/dev->sensor.optical_res;
  
  /* initial calibration reg values */
  memcpy (dev->calib_reg, dev->reg,
	  GENESYS_GL843_MAX_REGS * sizeof (Genesys_Register_Set));

  status = gl843_init_scan_regs (dev,
				 dev->calib_reg,
				 used_res,
				 dev->motor.base_ydpi,
				 0,
				 0,
				 num_pixels,
                                 1,
                                 depth,
                                 channels,
				 dev->settings.color_filter,
				 SCAN_FLAG_DISABLE_SHADING |
				 SCAN_FLAG_DISABLE_GAMMA |
				 SCAN_FLAG_SINGLE_LINE |
				 SCAN_FLAG_IGNORE_LINE_DISTANCE |
				 SCAN_FLAG_USE_OPTICAL_RES);

  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl843_led_calibration: failed to setup scan: %s\n",
	   sane_strstatus (status));
      return status;
    }

  RIE (gl843_bulk_write_register
       (dev, dev->calib_reg, GENESYS_GL843_MAX_REGS));


  total_size = num_pixels * channels * (depth/8) * 1;	/* colors * bytes_per_color * scan lines */

  line = malloc (total_size);
  if (!line)
    return SANE_STATUS_NO_MEM;

/* 
   we try to get equal bright leds here:

   loop:
     average per color
     adjust exposure times
 */

  expr = (dev->sensor.regs_0x10_0x1d[0] << 8) | dev->sensor.regs_0x10_0x1d[1];
  expg = (dev->sensor.regs_0x10_0x1d[2] << 8) | dev->sensor.regs_0x10_0x1d[3];
  expb = (dev->sensor.regs_0x10_0x1d[4] << 8) | dev->sensor.regs_0x10_0x1d[5];

  turn = 0;

  do
    {

      dev->sensor.regs_0x10_0x1d[0] = (expr >> 8) & 0xff;
      dev->sensor.regs_0x10_0x1d[1] = expr & 0xff;
      dev->sensor.regs_0x10_0x1d[2] = (expg >> 8) & 0xff;
      dev->sensor.regs_0x10_0x1d[3] = expg & 0xff;
      dev->sensor.regs_0x10_0x1d[4] = (expb >> 8) & 0xff;
      dev->sensor.regs_0x10_0x1d[5] = expb & 0xff;

      for (i = 0; i < 6; i++)
	{
          r = sanei_genesys_get_address (dev->calib_reg, 0x10+i);
	  r->value = dev->sensor.regs_0x10_0x1d[i];
	}

      RIE (gl843_bulk_write_register
	   (dev, dev->calib_reg, GENESYS_GL843_MAX_REGS));

      DBG (DBG_info, "gl843_led_calibration: starting first line reading\n");
      RIE (gl843_begin_scan (dev, dev->calib_reg, SANE_TRUE));
      RIE (sanei_genesys_read_data_from_scanner (dev, line, total_size));

      if (DBG_LEVEL >= DBG_data)
	{
	  snprintf (fn, 20, "led_%02d.pnm", turn);
	  sanei_genesys_write_pnm_file (fn,
					line, depth, channels, num_pixels, 1);
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

      DBG (DBG_info, "gl843_led_calibration: average: "
	   "%d,%d,%d\n", avg[0], avg[1], avg[2]);

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

      RIE (gl843_stop_action (dev));

      turn++;

    }
  while (!acceptable && turn < 100);

  DBG (DBG_info, "gl843_led_calibration: acceptable exposure: %d,%d,%d\n",
       expr, expg, expb);

  /* cleanup before return */
  free (line);
 
  gl843_slow_back_home (dev, SANE_TRUE);

  DBG (DBG_proc, "gl843_led_calibration: completed\n");
  return status;
}

/* this function does the offset calibration by scanning one line of the calibration
   area below scanner's top. There is a black margin and the remaining is white.
   sanei_genesys_search_start() must have been called so that the offsets and margins
   are allready known.

this function expects the slider to be where?
*/
static SANE_Status
gl843_offset_calibration (Genesys_Device * dev)
{
  DBG (DBG_proc, "%s: not implemented \n", __FUNCTION__);
  if(dev==NULL)
    return SANE_STATUS_INVAL;
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
gl843_coarse_gain_calibration (Genesys_Device * dev, int dpi)
{
  DBG (DBG_proc, "gl843_coarse_gain_calibration: dpi = %d\n", dpi);
  DBG (DBG_proc, "%s: not implemented \n", __FUNCTION__);
  if(dev==NULL)
    return SANE_STATUS_INVAL;
  DBGCOMPLETED;
  return SANE_STATUS_GOOD;
}

/*
 * wait for lamp warmup by scanning the same line until difference
 * between 2 scans is below a threshold
 */
static SANE_Status
gl843_init_regs_for_warmup (Genesys_Device * dev,
			    Genesys_Register_Set * reg,
			    int *channels,
                            int *total_size)
{
  DBG (DBG_proc, "%s: not implemented \n", __FUNCTION__);
  if(dev==NULL || reg==NULL || channels==NULL || total_size == NULL)
    return SANE_STATUS_INVAL;

  DBGCOMPLETED;
  return SANE_STATUS_GOOD;
}

static SANE_Status
gl843_is_compatible_calibration (Genesys_Device * dev,
				 Genesys_Calibration_Cache * cache,
				 int for_overwrite)
{
  SANE_Status status;

  DBG (DBG_proc, "gl843_is_compatible_calibration\n");

  status = gl843_calculate_current_setup (dev);

  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl843_is_compatible_calibration: failed to calculate current setup: %s\n",
	   sane_strstatus (status));
      return status;
    }

  DBG (DBG_proc, "gl843_is_compatible_calibration: checking\n");

  if (dev->current_setup.half_ccd != cache->used_setup.half_ccd)
    return SANE_STATUS_UNSUPPORTED;

  if(for_overwrite)
    return SANE_STATUS_UNSUPPORTED;

  DBGCOMPLETED;
  return SANE_STATUS_GOOD;
}

/** 
 * set up GPIO/GPOE for idle state
 */
static SANE_Status
gl843_init_gpio (Genesys_Device * dev)
{
  SANE_Status status = SANE_STATUS_GOOD;
  uint8_t val, effective;

  DBG (DBG_proc, "gl843_init_gpio: start\n");

  RIE (sanei_genesys_write_register (dev, 0x6e, dev->gpo.enable[0]));
  RIE (sanei_genesys_write_register (dev, 0x6f, dev->gpo.enable[1]));
  RIE (sanei_genesys_write_register (dev, 0xa7, 0x04));
  RIE (sanei_genesys_write_register (dev, 0xa8, 0x00));
  RIE (sanei_genesys_write_register (dev, 0xa9, 0x00));

  /* toggle needed bits one after all */
  /* TODO define a function for bit toggling */
  RIE (sanei_genesys_read_register (dev, REG6C, &effective));
  val = effective | 0x80;
  RIE (sanei_genesys_write_register (dev, REG6C, val));
  RIE (sanei_genesys_read_register (dev, REG6C, &effective));
  if (effective != val)
    {
      DBG (DBG_warn,
	   "gl843_init_gpio: effective!=needed (0x%02x!=0x%02x) \n",
	   effective, val);
    }

  val = effective | 0x40;
  RIE (sanei_genesys_write_register (dev, REG6C, val));
  RIE (sanei_genesys_read_register (dev, REG6C, &effective));
  if (effective != val)
    {
      DBG (DBG_warn,
	   "gl843_init_gpio: effective!=needed (0x%02x!=0x%02x) \n",
	   effective, val);
    }

  val = effective | 0x20;
  RIE (sanei_genesys_write_register (dev, REG6C, val));

  /* seems useless : memory or clock related ? */
  RIE (sanei_genesys_read_register (dev, REG0B, &effective));
  RIE (sanei_genesys_write_register (dev, REG0B, effective));

  RIE (sanei_genesys_read_register (dev, REG6C, &effective));
  if (effective != val)
    {
      DBG (DBG_warn,
	   "gl843_init_gpio: effective!=needed (0x%02x!=0x%02x) \n",
	   effective, val);
    }

  val = effective | 0x02;
  RIE (sanei_genesys_write_register (dev, REG6C, val));
  RIE (sanei_genesys_read_register (dev, REG6C, &effective));
  if (effective != val)
    {
      DBG (DBG_warn,
	   "gl843_init_gpio: effective!=needed (0x%02x!=0x%02x) \n",
	   effective, val);
    }

  val = effective | 0x01;
  RIE (sanei_genesys_write_register (dev, REG6C, val));
  RIE (sanei_genesys_read_register (dev, REG6C, &effective));
  if (effective != val)
    {
      DBG (DBG_warn,
	   "gl843_init_gpio: effective!=needed (0x%02x!=0x%02x) \n",
	   effective, val);
    }

  DBG (DBG_proc, "gl843_init_gpio: completed\n");
  return status;
}


/* *
 * initialize ASIC from power on condition
 */
static SANE_Status
gl843_cold_boot (Genesys_Device * dev)
{
  SANE_Status status;
  uint8_t val;

  DBGSTART;

  /* XXX STEF XXX 
  RIE (sanei_genesys_write_register (dev, 0x0e, 0x01));
  RIE (sanei_genesys_write_register (dev, 0x0e, 0x00)); */

  /* test CHKVER */
  RIE (sanei_genesys_read_register (dev, REG40, &val));
  if (val & REG40_CHKVER)
    {
      RIE (sanei_genesys_read_register (dev, 0x00, &val));
      DBG (DBG_info, "gl843_cold_boot: reported version for genesys chip is 0x%02x\n", val);
    }

  /* setup GPIO */
  sanei_genesys_read_register (dev, REGA6, &val);
  sanei_genesys_write_register (dev, REGA6, val | 0x04);
  sanei_genesys_write_register (dev, REGA7, 0x0f);
  sanei_genesys_write_register (dev, REGA9, 0x00);

  /* Set default values for registers */
  gl843_init_registers (dev);

  RIE (sanei_genesys_write_register (dev, REG6B, 0x02));
  RIE (sanei_genesys_write_register (dev, REG6C, 0x00));
  RIE (sanei_genesys_write_register (dev, REG6D, 0x20));
  RIE (sanei_genesys_write_register (dev, REG6E, 0x7e));
  RIE (sanei_genesys_write_register (dev, REG6F, 0x21));

  /* Write initial registers */
  RIE (gl843_bulk_write_register (dev, dev->reg, GENESYS_GL843_MAX_REGS));

  /* Enable DRAM by setting a rising edge on bit 3 of reg 0x0b */
  val = dev->reg[reg_0x0b].value & REG0B_DRAMSEL;
  val = (val | REG0B_ENBDRAM);
  RIE (sanei_genesys_write_register (dev, REG0B, val));
  dev->reg[reg_0x0b].value = val;
  /* URB    14  control  0x40 0x0c 0x8c 0x10 len     1 wrote 0xb4 */
  RIE (write_end_access (dev, 0x10, 0xb4));

  /* set up clock once for all */
  RIE (sanei_genesys_write_register (dev, 0x77, 0x00));
  RIE (sanei_genesys_write_register (dev, 0x78, 0x00));
  RIE (sanei_genesys_write_register (dev, 0x79, 0x9f));

  /* CLKSET */
  val = (dev->reg[reg_0x0b].value & ~REG0B_CLKSET) | REG0B_30MHZ;
  RIE (sanei_genesys_write_register (dev, REG0B, val));
  dev->reg[reg_0x0b].value = val;

  /* prevent further writings by bulk write register */
  dev->reg[reg_0x0b].address = 0x00;

  /* set up end access */
  sanei_genesys_write_register (dev, REGA7, 0x04);
  sanei_genesys_write_register (dev, REGA9, 0x00);

  /* setup gpio */
  RIE (gl843_init_gpio (dev));

  DBGCOMPLETED;
  return SANE_STATUS_GOOD;
}

/* *
 * initialize backend and ASIC : registers, motor tables, and gamma tables
 * then ensure scanner's head is at home
 */
static SANE_Status
gl843_init (Genesys_Device * dev)
{
  SANE_Status status;
  uint8_t val;
  SANE_Bool cold = SANE_TRUE;
  int size;

  DBG_INIT ();
  DBGSTART;
 
  /* URB    16  control  0xc0 0x0c 0x8e 0x0b len     1 read  0x00 */
  status = sanei_usb_control_msg (dev->dn, REQUEST_TYPE_IN, REQUEST_REGISTER, VALUE_GET_REGISTER, 0x0b, 1, &val);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl843_init: request register failed %s\n", sane_strstatus (status));
      return status;
    }
  DBG( DBG_io2, "gl843_init: value=0x%02x\n",val);
  DBG (DBG_info, "%s: device is %s\n", __FUNCTION__, (val & 0x08) ? "USB 1.0" : "USB2.0");
  if(val & 0x08)
    {
      /* URB    17  control  0x40 0x0c 0x8c 0x0f len     1 wrote 0x14  */
      dev->usb_mode=1;
      val=0x14;
    }
  else
    {
      /* URB    17  control  0x40 0x0c 0x8c 0x0f len     1 wrote 0x11  */
      dev->usb_mode=2;
      val=0x11;
    }
  RIE (write_end_access (dev, 0x0f, val));

  /* check if the device has already been initialized and powered up 
   * we read register 6 and check PWRBIT, if reset scanner has been
   * freshly powered up. This bit will be set to later so that following
   * reads can detect power down/up cycle*/
  RIE (sanei_genesys_read_register (dev, 0x06, &val));
  if (val & REG06_PWRBIT)
    {
      cold = SANE_FALSE;
    }
  DBG (DBG_info, "%s: device is %s\n", __FUNCTION__, cold ? "cold" : "warm");

  /* don't do anything if backend is initialized and hardware hasn't been
   * replug */
  if (dev->already_initialized && !cold)
    {
      DBG (DBG_info, "gl843_init: already initialized, nothing to do\n");
      return SANE_STATUS_GOOD;
    }

  /* set up hardware and registers */
  RIE (gl843_cold_boot (dev));

  /* now hardware part is OK, set up device struct */
  FREE_IFNOT_NULL (dev->white_average_data);
  FREE_IFNOT_NULL (dev->dark_average_data);
  FREE_IFNOT_NULL (dev->sensor.red_gamma_table);
  FREE_IFNOT_NULL (dev->sensor.green_gamma_table);
  FREE_IFNOT_NULL (dev->sensor.blue_gamma_table);

  dev->settings.color_filter = 0;

  memcpy (dev->calib_reg, dev->reg,
	  GENESYS_GL843_MAX_REGS * sizeof (Genesys_Register_Set));

  /* Set analog frontend */
  RIE (gl843_set_fe (dev, AFE_INIT));

  /* init gamma tables */
  size = 256;
  if (dev->sensor.red_gamma_table == NULL)
    {
      dev->sensor.red_gamma_table = (uint16_t *) malloc (2 * size);
      if (dev->sensor.red_gamma_table == NULL)
	{
	  DBG (DBG_error,
	       "gl843_init: could not allocate memory for gamma table\n");
	  return SANE_STATUS_NO_MEM;
	}
      sanei_genesys_create_gamma_table (dev->sensor.red_gamma_table, size,
					65535, 65535, dev->sensor.red_gamma);
    }
  if (dev->sensor.green_gamma_table == NULL)
    {
      dev->sensor.green_gamma_table = (uint16_t *) malloc (2 * size);
      if (dev->sensor.red_gamma_table == NULL)
	{
	  DBG (DBG_error,
	       "gl843_init: could not allocate memory for gamma table\n");
	  return SANE_STATUS_NO_MEM;
	}
      sanei_genesys_create_gamma_table (dev->sensor.green_gamma_table, size,
					65535, 65535,
					dev->sensor.green_gamma);
    }
  if (dev->sensor.blue_gamma_table == NULL)
    {
      dev->sensor.blue_gamma_table = (uint16_t *) malloc (2 * size);
      if (dev->sensor.red_gamma_table == NULL)
	{
	  DBG (DBG_error,
	       "gl843_init: could not allocate memory for gamma table\n");
	  return SANE_STATUS_NO_MEM;
	}
      sanei_genesys_create_gamma_table (dev->sensor.blue_gamma_table, size,
					65535, 65535, dev->sensor.blue_gamma);
    }
  
  dev->oe_buffer.buffer=NULL;
  dev->already_initialized = SANE_TRUE;

  /* Move home if needed */
  RIE (gl843_slow_back_home (dev, SANE_TRUE));
  dev->scanhead_position_in_steps = 0;

  /* Set powersaving (default = 15 minutes) */
  RIE (gl843_set_powersaving (dev, 15));

  DBGCOMPLETED;
  return status;
}

static SANE_Status
gl843_update_hardware_sensors (Genesys_Scanner * s)
{
  /* do what is needed to get a new set of events, but try to not lose
     any of them.
   */
  SANE_Status status = SANE_STATUS_GOOD;
  uint8_t val;

  RIE (sanei_genesys_read_register (s->dev, REG6D, &val));

  if (s->val[OPT_SCAN_SW].b == s->last_val[OPT_SCAN_SW].b)
    s->val[OPT_SCAN_SW].b = (val & 0x01) == 0;
  if (s->val[OPT_FILE_SW].b == s->last_val[OPT_FILE_SW].b)
    s->val[OPT_FILE_SW].b = (val & 0x02) == 0;
  if (s->val[OPT_EMAIL_SW].b == s->last_val[OPT_EMAIL_SW].b)
    s->val[OPT_EMAIL_SW].b = (val & 0x04) == 0;
  if (s->val[OPT_COPY_SW].b == s->last_val[OPT_COPY_SW].b)
    s->val[OPT_COPY_SW].b = (val & 0x08) == 0;

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
gl843_search_strip (Genesys_Device * dev, SANE_Bool forward, SANE_Bool black)
{
  unsigned int pixels, lines, channels;
  SANE_Status status;
  Genesys_Register_Set local_reg[GENESYS_GL843_MAX_REGS];
  size_t size;
  uint8_t *data;
  int steps, depth, dpi;
  unsigned int pass, count, found, x, y;
  char title[80];
  Genesys_Register_Set *r;

  DBG (DBG_proc, "gl843_search_strip %s %s\n", black ? "black" : "white",
       forward ? "forward" : "reverse");

  gl843_set_fe (dev, AFE_SET);
  status = gl843_stop_action (dev);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl843_search_strip: failed to stop: %s\n",
	   sane_strstatus (status));
      return status;
    }

  /* set up for a gray scan at lowest dpi */
  dpi = 9600;
  for (x = 0; x < MAX_RESOLUTIONS; x++)
    {
      if (dev->model->xdpi_values[x] > 0 && dev->model->xdpi_values[x] < dpi)
	dpi = dev->model->xdpi_values[x];
    }
  channels = 1;
  /* 10 MM */
  lines = (10 * dpi) / MM_PER_INCH;
  /* shading calibation is done with dev->motor.base_ydpi */
  lines = (dev->model->shading_lines * dpi) / dev->motor.base_ydpi;
  depth = 8;
  pixels = (dev->sensor.sensor_pixels * dpi) / dev->sensor.optical_res;
  size = pixels * channels * lines * (depth / 8);
  data = malloc (size);
  if (!data)
    {
      DBG (DBG_error, "gl843_search_strip: failed to allocate memory\n");
      return SANE_STATUS_NO_MEM;
    }
  dev->scanhead_position_in_steps = 0;

  memcpy (local_reg, dev->reg,
	  GENESYS_GL843_MAX_REGS * sizeof (Genesys_Register_Set));

  status = gl843_init_scan_regs (dev,
				 local_reg,
				 dpi,
				 dpi,
				 0,
				 0,
				 pixels,
				 lines,
				 depth,
				 channels,
				 0,
				 SCAN_FLAG_DISABLE_SHADING |
				 SCAN_FLAG_DISABLE_GAMMA);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl843_search_strip: failed to setup for scan: %s\n",
	   sane_strstatus (status));
      return status;
    }

  /* set up for reverse or forward */
  r = sanei_genesys_get_address (local_reg, REG02);
  if (forward)
    r->value &= ~REG02_MTRREV;
  else
    r->value |= REG02_MTRREV;


  status = gl843_bulk_write_register (dev, local_reg, GENESYS_GL843_MAX_REGS);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl843_search_strip: Failed to bulk write registers: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status = gl843_begin_scan (dev, local_reg, SANE_TRUE);
  if (status != SANE_STATUS_GOOD)
    {
      free (data);
      DBG (DBG_error,
	   "gl843_search_strip: failed to begin scan: %s\n",
	   sane_strstatus (status));
      return status;
    }

  /* waits for valid data */
  do
    sanei_genesys_test_buffer_empty (dev, &steps);
  while (steps);

  /* now we're on target, we can read data */
  status = sanei_genesys_read_data_from_scanner (dev, data, size);
  if (status != SANE_STATUS_GOOD)
    {
      free (data);
      DBG (DBG_error,
	   "gl843_search_start_position: failed to read data: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status = gl843_stop_action (dev);
  if (status != SANE_STATUS_GOOD)
    {
      free (data);
      DBG (DBG_error, "gl843_search_strip: gl843_stop_action failed\n");
      return status;
    }

  pass = 0;
  if (DBG_LEVEL >= DBG_data)
    {
      sprintf (title, "search_strip_%s_%s%02d.pnm",
	       black ? "black" : "white", forward ? "fwd" : "bwd", pass);
      sanei_genesys_write_pnm_file (title, data, depth, channels, pixels,
				    lines);
    }

  /* loop until strip is found or maximum pass number done */
  found = 0;
  while (pass < 20 && !found)
    {
      status =
	gl843_bulk_write_register (dev, local_reg, GENESYS_GL843_MAX_REGS);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "gl843_search_strip: Failed to bulk write registers: %s\n",
	       sane_strstatus (status));
	  return status;
	}

      /* now start scan */
      status = gl843_begin_scan (dev, local_reg, SANE_TRUE);
      if (status != SANE_STATUS_GOOD)
	{
	  free (data);
	  DBG (DBG_error,
	       "gl843_search_strip: failed to begin scan: %s\n",
	       sane_strstatus (status));
	  return status;
	}

      /* waits for valid data */
      do
	sanei_genesys_test_buffer_empty (dev, &steps);
      while (steps);

      /* now we're on target, we can read data */
      status = sanei_genesys_read_data_from_scanner (dev, data, size);
      if (status != SANE_STATUS_GOOD)
	{
	  free (data);
	  DBG (DBG_error,
	       "gl843_search_start_position: failed to read data: %s\n",
	       sane_strstatus (status));
	  return status;
	}

      status = gl843_stop_action (dev);
      if (status != SANE_STATUS_GOOD)
	{
	  free (data);
	  DBG (DBG_error, "gl843_search_strip: gl843_stop_action failed\n");
	  return status;
	}

      if (DBG_LEVEL >= DBG_data)
	{
	  sprintf (title, "search_strip_%s_%s%02d.pnm",
		   black ? "black" : "white", forward ? "fwd" : "bwd", pass);
	  sanei_genesys_write_pnm_file (title, data, depth, channels,
					pixels, lines);
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
		  DBG (DBG_data,
		       "gl843_search_strip: strip found forward during pass %d at line %d\n",
		       pass, y);
		}
	      else
		{
		  DBG (DBG_data,
		       "gl843_search_strip: pixels=%d, count=%d (%d%%)\n",
		       pixels, count, (100 * count) / pixels);
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
	      DBG (DBG_data,
		   "gl843_search_strip: strip found backward during pass %d \n",
		   pass);
	    }
	  else
	    {
	      DBG (DBG_data,
		   "gl843_search_strip: pixels=%d, count=%d (%d%%)\n",
		   pixels, count, (100 * count) / pixels);
	    }
	}
      pass++;
    }
  free (data);
  if (found)
    {
      status = SANE_STATUS_GOOD;
      DBG (DBG_info, "gl843_search_strip: %s strip found\n",
	   black ? "black" : "white");
    }
  else
    {
      status = SANE_STATUS_UNSUPPORTED;
      DBG (DBG_info, "gl843_search_strip: %s strip not found\n",
	   black ? "black" : "white");
    }

  DBG (DBG_proc, "gl843_search_strip: completed\n");
  return status;
}

/** the gl843 command set */
static Genesys_Command_Set gl843_cmd_set = {
  "gl843-generic",		/* the name of this set */

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

  gl843_bulk_full_size,

  gl843_set_fe,
  gl843_set_powersaving,
  gl843_save_power,

  gl843_set_motor_power,
  gl843_set_lamp_power,

  gl843_begin_scan,
  gl843_end_scan,

  gl843_send_gamma_table,

  gl843_search_start_position,

  gl843_offset_calibration,
  gl843_coarse_gain_calibration,
  gl843_led_calibration,

  gl843_slow_back_home,

  gl843_bulk_write_register,
  NULL,
  gl843_bulk_read_data,

  gl843_update_hardware_sensors,

  gl843_load_document,
  gl843_detect_document_end,
  gl843_eject_document,
  gl843_search_strip,

  gl843_is_compatible_calibration,
  NULL,
  gl843_send_shading_data
};

SANE_Status
sanei_gl843_init_cmd_set (Genesys_Device * dev)
{
  dev->model->cmd_set = &gl843_cmd_set;
  return SANE_STATUS_GOOD;
}

/* vim: set sw=2 cino=>2se-1sn-1s{s^-1st0(0u0 smarttab expandtab: */
