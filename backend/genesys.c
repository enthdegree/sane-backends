/* sane - Scanner Access Now Easy.

   Copyright (C) 2003, 2004 Henning Meier-Geinitz <henning@meier-geinitz.de>
   Copyright (C) 2004, 2005 Gerhard Jaeger <gerhard@gjaeger.de>
   Copyright (C) 2004, 2005 Stephane Voltz <svoltz@numericable.fr>

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

/*
 * SANE backend for Genesys Logic GL646/GL841 based scanners
 */

#include "../include/sane/config.h"

#define BUILD 6

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"
#include "../include/sane/saneopts.h"

#define BACKEND_NAME genesys

#include "../include/sane/sanei_backend.h"
#include "../include/sane/sanei_usb.h"
#include "../include/sane/sanei_config.h"

#include "genesys.h"
#include "genesys_devices.c"

static SANE_Int num_devices = 0;
static Genesys_Device *first_dev = 0;
static Genesys_Scanner *first_handle = 0;
static const SANE_Device **devlist = 0;
/* Array of newly attached devices */
static Genesys_Device **new_dev = 0;
/* Length of new_dev array */
static SANE_Int new_dev_len = 0;
/* Number of entries alloced for new_dev */
static SANE_Int new_dev_alloced = 0;
/* Is this computer little-endian ?*/
SANE_Bool little_endian;

static SANE_String_Const mode_list[] = {
  SANE_I18N ("Color"),
  SANE_I18N ("Gray"),
  /* SANE_I18N ("Halftone"),  currently unused */
  SANE_I18N ("Lineart"),
  0
};

static SANE_String_Const source_list[] = {
  SANE_I18N ("Flatbed"),
  SANE_I18N ("Transparency Adapter"),
  0
};

static SANE_Range x_range = {
  SANE_FIX (0.0),		/* minimum */
  SANE_FIX (216.0),		/* maximum */
  SANE_FIX (0.0)		/* quantization */
};

static SANE_Range y_range = {
  SANE_FIX (0.0),		/* minimum */
  SANE_FIX (299.0),		/* maximum */
  SANE_FIX (0.0)		/* quantization */
};

static SANE_Range time_range = {
  0,				/* minimum */
  60,				/* maximum */
  0				/* quantization */
};

static const SANE_Range u8_range = {
  0,				/* minimum */
  255,				/* maximum */
  0				/* quantization */
};


/* ------------------------------------------------------------------------ */
/*                  functions calling ASIC specific functions               */
/* ------------------------------------------------------------------------ */

/*
 * returns true if filter bit is on (monochrome scan)
 */
static SANE_Status
genesys_init_cmd_set (Genesys_Device * dev)
{
  switch (dev->model->asic_type)
    {
    case GENESYS_GL646:
      return sanei_gl646_init_cmd_set (dev);
    case GENESYS_GL841:
      return sanei_gl841_init_cmd_set (dev);
    default:
      return SANE_STATUS_INVAL;
    }
}

/* ------------------------------------------------------------------------ */
/*                  General IO and debugging functions                      */
/* ------------------------------------------------------------------------ */

/* Write data to a pnm file (e.g. calibration). For debugging only */
SANE_Status
sanei_genesys_write_pnm_file (char *filename, u_int8_t * data, int depth,
			      int channels, int pixels_per_line, int lines)
{
  FILE *out;

  DBG (DBG_info,
       "sanei_genesys_write_pnm_file: depth=%d, channels=%d, ppl=%d, lines=%d\n",
       depth, channels, pixels_per_line, lines);

  out = fopen (filename, "w");
  if (!out)
    {
      DBG (DBG_error,
	   "sanei_genesys_write_pnm_file: could nor open %s for writing: %s\n",
	   filename, strerror (errno));
      return SANE_STATUS_INVAL;
    }
  fprintf (out, "P%c\n%d\n%d\n%d\n", channels == 1 ? '5' : '6',
	   pixels_per_line, lines, (int) pow (2, depth) - 1);
  if (channels == 3)
    {
      int count = 0;
      for (count = 0; count < (pixels_per_line * lines * 3); count++)
	{
	  if (depth == 16)
	    fputc (*(data + 1), out);
	  fputc (*(data++), out);
	  if (depth == 16)
	    data++;
	}
    }
  else
    {
      int count = 0;
      for (count = 0; count < (pixels_per_line * lines); count++)
	{
	  if (depth == 8)
	    {
	      fputc (*(data++), out);
	    }
	  else
	    {
	      fputc (*(data + 1), out);
	      fputc (*(data), out);
	      data += 2;
	    }
	}
    }
  fclose (out);

  DBG (DBG_proc, "sanei_genesys_write_pnm_file: finished\n");
  return SANE_STATUS_GOOD;
}

/* the following 2 functions are used to handle registers in a
   way that doesn't depend on the actual ASIC type */

/* Reads a register from a register set */
SANE_Byte
sanei_genesys_read_reg_from_set (Genesys_Register_Set * reg,
				 SANE_Byte address)
{
  SANE_Int i;

  for (i = 0; i < GENESYS_MAX_REGS; i++)
    {
      if (reg[i].address == address)
	{
	  return reg[i].value;
	}
    }
  return 0;
}

/* Reads a register from a register set */
void
sanei_genesys_set_reg_from_set (Genesys_Register_Set * reg, SANE_Byte address,
				SANE_Byte value)
{
  SANE_Int i;

  for (i = 0; i < GENESYS_MAX_REGS; i++)
    {
      if (reg[i].address == address)
	reg[i].value = value;
    }
}


/* ------------------------------------------------------------------------ */
/*                  Read and write RAM, registers and AFE                   */
/* ------------------------------------------------------------------------ */


/* Write to one register */
SANE_Status
sanei_genesys_write_register (Genesys_Device * dev, u_int8_t reg,
			      u_int8_t val)
{
  SANE_Status status;

  status =
    sanei_usb_control_msg (dev->dn, REQUEST_TYPE_OUT, REQUEST_REGISTER,
			   VALUE_SET_REGISTER, INDEX, 1, &reg);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "sanei_genesys_write_register (0x%02x, 0x%02x): failed while setting register: %s\n",
	   reg, val, sane_strstatus (status));
      return status;
    }

  status =
    sanei_usb_control_msg (dev->dn, REQUEST_TYPE_OUT, REQUEST_REGISTER,
			   VALUE_WRITE_REGISTER, INDEX, 1, &val);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "sanei_genesys_write_register (0x%02x, 0x%02x): failed while writing register value: %s\n",
	   reg, val, sane_strstatus (status));
      return status;
    }

  DBG (DBG_io, "sanei_genesys_write_register (0x%02x, 0x%02x) completed\n",
       reg, val);

  return status;
}


/* Read from one register */
SANE_Status
sanei_genesys_read_register (Genesys_Device * dev, u_int8_t reg,
			     u_int8_t * val)
{
  SANE_Status status;

  status =
    sanei_usb_control_msg (dev->dn, REQUEST_TYPE_OUT, REQUEST_REGISTER,
			   VALUE_SET_REGISTER, INDEX, 1, &reg);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "sanei_genesys_read_register (0x%02x, 0x%02x): failed while setting register: %s\n",
	   reg, *val, sane_strstatus (status));
      return status;
    }

  status =
    sanei_usb_control_msg (dev->dn, REQUEST_TYPE_IN, REQUEST_REGISTER,
			   VALUE_READ_REGISTER, INDEX, 1, val);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "sanei_genesys_read_register (0x%02x, 0x%02x): failed while reading register value: %s\n",
	   reg, *val, sane_strstatus (status));
      return status;
    }

  DBG (DBG_io, "sanei_genesys_read_register (0x%02x, 0x%02x) completed\n",
       reg, *val);

  return status;
}


/* Write to many registers */
SANE_Status
sanei_genesys_bulk_write_register (Genesys_Device * dev,
				   Genesys_Register_Set * reg, size_t size)
{
  SANE_Status status;
  u_int8_t outdata[8];
  unsigned int i;

  /* handle differently sized register sets, reg[0x00] is the last one */
  i = 0;
  while ((i < size / 2) && (reg[i].address != 0))
    i++;
  size = i * 2;

  DBG (DBG_io, "sanei_genesys_bulk_write_register (size = %lu)\n",
       (u_long) size);

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
	   "sanei_genesys_bulk_write_register: failed while writing command: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status = sanei_usb_write_bulk (dev->dn, (u_int8_t *) reg, &size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "sanei_genesys_bulk_write_register: failed while writing bulk data: %s\n",
	   sane_strstatus (status));
      return status;
    }

  for (i = 0; i < size / 2; i++)
    DBG (DBG_io2, "reg[0x%02x] = 0x%02x\n", ((u_int8_t *) reg)[2 * i],
	 ((u_int8_t *) reg)[2 * i + 1]);

  DBG (DBG_io, "sanei_genesys_bulk_write_register: wrote %d bytes\n", size);
  return status;
}


/* Set address for writing data */
SANE_Status
sanei_genesys_set_buffer_address (Genesys_Device * dev, u_int32_t addr)
{
  SANE_Status status;

  DBG (DBG_io,
       "sanei_genesys_set_buffer_address: setting address to 0x%05x\n",
       addr & 0xfffffff0);

  addr = addr >> 4;

  status = sanei_genesys_write_register (dev, 0x2b, (addr & 0xff));
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "sanei_genesys_set_buffer_address: failed while writing low byte: %s\n",
	   sane_strstatus (status));
      return status;
    }

  addr = addr >> 8;
  status = sanei_genesys_write_register (dev, 0x2a, (addr & 0xff));
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "sanei_genesys_set_buffer_address: failed while writing high byte: %s\n",
	   sane_strstatus (status));
      return status;
    }

  DBG (DBG_io, "sanei_genesys_set_buffer_address: completed\n");

  return status;
}

/* Write bulk data (e.g. shading, gamma) */
SANE_Status
sanei_genesys_bulk_write_data (Genesys_Device * dev, u_int8_t addr,
			       u_int8_t * data, size_t len)
{
  SANE_Status status;
  size_t size;
  u_int8_t outdata[8];

  DBG (DBG_io, "sanei_genesys_bulk_write_data writing %lu bytes\n",
       (u_long) len);

  status =
    sanei_usb_control_msg (dev->dn, REQUEST_TYPE_OUT, REQUEST_REGISTER,
			   VALUE_SET_REGISTER, INDEX, 1, &addr);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "sanei_genesys_bulk_write_data failed while setting register: %s\n",
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
	       "sanei_genesys_bulk_write_data failed while writing command: %s\n",
	       sane_strstatus (status));
	  return status;
	}

      status = sanei_usb_write_bulk (dev->dn, data, &size);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "sanei_genesys_bulk_write_data failed while writing bulk data: %s\n",
	       sane_strstatus (status));
	  return status;
	}

      DBG (DBG_io2,
	   "sanei_genesys_bulk_write_data wrote %lu bytes, %lu remaining\n",
	   (u_long) size, (u_long) (len - size));

      len -= size;
      data += size;
    }

  DBG (DBG_io, "sanei_genesys_bulk_write_data: completed\n");

  return status;
}


/* Read bulk data (e.g. scanned data) */
SANE_Status
sanei_genesys_bulk_read_data (Genesys_Device * dev, u_int8_t addr,
			      u_int8_t * data, size_t len)
{
  SANE_Status status;
  size_t size;
  u_int8_t outdata[8];

  DBG (DBG_io, "sanei_genesys_bulk_read_data: requesting %lu bytes\n",
       (u_long) len);

  status =
    sanei_usb_control_msg (dev->dn, REQUEST_TYPE_OUT, REQUEST_REGISTER,
			   VALUE_SET_REGISTER, INDEX, 1, &addr);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "sanei_genesys_bulk_read_data failed while setting register: %s\n",
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
	   "sanei_genesys_bulk_read_data failed while writing command: %s\n",
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
	   "sanei_genesys_bulk_read_data: trying to read %lu bytes of data\n",
	   (u_long) size);
      status = sanei_usb_read_bulk (dev->dn, data, &size);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "sanei_genesys_bulk_read_data failed while reading bulk data: %s\n",
	       sane_strstatus (status));
	  return status;
	}

      DBG (DBG_io2,
	   "sanei_genesys_bulk_read_data read %lu bytes, %lu remaining\n",
	   (u_long) size, (u_long) (len - size));

      len -= size;
      data += size;
    }

  DBG (DBG_io, "sanei_genesys_bulk_read_data: completed\n");

  return status;
}

void
sanei_genesys_init_structs (Genesys_Device * dev)
{

  /* initialize the sensor data stuff */
  memcpy (&dev->sensor, &Sensor[dev->model->ccd_type],
	  sizeof (Genesys_Sensor));

  /* initialize the GPO data stuff */
  memcpy (&dev->gpo, &Gpo[dev->model->gpo_type], sizeof (Genesys_Gpo));

  /* initialize the motor data stuff */
  memcpy (&dev->motor, &Motor[dev->model->motor_type],
	  sizeof (Genesys_Motor));
}

void
sanei_genesys_init_fe (Genesys_Device * dev)
{

  memcpy (&dev->frontend, &Wolfson[dev->model->dac_type],
	  sizeof (Genesys_Frontend));
}

/* Write data for analog frontend */
SANE_Status
sanei_genesys_fe_write_data (Genesys_Device * dev, u_int8_t addr,
			     u_int16_t data)
{
  SANE_Status status;
  u_int8_t outdata[6];

  DBG (DBG_io, "sanei_genesys_fe_write_data (0x%02x, 0x%04x)\n", addr, data);

  outdata[0] = 0x51;
  outdata[1] = addr;
  outdata[2] = 0x3a;
  outdata[3] = (data / 256) & 0xff;
  outdata[4] = 0x3b;
  outdata[5] = data & 0xff;

  status =
    sanei_genesys_bulk_write_register (dev, (Genesys_Register_Set *) outdata,
				       6);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "sanei_genesys_fe_write_data: Failed while bulk writing registers: %s\n",
	   sane_strstatus (status));
      return status;
    }

  DBG (DBG_io, "sanei_genesys_fe_write_data: completed\n");

  return status;
}

/* ------------------------------------------------------------------------ */
/*                       Medium level functions                             */
/* ------------------------------------------------------------------------ */

/** read the status register
 */
SANE_Status
sanei_genesys_get_status (Genesys_Device * dev, u_int8_t * status)
{
  return sanei_genesys_read_register (dev, 0x41, status);
}

/* returns pixels per line from register set */
static int
genesys_pixels_per_line (Genesys_Register_Set * reg)
{
  int pixels_per_line;

  pixels_per_line =
    sanei_genesys_read_reg_from_set (reg,
				     0x32) * 256 +
    sanei_genesys_read_reg_from_set (reg, 0x33);
  pixels_per_line -=
    (sanei_genesys_read_reg_from_set (reg, 0x30) * 256 +
     sanei_genesys_read_reg_from_set (reg, 0x31));

  return pixels_per_line;
}

/** read the number of valid words in scanner's RAM
 * ie registers 42-43-44
 */

static SANE_Status
genesys_read_valid_words (Genesys_Device * dev, int *words)
{
  SANE_Status status;
  u_int8_t value;

  DBG (DBG_proc, "genesys_read_valid_words\n");

  RIE (sanei_genesys_read_register (dev, 0x44, &value));
  *words = value;
  RIE (sanei_genesys_read_register (dev, 0x43, &value));
  *words += (value * 256);
  RIE (sanei_genesys_read_register (dev, 0x42, &value));
  if (dev->model->asic_type == GENESYS_GL646)
    *words += ((value & 0x03) * 256 * 256);
  else
    *words += ((value & 0x0f) * 256 * 256);

  DBG (DBG_proc, "genesys_read_valid_words: %d words\n", *words);
  return SANE_STATUS_GOOD;
}

Genesys_Register_Set *
sanei_genesys_get_address (Genesys_Register_Set * regs, SANE_Byte addr)
{
  int i;
  for (i = 0; i < GENESYS_MAX_REGS; i++)
    {
      if (regs[i].address == addr)
	return &regs[i];
    }
  return NULL;
}

SANE_Status
sanei_genesys_start_motor (Genesys_Device * dev)
{
  return sanei_genesys_write_register (dev, 0x0f, 0x01);
}

SANE_Status
sanei_genesys_stop_motor (Genesys_Device * dev)
{
  return sanei_genesys_write_register (dev, 0x0f, 0x00);
}

/* alternate slope table creation function        */
/* the hardcoded values (g and vstart) will go in a motor struct */
static SANE_Int
genesys_create_slope_table2 (Genesys_Device * dev,
			     u_int16_t * slope_table, int steps,
			     int step_type, int exposure_time,
			     SANE_Bool same_speed, double yres)
{
  double t, g;
  SANE_Int sum = 0;
  int vstart, vend;
  int i;

  DBG (DBG_proc,
       "sanei_genesys_create_slope_table2: %d steps, step_type = %d, "
       "exposure_time = %d, same_speed =%d\n", steps, step_type,
       exposure_time, same_speed);
  DBG (DBG_proc, "sanei_genesys_create_slope_table2: yres = %.2f\n", yres);

  /* start speed */
  if (dev->model->motor_type == MOTOR_5345)
    {
      if (yres < dev->motor.base_ydpi / 6)
	vstart = 2500;
      else
	vstart = 2000;
    }
  else
    {
      if (steps == 2)
	vstart = exposure_time;
      else if (steps == 3)
	vstart = 2 * exposure_time;
      else if (steps == 4)
	vstart = 1.5 * exposure_time;
      else if (steps == 120)
	vstart = 1.81674 * exposure_time;
      else
	vstart = exposure_time;
    }

  /* final speed */
  vend = (exposure_time * yres) / (dev->motor.base_ydpi * (1 << step_type));

  /*
     type=1 : full
     type=2 : half
     type=4 : quater
     vend * type * base_ydpi / exposure = yres
   */

  /* acceleration */
  switch (steps)
    {
    case 255:
      /* test for special case: fast moving slope */
      /* todo: a 'fast' boolean parameter should be better */
      if (vstart == 2000)
	g = 0.2013;
      else
	g = 0.1677;
      break;
    case 120:
      g = 0.5;
      break;
    case 67:
      g = 0.5;
      break;
    case 64:
      g = 0.2555;
      break;
    case 44:
      g = 0.5;
      break;
    case 4:
      g = 0.5;
      break;
    case 3:
      g = 1;
      break;
    case 2:
      vstart = vend;
      g = 1;
      break;
    default:
      g = 0.2635;
    }

  /* if same speed, no 'g' */
  sum = 0;
  if (same_speed)
    {
      for (i = 0; i < 255; i++)
	{
	  slope_table[i] = vend;
	  sum += slope_table[i];
	  DBG (DBG_io, "slope_table[%3d] = %5d\n", i, slope_table[i]);
	}
    }
  else
    {
      for (i = 0; i < steps; i++)
	{
	  t = pow (((double) i) / ((double) (steps - 1)), g);
	  slope_table[i] = vstart * (1 - t) + t * vend;
	  DBG (DBG_io, "slope_table[%3d] = %5d\n", i, slope_table[i]);
	  sum += slope_table[i];
	}
      for (i = steps; i < 255; i++)
	{
	  slope_table[i] = vend;
	  DBG (DBG_io, "slope_table[%3d] = %5d\n", i, slope_table[i]);
	  sum += slope_table[i];
	}
    }


  DBG (DBG_proc,
       "sanei_genesys_create_slope_table2: returns sum=%d, completed\n", sum);

  return sum;
}

