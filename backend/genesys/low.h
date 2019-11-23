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

#include "device.h"
#include "enums.h"
#include "error.h"
#include "fwd.h"
#include "usb_device.h"
#include "sensor.h"
#include "serialize.h"
#include "settings.h"
#include "register.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <functional>
#include <iostream>
#include <sstream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#define GENESYS_RED   0
#define GENESYS_GREEN 1
#define GENESYS_BLUE  2

/* Flags */
#define GENESYS_FLAG_UNTESTED     (1 << 0)	/**< Print a warning for these scanners */
#define GENESYS_FLAG_14BIT_GAMMA  (1 << 1)	/**< use 14bit Gamma table instead of 12 */
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
// scanner calibration is handled on the host side
#define GENESYS_FLAG_CALIBRATION_HOST_SIDE (1 << 21)
#define GENESYS_FLAG_16BIT_DATA_INVERTED (1 << 22)

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

#define MAX_RESOLUTIONS 13
#define MAX_DPI 4

namespace genesys {

struct Genesys_USB_Device_Entry {

    Genesys_USB_Device_Entry(unsigned v, unsigned p, const Genesys_Model& m) :
        vendor(v), product(p), model(m)
    {}

    // USB vendor identifier
    std::uint16_t vendor;
    // USB product identifier
    std::uint16_t product;
    // Scanner model information
    Genesys_Model model;
};

/**
 * structure for motor database
 */
struct Motor_Profile
{
    MotorId motor_id;
	int exposure;    /**< exposure for the slope table */
    StepType step_type;   // default step type for given exposure
        uint32_t *table;  // 0-terminated slope table at full step (i.e. step_type == 0)
};

extern Motor_Profile gl843_motor_profiles[];
extern Motor_Profile gl846_motor_profiles[];
extern Motor_Profile gl847_motor_profiles[];
extern Motor_Profile gl124_motor_profiles[];

constexpr unsigned SLOPE_TABLE_SIZE = 1024;

#define SCAN_FLAG_SINGLE_LINE               0x001
#define SCAN_FLAG_DISABLE_SHADING           0x002
#define SCAN_FLAG_DISABLE_GAMMA             0x004
#define SCAN_FLAG_DISABLE_BUFFER_FULL_MOVE  0x008
#define SCAN_FLAG_IGNORE_LINE_DISTANCE      0x010
#define SCAN_FLAG_DISABLE_LAMP              0x040
#define SCAN_FLAG_CALIBRATION               0x100
#define SCAN_FLAG_FEEDING                   0x200
#define SCAN_FLAG_USE_XPA                   0x400
#define SCAN_FLAG_ENABLE_LEDADD             0x800
#define SCAN_FLAG_USE_XCORRECTION          0x1000
#define MOTOR_FLAG_AUTO_GO_HOME             0x01
#define MOTOR_FLAG_DISABLE_BUFFER_FULL_MOVE 0x02
#define MOTOR_FLAG_FEED                     0x04
#define MOTOR_FLAG_USE_XPA                  0x08

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

extern void sanei_genesys_init_cmd_set(Genesys_Device* dev);

std::uint8_t sanei_genesys_get_status(Genesys_Device* dev);

extern void sanei_genesys_print_status (uint8_t val);

extern void sanei_genesys_write_ahb(Genesys_Device* dev, uint32_t addr, uint32_t size,
                                    uint8_t* data);

extern void sanei_genesys_init_structs (Genesys_Device * dev);

const Genesys_Sensor& sanei_genesys_find_sensor_any(Genesys_Device* dev);
const Genesys_Sensor& sanei_genesys_find_sensor(Genesys_Device* dev, unsigned dpi,
                                                unsigned channels, ScanMethod scan_method);
bool sanei_genesys_has_sensor(Genesys_Device* dev, unsigned dpi, unsigned channels,
                              ScanMethod scan_method);
Genesys_Sensor& sanei_genesys_find_sensor_for_write(Genesys_Device* dev, unsigned dpi,
                                                    unsigned channels, ScanMethod scan_method);

std::vector<std::reference_wrapper<const Genesys_Sensor>>
    sanei_genesys_find_sensors_all(Genesys_Device* dev, ScanMethod scan_method);
std::vector<std::reference_wrapper<Genesys_Sensor>>
    sanei_genesys_find_sensors_all_for_write(Genesys_Device* dev, ScanMethod scan_method);

extern void sanei_genesys_init_shading_data(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                            int pixels_per_line);

extern void sanei_genesys_read_valid_words(Genesys_Device* dev, unsigned int* steps);

extern void sanei_genesys_read_scancnt(Genesys_Device* dev, unsigned int* steps);

extern void sanei_genesys_read_feed_steps(Genesys_Device* dev, unsigned int* steps);

void sanei_genesys_set_lamp_power(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                  Genesys_Register_Set& regs, bool set);

void sanei_genesys_set_motor_power(Genesys_Register_Set& regs, bool set);

bool should_enable_gamma(const ScanSession& session, const Genesys_Sensor& sensor);

/** Calculates the values of the Z{1,2}MOD registers. They are a phase correction to synchronize
    with the line clock during acceleration and deceleration.

    two_table is true if moving is done by two tables, false otherwise.

    acceleration_steps is the number of steps for acceleration, i.e. the number written to
    REG_STEPNO.

    move_steps number of steps to move, i.e. the number written to REG_FEEDL.

    buffer_acceleration_steps, the number of steps for acceleration when buffer condition is met,
    i.e. the number written to REG_FWDSTEP.
*/
void sanei_genesys_calculate_zmod(bool two_table,
                                  uint32_t exposure_time,
                                  const std::vector<uint16_t>& slope_table,
                                  unsigned acceleration_steps,
                                  unsigned move_steps,
                                  unsigned buffer_acceleration_steps,
                                  uint32_t* out_z1, uint32_t* out_z2);

extern void sanei_genesys_set_buffer_address(Genesys_Device* dev, uint32_t addr);

unsigned sanei_genesys_get_bulk_max_size(AsicType asic_type);

SANE_Int sanei_genesys_exposure_time2(Genesys_Device * dev, float ydpi, StepType step_type,
                                      int endpixel, int led_exposure);

SANE_Int sanei_genesys_generate_slope_table(std::vector<uint16_t>& slope_table, unsigned int max_steps,
			      unsigned int use_steps, uint16_t stop_at,
			      uint16_t vstart, uint16_t vend,
			      unsigned int steps, double g,
			      unsigned int *used_steps, unsigned int *vfinal);

SANE_Int sanei_genesys_create_slope_table3(const Genesys_Motor& motor,
                                           std::vector<uint16_t>& slope_table, int max_step,
				   unsigned int use_steps,
                                           StepType step_type, int exposure_time,
                                           unsigned yres,
				   unsigned int *used_steps,
                                   unsigned int *final_exposure);

void sanei_genesys_create_default_gamma_table(Genesys_Device* dev,
                                              std::vector<uint16_t>& gamma_table, float gamma);

std::vector<uint16_t> get_gamma_table(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                      int color);

void sanei_genesys_send_gamma_table(Genesys_Device* dev, const Genesys_Sensor& sensor);

extern void sanei_genesys_stop_motor(Genesys_Device* dev);

extern void sanei_genesys_search_reference_point(Genesys_Device* dev, Genesys_Sensor& sensor,
                                                 const uint8_t* src_data, int start_pixel, int dpi,
                                                 int width, int height);

extern void sanei_genesys_write_file(const char* filename, const std::uint8_t* data,
                                     std::size_t length);

extern void sanei_genesys_write_pnm_file(const char* filename, const std::uint8_t* data, int depth,
                                         int channels, int pixels_per_line, int lines);

void sanei_genesys_write_pnm_file(const char* filename, const Image& image);

extern void sanei_genesys_write_pnm_file16(const char* filename, const uint16_t *data, unsigned channels,
                                           unsigned pixels_per_line, unsigned lines);

void wait_until_buffer_non_empty(Genesys_Device* dev, bool check_status_twice = false);

extern void sanei_genesys_read_data_from_scanner(Genesys_Device* dev, uint8_t* data, size_t size);

Image read_unshuffled_image_from_scanner(Genesys_Device* dev, const ScanSession& session,
                                         std::size_t total_bytes);

inline void sanei_genesys_set_exposure(Genesys_Register_Set& regs, const SensorExposure& exposure)
{
    regs.set8(0x10, (exposure.red >> 8) & 0xff);
    regs.set8(0x11, exposure.red & 0xff);
    regs.set8(0x12, (exposure.green >> 8) & 0xff);
    regs.set8(0x13, exposure.green & 0xff);
    regs.set8(0x14, (exposure.blue >> 8) & 0xff);
    regs.set8(0x15, exposure.blue & 0xff);
}

void sanei_genesys_set_dpihw(Genesys_Register_Set& regs, const Genesys_Sensor& sensor,
                             unsigned dpihw);

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

extern void sanei_genesys_wait_for_home(Genesys_Device* dev);

extern void sanei_genesys_asic_init(Genesys_Device* dev, bool cold);

Motor_Profile* sanei_genesys_get_motor_profile(Motor_Profile *motors, MotorId motor_id,
                                               int exposure);

int sanei_genesys_slope_table(std::vector<uint16_t>& slope, int *steps, int dpi, int exposure,
                              int base_dpi, int factor, const Motor_Profile& motor_profile);

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

bool sanei_genesys_is_compatible_calibration(Genesys_Device* dev,
                                             const ScanSession& session,
                                             const Genesys_Calibration_Cache* cache,
                                             bool for_overwrite);

extern void sanei_genesys_load_lut(unsigned char* lut,
                                   int in_bits, int out_bits,
                                   int out_min, int out_max,
                                   int slope, int offset);

extern void sanei_genesys_generate_gamma_buffer(Genesys_Device* dev,
                                    const Genesys_Sensor& sensor,
                                    int bits,
                                    int max,
                                    int size,
                                    uint8_t* gamma);

void compute_session(const Genesys_Device* dev, ScanSession& s, const Genesys_Sensor& sensor);

void build_image_pipeline(Genesys_Device* dev, const ScanSession& session);

std::uint8_t compute_frontend_gain(float value, float target_value,
                                   FrontendType frontend_type);

const SensorProfile& get_sensor_profile(AsicType asic_type, const Genesys_Sensor& sensor,
                                        unsigned dpi, unsigned ccd_size_divisor);

template<class T>
inline T abs_diff(T a, T b)
{
    if (a < b) {
        return b - a;
    } else {
        return a - b;
    }
}

inline uint64_t align_multiple_floor(uint64_t x, uint64_t multiple)
{
    return (x / multiple) * multiple;
}

inline uint64_t align_multiple_ceil(uint64_t x, uint64_t multiple)
{
    return ((x + multiple - 1) / multiple) * multiple;
}

inline uint64_t multiply_by_depth_ceil(uint64_t pixels, uint64_t depth)
{
    if (depth == 1) {
        return (pixels / 8) + ((pixels % 8) ? 1 : 0);
    } else {
        return pixels * (depth / 8);
    }
}

template<class T>
inline T clamp(const T& value, const T& lo, const T& hi)
{
    if (value < lo)
        return lo;
    if (value > hi)
        return hi;
    return value;
}

/*---------------------------------------------------------------------------*/
/*                ASIC specific functions declarations                       */
/*---------------------------------------------------------------------------*/

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
        ptr_.reset();
    }

    const T* operator->() const { return ptr_.get(); }
    T* operator->() { return ptr_.get(); }
    const T& operator*() const { return *ptr_.get(); }
    T& operator*() { return *ptr_.get(); }

private:
    std::unique_ptr<T> ptr_;
};

extern StaticInit<std::vector<Genesys_Sensor>> s_sensors;
extern StaticInit<SensorProfile> s_fallback_sensor_profile_gl124;
extern StaticInit<SensorProfile> s_fallback_sensor_profile_gl846;
extern StaticInit<SensorProfile> s_fallback_sensor_profile_gl847;
extern StaticInit<std::vector<Genesys_Frontend>> s_frontends;
extern StaticInit<std::vector<Genesys_Gpo>> s_gpo;
extern StaticInit<std::vector<Genesys_Motor>> s_motors;
extern StaticInit<std::vector<Genesys_USB_Device_Entry>> s_usb_devices;

void genesys_init_sensor_tables();
void genesys_init_frontend_tables();
void genesys_init_gpo_tables();
void genesys_init_motor_tables();
void genesys_init_usb_device_tables();

template<class T>
void debug_dump(unsigned level, const T& value)
{
    std::stringstream out;
    out << value;
    DBG(level, "%s\n", out.str().c_str());
}

} // namespace genesys

#endif /* not GENESYS_LOW_H */
