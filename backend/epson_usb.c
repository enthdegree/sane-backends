#include <sys/types.h>
#include <sane/sanei_usb.h>
#include "epson_usb.h"


SANE_Word sanei_epson_usb_product_ids[] = {
  0x101,			/* GT-7000 / Perfection 636 */
  0x103,			/* GT-6600 / Perfection 610 */
  0x10c,			/* GT-6700 / Perfection 640 */
  0x104,			/* GT-7600 / Perfection 1200 */
  0x10b,			/* GT-7700 / Perfection 1240 */
  0x106,			/* Stylus Scan 2500 */
  0x109,			/* ES-8500 / Expression 1640 XL */
  0x10a,			/* GT-8700 / Perfection 1640 */
  0x107,			/* ES-2000 / Expression 1600 */
  0x10e,			/* ES-2200 / Expression 1680 */
  0x110,			/* GT-8200 / Perfection 1650 */
  0x112,			/* GT-9700 / Perfection 2450 */
  0x116,			/* GT-9400 / Perfection 3170 */
  0x11b,			/* GT-9300 / Perfection 2400 */
  0x11c,			/* GT-9800 / Perfection 3200 */
  0x11e,			/* GT-8300 / Perfection 1660 */
  0x126,			/* ES-7000 / GT-15000 */
  0x128,			/* GT-X700 / Perfection 4870 */
  0x801,			/* CC-600 / CX-5[1234]00 */
  0x802,			/* CC-570 / CX-3[12]00 */
  0x806,			/* PM-A850 / RX600 */
  0x807,			/* RX-500 */
  0x808,			/* CX-5400 */
  0				/* last entry - this is used for devices that are specified 
				   in the config file as "usb <vendor> <product>" */
};



int
sanei_epson_getNumberOfUSBProductIds (void)
{
  return sizeof (sanei_epson_usb_product_ids) / sizeof (SANE_Word);
}
