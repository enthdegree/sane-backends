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

#ifndef BACKEND_GENESYS_BUFFER_H
#define BACKEND_GENESYS_BUFFER_H

#include <vector>
#include <cstddef>
#include <cstdint>

namespace genesys {

/*  A FIFO buffer. Note, that this is _not_ a ringbuffer.
    if we need a block which does not fit at the end of our available data,
    we move the available data to the beginning.
*/
struct Genesys_Buffer
{
    Genesys_Buffer() = default;

    std::size_t size() const { return buffer_.size(); }
    std::size_t avail() const { return avail_; }
    std::size_t pos() const { return pos_; }

    // TODO: refactor code that uses this function to no longer use it
    void set_pos(std::size_t pos) { pos_ = pos; }

    void alloc(std::size_t size);
    void clear();

    void reset();

    std::uint8_t* get_write_pos(std::size_t size);
    std::uint8_t* get_read_pos(); // TODO: mark as const

    void produce(std::size_t size);
    void consume(std::size_t size);

private:
    std::vector<std::uint8_t> buffer_;
    // current position in read buffer
    std::size_t pos_ = 0;
    // data bytes currently in buffer
    std::size_t avail_ = 0;
};

} // namespace genesys

#endif // BACKEND_GENESYS_BUFFER_H
