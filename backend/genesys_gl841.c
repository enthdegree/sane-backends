/* sane - Scanner Access Now Easy.

   Copyright (C) 2003 Oliver Rauch
   Copyright (C) 2003, 2004 Henning Meier-Geinitz <henning@meier-geinitz.de>
   Copyright (C) 2004 Gerhard Jaeger <gerhard@gjaeger.de>
   Copyright (C) 2004, 2005 Stephane Voltz <stef.dev@free.fr>
   Copyright (C) 2005 Philipp Schmid <philipp8288@web.de>
   Copyright (C) 2005, 2006 Pierre Willenbrock <pierre@pirsoft.dnsalias.org>
   Copyright (C) 2006 Laurent Charpentier <laurent_pubs@yahoo.com>
   
    
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

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"
#include "../include/sane/saneopts.h"

#include "../include/sane/sanei_backend.h"
#include "../include/sane/sanei_config.h"
#include "../include/sane/sanei_usb.h"

#include "genesys.h"

/* Individual bits */
/*adapted to sanei_gl841
 *
 *todo: add macros
 *
 */
#define REG01_CISSET	0x80
#define REG01_DOGENB	0x40
#define REG01_DVDSET	0x20
#define REG01_M16DRAM	0x08
#define REG01_DRAMSEL	0x04
#define REG01_SHDAREA	0x02
#define REG01_SCAN	0x01

#define REG02_NOTHOME	0x80
#define REG02_ACDCDIS	0x40
#define REG02_AGOHOME	0x20
#define REG02_MTRPWR	0x10
#define REG02_FASTFED	0x08
#define REG02_MTRREV	0x04
#define REG02_HOMENEG	0x02
#define REG02_LONGCURV	0x01

#define REG03_LAMPDOG	0x80
#define REG03_AVEENB	0x40
#define REG03_XPASEL	0x20
#define REG03_LAMPPWR	0x10
#define REG03_LAMPTIM	0x0f

#define REG04_LINEART	0x80
#define REG04_BITSET	0x40
#define REG04_AFEMOD	0x30
#define REG04_FILTER	0x0c
#define REG04_FESET	0x03

#define REG04S_AFEMOD   4

#define REG05_DPIHW	0xc0
#define REG05_DPIHW_600	0x00
#define REG05_DPIHW_1200	0x40
#define REG05_DPIHW_2400	0x80
#define REG05_MTLLAMP	0x30
#define REG05_GMMENB	0x08
#define REG05_MTLBASE	0x03

#define REG06_SCANMOD	0xe0
#define REG06_PWRBIT	0x10
#define REG06_GAIN4	0x08
#define REG06_OPTEST	0x07

#define	REG07_SRAMSEL	0x08
#define REG07_FASTDMA	0x04
#define REG07_DMASEL	0x02
#define REG07_DMARDWR	0x01

#define REG08_DECFLAG 	0x40
#define REG08_GMMFFR	0x20
#define REG08_GMMFFG	0x10
#define REG08_GMMFFB	0x08
#define REG08_GMMZR	0x04
#define REG08_GMMZG	0x02
#define REG08_GMMZB	0x01

#define REG09_MCNTSET	0xc0
#define REG09_CLKSET	0x30
#define REG09_BACKSCAN	0x08
#define REG09_ENHANCE	0x04
#define REG09_SHORTTG	0x02
#define REG09_NWAIT	0x01

#define REG09S_MCNTSET  6
#define REG09S_CLKSET   4


#define REG0A_SRAMBUF	0x01

#define REG0D_CLRLNCNT	0x01

#define REG16_CTRLHI	0x80
#define REG16_TOSHIBA	0x40
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
#define REG17S_TGW      0

#define REG18_CNSET	0x80
#define REG18_DCKSEL	0x60
#define REG18_CKTOGGLE	0x10
#define REG18_CKDELAY	0x0c
#define REG18_CKSEL	0x03

#define REG1A_MANUAL3	0x02
#define REG1A_MANUAL1	0x01
#define REG1A_CK4INV	0x08
#define REG1A_CK3INV	0x04
#define REG1A_LINECLP	0x02

#define REG1C_TGTIME    0x07

#define REG1D_CK4LOW	0x80
#define REG1D_CK3LOW	0x40
#define REG1D_CK1LOW	0x20
#define REG1D_TGSHLD	0x1f
#define REG1DS_TGSHLD   0


#define REG1E_WDTIME	0xf0
#define REG1ES_WDTIME   4
#define REG1E_LINESEL	0x0f
#define REG1ES_LINESEL  0

#define REG40_HISPDFLG  0x04
#define REG40_MOTMFLG   0x02
#define REG40_DATAENB   0x01

#define REG41_PWRBIT	0x80
#define REG41_BUFEMPTY	0x40
#define REG41_FEEDFSH	0x20
#define REG41_SCANFSH	0x10
#define REG41_HOMESNR	0x08
#define REG41_LAMPSTS	0x04
#define REG41_FEBUSY	0x02
#define REG41_MOTORENB	0x01

#define REG58_VSMP      0xf8
#define REG58S_VSMP     3
#define REG58_VSMPW     0x07
#define REG58S_VSMPW    0

#define REG59_BSMP      0xf8
#define REG59S_BSMP     3
#define REG59_BSMPW     0x07
#define REG59S_BSMPW    0

#define REG5A_ADCLKINV  0x80
#define REG5A_RLCSEL    0x40
#define REG5A_CDSREF    0x30
#define REG5AS_CDSREF   4
#define REG5A_RLC       0x0f
#define REG5AS_RLC      0

#define REG5E_DECSEL    0xe0
#define REG5ES_DECSEL   5
#define REG5E_STOPTIM   0x1f
#define REG5ES_STOPTIM  0

#define REG60_ZIMOD	0x1f
#define REG61_Z1MOD	0xff
#define REG62_Z1MOD	0xff

#define REG63_Z2MOD	0x1f
#define REG64_Z2MOD	0xff
#define REG65_Z2MOD	0xff

#define REG67_STEPSEL	0xc0
#define REG67_FULLSTEP	0x00
#define REG67_HALFSTEP	0x40
#define REG67_QUATERSTEP	0x80
#define REG67_MTRPWM	0x3f

#define REG68_FSTPSEL	0xc0
#define REG68_FULLSTEP	0x00
#define REG68_HALFSTEP	0x40
#define REG68_QUATERSTEP	0x80
#define REG68_FASTPWM	0x3f

#define REG6B_MULTFILM	0x80
#define REG6B_GPOM13	0x40
#define REG6B_GPOM12	0x20
#define REG6B_GPOM11	0x10
#define REG6B_GPO18	0x02
#define REG6B_GPO17	0x01

#define REG6C_GPIOH	0xff
#define REG6C_GPIOL	0xff


/* we don't need a genesys_sanei_gl841.h yet, declarations aren't numerous enough */
			 /* writable registers *//*adapted to sanei_gl841 */
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

  reg_0x5d,
  reg_0x5e,
  reg_0x5f,
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
  reg_0x6e,
  reg_0x6f,
  reg_0x70,
  reg_0x71,
  reg_0x72,
  reg_0x73,
  reg_0x74,
  reg_0x75,
  reg_0x76,
  reg_0x77,
  reg_0x78,
  reg_0x79,
  reg_0x7a,
  reg_0x7b,
  reg_0x7c,
  reg_0x7d,
  reg_0x7e,
  reg_0x7f,
  reg_0x80,
  reg_0x81,
  reg_0x82,
  reg_0x83,
  reg_0x84,
  reg_0x85,
  reg_0x86,
  reg_0x87,
  GENESYS_GL841_MAX_REGS
};

/****************************************************************************
 Low level function
 ****************************************************************************/

/* ------------------------------------------------------------------------ */
/*                  Read and write RAM, registers and AFE                   */
/* ------------------------------------------------------------------------ */

/* Write to many registers */
/* Note: There is no known bulk register write, 
   this function is sending single registers instead */
static SANE_Status
gl841_bulk_write_register (Genesys_Device * dev,
			   Genesys_Register_Set * reg, size_t elems)
{
  SANE_Status status = SANE_STATUS_GOOD;
  unsigned int i, c;
  u_int8_t buffer[GENESYS_MAX_REGS * 2];

  /* handle differently sized register sets, reg[0x00] is the last one */
  i = 0;
  while ((i < elems) && (reg[i].address != 0))
    i++;

  elems = i;

  DBG (DBG_io, "gl841_bulk_write_register (elems = %lu)\n",
       (u_long) elems);

  for (i = 0; i < elems; i++) {

      buffer[i * 2 + 0] = reg[i].address;
      buffer[i * 2 + 1] = reg[i].value;
      
      DBG (DBG_io2, "reg[0x%02x] = 0x%02x\n", buffer[i * 2 + 0],
	   buffer[i * 2 + 1]);
  }

  for (i = 0; i < elems;) {
      c = elems - i;
      if (c > 32)  /*32 is max. checked that.*/
	  c = 32;
      status =
	  sanei_usb_control_msg (dev->dn, REQUEST_TYPE_OUT, REQUEST_BUFFER,
				 VALUE_SET_REGISTER, INDEX, c * 2, buffer + i * 2);
      if (status != SANE_STATUS_GOOD)
      {
	  DBG (DBG_error,
	       "gl841_bulk_write_register: failed while writing command: %s\n",
	       sane_strstatus (status));
	  return status;
      }

      i += c;
  }

  DBG (DBG_io, "gl841_bulk_write_register: wrote %lu registers\n",
       (u_long) elems);
  return status;
}

/* Write bulk data (e.g. shading, gamma) */
static SANE_Status
gl841_bulk_write_data (Genesys_Device * dev, u_int8_t addr,
			       u_int8_t * data, size_t len)
{
  SANE_Status status;
  size_t size;
  u_int8_t outdata[8];

  DBG (DBG_io, "gl841_bulk_write_data writing %lu bytes\n",
       (u_long) len);

  status =
    sanei_usb_control_msg (dev->dn, REQUEST_TYPE_OUT, REQUEST_REGISTER,
			   VALUE_SET_REGISTER, INDEX, 1, &addr);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl841_bulk_write_data failed while setting register: %s\n",
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
      outdata[2] = VALUE_BUFFER & 0xff;
      outdata[3] = (VALUE_BUFFER >> 8) & 0xff;
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
	       "gl841_bulk_write_data failed while writing command: %s\n",
	       sane_strstatus (status));
	  return status;
	}

      status = sanei_usb_write_bulk (dev->dn, data, &size);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "gl841_bulk_write_data failed while writing bulk data: %s\n",
	       sane_strstatus (status));
	  return status;
	}

      DBG (DBG_io2,
	   "gl841_bulk_write_data wrote %lu bytes, %lu remaining\n",
	   (u_long) size, (u_long) (len - size));

      len -= size;
      data += size;
    }

  DBG (DBG_io, "gl841_bulk_write_data: completed\n");

  return status;
}

/* for debugging transfer rate*/
/*
#include <sys/time.h>
static struct timeval start_time;
static void
starttime(){
    gettimeofday(&start_time,NULL);
}
static void
printtime(char *p) {
    struct timeval t;
    long long int dif;
    gettimeofday(&t,NULL);
    dif = t.tv_sec - start_time.tv_sec;
    dif = dif*1000000 + t.tv_usec - start_time.tv_usec;
    fprintf(stderr,"%s %lluµs\n",p,dif);
}
*/

/* Read bulk data (e.g. scanned data) */
static SANE_Status
gl841_bulk_read_data (Genesys_Device * dev, u_int8_t addr,
			      u_int8_t * data, size_t len)
{
  SANE_Status status;
  size_t size;
  u_int8_t outdata[8];

  DBG (DBG_io, "gl841_bulk_read_data: requesting %lu bytes\n",
       (u_long) len);

  if (len == 0) 
      return SANE_STATUS_GOOD;

  status =
    sanei_usb_control_msg (dev->dn, REQUEST_TYPE_OUT, REQUEST_REGISTER,
			   VALUE_SET_REGISTER, INDEX, 1, &addr);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl841_bulk_read_data failed while setting register: %s\n",
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
	   "gl841_bulk_read_data failed while writing command: %s\n",
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
	   "gl841_bulk_read_data: trying to read %lu bytes of data\n",
	   (u_long) size);

      status = sanei_usb_read_bulk (dev->dn, data, &size);

      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "gl841_bulk_read_data failed while reading bulk data: %s\n",
	       sane_strstatus (status));
	  return status;
	}

      DBG (DBG_io2,
	   "gl841_bulk_read_data read %lu bytes, %lu remaining\n",
	   (u_long) size, (u_long) (len - size));

      len -= size;
      data += size;
    }

  DBG (DBG_io, "gl841_bulk_read_data: completed\n");

  return SANE_STATUS_GOOD;
}

/* Set address for writing data */
static SANE_Status
gl841_set_buffer_address_gamma (Genesys_Device * dev, u_int32_t addr)
{
  SANE_Status status;

  DBG (DBG_io, "gl841_set_buffer_address_gamma: setting address to 0x%05x\n",
       addr & 0xfffffff0);

  addr = addr >> 4;

  status = sanei_genesys_write_register (dev, 0x5c, (addr & 0xff));
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl841_set_buffer_address_gamma: failed while writing low byte: %s\n",
	   sane_strstatus (status));
      return status;
    }

  addr = addr >> 8;
  status = sanei_genesys_write_register (dev, 0x5b, (addr & 0xff));
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl841_set_buffer_address_gamma: failed while writing high byte: %s\n",
	   sane_strstatus (status));
      return status;
    }

  DBG (DBG_io, "gl841_set_buffer_address_gamma: completed\n");

  return status;
}

/* Write bulk data (e.g. gamma) */
static SANE_Status
gl841_bulk_write_data_gamma (Genesys_Device * dev, u_int8_t addr,
			 u_int8_t * data, size_t len)
{
  SANE_Status status;
  size_t size;
  u_int8_t outdata[8];

  DBG (DBG_io, "gl841_bulk_write_data_gamma writing %lu bytes\n",
       (u_long) len);

  status =
    sanei_usb_control_msg (dev->dn, REQUEST_TYPE_OUT, REQUEST_REGISTER,
			   VALUE_SET_REGISTER, INDEX, 1, &addr);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "genesys_bulk_write_data_gamma failed while setting register: %s\n",
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
	       "genesys_bulk_write_data_gamma failed while writing command: %s\n",
	       sane_strstatus (status));
	  return status;
	}

      status = sanei_usb_write_bulk (dev->dn, data, &size);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "genesys_bulk_write_data_gamma failed while writing bulk data: %s\n",
	       sane_strstatus (status));
	  return status;
	}

      DBG (DBG_io2,
	   "genesys_bulk_write_data:gamma wrote %lu bytes, %lu remaining\n",
	   (u_long) size, (u_long) (len - size));

      len -= size;
      data += size;
    }

  DBG (DBG_io, "genesys_bulk_write_data_gamma: completed\n");

  return status;
}


/****************************************************************************
 Mid level functions 
 ****************************************************************************/

static SANE_Bool
gl841_get_fast_feed_bit (Genesys_Register_Set * regs)
{
  Genesys_Register_Set *r = NULL;

  r = sanei_genesys_get_address (regs, 0x02);
  if (r && (r->value & REG02_FASTFED))
    return SANE_TRUE;
  return SANE_FALSE;
}

static SANE_Bool
gl841_get_filter_bit (Genesys_Register_Set * regs)
{
  Genesys_Register_Set *r = NULL;

  r = sanei_genesys_get_address (regs, 0x04);
  if (r && (r->value & REG04_FILTER))
    return SANE_TRUE;
  return SANE_FALSE;
}

static SANE_Bool
gl841_get_lineart_bit (Genesys_Register_Set * regs)
{
  Genesys_Register_Set *r = NULL;

  r = sanei_genesys_get_address (regs, 0x04);
  if (r && (r->value & REG04_LINEART))
    return SANE_TRUE;
  return SANE_FALSE;
}

static SANE_Bool
gl841_get_bitset_bit (Genesys_Register_Set * regs)
{
  Genesys_Register_Set *r = NULL;

  r = sanei_genesys_get_address (regs, 0x04);
  if (r && (r->value & REG04_BITSET))
    return SANE_TRUE;
  return SANE_FALSE;
}

static SANE_Bool
gl841_get_gain4_bit (Genesys_Register_Set * regs)
{
  Genesys_Register_Set *r = NULL;

  r = sanei_genesys_get_address (regs, 0x06);
  if (r && (r->value & REG06_GAIN4))
    return SANE_TRUE;
  return SANE_FALSE;
}

static SANE_Bool
gl841_test_buffer_empty_bit (SANE_Byte val)
{
  if (val & REG41_BUFEMPTY)
    return SANE_TRUE;
  return SANE_FALSE;
}

static SANE_Bool
gl841_test_motor_flag_bit (SANE_Byte val)
{
  if (val & REG41_MOTORENB)
    return SANE_TRUE;
  return SANE_FALSE;
}

/*
 * dumps register set in a human readable format
 * todo : finish all register decoding
 *
 * adapted to sanei_gl841 but not tested at all
 */
