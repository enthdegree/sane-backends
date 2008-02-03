/*
 * epson2.c - SANE library for Epson scanners.
 *
 * Based on Kazuhiro Sasayama previous
 * Work on epson.[ch] file from the SANE package.
 * Please see those files for additional copyrights.
 *
 * Copyright (C) 2006-07 Tower Technologies
 * Author: Alessandro Zummo <a.zummo@towertech.it>
 *
 * This file is part of the SANE package.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2.
 */

#define	SANE_EPSON2_VERSION	"SANE Epson 2 Backend v0.1.16 - 2007-12-30"
#define SANE_EPSON2_BUILD	116

/* debugging levels:
 *
 *     127	e2_recv buffer
 *     125	e2_send buffer
 *	20	usb cmd counters
 *	18	sane_read
 *	17	setvalue
 *	15	e2_send, e2_recv calls
 *	13	e2_cmd_info_block
 *	12	epson_cmd_simple
 *	11	even more
 *	10	more debug in ESC/I commands
 *	 9	ESC x/FS x in e2_send
 *	 8	ESC/I commands
 *	 7	open/close/attach
 *	 6	print_params
 *	 5	basic functions
 *	 3	status information
 *	 1	scanner info and capabilities
 *		warnings
 */

#include <sane/config.h>

#include "epson2.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>

#include <byteorder.h>

#include <sane/saneopts.h>
#include <sane/sanei_scsi.h>
#include <sane/sanei_usb.h>
#include <sane/sanei_pio.h>
#include <sane/sanei_tcp.h>
#include <sane/sanei_udp.h>
#include <sane/sanei_backend.h>
#include <sane/sanei_config.h>

#include "epson2-io.h"
#include "epson2-commands.h"

#include "epson2_scsi.h"
#include "epson_usb.h"
#include "epson2_net.h"

static EpsonCmdRec epson_cmd[] = {

/*
 *	      request identity
 *	      |   request identity2
 *	      |    |    request status
 *	      |    |    |    request command parameter
 *	      |    |    |    |    set color mode
 *	      |    |    |    |    |    start scanning
 *	      |    |    |    |    |    |    set data format
 *	      |    |    |    |    |    |    |    set resolution
 *	      |    |    |    |    |    |    |    |    set zoom
 *	      |    |    |    |    |    |    |    |    |    set scan area
 *	      |    |    |    |    |    |    |    |    |    |    set brightness
 *	      |    |    |    |    |    |    |    |    |    |    |		set gamma
 *	      |    |    |    |    |    |    |    |    |    |    |		|    set halftoning
 *	      |    |    |    |    |    |    |    |    |    |    |		|    |    set color correction
 *	      |    |    |    |    |    |    |    |    |    |    |		|    |    |    initialize scanner
 *	      |    |    |    |    |    |    |    |    |    |    |		|    |    |    |    set speed
 *	      |    |    |    |    |    |    |    |    |    |    |		|    |    |    |    |    set lcount
 *	      |    |    |    |    |    |    |    |    |    |    |		|    |    |    |    |    |    mirror image
 *	      |    |    |    |    |    |    |    |    |    |    |		|    |    |    |    |    |    |    set gamma table
 *	      |    |    |    |    |    |    |    |    |    |    |		|    |    |    |    |    |    |    |    set outline emphasis
 *	      |    |    |    |    |    |    |    |    |    |    |		|    |    |    |    |    |    |    |    |    set dither
 *	      |    |    |    |    |    |    |    |    |    |    |		|    |    |    |    |    |    |    |    |    |    set color correction coefficients
 *	      |    |    |    |    |    |    |    |    |    |    |		|    |    |    |    |    |    |    |    |    |    |    request extended status
 *	      |    |    |    |    |    |    |    |    |    |    |		|    |    |    |    |    |    |    |    |    |    |    |    control an extension
 *	      |    |    |    |    |    |    |    |    |    |    |		|    |    |    |    |    |    |    |    |    |    |    |    |    forward feed / eject
 *	      |    |    |    |    |    |    |    |    |    |    |		|    |    |    |    |    |    |    |    |    |    |    |    |    |     feed
 *	      |    |    |    |    |    |    |    |    |    |    |		|    |    |    |    |    |    |    |    |    |    |    |    |    |     |     request push button status
 *	      |    |    |    |    |    |    |    |    |    |    |		|    |    |    |    |    |    |    |    |    |    |    |    |    |     |     |    control auto area segmentation
 *	      |    |    |    |    |    |    |    |    |    |    |		|    |    |    |    |    |    |    |    |    |    |    |    |    |     |     |    |    set film type
 *	      |    |    |    |    |    |    |    |    |    |    |		|    |    |    |    |    |    |    |    |    |    |    |    |    |     |     |    |    |    set exposure time
 *	      |    |    |    |    |    |    |    |    |    |    |		|    |    |    |    |    |    |    |    |    |    |    |    |    |     |     |    |    |    |    set bay
 *	      |    |    |    |    |    |    |    |    |    |    |		|    |    |    |    |    |    |    |    |    |    |    |    |    |     |     |    |    |    |    |    set threshold
 *	      |    |    |    |    |    |    |    |    |    |    |		|    |    |    |    |    |    |    |    |    |    |    |    |    |     |     |    |    |    |    |    |    set focus position
 *	      |    |    |    |    |    |    |    |    |    |    |		|    |    |    |    |    |    |    |    |    |    |    |    |    |     |     |    |    |    |    |    |    |    request focus position
 *	      |    |    |    |    |    |    |    |    |    |    |		|    |    |    |    |    |    |    |    |    |    |    |    |    |     |     |    |    |    |    |    |    |    |    request extended identity
 *	      |    |    |    |    |    |    |    |    |    |    |		|    |    |    |    |    |    |    |    |    |    |    |    |    |     |     |    |    |    |    |    |    |    |    |    request scanner status
 *	      |    |    |    |    |    |    |    |    |    |    |		|    |    |    |    |    |    |    |    |    |    |    |    |    |     |     |    |    |    |    |    |    |    |    |    |
 *	      |    |    |    |    |    |    |    |    |    |    |		|    |    |    |    |    |    |    |    |    |    |    |    |    |     |     |    |    |    |    |    |    |    |    |    |
 *	      |    |    |    |    |    |    |    |    |    |    |		|    |    |    |    |    |    |    |    |    |    |    |    |    |     |     |    |    |    |    |    |    |    |    |    |
 */
	{"A1", 'I', 0x0, 'F', 'S', 0x0, 'G', 0x0, 'R', 0x0, 'A', 0x0,
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
	 'f', 'e', '\f', 0x19, '!', 's', 'N', 0x0, 0x0, 0x0, 'p', 'q', 'I',
	 'F'},
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

static int film_params[] = { 0, 1, 2, 3 };

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

/* Bay list:
 * this is used for the FilmScan
 * XXX Add APS loader support
 */

static const SANE_String_Const bay_list[] = {
	"1",
	"2",
	"3",
	"4",
	"5",
	"6",
	NULL
};

/* minimum, maximum, quantization */
static const SANE_Range u8_range = { 0, 255, 0 };
static const SANE_Range s8_range = { -127, 127, 0 };

/* used for several boolean choices */
static int switch_params[] = {
	0,
	1
};

#define mirror_params switch_params

static const SANE_Range outline_emphasis_range = { -2, 2, 0 };


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

	unsigned char buf[4];

} EpsonDataRec;

static SANE_Status color_shuffle(SANE_Handle handle, int *new_length);
static SANE_Status attach_one_usb(SANE_String_Const devname);
static SANE_Status attach_one_net(SANE_String_Const devname);
static void filter_resolution_list(Epson_Scanner * s);


static SANE_Bool
e2_model(Epson_Scanner * s, const char *model)
{
	if (s->hw->model == NULL)
		return SANE_FALSE;

	if (strncmp(s->hw->model, model, strlen(model)) == 0)
		return SANE_TRUE;

	return SANE_FALSE;
}

/* A little helper function to correct the extended status reply
 * gotten from scanners with known buggy firmware.
 */
