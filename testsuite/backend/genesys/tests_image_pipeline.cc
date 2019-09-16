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

#include "../../../backend/genesys_image_pipeline.h"

#include <numeric>

void test_image_buffer_genesys_usb()
{
    std::vector<std::size_t> requests;

    auto on_read_usb = [&](std::size_t x, std::uint8_t* data)
    {
        (void) data;
        requests.push_back(x);
    };

    FakeBufferModel model;
    model.push_step(453120, 1);
    model.push_step(56640, 3540);
    ImageBufferGenesysUsb buffer{1086780, model, on_read_usb};

    std::vector<std::uint8_t> dummy;
    dummy.resize(1086780);

    ASSERT_TRUE(buffer.get_data(453120, dummy.data()));
    ASSERT_TRUE(buffer.get_data(56640, dummy.data()));
    ASSERT_TRUE(buffer.get_data(56640, dummy.data()));
    ASSERT_TRUE(buffer.get_data(56640, dummy.data()));
    ASSERT_TRUE(buffer.get_data(56640, dummy.data()));
    ASSERT_TRUE(buffer.get_data(56640, dummy.data()));
    ASSERT_TRUE(buffer.get_data(56640, dummy.data()));
    ASSERT_TRUE(buffer.get_data(56640, dummy.data()));
    ASSERT_TRUE(buffer.get_data(56640, dummy.data()));
    ASSERT_TRUE(buffer.get_data(56640, dummy.data()));
    ASSERT_TRUE(buffer.get_data(56640, dummy.data()));
    ASSERT_TRUE(buffer.get_data(56640, dummy.data()));

    std::vector<std::size_t> expected = {
        453120, 56576, 56576, 56576, 56832, 56576, 56576, 56576, 56832, 56576, 56576, 56576, 11008
    };
    ASSERT_EQ(requests, expected);
}

void test_image_buffer_genesys_usb_capped_remaining_bytes()
{
    std::vector<std::size_t> requests;

    auto on_read_usb = [&](std::size_t x, std::uint8_t* data)
    {
        (void) data;
        requests.push_back(x);
    };

    FakeBufferModel model;
    model.push_step(453120, 1);
    model.push_step(56640, 3540);
    ImageBufferGenesysUsb buffer{1086780, model, on_read_usb};

    std::vector<std::uint8_t> dummy;
    dummy.resize(1086780);

    ASSERT_TRUE(buffer.get_data(453120, dummy.data()));
    ASSERT_TRUE(buffer.get_data(56640, dummy.data()));
    ASSERT_TRUE(buffer.get_data(56640, dummy.data()));
    ASSERT_TRUE(buffer.get_data(56640, dummy.data()));
    ASSERT_TRUE(buffer.get_data(56640, dummy.data()));
    buffer.set_remaining_size(10000);
    ASSERT_FALSE(buffer.get_data(56640, dummy.data()));

    std::vector<std::size_t> expected = {
        // note that the sizes are rounded-up to 256 bytes
        453120, 56576, 56576, 56576, 56832, 10240
    };
    ASSERT_EQ(requests, expected);
}

void test_node_buffered_callable_source()
{
    using Data = std::vector<std::uint8_t>;

    Data in_data = {
        0, 1, 2, 3,
        4, 5, 6, 7,
        8, 9, 10, 11
    };

    std::size_t chunk_size = 3;
    std::size_t curr_index = 0;

    auto data_source_cb = [&](std::size_t size, std::uint8_t* out_data)
    {
        ASSERT_EQ(size, chunk_size);
        std::copy(in_data.begin() + curr_index,
                  in_data.begin() + curr_index + chunk_size, out_data);
        curr_index += chunk_size;
        return true;
    };

    ImagePipelineStack stack;
    stack.push_first_node<ImagePipelineNodeBufferedCallableSource>(4, 3, PixelFormat::I8,
                                                                   chunk_size, data_source_cb);

    Data out_data;
    out_data.resize(4);

    ASSERT_EQ(curr_index, 0u);

    ASSERT_TRUE(stack.get_next_row_data(out_data.data()));
    ASSERT_EQ(out_data, Data({0, 1, 2, 3}));
    ASSERT_EQ(curr_index, 6u);

    ASSERT_TRUE(stack.get_next_row_data(out_data.data()));
    ASSERT_EQ(out_data, Data({4, 5, 6, 7}));
    ASSERT_EQ(curr_index, 9u);

    ASSERT_TRUE(stack.get_next_row_data(out_data.data()));
    ASSERT_EQ(out_data, Data({8, 9, 10, 11}));
    ASSERT_EQ(curr_index, 12u);
}

