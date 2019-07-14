/* sane - Scanner Access Now Easy.

   Copyright (C) 2003 Oliver Rauch
   Copyright (C) 2003, 2004 Henning Meier-Geinitz <henning@meier-geinitz.de>
   Copyright (C) 2004, 2005 Gerhard Jaeger <gerhard@gjaeger.de>
   Copyright (C) 2004-2013 Stéphane Voltz <stef.dev@free.fr>
   Copyright (C) 2005-2009 Pierre Willenbrock <pierre@pirsoft.dnsalias.org>
   Copyright (C) 2006 Laurent Charpentier <laurent_pubs@yahoo.com>
   Parts of the structs have been taken from the gt68xx backend by
   Sergey Vlasov <vsu@altlinux.ru> et al.

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

#ifndef GENESYS_LOW_H
#define GENESYS_LOW_H


#include "../include/sane/config.h"

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <stddef.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_MKDIR
#include <sys/stat.h>
#include <sys/types.h>
#endif

#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"
#include "../include/sane/saneopts.h"

#include "../include/sane/sanei_backend.h"
#include "../include/sane/sanei_usb.h"

#include "../include/_stdint.h"

#include "genesys_error.h"
#include "genesys_sanei.h"
#include "genesys_serialize.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#define FREE_IFNOT_NULL(x)		if(x!=NULL) { free(x); x=NULL;}

#define GENESYS_RED   0
#define GENESYS_GREEN 1
#define GENESYS_BLUE  2

/* Flags */
#define GENESYS_FLAG_UNTESTED     (1 << 0)	/**< Print a warning for these scanners */
#define GENESYS_FLAG_14BIT_GAMMA  (1 << 1)	/**< use 14bit Gamma table instead of 12 */
#define GENESYS_FLAG_LAZY_INIT    (1 << 2)	/**< skip extensive ASIC test at init   */
#define GENESYS_FLAG_XPA          (1 << 3)
#define GENESYS_FLAG_SKIP_WARMUP  (1 << 4)	/**< skip genesys_warmup()              */
/** @brief offset calibration flag
 * signals that the scanner does offset calibration. In this case off_calibration() and
 * coarse_gain_calibration() functions must be implemented
 */
#define GENESYS_FLAG_OFFSET_CALIBRATION   (1 << 5)
#define GENESYS_FLAG_SEARCH_START (1 << 6)	/**< do start search before scanning    */
#define GENESYS_FLAG_REPARK       (1 << 7)	/**< repark head (and check for lock) by
						   moving without scanning */
#define GENESYS_FLAG_DARK_CALIBRATION (1 << 8)	/**< do dark calibration */
#define GENESYS_FLAG_STAGGERED_LINE   (1 << 9)	/**< pixel columns are shifted vertically for hi-res modes */

#define GENESYS_FLAG_MUST_WAIT        (1 << 10)	/**< tells wether the scanner must wait for the head when parking */


#define GENESYS_FLAG_HAS_UTA          (1 << 11)	/**< scanner has a transparency adapter */

#define GENESYS_FLAG_DARK_WHITE_CALIBRATION (1 << 12) /**< yet another calibration method. does white and dark shading in one run, depending on a black and a white strip*/
#define GENESYS_FLAG_CUSTOM_GAMMA     (1 << 13)       /**< allow custom gamma tables */
#define GENESYS_FLAG_NO_CALIBRATION   (1 << 14)       /**< allow scanners to use skip the calibration, needed for sheetfed scanners */
#define GENESYS_FLAG_SIS_SENSOR       (1 << 16)       /**< handling of multi-segments sensors in software */
#define GENESYS_FLAG_SHADING_NO_MOVE  (1 << 17)       /**< scanner doesn't move sensor during shading calibration */
#define GENESYS_FLAG_SHADING_REPARK   (1 << 18)       /**< repark head between shading scans */
#define GENESYS_FLAG_FULL_HWDPI_MODE  (1 << 19)       /**< scanner always use maximum hw dpi to setup the sensor */
// scanner has infrared transparency scanning capability
#define GENESYS_FLAG_HAS_UTA_INFRARED (1 << 20)

#define GENESYS_HAS_NO_BUTTONS       0              /**< scanner has no supported button */
#define GENESYS_HAS_SCAN_SW          (1 << 0)       /**< scanner has SCAN button */
#define GENESYS_HAS_FILE_SW          (1 << 1)       /**< scanner has FILE button */
#define GENESYS_HAS_COPY_SW          (1 << 2)       /**< scanner has COPY button */
#define GENESYS_HAS_EMAIL_SW         (1 << 3)       /**< scanner has EMAIL button */
#define GENESYS_HAS_PAGE_LOADED_SW   (1 << 4)       /**< scanner has paper in detection */
#define GENESYS_HAS_OCR_SW           (1 << 5)       /**< scanner has OCR button */
#define GENESYS_HAS_POWER_SW         (1 << 6)       /**< scanner has power button */
#define GENESYS_HAS_CALIBRATE        (1 << 7)       /**< scanner has 'calibrate' software button to start calibration */
#define GENESYS_HAS_EXTRA_SW         (1 << 8)       /**< scanner has extra function button */

/* USB control message values */
#define REQUEST_TYPE_IN		(USB_TYPE_VENDOR | USB_DIR_IN)
#define REQUEST_TYPE_OUT	(USB_TYPE_VENDOR | USB_DIR_OUT)
#define REQUEST_REGISTER	0x0c
#define REQUEST_BUFFER		0x04
#define VALUE_BUFFER		0x82
#define VALUE_SET_REGISTER	0x83
#define VALUE_READ_REGISTER	0x84
#define VALUE_WRITE_REGISTER	0x85
#define VALUE_INIT		0x87
#define GPIO_OUTPUT_ENABLE	0x89
#define GPIO_READ		0x8a
#define GPIO_WRITE		0x8b
#define VALUE_BUF_ENDACCESS	0x8c
#define VALUE_GET_REGISTER	0x8e
#define INDEX			0x00

/* todo: used?
#define VALUE_READ_STATUS	0x86
*/

/* Read/write bulk data/registers */
#define BULK_OUT		0x01
#define BULK_IN			0x00
#define BULK_RAM		0x00
#define BULK_REGISTER		0x11

#define BULKOUT_MAXSIZE         0xF000

/* AFE values */
#define AFE_INIT       1
#define AFE_SET        2
#define AFE_POWER_SAVE 4

#define LOWORD(x)  ((uint16_t)((x) & 0xffff))
#define HIWORD(x)  ((uint16_t)((x) >> 16))
#define LOBYTE(x)  ((uint8_t)((x) & 0xFF))
#define HIBYTE(x)  ((uint8_t)((x) >> 8))

/* Global constants */
/* TODO: emove this leftover of early backend days */
#define MOTOR_SPEED_MAX		350
#define DARK_VALUE		0

#define PWRBIT	        0x80
#define BUFEMPTY	0x40
#define FEEDFSH	        0x20
#define SCANFSH	        0x10
#define HOMESNR	        0x08
#define LAMPSTS	        0x04
#define FEBUSY	        0x02
#define MOTORENB	0x01

#define GENESYS_MAX_REGS 256

enum class ScanMethod : unsigned {
    // normal scan method
    FLATBED = 0,
    // scan using transparency adaptor
    TRANSPARENCY = 1,
    // scan using transparency adaptor via infrared channel
    TRANSPARENCY_INFRARED = 2
};

inline void serialize(std::istream& str, ScanMethod& x)
{
    unsigned value;
    serialize(str, value);
    x = static_cast<ScanMethod>(value);
}

inline void serialize(std::ostream& str, ScanMethod& x)
{
    unsigned value = static_cast<unsigned>(x);
    serialize(str, value);
}

enum class ScanColorMode : unsigned {
    LINEART = 0,
    HALFTONE,
    GRAY,
    COLOR_SINGLE_PASS
};

inline void serialize(std::istream& str, ScanColorMode& x)
{
    unsigned value;
    serialize(str, value);
    x = static_cast<ScanColorMode>(value);
}

