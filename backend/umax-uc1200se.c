
/* ------------------------------------------------------------------------- */

/* umax-uc1200se.c: inquiry for UMAX scanner uc1200se

   (C) 1998-2002 Oliver Rauch

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.

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

/* ------------------------------------------------------------------------- */
#include "umax-scanner.h"
/* ------------------------------------------------------------------------- */

static unsigned char UC1200SE_INQUIRY[] =
{
#define UC1200SE_INQUIRY_LEN 0x9d
/* 24 F/W support function */
	0x03,

/* 25 -27 exposure-times */
	0x00, 0x00, 0x00,

/* 28 - 29 reserved */
	0x00, 0x00,

/* 2a - 35 exposure times */
	0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00,

/* 36 - 37 reserved */
	0x00, 0x00,

/* 38 - 5f scsi reserved */
	00, 00, 00, 00, 00, 00, 00, 00,
	00, 00, 00, 00, 00, 00, 00, 00,
	00, 00, 00, 00, 00, 00, 00, 00,
	00, 00, 00, 00, 00, 00, 00, 00,
	00, 00, 00, 00, 00, 00, 00, 00,

/* 60 -62 scanner capability*/
	0x3e,
	0x0c,
	0x03,

/* 63 reserved */
	0x00,

/* 64 gamma */
	0x83,

/* 65 reserved */
	0x00,

/* 66 GIB */
	0x04,

/* 67 reserved */
	0x00,

/* 68 GOB */
	0x01,

/* 69 - 6a halftone */
	0x00, 0x2f,

/* 6b - 6c reserved */
	0x00, 0x00,

/* 6d color sequence */
	0xe1, /* 0xe8, 0xe9 or 0xe1 ? */

/* 6e - 71 video memory */
	0x00, 0x20, 0x00, 0x00,

/* 72 reserved */
	0x00,

/* 73 max optical res in 100 dpi */
	0x06,

/* 74 max x_res in 100 dpi */
	0x06,

/* 75 max y_res in 100 dpi */
	0x0c,

/* 76-77 fb max scan width in 0.01 inch */
	0x03, 0x20,

/* 78-79 fb max scan length in 0.01 inch */
	0x04, 0x4c,

/* 7a-7b uta x original point, may be 0x05, 0x89 */
	0x00, 0x76,

/* 7c-7d uta y original point, may be 0x06, 0x75 */
	0x00, 0x89,

/* 7e-7f uta max scan width in 0.01 inch */
	0x02, 0x4e,

/* 80-81 uta max scan length in 0.01 inch */
	0x03, 0x65,

/* 82-85 reserved */
	00, 00, 00, 00,

/* 86-87 dor x original point */
	0x00, 0x00,

/* 88-89 dor x original point */
	0x00, 0x00,

/* 8a-8b dor max scan width in 0.01 inch */
	0x00, 0x00,

/* 8c-8d dor max scan length in 0.01 inch */
	0x00, 0x00,

/* 8e reserved */
	0x00,

/* 8f last calibration lamp density */
	0x00,

/* 90 reserved */
	0x00,

/* 91 lamp warmup max time */
	0x00,

/* 92-93 window descriptor block length */
	0x00, 0x31,

/* 94 optical resolution residue (1dpi) */
	0x00,

/* 95 x_resolution residue (1dpi) */
	0x00,

/* 96 y_resolution residue (1dpi) */
	0x00,

/* 97 analog gamma table */
	0x00,

/* 98-99 reserved */
	0x00, 0x00,

/* 9a max calibration data lines */
	0x00,

/* 9b fb/uta colour-sequnce-mode */
	0x01,

/* 9c adf colour-sequnce-mode */
	0x01,

/* 9d line-distance of ccd */
	0x08
};

static inquiry_blk inquiry_uc1200se =
{
  "UC1200SE ",UC1200SE_INQUIRY, UC1200SE_INQUIRY_LEN
};
