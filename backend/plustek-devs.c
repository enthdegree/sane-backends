/*.............................................................................
 * Project : SANE library for Plustek USB flatbed scanners.
 *.............................................................................
 * File:	 plustek-devs.c
 *.............................................................................
 *
 * based on sources acquired from Plustek Inc.
 * Copyright (C) 2001-2002 Gerhard Jaeger <gerhard@gjaeger.de>
 *.............................................................................
 * History:
 * 0.40 - starting version of the USB support
 * 0.41 - added EPSON1250 entries
 *      - changed reg 0x58 of EPSON Hw0x04B8_0x010F_0 to 0x0d
 *      - reduced memory size of EPSON to 512
 *      - adjusted tpa origin of UT24
 * 0.42 - added register 0x27, 0x2c-0x37
 *        tweaked EPSON1250 settings according to Gene and Reinhard
 *        tweaked HP2200 settings according to Stefan
 *        added UMAX 3400 entries
 *        added HP2100 settings according to Craig Smoothey
 *        added LM9832 based U24
 *.............................................................................
 *
 * This file is part of the SANE package.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 * MA 02111-1307, USA.
 *
 * As a special exception, the authors of SANE give permission for
 * additional uses of the libraries contained in this release of SANE.
 *
 * The exception is that, if you link a SANE library with other files
 * to produce an executable, this does not by itself cause the
 * resulting executable to be covered by the GNU General Public
 * License.  Your use of that executable is in no way restricted on
 * account of linking the SANE library code into it.
 *
 * This exception does not, however, invalidate any other reasons why
 * the executable file might be covered by the GNU General Public
 * License.
 *
 * If you submit changes to SANE to the maintainers to be included in
 * a subsequent release, you agree by submitting the changes that
 * those changes may be distributed with this exception intact.
 *
 * If you write modifications of your own for SANE, it is your choice
 * whether to permit this exception to apply to your modifications.
 * If you do not wish that, delete this exception notice.
 */

/* the other stuff is included by plustek.c ...*/
#include "plustek-usb.h"

/*
 * for Register 0x26
 */
#define _RED_CH	  0x00
#define _GREEN_CH 0x08
#define _BLUE_CH  0x10


/* Plustek Model: UT12/UT16
 * KH: NS9831 + TPA + Button + NEC3799
 */
static DCapsDef Cap0x07B3_0x0017_0 =
{
	{	/* Normal */
		{0, 93},	    /* DataOrigin (X: 0, Y: 8mm from home) */
		0,				/* ShadingOriginY                      */
		{2550, 3508},	/* Size                                */
		{50, 50},		/* MinDpi                              */
		COLOR_BW		/* bMinDataType                        */
	},
	{	/* Positive */
		{1040 + 15, 744 - 32},	/* DataOrigin (X: 7cm + 1.8cm, Y: 8mm + 5.5cm)*/
		543,			/* ShadingOriginY (Y: 8mm + 3.8cm)     */
		{473, 414},		/* Size (X: 4cm, Y: 3.5cm)             */
		{150, 150},		/* MinDpi                              */
		COLOR_GRAY16	/* bMinDataType                        */
	},
	{	/* Negative */
		{1004 + 55, 744 + 12},	/* DataOrigin (X: 7cm + 1.5cm, Y: 8mm + 5.5cm)*/
		
		/* 533 blaustichig */
		537 /* hell */
		/* 543 gruenstichig  */
		
		/*543*/,			/* ShadingOriginY (Y: 8mm + 3.8cm)     */
		{567, 414},		/* Size	(X: 4.8cm, Y: 3.5cm)           */
		{150, 150},		/* MinDpi                              */
		COLOR_GRAY16	/* bMinDataType                        */
	},
	{	/* Adf */
		{0, 95},		/* DataOrigin (X: 0, Y: 8mm from home) */
		0,				/* ShadingOriginY                      */
		{2550, 3508},	/* Size                                */
		{50, 50},		/* MinDpi                              */
		COLOR_BW		/* bMinDataType                        */
	},
	{600, 600},  		                          /* OpticDpi */
	DEVCAPSFLAG_Positive + DEVCAPSFLAG_Negative,  /* wFlags   */
	SENSORORDER_rgb,	/* bSensorOrder    */
	4,					/* bSensorDistance */
	4,					/* bButtons */
	0,					/* bCCD */
	0x07,				/* bPCB */
	_WAF_NONE           /* no workarounds or other special stuff needed */
};

/* Plustek Model: ???
 * Description of the entries, see above...
 */
static DCapsDef Cap0x07B3_0x0015_0 =
{
	{{0, 93},               0,	 {2550, 3508}, {50, 50},   COLOR_BW     },
	{{1040 + 15, 744 - 32},	543, {473, 414},   {150, 150}, COLOR_GRAY16	},
	{{1004 + 20, 744 + 32},	543, {567, 414},   {150, 150}, COLOR_GRAY16 },
	{{0, 95},               0,   {2550, 3508}, {50, 50},   COLOR_BW		},
	{600, 600},		
	0,	
	SENSORORDER_rgb,
	4, 4, 0, 0x05, _WAF_NONE
};

/* Plustek Model: ???
 * KH: NS9831 + TPA + Button + NEC3799
 */
static DCapsDef Cap0x07B3_0x0014_0 =
{
	{{0, 93},               0,	 {2550, 3508}, {50, 50},   COLOR_BW     },
	{{1040 + 15, 744 - 32},	543, {473, 414},   {150, 150}, COLOR_GRAY16	},
	{{1004 + 20, 744 + 32},	543, {567, 414},   {150, 150}, COLOR_GRAY16 },
	{{0, 95},               0,   {2550, 3508}, {50, 50},   COLOR_BW		},
	{600, 600},		
	0,	
	SENSORORDER_rgb,
	4, 0, 0, 0x04, _WAF_NONE			
};

/* Plustek Model: ???
 * KH: NS9831 + TPA + Button + NEC3799
*/
static DCapsDef Cap0x07B3_0x0007_0 =
{
	{{0,    124},  36, {2550, 3508}, { 50, 50 }, COLOR_BW     },
	{{1040, 744}, 543, { 473, 414 }, {150, 150}, COLOR_GRAY16 },
	{{1004, 744}, 543, { 567, 414 }, {150, 150}, COLOR_GRAY16 },
	{{0,     95},	0, {2550, 3508}, { 50, 50 }, COLOR_BW     },
	{600, 600},		
	DEVCAPSFLAG_Positive + DEVCAPSFLAG_Negative,
	SENSORORDER_rgb,
	4, 5, 0, 0x07, _WAF_NONE
};

/* Plustek Model: ???
 * Hualien: NS9832 + Button + NEC548
 */
static DCapsDef Cap0x07B3_0x0005_2 =
{
	{{ 0, 64}, 0, {2550, 3508}, { 50, 50 }, COLOR_BW },
	{{ 0,  0}, 0, {0, 0}, { 0, 0 }, 0 },
	{{ 0,  0}, 0, {0, 0}, { 0, 0 }, 0 },
	{{ 0,  0}, 0, {0, 0}, { 0, 0 }, 0 },
	{600, 600},
	0,
	SENSORORDER_bgr,
	8, 2, 2, 0x05, _WAF_NONE
};

/* Plustek Model: ???
 * Hualien: NS9832 + TPA + Button + NEC3778
 */
static DCapsDef Cap0x07B3_0x0007_4 =
{
	{{       0,  111 - 4 },	  0, {2550, 3508}, { 50, 50 }, COLOR_BW     },
	{{1040 + 5,  744 - 32}, 543, { 473, 414 }, {150, 150}, COLOR_GRAY16 },
	{{1040 - 20, 768     },	543, { 567, 414 }, {150, 150}, COLOR_GRAY16 },
	{{        0, 95      },   0, {2550, 3508}, { 50, 50 }, COLOR_BW     },
	{1200, 1200},
	DEVCAPSFLAG_Positive + DEVCAPSFLAG_Negative,
	SENSORORDER_rgb,
	12,	5, 4, 0x07, _WAF_NONE
};

/* Plustek Model: ???
 * Hualien: NS9832 + Button + NEC3778
 */