/* Generate slope table for motor movement */
/* todo: check details */
SANE_Int
sanei_genesys_create_slope_table (Genesys_Device * dev,
				  u_int16_t * slope_table, int steps,
				  int step_type, int exposure_time,
				  SANE_Bool same_speed, double yres)
{
  double t;
  double start_speed;
  double g;
  u_int32_t time_period;
  int sum_time = 0;
  int i, divider;
  int same_step;

  dev = dev;

  if (dev->model->motor_type == MOTOR_5345
      || dev->model->motor_type == MOTOR_HP2300)
    return genesys_create_slope_table2 (dev, slope_table, steps,
					step_type, exposure_time,
					same_speed, yres);

  DBG (DBG_proc,
       "sanei_genesys_create_slope_table: %d steps, step_type = %d, "
       "exposure_time = %d, same_speed =%d\n", steps, step_type,
       exposure_time, same_speed);
  DBG (DBG_proc, "sanei_genesys_create_slope_table: yres = %.2f\n", yres);

  g = 0.6;
  start_speed = 0.01;
  same_step = 4;
  divider = 1 << step_type;

  time_period =
    (u_int32_t) (yres * exposure_time /
		 dev->motor.base_ydpi /*MOTOR_GEAR */ );
  if ((time_period < 2000) && (same_speed))
    same_speed = SANE_FALSE;

  time_period = time_period / divider;

  if (same_speed)
    {
      for (i = 0; i < steps; i++)
	{
	  slope_table[i] = (u_int16_t) time_period;
	  sum_time += time_period;

	  DBG (DBG_io, "slope_table[%d] = %d\n", i, time_period);
	}
      DBG (DBG_info,
	   "sanei_genesys_create_slope_table: returns sum_time=%d, completed\n",
	   sum_time);
      return sum_time;
    }

  if (time_period > MOTOR_SPEED_MAX * 5)
    {
      g = 1.0;
      start_speed = 0.05;
      same_step = 2;
    }
  else if (time_period > MOTOR_SPEED_MAX * 4)
    {
      g = 0.8;
      start_speed = 0.04;
      same_step = 2;
    }
  else if (time_period > MOTOR_SPEED_MAX * 3)
    {
      g = 0.7;
      start_speed = 0.03;
      same_step = 2;
    }
  else if (time_period > MOTOR_SPEED_MAX * 2)
    {
      g = 0.6;
      start_speed = 0.02;
      same_step = 3;
    }

  if (dev->model->motor_type == MOTOR_ST24)
    {
      steps = 255;
      switch ((int) yres)
	{
	case 2400:
	  g = 0.1672;
	  start_speed = 1.09;
	  break;
	case 1200:
	  g = 1;
	  start_speed = 6.4;
	  break;
	case 600:
	  g = 0.1672;
	  start_speed = 1.09;
	  break;
	case 400:
	  g = 0.2005;
	  start_speed = 20.0 / 3.0 /*7.5 */ ;
	  break;
	case 300:
	  g = 0.253;
	  start_speed = 2.182;
	  break;
	case 150:
	  g = 0.253;
	  start_speed = 4.367;
	  break;
	default:
	  g = 0.262;
	  start_speed = 7.29;
	}
      same_step = 1;
    }

  if (steps <= same_step)
    {
      time_period =
	(u_int32_t) (yres * exposure_time /
		     dev->motor.base_ydpi /*MOTOR_GEAR */ );
      time_period = time_period / divider;

      if (time_period > 65535)
	time_period = 65535;

      for (i = 0; i < same_step; i++)
	{
	  slope_table[i] = (u_int16_t) time_period;
	  sum_time += time_period;

	  DBG (DBG_io, "slope_table[%d] = %d\n", i, time_period);
	}

      DBG (DBG_proc,
	   "sanei_genesys_create_slope_table: returns sum_time=%d, completed\n",
	   sum_time);
      return sum_time;
    }

  for (i = 0; i < steps; i++)
    {
      double j = ((double) i) - same_step + 1;	/* start from 1/16 speed */

      if (j <= 0)
	t = 0;
      else
	t = pow (j / (steps - same_step), g);

      time_period =		/* time required for full steps */
	(u_int32_t) (yres * exposure_time /
		     dev->motor.base_ydpi /*MOTOR_GEAR */  *
		     (start_speed + (1 - start_speed) * t));

      time_period = time_period / divider;
      if (time_period > 65535)
	time_period = 65535;

      slope_table[i] = (u_int16_t) time_period;
      sum_time += time_period;

      DBG (DBG_io, "slope_table[%d] = %d\n", i, slope_table[i]);
    }

  DBG (DBG_proc,
       "sanei_genesys_create_slope_table: returns sum_time=%d, completed\n",
       sum_time);

  return sum_time;
}

/* computes gamma table */
void
sanei_genesys_create_gamma_table (u_int16_t * gamma_table, float size,
				  float maximum, float gamma_max, float gamma)
{
  int i;
  float value;

  for (i = 0; i < size; i++)
    {
      value = gamma_max * pow ((float) i / size, 1.0 / gamma);
      if (value > maximum)
	value = maximum;
      gamma_table[i] = value;
    }
}


/* computes the exposure_time on the basis of the given horizontal dpi */
/* we will clean/simplify it by using constants from a future motor struct */
SANE_Int
sanei_genesys_exposure_time (Genesys_Device * dev, Genesys_Register_Set * reg,
			     int xdpi)
{
  if (dev->model->motor_type == MOTOR_5345)
    {
      if (dev->model->cmd_set->get_filter_bit (reg))
	{
	  /* monochrome */
	  switch (xdpi)
	    {
	    case 600:
	      return 8500;
	    case 500:
	    case 400:
	    case 300:
	    case 250:
	    case 200:
	    case 150:
	      return 5500;
	    case 100:
	      return 6500;
	    case 50:
	      return 12000;
	    default:
	      return 11000;
	    }
	}
      else
	{
	  /* color scan */
	  switch (xdpi)
	    {
	    case 300:
	    case 250:
	    case 200:
	      return 5500;
	    case 50:
	      return 12000;
	    default:
	      return 11000;
	    }
	}
    }
  else if (dev->model->motor_type == MOTOR_HP2400)
    {
      if (dev->model->cmd_set->get_filter_bit (reg))
	{
	  /* monochrome */
	  switch (xdpi)
	    {
	    default:
	      return 11111;
	    }
	}
      else
	{
	  /* color scan */
	  switch (xdpi)
	    {
	    case 600:
	      return 11902;
	    default:
	      return 11111;
	    }
	}
    }
  else if (dev->model->motor_type == MOTOR_HP2300)
    {
      if (dev->model->cmd_set->get_filter_bit (reg))
	{
	  /* monochrome */
	  switch (xdpi)
	    {
	    case 600:
	      return 8699;	/* 3200; */
	    case 300:
	      return 3200;	/*10000;, 3200 -> too dark */
	    case 150:
	      return 4480;	/* 3200 ???, warmup needs 4480 */
	    case 75:
	      return 5500;
	    default:
	      return 11111;
	    }
	}
      else
	{
	  /* color scan */
	  switch (xdpi)
	    {
	    case 600:
	      return 8699;
	    case 300:
	      return 4349;
	    case 150:
	    case 75:
	      return 4480;
	    default:
	      return 11111;
	    }
	}
    }
  return dev->settings.exposure_time;
}



/* ? */
static SANE_Status
genesys_send_offset_and_shading (Genesys_Device * dev, u_int8_t * data,
				 int size)
{
  int dpihw;
  int start_address;
  SANE_Status status;

  DBG (DBG_proc, "genesys_send_offset_and_shading (size = %d)\n", size);

  dpihw = sanei_genesys_read_reg_from_set (dev->reg, 0x05) >> 6;

  if (dev->settings.scan_mode < 2 && dev->model->ccd_type != CCD_HP2300 && dev->model->ccd_type != CCD_HP2400 && dev->model->ccd_type != CCD_5345)	/* lineart, halftone */
    {
      if (dpihw == 0)		/* 600 dpi */
	start_address = 0x02a00;
      else if (dpihw == 1)	/* 1200 dpi */
	start_address = 0x05500;
      else if (dpihw == 2)	/* 2400 dpi */
	start_address = 0x0a800;
      else			/* reserved */
	return SANE_STATUS_INVAL;
    }
  else				/* color */
    start_address = 0x00;

  status = sanei_genesys_set_buffer_address (dev, start_address);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "genesys_send_offset_and_shading: failed to set buffer address: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status = sanei_genesys_bulk_write_data (dev, 0x3c, data, size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "genesys_send_offset_and_shading: failed to send shading table: %s\n",
	   sane_strstatus (status));
      return status;
    }

  DBG (DBG_proc, "genesys_send_offset_and_shading: completed\n");

  return SANE_STATUS_GOOD;
}

/* ? */
SANE_Status
sanei_genesys_init_shading_data (Genesys_Device * dev, int pixels_per_line)
{
  SANE_Status status;
  u_int8_t *shading_data, *shading_data_ptr;
  int channels;
  int i;

  DBG (DBG_proc, "sanei_genesys_init_shading_data (pixels_per_line = %d)\n",
       pixels_per_line);

  if (dev->settings.scan_mode >= 2)	/* 3 pass or single pass color */
    channels = 3;
  else
    channels = 1;

  shading_data = malloc (pixels_per_line * 4 * channels);	/* 16 bit black, 16 bit white */
  if (!shading_data)
    {
      DBG (DBG_error,
	   "sanei_genesys_init_shading_data: failed to allocate memory\n");
      return SANE_STATUS_NO_MEM;
    }

  shading_data_ptr = shading_data;

  for (i = 0; i < pixels_per_line * channels; i++)
    {
      *shading_data_ptr++ = 0x00;	/* dark lo */
      *shading_data_ptr++ = 0x00;	/* dark hi */
      *shading_data_ptr++ = 0x00;	/* white lo */
      *shading_data_ptr++ = 0x40;	/* white hi -> 0x4000 */
    }

  status =
    genesys_send_offset_and_shading (dev, shading_data,
				     pixels_per_line * 4 * channels);
  if (status != SANE_STATUS_GOOD)
    DBG (DBG_error,
	 "sanei_genesys_init_shading_data: failed to send shading data: %s\n",
	 sane_strstatus (status));

  free (shading_data);

  DBG (DBG_proc, "sanei_genesys_init_shading_data: completed\n");
  return status;
}

/* Checks if the scan buffer is empty */
SANE_Status
sanei_genesys_test_buffer_empty (Genesys_Device * dev, SANE_Bool * empty)
{
  u_int8_t val = 0;
  SANE_Status status;

  status = sanei_genesys_get_status (dev, &val);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "sanei_genesys_test_buffer_empty: failed to read buffer status: %s\n",
	   sane_strstatus (status));
      return status;
    }

  if (dev->model->cmd_set->test_buffer_empty_bit (val))
    {
      DBG (DBG_io2, "sanei_genesys_test_buffer_empty: buffer is empty\n");
      *empty = SANE_TRUE;
      return SANE_STATUS_GOOD;
    }

  *empty = SANE_FALSE;

  DBG (DBG_io, "sanei_genesys_test_buffer_empty: buffer is filled\n");
  return SANE_STATUS_GOOD;
}


/* Read data (e.g scanned image) from scan buffer */
SANE_Status
sanei_genesys_read_data_from_scanner (Genesys_Device * dev, u_int8_t * data,
				      size_t size)
{
  SANE_Status status;
  int time_count = 0;
  int words = 0;

  DBG (DBG_proc, "sanei_genesys_read_data_from_scanner (size = %lu bytes)\n",
       (u_long) size);

  /* wait until buffer not empty for up to 5 seconds */
  do
    {
      status = genesys_read_valid_words (dev, &words);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "sanei_genesys_read_data_from_scanner: checking for empty buffer failed: %s\n",
	       sane_strstatus (status));
	  return status;
	}
      if (words == 0)
	{
	  usleep (10000);	/* wait 10 msec */
	  time_count++;
	}
    }
  while ((time_count < 2500) && (words == 0));

  if (words == 0)		/* timeout, buffer does not get filled */
    {
      DBG (DBG_error,
	   "sanei_genesys_read_data_from_scanner: timeout, buffer does not get filled\n");
      return SANE_STATUS_IO_ERROR;
    }

  status = sanei_genesys_bulk_read_data (dev, 0x45, (u_int8_t *) data, size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "sanei_genesys_read_data_from_scanner: reading bulk data failed: %s\n",
	   sane_strstatus (status));
      return status;
    }

  DBG (DBG_proc, "sanei_genesys_read_data_from_scanner: completed\n");
  return SANE_STATUS_GOOD;
}

/* Find the position of the reference point: 
   takes gray level 8 bits data and find
   first CCD usable pixel and top of scanning area */
SANE_Status
sanei_genesys_search_reference_point (Genesys_Device * dev, u_int8_t * data,
				      int start_pixel, int dpi, int width,
				      int height)
{
  int x, y;
  int current, left, top = 0, bottom = 0;
  u_int8_t *image;
  int size;
  int level = 80;		/* edge threshold level */

  /*sanity check */
  if ((width < 3) || (height < 3))
    return SANE_STATUS_INVAL;

  /* transformed image data */
  size = width * height;
  image = malloc (size);
  if (!image)
    {
      DBG (DBG_error,
	   "sanei_genesys_search_reference_point: failed to allocate memory\n");
      return SANE_STATUS_NO_MEM;
    }

  /* laplace filter to denoise picture */
  for (y = 1; y < height - 1; y++)
    for (x = 1; x < width - 1; x++)
      {
	image[y * width + x] =
	  (data[(y - 1) * width + x + 1] + 2 * data[(y - 1) * width + x] +
	   data[(y - 1) * width + x - 1] + 2 * data[y * width + x + 1] +
	   4 * data[y * width + x] + 2 * data[y * width + x - 1] +
	   data[(y + 1) * width + x + 1] + 2 * data[(y + 1) * width + x] +
	   data[(y + 1) * width + x - 1]) / 16;
      }

  memcpy (data, image, size);
  if (DBG_LEVEL >= DBG_data)
    sanei_genesys_write_pnm_file ("laplace.pnm", image, 8, 1, width, height);

  /* apply X direction sobel filter 
     -1  0  1
     -2  0  2
     -1  0  1
   */
  for (y = 1; y < height - 1; y++)
    for (x = 1; x < width - 1; x++)
      {
	current =
	  data[(y - 1) * width + x + 1] - data[(y - 1) * width + x - 1] +
	  2 * data[y * width + x + 1] - 2 * data[y * width + x - 1] +
	  data[(y + 1) * width + x + 1] - data[(y + 1) * width + x - 1];
	if (current < 0)
	  current = -current;
	image[y * width + x] = current;
      }
  if (DBG_LEVEL >= DBG_data)
    sanei_genesys_write_pnm_file ("xsobel.pnm", image, 8, 1, width, height);

  /* find left black margin first
     todo: search top before left
     we average the result of N searches */
  left = 0;
  for (y = 1; y < 11; y++)
    {
      x = 8;
      while ((x < width / 2) && (image[y * width + x] < level))
	{
	  image[y * width + x] = 255;
	  x++;
	}
      left += x;
    }
  left = left / (y - 1);

  /* turn it in CCD pixel at full sensor optical resolution */
  dev->sensor.CCD_start_xoffset =
    start_pixel + (left * dev->sensor.optical_res) / dpi;

  /* find top edge by detecting black stripe */
  /* apply Y direction sobel filter 
     -1 -2 -1
     0  0  0
     1  2  1
   */
  for (y = 1; y < height - 1; y++)
    for (x = 1; x < width - 1; x++)
      {
	current =
	  -data[(y - 1) * width + x + 1] - 2 * data[(y - 1) * width + x] -
	  data[(y - 1) * width + x - 1] + data[(y + 1) * width + x + 1] +
	  2 * data[(y + 1) * width + x] + data[(y + 1) * width + x - 1];
	if (current < 0)
	  current = -current;
	image[y * width + x] = current;
      }
  if (DBG_LEVEL >= DBG_data)
    sanei_genesys_write_pnm_file ("ysobel.pnm", image, 8, 1, width, height);

  /* search top of horizontal black stripe */
  if (dev->model->ccd_type == CCD_5345
      && dev->model->motor_type == MOTOR_5345)
    {
      top = 0;
      for (x = width / 2; x < width - 1; x++)
	{
	  y = 2;
	  while ((y < height) && (image[x + y * width] < level))
	    y++;
	  top += y;
	}
      top = top / (width / 2 - 1);

      /* find bottom of black stripe */
      bottom = 0;
      for (x = width / 2; x < width - 1; x++)
	{
	  y = top + 5;
	  while ((y < height) && (image[x + y * width] < level))
	    y++;
	  bottom += y;
	}
      bottom = bottom / (width / 2 - 1);
      dev->model->y_offset_calib = SANE_FIX ((bottom * MM_PER_INCH) / dpi);
      DBG (DBG_info,
	   "sanei_genesys_search_reference_point: black stripe y_offset = %f mm \n",
	   SANE_UNFIX (dev->model->y_offset_calib));
    }

  /* find white corner in dark area */
  if (dev->model->ccd_type == CCD_HP2300
      && dev->model->motor_type == MOTOR_HP2300)
    {
      top = 0;
      for (x = 10; x < 60; x++)
	{
	  y = 2;
	  while ((y < height) && (image[x + y * width] < level))
	    y++;
	  top += y;
	}
      top = top / 50;
      dev->model->y_offset_calib = SANE_FIX ((top * MM_PER_INCH) / dpi);
      DBG (DBG_info,
	   "sanei_genesys_search_reference_point: white corner y_offset = %f mm \n",
	   SANE_UNFIX (dev->model->y_offset_calib));
    }

  free (image);
  DBG (DBG_proc,
       "sanei_genesys_search_reference_point: CCD_start_xoffset = %d, top = %d, bottom=%d\n",
       dev->sensor.CCD_start_xoffset, top, bottom);

  return SANE_STATUS_GOOD;
}


