/*.............................................................................
 * Project : SANE library for Plustek flatbed scanners.
 *.............................................................................
 * File:	 plustek.h - definitions for the backend
 *.............................................................................
 *
 * based on Kazuhiro Sasayama previous
 * Work on plustek.[ch] file from the SANE package.
 *
 * original code taken from sane-0.71
 * Copyright (C) 1997 Hypercore Software Design, Ltd.
 *
 * modifications
 * Copyright (C) 1998 Christian Bucher
 * Copyright (C) 1998 Kling & Hautzinger GmbH
 * Last Update:
 *		Gerhard Jaeger <g.jaeger@earthling.net>
 *.............................................................................
 * History:
 * 0.30 - initial version
 * 0.31 - no changes
 * 0.32 - no changes
 * 0.33 - no changes
 * 0.34 - moved some definitions and typedefs from plustek.c
 * 0.35 - removed OPT_MODEL from options list
 *		  added max_y in struct Plustek_Scan
 * 0.36 - added reader_pid, pipe and bytes_read to struct Plustek_Scanner
 *		  removed unused variables from struct Plustek_Scanner
 *
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
#ifndef __PLUSTEK_H__
#define __PLUSTEK_H__

/************************ some definitions ***********************************/

#define MM_PER_INCH         25.4

#define PLUSTEK_CONFIG_FILE	"plustek.conf"

#ifndef PATH_MAX
# define PATH_MAX 1024
#endif

#define  walloc(x)	( x *) malloc( sizeof( x) )
#define  walloca(x)	( x *) alloca( sizeof( x) )

/*
 * the default image size
 */
#define _DEFAULT_TLX  		0		/* 0..216 mm */
#define _DEFAULT_TLY  		0		/* 0..297 mm */
#define _DEFAULT_BRX		126		/* 0..216 mm*/
#define _DEFAULT_BRY		76.21	/* 0..297 mm */

#define _DEFAULT_TP_TLX  	3.5		/* 0..42.3 mm */
#define _DEFAULT_TP_TLY  	10.5	/* 0..43.1 mm */
#define _DEFAULT_TP_BRX		38.5	/* 0..42.3 mm */
#define _DEFAULT_TP_BRY		33.5	/* 0..43.1 mm */

#define _DEFAULT_NEG_TLX  	1.5		/* 0..38.9 mm */
#define _DEFAULT_NEG_TLY  	1.5		/* 0..29.6 mm */
#define _DEFAULT_NEG_BRX	37.5	/* 0..38.9 mm */
#define _DEFAULT_NEG_BRY	25.5	/* 0..29.6 mm */

/*
 * image sizes for normal, transparent and negative modes
 */
#define _NORMAL_X		216.0
#define _NORMAL_Y		297.0
#define _TP_X			((double)_TPAPageWidth/300.0 * MM_PER_INCH)
#define _TP_Y			((double)_TPAPageHeight/300.0 * MM_PER_INCH)
#define _NEG_X			((double)_NegativePageWidth/300.0 * MM_PER_INCH)
#define _NEG_Y			((double)_NegativePageHeight/300.0 * MM_PER_INCH)

/************************ some structures ************************************/

enum {
    OPT_NUM_OPTS = 0,
    OPT_MODE_GROUP,
    OPT_MODE,
	OPT_EXT_MODE,
    OPT_HALFTONE,
    OPT_DROPOUT,
    OPT_BRIGHTNESS,
    OPT_CONTRAST,
    OPT_RESOLUTION,
    OPT_GEOMETRY_GROUP,
    OPT_TL_X,
    OPT_TL_Y,
    OPT_BR_X,
    OPT_BR_Y,
    NUM_OPTIONS
};

/*
 * our own decription for some options
 */
#define PLUSTEK_DESC_SCAN_SOURCE \
"Selects the picture mode."

typedef struct
{
    SANE_Device sane;
	SANE_Int	model;
	SANE_Int	asic;
	SANE_Int	max_y;
    SANE_Int 	level;
    SANE_Range 	dpi_range;
    SANE_Range 	x_range;
    SANE_Range 	y_range;
    SANE_Int   *res_list;
    SANE_Int 	res_list_size;
} Plustek_Device;

typedef union
{
	SANE_Word w;
	SANE_Word *wa;		/* word array */
	SANE_String s;
} Option_Value;

typedef struct
{
	int 					fd;
    pid_t 					reader_pid;		/* process id of reader          */
    int 					pipe;			/* pipe to reader process        */
	unsigned long			bytes_read;		/* number of bytes currently read*/
    Plustek_Device 		   *hw;				/* pointer to current device     */
    Option_Value 			val[NUM_OPTIONS];
    SANE_Byte 			   *buf;            /* the image buffer              */
    SANE_Bool 				scanning;       /* TRUE during scan-process      */
    SANE_Parameters 		params;         /* for keeping the parameter     */
    SANE_Option_Descriptor	opt[NUM_OPTIONS];
} Plustek_Scanner;


typedef const struct mode_param {
	int color;
	int depth;
	int scanmode;
} ModeParam, *pModeParam;

#endif	/* guard __PLUSTEK_H__ */

/* END PLUSTEK.H.............................................................*/