static DCapsDef Cap0x07B3_0x0005_4 =
{
	{{ 0,  111 - 4 }, 0, {2550, 3508}, {50, 50}, COLOR_BW },
	{{ 0,  0}, 0, {0, 0}, { 0, 0 }, 0 },
	{{ 0,  0}, 0, {0, 0}, { 0, 0 }, 0 },
	{{ 0,  0}, 0, {0, 0}, { 0, 0 }, 0 },
	{1200, 1200},
	0,
	SENSORORDER_rgb,
	12,	5, 4, 0x05, _WAF_NONE
};

/* Plustek Model: ???
 * KH: NS9831 + TPA + Button + NEC3799
 */
static DCapsDef Cap0x07B3_0x000F_0 =
{
	{{   0, 130},  12, {2550, 3508}, { 50, 50 }, COLOR_BW	  },
	{{1040, 744}, 543, { 473, 414 }, {150, 150}, COLOR_GRAY16 },
	{{1004, 744}, 543, { 567, 414 }, {150, 150}, COLOR_GRAY16 },
	{{   0, 244},  12, {2550, 4200}, { 50, 50 }, COLOR_BW     },
	{600, 600},
	DEVCAPSFLAG_Normal + DEVCAPSFLAG_Adf,
	SENSORORDER_rgb,
	4, 5, 0, 0x0F, _WAF_NONE
};


/* Plustek Model: ???
 * KH: NS9831 + TPA + Button + NEC3799
 */
static DCapsDef Cap0x07B3_0x0013_0 =
{
	{{        0,       93},   0, {2550, 3508}, { 50,  50}, COLOR_BW     },
	{{1040 + 15, 744 - 32}, 543, { 473,  414}, {150, 150}, COLOR_GRAY16 },
	{{1004 + 30, 744 + 32},	543, { 567,  414}, {150, 150}, COLOR_GRAY16	},
	{{        0,       95},   0, {2550, 3508}, { 50,  50}, COLOR_BW 	},
	{600, 600},
	DEVCAPSFLAG_Positive + DEVCAPSFLAG_Negative,
	SENSORORDER_rgb,
	4, 4, 0, 0x03, _WAF_NONE
};

/* Plustek Model: U24
 * KH: NS9831 + Button + NEC3799
 */
static DCapsDef Cap0x07B3_0x0011_0 =
{
	{{        0,       93},   0, {2550, 3508}, { 50,  50}, COLOR_BW     },
	{{1040 + 15, 744 - 32},	543, { 473,  414}, {150, 150}, COLOR_GRAY16	},
	{{1004 + 20, 744 + 32}, 543, { 567,  414}, {150, 150}, COLOR_GRAY16 },
	{{        0,       95},   0, {2550, 3508}, { 50,  50}, COLOR_BW     },
	{600, 600},
	0,	
	SENSORORDER_rgb,
	4, 4, 0, 0x01, _WAF_NONE
};

/* Plustek Model: U12
 * KH: NS9831 + TPA + Button + NEC3799
 */
static DCapsDef Cap0x07B3_0x0010_0 =
{
	{{        0,       93},   0, {2550, 3508}, { 50,  50}, COLOR_BW		},
	{{1040 + 15, 744 - 32},	543, { 473,  414}, {150, 150}, COLOR_GRAY16	},
	{{1004 + 20, 744 + 32}, 543, { 567,  414}, {150, 150}, COLOR_GRAY16	},
	{{        0,       95},	  0, {2550, 3508}, { 50,  50}, COLOR_BW		},
	{600, 600},
	0,
	SENSORORDER_rgb,
	4, 0, 0, 0x00, _WAF_BSHIFT7_BUG
};

/* Plustek Model: ???
 * KH: NS9831 + TPA + Button + NEC3799
 */
static DCapsDef Cap0x07B3_0x0013_4 =
{
	{{        0, 99 /*114*/},   0, {2550, 3508}, { 50,  50}, COLOR_BW     },
	{{     1055,   744 - 84}, 543, { 473,  414}, {150, 150}, COLOR_GRAY16 },
	{{1004 + 20,   744 - 20}, 543, { 567,  414}, {150, 150}, COLOR_GRAY16 },
	{{        0,         95},   0, {2550, 3508}, { 50,  50}, COLOR_BW     },
	{1200, 1200},
	DEVCAPSFLAG_Positive + DEVCAPSFLAG_Negative,	
	SENSORORDER_rgb,
	12, 4, 4, 0x03, _WAF_NONE
};

/* Plustek Model: ???
 * KH: NS9831 + Button + NEC3799
 */
static DCapsDef Cap0x07B3_0x0011_4 =
{
	{{        0, 99 /*114*/},   0, {2550, 3508}, { 50,  50}, COLOR_BW     },
	{{     1055,   744 - 84}, 543, { 473,  414}, {150, 150}, COLOR_GRAY16 },
	{{1004 + 20,   744 - 20}, 543, { 567,  414}, {150, 150}, COLOR_GRAY16 },
	{{        0,         95},   0, {2550, 3508}, { 50,  50}, COLOR_BW     },
	{1200, 1200},
	0,
	SENSORORDER_rgb,
	12, 4, 4, 0x01, _WAF_NONE
};

/* Plustek Model: ???
 * KH: NS9831 + TPA + Button + NEC3799
 */
static DCapsDef Cap0x07B3_0x0010_4 =
{
	{{        0, 99 /*114*/},   0, {2550, 3508}, { 50,  50}, COLOR_BW     },
	{{     1055,   744 - 84}, 543, { 473,  414}, {150, 150}, COLOR_GRAY16 },
	{{1004 + 20,   744 - 20}, 543, { 567,  414}, {150, 150}, COLOR_GRAY16 },
	{{        0,         95},   0, {2550, 3508}, { 50,  50}, COLOR_BW     },
	{1200, 1200},
	0,
	SENSORORDER_rgb,
	12, 0, 4, 0x00, _WAF_NONE
};

/* Plustek Model: ???
 * KH: NS9831 + TPA + Button + NEC3799
 */
static DCapsDef Cap0x07B3_0x000F_4 =
{
	{{        0,      107},   0, {2550, 3508}, { 50,  50}, COLOR_BW     },
	{{ 1040 + 5, 744 - 32}, 543, { 473,  414}, {150, 150}, COLOR_GRAY16 },
	{{1040 - 20,      768}, 543, { 567,  414}, {150, 150}, COLOR_GRAY16 },
	{{        0,      244},   0, {2550, 4200}, { 50,  50}, COLOR_BW     },
	{1200, 1200},
	DEVCAPSFLAG_Normal + DEVCAPSFLAG_Adf,
	SENSORORDER_rgb,
	12, 5, 4, 0x0F, _WAF_NONE
};

/* Plustek Model: ???
 * KH: NS9831 + TPA + Button + NEC3799
 */
static DCapsDef Cap0x07B3_0x0016_4 =
{
	{{   0,  93},   0, {2550, 3508}, { 50,  50}, COLOR_BW     },
	{{ 954, 422}, 272, { 624, 1940}, {150, 150}, COLOR_GRAY16 },
	{{1120, 438}, 275, { 304, 1940}, {150, 150}, COLOR_GRAY16 },
	{{   0,  95},   0, {2550, 3508}, { 50,  50}, COLOR_BW     },
	{1200, 1200},
	DEVCAPSFLAG_Positive + DEVCAPSFLAG_Negative,
	SENSORORDER_rgb,
	12, 4, 4, 0x06, _WAF_NONE
};

/* Plustek Model: UT24
 * KH: NS9832 + TPA + Button + NEC3799
 */
static DCapsDef Cap0x07B3_0x0017_4 =
{
	{{             0,  99 - 6},	    0, {2550, 3508}, { 50,  50}, COLOR_BW     },
	{{1025 /*1055*/, 744 - 84},   543, { 473,  414}, {150, 150}, COLOR_GRAY16 },
	{{1048 /*1024*/, 754/*724*/}, 543, { 567,  414}, {150, 150}, COLOR_GRAY16 },
	{{            0,       95},     0, {2550, 3508}, { 50,  50}, COLOR_BW     },
	{1200, 1200},		
	DEVCAPSFLAG_Positive + DEVCAPSFLAG_Negative,
	SENSORORDER_rgb,
	12,	4, 4, 0x07, _WAF_NONE			
};

/* Plustek Model: ???
 * KH: NS9831 + Button + NEC3799
 */
