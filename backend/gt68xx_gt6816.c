/* sane - Scanner Access Now Easy.

   Copyright (C) 2002 Sergey Vlasov <vsu@altlinux.ru>
   Copyright (C) 2002-2004 Henning Meier-Geinitz <henning@meier-geinitz.de>
   
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
 * @brief Implementation of the gt6816 specific functions.
 */

#include "gt68xx_gt6816.h"

SANE_Status
gt6816_check_firmware (GT68xx_Device * dev, SANE_Bool * loaded)
{
  SANE_Status status;
  GT68xx_Packet req;

  memset (req, 0, sizeof (req));
  req[0] = 0x70;
  req[1] = 0x01;

  status = gt68xx_device_small_req (dev, req, req);
  if (status != SANE_STATUS_GOOD)
    {
      /* Assume that firmware is not loaded because without firmware, we need
	 64 bytes for the result, not 8 */
      *loaded = SANE_FALSE;
      return SANE_STATUS_GOOD;
    }
  /* check anyway */
  if (req[0] == 0x00 && req[1] == 0x70 && req[2] == 0xff)
    *loaded = SANE_TRUE;
  else
    *loaded = SANE_FALSE;

  return SANE_STATUS_GOOD;
}


#define MAX_DOWNLOAD_BLOCK_SIZE 64

SANE_Status
gt6816_download_firmware (GT68xx_Device * dev,
			  SANE_Byte * data, SANE_Word size)
{
  SANE_Status status;
  SANE_Byte download_buf[MAX_DOWNLOAD_BLOCK_SIZE];
  SANE_Byte check_buf[MAX_DOWNLOAD_BLOCK_SIZE];
  SANE_Byte *block;
  SANE_Word addr, bytes_left;
  GT68xx_Packet boot_req;
  SANE_Word block_size = MAX_DOWNLOAD_BLOCK_SIZE;

  CHECK_DEV_ACTIVE (dev, "gt6816_download_firmware");

  for (addr = 0; addr < size; addr += block_size)
    {
      bytes_left = size - addr;
      if (bytes_left > block_size)
	block = data + addr;
      else
	{
	  memset (download_buf, 0, block_size);
	  memcpy (download_buf, data + addr, bytes_left);
	  block = download_buf;
	}
      RIE (gt68xx_device_memory_write (dev, addr, block_size, block));
      RIE (gt68xx_device_memory_read (dev, addr, block_size, check_buf));
      if (memcmp (block, check_buf, block_size) != 0)
	{
	  DBG (3, "gt6816_download_firmware: mismatch at block 0x%0x\n", addr);
	  return SANE_STATUS_IO_ERROR;
	}
    }

  memset (boot_req, 0, sizeof (boot_req));
  boot_req[0] = 0x69;
  boot_req[1] = 0x01;
  boot_req[2] = LOBYTE (addr);
  boot_req[3] = HIBYTE (addr);
  RIE (gt68xx_device_req (dev, boot_req, boot_req));

  return SANE_STATUS_GOOD;
}


SANE_Status
gt6816_get_power_status (GT68xx_Device * dev, SANE_Bool * power_ok)
{
  SANE_Status status;
  GT68xx_Packet req;

  memset (req, 0, sizeof (req));
  req[0] = 0x3f;
  req[1] = 0x01;

  RIE (gt68xx_device_req (dev, req, req));

  if ((req[0] == 0x00 && req[1] == 0x3f && req[2] == 0x01) 
      || (dev->model->flags & GT68XX_FLAG_NO_POWER_STATUS))
    *power_ok = SANE_TRUE;
  else
    *power_ok = SANE_FALSE;

  return SANE_STATUS_GOOD;
}

SANE_Status
gt6816_get_ta_status (GT68xx_Device * dev, SANE_Bool * ta_attached)
{
  SANE_Status status;
  GT68xx_Packet req;

  memset (req, 0, sizeof (req));
  req[0] = 0x28;
  req[1] = 0x01;

  RIE (gt68xx_device_req (dev, req, req));

  if (req[0] == 0x00 && req[1] == 0x28 && (req[8] & 0x01) != 0
      && !dev->model->is_cis)
    *ta_attached = SANE_TRUE;
  else
    *ta_attached = SANE_FALSE;

  return SANE_STATUS_GOOD;
}


