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

#include "genesys_sanei.h"

#if WITH_USB_RECORD_REPLAY
#include <libxml/tree.h>
#endif

UsbDevice::~UsbDevice()
{
    if (is_open()) {
        DBG(DBG_error, "UsbDevice not closed; closing automatically");
        close();
    }
}

void UsbDevice::open(const char* dev_name)
{
    DBG_HELPER(dbg);

    if (is_open()) {
        throw SaneException("device already open");
    }
    int device_num = 0;

    dbg.status("open device");
    TIE(sanei_usb_open(dev_name, &device_num));

    name_ = dev_name;
    device_num_ = device_num;
    is_open_ = true;
}

void UsbDevice::clear_halt()
{
    DBG_HELPER(dbg);
    assert_is_open();
    TIE(sanei_usb_clear_halt(device_num_));
}

void UsbDevice::reset()
{
    DBG_HELPER(dbg);
    assert_is_open();
    TIE(sanei_usb_reset(device_num_));
}

void UsbDevice::close()
{
    DBG_HELPER(dbg);
    assert_is_open();

    // we can't do much if closing fails, so we close the device on our side regardless of the
    // function succeeds
    int device_num = device_num_;

    set_not_open();
    sanei_usb_close(device_num);
}

void UsbDevice::get_vendor_product(int& vendor, int& product)
{
    DBG_HELPER(dbg);
    assert_is_open();
    TIE(sanei_usb_get_vendor_product(device_num_, &vendor, &product));
}

void UsbDevice::control_msg(int rtype, int reg, int value, int index, int length, uint8_t* data)
{
    DBG_HELPER(dbg);
    assert_is_open();
    TIE(sanei_usb_control_msg(device_num_, rtype, reg, value, index, length, data));
}

void UsbDevice::bulk_read(uint8_t* buffer, size_t* size)
{
    DBG_HELPER(dbg);
    assert_is_open();
    TIE(sanei_usb_read_bulk(device_num_, buffer, size));
}

void UsbDevice::bulk_write(const uint8_t* buffer, size_t* size)
{
    DBG_HELPER(dbg);
    assert_is_open();
    TIE(sanei_usb_write_bulk(device_num_, buffer, size));
}

void UsbDevice::assert_is_open() const
{
    if (!is_open()) {
        throw SaneException("device not open");
    }
}

void UsbDevice::set_not_open()
{
    device_num_ = 0;
    is_open_ = false;
    name_ = "";
}

extern "C" {
    typedef enum {
        sanei_usb_testing_mode_disabled = 0,
        sanei_usb_testing_mode_record,
        sanei_usb_testing_mode_replay,
    }
    sanei_usb_testing_mode;

    extern sanei_usb_testing_mode testing_mode;
} // extern "C"

#if WITH_USB_RECORD_REPLAY

// from sanei_usb.c
#define FAIL_TEST(func, ...)                                                   \
  do {                                                                         \
    DBG(1, "%s: FAIL: ", func);                                                \
    DBG(1, __VA_ARGS__);                                                       \
    fail_test();                                                               \
  } while (0)

#define FAIL_TEST_TX(func, node, ...)                                          \
  do {                                                                         \
    sanei_xml_print_seq_if_any(node, func);                                    \
    DBG(1, "%s: FAIL: ", func);                                                \
    DBG(1, __VA_ARGS__);                                                       \
    fail_test();                                                               \
  } while (0)

extern "C" {
    void fail_test();
    xmlNode* sanei_xml_get_next_tx_node();
    int sanei_xml_is_known_commands_end(xmlNode* node);
    void sanei_xml_print_seq_if_any(xmlNode* node, const char* parent_fun);
    void sanei_xml_set_uint_attr(xmlNode* node, const char* attr_name,
                                 unsigned attr_value);
    xmlNode* sanei_xml_append_command(xmlNode* sibling,
                                      int indent, xmlNode* e_command);
    void sanei_xml_record_seq(xmlNode* node);
    void sanei_xml_break_if_needed(xmlNode* node);
    int sanei_usb_check_attr(xmlNode* node, const char* attr_name,
                             const char* expected, const char* parent_fun);

    extern xmlNode* testing_append_commands_node;
    extern unsigned testing_last_known_seq;
    extern int testing_development_mode;
    extern int testing_known_commands_input_failed;
} // extern "C"

static void sanei_usb_record_debug_msg(xmlNode* node, SANE_String_Const message)
{
  int node_was_null = node == NULL;
  if (node_was_null)
    node = testing_append_commands_node;

  xmlNode* e_tx = xmlNewNode(NULL, (const xmlChar*)"debug");
  sanei_xml_set_uint_attr(e_tx, "seq", ++testing_last_known_seq);
  xmlNewProp(e_tx, (const xmlChar*)"message", (const xmlChar*)message);

  node = sanei_xml_append_command(node, node_was_null, e_tx);

  if (node_was_null)
    testing_append_commands_node = node;
}

static void sanei_usb_record_replace_debug_msg(xmlNode* node, SANE_String_Const message)
{
  if (!testing_development_mode)
    return;

  testing_last_known_seq--;
  sanei_usb_record_debug_msg(node, message);
  xmlUnlinkNode(node);
  xmlFreeNode(node);
}

static void sanei_usb_replay_debug_msg(SANE_String_Const message)
{
  if (testing_known_commands_input_failed)
    return;

  xmlNode* node = sanei_xml_get_next_tx_node();
  if (node == NULL)
    {
      FAIL_TEST(__func__, "no more transactions\n");
      return;
    }

  if (sanei_xml_is_known_commands_end(node))
    {
      sanei_usb_record_debug_msg(NULL, message);
      return;
    }

  sanei_xml_record_seq(node);
  sanei_xml_break_if_needed(node);

  if (xmlStrcmp(node->name, (const xmlChar*)"debug") != 0)
    {
      FAIL_TEST_TX(__func__, node, "unexpected transaction type %s\n",
                   (const char*) node->name);
      sanei_usb_record_replace_debug_msg(node, message);
    }

  if (!sanei_usb_check_attr(node, "message", message, __func__))
    {
      sanei_usb_record_replace_debug_msg(node, message);
    }
}

void sanei_usb_testing_record_message(SANE_String_Const message)
{
  if (testing_mode == sanei_usb_testing_mode_record)
    {
      sanei_usb_record_debug_msg(NULL, message);
    }
  if (testing_mode == sanei_usb_testing_mode_replay)
    {
      sanei_usb_replay_debug_msg(message);
    }
}
#else
void sanei_usb_testing_record_message(SANE_String_Const message)
{
    (void) message;
}
#endif