static void
sanei_gl841_print_registers (Genesys_Register_Set * reg)
{
  SANE_Int i;
  SANE_Byte v;
  SANE_Int fastmode = 0;
  SANE_Int lperiod;
  SANE_Int cpp = 0;

  lperiod =
    sanei_genesys_read_reg_from_set (reg,
				     0x38) * 256 +
    sanei_genesys_read_reg_from_set (reg, 0x39);
#if 0
  fastmode =
    (sanei_genesys_read_reg_from_set (reg, 0x01) & REG01_FASTMOD) ? 1 : 0;
#endif

  for (i = 0; i < GENESYS_GL841_MAX_REGS; i++)
    {
      v = reg[i].value;
      DBG (DBG_info, "reg 0x%02x: 0x%02x ", reg[i].address, v);
      switch (reg[i].address)
	{
	case 0x01:
	  DBG (DBG_info, "%s, %s, %s, %s, %s, %s, %s, ",	/*%s", */
		   (v & REG01_CISSET) ? "CIS" : "CCD",
		   (v & REG01_DOGENB) ? "watchdog on" : "watchdog off",
		   (v & REG01_DVDSET) ? "shading on" : "shading off",
		   /*(v & REG01_FASTMOD) ? "fastmode on" : "fastmode off", */
		   (v & REG01_M16DRAM) ? "data comp on" : "data comp off",
		   (v & REG01_DRAMSEL) ? "1 MB RAM" : "512 KB RAM",
		   (v & REG01_SHDAREA) ? "shading=scan area" :
		   "shading=total line",
		   (v & REG01_SCAN) ? "enable scan" : "disable scan");
	  break;
	case 0x02:
	  DBG (DBG_info, "%s, %s, %s, %s, %s, %s, %s, %s",
		   (v & REG02_NOTHOME) ? "autohome doesn't work" :
		   "autohome works",
		   (v & REG02_ACDCDIS) ? "backtrack off" : "backtrack on",
		   (v & REG02_AGOHOME) ? "autohome on" : "autohome off",
		   (v & REG02_MTRPWR) ? "motor on" : "motor off",
		   (v & REG02_FASTFED) ? "2 tables" : "1 table",
		   (v & REG02_MTRREV) ? "reverse" : "forward",
		   (v & REG02_HOMENEG) ? "indicate home sensor falling edge" :
		   "indicate home sensor rising edge",
		   (v & REG02_LONGCURV) ?
		   "deceleration curve fast mode is table 5" :
		   "deceleration curve fast mode is table 4");
	  break;
	case 0x03:
	  DBG (DBG_info, "%s, %s, %s, %s, %s, lamptime: %d pixels",
		   (v & REG03_LAMPDOG) ? "lamp sleeping mode on" :
		   "lamp sleefping mode off",
		   (v & REG03_AVEENB) ? "dpi average" : "dpi deletion",
		   (v & REG03_XPASEL) ? "TA lamp" : "flatbed lamp",
		   (v & REG03_LAMPPWR) ? "lamp on" : "lamp off",
		   "lamp timer on",
		   v & REG03_LAMPTIM * (fastmode + 1) * 65536 * lperiod);
	  break;
	case 0x04:
	  DBG (DBG_info, "%s, %s, AFEMOD: %d, %s, %s",
		   /*(v & REG04_LINEART) ? "lineart on" : "lineart off", */
		   "sanei_gl841 manual rev 1.7 is strange here ",
		   (v & REG04_BITSET) ? "image data 16 bits" :
		   "image data 8 bits",
		   ((v & REG04_AFEMOD) >> 4),
		   ((v & REG04_FILTER) >> 2 ==
		    0) ? "color filter" : ((v & REG04_FILTER) >> 2 ==
					   1) ? "red filter" : ((v &
								 REG04_FILTER)
								>> 2 ==
								2) ?
		   "green filter" : "blue filter",
		   ((v & REG04_FESET) <
		   2) ? (((v & REG04_FESET) == 0) ? "ESIC type 1" :
			"ESIC type 2") : (((v & REG04_FESET) ==
					  2) ? "ADI type" : "reserved"));
	  break;
	case 0x05:
	  DBG (DBG_info, "%s, %s, %s, %s, %s, %s",
		   ((v & REG05_DPIHW) >> 6) == 0 ? "opt. sensor dpi: 600" :
		   ((v & REG05_DPIHW) >> 6) ==
		   1 ? "opt. sensor dpi: 1200 dpi" : ((v & REG05_DPIHW) >>
						      6) ==
		   2 ? "opt. sensor dpi: 2400 dpi" :
		   "opt. sensor dpi: reserved",
		   "lamp time out",
		   ((v & REG05_MTLLAMP) >> 4) < 2
		   ? (((v & REG05_MTLLAMP) >> 4) == 0
		      ? "1*LAMPTIM" : "2*LAMPTIM")
		   : ((v & REG05_MTLLAMP) >> 4) == 2
		   ? "4*LAMPTIM" : "reserved",
		   (v & REG05_GMMENB) ? "gamma correction on" :
		   "gamma correction off",
		   "pixes number under each system pixel time: ",
		   (v & REG05_MTLBASE) < 2
		   ? ((v & REG05_MTLBASE) == 0
		      ? "1" : "2") : (v & REG05_MTLBASE) == 2 ? "3" : "4");
	  /* I don't now if this works cause in gl646 the unit of 
	     measurement is clocks/pixel and in sanei_gl841 it is 
	     pixel/system pixel time */
	  {
	    switch (v & REG05_MTLBASE)
	      {
	      case 0:
		cpp = 1;
		break;
	      case 1:
		cpp = 2;
		break;
	      case 2:
		cpp = 3;
		break;
	      case 3:
		cpp = 4;
		break;
	      }
	  }
	  break;
	case 0x06:
	  DBG (DBG_info, "12 clk/pixel normal for scanning, %s, %s, %s, %s",
		   ((v & REG06_SCANMOD >> 5) > 5) ?
		   ((v & REG06_SCANMOD >> 5) == 4) ?
		   " 6 clk/pixel fast mode  "
		   : ((v & REG06_SCANMOD >> 5) == 5) ?
		   " 15 clk/pixel 16 color"
		   : " 18 clk/pixel 16 color"
		   :
		   ((v & REG06_SCANMOD >> 5) < 2) ?
		   ((v & REG06_SCANMOD >> 5) == 0) ?
		   "12 clk/pixel normal for scanning"
		   : "12 clk/pixel bypass for scanning"
		   :
		   " reserved",
		   (v & REG06_PWRBIT) ? "turn power on" :
		   "don't turn power on",
		   (v & REG06_GAIN4) ? "digital shading gain=4 times system" :
		   "digital shading gain=8 times system",
		   (v & REG06_OPTEST) ==
		   0 ? "ASIC test mode: off" : (v & REG06_OPTEST) == 1 ?
		   "ASIC test mode: simulation, motorgo" :
		   (v & REG06_OPTEST) == 2 ?
		   "ASIC test mode: image, pixel count" :
		   (v & REG06_OPTEST) == 3 ?
		   "ASIC test mode: image, line count" :
		   (v & REG06_OPTEST) == 4 ?
		   "ASIC test mode: simulation, counter + adder" :
		   (v & REG06_OPTEST) == 5 ?
		   "ASIC test mode: CCD TG" : "ASIC test mode: reserved");
	  break;
	case 0x07:
	  DBG (DBG_info, "%s, %s, %s, %s",
		   (v & REG07_SRAMSEL) ? "DMA access for SRAM" :
		   "DMA access for DRAM",
		   (v & REG07_FASTDMA) ? "2 clocks/access" :
		   "4 clocks/access",
		   (v & REG07_DMASEL) ? "DMA access for DRAM" :
		   "MPU access for DRAM",
		   (v & REG07_DMARDWR) ? "DMA read DRAM" : "DMA write DRAM");
	  break;
	case 0x08:
	  DBG (DBG_info, "%s, %s, %s, %s, %s, %s, %s",
		   (v & REG08_DECFLAG) ? "gamma table is decrement type" :
		   "gamma table is increment type",
		   (v & REG08_GMMFFR) ?
		   "red channel Gamma table address FFH is special type"
		   : " ",
		   (v & REG08_GMMFFG) ?
		   "green channel Gamma table address FFH is special type"
		   : " ",
		   (v & REG08_GMMFFB) ?
		   "blue channel Gamma table address FFH is special type"
		   : " ",
		   (v & REG08_GMMZR) ?
		   "red channel Gamma table address 00H is special type"
		   : " ",
		   (v & REG08_GMMZG) ?
		   "green channel Gamma table address 0H is special type"
		   : " ",
		   (v & REG08_GMMZB) ?
		   "blue channel Gamma table address 00H is special type"
		   : " ");
	  break;
	case 0x09:
	  DBG (DBG_info, "%s, %s, %s, %s, %s, %s",
		   ((v & REG09_MCNTSET) == 0) ? "pixel count" :
		   ((v & REG09_MCNTSET) == 1) ? "system clock*2" :
		   ((v & REG09_MCNTSET) == 2) ? "system clock*3" :
		   "system clock*4",
		   ((v & REG09_CLKSET) == 0) ? "24MHz" :
		   ((v & REG09_CLKSET) == 1) ? "30MHz" :
		   ((v & REG09_CLKSET) == 2) ? "40MHz" :
		   "48MHz",
		   (v & REG09_BACKSCAN) ?
		   "backward scan function"
		   : "forward scan function ",
		   (v & REG09_ENHANCE) ?
		   "enhance EPP interfgace speed for USB2.0 "
		   : "select normal EPP interface speed for USB2.0 ",
		   (v & REG09_SHORTTG) ?
		   "enable short CCD SH(TG) period for film scanning"
		   : " ",
		   (v & REG09_NWAIT) ?
		   "delay nWait (H_BUSY) one clock" : " ");
	  break;
	case 0x0a:
	  DBG (DBG_info, "%s",
		   (v & REG0A_SRAMBUF) ?
		   "select external SRAM as the image buffer" :
		   "select external DRAM as the image buffer");
	  break;
	case 0x0d:
	  DBG (DBG_info, "scanner command: %s",
		   (v & REG0D_CLRLNCNT) ? "clear SCANCNT(Reg4b,Reg4c,Reg4d)" :
		   "don't clear SCANCNT");
	  break;
	case 0x0e:
	  DBG (DBG_info, "scanner software reset");
	  break;
	case 0x0f:
	  DBG (DBG_info, "start motor move");
	  break;
	case 0x10:
	  DBG (DBG_info, "red exposure time hi");
	  break;
	case 0x11:
	  DBG (DBG_info, "red exposure time lo (total: 0x%x)",
		   v + 256 * sanei_genesys_read_reg_from_set (reg, 0x10));
	  break;
	case 0x12:
	  DBG (DBG_info, "green exposure time hi");
	  break;
	case 0x13:
	  DBG (DBG_info, "green exposure time lo (total: 0x%x)",
		   v + 256 * sanei_genesys_read_reg_from_set (reg, 0x12));
	  break;
	case 0x14:
	  DBG (DBG_info, "blue exposure time hi");
	  break;
	case 0x15:
	  DBG (DBG_info, "blue exposure time lo (total: 0x%x)",
		   v + 256 * sanei_genesys_read_reg_from_set (reg, 0x14));
	  break;
	case 0x16:
	  DBG (DBG_info, "%s, %s, %s, %s, %s, %s, %s, %s",
		   (v & REG16_CTRLHI) ? "CCD CP & RS hi when TG high" :
		   "CCD CP & RS lo when TG high",
		   (v & REG16_TOSHIBA) ? "image sensor is TOSHIBA CIS" :
		   " ",
		   (v & REG16_TGINV) ? "inverse CCD TG" : "normal CCD TG",
		   (v & REG16_CK1INV) ? "inverse CCD clock 1" :
		   "normal CCD clock 1",
		   (v & REG16_CK2INV) ? "inverse CCD clock 2" :
		   "normal CCD clock 2",
		   (v & REG16_CTRLINV) ? "inverse CCD CP & RS" :
		   "normal CCD CP & RS",
		   (v & REG16_CKDIS) ? "CCD TG position clock 1/2 signal off"
		   : "CCD TG position clock 1/2 signal on",
		   (v & REG16_CTRLDIS) ? "CCD TG position CP & RS signals off"
		   : "CCD TG position CP & RS signals off");
	  break;
	case 0x17:
	  DBG (DBG_info, "%s, CCD TG width: %0x",
		   ((v & REG17_TGMODE) >> 6) ==
		   0x00 ? "CCD TG without dummy line" : ((v & REG17_TGMODE) >>
							 6) ==
		   0x01 ? "CCD TG with dummy line" : "CCD TG reserved",
		   (v & REG17_TGW));
	  break;
	case 0x18:
	  DBG (DBG_info, "%s, %d time CCD clock speed for dummy line, %s, "
		   "delay %d system clocks for CCD clock 1/2, %d time CCD clock speed for capture image",
		   (v & REG18_CNSET) ? "TG and clock canon CIS style" :
		   "TG and clock non-canon CIS style",
		   ((v & REG18_DCKSEL) >> 5) + 1,
		   (v & REG18_CKTOGGLE) ?
		   "half cycle per pixel for CCD clock 1/2" :
		   "one cycle per pixel for CCD clock 1/2",
		   ((v & REG18_CKDELAY) >> 2), ((v & REG18_CKSEL) + 1));

	  break;
	case 0x19:
	  DBG (DBG_info, "dummy line exposure time (0x%x pixel times)",
		   v * 256);
	  break;
	case 0x1a:
	  DBG (DBG_info, "%s, %s, %s, %s, %s",
		   (v & REG1A_MANUAL3) ?
		   "CCD clock3,clock4 manual output"
		   : "CCD clock3,clock4 automatic output",
		   (v & REG1A_MANUAL1) ?
		   "CCD clock1,clock2 manual output"
		   : "CCD clock1,clock2 automatic output",
		   (v & REG1A_CK4INV) ?
		   "reverse CCD Clock4"
		   : " ",
		   (v & REG1A_CK3INV) ?
		   "reverse CCD Clock3"
		   : " ",
		   (v & REG1A_LINECLP) ?
		   "CCD line clamping" : "CCD pixel clamping");
	  break;
	case 0x1b:
	  DBG (DBG_info, "reserved");
	  break;
	}
      DBG (DBG_info, "\n");
    }
}

/** copy sensor specific settings */
/* *dev  : device infos
   *regs : registers to be set
   extended : do extended set up
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
sanei_gl841_setup_sensor (Genesys_Device * dev,
			  Genesys_Register_Set * regs,
			  SANE_Bool extended, SANE_Bool half_ccd)
{
  Genesys_Register_Set *r;
  int i;

  DBG (DBG_proc, "gl841_setup_sensor\n");

  r = sanei_genesys_get_address (regs, 0x70);
  for (i = 0; i < 4; i++, r++)
    r->value = dev->sensor.regs_0x08_0x0b[i];

  r = sanei_genesys_get_address (regs, 0x16);
  for (i = 0x06; i < 0x0a; i++, r++)
    r->value = dev->sensor.regs_0x10_0x1d[i];

/*TODO the ommitted values may be necessary for ccd*/
/*  r = sanei_genesys_get_address (regs, 0x);
  for (i = 0x0a; i < 0x0e; i++, r++)
  r->value = dev->sensor.regs_0x10_0x1d[i];*/
  r = sanei_genesys_get_address (regs, 0x1d);
  r->value = 0x02;

  r = sanei_genesys_get_address (regs, 0x52);
  for (i = 0; i < 9; i++, r++)
    r->value = dev->sensor.regs_0x52_0x5e[i];

  /* don't go any further if no extended setup */
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
	  r = sanei_genesys_get_address (regs, 0x70);
	  r->value = 0x00;
	  r = sanei_genesys_get_address (regs, 0x71);
	  r->value = 0x05;
	  r = sanei_genesys_get_address (regs, 0x72);
	  r->value = 0x06;
	  r = sanei_genesys_get_address (regs, 0x73);
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
	  r = sanei_genesys_get_address (regs, 0x70);
	  r->value = 0x16;
	  r = sanei_genesys_get_address (regs, 0x71);
	  r->value = 0x00;
	  r = sanei_genesys_get_address (regs, 0x72);
	  r->value = 0x01;
	  r = sanei_genesys_get_address (regs, 0x73);
	  r->value = 0x03;
	  /* manual clock programming */
	  r = sanei_genesys_get_address (regs, 0x1d);
	  r->value |= 0x80;
	}
      else
	{
	  r = sanei_genesys_get_address (regs, 0x70);
	  r->value = 1;
	  r = sanei_genesys_get_address (regs, 0x71);
	  r->value = 3;
	  r = sanei_genesys_get_address (regs, 0x72);
	  r->value = 4;
	  r = sanei_genesys_get_address (regs, 0x73);
	  r->value = 6;
	}
      r = sanei_genesys_get_address (regs, 0x58);
      r->value = 0x80 | (r->value & 0x03);	/* VSMP=16 */
      return;
    }
}

/** Test if the ASIC works 
 */
/*TODO: make this functional*/
static SANE_Status
sanei_gl841_asic_test (Genesys_Device * dev)
{
  SANE_Status status;
  u_int8_t val;
  u_int8_t *data;
  u_int8_t *verify_data;
  size_t size, verify_size;
  unsigned int i;

  DBG (DBG_proc, "sanei_gl841_asic_test\n");

  return SANE_STATUS_INVAL;

  /* set and read exposure time, compare if it's the same */
  status = sanei_genesys_write_register (dev, 0x38, 0xde);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "sanei_gl841_asic_test: failed to write register: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status = sanei_genesys_write_register (dev, 0x39, 0xad);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "sanei_gl841_asic_test: failed to write register: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status = sanei_genesys_read_register (dev, 0x38, &val);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "sanei_gl841_asic_test: failed to read register: %s\n",
	   sane_strstatus (status));
      return status;
    }
  if (val != 0xde)		/* value of register 0x38 */
    {
      DBG (DBG_error,
	   "sanei_gl841_asic_test: register contains invalid value\n");
      return SANE_STATUS_IO_ERROR;
    }

  status = sanei_genesys_read_register (dev, 0x39, &val);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "sanei_gl841_asic_test: failed to read register: %s\n",
	   sane_strstatus (status));
      return status;
    }
  if (val != 0xad)		/* value of register 0x39 */
    {
      DBG (DBG_error,
	   "sanei_gl841_asic_test: register contains invalid value\n");
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
      DBG (DBG_error, "sanei_gl841_asic_test: could not allocate memory\n");
      return SANE_STATUS_NO_MEM;
    }

  verify_data = (u_int8_t *) malloc (verify_size);
  if (!verify_data)
    {
      free (data);
      DBG (DBG_error, "sanei_gl841_asic_test: could not allocate memory\n");
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
      DBG (DBG_error,
	   "sanei_gl841_asic_test: failed to set buffer address: %s\n",
	   sane_strstatus (status));
      free (data);
      free (verify_data);
      return status;
    }

