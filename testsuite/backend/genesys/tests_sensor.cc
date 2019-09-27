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

#include "../../../backend/genesys_low.h"

#include <numeric>

void test_fill_segmented_buffer_depth8()
{
    Genesys_Device dev;
    dev.settings.scan_mode = ScanColorMode::COLOR_SINGLE_PASS;
    dev.settings.dynamic_lineart = false;
    dev.settings.depth = 8;

    // 2 lines, 4 segments, 5 bytes each
    dev.deseg_curr_byte = 0;
    dev.session.output_segment_start_offset = 0;
    dev.session.output_segment_pixel_group_count = 5;
    dev.session.segment_count = 4;
    dev.session.conseq_pixel_dist_bytes = 5;
    dev.session.output_line_bytes_raw = 20;
    dev.segment_order = { 0, 2, 1, 3 };

    dev.oe_buffer.alloc(1024);
    uint8_t* data = dev.oe_buffer.get_write_pos(40);
    std::array<uint8_t, 41> input_data = { {
         1,  5,  9, 13, 17,
         3,  7, 11, 15, 19,
         2,  6, 10, 14, 18,
         4,  8, 12, 16, 20,
        21, 25, 29, 33, 37,
        23, 27, 31, 35, 39,
        22, 26, 30, 34, 38,
        24, 28, 32, 36, 40,
        0 // one extra byte so that we don't attempt to refill the buffer
    } };
    std::copy(input_data.begin(), input_data.end(), data);
    dev.oe_buffer.produce(41);

    std::vector<uint8_t> out_data;
    out_data.resize(40, 0);

    genesys_fill_segmented_buffer(&dev, out_data.data(), 16);
    ASSERT_EQ(dev.deseg_curr_byte, 4u);
    genesys_fill_segmented_buffer(&dev, out_data.data() + 16, 4);
    ASSERT_EQ(dev.deseg_curr_byte, 0u);
    genesys_fill_segmented_buffer(&dev, out_data.data() + 20, 20);
    ASSERT_EQ(dev.deseg_curr_byte, 0u);

    std::vector<uint8_t> expected;
    expected.resize(40, 0);
    std::iota(expected.begin(), expected.end(), 1); // will fill with 1, 2, 3, ..., 40

    ASSERT_EQ(out_data, expected);
}

void test_sensor()
{
    test_fill_segmented_buffer_depth8();
}