inline void serialize(std::ostream& str, ScanColorMode& x)
{
    unsigned value = static_cast<unsigned>(x);
    serialize(str, value);
}

enum class ColorFilter : unsigned {
    RED = 0,
    GREEN,
    BLUE,
    NONE
};

inline void serialize(std::istream& str, ColorFilter& x)
{
    unsigned value;
    serialize(str, value);
    x = static_cast<ColorFilter>(value);
}

inline void serialize(std::ostream& str, ColorFilter& x)
{
    unsigned value = static_cast<unsigned>(x);
    serialize(str, value);
}

struct GenesysRegister {
    uint16_t address = 0;
    uint8_t value = 0;
};

inline bool operator<(const GenesysRegister& lhs, const GenesysRegister& rhs)
{
    return lhs.address < rhs.address;
}

struct GenesysRegisterSetState {
    bool is_lamp_on = false;
    bool is_xpa_on = false;
};

class Genesys_Register_Set {
public:
    using container = std::vector<GenesysRegister>;
    using iterator = typename container::iterator;
    using const_iterator = typename container::const_iterator;

    // FIXME: this shouldn't live here, but in a separate struct that contains Genesys_Register_Set
    GenesysRegisterSetState state;

    enum Options {
        SEQUENTIAL = 1
    };

    Genesys_Register_Set()
    {
        registers_.reserve(GENESYS_MAX_REGS);
    }

    // by default the register set is sorted by address. In certain cases it's importand to send
    // the registers in certain order: use the SEQUENTIAL option for that
    Genesys_Register_Set(Options opts) : Genesys_Register_Set()
    {
        if ((opts & SEQUENTIAL) == SEQUENTIAL) {
            sorted_ = false;
        }
    }

    void init_reg(uint16_t address, uint8_t default_value)
    {
        if (find_reg_index(address) >= 0) {
            set8(address, default_value);
            return;
        }
        GenesysRegister reg;
        reg.address = address;
        reg.value = default_value;
        registers_.push_back(reg);
        if (sorted_)
            std::sort(registers_.begin(), registers_.end());
    }

    void remove_reg(uint16_t address)
    {
        int i = find_reg_index(address);
        if (i < 0) {
            throw std::runtime_error("the register does not exist");
        }
        registers_.erase(registers_.begin() + i);
    }

    GenesysRegister& find_reg(uint16_t address)
    {
        int i = find_reg_index(address);
        if (i < 0) {
            throw std::runtime_error("the register does not exist");
        }
        return registers_[i];
    }

    const GenesysRegister& find_reg(uint16_t address) const
    {
        int i = find_reg_index(address);
        if (i < 0) {
            throw std::runtime_error("the register does not exist");
        }
        return registers_[i];
    }

    GenesysRegister* find_reg_address(uint16_t address)
    {
        return &find_reg(address);
    }

    const GenesysRegister* find_reg_address(uint16_t address) const
    {
        return &find_reg(address);
    }

    void set8(uint16_t address, uint8_t value)
    {
        find_reg(address).value = value;
    }

    void set8_mask(uint16_t address, uint8_t value, uint8_t mask)
    {
        auto& reg = find_reg(address);
        reg.value = (reg.value & ~mask) | value;
    }

    void set16(uint16_t address, uint16_t value)
    {
        find_reg(address).value = (value >> 8) & 0xff;
        find_reg(address + 1).value = value & 0xff;
    }

    void set24(uint16_t address, uint32_t value)
    {
        find_reg(address).value = (value >> 16) & 0xff;
        find_reg(address + 1).value = (value >> 8) & 0xff;
        find_reg(address + 2).value = value & 0xff;
    }

    uint8_t get8(uint16_t address) const
    {
        return find_reg(address).value;
    }

    uint16_t get16(uint16_t address) const
    {
        return (find_reg(address).value << 8) | find_reg(address + 1).value;
    }

    uint32_t get24(uint16_t address) const
    {
        return (find_reg(address).value << 16) |
               (find_reg(address + 1).value << 8) |
                find_reg(address + 2).value;
    }

    void clear() { registers_.clear(); }
    size_t size() const { return registers_.size(); }

    iterator begin() { return registers_.begin(); }
    const_iterator begin() const { return registers_.begin(); }

    iterator end() { return registers_.end(); }
    const_iterator end() const { return registers_.end(); }

private:
    int find_reg_index(uint16_t address) const
    {
        if (!sorted_) {
            for (size_t i = 0; i < registers_.size(); i++) {
                if (registers_[i].address == address) {
                    return i;
                }
            }
            return -1;
        }

        GenesysRegister search;
        search.address = address;
        auto it = std::lower_bound(registers_.begin(), registers_.end(), search);
        if (it == registers_.end())
            return -1;
        if (it->address != address)
            return -1;
        return std::distance(registers_.begin(), it);
    }

    // registers are stored in a sorted vector
    bool sorted_ = true;
    std::vector<GenesysRegister> registers_;
};

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

struct GenesysRegisterSetting {
    GenesysRegisterSetting() = default;

    GenesysRegisterSetting(uint16_t p_address, uint8_t p_value) :
        address(p_address), value(p_value)
    {}

    GenesysRegisterSetting(uint16_t p_address, uint8_t p_value, uint8_t p_mask) :
        address(p_address), value(p_value), mask(p_mask)
    {}

    uint16_t address = 0;
    uint8_t value = 0;
    uint8_t mask = 0xff;

    bool operator==(const GenesysRegisterSetting& other) const
    {
        return address == other.address && value == other.value && mask == other.mask;
    }
};

template<class Stream>
void serialize(Stream& str, GenesysRegisterSetting& reg)
{
    serialize(str, reg.address);
    serialize(str, reg.value);
    serialize(str, reg.mask);
}

class GenesysRegisterSettingSet {
public:
    using container = std::vector<GenesysRegisterSetting>;
    using iterator = typename container::iterator;
    using const_iterator = typename container::const_iterator;

    GenesysRegisterSettingSet() = default;
    GenesysRegisterSettingSet(std::initializer_list<GenesysRegisterSetting> ilist) : regs_(ilist) {}

    iterator begin() { return regs_.begin(); }
    const_iterator begin() const { return regs_.begin(); }
    iterator end() { return regs_.end(); }
    const_iterator end() const { return regs_.end(); }

    GenesysRegisterSetting& operator[](size_t i) { return regs_[i]; }
    const GenesysRegisterSetting& operator[](size_t i) const { return regs_[i]; }

    size_t size() const { return regs_.size(); }
    bool empty() const { return regs_.empty(); }
    void clear() { regs_.clear(); }

    void push_back(GenesysRegisterSetting reg) { regs_.push_back(reg); }

    void merge(const GenesysRegisterSettingSet& other)
    {
        for (const auto& reg : other) {
            set_value(reg.address, reg.value);
        }
    }

    uint8_t get_value(uint16_t address) const
    {
        for (const auto& reg : regs_) {
            if (reg.address == address)
                return reg.value;
        }
        throw std::runtime_error("Unknown register");
    }

    void set_value(uint16_t address, uint8_t value)
    {
        for (auto& reg : regs_) {
            if (reg.address == address) {
                reg.value = value;
                return;
            }
        }
        push_back(GenesysRegisterSetting(address, value));
    }

    friend void serialize(std::istream& str, GenesysRegisterSettingSet& reg);
    friend void serialize(std::ostream& str, GenesysRegisterSettingSet& reg);

    bool operator==(const GenesysRegisterSettingSet& other) const
    {
        return regs_ == other.regs_;
    }

private:
    std::vector<GenesysRegisterSetting> regs_;
};

inline void serialize(std::istream& str, GenesysRegisterSettingSet& reg)
{
    reg.clear();
    const size_t max_register_address =
            1 << (sizeof(GenesysRegisterSetting::address) * CHAR_BIT);
    serialize(str, reg.regs_, max_register_address);
}