/*  status = gl841_bulk_write_data (dev, 0x3c, data, size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "sanei_gl841_asic_test: failed to bulk write data: %s\n",
	   sane_strstatus (status));
      free (data);
      free (verify_data);
      return status;
      }*/

  status = sanei_genesys_set_buffer_address (dev, 0x0000);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "sanei_gl841_asic_test: failed to set buffer address: %s\n",
	   sane_strstatus (status));
      free (data);
      free (verify_data);
      return status;
    }

  status =
    gl841_bulk_read_data (dev, 0x45, (u_int8_t *) verify_data,
				  verify_size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "sanei_gl841_asic_test: failed to bulk read data: %s\n",
	   sane_strstatus (status));
      free (data);
      free (verify_data);
      return status;
    }

  /* todo: why i + 2 ? */
  for (i = 0; i < size; i++)
    {
      if (verify_data[i] != data[i])
	{
	  DBG (DBG_error, "sanei_gl841_asic_test: data verification error\n");
	  DBG (DBG_info, "0x%.8x: got %.2x %.2x %.2x %.2x, expected %.2x %.2x %.2x %.2x\n", 
	       i, 
	       verify_data[i], 
	       verify_data[i+1], 
	       verify_data[i+2], 
	       verify_data[i+3], 
	       data[i], 
	       data[i+1], 
	       data[i+2], 
	       data[i+3]);
	  free (data);
	  free (verify_data);
	  return SANE_STATUS_IO_ERROR;
	}
    }

  free (data);
  free (verify_data);

  DBG (DBG_info, "sanei_gl841_asic_test: completed\n");

  return SANE_STATUS_GOOD;
}

/* returns the max register bulk size */
static int
gl841_bulk_full_size (void)
{
  return GENESYS_GL841_MAX_REGS;
}

/*
 * Set all registers to default values 
 * (function called only once at the beginning)
 */
static void
gl841_init_registers (Genesys_Device * dev)
{
  int nr, addr;

  DBG (DBG_proc, "gl841_init_registers\n");

  nr = 0;
  memset (dev->reg, 0, GENESYS_MAX_REGS * sizeof (Genesys_Register_Set));

  for (addr = 1; addr <= 0x0a; addr++)
    dev->reg[nr++].address = addr;
  for (addr = 0x10; addr <= 0x27; addr++)
    dev->reg[nr++].address = addr;
  dev->reg[nr++].address = 0x29;
  for (addr = 0x2c; addr <= 0x39; addr++)
    dev->reg[nr++].address = addr;
  for (addr = 0x3d; addr <= 0x3f; addr++)
    dev->reg[nr++].address = addr;
  for (addr = 0x52; addr <= 0x5a; addr++)
    dev->reg[nr++].address = addr;
  for (addr = 0x5d; addr <= 0x87; addr++)
    dev->reg[nr++].address = addr;


  dev->reg[reg_0x01].value = 0x20;	/* (enable shading), CCD, color, 1M */
  dev->reg[reg_0x01].value |= REG01_CISSET;
  
 dev->reg[reg_0x02].value = 0x30 /*0x38 */ ;	/* auto home, one-table-move, full step */
  dev->reg[reg_0x02].value |= REG02_AGOHOME;
  dev->reg[reg_0x02].value |= REG02_MTRPWR;
  dev->reg[reg_0x02].value |= REG02_FASTFED;
  
  dev->reg[reg_0x03].value = 0x1f /*0x17 */ ;	/* lamp on */
  dev->reg[reg_0x03].value |= REG03_AVEENB;

  dev->reg[reg_0x04].value |= 1 << REG04S_AFEMOD;

  dev->reg[reg_0x05].value = 0x00;	/* disable gamma, 24 clocks/pixel */
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


  dev->reg[reg_0x06].value |= REG06_PWRBIT;
  dev->reg[reg_0x06].value |= REG06_GAIN4;
  
  dev->reg[reg_0x09].value |= 1 << REG09S_CLKSET;

  dev->reg[reg_0x1e].value = 0xf0;	/* watch-dog time */

  dev->reg[reg_0x17].value |= 1 << REG17S_TGW;

  dev->reg[reg_0x19].value = 0x50;

  dev->reg[reg_0x1d].value |= 1 << REG1DS_TGSHLD;

  dev->reg[reg_0x1e].value |= 1 << REG1ES_WDTIME;

/*SCANFED*/
  dev->reg[reg_0x1f].value = 0x01;

/*BUFSEL*/
  dev->reg[reg_0x20].value = 0x20;
  
/*LAMPPWM*/
  dev->reg[reg_0x29].value = 0xff;

/*BWHI*/
  dev->reg[reg_0x2e].value = 0x80;

/*BWLOW*/
  dev->reg[reg_0x2f].value = 0x80;

/*LPERIOD*/
  dev->reg[reg_0x38].value = 0x4f;
  dev->reg[reg_0x39].value = 0xc1;

/*VSMPW*/
  dev->reg[reg_0x58].value |= 3 << REG58S_VSMPW;

/*BSMPW*/
  dev->reg[reg_0x59].value |= 3 << REG59S_BSMPW;

/*RLCSEL*/
  dev->reg[reg_0x5a].value |= REG5A_RLCSEL;

/*STOPTIM*/
  dev->reg[reg_0x5e].value |= 0x2 << REG5ES_STOPTIM;

  sanei_gl841_setup_sensor (dev, dev->reg, 0, 0); 



  dev->reg[reg_0x6c].value = dev->gpo.value[0];
  dev->reg[reg_0x6d].value = dev->gpo.value[1];
  dev->reg[reg_0x6e].value = dev->gpo.enable[0];
  dev->reg[reg_0x6f].value = dev->gpo.enable[1];

  dev->reg[reg_0x6b].value |= REG6B_GPO18;
  dev->reg[reg_0x6b].value &= ~REG6B_GPO17;

  DBG (DBG_proc, "gl841_init_registers complete\n");
}

/* Send slope table for motor movement 
   slope_table in machine byte order
 */
static SANE_Status
gl841_send_slope_table (Genesys_Device * dev, int table_nr,
			      u_int16_t * slope_table, int steps)
{
  int dpihw;
  int start_address;
  SANE_Status status;
  u_int8_t *table;
/*#ifdef WORDS_BIGENDIAN*/
  int i;
/*#endif*/

  DBG (DBG_proc, "gl841_send_slope_table (table_nr = %d, steps = %d)\n",
       table_nr, steps);

  dpihw = dev->reg[reg_0x05].value >> 6;

  if (dpihw == 0)		/* 600 dpi */
    start_address = 0x08000;
  else if (dpihw == 1)		/* 1200 dpi */
    start_address = 0x10000;
  else if (dpihw == 2)		/* 2400 dpi */
    start_address = 0x20000;
  else				/* reserved */
    return SANE_STATUS_INVAL;

/*#ifdef WORDS_BIGENDIAN*/
  table = (u_int8_t*)malloc(steps * 2);
  for(i = 0; i < steps; i++) {
      table[i * 2] = slope_table[i] & 0xff;
      table[i * 2 + 1] = slope_table[i] >> 8;
  }
/*#else
  table = (u_int8_t*)slope_table;
  #endif*/

  status =
    sanei_genesys_set_buffer_address (dev, start_address + table_nr * 0x200);
  if (status != SANE_STATUS_GOOD)
    {
/*#ifdef WORDS_BIGENDIAN*/
      free(table);
/*#endif*/
      DBG (DBG_error,
	   "gl841_send_slope_table: failed to set buffer address: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status =
    gl841_bulk_write_data (dev, 0x3c, (u_int8_t *) table,
				   steps * 2);
  if (status != SANE_STATUS_GOOD)
    {
/*#ifdef WORDS_BIGENDIAN*/
      free(table);
/*#endif*/
      DBG (DBG_error,
	   "gl841_send_slope_table: failed to send slope table: %s\n",
	   sane_strstatus (status));
      return status;
    }

/*#ifdef WORDS_BIGENDIAN*/
  free(table);
/*#endif*/
  DBG (DBG_proc, "gl841_send_slope_table: completed\n");
  return status;
}
 
/* Set values of analog frontend */
static SANE_Status
gl841_set_fe (Genesys_Device * dev, u_int8_t set)
{
  SANE_Status status;
  int i;
  u_int8_t val;

  DBG (DBG_proc, "gl841_set_fe (%s)\n",
       set == 1 ? "init" : set == 2 ? "set" : set ==
       3 ? "powersave" : "huh?");

  if ((dev->reg[reg_0x04].value & REG04_FESET) != 0x00)
    {
      DBG (DBG_proc, "gl841_set_fe(): unsupported frontend type %d\n",
	   dev->reg[reg_0x04].value & REG04_FESET);
      return SANE_STATUS_UNSUPPORTED;
    }

  if (set == AFE_INIT)
    {
      DBG (DBG_proc, "gl841_set_fe(): setting DAC %u\n",
	   dev->model->dac_type);
      sanei_genesys_init_fe (dev);

      /* reset only done on init */
      status = sanei_genesys_fe_write_data (dev, 0x04, 0x80);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "gl841_set_fe: reset fe failed: %s\n",
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
		       "gl841_set_fe failed resetting frontend: %s\n",
		       sane_strstatus (status));
		  return status;
		}
	    }
	}
    }

  DBG (DBG_proc, "gl841_set_fe(): frontend reset complete\n");

  if (set == AFE_POWER_SAVE)
    {
      status = sanei_genesys_fe_write_data (dev, 0x01, 0x02);
      if (status != SANE_STATUS_GOOD)
	DBG (DBG_error, "gl841_set_fe: writing data failed: %s\n",
	     sane_strstatus (status));
      return status;
    }

  /* todo :  base this test on cfg reg3 or a CCD family flag to be created */
  /*if (dev->model->ccd_type!=CCD_HP2300 && dev->model->ccd_type!=CCD_HP2400) */
  {

    status = sanei_genesys_fe_write_data (dev, 0x00, dev->frontend.reg[0]);
    if (status != SANE_STATUS_GOOD)
      {
	DBG (DBG_error, "gl841_set_fe: writing reg0 failed: %s\n",
	     sane_strstatus (status));
	return status;
      }
    status = sanei_genesys_fe_write_data (dev, 0x02, dev->frontend.reg[2]);
    if (status != SANE_STATUS_GOOD)
      {
	DBG (DBG_error, "gl841_set_fe: writing reg2 failed: %s\n",
	     sane_strstatus (status));
	return status;
      }
  }

  status = sanei_genesys_fe_write_data (dev, 0x01, dev->frontend.reg[1]);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "gl841_set_fe: writing reg1 failed: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status = sanei_genesys_fe_write_data (dev, 0x03, dev->frontend.reg[3]);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "gl841_set_fe: writing reg3 failed: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status = sanei_genesys_fe_write_data (dev, 0x06, dev->frontend.reg2[0]);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "gl841_set_fe: writing reg6 failed: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status = sanei_genesys_fe_write_data (dev, 0x08, dev->frontend.reg2[1]);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "gl841_set_fe: writing reg8 failed: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status = sanei_genesys_fe_write_data (dev, 0x09, dev->frontend.reg2[2]);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "gl841_set_fe: writing reg9 failed: %s\n",
	   sane_strstatus (status));
      return status;
    }

  for (i = 0; i < 3; i++)
      {
	status =
	  sanei_genesys_fe_write_data (dev, 0x24 + i, dev->frontend.sign[i]);
	if (status != SANE_STATUS_GOOD)
	  {
	    DBG (DBG_error,
		 "gl841_set_fe: writing sign[%d] failed: %s\n", i,
		 sane_strstatus (status));
	    return status;
	  }

	status =
	  sanei_genesys_fe_write_data (dev, 0x28 + i, dev->frontend.gain[i]);
	if (status != SANE_STATUS_GOOD)
	  {
	    DBG (DBG_error,
		 "gl841_set_fe: writing gain[%d] failed: %s\n", i,
		 sane_strstatus (status));
	    return status;
	  }

	status =
	  sanei_genesys_fe_write_data (dev, 0x20 + i,
				       dev->frontend.offset[i]);
	if (status != SANE_STATUS_GOOD)
	  {
	    DBG (DBG_error,
		 "gl841_set_fe: writing offset[%d] failed: %s\n", i,
		 sane_strstatus (status));
	    return status;
	  }
      }


  DBG (DBG_proc, "gl841_set_fe: completed\n");

  return SANE_STATUS_GOOD;
}

#define MOTOR_FLAG_AUTO_GO_HOME             1
#define MOTOR_FLAG_DISABLE_BUFFER_FULL_MOVE 2

#define MOTOR_ACTION_FEED       1
#define MOTOR_ACTION_GO_HOME    2
#define MOTOR_ACTION_HOME_FREE  3

/* setup motor for given parameters */
static SANE_Status
gl841_init_motor_regs_off(Genesys_Device * dev,
			  Genesys_Register_Set * reg,
			  unsigned int scan_lines/*lines, scan resolution*/
    ) 
{
    unsigned int feedl;
    Genesys_Register_Set * r;

    DBG (DBG_proc, "gl841_init_motor_regs_off : scan_lines=%d\n",
	 scan_lines);

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
    r = sanei_genesys_get_address (reg, 0x5e);	
    r->value &= ~0xe0;

    r = sanei_genesys_get_address (reg, 0x25);
    r->value = (scan_lines >> 16) & 0xf;
    r = sanei_genesys_get_address (reg, 0x26);
    r->value = (scan_lines >> 8) & 0xff;
    r = sanei_genesys_get_address (reg, 0x27);	
    r->value = scan_lines & 0xff;

    r = sanei_genesys_get_address (reg, 0x02);
    r->value &= ~0x01; /*LONGCURV OFF*/
    r->value &= ~0x80; /*NOT_HOME OFF*/

    r->value &= ~0x10;

    r->value &= ~0x06;

    r->value &= ~0x08;

    r->value &= ~0x20;

    r->value &= ~0x40;

    r = sanei_genesys_get_address (reg, 0x67);	
    r->value = 0x3f;

    r = sanei_genesys_get_address (reg, 0x68);
    r->value = 0x3f;

    r = sanei_genesys_get_address (reg, 0x21);
    r->value = 0;
    
    r = sanei_genesys_get_address (reg, 0x24);
    r->value = 0;
    
    r = sanei_genesys_get_address (reg, 0x69);
    r->value = 0;
    
    r = sanei_genesys_get_address (reg, 0x6a);
    r->value = 0;
    
    r = sanei_genesys_get_address (reg, 0x5f);
    r->value = 0;


    DBG (DBG_proc, "gl841_init_motor_regs_off : completed. \n");

    return SANE_STATUS_GOOD;	
}

static SANE_Status
gl841_init_motor_regs(Genesys_Device * dev,
		      Genesys_Register_Set * reg,
		      unsigned int feed_steps,/*1/base_ydpi*/
/*maybe float for half/quarter step resolution?*/
		      unsigned int action,
		      unsigned int flags) 
{
    SANE_Status status;
    unsigned int fast_exposure;
    int use_fast_fed = 0;
    u_int16_t fast_slope_table[256];
    unsigned int fast_slope_time;
    unsigned int fast_slope_steps = 0;
    unsigned int feedl;
    Genesys_Register_Set * r;
/*number of scan lines to add in a scan_lines line*/

    DBG (DBG_proc, "gl841_init_motor_regs : feed_steps=%d, action=%d, flags=%x\n",
	 feed_steps,
	 action,
	 flags);

    memset(fast_slope_table,0xff,512);
    
    gl841_send_slope_table (dev, 0, fast_slope_table, 256);
    gl841_send_slope_table (dev, 1, fast_slope_table, 256);
    gl841_send_slope_table (dev, 2, fast_slope_table, 256);
    gl841_send_slope_table (dev, 3, fast_slope_table, 256);
    gl841_send_slope_table (dev, 4, fast_slope_table, 256);


    if (action == MOTOR_ACTION_FEED || action == MOTOR_ACTION_GO_HOME) {
/* FEED and GO_HOME can use fastest slopes available */
	fast_slope_steps = 256;
	fast_exposure = sanei_genesys_exposure_time2(
	    dev,
	    dev->motor.base_ydpi / 4,
	    0,/*step_type*/
	    0,/*last used pixel*/
	    0,
	    0);
	
	DBG (DBG_info, "gl841_init_motor_regs : fast_exposure=%d pixels\n",
	     fast_exposure);
    }

    if (action == MOTOR_ACTION_HOME_FREE) {
/* HOME_FREE must be able to stop in one step, so do not try to get faster */
	fast_slope_steps = 256;
	fast_exposure = dev->motor.slopes[0][0].maximum_start_speed;
    }

    fast_slope_time = sanei_genesys_create_slope_table3 (
	dev,
	fast_slope_table, 256,
	fast_slope_steps,
	0, 
	fast_exposure,
	dev->motor.base_ydpi / 4,
	&fast_slope_steps,
	&fast_exposure, 0);
    
    feedl = feed_steps - fast_slope_steps*2;
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
    r = sanei_genesys_get_address (reg, 0x5e);	
    r->value &= ~0xe0;

    r = sanei_genesys_get_address (reg, 0x25);
    r->value = 0;
    r = sanei_genesys_get_address (reg, 0x26);
    r->value = 0;
    r = sanei_genesys_get_address (reg, 0x27);	
    r->value = 0;

    r = sanei_genesys_get_address (reg, 0x02);
    r->value &= ~0x01; /*LONGCURV OFF*/
    r->value &= ~0x80; /*NOT_HOME OFF*/

    r->value |= 0x10;

    if (action == MOTOR_ACTION_GO_HOME) 
	r->value |= 0x06;
    else
	r->value &= ~0x06;

    if (use_fast_fed)
	r->value |= 0x08;
    else
	r->value &= ~0x08;

    if (flags & MOTOR_FLAG_AUTO_GO_HOME)
	r->value |= 0x20;
    else
	r->value &= ~0x20;

    r->value &= ~0x40;

    status = gl841_send_slope_table (dev, 3, fast_slope_table, 256);
    
    if (status != SANE_STATUS_GOOD)
	return status;
	
    r = sanei_genesys_get_address (reg, 0x67);	
    r->value = 0x3f;

    r = sanei_genesys_get_address (reg, 0x68);
    r->value = 0x3f;

    r = sanei_genesys_get_address (reg, 0x21);
    r->value = 0;
    
    r = sanei_genesys_get_address (reg, 0x24);
    r->value = 0;
    
    r = sanei_genesys_get_address (reg, 0x69);
    r->value = 0;
    
    r = sanei_genesys_get_address (reg, 0x6a);
    r->value = (fast_slope_steps >> 1) + (fast_slope_steps & 1);
    
    r = sanei_genesys_get_address (reg, 0x5f);
    r->value = (fast_slope_steps >> 1) + (fast_slope_steps & 1);


    DBG (DBG_proc, "gl841_init_motor_regs : completed. \n");

    return SANE_STATUS_GOOD;	
}

