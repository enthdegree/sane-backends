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

StaticInit<std::vector<Genesys_Motor>> s_motors;

void genesys_init_motor_tables()
{
    s_motors.init();

    Genesys_Motor motor;
    motor.id = MotorId::UMAX;
    motor.base_ydpi = 1200;
    motor.optical_ydpi = 2400;
    motor.profiles.push_back({MotorSlope::create_from_steps(11000, 3000, 128), StepType::FULL, 0});
    motor.profiles.push_back({MotorSlope::create_from_steps(11000, 3000, 128), StepType::HALF, 0});
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::MD_5345; // MD5345/6228/6471
    motor.base_ydpi = 1200;
    motor.optical_ydpi = 2400;
    motor.profiles.push_back({MotorSlope::create_from_steps(2000, 1375, 128), StepType::FULL, 0});
    motor.profiles.push_back({MotorSlope::create_from_steps(2000, 1375, 128), StepType::HALF, 0});
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::ST24;
    motor.base_ydpi = 2400;
    motor.optical_ydpi = 2400;
    motor.profiles.push_back({MotorSlope::create_from_steps(2289, 2100, 128), StepType::FULL, 0});
    motor.profiles.push_back({MotorSlope::create_from_steps(2289, 2100, 128), StepType::HALF, 0});
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::HP3670;
    motor.base_ydpi = 1200;
    motor.optical_ydpi = 1200;
    motor.profiles.push_back({MotorSlope::create_from_steps(11000, 3000, 128), StepType::FULL, 0});
    motor.profiles.push_back({MotorSlope::create_from_steps(11000, 3000, 128), StepType::HALF, 0});
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::HP2400;
    motor.base_ydpi = 1200;
    motor.optical_ydpi = 1200;
    motor.profiles.push_back({MotorSlope::create_from_steps(11000, 3000, 128), StepType::FULL, 0});
    motor.profiles.push_back({MotorSlope::create_from_steps(11000, 3000, 128), StepType::HALF, 0});
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::HP2300;
    motor.base_ydpi = 600;
    motor.optical_ydpi = 1200;
    motor.profiles.push_back({MotorSlope::create_from_steps(3200, 1200, 128), StepType::FULL, 0});
    motor.profiles.push_back({MotorSlope::create_from_steps(3200, 1200, 128), StepType::HALF, 0});
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::CANON_LIDE_35;
    motor.base_ydpi = 1200;
    motor.optical_ydpi = 2400;
    motor.profiles.push_back({MotorSlope::create_from_steps(3500, 1300, 60), StepType::FULL, 0});
    motor.profiles.push_back({MotorSlope::create_from_steps(3500, 1400, 60), StepType::HALF, 0});
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::XP200;
    motor.base_ydpi = 600;
    motor.optical_ydpi = 600;
    motor.profiles.push_back({MotorSlope::create_from_steps(3500, 1300, 60), StepType::FULL, 0});
    motor.profiles.push_back({MotorSlope::create_from_steps(3500, 1300, 60), StepType::HALF, 0});
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::XP300;
    motor.base_ydpi = 300;
    motor.optical_ydpi = 600;
    // works best with GPIO10, GPIO14 off
    motor.profiles.push_back({MotorSlope::create_from_steps(3700, 3700, 2), StepType::FULL, 0});
    motor.profiles.push_back({MotorSlope::create_from_steps(11000, 11000, 2), StepType::HALF, 0});
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::DP665;
    motor.base_ydpi = 750;
    motor.optical_ydpi = 1500;
    motor.profiles.push_back({MotorSlope::create_from_steps(3000, 2500, 10), StepType::FULL, 0});
    motor.profiles.push_back({MotorSlope::create_from_steps(11000, 11000, 2), StepType::HALF, 0});
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::ROADWARRIOR;
    motor.base_ydpi = 750;
    motor.optical_ydpi = 1500;
    motor.profiles.push_back({MotorSlope::create_from_steps(3000, 2600, 10), StepType::FULL, 0});
    motor.profiles.push_back({MotorSlope::create_from_steps(11000, 11000, 2), StepType::HALF, 0});
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::DSMOBILE_600;
    motor.base_ydpi = 750;
    motor.optical_ydpi = 1500;
    motor.profiles.push_back({MotorSlope::create_from_steps(6666, 3700, 8), StepType::FULL, 0});
    motor.profiles.push_back({MotorSlope::create_from_steps(6666, 3700, 8), StepType::HALF, 0});
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::CANON_LIDE_100;
    motor.base_ydpi = 1200;
    motor.optical_ydpi = 6400;
    motor.profiles.push_back({MotorSlope::create_from_steps(46876, 534, 255),
                              StepType::HALF, 1424});
    motor.profiles.push_back({MotorSlope::create_from_steps(46876, 534, 255),
                              StepType::HALF, 1432});
    motor.profiles.push_back({MotorSlope::create_from_steps(46876, 534, 255),
                              StepType::HALF, 2848});
    motor.profiles.push_back({MotorSlope::create_from_steps(46876, 534, 279),
                              StepType::QUARTER, 2712});
    motor.profiles.push_back({MotorSlope::create_from_steps(31680, 534, 247),
                              StepType::EIGHTH, 5280});
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::CANON_LIDE_200;
    motor.base_ydpi = 1200;
    motor.optical_ydpi = 6400;
    motor.profiles.push_back({MotorSlope::create_from_steps(46876, 534, 255),
                              StepType::HALF, 1424});
    motor.profiles.push_back({MotorSlope::create_from_steps(46876, 534, 255),
                              StepType::HALF, 1432});
    motor.profiles.push_back({MotorSlope::create_from_steps(46876, 534, 255),
                              StepType::HALF, 2848});
    motor.profiles.push_back({MotorSlope::create_from_steps(46876, 534, 279),
                              StepType::QUARTER, 2712});
    motor.profiles.push_back({MotorSlope::create_from_steps(31680, 534, 247),
                              StepType::EIGHTH, 5280});
    motor.profiles.push_back({MotorSlope::create_from_steps(31680, 534, 247),
                              StepType::EIGHTH, 10416});
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::CANON_LIDE_700;
    motor.base_ydpi = 1200;
    motor.optical_ydpi = 6400;
    motor.profiles.push_back({MotorSlope::create_from_steps(46876, 534, 255),
                              StepType::HALF, 1424});
    motor.profiles.push_back({MotorSlope::create_from_steps(46876, 534, 255),
                              StepType::HALF, 1504});
    motor.profiles.push_back({MotorSlope::create_from_steps(46876, 2022, 127),
                              StepType::HALF, 2696});
    motor.profiles.push_back({MotorSlope::create_from_steps(46876, 534, 255),
                              StepType::HALF, 2848});
    motor.profiles.push_back({MotorSlope::create_from_steps(46876, 15864, 2),
                              StepType::EIGHTH, 10576});
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::KVSS080;
    motor.base_ydpi = 1200;
    motor.optical_ydpi = 1200;
    motor.profiles.push_back({MotorSlope::create_from_steps(44444, 500, 489),
                              StepType::HALF, 8000});
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::G4050;
    motor.base_ydpi = 2400;
    motor.optical_ydpi = 9600;
    motor.profiles.push_back({MotorSlope::create_from_steps(7842, 320, 602),
                              StepType::HALF, 8016});
    motor.profiles.push_back({MotorSlope::create_from_steps(9422, 254, 1004),
                              StepType::HALF, 15624});
    motor.profiles.push_back({MotorSlope::create_from_steps(28032, 2238, 604),
                              StepType::HALF, 56064});
    motor.profiles.push_back({MotorSlope::create_from_steps(42752, 1706, 610),
                              StepType::QUARTER, 42752});
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::CANON_4400F;
    motor.base_ydpi = 2400;
    motor.optical_ydpi = 9600;
    motor.profiles.push_back({MotorSlope::create_from_steps(49152, 484, 1014),
                              StepType::HALF, 11640});
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::CANON_8400F;
    motor.base_ydpi = 1600;
    motor.optical_ydpi = 6400;
    motor.profiles.push_back({MotorSlope::create_from_steps(8743, 300, 794),
                              StepType::QUARTER, 50000});
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::CANON_8600F;
    motor.base_ydpi = 2400;
    motor.optical_ydpi = 9600;
    // BUG: this is a fallback slope that was selected previously and preserved for compatibility
    motor.profiles.push_back({MotorSlope::create_from_steps(44444, 500, 489), StepType::HALF, 0});
    motor.profiles.push_back({MotorSlope::create_from_steps(54612, 1500, 219),
                              StepType::QUARTER, 23000});
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::CANON_LIDE_110;
    motor.base_ydpi = 4800;
    motor.optical_ydpi = 9600;
    motor.profiles.push_back({MotorSlope::create_from_steps(62496, 335, 255),
                              StepType::FULL, 2768});
    motor.profiles.push_back({MotorSlope::create_from_steps(62496, 335, 469),
                              StepType::HALF, 5360});
    motor.profiles.push_back({MotorSlope::create_from_steps(62496, 2632, 3),
                              StepType::HALF, 10528});
    motor.profiles.push_back({MotorSlope::create_from_steps(62496, 10432, 3),
                              StepType::QUARTER, 20864});
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::CANON_LIDE_120;
    motor.base_ydpi = 4800;
    motor.optical_ydpi = 9600;
    motor.profiles.push_back({MotorSlope::create_from_steps(62496, 864, 127),
                              StepType::FULL, 4608});
    motor.profiles.push_back({MotorSlope::create_from_steps(62496, 2010, 63),
                              StepType::HALF, 5360});
    motor.profiles.push_back({MotorSlope::create_from_steps(62464, 2632, 3),
                              StepType::QUARTER, 10528});
    motor.profiles.push_back({MotorSlope::create_from_steps(62592, 10432, 5),
                              StepType::QUARTER, 20864});
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::CANON_LIDE_210;
    motor.base_ydpi = 4800;
    motor.optical_ydpi = 9600;
    motor.profiles.push_back({MotorSlope::create_from_steps(62496, 335, 255),
                              StepType::FULL, 2768});
    motor.profiles.push_back({MotorSlope::create_from_steps(62496, 335, 469),
                              StepType::HALF, 5360});
    motor.profiles.push_back({MotorSlope::create_from_steps(62496, 2632, 3),
                              StepType::HALF, 10528});
    motor.profiles.push_back({MotorSlope::create_from_steps(62496, 10432, 4),
                              StepType::QUARTER, 20864});
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::PLUSTEK_OPTICPRO_3600;
    motor.base_ydpi = 1200;
    motor.optical_ydpi = 2400;
    motor.profiles.push_back({MotorSlope::create_from_steps(3500, 1300, 60), StepType::FULL, 0});
    motor.profiles.push_back({MotorSlope::create_from_steps(3500, 3250, 60), StepType::HALF, 0});
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::PLUSTEK_OPTICFILM_7200I;
    motor.base_ydpi = 3600;
    motor.optical_ydpi = 3600;
    motor.profiles.push_back({MotorSlope::create_from_steps(39682, 1191, 15), StepType::HALF, 0});
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::PLUSTEK_OPTICFILM_7300;
    motor.base_ydpi = 3600;
    motor.optical_ydpi = 3600;
    motor.profiles.push_back({MotorSlope::create_from_steps(31250, 1512, 6),
                              StepType::QUARTER, 12100});
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::PLUSTEK_OPTICFILM_7500I;
    motor.base_ydpi = 3600;
    motor.optical_ydpi = 3600;
    motor.profiles.push_back({MotorSlope::create_from_steps(31250, 1375, 7),
                              StepType::QUARTER, 0});
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::IMG101;
    motor.base_ydpi = 600;
    motor.optical_ydpi = 1200;
    motor.profiles.push_back({MotorSlope::create_from_steps(22000, 1000, 1017),
                              StepType::HALF, 11000});
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::PLUSTEK_OPTICBOOK_3800;
    motor.base_ydpi = 600;
    motor.optical_ydpi = 1200;
    motor.profiles.push_back({MotorSlope::create_from_steps(22000, 1000, 1017),
                              StepType::HALF, 11000});
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::CANON_LIDE_80;
    motor.base_ydpi = 2400;
    motor.optical_ydpi = 4800; // 9600
    motor.profiles.push_back({MotorSlope::create_from_steps(9560, 1912, 31), StepType::FULL, 0});
    s_motors->push_back(std::move(motor));
}

} // namespace genesys
