/*
 * epson2.c - SANE library for Epson scanners.
 *
 * Based on Kazuhiro Sasayama previous
 * Work on epson.[ch] file from the SANE package.
 * Please see those files for original copyrights.
 *
 * Copyright (C) 2006 Tower Technologies
 * Author: Alessandro Zummo <a.zummo@towertech.it>
 *
 * This file is part of the SANE package.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2.
 */

#define	SANE_EPSON2_VERSION	"SANE Epson 2 Backend v0.1.12 - 2006-12-07"
#define SANE_EPSON2_BUILD	112

/* debugging levels:
 *
 *     127	epson_recv buffer
 *     125	epson_send buffer
 *	20	usb cmd counters
 *	15	epson_send, epson_recv calls
 *	12	epson_cmd_simple
 *	10	some more details on scanner commands
 *	 9	ESC x/FS I in epson_send
 *	 8	scanner commands
 *	 5
 *	 4
 *	 3	status information
 *	 1	scanner info and capabilities
 *		warnings
 */

#include <sane/config.h>

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <byteorder.h>

#include <sane/sane.h>
#include <sane/saneopts.h>
#include <sane/sanei_scsi.h>
#include <sane/sanei_usb.h>
#include <sane/sanei_pio.h>
#include <sane/sanei_tcp.h>
#include <sane/sanei_backend.h>
#include <sane/sanei_config.h>

#include "epson2.h"
#include "epson2_scsi.h"
#include "epson_usb.h"
#include "epson2_net.h"

#define EPSON2_CONFIG_FILE	"epson2.conf"

#ifndef PATH_MAX
#define PATH_MAX		(1024)
#endif

#ifndef MM_PER_INCH
#define MM_PER_INCH		25.4
#endif

#ifdef __GNUC__
#define __func__ __FUNCTION__
#else
#define __func__ "(undef)"
/* I cast my vote for C99... :) */
#endif

#ifndef XtNumber
#define XtNumber(x)  (sizeof(x) / sizeof(x[0]))
#define XtOffset(p_type, field)  ((size_t)&(((p_type)NULL)->field))
#define XtOffsetOf(s_type, field)  XtOffset(s_type*, field)
#endif

#define NUM_OF_HEX_ELEMENTS (16)	/* number of hex numbers per line for data dump */
#define DEVICE_NAME_LEN	(16)	/* length of device name in extended status */

static EpsonCmdRec epson_cmd[] = {

/*
 *              request identity
 *              |   request identity2
 *              |    |    request status
 *              |    |    |    request command parameter
 *              |    |    |    |    set color mode
 *              |    |    |    |    |    start scanning
 *              |    |    |    |    |    |    set data format
 *              |    |    |    |    |    |    |    set resolution
 *              |    |    |    |    |    |    |    |    set zoom
 *              |    |    |    |    |    |    |    |    |    set scan area
 *              |    |    |    |    |    |    |    |    |    |    set brightness
 *              |    |    |    |    |    |    |    |    |    |    |                set gamma
 *              |    |    |    |    |    |    |    |    |    |    |                |    set halftoning
 *              |    |    |    |    |    |    |    |    |    |    |                |    |    set color correction
 *              |    |    |    |    |    |    |    |    |    |    |                |    |    |    initialize scanner
 *              |    |    |    |    |    |    |    |    |    |    |                |    |    |    |    set speed
 *              |    |    |    |    |    |    |    |    |    |    |                |    |    |    |    |    set lcount
 *              |    |    |    |    |    |    |    |    |    |    |                |    |    |    |    |    |    mirror image
 *              |    |    |    |    |    |    |    |    |    |    |                |    |    |    |    |    |    |    set gamma table
 *              |    |    |    |    |    |    |    |    |    |    |                |    |    |    |    |    |    |    |    set outline emphasis
 *              |    |    |    |    |    |    |    |    |    |    |                |    |    |    |    |    |    |    |    |    set dither
 *              |    |    |    |    |    |    |    |    |    |    |                |    |    |    |    |    |    |    |    |    |    set color correction coefficients
 *              |    |    |    |    |    |    |    |    |    |    |                |    |    |    |    |    |    |    |    |    |    |    request extended status
 *              |    |    |    |    |    |    |    |    |    |    |                |    |    |    |    |    |    |    |    |    |    |    |    control an extension
 *              |    |    |    |    |    |    |    |    |    |    |                |    |    |    |    |    |    |    |    |    |    |    |    |    forward feed / eject
 *              |    |    |    |    |    |    |    |    |    |    |                |    |    |    |    |    |    |    |    |    |    |    |    |    |     feed
 *              |    |    |    |    |    |    |    |    |    |    |                |    |    |    |    |    |    |    |    |    |    |    |    |    |     |     request push button status
 *              |    |    |    |    |    |    |    |    |    |    |                |    |    |    |    |    |    |    |    |    |    |    |    |    |     |     |    control auto area segmentation
 *              |    |    |    |    |    |    |    |    |    |    |                |    |    |    |    |    |    |    |    |    |    |    |    |    |     |     |    |    set film type
 *              |    |    |    |    |    |    |    |    |    |    |                |    |    |    |    |    |    |    |    |    |    |    |    |    |     |     |    |    |    set exposure time
 *              |    |    |    |    |    |    |    |    |    |    |                |    |    |    |    |    |    |    |    |    |    |    |    |    |     |     |    |    |    |    set bay
 *              |    |    |    |    |    |    |    |    |    |    |                |    |    |    |    |    |    |    |    |    |    |    |    |    |     |     |    |    |    |    |    set threshold
 *              |    |    |    |    |    |    |    |    |    |    |                |    |    |    |    |    |    |    |    |    |    |    |    |    |     |     |    |    |    |    |    |    set focus position
 *              |    |    |    |    |    |    |    |    |    |    |                |    |    |    |    |    |    |    |    |    |    |    |    |    |     |     |    |    |    |    |    |    |    request focus position
 *              |    |    |    |    |    |    |    |    |    |    |                |    |    |    |    |    |    |    |    |    |    |    |    |    |     |     |    |    |    |    |    |    |    |    request extended identity
 *              |    |    |    |    |    |    |    |    |    |    |                |    |    |    |    |    |    |    |    |    |    |    |    |    |     |     |    |    |    |    |    |    |    |    |    request scanner status
 *              |    |    |    |    |    |    |    |    |    |    |                |    |    |    |    |    |    |    |    |    |    |    |    |    |     |     |    |    |    |    |    |    |    |    |    |
 *              |    |    |    |    |    |    |    |    |    |    |                |    |    |    |    |    |    |    |    |    |    |    |    |    |     |     |    |    |    |    |    |    |    |    |    |
 *              |    |    |    |    |    |    |    |    |    |    |                |    |    |    |    |    |    |    |    |    |    |    |    |    |     |     |    |    |    |    |    |    |    |    |    |
 */ {"A1", 'I', 0x0, 'F', 'S', 0x0, 'G', 0x0, 'R', 0x0, 'A', 0x0,
	     {-0, 0, 0}, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
	     0x0, 0x0, 0x0, 0x0, 0x00, 0x00, 0x0, 0x0, 0x0, 0x0, 0x0,
	     0x0, 0x0, 0x0, 0x0, 0x0},
	{"A2", 'I', 0x0, 'F', 'S', 0x0, 'G', 'D', 'R', 'H', 'A', 'L',
	 {-3, 3, 0}, 'Z', 'B', 0x0, '@', 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
	 0x0, 0x0, 0x00, 0x00, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
	 0x0},
	{"B1", 'I', 0x0, 'F', 'S', 'C', 'G', 'D', 'R', 0x0, 'A', 0x0,
	 {-0, 0, 0}, 0x0, 'B', 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
	 0x0, 0x0, 0x00, 0x00, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
	 0x0},
	{"B2", 'I', 0x0, 'F', 'S', 'C', 'G', 'D', 'R', 'H', 'A', 'L',
	 {-3, 3, 0}, 'Z', 'B', 0x0, '@', 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
	 0x0, 0x0, 0x00, 0x00, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
	 0x0},
	{"B3", 'I', 0x0, 'F', 'S', 'C', 'G', 'D', 'R', 'H', 'A', 'L',
	 {-3, 3, 0}, 'Z', 'B', 'M', '@', 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 'm',
	 'f', 'e', 0x00, 0x00, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
	 0x0},
	{"B4", 'I', 0x0, 'F', 'S', 'C', 'G', 'D', 'R', 'H', 'A', 'L',
	 {-3, 3, 0}, 'Z', 'B', 'M', '@', 'g', 'd', 0x0, 'z', 'Q', 'b', 'm',
	 'f', 'e', 0x00, 0x00, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
	 0x0},
	{"B5", 'I', 0x0, 'F', 'S', 'C', 'G', 'D', 'R', 'H', 'A', 'L',
	 {-3, 3, 0}, 'Z', 'B', 'M', '@', 'g', 'd', 'K', 'z', 'Q', 'b', 'm',
	 'f', 'e', 0x00, 0x00, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
	 0x0},
	{"B6", 'I', 0x0, 'F', 'S', 'C', 'G', 'D', 'R', 'H', 'A', 'L',
	 {-3, 3, 0}, 'Z', 'B', 'M', '@', 'g', 'd', 'K', 'z', 'Q', 'b', 'm',
	 'f', 'e', 0x00, 0x00, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
	 0x0},
	{"B7", 'I', 0x0, 'F', 'S', 'C', 'G', 'D', 'R', 'H', 'A', 'L',
	 {-4, 3, 0}, 'Z', 'B', 'M', '@', 'g', 'd', 'K', 'z', 'Q', 'b', 'm',
	 'f', 'e', '\f', 0x00, '!', 's', 'N', 0x0, 0x0, 't', 0x0, 0x0, 'I',
	 'F'},
	{"B8", 'I', 0x0, 'F', 'S', 'C', 'G', 'D', 'R', 'H', 'A', 'L',
	 {-4, 3, 0}, 'Z', 'B', 'M', '@', 'g', 'd', 'K', 'z', 'Q', 'b', 'm',
	 'f', 'e', '\f', 0x19, '!', 's', 'N', 0x0, 0x0, 0x0, 'p', 'q', 0x0,
	 0x0},
	{"F5", 'I', 0x0, 'F', 'S', 'C', 'G', 'D', 'R', 'H', 'A', 'L',
	 {-3, 3, 0}, 'Z', 0x0, 'M', '@', 'g', 'd', 'K', 'z', 'Q', 0x0, 'm',
	 0x0, 'e', '\f', 0x00, 0x0, 0x0, 'N', 'T', 'P', 0x0, 0x0, 0x0, 0x0,
	 0x0},
	{"D1", 'I', 'i', 'F', 0x0, 'C', 'G', 'D', 'R', 0x0, 'A', 0x0,
	 {-0, 0, 0}, 'Z', 0x0, 0x0, '@', 'g', 'd', 0x0, 'z', 0x0, 0x0, 0x0,
	 'f', 0x0, 0x00, 0x00, '!', 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
	 0x0},
	{"D7", 'I', 'i', 'F', 0x0, 'C', 'G', 'D', 'R', 0x0, 'A', 0x0,
	 {-0, 0, 0}, 'Z', 0x0, 0x0, '@', 'g', 'd', 0x0, 'z', 0x0, 0x0, 0x0,
	 'f', 0x0, 0x00, 0x00, '!', 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
	 0x0},
	{"D8", 'I', 'i', 'F', 0x0, 'C', 'G', 'D', 'R', 0x0, 'A', 0x0,
	 {-0, 0, 0}, 'Z', 0x0, 0x0, '@', 'g', 'd', 0x0, 'z', 0x0, 0x0, 0x0,
	 'f', 'e', 0x00, 0x00, '!', 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
	 0x0},
};

/*
 * Definition of the mode_param struct, that is used to
 * specify the valid parameters for the different scan modes.
 *
 * The depth variable gets updated when the bit depth is modified.
 */

struct mode_param
{
	int color;
	int flags;
	int dropout_mask;
	int depth;
};

static struct mode_param mode_params[] = {
	{0, 0x00, 0x30, 1},
	{0, 0x00, 0x30, 8},
	{1, 0x02, 0x00, 8}
};

static const SANE_String_Const mode_list[] = {
	SANE_I18N("Binary"),
	SANE_I18N("Gray"),
	SANE_I18N("Color"),
	NULL
};

static const SANE_String_Const adf_mode_list[] = {
	SANE_I18N("Simplex"),
	SANE_I18N("Duplex"),
	NULL
};

/* Define the different scan sources */

#define FBF_STR	SANE_I18N("Flatbed")
#define TPU_STR	SANE_I18N("Transparency Unit")
#define ADF_STR	SANE_I18N("Automatic Document Feeder")

/*
 * source list need one dummy entry (save device settings is crashing).
 * NOTE: no const - this list gets created while exploring the capabilities
 * of the scanner.
 */

static SANE_String_Const source_list[] = {
	FBF_STR,
	NULL,
	NULL,
	NULL
};

/* some defines to make handling the TPU easier */
#define FILM_TYPE_NEGATIVE	(1L << 0)
#define FILM_TYPE_SLIDE		(1L << 1)

static const SANE_String_Const film_list[] = {
	SANE_I18N("Positive Film"),
	SANE_I18N("Negative Film"),
	SANE_I18N("Positive Slide"),
	SANE_I18N("Negative Slide"),
	NULL
};

static int film_params[] = {
	0,
	1,
	2,
	3
};

static const SANE_String_Const focus_list[] = {
	SANE_I18N("Focus on glass"),
	SANE_I18N("Focus 2.5mm above glass"),
	NULL
};

#define HALFTONE_NONE 0x01
#define HALFTONE_TET 0x03

static const int halftone_params[] = {
	HALFTONE_NONE,
	0x00,
	0x10,
	0x20,
	0x80,
	0x90,
	0xa0,
	0xb0,
	HALFTONE_TET,
	0xc0,
	0xd0
};

static const SANE_String_Const halftone_list[] = {
	SANE_I18N("None"),
	SANE_I18N("Halftone A (Hard Tone)"),
	SANE_I18N("Halftone B (Soft Tone)"),
	SANE_I18N("Halftone C (Net Screen)"),
	NULL
};

static const SANE_String_Const halftone_list_4[] = {
	SANE_I18N("None"),
	SANE_I18N("Halftone A (Hard Tone)"),
	SANE_I18N("Halftone B (Soft Tone)"),
	SANE_I18N("Halftone C (Net Screen)"),
	SANE_I18N("Dither A (4x4 Bayer)"),
	SANE_I18N("Dither B (4x4 Spiral)"),
	SANE_I18N("Dither C (4x4 Net Screen)"),
	SANE_I18N("Dither D (8x4 Net Screen)"),
	NULL
};

static const SANE_String_Const halftone_list_7[] = {
	SANE_I18N("None"),
	SANE_I18N("Halftone A (Hard Tone)"),
	SANE_I18N("Halftone B (Soft Tone)"),
	SANE_I18N("Halftone C (Net Screen)"),
	SANE_I18N("Dither A (4x4 Bayer)"),
	SANE_I18N("Dither B (4x4 Spiral)"),
	SANE_I18N("Dither C (4x4 Net Screen)"),
	SANE_I18N("Dither D (8x4 Net Screen)"),
	SANE_I18N("Text Enhanced Technology"),
	SANE_I18N("Download pattern A"),
	SANE_I18N("Download pattern B"),
	NULL
};

static const int dropout_params[] = {
	0x00,			/* none */
	0x10,			/* red */
	0x20,			/* green */
	0x30			/* blue */
};

static const SANE_String_Const dropout_list[] = {
	SANE_I18N("None"),
	SANE_I18N("Red"),
	SANE_I18N("Green"),
	SANE_I18N("Blue"),
	NULL
};

/*
 * Color correction:
 * One array for the actual parameters that get sent to the scanner (color_params[]),
 * one array for the strings that get displayed in the user interface (color_list[])
 * and one array to mark the user defined color correction (dolor_userdefined[]).
 */
static const int color_params[] = {
	0x00,
	0x01,
	0x10,
	0x20,
	0x40,
	0x80
};

static const SANE_Bool color_userdefined[] = {
	SANE_FALSE,
	SANE_TRUE,
	SANE_FALSE,
	SANE_FALSE,
	SANE_FALSE,
	SANE_FALSE
};

static const SANE_String_Const color_list[] = {
	SANE_I18N("No Correction"),
	SANE_I18N("User defined"),
	SANE_I18N("Impact-dot printers"),
	SANE_I18N("Thermal printers"),
	SANE_I18N("Ink-jet printers"),
	SANE_I18N("CRT monitors"),
	NULL
};

/*
 * Gamma correction:
 * The A and B level scanners work differently than the D level scanners, therefore
 * I define two different sets of arrays, plus one set of variables that get set to
 * the actally used params and list arrays at runtime.
 */
static int gamma_params_ab[] = {
	0x01,
	0x03,
	0x00,
	0x10,
	0x20
};

static const SANE_String_Const gamma_list_ab[] = {
	SANE_I18N("Default"),
	SANE_I18N("User defined"),
	SANE_I18N("High density printing"),
	SANE_I18N("Low density printing"),
	SANE_I18N("High contrast printing"),
	NULL
};

static SANE_Bool gamma_userdefined_ab[] = {
	SANE_FALSE,
	SANE_TRUE,
	SANE_FALSE,
	SANE_FALSE,
	SANE_FALSE,
};

static int gamma_params_d[] = {
	0x03,
	0x04
};

static const SANE_String_Const gamma_list_d[] = {
	SANE_I18N("User defined (Gamma=1.0)"),
	SANE_I18N("User defined (Gamma=1.8)"),
	NULL
};

static SANE_Bool gamma_userdefined_d[] = {
	SANE_TRUE,
	SANE_TRUE
};

static SANE_Bool *gamma_userdefined;
static int *gamma_params;

/* flaming hack to get USB scanners
 * working without timeouts under linux
 * (cribbed from fujitsu.c)
 */
static unsigned int r_cmd_count = 0;
static unsigned int w_cmd_count = 0;

/* Bay list:
 * this is used for the FilmScan
 * XXX Add APS loader support
 */

static const SANE_String_Const bay_list[] = {
	" 1 ",
	" 2 ",
	" 3 ",
	" 4 ",
	" 5 ",
	" 6 ",
	NULL
};

/* minimum, maximum, quantization */
static const SANE_Range u8_range = { 0, 255, 0 };
static const SANE_Range s8_range = { -127, 127, 0 };
static const SANE_Range zoom_range = { 50, 200, 0 };

/* used for several boolean choices */
static int switch_params[] = {
	0,
	1
};

#define mirror_params switch_params
#define speed_params switch_params

static const SANE_Range outline_emphasis_range = { -2, 2, 0 };

struct qf_param
{
	SANE_Word tl_x;
	SANE_Word tl_y;
	SANE_Word br_x;
	SANE_Word br_y;
};

static struct qf_param qf_params[] = {
	{0, 0, SANE_FIX(120.0), SANE_FIX(120.0)},
	{0, 0, SANE_FIX(148.5), SANE_FIX(210.0)},
	{0, 0, SANE_FIX(210.0), SANE_FIX(148.5)},
	{0, 0, SANE_FIX(215.9), SANE_FIX(279.4)},	/* 8.5" x 11" */
	{0, 0, SANE_FIX(210.0), SANE_FIX(297.0)},
	{0, 0, 0, 0}
};

static const SANE_String_Const qf_list[] = {
	SANE_I18N("CD"),
	SANE_I18N("A5 portrait"),
	SANE_I18N("A5 landscape"),
	SANE_I18N("Letter"),
	SANE_I18N("A4"),
	SANE_I18N("Max"),
	NULL
};

static SANE_Word *bitDepthList = NULL;



/*
 * List of pointers to devices - will be dynamically allocated depending
 * on the number of devices found.
 */
static const SANE_Device **devlist = 0;


/* Some utility functions */

static size_t
max_string_size(const SANE_String_Const strings[])
{
	size_t size, max_size = 0;
	int i;

	for (i = 0; strings[i]; i++) {
		size = strlen(strings[i]) + 1;
		if (size > max_size)
			max_size = size;
	}
	return max_size;
}

typedef struct
{
	unsigned char code;
	unsigned char status;
	unsigned char count1;
	unsigned char count2;
	unsigned char buf[1];

} EpsonHdrRec, *EpsonHdr;

typedef struct
{
	unsigned char code;
	unsigned char status;
	u_short count;

	unsigned char buf[1];

} EpsonParameterRec, *EpsonParameter;

typedef struct
{
	unsigned char code;
	unsigned char status;

	unsigned char buf[4];

} EpsonDataRec, *EpsonData;

static SANE_Status color_shuffle(SANE_Handle handle, int *new_length);
static void activateOption(Epson_Scanner * s, SANE_Int option,
			   SANE_Bool * change);
static void deactivateOption(Epson_Scanner * s, SANE_Int option,
			     SANE_Bool * change);
static void setOptionState(Epson_Scanner * s, SANE_Bool state,
			   SANE_Int option, SANE_Bool * change);
static void close_scanner(Epson_Scanner * s);
static SANE_Status open_scanner(Epson_Scanner * s);
static SANE_Status attach_one_usb(SANE_String_Const devname);
static SANE_Status attach_one_net(SANE_String_Const devname);
static void filter_resolution_list(Epson_Scanner * s);
static void epson2_scan_finish(Epson_Scanner * s);
SANE_Status sane_auto_eject(Epson_Scanner * s);


static int
epson_send(Epson_Scanner * s, void *buf, size_t buf_size, size_t reply_len,
	   SANE_Status * status)
{
	DBG(15, "%s: size = %lu, reply = %lu\n",
	    __func__, (u_long) buf_size, (u_long) reply_len);

	if (buf_size == 2) {
		char *cmd = buf;

		switch (cmd[0]) {
		case ESC:
			DBG(9, "%s: ESC %c\n", __func__, cmd[1]);
			break;

		case FS:
			DBG(9, "%s: FS %c\n", __func__, cmd[1]);
			break;
		}
	}

	if (DBG_LEVEL >= 125) {
		unsigned int k;
		const unsigned char *s = buf;

		for (k = 0; k < buf_size; k++) {
			DBG(125, "buf[%d] %02x %c\n", k, s[k],
			    isprint(s[k]) ? s[k] : '.');
		}
	}

	if (s->hw->connection == SANE_EPSON_NET) {
		if (reply_len == 0) {
			DBG(0,
			    "Cannot send this command to a networked scanner\n");
			return SANE_STATUS_INVAL;
		}
		return sanei_epson_net_write(s, 0x2000, buf, buf_size,
					     reply_len, status);
	} else if (s->hw->connection == SANE_EPSON_SCSI) {
		return sanei_epson2_scsi_write(s->fd, buf, buf_size, status);
	} else if (s->hw->connection == SANE_EPSON_PIO) {
		size_t n;

		if (buf_size == (n = sanei_pio_write(s->fd, buf, buf_size)))
			*status = SANE_STATUS_GOOD;
		else
			*status = SANE_STATUS_INVAL;

		return n;

	} else if (s->hw->connection == SANE_EPSON_USB) {
		size_t n;
		n = buf_size;
		*status = sanei_usb_write_bulk(s->fd, buf, &n);
		w_cmd_count++;
		DBG(20, "%s: cmd count, r = %d, w = %d\n",
		    __func__, r_cmd_count, w_cmd_count);

		return n;
	}

	*status = SANE_STATUS_INVAL;
	return 0;
	/* never reached */
}

static ssize_t
epson_recv(Epson_Scanner * s, void *buf, ssize_t buf_size,
	   SANE_Status * status)
{
	ssize_t n = 0;

	DBG(15, "%s: size = %d, buf = %p\n", __func__, buf_size, buf);

	if (s->hw->connection == SANE_EPSON_NET) {
		n = sanei_epson_net_read(s, buf, buf_size, status);
	} else if (s->hw->connection == SANE_EPSON_SCSI) {
		n = sanei_epson2_scsi_read(s->fd, buf, buf_size, status);
	} else if (s->hw->connection == SANE_EPSON_PIO) {
		if (buf_size ==
		    (n = sanei_pio_read(s->fd, buf, (size_t) buf_size)))
			*status = SANE_STATUS_GOOD;
		else
			*status = SANE_STATUS_INVAL;
	} else if (s->hw->connection == SANE_EPSON_USB) {
		/* !!! only report an error if we don't read anything */
		n = buf_size;	/* buf_size gets overwritten */
		*status =
			sanei_usb_read_bulk(s->fd, (SANE_Byte *) buf,
					    (size_t *) & n);
		r_cmd_count += (n + 63) / 64;	/* add # of packets, rounding up */
		DBG(20, "%s: cmd count, r = %d, w = %d\n",
		    __func__, r_cmd_count, w_cmd_count);

		if (n > 0)
			*status = SANE_STATUS_GOOD;
	}

	if (n < buf_size)
		DBG(1, "%s: expected = %lu, got = %ld\n", __func__,
		    (u_long) buf_size, (long) n);

	/* dump buffer if appropriate */
	if (DBG_LEVEL >= 127 && n > 0) {
		int k;
		const unsigned char *s = buf;

		for (k = 0; k < n; k++)
			DBG(127, "buf[%d] %02x %c\n", k, s[k],
			    isprint(s[k]) ? s[k] : '.');
	}

	return n;
}