static SANE_Status
gl841_init_motor_regs_scan(Genesys_Device * dev,
		      Genesys_Register_Set * reg,
		      unsigned int scan_exposure_time,/*pixel*/
		      float scan_yres,/*dpi, motor resolution*/
		      int scan_step_type,/*0: full, 1: half, 2: quarter*/
		      unsigned int scan_lines,/*lines, scan resolution*/
		      unsigned int scan_dummy,
/*number of scan lines to add in a scan_lines line*/
		      unsigned int feed_steps,/*1/base_ydpi*/
/*maybe float for half/quarter step resolution?*/
		      int scan_power_mode,
		      unsigned int flags) 
{
    SANE_Status status;
    unsigned int fast_exposure;
    int use_fast_fed = 0;
    unsigned int fast_time;
    unsigned int slow_time;
    u_int16_t slow_slope_table[256];
    u_int16_t fast_slope_table[256];
    u_int16_t back_slope_table[256];
    unsigned int slow_slope_time;
    unsigned int fast_slope_time;
    unsigned int back_slope_time;
    unsigned int slow_slope_steps = 0;
    unsigned int fast_slope_steps = 0;
    unsigned int back_slope_steps = 0;
    unsigned int feedl;
    Genesys_Register_Set * r;
    unsigned int min_restep = 0x20;
    u_int32_t z1, z2;

    DBG (DBG_proc, "gl841_init_motor_regs_scan : scan_exposure_time=%d, "
	 "scan_yres=%g, scan_step_type=%d, scan_lines=%d, scan_dummy=%d, "
	 "feed_steps=%d, scan_power_mode=%d, flags=%x\n",
	 scan_exposure_time,
	 scan_yres,
	 scan_step_type,
	 scan_lines,
	 scan_dummy,
	 feed_steps,
	 scan_power_mode,
	 flags);

    fast_exposure = sanei_genesys_exposure_time2(
	dev,
	dev->motor.base_ydpi / 4,
	0,/*step_type*/
	0,/*last used pixel*/
	0,
	scan_power_mode);
    
    DBG (DBG_info, "gl841_init_motor_regs_scan : fast_exposure=%d pixels\n",
	 fast_exposure);


    memset(slow_slope_table,0xff,512);
    
    gl841_send_slope_table (dev, 0, slow_slope_table, 256);
    gl841_send_slope_table (dev, 1, slow_slope_table, 256);
    gl841_send_slope_table (dev, 2, slow_slope_table, 256);
    gl841_send_slope_table (dev, 3, slow_slope_table, 256);
    gl841_send_slope_table (dev, 4, slow_slope_table, 256);


/*
  we calculate both tables for SCAN. the fast slope step count depends on
  how many steps we need for slow acceleration and how much steps we are
  allowed to use.
 */
    slow_slope_time = sanei_genesys_create_slope_table3 (
	dev,
	slow_slope_table, 256,
	256,
	scan_step_type, 
	scan_exposure_time,
	scan_yres,
	&slow_slope_steps,
	NULL,
	scan_power_mode);
    
    back_slope_time = sanei_genesys_create_slope_table3 (
	dev,
	back_slope_table, 256,
	256,
	scan_step_type, 
	0,
	scan_yres,
	&back_slope_steps,
	NULL,
	scan_power_mode);
	
    if (feed_steps < (slow_slope_steps >> scan_step_type)) {
	/*TODO: what should we do here?? go back to exposure calculation?*/
	feed_steps = slow_slope_steps >> scan_step_type;
    }
    
    if (feed_steps > fast_slope_steps*2 - 
	    (slow_slope_steps >> scan_step_type)) 
	fast_slope_steps = 256;
    else 
/* we need to shorten fast_slope_steps here. */
	fast_slope_steps = (feed_steps - 
			    (slow_slope_steps >> scan_step_type))/2;
    
    DBG(DBG_info,"gl841_init_motor_regs_scan: Maximum allowed slope steps for fast slope: %d\n",fast_slope_steps);
    
    fast_slope_time = sanei_genesys_create_slope_table3 (
	dev,
	fast_slope_table, 256,
	fast_slope_steps,
	0, 
	fast_exposure,
	dev->motor.base_ydpi / 4,
	&fast_slope_steps,
	&fast_exposure,
	scan_power_mode);
    
    if (feed_steps < fast_slope_steps*2 + (slow_slope_steps >> scan_step_type)) {
	use_fast_fed = 0;
	DBG(DBG_info,"gl841_init_motor_regs_scan: feed too short, slow move forced.\n");
    } else {
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
	    (feed_steps - fast_slope_steps*2 - 
	     (slow_slope_steps >> scan_step_type)) 
	    + fast_slope_time*2 + slow_slope_time;
	slow_time = 
	    (scan_exposure_time * scan_yres) / dev->motor.base_ydpi *
	    (feed_steps - (slow_slope_steps >> scan_step_type)) 
	    + slow_slope_time;

	DBG(DBG_info,"gl841_init_motor_regs_scan: Time for slow move: %d\n",
	    slow_time);
	DBG(DBG_info,"gl841_init_motor_regs_scan: Time for fast move: %d\n",
	    fast_time);
	
	use_fast_fed = fast_time < slow_time;
    }
    
    if (use_fast_fed) 
	feedl = feed_steps - fast_slope_steps*2 - 
	    (slow_slope_steps >> scan_step_type);
    else 
	if ((feed_steps << scan_step_type) < slow_slope_steps)
	    feedl = 0;
	else
	    feedl = (feed_steps << scan_step_type) - slow_slope_steps;
    DBG(DBG_info,"gl841_init_motor_regs_scan: Decided to use %s mode\n",
	use_fast_fed?"fast feed":"slow feed");

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
    r = sanei_genesys_get_address (reg, 0x5e);	
    r->value &= ~0xe0;

    r = sanei_genesys_get_address (reg, 0x25);
    r->value = (scan_lines >> 16) & 0xf;
    r = sanei_genesys_get_address (reg, 0x26);
    r->value = (scan_lines >> 8) & 0xff;
    r = sanei_genesys_get_address (reg, 0x27);	
    r->value = scan_lines & 0xff;

    r = sanei_genesys_get_address (reg, 0x02);
    r->value &= ~0x01; /*LONGCURV OFF*/
    r->value &= ~0x80; /*NOT_HOME OFF*/
    r->value |= 0x10;

    r->value &= ~0x06;

    if (use_fast_fed)
	r->value |= 0x08;
    else
	r->value &= ~0x08;

    if (flags & MOTOR_FLAG_AUTO_GO_HOME)
	r->value |= 0x20;
    else
	r->value &= ~0x20;

    if (flags & MOTOR_FLAG_DISABLE_BUFFER_FULL_MOVE)
	r->value |= 0x40;
    else
	r->value &= ~0x40;

    status = gl841_send_slope_table (dev, 0, slow_slope_table, 256);
    
    if (status != SANE_STATUS_GOOD)
	return status;
    
    status = gl841_send_slope_table (dev, 1, back_slope_table, 256);
    
    if (status != SANE_STATUS_GOOD)
	return status;
    
    status = gl841_send_slope_table (dev, 2, slow_slope_table, 256);
    
    if (status != SANE_STATUS_GOOD)
	return status;
    
    if (use_fast_fed) {
	status = gl841_send_slope_table (dev, 3, fast_slope_table, 256);
	
	if (status != SANE_STATUS_GOOD)
	    return status;
    }
    
    if (flags & MOTOR_FLAG_AUTO_GO_HOME){
	status = gl841_send_slope_table (dev, 4, fast_slope_table, 256);
	
	if (status != SANE_STATUS_GOOD)
	    return status;
    }
    
    
/* now reg 0x21 and 0x24 are available, we can calculate reg 0x22 and 0x23,
   reg 0x60-0x62 and reg 0x63-0x65
   rule:
   2*STEPNO+FWDSTEP=2*FASTNO+BWDSTEP
*/
/* steps of table 0*/
    if (min_restep < slow_slope_steps*2+2)
	min_restep = slow_slope_steps*2+2;
/* steps of table 1*/
    if (min_restep < back_slope_steps*2+2) 
	min_restep = back_slope_steps*2+2;
/* steps of table 0*/
    r = sanei_genesys_get_address (reg, 0x22);
    r->value = min_restep - slow_slope_steps*2;
/* steps of table 1*/
    r = sanei_genesys_get_address (reg, 0x23);
    r->value = min_restep - back_slope_steps*2;
    
/*
  for z1/z2:
  in dokumentation mentioned variables a-d:
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
    z1 = z2 = 0;
    
    DBG (DBG_info, "gl841_init_motor_regs_scan: z1 = %d\n", z1);
    DBG (DBG_info, "gl841_init_motor_regs_scan: z2 = %d\n", z2);
    r = sanei_genesys_get_address (reg, 0x60);
    r->value = ((z1 >> 16) & 0xff);
    r = sanei_genesys_get_address (reg, 0x61);
    r->value = ((z1 >> 8) & 0xff);
    r = sanei_genesys_get_address (reg, 0x62);
    r->value = (z1 & 0xff);
    r = sanei_genesys_get_address (reg, 0x63);
    r->value = ((z2 >> 16) & 0xff);
    r = sanei_genesys_get_address (reg, 0x64);
    r->value = ((z2 >> 8) & 0xff);
    r = sanei_genesys_get_address (reg, 0x65);
    r->value = (z2 & 0xff);
    
    r = sanei_genesys_get_address (reg, 0x1e);
    r->value &= 0xf0;	/* 0 dummy lines */
    r->value |= scan_dummy;	/* dummy lines */

    r = sanei_genesys_get_address (reg, 0x67);	
    r->value = 0x3f | (scan_step_type << 6);

    r = sanei_genesys_get_address (reg, 0x68);
    r->value = 0x3f;

    r = sanei_genesys_get_address (reg, 0x21);
    r->value = (slow_slope_steps >> 1) + (slow_slope_steps & 1);
    
    r = sanei_genesys_get_address (reg, 0x24);
    r->value = (back_slope_steps >> 1) + (back_slope_steps & 1);
    
    r = sanei_genesys_get_address (reg, 0x69);
    r->value = (slow_slope_steps >> 1) + (slow_slope_steps & 1);
    
    r = sanei_genesys_get_address (reg, 0x6a);
    r->value = (fast_slope_steps >> 1) + (fast_slope_steps & 1);
    
    r = sanei_genesys_get_address (reg, 0x5f);
    r->value = (fast_slope_steps >> 1) + (fast_slope_steps & 1);


    DBG (DBG_proc, "gl841_init_motor_regs_scan : completed. \n");

    return SANE_STATUS_GOOD;	
}

#define OPTICAL_FLAG_DISABLE_GAMMA 1
#define OPTICAL_FLAG_DISABLE_SHADING 2
#define OPTICAL_FLAG_DISABLE_LAMP 4

static SANE_Status
gl841_init_optical_regs_off(Genesys_Device * dev,
			    Genesys_Register_Set * reg)
{
    Genesys_Register_Set * r;

    DBG (DBG_proc, "gl841_init_optical_regs_off : start\n");

    r = sanei_genesys_get_address (reg, 0x01);
    r->value &= ~REG01_SCAN;

    DBG (DBG_proc, "gl841_init_optical_regs_off : completed. \n");
    return SANE_STATUS_GOOD;	
}