void test_node_format_convert()
{
    using Data = std::vector<std::uint8_t>;

    Data in_data = {
        0x12, 0x34, 0x56,
        0x78, 0x98, 0xab,
        0xcd, 0xef, 0x21,
    };

    ImagePipelineStack stack;
    stack.push_first_node<ImagePipelineNodeArraySource>(3, 1, PixelFormat::RGB888,
                                                        std::move(in_data));
    stack.push_node<ImagePipelineNodeFormatConvert>(PixelFormat::BGR161616);

    ASSERT_EQ(stack.get_output_width(), 3u);
    ASSERT_EQ(stack.get_output_height(), 1u);
    ASSERT_EQ(stack.get_output_row_bytes(), 6u * 3);
    ASSERT_EQ(stack.get_output_format(), PixelFormat::BGR161616);

    auto out_data = stack.get_all_data();

    Data expected_data = {
        0x56, 0x56, 0x34, 0x34, 0x12, 0x12,
        0xab, 0xab, 0x98, 0x98, 0x78, 0x78,
        0x21, 0x21, 0xef, 0xef, 0xcd, 0xcd,
    };

    ASSERT_EQ(out_data, expected_data);
}

void test_node_desegment_1_line()
{
    using Data = std::vector<std::uint8_t>;

    Data in_data = {
         1,  5,  9, 13, 17,
         3,  7, 11, 15, 19,
         2,  6, 10, 14, 18,
         4,  8, 12, 16, 20,
        21, 25, 29, 33, 37,
        23, 27, 31, 35, 39,
        22, 26, 30, 34, 38,
        24, 28, 32, 36, 40,
    };

    ImagePipelineStack stack;
    stack.push_first_node<ImagePipelineNodeArraySource>(20, 2, PixelFormat::I8,
                                                        std::move(in_data));
    stack.push_node<ImagePipelineNodeDesegment>(20, std::vector<unsigned>{ 0, 2, 1, 3 }, 5, 1, 1);

    ASSERT_EQ(stack.get_output_width(), 20u);
    ASSERT_EQ(stack.get_output_height(), 2u);
    ASSERT_EQ(stack.get_output_row_bytes(), 20u);
    ASSERT_EQ(stack.get_output_format(), PixelFormat::I8);

    auto out_data = stack.get_all_data();

    Data expected_data;
    expected_data.resize(40, 0);
    std::iota(expected_data.begin(), expected_data.end(), 1); // will fill with 1, 2, 3, ..., 40

    ASSERT_EQ(out_data, expected_data);
}

void test_node_deinterleave_lines_i8()
{
    using Data = std::vector<std::uint8_t>;

    Data in_data = {
        1, 3, 5, 7,  9, 11, 13, 15, 17, 19,
        2, 4, 6, 8, 10, 12, 14, 16, 18, 20,
    };

    ImagePipelineStack stack;
    stack.push_first_node<ImagePipelineNodeArraySource>(10, 2, PixelFormat::I8,
                                                        std::move(in_data));
    stack.push_node<ImagePipelineNodeDeinterleaveLines>(2, 1);

    ASSERT_EQ(stack.get_output_width(), 20u);
    ASSERT_EQ(stack.get_output_height(), 1u);
    ASSERT_EQ(stack.get_output_row_bytes(), 20u);
    ASSERT_EQ(stack.get_output_format(), PixelFormat::I8);

    auto out_data = stack.get_all_data();

    Data expected_data;
    expected_data.resize(20, 0);
    std::iota(expected_data.begin(), expected_data.end(), 1); // will fill with 1, 2, 3, ..., 20

    ASSERT_EQ(out_data, expected_data);
}

