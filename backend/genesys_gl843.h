/* sane - Scanner Access Now Easy.

   Copyright (C) 2010 Stéphane Voltz <stef.dev@free.fr>

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
#include "../include/sane/config.h"

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"
#include "../include/sane/saneopts.h"

#ifndef HACK
#undef BACKEND_NAME
#define BACKEND_NAME genesys_gl843
#endif

#include "../include/sane/sanei_backend.h"
#include "../include/sane/sanei_config.h"
#include "../include/sane/sanei_usb.h"

#include "../include/_stdint.h"
#include "genesys.h"

#define DBGSTART DBG (DBG_proc, "%s start\n", __FUNCTION__);
#define DBGCOMPLETED DBG (DBG_proc, "%s completed\n", __FUNCTION__);

#define REG01           0x01
#define REG01_CISSET	0x80
#define REG01_DOGENB	0x40
#define REG01_DVDSET	0x20
#define REG01_STAGGER   0x10
#define REG01_COMPENB	0x08
#define REG01_TRUEGRAY  0x04
#define REG01_SHDAREA	0x02
#define REG01_SCAN	0x01

#define REG02        	0x02
#define REG02_NOTHOME	0x80
#define REG02_ACDCDIS	0x40
#define REG02_AGOHOME	0x20
#define REG02_MTRPWR	0x10
#define REG02_FASTFED	0x08
#define REG02_MTRREV	0x04
#define REG02_HOMENEG	0x02
#define REG02_LONGCURV	0x01

#define REG03           0x03
#define REG03_LAMPDOG	0x80
#define REG03_AVEENB	0x40
#define REG03_XPASEL	0x20
#define REG03_LAMPPWR	0x10
#define REG03_LAMPTIM	0x0f

#define REG04        	0x04
#define REG04_LINEART	0x80
#define REG04_BITSET	0x40
#define REG04_AFEMOD	0x30
#define REG04_FILTER	0x0c
#define REG04_FESET	0x03

#define REG04S_AFEMOD   4

#define REG05 		0x05
#define REG05_DPIHW	0xc0
#define REG05_DPIHW_600	0x00
#define REG05_DPIHW_1200	0x40
#define REG05_DPIHW_2400	0x80
#define REG05_DPIHW_4800	0xc0
#define REG05_MTLLAMP	0x30
#define REG05_GMMENB	0x08
#define REG05_MTLBASE	0x03

#define REG06 		0x06
#define REG06_SCANMOD	0xe0
#define REG06S_SCANMOD	5
#define REG06_PWRBIT	0x10
#define REG06_GAIN4	0x08
#define REG06_OPTEST	0x07

#define	REG07_LAMPSIM	0x80

#define REG08_DECFLAG 	0x40
#define REG08_GMMFFR    0x20
#define REG08_GMMFFG    0x10
#define REG08_GMMFFB	0x08
#define REG08_GMMZR     0x04
#define REG08_GMMZG     0x02
#define REG08_GMMZB     0x01

#define REG09_MCNTSET	0xc0
#define REG09_EVEN1ST   0x20
#define REG09_BLINE1ST  0x10
#define REG09_BACKSCAN	0x08
#define REG09_ENHANCE	0x04
#define REG09_SHORTTG	0x02
#define REG09_NWAIT	0x01

#define REG09S_MCNTSET  6
#define REG09S_CLKSET   4

#define REG0B           0x0b
#define REG0B_DRAMSEL   0x07
#define REG0B_ENBDRAM   0x08
#define REG0B_ENBDRAM   0x08
#define REG0B_RFHDIS    0x10
#define REG0B_CLKSET    0xe0
#define REG0B_24MHZ     0x00
#define REG0B_30MHZ     0x20
#define REG0B_40MHZ     0x40
#define REG0B_48MHZ     0x60
#define REG0B_60MHZ     0x80

#define REG0D 		0x0d
#define REG0D_JAMPCMD   0x80
#define REG0D_DOCCMD    0x40
#define REG0D_CCDCMD    0x20
#define REG0D_FULLSTP   0x10
#define REG0D_SEND      0x08
#define REG0D_CLRMCNT   0x04
#define REG0D_CLRDOCJM  0x02
#define REG0D_CLRLNCNT	0x01

#define REG0F 		0x0f

#define REG16_CTRLHI	0x80
#define REG16_TOSHIBA	0x40
#define REG16_TGINV	0x20
#define REG16_CK1INV	0x10
#define REG16_CK2INV	0x08
#define REG16_CTRLINV	0x04
#define REG16_CKDIS	0x02
#define REG16_CTRLDIS	0x01

#define REG17_TGMODE	0xc0
#define REG17_TGMODE_NO_DUMMY	0x00
#define REG17_TGMODE_REF	0x40
#define REG17_TGMODE_XPA	0x80
#define REG17_TGW	0x3f
#define REG17S_TGW      0

#define REG18 		0x18
#define REG18_CNSET	0x80
#define REG18_DCKSEL	0x60
#define REG18_CKTOGGLE	0x10
#define REG18_CKDELAY	0x0c
#define REG18_CKSEL	0x03

#define REG1A_TGLSW2 	0x80
#define REG1A_TGLSW1 	0x40
#define REG1A_MANUAL3	0x02
#define REG1A_MANUAL1	0x01
#define REG1A_CK4INV	0x08
#define REG1A_CK3INV	0x04
#define REG1A_LINECLP	0x02

#define REG1C_TGTIME    0x07

#define REG1D_CK4LOW	0x80
#define REG1D_CK3LOW	0x40
#define REG1D_CK1LOW	0x20
#define REG1D_TGSHLD	0x1f
#define REG1DS_TGSHLD   0


#define REG1E           0x1e
#define REG1E_WDTIME	0xf0
#define REG1ES_WDTIME   4
#define REG1E_LINESEL	0x0f
#define REG1ES_LINESEL  0

#define REG21           0x21

#define REG29           0x29
#define REG2A           0x2a
#define REG2B           0x2b

#define REG40           0x40
#define REG40_DOCSNR    0x80
#define REG40_ADFSNR    0x40
#define REG40_COVERSNR  0x20
#define REG40_CHKVER    0x10
#define REG40_DOCJAM    0x08
#define REG40_HISPDFLG  0x04
#define REG40_MOTMFLG   0x02
#define REG40_DATAENB   0x01

#define REG41_PWRBIT	0x80
#define REG41_BUFEMPTY	0x40
#define REG41_FEEDFSH	0x20
#define REG41_SCANFSH	0x10
#define REG41_HOMESNR	0x08
#define REG41_LAMPSTS	0x04
#define REG41_FEBUSY	0x02
#define REG41_MOTORENB	0x01

#define REG58_VSMP      0xf8
#define REG58S_VSMP     3
#define REG58_VSMPW     0x07
#define REG58S_VSMPW    0

#define REG59_BSMP      0xf8
#define REG59S_BSMP     3
#define REG59_BSMPW     0x07
#define REG59S_BSMPW    0

#define REG5A_ADCLKINV  0x80
#define REG5A_RLCSEL    0x40
#define REG5A_CDSREF    0x30
#define REG5AS_CDSREF   4
#define REG5A_RLC       0x0f
#define REG5AS_RLC      0

#define REG5E 		0x5e
#define REG5E_DECSEL    0xe0
#define REG5ES_DECSEL   5
#define REG5E_STOPTIM   0x1f
#define REG5ES_STOPTIM  0

#define REG5F 		0x5f

#define REG60           0x60
#define REG60_Z1MOD	0x1f
#define REG61           0x61
#define REG61_Z1MOD	0xff
#define REG62           0x62
#define REG62_Z1MOD	0xff

#define REG63           0x63
#define REG63_Z2MOD	0x1f
#define REG64           0x64
#define REG64_Z2MOD	0xff
#define REG65           0x65
#define REG65_Z2MOD	0xff

#define REG67 		0x67

#define REG68 		0x68

#define REG67S_STEPSEL      6
#define REG67_STEPSEL	 0xc0
#define REG67_FULLSTEP	 0x00
#define REG67_HALFSTEP	 0x20
#define REG67_EIGHTHSTEP 0x60
#define REG67_16THSTEP   0x80

#define REG68S_FSTPSEL      6
#define REG68_FSTPSEL	 0xc0
#define REG68_FULLSTEP	 0x00
#define REG68_HALFSTEP	 0x20
#define REG68_EIGHTHSTEP 0x60
#define REG68_16THSTEP   0x80

#define REG69          	0x69
#define REG6A          	0x6a

#define REG6B          	0x6b
#define REG6B_MULTFILM	0x80
#define REG6B_GPOM13	0x40
#define REG6B_GPOM12	0x20
#define REG6B_GPOM11	0x10
#define REG6B_GPO18	0x02
#define REG6B_GPO17	0x01

#define REG6C           0x6c
#define REG6C_GPIO16    0x80
#define REG6C_GPIO15    0x40
#define REG6C_GPIO14    0x20
#define REG6C_GPIO13    0x10
#define REG6C_GPIO12    0x08
#define REG6C_GPIO11    0x04
#define REG6C_GPIO10    0x02
#define REG6C_GPIO9     0x01
#define REG6C_GPIOH	0xff
#define REG6C_GPIOL	0xff

#define REG6D           0x6d
#define REG6E           0x6e
#define REG6F           0x6f

#define REG9D           0x9d
#define REG9DS_STEPTIM  2

#define REG87_LEDADD    0x04

#define REGA6   	0xa6
#define REGA7 		0xa7
#define REGA8 		0xa8
#define REGA9 		0xa9

#define SCAN_TABLE 	0 	/* table 1 at 0x4000 */
#define BACKTRACK_TABLE 1 	/* table 2 at 0x4800 */
#define STOP_TABLE 	2 	/* table 3 at 0x5000 */
#define FAST_TABLE 	3 	/* table 4 at 0x5800 */
#define HOME_TABLE 	4 	/* table 5 at 0x6000 */