static void
fix_up_extended_status_reply(Epson_Scanner * s, unsigned char *buf)
{
	if (e2_model(s, "ES-9000H") || e2_model(s, "GT-30000")) {
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
	DBG(6, "params.format = %d\n", params.format);
	DBG(6, "params.last_frame = %d\n", params.last_frame);
	DBG(6, "params.bytes_per_line = %d\n", params.bytes_per_line);
	DBG(6, "params.pixels_per_line = %d\n", params.pixels_per_line);
	DBG(6, "params.lines = %d\n", params.lines);
	DBG(6, "params.depth = %d\n", params.depth);
}

static void
e2_set_cmd_level(SANE_Handle handle, unsigned char *level)
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

/*
 * close_scanner()
 *
 * Close the open scanner. Depending on the connection method, a different
 * close function is called.
 */

static void
close_scanner(Epson_Scanner * s)
{
	DBG(7, "%s: fd = %d\n", __func__, s->fd);

	if (s->fd == -1)
		return;

	/* send a request_status. This toggles w_cmd_count and r_cmd_count */
	if (r_cmd_count % 2)
		esci_request_status(s, NULL);

	/* request extended status. This toggles w_cmd_count only */
	if (w_cmd_count % 2) {
		esci_request_extended_status(s, NULL, NULL);
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

static void
e2_network_discovery(void)
{
	fd_set rfds;
	int fd, len;
	SANE_Status status;

	char *ip, *query = "EPSONP\x00\xff\x00\x00\x00\x00\x00\x00\x00";
	u_char buf[76];

	struct timeval to;

	status = sanei_udp_open_broadcast(&fd);
	if (status != SANE_STATUS_GOOD)
		return;

	sanei_udp_write_broadcast(fd, 3289, (u_char *) query, 15);

	DBG(5, "%s, sent discovery packet\n", __func__);

	to.tv_sec = 1;
	to.tv_usec = 0;

	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);

	if (select(fd + 1, &rfds, NULL, NULL, &to) > 0) {
		while ((len = sanei_udp_recvfrom(fd, buf, 76, &ip)) == 76) {
			DBG(5, " response from %s\n", ip);

			/* minimal check, protocol unknown */
			if (strncmp((char *) buf, "EPSON", 5) == 0)
				attach_one_net(ip);
		}
	}

	DBG(5, "%s, end\n", __func__);

	sanei_udp_close(fd);
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

	DBG(7, "%s\n", __func__);

	if (s->fd != -1) {
		DBG(5, "scanner is already open: fd = %d\n", s->fd);
		return SANE_STATUS_GOOD;	/* no need to open the scanner */
	}

	if (s->hw->connection == SANE_EPSON_NET) {
		unsigned char buf[5];

		/* Sleep a bit or the network scanner will not be ready */
		sleep(1);

		status = sanei_tcp_open(s->hw->sane.name, 1865, &s->fd);
		if (status != SANE_STATUS_GOOD)
			goto end;

		s->netlen = 0;
		/* the scanner sends a kind of welcome msg */
		e2_recv(s, buf, 5, &status);

		/* lock the scanner for use by sane */
		sanei_epson_net_lock(s);
	} else if (s->hw->connection == SANE_EPSON_SCSI)
		status = sanei_scsi_open(s->hw->sane.name, &s->fd,
					 sanei_epson2_scsi_sense_handler,
					 NULL);
	else if (s->hw->connection == SANE_EPSON_PIO)
		status = sanei_pio_open(s->hw->sane.name, &s->fd);
	else if (s->hw->connection == SANE_EPSON_USB)
		status = sanei_usb_open(s->hw->sane.name, &s->fd);

      end:

	if (status != SANE_STATUS_GOOD)
		DBG(1, "%s open failed: %s\n", s->hw->sane.name,
		    sane_strstatus(status));

	return status;
}


static int num_devices = 0;	/* number of scanners attached to backend */
static Epson_Device *first_dev = NULL;	/* first EPSON scanner in list */
static Epson_Scanner *first_handle = NULL;

static SANE_Status
e2_set_model(Epson_Scanner * s, unsigned char *model, size_t len)
{
	unsigned char *buf;
	char *p;
	struct Epson_Device *dev = s->hw;

	buf = malloc(len + 1);
	if (buf == NULL)
		return SANE_STATUS_NO_MEM;

	memcpy(buf, model, len);
	buf[len] = '\0';

	p = strchr((const char *) buf, ' ');
	if (p != NULL)
		*p = '\0';

	if (dev->model)
		free(dev->model);

	dev->model = strndup((const char *) buf, len);
	dev->sane.model = dev->model;

	DBG(10, "%s: model is '%s'\n", __func__, dev->model);

	free(buf);

	return SANE_STATUS_GOOD;
}

static SANE_Status
e2_add_resolution(Epson_Scanner * s, int r)
{
	struct Epson_Device *dev = s->hw;

	dev->res_list_size++;
	dev->res_list = (SANE_Int *) realloc(dev->res_list,
					     dev->res_list_size *
					     sizeof(SANE_Word));

	DBG(10, "%s: add (dpi): %d\n", __func__, r);

	if (dev->res_list == NULL)
		return SANE_STATUS_NO_MEM;

	dev->res_list[dev->res_list_size - 1] = (SANE_Int) r;

	return SANE_STATUS_GOOD;
}


/* Helper function to correct the dpi
 * gotten from scanners with known buggy firmware.
 * - Epson Perfection 4990 Photo / GT-X800
 * - Epson Perfection 4870 Photo / GT-X700 (untested)
 */
static void
fix_up_dpi(Epson_Scanner * s)
{
	SANE_Status status;
	/*
	 * EPSON Programming guide for
	 * EPSON Color Image Scanner Perfection 4870/4990
	 */

	if (e2_model(s, "GT-X800")
	    || e2_model(s, "GT-X700")) {
		status = e2_add_resolution(s, 4800);
		status = e2_add_resolution(s, 6400);
		status = e2_add_resolution(s, 9600);
		status = e2_add_resolution(s, 12800);
	}
}

static void
e2_set_fbf_area(Epson_Scanner * s, int x, int y, int unit)
{
	struct Epson_Device *dev = s->hw;

	if (x == 0 || y == 0)
		return;

	dev->fbf_x_range.min = 0;
	dev->fbf_x_range.max = SANE_FIX(x * MM_PER_INCH / unit);
	dev->fbf_x_range.quant = 0;

	dev->fbf_y_range.min = 0;
	dev->fbf_y_range.max = SANE_FIX(y * MM_PER_INCH / unit);
	dev->fbf_y_range.quant = 0;

	DBG(5, "%s: %f,%f %f,%f %d [mm]\n",
	    __func__,
	    SANE_UNFIX(dev->fbf_x_range.min),
	    SANE_UNFIX(dev->fbf_y_range.min),
	    SANE_UNFIX(dev->fbf_x_range.max),
	    SANE_UNFIX(dev->fbf_y_range.max), unit);
}

static void
e2_set_adf_area(struct Epson_Scanner *s, int x, int y, int unit)
{
	struct Epson_Device *dev = s->hw;

	dev->adf_x_range.min = 0;
	dev->adf_x_range.max = SANE_FIX(x * MM_PER_INCH / unit);
	dev->adf_x_range.quant = 0;

	dev->adf_y_range.min = 0;
	dev->adf_y_range.max = SANE_FIX(y * MM_PER_INCH / unit);
	dev->adf_y_range.quant = 0;

	DBG(5, "%s: %f,%f %f,%f %d [mm]\n",
	    __func__,
	    SANE_UNFIX(dev->adf_x_range.min),
	    SANE_UNFIX(dev->adf_y_range.min),
	    SANE_UNFIX(dev->adf_x_range.max),
	    SANE_UNFIX(dev->adf_y_range.max), unit);
}

static void
e2_set_tpu_area(struct Epson_Scanner *s, int x, int y, int unit)
{
	struct Epson_Device *dev = s->hw;

	dev->tpu_x_range.min = 0;
	dev->tpu_x_range.max = SANE_FIX(x * MM_PER_INCH / unit);
	dev->tpu_x_range.quant = 0;

	dev->tpu_y_range.min = 0;
	dev->tpu_y_range.max = SANE_FIX(y * MM_PER_INCH / unit);
	dev->tpu_y_range.quant = 0;

	DBG(5, "%s: %f,%f %f,%f %d [mm]\n",
	    __func__,
	    SANE_UNFIX(dev->tpu_x_range.min),
	    SANE_UNFIX(dev->tpu_y_range.min),
	    SANE_UNFIX(dev->tpu_x_range.max),
	    SANE_UNFIX(dev->tpu_y_range.max), unit);
}

static void
e2_add_depth(Epson_Device * dev, SANE_Word depth)
{
	if (dev->maxDepth == 0)
		dev->maxDepth = depth;

	bitDepthList[0]++;
	bitDepthList[bitDepthList[0]] = depth;
}

static SANE_Status
e2_discover_capabilities(Epson_Scanner *s)
{
	SANE_Status status;

	unsigned char scanner_status;
	Epson_Device *dev = s->hw;

	SANE_String_Const *source_list_add = source_list;

	DBG(5, "%s\n", __func__);

	/* ESC I, request identity
	 * this must be the first command on the FilmScan 200
	 */
	if (dev->connection != SANE_EPSON_NET) {
		unsigned int n, k, x = 0, y = 0;
		unsigned char *buf, *area;
		size_t len;

		status = esci_request_identity(s, &buf, &len);
		if (status != SANE_STATUS_GOOD)
			return status;

		e2_set_cmd_level(s, &buf[0]);

		/* Setting available resolutions and xy ranges for sane frontend. */
		/* cycle thru the resolutions, saving them in a list */
		for (n = 2, k = 0; n < len; n += k) {

			area = buf + n;

			switch (*area) {
			case 'R':
			{
				int val = area[2] << 8 | area[1];

				status = e2_add_resolution(s, val);
				k = 3;
				continue;
			}
			case 'A':
			{
				x = area[2] << 8 | area[1];
				y = area[4] << 8 | area[3];

				DBG(1, "maximum scan area: %dx%d\n", x, y);
				k = 5;
				continue;
			}
			default:
				break;
			}
		}

		/* min and max dpi */
		dev->dpi_range.min = dev->res_list[0];
		dev->dpi_range.max = dev->res_list[dev->res_list_size - 1];
		dev->dpi_range.quant = 0;

		e2_set_fbf_area(s, x, y, dev->dpi_range.max);

		free(buf);
	}

	/* ESC F, request status */
	status = esci_request_status(s, &scanner_status);
	if (status != SANE_STATUS_GOOD)
		return status;;

	/* set capabilities */
	if (scanner_status & STATUS_OPTION)
		dev->extension = SANE_TRUE;

	if (scanner_status & STATUS_EXT_COMMANDS)
		dev->extended_commands = 1;

	/*
	 * Extended status flag request (ESC f).
	 * this also requests the scanner device name from the the scanner.
	 * It seems unsupported on the network transport (CX11NF/LP-A500),
	 * so avoid it if the device support extended commands.
	 */

	if (!dev->extended_commands && dev->cmd->request_extended_status) {
		unsigned char *es;
		size_t es_len;

		status = esci_request_extended_status(s, &es, &es_len);
		if (status != SANE_STATUS_GOOD)
			return status;

		/*
		 * Get the device name and copy it to dev->sane.model.
		 * The device name starts at es[0x1A] and is up to 16 bytes long
		 * We are overwriting whatever was set previously!
		 */
		if (es_len == CMD_SIZE_EXT_STATUS)	/* 42 */
			e2_set_model(s, es + 0x1A, 16);

		if (es[0] & EXT_STATUS_LID)
			DBG(1, "LID detected\n");

		if (es[0] & EXT_STATUS_PB)
			DBG(1, "push button detected\n");
		else
			dev->cmd->request_push_button_status = 0;

		/* Flatbed */
		*source_list_add++ = FBF_STR;

		e2_set_fbf_area(s, es[13] << 8 | es[12], es[15] << 8 | es[14],
				dev->dpi_range.max);

		/* ADF */
		if (dev->extension && (es[1] & EXT_STATUS_IST)) {
			DBG(1, "ADF detected\n");

			fix_up_extended_status_reply(s, es);

			dev->duplex = (es[0] & EXT_STATUS_ADFS) != 0;
			if (dev->duplex)
				DBG(1, "ADF supports duplex\n");

			if (es[1] & EXT_STATUS_EN) {
				DBG(1, "ADF is enabled\n");
				dev->x_range = &dev->adf_x_range;
				dev->y_range = &dev->adf_y_range;
			}

			e2_set_adf_area(s, es[3] << 8 | es[2],
					es[5] << 8 | es[4],
					dev->dpi_range.max);
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

			e2_set_tpu_area(s,
					(es[8] << 8 | es[7]),
					(es[10] << 8 | es[9]),
					dev->dpi_range.max);

			*source_list_add++ = TPU_STR;
			dev->TPU = SANE_TRUE;
		}

		free(es);
	}

	/* FS I, request extended identity */
	if (dev->extended_commands) {
		unsigned char buf[80];

		status = esci_request_extended_identity(s, buf);
		if (status != SANE_STATUS_GOOD)
			return status;

		e2_set_cmd_level(s, &buf[0]);

		dev->maxDepth = buf[67];

		/* set model name. it will probably be
		 * different than the one reported by request_identity
		 * for the same unit (i.e. LP-A500 vs CX11) .
		 */
		e2_set_model(s, &buf[46], 16);

		dev->optical_res = le32atoh(&buf[4]);

		dev->dpi_range.min = le32atoh(&buf[8]);
		dev->dpi_range.max = le32atoh(&buf[12]);

		/* Flatbed */
		*source_list_add++ = FBF_STR;

		e2_set_fbf_area(s, le32atoh(&buf[20]),
				le32atoh(&buf[24]), dev->optical_res);

		/* ADF */
		if (le32atoh(&buf[28]) > 0) {
			e2_set_adf_area(s, le32atoh(&buf[28]),
					le32atoh(&buf[32]), dev->optical_res);

			if (!dev->ADF) {
				*source_list_add++ = ADF_STR;
				dev->ADF = SANE_TRUE;
			}
		}

		/* TPU */

		if (e2_model(s, "GT-X800")) {
			if (le32atoh(&buf[68]) > 0 && !dev->TPU) {
				e2_set_tpu_area(s,
						le32atoh(&buf[68]),
						le32atoh(&buf[72]),
						dev->optical_res);

				*source_list_add++ = TPU_STR;
				dev->TPU = SANE_TRUE;
				dev->TPU2 = SANE_TRUE;
			}
		}

		if (le32atoh(&buf[36]) > 0 && !dev->TPU) {
			e2_set_tpu_area(s,
					le32atoh(&buf[36]),
					le32atoh(&buf[40]), dev->optical_res);

			*source_list_add++ = TPU_STR;
			dev->TPU = SANE_TRUE;
		}


		/* fix problem with broken report of dpi */
		fix_up_dpi(s);
	}


	*source_list_add = NULL;        /* add end marker to source list */

	/*
	 * request identity 2 (ESC i), if available will
	 * get the information from the scanner and store it in dev
	 */

	if (dev->cmd->request_identity2) {
		unsigned char *buf;
		status = esci_request_identity2(s, &buf);
		if (status != SANE_STATUS_GOOD)
			return status;

		/* the first two bytes of the buffer contain the optical resolution */
		dev->optical_res = buf[1] << 8 | buf[0];

		/*
		 * the 4th and 5th byte contain the line distance. Both values have to
		 * be identical, otherwise this software can not handle this scanner.
		 */
		if (buf[4] != buf[5]) {
			status = SANE_STATUS_INVAL;
			return status;
		}

		dev->max_line_distance = buf[4];
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

	bitDepthList[0] = 0;

	/* maximum depth discovery */
	DBG(3, "discovering max depth, NAKs are expected\n");

	if (dev->maxDepth >= 16 || dev->maxDepth == 0) {
		if (esci_set_data_format(s, 16) == SANE_STATUS_GOOD)
			e2_add_depth(dev, 16);
	}

	if (dev->maxDepth >= 14 || dev->maxDepth == 0) {
		if (esci_set_data_format(s, 14) == SANE_STATUS_GOOD)
			e2_add_depth(dev, 14);
	}

	if (dev->maxDepth >= 12 || dev->maxDepth == 0) {
		if (esci_set_data_format(s, 12) == SANE_STATUS_GOOD)
			e2_add_depth(dev, 12);
	}

	/* add default depth */
	e2_add_depth(dev, 8);

	DBG(1, "maximum supported color depth: %d\n", dev->maxDepth);

	/*
	 * Check for "request focus position" command. If this command is
	 * supported, then the scanner does also support the "set focus
	 * position" command.
	 * XXX ???
	 */

	if (esci_request_focus_position(s, &s->currentFocusPosition) ==
	    SANE_STATUS_GOOD) {
		DBG(1, "setting focus is supported\n");
		dev->focusSupport = SANE_TRUE;
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
		dev->focusSupport = SANE_FALSE;
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
		DBG(1, "found buggy scan area, doubling it.\n");
		dev->y_range->max += (dev->y_range->max - dev->y_range->min);
		dev->need_double_vertical = SANE_TRUE;
		dev->need_color_reorder = SANE_TRUE;
	}

	/* FS F, request scanner status */
	if (dev->extended_commands) {
		unsigned char buf[16];

		status = esci_request_scanner_status(s, buf);
		if (status != SANE_STATUS_GOOD)
			return status;
	}

	return status;
}

/* attach device to backend */

static SANE_Status
attach(const char *name, Epson_Device * *devp, int type)
{
	SANE_Status status;
	Epson_Scanner *s;
	struct Epson_Device *dev;
	int port;

	DBG(1, "%s\n", SANE_EPSON2_VERSION);

	DBG(7, "%s: devname = %s, type = %d\n", __func__, name, type);

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

	dev->name = NULL;
	dev->model = NULL;

	dev->sane.name = NULL;
	dev->sane.model = NULL;

	dev->sane.type = "flatbed scanner";
	dev->sane.vendor = "Epson";

	dev->optical_res = 0;	/* just to have it initialized */
	dev->color_shuffle = SANE_FALSE;
	dev->extension = SANE_FALSE;
	dev->use_extension = SANE_FALSE;

	dev->need_color_reorder = SANE_FALSE;
	dev->need_double_vertical = SANE_FALSE;

	dev->cmd = &epson_cmd[EPSON_LEVEL_DEFAULT];	/* default function level */
	dev->connection = type;

	/* Change default level when using a network connection */
	if (dev->connection == SANE_EPSON_NET)
		dev->cmd = &epson_cmd[EPSON_LEVEL_B7];

	DBG(3, "%s: opening %s, type = %d\n", __func__, name,
	    dev->connection);

	dev->last_res = 0;
	dev->last_res_preview = 0;	/* set resolution to safe values */

	dev->res_list_size = 0;
	dev->res_list = NULL;

	if (dev->connection == SANE_EPSON_NET) {
		unsigned char buf[5];

		if (strncmp(name, "autodiscovery", 13) == 0) {
			e2_network_discovery();
			status = SANE_STATUS_INVAL;
			goto free;
		}

		status = sanei_tcp_open(name, 1865, &s->fd);
		if (status != SANE_STATUS_GOOD) {
			DBG(1, "%s: %s open failed: %s\n", __func__,
			    name, sane_strstatus(status));
			goto free;
		}

		s->netlen = 0;

		/* the scanner sends a kind of welcome msg */
		/* XXX use a shorter timeout here */
		e2_recv(s, buf, 5, &status);

		/* lock the scanner for use by sane */
		sanei_epson_net_lock(s);

	} else if (dev->connection == SANE_EPSON_SCSI) {
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
			dev->sane.type = "film scanner";
			e2_set_model(s, (unsigned char *) model, 12);
		}
	}
	/* use the SANEI functions to handle a PIO device */
	else if (dev->connection == SANE_EPSON_PIO) {
		if (SANE_STATUS_GOOD !=
		    (status = sanei_pio_open(name, &s->fd))) {
			DBG(1,
			    "cannot open %s as a parallel-port device: %s\n",
			    name, sane_strstatus(status));
			goto free;
		}
	}
	/* use the SANEI functions to handle a USB device */
	else if (dev->connection == SANE_EPSON_USB) {
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

	/* set name and model (if not already set) */
	if (dev->model == NULL)
		e2_set_model(s, (unsigned char *) "generic", 7);

	dev->name = strdup(name);
	dev->sane.name = dev->name;


	/* Issue a test unit ready SCSI command. The FilmScan 200
	 * requires it for a sort of "wake up". We might eventually
	 * get the return code and reissue it in case of failure.
	 */
	if (dev->connection == SANE_EPSON_SCSI)
		sanei_epson2_scsi_test_unit_ready(s->fd);

	/* ESC @, reset */
	esci_reset(s);

	status = e2_discover_capabilities(s);
	if (status != SANE_STATUS_GOOD)
		goto free;

	/* If we have been unable to obtain supported resolutions
	 * due to the fact we are on the network transport,
	 * add some convenient ones
	 */

	if (dev->res_list_size == 0 && dev->connection == SANE_EPSON_NET) {
		int val = 150;

		DBG(1, "networked scanner, faking resolution list\n");

		e2_add_resolution(s, 50);
		e2_add_resolution(s, 75);
		e2_add_resolution(s, 100);

		while (val <= dev->dpi_range.max) {
			e2_add_resolution(s, val);
			val *= 2;
		}
	}

	/*
	 * Copy the resolution list to the resolution_list array so that the frontend can
	 * display the correct values
	 */

	dev->resolution_list =
		malloc((dev->res_list_size + 1) * sizeof(SANE_Word));

	if (dev->resolution_list == NULL) {
		status = SANE_STATUS_NO_MEM;
		goto free;
	}

	*(dev->resolution_list) = dev->res_list_size;
	memcpy(&(dev->resolution_list[1]), dev->res_list,
	       dev->res_list_size * sizeof(SANE_Word));

	/* XXX necessary? */
	esci_reset(s);

	DBG(1, "scanner model: %s\n", dev->model);

	/* establish defaults */
	dev->need_reset_on_source_change = SANE_FALSE;

	if (e2_model(s, "ES-9000H") || e2_model(s, "GT-30000")) {
		dev->cmd->set_focus_position = 0;
		dev->cmd->feed = 0x19;
	}

	if (e2_model(s, "GT-8200") || e2_model(s, "Perfection1650")
	    || e2_model(s, "Perfection1640") || e2_model(s, "GT-8700")) {
		dev->cmd->feed = 0;
		dev->cmd->set_focus_position = 0;
		dev->need_reset_on_source_change = SANE_TRUE;
	}

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
	DBG(7, "%s: dev = %s\n", __func__, dev);
	return attach(dev, 0, SANE_EPSON_SCSI);
}

SANE_Status
attach_one_usb(const char *dev)
{
	DBG(7, "%s: dev = %s\n", __func__, dev);
	return attach(dev, 0, SANE_EPSON_USB);
}

static SANE_Status
attach_one_net(const char *dev)
{
	DBG(7, "%s: dev = %s\n", __func__, dev);
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
	DBG(2, "%s: " PACKAGE " " VERSION "\n", __func__);

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
				numIds = sanei_epson_getNumberOfUSBProductIds
					();
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
		free(dev->name);
		free(dev->model);
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

	local_only = local_only;	/* just to get rid of the compailer warning */

	if (devlist)
		free(devlist);

	devlist = malloc((num_devices + 1) * sizeof(devlist[0]));
	if (!devlist) {
		DBG(1, "out of memory (line %d)\n", __LINE__);
		return SANE_STATUS_NO_MEM;
	}

	for (i = 0, dev = first_dev; i < num_devices; dev = dev->next, i++) {
		DBG(1, " %d: %s\n", i, dev->model);
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
	s->opt[OPT_NUM_OPTS].type = SANE_TYPE_INT;
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

	if (!s->hw->cmd->set_outline_emphasis)
		s->opt[OPT_SHARPNESS].cap |= SANE_CAP_INACTIVE;

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

/*		s->opt[ OPT_GAMMA_VECTOR].cap &= ~SANE_CAP_INACTIVE; */
		s->opt[OPT_GAMMA_VECTOR_R].cap &= ~SANE_CAP_INACTIVE;
		s->opt[OPT_GAMMA_VECTOR_G].cap &= ~SANE_CAP_INACTIVE;
		s->opt[OPT_GAMMA_VECTOR_B].cap &= ~SANE_CAP_INACTIVE;
	} else {

/*		s->opt[ OPT_GAMMA_VECTOR].cap |= SANE_CAP_INACTIVE; */
		s->opt[OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
		s->opt[OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
		s->opt[OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;
	}

	/* initialize the Gamma tables */
	memset(&s->gamma_table[0], 0, 256 * sizeof(SANE_Word));
	memset(&s->gamma_table[1], 0, 256 * sizeof(SANE_Word));
	memset(&s->gamma_table[2], 0, 256 * sizeof(SANE_Word));

/*	memset(&s->gamma_table[3], 0, 256 * sizeof(SANE_Word)); */
	for (i = 0; i < 256; i++) {
		s->gamma_table[0][i] = i;
		s->gamma_table[1][i] = i;
		s->gamma_table[2][i] = i;

/*		s->gamma_table[3][i] = i; */
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

	s->opt[OPT_CCT_1].cap |= SANE_CAP_ADVANCED | SANE_CAP_INACTIVE;
	s->opt[OPT_CCT_2].cap |= SANE_CAP_ADVANCED | SANE_CAP_INACTIVE;
	s->opt[OPT_CCT_3].cap |= SANE_CAP_ADVANCED | SANE_CAP_INACTIVE;
	s->opt[OPT_CCT_4].cap |= SANE_CAP_ADVANCED | SANE_CAP_INACTIVE;
	s->opt[OPT_CCT_5].cap |= SANE_CAP_ADVANCED | SANE_CAP_INACTIVE;
	s->opt[OPT_CCT_6].cap |= SANE_CAP_ADVANCED | SANE_CAP_INACTIVE;
	s->opt[OPT_CCT_7].cap |= SANE_CAP_ADVANCED | SANE_CAP_INACTIVE;
	s->opt[OPT_CCT_8].cap |= SANE_CAP_ADVANCED | SANE_CAP_INACTIVE;
	s->opt[OPT_CCT_9].cap |= SANE_CAP_ADVANCED | SANE_CAP_INACTIVE;

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

	if (!s->hw->cmd->mirror_image)
		s->opt[OPT_MIRROR].cap |= SANE_CAP_INACTIVE;

	/* auto area segmentation */
	s->opt[OPT_AAS].name = "auto-area-segmentation";
	s->opt[OPT_AAS].title = SANE_I18N("Auto area segmentation");
	s->opt[OPT_AAS].desc =
		"Enables different dithering modes in image and text areas";

	s->opt[OPT_AAS].type = SANE_TYPE_BOOL;
	s->val[OPT_AAS].w = SANE_TRUE;

	if (!s->hw->cmd->control_auto_area_segmentation)
		s->opt[OPT_AAS].cap |= SANE_CAP_INACTIVE;

	/* limit resolution list */
	s->opt[OPT_LIMIT_RESOLUTION].name = "short-resolution";
	s->opt[OPT_LIMIT_RESOLUTION].title =
		SANE_I18N("Short resolution list");
	s->opt[OPT_LIMIT_RESOLUTION].desc =
		SANE_I18N("Display a shortened resolution list");
	s->opt[OPT_LIMIT_RESOLUTION].type = SANE_TYPE_BOOL;
	s->val[OPT_LIMIT_RESOLUTION].w = SANE_FALSE;


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

	if (!s->hw->extension)
		s->opt[OPT_SOURCE].cap |= SANE_CAP_INACTIVE;

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


	s->opt[OPT_ADF_MODE].name = "adf-mode";
	s->opt[OPT_ADF_MODE].title = SANE_I18N("ADF Mode");
	s->opt[OPT_ADF_MODE].desc =
		SANE_I18N("Selects the ADF mode (simplex/duplex)");
	s->opt[OPT_ADF_MODE].type = SANE_TYPE_STRING;
	s->opt[OPT_ADF_MODE].size = max_string_size(adf_mode_list);
	s->opt[OPT_ADF_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
	s->opt[OPT_ADF_MODE].constraint.string_list = adf_mode_list;
	s->val[OPT_ADF_MODE].w = 0;	/* simplex */

	if ((!s->hw->ADF) || (s->hw->duplex == SANE_FALSE))
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
	SANE_Status status;
	Epson_Device *dev;
	Epson_Scanner *s;

	DBG(7, "%s: name = %s\n", __func__, name);

	/* search for device */
	if (name[0]) {
		for (dev = first_dev; dev; dev = dev->next)
			if (strcmp(dev->sane.name, name) == 0)
				break;
	} else
		dev = first_dev;

	if (!dev) {
		DBG(1, "error opening the device\n");
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

	status = open_scanner(s);
	if (status != SANE_STATUS_GOOD)
		return status;

	esci_reset(s);

	return status;
}

void
sane_close(SANE_Handle handle)
{
	int i;
	Epson_Scanner *s, *prev;

	/*
	 * XXX Test if there is still data pending from
	 * the scanner. If so, then do a cancel
	 */

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

	for (i = 0; i < LINES_SHUFFLE_MAX; i++) {
		if (s->line_buffer[i] != NULL)
			free(s->line_buffer[i]);
	}

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
	case OPT_BIT_DEPTH:
	case OPT_WAIT_FOR_BUTTON:
	case OPT_LIMIT_RESOLUTION:
		*((SANE_Word *) value) = sval->w;
		break;

	case OPT_MODE:
	case OPT_ADF_MODE:
	case OPT_HALFTONE:
	case OPT_DROPOUT:
	case OPT_SOURCE:
	case OPT_FILM_TYPE:
	case OPT_GAMMA_CORRECTION:
	case OPT_COLOR_CORRECTION:
	case OPT_BAY:
	case OPT_FOCUS:
		strcpy((char *) value, sopt->constraint.string_list[sval->w]);
		break;

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

	DBG(1, "%s: optindex = %d, source = '%s'\n", __func__, optindex,
	    value);

	/* reset the scanner when we are changing the source setting -
	   this is necessary for the Perfection 1650 */
	if (s->hw->need_reset_on_source_change)
		esci_reset(s);

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
		if (s->hw->duplex) {
			activateOption(s, OPT_ADF_MODE, &dummy);
		} else {
			deactivateOption(s, OPT_ADF_MODE, &dummy);
			s->val[OPT_ADF_MODE].w = 0;
		}

		DBG(1, "adf activated (%d %d)\n", s->hw->use_extension,
		    s->hw->duplex);

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
	if (s->hw->cmd->level[0] == 'F')
		activateOption(s, OPT_FILM_TYPE, &dummy);

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

	DBG(17, "%s: option = %d, value = %p\n", __func__, option, value);

	status = sanei_constrain_value(sopt, value, info);
	if (status != SANE_STATUS_GOOD)
		return status;

	if (info && value && (*info & SANE_INFO_INEXACT)
	    && sopt->type == SANE_TYPE_INT)
		DBG(17, "%s: constrained val = %d\n", __func__,
		    *(SANE_Word *) value);

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
		/* XXX required?  control_extension(s, 1); */
		esci_eject(s);
		break;

	case OPT_RESOLUTION:
		sval->w = *((SANE_Word *) value);
		DBG(17, "setting resolution to %d\n", sval->w);
		reload = SANE_TRUE;
		break;

	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
		sval->w = *((SANE_Word *) value);
		DBG(17, "setting size to %f\n", SANE_UNFIX(sval->w));
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
			color_userdefined[s->val[OPT_COLOR_CORRECTION].w];

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
				       && userDefined, OPT_CCT_1, &reload);
			setOptionState(s, isColor
				       && userDefined, OPT_CCT_2, &reload);
			setOptionState(s, isColor
				       && userDefined, OPT_CCT_3, &reload);
			setOptionState(s, isColor
				       && userDefined, OPT_CCT_4, &reload);
			setOptionState(s, isColor
				       && userDefined, OPT_CCT_5, &reload);
			setOptionState(s, isColor
				       && userDefined, OPT_CCT_6, &reload);
			setOptionState(s, isColor
				       && userDefined, OPT_CCT_7, &reload);
			setOptionState(s, isColor
				       && userDefined, OPT_CCT_8, &reload);
			setOptionState(s, isColor
				       && userDefined, OPT_CCT_9, &reload);
		}

		/* if binary, then disable the bit depth selection */
		if (optindex == 0) {
			s->opt[OPT_BIT_DEPTH].cap |= SANE_CAP_INACTIVE;
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
	case OPT_AAS:
	case OPT_PREVIEW:	/* needed? */
	case OPT_BRIGHTNESS:
	case OPT_SHARPNESS:
	case OPT_AUTO_EJECT:
	case OPT_THRESHOLD:
	case OPT_WAIT_FOR_BUTTON:
		sval->w = *((SANE_Word *) value);
		break;

	case OPT_LIMIT_RESOLUTION:
		sval->w = *((SANE_Word *) value);
		filter_resolution_list(s);
		reload = SANE_TRUE;
		break;

	default:
		return SANE_STATUS_INVAL;
	}

	if (reload && info != NULL)
		*info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;

	DBG(17, "%s: end\n", __func__);

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
e2_set_extended_scanning_parameters(Epson_Scanner * s)
{
	unsigned char buf[64];

	const struct mode_param *mparam;

	DBG(1, "%s\n", __func__);

	mparam = &mode_params[s->val[OPT_MODE].w];

	memset(buf, 0x00, sizeof(buf));

	/* ESC R, resolution */
	htole32a(&buf[0], s->val[OPT_RESOLUTION].w);
	htole32a(&buf[4], s->val[OPT_RESOLUTION].w);

	/* ESC A, scanning area */
	htole32a(&buf[8], s->left);
	htole32a(&buf[12], s->top);
	htole32a(&buf[16], s->params.pixels_per_line);
	htole32a(&buf[20], s->params.lines);

	/*
	 * The byte sequence mode was introduced in B5,
	 *for B[34] we need line sequence mode
	 */

	/* ESC C, set color */
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

	/* ESC D, set data format */
	mparam = &mode_params[s->val[OPT_MODE].w];
	buf[25] = mparam->depth;

	/* ESC e, control option */
	if (s->hw->extension) {

		char extensionCtrl;
		extensionCtrl = (s->hw->use_extension ? 1 : 0);
		if (s->hw->use_extension && (s->val[OPT_ADF_MODE].w == 1))
			extensionCtrl = 2;

		/* Test for TPU2
		 * Epson Perfection 4990 Command Specifications
		 * JZIS-0075 Rev. A, page 31
		 */
		if (s->hw->use_extension && s->hw->TPU2)
			extensionCtrl = 5;

		/* ESC e */
		buf[26] = extensionCtrl;

		/* XXX focus */
	}

	/* ESC g, scanning mode (normal or high speed) */
	if (s->val[OPT_PREVIEW].w)
		buf[27] = 1;	/* High speed */
	else
		buf[27] = 0;

	/* ESC d, block line number */
	buf[28] = s->lcount;

	/* ESC Z, set gamma correction */
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

	/* ESC L, set brightness */
	if (SANE_OPTION_IS_ACTIVE(s->opt[OPT_BRIGHTNESS].cap))
		buf[30] = s->val[OPT_BRIGHTNESS].w;

	/* ESC B, set halftoning mode / halftone processing */
	if (SANE_OPTION_IS_ACTIVE(s->opt[OPT_HALFTONE].cap))
		buf[32] = halftone_params[s->val[OPT_HALFTONE].w];

	/* ESC s, auto area segmentation */
	if (SANE_OPTION_IS_ACTIVE(s->opt[OPT_AAS].cap))
		buf[34] = s->val[OPT_AAS].w;

	/* ESC Q, set sharpness / sharpness control */
	if (SANE_OPTION_IS_ACTIVE(s->opt[OPT_SHARPNESS].cap))
		buf[35] = s->val[OPT_SHARPNESS].w;

	/* ESC K, set data order / mirroring */
	if (SANE_OPTION_IS_ACTIVE(s->opt[OPT_MIRROR].cap))
		buf[36] = mirror_params[s->val[OPT_MIRROR].w];

	s->invert_image = SANE_FALSE;	/* default: do no invert the image */

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

	return esci_set_scanning_parameter(s, buf);
}

static SANE_Status
e2_set_scanning_parameters(Epson_Scanner * s)
{
	SANE_Status status;
	struct mode_param *mparam = &mode_params[s->val[OPT_MODE].w];
	unsigned char color_mode;

	DBG(1, "%s\n", __func__);

	/*
	 *  There is some undocumented special behavior with the TPU enable/disable.
	 *      TPU power       ESC e      status
	 *      on            0        NAK
	 *      on            1        ACK
	 *      off          0         ACK
	 *      off          1         NAK
	 *
	 * It makes no sense to scan with TPU powered on and source flatbed, because
	 * light will come from both sides.
	 */

	if (s->hw->extension) {

		int extensionCtrl;
		extensionCtrl = (s->hw->use_extension ? 1 : 0);
		if (s->hw->use_extension && (s->val[OPT_ADF_MODE].w == 1))
			extensionCtrl = 2;

		status = esci_control_extension(s, extensionCtrl);
		if (status != SANE_STATUS_GOOD) {
			DBG(1, "you may have to power %s your TPU\n",
			    s->hw->use_extension ? "on" : "off");
			DBG(1,
			    "and you may also have to restart the SANE frontend.\n");
			return status;
		}

		/* XXX use request_extended_status and analyze
		 * buffer to set the scan area for
		 * ES-9000H and GT-30000
		 */

		/*
		 * set the focus position according to the extension used:
		 * if the TPU is selected, then focus 2.5mm above the glass,
		 * otherwise focus on the glass. Scanners that don't support
		 * this feature, will just ignore these calls.
		 */

		if (s->hw->focusSupport == SANE_TRUE) {
			if (s->val[OPT_FOCUS].w == 0) {
				DBG(1, "setting focus to glass surface\n");
				esci_set_focus_position(s, 0x40);
			} else {
				DBG(1,
				    "setting focus to 2.5mm above glass\n");
				esci_set_focus_position(s, 0x59);
			}
		}
	}

	/* ESC C, Set color */
	color_mode = mparam->flags | (mparam->dropout_mask
				      & dropout_params[s->val[OPT_DROPOUT].
						       w]);

	/*
	 * The byte sequence mode was introduced in B5, for B[34] we need line sequence mode
	 * XXX Check what to do for the FilmScan 200
	 */
	if ((s->hw->cmd->level[0] == 'D'
	     || (s->hw->cmd->level[0] == 'B' && s->hw->level >= 5))
	    && mparam->flags == 0x02)
		color_mode = 0x13;

	status = esci_set_color_mode(s, color_mode);
	if (status != SANE_STATUS_GOOD)
		return status;

	/* ESC D, set data format */
	DBG(1, "%s: setting data format to %d bits\n", __func__,
	    mparam->depth);
	status = esci_set_data_format(s, mparam->depth);
	if (status != SANE_STATUS_GOOD)
		return status;

	/* ESC B, set halftoning mode */
	if (s->hw->cmd->set_halftoning
	    && SANE_OPTION_IS_ACTIVE(s->opt[OPT_HALFTONE].cap)) {
		status = esci_set_halftoning(s,
					     halftone_params[s->
							     val
							     [OPT_HALFTONE].
							     w]);
		if (status != SANE_STATUS_GOOD)
			return status;
	}

	/* ESC L, set brightness */
	if (SANE_OPTION_IS_ACTIVE(s->opt[OPT_BRIGHTNESS].cap)) {
		status = esci_set_bright(s, s->val[OPT_BRIGHTNESS].w);
		if (status != SANE_STATUS_GOOD)
			return status;
	}

	if (SANE_OPTION_IS_ACTIVE(s->opt[OPT_AAS].cap)) {
		status = esci_set_auto_area_segmentation(s,
							 s->val[OPT_AAS].w);
		if (status != SANE_STATUS_GOOD)
			return status;
	}

	s->invert_image = SANE_FALSE;	/* default: to not inverting the image */

	if (SANE_OPTION_IS_ACTIVE(s->opt[OPT_FILM_TYPE].cap)) {
		s->invert_image =
			(s->val[OPT_FILM_TYPE].w == FILM_TYPE_NEGATIVE);
		status = esci_set_film_type(s,
					    film_params[s->val[OPT_FILM_TYPE].
							w]);
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

		status = esci_set_gamma(s, val);
		if (status != SANE_STATUS_GOOD)
			return status;
	}

	if (s->hw->cmd->set_threshold != 0
	    && SANE_OPTION_IS_ACTIVE(s->opt[OPT_THRESHOLD].cap)) {
		status = esci_set_threshold(s, s->val[OPT_THRESHOLD].w);
		if (status != SANE_STATUS_GOOD)
			return status;
	}

	/* XXX ESC Z here */

	/* ESC M, set color correction */
	if (SANE_OPTION_IS_ACTIVE(s->opt[OPT_COLOR_CORRECTION].cap)) {
		status = esci_set_color_correction(s,
						   color_params[s->
								val
								[OPT_COLOR_CORRECTION].
								w]);
		if (status != SANE_STATUS_GOOD)
			return status;
	}

	/* ESC Q, set sharpness */
	if (SANE_OPTION_IS_ACTIVE(s->opt[OPT_SHARPNESS].cap)) {

		status = esci_set_sharpness(s, s->val[OPT_SHARPNESS].w);
		if (status != SANE_STATUS_GOOD)
			return status;
	}

	/* ESC g, set scanning mode */
	if (s->val[OPT_PREVIEW].w)
		status = esci_set_speed(s, 1);
	else
		status = esci_set_speed(s, 0);

	if (status != SANE_STATUS_GOOD)
		return status;

	/* ESC K, set data order */
	if (SANE_OPTION_IS_ACTIVE(s->opt[OPT_MIRROR].cap)) {
		status = esci_mirror_image(s, mirror_params[s->val[OPT_MIRROR].w]);
		if (status != SANE_STATUS_GOOD)
			return status;
	}

	/* ESC R */
	status = esci_set_resolution(s, s->val[OPT_RESOLUTION].w,
				     s->val[OPT_RESOLUTION].w);
	if (status != SANE_STATUS_GOOD)
		return status;

	/* ESC H, set zoom */
	/* not implemented */

	/* ESC A, set scanning area */
	status = esci_set_scan_area(s, s->left, s->top,
				    s->params.pixels_per_line,
				    s->params.lines);

	if (status != SANE_STATUS_GOOD)
		return status;

	/* ESC d, set block line number / set line counter */
	status = esci_set_lcount(s, s->lcount);
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

		DBG(5, "resolution = %d, preview = %d\n",
		    s->val[OPT_RESOLUTION].w, s->val[OPT_PREVIEW].w);

		DBG(5, "get para tlx %f tly %f brx %f bry %f [mm]\n",
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
		(SANE_UNFIX(s->val[OPT_BR_X].w -
			    s->val[OPT_TL_X].w) / (MM_PER_INCH * dpi)) + 0.5;
	s->params.lines =
		(SANE_UNFIX(s->val[OPT_BR_Y].w -
			    s->val[OPT_TL_Y].w) / (MM_PER_INCH * dpi)) + 0.5;

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

	DBG(5, "resolution = %d, preview = %d\n",
	    s->val[OPT_RESOLUTION].w, s->val[OPT_PREVIEW].w);

	DBG(5, "get para %p %p tlx %f tly %f brx %f bry %f [mm]\n",
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

static void
e2_setup_block_mode(Epson_Scanner * s)
{
	s->block = SANE_TRUE;
	s->lcount = sanei_scsi_max_request_size / s->params.bytes_per_line;

	/* XXX Check if we can do this with other scanners,
	 * by bus type
	 */
	DBG(1, "max req size: %d\n", sanei_scsi_max_request_size);

	if (s->lcount < 3 && e2_model(s, "GT-X800")) {
		s->lcount = 21;
		DBG(17,
		    "%s: set lcount = %i bigger than sanei_scsi_max_request_size\n",
		    __func__, s->lcount);
	}

	if (s->lcount >= 255) {
		s->lcount = 255;
	}

	/* XXX why this? */
	if (s->hw->TPU && s->hw->use_extension && s->lcount > 32) {
		s->lcount = 32;
	}

	/*
	 * The D1 series of scanners only allow an even line number
	 * for bi-level scanning. If a bit depth of 1 is selected, then
	 * make sure the next lower even number is selected.
	 */
	if (s->lcount > 3 && s->lcount % 2) {
		s->lcount -= 1;
	}

	DBG(1, "line count is %d\n", s->lcount);
}


static SANE_Status
e2_init_parameters(Epson_Scanner * s)
{
	int dpi, max_y, max_x, bytes_per_pixel;
	struct mode_param *mparam;

	memset(&s->params, 0, sizeof(SANE_Parameters));

	max_x = max_y = 0;
	dpi = s->val[OPT_RESOLUTION].w;

	mparam = &mode_params[s->val[OPT_MODE].w];

	s->left = SANE_UNFIX(s->val[OPT_TL_X].w) / MM_PER_INCH *
		s->val[OPT_RESOLUTION].w + 0.5;

	s->top = SANE_UNFIX(s->val[OPT_TL_Y].w) / MM_PER_INCH *
		s->val[OPT_RESOLUTION].w + 0.5;

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
	    __func__, (void *) s, (void *) s->val,
	    SANE_UNFIX(s->val[OPT_TL_X].w), SANE_UNFIX(s->val[OPT_TL_Y].w),
	    SANE_UNFIX(s->val[OPT_BR_X].w), SANE_UNFIX(s->val[OPT_BR_Y].w));

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

	/* this works because it can only be set to 1, 8 or 16 */
	bytes_per_pixel = s->params.depth / 8;
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

		/* start the scan 2 * line_distance earlier */
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

	if ((s->hw->cmd->level[0] == 'B') && (s->hw->level >= 5))
		e2_setup_block_mode(s);
	else if ((s->hw->cmd->level[0] == 'B') && (s->hw->level == 4)
		 && (!mode_params[s->val[OPT_MODE].w].color))
		e2_setup_block_mode(s);
	else if (s->hw->cmd->level[0] == 'D')
		e2_setup_block_mode(s);

	print_params(s->params);

	return SANE_STATUS_GOOD;
}

static void
e2_wait_button(Epson_Scanner * s)
{
	DBG(5, "%s\n", __func__);

	s->hw->wait_for_button = SANE_TRUE;

	while (s->hw->wait_for_button == SANE_TRUE) {
		unsigned char button_status = 0;

		if (s->canceling == SANE_TRUE) {
			s->hw->wait_for_button = SANE_FALSE;
		}
		/* get the button status from the scanner */
		else if (esci_request_push_button_status(s, &button_status) ==
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


static SANE_Status
e2_check_warm_up(Epson_Scanner * s, SANE_Bool * wup)
{
	SANE_Status status;

	DBG(5, "%s\n", __func__);

	*wup = SANE_FALSE;

	if (s->hw->extended_commands) {
		unsigned char buf[16];

		status = esci_request_scanner_status(s, buf);
		if (status != SANE_STATUS_GOOD)
			return status;

		if (buf[0] & FSF_STATUS_MAIN_WU)
			*wup = SANE_TRUE;

	} else {
		unsigned char *es;

		/* this command is not available on some scanners */
		if (!s->hw->cmd->request_extended_status)
			return SANE_STATUS_GOOD;

		status = esci_request_extended_status(s, &es, NULL);
		if (status != SANE_STATUS_GOOD)
			return status;

		if (es[0] & EXT_STATUS_WU)
			*wup = SANE_TRUE;

		free(es);
	}

	return status;
}

static SANE_Status
e2_wait_warm_up(Epson_Scanner * s)
{
	SANE_Status status;
	SANE_Bool wup;

	DBG(5, "%s\n", __func__);

	s->retry_count = 0;

	while (1) {
		status = e2_check_warm_up(s, &wup);
		if (status != SANE_STATUS_GOOD)
			return status;

		if (wup == SANE_FALSE)
			break;

		s->retry_count++;

		if (s->retry_count > SANE_EPSON_MAX_RETRIES) {
			DBG(1, "max retry count exceeded (%d)\n",
			    s->retry_count);
			return SANE_STATUS_DEVICE_BUSY;
		}
		sleep(5);
	}

	return SANE_STATUS_GOOD;
}

static SANE_Status
e2_check_adf(Epson_Scanner * s)
{
	SANE_Status status;

	DBG(5, "%s\n", __func__);

	if (s->hw->use_extension == SANE_FALSE)
		return SANE_STATUS_GOOD;

	if (s->hw->extended_commands) {
		unsigned char buf[16];

		status = esci_request_scanner_status(s, buf);
		if (status != SANE_STATUS_GOOD)
			return status;

		if (buf[1] & FSF_STATUS_ADF_PE)
			return SANE_STATUS_NO_DOCS;

		if (buf[1] & FSF_STATUS_ADF_PJ)
			return SANE_STATUS_JAMMED;

	} else {
		unsigned char *buf, t;

		status = esci_request_extended_status(s, &buf, NULL);
		if (status != SANE_STATUS_GOOD)
			return status;;

		t = buf[1];

		free(buf);

		if (t & EXT_STATUS_PE)
			return SANE_STATUS_NO_DOCS;

		if (t & EXT_STATUS_PJ)
			return SANE_STATUS_JAMMED;
	}

	return SANE_STATUS_GOOD;
}

static SANE_Status
e2_start_std_scan(Epson_Scanner * s)
{
	SANE_Status status;
	unsigned char params[2];

	DBG(5, "%s\n", __func__);

	/* ESC g */
	params[0] = ESC;
	params[1] = s->hw->cmd->start_scanning;

	e2_send(s, params, 2, 6 + (s->lcount * s->params.bytes_per_line),
		&status);
	return status;
}

static SANE_Status
e2_start_ext_scan(Epson_Scanner * s)
{
	SANE_Status status;
	unsigned char params[2];
	unsigned char buf[14];

	DBG(5, "%s\n", __func__);

	params[0] = FS;
	params[1] = 'G';

	status = e2_txrx(s, params, 2, buf, 14);
	if (status != SANE_STATUS_GOOD)
		return status;

	if (buf[0] != STX)
		return SANE_STATUS_INVAL;

	if (buf[1] & 0x80) {
		DBG(1, "%s: fatal error\n", __func__);
		return SANE_STATUS_IO_ERROR;
	}

	s->ext_block_len = le32atoh(&buf[2]);
	s->ext_blocks = le32atoh(&buf[6]);
	s->ext_last_len = le32atoh(&buf[10]);

	s->ext_counter = 0;

	DBG(5, " status         : 0x%02x\n", buf[1]);
	DBG(5, " block size     : %lu\n", (u_long) le32atoh(&buf[2]));
	DBG(5, " block count    : %lu\n", (u_long) le32atoh(&buf[6]));
	DBG(5, " last block size: %lu\n", (u_long) le32atoh(&buf[10]));

	if (s->ext_last_len) {
		s->ext_blocks++;
		DBG(1, "adj block count: %d\n", s->ext_blocks);
	}

	/* adjust block len if we have only one block to read */
	if (s->ext_block_len == 0 && s->ext_last_len)
		s->ext_block_len = s->ext_last_len;

	return status;
}

/* Helper function to correct the error for warmup lamp
 * gotten from scanners with known buggy firmware.
 * Epson Perfection 4990 Photo
 */

static SANE_Status
fix_warmup_lamp(Epson_Scanner * s, SANE_Status status)
{
	/*
	 * Check for Perfection 4990 photo/GT-X800 scanner.
	 * Scanner sometimes report "Fatal error" in status in informationblock when
	 * lamp warm up. Solution send FS G one more time.
	 */
	if (e2_model(s, "GT-X800")) {
		SANE_Status status2;

		DBG(1, "%s: Epson Perfection 4990 lamp warm up problem \n",
		    __func__);
		status2 = e2_wait_warm_up(s);
		if (status2 == SANE_STATUS_GOOD) {
			status = e2_start_ext_scan(s);
			return status;
		}
	}

	return status;
}



/*
 * This function is part of the SANE API and gets called from the front end to
 * start the scan process.
 */

SANE_Status
sane_start(SANE_Handle handle)
{
	Epson_Scanner *s = (Epson_Scanner *) handle;
	Epson_Device *dev = s->hw;
	SANE_Status status;

	DBG(5, "%s\n", __func__);

	/* check if we just have finished working with the ADF */
	status = e2_check_adf(s);
	if (status != SANE_STATUS_GOOD)
		return status;

	/* calc scanning parameters */
	e2_init_parameters(s);

	/* ESC , bay */
	if (SANE_OPTION_IS_ACTIVE(s->opt[OPT_BAY].cap)) {
		status = esci_set_bay(s, s->val[OPT_BAY].w);
		if (status != SANE_STATUS_GOOD)
			return status;
	}

	/* set scanning parameters */
	if (dev->extended_commands) {
		status = e2_set_extended_scanning_parameters(s);
	} else
		status = e2_set_scanning_parameters(s);

	if (status != SANE_STATUS_GOOD)
		return status;

	/* ESC z, user defined gamma table */
	if (dev->cmd->set_gamma_table
	    && gamma_userdefined[s->val[OPT_GAMMA_CORRECTION].w]) {
		status = esci_set_gamma_table(s);
		if (status != SANE_STATUS_GOOD)
			return status;
	}

	/* ESC m, user defined color correction */
	if (s->val[OPT_COLOR_CORRECTION].w == 1) {
		status = esci_set_color_correction_coefficients(s);
		if (status != SANE_STATUS_GOOD)
			return status;
	}

	/* check if we just have finished working with the ADF.
	 * this seems to work only after the scanner has been
	 * set up with scanning parameters
	 */
	status = e2_check_adf(s);
	if (status != SANE_STATUS_GOOD)
		return status;


/*
	status = sane_get_parameters(handle, NULL);
	if (status != SANE_STATUS_GOOD)
		return status;
*/
	/*
	 * If WAIT_FOR_BUTTON is active, then do just that:
	 * Wait until the button is pressed. If the button was already
	 * pressed, then we will get the button pressed event right away.
	 */
	if (s->val[OPT_WAIT_FOR_BUTTON].w == SANE_TRUE)
		e2_wait_button(s);

	/* for debug, request command parameter */
/*	if (DBG_LEVEL) {
		unsigned char buf[45];
		request_command_parameter(s, buf);
	}
*/
	/* set the retry count to 0 */
	s->retry_count = 0;


	/* allocate buffers for color shuffling */
	if (dev->color_shuffle == SANE_TRUE) {
		int i;
		/* initialize the line buffers */
		for (i = 0; i < s->line_distance * 2 + 1; i++) {
			if (s->line_buffer[i] != NULL)
				free(s->line_buffer[i]);

			s->line_buffer[i] = malloc(s->params.bytes_per_line);
			if (s->line_buffer[i] == NULL) {
				DBG(1, "out of memory (line %d)\n", __LINE__);
				return SANE_STATUS_NO_MEM;
			}
		}
	}

	/* prepare buffer here so that a memory allocation failure
	 * will leave the scanner in a sane state.
	 * the buffer will have to hold the image data plus
	 * an error code in the extended handshaking mode.
	 */
	s->buf = realloc(s->buf, (s->lcount * s->params.bytes_per_line) + 1);
	if (s->buf == NULL)
		return SANE_STATUS_NO_MEM;

	s->eof = SANE_FALSE;
	s->ptr = s->end = s->buf;
	s->canceling = SANE_FALSE;

	/* feed the first sheet in the ADF */
	if (dev->ADF && dev->use_extension && dev->cmd->feed) {
		status = esci_feed(s);
		if (status != SANE_STATUS_GOOD)
			return status;
	}

	/* this seems to work only for some devices */
	status = e2_wait_warm_up(s);
	if (status != SANE_STATUS_GOOD)
		return status;

	/* start scanning */
	DBG(1, "%s: scanning...\n", __func__);

	if (dev->extended_commands) {
		status = e2_start_ext_scan(s);

		/* this is a kind of read request */
		if (dev->connection == SANE_EPSON_NET)
			sanei_epson_net_write(s, 0x2000, NULL, 0,
					      s->ext_block_len + 1, &status);
	} else
		status = e2_start_std_scan(s);

	if (status != SANE_STATUS_GOOD) {
		DBG(1, "%s: start failed: %s\n", __func__,
		    sane_strstatus(status));
		if (status == SANE_STATUS_IO_ERROR)
			status = fix_warmup_lamp(s, status);
	}

	return status;
}

/* XXX this routine is ugly and should be avoided */
static SANE_Status
read_info_block(Epson_Scanner * s, EpsonDataRec * result)
{
	SANE_Status status;
	unsigned char params[2];

      retry:
	e2_recv(s, result, s->block ? 6 : 4, &status);
	if (status != SANE_STATUS_GOOD)
		return status;

	if (result->code != STX) {
		DBG(1, "error: got %02x, expected STX\n", result->code);
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
		status = esci_request_extended_status(s, &ext_status, NULL);
		if (status != SANE_STATUS_GOOD)
			return status;

		if (ext_status[0] & EXT_STATUS_WU) {
			free(ext_status);

			sleep(5);	/* for the next attempt */

			DBG(1, "retrying ESC G - %d\n", ++(s->retry_count));

			params[0] = ESC;
			params[1] = s->hw->cmd->start_scanning;

			e2_send(s, params, 2, 0, &status);
			if (status != SANE_STATUS_GOOD)
				return status;

			goto retry;
		} else
			free(ext_status);
	}

	return status;
}

static void
e2_scan_finish(Epson_Scanner * s)
{
	DBG(5, "%s\n", __func__);

	free(s->buf);
	s->buf = NULL;

	if (s->hw->ADF && s->hw->use_extension && s->val[OPT_AUTO_EJECT].w)
		if (e2_check_adf(s) == SANE_STATUS_NO_DOCS)
			esci_eject(s);

	/* XXX required? */
	esci_reset(s);
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

static void
e2_copy_image_data(Epson_Scanner * s, SANE_Byte * data, SANE_Int max_length,
		   SANE_Int * length)
{
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
}

static SANE_Status
e2_ext_sane_read(SANE_Handle handle)
{
	Epson_Scanner *s = (Epson_Scanner *) handle;
	SANE_Status status = SANE_STATUS_GOOD;
	size_t buf_len = 0, read;

	/* did we passed everything we read to sane? */
	if (s->ptr == s->end) {

		if (s->eof)
			return SANE_STATUS_EOF;

		s->ext_counter++;

		/* sane has already got the data, read some more, the final
		 * error byte must not be included in buf_len
		 */
		buf_len = s->ext_block_len;

		if (s->ext_counter == s->ext_blocks && s->ext_last_len)
			buf_len = s->ext_last_len;

		DBG(18, "%s: block %d, size %lu\n", __func__, s->ext_counter,
		    (u_long) buf_len);

		/* receive image data + error code */
		read = e2_recv(s, s->buf, buf_len + 1, &status);

		DBG(18, "%s: read %lu bytes\n", __func__, (u_long) read);

		if (read != buf_len + 1)
			return SANE_STATUS_IO_ERROR;

		/* XXX check error code in buf[buf_len]
		   if (s->buf[buf_len] & ...
		 */

		/* ack every block except the last one */
		if (s->ext_counter < s->ext_blocks) {
			size_t next_len = s->ext_block_len;

			if (s->ext_counter == (s->ext_blocks - 1))
				next_len = s->ext_last_len;

			status = e2_ack_next(s, next_len + 1);
		} else
			s->eof = SANE_TRUE;

		s->end = s->buf + buf_len;
		s->ptr = s->buf;
	}

	return status;
}

static SANE_Status
e2_block_sane_read(SANE_Handle handle)
{
	Epson_Scanner *s = (Epson_Scanner *) handle;
	SANE_Status status;
	SANE_Bool reorder = SANE_FALSE;
	SANE_Bool needStrangeReorder = SANE_FALSE;

      START_READ:
	DBG(18, "%s: begin\n", __func__);

	if (s->ptr == s->end) {
		EpsonDataRec result;
		size_t buf_len;

		if (s->eof) {
			if (s->hw->color_shuffle) {
				DBG(1,
				    "written %d lines after color shuffle\n",
				    s->lines_written);
				DBG(1, "lines requested: %d\n",
				    s->params.lines);
			}

			return SANE_STATUS_EOF;
		}

		status = read_info_block(s, &result);
		if (status != SANE_STATUS_GOOD) {
			return status;
		}

		buf_len = result.buf[1] << 8 | result.buf[0];
		buf_len *= (result.buf[3] << 8 | result.buf[2]);

		DBG(18, "%s: buf len = %lu\n", __func__, (u_long) buf_len);

		{
			/* do we have to reorder the data ? */
			if (get_color(result.status) == 0x01)
				reorder = SANE_TRUE;

			e2_recv(s, s->buf, buf_len, &status);
			if (status != SANE_STATUS_GOOD) {
				return status;
			}
		}

		if (result.status & STATUS_AREA_END) {
			DBG(1, "%s: EOF\n", __func__);
			s->eof = SANE_TRUE;
		} else {
			if (s->canceling) {
				status = e2_cmd_simple(s, S_CAN, 1);
				return SANE_STATUS_CANCELLED;
			} else {
				status = e2_ack(s);
			}
		}

		s->end = s->buf + buf_len;
		s->ptr = s->buf;

		/*
		 * if we have to re-order the color components (GRB->RGB) we
		 * are doing this here:
		 */

		/*
		 * Some scanners (e.g. the Perfection 1640 and GT-2200) seem
		 * to have the R and G channels swapped.
		 * The GT-8700 is the Asian version of the Perfection 1640.
		 * If the scanner name is one of these and the scan mode is
		 * RGB then swap the colors.
		 */

		needStrangeReorder =
			(strstr(s->hw->model, "GT-2200") ||
			 ((strstr(s->hw->model, "1640")
			   && strstr(s->hw->model, "Perfection"))
			  || strstr(s->hw->model, "GT-8700")))
			&& s->params.format == SANE_FRAME_RGB;

		/*
		 * Certain Perfection 1650 also need this re-ordering of the two
		 * color channels. These scanners are identified by the problem
		 * with the half vertical scanning area. When we corrected this,
		 * we also set the variable s->hw->need_color_reorder
		 */
		if (s->hw->need_color_reorder)
			reorder = SANE_FALSE;	/* reordering once is enough */

		if (reorder && s->params.format == SANE_FRAME_RGB) {
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

		DBG(18, "%s: begin scan2\n", __func__);
	}

	DBG(18, "%s: end\n", __func__);

	return SANE_STATUS_GOOD;
}


SANE_Status
sane_read(SANE_Handle handle, SANE_Byte * data, SANE_Int max_length,
	  SANE_Int * length)
{
	SANE_Status status;
	Epson_Scanner *s = (Epson_Scanner *) handle;

	*length = 0;

	if (s->hw->extended_commands)
		status = e2_ext_sane_read(handle);
	else
		status = e2_block_sane_read(handle);

	DBG(18, "moving data\n");
	e2_copy_image_data(s, data, max_length, length);

	/* continue reading if appropriate */
	if (status == SANE_STATUS_GOOD)
		return status;

	e2_scan_finish(s);

	return status;
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
	SANE_Status status = SANE_STATUS_GOOD;

	/*
	 * If the s->ptr pointer is not NULL, then a scan operation
	 * was started and if s->eof is FALSE, it was not finished.
	 */

	if (s->buf) {
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

			/* XXX check this condition, we used to check
			 * for SANE_STATUS_CANCELLED  */
			while (!s->eof &&
			       (status == SANE_STATUS_GOOD
				|| status == SANE_STATUS_DEVICE_BUSY)) {
				/* empty body, the while condition does the processing */
				status = sane_read(s, dummy,
						   s->params.bytes_per_line,
						   &len);
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

		for (i = 0; i < s->hw->res_list_size; i++) {
			SANE_Word res;
			res = s->hw->res_list[i];
			if ((res < 100) || res == 150 || (0 == (res % 300))
			    || (0 == (res % 400))) {
				/* add the value */
				new_size++;

				s->hw->resolution_list[new_size] =
					s->hw->res_list[i];

				/* check for a valid current resolution */
				if (res == s->val[OPT_RESOLUTION].w)
					is_correct_resolution = SANE_TRUE;
			}
		}
		s->hw->resolution_list[0] = new_size;

		if (is_correct_resolution == SANE_FALSE) {
			for (i = 1; i <= new_size; i++) {
				if (s->val[OPT_RESOLUTION].w <
				    s->hw->resolution_list[i]) {
					s->val[OPT_RESOLUTION].w =
						s->hw->resolution_list[i];
					break;
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