SANE_Status
sanei_genesys_read_feed_steps (Genesys_Device * dev, int *steps)
{
  SANE_Status status;
  u_int8_t value;

  DBG (DBG_proc, "sanei_genesys_read_feed_steps\n");

  RIE (sanei_genesys_read_register (dev, 0x4a, &value));
  *steps = value;
  RIE (sanei_genesys_read_register (dev, 0x49, &value));
  *steps += (value * 256);
  RIE (sanei_genesys_read_register (dev, 0x48, &value));
  if (dev->model->asic_type == GENESYS_GL646)
    *steps += ((value & 0x03) * 256 * 256);
  else
    *steps += ((value & 0x0f) * 256 * 256);

  DBG (DBG_proc, "sanei_genesys_read_feed_steps: %d steps\n", *steps);
  return SANE_STATUS_GOOD;
}


void
sanei_genesys_calculate_zmode2 (SANE_Bool two_table,
				u_int32_t exposure_time,
				u_int16_t * slope_table,
				int reg21,
				int move, int reg22, u_int32_t * z1,
				u_int32_t * z2)
{
  int i;
  int sum;
  DBG (DBG_info, "sanei_genesys_calculate_zmode2: two_table=%d\n", two_table);

  /* acceleration total time */
  sum = 0;
  for (i = 0; i < reg21; i++)
    sum += slope_table[i];

  /* compute Z1MOD */
  /* c=sum(slope_table;reg21)
     d=reg22*cruising speed
     Z1MOD=(c+d) % exposure_time */
  *z1 = (sum + reg22 * slope_table[reg21 - 1]) % exposure_time;

  /* compute Z2MOD */
  /* a=sum(slope_table;reg21), b=move or 1 if 2 tables */
  /* Z2MOD=(a+b) % exposure_time */
  if (!two_table)
    sum = sum + (move * slope_table[reg21 - 1]);
  else
    sum = sum + slope_table[reg21 - 1];
  *z2 = sum % exposure_time;
}


/* huh? */
/* todo: double check */
/* Z1 and Z2 seem to be a time to synchronize with clock or a phase correction */
/* steps_sum	is the result of create_slope_table 	*/
/* last_speed	is the last entry of the slope_table 	*/
/* feedl	is registers 3d,3e,3f 			 */
/* fastfed	is register 02 bit 3		 	*/
/* scanfed	is register 1f 				*/
/* fwdstep	is register 22 				*/
/* tgtime	is register 6c bit 6+7 >> 6 		*/

void
sanei_genesys_calculate_zmode (Genesys_Device * dev, u_int32_t exposure_time,
			       u_int32_t steps_sum, u_int16_t last_speed,
			       u_int32_t feedl, u_int8_t fastfed,
			       u_int8_t scanfed, u_int8_t fwdstep,
			       u_int8_t tgtime, u_int32_t * z1,
			       u_int32_t * z2)
{
  u_int8_t exposure_factor;

  dev = dev;

  exposure_factor = pow (2, tgtime);	/* todo: originally, this is always 2^0 ! */

  /* Z1 is for buffer-full backward forward moving */
  *z1 =
    exposure_factor * ((steps_sum + fwdstep * last_speed) % exposure_time);

  /* Z2 is for acceleration before scan */
  if (fastfed)			/* two curve mode */
    {
      *z2 =
	exposure_factor * ((steps_sum + scanfed * last_speed) %
			   exposure_time);
    }
  else				/* one curve mode */
    {
      *z2 =
	exposure_factor * ((steps_sum + feedl * last_speed) % exposure_time);
    }
}


static void
genesys_adjust_gain (Genesys_Device * dev, double *applied_multi,
		     u_int8_t * new_gain, double multi, u_int8_t gain)
{
  double voltage, original_voltage;

  DBG (DBG_proc, "genesys_adjust_gain: multi=%f, gain=%d\n", multi, gain);

  dev = dev;

  voltage = 0.5 + gain * 0.25;
  original_voltage = voltage;

  voltage *= multi;

  *new_gain = (u_int8_t) ((voltage - 0.5) * 4);
  if (*new_gain > 0x0e)
    *new_gain = 0x0e;

  voltage = 0.5 + (*new_gain) * 0.25;

  *applied_multi = voltage / original_voltage;

  DBG (DBG_proc,
       "genesys_adjust_gain: orig voltage=%.2f, new voltage=%.2f, "
       "*applied_multi=%f, *new_gain=%d\n", original_voltage, voltage,
       *applied_multi, *new_gain);
  return;
}


/* todo: is return status necessary (unchecked?) */
static SANE_Status
genesys_average_white (Genesys_Device * dev, int channels, int channel,
		       u_int8_t * data, int size, int *max_average)
{
  int gain_white_ref, sum, range;
  int average;
  int i;

  DBG (DBG_proc,
       "genesys_average_white: channels=%d, channel=%d, size=%d\n",
       channels, channel, size);

  range = size / 50;

  if (dev->settings.scan_method == 2)	/* transparency mode */
    gain_white_ref = dev->sensor.fau_gain_white_ref * 256;
  else
    gain_white_ref = dev->sensor.gain_white_ref * 256;

  if (range < 1)
    range = 1;

  size = size / (2 * range * channels);

  data += (channel * 2);

  *max_average = 0;

  while (size--)
    {
      sum = 0;
      for (i = 0; i < range; i++)
	{
	  sum += (*data);
	  sum += *(data + 1) * 256;
	  data += (2 * channels);	/* byte based */
	}

      average = (sum / range);
      if (average > *max_average)
	*max_average = average;
    }

  DBG (DBG_proc,
       "genesys_average_white: max_average=%d, gain_white_ref = %d, finished\n",
       *max_average, gain_white_ref);

  if (*max_average >= gain_white_ref)
    return SANE_STATUS_INVAL;

  return SANE_STATUS_GOOD;
}

/* todo: understand, values are too high */
static int
genesys_average_black (Genesys_Device * dev, int channel,
		       u_int8_t * data, int pixels)
{
  int i;
  int sum;
  int pixel_step;

  DBG (DBG_proc, "genesys_average_black: channel=%d, pixels=%d\n",
       channel, pixels);

  sum = 0;

  if (dev->settings.scan_mode == 4)	/* single pass color */
    {
      data += (channel * 2);
      pixel_step = 3 * 2;
    }
  else
    {
      pixel_step = 2;
    }

  for (i = 0; i < pixels; i++)
    {
      sum += *data;
      sum += *(data + 1) * 256;

      data += pixel_step;
    }

  DBG (DBG_proc, "genesys_average_black = %d\n", sum / pixels);

  return (int) (sum / pixels);
}


/* todo: check; it works but the lines 1, 2, and 3 are too dark even with the
   same offset and gain settings? */
static SANE_Status
genesys_coarse_calibration (Genesys_Device * dev)
{
  int size;
  int black_pixels;
  int white_average;
  int channels;
  SANE_Status status;
  u_int8_t offset[4] = { 0xa0, 0x00, 0xa0, 0x40 };	/* first value isn't used */
  u_int16_t white[12], dark[12];
  int i, j;
  u_int8_t *calibration_data, *all_data;

  DBG (DBG_info, "genesys_coarse_calibration (scan_mode = %d)\n",
       dev->settings.scan_mode);

  black_pixels = dev->sensor.black_pixels
    * dev->settings.xres / dev->sensor.optical_res;

  if (dev->settings.scan_mode == 4)	/* single pass color */
    channels = 3;
  else
    channels = 1;

  size = channels * 2 * dev->model->y_size * dev->settings.xres;

  calibration_data = malloc (size);
  if (!calibration_data)
    {
      DBG (DBG_error,
	   "genesys_coarse_calibration: failed to allocate memory\n");
      return SANE_STATUS_NO_MEM;
    }

  all_data = calloc (1, size * 4);

  status = dev->model->cmd_set->set_fe (dev, AFE_INIT);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "genesys_coarse_calibration: failed to set frontend: %s\n",
	   sane_strstatus (status));
      return status;
    }
  dev->frontend.sign[0] = dev->frontend.sign[1] = dev->frontend.sign[2] = 0;
  dev->frontend.gain[0] = dev->frontend.gain[1] = dev->frontend.gain[2] = 2;	/* todo: ?  was 2 */
  dev->frontend.offset[0] = dev->frontend.offset[1] =
    dev->frontend.offset[2] = offset[0];

  for (i = 0; i < 4; i++)	/* read 4 lines */
    {
      if (i < 3)		/* first 3 lines */
	dev->frontend.offset[0] = dev->frontend.offset[1] =
	  dev->frontend.offset[2] = offset[i];

      if (i == 1)		/* second line */
	{
	  double applied_multi;
	  double gain_white_ref;

	  if (dev->settings.scan_method == 2)	/* Transparency */
	    gain_white_ref = dev->sensor.fau_gain_white_ref * 256;
	  else
	    gain_white_ref = dev->sensor.gain_white_ref * 256;
	  /* white and black are defined downwards */

	  genesys_adjust_gain (dev, &applied_multi,
			       &dev->frontend.gain[0],
			       gain_white_ref / (white[0] - dark[0]),
			       dev->frontend.gain[0]);
	  genesys_adjust_gain (dev, &applied_multi,
			       &dev->frontend.gain[1],
			       gain_white_ref / (white[1] - dark[1]),
			       dev->frontend.gain[1]);
	  genesys_adjust_gain (dev, &applied_multi,
			       &dev->frontend.gain[2],
			       gain_white_ref / (white[2] - dark[2]),
			       dev->frontend.gain[2]);

	  dev->frontend.gain[0] = dev->frontend.gain[1] =
	    dev->frontend.gain[2] = 2;

	  status =
	    sanei_genesys_fe_write_data (dev, 0x28, dev->frontend.gain[0]);
	  if (status != SANE_STATUS_GOOD)	/* todo: this was 0x28 + 3 ? */
	    {
	      DBG (DBG_error,
		   "genesys_coarse_calibration: Failed to write gain[0]: %s\n",
		   sane_strstatus (status));
	      return status;
	    }

	  status =
	    sanei_genesys_fe_write_data (dev, 0x29, dev->frontend.gain[1]);
	  if (status != SANE_STATUS_GOOD)
	    {
	      DBG (DBG_error,
		   "genesys_coarse_calibration: Failed to write gain[1]: %s\n",
		   sane_strstatus (status));
	      return status;
	    }

	  status =
	    sanei_genesys_fe_write_data (dev, 0x2a, dev->frontend.gain[2]);
	  if (status != SANE_STATUS_GOOD)
	    {
	      DBG (DBG_error,
		   "genesys_coarse_calibration: Failed to write gain[2]: %s\n",
		   sane_strstatus (status));
	      return status;
	    }
	}

      if (i == 3)		/* last line */
	{
	  double x, y, rate;

	  for (j = 0; j < 3; j++)
	    {

	      x =
		(double) (dark[(i - 2) * 3 + j] -
			  dark[(i - 1) * 3 + j]) * 254 / (offset[i - 1] / 2 -
							  offset[i - 2] / 2);
	      y = x - x * (offset[i - 1] / 2) / 254 - dark[(i - 1) * 3 + j];
	      rate = (x - DARK_VALUE - y) * 254 / x + 0.5;

	      dev->frontend.offset[j] = (u_int8_t) (rate);

	      if (dev->frontend.offset[j] > 0x7f)
		dev->frontend.offset[j] = 0x7f;
	      dev->frontend.offset[j] <<= 1;
	    }
	}
      status =
	sanei_genesys_fe_write_data (dev, 0x20, dev->frontend.offset[0]);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "genesys_coarse_calibration: Failed to write offset[0]: %s\n",
	       sane_strstatus (status));
	  return status;
	}

      status =
	sanei_genesys_fe_write_data (dev, 0x21, dev->frontend.offset[1]);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "genesys_coarse_calibration: Failed to write offset[1]: %s\n",
	       sane_strstatus (status));
	  return status;
	}

      status =
	sanei_genesys_fe_write_data (dev, 0x22, dev->frontend.offset[2]);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "genesys_coarse_calibration: Failed to write offset[2]: %s\n",
	       sane_strstatus (status));
	  return status;
	}

      DBG (DBG_info,
	   "genesys_coarse_calibration: doing scan: sign: %d/%d/%d, gain: %d/%d/%d, offset: %d/%d/%d\n",
	   dev->frontend.sign[0], dev->frontend.sign[1],
	   dev->frontend.sign[2], dev->frontend.gain[0],
	   dev->frontend.gain[1], dev->frontend.gain[2],
	   dev->frontend.offset[0], dev->frontend.offset[1],
	   dev->frontend.offset[2]);

      status =
	dev->model->cmd_set->begin_scan (dev, dev->calib_reg, SANE_FALSE);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "genesys_coarse_calibration: Failed to begin scan: %s\n",
	       sane_strstatus (status));
	  return status;
	}

      status =
	sanei_genesys_read_data_from_scanner (dev, calibration_data, size);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "genesys_coarse_calibration: Failed to read data: %s\n",
	       sane_strstatus (status));
	  return status;
	}

      memcpy (all_data + i * size, calibration_data, size);
      if (i == 3)		/* last line */
	{
	  SANE_Byte *all_data_8 = malloc (size * 4 / 2);
	  unsigned int count;

	  for (count = 0; count < (unsigned int) (size * 4 / 2); count++)
	    all_data_8[count] = all_data[count * 2 + 1];
	  status =
	    sanei_genesys_write_pnm_file ("coarse.pnm", all_data_8, 8,
					  channels, size / 6, 4);
	  if (status != SANE_STATUS_GOOD)
	    {
	      DBG (DBG_error,
		   "genesys_coarse_calibration: sanei_genesys_write_pnm_file failed: %s\n",
		   sane_strstatus (status));
	      return status;
	    }
	}

      status = dev->model->cmd_set->end_scan (dev, dev->calib_reg, SANE_TRUE);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "genesys_coarse_calibration: Failed to end scan: %s\n",
	       sane_strstatus (status));
	  return status;
	}
      if (dev->settings.scan_mode == 4)	/* single pass color */
	{
	  for (j = 0; j < 3; j++)
	    {
	      genesys_average_white (dev, 3, j, calibration_data, size,
				     &white_average);
	      white[i * 3 + j] = white_average;
	      dark[i * 3 + j] =
		genesys_average_black (dev, j, calibration_data,
				       black_pixels);
	      DBG (DBG_info,
		   "genesys_coarse_calibration: white[%d]=%d, black[%d]=%d\n",
		   i * 3 + j, white[i * 3 + j], i * 3 + j, dark[i * 3 + j]);
	    }
	}
      else			/* one color-component modes */
	{
	  genesys_average_white (dev, 1, 0, calibration_data, size,
				 &white_average);
	  white[i * 3 + 0] = white[i * 3 + 1] = white[i * 3 + 2] =
	    white_average;
	  dark[i * 3 + 0] = dark[i * 3 + 1] = dark[i * 3 + 2] =
	    genesys_average_black (dev, 0, calibration_data, black_pixels);
	}

      if (i == 3)
	{
	  if (dev->settings.scan_mode == 4)	/* single pass color */
	    {
	      /* todo: huh? */
	      dev->dark[0] =
		(u_int16_t) (1.6925 * dark[i * 3 + 0] + 0.1895 * 256);
	      dev->dark[1] =
		(u_int16_t) (1.4013 * dark[i * 3 + 1] + 0.3147 * 256);
	      dev->dark[2] =
		(u_int16_t) (1.2931 * dark[i * 3 + 2] + 0.1558 * 256);
	    }
	  else			/* one color-component modes */
	    {
	      switch (dev->settings.color_filter)
		{
		case 0:
		default:
		  dev->dark[0] =
		    (u_int16_t) (1.6925 * dark[i * 3 + 0] +
				 (1.1895 - 1.0) * 256);
		  dev->dark[1] = dev->dark[2] = dev->dark[0];
		  break;

		case 1:
		  dev->dark[1] =
		    (u_int16_t) (1.4013 * dark[i * 3 + 1] +
				 (1.3147 - 1.0) * 256);
		  dev->dark[0] = dev->dark[2] = dev->dark[1];
		  break;

		case 2:
		  dev->dark[2] =
		    (u_int16_t) (1.2931 * dark[i * 3 + 2] +
				 (1.1558 - 1.0) * 256);
		  dev->dark[0] = dev->dark[1] = dev->dark[2];
		  break;
		}
	    }
	}
    }				/* for (i = 0; i < 4; i++) */

  DBG (DBG_info,
       "genesys_coarse_calibration: final: sign: %d/%d/%d, gain: %d/%d/%d, offset: %d/%d/%d\n",
       dev->frontend.sign[0], dev->frontend.sign[1], dev->frontend.sign[2],
       dev->frontend.gain[0], dev->frontend.gain[1], dev->frontend.gain[2],
       dev->frontend.offset[0], dev->frontend.offset[1],
       dev->frontend.offset[2]);
  DBG (DBG_proc, "genesys_coarse_calibration: completed\n");

  return status;
}


static void
genesys_average_data (u_int8_t * average_data,
		      u_int8_t * calibration_data, u_int16_t lines,
		      u_int16_t pixel_components_per_line)
{
  int x, y;
  u_int32_t sum;

  for (x = 0; x < pixel_components_per_line; x++)
    {
      sum = 0;
      for (y = 0; y < lines; y++)
	{
	  sum += calibration_data[(x + y * pixel_components_per_line) * 2];
	  sum +=
	    calibration_data[(x + y * pixel_components_per_line) * 2 +
			     1] * 256;
	}
      sum /= lines;
      *average_data++ = sum & 255;
      *average_data++ = sum / 256;
    }
}

