/* sane - Scanner Access Now Easy.

   Copyright (C) 2002 Sergey Vlasov <vsu@altlinux.ru>
   GT6801 support by Andreas Nowack <nowack.andreas@gmx.de>
   Copyright (C) 2002 Henning Meier-Geinitz <henning@meier-geinitz.de>
   
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
 * @brief Implementation of the GT6801 specific functions.
 */

#include "gt68xx_gt6801.h"

#if 0
SANE_Status
gt6801_check_firmware (GT68xx_Device * dev, SANE_Bool * loaded)
{
  SANE_Status status;
  GT68xx_Packet req;

  memset (req, 0, sizeof (req));
  req[0] = 0x00;
  req[1] = 0x01;

  RIE (gt68xx_device_small_req (dev, req, req));

  /*
   * this is wrong but i do not get the right answer from the scanner....
   * should be 0x00 0x55 0x55 0x55
   */
  if (req[0] == 0x00 && req[1] == 0x12 && req[2] == 0x00 && req[3] == 0x00)
    *loaded = SANE_TRUE;
  else
    *loaded = SANE_FALSE;

  return SANE_STATUS_GOOD;
}
#endif
#if 1
/* doesn't work with plustek scanner */
SANE_Status
gt6801_check_firmware (GT68xx_Device * dev, SANE_Bool * loaded)
{
  SANE_Status status;
  GT68xx_Packet req;

  memset (req, 0, sizeof (req));
  req[0] = 0x50;
  req[1] = 0x01;
  req[2] = 0x80;

  RIE (gt68xx_device_req (dev, req, req));

  if (req[0] == 0x00 && req[1] == 0x50)
    *loaded = SANE_TRUE;
  else
    *loaded = SANE_FALSE;

  return SANE_STATUS_GOOD;
}
#endif

/* doesn't work with at least cytron scanner */
SANE_Status
gt6801_check_plustek_firmware (GT68xx_Device * dev, SANE_Bool * loaded)
{
  SANE_Status status;
  GT68xx_Packet req;

  memset (req, 0, sizeof (req));

  req[0] = 0x73;
  req[1] = 0x01;

  RIE (gt68xx_device_small_req (dev, req, req));

  /* check for correct answer */
  if ((req[0] == 0) && (req[1] == 0x12) && (req[3] != 0))
    /* req[3] is 0 if fw is not loaded, if loaded it's 0x80
     * at least on my 1284u (gerhard@gjaeger.de)
     */
    *loaded = SANE_TRUE;
  else
    *loaded = SANE_FALSE;

  return SANE_STATUS_GOOD;
}

#define MAX_DOWNLOAD_BLOCK_SIZE 64

SANE_Status
gt6801_download_firmware (GT68xx_Device * dev,
			  SANE_Byte * data, SANE_Word size)
{
  DECLARE_FUNCTION_NAME ("gt6801_download_firmware") SANE_Status status;
  SANE_Byte download_buf[MAX_DOWNLOAD_BLOCK_SIZE];
  SANE_Byte check_buf[MAX_DOWNLOAD_BLOCK_SIZE];
  SANE_Byte *block;
  SANE_Word addr, bytes_left;
  GT68xx_Packet boot_req;
  SANE_Word block_size = MAX_DOWNLOAD_BLOCK_SIZE;

  CHECK_DEV_ACTIVE (dev, function_name);

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
      RIE (gt68xx_device_memory_read (dev, 0x3f00, block_size, check_buf));

      /*
       * For GT6816 this was:
       *   if (memcmp (block, check_buf, block_size) != 0) ...
       * Apparently the GT6801 does something different...
       * 
       * hmg: For my BP 1200 CU the result is 00 09 so maybe only the 0 is
       * relevant?
       */
      if ((check_buf[0] != 0) && (check_buf[1] != 0x40))
	{
	  XDBG ((3, "%s: mismatch at block 0x%0x\n", function_name, addr));
	  return SANE_STATUS_IO_ERROR;
	}
    }

  memset (boot_req, 0, sizeof (boot_req));
  boot_req[0] = 0x69;
  boot_req[1] = 0x01;
  boot_req[2] = 0xc0;
  boot_req[3] = 0x1c;
  RIE (gt68xx_device_req (dev, boot_req, boot_req));

#if 0
  /* hmg: the following isn't in my log: */
  memset (boot_req, 0, sizeof (boot_req));	/* I don't know if this is needed */
  boot_req[0] = 0x01;
  boot_req[1] = 0x01;
  RIE (gt68xx_device_small_req (dev, boot_req, boot_req));