static DCapsDef Cap0x07B3_0x0015_4 =
{
	{{        0,   99 - 6},   0, {2550, 3508}, { 50,  50}, COLOR_BW     },
	{{     1055, 744 - 84}, 543, { 473,  414}, {150, 150}, COLOR_GRAY16 },
	{{1004 + 20, 744 - 20}, 543, { 567,  414}, {150, 150}, COLOR_GRAY16 },
	{{        0,       95},   0, {2550, 3508}, { 50,  50}, COLOR_BW     },
	{1200, 1200},		
	0,	
	SENSORORDER_rgb,	
	12, 4, 4, 0x05, _WAF_NONE				
};

/* Plustek Model: ???
 * KH: NS9831 + TPA + Button + NEC3799
 */
static DCapsDef Cap0x07B3_0x0014_4 =
{
	{{        0,   99 - 6},   0, {2550, 3508}, { 50,  50}, COLOR_BW     },
	{{     1055, 744 - 84}, 543, { 473,  414}, {150, 150}, COLOR_GRAY16 },
	{{1004 + 20, 744 - 20},	543, { 567,  414}, {150, 150}, COLOR_GRAY16 },
	{{        0,       95},   0, {2550, 3508}, { 50,  50}, COLOR_BW     },
	{1200, 1200},		
	0,
	SENSORORDER_rgb,	
	12, 0, 4, 0x04, _WAF_NONE				
};

/* Plustek Model: ???
 * KH: NS9831 + TPA + Button + NEC3799
 */
static DCapsDef Cap0x07B3_0x0014_1 =
{
	{{        0,       93},   0, {3600, 5100}, { 50,  50}, COLOR_BW     },
	{{1040 + 15, 744 - 32}, 543, { 473,  414}, {150, 150}, COLOR_GRAY16 },
	{{1004 + 20, 744 + 32}, 543, { 567,  414}, {150, 150}, COLOR_GRAY16 },
	{{        0,       95},   0, {2550, 3508}, { 50,  50}, COLOR_BW     },
	{400, 400},			
	0,	
	SENSORORDER_rgb,	
	8, 0, 1, 0x04, _WAF_NONE				
};

/* Model: ???
 * KH: NS9832 + NEC3799 + 600 DPI Motor (for Brother demo only)
 */
static DCapsDef Cap0x07B3_0x0012_0 =
{
	{{        0,       93},   0, {2550, 3508}, { 50,  50}, COLOR_BW     },
	{{1040 + 15, 744 - 32},	543, { 473,  414}, {150, 150}, COLOR_GRAY16 },
	{{1004 + 20, 744 + 32}, 543, { 567,  414}, {150, 150}, COLOR_GRAY16 },
	{{        0,       95},   0, {2550, 3508}, { 50,  50}, COLOR_BW     },
	{600, 600},
	0,	
	SENSORORDER_rgb,	
	4, 0, 0, 0x02, _WAF_NONE				
};

/* Plustek Model: ???
 * KH: NS9831 + TPA + Button + NEC3799
 */
static DCapsDef Cap0x07B3_0x0017_2 =
{
	{{        0,       93},   0, {2550, 3508}, { 50,  50}, COLOR_BW     },
	{{1040 + 15, 744 - 32}, 543, { 473,  414}, {150, 150}, COLOR_GRAY16 },
	{{     1004,      744},	543, { 567,  414}, {150, 150}, COLOR_GRAY16	},
	{{        0,       95},   0, {2550, 3508}, { 50,  50}, COLOR_BW     },
	{600, 600},		
	DEVCAPSFLAG_Positive + DEVCAPSFLAG_Negative,	
	SENSORORDER_bgr,	
	8, 4, 2, 0x07, _WAF_NONE				
};

/* Plustek Model: ???
 * KH: NS9831 + TPA + Button + NEC3799
 */
static DCapsDef Cap0x07B3_0x0017_3 =
{
	{{        0,       93},   0, {2550, 3508}, { 50,  50}, COLOR_BW     },
	{{1040 + 15, 744 - 32}, 543, { 473,  414}, {150, 150}, COLOR_GRAY16 },
	{{1004 + 30, 744 + 32}, 543, { 567,  414}, {150, 150}, COLOR_GRAY16	},
	{{        0,       95},   0, {2550, 3508}, { 50,  50}, COLOR_BW     },
	{600, 600},
	DEVCAPSFLAG_Positive + DEVCAPSFLAG_Negative,	
	SENSORORDER_rgb,	
	8, 4, kNEC8861,	0x07, _WAF_NONE				
};

/*
 * HEINER: 17_1 and 15_1 are currently not used!!!! Maybe for future models...
 */
#if 0
/* Plustek Model: ???
 * A3: NS9832 + TPA + Button + SONY518
 */
static DCapsDef Cap0x07B3_0x0017_1 =
{
	{{        0,       93},   0, {2550, 3508}, { 50,  50}, COLOR_BW     },
	{{1040 + 15, 744 - 32}, 543, { 473,  414}, {150, 150}, COLOR_GRAY16 },
	{{1004 + 30, 744 + 32}, 543, { 567,  414}, {150, 150}, COLOR_GRAY16 },
	{{        0,       95},   0, {2550, 3508}, { 50,  50}, COLOR_BW     },
	{600, 600},	
	DEVCAPSFLAG_Positive + DEVCAPSFLAG_Negative,	
	SENSORORDER_rgb,	
	8, 4, 1, 0x07, _WAF_NONE				
};

static DCapsDef Cap0x07B3_0x0015_1 =
{
	{{        0,       93},   0, {2550, 3508}, { 50,  50}, COLOR_BW     },
	{{1040 + 15, 744 - 32}, 543, { 473,  414}, {150, 150}, COLOR_GRAY16 },
	{{1004 + 20, 744 + 32}, 543, { 567,  414}, {150, 150}, COLOR_GRAY16 },
	{{        0,       95},   0, {2550, 3508}, { 50,  50}, COLOR_BW     },
	{600, 200},		
	0,	
	SENSORORDER_rgb,	
	8, 4, 1, 0x05, _WAF_NONE				
};

static DCapsDef Cap0x07B3_0x0015_2 =
{
	{{        0,       93},   0, {2550, 3508}, { 50,  50}, COLOR_BW     },
	{{1040 + 15, 744 - 32}, 543, { 473,  414}, {150, 150}, COLOR_GRAY16	},
	{{1004 + 20, 744 + 32}, 543, { 567,  414}, {150, 150}, COLOR_GRAY16 },
	{{        0,       95},   0, {2550, 3508}, { 50,  50}, COLOR_BW     },
	{600, 600},	
	0,
	SENSORORDER_bgr,	
	8, 4, 2, 0x05, _WAF_NONE				
};

/* Plustek Model: ???
 * KH: NS9831 + TPA + Button + NEC3799
 */
static DCapsDef Cap0x07B3_0x0014_2 =
{
	{{        0,       93},   0, {2550, 3508}, { 50,  50}, COLOR_BW     },
	{{1040 + 15, 744 - 32}, 543, { 473,  414}, {150, 150}, COLOR_GRAY16 },
	{{1004 + 20, 744 + 32}, 543, { 567,  414}, {150, 150}, COLOR_GRAY16 },
	{{        0,       95},   0, {2550, 3508}, { 50,  50}, COLOR_BW     },
	{600, 600},	
	0,
	SENSORORDER_bgr,	
	8, 0, 2, 0x04, _WAF_NONE					
};

#endif

/* Model: HP Scanjet 2100C */
static DCapsDef Cap0x03F0_0x0505 =
{
	{{        0,       93},   0, {2550, 3508}, { 50,  50}, COLOR_BW     },
 	{{1040 + 15, 744 - 32},	543, { 473,  414}, {150, 150}, COLOR_GRAY16 },
 	{{1004 + 20, 744 + 32}, 543, { 567,  414}, {150, 150}, COLOR_GRAY16 },
 	{{        0,       95},   0, {2550, 3508}, { 50,  50}, COLOR_BW     },
 	{600, 600},
 	0,
 	SENSORORDER_rgb,
 	4, 0, 0, 0x02, _WAF_NONE
};

/* Model: HP Scanjet 2200c (thanks to Stefan Nilsen)
 * KH: NS9832 + 2 Buttons + NEC3799(?) + 600 DPI Motor
 */