static SANE_Status
gl841_init_optical_regs_scan(Genesys_Device * dev,
			     Genesys_Register_Set * reg,
			     unsigned int exposure_time,
			     unsigned int used_res,
			     unsigned int start, 
			     unsigned int pixels,
			     int channels,
			     int depth,
			     SANE_Bool half_ccd,
			     int color_filter,
			     int flags
    )
{
    unsigned int words_per_line;
    unsigned int end;
    unsigned int dpiset;
    unsigned int optical_res;
    unsigned int i;
    Genesys_Register_Set * r;
    SANE_Status status;

    DBG (DBG_proc, "gl841_init_optical_regs_scan :  exposure_time=%d, "
	 "used_res=%d, start=%d, pixels=%d, channels=%d, depth=%d, "
	 "half_ccd=%d, flags=%x\n",
	 exposure_time,
	 used_res,
	 start, 
	 pixels,
	 channels,
	 depth,
	 half_ccd,
	 flags);

    end = start + pixels;
    
/* optical_res */

    optical_res = dev->sensor.optical_res;
    if (half_ccd)
	optical_res /= 2;


    status = gl841_set_fe (dev, AFE_SET);
    if (status != SANE_STATUS_GOOD)
    {
	DBG (DBG_error,
	     "gl841_init_optical_regs_scan: failed to set frontend: %s\n",
	     sane_strstatus (status));
	return status;
    }
    
/*
  with half_ccd the optical resolution of the ccd is halfed. We don't apply this
  to dpihw, so we need to double dpiset.
  
  For the scanner only the ratio of dpiset and dpihw is of relevance to scale
  down properly.
*/
    if (half_ccd) 
	dpiset = used_res * 2;
    else
	dpiset = used_res;
    
/* gpio part. here: for canon lide 35 */
    
    r = sanei_genesys_get_address (reg, 0x6c);
    if (half_ccd)
	r->value &= ~0x80;
    else
	r->value |= 0x80;
    
    /* enable shading */
/*  dev->reg[reg_0x01].value |= REG01_DVDSET | REG01_SCAN;*/
    r = sanei_genesys_get_address (reg, 0x01);
    r->value |= REG01_SCAN;
    if (flags & OPTICAL_FLAG_DISABLE_SHADING)
	r->value &= ~REG01_DVDSET;
    else
	r->value |= REG01_DVDSET;
    
    /* average looks better than deletion, and we are already set up to 
       use  one of the average enabled resolutions
    */
    r = sanei_genesys_get_address (reg, 0x03);
    r->value |= REG03_AVEENB;
    if (flags & OPTICAL_FLAG_DISABLE_LAMP)
	r->value &= ~REG03_LAMPPWR;
    else
	r->value |= REG03_LAMPPWR;
    
    /* exposure times */
    r = sanei_genesys_get_address (reg, 0x10);
    for (i = 0; i < 6; i++, r++) {
	if (flags & OPTICAL_FLAG_DISABLE_LAMP)
	    r->value = 0x01;/* 0x0101 is as off as possible */
	else
	    if (dev->sensor.regs_0x10_0x1d[i] == 0x00)
		r->value = 0x01;/*0x00 will not be accepted*/
	    else
		r->value = dev->sensor.regs_0x10_0x1d[i];
    }

    r = sanei_genesys_get_address (reg, 0x19);
    if (flags & OPTICAL_FLAG_DISABLE_LAMP)
	r->value = 0xff;
    else
	r->value = 0x50;

    /* BW threshold */
    r = sanei_genesys_get_address (reg, 0x2e);
    r->value = dev->settings.threshold;
    r = sanei_genesys_get_address (reg, 0x2f);
    r->value = dev->settings.threshold;


    /* monochrome / color scan */
    r = sanei_genesys_get_address (reg, 0x04);
    switch (depth) {
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
    /* we could make the color filter used an advanced option of the 
       backend */
    if (channels == 1) {
	switch (color_filter) {
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
    } else 
	r->value |= 0x10;	/* color pixel by pixel */
    
    /* enable gamma tables */
    r = sanei_genesys_get_address (reg, 0x05);
    if (flags & OPTICAL_FLAG_DISABLE_GAMMA)
	r->value &= ~REG05_GMMENB;
    else
	r->value |= REG05_GMMENB;
    
    /* sensor parameters */
    sanei_gl841_setup_sensor (dev, dev->reg, 1, half_ccd);
    
    r = sanei_genesys_get_address (reg, 0x29);
    r->value = 255; /*<<<"magic" number, only suitable for cis*/
    
    r = sanei_genesys_get_address (reg, 0x2c);
    r->value = HIBYTE (dpiset);
    r = sanei_genesys_get_address (reg, 0x2d);
    r->value = LOBYTE (dpiset);
    
    r = sanei_genesys_get_address (reg, 0x30);
    r->value = HIBYTE (start);
    r = sanei_genesys_get_address (reg, 0x31);
    r->value = LOBYTE (start);
    r = sanei_genesys_get_address (reg, 0x32);
    r->value = HIBYTE (end);
    r = sanei_genesys_get_address (reg, 0x33);
    r->value = LOBYTE (end);
    
/* words(16bit) before gamma, conversion to 8 bit or lineart*/
    words_per_line = (pixels * used_res) / optical_res; 
    
    words_per_line *= channels;

    if (depth == 1)
	words_per_line = (words_per_line >> 3) + ((words_per_line & 7)?1:0);
    else
	words_per_line *= depth / 8;
    
    r = sanei_genesys_get_address (reg, 0x35);
    r->value = LOBYTE (HIWORD (words_per_line));
    r = sanei_genesys_get_address (reg, 0x36);
    r->value = HIBYTE (LOWORD (words_per_line));
    r = sanei_genesys_get_address (reg, 0x37);
    r->value = LOBYTE (LOWORD (words_per_line));
    
    r = sanei_genesys_get_address (reg, 0x38);
    r->value = HIBYTE (exposure_time);
    r = sanei_genesys_get_address (reg, 0x39);
    r->value = LOBYTE (exposure_time);

    r = sanei_genesys_get_address (reg, 0x34);
    r->value = dev->sensor.dummy_pixel;

    DBG (DBG_proc, "gl841_init_optical_regs_scan : completed. \n");
    return SANE_STATUS_GOOD;	
}


static int 
gl841_get_led_exposure(Genesys_Device * dev) 
{
    int d,r,g,b,m;
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

#define SCAN_FLAG_SINGLE_LINE              0x01
#define SCAN_FLAG_DISABLE_SHADING          0x02
#define SCAN_FLAG_DISABLE_GAMMA            0x04
#define SCAN_FLAG_DISABLE_BUFFER_FULL_MOVE 0x08
#define SCAN_FLAG_IGNORE_LINE_DISTANCE     0x10
#define SCAN_FLAG_USE_OPTICAL_RES          0x20
#define SCAN_FLAG_DISABLE_LAMP             0x40

/* set up registers for an actual scan
 *
 * this function sets up the scanner to scan in normal or single line mode
 */
static SANE_Status
gl841_init_scan_regs (Genesys_Device * dev,
		      Genesys_Register_Set * reg,
		      float xres,/*dpi*/
		      float yres,/*dpi*/
		      float startx,/*optical_res, from dummy_pixel+1*/
		      float starty,/*base_ydpi, from home!*/
		      float pixels,
		      float lines,
		      unsigned int depth,
		      unsigned int channels,
		      int color_filter,
		      unsigned int flags
		      )
{
  int used_res;
  int start, used_pixels;
  int bytes_per_line;
  int move;
  unsigned int lincnt;
  int exposure_time, exposure_time2, led_exposure;
  int i;
  int stagger;

  int slope_dpi = 0;
  int move_dpi = 0;
  int dummy = 0;
  int scan_step_type = 1;
  int scan_power_mode = 0;
  int max_shift;
  size_t requested_buffer_size, read_buffer_size;

  SANE_Bool half_ccd;		/* false: full CCD res is used, true, half max CCD res is used */
  int optical_res;
  SANE_Status status;

  DBG (DBG_info,
       "gl841_init_scan_regs settings:\n"
       "Resolution    : %gDPI/%gDPI\n"
       "Lines         : %g\n"
       "PPL           : %g\n"
       "Startpos      : %g/%g\n"
       "Depth/Channels: %u/%u\n"
       "Flags         : %x\n\n",
       xres, yres, lines, pixels,
       startx, starty,
       depth, channels,
       flags);

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
  if (dev->sensor.optical_res  < 2 * xres) {
      half_ccd = SANE_FALSE;
  } else {
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
  DBG (DBG_info, "gl841_init_scan_regs : stagger=%d lines\n",
       stagger);

/* used_res */
  i = optical_res / xres;

/* gl841 supports 1/1 1/2 1/3 1/4 1/5 1/6 1/8 1/10 1/12 1/15 averaging */

  if (i < 2 || (flags & SCAN_FLAG_USE_OPTICAL_RES)) /* optical_res >= xres > optical_res/2 */
      used_res = optical_res;
  else if (i < 3)  /* optical_res/2 >= xres > optical_res/3 */
      used_res = optical_res/2;  
  else if (i < 4)  /* optical_res/3 >= xres > optical_res/4 */
      used_res = optical_res/3;  
  else if (i < 5)  /* optical_res/4 >= xres > optical_res/5 */
      used_res = optical_res/4;  
  else if (i < 6)  /* optical_res/5 >= xres > optical_res/6 */
      used_res = optical_res/5;  
  else if (i < 8)  /* optical_res/6 >= xres > optical_res/8 */
      used_res = optical_res/6;  
  else if (i < 10)  /* optical_res/8 >= xres > optical_res/10 */
      used_res = optical_res/8;  
  else if (i < 12)  /* optical_res/10 >= xres > optical_res/12 */
      used_res = optical_res/10;  
  else if (i < 15)  /* optical_res/12 >= xres > optical_res/15 */
      used_res = optical_res/12;  
  else
      used_res = optical_res/15;

  /* compute scan parameters values */
  /* pixels are allways given at half or full CCD optical resolution */
  /* use detected left margin  and fixed value */
/* start */
  /* add x coordinates */
  start = 
      ((dev->sensor.CCD_start_xoffset + startx) * used_res) /
      dev->sensor.optical_res;

/* needs to be aligned for used_res */
  start = (start * optical_res) / used_res;

  start += dev->sensor.dummy_pixel + 1;

  if (stagger > 0)
    start |= 1;

  /* compute correct pixels number */
/* pixels */
  used_pixels =
    (pixels * optical_res) / xres;

  /* round up pixels number if needed */
  if (used_pixels * xres < pixels * optical_res)
      used_pixels++;

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
  dummy = 0;

/* slope_dpi */
/* cis color scan is effectively a gray scan with 3 gray lines per color
   line and a FILTER of 0 */
  if (dev->model->is_cis) 
      slope_dpi = yres*channels;
  else
      slope_dpi = yres;

  slope_dpi = slope_dpi * (1 + dummy);

/* scan_step_type */
/* Try to do at least 4 steps per line. if that is impossible we will have to
   live with that
 */
  if (yres*4 < dev->motor.base_ydpi
      || dev->motor.max_step_type <= 0)
      scan_step_type = 0;
  else if (yres*4 < dev->motor.base_ydpi*2
      || dev->motor.max_step_type <= 1)
      scan_step_type = 1;
  else
      scan_step_type = 2;
  
/* exposure_time */
  led_exposure = gl841_get_led_exposure(dev);

  exposure_time = sanei_genesys_exposure_time2(
      dev,
      slope_dpi,
      scan_step_type,
      start+used_pixels,/*+tgtime? currently done in sanei_genesys_exposure_time2 with tgtime = 32 pixel*/
      led_exposure,
      scan_power_mode);
    
  while(scan_power_mode + 1 < dev->motor.power_mode_count) {
      exposure_time2 = sanei_genesys_exposure_time2(
	  dev,
	  slope_dpi,
	  scan_step_type,
	  start+used_pixels,/*+tgtime? currently done in sanei_genesys_exposure_time2 with tgtime = 32 pixel*/
	  led_exposure,
	  scan_power_mode + 1);
      if (exposure_time < exposure_time2)
	  break;
      exposure_time = exposure_time2;
      scan_power_mode++;
  }
  

  DBG (DBG_info, "gl841_init_scan_regs : exposure_time=%d pixels\n",
       exposure_time);

/*** optical parameters ***/


  if (depth == 16)
      flags |= SCAN_FLAG_DISABLE_GAMMA;

  status = gl841_init_optical_regs_scan(dev, 
					reg, 
					exposure_time,
					used_res,
					start, 
					used_pixels,
					channels,
					depth,
					half_ccd,
					color_filter,
					((flags & SCAN_FLAG_DISABLE_SHADING)?
					 OPTICAL_FLAG_DISABLE_SHADING:0) |
					((flags & SCAN_FLAG_DISABLE_GAMMA)?
					 OPTICAL_FLAG_DISABLE_GAMMA:0) |
					((flags & SCAN_FLAG_DISABLE_LAMP)?
					 OPTICAL_FLAG_DISABLE_LAMP:0)
      );

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
      max_shift =
	(max_shift * yres) / dev->motor.base_ydpi;
    }
  else
    {
      max_shift = 0;
    }

/* lincnt */
  lincnt = lines + max_shift + stagger;

/* move */
  move_dpi = dev->motor.base_ydpi;

  /* add tl_y to base movement */
  move = starty;
  DBG (DBG_info, "gl841_init_scan_regs: move=%d steps\n", move);

  /* subtract current head position */
  move -= dev->scanhead_position_in_steps;
  DBG (DBG_info, "gl841_init_scan_regs: move=%d steps\n", move);

  if (move < 0)
      move = 0;

  /* round it */
/* the move is not affected by dummy -- pierre */
/*  move = ((move + dummy) / (dummy + 1)) * (dummy + 1);
    DBG (DBG_info, "gl841_init_scan_regs: move=%d steps\n", move);*/

  if (flags & SCAN_FLAG_SINGLE_LINE)
      status = gl841_init_motor_regs_off(dev,
					 reg,
					 dev->model->is_cis?lincnt*channels:lincnt
	  );
  else
      status = gl841_init_motor_regs_scan(dev,
					  reg,
					  exposure_time,
					  slope_dpi,
					  scan_step_type,
					  dev->model->is_cis?lincnt*channels:lincnt,
					  dummy,
					  move,
					  scan_power_mode,
					  (flags & SCAN_FLAG_DISABLE_BUFFER_FULL_MOVE)?
					  MOTOR_FLAG_DISABLE_BUFFER_FULL_MOVE:0
	  );

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

  RIE(sanei_genesys_buffer_free(&(dev->read_buffer)));
  RIE(sanei_genesys_buffer_alloc(&(dev->read_buffer), read_buffer_size));

  RIE(sanei_genesys_buffer_free(&(dev->lines_buffer)));
  RIE(sanei_genesys_buffer_alloc(&(dev->lines_buffer), read_buffer_size));
  
  RIE(sanei_genesys_buffer_free(&(dev->shrink_buffer)));
  RIE(sanei_genesys_buffer_alloc(&(dev->shrink_buffer), 
				 requested_buffer_size));
  
  RIE(sanei_genesys_buffer_free(&(dev->out_buffer)));
  RIE(sanei_genesys_buffer_alloc(&(dev->out_buffer), 
			(8 * dev->settings.pixels * channels * depth) / 8));


  dev->read_bytes_left = bytes_per_line * lincnt;

  DBG (DBG_info,
       "gl841_init_scan_regs: physical bytes to read = %lu\n",
       (u_long) dev->read_bytes_left);
  dev->read_active = SANE_TRUE;


  dev->current_setup.pixels = (used_pixels * used_res)/optical_res;
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
  if (depth == 1 || dev->settings.scan_mode == 0)
      dev->total_bytes_to_read =
	  ((dev->settings.pixels * dev->settings.lines) / 8 +
	   (((dev->settings.pixels * dev->settings.lines)%8)?1:0)
	      ) * channels;
  else
      dev->total_bytes_to_read =
	  dev->settings.pixels * dev->settings.lines * channels * (depth / 8);

  DBG (DBG_info, "gl646_init_scan_regs: total bytes to send = %lu\n",
       (u_long) dev->total_bytes_to_read);
/* END TODO */

  DBG (DBG_proc, "gl841_init_scan_regs: completed\n");
  return SANE_STATUS_GOOD;
}

static void
gl841_set_motor_power (Genesys_Register_Set * regs, SANE_Bool set)
{

  DBG (DBG_proc, "gl841_set_motor_power\n");

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
gl841_set_lamp_power (Genesys_Device * dev, 
		      Genesys_Register_Set * regs, SANE_Bool set)
{
  Genesys_Register_Set * r;
  int i;

  if (set)
    {
      sanei_genesys_set_reg_from_set (regs, 0x03,
				      sanei_genesys_read_reg_from_set (regs,
								       0x03) |
				      REG03_LAMPPWR);

      r = sanei_genesys_get_address (regs, 0x10);
      for (i = 0; i < 6; i++, r++) {
	if (dev->sensor.regs_0x10_0x1d[i] == 0x00)
	  r->value = 0x01;/*0x00 will not be accepted*/
	else
	  r->value = dev->sensor.regs_0x10_0x1d[i];
      }
      r = sanei_genesys_get_address (regs, 0x19);
      r->value = 0x50;
    }
  else
    {
      sanei_genesys_set_reg_from_set (regs, 0x03,
				      sanei_genesys_read_reg_from_set (regs,
								       0x03) &
				      ~REG03_LAMPPWR);

      r = sanei_genesys_get_address (regs, 0x10);
      for (i = 0; i < 6; i++, r++) {
	r->value = 0x01;/* 0x0101 is as off as possible */
      }
      r = sanei_genesys_get_address (regs, 0x19);
      r->value = 0xff;
    }
}

/*for fast power saving methods only, like disabling certain amplifiers*/
static SANE_Status
gl841_save_power(Genesys_Device * dev, SANE_Bool enable) {
    u_int8_t val;
    
    DBG(DBG_proc, "gl841_save_power: enable = %d\n", enable);

    if (enable)
    {
	if (dev->model->gpo_type == GPO_CANONLIDE35) 
	{
/* expect GPIO17 to be enabled, and GPIO9 to be disabled, 
   while GPIO8 is disabled*/
/* final state: GPIO8 disabled, GPIO9 enabled, GPIO17 disabled, 
   GPIO18 disabled*/

	    sanei_genesys_read_register(dev, 0x6D, &val);
	    sanei_genesys_write_register(dev, 0x6D, val | 0x80);

	    usleep(1000);

	    /*enable GPIO9*/
	    sanei_genesys_read_register(dev, 0x6C, &val);
	    sanei_genesys_write_register(dev, 0x6C, val | 0x01);
	    
	    /*disable GPO17*/
	    sanei_genesys_read_register(dev, 0x6B, &val);
	    sanei_genesys_write_register(dev, 0x6B, val & ~REG6B_GPO17);

	    /*disable GPO18*/
	    sanei_genesys_read_register(dev, 0x6B, &val);
	    sanei_genesys_write_register(dev, 0x6B, val & ~REG6B_GPO18);

	    usleep(1000);

	    sanei_genesys_read_register(dev, 0x6D, &val);
	    sanei_genesys_write_register(dev, 0x6D, val & ~0x80);

	}

	gl841_set_fe (dev, AFE_POWER_SAVE);

    } 
    else 
    {
	if (dev->model->gpo_type == GPO_CANONLIDE35) 
	{
/* expect GPIO17 to be enabled, and GPIO9 to be disabled, 
   while GPIO8 is disabled*/
/* final state: GPIO8 enabled, GPIO9 disabled, GPIO17 enabled, 
   GPIO18 enabled*/

	    sanei_genesys_read_register(dev, 0x6D, &val);
	    sanei_genesys_write_register(dev, 0x6D, val | 0x80);
	    dev->reg[reg_0x6d].value |= 0x80;
	    dev->calib_reg[reg_0x6d].value |= 0x80;

	    usleep(10000);

	    /*disable GPIO9*/
	    sanei_genesys_read_register(dev, 0x6C, &val);
	    sanei_genesys_write_register(dev, 0x6C, val & ~0x01);
	    dev->reg[reg_0x6c].value &= ~0x01;
	    dev->calib_reg[reg_0x6c].value &= ~0x01;

	    /*enable GPIO10*/
	    sanei_genesys_read_register(dev, 0x6C, &val);
	    sanei_genesys_write_register(dev, 0x6C, val | 0x02);
	    dev->reg[reg_0x6c].value |= 0x02;
	    dev->calib_reg[reg_0x6c].value |= 0x02;

	    /*enable GPO17*/
	    sanei_genesys_read_register(dev, 0x6B, &val);
	    sanei_genesys_write_register(dev, 0x6B, val | REG6B_GPO17);
	    dev->reg[reg_0x6b].value |= REG6B_GPO17;
	    dev->calib_reg[reg_0x6b].value |= REG6B_GPO17;

	    /*enable GPO18*/
	    sanei_genesys_read_register(dev, 0x6B, &val);
	    sanei_genesys_write_register(dev, 0x6B, val | REG6B_GPO18);
	    dev->reg[reg_0x6b].value |= REG6B_GPO18;
	    dev->calib_reg[reg_0x6b].value |= REG6B_GPO18;

	}
    }

    return SANE_STATUS_GOOD;
}

static SANE_Status
gl841_set_powersaving (Genesys_Device * dev,
			     int delay /* in minutes */ )
{
  SANE_Status status;
  Genesys_Register_Set local_reg[7];
  int rate, exposure_time, tgtime, time;

  DBG (DBG_proc, "gl841_set_powersaving (delay = %d)\n", delay);

  local_reg[0].address = 0x01;
  local_reg[0].value = sanei_genesys_read_reg_from_set (dev->reg, 0x01);	/* disable fastmode */

  local_reg[1].address = 0x03;
  local_reg[1].value = sanei_genesys_read_reg_from_set (dev->reg, 0x03);	/* Lamp power control */

  local_reg[2].address = 0x05;
  local_reg[2].value = sanei_genesys_read_reg_from_set (dev->reg, 0x05) /*& ~REG05_BASESEL*/;	/* 24 clocks/pixel */

  local_reg[3].address = 0x18;	/* Set CCD type */
  local_reg[3].value = 0x00;

  local_reg[4].address = 0x38;	/* line period low */
  local_reg[4].value = 0x00;

  local_reg[5].address = 0x39;	/* line period high */
  local_reg[5].value = 0x00;

  local_reg[6].address = 0x1c;	/* period times for LPeriod, expR,expG,expB, Z1MODE, Z2MODE */
  local_reg[6].value = sanei_genesys_read_reg_from_set (dev->reg, 0x05) & ~REG1C_TGTIME;

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

  local_reg[6].value |= tgtime;
  exposure_time /= rate;

  if (exposure_time > 65535)
    exposure_time = 65535;

  local_reg[4].value = exposure_time >> 8;	/* highbyte */
  local_reg[5].value = exposure_time & 255;	/* lowbyte */

  status =
    gl841_bulk_write_register (dev, local_reg, 
			       sizeof (local_reg)/sizeof (local_reg[0]));
  if (status != SANE_STATUS_GOOD)
    DBG (DBG_error,
	 "gl841_set_powersaving: Failed to bulk write registers: %s\n",
	 sane_strstatus (status));

  DBG (DBG_proc, "gl841_set_powersaving: completed\n");
  return status;
}

/* Send the low-level scan command */
/* todo : is this that useful ? */
static SANE_Status
gl841_begin_scan (Genesys_Device * dev, Genesys_Register_Set * reg,
			SANE_Bool start_motor)
{
  SANE_Status status;
  Genesys_Register_Set local_reg[4];

  DBG (DBG_proc, "gl841_begin_scan\n");

  local_reg[0].address = 0x03;
  local_reg[0].value = sanei_genesys_read_reg_from_set (reg, 0x03) | REG03_LAMPPWR;

  local_reg[1].address = 0x01;
  local_reg[1].value = sanei_genesys_read_reg_from_set (reg, 0x01) | REG01_SCAN;	/* set scan bit */

  local_reg[2].address = 0x0d;
  local_reg[2].value = 0x01;

  local_reg[3].address = 0x0f;
  if (start_motor)
    local_reg[3].value = 0x01;
  else
    local_reg[3].value = 0x00;	/* do not start motor yet */

  status =
    gl841_bulk_write_register (dev, local_reg, 
			       sizeof (local_reg)/sizeof (local_reg[0]));
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl841_begin_scan: Failed to bulk write registers: %s\n",
	   sane_strstatus (status));
      return status;
    }

  DBG (DBG_proc, "gl841_begin_scan: completed\n");

  return status;
}


/* Send the stop scan command */
static SANE_Status
gl841_end_scan (Genesys_Device * dev, Genesys_Register_Set * reg,
		      SANE_Bool check_stop)
{
  SANE_Status status;
  int i = 0;
  u_int8_t val;

  DBG (DBG_proc, "gl841_end_scan (check_stop = %d)\n", check_stop);

  status = sanei_genesys_write_register (dev, 0x01, sanei_genesys_read_reg_from_set (reg, 0x01) & ~REG01_SCAN);	/* disable scan */
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "gl841_end_scan: Failed to write registers: %s\n",
	   sane_strstatus (status));
      return status;
    }

  if (check_stop)
    {
      for (i = 0; i < 300; i++)	/* do not wait longer than 3 seconds */
	{
	  status = sanei_genesys_get_status (dev, &val);
	  if (status != SANE_STATUS_GOOD)
	    {
	      DBG (DBG_error,
		   "gl841_end_scan: Failed to read register: %s\n",
		   sane_strstatus (status));
	      return status;
	    }

	  if ((!(val & REG41_MOTORENB)) && (val & REG41_SCANFSH))
	    {
	      DBG (DBG_proc, "gl841_end_scan: scan finished\n");
	      break;		/* leave while loop */
	    }


	  usleep (10000);	/* sleep 100 ms */
	}

    }

  DBG (DBG_proc, "gl841_end_scan: completed (i=%u)\n", i);

  return status;
}