#endif

  return SANE_STATUS_GOOD;
}


SANE_Status
gt6801_get_power_status (GT68xx_Device * dev, SANE_Bool * power_ok)
{
  SANE_Status status;
  GT68xx_Packet req;

  memset (req, 0, sizeof (req));
  req[0] = 0x10;
  req[1] = 0x01;

  RIE (gt68xx_device_req (dev, req, req));

  /* I don't know what power_ok = SANE_FALSE looks like... */
  /* hmg: let's assume it's different from the usual 00 10 */
  if (gt68xx_device_check_result (req, 0x10) == SANE_STATUS_GOOD)
    *power_ok = SANE_TRUE;
  else
    *power_ok = SANE_FALSE;

  return SANE_STATUS_GOOD;
}


SANE_Status
gt6801_lamp_control (GT68xx_Device * dev, SANE_Bool fb_lamp,
		     SANE_Bool ta_lamp)
{
  if (!dev->model->is_cis)
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
  return SANE_STATUS_GOOD;
}


SANE_Status
gt6801_is_moving (GT68xx_Device * dev, SANE_Bool * moving)
{
  /* this seems not to be supported by the scanner */
  (void) dev;

  *moving = SANE_FALSE;

  return SANE_STATUS_GOOD;
}


SANE_Status
gt6801_carriage_home (GT68xx_Device * dev)
{
  GT68xx_Packet req;
  SANE_Status status;

  memset (req, 0, sizeof (req));

  if (dev->model->flags & GT68XX_FLAG_MOTOR_HOME)
    {
      req[0] = 0x34;
      req[1] = 0x01;
      status = gt68xx_device_req (dev, req, req);
    }
  else
    {
      req[0] = 0x12;
      req[1] = 0x01;
      if ((status = gt68xx_device_req (dev, req, req)) == SANE_STATUS_GOOD)
	{
	  RIE (gt68xx_device_check_result (req, 0x12));
	  memset (req, 0, sizeof (req));
	  req[0] = 0x24;
	  req[1] = 0x01;
	  status = gt68xx_device_req (dev, req, req);
	  RIE (gt68xx_device_check_result (req, 0x24));
	}
    }
  return status;
}


SANE_Status
gt6801_stop_scan (GT68xx_Device * dev)
{
  GT68xx_Packet req;
  SANE_Status status;

  memset (req, 0, sizeof (req));
  req[0] = 0x42;
  req[1] = 0x01;

  RIE (gt68xx_device_req (dev, req, req));
  RIE (gt68xx_device_check_result (req, 0x42));
  return SANE_STATUS_GOOD;
}

