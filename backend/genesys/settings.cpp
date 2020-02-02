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

#include "settings.h"
#include "utilities.h"
#include <iomanip>

namespace genesys {

std::ostream& operator<<(std::ostream& out, const Genesys_Settings& settings)
{
    StreamStateSaver state_saver{out};

    out << "Genesys_Settings{\n"
        << "    xres: " << settings.xres << " yres: " << settings.yres << '\n'
        << "    lines: " << settings.lines << '\n'
        << "    pixels per line (actual): " << settings.pixels << '\n'
        << "    pixels per line (requested): " << settings.requested_pixels << '\n'
        << "    depth: " << settings.depth << '\n';
    auto prec = out.precision();
    out.precision(3);
    out << "    tl_x: " << settings.tl_x << " tl_y: " << settings.tl_y << '\n';
    out.precision(prec);
    out << "    scan_mode: " << settings.scan_mode << '\n'
        << '}';
    return out;
}

std::ostream& operator<<(std::ostream& out, const SetupParams& params)
{
    StreamStateSaver state_saver{out};

    bool reverse = has_flag(params.flags, ScanFlag::REVERSE);

    out << "SetupParams{\n"
        << "    xres: " << params.xres
            << " startx: " << params.startx
            << " pixels per line (actual): " << params.pixels
            << " pixels per line (requested): " << params.requested_pixels << '\n'

        << "    yres: " << params.yres
            << " lines: " << params.lines
            << " starty: " << params.starty << (reverse ? " (reverse)" : "") << '\n'

        << "    depth: " << params.depth << '\n'
        << "    channels: " << params.channels << '\n'
        << "    scan_mode: " << params.scan_mode << '\n'
        << "    color_filter: " << params.color_filter << '\n'
        << "    flags: " << params.flags << '\n'
        << "}";
    return out;
}

std::ostream& operator<<(std::ostream& out, const ScanSession& session)
{
    out << "ScanSession{\n"
        << "    computed: " << session.computed << '\n'
        << "    hwdpi_divisor: " << session.hwdpi_divisor << '\n'
        << "    ccd_size_divisor: " << session.ccd_size_divisor << '\n'
        << "    optical_resolution: " << session.optical_resolution << '\n'
        << "    optical_pixels: " << session.optical_pixels << '\n'
        << "    optical_pixels_raw: " << session.optical_pixels_raw << '\n'
        << "    optical_line_count: " << session.optical_line_count << '\n'
        << "    output_resolution: " << session.output_resolution << '\n'
        << "    output_pixels: " << session.output_pixels << '\n'
        << "    output_line_bytes: " << session.output_line_bytes << '\n'
        << "    output_line_bytes_raw: " << session.output_line_bytes_raw << '\n'
        << "    output_line_count: " << session.output_line_count << '\n'
        << "    num_staggered_lines: " << session.num_staggered_lines << '\n'
        << "    color_shift_lines_r: " << session.color_shift_lines_r << '\n'
        << "    color_shift_lines_g: " << session.color_shift_lines_g << '\n'
        << "    color_shift_lines_b: " << session.color_shift_lines_b << '\n'
        << "    max_color_shift_lines: " << session.max_color_shift_lines << '\n'
        << "    enable_ledadd: " << session.enable_ledadd << '\n'
        << "    segment_count: " << session.segment_count << '\n'
        << "    pixel_startx: " << session.pixel_startx << '\n'
        << "    pixel_endx: " << session.pixel_endx << '\n'
        << "    conseq_pixel_dist: " << session.conseq_pixel_dist << '\n'
        << "    output_segment_pixel_group_count: "
            << session.output_segment_pixel_group_count << '\n'
        << "    buffer_size_read: " << session.buffer_size_read << '\n'
        << "    buffer_size_read: " << session.buffer_size_lines << '\n'
        << "    buffer_size_shrink: " << session.buffer_size_shrink << '\n'
        << "    buffer_size_out: " << session.buffer_size_out << '\n'
        << "    filters: "
            << (session.pipeline_needs_reorder ? " reorder": "")
            << (session.pipeline_needs_ccd ? " ccd": "")
            << (session.pipeline_needs_shrink ? " shrink": "") << '\n'
        << "    params: " << format_indent_braced_list(4, session.params) << '\n'
        << "}";
    return out;
}

std::ostream& operator<<(std::ostream& out, const SANE_Parameters& params)
{
    out << "SANE_Parameters{\n"
        << "    format: " << static_cast<unsigned>(params.format) << '\n'
        << "    last_frame: " << params.last_frame << '\n'
        << "    bytes_per_line: " << params.bytes_per_line << '\n'
        << "    pixels_per_line: " << params.pixels_per_line << '\n'
        << "    lines: " << params.lines << '\n'
        << "    depth: " << params.depth << '\n'
        << '}';
    return out;
}

} // namespace genesys
