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

#define DEBUG_DECLARE_ONLY

#include "motor.h"
#include "utilities.h"
#include <cmath>

namespace genesys {

unsigned MotorSlope::get_table_step_shifted(unsigned step, StepType step_type) const
{
    // first two steps are always equal to the initial speed
    if (step < 2) {
        return initial_speed_w >> static_cast<unsigned>(step_type);
    }
    step--;

    float initial_speed_v = 1.0f / initial_speed_w;
    float speed_v = std::sqrt(initial_speed_v * initial_speed_v + 2 * acceleration * step);
    return static_cast<unsigned>(1.0f / speed_v) >> static_cast<unsigned>(step_type);
}

float compute_acceleration_for_steps(unsigned initial_w, unsigned max_w, unsigned steps)
{
    float initial_speed_v = 1.0f / static_cast<float>(initial_w);
    float max_speed_v = 1.0f / static_cast<float>(max_w);
    return (max_speed_v * max_speed_v - initial_speed_v * initial_speed_v) / (2 * steps);
}


MotorSlope MotorSlope::create_from_steps(unsigned initial_w, unsigned max_w,
                                         unsigned steps)
{
    MotorSlope slope;
    slope.initial_speed_w = initial_w;
    slope.max_speed_w = max_w;
    slope.acceleration = compute_acceleration_for_steps(initial_w, max_w, steps);
    return slope;
}

MotorSlopeTable create_slope_table(const MotorSlope& slope, unsigned target_speed_w,
                                   StepType step_type, unsigned steps_alignment,
                                   unsigned min_size)
{
    DBG_HELPER_ARGS(dbg, "target_speed_w: %d, step_type: %d, steps_alignment: %d, min_size: %d",
                    target_speed_w, static_cast<unsigned>(step_type), steps_alignment, min_size);
    MotorSlopeTable table;

    unsigned step_shift = static_cast<unsigned>(step_type);

    unsigned target_speed_shifted_w = target_speed_w >> step_shift;
    unsigned max_speed_shifted_w = slope.max_speed_w >> step_shift;

    if (target_speed_shifted_w < max_speed_shifted_w) {
        dbg.log(DBG_warn, "failed to reach target speed");
    }

    unsigned final_speed = std::max(target_speed_shifted_w, max_speed_shifted_w);

    table.table.reserve(MotorSlopeTable::SLOPE_TABLE_SIZE);

    while (true) {
        unsigned current = slope.get_table_step_shifted(table.table.size(), step_type);
        if (current <= final_speed) {
            break;
        }
        table.table.push_back(current);
        table.pixeltime_sum += current;
    }

    // make sure the target speed (or the max speed if target speed is too high) is present in
    // the table
    table.table.push_back(final_speed);
    table.pixeltime_sum += table.table.back();

    // fill the table up to the specified size
    while (table.table.size() % steps_alignment != 0 || table.table.size() < min_size) {
        table.table.push_back(table.table.back());
        table.pixeltime_sum += table.table.back();
    }

    table.scan_steps = table.table.size();

    // fill the rest of the table with the final speed
    table.table.resize(MotorSlopeTable::SLOPE_TABLE_SIZE, final_speed);

    return table;
}

std::ostream& operator<<(std::ostream& out, const MotorSlope& slope)
{
    out << "Genesys_Motor_Slope{\n"
        << "    initial_speed_w: " << slope.initial_speed_w << '\n'
        << "    max_speed_w: " << slope.max_speed_w << '\n'
        << "    a: " << slope.acceleration << '\n'
        << '}';
    return out;
}

std::ostream& operator<<(std::ostream& out, const MotorSlopeLegacy& slope)
{
    out << "MotorSlopeLegacy{\n"
        << "    maximum_start_speed: " << slope.maximum_start_speed << '\n'
        << "    maximum_speed: " << slope.maximum_speed << '\n'
        << "    minimum_steps: " << slope.minimum_steps << '\n'
        << "    g: " << slope.g << '\n'
        << '}';
    return out;
}

std::ostream& operator<<(std::ostream& out, const Genesys_Motor_Slope& slope)
{
    if (slope.type() == Genesys_Motor_Slope::LEGACY) {
        out << slope.legacy();
    } else {
        out << slope.physical();
    }
    return out;
}

std::ostream& operator<<(std::ostream& out, const Genesys_Motor& motor)
{
    out << "Genesys_Motor{\n"
        << "    id: " << static_cast<unsigned>(motor.id) << '\n'
        << "    base_ydpi: " << motor.base_ydpi << '\n'
        << "    optical_ydpi: " << motor.optical_ydpi << '\n'
        << "    slopes: "
        << format_indent_braced_list(4, format_vector_indent_braced(4, "Genesys_Motor_Slope",
                                                                    motor.slopes))
        << '}';
    return out;
}

} // namespace genesys
