/* sane - Scanner Access Now Easy.

   Copyright (C) 2003 Oliver Rauch
   Copyright (C) 2003, 2004 Henning Meier-Geinitz <henning@meier-geinitz.de>
   Copyright (C) 2004 Gerhard Jaeger <gerhard@gjaeger.de>
   Copyright (C) 2004 - 2007 Stephane Voltz <stef.dev@free.fr>
   Copyright (C) 2005, 2006 Pierre Willenbrock <pierre@pirsoft.dnsalias.org>
   Copyright (C) 2007 Luke <iceyfor@gmail.com>
   
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

#include "../include/sane/config.h"

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"
#include "../include/sane/saneopts.h"

#include "../include/sane/sanei_backend.h"
#include "../include/sane/sanei_config.h"
#include "../include/sane/sanei_usb.h"

#include "genesys.h"

/* Individual bits */
#define REG01_CISSET	0x80
#define REG01_DOGENB	0x40
#define REG01_DVDSET	0x20
#define REG01_FASTMOD	0x10
#define REG01_COMPENB	0x08
#define REG01_DRAMSEL	0x04
#define REG01_SHDAREA	0x02
#define REG01_SCAN	0x01

#define REG02_NOTHOME	0x80
#define REG02_ACDCDIS	0x40
#define REG02_AGOHOME	0x20
#define REG02_MTRPWR	0x10
#define REG02_FASTFED	0x08
#define REG02_MTRREV	0x04
#define REG02_STEPSEL	0x03

#define REG02_FULLSTEP	0x00
#define REG02_HALFSTEP	0x01
#define REG02_QUATERSTEP	0x02

#define REG03_TG3	0x80
#define REG03_AVEENB	0x40
#define REG03_XPASEL	0x20
#define REG03_LAMPPWR	0x10
#define REG03_LAMPDOG	0x08
#define REG03_LAMPTIM	0x07

#define REG04_LINEART	0x80
#define REG04_BITSET	0x40
#define REG04_ADTYPE	0x30
#define REG04_FILTER	0x0c
#define REG04_FESET	0x03

#define REG05_DPIHW	0xc0
#define REG05_DPIHW_600	0x00
#define REG05_DPIHW_1200	0x40
#define REG05_DPIHW_2400	0x80
#define REG05_GMMTYPE	0x30
#define REG05_GMM14BIT  0x10
#define REG05_GMMENB	0x08
#define REG05_LEDADD	0x04
#define REG05_BASESEL	0x03

#define REG06_PWRBIT	0x10
#define REG06_GAIN4	0x08
#define REG06_OPTEST	0x07

#define REG07_DMASEL	0x02
#define REG07_DMARDWR	0x01

#define REG16_CTRLHI	0x80
#define REG16_SELINV	0x40
#define REG16_TGINV	0x20
#define REG16_CK1INV	0x10
#define REG16_CK2INV	0x08
#define REG16_CTRLINV	0x04
#define REG16_CKDIS	0x02
#define REG16_CTRLDIS	0x01

#define REG17_TGMODE	0xc0
#define REG17_TGMODE_NO_DUMMY	0x00
#define REG17_TGMODE_REF	0x40
#define REG17_TGMODE_XPA	0x80
#define REG17_TGW	0x3f

#define REG18_CNSET	0x80
#define REG18_DCKSEL	0x60
#define REG18_CKTOGGLE	0x10
#define REG18_CKDELAY	0x0c
#define REG18_CKSEL	0x03

#define REG1D_CKMANUAL	0x80

#define REG1E_WDTIME	0xf0
#define REG1E_LINESEL	0x0f

#define REG41_PWRBIT	0x80
#define REG41_BUFEMPTY	0x40
#define REG41_FEEDFSH	0x20
#define REG41_SCANFSH	0x10
#define REG41_HOMESNR	0x08
#define REG41_LAMPSTS	0x04
#define REG41_FEBUSY	0x02
#define REG41_MOTMFLG	0x01

#define REG66_LOW_CURRENT	0x10

#define REG6A_FSTPSEL	0xc0
#define REG6A_FASTPWM	0x3f

#define REG6C_TGTIME	0xc0
#define REG6C_Z1MOD	0x38
#define REG6C_Z2MOD	0x07

/* we don't need a genesys_gl646.h yet, declarations aren't numerous enough */
/* writable registers */
enum
{
  reg_0x01 = 0,
  reg_0x02,
  reg_0x03,
  reg_0x04,
  reg_0x05,
  reg_0x06,
  reg_0x07,
  reg_0x08,
  reg_0x09,
  reg_0x0a,
  reg_0x0b,
  reg_0x10,
  reg_0x11,
  reg_0x12,
  reg_0x13,
  reg_0x14,
  reg_0x15,
  reg_0x16,
  reg_0x17,
  reg_0x18,
  reg_0x19,
  reg_0x1a,
  reg_0x1b,
  reg_0x1c,
  reg_0x1d,
  reg_0x1e,
  reg_0x1f,
  reg_0x20,
  reg_0x21,
  reg_0x22,
  reg_0x23,
  reg_0x24,
  reg_0x25,
  reg_0x26,
  reg_0x27,
  reg_0x28,
  reg_0x29,
  reg_0x2c,
  reg_0x2d,
  reg_0x2e,
  reg_0x2f,
  reg_0x30,
  reg_0x31,
  reg_0x32,
  reg_0x33,
  reg_0x34,
  reg_0x35,
  reg_0x36,
  reg_0x37,
  reg_0x38,
  reg_0x39,
  reg_0x3d,
  reg_0x3e,
  reg_0x3f,
  reg_0x52,
  reg_0x53,
  reg_0x54,
  reg_0x55,
  reg_0x56,
  reg_0x57,
  reg_0x58,
  reg_0x59,
  reg_0x5a,
  reg_0x5b,
  reg_0x5c,
  reg_0x5d,
  reg_0x5e,
  reg_0x60,
  reg_0x61,
  reg_0x62,
  reg_0x63,
  reg_0x64,
  reg_0x65,
  reg_0x66,
  reg_0x67,
  reg_0x68,
  reg_0x69,
  reg_0x6a,
  reg_0x6b,
  reg_0x6c,
  reg_0x6d,
  GENESYS_GL646_MAX_REGS
};

/* Write to many registers */
static SANE_Status
gl646_bulk_write_register (Genesys_Device * dev,
			   Genesys_Register_Set * reg, size_t elems)
{
  SANE_Status status;
  u_int8_t outdata[8];
  u_int8_t buffer[GENESYS_MAX_REGS * 2];
  size_t size;
  unsigned int i;

  /* handle differently sized register sets, reg[0x00] may be the last one */
  i = 0;
  while ((i < elems) && (reg[i].address != 0))
    i++;
  elems = i;
  size = i * 2;

  DBG (DBG_io, "gl646_bulk_write_register (elems= %lu, size = %lu)\n", 
       (u_long) elems, (u_long) size);


  outdata[0] = BULK_OUT;
  outdata[1] = BULK_REGISTER;
  outdata[2] = 0x00;
  outdata[3] = 0x00;
  outdata[4] = (size & 0xff);
  outdata[5] = ((size >> 8) & 0xff);
  outdata[6] = ((size >> 16) & 0xff);
  outdata[7] = ((size >> 24) & 0xff);

  status =
    sanei_usb_control_msg (dev->dn, REQUEST_TYPE_OUT, REQUEST_BUFFER,
			   VALUE_BUFFER, INDEX, sizeof (outdata), outdata);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl646_bulk_write_register: failed while writing command: %s\n",
	   sane_strstatus (status));
      return status;
    }

  /* copy registers and values in data buffer */
  for (i = 0; i < size; i += 2)
    {
      buffer[i] = reg[i / 2].address;
      buffer[i + 1] = reg[i / 2].value;
    }

  status = sanei_usb_write_bulk (dev->dn, buffer, &size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl646_bulk_write_register: failed while writing bulk data: %s\n",
	   sane_strstatus (status));
      return status;
    }

  for (i = 0; i < size; i += 2)
    DBG (DBG_io2, "reg[0x%02x] = 0x%02x\n", buffer[i], buffer[i + 1]);

  DBG (DBG_io, "gl646_bulk_write_register: wrote %lu bytes, %lu registers\n", 
       (u_long) size, (u_long) elems);
  return status;
}

/* Write bulk data (e.g. shading, gamma) */
static SANE_Status
gl646_bulk_write_data (Genesys_Device * dev, u_int8_t addr,
		       u_int8_t * data, size_t len)
{
  SANE_Status status;
  size_t size;
  u_int8_t outdata[8];

  DBG (DBG_io, "gl646_bulk_write_data writing %lu bytes\n", (u_long) len);

  status =
    sanei_usb_control_msg (dev->dn, REQUEST_TYPE_OUT, REQUEST_REGISTER,
			   VALUE_SET_REGISTER, INDEX, 1, &addr);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl646_bulk_write_data failed while setting register: %s\n",
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
      outdata[2] = 0x00;
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
	       "gl646_bulk_write_data failed while writing command: %s\n",
	       sane_strstatus (status));
	  return status;
	}

      status = sanei_usb_write_bulk (dev->dn, data, &size);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "gl646_bulk_write_data failed while writing bulk data: %s\n",
	       sane_strstatus (status));
	  return status;
	}

      DBG (DBG_io2,
	   "gl646_bulk_write_data wrote %lu bytes, %lu remaining\n",
	   (u_long) size, (u_long) (len - size));

      len -= size;
      data += size;
    }

  DBG (DBG_io, "gl646_bulk_write_data: completed\n");

  return status;
}

/* Read bulk data (e.g. scanned data) */
static SANE_Status
gl646_bulk_read_data (Genesys_Device * dev, u_int8_t addr,
		      u_int8_t * data, size_t len)
{
  SANE_Status status;
  size_t size;
  u_int8_t outdata[8];

  DBG (DBG_io, "gl646_bulk_read_data: requesting %lu bytes\n", (u_long) len);

  status =
    sanei_usb_control_msg (dev->dn, REQUEST_TYPE_OUT, REQUEST_REGISTER,
			   VALUE_SET_REGISTER, INDEX, 1, &addr);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl646_bulk_read_data failed while setting register: %s\n",
	   sane_strstatus (status));
      return status;
    }

  outdata[0] = BULK_IN;
  outdata[1] = BULK_RAM;
  outdata[2] = 0x00;
  outdata[3] = 0x00;
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
	   "gl646_bulk_read_data failed while writing command: %s\n",
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
	   "gl646_bulk_read_data: trying to read %lu bytes of data\n",
	   (u_long) size);
      status = sanei_usb_read_bulk (dev->dn, data, &size);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "gl646_bulk_read_data failed while reading bulk data: %s\n",
	       sane_strstatus (status));
	  return status;
	}

      DBG (DBG_io2,
	   "gl646_bulk_read_data read %lu bytes, %lu remaining\n",
	   (u_long) size, (u_long) (len - size));

      len -= size;
      data += size;
    }

  DBG (DBG_io, "gl646_bulk_read_data: completed\n");

  return status;
}

static SANE_Bool
gl646_get_fast_feed_bit (Genesys_Register_Set * regs)
{
  Genesys_Register_Set *r = NULL;

  r = sanei_genesys_get_address (regs, 0x02);
  if (r && (r->value & REG02_FASTFED))
    return SANE_TRUE;
  return SANE_FALSE;
}

static SANE_Bool
gl646_get_filter_bit (Genesys_Register_Set * regs)
{
  Genesys_Register_Set *r = NULL;

  r = sanei_genesys_get_address (regs, 0x04);
  if (r && (r->value & REG04_FILTER))
    return SANE_TRUE;
  return SANE_FALSE;
}

static SANE_Bool
gl646_get_lineart_bit (Genesys_Register_Set * regs)
{
  Genesys_Register_Set *r = NULL;

  r = sanei_genesys_get_address (regs, 0x04);
  if (r && (r->value & REG04_LINEART))
    return SANE_TRUE;
  return SANE_FALSE;
}

static SANE_Bool
gl646_get_bitset_bit (Genesys_Register_Set * regs)
{
  Genesys_Register_Set *r = NULL;

  r = sanei_genesys_get_address (regs, 0x04);
  if (r && (r->value & REG04_BITSET))
    return SANE_TRUE;
  return SANE_FALSE;
}

static SANE_Bool
gl646_get_gain4_bit (Genesys_Register_Set * regs)
{
  Genesys_Register_Set *r = NULL;

  r = sanei_genesys_get_address (regs, 0x06);
  if (r && (r->value & REG06_GAIN4))
    return SANE_TRUE;
  return SANE_FALSE;
}

static SANE_Bool
gl646_test_buffer_empty_bit (SANE_Byte val)
{
  if (val & REG41_BUFEMPTY)
    return SANE_TRUE;
  return SANE_FALSE;
}

static SANE_Bool
gl646_test_motor_flag_bit (SANE_Byte val)
{
  if (val & REG41_MOTMFLG)
    return SANE_TRUE;
  return SANE_FALSE;
}

/*
 * sets r21 to r23 registers, ie number of steps for motor
 * todo : some hardcoded values will be put in Motor_Struct 
 */
static void
gl646_setup_steps (Genesys_Device * dev, Genesys_Register_Set * regs, int dpi)
{
  Genesys_Register_Set *r;
  int fw, bw;

  if (regs[reg_0x04].value & REG04_FILTER)
    {
      /* monochrome */
      switch (dev->model->motor_type)
	{
	case MOTOR_HP2300:
	case MOTOR_HP2400:
	  if (dpi <= 75)
	    fw = 120;
	  else if (dpi <= 150)
	    fw = 67;
	  else if (dpi <= 300)
	    fw = 44;
	  else
	    fw = 3;
	  bw = 16;
	  break;
	case MOTOR_5345:
	default:
	  if (dpi <= 600)
	    fw = 255;
	  else
	    fw = 32;
	  bw = 64;
	  break;
	}
    }
  else
    {
      switch (dev->model->motor_type)
	{
	case MOTOR_HP2300:
	case MOTOR_HP2400:
	  if (dpi <= 75)
	    fw = 120;
	  else if (dpi <= 150)
	    fw = 67;
	  else if (dpi <= 300)
	    fw = 44;
	  else
	    fw = 3;
	  bw = 16;
	  break;

	case MOTOR_5345:
	default:
	  if (dpi < 200)
	    {
	      fw = 255;
	      bw = 64;
	    }
	  else if (dpi < 400)
	    {
	      fw = 64;
	      bw = 32;
	    }
	  else if (dpi <= 600)
	    {
	      fw = 32;
	      bw = 32;
	    }
	  else
	    {
	      fw = 16;
	      bw = 146;
	    }
	}
    }

  r = sanei_genesys_get_address (regs, 0x21);
  r->value = fw;
  r = sanei_genesys_get_address (regs, 0x22);
  r->value = bw;
  r = sanei_genesys_get_address (regs, 0x23);
  r->value = bw;
  r = sanei_genesys_get_address (regs, 0x24);
  r->value = fw;
}

/** copy sensor specific settings */
/* *dev  : device infos
   *regs : regiters to be set
   extended : do extebded set up
   half_ccd: set up for half ccd resolution
   all registers 08-0B, 10-1D, 52-59 are set up. They shouldn't
   appear anywhere else but in register_ini
*/
static void
gl646_setup_sensor (Genesys_Device * dev,
		    Genesys_Register_Set * regs,
		    SANE_Bool extended, SANE_Bool half_ccd)
{
  Genesys_Register_Set *r;
  int i;

  r = sanei_genesys_get_address (regs, 0x08);
  for (i = 0; i < 4; i++, r++)
    r->value = dev->sensor.regs_0x08_0x0b[i];

  r = sanei_genesys_get_address (regs, 0x10);
  for (i = 0; i < 14; i++, r++)
    r->value = dev->sensor.regs_0x10_0x1d[i];

  r = sanei_genesys_get_address (regs, 0x52);
  for (i = 0; i < 13; i++, r++)
    r->value = dev->sensor.regs_0x52_0x5e[i];

  /* don't go nay further if no extended setup */
  if (!extended)
    return;

  /* todo : add more CCD types if needed */
  /* we might want to expand the Sensor struct to have these
     2 kind of settings */
  if (dev->model->ccd_type == CCD_5345)
    {
      if (half_ccd)
	{
	  /* settings for CCD used at half is max resolution */
	  r = sanei_genesys_get_address (regs, 0x08);
	  r->value = 0x00;
	  r = sanei_genesys_get_address (regs, 0x09);
	  r->value = 0x05;
	  r = sanei_genesys_get_address (regs, 0x0a);
	  r->value = 0x06;
	  r = sanei_genesys_get_address (regs, 0x0b);
	  r->value = 0x08;
	  r = sanei_genesys_get_address (regs, 0x18);
	  r->value = 0x28;
	  r = sanei_genesys_get_address (regs, 0x58);
	  r->value = 0x80 | (r->value & 0x03);	/* VSMP=16 */
	}
      else
	{
	  /* swap latch times */
	  r = sanei_genesys_get_address (regs, 0x18);
	  r->value = 0x30;
	  r = sanei_genesys_get_address (regs, 0x52);
	  for (i = 0; i < 6; i++, r++)
	    r->value = dev->sensor.regs_0x52_0x5e[(i + 3) % 6];
	  r = sanei_genesys_get_address (regs, 0x58);
	  r->value = 0x20 | (r->value & 0x03);	/* VSMP=4 */
	}
      return;
    }

  if (dev->model->ccd_type == CCD_HP2300)
    {
      /* settings for CCD used at half is max resolution */
      if (half_ccd)
	{
	  r = sanei_genesys_get_address (regs, 0x08);
	  r->value = 0x16;
	  r = sanei_genesys_get_address (regs, 0x09);
	  r->value = 0x00;
	  r = sanei_genesys_get_address (regs, 0x0a);
	  r->value = 0x01;
	  r = sanei_genesys_get_address (regs, 0x0b);
	  r->value = 0x03;
	  /* manual clock programming */
	  r = sanei_genesys_get_address (regs, 0x1d);
	  r->value |= 0x80;
	}
      else
	{
	  r = sanei_genesys_get_address (regs, 0x08);
	  r->value = 1;
	  r = sanei_genesys_get_address (regs, 0x09);
	  r->value = 3;
	  r = sanei_genesys_get_address (regs, 0x0a);
	  r->value = 4;
	  r = sanei_genesys_get_address (regs, 0x0b);
	  r->value = 6;
	}
      r = sanei_genesys_get_address (regs, 0x58);
      r->value = 0x80 | (r->value & 0x03);	/* VSMP=16 */
      return;
    }

  if (dev->model->ccd_type == CCD_HP2400)
    {
      /* settings for CCD used at half is max resolution */
      if (half_ccd)
	{
	  r = sanei_genesys_get_address (regs, 0x08);
	  r->value = 0x14;
	  r = sanei_genesys_get_address (regs, 0x09);
	  r->value = 0x15;
	  r = sanei_genesys_get_address (regs, 0x0a);
	  r->value = 0x00;
	  r = sanei_genesys_get_address (regs, 0x0b);
	  r->value = 0x00;
	}
      else
	{
	  r = sanei_genesys_get_address (regs, 0x08);
	  r->value = 2;
	  r = sanei_genesys_get_address (regs, 0x09);
	  r->value = 4;
	  r = sanei_genesys_get_address (regs, 0x0a);
	  r->value = 0;
	  r = sanei_genesys_get_address (regs, 0x0b);
	  r->value = 0;
	}

    }
}

/** Test if the ASIC works 
 */
