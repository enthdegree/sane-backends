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

#ifndef BACKEND_GENESYS_MOTOR_H
#define BACKEND_GENESYS_MOTOR_H

#include <cstdint>
#include <vector>
#include "genesys_enums.h"

struct Genesys_Motor_Slope
{
    Genesys_Motor_Slope() = default;
    Genesys_Motor_Slope(int p_maximum_start_speed, int p_maximum_speed, int p_minimum_steps,
                        float p_g) :
        maximum_start_speed(p_maximum_start_speed),
        maximum_speed(p_maximum_speed),
        minimum_steps(p_minimum_steps),
        g(p_g)
    {}

    // maximum speed allowed when accelerating from standstill. Unit: pixeltime/step
    int maximum_start_speed = 0;
    // maximum speed allowed. Unit: pixeltime/step
    int maximum_speed = 0;
    // number of steps used for default curve
    int minimum_steps = 0;

    /*  power for non-linear acceleration curves.
        vs*(1-i^g)+ve*(i^g) where
        vs = start speed, ve = end speed,
        i = 0.0 for first entry and i = 1.0 for last entry in default table
    */
    float g = 0;

    /* start speed, max end speed, step number */
    /* maximum speed (second field) is used to compute exposure as seen by motor */
    /* exposure=max speed/ slope dpi * base dpi */
    /* 5144 = max pixels at 600 dpi */
    /* 1288=(5144+8)*ydpi(=300)/base_dpi(=1200) , where 5152 is exposure */
    /* 6440=9660/(1932/1288) */
    // {  9560,  1912, 31, 0.8 },
};


struct Genesys_Motor
{
    Genesys_Motor() = default;

    // id of the motor description
    MotorId id = MotorId::UNKNOWN;
    // motor base steps. Unit: 1/inch
    int base_ydpi = 0;
    // maximum resolution in y-direction. Unit: 1/inch
    int optical_ydpi = 0;
    // maximum step type. 0-2
    int max_step_type = 0;
    // slopes to derive individual slopes from
    std::vector<Genesys_Motor_Slope> slopes;
};

#endif // BACKEND_GENESYS_MOTOR_H
