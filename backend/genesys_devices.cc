/* sane - Scanner Access Now Easy.

   Copyright (C) 2003 Oliver Rauch
   Copyright (C) 2003-2005 Henning Meier-Geinitz <henning@meier-geinitz.de>
   Copyright (C) 2004, 2005 Gerhard Jaeger <gerhard@gjaeger.de>
   Copyright (C) 2004-2013 Stéphane Voltz <stef.dev@free.fr>
   Copyright (C) 2005-2009 Pierre Willenbrock <pierre@pirsoft.dnsalias.org>
   Copyright (C) 2007 Luke <iceyfor@gmail.com>
   Copyright (C) 2010 Jack McGill <jmcgill85258@yahoo.com>
   Copyright (C) 2010 Andrey Loginov <avloginov@gmail.com>,
                   xerox travelscan device entry
   Copyright (C) 2010 Chris Berry <s0457957@sms.ed.ac.uk> and Michael Rickmann <mrickma@gwdg.de>
                 for Plustek Opticbook 3600 support

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

/* ------------------------------------------------------------------------ */
/*                     Some setup DAC and CCD tables                        */
/* ------------------------------------------------------------------------ */

#define DEBUG_DECLARE_ONLY

#include "genesys_low.h"

StaticInit<std::vector<Genesys_Frontend>> s_frontends;

void genesys_init_frontend_tables()
{
    s_frontends.init();

    GenesysFrontendLayout wolfson_layout;
    wolfson_layout.offset_addr = { 0x20, 0x21, 0x22 };
    wolfson_layout.gain_addr = { 0x28, 0x29, 0x2a };

    Genesys_Frontend fe;
    fe.fe_id = DAC_WOLFSON_UMAX;
    fe.layout = wolfson_layout;
    fe.regs = {
        { 0x00, 0x00 },
        { 0x01, 0x03 },
        { 0x02, 0x05 },
        { 0x03, 0x11 },
        { 0x20, 0x80 },
        { 0x21, 0x80 },
        { 0x22, 0x80 },
        { 0x24, 0x00 },
        { 0x25, 0x00 },
        { 0x26, 0x00 },
        { 0x28, 0x02 },
        { 0x29, 0x02 },
        { 0x2a, 0x02 },
    };
    fe.reg2 = {0x00, 0x00, 0x00};
    s_frontends->push_back(fe);


    fe = Genesys_Frontend();
    fe.fe_id = DAC_WOLFSON_ST12;
    fe.layout = wolfson_layout;
    fe.regs = {
        { 0x00, 0x00 },
        { 0x01, 0x03 },
        { 0x02, 0x05 },
        { 0x03, 0x03 },
        { 0x20, 0xc8 },
        { 0x21, 0xc8 },
        { 0x22, 0xc8 },
        { 0x24, 0x00 },
        { 0x25, 0x00 },
        { 0x26, 0x00 },
        { 0x28, 0x04 },
        { 0x29, 0x04 },
        { 0x2a, 0x04 },
    };
    fe.reg2 = {0x00, 0x00, 0x00};
    s_frontends->push_back(fe);


    fe = Genesys_Frontend();
    fe.fe_id = DAC_WOLFSON_ST24;
    fe.layout = wolfson_layout;
    fe.regs = {
        { 0x00, 0x00 },
        { 0x01, 0x03 },
        { 0x02, 0x05 },
        { 0x03, 0x21 },
        { 0x20, 0xc8 },
        { 0x21, 0xc8 },
        { 0x22, 0xc8 },
        { 0x24, 0x00 },
        { 0x25, 0x00 },
        { 0x26, 0x00 },
        { 0x28, 0x06 },
        { 0x29, 0x06 },
        { 0x2a, 0x06 },
    };
    fe.reg2 = {0x00, 0x00, 0x00};
    s_frontends->push_back(fe);


    fe = Genesys_Frontend();
    fe.fe_id = DAC_WOLFSON_5345;
    fe.layout = wolfson_layout;
    fe.regs = {
        { 0x00, 0x00 },
        { 0x01, 0x03 },
        { 0x02, 0x05 },
        { 0x03, 0x12 },
        { 0x20, 0xb8 },
        { 0x21, 0xb8 },
        { 0x22, 0xb8 },
        { 0x24, 0x00 },
        { 0x25, 0x00 },
        { 0x26, 0x00 },
        { 0x28, 0x04 },
        { 0x29, 0x04 },
        { 0x2a, 0x04 },
    };
    fe.reg2 = {0x00, 0x00, 0x00};
    s_frontends->push_back(fe);


    // reg3=0x02 for 50-600 dpi, 0x32 (0x12 also works well) at 1200
    fe = Genesys_Frontend();
    fe.fe_id = DAC_WOLFSON_HP2400;
    fe.layout = wolfson_layout;
    fe.regs = {
        { 0x00, 0x00 },
        { 0x01, 0x03 },
        { 0x02, 0x05 },
        { 0x03, 0x02 },
        { 0x20, 0xb4 },
        { 0x21, 0xb6 },
        { 0x22, 0xbc },
        { 0x24, 0x00 },
        { 0x25, 0x00 },
        { 0x26, 0x00 },
        { 0x28, 0x06 },
        { 0x29, 0x09 },
        { 0x2a, 0x08 },
    };
    fe.reg2 = {0x00, 0x00, 0x00};
    s_frontends->push_back(fe);


    fe = Genesys_Frontend();
    fe.fe_id = DAC_WOLFSON_HP2300;
    fe.layout = wolfson_layout;
    fe.regs = {
        { 0x00, 0x00 },
        { 0x01, 0x03 },
        { 0x02, 0x04 },
        { 0x03, 0x02 },
        { 0x20, 0xbe },
        { 0x21, 0xbe },
        { 0x22, 0xbe },
        { 0x24, 0x00 },
        { 0x25, 0x00 },
        { 0x26, 0x00 },
        { 0x28, 0x04 },
        { 0x29, 0x04 },
        { 0x2a, 0x04 },
    };
    fe.reg2 = {0x00, 0x00, 0x00};
    s_frontends->push_back(fe);


    fe = Genesys_Frontend();
    fe.fe_id = DAC_CANONLIDE35;
    fe.layout = wolfson_layout;
    fe.regs = {
        { 0x00, 0x00 },
        { 0x01, 0x3d },
        { 0x02, 0x08 },
        { 0x03, 0x00 },
        { 0x20, 0xe1 },
        { 0x21, 0xe1 },
        { 0x22, 0xe1 },
        { 0x24, 0x00 },
        { 0x25, 0x00 },
        { 0x26, 0x00 },
        { 0x28, 0x93 },
        { 0x29, 0x93 },
        { 0x2a, 0x93 },
    };
    fe.reg2 = {0x00, 0x19, 0x06};
    s_frontends->push_back(fe);


    fe = Genesys_Frontend();
    fe.fe_id = DAC_AD_XP200;
    fe.layout = wolfson_layout;
    fe.regs = {
        { 0x00, 0x58 },
        { 0x01, 0x80 },
        { 0x02, 0x00 },
        { 0x03, 0x00 },
        { 0x20, 0x09 },
        { 0x21, 0x09 },
        { 0x22, 0x09 },
        { 0x24, 0x00 },
        { 0x25, 0x00 },
        { 0x26, 0x00 },
        { 0x28, 0x09 },
        { 0x29, 0x09 },
        { 0x2a, 0x09 },
    };
    fe.reg2 = {0x00, 0x00, 0x00};
    s_frontends->push_back(fe);


    fe = Genesys_Frontend();
    fe.fe_id = DAC_WOLFSON_XP300;
    fe.layout = wolfson_layout;
    fe.regs = {
        { 0x00, 0x00 },
        { 0x01, 0x35 },
        { 0x02, 0x20 },
        { 0x03, 0x14 },
        { 0x20, 0xe1 },
        { 0x21, 0xe1 },
        { 0x22, 0xe1 },
        { 0x24, 0x00 },
        { 0x25, 0x00 },
        { 0x26, 0x00 },
        { 0x28, 0x93 },
        { 0x29, 0x93 },
        { 0x2a, 0x93 },
    };
    fe.reg2 = {0x07, 0x00, 0x00};
    s_frontends->push_back(fe);


    fe = Genesys_Frontend();
    fe.fe_id = DAC_WOLFSON_HP3670;
    fe.layout = wolfson_layout;
    fe.regs = {
        { 0x00, 0x00 },
        { 0x01, 0x03 },
        { 0x02, 0x05 },
        { 0x03, 0x32 },
        { 0x20, 0xba },
        { 0x21, 0xb8 },
        { 0x22, 0xb8 },
        { 0x24, 0x00 },
        { 0x25, 0x00 },
        { 0x26, 0x00 },
        { 0x28, 0x06 },
        { 0x29, 0x05 },
        { 0x2a, 0x04 },
    };
    fe.reg2 = {0x00, 0x00, 0x00};
    s_frontends->push_back(fe);


    fe = Genesys_Frontend();
    fe.fe_id = DAC_WOLFSON_DSM600;
    fe.layout = wolfson_layout;
    fe.regs = {
        { 0x00, 0x00 },
        { 0x01, 0x35 },
        { 0x02, 0x20 },
        { 0x03, 0x14 },
        { 0x20, 0x85 },
        { 0x21, 0x85 },
        { 0x22, 0x85 },
        { 0x24, 0x00 },
        { 0x25, 0x00 },
        { 0x26, 0x00 },
        { 0x28, 0xa0 },
        { 0x29, 0xa0 },
        { 0x2a, 0xa0 },
    };
    fe.reg2 = {0x07, 0x00, 0x00};
    s_frontends->push_back(fe);


    fe = Genesys_Frontend();
    fe.fe_id = DAC_CANONLIDE200;
    fe.layout = wolfson_layout;
    fe.regs = {
        { 0x00, 0x9d },
        { 0x01, 0x91 },
        { 0x02, 0x00 },
        { 0x03, 0x00 },
        { 0x20, 0x00 },
        { 0x21, 0x3f },
        { 0x22, 0x00 },
        { 0x24, 0x00 },
        { 0x25, 0x00 },
        { 0x26, 0x00 },
        { 0x28, 0x32 },
        { 0x29, 0x04 },
        { 0x2a, 0x00 },
    };
    fe.reg2 = {0x00, 0x00, 0x00};
    s_frontends->push_back(fe);


    fe = Genesys_Frontend();
    fe.fe_id = DAC_CANONLIDE700;
    fe.layout = wolfson_layout;
    fe.regs = {
        { 0x00, 0x9d },
        { 0x01, 0x9e },
        { 0x02, 0x00 },
        { 0x03, 0x00 },
        { 0x20, 0x00 },
        { 0x21, 0x3f },
        { 0x22, 0x00 },
        { 0x24, 0x00 },
        { 0x25, 0x00 },
        { 0x26, 0x00 },
        { 0x28, 0x2f },
        { 0x29, 0x04 },
        { 0x2a, 0x00 },
    };
    fe.reg2 = {0x00, 0x00, 0x00};
    s_frontends->push_back(fe);


    fe = Genesys_Frontend();
    fe.fe_id = DAC_KVSS080;
    fe.layout = wolfson_layout;
    fe.regs = {
        { 0x00, 0x00 },
        { 0x01, 0x23 },
        { 0x02, 0x24 },
        { 0x03, 0x0f },
        { 0x20, 0x80 },
        { 0x21, 0x80 },
        { 0x22, 0x80 },
        { 0x24, 0x00 },
        { 0x25, 0x00 },
        { 0x26, 0x00 },
        { 0x28, 0x4b },
        { 0x29, 0x4b },
        { 0x2a, 0x4b },
    };
    fe.reg2 = {0x00,0x00,0x00};
    s_frontends->push_back(fe);


    fe = Genesys_Frontend();
    fe.fe_id = DAC_G4050;
    fe.layout = wolfson_layout;
    fe.regs = {
        { 0x00, 0x00 },
        { 0x01, 0x23 },
        { 0x02, 0x24 },
        { 0x03, 0x1f },
        { 0x20, 0x45 },
        { 0x21, 0x45 },
        { 0x22, 0x45 },
        { 0x24, 0x00 },
        { 0x25, 0x00 },
        { 0x26, 0x00 },
        { 0x28, 0x4b },
        { 0x29, 0x4b },
        { 0x2a, 0x4b },
    };
    fe.reg2 = {0x00,0x00,0x00};
    s_frontends->push_back(fe);


    fe = Genesys_Frontend();
    fe.fe_id = DAC_CANONLIDE110;
    fe.layout = wolfson_layout;
    fe.regs = {
        { 0x00, 0x80 },
        { 0x01, 0x8a },
        { 0x02, 0x23 },
        { 0x03, 0x4c },
        { 0x20, 0x00 },
        { 0x21, 0x00 },
        { 0x22, 0x00 },
        { 0x24, 0x00 },
        { 0x25, 0xca },
        { 0x26, 0x94 },
        { 0x28, 0x00 },
        { 0x29, 0x00 },
        { 0x2a, 0x00 },
    };
    fe.reg2 = {0x00, 0x00, 0x00};
    s_frontends->push_back(fe);

    /** @brief GL124 special case
    * for GL124 based scanners, this struct is "abused"
    * in fact the fields are map like below to AFE registers
    * (from Texas Instrument or alike ?)
    */
    fe = Genesys_Frontend();
    fe.fe_id = DAC_CANONLIDE120;
    fe.layout = wolfson_layout;
    fe.regs = {
        { 0x00, 0x80 },
        { 0x01, 0xa3 },
        { 0x02, 0x2b },
        { 0x03, 0x4c },
        { 0x20, 0x00 },
        { 0x21, 0x00 },
        { 0x22, 0x00 },
        { 0x24, 0x00 }, // actual address 0x05
        { 0x25, 0xca }, // actual address 0x06
        { 0x26, 0x95 }, // actual address 0x07
        { 0x28, 0x00 },
        { 0x29, 0x00 },
        { 0x2a, 0x00 },
    };
    fe.reg2 = {0x00, 0x00, 0x00};
    s_frontends->push_back(fe);


    fe = Genesys_Frontend();
    fe.fe_id = DAC_PLUSTEK_3600;
    fe.layout = wolfson_layout;
    fe.regs = {
        { 0x00, 0x70 },
        { 0x01, 0x80 },
        { 0x02, 0x00 },
        { 0x03, 0x00 },
        { 0x20, 0x00 },
        { 0x21, 0x00 },
        { 0x22, 0x00 },
        { 0x24, 0x00 },
        { 0x25, 0x00 },
        { 0x26, 0x00 },
        { 0x28, 0x3f },
        { 0x29, 0x3d },
        { 0x2a, 0x3d },
    };
    fe.reg2 = {0x00, 0x00, 0x00};
    s_frontends->push_back(fe);


    fe = Genesys_Frontend();
    fe.fe_id = DAC_CS8400F;
    fe.layout = wolfson_layout;
    fe.regs = {
        { 0x00, 0x00 },
        { 0x01, 0x23 },
        { 0x02, 0x24 },
        { 0x03, 0x0f },
        { 0x20, 0x60 },
        { 0x21, 0x5c },
        { 0x22, 0x6c },
        { 0x24, 0x00 },
        { 0x25, 0x00 },
        { 0x26, 0x00 },
        { 0x28, 0x8a },
        { 0x29, 0x9f },
        { 0x2a, 0xc2 },
    };
    fe.reg2 = {0x00, 0x00, 0x00};
    s_frontends->push_back(fe);


    fe = Genesys_Frontend();
    fe.fe_id = DAC_CS8600F;
    fe.layout = wolfson_layout;
    fe.regs = {
        { 0x00, 0x00 },
        { 0x01, 0x23 },
        { 0x02, 0x24 },
        { 0x03, 0x2f },
        { 0x20, 0x67 },
        { 0x21, 0x69 },
        { 0x22, 0x68 },
        { 0x24, 0x00 },
        { 0x25, 0x00 },
        { 0x26, 0x00 },
        { 0x28, 0xdb },
        { 0x29, 0xda },
        { 0x2a, 0xd7 },
    };
    fe.reg2 = { 0x00, 0x00, 0x00 };
    s_frontends->push_back(fe);


    fe = Genesys_Frontend();
    fe.fe_id = DAC_IMG101;
    fe.layout = wolfson_layout;
    fe.regs = {
        { 0x00, 0x78 },
        { 0x01, 0xf0 },
        { 0x02, 0x00 },
        { 0x03, 0x00 },
        { 0x20, 0x00 },
        { 0x21, 0x00 },
        { 0x22, 0x00 },
        { 0x24, 0x00 },
        { 0x25, 0x00 },
        { 0x26, 0x00 },
        { 0x28, 0x00 },
        { 0x29, 0x00 },
        { 0x2a, 0x00 },
    };
    fe.reg2 = {0x00, 0x00, 0x00};
    s_frontends->push_back(fe);


    fe = Genesys_Frontend();
    fe.fe_id = DAC_PLUSTEK3800;
    fe.layout = wolfson_layout;
    fe.regs = {
        { 0x00, 0x78 },
        { 0x01, 0xf0 },
        { 0x02, 0x00 },
        { 0x03, 0x00 },
        { 0x20, 0x00 },
        { 0x21, 0x00 },
        { 0x22, 0x00 },
        { 0x24, 0x00 },
        { 0x25, 0x00 },
        { 0x26, 0x00 },
        { 0x28, 0x00 },
        { 0x29, 0x00 },
        { 0x2a, 0x00 },
    };
    fe.reg2 = {0x00, 0x00, 0x00};
    s_frontends->push_back(fe);


    /* reg0: control 74 data, 70 no data
    * reg3: offset
    * reg6: gain
    * reg0 , reg3, reg6 */
    fe = Genesys_Frontend();
    fe.fe_id = DAC_CANONLIDE80;
    fe.layout = wolfson_layout;
    fe.regs = {
        { 0x00, 0x70 },
        { 0x01, 0x16 },
        { 0x02, 0x60 },
        { 0x03, 0x00 },
        { 0x20, 0x00 },
        { 0x21, 0x00 },
        { 0x22, 0x00 },
        { 0x24, 0x00 },
        { 0x25, 0x00 },
        { 0x26, 0x00 },
        { 0x28, 0x00 },
        { 0x29, 0x00 },
        { 0x2a, 0x00 },
    };
    fe.reg2 = {0x00, 0x00, 0x00};
    s_frontends->push_back(fe);
}

inline unsigned default_get_logical_hwdpi(const Genesys_Sensor& sensor, unsigned xres)
{
    // can't be below 600 dpi
    if (xres <= 600) {
        return 600;
    }
    if (xres <= static_cast<unsigned>(sensor.optical_res) / 4) {
        return sensor.optical_res / 4;
    }
    if (xres <= static_cast<unsigned>(sensor.optical_res) / 2) {
        return sensor.optical_res / 2;
    }
    return sensor.optical_res;
}

inline unsigned get_sensor_optical_with_ccd_divisor(const Genesys_Sensor& sensor, unsigned xres)
{
    unsigned hwres = sensor.optical_res / sensor.get_ccd_size_divisor_for_dpi(xres);

    if (xres <= hwres / 4) {
        return hwres / 4;
    }
    if (xres <= hwres / 2) {
        return hwres / 2;
    }
    return hwres;
}

inline unsigned default_get_ccd_size_divisor_for_dpi(const Genesys_Sensor& sensor, unsigned xres)
{
    if (sensor.ccd_size_divisor >= 4 && xres * 4 <= static_cast<unsigned>(sensor.optical_res)) {
        return 4;
    }
    if (sensor.ccd_size_divisor >= 2 && xres * 2 <= static_cast<unsigned>(sensor.optical_res)) {
        return 2;
    }
    return 1;
}

inline unsigned default_get_hwdpi_divisor_for_dpi(const Genesys_Sensor& sensor, unsigned xres)
{
    return sensor.optical_res / default_get_logical_hwdpi(sensor, xres);
}

/** for setting up the sensor-specific settings:
 * Optical Resolution, number of black pixels, number of dummy pixels,
 * CCD_start_xoffset, and overall number of sensor pixels
 * registers 0x08-0x0b, 0x10-0x1d and 0x52-0x5e
 */
StaticInit<std::vector<Genesys_Sensor>> s_sensors;