static SANE_Status
gl646_asic_test (Genesys_Device * dev)
{
  SANE_Status status;
  u_int8_t val;
  u_int8_t *data;
  u_int8_t *verify_data;
  size_t size, verify_size;
  unsigned int i;

  DBG (DBG_proc, "gl646_asic_test\n");

  /* set and read exposure time, compare if it's the same */
  status = sanei_genesys_write_register (dev, 0x38, 0xde);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "gl646_asic_test: failed to write register: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status = sanei_genesys_write_register (dev, 0x39, 0xad);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "gl646_asic_test: failed to write register: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status = sanei_genesys_read_register (dev, 0x4e, &val);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "gl646_asic_test: failed to read register: %s\n",
	   sane_strstatus (status));
      return status;
    }
  if (val != 0xde)		/* value of register 0x38 */
    {
      DBG (DBG_error, "gl646_asic_test: register contains invalid value\n");
      return SANE_STATUS_IO_ERROR;
    }

  status = sanei_genesys_read_register (dev, 0x4f, &val);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "gl646_asic_test: failed to read register: %s\n",
	   sane_strstatus (status));
      return status;
    }
  if (val != 0xad)		/* value of register 0x39 */
    {
      DBG (DBG_error, "gl646_asic_test: register contains invalid value\n");
      return SANE_STATUS_IO_ERROR;
    }

  /* ram test: */
  size = 0x40000;
  verify_size = size + 0x80;
  /* todo: looks like the read size must be a multiple of 128?
     otherwise the read doesn't succeed the second time after the scanner has 
     been plugged in. Very strange. */

  data = (u_int8_t *) malloc (size);
  if (!data)
    {
      DBG (DBG_error, "gl646_asic_test: could not allocate memory\n");
      return SANE_STATUS_NO_MEM;
    }

  verify_data = (u_int8_t *) malloc (verify_size);
  if (!verify_data)
    {
      free (data);
      DBG (DBG_error, "gl646_asic_test: could not allocate memory\n");
      return SANE_STATUS_NO_MEM;
    }

  for (i = 0; i < (size - 1); i += 2)
    {
      data[i] = i / 512;
      data[i + 1] = (i / 2) % 256;
    }

  status = sanei_genesys_set_buffer_address (dev, 0x0000);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "gl646_asic_test: failed to set buffer address: %s\n",
	   sane_strstatus (status));
      free (data);
      free (verify_data);
      return status;
    }

  status = gl646_bulk_write_data (dev, 0x3c, data, size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "gl646_asic_test: failed to bulk write data: %s\n",
	   sane_strstatus (status));
      free (data);
      free (verify_data);
      return status;
    }

  status = sanei_genesys_set_buffer_address (dev, 0x0000);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "gl646_asic_test: failed to set buffer address: %s\n",
	   sane_strstatus (status));
      free (data);
      free (verify_data);
      return status;
    }

  status =
    gl646_bulk_read_data (dev, 0x45, (u_int8_t *) verify_data, verify_size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "gl646_asic_test: failed to bulk read data: %s\n",
	   sane_strstatus (status));
      free (data);
      free (verify_data);
      return status;
    }

  /* i + 2 is needed as the changed address goes into effect only after one
     data word is sent. */
  for (i = 0; i < size; i++)
    {
      if (verify_data[i + 2] != data[i])
	{
	  DBG (DBG_error, "gl646_asic_test: data verification error\n");
	  free (data);
	  free (verify_data);
	  return SANE_STATUS_IO_ERROR;
	}
    }

  free (data);
  free (verify_data);

  DBG (DBG_info, "gl646_asic_test: completed\n");

  return SANE_STATUS_GOOD;
}

/* returns the max register bulk size */
static int
gl646_bulk_full_size (void)
{
  return GENESYS_GL646_MAX_REGS;
}

/*
 * Set all registers to default values 
 */
static void
gl646_init_regs (Genesys_Device * dev)
{
  int nr, addr, i;
  Genesys_Register_Set *r = NULL;

  DBG (DBG_proc, "gl646_init_regs\n");

  nr = 0;
  memset (dev->reg, 0, GENESYS_MAX_REGS * sizeof (Genesys_Register_Set));

  for (addr = 1; addr <= 0x0b; addr++)
    dev->reg[nr++].address = addr;
  for (addr = 0x10; addr <= 0x29; addr++)
    dev->reg[nr++].address = addr;
  for (addr = 0x2c; addr <= 0x39; addr++)
    dev->reg[nr++].address = addr;
  for (addr = 0x3d; addr <= 0x3f; addr++)
    dev->reg[nr++].address = addr;
  for (addr = 0x52; addr <= 0x5e; addr++)
    dev->reg[nr++].address = addr;
  for (addr = 0x60; addr <= 0x6d; addr++)
    dev->reg[nr++].address = addr;

/* ST12: 0x01 0x20 0x02 0x30 0x03 0x1f 0x04 0x13 0x05 0x10 */
/* ST24: 0x01 0x20 0x02 0x30 0x03 0x1f 0x04 0x13 0x05 0x50 */
  dev->reg[reg_0x01].value = 0x20 /*0x22 */ ;	/* enable shading, CCD, color, 1M */
  dev->reg[reg_0x02].value = 0x30 /*0x38 */ ;	/* auto home, one-table-move, full step */
  if (dev->model->motor_type == MOTOR_5345)
    dev->reg[reg_0x02].value |= 0x01;	/* half-step */
  dev->reg[reg_0x03].value = 0x1f /*0x17 */ ;	/* lamp on */
  dev->reg[reg_0x04].value = 0x13 /*0x03 */ ;	/* 8 bits data, 16 bits A/D, color, Wolfson fe *//* todo: according to spec, 0x0 is reserved? */

  dev->reg[reg_0x05].value = 0x00;	/* 12 bits gamma, disable gamma, 24 clocks/pixel */
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
    }
  if (dev->model->flags & GENESYS_FLAG_14BIT_GAMMA)
    dev->reg[reg_0x05].value |= REG05_GMM14BIT;

  if (dev->model->ccd_type == CCD_HP2300)
    dev->reg[reg_0x06].value = 0x00;	/* PWRBIT off, shading gain=4, normal AFE image capture */
  else
    dev->reg[reg_0x06].value = 0x18;	/* PWRBIT on, shading gain=8, normal AFE image capture */


  gl646_setup_sensor (dev, dev->reg, 0, 0);

  dev->reg[reg_0x1e].value = 0xf0;	/* watch-dog time */

/* ST12: 0x20 0x10 0x21 0x08 0x22 0x10 0x23 0x10 0x24 0x08 0x25 0x00 0x26 0x00 0x27 0xd4 0x28 0x01 0x29 0xff */
/* ST24: 0x20 0x10 0x21 0x08 0x22 0x10 0x23 0x10 0x24 0x08 0x25 0x00 0x26 0x00 0x27 0xd4 0x28 0x01 0x29 0xff */
  /* TODO this construct deserve a switch */
  if (dev->model->ccd_type != CCD_HP2300)
    {
      if (dev->model->ccd_type == CCD_HP2400)
	{
	  dev->reg[reg_0x1e].value = 0x40;
	  dev->reg[reg_0x1f].value = 0x10;
	  dev->reg[reg_0x20].value = 0x20;
	}
      else
	{
	  dev->reg[reg_0x1f].value = 0x01;	/* scan feed step for table one in two table mode only */
	  dev->reg[reg_0x20].value = 0x10 /*0x01 */ ;	/* n * 2k, below this condition, motor move forward *//* todo: huh, 2k is pretty low? */
	}
    }
  else
    {
      dev->reg[reg_0x1e].value = 0xf0;
      dev->reg[reg_0x1f].value = 0x10;
      dev->reg[reg_0x20].value = 0x20;
    }
  dev->reg[reg_0x21].value = 0x08 /*0x20 */ ;	/* table one steps number for forward slope curve of the acc/dec */
  dev->reg[reg_0x22].value = 0x10 /*0x08 */ ;	/* steps number of the forward steps for start/stop */
  dev->reg[reg_0x23].value = 0x10 /*0x08 */ ;	/* steps number of the backward steps for start/stop */
  dev->reg[reg_0x24].value = 0x08 /*0x20 */ ;	/* table one steps number backward slope curve of the acc/dec */
  dev->reg[reg_0x25].value = 0x00;	/* scan line numbers (7000) */
  dev->reg[reg_0x26].value = 0x00 /*0x1b */ ;
  dev->reg[reg_0x27].value = 0xd4 /*0x58 */ ;
  dev->reg[reg_0x28].value = 0x01;	/* PWM duty for lamp control */
  dev->reg[reg_0x29].value = 0xff;

/* ST12: 0x2c 0x02 0x2d 0x58 0x2e 0x6e 0x2f 0x80 */
/* ST24: 0x2c 0x02 0x2d 0x58 0x2e 0x6e 0x2f 0x80 */

  dev->reg[reg_0x2c].value = 0x02;	/* set resolution (600 DPI) */
  dev->reg[reg_0x2d].value = 0x58;
  dev->reg[reg_0x2e].value = 0x78;	/* set black&white threshold high level */
  dev->reg[reg_0x2f].value = 0x7f;	/* set black&white threshold low level */
  if (dev->model->ccd_type == CCD_5345)
    {
      dev->calib_reg[reg_0x2e].value = 0x6e;
      dev->calib_reg[reg_0x2f].value = 0x6e;
    }

/* ST12: 0x30 0x00 0x31 0x0e 0x32 0x17 0x33 0x70 0x34 0x0c 0x35 0x01 0x36 0x00 0x37 0x00 0x38 0x17 0x39 0x70 */
/* ST24: 0x30 0x00 0x31 0x0c 0x32 0x2a 0x33 0xf8 0x34 0x0c 0x35 0x01 0x36 0x00 0x37 0x00 0x38 0x2a 0x39 0xf8 */
  dev->reg[reg_0x30].value = 0x00;	/* begin pixel positon (16) */
  dev->reg[reg_0x31].value = dev->sensor.dummy_pixel /*0x10 */ ;	/* TGW + 2*TG_SHLD + x  */
  dev->reg[reg_0x32].value = 0x2a /*0x15 */ ;	/* end pixel position (5390) */
  dev->reg[reg_0x33].value = 0xf8 /*0x0e */ ;	/* TGW + 2*TG_SHLD + y   */
  dev->reg[reg_0x34].value = dev->sensor.dummy_pixel;
  dev->reg[reg_0x35].value = 0x01 /*0x00 */ ;	/* set maximum word size per line, for buffer full control (10800) */
  dev->reg[reg_0x36].value = 0x00 /*0x2a */ ;
  dev->reg[reg_0x37].value = 0x00 /*0x30 */ ;
  dev->reg[reg_0x38].value = HIBYTE (dev->settings.exposure_time) /*0x2a */ ;	/* line period (exposure time = 11000 pixels) */
  dev->reg[reg_0x39].value = LOBYTE (dev->settings.exposure_time) /*0xf8 */ ;

/* ST12: 0x3d 0x00 0x3e 0x00 0x3f 0x01 */
/* ST24: 0x3d 0x00 0x3e 0x00 0x3f 0x01 */
  dev->reg[reg_0x3d].value = 0x00;	/* set feed steps number of motor move */
  dev->reg[reg_0x3e].value = 0x00;
  dev->reg[reg_0x3f].value = 0x01 /*0x00 */ ;

#if 0
/* ST12: 0x52 0x0f 0x53 0x13 0x54 0x17 0x55 0x03 0x56 0x07 0x57 0x0b 0x58 0x83 0x59 0x00 */
/* ST24: 0x52 0x0f 0x53 0x13 0x54 0x17 0x55 0x03 0x56 0x07 0x57 0x0b 0x58 0x83 0x59 0x00 */
  dev->reg[reg_0x52].value = 0x0f /*0x13 */ ;	/* R position */
  dev->reg[reg_0x53].value = 0x13 /*0x17 */ ;
  dev->reg[reg_0x54].value = 0x17 /*0x03 */ ;	/* G position */
  dev->reg[reg_0x55].value = 0x03 /*0x07 */ ;
  dev->reg[reg_0x56].value = 0x07 /*0x0b */ ;	/* B position on AFE signal */
  dev->reg[reg_0x57].value = 0x0b /*0x0f */ ;
  dev->reg[reg_0x58].value = 0x83 /*0x23 */ ;	/* set rising edge and width of image sample for AFE */
  dev->reg[reg_0x59].value = 0x00;	/* set rising edge and width of dark voltage position */
#endif

  for (i = 0; i < 6; i++, r++)
    {
      r = sanei_genesys_get_address (dev->reg, 0x52 + i);
      r->value = dev->sensor.regs_0x52_0x5e[i];
    }

/* ST12: 0x5a 0xc1 0x5b 0x00 0x5c 0x00 0x5d 0x00 0x5e 0x00 */
/* ST24: 0x5a 0xc1 0x5b 0x00 0x5c 0x00 0x5d 0x00 0x5e 0x00 */
  dev->reg[reg_0x5a].value = 0xc1;	/* Wolfson AFE, reset level clamp on pixel, RLC of AFE for line rate scanning type */
  dev->reg[reg_0x5b].value = 0x00;	/* First point of rising edge */
  dev->reg[reg_0x5c].value = 0x00;	/* First point of falling edge */
  dev->reg[reg_0x5d].value = 0x00;	/* Second point of rising edge */
  dev->reg[reg_0x5e].value = 0x00;	/* Second point of falling edge */

/* ST12: 0x60 0x00 0x61 0x00 0x62 0x00 0x63 0x00 0x64 0x00 0x65 0x00 0x66 0x00 0x67 0x00 0x68 0x51 0x69 0x20 */
/* ST24: 0x60 0x00 0x61 0x00 0x62 0x00 0x63 0x00 0x64 0x00 0x65 0x00 0x66 0x00 0x67 0x00 0x68 0x51 0x69 0x20 */
  dev->reg[reg_0x60].value = 0x00;	/* Z1MOD, 60h:61h:(6D b5:b3), remainder for start/stop */
  dev->reg[reg_0x61].value = 0x00;	/* (21h+22h)/LPeriod */
  dev->reg[reg_0x62].value = 0x00;	/* Z2MODE, 62h:63h:(6D b2:b0), remainder for start scan */
  dev->reg[reg_0x63].value = 0x00;	/* (3Dh+3Eh+3Fh)/LPeriod for one-table mode,(21h+1Fh)/LPeriod */
  dev->reg[reg_0x64].value = 0x00;	/* motor PWM frequency */
  dev->reg[reg_0x65].value = 0x00;	/* PWM duty cycle for table one motor phase (63 = max) */
  if (dev->model->motor_type == MOTOR_5345)
    dev->reg[reg_0x65].value = 0x02;	/* PWM duty cycle for table one motor phase (63 = max) */
  dev->reg[reg_0x66].value = dev->gpo.value[0];
  dev->reg[reg_0x67].value = dev->gpo.value[1];
  dev->reg[reg_0x68].value = dev->gpo.enable[0];
  dev->reg[reg_0x69].value = dev->gpo.enable[1];

/* ST12: 0x6a 0x7f 0x6b 0xff 0x6c 0x00 0x6d 0x01 */
/* ST24: 0x6a 0x40 0x6b 0xff 0x6c 0x00 0x6d 0x01 */
  if (dev->model->motor_type == MOTOR_HP2300
      || dev->model->motor_type == MOTOR_HP2400)
    {
      dev->reg[reg_0x6a].value = 0x7f;	/* table two steps number for acc/dec */
      dev->reg[reg_0x6b].value = 0x78;	/* table two steps number for acc/dec */
    }
  else
    {
      dev->reg[reg_0x6b].value = 0xff;	/* table two steps number for acc/dec */
      dev->reg[reg_0x6a].value = 0x40;	/* table two fast moving step type, PWM duty for table two */
    }
  dev->reg[reg_0x6c].value = 0x00;	/* peroid times for LPeriod, expR,expG,expB, Z1MODE, Z2MODE (one period time) */
  dev->reg[reg_0x6d].value = 0x01 /*0x1f */ ;	/* select deceleration steps whenever go home (0), accel/decel stop time (31 * LPeriod) */
  if (dev->model->motor_type == MOTOR_5345)
    {
      dev->reg[reg_0x6a].value = 0x42;	/* table two fast moving step type, PWM duty for table two */
      dev->reg[reg_0x6d].value = 0x41 /*0x1f */ ;	/* select deceleration steps whenever go home (0), accel/decel stop time (31 * LPeriod) */
    }
}


/* Send slope table for motor movement 
   slope_table in machine byte order
*/
static SANE_Status
gl646_send_slope_table (Genesys_Device * dev, int table_nr,
			u_int16_t * slope_table, int steps)
{
  int dpihw;
  int start_address;
  SANE_Status status;
  u_int8_t *table;
#ifdef WORDS_BIGENDIAN
  int i;
#endif

  DBG (DBG_proc, "gl646_send_slope_table (table_nr = %d, steps = %d)\n",
       table_nr, steps);

  dpihw = dev->reg[reg_0x05].value >> 6;

  if (dpihw == 0)		/* 600 dpi */
    start_address = 0x08000;
  else if (dpihw == 1)		/* 1200 dpi */
    start_address = 0x10000;
  else if (dpihw == 2)		/* 2400 dpi */
    start_address = 0x1f800;
  else				/* reserved */
    return SANE_STATUS_INVAL;

#ifdef WORDS_BIGENDIAN
  table = (u_int8_t *) malloc (steps * 2);
  for (i = 0; i < steps; i++)
    {
      table[i * 2] = slope_table[i] & 0xff;
      table[i * 2 + 1] = slope_table[i] >> 8;
    }
#else
  table = (u_int8_t *) slope_table;
#endif

  status =
    sanei_genesys_set_buffer_address (dev, start_address + table_nr * 0x100);
  if (status != SANE_STATUS_GOOD)
    {
#ifdef WORDS_BIGENDIAN
      free (table);
#endif
      DBG (DBG_error,
	   "gl646_send_slope_table: failed to set buffer address: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status = gl646_bulk_write_data (dev, 0x3c, (u_int8_t *) table, steps * 2);
  if (status != SANE_STATUS_GOOD)
    {
#ifdef WORDS_BIGENDIAN
      free (table);
#endif
      DBG (DBG_error,
	   "gl646_send_slope_table: failed to send slope table: %s\n",
	   sane_strstatus (status));
      return status;
    }

#ifdef WORDS_BIGENDIAN
  free (table);
#endif
  DBG (DBG_proc, "gl646_send_slope_table: completed\n");
  return status;
}


/* Set values of analog frontend */
static SANE_Status
gl646_set_fe (Genesys_Device * dev, u_int8_t set)
{
  SANE_Status status;
  int i;
  u_int8_t val;

  DBG (DBG_proc, "gl646_set_fe (%s)\n",
       set == AFE_INIT ? "init" : set == AFE_SET ? "set" : set ==
       AFE_POWER_SAVE ? "powersave" : "huh?");

  if ((dev->reg[reg_0x04].value & REG04_FESET) != 0x03)
    {
      DBG (DBG_proc, "gl646_set_fe(): unspported frontend type %d\n",
	   dev->reg[reg_0x04].value & REG04_FESET);
      return SANE_STATUS_UNSUPPORTED;
    }

  if (set == AFE_INIT)
    {
      DBG (DBG_proc, "gl646_set_fe(): setting DAC %u\n",
	   dev->model->dac_type);
      sanei_genesys_init_fe (dev);

      /* reset only done on init */
      status = sanei_genesys_fe_write_data (dev, 0x04, 0x80);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "gl646_set_fe: reset fe failed: %s\n",
	       sane_strstatus (status));
	  return status;
	  if (dev->model->ccd_type == CCD_HP2300
	      || dev->model->ccd_type == CCD_HP2400)
	    {
	      val = 0x07;
	      status =
		sanei_usb_control_msg (dev->dn, REQUEST_TYPE_OUT,
				       REQUEST_REGISTER, GPIO_OUTPUT_ENABLE,
				       INDEX, 1, &val);
	      if (status != SANE_STATUS_GOOD)
		{
		  DBG (DBG_error,
		       "gl646_set_fe failed resetting frontend: %s\n",
		       sane_strstatus (status));
		  return status;
		}
	    }
	}
    }

  if (set == AFE_POWER_SAVE)
    {
      status = sanei_genesys_fe_write_data (dev, 0x01, 0x02);
      if (status != SANE_STATUS_GOOD)
	DBG (DBG_error, "gl646_set_fe: writing data failed: %s\n",
	     sane_strstatus (status));
      return status;
    }

  /* todo :  base this test on cfg reg3 or a CCD family flag to be created */
  /*if (dev->model->ccd_type!=CCD_HP2300 && dev->model->ccd_type!=CCD_HP2400) */
  {
    status = sanei_genesys_fe_write_data (dev, 0x00, dev->frontend.reg[0]);
    if (status != SANE_STATUS_GOOD)
      {
	DBG (DBG_error, "gl646_set_fe: writing reg0 failed: %s\n",
	     sane_strstatus (status));
	return status;
      }
    status = sanei_genesys_fe_write_data (dev, 0x02, dev->frontend.reg[2]);
    if (status != SANE_STATUS_GOOD)
      {
	DBG (DBG_error, "gl646_set_fe: writing reg2 failed: %s\n",
	     sane_strstatus (status));
	return status;
      }
  }

  status = sanei_genesys_fe_write_data (dev, 0x01, dev->frontend.reg[1]);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "gl646_set_fe: writing reg1 failed: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status = sanei_genesys_fe_write_data (dev, 0x03, dev->frontend.reg[3]);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "gl646_set_fe: writing reg3 failed: %s\n",
	   sane_strstatus (status));
      return status;
    }

  for (i = 0; i < 3; i++)
    {
      status =
	sanei_genesys_fe_write_data (dev, 0x24 + i, dev->frontend.sign[i]);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "gl646_set_fe: writing sign[%d] failed: %s\n",
	       i, sane_strstatus (status));
	  return status;
	}

      status =
	sanei_genesys_fe_write_data (dev, 0x28 + i, dev->frontend.gain[i]);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "gl646_set_fe: writing gain[%d] failed: %s\n",
	       i, sane_strstatus (status));
	  return status;
	}

      status =
	sanei_genesys_fe_write_data (dev, 0x20 + i, dev->frontend.offset[i]);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "gl646_set_fe: writing offset[%d] failed: %s\n", i,
	       sane_strstatus (status));
	  return status;
	}
    }

  DBG (DBG_proc, "gl646_set_fe: completed\n");

  return SANE_STATUS_GOOD;
}

