/* sane - Scanner Access Now Easy.

   Copyright (C) 2010-2013 Stéphane Voltz <stef.dev@free.fr>


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

#include "low.h"
#include "assert.h"

#include <cstdio>
#include <vector>

/* ------------------------------------------------------------------------ */
/*                  functions calling ASIC specific functions               */
/* ------------------------------------------------------------------------ */

/**
 * setup the hardware dependent functions
 */

std::unique_ptr<CommandSet> create_gl124_cmd_set();
std::unique_ptr<CommandSet> create_gl646_cmd_set();
std::unique_ptr<CommandSet> create_gl841_cmd_set();
std::unique_ptr<CommandSet> create_gl843_cmd_set();
std::unique_ptr<CommandSet> create_gl846_cmd_set();
std::unique_ptr<CommandSet> create_gl847_cmd_set();

void sanei_genesys_init_cmd_set(Genesys_Device* dev)
{
  DBG_INIT ();
    DBG_HELPER(dbg);
    switch (dev->model->asic_type) {
        case AsicType::GL646: dev->cmd_set = create_gl646_cmd_set(); break;
        case AsicType::GL841: dev->cmd_set = create_gl841_cmd_set(); break;
        case AsicType::GL843: dev->cmd_set = create_gl843_cmd_set(); break;
        case AsicType::GL845: // since only a few reg bits differs we handle both together
        case AsicType::GL846: dev->cmd_set = create_gl846_cmd_set(); break;
        case AsicType::GL847: dev->cmd_set = create_gl847_cmd_set(); break;
        case AsicType::GL124: dev->cmd_set = create_gl124_cmd_set(); break;
        default: throw SaneException(SANE_STATUS_INVAL, "unknown ASIC type");
    }
}

/* ------------------------------------------------------------------------ */
/*                  General IO and debugging functions                      */
/* ------------------------------------------------------------------------ */

void sanei_genesys_write_file(const char* filename, const std::uint8_t* data, std::size_t length)
{
    DBG_HELPER(dbg);
    FILE *out;

    out = fopen (filename, "w");
    if (!out) {
        throw SaneException("could not open %s for writing: %s", filename, strerror(errno));
    }
    fwrite(data, 1, length, out);
    fclose(out);
}

// Write data to a pnm file (e.g. calibration). For debugging only
// data is RGB or grey, with little endian byte order
void sanei_genesys_write_pnm_file(const char* filename, const std::uint8_t* data, int depth,
                                  int channels, int pixels_per_line, int lines)
{
    DBG_HELPER_ARGS(dbg, "depth=%d, channels=%d, ppl=%d, lines=%d", depth, channels,
                    pixels_per_line, lines);
  int count;

    std::FILE* out = std::fopen(filename, "w");
  if (!out)
    {
        throw SaneException("could not open %s for writing: %s\n", filename, strerror(errno));
    }
  if(depth==1)
    {
      fprintf (out, "P4\n%d\n%d\n", pixels_per_line, lines);
    }
  else
    {
        std::fprintf(out, "P%c\n%d\n%d\n%d\n", channels == 1 ? '5' : '6', pixels_per_line, lines,
                     static_cast<int>(std::pow(static_cast<double>(2),
                                               static_cast<double>(depth - 1))));
    }
  if (channels == 3)
    {
      for (count = 0; count < (pixels_per_line * lines * 3); count++)
	{
	  if (depth == 16)
	    fputc (*(data + 1), out);
	  fputc (*(data++), out);
	  if (depth == 16)
	    data++;
	}
    }
  else
    {
      if (depth==1)
        {
          pixels_per_line/=8;
        }
      for (count = 0; count < (pixels_per_line * lines); count++)
	{
          switch (depth)
            {
              case 8:
	        fputc (*(data + count), out);
                break;
              case 16:
	        fputc (*(data + 1), out);
	        fputc (*(data), out);
	        data += 2;
                break;
              default:
                fputc(data[count], out);
                break;
            }
	}
    }
    std::fclose(out);
}

void sanei_genesys_write_pnm_file16(const char* filename, const uint16_t* data, unsigned channels,
                                    unsigned pixels_per_line, unsigned lines)
{
    DBG_HELPER_ARGS(dbg, "channels=%d, ppl=%d, lines=%d", channels,
                    pixels_per_line, lines);

    std::FILE* out = std::fopen(filename, "w");
    if (!out) {
        throw SaneException("could not open %s for writing: %s\n", filename, strerror(errno));
    }
    std::fprintf(out, "P%c\n%d\n%d\n%d\n", channels == 1 ? '5' : '6',
                 pixels_per_line, lines, 256 * 256 - 1);

    for (unsigned count = 0; count < (pixels_per_line * lines * channels); count++) {
        fputc(*data >> 8, out);
        fputc(*data & 0xff, out);
        data++;
    }
    std::fclose(out);
}

bool is_supported_write_pnm_file_image_format(PixelFormat format)
{
    switch (format) {
        case PixelFormat::I1:
        case PixelFormat::RGB111:
        case PixelFormat::I8:
        case PixelFormat::RGB888:
        case PixelFormat::I16:
        case PixelFormat::RGB161616:
            return true;
        default:
            return false;
    }
}

void sanei_genesys_write_pnm_file(const char* filename, const Image& image)
{
    if (!is_supported_write_pnm_file_image_format(image.get_format())) {
        throw SaneException("Unsupported format %d", static_cast<unsigned>(image.get_format()));
    }

    sanei_genesys_write_pnm_file(filename, image.get_row_ptr(0),
                                 get_pixel_format_depth(image.get_format()),
                                 get_pixel_channels(image.get_format()),
                                 image.get_width(), image.get_height());
}

/* ------------------------------------------------------------------------ */
/*                  Read and write RAM, registers and AFE                   */
/* ------------------------------------------------------------------------ */

unsigned sanei_genesys_get_bulk_max_size(AsicType asic_type)
{
    /*  Genesys supports 0xFE00 maximum size in general, wheraus GL646 supports
        0xFFC0. We use 0xF000 because that's the packet limit in the Linux usbmon
        USB capture stack. By default it limits packet size to b_size / 5 where
        b_size is the size of the ring buffer. By default it's 300*1024, so the
        packet is limited 61440 without any visibility to acquiring software.
    */
    if (asic_type == AsicType::GL124 ||
        asic_type == AsicType::GL846 ||
        asic_type == AsicType::GL847)
    {
        return 0xeff0;
    }
    return 0xf000;
}

void sanei_genesys_bulk_read_data_send_header(Genesys_Device* dev, size_t len)
{
    DBG_HELPER(dbg);

    uint8_t outdata[8];
    if (dev->model->asic_type == AsicType::GL124 ||
        dev->model->asic_type == AsicType::GL846 ||
        dev->model->asic_type == AsicType::GL847)
    {
        // hard coded 0x10000000 address
        outdata[0] = 0;
        outdata[1] = 0;
        outdata[2] = 0;
        outdata[3] = 0x10;
    } else if (dev->model->asic_type == AsicType::GL841 ||
               dev->model->asic_type == AsicType::GL843) {
        outdata[0] = BULK_IN;
        outdata[1] = BULK_RAM;
        outdata[2] = 0x82; //
        outdata[3] = 0x00;
    } else {
        outdata[0] = BULK_IN;
        outdata[1] = BULK_RAM;
        outdata[2] = 0x00;
        outdata[3] = 0x00;
    }

    /* data size to transfer */
    outdata[4] = (len & 0xff);
    outdata[5] = ((len >> 8) & 0xff);
    outdata[6] = ((len >> 16) & 0xff);
    outdata[7] = ((len >> 24) & 0xff);

    dev->usb_dev.control_msg(REQUEST_TYPE_OUT, REQUEST_BUFFER, VALUE_BUFFER, 0x00,
                             sizeof(outdata), outdata);
}

void sanei_genesys_bulk_read_data(Genesys_Device * dev, uint8_t addr, uint8_t* data,
                                  size_t len)
{
    DBG_HELPER(dbg);

    // currently supported: GL646, GL841, GL843, GL846, GL847, GL124
    size_t size, target;
    uint8_t *buffer;

    unsigned is_addr_used = 1;
    unsigned has_header_before_each_chunk = 0;
    if (dev->model->asic_type == AsicType::GL124 ||
        dev->model->asic_type == AsicType::GL846 ||
        dev->model->asic_type == AsicType::GL847)
    {
        is_addr_used = 0;
        has_header_before_each_chunk = 1;
    }

    if (is_addr_used) {
        DBG(DBG_io, "%s: requesting %zu bytes from 0x%02x addr\n", __func__, len, addr);
    } else {
        DBG(DBG_io, "%s: requesting %zu bytes\n", __func__, len);
    }

    if (len == 0)
        return;

    if (is_addr_used) {
        dev->usb_dev.control_msg(REQUEST_TYPE_OUT, REQUEST_REGISTER, VALUE_SET_REGISTER, 0x00,
                                 1, &addr);
    }

    target = len;
    buffer = data;

    size_t max_in_size = sanei_genesys_get_bulk_max_size(dev->model->asic_type);

    if (!has_header_before_each_chunk) {
        sanei_genesys_bulk_read_data_send_header(dev, len);
    }

    // loop until computed data size is read
    while (target) {
        if (target > max_in_size) {
            size = max_in_size;
        } else {
            size = target;
        }

        if (has_header_before_each_chunk) {
            sanei_genesys_bulk_read_data_send_header(dev, size);
        }

        DBG(DBG_io2, "%s: trying to read %zu bytes of data\n", __func__, size);

        dev->usb_dev.bulk_read(data, &size);

        DBG(DBG_io2, "%s: read %zu bytes, %zu remaining\n", __func__, size, target - size);

        target -= size;
        data += size;
    }

    if (DBG_LEVEL >= DBG_data && dev->binary != nullptr) {
        fwrite(buffer, len, 1, dev->binary);
    }
}

void sanei_genesys_bulk_write_data(Genesys_Device* dev, uint8_t addr, uint8_t* data, size_t len)
{
    DBG_HELPER_ARGS(dbg, "writing %zu bytes", len);

    // supported: GL646, GL841, GL843
    size_t size;
    uint8_t outdata[8];

    dev->usb_dev.control_msg(REQUEST_TYPE_OUT, REQUEST_REGISTER, VALUE_SET_REGISTER, INDEX,
                             1, &addr);

    size_t max_out_size = sanei_genesys_get_bulk_max_size(dev->model->asic_type);

    while (len) {
        if (len > max_out_size)
            size = max_out_size;
        else
            size = len;

        if (dev->model->asic_type == AsicType::GL841) {
            outdata[0] = BULK_OUT;
            outdata[1] = BULK_RAM;
            // both 0x82 and 0x00 works on GL841.
            outdata[2] = 0x82;
            outdata[3] = 0x00;
        } else {
            outdata[0] = BULK_OUT;
            outdata[1] = BULK_RAM;
            // 8600F uses 0x82, but 0x00 works too. 8400F uses 0x02 for certain transactions.
            outdata[2] = 0x00;
            outdata[3] = 0x00;
        }

        outdata[4] = (size & 0xff);
        outdata[5] = ((size >> 8) & 0xff);
        outdata[6] = ((size >> 16) & 0xff);
        outdata[7] = ((size >> 24) & 0xff);

        dev->usb_dev.control_msg(REQUEST_TYPE_OUT, REQUEST_BUFFER, VALUE_BUFFER, 0x00,
                                 sizeof(outdata), outdata);

        dev->usb_dev.bulk_write(data, &size);

        DBG(DBG_io2, "%s: wrote %zu bytes, %zu remaining\n", __func__, size, len - size);

        len -= size;
        data += size;
    }
}

/** @brief write to one high (addr >= 0x100) register
 * write to a register which address is higher than 0xff.
 * @param dev opened device to write to
 * @param reg LSB of register address
 * @param val value to write
 */
