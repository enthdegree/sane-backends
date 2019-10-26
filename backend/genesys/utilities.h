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

#ifndef BACKEND_GENESYS_UTILITIES_H
#define BACKEND_GENESYS_UTILITIES_H

#include "error.h"
#include <algorithm>
#include <iostream>
#include <vector>

namespace genesys {

template<class T>
void compute_array_percentile_approx(T* result, const T* data,
                                     std::size_t line_count, std::size_t elements_per_line,
                                     float percentile)
{
    if (line_count == 0) {
        throw SaneException("invalid line count");
    }

    if (line_count == 1) {
        std::copy(data, data + elements_per_line, result);
        return;
    }

    std::vector<T> column_elems;
    column_elems.resize(line_count, 0);

    std::size_t select_elem = std::min(static_cast<std::size_t>(line_count * percentile),
                                       line_count - 1);

    auto select_it = column_elems.begin() + select_elem;

    for (std::size_t ix = 0; ix < elements_per_line; ++ix) {
        for (std::size_t iy = 0; iy < line_count; ++iy) {
            column_elems[iy] = data[iy * elements_per_line + ix];
        }

        std::nth_element(column_elems.begin(), select_it, column_elems.end());

        *result++ = *select_it;
    }
}

template<class Char, class Traits>
class BasicStreamStateSaver
{
public:
    explicit BasicStreamStateSaver(std::basic_ios<Char, Traits>& stream) :
        stream_{stream}
    {
        flags_ = stream_.flags();
        width_ = stream_.width();
        precision_ = stream_.precision();
        fill_ = stream_.fill();
    }

    ~BasicStreamStateSaver()
    {
        stream_.flags(flags_);
        stream_.width(width_);
        stream_.precision(precision_);
        stream_.fill(fill_);
    }

    BasicStreamStateSaver(const BasicStreamStateSaver&) = delete;
    BasicStreamStateSaver& operator=(const BasicStreamStateSaver&) = delete;

private:
    std::basic_ios<Char, Traits>& stream_;
    std::ios_base::fmtflags flags_;
    std::streamsize width_ = 0;
    std::streamsize precision_ = 0;
    Char fill_ = ' ';
};

using StreamStateSaver = BasicStreamStateSaver<char, std::char_traits<char>>;

} // namespace genesys

#endif // BACKEND_GENESYS_UTILITIES_H
