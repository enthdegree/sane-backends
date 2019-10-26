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

#include "test_usb_device.h"
#include "low.h"

namespace genesys {

TestUsbDevice::TestUsbDevice(std::uint16_t vendor, std::uint16_t product) :
    vendor_{vendor},
    product_{product}
{
}

TestUsbDevice::~TestUsbDevice()
{
    if (is_open()) {
        DBG(DBG_error, "TestUsbDevice not closed; closing automatically");
        close();
    }
}

void TestUsbDevice::open(const char* dev_name)
{
    DBG_HELPER(dbg);

    if (is_open()) {
        throw SaneException("device already open");
    }
    name_ = dev_name;
    is_open_ = true;
}

void TestUsbDevice::clear_halt()
{
    DBG_HELPER(dbg);
    assert_is_open();
}

void TestUsbDevice::reset()
{
    DBG_HELPER(dbg);
    assert_is_open();
}

void TestUsbDevice::close()
{
    DBG_HELPER(dbg);
    assert_is_open();

    is_open_ = false;
    name_ = "";
}

void TestUsbDevice::get_vendor_product(int& vendor, int& product)
{
    DBG_HELPER(dbg);
    assert_is_open();
    vendor = vendor_;
    product = product_;
}

void TestUsbDevice::control_msg(int rtype, int reg, int value, int index, int length,
                                std::uint8_t* data)
{
    (void) reg;
    (void) value;
    (void) index;
    DBG_HELPER(dbg);
    assert_is_open();
    if (rtype == REQUEST_TYPE_IN) {
        std::memset(data, 0, length);
    }
}

void TestUsbDevice::bulk_read(std::uint8_t* buffer, std::size_t* size)
{

    DBG_HELPER(dbg);
    assert_is_open();
    std::memset(buffer, 0, *size);
}

void TestUsbDevice::bulk_write(const std::uint8_t* buffer, std::size_t* size)
{
    (void) buffer;
    (void) size;
    DBG_HELPER(dbg);
    assert_is_open();
}

void TestUsbDevice::assert_is_open() const
{
    if (!is_open()) {
        throw SaneException("device not open");
    }
}

} // namespace genesys