static void sanei_genesys_write_hregister(Genesys_Device* dev, uint16_t reg, uint8_t val)
{
    DBG_HELPER(dbg);

  uint8_t buffer[2];

  buffer[0]=reg & 0xff;
  buffer[1]=val;


    dev->usb_dev.control_msg(REQUEST_TYPE_OUT, REQUEST_BUFFER, 0x100 | VALUE_SET_REGISTER, INDEX,
                             2, buffer);

    DBG(DBG_io, "%s (0x%02x, 0x%02x) completed\n", __func__, reg, val);
}

/** @brief read from one high (addr >= 0x100) register
 * Read to a register which address is higher than 0xff. Second byte is check to detect
 * physical link errors.
 * @param dev opened device to read from
 * @param reg LSB of register address
 * @param val value to write
 */
static void sanei_genesys_read_hregister(Genesys_Device* dev, uint16_t reg, uint8_t* val)
{
    DBG_HELPER(dbg);

  SANE_Byte value[2];

    dev->usb_dev.control_msg(REQUEST_TYPE_IN, REQUEST_BUFFER, 0x100 | VALUE_GET_REGISTER,
                             0x22+((reg & 0xff)<<8), 2, value);

  *val=value[0];
  DBG(DBG_io2, "%s(0x%02x)=0x%02x\n", __func__, reg, *val);

  /* check usb link status */
    if ((value[1] & 0xff) != 0x55) {
        throw SaneException(SANE_STATUS_IO_ERROR, "invalid read, scanner unplugged");
    }
}

/**
 * Write to one GL847 ASIC register
URB    10  control  0x40 0x04 0x83 0x00 len     2 wrote 0xa6 0x04
 */
static void sanei_genesys_write_gl847_register(Genesys_Device* dev, uint8_t reg, uint8_t val)
{
    DBG_HELPER(dbg);

  uint8_t buffer[2];

  buffer[0]=reg;
  buffer[1]=val;

    dev->usb_dev.control_msg(REQUEST_TYPE_OUT, REQUEST_BUFFER, VALUE_SET_REGISTER, INDEX,
                             2, buffer);

  DBG(DBG_io, "%s (0x%02x, 0x%02x) completed\n", __func__, reg, val);
}

/**
 * Write to one ASIC register
 */
void sanei_genesys_write_register(Genesys_Device* dev, uint16_t reg, uint8_t val)
{
    DBG_HELPER(dbg);

  SANE_Byte reg8;

    // 16 bit register address space
    if (reg > 255) {
        sanei_genesys_write_hregister(dev, reg, val);
        return;
    }

    // route to gl847 function if needed
    if (dev->model->asic_type == AsicType::GL847 ||
        dev->model->asic_type == AsicType::GL845 ||
        dev->model->asic_type == AsicType::GL846 ||
        dev->model->asic_type == AsicType::GL124)
    {
        sanei_genesys_write_gl847_register(dev, reg, val);
        return;
    }

  reg8=reg & 0xff;

    dev->usb_dev.control_msg(REQUEST_TYPE_OUT, REQUEST_REGISTER, VALUE_SET_REGISTER, INDEX,
                             1, &reg8);

    dev->usb_dev.control_msg(REQUEST_TYPE_OUT, REQUEST_REGISTER, VALUE_WRITE_REGISTER, INDEX,
                             1, &val);

  DBG(DBG_io, "%s (0x%02x, 0x%02x) completed\n", __func__, reg, val);
    return;
}

/**
 * @brief write command to 0x8c endpoint
 * Write a value to 0x8c end point (end access), for USB firmware related operations
 * Known values are 0x0f, 0x11 for USB 2.0 data transfer and 0x0f,0x14 for USB1.1
 * @param dev device to write to
 * @param index index of the command
 * @param val value to write
 */
void sanei_genesys_write_0x8c(Genesys_Device* dev, uint8_t index, uint8_t val)
{
    DBG_HELPER_ARGS(dbg, "0x%02x,0x%02x", index, val);
    dev->usb_dev.control_msg(REQUEST_TYPE_OUT, REQUEST_REGISTER, VALUE_BUF_ENDACCESS, index, 1,
                             &val);
}

/* read reg 0x41:
 * URB   164  control  0xc0 0x04 0x8e 0x4122 len     2 read  0xfc 0x55
 */
static void sanei_genesys_read_gl847_register(Genesys_Device* dev, uint16_t reg, uint8_t* val)
{
    DBG_HELPER(dbg);
  SANE_Byte value[2];

    dev->usb_dev.control_msg(REQUEST_TYPE_IN, REQUEST_BUFFER, VALUE_GET_REGISTER, 0x22+(reg<<8),
                             2, value);

  *val=value[0];
  DBG(DBG_io2, "%s(0x%02x)=0x%02x\n", __func__, reg, *val);

  /* check usb link status */
  if((value[1] & 0xff) != 0x55)
    {
      throw SaneException(SANE_STATUS_IO_ERROR, "invalid read, scanner unplugged?");
    }
}

// Read from one register
void sanei_genesys_read_register(Genesys_Device* dev, uint16_t reg, uint8_t* val)
{
    DBG_HELPER(dbg);

  SANE_Byte reg8;

    // 16 bit register address space
    if (reg > 255) {
        sanei_genesys_read_hregister(dev, reg, val);
        return;
    }

    // route to gl847 function if needed
    if (dev->model->asic_type == AsicType::GL847 ||
        dev->model->asic_type == AsicType::GL845 ||
        dev->model->asic_type == AsicType::GL846 ||
        dev->model->asic_type == AsicType::GL124)
    {
        sanei_genesys_read_gl847_register(dev, reg, val);
        return;
    }

  /* 8 bit register address space */
    reg8 = reg & 0Xff;

    dev->usb_dev.control_msg(REQUEST_TYPE_OUT, REQUEST_REGISTER, VALUE_SET_REGISTER, INDEX,
                             1, &reg8);

  *val = 0;

    dev->usb_dev.control_msg(REQUEST_TYPE_IN, REQUEST_REGISTER, VALUE_READ_REGISTER, INDEX,
                             1, val);

  DBG(DBG_io, "%s (0x%02x, 0x%02x) completed\n", __func__, reg, *val);
}

// Set address for writing data
void sanei_genesys_set_buffer_address(Genesys_Device* dev, uint32_t addr)
{
    DBG_HELPER(dbg);

    if (dev->model->asic_type==AsicType::GL847 ||
        dev->model->asic_type==AsicType::GL845 ||
        dev->model->asic_type==AsicType::GL846 ||
        dev->model->asic_type==AsicType::GL124)
    {
      DBG(DBG_warn, "%s: shouldn't be used for GL846+ ASICs\n", __func__);
      return;
    }

  DBG(DBG_io, "%s: setting address to 0x%05x\n", __func__, addr & 0xfffffff0);

  addr = addr >> 4;

    dev->write_register(0x2b, (addr & 0xff));

  addr = addr >> 8;
    dev->write_register(0x2a, (addr & 0xff));
}

/**@brief read data from analog frontend (AFE)
 * @param dev device owning the AFE
 * @param addr register address to read
 * @param data placeholder for the result
 */
void sanei_genesys_fe_read_data (Genesys_Device* dev, uint8_t addr, uint16_t* data)
{
    DBG_HELPER(dbg);
  Genesys_Register_Set reg;

  reg.init_reg(0x50, addr);

    // set up read address
    dev->write_registers(reg);

    // read data
    uint8_t value = dev->read_register(0x46);
    *data = 256 * value;
    value = dev->read_register(0x47);
    *data += value;

  DBG(DBG_io, "%s (0x%02x, 0x%04x)\n", __func__, addr, *data);
}

/*@brief write data to analog frontend
 * writes data to analog frontend to set it up accordingly
 * to the sensor settings (exposure, timings, color, bit depth, ...)
 * @param dev devie owning the AFE to write to
 * @param addr AFE rister address
 * @param data value to write to AFE register
 **/
void sanei_genesys_fe_write_data(Genesys_Device* dev, uint8_t addr, uint16_t data)
{
    DBG_HELPER_ARGS(dbg, "0x%02x, 0x%04x", addr, data);
  Genesys_Register_Set reg(Genesys_Register_Set::SEQUENTIAL);

    reg.init_reg(0x51, addr);
    if (dev->model->asic_type == AsicType::GL124) {
        reg.init_reg(0x5d, (data / 256) & 0xff);
        reg.init_reg(0x5e, data & 0xff);
    } else {
        reg.init_reg(0x3a, (data / 256) & 0xff);
        reg.init_reg(0x3b, data & 0xff);
    }

    dev->write_registers(reg);
}

/* ------------------------------------------------------------------------ */
/*                       Medium level functions                             */
/* ------------------------------------------------------------------------ */

/** read the status register
 */
std::uint8_t sanei_genesys_get_status(Genesys_Device* dev)
{
    DBG_HELPER(dbg);
    std::uint16_t address = 0x41;
    if (dev->model->asic_type == AsicType::GL124) {
        address = 0x101;
    }
    return dev->read_register(address);
}

/**
 * decodes and prints content of status register
 * @param val value read from status register
 */
void sanei_genesys_print_status (uint8_t val)
{
  char msg[80];

  sprintf (msg, "%s%s%s%s%s%s%s%s",
	   val & PWRBIT ? "PWRBIT " : "",
	   val & BUFEMPTY ? "BUFEMPTY " : "",
	   val & FEEDFSH ? "FEEDFSH " : "",
	   val & SCANFSH ? "SCANFSH " : "",
	   val & HOMESNR ? "HOMESNR " : "",
	   val & LAMPSTS ? "LAMPSTS " : "",
	   val & FEBUSY ? "FEBUSY " : "",
	   val & MOTORENB ? "MOTORENB" : "");
  DBG(DBG_info, "status=%s\n", msg);
}

#if 0
/* returns pixels per line from register set */
/*candidate for moving into chip specific files?*/
static int
genesys_pixels_per_line (Genesys_Register_Set * reg)
{
    int pixels_per_line;

    pixels_per_line = reg->get8(0x32) * 256 + reg->get8(0x33);
    pixels_per_line -= (reg->get8(0x30) * 256 + reg->get8(0x31));

    return pixels_per_line;
}

/* returns dpiset from register set */
/*candidate for moving into chip specific files?*/
static int
genesys_dpiset (Genesys_Register_Set * reg)
{
    return reg->get8(0x2c) * 256 + reg->get8(0x2d);
}
#endif

/** read the number of valid words in scanner's RAM
 * ie registers 42-43-44
 */
// candidate for moving into chip specific files?
void sanei_genesys_read_valid_words(Genesys_Device* dev, unsigned int* words)
{
    DBG_HELPER(dbg);

  switch (dev->model->asic_type)
    {
    case AsicType::GL124:
            *words = dev->read_register(0x102) & 0x03;
            *words = *words * 256 + dev->read_register(0x103);
            *words = *words * 256 + dev->read_register(0x104);
            *words = *words * 256 + dev->read_register(0x105);
            break;

    case AsicType::GL845:
    case AsicType::GL846:
            *words = dev->read_register(0x42) & 0x02;
            *words = *words * 256 + dev->read_register(0x43);
            *words = *words * 256 + dev->read_register(0x44);
            *words = *words * 256 + dev->read_register(0x45);
            break;

    case AsicType::GL847:
            *words = dev->read_register(0x42) & 0x03;
            *words = *words * 256 + dev->read_register(0x43);
            *words = *words * 256 + dev->read_register(0x44);
            *words = *words * 256 + dev->read_register(0x45);
            break;

    default:
            *words = dev->read_register(0x44);
            *words += dev->read_register(0x43) * 256;
            if (dev->model->asic_type == AsicType::GL646) {
                *words += ((dev->read_register(0x42) & 0x03) * 256 * 256);
            } else {
                *words += ((dev->read_register(0x42) & 0x0f) * 256 * 256);
            }
    }

  DBG(DBG_proc, "%s: %d words\n", __func__, *words);
}

