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

#define REG01           0x01
#define REG01_CISSET    0x80
#define REG01_DOGENB    0x40
#define REG01_DVDSET    0x20
#define REG01_STAGGER   0x10
#define REG01_COMPENB   0x08
#define REG01_TRUEGRAY  0x04
#define REG01_SHDAREA   0x02
#define REG01_SCAN      0x01

#define REG02           0x02
#define REG02_NOTHOME   0x80
#define REG02_ACDCDIS   0x40
#define REG02_AGOHOME   0x20
#define REG02_MTRPWR    0x10
#define REG02_FASTFED   0x08
#define REG02_MTRREV    0x04
#define REG02_HOMENEG   0x02
#define REG02_LONGCURV  0x01

#define REG03           0x03
#define REG03_LAMPDOG   0x80
#define REG03_AVEENB    0x40
#define REG03_XPASEL    0x20
#define REG03_LAMPPWR   0x10
#define REG03_LAMPTIM   0x0f

#define REG04           0x04
#define REG04_LINEART   0x80
#define REG04_BITSET    0x40
#define REG04_FILTER    0x30
#define REG04_AFEMOD    0x07

#define REG05           0x05
#define REG05_DPIHW     0xc0
#define REG05_DPIHW_600  0x00
#define REG05_DPIHW_1200 0x40
#define REG05_DPIHW_2400 0x80
#define REG05_DPIHW_4800 0xc0
#define REG05_MTLLAMP   0x30
#define REG05_GMMENB    0x08
#define REG05_ENB20M    0x04
#define REG05_MTLBASE   0x03

#define REG06           0x06
#define REG06_SCANMOD   0xe0
#define REG06S_SCANMOD     5
#define REG06_PWRBIT    0x10
#define REG06_GAIN4     0x08
#define REG06_OPTEST    0x07

#define REG07_LAMPSIM   0x80

#define REG08_DRAM2X    0x80
#define REG08_MPENB     0x20
#define REG08_CIS_LINE  0x10
#define REG08_IR2_ENB   0x08
#define REG08_IR1_ENB   0x04
#define REG08_ENB24M    0x01

#define REG09_MCNTSET   0xc0
#define REG09_EVEN1ST   0x20
#define REG09_BLINE1ST  0x10
#define REG09_BACKSCAN  0x08
#define REG09_OUTINV    0x04
#define REG09_SHORTTG   0x02

#define REG09S_MCNTSET  6
#define REG09S_CLKSET   4

#define REG0A           0x0a
#define REG0A_SIFSEL    0xc0
#define REG0AS_SIFSEL   6
#define REG0A_SHEETFED  0x20
#define REG0A_LPWMEN    0x10

#define REG0B           0x0b
#define REG0B_DRAMSEL   0x07
#define REG0B_16M       0x01
#define REG0B_64M       0x02
#define REG0B_128M      0x03
#define REG0B_256M      0x04
#define REG0B_512M      0x05
#define REG0B_1G        0x06
#define REG0B_ENBDRAM   0x08
#define REG0B_RFHDIS    0x10
#define REG0B_CLKSET    0xe0
#define REG0B_24MHZ     0x00
#define REG0B_30MHZ     0x20
#define REG0B_40MHZ     0x40
#define REG0B_48MHZ     0x60
#define REG0B_60MHZ     0x80

#define REG0D           0x0d
#define REG0D_MTRP_RDY  0x80
#define REG0D_FULLSTP   0x10
#define REG0D_CLRMCNT   0x04
#define REG0D_CLRDOCJM  0x02
#define REG0D_CLRLNCNT  0x01

#define REG0F           0x0f

#define REG16_CTRLHI    0x80
#define REG16_TOSHIBA   0x40
#define REG16_TGINV     0x20
#define REG16_CK1INV    0x10
#define REG16_CK2INV    0x08
#define REG16_CTRLINV   0x04
#define REG16_CKDIS     0x02
#define REG16_CTRLDIS   0x01

#define REG17_TGMODE    0xc0
#define REG17_SNRSYN    0x0f

#define REG18           0x18
#define REG18_CNSET     0x80
#define REG18_DCKSEL    0x60
#define REG18_CKTOGGLE  0x10
#define REG18_CKDELAY   0x0c
#define REG18_CKSEL     0x03

#define REG1A_SW2SET    0x80
#define REG1A_SW1SET    0x40
#define REG1A_MANUAL3   0x02
#define REG1A_MANUAL1   0x01
#define REG1A_CK4INV    0x08
#define REG1A_CK3INV    0x04
#define REG1A_LINECLP   0x02

#define REG1C_TBTIME    0x07

#define REG1D           0x1d
#define REG1D_CK4LOW    0x80
#define REG1D_CK3LOW    0x40
#define REG1D_CK1LOW    0x20
#define REG1D_LINESEL   0x1f
#define REG1DS_LINESEL  0

#define REG1E           0x1e
#define REG1E_WDTIME    0xf0
#define REG1ES_WDTIME   4
#define REG1E_WDTIME    0xf0

#define REG30           0x30
#define REG31           0x31
#define REG32           0x32
#define REG32_GPIO16    0x80
#define REG32_GPIO15    0x40
#define REG32_GPIO14    0x20
#define REG32_GPIO13    0x10
#define REG32_GPIO12    0x08
#define REG32_GPIO11    0x04
#define REG32_GPIO10    0x02
#define REG32_GPIO9     0x01
#define REG33           0x33
#define REG34           0x34
#define REG35           0x35
#define REG36           0x36
#define REG37           0x37
#define REG38           0x38
#define REG39           0x39

#define REG60           0x60
#define REG60_LED4TG    0x80
#define REG60_YENB      0x40
#define REG60_YBIT      0x20
#define REG60_ACYNCNRLC 0x10
#define REG60_ENOFFSET  0x08
#define REG60_LEDADD    0x04
#define REG60_CK4ADC    0x02
#define REG60_AUTOCONF  0x01

#define REG80           0x80
#define REG81           0x81

#define REGA0           0xa0
#define REGA0_FSTPSEL   0x28
#define REGA0S_FSTPSEL  3
#define REGA0_STEPSEL   0x03
#define REGA0S_STEPSEL  0

#define REGA1           0xa1
#define REGA2           0xa2
#define REGA3           0xa3
#define REGA4           0xa4
#define REGA5           0xa5
#define REGA6           0xa6
#define REGA7           0xa7
#define REGA8           0xa8
#define REGA9           0xa9
#define REGAA           0xaa
#define REGAB           0xab
#define REGAC           0xac
#define REGAD           0xad
#define REGAE           0xae
#define REGAF           0xaf
#define REGB0           0xb0
#define REGB1           0xb1

#define REGB2           0xb2
#define REGB2_Z1MOD     0x1f
#define REGB3           0xb3
#define REGB3_Z1MOD     0xff
#define REGB4           0xb4
#define REGB4_Z1MOD     0xff

#define REGB5           0xb5
#define REGB5_Z2MOD     0x1f
#define REGB6           0xb6
#define REGB6_Z2MOD     0xff
#define REGB7           0xb7
#define REGB7_Z2MOD     0xff

#define REG100          0x100
#define REG100_DOCSNR   0x80
#define REG100_ADFSNR   0x40
#define REG100_COVERSNR 0x20
#define REG100_CHKVER   0x10
#define REG100_DOCJAM   0x08
#define REG100_HISPDFLG 0x04
#define REG100_MOTMFLG  0x02
#define REG100_DATAENB  0x01

#define REG114          0x114
#define REG115          0x115

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

#endif // BACKEND_GENESYS_GL843_REGISTERS_H