/* Moves the slider to steps */
static SANE_Status
gl841_feed (Genesys_Device * dev, int steps)
{
  Genesys_Register_Set local_reg[GENESYS_GL841_MAX_REGS+1];
  SANE_Status status;
  u_int8_t val;
  int i;
  int loop = 0;  

  DBG (DBG_proc, "gl841_feed (steps = %d)\n",
       steps);

  memset (local_reg, 0, sizeof(local_reg));
  val = 0;

  /* stop motor if needed */
  if (val & REG41_MOTORENB)
    {
      status = sanei_genesys_stop_motor (dev);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "gl841_feed: failed to stop motor: %s\n",
	       sane_strstatus (status));
	  return SANE_STATUS_IO_ERROR;
	}
      usleep (200 * 1000);
    }

  memcpy (local_reg, dev->reg, (GENESYS_GL841_MAX_REGS+1) * sizeof (Genesys_Register_Set));

  gl841_init_optical_regs_off(dev,local_reg);

  gl841_init_motor_regs(dev,local_reg,
			steps,MOTOR_ACTION_FEED,0);

/* when scanhead is moving then wait until scanhead stops or timeout */
  DBG (DBG_info, "gl841_feed: ensuring that motor is off\n");
  for (i = 400; i > 0; i--)	/* do not wait longer than 40 seconds, count down to get i = 0 when busy */
    {
      status = sanei_genesys_get_status (dev, &val);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "gl841_feed: Failed to read home sensor & motor status: %s\n",
	       sane_strstatus (status));
	  return status;
	}

      if (!(val & REG41_MOTORENB))	/* motor is off */
	{
	  DBG (DBG_info,
	       "gl841_feed: motor is off\n");
	  break;		/* motor is off and scanhead is not at home: continue */
	}

      usleep (100 * 1000);	/* sleep 100 ms */
    }

  if (!i)			/* the loop counted down to 0, scanner still is busy */
    {
      DBG (DBG_error,
	   "gl841_feed: motor is still on: device busy\n");
      return SANE_STATUS_DEVICE_BUSY;
    }

  status =
    gl841_bulk_write_register (dev, local_reg,
			       GENESYS_GL841_MAX_REGS);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl841_feed: Failed to bulk write registers: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status = sanei_genesys_start_motor (dev);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl841_feed: Failed to start motor: %s\n",
	   sane_strstatus (status));
      sanei_genesys_stop_motor (dev);
      /* send original registers */
      gl841_bulk_write_register (dev, dev->reg,
				 GENESYS_GL841_MAX_REGS);
      return status;
    }

  while (loop < 300)		/* do not wait longer then 30 seconds */
  {
      status = sanei_genesys_get_status (dev, &val);
      if (status != SANE_STATUS_GOOD)
      {
	  DBG (DBG_error,
	       "gl841_feed: Failed to read home sensor: %s\n",
	       sane_strstatus (status));
	  return status;
      }
      
      if (!(val & REG41_MOTORENB))	/* motor enabled */
      {
	  DBG (DBG_proc, "gl841_feed: finished\n");
	  dev->scanhead_position_in_steps += steps;
	  return SANE_STATUS_GOOD;
      }
      usleep (100000);	/* sleep 100 ms */
      ++loop;
  }

  /* when we come here then the scanner needed too much time for this, so we better stop the motor */
  sanei_genesys_stop_motor (dev);

  DBG (DBG_error,
       "gl841_slow_back_home: timeout while waiting for scanhead to go home\n");
  return SANE_STATUS_IO_ERROR;
}

/* Moves the slider to the home (top) postion slowly */
static SANE_Status
gl841_slow_back_home (Genesys_Device * dev, SANE_Bool wait_until_home)
{
  Genesys_Register_Set local_reg[GENESYS_GL841_MAX_REGS+1];
  SANE_Status status;
  u_int8_t val;
  int i;

  DBG (DBG_proc, "gl841_slow_back_home (wait_until_home = %d)\n",
       wait_until_home);

  memset (local_reg, 0, sizeof (local_reg));
  val = 0;
  status = sanei_genesys_get_status (dev, &val);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl841_slow_back_home: Failed to read home sensor: %s\n",
	   sane_strstatus (status));
      return status;
    }

  dev->scanhead_position_in_steps = 0;

  if (val & REG41_HOMESNR)	/* is sensor at home? */
    {
      DBG (DBG_info,
	   "gl841_slow_back_home: already at home, completed\n");
      dev->scanhead_position_in_steps = 0;
      return SANE_STATUS_GOOD;
    }

  /* stop motor if needed */
  if (val & REG41_MOTORENB)
    {
      status = sanei_genesys_stop_motor (dev);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "gl841_slow_back_home: failed to stop motor: %s\n",
	       sane_strstatus (status));
	  return SANE_STATUS_IO_ERROR;
	}
      usleep (200 * 1000);
    }

  memcpy (local_reg, dev->reg, (GENESYS_GL841_MAX_REGS+1) * sizeof (Genesys_Register_Set));

  gl841_init_optical_regs_off(dev,local_reg);

  gl841_init_motor_regs(dev,local_reg,
			65536,MOTOR_ACTION_GO_HOME,0);

/* when scanhead is moving then wait until scanhead stops or timeout */
  DBG (DBG_info, "gl841_slow_back_home: ensuring that motor is off\n");
  for (i = 400; i > 0; i--)	/* do not wait longer than 40 seconds, count down to get i = 0 when busy */
    {
      status = sanei_genesys_get_status (dev, &val);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "gl841_slow_back_home: Failed to read home sensor & motor status: %s\n",
	       sane_strstatus (status));
	  return status;
	}
      if (((val & (REG41_MOTORENB | REG41_HOMESNR)) == REG41_HOMESNR))	/* at home and motor is off */
	{
	  DBG (DBG_info,
	       "gl841_slow_back_home: already at home and nor moving\n");
	  dev->scanhead_position_in_steps = 0;
	  return SANE_STATUS_GOOD;
	}

      if (!(val & REG41_MOTORENB))	/* motor is off */
	{
	  DBG (DBG_info,
	       "gl841_slow_back_home: motor is off but scanhead is not home\n");
	  break;		/* motor is off and scanhead is not at home: continue */
	}

      usleep (100 * 1000);	/* sleep 100 ms */
    }

  if (!i)			/* the loop counted down to 0, scanner still is busy */
    {
      DBG (DBG_error,
	   "gl841_slow_back_home: motor is still on: device busy\n");
      return SANE_STATUS_DEVICE_BUSY;
    }

  status =
    gl841_bulk_write_register (dev, local_reg, GENESYS_GL841_MAX_REGS);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl841_slow_back_home: Failed to bulk write registers: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status = sanei_genesys_start_motor (dev);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl841_slow_back_home: Failed to start motor: %s\n",
	   sane_strstatus (status));
      sanei_genesys_stop_motor (dev);
      /* send original registers */
      gl841_bulk_write_register (dev, dev->reg, GENESYS_GL841_MAX_REGS);
      return status;
    }

  if (wait_until_home)
    {
      int loop = 0;

      while (loop < 300)		/* do not wait longer then 30 seconds */
	{
	  status = sanei_genesys_get_status (dev, &val);
	  if (status != SANE_STATUS_GOOD)
	    {
	      DBG (DBG_error,
		   "gl841_slow_back_home: Failed to read home sensor: %s\n",
		   sane_strstatus (status));
	      return status;
	    }

	  if (val & 0x08)	/* home sensor */
	    {
	      DBG (DBG_info,
		   "gl841_slow_back_home: reached home position\n");
	      DBG (DBG_proc, "gl841_slow_back_home: finished\n");
	      return SANE_STATUS_GOOD;
	    }
	  usleep (100000);	/* sleep 100 ms */
	  ++loop;
	}

      /* when we come here then the scanner needed too much time for this, so we better stop the motor */
      sanei_genesys_stop_motor (dev);
      DBG (DBG_error,
	   "gl841_slow_back_home: timeout while waiting for scanhead to go home\n");
      return SANE_STATUS_IO_ERROR;
    }

  DBG (DBG_info, "gl841_slow_back_home: scanhead is still moving\n");
  DBG (DBG_proc, "gl841_slow_back_home: finished\n");
  return SANE_STATUS_GOOD;
}

/* Moves the slider to the home (top) postion */
/* relies on the settings given for scan,
   but disables scan, puts motor in reverse and
   steps to go before area to 65535 so head will
   go home */
static SANE_Status
gl841_park_head (Genesys_Device * dev, Genesys_Register_Set * reg,
		       SANE_Bool wait_until_home)
{
  Genesys_Register_Set local_reg[GENESYS_GL841_MAX_REGS+1];
  SANE_Status status;
  u_int8_t val = 0;
  int loop = 0;
  int i = 0;

  DBG (DBG_proc, "gl841_park_head (wait_until_home = %d)\n",
       wait_until_home);

  memset (local_reg, 0, sizeof (local_reg));

  /* read status */
  status = sanei_genesys_get_status (dev, &val);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl841_park_head: failed to read home sensor: %s\n",
	   sane_strstatus (status));
      return status;
    }

  /* no need to park if already at home */
  if (val & REG41_HOMESNR)
    {
      dev->scanhead_position_in_steps = 0;
      return status;
    }

  /* stop motor if needed */
  if (val & REG41_MOTORENB)
    {
      status = sanei_genesys_stop_motor (dev);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "gl841_park_head: failed to stop motor: %s\n",
	       sane_strstatus (status));
	  return SANE_STATUS_IO_ERROR;
	}
      usleep (200 * 1000);
    }

  memcpy (local_reg, dev->reg, (GENESYS_GL841_MAX_REGS+1) * sizeof (Genesys_Register_Set));

  gl841_init_optical_regs_off(dev,local_reg);

  gl841_init_motor_regs(dev,local_reg,
			65536,MOTOR_ACTION_GO_HOME,0);

  /* writes register */
  status = gl841_bulk_write_register (dev, local_reg, i);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl841_park_head: failed to bulk write registers: %s\n",
	   sane_strstatus (status));
      return status;
    }

  /* start motor */
  status = sanei_genesys_start_motor (dev);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "gl841_park_head: failed to start motor: %s\n",
	   sane_strstatus (status));
      sanei_genesys_stop_motor (dev);
      /* restore original registers */
      gl841_bulk_write_register (dev, reg, GENESYS_GL841_MAX_REGS);
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
		   "gl841_park_head: failed to read home sensor: %s\n",
		   sane_strstatus (status));
	      return status;
	    }

	  /* test home sensor */
	  if (val & REG41_HOMESNR)
	    {
	      DBG (DBG_info,
		   "gl841_park_head: reached home position\n");
	      DBG (DBG_proc, "gl841_park_head: finished\n");
	      dev->scanhead_position_in_steps = 0;
	      return SANE_STATUS_GOOD;
	    }
	  /* hack around a bug ? */
	  if (!(val & REG41_MOTORENB))
	    {
	      DBG (DBG_info, "gl841_park_head: restarting motor\n");
	      status = sanei_genesys_start_motor (dev);
	      if (status != SANE_STATUS_GOOD)
		{
		  DBG (DBG_error,
		       "gl841_park_head: failed to restart motor: %s\n",
		       sane_strstatus (status));
		}
	    }
	  usleep (100000);
	  ++loop;
	}
    } else {
	DBG (DBG_info,
	     "gl841_park_head: exiting while moving home\n");
	return SANE_STATUS_GOOD;
    }

  /* when we come here then the scanner needed too much time for this,
     so we better stop the motor */
  sanei_genesys_stop_motor (dev);
  DBG (DBG_error,
       "gl841_park_head: timeout while waiting for scanhead to go home\n");
  return SANE_STATUS_IO_ERROR;
}


/* Automatically set top-left edge of the scan area by scanning a 200x200 pixels
   area at 600 dpi from very top of scanner */
static SANE_Status
gl841_search_start_position (Genesys_Device * dev)
{
  int size;
  SANE_Status status;
  u_int8_t *data;
  Genesys_Register_Set local_reg[GENESYS_GL841_MAX_REGS+1];
  int steps;

  int pixels = 600;
  int dpi = 300;

  DBG (DBG_proc, "gl841_search_start_position\n");

  memset (local_reg, 0, sizeof (local_reg));
  memcpy (local_reg, dev->reg, (GENESYS_GL841_MAX_REGS +1) * sizeof (Genesys_Register_Set));

  /* sets for a 200 lines * 600 pixels */
  /* normal scan with no shading */

  status = gl841_init_scan_regs (dev,
				 local_reg,
				 dpi,
				 dpi,
				 0,
				 0,/*we should give a small offset here~60 steps*/
				 600,
				 dev->model->search_lines,
				 8,
				 1,
				 1,/*green*/
				 SCAN_FLAG_DISABLE_SHADING |
				 SCAN_FLAG_DISABLE_GAMMA |
				 SCAN_FLAG_IGNORE_LINE_DISTANCE |
				 SCAN_FLAG_DISABLE_BUFFER_FULL_MOVE
      );

  /* send to scanner */
  status =
    gl841_bulk_write_register (dev, local_reg, GENESYS_GL841_MAX_REGS);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl841_search_start_position: failed to bulk write registers: %s\n",
	   sane_strstatus (status));
      return status;
    }

  size = pixels * dev->model->search_lines;

  data = malloc (size);
  if (!data)
    {
      DBG (DBG_error,
	   "gl841_search_start_position: failed to allocate memory\n");
      return SANE_STATUS_NO_MEM;
    }

  status = gl841_begin_scan (dev, local_reg, SANE_TRUE);
  if (status != SANE_STATUS_GOOD)
    {
      free (data);
      DBG (DBG_error,
	   "gl841_search_start_position: failed to begin scan: %s\n",
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
	   "gl841_search_start_position: failed to read data: %s\n",
	   sane_strstatus (status));
      return status;
    }

  if (DBG_LEVEL >= DBG_data)
    sanei_genesys_write_pnm_file ("search_position.pnm", data, 8, 1, pixels,
				  dev->model->search_lines);

  status = gl841_end_scan (dev, local_reg, SANE_TRUE);
  if (status != SANE_STATUS_GOOD)
    {
      free (data);
      DBG (DBG_error,
	   "gl841_search_start_position: failed to end scan: %s\n",
	   sane_strstatus (status));
      return status;
    }

  /* update regs to copy ASIC internal state */
  memcpy (dev->reg, local_reg, (GENESYS_GL841_MAX_REGS + 1) * sizeof (Genesys_Register_Set));

/*TODO: find out where sanei_genesys_search_reference_point 
  stores information, and use that correctly*/
  status =
    sanei_genesys_search_reference_point (dev, data, 0, dpi, pixels,
					  dev->model->search_lines);
  if (status != SANE_STATUS_GOOD)
    {
      free (data);
      DBG (DBG_error,
	   "gl841_search_start_position: failed to set search reference point: %s\n",
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
gl841_init_regs_for_coarse_calibration (Genesys_Device * dev)
{
  SANE_Status status;
  u_int8_t channels;
  u_int8_t cksel;

  DBG (DBG_proc, "gl841_init_regs_for_coarse_calibration\n");


  cksel = (dev->calib_reg[reg_0x18].value & REG18_CKSEL) + 1;	/* clock speed = 1..4 clocks */

  /* set line size */
  if (dev->settings.scan_mode == 4)	/* single pass color */
    channels = 3;
  else
    channels = 1;

  status = gl841_init_scan_regs (dev,
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
				 SCAN_FLAG_IGNORE_LINE_DISTANCE
      );
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl841_init_register_for_coarse_calibration: Failed to setup scan: %s\n",
	   sane_strstatus (status));
      return status;
    }

  DBG (DBG_info,
       "gl841_init_register_for_coarse_calibration: optical sensor res: %d dpi, actual res: %d\n",
       dev->sensor.optical_res / cksel, dev->settings.xres);

  status =
    gl841_bulk_write_register (dev, dev->calib_reg, GENESYS_GL841_MAX_REGS);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl841_init_register_for_coarse_calibration: Failed to bulk write registers: %s\n",
	   sane_strstatus (status));
      return status;
    }

  DBG (DBG_proc,
       "gl841_init_register_for_coarse_calibration: completed\n");

/*  if (DBG_LEVEL >= DBG_info)
    sanei_gl841_print_registers (dev->calib_reg);*/


  return SANE_STATUS_GOOD;
}


/* init registers for shading calibration */
static SANE_Status
gl841_init_regs_for_shading (Genesys_Device * dev)
{
  SANE_Status status;
  u_int8_t channels;
  

  DBG (DBG_proc, "gl841_init_regs_for_shading: lines = %d\n",
       dev->model->shading_lines);

  if (dev->settings.scan_mode == 4)	/* single pass color */
    channels = 3;
  else
    channels = 1;

  status = gl841_init_scan_regs (dev,
				 dev->calib_reg,
				 dev->settings.xres,
				 dev->motor.base_ydpi,
				 0,
				 0,
				 (dev->sensor.sensor_pixels * dev->settings.xres) / dev->sensor.optical_res,
				 dev->model->shading_lines,
				 16,
				 channels,
				 dev->settings.color_filter,
				 SCAN_FLAG_DISABLE_SHADING |
				 SCAN_FLAG_DISABLE_GAMMA |
/* we don't handle differing shading areas very well */
/*				 SCAN_FLAG_DISABLE_BUFFER_FULL_MOVE |*/
				 SCAN_FLAG_IGNORE_LINE_DISTANCE
      );

  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl841_init_registers_for_shading: Failed to setup scan: %s\n",
	   sane_strstatus (status));
      return status;
    }

  dev->scanhead_position_in_steps += dev->model->shading_lines;

  status =
    gl841_bulk_write_register (dev, dev->calib_reg, GENESYS_GL841_MAX_REGS);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl841_init_registers_for_shading: Failed to bulk write registers: %s\n",
	   sane_strstatus (status));
      return status;
    }

  DBG (DBG_proc, "gl841_init_regs_for_shading: completed\n");

  return SANE_STATUS_GOOD;
}

