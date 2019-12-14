/*  sane - Scanner Access Now Easy.

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

#define DEBUG_DECLARE_ONLY

#include "low.h"

namespace genesys {

StaticInit<std::vector<Motor_Profile>> gl843_motor_profiles;

void genesys_init_motor_profile_tables_gl843()
{
    gl843_motor_profiles.init();

    auto profile = Motor_Profile();
    profile.motor_id = MotorId::KVSS080;
    profile.exposure = 8000;
    profile.step_type = StepType::HALF;
    profile.slope = MotorSlope::create_from_steps(44444, 500, 489);
    gl843_motor_profiles->push_back(profile);

    profile = Motor_Profile();
    profile.motor_id = MotorId::G4050;
    profile.exposure = 8016;
    profile.step_type = StepType::HALF;
    profile.slope = MotorSlope::create_from_steps(7842, 320, 602);
    gl843_motor_profiles->push_back(profile);

    profile = Motor_Profile();
    profile.motor_id = MotorId::G4050;
    profile.exposure = 15624;
    profile.step_type = StepType::HALF;
    profile.slope = MotorSlope::create_from_steps(9422, 254, 1004);
    gl843_motor_profiles->push_back(profile);

    profile = Motor_Profile();
    profile.motor_id = MotorId::G4050;
    profile.exposure = 42752;
    profile.step_type = StepType::QUARTER;
    profile.slope = MotorSlope::create_from_steps(42752, 1706, 610);
    gl843_motor_profiles->push_back(profile);

    profile = Motor_Profile();
    profile.motor_id = MotorId::G4050;
    profile.exposure = 56064;
    profile.step_type = StepType::HALF;
    profile.slope = MotorSlope::create_from_steps(28032, 2238, 604);
    gl843_motor_profiles->push_back(profile);

    profile = Motor_Profile();
    profile.motor_id = MotorId::CANON_4400F;
    profile.exposure = 11640;
    profile.step_type = StepType::HALF;
    profile.slope = MotorSlope::create_from_steps(49152, 484, 1014);
    gl843_motor_profiles->push_back(profile);

    profile = Motor_Profile();
    profile.motor_id = MotorId::CANON_8400F;
    profile.exposure = 50000;
    profile.step_type = StepType::QUARTER;
    profile.slope = MotorSlope::create_from_steps(8743, 300, 794);
    gl843_motor_profiles->push_back(profile);

    profile = Motor_Profile();
    profile.motor_id = MotorId::CANON_8600F;
    profile.exposure = 0x59d8;
    profile.step_type = StepType::QUARTER;
    // FIXME: if the exposure is lower then we'll select another motor
    profile.slope = MotorSlope::create_from_steps(54612, 1500, 219);
    gl843_motor_profiles->push_back(profile);

    /* TODO:
        1800 dpi: 31250, 3105, 3025, 3025

        3600 dpi: 31250, 6050, 6050

        7200 dpi: 31250, 12100, 12100

        isrd:

        1800 dpi: 31250, 3105, 2750, 2750

        3600 dpi: 31250, 5500, 5500,

        7200 dpi: 31250, 11000, 11000,
    */
    profile = Motor_Profile();
    profile.motor_id = MotorId::PLUSTEK_OPTICFILM_7200I;
    profile.exposure = 0x19c8;
    profile.step_type = StepType::HALF;
    profile.slope = MotorSlope::create_from_steps(11000, 11000, 2);
    gl843_motor_profiles->push_back(profile);

    profile = Motor_Profile();
    profile.motor_id = MotorId::PLUSTEK_OPTICFILM_7200I;
    profile.exposure = 0x2538;
    profile.step_type = StepType::HALF;
    profile.slope = MotorSlope::create_from_steps(15880, 15880, 2);
    gl843_motor_profiles->push_back(profile);

    profile = Motor_Profile();
    profile.motor_id = MotorId::PLUSTEK_OPTICFILM_7300;
    profile.exposure = 0x2f44;
    profile.step_type = StepType::QUARTER;
    profile.slope = MotorSlope::create_from_steps(31250, 1512, 6);
    gl843_motor_profiles->push_back(profile);

    profile = Motor_Profile();
    profile.motor_id = MotorId::PLUSTEK_OPTICFILM_7500I;
    profile.exposure = 0x2f44;
    profile.step_type = StepType::QUARTER;
    profile.slope = MotorSlope::create_from_steps(31250, 1512, 6);
    gl843_motor_profiles->push_back(profile);

    profile = Motor_Profile();
    profile.motor_id = MotorId::PLUSTEK_OPTICFILM_7500I;
    profile.exposure = 0x2af8;
    profile.step_type = StepType::QUARTER;
    profile.slope = MotorSlope::create_from_steps(31250, 1375, 7);
    gl843_motor_profiles->push_back(profile);
}

StaticInit<std::vector<Motor_Profile>> gl846_motor_profiles;

