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

#ifndef BACKEND_GENESYS_DEVICE_H
#define BACKEND_GENESYS_DEVICE_H

#include "calibration.h"
#include "command_set.h"
#include "buffer.h"
#include "enums.h"
#include "image_pipeline.h"
#include "motor.h"
#include "settings.h"
#include "sensor.h"
#include "register.h"
#include "usb_device.h"
#include "scanner_interface.h"
#include <vector>

namespace genesys {

struct Genesys_Gpo
{
    Genesys_Gpo() = default;

    // Genesys_Gpo
    GpioId id = GpioId::UNKNOWN;

    /*  GL646 and possibly others:
        - have the value registers at 0x66 and 0x67
        - have the enable registers at 0x68 and 0x69

        GL841, GL842, GL843, GL846, GL848 and possibly others:
        - have the value registers at 0x6c and 0x6d.
        - have the enable registers at 0x6e and 0x6f.
    */
    GenesysRegisterSettingSet regs;
};

/// Stores a SANE_Fixed value which is automatically converted from and to floating-point values
class FixedFloat
{
public:
    FixedFloat() = default;
    FixedFloat(const FixedFloat&) = default;
    FixedFloat(double number) : value_{SANE_FIX(number)} {}
    FixedFloat& operator=(const FixedFloat&) = default;
    FixedFloat& operator=(double number) { value_ = SANE_FIX(number); return *this; }

    operator double() const { return value(); }

    double value() const { return SANE_UNFIX(value_); }

private:
    SANE_Fixed value_ = 0;
};

struct MethodResolutions
{
    std::vector<ScanMethod> methods;
    std::vector<unsigned> resolutions_x;
    std::vector<unsigned> resolutions_y;

    unsigned get_min_resolution_x() const
    {
        return *std::min_element(resolutions_x.begin(), resolutions_x.end());
    }

    unsigned get_min_resolution_y() const
    {
        return *std::min_element(resolutions_y.begin(), resolutions_y.end());
    }

    std::vector<unsigned> get_resolutions() const;
};

/** @brief structure to describe a scanner model
 * This structure describes a model. It is composed of information on the
 * sensor, the motor, scanner geometry and flags to drive operation.
 */
struct Genesys_Model
{
    Genesys_Model() = default;

    const char* name = nullptr;
    const char* vendor = nullptr;
    const char* model = nullptr;
    ModelId model_id = ModelId::UNKNOWN;

    AsicType asic_type = AsicType::UNKNOWN;

    // possible x and y resolutions for each method supported by the scanner
    std::vector<MethodResolutions> resolutions;

    // possible depths in gray mode
    std::vector<unsigned> bpp_gray_values;
    // possible depths in color mode
    std::vector<unsigned> bpp_color_values;

    // the default scanning method. This is used when moving the head for example
    ScanMethod default_method = ScanMethod::FLATBED;

    // All offsets below are with respect to the sensor home position

    // Start of scan area in mm
    FixedFloat x_offset = 0;

    // Start of scan area in mm (Amount of feeding needed to get to the medium)
    FixedFloat y_offset = 0;

    // Size of scan area in mm
    FixedFloat x_size = 0;

    // Size of scan area in mm
    FixedFloat y_size = 0;

    // Start of white strip in mm
    FixedFloat y_offset_calib_white = 0;

    // Start of black mark in mm
    FixedFloat x_offset_calib_black = 0;

    // Start of scan area in transparency mode in mm
    FixedFloat x_offset_ta = 0;

    // Start of scan area in transparency mode in mm
    FixedFloat y_offset_ta = 0;

    // Size of scan area in transparency mode in mm
    FixedFloat x_size_ta = 0;

    // Size of scan area in transparency mode in mm
    FixedFloat y_size_ta = 0;

    // The position of the sensor when it's aligned with the lamp for transparency scanning
    FixedFloat y_offset_sensor_to_ta = 0;

    // Start of white strip in transparency mode in mm
    FixedFloat y_offset_calib_white_ta = 0;

    // Start of black strip in transparency mode in mm
    FixedFloat y_offset_calib_black_ta = 0;

    // Size of scan area after paper sensor stop sensing document in mm
    FixedFloat post_scan = 0;

    // Amount of feeding needed to eject document after finishing scanning in mm
    FixedFloat eject_feed = 0;

    // Line-distance correction (in pixel at optical_ydpi) for CCD scanners
    SANE_Int ld_shift_r = 0;
    SANE_Int ld_shift_g = 0;
    SANE_Int ld_shift_b = 0;

    // Order of the CCD/CIS colors
    ColorOrder line_mode_color_order = ColorOrder::RGB;

    // Is this a CIS or CCD scanner?
    bool is_cis = false;

    // Is this sheetfed scanner?
    bool is_sheetfed = false;

    // sensor type
    SensorId sensor_id = SensorId::UNKNOWN;
    // Analog-Digital converter type
    AdcId adc_id = AdcId::UNKNOWN;
    // General purpose output type
    GpioId gpio_id = GpioId::UNKNOWN;
    // stepper motor type
    MotorId motor_id = MotorId::UNKNOWN;