static DCapsDef Cap0x03F0_0x0605 =
{
	{{ 0, 180 },  /* OK */
	135,          /* OK, most values between 80 and 150 works, Might need
	                 tweaking on different hardware                       */
	{2550, 3508}, /* OK */
	{ 50,  50},   /* OK */
	COLOR_BW },   /* OK */
	
    {{ 0,  0}, 0, {0, 0}, { 0, 0 }, 0 }, /* No film scanner module    */
    {{ 0,  0}, 0, {0, 0}, { 0, 0 }, 0 }, /* No film scanner module    */
    {{ 0,  0}, 0, {0, 0}, { 0, 0 }, 0 }, /* No ADF                    */
	
	{600, 600},                          /* Motor can handle 1200 DPI */
	0,	                                 /* OK */
	SENSORORDER_rgb,	                 /* OK */
	4, 2, 0, 0x02, _WAF_NONE             /* OK */				
};

/* Mustek BearPaw 1200 (thanks to Henning Meier-Geinitz)
 * NS9831 + 5 Buttons + NEC548 (?)
 */
static DCapsDef Cap0x0400_0x1000_0 =
{
    {{ 67, 55}, 55, {2550, 3530}, { 75, 75 }, COLOR_BW },
    {{ 0,  0}, 0, {0, 0}, { 0, 0 }, 0 },
    {{ 0,  0}, 0, {0, 0}, { 0, 0 }, 0 },
    {{ 0,  0}, 0, {0, 0}, { 0, 0 }, 0 },
    {600, 600},/*1200*/
    0,
    SENSORORDER_bgr,
    4, 5, 3, 0x00, _WAF_NONE
};

/* Mustek BearPaw 2400
 * NS9832 + 5 Buttons + NEC548 (?)
 */
static DCapsDef Cap0x0400_0x1001_0 =
{
	{{ 15, 115}, 0, {2550, 3508}, { 100, 100 }, COLOR_BW },
    {{ 0,  0}, 0, {0, 0}, { 0, 0 }, 0 },
    {{ 0,  0}, 0, {0, 0}, { 0, 0 }, 0 },
    {{ 0,  0}, 0, {0, 0}, { 0, 0 }, 0 },
    {600, 600},
    0,
    SENSORORDER_bgr,
    6, 5, 2, 0x05, _WAF_NONE
};

/* Epson Perfection/Photo1250 (thanks to Gene Heskett and Reinhard Max)
 * NS9832 + 4 Buttons + CCD????
 */
static DCapsDef Cap0x04B8_0x010F_0 =
{
	{{ 25, 80}, 10, {2550, 3508}, { 100, 100 }, COLOR_BW },
	{{ 0,  0},   0, {0, 0}, { 0, 0 }, 0 },
	{{ 0,  0},   0, {0, 0}, { 0, 0 }, 0 },
	{{ 0,  0},   0, {0, 0}, { 0, 0 }, 0 },
	{1200, 1200},
	0,
	SENSORORDER_rgb,
	8,			        /* sensor distance                         */
	4,      	        /* number of buttons                       */
	kNEC8861,           /* use default settings during calibration */
	0,                  /* not used here...                        */
	_WAF_MISC_IO6_LAMP  /* use miscio 6 for lamp switching         */
};

/* Umax 3400
 */
static DCapsDef Cap0x1606_0x0060_0 =
{
	/* the ini file provided by umax says the scanner bed is 11.7", but
	   setting the value below to 3510 (11.7 * 300) results in the head
	   hitting the end at the end of the scan.  so i'm just guessing that
 	   the scanner bed area in the .ini file includes the dead area at
 	   the beginning, and the number below does not. */
 	{{ 0, 165}, 0, {2550, 3510 - 165}, {100, 100}, COLOR_BW },
 	{{ 0,  0}, 0, {0, 0}, { 0, 0 }, 0 },
 	{{ 0,  0}, 0, {0, 0}, { 0, 0 }, 0 },
 	{{ 0,  0}, 0, {0, 0}, { 0, 0 }, 0 },
 	{600, 600},
 	0,
 	SENSORORDER_bgr,
 	8,			        /* sensor distance                         */
 	4,		      	    /* number of buttons                       */
 	kNEC8861,           /* use default settings during calibration */
 	0,                  /* not used here...                        */
 	_WAF_MISC_IO3_LAMP  /* use miscio 3 for lamp switching         */
};

/******************* additional Hardware descriptions ************************/

static HWDef Hw0x07B3_0x0017_0 =
{
	1.5,	        /* dMaxMotorSpeed (Max_Speed)               */
	1.2,	        /* dMaxMoveSpeed (Max_Speed)                */
	9,		        /* wIntegrationTimeLowLamp                  */
	9,		        /* wIntegrationTimeHighLamp                 */
	300,	        /* wMotorDpi (Full step DPI)                */
	/* 100,	// wStartY (The top scanning origin in Full Steps)  */
	512,	        /* wRAMSize (KB)                            */
	4,		        /* wMinIntegrationTimeLowres (ms)           */
	5,		        /* wMinIntegrationTimeHighres (ms)          */
	3000,	        /* wGreenPWMDutyCycleLow                    */
	4095,	        /* wGreenPWMDutyCycleHigh                   */
	0x02,	        /* bSensorConfiguration (0x0b)              */
	0x04,	        /* bReg_0x0c                                */
	0x37,	        /* bReg_0x0d                                */
	0x13,	        /* bReg_0x0e                                */
    /* bReg_0x0f_Mono [10]	(0x0f to 0x18)                      */
	{2, 7, 0, 1, 0, 0, 0, 0, 4, 0},		
    /* bReg_0x0f_Color [10]	(0x0f to 0x18)                      */
	{5, 23, 1, 3, 0, 0, 0, 12, 10, 22},	
	
	_GREEN_CH,      /* bReg_0x26 color mode - bits 4 and 5      */
	0,              /* bReg 0x27 color mode                     */
	
	1,		        /* StepperPhaseCorrection (0x1a & 0x1b)     */
	14,		        /* 15,	bOpticBlackStart (0x1c)             */
	62,		        /* 60,	bOpticBlackEnd (0x1d)               */
	110,	        /* 65,	wActivePixelsStart (0x1e & 0x1f)    */
	5400,	        /* 5384 ,wLineEnd	(0x20 & 0x21)           */

    0,              /* red lamp on    (reg 0x2c + 0x2d)         */
    16383,          /* red lamp off   (reg 0x2e + 0x2f)         */
    0,              /* green lamp on  (reg 0x30 + 0x31)         */
    0,              /* green lamp off (reg 0x32 + 0x33)         */
    0,              /* blue lamp on   (reg 0x34 + 0x35)         */
    16383,          /* blue lamp off  (reg 0x36 + 0x37)         */

	/* Misc                                                     */
	3,		        /* bReg_0x45                                */
	0,		        /* wStepsAfterPaperSensor2 (0x4c & 0x4d)    */
	0xa8,	        /* 0xfc -bReg_0x51                          */
	0,		        /* bReg_0x54                                */
	0xff,	        /* 0xa3 - bReg_0x55                         */
	64,		        /* bReg_0x56                                */
	20,		        /* bReg_0x57                                */
	0x0d,	        /* bReg_0x58                                */
	0x22,	        /* bReg_0x59                                */
	0x82,	        /* bReg_0x5a                                */
	0x88,	        /* bReg_0x5b                                */
	0,		        /* bReg_0x5c                                */
	0,		        /* bReg_0x5d                                */
	0,		        /* bReg_0x5e                                */
	_LM9832,        /* chip type                                */
	MODEL_KaoHsiung	/* ScannerModel                             */
};

static HWDef Hw0x07B3_0x0007_0 =
{
	1.5, 1.2,
	9, 9,
	300,
	512,
	4, 5,
	3000, 4095,
	0x02, 0x14,	0x27, 0x13,
	{2, 7, 0, 1, 0, 0, 0, 0, 4, 0},
	{5, 23, 1, 3, 0, 0, 0, 6, 10, 22},
	_GREEN_CH,
	0,
	1,		
	14,		
	62,		
	110,	
	5384,	
    0,
    16383,
    0,
    0,
    0,
    16383,
	3,	
	0,	
	0xa8,
	0,	
	0xff,
	64,	
	20,	
	0x0d, 0x88, 0x28, 0x3b,
	0, 0, 0,	
	_LM9832,
	MODEL_HuaLien
};

