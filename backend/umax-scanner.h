/* -------------------------------------------------------------------- */

/* umax-scanner.h: scanner-definiton header-file for UMAX scanner driver.
  
   (C) 1997-2000 Oliver Rauch

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

/* -------------------------------------------------------------------- */

#ifndef UMAX_SCANNER_H
#define UMAX_SCANNER_H


/* ==================================================================== */


typedef struct
{
  char *scanner;
  unsigned char *inquiry;
  int inquiry_len;
} inquiry_blk;


/* ==================================================================== */

/* scanners that are supported because the driver knows missing */
/* inquiry-data */

#include "umax-uc630.h"
#include "umax-uc840.h"
#include "umax-ug630.h"
#include "umax-ug80.h"
#include "umax-uc1200s.h"
#include "umax-uc1200se.h"
#include "umax-uc1260.h"

static inquiry_blk *inquiry_table[] =
{
  &inquiry_uc630,
  &inquiry_uc840,
  &inquiry_ug630,
  &inquiry_ug80,
  &inquiry_uc1200s,
  &inquiry_uc1200se,
  &inquiry_uc1260
};

#define known_inquiry 7

/* ==================================================================== */

/* names of scanners that are supported because */
/* the inquiry_return_block is ok and driver is tested */

static char *scanner_str[] =
{
  "UMAX ",	"Vista-T630 ",
  "UMAX ",	"Vista-S6 ",
  "UMAX ",	"Vista-S6E ",
  "UMAX ",	"UMAX S-6E ",
  "UMAX ",	"UMAX S-6EG ",
  "UMAX ",	"Vista-S8 ",
  "UMAX ",	"UMAX S-12 ",
  "UMAX ",	"UMAX S-12G ",
  "UMAX ",	"SuperVista S-12 ",
  "UMAX ",	"PSD ",
  "UMAX ",	"PL-II ",
  "UMAX ",	"Astra 600S ",
  "UMAX ",	"Astra 610S ",
  "UMAX ",	"Astra 1200S ",
  "UMAX ",	"Astra 1220S ", 
  "UMAX ",	"Astra 2200 ", 
  "UMAX ",	"Astra 2400S ",
  "UMAX ",	"Mirage D-16L ",
/*  "UMAX ",	"Mirage II ", */
  "UMAX ",	"Mirage IIse ",
/*  "UMAX ",	"PL-II ", */
/*  "UMAX ",	"Power Look 2000 ", */
  "UMAX ",	"PowerLook III ",
/*  "UMAX ",	"Power Look 3000 ", */
  "UMAX ",	"Gemini D-16 ",
  "LinoHell",	"Office ",
  "LinoHell",	"JADE ",
  "LinoHell",	"Office2 ",
  "LinoHell",	"SAPHIR2 ",
/*  "LinoHell",	"SAPHIR4 ", */
  "Nikon ",	"AX-210 ",
  "KYE ",	"ColorPage-HR5 ", 
  "EPSON ",	"Perfection600 ", 
  "END_OF_LIST"
};

/* ==================================================================== */

#endif