#define SCAN_FLAG_SINGLE_LINE              0x001
#define SCAN_FLAG_DISABLE_SHADING          0x002
#define SCAN_FLAG_DISABLE_GAMMA            0x004
#define SCAN_FLAG_DISABLE_BUFFER_FULL_MOVE 0x008
#define SCAN_FLAG_IGNORE_LINE_DISTANCE     0x010
#define SCAN_FLAG_USE_OPTICAL_RES          0x020
#define SCAN_FLAG_DISABLE_LAMP             0x040
#define SCAN_FLAG_DYNAMIC_LINEART          0x080

/**
 * writable scanner registers */
enum
{
  reg_0x01 = 0,
  reg_0x02,
  reg_0x03,
  reg_0x04,
  reg_0x05,
  reg_0x06,
  reg_0x08,
  reg_0x09,
  reg_0x0a,
  reg_0x0b,
  reg_0x0c,
  reg_0x0f,
  reg_0x10,
  reg_0x11,
  reg_0x12,
  reg_0x13,
  reg_0x14,
  reg_0x15,
  reg_0x16,
  reg_0x17,
  reg_0x18,
  reg_0x19,
  reg_0x1a,
  reg_0x1b,
  reg_0x1c,
  reg_0x1d,
  reg_0x1e,
  reg_0x1f,
  reg_0x20,
  reg_0x21,
  reg_0x22,
  reg_0x23,
  reg_0x24,
  reg_0x25,
  reg_0x26,
  reg_0x27,
  reg_0x28,
  reg_0x2c,
  reg_0x2d,
  reg_0x2e,
  reg_0x2f,
  reg_0x30,
  reg_0x31,
  reg_0x32,
  reg_0x33,
  reg_0x34,
  reg_0x35,
  reg_0x36,
  reg_0x37,
  reg_0x38,
  reg_0x39,
  reg_0x3a,
  reg_0x3b,
  reg_0x3c,
  reg_0x3d,
  reg_0x3e,
  reg_0x3f,
  reg_0x51,
  reg_0x52,
  reg_0x53,
  reg_0x54,
  reg_0x55,
  reg_0x56,
  reg_0x57,
  reg_0x58,
  reg_0x59,
  reg_0x5a,
  reg_0x5d,
  reg_0x5e,
  reg_0x5f,
  reg_0x60,
  reg_0x61,
  reg_0x62,
  reg_0x63,
  reg_0x64,
  reg_0x65,
  reg_0x67,
  reg_0x68,
  reg_0x69,
  reg_0x6a,
  reg_0x6b,
  reg_0x70,
  reg_0x71,
  reg_0x72,
  reg_0x73,
  reg_0x74,
  reg_0x75,
  reg_0x76,
  reg_0x77,
  reg_0x78,
  reg_0x79,
  reg_0x7a,
  reg_0x7b,
  reg_0x7c,
  reg_0x7d,
  reg_0x7e,
  reg_0x7f,
  reg_0x80,
  reg_0x81,
  reg_0x82,
  reg_0x83,
  reg_0x84,
  reg_0x85,
  reg_0x86,
  reg_0x87,
  reg_0x88,
  reg_0x89,
  reg_0x8a,
  reg_0x8b,
  reg_0x8c,
  reg_0x8d,
  reg_0x8e,
  reg_0x8f,
  reg_0x90,
  reg_0x91,
  reg_0x92,
  reg_0x93,
  reg_0x94,
  reg_0x95,
  reg_0x96,
  reg_0x97,
  reg_0x98,
  reg_0x99,
  reg_0x9a,
  reg_0x9b,
  reg_0x9c,
  reg_0x9d,
  reg_0xa0,
  reg_0xa1,
  reg_0xa2,
  reg_0xa3,
  reg_0xa4,
  reg_0xa5,
  reg_0xab,
  reg_0xac,
  reg_0xad,
  reg_0xae,
  reg_0xaf,
  GENESYS_GL843_MAX_REGS
};