/** read the number of lines scanned
 * ie registers 4b-4c-4d
 */
void sanei_genesys_read_scancnt(Genesys_Device* dev, unsigned int* words)
{
    DBG_HELPER(dbg);

    if (dev->model->asic_type == AsicType::GL124) {
        *words = (dev->read_register(0x10b) & 0x0f) << 16;
        *words += (dev->read_register(0x10c) << 8);
        *words += dev->read_register(0x10d);
    }
  else
    {
        *words = dev->read_register(0x4d);
        *words += dev->read_register(0x4c) * 256;
        if (dev->model->asic_type == AsicType::GL646) {
            *words += ((dev->read_register(0x4b) & 0x03) * 256 * 256);
        } else {
            *words += ((dev->read_register(0x4b) & 0x0f) * 256 * 256);
        }
    }

  DBG(DBG_proc, "%s: %d lines\n", __func__, *words);
}

/** @brief Check if the scanner's internal data buffer is empty
 * @param *dev device to test for data
 * @param *empty return value
 * @return empty will be set to true if there is no scanned data.
 **/
bool sanei_genesys_is_buffer_empty(Genesys_Device* dev)
{
    DBG_HELPER(dbg);
  uint8_t val = 0;

  sanei_genesys_sleep_ms(1);
    val = sanei_genesys_get_status(dev);

    if (dev->cmd_set->test_buffer_empty_bit(val)) {
      /* fix timing issue on USB3 (or just may be too fast) hardware
       * spotted by John S. Weber <jweber53@gmail.com>
       */
      sanei_genesys_sleep_ms(1);
      DBG(DBG_io2, "%s: buffer is empty\n", __func__);
        return true;
    }


  DBG(DBG_io, "%s: buffer is filled\n", __func__);
    return false;
}

void wait_until_buffer_non_empty(Genesys_Device* dev, bool check_status_twice)
{
    // FIXME: reduce MAX_RETRIES once tests are updated
    const unsigned MAX_RETRIES = 100000;
    for (unsigned i = 0; i < MAX_RETRIES; ++i) {

        if (check_status_twice) {
            // FIXME: this only to preserve previous behavior, can be removed
            sanei_genesys_get_status(dev);
        }

        bool empty = sanei_genesys_is_buffer_empty(dev);
        sanei_genesys_sleep_ms(10);
        if (!empty)
            return;
    }
    throw SaneException(SANE_STATUS_IO_ERROR, "failed to read data");
}

void wait_until_has_valid_words(Genesys_Device* dev)
{
    unsigned words = 0;
    unsigned sleep_time_ms = 10;

    for (unsigned wait_ms = 0; wait_ms < 50000; wait_ms += sleep_time_ms) {
        sanei_genesys_read_valid_words(dev, &words);
        if (words != 0)
            break;
        sanei_genesys_sleep_ms(sleep_time_ms);
    }

    if (words == 0) {
        throw SaneException(SANE_STATUS_IO_ERROR, "timeout, buffer does not get filled");
    }
}

// Read data (e.g scanned image) from scan buffer
void sanei_genesys_read_data_from_scanner(Genesys_Device* dev, uint8_t* data, size_t size)
{
    DBG_HELPER_ARGS(dbg, "size = %zu bytes", size);

  if (size & 1)
    DBG(DBG_info, "WARNING %s: odd number of bytes\n", __func__);

    wait_until_has_valid_words(dev);

    dev->cmd_set->bulk_read_data(dev, 0x45, data, size);
}

Image read_unshuffled_image_from_scanner(Genesys_Device* dev, const ScanSession& session,
                                         std::size_t total_bytes)
{
    DBG_HELPER(dbg);

    auto format = create_pixel_format(session.params.depth,
                                      dev->model->is_cis ? 1 : session.params.channels,
                                      dev->model->line_mode_color_order);

    auto width = get_pixels_from_row_bytes(format, session.output_line_bytes_raw);
    auto height = session.output_line_count * (dev->model->is_cis ? session.params.channels : 1);

    Image image(width, height, format);

    auto max_bytes = image.get_row_bytes() * height;
    if (total_bytes > max_bytes) {
        throw SaneException("Trying to read too much data %zu (max %zu)", total_bytes, max_bytes);
    }
    if (total_bytes != max_bytes) {
        DBG(DBG_info, "WARNING %s: trying to read not enough data (%zu, full fill %zu\n", __func__,
            total_bytes, max_bytes);
    }

    sanei_genesys_read_data_from_scanner(dev, image.get_row_ptr(0), total_bytes);

    ImagePipelineStack pipeline;
    pipeline.push_first_node<ImagePipelineNodeImageSource>(image);

    if ((dev->model->flags & GENESYS_FLAG_16BIT_DATA_INVERTED) && session.params.depth == 16) {
        dev->pipeline.push_node<ImagePipelineNodeSwap16BitEndian>();
    }

#ifdef WORDS_BIGENDIAN
    if (depth == 16) {
        dev->pipeline.push_node<ImagePipelineNodeSwap16BitEndian>();
    }
#endif

    if (dev->model->is_cis && session.params.channels == 3) {
        dev->pipeline.push_node<ImagePipelineNodeMergeMonoLines>(dev->model->line_mode_color_order);
    }

    if (dev->pipeline.get_output_format() == PixelFormat::BGR888) {
        dev->pipeline.push_node<ImagePipelineNodeFormatConvert>(PixelFormat::RGB888);
    }

    if (dev->pipeline.get_output_format() == PixelFormat::BGR161616) {
        dev->pipeline.push_node<ImagePipelineNodeFormatConvert>(PixelFormat::RGB161616);
    }

    return pipeline.get_image();
}

void sanei_genesys_read_feed_steps(Genesys_Device* dev, unsigned int* steps)
{
    DBG_HELPER(dbg);

    if (dev->model->asic_type == AsicType::GL124) {
        *steps = (dev->read_register(0x108) & 0x1f) << 16;
        *steps += (dev->read_register(0x109) << 8);
        *steps += dev->read_register(0x10a);
    }
  else
    {
        *steps = dev->read_register(0x4a);
        *steps += dev->read_register(0x49) * 256;
        if (dev->model->asic_type == AsicType::GL646) {
            *steps += ((dev->read_register(0x48) & 0x03) * 256 * 256);
        } else if (dev->model->asic_type == AsicType::GL841) {
            *steps += ((dev->read_register(0x48) & 0x0f) * 256 * 256);
        } else {
            *steps += ((dev->read_register(0x48) & 0x1f) * 256 * 256);
        }
    }

  DBG(DBG_proc, "%s: %d steps\n", __func__, *steps);
}

void sanei_genesys_set_lamp_power(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                  Genesys_Register_Set& regs, bool set)
{
    static const uint8_t REG03_LAMPPWR = 0x10;

    if (set) {
        regs.find_reg(0x03).value |= REG03_LAMPPWR;

        if (dev->model->asic_type == AsicType::GL841) {
            sanei_genesys_set_exposure(regs, sanei_genesys_fixup_exposure(sensor.exposure));
            regs.set8(0x19, 0x50);
        }

        if (dev->model->asic_type == AsicType::GL843) {
            sanei_genesys_set_exposure(regs, sensor.exposure);

            // we don't actually turn on lamp on infrared scan
            if ((dev->model->model_id == ModelId::CANON_8400F ||
                 dev->model->model_id == ModelId::CANON_8600F ||
                 dev->model->model_id == ModelId::PLUSTEK_OPTICFILM_7200I ||
                 dev->model->model_id == ModelId::PLUSTEK_OPTICFILM_7500I) &&
                dev->settings.scan_method == ScanMethod::TRANSPARENCY_INFRARED)
            {
                regs.find_reg(0x03).value &= ~REG03_LAMPPWR;
            }
        }
    } else {
        regs.find_reg(0x03).value &= ~REG03_LAMPPWR;

        if (dev->model->asic_type == AsicType::GL841) {
            sanei_genesys_set_exposure(regs, {0x0101, 0x0101, 0x0101});
            regs.set8(0x19, 0xff);
        }

        if (dev->model->asic_type == AsicType::GL843) {
            if (dev->model->model_id == ModelId::PANASONIC_KV_SS080 ||
                dev->model->model_id == ModelId::HP_SCANJET_4850C ||
                dev->model->model_id == ModelId::HP_SCANJET_G4010 ||
                dev->model->model_id == ModelId::HP_SCANJET_G4050)
            {
                // BUG: datasheet says we shouldn't set exposure to zero
                sanei_genesys_set_exposure(regs, {0, 0, 0});
            }
        }
    }
    regs.state.is_lamp_on = set;
}

void sanei_genesys_set_motor_power(Genesys_Register_Set& regs, bool set)
{
    static const uint8_t REG02_MTRPWR = 0x10;

    if (set) {
        regs.find_reg(0x02).value |= REG02_MTRPWR;
    } else {
        regs.find_reg(0x02).value &= ~REG02_MTRPWR;
    }
}

/**
 * Write to many registers at once
 * Note: sequential call to write register, no effective
 * bulk write implemented.
 * @param dev device to write to
 * @param reg pointer to an array of registers
 * @param elems size of the array
 */
void sanei_genesys_bulk_write_register(Genesys_Device* dev, const Genesys_Register_Set& reg)
{
    DBG_HELPER(dbg);

    if (dev->model->asic_type == AsicType::GL646 ||
        dev->model->asic_type == AsicType::GL841)
    {
        uint8_t outdata[8];
        std::vector<uint8_t> buffer;
        buffer.reserve(reg.size() * 2);

        /* copy registers and values in data buffer */
        for (const auto& r : reg) {
            buffer.push_back(r.address);
            buffer.push_back(r.value);
        }

        DBG(DBG_io, "%s (elems= %zu, size = %zu)\n", __func__, reg.size(), buffer.size());

        if (dev->model->asic_type == AsicType::GL646) {
            outdata[0] = BULK_OUT;
            outdata[1] = BULK_REGISTER;
            outdata[2] = 0x00;
            outdata[3] = 0x00;
            outdata[4] = (buffer.size() & 0xff);
            outdata[5] = ((buffer.size() >> 8) & 0xff);
            outdata[6] = ((buffer.size() >> 16) & 0xff);
            outdata[7] = ((buffer.size() >> 24) & 0xff);

            dev->usb_dev.control_msg(REQUEST_TYPE_OUT, REQUEST_BUFFER, VALUE_BUFFER, INDEX,
                                     sizeof(outdata), outdata);

            size_t write_size = buffer.size();

            dev->usb_dev.bulk_write(buffer.data(), &write_size);
        } else {
            for (size_t i = 0; i < reg.size();) {
                size_t c = reg.size() - i;
                if (c > 32)  /*32 is max on GL841. checked that.*/
                    c = 32;

                dev->usb_dev.control_msg(REQUEST_TYPE_OUT, REQUEST_BUFFER, VALUE_SET_REGISTER,
                                         INDEX, c * 2, buffer.data() + i * 2);

                i += c;
            }
        }
    } else {
        for (const auto& r : reg) {
            dev->write_register(r.address, r.value);
        }
    }

    DBG (DBG_io, "%s: wrote %zu registers\n", __func__, reg.size());
}



/**
 * writes a block of data to AHB
 * @param dn USB device index
 * @param usb_mode usb mode : 1 usb 1.1, 2 usb 2.0
 * @param addr AHB address to write to
 * @param size size of the chunk of data
 * @param data pointer to the data to write
 */
