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

#include "genesys_device.h"
#include "genesys_low.h"


Genesys_Device::~Genesys_Device()
{
    clear();
}

void Genesys_Device::clear()
{
    read_buffer.clear();
    lines_buffer.clear();
    shrink_buffer.clear();
    out_buffer.clear();
    binarize_buffer.clear();
    local_buffer.clear();

    calib_file.clear();

    calibration_cache.clear();

    white_average_data.clear();
    dark_average_data.clear();
}

void apply_reg_settings_to_device(Genesys_Device& dev, const GenesysRegisterSettingSet& regs)
{
    for (const auto& reg : regs) {
        uint8_t val;
        sanei_genesys_read_register(&dev, reg.address, &val);
        val = (val & ~reg.mask) | (reg.value & reg.mask);
        sanei_genesys_write_register(&dev, reg.address, val);
    }
}
