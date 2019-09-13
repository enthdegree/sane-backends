/* sane - Scanner Access Now Easy.

   Copyright (C) 2005 Pierre Willenbrock <pierre@pirsoft.dnsalias.org>

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
 * Conversion filters for genesys backend
 */

static void FUNC_NAME(genesys_shrink_lines) (uint8_t* src_data, uint8_t* dst_data,
                                             unsigned int lines,
                                             unsigned int src_pixels, unsigned int dst_pixels,
                                             unsigned int channels)
{
    DBG_HELPER(dbg);
    unsigned int dst_x, src_x, y, c, cnt;
    unsigned int avg[3];
    unsigned int count;
    COMPONENT_TYPE *src = (COMPONENT_TYPE *)src_data;
    COMPONENT_TYPE *dst = (COMPONENT_TYPE *)dst_data;

    if (src_pixels > dst_pixels) {
/*average*/
	for (c = 0; c < channels; c++)
	    avg[c] = 0;
	for(y = 0; y < lines; y++) {
	    cnt = src_pixels / 2;
	    src_x = 0;
	    for (dst_x = 0; dst_x < dst_pixels; dst_x++) {
		count = 0;
		while (cnt < src_pixels && src_x < src_pixels) {
		    cnt += dst_pixels;

		    for (c = 0; c < channels; c++)
			avg[c] += *src++;
		    src_x++;
		    count++;
		}
		cnt -= src_pixels;

		for (c = 0; c < channels; c++) {
		    *dst++ = avg[c] / count;
		    avg[c] = 0;
		}
	    }
	}
    } else {
/*interpolate. copy pixels*/
	for(y = 0; y < lines; y++) {
	    cnt = dst_pixels / 2;
	    dst_x = 0;
	    for (src_x = 0; src_x < src_pixels; src_x++) {
		for (c = 0; c < channels; c++)
		    avg[c] = *src++;
		while ((cnt < dst_pixels || src_x + 1 == src_pixels) &&
		       dst_x < dst_pixels) {
		    cnt += src_pixels;

		    for (c = 0; c < channels; c++)
			*dst++ = avg[c];
		    dst_x++;
		}
		cnt -= dst_pixels;
	    }
	}
    }
}