static HWDef Hw0x07B3_0x0007_2 =
{
	1.4, 1.2,
	9, 9,
	600,
	512,
	4, 5,
	3000, 4095,
	0x02, 0x3f,	0x2f, 0x36,
	{2, 7, 0, 1, 0, 0, 0, 0, 4, 0},
	{7, 20, 1, 4, 7, 10, 0, 6, 12, 0},
	_GREEN_CH,
	0,
	1,
	16,
	64,
	152,
	5416,
    0,
    16383,
    0,
    0,
    0,
    16383,
	3,
	0,
	0xfc,
	0,
	0xff,
	64,
	20,
	0x0d, 0x88, 0x28, 0x3b,
	0, 0, 0,
	_LM9832,
	MODEL_Tokyo600
};

static HWDef Hw0x07B3_0x0007_4 =
{
	1.1, 0.9,
	12,	12,
	600,
	2048,
	8, 8,
	4095, 4095,
	0x06, 0x30,	0x2f, 0x2a,
	{2, 7, 5, 6, 6, 7, 0, 0, 0, 5},
	{20, 4, 13, 16, 19, 22, 0, 6, 23, 11},
	_GREEN_CH,
	0,
	1,
	13,
	62,
	304,
	10684,
    0,
    16383,
    0,
    0,
    0,
    16383,
	3,
	0,
	0xa8,
	0,
	0xff,
    24,
	40,
	0x0d, 0x88, 0x28, 0x3b,
	0, 0, 0,
	_LM9832,
	MODEL_HuaLien
};

static HWDef Hw0x07B3_0x000F_0 =
{
	1.5, 1.0,
	9, 9,
	300,
	512,
	4, 5,
	3000, 4095,
	0x02, 0x14,	0x27, 0x13,
	{2, 7, 0, 1, 0, 0, 0, 0, 4, 0},
	{5, 23, 1, 3, 0, 0, 0, 6, 10, 22},
	_GREEN_CH,
	0,
	1,
	14,
	62,
	110,
	5384,
    0,
    16383,
    0,
    0,
    0,
    16383,
	3,
	0,
	0xa8,
	0,
	0xff,
	64,
	20,
	0x05, 0x88,	0x08, 0x3b,
	0, 0, 0,
	_LM9832,
	MODEL_HuaLien
};

static HWDef Hw0x07B3_0x0013_0 =
{
	1.5, 1.2,
	9, 9,
	300,
	512,
	4, 5,
	3000, 4095,
	0x02, 0x04, 0x37, 0x13,
	{2, 7, 0, 1, 0, 0, 0, 0, 4, 0},
	{5, 23, 1, 3, 0, 0, 0, 12, 10, 22},
	_GREEN_CH,
	0,
	1,
	14,
	62,
	110,
	5400,
    0,
    16383,
    0,
    0,
    0,
    16383,
	3,
	0,
	0xa8,
	0,	
	0xff,
	64,	
	20,
	0x0d, 0x22,	0x82, 0x88,
	0, 0, 0,
	_LM9831,
	MODEL_KaoHsiung
};

static HWDef Hw0x07B3_0x0013_4 =
{
	1.0, 0.9,
	12, 12,
	600,	
	2048,
	8, 8,
	4095, 4095,
	0x06, 0x20, 0x2f, 0x2a,
	{2, 7, 5, 6, 6, 7, 0, 0, 0, 5},
	{20, 4, 13, 16, 19, 22, 0, 0, 23, 11},
	_GREEN_CH,
	0,
	1,		
	13,
	62,	
	320,	
	10684,	
    0,
    16383,
    0,
    0,
    0,
    16383,
	3,		
	0,		
	0xa8,	
	0,		
	0xff,	
	10,		
	48,		
	0x0d, 0x22,	0x82, 0x88,
	0, 0, 0,
	_LM9831,
	MODEL_KaoHsiung
};

static HWDef Hw0x07B3_0x000F_4 =
{
	1.1, 0.9,
	12,	12,
	600,	
	2048,	
	8, 8,		
	4095, 4095,
	0x06, 0x30,	0x2f, 0x2a,
	{2, 7, 5, 6, 6, 7, 0, 0, 0, 5},
	{20, 4, 13, 16, 19, 22, 0, 6, 23, 11},
	_GREEN_CH,
	0,
	1,
	13,
	62,
	304,
	10684,
    0,
    16383,
    0,
    0,
    0,
    16383,
	3,
	0,
	0xa8,
	0,
	0xff,
	24,
	40,
	0x05, 0x88, 0x08, 0x3b,
	0, 0, 0,
	_LM9832,
	MODEL_HuaLien
};

static HWDef Hw0x07B3_0x0016_4 =
{
	1.0, 0.9,
	12,	12,
	600,	
	2048,	
	8, 8,
	4095, 4095,
	0x06, 0x20,	0x2f, 0x2a,
	{2, 7, 5, 6, 6, 7, 0, 0, 0, 5},
	{20, 4, 13, 16, 19, 22, 0, 0, 23, 11},
	_GREEN_CH,
	0,
	1,		
	13,		
	62,		
	320,
	10684,
    0,
    16383,
    0,
    0,
    0,
    16383,
	3,
	0,
	0xa8,
	0,
	0xff,
	10,	
	48,	
	0x0d, 0x22,	0x82, 0x88,
	0, 0, 0,
	_LM9832,
	MODEL_KaoHsiung
};

/*
 * Plustek OpticPro UT24 and others...
 */
static HWDef Hw0x07B3_0x0017_4 =
{
	1.0, 0.9,
	12, 12,		
	600,	
	2048,
	8, 8,	
	4095, 4095,
	0x06, 0x20,	0x2f, 0x2a,
	{2, 7, 5, 6, 6, 7, 0, 0, 0, 5},	
	{20, 4, 13, 16, 19, 22, 0, 0, 23, 11},
	_GREEN_CH,
	0,
	1,		
	13,		
	62,		
	320,	
	10684,	
    0,
    16383,
    0,
    0,
    0,
    16383,
	3,		
	0,		
	0xa8,	
	0,		
	0xff,
	10,		
	48,		
	0x0d, 0x22, 0x82, 0x88,	
	0, 0, 0,
	_LM9832,
	MODEL_KaoHsiung	
};

static HWDef Hw0x07B3_0x0017_1 =
{
	1.5, 1.5,
	9, 9,
	200,
	2048,
	4, 5,
	3000, 4095,
	0x02, 0x08, 0x2f, 0x36,
	{2, 7, 0, 1, 0, 0, 0, 0, 4, 0},
	{5, 23, 1, 4, 7, 10, 0, 0, 10, 12},
	_GREEN_CH,
	0,
	1,
	15,
	60,
	110,
	5415,
    0,
    16383,
    0,
    0,
    0,
    16383,
	3,	
	0,	
	0xa8,
	0,	
	0xff,
	64,		
	20,		
	0x0d, 0x22,	0x82, 0x88,
	0, 0, 0,
	_LM9832,
	MODEL_KaoHsiung
};

static HWDef Hw0x07B3_0x0012_0 =
{
	1.5, 1.4,	
	9, 9,		
	600,	
	2048,	
	4, 5,		
	3000, 4095,	
	0x02, 0x04, 0x37, 0x13,	
	{2, 7, 0, 1, 0, 0, 0, 0, 4, 0},		
	{5, 23, 1, 3, 0, 0, 0, 12, 10, 22},	
	_GREEN_CH,
	0,
	1,		
	14,		
	62,		
	110,	
	5400,	
    0,
    16383,
    0,
    0,
    0,
    16383,
    3,		
	0,		
	0xa8,	
	0,		
	0xff,	
	64,		
	20,		
	0x0d, 0x22, 0x82, 0x88,	
	0, 0, 0,		
	_LM9832,
	MODEL_KaoHsiung	
};

static HWDef Hw0x07B3_0x0017_2 =
{
	1.5, 1.2,	
	9, 9,		
	300,	
	512,	
	4, 5,		
	3000, 4095,	
	0x02, 0, 0x2f, 0x36,	
	{2, 7, 0, 1, 0, 0, 0, 0, 4, 0},
	{5, 0, 1, 4, 7, 10, 0, 0, 12, 0},
	_GREEN_CH,
	0,
	1,		
	16,		
	64,		
	110,	
	5416,	
    0,
    16383,
    0,
    0,
    0,
    16383,
	3,		
	0,		
	0xa8,	
	0,		
	0xff,	
	64,		
	20,		
	0x0d, 0x22,	0x82, 0x88,	
	0, 0, 0,		
	_LM9832,
	MODEL_KaoHsiung
};

