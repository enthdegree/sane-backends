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
#include "enums.h"

namespace genesys {

/*  Describes a motor acceleration curve.

    The curves are described in two ways: legacy and physical modes.

        LEGACY mode

    The legacy mode is to be removed and described the motor slope as a purely mathematical
    power law formula:

    v(step) = sl.maximum_start_speed * (1 - pow(q, sl.g)) + sl.maximum_speed * pow(q, sl.g)     (1)

    where `q = step_number / s.minimum_steps`, `sl` is the slope config.


        PHYSICAL mode

    Definitions:
        v - speed in steps per pixeltime
        w - speed in pixel times per step. w = 1 / v
        a - acceleration in steps per pixeltime squared
        s - distance travelled in steps
        t - time in pixeltime

    The physical mode defines the curve in physical quantities. We asssume that the scanner head
    accelerates from standstill to the target speed uniformly. Then:

    v(t) = v(0) + a * t                                                                         (2)

    Where `a` is acceleration, `t` is time. Also we can calculate the travelled distance `s`:

    s(t) = v(0) * t + a * t^2 / 2                                                               (3)

    The actual motor slope is defined as the duration of each motor step. That means we need to
    define speed in terms of travelled distance.

    Solving (3) for `t` gives:

           sqrt( v(0)^2 + 2 * a * s ) - v(0)
    t(s) = ---------------------------------                                                    (4)
                          a

    Combining (4) and (2) will yield:

    v(s) = sqrt( v(0)^2 + 2 * a * s )                                                           (5)

    The data in the slope struct MotorSlope corresponds to the above in the following way:

    maximum_start_speed is `w(0) = 1/v(0)`

    maximum_speed is defines maximum speed which should not be exceeded

    minimum_steps is not used

    g is `a`

    Given the start and target speeds on a known motor curve, `a` can be computed as follows:

        v(t1)^2 - v(t0)^2
    a = -----------------                                                                       (6)
               2 * s

    Here `v(t0)` and `v(t1)` are the start and target speeds and `s` is the number of step required
    to reach the target speeds.
*/
struct MotorSlopeLegacy
{
    // maximum speed allowed when accelerating from standstill. Unit: pixeltime/step
    int maximum_start_speed = 0;
    // maximum speed allowed. Unit: pixeltime/step
    int maximum_speed = 0;
    // number of steps used for default curve
    int minimum_steps = 0;

    // power for non-linear acceleration curves.
    float g = 0;
};

struct MotorSlope
{
    // initial speed in pixeltime per step
    unsigned initial_speed_w = 0;

    // max speed in pixeltime per step
    unsigned max_speed_w = 0;

    // acceleration in steps per pixeltime squared.
    float acceleration = 0;
};

class Genesys_Motor_Slope
{
public:
    enum SlopeType : unsigned {
        LEGACY,
        PHYSICAL
    };

    Genesys_Motor_Slope(const MotorSlopeLegacy& slope) : legacy_slope_{slope}, type_{LEGACY} {}
    Genesys_Motor_Slope(const MotorSlope& slope) : slope_{slope}, type_{PHYSICAL} {}
    Genesys_Motor_Slope(const Genesys_Motor_Slope&) = default;
    Genesys_Motor_Slope& operator=(const Genesys_Motor_Slope&) = default;

    SlopeType type() const { return type_; }

    const MotorSlopeLegacy& legacy() const
    {
        if (type_ != LEGACY)
            throw SaneException("Unexpected slope type");
        return legacy_slope_;
    }

    const MotorSlope& physical() const
    {
        if (type_ != PHYSICAL)
            throw SaneException("Unexpected slope type");
        return slope_;
    }

private:
    MotorSlopeLegacy legacy_slope_;
    MotorSlope slope_;
    SlopeType type_;
};

std::ostream& operator<<(std::ostream& out, const Genesys_Motor_Slope& slope);


struct Genesys_Motor
{
    Genesys_Motor() = default;

    // id of the motor description
    MotorId id = MotorId::UNKNOWN;
    // motor base steps. Unit: 1/inch
    int base_ydpi = 0;
    // maximum resolution in y-direction. Unit: 1/inch
    int optical_ydpi = 0;
    // slopes to derive individual slopes from
    std::vector<Genesys_Motor_Slope> slopes;

    Genesys_Motor_Slope& get_slope(StepType step_type)
    {
        return slopes[static_cast<unsigned>(step_type)];
    }

    const Genesys_Motor_Slope& get_slope(StepType step_type) const
    {
        return slopes[static_cast<unsigned>(step_type)];
    }

    StepType max_step_type() const
    {
        if (slopes.empty()) {
            throw std::runtime_error("Slopes table is empty");
        }
        return static_cast<StepType>(slopes.size() - 1);
    }
};

std::ostream& operator<<(std::ostream& out, const Genesys_Motor& motor);

} // namespace genesys

#endif // BACKEND_GENESYS_MOTOR_H