    // Which hacks are needed for this scanner?
    SANE_Word flags = 0;

    // Button flags, described existing buttons for the model
    SANE_Word buttons = 0;

    // how many lines are used for shading calibration
    SANE_Int shading_lines = 0;
    // how many lines are used for shading calibration in TA mode
    SANE_Int shading_ta_lines = 0;
    // how many lines are used to search start position
    SANE_Int search_lines = 0;

    const MethodResolutions& get_resolution_settings(ScanMethod method) const;

    std::vector<unsigned> get_resolutions(ScanMethod method) const;
};

/**
 * Describes the current device status for the backend
 * session. This should be more accurately called
 * Genesys_Session .
 */
struct Genesys_Device
{
    Genesys_Device() = default;
    ~Genesys_Device();

    using Calibration = std::vector<Genesys_Calibration_Cache>;

    // frees commonly used data
    void clear();

    SANE_Word vendorId = 0;			/**< USB vendor identifier */
    SANE_Word productId = 0;			/**< USB product identifier */

    // USB mode:
    // 0: not set
    // 1: USB 1.1
    // 2: USB 2.0
    SANE_Int usb_mode = 0;

    std::string file_name;
    std::string calib_file;

    // if enabled, no calibration data will be loaded or saved to files
    SANE_Int force_calibration = 0;
    // if enabled, will ignore the scan offsets and start scanning at true origin. This allows
    // acquiring the positions of the black and white strips and the actual scan area
    bool ignore_offsets = false;

    Genesys_Model *model = nullptr;

    // pointers to low level functions
    std::unique_ptr<CommandSet> cmd_set;

    Genesys_Register_Set reg;
    Genesys_Register_Set calib_reg;
    Genesys_Settings settings;
    Genesys_Frontend frontend, frontend_initial;

    // whether the frontend is initialized. This is currently used just to preserve historical
    // behavior
    bool frontend_is_init = false;

    Genesys_Gpo gpo;
    Genesys_Motor motor;
    std::uint8_t control[6] = {};

    size_t average_size = 0;
    // number of pixels used during shading calibration
    size_t calib_pixels = 0;
    // number of lines used during shading calibration
    size_t calib_lines = 0;
    size_t calib_channels = 0;
    size_t calib_resolution = 0;
     // bytes to read from USB when calibrating. If 0, this is not set
    size_t calib_total_bytes_to_read = 0;

    // the session that was configured for calibration
    ScanSession calib_session;

    // certain scanners support much higher resolution when scanning transparency, but we can't
    // read whole width of the scanner as a single line at that resolution. Thus for stuff like
    // calibration we want to read only the possible calibration area.
    size_t calib_pixels_offset = 0;

    // gamma overrides. If a respective array is not empty then it means that the gamma for that
    // color is overridden.
    std::vector<std::uint16_t> gamma_override_tables[3];

    std::vector<std::uint16_t> white_average_data;
    std::vector<std::uint16_t> dark_average_data;

    bool already_initialized = false;
    SANE_Int scanhead_position_in_steps = 0;

    bool read_active = false;
    // signal wether the park command has been issued
    bool parking = false;

    // for sheetfed scanner's, is TRUE when there is a document in the scanner
    bool document = false;

    bool needs_home_ta = false;

    Genesys_Buffer read_buffer;

    // buffer for digital lineart from gray data
    Genesys_Buffer binarize_buffer;
    // local buffer for gray data during dynamix lineart
    Genesys_Buffer local_buffer;

    // total bytes read sent to frontend
    size_t total_bytes_read = 0;
    // total bytes read to be sent to frontend
    size_t total_bytes_to_read = 0;

    // contains computed data for the current setup
    ScanSession session;

    // look up table used in dynamic rasterization
    unsigned char lineart_lut[256] = {};

    Calibration calibration_cache;

    // number of scan lines used during scan
    int line_count = 0;

    // array describing the order of the sub-segments of the sensor
    std::vector<unsigned> segment_order;

    // buffer to handle even/odd data
    Genesys_Buffer oe_buffer = {};

    // stores information about how the input image should be processed
    ImagePipelineStack pipeline;

    // an buffer that allows reading from `pipeline` in chunks of any size
    ImageBuffer pipeline_buffer;

    // when true the scanned picture is first buffered to allow software image enhancements
    bool buffer_image = false;

    // image buffer where the scanned picture is stored
    std::vector<std::uint8_t> img_buffer;

    ImagePipelineNodeBytesSource& get_pipeline_source();

    std::unique_ptr<ScannerInterface> interface;

private:
    friend class ScannerInterfaceUsb;
};

std::ostream& operator<<(std::ostream& out, const Genesys_Device& dev);

void apply_reg_settings_to_device(Genesys_Device& dev, const GenesysRegisterSettingSet& regs);

} // namespace genesys

#endif