static HWDef Hw0x07B3_0x0017_3 =
{
	1.5, 1.2,	
	9, 9,		
	300,	
	512,
	4, 5,		
	3000, 4095,	
	0x02, 0x04, 0x37, 0x13,	
	{2, 7, 0, 1, 0, 0, 0, 0, 4, 0},		
	{5, 23, 1, 4, 7, 10, 0, 0, 11, 23},	
	_GREEN_CH,
	0,
	1,		
	14,		
	62,		
	110,	
	5400,	
    0,
    16383,
    0,
    0,
    0,
    16383,
	3,		
	0,		
	0xa8,	
	0,		
	0xff,	
	64,		
	20,		
	0x0d, 0x22,	0x82, 0x88,	
	0, 0, 0,		
	_LM9832,
	MODEL_KaoHsiung	
};

/* HP Scanjet 2100C */
static HWDef Hw0x03F0_0x0505 =
{
 	1.5,	/* dMaxMotorSpeed (Max_Speed)               */
 	1.2,	/* dMaxMoveSpeed (Max_Speed)                */
 	9,		/* wIntegrationTimeLowLamp                  */
 	9,		/* wIntegrationTimeHighLamp                 */
 	600,	/* ok wMotorDpi (Full step DPI)             */
 	512,	/* wRAMSize (KB)                            */
 	4,		/* wMinIntegrationTimeLowres (ms)           */
 	5,		/* wMinIntegrationTimeHighres (ms)          */
 	3000,	/* wGreenPWMDutyCycleLow                    */
 	4095,	/* wGreenPWMDutyCycleHigh                   */
 	0x02,	/* bSensorConfiguration (0x0b)              */
 	0x04,	/* bReg_0x0c                                */
 	0x37,	/* bReg_0x0d                                */
 	0x13,	/* bReg_0x0e                                */
 		    /* bReg_0x0f_Mono[10] (0x0f to 0x18)        */
 	{2, 7, 0, 1, 0, 0, 0, 0, 4, 0},
     		/* bReg_0x0f_Color[10] (0x0f to 0x18)       */
  	{5, 23, 1, 3, 0, 0, 0, 12, 10, 22},

	_GREEN_CH,	/* bReg_0x26 color mode - bits 4 and 5  */
	0,          /* bReg 0x27 color mode                 */
  	
 	1,		/* StepperPhaseCorrection (0x1a & 0x1b)     */
 	14,		/* 15,= bOpticBlackStart (0x1c)             */
 	62,		/* 60,= bOpticBlackEnd (0x1d)               */
 	110,	/* 65,= wActivePixelsStart (0x1e & 0x1f)    */
 	5400,	/* 5384 ,wLineEnd=(0x20 & 0x21)         	*/

    0,      /* red lamp on    (reg 0x2c + 0x2d)         */
    16383,  /* red lamp off   (reg 0x2e + 0x2f)         */
    0,      /* green lamp on  (reg 0x30 + 0x31)         */
    0,      /* green lamp off (reg 0x32 + 0x33)         */
    0,      /* blue lamp on   (reg 0x34 + 0x35)         */
    16383,  /* blue lamp off  (reg 0x36 + 0x37)         */
 	
 	/* Misc                                             */
 	3,		/* bReg_0x45                                */
 	0,		/* wStepsAfterPaperSensor2 (0x4c & 0x4d)    */
 	0xa8,	/* 0xfc -bReg_0x51                          */
 	0,		/* bReg_0x54                                */
 	0xff,	/* 0xa3 - bReg_0x55                         */
 	64,		/* bReg_0x56                                */
 	20,		/* bReg_0x57                                */
 	0x0d,	/* bReg_0x58                                */
 	0x22,	/* bReg_0x59                                */
 	0x82,	/* bReg_0x5a                                */
 	0x88,	/* bReg_0x5b                                */
 	0,		/* bReg_0x5c                                */
 	0,		/* bReg_0x5d                                */
 	0,		/* bReg_0x5e                                */
	_LM9832,                /* chiptype                 */
	MODEL_NOPLUSTEK			/* ScannerModel             */
};

/* HP 2200C */
static HWDef Hw0x03F0_0x0605 =
{
	1.05,	/* dMaxMotorSpeed (Max_Speed)               */
	1.05,	/* dMaxMoveSpeed (Max_Speed)                */
	12,		/* wIntegrationTimeLowLamp                  */
	12,		/* wIntegrationTimeHighLamp                 */
	600,	/* ok wMotorDpi (Full step DPI)             */
	512,	/* wRAMSize (KB)                            */
	9,		/* wMinIntegrationTimeLowres (ms)           */
	9,		/* wMinIntegrationTimeHighres (ms)          */
	0,	    /* wGreenPWMDutyCycleLow                    */
	0,	    /* wGreenPWMDutyCycleHigh                   */
	0x02,	/* bSensorConfiguration (0x0b)              */
	0x04,	/* bReg_0x0c                                */
	0x37,	/* bReg_0x0d                                */
	0x13,	/* bReg_0x0e                                */
		    /* bReg_0x0f_Mono[10] (0x0f to 0x18)        */
	{2, 7, 0, 1, 0, 0, 0, 0, 4, 0},
    		/* bReg_0x0f_Color[10] (0x0f to 0x18)       */
 	{5, 23, 1, 3, 0, 0, 0, 12, 10, 22},
	
	_GREEN_CH,	/* bReg_0x26 color mode - bits 4 and 5  */
	0,          /* bReg 0x27 color mode                 */

	1,		/* StepperPhaseCorrection (0x1a & 0x1b)     */
	14,		/* 15,= bOpticBlackStart (0x1c)             */
	62,		/* 60,= bOpticBlackEnd (0x1d)               */
	110,	/* 65,= wActivePixelsStart (0x1e & 0x1f)    */
	5384,	/* 5400 (?) ,wLineEnd=(0x20 & 0x21)       	*/

    0,      /* red lamp on    (reg 0x2c + 0x2d)         */
    16383,  /* red lamp off   (reg 0x2e + 0x2f)         */
    0,      /* green lamp on  (reg 0x30 + 0x31)         */
    0,      /* green lamp off (reg 0x32 + 0x33)         */
    0,      /* blue lamp on   (reg 0x34 + 0x35)         */
    16383,  /* blue lamp off  (reg 0x36 + 0x37)         */
	
	/* Misc                                             */
	3,		/* bReg_0x45                                */
	0,		/* wStepsAfterPaperSensor2 (0x4c & 0x4d)    */
	0xa8,	/* 0xfc -bReg_0x51                          */
	0,		/* bReg_0x54                                */
	0xff,	/* 0xa3 - bReg_0x55                         */
	64,		/* bReg_0x56                                */
	20,		/* bReg_0x57                                */
	0x0d,	/* bReg_0x58                                */
	0x22,	/* bReg_0x59                                */
	0x82,	/* bReg_0x5a                                */
	0x88,	/* bReg_0x5b                                */
	0,		/* bReg_0x5c                                */
	0,		/* bReg_0x5d                                */
	0,		/* bReg_0x5e                                */
	_LM9832,                /* chiptype                 */
	MODEL_NOPLUSTEK			/* ScannerModel             */
};