inline void serialize(std::ostream& str, GenesysRegisterSettingSet& reg)
{
    serialize(str, reg.regs_);
}

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

struct Genesys_Gpo
{
    Genesys_Gpo() = default;

    Genesys_Gpo(uint8_t id, const std::array<uint8_t, 2>& v, const std::array<uint8_t, 2>& e)
    {
        gpo_id = id;
        value[0] = v[0];
        value[1] = v[1];
        enable[0] = e[0];
        enable[1] = e[1];
    }

    // Genesys_Gpo
    uint8_t gpo_id = 0;

    // registers 0x6c and 0x6d on GL841, GL842, GL843, GL846, GL848 and possibly others
    uint8_t value[2] = { 0, 0 };

    // registers 0x6e and 0x6f on GL841, GL842, GL843, GL846, GL848 and possibly others
    uint8_t enable[2] = { 0, 0 };
};

struct Genesys_Motor_Slope
{
    Genesys_Motor_Slope() = default;
    Genesys_Motor_Slope(int p_maximum_start_speed, int p_maximum_speed, int p_minimum_steps,
                        float p_g) :
        maximum_start_speed(p_maximum_start_speed),
        maximum_speed(p_maximum_speed),
        minimum_steps(p_minimum_steps),
        g(p_g)
    {}

    // maximum speed allowed when accelerating from standstill. Unit: pixeltime/step
    int maximum_start_speed = 0;
    // maximum speed allowed. Unit: pixeltime/step
    int maximum_speed = 0;
    // number of steps used for default curve
    int minimum_steps = 0;

    /*  power for non-linear acceleration curves.
        vs*(1-i^g)+ve*(i^g) where
        vs = start speed, ve = end speed,
        i = 0.0 for first entry and i = 1.0 for last entry in default table
    */
    float g = 0;
};


struct Genesys_Motor
{
    Genesys_Motor() = default;
    Genesys_Motor(uint8_t p_motor_id, int p_base_ydpi, int p_optical_ydpi, int p_max_step_type,
                  int p_power_mode_count,
                  const std::vector<std::vector<Genesys_Motor_Slope>>& p_slopes) :
        motor_id(p_motor_id),
        base_ydpi(p_base_ydpi),
        optical_ydpi(p_optical_ydpi),
        max_step_type(p_max_step_type),
        power_mode_count(p_power_mode_count),
        slopes(p_slopes)
    {}

    // id of the motor description
    uint8_t motor_id = 0;
    // motor base steps. Unit: 1/inch
    int base_ydpi = 0;
    // maximum resolution in y-direction. Unit: 1/inch
    int optical_ydpi = 0;
    // maximum step type. 0-2
    int max_step_type = 0;
    // number of power modes
    int power_mode_count = 0;
    // slopes to derive individual slopes from
    std::vector<std::vector<Genesys_Motor_Slope>> slopes;
};

typedef enum Genesys_Color_Order
{
  COLOR_ORDER_RGB,
  COLOR_ORDER_BGR
}
Genesys_Color_Order;


#define MAX_RESOLUTIONS 13
#define MAX_DPI 4

#define GENESYS_GL646	 646
#define GENESYS_GL841	 841
#define GENESYS_GL843	 843
#define GENESYS_GL845	 845
#define GENESYS_GL846	 846
#define GENESYS_GL847	 847
#define GENESYS_GL848	 848
#define GENESYS_GL123	 123
#define GENESYS_GL124	 124

enum Genesys_Model_Type
{
  MODEL_UMAX_ASTRA_4500 = 0,
  MODEL_CANON_LIDE_50,
  MODEL_PANASONIC_KV_SS080,
  MODEL_HP_SCANJET_4850C,
  MODEL_HP_SCANJET_G4010,
  MODEL_HP_SCANJET_G4050,
  MODEL_CANON_CANOSCAN_4400F,
  MODEL_CANON_CANOSCAN_8400F,
  MODEL_CANON_CANOSCAN_8600F,
  MODEL_CANON_LIDE_100,
  MODEL_CANON_LIDE_110,
  MODEL_CANON_LIDE_120,
  MODEL_CANON_LIDE_210,
  MODEL_CANON_LIDE_220,
  MODEL_CANON_CANOSCAN_5600F,
  MODEL_CANON_LIDE_700F,
  MODEL_CANON_LIDE_200,
  MODEL_CANON_LIDE_60,
  MODEL_CANON_LIDE_80,
  MODEL_HP_SCANJET_2300C,
  MODEL_HP_SCANJET_2400C,
  MODEL_VISIONEER_STROBE_XP200,
  MODEL_HP_SCANJET_3670C,
  MODEL_PLUSTEK_OPTICPRO_ST12,
  MODEL_PLUSTEK_OPTICPRO_ST24,
  MODEL_MEDION_MD5345,
  MODEL_VISIONEER_STROBE_XP300,
  MODEL_SYSCAN_DOCKETPORT_665,
  MODEL_VISIONEER_ROADWARRIOR,
  MODEL_SYSCAN_DOCKETPORT_465,
  MODEL_VISIONEER_STROBE_XP100_REVISION3,
  MODEL_PENTAX_DSMOBILE_600,
  MODEL_SYSCAN_DOCKETPORT_467,
  MODEL_SYSCAN_DOCKETPORT_685,
  MODEL_SYSCAN_DOCKETPORT_485,
  MODEL_DCT_DOCKETPORT_487,
  MODEL_VISIONEER_7100,
  MODEL_XEROX_2400,
  MODEL_XEROX_TRAVELSCANNER_100,
  MODEL_PLUSTEK_OPTICPRO_3600,
  MODEL_HP_SCANJET_N6310,
  MODEL_PLUSTEK_OPTICBOOK_3800,
  MODEL_CANON_IMAGE_FORMULA_101
};

enum Genesys_Dac_Type
{
  DAC_WOLFSON_UMAX = 0,
  DAC_WOLFSON_ST12,
  DAC_WOLFSON_ST24,
  DAC_WOLFSON_5345,
  DAC_WOLFSON_HP2400,
  DAC_WOLFSON_HP2300,
  DAC_CANONLIDE35,
  DAC_AD_XP200,
  DAC_WOLFSON_XP300,
  DAC_WOLFSON_HP3670,
  DAC_WOLFSON_DSM600,
  DAC_CANONLIDE200,
  DAC_KVSS080,
  DAC_G4050,
  DAC_CANONLIDE110,
  DAC_PLUSTEK_3600,
  DAC_CANONLIDE700,
  DAC_CS8400F,
  DAC_CS8600F,
  DAC_IMG101,
  DAC_PLUSTEK3800,
  DAC_CANONLIDE80,
  DAC_CANONLIDE120
};

enum Genesys_Sensor_Type
{
  CCD_UMAX = 0,
  CCD_ST12,         // SONY ILX548: 5340 Pixel  ???
  CCD_ST24,         // SONY ILX569: 10680 Pixel ???
  CCD_5345,
  CCD_HP2400,
  CCD_HP2300,
  CCD_CANONLIDE35,
  CIS_XP200,        // CIS sensor for Strobe XP200,
  CCD_HP3670,
  CCD_DP665,
  CCD_ROADWARRIOR,
  CCD_DSMOBILE600,
  CCD_XP300,
  CCD_DP685,
  CIS_CANONLIDE200,
  CIS_CANONLIDE100,
  CCD_KVSS080,
  CCD_G4050,
  CIS_CANONLIDE110,
  CCD_PLUSTEK_3600,
  CCD_HP_N6310,
  CIS_CANONLIDE700,
  CCD_CS4400F,
  CCD_CS8400F,
  CCD_CS8600F,
  CCD_IMG101,
  CCD_PLUSTEK3800,
  CIS_CANONLIDE210,
  CIS_CANONLIDE80,
  CIS_CANONLIDE220,
  CIS_CANONLIDE120,
};