void genesys_init_sensor_tables()
{
    s_sensors.init();

    Genesys_Sensor sensor;
    sensor.sensor_id = CCD_UMAX;
    sensor.optical_res = 1200;
    sensor.black_pixels = 48;
    sensor.dummy_pixel = 64;
    sensor.CCD_start_xoffset = 0;
    sensor.sensor_pixels = 10800;
    sensor.fau_gain_white_ref = 210;
    sensor.gain_white_ref = 230;
    sensor.exposure = { 0x0000, 0x0000, 0x0000 };
    sensor.custom_regs = {
        { 0x08, 0x01 },
        { 0x09, 0x03 },
        { 0x0a, 0x05 },
        { 0x0b, 0x07 },
        { 0x16, 0x33 },
        { 0x17, 0x05 },
        { 0x18, 0x31 },
        { 0x19, 0x2a },
        { 0x1a, 0x00 },
        { 0x1b, 0x00 },
        { 0x1c, 0x00 },
        { 0x1d, 0x02 },
        { 0x52, 0x13 },
        { 0x53, 0x17 },
        { 0x54, 0x03 },
        { 0x55, 0x07 },
        { 0x56, 0x0b },
        { 0x57, 0x0f },
        { 0x58, 0x23 },
        { 0x59, 0x00 },
        { 0x5a, 0xc1 },
        { 0x5b, 0x00 },
        { 0x5c, 0x00 },
        { 0x5d, 0x00 },
        { 0x5e, 0x00 },
    };
    sensor.gamma = {1.0, 1.0, 1.0};
    sensor.get_logical_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_register_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_hwdpi_divisor_fun = default_get_hwdpi_divisor_for_dpi;
    sensor.get_ccd_size_divisor_fun = default_get_ccd_size_divisor_for_dpi;
    s_sensors->push_back(sensor);


    sensor = Genesys_Sensor();
    sensor.sensor_id = CCD_ST12;
    sensor.optical_res = 600;
    sensor.black_pixels = 48;
    sensor.dummy_pixel = 85;
    sensor.CCD_start_xoffset = 152;
    sensor.sensor_pixels = 5416;
    sensor.fau_gain_white_ref = 210;
    sensor.gain_white_ref = 230;
    sensor.exposure = { 0x0000, 0x0000, 0x0000 };
    sensor.custom_regs = {
        { 0x08, 0x02 },
        { 0x09, 0x00 },
        { 0x0a, 0x06 },
        { 0x0b, 0x04 },
        { 0x16, 0x2b },
        { 0x17, 0x08 },
        { 0x18, 0x20 },
        { 0x19, 0x2a },
        { 0x1a, 0x00 },
        { 0x1b, 0x00 },
        { 0x1c, 0x0c },
        { 0x1d, 0x03 },
        { 0x52, 0x0f },
        { 0x53, 0x13 },
        { 0x54, 0x17 },
        { 0x55, 0x03 },
        { 0x56, 0x07 },
        { 0x57, 0x0b },
        { 0x58, 0x83 },
        { 0x59, 0x00 },
        { 0x5a, 0xc1 },
        { 0x5b, 0x00 },
        { 0x5c, 0x00 },
        { 0x5d, 0x00 },
        { 0x5e, 0x00 },
    };
    sensor.gamma = {1.0, 1.0, 1.0};
    sensor.get_logical_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_register_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_hwdpi_divisor_fun = default_get_hwdpi_divisor_for_dpi;
    sensor.get_ccd_size_divisor_fun = default_get_ccd_size_divisor_for_dpi;
    s_sensors->push_back(sensor);


    sensor = Genesys_Sensor();
    sensor.sensor_id = CCD_ST24;
    sensor.optical_res = 1200;
    sensor.black_pixels = 48;
    sensor.dummy_pixel = 64;
    sensor.CCD_start_xoffset = 0;
    sensor.sensor_pixels = 10800;
    sensor.fau_gain_white_ref = 210;
    sensor.gain_white_ref = 230;
    sensor.exposure = { 0x0000, 0x0000, 0x0000 };
    sensor.custom_regs = {
        { 0x08, 0x0e },
        { 0x09, 0x0c },
        { 0x0a, 0x00 },
        { 0x0b, 0x0c },
        { 0x16, 0x33 },
        { 0x17, 0x08 },
        { 0x18, 0x31 },
        { 0x19, 0x2a },
        { 0x1a, 0x00 },
        { 0x1b, 0x00 },
        { 0x1c, 0x00 },
        { 0x1d, 0x02 },
        { 0x52, 0x17 },
        { 0x53, 0x03 },
        { 0x54, 0x07 },
        { 0x55, 0x0b },
        { 0x56, 0x0f },
        { 0x57, 0x13 },
        { 0x58, 0x03 },
        { 0x59, 0x00 },
        { 0x5a, 0xc1 },
        { 0x5b, 0x00 },
        { 0x5c, 0x00 },
        { 0x5d, 0x00 },
        { 0x5e, 0x00 },
    };
    sensor.gamma = {1.0, 1.0, 1.0};
    sensor.get_logical_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_register_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_hwdpi_divisor_fun = default_get_hwdpi_divisor_for_dpi;
    sensor.get_ccd_size_divisor_fun = default_get_ccd_size_divisor_for_dpi;
    s_sensors->push_back(sensor);


    sensor = Genesys_Sensor();
    sensor.sensor_id = CCD_5345;
    sensor.optical_res = 1200;
    sensor.ccd_size_divisor = 2;
    sensor.black_pixels = 48;
    sensor.dummy_pixel = 16;
    sensor.CCD_start_xoffset = 0;
    sensor.sensor_pixels = 10872;
    sensor.fau_gain_white_ref = 190;
    sensor.gain_white_ref = 190;
    sensor.exposure = { 0x0000, 0x0000, 0x0000 };
    sensor.custom_regs = {
        { 0x08, 0x0d },
        { 0x09, 0x0f },
        { 0x0a, 0x11 },
        { 0x0b, 0x13 },
        { 0x16, 0x0b },
        { 0x17, 0x0a },
        { 0x18, 0x30 },
        { 0x19, 0x2a },
        { 0x1a, 0x00 },
        { 0x1b, 0x00 },
        { 0x1c, 0x00 },
        { 0x1d, 0x03 },
        { 0x52, 0x0f },
        { 0x53, 0x13 },
        { 0x54, 0x17 },
        { 0x55, 0x03 },
        { 0x56, 0x07 },
        { 0x57, 0x0b },
        { 0x58, 0x23 },
        { 0x59, 0x00 },
        { 0x5a, 0xc1 },
        { 0x5b, 0x00 },
        { 0x5c, 0x00 },
        { 0x5d, 0x00 },
        { 0x5e, 0x00 },
    };
    sensor.gamma = {2.38, 2.35, 2.34};
    sensor.get_logical_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_register_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_hwdpi_divisor_fun = default_get_hwdpi_divisor_for_dpi;
    sensor.get_ccd_size_divisor_fun = default_get_ccd_size_divisor_for_dpi;
    s_sensors->push_back(sensor);


    sensor = Genesys_Sensor();
    sensor.sensor_id = CCD_HP2400;
    sensor.optical_res = 1200,
    sensor.black_pixels = 48;
    sensor.dummy_pixel = 15;
    sensor.CCD_start_xoffset = 0;
    sensor.sensor_pixels = 10872;
    sensor.fau_gain_white_ref = 210;
    sensor.gain_white_ref = 200;
    sensor.exposure = { 0x0000, 0x0000, 0x0000 };
    sensor.custom_regs = {
        { 0x08, 0x14 },
        { 0x09, 0x15 },
        { 0x0a, 0x00 },
        { 0x0b, 0x00 },
        { 0x16, 0xbf },
        { 0x17, 0x08 },
        { 0x18, 0x3f },
        { 0x19, 0x2a },
        { 0x1a, 0x00 },
        { 0x1b, 0x00 },
        { 0x1c, 0x00 },
        { 0x1d, 0x02 },
        { 0x52, 0x0b },
        { 0x53, 0x0f },
        { 0x54, 0x13 },
        { 0x55, 0x17 },
        { 0x56, 0x03 },
        { 0x57, 0x07 },
        { 0x58, 0x63 },
        { 0x59, 0x00 },
        { 0x5a, 0xc1 },
        { 0x5b, 0x00 },
        { 0x5c, 0x0e },
        { 0x5d, 0x00 },
        { 0x5e, 0x00 },
    };
    sensor.gamma = {2.1, 2.1, 2.1};
    sensor.get_logical_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_register_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_hwdpi_divisor_fun = default_get_hwdpi_divisor_for_dpi;
    sensor.get_ccd_size_divisor_fun = default_get_ccd_size_divisor_for_dpi;
    s_sensors->push_back(sensor);


    sensor = Genesys_Sensor();
    sensor.sensor_id = CCD_HP2300;
    sensor.optical_res = 600;
    sensor.ccd_size_divisor = 2;
    sensor.black_pixels = 48;
    sensor.dummy_pixel = 20;
    sensor.CCD_start_xoffset = 0;
    sensor.sensor_pixels = 5368;
    sensor.fau_gain_white_ref = 180;
    sensor.gain_white_ref = 180;
    sensor.exposure = { 0x0000, 0x0000, 0x0000 };
    sensor.custom_regs = {
        { 0x08, 0x16 },
        { 0x09, 0x00 },
        { 0x0a, 0x01 },
        { 0x0b, 0x03 },
        { 0x16, 0xb7 },
        { 0x17, 0x0a },
        { 0x18, 0x20 },
        { 0x19, 0x2a },
        { 0x1a, 0x6a },
        { 0x1b, 0x8a },
        { 0x1c, 0x00 },
        { 0x1d, 0x05 },
        { 0x52, 0x0f },
        { 0x53, 0x13 },
        { 0x54, 0x17 },
        { 0x55, 0x03 },
        { 0x56, 0x07 },
        { 0x57, 0x0b },
        { 0x58, 0x83 },
        { 0x59, 0x00 },
        { 0x5a, 0xc1 },
        { 0x5b, 0x06 },
        { 0x5c, 0x0b },
        { 0x5d, 0x10 },
        { 0x5e, 0x16 },
    };
    sensor.gamma = {2.1, 2.1, 2.1};
    sensor.get_logical_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_register_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_hwdpi_divisor_fun = default_get_hwdpi_divisor_for_dpi;
    sensor.get_ccd_size_divisor_fun = default_get_ccd_size_divisor_for_dpi;
    s_sensors->push_back(sensor);


    sensor = Genesys_Sensor();
    sensor.sensor_id = CCD_CANONLIDE35;
    sensor.optical_res = 1200;
    sensor.ccd_size_divisor = 2;
    sensor.black_pixels = 87;
    sensor.dummy_pixel = 87;
    sensor.CCD_start_xoffset = 0;
    sensor.sensor_pixels = 10400;
    sensor.fau_gain_white_ref = 0;
    sensor.gain_white_ref = 0;
    sensor.exposure = { 0x0400, 0x0400, 0x0400 };
    sensor.custom_regs = {
        { 0x08, 0x00 },
        { 0x09, 0x00 },
        { 0x0a, 0x00 },
        { 0x0b, 0x00 },
        { 0x16, 0x00 },
        { 0x17, 0x02 },
        { 0x18, 0x00 },
        { 0x19, 0x50 },
        { 0x1a, 0x00 }, // TODO: 1a-1d: these do no harm, but may be neccessery for CCD
        { 0x1b, 0x00 },
        { 0x1c, 0x00 },
        { 0x1d, 0x02 },
        { 0x52, 0x05 }, // [GB](HI|LOW) not needed for cis
        { 0x53, 0x07 },
        { 0x54, 0x00 },
        { 0x55, 0x00 },
        { 0x56, 0x00 },
        { 0x57, 0x00 },
        { 0x58, 0x3a },
        { 0x59, 0x03 },
        { 0x5a, 0x40 },
        { 0x5b, 0x00 }, // TODO: 5b-5e
        { 0x5c, 0x00 },
        { 0x5d, 0x00 },
        { 0x5e, 0x00 },
    };
    sensor.gamma = {1.0, 1.0, 1.0};
    sensor.get_logical_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_register_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_hwdpi_divisor_fun = default_get_hwdpi_divisor_for_dpi;
    sensor.get_ccd_size_divisor_fun = default_get_ccd_size_divisor_for_dpi;
    s_sensors->push_back(sensor);


    sensor = Genesys_Sensor();
    sensor.sensor_id = CIS_XP200;
    sensor.optical_res = 600;
    sensor.black_pixels = 5;
    sensor.dummy_pixel = 38;
    sensor.CCD_start_xoffset = 0;
    sensor.sensor_pixels = 5200;
    sensor.fau_gain_white_ref = 200;
    sensor.gain_white_ref = 200;
    sensor.exposure = { 0x1450, 0x0c80, 0x0a28 };
    sensor.custom_regs = {
        { 0x08, 0x16 },
        { 0x09, 0x00 },
        { 0x0a, 0x01 },
        { 0x0b, 0x03 },
        { 0x16, 0xb7 },
        { 0x17, 0x0a },
        { 0x18, 0x20 },
        { 0x19, 0x2a },
        { 0x1a, 0x6a },
        { 0x1b, 0x8a },
        { 0x1c, 0x00 },
        { 0x1d, 0x05 },
        { 0x52, 0x0f },
        { 0x53, 0x13 },
        { 0x54, 0x17 },
        { 0x55, 0x03 },
        { 0x56, 0x07 },
        { 0x57, 0x0b },
        { 0x58, 0x83 },
        { 0x59, 0x00 },
        { 0x5a, 0xc1 },
        { 0x5b, 0x06 },
        { 0x5c, 0x0b },
        { 0x5d, 0x10 },
        { 0x5e, 0x16 },
    };
    sensor.gamma = {2.1, 2.1, 2.1};
    sensor.get_logical_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_register_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_hwdpi_divisor_fun = default_get_hwdpi_divisor_for_dpi;
    sensor.get_ccd_size_divisor_fun = default_get_ccd_size_divisor_for_dpi;
    s_sensors->push_back(sensor);


    sensor = Genesys_Sensor();
    sensor.sensor_id = CCD_HP3670;
    sensor.optical_res = 1200;
    sensor.black_pixels = 48;
    sensor.dummy_pixel = 16;
    sensor.CCD_start_xoffset = 0;
    sensor.sensor_pixels = 10872;
    sensor.fau_gain_white_ref = 210;
    sensor.gain_white_ref = 200;
    sensor.exposure = { 0x0000, 0x0000, 0x0000 };
    sensor.custom_regs = {
        { 0x08, 0x00 },
        { 0x09, 0x0a },
        { 0x0a, 0x0b },
        { 0x0b, 0x0d },
        { 0x16, 0x33 },
        { 0x17, 0x07 },
        { 0x18, 0x20 },
        { 0x19, 0x2a },
        { 0x1a, 0x00 },
        { 0x1b, 0x00 },
        { 0x1c, 0xc0 },
        { 0x1d, 0x43 },
        { 0x52, 0x0f },
        { 0x53, 0x13 },
        { 0x54, 0x17 },
        { 0x55, 0x03 },
        { 0x56, 0x07 },
        { 0x57, 0x0b },
        { 0x58, 0x83 },
        { 0x59, 0x00 },
        { 0x5a, 0x15 },
        { 0x5b, 0x05 },
        { 0x5c, 0x0a },
        { 0x5d, 0x0f },
        { 0x5e, 0x00 },
    };
    sensor.gamma = {1.0, 1.0, 1.0};
    sensor.get_logical_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_register_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_hwdpi_divisor_fun = default_get_hwdpi_divisor_for_dpi;
    sensor.get_ccd_size_divisor_fun = default_get_ccd_size_divisor_for_dpi;
    s_sensors->push_back(sensor);


    sensor = Genesys_Sensor();
    sensor.sensor_id = CCD_DP665;
    sensor.optical_res = 600;
    sensor.black_pixels = 27;
    sensor.dummy_pixel = 27;
    sensor.CCD_start_xoffset = 0;
    sensor.sensor_pixels = 2496;
    sensor.fau_gain_white_ref = 210;
    sensor.gain_white_ref = 200;
    sensor.exposure = { 0x1100, 0x1100, 0x1100 };
    sensor.custom_regs = {
        { 0x08, 0x00 },
        { 0x09, 0x00 },
        { 0x0a, 0x00 },
        { 0x0b, 0x00 },
        { 0x16, 0x00 },
        { 0x17, 0x02 },
        { 0x18, 0x04 },
        { 0x19, 0x50 },
        { 0x1a, 0x10 },
        { 0x1b, 0x00 },
        { 0x1c, 0x20 },
        { 0x1d, 0x02 },
        { 0x52, 0x04 }, // [GB](HI|LOW) not needed for cis
        { 0x53, 0x05 },
        { 0x54, 0x00 },
        { 0x55, 0x00 },
        { 0x56, 0x00 },
        { 0x57, 0x00 },
        { 0x58, 0x54 },
        { 0x59, 0x03 },
        { 0x5a, 0x00 },
        { 0x5b, 0x00 }, // TODO: 5b-5e
        { 0x5c, 0x00 },
        { 0x5d, 0x00 },
        { 0x5e, 0x01 },
    };
    sensor.gamma = {1.0, 1.0, 1.0};
    sensor.get_logical_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_register_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_hwdpi_divisor_fun = default_get_hwdpi_divisor_for_dpi;
    sensor.get_ccd_size_divisor_fun = default_get_ccd_size_divisor_for_dpi;
    s_sensors->push_back(sensor);


    sensor = Genesys_Sensor();
    sensor.sensor_id = CCD_ROADWARRIOR;
    sensor.optical_res = 600;
    sensor.black_pixels = 27;
    sensor.dummy_pixel = 27;
    sensor.CCD_start_xoffset = 0;
    sensor.sensor_pixels = 5200;
    sensor.fau_gain_white_ref = 210;
    sensor.gain_white_ref = 200;
    sensor.exposure = { 0x1100, 0x1100, 0x1100 };
    sensor.custom_regs = {
        { 0x08, 0x00 },
        { 0x09, 0x00 },
        { 0x0a, 0x00 },
        { 0x0b, 0x00 },
        { 0x16, 0x00 },
        { 0x17, 0x02 },
        { 0x18, 0x04 },
        { 0x19, 0x50 },
        { 0x1a, 0x10 },
        { 0x1b, 0x00 },
        { 0x1c, 0x20 },
        { 0x1d, 0x02 },
        { 0x52, 0x04 }, // [GB](HI|LOW) not needed for cis
        { 0x53, 0x05 },
        { 0x54, 0x00 },
        { 0x55, 0x00 },
        { 0x56, 0x00 },
        { 0x57, 0x00 },
        { 0x58, 0x54 },
        { 0x59, 0x03 },
        { 0x5a, 0x00 },
        { 0x5b, 0x00 }, // TODO: 5b-5e
        { 0x5c, 0x00 },
        { 0x5d, 0x00 },
        { 0x5e, 0x01 },
    };
    sensor.gamma = {1.0, 1.0, 1.0};
    sensor.get_logical_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_register_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_hwdpi_divisor_fun = default_get_hwdpi_divisor_for_dpi;
    sensor.get_ccd_size_divisor_fun = default_get_ccd_size_divisor_for_dpi;
    s_sensors->push_back(sensor);


    sensor = Genesys_Sensor();
    sensor.sensor_id = CCD_DSMOBILE600;
    sensor.optical_res = 600;
    sensor.black_pixels = 28;
    sensor.dummy_pixel = 28;
    sensor.CCD_start_xoffset = 0;
    sensor.sensor_pixels = 5200;
    sensor.fau_gain_white_ref = 210;
    sensor.gain_white_ref = 200;
    sensor.exposure = { 0x1544, 0x1544, 0x1544 };
    sensor.custom_regs = {
        { 0x08, 0x00 },
        { 0x09, 0x00 },
        { 0x0a, 0x00 },
        { 0x0b, 0x00 },
        { 0x16, 0x00 },
        { 0x17, 0x02 },
        { 0x18, 0x04 },
        { 0x19, 0x50 },
        { 0x1a, 0x10 },
        { 0x1b, 0x00 },
        { 0x1c, 0x20 },
        { 0x1d, 0x02 },
        { 0x52, 0x04 }, // [GB](HI|LOW) not needed for cis
        { 0x53, 0x05 },
        { 0x54, 0x00 },
        { 0x55, 0x00 },
        { 0x56, 0x00 },
        { 0x57, 0x00 },
        { 0x58, 0x54 },
        { 0x59, 0x03 },
        { 0x5a, 0x00 },
        { 0x5b, 0x00 }, // TODO: 5b-5e
        { 0x5c, 0x00 },
        { 0x5d, 0x00 },
        { 0x5e, 0x01 },
    };
    sensor.gamma = {1.0, 1.0, 1.0};
    sensor.get_logical_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_register_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_hwdpi_divisor_fun = default_get_hwdpi_divisor_for_dpi;
    sensor.get_ccd_size_divisor_fun = default_get_ccd_size_divisor_for_dpi;
    s_sensors->push_back(sensor);


    sensor = Genesys_Sensor();
    sensor.sensor_id = CCD_XP300;
    sensor.optical_res = 600;
    sensor.black_pixels = 27;
    sensor.dummy_pixel = 27;
    sensor.CCD_start_xoffset = 0;
    sensor.sensor_pixels = 10240;
    sensor.fau_gain_white_ref = 210;
    sensor.gain_white_ref = 200;
    sensor.exposure = { 0x1100, 0x1100, 0x1100 };
    sensor.custom_regs = {
        { 0x08, 0x00 },
        { 0x09, 0x00 },
        { 0x0a, 0x00 },
        { 0x0b, 0x00 },
        { 0x16, 0x00 },
        { 0x17, 0x02 },
        { 0x18, 0x04 },
        { 0x19, 0x50 },
        { 0x1a, 0x10 },
        { 0x1b, 0x00 },
        { 0x1c, 0x20 },
        { 0x1d, 0x02 },
        { 0x52, 0x04 }, // [GB](HI|LOW) not needed for cis
        { 0x53, 0x05 },
        { 0x54, 0x00 },
        { 0x55, 0x00 },
        { 0x56, 0x00 },
        { 0x57, 0x00 },
        { 0x58, 0x54 },
        { 0x59, 0x03 },
        { 0x5a, 0x00 },
        { 0x5b, 0x00 }, // TODO: 5b-5e
        { 0x5c, 0x00 },
        { 0x5d, 0x00 },
        { 0x5e, 0x01 },
    };
    sensor.gamma = {1.0, 1.0, 1.0};
    sensor.get_logical_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_register_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_hwdpi_divisor_fun = default_get_hwdpi_divisor_for_dpi;
    sensor.get_ccd_size_divisor_fun = default_get_ccd_size_divisor_for_dpi;
    s_sensors->push_back(sensor);


    sensor = Genesys_Sensor();
    sensor.sensor_id = CCD_DP685;
    sensor.optical_res = 600;
    sensor.black_pixels = 27;
    sensor.dummy_pixel = 27;
    sensor.CCD_start_xoffset = 0;
    sensor.sensor_pixels = 5020;
    sensor.fau_gain_white_ref = 210;
    sensor.gain_white_ref = 200;
    sensor.exposure = { 0x1100, 0x1100, 0x1100 };
    sensor.custom_regs = {
        { 0x08, 0x00 },
        { 0x09, 0x00 },
        { 0x0a, 0x00 },
        { 0x0b, 0x00 },
        { 0x16, 0x00 },
        { 0x17, 0x02 },
        { 0x18, 0x04 },
        { 0x19, 0x50 },
        { 0x1a, 0x10 },
        { 0x1b, 0x00 },
        { 0x1c, 0x20 },
        { 0x1d, 0x02 },
        { 0x52, 0x04 }, // [GB](HI|LOW) not needed for cis
        { 0x53, 0x05 },
        { 0x54, 0x00 },
        { 0x55, 0x00 },
        { 0x56, 0x00 },
        { 0x57, 0x00 },
        { 0x58, 0x54 },
        { 0x59, 0x03 },
        { 0x5a, 0x00 },
        { 0x5b, 0x00 }, // TODO: 5b-5e
        { 0x5c, 0x00 },
        { 0x5d, 0x00 },
        { 0x5e, 0x01 },
    };
    sensor.gamma = {1.0, 1.0, 1.0};
    sensor.get_logical_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_register_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_hwdpi_divisor_fun = default_get_hwdpi_divisor_for_dpi;
    sensor.get_ccd_size_divisor_fun = default_get_ccd_size_divisor_for_dpi;
    s_sensors->push_back(sensor);


    sensor = Genesys_Sensor();
    sensor.sensor_id = CIS_CANONLIDE200;
    sensor.optical_res = 4800;
    sensor.black_pixels = 87*4;
    sensor.dummy_pixel = 16*4;
    sensor.CCD_start_xoffset = 320*8;
    sensor.sensor_pixels = 5136*8;
    sensor.fau_gain_white_ref = 210;
    sensor.gain_white_ref = 200;
    sensor.exposure = { 0x0000, 0x0000, 0x0000 };
    sensor.custom_regs = {
        { 0x08, 0x00 },
        { 0x09, 0x00 },
        { 0x0a, 0x00 },
        { 0x0b, 0x00 },
        { 0x16, 0x10 },
        { 0x17, 0x08 },
        { 0x18, 0x00 },
        { 0x19, 0xff },
        { 0x1a, 0x34 },
        { 0x1b, 0x00 },
        { 0x1c, 0x02 },
        { 0x1d, 0x04 },
        { 0x52, 0x03 },
        { 0x53, 0x07 },
        { 0x54, 0x00 },
        { 0x55, 0x00 },
        { 0x56, 0x00 },
        { 0x57, 0x00 },
        { 0x58, 0x2a },
        { 0x59, 0xe1 },
        { 0x5a, 0x55 },
        { 0x5b, 0x00 },
        { 0x5c, 0x00 },
        { 0x5d, 0x00 },
        { 0x5e, 0x41 },
    };
    sensor.gamma = {1.7, 1.7, 1.7};
    sensor.get_logical_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_register_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_hwdpi_divisor_fun = default_get_hwdpi_divisor_for_dpi;
    sensor.get_ccd_size_divisor_fun = default_get_ccd_size_divisor_for_dpi;
    s_sensors->push_back(sensor);


    sensor = Genesys_Sensor();
    sensor.sensor_id = CIS_CANONLIDE700;
    sensor.optical_res = 4800;
    sensor.black_pixels = 73*8; // black pixels 73 at 600 dpi
    sensor.dummy_pixel = 16*8;
    // 384 at 600 dpi
    sensor.CCD_start_xoffset = 384*8;
    // 8x5570 segments, 5187+1 for rounding
    sensor.sensor_pixels = 5188*8;
    sensor.fau_gain_white_ref = 210;
    sensor.gain_white_ref = 200;
    sensor.exposure = { 0x0000, 0x0000, 0x0000 };
    sensor.custom_regs = {
        { 0x08, 0x00 },
        { 0x09, 0x00 },
        { 0x0a, 0x00 },
        { 0x0b, 0x00 },
        { 0x16, 0x10 },
        { 0x17, 0x08 },
        { 0x18, 0x00 },
        { 0x19, 0xff },
        { 0x1a, 0x34 },
        { 0x1b, 0x00 },
        { 0x1c, 0x02 },
        { 0x1d, 0x04 },
        { 0x52, 0x07 },
        { 0x53, 0x03 },
        { 0x54, 0x00 },
        { 0x55, 0x00 },
        { 0x56, 0x00 },
        { 0x57, 0x00 },
        { 0x58, 0x2a },
        { 0x59, 0xe1 },
        { 0x5a, 0x55 },
        { 0x5b, 0x00 },
        { 0x5c, 0x00 },
        { 0x5d, 0x00 },
        { 0x5e, 0x41 },
    };
    sensor.gamma = {1.0, 1.0, 1.0};
    sensor.get_logical_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_register_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_hwdpi_divisor_fun = default_get_hwdpi_divisor_for_dpi;
    sensor.get_ccd_size_divisor_fun = default_get_ccd_size_divisor_for_dpi;
    s_sensors->push_back(sensor);


    sensor = Genesys_Sensor();
    sensor.sensor_id = CIS_CANONLIDE100;
    sensor.optical_res = 2400;
    sensor.black_pixels = 87*4,        /* black pixels */
    sensor.dummy_pixel = 16*4;
    sensor.CCD_start_xoffset = 320*4;
    sensor.sensor_pixels = 5136*4;
    sensor.fau_gain_white_ref = 210;
    sensor.gain_white_ref = 200;
    sensor.exposure = { 0x01c1, 0x0126, 0x00e5 };
    sensor.custom_regs = {
        { 0x08, 0x00 },
        { 0x09, 0x00 },
        { 0x0a, 0x00 },
        { 0x0b, 0x00 },
        { 0x16, 0x10 },
        { 0x17, 0x08 },
        { 0x18, 0x00 },
        { 0x19, 0x50 },
        { 0x1a, 0x34 },
        { 0x1b, 0x00 },
        { 0x1c, 0x02 },
        { 0x1d, 0x04 },
        { 0x52, 0x03 },
        { 0x53, 0x07 },
        { 0x54, 0x00 },
        { 0x55, 0x00 },
        { 0x56, 0x00 },
        { 0x57, 0x00 },
        { 0x58, 0x2a },
        { 0x59, 0xe1 },
        { 0x5a, 0x55 },
        { 0x5b, 0x00 },
        { 0x5c, 0x00 },
        { 0x5d, 0x00 },
        { 0x5e, 0x41 },
    };
    sensor.gamma = {1.7, 1.7, 1.7};
    sensor.get_logical_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_register_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_hwdpi_divisor_fun = default_get_hwdpi_divisor_for_dpi;
    sensor.get_ccd_size_divisor_fun = default_get_ccd_size_divisor_for_dpi;
    s_sensors->push_back(sensor);


    sensor = Genesys_Sensor();
    sensor.sensor_id = CCD_KVSS080;
    sensor.optical_res = 600;
    sensor.black_pixels = 38;
    sensor.dummy_pixel = 38;
    sensor.CCD_start_xoffset = 152;
    sensor.sensor_pixels = 5376;
    sensor.fau_gain_white_ref = 160;
    sensor.gain_white_ref = 160;
    sensor.exposure = { 0x0000, 0x0000, 0x0000 };
    sensor.exposure_lperiod = 8000;
    sensor.custom_regs = {
        { 0x74, 0x00 }, { 0x75, 0x00 }, { 0x76, 0x00 },
        { 0x77, 0x00 }, { 0x78, 0xff }, { 0x79, 0xff },
        { 0x7a, 0x03 }, { 0x7b, 0xff }, { 0x7c, 0xff },
        { 0x0c, 0x00 },
        { 0x70, 0x01 },
        { 0x71, 0x03 },
        { 0x9e, 0x00 },
        { 0xaa, 0x00 },
        { 0x16, 0x33 },
        { 0x17, 0x1c },
        { 0x18, 0x00 },
        { 0x19, 0x2a },
        { 0x1a, 0x2c },
        { 0x1b, 0x00 },
        { 0x1c, 0x20 },
        { 0x1d, 0x04 },
        { 0x52, 0x0c },
        { 0x53, 0x0f },
        { 0x54, 0x00 },
        { 0x55, 0x03 },
        { 0x56, 0x06 },
        { 0x57, 0x09 },
        { 0x58, 0x6b },
        { 0x59, 0x00 },
        { 0x5a, 0xc0 },
    };
    sensor.gamma = {1.0, 1.0, 1.0};
    sensor.get_logical_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_register_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_hwdpi_divisor_fun = default_get_hwdpi_divisor_for_dpi;
    sensor.get_ccd_size_divisor_fun = default_get_ccd_size_divisor_for_dpi;
    s_sensors->push_back(sensor);


    sensor = Genesys_Sensor();
    sensor.sensor_id = CCD_G4050;
    sensor.optical_res = 4800;
    sensor.black_pixels = 50*8;
    // 31 at 600 dpi dummy_pixels 58 at 1200
    sensor.dummy_pixel = 58;
    sensor.CCD_start_xoffset = 152;
    sensor.sensor_pixels = 5360*8;
    sensor.fau_gain_white_ref = 160;
    sensor.gain_white_ref = 160;
    sensor.exposure = { 0x2c09, 0x22b8, 0x10f0 };
    sensor.custom_regs = {};
    sensor.gamma = {1.0, 1.0, 1.0};
    sensor.get_logical_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_register_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_hwdpi_divisor_fun = default_get_hwdpi_divisor_for_dpi;
    sensor.get_ccd_size_divisor_fun = default_get_ccd_size_divisor_for_dpi;

    {
        struct CustomSensorSettings {
            int min_resolution;
            int max_resolution;
            int exposure_lperiod;
            ScanMethod method;
            GenesysRegisterSettingSet extra_custom_regs;
        };

        CustomSensorSettings custom_settings[] = {
            { -1, 600, 8016, ScanMethod::FLATBED, {
                    { 0x74, 0x00 }, { 0x75, 0x01 }, { 0x76, 0xff },
                    { 0x77, 0x03 }, { 0x78, 0xff }, { 0x79, 0xff },
                    { 0x7a, 0x03 }, { 0x7b, 0xff }, { 0x7c, 0xff },
                    { 0x0c, 0x00 },
                    { 0x70, 0x00 },
                    { 0x71, 0x02 },
                    { 0x9e, 0x00 },
                    { 0xaa, 0x00 },
                    { 0x16, 0x33 },
                    { 0x17, 0x0c },
                    { 0x18, 0x00 },
                    { 0x19, 0x2a },
                    { 0x1a, 0x30 },
                    { 0x1b, 0x00 },
                    { 0x1c, 0x00 },
                    { 0x1d, 0x08 },
                    { 0x52, 0x0b },
                    { 0x53, 0x0e },
                    { 0x54, 0x11 },
                    { 0x55, 0x02 },
                    { 0x56, 0x05 },
                    { 0x57, 0x08 },
                    { 0x58, 0x63 },
                    { 0x59, 0x00 },
                    { 0x5a, 0x40 },
                }
            },
            { 1200, 1200, 56064, ScanMethod::FLATBED, {
                    { 0x74, 0x0f }, { 0x75, 0xff }, { 0x76, 0xff },
                    { 0x77, 0x00 }, { 0x78, 0x01 }, { 0x79, 0xff },
                    { 0x7a, 0x00 }, { 0x7b, 0x01 }, { 0x7c, 0xff },
                    { 0x0c, 0x20 },
                    { 0x70, 0x08 },
                    { 0x71, 0x0c },
                    { 0x9e, 0xc0 },
                    { 0xaa, 0x05 },
                    { 0x16, 0x3b },
                    { 0x17, 0x0c },
                    { 0x18, 0x10 },
                    { 0x19, 0x2a },
                    { 0x1a, 0x38 },
                    { 0x1b, 0x10 },
                    { 0x1c, 0x00 },
                    { 0x1d, 0x08 },
                    { 0x52, 0x02 },
                    { 0x53, 0x05 },
                    { 0x54, 0x08 },
                    { 0x55, 0x0b },
                    { 0x56, 0x0e },
                    { 0x57, 0x11 },
                    { 0x58, 0x1b },
                    { 0x59, 0x00 },
                    { 0x5a, 0x40 },
                }
            },
            { 2400, 2400, 56064, ScanMethod::FLATBED, {
                    { 0x74, 0x0f }, { 0x75, 0xff }, { 0x76, 0xff },
                    { 0x77, 0x00 }, { 0x78, 0x00 }, { 0x79, 0x00 },
                    { 0x7a, 0x00 }, { 0x7b, 0x00 }, { 0x7c, 0x00 },
                    { 0x0c, 0x20 },
                    { 0x70, 0x08 },
                    { 0x71, 0x0a },
                    { 0x9e, 0xc0 },
                    { 0xaa, 0x05 },
                    { 0x16, 0x3b },
                    { 0x17, 0x0c },
                    { 0x18, 0x10 },
                    { 0x19, 0x2a },
                    { 0x1a, 0x38 },
                    { 0x1b, 0x10 },
                    { 0x1c, 0xc0 },
                    { 0x1d, 0x08 },
                    { 0x52, 0x02 },
                    { 0x53, 0x05 },
                    { 0x54, 0x08 },
                    { 0x55, 0x0b },
                    { 0x56, 0x0e },
                    { 0x57, 0x11 },
                    { 0x58, 0x1b },
                    { 0x59, 0x00 },
                    { 0x5a, 0x40 },
                }
            },
            { 4800, 4800, 42752, ScanMethod::FLATBED, {
                    { 0x74, 0x0f }, { 0x75, 0xff }, { 0x76, 0xff },
                    { 0x77, 0x00 }, { 0x78, 0x00 }, { 0x79, 0x00 },
                    { 0x7a, 0x00 }, { 0x7b, 0x00 }, { 0x7c, 0x00 },
                    { 0x0c, 0x21 },
                    { 0x70, 0x08 },
                    { 0x71, 0x0a },
                    { 0x9e, 0xc0 },
                    { 0xaa, 0x07 },
                    { 0x16, 0x3b },
                    { 0x17, 0x0c },
                    { 0x18, 0x10 },
                    { 0x19, 0x2a },
                    { 0x1a, 0x38 },
                    { 0x1b, 0x10 },
                    { 0x1c, 0xc1 },
                    { 0x1d, 0x08 },
                    { 0x52, 0x02 },
                    { 0x53, 0x05 },
                    { 0x54, 0x08 },
                    { 0x55, 0x0b },
                    { 0x56, 0x0e },
                    { 0x57, 0x11 },
                    { 0x58, 0x1b },
                    { 0x59, 0x00 },
                    { 0x5a, 0x40 },
                }
            },
            { -1, -1, 15624, ScanMethod::TRANSPARENCY, {
                    { 0x74, 0x00 }, { 0x75, 0x1c }, { 0x76, 0x7f },
                    { 0x77, 0x03 }, { 0x78, 0xff }, { 0x79, 0xff },
                    { 0x7a, 0x03 }, { 0x7b, 0xff }, { 0x7c, 0xff },
                    { 0x0c, 0x00 },
                    { 0x70, 0x00 },
                    { 0x71, 0x02 },
                    { 0x9e, 0x00 },
                    { 0xaa, 0x00 },
                    { 0x16, 0x33 },
                    { 0x17, 0x4c },
                    { 0x18, 0x01 },
                    { 0x19, 0x2a },
                    { 0x1a, 0x30 },
                    { 0x1b, 0x00 },
                    { 0x1c, 0x00 },
                    { 0x1d, 0x08 },
                    { 0x52, 0x0e },
                    { 0x53, 0x11 },
                    { 0x54, 0x02 },
                    { 0x55, 0x05 },
                    { 0x56, 0x08 },
                    { 0x57, 0x0b },
                    { 0x58, 0x6b },
                    { 0x59, 0x00 },
                    { 0x5a, 0xc0 },
                }
            }
        };

        auto base_custom_regs = sensor.custom_regs;
        for (const CustomSensorSettings& setting : custom_settings)
        {
            sensor.min_resolution = setting.min_resolution;
            sensor.max_resolution = setting.max_resolution;
            sensor.exposure_lperiod = setting.exposure_lperiod;
            sensor.method = setting.method;
            sensor.custom_regs = base_custom_regs;
            sensor.custom_regs.merge(setting.extra_custom_regs);
            s_sensors->push_back(sensor);
        }
    }

    sensor = Genesys_Sensor();
    sensor.sensor_id = CCD_CS4400F;
    sensor.optical_res = 4800;
    sensor.ccd_size_divisor = 4;
    sensor.black_pixels = 50*8;
    // 31 at 600 dpi, 58 at 1200 dpi
    sensor.dummy_pixel = 20;
    sensor.CCD_start_xoffset = 152;
    // 5360 max at 600 dpi
    sensor.sensor_pixels = 5360*8;
    sensor.fau_gain_white_ref = 160;
    sensor.gain_white_ref = 160;
    sensor.exposure = { 0x9c40, 0x9c40, 0x9c40 };
    sensor.exposure_lperiod = 11640;
    sensor.custom_regs = {
        { 0x74, 0x00 }, { 0x75, 0xf8 }, { 0x76, 0x38 },
        { 0x77, 0x00 }, { 0x78, 0xfc }, { 0x79, 0x00 },
        { 0x7a, 0x00 }, { 0x7b, 0x92 }, { 0x7c, 0xa4 },
        { 0x0c, 0x00 },
        { 0x70, 0x00 },
        { 0x71, 0x02 },
        { 0x9e, 0x2d },
        { 0xaa, 0x00 },
        { 0x16, 0x13 },
        { 0x17, 0x0a },
        { 0x18, 0x10 },
        { 0x19, 0x2a },
        { 0x1a, 0x30 },
        { 0x1b, 0x00 },
        { 0x1c, 0x00 },
        { 0x1d, 0x6b },
        { 0x52, 0x0a },
        { 0x53, 0x0d },
        { 0x54, 0x00 },
        { 0x55, 0x03 },
        { 0x56, 0x06 },
        { 0x57, 0x08 },
        { 0x58, 0x5b },
        { 0x59, 0x00 },
        { 0x5a, 0x40 },
    };
    sensor.gamma = {1.0, 1.0, 1.0};
    sensor.get_logical_hwdpi_fun = get_sensor_optical_with_ccd_divisor;
    sensor.get_register_hwdpi_fun = [](const Genesys_Sensor&, unsigned) { return 4800; };
    sensor.get_hwdpi_divisor_fun = [](const Genesys_Sensor&, unsigned) { return 1; };
    sensor.get_ccd_size_divisor_fun = default_get_ccd_size_divisor_for_dpi;
    s_sensors->push_back(sensor);


    sensor = Genesys_Sensor();
    sensor.sensor_id = CCD_CS8400F;
    sensor.optical_res = 4800;
    sensor.ccd_size_divisor = 2;
    sensor.black_pixels = 50*8;
    // 31 at 600 dpi, 58 at 1200 dpi
    sensor.dummy_pixel = 20;
    sensor.CCD_start_xoffset = 152;
    // 5360 max at 600 dpi
    sensor.sensor_pixels = 13600*4;
    sensor.fau_gain_white_ref = 160;
    sensor.gain_white_ref = 160;
    sensor.exposure = { 0x9c40, 0x9c40, 0x9c40 };
    sensor.custom_regs = {};
    sensor.gamma = {1.0, 1.0, 1.0};
    sensor.get_logical_hwdpi_fun = get_sensor_optical_with_ccd_divisor;
    sensor.get_register_hwdpi_fun = [](const Genesys_Sensor&, unsigned) { return 4800; };
    sensor.get_hwdpi_divisor_fun = [](const Genesys_Sensor&, unsigned) { return 1; };
    sensor.get_ccd_size_divisor_fun = default_get_ccd_size_divisor_for_dpi;

    {
        struct CustomSensorSettings {
            int min_resolution;
            int max_resolution;
            int exposure_lperiod;
            ScanMethod method;
            GenesysRegisterSettingSet extra_custom_regs;
            GenesysRegisterSettingSet custom_fe_regs;
        };

        CustomSensorSettings custom_settings[] = {
            { -1, 600, 7200, ScanMethod::FLATBED, {
                    { 0x74, 0x00 }, { 0x75, 0x0e }, { 0x76, 0x3f },
                    { 0x77, 0x00 }, { 0x78, 0x00 }, { 0x79, 0x00 },
                    { 0x7a, 0x01 }, { 0x7b, 0xb6 }, { 0x7c, 0xdb },
                    { 0x70, 0x01 },
                    { 0x71, 0x02 },
                    { 0x72, 0x03 },
                    { 0x73, 0x04 },
                    { 0x16, 0x33 },
                    { 0x17, 0x0c },
                    { 0x18, 0x13 },
                    { 0x19, 0x2a },
                    { 0x1a, 0x30 },
                    { 0x1b, 0x00 },
                    { 0x1c, 0x00 },
                    { 0x1d, 0x84 },
                    { 0x52, 0x0d },
                    { 0x53, 0x10 },
                    { 0x54, 0x01 },
                    { 0x55, 0x04 },
                    { 0x56, 0x07 },
                    { 0x57, 0x0a },
                    { 0x58, 0x6b },
                    { 0x59, 0x00 },
                    { 0x5a, 0x40 },
                    { 0x1e, 0xa0 },
                    { 0x80, 0x20 },
                }, {}
            },
            { 1200, 1200, 14400, ScanMethod::FLATBED, {
                    { 0x74, 0x00 }, { 0x75, 0x01 }, { 0x76, 0xff },
                    { 0x77, 0x00 }, { 0x78, 0x00 }, { 0x79, 0x00 },
                    { 0x7a, 0x02 }, { 0x7b, 0x49 }, { 0x7c, 0x24 },
                    { 0x70, 0x01 },
                    { 0x71, 0x02 },
                    { 0x72, 0x02 },
                    { 0x73, 0x03 },
                    { 0x16, 0x33 },
                    { 0x17, 0x0c },
                    { 0x18, 0x11 },
                    { 0x19, 0x2a },
                    { 0x1a, 0x30 },
                    { 0x1b, 0x00 },
                    { 0x1c, 0x00 },
                    { 0x1d, 0x84 },
                    { 0x52, 0x0b },
                    { 0x53, 0x0e },
                    { 0x54, 0x11 },
                    { 0x55, 0x02 },
                    { 0x56, 0x05 },
                    { 0x57, 0x08 },
                    { 0x58, 0x63 },
                    { 0x59, 0x00 },
                    { 0x5a, 0x40 },
                    { 0x1e, 0xa1 },
                    { 0x80, 0x28 },
                }, {
                    { 0x03, 0x1f },
                }
            },
            { -1, 600, 14400, ScanMethod::TRANSPARENCY, {
                    { 0x16, 0x33 },
                    { 0x17, 0x0c },
                    { 0x18, 0x13 },
                    { 0x19, 0x2a },
                    { 0x1a, 0x30 },
                    { 0x1b, 0x00 },
                    { 0x1c, 0x00 },
                    { 0x1d, 0x84 },
                    { 0x1e, 0xa0 },
                    { 0x52, 0x0d },
                    { 0x53, 0x10 },
                    { 0x54, 0x01 },
                    { 0x55, 0x04 },
                    { 0x56, 0x07 },
                    { 0x57, 0x0a },
                    { 0x58, 0x6b },
                    { 0x59, 0x00 },
                    { 0x5a, 0x40 },
                    { 0x74, 0x00 }, { 0x75, 0x0e }, { 0x76, 0x3f },
                    { 0x77, 0x00 }, { 0x78, 0x00 }, { 0x79, 0x00 },
                    { 0x7a, 0x01 }, { 0x7b, 0xb6 }, { 0x7c, 0xdb },
                    { 0x70, 0x01 }, { 0x71, 0x02 }, { 0x72, 0x03 }, { 0x73, 0x04 },
                    { 0x80, 0x20 },
                }, {}
            },
            { 1200, 1200, 28800, ScanMethod::TRANSPARENCY, {
                    { 0x16, 0x33 },
                    { 0x17, 0x0c },
                    { 0x18, 0x11 },
                    { 0x19, 0x2a },
                    { 0x1a, 0x30 },
                    { 0x1b, 0x00 },
                    { 0x1c, 0x00 },
                    { 0x1d, 0x84 },
                    { 0x1e, 0xa0 },
                    { 0x52, 0x0b },
                    { 0x53, 0x0e },
                    { 0x54, 0x11 },
                    { 0x55, 0x02 },
                    { 0x56, 0x05 },
                    { 0x57, 0x08 },
                    { 0x58, 0x63 },
                    { 0x59, 0x00 },
                    { 0x5a, 0x40 },
                    { 0x70, 0x00 }, { 0x71, 0x01 }, { 0x72, 0x02 }, { 0x73, 0x03 },
                    { 0x74, 0x00 }, { 0x75, 0x01 }, { 0x76, 0xff },
                    { 0x77, 0x00 }, { 0x78, 0x00 }, { 0x79, 0x00 },
                    { 0x7a, 0x02 }, { 0x7b, 0x49 }, { 0x7c, 0x24 },
                    { 0x80, 0x29 },
                }, {
                    { 0x03, 0x1f },
                },
            },
            { 2400, 2400, 28800, ScanMethod::TRANSPARENCY, {
                    { 0x16, 0x33 },
                    { 0x17, 0x0c },
                    { 0x18, 0x10 },
                    { 0x19, 0x2a },
                    { 0x1a, 0x30 },
                    { 0x1b, 0x00 },
                    { 0x1c, 0x20 },
                    { 0x1d, 0x84 },
                    { 0x1e, 0xa0 },
                    { 0x52, 0x02 },
                    { 0x53, 0x05 },
                    { 0x54, 0x08 },
                    { 0x55, 0x0b },
                    { 0x56, 0x0e },
                    { 0x57, 0x11 },
                    { 0x58, 0x1b },
                    { 0x59, 0x00 },
                    { 0x5a, 0x40 },
                    { 0x70, 0x09 }, { 0x71, 0x0a }, { 0x72, 0x0b }, { 0x73, 0x0c },
                    { 0x74, 0x00 }, { 0x75, 0x00 }, { 0x76, 0x00 },
                    { 0x77, 0x00 }, { 0x78, 0x00 }, { 0x79, 0x00 },
                    { 0x7a, 0x02 }, { 0x7b, 0x49 }, { 0x7c, 0x24 },
                    { 0x80, 0x2b },
                }, {
                    { 0x03, 0x1f },
                },
            },
            { 1200, 1200, 28800, ScanMethod::TRANSPARENCY_INFRARED, {
                    { 0x16, 0x33 },
                    { 0x17, 0x0c },
                    { 0x18, 0x11 },
                    { 0x19, 0x2a },
                    { 0x1a, 0x30 },
                    { 0x1b, 0x00 },
                    { 0x1c, 0x00 },
                    { 0x1d, 0x84 },
                    { 0x1e, 0xa0 },
                    { 0x52, 0x0b },
                    { 0x53, 0x0e },
                    { 0x54, 0x11 },
                    { 0x55, 0x02 },
                    { 0x56, 0x05 },
                    { 0x57, 0x08 },
                    { 0x58, 0x63 },
                    { 0x59, 0x00 },
                    { 0x5a, 0x40 },
                    { 0x70, 0x00 }, { 0x71, 0x01 }, { 0x72, 0x02 }, { 0x73, 0x03 },
                    { 0x74, 0x00 }, { 0x75, 0x01 }, { 0x76, 0xff },
                    { 0x77, 0x00 }, { 0x78, 0x00 }, { 0x79, 0x00 },
                    { 0x7a, 0x02 }, { 0x7b, 0x49 }, { 0x7c, 0x24 },
                    { 0x80, 0x21 },
                }, {
                    { 0x03, 0x1f },
                },
            },
        };

        for (const CustomSensorSettings& setting : custom_settings)
        {
            sensor.min_resolution = setting.min_resolution;
            sensor.max_resolution = setting.max_resolution;
            sensor.exposure_lperiod = setting.exposure_lperiod;
            sensor.method = setting.method;
            sensor.custom_regs = setting.extra_custom_regs;
            sensor.custom_fe_regs = setting.custom_fe_regs;
            s_sensors->push_back(sensor);
        }
    }


    sensor = Genesys_Sensor();
    sensor.sensor_id = CCD_CS8600F;
    sensor.optical_res = 4800;
    sensor.ccd_size_divisor = 4;
    sensor.black_pixels = 31;
    sensor.dummy_pixel = 20;
    sensor.CCD_start_xoffset = 0; // not used at the moment
    // 11372 pixels at 1200 dpi
    sensor.sensor_pixels = 11372*4;
    sensor.fau_gain_white_ref = 160;
    sensor.gain_white_ref = 160;
    sensor.exposure = { 0x9c40, 0x9c40, 0x9c40 };
    sensor.custom_regs = {};
    sensor.gamma = {1.0, 1.0, 1.0};
    sensor.get_logical_hwdpi_fun = get_sensor_optical_with_ccd_divisor;
    sensor.get_register_hwdpi_fun = [](const Genesys_Sensor&, unsigned) { return 4800; };
    sensor.get_hwdpi_divisor_fun = [](const Genesys_Sensor&, unsigned) { return 1; };
    sensor.get_ccd_size_divisor_fun = default_get_ccd_size_divisor_for_dpi;

    {
        struct CustomSensorSettings {
            int min_resolution;
            int max_resolution;
            int exposure_lperiod;
            ScanMethod method;
            GenesysRegisterSettingSet extra_custom_regs;
            GenesysRegisterSettingSet custom_fe_regs;
        };

        CustomSensorSettings custom_settings[] = {
            { -1, 1200, 24000, ScanMethod::FLATBED, {
                    { 0x74, 0x03 }, { 0x75, 0xf0 }, { 0x76, 0xf0 },
                    { 0x77, 0x03 }, { 0x78, 0xfe }, { 0x79, 0x00 },
                    { 0x7a, 0x00 }, { 0x7b, 0x92 }, { 0x7c, 0x49 },
                    { 0x0c, 0x00 },
                    { 0x70, 0x00 },
                    { 0x71, 0x02 },
                    { 0x72, 0x02 },
                    { 0x73, 0x04 },
                    { 0x9e, 0x2d },
                    { 0xaa, 0x00 },
                    { 0x16, 0x13 },
                    { 0x17, 0x0a },
                    { 0x18, 0x10 },
                    { 0x19, 0x2a },
                    { 0x1a, 0x30 },
                    { 0x1b, 0x00 },
                    { 0x1c, 0x00 },
                    { 0x1d, 0x6b },
                    { 0x52, 0x0c },
                    { 0x53, 0x0f },
                    { 0x54, 0x00 },
                    { 0x55, 0x03 },
                    { 0x56, 0x06 },
                    { 0x57, 0x09 },
                    { 0x58, 0x6b },
                    { 0x59, 0x00 },
                    { 0x5a, 0x40 },
                },
                {},
            },
            { -1, 1200, 45000, ScanMethod::TRANSPARENCY, {
                    { 0x74, 0x03 }, { 0x75, 0xf0 }, { 0x76, 0xf0 },
                    { 0x77, 0x03 }, { 0x78, 0xfe }, { 0x79, 0x00 },
                    { 0x7a, 0x00 }, { 0x7b, 0x92 }, { 0x7c, 0x49 },
                    { 0x0c, 0x00 },
                    { 0x70, 0x00 },
                    { 0x71, 0x02 },
                    { 0x72, 0x02 },
                    { 0x73, 0x04 },
                    { 0x9e, 0x2d },
                    { 0xaa, 0x00 },
                    { 0x16, 0x13 },
                    { 0x17, 0x0a },
                    { 0x18, 0x10 },
                    { 0x19, 0x2a },
                    { 0x1a, 0x30 },
                    { 0x1b, 0x00 },
                    { 0x1c, 0x00 },
                    { 0x1d, 0x6b },
                    { 0x52, 0x0c },
                    { 0x53, 0x0f },
                    { 0x54, 0x00 },
                    { 0x55, 0x03 },
                    { 0x56, 0x06 },
                    { 0x57, 0x09 },
                    { 0x58, 0x6b },
                    { 0x59, 0x00 },
                    { 0x5a, 0x40 },
                },
                {},
            },
            { 2400, 2400, 45000, ScanMethod::TRANSPARENCY, {
                    { 0x74, 0x03 }, { 0x75, 0xfe }, { 0x76, 0x00 },
                    { 0x77, 0x03 }, { 0x78, 0xfe }, { 0x79, 0x00 },
                    { 0x7a, 0x00 }, { 0x7b, 0x92 }, { 0x7c, 0x49 },
                    { 0x0c, 0x00 },
                    { 0x70, 0x00 },
                    { 0x71, 0x02 },
                    { 0x72, 0x02 },
                    { 0x73, 0x04 },
                    { 0x9e, 0x2d },
                    { 0xaa, 0x00 },
                    { 0x16, 0x13 },
                    { 0x17, 0x15 },
                    { 0x18, 0x10 },
                    { 0x19, 0x2a },
                    { 0x1a, 0x30 },
                    { 0x1b, 0x00 },
                    { 0x1c, 0x01 },
                    { 0x1d, 0x75 },
                    { 0x52, 0x0c },
                    { 0x53, 0x0f },
                    { 0x54, 0x00 },
                    { 0x55, 0x03 },
                    { 0x56, 0x06 },
                    { 0x57, 0x09 },
                    { 0x58, 0x6b },
                    { 0x59, 0x00 },
                    { 0x5a, 0x40 },
                },
                {},
            },
            { 4800, 4800, 45000, ScanMethod::TRANSPARENCY, {
                    { 0x74, 0x03 }, { 0x75, 0xff }, { 0x76, 0xff },
                    { 0x77, 0x03 }, { 0x78, 0xff }, { 0x79, 0xff },
                    { 0x7a, 0x00 }, { 0x7b, 0x92 }, { 0x7c, 0x49 },
                    { 0x0c, 0x00 },
                    { 0x70, 0x0a },
                    { 0x71, 0x0c },
                    { 0x72, 0x0c },
                    { 0x73, 0x0e },
                    { 0x9e, 0x2d },
                    { 0xaa, 0x00 },
                    { 0x16, 0x13 },
                    { 0x17, 0x15 },
                    { 0x18, 0x10 },
                    { 0x19, 0x2a },
                    { 0x1a, 0x30 },
                    { 0x1b, 0x00 },
                    { 0x1c, 0x61 },
                    { 0x1d, 0x75 },
                    { 0x52, 0x03 },
                    { 0x53, 0x06 },
                    { 0x54, 0x09 },
                    { 0x55, 0x0c },
                    { 0x56, 0x0f },
                    { 0x57, 0x00 },
                    { 0x58, 0x23 },
                    { 0x59, 0x00 },
                    { 0x5a, 0x40 },
                },
                {   { 0x03, 0x1f },
                },
            },
            { -1, 1200, 45000, ScanMethod::TRANSPARENCY_INFRARED, {
                    { 0x74, 0x03 }, { 0x75, 0xf0 }, { 0x76, 0xf0 },
                    { 0x77, 0x03 }, { 0x78, 0xfe }, { 0x79, 0x00 },
                    { 0x7a, 0x00 }, { 0x7b, 0x92 }, { 0x7c, 0x49 },
                    { 0x0c, 0x00 },
                    { 0x70, 0x00 },
                    { 0x71, 0x02 },
                    { 0x9e, 0x2d },
                    { 0xaa, 0x00 },
                    { 0x16, 0x13 },
                    { 0x17, 0x0a },
                    { 0x18, 0x10 },
                    { 0x19, 0x2a },
                    { 0x1a, 0x30 },
                    { 0x1b, 0x00 },
                    { 0x1c, 0x00 },
                    { 0x1d, 0x6b },
                    { 0x52, 0x0c },
                    { 0x53, 0x0f },
                    { 0x54, 0x00 },
                    { 0x55, 0x03 },
                    { 0x56, 0x06 },
                    { 0x57, 0x09 },
                    { 0x58, 0x6b },
                    { 0x59, 0x00 },
                    { 0x5a, 0x40 },
                },
                {},
            },
        };

        for (const CustomSensorSettings& setting : custom_settings)
        {
            sensor.min_resolution = setting.min_resolution;
            sensor.max_resolution = setting.max_resolution;
            sensor.method = setting.method;
            sensor.exposure_lperiod = setting.exposure_lperiod;
            sensor.custom_regs = setting.extra_custom_regs;
            sensor.custom_fe_regs = setting.custom_fe_regs;
            s_sensors->push_back(sensor);
        }
    }


    sensor = Genesys_Sensor();
    sensor.sensor_id = CCD_HP_N6310;
    sensor.optical_res = 2400;
    // sensor.ccd_size_divisor = 2; Possibly half CCD, needs checking
    sensor.black_pixels = 96;
    sensor.dummy_pixel = 26;
    sensor.CCD_start_xoffset = 128;
    sensor.sensor_pixels = 42720;
    sensor.fau_gain_white_ref = 210;
    sensor.gain_white_ref = 230;
    sensor.exposure = { 0x0000, 0x0000, 0x0000 };
    sensor.custom_regs = {
        { 0x08, 0x00 },
        { 0x09, 0x10 },
        { 0x0a, 0x10 },
        { 0x0b, 0x0c },
        { 0x16, 0x33 },
        { 0x17, 0x0c },
        { 0x18, 0x02 },
        { 0x19, 0x2a },
        { 0x1a, 0x30 },
        { 0x1b, 0x00 },
        { 0x1c, 0x00 },
        { 0x1d, 0x08 },
        { 0x52, 0x0b },
        { 0x53, 0x0e },
        { 0x54, 0x11 },
        { 0x55, 0x02 },
        { 0x56, 0x05 },
        { 0x57, 0x08 },
        { 0x58, 0x63 },
        { 0x59, 0x00 },
        { 0x5a, 0x40 },
        { 0x5b, 0x00 },
        { 0x5c, 0x00 },
        { 0x5d, 0x06 },
        { 0x5e, 0x6f },
    };
    sensor.gamma = {1.0, 1.0, 1.0};
    sensor.get_logical_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_register_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_hwdpi_divisor_fun = default_get_hwdpi_divisor_for_dpi;
    sensor.get_ccd_size_divisor_fun = default_get_ccd_size_divisor_for_dpi;
    s_sensors->push_back(sensor);


    sensor = Genesys_Sensor();
    sensor.sensor_id = CIS_CANONLIDE110;
    sensor.optical_res = 2400;
    sensor.ccd_size_divisor = 2;
    sensor.black_pixels = 87;
    sensor.dummy_pixel = 16;
    sensor.CCD_start_xoffset = 303;
    sensor.sensor_pixels = 5168*4;
    sensor.fau_gain_white_ref = 210;
    sensor.gain_white_ref = 200;
    sensor.exposure = { 0x0000, 0x0000, 0x0000 };
    sensor.custom_regs = {
        { 0x08, 0x00 },
        { 0x09, 0x00 },
        { 0x0a, 0x00 },
        { 0x0b, 0x00 },
        { 0x16, 0x10 },
        { 0x17, 0x04 },
        { 0x18, 0x00 },
        { 0x19, 0x01 },
        { 0x1a, 0x30 },
        { 0x1b, 0x00 },
        { 0x1c, 0x02 },
        { 0x1d, 0x01 },
        { 0x52, 0x00 },
        { 0x53, 0x02 },
        { 0x54, 0x04 },
        { 0x55, 0x06 },
        { 0x56, 0x04 },
        { 0x57, 0x04 },
        { 0x58, 0x04 },
        { 0x59, 0x04 },
        { 0x5a, 0x1a },
        { 0x5b, 0x00 },
        { 0x5c, 0xc0 },
        { 0x5d, 0x00 },
        { 0x5e, 0x00 },
    };
    sensor.gamma = {2.1, 2.1, 2.1};
    sensor.get_logical_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_register_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_hwdpi_divisor_fun = default_get_hwdpi_divisor_for_dpi;
    sensor.get_ccd_size_divisor_fun = default_get_ccd_size_divisor_for_dpi;
    s_sensors->push_back(sensor);


    sensor = Genesys_Sensor();
    sensor.sensor_id = CIS_CANONLIDE120;
    sensor.optical_res = 2400;
    sensor.ccd_size_divisor = 2;
    sensor.black_pixels = 87;
    sensor.dummy_pixel = 16;
    sensor.CCD_start_xoffset = 303;
    // SEGCNT at 600 DPI by number of segments
    sensor.sensor_pixels = 5104*4;
    sensor.fau_gain_white_ref = 210;
    sensor.gain_white_ref = 200;
    sensor.exposure = { 0x0000, 0x0000, 0x0000 };
    sensor.custom_regs = {
        { 0x08, 0x00 },
        { 0x09, 0x00 },
        { 0x0a, 0x00 },
        { 0x0b, 0x00 },
        { 0x16, 0x15 },
        { 0x17, 0x04 },
        { 0x18, 0x00 },
        { 0x19, 0x01 },
        { 0x1a, 0x30 },
        { 0x1b, 0x00 },
        { 0x1c, 0x02 },
        { 0x1d, 0x01 },
        { 0x52, 0x04 },
        { 0x53, 0x06 },
        { 0x54, 0x00 },
        { 0x55, 0x02 },
        { 0x56, 0x04 },
        { 0x57, 0x04 },
        { 0x58, 0x04 },
        { 0x59, 0x04 },
        { 0x5a, 0x3a },
        { 0x5b, 0x00 },
        { 0x5c, 0x00 },
        { 0x5d, 0x00 },
        { 0x5e, 0x1f },
    };
    sensor.gamma = {2.1, 2.1, 2.1};
    sensor.get_logical_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_register_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_hwdpi_divisor_fun = default_get_hwdpi_divisor_for_dpi;
    sensor.get_ccd_size_divisor_fun = default_get_ccd_size_divisor_for_dpi;
    s_sensors->push_back(sensor);


    sensor = Genesys_Sensor();
    sensor.sensor_id = CIS_CANONLIDE210;
    sensor.optical_res = 2400;
    sensor.ccd_size_divisor = 2;
    sensor.black_pixels = 87;
    sensor.dummy_pixel = 16;
    sensor.CCD_start_xoffset = 303;
    sensor.sensor_pixels = 5168*4;
    sensor.fau_gain_white_ref = 210;
    sensor.gain_white_ref = 200;
    sensor.exposure = { 0x0000, 0x0000, 0x0000 };
    sensor.custom_regs = {
        { 0x08, 0x00 },
        { 0x09, 0x00 },
        { 0x0a, 0x00 },
        { 0x0b, 0x00 },
        { 0x16, 0x10 },
        { 0x17, 0x04 },
        { 0x18, 0x00 },
        { 0x19, 0x01 },
        { 0x1a, 0x30 },
        { 0x1b, 0x00 },
        { 0x1c, 0x02 },
        { 0x1d, 0x01 },
        { 0x52, 0x00 },
        { 0x53, 0x02 },
        { 0x54, 0x04 },
        { 0x55, 0x06 },
        { 0x56, 0x04 },
        { 0x57, 0x04 },
        { 0x58, 0x04 },
        { 0x59, 0x04 },
        { 0x5a, 0x1a },
        { 0x5b, 0x00 },
        { 0x5c, 0xc0 },
        { 0x5d, 0x00 },
        { 0x5e, 0x00 },
    };
    sensor.gamma = {2.1, 2.1, 2.1};
    sensor.get_logical_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_register_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_hwdpi_divisor_fun = default_get_hwdpi_divisor_for_dpi;
    sensor.get_ccd_size_divisor_fun = default_get_ccd_size_divisor_for_dpi;
    s_sensors->push_back(sensor);


    sensor = Genesys_Sensor();
    sensor.sensor_id = CIS_CANONLIDE220;
    sensor.optical_res = 2400;
    sensor.ccd_size_divisor = 2;
    sensor.black_pixels = 87;
    sensor.dummy_pixel = 16;
    sensor.CCD_start_xoffset = 303;
    sensor.sensor_pixels = 5168*4;
    sensor.fau_gain_white_ref = 210;
    sensor.gain_white_ref = 200;
    sensor.exposure = { 0x0000, 0x0000, 0x0000 };
    sensor.custom_regs = {
        { 0x08, 0x00 },
        { 0x09, 0x00 },
        { 0x0a, 0x00 },
        { 0x0b, 0x00 },
        { 0x16, 0x10 },
        { 0x17, 0x04 },
        { 0x18, 0x00 },
        { 0x19, 0x01 },
        { 0x1a, 0x30 },
        { 0x1b, 0x00 },
        { 0x1c, 0x02 },
        { 0x1d, 0x01 },
        { 0x52, 0x00 },
        { 0x53, 0x02 },
        { 0x54, 0x04 },
        { 0x55, 0x06 },
        { 0x56, 0x04 },
        { 0x57, 0x04 },
        { 0x58, 0x04 },
        { 0x59, 0x04 },
        { 0x5a, 0x1a },
        { 0x5b, 0x00 },
        { 0x5c, 0xc0 },
        { 0x5d, 0x00 },
        { 0x5e, 0x00 },
    };
    sensor.gamma = {2.1, 2.1, 2.1};
    sensor.get_logical_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_register_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_hwdpi_divisor_fun = default_get_hwdpi_divisor_for_dpi;
    sensor.get_ccd_size_divisor_fun = default_get_ccd_size_divisor_for_dpi;
    s_sensors->push_back(sensor);


    sensor = Genesys_Sensor();
    sensor.sensor_id = CCD_PLUSTEK_3600;
    sensor.optical_res = 1200;
    sensor.ccd_size_divisor = 2;
    sensor.black_pixels = 87;
    sensor.dummy_pixel = 87;
    sensor.CCD_start_xoffset = 0;
    sensor.sensor_pixels = 10100;
    sensor.fau_gain_white_ref = 210;
    sensor.gain_white_ref = 230;
    sensor.exposure = { 0x0000, 0x0000, 0x0000 };
    sensor.custom_regs = {
        { 0x08, 0x00 },
        { 0x09, 0x00 },
        { 0x0a, 0x00 },
        { 0x0b, 0x00 },
        { 0x16, 0x33 },
        { 0x17, 0x0b },
        { 0x18, 0x11 },
        { 0x19, 0x2a },
        { 0x1a, 0x00 },
        { 0x1b, 0x00 },
        { 0x1c, 0x00 },
        { 0x1d, 0xc4 },
        { 0x52, 0x07 }, // [GB](HI|LOW) not needed for cis
        { 0x53, 0x0a },
        { 0x54, 0x0c },
        { 0x55, 0x00 },
        { 0x56, 0x02 },
        { 0x57, 0x06 },
        { 0x58, 0x22 },
        { 0x59, 0x69 },
        { 0x5a, 0x40 },
        { 0x5b, 0x00 }, // TODO: 5b-5e
        { 0x5c, 0x00 },
        { 0x5d, 0x00 },
        { 0x5e, 0x02 },
    };
    sensor.gamma = {1.0, 1.0, 1.0};
    sensor.get_logical_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_register_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_hwdpi_divisor_fun = default_get_hwdpi_divisor_for_dpi;
    sensor.get_ccd_size_divisor_fun = default_get_ccd_size_divisor_for_dpi;
    s_sensors->push_back(sensor);


    sensor = Genesys_Sensor();
    sensor.sensor_id = CCD_IMG101;
    sensor.optical_res = 1200;
    sensor.black_pixels = 31;
    sensor.dummy_pixel = 31;
    sensor.CCD_start_xoffset = 0;
    sensor.sensor_pixels = 10800;
    sensor.fau_gain_white_ref = 210;
    sensor.gain_white_ref = 200;
    sensor.exposure = { 0x0000, 0x0000, 0x0000 };
    sensor.custom_regs = {
        { 0x08, 0x60 },
        { 0x09, 0x00 },
        { 0x0a, 0x00 },
        { 0x0b, 0x8b },
        { 0x16, 0xbb },
        { 0x17, 0x13 },
        { 0x18, 0x10 },
        { 0x19, 0x2a },
        { 0x1a, 0x34 },
        { 0x1b, 0x00 },
        { 0x1c, 0x20 },
        { 0x1d, 0x06 },
        { 0x52, 0x02 },
        { 0x53, 0x04 },
        { 0x54, 0x06 },
        { 0x55, 0x08 },
        { 0x56, 0x0a },
        { 0x57, 0x00 },
        { 0x58, 0x59 },
        { 0x59, 0x31 },
        { 0x5a, 0x40 },
        { 0x5b, 0x00 },
        { 0x5c, 0x00 },
        { 0x5d, 0x00 },
        { 0x5e, 0x1f },
    };
    sensor.gamma = {1.7, 1.7, 1.7};
    sensor.get_logical_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_register_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_hwdpi_divisor_fun = default_get_hwdpi_divisor_for_dpi;
    sensor.get_ccd_size_divisor_fun = default_get_ccd_size_divisor_for_dpi;
    s_sensors->push_back(sensor);


    sensor = Genesys_Sensor();
    sensor.sensor_id = CCD_PLUSTEK3800;
    sensor.optical_res = 1200;
    sensor.black_pixels = 31;
    sensor.dummy_pixel = 31;
    sensor.CCD_start_xoffset = 0;
    sensor.sensor_pixels = 10200;
    sensor.fau_gain_white_ref = 210;
    sensor.gain_white_ref = 200;
    sensor.exposure = { 0x0000, 0x0000, 0x0000 };
    sensor.custom_regs = {
        { 0x08, 0x60 },
        { 0x09, 0x00 },
        { 0x0a, 0x00 },
        { 0x0b, 0x8b },
        { 0x16, 0xbb },
        { 0x17, 0x13 },
        { 0x18, 0x10 },
        { 0x19, 0x2a },
        { 0x1a, 0x34 },
        { 0x1b, 0x00 },
        { 0x1c, 0x20 },
        { 0x1d, 0x06 },
        { 0x52, 0x02 },
        { 0x53, 0x04 },
        { 0x54, 0x06 },
        { 0x55, 0x08 },
        { 0x56, 0x0a },
        { 0x57, 0x00 },
        { 0x58, 0x59 },
        { 0x59, 0x31 },
        { 0x5a, 0x40 },
        { 0x5b, 0x00 },
        { 0x5c, 0x00 },
        { 0x5d, 0x00 },
        { 0x5e, 0x1f },
    };
    sensor.gamma = {1.7, 1.7, 1.7};
    sensor.get_logical_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_register_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_hwdpi_divisor_fun = default_get_hwdpi_divisor_for_dpi;
    sensor.get_ccd_size_divisor_fun = default_get_ccd_size_divisor_for_dpi;
    s_sensors->push_back(sensor);


    sensor = Genesys_Sensor();
    sensor.sensor_id = CIS_CANONLIDE80,
    sensor.optical_res = 1200; // real hardware limit is 2400
    sensor.ccd_size_divisor = 2;
    sensor.black_pixels = 20;
    sensor.dummy_pixel = 6;
    // tuned to give 3*8 multiple startx coordinate during shading calibration
    sensor.CCD_start_xoffset = 34; // 14=>3, 20=>2
    // 10400, too wide=>10288 in shading data 10240~
    // 10208 too short for shading, max shading data = 10240 pixels, endpix-startpix=10208
    sensor.sensor_pixels = 10240;
    sensor.fau_gain_white_ref = 150;
    sensor.gain_white_ref = 150;
    // maps to 0x70-0x73 for GL841
    sensor.exposure = { 0x1000, 0x1000, 0x0500 };
    sensor.custom_regs = {
        { 0x08, 0x00 },
        { 0x09, 0x05 },
        { 0x0a, 0x07 },
        { 0x0b, 0x09 },
        { 0x16, 0x00 },
        { 0x17, 0x01 },
        { 0x18, 0x00 },
        { 0x19, 0x06 },
        { 0x1a, 0x00 },
        { 0x1b, 0x00 },
        { 0x1c, 0x00 },
        { 0x1d, 0x04 },
        { 0x52, 0x03 },
        { 0x53, 0x07 },
        { 0x54, 0x00 },
        { 0x55, 0x00 },
        { 0x56, 0x00 },
        { 0x57, 0x00 },
        { 0x58, 0x29 },
        { 0x59, 0x69 },
        { 0x5a, 0x55 },
        { 0x5b, 0x00 },
        { 0x5c, 0x00 },
        { 0x5d, 0x20 },
        { 0x5e, 0x41 },
    };
    sensor.gamma = {1.0, 1.0, 1.0};
    sensor.get_logical_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_register_hwdpi_fun = default_get_logical_hwdpi;
    sensor.get_hwdpi_divisor_fun = default_get_hwdpi_divisor_for_dpi;
    sensor.get_ccd_size_divisor_fun = default_get_ccd_size_divisor_for_dpi;
    s_sensors->push_back(sensor);
}