SANE_Status
gt6816_lamp_control (GT68xx_Device * dev, SANE_Bool fb_lamp,
		     SANE_Bool ta_lamp)
{
  GT68xx_Packet req;

  memset (req, 0, sizeof (req));
  req[0] = 0x25;
  req[1] = 0x01;
  req[2] = 0;
  if (fb_lamp)
    req[2] |= 0x01;
  if (ta_lamp)
    req[2] |= 0x02;

  return gt68xx_device_req (dev, req, req);
}


SANE_Status
gt6816_is_moving (GT68xx_Device * dev, SANE_Bool * moving)
{
  SANE_Status status;
  GT68xx_Packet req;

  memset (req, 0, sizeof (req));
  req[0] = 0x17;
  req[1] = 0x01;

  RIE (gt68xx_device_req (dev, req, req));

  if (req[0] == 0x00 && req[1] == 0x17)
    {
      if (req[2] == 0 && (req[3] == 0 || req[3] == 2))
	*moving = SANE_FALSE;
      else
	*moving = SANE_TRUE;
    }
  else
    return SANE_STATUS_IO_ERROR;

  return SANE_STATUS_GOOD;
}


SANE_Status
gt6816_carriage_home (GT68xx_Device * dev)
{
  GT68xx_Packet req;

  memset (req, 0, sizeof (req));
  req[0] = 0x24;
  req[1] = 0x01;

  return gt68xx_device_req (dev, req, req);
}


SANE_Status
gt6816_stop_scan (GT68xx_Device * dev)
{
  GT68xx_Packet req;

  memset (req, 0, sizeof (req));
  req[0] = 0x41;
  req[1] = 0x01;

  return gt68xx_device_small_req (dev, req, req);
}

