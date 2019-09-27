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

#ifndef BACKEND_GENESYS_IMAGE_BUFFER_H
#define BACKEND_GENESYS_IMAGE_BUFFER_H

#include "genesys_enums.h"
#include "genesys_row_buffer.h"
#include <algorithm>
#include <functional>

struct Pixel
{
    Pixel() = default;
    Pixel(std::uint16_t red, std::uint16_t green, std::uint16_t blue) :
        r{red}, g{green}, b{blue} {}

    std::uint16_t r = 0;
    std::uint16_t g = 0;
    std::uint16_t b = 0;

    bool operator==(const Pixel& other) const
    {
        return r == other.r && g == other.g && b == other.b;
    }
};

struct RawPixel
{
    RawPixel() = default;
    RawPixel(std::uint8_t d0) : data{d0, 0, 0, 0, 0, 0} {}
    RawPixel(std::uint8_t d0, std::uint8_t d1) : data{d0, d1, 0, 0, 0, 0} {}
    RawPixel(std::uint8_t d0, std::uint8_t d1, std::uint8_t d2) : data{d0, d1, d2, 0, 0, 0} {}
    RawPixel(std::uint8_t d0, std::uint8_t d1, std::uint8_t d2,
             std::uint8_t d3, std::uint8_t d4, std::uint8_t d5) : data{d0, d1, d2, d3, d4, d5} {}
    std::uint8_t data[6] = {};

    bool operator==(const RawPixel& other) const
    {
        return std::equal(std::begin(data), std::end(data),
                          std::begin(other.data), std::end(other.data));
    }
};

// This class allows reading from row-based source in smaller or larger chunks of data
class ImageBuffer
{
public:
    using ProducerCallback = std::function<void(std::size_t size, std::uint8_t* out_data)>;

    ImageBuffer() {}
    ImageBuffer(std::size_t size, ProducerCallback producer);

    std::size_t size() const { return size_; }
    std::size_t available() const { return size_ - buffer_offset_; }

    void get_data(std::size_t size, std::uint8_t* out_data);

private:
    ProducerCallback producer_;
    std::size_t size_ = 0;

    std::size_t buffer_offset_ = 0;
    std::vector<std::uint8_t> buffer_;
};

class FakeBufferModel
{
public:
    FakeBufferModel() {}

    void push_step(std::size_t buffer_size, std::size_t row_bytes);

    std::size_t available_space() const;

    void simulate_read(std::size_t size);

private:
    std::vector<std::size_t> sizes_;
    std::vector<std::size_t> available_sizes_;
    std::vector<std::size_t> row_bytes_;
};

// This class is similar to ImageBuffer, but preserves historical peculiarities of buffer handling
// in the backend to preserve exact behavior
class ImageBufferGenesysUsb
{
public:
    using ProducerCallback = std::function<void(std::size_t size, std::uint8_t* out_data)>;

    ImageBufferGenesysUsb() {}
    ImageBufferGenesysUsb(std::size_t total_size, const FakeBufferModel& buffer_model,
                          ProducerCallback producer);

    std::size_t remaining_size() const { return remaining_size_; }

    std::size_t available() const { return buffer_end_ - buffer_offset_; }

    void get_data(std::size_t size, std::uint8_t* out_data);

private:

    std::size_t get_read_size();

    std::size_t remaining_size_ = 0;

    std::size_t buffer_offset_ = 0;
    std::size_t buffer_end_ = 0;
    std::vector<std::uint8_t> buffer_;

    FakeBufferModel buffer_model_;

    ProducerCallback producer_;
};

#endif // BACKEND_GENESYS_IMAGE_BUFFER_H