void sanei_genesys_write_ahb(Genesys_Device* dev, uint32_t addr, uint32_t size, uint8_t* data)
{
    DBG_HELPER(dbg);

  uint8_t outdata[8];
  size_t written,blksize;
  int i;
  char msg[100]="AHB=";

  outdata[0] = addr & 0xff;
  outdata[1] = ((addr >> 8) & 0xff);
  outdata[2] = ((addr >> 16) & 0xff);
  outdata[3] = ((addr >> 24) & 0xff);
  outdata[4] = (size & 0xff);
  outdata[5] = ((size >> 8) & 0xff);
  outdata[6] = ((size >> 16) & 0xff);
  outdata[7] = ((size >> 24) & 0xff);

  if (DBG_LEVEL >= DBG_io)
    {
      for (i = 0; i < 8; i++)
	{
          sprintf (msg+strlen(msg), " 0x%02x", outdata[i]);
	}
      DBG (DBG_io, "%s: write(0x%08x,0x%08x)\n", __func__, addr,size);
      DBG (DBG_io, "%s: %s\n", __func__, msg);
    }

    // write addr and size for AHB
    dev->usb_dev.control_msg(REQUEST_TYPE_OUT, REQUEST_BUFFER, VALUE_BUFFER, 0x01, 8, outdata);

  size_t max_out_size = sanei_genesys_get_bulk_max_size(dev->model->asic_type);

  /* write actual data */
  written = 0;
  do
    {
      if (size - written > max_out_size)
        {
          blksize = max_out_size;
        }
      else
        {
          blksize = size - written;
        }
        dev->usb_dev.bulk_write(data + written, &blksize);

      written += blksize;
    }
  while (written < size);
}


std::vector<uint16_t> get_gamma_table(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                      int color)
{
    if (!dev->gamma_override_tables[color].empty()) {
        return dev->gamma_override_tables[color];
    } else {
        std::vector<uint16_t> ret;
        sanei_genesys_create_default_gamma_table(dev, ret, sensor.gamma[color]);
        return ret;
    }
}

/** @brief generates gamma buffer to transfer
 * Generates gamma table buffer to send to ASIC. Applies
 * contrast and brightness if set.
 * @param dev device to set up
 * @param bits number of bits used by gamma
 * @param max value for gamma
 * @param size of the gamma table
 * @param gamma allocated gamma buffer to fill
 */
void sanei_genesys_generate_gamma_buffer(Genesys_Device* dev,
                                                const Genesys_Sensor& sensor,
                                                int bits,
                                                int max,
                                                int size,
                                                uint8_t* gamma)
{
    DBG_HELPER(dbg);
    std::vector<uint16_t> rgamma = get_gamma_table(dev, sensor, GENESYS_RED);
    std::vector<uint16_t> ggamma = get_gamma_table(dev, sensor, GENESYS_GREEN);
    std::vector<uint16_t> bgamma = get_gamma_table(dev, sensor, GENESYS_BLUE);

  if(dev->settings.contrast!=0 || dev->settings.brightness!=0)
    {
      std::vector<uint16_t> lut(65536);
      sanei_genesys_load_lut(reinterpret_cast<unsigned char *>(lut.data()),
                             bits,
                             bits,
                             0,
                             max,
                             dev->settings.contrast,
                             dev->settings.brightness);
      for (int i = 0; i < size; i++)
        {
          uint16_t value=rgamma[i];
          value=lut[value];
          gamma[i * 2 + size * 0 + 0] = value & 0xff;
          gamma[i * 2 + size * 0 + 1] = (value >> 8) & 0xff;

          value=ggamma[i];
          value=lut[value];
          gamma[i * 2 + size * 2 + 0] = value & 0xff;
          gamma[i * 2 + size * 2 + 1] = (value >> 8) & 0xff;

          value=bgamma[i];
          value=lut[value];
          gamma[i * 2 + size * 4 + 0] = value & 0xff;
          gamma[i * 2 + size * 4 + 1] = (value >> 8) & 0xff;
        }
    }
  else
    {
      for (int i = 0; i < size; i++)
        {
          uint16_t value=rgamma[i];
          gamma[i * 2 + size * 0 + 0] = value & 0xff;
          gamma[i * 2 + size * 0 + 1] = (value >> 8) & 0xff;

          value=ggamma[i];
          gamma[i * 2 + size * 2 + 0] = value & 0xff;
          gamma[i * 2 + size * 2 + 1] = (value >> 8) & 0xff;

          value=bgamma[i];
          gamma[i * 2 + size * 4 + 0] = value & 0xff;
          gamma[i * 2 + size * 4 + 1] = (value >> 8) & 0xff;
        }
    }
}


/** @brief send gamma table to scanner
 * This function sends generic gamma table (ie ones built with
 * provided gamma) or the user defined one if provided by
 * fontend. Used by gl846+ ASICs
 * @param dev device to write to
 */
void sanei_genesys_send_gamma_table(Genesys_Device* dev, const Genesys_Sensor& sensor)
{
    DBG_HELPER(dbg);
  int size;
  int i;

  size = 256 + 1;

  /* allocate temporary gamma tables: 16 bits words, 3 channels */
  std::vector<uint8_t> gamma(size * 2 * 3, 255);

    sanei_genesys_generate_gamma_buffer(dev, sensor, 16, 65535, size, gamma.data());

    // loop sending gamma tables NOTE: 0x01000000 not 0x10000000
    for (i = 0; i < 3; i++) {
        // clear corresponding GMM_N bit
        uint8_t val = dev->read_register(0xbd);
        val &= ~(0x01 << i);
        dev->write_register(0xbd, val);

        // clear corresponding GMM_F bit
        val = dev->read_register(0xbe);
      val &= ~(0x01 << i);
        dev->write_register(0xbe, val);

      // FIXME: currently the last word of each gamma table is not initialied, so to work around
      // unstable data, just set it to 0 which is the most likely value of uninitialized memory
      // (proper value is probably 0xff)
      gamma[size * 2 * i + size * 2 - 2] = 0;
      gamma[size * 2 * i + size * 2 - 1] = 0;

      /* set GMM_Z */
        dev->write_register(0xc5+2*i, gamma[size*2*i+1]);
        dev->write_register(0xc6+2*i, gamma[size*2*i]);

        sanei_genesys_write_ahb(dev, 0x01000000 + 0x200 * i, (size-1) * 2,
                                gamma.data() + i * size * 2+2);
    }
}

static unsigned align_int_up(unsigned num, unsigned alignment)
{
    unsigned mask = alignment - 1;
    if (num & mask)
        num = (num & ~mask) + alignment;
    return num;
}

void compute_session_buffer_sizes(AsicType asic, ScanSession& s)
{
    size_t line_bytes = s.output_line_bytes;
    size_t line_bytes_stagger = s.output_line_bytes;

    if (asic != AsicType::GL646) {
        // BUG: this is historical artifact and should be removed. Note that buffer sizes affect
        // how often we request the scanner for data and thus change the USB traffic.
        line_bytes_stagger =
                multiply_by_depth_ceil(s.optical_pixels, s.params.depth) * s.params.channels;
    }

    struct BufferConfig {
        size_t* result_size = nullptr;
        size_t lines = 0;
        size_t lines_mult = 0;
        size_t max_size = 0; // does not apply if 0
        size_t stagger_lines = 0;

        BufferConfig() = default;
        BufferConfig(std::size_t* rs, std::size_t l, std::size_t lm, std::size_t ms,
                     std::size_t sl) :
            result_size{rs},
            lines{l},
            lines_mult{lm},
            max_size{ms},
            stagger_lines{sl}
        {}
    };

    std::array<BufferConfig, 4> configs;
    if (asic == AsicType::GL124 || asic == AsicType::GL843) {
        configs = { {
            { &s.buffer_size_read, 32, 1, 0, s.max_color_shift_lines + s.num_staggered_lines },
            { &s.buffer_size_lines, 32, 1, 0, s.max_color_shift_lines + s.num_staggered_lines },
            { &s.buffer_size_shrink, 16, 1, 0, 0 },
            { &s.buffer_size_out, 8, 1, 0, 0 },
        } };
    } else if (asic == AsicType::GL841) {
        size_t max_buf = sanei_genesys_get_bulk_max_size(asic);
        configs = { {
            { &s.buffer_size_read, 8, 2, max_buf, s.max_color_shift_lines + s.num_staggered_lines },
            { &s.buffer_size_lines, 8, 2, max_buf, s.max_color_shift_lines + s.num_staggered_lines },
            { &s.buffer_size_shrink, 8, 1, max_buf, 0 },
            { &s.buffer_size_out, 8, 1, 0, 0 },
        } };
    } else {
        configs = { {
            { &s.buffer_size_read, 16, 1, 0, s.max_color_shift_lines + s.num_staggered_lines },
            { &s.buffer_size_lines, 16, 1, 0, s.max_color_shift_lines + s.num_staggered_lines },
            { &s.buffer_size_shrink, 8, 1, 0, 0 },
            { &s.buffer_size_out, 8, 1, 0, 0 },
        } };
    }

    for (BufferConfig& config : configs) {
        size_t buf_size = line_bytes * config.lines;
        if (config.max_size > 0 && buf_size > config.max_size) {
            buf_size = (config.max_size / line_bytes) * line_bytes;
        }
        buf_size *= config.lines_mult;
        buf_size += line_bytes_stagger * config.stagger_lines;
        *config.result_size = buf_size;
    }
}

void compute_session_pipeline(const Genesys_Device* dev, ScanSession& s)
{
    auto channels = s.params.channels;
    auto depth = s.params.depth;

    s.pipeline_needs_reorder = true;
    if (channels != 3 && depth != 16) {
        s.pipeline_needs_reorder = false;
    }
#ifndef WORDS_BIGENDIAN
    if (channels != 3 && depth == 16) {
        s.pipeline_needs_reorder = false;
    }
    if (channels == 3 && depth == 16 && !dev->model->is_cis &&
        dev->model->line_mode_color_order == ColorOrder::RGB)
    {
        s.pipeline_needs_reorder = false;
    }
#endif
    if (channels == 3 && depth == 8 && !dev->model->is_cis &&
        dev->model->line_mode_color_order == ColorOrder::RGB)
    {
        s.pipeline_needs_reorder = false;
    }
    s.pipeline_needs_ccd = s.max_color_shift_lines + s.num_staggered_lines > 0;
    s.pipeline_needs_shrink = dev->settings.requested_pixels != s.output_pixels;
}