void genesys_init_motor_profile_tables_gl846()
{
    gl846_motor_profiles.init();

    auto profile = Motor_Profile();
    profile.motor_id = MotorId::IMG101;
    profile.exposure = 11000;
    profile.step_type = StepType::HALF;
    profile.slope = MotorSlope::create_from_steps(22000, 1000, 1017);

    gl846_motor_profiles->push_back(profile);

    profile = Motor_Profile();
    profile.motor_id = MotorId::PLUSTEK_OPTICBOOK_3800;
    profile.exposure = 11000;
    profile.step_type = StepType::HALF;
    profile.slope = MotorSlope::create_from_steps(22000, 1000, 1017);
    gl846_motor_profiles->push_back(profile);
}

/**
 * database of motor profiles
 */

StaticInit<std::vector<Motor_Profile>> gl847_motor_profiles;

void genesys_init_motor_profile_tables_gl847()
{
    gl847_motor_profiles.init();

    auto profile = Motor_Profile();
    profile.motor_id = MotorId::CANON_LIDE_100;
    profile.exposure = 2848;
    profile.step_type = StepType::HALF;
    profile.slope = MotorSlope::create_from_steps(46876, 534, 255);
    gl847_motor_profiles->push_back(profile);

    profile = Motor_Profile();
    profile.motor_id = MotorId::CANON_LIDE_100;
    profile.exposure = 1424;
    profile.step_type = StepType::HALF;
    profile.slope = MotorSlope::create_from_steps(46876, 534, 255);
    gl847_motor_profiles->push_back(profile);

    profile = Motor_Profile();
    profile.motor_id = MotorId::CANON_LIDE_100;
    profile.exposure = 1432;
    profile.step_type = StepType::HALF;
    profile.slope = MotorSlope::create_from_steps(46876, 534, 255);
    gl847_motor_profiles->push_back(profile);

    profile = Motor_Profile();
    profile.motor_id = MotorId::CANON_LIDE_100;
    profile.exposure = 2712;
    profile.step_type = StepType::QUARTER;
    profile.slope = MotorSlope::create_from_steps(46876, 534, 279);
    gl847_motor_profiles->push_back(profile);

    profile = Motor_Profile();
    profile.motor_id = MotorId::CANON_LIDE_100;
    profile.exposure = 5280;
    profile.step_type = StepType::EIGHTH;
    profile.slope = MotorSlope::create_from_steps(31680, 534, 247);
    gl847_motor_profiles->push_back(profile);

    profile = Motor_Profile();
    profile.motor_id = MotorId::CANON_LIDE_200;
    profile.exposure = 2848;
    profile.step_type = StepType::HALF;
    profile.slope = MotorSlope::create_from_steps(46876, 534, 255);
    gl847_motor_profiles->push_back(profile);

    profile = Motor_Profile();
    profile.motor_id = MotorId::CANON_LIDE_200;
    profile.exposure = 1424;
    profile.step_type = StepType::HALF;
    profile.slope = MotorSlope::create_from_steps(46876, 534, 255);
    gl847_motor_profiles->push_back(profile);

    profile = Motor_Profile();
    profile.motor_id = MotorId::CANON_LIDE_200;
    profile.exposure = 1432;
    profile.step_type = StepType::HALF;
    profile.slope = MotorSlope::create_from_steps(46876, 534, 255);
    gl847_motor_profiles->push_back(profile);

    profile = Motor_Profile();
    profile.motor_id = MotorId::CANON_LIDE_200;
    profile.exposure = 2712;
    profile.step_type = StepType::QUARTER;
    profile.slope = MotorSlope::create_from_steps(46876, 534, 279);
    gl847_motor_profiles->push_back(profile);

    profile = Motor_Profile();
    profile.motor_id = MotorId::CANON_LIDE_200;
    profile.exposure = 5280;
    profile.step_type = StepType::EIGHTH;
    profile.slope = MotorSlope::create_from_steps(31680, 534, 247);
    gl847_motor_profiles->push_back(profile);

    profile = Motor_Profile();
    profile.motor_id = MotorId::CANON_LIDE_200;
    profile.exposure = 10416;
    profile.step_type = StepType::EIGHTH;
    profile.slope = MotorSlope::create_from_steps(31680, 534, 247);
    gl847_motor_profiles->push_back(profile);

    profile = Motor_Profile();
    profile.motor_id = MotorId::CANON_LIDE_700;
    profile.exposure = 2848;
    profile.step_type = StepType::HALF;
    profile.slope = MotorSlope::create_from_steps(46876, 534, 255);
    gl847_motor_profiles->push_back(profile);

    profile = Motor_Profile();
    profile.motor_id = MotorId::CANON_LIDE_700;
    profile.exposure = 1424;
    profile.step_type = StepType::HALF;
    profile.slope = MotorSlope::create_from_steps(46876, 534, 255);
    gl847_motor_profiles->push_back(profile);

    profile = Motor_Profile();
    profile.motor_id = MotorId::CANON_LIDE_700;
    profile.exposure = 1504;
    profile.step_type = StepType::HALF;
    profile.slope = MotorSlope::create_from_steps(46876, 534, 255);
    gl847_motor_profiles->push_back(profile);

    profile = Motor_Profile();
    profile.motor_id = MotorId::CANON_LIDE_700;
    profile.exposure = 2696;
    profile.step_type = StepType::HALF;
    profile.slope = MotorSlope::create_from_steps(46876, 2022, 127);
    gl847_motor_profiles->push_back(profile);

    profile = Motor_Profile();
    profile.motor_id = MotorId::CANON_LIDE_700;
    profile.exposure = 10576;
    profile.step_type = StepType::EIGHTH;
    profile.slope = MotorSlope::create_from_steps(46876, 15864, 2);
    gl847_motor_profiles->push_back(profile);
}