/** for General Purpose Output specific settings:
 * initial GPO value (registers 0x66-0x67/0x6c-0x6d)
 * GPO enable mask (registers 0x68-0x69/0x6e-0x6f)
 * The first register is for GPIO9-GPIO16, the second for GPIO1-GPIO8
 */

StaticInit<std::vector<Genesys_Gpo>> s_gpo;

void genesys_init_gpo_tables()
{
    s_gpo.init();

    Genesys_Gpo gpo;
    gpo.gpo_id = GPO_UMAX;
    gpo.regs = {
        { 0x66, 0x11 },
        { 0x67, 0x00 },
        { 0x68, 0x51 },
        { 0x69, 0x20 },
    };
    s_gpo->push_back(gpo);


    gpo = Genesys_Gpo();
    gpo.gpo_id = GPO_ST12;
    gpo.regs = {
        { 0x66, 0x11 },
        { 0x67, 0x00 },
        { 0x68, 0x51 },
        { 0x69, 0x20 },
    };
    s_gpo->push_back(gpo);


    gpo = Genesys_Gpo();
    gpo.gpo_id = GPO_ST24;
    gpo.regs = {
        { 0x66, 0x00 },
        { 0x67, 0x00 },
        { 0x68, 0x51 },
        { 0x69, 0x20 },
    };
    s_gpo->push_back(gpo);


    gpo = Genesys_Gpo();
    gpo.gpo_id = GPO_5345; // bits 11-12 are for bipolar V-ref input voltage
    gpo.regs = {
        { 0x66, 0x30 },
        { 0x67, 0x18 },
        { 0x68, 0xa0 },
        { 0x69, 0x18 },
    };
    s_gpo->push_back(gpo);


    gpo = Genesys_Gpo();
    gpo.gpo_id = GPO_HP2400;
    gpo.regs = {
        { 0x66, 0x30 },
        { 0x67, 0x00 },
        { 0x68, 0x31 },
        { 0x69, 0x00 },
    };
    s_gpo->push_back(gpo);


    gpo = Genesys_Gpo();
    gpo.gpo_id = GPO_HP2300;
    gpo.regs = {
        { 0x66, 0x00 },
        { 0x67, 0x00 },
        { 0x68, 0x00 },
        { 0x69, 0x00 },
    };
    s_gpo->push_back(gpo);


    gpo = Genesys_Gpo();
    gpo.gpo_id = GPO_CANONLIDE35;
    gpo.regs = {
        { 0x6c, 0x02 },
        { 0x6d, 0x80 },
        { 0x6e, 0xef },
        { 0x6f, 0x80 },
    };
    s_gpo->push_back(gpo);


    gpo = Genesys_Gpo();
    gpo.gpo_id = GPO_XP200;
    gpo.regs = {
        { 0x66, 0x30 },
        { 0x67, 0x00 },
        { 0x68, 0xb0 },
        { 0x69, 0x00 },
    };
    s_gpo->push_back(gpo);


    gpo = Genesys_Gpo();
    gpo.gpo_id = GPO_HP3670;
    gpo.regs = {
        { 0x66, 0x00 },
        { 0x67, 0x00 },
        { 0x68, 0x00 },
        { 0x69, 0x00 },
    };
    s_gpo->push_back(gpo);


    gpo = Genesys_Gpo();
    gpo.gpo_id = GPO_XP300;
    gpo.regs = {
        { 0x6c, 0x09 },
        { 0x6d, 0xc6 },
        { 0x6e, 0xbb },
        { 0x6f, 0x00 },
    };
    s_gpo->push_back(gpo);


    gpo = Genesys_Gpo();
    gpo.gpo_id = GPO_DP665;
    gpo.regs = {
        { 0x6c, 0x18 },
        { 0x6d, 0x00 },
        { 0x6e, 0xbb },
        { 0x6f, 0x00 },
    };
    s_gpo->push_back(gpo);


    gpo = Genesys_Gpo();
    gpo.gpo_id = GPO_DP685;
    gpo.regs = {
        { 0x6c, 0x3f },
        { 0x6d, 0x46 },
        { 0x6e, 0xfb },
        { 0x6f, 0x00 },
    };
    s_gpo->push_back(gpo);


    gpo = Genesys_Gpo();
    gpo.gpo_id = GPO_CANONLIDE200;
    gpo.regs = {
        { 0x6c, 0xfb }, // 0xfb when idle , 0xf9/0xe9 (1200) when scanning
        { 0x6d, 0x20 },
        { 0x6e, 0xff },
        { 0x6f, 0x00 },
    };
    s_gpo->push_back(gpo);


    gpo = Genesys_Gpo();
    gpo.gpo_id = GPO_CANONLIDE700;
    gpo.regs = {
        { 0x6c, 0xdb },
        { 0x6d, 0xff },
        { 0x6e, 0xff },
        { 0x6f, 0x80 },
    };
    s_gpo->push_back(gpo);


    gpo = Genesys_Gpo();
    gpo.gpo_id = GPO_KVSS080;
    gpo.regs = {
        { 0x6c, 0xf5 },
        { 0x6d, 0x20 },
        { 0x6e, 0x7e },
        { 0x6f, 0xa1 },
    };
    s_gpo->push_back(gpo);


    gpo = Genesys_Gpo();
    gpo.gpo_id = GPO_G4050;
    gpo.regs = {
        { 0x6c, 0x20 },
        { 0x6d, 0x00 },
        { 0x6e, 0xfc },
        { 0x6f, 0x00 },
    };
    s_gpo->push_back(gpo);


    gpo = Genesys_Gpo();
    gpo.gpo_id = GPO_HP_N6310;
    gpo.regs = {
        { 0x6c, 0xa3 },
        { 0x6d, 0x00 },
        { 0x6e, 0x7f },
        { 0x6f, 0x00 },
    };
    s_gpo->push_back(gpo);


    gpo = Genesys_Gpo();
    gpo.gpo_id = GPO_CANONLIDE110;
    gpo.regs = {
        { 0x6c, 0xfb },
        { 0x6d, 0x20 },
        { 0x6e, 0xff },
        { 0x6f, 0x00 },
    };
    s_gpo->push_back(gpo);


    gpo = Genesys_Gpo();
    gpo.gpo_id = GPO_CANONLIDE120;
    gpo.regs = {
        { 0x6c, 0xfb },
        { 0x6d, 0x20 },
        { 0x6e, 0xff },
        { 0x6f, 0x00 },
    };
    s_gpo->push_back(gpo);


    gpo = Genesys_Gpo();
    gpo.gpo_id = GPO_CANONLIDE210;
    gpo.regs = {
        { 0x6c, 0xfb },
        { 0x6d, 0x20 },
        { 0x6e, 0xff },
        { 0x6f, 0x00 },
    };
    s_gpo->push_back(gpo);


    gpo = Genesys_Gpo();
    gpo.gpo_id = GPO_PLUSTEK_3600;
    gpo.regs = {
        { 0x6c, 0x02 },
        { 0x6d, 0x00 },
        { 0x6e, 0x1e },
        { 0x6f, 0x80 },
    };
    s_gpo->push_back(gpo);


    gpo = Genesys_Gpo();
    gpo.gpo_id = GPO_CS4400F;
    gpo.regs = {
        { 0x6c, 0x01 },
        { 0x6d, 0x7f },
        { 0x6e, 0xff },
        { 0x6f, 0x00 },
    };
    s_gpo->push_back(gpo);


    gpo = Genesys_Gpo();
    gpo.gpo_id = GPO_CS8400F;
    gpo.regs = {
        { 0x6c, 0x9a },
        { 0x6d, 0xdf },
        { 0x6e, 0xfe },
        { 0x6f, 0x60 },
    };
    s_gpo->push_back(gpo);


    gpo = Genesys_Gpo();
    gpo.gpo_id = GPO_CS8600F;
    gpo.regs = {
        { 0x6c, 0x20 },
        { 0x6d, 0x7c },
        { 0x6e, 0xff },
        { 0x6f, 0x00 },
    };
    s_gpo->push_back(gpo);


    gpo = Genesys_Gpo();
    gpo.gpo_id = GPO_IMG101;
    gpo.regs = {
        { 0x6c, 0x41 },
        { 0x6d, 0xa4 },
        { 0x6e, 0x13 },
        { 0x6f, 0xa7 },
    };
    s_gpo->push_back(gpo);


    gpo = Genesys_Gpo();
    gpo.gpo_id = GPO_PLUSTEK3800;
    gpo.regs = {
        { 0x6c, 0x41 },
        { 0x6d, 0xa4 },
        { 0x6e, 0x13 },
        { 0x6f, 0xa7 },
    };
    s_gpo->push_back(gpo);


    gpo = Genesys_Gpo();
    gpo.gpo_id = GPO_CANONLIDE80;
    gpo.regs = {
        { 0x6c, 0x28 },
        { 0x6d, 0x90 },
        { 0x6e, 0x75 },
        { 0x6f, 0x80 },
    };
    s_gpo->push_back(gpo);
}