static SANE_Status
genesys_dark_shading_calibration (Genesys_Device * dev)
{
  SANE_Status status;
  size_t size;
  u_int16_t pixels_per_line;
  u_int8_t channels;
  u_int8_t *calibration_data;

  DBG (DBG_proc, "genesys_dark_shading_calibration\n");
  /* end pixel - start pixel */
  pixels_per_line = genesys_pixels_per_line (dev->calib_reg);

  if (dev->settings.scan_mode == 4)	/* single pass color */
    channels = 3;
  else
    channels = 1;

  if (dev->dark_average_data)
    free (dev->dark_average_data);

  dev->dark_average_data = malloc (channels * 2 * pixels_per_line);
  if (!dev->dark_average_data)
    {
      DBG (DBG_error,
	   "genesys_dark_shading_calibration: failed to allocate average memory\n");
      return SANE_STATUS_NO_MEM;
    }

  /* size is size in bytes for scanarea: bytes_per_line * lines */
  size = channels * 2 * pixels_per_line * dev->model->shading_lines;

  calibration_data = malloc (size);
  if (!calibration_data)
    {
      DBG (DBG_error, "genesys_dark_shading_calibration: "
	   "failed to allocate calibration data memory\n");
      return SANE_STATUS_NO_MEM;
    }

  /* turn off motor and lamp power */
  dev->model->cmd_set->set_lamp_power (dev->calib_reg, SANE_FALSE);
  dev->model->cmd_set->set_motor_power (dev->calib_reg, SANE_FALSE);

  status =
    sanei_genesys_bulk_write_register (dev, dev->calib_reg,
				       dev->model->cmd_set->
				       bulk_full_size () * 2);
  if (status != SANE_STATUS_GOOD)
    {
      free (calibration_data);
      DBG (DBG_error,
	   "genesys_dark_shading_calibration: Failed to bulk write registers: %s\n",
	   sane_strstatus (status));
      return status;
    }

  usleep (200 * 1000);		/* wait 200 ms: lamp needs some time to get dark */

  status = dev->model->cmd_set->begin_scan (dev, dev->calib_reg, SANE_TRUE);
  if (status != SANE_STATUS_GOOD)
    {
      free (calibration_data);
      DBG (DBG_error,
	   "genesys_dark_shading_calibration: Failed to begin scan: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status = sanei_genesys_read_data_from_scanner (dev, calibration_data, size);
  if (status != SANE_STATUS_GOOD)
    {
      free (calibration_data);
      DBG (DBG_error,
	   "genesys_dark_shading_calibration: Failed to read data: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status = dev->model->cmd_set->end_scan (dev, dev->calib_reg, SANE_TRUE);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "genesys_dark_shading_calibration: Failed to end scan: %s\n",
	   sane_strstatus (status));
      return status;
    }

  dev->model->cmd_set->set_motor_power (dev->calib_reg, SANE_FALSE);
  dev->model->cmd_set->set_lamp_power (dev->calib_reg, SANE_TRUE);

  status =
    sanei_genesys_bulk_write_register (dev, dev->calib_reg,
				       dev->model->cmd_set->
				       bulk_full_size () * 2);
  if (status != SANE_STATUS_GOOD)
    {
      free (calibration_data);
      DBG (DBG_error,
	   "genesys_dark_shading_calibration: Failed to bulk write registers: %s\n",
	   sane_strstatus (status));
      return status;
    }

  if (DBG_LEVEL >= DBG_data)
    sanei_genesys_write_pnm_file ("black_shading.pnm", calibration_data, 16,
				  channels, pixels_per_line,
				  dev->model->shading_lines);

  genesys_average_data (dev->dark_average_data, calibration_data,
			dev->model->shading_lines,
			pixels_per_line * channels);

  if (DBG_LEVEL >= DBG_data)
    sanei_genesys_write_pnm_file ("black_average.pnm", dev->dark_average_data,
				  16, channels, pixels_per_line, 1);

  free (calibration_data);

  DBG (DBG_proc, "genesys_dark_shading_calibration: completed\n");

  return SANE_STATUS_GOOD;
}

/*
 * this function builds dummy dark calibration data so that we can
 * compute shading coefficient in a clean way
 *  todo: current values are hardcoded, we have to find if they 
 * can be computed from previous calibration data (when doing offset
 * calibration ?)
 */
static SANE_Status
genesys_dummy_dark_shading (Genesys_Device * dev)
{
  u_int16_t pixels_per_line;
  u_int8_t channels;
  int x, skip, xend;
  int dummy1, dummy2, dummy3;	/* dummy black average per channel */

  DBG (DBG_proc, "genesys_dummy_dark_shading \n");

  pixels_per_line = genesys_pixels_per_line (dev->calib_reg);

  if (dev->settings.scan_mode == 4)	/* single pass color */
    channels = 3;
  else
    channels = 1;

  if (dev->dark_average_data)
    free (dev->dark_average_data);

  dev->dark_average_data = malloc (channels * 2 * pixels_per_line);
  if (!dev->dark_average_data)
    {
      DBG (DBG_error,
	   "genesys_dummy_dark_shading: failed to allocate average memory\n");
      return SANE_STATUS_NO_MEM;
    }
  memset (dev->dark_average_data, 0x00, channels * 2 * pixels_per_line);

  /* we average values on 'the left' where CCD pixels are under casing and
     give darkest values. We then use these as dummy dark calibration */
  if (dev->settings.xres <= dev->sensor.optical_res / 2)
    {
      skip = 4;
      xend = 36;
    }
  else
    {
      skip = 4;
      xend = 68;
    }

  /* average each channels on half left margin */
  dummy1 = 0;
  dummy2 = 0;
  dummy3 = 0;

  for (x = skip + 1; x <= xend; x++)
    {
      dummy1 +=
	dev->white_average_data[channels * 2 * x] +
	256 * dev->white_average_data[channels * 2 * x + 1];
      if (channels > 1)
	{
	  dummy2 +=
	    (dev->white_average_data[channels * 2 * x + 2] +
	     256 * dev->white_average_data[channels * 2 * x + 3]);
	  dummy3 +=
	    (dev->white_average_data[channels * 2 * x + 4] +
	     256 * dev->white_average_data[channels * 2 * x + 5]);
	}
    }

  dummy1 /= (xend - skip);
  if (channels > 1)
    {
      dummy2 /= (xend - skip);
      dummy3 /= (xend - skip);
    }
  DBG (DBG_proc,
       "genesys_dummy_dark_shading: dummy1=%d, dummy2=%d, dummy3=%d \n",
       dummy1, dummy2, dummy3);

  /* fill dark_average */
  for (x = 0; x < pixels_per_line; x++)
    {
      dev->dark_average_data[channels * 2 * x] = dummy1 & 0xff;
      dev->dark_average_data[channels * 2 * x + 1] = dummy1 >> 8;
      if (channels > 1)
	{
	  dev->dark_average_data[channels * 2 * x + 2] = dummy2 & 0xff;
	  dev->dark_average_data[channels * 2 * x + 3] = dummy2 >> 8;
	  dev->dark_average_data[channels * 2 * x + 4] = dummy3 & 0xff;
	  dev->dark_average_data[channels * 2 * x + 5] = dummy3 >> 8;
	}
    }

  DBG (DBG_proc, "genesys_dummy_dark_shading: completed \n");
  return SANE_STATUS_GOOD;
}


static SANE_Status
genesys_white_shading_calibration (Genesys_Device * dev)
{
  SANE_Status status;
  size_t size;
  u_int16_t pixels_per_line;
  u_int8_t *calibration_data;
  u_int8_t channels;

  DBG (DBG_proc, "genesys_white_shading_calibration (lines = %d)\n",
       dev->model->shading_lines);

  pixels_per_line = genesys_pixels_per_line (dev->calib_reg);

  if (dev->settings.scan_mode == 4)	/* single pass color */
    channels = 3;
  else
    channels = 1;

  if (dev->white_average_data)
    free (dev->white_average_data);

  dev->white_average_data = malloc (channels * 2 * pixels_per_line);
  if (!dev->white_average_data)
    {
      DBG (DBG_error,
	   "genesys_white_shading_calibration: failed to allocate average memory\n");
      return SANE_STATUS_NO_MEM;
    }

  size = channels * 2 * pixels_per_line * dev->model->shading_lines;

  calibration_data = malloc (size);
  if (!calibration_data)
    {
      DBG (DBG_error,
	   "genesys_white_shading_calibration: failed to allocate calibration memory\n");
      return SANE_STATUS_NO_MEM;
    }

  /* turn on motor and lamp power */
  dev->model->cmd_set->set_lamp_power (dev->calib_reg, SANE_TRUE);
  dev->model->cmd_set->set_motor_power (dev->calib_reg, SANE_TRUE);

  status =
    sanei_genesys_bulk_write_register (dev, dev->calib_reg,
				       dev->model->cmd_set->
				       bulk_full_size () * 2);
  if (status != SANE_STATUS_GOOD)
    {
      free (calibration_data);
      DBG (DBG_error,
	   "genesys_white_shading_calibration: failed to bulk write registers: %s\n",
	   sane_strstatus (status));
      return status;
    }

  if (dev->model->flags & GENESYS_FLAG_DARK_CALIBRATION)
    usleep (500 * 1000);	/* wait 500ms to make sure lamp is bright again */

  status = dev->model->cmd_set->begin_scan (dev, dev->calib_reg, SANE_FALSE);
  if (status != SANE_STATUS_GOOD)
    {
      free (calibration_data);
      DBG (DBG_error,
	   "genesys_white_shading_calibration: Failed to begin scan: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status = sanei_genesys_read_data_from_scanner (dev, calibration_data, size);
  if (status != SANE_STATUS_GOOD)
    {
      free (calibration_data);
      DBG (DBG_error,
	   "genesys_white_shading_calibration: Failed to read data: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status = dev->model->cmd_set->end_scan (dev, dev->calib_reg, SANE_TRUE);
  if (status != SANE_STATUS_GOOD)
    {
      free (calibration_data);
      DBG (DBG_error,
	   "genesys_white_shading_calibration: Failed to end scan: %s\n",
	   sane_strstatus (status));
      return status;
    }

  if (DBG_LEVEL >= DBG_data)
    sanei_genesys_write_pnm_file ("white_shading.pnm", calibration_data, 16,
				  channels, pixels_per_line,
				  dev->model->shading_lines);

  genesys_average_data (dev->white_average_data, calibration_data,
			dev->model->shading_lines,
			pixels_per_line * channels);

  if (DBG_LEVEL >= DBG_data)
    sanei_genesys_write_pnm_file ("white_average.pnm",
				  dev->white_average_data, 16, channels,
				  pixels_per_line, 1);

  free (calibration_data);

  /* in case we haven't done dark calibration, build dummy data from white_average */
  if (!(dev->model->flags & GENESYS_FLAG_DARK_CALIBRATION))
    {
      status = genesys_dummy_dark_shading (dev);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "genesys_white_shading_calibration: failed to do dummy dark shading calibration: %s\n",
	       sane_strstatus (status));
	  return status;
	}
    }

  DBG (DBG_proc, "genesys_white_shading_calibration: completed\n");

  return SANE_STATUS_GOOD;
}


static SANE_Status
genesys_send_shading_coefficient (Genesys_Device * dev)
{
  SANE_Status status;
  u_int16_t pixels_per_line;
  u_int8_t *shading_data;
  u_int8_t channels;
  int x, i;
  unsigned int coeff, target_code, val;

  DBG (DBG_proc, "genesys_send_shading_coefficient\n");


  pixels_per_line = genesys_pixels_per_line (dev->calib_reg);

  if (dev->settings.scan_mode == 4)	/* single pass color */
    channels = 3;
  else
    channels = 1;

  /* we allways build data for three channels, even for gray */
  shading_data = malloc (pixels_per_line * 4 * 3);	/* 16 bit black, 16 bit white */
  if (!shading_data)
    {
      DBG (DBG_error,
	   "genesys_send_shading_coefficient: failed to allocate memory\n");
      return SANE_STATUS_NO_MEM;
    }

  /* TARGET/(Wn-Dn) = white gain -> ~1.xxx then it is multiplied by 0x2000
     or 0x4000 to give an integer 
     Wn = white average for column n
     Dn = dark average for column n
   */
  if (dev->model->cmd_set->get_gain4_bit (dev->calib_reg))
    coeff = 0x4000;
  else
    coeff = 0x2000;
  switch (dev->model->ccd_type)
    {
    case CCD_5345:
      /* from the logs, we can see that the first 2 coefficients are zeroed, 
         which surely means that coeff depends in a way or another on the 2 previous
         pixel columns */
      /* depends upon R06 shading gain */

      target_code = 0xFA00;
      memset (shading_data, 0x00, pixels_per_line * 4 * channels);

      /* todo: if GAIN4, shading data is 'chunky', while it is 'line' when
         GAIN4 is reset */
      for (x = 2; x < pixels_per_line; x++)
	{
	  /* dark data */
	  shading_data[x * 2 * 2 * 3] =
	    dev->dark_average_data[x * 2 * channels];
	  shading_data[x * 2 * 2 * 3 + 1] =
	    dev->dark_average_data[x * 2 * channels + 1];
	  if (channels > 1)
	    {
	      shading_data[x * 2 * 2 * 3 + 4] =
		dev->dark_average_data[x * 2 * channels + 2];
	      shading_data[x * 2 * 2 * 3 + 5] =
		dev->dark_average_data[x * 2 * channels + 3];
	      shading_data[x * 2 * 2 * 3 + 8] =
		dev->dark_average_data[x * 2 * channels + 4];
	      shading_data[x * 2 * 2 * 3 + 9] =
		dev->dark_average_data[x * 2 * channels + 5];
	    }
	  else
	    {
	      shading_data[x * 2 * 2 * 3 + 4] =
		dev->dark_average_data[x * 2 * channels];
	      shading_data[x * 2 * 2 * 3 + 5] =
		dev->dark_average_data[x * 2 * channels + 1];
	      shading_data[x * 2 * 2 * 3 + 8] =
		dev->dark_average_data[x * 2 * channels];
	      shading_data[x * 2 * 2 * 3 + 9] =
		dev->dark_average_data[x * 2 * channels + 1];
	    }

	  /* white data */
	  /* red channel */
	  val = 0;

	  val +=
	    256 * dev->white_average_data[(x - 2) * 2 * channels + 1] +
	    dev->white_average_data[(x - 2) * 2 * channels];
	  val -=
	    (256 * dev->dark_average_data[(x - 2) * 2 * channels + 1] +
	     dev->dark_average_data[(x - 2) * 2 * channels]);

	  val +=
	    256 * dev->white_average_data[(x - 1) * 2 * channels + 1] +
	    dev->white_average_data[(x - 1) * 2 * channels];
	  val -=
	    (256 * dev->dark_average_data[(x - 1) * 2 * channels + 1] +
	     dev->dark_average_data[(x - 1) * 2 * channels]);

	  val +=
	    256 * dev->white_average_data[(x) * 2 * channels + 1] +
	    dev->white_average_data[(x) * 2 * channels];
	  val -=
	    (256 * dev->dark_average_data[(x) * 2 * channels + 1] +
	     dev->dark_average_data[(x) * 2 * channels]);

	  val /= 3;
	  if ((65535 * val) / coeff > target_code)
	    val = (coeff * target_code) / val;
	  else
	    val = 65535;
	  shading_data[x * 2 * 2 * 3 + 2] = val & 0xff;
	  shading_data[x * 2 * 2 * 3 + 3] = val >> 8;
	  if (channels > 1)
	    {
	      /* green */
	      val = 0;
	      val +=
		256 * dev->white_average_data[(x - 2) * 2 * channels + 3] +
		dev->white_average_data[(x - 2) * 2 * channels + 2];
	      val -=
		(256 * dev->dark_average_data[(x - 2) * 2 * channels + 3] +
		 dev->dark_average_data[(x - 2) * 2 * channels + 2]);
	      val +=
		256 * dev->white_average_data[(x - 1) * 2 * channels + 3] +
		dev->white_average_data[(x - 1) * 2 * channels + 2];
	      val -=
		(256 * dev->dark_average_data[(x - 1) * 2 * channels + 3] +
		 dev->dark_average_data[(x - 1) * 2 * channels + 2]);
	      val +=
		256 * dev->white_average_data[(x) * 2 * channels + 3] +
		dev->white_average_data[(x) * 2 * channels + 2];
	      val -=
		(256 * dev->dark_average_data[(x) * 2 * channels + 3] +
		 dev->dark_average_data[(x) * 2 * channels + 2]);
	      val /= 3;
	      if ((65535 * val) / coeff > target_code)
		val = (coeff * target_code) / val;
	      else
		val = 65535;
	      shading_data[x * 2 * 2 * 3 + 6] = val & 0xff;
	      shading_data[x * 2 * 2 * 3 + 7] = val >> 8;

	      /* blue */
	      val = 0;
	      val +=
		256 * dev->white_average_data[(x) * 2 * channels + 5] +
		dev->white_average_data[(x) * 2 * channels + 4];
	      val -=
		(256 * dev->dark_average_data[(x) * 2 * channels + 5] +
		 dev->dark_average_data[(x) * 2 * channels + 4]);
	      val +=
		256 * dev->white_average_data[(x) * 2 * channels + 5] +
		dev->white_average_data[(x) * 2 * channels + 4];
	      val -=
		(256 * dev->dark_average_data[(x) * 2 * channels + 5] +
		 dev->dark_average_data[(x) * 2 * channels + 4]);
	      val +=
		256 * dev->white_average_data[(x) * 2 * channels + 5] +
		dev->white_average_data[(x) * 2 * channels + 4];
	      val -=
		(256 * dev->dark_average_data[(x) * 2 * channels + 5] +
		 dev->dark_average_data[(x) * 2 * channels + 4]);
	      val /= 3;
	      if ((65535 * val) / coeff > target_code)
		val = (coeff * target_code) / val;
	      else
		val = 65535;
	      shading_data[x * 2 * 2 * 3 + 10] = val & 0xff;
	      shading_data[x * 2 * 2 * 3 + 11] = val >> 8;
	    }
	  else
	    {
	      shading_data[x * 2 * 2 * 3 + 6] = val & 0xff;
	      shading_data[x * 2 * 2 * 3 + 7] = val >> 8;
	      shading_data[x * 2 * 2 * 3 + 10] = val & 0xff;
	      shading_data[x * 2 * 2 * 3 + 11] = val >> 8;
	    }
	}
      break;
    case CCD_HP2300:
    default:
      target_code = 0xFA00;
      for (x = 6; x < pixels_per_line; x++)
	{
	  /* dark data */
	  val = 0;
	  for (i = 0; i < 7; i++)
	    {
	      val += dev->dark_average_data[(x - i) * 2 * channels];
	      val += 256 * dev->dark_average_data[(x - i) * 2 * channels + 1];
	    }
	  val /= i;
	  shading_data[x * 2 * 2 * 3] = val & 0xff;
	  shading_data[x * 2 * 2 * 3 + 1] = val >> 8;

	  if (channels > 1)
	    {
	      val = 0;
	      for (i = 0; i < 7; i++)
		{
		  val += dev->dark_average_data[(x - i) * 2 * channels + 2];
		  val +=
		    256 * dev->dark_average_data[(x - i) * 2 * channels + 3];
		}
	      val /= i;
	      shading_data[x * 2 * 2 * 3 + 4] = val & 0xff;
	      shading_data[x * 2 * 2 * 3 + 5] = val >> 8;

	      val = 0;
	      for (i = 0; i < 7; i++)
		{
		  val += dev->dark_average_data[(x - i) * 2 * channels + 4];
		  val +=
		    256 * dev->dark_average_data[(x - i) * 2 * channels + 5];
		}
	      val /= i;
	      shading_data[x * 2 * 2 * 3 + 8] = val & 0xff;
	      shading_data[x * 2 * 2 * 3 + 9] = val >> 8;
	    }
	  else
	    {
	      shading_data[x * 2 * 2 * 3 + 4] = shading_data[x * 2 * 2 * 3];
	      shading_data[x * 2 * 2 * 3 + 5] =
		shading_data[x * 2 * 2 * 3 + 1];
	      shading_data[x * 2 * 2 * 3 + 8] = shading_data[x * 2 * 2 * 3];
	      shading_data[x * 2 * 2 * 3 + 9] =
		shading_data[x * 2 * 2 * 3 + 1];
	    }

	  /* white data */
	  val = 0;

	  /* average on 7 rows */
	  for (i = 0; i < 7; i++)
	    {
	      val +=
		256 * dev->white_average_data[(x - i) * 2 * channels + 1] +
		dev->white_average_data[(x - i) * 2 * channels];
	      val -=
		(256 * dev->dark_average_data[(x - i) * 2 * channels + 1] +
		 dev->dark_average_data[(x - i) * 2 * channels]);
	    }
	  val /= i;

	  if ((65535 * val) / coeff > target_code)
	    val = (coeff * target_code) / val;
	  else
	    val = 65535;
	  shading_data[x * 2 * 2 * 3 + 2] = val & 0xff;
	  shading_data[x * 2 * 2 * 3 + 3] = val >> 8;
	  if (channels > 1)
	    {
	      /* green */
	      /* average on 7 rows */
	      val = 0;
	      for (i = 0; i < 7; i++)
		{
		  val +=
		    256 * dev->white_average_data[(x - i) * 2 * channels +
						  3] +
		    dev->white_average_data[(x - i) * 2 * channels + 2];
		  val -=
		    (256 *
		     dev->dark_average_data[(x - i) * 2 * channels + 3] +
		     dev->dark_average_data[(x - i) * 2 * channels + 2]);
		}
	      val /= i;

	      if ((65535 * val) / coeff > target_code)
		val = (coeff * target_code) / val;
	      else
		val = 65535;
	      shading_data[x * 2 * 2 * 3 + 6] = val & 0xff;
	      shading_data[x * 2 * 2 * 3 + 7] = val >> 8;

	      /* blue */
	      /* average on 7 rows */
	      val = 0;
	      for (i = 0; i < 7; i++)
		{
		  val +=
		    256 * dev->white_average_data[(x - i) * 2 * channels +
						  5] +
		    dev->white_average_data[(x - i) * 2 * channels + 4];
		  val -=
		    (256 *
		     dev->dark_average_data[(x - i) * 2 * channels + 5] +
		     dev->dark_average_data[(x - i) * 2 * channels + 4]);
		}
	      val /= i;

	      if ((65535 * val) / coeff > target_code)
		val = (coeff * target_code) / val;
	      else
		val = 65535;
	      shading_data[x * 2 * 2 * 3 + 10] = val & 0xff;
	      shading_data[x * 2 * 2 * 3 + 11] = val >> 8;
	    }
	  else
	    {
	      shading_data[x * 2 * 2 * 3 + 6] = val & 0xff;
	      shading_data[x * 2 * 2 * 3 + 7] = val >> 8;
	      shading_data[x * 2 * 2 * 3 + 10] = val & 0xff;
	      shading_data[x * 2 * 2 * 3 + 11] = val >> 8;
	    }
	}
      break;
    }

  status =
    genesys_send_offset_and_shading (dev, shading_data,
				     pixels_per_line * 4 * 3);
  if (status != SANE_STATUS_GOOD)
    DBG (DBG_error,
	 "genesys_send_shading_coefficient: failed to send shading data: %s\n",
	 sane_strstatus (status));

  free (shading_data);
  DBG (DBG_proc, "genesys_send_shading_coefficient: completed\n");

  return SANE_STATUS_GOOD;
}


static SANE_Status
genesys_flatbed_calibration (Genesys_Device * dev)
{
  SANE_Status status;
  u_int16_t pixels_per_line;
  int yres;

  DBG (DBG_info, "genesys_flatbed_calibration\n");

  yres = dev->sensor.optical_res;
  if (dev->settings.yres <= dev->sensor.optical_res / 2)
    yres /= 2;

  /* send default gamma table */
  status = dev->model->cmd_set->send_gamma_table (dev, 1);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "genesys_flatbed_calibration: failed to init gamma table: %s\n",
	   sane_strstatus (status));
      return status;
    }

  /* do offset calibration if needed */
  if (dev->model->flags & GENESYS_FLAG_OFFSET_CALIBRATION)
    {
      status = dev->model->cmd_set->offset_calibration (dev);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "genesys_flatbed_calibration: offset calibration failed: %s\n",
	       sane_strstatus (status));
	  return status;
	}

      /* since all the registers are set up correctly, just use them */
      status = dev->model->cmd_set->coarse_gain_calibration (dev, yres);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "genesys_flatbed_calibration: coarse gain calibration: %s\n",
	       sane_strstatus (status));
	  return status;
	}
    }

  pixels_per_line = dev->model->y_size * dev->settings.xres;

  /* send default shading data */
  status = sanei_genesys_init_shading_data (dev, pixels_per_line);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "genesys_flatbed_calibration: failed to init shading process: %s\n",
	   sane_strstatus (status));
      return status;
    }

  /* since we have 2 gain calibration proc, skip second if first one was
     used. */
  if (!dev->model->flags & GENESYS_FLAG_OFFSET_CALIBRATION)
    {
      status = dev->model->cmd_set->init_regs_for_coarse_calibration (dev);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "genesys_flatbed_calibration: failed to send calibration registers: %s\n",
	       sane_strstatus (status));
	  return status;
	}

      status = genesys_coarse_calibration (dev);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "genesys_flatbed_calibration: failed to do static calibration: %s\n",
	       sane_strstatus (status));
	  return status;
	}
    }


  /* shading calibration */
  status = dev->model->cmd_set->init_regs_for_shading (dev);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "genesys_flatbed_calibration: failed to send shading "
	   "registers: %s\n", sane_strstatus (status));
      return status;
    }

  if (dev->model->flags & GENESYS_FLAG_DARK_CALIBRATION)
    {
      status = genesys_dark_shading_calibration (dev);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "genesys_flatbed_calibration: failed to do dark shading calibration: %s\n",
	       sane_strstatus (status));
	  return status;
	}
    }

  status = genesys_white_shading_calibration (dev);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "genesys_flatbed_calibration: failed to do white shading calibration: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status = genesys_send_shading_coefficient (dev);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "genesys_flatbed_calibration: failed to send shading calibration coefficients: %s\n",
	   sane_strstatus (status));
      return status;
    }

  /* send gamma tables if needed */
  status = dev->model->cmd_set->send_gamma_table (dev, 0);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "genesys_flatbed_calibration: failed to send specific gamma tables: %s\n",
	   sane_strstatus (status));
      return status;
    }

  DBG (DBG_info, "genesys_flatbed_calibration: completed\n");

  return SANE_STATUS_GOOD;
}