/* Mustek BearPaw 1200 */
static HWDef Hw0x0400_0x1000_0 =
{
     1.25,   /* ok dMaxMotorSpeed (Max_Speed)                */
     1.25,   /* ok dMaxMoveSpeed (Max_Speed)                 */
     12,     /* ok wIntegrationTimeLowLamp                   */
     12,     /* ok wIntegrationTimeHighLamp                  */
     600,    /* ok wMotorDpi (Full step DPI)                 */
     512,    /* ok wRAMSize (KB)                             */
     9,      /* ok wMinIntegrationTimeLowres (ms)            */
     9,      /* ok wMinIntegrationTimeHighres (ms)           */
     1169,   /* ok wGreenPWMDutyCycleLow (reg 0x2a + 0x2b)   */
     1169,   /* ok wGreenPWMDutyCycleHigh (reg 0x2a + 0x2b)  */
     0x02,   /* ok bSensorConfiguration (0x0b)               */
     0x7c,   /* ok sensor control settings (reg 0x0c)        */
     0x3f,   /* ok sensor control settings (reg 0x0d)        */
     0x15,   /* ok sensor control settings (reg 0x0e)        */
     {0x04, 0x07, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x03, 0x06},
             /* ok  mono (reg 0x0f to 0x18) */
     {0x04, 0x16, 0x01, 0x02, 0x05, 0x06, 0x00, 0x00, 0x0a, 0x16},
             /* ok color (reg 0x0f to 0x18)                  */
	 _GREEN_CH,	 /* bReg_0x26 color mode - bits 4 and 5      */
	 0,          /* bReg 0x27 color mode                     */
     257,    /* ok StepperPhaseCorrection (reg 0x1a + 0x1b)  */
     0x0e,   /* ok bOpticBlackStart (reg 0x1c)               */
     0x1d,   /* ok bOpticBlackEnd (reg 0x1d)                 */
     0,      /* ok wActivePixelsStart (reg 0x1e + 0x1f)      */
     5369,   /* ok wLineEnd (reg 0x20 + 0x21)                */
     0,      /* red lamp on    (reg 0x2c + 0x2d)             */
     16383,  /* red lamp off   (reg 0x2e + 0x2f)             */
     0,      /* green lamp on  (reg 0x30 + 0x31)             */
     0,      /* green lamp off (reg 0x32 + 0x33)             */
     0,      /* blue lamp on   (reg 0x34 + 0x35)             */
     16383,  /* blue lamp off  (reg 0x36 + 0x37)             */
     0x13,   /* ok stepper motor control (reg 0x45)          */
     0,      /* ?  wStepsAfterPaperSensor2 (reg 0x4c + 0x4d) */
     0xfc,   /* ok acceleration profile (reg 0x51)           */
     0,      /* ok lines to process (reg 0x54)               */
     0x13,   /* ok kickstart (reg 0x55)                      */
     0x03,   /* ok pwm freq (reg 0x56)                       */
     0x20,   /* ok pwm duty cycle (reg 0x57)                 */
     0x15,   /* ok Paper sense (reg 0x58)                    */
     0x44,   /* ok misc io12 (reg 0x59)                      */
     0x44,   /* ok misc io34 (reg 0x5a)                      */
     0x4f,   /* ok misc io56 (reg 0x5b)                      */
     0,      /* ok test mode ADC Output CODE MSB (reg 0x5c)  */
     0,      /* ok test mode ADC Output CODE LSB (reg 0x5d)  */
     0,      /* ok test mode (reg 0x5e)                      */
	_LM9831,
     MODEL_Tokyo600
};

/* BearPaw 2400 */
static HWDef Hw0x0400_0x1001_0 =
{
	1.0, 0.9,
    9, 9,
    1200,
    2048,
    4, 5,
    3000, 4095,
    0x02, 0x3f,     0x2f, 0x36,
    {2, 7, 0, 1, 0, 0, 0, 0, 4, 0},
    {7, 20, 1, 4, 7, 10, 0, 6, 12, 0},
    _GREEN_CH,
    0,
    1,
    16,
    64,
    152,
    5416,
    0,
    16383,
    0,
    0,
    0,
    16383,
    3,
    0,
    0xfc,
    0,
    0xff,
    64,
    20,
    0x0d, 0x88, 0x28, 0x3b,
    0, 0, 0,
	_LM9832,
    MODEL_Tokyo600
};

/* EPSON Perfection/Photo 1250 */
static HWDef Hw0x04B8_0x010F_0 =
{
    1.0,    /* dMaxMotorSpeed (Max_Speed)                */
    0.9,    /* dMaxMoveSpeed (Max_Speed)                 */
    12,     /* wIntegrationTimeLowLamp                   */
    12,     /* wIntegrationTimeHighLamp                  */
    600,    /* wMotorDpi (Full step DPI)                 */
    512,    /* wRAMSize (KB)                             */
    4,      /* wMinIntegrationTimeLowres (ms)            */
    5,      /* wMinIntegrationTimeHighres (ms)           */
    3000,   /* ok wGreenPWMDutyCycleLow (reg 0x2a + 0x2b)   */
    4095,   /* ok wGreenPWMDutyCycleHigh (reg 0x2a + 0x2b)  */

    0x02,   /* ok bSensorConfiguration (0x0b)               */
    0x04,   /* ok sensor control settings (reg 0x0c)        */
    0x7f,   /* ok sensor control settings (reg 0x0d)        */
    0x13,   /* ok sensor control settings (reg 0x0e)        */

    {0x02, 0x07, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00},
            /* ?? mono (reg 0x0f to 0x18) */

    {0x06, 0x16, 0x00, 0x05, 0x0c, 0x17, 0x00, 0x00, 0x08, 0x14},
            /* ok color (reg 0x0f to 0x18)                  */
	_GREEN_CH,	/* ok bReg_0x26 color mode - bits 4 and 5   */
	0x40,       /* ok bReg 0x27 color mode                  */

    1,      /* ok StepperPhaseCorrection (reg 0x1a + 0x1b)  */
    0x00,   /* ok bOpticBlackStart (reg 0x1c)               */
    0x42,   /* ok bOpticBlackEnd (reg 0x1d)                 */
    69,     /* ok wActivePixelsStart (reg 0x1e + 0x1f)      */
    10766,  /* ok wLineEnd (reg 0x20 + 0x21)                */

    16383,  /* ok red lamp on    (reg 0x2c + 0x2d)          */
    0,      /* ok red lamp off   (reg 0x2e + 0x2f)          */
    16383,  /* ok green lamp on  (reg 0x30 + 0x31)          */
    0,      /* ok green lamp off (reg 0x32 + 0x33)          */
    16383,  /* ok blue lamp on   (reg 0x34 + 0x35)          */
    0,      /* ok blue lamp off  (reg 0x36 + 0x37)          */

    3,      /* ok stepper motor control (reg 0x45)          */
    0,      /* ok wStepsAfterPaperSensor2 (reg 0x4c + 0x4d) */
    0x0c,   /* ok acceleration profile (reg 0x51)           */
    0,      /* ok lines to process (reg 0x54)               */
    0x0f,   /* ok kickstart (reg 0x55)                      */
    0x02,   /* ok pwm freq (reg 0x56)                       */
    1,      /* ok pwm duty cycle (reg 0x57)                 */

    0x0d,   /* ok Paper sense (reg 0x58)                    */

    0x41,   /* ok misc io12 (reg 0x59)                      */
    0x44,   /* ok misc io34 (reg 0x5a)                      */
    0x49,   /* ok misc io56 (reg 0x5b)                      */
    0,      /* ok test mode ADC Output CODE MSB (reg 0x5c)  */
    0,      /* ok test mode ADC Output CODE LSB (reg 0x5d)  */
    0,      /* ok test mode (reg 0x5e)                      */
	_LM9832,
    MODEL_NOPLUSTEK
};