StaticInit<std::vector<Genesys_Motor>> s_motors;

void genesys_init_motor_tables()
{
    s_motors.init();

    Genesys_Motor_Slope slope;

    Genesys_Motor motor;
    motor.motor_id = MOTOR_UMAX;
    motor.base_ydpi = 1200;
    motor.optical_ydpi = 2400;
    motor.max_step_type = 1;

    slope = Genesys_Motor_Slope();
    slope.maximum_start_speed = 11000;
    slope.maximum_speed = 3000;
    slope.minimum_steps = 128;
    slope.g = 1.0;
    motor.slopes.push_back(slope);

    slope = Genesys_Motor_Slope();
    slope.maximum_start_speed = 11000;
    slope.maximum_speed = 3000;
    slope.minimum_steps = 128;
    slope.g = 1.0;
    motor.slopes.push_back(slope);

    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.motor_id = MOTOR_5345; // MD5345/6228/6471
    motor.base_ydpi = 1200;
    motor.optical_ydpi = 2400;
    motor.max_step_type = 1;

    slope = Genesys_Motor_Slope();
    slope.maximum_start_speed = 2000;
    slope.maximum_speed = 1375;
    slope.minimum_steps = 128;
    slope.g = 0.5;
    motor.slopes.push_back(slope);

    slope = Genesys_Motor_Slope();
    slope.maximum_start_speed = 2000;
    slope.maximum_speed = 1375;
    slope.minimum_steps = 128;
    slope.g = 0.5;
    motor.slopes.push_back(slope);

    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.motor_id = MOTOR_ST24;
    motor.base_ydpi = 2400;
    motor.optical_ydpi = 2400;
    motor.max_step_type = 1;

    slope = Genesys_Motor_Slope();
    slope.maximum_start_speed = 2289;
    slope.maximum_speed = 2100;
    slope.minimum_steps = 128;
    slope.g = 0.3;
    motor.slopes.push_back(slope);

    slope = Genesys_Motor_Slope();
    slope.maximum_start_speed = 2289;
    slope.maximum_speed = 2100;
    slope.minimum_steps = 128;
    slope.g = 0.3;
    motor.slopes.push_back(slope);

    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.motor_id = MOTOR_HP3670;
    motor.base_ydpi = 1200;
    motor.optical_ydpi = 2400;
    motor.max_step_type = 1;

    slope = Genesys_Motor_Slope();
    slope.maximum_start_speed = 11000;
    slope.maximum_speed = 3000;
    slope.minimum_steps = 128;
    slope.g = 0.25;
    motor.slopes.push_back(slope);

    slope = Genesys_Motor_Slope();
    slope.maximum_start_speed = 11000;
    slope.maximum_speed = 3000;
    slope.minimum_steps = 128;
    slope.g = 0.5;
    motor.slopes.push_back(slope);

    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.motor_id = MOTOR_HP2400;
    motor.base_ydpi = 1200;
    motor.optical_ydpi = 1200;
    motor.max_step_type = 1;

    slope = Genesys_Motor_Slope();

    slope.maximum_start_speed = 11000;
    slope.maximum_speed = 3000;
    slope.minimum_steps = 128;
    slope.g = 0.25;
    motor.slopes.push_back(slope);

    slope = Genesys_Motor_Slope();
    slope.maximum_start_speed = 11000;
    slope.maximum_speed = 3000;
    slope.minimum_steps = 128;
    slope.g = 0.5;
    motor.slopes.push_back(slope);

    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.motor_id = MOTOR_HP2300;
    motor.base_ydpi = 600;
    motor.optical_ydpi = 1200;
    motor.max_step_type = 1;

    slope = Genesys_Motor_Slope();
    slope.maximum_start_speed = 3200;
    slope.maximum_speed = 1200;
    slope.minimum_steps = 128;
    slope.g = 0.5;
    motor.slopes.push_back(slope);

    slope = Genesys_Motor_Slope();
    slope.maximum_start_speed = 3200;
    slope.maximum_speed = 1200;
    slope.minimum_steps = 128;
    slope.g = 0.5;
    motor.slopes.push_back(slope);

    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.motor_id = MOTOR_CANONLIDE35;
    motor.base_ydpi = 1200;
    motor.optical_ydpi = 2400;
    motor.max_step_type = 1;

    slope = Genesys_Motor_Slope();
    slope.maximum_start_speed = 3500;
    slope.maximum_speed = 1300;
    slope.minimum_steps = 60;
    slope.g = 0.8;
    motor.slopes.push_back(slope);

    slope = Genesys_Motor_Slope();
    slope.maximum_start_speed = 3500;
    slope.maximum_speed = 1400;
    slope.minimum_steps = 60;
    slope.g = 0.8;
    motor.slopes.push_back(slope);

    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.motor_id = MOTOR_XP200;
    motor.base_ydpi = 600;
    motor.optical_ydpi = 600;
    motor.max_step_type = 1;

    slope = Genesys_Motor_Slope();
    slope.maximum_start_speed = 3500;
    slope.maximum_speed = 1300;
    slope.minimum_steps = 60;
    slope.g = 0.25;
    motor.slopes.push_back(slope);

    slope = Genesys_Motor_Slope();
    slope.maximum_start_speed = 3500;
    slope.maximum_speed = 1400;
    slope.minimum_steps = 60;
    slope.g = 0.5;
    motor.slopes.push_back(slope);

    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.motor_id = MOTOR_XP300;
    motor.base_ydpi = 300;
    motor.optical_ydpi = 600;
    motor.max_step_type = 1;

    slope = Genesys_Motor_Slope();
    // works best with GPIO10, GPIO14 off
    slope.maximum_start_speed = 3700;
    slope.maximum_speed = 3700;
    slope.minimum_steps = 2;
    slope.g = 0.8;
    motor.slopes.push_back(slope);

    slope = Genesys_Motor_Slope();
    slope.maximum_start_speed = 11000;
    slope.maximum_speed = 11000;
    slope.minimum_steps = 2;
    slope.g = 0.8;
    motor.slopes.push_back(slope);

    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.motor_id = MOTOR_DP665;
    motor.base_ydpi = 750;
    motor.optical_ydpi = 1500;
    motor.max_step_type = 1;

    slope = Genesys_Motor_Slope();
    slope.maximum_start_speed = 3000;
    slope.maximum_speed = 2500;
    slope.minimum_steps = 10;
    slope.g = 0.8;
    motor.slopes.push_back(slope);

    slope = Genesys_Motor_Slope();
    slope.maximum_start_speed = 11000;
    slope.maximum_speed = 11000;
    slope.minimum_steps = 2;
    slope.g = 0.8;
    motor.slopes.push_back(slope);

    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.motor_id = MOTOR_ROADWARRIOR;
    motor.base_ydpi = 750;
    motor.optical_ydpi = 1500;
    motor.max_step_type = 1;

    slope = Genesys_Motor_Slope();
    slope.maximum_start_speed = 3000;
    slope.maximum_speed = 2600;
    slope.minimum_steps = 10;
    slope.g = 0.8;
    motor.slopes.push_back(slope);

    slope = Genesys_Motor_Slope();
    slope.maximum_start_speed = 11000;
    slope.maximum_speed = 11000;
    slope.minimum_steps = 2;
    slope.g = 0.8;
    motor.slopes.push_back(slope);
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.motor_id = MOTOR_DSMOBILE_600;
    motor.base_ydpi = 750;
    motor.optical_ydpi = 1500;
    motor.max_step_type = 2;

    slope = Genesys_Motor_Slope();
    slope.maximum_start_speed = 6666;
    slope.maximum_speed = 3700;
    slope.minimum_steps = 8;
    slope.g = 0.8;
    motor.slopes.push_back(slope);

    slope = Genesys_Motor_Slope();
    slope.maximum_start_speed = 6666;
    slope.maximum_speed = 3700;
    slope.minimum_steps = 8;
    slope.g = 0.8;
    motor.slopes.push_back(slope);

    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.motor_id = MOTOR_CANONLIDE100;
    motor.base_ydpi = 1200;
    motor.optical_ydpi = 6400;
    motor.max_step_type = 2;

    slope = Genesys_Motor_Slope(); // full step
    slope.maximum_start_speed = 3000;
    slope.maximum_speed = 1000;
    slope.minimum_steps = 127;
    slope.g = 0.50;
    motor.slopes.push_back(slope);

    slope = Genesys_Motor_Slope(); // half step
    slope.maximum_start_speed = 3000;
    slope.maximum_speed = 1500;
    slope.minimum_steps = 127;
    slope.g = 0.50;
    motor.slopes.push_back(slope);

    slope = Genesys_Motor_Slope(); // quarter step 0.75*2712
    slope.maximum_start_speed = 3*2712;
    slope.maximum_speed = 3*2712;
    slope.minimum_steps = 16;
    slope.g = 0.80;
    motor.slopes.push_back(slope);

    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.motor_id = MOTOR_CANONLIDE200;
    motor.base_ydpi = 1200;
    motor.optical_ydpi = 6400;
    motor.max_step_type = 2;

    slope = Genesys_Motor_Slope(); // full step
    slope.maximum_start_speed = 3000;
    slope.maximum_speed = 1000;
    slope.minimum_steps = 127;
    slope.g = 0.50;
    motor.slopes.push_back(slope);

    slope = Genesys_Motor_Slope(); // half step
    slope.maximum_start_speed = 3000;
    slope.maximum_speed = 1500;
    slope.minimum_steps = 127;
    slope.g = 0.50;
    motor.slopes.push_back(slope);

    slope = Genesys_Motor_Slope(); // quarter step 0.75*2712
    slope.maximum_start_speed = 3*2712;
    slope.maximum_speed = 3*2712;
    slope.minimum_steps = 16;
    slope.g = 0.80;
    motor.slopes.push_back(slope);

    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.motor_id = MOTOR_CANONLIDE700;
    motor.base_ydpi = 1200;
    motor.optical_ydpi = 6400;
    motor.max_step_type = 2;

    slope = Genesys_Motor_Slope(); // full step
    slope.maximum_start_speed = 3000;
    slope.maximum_speed = 1000;
    slope.minimum_steps = 127;
    slope.g = 0.50;
    motor.slopes.push_back(slope);

    slope = Genesys_Motor_Slope(); // half step
    slope.maximum_start_speed = 3000;
    slope.maximum_speed = 1500;
    slope.minimum_steps = 127;
    slope.g = 0.50;
    motor.slopes.push_back(slope);

    slope = Genesys_Motor_Slope(); // quarter step 0.75*2712
    slope.maximum_start_speed = 3*2712;
    slope.maximum_speed = 3*2712;
    slope.minimum_steps = 16;
    slope.g = 0.80;
    motor.slopes.push_back(slope);

    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.motor_id = MOTOR_KVSS080;
    motor.base_ydpi = 1200;
    motor.optical_ydpi = 1200;
    motor.max_step_type = 2;

    slope = Genesys_Motor_Slope(); // max speed / dpi * base dpi => exposure
    slope.maximum_start_speed = 22222;
    slope.maximum_speed = 500;
    slope.minimum_steps = 246;
    slope.g = 0.5;
    motor.slopes.push_back(slope);

    slope = Genesys_Motor_Slope();
    slope.maximum_start_speed = 22222;
    slope.maximum_speed = 500;
    slope.minimum_steps = 246;
    slope.g = 0.5;
    motor.slopes.push_back(slope);

    slope = Genesys_Motor_Slope();
    slope.maximum_start_speed = 22222;
    slope.maximum_speed = 500;
    slope.minimum_steps = 246;
    slope.g = 0.5;
    motor.slopes.push_back(slope);

    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.motor_id = MOTOR_G4050;
    motor.base_ydpi = 2400;
    motor.optical_ydpi = 9600;
    motor.max_step_type = 2;

    slope = Genesys_Motor_Slope(); // full step
    slope.maximum_start_speed = 3961;
    slope.maximum_speed = 240;
    slope.minimum_steps = 246;
    slope.g = 0.8;
    motor.slopes.push_back(slope);

    slope = Genesys_Motor_Slope(); // half step
    slope.maximum_start_speed = 3961;
    slope.maximum_speed = 240;
    slope.minimum_steps = 246;
    slope.g = 0.8;
    motor.slopes.push_back(slope);

    slope = Genesys_Motor_Slope(); // quarter step
    slope.maximum_start_speed = 3961;
    slope.maximum_speed = 240;
    slope.minimum_steps = 246;
    slope.g = 0.8;
    motor.slopes.push_back(slope);

    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.motor_id = MOTOR_CS8400F;
    motor.base_ydpi = 1200;
    motor.optical_ydpi = 9600;
    motor.max_step_type = 2;

    slope = Genesys_Motor_Slope(); // full step
    slope.maximum_start_speed = 3961;
    slope.maximum_speed = 240;
    slope.minimum_steps = 246;
    slope.g = 0.8;
    motor.slopes.push_back(slope);

    slope = Genesys_Motor_Slope(); // half step
    slope.maximum_start_speed = 3961;
    slope.maximum_speed = 240;
    slope.minimum_steps = 246;
    slope.g = 0.8;
    motor.slopes.push_back(slope);

    slope = Genesys_Motor_Slope(); // quarter step
    slope.maximum_start_speed = 3961;
    slope.maximum_speed = 240;
    slope.minimum_steps = 246;
    slope.g = 0.8;
    motor.slopes.push_back(slope);

    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.motor_id = MOTOR_CS8600F;
    motor.base_ydpi = 2400;
    motor.optical_ydpi = 9600;
    motor.max_step_type = 2;

    slope = Genesys_Motor_Slope(); // full step
    slope.maximum_start_speed = 3961;
    slope.maximum_speed = 240;
    slope.minimum_steps = 246;
    slope.g = 0.8;
    motor.slopes.push_back(slope);

    slope = Genesys_Motor_Slope(); // half step
    slope.maximum_start_speed = 3961;
    slope.maximum_speed = 240;
    slope.minimum_steps = 246;
    slope.g = 0.8;
    motor.slopes.push_back(slope);

    slope = Genesys_Motor_Slope(); // quarter step
    slope.maximum_start_speed = 3961;
    slope.maximum_speed = 240;
    slope.minimum_steps = 246;
    slope.g = 0.8;
    motor.slopes.push_back(slope);

    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.motor_id = MOTOR_CANONLIDE110;
    motor.base_ydpi = 4800;
    motor.optical_ydpi = 9600;
    motor.max_step_type = 1;

    slope = Genesys_Motor_Slope(); // full step
    slope.maximum_start_speed = 3000;
    slope.maximum_speed = 1000;
    slope.minimum_steps = 256;
    slope.g = 0.50;
    motor.slopes.push_back(slope);

    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.motor_id = MOTOR_CANONLIDE120;
    motor.base_ydpi = 4800;
    motor.optical_ydpi = 9600;
    motor.max_step_type = 1;

    slope = Genesys_Motor_Slope();
    slope.maximum_start_speed = 3000;
    slope.maximum_speed = 1000;
    slope.minimum_steps = 256;
    slope.g = 0.50;
    motor.slopes.push_back(slope);

    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.motor_id = MOTOR_CANONLIDE210;
    motor.base_ydpi = 4800;
    motor.optical_ydpi = 9600;
    motor.max_step_type = 1;

    slope = Genesys_Motor_Slope();
    slope.maximum_start_speed = 3000;
    slope.maximum_speed = 1000;
    slope.minimum_steps = 256;
    slope.g = 0.50;
    motor.slopes.push_back(slope);

    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.motor_id = MOTOR_PLUSTEK_3600;
    motor.base_ydpi = 1200;
    motor.optical_ydpi = 2400;
    motor.max_step_type = 1;

    slope = Genesys_Motor_Slope();
    slope.maximum_start_speed = 3500;
    slope.maximum_speed = 1300;
    slope.minimum_steps = 60;
    slope.g = 0.8;
    motor.slopes.push_back(slope);

    slope = Genesys_Motor_Slope();
    slope.maximum_start_speed = 3500;
    slope.maximum_speed = 3250;
    slope.minimum_steps = 60;
    slope.g = 0.8;
    motor.slopes.push_back(slope);

    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.motor_id = MOTOR_IMG101;
    motor.base_ydpi = 600;
    motor.optical_ydpi = 1200;
    motor.max_step_type = 1;

    slope = Genesys_Motor_Slope();
    slope.maximum_start_speed = 3500;
    slope.maximum_speed = 1300;
    slope.minimum_steps = 60;
    slope.g = 0.8;
    motor.slopes.push_back(slope);

    slope = Genesys_Motor_Slope();
    slope.maximum_start_speed = 3500;
    slope.maximum_speed = 3250;
    slope.minimum_steps = 60;
    slope.g = 0.8;
    motor.slopes.push_back(slope);

    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.motor_id = MOTOR_PLUSTEK3800;
    motor.base_ydpi = 600;
    motor.optical_ydpi = 1200;
    motor.max_step_type = 1;

    slope = Genesys_Motor_Slope();
    slope.maximum_start_speed = 3500;
    slope.maximum_speed = 1300;
    slope.minimum_steps = 60;
    slope.g = 0.8;
    motor.slopes.push_back(slope);

    slope = Genesys_Motor_Slope();
    slope.maximum_start_speed = 3500;
    slope.maximum_speed = 3250;
    slope.minimum_steps = 60;
    slope.g = 0.8;
    motor.slopes.push_back(slope);

    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.motor_id = MOTOR_CANONLIDE80;
    motor.base_ydpi = 2400;
    motor.optical_ydpi = 4800, // 9600
    motor.max_step_type = 1;

    slope = Genesys_Motor_Slope();
    slope.maximum_start_speed = 9560;
    slope.maximum_speed = 1912;
    slope.minimum_steps = 31;
    slope.g = 0.8;
    motor.slopes.push_back(slope);

    s_motors->push_back(std::move(motor));
}