SANE_Status
gt6801_setup_scan (GT68xx_Device * dev,
		   GT68xx_Scan_Request * request,
		   GT68xx_Scan_Action action, GT68xx_Scan_Parameters * params)
{
  DECLARE_FUNCTION_NAME ("gt6801_setup_scan") SANE_Status status;
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

  XDBG ((5, "%s: enter (action=%s)\n", function_name,
	 action == SA_CALIBRATE ? "calibrate" :
	 action == SA_CALIBRATE_ONE_LINE ? "calibrate one line" :
	 action == SA_SCAN ? "scan" : "calculate only"));

  model = dev->model;

  xdpi = request->xdpi;
  ydpi = request->ydpi;
  color = request->color;
  depth = request->depth;
  backtrack = 0;

  base_xdpi = model->base_xdpi;
  base_ydpi = model->base_ydpi;

#if 0
  if (xdpi > model->base_xdpi)
    base_xdpi = model->optical_xdpi;
#endif
  if (!model->constant_ydpi)
    {
      if (ydpi > model->base_ydpi)
	base_ydpi = model->optical_ydpi;
    }

  XDBG ((5, "%s: base_xdpi=%d, base_ydpi=%d\n", function_name,
	 base_xdpi, base_ydpi));

  switch (action)
    {
      /* Warning: Support for TA not implemented */
    case SA_CALIBRATE_ONE_LINE:
      {
	x0 = request->x0;
	y0 = model->y_offset_calib;
	ys = SANE_FIX (1.0 * MM_PER_INCH / ydpi);	/* 1 calibration line */
	xs = request->xs;
	depth = 8;
	color = SANE_TRUE;
	break;
      }
    case SA_CALIBRATE:
      {
	if (dev->model->flags & GT68XX_FLAG_MIRROR_X)
	  x0 = request->x0 - model->x_offset;
	else
	  x0 = request->x0 + model->x_offset;
	if (request->mbs)
	  y0 = model->y_offset_calib;
	else
	  y0 = 0;
	ys = SANE_FIX (CALIBRATION_HEIGHT);
	xs = request->xs;
	color = request->color;
	break;
      }
    case SA_SCAN:
      {
	request->mbs = SANE_TRUE;
	if (dev->model->flags & GT68XX_FLAG_MIRROR_X)
	  x0 = request->x0 - model->x_offset;
	else
	  x0 = request->x0 + model->x_offset;
	y0 = request->y0 + model->y_offset;
	ys = request->ys;
	xs = request->xs;
	backtrack = SANE_TRUE;
	break;
      }

    default:
      XDBG ((3, "%s: invalid action=%d\n", function_name, (int) action));
      return SANE_STATUS_INVAL;
    }

  pixel_x0 = SANE_UNFIX (x0) * xdpi / MM_PER_INCH + 0.5;
  pixel_y0 = SANE_UNFIX (y0) * ydpi / MM_PER_INCH + 0.5;
  pixel_ys = SANE_UNFIX (ys) * ydpi / MM_PER_INCH + 0.5;
  pixel_xs = SANE_UNFIX (xs) * xdpi / MM_PER_INCH + 0.5;

  XDBG ((5, "%s: xdpi=%d, ydpi=%d\n", function_name, xdpi, ydpi));
  XDBG ((5, "%s: color=%s, depth=%d\n", function_name,
	 color ? "TRUE" : "FALSE", depth));
  XDBG ((5, "%s: pixel_x0=%d, pixel_y0=%d\n", function_name,
	 pixel_x0, pixel_y0));
  XDBG ((5, "%s: pixel_xs=%d, pixel_ys=%d\n", function_name,
	 pixel_xs, pixel_ys));
  XDBG ((5, "%s: backtrack=%d\n", function_name, backtrack));

  color_mode_code = 0x80;	/* What does this mean ? */
  if (color)
    color_mode_code |= (1 << 2);
  else
    color_mode_code |= (1 << 1);

  if (depth > 12)
    color_mode_code |= (1 << 5);
  else if (depth > 8)
    {
      color_mode_code &= 0x7f;
      color_mode_code |= (1 << 4);
    }
  XDBG ((5, "%s: color_mode_code = 0x%02X\n", function_name,
	 color_mode_code));

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

      XDBG ((5, "%s: overscan=%d, ld=%d/%d/%d\n", function_name,
	     overscan_lines, params->ld_shift_r, params->ld_shift_g,
	     params->ld_shift_b));
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
      XDBG ((5, "%s: overscan=%d, ld double=%d\n", function_name,
	     overscan_lines, params->ld_shift_double));
    }


  XDBG ((5, "%s: base_xdpi=%d, base_ydpi=%d\n", function_name,
	 base_xdpi, base_ydpi));

  abs_x0 = pixel_x0 * base_xdpi / xdpi;
  abs_y0 = pixel_y0 * base_ydpi / ydpi;
  XDBG ((5, "%s: abs_x0=%d, abs_y0=%d\n", function_name, abs_x0, abs_y0));

  params->double_column = abs_x0 & 1;

  /* Calculate minimum number of pixels which span an integral multiple of 64
   * bytes. pixels must be reduced by 2 of this value if xdpi != base_dpi. */
  pixel_align = 32;		/* best case for depth = 16 */
  while ((depth * pixel_align) % (64 * 8) != 0)
    pixel_align *= 2;
  XDBG ((5, "%s: pixel_align=%d\n", function_name, pixel_align));

  if (pixel_xs % pixel_align == 0)
    scan_xs = pixel_xs;
  else
    scan_xs = (pixel_xs / pixel_align + 1) * pixel_align;
  scan_ys = pixel_ys + overscan_lines;

  bits_per_line = depth * scan_xs;

  if (xdpi != base_xdpi)
    {
      if (dev->model->flags & GT68XX_FLAG_SE_2400)
	abs_xs = (scan_xs - 1) * base_xdpi / xdpi;
      else
	abs_xs = (scan_xs - 2) * base_xdpi / xdpi;
    }
  else
    abs_xs = scan_xs * base_xdpi / xdpi;

  if (action == SA_CALIBRATE_ONE_LINE)
    abs_ys = 2;
  else
    abs_ys = scan_ys * base_ydpi / ydpi;
  XDBG ((5, "%s: abs_xs=%d, abs_ys=%d\n", function_name, abs_xs, abs_ys));

  if (model->is_cis)
    {
      line_mode = SANE_TRUE;
      XDBG ((5, "%s: using line mode (CIS)\n", function_name));
    }
  else
    {
      line_mode = SANE_FALSE;
      if (!color)
	{
	  XDBG ((5, "%s: using line mode for monochrome scan\n",
		 function_name));
	  line_mode = SANE_TRUE;
	}
      else if (ydpi >= model->ydpi_force_line_mode)
	{
	  XDBG ((5, "%s: forcing line mode for ydpi=%d\n", function_name,
		 ydpi));
	  line_mode = SANE_TRUE;
	}
      else if (ydpi == 600 && depth == 16)	/* XXX */
	{
	  XDBG ((5, "%s: forcing line mode for ydpi=%d, depth=%d\n",
		 function_name, ydpi, depth));
	  line_mode = SANE_TRUE;
	}
    }

  if (color)
    if (line_mode || dev->model->flags & GT68XX_FLAG_SE_2400) /* ??? */
      scan_ys *= 3;


  XDBG ((5, "%s: scan_xs=%d, scan_ys=%d\n", function_name, scan_xs, scan_ys));

  if (color && !line_mode)
    bits_per_line *= 3;
  if (bits_per_line % 8)	/* impossible */
    {
      XDBG ((0, "%s: BUG: unaligned bits_per_line=%d\n", function_name,
	     bits_per_line));
      return SANE_STATUS_INVAL;
    }
  scan_bpl = bits_per_line / 8;

  if (scan_bpl > 15600 && !line_mode)
    {
      XDBG ((5, "%s: scan_bpl=%d, trying line mode\n", function_name,
	     scan_bpl));
      line_mode = SANE_TRUE;
      if (scan_bpl % 3)
	{
	  XDBG ((0, "%s: BUG: monochrome scan in pixel mode?\n",
		 function_name));
	  return SANE_STATUS_INVAL;
	}
      scan_bpl /= 3;
    }

  if (scan_bpl % 64)		/* impossible */
    {
      XDBG ((0, "%s: BUG: unaligned scan_bpl=%d\n", function_name, scan_bpl));
      return SANE_STATUS_INVAL;
    }

  if (scan_bpl > 15600)
    {
      XDBG ((3, "%s: scan_bpl=%d, too large\n", function_name, scan_bpl));
      return SANE_STATUS_INVAL;
    }

  XDBG ((5, "%s: scan_bpl=%d\n", function_name, scan_bpl));


  if (!request->calculate)
    {
      GT68xx_Packet req;
      SANE_Byte motor_mode_1, motor_mode_2;

      motor_mode_1 = (request->mbs ? 0 : 1) << 1;
      motor_mode_1 |= (request->mds ? 0 : 1) << 2;
      motor_mode_1 |= (request->mas ? 0 : 1) << 0;
      motor_mode_1 |= (backtrack ? 1 : 0) << 3;

      motor_mode_2 = (request->lamp ? 0 : 1) << 0;
      motor_mode_2 |= (line_mode ? 0 : 1) << 2;
      XDBG ((5, "%s: motor_mode_1 = 0x%02X, motor_mode_2 = 0x%02X\n",
	     function_name, motor_mode_1, motor_mode_2));

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
      req[0x0b] = 0x60;		/* CIS/CCD? */
      req[0x0c] = LOBYTE (xdpi);
      req[0x0d] = HIBYTE (xdpi);
      req[0x0e] = (action == SA_SCAN) ? 0x3c : 0xf0;	/* ???? */
      /* hmg: doesn't seem to matter and varies for SA_SCAN */
      req[0x0f] = 0x00;
      req[0x10] = LOBYTE (scan_bpl);
      req[0x11] = HIBYTE (scan_bpl);
      req[0x12] = LOBYTE (scan_ys);
      req[0x13] = HIBYTE (scan_ys);
      req[0x14] = motor_mode_1;
      req[0x15] = motor_mode_2;
      req[0x16] = LOBYTE (ydpi);
      req[0x17] = HIBYTE (ydpi);
      if (backtrack)
	{
	  if (model->is_cis)
	    req[0x18] = 0x20;
	  else
	    req[0x18] = 0x3f;
	}
      else
	req[0x18] = 0x00;

      status = gt68xx_device_req (dev, req, req);
      if (status != SANE_STATUS_GOOD)
	{
	  XDBG ((3, "%s: setup request failed: %s\n", function_name,
		 sane_strstatus (status)));
	  return status;
	}
      RIE (gt68xx_device_check_result (req, 0x20));
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

  XDBG ((6, "%s: leave: ok\n", function_name));
  return SANE_STATUS_GOOD;
}

/* vim: set sw=2 cino=>2se-1sn-1s{s^-1st0(0u0 smarttab expandtab: */