enum Genesys_Gpo_Type
{
  GPO_UMAX,
  GPO_ST12,
  GPO_ST24,
  GPO_5345,
  GPO_HP2400,
  GPO_HP2300,
  GPO_CANONLIDE35,
  GPO_XP200,
  GPO_XP300,
  GPO_HP3670,
  GPO_DP665,
  GPO_DP685,
  GPO_CANONLIDE200,
  GPO_KVSS080,
  GPO_G4050,
  GPO_CANONLIDE110,
  GPO_PLUSTEK_3600,
  GPO_CANONLIDE210,
  GPO_HP_N6310,
  GPO_CANONLIDE700,
  GPO_CS4400F,
  GPO_CS8400F,
  GPO_CS8600F,
  GPO_IMG101,
  GPO_PLUSTEK3800,
  GPO_CANONLIDE80,
  GPO_CANONLIDE120
};

enum Genesys_Motor_Type
{
  MOTOR_UMAX = 0,
  MOTOR_5345,
  MOTOR_ST24,
  MOTOR_HP2400,
  MOTOR_HP2300,
  MOTOR_CANONLIDE35,
  MOTOR_XP200,
  MOTOR_XP300,
  MOTOR_HP3670,
  MOTOR_DP665,
  MOTOR_ROADWARRIOR,
  MOTOR_DSMOBILE_600,
  MOTOR_CANONLIDE200,
  MOTOR_CANONLIDE100,
  MOTOR_KVSS080,
  MOTOR_G4050,
  MOTOR_CANONLIDE110,
  MOTOR_PLUSTEK_3600,
  MOTOR_CANONLIDE700,
  MOTOR_CS8400F,
  MOTOR_CS8600F,
  MOTOR_IMG101,
  MOTOR_PLUSTEK3800,
  MOTOR_CANONLIDE210,
  MOTOR_CANONLIDE80,
  MOTOR_CANONLIDE120
};

/* Forward typedefs */
typedef struct Genesys_Device Genesys_Device;
struct Genesys_Scanner;
typedef struct Genesys_Calibration_Cache  Genesys_Calibration_Cache;

/**
 * Scanner command set description.
 *
 * This description contains parts which are common to all scanners with the
 * same command set, but may have different optical resolution and other
 * parameters.
 */
typedef struct Genesys_Command_Set
{
  /** @name Identification */
  /*@{ */

  /** Name of this command set */
  SANE_String_Const name;

  /*@} */

    bool (*needs_home_before_init_regs_for_scan) (Genesys_Device* dev);

  /** For ASIC initialization */
    SANE_Status (*init) (Genesys_Device * dev);

    SANE_Status (*init_regs_for_warmup) (Genesys_Device * dev,
                                         const Genesys_Sensor& sensor,
					 Genesys_Register_Set * regs,
					 int *channels, int *total_size);
    SANE_Status (*init_regs_for_coarse_calibration) (Genesys_Device * dev,
                                                     const Genesys_Sensor& sensor,
                                                     Genesys_Register_Set& regs);
    SANE_Status (*init_regs_for_shading) (Genesys_Device * dev, const Genesys_Sensor& sensor,
                                          Genesys_Register_Set& regs);
    SANE_Status (*init_regs_for_scan) (Genesys_Device * dev, const Genesys_Sensor& sensor);

    SANE_Bool (*get_filter_bit) (Genesys_Register_Set * reg);
    SANE_Bool (*get_lineart_bit) (Genesys_Register_Set * reg);
    SANE_Bool (*get_bitset_bit) (Genesys_Register_Set * reg);
    SANE_Bool (*get_gain4_bit) (Genesys_Register_Set * reg);
    SANE_Bool (*get_fast_feed_bit) (Genesys_Register_Set * reg);

    SANE_Bool (*test_buffer_empty_bit) (SANE_Byte val);
    SANE_Bool (*test_motor_flag_bit) (SANE_Byte val);

    SANE_Status (*set_fe) (Genesys_Device * dev, const Genesys_Sensor& sensor, uint8_t set);
    SANE_Status (*set_powersaving) (Genesys_Device * dev, int delay);
    SANE_Status (*save_power) (Genesys_Device * dev, SANE_Bool enable);

    SANE_Status (*begin_scan) (Genesys_Device * dev,
                               const Genesys_Sensor& sensor,
			       Genesys_Register_Set * regs,
			       SANE_Bool start_motor);
    SANE_Status (*end_scan) (Genesys_Device * dev,
			     Genesys_Register_Set * regs,
			     SANE_Bool check_stop);

    /**
     * Send gamma tables to ASIC
     */
    SANE_Status (*send_gamma_table) (Genesys_Device * dev, const Genesys_Sensor& sensor);

    SANE_Status (*search_start_position) (Genesys_Device * dev);
    SANE_Status (*offset_calibration) (Genesys_Device * dev, const Genesys_Sensor& sensor,
                                       Genesys_Register_Set& regs);
    SANE_Status (*coarse_gain_calibration) (Genesys_Device * dev,
                                            const Genesys_Sensor& sensor,
                                            Genesys_Register_Set& regs, int dpi);
    SANE_Status (*led_calibration) (Genesys_Device * dev, Genesys_Sensor& sensor,
                                    Genesys_Register_Set& regs);

    void (*wait_for_motor_stop) (Genesys_Device* dev);
    SANE_Status (*slow_back_home) (Genesys_Device * dev, SANE_Bool wait_until_home);
    SANE_Status (*rewind) (Genesys_Device * dev);

    SANE_Status (*bulk_write_register) (Genesys_Device * dev,
                                        Genesys_Register_Set& regs);

    SANE_Status (*bulk_write_data) (Genesys_Device * dev, uint8_t addr,
				    uint8_t * data, size_t len);

    void (*bulk_read_data) (Genesys_Device * dev, uint8_t addr, uint8_t * data, size_t len);

  // Updates hardware sensor information in Genesys_Scanner.val[].
  SANE_Status (*update_hardware_sensors) (struct Genesys_Scanner * s);

    /* functions for sheetfed scanners */
    /**
     * load document into scanner
     */
    SANE_Status (*load_document) (Genesys_Device * dev);
    /**
     * detects is the scanned document has left scanner. In this
     * case it updates the amount of data to read and set up
     * flags in the dev struct
     */
    SANE_Status (*detect_document_end) (Genesys_Device * dev);
    /**
     * eject document from scanner
     */
    SANE_Status (*eject_document) (Genesys_Device * dev);
    /**
     * search for an black or white area in forward or reverse
     * direction */
    SANE_Status (*search_strip) (Genesys_Device * dev, const Genesys_Sensor& sensor,
                                 SANE_Bool forward, SANE_Bool black);

    bool (*is_compatible_calibration) (Genesys_Device* dev, const Genesys_Sensor& sensor,
                                       Genesys_Calibration_Cache* cache, SANE_Bool for_overwrite);

    /* functions for transparency adapter */
    /**
     * move scanning head to transparency adapter
     */
    SANE_Status (*move_to_ta) (Genesys_Device * dev);

    /**
     * write shading data calibration to ASIC
     */
    SANE_Status (*send_shading_data) (Genesys_Device * dev, const Genesys_Sensor& sensor,
                                      uint8_t * data, int size);

    // calculate current scan setup
    void (*calculate_current_setup) (Genesys_Device * dev, const Genesys_Sensor& sensor);

    /**
     * cold boot init function
     */
    SANE_Status (*asic_boot) (Genesys_Device * dev, SANE_Bool cold);

} Genesys_Command_Set;

/** @brief structure to describe a scanner model
 * This structure describes a model. It is composed of information on the
 * sensor, the motor, scanner geometry and flags to drive operation.
 */