/* Simple function to exchange a fixed amount of
 * data with the scanner
 */

static SANE_Status
epson2_txrx(Epson_Scanner *s, unsigned char *txbuf, size_t txlen,
	unsigned char *rxbuf, size_t rxlen)
{
	SANE_Status status;

	epson_send(s, txbuf, txlen, rxlen, &status);
	if (status != SANE_STATUS_GOOD) {
		DBG(1, "%s: tx err, %s\n", __func__, sane_strstatus(status));
                return status;
	}

	epson_recv(s, rxbuf, rxlen, &status);
	if (status != SANE_STATUS_GOOD) {
		DBG(1, "%s: rx err, %s\n", __func__, sane_strstatus(status));
	}

	return status;
}

/* This function should be used to send codes that only requires the scanner
 * to give back an ACK or a NAK.
 */
static SANE_Status
epson2_cmd_simple(Epson_Scanner * s, void *buf, size_t buf_size)
{
	unsigned char result;
	SANE_Status status;

	DBG(12, "%s: size = %d\n", __func__, buf_size);

	status = epson2_txrx(s, buf, buf_size, &result, 1);
	if (status != SANE_STATUS_GOOD) {
		DBG(1, "%s: failed, %s\n", __func__, sane_strstatus(status));
		return status;
	}

	if (result == ACK)
		return SANE_STATUS_GOOD;

	if (result == NAK) {
		DBG(3, "%s: NAK\n", __func__);
		return SANE_STATUS_INVAL;
	}

	DBG(1, "%s: result is neither ACK nor NAK but 0x%02x\n", __func__,
	    result);

	return SANE_STATUS_GOOD;
}

/* receives a 4 or 6 bytes information block from the scanner*/
static SANE_Status
epson2_recv_info_block(Epson_Scanner * s, unsigned char *scanner_status,
		      size_t info_size, size_t * payload_size)
{
	SANE_Status status;
	unsigned char info[6];

	if (s->hw->connection == SANE_EPSON_PIO)
		epson_recv(s, info, 1, &status);
	else
		epson_recv(s, info, info_size, &status);

	if (status != SANE_STATUS_GOOD)
		return status;

	/* check for explicit NAK */
	if (info[0] == NAK) {
		DBG(1, "%s: command not supported\n", __func__);
		return SANE_STATUS_UNSUPPORTED;
	}

	/* check the first byte: if it's not STX, bail out */
	if (info[0] != STX) {
		DBG(1, "%s: expecting STX, got %02X\n", __func__,
		    info[0]);
		return SANE_STATUS_INVAL;
	}

	/* if connection is PIO read the remaining bytes. */
	if (s->hw->connection == SANE_EPSON_PIO) {
		epson_recv(s, &info[1], info_size - 1, &status);
		if (status != SANE_STATUS_GOOD)
			return status;
	}

	if (scanner_status)
		*scanner_status = info[1];

	if (payload_size) {
		*payload_size = le16atoh(&info[2]);

		if (info_size == 6)
			*payload_size *= le16atoh(&info[4]);

		DBG(14, "%s: payload length: %d\n", __func__, *payload_size);
	}

	return SANE_STATUS_GOOD;
}

/* This function can be called for commands that
 * will be answered by the scanner with an info block of 4 bytes
 * and a variable payload. The payload is passed back to the caller
 * in **buf. The caller must free it if != NULL,
 * even if the status != SANE_STATUS_GOOD.
 */

static SANE_Status
epson2_cmd_info_block(SANE_Handle handle, unsigned char *params,
	unsigned char params_len, size_t reply_len,
	unsigned char **buf, size_t *buf_len)
{
	SANE_Status status;
	Epson_Scanner *s = (Epson_Scanner *) handle;
	size_t len;

	DBG(8, "%s, params len = %d, reply len = %d, buf = %p\n",
		__func__, params_len, reply_len, (void *) buf);

	if (buf == NULL)
		return SANE_STATUS_INVAL;

	/* initialize */
	*buf = NULL;

	/* send command, we expect the info block + reply_len back */
	epson_send(s, params, params_len,
		reply_len ? reply_len + 4 : 0, &status);

	if (status != SANE_STATUS_GOOD)
		goto end;

	status = epson2_recv_info_block(s, NULL, 4, &len);
	if (status != SANE_STATUS_GOOD)
		goto end;

	/* do we need to provide the length of the payload? */
	if (buf_len)
		*buf_len = len;

	/* no payload, stop here */
	if (len == 0)
		goto end;

	/* if a reply_len has been specified and the actual
	 * length differs, throw a warning
	 */
	if (reply_len && (len != reply_len)) {
		DBG(1, "%s: mismatched len - expected %d, got %d\n",
			__func__, reply_len, len);
	}

	/* allocate and receive the payload */
	*buf = malloc(len);

	if (*buf) {
		memset(*buf, 0x00, len);
		epson_recv(s, *buf, len, &status);	/* receive actual data */
	}
	else
		status = SANE_STATUS_NO_MEM;
end:

	if (status != SANE_STATUS_GOOD) {
		DBG(1, "%s: failed, %s\n", __func__, sane_strstatus(status));

		if (*buf) {
			free(*buf);
			*buf = NULL;
		}
	}

	return status;
}	


/* This is used for ESC commands with a single byte parameter. Scanner
 * will answer with ACK/NAK.
 */
static SANE_Status
epson2_esc_cmd(Epson_Scanner * s, unsigned char cmd, unsigned char val)
{
	SANE_Status status;
	unsigned char params[2];

	DBG(8, "%s: cmd = 0x%02x, val = %d\n", __func__, cmd, val);
	if (!cmd)
		return SANE_STATUS_UNSUPPORTED;

	params[0] = ESC;
	params[1] = cmd;

	status = epson2_cmd_simple(s, params, 2);
	if (status != SANE_STATUS_GOOD)
		return status;

	params[0] = val;

	return epson2_cmd_simple(s, params, 1);
}

/* Send an ACK to the scanner */

static SANE_Status
epson2_ack(Epson_Scanner *s)
{
	SANE_Status status;
	epson_send(s, S_ACK, 1, 0, &status);
	return status;
}



/* A little helper function to correct the extended status reply
 * gotten from scanners with known buggy firmware.
 */
static void
fix_up_extended_status_reply(const char *model, unsigned char *buf)
{
	if (strncmp(model, "ES-9000H", strlen("ES-9000H")) == 0
	    || strncmp(model, "GT-30000", strlen("GT-30000")) == 0) {
		DBG(1, "fixing up buggy ADF max scan dimensions.\n");
		buf[2] = 0xB0;
		buf[3] = 0x6D;
		buf[4] = 0x60;
		buf[5] = 0x9F;
	}
}

static void
print_params(const SANE_Parameters params)
{
	DBG(5, "params.format = %d\n", params.format);
	DBG(5, "params.last_frame = %d\n", params.last_frame);
	DBG(5, "params.bytes_per_line = %d\n", params.bytes_per_line);
	DBG(5, "params.pixels_per_line = %d\n", params.pixels_per_line);
	DBG(5, "params.lines = %d\n", params.lines);
	DBG(5, "params.depth = %d\n", params.depth);
}

static void
epson2_set_cmd_level(SANE_Handle handle, unsigned char *level)
{
	Epson_Scanner *s = (Epson_Scanner *) handle;
	Epson_Device *dev = s->hw;

	int n;

	DBG(1, "%s: %c%c\n", __func__, level[0], level[1]);

	/* set command type and level */
	for (n = 0; n < NELEMS(epson_cmd); n++) {
		char type_level[3];
		sprintf(type_level, "%c%c", level[0], level[1]);
		if (!strncmp(type_level, epson_cmd[n].level, 2))
			break;
	}

	if (n < NELEMS(epson_cmd)) {
		dev->cmd = &epson_cmd[n];
	} else {
		dev->cmd = &epson_cmd[EPSON_LEVEL_DEFAULT];
		DBG(1, " unknown type %c or level %c, using %s\n",
		    level[0], level[1], dev->cmd->level);
	}

	s->hw->level = dev->cmd->level[1] - '0';
}


/* simple scanner commands, ESC v */

#define set_focus_position(s,v)		epson2_esc_cmd( s,(s)->hw->cmd->set_focus_position, v)
#define set_color_mode(s,v)		epson2_esc_cmd( s,(s)->hw->cmd->set_color_mode, v)
#define set_data_format(s,v)		epson2_esc_cmd( s,(s)->hw->cmd->set_data_format, v)
#define set_halftoning(s,v)		epson2_esc_cmd( s,(s)->hw->cmd->set_halftoning, v)
#define set_gamma(s,v)			epson2_esc_cmd( s,(s)->hw->cmd->set_gamma, v)
#define set_color_correction(s,v)	epson2_esc_cmd( s,(s)->hw->cmd->set_color_correction, v)
#define set_lcount(s,v)			epson2_esc_cmd( s,(s)->hw->cmd->set_lcount, v)
#define set_bright(s,v)			epson2_esc_cmd( s,(s)->hw->cmd->set_bright, v)
#define mirror_image(s,v)		epson2_esc_cmd( s,(s)->hw->cmd->mirror_image, v)
#define set_speed(s,v)			epson2_esc_cmd( s,(s)->hw->cmd->set_speed, v)
#define set_sharpness(s,v)		epson2_esc_cmd( s,(s)->hw->cmd->set_outline_emphasis, v)
#define set_auto_area_segmentation(s,v)	epson2_esc_cmd( s,(s)->hw->cmd->control_auto_area_segmentation, v)
#define set_film_type(s,v)		epson2_esc_cmd( s,(s)->hw->cmd->set_film_type, v)
#define set_exposure_time(s,v)		epson2_esc_cmd( s,(s)->hw->cmd->set_exposure_time, v)
#define set_bay(s,v)			epson2_esc_cmd( s,(s)->hw->cmd->set_bay, v)
#define set_threshold(s,v)		epson2_esc_cmd( s,(s)->hw->cmd->set_threshold, v)
#define control_extension(s,v)		epson2_esc_cmd( s,(s)->hw->cmd->control_an_extension, v)

static SANE_Status
set_zoom(Epson_Scanner * s, unsigned char x, unsigned char y)
{
	SANE_Status status;
	unsigned char params[2];

	DBG(8, "%s: x = %d, y = %d\n", __func__, x, y);

	if (!s->hw->cmd->set_zoom) {
		DBG(1, "%s: not supported\n", __func__);
		return SANE_STATUS_GOOD;
	}

	params[0] = ESC;
	params[1] = s->hw->cmd->set_zoom;

	status = epson2_cmd_simple(s, params, 2);
	if (status != SANE_STATUS_GOOD)
		return status;

	params[0] = x;
	params[1] = y;

	return epson2_cmd_simple(s, params, 2);
}

/* ESC R */
static SANE_Status
set_resolution(Epson_Scanner * s, int x, int y)
{
	SANE_Status status;
	unsigned char params[4];

	DBG(8, "%s: x = %d, y = %d\n", __func__, x, y);

	if (!s->hw->cmd->set_resolution) {
		DBG(1, "%s: not supported\n", __func__);
		return SANE_STATUS_GOOD;
	}

	params[0] = ESC;
	params[1] = s->hw->cmd->set_resolution;

	status = epson2_cmd_simple(s, params, 2);
	if (status != SANE_STATUS_GOOD)
		return status;

	params[0] = x;
	params[1] = x >> 8;
	params[2] = y;
	params[3] = y >> 8;

	return epson2_cmd_simple(s, params, 4);
}

/*
 * Sends the "set scan area" command to the scanner with the currently selected
 * scan area. This scan area is already corrected for "color shuffling" if
 * necessary.
 */

static SANE_Status
set_scan_area(Epson_Scanner * s, int x, int y, int width, int height)
{
	SANE_Status status;
	unsigned char params[8];

	DBG(8, "%s: x = %d, y = %d, w = %d, h = %d\n",
	    __func__, x, y, width, height);

	if (!s->hw->cmd->set_scan_area) {
		DBG(1, "%s: not supported\n", __func__);
		return SANE_STATUS_UNSUPPORTED;
	}

	/* verify the scan area */
	if (x < 0 || y < 0 || width <= 0 || height <= 0)
		return SANE_STATUS_INVAL;

	params[0] = ESC;
	params[1] = s->hw->cmd->set_scan_area;

	status = epson2_cmd_simple(s, params, 2);
	if (status != SANE_STATUS_GOOD)
		return status;

	params[0] = x;
	params[1] = x >> 8;
	params[2] = y;
	params[3] = y >> 8;
	params[4] = width;
	params[5] = width >> 8;
	params[6] = height;
	params[7] = height >> 8;

	return epson2_cmd_simple(s, params, 8);
}

/*
 * Sends the "set color correction coefficients" command with the
 * currently selected parameters to the scanner.
 */

static SANE_Status
set_color_correction_coefficients(Epson_Scanner * s)
{
	SANE_Status status;
	unsigned char params[2];
	signed char cct[9];

	DBG(8, "%s\n", __func__);
	if (!s->hw->cmd->set_color_correction_coefficients) {
		DBG(1, "%s: not supported\n", __func__);
		return SANE_STATUS_UNSUPPORTED;
	}

	params[0] = ESC;
	params[1] = s->hw->cmd->set_color_correction_coefficients;

	status = epson2_cmd_simple(s, params, 2);
	if (status != SANE_STATUS_GOOD)
		return status;

	cct[0] = s->val[OPT_CCT_1].w;
	cct[1] = s->val[OPT_CCT_2].w;
	cct[2] = s->val[OPT_CCT_3].w;
	cct[3] = s->val[OPT_CCT_4].w;
	cct[4] = s->val[OPT_CCT_5].w;
	cct[5] = s->val[OPT_CCT_6].w;
	cct[6] = s->val[OPT_CCT_7].w;
	cct[7] = s->val[OPT_CCT_8].w;
	cct[8] = s->val[OPT_CCT_9].w;

	DBG(10, "%s: %d,%d,%d %d,%d,%d %d,%d,%d\n", __func__,
	    cct[0], cct[1], cct[2], cct[3],
	    cct[4], cct[5], cct[6], cct[7], cct[8]);

	return epson2_cmd_simple(s, params, 9);
}

static SANE_Status
set_gamma_table(Epson_Scanner * s)
{
	SANE_Status status;
	unsigned char params[2];
	unsigned char gamma[257];
	int n;
	int table;

/*	static const char gamma_cmds[] = { 'M', 'R', 'G', 'B' }; */
	static const char gamma_cmds[] = { 'R', 'G', 'B' };

	DBG(8, "%s\n", __func__);
	if (!s->hw->cmd->set_gamma_table)
		return SANE_STATUS_UNSUPPORTED;

	params[0] = ESC;
	params[1] = s->hw->cmd->set_gamma_table;

	/* Print the gamma tables before sending them to the scanner */

	if (DBG_LEVEL >= 10) {
		int c, i, j;

		for (c = 0; c < 3; c++) {
			for (i = 0; i < 256; i += 16) {
				char gammaValues[16 * 3 + 1], newValue[4];

				gammaValues[0] = '\0';

				for (j = 0; j < 16; j++) {
					sprintf(newValue, " %02x",
						s->gamma_table[c][i + j]);
					strcat(gammaValues, newValue);
				}
				DBG(10, "gamma table[%d][%d] %s\n", c, i,
				    gammaValues);
			}
		}
	}

	/*
	 * When handling inverted images, we must also invert the user
	 * supplied gamma function. This is *not* just 255-gamma -
	 * this gives a negative image.
	 */

	for (table = 0; table < 3; table++) {
		gamma[0] = gamma_cmds[table];

		if (s->invert_image) {
			for (n = 0; n < 256; ++n) {
				gamma[n + 1] =
					255 - s->gamma_table[table][255 - n];
			}
		} else {
			for (n = 0; n < 256; ++n) {
				gamma[n + 1] = s->gamma_table[table][n];
			}
		}

		status = epson2_cmd_simple(s, params, 2);
		if (status != SANE_STATUS_GOOD)
			return status;

		status = epson2_cmd_simple(s, gamma, 257);
		if (status != SANE_STATUS_GOOD)
			return status;
	}

	return status;
}


/* ESC F - Request Status
 * -> ESC f
 * <- Information block
 */

static SANE_Status
request_status(SANE_Handle handle, unsigned char *scanner_status)
{
	Epson_Scanner *s = (Epson_Scanner *) handle;
	SANE_Status status;
	unsigned char params[2];

	DBG(8, "%s\n", __func__);

	if (s->hw->cmd->request_status == 0)
		return SANE_STATUS_UNSUPPORTED;

	params[0] = ESC;
	params[1] = s->hw->cmd->request_status;

	epson_send(s, params, 2, 4, &status);
	if (status != SANE_STATUS_GOOD)
		return status;

	status = epson2_recv_info_block(s, params, 4, NULL);
	if (status != SANE_STATUS_GOOD)
		return status;

	if (scanner_status)
		*scanner_status = params[0];

	DBG(1, "status:\n");

	if (params[0] & STATUS_NOT_READY)
		DBG(1, " scanner in use on another interface\n");
	else
		DBG(1, " ready\n");

	if (params[0] & STATUS_FER)
		DBG(1, " system error\n");

	if (params[0] & STATUS_OPTION)
		DBG(1, " option equipment is installed\n");
	else
		DBG(1, " no option equipment installed\n");

	if (params[0] & STATUS_EXT_COMMANDS)
		DBG(1, " support extended commands\n");
	else
		DBG(1, " does NOT support extended commands\n");

	if (params[0] & STATUS_RESERVED)
		DBG(0,
		    "a reserved bit is set, please contact <a.zummo@towertech.it>.\n");

	return status;
}

/* extended commands */

/* FS I, Request Extended Identity
 * -> FS I
 * <- Extended identity data (80)
 *
 * Request the properties of the scanner.
 */

static SANE_Status
request_extended_identity(SANE_Handle handle, unsigned char *buf)
{
	unsigned char model[17];
	Epson_Scanner *s = (Epson_Scanner *) handle;
	SANE_Status status;
	unsigned char params[2];

	DBG(8, "%s\n", __func__);

	if (buf == NULL)
		return SANE_STATUS_INVAL;

	if (s->hw->cmd->request_extended_identity == 0)
		return SANE_STATUS_UNSUPPORTED;

	params[0] = FS;
	params[1] = s->hw->cmd->request_extended_identity;

	status = epson2_txrx(s, params, 2, buf, 80);
	if (status != SANE_STATUS_GOOD)
		return status;

	DBG(1, " command level   : %c%c\n", buf[0], buf[1]);
	DBG(1, " basic resolution: %d\n", le32atoh(&buf[4]));
	DBG(1, " min resolution  : %d\n", le32atoh(&buf[8]));
	DBG(1, " max resolution  : %d\n", le32atoh(&buf[12]));
	DBG(1, " max pixel num   : %d\n", le32atoh(&buf[16]));
	DBG(1, " scan area       : %dx%d\n", le32atoh(&buf[20]),
	    le32atoh(&buf[24]));
	DBG(1, " adf area        : %dx%d\n", le32atoh(&buf[28]),
	    le32atoh(&buf[32]));
	DBG(1, " tpu area        : %dx%d\n", le32atoh(&buf[36]),
	    le32atoh(&buf[40]));

	DBG(1, " main status     : 0x%02x\n", buf[44]);
	DBG(1, " input depth     : %d\n", buf[66]);
	DBG(1, " max output depth: %d\n", buf[67]);
	DBG(1, " rom version     : %c%c%c%c\n", buf[62], buf[63], buf[64],
	    buf[65]);

	memcpy(model, &buf[46], 16);
	model[16] = '\0';
	DBG(1, " model name      : %s\n", model);

	if (buf[44] & EXT_IDTY_STATUS_DLF)
		DBG(1, "  main lamp change is supported\n");

	if (buf[44] & EXT_IDTY_STATUS_TPIR)
		DBG(1, "  infrared scanning is supported\n");

	return SANE_STATUS_GOOD;
}

/* FS F, request scanner status */
static SANE_Status
request_scanner_status(SANE_Handle handle, unsigned char *buf)
{
	Epson_Scanner *s = (Epson_Scanner *) handle;
	SANE_Status status;
	unsigned char params[2];

	DBG(8, "%s\n", __func__);

	if (!s->hw->extended_commands)
		return SANE_STATUS_UNSUPPORTED;

	if (buf == NULL)
		return SANE_STATUS_INVAL;

	params[0] = FS;
	params[1] = 'F';

	status = epson2_txrx(s, params, 2, buf, 16);
	if (status != SANE_STATUS_GOOD)
		return status;

	DBG(1, "global status   : %02x\n", buf[0]);

	if (buf[0] & FSF_STATUS_MAIN_FER)
		DBG(1, " system error\n");

	if (buf[0] & FSF_STATUS_MAIN_NR)
		DBG(1, " not ready\n");

	if (buf[0] & FSF_STATUS_MAIN_WU)
		DBG(1, " warming up\n");


	DBG(1, "adf status      : %02x\n", buf[1]);

	if (buf[1] & FSF_STATUS_ADF_IST)
		DBG(1, " installed\n");
	else
		DBG(1, " not installed\n");

	if (buf[1] & FSF_STATUS_ADF_EN)
		DBG(1, " enabled\n");
	else
		DBG(1, " not enabled\n");

	if (buf[1] & FSF_STATUS_ADF_ERR)
		DBG(1, " error\n");

	if (buf[1] & FSF_STATUS_ADF_PE)
		DBG(1, " paper empty\n");

	if (buf[1] & FSF_STATUS_ADF_PJ)
		DBG(1, " paper jam\n");

	if (buf[1] & FSF_STATUS_ADF_OPN)
		DBG(1, " cover open\n");

	if (buf[1] & FSF_STATUS_ADF_PAG)
		DBG(1, " duplex capable\n");


	DBG(1, "tpu status      : %02x\n", buf[2]);

	if (buf[2] & FSF_STATUS_TPU_IST)
		DBG(1, " installed\n");
	else
		DBG(1, " not installed\n");

	if (buf[2] & FSF_STATUS_TPU_EN)
		DBG(1, " enabled\n");
	else
		DBG(1, " not enabled\n");

	if (buf[2] & FSF_STATUS_TPU_ERR)
		DBG(1, " error\n");

	if (buf[1] & FSF_STATUS_TPU_OPN)
		DBG(1, " cover open\n");


	DBG(1, "main body status: %02x\n", buf[3]);

	if (buf[3] & FSF_STATUS_MAIN2_PE)
		DBG(1, " paper empty\n");

	if (buf[3] & FSF_STATUS_MAIN2_PJ)
		DBG(1, " paper jam\n");

	if (buf[3] & FSF_STATUS_MAIN2_OPN)
		DBG(1, " cover open\n");

	return SANE_STATUS_GOOD;
}

static SANE_Status
set_scanning_parameter(SANE_Handle handle, unsigned char *buf)
{
	Epson_Scanner *s = (Epson_Scanner *) handle;
	SANE_Status status;
	unsigned char params[2];

	DBG(8, "%s\n", __func__);

	if (buf == NULL)
		return SANE_STATUS_INVAL;

	params[0] = FS;
	params[1] = 'W';

	DBG(9, "resolution of main scan     : %d\n", le32atoh(&buf[0]));
	DBG(9, "resolution of sub scan      : %d\n", le32atoh(&buf[4]));
	DBG(9, "offset length of main scan  : %d\n", le32atoh(&buf[8]));
	DBG(9, "offset length of sub scan   : %d\n", le32atoh(&buf[12]));
	DBG(9, "scanning length of main scan: %d\n", le32atoh(&buf[16]));
	DBG(9, "scanning length of sub scan : %d\n", le32atoh(&buf[20]));
	DBG(9, "scanning color              : %d\n", buf[24]);
	DBG(9, "data format                 : %d\n", buf[25]);
	DBG(9, "option control              : %d\n", buf[26]);
	DBG(9, "scanning mode               : %d\n", buf[27]);
	DBG(9, "block line number           : %d\n", buf[28]);
	DBG(9, "gamma correction            : %d\n", buf[29]);
	DBG(9, "brightness                  : %d\n", buf[30]);
	DBG(9, "color correction            : %d\n", buf[31]);
	DBG(9, "halftone processing         : %d\n", buf[32]);
	DBG(9, "threshold                   : %d\n", buf[33]);
	DBG(9, "auto area segmentation      : %d\n", buf[34]);
	DBG(9, "sharpness control           : %d\n", buf[35]);
	DBG(9, "mirroring                   : %d\n", buf[36]);
	DBG(9, "film type                   : %d\n", buf[37]);
	DBG(9, "main lamp lighting mode     : %d\n", buf[38]);

	status = epson2_cmd_simple(s, params, 2);
	if (status != SANE_STATUS_GOOD)
		return status;

	status = epson2_cmd_simple(s, buf, 64);
	if (status != SANE_STATUS_GOOD)
		return status;

	return SANE_STATUS_GOOD;
}

