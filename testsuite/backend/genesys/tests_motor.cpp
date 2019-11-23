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
*/

#define DEBUG_DECLARE_ONLY

#include "tests.h"
#include "minigtest.h"
#include "tests_printers.h"

#include "../../../backend/genesys/low.h"
#include "../../../backend/genesys/enums.h"

namespace genesys {

void test_create_slope_table()
{
    MotorSlopeLegacy slope;

    Genesys_Motor motor;
    motor.id = MotorId::CANON_LIDE_200;
    motor.base_ydpi = 1200;
    motor.optical_ydpi = 6400;

    slope = MotorSlopeLegacy(); // full step
    slope.maximum_start_speed = 10000;
    slope.maximum_speed = 1000;
    slope.minimum_steps = 20;
    slope.g = 0.80;
    motor.slopes.push_back(slope);

    slope = MotorSlopeLegacy(); // half step
    slope.maximum_start_speed = 10000;
    slope.maximum_speed = 1000;
    slope.minimum_steps = 20;
    slope.g = 0.80;
    motor.slopes.push_back(slope);

    slope = MotorSlopeLegacy(); // quarter step 0.75*2712
    slope.maximum_start_speed = 10000;
    slope.maximum_speed = 1000;
    slope.minimum_steps = 16;
    slope.g = 0.80f;
    motor.slopes.push_back(slope);

    unsigned used_steps = 0;
    unsigned final_exposure = 0;
    unsigned sum_time = 0;
    std::vector<std::uint16_t> slope_table;

    sum_time = sanei_genesys_create_slope_table3(motor, slope_table, 20, 20, StepType::FULL,
                                                 10000, motor.base_ydpi, &used_steps,
                                                 &final_exposure);

    ASSERT_EQ(sum_time, 10000u);
    ASSERT_EQ(used_steps, 1u);
    ASSERT_EQ(final_exposure, 10000u);

    std::vector<std::uint16_t> expected_steps = {
        10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000,
        10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000,
    };

    ASSERT_EQ(slope_table, expected_steps);

    sum_time = sanei_genesys_create_slope_table3(motor, slope_table, 20, 20, StepType::FULL,
                                                 2000, motor.base_ydpi, &used_steps,
                                                 &final_exposure);

    ASSERT_EQ(sum_time, 98183u);
    ASSERT_EQ(used_steps, 18u);
    ASSERT_EQ(final_exposure, 1766u);

    expected_steps = {
        10000, 9146, 8513, 7944, 7412, 6906, 6421, 5951, 5494, 5049,
        4614, 4187, 3768, 3356, 2950, 2550, 2156, 1766, 1766, 1766,
    };

    ASSERT_EQ(slope_table, expected_steps);

    sum_time = sanei_genesys_create_slope_table3(motor, slope_table, 20, 20, StepType::HALF,
                                                 10000, motor.base_ydpi, &used_steps,
                                                 &final_exposure);

    ASSERT_EQ(sum_time, 5000u);
    ASSERT_EQ(used_steps, 1u);
    ASSERT_EQ(final_exposure, 5000u);

    expected_steps = {
        5000, 5000, 5000, 5000, 5000, 5000, 5000, 5000, 5000, 5000,
        5000, 5000, 5000, 5000, 5000, 5000, 5000, 5000, 5000, 5000,
    };

    ASSERT_EQ(slope_table, expected_steps);

    sum_time = sanei_genesys_create_slope_table3(motor, slope_table, 20, 20, StepType::HALF,
                                                 2000, motor.base_ydpi, &used_steps,
                                                 &final_exposure);

    ASSERT_EQ(sum_time, 72131u);
    ASSERT_EQ(used_steps, 20u);
    ASSERT_EQ(final_exposure, 2575u);

    expected_steps = {
        5000, 4759, 4581, 4421, 4272, 4129, 3993, 3861, 3732, 3607,
        3485, 3365, 3247, 3131, 3017, 2904, 2793, 2684, 2575, 2575,
    };

    ASSERT_EQ(slope_table, expected_steps);

    sum_time = sanei_genesys_create_slope_table3(motor, slope_table, 20, 20, StepType::QUARTER,
                                                 10000, motor.base_ydpi, &used_steps,
                                                 &final_exposure);

    ASSERT_EQ(sum_time, 2500u);
    ASSERT_EQ(used_steps, 1u);
    ASSERT_EQ(final_exposure, 2500u);

    expected_steps = {
        2500, 2500, 2500, 2500, 2500, 2500, 2500, 2500, 2500, 2500,
        2500, 2500, 2500, 2500, 2500, 2500, 2500, 2500, 2500, 2500,
    };

    ASSERT_EQ(slope_table, expected_steps);

    sum_time = sanei_genesys_create_slope_table3(motor, slope_table, 20, 20, StepType::QUARTER,
                                                 2000, motor.base_ydpi, &used_steps,
                                                 &final_exposure);

    ASSERT_EQ(sum_time, 40503u);
    ASSERT_EQ(used_steps, 20u);
    ASSERT_EQ(final_exposure, 1674u);

    expected_steps = {
        2500, 2418, 2357, 2303, 2252, 2203, 2157, 2112, 2068, 2025,
        1983, 1943, 1902, 1863, 1824, 1786, 1748, 1711, 1674, 1674,
    };

    ASSERT_EQ(slope_table, expected_steps);
}

void test_motor()
{
    test_create_slope_table();
}

} // namespace genesys