void test_node_deinterleave_lines_rgb888()
{
    using Data = std::vector<std::uint8_t>;

    Data in_data = {
        1, 2, 3,  7,  8,  9, 13, 14, 15, 19, 20, 21,
        4, 5, 6, 10, 11, 12, 16, 17, 18, 22, 23, 24,
    };

    ImagePipelineStack stack;
    stack.push_first_node<ImagePipelineNodeArraySource>(4, 2, PixelFormat::RGB888,
                                                        std::move(in_data));
    stack.push_node<ImagePipelineNodeDeinterleaveLines>(2, 1);

    ASSERT_EQ(stack.get_output_width(), 8u);
    ASSERT_EQ(stack.get_output_height(), 1u);
    ASSERT_EQ(stack.get_output_row_bytes(), 24u);
    ASSERT_EQ(stack.get_output_format(), PixelFormat::RGB888);

    auto out_data = stack.get_all_data();

    Data expected_data;
    expected_data.resize(24, 0);
    std::iota(expected_data.begin(), expected_data.end(), 1); // will fill with 1, 2, 3, ..., 20

    ASSERT_EQ(out_data, expected_data);
}

void test_node_swap_16bit_endian()
{
    using Data = std::vector<std::uint8_t>;

    Data in_data = {
        0x10, 0x20, 0x30, 0x11, 0x21, 0x31,
        0x12, 0x22, 0x32, 0x13, 0x23, 0x33,
        0x14, 0x24, 0x34, 0x15, 0x25, 0x35,
        0x16, 0x26, 0x36, 0x17, 0x27, 0x37,
    };

    ImagePipelineStack stack;
    stack.push_first_node<ImagePipelineNodeArraySource>(4, 1, PixelFormat::RGB161616,
                                                        std::move(in_data));
    stack.push_node<ImagePipelineNodeSwap16BitEndian>();

    ASSERT_EQ(stack.get_output_width(), 4u);
    ASSERT_EQ(stack.get_output_height(), 1u);
    ASSERT_EQ(stack.get_output_row_bytes(), 24u);
    ASSERT_EQ(stack.get_output_format(), PixelFormat::RGB161616);

    auto out_data = stack.get_all_data();

    Data expected_data = {
        0x20, 0x10, 0x11, 0x30, 0x31, 0x21,
        0x22, 0x12, 0x13, 0x32, 0x33, 0x23,
        0x24, 0x14, 0x15, 0x34, 0x35, 0x25,
        0x26, 0x16, 0x17, 0x36, 0x37, 0x27,
    };

    ASSERT_EQ(out_data, expected_data);
}

void test_node_merge_mono_lines()
{
    using Data = std::vector<std::uint8_t>;

    Data in_data = {
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
    };

    ImagePipelineStack stack;
    stack.push_first_node<ImagePipelineNodeArraySource>(8, 3, PixelFormat::I8,
                                                        std::move(in_data));
    stack.push_node<ImagePipelineNodeMergeMonoLines>(ColorOrder::RGB);

    ASSERT_EQ(stack.get_output_width(), 8u);
    ASSERT_EQ(stack.get_output_height(), 1u);
    ASSERT_EQ(stack.get_output_row_bytes(), 24u);
    ASSERT_EQ(stack.get_output_format(), PixelFormat::RGB888);

    auto out_data = stack.get_all_data();

    Data expected_data = {
        0x10, 0x20, 0x30, 0x11, 0x21, 0x31,
        0x12, 0x22, 0x32, 0x13, 0x23, 0x33,
        0x14, 0x24, 0x34, 0x15, 0x25, 0x35,
        0x16, 0x26, 0x36, 0x17, 0x27, 0x37,
    };

    ASSERT_EQ(out_data, expected_data);
}

void test_node_split_mono_lines()
{
    using Data = std::vector<std::uint8_t>;

    Data in_data = {
        0x10, 0x20, 0x30, 0x11, 0x21, 0x31,
        0x12, 0x22, 0x32, 0x13, 0x23, 0x33,
        0x14, 0x24, 0x34, 0x15, 0x25, 0x35,
        0x16, 0x26, 0x36, 0x17, 0x27, 0x37,
    };

    ImagePipelineStack stack;
    stack.push_first_node<ImagePipelineNodeArraySource>(8, 1, PixelFormat::RGB888,
                                                        std::move(in_data));
    stack.push_node<ImagePipelineNodeSplitMonoLines>();

    ASSERT_EQ(stack.get_output_width(), 8u);
    ASSERT_EQ(stack.get_output_height(), 3u);
    ASSERT_EQ(stack.get_output_row_bytes(), 8u);
    ASSERT_EQ(stack.get_output_format(), PixelFormat::I8);

    auto out_data = stack.get_all_data();

    Data expected_data = {
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
    };

    ASSERT_EQ(out_data, expected_data);
}