typedef struct Genesys_Model
{
  SANE_String_Const name;
  SANE_String_Const vendor;
  SANE_String_Const model;
  SANE_Int model_id;

  SANE_Int asic_type;		/* ASIC type gl646 or gl841 */
  Genesys_Command_Set *cmd_set;	/* pointers to low level functions */

  SANE_Int xdpi_values[MAX_RESOLUTIONS];	/* possible x resolutions */
  SANE_Int ydpi_values[MAX_RESOLUTIONS];	/* possible y resolutions */
  SANE_Int bpp_gray_values[MAX_DPI];	/* possible depths in gray mode */
  SANE_Int bpp_color_values[MAX_DPI];	/* possible depths in color mode */

    // All offsets below are with respect to the sensor home position
  SANE_Fixed x_offset;		/* Start of scan area in mm */
  SANE_Fixed y_offset;		/* Start of scan area in mm (Amount of
				   feeding needed to get to the medium) */
  SANE_Fixed x_size;		/* Size of scan area in mm */
  SANE_Fixed y_size;		/* Size of scan area in mm */

  SANE_Fixed y_offset_calib;	/* Start of white strip in mm */
  SANE_Fixed x_offset_mark;	/* Start of black mark in mm */

  SANE_Fixed x_offset_ta;	/* Start of scan area in TA mode in mm */
  SANE_Fixed y_offset_ta;	/* Start of scan area in TA mode in mm */
  SANE_Fixed x_size_ta;		/* Size of scan area in TA mode in mm */
  SANE_Fixed y_size_ta;		/* Size of scan area in TA mode in mm */

  // The position of the sensor when it's aligned with the lamp for transparency scanning
  SANE_Fixed y_offset_sensor_to_ta;

  SANE_Fixed y_offset_calib_ta;	/* Start of white strip in TA mode in mm */

  SANE_Fixed post_scan;		/* Size of scan area after paper sensor stops
				   sensing document in mm */
  SANE_Fixed eject_feed;	/* Amount of feeding needed to eject document
				   after finishing scanning in mm */

  /* Line-distance correction (in pixel at optical_ydpi) for CCD scanners */
  SANE_Int ld_shift_r;		/* red */
  SANE_Int ld_shift_g;		/* green */
  SANE_Int ld_shift_b;		/* blue */

  Genesys_Color_Order line_mode_color_order;	/* Order of the CCD/CIS colors */

  SANE_Bool is_cis;		/* Is this a CIS or CCD scanner? */
  SANE_Bool is_sheetfed;	/* Is this sheetfed scanner? */

  SANE_Int ccd_type;		/* which SENSOR type do we have ? */
  SANE_Int dac_type;		/* which DAC do we have ? */
  SANE_Int gpo_type;		/* General purpose output type */
  SANE_Int motor_type;		/* stepper motor type */
  SANE_Word flags;		/* Which hacks are needed for this scanner? */
  SANE_Word buttons;		/* Button flags, described existing buttons for the model */
  /*@} */
  SANE_Int shading_lines;	/* how many lines are used for shading calibration */
  SANE_Int shading_ta_lines; // how many lines are used for shading calibration in TA mode
  SANE_Int search_lines;	/* how many lines are used to search start position */
} Genesys_Model;

struct Genesys_Settings
{
    ScanMethod scan_method = ScanMethod::FLATBED;
    ScanColorMode scan_mode = ScanColorMode::LINEART;

    // horizontal dpi
    int xres = 0;
    // vertical dpi
    int yres = 0;

    //x start on scan table in mm
    double tl_x = 0;
    // y start on scan table in mm
    double tl_y = 0;

    // number of lines at scan resolution
    unsigned int lines = 0;
    // number of pixels at scan resolution
    unsigned int pixels = 0;

    // bit depth of the scan
    unsigned int depth = 0;

    ColorFilter color_filter = ColorFilter::NONE;

    // true if scan is true gray, false if monochrome scan
    int true_gray = 0;

    // lineart threshold
    int threshold = 0;

    // lineart threshold curve for dynamic rasterization
    int threshold_curve = 0;

    // Disable interpolation for xres<yres
    int disable_interpolation = 0;

    // true is lineart is generated from gray data by the dynamic rasterization algoright
    int dynamic_lineart = 0;

    // value for contrast enhancement in the [-100..100] range
    int contrast = 0;

    // value for brightness enhancement in the [-100..100] range
    int brightness = 0;

    // cache entries expiration time
    int expiration_time = 0;
};

struct SetupParams {

    static constexpr unsigned NOT_SET = std::numeric_limits<unsigned>::max();

    // resolution in x direction
    unsigned xres = NOT_SET;
    // resolution in y direction
    unsigned yres = NOT_SET;
    // start pixel in X direction, from dummy_pixel + 1
    float startx = -1;
    // start pixel in Y direction, counted according to base_ydpi
    float starty = -1;
    // the number of pixels in X direction. Note that each logical pixel may correspond to more
    // than one CCD pixel, see CKSEL and GenesysSensor::ccd_pixels_per_system_pixel()
    unsigned pixels = NOT_SET;
    // the number of pixels in Y direction
    unsigned lines = NOT_SET;
    // the depth of the scan in bits. Allowed are 1, 8, 16
    unsigned depth = NOT_SET;
    // the number of channels
    unsigned channels = NOT_SET;

    ScanMethod scan_method = static_cast<ScanMethod>(NOT_SET);

    ScanColorMode scan_mode = static_cast<ScanColorMode>(NOT_SET);

    ColorFilter color_filter = static_cast<ColorFilter>(NOT_SET);

    unsigned flags = NOT_SET;

    void assert_valid() const
    {
        if (xres == NOT_SET || yres == NOT_SET || startx < 0 || starty < 0 ||
            pixels == NOT_SET || lines == NOT_SET ||depth == NOT_SET || channels == NOT_SET ||
            scan_method == static_cast<ScanMethod>(NOT_SET) ||
            scan_mode == static_cast<ScanColorMode>(NOT_SET) ||
            color_filter == static_cast<ColorFilter>(NOT_SET) ||
            flags == NOT_SET)
        {
            throw std::runtime_error("SetupParams are not valid");
        }
    }

    bool operator==(const SetupParams& other) const
    {
        return xres == other.xres &&
            yres == other.yres &&
            startx == other.startx &&
            starty == other.starty &&
            pixels == other.pixels &&
            lines == other.lines &&
            depth == other.depth &&
            channels == other.channels &&
            scan_method == other.scan_method &&
            scan_mode == other.scan_mode &&
            color_filter == other.color_filter &&
            flags == other.flags;
    }
};

template<class Stream>
void serialize(Stream& str, SetupParams& x)
{
    serialize(str, x.xres);
    serialize(str, x.yres);
    serialize(str, x.startx);
    serialize(str, x.starty);
    serialize(str, x.pixels);
    serialize(str, x.lines);
    serialize(str, x.depth);
    serialize(str, x.channels);
    serialize(str, x.scan_method);
    serialize(str, x.scan_mode);
    serialize(str, x.color_filter);
    serialize(str, x.flags);
}

struct Genesys_Current_Setup
{
    // params used for this setup
    SetupParams params;

    // pixel count expected from scanner
    int pixels = 0;
    // line count expected from scanner
    int lines = 0;
    // depth expected from scanner
    int depth = 0;
    // channel count expected from scanner
    int channels = 0;

    // used exposure time
    int exposure_time = 0;
    // used xres
    float xres = 0;
    // used yres
    float yres = 0;
    // half ccd mode
    unsigned ccd_size_divisor = 1;
    SANE_Int stagger = 0;
    //  max shift of any ccd component, including staggered pixels
    SANE_Int max_shift = 0;

    bool operator==(const Genesys_Current_Setup& other) const
    {
        return params == other.params &&
            pixels == other.pixels &&
            lines == other.lines &&
            depth == other.depth &&
            channels == other.channels &&
            exposure_time == other.exposure_time &&
            xres == other.xres &&
            yres == other.yres &&
            ccd_size_divisor == other.ccd_size_divisor &&
            stagger == other.stagger &&
            max_shift == other.max_shift;
    }
};

template<class Stream>
void serialize(Stream& str, Genesys_Current_Setup& x)
{
    serialize(str, x.params);
    serialize_newline(str);
    serialize(str, x.pixels);
    serialize(str, x.lines);
    serialize(str, x.depth);
    serialize(str, x.channels);
    serialize(str, x.exposure_time);
    serialize(str, x.xres);
    serialize(str, x.yres);
    serialize(str, x.ccd_size_divisor);
    serialize(str, x.stagger);
    serialize(str, x.max_shift);
}

