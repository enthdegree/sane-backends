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

#ifndef BACKEND_GENESYS_ENUMS_H
#define BACKEND_GENESYS_ENUMS_H

#include <iostream>
#include "genesys_serialize.h"

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

enum class ColorOrder
{
    RGB,
    GBR,
    BGR,
};

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

enum class AsicType : unsigned
{
    UNKNOWN = 0,
    GL646,
    GL841,
    GL843,
    GL845,
    GL846,
    GL847,
    GL124,
};

#endif // BACKEND_GENESYS_ENUMS_H
