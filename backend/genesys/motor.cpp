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

#include "motor.h"
#include "utilities.h"

namespace genesys {

std::ostream& operator<<(std::ostream& out, const Genesys_Motor_Slope& slope)
{
    out << "Genesys_Motor_Slope{\n"
        << "    maximum_start_speed: " << slope.maximum_start_speed << '\n'
        << "    maximum_speed: " << slope.maximum_speed << '\n'
        << "    minimum_steps: " << slope.minimum_steps << '\n'
        << "    g: " << slope.g << '\n'
        << '}';
    return out;
}

std::ostream& operator<<(std::ostream& out, const Genesys_Motor& motor)
{
    out << "Genesys_Motor{\n"
        << "    id: " << static_cast<unsigned>(motor.id) << '\n'
        << "    base_ydpi: " << motor.base_ydpi << '\n'
        << "    optical_ydpi: " << motor.optical_ydpi << '\n'
        << "    slopes: "
        << format_indent_braced_list(4, format_vector_indent_braced(4, "Genesys_Motor_Slope",
                                                                    motor.slopes))
        << '}';
    return out;
}

} // namespace genesys