/* unused function kept in case it may be usefull in the futur */
#if 0
static SANE_Status
genesys_wait_not_moving (Genesys_Device * dev, int mseconds)
{
  u_int8_t value;
  SANE_Status status;

  DBG (DBG_proc,
       "genesys_wait_not_moving: waiting %d mseconds for motor to stop\n",
       mseconds);
  while (mseconds > 0)
    {
      RIE (sanei_genesys_get_status (dev, &value));

      if (dev->model->cmd_set->test_motor_flag_bit (value))
	{
	  usleep (100 * 1000);
	  mseconds -= 100;
	  DBG (DBG_io,
	       "genesys_wait_not_moving: motor is moving, %d mseconds to go\n",
	       mseconds);
	}
      else
	{
	  DBG (DBG_info,
	       "genesys_wait_not_moving: motor is not moving, exiting\n");
	  return SANE_STATUS_GOOD;
	}

    }
  DBG (DBG_error,
       "genesys_wait_not_moving: motor is still moving, timeout exceeded\n");
  return SANE_STATUS_DEVICE_BUSY;
}
#endif

/* ------------------------------------------------------------------------ */
/*                  High level (exported) functions                         */
/* ------------------------------------------------------------------------ */

/*
 * wait lamp to be warm enough by scanning the same line until
 * differences between two scans are below a threshold
 */
static SANE_Status
genesys_warmup_lamp (Genesys_Device * dev)
{
  Genesys_Register_Set local_reg[GENESYS_MAX_REGS];
  u_int8_t *first_line, *second_line;
  int seconds = 0;
  int pixel;
  int channels, total_size;
  double first_average = 0;
  double second_average = 0;
  int difference = 255;
  int empty;
  SANE_Status status = SANE_STATUS_IO_ERROR;

  DBG (DBG_proc, "genesys_warmup_lamp: start\n");

  dev->model->cmd_set->init_regs_for_warmup (dev, local_reg, &channels,
					     &total_size);
  first_line = malloc (total_size);
  if (!first_line)
    return SANE_STATUS_NO_MEM;

  second_line = malloc (total_size);
  if (!second_line)
    return SANE_STATUS_NO_MEM;

  do
    {

      DBG (DBG_info, "genesys_warmup_lamp: one more loop\n");
      RIE (dev->model->cmd_set->begin_scan (dev, local_reg, SANE_TRUE));
      do
	{
	  sanei_genesys_test_buffer_empty (dev, &empty);
	}
      while (empty);
      RIE (sanei_genesys_read_data_from_scanner
	   (dev, first_line, total_size));
      RIE (dev->model->cmd_set->end_scan (dev, local_reg, SANE_FALSE));

      sleep (1);		/* sleep 1 s */
      seconds++;

      RIE (dev->model->cmd_set->begin_scan (dev, local_reg, SANE_FALSE));
      do
	{
	  sanei_genesys_test_buffer_empty (dev, &empty);
	}
      while (empty);
      RIE (sanei_genesys_read_data_from_scanner
	   (dev, second_line, total_size));
      RIE (dev->model->cmd_set->end_scan (dev, local_reg, SANE_FALSE));

      sleep (1);		/* sleep 1 s */
      seconds++;

      for (pixel = 0; pixel < total_size; pixel++)
	{
	  if (dev->model->cmd_set->get_bitset_bit (local_reg))
	    {
	      first_average +=
		(first_line[pixel] + first_line[pixel + 1] * 256);
	      second_average +=
		(second_line[pixel] + second_line[pixel + 1] * 256);
	      pixel++;
	    }
	  else
	    {
	      first_average += first_line[pixel];
	      second_average += second_line[pixel];
	    }
	}
      if (dev->model->cmd_set->get_bitset_bit (local_reg))
	{
	  DBG (DBG_info,
	       "genesys_warmup_lamp: average = %.2f %%, diff = %.3f %%\n",
	       100 * ((second_average) / (256 * 256)),
	       100 * (difference / second_average));
	  first_average /= pixel;
	  second_average /= pixel;
	  difference = abs (first_average - second_average);

	  if (second_average > (110 * 256)
	      && (difference / second_average) < 0.002)
	    break;
	}
      else
	{
	  first_average /= pixel;
	  second_average /= pixel;
	  if (DBG_LEVEL >= DBG_data)
	    {
	      sanei_genesys_write_pnm_file ("warmup1.pnm", first_line, 8,
					    channels, total_size / 2, 2);
	      sanei_genesys_write_pnm_file ("warmup2.pnm", second_line, 8,
					    channels, total_size / 2, 2);
	    }
	  DBG (DBG_info,
	       "genesys_warmup_lamp: average 1 = %.2f %%, average 2 = %.2f %%\n",
	       first_average, second_average);
	  if (abs (first_average - second_average) < 15
	      && second_average > 120)
	    break;
	}
    }
  while (seconds < WARMUP_TIME);

  if (seconds >= WARMUP_TIME)
    {
      DBG (DBG_error,
	   "genesys_warmup_lamp: warmup timed out after %d seconds. Lamp defective?\n",
	   seconds);
      status = SANE_STATUS_IO_ERROR;
    }
  else
    {
      DBG (DBG_info,
	   "genesys_warmup_lamp: warmup succeeded after %d seconds\n",
	   seconds);
    }

  free (first_line);
  free (second_line);
  return status;
}


/* High-level start of scanning */
static SANE_Status
genesys_start_scan (Genesys_Device * dev)
{
  SANE_Status status;
  int steps, expected;

  DBG (DBG_proc, "genesys_start_scan\n");

  /* wait for lamp warmup */
  if (!(dev->model->flags & GENESYS_FLAG_SKIP_WARMUP))
    {
      RIE (genesys_warmup_lamp (dev));
    }

  /* set top left x and y values */
  if ((dev->model->flags & GENESYS_FLAG_SEARCH_START)
      && (dev->model->y_offset_calib == 0))
    {
      status = dev->model->cmd_set->search_start_position (dev);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "genesys_start_scan: failed to search start position: %s\n",
	       sane_strstatus (status));
	  return status;
	}

      if (dev->model->flags & GENESYS_FLAG_USE_PARK)
	status = dev->model->cmd_set->park_head (dev, dev->reg, 1);
      else
	status = dev->model->cmd_set->slow_back_home (dev, 1);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "genesys_start_scan: failed to move scanhead to "
	       "home position: %s\n", sane_strstatus (status));
	  return status;
	}
      dev->scanhead_position_in_steps = 0;
    }
  else
    {
      /* Go home */
      /* too: check we can drop this since we cannot have the
         scanner's head wandering here */
      if (dev->model->flags & GENESYS_FLAG_USE_PARK)
	status = dev->model->cmd_set->park_head (dev, dev->reg, 1);
      else
	status = dev->model->cmd_set->slow_back_home (dev, 1);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "genesys_start_scan: failed to move scanhead to "
	       "home position: %s\n", sane_strstatus (status));
	  return status;
	}
      dev->scanhead_position_in_steps = 0;
    }

  /* calibration */
  status = genesys_flatbed_calibration (dev);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "genesys_start_scan: failed to do flatbed calibration: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status = dev->model->cmd_set->init_regs_for_scan (dev);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "genesys_start_scan: failed to do init registers for scan: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status =
    dev->model->cmd_set->send_slope_table (dev, 0, dev->slope_table0,
					   sanei_genesys_read_reg_from_set
					   (dev->reg, 0x21));
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "genesys_start_scan: failed to send slope table 0: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status =
    dev->model->cmd_set->send_slope_table (dev, 1, dev->slope_table1,
					   sanei_genesys_read_reg_from_set
					   (dev->reg, 0x6b));
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "genesys_start_scan: failed to send slope table 1: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status =
    sanei_genesys_bulk_write_register (dev, dev->reg,
				       dev->model->cmd_set->
				       bulk_full_size () * 2);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "genesys_start_scan: Failed to bulk write registers, status = %d\n",
	   status);
      return status;
    }

  status = sanei_genesys_start_motor (dev);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "genesys_start_scan: failed to start motor: %s\n",
	   sane_strstatus (status));
      return SANE_STATUS_IO_ERROR;
    }

  /* waits for head to reach scanning position */
  expected =
    sanei_genesys_read_reg_from_set (dev->reg,
				     0x3d) * 65536 +
    sanei_genesys_read_reg_from_set (dev->reg,
				     0x3e) * 256 +
    sanei_genesys_read_reg_from_set (dev->reg, 0x3f);
  do
    {
      /* wait 1/10th of second between each test to avoid
         overloading USB and CPU */
      usleep (100 * 1000);
      status = sanei_genesys_read_feed_steps (dev, &steps);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "genesys_start_scan: Failed to read feed steps: %s\n",
	       sane_strstatus (status));
	  return status;
	}
    }
  while (steps < expected);

  /* when doing one or two-table movement, let the motor settle to scanning speed */
  /* and scanning start before reading data                                        */
  if (dev->model->cmd_set->get_fast_feed_bit (dev->reg))
    usleep (1000 * 1000);
  else
    usleep (500 * 1000);

  /* then we wait for at least one word of valid scan data */
  do
    {
      usleep (100 * 1000);
      status = genesys_read_valid_words (dev, &steps);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "genesys_start_scan: Failed to read valid words: %s\n",
	       sane_strstatus (status));
	  return status;
	}
    }
  while (steps < 1);

  DBG (DBG_proc, "genesys_start_scan: completed\n");
  return SANE_STATUS_GOOD;
}

/* the function shrinks the data lines by the given factor
   height remain unaffected 
   factor below 1 and higher than 2 haven't been tested yet */
