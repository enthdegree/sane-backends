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

#ifndef BACKEND_GENESYS_GL124_REGISTERS_H
#define BACKEND_GENESYS_GL124_REGISTERS_H

namespace genesys {
namespace gl124 {

#define REG_0x01           0x01
#define REG_0x01_CISSET    0x80
#define REG_0x01_DOGENB    0x40
#define REG_0x01_DVDSET    0x20
#define REG_0x01_STAGGER   0x10
#define REG_0x01_COMPENB   0x08
#define REG_0x01_TRUEGRAY  0x04
#define REG_0x01_SHDAREA   0x02
#define REG_0x01_SCAN      0x01

#define REG_0x02           0x02
#define REG_0x02_NOTHOME   0x80
#define REG_0x02_ACDCDIS   0x40
#define REG_0x02_AGOHOME   0x20
#define REG_0x02_MTRPWR    0x10
#define REG_0x02_FASTFED   0x08
#define REG_0x02_MTRREV    0x04
#define REG_0x02_HOMENEG   0x02
#define REG_0x02_LONGCURV  0x01

#define REG_0x03           0x03
#define REG_0x03_LAMPDOG   0x80
#define REG_0x03_AVEENB    0x40
#define REG_0x03_XPASEL    0x20
#define REG_0x03_LAMPPWR   0x10
#define REG_0x03_LAMPTIM   0x0f

#define REG_0x04           0x04
#define REG_0x04_LINEART   0x80
#define REG_0x04_BITSET    0x40
#define REG_0x04_FILTER    0x30
#define REG_0x04_AFEMOD    0x07

#define REG_0x05           0x05
#define REG_0x05_DPIHW     0xc0
#define REG_0x05_DPIHW_600  0x00
#define REG_0x05_DPIHW_1200 0x40
#define REG_0x05_DPIHW_2400 0x80
#define REG_0x05_DPIHW_4800 0xc0
#define REG_0x05_MTLLAMP   0x30
#define REG_0x05_GMMENB    0x08
#define REG_0x05_ENB20M    0x04
#define REG_0x05_MTLBASE   0x03

#define REG_0x06           0x06
#define REG_0x06_SCANMOD   0xe0
#define REG_0x06S_SCANMOD     5
#define REG_0x06_PWRBIT    0x10
#define REG_0x06_GAIN4     0x08
#define REG_0x06_OPTEST    0x07

#define REG_0x07_LAMPSIM   0x80

#define REG_0x08_DRAM2X    0x80
#define REG_0x08_MPENB     0x20
#define REG_0x08_CIS_LINE  0x10
#define REG_0x08_IR2_ENB   0x08
#define REG_0x08_IR1_ENB   0x04
#define REG_0x08_ENB24M    0x01

#define REG_0x09_MCNTSET   0xc0
#define REG_0x09_EVEN1ST   0x20
#define REG_0x09_BLINE1ST  0x10
#define REG_0x09_BACKSCAN  0x08
#define REG_0x09_OUTINV    0x04
#define REG_0x09_SHORTTG   0x02

#define REG_0x09S_MCNTSET  6
#define REG_0x09S_CLKSET   4

#define REG_0x0A           0x0a
#define REG_0x0A_SIFSEL    0xc0
#define REG_0x0AS_SIFSEL   6
#define REG_0x0A_SHEETFED  0x20
#define REG_0x0A_LPWMEN    0x10

#define REG_0x0B           0x0b
#define REG_0x0B_DRAMSEL   0x07
#define REG_0x0B_16M       0x01
#define REG_0x0B_64M       0x02
#define REG_0x0B_128M      0x03
#define REG_0x0B_256M      0x04
#define REG_0x0B_512M      0x05
#define REG_0x0B_1G        0x06
#define REG_0x0B_ENBDRAM   0x08
#define REG_0x0B_RFHDIS    0x10
#define REG_0x0B_CLKSET    0xe0
#define REG_0x0B_24MHZ     0x00
#define REG_0x0B_30MHZ     0x20
#define REG_0x0B_40MHZ     0x40
#define REG_0x0B_48MHZ     0x60
#define REG_0x0B_60MHZ     0x80

#define REG_0x0D           0x0d
#define REG_0x0D_MTRP_RDY  0x80
#define REG_0x0D_FULLSTP   0x10
#define REG_0x0D_CLRMCNT   0x04
#define REG_0x0D_CLRDOCJM  0x02
#define REG_0x0D_CLRLNCNT  0x01

#define REG_0x0F           0x0f

#define REG_0x16_CTRLHI    0x80
#define REG_0x16_TOSHIBA   0x40
#define REG_0x16_TGINV     0x20
#define REG_0x16_CK1INV    0x10
#define REG_0x16_CK2INV    0x08
#define REG_0x16_CTRLINV   0x04
#define REG_0x16_CKDIS     0x02
#define REG_0x16_CTRLDIS   0x01

#define REG_0x17_TGMODE    0xc0
#define REG_0x17_SNRSYN    0x0f

#define REG_0x18           0x18
#define REG_0x18_CNSET     0x80
#define REG_0x18_DCKSEL    0x60
#define REG_0x18_CKTOGGLE  0x10
#define REG_0x18_CKDELAY   0x0c
#define REG_0x18_CKSEL     0x03

#define REG_0x1A_SW2SET    0x80
#define REG_0x1A_SW1SET    0x40
#define REG_0x1A_MANUAL3   0x02
#define REG_0x1A_MANUAL1   0x01
#define REG_0x1A_CK4INV    0x08
#define REG_0x1A_CK3INV    0x04
#define REG_0x1A_LINECLP   0x02

#define REG_0x1C_TBTIME    0x07

#define REG_0x1D           0x1d
#define REG_0x1D_CK4LOW    0x80
#define REG_0x1D_CK3LOW    0x40
#define REG_0x1D_CK1LOW    0x20
#define REG_0x1D_LINESEL   0x1f
#define REG_0x1DS_LINESEL  0

#define REG_0x1E           0x1e
#define REG_0x1E_WDTIME    0xf0
#define REG_0x1ES_WDTIME   4
#define REG_0x1E_WDTIME    0xf0

#define REG_0x30           0x30
#define REG_0x31           0x31
#define REG_0x32           0x32
#define REG_0x32_GPIO16    0x80
#define REG_0x32_GPIO15    0x40
#define REG_0x32_GPIO14    0x20
#define REG_0x32_GPIO13    0x10
#define REG_0x32_GPIO12    0x08
#define REG_0x32_GPIO11    0x04
#define REG_0x32_GPIO10    0x02
#define REG_0x32_GPIO9     0x01
#define REG_0x33           0x33
#define REG_0x34           0x34
#define REG_0x35           0x35
#define REG_0x36           0x36
#define REG_0x37           0x37
#define REG_0x38           0x38
#define REG_0x39           0x39

#define REG_0x60           0x60
#define REG_0x60_LED4TG    0x80
#define REG_0x60_YENB      0x40
#define REG_0x60_YBIT      0x20
#define REG_0x60_ACYNCNRLC 0x10
#define REG_0x60_ENOFFSET  0x08
#define REG_0x60_LEDADD    0x04
#define REG_0x60_CK4ADC    0x02
#define REG_0x60_AUTOCONF  0x01

#define REG_0x80           0x80
#define REG_0x81           0x81

#define REG_0xA0           0xa0
#define REG_0xA0_FSTPSEL   0x28
#define REG_0xA0S_FSTPSEL  3
#define REG_0xA0_STEPSEL   0x03
#define REG_0xA0S_STEPSEL  0

#define REG_0xA1           0xa1
#define REG_0xA2           0xa2
#define REG_0xA3           0xa3
#define REG_0xA4           0xa4
#define REG_0xA5           0xa5
#define REG_0xA6           0xa6
#define REG_0xA7           0xa7
#define REG_0xA8           0xa8
#define REG_0xA9           0xa9
#define REG_0xAA           0xaa
#define REG_0xAB           0xab
#define REG_0xAC           0xac
#define REG_0xAD           0xad
#define REG_0xAE           0xae
#define REG_0xAF           0xaf
#define REG_0xB0           0xb0
#define REG_0xB1           0xb1

#define REG_0xB2           0xb2
#define REG_0xB2_Z1MOD     0x1f
#define REG_0xB3           0xb3
#define REG_0xB3_Z1MOD     0xff
#define REG_0xB4           0xb4
#define REG_0xB4_Z1MOD     0xff

#define REG_0xB5           0xb5
#define REG_0xB5_Z2MOD     0x1f
#define REG_0xB6           0xb6
#define REG_0xB6_Z2MOD     0xff
#define REG_0xB7           0xb7
#define REG_0xB7_Z2MOD     0xff

#define REG_0x100          0x100
#define REG_0x100_DOCSNR   0x80
#define REG_0x100_ADFSNR   0x40
#define REG_0x100_COVERSNR 0x20
#define REG_0x100_CHKVER   0x10
#define REG_0x100_DOCJAM   0x08
#define REG_0x100_HISPDFLG 0x04
#define REG_0x100_MOTMFLG  0x02
#define REG_0x100_DATAENB  0x01

#define REG_0x114          0x114
#define REG_0x115          0x115

#define REG_LINCNT      0x25
#define REG_MAXWD       0x28
#define REG_DPISET      0x2c
#define REG_FEEDL       0x3d
#define REG_CK1MAP      0x74
#define REG_CK3MAP      0x77
#define REG_CK4MAP      0x7a
#define REG_LPERIOD     0x7d
#define REG_DUMMY       0x80
#define REG_STRPIXEL    0x82
#define REG_ENDPIXEL    0x85
#define REG_EXPDMY      0x88
#define REG_EXPR        0x8a
#define REG_EXPG        0x8d
#define REG_EXPB        0x90
#define REG_SEGCNT      0x93
#define REG_TG0CNT      0x96
#define REG_SCANFED     0xa2
#define REG_STEPNO      0xa4
#define REG_FWDSTEP     0xa6
#define REG_BWDSTEP     0xa8
#define REG_FASTNO      0xaa
#define REG_FSHDEC      0xac
#define REG_FMOVNO      0xae
#define REG_FMOVDEC     0xb0
#define REG_Z1MOD       0xb2
#define REG_Z2MOD       0xb5

#define REG_TRUER       0x110
#define REG_TRUEG       0x111
#define REG_TRUEB       0x112

} // namespace gl124
} // namespace genesys

#endif // BACKEND_GENESYS_GL843_REGISTERS_H
