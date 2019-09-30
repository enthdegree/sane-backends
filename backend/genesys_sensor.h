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

enum class FrontendType : unsigned
{
    UNKNOWN,
    WOLFSON,
    ANALOG_DEVICES
};

struct GenesysFrontendLayout
{
    FrontendType type = FrontendType::UNKNOWN;
    std::array<uint16_t, 3> offset_addr = {};
    std::array<uint16_t, 3> gain_addr = {};

    bool operator==(const GenesysFrontendLayout& other) const
    {
        return type == other.type &&
                offset_addr == other.offset_addr &&
                gain_addr == other.gain_addr;
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

    // all registers of the frontend. Note that the registers can hold 9-bit values
    RegisterSettingSet<std::uint16_t> regs;

    // extra control registers
    std::array<std::uint16_t, 3> reg2 = {};

    GenesysFrontendLayout layout;

    void set_offset(unsigned which, std::uint16_t value)
    {
        regs.set_value(layout.offset_addr[which], value);
    }

    void set_gain(unsigned which, std::uint16_t value)
    {
        regs.set_value(layout.gain_addr[which], value);
    }

    std::uint16_t get_offset(unsigned which) const
    {
        return regs.get_value(layout.offset_addr[which]);
    }

    std::uint16_t get_gain(unsigned which) const
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
    uint16_t red = 0;
    uint16_t green = 0;
    uint16_t blue = 0;

    SensorExposure() = default;
    SensorExposure(std::uint16_t r, std::uint16_t g, std::uint16_t b) :
        red{r}, green{g}, blue{b}
    {}

    bool operator==(const SensorExposure& other) const
    {
        return red == other.red && green == other.green && blue == other.blue;
    }
};

struct SensorProfile
{
    unsigned dpi = 0;
    unsigned ccd_size_divisor = 1;
    unsigned exposure_lperiod = 0;
    SensorExposure exposure;
    unsigned segment_size = 0; // only on GL846, GL847

    // the order of the segments, if any, for the profile. If the sensor is not segmented or uses
    // only single segment, this array can be empty
    std::vector<unsigned> segment_order;

    GenesysRegisterSettingSet custom_regs;

    unsigned get_segment_count() const
    {
        if (segment_order.size() < 2)
            return 1;
        return segment_order.size();
    }

    bool operator==(const SensorProfile& other) const
    {
        return  dpi == other.dpi &&
                ccd_size_divisor == other.ccd_size_divisor &&
                exposure_lperiod == other.exposure_lperiod &&
                exposure == other.exposure &&
                segment_order == other.segment_order &&
                custom_regs == other.custom_regs;
    }
};

template<class Stream>
void serialize(Stream& str, SensorProfile& x)
{
    serialize(str, x.dpi);
    serialize(str, x.ccd_size_divisor);
    serialize(str, x.exposure_lperiod);
    serialize(str, x.exposure.red);
    serialize(str, x.exposure.green);
    serialize(str, x.exposure.blue);
    serialize(str, x.segment_order);
    serialize(str, x.custom_regs);
}

class ResolutionFilter
{
public:
    struct Any {};
    static constexpr Any ANY{};

    ResolutionFilter() : matches_any_{false} {}
    ResolutionFilter(Any) : matches_any_{true} {}
    ResolutionFilter(std::initializer_list<unsigned> resolutions) :
        matches_any_{false},
        resolutions_{resolutions}
    {}

    bool matches(unsigned resolution) const
    {
        if (matches_any_)
            return true;
        auto it = std::find(resolutions_.begin(), resolutions_.end(), resolution);
        return it != resolutions_.end();
    }

    bool operator==(const ResolutionFilter& other) const
    {
        return  matches_any_ == other.matches_any_ && resolutions_ == other.resolutions_;
    }

    bool matches_any() const { return matches_any_; }
    const std::vector<unsigned>& resolutions() const { return resolutions_; }

private:
    bool matches_any_ = false;
    std::vector<unsigned> resolutions_;

    template<class Stream>
    friend void serialize(Stream& str, ResolutionFilter& x);
};

template<class Stream>
void serialize(Stream& str, ResolutionFilter& x)
{
    serialize(str, x.matches_any_);
    serialize_newline(str);
    serialize(str, x.resolutions_);
}

struct Genesys_Sensor {

    Genesys_Sensor() = default;
    ~Genesys_Sensor() = default;

    // id of the sensor description
    uint8_t sensor_id = 0;

    // sensor resolution in CCD pixels. Note that we may read more than one CCD pixel per logical
    // pixel, see ccd_pixels_per_system_pixel()
    int optical_res = 0;

    // the resolution list that the sensor is usable at.
    ResolutionFilter resolutions = ResolutionFilter::ANY;

    // the channel list that the sensor is usable at
    std::vector<unsigned> channels = { 1, 3 };

    // the scan method used with the sensor
    ScanMethod method = ScanMethod::FLATBED;

    // The scanner may be setup to use a custom dpihw that does not correspond to any actual
    // resolution. The value zero does not set the override.
    unsigned register_dpihw_override = 0;

    // The scanner may be setup to use a custom logical dpihw that does not correspond to any actual
    // resolution. The value zero does not set the override.
    unsigned logical_dpihw_override = 0;

    // The scanner may be setup to use a custom dpiset value that does not correspond to any actual
    // resolution. The value zero does not set the override.
    unsigned dpiset_override = 0;