static SANE_Status
request_command_parameter(SANE_Handle handle, unsigned char *buf)
{
	Epson_Scanner *s = (Epson_Scanner *) handle;
	SANE_Status status;
	unsigned char params[2];

	DBG(8, "%s\n", __func__);

	if (s->hw->cmd->request_condition == 0)
		return SANE_STATUS_UNSUPPORTED;

	params[0] = ESC;
	params[1] = s->hw->cmd->request_condition;

	status = epson2_cmd_info_block(s, params, 2, 45, &buf, NULL);
	if (status != SANE_STATUS_GOOD)
		return status;

	DBG(1, "scanning parameters:\n");
	DBG(1, "color                  : %d\n", buf[1]);
	DBG(1, "resolution             : %dx%d\n", buf[4] << 8 | buf[3],
	    buf[6] << 8 | buf[5]);
	DBG(1, "halftone               : %d\n", buf[19]);
	DBG(1, "brightness             : %d\n", buf[21]);
	DBG(1, "color correction       : %d\n", buf[28]);
	DBG(1, "gamma                  : %d\n", buf[23]);
	DBG(1, "sharpness              : %d\n", buf[30]);
	DBG(1, "threshold              : %d\n", buf[38]);
	DBG(1, "data format            : %d\n", buf[17]);
	DBG(1, "mirroring              : %d\n", buf[34]);
	DBG(1, "option unit control    : %d\n", buf[42]);
	DBG(1, "film type              : %d\n", buf[44]);
	DBG(1, "auto area segmentation : %d\n", buf[36]);
	DBG(1, "line counter           : %d\n", buf[40]);
	DBG(1, "scanning mode          : %d\n", buf[32]);
	DBG(1, "zoom                   : %d,%d\n", buf[26], buf[25]);
	DBG(1, "scan area              : %d,%d %d,%d\n", buf[9] << 8 | buf[8],
	    buf[11] << 8 | buf[10], buf[13] << 8 | buf[12],
	    buf[15] << 8 | buf[14]);
	return status;
}

/* ESC q - Request Focus Position 
 * -> ESC q
 * <- Information block
 * <- Focus position status (2)
 *	0 - Error status
 *	1 - Focus position
 */

static SANE_Status
request_focus_position(SANE_Handle handle, unsigned char *position)
{
	SANE_Status status;
	unsigned char *buf;
	Epson_Scanner *s = (Epson_Scanner *) handle;

	unsigned char params[2];

	DBG(8, "%s\n", __func__);

	if (s->hw->cmd->request_focus_position == 0)
		return SANE_STATUS_UNSUPPORTED;

	params[0] = ESC;
	params[1] = s->hw->cmd->request_focus_position;

	status = epson2_cmd_info_block(s, params, 2, 2, &buf, NULL);
	if (status != SANE_STATUS_GOOD)
		return status;

	if (buf[0] & 0x01)
		DBG(1, "autofocus error\n");

	*position = buf[1];
	DBG(8, " focus position = 0x%x\n", buf[1]);

	free(buf);

	return status;
}

/* ESC ! - Request Push Button Status
 * -> ESC !
 * <- Information block
 * <- Push button status (1)
 */

static SANE_Status
request_push_button_status(SANE_Handle handle, unsigned char *bstatus)
{
	Epson_Scanner *s = (Epson_Scanner *) handle;
	SANE_Status status;
	unsigned char params[2];
	unsigned char *buf;

	DBG(8, "%s\n", __func__);

	if (s->hw->cmd->request_push_button_status == 0) {
		DBG(1, "push button status unsupported\n");
		return SANE_STATUS_UNSUPPORTED;
	}

	params[0] = ESC;
	params[1] = s->hw->cmd->request_push_button_status;

	status = epson2_cmd_info_block(s, params, 2, 1, &buf, NULL);
	if (status != SANE_STATUS_GOOD)
		return status;

	DBG(1, "push button status = %d\n", buf[0]);
	*bstatus = buf[0];

	free(buf);

	return status;
}


/*
 * Request Identity information from scanner and fill in information
 * into dev and/or scanner structures.
 * XXX information shoul dbe parsed separately.
 */
static SANE_Status
request_identity(SANE_Handle handle)
{
	Epson_Scanner *s = (Epson_Scanner *) handle;
	Epson_Device *dev = s->hw;
	unsigned char params[2];
	SANE_Status status;
	size_t len;
	unsigned int n;
	unsigned char *buf;
/*	unsigned char scanner_status; */

	DBG(1, "%s\n", __func__);

	if (!s->hw->cmd->request_identity)
		return SANE_STATUS_INVAL;

	params[0] = ESC;
	params[1] = s->hw->cmd->request_identity;

	status = epson2_cmd_info_block(s, params, 2, 0, &buf, &len);
	if (status != SANE_STATUS_GOOD)
		return status;

	epson2_set_cmd_level(s, &buf[0]);

	/* Setting available resolutions and xy ranges for sane frontend. */
	s->hw->res_list_size = 0;
	s->hw->res_list =
		(SANE_Int *) calloc(s->hw->res_list_size, sizeof(SANE_Int));

	if (s->hw->res_list) {
		unsigned char *area = buf + 2;
		unsigned int k = 0, x = 0, y = 0;

		/* cycle thru the resolutions, saving them in a list */
		for (n = 2; n < len; n += k, area += k) {
			switch (*area) {
			case 'R':
				{
					int val = area[2] << 8 | area[1];

					s->hw->res_list_size++;
					s->hw->res_list =
						(SANE_Int *) realloc(s->hw->
								     res_list,
								     s->hw->
								     res_list_size
								     *
								     sizeof
								     (SANE_Int));

					if (s->hw->res_list == NULL) {
						status = SANE_STATUS_NO_MEM;
						goto exit;
					}

					s->hw->res_list[s->hw->res_list_size -
							1] = (SANE_Int) val;

					DBG(10, " resolution (dpi): %d\n",
					    val);
					k = 3;
					continue;
				}
			case 'A':
				{
					x = area[2] << 8 | area[1];
					y = area[4] << 8 | area[3];

					DBG(1, " maximum scan area: %dx%d\n",
					    x, y);
					k = 5;
					continue;
				}
			default:
				break;
			}
			break;
		}

		dev->dpi_range.min = s->hw->res_list[0];
		dev->dpi_range.max =
			s->hw->res_list[s->hw->res_list_size - 1];
		dev->dpi_range.quant = 0;

		dev->fbf_x_range.min = 0;
		dev->fbf_x_range.max =
			SANE_FIX(x * MM_PER_INCH / dev->dpi_range.max);
		dev->fbf_x_range.quant = 0;

		dev->fbf_y_range.min = 0;
		dev->fbf_y_range.max =
			SANE_FIX(y * MM_PER_INCH / dev->dpi_range.max);
		dev->fbf_y_range.quant = 0;

		DBG(5, " fbf tlx %f tly %f brx %f bry %f [mm]\n",
		    SANE_UNFIX(dev->fbf_x_range.min),
		    SANE_UNFIX(dev->fbf_y_range.min),
		    SANE_UNFIX(dev->fbf_x_range.max),
		    SANE_UNFIX(dev->fbf_y_range.max));
	} else {
		status = SANE_STATUS_NO_MEM;
		goto exit;
	}

	/*
	 * Copy the resolution list to the resolution_list array so that the frontend can
	 * display the correct values
	 */

	s->hw->resolution_list =
		malloc((s->hw->res_list_size + 1) * sizeof(SANE_Word));

	if (s->hw->resolution_list == NULL) {
		status = SANE_STATUS_NO_MEM;
		goto exit;
	}

	*(s->hw->resolution_list) = s->hw->res_list_size;
	memcpy(&(s->hw->resolution_list[1]), s->hw->res_list,
	       s->hw->res_list_size * sizeof(SANE_Word));

	/* filter the resolution list */
	/* the option is not yet initialized, for now just set it to false */
	s->val[OPT_LIMIT_RESOLUTION].w = SANE_FALSE;
	filter_resolution_list(s);


exit:
	free(buf);

	return status;
}


/*
 * Request information from scanner and fill in information
 * into dev and/or scanner structures.
 */
static SANE_Status
request_identity2(SANE_Handle handle)
{
	Epson_Scanner *s = (Epson_Scanner *) handle;
	SANE_Status status;
	size_t len;
	unsigned char params[2];
	unsigned char *buf;

	DBG(5, "%s\n", __func__);

	if (s->hw->cmd->request_identity2 == 0)
		return SANE_STATUS_UNSUPPORTED;

	params[0] = ESC;
	params[1] = s->hw->cmd->request_identity2;

	status = epson2_cmd_info_block(s, params, 2, 0, &buf, &len);
	if (status != SANE_STATUS_GOOD)
		return status;

	if (buf[0] & 0x80) { /* what's this for? */
		status = SANE_STATUS_INVAL;
		goto exit;
	}

	/* the first two bytes of the buffer contain the optical resolution */
	s->hw->optical_res = buf[1] << 8 | buf[0];

	/*
	 * the 4th and 5th byte contain the line distance. Both values have to
	 * be identical, otherwise this software can not handle this scanner.
	 */
	if (buf[4] != buf[5]) {
		status = SANE_STATUS_INVAL;
		goto exit;
	}

	s->hw->max_line_distance = buf[4];

exit:
	if (buf)
		free(buf);

	return status;
}

/* Send the "initialize scanner" command to the device and reset it */

static SANE_Status
reset(Epson_Scanner * s)
{
	SANE_Status status;
	unsigned char params[2];
	SANE_Bool needToClose = SANE_FALSE;

	DBG(8, "%s\n", __func__);

	if (!s->hw->cmd->initialize_scanner)
		return SANE_STATUS_GOOD;

	params[0] = ESC;
	params[1] = s->hw->cmd->initialize_scanner;

	if (s->fd == -1) {
		needToClose = SANE_TRUE;
		DBG(5, "reset calling open_scanner\n");
		if ((status = open_scanner(s)) != SANE_STATUS_GOOD)
			return status;
	}

	status = epson2_cmd_simple(s, params, 2);

	if (needToClose)
		close_scanner(s);

	return status;
}

static SANE_Status
feed(Epson_Scanner * s)
{
	SANE_Status status;
	unsigned char params[1];

	DBG(8, "%s\n", __func__);

	if (!s->hw->cmd->feed) {
		DBG(5, "feed is not supported\n");
		return SANE_STATUS_UNSUPPORTED;
	}

	params[0] = s->hw->cmd->feed;

	status = epson2_cmd_simple(s, params, 1);
	if (status != SANE_STATUS_GOOD) {
		close_scanner(s);
		return status;
	}

	return status;
}


/*
 * Eject the current page from the ADF. The scanner is opened prior to
 * sending the command and closed afterwards.
 */

static SANE_Status
eject(Epson_Scanner * s)
{
	SANE_Status status;
	unsigned char params[1];
	SANE_Bool needToClose = SANE_FALSE;

	DBG(8, "%s\n", __func__);

	if (!s->hw->cmd->eject)
		return SANE_STATUS_UNSUPPORTED;

	if (s->fd == -1) {
		needToClose = SANE_TRUE;
		if (SANE_STATUS_GOOD != (status = open_scanner(s)))
			return status;

		reset(s);
		control_extension(s, 1);
	}

	params[0] = s->hw->cmd->eject;

	status = epson2_cmd_simple(s, params, 1);
	if (status != SANE_STATUS_GOOD) {
		close_scanner(s);
		return status;
	}

	if (needToClose)
		close_scanner(s);

	return status;
}

static SANE_Status
request_extended_status(SANE_Handle handle, unsigned char **data, size_t *data_len)
{
	Epson_Scanner *s = (Epson_Scanner *) handle;
	SANE_Status status = SANE_STATUS_GOOD;
	unsigned char params[2];
	unsigned char *buf;
	size_t buf_len;

	DBG(8, "%s\n", __func__);

	if (s->hw->cmd->request_extended_status == 0)
		return SANE_STATUS_UNSUPPORTED;

	params[0] = ESC;
	params[1] = s->hw->cmd->request_extended_status;

	/* This command returns 33 bytes of data on old scanners
	 * and 42 (CMD_SIZE_EXT_STATUS) on new ones.
	 */
	status = epson2_cmd_info_block(s, params, 2, CMD_SIZE_EXT_STATUS,
					&buf, &buf_len);
	if (status != SANE_STATUS_GOOD)
		return status;

	switch (buf_len) {
	case 33:
	case 42:
		break;
	default:
		DBG(1, "%s: unknown reply length (%d)\n", __func__, buf_len);
		break;
	}

	DBG(4, "main status  : 0x%02x\n", buf[0]);
	DBG(4, "ADF status   : 0x%02x\n", buf[1]);
	DBG(4, "TPU status   : 0x%02x\n", buf[6]);
	DBG(4, "main status 2: 0x%02x\n", buf[11]);

	if (buf[0] & EXT_STATUS_FER)
		DBG(1, "system error\n");

	if (buf[0] & EXT_STATUS_WU)
		DBG(1, "scanner is warming up\n");

	if (buf[1] & EXT_STATUS_ERR)
		DBG(1, "ADF: other error\n");

	if (buf[1] & EXT_STATUS_PE)
		DBG(1, "ADF: no paper\n");

	if (buf[1] & EXT_STATUS_PJ)
		DBG(1, "ADF: paper jam\n");

	if (buf[1] & EXT_STATUS_OPN)
		DBG(1, "ADF: cover open\n");

	if (buf[6] & EXT_STATUS_ERR)
		DBG(1, "TPU: other error\n");

	/* give back a pointer to the payload
	 * if the user requested it, otherwise
	 * free it.
	 */

	if (data)
		*data = buf;
	else
		free(buf);

	if (data_len)
		*data_len = buf_len;

	return status;
}


/*** XXXX ***/

/*
 * close_scanner()
 *
 * Close the open scanner. Depending on the connection method, a different
 * close function is called.
 */

static void
close_scanner(Epson_Scanner * s)
{
	DBG(8, "%s: fd = %d\n", __func__, s->fd);

	if (s->fd == -1)
		return;

	/* send a request_status. This toggles w_cmd_count and r_cmd_count */
	if (r_cmd_count % 2)
		request_status(s, NULL);

	/* request extended status. This toggles w_cmd_count only */
	if (w_cmd_count % 2) {
		request_extended_status(s, NULL, NULL);
	}

	if (s->hw->connection == SANE_EPSON_NET) {
		sanei_epson_net_unlock(s);
		sanei_tcp_close(s->fd);
	} else if (s->hw->connection == SANE_EPSON_SCSI) {
		sanei_scsi_close(s->fd);
	} else if (s->hw->connection == SANE_EPSON_PIO) {
		sanei_pio_close(s->fd);
	} else if (s->hw->connection == SANE_EPSON_USB) {
		sanei_usb_close(s->fd);
	}

	s->fd = -1;
	return;
}

/*
 * open_scanner()
 *
 * Open the scanner device. Depending on the connection method,
 * different open functions are called.
 */

static SANE_Status
open_scanner(Epson_Scanner * s)
{
	SANE_Status status = 0;

	DBG(8, "%s\n", __func__);

	if (s->fd != -1) {
		DBG(5, "scanner is already open: fd = %d\n", s->fd);
		return SANE_STATUS_GOOD;	/* no need to open the scanner */
	}

	if (s->hw->connection == SANE_EPSON_NET)
		status = sanei_tcp_open(s->hw->sane.name, 1865, &s->fd);
	else if (s->hw->connection == SANE_EPSON_SCSI)
		status = sanei_scsi_open(s->hw->sane.name, &s->fd,
					 sanei_epson2_scsi_sense_handler,
					 NULL);
	else if (s->hw->connection == SANE_EPSON_PIO)
		status = sanei_pio_open(s->hw->sane.name, &s->fd);
	else if (s->hw->connection == SANE_EPSON_USB)
		status = sanei_usb_open(s->hw->sane.name, &s->fd);

	if (status != SANE_STATUS_GOOD)
		DBG(1, "%s open failed: %s\n", s->hw->sane.name,
		    sane_strstatus(status));

	return status;
}


static int num_devices = 0;	/* number of scanners attached to backend */
static Epson_Device *first_dev = NULL;	/* first EPSON scanner in list */
static Epson_Scanner *first_handle = NULL;

/*
 * static SANE_Status attach()
 *
 * Attach one device with name *name to the backend.
 */