void compute_session_pixel_offsets(const Genesys_Device* dev, ScanSession& s,
                                   const Genesys_Sensor& sensor,
                                   const SensorProfile* sensor_profile)
{
    unsigned ccd_pixels_per_system_pixel = sensor.ccd_pixels_per_system_pixel();

    if (dev->model->asic_type == AsicType::GL646) {

        // startx cannot be below dummy pixel value
        s.pixel_startx = sensor.dummy_pixel;
        if ((s.params.flags & SCAN_FLAG_USE_XCORRECTION) && sensor.ccd_start_xoffset > 0) {
            s.pixel_startx = sensor.ccd_start_xoffset;
        }
        s.pixel_startx += s.params.startx;

        if (dev->model->flags & GENESYS_FLAG_STAGGERED_LINE) {
            s.pixel_startx |= 1;
        }

        s.pixel_endx = s.pixel_startx + s.optical_pixels;

        s.pixel_startx /= sensor.ccd_pixels_per_system_pixel() * s.ccd_size_divisor;
        s.pixel_endx /= sensor.ccd_pixels_per_system_pixel() * s.ccd_size_divisor;

    } else if (dev->model->asic_type == AsicType::GL841) {
        s.pixel_startx = ((sensor.ccd_start_xoffset + s.params.startx) * s.optical_resolution)
                                / sensor.optical_res;

        s.pixel_startx += sensor.dummy_pixel + 1;

        if (s.num_staggered_lines > 0 && (s.pixel_startx & 1) == 0) {
            s.pixel_startx++;
        }

        /*  In case of SHDAREA, we need to align start on pixel average factor, startx is
            different than 0 only when calling for function to setup for scan, where shading data
            needs to be align.

            NOTE: we can check the value of the register here, because we don't set this bit
            anywhere except in initialization.
        */
        const uint8_t REG01_SHDAREA = 0x02;
        if ((dev->reg.find_reg(0x01).value & REG01_SHDAREA) != 0) {
            unsigned average_factor = s.optical_resolution / s.params.xres;
            s.pixel_startx = align_multiple_floor(s.pixel_startx, average_factor);
        }

        s.pixel_endx = s.pixel_startx + s.optical_pixels;

    } else if (dev->model->asic_type == AsicType::GL843) {

        s.pixel_startx = (s.params.startx + sensor.dummy_pixel) / ccd_pixels_per_system_pixel;
        s.pixel_endx = s.pixel_startx + s.optical_pixels / ccd_pixels_per_system_pixel;

        s.pixel_startx /= s.hwdpi_divisor;
        s.pixel_endx /= s.hwdpi_divisor;

        // in case of stagger we have to start at an odd coordinate
        if (s.num_staggered_lines > 0 && (s.pixel_startx & 1) == 0) {
            s.pixel_startx++;
            s.pixel_endx++;
        }

    } else if (dev->model->asic_type == AsicType::GL845 ||
               dev->model->asic_type == AsicType::GL846 ||
               dev->model->asic_type == AsicType::GL847)
    {
        s.pixel_startx = s.params.startx;

        if (s.num_staggered_lines > 0) {
            s.pixel_startx |= 1;
        }

        s.pixel_startx += sensor.ccd_start_xoffset * ccd_pixels_per_system_pixel;
        s.pixel_endx = s.pixel_startx + s.optical_pixels_raw;

        s.pixel_startx /= s.hwdpi_divisor * s.segment_count * ccd_pixels_per_system_pixel;
        s.pixel_endx /= s.hwdpi_divisor * s.segment_count * ccd_pixels_per_system_pixel;

    } else if (dev->model->asic_type == AsicType::GL124) {
        s.pixel_startx = s.params.startx;

        if (s.num_staggered_lines > 0) {
            s.pixel_startx |= 1;
        }

        s.pixel_startx /= ccd_pixels_per_system_pixel;
        // FIXME: should we add sensor.dummy_pxel to pixel_startx at this point?
        s.pixel_endx = s.pixel_startx + s.optical_pixels / ccd_pixels_per_system_pixel;

        s.pixel_startx /= s.hwdpi_divisor * s.segment_count;
        s.pixel_endx /= s.hwdpi_divisor * s.segment_count;

        const uint16_t REG_SEGCNT = 0x93;

        std::uint32_t segcnt = (sensor_profile->custom_regs.get_value(REG_SEGCNT) << 16) +
                               (sensor_profile->custom_regs.get_value(REG_SEGCNT + 1) << 8) +
                               sensor_profile->custom_regs.get_value(REG_SEGCNT + 2);
        if (s.pixel_endx == segcnt) {
            s.pixel_endx = 0;
        }
    }

    s.pixel_startx *= sensor.pixel_count_multiplier;
    s.pixel_endx *= sensor.pixel_count_multiplier;
}

void compute_session(Genesys_Device* dev, ScanSession& s, const Genesys_Sensor& sensor)
{
    DBG_HELPER(dbg);

    (void) dev;
    s.params.assert_valid();

    if (s.params.depth != 8 && s.params.depth != 16) {
        throw SaneException("Unsupported depth setting %d", s.params.depth);
    }

    unsigned ccd_pixels_per_system_pixel = sensor.ccd_pixels_per_system_pixel();

    // compute optical and output resolutions

    if (dev->model->asic_type == AsicType::GL843) {
        // FIXME: this may be incorrect, but need more scanners to test
        s.hwdpi_divisor = sensor.get_hwdpi_divisor_for_dpi(s.params.xres);
    } else {
        s.hwdpi_divisor = sensor.get_hwdpi_divisor_for_dpi(s.params.xres * ccd_pixels_per_system_pixel);
    }

    s.ccd_size_divisor = sensor.get_ccd_size_divisor_for_dpi(s.params.xres);

    if (dev->model->asic_type == AsicType::GL646) {
        s.optical_resolution = sensor.optical_res;
    } else {
        s.optical_resolution = sensor.optical_res / s.ccd_size_divisor;
    }
    s.output_resolution = s.params.xres;

    if (s.output_resolution > s.optical_resolution) {
        throw std::runtime_error("output resolution higher than optical resolution");
    }

    // compute the number of optical pixels that will be acquired by the chip
    s.optical_pixels = (s.params.pixels * s.optical_resolution) / s.output_resolution;
    if (s.optical_pixels * s.output_resolution < s.params.pixels * s.optical_resolution) {
        s.optical_pixels++;
    }

    if (dev->model->asic_type == AsicType::GL841) {
        if (s.optical_pixels & 1)
            s.optical_pixels++;
    }

    if (dev->model->asic_type == AsicType::GL646 && s.params.xres == 400) {
        s.optical_pixels = (s.optical_pixels / 6) * 6;
    }

    if (dev->model->asic_type == AsicType::GL843) {
        // ensure the number of optical pixels is divisible by 2.
        // In quarter-CCD mode optical_pixels is 4x larger than the actual physical number
        s.optical_pixels = align_int_up(s.optical_pixels, 2 * s.ccd_size_divisor);

        if (dev->model->model_id == ModelId::PLUSTEK_OPTICFILM_7200I ||
            dev->model->model_id == ModelId::PLUSTEK_OPTICFILM_7300 ||
            dev->model->model_id == ModelId::PLUSTEK_OPTICFILM_7500I)
        {
            s.optical_pixels = align_int_up(s.optical_pixels, 16);
        }
    }

    // after all adjustments on the optical pixels have been made, compute the number of pixels
    // to retrieve from the chip
    s.output_pixels = (s.optical_pixels * s.output_resolution) / s.optical_resolution;

    // Note: staggering is not applied for calibration. Staggering starts at 2400 dpi
    s.num_staggered_lines = 0;

    if (dev->model->asic_type == AsicType::GL124 ||
        dev->model->asic_type == AsicType::GL841 ||
        dev->model->asic_type == AsicType::GL845 ||
        dev->model->asic_type == AsicType::GL846 ||
        dev->model->asic_type == AsicType::GL847)
    {
        if (s.ccd_size_divisor == 1 && (dev->model->flags & GENESYS_FLAG_STAGGERED_LINE)) {
            s.num_staggered_lines = (4 * s.params.yres) / dev->motor.base_ydpi;
        }
    }

    if (dev->model->asic_type == AsicType::GL646) {
        if (s.ccd_size_divisor == 1 && (dev->model->flags & GENESYS_FLAG_STAGGERED_LINE)) {
            // for HP3670, stagger happens only at >=1200 dpi
            if ((dev->model->motor_id != MotorId::HP3670 &&
                 dev->model->motor_id != MotorId::HP2400) ||
                s.params.yres >= static_cast<unsigned>(sensor.optical_res))
            {
                s.num_staggered_lines = (4 * s.params.yres) / dev->motor.base_ydpi;
            }
        }
    }

    if (dev->model->asic_type == AsicType::GL843) {
        if ((s.params.yres > 1200) && // FIXME: maybe ccd_size_divisor is the one that controls this?
            ((s.params.flags & SCAN_FLAG_IGNORE_LINE_DISTANCE) == 0) &&
            (dev->model->flags & GENESYS_FLAG_STAGGERED_LINE))
        {
            s.num_staggered_lines = (4 * s.params.yres) / dev->motor.base_ydpi;
        }
    }

    s.max_color_shift_lines = sanei_genesys_compute_max_shift(dev, s.params.channels,
                                                              s.params.yres, s.params.flags);

    s.output_line_count = s.params.lines + s.max_color_shift_lines + s.num_staggered_lines;

    s.output_channel_bytes = multiply_by_depth_ceil(s.output_pixels, s.params.depth);
    s.output_line_bytes = s.output_channel_bytes * s.params.channels;

    const SensorProfile* sensor_profile = nullptr;
    if (dev->model->asic_type == AsicType::GL124 ||
        dev->model->asic_type == AsicType::GL845 ||
        dev->model->asic_type == AsicType::GL846 ||
        dev->model->asic_type == AsicType::GL847)
    {
        unsigned ccd_size_divisor_for_profile = 1;
        if (dev->model->asic_type == AsicType::GL124) {
            ccd_size_divisor_for_profile = s.ccd_size_divisor;
        }
        unsigned dpihw = sensor.get_register_hwdpi(s.output_resolution * ccd_pixels_per_system_pixel);

        sensor_profile = &get_sensor_profile(dev->model->asic_type, sensor, dpihw,
                                             ccd_size_divisor_for_profile);
    }

    s.segment_count = 1;
    if (dev->model->flags & GENESYS_FLAG_SIS_SENSOR || dev->model->asic_type == AsicType::GL124) {
        s.segment_count = sensor_profile->get_segment_count();
    } else {
        s.segment_count = sensor.get_segment_count();
    }

    s.optical_pixels_raw = s.optical_pixels;
    s.output_line_bytes_raw = s.output_line_bytes;
    s.conseq_pixel_dist = 0;

    if (dev->model->asic_type == AsicType::GL845 ||
        dev->model->asic_type == AsicType::GL846 ||
        dev->model->asic_type == AsicType::GL847)
    {
        if (s.segment_count > 1) {
            s.conseq_pixel_dist = sensor_profile->segment_size;

            // in case of multi-segments sensor, we have to add the width of the sensor crossed by
            // the scan area
            unsigned extra_segment_scan_area = align_multiple_ceil(s.conseq_pixel_dist, 2);
            extra_segment_scan_area *= s.segment_count - 1;
            extra_segment_scan_area *= s.hwdpi_divisor * s.segment_count;
            extra_segment_scan_area *= ccd_pixels_per_system_pixel;

            s.optical_pixels_raw += extra_segment_scan_area;
        }

        s.output_line_bytes_raw = multiply_by_depth_ceil(
                    (s.optical_pixels_raw * s.output_resolution) / sensor.optical_res / s.segment_count,
                    s.params.depth);
    }

    if (dev->model->asic_type == AsicType::GL841) {
        if (dev->model->is_cis) {
            s.output_line_bytes_raw = s.output_channel_bytes;
        }
    }

    if (dev->model->asic_type == AsicType::GL124) {
        if (dev->model->is_cis) {
            s.output_line_bytes_raw = s.output_channel_bytes;
        }
        s.conseq_pixel_dist = s.output_pixels / s.ccd_size_divisor / s.segment_count;
    }

    if (dev->model->asic_type == AsicType::GL843) {
        s.conseq_pixel_dist = s.output_pixels / s.segment_count;
    }

    s.output_segment_pixel_group_count = 0;
    if (dev->model->asic_type == AsicType::GL124 ||
        dev->model->asic_type == AsicType::GL843)
    {
        s.output_segment_pixel_group_count = multiply_by_depth_ceil(
            s.output_pixels / s.ccd_size_divisor / s.segment_count, s.params.depth);
    }
    if (dev->model->asic_type == AsicType::GL845 ||
        dev->model->asic_type == AsicType::GL846 ||
        dev->model->asic_type == AsicType::GL847)
    {
        s.output_segment_pixel_group_count = multiply_by_depth_ceil(
            s.optical_pixels / (s.hwdpi_divisor * s.segment_count * ccd_pixels_per_system_pixel),
            s.params.depth);
    }

    s.output_line_bytes_requested = multiply_by_depth_ceil(
            s.params.get_requested_pixels() * s.params.channels, s.params.depth);

    s.output_total_bytes_raw = s.output_line_bytes_raw * s.output_line_count;
    s.output_total_bytes = s.output_line_bytes * s.output_line_count;

    compute_session_buffer_sizes(dev->model->asic_type, s);
    compute_session_pipeline(dev, s);
    compute_session_pixel_offsets(dev, s, sensor, sensor_profile);
}

