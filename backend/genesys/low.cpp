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
#include "test_settings.h"

#include "gl124_registers.h"
#include "gl646_registers.h"
#include "gl841_registers.h"
#include "gl843_registers.h"
#include "gl846_registers.h"
#include "gl847_registers.h"
#include "gl646_registers.h"

#include <cstdio>
#include <cmath>
#include <vector>

/* ------------------------------------------------------------------------ */
/*                  functions calling ASIC specific functions               */
/* ------------------------------------------------------------------------ */

namespace genesys {

/**
 * setup the hardware dependent functions
 */

namespace gl124 { std::unique_ptr<CommandSet> create_gl124_cmd_set(); }
namespace gl646 { std::unique_ptr<CommandSet> create_gl646_cmd_set(); }
namespace gl841 { std::unique_ptr<CommandSet> create_gl841_cmd_set(); }
namespace gl843 { std::unique_ptr<CommandSet> create_gl843_cmd_set(); }
namespace gl846 { std::unique_ptr<CommandSet> create_gl846_cmd_set(); }
namespace gl847 { std::unique_ptr<CommandSet> create_gl847_cmd_set(); }

void sanei_genesys_init_cmd_set(Genesys_Device* dev)
{
  DBG_INIT ();
    DBG_HELPER(dbg);
    switch (dev->model->asic_type) {
        case AsicType::GL646: dev->cmd_set = gl646::create_gl646_cmd_set(); break;
        case AsicType::GL841: dev->cmd_set = gl841::create_gl841_cmd_set(); break;
        case AsicType::GL843: dev->cmd_set = gl843::create_gl843_cmd_set(); break;
        case AsicType::GL845: // since only a few reg bits differs we handle both together
        case AsicType::GL846: dev->cmd_set = gl846::create_gl846_cmd_set(); break;
        case AsicType::GL847: dev->cmd_set = gl847::create_gl847_cmd_set(); break;
        case AsicType::GL124: dev->cmd_set = gl124::create_gl124_cmd_set(); break;
        default: throw SaneException(SANE_STATUS_INVAL, "unknown ASIC type");
    }
}

/* ------------------------------------------------------------------------ */
/*                  General IO and debugging functions                      */
/* ------------------------------------------------------------------------ */

void sanei_genesys_write_file(const char* filename, const std::uint8_t* data, std::size_t length)
{
    DBG_HELPER(dbg);
    std::FILE* out = std::fopen(filename, "w");
    if (!out) {
        throw SaneException("could not open %s for writing: %s", filename, strerror(errno));
    }
    std::fwrite(data, 1, length, out);
    std::fclose(out);
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

    dev->interface->write_register(0x2b, (addr & 0xff));

  addr = addr >> 8;
    dev->interface->write_register(0x2a, (addr & 0xff));
}

/* ------------------------------------------------------------------------ */
/*                       Medium level functions                             */
/* ------------------------------------------------------------------------ */

Status scanner_read_status(Genesys_Device& dev)
{
    DBG_HELPER(dbg);
    std::uint16_t address = 0;

    switch (dev.model->asic_type) {
        case AsicType::GL124: address = 0x101; break;
        case AsicType::GL646:
        case AsicType::GL841:
        case AsicType::GL843:
        case AsicType::GL845:
        case AsicType::GL846:
        case AsicType::GL847: address = 0x41; break;
        default: throw SaneException("Unsupported asic type");
    }

    // same for all chips
    constexpr std::uint8_t PWRBIT = 0x80;
    constexpr std::uint8_t BUFEMPTY	= 0x40;
    constexpr std::uint8_t FEEDFSH = 0x20;
    constexpr std::uint8_t SCANFSH = 0x10;
    constexpr std::uint8_t HOMESNR = 0x08;
    constexpr std::uint8_t LAMPSTS = 0x04;
    constexpr std::uint8_t FEBUSY = 0x02;
    constexpr std::uint8_t MOTORENB	= 0x01;

    auto value = dev.interface->read_register(address);
    Status status;
    status.is_replugged = !(value & PWRBIT);
    status.is_buffer_empty = value & BUFEMPTY;
    status.is_feeding_finished = value & FEEDFSH;
    status.is_scanning_finished = value & SCANFSH;
    status.is_at_home = value & HOMESNR;
    status.is_lamp_on = value & LAMPSTS;
    status.is_front_end_busy = value & FEBUSY;
    status.is_motor_enabled = value & MOTORENB;

    if (DBG_LEVEL >= DBG_io) {
        debug_print_status(dbg, status);
    }

    return status;
}

Status scanner_read_reliable_status(Genesys_Device& dev)
{
    DBG_HELPER(dbg);

    scanner_read_status(dev);
    dev.interface->sleep_ms(100);
    return scanner_read_status(dev);
}

void scanner_read_print_status(Genesys_Device& dev)
{
    scanner_read_status(dev);
}

/**
 * decodes and prints content of status register
 * @param val value read from status register
 */
void debug_print_status(DebugMessageHelper& dbg, Status val)
{
    std::stringstream str;
    str << val;
    dbg.vlog(DBG_info, "status=%s\n", str.str().c_str());
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
            *words = dev->interface->read_register(0x102) & 0x03;
            *words = *words * 256 + dev->interface->read_register(0x103);
            *words = *words * 256 + dev->interface->read_register(0x104);
            *words = *words * 256 + dev->interface->read_register(0x105);
            break;

    case AsicType::GL845:
    case AsicType::GL846:
            *words = dev->interface->read_register(0x42) & 0x02;
            *words = *words * 256 + dev->interface->read_register(0x43);
            *words = *words * 256 + dev->interface->read_register(0x44);
            *words = *words * 256 + dev->interface->read_register(0x45);
            break;

    case AsicType::GL847:
            *words = dev->interface->read_register(0x42) & 0x03;
            *words = *words * 256 + dev->interface->read_register(0x43);
            *words = *words * 256 + dev->interface->read_register(0x44);
            *words = *words * 256 + dev->interface->read_register(0x45);
            break;

    default:
            *words = dev->interface->read_register(0x44);
            *words += dev->interface->read_register(0x43) * 256;
            if (dev->model->asic_type == AsicType::GL646) {
                *words += ((dev->interface->read_register(0x42) & 0x03) * 256 * 256);
            } else {
                *words += ((dev->interface->read_register(0x42) & 0x0f) * 256 * 256);
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
        *words = (dev->interface->read_register(0x10b) & 0x0f) << 16;
        *words += (dev->interface->read_register(0x10c) << 8);
        *words += dev->interface->read_register(0x10d);
    }
  else
    {
        *words = dev->interface->read_register(0x4d);
        *words += dev->interface->read_register(0x4c) * 256;
        if (dev->model->asic_type == AsicType::GL646) {
            *words += ((dev->interface->read_register(0x4b) & 0x03) * 256 * 256);
        } else {
            *words += ((dev->interface->read_register(0x4b) & 0x0f) * 256 * 256);
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

    dev->interface->sleep_ms(1);

    auto status = scanner_read_status(*dev);

    if (status.is_buffer_empty) {
      /* fix timing issue on USB3 (or just may be too fast) hardware
       * spotted by John S. Weber <jweber53@gmail.com>
       */
        dev->interface->sleep_ms(1);
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
            scanner_read_status(*dev);
        }

        bool empty = sanei_genesys_is_buffer_empty(dev);
        dev->interface->sleep_ms(10);
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
        dev->interface->sleep_ms(sleep_time_ms);
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

    dev->interface->bulk_read_data(0x45, data, size);
}

Image read_unshuffled_image_from_scanner(Genesys_Device* dev, const ScanSession& session,
                                         std::size_t total_bytes)
{
    DBG_HELPER(dbg);

    auto format = create_pixel_format(session.params.depth,
                                      dev->model->is_cis ? 1 : session.params.channels,
                                      dev->model->line_mode_color_order);

    auto width = get_pixels_from_row_bytes(format, session.output_line_bytes_raw);
    auto height = session.optical_line_count;

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

    if (session.segment_count > 1) {
        auto output_width = session.output_segment_pixel_group_count * session.segment_count;
        pipeline.push_node<ImagePipelineNodeDesegment>(output_width, dev->segment_order,
                                                       session.conseq_pixel_dist,
                                                       1, 1);
    }

    if (has_flag(dev->model->flags, ModelFlag::INVERTED_16BIT_DATA) && session.params.depth == 16) {
        pipeline.push_node<ImagePipelineNodeSwap16BitEndian>();
    }

#ifdef WORDS_BIGENDIAN
    if (session.params.depth == 16) {
        pipeline.push_node<ImagePipelineNodeSwap16BitEndian>();
    }
#endif

    if (dev->model->is_cis && session.params.channels == 3) {
        pipeline.push_node<ImagePipelineNodeMergeMonoLines>(dev->model->line_mode_color_order);
    }

    if (pipeline.get_output_format() == PixelFormat::BGR888) {
        pipeline.push_node<ImagePipelineNodeFormatConvert>(PixelFormat::RGB888);
    }

    if (pipeline.get_output_format() == PixelFormat::BGR161616) {
        pipeline.push_node<ImagePipelineNodeFormatConvert>(PixelFormat::RGB161616);
    }

    return pipeline.get_image();
}

void sanei_genesys_read_feed_steps(Genesys_Device* dev, unsigned int* steps)
{
    DBG_HELPER(dbg);

    if (dev->model->asic_type == AsicType::GL124) {
        *steps = (dev->interface->read_register(0x108) & 0x1f) << 16;
        *steps += (dev->interface->read_register(0x109) << 8);
        *steps += dev->interface->read_register(0x10a);
    }
  else
    {
        *steps = dev->interface->read_register(0x4a);
        *steps += dev->interface->read_register(0x49) * 256;
        if (dev->model->asic_type == AsicType::GL646) {
            *steps += ((dev->interface->read_register(0x48) & 0x03) * 256 * 256);
        } else if (dev->model->asic_type == AsicType::GL841) {
            *steps += ((dev->interface->read_register(0x48) & 0x0f) * 256 * 256);
        } else {
            *steps += ((dev->interface->read_register(0x48) & 0x1f) * 256 * 256);
        }
    }

  DBG(DBG_proc, "%s: %d steps\n", __func__, *steps);
}

void sanei_genesys_set_lamp_power(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                  Genesys_Register_Set& regs, bool set)
{
    static const uint8_t REG_0x03_LAMPPWR = 0x10;

    if (set) {
        regs.find_reg(0x03).value |= REG_0x03_LAMPPWR;

        if (dev->model->asic_type == AsicType::GL841) {
            regs_set_exposure(dev->model->asic_type, regs,
                              sanei_genesys_fixup_exposure(sensor.exposure));
            regs.set8(0x19, 0x50);
        }

        if (dev->model->asic_type == AsicType::GL843) {
            regs_set_exposure(dev->model->asic_type, regs, sensor.exposure);

            // we don't actually turn on lamp on infrared scan
            if ((dev->model->model_id == ModelId::CANON_8400F ||
                 dev->model->model_id == ModelId::CANON_8600F ||
                 dev->model->model_id == ModelId::PLUSTEK_OPTICFILM_7200I ||
                 dev->model->model_id == ModelId::PLUSTEK_OPTICFILM_7500I) &&
                dev->settings.scan_method == ScanMethod::TRANSPARENCY_INFRARED)
            {
                regs.find_reg(0x03).value &= ~REG_0x03_LAMPPWR;
            }
        }
    } else {
        regs.find_reg(0x03).value &= ~REG_0x03_LAMPPWR;

        if (dev->model->asic_type == AsicType::GL841) {
            regs_set_exposure(dev->model->asic_type, regs, {0x0101, 0x0101, 0x0101});
            regs.set8(0x19, 0xff);
        }
    }
    regs.state.is_lamp_on = set;
}

void sanei_genesys_set_motor_power(Genesys_Register_Set& regs, bool set)
{
    static const uint8_t REG_0x02_MTRPWR = 0x10;

    if (set) {
        regs.find_reg(0x02).value |= REG_0x02_MTRPWR;
    } else {
        regs.find_reg(0x02).value &= ~REG_0x02_MTRPWR;
    }
    regs.state.is_motor_on = set;
}

bool should_enable_gamma(const ScanSession& session, const Genesys_Sensor& sensor)
{
    if ((session.params.flags & ScanFlag::DISABLE_GAMMA) != ScanFlag::NONE) {
        return false;
    }
    if (sensor.gamma[0] == 1.0f || sensor.gamma[1] == 1.0f || sensor.gamma[2] == 1.0f) {
        return false;
    }
    if (session.params.depth == 16)
        return false;

    return true;
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
        uint8_t val = dev->interface->read_register(0xbd);
        val &= ~(0x01 << i);
        dev->interface->write_register(0xbd, val);

        // clear corresponding GMM_F bit
        val = dev->interface->read_register(0xbe);
      val &= ~(0x01 << i);
        dev->interface->write_register(0xbe, val);

      // FIXME: currently the last word of each gamma table is not initialied, so to work around
      // unstable data, just set it to 0 which is the most likely value of uninitialized memory
      // (proper value is probably 0xff)
      gamma[size * 2 * i + size * 2 - 2] = 0;
      gamma[size * 2 * i + size * 2 - 1] = 0;

      /* set GMM_Z */
        dev->interface->write_register(0xc5+2*i, gamma[size*2*i+1]);
        dev->interface->write_register(0xc6+2*i, gamma[size*2*i]);

        dev->interface->write_ahb(0x01000000 + 0x200 * i, (size-1) * 2,
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
                                   const Genesys_Sensor& sensor)
{
    unsigned ccd_pixels_per_system_pixel = sensor.ccd_pixels_per_system_pixel();

    if (dev->model->asic_type == AsicType::GL646) {

        // startx cannot be below dummy pixel value
        s.pixel_startx = sensor.dummy_pixel;
        if (has_flag(s.params.flags, ScanFlag::USE_XCORRECTION) && sensor.ccd_start_xoffset > 0) {
            s.pixel_startx = sensor.ccd_start_xoffset;
        }
        s.pixel_startx += s.params.startx * sensor.optical_res / s.params.xres;

        if (sensor.stagger_config.stagger_at_resolution(s.params.xres, s.params.yres) > 0) {
            s.pixel_startx |= 1;
        }

        s.pixel_endx = s.pixel_startx + s.optical_pixels;

        s.pixel_startx /= sensor.ccd_pixels_per_system_pixel() * s.ccd_size_divisor;
        s.pixel_endx /= sensor.ccd_pixels_per_system_pixel() * s.ccd_size_divisor;

    } else if (dev->model->asic_type == AsicType::GL841) {
        unsigned startx = s.params.startx * sensor.optical_res / s.params.xres;

        s.pixel_startx = ((sensor.ccd_start_xoffset + startx) * s.optical_resolution)
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
        const uint8_t REG_0x01_SHDAREA = 0x02;
        if ((dev->reg.find_reg(0x01).value & REG_0x01_SHDAREA) != 0) {
            unsigned average_factor = s.optical_resolution / s.params.xres;
            s.pixel_startx = align_multiple_floor(s.pixel_startx, average_factor);
        }

        s.pixel_endx = s.pixel_startx + s.optical_pixels;

    } else if (dev->model->asic_type == AsicType::GL843) {
        unsigned startx = s.params.startx * sensor.optical_res / s.params.xres;

        s.pixel_startx = (startx + sensor.dummy_pixel) / ccd_pixels_per_system_pixel;
        s.pixel_endx = s.pixel_startx + s.optical_pixels / ccd_pixels_per_system_pixel;

        s.pixel_startx /= s.hwdpi_divisor;
        s.pixel_endx /= s.hwdpi_divisor;

        // in case of stagger we have to start at an odd coordinate
        bool stagger_starts_even = false;
        if (dev->model->model_id == ModelId::CANON_4400F ||
            dev->model->model_id == ModelId::CANON_8400F)
        {
            stagger_starts_even = true;
        }

        if (s.num_staggered_lines > 0) {
            if (!stagger_starts_even && (s.pixel_startx & 1) == 0) {
                s.pixel_startx++;
                s.pixel_endx++;
            } else if (stagger_starts_even && (s.pixel_startx & 1) != 0) {
                s.pixel_startx++;
                s.pixel_endx++;
            }
        }

    } else if (dev->model->asic_type == AsicType::GL845 ||
               dev->model->asic_type == AsicType::GL846 ||
               dev->model->asic_type == AsicType::GL847)
    {
        unsigned startx = s.params.startx * sensor.optical_res / s.params.xres;

        s.pixel_startx = startx;

        if (s.num_staggered_lines > 0) {
            s.pixel_startx |= 1;
        }

        s.pixel_startx += sensor.ccd_start_xoffset * ccd_pixels_per_system_pixel;
        s.pixel_endx = s.pixel_startx + s.optical_pixels_raw;

        s.pixel_startx /= s.hwdpi_divisor * s.segment_count * ccd_pixels_per_system_pixel;
        s.pixel_endx /= s.hwdpi_divisor * s.segment_count * ccd_pixels_per_system_pixel;

    } else if (dev->model->asic_type == AsicType::GL124) {
        unsigned startx = s.params.startx * sensor.optical_res / s.params.xres;

        s.pixel_startx = startx;

        if (s.num_staggered_lines > 0) {
            s.pixel_startx |= 1;
        }

        s.pixel_startx /= ccd_pixels_per_system_pixel;
        // FIXME: should we add sensor.dummy_pxel to pixel_startx at this point?
        s.pixel_endx = s.pixel_startx + s.optical_pixels / ccd_pixels_per_system_pixel;

        s.pixel_startx /= s.hwdpi_divisor * s.segment_count;
        s.pixel_endx /= s.hwdpi_divisor * s.segment_count;

        std::uint32_t segcnt = (sensor.custom_regs.get_value(gl124::REG_SEGCNT) << 16) +
                               (sensor.custom_regs.get_value(gl124::REG_SEGCNT + 1) << 8) +
                                sensor.custom_regs.get_value(gl124::REG_SEGCNT + 2);
        if (s.pixel_endx == segcnt) {
            s.pixel_endx = 0;
        }
    }

    s.pixel_count_multiplier = sensor.pixel_count_multiplier;

    s.pixel_startx *= sensor.pixel_count_multiplier;
    s.pixel_endx *= sensor.pixel_count_multiplier;
}

void compute_session(const Genesys_Device* dev, ScanSession& s, const Genesys_Sensor& sensor)
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

    s.num_staggered_lines = 0;
    if (!has_flag(s.params.flags, ScanFlag::IGNORE_LINE_DISTANCE))
    {
        s.num_staggered_lines = sensor.stagger_config.stagger_at_resolution(s.params.xres,
                                                                            s.params.yres);
    }

    s.color_shift_lines_r = dev->model->ld_shift_r;
    s.color_shift_lines_g = dev->model->ld_shift_g;
    s.color_shift_lines_b = dev->model->ld_shift_b;

    if (dev->model->motor_id == MotorId::G4050 && s.params.yres > 600) {
        // it seems base_dpi of the G4050 motor is changed above 600 dpi
        s.color_shift_lines_r = (s.color_shift_lines_r * 3800) / dev->motor.base_ydpi;
        s.color_shift_lines_g = (s.color_shift_lines_g * 3800) / dev->motor.base_ydpi;
        s.color_shift_lines_b = (s.color_shift_lines_b * 3800) / dev->motor.base_ydpi;
    }

    s.color_shift_lines_r = (s.color_shift_lines_r * s.params.yres) / dev->motor.base_ydpi;
    s.color_shift_lines_g = (s.color_shift_lines_g * s.params.yres) / dev->motor.base_ydpi;
    s.color_shift_lines_b = (s.color_shift_lines_b * s.params.yres) / dev->motor.base_ydpi;

    s.max_color_shift_lines = 0;
    if (s.params.channels > 1 && !has_flag(s.params.flags, ScanFlag::IGNORE_LINE_DISTANCE)) {
        s.max_color_shift_lines = std::max(s.color_shift_lines_r, std::max(s.color_shift_lines_g,
                                                                           s.color_shift_lines_b));
    }

    s.output_line_count = s.params.lines + s.max_color_shift_lines + s.num_staggered_lines;
    s.optical_line_count = dev->model->is_cis ? s.output_line_count * s.params.channels
                                              : s.output_line_count;

    s.output_channel_bytes = multiply_by_depth_ceil(s.output_pixels, s.params.depth);
    s.output_line_bytes = s.output_channel_bytes * s.params.channels;

    s.segment_count = sensor.get_segment_count();

    s.optical_pixels_raw = s.optical_pixels;
    s.output_line_bytes_raw = s.output_line_bytes;
    s.conseq_pixel_dist = 0;

    if (dev->model->asic_type == AsicType::GL845 ||
        dev->model->asic_type == AsicType::GL846 ||
        dev->model->asic_type == AsicType::GL847)
    {
        if (s.segment_count > 1) {
            s.conseq_pixel_dist = sensor.segment_size;

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
        s.output_segment_pixel_group_count = s.output_pixels /
                (s.ccd_size_divisor * s.segment_count);
    }
    if (dev->model->asic_type == AsicType::GL845 ||
        dev->model->asic_type == AsicType::GL846 ||
        dev->model->asic_type == AsicType::GL847)
    {
        s.output_segment_pixel_group_count = s.optical_pixels /
                (s.hwdpi_divisor * s.segment_count * ccd_pixels_per_system_pixel);
    }

    s.output_line_bytes_requested = multiply_by_depth_ceil(
            s.params.get_requested_pixels() * s.params.channels, s.params.depth);

    s.output_total_bytes_raw = s.output_line_bytes_raw * s.output_line_count;
    s.output_total_bytes = s.output_line_bytes * s.output_line_count;

    compute_session_buffer_sizes(dev->model->asic_type, s);
    compute_session_pipeline(dev, s);
    compute_session_pixel_offsets(dev, s, sensor);

    if (dev->model->asic_type == AsicType::GL124 ||
        dev->model->asic_type == AsicType::GL845 ||
        dev->model->asic_type == AsicType::GL846)
    {
        s.enable_ledadd = (s.params.channels == 1 && dev->model->is_cis && dev->settings.true_gray);
    }

    s.use_host_side_calib = sensor.use_host_side_calib;

    if (dev->model->asic_type == AsicType::GL841 ||
        dev->model->asic_type == AsicType::GL843)
    {
        // no 16 bit gamma for this ASIC
        if (s.params.depth == 16) {
            s.params.flags |= ScanFlag::DISABLE_GAMMA;
        }
    }

    s.computed = true;

    DBG(DBG_info, "%s ", __func__);
    debug_dump(DBG_info, s);
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

        case AsicType::GL845:
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

void build_image_pipeline(Genesys_Device* dev, const Genesys_Sensor& sensor,
                          const ScanSession& session)
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
        dev->interface->bulk_read_data(0x45, data, size);
        return true;
    };

    auto lines = session.optical_line_count;

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

    if (has_flag(dev->model->flags, ModelFlag::INVERTED_16BIT_DATA) && depth == 16) {
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
        dev->pipeline.push_node<ImagePipelineNodeComponentShiftLines>(
                    session.color_shift_lines_r,
                    session.color_shift_lines_g,
                    session.color_shift_lines_b);
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

    if (session.use_host_side_calib &&
        !has_flag(dev->model->flags, ModelFlag::NO_CALIBRATION) &&
        !has_flag(session.params.flags, ScanFlag::DISABLE_SHADING))
    {
        unsigned pixel_shift = session.params.startx;
        if (dev->model->model_id == ModelId::CANON_4400F) {
            pixel_shift =
                    session.params.startx * sensor.optical_res / dev->calib_session.params.xres;
        }
        dev->pipeline.push_node<ImagePipelineNodeCalibrate>(dev->dark_average_data,
                                                            dev->white_average_data,
                                                            pixel_shift *
                                                                dev->calib_session.params.channels);

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
    int code = static_cast<int>(283 * (1 - gain));
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
    dev->interface->get_usb_device().control_msg(REQUEST_TYPE_IN, REQUEST_REGISTER,
                                                 VALUE_GET_REGISTER, 0x00, 1, &val);

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
    if (!is_testing_mode()) {
        if (dev->interface->read_register(0x06) & 0x10) {
            cold = false;
        }
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

    dev->initial_regs = dev->reg;

  const auto& sensor = sanei_genesys_find_sensor_any(dev);

    // Set analog frontend
    dev->cmd_set->set_fe(dev, sensor, AFE_INIT);

    dev->already_initialized = true;

    // Move to home if needed
    if (dev->model->model_id == ModelId::CANON_8600F) {
        if (!dev->cmd_set->is_head_home(*dev, ScanHeadId::SECONDARY)) {
            dev->set_head_pos_unknown(ScanHeadId::SECONDARY);
        }
        if (!dev->cmd_set->is_head_home(*dev, ScanHeadId::PRIMARY)) {
            dev->set_head_pos_unknown(ScanHeadId::SECONDARY);
        }
    }
    dev->cmd_set->move_back_home(dev, true);

    // Set powersaving (default = 15 minutes)
    dev->cmd_set->set_powersaving(dev, 15);
}

void scanner_start_action(Genesys_Device& dev, bool start_motor)
{
    DBG_HELPER(dbg);
    switch (dev.model->asic_type) {
        case AsicType::GL646:
        case AsicType::GL841:
        case AsicType::GL843:
        case AsicType::GL845:
        case AsicType::GL846:
        case AsicType::GL847:
        case AsicType::GL124:
            break;
        default:
            throw SaneException("Unsupported chip");
    }

    if (start_motor) {
        dev.interface->write_register(0x0f, 0x01);
    } else {
        dev.interface->write_register(0x0f, 0);
    }
}

void sanei_genesys_set_dpihw(Genesys_Register_Set& regs, const Genesys_Sensor& sensor,
                             unsigned dpihw)
{
    // same across GL646, GL841, GL843, GL846, GL847, GL124
    const uint8_t REG_0x05_DPIHW_MASK = 0xc0;
    const uint8_t REG_0x05_DPIHW_600 = 0x00;
    const uint8_t REG_0x05_DPIHW_1200 = 0x40;
    const uint8_t REG_0x05_DPIHW_2400 = 0x80;
    const uint8_t REG_0x05_DPIHW_4800 = 0xc0;

    if (sensor.register_dpihw_override != 0) {
        dpihw = sensor.register_dpihw_override;
    }

    uint8_t dpihw_setting;
    switch (dpihw) {
        case 600:
            dpihw_setting = REG_0x05_DPIHW_600;
            break;
        case 1200:
            dpihw_setting = REG_0x05_DPIHW_1200;
            break;
        case 2400:
            dpihw_setting = REG_0x05_DPIHW_2400;
            break;
        case 4800:
            dpihw_setting = REG_0x05_DPIHW_4800;
            break;
        default:
            throw SaneException("Unknown dpihw value: %d", dpihw);
    }
    regs.set8_mask(0x05, dpihw_setting, REG_0x05_DPIHW_MASK);
}

void regs_set_exposure(AsicType asic_type, Genesys_Register_Set& regs,
                       const SensorExposure& exposure)
{
    switch (asic_type) {
        case AsicType::GL124: {
            regs.set24(gl124::REG_EXPR, exposure.red);
            regs.set24(gl124::REG_EXPG, exposure.green);
            regs.set24(gl124::REG_EXPB, exposure.blue);
            break;
        }
        case AsicType::GL646: {
            regs.set16(gl646::REG_EXPR, exposure.red);
            regs.set16(gl646::REG_EXPG, exposure.green);
            regs.set16(gl646::REG_EXPB, exposure.blue);
            break;
        }
        case AsicType::GL841: {
            regs.set16(gl841::REG_EXPR, exposure.red);
            regs.set16(gl841::REG_EXPG, exposure.green);
            regs.set16(gl841::REG_EXPB, exposure.blue);
            break;
        }
        case AsicType::GL843: {
            regs.set16(gl843::REG_EXPR, exposure.red);
            regs.set16(gl843::REG_EXPG, exposure.green);
            regs.set16(gl843::REG_EXPB, exposure.blue);
            break;
        }
        case AsicType::GL845:
        case AsicType::GL846: {
            regs.set16(gl846::REG_EXPR, exposure.red);
            regs.set16(gl846::REG_EXPG, exposure.green);
            regs.set16(gl846::REG_EXPB, exposure.blue);
            break;
        }
        case AsicType::GL847: {
            regs.set16(gl847::REG_EXPR, exposure.red);
            regs.set16(gl847::REG_EXPG, exposure.green);
            regs.set16(gl847::REG_EXPB, exposure.blue);
            break;
        }
        default:
            throw SaneException("Unsupported asic");
    }
}

void regs_set_optical_off(AsicType asic_type, Genesys_Register_Set& regs)
{
    DBG_HELPER(dbg);
    switch (asic_type) {
        case AsicType::GL646: {
            regs.find_reg(gl646::REG_0x01).value &= ~gl646::REG_0x01_SCAN;
            break;
        }
        case AsicType::GL841: {
            regs.find_reg(gl841::REG_0x01).value &= ~gl841::REG_0x01_SCAN;
            break;
        }
        case AsicType::GL843: {
            regs.find_reg(gl843::REG_0x01).value &= ~gl843::REG_0x01_SCAN;
            break;
        }
        case AsicType::GL845:
        case AsicType::GL846: {
            regs.find_reg(gl846::REG_0x01).value &= ~gl846::REG_0x01_SCAN;
            break;
        }
        case AsicType::GL847: {
            regs.find_reg(gl847::REG_0x01).value &= ~gl847::REG_0x01_SCAN;
            break;
        }
        case AsicType::GL124: {
            regs.find_reg(gl124::REG_0x01).value &= ~gl124::REG_0x01_SCAN;
            break;
        }
        default:
            throw SaneException("Unsupported asic");
    }
}

bool get_registers_gain4_bit(AsicType asic_type, const Genesys_Register_Set& regs)
{
    switch (asic_type) {
        case AsicType::GL646:
            return static_cast<bool>(regs.get8(gl646::REG_0x06) & gl646::REG_0x06_GAIN4);
        case AsicType::GL841:
            return static_cast<bool>(regs.get8(gl841::REG_0x06) & gl841::REG_0x06_GAIN4);
        case AsicType::GL843:
            return static_cast<bool>(regs.get8(gl843::REG_0x06) & gl843::REG_0x06_GAIN4);
        case AsicType::GL845:
        case AsicType::GL846:
            return static_cast<bool>(regs.get8(gl846::REG_0x06) & gl846::REG_0x06_GAIN4);
        case AsicType::GL847:
            return static_cast<bool>(regs.get8(gl847::REG_0x06) & gl847::REG_0x06_GAIN4);
        case AsicType::GL124:
            return static_cast<bool>(regs.get8(gl124::REG_0x06) & gl124::REG_0x06_GAIN4);
        default:
            throw SaneException("Unsupported chipset");
    }
}

/**
 * Wait for the scanning head to park
 */
void sanei_genesys_wait_for_home(Genesys_Device* dev)
{
    DBG_HELPER(dbg);

  /* clear the parking status whatever the outcome of the function */
    dev->parking = false;

    if (is_testing_mode()) {
        return;
    }

    // read initial status, if head isn't at home and motor is on we are parking, so we wait.
    // gl847/gl124 need 2 reads for reliable results
    auto status = scanner_read_status(*dev);
    dev->interface->sleep_ms(10);
    status = scanner_read_status(*dev);

    if (status.is_at_home) {
	  DBG (DBG_info,
	       "%s: already at home\n", __func__);
        return;
    }

    unsigned timeout_ms = 200000;
    unsigned elapsed_ms = 0;
  do
    {
      dev->interface->sleep_ms(100);
        elapsed_ms += 100;

        status = scanner_read_status(*dev);
    } while (elapsed_ms < timeout_ms && !status.is_at_home);

  /* if after the timeout, head is still not parked, error out */
    if (elapsed_ms >= timeout_ms && !status.is_at_home) {
        DBG (DBG_error, "%s: failed to reach park position in %dseconds\n", __func__,
             timeout_ms / 1000);
        throw SaneException(SANE_STATUS_IO_ERROR, "failed to reach park position");
    }
}

const MotorProfile* get_motor_profile_ptr(const std::vector<MotorProfile>& profiles,
                                          unsigned exposure,
                                          const ScanSession& session)
{
    int best_i = -1;

    for (unsigned i = 0; i < profiles.size(); ++i) {
        const auto& profile = profiles[i];

        if (!profile.resolutions.matches(session.params.yres)) {
            continue;
        }
        if (!profile.scan_methods.matches(session.params.scan_method)) {
            continue;
        }

        if (profile.max_exposure == exposure) {
            return &profile;
        }

        if (profile.max_exposure == 0 || profile.max_exposure >= exposure) {
            if (best_i < 0) {
                // no match found yet
                best_i = i;
            } else {
                // test for better match
                if (profiles[i].max_exposure < profiles[best_i].max_exposure) {
                    best_i = i;
                }
            }
        }
    }

    if (best_i < 0) {
        return nullptr;
    }

    return &profiles[best_i];
}

const MotorProfile& get_motor_profile(const std::vector<MotorProfile>& profiles,
                                      unsigned exposure,
                                      const ScanSession& session)
{
    const auto* profile = get_motor_profile_ptr(profiles, exposure, session);
    if (profile == nullptr) {
        throw SaneException("Motor slope is not configured");
    }

    return *profile;
}

MotorSlopeTable sanei_genesys_slope_table(AsicType asic_type, int dpi, int exposure, int base_dpi,
                                          unsigned step_multiplier,
                                          const MotorProfile& motor_profile)
{
    unsigned target_speed_w = ((exposure * dpi) / base_dpi);

    auto table = create_slope_table(motor_profile.slope, target_speed_w, motor_profile.step_type,
                                    step_multiplier, 2 * step_multiplier,
                                    get_slope_table_max_size(asic_type));
    return table;
}

MotorSlopeTable create_slope_table_fastest(AsicType asic_type, unsigned step_multiplier,
                                           const MotorProfile& motor_profile)
{
    return create_slope_table(motor_profile.slope, motor_profile.slope.max_speed_w,
                              motor_profile.step_type,
                              step_multiplier, 2 * step_multiplier,
                              get_slope_table_max_size(asic_type));
}

/** @brief returns the lowest possible ydpi for the device
 * Parses device entry to find lowest motor dpi.
 * @param dev device description
 * @return lowest motor resolution
 */
int sanei_genesys_get_lowest_ydpi(Genesys_Device *dev)
{
    const auto& resolution_settings = dev->model->get_resolution_settings(dev->settings.scan_method);
    return resolution_settings.get_min_resolution_y();
}

/** @brief returns the lowest possible dpi for the device
 * Parses device entry to find lowest motor or sensor dpi.
 * @param dev device description
 * @return lowest motor resolution
 */
int sanei_genesys_get_lowest_dpi(Genesys_Device *dev)
{
    const auto& resolution_settings = dev->model->get_resolution_settings(dev->settings.scan_method);
    return std::min(resolution_settings.get_min_resolution_x(),
                    resolution_settings.get_min_resolution_y());
}

/** @brief check is a cache entry may be used
 * Compares current settings with the cache entry and return
 * true if they are compatible.
 * A calibration cache is compatible if color mode and x dpi match the user
 * requested scan. In the case of CIS scanners, dpi isn't a criteria.
 * flatbed cache entries are considred too old and then expires if they
 * are older than the expiration time option, forcing calibration at least once
 * then given time. */
bool sanei_genesys_is_compatible_calibration(Genesys_Device* dev,
                                             const ScanSession& session,
                                             const Genesys_Calibration_Cache* cache,
                                             bool for_overwrite)
{
    DBG_HELPER(dbg);
#ifdef HAVE_SYS_TIME_H
  struct timeval time;
#endif

    bool compatible = true;

    const auto& dev_params = session.params;

    if (dev_params.scan_method != cache->params.scan_method) {
        dbg.vlog(DBG_io, "incompatible: scan_method %d vs. %d\n",
                 static_cast<unsigned>(dev_params.scan_method),
                 static_cast<unsigned>(cache->params.scan_method));
        compatible = false;
    }

    if (dev_params.xres != cache->params.xres) {
        dbg.vlog(DBG_io, "incompatible: params.xres %d vs. %d\n",
                 dev_params.xres, cache->params.xres);
        compatible = false;
    }

    if (dev_params.yres != cache->params.yres) {
        // exposure depends on selected sensor and we select the sensor according to yres
        dbg.vlog(DBG_io, "incompatible: params.yres %d vs. %d\n",
                 dev_params.yres, cache->params.yres);
        compatible = false;
    }

    if (dev_params.channels != cache->params.channels) {
        // exposure depends on total number of pixels at least on gl841
        dbg.vlog(DBG_io, "incompatible: params.channels %d vs. %d\n",
                 dev_params.channels, cache->params.channels);
        compatible = false;
    }

    if (dev_params.startx != cache->params.startx) {
        // exposure depends on total number of pixels at least on gl841
        dbg.vlog(DBG_io, "incompatible: params.startx %d vs. %d\n",
                 dev_params.startx, cache->params.startx);
        compatible = false;
    }

    if (dev_params.pixels != cache->params.pixels) {
        // exposure depends on total number of pixels at least on gl841
        dbg.vlog(DBG_io, "incompatible: params.pixels %d vs. %d\n",
                 dev_params.pixels, cache->params.pixels);
        compatible = false;
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
    auto pi_4 = M_PI / 4.0;
    rise = std::tan(static_cast<double>(slope) / 128 * pi_4 + pi_4) * max_out_val / max_in_val;

  /* line must stay vertically centered, so figure
   * out vertical offset at central input value */
    shift = static_cast<double>(max_out_val) / 2 - (rise * max_in_val / 2);

  /* convert the user offset setting to scale of output
   * first [-127,127] to [-1,1]
   * then to [-max_out_val/2,max_out_val/2]*/
    shift += static_cast<double>(offset) / 127 * max_out_val / 2;

  for (i = 0; i <= max_in_val; i++)
    {
        j = static_cast<int>(rise * i + shift);

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

} // namespace genesys