SANE_Status
gt6816_setup_scan (GT68xx_Device * dev,
		   GT68xx_Scan_Request * request,
		   GT68xx_Scan_Action action, GT68xx_Scan_Parameters * params)
{
  SANE_Status status;
  GT68xx_Model *model;
  SANE_Int xdpi, ydpi;
  SANE_Bool color;
  SANE_Int depth;
  SANE_Int pixel_x0, pixel_y0, pixel_xs, pixel_ys;
  SANE_Int pixel_align;

  SANE_Int abs_x0, abs_y0, abs_xs, abs_ys, base_xdpi, base_ydpi;
  SANE_Int scan_xs, scan_ys, scan_bpl;
  SANE_Int bits_per_line;
  SANE_Byte color_mode_code;
  SANE_Bool line_mode;
  SANE_Int overscan_lines;
  SANE_Fixed x0, y0, xs, ys;
  SANE_Bool backtrack = SANE_FALSE;

  DBG (6, "gt6816_setup_scan: enter (action=%s)\n",
	 action == SA_CALIBRATE ? "calibrate" :
	 action == SA_CALIBRATE_ONE_LINE ? "calibrate one line" :
	 action == SA_SCAN ? "scan" : "calculate only");

  model = dev->model;

  xdpi = request->xdpi;
  ydpi = request->ydpi;
  color = request->color;
  depth = request->depth;

  base_xdpi = model->base_xdpi;
  base_ydpi = model->base_ydpi;

  if (xdpi > model->base_xdpi)
    base_xdpi = model->optical_xdpi;

  /* Special fixes */
  if (strcmp (model->name, "mustek-bearpaw-2400-ta-plus") == 0 && xdpi <= 50)
    base_xdpi = model->optical_xdpi;
  if (ydpi <= 100)
    {
      if ((strcmp (model->name, "mustek-bearpaw-2400-ta-plus") == 0 ||
	   strcmp (model->name, "mustek-bearpaw-2400-ta") == 0) &&
	  !request->use_ta && action == SA_SCAN)
	request->mbs = SANE_TRUE;	/* 50/100 dpi need a mimimum y0 */
    }

  if (!model->constant_ydpi)
    {
      if (ydpi > model->base_ydpi)
	base_ydpi = model->optical_ydpi;
    }

  DBG (6, "gt6816_setup_scan: base_xdpi=%d, base_ydpi=%d\n", 
       base_xdpi, base_ydpi);


  switch (action)
    {
    case SA_CALIBRATE_ONE_LINE:
      {
	x0 = request->x0;
	if (request->use_ta)
	  y0 = model->y_offset_calib_ta;
	else
	  y0 = model->y_offset_calib;
	ys = SANE_FIX (1.0 * MM_PER_INCH / ydpi);	/* one line */
	xs = request->xs;
	depth = 8;
	break;
      }
    case SA_CALIBRATE:
      {
	if (request->use_ta)
	  {
	    x0 = request->x0 + model->x_offset_ta;
	    if (request->mbs)
	      y0 = model->y_offset_calib_ta;
	    else
	      y0 = 0;
	  }
	else
	  {
	    x0 = request->x0 + model->x_offset;
	    if (request->mbs)
	      y0 = model->y_offset_calib;
	    else
	      y0 = 0;
	  }
	ys = SANE_FIX (CALIBRATION_HEIGHT);
	xs = request->xs;
	break;
      }
    case SA_SCAN:
      {
	SANE_Fixed x_offset, y_offset;
	if (request->use_ta)
	  {
	    x_offset = model->x_offset_ta;
	    if (request->mbs)
	      y_offset = model->y_offset_ta;
	    else
	      y_offset = model->y_offset_ta - model->y_offset_calib_ta
		- SANE_FIX (CALIBRATION_HEIGHT);
	  }
	else
	  {
	    x_offset = model->x_offset;
	    if (request->mbs)
	      y_offset = model->y_offset;
	    else
	      y_offset = model->y_offset - model->y_offset_calib
		- SANE_FIX (CALIBRATION_HEIGHT);
	  }
	x0 = request->x0 + x_offset;
	y0 = request->y0 + y_offset;
	if (y0 < 0)
	  y0 = 0;
	ys = request->ys;
	xs = request->xs;

	backtrack = request->backtrack;
	break;
      }

    default:
      DBG (1, "gt6816_setup_scan: invalid action=%d\n", (int) action);
      return SANE_STATUS_INVAL;
    }

  pixel_x0 = SANE_UNFIX (x0) * xdpi / MM_PER_INCH + 0.5;
  pixel_y0 = SANE_UNFIX (y0) * ydpi / MM_PER_INCH + 0.5;
  pixel_ys = SANE_UNFIX (ys) * ydpi / MM_PER_INCH + 0.5;
  pixel_xs = SANE_UNFIX (xs) * xdpi / MM_PER_INCH + 0.5;


  DBG (6, "gt6816_setup_scan: xdpi=%d, ydpi=%d\n", xdpi, ydpi);
  DBG (6, "gt6816_setup_scan: color=%s, depth=%d\n",
	 color ? "TRUE" : "FALSE", depth);
  DBG (6, "gt6816_setup_scan: pixel_x0=%d, pixel_y0=%d\n",
	 pixel_x0, pixel_y0);
  DBG (6, "gt6816_setup_scan: pixel_xs=%d, pixel_ys=%d\n",
	 pixel_xs, pixel_ys);
  DBG (6, "gt6816_setup_scan: backtrack=%d\n", backtrack);


  color_mode_code = 0x80;
  if (color)
    color_mode_code |= (1 << 2);
  else
    color_mode_code |= dev->gray_mode_color;

  if (depth > 12)
    color_mode_code |= (1 << 5);
  else if (depth > 8)
    {
      color_mode_code &= 0x7f;
      color_mode_code |= (1 << 4);
    }

  DBG (6, "gt6816_setup_scan: color_mode_code = 0x%02X\n",
	 color_mode_code);

  overscan_lines = 0;
  params->ld_shift_r = params->ld_shift_g = params->ld_shift_b = 0;
  params->ld_shift_double = 0;

  if (action == SA_SCAN && color)
    {
      /* Line distance correction is required for color scans. */
      SANE_Int optical_ydpi = model->optical_ydpi;
      SANE_Int ld_shift_r = model->ld_shift_r;
      SANE_Int ld_shift_g = model->ld_shift_g;
      SANE_Int ld_shift_b = model->ld_shift_b;
      SANE_Int max_ld = MAX (MAX (ld_shift_r, ld_shift_g), ld_shift_b);

      overscan_lines = max_ld * ydpi / optical_ydpi;
      params->ld_shift_r = ld_shift_r * ydpi / optical_ydpi;
      params->ld_shift_g = ld_shift_g * ydpi / optical_ydpi;
      params->ld_shift_b = ld_shift_b * ydpi / optical_ydpi;
      params->ld_shift_double = 0;
      DBG (6, "gt6816_setup_scan: overscan=%d, ld=%d/%d/%d\n",
	     overscan_lines, params->ld_shift_r, params->ld_shift_g,
	     params->ld_shift_b);
    }

  if (action == SA_SCAN && xdpi >= model->optical_xdpi
      && model->ld_shift_double > 0)
    {
      params->ld_shift_double =
	model->ld_shift_double * ydpi / model->optical_ydpi;
      if (color)
	overscan_lines += (params->ld_shift_double * 3);
      else
	overscan_lines += params->ld_shift_double;

      DBG (6, "gt6816_setup_scan: overscan=%d, ld double=%d\n",
	     overscan_lines, params->ld_shift_double);
    }

  abs_x0 = pixel_x0 * base_xdpi / xdpi;
  abs_y0 = pixel_y0 * base_ydpi / ydpi;
  DBG (6, "gt6816_setup_scan: abs_x0=%d, abs_y0=%d\n", abs_x0, abs_y0);

  params->double_column = abs_x0 & 1;

  /* Calculate minimum number of pixels which span an integral multiple of 64
   * bytes. */
  pixel_align = 32;		/* best case for depth = 16 */
  while ((depth * pixel_align) % (64 * 8) != 0)
    pixel_align *= 2;
  DBG (6, "gt6816_setup_scan: pixel_align=%d\n", pixel_align);

  if (pixel_xs % pixel_align == 0)
    scan_xs = pixel_xs;
  else
    scan_xs = (pixel_xs / pixel_align + 1) * pixel_align;
  scan_ys = pixel_ys + overscan_lines;

  abs_xs = scan_xs * base_xdpi / xdpi;
  if (action == SA_CALIBRATE_ONE_LINE)
    abs_ys = 2;
  else
    abs_ys = scan_ys * base_ydpi / ydpi;
  DBG (6, "gt6816_setup_scan: abs_xs=%d, abs_ys=%d\n", abs_xs, abs_ys);

  if (model->is_cis)
    {
      line_mode = SANE_TRUE;
    }
  else
    {
      line_mode = SANE_FALSE;
      if (!color)
	{
	  DBG (6, "gt6816_setup_scan: using line mode for monochrome scan\n");
	  line_mode = SANE_TRUE;
	}
      else if (ydpi >= model->ydpi_force_line_mode) /* really necessary? */
	{
	  DBG (6, "gt6816_setup_scan: forcing line mode for ydpi=%d\n",
		 ydpi);
	  line_mode = SANE_TRUE;
	}
    }
  bits_per_line = depth * scan_xs;
  if (color && !line_mode)
    bits_per_line *= 3;
  if (bits_per_line % 8)	/* impossible */
    {
      DBG (0, "gt6816_setup_scan: BUG: unaligned bits_per_line=%d\n",
	     bits_per_line);
      return SANE_STATUS_INVAL;
    }
  scan_bpl = bits_per_line / 8;

  if (scan_bpl > 15600 && !line_mode)	/* ??? */
    {
      DBG (6, "gt6816_setup_scan: scan_bpl=%d, trying line mode\n",
	     scan_bpl);
      line_mode = SANE_TRUE;
      if (scan_bpl % 3)
	{
	  DBG (0, "gt6816_setup_scan: BUG: monochrome scan in pixel mode?\n");
	  return SANE_STATUS_INVAL;
	}
      scan_bpl /= 3;
    }

  if (scan_bpl % 64)		/* impossible */
    {
      DBG (0, "gt6816_setup_scan: BUG: unaligned scan_bpl=%d\n", scan_bpl);
      return SANE_STATUS_INVAL;
    }

  /* really? */
  if (color && line_mode)
    scan_ys *= 3;

  DBG (6, "gt6816_setup_scan: scan_xs=%d, scan_ys=%d\n",scan_xs, scan_ys);

  DBG (6, "gt6816_setup_scan: scan_bpl=%d\n", scan_bpl);

  if (!request->calculate)
    {
      GT68xx_Packet req;
      SANE_Byte motor_mode_1, motor_mode_2;

      if (scan_bpl > 15600)		/* ??? */
	{
	  DBG (0, "gt6816_setup_scan: scan_bpl=%d, too large\n", scan_bpl);
	  return SANE_STATUS_NO_MEM;
	}

      if ((dev->model->flags & GT68XX_FLAG_NO_LINEMODE) && line_mode && color)
	{
	  DBG (0, "gt6816_setup_scan: the scanner's memory is too small for "
	       "that combination of resolution, dpi and width\n");
	  return SANE_STATUS_NO_MEM;
	}

      motor_mode_1 = (request->mbs ? 0 : 1) << 1;
      motor_mode_1 |= (request->mds ? 0 : 1) << 2;
      motor_mode_1 |= (request->mas ? 0 : 1) << 0;

      motor_mode_1 |= (backtrack ? 1 : 0) << 3;

      motor_mode_2 = (request->lamp ? 0 : 1) << 0;
      motor_mode_2 |= (line_mode ? 0 : 1) << 2;

      if (action != SA_SCAN)
	motor_mode_2 |= 1 << 3;

      DBG (6, "gt6816_setup_scan: motor_mode_1 = 0x%02X, motor_mode_2 = 0x%02X\n",
	     motor_mode_1, motor_mode_2);

      /* Fill in the setup command */
      memset (req, 0, sizeof (req));
      req[0x00] = 0x20;
      req[0x01] = 0x01;
      req[0x02] = LOBYTE (abs_y0);
      req[0x03] = HIBYTE (abs_y0);
      req[0x04] = LOBYTE (abs_ys);
      req[0x05] = HIBYTE (abs_ys);
      req[0x06] = LOBYTE (abs_x0);
      req[0x07] = HIBYTE (abs_x0);
      req[0x08] = LOBYTE (abs_xs);
      req[0x09] = HIBYTE (abs_xs);
      req[0x0a] = color_mode_code;
      if (model->is_cis)
	req[0x0b] = 0x60;
      else
	req[0x0b] = 0x20; 
      req[0x0c] = LOBYTE (xdpi);
      req[0x0d] = HIBYTE (xdpi);
      req[0x0e] = 0x12;		/* 0x12 */
      req[0x0f] = 0x00;         /* 0x00 */  
      req[0x10] = LOBYTE (scan_bpl);
      req[0x11] = HIBYTE (scan_bpl);
      req[0x12] = LOBYTE (scan_ys);
      req[0x13] = HIBYTE (scan_ys);
      req[0x14] = motor_mode_1;
      req[0x15] = motor_mode_2;
      req[0x16] = LOBYTE (ydpi);
      req[0x17] = HIBYTE (ydpi);
      if (backtrack)
	req[0x18] = request->backtrack_lines;
      else
	req[0x18] = 0x00;
      status = gt68xx_device_req (dev, req, req);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (3, "gt6816_setup_scan: setup request failed: %s\n",
		 sane_strstatus (status));
	  return status;
	}
    }

  /* Fill in calculated values */
  params->xdpi = xdpi;
  params->ydpi = ydpi;
  params->depth = depth;
  params->color = color;
  params->pixel_xs = pixel_xs;
  params->pixel_ys = pixel_ys;
  params->scan_xs = scan_xs;
  params->scan_ys = scan_ys;
  params->scan_bpl = scan_bpl;
  params->line_mode = line_mode;
  params->overscan_lines = overscan_lines;

  DBG (6, "gt6816_setup_scan: leave: ok\n");
  return SANE_STATUS_GOOD;
}