struct Genesys_Buffer
{
    Genesys_Buffer() = default;

    size_t size() const { return buffer_.size(); }
    size_t avail() const { return avail_; }
    size_t pos() const { return pos_; }

    // TODO: refactor code that uses this function to no longer use it
    void set_pos(size_t pos) { pos_ = pos; }

    void alloc(size_t size);
    void clear();

    void reset();

    uint8_t* get_write_pos(size_t size);
    uint8_t* get_read_pos(); // TODO: mark as const

    void produce(size_t size);
    void consume(size_t size);

private:
    std::vector<uint8_t> buffer_;
    // current position in read buffer
    size_t pos_ = 0;
    // data bytes currently in buffer
    size_t avail_ = 0;
};

struct Genesys_Calibration_Cache
{
    Genesys_Calibration_Cache() = default;
    ~Genesys_Calibration_Cache() = default;

    // used to check if entry is compatible
    Genesys_Current_Setup used_setup;
    time_t last_calibration = 0;

    Genesys_Frontend frontend;
    Genesys_Sensor sensor;

    size_t calib_pixels = 0;
    size_t calib_channels = 0;
    size_t average_size = 0;
    std::vector<uint8_t> white_average_data;
    std::vector<uint8_t> dark_average_data;

    bool operator==(const Genesys_Calibration_Cache& other) const
    {
        return used_setup == other.used_setup &&
            last_calibration == other.last_calibration &&
            frontend == other.frontend &&
            sensor == other.sensor &&
            calib_pixels == other.calib_pixels &&
            calib_channels == other.calib_channels &&
            average_size == other.average_size &&
            white_average_data == other.white_average_data &&
            dark_average_data == other.dark_average_data;
    }
};

template<class Stream>
void serialize(Stream& str, Genesys_Calibration_Cache& x)
{
    serialize(str, x.used_setup);
    serialize_newline(str);
    serialize(str, x.last_calibration);
    serialize_newline(str);
    serialize(str, x.frontend);
    serialize_newline(str);
    serialize(str, x.sensor);
    serialize_newline(str);
    serialize(str, x.calib_pixels);
    serialize(str, x.calib_channels);
    serialize(str, x.average_size);
    serialize_newline(str);
    serialize(str, x.white_average_data);
    serialize_newline(str);
    serialize(str, x.dark_average_data);
}

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

    UsbDevice usb_dev;
    SANE_Word vendorId = 0;			/**< USB vendor identifier */
    SANE_Word productId = 0;			/**< USB product identifier */

    // USB mode:
    // 0: not set
    // 1: USB 1.1
    // 2: USB 2.0
    SANE_Int usb_mode = 0;

    SANE_String file_name = nullptr;
    std::string calib_file;

    // if enabled, no calibration data will be loaded or saved to files
    SANE_Int force_calibration = 0;
    // if enabled, will ignore the scan offsets and start scanning at true origin. This allows
    // acquiring the positions of the black and white strips and the actual scan area
    bool ignore_offsets = false;

    Genesys_Model *model = nullptr;

    Genesys_Register_Set reg;
    Genesys_Register_Set calib_reg;
    Genesys_Settings settings;
    Genesys_Frontend frontend, frontend_initial;
    Genesys_Gpo gpo;
    Genesys_Motor motor;
    uint8_t  control[6] = {};
    time_t init_date = 0;

    size_t average_size = 0;
    // number of pixels used during shading calibration
    size_t calib_pixels = 0;
    // number of lines used during shading calibration
    size_t calib_lines = 0;
    size_t calib_channels = 0;
    size_t calib_resolution = 0;
     // bytes to read from USB when calibrating. If 0, this is not set
    size_t calib_total_bytes_to_read = 0;
    // certain scanners support much higher resolution when scanning transparency, but we can't
    // read whole width of the scanner as a single line at that resolution. Thus for stuff like
    // calibration we want to read only the possible calibration area.
    size_t calib_pixels_offset = 0;

    // gamma overrides. If a respective array is not empty then it means that the gamma for that
    // color is overridden.
    std::vector<uint16_t> gamma_override_tables[3];

    std::vector<uint8_t> white_average_data;
    std::vector<uint8_t> dark_average_data;
    uint16_t dark[3] = {};

    SANE_Bool already_initialized = 0;
    SANE_Int scanhead_position_in_steps = 0;
    SANE_Int lamp_off_time = 0;

    SANE_Bool read_active = 0;
    // signal wether the park command has been issued
    SANE_Bool parking = 0;

    // for sheetfed scanner's, is TRUE when there is a document in the scanner
    SANE_Bool document = 0;

    SANE_Bool needs_home_ta = 0;

    Genesys_Buffer read_buffer;
    Genesys_Buffer lines_buffer;
    Genesys_Buffer shrink_buffer;
    Genesys_Buffer out_buffer;

    // buffer for digital lineart from gray data
    Genesys_Buffer binarize_buffer = {};
    // local buffer for gray data during dynamix lineart
    Genesys_Buffer local_buffer = {};

    // bytes to read from scanner
    size_t read_bytes_left = 0;

    // total bytes read sent to frontend
    size_t total_bytes_read = 0;
    // total bytes read to be sent to frontend
    size_t total_bytes_to_read = 0;
    //  asic's word per line
    size_t wpl = 0;

    // contains the real used values
    Genesys_Current_Setup current_setup;

    // look up table used in dynamic rasterization
    unsigned char lineart_lut[256] = {};

    Calibration calibration_cache;

    // used red line-distance shift
    SANE_Int ld_shift_r = 0;
    // used green line-distance shift
    SANE_Int ld_shift_g = 0;
    // used blue line-distance shift
    SANE_Int ld_shift_b = 0;
    // number of segments composing the sensor
    int segnb = 0;
    // number of lines used in line interpolation
    int line_interp = 0;
    // number of scan lines used during scan
    int line_count = 0;
    // bytes per full scan widthline
    size_t bpl = 0;
    // bytes distance between an odd and an even pixel
    size_t dist = 0;
    // number of even pixels
    size_t len = 0;
    // current pixel position within sub window
    size_t cur = 0;
    // number of bytes to skip at start of line
    size_t skip = 0;

    // array describing the order of the sub-segments of the sensor
    size_t* order = nullptr;

    // buffer to handle even/odd data
    Genesys_Buffer oe_buffer = {};

    // when true the scanned picture is first buffered to allow software image enhancements
    SANE_Bool buffer_image = 0;

    // image buffer where the scanned picture is stored
    std::vector<uint8_t> img_buffer;

    // binary logger file
    FILE *binary = nullptr;
};

typedef struct Genesys_USB_Device_Entry
{
  SANE_Word vendor;			/**< USB vendor identifier */
  SANE_Word product;			/**< USB product identifier */
  Genesys_Model *model;			/**< Scanner model information */
} Genesys_USB_Device_Entry;

/**
 * structure for motor database
 */
typedef struct {
	int motor_type;	 /**< motor id */
	int exposure;    /**< exposure for the slope table */
        int step_type;   /**< default step type for given exposure */
        uint32_t *table;  // 0-terminated slope table at full step (i.e. step_type == 0)
} Motor_Profile;

#define FULL_STEP       0
#define HALF_STEP       1
#define QUARTER_STEP    2
#define EIGHTH_STEP     3

#define SLOPE_TABLE_SIZE 1024

#define SCAN_TABLE 	0 	/* table 1 at 0x4000 for gl124 */
#define BACKTRACK_TABLE 1 	/* table 2 at 0x4800 for gl124 */
#define STOP_TABLE 	2 	/* table 3 at 0x5000 for gl124 */
#define FAST_TABLE 	3 	/* table 4 at 0x5800 for gl124 */
#define HOME_TABLE 	4 	/* table 5 at 0x6000 for gl124 */

