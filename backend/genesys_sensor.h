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

#ifndef BACKEND_GENESYS_SENSOR_H
#define BACKEND_GENESYS_SENSOR_H

#include "genesys_enums.h"
#include "genesys_register.h"
#include "genesys_serialize.h"
#include <array>
#include <functional>

template<class T, size_t Size>
struct AssignableArray : public std::array<T, Size> {
    AssignableArray() = default;
    AssignableArray(const AssignableArray&) = default;
    AssignableArray& operator=(const AssignableArray&) = default;

    AssignableArray& operator=(std::initializer_list<T> init)
    {
        if (init.size() != std::array<T, Size>::size())
            throw std::runtime_error("An array of incorrect size assigned");
        std::copy(init.begin(), init.end(), std::array<T, Size>::begin());
        return *this;
    }
};

struct GenesysFrontendLayout
{
    std::array<uint16_t, 3> offset_addr = {};
    std::array<uint16_t, 3> gain_addr = {};

    bool operator==(const GenesysFrontendLayout& other) const
    {
        return offset_addr == other.offset_addr && gain_addr == other.gain_addr;
    }
};

/** @brief Data structure to set up analog frontend.
    The analog frontend converts analog value from image sensor to digital value. It has its own
    control registers which are set up with this structure. The values are written using
    sanei_genesys_fe_write_data.
 */
struct Genesys_Frontend
{
    Genesys_Frontend() = default;

    // id of the frontend description
    uint8_t fe_id = 0;

    // all registers of the frontend
    GenesysRegisterSettingSet regs;

    // extra control registers
    std::array<uint8_t, 3> reg2 = {};

    GenesysFrontendLayout layout;

    void set_offset(unsigned which, uint8_t value)
    {
        regs.set_value(layout.offset_addr[which], value);
    }

    void set_gain(unsigned which, uint8_t value)
    {
        regs.set_value(layout.gain_addr[which], value);
    }

    uint8_t get_offset(unsigned which) const
    {
        return regs.get_value(layout.offset_addr[which]);
    }

    uint8_t get_gain(unsigned which) const
    {
        return regs.get_value(layout.gain_addr[which]);
    }

    bool operator==(const Genesys_Frontend& other) const
    {
        return fe_id == other.fe_id &&
            regs == other.regs &&
            reg2 == other.reg2 &&
            layout == other.layout;
    }
};

template<class Stream>
void serialize(Stream& str, Genesys_Frontend& x)
{
    serialize(str, x.fe_id);
    serialize_newline(str);
    serialize(str, x.regs);
    serialize_newline(str);
    serialize(str, x.reg2);
    serialize_newline(str);
    serialize(str, x.layout.offset_addr);
    serialize(str, x.layout.gain_addr);
}

struct SensorExposure {
    uint16_t red, green, blue;
};

struct Genesys_Sensor {

    Genesys_Sensor() = default;
    ~Genesys_Sensor() = default;

    // id of the sensor description
    uint8_t sensor_id = 0;

    // sensor resolution in CCD pixels. Note that we may read more than one CCD pixel per logical
    // pixel, see ccd_pixels_per_system_pixel()
    int optical_res = 0;

    // the minimum and maximum resolution this sensor is usable at. -1 means that the resolution
    // can be any.
    int min_resolution = -1;
    int max_resolution = -1;

    // the scan method used with the sensor
    ScanMethod method = ScanMethod::FLATBED;

    // CCD may present itself as half or quarter-size CCD on certain resolutions
    int ccd_size_divisor = 1;

    int black_pixels = 0;
    // value of the dummy register
    int dummy_pixel = 0;
    // last pixel of CCD margin at optical resolution
    int CCD_start_xoffset = 0;
    // total pixels used by the sensor
    int sensor_pixels = 0;
    // TA CCD target code (reference gain)
    int fau_gain_white_ref = 0;
    // CCD target code (reference gain)
    int gain_white_ref = 0;

    // red, green and blue initial exposure values
    SensorExposure exposure;

    int exposure_lperiod = -1;

    GenesysRegisterSettingSet custom_regs;
    GenesysRegisterSettingSet custom_fe_regs;

    // red, green and blue gamma coefficient for default gamma tables
    AssignableArray<float, 3> gamma;

    std::function<unsigned(const Genesys_Sensor&, unsigned)> get_logical_hwdpi_fun;
    std::function<unsigned(const Genesys_Sensor&, unsigned)> get_register_hwdpi_fun;

    unsigned get_logical_hwdpi(unsigned xres) const { return get_logical_hwdpi_fun(*this, xres); }
    unsigned get_register_hwdpi(unsigned xres) const { return get_register_hwdpi_fun(*this, xres); }

    int get_ccd_size_divisor_for_dpi(int xres) const
    {
        if (ccd_size_divisor >= 4 && xres * 4 <= optical_res) {
            return 4;
        }
        if (ccd_size_divisor >= 2 && xres * 2 <= optical_res) {
            return 2;
        }
        return 1;
    }

    // how many CCD pixels are processed per system pixel time. This corresponds to CKSEL + 1
    unsigned ccd_pixels_per_system_pixel() const
    {
        // same on GL646, GL841, GL843, GL846, GL847, GL124
        constexpr unsigned REG_0x18_CKSEL = 0x03;
        return (custom_regs.get_value(0x18) & REG_0x18_CKSEL) + 1;
    }

    bool operator==(const Genesys_Sensor& other) const
    {
        return sensor_id == other.sensor_id &&
            optical_res == other.optical_res &&
            min_resolution == other.min_resolution &&
            max_resolution == other.max_resolution &&
            method == other.method &&
            ccd_size_divisor == other.ccd_size_divisor &&
            black_pixels == other.black_pixels &&
            dummy_pixel == other.dummy_pixel &&
            CCD_start_xoffset == other.CCD_start_xoffset &&
            sensor_pixels == other.sensor_pixels &&
            fau_gain_white_ref == other.fau_gain_white_ref &&
            gain_white_ref == other.gain_white_ref &&
            exposure.blue == other.exposure.blue &&
            exposure.green == other.exposure.green &&
            exposure.red == other.exposure.red &&
            exposure_lperiod == other.exposure_lperiod &&
            custom_regs == other.custom_regs &&
            custom_fe_regs == other.custom_fe_regs &&
            gamma == other.gamma;
    }
};

template<class Stream>
void serialize(Stream& str, Genesys_Sensor& x)
{
    serialize(str, x.sensor_id);
    serialize(str, x.optical_res);
    serialize(str, x.min_resolution);
    serialize(str, x.max_resolution);
    serialize(str, x.method);
    serialize(str, x.ccd_size_divisor);
    serialize(str, x.black_pixels);
    serialize(str, x.dummy_pixel);
    serialize(str, x.CCD_start_xoffset);
    serialize(str, x.sensor_pixels);
    serialize(str, x.fau_gain_white_ref);
    serialize(str, x.gain_white_ref);
    serialize_newline(str);
    serialize(str, x.exposure.blue);
    serialize(str, x.exposure.green);
    serialize(str, x.exposure.red);
    serialize(str, x.exposure_lperiod);
    serialize_newline(str);
    serialize(str, x.custom_regs);
    serialize_newline(str);
    serialize(str, x.custom_fe_regs);
    serialize_newline(str);
    serialize(str, x.gamma);
}

#endif //