static SANE_Status
attach(const char *name, Epson_Device * *devp, int type)
{
	SANE_Status status;
	Epson_Scanner *s;
	struct Epson_Device *dev;
	SANE_String_Const *source_list_add = source_list;
	int port;

	DBG(1, "%s\n", SANE_EPSON2_VERSION);

	DBG(8, "%s: devname = %s, type = %d\n", __func__, name, type);

	for (dev = first_dev; dev; dev = dev->next) {
		if (strcmp(dev->sane.name, name) == 0) {
			if (devp) {
				*devp = dev;
			}
			return SANE_STATUS_GOOD;
		}
	}


	/* alloc and clear our device structure */
	dev = malloc(sizeof(*dev));
	if (!dev) {
		DBG(1, "out of memory (line %d)\n", __LINE__);
		return SANE_STATUS_NO_MEM;
	}
	memset(dev, 0x00, sizeof(struct Epson_Device));

	/* check for PIO devices */
	/* can we convert the device name to an integer? This is only possible
	   with PIO devices */
	if (type != SANE_EPSON_NET) {
		port = atoi(name);
		if (port != 0)
			type = SANE_EPSON_PIO;
	}

	if (strncmp
	    (name, SANE_EPSON_CONFIG_PIO,
	     strlen(SANE_EPSON_CONFIG_PIO)) == 0) {
		/* we have a match for the PIO string and adjust the device name */
		name += strlen(SANE_EPSON_CONFIG_PIO);
		name = sanei_config_skip_whitespace(name);
		type = SANE_EPSON_PIO;
	}


	s = malloc(sizeof(struct Epson_Scanner));
	if (s == NULL)
		return SANE_STATUS_NO_MEM;

	memset(s, 0x00, sizeof(struct Epson_Scanner));


	/*
	 *  set dummy values.
	 */

	s->hw = dev;
	s->hw->sane.name = NULL;
	s->hw->sane.type = "flatbed scanner";
	s->hw->sane.vendor = "Epson";
	s->hw->sane.model = NULL;
	s->hw->optical_res = 0;	/* just to have it initialized */
	s->hw->color_shuffle = SANE_FALSE;
	s->hw->extension = SANE_FALSE;
	s->hw->use_extension = SANE_FALSE;

	s->hw->need_color_reorder = SANE_FALSE;
	s->hw->need_double_vertical = SANE_FALSE;

	s->hw->cmd = &epson_cmd[EPSON_LEVEL_DEFAULT];	/* default function level */
	s->hw->connection = type;

	if (s->hw->connection == SANE_EPSON_NET)
		s->hw->cmd = &epson_cmd[EPSON_LEVEL_B7];

	DBG(3, "%s: opening %s, type = %d\n", __func__, name,
	    s->hw->connection);

	s->hw->last_res = 0;
	s->hw->last_res_preview = 0;	/* set resolution to safe values */

	if (s->hw->connection == SANE_EPSON_NET) {
		unsigned char buf[5];

		status = sanei_tcp_open(name, 1865, &s->fd);
		if (status != SANE_STATUS_GOOD) {
			DBG(1, "%s: %s open failed: %s\n", __func__,
			    name, sane_strstatus(status));
			goto free;
		}

		s->netlen = 0;
		/* the printer sends a kind of welcome msg */
		epson_recv(s, buf, 5, &status);

		sanei_epson_net_lock(s);

	} else if (s->hw->connection == SANE_EPSON_SCSI) {
		char buf[INQUIRY_BUF_SIZE + 1];
		size_t buf_size = INQUIRY_BUF_SIZE;

		char *vendor = buf + 8;
		char *model = buf + 16;
		char *rev = buf + 32;

		status = sanei_scsi_open(name, &s->fd,
					 sanei_epson2_scsi_sense_handler,
					 NULL);
		if (status != SANE_STATUS_GOOD) {
			DBG(1, "%s: open failed: %s\n", __func__,
			    sane_strstatus(status));
			goto free;
		}
		status = sanei_epson2_scsi_inquiry(s->fd, buf, &buf_size);
		if (status != SANE_STATUS_GOOD) {
			DBG(1, "%s: inquiry failed: %s\n", __func__,
			    sane_strstatus(status));
			close_scanner(s);
			goto free;
		}

		buf[INQUIRY_BUF_SIZE] = 0;
		DBG(1, "inquiry data:\n");
		DBG(1, " vendor  : %.8s\n", vendor);
		DBG(1, " model   : %.16s\n", model);
		DBG(1, " revision: %.4s\n", rev);

		if (buf[0] != TYPE_PROCESSOR) {
			DBG(1, "%s: device is not of processor type (%d)\n",
			    __func__, buf[0]);
			return SANE_STATUS_INVAL;
		}

		if (strncmp(vendor, "EPSON", 5) != 0) {
			DBG(1,
			    "%s: device doesn't look like an EPSON scanner\n",
			    __func__);
			close_scanner(s);
			return SANE_STATUS_INVAL;
		}

		if (strncmp(model, "SCANNER ", 8) != 0
		    && strncmp(model, "FilmScan 200", 12) != 0
		    && strncmp(model, "Perfection", 10) != 0
		    && strncmp(model, "Expression", 10) != 0
		    && strncmp(model, "GT", 2) != 0) {
			DBG(1, "%s: this EPSON scanner is not supported\n",
			    __func__);
			close_scanner(s);
			return SANE_STATUS_INVAL;
		}

		if (strncmp(model, "FilmScan 200", 12) == 0) {
			s->hw->sane.type = "film scanner";
			s->hw->sane.model = strndup(model, 12);
		}
	}
	/* use the SANEI functions to handle a PIO device */
	else if (s->hw->connection == SANE_EPSON_PIO) {
		if (SANE_STATUS_GOOD !=
		    (status = sanei_pio_open(name, &s->fd))) {
			DBG(1,
			    "cannot open %s as a parallel-port device: %s\n",
			    name, sane_strstatus(status));
			goto free;
		}
	}
	/* use the SANEI functions to handle a USB device */
	else if (s->hw->connection == SANE_EPSON_USB) {
		SANE_Word vendor;
		SANE_Word product;
		SANE_Bool isLibUSB;

		isLibUSB = (strncmp(name, "libusb:", strlen("libusb:")) == 0);

		if ((!isLibUSB) && (strlen(name) == 0)) {
			int i;
			int numIds;

			numIds = sanei_epson_getNumberOfUSBProductIds();

			for (i = 0; i < numIds; i++) {
				product = sanei_epson_usb_product_ids[i];
				vendor = 0x4b8;

				status = sanei_usb_find_devices(vendor,
								product,
								attach_one_usb);
			}
			return SANE_STATUS_INVAL;	/* return - the attach_one_usb()
							   will take care of this */
		}

		status = sanei_usb_open(name, &s->fd);

		if (status != SANE_STATUS_GOOD) {
			goto free;
		}

		/* if the sanei_usb_get_vendor_product call is not supported,
		   then we just ignore this and rely on the user to config
		   the correct device.
		 */

		if (sanei_usb_get_vendor_product(s->fd, &vendor, &product) ==
		    SANE_STATUS_GOOD) {
			int i;	/* loop variable */
			int numIds;
			SANE_Bool is_valid;

			/* check the vendor ID to see if we are dealing with an EPSON device */
			if (vendor != SANE_EPSON_VENDOR_ID) {
				/* this is not a supported vendor ID */
				DBG(1,
				    "the device at %s is not manufactured by EPSON (vendor id=0x%x)\n",
				    name, vendor);
				sanei_usb_close(s->fd);
				s->fd = -1;
				return SANE_STATUS_INVAL;
			}

			numIds = sanei_epson_getNumberOfUSBProductIds();
			is_valid = SANE_FALSE;
			i = 0;

			/* check all known product IDs to verify that we know
			   about the device */
			while (i != numIds && !is_valid) {
				if (product == sanei_epson_usb_product_ids[i])
					is_valid = SANE_TRUE;
				i++;
			}

			if (is_valid == SANE_FALSE) {
				DBG(1,
				    "the device at %s is not a supported EPSON scanner (product id=0x%x)\n",
				    name, product);
				sanei_usb_close(s->fd);
				s->fd = -1;
				return SANE_STATUS_INVAL;
			}
			DBG(1,
			    "found valid EPSON scanner: 0x%x/0x%x (vendorID/productID)\n",
			    vendor, product);
		} else
			DBG(1,
			    "cannot use IOCTL interface to verify that device is a scanner - will continue\n");
	}

	if (s->hw->sane.model == NULL)
		s->hw->sane.model = strdup("generic");

	/* Issue a test unit ready SCSI command. The FilmScan 200
	 * requires it for a sort of "wake up". We might eventually
	 * get the return code and reissue in case of failure.
	 */
	if (s->hw->connection == SANE_EPSON_SCSI)
		sanei_epson2_scsi_test_unit_ready(s->fd);

	/* ESC @, reset */
	reset(s);

	/* ESC I, request identity
	 * this must be the first command on the FilmScan 200
	 */
	if (s->hw->connection != SANE_EPSON_NET
	    && s->hw->cmd->request_identity) {

		status = request_identity(s);
		if (status != SANE_STATUS_GOOD)
			goto free;
	}

	/* ESC F, request status */
	if (s->hw->cmd->request_status) {
		unsigned char scanner_status;

		status = request_status(s, &scanner_status);
		if (status != SANE_STATUS_GOOD)
			goto free;

		/* set capabilities */
		if (scanner_status & STATUS_OPTION)
			dev->extension = SANE_TRUE;

		if (scanner_status & STATUS_EXT_COMMANDS)
			dev->extended_commands = 1;
	}

	/* FS I, request extended identity */
	if (s->hw->extended_commands) {
		char *p, model[17];
		unsigned char buf[80];

		status = request_extended_identity(s, buf);
		if (status != SANE_STATUS_GOOD)
			goto free;

		epson2_set_cmd_level(s, &buf[0]);

		s->hw->maxDepth = buf[67];

		/* get model name. it will probably be
		 * different than the one reported by request_identity
		 * for the same unit (i.e. LP-A500 vs CX11) .
		 */

		memcpy(model, &buf[46], 16);
		model[16] = '\0';

		p = strchr(model, ' ');
		if (p != NULL)
			*p = '\0';

		if (dev->sane.model)
			free(dev->sane.model);

		dev->sane.model = strndup(model, 16);
	}

	/*
	 * request identity 2 (ESC i), if available will
	 * get the information from the scanner and store it in dev
	 * XXX which scanner requires this command? Where's documented?
	 */
	if (s->hw->cmd->request_identity2) {
		status = request_identity2(s);
		if (status != SANE_STATUS_GOOD)
			goto free;

		/* XXX parse the result buffer here */
	}

	/*
	 * Check for the max. supported color depth and assign
	 * the values to the bitDepthList.
	 */
	bitDepthList = malloc(sizeof(SANE_Word) * 4);
	if (bitDepthList == NULL) {
		DBG(1, "out of memory (line %d)\n", __LINE__);
		return SANE_STATUS_NO_MEM;
	}

	bitDepthList[0] = 1;	/* we start with one element in the list */
	bitDepthList[1] = 8;	/* 8bit is the default */

	/* if s->hw->maxDepth has not previously set, try to discover it */

	if (s->hw->maxDepth == 0) {
		DBG(3, "discovering max depth, NAKs are expected\n");

		if (set_data_format(s, 16) == SANE_STATUS_GOOD) {
			s->hw->maxDepth = 16;

			bitDepthList[0]++;
			bitDepthList[bitDepthList[0]] = 16;

		} else if (set_data_format(s, 14) == SANE_STATUS_GOOD) {
			s->hw->maxDepth = 14;

			bitDepthList[0]++;
			bitDepthList[bitDepthList[0]] = 14;
		} else if (set_data_format(s, 12) == SANE_STATUS_GOOD) {
			s->hw->maxDepth = 12;

			bitDepthList[0]++;
			bitDepthList[bitDepthList[0]] = 12;
		} else {
			s->hw->maxDepth = 8;
		}
		/* the default depth is already in the list */

		DBG(3, "done\n");
	}

	DBG(1, "maximum supported color depth: %d\n", s->hw->maxDepth);

	/*
	 * Check for "request focus position" command. If this command is
	 * supported, then the scanner does also support the "set focus
	 * position" command.
	 * XXX ???
	 */

	if (request_focus_position(s, &s->currentFocusPosition) ==
	    SANE_STATUS_GOOD) {
		DBG(1, "setting focus is supported\n");
		s->hw->focusSupport = SANE_TRUE;
		s->opt[OPT_FOCUS].cap &= ~SANE_CAP_INACTIVE;

		/* reflect the current focus position in the GUI */
		if (s->currentFocusPosition < 0x4C) {
			/* focus on glass */
			s->val[OPT_FOCUS].w = 0;
		} else {
			/* focus 2.5mm above glass */
			s->val[OPT_FOCUS].w = 1;
		}

	} else {
		DBG(1, "setting focus is not supported\n");
		s->hw->focusSupport = SANE_FALSE;
		s->opt[OPT_FOCUS].cap |= SANE_CAP_INACTIVE;
		s->val[OPT_FOCUS].w = 0;	/* on glass - just in case */
	}

	/* Set defaults for no extension. */

	dev->x_range = &dev->fbf_x_range;
	dev->y_range = &dev->fbf_y_range;

	/*
	 * Correct for a firmware bug in some Perfection 1650 scanners:
	 * Firmware version 1.08 reports only half the vertical scan area, we have
	 * to double the number. To find out if we have to do this, we just compare
	 * is the vertical range is smaller than the horizontal range.
	 */

	if ((dev->x_range->max - dev->x_range->min) >
	    (dev->y_range->max - dev->y_range->min)) {
		dev->y_range->max += (dev->y_range->max - dev->y_range->min);
		dev->need_double_vertical = SANE_TRUE;
		dev->need_color_reorder = SANE_TRUE;
	}

	/*
	 * Extended status flag request (ESC f).
	 * this also requests the scanner device name from the the scanner.
	 * It seems unsupported on the network transport (CX11NF/LP-A500).
	 */
	if (s->hw->connection != SANE_EPSON_NET &&
	    s->hw->cmd->request_extended_status) {
		char *model, *p;
		unsigned char *es;
		size_t es_len;

		status = request_extended_status(s, &es, &es_len);
		if (status != SANE_STATUS_GOOD)
			goto free;

		/*
		 * Get the device name and copy it to dev->sane.model.
		 * The device name starts at es[0x1A] and is up to 16 bytes long
		 * We are overwriting whatever was set previously!
		 */
		if (es_len == CMD_SIZE_EXT_STATUS) { /* 42 */ 

			model = (char *) (es + 0x1A);
			p = strchr(model, ' ');
			if (p != NULL)
				*p = '\0';

			if (dev->sane.model)
				free(dev->sane.model);

			dev->sane.model = strndup(model, 16);
		}

		if (es[0] & EXT_STATUS_LID)
			DBG(1, "LID detected\n");

		if (es[0] & EXT_STATUS_PB)
			DBG(1, "push button(s) detected\n");
		else
			s->hw->cmd->request_push_button_status = 0;

		/* Flatbed */
		*source_list_add++ = FBF_STR;

		s->hw->devtype = es[11] >> 6;
		s->hw->fbf_max_x = es[13] << 8 | es[12];
		s->hw->fbf_max_y = es[15] << 8 | es[14];

		/* ADF */
		if (dev->extension && (es[1] & EXT_STATUS_IST)) {
			DBG(1, "ADF detected\n");

			fix_up_extended_status_reply(dev->sane.model, es);

			dev->duplexSupport = (es[0] & EXT_STATUS_ADFS) != 0;
			if (dev->duplexSupport) {
				DBG(1, "ADF supports duplex\n");
			}

			if (es[1] & EXT_STATUS_EN) {
				DBG(1, "ADF is enabled\n");
				dev->x_range = &dev->adf_x_range;
				dev->y_range = &dev->adf_y_range;
			}

			dev->adf_x_range.min = 0;
			dev->adf_x_range.max =
				SANE_FIX((es[3] << 8 | es[2]) *
					 MM_PER_INCH / dev->dpi_range.max);
			dev->adf_x_range.quant = 0;

			dev->adf_max_x = es[3] << 8 | es[2];

			dev->adf_y_range.min = 0;
			dev->adf_y_range.max =
				SANE_FIX((es[5] << 8 | es[4]) *
					 MM_PER_INCH / dev->dpi_range.max);
			dev->adf_y_range.quant = 0;

			dev->adf_max_y = es[5] << 8 | es[4];

			DBG(5, "adf tlx %f tly %f brx %f bry %f [mm]\n",
			    SANE_UNFIX(dev->adf_x_range.min),
			    SANE_UNFIX(dev->adf_y_range.min),
			    SANE_UNFIX(dev->adf_x_range.max),
			    SANE_UNFIX(dev->adf_y_range.max));

			*source_list_add++ = ADF_STR;

			dev->ADF = SANE_TRUE;
		}

		/* TPU */
		if (dev->extension && (es[6] & EXT_STATUS_IST)) {
			DBG(1, "TPU detected\n");

			if (es[6] & EXT_STATUS_EN) {
				DBG(1, "TPU is enabled\n");
				dev->x_range = &dev->tpu_x_range;
				dev->y_range = &dev->tpu_y_range;
			}

			dev->tpu_x_range.min = 0;
			dev->tpu_x_range.max =
				SANE_FIX((es[8] << 8 | es[7]) *
					 MM_PER_INCH / dev->dpi_range.max);
			dev->tpu_x_range.quant = 0;

			dev->tpu_y_range.min = 0;
			dev->tpu_y_range.max =
				SANE_FIX((es[10] << 8 | es[9]) *
					 MM_PER_INCH / dev->dpi_range.max);
			dev->tpu_y_range.quant = 0;

			DBG(5, "tpu tlx %f tly %f brx %f bry %f [mm]\n",
			    SANE_UNFIX(dev->tpu_x_range.min),
			    SANE_UNFIX(dev->tpu_y_range.min),
			    SANE_UNFIX(dev->tpu_x_range.max),
			    SANE_UNFIX(dev->tpu_y_range.max));

			*source_list_add++ = TPU_STR;

			dev->TPU = SANE_TRUE;
		}

		free(es);
	}

	/* FS F, request scanner status */
	if (s->hw->extended_commands) {
		unsigned char buf[16];

		status = request_scanner_status(s, buf);
		if (status != SANE_STATUS_GOOD)
			goto free;
	}

	/* XXX necessary? */
	reset(s);


	*source_list_add = NULL;	/* add end marker to source list */

	DBG(1, "scanner model: %s\n", dev->sane.model);

	/* establish defaults */
	s->hw->need_reset_on_source_change = SANE_FALSE;

	if (strcmp("ES-9000H", dev->sane.model) == 0
	    || strcmp("GT-30000", dev->sane.model) == 0) {
		s->hw->cmd->set_focus_position = 0;
		s->hw->cmd->feed = 0x19;
	} else if (strcmp("GT-8200", dev->sane.model) == 0
		   || strcmp("Perfection1650", dev->sane.model) == 0
		   || strcmp("Perfection1640", dev->sane.model) == 0
		   || strcmp("GT-8700", dev->sane.model) == 0) {
		s->hw->cmd->feed = 0;
		s->hw->cmd->set_focus_position = 0;
		s->hw->need_reset_on_source_change = SANE_TRUE;
	}

	/* Set values for quick format "max" entry */
	qf_params[XtNumber(qf_params) - 1].tl_x = dev->x_range->min;
	qf_params[XtNumber(qf_params) - 1].tl_y = dev->y_range->min;
	qf_params[XtNumber(qf_params) - 1].br_x = dev->x_range->max;
	qf_params[XtNumber(qf_params) - 1].br_y = dev->y_range->max;

	/* Now we can finally set the device name */
	dev->sane.name = strdup(name);

	close_scanner(s);

	/* we are done with this one, prepare for the next scanner */
	num_devices++;
	dev->next = first_dev;
	first_dev = dev;

	if (devp)
		*devp = dev;

free:
	free(s);
	return status;
}

/*
 * Part of the SANE API: Attaches the scanner with the device name in *dev.
 */

static SANE_Status
attach_one(const char *dev)
{
	DBG(8, "%s: dev = %s\n", __func__, dev);
	return attach(dev, 0, SANE_EPSON_SCSI);
}

SANE_Status
attach_one_usb(const char *dev)
{
	DBG(8, "%s: dev = %s\n", __func__, dev);
	return attach(dev, 0, SANE_EPSON_USB);
}

static SANE_Status
attach_one_net(const char *dev)
{
	DBG(8, "%s: dev = %s\n", __func__, dev);
	return attach(dev, 0, SANE_EPSON_NET);
}

SANE_Status
sane_init(SANE_Int * version_code, SANE_Auth_Callback authorize)
{
	size_t len;
	FILE *fp;

	authorize = authorize;	/* get rid of compiler warning */

	/* sanei_authorization(devicename, STRINGIFY(BACKEND_NAME), auth_callback); */

	DBG_INIT();
#if defined PACKAGE && defined VERSION
	DBG(2, "%s: " PACKAGE " " VERSION "\n", __func__);
#endif

	if (version_code != NULL)
		*version_code =
			SANE_VERSION_CODE(V_MAJOR, V_MINOR,
					  SANE_EPSON2_BUILD);

	sanei_usb_init();

	if ((fp = sanei_config_open(EPSON2_CONFIG_FILE))) {
		char line[PATH_MAX];

		DBG(3, "%s: reading config file, %s\n", __func__,
		    EPSON2_CONFIG_FILE);

		while (sanei_config_read(line, sizeof(line), fp)) {
			int vendor, product;

			if (line[0] == '#')	/* ignore line comments */
				continue;

			len = strlen(line);
			if (len == 0)
				continue;	/* ignore empty lines */

			DBG(120, " %s\n", line);

			if (sscanf(line, "usb %i %i", &vendor, &product) == 2) {
				int numIds;

				/* add the vendor and product IDs to the list of
				   known devices before we call the attach function */
				numIds = sanei_epson_getNumberOfUSBProductIds();
				if (vendor != 0x4b8)
					continue;	/* this is not an EPSON device */

				sanei_epson_usb_product_ids[numIds - 1] =
					product;
				sanei_usb_attach_matching_devices(line,
								  attach_one_usb);
			} else if (strncmp(line, "usb", 3) == 0) {
				const char *name;
				/* remove the "usb" sub string */
				name = sanei_config_skip_whitespace(line + 3);
				attach_one_usb(name);
			} else if (strncmp(line, "net", 3) == 0) {
				const char *name;
				/* remove the "net" sub string */
				name = sanei_config_skip_whitespace(line + 3);
				attach_one_net(name);
			} else {
				sanei_config_attach_matching_devices(line,
								     attach_one);
			}
		}
		fclose(fp);
	}

	/* read the option section and assign the connection type to the
	   scanner structure - which we don't have at this time. So I have
	   to come up with something :-) */

	return SANE_STATUS_GOOD;
}

/* Clean up the list of attached scanners. */
void
sane_exit(void)
{
	Epson_Device *dev, *next;

	for (dev = first_dev; dev; dev = next) {
		next = dev->next;
		free(dev->sane.name);
		free(dev->sane.model);
		free(dev);
	}

	free(devlist);
}

SANE_Status
sane_get_devices(const SANE_Device * **device_list, SANE_Bool local_only)
{
	Epson_Device *dev;
	int i;

	DBG(5, "%s\n", __func__);

	local_only = local_only;	/* just to get rid of the compiler warning */

	if (devlist) {
		free(devlist);
	}

	devlist = malloc((num_devices + 1) * sizeof(devlist[0]));
	if (!devlist) {
		DBG(1, "out of memory (line %d)\n", __LINE__);
		return SANE_STATUS_NO_MEM;
	}

	for (i = 0, dev = first_dev; i < num_devices; dev = dev->next, i++) {
		DBG(1, " %d: %s\n", i, dev->sane.model);
		devlist[i] = &dev->sane;
	}

	devlist[i] = NULL;

	*device_list = devlist;

	return SANE_STATUS_GOOD;
}

static SANE_Status
init_options(Epson_Scanner * s)
{
	int i;

	DBG(5, "%s\n", __func__);

	for (i = 0; i < NUM_OPTIONS; ++i) {
		s->opt[i].size = sizeof(SANE_Word);
		s->opt[i].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
	}

	s->opt[OPT_NUM_OPTS].title = SANE_TITLE_NUM_OPTIONS;
	s->opt[OPT_NUM_OPTS].desc = SANE_DESC_NUM_OPTIONS;
	s->opt[OPT_NUM_OPTS].cap = SANE_CAP_SOFT_DETECT;
	s->val[OPT_NUM_OPTS].w = NUM_OPTIONS;

	/* "Scan Mode" group: */

	s->opt[OPT_MODE_GROUP].title = SANE_I18N("Scan Mode");
	s->opt[OPT_MODE_GROUP].desc = "";
	s->opt[OPT_MODE_GROUP].type = SANE_TYPE_GROUP;
	s->opt[OPT_MODE_GROUP].cap = 0;

	/* scan mode */
	s->opt[OPT_MODE].name = SANE_NAME_SCAN_MODE;
	s->opt[OPT_MODE].title = SANE_TITLE_SCAN_MODE;
	s->opt[OPT_MODE].desc = SANE_DESC_SCAN_MODE;
	s->opt[OPT_MODE].type = SANE_TYPE_STRING;
	s->opt[OPT_MODE].size = max_string_size(mode_list);
	s->opt[OPT_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
	s->opt[OPT_MODE].constraint.string_list = mode_list;
	s->val[OPT_MODE].w = 0;	/* Binary */

	/* bit depth */
	s->opt[OPT_BIT_DEPTH].name = SANE_NAME_BIT_DEPTH;
	s->opt[OPT_BIT_DEPTH].title = SANE_TITLE_BIT_DEPTH;
	s->opt[OPT_BIT_DEPTH].desc = SANE_DESC_BIT_DEPTH;
	s->opt[OPT_BIT_DEPTH].type = SANE_TYPE_INT;
	s->opt[OPT_BIT_DEPTH].unit = SANE_UNIT_NONE;
	s->opt[OPT_BIT_DEPTH].constraint_type = SANE_CONSTRAINT_WORD_LIST;
	s->opt[OPT_BIT_DEPTH].constraint.word_list = bitDepthList;
	s->opt[OPT_BIT_DEPTH].cap |= SANE_CAP_INACTIVE;
	s->val[OPT_BIT_DEPTH].w = bitDepthList[1];	/* the first "real" element is the default */

	if (bitDepthList[0] == 1)	/* only one element in the list -> hide the option */
		s->opt[OPT_BIT_DEPTH].cap |= SANE_CAP_INACTIVE;

	/* halftone */
	s->opt[OPT_HALFTONE].name = SANE_NAME_HALFTONE;
	s->opt[OPT_HALFTONE].title = SANE_TITLE_HALFTONE;
	s->opt[OPT_HALFTONE].desc = SANE_I18N("Selects the halftone.");

	s->opt[OPT_HALFTONE].type = SANE_TYPE_STRING;
	s->opt[OPT_HALFTONE].size = max_string_size(halftone_list_7);
	s->opt[OPT_HALFTONE].constraint_type = SANE_CONSTRAINT_STRING_LIST;

	if (s->hw->level >= 7)
		s->opt[OPT_HALFTONE].constraint.string_list = halftone_list_7;
	else if (s->hw->level >= 4)
		s->opt[OPT_HALFTONE].constraint.string_list = halftone_list_4;
	else
		s->opt[OPT_HALFTONE].constraint.string_list = halftone_list;

	s->val[OPT_HALFTONE].w = 1;	/* Halftone A */

	if (!s->hw->cmd->set_halftoning) {
		s->opt[OPT_HALFTONE].cap |= SANE_CAP_INACTIVE;
	}

	/* dropout */
	s->opt[OPT_DROPOUT].name = "dropout";
	s->opt[OPT_DROPOUT].title = SANE_I18N("Dropout");
	s->opt[OPT_DROPOUT].desc = SANE_I18N("Selects the dropout.");

	s->opt[OPT_DROPOUT].type = SANE_TYPE_STRING;
	s->opt[OPT_DROPOUT].size = max_string_size(dropout_list);
	s->opt[OPT_DROPOUT].cap |= SANE_CAP_ADVANCED;
	s->opt[OPT_DROPOUT].constraint_type = SANE_CONSTRAINT_STRING_LIST;
	s->opt[OPT_DROPOUT].constraint.string_list = dropout_list;
	s->val[OPT_DROPOUT].w = 0;	/* None */

	/* brightness */
	s->opt[OPT_BRIGHTNESS].name = SANE_NAME_BRIGHTNESS;
	s->opt[OPT_BRIGHTNESS].title = SANE_TITLE_BRIGHTNESS;
	s->opt[OPT_BRIGHTNESS].desc = SANE_I18N("Selects the brightness.");

	s->opt[OPT_BRIGHTNESS].type = SANE_TYPE_INT;
	s->opt[OPT_BRIGHTNESS].unit = SANE_UNIT_NONE;
	s->opt[OPT_BRIGHTNESS].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_BRIGHTNESS].constraint.range = &s->hw->cmd->bright_range;
	s->val[OPT_BRIGHTNESS].w = 0;	/* Normal */

	if (!s->hw->cmd->set_bright) {
		s->opt[OPT_BRIGHTNESS].cap |= SANE_CAP_INACTIVE;
	}

	/* sharpness */
	s->opt[OPT_SHARPNESS].name = "sharpness";
	s->opt[OPT_SHARPNESS].title = SANE_I18N("Sharpness");
	s->opt[OPT_SHARPNESS].desc = "";

	s->opt[OPT_SHARPNESS].type = SANE_TYPE_INT;
	s->opt[OPT_SHARPNESS].unit = SANE_UNIT_NONE;
	s->opt[OPT_SHARPNESS].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_SHARPNESS].constraint.range = &outline_emphasis_range;
	s->val[OPT_SHARPNESS].w = 0;	/* Normal */

	if (!s->hw->cmd->set_outline_emphasis) {
		s->opt[OPT_SHARPNESS].cap |= SANE_CAP_INACTIVE;
	}


	/* gamma */
	s->opt[OPT_GAMMA_CORRECTION].name = SANE_NAME_GAMMA_CORRECTION;
	s->opt[OPT_GAMMA_CORRECTION].title = SANE_TITLE_GAMMA_CORRECTION;
	s->opt[OPT_GAMMA_CORRECTION].desc = SANE_DESC_GAMMA_CORRECTION;

	s->opt[OPT_GAMMA_CORRECTION].type = SANE_TYPE_STRING;
	s->opt[OPT_GAMMA_CORRECTION].constraint_type =
		SANE_CONSTRAINT_STRING_LIST;
	/*
	 * special handling for D1 function level - at this time I'm not
	 * testing for D1, I'm just assuming that all D level scanners will
	 * behave the same way. This has to be confirmed with the next D-level
	 * scanner
	 */
	if (s->hw->cmd->level[0] == 'D') {
		s->opt[OPT_GAMMA_CORRECTION].size =
			max_string_size(gamma_list_d);
		s->opt[OPT_GAMMA_CORRECTION].constraint.string_list =
			gamma_list_d;
		s->val[OPT_GAMMA_CORRECTION].w = 1;	/* Default */
		gamma_userdefined = gamma_userdefined_d;
		gamma_params = gamma_params_d;
	} else {
		s->opt[OPT_GAMMA_CORRECTION].size =
			max_string_size(gamma_list_ab);
		s->opt[OPT_GAMMA_CORRECTION].constraint.string_list =
			gamma_list_ab;
		s->val[OPT_GAMMA_CORRECTION].w = 0;	/* Default */
		gamma_userdefined = gamma_userdefined_ab;
		gamma_params = gamma_params_ab;
	}

	if (!s->hw->cmd->set_gamma) {
		s->opt[OPT_GAMMA_CORRECTION].cap |= SANE_CAP_INACTIVE;
	}


	/* gamma vector */

