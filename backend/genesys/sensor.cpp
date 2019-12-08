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

std::ostream& operator<<(std::ostream& out, const FrontendType& type)
{
    switch (type) {
        case FrontendType::UNKNOWN: out << "UNKNOWN"; break;
        case FrontendType::WOLFSON: out << "WOLFSON"; break;
        case FrontendType::ANALOG_DEVICES: out << "ANALOG_DEVICES"; break;
        default: out << "(unknown value)";
    }
    return out;
}

std::ostream& operator<<(std::ostream& out, const GenesysFrontendLayout& layout)
{
    StreamStateSaver state_saver{out};

    out << "GenesysFrontendLayout{\n"
        << "    type: " << layout.type << '\n'
        << std::hex
        << "    offset_addr[0]: " << layout.offset_addr[0] << '\n'
        << "    offset_addr[1]: " << layout.offset_addr[1] << '\n'
        << "    offset_addr[2]: " << layout.offset_addr[2] << '\n'
        << "    gain_addr[0]: " << layout.gain_addr[0] << '\n'
        << "    gain_addr[1]: " << layout.gain_addr[1] << '\n'
        << "    gain_addr[2]: " << layout.gain_addr[2] << '\n'
        << '}';
    return out;
}

std::ostream& operator<<(std::ostream& out, const Genesys_Frontend& frontend)
{
    StreamStateSaver state_saver{out};

    out << "Genesys_Frontend{\n"
        << "    id: " << static_cast<unsigned>(frontend.id) << '\n'
        << "    regs: " << format_indent_braced_list(4, frontend.regs) << '\n'
        << std::hex
        << "    reg2[0]: " << frontend.reg2[0] << '\n'
        << "    reg2[1]: " << frontend.reg2[1] << '\n'
        << "    reg2[2]: " << frontend.reg2[2] << '\n'
        << "    layout: " << format_indent_braced_list(4, frontend.layout) << '\n'
        << '}';
    return out;
}

std::ostream& operator<<(std::ostream& out, const SensorExposure& exposure)
{
    out << "SensorExposure{\n"
        << "    red: " << exposure.red << '\n'
        << "    green: " << exposure.green << '\n'
        << "    blue: " << exposure.blue << '\n'
        << '}';
    return out;
}

std::ostream& operator<<(std::ostream& out, const ResolutionFilter& resolutions)
{
    if (resolutions.matches_any()) {
        out << "ANY";
        return out;
    }
    out << format_vector_unsigned(4, resolutions.resolutions());
    return out;
}

std::ostream& operator<<(std::ostream& out, const SensorProfile& profile)
{
    out << "SensorProfile{\n"
        << "    resolutions: " << format_indent_braced_list(4, profile.resolutions) << '\n'
        << "    exposure_lperiod: " << profile.exposure_lperiod << '\n'
        << "    exposure: " << format_indent_braced_list(4, profile.exposure) << '\n'
        << "    segment_size: " << profile.segment_size << '\n'
        << "    segment_order: "
        << format_indent_braced_list(4, format_vector_unsigned(4, profile.segment_order)) << '\n'
        << "    custom_regs: " << format_indent_braced_list(4, profile.custom_regs) << '\n'
        << '}';
    return out;
}

std::ostream& operator<<(std::ostream& out, const Genesys_Sensor& sensor)
{
    out << "Genesys_Sensor{\n"
        << "    sensor_id: " << static_cast<unsigned>(sensor.sensor_id) << '\n'
        << "    optical_res: " << sensor.optical_res << '\n'
        << "    resolutions: " << format_indent_braced_list(4, sensor.resolutions) << '\n'
        << "    channels: " << format_vector_unsigned(4, sensor.channels) << '\n'
        << "    method: " << sensor.method << '\n'
        << "    register_dpihw_override: " << sensor.register_dpihw_override << '\n'
        << "    logical_dpihw_override: " << sensor.logical_dpihw_override << '\n'
        << "    dpiset_override: " << sensor.dpiset_override << '\n'
        << "    ccd_size_divisor: " << sensor.ccd_size_divisor << '\n'
        << "    pixel_count_multiplier: " << sensor.pixel_count_multiplier << '\n'
        << "    black_pixels: " << sensor.black_pixels << '\n'
        << "    dummy_pixel: " << sensor.dummy_pixel << '\n'
        << "    ccd_start_xoffset: " << sensor.ccd_start_xoffset << '\n'
        << "    sensor_pixels: " << sensor.sensor_pixels << '\n'
        << "    fau_gain_white_ref: " << sensor.fau_gain_white_ref << '\n'
        << "    gain_white_ref: " << sensor.gain_white_ref << '\n'
        << "    exposure: " << format_indent_braced_list(4, sensor.exposure) << '\n'
        << "    exposure_lperiod: " << sensor.exposure_lperiod << '\n'
        << "    segment_size: " << sensor.segment_size << '\n'
        << "    segment_order: "
        << format_indent_braced_list(4, format_vector_unsigned(4, sensor.segment_order)) << '\n'
        << "    custom_base_regs: " << format_indent_braced_list(4, sensor.custom_base_regs) << '\n'
        << "    custom_regs: " << format_indent_braced_list(4, sensor.custom_regs) << '\n'
        << "    custom_fe_regs: " << format_indent_braced_list(4, sensor.custom_fe_regs) << '\n'
        << "    gamma.red: " << sensor.gamma[0] << '\n'
        << "    gamma.green: " << sensor.gamma[1] << '\n'
        << "    gamma.blue: " << sensor.gamma[2] << '\n'
        << "}";
    return out;
}

} // namespace genesys
