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

#include "test_scanner_interface.h"
#include <cstring>

namespace genesys {

TestScannerInterface::~TestScannerInterface() = default;

bool TestScannerInterface::is_mock() const
{
    return true;
}

std::uint8_t TestScannerInterface::read_register(std::uint16_t address)
{
    return cached_regs_.get(address);
}

void TestScannerInterface::write_register(std::uint16_t address, std::uint8_t value)
{
    cached_regs_.update(address, value);
}

void TestScannerInterface::write_registers(const Genesys_Register_Set& regs)
{
    cached_regs_.update(regs);
}


void TestScannerInterface::write_0x8c(std::uint8_t index, std::uint8_t value)
{
    (void) index;
    (void) value;
}

void TestScannerInterface::bulk_read_data(std::uint8_t addr, std::uint8_t* data, std::size_t size)
{
    (void) addr;
    std::memset(data, 0, size);
}

void TestScannerInterface::bulk_write_data(std::uint8_t addr, std::uint8_t* data, std::size_t size)
{
    (void) addr;
    (void) data;
    (void) size;
}

void TestScannerInterface::write_buffer(std::uint8_t type, std::uint32_t addr, std::uint8_t* data,
                                        std::size_t size, Flags flags)
{
    (void) type;
    (void) addr;
    (void) data;
    (void) size;
    (void) flags;
}

void TestScannerInterface::write_gamma(std::uint8_t type, std::uint32_t addr, std::uint8_t* data,
                                       std::size_t size, Flags flags)
{
    (void) type;
    (void) addr;
    (void) data;
    (void) size;
    (void) flags;
}

void TestScannerInterface::write_ahb(std::uint32_t addr, std::uint32_t size, std::uint8_t* data)
{
    (void) addr;
    (void) size;
    (void) data;
}

std::uint16_t TestScannerInterface::read_fe_register(std::uint8_t address)
{
    return cached_fe_regs_.get(address);
}

void TestScannerInterface::write_fe_register(std::uint8_t address, std::uint16_t value)
{
    cached_fe_regs_.update(address, value);
}

IUsbDevice& TestScannerInterface::get_usb_device()
{
    return usb_dev_;
}

void TestScannerInterface::record_test_message(const char* msg)
{
    last_test_message_ = msg;
}

const std::string& TestScannerInterface::last_test_message() const
{
    return last_test_message_;
}

} // namespace genesys