static std::size_t get_usb_buffer_read_size(AsicType asic, const ScanSession& session)
{
    switch (asic) {
        case AsicType::GL646:
            // buffer not used on this chip set
            return 1;

        case AsicType::GL124:
            // BUG: we shouldn't multiply by channels here nor divide by ccd_size_divisor
            return session.output_line_bytes_raw / session.ccd_size_divisor * session.params.channels;

        case AsicType::GL846:
        case AsicType::GL847:
            // BUG: we shouldn't multiply by channels here
            return session.output_line_bytes_raw * session.params.channels;

        case AsicType::GL843:
            return session.output_line_bytes_raw * 2;

        default:
            throw SaneException("Unknown asic type");
    }
}

static FakeBufferModel get_fake_usb_buffer_model(const ScanSession& session)
{
    FakeBufferModel model;
    model.push_step(session.buffer_size_read, 1);

    if (session.pipeline_needs_reorder) {
        model.push_step(session.buffer_size_lines, session.output_line_bytes);
    }
    if (session.pipeline_needs_ccd) {
        model.push_step(session.buffer_size_shrink, session.output_line_bytes);
    }
    if (session.pipeline_needs_shrink) {
        model.push_step(session.buffer_size_out, session.output_line_bytes);
    }

    return model;
}

void build_image_pipeline(Genesys_Device* dev, const ScanSession& session)
{
    static unsigned s_pipeline_index = 0;

    s_pipeline_index++;

    auto format = create_pixel_format(session.params.depth,
                                      dev->model->is_cis ? 1 : session.params.channels,
                                      dev->model->line_mode_color_order);
    auto depth = get_pixel_format_depth(format);
    auto width = get_pixels_from_row_bytes(format, session.output_line_bytes_raw);

    auto read_data_from_usb = [dev](std::size_t size, std::uint8_t* data)
    {
        dev->cmd_set->bulk_read_data(dev, 0x45, data, size);
        return true;
    };

    auto lines = session.output_line_count * (dev->model->is_cis ? session.params.channels : 1);

    dev->pipeline.clear();

    // FIXME: here we are complicating things for the time being to preserve the existing behaviour
    // This allows to be sure that the changes to the image pipeline have not introduced
    // regressions.

    if (session.segment_count > 1) {
        // BUG: we're reading one line too much
        dev->pipeline.push_first_node<ImagePipelineNodeBufferedCallableSource>(
                width, lines + 1, format,
                get_usb_buffer_read_size(dev->model->asic_type, session), read_data_from_usb);

        auto output_width = session.output_segment_pixel_group_count * session.segment_count;
        dev->pipeline.push_node<ImagePipelineNodeDesegment>(output_width, dev->segment_order,
                                                            session.conseq_pixel_dist,
                                                            1, 1);
    } else {
        auto read_bytes_left_after_deseg = session.output_line_bytes * session.output_line_count;
        if (dev->model->asic_type == AsicType::GL646) {
            read_bytes_left_after_deseg *= dev->model->is_cis ? session.params.channels : 1;
        }

        dev->pipeline.push_first_node<ImagePipelineNodeBufferedGenesysUsb>(
                width, lines, format, read_bytes_left_after_deseg,
                get_fake_usb_buffer_model(session), read_data_from_usb);
    }

    if (DBG_LEVEL >= DBG_io2) {
        dev->pipeline.push_node<ImagePipelineNodeDebug>("gl_pipeline_" +
                                                        std::to_string(s_pipeline_index) +
                                                        "_0_before_swap.pnm");
    }

    if ((dev->model->flags & GENESYS_FLAG_16BIT_DATA_INVERTED) && depth == 16) {
        dev->pipeline.push_node<ImagePipelineNodeSwap16BitEndian>();
    }

#ifdef WORDS_BIGENDIAN
    if (depth == 16) {
        dev->pipeline.push_node<ImagePipelineNodeSwap16BitEndian>();
    }
#endif

    if (DBG_LEVEL >= DBG_io2) {
        dev->pipeline.push_node<ImagePipelineNodeDebug>("gl_pipeline_" +
                                                        std::to_string(s_pipeline_index) +
                                                        "_1_after_swap.pnm");
    }

    if (dev->model->is_cis && session.params.channels == 3) {
        dev->pipeline.push_node<ImagePipelineNodeMergeMonoLines>(dev->model->line_mode_color_order);
    }

    if (dev->pipeline.get_output_format() == PixelFormat::BGR888) {
        dev->pipeline.push_node<ImagePipelineNodeFormatConvert>(PixelFormat::RGB888);
    }

    if (dev->pipeline.get_output_format() == PixelFormat::BGR161616) {
        dev->pipeline.push_node<ImagePipelineNodeFormatConvert>(PixelFormat::RGB161616);
    }

    if (session.max_color_shift_lines > 0 && session.params.channels == 3) {
        std::size_t shift_r = (dev->ld_shift_r * session.params.yres) / dev->motor.base_ydpi;
        std::size_t shift_g = (dev->ld_shift_g * session.params.yres) / dev->motor.base_ydpi;
        std::size_t shift_b = (dev->ld_shift_b * session.params.yres) / dev->motor.base_ydpi;
        dev->pipeline.push_node<ImagePipelineNodeComponentShiftLines>(shift_r, shift_g, shift_b);
    }

    if (DBG_LEVEL >= DBG_io2) {
        dev->pipeline.push_node<ImagePipelineNodeDebug>("gl_pipeline_" +
                                                        std::to_string(s_pipeline_index) +
                                                        "_2_after_shift.pnm");
    }

    if (session.num_staggered_lines > 0) {
        std::vector<std::size_t> shifts{0, session.num_staggered_lines};
        dev->pipeline.push_node<ImagePipelineNodePixelShiftLines>(shifts);
    }

    if (DBG_LEVEL >= DBG_io2) {
        dev->pipeline.push_node<ImagePipelineNodeDebug>("gl_pipeline_" +
                                                        std::to_string(s_pipeline_index) +
                                                        "_3_after_stagger.pnm");
    }

    if ((dev->model->flags & GENESYS_FLAG_CALIBRATION_HOST_SIDE) &&
        !(dev->model->flags & GENESYS_FLAG_NO_CALIBRATION))
    {
        dev->pipeline.push_node<ImagePipelineNodeCalibrate>(dev->dark_average_data,
                                                            dev->white_average_data);

        if (DBG_LEVEL >= DBG_io2) {
            dev->pipeline.push_node<ImagePipelineNodeDebug>("gl_pipeline_" +
                                                            std::to_string(s_pipeline_index) +
                                                            "_4_after_calibrate.pnm");
        }
    }

    if (session.output_pixels != session.params.get_requested_pixels()) {
        dev->pipeline.push_node<ImagePipelineNodeScaleRows>(session.params.get_requested_pixels());
    }

    auto read_from_pipeline = [dev](std::size_t size, std::uint8_t* out_data)
    {
        (void) size; // will be always equal to dev->pipeline.get_output_row_bytes()
        return dev->pipeline.get_next_row_data(out_data);
    };
    dev->pipeline_buffer = ImageBuffer{dev->pipeline.get_output_row_bytes(),
                                       read_from_pipeline};
}

std::uint8_t compute_frontend_gain_wolfson(float value, float target_value)
{
    /*  the flow of data through the frontend ADC is as follows (see e.g. WM8192 datasheet)
        input
        -> apply offset (o = i + 260mV * (DAC[7:0]-127.5)/127.5) ->
        -> apply gain (o = i * 208/(283-PGA[7:0])
        -> ADC

        Here we have some input data that was acquired with zero gain (PGA==0).
        We want to compute gain such that the output would approach full ADC range (controlled by
        target_value).

        We want to solve the following for {PGA}:

        {value}         = {input} * 208 / (283 - 0)
        {target_value}  = {input} * 208 / (283 - {PGA})

        The solution is the following equation:

        {PGA} = 283 * (1 - {value} / {target_value})
    */
    float gain = value / target_value;
    int code = 283 * (1 - gain);
    return clamp(code, 0, 255);
}

std::uint8_t compute_frontend_gain_analog_devices(float value, float target_value)
{
    /*  The flow of data through the frontend ADC is as follows (see e.g. AD9826 datasheet)
        input
        -> apply offset (o = i + 300mV * (OFFSET[8] ? 1 : -1) * (OFFSET[7:0] / 127)
        -> apply gain (o = i * 6 / (1 + 5 * ( 63 - PGA[5:0] ) / 63 ) )
        -> ADC

        We want to solve the following for {PGA}:

        {value}         = {input} * 6 / (1 + 5 * ( 63 - 0) / 63 ) )
        {target_value}  = {input} * 6 / (1 + 5 * ( 63 - {PGA}) / 63 ) )

        The solution is the following equation:

        {PGA} = (378 / 5) * ({target_value} - {value} / {target_value})
    */
    int code = static_cast<int>((378.0f / 5.0f) * ((target_value - value) / target_value));
    return clamp(code, 0, 63);
}

std::uint8_t compute_frontend_gain(float value, float target_value,
                                   FrontendType frontend_type)
{
    if (frontend_type == FrontendType::WOLFSON) {
        return compute_frontend_gain_wolfson(value, target_value);
    }
    if (frontend_type == FrontendType::ANALOG_DEVICES) {
        return compute_frontend_gain_analog_devices(value, target_value);
    }
    throw SaneException("Unknown frontend to compute gain for");
}

const SensorProfile& get_sensor_profile(AsicType asic_type, const Genesys_Sensor& sensor,
                                        unsigned dpi, unsigned ccd_size_divisor)
{
    int best_i = -1;
    for (unsigned i = 0; i < sensor.sensor_profiles.size(); ++i) {
        // exact match
        if (sensor.sensor_profiles[i].dpi == dpi &&
            sensor.sensor_profiles[i].ccd_size_divisor == ccd_size_divisor)
        {
            return sensor.sensor_profiles[i];
        }
        // closest match
        if (sensor.sensor_profiles[i].ccd_size_divisor == ccd_size_divisor) {
            if (best_i < 0) {
                best_i = i;
            } else {
                if (sensor.sensor_profiles[i].dpi >= dpi &&
                    sensor.sensor_profiles[i].dpi < sensor.sensor_profiles[best_i].dpi)
                {
                    best_i = i;
                }
            }
        }
    }

    // default fallback
    if (best_i < 0) {
        DBG(DBG_warn, "%s: using default sensor profile\n", __func__);
        if (asic_type == AsicType::GL124)
            return *s_fallback_sensor_profile_gl124;
        if (asic_type == AsicType::GL845 || asic_type == AsicType::GL846)
            return *s_fallback_sensor_profile_gl846;
        if (asic_type == AsicType::GL847)
            return *s_fallback_sensor_profile_gl847;
        throw SaneException("Unknown asic type for default profile %d",
                            static_cast<unsigned>(asic_type));
    }

    return sensor.sensor_profiles[best_i];
}


/** @brief initialize device
 * Initialize backend and ASIC : registers, motor tables, and gamma tables
 * then ensure scanner's head is at home. Designed for gl846+ ASICs.
 * Detects cold boot (ie first boot since device plugged) in this case
 * an extensice setup up is done at hardware level.
 *
 * @param dev device to initialize
 * @param max_regs umber of maximum used registers
 */
