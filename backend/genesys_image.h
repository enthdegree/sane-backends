/* sane - Scanner Access Now Easy.

   Copyright (C) 2019 Povilas Kanapickas <povilas@radix.lt>

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

#ifndef BACKEND_GENESYS_IMAGE_H
#define BACKEND_GENESYS_IMAGE_H

#include "genesys_enums.h"
#include "genesys_error.h"
#include "genesys_image_buffer.h"

#include <algorithm>
#include <memory>

ColorOrder get_pixel_format_color_order(PixelFormat format);
unsigned get_pixel_format_depth(PixelFormat format);
unsigned get_pixel_channels(PixelFormat format);
std::size_t get_pixel_row_bytes(PixelFormat format, std::size_t width);

std::size_t get_pixels_from_row_bytes(PixelFormat format, std::size_t row_bytes);

PixelFormat create_pixel_format(unsigned depth, unsigned channels, ColorOrder order);

// retrieves or sets the logical pixel values in 16-bit range.
Pixel get_pixel_from_row(const std::uint8_t* data, std::size_t x, PixelFormat format);
void set_pixel_to_row(std::uint8_t* data, std::size_t x, Pixel pixel, PixelFormat format);

// retrieves or sets the physical pixel values. The low bytes of the RawPixel are interpreted as
// the retrieved values / values to set
RawPixel get_raw_pixel_from_row(const std::uint8_t* data, std::size_t x, PixelFormat format);
void set_raw_pixel_to_row(std::uint8_t* data, std::size_t x, RawPixel pixel, PixelFormat format);

// retrieves or sets the physical value of specific channel of the pixel. The channels are numbered
// in the same order as the pixel is laid out in memory, that is, whichever channel comes first
// has the index 0. E.g. 0-th channel in RGB888 is the red byte, but in BGR888 is the blue byte.
std::uint16_t get_raw_channel_from_row(const std::uint8_t* data, std::size_t x, unsigned channel,
                                       PixelFormat format);
void set_raw_channel_to_row(std::uint8_t* data, std::size_t x, unsigned channel, std::uint16_t pixel,
                            PixelFormat format);

void convert_pixel_row_format(const std::uint8_t* in_data, PixelFormat in_format,
                              std::uint8_t* out_data, PixelFormat out_format, std::size_t count);

template<PixelFormat Format>
Pixel get_pixel_from_row(const std::uint8_t* data, std::size_t x);
template<PixelFormat Format>
void set_pixel_to_row(std::uint8_t* data, std::size_t x, RawPixel pixel);

template<PixelFormat Format>
Pixel get_raw_pixel_from_row(const std::uint8_t* data, std::size_t x);
template<PixelFormat Format>
void set_raw_pixel_to_row(std::uint8_t* data, std::size_t x, RawPixel pixel);

template<PixelFormat Format>
std::uint16_t get_raw_channel_from_row(const std::uint8_t* data, std::size_t x, unsigned channel);
template<PixelFormat Format>
void set_raw_channel_to_row(std::uint8_t* data, std::size_t x, unsigned channel,
                            std::uint16_t pixel);


#endif // ifndef BACKEND_GENESYS_IMAGE_H
