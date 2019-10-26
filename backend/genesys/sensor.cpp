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

#include "sensor.h"
#include "utilities.h"
#include <iomanip>

namespace genesys {

std::ostream& operator<<(std::ostream& out, const Genesys_Sensor& sensor)
{
    out << "Genesys_Sensor{\n"
        << "    sensor_id: " << static_cast<unsigned>(sensor.sensor_id) << '\n'
        << "    optical_res: " << sensor.optical_res << '\n';
    if (sensor.resolutions.matches_any()) {
        out << "    resolutions: ANY\n";
    } else {
        out << "    resolutions: {\n";
        for (unsigned resolution : sensor.resolutions.resolutions()) {
            out << "        " << resolution << ",\n";
        }
        out << "    }\n";
    }
    out << "    method: " << sensor.method << '\n'
        << "    ccd_size_divisor: " << sensor.ccd_size_divisor << '\n'
        << "    black_pixels: " << sensor.black_pixels << '\n'
        << "    dummy_pixel: " << sensor.dummy_pixel << '\n'
        << "    ccd_start_xoffset: " << sensor.ccd_start_xoffset << '\n'
        << "    sensor_pixels: " << sensor.sensor_pixels << '\n'
        << "    fau_gain_white_ref: " << sensor.fau_gain_white_ref << '\n'
        << "    gain_white_ref: " << sensor.gain_white_ref << '\n'
        << "    exposure.red: " << sensor.exposure.red << '\n'
        << "    exposure.green: " << sensor.exposure.green << '\n'
        << "    exposure.blue: " << sensor.exposure.blue << '\n'
        << "    exposure_lperiod: " << sensor.exposure_lperiod << '\n'
        << "    custom_regs: " << format_indent_braced_list(4, sensor.custom_regs) << '\n'
        << "    custom_fe_regs: " << format_indent_braced_list(4, sensor.custom_fe_regs) << '\n'
        << "    gamma.red: " << sensor.gamma[0] << '\n'
        << "    gamma.green: " << sensor.gamma[1] << '\n'
        << "    gamma.blue: " << sensor.gamma[2] << '\n'
        << "}";
    return out;
}

} // namespace genesys
