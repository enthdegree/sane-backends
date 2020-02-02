/* sane - Scanner Access Now Easy.

   Copyright (C) 2012-2013 Stéphane Voltz <stef.dev@free.fr>

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
#include "command_set_common.h"

#ifndef BACKEND_GENESYS_GL846_H
#define BACKEND_GENESYS_GL846_H

namespace genesys {
namespace gl846 {

typedef struct
{
    GpioId gpio_id;
  uint8_t r6b;
  uint8_t r6c;
  uint8_t r6d;
  uint8_t r6e;
  uint8_t r6f;
  uint8_t ra6;
  uint8_t ra7;
  uint8_t ra8;
  uint8_t ra9;
} Gpio_Profile;

static Gpio_Profile gpios[]={
    { GpioId::IMG101, 0x72, 0x1f, 0xa4, 0x13, 0xa7, 0x11, 0xff, 0x19, 0x05},
    { GpioId::PLUSTEK_OPTICBOOK_3800, 0x30, 0x01, 0x80, 0x2d, 0x80, 0x0c, 0x8f, 0x08, 0x04},
    { GpioId::UNKNOWN, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

typedef struct
{
  const char *model;
  uint8_t dramsel;
  /* shading data address */
  uint8_t rd0;
  uint8_t rd1;
  uint8_t rd2;
  /* scanned data address */
  uint8_t rx[24];
} Memory_layout;

static Memory_layout layouts[]={
	/* Image formula 101 */
	{
          "canon-image-formula-101",
          0x8b,
          0x0a, 0x1b, 0x00,
          {                         /* RED ODD START / RED ODD END */
            0x00, 0xb0, 0x05, 0xe7, /* [0x00b0, 0x05e7] 1336*4000w */
                                    /* RED EVEN START / RED EVEN END */
            0x05, 0xe8, 0x0b, 0x1f, /* [0x05e8, 0x0b1f] */
                                    /* GREEN ODD START / GREEN ODD END */
            0x0b, 0x20, 0x10, 0x57, /* [0x0b20, 0x1057] */
                                    /* GREEN EVEN START / GREEN EVEN END */
            0x10, 0x58, 0x15, 0x8f, /* [0x1058, 0x158f] */
                                    /* BLUE ODD START / BLUE ODD END */
            0x15, 0x90, 0x1a, 0xc7, /* [0x1590,0x1ac7] */
                                    /* BLUE EVEN START / BLUE EVEN END */
            0x1a, 0xc8, 0x1f, 0xff  /* [0x1ac8,0x1fff] */
          }
	},
        /* OpticBook 3800 */
	{
          "plustek-opticbook-3800",
          0x2a,
          0x0a, 0x0a, 0x0a,
          { /* RED ODD START / RED ODD END */
            0x00, 0x68, 0x03, 0x00,
            /* RED EVEN START / RED EVEN END */
            0x03, 0x01, 0x05, 0x99,
            /* GREEN ODD START / GREEN ODD END */
            0x05, 0x9a, 0x08, 0x32,
            /* GREEN EVEN START / GREEN EVEN END */
            0x08, 0x33, 0x0a, 0xcb,
            /* BLUE ODD START / BLUE ODD END */
            0x0a, 0xcc, 0x0d, 0x64,
            /* BLUE EVEN START / BLUE EVEN END */
            0x0d, 0x65, 0x0f, 0xfd
          }
	},
        /* list terminating entry */
        { nullptr, 0, 0, 0, 0, {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0} }
};

class CommandSetGl846 : public CommandSetCommon
{
public:
    ~CommandSetGl846() override = default;

    bool needs_home_before_init_regs_for_scan(Genesys_Device* dev) const override;

    void init(Genesys_Device* dev) const override;

    void init_regs_for_warmup(Genesys_Device* dev, const Genesys_Sensor& sensor,
                              Genesys_Register_Set* regs, int* channels,
                              int* total_size) const override;

    void init_regs_for_coarse_calibration(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                          Genesys_Register_Set& regs) const override;

    void init_regs_for_shading(Genesys_Device* dev, const Genesys_Sensor& sensor,
                               Genesys_Register_Set& regs) const override;

    void init_regs_for_scan(Genesys_Device* dev, const Genesys_Sensor& sensor,
                            Genesys_Register_Set& regs) const override;

    void init_regs_for_scan_session(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                    Genesys_Register_Set* reg,
                                    const ScanSession& session) const override;

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

    void move_back_home(Genesys_Device* dev, bool wait_until_home) const override;

    void update_hardware_sensors(struct Genesys_Scanner* s) const override;

    bool needs_update_home_sensor_gpio() const override { return true; }

    void update_home_sensor_gpio(Genesys_Device& dev) const override;

    void load_document(Genesys_Device* dev) const override;

    void detect_document_end(Genesys_Device* dev) const override;

    void eject_document(Genesys_Device* dev) const override;

    void search_strip(Genesys_Device* dev, const Genesys_Sensor& sensor,
                      bool forward, bool black) const override;

    void move_to_ta(Genesys_Device* dev) const override;

    void send_shading_data(Genesys_Device* dev, const Genesys_Sensor& sensor, uint8_t* data,
                           int size) const override;

    ScanSession calculate_scan_session(const Genesys_Device* dev,
                                       const Genesys_Sensor& sensor,
                                       const Genesys_Settings& settings) const override;

    void asic_boot(Genesys_Device* dev, bool cold) const override;
};

enum SlopeTable
{
    SCAN_TABLE = 0, // table 1 at 0x4000
    BACKTRACK_TABLE = 1, // table 2 at 0x4800
    STOP_TABLE = 2, // table 3 at 0x5000
    FAST_TABLE = 3, // table 4 at 0x5800
    HOME_TABLE = 4, // table 5 at 0x6000
};

} // namespace gl846
} // namespace genesys

#endif // BACKEND_GENESYS_GL846_H