static void
gl646_set_motor_power (Genesys_Register_Set * regs, SANE_Bool set)
{
  if (set)
    {
      sanei_genesys_set_reg_from_set (regs, 0x02,
				      sanei_genesys_read_reg_from_set (regs,
								       0x02) |
				      REG02_MTRPWR);
    }
  else
    {
      sanei_genesys_set_reg_from_set (regs, 0x02,
				      sanei_genesys_read_reg_from_set (regs,
								       0x02) &
				      ~REG02_MTRPWR);
    }
}

static void
gl646_set_lamp_power (Genesys_Device * dev, 
		      Genesys_Register_Set * regs, SANE_Bool set)
{
  if (set)
    {
      sanei_genesys_set_reg_from_set (regs, 0x03,
				      sanei_genesys_read_reg_from_set (regs,
								       0x03) |
				      REG03_LAMPPWR);
    }
  else
    {
      sanei_genesys_set_reg_from_set (regs, 0x03,
				      sanei_genesys_read_reg_from_set (regs,
								       0x03) &
				      ~REG03_LAMPPWR);
    }
}

static SANE_Status
gl646_save_power (Genesys_Device * dev, SANE_Bool enable)
{

  DBG (DBG_proc, "gl646_save_power: enable = %d\n", enable);

  if (enable)
    {
      /* gl646_set_fe (dev, AFE_POWER_SAVE); */
    }
  else
    {
      gl646_set_fe (dev, AFE_INIT);
    }

  return SANE_STATUS_GOOD;
}

static SANE_Status
gl646_set_powersaving (Genesys_Device * dev, int delay /* in minutes */ )
{
  SANE_Status status = SANE_STATUS_GOOD;
  Genesys_Register_Set local_reg[6];
  int rate, exposure_time, tgtime, time;

  DBG (DBG_proc, "gl646_set_powersaving (delay = %d)\n", delay);

  local_reg[0].address = 0x01;
  local_reg[0].value = sanei_genesys_read_reg_from_set (dev->reg, 0x01);	/* disable fastmode */

  local_reg[1].address = 0x03;
  local_reg[1].value = sanei_genesys_read_reg_from_set (dev->reg, 0x03);	/* Lamp power control */

  local_reg[2].address = 0x05;
  local_reg[2].value = sanei_genesys_read_reg_from_set (dev->reg, 0x05) & ~REG05_BASESEL;	/* 24 clocks/pixel */

  local_reg[3].address = 0x38;	/* line period low */
  local_reg[3].value = 0x00;

  local_reg[4].address = 0x39;	/* line period high */
  local_reg[4].value = 0x00;

  local_reg[5].address = 0x6c;	/* period times for LPeriod, expR,expG,expB, Z1MODE, Z2MODE */
  local_reg[5].value = 0x00;

  if (!delay)
    local_reg[1].value = local_reg[1].value & 0xf0;	/* disable lampdog and set lamptime = 0 */
  else if (delay < 20)
    local_reg[1].value = (local_reg[1].value & 0xf0) | 0x09;	/* enable lampdog and set lamptime = 1 */
  else
    local_reg[1].value = (local_reg[1].value & 0xf0) | 0x0f;	/* enable lampdog and set lamptime = 7 */

  time = delay * 1000 * 60;	/* -> msec */
  exposure_time =
    (u_int32_t) (time * 32000.0 /
		 (24.0 * 64.0 * (local_reg[1].value & REG03_LAMPTIM) *
		  1024.0) + 0.5);
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

  local_reg[5].value |= tgtime << 6;
  exposure_time /= rate;

  if (exposure_time > 65535)
    exposure_time = 65535;

  local_reg[3].value = exposure_time / 256;	/* highbyte */
  local_reg[4].value = exposure_time & 255;	/* lowbyte */

  status = gl646_bulk_write_register (dev, local_reg, 
				      sizeof (local_reg)/sizeof (local_reg[0]));
  if (status != SANE_STATUS_GOOD)
    DBG (DBG_error,
	 "gl646_set_powersaving: Failed to bulk write registers: %s\n",
	 sane_strstatus (status));

  DBG (DBG_proc, "gl646_set_powersaving: completed\n");
  return status;
}

/* Send the low-level scan command */
/* todo : is this that useful ? */
static SANE_Status
gl646_begin_scan (Genesys_Device * dev, Genesys_Register_Set * reg,
		  SANE_Bool start_motor)
{
  SANE_Status status;
  Genesys_Register_Set local_reg[3];

  DBG (DBG_proc, "gl646_begin_scan\n");

  local_reg[0].address = 0x03;
  local_reg[0].value = sanei_genesys_read_reg_from_set (reg, 0x03);

  local_reg[1].address = 0x01;
  local_reg[1].value = sanei_genesys_read_reg_from_set (reg, 0x01) | REG01_SCAN;	/* set scan bit */

  local_reg[2].address = 0x0f;
  if (start_motor)
    local_reg[2].value = 0x01;
  else
    local_reg[2].value = 0x00;	/* do not start motor yet */

  status = gl646_bulk_write_register (dev, local_reg, 
				      sizeof (local_reg)/sizeof (local_reg[0]));
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl646_begin_scan: Failed to bulk write registers: %s\n",
	   sane_strstatus (status));
      return status;
    }

  DBG (DBG_proc, "gl646_begin_scan: completed\n");

  return status;
}


/* Send the stop scan command */
static SANE_Status
gl646_end_scan (Genesys_Device * dev, Genesys_Register_Set * reg,
		SANE_Bool check_stop)
{
  SANE_Status status;
  int i = 0;
  u_int8_t val;

  DBG (DBG_proc, "gl646_end_scan (check_stop = %d)\n", check_stop);

  status = sanei_genesys_write_register (dev, 0x01, sanei_genesys_read_reg_from_set (reg, 0x01) & ~REG01_SCAN);	/* disable scan */
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "gl646_end_scan: Failed to write registers: %s\n",
	   sane_strstatus (status));
      return status;
    }

  if (check_stop)
    {
      for (i = 0; i < 300; i++)	/* do not wait longer than wait 30 seconds */
	{
	  status = sanei_genesys_get_status (dev, &val);
	  if (status != SANE_STATUS_GOOD)
	    {
	      DBG (DBG_error,
		   "gl646_end_scan: Failed to read register: %s\n",
		   sane_strstatus (status));
	      return status;
	    }

	  if ((!(val & REG41_MOTMFLG)) && (val & REG41_FEEDFSH))
	    {
	      DBG (DBG_proc, "gl646_end_scan: scanfeed finished\n");
	      break;		/* leave while loop */
	    }

	  usleep (10000UL);	/* sleep 100 ms */
	}
    }

  DBG (DBG_proc, "gl646_end_scan: completed (i=%u)\n", i);

  return status;
}

/* Moves the slider to the home (top) postion slowly */
static SANE_Status
gl646_slow_back_home (Genesys_Device * dev, SANE_Bool wait_until_home)
{
  Genesys_Register_Set local_reg[GENESYS_GL646_MAX_REGS + 1];
  SANE_Status status;
  u_int8_t val = 0;
  u_int8_t prepare_steps;
  u_int32_t steps;
  u_int16_t slope_table0[256];
  u_int16_t exposure_time;
  int i, dpi;

  DBG (DBG_proc, "gl646_slow_back_home (wait_until_home = %d)\n",
       wait_until_home);

  memset (local_reg, 0, sizeof (local_reg));
  status = sanei_genesys_get_status (dev, &val);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl646_slow_back_home: Failed to read home sensor: %s\n",
	   sane_strstatus (status));
      return status;
    }

  dev->scanhead_position_in_steps = 0;

  if (val & REG41_HOMESNR)	/* is sensor at home? */
    {
      DBG (DBG_info, "gl646_slow_back_home: already at home, completed\n");
      dev->scanhead_position_in_steps = 0;
      return SANE_STATUS_GOOD;
    }

  /* stop motor if needed */
  if (val & REG41_MOTMFLG)
    {
      status = sanei_genesys_start_motor (dev);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "gl646_slow_back_home: failed to start motor: %s\n",
	       sane_strstatus (status));
	  return SANE_STATUS_IO_ERROR;
	}
      usleep (200000UL);
    }

  memcpy (local_reg, dev->reg, (GENESYS_GL646_MAX_REGS + 1) * sizeof(Genesys_Register_Set));

  prepare_steps = 4;
  exposure_time = 6 * MOTOR_SPEED_MAX;
  steps = 14700 - 2 * prepare_steps;
  if (dev->model->motor_type == MOTOR_HP2300
      || dev->model->motor_type == MOTOR_HP2400)
    {
      steps = 65535;		/* enough to get back home ... */
      dpi = dev->motor.base_ydpi / 4;
      local_reg[reg_0x04].value &= ~REG04_FILTER;
      exposure_time = sanei_genesys_exposure_time (dev, local_reg, dpi);
    }
  else
    dpi = dev->motor.base_ydpi;

  local_reg[reg_0x01].value =
    local_reg[reg_0x01].value & ~REG01_FASTMOD & ~REG01_SCAN;
  local_reg[reg_0x02].value = (local_reg[reg_0x02].value & ~REG02_FASTFED & ~REG02_STEPSEL) | REG02_MTRPWR | REG02_MTRREV;	/* Motor on, direction = reverse */
  if (dev->model->motor_type == MOTOR_HP2300
      || dev->model->motor_type == MOTOR_HP2400)
    local_reg[reg_0x02].value =
      (local_reg[reg_0x02].value & ~REG02_STEPSEL) | REG02_HALFSTEP;

  local_reg[reg_0x21].value = prepare_steps;
  local_reg[reg_0x24].value = prepare_steps;

  local_reg[reg_0x38].value = HIBYTE (exposure_time);
  local_reg[reg_0x39].value = LOBYTE (exposure_time);

  local_reg[reg_0x3d].value = LOBYTE (HIWORD (steps));
  local_reg[reg_0x3e].value = HIBYTE (LOWORD (steps));
  local_reg[reg_0x3f].value = LOBYTE (LOWORD (steps));

  local_reg[reg_0x6c].value = 0x00;	/* one time period (only one speed) */
  if (dev->model->motor_type != MOTOR_HP2300
      && dev->model->motor_type != MOTOR_HP2400)
    {
      local_reg[reg_0x66].value = local_reg[reg_0x66].value | REG66_LOW_CURRENT;	/* gpio7-12: reset GPIO11 (low current) */
      local_reg[reg_0x6d].value = 0x54;
    }

  if (dev->model->motor_type != MOTOR_UMAX)
    {
      sanei_genesys_create_slope_table (dev,
					slope_table0,
					local_reg[reg_0x21].value,
					local_reg[reg_0x02].
					value & REG02_STEPSEL, exposure_time,
					0, dpi, 0);
    }
  else
    {
      sanei_genesys_create_slope_table (dev, slope_table0, prepare_steps, 0,
					exposure_time, 0,
					dev->motor.base_ydpi, /*MOTOR_GEAR */ 
					0);
    }

  /* when scanhead is moving then wait until scanhead stops or timeout */
  DBG (DBG_info, "gl646_slow_back_home: ensuring that motor is off\n");
  for (i = 400; i > 0; i--)	/* do not wait longer than 40 seconds, count down to get i = 0 when busy */
    {
      status = sanei_genesys_get_status (dev, &val);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "gl646_slow_back_home: Failed to read home sensor & motor status: %s\n",
	       sane_strstatus (status));
	  return status;
	}
      if (((val & (REG41_MOTMFLG | REG41_HOMESNR)) == REG41_HOMESNR))	/* at home and motor is off */
	{
	  DBG (DBG_info,
	       "gl646_slow_back_home: already at home and nor moving\n");
	  dev->scanhead_position_in_steps = 0;
	  return SANE_STATUS_GOOD;
	}

      if (!(val & REG41_MOTMFLG))	/* motor is off (todo: was this really wrong?) */
	{
	  DBG (DBG_info,
	       "gl646_slow_back_home: motor is off but scanhead is not home\n");
	  break;		/* motor is off and scanhead is not at home: continue */
	}

      usleep (100 * 1000);	/* sleep 100 ms (todo: fixed to really sleep 100 ms) */
    }

  if (!i)			/* the loop counted down to 0, scanner still is busy */
    {
      DBG (DBG_error,
	   "gl646_slow_back_home: motor is still on: device busy\n");
      return SANE_STATUS_DEVICE_BUSY;
    }

  status =
    gl646_send_slope_table (dev, 0, slope_table0, local_reg[reg_0x21].value);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl646_slow_back_home: Failed to send slope table 0: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status =
    gl646_bulk_write_register (dev, local_reg, GENESYS_GL646_MAX_REGS);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl646_slow_back_home: Failed to bulk write registers: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status = sanei_genesys_start_motor (dev);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "gl646_slow_back_home: Failed to start motor: %s\n",
	   sane_strstatus (status));
      sanei_genesys_stop_motor (dev);
      /* send original registers */
      gl646_bulk_write_register (dev, dev->reg, GENESYS_GL646_MAX_REGS);
      return status;
    }

  if (wait_until_home)
    {
      int loop = 0;

      while (loop < 3)		/* do not wait longer then 30 seconds */
	{
	  status = sanei_genesys_get_status (dev, &val);
	  if (status != SANE_STATUS_GOOD)
	    {
	      DBG (DBG_error,
		   "gl646_slow_back_home: Failed to read home sensor: %s\n",
		   sane_strstatus (status));
	      return status;
	    }

	  if (val & 0x08)	/* home sensor */
	    {
	      DBG (DBG_info, "gl646_slow_back_home: reached home position\n");
	      DBG (DBG_proc, "gl646_slow_back_home: finished\n");
	      return SANE_STATUS_GOOD;
	    }
	  usleep (100000);	/* sleep 100 ms */
	}

      /* when we come here then the scanner needed too much time for this, so we better stop the motor */
      sanei_genesys_stop_motor (dev);
      DBG (DBG_error,
	   "gl646_slow_back_home: timeout while waiting for scanhead to go home\n");
      return SANE_STATUS_IO_ERROR;
    }

  DBG (DBG_info, "gl646_slow_back_home: scanhead is still moving\n");
  DBG (DBG_proc, "gl646_slow_back_home: finished\n");
  return SANE_STATUS_GOOD;
}

/* Moves the slider to the home (top) postion */
/* relies on the settings given for scan,
   but disables scan, puts motor in reverse and
   steps to go before area to 65535 so head will
   go home */
static SANE_Status
gl646_park_head (Genesys_Device * dev, Genesys_Register_Set * reg,
		 SANE_Bool wait_until_home)
{
  Genesys_Register_Set local_reg[9];
  SANE_Status status;
  u_int8_t val = 0;
  int loop = 0;
  int i;
  int exposure_time;

  DBG (DBG_proc, "gl646_park_head (wait_until_home = %d)\n", wait_until_home);
  memset (local_reg, 0, sizeof (local_reg));

  /* read status */
  status = sanei_genesys_get_status (dev, &val);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl646_park_head: failed to read home sensor: %s\n",
	   sane_strstatus (status));
      return status;
    }

  /* no need to park if allready at home */
  if (val & REG41_HOMESNR)
    {
      dev->scanhead_position_in_steps = 0;
      return status;
    }

  /* stop motor if needed */
  if (val & REG41_MOTMFLG)
    {
      status = sanei_genesys_stop_motor (dev);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "gl646_search_par_head: failed to stop motor: %s\n",
	       sane_strstatus (status));
	  return SANE_STATUS_IO_ERROR;
	}
      usleep (200 * 1000);
    }

  /* toggle values to get backward move */
  i = 0;
  /* disable scan */
  local_reg[i].address = 0x01;
  local_reg[i++].value = reg[reg_0x01].value & ~REG01_SCAN;

  /* motor reverse */
  local_reg[i].address = 0x02;
  local_reg[i].value = reg[reg_0x02].value | REG02_MTRREV;

  switch (dev->model->motor_type)
    {
    case MOTOR_HP2300:
      exposure_time = 2000;
      local_reg[i++].value &= ~REG02_FASTFED;
      break;
    case MOTOR_HP2400:
      exposure_time = 675;
      local_reg[i++].value &= ~REG02_FASTFED;
      break;
    case MOTOR_5345:
      exposure_time = 3600;
      local_reg[i++].value |= REG02_FASTFED;
      break;
    default:
      exposure_time = 3600;
      local_reg[i++].value &= ~REG02_FASTFED;
      break;
    }

  /* enough steps (65535) to get home */
  local_reg[i].address = 0x3d;
  local_reg[i++].value = 0x00;
  local_reg[i].address = 0x3e;
  local_reg[i++].value = HIBYTE (65535);
  local_reg[i].address = 0x3f;
  local_reg[i++].value = LOBYTE (65535);

  /* set line period */
  local_reg[i].address = 0x38;
  local_reg[i++].value = HIBYTE (exposure_time);
  local_reg[i].address = 0x39;
  local_reg[i++].value = LOBYTE (exposure_time);

  /* writes register */
  status = gl646_bulk_write_register (dev, local_reg, i);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl646_park_head: failed to bulk write registers: %s\n",
	   sane_strstatus (status));
      return status;
    }

  /* sends slope table 1 (move before scan area) */
  status = gl646_send_slope_table (dev, 1, dev->slope_table1,
				   reg[reg_0x6b].value);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl646_park_head: failed to send slope table 1: %s\n",
	   sane_strstatus (status));
      return status;
    }

  /* start motor */
  status = sanei_genesys_start_motor (dev);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "gl646_park_head: failed to start motor: %s\n",
	   sane_strstatus (status));
      sanei_genesys_stop_motor (dev);
      /* restore original registers */
      gl646_bulk_write_register (dev, reg, GENESYS_GL646_MAX_REGS);
      return status;
    }

  /* wait for head to park if needed */
  if (wait_until_home)
    {
      /* no more than 300 loops of 100 ms each -> 30 second time out */
      while (loop < 300)
	{
	  status = sanei_genesys_get_status (dev, &val);
	  if (status != SANE_STATUS_GOOD)
	    {
	      DBG (DBG_error,
		   "gl646_park_head: failed to read home sensor: %s\n",
		   sane_strstatus (status));
	      return status;
	    }

	  /* test home sensor */
	  if (val & REG41_HOMESNR)
	    {
	      DBG (DBG_info, "gl646_park_head: reached home position\n");
	      DBG (DBG_proc, "gl646_park_head: finished\n");
	      dev->scanhead_position_in_steps = 0;
	      return SANE_STATUS_GOOD;
	    }
	  /* hack around a bug ? */
	  if (!(val & REG41_MOTMFLG))
	    {
	      DBG (DBG_info, "gl646_park_head: restarting motor\n");
	      status = sanei_genesys_start_motor (dev);
	      if (status != SANE_STATUS_GOOD)
		{
		  DBG (DBG_error,
		       "gl646_park_head: failed to restart motor: %s\n",
		       sane_strstatus (status));
		}
	    }
	  usleep (100000);
	}
    }

  /* when we come here then the scanner needed too much time for this,
     so we better stop the motor */
  sanei_genesys_start_motor (dev);
  DBG (DBG_error,
       "gl646_park_head: timeout while waiting for scanhead to go home\n");
  return SANE_STATUS_IO_ERROR;
}


