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

#ifndef BACKEND_GENESYS_COMMAND_SET_H
#define BACKEND_GENESYS_COMMAND_SET_H

#include "genesys_device.h"
#include <cstdint>

class Genesys_Device;

/** Scanner command set description.

    This description contains parts which are common to all scanners with the
    same command set, but may have different optical resolution and other
    parameters.
 */
class CommandSet
{
public:
    virtual ~CommandSet() = default;

    virtual bool needs_home_before_init_regs_for_scan(Genesys_Device* dev) const = 0;

    virtual void init(Genesys_Device* dev) const = 0;

    virtual void init_regs_for_warmup(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                      Genesys_Register_Set* regs, int* channels,
                                      int* total_size) const = 0;

    virtual void init_regs_for_coarse_calibration(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                                  Genesys_Register_Set& regs) const = 0;
    virtual void init_regs_for_shading(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                       Genesys_Register_Set& regs) const = 0;
    virtual void init_regs_for_scan(Genesys_Device* dev, const Genesys_Sensor& sensor) const = 0;

    virtual bool get_bitset_bit(Genesys_Register_Set * reg) const = 0;
    virtual bool get_gain4_bit(Genesys_Register_Set * reg) const = 0;

    virtual bool test_buffer_empty_bit(std::uint8_t val) const = 0;

    virtual void set_fe(Genesys_Device* dev, const Genesys_Sensor& sensor, uint8_t set) const = 0;
    virtual void set_powersaving(Genesys_Device* dev, int delay) const = 0;
    virtual void save_power(Genesys_Device* dev, bool enable) const = 0;

    virtual void begin_scan(Genesys_Device* dev, const Genesys_Sensor& sensor,
                            Genesys_Register_Set* regs, bool start_motor) const = 0;
    virtual void end_scan(Genesys_Device* dev, Genesys_Register_Set* regs,
                          bool check_stop) const = 0;

    /**
     * Send gamma tables to ASIC
     */
    virtual void send_gamma_table(Genesys_Device* dev, const Genesys_Sensor& sensor) const = 0;

    virtual void search_start_position(Genesys_Device* dev) const = 0;
    virtual void offset_calibration(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                    Genesys_Register_Set& regs) const = 0;
    virtual void coarse_gain_calibration(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                         Genesys_Register_Set& regs, int dpi) const = 0;
    virtual SensorExposure led_calibration(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                           Genesys_Register_Set& regs) const = 0;

    virtual void wait_for_motor_stop(Genesys_Device* dev) const = 0;
    virtual void slow_back_home(Genesys_Device* dev, bool wait_until_home) const = 0;
    virtual void rewind(Genesys_Device* dev) const = 0;

    virtual bool has_rewind() const { return true; }

    virtual void bulk_write_data(Genesys_Device* dev, uint8_t addr, uint8_t* data,
                                 size_t len) const = 0;
    virtual void bulk_read_data(Genesys_Device * dev, uint8_t addr, uint8_t* data,
                                size_t len) const = 0;

    // Updates hardware sensor information in Genesys_Scanner.val[].
    virtual void update_hardware_sensors(struct Genesys_Scanner* s) const = 0;

    // functions for sheetfed scanners

    // load document into scanner
    virtual void load_document(Genesys_Device* dev) const = 0;

    /** Detects is the scanned document has left scanner. In this case it updates the amount of
        data to read and set up flags in the dev struct
     */
    virtual void detect_document_end(Genesys_Device* dev) const = 0;

    /// eject document from scanner
    virtual void eject_document(Genesys_Device* dev) const = 0;
    /**
     * search for an black or white area in forward or reverse
     * direction */
    virtual void search_strip(Genesys_Device* dev, const Genesys_Sensor& sensor,
                              bool forward, bool black) const = 0;

    virtual bool is_compatible_calibration(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                           Genesys_Calibration_Cache* cache,
                                           bool for_overwrite) const = 0;

    /// move scanning head to transparency adapter
    virtual void move_to_ta(Genesys_Device* dev) const = 0;

    /// write shading data calibration to ASIC
    virtual void send_shading_data(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                   uint8_t* data, int size) const = 0;

    virtual bool has_send_shading_data() const
    {
        return true;
    }

    /// calculate current scan setup
    virtual void calculate_current_setup(Genesys_Device* dev,
                                         const Genesys_Sensor& sensor) const = 0;

    virtual bool has_calculate_current_setup() const
    {
        return true;
    }

    /// cold boot init function
    virtual void asic_boot(Genesys_Device* dev, bool cold) const = 0;
};


#endif // BACKEND_GENESYS_COMMAND_SET_H
