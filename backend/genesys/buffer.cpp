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

#include "buffer.h"
#include <cstring>
#include <stdexcept>

void Genesys_Buffer::alloc(std::size_t size)
{
    buffer_.resize(size);
    avail_ = 0;
    pos_ = 0;
}

void Genesys_Buffer::clear()
{
    buffer_.clear();
    avail_ = 0;
    pos_ = 0;
}

void Genesys_Buffer::reset()
{
    avail_ = 0;
    pos_ = 0;
}

std::uint8_t* Genesys_Buffer::get_write_pos(std::size_t size)
{
    if (avail_ + size > buffer_.size())
        return nullptr;
    if (pos_ + avail_ + size > buffer_.size())
    {
        std::memmove(buffer_.data(), buffer_.data() + pos_, avail_);
        pos_ = 0;
    }
    return buffer_.data() + pos_ + avail_;
}

std::uint8_t* Genesys_Buffer::get_read_pos()
{
    return buffer_.data() + pos_;
}

void Genesys_Buffer::produce(std::size_t size)
{
    if (size > buffer_.size() - avail_)
        throw std::runtime_error("buffer size exceeded");
    avail_ += size;
}

void Genesys_Buffer::consume(std::size_t size)
{
    if (size > avail_)
        throw std::runtime_error("no more data in buffer");
    avail_ -= size;
    pos_ += size;
}
