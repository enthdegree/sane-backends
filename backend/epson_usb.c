#include <sys/types.h>
#include <sane/sanei_usb.h>
#include "epson_usb.h"


SANE_Word sanei_epson_usb_product_ids [] =
{
	0x101, 	/* Perfection 636 */
	0x103, 	/* Perfection 610 */
	0x10c, 	/* Perfection 640 */
	0x104, 	/* Perfection 1200 */
	0x10b, 	/* Perfection 1240 */
	0x106, 	/* Stylus Scan 2500 */
	0x10a,	/* Perfection 1640 */
	0x107, 	/* Expression 1600 */
	0x10e, 	/* Expression 1680 */
	0x110, 	/* Perfection 1650 */
	0x112, 	/* Perfection 2450 */
	0x11b, 	/* Perfection 2400 */
	0x11c,	/* GT-9800  / Perfection 3200 */
	0x11e, 	/* Perfection 1660 */
	0x801,	/* CX-5200 */
	0x802,	/* CX-3200 */
	0x807,	/* RX-500 */
	0	/* last entry - this is used for devices that are specified 
		   in the config file as "usb <vendor> <product>" */
};



int sanei_epson_getNumberOfUSBProductIds(void)
{
	return sizeof(sanei_epson_usb_product_ids)/sizeof(SANE_Word);
}