#define SCAN_FLAG_SINGLE_LINE               0x001
#define SCAN_FLAG_DISABLE_SHADING           0x002
#define SCAN_FLAG_DISABLE_GAMMA             0x004
#define SCAN_FLAG_DISABLE_BUFFER_FULL_MOVE  0x008
#define SCAN_FLAG_IGNORE_LINE_DISTANCE      0x010
#define SCAN_FLAG_USE_OPTICAL_RES           0x020
#define SCAN_FLAG_DISABLE_LAMP              0x040
#define SCAN_FLAG_DYNAMIC_LINEART           0x080
#define SCAN_FLAG_CALIBRATION               0x100
#define SCAN_FLAG_FEEDING                   0x200
#define SCAN_FLAG_USE_XPA                   0x400
#define SCAN_FLAG_ENABLE_LEDADD             0x800
#define MOTOR_FLAG_AUTO_GO_HOME             0x01
#define MOTOR_FLAG_DISABLE_BUFFER_FULL_MOVE 0x02
#define MOTOR_FLAG_FEED                     0x04
#define MOTOR_FLAG_USE_XPA                  0x08

/** @name "Optical flags" */
/*@{ optical flags available when setting up sensor for scan */

#define OPTICAL_FLAG_DISABLE_GAMMA          0x01 /**< disable gamma correction */
#define OPTICAL_FLAG_DISABLE_SHADING        0x02 /**< disable shading correction */
#define OPTICAL_FLAG_DISABLE_LAMP           0x04 /**< turn off lamp */
#define OPTICAL_FLAG_ENABLE_LEDADD          0x08 /**< enable true CIS gray by enabling LED addition */
#define OPTICAL_FLAG_DISABLE_DOUBLE         0x10 /**< disable automatic x-direction double data expansion */
#define OPTICAL_FLAG_STAGGER                0x20 /**< disable stagger correction */
#define OPTICAL_FLAG_USE_XPA                0x40 /**< use XPA lamp rather than regular one */

/*@} */

/*--------------------------------------------------------------------------*/
/*       common functions needed by low level specific functions            */
/*--------------------------------------------------------------------------*/

inline GenesysRegister* sanei_genesys_get_address(Genesys_Register_Set* regs, uint16_t addr)
{
    auto* ret = regs->find_reg_address(addr);
    if (ret == nullptr) {
        DBG(DBG_error, "%s: failed to find address for register 0x%02x, crash expected !\n",
            __func__, addr);
    }
    return ret;
}

inline uint8_t sanei_genesys_read_reg_from_set(Genesys_Register_Set* regs, uint16_t address)
{
    return regs->get8(address);
}

inline void sanei_genesys_set_reg_from_set(Genesys_Register_Set* regs, uint16_t address,
                                           uint8_t value)
{
    regs->set8(address, value);
}

extern SANE_Status sanei_genesys_init_cmd_set (Genesys_Device * dev);

extern SANE_Status
sanei_genesys_read_register (Genesys_Device * dev, uint16_t reg, uint8_t * val);

extern SANE_Status
sanei_genesys_write_register (Genesys_Device * dev, uint16_t reg, uint8_t val);

extern void sanei_genesys_read_hregister(Genesys_Device* dev, uint16_t reg, uint8_t* val);

extern void sanei_genesys_write_hregister(Genesys_Device* dev, uint16_t reg, uint8_t val);

extern SANE_Status
sanei_genesys_bulk_write_register(Genesys_Device * dev,
                                   Genesys_Register_Set& regs);

extern void sanei_genesys_write_0x8c(Genesys_Device* dev, uint8_t index, uint8_t val);

extern unsigned sanei_genesys_get_bulk_max_size(Genesys_Device * dev);

extern void sanei_genesys_bulk_read_data(Genesys_Device * dev, uint8_t addr, uint8_t* data,
                                         size_t len);

extern SANE_Status sanei_genesys_bulk_write_data(Genesys_Device * dev, uint8_t addr, uint8_t* data,
                                                 size_t len);

extern SANE_Status sanei_genesys_get_status (Genesys_Device * dev, uint8_t * status);

extern void sanei_genesys_print_status (uint8_t val);

extern SANE_Status
sanei_genesys_write_ahb(Genesys_Device* dev, uint32_t addr, uint32_t size, uint8_t * data);

extern void sanei_genesys_init_structs (Genesys_Device * dev);

const Genesys_Sensor& sanei_genesys_find_sensor_any(Genesys_Device* dev);
Genesys_Sensor& sanei_genesys_find_sensor_any_for_write(Genesys_Device* dev);
const Genesys_Sensor& sanei_genesys_find_sensor(Genesys_Device* dev, int dpi,
                                                ScanMethod scan_method);
Genesys_Sensor& sanei_genesys_find_sensor_for_write(Genesys_Device* dev, int dpi,
                                                    ScanMethod scan_method);

extern SANE_Status
sanei_genesys_init_shading_data (Genesys_Device * dev, const Genesys_Sensor& sensor,
                                 int pixels_per_line);

extern SANE_Status sanei_genesys_read_valid_words (Genesys_Device * dev,
						  unsigned int *steps);

extern SANE_Status sanei_genesys_read_scancnt (Genesys_Device * dev,
						  unsigned int *steps);

extern SANE_Status sanei_genesys_read_feed_steps (Genesys_Device * dev,
						  unsigned int *steps);

void sanei_genesys_set_lamp_power(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                  Genesys_Register_Set& regs, bool set);

void sanei_genesys_set_motor_power(Genesys_Register_Set& regs, bool set);

extern void
sanei_genesys_calculate_zmode2 (SANE_Bool two_table,
				uint32_t exposure_time,
				uint16_t * slope_table,
				int reg21,
				int move, int reg22, uint32_t * z1,
				uint32_t * z2);

extern void
sanei_genesys_calculate_zmode (uint32_t exposure_time,
			       uint32_t steps_sum,
			       uint16_t last_speed, uint32_t feedl,
			       uint8_t fastfed, uint8_t scanfed,
			       uint8_t fwdstep, uint8_t tgtime,
			       uint32_t * z1, uint32_t * z2);

extern SANE_Status
sanei_genesys_set_buffer_address (Genesys_Device * dev, uint32_t addr);

/** @brief Reads data from frontend register.
 * Reads data from the given frontend register. May be used to query
 * analog frontend status by reading the right register.
 */
extern SANE_Status
sanei_genesys_fe_read_data (Genesys_Device * dev, uint8_t addr,
			    uint16_t *data);
/** @brief Write data to frontend register.
 * Writes data to analog frontend register at the given address.
 * The use and address of registers change from model to model.
 */
extern SANE_Status
sanei_genesys_fe_write_data (Genesys_Device * dev, uint8_t addr,
			     uint16_t data);

extern SANE_Int
sanei_genesys_exposure_time2 (Genesys_Device * dev,
			      float ydpi, int step_type, int endpixel,
			      int led_exposure, int power_mode);

extern SANE_Int
sanei_genesys_exposure_time (Genesys_Device * dev, Genesys_Register_Set * reg,
			     int xdpi);
extern SANE_Int
sanei_genesys_generate_slope_table (uint16_t * slope_table, unsigned int max_steps,
			      unsigned int use_steps, uint16_t stop_at,
			      uint16_t vstart, uint16_t vend,
			      unsigned int steps, double g,
			      unsigned int *used_steps, unsigned int *vfinal);

extern SANE_Int
sanei_genesys_create_slope_table (Genesys_Device * dev,
				  uint16_t * slope_table, int steps,
				  int step_type, int exposure_time,
				  SANE_Bool same_speed, double yres,
				  int power_mode);

SANE_Int
sanei_genesys_create_slope_table3 (Genesys_Device * dev,
				   uint16_t * slope_table, int max_step,
				   unsigned int use_steps,
				   int step_type, int exposure_time,
				   double yres,
				   unsigned int *used_steps,
				   unsigned int *final_exposure,
				   int power_mode);

