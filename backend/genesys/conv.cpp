/* sane - Scanner Access Now Easy.

   Copyright (C) 2005, 2006 Pierre Willenbrock <pierre@pirsoft.dnsalias.org>
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

#include "conv.h"

namespace genesys {

/**
 * uses the threshold/threshold_curve to control software binarization
 * This code was taken from the epjistsu backend by m. allan noah
 * @param dev device set up for the scan
 * @param src pointer to raw data
 * @param dst pointer where to store result
 * @param width width of the processed line
 * */
void binarize_line(Genesys_Device* dev, std::uint8_t* src, std::uint8_t* dst, int width)
{
    DBG_HELPER(dbg);
  int j, windowX, sum = 0;
  int thresh;
  int offset, addCol, dropCol;
  unsigned char mask;

  int x;
    std::uint8_t min, max;

  /* normalize line */
  min = 255;
  max = 0;
    for (x = 0; x < width; x++)
      {
	if (src[x] > max)
	  {
	    max = src[x];
	  }
	if (src[x] < min)
	  {
	    min = src[x];
	  }
      }

    /* safeguard against dark or white areas */
    if(min>80)
	    min=0;
    if(max<80)
	    max=255;
    for (x = 0; x < width; x++)
      {
	src[x] = ((src[x] - min) * 255) / (max - min);
      }

  /* ~1mm works best, but the window needs to have odd # of pixels */
  windowX = (6 * dev->settings.xres) / 150;
  if (!(windowX % 2))
    windowX++;

  /* second, prefill the sliding sum */
  for (j = 0; j < windowX; j++)
    sum += src[j];

  /* third, walk the input buffer, update the sliding sum, */
  /* determine threshold, output bits */
  for (j = 0; j < width; j++)
    {
      /* output image location */
      offset = j % 8;
      mask = 0x80 >> offset;
      thresh = dev->settings.threshold;

      /* move sum/update threshold only if there is a curve */
      if (dev->settings.threshold_curve)
	{
	  addCol = j + windowX / 2;
	  dropCol = addCol - windowX;

	  if (dropCol >= 0 && addCol < width)
	    {
	      sum -= src[dropCol];
	      sum += src[addCol];
	    }
	  thresh = dev->lineart_lut[sum / windowX];
	}

      /* use average to lookup threshold */
      if (src[j] > thresh)
	*dst &= ~mask;		/* white */
      else
	*dst |= mask;		/* black */

      if (offset == 7)
	dst++;
    }
}

/**
 * software lineart using data from a 8 bit gray scan. We assume true gray
 * or monochrome scan as input.
 */
void genesys_gray_lineart(Genesys_Device* dev,
                          std::uint8_t* src_data, std::uint8_t* dst_data,
                          std::size_t pixels, std::size_t lines, std::uint8_t threshold)
{
    DBG_HELPER(dbg);
    std::size_t y;
    (void) threshold;

  for (y = 0; y < lines; y++)
    {
      binarize_line (dev, src_data + y * pixels, dst_data, pixels);
      dst_data += pixels / 8;
    }
}

} // namespace genesys
