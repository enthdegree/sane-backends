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
*/

#ifndef SANE_TESTSUITE_BACKEND_GENESYS_TESTS_PRINTERS_H
#define SANE_TESTSUITE_BACKEND_GENESYS_TESTS_PRINTERS_H

#include "../../../backend/genesys/image_pixel.h"
#include <iostream>
#include <iomanip>
#include <vector>

template<class T>
std::ostream& operator<<(std::ostream& str, const std::vector<T>& arg)
{
    str << "{ ";
    for (const auto& el : arg) {
        str << static_cast<unsigned>(el) << ", ";
    }
    str << "}\n";
    return str;
}

inline std::ostream& operator<<(std::ostream& str, const PixelFormat& arg)
{
    str << static_cast<unsigned>(arg);
    return str;
}

inline std::ostream& operator<<(std::ostream& str, const Pixel& arg)
{
    str << "{ " << arg.r << ", " << arg.g << ", " << arg.b << " }";
    return str;
}

inline std::ostream& operator<<(std::ostream& str, const RawPixel& arg)
{
    auto flags = str.flags();
    str << std::hex;
    for (auto el : arg.data) {
        str << static_cast<unsigned>(el) << " ";
    }
    str.flags(flags);
    return str;
}

#endif
