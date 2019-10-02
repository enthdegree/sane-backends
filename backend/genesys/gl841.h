/* sane - Scanner Access Now Easy.

   Copyright (C) 2011-2013 Stéphane Voltz <stef.dev@free.fr>

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

#include "genesys.h"
#include "command_set.h"

#ifndef BACKEND_GENESYS_GL841_H
#define BACKEND_GENESYS_GL841_H

#define INITREG(adr,val) {dev->reg.init_reg(adr, val); }

namespace genesys {

/**
 * prototypes declaration in case of unit testing
 */

static
int gl841_exposure_time(Genesys_Device *dev, const Genesys_Sensor& sensor,
                    float slope_dpi,
                    int scan_step_type,
                    int start,
                    int used_pixels);

class CommandSetGl841 : public CommandSet
{
public:
    ~CommandSetGl841() override = default;

    bool needs_home_before_init_regs_for_scan(Genesys_Device* dev) const override;

    void init(Genesys_Device* dev) const override;

    void init_regs_for_warmup(Genesys_Device* dev, const Genesys_Sensor& sensor,
                              Genesys_Register_Set* regs, int* channels,
                              int* total_size) const override;

    void init_regs_for_coarse_calibration(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                          Genesys_Register_Set& regs) const override;

    void init_regs_for_shading(Genesys_Device* dev, const Genesys_Sensor& sensor,
                               Genesys_Register_Set& regs) const override;

    void init_regs_for_scan(Genesys_Device* dev, const Genesys_Sensor& sensor) const override;

    bool get_gain4_bit(Genesys_Register_Set* reg) const override;

    bool test_buffer_empty_bit(std::uint8_t val) const override;

    void set_fe(Genesys_Device* dev, const Genesys_Sensor& sensor, uint8_t set) const override;
    void set_powersaving(Genesys_Device* dev, int delay) const override;
    void save_power(Genesys_Device* dev, bool enable) const override;

    void begin_scan(Genesys_Device* dev, const Genesys_Sensor& sensor,
                    Genesys_Register_Set* regs, bool start_motor) const override;

    void end_scan(Genesys_Device* dev, Genesys_Register_Set* regs, bool check_stop) const override;

    void send_gamma_table(Genesys_Device* dev, const Genesys_Sensor& sensor) const override;

    void search_start_position(Genesys_Device* dev) const override;

    void offset_calibration(Genesys_Device* dev, const Genesys_Sensor& sensor,
                            Genesys_Register_Set& regs) const override;

    void coarse_gain_calibration(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                 Genesys_Register_Set& regs, int dpi) const override;

    SensorExposure led_calibration(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                   Genesys_Register_Set& regs) const override;

    void wait_for_motor_stop(Genesys_Device* dev) const override;
    void slow_back_home(Genesys_Device* dev, bool wait_until_home) const override;
    void rewind(Genesys_Device* dev) const override;

    bool has_rewind() const override { return false; }

    void bulk_write_data(Genesys_Device* dev, uint8_t addr, uint8_t* data,
                         size_t len) const override;
    void bulk_read_data(Genesys_Device * dev, uint8_t addr, uint8_t * data,
                        size_t len) const override;

    void update_hardware_sensors(struct Genesys_Scanner* s) const override;

    void load_document(Genesys_Device* dev) const override;

    void detect_document_end(Genesys_Device* dev) const override;

    void eject_document(Genesys_Device* dev) const override;

    void search_strip(Genesys_Device* dev, const Genesys_Sensor& sensor,
                      bool forward, bool black) const override;

    bool is_compatible_calibration(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                   Genesys_Calibration_Cache* cache,
                                   bool for_overwrite) const override;

    void move_to_ta(Genesys_Device* dev) const override;

    void send_shading_data(Genesys_Device* dev, const Genesys_Sensor& sensor, uint8_t* data,
                           int size) const override;

    void calculate_current_setup(Genesys_Device * dev, const Genesys_Sensor& sensor) const override;
    void asic_boot(Genesys_Device* dev, bool cold) const override;
};

} // namespace genesys

#endif // BACKEND_GENESYS_GL841_H
