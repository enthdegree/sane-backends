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

const MethodResolutions& Genesys_Model::get_resolution_settings(ScanMethod method) const
{
    for (const auto& res_for_method : resolutions) {
        for (auto res_method : res_for_method.methods) {
            if (res_method == method) {
                return res_for_method;
            }
        }
    }
    throw SaneException("Could not find resolution settings for method %d",
                        static_cast<unsigned>(method));
}

std::vector<unsigned> Genesys_Model::get_resolutions(ScanMethod method) const
{
    return get_resolution_settings(method).get_resolutions();
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

UsbDevice& Genesys_Device::get_usb_device()
{
    if (interface->is_mock())
        throw SaneException("Can't get USB device of mock interface");
    return usb_dev;
}

void apply_reg_settings_to_device(Genesys_Device& dev, const GenesysRegisterSettingSet& regs)
{
    for (const auto& reg : regs) {
        uint8_t val = dev.interface->read_register(reg.address);
        val = (val & ~reg.mask) | (reg.value & reg.mask);
        dev.interface->write_register(reg.address, val);
    }
}

} // namespace genesys