#define SETREG(adr,val) {dev->reg[reg_##adr].address=adr;dev->reg[reg_##adr].value=val;}

typedef struct
{
  uint8_t ra6;
  uint8_t ra7;
  uint8_t ra8;
  uint8_t ra9;
} Gpio_layout;

static Gpio_layout gpios[]={
	/* G4050 */
	{
		0x08, 0x1e, 0x3e, 0x06
	},
	/* KV-SS080 */
	{
		0x06, 0x0f, 0x00, 0x08
	}
};

/**
 * structure for motor database
 */
typedef struct {
	int motor_type;	 /**> motor id */
	int exposure;    /**> exposure for the slope table */
	uint16_t *table; /**> slope table at full step */
} Motor_Profile;

static uint16_t kvss080[]={44444, 34188, 32520, 29630, 26666, 24242, 22222, 19048, 16666, 15686, 14814, 14034, 12402, 11110, 8888, 7618, 6666, 5926, 5228, 4678, 4172, 3682, 3336, 3074, 2866, 2702, 2566, 2450, 2352, 2266, 2188, 2118, 2056, 2002, 1950, 1904, 1860, 1820, 1784, 1748, 1716, 1684, 1656, 1628, 1600, 1576, 1552, 1528, 1506, 1486, 1466, 1446, 1428, 1410, 1394, 1376, 1360, 1346, 1330, 1316, 1302, 1288, 1276, 1264, 1250, 1238, 1228, 1216, 1206, 1194, 1184, 1174, 1164, 1154, 1146, 1136, 1128, 1120, 1110, 1102, 1094, 1088, 1080, 1072, 1064, 1058, 1050, 1044, 1038, 1030, 1024, 1018, 1012, 1006, 1000, 994, 988, 984, 978, 972, 968, 962, 958, 952, 948, 942, 938, 934, 928, 924, 920, 916, 912, 908, 904, 900, 896, 892, 888, 884, 882, 878, 874, 870, 868, 864, 860, 858, 854, 850, 848, 844, 842, 838, 836, 832, 830, 826, 824, 822, 820, 816, 814, 812, 808, 806, 804, 802, 800, 796, 794, 792, 790, 788, 786, 784, 782, 778, 776, 774, 772, 770, 768, 766, 764, 762, 760, 758, 756, 754, 752, 750, 750, 748, 746, 744, 742, 740, 738, 736, 734, 734, 732, 730, 728, 726, 724, 724, 722, 720, 718, 716, 716, 714, 712, 710, 710, 708, 706, 704, 704, 702, 700, 698, 698, 696, 694, 694, 692, 690, 690, 688, 686, 686, 684, 682, 682, 680, 678, 678, 676, 674, 674, 672, 672, 670, 668, 668, 666, 666, 664, 662, 662, 660, 660, 658, 656, 656, 654, 654, 652, 652, 650, 650, 648, 646, 646, 644, 644, 642, 642, 640, 640, 638, 638, 636, 636, 636, 634, 634, 632, 632, 630, 630, 628, 628, 626, 626, 624, 624, 624, 622, 622, 620, 620, 618, 618, 618, 616, 616, 614, 614, 612, 612, 612, 610, 610, 608, 608, 608, 606, 606, 606, 604, 604, 602, 602, 602, 600, 600, 600, 598, 598, 596, 596, 596, 594, 594, 594, 592, 592, 592, 590, 590, 590, 588, 588, 588, 586, 586, 586, 584, 584, 584, 582, 582, 582, 590, 590, 590, 588, 588, 588, 586, 586, 586, 584, 584, 584, 582, 582, 582, 580, 580, 580, 578, 578, 578, 576, 576, 576, 576, 574, 574, 574, 572, 572, 572, 570, 570, 570, 568, 568, 568, 568, 566, 566, 566, 564, 564, 564, 562, 562, 562, 562, 560, 560, 560, 558, 558, 558, 558, 556, 556, 556, 554, 554, 554, 552, 552, 552, 552, 550, 550, 550, 548, 548, 548, 548, 546, 546, 546, 546, 544, 544, 544, 542, 542, 542, 542, 540, 540, 540, 538, 538, 538, 538, 536, 536, 536, 536, 534, 534, 534, 534, 532, 532, 532, 530, 530, 530, 530, 528, 528, 528, 528, 526, 526, 526, 526, 524, 524, 524, 524, 522, 522, 522, 522, 520, 520, 520, 520, 518, 518, 518, 516, 516, 516, 516, 514, 514, 514, 514, 514, 512, 512, 512, 512, 510, 510, 510, 510, 508, 508, 508, 508, 506, 506, 506, 506, 504, 504, 504, 504, 502, 502, 502, 502, 500, 500, 500, 500, 0};
static uint16_t g4050[]={7842,5898,4384,4258,4152,4052,3956,3864,3786,3714,3632,3564,3498,3444,3384,3324,3276,3228,3174,3128,3086,3044,3002,2968,2930,2892,2860,2824,2794,2760,2732,2704,2676,2650,2618,2594,2568,2548,2524,2500,2478,2454,2436,2414,2392,2376,2354,2338,2318,2302,2282,2266,2252,2232,2218,2202,2188,2174,2160,2142,2128,2116,2102,2088,2076,2062,2054,2040,2028,2020,2014,2008,2004,2002,2002,2002,1946,1882,1826,1770,1716,1662,1612,1568,1526,1488,1454,1422,1390,1362,1336,1310,1288,1264,1242,1222,1204,1184,1166,1150,1134,1118,1104,1090,1076,1064,1050,1038,1026,1016,1004,994,984,972,964,954,944,936,928,920,910,902,896,888,880,874,866,860,854,848,840,834,828,822,816,812,806,800,796,790,784,780,776,770,766,760,756,752,748,744,740,736,732,728,724,720,716,712,708,704,702,698,694,690,688,684,682,678,674,672,668,666,662,660,656,654,650,648,646,644,640,638,636,632,630,628,624,622,620,618,616,614,610,608,606,604,602,600,598,596,594,592,590,588,586,584,582,580,578,576,574,572,570,568,566,564,564,562,560,558,556,554,552,552,550,548,546,546,544,542,540,538,538,536,534,532,532,530,528,528,526,524,522,522,520,518,518,516,514,514,512,512,510,508,508,506,504,504,502,502,500,498,498,496,496,494,494,492,490,490,488,488,486,486,484,484,482,480,480,478,478,476,476,474,474,472,472,470,470,468,468,468,466,466,464,464,462,462,460,460,458,458,456,456,456,454,454,452,452,450,450,450,448,448,446,446,444,444,444,442,442,440,440,440,438,438,438,436,436,434,434,434,432,432,432,430,430,428,428,428,426,426,426,424,424,424,422,422,422,420,420,420,418,418,418,416,416,416,414,414,414,412,412,412,410,410,410,408,408,408,406,406,406,404,404,404,404,402,402,402,400,400,400,400,398,398,398,396,396,396,396,394,394,394,392,392,392,392,390,390,390,388,388,388,388,386,386,386,386,384,384,384,384,382,382,382,382,380,380,380,380,378,378,378,378,376,376,376,376,376,374,374,374,374,374,372,372,372,372,372,370,370,370,370,370,368,368,368,368,368,366,366,366,366,366,364,364,364,364,364,364,362,362,362,362,362,360,360,360,360,360,360,358,358,358,358,358,358,356,356,356,356,356,356,354,354,354,354,354,352,352,352,352,352,352,350,350,350,350,350,350,350,348,348,348,348,348,348,346,346,346,346,346,346,344,344,344,344,344,344,344,342,342,342,342,342,342,340,340,340,340,340,340,340,338,338,338,338,338,338,338,336,336,336,336,336,336,336,334,334,334,334,334,334,334,332,332,332,332,332,332,332,332,330,330,330,330,330,330,330,328,328,328,328,328,328,328,328,326,326,326,326,326,326,326,324,324,324,324,324,324,324,324,322,322,322,322,322,322,322,322,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320,320, 0};

/**
 * database of motor profiles
 */

/* *INDENT-OFF* */
static Motor_Profile motors[]={
	{MOTOR_KVSS080,  8000, kvss080},
	{MOTOR_G4050,    8016, g4050},
	{MOTOR_G4050,    21376, g4050},
	{MOTOR_G4050,    56064, g4050},
};
/* *INDENT-ON* */

/* vim: set sw=2 cino=>2se-1sn-1s{s^-1st0(0u0 smarttab expandtab: */