/* Automatically set top-left edge of the scan area by scanning a 200x200 pixels
   area at 600 dpi from very top of scanner */
static SANE_Status
gl646_search_start_position (Genesys_Device * dev)
{
  int size;
  SANE_Status status;
  u_int8_t *data;
  u_int16_t slope_table0[256];
  Genesys_Register_Set local_reg[GENESYS_GL646_MAX_REGS + 1];
  int i, steps;
  int exposure_time, half_ccd = 0;
  struct timeval tv;

  int pixels = 600, depth, words;
  int dpi = 300;
  int start_pixel, end_pixel;

  DBG (DBG_proc, "gl646_search_start_position\n");
  memset (local_reg, 0, sizeof (local_reg));
  memcpy (local_reg, dev->reg, (GENESYS_GL646_MAX_REGS + 1) * sizeof(Genesys_Register_Set));

  gettimeofday (&tv, NULL);
  /* is scanner warm enough ? */
  /* we have 2 options there, either wait, or return SANE_STATUS_DEVICE_BUSY */
  /* we'd rather wait ... */
  if ((dev->model->flags & GENESYS_FLAG_MUST_WAIT)
      && (tv.tv_sec - dev->init_date < 60))
    {
      DBG (DBG_proc,
	   "gl646_search_start_position: waiting for scanner to be ready\n");
      usleep (1000 * 1000 * (tv.tv_sec - dev->init_date));
    }

  /* sets for a 200 lines * 600 pixels */
  /* normal scan with no shading */
  local_reg[reg_0x01].value =
    (local_reg[reg_0x01].
     value & (REG01_CISSET | REG01_DRAMSEL | REG01_DOGENB)) | (REG01_SCAN |
							       REG01_DOGENB);
  if (dev->model->ccd_type == CCD_HP2300
      || dev->model->ccd_type == CCD_HP2400)
    {
      local_reg[reg_0x02].value = 0x30;
      half_ccd = 0;
    }
  else
    {
      local_reg[reg_0x02].value = 0xd0;	/* auto-go-home disabled, disable moving when buffer full, auto-go-home after scan disabled, turn on MOTOR power and phase, one table motor moving, motor forward */
      half_ccd = 1;
    }
  local_reg[reg_0x02].value =
    (local_reg[reg_0x02].value | REG02_HALFSTEP) & ~REG02_AGOHOME;

  /* we are doing a monochrome scan */
  local_reg[reg_0x04].value = 0x57;	/* color lineart, 16 bits data, frontend type 16 bits, scan color type R, frontend B  */
  gl646_setup_sensor (dev, local_reg, 1, half_ccd);

  if (dev->model->ccd_type == CCD_HP2300
      || dev->model->ccd_type == CCD_HP2400)
    {
      local_reg[reg_0x04].value &= ~(REG04_BITSET | REG04_FILTER);
      local_reg[reg_0x04].value |= 0x08;	/* green filter */
      local_reg[reg_0x05].value |= REG05_GMMENB;
      depth = 8;
      steps = 18;
    }
  else
    {
      local_reg[reg_0x05].value &= ~REG05_GMMENB;
      depth = 16;
      /*local_reg[reg_0x1f].value = 0x01; */
      steps = 1;
    }

  /* no dummy lines */
  local_reg[reg_0x1e].value &= 0xf0;
  if (dev->model->ccd_type == CCD_HP2300
      || dev->model->ccd_type == CCD_HP2400)
    local_reg[reg_0x1e].value = 0x40;

  /* step move */
  local_reg[reg_0x3d].value = LOBYTE (HIWORD (steps));
  local_reg[reg_0x3e].value = HIBYTE (LOWORD (steps));
  local_reg[reg_0x3f].value = LOBYTE (LOWORD (steps));

  /* reset GPO */
  local_reg[reg_0x66].value = dev->gpo.value[0];
  local_reg[reg_0x67].value = dev->gpo.value[1];
  local_reg[reg_0x68].value = dev->gpo.enable[0];
  local_reg[reg_0x69].value = dev->gpo.enable[1];

  exposure_time = sanei_genesys_exposure_time (dev, local_reg, dpi);
  if (dev->model->motor_type == MOTOR_5345)
    gl646_setup_steps (dev, local_reg, dpi);
  else
    {
      local_reg[reg_0x21].value = 3;
      local_reg[reg_0x22].value = 16;
      local_reg[reg_0x23].value = 16;
      local_reg[reg_0x24].value = 3;
    }

  /* number of scan lines */
  local_reg[reg_0x25].value = LOBYTE (HIWORD (dev->model->search_lines));
  local_reg[reg_0x26].value = HIBYTE (LOWORD (dev->model->search_lines));
  local_reg[reg_0x27].value = LOBYTE (LOWORD (dev->model->search_lines));

  /* DPISET is at CCD max, word count gives the ratio, and the real dpi */
  local_reg[reg_0x2c].value = HIBYTE (dev->sensor.optical_res);
  local_reg[reg_0x2d].value = LOBYTE (dev->sensor.optical_res);

  /* start pixel */
  start_pixel = dev->sensor.dummy_pixel;
  if (dev->model->ccd_type == CCD_HP2300)
    start_pixel += 64;
  local_reg[reg_0x30].value = HIBYTE (start_pixel);
  local_reg[reg_0x31].value = LOBYTE (start_pixel);


  /* end CCD pixel */
  end_pixel = start_pixel + pixels;
  local_reg[reg_0x32].value = HIBYTE (end_pixel);
  local_reg[reg_0x33].value = LOBYTE (end_pixel);

  words = (pixels * depth) / 8;
  local_reg[reg_0x35].value = LOBYTE (HIWORD (words));
  local_reg[reg_0x36].value = HIBYTE (LOWORD (words));	/*  */
  local_reg[reg_0x37].value = LOBYTE (LOWORD (words));	/* maximum word size per line=1200  */

  local_reg[reg_0x38].value = HIBYTE (exposure_time);	/* half default period of 11000 */
  local_reg[reg_0x39].value = LOBYTE (exposure_time);	/* CCD line period set to 5500  */

  /* Z1MOD = Z2MOD = 0 */
  local_reg[reg_0x60].value = LOBYTE (0);
  local_reg[reg_0x61].value = HIBYTE (0);
  local_reg[reg_0x62].value = LOBYTE (0);
  local_reg[reg_0x63].value = HIBYTE (0);

  if (dev->model->motor_type != MOTOR_HP2300
      && dev->model->motor_type != MOTOR_HP2400)
    local_reg[reg_0x65].value = 0x00;	/* PWM duty cycle for table one motor phase (63 = max) */
  else
    {
      local_reg[reg_0x65].value = 0x3f;
      local_reg[reg_0x6a].value = 0x7f;	/* table two fast moving step type, PWM duty for table two */
      local_reg[reg_0x6d].value = 0x7f;	/* select deceleration steps whenever go home (0), accel/decel stop time (31 * LPeriod) */
    }

  /* send to scanner */
  status =
    gl646_bulk_write_register (dev, local_reg, GENESYS_GL646_MAX_REGS);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl646_search_start_position: failed to bulk write registers: %s\n",
	   sane_strstatus (status));
      return status;
    }

  /* create local slope tables */
  if (dev->model->motor_type == MOTOR_5345)
    sanei_genesys_create_slope_table (dev,
				      slope_table0,
				      local_reg[reg_0x21].value,
				      local_reg[reg_0x02].
				      value & REG02_STEPSEL, exposure_time, 0,
				      dpi, 0);
  else
    sanei_genesys_create_slope_table (dev,
				      slope_table0,
				      local_reg[reg_0x21].value,
				      local_reg[reg_0x02].
				      value & REG02_STEPSEL, exposure_time, 0,
				      dev->motor.base_ydpi, 0);

  status =
    gl646_send_slope_table (dev, 0, slope_table0, local_reg[reg_0x21].value);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl646_search_start_position: failed to send slope table 0: %s\n",
	   sane_strstatus (status));
      return status;
    }

  size = words * (dev->model->search_lines + 1);

  data = malloc (size);
  if (!data)
    {
      DBG (DBG_error,
	   "gl646_search_start_position: failed to allocate memory\n");
      return SANE_STATUS_NO_MEM;
    }

  status = gl646_set_fe (dev, AFE_INIT);
  if (status != SANE_STATUS_GOOD)
    {
      free (data);
      DBG (DBG_error,
	   "gl646_search_start_position: failed to set frontend: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status = gl646_begin_scan (dev, local_reg, SANE_TRUE);
  if (status != SANE_STATUS_GOOD)
    {
      free (data);
      DBG (DBG_error,
	   "gl646_search_start_position: failed to begin scan: %s\n",
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
	   "gl646_search_start_position: failed to read data: %s\n",
	   sane_strstatus (status));
      return status;
    }

  /* start position search is done on 8 bit data */
  if (depth == 16)
    {
      if (DBG_LEVEL >= DBG_data)
	sanei_genesys_write_pnm_file ("search_position16.pnm", data, depth, 1,
				      pixels, dev->model->search_lines);
      size /= 2;
      for (i = 0; i < size; i++)
	data[i] = data[i * 2 + 1];
    }

  /* x resolution may be the double of y resolution */
  /* after that we have a 300 dpi gray level picture */
  if (dev->sensor.optical_res > 2 * dpi)
    {
      size /= 2;
      for (i = 0; i < size; i++)
	data[i] = (data[i * 2] + data[i * 2 + 1]) / 2;
      pixels /= 2;
    }

  if (DBG_LEVEL >= DBG_data)
    sanei_genesys_write_pnm_file ("search_position.pnm", data, 8, 1, pixels,
				  dev->model->search_lines);

  status = gl646_end_scan (dev, local_reg, SANE_FALSE);
  if (status != SANE_STATUS_GOOD)
    {
      free (data);
      DBG (DBG_error,
	   "gl646_search_start_position: failed to end scan: %s\n",
	   sane_strstatus (status));
      return status;
    }

  /* update regs to copy ASIC internal state */
  dev->reg[reg_0x01].value = local_reg[reg_0x01].value;
  dev->reg[reg_0x02].value = local_reg[reg_0x02].value;
  memcpy (dev->reg, local_reg, (GENESYS_GL646_MAX_REGS + 1) * sizeof(Genesys_Register_Set));

  status =
    sanei_genesys_search_reference_point (dev, data, start_pixel, dpi, pixels,
					  dev->model->search_lines);
  if (status != SANE_STATUS_GOOD)
    {
      free (data);
      DBG (DBG_error,
	   "gl646_search_start_position: failed to set search reference point: %s\n",
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
gl646_init_regs_for_coarse_calibration (Genesys_Device * dev)
{
  SANE_Status status;
  u_int32_t bytes_per_line;
  u_int32_t words_per_line;
  u_int32_t steps_sum;
  u_int32_t z1, z2;
  u_int16_t slope_table[256];
  u_int16_t strpixel;
  u_int16_t endpixel;
  u_int8_t channels;
  u_int8_t cksel;

  DBG (DBG_proc, "gl646_init_regs_for_coarse_calibration\n");

/* ST12:
	0x01 0x00 0x02 0x71 0x03 0x1f 0x04 0x57 0x05 0x10 0x06 0x18 0x08 0x02 0x09 0x00 
	0x0a 0x06 0x0b 0x04 
	0x10 0x00 0x11 0x00 0x12 0x00 0x13 0x00 0x14 0x00 0x15 0x00 0x16 0x2b 0x17 0x08 0x18 0x20 0x19 0x2a 
	0x1c 0xc0 0x1d 0x03 */

  /* shading off, compression off */
  dev->calib_reg[reg_0x01].value = REG01_SCAN;
  /* disable forward/backward+autogohome+fastfed+motor */
  dev->calib_reg[reg_0x02].value =
    (dev->calib_reg[reg_0x02].
     value & ~REG02_MTRPWR & ~REG02_AGOHOME & ~REG02_FASTFED & ~REG02_STEPSEL)
    | REG02_ACDCDIS;
  if (dev->model->motor_type == MOTOR_5345)
    dev->calib_reg[reg_0x02].value |= REG02_QUATERSTEP;

  /* disable lineart, enable 16bit */
  dev->calib_reg[reg_0x04].value =
    (dev->calib_reg[reg_0x04].value & ~REG04_LINEART) | REG04_BITSET;
  dev->calib_reg[reg_0x05].value = (dev->calib_reg[reg_0x05].value & ~REG05_GMMENB);	/* disable gamma */

/* ST12:
	0x21 0x20 0x22 0x10 0x23 0x10 0x24 0x20 0x25 0x00 0x26 0x00 0x27 0x64 
	0x32 0x02 0x33 0x66 0x35 0x00 0x36 0x04 0x37 0xb0 0x38 0x27 0x39 0x10 
	0x52 0x0f 0x53 0x13 0x54 0x17 0x55 0x03 0x56 0x07 0x57 0x0b 0x58 0x83 
	0x65 0x3f 0x6b 0xff */

  dev->calib_reg[reg_0x21].value = 0x20 /*0x01 */ ;
  dev->calib_reg[reg_0x22].value = 0x10 /*0x00 */ ;
  dev->calib_reg[reg_0x23].value = 0x10 /*0x01 */ ;
  dev->calib_reg[reg_0x24].value = 0x20 /*0x00 */ ;
  dev->calib_reg[reg_0x25].value = 0x00;
  dev->calib_reg[reg_0x26].value = 0x00;
  dev->calib_reg[reg_0x27].value = 0x64 /*0x01 */ ;
  dev->calib_reg[reg_0x3d].value = 0x00;
  dev->calib_reg[reg_0x3e].value = 0x00;
  dev->calib_reg[reg_0x3f].value = 0x00;
  /* dev->calib_reg[reg_0x6b].value = 0xff; done in init *//*0x01 */

  cksel = (dev->calib_reg[reg_0x18].value & REG18_CKSEL) + 1;	/* clock speed = 1..4 clocks */

  /* start position */
  strpixel = dev->sensor.dummy_pixel;
  dev->calib_reg[reg_0x30].value = HIBYTE (strpixel);
  dev->calib_reg[reg_0x31].value = LOBYTE (strpixel);

  /* set right position todo: optical resolution, not real x resolution? */
  endpixel = strpixel + (dev->sensor.optical_res / cksel);

  dev->calib_reg[reg_0x32].value = HIBYTE (endpixel);
  dev->calib_reg[reg_0x33].value = LOBYTE (endpixel);
  DBG (DBG_info,
       "gl646_init_register_for_coarse_calibration: left pos: %d CCD pixels, right pos: %d CCD pixels\n",
       strpixel, endpixel);

  /* set dummy pixels */
  /* todo: dummy pixels + tgw + 2* tgs according spec? */
  dev->calib_reg[reg_0x34].value = dev->sensor.dummy_pixel;
  DBG (DBG_info,
       "gl646_init_register_for_coarse_calibration: dummy pixels: %d CCD pixels\n",
       dev->sensor.dummy_pixel);

  DBG (DBG_info,
       "gl646_init_register_for_coarse_calibration: optical sensor res: %d dpi, actual res: %d\n",
       dev->sensor.optical_res / cksel, dev->settings.xres);

  /* set line size */
  if (dev->settings.scan_mode == 4)	/* single pass color */
    channels = 3;
  else
    channels = 1;

  bytes_per_line = channels * 2 * (endpixel - strpixel);

  words_per_line = (bytes_per_line + 1) / 2 + 1;

  dev->calib_reg[reg_0x35].value = LOBYTE (HIWORD (words_per_line));
  dev->calib_reg[reg_0x36].value = HIBYTE (LOWORD (words_per_line));
  dev->calib_reg[reg_0x37].value = LOBYTE (LOWORD (words_per_line));

  DBG (DBG_info,
       "gl646_init_register_for_coarse_calibration: bytes_per_line=%d, words_per_line=%d\n",
       bytes_per_line, words_per_line);

  /* todo: only 2 steps? */

  steps_sum =
    sanei_genesys_create_slope_table (dev, slope_table,
				      dev->calib_reg[reg_0x21].value,
				      dev->calib_reg[reg_0x02].
				      value & REG02_STEPSEL,
				      dev->settings.exposure_time, 0,
				      dev->motor.base_ydpi, /*MOTOR_GEAR */ 
				      0);

  /* todo: z1 = z2 = 0? */
  sanei_genesys_calculate_zmode (dev,
				 dev->settings.exposure_time,
				 steps_sum,
				 slope_table[dev->calib_reg[reg_0x21].value -
					     1],
				 dev->calib_reg[reg_0x3d].value * 65536 +
				 dev->calib_reg[reg_0x3e].value * 256 +
				 dev->calib_reg[reg_0x3f].value,
				 dev->calib_reg[reg_0x02].
				 value & REG02_FASTFED,
				 dev->calib_reg[reg_0x1f].value,
				 dev->calib_reg[reg_0x22].value,
				 (dev->calib_reg[reg_0x6c].
				  value & REG6C_TGTIME) >> 6, &z1, &z2);
  DBG (DBG_info, "gl646_init_register_for_coarse_calibration: z1 =  %d\n",
       z1);
  DBG (DBG_info, "gl646_init_register_for_coarse_calibration: z2 =  %d\n",
       z2);
  dev->calib_reg[reg_0x60].value = HIBYTE (z1);
  dev->calib_reg[reg_0x61].value = LOBYTE (z1);
  dev->calib_reg[reg_0x62].value = HIBYTE (z2);
  dev->calib_reg[reg_0x63].value = LOBYTE (z2);
  dev->calib_reg[reg_0x6c].value = (dev->calib_reg[reg_0x6c].value & REG6C_TGTIME) | ((z1 >> 13) & 0x38) | ((z2 >> 16) & 0x07);	/* todo: double check */

  status =
    gl646_send_slope_table (dev, 0, slope_table,
			    dev->calib_reg[reg_0x21].value);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl646_init_register_for_coarse_calibration: Failed to send slope table: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status =
    gl646_bulk_write_register (dev, dev->calib_reg,
			       GENESYS_GL646_MAX_REGS);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl646_init_register_for_coarse_calibration: Failed to bulk write registers: %s\n",
	   sane_strstatus (status));
      return status;
    }

  DBG (DBG_proc, "gl646_init_register_for_coarse_calibration: completed\n");

  return SANE_STATUS_GOOD;
}


/* init registers for shading calibration */
static SANE_Status
gl646_init_regs_for_shading (Genesys_Device * dev)
{
  SANE_Status status;
  u_int32_t bytes_per_line;
  u_int32_t num_pixels;
  u_int32_t steps_sum;
  int move = 0;
  int exposure_time;
  u_int32_t z1, z2;
  u_int16_t slope_table[256];
  u_int16_t steps;
  u_int8_t step_parts;
  u_int8_t channels;
  u_int8_t dummy_lines = 3;
  int dpiset;
  SANE_Bool half_ccd;
  int workaround;

  DBG (DBG_proc, "gl646_init_register_for_shading: lines = %d\n",
       dev->model->shading_lines);

  if (dev->settings.scan_mode == 4)	/* single pass color */
    channels = 3;
  else
    channels = 1;

  /* monochrome / color scan */
  dev->calib_reg[reg_0x04].value &= ~REG04_FILTER;
  if (channels == 1)
    dev->calib_reg[reg_0x04].value |= 0x08;

  /* disable forward/backward+motor, stepsel = 1 */
  dev->calib_reg[reg_0x02].value =
    (dev->calib_reg[reg_0x02].
     value & ~REG02_MTRPWR & ~REG02_STEPSEL & ~REG02_AGOHOME) | REG02_ACDCDIS
    | 0x01;

  /* dpi=600 for scan at 600 dpi and below, and dpi=1200 when above */
  dpiset = dev->sensor.optical_res;

  if (dev->settings.xres <= dev->sensor.optical_res / 2)
    {
      num_pixels = dev->sensor.sensor_pixels / 2;
      half_ccd = 1;
    }
  else
    {
      num_pixels = dev->sensor.sensor_pixels;
      half_ccd = 0;
    }
  exposure_time =
    sanei_genesys_exposure_time (dev, dev->calib_reg, dev->settings.yres);
  DBG (DBG_info,
       "gl646_init_regs_for_shading: half_ccd = %d, yres = %d\n",
       half_ccd, dev->settings.yres);

  if (dev->model->motor_type == MOTOR_5345)
    {
      dev->calib_reg[reg_0x02].value =
	REG02_ACDCDIS | REG02_MTRPWR | REG02_QUATERSTEP;
      dev->calib_reg[reg_0x03].value |= REG03_AVEENB;
      dev->calib_reg[reg_0x04].value |= REG04_BITSET;
      dev->calib_reg[reg_0x05].value &= ~REG05_GMMENB;
    }

  /* full, half or quarter step, 0x11 is reserved! (--> 1 << 1 = 0x02) */
  step_parts = (1 << (dev->calib_reg[reg_0x02].value & REG02_STEPSEL));
  steps = step_parts * 0x40;	/* base = 64 lines --> 0x80 */
  if (steps > 255)
    steps = 255;

  if (dev->model->motor_type == MOTOR_5345)
    gl646_setup_steps (dev, dev->calib_reg, dpiset);
  else if (dev->model->motor_type == MOTOR_HP2300
	   || dev->model->motor_type == MOTOR_HP2400)
    {
      dev->calib_reg[reg_0x21].value = 2;
      dev->calib_reg[reg_0x22].value = 16;
    }
  else
    {
      dev->calib_reg[reg_0x21].value = (u_int8_t) steps;
      steps = 2 * step_parts;
      if (steps > 255)
	steps = 255;

      dev->calib_reg[reg_0x22].value = (u_int8_t) steps;
    }
  dev->calib_reg[reg_0x23].value = dev->calib_reg[reg_0x22].value;
  dev->calib_reg[reg_0x24].value = dev->calib_reg[reg_0x21].value;


  steps = dev->model->y_offset * step_parts;
  if (dev->model->motor_type == MOTOR_5345)
    steps = 0;
  else if (dev->model->motor_type == MOTOR_HP2300
	   || dev->model->motor_type == MOTOR_HP2400)
    steps = 1;
  else
    dev->calib_reg[reg_0x6b].value = 0x20 * step_parts;

  bytes_per_line =
    (channels * 2 * num_pixels * dpiset) / dev->sensor.optical_res;

  dev->calib_reg[reg_0x25].value =
    LOBYTE (HIWORD (dev->model->shading_lines));
  dev->calib_reg[reg_0x26].value =
    HIBYTE (LOWORD (dev->model->shading_lines));
  dev->calib_reg[reg_0x27].value =
    LOBYTE (LOWORD (dev->model->shading_lines));

  dev->calib_reg[reg_0x2c].value = HIBYTE (dpiset);
  dev->calib_reg[reg_0x2d].value = LOBYTE (dpiset);

  dev->calib_reg[reg_0x30].value = HIBYTE (dev->sensor.dummy_pixel);
  dev->calib_reg[reg_0x31].value = LOBYTE (dev->sensor.dummy_pixel);
  dev->calib_reg[reg_0x32].value =
    HIBYTE (dev->sensor.dummy_pixel + num_pixels);
  dev->calib_reg[reg_0x33].value =
    LOBYTE (dev->sensor.dummy_pixel + num_pixels);

  dev->calib_reg[reg_0x35].value = LOBYTE (HIWORD (bytes_per_line));
  dev->calib_reg[reg_0x36].value = HIBYTE (LOWORD (bytes_per_line));
  dev->calib_reg[reg_0x37].value = LOBYTE (LOWORD (bytes_per_line));

  dev->calib_reg[reg_0x38].value = HIBYTE (exposure_time);
  dev->calib_reg[reg_0x39].value = LOBYTE (exposure_time);

  dev->calib_reg[reg_0x3d].value = LOBYTE (HIWORD (steps));
  dev->calib_reg[reg_0x3e].value = HIBYTE (LOWORD (steps));
  dev->calib_reg[reg_0x3f].value = LOBYTE (LOWORD (steps));

  gl646_setup_sensor (dev, dev->calib_reg, 1, half_ccd);

  z1 = 0;
  z2 = 0;
  if (dev->model->motor_type != MOTOR_5345)
    {
      dpiset /= (1 + half_ccd);
      steps_sum = sanei_genesys_create_slope_table (dev,
						    slope_table,
						    dev->calib_reg[reg_0x21].
						    value,
						    dev->calib_reg[reg_0x02].
						    value & REG02_STEPSEL,
						    sanei_genesys_exposure_time
						    (dev, dev->reg, dpiset),
						    0, dpiset, 0);
      move = (dev->model->shading_lines * dev->motor.base_ydpi) / dpiset;
      DBG (DBG_info,
	   "gl646_init_regs_for_shading: computed move = %d, dpiset = %d\n",
	   move, dpiset);
      /* todo: sort this out, there's on offset somewhere ... */
      /* I really don't get what's going on here ... */
      /* Or move computing is wrong when doing reg init for final scan */
      if (dev->settings.yres == 300)
	move = 115;
      else if (dev->settings.yres == 150)
	move = 0;
      else if (dev->settings.yres == 75)
	move = 60;
      DBG (DBG_info,
	   "gl646_init_regs_for_shading: overrided move = %d, yres = %d\n",
	   move, dev->settings.yres);
    }
  else
    {
      dev->calib_reg[reg_0x21].value = 1;
      dev->calib_reg[reg_0x6b].value = 255;

      /* seems to be 2 if dpi <= 150, but is it useful ? */
      if (dev->settings.yres <= 150)
	dev->calib_reg[reg_0x65].value = 2;
      else
	dev->calib_reg[reg_0x65].value = 0;

      /* specific slope-table for shading */
      /* it appears win200 and winMe aren't using the same speeds */
      /* consumer / pro settings ? */
      /* this is currently real magic to me */
      /* todo: find out the real rule */
      /* quarter steps for every resolution */
      if (dev->settings.yres <= 150)	/* 11000 exposure time  */
	{
	  slope_table[0] = 3471;
	  dummy_lines = 2;
	  workaround = -390;
	}
      else if (dev->settings.yres <= 300)	/* 5500 exposure time  */
	{
	  slope_table[0] = 4400;
	  dummy_lines = 7;
	  workaround = -260;
	}
      else if (dev->settings.yres <= 600)	/* 11000 exposure time  */
	{
	  slope_table[0] = 4400;
	  if (channels > 1)
	    dummy_lines = 3;
	  else
	    dummy_lines = 1;
	  workaround = -600;
	}
      else if (dev->settings.yres <= 800)	/* 11000 exposure time  */
	{
	  slope_table[0] = 9258;
	  dummy_lines = 7;
	  workaround = -600;
	}
      else
	{
	  slope_table[0] = 8378;
	  dummy_lines = 7;
	  workaround = -600;
	}

      /* computes motor steps done during shading calibration 
         dpi = slope_table[0] * dev->motor.base_ydpi * step_parts / (exposure_time * (1+dummy_lines));
         move = (lines / dpi) * dev->motor.optical_ydpi */
      move =
	(dev->model->shading_lines * exposure_time * (1 + dummy_lines) * 2) /
	(step_parts * slope_table[0]);

      /* todo: find a way to remove this workaround */
      move -= (dev->model->shading_lines * workaround) / 32;

      /* rest of table */
      for (steps = 1; steps < 255; steps++)
	slope_table[steps] = slope_table[0];

      /* set all available bits */
      if (dev->model->motor_type == MOTOR_5345)
	{
	  dev->calib_reg[reg_0x66].value = 0x30;
	  dev->calib_reg[reg_0x67].value = dev->gpo.enable[1];
	}
    }

  DBG (DBG_info, "gl646_init_regs_for_shading: move = %d\n", move);
  dev->calib_reg[reg_0x60].value = HIBYTE (z1);
  dev->calib_reg[reg_0x61].value = LOBYTE (z1);
  dev->calib_reg[reg_0x62].value = HIBYTE (z2);
  dev->calib_reg[reg_0x63].value = LOBYTE (z2);

  /* linesel = 3 */
  dev->calib_reg[reg_0x1e].value =
    (dev->calib_reg[reg_0x1e].value & ~REG1E_LINESEL) | dummy_lines;

  /* dark calibration move : motor if off ..., so we don't move */
  /* if (dev->model->flags & GENESYS_FLAG_DARK_CALIBRATION)
     dev->scanhead_position_in_steps += move; */

  /* white shading */
  dev->scanhead_position_in_steps += move;

  status =
    gl646_send_slope_table (dev, 0, slope_table,
			    dev->calib_reg[reg_0x21].value);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl646_init_regs_for_shading: Failed to send slope table: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status =
    gl646_bulk_write_register (dev, dev->calib_reg,
			       GENESYS_GL646_MAX_REGS);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl646_init_regs_for_shading: Failed to bulk write registers: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status = gl646_set_fe (dev, AFE_SET);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl646_init_regs_for_shading: Failed to set frontend: %s\n",
	   sane_strstatus (status));
      return status;
    }

  DBG (DBG_proc, "gl646_init_register_for_shading: completed\n");

  return SANE_STATUS_GOOD;
}

