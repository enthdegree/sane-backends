/* sane - Scanner Access Now Easy.

   Copyright (C) 2002 Sergey Vlasov <vsu@altlinux.ru>
   
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
 * @brief GT68xx commands common for most GT68xx-based scanners.
 */

#include "gt68xx_generic.h"


SANE_Status
gt68xx_generic_move_relative (GT68xx_Device * dev, SANE_Int distance)
{
  GT68xx_Packet req;

  memset (req, 0, sizeof (req));
  if (distance >= 0)
    req[0] = 0x14;
  else
    {
      req[0] = 0x15;
      distance = -distance;
    }
  req[1] = 0x01;
  req[2] = LOBYTE (distance);
  req[3] = HIBYTE (distance);

  return gt68xx_device_req (dev, req, req);
}

SANE_Status
gt68xx_generic_start_scan (GT68xx_Device * dev)
{
  GT68xx_Packet req;
  SANE_Status status;

  memset (req, 0, sizeof (req));
  req[0] = 0x43;
  req[1] = 0x01;
  RIE (gt68xx_device_req (dev, req, req));
  RIE (gt68xx_device_check_result (req, 0x43));

  return SANE_STATUS_GOOD;
}

SANE_Status
gt68xx_generic_read_scanned_data (GT68xx_Device * dev, SANE_Bool * ready)
{
  SANE_Status status;
  GT68xx_Packet req;

  memset (req, 0, sizeof (req));
  req[0] = 0x35;
  req[1] = 0x01;

  RIE (gt68xx_device_req (dev, req, req));

  if (req[0] == 0)
    *ready = SANE_TRUE;
  else
    *ready = SANE_FALSE;

  return SANE_STATUS_GOOD;
}

static SANE_Byte
gt68xx_generic_fix_gain (SANE_Int gain)
{
  if (gain < 0)
    gain = 0;
  else if (gain > 31)
    {
      gain = gain / 2 + 32;
      if (gain > 63)
	gain = 63;
    }
  return gain;
}

static SANE_Byte
gt68xx_generic_fix_offset (SANE_Int offset)
{
  if (offset < 0)
    offset = 0;
  else if (offset > 63)
    offset = 63;
  return offset;
}

SANE_Status
gt68xx_generic_set_afe (GT68xx_Device * dev, GT68xx_AFE_Parameters * params)
{
  GT68xx_Packet req;

  memset (req, 0, sizeof (req));
  req[0] = 0x22;
  req[1] = 0x01;
  req[2] = gt68xx_generic_fix_offset (params->r_offset);
  req[3] = gt68xx_generic_fix_gain (params->r_pga);
  req[4] = gt68xx_generic_fix_offset (params->g_offset);
  req[5] = gt68xx_generic_fix_gain (params->g_pga);
  req[6] = gt68xx_generic_fix_offset (params->b_offset);
  req[7] = gt68xx_generic_fix_gain (params->b_pga);

  DBG (6, "gt68xx_generic_set_afe: real AFE: 0x%02x 0x%02x  0x%02x 0x%02x  0x%02x 0x%02x\n",
       req [2], req [3], req [4], req [5], req [6], req [7]);
  return gt68xx_device_req (dev, req, req);
}

SANE_Status
gt68xx_generic_set_exposure_time (GT68xx_Device * dev,
				  GT68xx_Exposure_Parameters * params)
{
  GT68xx_Packet req;
  SANE_Status status;

  memset (req, 0, sizeof (req));
  req[0] = 0x76;
  req[1] = 0x01;
  req[2] = req[6] = req[10] = 0x04;
  req[4] = LOBYTE (params->r_time);
  req[5] = HIBYTE (params->r_time);
  req[8] = LOBYTE (params->g_time);
  req[9] = HIBYTE (params->g_time);
  req[12] = LOBYTE (params->b_time);
  req[13] = HIBYTE (params->b_time);

  DBG (6, "gt68xx_generic_set_exposure_time: 0x%03x 0x%03x 0x%03x\n",
       params->r_time, params->g_time, params->b_time);

  RIE (gt68xx_device_req (dev, req, req));
  RIE (gt68xx_device_check_result (req, 0x76));
  return SANE_STATUS_GOOD;
}

SANE_Status
gt68xx_generic_get_id (GT68xx_Device * dev)
{
  GT68xx_Packet req;
  SANE_Status status;

  memset (req, 0, sizeof (req));
  req[0] = 0x2e;
  req[1] = 0x01;
  RIE (gt68xx_device_req (dev, req, req));
  RIE (gt68xx_device_check_result (req, 0x2e));

  DBG (2,
       "get_id: vendor id=0x%04X, product id=0x%04X, DID=0x%08X, FID=0x%04X\n",
       req[2] + (req[3] << 8), req[4] + (req[5] << 8),
       req[6] + (req[7] << 8) + (req[8] << 16) + (req[9] << 24),
       req[10] + (req[11] << 8));
  return SANE_STATUS_GOOD;
}

/* vim: set sw=2 cino=>2se-1sn-1s{s^-1st0(0u0 smarttab expandtab: */