/* set up registers for the actual scan
 */
static SANE_Status
gl841_init_regs_for_scan (Genesys_Device * dev)
{
  int channels;
  int depth;
  float move;
  int move_dpi;
  float start;

  SANE_Status status;

  DBG (DBG_info,
       "gl841_init_regs_for_scan settings:\nResolution: %uDPI\n"
       "Lines     : %u\nPPL       : %u\nStartpos  : %.3f/%.3f\nScan mode : %d\n\n",
       dev->settings.yres, dev->settings.lines, dev->settings.pixels,
       dev->settings.tl_x, dev->settings.tl_y, dev->settings.scan_mode);

  gl841_slow_back_home(dev,1);

/* channels */
  if (dev->settings.scan_mode == 4)	/* single pass color */
    channels = 3;
  else
    channels = 1;

/* depth */
  depth = dev->settings.depth;
  if (dev->settings.scan_mode == 0)
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

  move = 0;
  if (dev->model->flags & GENESYS_FLAG_SEARCH_START)
    move += SANE_UNFIX (dev->model->y_offset_calib);

  DBG (DBG_info, "gl841_init_regs_for_scan: move=%f steps\n", move);

  move += SANE_UNFIX (dev->model->y_offset);
  DBG (DBG_info, "gl841_init_regs_for_scan: move=%f steps\n", move);

  move += dev->settings.tl_y;
  DBG (DBG_info, "gl841_init_regs_for_scan: move=%f steps\n", move);

  move = (move * move_dpi) / MM_PER_INCH;

/* start */
  start = SANE_UNFIX (dev->model->x_offset);

  start += dev->settings.tl_x;

  start = (start * dev->sensor.optical_res) / MM_PER_INCH;

  status = gl841_init_scan_regs (dev,
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
				 0
      );
  
  if (status != SANE_STATUS_GOOD)
      return status;


  DBG (DBG_proc, "gl841_init_register_for_scan: completed\n");
  return SANE_STATUS_GOOD;
}

/*
 * this function sends generic gamma table (ie linear ones)
 * or the Sensor specific one if provided
 */
static SANE_Status
gl841_send_gamma_table (Genesys_Device * dev, SANE_Bool generic)
{
  int size;
  int status;
  u_int8_t *gamma;
  int i;

  DBG (DBG_proc, "gl841_send_gamma_table\n");

  /* don't send anything if no specific gamma table defined */
  if (!generic
      && (dev->sensor.red_gamma_table == NULL
	  || dev->sensor.green_gamma_table == NULL
	  || dev->sensor.blue_gamma_table == NULL))
    {
      DBG (DBG_proc,
	   "gl841_send_gamma_table: nothing to send, skipping\n");
      return SANE_STATUS_GOOD;
    }

  size = 256;

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
	  gamma[i*2 + size * 0 + 0] = i & 0xff;
	  gamma[i*2 + size * 0 + 1] = (i >> 8) & 0xff;
	  gamma[i*2 + size * 2 + 0] = i & 0xff;
	  gamma[i*2 + size * 2 + 1] = (i >> 8) & 0xff;
	  gamma[i*2 + size * 4 + 0] = i & 0xff;
	  gamma[i*2 + size * 4 + 1] = (i >> 8) & 0xff;
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
  status = gl841_set_buffer_address_gamma (dev, 0x00000);
  if (status != SANE_STATUS_GOOD)
    {
      free (gamma);
      DBG (DBG_error,
	   "gl841_send_gamma_table: failed to set buffer address: %s\n",
	   sane_strstatus (status));
      return status;
    }

  /* send data */
  status =
    gl841_bulk_write_data_gamma (dev, 0x28, (u_int8_t *) gamma,
				   size * 2 * 3);
  if (status != SANE_STATUS_GOOD)
    {
      free (gamma);
      DBG (DBG_error,
	   "gl841_send_gamma_table: failed to send gamma table: %s\n",
	   sane_strstatus (status));
      return status;
    }

  DBG (DBG_proc, "gl841_send_gamma_table: completed\n");
  free (gamma);
  return SANE_STATUS_GOOD;
}


/* this function does the led calibration by scanning one line of the calibration
   area below scanner's top on white strip.

-needs working coarse/gain
*/
static SANE_Status
gl841_led_calibration (Genesys_Device * dev)
{
  int num_pixels;
  int total_size;
  int used_res;
  u_int8_t *line;
  int i, j;
  SANE_Status status = SANE_STATUS_GOOD;
  int val;
  int channels;
  int avg[3], avga, avge;
  int turn;
  char fn[20];
  u_int16_t expr, expg, expb;
  Genesys_Register_Set *r;

  SANE_Bool acceptable = SANE_FALSE;

  DBG (DBG_proc, "gl841_led_calibration\n");

  status = gl841_feed(dev, 280);/*feed to white strip. canon lide 35 only.*/
  
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl841_coarse_gain_calibration: Failed to feed: %s\n",
	   sane_strstatus (status));
      return status;
    }

  /* offset calibration is always done in color mode */
  channels = 3;

  status = gl841_init_scan_regs (dev,
				 dev->calib_reg,
				 dev->settings.xres,
				 dev->settings.yres,
				 0,
				 0,
				 (dev->sensor.sensor_pixels*dev->settings.xres) / dev->sensor.optical_res,
				 1,
				 16,
				 channels,
				 dev->settings.color_filter,
				 SCAN_FLAG_DISABLE_SHADING |
				 SCAN_FLAG_DISABLE_GAMMA |
				 SCAN_FLAG_SINGLE_LINE |
				 SCAN_FLAG_IGNORE_LINE_DISTANCE |
				 SCAN_FLAG_USE_OPTICAL_RES
      );

  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl841_led_calibration: Failed to setup scan: %s\n",
	   sane_strstatus (status));
      return status;
    }

  RIE (gl841_bulk_write_register(dev, dev->calib_reg, GENESYS_GL841_MAX_REGS));

  used_res = dev->current_setup.xres;
  num_pixels = dev->current_setup.pixels;

  total_size = num_pixels * channels * 2 * 1;	/* colors * bytes_per_color * scan lines */

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

  do {

      dev->sensor.regs_0x10_0x1d[0] = (expr >> 8) & 0xff;
      dev->sensor.regs_0x10_0x1d[1] = expr & 0xff;
      dev->sensor.regs_0x10_0x1d[2] = (expg >> 8) & 0xff;
      dev->sensor.regs_0x10_0x1d[3] = expg & 0xff;
      dev->sensor.regs_0x10_0x1d[4] = (expb >> 8) & 0xff;
      dev->sensor.regs_0x10_0x1d[5] = expb & 0xff;

      r = &(dev->calib_reg[reg_0x10]);
      for (i = 0; i < 6; i++, r++) {
	  r->value = dev->sensor.regs_0x10_0x1d[i];
	  RIE (sanei_genesys_write_register (dev, 0x10+i, dev->sensor.regs_0x10_0x1d[i]));
      }

      RIE (gl841_bulk_write_register
	   (dev, dev->calib_reg, GENESYS_GL841_MAX_REGS));

      DBG (DBG_info,
	   "gl841_led_calibration: starting first line reading\n");
      RIE (gl841_begin_scan (dev, dev->calib_reg, SANE_TRUE));
      RIE (sanei_genesys_read_data_from_scanner (dev, line, total_size));
      
      if (DBG_LEVEL >= DBG_data) {
	  snprintf(fn,20,"led_%d.pnm",turn);
	  sanei_genesys_write_pnm_file (fn,
					line,
					16,
					channels,
					num_pixels, 1);
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

      DBG(DBG_info,"gl841_led_calibration: average: "
	  "%d,%d,%d\n",
	  avg[0],avg[1],avg[2]);

      acceptable = SANE_TRUE;
      
      if (avg[0] < avg[1] * 0.95 || avg[1] < avg[0] * 0.95 ||
	  avg[0] < avg[2] * 0.95 || avg[2] < avg[0] * 0.95 ||
	  avg[1] < avg[2] * 0.95 || avg[2] < avg[1] * 0.95)
	  acceptable = SANE_FALSE;
      
      if (!acceptable) {
	  avga = (avg[0]+avg[1]+avg[2])/3;
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

	  if (avge > 2000) {
	      expr = (expr * 2000) / avge;
	      expg = (expg * 2000) / avge;
	      expb = (expb * 2000) / avge;
	  }
	  if (avge < 500) {
	      expr = (expr * 500) / avge;
	      expg = (expg * 500) / avge;
	      expb = (expb * 500) / avge;
	  }
	  
      }      

      RIE (gl841_end_scan (dev, dev->calib_reg, SANE_TRUE));

      turn++;

  } while (!acceptable && turn < 100);
      
  DBG(DBG_info,"gl841_led_calibration: acceptable exposure: %d,%d,%d\n",
      expr,expg,expb);

  /* cleanup before return */
  free (line);

  gl841_slow_back_home(dev, SANE_TRUE);

  DBG (DBG_proc, "gl841_led_calibration: completed\n");
  return status;
}

/* this function does the offset calibration by scanning one line of the calibration
   area below scanner's top. There is a black margin and the remaining is white.
   sanei_genesys_search_start() must have been called so that the offsets and margins
   are allready known.

this function expects the slider to be where?
*/
static SANE_Status
gl841_offset_calibration (Genesys_Device * dev)
{
  int num_pixels;
  int total_size;
  int used_res;
  u_int8_t *first_line, *second_line;
  int i, j;
  SANE_Status status = SANE_STATUS_GOOD;
  int val;
  int channels;
  int off[3],offh[3],offl[3],off1[3],off2[3];
  int min1[3],min2[3];
  int cmin[3],cmax[3];
  int turn;
  char fn[20];
  SANE_Bool acceptable = SANE_FALSE;
  int mintgt = 0x2000;

  DBG (DBG_proc, "gl841_offset_calibration\n");

  /* offset calibration is always done in color mode */
  channels = 3;

  status = gl841_init_scan_regs (dev,
				 dev->calib_reg,
				 dev->settings.xres,
				 dev->settings.yres,
				 0,
				 0,
				 (dev->sensor.sensor_pixels*dev->settings.xres) / dev->sensor.optical_res,
				 1,
				 16,
				 channels,
				 dev->settings.color_filter,
				 SCAN_FLAG_DISABLE_SHADING |
				 SCAN_FLAG_DISABLE_GAMMA |
				 SCAN_FLAG_SINGLE_LINE |
				 SCAN_FLAG_IGNORE_LINE_DISTANCE |
				 SCAN_FLAG_USE_OPTICAL_RES
      );

  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl841_offset_calibration: Failed to setup scan: %s\n",
	   sane_strstatus (status));
      return status;
    }

  RIE (gl841_bulk_write_register
       (dev, dev->calib_reg, GENESYS_GL841_MAX_REGS));

  used_res = dev->current_setup.xres;
  num_pixels = dev->current_setup.pixels;

  total_size = num_pixels * channels * 2 * 1;	/* colors * bytes_per_color * scan lines */

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
/*WM8199: gain=0.73; offset=-260mV*/
/*okay. the sensor black level is now at -260V. we only get 0 from AFE...*/
/* we should probably do real calibration here:
 * -detect acceptable offset with binary search
 * -calculate offset from this last version 
 *
 * acceptable offset means 
 *   - few completely black pixels(<10%?)
 *   - few completely white pixels(<10%?)
 *
 * final offset should map the minimum not completely black 
 * pixel to 0(16 bits)
 * 
 * this does account for dummy pixels at the end of ccd
 * this assumes slider is at black strip(which is not quite as black as "no
 * signal").
 *
 */
  dev->frontend.gain[0] = 0x00;
  dev->frontend.gain[1] = 0x00;
  dev->frontend.gain[2] = 0x00;
  offh[0] = 0xff;
  offh[1] = 0xff;
  offh[2] = 0xff;
  offl[0] = 0x00;
  offl[1] = 0x00;
  offl[2] = 0x00;
  turn = 0;

  do {

      for (j=0; j < channels; j++) {
	  off[j] = (offh[j]+offl[j])/2;
	  dev->frontend.offset[j] = off[j];
      }      
      
      status = gl841_set_fe(dev, AFE_SET);

      if (status != SANE_STATUS_GOOD)
      {
	  DBG (DBG_error,
	       "gl841_offset_calibration: Failed to setup frontend: %s\n",
	   sane_strstatus (status));
	  return status;
      }

      DBG (DBG_info,
	   "gl841_offset_calibration: starting first line reading\n");
      RIE (gl841_begin_scan (dev, dev->calib_reg, SANE_TRUE));

      RIE (sanei_genesys_read_data_from_scanner (dev, first_line, total_size));
    
      if (DBG_LEVEL >= DBG_data) {
	  snprintf(fn,20,"offset1_%d.pnm",turn);
	  sanei_genesys_write_pnm_file (fn,
					first_line,
					16,
					channels,
					num_pixels, 1);
      }
      
      acceptable = SANE_TRUE;
      
      for (j = 0; j < channels; j++)
      {
	  cmin[j] = 0;
	  cmax[j] = 0;

	  for (i = 0; i < num_pixels; i++)
	  {
	      if (dev->model->is_cis) 
		  val =
		      first_line[i * 2 + j * 2 * num_pixels + 1] * 256 +
		      first_line[i * 2 + j * 2 * num_pixels];
	      else
		  val =
		      first_line[i * 2 * channels + 2 * j + 1] * 256 +
		      first_line[i * 2 * channels + 2 * j];
	      if (val < 10)
		  cmin[j]++;
	      if (val > 65525)
		  cmax[j]++;
	  }

	  if (cmin[j] > num_pixels/100) {
	      acceptable = SANE_FALSE;
	      if (dev->model->is_cis)
		  offl[0] = off[0];
	      else
		  offl[j] = off[j];
	  } 
	  if (cmax[j] > num_pixels/100) {
	      acceptable = SANE_FALSE;
	      if (dev->model->is_cis)
		  offh[0] = off[0];
	      else
		  offh[j] = off[j];
	  }
      }

      DBG(DBG_info,"gl841_offset_calibration: black/white pixels: "
	  "%d/%d,%d/%d,%d/%d\n",
	  cmin[0],cmax[0],cmin[1],cmax[1],cmin[2],cmax[2]);

      if (dev->model->is_cis) {
	  offh[2] = offh[1] = offh[0];
	  offl[2] = offl[1] = offl[0];
      }

      RIE (gl841_end_scan (dev, dev->calib_reg, SANE_TRUE));

      turn++;

  } while (!acceptable && turn < 100);
      
  DBG(DBG_info,"gl841_offset_calibration: acceptable offsets: %d,%d,%d\n",
      off[0],off[1],off[2]);


  for (j = 0; j < channels; j++)
  {
      off1[j] = off[j];
      
      min1[j] = 65536;
      
      for (i = 0; i < num_pixels; i++) 
      {
	  if (dev->model->is_cis) 
	      val =
		  first_line[i * 2 + j * 2 * num_pixels + 1] * 256 +
		  first_line[i * 2 + j * 2 * num_pixels];
	  else
	      val =
		  first_line[i * 2 * channels + 2 * j + 1] * 256 +
		  first_line[i * 2 * channels + 2 * j];
	  if (min1[j] > val)
	      min1[j] = val;
      }
  }
  

  offl[0] = off[0];
  offl[1] = off[0];
  offl[2] = off[0];
  turn = 0;
  
  do {
      
      for (j=0; j < channels; j++) {
	  off[j] = (offh[j]+offl[j])/2;
	  dev->frontend.offset[j] = off[j];
      }      
      
      status = gl841_set_fe(dev, AFE_SET);

      if (status != SANE_STATUS_GOOD)
      {
	  DBG (DBG_error,
	       "gl841_offset_calibration: Failed to setup frontend: %s\n",
	   sane_strstatus (status));
	  return status;
      }
      
      DBG (DBG_info,
	   "gl841_offset_calibration: starting second line reading\n");
      RIE (gl841_begin_scan (dev, dev->calib_reg, SANE_TRUE));
      RIE (sanei_genesys_read_data_from_scanner (dev, second_line, total_size));
      
      if (DBG_LEVEL >= DBG_data) {
	  snprintf(fn,20,"offset2_%d.pnm",turn);
	  sanei_genesys_write_pnm_file (fn,
					second_line,
					16,
					channels,
					num_pixels, 1);
      }
      
      acceptable = SANE_TRUE;
      
      for (j = 0; j < channels; j++)
      {
	  cmin[j] = 0;
	  cmax[j] = 0;

	  for (i = 0; i < num_pixels; i++)
	  {
	      if (dev->model->is_cis) 
		  val =
		      second_line[i * 2 + j * 2 * num_pixels + 1] * 256 +
		      second_line[i * 2 + j * 2 * num_pixels];
	      else
		  val =
		      second_line[i * 2 * channels + 2 * j + 1] * 256 +
		      second_line[i * 2 * channels + 2 * j];
	      if (val < 10)
		  cmin[j]++;
	      if (val > 65525)
		  cmax[j]++;
	  }

	  if (cmin[j] > num_pixels/100) {
	      acceptable = SANE_FALSE;
	      if (dev->model->is_cis)
		  offl[0] = off[0];
	      else
		  offl[j] = off[j];
	  } 
	  if (cmax[j] > num_pixels/100) {
	      acceptable = SANE_FALSE;
	      if (dev->model->is_cis)
		  offh[0] = off[0];
	      else
		  offh[j] = off[j];
	  }
      }

      DBG(DBG_info,"gl841_offset_calibration: black/white pixels: "
	  "%d/%d,%d/%d,%d/%d\n",
	  cmin[0],cmax[0],cmin[1],cmax[1],cmin[2],cmax[2]);

      if (dev->model->is_cis) {
	  offh[2] = offh[1] = offh[0];
	  offl[2] = offl[1] = offl[0];
      }

      RIE (gl841_end_scan (dev, dev->calib_reg, SANE_TRUE));

      turn++;

  } while (!acceptable && turn < 100);
  
  DBG(DBG_info,"gl841_offset_calibration: acceptable offsets: %d,%d,%d\n",
      off[0],off[1],off[2]);


  for (j = 0; j < channels; j++)
  {
      off2[j] = off[j];
      
      min2[j] = 65536;

      for (i = 0; i < num_pixels; i++) 
      {
	  if (dev->model->is_cis) 
	      val =
		  second_line[i * 2 + j * 2 * num_pixels + 1] * 256 +
		  second_line[i * 2 + j * 2 * num_pixels];
	  else
	      val =
		  second_line[i * 2 * channels + 2 * j + 1] * 256 +
		  second_line[i * 2 * channels + 2 * j];
	  if (min2[j] > val)
	      min2[j] = val;
      }
  }

  DBG(DBG_info,"gl841_offset_calibration: first set: %d/%d,%d/%d,%d/%d\n",
      off1[0],min1[0],off1[1],min1[1],off1[2],min1[2]);

  DBG(DBG_info,"gl841_offset_calibration: second set: %d/%d,%d/%d,%d/%d\n",
      off2[0],min2[0],off2[1],min2[1],off2[2],min2[2]);