void test_node_component_shift_lines()
{
    using Data = std::vector<std::uint8_t>;

    Data in_data = {
        0x10, 0x20, 0x30, 0x11, 0x21, 0x31, 0x12, 0x22, 0x32, 0x13, 0x23, 0x33,
        0x14, 0x24, 0x34, 0x15, 0x25, 0x35, 0x16, 0x26, 0x36, 0x17, 0x27, 0x37,
        0x18, 0x28, 0x38, 0x19, 0x29, 0x39, 0x1a, 0x2a, 0x3a, 0x1b, 0x2b, 0x3b,
        0x1c, 0x2c, 0x3c, 0x1d, 0x2d, 0x3d, 0x1e, 0x2e, 0x3e, 0x1f, 0x2f, 0x3f,
    };

    ImagePipelineStack stack;
    stack.push_first_node<ImagePipelineNodeArraySource>(4, 4, PixelFormat::RGB888,
                                                        std::move(in_data));
    stack.push_node<ImagePipelineNodeComponentShiftLines>(0, 1, 2);

    ASSERT_EQ(stack.get_output_width(), 4u);
    ASSERT_EQ(stack.get_output_height(), 2u);
    ASSERT_EQ(stack.get_output_row_bytes(), 12u);
    ASSERT_EQ(stack.get_output_format(), PixelFormat::RGB888);

    auto out_data = stack.get_all_data();

    Data expected_data = {
        0x10, 0x24, 0x38, 0x11, 0x25, 0x39, 0x12, 0x26, 0x3a, 0x13, 0x27, 0x3b,
        0x14, 0x28, 0x3c, 0x15, 0x29, 0x3d, 0x16, 0x2a, 0x3e, 0x17, 0x2b, 0x3f,
    };

    ASSERT_EQ(out_data, expected_data);
}

void test_node_pixel_shift_lines()
{
    using Data = std::vector<std::uint8_t>;

    Data in_data = {
        0x10, 0x20, 0x30, 0x11, 0x21, 0x31, 0x12, 0x22, 0x32, 0x13, 0x23, 0x33,
        0x14, 0x24, 0x34, 0x15, 0x25, 0x35, 0x16, 0x26, 0x36, 0x17, 0x27, 0x37,
        0x18, 0x28, 0x38, 0x19, 0x29, 0x39, 0x1a, 0x2a, 0x3a, 0x1b, 0x2b, 0x3b,
        0x1c, 0x2c, 0x3c, 0x1d, 0x2d, 0x3d, 0x1e, 0x2e, 0x3e, 0x1f, 0x2f, 0x3f,
    };

    ImagePipelineStack stack;
    stack.push_first_node<ImagePipelineNodeArraySource>(4, 4, PixelFormat::RGB888,
                                                        std::move(in_data));
    stack.push_node<ImagePipelineNodePixelShiftLines>(std::vector<std::size_t>{0, 2});

    ASSERT_EQ(stack.get_output_width(), 4u);
    ASSERT_EQ(stack.get_output_height(), 2u);
    ASSERT_EQ(stack.get_output_row_bytes(), 12u);
    ASSERT_EQ(stack.get_output_format(), PixelFormat::RGB888);

    auto out_data = stack.get_all_data();

    Data expected_data = {
        0x10, 0x20, 0x30, 0x19, 0x29, 0x39, 0x12, 0x22, 0x32, 0x1b, 0x2b, 0x3b,
        0x14, 0x24, 0x34, 0x1d, 0x2d, 0x3d, 0x16, 0x26, 0x36, 0x1f, 0x2f, 0x3f,
    };

    ASSERT_EQ(out_data, expected_data);
}

void test_image_pipeline()
{
    test_image_buffer_genesys_usb();
    test_image_buffer_genesys_usb_capped_remaining_bytes();
    test_node_buffered_callable_source();
    test_node_format_convert();
    test_node_desegment_1_line();
    test_node_deinterleave_lines_i8();
    test_node_deinterleave_lines_rgb888();
    test_node_swap_16bit_endian();
    test_node_merge_mono_lines();
    test_node_split_mono_lines();
    test_node_component_shift_lines();
    test_node_pixel_shift_lines();
}
