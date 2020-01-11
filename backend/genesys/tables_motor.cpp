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
    motor.slopes.push_back(MotorSlope::create_from_steps(11000, 3000, 128));
    motor.slopes.push_back(MotorSlope::create_from_steps(11000, 3000, 128));
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::MD_5345; // MD5345/6228/6471
    motor.base_ydpi = 1200;
    motor.optical_ydpi = 2400;
    motor.slopes.push_back(MotorSlope::create_from_steps(2000, 1375, 128));
    motor.slopes.push_back(MotorSlope::create_from_steps(2000, 1375, 128));
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::ST24;
    motor.base_ydpi = 2400;
    motor.optical_ydpi = 2400;
    motor.slopes.push_back(MotorSlope::create_from_steps(2289, 2100, 128));
    motor.slopes.push_back(MotorSlope::create_from_steps(2289, 2100, 128));
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::HP3670;
    motor.base_ydpi = 1200;
    motor.optical_ydpi = 1200;
    motor.slopes.push_back(MotorSlope::create_from_steps(11000, 3000, 128));
    motor.slopes.push_back(MotorSlope::create_from_steps(11000, 3000, 128));
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::HP2400;
    motor.base_ydpi = 1200;
    motor.optical_ydpi = 1200;
    motor.slopes.push_back(MotorSlope::create_from_steps(11000, 3000, 128));
    motor.slopes.push_back(MotorSlope::create_from_steps(11000, 3000, 128));
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::HP2300;
    motor.base_ydpi = 600;
    motor.optical_ydpi = 1200;
    motor.slopes.push_back(MotorSlope::create_from_steps(3200, 1200, 128));
    motor.slopes.push_back(MotorSlope::create_from_steps(3200, 1200, 128));
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::CANON_LIDE_35;
    motor.base_ydpi = 1200;
    motor.optical_ydpi = 2400;
    motor.slopes.push_back(MotorSlope::create_from_steps(3500, 1300, 60));
    motor.slopes.push_back(MotorSlope::create_from_steps(3500, 1400, 60));
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::XP200;
    motor.base_ydpi = 600;
    motor.optical_ydpi = 600;
    motor.slopes.push_back(MotorSlope::create_from_steps(3500, 1300, 60));
    motor.slopes.push_back(MotorSlope::create_from_steps(3500, 1300, 60));
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::XP300;
    motor.base_ydpi = 300;
    motor.optical_ydpi = 600;
    // works best with GPIO10, GPIO14 off
    motor.slopes.push_back(MotorSlope::create_from_steps(3700, 3700, 2));
    motor.slopes.push_back(MotorSlope::create_from_steps(11000, 11000, 2));
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::DP665;
    motor.base_ydpi = 750;
    motor.optical_ydpi = 1500;
    motor.slopes.push_back(MotorSlope::create_from_steps(3000, 2500, 10));
    motor.slopes.push_back(MotorSlope::create_from_steps(11000, 11000, 2));
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::ROADWARRIOR;
    motor.base_ydpi = 750;
    motor.optical_ydpi = 1500;
    motor.slopes.push_back(MotorSlope::create_from_steps(3000, 2600, 10));
    motor.slopes.push_back(MotorSlope::create_from_steps(11000, 11000, 2));
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::DSMOBILE_600;
    motor.base_ydpi = 750;
    motor.optical_ydpi = 1500;
    motor.slopes.push_back(MotorSlope::create_from_steps(6666, 3700, 8));
    motor.slopes.push_back(MotorSlope::create_from_steps(6666, 3700, 8));
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::CANON_LIDE_100;
    motor.base_ydpi = 1200;
    motor.optical_ydpi = 6400;
    motor.slopes.push_back(MotorSlope::create_from_steps(3000, 1000, 127));
    motor.slopes.push_back(MotorSlope::create_from_steps(3000, 1500, 127));
    motor.slopes.push_back(MotorSlope::create_from_steps(3 * 2712, 3 * 2712, 16));
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::CANON_LIDE_200;
    motor.base_ydpi = 1200;
    motor.optical_ydpi = 6400;
    motor.slopes.push_back(MotorSlope::create_from_steps(3000, 1000, 127));
    motor.slopes.push_back(MotorSlope::create_from_steps(3000, 1500, 127));
    motor.slopes.push_back(MotorSlope::create_from_steps(3 * 2712, 3 * 2712, 16));
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::CANON_LIDE_700;
    motor.base_ydpi = 1200;
    motor.optical_ydpi = 6400;
    motor.slopes.push_back(MotorSlope::create_from_steps(3000, 1000, 127));
    motor.slopes.push_back(MotorSlope::create_from_steps(3000, 1500, 127));
    motor.slopes.push_back(MotorSlope::create_from_steps(3 * 2712, 3 * 2712, 16));
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::KVSS080;
    motor.base_ydpi = 1200;
    motor.optical_ydpi = 1200;
    motor.slopes.push_back(MotorSlope::create_from_steps(22222, 500, 246));
    motor.slopes.push_back(MotorSlope::create_from_steps(22222, 500, 246));
    motor.slopes.push_back(MotorSlope::create_from_steps(22222, 500, 246));
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::G4050;
    motor.base_ydpi = 2400;
    motor.optical_ydpi = 9600;
    motor.slopes.push_back(MotorSlope::create_from_steps(3961, 240, 246));
    motor.slopes.push_back(MotorSlope::create_from_steps(3961, 240, 246));
    motor.slopes.push_back(MotorSlope::create_from_steps(3961, 240, 246));
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::CANON_4400F;
    motor.base_ydpi = 2400;
    motor.optical_ydpi = 9600;
    motor.slopes.push_back(MotorSlope::create_from_steps(3961, 240, 246));
    motor.slopes.push_back(MotorSlope::create_from_steps(3961, 240, 246));
    motor.slopes.push_back(MotorSlope::create_from_steps(3961, 240, 246));
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::CANON_8400F;
    motor.base_ydpi = 1600;
    motor.optical_ydpi = 6400;
    motor.slopes.push_back(MotorSlope::create_from_steps(3961, 240, 246));
    motor.slopes.push_back(MotorSlope::create_from_steps(3961, 240, 246));
    motor.slopes.push_back(MotorSlope::create_from_steps(3961, 240, 246));
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::CANON_8600F;
    motor.base_ydpi = 2400;
    motor.optical_ydpi = 9600;
    motor.slopes.push_back(MotorSlope::create_from_steps(3961, 240, 246));
    motor.slopes.push_back(MotorSlope::create_from_steps(3961, 240, 246));
    motor.slopes.push_back(MotorSlope::create_from_steps(3961, 240, 246));
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::CANON_LIDE_110;
    motor.base_ydpi = 4800;
    motor.optical_ydpi = 9600;
    motor.slopes.push_back(MotorSlope::create_from_steps(3000, 1000, 256));
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::CANON_LIDE_120;
    motor.base_ydpi = 4800;
    motor.optical_ydpi = 9600;
    motor.slopes.push_back(MotorSlope::create_from_steps(3000, 1000, 256));
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::CANON_LIDE_210;
    motor.base_ydpi = 4800;
    motor.optical_ydpi = 9600;
    motor.slopes.push_back(MotorSlope::create_from_steps(3000, 1000, 256));
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::PLUSTEK_OPTICPRO_3600;
    motor.base_ydpi = 1200;
    motor.optical_ydpi = 2400;
    motor.slopes.push_back(MotorSlope::create_from_steps(3500, 1300, 60));
    motor.slopes.push_back(MotorSlope::create_from_steps(3500, 3250, 60));
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::PLUSTEK_OPTICFILM_7200I;
    motor.base_ydpi = 3600;
    motor.optical_ydpi = 3600;
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::PLUSTEK_OPTICFILM_7300;
    motor.base_ydpi = 3600;
    motor.optical_ydpi = 3600;
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::PLUSTEK_OPTICFILM_7500I;
    motor.base_ydpi = 3600;
    motor.optical_ydpi = 3600;
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::IMG101;
    motor.base_ydpi = 600;
    motor.optical_ydpi = 1200;
    motor.slopes.push_back(MotorSlope::create_from_steps(3500, 1300, 60));
    motor.slopes.push_back(MotorSlope::create_from_steps(3500, 3250, 60));
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::PLUSTEK_OPTICBOOK_3800;
    motor.base_ydpi = 600;
    motor.optical_ydpi = 1200;
    motor.slopes.push_back(MotorSlope::create_from_steps(3500, 1300, 60));
    motor.slopes.push_back(MotorSlope::create_from_steps(3500, 3250, 60));
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::CANON_LIDE_80;
    motor.base_ydpi = 2400;
    motor.optical_ydpi = 4800; // 9600
    motor.slopes.push_back(MotorSlope::create_from_steps(9560, 1912, 31));
    s_motors->push_back(std::move(motor));
}

} // namespace genesys