StaticInit<std::vector<Genesys_USB_Device_Entry>> s_usb_devices;

void genesys_init_usb_device_tables()
{
    s_usb_devices.init();

    Genesys_Model model;
    model.name = "umax-astra-4500";
    model.vendor = "UMAX";
    model.model = "Astra 4500";
    model.model_id = MODEL_UMAX_ASTRA_4500;
    model.asic_type = AsicType::GL646;

    model.xdpi_values = { 1200, 600, 300, 150, 75 };
    model.ydpi_values = { 2400, 1200, 600, 300, 150, 75 };
    model.bpp_gray_values = { 16, 8 };
    model.bpp_color_values = { 16, 8 };

    model.x_offset = SANE_FIX(3.5);
    model.y_offset = SANE_FIX(7.5);
    model.x_size = SANE_FIX(218.0);
    model.y_size = SANE_FIX(299.0);

    model.y_offset_calib = SANE_FIX(0.0);
    model.x_offset_mark = SANE_FIX(1.0);

    model.x_offset_ta = SANE_FIX(0.0);
    model.y_offset_ta = SANE_FIX(0.0);
    model.x_size_ta = SANE_FIX(100.0);
    model.y_size_ta = SANE_FIX(100.0);

    model.y_offset_sensor_to_ta = SANE_FIX(0.0);
    model.y_offset_calib_ta = SANE_FIX(0.0);

    model.post_scan = SANE_FIX(0.0);
    model.eject_feed = SANE_FIX(0.0);

    model.ld_shift_r = 0;
    model.ld_shift_g = 8;
    model.ld_shift_b = 16;

    model.line_mode_color_order = COLOR_ORDER_BGR;

    model.is_cis = SANE_FALSE;
    model.is_sheetfed = SANE_FALSE;
    model.ccd_type = CCD_UMAX;
    model.dac_type = DAC_WOLFSON_UMAX;
    model.gpo_type = GPO_UMAX;
    model.motor_type = MOTOR_UMAX;
    model.flags = GENESYS_FLAG_UNTESTED;
    model.buttons = GENESYS_HAS_NO_BUTTONS;
    model.shading_lines = 20;
    model.shading_ta_lines = 0;
    model.search_lines = 200;

    s_usb_devices->emplace_back(0x0638, 0x0a10, model);


    model = Genesys_Model();
    model.name = "canon-lide-50";
    model.vendor = "Canon";
    model.model = "LiDE 35/40/50";
    model.model_id = MODEL_CANON_LIDE_50;
    model.asic_type = AsicType::GL841;

    model.xdpi_values = { 1200, 600, 400, 300, 240, 200, 150, 75 };
    model.ydpi_values = { 2400, 1200, 600, 400, 300, 240, 200, 150, 75 };
    model.bpp_gray_values = { 16, 8 };
    model.bpp_color_values = { 16, 8 };

    model.x_offset = SANE_FIX(0.42);
    model.y_offset = SANE_FIX(7.9);
    model.x_size = SANE_FIX(218.0);
    model.y_size = SANE_FIX(299.0);

    model.y_offset_calib = SANE_FIX(6.0);
    model.x_offset_mark = SANE_FIX(0.0);

    model.x_offset_ta = SANE_FIX(0.0);
    model.y_offset_ta = SANE_FIX(0.0);
    model.x_size_ta = SANE_FIX(100.0);
    model.y_size_ta = SANE_FIX(100.0);

    model.y_offset_sensor_to_ta = SANE_FIX(0.0);
    model.y_offset_calib_ta = SANE_FIX(0.0);

    model.post_scan = SANE_FIX(0.0);
    model.eject_feed = SANE_FIX(0.0);

    model.ld_shift_r = 0;
    model.ld_shift_g = 0;
    model.ld_shift_b = 0;

    model.line_mode_color_order = COLOR_ORDER_RGB;

    model.is_cis = SANE_TRUE;
    model.is_sheetfed = SANE_FALSE;
    model.ccd_type = CCD_CANONLIDE35;
    model.dac_type = DAC_CANONLIDE35;
    model.gpo_type = GPO_CANONLIDE35;
    model.motor_type = MOTOR_CANONLIDE35;
    model.flags = GENESYS_FLAG_LAZY_INIT |
                  GENESYS_FLAG_SKIP_WARMUP |
                  GENESYS_FLAG_OFFSET_CALIBRATION |
                  GENESYS_FLAG_DARK_WHITE_CALIBRATION |
                  GENESYS_FLAG_CUSTOM_GAMMA;
    model.buttons = GENESYS_HAS_SCAN_SW |
                    GENESYS_HAS_FILE_SW |
                    GENESYS_HAS_EMAIL_SW |
                    GENESYS_HAS_COPY_SW;
    model.shading_lines = 280;
    model.shading_ta_lines = 0;
    model.search_lines = 400;

    s_usb_devices->emplace_back(0x04a9, 0x2213, model);


    model = Genesys_Model();
    model.name = "panasonic-kv-ss080";
    model.vendor = "Panasonic";
    model.model = "KV-SS080";
    model.model_id = MODEL_PANASONIC_KV_SS080;
    model.asic_type = AsicType::GL843;

    model.xdpi_values = { 600, /* 500, 400,*/ 300, 200, 150, 100, 75 };
    model.ydpi_values = { 1200, 600, /* 500, 400, */ 300, 200, 150, 100, 75 };
    model.bpp_gray_values = { 16, 8 };
    model.bpp_color_values = { 16, 8 };

    model.x_offset = SANE_FIX(7.2);
    model.y_offset = SANE_FIX(14.7);
    model.x_size = SANE_FIX(217.7);
    model.y_size = SANE_FIX(300.0);

    model.y_offset_calib = SANE_FIX(9.0);
    model.x_offset_mark = SANE_FIX(0.0);

    model.x_offset_ta = SANE_FIX(0.0);
    model.y_offset_ta = SANE_FIX(0.0);
    model.x_size_ta = SANE_FIX(0.0);
    model.y_size_ta = SANE_FIX(0.0);

    model.y_offset_sensor_to_ta = SANE_FIX(0.0);
    model.y_offset_calib_ta = SANE_FIX(0.0);

    model.post_scan = SANE_FIX(0.0);
    model.eject_feed = SANE_FIX(0.0);

    model.ld_shift_r = 0;
    model.ld_shift_g = 8;
    model.ld_shift_b = 16;

    model.line_mode_color_order = COLOR_ORDER_RGB;

    model.is_cis = SANE_FALSE;
    model.is_sheetfed = SANE_FALSE;
    model.ccd_type = CCD_KVSS080;
    model.dac_type = DAC_KVSS080;
    model.gpo_type = GPO_KVSS080;
    model.motor_type = MOTOR_KVSS080;
    model.flags = GENESYS_FLAG_LAZY_INIT |
                  GENESYS_FLAG_SKIP_WARMUP |
                  GENESYS_FLAG_OFFSET_CALIBRATION |
                  GENESYS_FLAG_CUSTOM_GAMMA;
    model.buttons = GENESYS_HAS_SCAN_SW;
    model.shading_lines = 100;
    model.shading_ta_lines = 0;
    model.search_lines = 100;

    s_usb_devices->emplace_back(0x04da, 0x100f, model);


    model = Genesys_Model();
    model.name = "hewlett-packard-scanjet-4850c";
    model.vendor = "Hewlett Packard";
    model.model = "ScanJet 4850C";
    model.model_id = MODEL_HP_SCANJET_4850C;
    model.asic_type = AsicType::GL843;

    model.xdpi_values = { 2400, 1200, 600, 400, 300, 200, 150, 100 };
    model.ydpi_values = { 2400, 1200, 600, 400, 300, 200, 150, 100 };
    model.bpp_gray_values = { 16, 8 };
    model.bpp_color_values = { 16, 8 };

    model.x_offset = SANE_FIX(7.9);
    model.y_offset = SANE_FIX(5.9);
    model.x_size = SANE_FIX(219.6);
    model.y_size = SANE_FIX(314.5);

    model.y_offset_calib = SANE_FIX(3.0);
    model.x_offset_mark = SANE_FIX(0.0);

    model.x_offset_ta = SANE_FIX(0.0);
    model.y_offset_ta = SANE_FIX(0.0);
    model.x_size_ta = SANE_FIX(100.0);
    model.y_size_ta = SANE_FIX(100.0);

    model.y_offset_sensor_to_ta = SANE_FIX(0.0);
    model.y_offset_calib_ta = SANE_FIX(0.0);

    model.post_scan = SANE_FIX(0.0);
    model.eject_feed = SANE_FIX(0.0);

    model.ld_shift_r = 0;
    model.ld_shift_g = 24;
    model.ld_shift_b = 48;

    model.line_mode_color_order = COLOR_ORDER_RGB;

    model.is_cis = SANE_FALSE;
    model.is_sheetfed = SANE_FALSE;
    model.ccd_type = CCD_G4050;
    model.dac_type = DAC_G4050;
    model.gpo_type = GPO_G4050;
    model.motor_type = MOTOR_G4050;
    model.flags = GENESYS_FLAG_LAZY_INIT |
                  GENESYS_FLAG_OFFSET_CALIBRATION |
                  GENESYS_FLAG_STAGGERED_LINE |
                  GENESYS_FLAG_SKIP_WARMUP |
                  GENESYS_FLAG_DARK_CALIBRATION |
                  GENESYS_FLAG_CUSTOM_GAMMA;
    model.buttons = GENESYS_HAS_SCAN_SW | GENESYS_HAS_FILE_SW | GENESYS_HAS_COPY_SW;
    model.shading_lines = 100;
    model.shading_ta_lines = 0;
    model.search_lines = 100;
    s_usb_devices->emplace_back(0x03f0, 0x1b05, model);


    model = Genesys_Model();
    model.name = "hewlett-packard-scanjet-g4010";
    model.vendor = "Hewlett Packard";
    model.model = "ScanJet G4010";
    model.model_id = MODEL_HP_SCANJET_G4010;
    model.asic_type = AsicType::GL843;

    model.xdpi_values = { 2400, 1200, 600, 400, 300, 200, 150, 100 };
    model.ydpi_values = { 2400, 1200, 600, 400, 300, 200, 150, 100 };
    model.bpp_gray_values = { 16, 8 };
    model.bpp_color_values = { 16, 8 };

    model.x_offset = SANE_FIX(8.0);
    model.y_offset = SANE_FIX(13.00);
    model.x_size = SANE_FIX(217.9);
    model.y_size = SANE_FIX(315.0);

    model.y_offset_calib = SANE_FIX(3.0);
    model.x_offset_mark = SANE_FIX(0.0);

    model.x_offset_ta = SANE_FIX(0.0);
    model.y_offset_ta = SANE_FIX(0.0);
    model.x_size_ta = SANE_FIX(100.0);
    model.y_size_ta = SANE_FIX(100.0);

    model.y_offset_sensor_to_ta = SANE_FIX(0.0);
    model.y_offset_calib_ta = SANE_FIX(0.0);

    model.post_scan = SANE_FIX(0.0);
    model.eject_feed = SANE_FIX(0.0);

    model.ld_shift_r = 0;
    model.ld_shift_g = 24;
    model.ld_shift_b = 48;
    model.line_mode_color_order = COLOR_ORDER_RGB;

    model.is_cis = SANE_FALSE;
    model.is_sheetfed = SANE_FALSE;
    model.ccd_type = CCD_G4050;
    model.dac_type = DAC_G4050;
    model.gpo_type = GPO_G4050;
    model.motor_type = MOTOR_G4050;
    model.flags = GENESYS_FLAG_LAZY_INIT |
                  GENESYS_FLAG_OFFSET_CALIBRATION |
                  GENESYS_FLAG_STAGGERED_LINE |
                  GENESYS_FLAG_SKIP_WARMUP |
                  GENESYS_FLAG_DARK_CALIBRATION |
                  GENESYS_FLAG_CUSTOM_GAMMA;
    model.buttons = GENESYS_HAS_SCAN_SW | GENESYS_HAS_FILE_SW | GENESYS_HAS_COPY_SW;
    model.shading_lines = 100;
    model.shading_ta_lines = 0;
    model.search_lines = 100;

    s_usb_devices->emplace_back(0x03f0, 0x4505, model);


    model = Genesys_Model();
    model.name = "hewlett-packard-scanjet-g4050";
    model.vendor = "Hewlett Packard";
    model.model = "ScanJet G4050";
    model.model_id = MODEL_HP_SCANJET_G4050;
    model.asic_type = AsicType::GL843;

    model.xdpi_values = { 2400, 1200, 600, 400, 300, 200, 150, 100 };
    model.ydpi_values = { 2400, 1200, 600, 400, 300, 200, 150, 100 };
    model.bpp_gray_values = { 16, 8 };
    model.bpp_color_values = { 16, 8 };

    model.x_offset = SANE_FIX(8.0);
    model.y_offset = SANE_FIX(13.00);
    model.x_size = SANE_FIX(217.9);
    model.y_size = SANE_FIX(315.0);

    model.y_offset_calib = SANE_FIX(3.0);
    model.x_offset_mark = SANE_FIX(0.0);

    model.x_offset_ta = SANE_FIX(8.0);
    model.y_offset_ta = SANE_FIX(13.00);
    model.x_size_ta = SANE_FIX(217.9);
    model.y_size_ta = SANE_FIX(250.0);

    model.y_offset_sensor_to_ta = SANE_FIX(0.0);
    model.y_offset_calib_ta = SANE_FIX(40.0);

    model.post_scan = SANE_FIX(0.0);
    model.eject_feed = SANE_FIX(0.0);

    model.ld_shift_r = 0;
    model.ld_shift_g = 24;
    model.ld_shift_b = 48;

    model.line_mode_color_order = COLOR_ORDER_RGB;

    model.is_cis = SANE_FALSE;
    model.is_sheetfed = SANE_FALSE;
    model.ccd_type = CCD_G4050;
    model.dac_type = DAC_G4050;
    model.gpo_type = GPO_G4050;
    model.motor_type = MOTOR_G4050;
    model.flags = GENESYS_FLAG_LAZY_INIT |
                  GENESYS_FLAG_OFFSET_CALIBRATION |
                  GENESYS_FLAG_STAGGERED_LINE |
                  GENESYS_FLAG_SKIP_WARMUP |
                  GENESYS_FLAG_DARK_CALIBRATION |
                  GENESYS_FLAG_CUSTOM_GAMMA;
    model.buttons = GENESYS_HAS_SCAN_SW | GENESYS_HAS_FILE_SW | GENESYS_HAS_COPY_SW;
    model.shading_lines = 100;
    model.shading_ta_lines = 0;
    model.search_lines = 100;

    s_usb_devices->emplace_back(0x03f0, 0x4605, model);


    model = Genesys_Model();
    model.name = "canon-canoscan-4400f";
    model.vendor = "Canon";
    model.model = "Canoscan 4400f";
    model.model_id = MODEL_CANON_CANOSCAN_4400F;
    model.asic_type = AsicType::GL843;

    model.xdpi_values = { 4800, 2400, 1200, 600, 400, 300, 200, 150, 100 };
    model.ydpi_values = { 4800, 2400, 1200, 600, 400, 300, 200, 150, 100 };
    model.bpp_gray_values = { 16, 8 };
    model.bpp_color_values = { 16, 8 };

    model.x_offset = SANE_FIX(6.0);
    model.y_offset = SANE_FIX(13.00);
    model.x_size = SANE_FIX(217.9);
    model.y_size = SANE_FIX(315.0);

    model.y_offset_calib = SANE_FIX(3.0);
    model.x_offset_mark = SANE_FIX(0.0);

    model.x_offset_ta = SANE_FIX(8.0);
    model.y_offset_ta = SANE_FIX(13.00);
    model.x_size_ta = SANE_FIX(217.9);
    model.y_size_ta = SANE_FIX(250.0);

    model.y_offset_sensor_to_ta = SANE_FIX(0.0);
    model.y_offset_calib_ta = SANE_FIX(40.0);

    model.post_scan = SANE_FIX(0.0);
    model.eject_feed = SANE_FIX(0.0);

    model.ld_shift_r = 0;
    model.ld_shift_g = 24;
    model.ld_shift_b = 48;

    model.line_mode_color_order = COLOR_ORDER_RGB;

    model.is_cis = SANE_FALSE;
    model.is_sheetfed = SANE_FALSE;
    model.ccd_type = CCD_CS4400F;
    model.dac_type = DAC_G4050;
    model.gpo_type = GPO_CS4400F;
    model.motor_type = MOTOR_G4050;
    model.flags = GENESYS_FLAG_NO_CALIBRATION |
                  GENESYS_FLAG_LAZY_INIT |
                  GENESYS_FLAG_OFFSET_CALIBRATION |
                  GENESYS_FLAG_STAGGERED_LINE |
                  GENESYS_FLAG_SKIP_WARMUP |
                  GENESYS_FLAG_DARK_CALIBRATION |
                  GENESYS_FLAG_FULL_HWDPI_MODE |
                  GENESYS_FLAG_CUSTOM_GAMMA;
    model.buttons = GENESYS_HAS_SCAN_SW | GENESYS_HAS_FILE_SW | GENESYS_HAS_COPY_SW;
    model.shading_lines = 100;
    model.shading_ta_lines = 0;
    model.search_lines = 100;

    s_usb_devices->emplace_back(0x04a9, 0x2228, model);


    model = Genesys_Model();
    model.name = "canon-canoscan-8400f";
    model.vendor = "Canon";
    model.model = "Canoscan 8400f";
    model.model_id = MODEL_CANON_CANOSCAN_8400F;
    model.asic_type = AsicType::GL843;

    model.xdpi_values = { 4800, 2400, 1200, 600, 400, 300, 200, 150, 100 };
    model.ydpi_values = { 4800, 2400, 1200, 600, 400, 300, 200, 150, 100 };
    model.bpp_gray_values = { 16, 8 };
    model.bpp_color_values = { 16, 8 };

    model.x_offset = SANE_FIX(3.5);
    model.y_offset = SANE_FIX(17.00);
    model.x_size = SANE_FIX(219.9);
    model.y_size = SANE_FIX(315.0);

    model.y_offset_calib = SANE_FIX(0.0);
    model.x_offset_mark = SANE_FIX(10.0);

    model.x_offset_ta = SANE_FIX(100.0);
    model.y_offset_ta = SANE_FIX(50.00);
    model.x_size_ta = SANE_FIX(100.0);
    model.y_size_ta = SANE_FIX(230.0);

    model.y_offset_sensor_to_ta = SANE_FIX(22.0);
    model.y_offset_calib_ta = SANE_FIX(25.0);

    model.post_scan = SANE_FIX(0.0);
    model.eject_feed = SANE_FIX(0.0);

    model.ld_shift_r = 0;
    model.ld_shift_g = 24;
    model.ld_shift_b = 48;

    model.line_mode_color_order = COLOR_ORDER_RGB;

    model.is_cis = SANE_FALSE;
    model.is_sheetfed = SANE_FALSE;
    model.ccd_type = CCD_CS8400F;
    model.dac_type = DAC_CS8400F;
    model.gpo_type = GPO_CS8400F;
    model.motor_type = MOTOR_CS8400F;
    model.flags = GENESYS_FLAG_HAS_UTA |
                  GENESYS_FLAG_HAS_UTA_INFRARED |
                  GENESYS_FLAG_LAZY_INIT |
                  GENESYS_FLAG_OFFSET_CALIBRATION |
                  GENESYS_FLAG_STAGGERED_LINE |
                  GENESYS_FLAG_SKIP_WARMUP |
                  GENESYS_FLAG_DARK_CALIBRATION |
                  GENESYS_FLAG_FULL_HWDPI_MODE |
                  GENESYS_FLAG_CUSTOM_GAMMA |
                  GENESYS_FLAG_SHADING_REPARK;
    model.buttons = GENESYS_HAS_SCAN_SW | GENESYS_HAS_FILE_SW | GENESYS_HAS_COPY_SW;
    model.shading_lines = 100;
    model.shading_ta_lines = 50;
    model.search_lines = 100;

    s_usb_devices->emplace_back(0x04a9, 0x221e, model);


    model = Genesys_Model();
    model.name = "canon-canoscan-8600f";
    model.vendor = "Canon";
    model.model = "Canoscan 8600f";
    model.model_id = MODEL_CANON_CANOSCAN_8600F;
    model.asic_type = AsicType::GL843;

    model.xdpi_values = { 4800, 2400, 1200, 600, 400, 300 }; // TODO: resolutions for non-XPA mode
    model.ydpi_values = { 4800, 2400, 1200, 600, 400, 300 }; // TODO: resolutions for non-XPA mode
    model.bpp_gray_values = { 16, 8 };
    model.bpp_color_values = { 16, 8 };

    model.x_offset = SANE_FIX(24.0);
    model.y_offset = SANE_FIX(10.0);
    model.x_size = SANE_FIX(216.0);
    model.y_size = SANE_FIX(297.0);

    model.y_offset_calib = SANE_FIX(0.0);
    model.x_offset_mark = SANE_FIX(8.0);

    model.x_offset_ta = SANE_FIX(85.0);
    model.y_offset_ta = SANE_FIX(26.0);
    model.x_size_ta = SANE_FIX(70.0);
    model.y_size_ta = SANE_FIX(230.0);

    model.y_offset_sensor_to_ta = SANE_FIX(11.5);
    model.y_offset_calib_ta = SANE_FIX(14.0);

    model.post_scan = SANE_FIX(0.0);
    model.eject_feed = SANE_FIX(0.0);

    model.ld_shift_r = 0;
    model.ld_shift_g = 48;
    model.ld_shift_b = 96;

    model.line_mode_color_order = COLOR_ORDER_RGB;

    model.is_cis = SANE_FALSE;
    model.is_sheetfed = SANE_FALSE;
    model.ccd_type = CCD_CS8600F;
    model.dac_type = DAC_CS8600F;
    model.gpo_type = GPO_CS8600F;
    model.motor_type = MOTOR_CS8600F;
    model.flags = GENESYS_FLAG_HAS_UTA |
                  GENESYS_FLAG_HAS_UTA_INFRARED |
                  GENESYS_FLAG_LAZY_INIT |
                  GENESYS_FLAG_OFFSET_CALIBRATION |
                  GENESYS_FLAG_STAGGERED_LINE |
                  GENESYS_FLAG_SKIP_WARMUP |
                  GENESYS_FLAG_DARK_CALIBRATION |
                  GENESYS_FLAG_FULL_HWDPI_MODE |
                  GENESYS_FLAG_CUSTOM_GAMMA |
                  GENESYS_FLAG_SHADING_REPARK;
    model.buttons = GENESYS_HAS_SCAN_SW | GENESYS_HAS_FILE_SW | GENESYS_HAS_COPY_SW;
    model.shading_lines = 50;
    model.shading_ta_lines = 50;
    model.search_lines = 100;

    s_usb_devices->emplace_back(0x04a9, 0x2229, model);


    model = Genesys_Model();
    model.name = "canon-lide-100";
    model.vendor = "Canon";
    model.model = "LiDE 100";
    model.model_id = MODEL_CANON_LIDE_100;
    model.asic_type = AsicType::GL847;

    model.xdpi_values = { 4800, 2400, 1200, 600, 300, 200, 150, 100, 75 };
    model.ydpi_values = { 4800, 2400, 1200, 600, 300, 200, 150, 100, 75 };
    model.bpp_gray_values = { 16, 8 };
    model.bpp_color_values = { 16, 8 };

    model.x_offset = SANE_FIX(1.1);
    model.y_offset = SANE_FIX(8.3);
    model.x_size = SANE_FIX(216.07);
    model.y_size = SANE_FIX(299.0);

    model.y_offset_calib = SANE_FIX(1.0);
    model.x_offset_mark = SANE_FIX(0.0);

    model.x_offset_ta = SANE_FIX(0.0);
    model.y_offset_ta = SANE_FIX(0.0);
    model.x_size_ta = SANE_FIX(100.0);
    model.y_size_ta = SANE_FIX(100.0);

    model.y_offset_sensor_to_ta = SANE_FIX(0.0);
    model.y_offset_calib_ta = SANE_FIX(0.0);

    model.post_scan = SANE_FIX(0.0);
    model.eject_feed = SANE_FIX(0.0);

    model.ld_shift_r = 0;
    model.ld_shift_g = 0;
    model.ld_shift_b = 0;

    model.line_mode_color_order = COLOR_ORDER_RGB;

    model.is_cis = SANE_TRUE;
    model.is_sheetfed = SANE_FALSE;
    model.ccd_type = CIS_CANONLIDE100;
    model.dac_type = DAC_CANONLIDE200;
    model.gpo_type = GPO_CANONLIDE200;
    model.motor_type = MOTOR_CANONLIDE100;
    model.flags = GENESYS_FLAG_SKIP_WARMUP |
                  GENESYS_FLAG_SIS_SENSOR |
                  GENESYS_FLAG_DARK_CALIBRATION |
                  GENESYS_FLAG_SHADING_REPARK |
                  GENESYS_FLAG_OFFSET_CALIBRATION |
                  GENESYS_FLAG_CUSTOM_GAMMA;
    model.buttons = GENESYS_HAS_SCAN_SW |
                    GENESYS_HAS_COPY_SW |
                    GENESYS_HAS_EMAIL_SW |
                    GENESYS_HAS_FILE_SW;
    model.shading_lines = 50;
    model.shading_ta_lines = 0;
    model.search_lines = 400;

    s_usb_devices->emplace_back(0x04a9, 0x1904, model);


    model = Genesys_Model();
    model.name = "canon-lide-110";
    model.vendor = "Canon";
    model.model = "LiDE 110";
    model.model_id = MODEL_CANON_LIDE_110;
    model.asic_type = AsicType::GL124;

    model.xdpi_values = { 4800, 2400, 1200, 600, /* 400,*/ 300, 150, 100, 75 };
    model.ydpi_values = { 4800, 2400, 1200, 600, /* 400,*/ 300, 150, 100, 75 };
    model.bpp_gray_values = { 16, 8 };
    model.bpp_color_values = { 16, 8 };

    model.x_offset = SANE_FIX(2.2);
    model.y_offset = SANE_FIX(9.0);
    model.x_size = SANE_FIX(216.70);
    model.y_size = SANE_FIX(300.0);

    model.y_offset_calib = SANE_FIX(1.0);
    model.x_offset_mark = SANE_FIX(0.0);

    model.x_offset_ta = SANE_FIX(0.0);
    model.y_offset_ta = SANE_FIX(0.0);
    model.x_size_ta = SANE_FIX(100.0);
    model.y_size_ta = SANE_FIX(100.0);

    model.y_offset_sensor_to_ta = SANE_FIX(0.0);
    model.y_offset_calib_ta = SANE_FIX(0.0);

    model.post_scan = SANE_FIX(0.0);
    model.eject_feed = SANE_FIX(0.0);

    model.ld_shift_r = 0;
    model.ld_shift_g = 0;
    model.ld_shift_b = 0;
    model.line_mode_color_order = COLOR_ORDER_RGB;

    model.is_cis = SANE_TRUE;
    model.is_sheetfed = SANE_FALSE;
    model.ccd_type = CIS_CANONLIDE110;
    model.dac_type = DAC_CANONLIDE110;
    model.gpo_type = GPO_CANONLIDE110;
    model.motor_type = MOTOR_CANONLIDE110;
    model.flags = GENESYS_FLAG_SKIP_WARMUP |
                  GENESYS_FLAG_OFFSET_CALIBRATION |
                  GENESYS_FLAG_DARK_CALIBRATION |
                  GENESYS_FLAG_SHADING_REPARK |
                  GENESYS_FLAG_CUSTOM_GAMMA;
    model.buttons = GENESYS_HAS_SCAN_SW |
                    GENESYS_HAS_COPY_SW |
                    GENESYS_HAS_EMAIL_SW |
                    GENESYS_HAS_FILE_SW;
    model.shading_lines = 50;
    model.shading_ta_lines = 0;
    model.search_lines = 400;

    s_usb_devices->emplace_back(0x04a9, 0x1909, model);


    model = Genesys_Model();
    model.name = "canon-lide-120";
    model.vendor = "Canon";
    model.model = "LiDE 120";
    model.model_id = MODEL_CANON_LIDE_120;
    model.asic_type = AsicType::GL124;

    model.xdpi_values = { 4800, 2400, 1200, 600, 300, 150, 100, 75 };
    model.ydpi_values = { 4800, 2400, 1200, 600, 300, 150, 100, 75 };
    model.bpp_gray_values = { 16, 8 };
    model.bpp_color_values = { 16, 8 };

    model.x_offset = SANE_FIX(0.0);
    model.y_offset = SANE_FIX(8.0);
    model.x_size = SANE_FIX(216.0);
    model.y_size = SANE_FIX(300.0);

    model.y_offset_calib = SANE_FIX(1.0);
    model.x_offset_mark = SANE_FIX(0.0);

    model.x_offset_ta = SANE_FIX(0.0);
    model.y_offset_ta = SANE_FIX(0.0);
    model.x_size_ta = SANE_FIX(100.0);
    model.y_size_ta = SANE_FIX(100.0);

    model.y_offset_sensor_to_ta = SANE_FIX(0.0);
    model.y_offset_calib_ta = SANE_FIX(0.0);

    model.post_scan = SANE_FIX(0.0);
    model.eject_feed = SANE_FIX(0.0);

    model.ld_shift_r = 0;
    model.ld_shift_g = 0;
    model.ld_shift_b = 0;
    model.line_mode_color_order = COLOR_ORDER_RGB;
    model.is_cis = SANE_TRUE;
    model.is_sheetfed = SANE_FALSE;
    model.ccd_type = CIS_CANONLIDE120;
    model.dac_type = DAC_CANONLIDE120;
    model.gpo_type = GPO_CANONLIDE120;
    model.motor_type = MOTOR_CANONLIDE120;
    model.flags = GENESYS_FLAG_SKIP_WARMUP |
                  GENESYS_FLAG_OFFSET_CALIBRATION |
                  GENESYS_FLAG_DARK_CALIBRATION |
                  GENESYS_FLAG_SHADING_REPARK |
                  GENESYS_FLAG_CUSTOM_GAMMA;
    model.buttons = GENESYS_HAS_SCAN_SW |
                    GENESYS_HAS_COPY_SW |
                    GENESYS_HAS_EMAIL_SW |
                    GENESYS_HAS_FILE_SW;
    model.shading_lines = 50;
    model.shading_ta_lines = 0;
    model.search_lines = 400;

    s_usb_devices->emplace_back(0x04a9, 0x190e, model);


    model = Genesys_Model();
    model.name = "canon-lide-210";
    model.vendor = "Canon";
    model.model = "LiDE 210";
    model.model_id = MODEL_CANON_LIDE_210;
    model.asic_type = AsicType::GL124;

    model.xdpi_values = { 4800, 2400, 1200, 600, /* 400,*/ 300, 150, 100, 75 };
    model.ydpi_values = { 4800, 2400, 1200, 600, /* 400,*/ 300, 150, 100, 75 };
    model.bpp_gray_values = { 16, 8 };
    model.bpp_color_values = { 16, 8 };

    model.x_offset = SANE_FIX(2.2);
    model.y_offset = SANE_FIX(8.7);
    model.x_size = SANE_FIX(216.70);
    model.y_size = SANE_FIX(297.5);

    model.y_offset_calib = SANE_FIX(0.0);
    model.x_offset_mark = SANE_FIX(0.0);

    model.x_offset_ta = SANE_FIX(0.0);
    model.y_offset_ta = SANE_FIX(0.0);
    model.x_size_ta = SANE_FIX(100.0);
    model.y_size_ta = SANE_FIX(100.0);

    model.y_offset_sensor_to_ta = SANE_FIX(0.0);
    model.y_offset_calib_ta = SANE_FIX(0.0);

    model.post_scan = SANE_FIX(0.0);
    model.eject_feed = SANE_FIX(0.0);

    model.ld_shift_r = 0;
    model.ld_shift_g = 0;
    model.ld_shift_b = 0;

    model.line_mode_color_order = COLOR_ORDER_RGB;

    model.is_cis = SANE_TRUE;
    model.is_sheetfed = SANE_FALSE;
    model.ccd_type = CIS_CANONLIDE210;
    model.dac_type = DAC_CANONLIDE110;
    model.gpo_type = GPO_CANONLIDE210;
    model.motor_type = MOTOR_CANONLIDE210;
    model.flags = GENESYS_FLAG_SKIP_WARMUP |
                  GENESYS_FLAG_OFFSET_CALIBRATION |
                  GENESYS_FLAG_DARK_CALIBRATION |
                  GENESYS_FLAG_SHADING_REPARK |
                  GENESYS_FLAG_CUSTOM_GAMMA;
    model.buttons = GENESYS_HAS_SCAN_SW |
                    GENESYS_HAS_COPY_SW |
                    GENESYS_HAS_EMAIL_SW |
                    GENESYS_HAS_FILE_SW |
                    GENESYS_HAS_EXTRA_SW;
    model.shading_lines = 60;
    model.shading_ta_lines = 0;
    model.search_lines = 400;

    s_usb_devices->emplace_back(0x04a9, 0x190a, model);


    model = Genesys_Model();
    model.name = "canon-lide-220";
    model.vendor = "Canon";
    model.model = "LiDE 220";
    model.model_id = MODEL_CANON_LIDE_220;
    model.asic_type = AsicType::GL124; // or a compatible one

    model.xdpi_values = { 4800, 2400, 1200, 600, 300, 150, 100, 75 };
    model.ydpi_values = { 4800, 2400, 1200, 600, 300, 150, 100, 75 };
    model.bpp_gray_values = { 16, 8 };
    model.bpp_color_values = { 16, 8 };

    model.x_offset = SANE_FIX(2.2);
    model.y_offset = SANE_FIX(8.7);
    model.x_size = SANE_FIX(216.70);
    model.y_size = SANE_FIX(297.5);

    model.y_offset_calib = SANE_FIX(0.0);
    model.x_offset_mark = SANE_FIX(0.0);

    model.x_offset_ta = SANE_FIX(0.0);
    model.y_offset_ta = SANE_FIX(0.0);
    model.x_size_ta = SANE_FIX(100.0);
    model.y_size_ta = SANE_FIX(100.0);

    model.y_offset_sensor_to_ta = SANE_FIX(0.0);
    model.y_offset_calib_ta = SANE_FIX(0.0);

    model.post_scan = SANE_FIX(0.0);
    model.eject_feed = SANE_FIX(0.0);

    model.ld_shift_r = 0;
    model.ld_shift_g = 0;
    model.ld_shift_b = 0;

    model.line_mode_color_order = COLOR_ORDER_RGB;
    model.is_cis = SANE_TRUE;
    model.is_sheetfed = SANE_FALSE;
    model.ccd_type = CIS_CANONLIDE220;
    model.dac_type = DAC_CANONLIDE110;
    model.gpo_type = GPO_CANONLIDE210;
    model.motor_type = MOTOR_CANONLIDE210;
    model.flags = GENESYS_FLAG_SKIP_WARMUP |
                  GENESYS_FLAG_OFFSET_CALIBRATION |
                  GENESYS_FLAG_DARK_CALIBRATION |
                  GENESYS_FLAG_SHADING_REPARK |
                  GENESYS_FLAG_CUSTOM_GAMMA;
    model.buttons = GENESYS_HAS_SCAN_SW |
                    GENESYS_HAS_COPY_SW |
                    GENESYS_HAS_EMAIL_SW |
                    GENESYS_HAS_FILE_SW |
                    GENESYS_HAS_EXTRA_SW;
    model.shading_lines = 60;
    model.shading_ta_lines = 0;
    model.search_lines = 400;

    s_usb_devices->emplace_back(0x04a9, 0x190f, model);


    model = Genesys_Model();
    model.name = "canon-5600f";
    model.vendor = "Canon";
    model.model = "5600F";
    model.model_id = MODEL_CANON_CANOSCAN_5600F;
    model.asic_type = AsicType::GL847;

    model.xdpi_values = { 1200, 600, 400, 300, 200, 150, 100, 75 };
    model.ydpi_values = { 1200, 600, 400, 300, 200, 150, 100, 75 };
    model.bpp_gray_values = { 16, 8 };
    model.bpp_color_values = { 16, 8 };

    model.x_offset = SANE_FIX(1.1);
    model.y_offset = SANE_FIX(8.3);
    model.x_size = SANE_FIX(216.07);
    model.y_size = SANE_FIX(299.0);

    model.y_offset_calib = SANE_FIX(3.0);
    model.x_offset_mark = SANE_FIX(0.0);

    model.x_offset_ta = SANE_FIX(0.0);
    model.y_offset_ta = SANE_FIX(0.0);
    model.x_size_ta = SANE_FIX(100.0);
    model.y_size_ta = SANE_FIX(100.0);

    model.y_offset_sensor_to_ta = SANE_FIX(0.0);
    model.y_offset_calib_ta = SANE_FIX(0.0);

    model.post_scan = SANE_FIX(0.0);
    model.eject_feed = SANE_FIX(0.0);

    model.ld_shift_r = 0;
    model.ld_shift_g = 0;
    model.ld_shift_b = 0;

    model.line_mode_color_order = COLOR_ORDER_RGB;

    model.is_cis = SANE_TRUE;
    model.is_sheetfed = SANE_FALSE;
    model.ccd_type = CIS_CANONLIDE200;
    model.dac_type = DAC_CANONLIDE200;
    model.gpo_type = GPO_CANONLIDE200;
    model.motor_type = MOTOR_CANONLIDE200;
    model.flags = GENESYS_FLAG_UNTESTED |
                  GENESYS_FLAG_SKIP_WARMUP |
                  GENESYS_FLAG_SIS_SENSOR |
                  GENESYS_FLAG_DARK_CALIBRATION |
                  GENESYS_FLAG_OFFSET_CALIBRATION |
                  GENESYS_FLAG_CUSTOM_GAMMA;
    model.buttons = GENESYS_HAS_SCAN_SW |
                    GENESYS_HAS_COPY_SW |
                    GENESYS_HAS_EMAIL_SW |
                    GENESYS_HAS_FILE_SW;
    model.shading_lines = 50;
    model.shading_ta_lines = 0;
    model.search_lines = 400;

    s_usb_devices->emplace_back(0x04a9, 0x1906, model);


    model = Genesys_Model();
    model.name = "canon-lide-700f";
    model.vendor = "Canon";
    model.model = "LiDE 700F";
    model.model_id = MODEL_CANON_LIDE_700F;
    model.asic_type = AsicType::GL847;

    model.xdpi_values = { 4800, 2400, 1200, 600, 300, 200, 150, 100, 75 };
    model.ydpi_values = { 4800, 2400, 1200, 600, 300, 200, 150, 100, 75 };
    model.bpp_gray_values = { 16, 8 };
    model.bpp_color_values = { 16, 8 };

    model.x_offset = SANE_FIX(3.1);
    model.y_offset = SANE_FIX(8.1);
    model.x_size = SANE_FIX(216.07);
    model.y_size = SANE_FIX(297.0);

    model.y_offset_calib = SANE_FIX(1.0);
    model.x_offset_mark = SANE_FIX(0.0);

    model.x_offset_ta = SANE_FIX(0.0);
    model.y_offset_ta = SANE_FIX(0.0);
    model.x_size_ta = SANE_FIX(100.0);
    model.y_size_ta = SANE_FIX(100.0);

    model.y_offset_sensor_to_ta = SANE_FIX(0.0);
    model.y_offset_calib_ta = SANE_FIX(0.0);
    model.post_scan = SANE_FIX(0.0);
    model.eject_feed = SANE_FIX(0.0);

    model.ld_shift_r = 0;
    model.ld_shift_g = 0;
    model.ld_shift_b = 0;

    model.line_mode_color_order = COLOR_ORDER_RGB;

    model.is_cis = SANE_TRUE;
    model.is_sheetfed = SANE_FALSE;
    model.ccd_type = CIS_CANONLIDE700;
    model.dac_type = DAC_CANONLIDE700;
    model.gpo_type = GPO_CANONLIDE700;
    model.motor_type = MOTOR_CANONLIDE700;
    model.flags = GENESYS_FLAG_SKIP_WARMUP |
                  GENESYS_FLAG_SIS_SENSOR |
                  GENESYS_FLAG_OFFSET_CALIBRATION |
                  GENESYS_FLAG_DARK_CALIBRATION |
                  GENESYS_FLAG_SHADING_REPARK |
                  GENESYS_FLAG_CUSTOM_GAMMA;
    model.buttons = GENESYS_HAS_SCAN_SW |
                    GENESYS_HAS_COPY_SW |
                    GENESYS_HAS_EMAIL_SW |
                    GENESYS_HAS_FILE_SW;
    model.shading_lines = 70;
    model.shading_ta_lines = 0;
    model.search_lines = 400;

    s_usb_devices->emplace_back(0x04a9, 0x1907, model);


    model = Genesys_Model();
    model.name = "canon-lide-200";
    model.vendor = "Canon";
    model.model = "LiDE 200";
    model.model_id = MODEL_CANON_LIDE_200;
    model.asic_type = AsicType::GL847;

    model.xdpi_values = { 4800, 2400, 1200, 600, 300, 200, 150, 100, 75 };
    model.ydpi_values = { 4800, 2400, 1200, 600, 300, 200, 150, 100, 75 };
    model.bpp_gray_values = { 16, 8 };
    model.bpp_color_values = { 16, 8 };

    model.x_offset = SANE_FIX(1.1);
    model.y_offset = SANE_FIX(8.3);
    model.x_size = SANE_FIX(216.07);
    model.y_size = SANE_FIX(299.0);

    model.y_offset_calib = SANE_FIX(0.0);
    model.x_offset_mark = SANE_FIX(0.0);

    model.x_offset_ta = SANE_FIX(0.0);
    model.y_offset_ta = SANE_FIX(0.0);
    model.x_size_ta = SANE_FIX(100.0);
    model.y_size_ta = SANE_FIX(100.0);

    model.y_offset_sensor_to_ta = SANE_FIX(0.0);
    model.y_offset_calib_ta = SANE_FIX(0.0);

    model.post_scan = SANE_FIX(0.0);
    model.eject_feed = SANE_FIX(0.0);

    model.ld_shift_r = 0;
    model.ld_shift_g = 0;
    model.ld_shift_b = 0;

    model.line_mode_color_order = COLOR_ORDER_RGB;
    model.is_cis = SANE_TRUE;
    model.is_sheetfed = SANE_FALSE;
    model.ccd_type = CIS_CANONLIDE200;
    model.dac_type = DAC_CANONLIDE200;
    model.gpo_type = GPO_CANONLIDE200;
    model.motor_type = MOTOR_CANONLIDE200;
    model.flags = GENESYS_FLAG_SKIP_WARMUP |
                  GENESYS_FLAG_SIS_SENSOR |
                  GENESYS_FLAG_OFFSET_CALIBRATION |
                  GENESYS_FLAG_DARK_CALIBRATION |
                  GENESYS_FLAG_SHADING_REPARK |
                  GENESYS_FLAG_CUSTOM_GAMMA;
    model.buttons = GENESYS_HAS_SCAN_SW |
                    GENESYS_HAS_COPY_SW |
                    GENESYS_HAS_EMAIL_SW |
                    GENESYS_HAS_FILE_SW;
    model.shading_lines = 50;
    model.shading_ta_lines = 0;
    model.search_lines = 400;

    s_usb_devices->emplace_back(0x04a9, 0x1905, model);


    model = Genesys_Model();
    model.name = "canon-lide-60";
    model.vendor = "Canon";
    model.model = "LiDE 60";
    model.model_id = MODEL_CANON_LIDE_60;
    model.asic_type = AsicType::GL841;

    model.xdpi_values = { 1200, 600, 300, 150, 75 };
    model.ydpi_values = { 2400, 1200, 600, 300, 150, 75 };
    model.bpp_gray_values = { 16, 8 };
    model.bpp_color_values = { 16, 8 };

    model.x_offset = SANE_FIX(0.42);
    model.y_offset = SANE_FIX(7.9);
    model.x_size = SANE_FIX(218.0);
    model.y_size = SANE_FIX(299.0);

    model.y_offset_calib = SANE_FIX(6.0);
    model.x_offset_mark = SANE_FIX(0.0);

    model.x_offset_ta = SANE_FIX(0.0);
    model.y_offset_ta = SANE_FIX(0.0);
    model.x_size_ta = SANE_FIX(100.0);
    model.y_size_ta = SANE_FIX(100.0);

    model.y_offset_sensor_to_ta = SANE_FIX(0.0);
    model.y_offset_calib_ta = SANE_FIX(0.0);

    model.post_scan = SANE_FIX(0.0);
    model.eject_feed = SANE_FIX(0.0);

    model.ld_shift_r = 0;
    model.ld_shift_g = 0;
    model.ld_shift_b = 0;
    model.line_mode_color_order = COLOR_ORDER_RGB;

    model.is_cis = SANE_TRUE;
    model.is_sheetfed = SANE_FALSE;
    model.ccd_type = CCD_CANONLIDE35;
    model.dac_type = DAC_CANONLIDE35;
    model.gpo_type = GPO_CANONLIDE35;
    model.motor_type = MOTOR_CANONLIDE35;
    model.flags = GENESYS_FLAG_LAZY_INIT |
                  GENESYS_FLAG_SKIP_WARMUP |
                  GENESYS_FLAG_OFFSET_CALIBRATION |
                  GENESYS_FLAG_DARK_WHITE_CALIBRATION |
                  GENESYS_FLAG_CUSTOM_GAMMA;

    model.buttons = GENESYS_HAS_COPY_SW |
                    GENESYS_HAS_SCAN_SW |
                    GENESYS_HAS_FILE_SW |
                    GENESYS_HAS_EMAIL_SW;
    model.shading_lines = 300;
    model.shading_ta_lines = 0;
    model.search_lines = 400;
    // this is completely untested
    s_usb_devices->emplace_back(0x04a9, 0x221c, model);


    model = Genesys_Model();
    model.name = "canon-lide-80";
    model.vendor = "Canon";
    model.model = "LiDE 80";
    model.model_id = MODEL_CANON_LIDE_80;
    model.asic_type = AsicType::GL841;

    model.xdpi_values = {      1200, 600, 400, 300, 240, 150, 100, 75 };
    model.ydpi_values = { 2400, 1200, 600, 400, 300, 240, 150, 100, 75 };
    model.bpp_gray_values = { 16, 8 };
    model.bpp_color_values = { 16, 8 };
    model.x_offset = SANE_FIX(0.42);
    model.y_offset = SANE_FIX(7.90);
    model.x_size = SANE_FIX(216.07);
    model.y_size = SANE_FIX(299.0);

    model.y_offset_calib = SANE_FIX(4.5);
    model.x_offset_mark = SANE_FIX(0.0);

    model.x_offset_ta = SANE_FIX(0.0);
    model.y_offset_ta = SANE_FIX(0.0);
    model.x_size_ta = SANE_FIX(100.0);
    model.y_size_ta = SANE_FIX(100.0);

    model.y_offset_sensor_to_ta = SANE_FIX(0.0);
    model.y_offset_calib_ta = SANE_FIX(0.0);

    model.post_scan = SANE_FIX(0.0);
    model.eject_feed = SANE_FIX(0.0);

    model.ld_shift_r = 0;
    model.ld_shift_g = 0;
    model.ld_shift_b = 0;

    model.line_mode_color_order = COLOR_ORDER_RGB;

    model.is_cis = SANE_TRUE;
    model.is_sheetfed = SANE_FALSE;
    model.ccd_type = CIS_CANONLIDE80;
    model.dac_type = DAC_CANONLIDE80;
    model.gpo_type = GPO_CANONLIDE80;
    model.motor_type = MOTOR_CANONLIDE80;
    model.flags = GENESYS_FLAG_LAZY_INIT |
                  GENESYS_FLAG_SKIP_WARMUP |
                  GENESYS_FLAG_OFFSET_CALIBRATION |
                  GENESYS_FLAG_DARK_WHITE_CALIBRATION |
                  GENESYS_FLAG_CUSTOM_GAMMA;
    model.buttons = GENESYS_HAS_SCAN_SW |
                    GENESYS_HAS_FILE_SW |
                    GENESYS_HAS_EMAIL_SW |
                    GENESYS_HAS_COPY_SW;
    model.shading_lines = 160;
    model.shading_ta_lines = 0;
    model.search_lines = 400;

    s_usb_devices->emplace_back(0x04a9, 0x2214, model);


    model = Genesys_Model();
    model.name = "hewlett-packard-scanjet-2300c";
    model.vendor = "Hewlett Packard";
    model.model = "ScanJet 2300c";
    model.model_id = MODEL_HP_SCANJET_2300C;
    model.asic_type = AsicType::GL646;

    model.xdpi_values = { 600, 300, 150, 75 };
    model.ydpi_values = { 1200, 600, 300, 150, 75 };
    model.bpp_gray_values = { 16, 8 };
    model.bpp_color_values = { 16, 8 };

    model.x_offset = SANE_FIX(2.0);
    model.y_offset = SANE_FIX(7.5);
    model.x_size = SANE_FIX(215.9);
    model.y_size = SANE_FIX(295.0);

    model.y_offset_calib = SANE_FIX(0.0);
    model.x_offset_mark = SANE_FIX(1.0);

    model.x_offset_ta = SANE_FIX(0.0);
    model.y_offset_ta = SANE_FIX(0.0);
    model.x_size_ta = SANE_FIX(100.0);
    model.y_size_ta = SANE_FIX(100.0);

    model.y_offset_sensor_to_ta = SANE_FIX(0.0);
    model.y_offset_calib_ta = SANE_FIX(0.0);

    model.post_scan = SANE_FIX(0.0);
    model.eject_feed = SANE_FIX(0.0);

    model.ld_shift_r = 16;
    model.ld_shift_g = 8;
    model.ld_shift_b = 0;

    model.line_mode_color_order = COLOR_ORDER_RGB;
    model.is_cis = SANE_FALSE;
    model.is_sheetfed = SANE_FALSE;
    model.ccd_type = CCD_HP2300;
    model.dac_type = DAC_WOLFSON_HP2300;
    model.gpo_type = GPO_HP2300;
    model.motor_type = MOTOR_HP2300;
    model.flags = GENESYS_FLAG_14BIT_GAMMA |
                  GENESYS_FLAG_SKIP_WARMUP |
                  GENESYS_FLAG_LAZY_INIT |
                  GENESYS_FLAG_SEARCH_START |
                  GENESYS_FLAG_DARK_CALIBRATION |
                  GENESYS_FLAG_OFFSET_CALIBRATION |
                  GENESYS_FLAG_CUSTOM_GAMMA;
    model.buttons = GENESYS_HAS_SCAN_SW | GENESYS_HAS_COPY_SW;
    model.shading_lines = 40;
    model.shading_ta_lines = 0;
    model.search_lines = 132;

    s_usb_devices->emplace_back(0x03f0, 0x0901, model);


    model = Genesys_Model();
    model.name = "hewlett-packard-scanjet-2400c";
    model.vendor = "Hewlett Packard";
    model.model = "ScanJet 2400c";
    model.model_id = MODEL_HP_SCANJET_2400C;
    model.asic_type = AsicType::GL646;

    model.xdpi_values = { 1200, 600, 300, 150, 100, 50 };
    model.ydpi_values = { 1200, 600, 300, 150, 100, 50 };
    model.bpp_gray_values = { 16, 8 };
    model.bpp_color_values = { 16, 8 };

    model.x_offset = SANE_FIX(6.5);
    model.y_offset = SANE_FIX(2.5);
    model.x_size = SANE_FIX(220.0);
    model.y_size = SANE_FIX(297.2);

    model.y_offset_calib = SANE_FIX(0.0);
    model.x_offset_mark = SANE_FIX(1.0);

    model.x_offset_ta = SANE_FIX(0.0);
    model.y_offset_ta = SANE_FIX(0.0);
    model.x_size_ta = SANE_FIX(100.0);
    model.y_size_ta = SANE_FIX(100.0);

    model.y_offset_sensor_to_ta = SANE_FIX(0.0);
    model.y_offset_calib_ta = SANE_FIX(0.0);

    model.post_scan = SANE_FIX(0.0);
    model.eject_feed = SANE_FIX(0.0);

    model.ld_shift_r = 0;
    model.ld_shift_g = 24;
    model.ld_shift_b = 48;

    model.line_mode_color_order = COLOR_ORDER_RGB;

    model.is_cis = SANE_FALSE;
    model.is_sheetfed = SANE_FALSE;
    model.ccd_type = CCD_HP2400;
    model.dac_type = DAC_WOLFSON_HP2400;
    model.gpo_type = GPO_HP2400;
    model.motor_type = MOTOR_HP2400;
    model.flags = GENESYS_FLAG_LAZY_INIT |
                  GENESYS_FLAG_14BIT_GAMMA |
                  GENESYS_FLAG_DARK_CALIBRATION |
                  GENESYS_FLAG_OFFSET_CALIBRATION |
                  GENESYS_FLAG_SKIP_WARMUP |
                  GENESYS_FLAG_STAGGERED_LINE |
                  GENESYS_FLAG_CUSTOM_GAMMA;
    model.buttons = GENESYS_HAS_COPY_SW | GENESYS_HAS_EMAIL_SW | GENESYS_HAS_SCAN_SW;
    model.shading_lines = 20;
    model.shading_ta_lines = 0;
    model.search_lines = 132;

    s_usb_devices->emplace_back(0x03f0, 0x0a01, model);


    model = Genesys_Model();
    model.name = "visioneer-strobe-xp200";
    model.vendor = "Visioneer";
    model.model = "Strobe XP200";
    model.model_id = MODEL_VISIONEER_STROBE_XP200;
    model.asic_type = AsicType::GL646;

    model.xdpi_values = { 600, 300, 200, 100, 75 };
    model.ydpi_values = { 600, 300, 200, 100, 75 };
    model.bpp_gray_values = { 16, 8 };
    model.bpp_color_values = { 16, 8 };

    model.x_offset = SANE_FIX(0.5);
    model.y_offset = SANE_FIX(16.0);
    model.x_size = SANE_FIX(215.9);
    model.y_size = SANE_FIX(297.2);

    model.y_offset_calib = SANE_FIX(0.0);
    model.x_offset_mark = SANE_FIX(0.0);

    model.x_offset_ta = SANE_FIX(0.0);
    model.y_offset_ta = SANE_FIX(0.0);
    model.x_size_ta = SANE_FIX(100.0);
    model.y_size_ta = SANE_FIX(100.0);

    model.y_offset_sensor_to_ta = SANE_FIX(0.0);
    model.y_offset_calib_ta = SANE_FIX(0.0);

    model.post_scan = SANE_FIX(0.0);
    model.eject_feed = SANE_FIX(0.0);

    model.ld_shift_r = 0;
    model.ld_shift_g = 0;
    model.ld_shift_b = 0;

    model.line_mode_color_order = COLOR_ORDER_RGB;

    model.is_cis = SANE_TRUE;
    model.is_sheetfed = SANE_TRUE;
    model.ccd_type = CIS_XP200;
    model.dac_type = DAC_AD_XP200;
    model.gpo_type = GPO_XP200;
    model.motor_type = MOTOR_XP200;
    model.flags = GENESYS_FLAG_14BIT_GAMMA |
                  GENESYS_FLAG_LAZY_INIT |
                  GENESYS_FLAG_CUSTOM_GAMMA |
                  GENESYS_FLAG_SKIP_WARMUP |
                  GENESYS_FLAG_DARK_CALIBRATION |
                  GENESYS_FLAG_OFFSET_CALIBRATION;
    model.buttons = GENESYS_HAS_SCAN_SW | GENESYS_HAS_PAGE_LOADED_SW | GENESYS_HAS_CALIBRATE;
    model.shading_lines = 120;
    model.shading_ta_lines = 0;
    model.search_lines = 132;

    s_usb_devices->emplace_back(0x04a7, 0x0426, model);


    model = Genesys_Model();
    model.name = "hewlett-packard-scanjet-3670c";
    model.vendor = "Hewlett Packard";
    model.model = "ScanJet 3670c";
    model.model_id = MODEL_HP_SCANJET_3670C;
    model.asic_type = AsicType::GL646;

    model.xdpi_values = { 1200, 600, 300, 150, 100, 75 };
    model.ydpi_values = { 1200, 600, 300, 150, 100, 75 };
    model.bpp_gray_values = { 16, 8 };
    model.bpp_color_values = { 16, 8 };

    model.x_offset = SANE_FIX(8.5);
    model.y_offset = SANE_FIX(11.0);
    model.x_size = SANE_FIX(215.9);
    model.y_size = SANE_FIX(300.0);

    model.y_offset_calib = SANE_FIX(0.0);
    model.x_offset_mark = SANE_FIX(1.0);

    model.x_offset_ta = SANE_FIX(104.0);
    model.y_offset_ta = SANE_FIX(55.6);
    model.x_size_ta = SANE_FIX(25.6);
    model.y_size_ta = SANE_FIX(78.0);

    model.y_offset_sensor_to_ta = SANE_FIX(0.0);
    model.y_offset_calib_ta = SANE_FIX(76.0);

    model.post_scan = SANE_FIX(0.0);
    model.eject_feed = SANE_FIX(0.0);

    model.ld_shift_r = 0;
    model.ld_shift_g = 24;
    model.ld_shift_b = 48;

    model.line_mode_color_order = COLOR_ORDER_RGB;

    model.is_cis = SANE_FALSE;
    model.is_sheetfed = SANE_FALSE;
    model.ccd_type = CCD_HP3670;
    model.dac_type = DAC_WOLFSON_HP3670;
    model.gpo_type = GPO_HP3670;
    model.motor_type = MOTOR_HP3670;
    model.flags = GENESYS_FLAG_LAZY_INIT |
                  GENESYS_FLAG_14BIT_GAMMA |
                  GENESYS_FLAG_XPA |
                  GENESYS_FLAG_DARK_CALIBRATION |
                  GENESYS_FLAG_OFFSET_CALIBRATION |
                  GENESYS_FLAG_STAGGERED_LINE |
                  GENESYS_FLAG_CUSTOM_GAMMA;
    model.buttons = GENESYS_HAS_COPY_SW | GENESYS_HAS_EMAIL_SW | GENESYS_HAS_SCAN_SW;
    model.shading_lines = 20;
    model.shading_ta_lines = 0;
    model.search_lines = 200;

    s_usb_devices->emplace_back(0x03f0, 0x1405, model);


    model = Genesys_Model();
    model.name = "plustek-opticpro-st12";
    model.vendor = "Plustek";
    model.model = "OpticPro ST12";
    model.model_id = MODEL_PLUSTEK_OPTICPRO_ST12;
    model.asic_type = AsicType::GL646;

    model.xdpi_values = { 600, 300, 150, 75 };
    model.ydpi_values = { 1200, 600, 300, 150, 75 };
    model.bpp_gray_values = { 16, 8 };
    model.bpp_color_values = { 16, 8 };

    model.x_offset = SANE_FIX(3.5);
    model.y_offset = SANE_FIX(7.5);
    model.x_size = SANE_FIX(218.0);
    model.y_size = SANE_FIX(299.0);

    model.y_offset_calib = SANE_FIX(0.0);
    model.x_offset_mark = SANE_FIX(1.0);

    model.x_offset_ta = SANE_FIX(0.0);
    model.y_offset_ta = SANE_FIX(0.0);
    model.x_size_ta = SANE_FIX(100.0);
    model.y_size_ta = SANE_FIX(100.0);

    model.y_offset_sensor_to_ta = SANE_FIX(0.0);
    model.y_offset_calib_ta = SANE_FIX(0.0);

    model.post_scan = SANE_FIX(0.0);
    model.eject_feed = SANE_FIX(0.0);

    model.ld_shift_r = 0;
    model.ld_shift_g = 8;
    model.ld_shift_b = 16;

    model.line_mode_color_order = COLOR_ORDER_BGR;

    model.is_cis = SANE_FALSE;
    model.is_sheetfed = SANE_FALSE;
    model.ccd_type = CCD_ST12;
    model.dac_type = DAC_WOLFSON_ST12;
    model.gpo_type = GPO_ST12;
    model.motor_type = MOTOR_UMAX;
    model.flags = GENESYS_FLAG_UNTESTED | GENESYS_FLAG_14BIT_GAMMA;
    model.buttons = GENESYS_HAS_NO_BUTTONS,
    model.shading_lines = 20;
    model.shading_ta_lines = 0;
    model.search_lines = 200;

    s_usb_devices->emplace_back(0x07b3, 0x0600, model);

    model = Genesys_Model();
    model.name = "plustek-opticpro-st24";
    model.vendor = "Plustek";
    model.model = "OpticPro ST24";
    model.model_id = MODEL_PLUSTEK_OPTICPRO_ST24;
    model.asic_type = AsicType::GL646;

    model.xdpi_values = { 1200, 600, 300, 150, 75 };
    model.ydpi_values = { 2400, 1200, 600, 300, 150, 75 };
    model.bpp_gray_values = { 16, 8 };
    model.bpp_color_values = { 16, 8 };

    model.x_offset = SANE_FIX(3.5);
    model.y_offset = SANE_FIX(7.5);
    model.x_size = SANE_FIX(218.0);
    model.y_size = SANE_FIX(299.0);

    model.y_offset_calib = SANE_FIX(0.0);
    model.x_offset_mark = SANE_FIX(1.0);

    model.x_offset_ta = SANE_FIX(0.0);
    model.y_offset_ta = SANE_FIX(0.0);
    model.x_size_ta = SANE_FIX(100.0);
    model.y_size_ta = SANE_FIX(100.0);

    model.y_offset_sensor_to_ta = SANE_FIX(0.0);
    model.y_offset_calib_ta = SANE_FIX(0.0);

    model.post_scan = SANE_FIX(0.0);
    model.eject_feed = SANE_FIX(0.0);

    model.ld_shift_r = 0;
    model.ld_shift_g = 8;
    model.ld_shift_b = 16;

    model.line_mode_color_order = COLOR_ORDER_BGR;

    model.is_cis = SANE_FALSE;
    model.is_sheetfed = SANE_FALSE;
    model.ccd_type = CCD_ST24;
    model.dac_type = DAC_WOLFSON_ST24;
    model.gpo_type = GPO_ST24;
    model.motor_type = MOTOR_ST24;
    model.flags = GENESYS_FLAG_UNTESTED |
                  GENESYS_FLAG_14BIT_GAMMA |
                  GENESYS_FLAG_LAZY_INIT |
                  GENESYS_FLAG_CUSTOM_GAMMA |
                  GENESYS_FLAG_SEARCH_START |
                  GENESYS_FLAG_OFFSET_CALIBRATION;
    model.buttons = GENESYS_HAS_NO_BUTTONS,
    model.shading_lines = 20;
    model.shading_ta_lines = 0;
    model.search_lines = 200;

    s_usb_devices->emplace_back(0x07b3, 0x0601, model);

    model = Genesys_Model();
    model.name = "medion-md5345-model";
    model.vendor = "Medion";
    model.model = "MD5345/MD6228/MD6471";
    model.model_id = MODEL_MEDION_MD5345;
    model.asic_type = AsicType::GL646;

    model.xdpi_values = { 1200, 600, 400, 300, 200, 150, 100, 75, 50 };
    model.ydpi_values = { 2400, 1200, 600, 400, 300, 200, 150, 100, 75, 50 };
    model.bpp_gray_values = { 16, 8 };
    model.bpp_color_values = { 16, 8 };

    model.x_offset = SANE_FIX(0.30);
    model.y_offset = SANE_FIX(0.80);
    model.x_size = SANE_FIX(220.0);
    model.y_size = SANE_FIX(296.4);

    model.y_offset_calib = SANE_FIX(0.00);
    model.x_offset_mark = SANE_FIX(0.00);

    model.x_offset_ta = SANE_FIX(0.00);
    model.y_offset_ta = SANE_FIX(0.00);
    model.x_size_ta = SANE_FIX(0.00);
    model.y_size_ta = SANE_FIX(0.00);

    model.y_offset_sensor_to_ta = SANE_FIX(0.0);
    model.y_offset_calib_ta = SANE_FIX(0.00);

    model.post_scan = SANE_FIX(0.0);
    model.eject_feed = SANE_FIX(0.0);

    model.ld_shift_r = 48;
    model.ld_shift_g = 24;
    model.ld_shift_b = 0;
    model.line_mode_color_order = COLOR_ORDER_RGB;

    model.is_cis = SANE_FALSE;
    model.is_sheetfed = SANE_FALSE;
    model.ccd_type = CCD_5345;
    model.dac_type = DAC_WOLFSON_5345;
    model.gpo_type = GPO_5345;
    model.motor_type = MOTOR_5345;
    model.flags = GENESYS_FLAG_14BIT_GAMMA |
                  GENESYS_FLAG_LAZY_INIT |
                  GENESYS_FLAG_SEARCH_START |
                  GENESYS_FLAG_STAGGERED_LINE |
                  GENESYS_FLAG_DARK_CALIBRATION |
                  GENESYS_FLAG_OFFSET_CALIBRATION |
                  GENESYS_FLAG_SHADING_NO_MOVE |
                  GENESYS_FLAG_CUSTOM_GAMMA;
    model.buttons = GENESYS_HAS_COPY_SW |
                    GENESYS_HAS_EMAIL_SW |
                    GENESYS_HAS_POWER_SW |
                    GENESYS_HAS_OCR_SW |
                    GENESYS_HAS_SCAN_SW;
    model.shading_lines = 40;
    model.shading_ta_lines = 0;
    model.search_lines = 200;

    s_usb_devices->emplace_back(0x0461, 0x0377, model);

    model = Genesys_Model();
    model.name = "visioneer-strobe-xp300";
    model.vendor = "Visioneer";
    model.model = "Strobe XP300";
    model.model_id = MODEL_VISIONEER_STROBE_XP300;
    model.asic_type = AsicType::GL841;

    model.xdpi_values = { 600, 300, 150, 75 };
    model.ydpi_values = { 600, 300, 150, 75 };
    model.bpp_gray_values = { 16, 8 };
    model.bpp_color_values = { 16, 8 };

    model.x_offset = SANE_FIX(0.0);
    model.y_offset = SANE_FIX(1.0);
    model.x_size = SANE_FIX(435.0);
    model.y_size = SANE_FIX(511);

    model.y_offset_calib = SANE_FIX(0.0);
    model.x_offset_mark = SANE_FIX(0.0);

    model.x_offset_ta = SANE_FIX(0.0);
    model.y_offset_ta = SANE_FIX(0.0);
    model.x_size_ta = SANE_FIX(100.0);
    model.y_size_ta = SANE_FIX(100.0);

    model.y_offset_sensor_to_ta = SANE_FIX(0.0);
    model.y_offset_calib_ta = SANE_FIX(0.0);

    model.post_scan = SANE_FIX(26.5);
    // this is larger than needed -- accounts for second sensor head, which is a calibration item
    model.eject_feed = SANE_FIX(0.0);
    model.ld_shift_r = 0;
    model.ld_shift_g = 0;
    model.ld_shift_b = 0;

    model.line_mode_color_order = COLOR_ORDER_RGB;

    model.is_cis = SANE_TRUE;
    model.is_sheetfed = SANE_TRUE;
    model.ccd_type = CCD_XP300;
    model.dac_type = DAC_WOLFSON_XP300;
    model.gpo_type = GPO_XP300;
    model.motor_type = MOTOR_XP300;
    model.flags = GENESYS_FLAG_LAZY_INIT |
                  GENESYS_FLAG_SKIP_WARMUP |
                  GENESYS_FLAG_OFFSET_CALIBRATION |
                  GENESYS_FLAG_DARK_CALIBRATION |
                  GENESYS_FLAG_CUSTOM_GAMMA;
    model.buttons = GENESYS_HAS_SCAN_SW | GENESYS_HAS_PAGE_LOADED_SW | GENESYS_HAS_CALIBRATE;
    model.shading_lines = 100;
    model.shading_ta_lines = 0;
    model.search_lines = 400;

    s_usb_devices->emplace_back(0x04a7, 0x0474, model);

    model = Genesys_Model();
    model.name = "syscan-docketport-665";
    model.vendor = "Syscan/Ambir";
    model.model = "DocketPORT 665";
    model.model_id = MODEL_SYSCAN_DOCKETPORT_665;
    model.asic_type = AsicType::GL841;

    model.xdpi_values = { 600, 300, 150, 75 };
    model.ydpi_values = { 1200, 600, 300, 150, 75 };
    model.bpp_gray_values = { 16, 8 };
    model.bpp_color_values = { 16, 8 };

    model.x_offset = SANE_FIX(0.0);
    model.y_offset = SANE_FIX(0.0);
    model.x_size = SANE_FIX(108.0);
    model.y_size = SANE_FIX(511);

    model.y_offset_calib = SANE_FIX(0.0);
    model.x_offset_mark = SANE_FIX(0.0);

    model.x_offset_ta = SANE_FIX(0.0);
    model.y_offset_ta = SANE_FIX(0.0);
    model.x_size_ta = SANE_FIX(100.0);
    model.y_size_ta = SANE_FIX(100.0);

    model.y_offset_sensor_to_ta = SANE_FIX(0.0);
    model.y_offset_calib_ta = SANE_FIX(0.0);

    model.post_scan = SANE_FIX(17.5);
    model.eject_feed = SANE_FIX(0.0);

    model.ld_shift_r = 0;
    model.ld_shift_g = 0;
    model.ld_shift_b = 0;

    model.line_mode_color_order = COLOR_ORDER_RGB;

    model.is_cis = SANE_TRUE;
    model.is_sheetfed = SANE_TRUE;
    model.ccd_type = CCD_DP665;
    model.dac_type = DAC_WOLFSON_XP300;
    model.gpo_type = GPO_DP665;
    model.motor_type = MOTOR_DP665;
    model.flags = GENESYS_FLAG_LAZY_INIT |
                  GENESYS_FLAG_SKIP_WARMUP |
                  GENESYS_FLAG_OFFSET_CALIBRATION |
                  GENESYS_FLAG_DARK_CALIBRATION |
                  GENESYS_FLAG_CUSTOM_GAMMA;
    model.buttons = GENESYS_HAS_SCAN_SW | GENESYS_HAS_PAGE_LOADED_SW | GENESYS_HAS_CALIBRATE;
    model.shading_lines = 100;
    model.shading_ta_lines = 0;
    model.search_lines = 400;

    s_usb_devices->emplace_back(0x0a82, 0x4803, model);

    model = Genesys_Model();
    model.name = "visioneer-roadwarrior";
    model.vendor = "Visioneer";
    model.model = "Readwarrior";
    model.model_id = MODEL_VISIONEER_ROADWARRIOR;
    model.asic_type = AsicType::GL841;

    model.xdpi_values = { 600, 300, 150, 75 };
    model.ydpi_values = { 1200, 600, 300, 150, 75 };
    model.bpp_gray_values = { 16, 8 };
    model.bpp_color_values = { 16, 8 };

    model.x_offset = SANE_FIX(0.0);
    model.y_offset = SANE_FIX(0.0);
    model.x_size = SANE_FIX(220.0);
    model.y_size = SANE_FIX(511);

    model.y_offset_calib = SANE_FIX(0.0);
    model.x_offset_mark = SANE_FIX(0.0);

    model.x_offset_ta = SANE_FIX(0.0);
    model.y_offset_ta = SANE_FIX(0.0);
    model.x_size_ta = SANE_FIX(100.0);
    model.y_size_ta = SANE_FIX(100.0);

    model.y_offset_sensor_to_ta = SANE_FIX(0.0);
    model.y_offset_calib_ta = SANE_FIX(0.0);

    model.post_scan = SANE_FIX(16.0);
    model.eject_feed = SANE_FIX(0.0);

    model.ld_shift_r = 0;
    model.ld_shift_g = 0;
    model.ld_shift_b = 0;

    model.line_mode_color_order = COLOR_ORDER_RGB;

    model.is_cis = SANE_TRUE;
    model.is_sheetfed = SANE_TRUE;
    model.ccd_type = CCD_ROADWARRIOR;
    model.dac_type = DAC_WOLFSON_XP300;
    model.gpo_type = GPO_DP665;
    model.motor_type = MOTOR_ROADWARRIOR;
    model.flags = GENESYS_FLAG_LAZY_INIT |
                  GENESYS_FLAG_SKIP_WARMUP |
                  GENESYS_FLAG_OFFSET_CALIBRATION |
                  GENESYS_FLAG_CUSTOM_GAMMA |
                  GENESYS_FLAG_DARK_CALIBRATION;
    model.buttons = GENESYS_HAS_SCAN_SW | GENESYS_HAS_PAGE_LOADED_SW | GENESYS_HAS_CALIBRATE;
    model.shading_lines = 100;
    model.shading_ta_lines = 0;
    model.search_lines = 400;

    s_usb_devices->emplace_back(0x04a7, 0x0494, model);

    model = Genesys_Model();
    model.name = "syscan-docketport-465";
    model.vendor = "Syscan";
    model.model = "DocketPORT 465";
    model.model_id = MODEL_SYSCAN_DOCKETPORT_465;
    model.asic_type = AsicType::GL841;

    model.xdpi_values = { 600, 300, 150, 75 };
    model.ydpi_values = { 1200, 600, 300, 150, 75 };
    model.bpp_gray_values = { 16, 8 };
    model.bpp_color_values = { 16, 8 };

    model.x_offset = SANE_FIX(0.0);
    model.y_offset = SANE_FIX(0.0);
    model.x_size = SANE_FIX(220.0);
    model.y_size = SANE_FIX(511);

    model.y_offset_calib = SANE_FIX(0.0);
    model.x_offset_mark = SANE_FIX(0.0);

    model.x_offset_ta = SANE_FIX(0.0);
    model.y_offset_ta = SANE_FIX(0.0);
    model.x_size_ta = SANE_FIX(100.0);
    model.y_size_ta = SANE_FIX(100.0);

    model.y_offset_sensor_to_ta = SANE_FIX(0.0);
    model.y_offset_calib_ta = SANE_FIX(0.0);

    model.post_scan = SANE_FIX(16.0);
    model.eject_feed = SANE_FIX(0.0);

    model.ld_shift_r = 0;
    model.ld_shift_g = 0;
    model.ld_shift_b = 0;

    model.line_mode_color_order = COLOR_ORDER_RGB;

    model.is_cis = SANE_TRUE;
    model.is_sheetfed = SANE_TRUE;
    model.ccd_type = CCD_ROADWARRIOR;
    model.dac_type = DAC_WOLFSON_XP300;
    model.gpo_type = GPO_DP665;
    model.motor_type = MOTOR_ROADWARRIOR;
    model.flags = GENESYS_FLAG_LAZY_INIT |
                  GENESYS_FLAG_SKIP_WARMUP |
                  GENESYS_FLAG_NO_CALIBRATION |
                  GENESYS_FLAG_CUSTOM_GAMMA |
                  GENESYS_FLAG_UNTESTED;
    model.buttons = GENESYS_HAS_SCAN_SW | GENESYS_HAS_PAGE_LOADED_SW;
    model.shading_lines = 300;
    model.shading_ta_lines = 0;
    model.search_lines = 400;

    s_usb_devices->emplace_back(0x0a82, 0x4802, model);


    model = Genesys_Model();
    model.name = "visioneer-xp100-revision3";
    model.vendor = "Visioneer";
    model.model = "XP100 Revision 3";
    model.model_id = MODEL_VISIONEER_STROBE_XP100_REVISION3;
    model.asic_type = AsicType::GL841;

    model.xdpi_values = { 600, 300, 150, 75 };
    model.ydpi_values = { 1200, 600, 300, 150, 75 };
    model.bpp_gray_values = { 16, 8 };
    model.bpp_color_values = { 16, 8 };

    model.x_offset = SANE_FIX(0.0);
    model.y_offset = SANE_FIX(0.0);
    model.x_size = SANE_FIX(220.0);
    model.y_size = SANE_FIX(511);

    model.y_offset_calib = SANE_FIX(0.0);
    model.x_offset_mark = SANE_FIX(0.0);

    model.x_offset_ta = SANE_FIX(0.0);
    model.y_offset_ta = SANE_FIX(0.0);
    model.x_size_ta = SANE_FIX(100.0);
    model.y_size_ta = SANE_FIX(100.0);

    model.y_offset_sensor_to_ta = SANE_FIX(0.0);
    model.y_offset_calib_ta = SANE_FIX(0.0);

    model.post_scan = SANE_FIX(16.0);
    model.eject_feed = SANE_FIX(0.0);

    model.ld_shift_r = 0;
    model.ld_shift_g = 0;
    model.ld_shift_b = 0;

    model.line_mode_color_order = COLOR_ORDER_RGB;

    model.is_cis = SANE_TRUE;
    model.is_sheetfed = SANE_TRUE;
    model.ccd_type = CCD_ROADWARRIOR;
    model.dac_type = DAC_WOLFSON_XP300;
    model.gpo_type = GPO_DP665;
    model.motor_type = MOTOR_ROADWARRIOR;
    model.flags = GENESYS_FLAG_LAZY_INIT |
                  GENESYS_FLAG_SKIP_WARMUP |
                  GENESYS_FLAG_OFFSET_CALIBRATION |
                  GENESYS_FLAG_CUSTOM_GAMMA |
                  GENESYS_FLAG_DARK_CALIBRATION;
    model.buttons = GENESYS_HAS_SCAN_SW | GENESYS_HAS_PAGE_LOADED_SW | GENESYS_HAS_CALIBRATE;
    model.shading_lines = 100;
    model.shading_ta_lines = 0;
    model.search_lines = 400;

    s_usb_devices->emplace_back(0x04a7, 0x049b, model);

    model = Genesys_Model();
    model.name = "pentax-dsmobile-600";
    model.vendor = "Pentax";
    model.model = "DSmobile 600";
    model.model_id = MODEL_PENTAX_DSMOBILE_600;
    model.asic_type = AsicType::GL841;

    model.xdpi_values = { 600, 300, 150, 75 };
    model.ydpi_values = { 1200, 600, 300, 150, 75 };
    model.bpp_gray_values = { 16, 8 };
    model.bpp_color_values = { 16, 8 };

    model.x_offset = SANE_FIX(0.0);
    model.y_offset = SANE_FIX(0.0);
    model.x_size = SANE_FIX(220.0);
    model.y_size = SANE_FIX(511);

    model.y_offset_calib = SANE_FIX(0.0);
    model.x_offset_mark = SANE_FIX(0.0);

    model.x_offset_ta = SANE_FIX(0.0);
    model.y_offset_ta = SANE_FIX(0.0);
    model.x_size_ta = SANE_FIX(100.0);
    model.y_size_ta = SANE_FIX(100.0);

    model.y_offset_sensor_to_ta = SANE_FIX(0.0);
    model.y_offset_calib_ta = SANE_FIX(0.0);

    model.post_scan = SANE_FIX(16.0);
    model.eject_feed = SANE_FIX(0.0);

    model.ld_shift_r = 0;
    model.ld_shift_g = 0;
    model.ld_shift_b = 0;

    model.line_mode_color_order = COLOR_ORDER_RGB;

    model.is_cis = SANE_TRUE;
    model.is_sheetfed = SANE_TRUE;
    model.ccd_type = CCD_DSMOBILE600;
    model.dac_type = DAC_WOLFSON_DSM600;
    model.gpo_type = GPO_DP665;
    model.motor_type = MOTOR_DSMOBILE_600;
    model.flags = GENESYS_FLAG_LAZY_INIT |
                  GENESYS_FLAG_SKIP_WARMUP |
                  GENESYS_FLAG_OFFSET_CALIBRATION |
                  GENESYS_FLAG_CUSTOM_GAMMA |
                  GENESYS_FLAG_DARK_CALIBRATION;
    model.buttons = GENESYS_HAS_SCAN_SW | GENESYS_HAS_PAGE_LOADED_SW | GENESYS_HAS_CALIBRATE;
    model.shading_lines = 100;
    model.shading_ta_lines = 0;
    model.search_lines = 400;

    s_usb_devices->emplace_back(0x0a17, 0x3210, model);
    // clone, only usb id is different
    s_usb_devices->emplace_back(0x04f9, 0x2038, model);

    model = Genesys_Model();
    model.name = "syscan-docketport-467";
    model.vendor = "Syscan";
    model.model = "DocketPORT 467";
    model.model_id = MODEL_SYSCAN_DOCKETPORT_467;
    model.asic_type = AsicType::GL841;

    model.xdpi_values = { 600, 300, 150, 75 };
    model.ydpi_values = { 1200, 600, 300, 150, 75 };
    model.bpp_gray_values = { 16, 8 };
    model.bpp_color_values = { 16, 8 };

    model.x_offset = SANE_FIX(0.0);
    model.y_offset = SANE_FIX(0.0);
    model.x_size = SANE_FIX(220.0);
    model.y_size = SANE_FIX(511);

    model.y_offset_calib = SANE_FIX(0.0);
    model.x_offset_mark = SANE_FIX(0.0);

    model.x_offset_ta = SANE_FIX(0.0);
    model.y_offset_ta = SANE_FIX(0.0);
    model.x_size_ta = SANE_FIX(100.0);
    model.y_size_ta = SANE_FIX(100.0);

    model.y_offset_sensor_to_ta = SANE_FIX(0.0);
    model.y_offset_calib_ta = SANE_FIX(0.0);

    model.post_scan = SANE_FIX(16.0);
    model.eject_feed = SANE_FIX(0.0);

    model.ld_shift_r = 0;
    model.ld_shift_g = 0;
    model.ld_shift_b = 0;
    model.line_mode_color_order = COLOR_ORDER_RGB;

    model.is_cis = SANE_TRUE;
    model.is_sheetfed = SANE_TRUE;
    model.ccd_type = CCD_DSMOBILE600;
    model.dac_type = DAC_WOLFSON_DSM600;
    model.gpo_type = GPO_DP665;
    model.motor_type = MOTOR_DSMOBILE_600;
    model.flags = GENESYS_FLAG_LAZY_INIT |
                  GENESYS_FLAG_SKIP_WARMUP |
                  GENESYS_FLAG_OFFSET_CALIBRATION |
                  GENESYS_FLAG_CUSTOM_GAMMA |
                  GENESYS_FLAG_DARK_CALIBRATION;
    model.buttons = GENESYS_HAS_SCAN_SW | GENESYS_HAS_PAGE_LOADED_SW | GENESYS_HAS_CALIBRATE;
    model.shading_lines = 100;
    model.shading_ta_lines = 0;
    model.search_lines = 400;

    s_usb_devices->emplace_back(0x1dcc, 0x4812, model);

    model = Genesys_Model();
    model.name = "syscan-docketport-685";
    model.vendor = "Syscan/Ambir";
    model.model = "DocketPORT 685";
    model.model_id = MODEL_SYSCAN_DOCKETPORT_685;
    model.asic_type = AsicType::GL841;

    model.xdpi_values = { 600, 300, 150, 75 };
    model.ydpi_values = { 600, 300, 150, 75 };
    model.bpp_gray_values = { 16, 8 };
    model.bpp_color_values = { 16, 8 };

    model.x_offset = SANE_FIX(0.0);
    model.y_offset = SANE_FIX(1.0);
    model.x_size = SANE_FIX(212.0);
    model.y_size = SANE_FIX(500);

    model.y_offset_calib = SANE_FIX(0.0);
    model.x_offset_mark = SANE_FIX(0.0);

    model.x_offset_ta = SANE_FIX(0.0);
    model.y_offset_ta = SANE_FIX(0.0);
    model.x_size_ta = SANE_FIX(100.0);
    model.y_size_ta = SANE_FIX(100.0);

    model.y_offset_sensor_to_ta = SANE_FIX(0.0);
    model.y_offset_calib_ta = SANE_FIX(0.0);

    model.post_scan = SANE_FIX(26.5);
    // this is larger than needed -- accounts for second sensor head, which is a calibration item
    model.eject_feed = SANE_FIX(0.0);
    model.ld_shift_r = 0;
    model.ld_shift_g = 0;
    model.ld_shift_b = 0;

    model.line_mode_color_order = COLOR_ORDER_RGB;

    model.is_cis = SANE_TRUE;
    model.is_sheetfed = SANE_TRUE;
    model.ccd_type = CCD_DP685;
    model.dac_type = DAC_WOLFSON_DSM600;
    model.gpo_type = GPO_DP685;
    model.motor_type = MOTOR_XP300;
    model.flags = GENESYS_FLAG_LAZY_INIT |
                  GENESYS_FLAG_SKIP_WARMUP |
                  GENESYS_FLAG_OFFSET_CALIBRATION |
                  GENESYS_FLAG_CUSTOM_GAMMA |
                  GENESYS_FLAG_DARK_CALIBRATION;
    model.buttons = GENESYS_HAS_SCAN_SW | GENESYS_HAS_PAGE_LOADED_SW | GENESYS_HAS_CALIBRATE;
    model.shading_lines = 100;
    model.shading_ta_lines = 0;
    model.search_lines = 400;


    s_usb_devices->emplace_back(0x0a82, 0x480c, model);


    model = Genesys_Model();
    model.name = "syscan-docketport-485";
    model.vendor = "Syscan/Ambir";
    model.model = "DocketPORT 485";
    model.model_id = MODEL_SYSCAN_DOCKETPORT_485;
    model.asic_type = AsicType::GL841;

    model.xdpi_values = { 600, 300, 150, 75 };
    model.ydpi_values = { 600, 300, 150, 75 };
    model.bpp_gray_values = { 16, 8 };
    model.bpp_color_values = { 16, 8 };

    model.x_offset = SANE_FIX(0.0);
    model.y_offset = SANE_FIX(1.0);
    model.x_size = SANE_FIX(435.0);
    model.y_size = SANE_FIX(511);

    model.y_offset_calib = SANE_FIX(0.0);
    model.x_offset_mark = SANE_FIX(0.0);

    model.x_offset_ta = SANE_FIX(0.0);
    model.y_offset_ta = SANE_FIX(0.0);
    model.x_size_ta = SANE_FIX(100.0);
    model.y_size_ta = SANE_FIX(100.0);

    model.y_offset_sensor_to_ta = SANE_FIX(0.0);
    model.y_offset_calib_ta = SANE_FIX(0.0);

    model.post_scan = SANE_FIX(26.5);
    // this is larger than needed -- accounts for second sensor head, which is a calibration item
    model.eject_feed = SANE_FIX(0.0);
    model.ld_shift_r = 0;
    model.ld_shift_g = 0;
    model.ld_shift_b = 0;

    model.line_mode_color_order = COLOR_ORDER_RGB;

    model.is_cis = SANE_TRUE;
    model.is_sheetfed = SANE_TRUE;
    model.ccd_type = CCD_XP300;
    model.dac_type = DAC_WOLFSON_XP300;
    model.gpo_type = GPO_XP300;
    model.motor_type = MOTOR_XP300;
    model.flags = GENESYS_FLAG_LAZY_INIT |
                  GENESYS_FLAG_SKIP_WARMUP |
                  GENESYS_FLAG_OFFSET_CALIBRATION |
                  GENESYS_FLAG_CUSTOM_GAMMA |
                  GENESYS_FLAG_DARK_CALIBRATION;
    model.buttons = GENESYS_HAS_SCAN_SW | GENESYS_HAS_PAGE_LOADED_SW | GENESYS_HAS_CALIBRATE;
    model.shading_lines = 100;
    model.shading_ta_lines = 0;
    model.search_lines = 400;

    s_usb_devices->emplace_back(0x0a82, 0x4800, model);


    model = Genesys_Model();
    model.name = "dct-docketport-487";
    model.vendor = "DCT";
    model.model = "DocketPORT 487";
    model.model_id = MODEL_DCT_DOCKETPORT_487;
    model.asic_type = AsicType::GL841;

    model.xdpi_values = { 600, 300, 150, 75 };
    model.ydpi_values = { 600, 300, 150, 75 };
    model.bpp_gray_values = { 16, 8 };
    model.bpp_color_values = { 16, 8 };

    model.x_offset = SANE_FIX(0.0);
    model.y_offset = SANE_FIX(1.0);
    model.x_size = SANE_FIX(435.0);
    model.y_size = SANE_FIX(511);

    model.y_offset_calib = SANE_FIX(0.0);
    model.x_offset_mark = SANE_FIX(0.0);

    model.x_offset_ta = SANE_FIX(0.0);
    model.y_offset_ta = SANE_FIX(0.0);
    model.x_size_ta = SANE_FIX(100.0);
    model.y_size_ta = SANE_FIX(100.0);

    model.y_offset_sensor_to_ta = SANE_FIX(0.0);
    model.y_offset_calib_ta = SANE_FIX(0.0);

    model.post_scan = SANE_FIX(26.5);
    // this is larger than needed -- accounts for second sensor head, which is a calibration item
    model.eject_feed = SANE_FIX(0.0);
    model.ld_shift_r = 0;
    model.ld_shift_g = 0;
    model.ld_shift_b = 0;

    model.line_mode_color_order = COLOR_ORDER_RGB;

    model.is_cis = SANE_TRUE;
    model.is_sheetfed = SANE_TRUE;
    model.ccd_type = CCD_XP300;
    model.dac_type = DAC_WOLFSON_XP300;
    model.gpo_type = GPO_XP300;
    model.motor_type = MOTOR_XP300;
    model.flags = GENESYS_FLAG_LAZY_INIT |
                  GENESYS_FLAG_SKIP_WARMUP |
                  GENESYS_FLAG_OFFSET_CALIBRATION |
                  GENESYS_FLAG_DARK_CALIBRATION |
                  GENESYS_FLAG_CUSTOM_GAMMA |
                  GENESYS_FLAG_UNTESTED;
    model.buttons = GENESYS_HAS_SCAN_SW | GENESYS_HAS_PAGE_LOADED_SW | GENESYS_HAS_CALIBRATE;
    model.shading_lines = 100;
    model.shading_ta_lines = 0;
    model.search_lines = 400;

    s_usb_devices->emplace_back(0x1dcc, 0x4810, model);


    model = Genesys_Model();
    model.name = "visioneer-7100-model";
    model.vendor = "Visioneer";
    model.model = "OneTouch 7100";
    model.model_id = MODEL_VISIONEER_7100;
    model.asic_type = AsicType::GL646;

    model.xdpi_values = { 1200, 600, 400, 300, 200, 150, 100, 75, 50 };
    model.ydpi_values = { 2400, 1200, 600, 400, 300, 200, 150, 100, 75, 50 };
    model.bpp_gray_values = { 16, 8 };
    model.bpp_color_values = { 16, 8 };

    model.x_offset = SANE_FIX(4.00);
    model.y_offset = SANE_FIX(0.80);
    model.x_size = SANE_FIX(215.9);
    model.y_size = SANE_FIX(296.4);

    model.y_offset_calib = SANE_FIX(0.00);
    model.x_offset_mark = SANE_FIX(0.00);

    model.x_offset_ta = SANE_FIX(0.00);
    model.y_offset_ta = SANE_FIX(0.00);
    model.x_size_ta = SANE_FIX(0.00);
    model.y_size_ta = SANE_FIX(0.00);

    model.y_offset_sensor_to_ta = SANE_FIX(0.0);
    model.y_offset_calib_ta = SANE_FIX(0.00);

    model.post_scan = SANE_FIX(0.0);
    model.eject_feed = SANE_FIX(0.0);

    model.ld_shift_r = 48;
    model.ld_shift_g = 24;
    model.ld_shift_b = 0;
    model.line_mode_color_order = COLOR_ORDER_RGB;

    model.is_cis = SANE_FALSE;
    model.is_sheetfed = SANE_FALSE;
    model.ccd_type = CCD_5345;
    model.dac_type = DAC_WOLFSON_5345;
    model.gpo_type = GPO_5345;
    model.motor_type = MOTOR_5345;
    model.flags = GENESYS_FLAG_14BIT_GAMMA |
                  GENESYS_FLAG_LAZY_INIT |
                  GENESYS_FLAG_SEARCH_START |
                  GENESYS_FLAG_STAGGERED_LINE |
                  GENESYS_FLAG_DARK_CALIBRATION |
                  GENESYS_FLAG_OFFSET_CALIBRATION |
                  GENESYS_FLAG_CUSTOM_GAMMA;
    model.buttons = GENESYS_HAS_COPY_SW |
                    GENESYS_HAS_EMAIL_SW |
                    GENESYS_HAS_POWER_SW |
                    GENESYS_HAS_OCR_SW |
                    GENESYS_HAS_SCAN_SW;
    model.shading_lines = 40;
    model.shading_ta_lines = 0;
    model.search_lines = 200;

    s_usb_devices->emplace_back(0x04a7, 0x0229, model);


    model = Genesys_Model();
    model.name = "xerox-2400-model";
    model.vendor = "Xerox";
    model.model = "OneTouch 2400";
    model.model_id = MODEL_XEROX_2400;
    model.asic_type = AsicType::GL646;

    model.xdpi_values = { 1200, 600, 400, 300, 200, 150, 100, 75, 50 };
    model.ydpi_values = { 2400, 1200, 600, 400, 300, 200, 150, 100, 75, 50 };
    model.bpp_gray_values = { 16, 8 };
    model.bpp_color_values = { 16, 8 };

    model.x_offset = SANE_FIX(4.00);
    model.y_offset = SANE_FIX(0.80);
    model.x_size = SANE_FIX(215.9);
    model.y_size = SANE_FIX(296.4);

    model.y_offset_calib = SANE_FIX(0.00);
    model.x_offset_mark = SANE_FIX(0.00);

    model.x_offset_ta = SANE_FIX(0.00);
    model.y_offset_ta = SANE_FIX(0.00);
    model.x_size_ta = SANE_FIX(0.00);
    model.y_size_ta = SANE_FIX(0.00);

    model.y_offset_sensor_to_ta = SANE_FIX(0.0);
    model.y_offset_calib_ta = SANE_FIX(0.00);

    model.post_scan = SANE_FIX(0.0);
    model.eject_feed = SANE_FIX(0.0);

    model.ld_shift_r = 48;
    model.ld_shift_g = 24;
    model.ld_shift_b = 0;
    model.line_mode_color_order = COLOR_ORDER_RGB;

    model.is_cis = SANE_FALSE;
    model.is_sheetfed = SANE_FALSE;
    model.ccd_type = CCD_5345;
    model.dac_type = DAC_WOLFSON_5345;
    model.gpo_type = GPO_5345;
    model.motor_type = MOTOR_5345;
    model.flags = GENESYS_FLAG_14BIT_GAMMA |
                  GENESYS_FLAG_LAZY_INIT |
                  GENESYS_FLAG_SEARCH_START |
                  GENESYS_FLAG_STAGGERED_LINE |
                  GENESYS_FLAG_DARK_CALIBRATION |
                  GENESYS_FLAG_OFFSET_CALIBRATION |
                  GENESYS_FLAG_CUSTOM_GAMMA;
    model.buttons = GENESYS_HAS_COPY_SW |
                    GENESYS_HAS_EMAIL_SW |
                    GENESYS_HAS_POWER_SW |
                    GENESYS_HAS_OCR_SW |
                    GENESYS_HAS_SCAN_SW;
    model.shading_lines = 40;
    model.shading_ta_lines = 0;
    model.search_lines = 200;

    s_usb_devices->emplace_back(0x0461, 0x038b, model);


    model = Genesys_Model();
    model.name = "xerox-travelscanner";
    model.vendor = "Xerox";
    model.model = "Travelscanner 100";
    model.model_id = MODEL_XEROX_TRAVELSCANNER_100;
    model.asic_type = AsicType::GL841;

    model.xdpi_values = { 600, 300, 150, 75 };
    model.ydpi_values = { 1200, 600, 300, 150, 75 };
    model.bpp_gray_values = { 16, 8 };
    model.bpp_color_values = { 16, 8 };

    model.x_offset = SANE_FIX(4.0);
    model.y_offset = SANE_FIX(0.0);
    model.x_size = SANE_FIX(220.0);
    model.y_size = SANE_FIX(511);

    model.y_offset_calib = SANE_FIX(0.0);
    model.x_offset_mark = SANE_FIX(0.0);

    model.x_offset_ta = SANE_FIX(0.0);
    model.y_offset_ta = SANE_FIX(0.0);
    model.x_size_ta = SANE_FIX(100.0);
    model.y_size_ta = SANE_FIX(100.0);

    model.y_offset_sensor_to_ta = SANE_FIX(0.0);
    model.y_offset_calib_ta = SANE_FIX(0.0);

    model.post_scan = SANE_FIX(16.0);
    model.eject_feed = SANE_FIX(0.0);

    model.ld_shift_r = 0;
    model.ld_shift_g = 0;
    model.ld_shift_b = 0;

    model.line_mode_color_order = COLOR_ORDER_RGB;

    model.is_cis = SANE_TRUE;
    model.is_sheetfed = SANE_TRUE;
    model.ccd_type = CCD_ROADWARRIOR;
    model.dac_type = DAC_WOLFSON_XP300;
    model.gpo_type = GPO_DP665;
    model.motor_type = MOTOR_ROADWARRIOR;
    model.flags = GENESYS_FLAG_LAZY_INIT |
                  GENESYS_FLAG_SKIP_WARMUP |
                  GENESYS_FLAG_OFFSET_CALIBRATION |
                  GENESYS_FLAG_CUSTOM_GAMMA |
                  GENESYS_FLAG_DARK_CALIBRATION;
    model.buttons = GENESYS_HAS_SCAN_SW | GENESYS_HAS_PAGE_LOADED_SW | GENESYS_HAS_CALIBRATE;
    model.shading_lines = 100;
    model.shading_ta_lines = 0;
    model.search_lines = 400;

    s_usb_devices->emplace_back(0x04a7, 0x04ac, model);


    model = Genesys_Model();
    model.name = "plustek-opticbook-3600";
    model.vendor = "PLUSTEK";
    model.model = "OpticBook 3600";
    model.model_id = MODEL_PLUSTEK_OPTICPRO_3600;
    model.asic_type = AsicType::GL841;
    model.xdpi_values = { /*1200,*/ 600, 400, 300, 200, 150, 100, 75 };
    model.ydpi_values = { /*2400,*/ 1200, 600, 400, 300, 200, 150, 100, 75 };
    model.bpp_gray_values = { 16, 8 };
    model.bpp_color_values = { 16, 8 };

    model.x_offset = SANE_FIX(0.42);
    model.y_offset = SANE_FIX(6.75);
    model.x_size = SANE_FIX(216.0);
    model.y_size = SANE_FIX(297.0);

    model.y_offset_calib = SANE_FIX(0.0);
    model.x_offset_mark = SANE_FIX(0.0);

    model.x_offset_ta = SANE_FIX(0.0);
    model.y_offset_ta = SANE_FIX(0.0);
    model.x_size_ta = SANE_FIX(0.0);
    model.y_size_ta = SANE_FIX(0.0);

    model.y_offset_sensor_to_ta = SANE_FIX(0.0);
    model.y_offset_calib_ta = SANE_FIX(0.0);

    model.post_scan = SANE_FIX(0.0);
    model.eject_feed = SANE_FIX(0.0);

    model.ld_shift_r = 0;
    model.ld_shift_g = 24;
    model.ld_shift_b = 48;

    model.line_mode_color_order = COLOR_ORDER_RGB;

    model.is_cis = SANE_FALSE;
    model.is_sheetfed = SANE_FALSE;
    model.ccd_type = CCD_PLUSTEK_3600;
    model.dac_type = DAC_PLUSTEK_3600;
    model.gpo_type = GPO_PLUSTEK_3600;
    model.motor_type = MOTOR_PLUSTEK_3600;
    model.flags = GENESYS_FLAG_UNTESTED |                // not fully working yet
                  GENESYS_FLAG_CUSTOM_GAMMA |
                  GENESYS_FLAG_SKIP_WARMUP |
                  GENESYS_FLAG_DARK_CALIBRATION |
                  GENESYS_FLAG_OFFSET_CALIBRATION |
                  GENESYS_FLAG_LAZY_INIT;
    model.buttons = GENESYS_HAS_NO_BUTTONS;
    model.shading_lines = 7;
    model.shading_ta_lines = 0;
    model.search_lines = 200;

    s_usb_devices->emplace_back(0x07b3, 0x0900, model);


    model = Genesys_Model();
    model.name = "hewlett-packard-scanjet-N6310";
    model.vendor = "Hewlett Packard";
    model.model = "ScanJet N6310";
    model.model_id = MODEL_HP_SCANJET_N6310;
    model.asic_type = AsicType::GL847;

    model.xdpi_values = { 2400, 1200, 600, 400, 300, 200, 150, 100, 75 };
    model.ydpi_values = { 2400, 1200, 600, 400, 300, 200, 150, 100, 75 };

    model.bpp_gray_values = { 16, 8 };
    model.bpp_color_values = { 16, 8 };

    model.x_offset = SANE_FIX(6);
    model.y_offset = SANE_FIX(2);
    model.x_size = SANE_FIX(216);
    model.y_size = SANE_FIX(511);

    model.y_offset_calib = SANE_FIX(3.0);
    model.x_offset_mark = SANE_FIX(0.0);

    model.x_offset_ta = SANE_FIX(0.0);
    model.y_offset_ta = SANE_FIX(0.0);
    model.x_size_ta = SANE_FIX(100.0);
    model.y_size_ta = SANE_FIX(100.0);

    model.y_offset_sensor_to_ta = SANE_FIX(0.0);
    model.y_offset_calib_ta = SANE_FIX(0);

    model.post_scan = SANE_FIX(0);
    model.eject_feed = SANE_FIX(0);

    model.ld_shift_r = 0;
    model.ld_shift_g = 0;
    model.ld_shift_b = 0;

    model.line_mode_color_order = COLOR_ORDER_RGB;

    model.is_cis = SANE_FALSE;
    model.is_sheetfed = SANE_FALSE;
    model.ccd_type = CCD_HP_N6310;
    model.dac_type = DAC_CANONLIDE200;        // Not defined yet for N6310
    model.gpo_type = GPO_HP_N6310;
    model.motor_type = MOTOR_CANONLIDE200,    // Not defined yet for N6310
    model.flags = GENESYS_FLAG_UNTESTED |
                  GENESYS_FLAG_LAZY_INIT |
                  GENESYS_FLAG_14BIT_GAMMA |
                  GENESYS_FLAG_DARK_CALIBRATION |
                  GENESYS_FLAG_OFFSET_CALIBRATION |
                  GENESYS_FLAG_CUSTOM_GAMMA |
                  GENESYS_FLAG_SKIP_WARMUP |
                  GENESYS_FLAG_NO_CALIBRATION;

    model.buttons = GENESYS_HAS_NO_BUTTONS;
    model.shading_lines = 100;
    model.shading_ta_lines = 0;
    model.search_lines = 100;

    s_usb_devices->emplace_back(0x03f0, 0x4705, model);


    model = Genesys_Model();
    model.name = "plustek-opticbook-3800";
    model.vendor = "PLUSTEK";
    model.model = "OpticBook 3800";
    model.model_id = MODEL_PLUSTEK_OPTICBOOK_3800;
    model.asic_type = AsicType::GL845;

    model.xdpi_values = { 1200, 600, 300, 150, 100, 75 };
    model.ydpi_values = { 1200, 600, 300, 150, 100, 75 };
    model.bpp_gray_values = { 16, 8 };
    model.bpp_color_values = { 16, 8 };

    model.x_offset = SANE_FIX(7.2);
    model.y_offset = SANE_FIX(14.7);
    model.x_size = SANE_FIX(217.7);
    model.y_size = SANE_FIX(300.0);

    model.y_offset_calib = SANE_FIX(9.0);
    model.x_offset_mark = SANE_FIX(0.0);

    model.x_offset_ta = SANE_FIX(0.0);
    model.y_offset_ta = SANE_FIX(0.0);
    model.x_size_ta = SANE_FIX(0.0);
    model.y_size_ta = SANE_FIX(0.0);

    model.y_offset_sensor_to_ta = SANE_FIX(0.0);
    model.y_offset_calib_ta = SANE_FIX(0.0);

    model.post_scan = SANE_FIX(0.0);
    model.eject_feed = SANE_FIX(0.0);

    model.ld_shift_r = 0;
    model.ld_shift_g = 24;
    model.ld_shift_b = 48;

    model.line_mode_color_order = COLOR_ORDER_RGB;

    model.is_cis = SANE_FALSE;
    model.is_sheetfed = SANE_FALSE;
    model.ccd_type = CCD_PLUSTEK3800;
    model.dac_type = DAC_PLUSTEK3800;
    model.gpo_type = GPO_PLUSTEK3800;
    model.motor_type = MOTOR_PLUSTEK3800;
    model.flags = GENESYS_FLAG_LAZY_INIT |
                  GENESYS_FLAG_SKIP_WARMUP |
                  GENESYS_FLAG_OFFSET_CALIBRATION |
                  GENESYS_FLAG_CUSTOM_GAMMA;
    model.buttons = GENESYS_HAS_NO_BUTTONS;  // TODO there are 4 buttons to support
    model.shading_lines = 100;
    model.shading_ta_lines = 0;
    model.search_lines = 100;

    s_usb_devices->emplace_back(0x07b3, 0x1300, model);


    model = Genesys_Model();
    model.name = "canon-image-formula-101";
    model.vendor = "Canon";
    model.model = "Image Formula 101";
    model.model_id = MODEL_CANON_IMAGE_FORMULA_101;
    model.asic_type = AsicType::GL846;

    model.xdpi_values = { 1200, 600, 300, 150, 100, 75 };
    model.ydpi_values = { 1200, 600, 300, 150, 100, 75 };
    model.bpp_gray_values = { 16, 8 };
    model.bpp_color_values = { 16, 8 };

    model.x_offset = SANE_FIX(7.2);
    model.y_offset = SANE_FIX(14.7);
    model.x_size = SANE_FIX(217.7);
    model.y_size = SANE_FIX(300.0);

    model.y_offset_calib = SANE_FIX(9.0);
    model.x_offset_mark = SANE_FIX(0.0);

    model.x_offset_ta = SANE_FIX(0.0);
    model.y_offset_ta = SANE_FIX(0.0);
    model.x_size_ta = SANE_FIX(0.0);
    model.y_size_ta = SANE_FIX(0.0);

    model.y_offset_sensor_to_ta = SANE_FIX(0.0);
    model.y_offset_calib_ta = SANE_FIX(0.0);

    model.post_scan = SANE_FIX(0.0);
    model.eject_feed = SANE_FIX(0.0);

    model.ld_shift_r = 0;
    model.ld_shift_g = 24;
    model.ld_shift_b = 48;

    model.line_mode_color_order = COLOR_ORDER_RGB;

    model.is_cis = SANE_FALSE;
    model.is_sheetfed = SANE_FALSE;
    model.ccd_type = CCD_IMG101;
    model.dac_type = DAC_IMG101;
    model.gpo_type = GPO_IMG101;
    model.motor_type = MOTOR_IMG101;
    model.flags = GENESYS_FLAG_LAZY_INIT |
                  GENESYS_FLAG_SKIP_WARMUP |
                  GENESYS_FLAG_OFFSET_CALIBRATION |
                  GENESYS_FLAG_CUSTOM_GAMMA;
    model.buttons = GENESYS_HAS_NO_BUTTONS ;
    model.shading_lines = 100;
    model.shading_ta_lines = 0;
    model.search_lines = 100;

    s_usb_devices->emplace_back(0x1083, 0x162e, model);
 }
