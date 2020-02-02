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

#include "device.h"
#include "command_set.h"
#include "low.h"
#include "utilities.h"

namespace genesys {

std::vector<unsigned> MethodResolutions::get_resolutions() const
{
    std::vector<unsigned> ret;
    std::copy(resolutions_x.begin(), resolutions_x.end(), std::back_inserter(ret));
    std::copy(resolutions_y.begin(), resolutions_y.end(), std::back_inserter(ret));
    // sort in decreasing order

    std::sort(ret.begin(), ret.end(), std::greater<unsigned>());
    ret.erase(std::unique(ret.begin(), ret.end()), ret.end());
    return ret;
}

const MethodResolutions* Genesys_Model::get_resolution_settings_ptr(ScanMethod method) const
{
    for (const auto& res_for_method : resolutions) {
        for (auto res_method : res_for_method.methods) {
            if (res_method == method) {
                return &res_for_method;
            }
        }
    }
    return nullptr;

}
const MethodResolutions& Genesys_Model::get_resolution_settings(ScanMethod method) const
{
    const auto* ptr = get_resolution_settings_ptr(method);
    if (ptr)
        return *ptr;

    throw SaneException("Could not find resolution settings for method %d",
                        static_cast<unsigned>(method));
}

std::vector<unsigned> Genesys_Model::get_resolutions(ScanMethod method) const
{
    return get_resolution_settings(method).get_resolutions();
}

bool Genesys_Model::has_method(ScanMethod method) const
{
    return get_resolution_settings_ptr(method) != nullptr;
}


Genesys_Device::~Genesys_Device()
{
    clear();
}

void Genesys_Device::clear()
{
    read_buffer.clear();
    binarize_buffer.clear();
    local_buffer.clear();

    calib_file.clear();

    calibration_cache.clear();

    white_average_data.clear();
    dark_average_data.clear();
}

ImagePipelineNodeBytesSource& Genesys_Device::get_pipeline_source()
{
    return static_cast<ImagePipelineNodeBytesSource&>(pipeline.front());
}

bool Genesys_Device::is_head_pos_known(ScanHeadId scan_head) const
{
    switch (scan_head) {
        case ScanHeadId::PRIMARY: return is_head_pos_primary_known_;
        case ScanHeadId::SECONDARY: return is_head_pos_secondary_known_;
        case ScanHeadId::ALL: return is_head_pos_primary_known_ && is_head_pos_secondary_known_;
        default:
            throw SaneException("Unknown scan head ID");
    }
}
unsigned Genesys_Device::head_pos(ScanHeadId scan_head) const
{
    switch (scan_head) {
        case ScanHeadId::PRIMARY: return head_pos_primary_;
        case ScanHeadId::SECONDARY: return head_pos_secondary_;
        default:
            throw SaneException("Unknown scan head ID");
    }
}

void Genesys_Device::set_head_pos_unknown(ScanHeadId scan_head)
{
    if ((scan_head & ScanHeadId::PRIMARY) != ScanHeadId::NONE) {
        is_head_pos_primary_known_ = false;
    }
    if ((scan_head & ScanHeadId::SECONDARY) != ScanHeadId::NONE) {
        is_head_pos_secondary_known_ = false;
    }
}

void Genesys_Device::set_head_pos_zero(ScanHeadId scan_head)
{
    if ((scan_head & ScanHeadId::PRIMARY) != ScanHeadId::NONE) {
        head_pos_primary_ = 0;
        is_head_pos_primary_known_ = true;
    }
    if ((scan_head & ScanHeadId::SECONDARY) != ScanHeadId::NONE) {
        head_pos_secondary_ = 0;
        is_head_pos_secondary_known_ = true;
    }
}

void Genesys_Device::advance_head_pos_by_session(ScanHeadId scan_head)
{
    int motor_steps = session.params.starty +
                      (session.params.lines * motor.base_ydpi) / session.params.yres;
    auto direction = has_flag(session.params.flags, ScanFlag::REVERSE) ? Direction::BACKWARD
                                                                       : Direction::FORWARD;
    advance_head_pos_by_steps(scan_head, direction, motor_steps);
}

static void advance_pos(unsigned& pos, Direction direction, unsigned offset)
{
    if (direction == Direction::FORWARD) {
        pos += offset;
    } else {
        if (pos < offset) {
            throw SaneException("Trying to advance head behind the home sensor");
        }
        pos -= offset;
    }
}

void Genesys_Device::advance_head_pos_by_steps(ScanHeadId scan_head, Direction direction,
                                               unsigned steps)
{
    if ((scan_head & ScanHeadId::PRIMARY) != ScanHeadId::NONE) {
        if (!is_head_pos_primary_known_) {
            throw SaneException("Trying to advance head while scanhead position is not known");
        }
        advance_pos(head_pos_primary_, direction, steps);
    }
    if ((scan_head & ScanHeadId::SECONDARY) != ScanHeadId::NONE) {
        if (!is_head_pos_secondary_known_) {
            throw SaneException("Trying to advance head while scanhead position is not known");
        }
        advance_pos(head_pos_secondary_, direction, steps);
    }
}

void print_scan_position(std::ostream& out, const Genesys_Device& dev, ScanHeadId scan_head)
{
    if (dev.is_head_pos_known(scan_head)) {
        out << dev.head_pos(scan_head);
    } else {
        out <<"(unknown)";
    }
}

std::ostream& operator<<(std::ostream& out, const Genesys_Device& dev)
{
    StreamStateSaver state_saver{out};

    out << "Genesys_Device{\n"
        << std::hex
        << "    vendorId: 0x" << dev.vendorId << '\n'
        << "    productId: 0x" << dev.productId << '\n'
        << std::dec
        << "    usb_mode: " << dev.usb_mode << '\n'
        << "    file_name: " << dev.file_name << '\n'
        << "    calib_file: " << dev.calib_file << '\n'
        << "    force_calibration: " << dev.force_calibration << '\n'
        << "    ignore_offsets: " << dev.ignore_offsets << '\n'
        << "    model: (not printed)\n"
        << "    reg: " << format_indent_braced_list(4, dev.reg) << '\n'
        << "    initial_regs: " << format_indent_braced_list(4, dev.initial_regs) << '\n'
        << "    settings: " << format_indent_braced_list(4, dev.settings) << '\n'
        << "    frontend: " << format_indent_braced_list(4, dev.frontend) << '\n'
        << "    frontend_initial: " << format_indent_braced_list(4, dev.frontend_initial) << '\n'
        << "    frontend_is_init: " << dev.frontend_is_init << '\n'
        << "    gpo.regs: " << format_indent_braced_list(4, dev.gpo.regs) << '\n'
        << "    motor: " << format_indent_braced_list(4, dev.motor) << '\n'
        << "    control[0..6]: " << std::hex
        << static_cast<unsigned>(dev.control[0]) << ' '
        << static_cast<unsigned>(dev.control[1]) << ' '
        << static_cast<unsigned>(dev.control[2]) << ' '
        << static_cast<unsigned>(dev.control[3]) << ' '
        << static_cast<unsigned>(dev.control[4]) << ' '
        << static_cast<unsigned>(dev.control[5]) << '\n' << std::dec
        << "    average_size: " << dev.average_size << '\n'
        << "    calib_total_bytes_to_read: " << dev.calib_total_bytes_to_read << '\n'
        << "    calib_session: " << format_indent_braced_list(4, dev.calib_session) << '\n'
        << "    gamma_override_tables[0].size(): " << dev.gamma_override_tables[0].size() << '\n'
        << "    gamma_override_tables[1].size(): " << dev.gamma_override_tables[1].size() << '\n'
        << "    gamma_override_tables[2].size(): " << dev.gamma_override_tables[2].size() << '\n'
        << "    white_average_data.size(): " << dev.white_average_data.size() << '\n'
        << "    dark_average_data.size(): " << dev.dark_average_data.size() << '\n'
        << "    already_initialized: " << dev.already_initialized << '\n'
        << "    scanhead_position[PRIMARY]: ";
    print_scan_position(out, dev, ScanHeadId::PRIMARY);
    out << '\n'
        << "    scanhead_position[SECONDARY]: ";
    print_scan_position(out, dev, ScanHeadId::SECONDARY);
    out << '\n'
        << "    read_active: " << dev.read_active << '\n'
        << "    parking: " << dev.parking << '\n'
        << "    document: " << dev.document << '\n'
        << "    read_buffer.size(): " << dev.read_buffer.size() << '\n'
        << "    binarize_buffer.size(): " << dev.binarize_buffer.size() << '\n'
        << "    local_buffer.size(): " << dev.local_buffer.size() << '\n'
        << "    oe_buffer.size(): " << dev.oe_buffer.size() << '\n'
        << "    total_bytes_read: " << dev.total_bytes_read << '\n'
        << "    total_bytes_to_read: " << dev.total_bytes_to_read << '\n'
        << "    session: " << format_indent_braced_list(4, dev.session) << '\n'
        << "    lineart_lut: (not printed)\n"
        << "    calibration_cache: (not printed)\n"
        << "    line_count: " << dev.line_count << '\n'
        << "    segment_order: "
        << format_indent_braced_list(4, format_vector_unsigned(4, dev.segment_order)) << '\n'
        << "    buffer_image: " << dev.buffer_image << '\n'
        << "    img_buffer.size(): " << dev.img_buffer.size() << '\n'
        << '}';
    return out;
}

void apply_reg_settings_to_device(Genesys_Device& dev, const GenesysRegisterSettingSet& regs)
{
    apply_reg_settings_to_device_with_backup(dev, regs);
}

GenesysRegisterSettingSet
    apply_reg_settings_to_device_with_backup(Genesys_Device& dev,
                                             const GenesysRegisterSettingSet& regs)
{
    GenesysRegisterSettingSet backup;
    for (const auto& reg : regs) {
        std::uint8_t old_val = dev.interface->read_register(reg.address);
        std::uint8_t new_val = (old_val & ~reg.mask) | (reg.value & reg.mask);
        dev.interface->write_register(reg.address, new_val);

        using SettingType = GenesysRegisterSettingSet::SettingType;
        backup.push_back(SettingType{reg.address,
                                     static_cast<std::uint8_t>(old_val & reg.mask),
                                     reg.mask});
    }
    return backup;
}

} // namespace genesys
