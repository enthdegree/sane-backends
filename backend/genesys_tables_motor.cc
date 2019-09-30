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

#include "genesys_low.h"

StaticInit<std::vector<Genesys_Motor>> s_motors;

void genesys_init_motor_tables()
{
    s_motors.init();

    Genesys_Motor_Slope slope;

    Genesys_Motor motor;
    motor.id = MotorId::UMAX;
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
    motor.id = MotorId::MD_5345; // MD5345/6228/6471
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
    motor.id = MotorId::ST24;
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
    motor.id = MotorId::HP3670;
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
    motor.id = MotorId::HP2400;
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
    motor.id = MotorId::HP2300;
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
    motor.id = MotorId::CANONLIDE35;
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
    motor.id = MotorId::XP200;
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
    motor.id = MotorId::XP300;
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
    motor.id = MotorId::DP665;
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
    motor.id = MotorId::ROADWARRIOR;
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
    motor.id = MotorId::DSMOBILE_600;
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
    motor.id = MotorId::CANONLIDE100;
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
    motor.id = MotorId::CANONLIDE200;
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
    motor.id = MotorId::CANONLIDE700;
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
    motor.id = MotorId::KVSS080;
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
    motor.id = MotorId::G4050;
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
    motor.id = MotorId::CS8400F;
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
    motor.id = MotorId::CS8600F;
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
    motor.id = MotorId::CANONLIDE110;
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
    motor.id = MotorId::CANONLIDE120;
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
    motor.id = MotorId::CANONLIDE210;
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
    motor.id = MotorId::PLUSTEK_OPTICPRO_3600;
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
    motor.id = MotorId::PLUSTEK_OPTICFILM_7200I;
    motor.base_ydpi = 3600;
    motor.optical_ydpi = 3600;
    motor.max_step_type = 0; // only used on GL841
    s_motors->push_back(std::move(motor));


    motor = Genesys_Motor();
    motor.id = MotorId::IMG101;
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
    motor.id = MotorId::PLUSTEK_OPTICBOOK_3800;
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
    motor.id = MotorId::CANONLIDE80;
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