StaticInit<std::vector<Motor_Profile>> gl124_motor_profiles;

void genesys_init_motor_profile_tables_gl124()
{
    gl124_motor_profiles.init();

    // NEXT LPERIOD=PREVIOUS*2-192
    Motor_Profile profile;
    profile.motor_id = MotorId::CANON_LIDE_110;
    profile.exposure = 2768;
    profile.step_type = StepType::FULL;
    profile.slope = MotorSlope::create_from_steps(62496, 335, 255);
    gl124_motor_profiles->push_back(profile);

    profile = Motor_Profile();
    profile.motor_id = MotorId::CANON_LIDE_110;
    profile.exposure = 5360;
    profile.step_type = StepType::HALF;
    profile.slope = MotorSlope::create_from_steps(62496, 335, 469);
    gl124_motor_profiles->push_back(profile);

    profile = Motor_Profile();
    profile.motor_id = MotorId::CANON_LIDE_110;
    profile.exposure = 10528;
    profile.step_type = StepType::HALF;
    profile.slope = MotorSlope::create_from_steps(62496, 2632, 3);
    gl124_motor_profiles->push_back(profile);

    profile = Motor_Profile();
    profile.motor_id = MotorId::CANON_LIDE_110;
    profile.exposure = 20864;
    profile.step_type = StepType::QUARTER;
    profile.slope = MotorSlope::create_from_steps(62496, 10432, 3);
    gl124_motor_profiles->push_back(profile);

    profile = Motor_Profile();
    profile.motor_id = MotorId::CANON_LIDE_120;
    profile.exposure = 4608;
    profile.step_type = StepType::FULL;
    profile.slope = MotorSlope::create_from_steps(62496, 864, 127);
    gl124_motor_profiles->push_back(profile);

    profile = Motor_Profile();
    profile.motor_id = MotorId::CANON_LIDE_120;
    profile.exposure = 5360;
    profile.step_type = StepType::HALF;
    profile.slope = MotorSlope::create_from_steps(62496, 2010, 63);
    gl124_motor_profiles->push_back(profile);

    profile = Motor_Profile();
    profile.motor_id = MotorId::CANON_LIDE_120;
    profile.exposure = 10528;
    profile.step_type = StepType::QUARTER;
    profile.slope = MotorSlope::create_from_steps(62464, 2632, 3);
    gl124_motor_profiles->push_back(profile);

    profile = Motor_Profile();
    profile.motor_id = MotorId::CANON_LIDE_120;
    profile.exposure = 20864;
    profile.step_type = StepType::QUARTER;
    profile.slope = MotorSlope::create_from_steps(62592, 10432, 5);
    gl124_motor_profiles->push_back(profile);

    profile = Motor_Profile();
    profile.motor_id = MotorId::CANON_LIDE_210;
    profile.exposure = 2768;
    profile.step_type = StepType::FULL;
    profile.slope = MotorSlope::create_from_steps(62496, 335, 255);
    gl124_motor_profiles->push_back(profile);

    profile = Motor_Profile();
    profile.motor_id = MotorId::CANON_LIDE_210;
    profile.exposure = 5360;
    profile.step_type = StepType::HALF;
    profile.slope = MotorSlope::create_from_steps(62496, 335, 469);
    gl124_motor_profiles->push_back(profile);

    profile = Motor_Profile();
    profile.motor_id = MotorId::CANON_LIDE_210;
    profile.exposure = 10528;
    profile.step_type = StepType::HALF;
    profile.slope = MotorSlope::create_from_steps(62496, 2632, 3);
    gl124_motor_profiles->push_back(profile);

    profile = Motor_Profile();
    profile.motor_id = MotorId::CANON_LIDE_210;
    profile.exposure = 20864;
    profile.step_type = StepType::QUARTER;
    profile.slope = MotorSlope::create_from_steps(62496, 10432, 4);
    gl124_motor_profiles->push_back(profile);
}

void genesys_init_motor_profile_tables()
{
    genesys_init_motor_profile_tables_gl843();
    genesys_init_motor_profile_tables_gl846();
    genesys_init_motor_profile_tables_gl847();
    genesys_init_motor_profile_tables_gl124();
}

} // namespace genesys