/* Umax 3400 */
static HWDef Hw0x1606_0x0060_0 =
{
    1.7,    /* dMaxMotorSpeed (Max_Speed)                */
    0.8,    /* dMaxMoveSpeed (Max_Speed)                 */
    9,      /* wIntegrationTimeLowLamp                   */
    9,      /* wIntegrationTimeHighLamp                  */
    600,    /* wMotorDpi (Full step DPI)                 */
    512,    /* wRAMSize (KB)                             */
    8,      /* wMinIntegrationTimeLowres (ms)            */
    8,      /* wMinIntegrationTimeHighres (ms)           */
    4095,   /* wGreenPWMDutyCycleLow (reg 0x2a + 0x2b)   */
    4095,   /* wGreenPWMDutyCycleHigh (reg 0x2a + 0x2b)  */

    0x06,   /* bSensorConfiguration (0x0b)               */
    0x73,   /* sensor control settings (reg 0x0c)        */
    0x77,   /* sensor control settings (reg 0x0d)        */
    0x15,   /* sensor control settings (reg 0x0e)        */

    {0x00, 0x03, 0x04, 0x05, 0x00, 0x00, 0x00, 0x00, 0x07, 0x03},
                /* mono (reg 0x0f to 0x18) */

     {0x01, 0x0c, 0x0e, 0x10, 0x00, 0x00, 0x00, 0x00, 0x16, 0x0c},
                /* color (reg 0x0f to 0x18)              */
 	_GREEN_CH,	/* bReg_0x26 color mode - bits 4 and 5   */
 	0x40,       /* bReg 0x27 color mode                  */

    1,      /* StepperPhaseCorrection (reg 0x1a + 0x1b)  */
    0x2f,   /* bOpticBlackStart (reg 0x1c)               */
    0x3e,   /* bOpticBlackEnd (reg 0x1d)                 */
    110,    /* ? wActivePixelsStart (reg 0x1e + 0x1f)    */
    5469,   /* wLineEnd (reg 0x20 + 0x21)                */

    1,      /* red lamp on    (reg 0x2c + 0x2d)          */
    16383,  /* red lamp off   (reg 0x2e + 0x2f)          */
    0,      /* green lamp on  (reg 0x30 + 0x31)          */
    0,      /* green lamp off (reg 0x32 + 0x33)          */
    32,     /* blue lamp on   (reg 0x34 + 0x35)          */
    48,     /* blue lamp off  (reg 0x36 + 0x37)          */

    3,      /* stepper motor control (reg 0x45)          */
    0,      /* wStepsAfterPaperSensor2 (reg 0x4c + 0x4d) */
    0xf4,   /* acceleration profile (reg 0x51)           */
    0,      /* lines to process (reg 0x54)               */
    0xcb,   /* kickstart (reg 0x55)                      */
    0x05,   /* pwm freq (reg 0x56)                       */
    5,      /* pwm duty cycle (reg 0x57)                 */

    0x0d,   /* Paper sense (reg 0x58)                    */

    0x44,   /* misc io12 (reg 0x59)                      */
    0x45,   /* misc io34 (reg 0x5a)                      */
    0x7c,   /* misc io56 (reg 0x5b)                      */
    0,      /* test mode ADC Output CODE MSB (reg 0x5c)  */
    0,      /* test mode ADC Output CODE LSB (reg 0x5d)  */
    0,      /* test mode (reg 0x5e)                      */
    _LM9832,
	MODEL_NOPLUSTEK
};

/******************** all available combinations *****************************/

/*
 * here we have all supported devices and their settings...
 */
static SetDef Settings[] =
{
	/* Plustek devices... */
	{"0x07B3-0x0013-0", &Cap0x07B3_0x0013_0, &Hw0x07B3_0x0013_0, "Unknown device" },
	{"0x07B3-0x0011-0", &Cap0x07B3_0x0011_0, &Hw0x07B3_0x0013_0, "OpticPro U24"   },
	{"0x07B3-0x0010-0", &Cap0x07B3_0x0010_0, &Hw0x07B3_0x0013_0, "OpticPro U12"   },
	{"0x07B3-0x0013-4", &Cap0x07B3_0x0013_4, &Hw0x07B3_0x0013_4, "Unknown device" },
	{"0x07B3-0x0011-4", &Cap0x07B3_0x0011_4, &Hw0x07B3_0x0013_4, "Unknown device" },
	{"0x07B3-0x0010-4", &Cap0x07B3_0x0010_4, &Hw0x07B3_0x0013_4, "Unknown device" },

	{"0x07B3-0x0017-0", &Cap0x07B3_0x0017_0, &Hw0x07B3_0x0017_0, "OpticPro UT12/UT16" },
	{"0x07B3-0x0015-0", &Cap0x07B3_0x0015_0, &Hw0x07B3_0x0017_0, "OpticPro U24"   },
	{"0x07B3-0x0014-0", &Cap0x07B3_0x0014_0, &Hw0x07B3_0x0017_0, "Unknown device" },
	{"0x07B3-0x0017-4", &Cap0x07B3_0x0017_4, &Hw0x07B3_0x0017_4, "OpticPro UT24"  },
	{"0x07B3-0x0015-4", &Cap0x07B3_0x0015_4, &Hw0x07B3_0x0017_4, "Unknown device" },
	{"0x07B3-0x0014-4", &Cap0x07B3_0x0014_4, &Hw0x07B3_0x0017_4, "Unknown device" },
	{"0x07B3-0x0016-4", &Cap0x07B3_0x0016_4, &Hw0x07B3_0x0016_4, "Unknown device" },
	{"0x07B3-0x0017-2", &Cap0x07B3_0x0017_2, &Hw0x07B3_0x0017_2, "Unknown device" },
	{"0x07B3-0x0017-3", &Cap0x07B3_0x0017_3, &Hw0x07B3_0x0017_3, "Unknown device" },
	
	{"0x07B3-0x0007",	&Cap0x07B3_0x0007_0, &Hw0x07B3_0x0007_0, "Unknown device" },
	{"0x07B3-0x000F",	&Cap0x07B3_0x000F_0, &Hw0x07B3_0x000F_0, "Unknown device" },
	{"0x07B3-0x000F-4",	&Cap0x07B3_0x000F_4, &Hw0x07B3_0x000F_4, "Unknown device" },
	{"0x07B3-0x0005-2",	&Cap0x07B3_0x0005_2, &Hw0x07B3_0x0007_2, "Unknown device" }, /* TOKYO 600 */
	{"0x07B3-0x0014-1",	&Cap0x07B3_0x0014_1, &Hw0x07B3_0x0017_1, "Unknown device" }, /* A3 */
	{"0x07B3-0x0012-0",	&Cap0x07B3_0x0012_0, &Hw0x07B3_0x0012_0, "Unknown device" }, /* Brother Demo */
	
	/* Mustek BearPaw...*/
    {"0x0400-0x1000",   &Cap0x0400_0x1000_0, &Hw0x0400_0x1000_0, "BearPaw 1200" },
	{"0x0400-0x1001",	&Cap0x0400_0x1001_0, &Hw0x0400_0x1001_0, "BearPaw 2400" },
    	
	/* Genius devices... */
	{"0x0458-0x2007",	&Cap0x07B3_0x0007_0, &Hw0x07B3_0x0007_0, "ColorPage-HR6 V2" },
	{"0x0458-0x2008",	&Cap0x07B3_0x0007_0, &Hw0x07B3_0x0007_0, "Unknown device" },
	{"0x0458-0x2009",	&Cap0x07B3_0x000F_0, &Hw0x07B3_0x000F_0, "Unknown device" },
	{"0x0458-0x2013",	&Cap0x07B3_0x0007_4, &Hw0x07B3_0x0007_4, "Unknown device" },
	{"0x0458-0x2015",	&Cap0x07B3_0x0005_4, &Hw0x07B3_0x0007_4, "Unknown device" },
	{"0x0458-0x2016",	&Cap0x07B3_0x0005_4, &Hw0x07B3_0x0007_0, "Unknown device" },

	/* Hewlett Packard... */
 	{"0x03F0-0x0505",	&Cap0x03F0_0x0505, &Hw0x03F0_0x0505, "Scanjet 2100c" },
	{"0x03F0-0x0605",	&Cap0x03F0_0x0605, &Hw0x03F0_0x0605, "Scanjet 2200c" },

	/* EPSON... */
	{"0x04B8-0x010F",	&Cap0x04B8_0x010F_0, &Hw0x04B8_0x010F_0, "Perfection 1250/Photo" },

	/* UMAX... */
 	{"0x1606-0x0060",	&Cap0x1606_0x0060_0, &Hw0x1606_0x0060_0, "UMAX 3400" },
		
	/* CANON... */
/*	{"0x04A9-0x220D", ,, "N670U" }, */
		
	/* Please add other devices here...
	 * The first entry is a string, composed out of the vendor and product id,
	 * it's used by the driver to select the device settings. For other devices
	 * than those of Plustek, you'll not need to add the second '-' part
	 *
	 * The second entry decribes the capabilities of the device, you may find
	 * one suitable for your scanner, for a better description of the entries
	 * have a look at the beginning of this file at Cap0x07B3_0x0017_0 for
	 * the UT12
	 *
	 * The third entry is for the default setting of the LM983x register
	 * settings, you can often find these in you Windoze driver ini
	 * Have a look at the Hw0x0400_0x1000_0 or Hw0x07B3_0x0017_0 for further
	 * description
	 *
	 * The fourth entry is simply the name of the device, which will be
	 * displayed by the frontend
	 */
    { NULL, NULL, NULL, NULL }  /* last entry, never remove... */
};

/* END PLUSTEK-DEVS.C .......................................................*/