static SANE_Status
genesys_shrink_lines (SANE_Byte * source, SANE_Byte * dest, int src_width,
		      int dst_width, int lines, int channels, int depth)
{
  int i, j, k, f, l;
  float first, last;
  float sum, sum1, sum2, sum3;
  float factor;

  factor = ((float) src_width) / ((float) dst_width);
  for (j = 0; j < lines; j++)
    {
      for (i = 0; i < dst_width; i++)
	{
	  first = i * factor;
	  f = (int) (first);
	  last = (i + 1) * factor;
	  l = (int) (last);
	  switch (channels * depth)
	    {
	    case 8:		/* 8 bits gray scan */
	      if (f == l)
		{
		  sum = source[f + j * src_width] * factor;
		}
	      else
		{
		  sum = source[f + j * src_width] * (1 - first + f);
		  sum += source[l + j * src_width] * (last - l);
		  for (k = f + 1; k < l; k++)
		    sum += source[k + j * src_width];
		}
	      dest[i + j * dst_width] = (unsigned char) (sum / factor + 0.5);
	      break;
	    case 16:		/* 16 bits gray level scan */
	      if (f == l)
		{
		  sum =
		    source[2 * (f + j * src_width)] +
		    (256 * source[2 * (f + j * src_width) + 1]);
		  sum *= factor;
		}
	      else
		{
		  sum =
		    source[2 * (f + j * src_width)] +
		    256 * source[2 * (f + j * src_width) + 1];
		  sum *= (1 - first + f);
		  sum +=
		    (source[2 * (l + j * src_width)] +
		     256 * source[2 * (l + j * src_width) + 1]) * (last - l);
		  for (k = f + 1; k < l; k++)
		    sum +=
		      source[2 * (k + j * src_width)] +
		      256 * source[2 * (k + j * src_width) + 1];
		}
	      sum = sum / factor + 0.5;
	      dest[2 * (i + j * dst_width)] =
		(unsigned char) ((int) (sum) & 0xFF);
	      dest[2 * (i + j * dst_width) + 1] = (unsigned char) (sum / 256);
	      break;
	    case 24:		/* 8 bits color scan */
	      if (f == l)
		{
		  sum1 = source[3 * (f + j * src_width)] * factor;
		  sum2 = source[3 * (f + j * src_width) + 1] * factor;
		  sum3 = source[3 * (f + j * src_width) + 2] * factor;
		}
	      else
		{
		  sum1 = source[3 * (f + j * src_width)] * (1 - first + f);
		  sum1 += source[3 * (l + j * src_width)] * (last - l);
		  sum2 =
		    source[3 * (f + j * src_width) + 1] * (1 - first + f);
		  sum2 += source[3 * (l + j * src_width) + 1] * (last - l);
		  sum3 =
		    source[3 * (f + j * src_width) + 2] * (1 - first + f);
		  sum3 += source[3 * (l + j * src_width) + 2] * (last - l);
		  for (k = f + 1; k < l; k++)
		    {
		      sum1 += source[3 * (k + j * src_width)];
		      sum2 += source[3 * (k + j * src_width) + 1];
		      sum3 += source[3 * (k + j * src_width) + 2];
		    }
		}
	      dest[(i + j * dst_width) * 3] =
		(unsigned char) (sum1 / factor + 0.5);
	      dest[(i + j * dst_width) * 3 + 1] =
		(unsigned char) (sum2 / factor + 0.5);
	      dest[(i + j * dst_width) * 3 + 2] =
		(unsigned char) (sum3 / factor + 0.5);
	      break;
	    case 48:		/* 16 bits color scan */
	      if (f == l)
		{
		  sum1 =
		    source[6 * (f + j * src_width)] +
		    (256 * source[6 * (f + j * src_width) + 1]);
		  sum1 *= factor;
		  sum2 =
		    source[6 * (f + j * src_width) + 2] +
		    (256 * source[6 * (f + j * src_width) + 3]);
		  sum2 *= factor;
		  sum3 =
		    source[6 * (f + j * src_width) + 4] +
		    (256 * source[6 * (f + j * src_width) + 5]);
		  sum3 *= factor;
		}
	      else
		{
		  sum1 =
		    source[6 * (f + j * src_width)] +
		    256 * source[6 * (f + j * src_width) + 1];
		  sum1 *= (1 - first + f);
		  sum2 =
		    source[6 * (f + j * src_width) + 2] +
		    256 * source[6 * (f + j * src_width) + 3];
		  sum2 *= (1 - first + f);
		  sum3 =
		    source[6 * (f + j * src_width) + 4] +
		    256 * source[6 * (f + j * src_width) + 5];
		  sum3 *= (1 - first + f);

		  sum1 +=
		    (source[6 * (l + j * src_width)] +
		     256 * source[6 * (l + j * src_width) + 1]) * (last - l);
		  sum2 +=
		    (source[6 * (l + j * src_width) + 2] +
		     256 * source[6 * (l + j * src_width) + 3]) * (last - l);
		  sum3 +=
		    (source[6 * (l + j * src_width) + 4] +
		     256 * source[6 * (l + j * src_width) + 5]) * (last - l);
		  for (k = f + 1; k < l; k++)
		    {
		      sum1 +=
			source[6 * (k + j * src_width)] +
			256 * source[6 * (k + j * src_width) + 1];
		      sum2 +=
			source[6 * (k + j * src_width) + 2] +
			256 * source[6 * (k + j * src_width) + 3];
		      sum3 +=
			source[6 * (k + j * src_width) + 4] +
			256 * source[6 * (k + j * src_width) + 5];
		    }
		}
	      sum1 = sum1 / factor + 0.5;
	      sum2 = sum2 / factor + 0.5;
	      sum3 = sum3 / factor + 0.5;
	      dest[6 * (i + j * dst_width)] =
		(unsigned char) ((int) sum1 & 0xFF);
	      dest[6 * (i + j * dst_width) + 1] =
		(unsigned char) (sum1 / 256);
	      dest[6 * (i + j * dst_width) + 2] =
		(unsigned char) ((int) sum2 & 0xFF);
	      dest[6 * (i + j * dst_width) + 3] =
		(unsigned char) (sum2 / 256);
	      dest[6 * (i + j * dst_width) + 4] =
		(unsigned char) ((int) sum3 & 0xFF);
	      dest[6 * (i + j * dst_width) + 5] =
		(unsigned char) (sum3 / 256);
	      break;
	    case 1:		/* hardware lineart */
	      return SANE_STATUS_IO_ERROR;
	    }
	}
    }
  return SANE_STATUS_GOOD;
}

/* this function does the effective data read in a manner that suits 
   the scanner. It does data reordering and resizing if need.  
   It also manages EOF and I/O errors, and line distance correction.
   todo : try to make is more easy to read and maintain, maybe split it
   */

static SANE_Status
genesys_read_ordered_data (Genesys_Device * dev, SANE_Byte * destination,
			   size_t * len)
{
  SANE_Status status;
  size_t bytes, offset;
  size_t fixup, stagger_value;
  size_t work_size, end;
  size_t size, n;
  int channels, depth, pixels;
#ifdef SANE_DEBUG_LOG_RAW_DATA
  static FILE *f = NULL;
#endif
  int ldc_shift[3];
  u_int8_t *work_buffer;
  SANE_Byte high, flag;

  DBG (DBG_proc, "genesys_read_ordered_data\n");
  if (dev->read_active != SANE_TRUE)
    {
      DBG (DBG_error, "genesys_read_ordered_data: read not active!\n");
      *len = 0;
      return SANE_STATUS_INVAL;
    }

  /* current settings */
  if (dev->model->cmd_set->get_filter_bit (dev->reg))
    channels = 1;
  else
    channels = 3;
  if (dev->model->cmd_set->get_bitset_bit (dev->reg))
    depth = 16;
  else
    depth = 8;
  if (dev->model->cmd_set->get_lineart_bit (dev->reg))
    depth = 1;

  /* pixel end - pixel start / words */
  /* pixels = 256 * sanei_genesys_read_reg_from_set (dev->reg, 0x32) 
     +       sanei_genesys_read_reg_from_set (dev->reg, 0x33)
     - 256 * sanei_genesys_read_reg_from_set (dev->reg, 0x30)
     -       sanei_genesys_read_reg_from_set (dev->reg, 0x31); */
  if (dev->read_factor > 0)
    pixels = (dev->words_per_line * 8) / (channels * depth);
  else
    pixels = dev->settings.pixels;

  DBG (DBG_info,
       "genesys_read_ordered_data: bytes_to_read=%lu, total_bytes_read=%lu\n",
       (u_long) dev->bytes_to_read, (u_long) dev->total_bytes_read);
  /* is there data left to scan */
  if (dev->total_bytes_read >= dev->bytes_to_read)
    {
      DBG (DBG_proc,
	   "genesys_read_ordered_data: nothing more to scan: EOF\n");
      *len = 0;
      return SANE_STATUS_EOF;
    }

  /* don't allow reading too much data: physical data left first */
  if (*len + dev->total_bytes_read > dev->bytes_to_read)
    *len = dev->bytes_to_read - dev->total_bytes_read;

  /* we hold dev->requested_buffer_size usable bytes in buffer maximum,
     so don't read more */
  if (*len > dev->requested_buffer_size)
    *len = dev->requested_buffer_size;

  /* computes offset needed to correct staggering effect */
  stagger_value =
    (dev->stagger * dev->settings.pixels * channels * depth) / 8;

  /* offset is against allready shrunk lines */
  offset =
    ((dev->max_shift +
      dev->stagger) * dev->settings.pixels * channels * depth) / 8;
  DBG (DBG_io, "genesys_read_ordered_data: offset is %lu bytes\n",
       (u_long) offset);
  ldc_shift[0] =
    (((dev->model->ld_shift_r * dev->settings.yres) / dev->motor.base_ydpi) *
     dev->settings.pixels * channels * depth) / 8;
  ldc_shift[1] =
    (((dev->model->ld_shift_g * dev->settings.yres) / dev->motor.base_ydpi) *
     dev->settings.pixels * channels * depth) / 8;
  ldc_shift[2] =
    (((dev->model->ld_shift_b * dev->settings.yres) / dev->motor.base_ydpi) *
     dev->settings.pixels * channels * depth) / 8;

  /* do we have to read data from scanner ? */
  if (dev->read_bytes_in_buffer < *len + offset)
    {
      /* move data to start of buffer if there isn't enough room */
      end = dev->read_pos + dev->read_bytes_in_buffer;
      if (end > dev->requested_buffer_size + offset)
	{
	  memcpy (dev->read_buffer, dev->read_buffer + dev->read_pos,
		  dev->read_bytes_in_buffer);
	  end = dev->read_bytes_in_buffer;
	  dev->read_pos = 0;
	}

      /* actual read */
      if (dev->requested_buffer_size > dev->read_bytes_left)
	size = dev->read_bytes_left;
      else
	size = dev->requested_buffer_size;
      if ((dev->read_factor > 0.0))
	{
	  n = size / dev->words_per_line;
	  work_size = (channels * depth * dev->settings.pixels * n) / 8;
	}
      else
	work_size = size;
      while ((dev->read_bytes_in_buffer < *len + offset)
	     && (dev->read_bytes_left > 0)
	     && (dev->read_bytes_in_buffer <=
		 dev->read_buffer_size - work_size))
	{
	  DBG (DBG_error, "genesys_read_ordered_data: reading %lu bytes\n",
	       (u_long) size);
	  if ((dev->read_factor > 0.0))
	    work_buffer = (u_int8_t *) (dev->resize_buffer);
	  else
	    work_buffer = (u_int8_t *) (dev->read_buffer + end);
	  status =
	    sanei_genesys_bulk_read_data (dev, 0x45, work_buffer, size);
	  if (status != SANE_STATUS_GOOD)
	    {
	      DBG (DBG_error,
		   "genesys_read_ordered_data: failed to read %lu bytes (%s)\n",
		   (u_long) size, sane_strstatus (status));
	      return SANE_STATUS_IO_ERROR;
	    }

#ifdef SANE_DEBUG_LOG_RAW_DATA
	  if (f == NULL)
	    {
	      f = fopen ("raw.pnm", "wb");
	      if (f != NULL)
		{
		  fprintf (f,
			   "P%c\n%d %d\n%d\n",
			   channels == 1 ? '5' : '6',
			   pixels,
			   dev->read_bytes_left / dev->words_per_line,
			   (1 << depth) - 1);
		}
	    }
	  else
	    {
	      fwrite (work_buffer, size, 1, f);
	    }
#endif

	  dev->read_bytes_left -= size;
	  /* when x dpy isn't in sync with ydpi, we have to shrink lines. We do
	     it by read_factor, relying on the fact we allways read a round number
	     of lines in read buffer */
	  if (dev->read_factor > 0.0)
	    {
	      /* lines read */
	      n = size / dev->words_per_line;
	      status = genesys_shrink_lines (dev->resize_buffer,
					     dev->read_buffer + end,
					     pixels,
					     dev->settings.pixels,
					     n, channels, depth);
	      /* shrunken size */
	      size = (channels * depth * dev->settings.pixels * n) / 8;
	    }

	  /* emulated lineart handling : shrink data into bytes here */
	  if (dev->settings.scan_mode == 0)
	    {
	      high = sanei_genesys_read_reg_from_set (dev->reg, 0x2e);
	      for (n = 0; n < size; n++)
		{
		  if (dev->read_buffer[end + n] >= high)
		    flag = 0;
		  else
		    flag = 128;
		  if (n % 8 == 0)
		    dev->read_buffer[end + n / 8] = flag;
		  else
		    dev->read_buffer[end + n / 8] |= flag >> (n % 8);
		}
	      size /= 8;
	    }

	  dev->read_bytes_in_buffer += size;
	  end += size;

	  /* computes next needed next size */
	  if (dev->requested_buffer_size > dev->read_bytes_left)
	    size = dev->read_bytes_left;
	  else
	    size = dev->requested_buffer_size;
	  if ((dev->read_factor > 0.0))
	    {
	      n = size / dev->words_per_line;
	      work_size = (channels * depth * dev->settings.pixels * n) / 8;
	    }
	  else
	    work_size = size;
	}			/* end read loop */
    }

  /* copy data according to line distance correction, color order and
     'stagger' value if any */
  DBG (DBG_proc, "genesys_read_ordered_data: color_order=%d, channels=%d\n",
       dev->model->line_mode_color_order, channels);
  switch (channels * depth)
    {
    case 1:
    case 8:
      if (dev->stagger > 0)
	{
	  bytes = 0;
	  while ((bytes < *len) && (dev->read_bytes_in_buffer >= offset))
	    {
	      fixup = (dev->total_bytes_read + bytes) % dev->settings.pixels;
	      fixup = ((fixup % 2)) * stagger_value;
	      destination[bytes] = dev->read_buffer[dev->read_pos + fixup];
	      bytes++;
	      dev->read_pos++;
	      dev->read_bytes_in_buffer--;
	    }
	}
      else
	{
	  if (*len > dev->read_bytes_in_buffer)
	    bytes = dev->read_bytes_in_buffer;
	  else
	    bytes = *len;
	  memcpy (destination, dev->read_buffer + dev->read_pos, bytes);
	  dev->read_pos += bytes;
	  dev->read_bytes_in_buffer -= bytes;
	}
      break;
    case 16:
      if (dev->stagger > 0)
	{
	  bytes = 0;
	  while ((bytes < *len) && (dev->read_bytes_in_buffer >= offset))
	    {
	      fixup =
		((dev->total_bytes_read + bytes) / 2) % dev->settings.pixels;
	      fixup = ((fixup % 2)) * stagger_value;
	      destination[bytes] = dev->read_buffer[dev->read_pos + fixup];
	      bytes++;
	      dev->read_pos++;
	      dev->read_bytes_in_buffer--;
	    }
	}
      else
	{
	  if (*len > dev->read_bytes_in_buffer)
	    bytes = dev->read_bytes_in_buffer;
	  else
	    bytes = *len;
	  memcpy (destination, dev->read_buffer + dev->read_pos, bytes);
	  dev->read_pos += bytes;
	  dev->read_bytes_in_buffer -= bytes;
	}
      break;
    case 24:
      bytes = 0;
      while ((bytes < *len) && (dev->read_bytes_in_buffer >= offset))
	{
	  if ((dev->model->line_mode_color_order == COLOR_ORDER_RGB)
	      && (dev->stagger > 0))
	    {
	      fixup =
		((dev->total_bytes_read + bytes) / 3) % dev->settings.pixels;
	      fixup = ((fixup % 2)) * stagger_value;
	      destination[bytes] =
		dev->read_buffer[dev->read_pos + fixup +
				 ldc_shift[(bytes +
					    dev->total_bytes_read) % 3]];
	    }
	  else if (dev->model->line_mode_color_order == COLOR_ORDER_RGB)
	    destination[bytes] =
	      dev->read_buffer[dev->read_pos +
			       ldc_shift[(bytes +
					  dev->total_bytes_read) % 3]];
	  else
	    destination[bytes] =
	      dev->read_buffer[dev->read_pos +
			       ldc_shift[2 -
					 (bytes +
					  dev->total_bytes_read) % 3]];
	  bytes++;
	  dev->read_pos++;
	  dev->read_bytes_in_buffer--;
	}
      break;
    case 48:
      bytes = 0;
      if (little_endian)
	{
	  while ((bytes < *len) && (dev->read_bytes_in_buffer >= offset))
	    {
	      if ((dev->model->line_mode_color_order == COLOR_ORDER_RGB)
		  && (dev->stagger > 0))
		{
		  fixup =
		    ((dev->total_bytes_read +
		      bytes) / 6) % dev->settings.pixels;
		  fixup = ((fixup % 2)) * stagger_value;
		  destination[bytes] =
		    dev->read_buffer[dev->read_pos + fixup +
				     ldc_shift[((bytes +
						 dev->total_bytes_read) / 2) %
					       3]];
		}
	      else if (dev->model->line_mode_color_order == COLOR_ORDER_RGB)
		destination[bytes] =
		  dev->read_buffer[dev->read_pos +
				   ldc_shift[((bytes +
					       dev->total_bytes_read) / 2) %
					     3]];
	      else
		destination[bytes] =
		  dev->read_buffer[dev->read_pos +
				   ldc_shift[2 -
					     ((bytes +
					       dev->total_bytes_read) / 2 %
					      3)]];
	      bytes++;
	      dev->read_pos++;
	      dev->read_bytes_in_buffer--;
	    }
	}
      else
	{
	  while ((bytes < *len) && (dev->read_bytes_in_buffer >= offset))
	    {
	      fixup = 1 - (dev->read_pos & 0x01);
	      if ((dev->model->line_mode_color_order == COLOR_ORDER_RGB)
		  && (dev->stagger > 0))
		{
		  fixup =
		    ((dev->total_bytes_read +
		      bytes) / 6) % dev->settings.pixels;
		  fixup =
		    ((fixup % 2)) * stagger_value + 1 -
		    (dev->read_pos & 0x01);
		  destination[bytes] =
		    dev->read_buffer[dev->read_pos + fixup +
				     ldc_shift[((bytes +
						 dev->total_bytes_read) / 2) %
					       3]];
		}
	      else if (dev->model->line_mode_color_order == COLOR_ORDER_RGB)
		destination[bytes] =
		  dev->read_buffer[dev->read_pos + fixup +
				   ldc_shift[((bytes +
					       dev->total_bytes_read) / 2) %
					     3]];
	      else
		destination[bytes] =
		  dev->read_buffer[dev->read_pos + fixup +
				   ldc_shift[2 -
					     ((bytes +
					       dev->total_bytes_read) / 2 %
					      3)]];
	      bytes++;
	      dev->read_pos++;
	      dev->read_bytes_in_buffer--;
	    }
	}
      break;
    default:
      DBG (DBG_error, "genesys_read_ordered_data: unimplemented copy\n");
      return SANE_STATUS_IO_ERROR;
    }
  *len = bytes;
  /* test for emulated lineart */
  if (dev->settings.scan_mode == 0)
    dev->total_bytes_read += 8 * bytes;
  else
    dev->total_bytes_read += bytes;

  DBG (DBG_proc, "genesys_read_ordered_data: completed, %lu bytes read\n",
       (u_long) bytes);
  return SANE_STATUS_GOOD;
}