void sanei_genesys_asic_init(Genesys_Device* dev, bool /*max_regs*/)
{
    DBG_HELPER(dbg);

  uint8_t val;
    bool cold = true;

    // URB    16  control  0xc0 0x0c 0x8e 0x0b len     1 read  0x00 */
    dev->usb_dev.control_msg(REQUEST_TYPE_IN, REQUEST_REGISTER, VALUE_GET_REGISTER, 0x00, 1, &val);

  DBG (DBG_io2, "%s: value=0x%02x\n", __func__, val);
  DBG (DBG_info, "%s: device is %s\n", __func__, (val & 0x08) ? "USB 1.0" : "USB2.0");
  if (val & 0x08)
    {
      dev->usb_mode = 1;
    }
  else
    {
      dev->usb_mode = 2;
    }

    /*  Check if the device has already been initialized and powered up. We read register 0x06 and
        check PWRBIT, if reset scanner has been freshly powered up. This bit will be set to later
        so that following reads can detect power down/up cycle
    */
    if (dev->read_register(0x06) & 0x10) {
        cold = false;
    }
  DBG (DBG_info, "%s: device is %s\n", __func__, cold ? "cold" : "warm");

  /* don't do anything if backend is initialized and hardware hasn't been
   * replug */
  if (dev->already_initialized && !cold)
    {
      DBG (DBG_info, "%s: already initialized, nothing to do\n", __func__);
        return;
    }

    // set up hardware and registers
    dev->cmd_set->asic_boot(dev, cold);

  /* now hardware part is OK, set up device struct */
  dev->white_average_data.clear();
  dev->dark_average_data.clear();

  dev->settings.color_filter = ColorFilter::RED;

  /* duplicate initial values into calibration registers */
  dev->calib_reg = dev->reg;

  const auto& sensor = sanei_genesys_find_sensor_any(dev);

    // Set analog frontend
    dev->cmd_set->set_fe(dev, sensor, AFE_INIT);

    dev->already_initialized = true;

    // Move to home if needed
    dev->cmd_set->slow_back_home(dev, true);
  dev->scanhead_position_in_steps = 0;

    // Set powersaving (default = 15 minutes)
    dev->cmd_set->set_powersaving(dev, 15);
}


void sanei_genesys_set_dpihw(Genesys_Register_Set& regs, const Genesys_Sensor& sensor,
                             unsigned dpihw)
{
    // same across GL646, GL841, GL843, GL846, GL847, GL124
    const uint8_t REG05_DPIHW_MASK = 0xc0;
    const uint8_t REG05_DPIHW_600 = 0x00;
    const uint8_t REG05_DPIHW_1200 = 0x40;
    const uint8_t REG05_DPIHW_2400 = 0x80;
    const uint8_t REG05_DPIHW_4800 = 0xc0;

    if (sensor.register_dpihw_override != 0) {
        dpihw = sensor.register_dpihw_override;
    }

    uint8_t dpihw_setting;
    switch (dpihw) {
        case 600:
            dpihw_setting = REG05_DPIHW_600;
            break;
        case 1200:
            dpihw_setting = REG05_DPIHW_1200;
            break;
        case 2400:
            dpihw_setting = REG05_DPIHW_2400;
            break;
        case 4800:
            dpihw_setting = REG05_DPIHW_4800;
            break;
        default:
            throw SaneException("Unknown dpihw value: %d", dpihw);
    }
    regs.set8_mask(0x05, dpihw_setting, REG05_DPIHW_MASK);
}

/**
 * Wait for the scanning head to park
 */
void sanei_genesys_wait_for_home(Genesys_Device* dev)
{
    DBG_HELPER(dbg);
  uint8_t val;

  /* clear the parking status whatever the outcome of the function */
    dev->parking = false;

    // read initial status, if head isn't at home and motor is on we are parking, so we wait.
    // gl847/gl124 need 2 reads for reliable results
    val = sanei_genesys_get_status(dev);
  sanei_genesys_sleep_ms(10);
    val = sanei_genesys_get_status(dev);

  /* if at home, return */
  if(val & HOMESNR)
    {
	  DBG (DBG_info,
	       "%s: already at home\n", __func__);
        return;
    }

    unsigned timeout_ms = 200000;
    unsigned elapsed_ms = 0;
  do
    {
      sanei_genesys_sleep_ms(100);
        elapsed_ms += 100;

        val = sanei_genesys_get_status(dev);

          if (DBG_LEVEL >= DBG_io2)
            {
              sanei_genesys_print_status (val);
            }
    } while (elapsed_ms < timeout_ms && !(val & HOMESNR));

  /* if after the timeout, head is still not parked, error out */
    if (elapsed_ms >= timeout_ms && !(val & HOMESNR)) {
        DBG (DBG_error, "%s: failed to reach park position in %dseconds\n", __func__,
             timeout_ms / 1000);
        throw SaneException(SANE_STATUS_IO_ERROR, "failed to reach park position");
    }
}

/** @brief motor profile
 * search for the database of motor profiles and get the best one. Each
 * profile is at full step and at a reference exposure. Use first entry
 * by default.
 * @param motors motor profile database
 * @param motor_type motor id
 * @param exposure exposure time
 * @return a pointer to a Motor_Profile struct
 */
Motor_Profile* sanei_genesys_get_motor_profile(Motor_Profile *motors, MotorId motor_id,
                                               int exposure)
{
  unsigned int i;
  int idx;

  i=0;
  idx=-1;
  while(motors[i].exposure!=0)
    {
        // exact match
        if (motors[i].motor_id == motor_id && motors[i].exposure==exposure) {
          return &(motors[i]);
        }

        // closest match
        if (motors[i].motor_id == motor_id) {
          /* if profile exposure is higher than the required one,
           * the entry is a candidate for the closest match */
          if(motors[i].exposure>=exposure)
            {
              if(idx<0)
                {
                  /* no match found yet */
                  idx=i;
                }
              else
                {
                  /* test for better match */
                  if(motors[i].exposure<motors[idx].exposure)
                    {
                      idx=i;
                    }
                }
            }
        }
      i++;
    }

  /* default fallback */
  if(idx<0)
    {
      DBG (DBG_warn,"%s: using default motor profile\n",__func__);
      idx=0;
    }

  return &(motors[idx]);
}

/**@brief compute motor step type to use
 * compute the step type (full, half, quarter, ...) to use based
 * on target resolution
 * @return 0 for full step
 *         1 for half step
 *         2 for quarter step
 *         3 for eighth step
 */
StepType sanei_genesys_compute_step_type(Motor_Profile* motors, MotorId motor_id, int exposure)
{
Motor_Profile *profile;

    profile = sanei_genesys_get_motor_profile(motors, motor_id, exposure);
    return profile->step_type;
}

/** @brief generate slope table
 * Generate the slope table to use for the scan using a reference slope
 * table.
 * @param slope pointer to the slope table to fill
 * @param steps pointer to return used step number
 * @param dpi   desired motor resolution
 * @param exposure exposure used
 * @param base_dpi base resolution of the motor
 * @param step_type step type used for scan
 * @param factor shrink factor for the slope
 * @param motor_type motor id
 * @param motors motor profile database
 */
int sanei_genesys_slope_table(std::vector<uint16_t>& slope,
                              int* steps, int dpi, int exposure, int base_dpi, StepType step_type,
                              int factor, MotorId motor_id, Motor_Profile* motors)
{
int sum, i;
uint16_t target,current;
Motor_Profile *profile;

    unsigned step_shift = static_cast<unsigned>(step_type);
    slope.clear();

	/* required speed */
    target = ((exposure * dpi) / base_dpi) >> step_shift;
        DBG (DBG_io2, "%s: exposure=%d, dpi=%d, target=%d\n", __func__, exposure, dpi, target);

	/* fill result with target speed */
    slope.resize(SLOPE_TABLE_SIZE, target);

        profile=sanei_genesys_get_motor_profile(motors, motor_id, exposure);

	/* use profile to build table */
        i=0;
	sum=0;

        /* first step is always used unmodified */
        current=profile->table[0];

        /* loop on profile copying and apply step type */
        while(profile->table[i]!=0 && current>=target)
          {
            slope[i]=current;
            sum+=slope[i];
            i++;
        current = profile->table[i] >> step_shift;
          }

        /* ensure last step is required speed in case profile doesn't contain it */
        if(current!=0 && current<target)
          {
            slope[i]=target;
            sum+=slope[i];
            i++;
          }

        /* range checking */
        if(profile->table[i]==0 && DBG_LEVEL >= DBG_warn && current>target)
          {
            DBG (DBG_warn,"%s: short slope table, failed to reach %d. target too low ?\n",__func__,target);
          }
        if(i<3 && DBG_LEVEL >= DBG_warn)
          {
            DBG (DBG_warn,"%s: short slope table, failed to reach %d. target too high ?\n",__func__,target);
          }

        /* align on factor */
        while(i%factor!=0)
          {
            slope[i+1]=slope[i];
            sum+=slope[i];
            i++;
          }

        /* ensure minimal slope size */
        while(i<2*factor)
          {
            slope[i+1]=slope[i];
            sum+=slope[i];
            i++;
          }

        // return used steps and taken time
        *steps=i/factor;
	return sum;
}

/** @brief returns the lowest possible ydpi for the device
 * Parses device entry to find lowest motor dpi.
 * @param dev device description
 * @return lowest motor resolution
 */
int sanei_genesys_get_lowest_ydpi(Genesys_Device *dev)
{
    return *std::min_element(dev->model->ydpi_values.begin(), dev->model->ydpi_values.end());
}

/** @brief returns the lowest possible dpi for the device
 * Parses device entry to find lowest motor or sensor dpi.
 * @param dev device description
 * @return lowest motor resolution
 */
int sanei_genesys_get_lowest_dpi(Genesys_Device *dev)
{
    return std::min(*std::min_element(dev->model->xdpi_values.begin(),
                                      dev->model->xdpi_values.end()),
                    *std::min_element(dev->model->ydpi_values.begin(),
                                      dev->model->ydpi_values.end()));
}

/** @brief check is a cache entry may be used
 * Compares current settings with the cache entry and return
 * true if they are compatible.
 * A calibration cache is compatible if color mode and x dpi match the user
 * requested scan. In the case of CIS scanners, dpi isn't a criteria.
 * flatbed cache entries are considred too old and then expires if they
 * are older than the expiration time option, forcing calibration at least once
 * then given time. */
bool sanei_genesys_is_compatible_calibration(Genesys_Device * dev, const Genesys_Sensor& sensor,
                                             Genesys_Calibration_Cache * cache, int for_overwrite)
{
    DBG_HELPER(dbg);
#ifdef HAVE_SYS_TIME_H
  struct timeval time;
#endif
    int compatible = 1;

    if (dev->cmd_set->has_calculate_current_setup()) {
      DBG (DBG_proc, "%s: no calculate_setup, non compatible cache\n", __func__);
      return false;
    }

    dev->cmd_set->calculate_current_setup(dev, sensor);

  DBG (DBG_proc, "%s: checking\n", __func__);

  /* a calibration cache is compatible if color mode and x dpi match the user
   * requested scan. In the case of CIS scanners, dpi isn't a criteria */
    if (!dev->model->is_cis) {
        compatible = (dev->settings.xres == static_cast<int>(cache->used_setup.xres));
    }
  else
    {
        compatible = (sensor.get_register_hwdpi(dev->settings.xres) ==
                      sensor.get_register_hwdpi(cache->used_setup.xres));
    }
  DBG (DBG_io, "%s: after resolution check current compatible=%d\n", __func__, compatible);
  if (dev->current_setup.ccd_size_divisor != cache->used_setup.ccd_size_divisor)
    {
      DBG (DBG_io, "%s: ccd_size_divisor=%d, used=%d\n", __func__,
           dev->current_setup.ccd_size_divisor, cache->used_setup.ccd_size_divisor);
      compatible = 0;
    }
  if (dev->session.params.scan_method != cache->params.scan_method)
    {
      DBG (DBG_io, "%s: current method=%d, used=%d\n", __func__,
           static_cast<unsigned>(dev->session.params.scan_method),
           static_cast<unsigned>(cache->params.scan_method));
      compatible = 0;
    }
  if (!compatible)
    {
      DBG (DBG_proc, "%s: completed, non compatible cache\n", __func__);
      return false;
    }

  /* a cache entry expires after afetr expiration time for non sheetfed scanners */
  /* this is not taken into account when overwriting cache entries    */
#ifdef HAVE_SYS_TIME_H
    if (!for_overwrite && dev->settings.expiration_time >=0)
    {
        gettimeofday(&time, nullptr);
      if ((time.tv_sec - cache->last_calibration > dev->settings.expiration_time*60)
          && !dev->model->is_sheetfed
          && (dev->settings.scan_method == ScanMethod::FLATBED))
        {
          DBG (DBG_proc, "%s: expired entry, non compatible cache\n", __func__);
          return false;
        }
    }
#endif

  return true;
}