    // CCD may present itself as half or quarter-size CCD on certain resolutions
    int ccd_size_divisor = 1;

    // Some scanners need an additional multiplier over the scan coordinates
    int pixel_count_multiplier = 1;

    int black_pixels = 0;
    // value of the dummy register
    int dummy_pixel = 0;
    // last pixel of CCD margin at optical resolution
    int ccd_start_xoffset = 0;
    // total pixels used by the sensor
    int sensor_pixels = 0;
    // TA CCD target code (reference gain)
    int fau_gain_white_ref = 0;
    // CCD target code (reference gain)
    int gain_white_ref = 0;

    // red, green and blue initial exposure values
    SensorExposure exposure;

    int exposure_lperiod = -1;

    // the number of pixels in a single segment.
    // only on gl843
    unsigned segment_size = 0;

    // the order of the segments, if any, for the sensor. If the sensor is not segmented or uses
    // only single segment, this array can be empty
    // only on gl843
    std::vector<unsigned> segment_order;

    GenesysRegisterSettingSet custom_base_regs; // gl646-specific
    GenesysRegisterSettingSet custom_regs;
    GenesysRegisterSettingSet custom_fe_regs;

    // red, green and blue gamma coefficient for default gamma tables
    AssignableArray<float, 3> gamma;

    std::vector<SensorProfile> sensor_profiles;

    std::function<unsigned(const Genesys_Sensor&, unsigned)> get_logical_hwdpi_fun;
    std::function<unsigned(const Genesys_Sensor&, unsigned)> get_register_hwdpi_fun;
    std::function<unsigned(const Genesys_Sensor&, unsigned)> get_ccd_size_divisor_fun;
    std::function<unsigned(const Genesys_Sensor&, unsigned)> get_hwdpi_divisor_fun;

    unsigned get_logical_hwdpi(unsigned xres) const { return get_logical_hwdpi_fun(*this, xres); }
    unsigned get_register_hwdpi(unsigned xres) const { return get_register_hwdpi_fun(*this, xres); }
    unsigned get_ccd_size_divisor_for_dpi(unsigned xres) const
    {
        return get_ccd_size_divisor_fun(*this, xres);
    }
    unsigned get_hwdpi_divisor_for_dpi(unsigned xres) const
    {
        return get_hwdpi_divisor_fun(*this, xres);
    }

    // how many CCD pixels are processed per system pixel time. This corresponds to CKSEL + 1
    unsigned ccd_pixels_per_system_pixel() const
    {
        // same on GL646, GL841, GL843, GL846, GL847, GL124
        constexpr unsigned REG_0x18_CKSEL = 0x03;
        return (custom_regs.get_value(0x18) & REG_0x18_CKSEL) + 1;
    }

    bool matches_channel_count(unsigned count) const
    {
        return std::find(channels.begin(), channels.end(), count) != channels.end();
    }

    unsigned get_segment_count() const
    {
        if (segment_order.size() < 2)
            return 1;
        return segment_order.size();
    }

    bool operator==(const Genesys_Sensor& other) const
    {
        return sensor_id == other.sensor_id &&
            optical_res == other.optical_res &&
            resolutions == other.resolutions &&
            method == other.method &&
            ccd_size_divisor == other.ccd_size_divisor &&
            black_pixels == other.black_pixels &&
            dummy_pixel == other.dummy_pixel &&
            ccd_start_xoffset == other.ccd_start_xoffset &&
            sensor_pixels == other.sensor_pixels &&
            fau_gain_white_ref == other.fau_gain_white_ref &&
            gain_white_ref == other.gain_white_ref &&
            exposure == other.exposure &&
            exposure_lperiod == other.exposure_lperiod &&
            segment_size == other.segment_size &&
            segment_order == other.segment_order &&
            custom_base_regs == other.custom_base_regs &&
            custom_regs == other.custom_regs &&
            custom_fe_regs == other.custom_fe_regs &&
            gamma == other.gamma &&
            sensor_profiles == other.sensor_profiles;
    }
};

template<class Stream>
void serialize(Stream& str, Genesys_Sensor& x)
{
    serialize(str, x.sensor_id);
    serialize(str, x.optical_res);
    serialize(str, x.resolutions);
    serialize(str, x.method);
    serialize(str, x.ccd_size_divisor);
    serialize(str, x.black_pixels);
    serialize(str, x.dummy_pixel);
    serialize(str, x.ccd_start_xoffset);
    serialize(str, x.sensor_pixels);
    serialize(str, x.fau_gain_white_ref);
    serialize(str, x.gain_white_ref);
    serialize_newline(str);
    serialize(str, x.exposure.blue);
    serialize(str, x.exposure.green);
    serialize(str, x.exposure.red);
    serialize(str, x.exposure_lperiod);
    serialize_newline(str);
    serialize(str, x.segment_size);
    serialize_newline(str);
    serialize(str, x.segment_order);
    serialize_newline(str);
    serialize(str, x.custom_base_regs);
    serialize_newline(str);
    serialize(str, x.custom_regs);
    serialize_newline(str);
    serialize(str, x.custom_fe_regs);
    serialize_newline(str);
    serialize(str, x.gamma);
    serialize_newline(str);
    serialize(str, x.sensor_profiles);
}

#endif // BACKEND_GENESYS_SENSOR_H