/* ------------------------------------------------------------------------ */
/*                  Start of higher level functions                         */
/* ------------------------------------------------------------------------ */

/* Test if this machine is little endian (from coolscan.c) */
static SANE_Bool
calc_little_endian (void)
{
  SANE_Int testvalue = 255;
  u_int8_t *firstbyte = (u_int8_t *) & testvalue;

  if (*firstbyte == 255)
    return SANE_TRUE;
  return SANE_FALSE;
}

static size_t
max_string_size (const SANE_String_Const strings[])
{
  size_t size, max_size = 0;
  SANE_Int i;

  for (i = 0; strings[i]; ++i)
    {
      size = strlen (strings[i]) + 1;
      if (size > max_size)
	max_size = size;
    }
  return max_size;
}

static SANE_Status
calc_parameters (Genesys_Scanner * s)
{
  SANE_String mode, source;
  SANE_Status status = SANE_STATUS_GOOD;
  SANE_Int depth = 0, resolution = 0;
  double tl_x = 0, tl_y = 0, br_x = 0, br_y = 0;

  mode = s->val[OPT_MODE].s;
  source = s->val[OPT_SOURCE].s;
  depth = s->val[OPT_BIT_DEPTH].w;
  resolution = s->val[OPT_RESOLUTION].w;
  tl_x = SANE_UNFIX (s->val[OPT_TL_X].w);
  tl_y = SANE_UNFIX (s->val[OPT_TL_Y].w);
  br_x = SANE_UNFIX (s->val[OPT_BR_X].w);
  br_y = SANE_UNFIX (s->val[OPT_BR_Y].w);

  s->params.last_frame = SANE_TRUE;	/* only single pass scanning supported */

  if (strcmp (mode, "Gray") == 0 || strcmp (mode, "Lineart") == 0)
    s->params.format = SANE_FRAME_GRAY;
  else				/* Color */
    s->params.format = SANE_FRAME_RGB;

  if (strcmp (mode, "Lineart") == 0)
    s->params.depth = 1;
  else
    s->params.depth = depth;
  s->dev->settings.depth = depth;


  /* Hardware settings */
  if (resolution > s->dev->sensor.optical_res)
    s->dev->settings.xres = s->dev->sensor.optical_res;
  else
    s->dev->settings.xres = resolution;
  s->dev->settings.yres = resolution;


  s->params.lines = ((br_y - tl_y) * s->dev->settings.yres) / MM_PER_INCH;
  s->params.pixels_per_line =
    ((br_x - tl_x) * s->dev->settings.xres) / MM_PER_INCH;

  s->params.bytes_per_line = s->params.pixels_per_line;
  if (s->params.depth > 8)
    {
      s->params.depth = 16;
      s->params.bytes_per_line *= 2;
    }
  else if (s->params.depth == 1)
    {
      s->params.bytes_per_line /= 8;
      /* round down pixel number */
      s->params.pixels_per_line = 8 * s->params.bytes_per_line;
    }

  if (s->params.format == SANE_FRAME_RGB)
    s->params.bytes_per_line *= 3;

  if (strcmp (mode, "Color") == 0)
    s->dev->settings.scan_mode = 4;
  else if (strcmp (mode, "Gray") == 0)
    s->dev->settings.scan_mode = 2;
  else if (strcmp (mode, "Halftone") == 0)
    s->dev->settings.scan_mode = 1;
  else				/* Lineart */
    s->dev->settings.scan_mode = 0;

  /* todo: change and check */
  if (strcmp (source, "Flatbed") == 0)
    s->dev->settings.scan_method = 0;
  else				/* transparency */
    s->dev->settings.scan_method = 2;

  s->dev->settings.lines = s->params.lines;
  s->dev->settings.pixels = s->params.pixels_per_line;
  s->dev->settings.tl_x = tl_x;
  s->dev->settings.tl_y = tl_y;

  return status;
}


static SANE_Status
create_bpp_list (Genesys_Scanner * s, SANE_Int * bpp)
{
  int count;

  for (count = 0; bpp[count] != 0; count++)
    ;
  s->bpp_list[0] = count;
  for (count = 0; bpp[count] != 0; count++)
    {
      s->bpp_list[s->bpp_list[0] - count] = bpp[count];
    }
  return SANE_STATUS_GOOD;
}

static SANE_Status
init_options (Genesys_Scanner * s)
{
  SANE_Int option, count;
  SANE_Status status;
  SANE_Word *dpi_list;
  Genesys_Model *model = s->dev->model;
  SANE_Bool has_ta;

  DBG (DBG_proc, "init_options: start\n");

  /* no transparency adaptor support yet */
  has_ta = SANE_FALSE;

  memset (s->opt, 0, sizeof (s->opt));
  memset (s->val, 0, sizeof (s->val));

  for (option = 0; option < NUM_OPTIONS; ++option)
    {
      s->opt[option].size = sizeof (SANE_Word);
      s->opt[option].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    }
  s->opt[OPT_NUM_OPTS].name = SANE_NAME_NUM_OPTIONS;
  s->opt[OPT_NUM_OPTS].title = SANE_TITLE_NUM_OPTIONS;
  s->opt[OPT_NUM_OPTS].desc = SANE_DESC_NUM_OPTIONS;
  s->opt[OPT_NUM_OPTS].type = SANE_TYPE_INT;
  s->opt[OPT_NUM_OPTS].cap = SANE_CAP_SOFT_DETECT;
  s->val[OPT_NUM_OPTS].w = NUM_OPTIONS;

  /* "Mode" group: */
  s->opt[OPT_MODE_GROUP].title = SANE_I18N ("Scan Mode");
  s->opt[OPT_MODE_GROUP].desc = "";
  s->opt[OPT_MODE_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_MODE_GROUP].size = 0;
  s->opt[OPT_MODE_GROUP].cap = 0;
  s->opt[OPT_MODE_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* scan mode */
  s->opt[OPT_MODE].name = SANE_NAME_SCAN_MODE;
  s->opt[OPT_MODE].title = SANE_TITLE_SCAN_MODE;
  s->opt[OPT_MODE].desc = SANE_DESC_SCAN_MODE;
  s->opt[OPT_MODE].type = SANE_TYPE_STRING;
  s->opt[OPT_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_MODE].size = max_string_size (mode_list);
  s->opt[OPT_MODE].constraint.string_list = mode_list;
  s->val[OPT_MODE].s = strdup ("Gray");

  /* scan source */
  s->opt[OPT_SOURCE].name = SANE_NAME_SCAN_SOURCE;
  s->opt[OPT_SOURCE].title = SANE_TITLE_SCAN_SOURCE;
  s->opt[OPT_SOURCE].desc = SANE_DESC_SCAN_SOURCE;
  s->opt[OPT_SOURCE].type = SANE_TYPE_STRING;
  s->opt[OPT_SOURCE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_SOURCE].size = max_string_size (source_list);
  s->opt[OPT_SOURCE].constraint.string_list = source_list;
  s->val[OPT_SOURCE].s = strdup ("Flatbed");
  /*  status = gt68xx_device_get_ta_status (s->dev, &has_ta);
     if (status != SANE_STATUS_GOOD || !has_ta) */
  DISABLE (OPT_SOURCE);

  /* preview */
  s->opt[OPT_PREVIEW].name = SANE_NAME_PREVIEW;
  s->opt[OPT_PREVIEW].title = SANE_TITLE_PREVIEW;
  s->opt[OPT_PREVIEW].desc = SANE_DESC_PREVIEW;
  s->opt[OPT_PREVIEW].type = SANE_TYPE_BOOL;
  s->opt[OPT_PREVIEW].unit = SANE_UNIT_NONE;
  s->opt[OPT_PREVIEW].constraint_type = SANE_CONSTRAINT_NONE;
  s->val[OPT_PREVIEW].w = SANE_FALSE;

  /* bit depth */
  s->opt[OPT_BIT_DEPTH].name = SANE_NAME_BIT_DEPTH;
  s->opt[OPT_BIT_DEPTH].title = SANE_TITLE_BIT_DEPTH;
  s->opt[OPT_BIT_DEPTH].desc = SANE_DESC_BIT_DEPTH;
  s->opt[OPT_BIT_DEPTH].type = SANE_TYPE_INT;
  s->opt[OPT_BIT_DEPTH].constraint_type = SANE_CONSTRAINT_WORD_LIST;
  s->opt[OPT_BIT_DEPTH].size = sizeof (SANE_Word);
  s->opt[OPT_BIT_DEPTH].constraint.word_list = 0;
  s->opt[OPT_BIT_DEPTH].constraint.word_list = s->bpp_list;
  create_bpp_list (s, model->bpp_gray_values);
  s->val[OPT_BIT_DEPTH].w = 8;
  if (s->opt[OPT_BIT_DEPTH].constraint.word_list[0] < 2)
    DISABLE (OPT_BIT_DEPTH);

  /* resolution */
  for (count = 0; model->ydpi_values[count] != 0; count++);
  dpi_list = malloc ((count + 1) * sizeof (SANE_Word));
  if (!dpi_list)
    return SANE_STATUS_NO_MEM;
  dpi_list[0] = count;
  for (count = 0; model->ydpi_values[count] != 0; count++)
    dpi_list[count + 1] = model->ydpi_values[count];
  s->opt[OPT_RESOLUTION].name = SANE_NAME_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].type = SANE_TYPE_INT;
  s->opt[OPT_RESOLUTION].unit = SANE_UNIT_DPI;
  s->opt[OPT_RESOLUTION].constraint_type = SANE_CONSTRAINT_WORD_LIST;
  s->opt[OPT_RESOLUTION].constraint.word_list = dpi_list;
  s->val[OPT_RESOLUTION].w = 300;

  /* "Geometry" group: */
  s->opt[OPT_GEOMETRY_GROUP].title = SANE_I18N ("Geometry");
  s->opt[OPT_GEOMETRY_GROUP].desc = "";
  s->opt[OPT_GEOMETRY_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_GEOMETRY_GROUP].cap = SANE_CAP_ADVANCED;
  s->opt[OPT_GEOMETRY_GROUP].size = 0;
  s->opt[OPT_GEOMETRY_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  x_range.max = model->x_size;
  y_range.max = model->y_size;

  /* top-left x */
  s->opt[OPT_TL_X].name = SANE_NAME_SCAN_TL_X;
  s->opt[OPT_TL_X].title = SANE_TITLE_SCAN_TL_X;
  s->opt[OPT_TL_X].desc = SANE_DESC_SCAN_TL_X;
  s->opt[OPT_TL_X].type = SANE_TYPE_FIXED;
  s->opt[OPT_TL_X].unit = SANE_UNIT_MM;
  s->opt[OPT_TL_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TL_X].constraint.range = &x_range;
  s->val[OPT_TL_X].w = 0;

  /* top-left y */
  s->opt[OPT_TL_Y].name = SANE_NAME_SCAN_TL_Y;
  s->opt[OPT_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
  s->opt[OPT_TL_Y].desc = SANE_DESC_SCAN_TL_Y;
  s->opt[OPT_TL_Y].type = SANE_TYPE_FIXED;
  s->opt[OPT_TL_Y].unit = SANE_UNIT_MM;
  s->opt[OPT_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TL_Y].constraint.range = &y_range;
  s->val[OPT_TL_Y].w = 0;

  /* bottom-right x */
  s->opt[OPT_BR_X].name = SANE_NAME_SCAN_BR_X;
  s->opt[OPT_BR_X].title = SANE_TITLE_SCAN_BR_X;
  s->opt[OPT_BR_X].desc = SANE_DESC_SCAN_BR_X;
  s->opt[OPT_BR_X].type = SANE_TYPE_FIXED;
  s->opt[OPT_BR_X].unit = SANE_UNIT_MM;
  s->opt[OPT_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_X].constraint.range = &x_range;
  s->val[OPT_BR_X].w = x_range.max;

  /* bottom-right y */
  s->opt[OPT_BR_Y].name = SANE_NAME_SCAN_BR_Y;
  s->opt[OPT_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
  s->opt[OPT_BR_Y].desc = SANE_DESC_SCAN_BR_Y;
  s->opt[OPT_BR_Y].type = SANE_TYPE_FIXED;
  s->opt[OPT_BR_Y].unit = SANE_UNIT_MM;
  s->opt[OPT_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_Y].constraint.range = &y_range;
  s->val[OPT_BR_Y].w = y_range.max;

  /* "Extras" group: */
  s->opt[OPT_EXTRAS_GROUP].title = SANE_I18N ("Extras");
  s->opt[OPT_EXTRAS_GROUP].desc = "";
  s->opt[OPT_EXTRAS_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_EXTRAS_GROUP].cap = SANE_CAP_ADVANCED;
  s->opt[OPT_EXTRAS_GROUP].size = 0;
  s->opt[OPT_EXTRAS_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* Powersave time (turn lamp off) */
  s->opt[OPT_LAMP_OFF_TIME].name = "lamp-off-time";
  s->opt[OPT_LAMP_OFF_TIME].title = SANE_I18N ("Lamp off time");
  s->opt[OPT_LAMP_OFF_TIME].desc =
    SANE_I18N
    ("The lamp will be turned off after the given time (in minutes). "
     "A value of 0 means, that the lamp won't be turned off.");
  s->opt[OPT_LAMP_OFF_TIME].type = SANE_TYPE_INT;
  s->opt[OPT_LAMP_OFF_TIME].unit = SANE_UNIT_NONE;
  s->opt[OPT_LAMP_OFF_TIME].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_LAMP_OFF_TIME].constraint.range = &time_range;
  s->val[OPT_LAMP_OFF_TIME].w = 15;	/* 15 minutes */

  RIE (calc_parameters (s));

  DBG (DBG_proc, "init_options: exit\n");
  return SANE_STATUS_GOOD;
}

static SANE_Status
attach (SANE_String_Const devname, Genesys_Device ** devp, SANE_Bool may_wait)
{
  Genesys_Device *dev = 0;
  SANE_Int dn, vendor, product;
  SANE_Status status;
  int i;


  DBG (DBG_proc, "attach: start: devp %s NULL, may_wait = %d\n",
       devp ? "!=" : "==", may_wait);

  if (devp)
    *devp = 0;

  if (!devname)
    {
      DBG (DBG_error, "attach: devname == NULL\n");
      return SANE_STATUS_INVAL;
    }

  for (dev = first_dev; dev; dev = dev->next)
    {
      if (strcmp (dev->file_name, devname) == 0)
	{
	  if (devp)
	    *devp = dev;
	  DBG (DBG_info, "attach: device `%s' was already in device list\n",
	       devname);
	  return SANE_STATUS_GOOD;
	}
    }

  DBG (DBG_info, "attach: trying to open device `%s'\n", devname);

  status = sanei_usb_open (devname, &dn);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_warn, "attach: couldn't open device `%s': %s\n", devname,
	   sane_strstatus (status));
      return status;
    }
  else
    DBG (DBG_info, "attach: device `%s' successfully opened\n", devname);

  status = sanei_usb_get_vendor_product (dn, &vendor, &product);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "attach: couldn't get vendor and product ids of device `%s': %s\n",
	   devname, sane_strstatus (status));
      return status;
    }

  for (i = 0; i < MAX_SCANNERS && genesys_usb_device_list[i].model != 0; i++)
    {
      if (vendor == genesys_usb_device_list[i].vendor &&
	  product == genesys_usb_device_list[i].product)
	{
	  dev = malloc (sizeof (*dev));
	  if (!dev)
	    return SANE_STATUS_NO_MEM;
	  break;
	}
    }

  if (!dev)
    {
      DBG (DBG_error,
	   "attach: vendor %d product %d is not supported by this backend\n",
	   vendor, product);
      return SANE_STATUS_INVAL;
    }

  dev->file_name = strdup (devname);
  if (!dev)
    return SANE_STATUS_NO_MEM;

  dev->model = genesys_usb_device_list[i].model;
  dev->already_initialized = SANE_FALSE;

  DBG (DBG_info, "attach: found %s flatbed scanner %s at %s\n",
       dev->model->vendor, dev->model->model, dev->file_name);
  ++num_devices;
  dev->next = first_dev;
  first_dev = dev;

  if (devp)
    *devp = dev;
  sanei_usb_close (dn);
  DBG (DBG_proc, "attach: exit\n");
  return SANE_STATUS_GOOD;
}

static SANE_Status
attach_one_device (SANE_String_Const devname)
{
  Genesys_Device *dev;
  SANE_Status status;

  RIE (attach (devname, &dev, SANE_FALSE));

  if (dev)
    {
      /* Keep track of newly attached devices so we can set options as
         necessary.  */
      if (new_dev_len >= new_dev_alloced)
	{
	  new_dev_alloced += 4;
	  if (new_dev)
	    new_dev =
	      realloc (new_dev, new_dev_alloced * sizeof (new_dev[0]));
	  else
	    new_dev = malloc (new_dev_alloced * sizeof (new_dev[0]));
	  if (!new_dev)
	    {
	      DBG (DBG_error, "attach_one_device: out of memory\n");
	      return SANE_STATUS_NO_MEM;
	    }
	}
      new_dev[new_dev_len++] = dev;
    }
  return SANE_STATUS_GOOD;
}


/* -------------------------- SANE API functions ------------------------- */