/* set up registers for the actual scan
 * todo: once we clearly figure out how to set up
 * registers for every scan parameters, use
 * it to improve register settings in calibration
 * functions
 */
static SANE_Status
gl646_init_regs_for_scan (Genesys_Device * dev)
{
  SANE_Bool same_speed = 0;
  int dpiset;
  int start, end, pixels;
  int channels;
  int words_per_line;
  int move, lincnt;
  int exposure_time;
  int i, depth;
  int slope_dpi = 0;
  int move_dpi = 0;
  int fast_dpi = 0;
  int fast_exposure = 0;
  int dummy;
  u_int32_t steps_sum, z1, z2;
  SANE_Bool half_ccd;		/* false: full CCD res is used, true, half max CCD res is used */
  SANE_Status status;
  unsigned int stagger;
  unsigned int max_shift;
  float read_factor;
  size_t requested_buffer_size;
  size_t read_buffer_size;

  DBG (DBG_info,
       "gl646_init_regs_for_scan settings:\nResolution: %ux%uDPI\n"
       "Lines     : %u\nPPL       : %u\nStartpos  : %.3f/%.3f\nScan mode : %d\n\n",
       dev->settings.xres, dev->settings.yres, dev->settings.lines,
       dev->settings.pixels, dev->settings.tl_x, dev->settings.tl_y,
       dev->settings.scan_mode);

  /* we have 2 domains for ccd: yres below or above half ccd max dpi */
  /* for some reason, windows sets DPISET to twice the needed value,
     we're doing the same for now, but maybe we could avoid this, which
     would avoid cropping data in reads */
  half_ccd = SANE_TRUE;
  dpiset = dev->sensor.optical_res;
  i = dev->sensor.optical_res / dev->settings.xres;
  if (i <= 1)
    half_ccd = SANE_FALSE;
  if (i <= 3)
    dpiset = dev->sensor.optical_res;
  else if (i <= 5)
    dpiset = dev->sensor.optical_res / 2;
  else if (i <= 7)
    dpiset = dev->sensor.optical_res / 3;
  else if (i <= 11)
    dpiset = dev->sensor.optical_res / 4;
  else if (i <= 15)
    dpiset = dev->sensor.optical_res / 6;
  else if (i <= 23)
    dpiset = dev->sensor.optical_res / 8;
  else if (i <= 24)
    dpiset = dev->sensor.optical_res / 12;
  DBG (DBG_info,
       "gl646_init_regs_for_scan : i=%d, dpiset=%d, half_ccd=%d\n", i,
       dpiset, half_ccd);

  /* sigh ... MD6471 motor's provide vertically shifted columns at hi res ... */
  if ((!half_ccd) && (dev->model->flags & GENESYS_FLAG_STAGGERED_LINE))
    stagger = (4 * dev->settings.yres) / dev->motor.base_ydpi;
  else
    stagger = 0;
  DBG (DBG_info, "gl646_init_regs_for_scan : stagger=%d lines\n", stagger);

  /* compute scan parameters values */
  /* pixels are allways given at half or full CCD optical resolution */
  /* use detected left margin  and fixed value */
  start =
    dev->sensor.CCD_start_xoffset +
    (SANE_UNFIX (dev->model->x_offset) * dev->sensor.optical_res) /
    MM_PER_INCH;

  /* add x coordinates */
  start += (dev->settings.tl_x * dev->sensor.optical_res) / MM_PER_INCH;
  if (half_ccd)
    start = start / 2;
  if (stagger > 0)
    start = start | 1;

  /* compute correct pixels number */
  read_factor = 0.0;
  pixels =
    (dev->settings.pixels * dev->sensor.optical_res) / dev->settings.xres;
  if (half_ccd)
    pixels = pixels / 2;

  /* round up pixels number if needed */
  if (dpiset != 2 * dev->settings.xres
      && dev->settings.xres < dev->sensor.optical_res)
    {
      i = dev->sensor.optical_res / dpiset;
      pixels = ((pixels + i - 1) / i) * i;
      /* sets flag for reading */
      /* the 2 factor is due to the fact that dpiset = 2 desired ccd dpi */
      read_factor = (float) pixels / (float) (dev->settings.pixels * 2);
      DBG (DBG_info,
	   "gl646_init_regs_for_scan : rounding up pixels, factor=%f\n",
	   read_factor);
    }

  end = start + pixels;

  /* set line size */
  if (dev->settings.scan_mode == 4)	/* single pass color */
    channels = 3;
  else
    channels = 1;

  /* scanned area must be enlarged by max color shift needed */
  /* all values are assumed >= 0 */
  if (channels > 1)
    {
      max_shift = dev->model->ld_shift_r;
      if ((unsigned int) dev->model->ld_shift_b > max_shift)
	max_shift = dev->model->ld_shift_b;
      if ((unsigned int) dev->model->ld_shift_g > max_shift)
	max_shift = dev->model->ld_shift_g;
      max_shift = (max_shift * dev->settings.yres) / dev->motor.base_ydpi;
    }
  else
    {
      max_shift = 0;
    }

  /* enable shading */
  dev->reg[reg_0x01].value |= REG01_DVDSET;
  dev->reg[reg_0x01].value &= ~REG01_FASTMOD;

  dev->reg[reg_0x02].value = REG02_NOTHOME | REG02_MTRPWR;

  /* if dpi is low enough, we don't need to use 2 tables moving */
  if (dev->settings.yres > dev->sensor.optical_res / 4)
    dev->reg[reg_0x02].value |= REG02_FASTFED;

  /* motor speed */
  dev->reg[reg_0x02].value &= ~REG02_STEPSEL;
  if (dev->model->motor_type == MOTOR_5345)
    {
      if (half_ccd)
	dev->reg[reg_0x02].value |= REG02_HALFSTEP;
      else
	dev->reg[reg_0x02].value |= REG02_QUATERSTEP;
    }
  else
    dev->reg[reg_0x02].value |= REG02_FULLSTEP;

  /* average looks better than deletion */
  dev->reg[reg_0x03].value |= REG03_AVEENB;

  /* monochrome / color scan */
  dev->reg[reg_0x04].value &= ~(REG04_FILTER | REG04_BITSET);
  /* we could make the color filter used an advanced option of the backend */
  if (channels == 1)
    dev->reg[reg_0x04].value |= 0x08;	/* green filter */

  /* bit depth: 8bit + gamma, 16 bits + no gamma */
  if (dev->settings.scan_mode == 0)
    {
      /* lineart, todo : find logs of real lineart, not an emulated one
         with gray level scanning */
      depth = 8;
      dev->reg[reg_0x04].value &= ~REG04_LINEART;
    }
  else
    {
      if (dev->settings.depth > 8)
	{
	  depth = 16;
	  dev->reg[reg_0x04].value |= REG04_BITSET;
	}
      else
	depth = 8;
      dev->reg[reg_0x04].value &= ~REG04_LINEART;
    }

  /* it is useles to scan at 16 bit if gamma table is at 12 or 14 bits
     so we disable hardware gamma correction when doing 16 bits scans */
  if (dev->settings.depth < 16)
    dev->reg[reg_0x05].value |= REG05_GMMENB;
  else
    dev->reg[reg_0x05].value &= ~REG05_GMMENB;

  /* sensor parameters */
  gl646_setup_sensor (dev, dev->reg, 1, half_ccd);

  /* dummy lines: may not be usefull, for instance 250 dpi works with 0 or 1
     dummy line. Maybe the dummy line adds correctness since the motor runs 
     slower (higher dpi) */
  dev->reg[reg_0x1e].value = dev->reg[reg_0x1e].value & 0xf0;	/* 0 dummy lines */
  dummy = 0;
  if (dev->model->ccd_type == CCD_5345)
    {
      if (dpiset >= dev->sensor.optical_res / 3
	  && dpiset <= dev->sensor.optical_res / 2)
	dummy = 1;
      if (dev->settings.yres == 800)
	dummy = 3;
    }
  dev->reg[reg_0x1e].value = dev->reg[reg_0x1e].value | dummy;	/* dummy lines */

  switch (dev->model->motor_type)
    {
    case MOTOR_5345:
      gl646_setup_steps (dev, dev->reg, dev->settings.yres);
      fast_dpi = 200;
      fast_exposure = 3600;
      slope_dpi = dev->settings.yres;

      /* we use same_speed until below a threshold */
      if (slope_dpi < dev->motor.base_ydpi / 2)
	same_speed = 0;
      else
	same_speed = 1;
      break;

    case MOTOR_HP2400:
    case MOTOR_HP2300:
      gl646_setup_steps (dev, dev->reg, dev->settings.yres);
      fast_dpi = dev->motor.base_ydpi / 4;
      fast_exposure = 2700;
      slope_dpi = dev->settings.yres;
      same_speed = 0;
      break;
    }
  if (slope_dpi == 75 && dev->model->motor_type == MOTOR_5345)	/* todo: do not hard code this one */
    slope_dpi = 100;
  slope_dpi = slope_dpi * (1 + dummy);

  lincnt = dev->settings.lines - 1 + max_shift + stagger;

  dev->reg[reg_0x25].value = LOBYTE (HIWORD (lincnt));	/* scan line numbers - here one line */
  dev->reg[reg_0x26].value = HIBYTE (LOWORD (lincnt));
  dev->reg[reg_0x27].value = LOBYTE (LOWORD (lincnt));

  if ((dev->model->ccd_type == CCD_5345) && (channels == 1) && (!half_ccd))
    {
      dev->reg[reg_0x28].value = HIBYTE (250);
      dev->reg[reg_0x29].value = LOBYTE (250);
    }
  else
    {
      dev->reg[reg_0x28].value = HIBYTE (511);
      dev->reg[reg_0x29].value = LOBYTE (511);
    }


  dev->reg[reg_0x2c].value = HIBYTE (dpiset);
  dev->reg[reg_0x2d].value = LOBYTE (dpiset);

  dev->reg[reg_0x30].value = HIBYTE (start);
  dev->reg[reg_0x31].value = LOBYTE (start);
  dev->reg[reg_0x32].value = HIBYTE (end);
  dev->reg[reg_0x33].value = LOBYTE (end);

  if (half_ccd)
    words_per_line = (dpiset * pixels * channels) / dev->sensor.optical_res;
  else
    words_per_line = pixels * channels;
  words_per_line = (words_per_line * depth) / 8;

  dev->reg[reg_0x35].value = LOBYTE (HIWORD (words_per_line));
  dev->reg[reg_0x36].value = HIBYTE (LOWORD (words_per_line));
  dev->reg[reg_0x37].value = LOBYTE (LOWORD (words_per_line));

  if ((dev->model->ccd_type == CCD_5345)
      || (dev->model->ccd_type == CCD_HP2300)
      || (dev->model->ccd_type == CCD_HP2400))
    exposure_time =
      sanei_genesys_exposure_time (dev, dev->reg, dev->settings.xres);
  else
    exposure_time = dev->settings.exposure_time;

  dev->reg[reg_0x38].value = HIBYTE (exposure_time);
  dev->reg[reg_0x39].value = LOBYTE (exposure_time);

  /* build slope table for choosen dpi */
  sanei_genesys_create_slope_table (dev,
				    dev->slope_table0,
				    dev->reg[reg_0x21].value,
				    dev->reg[reg_0x02].value & REG02_STEPSEL,
				    exposure_time, same_speed, slope_dpi, 0);

  /* computes sum of used steps */
  steps_sum = 0;
  for (i = 0; i < dev->reg[reg_0x1f].value - 1; i++)
    steps_sum += dev->slope_table0[i];

  /* build slope1 (fast moving) */
  sanei_genesys_create_slope_table (dev,
				    dev->slope_table1,
				    dev->reg[reg_0x6b].value,
				    (dev->reg[reg_0x6a].
				     value & REG6A_FSTPSEL) >> 6,
				    fast_exposure, 0, fast_dpi, 0);

  /* steps to move to reach scanning area:
     - first we move to physical start of scanning
     either by a fixed steps amount from the black strip
     or by a fixed amount from parking position,
     minus the steps done during shading calibration
     - then we move by the needed offset whitin physical
     scanning area
     - todo: substract steps done during motor acceleration or
     will it be included in y_offset ?

     assumption: steps are expressed at maximum motor resolution

     we need:   
     SANE_Fixed y_offset;                       
     SANE_Fixed y_size;                 
     SANE_Fixed y_offset_calib;
     mm_to_steps()=motor dpi / 2.54 / 10=motor dpi / MM_PER_INCH */

  /* if scanner uses GENESYS_FLAG_SEARCH_START y_offset is
     relative from origin, else, it is from parking position */
  switch (dev->model->motor_type)
    {
    case MOTOR_5345:
      move_dpi = dev->motor.optical_ydpi;
      break;
    default:
      move_dpi = dev->motor.base_ydpi;
      break;
    }
  if (dev->model->flags & GENESYS_FLAG_SEARCH_START)
    move = (SANE_UNFIX (dev->model->y_offset_calib) * move_dpi) / MM_PER_INCH;
  else
    move = 0;
  DBG (DBG_info, "gl646_init_regs_for_scan: move=%d steps\n", move);

  move += (SANE_UNFIX (dev->model->y_offset) * move_dpi) / MM_PER_INCH;
  if (move < 0)
    {
      DBG (DBG_error,
	   "gl646_init_regs_for_scan: overriding negative move value %d\n",
	   move);
      move = 1;
    }
  DBG (DBG_info, "gl646_init_regs_for_scan: move=%d steps\n", move);

  /* add tl_y to base movement */
  /* move += (dev->settings.tl_y * dev->motor.optical_ydpi) / MM_PER_INCH; */
  move += (dev->settings.tl_y * move_dpi) / MM_PER_INCH;
  DBG (DBG_info, "gl646_init_regs_for_scan: move=%d steps\n", move);

  /* substract current head position */
  move -= dev->scanhead_position_in_steps - stagger;
  DBG (DBG_info, "gl646_init_regs_for_scan: move=%d steps\n", move);

  if ((dev->reg[reg_0x02].value & REG02_FASTFED)
      && (dev->model->motor_type == MOTOR_HP2300))
    {
      move *= 2;
      DBG (DBG_info, "gl646_init_regs_for_scan: move=%d steps\n", move);
    }

  /* round it */
  move = ((move + dummy) / (dummy + 1)) * (dummy + 1);
  DBG (DBG_info, "gl646_init_regs_for_scan: move=%d steps\n", move);

  /* set feed steps number of motor move */
  dev->reg[reg_0x3d].value = LOBYTE (HIWORD (move));
  dev->reg[reg_0x3e].value = HIBYTE (LOWORD (move));
  dev->reg[reg_0x3f].value = LOBYTE (LOWORD (move));

  sanei_genesys_calculate_zmode2 (dev->reg[reg_0x02].value & REG02_FASTFED,
				  exposure_time,
				  dev->slope_table0,
				  dev->reg[reg_0x21].value,
				  move, dev->reg[reg_0x22].value, &z1, &z2);
  DBG (DBG_info, "gl646_init_regs_for_scan: z1 = %d\n", z1);
  DBG (DBG_info, "gl646_init_regs_for_scan: z2 = %d\n", z2);
  dev->reg[reg_0x60].value = HIBYTE (z1);
  dev->reg[reg_0x61].value = LOBYTE (z1);
  dev->reg[reg_0x62].value = HIBYTE (z2);
  dev->reg[reg_0x63].value = LOBYTE (z2);
  dev->reg[reg_0x6c].value = (dev->reg[reg_0x6c].value & REG6C_TGTIME) | ((z1 >> 13) & 0x38) | ((z2 >> 16) & 0x07);	/* todo: double check */

  /* todo: is it really that useful ? */
  if (dev->model->motor_type == MOTOR_5345)
    {
      if (dev->settings.yres > 150)
	dev->reg[reg_0x65].value = 0;
      else
	dev->reg[reg_0x65].value = 2;
    }
  else
    dev->reg[reg_0x65].value = 0x3f;
  dev->reg[reg_0x67].value = dev->gpo.enable[1];

  /* prepares data reordering */

  /* we must use a round number of words_per_line */
  requested_buffer_size = (BULKIN_MAXSIZE / words_per_line) * words_per_line;

  read_buffer_size =
    2 * requested_buffer_size +
    ((max_shift + stagger) * pixels * channels * depth) / 8;

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

  /* scan bytes to read */
  dev->read_bytes_left = words_per_line * (lincnt + 1);


  DBG (DBG_info,
       "gl646_init_regs_for_scan: physical bytes to read = %lu\n",
       (u_long) dev->read_bytes_left);
  dev->read_active = SANE_TRUE;


  dev->current_setup.pixels = (pixels * dpiset) / dev->sensor.optical_res;
  dev->current_setup.lines = lincnt + 1;
  dev->current_setup.depth = depth;
  dev->current_setup.channels = channels;
  dev->current_setup.exposure_time = exposure_time;
  if (half_ccd)
    dev->current_setup.xres = dpiset / 2;
  else
    dev->current_setup.xres = dpiset;
  dev->current_setup.yres = dev->settings.yres;
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
  if (depth == 1 || dev->settings.scan_mode == 0)
    dev->total_bytes_to_read =
      ((dev->settings.pixels * dev->settings.lines) / 8 +
       (((dev->settings.pixels * dev->settings.lines) % 8) ? 1 : 0)) *
      channels;
  else
    dev->total_bytes_to_read =
      dev->settings.pixels * dev->settings.lines * channels * (depth / 8);

  DBG (DBG_info, "gl646_init_regs_for_scan: total bytes to send = %lu\n",
       (u_long) dev->total_bytes_to_read);
/* END TODO */




  status =
    gl646_send_slope_table (dev, 0, dev->slope_table0,
			    sanei_genesys_read_reg_from_set (dev->reg, 0x21));
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl646_init_regs_for_scan: failed to send slope table 0: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status =
    gl646_send_slope_table (dev, 1, dev->slope_table1,
			    sanei_genesys_read_reg_from_set (dev->reg, 0x6b));


  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl646_init_regs_for_scan: failed to send slope table 1: %s\n",
	   sane_strstatus (status));
      return status;
    }

  DBG (DBG_proc, "gl646_init_regs_for_scan: completed\n");
  return SANE_STATUS_GOOD;
}