void sanei_genesys_create_default_gamma_table(Genesys_Device* dev,
                                              std::vector<uint16_t>& gamma_table, float gamma);

std::vector<uint16_t> get_gamma_table(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                      int color);

SANE_Status sanei_genesys_send_gamma_table(Genesys_Device * dev, const Genesys_Sensor& sensor);

extern SANE_Status sanei_genesys_start_motor (Genesys_Device * dev);

extern SANE_Status sanei_genesys_stop_motor (Genesys_Device * dev);

extern SANE_Status
sanei_genesys_search_reference_point(Genesys_Device * dev, Genesys_Sensor& sensor,
                                     uint8_t * data,
                                     int start_pixel, int dpi, int width,
                                     int height);

extern SANE_Status sanei_genesys_write_file(const char *filename, uint8_t* data, size_t length);

extern SANE_Status
sanei_genesys_write_pnm_file (const char *filename, uint8_t * data, int depth,
			      int channels, int pixels_per_line, int lines);

extern SANE_Status
sanei_genesys_test_buffer_empty (Genesys_Device * dev, SANE_Bool * empty);

extern SANE_Status
sanei_genesys_read_data_from_scanner (Genesys_Device * dev, uint8_t * data,
				      size_t size);

inline void sanei_genesys_set_double(Genesys_Register_Set* regs, uint16_t addr, uint16_t value)
{
    regs->set16(addr, value);
}

inline void sanei_genesys_set_triple(Genesys_Register_Set* regs, uint16_t addr, uint32_t value)
{
    regs->set24(addr, value);
}

inline void sanei_genesys_get_double(Genesys_Register_Set* regs, uint16_t addr, uint16_t* value)
{
    *value = regs->get16(addr);
}

inline void sanei_genesys_get_triple(Genesys_Register_Set* regs, uint16_t addr, uint32_t* value)
{
    *value = regs->get24(addr);
}

inline void sanei_genesys_set_exposure(Genesys_Register_Set& regs, const SensorExposure& exposure)
{
    regs.set8(0x10, (exposure.red >> 8) & 0xff);
    regs.set8(0x11, exposure.red & 0xff);
    regs.set8(0x12, (exposure.green >> 8) & 0xff);
    regs.set8(0x13, exposure.green & 0xff);
    regs.set8(0x14, (exposure.blue >> 8) & 0xff);
    regs.set8(0x15, exposure.blue & 0xff);
}

inline uint16_t sanei_genesys_fixup_exposure_value(uint16_t value)
{
    if ((value & 0xff00) == 0) {
        value |= 0x100;
    }
    if ((value & 0x00ff) == 0) {
        value |= 0x1;
    }
    return value;
}

inline SensorExposure sanei_genesys_fixup_exposure(SensorExposure exposure)
{
    exposure.red = sanei_genesys_fixup_exposure_value(exposure.red);
    exposure.green = sanei_genesys_fixup_exposure_value(exposure.green);
    exposure.blue = sanei_genesys_fixup_exposure_value(exposure.blue);
    return exposure;
}

extern SANE_Status
sanei_genesys_wait_for_home(Genesys_Device *dev);

extern SANE_Status
sanei_genesys_asic_init(Genesys_Device *dev, SANE_Bool cold);

int sanei_genesys_compute_dpihw(Genesys_Device *dev, const Genesys_Sensor& sensor, int xres);

int sanei_genesys_compute_dpihw_calibration(Genesys_Device *dev, const Genesys_Sensor& sensor,
                                            int xres);

extern
Motor_Profile *sanei_genesys_get_motor_profile(Motor_Profile *motors, int motor_type, int exposure);

extern
int sanei_genesys_compute_step_type(Motor_Profile *motors, int motor_type, int exposure);

extern
int sanei_genesys_slope_table(uint16_t *slope, int *steps, int dpi, int exposure, int base_dpi, int step_type, int factor, int motor_type, Motor_Profile *motors);

/** @brief find lowest motor resolution for the device.
 * Parses the resolution list for motor and
 * returns the lowest value.
 * @param dev for which to find the lowest motor resolution
 * @return the lowest available motor resolution for the device
 */
extern
int sanei_genesys_get_lowest_ydpi(Genesys_Device *dev);

/** @brief find lowest resolution for the device.
 * Parses the resolution list for motor and sensor and
 * returns the lowest value.
 * @param dev for which to find the lowest resolution
 * @return the lowest available resolution for the device
 */
extern
int sanei_genesys_get_lowest_dpi(Genesys_Device *dev);

extern bool
sanei_genesys_is_compatible_calibration (Genesys_Device * dev, const Genesys_Sensor& sensor,
				 Genesys_Calibration_Cache * cache,
				 int for_overwrite);

/** @brief compute maximum line distance shift
 * compute maximum line distance shift for the motor and sensor
 * combination. Line distance shift is the distance between different
 * color component of CCD sensors. Since these components aren't at
 * the same physical place, they scan diffrent lines. Software must
 * take this into account to accurately mix color data.
 * @param dev device session to compute max_shift for
 * @param channels number of color channels for the scan
 * @param yres motor resolution used for the scan
 * @param flags scan flags
 * @return 0 or line distance shift
 */
extern
int sanei_genesys_compute_max_shift(Genesys_Device *dev,
                                    int channels,
                                    int yres,
                                    int flags);

extern SANE_Status
sanei_genesys_load_lut (unsigned char * lut,
                        int in_bits,
                        int out_bits,
                        int out_min,
                        int out_max,
                        int slope,
                        int offset);

extern SANE_Status
sanei_genesys_generate_gamma_buffer(Genesys_Device * dev,
                                    const Genesys_Sensor& sensor,
                                    int bits,
                                    int max,
                                    int size,
                                    uint8_t *gamma);

/*---------------------------------------------------------------------------*/
/*                ASIC specific functions declarations                       */
/*---------------------------------------------------------------------------*/
extern SANE_Status sanei_gl646_init_cmd_set (Genesys_Device * dev);
extern SANE_Status sanei_gl841_init_cmd_set (Genesys_Device * dev);
extern SANE_Status sanei_gl843_init_cmd_set (Genesys_Device * dev);
extern SANE_Status sanei_gl846_init_cmd_set (Genesys_Device * dev);
extern SANE_Status sanei_gl847_init_cmd_set (Genesys_Device * dev);
extern SANE_Status sanei_gl124_init_cmd_set (Genesys_Device * dev);

// same as usleep, except that it does nothing if testing mode is enabled
extern void sanei_genesys_usleep(unsigned int useconds);

// same as sanei_genesys_usleep just that the duration is in milliseconds
extern void sanei_genesys_sleep_ms(unsigned int milliseconds);

void add_function_to_run_at_backend_exit(std::function<void()> function);

// calls functions added via add_function_to_run_at_backend_exit() in reverse order of being
// added.
void run_functions_at_backend_exit();

template<class T>
class StaticInit {
public:
    StaticInit() = default;
    StaticInit(const StaticInit&) = delete;
    StaticInit& operator=(const StaticInit&) = delete;

    template<class... Args>
    void init(Args&& ... args)
    {
        ptr_ = std::unique_ptr<T>(new T(std::forward<Args>(args)...));
        add_function_to_run_at_backend_exit([this](){ deinit(); });
    }

    void deinit()
    {
        ptr_.release();
    }

    const T* operator->() const { return ptr_.get(); }
    T* operator->() { return ptr_.get(); }
    const T& operator*() const { return *ptr_.get(); }
    T& operator*() { return *ptr_.get(); }

private:
    std::unique_ptr<T> ptr_;
};

extern StaticInit<std::vector<Genesys_Sensor>> s_sensors;
void genesys_init_sensor_tables();
void genesys_init_frontend_tables();

void debug_dump(unsigned level, const Genesys_Settings& settings);
void debug_dump(unsigned level, const SetupParams& params);
void debug_dump(unsigned level, const Genesys_Current_Setup& setup);
void debug_dump(unsigned level, const Genesys_Register_Set& regs);

#endif /* not GENESYS_LOW_H */