/** @brief compute maximum line distance shift
 * compute maximum line distance shift for the motor and sensor
 * combination. Line distance shift is the distance between different
 * color component of CCD sensors. Since these components aren't at
 * the same physical place, they scan diffrent lines. Software must
 * take this into account to accurately mix color data.
 * @param dev device session to compute max_shift for
 * @param channels number of color channels for the scan
 * @param yres motor resolution used for the scan
 * @param flags scan flags
 * @return 0 or line distance shift
 */
int sanei_genesys_compute_max_shift(Genesys_Device *dev,
                                    int channels,
                                    int yres,
                                    int flags)
{
  int max_shift;

  max_shift=0;
  if (channels > 1 && !(flags & SCAN_FLAG_IGNORE_LINE_DISTANCE))
    {
      max_shift = dev->ld_shift_r;
      if (dev->ld_shift_b > max_shift)
	max_shift = dev->ld_shift_b;
      if (dev->ld_shift_g > max_shift)
	max_shift = dev->ld_shift_g;
      max_shift = (max_shift * yres) / dev->motor.base_ydpi;
    }
  return max_shift;
}

/** @brief build lookup table for digital enhancements
 * Function to build a lookup table (LUT), often
   used by scanners to implement brightness/contrast/gamma
   or by backends to speed binarization/thresholding

   offset and slope inputs are -127 to +127

   slope rotates line around central input/output val,
   0 makes horizontal line

       pos           zero          neg
       .       x     .             .  x
       .      x      .             .   x
   out .     x       .xxxxxxxxxxx  .    x
       .    x        .             .     x
       ....x.......  ............  .......x....
            in            in            in

   offset moves line vertically, and clamps to output range
   0 keeps the line crossing the center of the table

       high           low
       .   xxxxxxxx   .
       . x            .
   out x              .          x
       .              .        x
       ............   xxxxxxxx....
            in             in

   out_min/max provide bounds on output values,
   useful when building thresholding lut.
   0 and 255 are good defaults otherwise.
  * @param lut pointer where to store the generated lut
  * @param in_bits number of bits for in values
  * @param out_bits number of bits of out values
  * @param out_min minimal out value
  * @param out_max maximal out value
  * @param slope slope of the generated data
  * @param offset offset of the generated data
  */
void sanei_genesys_load_lut(unsigned char* lut,
                            int in_bits, int out_bits,
                            int out_min, int out_max,
                            int slope, int offset)
{
    DBG_HELPER(dbg);
  int i, j;
  double shift, rise;
  int max_in_val = (1 << in_bits) - 1;
  int max_out_val = (1 << out_bits) - 1;
  uint8_t *lut_p8 = lut;
    uint16_t* lut_p16 = reinterpret_cast<std::uint16_t*>(lut);

  /* slope is converted to rise per unit run:
   * first [-127,127] to [-.999,.999]
   * then to [-PI/4,PI/4] then [0,PI/2]
   * then take the tangent (T.O.A)
   * then multiply by the normal linear slope
   * because the table may not be square, i.e. 1024x256*/
    rise = std::tan(static_cast<double>(slope) / 128 * M_PI_4 + M_PI_4) * max_out_val / max_in_val;

  /* line must stay vertically centered, so figure
   * out vertical offset at central input value */
    shift = static_cast<double>(max_out_val) / 2 - (rise * max_in_val / 2);

  /* convert the user offset setting to scale of output
   * first [-127,127] to [-1,1]
   * then to [-max_out_val/2,max_out_val/2]*/
    shift += static_cast<double>(offset) / 127 * max_out_val / 2;

  for (i = 0; i <= max_in_val; i++)
    {
      j = rise * i + shift;

      /* cap data to required range */
      if (j < out_min)
	{
	  j = out_min;
	}
      else if (j > out_max)
	{
	  j = out_max;
	}

      /* copy result according to bit depth */
      if (out_bits <= 8)
	{
	  *lut_p8 = j;
	  lut_p8++;
	}
      else
	{
	  *lut_p16 = j;
	  lut_p16++;
	}
    }
}

void sanei_genesys_usleep(unsigned int useconds)
{
    if (sanei_usb_is_replay_mode_enabled()) {
        return;
    }
  usleep(useconds);
}

void sanei_genesys_sleep_ms(unsigned int milliseconds)
{
  sanei_genesys_usleep(milliseconds * 1000);
}

static std::unique_ptr<std::vector<std::function<void()>>> s_functions_run_at_backend_exit;

void add_function_to_run_at_backend_exit(std::function<void()> function)
{
    if (!s_functions_run_at_backend_exit)
        s_functions_run_at_backend_exit.reset(new std::vector<std::function<void()>>());
    s_functions_run_at_backend_exit->push_back(std::move(function));
}

void run_functions_at_backend_exit()
{
    for (auto it = s_functions_run_at_backend_exit->rbegin();
         it != s_functions_run_at_backend_exit->rend(); ++it)
    {
        (*it)();
    }
    s_functions_run_at_backend_exit.reset();
}

void debug_dump(unsigned level, const Genesys_Settings& settings)
{
    DBG(level, "Genesys_Settings:\n"
        "Resolution X/Y : %u / %u dpi\n"
        "Lines : %u\n"
        "Pixels per line : %u\n"
        "Pixels per line (requested) : %u\n"
        "Depth : %u\n"
        "Start position X/Y : %.3f/%.3f\n"
        "Scan mode : %d\n\n",
        settings.xres, settings.yres,
        settings.lines, settings.pixels, settings.requested_pixels, settings.depth,
        settings.tl_x, settings.tl_y,
        static_cast<unsigned>(settings.scan_mode));
}

void debug_dump(unsigned level, const SetupParams& params)
{
    DBG(level, "SetupParams:\n"
        "Resolution X/Y : %u / %u dpi\n"
        "Lines : %u\n"
        "Pixels per line : %u\n"
        "Pixels per line (requested) : %u\n"
        "Depth : %u\n"
        "Channels : %u\n"
        "Start position X/Y : %g / %g\n"
        "Scan mode : %d\n"
        "Color filter : %d\n"
        "Flags : %x\n",
        params.xres, params.yres,
        params.lines, params.pixels, params.requested_pixels,
        params.depth, params.channels,
        params.startx, params.starty,
        static_cast<unsigned>(params.scan_mode),
        static_cast<unsigned>(params.color_filter),
        params.flags);
}

void debug_dump(unsigned level, const ScanSession& session)
{
    DBG(level, "session:\n");
    DBG(level, "    computed : %d\n", session.computed);
    DBG(level, "    hwdpi_divisor : %d\n", session.hwdpi_divisor);
    DBG(level, "    ccd_size_divisor : %d\n", session.ccd_size_divisor);
    DBG(level, "    optical_resolution : %d\n", session.optical_resolution);
    DBG(level, "    optical_pixels : %d\n", session.optical_pixels);
    DBG(level, "    optical_pixels_raw : %d\n", session.optical_pixels_raw);
    DBG(level, "    output_resolution : %d\n", session.output_resolution);
    DBG(level, "    output_pixels : %d\n", session.output_pixels);
    DBG(level, "    output_line_bytes : %d\n", session.output_line_bytes);
    DBG(level, "    output_line_bytes_raw : %d\n", session.output_line_bytes_raw);
    DBG(level, "    output_line_count : %d\n", session.output_line_count);
    DBG(level, "    num_staggered_lines : %d\n", session.num_staggered_lines);
    DBG(level, "    max_color_shift_lines : %d\n", session.max_color_shift_lines);
    DBG(level, "    enable_ledadd : %d\n", session.enable_ledadd);
    DBG(level, "    segment_count : %d\n", session.segment_count);
    DBG(level, "    pixel_startx : %d\n", session.pixel_startx);
    DBG(level, "    pixel_endx : %d\n", session.pixel_endx);
    DBG(level, "    conseq_pixel_dist : %d\n", session.conseq_pixel_dist);
    DBG(level, "    output_segment_pixel_group_count : %d\n",
        session.output_segment_pixel_group_count);
    DBG(level, "    buffer_size_read : %zu\n", session.buffer_size_read);
    DBG(level, "    buffer_size_read : %zu\n", session.buffer_size_lines);
    DBG(level, "    buffer_size_shrink : %zu\n", session.buffer_size_shrink);
    DBG(level, "    buffer_size_out : %zu\n", session.buffer_size_out);
    DBG(level, "    filters:%s%s%s\n",
        session.pipeline_needs_reorder ? " reorder" : "",
        session.pipeline_needs_ccd ? " ccd" : "",
        session.pipeline_needs_shrink ? " shrink" : "");
    debug_dump(level, session.params);
}

void debug_dump(unsigned level, const Genesys_Current_Setup& setup)
{
    DBG(level, "current_setup:\n"
        "Pixels: %d\n"
        "Lines: %d\n"
        "exposure_time: %d\n"
        "Resolution X: %g\n"
        "ccd_size_divisor: %d\n"
        "stagger: %d\n"
        "max_shift: %d\n",
        setup.pixels,
        setup.lines,
        setup.exposure_time,
        setup.xres,
        setup.ccd_size_divisor,
        setup.stagger,
        setup.max_shift);
}

void debug_dump(unsigned level, const Genesys_Register_Set& regs)
{
    DBG(level, "register_set:\n");
    for (const auto& reg : regs) {
        DBG(level, "    %04x=%02x\n", reg.address, reg.value);
    }
}

void debug_dump(unsigned level, const GenesysRegisterSettingSet& regs)
{
    DBG(level, "register_setting_set:\n");
    for (const auto& reg : regs) {
        DBG(level, "    %04x=%02x & %02x\n", reg.address, reg.value, reg.mask);
    }
}

void debug_dump(unsigned level, const Genesys_Sensor& sensor)
{
    DBG(level, "sensor:\n");
    DBG(level, "    sensor_id : %d\n", static_cast<unsigned>(sensor.sensor_id));
    DBG(level, "    optical_res : %d\n", sensor.optical_res);

    DBG(level, "    resolutions :");
    if (sensor.resolutions.matches_any()) {
        DBG(level, " ANY\n");
    } else {
        for (unsigned resolution : sensor.resolutions.resolutions()) {
            DBG(level, " %d", resolution);
        }
        DBG(level, "\n");
    }

    DBG(level, "    method : %d\n", static_cast<unsigned>(sensor.method));
    DBG(level, "    ccd_size_divisor : %d\n", sensor.ccd_size_divisor);
    DBG(level, "    black_pixels : %d\n", sensor.black_pixels);
    DBG(level, "    dummy_pixel : %d\n", sensor.dummy_pixel);
    DBG(level, "    ccd_start_xoffset : %d\n", sensor.ccd_start_xoffset);
    DBG(level, "    sensor_pixels : %d\n", sensor.sensor_pixels);
    DBG(level, "    fau_gain_white_ref : %d\n", sensor.fau_gain_white_ref);
    DBG(level, "    gain_white_ref : %d\n", sensor.gain_white_ref);
    DBG(level, "    exposure.red : %d\n", sensor.exposure.red);
    DBG(level, "    exposure.green : %d\n", sensor.exposure.green);
    DBG(level, "    exposure.blue : %d\n", sensor.exposure.blue);
    DBG(level, "    exposure_lperiod : %d\n", sensor.exposure_lperiod);
    DBG(level, "    custom_regs\n");
    debug_dump(level, sensor.custom_regs);
    DBG(level, "    custom_fe_regs\n");
    debug_dump(level, sensor.custom_fe_regs);
    DBG(level, "    gamma.red : %f\n", sensor.gamma[0]);
    DBG(level, "    gamma.green : %f\n", sensor.gamma[1]);
    DBG(level, "    gamma.blue : %f\n", sensor.gamma[2]);
}