/*
		s->opt[ OPT_GAMMA_VECTOR].name  = SANE_NAME_GAMMA_VECTOR;
		s->opt[ OPT_GAMMA_VECTOR].title = SANE_TITLE_GAMMA_VECTOR;
		s->opt[ OPT_GAMMA_VECTOR].desc  = SANE_DESC_GAMMA_VECTOR;

		s->opt[ OPT_GAMMA_VECTOR].type = SANE_TYPE_INT;
		s->opt[ OPT_GAMMA_VECTOR].unit = SANE_UNIT_NONE;
		s->opt[ OPT_GAMMA_VECTOR].size = 256 * sizeof (SANE_Word);
		s->opt[ OPT_GAMMA_VECTOR].constraint_type = SANE_CONSTRAINT_RANGE;
		s->opt[ OPT_GAMMA_VECTOR].constraint.range = &u8_range;
		s->val[ OPT_GAMMA_VECTOR].wa = &s->gamma_table [ 0] [ 0];
*/


	/* red gamma vector */
	s->opt[OPT_GAMMA_VECTOR_R].name = SANE_NAME_GAMMA_VECTOR_R;
	s->opt[OPT_GAMMA_VECTOR_R].title = SANE_TITLE_GAMMA_VECTOR_R;
	s->opt[OPT_GAMMA_VECTOR_R].desc = SANE_DESC_GAMMA_VECTOR_R;

	s->opt[OPT_GAMMA_VECTOR_R].type = SANE_TYPE_INT;
	s->opt[OPT_GAMMA_VECTOR_R].unit = SANE_UNIT_NONE;
	s->opt[OPT_GAMMA_VECTOR_R].size = 256 * sizeof(SANE_Word);
	s->opt[OPT_GAMMA_VECTOR_R].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_GAMMA_VECTOR_R].constraint.range = &u8_range;
	s->val[OPT_GAMMA_VECTOR_R].wa = &s->gamma_table[0][0];


	/* green gamma vector */
	s->opt[OPT_GAMMA_VECTOR_G].name = SANE_NAME_GAMMA_VECTOR_G;
	s->opt[OPT_GAMMA_VECTOR_G].title = SANE_TITLE_GAMMA_VECTOR_G;
	s->opt[OPT_GAMMA_VECTOR_G].desc = SANE_DESC_GAMMA_VECTOR_G;

	s->opt[OPT_GAMMA_VECTOR_G].type = SANE_TYPE_INT;
	s->opt[OPT_GAMMA_VECTOR_G].unit = SANE_UNIT_NONE;
	s->opt[OPT_GAMMA_VECTOR_G].size = 256 * sizeof(SANE_Word);
	s->opt[OPT_GAMMA_VECTOR_G].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_GAMMA_VECTOR_G].constraint.range = &u8_range;
	s->val[OPT_GAMMA_VECTOR_G].wa = &s->gamma_table[1][0];


	/* red gamma vector */
	s->opt[OPT_GAMMA_VECTOR_B].name = SANE_NAME_GAMMA_VECTOR_B;
	s->opt[OPT_GAMMA_VECTOR_B].title = SANE_TITLE_GAMMA_VECTOR_B;
	s->opt[OPT_GAMMA_VECTOR_B].desc = SANE_DESC_GAMMA_VECTOR_B;

	s->opt[OPT_GAMMA_VECTOR_B].type = SANE_TYPE_INT;
	s->opt[OPT_GAMMA_VECTOR_B].unit = SANE_UNIT_NONE;
	s->opt[OPT_GAMMA_VECTOR_B].size = 256 * sizeof(SANE_Word);
	s->opt[OPT_GAMMA_VECTOR_B].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_GAMMA_VECTOR_B].constraint.range = &u8_range;
	s->val[OPT_GAMMA_VECTOR_B].wa = &s->gamma_table[2][0];

	if (s->hw->cmd->set_gamma_table
	    && gamma_userdefined[s->val[OPT_GAMMA_CORRECTION].w] ==
	    SANE_TRUE) {

/*			s->opt[ OPT_GAMMA_VECTOR].cap &= ~SANE_CAP_INACTIVE; */
		s->opt[OPT_GAMMA_VECTOR_R].cap &= ~SANE_CAP_INACTIVE;
		s->opt[OPT_GAMMA_VECTOR_G].cap &= ~SANE_CAP_INACTIVE;
		s->opt[OPT_GAMMA_VECTOR_B].cap &= ~SANE_CAP_INACTIVE;
	} else {

/*			s->opt[ OPT_GAMMA_VECTOR].cap |= SANE_CAP_INACTIVE; */
		s->opt[OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
		s->opt[OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
		s->opt[OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;
	}

	/* initialize the Gamma tables */
	memset(&s->gamma_table[0], 0, 256 * sizeof(SANE_Word));
	memset(&s->gamma_table[1], 0, 256 * sizeof(SANE_Word));
	memset(&s->gamma_table[2], 0, 256 * sizeof(SANE_Word));

/*		memset(&s->gamma_table[3], 0, 256 * sizeof(SANE_Word)); */
	for (i = 0; i < 256; i++) {
		s->gamma_table[0][i] = i;
		s->gamma_table[1][i] = i;
		s->gamma_table[2][i] = i;

/*			s->gamma_table[3][i] = i; */
	}


	/* color correction */
	s->opt[OPT_COLOR_CORRECTION].name = "color-correction";
	s->opt[OPT_COLOR_CORRECTION].title = SANE_I18N("Color correction");
	s->opt[OPT_COLOR_CORRECTION].desc =
		SANE_I18N
		("Sets the color correction table for the selected output device.");

	s->opt[OPT_COLOR_CORRECTION].type = SANE_TYPE_STRING;
	s->opt[OPT_COLOR_CORRECTION].size = 32;
	s->opt[OPT_COLOR_CORRECTION].cap |= SANE_CAP_ADVANCED;
	s->opt[OPT_COLOR_CORRECTION].constraint_type =
		SANE_CONSTRAINT_STRING_LIST;
	s->opt[OPT_COLOR_CORRECTION].constraint.string_list = color_list;
	s->val[OPT_COLOR_CORRECTION].w = 5;	/* scanner default: CRT monitors */

	if (!s->hw->cmd->set_color_correction) {
		s->opt[OPT_COLOR_CORRECTION].cap |= SANE_CAP_INACTIVE;
	}

	/* resolution */
	s->opt[OPT_RESOLUTION].name = SANE_NAME_SCAN_RESOLUTION;
	s->opt[OPT_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
	s->opt[OPT_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;

	s->opt[OPT_RESOLUTION].type = SANE_TYPE_INT;
	s->opt[OPT_RESOLUTION].unit = SANE_UNIT_DPI;
	s->opt[OPT_RESOLUTION].constraint_type = SANE_CONSTRAINT_WORD_LIST;
	s->opt[OPT_RESOLUTION].constraint.word_list = s->hw->resolution_list;
	s->val[OPT_RESOLUTION].w = s->hw->dpi_range.min;

	/* threshold */
	s->opt[OPT_THRESHOLD].name = SANE_NAME_THRESHOLD;
	s->opt[OPT_THRESHOLD].title = SANE_TITLE_THRESHOLD;
	s->opt[OPT_THRESHOLD].desc = SANE_DESC_THRESHOLD;

	s->opt[OPT_THRESHOLD].type = SANE_TYPE_INT;
	s->opt[OPT_THRESHOLD].unit = SANE_UNIT_NONE;
	s->opt[OPT_THRESHOLD].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_THRESHOLD].constraint.range = &u8_range;
	s->val[OPT_THRESHOLD].w = 0x80;

	if (!s->hw->cmd->set_threshold) {
		s->opt[OPT_THRESHOLD].cap |= SANE_CAP_INACTIVE;
	}

	s->opt[OPT_CCT_GROUP].title =
		SANE_I18N("Color correction coefficients");
	s->opt[OPT_CCT_GROUP].desc =
		SANE_I18N("Matrix multiplication of RGB");
	s->opt[OPT_CCT_GROUP].type = SANE_TYPE_GROUP;
	s->opt[OPT_CCT_GROUP].cap = SANE_CAP_ADVANCED;


	/* color correction coefficients */
	s->opt[OPT_CCT_1].name = "cct-1";
	s->opt[OPT_CCT_2].name = "cct-2";
	s->opt[OPT_CCT_3].name = "cct-3";
	s->opt[OPT_CCT_4].name = "cct-4";
	s->opt[OPT_CCT_5].name = "cct-5";
	s->opt[OPT_CCT_6].name = "cct-6";
	s->opt[OPT_CCT_7].name = "cct-7";
	s->opt[OPT_CCT_8].name = "cct-8";
	s->opt[OPT_CCT_9].name = "cct-9";

	s->opt[OPT_CCT_1].title = SANE_I18N("Green");
	s->opt[OPT_CCT_2].title = SANE_I18N("Shift green to red");
	s->opt[OPT_CCT_3].title = SANE_I18N("Shift green to blue");
	s->opt[OPT_CCT_4].title = SANE_I18N("Shift red to green");
	s->opt[OPT_CCT_5].title = SANE_I18N("Red");
	s->opt[OPT_CCT_6].title = SANE_I18N("Shift red to blue");
	s->opt[OPT_CCT_7].title = SANE_I18N("Shift blue to green");
	s->opt[OPT_CCT_8].title = SANE_I18N("Shift blue to red");
	s->opt[OPT_CCT_9].title = SANE_I18N("Blue");

	s->opt[OPT_CCT_1].desc = SANE_I18N("Controls green level");
	s->opt[OPT_CCT_2].desc =
		SANE_I18N("Adds to red based on green level");
	s->opt[OPT_CCT_3].desc =
		SANE_I18N("Adds to blue based on green level");
	s->opt[OPT_CCT_4].desc =
		SANE_I18N("Adds to green based on red level");
	s->opt[OPT_CCT_5].desc = SANE_I18N("Controls red level");
	s->opt[OPT_CCT_6].desc = SANE_I18N("Adds to blue based on red level");
	s->opt[OPT_CCT_7].desc =
		SANE_I18N("Adds to green based on blue level");
	s->opt[OPT_CCT_8].desc = SANE_I18N("Adds to red based on blue level");
	s->opt[OPT_CCT_9].desc = SANE_I18N("Controls blue level");

	s->opt[OPT_CCT_1].type = SANE_TYPE_INT;
	s->opt[OPT_CCT_2].type = SANE_TYPE_INT;
	s->opt[OPT_CCT_3].type = SANE_TYPE_INT;
	s->opt[OPT_CCT_4].type = SANE_TYPE_INT;
	s->opt[OPT_CCT_5].type = SANE_TYPE_INT;
	s->opt[OPT_CCT_6].type = SANE_TYPE_INT;
	s->opt[OPT_CCT_7].type = SANE_TYPE_INT;
	s->opt[OPT_CCT_8].type = SANE_TYPE_INT;
	s->opt[OPT_CCT_9].type = SANE_TYPE_INT;

	s->opt[OPT_CCT_1].cap |= SANE_CAP_ADVANCED;
	s->opt[OPT_CCT_2].cap |= SANE_CAP_ADVANCED;
	s->opt[OPT_CCT_3].cap |= SANE_CAP_ADVANCED;
	s->opt[OPT_CCT_4].cap |= SANE_CAP_ADVANCED;
	s->opt[OPT_CCT_5].cap |= SANE_CAP_ADVANCED;
	s->opt[OPT_CCT_6].cap |= SANE_CAP_ADVANCED;
	s->opt[OPT_CCT_7].cap |= SANE_CAP_ADVANCED;
	s->opt[OPT_CCT_8].cap |= SANE_CAP_ADVANCED;
	s->opt[OPT_CCT_9].cap |= SANE_CAP_ADVANCED;

	s->opt[OPT_CCT_1].cap |= SANE_CAP_INACTIVE;
	s->opt[OPT_CCT_2].cap |= SANE_CAP_INACTIVE;
	s->opt[OPT_CCT_3].cap |= SANE_CAP_INACTIVE;
	s->opt[OPT_CCT_4].cap |= SANE_CAP_INACTIVE;
	s->opt[OPT_CCT_5].cap |= SANE_CAP_INACTIVE;
	s->opt[OPT_CCT_6].cap |= SANE_CAP_INACTIVE;
	s->opt[OPT_CCT_7].cap |= SANE_CAP_INACTIVE;
	s->opt[OPT_CCT_8].cap |= SANE_CAP_INACTIVE;
	s->opt[OPT_CCT_9].cap |= SANE_CAP_INACTIVE;

	s->opt[OPT_CCT_1].unit = SANE_UNIT_NONE;
	s->opt[OPT_CCT_2].unit = SANE_UNIT_NONE;
	s->opt[OPT_CCT_3].unit = SANE_UNIT_NONE;
	s->opt[OPT_CCT_4].unit = SANE_UNIT_NONE;
	s->opt[OPT_CCT_5].unit = SANE_UNIT_NONE;
	s->opt[OPT_CCT_6].unit = SANE_UNIT_NONE;
	s->opt[OPT_CCT_7].unit = SANE_UNIT_NONE;
	s->opt[OPT_CCT_8].unit = SANE_UNIT_NONE;
	s->opt[OPT_CCT_9].unit = SANE_UNIT_NONE;

	s->opt[OPT_CCT_1].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_CCT_2].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_CCT_3].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_CCT_4].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_CCT_5].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_CCT_6].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_CCT_7].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_CCT_8].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_CCT_9].constraint_type = SANE_CONSTRAINT_RANGE;

	s->opt[OPT_CCT_1].constraint.range = &s8_range;
	s->opt[OPT_CCT_2].constraint.range = &s8_range;
	s->opt[OPT_CCT_3].constraint.range = &s8_range;
	s->opt[OPT_CCT_4].constraint.range = &s8_range;
	s->opt[OPT_CCT_5].constraint.range = &s8_range;
	s->opt[OPT_CCT_6].constraint.range = &s8_range;
	s->opt[OPT_CCT_7].constraint.range = &s8_range;
	s->opt[OPT_CCT_8].constraint.range = &s8_range;
	s->opt[OPT_CCT_9].constraint.range = &s8_range;

	s->val[OPT_CCT_1].w = 32;
	s->val[OPT_CCT_2].w = 0;
	s->val[OPT_CCT_3].w = 0;
	s->val[OPT_CCT_4].w = 0;
	s->val[OPT_CCT_5].w = 32;
	s->val[OPT_CCT_6].w = 0;
	s->val[OPT_CCT_7].w = 0;
	s->val[OPT_CCT_8].w = 0;
	s->val[OPT_CCT_9].w = 32;

	if (!s->hw->cmd->set_color_correction_coefficients) {
		s->opt[OPT_CCT_1].cap |= SANE_CAP_INACTIVE;
		s->opt[OPT_CCT_2].cap |= SANE_CAP_INACTIVE;
		s->opt[OPT_CCT_3].cap |= SANE_CAP_INACTIVE;
		s->opt[OPT_CCT_4].cap |= SANE_CAP_INACTIVE;
		s->opt[OPT_CCT_5].cap |= SANE_CAP_INACTIVE;
		s->opt[OPT_CCT_6].cap |= SANE_CAP_INACTIVE;
		s->opt[OPT_CCT_7].cap |= SANE_CAP_INACTIVE;
		s->opt[OPT_CCT_8].cap |= SANE_CAP_INACTIVE;
		s->opt[OPT_CCT_9].cap |= SANE_CAP_INACTIVE;
	}


	/* "Advanced" group: */
	s->opt[OPT_ADVANCED_GROUP].title = SANE_I18N("Advanced");
	s->opt[OPT_ADVANCED_GROUP].desc = "";
	s->opt[OPT_ADVANCED_GROUP].type = SANE_TYPE_GROUP;
	s->opt[OPT_ADVANCED_GROUP].cap = SANE_CAP_ADVANCED;


	/* mirror */
	s->opt[OPT_MIRROR].name = "mirror";
	s->opt[OPT_MIRROR].title = SANE_I18N("Mirror image");
	s->opt[OPT_MIRROR].desc = SANE_I18N("Mirror the image.");

	s->opt[OPT_MIRROR].type = SANE_TYPE_BOOL;
	s->val[OPT_MIRROR].w = SANE_FALSE;

	if (!s->hw->cmd->mirror_image) {
		s->opt[OPT_MIRROR].cap |= SANE_CAP_INACTIVE;
	}


	/* speed */
	s->opt[OPT_SPEED].name = SANE_NAME_SCAN_SPEED;
	s->opt[OPT_SPEED].title = SANE_TITLE_SCAN_SPEED;
	s->opt[OPT_SPEED].desc = SANE_DESC_SCAN_SPEED;

	s->opt[OPT_SPEED].type = SANE_TYPE_BOOL;
	s->val[OPT_SPEED].w = SANE_FALSE;

	if (!s->hw->cmd->set_speed) {
		s->opt[OPT_SPEED].cap |= SANE_CAP_INACTIVE;
	}

	/* preview speed */
	s->opt[OPT_PREVIEW_SPEED].name = "preview-speed";
	s->opt[OPT_PREVIEW_SPEED].title = SANE_I18N("High speed preview");
	s->opt[OPT_PREVIEW_SPEED].desc = "";

	s->opt[OPT_PREVIEW_SPEED].type = SANE_TYPE_BOOL;
	s->val[OPT_PREVIEW_SPEED].w = SANE_FALSE;

	if (!s->hw->cmd->set_speed) {
		s->opt[OPT_PREVIEW_SPEED].cap |= SANE_CAP_INACTIVE;
	}

	/* auto area segmentation */
	s->opt[OPT_AAS].name = "auto-area-segmentation";
	s->opt[OPT_AAS].title = SANE_I18N("Auto area segmentation");
	s->opt[OPT_AAS].desc =
		"Enables different dithering modes in image and text areas";

	s->opt[OPT_AAS].type = SANE_TYPE_BOOL;
	s->val[OPT_AAS].w = SANE_TRUE;

	if (!s->hw->cmd->control_auto_area_segmentation) {
		s->opt[OPT_AAS].cap |= SANE_CAP_INACTIVE;
	}

	/* limit resolution list */
	s->opt[OPT_LIMIT_RESOLUTION].name = "short-resolution";
	s->opt[OPT_LIMIT_RESOLUTION].title =
		SANE_I18N("Short resolution list");
	s->opt[OPT_LIMIT_RESOLUTION].desc =
		SANE_I18N("Display short resolution list");
	s->opt[OPT_LIMIT_RESOLUTION].type = SANE_TYPE_BOOL;
	s->val[OPT_LIMIT_RESOLUTION].w = SANE_FALSE;


	/* zoom */
	s->opt[OPT_ZOOM].name = "zoom";
	s->opt[OPT_ZOOM].title = SANE_I18N("Zoom");
	s->opt[OPT_ZOOM].desc =
		SANE_I18N("Defines the zoom factor the scanner will use");

	s->opt[OPT_ZOOM].type = SANE_TYPE_INT;
	s->opt[OPT_ZOOM].unit = SANE_UNIT_NONE;
	s->opt[OPT_ZOOM].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_ZOOM].constraint.range = &zoom_range;
	s->val[OPT_ZOOM].w = 100;