SANE_Status
sane_init (SANE_Int * version_code, SANE_Auth_Callback authorize)
{
  SANE_Char line[PATH_MAX];
  SANE_Char *word;
  SANE_String_Const cp;
  SANE_Int linenumber;
  FILE *fp;

  DBG_INIT ();
  DBG (DBG_init, "SANE Genesys backend version %d.%d build %d from %s\n",
       V_MAJOR, V_MINOR, BUILD, PACKAGE_STRING);

  if (version_code)
    *version_code = SANE_VERSION_CODE (V_MAJOR, V_MINOR, BUILD);

  DBG (DBG_proc, "sane_init: authorize %s null\n", authorize ? "!=" : "==");

  sanei_usb_init ();

  num_devices = 0;
  first_dev = 0;
  first_handle = 0;
  devlist = 0;
  new_dev = 0;
  new_dev_len = 0;
  new_dev_alloced = 0;

  fp = sanei_config_open (GENESYS_CONFIG_FILE);
  if (!fp)
    {
      /* default to /dev/usb/scanner instead of insisting on config file */
      DBG (DBG_warn, "sane_init: couldn't open config file `%s': %s. Using "
	   "/dev/usb/scanner directly\n", GENESYS_CONFIG_FILE,
	   strerror (errno));
      attach ("/dev/usb/scanner", 0, SANE_FALSE);
      return SANE_STATUS_GOOD;
    }

  little_endian = calc_little_endian ();
  DBG (DBG_info, "sane_init: %s endian machine\n",
       little_endian ? "little" : "big");

  linenumber = 0;
  DBG (DBG_info, "sane_init: reading config file `%s'\n",
       GENESYS_CONFIG_FILE);
  while (sanei_config_read (line, sizeof (line), fp))
    {
      word = 0;
      linenumber++;

      cp = sanei_config_get_string (line, &word);
      if (!word || cp == line)
	{
	  DBG (DBG_io,
	       "sane_init: config file line %d: ignoring empty line\n",
	       linenumber);
	  if (word)
	    free (word);
	  continue;
	}
      if (word[0] == '#')
	{
	  DBG (DBG_io,
	       "sane_init: config file line %d: ignoring comment line\n",
	       linenumber);
	  free (word);
	  continue;
	}

      /*      else */
      {
	new_dev_len = 0;
	DBG (DBG_info,
	     "sane_init: config file line %d: trying to attach `%s'\n",
	     linenumber, line);
	sanei_usb_attach_matching_devices (line, attach_one_device);
	if (word)
	  free (word);
	word = 0;
      }
    }

  if (new_dev_alloced > 0)
    {
      new_dev_len = new_dev_alloced = 0;
      free (new_dev);
    }

  fclose (fp);
  DBG (DBG_proc, "sane_init: exit\n");

  return SANE_STATUS_GOOD;
}

void
sane_exit (void)
{
  Genesys_Device *dev, *next;

  DBG (DBG_proc, "sane_exit: start\n");
  for (dev = first_dev; dev; dev = next)
    {
      if (dev->already_initialized)
	{
	  if (dev->sensor.red_gamma_table)
	    free (dev->sensor.red_gamma_table);
	  if (dev->sensor.green_gamma_table)
	    free (dev->sensor.green_gamma_table);
	  if (dev->sensor.blue_gamma_table)
	    free (dev->sensor.blue_gamma_table);
	}
      next = dev->next;
      free (dev->file_name);
      free (dev);
    }
  first_dev = 0;
  first_handle = 0;
  if (devlist)
    free (devlist);
  devlist = 0;

  DBG (DBG_proc, "sane_exit: exit\n");
}

SANE_Status
sane_get_devices (const SANE_Device *** device_list, SANE_Bool local_only)
{
  Genesys_Device *dev;
  SANE_Int dev_num;

  DBG (DBG_proc, "sane_get_devices: start: local_only = %s\n",
       local_only == SANE_TRUE ? "true" : "false");

  if (devlist)
    free (devlist);

  devlist = malloc ((num_devices + 1) * sizeof (devlist[0]));
  if (!devlist)
    return SANE_STATUS_NO_MEM;

  dev_num = 0;
  for (dev = first_dev; dev_num < num_devices; dev = dev->next)
    {
      SANE_Device *sane_device;

      sane_device = malloc (sizeof (*sane_device));
      if (!sane_device)
	return SANE_STATUS_NO_MEM;
      sane_device->name = dev->file_name;
      sane_device->vendor = dev->model->vendor;
      sane_device->model = dev->model->model;
      sane_device->type = strdup ("flatbed scanner");
      devlist[dev_num++] = sane_device;
    }
  devlist[dev_num++] = 0;

  *device_list = devlist;

  DBG (DBG_proc, "sane_get_devices: exit\n");

  return SANE_STATUS_GOOD;
}

SANE_Status
sane_open (SANE_String_Const devicename, SANE_Handle * handle)
{
  Genesys_Device *dev;
  SANE_Status status;
  Genesys_Scanner *s;

  DBG (DBG_proc, "sane_open: start (devicename = `%s')\n", devicename);

  if (devicename[0])
    {
      for (dev = first_dev; dev; dev = dev->next)
	if (strcmp (dev->file_name, devicename) == 0)
	  break;

      if (!dev)
	{
	  DBG (DBG_info,
	       "sane_open: couldn't find `%s' in devlist, trying attach\n",
	       devicename);
	  RIE (attach (devicename, &dev, SANE_TRUE));
	}
      else
	DBG (DBG_info, "sane_open: found `%s' in devlist\n",
	     dev->model->name);
    }
  else
    {
      /* empty devicename -> use first device */
      dev = first_dev;
      if (dev)
	{
	  devicename = dev->file_name;
	  DBG (DBG_info, "sane_open: empty devicename, trying `%s'\n",
	       devicename);
	}
    }

  if (!dev)
    return SANE_STATUS_INVAL;

  if (dev->model->flags & GENESYS_FLAG_UNTESTED)
    {
      DBG (DBG_error0,
	   "WARNING: Your scanner is not fully supported or at least \n");
      DBG (DBG_error0,
	   "         had only limited testing. Please be careful and \n");
      DBG (DBG_error0, "         report any failure/success to \n");
      DBG (DBG_error0,
	   "         sane-devel@lists.alioth.debian.org. Please provide as many\n");
      DBG (DBG_error0,
	   "         details as possible, e.g. the exact name of your\n");
      DBG (DBG_error0, "         scanner and what does (not) work.\n");
    }

  status = sanei_usb_open (dev->file_name, &dev->dn);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_warn, "sane_open: couldn't open device `%s': %s\n",
	   dev->file_name, sane_strstatus (status));
      return status;
    }


  s = malloc (sizeof (*s));
  if (!s)
    return SANE_STATUS_NO_MEM;

  s->dev = dev;
  s->scanning = SANE_FALSE;
  s->dev->read_buffer = NULL;
  s->dev->resize_buffer = NULL;
  s->dev->read_active = SANE_FALSE;
  s->dev->white_average_data = NULL;
  s->dev->dark_average_data = NULL;

  /* insert newly opened handle into list of open handles: */
  s->next = first_handle;
  first_handle = s;
  *handle = s;

  RIE (init_options (s));

  if (genesys_init_cmd_set (s->dev) != SANE_STATUS_GOOD)
    {
      DBG (DBG_error0, "This device doesn't have a valid command set!!\n");
      return SANE_STATUS_IO_ERROR;
    }

  RIE (dev->model->cmd_set->init (dev));

  DBG (DBG_proc, "sane_open: exit\n");
  return SANE_STATUS_GOOD;
}

void
sane_close (SANE_Handle handle)
{
  Genesys_Scanner *prev, *s;

  DBG (DBG_proc, "sane_close: start\n");

  /* remove handle from list of open handles: */
  prev = 0;
  for (s = first_handle; s; s = s->next)
    {
      if (s == handle)
	break;
      prev = s;
    }
  if (!s)
    {
      DBG (DBG_error, "close: invalid handle %p\n", handle);
      return;			/* oops, not a handle we know about */
    }

  if (s->dev->read_buffer != NULL)
    free (s->dev->read_buffer);
  if (s->dev->resize_buffer != NULL)
    free (s->dev->resize_buffer);
  if (s->dev->white_average_data != NULL)
    free (s->dev->white_average_data);
  if (s->dev->dark_average_data != NULL)
    free (s->dev->dark_average_data);

  /* for an handful of bytes .. */
  free (s->opt[OPT_RESOLUTION].constraint.word_list);
  free (s->val[OPT_SOURCE].s);
  free (s->val[OPT_MODE].s);

  if (prev)
    prev->next = s->next;
  else
    first_handle = s->next;

  /* maybe todo: shut down scanner */

  sanei_usb_close (s->dev->dn);
  free (s);

  DBG (DBG_proc, "sane_close: exit\n");
}

const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
  Genesys_Scanner *s = handle;

  if ((unsigned) option >= NUM_OPTIONS)
    return 0;
  DBG (DBG_io2, "sane_get_option_descriptor: option = %s (%d)\n",
       s->opt[option].name, option);
  return s->opt + option;
}

SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option,
		     SANE_Action action, void *val, SANE_Int * info)
{
  Genesys_Scanner *s = handle;
  SANE_Status status;
  SANE_Word cap;
  SANE_Int myinfo = 0;

  DBG (DBG_io2,
       "sane_control_option: start: action = %s, option = %s (%d)\n",
       (action == SANE_ACTION_GET_VALUE) ? "get" : (action ==
						    SANE_ACTION_SET_VALUE) ?
       "set" : (action == SANE_ACTION_SET_AUTO) ? "set_auto" : "unknown",
       s->opt[option].name, option);

  if (info)
    *info = 0;

  if (s->scanning)
    {
      DBG (DBG_warn, "sane_control_option: don't call this function while "
	   "scanning (option = %s (%d))\n", s->opt[option].name, option);

      return SANE_STATUS_DEVICE_BUSY;
    }
  if (option >= NUM_OPTIONS || option < 0)
    {
      DBG (DBG_warn,
	   "sane_control_option: option %d >= NUM_OPTIONS || option < 0\n",
	   option);
      return SANE_STATUS_INVAL;
    }

  cap = s->opt[option].cap;

  if (!SANE_OPTION_IS_ACTIVE (cap))
    {
      DBG (DBG_warn, "sane_control_option: option %d is inactive\n", option);
      return SANE_STATUS_INVAL;
    }

  if (action == SANE_ACTION_GET_VALUE)
    {
      switch (option)
	{
	  /* word options: */
	case OPT_NUM_OPTS:
	case OPT_RESOLUTION:
	case OPT_BIT_DEPTH:
	case OPT_PREVIEW:
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
	case OPT_LAMP_OFF_TIME:
	  *(SANE_Word *) val = s->val[option].w;
	  break;
	  /* string options: */
	case OPT_MODE:
	  strcpy (val, s->val[option].s);
	  break;
	case OPT_SOURCE:
	  strcpy (val, s->val[option].s);
	  break;
	default:
	  DBG (DBG_warn, "sane_control_option: can't get unknown option %d\n",
	       option);
	}
    }
  else if (action == SANE_ACTION_SET_VALUE)
    {
      if (!SANE_OPTION_IS_SETTABLE (cap))
	{
	  DBG (DBG_warn, "sane_control_option: option %d is not settable\n",
	       option);
	  return SANE_STATUS_INVAL;
	}

      status = sanei_constrain_value (s->opt + option, val, &myinfo);

      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_warn,
	       "sane_control_option: sanei_constrain_value returned %s\n",
	       sane_strstatus (status));
	  return status;
	}

      switch (option)
	{
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
	  s->val[option].w = *(SANE_Word *) val;
	  RIE (calc_parameters (s));
	  myinfo |= SANE_INFO_RELOAD_PARAMS;
	  break;
	case OPT_RESOLUTION:
	case OPT_BIT_DEPTH:
	case OPT_PREVIEW:
	  s->val[option].w = *(SANE_Word *) val;
	  RIE (calc_parameters (s));
	  myinfo |= SANE_INFO_RELOAD_PARAMS;
	  break;
	case OPT_SOURCE:
	  if (strcmp (s->val[option].s, val) != 0)
	    {			/* something changed */
	      if (s->val[option].s)
		free (s->val[option].s);
	      s->val[option].s = strdup (val);
	      if (strcmp (s->val[option].s, "Transparency Adapter") == 0)
		{
		  /* RIE (gt68xx_device_lamp_control
		     (s->dev, SANE_FALSE, SANE_TRUE)); 
		     x_range.max = s->dev->model->x_size_ta;
		     y_range.max = s->dev->model->y_size_ta; */
		}
	      else
		{
		  /* RIE (gt68xx_device_lamp_control
		     (s->dev, SANE_TRUE, SANE_FALSE));
		     x_range.max = s->dev->model->x_size;
		     y_range.max = s->dev->model->y_size; */
		}
	      myinfo |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
	    }
	  break;
	case OPT_MODE:
	  if (s->val[option].s)
	    free (s->val[option].s);
	  s->val[option].s = strdup (val);
	  if (strcmp (s->val[option].s, "Lineart") == 0)
	    {
	      /* ENABLE (OPT_THRESHOLD); */
	      DISABLE (OPT_BIT_DEPTH);
	    }
	  else
	    {
	      /* DISABLE (OPT_THRESHOLD); */
	      if (strcmp (s->val[option].s, "Gray") == 0)
		create_bpp_list (s, s->dev->model->bpp_gray_values);
	      else
		create_bpp_list (s, s->dev->model->bpp_color_values);
	      if (s->bpp_list[0] < 2)
		DISABLE (OPT_BIT_DEPTH);
	      else
		ENABLE (OPT_BIT_DEPTH);
	    }
	  RIE (calc_parameters (s));
	  myinfo |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
	  break;
	case OPT_LAMP_OFF_TIME:
	  if (*(SANE_Word *) val != s->val[option].w)
	    {
	      s->val[option].w = *(SANE_Word *) val;
	      RIE (s->dev->model->cmd_set->
		   set_powersaving (s->dev, s->val[option].w));
	    }
	  break;
	  /* string options: */

	default:
	  DBG (DBG_warn, "sane_control_option: can't set unknown option %d\n",
	       option);
	}
    }
  else
    {
      DBG (DBG_warn, "sane_control_option: unknown action %d for option %d\n",
	   action, option);
      return SANE_STATUS_INVAL;
    }
  if (info)
    *info = myinfo;

  DBG (DBG_io2, "sane_control_option: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters * params)
{
  Genesys_Scanner *s = handle;
  SANE_Status status;

  DBG (DBG_proc, "sane_get_parameters: start\n");

  RIE (calc_parameters (s));
  if (params)
    *params = s->params;

  DBG (DBG_proc, "sane_get_parameters: exit\n");

  return SANE_STATUS_GOOD;
}

SANE_Status
sane_start (SANE_Handle handle)
{
  Genesys_Scanner *s = handle;
  SANE_Status status;

  DBG (DBG_proc, "sane_start: start\n");

  if (s->val[OPT_TL_X].w >= s->val[OPT_BR_X].w)
    {
      DBG (DBG_error0,
	   "sane_start: top left x >= bottom right x --- exiting\n");
      return SANE_STATUS_INVAL;
    }
  if (s->val[OPT_TL_Y].w >= s->val[OPT_BR_Y].w)
    {
      DBG (DBG_error0,
	   "sane_start: top left y >= bottom right y --- exiting\n");
      return SANE_STATUS_INVAL;
    }

  /* First make sure we have a current parameter set.  Some of the
     parameters will be overwritten below, but that's OK.  */

  RIE (calc_parameters (s));
  RIE (genesys_start_scan (s->dev));

  s->scanning = SANE_TRUE;

  DBG (DBG_proc, "sane_start: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_read (SANE_Handle handle, SANE_Byte * buf, SANE_Int max_len,
	   SANE_Int * len)
{
  Genesys_Scanner *s = handle;
  SANE_Status status;
  size_t local_len;

  if (!s)
    {
      DBG (DBG_error, "sane_read: handle is null!\n");
      return SANE_STATUS_INVAL;
    }

  if (!buf)
    {
      DBG (DBG_error, "sane_read: buf is null!\n");
      return SANE_STATUS_INVAL;
    }

  if (!len)
    {
      DBG (DBG_error, "sane_read: len is null!\n");
      return SANE_STATUS_INVAL;
    }

  *len = 0;

  if (!s->scanning)
    {
      DBG (DBG_warn, "sane_read: scan was cancelled, is over or has not been "
	   "initiated yet\n");
      return SANE_STATUS_CANCELLED;
    }

  DBG (DBG_proc, "sane_read: start\n");

  local_len = max_len;
  status = genesys_read_ordered_data (s->dev, buf, &local_len);

  *len = local_len;
  return status;
}

void
sane_cancel (SANE_Handle handle)
{
  Genesys_Scanner *s = handle;
  SANE_Status status = SANE_STATUS_GOOD;

  DBG (DBG_proc, "sane_cancel: start\n");

  s->scanning = SANE_FALSE;
  s->dev->read_active = SANE_FALSE;

  status = s->dev->model->cmd_set->end_scan (s->dev, s->dev->reg, SANE_TRUE);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "sane_cancel: Failed to end scan: %s\n",
	   sane_strstatus (status));
      return;
    }

  /* park head */
  if (s->dev->model->flags & GENESYS_FLAG_USE_PARK)
    status = s->dev->model->cmd_set->park_head (s->dev, s->dev->reg, 1);
  else
    status = s->dev->model->cmd_set->slow_back_home (s->dev, 1);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "sane_cancel: failed to move scanhead to home position: %s\n",
	   sane_strstatus (status));
      return;
    }

  DBG (DBG_proc, "sane_cancel: exit\n");
  return;
}

SANE_Status
sane_set_io_mode (SANE_Handle handle, SANE_Bool non_blocking)
{
  Genesys_Scanner *s = handle;

  DBG (DBG_proc, "sane_set_io_mode: handle = %p, non_blocking = %s\n",
       handle, non_blocking == SANE_TRUE ? "true" : "false");

  if (!s->scanning)
    {
      DBG (DBG_error, "sane_set_io_mode: not scanning\n");
      return SANE_STATUS_INVAL;
    }
  if (non_blocking)
    return SANE_STATUS_UNSUPPORTED;
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_get_select_fd (SANE_Handle handle, SANE_Int * fd)
{
  Genesys_Scanner *s = handle;

  DBG (DBG_proc, "sane_get_select_fd: handle = %p, fd = %p\n", handle,
       (void *) fd);

  if (!s->scanning)
    {
      DBG (DBG_error, "sane_get_select_fd: not scanning\n");
      return SANE_STATUS_INVAL;
    }
  return SANE_STATUS_UNSUPPORTED;
}