/* 
  calculate offset for each channel
  based on minimal pixel value min1 at offset off1 and minimal pixel value min2
  at offset off2
  
  to get min at off, values are linearly interpolated:
  min=real+off*fact
  min1=real+off1*fact
  min2=real+off2*fact

  fact=(min1-min2)/(off1-off2)
  real=min1-off1*(min1-min2)/(off1-off2)

  off=(min-min1+off1*(min1-min2)/(off1-off2))/((min1-min2)/(off1-off2))

  off=(min*(off1-off2)+min1*off2-off1*min2)/(min1-min2)

 */
  for (j = 0; j < channels; j++)
  {
      if (min2[j]-min1[j] == 0) {
/*TODO: try to avoid this*/
	  DBG(DBG_warn,"gl841_offset_calibration: difference too small\n");
	  if (mintgt * (off1[j] - off2[j]) + min1[j] * off2[j] - min2[j] * off1[j] >= 0)
	      off[j] = 0x0000;
	  else
	      off[j] = 0xffff;
      } else
	  off[j] = -(mintgt * (off1[j] - off2[j]) + min2[j] * off1[j] - min1[j] * off2[j])/(min2[j]-min1[j]);
      if (off[j] > 255)
	  off[j] = 255;
      if (off[j] < 0)
	  off[j] = 0;
      dev->frontend.offset[j] = off[j];
  }

  DBG(DBG_info,"gl841_offset_calibration: final offsets: %d,%d,%d\n",
      off[0],off[1],off[2]);

  if (dev->model->is_cis) {
      if (off[0] < off[1])
	  off[0] = off[1];
      if (off[0] < off[2])
	  off[0] = off[2];
      dev->frontend.offset[0] = off[0];
      dev->frontend.offset[1] = off[0];
      dev->frontend.offset[2] = off[0];
  }

  if (channels == 1)
    {
      dev->frontend.offset[1] = dev->frontend.offset[0];
      dev->frontend.offset[2] = dev->frontend.offset[0];
    }

  /* cleanup before return */
  free (first_line);
  free (second_line);
  DBG (DBG_proc, "gl841_offset_calibration: completed\n");
  return status;
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
gl841_coarse_gain_calibration (Genesys_Device * dev, int dpi)
{
  int num_pixels;
  int black_pixels;
  int total_size;
  u_int8_t *line;
  int i, j, channels;
  SANE_Status status = SANE_STATUS_GOOD;
  int max[3];
  float gain[3];
  int val;
  int used_res;

  DBG (DBG_proc, "gl841_coarse_gain_calibration\n");


  status = gl841_feed(dev, 280);/*feed to white strip. canon lide 35 only.*/
  
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl841_coarse_gain_calibration: Failed to feed: %s\n",
	   sane_strstatus (status));
      return status;
    }

  /* coarse gain calibration is allways done in color mode */
  channels = 3;

  status = gl841_init_scan_regs (dev,
				 dev->calib_reg,
				 dev->settings.xres,
				 dev->settings.yres,
				 0,
				 0,
				 (dev->sensor.sensor_pixels*dev->settings.xres) / dev->sensor.optical_res,
				 1,
				 16,
				 channels,
				 dev->settings.color_filter,
				 SCAN_FLAG_DISABLE_SHADING |
				 SCAN_FLAG_DISABLE_GAMMA |
				 SCAN_FLAG_SINGLE_LINE |
				 SCAN_FLAG_IGNORE_LINE_DISTANCE |
				 SCAN_FLAG_USE_OPTICAL_RES
      );

  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl841_coarse_calibration: Failed to setup scan: %s\n",
	   sane_strstatus (status));
      return status;
    }

  RIE (gl841_bulk_write_register
       (dev, dev->calib_reg, GENESYS_GL841_MAX_REGS));

  black_pixels =
    (dev->sensor.CCD_start_xoffset * dpi) / dev->sensor.optical_res;

  used_res = dev->current_setup.xres;
  num_pixels = dev->current_setup.pixels;

  total_size = num_pixels * channels * 2 * 1;	/* colors * bytes_per_color * scan lines */

  line = malloc (total_size);
  if (!line)
    return SANE_STATUS_NO_MEM;

  RIE (gl841_begin_scan (dev, dev->calib_reg, SANE_TRUE));
  RIE (sanei_genesys_read_data_from_scanner (dev, line, total_size));

  if (DBG_LEVEL >= DBG_data)
    sanei_genesys_write_pnm_file ("coarse.pnm", line, 16,
				  channels, num_pixels, 1);

  /* average high level for each channel and compute gain
     to reach the target code 
     we only use the central half of the CCD data         */
  for (j = 0; j < channels; j++)
    {
      max[j] = 0;
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

	  if (val > max[j])
	    max[j] = val;
	}

      gain[j] = 65535.0/max[j];

      if (dev->model->dac_type == DAC_CANONLIDE35) {
	  gain[j] *= 0.69;/*seems we don't get the real maximum. empirically derived*/
	  if (283 - 208/gain[j] > 255) 
	      dev->frontend.gain[j] = 255;
	  else if (283 - 208/gain[j] < 0)
	      dev->frontend.gain[j] = 0;
	  else
	      dev->frontend.gain[j] = 283 - 208/gain[j];
      }

      DBG (DBG_proc,
	   "gl841_coarse_gain_calibration: channel %d, max=%d, gain = %f, setting:%d\n",
	   j, max[j], gain[j],dev->frontend.gain[j]);
    }

  for (j = 0; j < channels; j++)
    {
      if(gain[j] > 10) 
        {
	  DBG (DBG_error0, "**********************************************\n");
	  DBG (DBG_error0, "**********************************************\n");
	  DBG (DBG_error0, "****                                      ****\n");
	  DBG (DBG_error0, "****  Extremely low Brightness detected.  ****\n");
	  DBG (DBG_error0, "****  Check the scanning head is          ****\n");
	  DBG (DBG_error0, "****  unlocked and moving.                ****\n");
	  DBG (DBG_error0, "****                                      ****\n");
	  DBG (DBG_error0, "**********************************************\n");
	  DBG (DBG_error0, "**********************************************\n");
	    
	  return SANE_STATUS_JAMMED;/* in search for something better */
        }
	
    }

  if (dev->model->is_cis) {
      if (dev->frontend.gain[0] > dev->frontend.gain[1])
	  dev->frontend.gain[0] = dev->frontend.gain[1];
      if (dev->frontend.gain[0] > dev->frontend.gain[2])
	  dev->frontend.gain[0] = dev->frontend.gain[2];
      dev->frontend.gain[2] = dev->frontend.gain[1] = dev->frontend.gain[0];
  }

  if (channels == 1)	
  {
      dev->frontend.gain[0] = dev->frontend.gain[1];
      dev->frontend.gain[2] = dev->frontend.gain[1];
  }
  
  free (line);

  RIE (gl841_end_scan (dev, dev->calib_reg, SANE_TRUE));

  gl841_slow_back_home(dev, SANE_TRUE);

  DBG (DBG_proc, "gl841_coarse_gain_calibration: completed\n");
  return status;
}

/*
 * wait for lamp warmup by scanning the same line until difference
 * between 2 scans is below a threshold
 */
static SANE_Status
gl841_init_regs_for_warmup (Genesys_Device * dev,
				       Genesys_Register_Set * local_reg,
				       int *channels, int *total_size)
{
  int num_pixels = (int) (4 * 300);
  SANE_Status status = SANE_STATUS_GOOD;

  DBG (DBG_proc, "sanei_gl841_warmup_lamp\n");

  memcpy (local_reg, dev->reg, (GENESYS_GL841_MAX_REGS + 1) * sizeof (Genesys_Register_Set));

/* okay.. these should be defaults stored somewhere */
  dev->frontend.gain[0] = 0x00;
  dev->frontend.gain[1] = 0x00;
  dev->frontend.gain[2] = 0x00;
  dev->frontend.offset[0] = 0x80;
  dev->frontend.offset[1] = 0x80;
  dev->frontend.offset[2] = 0x80;

  status = gl841_init_scan_regs (dev,
				 local_reg,
				 dev->sensor.optical_res,
				 dev->settings.yres,
				 dev->sensor.dummy_pixel,
				 0,
				 num_pixels,
				 1,
				 16,
				 *channels,
				 dev->settings.color_filter,
				 SCAN_FLAG_DISABLE_SHADING |
				 SCAN_FLAG_DISABLE_GAMMA |
				 SCAN_FLAG_SINGLE_LINE |
				 SCAN_FLAG_IGNORE_LINE_DISTANCE |
				 SCAN_FLAG_USE_OPTICAL_RES
      );

  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl841_init_regs_for_warmup: Failed to setup scan: %s\n",
	   sane_strstatus (status));
      return status;
    }

  num_pixels = dev->current_setup.pixels;

  *total_size = num_pixels * 3 * 2 * 1;	/* colors * bytes_per_color * scan lines */

  RIE (gl841_bulk_write_register
       (dev, local_reg, GENESYS_GL841_MAX_REGS));

  return status;
}


/*
 * this function moves head without scanning, forward, then backward
 * so that the head goes to park position.
 * as a by-product, also check for lock
 */
static SANE_Status
sanei_gl841_repark_head (Genesys_Device * dev)
{
  Genesys_Register_Set local_reg[GENESYS_GL841_MAX_REGS + 1];
  SANE_Status status;

  DBG (DBG_proc, "sanei_gl841_repark_head\n");
  
  status = gl841_feed(dev,232);

  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl841_repark_head: Failed to feed: %s\n",
	   sane_strstatus (status));
      return status;
    }

  /* toggle motor flag, put an huge step number and redo move backward */
  status = gl841_park_head (dev, local_reg, 1);
  DBG (DBG_proc, "gl841_park_head: completed\n");
  return status;
}



/* 
 * initialize ASIC : registers, motor tables, and gamma tables
 * then ensure scanner's head is at home
 */
static SANE_Status
gl841_init (Genesys_Device * dev)
{
  SANE_Status status;
  u_int8_t val;
  size_t size;
  u_int8_t *line;

  DBG_INIT ();
  DBG (DBG_proc, "gl841_init\n");

  /* Check if the device has already been initialized and powered up */
  if (dev->already_initialized)
    {
      RIE (sanei_genesys_get_status (dev, &val));
      if (val & REG41_PWRBIT)
	{
	  DBG (DBG_info, "gl841_init: already initialized\n");
	  return SANE_STATUS_GOOD;
	}
    }

  dev->dark_average_data = NULL;
  dev->white_average_data = NULL;

  dev->settings.color_filter = 0;

  /* Set default values for registers */
  gl841_init_registers (dev);

  /* Init device */
  val = 0x04;
  RIE (sanei_usb_control_msg (dev->dn, REQUEST_TYPE_OUT, REQUEST_REGISTER,
			      VALUE_INIT, INDEX, 1, &val));

  /* ASIC reset */
  RIE (sanei_genesys_write_register (dev, 0x0e, 0x00));

  /* Write initial registers */
  RIE (gl841_bulk_write_register
       (dev, dev->reg, GENESYS_GL841_MAX_REGS));

  /* Test ASIC and RAM */
  if (!(dev->model->flags & GENESYS_FLAG_LAZY_INIT))
    {
      RIE (sanei_gl841_asic_test (dev));
    }


  /* Set analog frontend */
  RIE (gl841_set_fe (dev, AFE_INIT));

  /* Move home */
  RIE (gl841_slow_back_home (dev, SANE_TRUE));

  /* Init shading data */
  RIE (sanei_genesys_init_shading_data (dev, dev->sensor.sensor_pixels));

  /* ensure head is correctly parked, and check lock */
  if (dev->model->flags & GENESYS_FLAG_REPARK)
    {
      status = sanei_gl841_repark_head (dev);
      if (status != SANE_STATUS_GOOD)
	{
	  if (status == SANE_STATUS_INVAL)
	    DBG (DBG_error0,
		 "Your scanner is locked. Please move the lock switch "
		 "to the unlocked position\n");
	  else
	    DBG (DBG_error,
		 "gl841_init: sanei_gl841_repark_head failed: %s\n",
		 sane_strstatus (status));
	  return status;
	}
    }

  size = 256;

  if (dev->sensor.red_gamma_table == NULL)
    {
      dev->sensor.red_gamma_table = (u_int16_t *) malloc (2 * size);
      if (dev->sensor.red_gamma_table == NULL)
	{
	  DBG (DBG_error,
	       "gl841_init: could not allocate memory for gamma table\n");
	  return SANE_STATUS_NO_MEM;
	}
      sanei_genesys_create_gamma_table (dev->sensor.red_gamma_table, size,
					65535, 65535,
					dev->sensor.red_gamma);
    }
  if (dev->sensor.green_gamma_table == NULL)
    {
      dev->sensor.green_gamma_table = (u_int16_t *) malloc (2 * size);
      if (dev->sensor.red_gamma_table == NULL)
	{
	  DBG (DBG_error,
	       "gl841_init: could not allocate memory for gamma table\n");
	  return SANE_STATUS_NO_MEM;
	}
      sanei_genesys_create_gamma_table (dev->sensor.green_gamma_table, size,
					65535, 65535,
					dev->sensor.green_gamma);
    }
  if (dev->sensor.blue_gamma_table == NULL)
    {
      dev->sensor.blue_gamma_table = (u_int16_t *) malloc (2 * size);
      if (dev->sensor.red_gamma_table == NULL)
	{
	  DBG (DBG_error,
	       "gl841_init: could not allocate memory for gamma table\n");
	  return SANE_STATUS_NO_MEM;
	}
      sanei_genesys_create_gamma_table (dev->sensor.blue_gamma_table, size,
					65535, 65535,
					dev->sensor.blue_gamma);
    }

  /* send gamma tables if needed */
  status = gl841_send_gamma_table (dev, 1);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl841_init: failed to send generic gamma tables: %s\n",
	   sane_strstatus (status));
      return status;
    }

  /* initial calibration reg values */
  memcpy (dev->calib_reg, dev->reg, (GENESYS_GL841_MAX_REGS + 1) * sizeof (Genesys_Register_Set));

  status = gl841_init_scan_regs (dev,
				 dev->calib_reg,
				 300,
				 300,
				 0,
				 0,
				 (16 * 300) / dev->sensor.optical_res,
				 1,
				 16,
				 3,
				 0,
				 SCAN_FLAG_DISABLE_SHADING |
				 SCAN_FLAG_DISABLE_GAMMA |
				 SCAN_FLAG_SINGLE_LINE |
				 SCAN_FLAG_IGNORE_LINE_DISTANCE |
				 SCAN_FLAG_USE_OPTICAL_RES
      );

  RIE (gl841_bulk_write_register
       (dev, dev->calib_reg, GENESYS_GL841_MAX_REGS));

  size = dev->current_setup.pixels * 3 * 2 * 1;	/* colors * bytes_per_color * scan lines */

  line = malloc (size);
  if (!line)
    return SANE_STATUS_NO_MEM;

  DBG (DBG_info,
       "gl841_init: starting dummy data reading\n");
  RIE (gl841_begin_scan (dev, dev->calib_reg, SANE_TRUE));

  sanei_usb_set_timeout(1000);/* 1 second*/

/*ignore errors. next read will succeed*/
  sanei_genesys_read_data_from_scanner (dev, line, size);

  sanei_usb_set_timeout(30 * 1000);/* 30 seconds*/

  RIE (gl841_end_scan (dev, dev->calib_reg, SANE_TRUE));

  free(line);

  memcpy (dev->calib_reg, dev->reg, (GENESYS_GL841_MAX_REGS + 1) * sizeof (Genesys_Register_Set));

  /* Set powersaving (default = 15 minutes) */
  RIE (gl841_set_powersaving (dev, 15));
  dev->already_initialized = SANE_TRUE;

  DBG (DBG_proc, "gl841_init: completed\n");
  return SANE_STATUS_GOOD;
}


/** the gl841 command set */
static Genesys_Command_Set gl841_cmd_set = {
  "gl841-generic",		/* the name of this set */

  gl841_init,
  gl841_init_regs_for_warmup,
  gl841_init_regs_for_coarse_calibration,
  gl841_init_regs_for_shading,
  gl841_init_regs_for_scan,

  gl841_get_filter_bit,
  gl841_get_lineart_bit,
  gl841_get_bitset_bit,
  gl841_get_gain4_bit,
  gl841_get_fast_feed_bit,
  gl841_test_buffer_empty_bit,
  gl841_test_motor_flag_bit,

  gl841_bulk_full_size,

  gl841_set_fe,
  gl841_set_powersaving,
  gl841_save_power,

  gl841_set_motor_power,
  gl841_set_lamp_power,

  gl841_begin_scan,
  gl841_end_scan,

  gl841_send_gamma_table,

  gl841_search_start_position,

  gl841_offset_calibration,
  gl841_coarse_gain_calibration,
  gl841_led_calibration,

  gl841_slow_back_home,
  gl841_park_head,

  gl841_bulk_write_register,
  gl841_bulk_write_data,
  gl841_bulk_read_data,
};

SANE_Status
sanei_gl841_init_cmd_set (Genesys_Device * dev)
{
  dev->model->cmd_set = &gl841_cmd_set;
  return SANE_STATUS_GOOD;
}