/*
 * this function sends generic gamma table (ie linear ones)
 * or the Sensor specific one if provided
 */
static SANE_Status
gl646_send_gamma_table (Genesys_Device * dev, SANE_Bool generic)
{
  int size;
  int address;
  int status;
  u_int8_t *gamma;
  int i;

  /* don't send anything if no specific gamma table defined */
  if (!generic
      && (dev->sensor.red_gamma_table == NULL
	  || dev->sensor.green_gamma_table == NULL
	  || dev->sensor.blue_gamma_table == NULL))
    {
      DBG (DBG_proc, "gl646_send_gamma_table: nothing to send, skipping\n");
      return SANE_STATUS_GOOD;
    }

  /* gamma table size */
  if (dev->reg[reg_0x05].value & REG05_GMMTYPE)
    size = 16384;
  else
    size = 4096;

  /* table address */
  switch (dev->reg[reg_0x05].value >> 6)
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
      return SANE_STATUS_INVAL;
    }

  /* allocate temporary gamma tables: 16 bits words, 3 channels */
  gamma = (u_int8_t *) malloc (size * 2 * 3);
  if (!gamma)
    return SANE_STATUS_NO_MEM;
  /* take care off generic/specific data */
  if (generic)
    {
      /* fill with default values */
      for (i = 0; i < size; i++)
	{
	  gamma[i * 2] = i & 0xff;
	  gamma[i * 2 + 1] = i >> 8;
	  gamma[i * 2 + size * 2] = i & 0xff;
	  gamma[i * 2 + 1 + size * 2] = i >> 8;
	  gamma[i * 2 + size * 4] = i & 0xff;
	  gamma[i * 2 + 1 + size * 4] = i >> 8;
	}
    }
  else
    {
      /* copy sensor specific's gamma tables */
      for (i = 0; i < size; i++)
	{
	  gamma[i * 2] = dev->sensor.red_gamma_table[i] & 0xff;
	  gamma[i * 2 + 1] = dev->sensor.red_gamma_table[i] >> 8;
	  gamma[i * 2 + size * 2] = dev->sensor.green_gamma_table[i] & 0xff;
	  gamma[i * 2 + 1 + size * 2] = dev->sensor.green_gamma_table[i] >> 8;
	  gamma[i * 2 + size * 4] = dev->sensor.blue_gamma_table[i] & 0xff;
	  gamma[i * 2 + 1 + size * 4] = dev->sensor.blue_gamma_table[i] >> 8;
	}
    }

  /* send address */
  status = sanei_genesys_set_buffer_address (dev, address);
  if (status != SANE_STATUS_GOOD)
    {
      free (gamma);
      DBG (DBG_error,
	   "gl646_send_gamma_table: failed to set buffer address: %s\n",
	   sane_strstatus (status));
      return status;
    }

  /* send data */
  status =
    gl646_bulk_write_data (dev, 0x3c, (u_int8_t *) gamma, size * 2 * 3);
  if (status != SANE_STATUS_GOOD)
    {
      free (gamma);
      DBG (DBG_error,
	   "gl646_send_gamma_table: failed to send gamma table: %s\n",
	   sane_strstatus (status));
      return status;
    }

  DBG (DBG_proc, "gl646_send_gamma_table: completed\n");
  free (gamma);
  return SANE_STATUS_GOOD;
}

/* this function does the led calibration.
*/
static SANE_Status
gl646_led_calibration (Genesys_Device * dev)
{
  DBG (DBG_error, "Implementation for led calibration missing\n");
  if (dev || dev == NULL)
    return SANE_STATUS_INVAL;
  return SANE_STATUS_INVAL;
}

