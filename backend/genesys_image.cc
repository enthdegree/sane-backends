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

#define DEBUG_DECLARE_ONLY

#include "genesys_image.h"

#include <array>

Image::Image() = default;

Image::Image(std::size_t width, std::size_t height, PixelFormat format) :
    width_{width},
    height_{height},
    format_{format},
    row_bytes_{get_pixel_row_bytes(format_, width_)}
{
    data_.resize(get_row_bytes() * height);
}

std::uint8_t* Image::get_row_ptr(std::size_t y)
{
    return data_.data() + row_bytes_ * y;
}

const std::uint8_t* Image::get_row_ptr(std::size_t y) const
{
    return data_.data() + row_bytes_ * y;
}

Pixel Image::get_pixel(std::size_t x, std::size_t y) const
{
    return get_pixel_from_row(get_row_ptr(y), x, format_);
}

void Image::set_pixel(std::size_t x, std::size_t y, const Pixel& pixel)
{
    set_pixel_to_row(get_row_ptr(y), x, pixel, format_);
}

RawPixel Image::get_raw_pixel(std::size_t x, std::size_t y) const
{
    return get_raw_pixel_from_row(get_row_ptr(y), x, format_);
}

void Image::set_raw_pixel(std::size_t x, std::size_t y, const RawPixel& pixel)
{
    set_raw_pixel_to_row(get_row_ptr(y), x, pixel, format_);
}

template<PixelFormat SrcFormat, PixelFormat DstFormat>
void convert_pixel_row_impl2(const std::uint8_t* in_data, std::uint8_t* out_data,
                             std::size_t count)
{
    for (std::size_t i = 0; i < count; ++i) {
        Pixel pixel = get_pixel_from_row(in_data, i, SrcFormat);
        set_pixel_to_row(out_data, i, pixel, DstFormat);
    }
}

template<PixelFormat SrcFormat>
void convert_pixel_row_impl(const std::uint8_t* in_data, std::uint8_t* out_data,
                            PixelFormat out_format, std::size_t count)
{
    switch (out_format) {
        case PixelFormat::I1: {
            convert_pixel_row_impl2<SrcFormat, PixelFormat::I1>(in_data, out_data, count);
            return;
        }
        case PixelFormat::RGB111: {
            convert_pixel_row_impl2<SrcFormat, PixelFormat::RGB111>(in_data, out_data, count);
            return;
        }
        case PixelFormat::I8: {
            convert_pixel_row_impl2<SrcFormat, PixelFormat::I8>(in_data, out_data, count);
            return;
        }
        case PixelFormat::RGB888: {
            convert_pixel_row_impl2<SrcFormat, PixelFormat::RGB888>(in_data, out_data, count);
            return;
        }
        case PixelFormat::BGR888: {
            convert_pixel_row_impl2<SrcFormat, PixelFormat::BGR888>(in_data, out_data, count);
            return;
        }
        case PixelFormat::I16: {
            convert_pixel_row_impl2<SrcFormat, PixelFormat::I16>(in_data, out_data, count);
            return;
        }
        case PixelFormat::RGB161616: {
            convert_pixel_row_impl2<SrcFormat, PixelFormat::RGB161616>(in_data, out_data, count);
            return;
        }
        case PixelFormat::BGR161616: {
            convert_pixel_row_impl2<SrcFormat, PixelFormat::BGR161616>(in_data, out_data, count);
            return;
        }
        default:
            throw SaneException("Unknown pixel format %d", static_cast<unsigned>(out_format));
    }
}
void convert_pixel_row_format(const std::uint8_t* in_data, PixelFormat in_format,
                              std::uint8_t* out_data, PixelFormat out_format, std::size_t count)
{
    if (in_format == out_format) {
        std::memcpy(out_data, in_data, get_pixel_row_bytes(in_format, count));
        return;
    }

    switch (in_format) {
        case PixelFormat::I1: {
            convert_pixel_row_impl<PixelFormat::I1>(in_data, out_data, out_format, count);
            return;
        }
        case PixelFormat::RGB111: {
            convert_pixel_row_impl<PixelFormat::RGB111>(in_data, out_data, out_format, count);
            return;
        }
        case PixelFormat::I8: {
            convert_pixel_row_impl<PixelFormat::I8>(in_data, out_data, out_format, count);
            return;
        }
        case PixelFormat::RGB888: {
            convert_pixel_row_impl<PixelFormat::RGB888>(in_data, out_data, out_format, count);
            return;
        }
        case PixelFormat::BGR888: {
            convert_pixel_row_impl<PixelFormat::BGR888>(in_data, out_data, out_format, count);
            return;
        }
        case PixelFormat::I16: {
            convert_pixel_row_impl<PixelFormat::I16>(in_data, out_data, out_format, count);
            return;
        }
        case PixelFormat::RGB161616: {
            convert_pixel_row_impl<PixelFormat::RGB161616>(in_data, out_data, out_format, count);
            return;
        }
        case PixelFormat::BGR161616: {
            convert_pixel_row_impl<PixelFormat::BGR161616>(in_data, out_data, out_format, count);
            return;
        }
        default:
            throw SaneException("Unknown pixel format %d", static_cast<unsigned>(in_format));
    }
}
