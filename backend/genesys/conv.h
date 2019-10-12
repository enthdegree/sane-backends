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

#ifndef BACKEND_GENESYS_CONV_H
#define BACKEND_GENESYS_CONV_H

#include "device.h"
#include "sensor.h"
#include "genesys.h"

void genesys_reverse_bits(uint8_t* src_data, uint8_t* dst_data, size_t bytes);

void binarize_line(Genesys_Device* dev, std::uint8_t* src, std::uint8_t* dst, int width);

void genesys_gray_lineart(Genesys_Device* dev,
                          std::uint8_t* src_data, std::uint8_t* dst_data,
                          std::size_t pixels, size_t lines, std::uint8_t threshold);

void genesys_crop(Genesys_Scanner* s);

void genesys_deskew(Genesys_Scanner *s, const Genesys_Sensor& sensor);

void genesys_despeck(Genesys_Scanner* s);

void genesys_derotate(Genesys_Scanner* s);

#endif // BACKEND_GENESYS_CONV_H