/* this function does the offset calibration by scanning one line of the calibration
   area below scanner's top. There is a black margin and the remaining is white.
   genesys_search_start() must have been called so that the offsets and margins
   are allready known.
*/
static SANE_Status
gl646_offset_calibration (Genesys_Device * dev)
{
  int num_pixels;
  int total_size;
  int avg[3];
  u_int8_t *first_line, *second_line;
  int i, j;
  SANE_Status status = SANE_STATUS_GOOD;
  int average, val, count;
  int minimum, offset, dpi, channels;
  SANE_Bool half_ccd = 1;
  int steps = 0, lincnt = 1, start_pixel;
  u_int16_t slope_table[256];

  DBG (DBG_proc, "gl646_offset_calibration\n");

  /* offset calibration is allways done in color mode */
  channels = 3;

  /* todo : turn it into a switch */
  /*  full CCD width is used */
  if (dev->model->ccd_type == CCD_5345)
    {
      if (dev->settings.xres > dev->sensor.optical_res / 2)
	{
	  half_ccd = 0;
	  num_pixels = dev->sensor.sensor_pixels;
	}
      else
	{
	  half_ccd = 1;
	  num_pixels = dev->sensor.sensor_pixels / 2;
	}
      dpi = 1200;
      steps = 0;
      lincnt = 1;
    }
  else if (dev->model->ccd_type == CCD_HP2300
	   || dev->model->ccd_type == CCD_HP2400)
    {
      dpi = dev->settings.xres;
      steps = 1;
      lincnt = 2;
      num_pixels = 2668;
      if (dev->settings.xres > dev->sensor.optical_res / 2)
	half_ccd = 0;
      else
	half_ccd = 1;
    }
  else
    {
      dpi = 600;
      num_pixels = 2 * dpi;
      lincnt = 1;
    }
  start_pixel = dev->sensor.dummy_pixel;

  total_size = num_pixels * channels * 2 * lincnt;	/* colors * bytes_per_color * scan lines */

  dev->calib_reg[reg_0x01].value &= ~REG01_DVDSET;
  dev->calib_reg[reg_0x02].value = REG02_ACDCDIS;
  if (dev->model->motor_type == MOTOR_5345 || MOTOR_HP2300 || MOTOR_HP2400)
    {
      if (half_ccd)
	dev->calib_reg[reg_0x02].value =
	  (dev->calib_reg[reg_0x02].value & ~REG02_STEPSEL) | REG02_HALFSTEP;
      else
	dev->calib_reg[reg_0x02].value =
	  (dev->calib_reg[reg_0x02].
	   value & ~REG02_STEPSEL) | REG02_QUATERSTEP;
      if (dev->model->motor_type == CCD_5345)
	dev->calib_reg[reg_0x03].value |= REG03_AVEENB;
      else
	dev->calib_reg[reg_0x03].value &= ~REG03_AVEENB;
    }

  dev->calib_reg[reg_0x04].value =
    (dev->calib_reg[reg_0x04].
     value & ~REG04_LINEART & ~REG04_FILTER) | REG04_BITSET;
  if (channels == 1)
    dev->calib_reg[reg_0x04].value |= 0x08;	/* green filter */

  dev->calib_reg[reg_0x05].value = (dev->calib_reg[reg_0x05].value & ~REG05_GMMENB);	/* disable gamma */

/* ST12: 0x01 0x00 0x02 0x41 0x03 0x1f 0x04 0x53 0x05 0x10 0x06 0x10 0x08 0x02 0x09 0x00 0x0a 0x06 0x0b 0x04 */
/* ST24:           0x02 0x71           0x04 0x5f 0x05 0x50           0x08 0x0e 0x09 0x0c 0x0a 0x00 0x0b 0x0c */
#if 0
  dev->calib_reg[reg_0x01].value = 0x00 /*0x02 */ ;	/* disable shading, enable CCD, color, 1M */
  dev->calib_reg[reg_0x02].value = 0x71 /*0x40 */ ;	/* no forward/backward, no auto-home, motor off, full step */
  dev->calib_reg[reg_0x03].value = 0x1f /*0x17 */ ;	/* lamp on */
  dev->calib_reg[reg_0x04].value = 0x53;	/* 16 bits data, 16 bits A/D, color, Wolfson fe */
  dev->calib_reg[reg_0x05].value = 0x50 /*0x00 */ ;	/* CCD res = 600 Dpi, 12 bits gamma, disable gamma, 24 clocks/pixel */
  dev->calib_reg[reg_0x06].value = 0x10;	/* power bit set, shading gain = 4 times system, no asic test */
  dev->calib_reg[reg_0x07].value = 0x00;	/* DMA access */
#endif

  gl646_setup_sensor (dev, dev->calib_reg, 1, half_ccd);

  /* motor & movement control , todo : could be removed */
  if (dev->model->motor_type == MOTOR_5345)
    dev->calib_reg[reg_0x1f].value = 0x01;	/* scan feed step for table one in two table mode only */

  if (dev->model->motor_type == MOTOR_5345)
    {
      gl646_setup_steps (dev, dev->calib_reg, dpi);
      dev->calib_reg[reg_0x21].value = 1;
    }
  else
    {
      dev->calib_reg[reg_0x21].value = 2;
      dev->calib_reg[reg_0x22].value = 16;
      dev->calib_reg[reg_0x23].value = 16;
      dev->calib_reg[reg_0x24].value = 2;
    }

  dev->calib_reg[reg_0x25].value = LOBYTE (HIWORD (lincnt));
  dev->calib_reg[reg_0x26].value = HIBYTE (LOWORD (lincnt));
  dev->calib_reg[reg_0x27].value = LOBYTE (LOWORD (lincnt));

  dev->calib_reg[reg_0x28].value = 0x01;	/* PWM duty for lamp control */
  dev->calib_reg[reg_0x29].value = 0xff;

  dev->calib_reg[reg_0x2c].value = HIBYTE (dpi);
  dev->calib_reg[reg_0x2d].value = LOBYTE (dpi);

  dev->calib_reg[reg_0x30].value = HIBYTE (start_pixel);
  dev->calib_reg[reg_0x31].value = LOBYTE (start_pixel);
  dev->calib_reg[reg_0x32].value = HIBYTE (start_pixel + num_pixels);
  dev->calib_reg[reg_0x33].value = LOBYTE (start_pixel + num_pixels);
  if (dev->model->ccd_type == CCD_5345)
    {
      /* full CCD width from 1st non dummy pixel */
      total_size = num_pixels * channels * 2 * lincnt;

      /* fixed maximum word number */
      dev->calib_reg[reg_0x35].value = LOBYTE (HIWORD (65536));
      dev->calib_reg[reg_0x36].value = HIBYTE (LOWORD (65536));
      dev->calib_reg[reg_0x37].value = LOBYTE (LOWORD (65536));
    }
  else
    {
      total_size =
	(num_pixels * channels * 2 * dpi * lincnt) / dev->sensor.optical_res;
      dev->calib_reg[reg_0x35].value = LOBYTE (HIWORD (total_size / lincnt));
      dev->calib_reg[reg_0x36].value = HIBYTE (LOWORD (total_size / lincnt));
      dev->calib_reg[reg_0x37].value = LOBYTE (LOWORD (total_size / lincnt));
    }

  dev->calib_reg[reg_0x34].value = dev->sensor.dummy_pixel;	/* set the CCD dummy & optical black pixels number, for whole line shading (64) */
  dev->calib_reg[reg_0x38].value =
    HIBYTE (sanei_genesys_exposure_time
	    (dev, dev->calib_reg, dev->settings.xres));
  dev->calib_reg[reg_0x39].value =
    LOBYTE (sanei_genesys_exposure_time
	    (dev, dev->calib_reg, dev->settings.xres));

  /* set feed steps number of motor move */
  dev->calib_reg[reg_0x3d].value = LOBYTE (HIWORD (steps));
  dev->calib_reg[reg_0x3e].value = HIBYTE (LOWORD (steps));
  dev->calib_reg[reg_0x3f].value = LOBYTE (LOWORD (steps));

/*
ST12: 0x60 0x00 0x61 0x00 0x62 0x00 0x63 0x00 0x64 0x00 0x65 0x3f 0x66 0x00 0x67 0x00 0x68 0x51 0x69 0x20 */
/* ST24: 0x60 0x00 0x61 0x00 0x62 0x00 0x63 0x00 0x64 0x00 0x65 0x00 0x66 0x00 0x67 0x00 0x68 0x51 0x69 0x20 */
  dev->calib_reg[reg_0x60].value = 0x00;	/* Z1MOD, 60h:61h:(6D b5:b3), remainder for start/stop */
  dev->calib_reg[reg_0x61].value = 0x00;	/* (21h+22h)/LPeriod */
  dev->calib_reg[reg_0x62].value = 0x00;	/* Z2MODE, 62h:63h:(6D b2:b0), remainder for start scan */
  dev->calib_reg[reg_0x63].value = 0x00;	/* (3Dh+3Eh+3Fh)/LPeriod for one-table mode,(21h+1Fh)/LPeriod */
  dev->calib_reg[reg_0x64].value = 0x00;	/* motor PWM frequency */
  dev->calib_reg[reg_0x65].value = 0x00;	/* PWM duty cycle for table one motor phase (63 = max) */
  if (dev->model->motor_type == MOTOR_5345)
    {
      dev->gpo.value[1] |= 0x18;	/* has to do with bipolar V-ref */
      if (dev->settings.yres <= 150)
	dev->calib_reg[reg_0x65].value = 0x02;
    }
  dev->calib_reg[reg_0x66].value = dev->gpo.value[0];
  dev->calib_reg[reg_0x67].value = dev->gpo.value[1];
  dev->calib_reg[reg_0x68].value = dev->gpo.enable[0];
  dev->calib_reg[reg_0x69].value = dev->gpo.enable[1];

/* ST12: 0x6a 0x7f 0x6b 0xff 0x6c 0x00 0x6d 0x01 */
/* ST24: 0x6a 0x40 0x6b 0xff 0x6c 0x00 0x6d 0x01 */
  dev->calib_reg[reg_0x6c].value = 0x00;	/* period times for LPeriod, expR,expG,expB, Z1MODE, Z2MODE (one period time) */
  dev->calib_reg[reg_0x6a].value = dev->calib_reg[reg_0x6a].value | 0x02;
  if (dev->model->motor_type == MOTOR_5345)
    {
      dev->calib_reg[reg_0x6b].value = 0xff;
      dev->calib_reg[reg_0x6d].value = 0x41;
    }

  dev->calib_reg[reg_0x60].value = HIBYTE (0);
  dev->calib_reg[reg_0x61].value = LOBYTE (0);
  dev->calib_reg[reg_0x62].value = HIBYTE (0);
  dev->calib_reg[reg_0x63].value = LOBYTE (0);

  if (dev->model->motor_type == MOTOR_HP2300
      || dev->model->motor_type == MOTOR_HP2400)
    {
      dev->calib_reg[reg_0x65].value = 0x3f;	/* PWM duty cycle for table one motor phase (63 = max) */
      dev->calib_reg[reg_0x6b].value = 0x02;
      dev->calib_reg[reg_0x6d].value = 0x7f;
      slope_table[0] = 4480;
      slope_table[1] = 4480;
      RIE (gl646_send_slope_table
	   (dev, 0, slope_table, dev->calib_reg[reg_0x21].value));
    }

  /* work around incorrect calibration when frequent color/dpi scan changes */
  RIE (gl646_set_fe (dev, AFE_INIT));

  RIE (gl646_set_fe (dev, AFE_SET));

  RIE (gl646_bulk_write_register
       (dev, dev->calib_reg, GENESYS_GL646_MAX_REGS));

  first_line = malloc (total_size);
  if (!first_line)
    return SANE_STATUS_NO_MEM;

  second_line = malloc (total_size);
  if (!second_line)
    {
      free (first_line);
      return SANE_STATUS_NO_MEM;
    }

  /* scan first line of data with no offset nor gain */
  dev->frontend.gain[0] = 0x00;
  dev->frontend.gain[1] = 0x00;
  dev->frontend.gain[2] = 0x00;
  offset = 0;
  dev->frontend.offset[0] = offset;
  dev->frontend.offset[1] = offset;
  dev->frontend.offset[2] = offset;
  status = gl646_set_fe (dev, AFE_SET);
  if (status != SANE_STATUS_GOOD)
    {
      free (first_line);
      free (second_line);
      DBG (DBG_error,
	   "gl646_offset_calibration: failed to set frontend: %s\n",
	   sane_strstatus (status));
      return status;
    }

  RIE (gl646_begin_scan (dev, dev->calib_reg, SANE_TRUE));
  RIE (sanei_genesys_read_data_from_scanner (dev, first_line, total_size));
  RIE (gl646_end_scan (dev, dev->calib_reg, SANE_FALSE));

  if (DBG_LEVEL >= DBG_data)
    sanei_genesys_write_pnm_file ("offset1.pnm",
				  first_line,
				  16,
				  channels,
				  (num_pixels * dpi) /
				  dev->sensor.optical_res, lincnt);

  /* We search for minimum dark value, then deduce a threshold from it.
     This allow us to use data without knowing the geometry of the
     black areas */
  val = 0;
  minimum = 65535;
  for (j = 0; j < channels; j++)
    {
      for (i = 0; i < num_pixels; i++)
	{
	  val =
	    first_line[i * 2 * channels + 2 * j + 1] * 256 +
	    first_line[i * 2 * channels + 2 * j];
	  if (val < minimum)
	    minimum = val;
	}
    }
  /* minimum accepted black pixel */
  minimum *= 1.1;
  DBG (DBG_proc, "gl646_offset_calibration:  black threshold = %d\n",
       minimum);

  /* computes lowest average black value on black margin */
  for (j = 0; j < channels; j++)
    {
      avg[j] = 0;
      count = 0;
      for (i = 0; i < num_pixels; i++)
	{
	  val =
	    first_line[i * 2 * channels + 2 * j + 1] * 256 +
	    first_line[i * 2 * channels + 2 * j];
	  if (val <= minimum)
	    {
	      avg[j] += val;
	      count++;
	    }
	}
      if (count)
	avg[j] /= count;
      DBG (DBG_proc, "gl646_offset_calibration:  avg[%d] = %d\n", j,
	   avg[j] / 256);
    }

  /* now finds minimal black average */
  minimum = avg[0];
  if (channels > 1)
    {
      if (avg[1] < minimum)
	minimum = avg[1];
      if (avg[2] < minimum)
	minimum = avg[2];
    }

  switch (dev->model->ccd_type)
    {
    case CCD_5345:
      offset = minimum / 256;
      break;
    case CCD_HP2300:
    case CCD_HP2400:
      offset = minimum / 2 * 256;
    }
  DBG (DBG_proc, "gl646_offset_calibration:  minimum = %.2f, offset = %d\n",
       (float) minimum / 256.0, offset);

  /* scan second line of data with a fixed offset and no gain */
  dev->frontend.offset[0] = offset;
  dev->frontend.offset[1] = offset;
  dev->frontend.offset[2] = offset;
  status = gl646_set_fe (dev, AFE_SET);
  if (status != SANE_STATUS_GOOD)
    {
      free (first_line);
      free (second_line);
      DBG (DBG_error,
	   "gl646_offset_calibration: failed to set frontend: %s\n",
	   sane_strstatus (status));
      return status;
    }

  RIE (gl646_begin_scan (dev, dev->calib_reg, SANE_TRUE));
  RIE (sanei_genesys_read_data_from_scanner (dev, second_line, total_size));
  RIE (gl646_end_scan (dev, dev->calib_reg, SANE_FALSE));

  if (DBG_LEVEL >= DBG_data)
    sanei_genesys_write_pnm_file ("offset2.pnm",
				  second_line,
				  16,
				  channels,
				  (num_pixels * dpi) /
				  dev->sensor.optical_res, lincnt);

  /* compute final offsets */
  for (j = 0; j < channels; j++)
    {
      /* finds minimum for current color channel */
      minimum = 65535;
      for (i = 0; i < num_pixels; i++)
	{
	  val =
	    second_line[i * 2 * channels + 2 * j + 1] * 256 +
	    second_line[i * 2 * channels + 2 * j];
	  if (val < minimum)
	    minimum = val;
	}
      minimum *= 1.1;

      /* computes black average for channel */
      average = 0;
      count = 0;
      for (i = 0; i < num_pixels; i++)
	{
	  val =
	    256 * second_line[i * 2 * channels + j * 2 + 1] +
	    second_line[i * 2 * channels + j * 2];
	  if (val < minimum)
	    {
	      average += val;
	      count++;
	    }
	}
      if (count)
	average /= count;

      switch (dev->model->ccd_type)
	{
	case CCD_5345:
	  /* dev->frontend.offset[j] = (offset*min[j]) / (min[j]-average); */
	  /* we assume a linear law, the 0.95 coeff is there because while we want
	     a DC offset of 0, values are varying a little and we let a little offset
	     to get sure we don't have "negative" values */
	  dev->frontend.offset[j] =
	    offset + ((float) (average * offset) * 0.95) / (avg[j] - average);
	  /* dev->frontend.offset[j] = offset + ((float) average) / (1.3 * 256) ; */
	  break;
	case CCD_HP2300:
	  if (j == 0)
	    dev->frontend.offset[j] = offset + average / (1.2 * 256);
	  else
	    dev->frontend.offset[j] = offset + average / (1.0 * 256);
	  break;
	}
      DBG (DBG_proc,
	   "gl646_offset_calibration: average[%d] = %.2f, offset = %d\n", j,
	   (float) average / 256, dev->frontend.offset[j]);
    }
  if (channels == 1)
    {
      dev->frontend.offset[1] = dev->frontend.offset[0];
      dev->frontend.offset[2] = dev->frontend.offset[0];
    }

  /* cleanup before return */
  free (first_line);
  free (second_line);
  DBG (DBG_proc, "gl646_offset_calibration: completed\n");
  return status;
}

/* alternative coarse gain calibration 
   this on uses the settings from offset_calibration and
   uses only one scanline
 */
static SANE_Status
gl646_coarse_gain_calibration (Genesys_Device * dev, int dpi)
{
  int num_pixels;
  int black_pixels;
  int total_size;
  u_int8_t *line;
  int i, j, channels;
  SANE_Status status = SANE_STATUS_GOOD;
  float average[3];
  int lincnt, words, count, val;
  int maximum;

  DBG (DBG_proc, "gl646_coarse_gain_calibration\n");

  /* coarse gain calibration is allways done in color mode */
  channels = 3;

  black_pixels =
    (dev->sensor.CCD_start_xoffset * dpi) / dev->sensor.optical_res;

  lincnt = 65536 * dev->calib_reg[reg_0x25].value;
  lincnt += 256 * dev->calib_reg[reg_0x26].value;
  lincnt += dev->calib_reg[reg_0x27].value;

  words = 65536 * dev->calib_reg[reg_0x35].value;
  words += 256 * dev->calib_reg[reg_0x36].value;
  words += dev->calib_reg[reg_0x37].value;

  num_pixels = words / (channels * 2);

  total_size = words * lincnt;	/* colors * bytes_per_color * scan lines */

  line = malloc (total_size);
  if (!line)
    return SANE_STATUS_NO_MEM;

  /* sends offsets computed by offset calibration */
  status = gl646_set_fe (dev, AFE_SET);
  if (status != SANE_STATUS_GOOD)
    {
      free (line);
      DBG (DBG_error,
	   "gl646_coarse_gain_calibration: failed to set frontend: %s\n",
	   sane_strstatus (status));
      return status;
    }

  RIE (gl646_begin_scan (dev, dev->calib_reg, SANE_TRUE));
  RIE (sanei_genesys_read_data_from_scanner (dev, line, total_size));
  RIE (gl646_end_scan (dev, dev->calib_reg, SANE_FALSE));

  if (DBG_LEVEL >= DBG_data)
    sanei_genesys_write_pnm_file ("alternative_coarse.pnm", line, 16,
				  channels, num_pixels, lincnt);

  /* average high level for each channel and compute gain
     to reach the target code 
     we only use the central half of the CCD data         */
  /* todo : use lincnt lines */
  for (j = 0; j < channels; j++)
    {
      /* we find the maximum white value, so we can deduce a threshold
         to average white values */
      maximum = 0;
      for (i = 0; i < num_pixels; i++)
	{
	  val =
	    256 * line[i * 2 * channels + 2 * j + 1] + line[i * 2 * channels +
							    2 * j];
	  if (val > maximum)
	    maximum = val;
	}
      /* threshold */
      maximum *= 0.9;

      /* computes white average */
      average[j] = 0;
      count = 0;
      for (i = 0; i < num_pixels; i++)
	{
	  /* averaging anly white points allow us not to care about dark margins */
	  val =
	    256 * line[i * 2 * channels + 2 * j + 1] + line[i * 2 * channels +
							    2 * j];
	  if (val > maximum)
	    {
	      average[j] += val;
	      count++;
	    }
	}
      average[j] = average[j] / count;

      switch (dev->model->ccd_type)
	{
	case CCD_HP2300:
	  dev->frontend.gain[j] =
	    (int) (((dev->sensor.gain_white_ref * 256) / average[j] -
		    1.0) / 0.4);
	  break;
	case CCD_5345:
	default:
	  /* 0.445 */
	  dev->frontend.gain[j] =
	    (int) (((dev->sensor.gain_white_ref * 256) / average[j] -
		    1.0) / 0.445);
	  break;
	}

      DBG (DBG_proc,
	   "gl646_coarse_gain_calibration: channel %d, average = %.2f, gain = %d\n",
	   j, average[j], dev->frontend.gain[j]);
    }

  if (dev->settings.scan_mode != 4)	/* single pass color */
    {
      dev->frontend.gain[0] = dev->frontend.gain[1];
      dev->frontend.gain[2] = dev->frontend.gain[1];
    }

  free (line);
  DBG (DBG_proc, "gl646_coarse_gain_calibration: completed\n");
  return status;
}

/*
 * wait for lamp warmup by scanning the same line until difference
 * between 2 scans is below a threshold
 */
static SANE_Status
gl646_init_regs_for_warmup (Genesys_Device * dev,
			    Genesys_Register_Set * local_reg,
			    int *channels, int *total_size)
{
  int num_pixels = (int) (4 * 300);
  int dpi = 300, lincnt, exposure_time, words_per_line;
  int startpixel, endpixel;
  int steps = 0;
  SANE_Status status = SANE_STATUS_GOOD;
  u_int16_t slope_table[256];

  DBG (DBG_proc, "gl646_warmup_lamp\n");

  memcpy (local_reg, dev->reg, (GENESYS_GL646_MAX_REGS + 1) * sizeof(Genesys_Register_Set));
  *total_size = num_pixels * 3 * 2 * 1;	/* colors * bytes_per_color * scan lines */

/* ST12: 0x01 0x00 0x02 0x41 0x03 0x1f 0x04 0x53 0x05 0x10 0x06 0x10 0x08 0x02 0x09 0x00 0x0a 0x06 0x0b 0x04 */
/* ST24:           0x02 0x71           0x04 0x5f 0x05 0x50           0x08 0x0e 0x09 0x0c 0x0a 0x00 0x0b 0x0c */
#if 0
  local_reg[reg_0x01].value = 0x00 /*0x02 */ ;	/* disable shading, enable CCD, color, 1M */
  local_reg[reg_0x02].value = 0x71 /*0x40 */ ;	/* no forward/backward, no auto-home, motor off, full step */
  local_reg[reg_0x03].value = 0x1f /*0x17 */ ;	/* lamp on */
  local_reg[reg_0x04].value = 0x53;	/* 16 bits data, 16 bits A/D, color, Wolfson fe */
  local_reg[reg_0x05].value = 0x50 /*0x00 */ ;	/* CCD res = 600 dpi, 12 bits gamma, disable gamma, 24 clocks/pixel */
  local_reg[reg_0x06].value = 0x10;	/* power bit set, shading gain = 4 times system, no asic test */
  local_reg[reg_0x07].value = 0x00;	/* DMA access */
  gl646_setup_sensor (dev, local_reg);
#endif


  local_reg[reg_0x03].value |= REG03_LAMPPWR;
  local_reg[reg_0x04].value =
    (local_reg[reg_0x04].value & ~REG04_LINEART) | REG04_BITSET;
  local_reg[reg_0x05].value = (local_reg[reg_0x05].value & ~REG05_GMMENB);	/* disable gamma */

  switch (dev->model->ccd_type)
    {
    case CCD_HP2300:
    case CCD_HP2400:
      local_reg[reg_0x01].value = REG01_DOGENB;
      local_reg[reg_0x02].value = REG02_ACDCDIS | REG02_HALFSTEP;
      local_reg[reg_0x04].value &= ~REG04_BITSET;	/* disable 16 bits scanning */
      local_reg[reg_0x05].value |= REG05_GMMENB;	/* enable gamma */
      *channels = 1;
      steps = 1;
      local_reg[reg_0x04].value =
	(local_reg[reg_0x04].value & ~REG04_FILTER) | 0x08;
      num_pixels = 2668;
      if (dev->model->motor_type == MOTOR_HP2300)
	{
	  dpi = 150;
	  slope_table[0] = 4480;
	  slope_table[1] = 4480;
	}
      else if (dev->model->motor_type == MOTOR_HP2400)
	{
	  dpi = 200;
	  slope_table[0] = 7210;
	  slope_table[1] = 7210;
	}
      RIE (gl646_send_slope_table
	   (dev, 0, slope_table, local_reg[reg_0x21].value));

      /* motor & movement control */
      local_reg[reg_0x1e].value = 0x80;
      local_reg[reg_0x1d].value |= REG1D_CKMANUAL;
      local_reg[reg_0x1f].value = 0x10;
      lincnt = 2;
      local_reg[reg_0x21].value = 2;	/* table one steps number for forward slope curve of the acc/dec */
      local_reg[reg_0x22].value = 16;	/* steps number of the forward steps for start/stop */

      /* start pixel */
      startpixel = dev->sensor.dummy_pixel + 34;
      local_reg[reg_0x30].value = HIBYTE (startpixel);
      local_reg[reg_0x31].value = LOBYTE (startpixel);

      /* end CCD pixel */
      endpixel = startpixel + num_pixels;
      local_reg[reg_0x32].value = HIBYTE (endpixel);
      local_reg[reg_0x33].value = LOBYTE (endpixel);

      words_per_line = num_pixels / (dev->sensor.optical_res / dpi);
      local_reg[reg_0x35].value = LOBYTE (HIWORD (words_per_line));
      local_reg[reg_0x36].value = HIBYTE (LOWORD (words_per_line));
      local_reg[reg_0x37].value = LOBYTE (LOWORD (words_per_line));

      exposure_time = sanei_genesys_exposure_time (dev, local_reg, dpi),
	local_reg[reg_0x38].value = HIBYTE (exposure_time);
      local_reg[reg_0x39].value = LOBYTE (exposure_time);

      /* monochrome scan */
      *total_size = words_per_line * lincnt;
      break;
    default:
      dpi = 300;
      steps = 0;
      *channels = 3;
      num_pixels = 4 * 300;
      local_reg[reg_0x1e].value = 0xf0 /*0x10 */ ;	/* watch-dog time */
      local_reg[reg_0x1f].value = 0x01;	/* scan feed step for table one in two table mode only */

      lincnt = 1;

      /* motor & movement control */
      local_reg[reg_0x21].value = 0x00;	/* table one steps number for forward slope curve of the acc/dec */
      local_reg[reg_0x22].value = 0x00;	/* steps number of the forward steps for start/stop */

      local_reg[reg_0x28].value = 0x01;	/* PWM duty for lamp control */
      local_reg[reg_0x29].value = 0xff;

      local_reg[reg_0x2e].value = 0x88;	/* set black&white threshold high level */
      local_reg[reg_0x2f].value = 0x78;	/* set black&white threshold low level */

      local_reg[reg_0x38].value = 0x2a /*0x2b */ ;	/* line period (exposure time) */
      local_reg[reg_0x39].value = 0xf8 /*0x44 */ ;
      local_reg[reg_0x35].value = 0x00 /*0x00 */ ;	/* set maximum word size per line, for buffer full control (10800) */
      local_reg[reg_0x36].value = 0x1d;
      local_reg[reg_0x37].value = 0xe3;
      break;
    }

  gl646_setup_sensor (dev, local_reg, 1, 1);

  /* motor & movement control */
  local_reg[reg_0x23].value = local_reg[reg_0x22].value;	/* steps number of the backward steps for start/stop */
  local_reg[reg_0x24].value = local_reg[reg_0x21].value;	/* table one steps number backward slope curve of the acc/dec */


  local_reg[reg_0x25].value = LOBYTE (HIWORD (lincnt));	/* scan line numbers - here one line */
  local_reg[reg_0x26].value = HIBYTE (LOWORD (lincnt));
  local_reg[reg_0x27].value = LOBYTE (LOWORD (lincnt));

  local_reg[reg_0x2c].value = HIBYTE (dpi);
  local_reg[reg_0x2d].value = LOBYTE (dpi);

  local_reg[reg_0x34].value = dev->sensor.dummy_pixel;

  /* set feed steps number of motor move */
  local_reg[reg_0x3d].value = HIWORD (LOBYTE (steps));
  local_reg[reg_0x3e].value = LOWORD (HIBYTE (steps));
  local_reg[reg_0x3f].value = LOWORD (LOBYTE (steps));

/* ST12: 0x60 0x00 0x61 0x00 0x62 0x00 0x63 0x00 0x64 0x00 0x65 0x3f 0x66 0x00 0x67 0x00 0x68 0x51 0x69 0x20 */
/* ST24: 0x60 0x00 0x61 0x00 0x62 0x00 0x63 0x00 0x64 0x00 0x65 0x00 0x66 0x00 0x67 0x00 0x68 0x51 0x69 0x20 */
  local_reg[reg_0x60].value = 0x00;	/* Z1MOD, 60h:61h:(6D b5:b3), remainder for start/stop */
  local_reg[reg_0x61].value = 0x00;	/* (21h+22h)/LPeriod */
  local_reg[reg_0x62].value = 0x00;	/* Z2MODE, 62h:63h:(6D b2:b0), remainder for start scan */
  local_reg[reg_0x63].value = 0x00;	/* (3Dh+3Eh+3Fh)/LPeriod for one-table mode,(21h+1Fh)/LPeriod */
  local_reg[reg_0x64].value = 0x00;	/* motor PWM frequency */

  local_reg[reg_0x66].value = dev->gpo.enable[0] & 0x10;
  local_reg[reg_0x67].value = dev->gpo.enable[1];
  local_reg[reg_0x68].value = dev->gpo.enable[0];
  local_reg[reg_0x69].value = dev->gpo.enable[1];

/* ST12: 0x6a 0x7f 0x6b 0xff 0x6c 0x00 0x6d 0x01 */
/* ST24: 0x6a 0x40 0x6b 0xff 0x6c 0x00 0x6d 0x01 */
  switch (dev->model->motor_type)
    {
    default:
      local_reg[reg_0x65].value = 0x00;	/* PWM duty cycle for table one motor phase (63 = max) */
      local_reg[reg_0x6a].value = 0x40;	/* table two fast moving step type, PWM duty for table two */
      local_reg[reg_0x6b].value = 0x01;	/* table two steps number for acc/dec */
      local_reg[reg_0x6c].value = 0x00;	/* period times for LPeriod, expR,expG,expB, Z1MODE, Z2MODE (one period time) */
      local_reg[reg_0x6d].value = 0x1f;	/* select deceleration steps whenever go home (0), accel/decel stop time (31 * LPeriod) */
      break;
    case MOTOR_HP2300:
    case MOTOR_HP2400:
      local_reg[reg_0x65].value = 0x3f;
      local_reg[reg_0x6a].value = 0x7f;
      local_reg[reg_0x6b].value = 0x02;
      local_reg[reg_0x6d].value = 0x7f;
      break;
    }

  RIE (gl646_set_fe (dev, AFE_INIT));

  RIE (gl646_bulk_write_register
       (dev, local_reg, GENESYS_GL646_MAX_REGS));

  return status;
}