/*		if( ! s->hw->cmd->set_zoom) */
	{
		s->opt[OPT_ZOOM].cap |= SANE_CAP_INACTIVE;
	}


	/* "Preview settings" group: */
	s->opt[OPT_PREVIEW_GROUP].title = SANE_TITLE_PREVIEW;
	s->opt[OPT_PREVIEW_GROUP].desc = "";
	s->opt[OPT_PREVIEW_GROUP].type = SANE_TYPE_GROUP;
	s->opt[OPT_PREVIEW_GROUP].cap = SANE_CAP_ADVANCED;

	/* preview */
	s->opt[OPT_PREVIEW].name = SANE_NAME_PREVIEW;
	s->opt[OPT_PREVIEW].title = SANE_TITLE_PREVIEW;
	s->opt[OPT_PREVIEW].desc = SANE_DESC_PREVIEW;

	s->opt[OPT_PREVIEW].type = SANE_TYPE_BOOL;
	s->val[OPT_PREVIEW].w = SANE_FALSE;

	/* "Geometry" group: */
	s->opt[OPT_GEOMETRY_GROUP].title = SANE_I18N("Geometry");
	s->opt[OPT_GEOMETRY_GROUP].desc = "";
	s->opt[OPT_GEOMETRY_GROUP].type = SANE_TYPE_GROUP;
	s->opt[OPT_GEOMETRY_GROUP].cap = SANE_CAP_ADVANCED;

	/* top-left x */
	s->opt[OPT_TL_X].name = SANE_NAME_SCAN_TL_X;
	s->opt[OPT_TL_X].title = SANE_TITLE_SCAN_TL_X;
	s->opt[OPT_TL_X].desc = SANE_DESC_SCAN_TL_X;

	s->opt[OPT_TL_X].type = SANE_TYPE_FIXED;
	s->opt[OPT_TL_X].unit = SANE_UNIT_MM;
	s->opt[OPT_TL_X].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_TL_X].constraint.range = s->hw->x_range;
	s->val[OPT_TL_X].w = 0;

	/* top-left y */
	s->opt[OPT_TL_Y].name = SANE_NAME_SCAN_TL_Y;
	s->opt[OPT_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
	s->opt[OPT_TL_Y].desc = SANE_DESC_SCAN_TL_Y;

	s->opt[OPT_TL_Y].type = SANE_TYPE_FIXED;
	s->opt[OPT_TL_Y].unit = SANE_UNIT_MM;
	s->opt[OPT_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_TL_Y].constraint.range = s->hw->y_range;
	s->val[OPT_TL_Y].w = 0;

	/* bottom-right x */
	s->opt[OPT_BR_X].name = SANE_NAME_SCAN_BR_X;
	s->opt[OPT_BR_X].title = SANE_TITLE_SCAN_BR_X;
	s->opt[OPT_BR_X].desc = SANE_DESC_SCAN_BR_X;

	s->opt[OPT_BR_X].type = SANE_TYPE_FIXED;
	s->opt[OPT_BR_X].unit = SANE_UNIT_MM;
	s->opt[OPT_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_BR_X].constraint.range = s->hw->x_range;
	s->val[OPT_BR_X].w = s->hw->x_range->max;

	/* bottom-right y */
	s->opt[OPT_BR_Y].name = SANE_NAME_SCAN_BR_Y;
	s->opt[OPT_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
	s->opt[OPT_BR_Y].desc = SANE_DESC_SCAN_BR_Y;

	s->opt[OPT_BR_Y].type = SANE_TYPE_FIXED;
	s->opt[OPT_BR_Y].unit = SANE_UNIT_MM;
	s->opt[OPT_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_BR_Y].constraint.range = s->hw->y_range;
	s->val[OPT_BR_Y].w = s->hw->y_range->max;

	/* Quick format */
	s->opt[OPT_QUICK_FORMAT].name = "quick-format";
	s->opt[OPT_QUICK_FORMAT].title = SANE_I18N("Quick format");
	s->opt[OPT_QUICK_FORMAT].desc = "";

	s->opt[OPT_QUICK_FORMAT].type = SANE_TYPE_STRING;
	s->opt[OPT_QUICK_FORMAT].size = max_string_size(qf_list);
	s->opt[OPT_QUICK_FORMAT].cap |= SANE_CAP_ADVANCED;
	s->opt[OPT_QUICK_FORMAT].constraint_type =
		SANE_CONSTRAINT_STRING_LIST;
	s->opt[OPT_QUICK_FORMAT].constraint.string_list = qf_list;
	s->val[OPT_QUICK_FORMAT].w = XtNumber(qf_params) - 1;	/* max */

	/* "Optional equipment" group: */
	s->opt[OPT_EQU_GROUP].title = SANE_I18N("Optional equipment");
	s->opt[OPT_EQU_GROUP].desc = "";
	s->opt[OPT_EQU_GROUP].type = SANE_TYPE_GROUP;
	s->opt[OPT_EQU_GROUP].cap = SANE_CAP_ADVANCED;

	/* source */
	s->opt[OPT_SOURCE].name = SANE_NAME_SCAN_SOURCE;
	s->opt[OPT_SOURCE].title = SANE_TITLE_SCAN_SOURCE;
	s->opt[OPT_SOURCE].desc = SANE_DESC_SCAN_SOURCE;

	s->opt[OPT_SOURCE].type = SANE_TYPE_STRING;
	s->opt[OPT_SOURCE].size = max_string_size(source_list);

	s->opt[OPT_SOURCE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
	s->opt[OPT_SOURCE].constraint.string_list = source_list;

	if (!s->hw->extension) {
		s->opt[OPT_SOURCE].cap |= SANE_CAP_INACTIVE;
	}
	s->val[OPT_SOURCE].w = 0;	/* always use Flatbed as default */


	/* film type */
	s->opt[OPT_FILM_TYPE].name = "film-type";
	s->opt[OPT_FILM_TYPE].title = SANE_I18N("Film type");
	s->opt[OPT_FILM_TYPE].desc = "";
	s->opt[OPT_FILM_TYPE].type = SANE_TYPE_STRING;
	s->opt[OPT_FILM_TYPE].size = max_string_size(film_list);
	s->opt[OPT_FILM_TYPE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
	s->opt[OPT_FILM_TYPE].constraint.string_list = film_list;
	s->val[OPT_FILM_TYPE].w = 0;

	if (!s->hw->cmd->set_bay)
		s->opt[OPT_FILM_TYPE].cap |= SANE_CAP_INACTIVE;

	/* focus position */
	s->opt[OPT_FOCUS].name = SANE_EPSON_FOCUS_NAME;
	s->opt[OPT_FOCUS].title = SANE_EPSON_FOCUS_TITLE;
	s->opt[OPT_FOCUS].desc = SANE_EPSON_FOCUS_DESC;
	s->opt[OPT_FOCUS].type = SANE_TYPE_STRING;
	s->opt[OPT_FOCUS].size = max_string_size(focus_list);
	s->opt[OPT_FOCUS].constraint_type = SANE_CONSTRAINT_STRING_LIST;
	s->opt[OPT_FOCUS].constraint.string_list = focus_list;
	s->val[OPT_FOCUS].w = 0;

	s->opt[OPT_FOCUS].cap |= SANE_CAP_ADVANCED;
	if (s->hw->focusSupport == SANE_TRUE) {
		s->opt[OPT_FOCUS].cap &= ~SANE_CAP_INACTIVE;
	} else {
		s->opt[OPT_FOCUS].cap |= SANE_CAP_INACTIVE;
	}

	/* forward feed / eject */
	s->opt[OPT_EJECT].name = "eject";
	s->opt[OPT_EJECT].title = SANE_I18N("Eject");
	s->opt[OPT_EJECT].desc = SANE_I18N("Eject the sheet in the ADF");
	s->opt[OPT_EJECT].type = SANE_TYPE_BUTTON;

	if ((!s->hw->ADF) && (!s->hw->cmd->set_bay)) {	/* Hack: Using set_bay to indicate. */
		s->opt[OPT_EJECT].cap |= SANE_CAP_INACTIVE;
	}


	/* auto forward feed / eject */
	s->opt[OPT_AUTO_EJECT].name = "auto-eject";
	s->opt[OPT_AUTO_EJECT].title = SANE_I18N("Auto eject");
	s->opt[OPT_AUTO_EJECT].desc =
		SANE_I18N("Eject document after scanning");

	s->opt[OPT_AUTO_EJECT].type = SANE_TYPE_BOOL;
	s->val[OPT_AUTO_EJECT].w = SANE_FALSE;

	if (!s->hw->ADF)
		s->opt[OPT_AUTO_EJECT].cap |= SANE_CAP_INACTIVE;


	s->opt[OPT_ADF_MODE].name = "adf_mode";
	s->opt[OPT_ADF_MODE].title = SANE_I18N("ADF Mode");
	s->opt[OPT_ADF_MODE].desc =
		SANE_I18N("Selects the ADF mode (simplex/duplex)");
	s->opt[OPT_ADF_MODE].type = SANE_TYPE_STRING;
	s->opt[OPT_ADF_MODE].size = max_string_size(adf_mode_list);
	s->opt[OPT_ADF_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
	s->opt[OPT_ADF_MODE].constraint.string_list = adf_mode_list;
	s->val[OPT_ADF_MODE].w = 0;	/* simplex */

	if ((!s->hw->ADF) || (s->hw->duplexSupport == SANE_FALSE))
		s->opt[OPT_ADF_MODE].cap |= SANE_CAP_INACTIVE;

	/* select bay */
	s->opt[OPT_BAY].name = "bay";
	s->opt[OPT_BAY].title = SANE_I18N("Bay");
	s->opt[OPT_BAY].desc = SANE_I18N("Select bay to scan");
	s->opt[OPT_BAY].type = SANE_TYPE_STRING;
	s->opt[OPT_BAY].size = max_string_size(bay_list);
	s->opt[OPT_BAY].constraint_type = SANE_CONSTRAINT_STRING_LIST;
	s->opt[OPT_BAY].constraint.string_list = bay_list;
	s->val[OPT_BAY].w = 0;	/* Bay 1 */

	if (!s->hw->cmd->set_bay)
		s->opt[OPT_BAY].cap |= SANE_CAP_INACTIVE;


	s->opt[OPT_WAIT_FOR_BUTTON].name = SANE_EPSON_WAIT_FOR_BUTTON_NAME;
	s->opt[OPT_WAIT_FOR_BUTTON].title = SANE_EPSON_WAIT_FOR_BUTTON_TITLE;
	s->opt[OPT_WAIT_FOR_BUTTON].desc = SANE_EPSON_WAIT_FOR_BUTTON_DESC;

	s->opt[OPT_WAIT_FOR_BUTTON].type = SANE_TYPE_BOOL;
	s->opt[OPT_WAIT_FOR_BUTTON].unit = SANE_UNIT_NONE;
	s->opt[OPT_WAIT_FOR_BUTTON].constraint_type = SANE_CONSTRAINT_NONE;
	s->opt[OPT_WAIT_FOR_BUTTON].constraint.range = NULL;
	s->opt[OPT_WAIT_FOR_BUTTON].cap |= SANE_CAP_ADVANCED;

	if (!s->hw->cmd->request_push_button_status) {
		s->opt[OPT_WAIT_FOR_BUTTON].cap |= SANE_CAP_INACTIVE;
	}

	return SANE_STATUS_GOOD;
}

SANE_Status
sane_open(SANE_String_Const name, SANE_Handle * handle)
{
	Epson_Device *dev;
	Epson_Scanner *s;

	DBG(5, "%s: name = %s\n", __func__, name);

	/* search for device */
	if (name[0]) {
		for (dev = first_dev; dev; dev = dev->next)
			if (strcmp(dev->sane.name, name) == 0)
				break;
	} else
		dev = first_dev;

	if (!dev) {
		DBG(1, "error opening the device");
		return SANE_STATUS_INVAL;
	}

	s = calloc(sizeof(Epson_Scanner), 1);
	if (!s) {
		DBG(1, "out of memory (line %d)\n", __LINE__);
		return SANE_STATUS_NO_MEM;
	}

	s->fd = -1;
	s->hw = dev;

	init_options(s);

	/* insert newly opened handle into list of open handles */
	s->next = first_handle;
	first_handle = s;

	*handle = (SANE_Handle) s;

	open_scanner(s);

	return SANE_STATUS_GOOD;
}

void
sane_close(SANE_Handle handle)
{
	Epson_Scanner *s, *prev;

	/*
	 * Test if there is still data pending from
	 * the scanner. If so, then do a cancel
	 */
	/* XXX */

	s = (Epson_Scanner *) handle;

	/* remove handle from list of open handles */
	prev = 0;
	for (s = first_handle; s; s = s->next) {
		if (s == handle)
			break;
		prev = s;
	}

	if (!s) {
		DBG(1, "%s: invalid handle (0x%p)\n", __func__, handle);
		return;
	}

	if (prev)
		prev->next = s->next;
	else
		first_handle = s->next;

	if (s->fd != -1)
		close_scanner(s);

	free(s);
}

const SANE_Option_Descriptor *
sane_get_option_descriptor(SANE_Handle handle, SANE_Int option)
{
	Epson_Scanner *s = (Epson_Scanner *) handle;

	if (option < 0 || option >= NUM_OPTIONS)
		return NULL;

	return (s->opt + option);
}

static const SANE_String_Const *
search_string_list(const SANE_String_Const * list, SANE_String value)
{
	while (*list != NULL && strcmp(value, *list) != 0) {
		++list;
	}

	return ((*list == NULL) ? NULL : list);
}

/*
    Activate, deactivate an option.  Subroutines so we can add
    debugging info if we want.  The change flag is set to TRUE
    if we changed an option.  If we did not change an option,
    then the value of the changed flag is not modified.
*/

static void
activateOption(Epson_Scanner * s, SANE_Int option, SANE_Bool * change)
{
	if (!SANE_OPTION_IS_ACTIVE(s->opt[option].cap)) {
		s->opt[option].cap &= ~SANE_CAP_INACTIVE;
		*change = SANE_TRUE;
	}
}

static void
deactivateOption(Epson_Scanner * s, SANE_Int option, SANE_Bool * change)
{
	if (SANE_OPTION_IS_ACTIVE(s->opt[option].cap)) {
		s->opt[option].cap |= SANE_CAP_INACTIVE;
		*change = SANE_TRUE;
	}
}

static void
setOptionState(Epson_Scanner * s, SANE_Bool state, SANE_Int option,
	       SANE_Bool * change)
{
	if (state)
		activateOption(s, option, change);
	else
		deactivateOption(s, option, change);
}

static SANE_Status
getvalue(SANE_Handle handle, SANE_Int option, void *value)
{
	Epson_Scanner *s = (Epson_Scanner *) handle;
	SANE_Option_Descriptor *sopt = &(s->opt[option]);
	Option_Value *sval = &(s->val[option]);

	switch (option) {

/* 	case OPT_GAMMA_VECTOR: */
	case OPT_GAMMA_VECTOR_R:
	case OPT_GAMMA_VECTOR_G:
	case OPT_GAMMA_VECTOR_B:
		memcpy(value, sval->wa, sopt->size);
		break;

	case OPT_NUM_OPTS:
	case OPT_RESOLUTION:
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
	case OPT_MIRROR:
	case OPT_SPEED:
	case OPT_PREVIEW_SPEED:
	case OPT_AAS:
	case OPT_PREVIEW:
	case OPT_BRIGHTNESS:
	case OPT_SHARPNESS:
	case OPT_AUTO_EJECT:
	case OPT_CCT_1:
	case OPT_CCT_2:
	case OPT_CCT_3:
	case OPT_CCT_4:
	case OPT_CCT_5:
	case OPT_CCT_6:
	case OPT_CCT_7:
	case OPT_CCT_8:
	case OPT_CCT_9:
	case OPT_THRESHOLD:
	case OPT_ZOOM:
	case OPT_BIT_DEPTH:
	case OPT_WAIT_FOR_BUTTON:
	case OPT_LIMIT_RESOLUTION:
		*((SANE_Word *) value) = sval->w;
		break;
	case OPT_MODE:
	case OPT_ADF_MODE:
	case OPT_HALFTONE:
	case OPT_DROPOUT:
	case OPT_QUICK_FORMAT:
	case OPT_SOURCE:
	case OPT_FILM_TYPE:
	case OPT_GAMMA_CORRECTION:
	case OPT_COLOR_CORRECTION:
	case OPT_BAY:
	case OPT_FOCUS:
		strcpy((char *) value, sopt->constraint.string_list[sval->w]);
		break;
#if 0
	case OPT_MODEL:
		strcpy(value, sval->s);
		break;
#endif


	default:
		return SANE_STATUS_INVAL;

	}

	return SANE_STATUS_GOOD;
}

/*
 * This routine handles common options between OPT_MODE and
 * OPT_HALFTONE.  These options are TET (a HALFTONE mode), AAS
 * - auto area segmentation, and threshold.  Apparently AAS
 * is some method to differentiate between text and photos.
 * Or something like that.
 *
 * AAS is available when the scan color depth is 1 and the
 * halftone method is not TET.
 *
 * Threshold is available when halftone is NONE, and depth is 1.
 */
static void
handle_depth_halftone(Epson_Scanner * s, SANE_Bool * reload)
{
	int hti = s->val[OPT_HALFTONE].w;
	int mdi = s->val[OPT_MODE].w;
	SANE_Bool aas = SANE_FALSE;
	SANE_Bool thresh = SANE_FALSE;

	if (!s->hw->cmd->control_auto_area_segmentation)
		return;

	if (mode_params[mdi].depth == 1) {
		if (halftone_params[hti] != HALFTONE_TET)
			aas = SANE_TRUE;

		if (halftone_params[hti] == HALFTONE_NONE)
			thresh = SANE_TRUE;
	}
	setOptionState(s, aas, OPT_AAS, reload);
	setOptionState(s, thresh, OPT_THRESHOLD, reload);
}

/*
 * Handles setting the source (flatbed, transparency adapter (TPU),
 * or auto document feeder (ADF)).
 *
 * For newer scanners it also sets the focus according to the
 * glass / TPU settings.
 */

static void
handle_source(Epson_Scanner * s, SANE_Int optindex, char *value)
{
	int force_max = SANE_FALSE;
	SANE_Bool dummy;

	/* reset the scanner when we are changing the source setting -
	   this is necessary for the Perfection 1650 */
	if (s->hw->need_reset_on_source_change)
		reset(s);

	s->focusOnGlass = SANE_TRUE;	/* this is the default */

	if (s->val[OPT_SOURCE].w == optindex)
		return;

	s->val[OPT_SOURCE].w = optindex;

	if (s->val[OPT_TL_X].w == s->hw->x_range->min
	    && s->val[OPT_TL_Y].w == s->hw->y_range->min
	    && s->val[OPT_BR_X].w == s->hw->x_range->max
	    && s->val[OPT_BR_Y].w == s->hw->y_range->max) {
		force_max = SANE_TRUE;
	}

	if (strcmp(ADF_STR, value) == 0) {
		s->hw->x_range = &s->hw->adf_x_range;
		s->hw->y_range = &s->hw->adf_y_range;
		s->hw->use_extension = SANE_TRUE;
		/* disable film type option */
		deactivateOption(s, OPT_FILM_TYPE, &dummy);
		s->val[OPT_FOCUS].w = 0;
		if (s->hw->duplexSupport) {
			activateOption(s, OPT_ADF_MODE, &dummy);
		} else {
			deactivateOption(s, OPT_ADF_MODE, &dummy);
			s->val[OPT_ADF_MODE].w = 0;
		}
	} else if (strcmp(TPU_STR, value) == 0) {
		s->hw->x_range = &s->hw->tpu_x_range;
		s->hw->y_range = &s->hw->tpu_y_range;
		s->hw->use_extension = SANE_TRUE;

		/* enable film type option only if the scanner supports it */
		if (s->hw->cmd->set_film_type != 0)
			activateOption(s, OPT_FILM_TYPE, &dummy);
		else
			deactivateOption(s, OPT_FILM_TYPE, &dummy);

		/* enable focus position if the scanner supports it */
		if (s->hw->cmd->set_focus_position != 0) {
			s->val[OPT_FOCUS].w = 1;
			s->focusOnGlass = SANE_FALSE;
		}

		deactivateOption(s, OPT_ADF_MODE, &dummy);
		deactivateOption(s, OPT_EJECT, &dummy);
		deactivateOption(s, OPT_AUTO_EJECT, &dummy);
	} else {
		/* neither ADF nor TPU active */
		s->hw->x_range = &s->hw->fbf_x_range;
		s->hw->y_range = &s->hw->fbf_y_range;
		s->hw->use_extension = SANE_FALSE;

		/* disable film type option */
		deactivateOption(s, OPT_FILM_TYPE, &dummy);
		s->val[OPT_FOCUS].w = 0;
		deactivateOption(s, OPT_ADF_MODE, &dummy);
	}

	/* special handling for FilmScan 200 */
	if (s->hw->cmd->level[0] == 'F') {
		activateOption(s, OPT_FILM_TYPE, &dummy);
	}

	qf_params[XtNumber(qf_params) - 1].tl_x = s->hw->x_range->min;
	qf_params[XtNumber(qf_params) - 1].tl_y = s->hw->y_range->min;
	qf_params[XtNumber(qf_params) - 1].br_x = s->hw->x_range->max;
	qf_params[XtNumber(qf_params) - 1].br_y = s->hw->y_range->max;

	s->opt[OPT_BR_X].constraint.range = s->hw->x_range;
	s->opt[OPT_BR_Y].constraint.range = s->hw->y_range;

	if (s->val[OPT_TL_X].w < s->hw->x_range->min || force_max)
		s->val[OPT_TL_X].w = s->hw->x_range->min;

	if (s->val[OPT_TL_Y].w < s->hw->y_range->min || force_max)
		s->val[OPT_TL_Y].w = s->hw->y_range->min;

	if (s->val[OPT_BR_X].w > s->hw->x_range->max || force_max)
		s->val[OPT_BR_X].w = s->hw->x_range->max;

	if (s->val[OPT_BR_Y].w > s->hw->y_range->max || force_max)
		s->val[OPT_BR_Y].w = s->hw->y_range->max;

	setOptionState(s, s->hw->ADF
		       && s->hw->use_extension, OPT_AUTO_EJECT, &dummy);
	setOptionState(s, s->hw->ADF
		       && s->hw->use_extension, OPT_EJECT, &dummy);
}

static SANE_Status
setvalue(SANE_Handle handle, SANE_Int option, void *value, SANE_Int * info)
{
	Epson_Scanner *s = (Epson_Scanner *) handle;
	SANE_Option_Descriptor *sopt = &(s->opt[option]);
	Option_Value *sval = &(s->val[option]);

	SANE_Status status;
	const SANE_String_Const *optval;
	int optindex;
	SANE_Bool reload = SANE_FALSE;

	DBG(5, "%s: option = %d, value = %p\n", __func__, option, value);

	status = sanei_constrain_value(sopt, value, info);

	if (status != SANE_STATUS_GOOD)
		return status;

	s->option_has_changed = SANE_TRUE;

	optval = NULL;
	optindex = 0;

	if (sopt->constraint_type == SANE_CONSTRAINT_STRING_LIST) {
		optval = search_string_list(sopt->constraint.string_list,
					    (char *) value);

		if (optval == NULL)
			return SANE_STATUS_INVAL;
		optindex = optval - sopt->constraint.string_list;
	}

	switch (option) {

/* 	case OPT_GAMMA_VECTOR: */
	case OPT_GAMMA_VECTOR_R:
	case OPT_GAMMA_VECTOR_G:
	case OPT_GAMMA_VECTOR_B:
		memcpy(sval->wa, value, sopt->size);	/* Word arrays */
		break;

	case OPT_CCT_1:
	case OPT_CCT_2:
	case OPT_CCT_3:
	case OPT_CCT_4:
	case OPT_CCT_5:
	case OPT_CCT_6:
	case OPT_CCT_7:
	case OPT_CCT_8:
	case OPT_CCT_9:
		sval->w = *((SANE_Word *) value);	/* Simple values */
		break;

	case OPT_DROPOUT:
	case OPT_FILM_TYPE:
	case OPT_BAY:
	case OPT_FOCUS:
		sval->w = optindex;	/* Simple lists */
		break;

	case OPT_EJECT:
		eject(s);
		break;

	case OPT_RESOLUTION:
		sval->w = *((SANE_Word *) value);
		reload = SANE_TRUE;
		break;

	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
		sval->w = *((SANE_Word *) value);
		DBG(1, "set = %f\n", SANE_UNFIX(sval->w));
		if (NULL != info)
			*info |= SANE_INFO_RELOAD_PARAMS;
		break;

	case OPT_SOURCE:
		handle_source(s, optindex, (char *) value);
		reload = SANE_TRUE;
		break;

	case OPT_MODE:
		{
			SANE_Bool isColor = mode_params[optindex].color;
			SANE_Bool userDefined =
				color_userdefined[s->
						  val[OPT_COLOR_CORRECTION].
						  w];

			sval->w = optindex;

			if (s->hw->cmd->set_halftoning != 0)
				setOptionState(s,
					       mode_params[optindex].depth ==
					       1, OPT_HALFTONE, &reload);

			setOptionState(s, !isColor, OPT_DROPOUT, &reload);

			if (s->hw->cmd->set_color_correction)
				setOptionState(s, isColor,
					       OPT_COLOR_CORRECTION, &reload);

			if (s->hw->cmd->set_color_correction_coefficients) {
				setOptionState(s, isColor
					       && userDefined, OPT_CCT_1,
					       &reload);
				setOptionState(s, isColor
					       && userDefined, OPT_CCT_2,
					       &reload);
				setOptionState(s, isColor
					       && userDefined, OPT_CCT_3,
					       &reload);
				setOptionState(s, isColor
					       && userDefined, OPT_CCT_4,
					       &reload);
				setOptionState(s, isColor
					       && userDefined, OPT_CCT_5,
					       &reload);
				setOptionState(s, isColor
					       && userDefined, OPT_CCT_6,
					       &reload);
				setOptionState(s, isColor
					       && userDefined, OPT_CCT_7,
					       &reload);
				setOptionState(s, isColor
					       && userDefined, OPT_CCT_8,
					       &reload);
				setOptionState(s, isColor
					       && userDefined, OPT_CCT_9,
					       &reload);
			}

			/* if binary, then disable the bit depth selection */
			if (optindex == 0) {
				s->opt[OPT_BIT_DEPTH].cap |=
					SANE_CAP_INACTIVE;
			} else {
				if (bitDepthList[0] == 1)
					s->opt[OPT_BIT_DEPTH].cap |=
						SANE_CAP_INACTIVE;
				else {
					s->opt[OPT_BIT_DEPTH].cap &=
						~SANE_CAP_INACTIVE;
					s->val[OPT_BIT_DEPTH].w =
						mode_params[optindex].depth;
				}
			}

			handle_depth_halftone(s, &reload);
			reload = SANE_TRUE;

			break;
		}

	case OPT_ADF_MODE:
		sval->w = optindex;
		break;

	case OPT_BIT_DEPTH:
		sval->w = *((SANE_Word *) value);
		mode_params[s->val[OPT_MODE].w].depth = sval->w;
		reload = SANE_TRUE;
		break;

	case OPT_HALFTONE:
		sval->w = optindex;
		handle_depth_halftone(s, &reload);
		break;

	case OPT_COLOR_CORRECTION:
		{
			SANE_Bool f = color_userdefined[optindex];

			sval->w = optindex;
			setOptionState(s, f, OPT_CCT_1, &reload);
			setOptionState(s, f, OPT_CCT_2, &reload);
			setOptionState(s, f, OPT_CCT_3, &reload);
			setOptionState(s, f, OPT_CCT_4, &reload);
			setOptionState(s, f, OPT_CCT_5, &reload);
			setOptionState(s, f, OPT_CCT_6, &reload);
			setOptionState(s, f, OPT_CCT_7, &reload);
			setOptionState(s, f, OPT_CCT_8, &reload);
			setOptionState(s, f, OPT_CCT_9, &reload);

			break;
		}

	case OPT_GAMMA_CORRECTION:
		{
			SANE_Bool f = gamma_userdefined[optindex];

			sval->w = optindex;

/*		setOptionState(s, f, OPT_GAMMA_VECTOR, &reload ); */
			setOptionState(s, f, OPT_GAMMA_VECTOR_R, &reload);
			setOptionState(s, f, OPT_GAMMA_VECTOR_G, &reload);
			setOptionState(s, f, OPT_GAMMA_VECTOR_B, &reload);
			setOptionState(s, !f, OPT_BRIGHTNESS, &reload);	/* Note... */

			break;
		}

	case OPT_MIRROR:
	case OPT_SPEED:
	case OPT_PREVIEW_SPEED:
	case OPT_AAS:
	case OPT_PREVIEW:	/* needed? */
	case OPT_BRIGHTNESS:
	case OPT_SHARPNESS:
	case OPT_AUTO_EJECT:
	case OPT_THRESHOLD:
	case OPT_ZOOM:
	case OPT_WAIT_FOR_BUTTON:
		sval->w = *((SANE_Word *) value);
		break;

	case OPT_LIMIT_RESOLUTION:
		sval->w = *((SANE_Word *) value);
		filter_resolution_list(s);
		reload = SANE_TRUE;
		break;

	case OPT_QUICK_FORMAT:
		sval->w = optindex;

		s->val[OPT_TL_X].w = qf_params[sval->w].tl_x;
		s->val[OPT_TL_Y].w = qf_params[sval->w].tl_y;
		s->val[OPT_BR_X].w = qf_params[sval->w].br_x;
		s->val[OPT_BR_Y].w = qf_params[sval->w].br_y;

		if (s->val[OPT_TL_X].w < s->hw->x_range->min)
			s->val[OPT_TL_X].w = s->hw->x_range->min;

		if (s->val[OPT_TL_Y].w < s->hw->y_range->min)
			s->val[OPT_TL_Y].w = s->hw->y_range->min;

		if (s->val[OPT_BR_X].w > s->hw->x_range->max)
			s->val[OPT_BR_X].w = s->hw->x_range->max;

		if (s->val[OPT_BR_Y].w > s->hw->y_range->max)
			s->val[OPT_BR_Y].w = s->hw->y_range->max;

		reload = SANE_TRUE;
		break;

	default:
		return SANE_STATUS_INVAL;
	}

	if (reload && info != NULL)
		*info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;

	return SANE_STATUS_GOOD;
}

SANE_Status
sane_control_option(SANE_Handle handle, SANE_Int option, SANE_Action action,
		    void *value, SANE_Int * info)
{
	if (option < 0 || option >= NUM_OPTIONS)
		return SANE_STATUS_INVAL;

	if (info != NULL)
		*info = 0;

	switch (action) {
	case SANE_ACTION_GET_VALUE:
		return getvalue(handle, option, value);

	case SANE_ACTION_SET_VALUE:
		return setvalue(handle, option, value, info);

	default:
		return SANE_STATUS_INVAL;
	}

	return SANE_STATUS_GOOD;
}

static SANE_Status
epson2_set_extended_scanning_parameters(Epson_Scanner * s)
{
	unsigned char buf[64];

	const struct mode_param *mparam;

	DBG(1, "%s\n", __func__);

	mparam = &mode_params[s->val[OPT_MODE].w];

	memset(buf, 0x00, sizeof(buf));

	/* ESC R, resolution */
	htole32a(&buf[0], s->val[OPT_RESOLUTION].w);
	htole32a(&buf[4], s->val[OPT_RESOLUTION].w);
/*
	*((__le32 *) & buf[0]) = htole32(s->val[OPT_RESOLUTION].w);
	*((__le32 *) & buf[4]) = htole32(s->val[OPT_RESOLUTION].w);
		__cpu_to_le32(s->val[OPT_RESOLUTION].w);
*/
	/* ESC A, scanning area */
	htole32a(&buf[8], s->left);
	htole32a(&buf[12], s->top);
	htole32a(&buf[16], s->params.pixels_per_line);
	htole32a(&buf[20], s->params.lines);
/*
	*((__le32 *) & buf[8]) = htole32(s->left);
	*((__le32 *) & buf[12]) = htole32(s->top);
	*((__le32 *) & buf[16]) = htole32(s->params.pixels_per_line);
	*((__le32 *) & buf[20]) = htole32(s->params.lines);
*/
	/*
	 * The byte sequence mode was introduced in B5,
	 *for B[34] we need line sequence mode
	 */

	/* ESC C */
	if ((s->hw->cmd->level[0] == 'D'
	     || (s->hw->cmd->level[0] == 'B' && s->hw->level >= 5))
	    && mparam->flags == 0x02) {
		buf[24] = 0x13;
	} else {
		buf[24] = mparam->flags | (mparam->dropout_mask
					   & dropout_params[s->
							    val[OPT_DROPOUT].
							    w]);
	}

	/* ESC D */
	mparam = &mode_params[s->val[OPT_MODE].w];
	buf[25] = mparam->depth;

	if (s->hw->extension) {

		char extensionCtrl;
		extensionCtrl = (s->hw->use_extension ? 1 : 0);
		if (s->hw->use_extension && (s->val[OPT_ADF_MODE].w == 1))
			extensionCtrl = 2;

		/* ESC e */
		buf[26] = extensionCtrl;

		/* XXX focus */
	}

	/* ESC g, scanning mode */
	if (SANE_OPTION_IS_ACTIVE(s->opt[OPT_SPEED].cap)) {

		if (s->val[OPT_PREVIEW].w)
			buf[27] = speed_params[s->val[OPT_PREVIEW_SPEED].w];
		else
			buf[27] = speed_params[s->val[OPT_SPEED].w];
	}

	/* ESC d, blok line number */
	buf[28] = s->lcount;

	/* ESC Z */
	buf[29] = 0x01;		/* default */

	if (SANE_OPTION_IS_ACTIVE(s->opt[OPT_GAMMA_CORRECTION].cap)) {
		char val;
		if (s->hw->cmd->level[0] == 'D') {
			/* The D1 level has only the two user defined gamma
			 * settings.
			 */
			val = gamma_params[s->val[OPT_GAMMA_CORRECTION].w];
		} else {
			val = gamma_params[s->val[OPT_GAMMA_CORRECTION].w];

			/*
			 * If "Default" is selected then determine the actual value
			 * to send to the scanner: If bilevel mode, just send the
			 * value from the table (0x01), for grayscale or color mode
			 * add one and send 0x02.
			 */

			if (s->val[OPT_GAMMA_CORRECTION].w == 0) {
				val += mparam->depth == 1 ? 0 : 1;
			}
		}

		buf[29] = val;
	}

	/* ESC L, brightness */
	if (SANE_OPTION_IS_ACTIVE(s->opt[OPT_BRIGHTNESS].cap))
		buf[30] = s->val[OPT_BRIGHTNESS].w;

	/* ESC B, halftone processing */
	if (SANE_OPTION_IS_ACTIVE(s->opt[OPT_HALFTONE].cap))
		buf[32] = halftone_params[s->val[OPT_HALFTONE].w];

	/* ESC s, auto area segmentation */
	if (SANE_OPTION_IS_ACTIVE(s->opt[OPT_AAS].cap))
		buf[34] = speed_params[s->val[OPT_AAS].w];

	/* ESC Q, sharpness control */
	if (SANE_OPTION_IS_ACTIVE(s->opt[OPT_SHARPNESS].cap))
		buf[35] = s->val[OPT_SHARPNESS].w;

	/* ESC K, mirroring */
	if (SANE_OPTION_IS_ACTIVE(s->opt[OPT_MIRROR].cap))
		buf[36] = mirror_params[s->val[OPT_MIRROR].w];

	s->invert_image = SANE_FALSE;	/* default: to not inverting the image */

	/* ESC N, film type */
	if (SANE_OPTION_IS_ACTIVE(s->opt[OPT_FILM_TYPE].cap)) {
		s->invert_image =
			(s->val[OPT_FILM_TYPE].w == FILM_TYPE_NEGATIVE);
		buf[37] = film_params[s->val[OPT_FILM_TYPE].w];
	}

	/* ESC M, color correction */
	buf[31] = color_params[s->val[OPT_COLOR_CORRECTION].w];

	/* ESC t, threshold */
	buf[33] = s->val[OPT_THRESHOLD].w;

	return set_scanning_parameter(s, buf);
}

static SANE_Status
epson2_set_scanning_parameters(Epson_Scanner * s)
{
	SANE_Status status;
	struct mode_param *mparam;

	DBG(1, "%s\n", __func__);

	/*
	 *  There is some undocumented special behavior with the TPU enable/disable.
	 *      TPU power       ESC e           status
	 *      on              0               NAK
	 *      on              1               ACK
	 *      off             0               ACK
	 *      off             1               NAK
	 *
	 * It makes no sense to scan with TPU powered on and source flatbed, because
	 * light will come from both sides.
	 */

	if (s->hw->extension) {

		int extensionCtrl;
		extensionCtrl = (s->hw->use_extension ? 1 : 0);
		if (s->hw->use_extension && (s->val[OPT_ADF_MODE].w == 1))
			extensionCtrl = 2;

		status = control_extension(s, extensionCtrl);
		if (status != SANE_STATUS_GOOD) {
			DBG(1, "you may have to power %s your TPU\n",
			    s->hw->use_extension ? "on" : "off");
			DBG(1,
			    "and you may also have to restart the SANE frontend.\n");
			close_scanner(s);
			return status;
		}

		/* XXX use request_extended_status and analyze
		 * buffer to set the scan area for
		 * ES-9000H and GT-30000
		 */

		if (s->hw->ADF && s->hw->use_extension && s->hw->cmd->feed) {
			status = feed(s);
			if (status != SANE_STATUS_GOOD) {
				close_scanner(s);
				return status;
			}
		}

		/*
		 * set the focus position according to the extension used:
		 * if the TPU is selected, then focus 2.5mm above the glass,
		 * otherwise focus on the glass. Scanners that don't support
		 * this feature, will just ignore these calls.
		 */

		if (s->hw->focusSupport == SANE_TRUE) {
			if (s->val[OPT_FOCUS].w == 0) {
				DBG(1, "setting focus to glass surface\n");
				set_focus_position(s, 0x40);
			} else {
				DBG(1,
				    "setting focus to 2.5mm above glass\n");
				set_focus_position(s, 0x59);
			}
		}
	}

	mparam = &mode_params[s->val[OPT_MODE].w];
	DBG(1, "%s: setting data format to %d bits\n", __func__,
	    mparam->depth);
	status = set_data_format(s, mparam->depth);
	if (status != SANE_STATUS_GOOD)
		return status;

	/*
	 * The byte sequence mode was introduced in B5, for B[34] we need line sequence mode
	 */

	if ((s->hw->cmd->level[0] == 'D'
	     || (s->hw->cmd->level[0] == 'B' && s->hw->level >= 5))
	    && mparam->flags == 0x02) {
		status = set_color_mode(s, 0x13);
	} else {
		status = set_color_mode(s, mparam->flags
					| (mparam->
					   dropout_mask & dropout_params[s->
									 val
									 [OPT_DROPOUT].
									 w]));
	}

	if (status != SANE_STATUS_GOOD)
		return status;

	if (s->hw->cmd->set_halftoning
	    && SANE_OPTION_IS_ACTIVE(s->opt[OPT_HALFTONE].cap)) {
		status = set_halftoning(s,
					halftone_params[s->val[OPT_HALFTONE].
							w]);
		if (status != SANE_STATUS_GOOD)
			return status;
	}

	if (SANE_OPTION_IS_ACTIVE(s->opt[OPT_BRIGHTNESS].cap)) {
		status = set_bright(s, s->val[OPT_BRIGHTNESS].w);
		if (status != SANE_STATUS_GOOD)
			return status;
	}

	if (SANE_OPTION_IS_ACTIVE(s->opt[OPT_MIRROR].cap)) {
		status = mirror_image(s, mirror_params[s->val[OPT_MIRROR].w]);
		if (status != SANE_STATUS_GOOD)
			return status;
	}

	/* ESC g, set scanning mode */
	if (SANE_OPTION_IS_ACTIVE(s->opt[OPT_SPEED].cap)) {

		if (s->val[OPT_PREVIEW].w)
			status = set_speed(s,
					   speed_params[s->
							val
							[OPT_PREVIEW_SPEED].
							w]);
		else
			status = set_speed(s,
					   speed_params[s->val[OPT_SPEED].w]);

		if (status != SANE_STATUS_GOOD)
			return status;
	}

	/*
	 *  use of speed_params is ok here since they are false and true.
	 *  NOTE: I think I should throw that "params" stuff as long w is already the value.
	 */

	if (SANE_OPTION_IS_ACTIVE(s->opt[OPT_AAS].cap)) {
		status = set_auto_area_segmentation(s,
						    speed_params[s->
								 val[OPT_AAS].
								 w]);
		if (status != SANE_STATUS_GOOD)
			return status;
	}

	s->invert_image = SANE_FALSE;	/* default: to not inverting the image */

	if (SANE_OPTION_IS_ACTIVE(s->opt[OPT_FILM_TYPE].cap)) {
		s->invert_image =
			(s->val[OPT_FILM_TYPE].w == FILM_TYPE_NEGATIVE);
		status = set_film_type(s,
				       film_params[s->val[OPT_FILM_TYPE].w]);
		if (status != SANE_STATUS_GOOD)
			return status;
	}

	if (SANE_OPTION_IS_ACTIVE(s->opt[OPT_BAY].cap)) {
		status = set_bay(s, s->val[OPT_BAY].w);
		if (status != SANE_STATUS_GOOD)
			return status;
	}

	if (SANE_OPTION_IS_ACTIVE(s->opt[OPT_SHARPNESS].cap)) {

		status = set_sharpness(s, s->val[OPT_SHARPNESS].w);
		if (status != SANE_STATUS_GOOD)
			return status;
	}

	if (s->hw->cmd->set_gamma
	    && SANE_OPTION_IS_ACTIVE(s->opt[OPT_GAMMA_CORRECTION].cap)) {
		int val;
		if (s->hw->cmd->level[0] == 'D') {
			/*
			 * The D1 level has only the two user defined gamma
			 * settings.
			 */
			val = gamma_params[s->val[OPT_GAMMA_CORRECTION].w];
		} else {
			val = gamma_params[s->val[OPT_GAMMA_CORRECTION].w];

			/*
			 * If "Default" is selected then determine the actual value
			 * to send to the scanner: If bilevel mode, just send the
			 * value from the table (0x01), for grayscale or color mode
			 * add one and send 0x02.
			 */

/*			if( s->val[ OPT_GAMMA_CORRECTION].w <= 1) { */
			if (s->val[OPT_GAMMA_CORRECTION].w == 0) {
				val += mparam->depth == 1 ? 0 : 1;
			}
		}

		status = set_gamma(s, val);
		if (status != SANE_STATUS_GOOD)
			return status;
	}

	/* XXX? */
	if (s->hw->cmd->set_gamma_table && gamma_userdefined[s->val[OPT_GAMMA_CORRECTION].w]) {	/* user defined. */
		status = set_gamma_table(s);

		if (status != SANE_STATUS_GOOD)
			return status;
	}

	/*
	 * TODO: think about if SANE_OPTION_IS_ACTIVE is a good criteria to send commands.
	 */

	if (SANE_OPTION_IS_ACTIVE(s->opt[OPT_COLOR_CORRECTION].cap)) {
		status = set_color_correction(s,
					      color_params[s->
							   val
							   [OPT_COLOR_CORRECTION].
							   w]);
		if (status != SANE_STATUS_GOOD)
			return status;
	}

	if (s->hw->cmd->set_threshold != 0
	    && SANE_OPTION_IS_ACTIVE(s->opt[OPT_THRESHOLD].cap)) {
		status = set_threshold(s, s->val[OPT_THRESHOLD].w);
		if (status != SANE_STATUS_GOOD)
			return status;
	}

	/* ESC d, block line number */
	status = set_lcount(s, s->lcount);
	if (status != SANE_STATUS_GOOD)
		return status;

	/* ESC R */
	status = set_resolution(s, s->val[OPT_RESOLUTION].w,
				s->val[OPT_RESOLUTION].w);
	if (status != SANE_STATUS_GOOD)
		return status;

	/* ESC A, scanning area. Should be the last command. */
	status = set_scan_area(s, s->left, s->top,
			       s->params.pixels_per_line, s->params.lines);

	if (status != SANE_STATUS_GOOD)
		return status;

	return SANE_STATUS_GOOD;
}

/*
 * This function is part of the SANE API and gets called when the front end
 * requests information aobut the scan configuration (e.g. color depth, mode,
 * bytes and pixels per line, number of lines. This information is returned
 * in the SANE_Parameters structure.
 *
 * Once a scan was started, this routine has to report the correct values, if
 * it is called before the scan is actually started, the values are based on
 * the current settings.
 */

SANE_Status
sane_get_parameters(SANE_Handle handle, SANE_Parameters * params)
{
	Epson_Scanner *s = (Epson_Scanner *) handle;
	int dpi, max_x, max_y;
	int bytes_per_pixel;

	DBG(5, "%s\n", __func__);

	/*
	 * If sane_start was already called, then just retrieve the parameters
	 * from the scanner data structure
	 */

	if (!s->eof && s->ptr != NULL) {
		DBG(5, "returning saved params structure\n");

		if (params != NULL) {
			DBG(1,
			    "restoring parameters from saved parameters\n");
			*params = s->params;
		}

		DBG(3, "resolution = %d, preview = %d\n",
		    s->val[OPT_RESOLUTION].w, s->val[OPT_PREVIEW].w);

		DBG(1, "get para tlx %f tly %f brx %f bry %f [mm]\n",
		    SANE_UNFIX(s->val[OPT_TL_X].w),
		    SANE_UNFIX(s->val[OPT_TL_Y].w),
		    SANE_UNFIX(s->val[OPT_BR_X].w),
		    SANE_UNFIX(s->val[OPT_BR_Y].w));

		print_params(s->params);

		return SANE_STATUS_GOOD;
	}

	/* otherwise initialize the params structure and gather the data */

	memset(&s->params, 0, sizeof(SANE_Parameters));

	dpi = s->val[OPT_RESOLUTION].w;

	max_x = max_y = 0;

	/* XXX check this */
	s->params.pixels_per_line =
		SANE_UNFIX(s->val[OPT_BR_X].w -
			   s->val[OPT_TL_X].w) / MM_PER_INCH * dpi + 0.5;
	s->params.lines =
		SANE_UNFIX(s->val[OPT_BR_Y].w -
			   s->val[OPT_TL_Y].w) / MM_PER_INCH * dpi + 0.5;

	/*
	 * Make sure that the number of lines is correct for color shuffling:
	 * The shuffling alghorithm produces 2xline_distance lines at the
	 * beginning and the same amount at the end of the scan that are not
	 * useable. If s->params.lines gets negative, 0 lines are reported
	 * back to the frontend.
	 */
	if (s->hw->color_shuffle) {
		s->params.lines -= 4 * s->line_distance;
		if (s->params.lines < 0) {
			s->params.lines = 0;
		}
		DBG(1,
		    "adjusted params.lines for color_shuffle by %d to %d\n",
		    4 * s->line_distance, s->params.lines);
	}

	DBG(3, "resolution = %d, preview = %d\n",
	    s->val[OPT_RESOLUTION].w, s->val[OPT_PREVIEW].w);

	DBG(1, "get para %p %p tlx %f tly %f brx %f bry %f [mm]\n",
	    (void *) s, (void *) s->val, SANE_UNFIX(s->val[OPT_TL_X].w),
	    SANE_UNFIX(s->val[OPT_TL_Y].w), SANE_UNFIX(s->val[OPT_BR_X].w),
	    SANE_UNFIX(s->val[OPT_BR_Y].w));


	/*
	 * Calculate bytes_per_pixel and bytes_per_line for
	 * any color depths.
	 *
	 * The default color depth is stored in mode_params.depth:
	 */

	if (mode_params[s->val[OPT_MODE].w].depth == 1)
		s->params.depth = 1;
	else
		s->params.depth = s->val[OPT_BIT_DEPTH].w;

	if (s->params.depth > 8) {
		s->params.depth = 16;	/*
					 * The frontends can only handle 8 or 16 bits
					 * for gray or color - so if it's more than 8,
					 * it gets automatically set to 16. This works
					 * as long as EPSON does not come out with a
					 * scanner that can handle more than 16 bits
					 * per color channel.
					 */

	}

	bytes_per_pixel = s->params.depth / 8;	/* this works because it can only be set to 1, 8 or 16 */
	if (s->params.depth % 8) {	/* just in case ... */
		bytes_per_pixel++;
	}

	/* pixels_per_line is rounded to the next 8bit boundary */
	s->params.pixels_per_line = s->params.pixels_per_line & ~7;

	s->params.last_frame = SANE_TRUE;

	if (mode_params[s->val[OPT_MODE].w].color) {
		s->params.format = SANE_FRAME_RGB;
		s->params.bytes_per_line =
			3 * s->params.pixels_per_line * bytes_per_pixel;
	} else {
		s->params.format = SANE_FRAME_GRAY;
		s->params.bytes_per_line =
			s->params.pixels_per_line * s->params.depth / 8;
	}

	if (NULL != params)
		*params = s->params;

	print_params(s->params);

	return SANE_STATUS_GOOD;
}

static SANE_Status
epson2_init_parameters(Epson_Scanner * s)
{
	int dpi, max_y, max_x, bytes_per_pixel;
	struct mode_param *mparam;

	memset(&s->params, 0, sizeof(SANE_Parameters));

	dpi = s->val[OPT_RESOLUTION].w;
	max_x = max_y = 0;
	mparam = &mode_params[s->val[OPT_MODE].w];

	s->left =
		SANE_UNFIX(s->val[OPT_TL_X].w) / MM_PER_INCH *
		s->val[OPT_RESOLUTION].w + 0.5, s->top =
		SANE_UNFIX(s->val[OPT_TL_Y].w) / MM_PER_INCH *
		s->val[OPT_RESOLUTION].w + 0.5,
		/* XXX check this */
		s->params.pixels_per_line =
		SANE_UNFIX(s->val[OPT_BR_X].w -
			   s->val[OPT_TL_X].w) / MM_PER_INCH * dpi + 0.5;
	s->params.lines =
		SANE_UNFIX(s->val[OPT_BR_Y].w -
			   s->val[OPT_TL_Y].w) / MM_PER_INCH * dpi + 0.5;

	/*
	 * Make sure that the number of lines is correct for color shuffling:
	 * The shuffling alghorithm produces 2xline_distance lines at the
	 * beginning and the same amount at the end of the scan that are not
	 * useable. If s->params.lines gets negative, 0 lines are reported
	 * back to the frontend.
	 */
	if (s->hw->color_shuffle) {
		s->params.lines -= 4 * s->line_distance;
		if (s->params.lines < 0) {
			s->params.lines = 0;
		}
		DBG(1,
		    "adjusted params.lines for color_shuffle by %d to %d\n",
		    4 * s->line_distance, s->params.lines);
	}

	DBG(1, "%s: %p %p tlx %f tly %f brx %f bry %f [mm]\n",
	    __func__,
	    (void *) s,
	    (void *) s->val, SANE_UNFIX(s->val[OPT_TL_X].w),
	    SANE_UNFIX(s->val[OPT_TL_Y].w), SANE_UNFIX(s->val[OPT_BR_X].w),
	    SANE_UNFIX(s->val[OPT_BR_Y].w));


	/*
	 * Calculate bytes_per_pixel and bytes_per_line for
	 * any color depths.
	 *
	 * The default color depth is stored in mode_params.depth:
	 */

	if (mode_params[s->val[OPT_MODE].w].depth == 1)
		s->params.depth = 1;
	else
		s->params.depth = s->val[OPT_BIT_DEPTH].w;

	if (s->params.depth > 8) {
		s->params.depth = 16;	/*
					 * The frontends can only handle 8 or 16 bits
					 * for gray or color - so if it's more than 8,
					 * it gets automatically set to 16. This works
					 * as long as EPSON does not come out with a
					 * scanner that can handle more than 16 bits
					 * per color channel.
					 */

	}

	bytes_per_pixel = s->params.depth / 8;	/* this works because it can only be set to 1, 8 or 16 */
	if (s->params.depth % 8) {	/* just in case ... */
		bytes_per_pixel++;
	}

	/* pixels_per_line is rounded to the next 8bit boundary */
	s->params.pixels_per_line = s->params.pixels_per_line & ~7;

	s->params.last_frame = SANE_TRUE;

	if (mode_params[s->val[OPT_MODE].w].color) {
		s->params.format = SANE_FRAME_RGB;
		s->params.bytes_per_line =
			3 * s->params.pixels_per_line * bytes_per_pixel;
	} else {
		s->params.format = SANE_FRAME_GRAY;
		s->params.bytes_per_line =
			s->params.pixels_per_line * s->params.depth / 8;
	}

	/*
	 * Calculate correction for line_distance in D1 scanner:
	 * Start line_distance lines earlier and add line_distance lines at the end
	 *
	 * Because the actual line_distance is not yet calculated we have to do this
	 * first.
	 */

	s->hw->color_shuffle = SANE_FALSE;
	s->current_output_line = 0;
	s->lines_written = 0;
	s->color_shuffle_line = 0;

	if ((s->hw->optical_res != 0) && (mparam->depth == 8)
	    && (mparam->flags != 0)) {
		s->line_distance =
			s->hw->max_line_distance * dpi / s->hw->optical_res;
		if (s->line_distance != 0) {
			s->hw->color_shuffle = SANE_TRUE;
		} else
			s->hw->color_shuffle = SANE_FALSE;
	}

	/*
	 * Modify the scan area: If the scanner requires color shuffling, then we try to
	 * scan more lines to compensate for the lines that will be removed from the scan
	 * due to the color shuffling alghorithm.
	 * At this time we add two times the line distance to the number of scan lines if
	 * this is possible - if not, then we try to calculate the number of additional
	 * lines according to the selected scan area.
	 */

	if (s->hw->color_shuffle == SANE_TRUE) {

		/* start the scan 2*line_distance earlier */
		s->top -= 2 * s->line_distance;
		if (s->top < 0) {
			s->top = 0;
		}

		/* scan 4*line_distance lines more */
		s->params.lines += 4 * s->line_distance;
	}

	/*
	 * If (s->top + s->params.lines) is larger than the max scan area, reset
	 * the number of scan lines:
	 */
	if (SANE_UNFIX(s->val[OPT_BR_Y].w) / MM_PER_INCH * dpi <
	    (s->params.lines + s->top)) {
		s->params.lines =
			((int) SANE_UNFIX(s->val[OPT_BR_Y].w) / MM_PER_INCH *
			 dpi + 0.5) - s->top;
	}

	s->block = SANE_FALSE;
	s->lcount = 1;

	/*
	 * The set line count commands needs to be sent for certain scanners in
	 * color mode. The D1 level requires it, we are however only testing for
	 * 'D' and not for the actual numeric level.
	 */
	if (((s->hw->cmd->level[0] == 'B')
	     && ((s->hw->level >= 5)
		 || ((s->hw->level >= 4)
		     && (!mode_params[s->val[OPT_MODE].w].color))))
	    || (s->hw->cmd->level[0] == 'D')) {
		s->block = SANE_TRUE;
		s->lcount =
			sanei_scsi_max_request_size /
			s->params.bytes_per_line;

		if (s->lcount >= 255) {
			s->lcount = 255;
		}

		if (s->hw->TPU && s->hw->use_extension && s->lcount > 32) {
			s->lcount = 32;
		}

		/*
		 * The D1 series of scanners only allow an even line number
		 * for bi-level scanning. If a bit depth of 1 is selected, then
		 * make sure the next lower even number is selected.
		 */
		if (s->hw->cmd->level[0] == 'D') {
			if (s->lcount % 2) {
				s->lcount -= 1;
			}
		}

		if (s->lcount == 0) {
			DBG(1, "%s: this shouldn't happen", __func__);	/* XXX ??? */
			return SANE_STATUS_INVAL;
		}
	}

	print_params(s->params);

	return SANE_STATUS_GOOD;
}

static void
epson2_wait_for_button(Epson_Scanner * s)
{
	s->hw->wait_for_button = SANE_TRUE;

	while (s->hw->wait_for_button == SANE_TRUE) {
		unsigned char button_status = 0;

		if (s->canceling == SANE_TRUE) {
			s->hw->wait_for_button = SANE_FALSE;
		}
		/* get the button status from the scanner */
		else if (request_push_button_status(s, &button_status) ==
			 SANE_STATUS_GOOD) {
			if (button_status)
				s->hw->wait_for_button = SANE_FALSE;
			else
				sleep(1);
		} else {
			/* we run into an error condition, just continue */
			s->hw->wait_for_button = SANE_FALSE;
		}
	}
}

/*
 * This function is part of the SANE API and gets called from the front end to
 * start the scan process.
 */

SANE_Status
sane_start(SANE_Handle handle)
{
	Epson_Scanner *s = (Epson_Scanner *) handle;
	SANE_Status status;
	unsigned char params[4];
	int i, j;		/* loop counter */

	DBG(5, "%s\n", __func__);

	/* XXX check return code? */
	open_scanner(s);

	/* calc scanning parameters */
	epson2_init_parameters(s);

	/* ESC , bay */
	if (SANE_OPTION_IS_ACTIVE(s->opt[OPT_BAY].cap)) {
		status = set_bay(s, s->val[OPT_BAY].w);
		if (status != SANE_STATUS_GOOD)
			return status;
	}

	/* ESC H, set zoom */
	if (s->hw->cmd->set_zoom != 0
	    && SANE_OPTION_IS_ACTIVE(s->opt[OPT_ZOOM].cap)) {
		status = set_zoom(s, s->val[OPT_ZOOM].w, s->val[OPT_ZOOM].w);
		if (status != SANE_STATUS_GOOD)
			return status;
	}

	/* set scanning parameters */
	if (s->hw->extended_commands)
		status = epson2_set_extended_scanning_parameters(s);
	else
		status = epson2_set_scanning_parameters(s);

	if (status != SANE_STATUS_GOOD)
		return status;

	/* ESC z, user defined gamma table */
	if (s->hw->cmd->set_gamma_table
	    && gamma_userdefined[s->val[OPT_GAMMA_CORRECTION].w]) {
		status = set_gamma_table(s);
		if (status != SANE_STATUS_GOOD)
			return status;
	}

	/* ESC m, user defined color correction */
	if (s->val[OPT_COLOR_CORRECTION].w == 1) {
		status = set_color_correction_coefficients(s);
		if (status != SANE_STATUS_GOOD)
			return status;
	}

	status = sane_get_parameters(handle, NULL);
	if (status != SANE_STATUS_GOOD)
		return status;

	/*
	 * If WAIT_FOR_BUTTON is active, then do just that:
	 * Wait until the button is pressed. If the button was already
	 * pressed, then we will get the button pressed event right away.
	 */
	if (s->val[OPT_WAIT_FOR_BUTTON].w == SANE_TRUE)
		epson2_wait_for_button(s);

	s->block = SANE_FALSE;
	s->lcount = 1;

	/*
	 * The set line count commands needs to be sent for certain scanners in
	 * color mode. The D1 level requires it, we are however only testing for
	 * 'D' and not for the actual numeric level.
	 */
	if (((s->hw->cmd->level[0] == 'B')
	     && ((s->hw->level >= 5)
		 || ((s->hw->level >= 4)
		     && (!mode_params[s->val[OPT_MODE].w].color))))
	    || (s->hw->cmd->level[0] == 'D')) {
		s->block = SANE_TRUE;
		s->lcount =
			sanei_scsi_max_request_size /
			s->params.bytes_per_line;

		if (s->lcount >= 255) {
			s->lcount = 255;
		}

		if (s->hw->TPU && s->hw->use_extension && s->lcount > 32) {
			s->lcount = 32;
		}

		/*
		 * The D1 series of scanners only allow an even line number
		 * for bi-level scanning. If a bit depth of 1 is selected, then
		 * make sure the next lower even number is selected.
		 */
		if (s->hw->cmd->level[0] == 'D') {
			if (s->lcount % 2) {
				s->lcount -= 1;
			}
		}

		if (s->lcount == 0) {
			DBG(1, "out of memory (line %d)\n", __LINE__);	/* XXX ??? */
			return SANE_STATUS_NO_MEM;
		}

		status = set_lcount(s, s->lcount);
		if (status != SANE_STATUS_GOOD)
			return status;
	}

	/* XXX do we need the extended status to check for STATUS_FER? */
	if (s->hw->cmd->request_extended_status != 0 && s->hw->extension) {
		unsigned char *ext_status;

		status = request_extended_status(s, &ext_status, NULL);
		if (status != SANE_STATUS_GOOD)
			return status;

		if (ext_status[0] & STATUS_FER) {
			close_scanner(s);
			return SANE_STATUS_INVAL;
		}

		free(ext_status);
	}

	/* for debug, request command parameter */
	if (DBG_LEVEL) {
		unsigned char buf[45];
		request_command_parameter(s, buf);
	}

	/* set the retry count to 0 */
	s->retry_count = 0;

	if (s->hw->color_shuffle == SANE_TRUE) {

		/* initialize the line buffers */
		for (i = 0; i < s->line_distance * 2 + 1; i++) {
			if (s->line_buffer[i] != NULL)
				free(s->line_buffer[i]);

			s->line_buffer[i] = malloc(s->params.bytes_per_line);
			if (s->line_buffer[i] == NULL) {
				/* free the memory we've malloced so far */
				for (j = 0; j < i; j++) {
					free(s->line_buffer[j]);
					s->line_buffer[j] = NULL;
				}
				DBG(1, "out of memory (line %d)\n", __LINE__);
				return SANE_STATUS_NO_MEM;
			}
		}
	}

	/* ESC g */
	params[0] = ESC;
	params[1] = s->hw->cmd->start_scanning;

	/* XXX check that */
	epson_send(s, params, 2, 0, &status);

	if (status != SANE_STATUS_GOOD) {
		DBG(1, "%s: start failed: %s\n", __func__,
		    sane_strstatus(status));
		return status;
	}

	s->eof = SANE_FALSE;
	s->buf = realloc(s->buf, s->lcount * s->params.bytes_per_line);
	s->ptr = s->end = s->buf;
	s->canceling = SANE_FALSE;

	return SANE_STATUS_GOOD;
}

SANE_Status
sane_auto_eject(Epson_Scanner * s)
{
	DBG(5, "%s\n", __func__);

	/* sequence! */
	if (s->hw->ADF && s->hw->use_extension && s->val[OPT_AUTO_EJECT].w)
		return eject(s);

	return SANE_STATUS_GOOD;
}

/* XXX this routine is ugly and should be avoided */
static SANE_Status
read_info_block(Epson_Scanner * s, EpsonDataRec * result)
{
	SANE_Status status;
	unsigned char params[2];

      retry:
	epson_recv(s, result, s->block ? 6 : 4, &status);
	if (status != SANE_STATUS_GOOD)
		return status;

	if (result->code != STX) {
		DBG(1, "code %02x\n", (int) result->code);
		DBG(1, "error, expected STX\n");
		return SANE_STATUS_INVAL;
	}

	/* XXX */
	if (result->status & STATUS_FER) {
		unsigned char *ext_status;

		DBG(1, "fatal error, status = %02x\n", result->status);

		if (s->retry_count > SANE_EPSON_MAX_RETRIES) {
			DBG(1, "max retry count exceeded (%d)\n",
			    s->retry_count);
			return SANE_STATUS_INVAL;
		}

		/* if the scanner is warming up, retry after a few secs */
		status = request_extended_status(s, &ext_status, NULL);
		if (status != SANE_STATUS_GOOD)
			return status;

		if (ext_status[0] & EXT_STATUS_WU) {
			free(ext_status);

			sleep(5);	/* for the next attempt */

			DBG(1, "retrying ESC G - %d\n", ++(s->retry_count));

			params[0] = ESC;
			params[1] = s->hw->cmd->start_scanning;

			epson_send(s, params, 2, 0, &status);
			if (status != SANE_STATUS_GOOD)
				return status;

			goto retry;
		}
		else
			free(ext_status);

	}

	return status;
}

void
epson2_scan_finish(Epson_Scanner * s)
{
	unsigned char *buf;
	SANE_Status status;
	int i;

	DBG(5, "%s\n", __func__);

	free(s->buf);
	s->buf = NULL;

	for (i = 0; i < s->line_distance; i++) {
		if (s->line_buffer[i] != NULL) {
			free(s->line_buffer[i]);
			s->line_buffer[i] = NULL;
		}
	}


	/* XXX do we have other means to check
	 * for paper empty? */
	status = request_extended_status(s, &buf, NULL);
	if (status != SANE_STATUS_GOOD)
		return;

	if (buf[1] & EXT_STATUS_PE)
		sane_auto_eject(s);

	free(buf);

	/* XXX required? */
	reset(s);
}

static inline int
get_color(int status)
{
	switch ((status >> 2) & 0x03) {
	case 1:
		return 1;
	case 2:
		return 0;
	case 3:
		return 2;
	default:
		return 0;	/* required to make the compiler happy */
	}
}

SANE_Status
sane_read(SANE_Handle handle, SANE_Byte * data, SANE_Int max_length,
	  SANE_Int * length)
{
	Epson_Scanner *s = (Epson_Scanner *) handle;
	SANE_Status status;
	int index = 0;
	SANE_Bool reorder = SANE_FALSE;
	SANE_Bool needStrangeReorder = SANE_FALSE;
	int bytes_to_process = 0;

      START_READ:
	DBG(5, "%s: begin\n", __func__);

	if (s->ptr == s->end) {
		EpsonDataRec result;
		size_t buf_len;

		if ((s->fd != -1) && s->eof) {
			if (s->hw->color_shuffle) {
				DBG(1,
				    "written %d lines after color shuffle\n",
				    s->lines_written);
				DBG(1, "lines requested: %d\n",
				    s->params.lines);
			}

			*length = 0;
			epson2_scan_finish(s);

			return SANE_STATUS_EOF;
		}

		if (SANE_STATUS_GOOD !=
		    (status = read_info_block(s, &result))) {
			*length = 0;
			epson2_scan_finish(s);
			return status;
		}

		buf_len = result.buf[1] << 8 | result.buf[0];

		if (s->block)
			buf_len *= (result.buf[3] << 8 | result.buf[2]);

		DBG(5, "%s: buf len = %lu\n", __func__, (u_long) buf_len);

		if (!s->block && SANE_FRAME_RGB == s->params.format) {
			/*
			 * Read color data in line mode
			 */

			/*
			 * read the first color line - the number of bytes to read
			 * is already known (from last call to read_info_block()
			 * We determine where to write the line from the color information
			 * in the data block. At the end we want the order RGB, but the
			 * way the data is delivered does not guarantee this - actually it's
			 * most likely that the order is GRB if it's not RGB!
			 */
			index = get_color(result.status);

			epson_recv(s,
				   s->buf + index * s->params.pixels_per_line,
				   buf_len, &status);
			if (status != SANE_STATUS_GOOD)
				return status;
			/*
			 * send the ACK signal to the scanner in order to make
			 * it ready for the next data block.
			 */
			status = epson2_ack(s);

			/*
			 * ... and request the next data block
			 */
			if (SANE_STATUS_GOOD !=
			    (status = read_info_block(s, &result)))
				return status;

			buf_len = result.buf[1] << 8 | result.buf[0];
			/*
			 * this should never happen, because we are already in
			 * line mode, but it does not hurt to check ...
			 */
			if (s->block)
				buf_len *=
					(result.buf[3] << 8 | result.buf[2]);

			DBG(5, "%s: buf len2 = %lu\n", __func__,
			    (u_long) buf_len);

			index = get_color(result.status);

			epson_recv(s,
				   s->buf + index * s->params.pixels_per_line,
				   buf_len, &status);

			if (status != SANE_STATUS_GOOD) {
				epson2_scan_finish(s);
				*length = 0;
				return status;
			}

			status = epson2_ack(s);

			/*
			 * ... and the last info block
			 */
			if (SANE_STATUS_GOOD !=
			    (status = read_info_block(s, &result))) {
				*length = 0;
				epson2_scan_finish(s);
				return status;
			}

			buf_len = result.buf[1] << 8 | result.buf[0];

			if (s->block)
				buf_len *=
					(result.buf[3] << 8 | result.buf[2]);

			DBG(5, "%s: buf len3 = %lu\n", __func__,
			    (u_long) buf_len);

			index = get_color(result.status);

			/* receive image data */
			epson_recv(s,
				   s->buf + index * s->params.pixels_per_line,
				   buf_len, &status);

			if (status != SANE_STATUS_GOOD) {
				*length = 0;
				epson2_scan_finish(s);
				return status;
			}
		} else {
			/*
			 * Read data in block mode
			 */

			/* do we have to reorder the data ? */
			if (get_color(result.status) == 0x01) {
				reorder = SANE_TRUE;
			}

			bytes_to_process =
				epson_recv(s, s->buf, buf_len, &status);

			/* bytes_to_process = buf_len; */

			if (status != SANE_STATUS_GOOD) {
				*length = 0;
				epson2_scan_finish(s);
				return status;
			}
		}

		if (result.status & STATUS_AREA_END) {
			s->eof = SANE_TRUE;
		} else {
			if (s->canceling) {
				status = epson2_cmd_simple(s, S_CAN, 1);

				*length = 0;

				epson2_scan_finish(s);

				return SANE_STATUS_CANCELLED;
			} else	/* XXX */
				status = epson2_ack(s);
		}

		s->end = s->buf + buf_len;
		s->ptr = s->buf;

		/*
		 * if we have to re-order the color components (GRB->RGB) we
		 * are doing this here:
		 */

		/*
		 * Some scaners (e.g. the Perfection 1640 and GT-2200) seem
		 * to have the R and G channels swapped.
		 * The GT-8700 is the Asian version of the Perfection1640.
		 * If the scanner name is one of these, and the scan mode is
		 * RGB then swap the colors.
		 */

		needStrangeReorder =
			(strstr(s->hw->sane.model, "GT-2200") ||
			 ((strstr(s->hw->sane.model, "1640")
			   && strstr(s->hw->sane.model, "Perfection"))
			  || strstr(s->hw->sane.model, "GT-8700")))
			&& s->params.format == SANE_FRAME_RGB;

		/*
		 * Certain Perfection 1650 also need this re-ordering of the two
		 * color channels. These scanners are identified by the problem
		 * with the half vertical scanning area. When we corrected this,
		 * we also set the variable s->hw->need_color_reorder
		 */
		if (s->hw->need_color_reorder) {
			needStrangeReorder = SANE_TRUE;
		}

		if (needStrangeReorder)
			reorder = SANE_FALSE;	/* reordering once is enough */

		if (s->params.format != SANE_FRAME_RGB)
			reorder = SANE_FALSE;	/* don't reorder for BW or gray */

		if (reorder) {
			SANE_Byte *ptr;

			ptr = s->buf;
			while (ptr < s->end) {
				if (s->params.depth > 8) {
					SANE_Byte tmp;

					/* R->G G->R */
					tmp = ptr[0];
					ptr[0] = ptr[2];	/* first Byte G */
					ptr[2] = tmp;	/* first Byte R */

					tmp = ptr[1];
					ptr[1] = ptr[3];	/* second Byte G */
					ptr[3] = tmp;	/* second Byte R */

					ptr += 6;	/* go to next pixel */
				} else {
					/* R->G G->R */
					SANE_Byte tmp;

					tmp = ptr[0];
					ptr[0] = ptr[1];	/* G */
					ptr[1] = tmp;	/* R */
					/* B stays the same */
					ptr += 3;	/* go to next pixel */
				}
			}
		}

		/*
		 * Do the color_shuffle if everything else is correct - at this time
		 * most of the stuff is hardcoded for the Perfection 610
		 */

		if (s->hw->color_shuffle) {
			int new_length = 0;

			status = color_shuffle(s, &new_length);

			/*
			 * If no bytes are returned, check if the scanner is already done, if so,
			 * we'll probably just return, but if there is more data to process get
			 * the next batch.
			 */
			if (new_length == 0 && s->end != s->ptr)
				goto START_READ;

			s->end = s->buf + new_length;
			s->ptr = s->buf;
		}

		DBG(5, "%s: begin scan2\n", __func__);
	}

	/*
	 * copy the image data to the data memory area
	 */

	if (!s->block && SANE_FRAME_RGB == s->params.format) {

		max_length /= 3;

		if (max_length > s->end - s->ptr)
			max_length = s->end - s->ptr;

		*length = 3 * max_length;

		if (s->invert_image == SANE_TRUE) {
			while (max_length-- != 0) {
				/* invert the three values */
				*data++ = (unsigned char) ~(s->ptr[0]);
				*data++ =
					(unsigned char) ~(s->
							  ptr[s->params.
							      pixels_per_line]);
				*data++ =
					(unsigned char) ~(s->
							  ptr[2 *
							      s->params.
							      pixels_per_line]);
				++s->ptr;
			}
		} else {
			while (max_length-- != 0) {
				*data++ = s->ptr[0];
				*data++ = s->ptr[s->params.pixels_per_line];
				*data++ =
					s->ptr[2 * s->params.pixels_per_line];
				++s->ptr;
			}
		}
	} else {
		if (max_length > s->end - s->ptr)
			max_length = s->end - s->ptr;

		*length = max_length;

		if (1 == s->params.depth) {
			if (s->invert_image == SANE_TRUE) {
				while (max_length-- != 0)
					*data++ = *s->ptr++;
			} else {
				while (max_length-- != 0)
					*data++ = ~*s->ptr++;
			}
		} else {

			if (s->invert_image == SANE_TRUE) {
				int i;

				for (i = 0; i < max_length; i++) {
					data[i] =
						(unsigned char) ~(s->ptr[i]);
				}
			} else {
				memcpy(data, s->ptr, max_length);
			}
			s->ptr += max_length;
		}
	}

	DBG(5, "%s: end\n", __func__);

	return SANE_STATUS_GOOD;
}


static SANE_Status
color_shuffle(SANE_Handle handle, int *new_length)
{
	Epson_Scanner *s = (Epson_Scanner *) handle;
	SANE_Byte *buf = s->buf;
	int length = s->end - s->buf;

	if (s->hw->color_shuffle == SANE_TRUE) {
		SANE_Byte *data_ptr;	/* ptr to data to process */
		SANE_Byte *data_end;	/* ptr to end of processed data */
		SANE_Byte *out_data_ptr;	/* ptr to memory when writing data */
		int i;		/* loop counter */

		/*
		 * It looks like we are dealing with a scanner that has an odd way
		 * of dealing with colors... The red and blue scan lines are shifted
		 * up or down by a certain number of lines relative to the green line.
		 */
		DBG(5, "%s\n", __func__);

		/*
		 * Initialize the variables we are going to use for the
		 * copying of the data. data_ptr is the pointer to
		 * the currently worked on scan line. data_end is the
		 * end of the data area as calculated from adding *length
		 * to the start of data.
		 * out_data_ptr is used when writing out the processed data
		 * and always points to the beginning of the next line to
		 * write.
		 */
		data_ptr = out_data_ptr = buf;
		data_end = data_ptr + length;

		/*
		 * The image data is in *buf, we know that the buffer contains s->end - s->buf ( = length)
		 * bytes of data. The width of one line is in s->params.bytes_per_line
		 *
		 * The buffer area is supposed to have a number of full scan
		 * lines, let's test if this is the case.
		 */

		if (length % s->params.bytes_per_line != 0) {
			DBG(1, "error in buffer size: %d / %d\n", length,
			    s->params.bytes_per_line);
			return SANE_STATUS_INVAL;
		}

		while (data_ptr < data_end) {
			SANE_Byte *source_ptr, *dest_ptr;
			int loop;

			/* copy the green information into the current line */

			source_ptr = data_ptr + 1;
			dest_ptr = s->line_buffer[s->color_shuffle_line] + 1;

			for (i = 0; i < s->params.bytes_per_line / 3; i++) {
				*dest_ptr = *source_ptr;
				dest_ptr += 3;
				source_ptr += 3;
			}

			/* copy the red information n lines back */

			if (s->color_shuffle_line >= s->line_distance) {
				source_ptr = data_ptr + 2;
				dest_ptr =
					s->line_buffer[s->color_shuffle_line -
						       s->line_distance] + 2;

/*				while (source_ptr < s->line_buffer[s->color_shuffle_line] + s->params.bytes_per_line) */
				for (loop = 0;
				     loop < s->params.bytes_per_line / 3;
				     loop++) {
					*dest_ptr = *source_ptr;
					dest_ptr += 3;
					source_ptr += 3;
				}
			}

			/* copy the blue information n lines forward */

			source_ptr = data_ptr;
			dest_ptr =
				s->line_buffer[s->color_shuffle_line +
					       s->line_distance];

/*			while (source_ptr < s->line_buffer[s->color_shuffle_line] + s->params.bytes_per_line) */
			for (loop = 0; loop < s->params.bytes_per_line / 3;
			     loop++) {
				*dest_ptr = *source_ptr;
				dest_ptr += 3;
				source_ptr += 3;
			}

			data_ptr += s->params.bytes_per_line;

			if (s->color_shuffle_line == s->line_distance) {
				/*
				 * We just finished the line in line_buffer[0] - write it to the
				 * output buffer and continue.
				 *
				 * The ouput buffer ist still "buf", but because we are
				 * only overwriting from the beginning of the memory area
				 * we are not interfering with the "still to shuffle" data
				 * in the same area.
				 */

				/*
				 * Strip the first and last n lines and limit to
				 */
				if ((s->current_output_line >=
				     s->line_distance)
				    && (s->current_output_line <
					s->params.lines + s->line_distance)) {
					memcpy(out_data_ptr,
					       s->line_buffer[0],
					       s->params.bytes_per_line);
					out_data_ptr +=
						s->params.bytes_per_line;

					s->lines_written++;
				}

				s->current_output_line++;

				/*
				 * Now remove the 0-entry and move all other
				 * lines up by one. There are 2*line_distance + 1
				 * buffers, * therefore the loop has to run from 0
				 * to * 2*line_distance, and because we want to
				 * copy every n+1st entry to n the loop runs
				 * from - to 2*line_distance-1!
				 */

				free(s->line_buffer[0]);

				for (i = 0; i < s->line_distance * 2; i++) {
					s->line_buffer[i] =
						s->line_buffer[i + 1];
				}

				/*
				 * and create one new buffer at the end
				 */

				s->line_buffer[s->line_distance * 2] =
					malloc(s->params.bytes_per_line);
				if (s->line_buffer[s->line_distance * 2] ==
				    NULL) {
					int i;
					for (i = 0; i < s->line_distance * 2;
					     i++) {
						free(s->line_buffer[i]);
						s->line_buffer[i] = NULL;
					}
					DBG(1, "out of memory (line %d)\n",
					    __LINE__);
					return SANE_STATUS_NO_MEM;
				}
			} else {
				s->color_shuffle_line++;	/* increase the buffer number */
			}
		}

		/*
		 * At this time we've used up all the new data from the scanner, some of
		 * it is still in the line_buffers, but we are ready to return some of it
		 * to the front end software. To do so we have to adjust the size of the
		 * data area and the *new_length variable.
		 */

		*new_length = out_data_ptr - buf;
	}

	return SANE_STATUS_GOOD;

}

/*
 * void sane_cancel(SANE_Handle handle)
 *
 * Set the cancel flag to true. The next time the backend requests data
 * from the scanner the CAN message will be sent.
 */

void
sane_cancel(SANE_Handle handle)
{
	Epson_Scanner *s = (Epson_Scanner *) handle;

	/*
	 * If the s->ptr pointer is not NULL, then a scan operation
	 * was started and if s->eof is FALSE, it was not finished.
	 */

	if (s->buf != NULL) {
		unsigned char *dummy;
		int len;

		/* malloc one line */
		dummy = malloc(s->params.bytes_per_line);
		if (dummy == NULL) {
			DBG(1, "Out of memory\n");
			return;
		} else {

			/* there is still data to read from the scanner */
			s->canceling = SANE_TRUE;

			while (!s->eof
			       && SANE_STATUS_CANCELLED != sane_read(s, dummy,
								     s->
								     params.
								     bytes_per_line,
								     &len)) {
				/* empty body, the while condition does the processing */
			}
			free(dummy);
		}
	}
}

static void
filter_resolution_list(Epson_Scanner * s)
{
	/* re-create the list */

	if (s->val[OPT_LIMIT_RESOLUTION].w == SANE_TRUE) {
		/* copy the short list */

		/* filter out all values that are not 300 or 400 dpi based */
		int i;

		int new_size = 0;
		SANE_Bool is_correct_resolution = SANE_FALSE;

		for (i = 1; i <= s->hw->res_list_size; i++) {
			SANE_Word res;
			res = s->hw->res_list[i];
			if ((res < 100) || (0 == (res % 300))
			    || (0 == (res % 400))) {
				/* add the value */
				new_size++;

				s->hw->resolution_list[new_size] =
					s->hw->res_list[i];

				/* check for a valid current resolution */
				if (res == s->val[OPT_RESOLUTION].w) {
					is_correct_resolution = SANE_TRUE;
				}
			}
		}
		s->hw->resolution_list[0] = new_size;

		if (is_correct_resolution == SANE_FALSE) {
			for (i = 1; i <= new_size; i++) {
				if (s->val[OPT_RESOLUTION].w <
				    s->hw->resolution_list[i]) {
					s->val[OPT_RESOLUTION].w =
						s->hw->resolution_list[i];
					i = new_size + 1;
				}
			}
		}

	} else {
		/* copy the full list */
		s->hw->resolution_list[0] = s->hw->res_list_size;
		memcpy(&(s->hw->resolution_list[1]), s->hw->res_list,
		       s->hw->res_list_size * sizeof(SANE_Word));
	}
}

/**********************************************************************************/

/*
 * SANE_Status sane_set_io_mode()
 *
 * not supported - for asynchronous I/O
 */

SANE_Status
sane_set_io_mode(SANE_Handle handle, SANE_Bool non_blocking)
{
	/* get rid of compiler warning */
	handle = handle;
	non_blocking = non_blocking;

	return SANE_STATUS_UNSUPPORTED;
}

/*
 * SANE_Status sane_get_select_fd()
 *
 * not supported - for asynchronous I/O
 */

SANE_Status
sane_get_select_fd(SANE_Handle handle, SANE_Int * fd)
{
	/* get rid of compiler warnings */
	handle = handle;
	fd = fd;

	return SANE_STATUS_UNSUPPORTED;
}