/*
 * this function moves head without scanning, forward, then backward
 * so that the head goes to park position.
 * as a by-product, also check for lock
 */
static SANE_Status
gl646_repark_head (Genesys_Device * dev)
{
  Genesys_Register_Set local_reg[GENESYS_GL646_MAX_REGS + 1];
  u_int16_t slope_table[256];
  unsigned int exposure_time;	/* todo : modify sanei_genesys_exposure_time() */
  SANE_Status status;
  unsigned int steps = 232;
  unsigned int lincnt = 212;
  unsigned int dpi = 600;
  unsigned int endpixel, expected;

  DBG (DBG_proc, "gl646_repark_head\n");

  memset (local_reg, 0, sizeof (local_reg));
  memcpy (local_reg, dev->reg, (GENESYS_GL646_MAX_REGS + 1) * sizeof(Genesys_Register_Set));

  local_reg[reg_0x01].value =
    local_reg[reg_0x01].value & ~REG01_DVDSET & ~REG01_FASTMOD & ~REG01_SCAN;
  local_reg[reg_0x02].value =
    (local_reg[reg_0x02].
     value & ~REG02_FASTFED & ~REG02_STEPSEL) | REG02_HALFSTEP;
  local_reg[reg_0x03].value =
    (local_reg[reg_0x03].value & ~REG03_LAMPTIM) | 0x01;

  gl646_setup_sensor (dev, local_reg, 0, 0);

  local_reg[reg_0x1e].value = 0x10;
  local_reg[reg_0x1f].value = 0x10;
  local_reg[reg_0x20].value = 0x20;

  if (dev->model->motor_type == MOTOR_5345)
    gl646_setup_steps (dev, local_reg, dpi);
  else
    {
      local_reg[reg_0x21].value = 4;
      local_reg[reg_0x22].value = 16;
      local_reg[reg_0x23].value = 16;
      local_reg[reg_0x24].value = 4;
    }

  local_reg[reg_0x25].value = LOBYTE (HIWORD (lincnt));
  local_reg[reg_0x26].value = HIBYTE (LOWORD (lincnt));
  local_reg[reg_0x27].value = LOBYTE (LOWORD (lincnt));

  local_reg[reg_0x2c].value = HIBYTE (dpi);
  local_reg[reg_0x2d].value = LOBYTE (dpi);

  /* start pixel */
  local_reg[reg_0x30].value = HIBYTE (dev->sensor.dummy_pixel);
  local_reg[reg_0x31].value = LOBYTE (dev->sensor.dummy_pixel);


  /* end CCD pixel */
  endpixel = dev->sensor.dummy_pixel + 2400;
  local_reg[reg_0x32].value = HIBYTE (endpixel);
  local_reg[reg_0x33].value = LOBYTE (endpixel);

  local_reg[reg_0x34].value = dev->sensor.dummy_pixel;

  local_reg[reg_0x35].value = LOBYTE (HIWORD (3600));
  local_reg[reg_0x36].value = HIBYTE (LOWORD (3600));	/*  */
  local_reg[reg_0x37].value = LOBYTE (LOWORD (3600));	/* maximum word size per line=1200  */

  local_reg[reg_0x3d].value = LOBYTE (HIWORD (steps));
  local_reg[reg_0x3e].value = HIBYTE (LOWORD (steps));
  local_reg[reg_0x3f].value = LOBYTE (LOWORD (steps));

  exposure_time = 1600;
  local_reg[reg_0x38].value = HIBYTE (exposure_time);
  local_reg[reg_0x39].value = LOBYTE (exposure_time);

  /* Z1MOD = Z2MOD = 0 */
  local_reg[reg_0x60].value = LOBYTE (0);
  local_reg[reg_0x61].value = HIBYTE (0);
  local_reg[reg_0x62].value = LOBYTE (0);
  local_reg[reg_0x63].value = HIBYTE (0);

  local_reg[reg_0x65].value = 0x3f;

  /* todo : maybe move thes in init once for all */
  local_reg[reg_0x66].value = dev->gpo.enable[0] & 0x10;
  local_reg[reg_0x67].value = dev->gpo.enable[1];
  local_reg[reg_0x68].value = dev->gpo.enable[0];
  local_reg[reg_0x69].value = dev->gpo.enable[1];
  local_reg[reg_0x6a].value = 0x7f;
  local_reg[reg_0x6b].value = 0x80;
  local_reg[reg_0x6c].value = 0x00;
  local_reg[reg_0x6d].value = 0x7f;

  /* todo : move this into create_slope_table */
  sanei_genesys_create_slope_table (dev,
				    slope_table,
				    local_reg[reg_0x21].value,
				    local_reg[reg_0x02].value & REG02_STEPSEL,
				    exposure_time, 0, 600, 0);

  sanei_genesys_create_slope_table (dev,
				    dev->slope_table1,
				    local_reg[reg_0x6b].value,
				    (local_reg[reg_0x6a].
				     value & REG6A_FSTPSEL) >> 6, 2700,
				    SANE_FALSE, dev->motor.base_ydpi / 4, 0);

  status =
    gl646_send_slope_table (dev, 0, slope_table, local_reg[reg_0x21].value);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl646_repark_head: failed to send slope table 0: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status =
    gl646_send_slope_table (dev, 1, dev->slope_table1,
			    local_reg[reg_0x6b].value);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl646_repark_head: failed to send slope table 1: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status =
    gl646_bulk_write_register (dev, local_reg, GENESYS_GL646_MAX_REGS);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "gl646_repark_head: failed to send registers: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status = sanei_genesys_start_motor (dev);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "gl646_repark_head: failed to start motor: %s\n",
	   sane_strstatus (status));
      return SANE_STATUS_IO_ERROR;
    }

  expected =
    local_reg[reg_0x3d].value * 65536 + local_reg[reg_0x3e].value * 256 +
    local_reg[reg_0x3f].value;
  do
    {
      usleep (100 * 1000);
      status = sanei_genesys_read_feed_steps (dev, &steps);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "gl646_repark_head: Failed to read feed steps: %s\n",
	       sane_strstatus (status));
	  return status;
	}
    }
  while (steps < expected);

  /* toggle motor flag, put an huge step number and redo move backward */
  status = gl646_park_head (dev, local_reg, 1);
  DBG (DBG_proc, "gl646_repark_head: completed\n");
  return status;
}

/* 
 * initialize ASIC : registers, motor tables, and gamma tables
 * then ensure scanner's head is at home
 */
static SANE_Status
gl646_init (Genesys_Device * dev)
{
  SANE_Status status;
  struct timeval tv;
  u_int8_t val;
  int size;
  u_int8_t data[64];

  DBG_INIT ();
  DBG (DBG_proc, "gl646_init\n");

  /* Check if the device has already been initialized and powered up */
  if (dev->already_initialized)
    {
      RIE (sanei_genesys_get_status (dev, &val));
      if (val & REG41_PWRBIT)
	{
	  DBG (DBG_info, "gl646_init: already initialized\n");
	  return SANE_STATUS_GOOD;
	}
    }


  dev->dark_average_data = NULL;
  dev->white_average_data = NULL;

  dev->settings.color_filter = 0;
  gettimeofday (&tv, NULL);
  dev->init_date = tv.tv_sec;

  switch (dev->model->motor_type)
    {
      /* set to 11111 to spot bugs, sanei_genesys_exposure_time should 
         have obsoleted this field  */
    case MOTOR_5345:
      dev->settings.exposure_time = 11111;
      break;

    case MOTOR_ST24:
      dev->settings.exposure_time = 11000;
      break;
    default:
      dev->settings.exposure_time = 11000;
      break;
    }

  /* Set default values for registers */
  gl646_init_regs (dev);

  /* create slopes */
  if (dev->model->motor_type == MOTOR_5345)
    {
      dev->reg[reg_0x02].value =
	(dev->reg[reg_0x02].value & ~REG02_STEPSEL) | REG02_HALFSTEP;
      sanei_genesys_create_slope_table (dev, dev->slope_table0,
					dev->reg[reg_0x21].value,
					dev->reg[reg_0x02].
					value & REG02_STEPSEL,
					sanei_genesys_exposure_time (dev,
								     dev->reg,
								     150), 0,
					150, 0);
      sanei_genesys_create_slope_table (dev, dev->slope_table1,
					dev->reg[reg_0x6b].value,
					(dev->reg[reg_0x6a].
					 value & REG6A_FSTPSEL) >> 6, 3600, 0,
					200, 0);
    }
  else if (dev->model->motor_type == MOTOR_ST24)
    {
      sanei_genesys_create_slope_table (dev, dev->slope_table0,
					dev->reg[reg_0x21].value,
					dev->reg[reg_0x02].
					value & REG02_STEPSEL,
					dev->settings.exposure_time, 0, 600, 
					0);
      sanei_genesys_create_slope_table (dev, dev->slope_table1,
					dev->reg[reg_0x6b].value,
					(dev->reg[reg_0x6a].
					 value & REG6A_FSTPSEL) >> 6, 1200, 0,
					400, 0);
    }
  else
    {
      sanei_genesys_create_slope_table (dev, dev->slope_table0,
					dev->reg[reg_0x21].value,
					dev->reg[reg_0x02].
					value & REG02_STEPSEL, 250, 0, 600, 0);
      sanei_genesys_create_slope_table (dev, dev->slope_table1,
					dev->reg[reg_0x6b].value,
					(dev->reg[reg_0x6a].
					 value & REG6A_FSTPSEL) >> 6, 250, 0,
					600, 0);
    }

  /* build default gamma tables */
  if (dev->reg[reg_0x05].value & REG05_GMMTYPE)
    size = 16384;
  else
    size = 4096;

  if (dev->sensor.red_gamma_table == NULL)
    {
      dev->sensor.red_gamma_table = (u_int16_t *) malloc (2 * size);
      if (dev->sensor.red_gamma_table == NULL)
	{
	  DBG (DBG_error,
	       "gl646_init: could not allocate memory for gamma table\n");
	  return SANE_STATUS_NO_MEM;
	}
      sanei_genesys_create_gamma_table (dev->sensor.red_gamma_table, size,
					size - 1, size - 1,
					dev->sensor.red_gamma);
    }
  if (dev->sensor.green_gamma_table == NULL)
    {
      dev->sensor.green_gamma_table = (u_int16_t *) malloc (2 * size);
      if (dev->sensor.red_gamma_table == NULL)
	{
	  DBG (DBG_error,
	       "gl646_init: could not allocate memory for gamma table\n");
	  return SANE_STATUS_NO_MEM;
	}
      sanei_genesys_create_gamma_table (dev->sensor.green_gamma_table, size,
					size - 1, size - 1,
					dev->sensor.green_gamma);
    }
  if (dev->sensor.blue_gamma_table == NULL)
    {
      dev->sensor.blue_gamma_table = (u_int16_t *) malloc (2 * size);
      if (dev->sensor.red_gamma_table == NULL)
	{
	  DBG (DBG_error,
	       "gl646_init: could not allocate memory for gamma table\n");
	  return SANE_STATUS_NO_MEM;
	}
      sanei_genesys_create_gamma_table (dev->sensor.blue_gamma_table, size,
					size - 1, size - 1,
					dev->sensor.blue_gamma);
    }

  /* Init device */
  val = 0x04;
  RIE (sanei_usb_control_msg (dev->dn, REQUEST_TYPE_OUT, REQUEST_REGISTER,
			      VALUE_INIT, INDEX, 1, &val));

  /* ASIC reset */
  RIE (sanei_genesys_write_register (dev, 0x0e, 0x00));
  usleep (10000UL);		/* sleep 100 ms */

  /* Write initial registers */
  RIE (gl646_bulk_write_register (dev, dev->reg, GENESYS_GL646_MAX_REGS));

  /* Test ASIC and RAM */
  if (!dev->model->flags & GENESYS_FLAG_LAZY_INIT)
    {
      RIE (gl646_asic_test (dev));
    }

  /* Set analog frontend */
  RIE (gl646_set_fe (dev, AFE_INIT));

  /*  fix for timeouts at init */
  if (dev->model->ccd_type == CCD_5345)
    {
      /* we read data without any pending scan to trigger timeout */
      status = dev->model->cmd_set->bulk_read_data (dev, 0x45, data, 64);
      if (status != SANE_STATUS_GOOD)
	{
	  /* expected error */
	  DBG (DBG_error, "gl646_init: read stream reset ... %s\n",
	       sane_strstatus (status));
	  return status;
	}
      /* restore timeout */
    }

  /* Move home */
  RIE (gl646_slow_back_home (dev, SANE_TRUE));

  /* Init shading data */
  RIE (sanei_genesys_init_shading_data (dev, dev->sensor.sensor_pixels));

  /* ensure head is correctly parked, and check lock */
  if (dev->model->flags & GENESYS_FLAG_REPARK)
    {
      status = gl646_repark_head (dev);
      if (status != SANE_STATUS_GOOD)
	{
	  if (status == SANE_STATUS_INVAL)
	    DBG (DBG_error0,
		 "Your scanner is locked. Please move the lock switch "
		 "to the unlocked position\n");
	  else
	    DBG (DBG_error, "gl646_init: gl646_repark_head failed: %s\n",
		 sane_strstatus (status));
	  return status;
	}
    }

  /* send gamma tables if needed */
  status = gl646_send_gamma_table (dev, 1);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl646_init: failed to send generic gamma tables: %s\n",
	   sane_strstatus (status));
      return status;
    }

  /* initial calibration reg values */
  memcpy (dev->calib_reg, dev->reg, (GENESYS_GL646_MAX_REGS + 1) * sizeof(Genesys_Register_Set));

  /* Set powersaving (default = 15 minutes) */
  RIE (gl646_set_powersaving (dev, 15));
  dev->already_initialized = SANE_TRUE;

  DBG (DBG_proc, "gl646_init: completed\n");
  return SANE_STATUS_GOOD;
}

/** the gl646 command set */
static Genesys_Command_Set gl646_cmd_set = {
  "gl646-generic",		/* the name of this set */

  gl646_init,
  gl646_init_regs_for_warmup,
  gl646_init_regs_for_coarse_calibration,
  gl646_init_regs_for_shading,
  gl646_init_regs_for_scan,

  gl646_get_filter_bit,
  gl646_get_lineart_bit,
  gl646_get_bitset_bit,
  gl646_get_gain4_bit,
  gl646_get_fast_feed_bit,
  gl646_test_buffer_empty_bit,
  gl646_test_motor_flag_bit,

  gl646_bulk_full_size,

  gl646_set_fe,
  gl646_set_powersaving,
  gl646_save_power,
  gl646_set_motor_power,
  gl646_set_lamp_power,

  gl646_begin_scan,
  gl646_end_scan,

  gl646_send_gamma_table,

  gl646_search_start_position,

  gl646_offset_calibration,
  gl646_coarse_gain_calibration,
  gl646_led_calibration,

  gl646_slow_back_home,
  gl646_park_head,

  gl646_bulk_write_register,
  gl646_bulk_write_data,
  gl646_bulk_read_data,
};

SANE_Status
sanei_gl646_init_cmd_set (Genesys_Device * dev)
{
  dev->model->cmd_set = &gl646_cmd_set;
  return SANE_STATUS_GOOD;
}
