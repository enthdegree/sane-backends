/* epson.c - SANE library for Epson flatbed scanners.

   based on Kazuhiro Sasayama previous
   Work on epson.[ch] file from the SANE package.

   original code taken from sane-0.71
   Copyright (C) 1997 Hypercore Software Design, Ltd.

   modifications
   Copyright (C) 1998-1999 Christian Bucher <bucher@vernetzt.at>
   Copyright (C) 1998-1999 Kling & Hautzinger GmbH
   Copyright (C) 1999 Norihiko Sawa <sawa@yb3.so-net.ne.jp>
   Copyright (C) 1999-2000 Karl Heinz Kremer <khk@khk.net>

   Version 0.1.15 Date 20-Feb-2000

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
   If you do not wish that, delete this exception notice.  */

/*
   2000-02-20   Added some cleanup on error conditions in attach()
                Use new sanei_config_read() instead of fgets() for
		compatibility with OS/2 (Yuri Dario)
   2000-02-19   Changed some "int" to "size_t" types
		Removed "Preview Resolution"
		Implemented resolution list as WORD_LIST instead of
		a RANGE (KHK)
   2000-02-11   Default scan source is always "Flatbed", regardless
		of installed options. Corrected some typos. (KHK)
   2000-02-03   Gamma curves now coupled with gamma correction menu.
		Only when "User defined" is selected are the curves
		selected. (Dave Hill)
		Renamed "Contrast" to "Gamma Correction" (KHK)
   2000-02-02   "Brown Paper Bag Release" Put the USB fix finally
		into the CVS repository.
   2000-02-01   Fixed problem with USB scanner not being recognized
		because of hte changes to attach a few days ago. (KHK)
   2000-01-29   fixed core dump with xscanimage by moving the gamma
		curves to the standard interface (no longer advanced)
   		Removed pragma pack() from source code to make it 
		easier to compile on non-gcc compilers (KHK)
   2000-01-26   fixed problem with resolution selection when using the
		resolution list in xsane (KHK)
   2000-01-25	moved the section where the device name is assigned 
		in attach. This avoids the core dump of frontend
		applications when no scanner is found (Dave Hill)
   2000-01-24	reorganization of SCSI related "helper" functions
		started support for user defined color correction -
		this is not yet available via the UI (Christian Bucher)
   2000-01-24	Removed C++ style comments '//' (KHK)
*/

#define	SANE_EPSON_VERSION	"SANE Epson Backend v0.1.15 - 2000-02-20"

#ifdef  _AIX
#	include  <lalloca.h>		/* MUST come first for AIX! */
#endif

#include  <sane/config.h>

#include  <lalloca.h>

#include  <limits.h>
#include  <stdio.h>
#include  <string.h>
#include  <stdlib.h>
#include  <ctype.h>
#include  <fcntl.h>
#include  <unistd.h>
#include  <errno.h>

#include  <sane/sane.h>
#include  <sane/saneopts.h>
#include  <sane/sanei_scsi.h>

/*
//  NOTE: try to isolate scsi stuff in own section.
//
*/

#define  TEST_UNIT_READY_COMMAND	0x00
#define  READ_6_COMMAND			0x08
#define  WRITE_6_COMMAND		0x0a
#define  INQUIRY_COMMAND		0x12
#define  TYPE_PROCESSOR			0x03

/*
//
//
*/

static SANE_Status inquiry ( int fd, int page_code, void * buf, size_t * buf_size) {
	unsigned char cmd [ 6];
	int status;

	memset( cmd, 0, 6);
	cmd[ 0] = INQUIRY_COMMAND;
	cmd[ 2] = page_code;
	cmd[ 4] = *buf_size > 255 ? 255 : *buf_size;
	status = sanei_scsi_cmd( fd, cmd, sizeof cmd, buf, buf_size);

	return status;
}

/*
//
//
*/

static int scsi_read ( int fd, void * buf, size_t buf_size, SANE_Status * status) {
	unsigned char cmd [ 6];

	memset( cmd, 0, 6);
	cmd[ 0] = READ_6_COMMAND;
	cmd[ 2] = buf_size >> 16;
	cmd[ 3] = buf_size >> 8;
	cmd[ 4] = buf_size;

	if( SANE_STATUS_GOOD == ( *status = sanei_scsi_cmd( fd, cmd, sizeof( cmd), buf, &buf_size)))
		return buf_size;

	return 0;
}

/*
//
//
*/

static int scsi_write ( int fd, const void * buf, size_t buf_size, SANE_Status * status) {
	unsigned char * cmd;

	cmd = alloca( 6 + buf_size);
	memset( cmd, 0, 6);
	cmd[ 0] = WRITE_6_COMMAND;
	cmd[ 2] = buf_size >> 16;
	cmd[ 3] = buf_size >> 8;
	cmd[ 4] = buf_size;
	memcpy( cmd + 6, buf, buf_size);

	if( SANE_STATUS_GOOD == ( *status = sanei_scsi_cmd( fd, cmd, 6 + buf_size, NULL, NULL)))
		return buf_size;

	return 0;
}

/*
//
//
*/

#include  <sane/sanei_pio.h>
#include  "epson.h"

#define  BACKEND_NAME	epson
#include  <sane/sanei_backend.h>

#include  <sane/sanei_config.h>
#define  EPSON_CONFIG_FILE	"epson.conf"

#ifndef  PATH_MAX
#	define  PATH_MAX	1024
#endif

#define  walloc(x)	( x *) malloc( sizeof( x) )
#define  walloca(x)	( x *) alloca( sizeof( x) )

#ifndef  XtNumber
#	define  XtNumber(x)  ( sizeof x / sizeof x [ 0] )
#	define  XtOffset(p_type,field)  ((size_t)&(((p_type)NULL)->field))
#	define  XtOffsetOf(s_type,field)  XtOffset(s_type*,field)
#endif

/* NOTE: you can find these codes with "man ascii". */
#define	 STX	0x02
#define	 ACK	0x06
#define	 NAK	0x15
#define	 CAN	0x18
#define	 ESC	0x1B

#define	 S_ACK	"\006"
#define	 S_CAN	"\030"

#define  STATUS_FER		0x80		/* fatal error */
#define  STATUS_AREA_END	0x20		/* area end */
#define  STATUS_OPTION		0x10		/* option installed */

#define  EXT_STATUS_FER		0x80		/* fatal error */
#define  EXT_STATUS_FBF		0x40		/* flat bed scanner */
#define  EXT_STATUS_PB		0x01		/* scanner has a push button */

#define  EXT_STATUS_IST		0x80		/* option detected */
#define  EXT_STATUS_EN		0x40		/* option enabled */
#define  EXT_STATUS_ERR		0x20		/* other error */
#define  EXT_STATUS_PE		0x08		/* no paper */
#define  EXT_STATUS_PJ		0x04		/* paper jam */
#define  EXT_STATUS_OPN		0x02		/* cover open */

#define	 EPSON_LEVEL_A1		 0
#define	 EPSON_LEVEL_A2		 1
#define	 EPSON_LEVEL_B1		 2
#define	 EPSON_LEVEL_B2		 3
#define	 EPSON_LEVEL_B3		 4
#define	 EPSON_LEVEL_B4		 5
#define	 EPSON_LEVEL_B5		 6
#define	 EPSON_LEVEL_B6		 7
#define	 EPSON_LEVEL_B7		 8
#define	 EPSON_LEVEL_B8		 9
#define	 EPSON_LEVEL_F5		10

/* there is also a function level "A5", which I'm igoring here until somebody can 
   convince me that this is still needed. The A5 level was for the GT-300, which
   was (is) a monochrome only scanner. So if somebody really wants to use this
   scanner with SANE get in touch with me and we can work something out - khk */

#define	 EPSON_LEVEL_DEFAULT	EPSON_LEVEL_B3

static EpsonCmdRec epson_cmd [ ] =
{
/*
//       request identity
//       |   request status
//       |   |   request condition
//       |   |   |   set color mode
//       |   |   |   |   start scanning
//       |   |   |   |   |   set data format
//       |   |   |   |   |   |   set resolution
//       |   |   |   |   |   |   |   set zoom
//       |   |   |   |   |   |   |   |   set scan area
//       |   |   |   |   |   |   |   |   |   set brightness
//       |   |   |   |   |   |   |   |   |   |            set gamma
//       |   |   |   |   |   |   |   |   |   |            |   set halftoning
//       |   |   |   |   |   |   |   |   |   |            |   |   set color correction
//       |   |   |   |   |   |   |   |   |   |            |   |   |   initialize scanner
//       |   |   |   |   |   |   |   |   |   |            |   |   |   |   set speed
//       |   |   |   |   |   |   |   |   |   |            |   |   |   |   |   set lcount
//       |   |   |   |   |   |   |   |   |   |            |   |   |   |   |   |   mirror image
//       |   |   |   |   |   |   |   |   |   |            |   |   |   |   |   |   |   set gamma table
//       |   |   |   |   |   |   |   |   |   |            |   |   |   |   |   |   |   |   set outline emphasis
//       |   |   |   |   |   |   |   |   |   |            |   |   |   |   |   |   |   |   |   set dither
//       |   |   |   |   |   |   |   |   |   |            |   |   |   |   |   |   |   |   |   |   set color correction coefficients
//       |   |   |   |   |   |   |   |   |   |            |   |   |   |   |   |   |   |   |   |   |   request extension status
//       |   |   |   |   |   |   |   |   |   |            |   |   |   |   |   |   |   |   |   |   |   |   control an extension
//       |   |   |   |   |   |   |   |   |   |            |   |   |   |   |   |   |   |   |   |   |   |   |    forward feed / eject
//       |   |   |   |   |   |   |   |   |   |            |   |   |   |   |   |   |   |   |   |   |   |   |    |   request push button status
//       |   |   |   |   |   |   |   |   |   |            |   |   |   |   |   |   |   |   |   |   |   |   |    |   |   control auto area segmentation
//       |   |   |   |   |   |   |   |   |   |            |   |   |   |   |   |   |   |   |   |   |   |   |    |   |   |   set film type
//       |   |   |   |   |   |   |   |   |   |            |   |   |   |   |   |   |   |   |   |   |   |   |    |   |   |   |   set exposure time
//       |   |   |   |   |   |   |   |   |   |            |   |   |   |   |   |   |   |   |   |   |   |   |    |   |   |   |   |   set bay
//       |   |   |   |   |   |   |   |   |   |            |   |   |   |   |   |   |   |   |   |   |   |   |    |   |   |   |   |   |
*/
  {"A1",'I','F','S', 0 ,'G', 0 ,'R', 0 ,'A', 0 ,{ 0,0,0}, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,  0 , 0 , 0 , 0 , 0 , 0 }
  ,
  {"A2",'I','F','S', 0 ,'G','D','R','H','A','L',{-3,3,0},'Z','B', 0 ,'@', 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,  0 , 0 , 0 , 0 , 0 , 0 }
  ,	      	      	      	      	
  {"B1",'I','F','S','C','G','D','R', 0 ,'A', 0 ,{ 0,0,0}, 0 ,'B', 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,  0 , 0 , 0 , 0 , 0 , 0 }
  ,	      	      	      	      	
  {"B2",'I','F','S','C','G','D','R','H','A','L',{-3,3,0},'Z','B', 0 ,'@', 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,  0 , 0 , 0 , 0 , 0 , 0 }
  ,	      	      	      	      	
  {"B3",'I','F','S','C','G','D','R','H','A','L',{-3,3,0},'Z','B','M','@', 0 , 0 , 0 , 0 , 0 , 0 ,'m','f','e',  0 , 0 , 0 , 0 , 0 , 0 }
  ,	      	      	      	      	
  {"B4",'I','F','S','C','G','D','R','H','A','L',{-3,3,0},'Z','B','M','@','g','d', 0 ,'z','Q','b','m','f','e',  0 , 0 , 0 , 0 , 0 , 0 }
  ,	      	      	      	      	
  {"B5",'I','F','S','C','G','D','R','H','A','L',{-3,3,0},'Z','B','M','@','g','d','K','z','Q','b','m','f','e',  0 , 0 , 0 , 0 , 0 , 0 }
  ,
  {"B6",'I','F','S','C','G','D','R','H','A','L',{-3,3,0},'Z','B','M','@','g','d','K','z','Q','b','m','f','e',  0 , 0 , 0 , 0 , 0 , 0 }
  ,	      	      	      	      	
  {"B7",'I','F','S','C','G','D','R','H','A','L',{-4,3,0},'Z','B','M','@','g','d','K','z','Q','b','m','f','e','\f','!','s','N', 0 , 0 }
  ,	      	      	      	      	
  {"B8",'I','F','S','C','G','D','R','H','A','L',{-4,3,0},'Z','B','M','@','g','d','K','z','Q','b','m','f','e',  0 ,'!','s','N', 0 , 0 }
  ,	      	      	      	      	
  {"F5",'I','F','S','C','G','D','R','H','A','L',{-3,3,0},'Z', 0 ,'M','@','g','d','K','z','Q', 0 ,'m','f','e','\f', 0 , 0 ,'N','T','P'}
};



/*

.) exposure time

- no docs found

- from epson_31101999.c

unsigned char default_tval[4] = {2, 0x80, 0x80, 0x80};
unsigned char     neg_tval[4] = {2, 0x8e, 0x86, 0x92};

ESC T + 4 byte

- there are defs in include/sane/saneopts.h

#define SANE_NAME_CAL_EXPOS_TIME	"cal-exposure-time"
#define SANE_NAME_CAL_EXPOS_TIME_R	"cal-exposure-time-r"
#define SANE_NAME_CAL_EXPOS_TIME_G	"cal-exposure-time-g"
#define SANE_NAME_CAL_EXPOS_TIME_B	"cal-exposure-time-b"
#define SANE_NAME_SCAN_EXPOS_TIME	"scan-exposure-time"
#define SANE_NAME_SCAN_EXPOS_TIME_R	"scan-exposure-time-r"
#define SANE_NAME_SCAN_EXPOS_TIME_G	"scan-exposure-time-g"
#define SANE_NAME_SCAN_EXPOS_TIME_B	"scan-exposure-time-b"
#define SANE_NAME_SELECT_EXPOSURE_TIME	"select-exposure-time"

Einmal für Calibrierung, andere für Scan. lamp-density gibt es auch noch.
One for calibration, other one for scan. There is also lamp-density defined.

- da war noch mal die blöde mail wo ganz genau drinsteht wie das geht.

.) 12 bit

- there are defs in include/sane/saneopts.h

#define SANE_NAME_TEN_BIT_MODE		"ten-bit-mode"
#define SANE_NAME_TWELVE_BIT_MODE	"twelve-bit-mode"

- not used by other backends.




*/

/*
//
//
*/

struct mode_param {
	int color;
	int mode_flags;
	int dropout_mask;
	int depth;
};

static const struct mode_param mode_params [ ] =
	{ { 0, 0x00, 0x30,  1}
	, { 0, 0x00, 0x30,  8}
	, { 1, 0x02, 0x00,  8}
	};

static const struct mode_param mode_params_5 [ ] =
	{ { 0, 0x00, 0x30,  1}
	, { 0, 0x00, 0x30,  8}
	, { 1, 0x03, 0x10,  8}
	};

static const SANE_String_Const mode_list [ ] =
	{ "Binary"
	, "Gray"
	, "Color"
	, NULL
	};

static const SANE_String_Const mode_list_5 [ ] =
	{ "Binary"
	, "Gray"
	, "Color"
	, NULL
	};

#define  FBF_STR	"Flatbed"
#define  TPU_STR	"Transparency Unit"
#define  ADF_STR	"Automatic Document Feeder"

/*
// source list need one dummy entry (save device settings is crashing).
// NOTE: no const.
*/

static SANE_String_Const source_list [ ] =
	{ FBF_STR
	, NULL
	, NULL
	, NULL
	};

static const SANE_String_Const film_list [ ] =
	{ "Positive Film"
	, "Negative Film"
	, NULL
	};

/*
// TODO: add some missing const.
*/

static int halftone_params [ ] =
	{ 0x01
	, 0x00
	, 0x10
	, 0x20
	, 0x80
	, 0x90
	, 0xa0
	, 0xb0
	, 0x03
	, 0xc0
	, 0xd0
	};

static const SANE_String_Const halftone_list [ ] =
	{ "None"
	, "Halftone A (Hard Tone)"
	, "Halftone B (Soft Tone)"
	, "Halftone C (Net Screen)"
	, NULL
	};

static const SANE_String_Const halftone_list_4 [ ] =
	{ "None"
	, "Halftone A (Hard Tone)"
	, "Halftone B (Soft Tone)"
	, "Halftone C (Net Screen)"
	, "Dither A (4x4 Bayer)"
	, "Dither B (4x4 Spiral)"
	, "Dither C (4x4 Net Screen)"
	, "Dither D (8x4 Net Screen)"
	, NULL
	};

static const SANE_String_Const halftone_list_7 [ ] =
	{ "None"
	, "Halftone A (Hard Tone)"
	, "Halftone B (Soft Tone)"
	, "Halftone C (Net Screen)"
	, "Dither A (4x4 Bayer)"
	, "Dither B (4x4 Spiral)"
	, "Dither C (4x4 Net Screen)"
	, "Dither D (8x4 Net Screen)"
	, "Text Enhanced Technology"
	, "Download pattern A"
	, "Download pattern B"
	, NULL
	};

static int dropout_params [ ] =
	{ 0x00
	, 0x10
	, 0x20
	, 0x30
	};

static const SANE_String_Const dropout_list [ ] =
	{ "None"
	, "Red"
	, "Green"
	, "Blue"
	, NULL
	};

/*
// NOTE: if enable "User defined" change also default from 4 to 5.
*/

static int color_params [ ] =
	{ 0x00
/*	, 0x01	*/
	, 0x10
	, 0x20
	, 0x40
	, 0x80
	};

static const SANE_String_Const color_list [ ] =
	{ "No Correction"
/*	, "User defined"	*/
	, "Impact-dot printers"
	, "Thermal printers"
	, "Ink-jet printers"
	, "CRT monitors"
	, NULL
	};

static int gamma_params [ ] =
	{ 0x01
	, 0x03
	, 0x00
	, 0x10
	, 0x20
	};

static const SANE_String_Const gamma_list [ ] =
	{ "Default"
	, "User defined"
	, "High density printing"
	, "Low density printing"
	, "High contrast printing"
	, NULL
	};

static const SANE_String_Const bay_list [ ] =
	{ " 1 "
	, " 2 "
	, " 3 "
	, " 4 "
	, " 5 "
	, " 6 "
	, NULL
	};

/*
//  minimum, maximum, quantization.
*/

static const SANE_Range u8_range = { 0, 255, 0};
static const SANE_Range s8_range = { -127, 127, 0};

static int mirror_params [ ] =
	{ 0
	, 1
	};

#define  speed_params	mirror_params
#define  film_params	mirror_params

static const SANE_Range outline_emphasis_range = { -2, 2, 0 };
/* static const SANE_Range gamma_range = { -2, 2, 0 }; */

struct qf_param {
	SANE_Word tl_x;
	SANE_Word tl_y;
	SANE_Word br_x;
	SANE_Word br_y;
};

/* gcc don't like to overwrite const field */
static /*const*/ struct qf_param qf_params [ ] =
	{ { 0, 0, SANE_FIX( 120.0), SANE_FIX( 120.0) }
	, { 0, 0, SANE_FIX( 148.5), SANE_FIX( 210.0) }
	, { 0, 0, SANE_FIX( 210.0), SANE_FIX( 148.5) }
	, { 0, 0, SANE_FIX( 215.9), SANE_FIX( 279.4) }		/* 8.5" x 11" */
	, { 0, 0, SANE_FIX( 210.0), SANE_FIX( 297.0) }
	, { 0, 0, 0, 0}
	};

static const SANE_String_Const qf_list [ ] =
	{ "CD"
	, "A5 portrait"
	, "A5 landscape"
	, "Letter"
	, "A4"
	, "max"
	, NULL
	};

static SANE_Word * resolution_list = NULL;

/*
//
//
*/

static size_t
max_string_size (const SANE_String_Const strings[])
{
  size_t size, max_size = 0;
  int i;

  for (i = 0; strings[i]; i++)
    {
      size = strlen(strings[i]) + 1;
      if (size > max_size)
        max_size = size;
    }
  return max_size;
}

typedef struct {
	u_char	code;
	u_char	status;
	u_short	count;
	u_char	buf [ 1];

} EpsonHdrRec, * EpsonHdr;

typedef struct {
	u_char	code;
	u_char	status;
	u_short	count;

	u_char	type;
	u_char	level;

	u_char	buf [ 1];

} EpsonIdentRec, * EpsonIdent;

typedef struct {
	u_char	code;
	u_char	status;
	u_short	count;

	u_char	buf [ 1];

} EpsonParameterRec, * EpsonParameter;

typedef struct {
	u_char	code;
	u_char	status;

	u_char	buf [ 4];

} EpsonDataRec, * EpsonData;

/*
//
//
*/

static EpsonHdr command ( Epson_Scanner * s, const u_char * cmd, size_t cmd_size, SANE_Status * status);

/*
//
//
*/

static int send ( Epson_Scanner * s, const void *buf, size_t buf_size, SANE_Status * status) {

	DBG( 3, "send buf, size = %lu\n", ( u_long) buf_size);

#if 1
	{
		size_t k;
		const unsigned char * s = buf;

		for( k = 0; k < buf_size; k++) {
			DBG( 6, "buf[%u] %02x %c\n", k, s[ k], isprint( s[ k]) ? s[ k] : '.');
		}
	}
#endif

	if( s->hw->connection == SANE_EPSON_SCSI) {
		return scsi_write( s->fd, buf, buf_size, status);
	} else if (s->hw->connection == SANE_EPSON_PIO) {
		size_t n;

		if( buf_size == ( n = sanei_pio_write( s->fd, buf, buf_size)))
			*status = SANE_STATUS_GOOD;
		else
			*status = SANE_STATUS_INVAL;

		return n;
	} else if (s->hw->connection == SANE_EPSON_USB) {
		size_t n;

		if( buf_size == ( n = write( s->fd, buf, buf_size)))
			*status = SANE_STATUS_GOOD;
		else
			*status = SANE_STATUS_INVAL;

		return n;
	}

	return SANE_STATUS_INVAL;
	/* never reached */
}

/*
//
//
*/

static int receive ( Epson_Scanner * s, void *buf, size_t buf_size, SANE_Status * status) {
	size_t n = 0;

	if( s->hw->connection == SANE_EPSON_SCSI) 
	{
		n = scsi_read( s->fd, buf, buf_size, status);
	} 
        else if ( s->hw->connection == SANE_EPSON_PIO) 
	{
		if( buf_size == ( n = sanei_pio_read( s->fd, buf, buf_size)))
			*status = SANE_STATUS_GOOD;
		else
			*status = SANE_STATUS_INVAL;
	}
	else if (s->hw->connection == SANE_EPSON_USB) 
	{
		if( buf_size == ( n = read( s->fd, buf, buf_size)))
			*status = SANE_STATUS_GOOD;
		else
			*status = SANE_STATUS_INVAL;
	}

	DBG( 7, "receive buf, expected = %lu, got = %d\n", ( u_long) buf_size, n);

#if 1
	{
		size_t k;
		const unsigned char * s = buf;

		for( k = 0; k < n; k++) {
			DBG( 8, "buf[%u] %02x %c\n", k, s[ k], isprint( s[ k]) ? s[ k] : '.');
	 	}
	}
#endif

	return n;
}

/*
//
//
*/

static SANE_Status expect_ack ( Epson_Scanner * s) {
	unsigned char result [ 1];
	size_t len;
	SANE_Status status;

	len = sizeof result;

	receive( s, result, len, &status);

	if( SANE_STATUS_GOOD != status)
		return status;

	if( ACK != result[ 0])
		return SANE_STATUS_INVAL;

	return SANE_STATUS_GOOD;
}

/*
//
//
*/

static SANE_Status set_cmd ( Epson_Scanner * s, unsigned char cmd, int val) {
	SANE_Status status;
	unsigned char params [ 2];

	if( ! cmd)
		return SANE_STATUS_UNSUPPORTED;

	params[ 0] = '\033';
	params[ 1] = cmd;

	send( s, params, 2, &status);
	if( SANE_STATUS_GOOD != ( status = expect_ack( s) ) )
		return status;

	params[ 0] = val;
	send( s, params, 1, &status);
	status = expect_ack( s);

	return status;
}

/*
//
//
*/

#define  set_color_mode(s,v)		set_cmd(s,(s)->hw->cmd->set_color_mode,v)
#define  set_data_format(s,v)		set_cmd( s,(s)->hw->cmd->set_data_format, v)
#define  set_halftoning(s,v)		set_cmd( s,(s)->hw->cmd->set_halftoning, v)
#define  set_gamma(s,v)			set_cmd( s,(s)->hw->cmd->set_gamma, v)
#define  set_color_correction(s,v)	set_cmd( s,(s)->hw->cmd->set_color_correction, v)
#define  set_lcount(s,v)		set_cmd( s,(s)->hw->cmd->set_lcount, v)
#define  set_bright(s,v)		set_cmd( s,(s)->hw->cmd->set_bright, v)
#define  mirror_image(s,v)		set_cmd( s,(s)->hw->cmd->mirror_image, v)
#define  set_speed(s,v)			set_cmd( s,(s)->hw->cmd->set_speed, v)
#define  set_outline_emphasis(s,v)	set_cmd( s,(s)->hw->cmd->set_outline_emphasis, v)
#define  control_auto_area_segmentation(s,v)	set_cmd( s,(s)->hw->cmd->control_auto_area_segmentation, v)
#define  set_film_type(s,v)		set_cmd( s,(s)->hw->cmd->set_film_type, v)
#define  set_exposure_time(s,v)		set_cmd( s,(s)->hw->cmd->set_exposure_time, v)
#define  set_bay(s,v)			set_cmd( s,(s)->hw->cmd->set_bay, v)
/*#define  forward_feed(s)		direct_cmd( s,(s)->hw->cmd->forward_feed) */

/*#define  (s,v)		set_cmd( s,(s)->hw->cmd->, v) */

static SANE_Status set_resolution ( Epson_Scanner * s, int xres, int yres) {
	SANE_Status status;
	unsigned char params[4];

	if( ! s->hw->cmd->set_resolution)
		return SANE_STATUS_GOOD;

	send( s, "\033R", 2, &status);
	status = expect_ack( s);

	if( status != SANE_STATUS_GOOD)
		return status;

	params[ 0] = xres;
	params[ 1] = xres >> 8;
	params[ 2] = yres;
	params[ 3] = yres >> 8;

	send( s, params, 4, &status);
	status = expect_ack( s);

	return status;
}

/*
//
//
*/

static SANE_Status set_scan_area ( Epson_Scanner * s, int x, int y, int width, int height) {
	SANE_Status status;
	unsigned char params[ 8];

	if( ! s->hw->cmd->set_scan_area) {
		return SANE_STATUS_GOOD;
	}

	send( s, "\033A", 2, &status);
	status = expect_ack( s);
	if( status != SANE_STATUS_GOOD)
		return status;

	params[ 0] = x;
	params[ 1] = x >> 8;
	params[ 2] = y;
	params[ 3] = y >> 8;
	params[ 4] = width;
	params[ 5] = width >> 8;
	params[ 6] = height;
	params[ 7] = height >> 8;

	DBG( 1, "%p %d %d %d %d\n", ( void *) s, x, y, width, height);

	send( s, params, 8, &status);
	status = expect_ack( s);

	return status;
}

/*
//
//
*/

static SANE_Status set_color_correction_coefficients ( Epson_Scanner * s) {
	SANE_Status status;
	unsigned char cmd = s->hw->cmd->set_color_correction_coefficients;
	unsigned char params [ 2];
	const int length = 9;
	signed char cct [ 9];

	if( ! cmd)
		return SANE_STATUS_UNSUPPORTED;

	params[ 0] = '\033';
	params[ 1] = cmd;

	send( s, params, 2, &status);
	if( SANE_STATUS_GOOD != ( status = expect_ack( s) ) )
		return status;

	cct[ 0] = s->val[ OPT_CCT_1].w;
	cct[ 1] = s->val[ OPT_CCT_2].w;
	cct[ 2] = s->val[ OPT_CCT_3].w;
	cct[ 3] = s->val[ OPT_CCT_4].w;
	cct[ 4] = s->val[ OPT_CCT_5].w;
	cct[ 5] = s->val[ OPT_CCT_6].w;
	cct[ 6] = s->val[ OPT_CCT_7].w;
	cct[ 7] = s->val[ OPT_CCT_8].w;
	cct[ 8] = s->val[ OPT_CCT_9].w;

	send( s, cct, length, &status);
	status = expect_ack( s);

	return status;
}

/*
//
//
*/

static SANE_Status set_gamma_table ( Epson_Scanner * s) {
	SANE_Status status;
	unsigned char cmd = s->hw->cmd->set_gamma_table;
	unsigned char params [ 2];
	const int length = 257;
	unsigned char gamma [ 257];
	int n;

	if( ! cmd)
		return SANE_STATUS_UNSUPPORTED;

	params[ 0] = '\033';
	params[ 1] = cmd;

/*
// TODO: &status in send make no sense like that.
*/

	send( s, params, 2, &status);
	if( SANE_STATUS_GOOD != ( status = expect_ack( s) ) )
		return status;

	gamma[ 0] = 'm';

	for( n = 0; n < 256; ++n) {
		gamma[ n + 1] = s->gamma_table[ 0] [ n];
	}

	send( s, gamma, length, &status);
	if( SANE_STATUS_GOOD != ( status = expect_ack( s) ) )
		return status;

	send( s, params, 2, &status);
	if( SANE_STATUS_GOOD != ( status = expect_ack( s) ) )
		return status;

	gamma[ 0] = 'r';

	for( n = 0; n < 256; ++n) {
		gamma[ n + 1] = s->gamma_table[ 1] [ n];
	}

	send( s, gamma, length, &status);
	if( SANE_STATUS_GOOD != ( status = expect_ack( s) ) )
		return status;

	send( s, params, 2, &status);
	if( SANE_STATUS_GOOD != ( status = expect_ack( s) ) )
		return status;

	gamma[ 0] = 'g';

	for( n = 0; n < 256; ++n) {
		gamma[ n + 1] = s->gamma_table[ 2] [ n];
	}

	send( s, gamma, length, &status);
	if( SANE_STATUS_GOOD != ( status = expect_ack( s) ) )
		return status;

	send( s, params, 2, &status);
	if( SANE_STATUS_GOOD != ( status = expect_ack( s) ) )
		return status;

	gamma[ 0] = 'b';

	for( n = 0; n < 256; ++n) {
		gamma[ n + 1] = s->gamma_table[ 3] [ n];
	}

	send( s, gamma, length, &status);
	status = expect_ack( s);

	return status;
}

/*
//
//
*/

static SANE_Status check_ext_status ( Epson_Scanner * s) {
	SANE_Status status;
	unsigned char cmd = s->hw->cmd->request_extention_status;
	unsigned char params [ 2];
	unsigned char * buf;
	EpsonHdr head;

	if( ! cmd)
		return SANE_STATUS_UNSUPPORTED;

	params[ 0] = '\033';
	params[ 1] = cmd;

	if( NULL == ( head = ( EpsonHdr) command( s, params, 2, &status) ) ) {
		DBG( 0, "Extended status flag request failed\n");
		return status;
	}

	buf = &head->buf[ 0];

	if( buf[ 0] & EXT_STATUS_FER) {
		DBG( 0, "option: fatal error\n");
		status = SANE_STATUS_INVAL;
	}

	if( buf[ 1] & EXT_STATUS_ERR) {
		DBG( 0, "ADF: other error\n");
		status = SANE_STATUS_INVAL;
	}

	if( buf[ 1] & EXT_STATUS_PE) {
		DBG( 0, "ADF: no paper\n");
		status = SANE_STATUS_INVAL;
	}

	if( buf[ 1] & EXT_STATUS_PJ) {
		DBG( 0, "ADF: paper jam\n");
		status = SANE_STATUS_INVAL;
	}

	if( buf[ 1] & EXT_STATUS_OPN) {
		DBG( 0, "ADF: cover open\n");
		status = SANE_STATUS_INVAL;
	}

	if( buf[ 6] & EXT_STATUS_ERR) {
		DBG( 0, "TPU: other error\n");
		status = SANE_STATUS_INVAL;
	}

	return status;
}

/*
//
//
*/

static SANE_Status reset ( Epson_Scanner * s) {
	SANE_Status status;

	if( ! s->hw->cmd->initialize_scanner)
		return SANE_STATUS_GOOD;

	send (s, "\033@", 2, &status);
	status = expect_ack( s);
	return status;
}

/*
//
//
*/

static void close_scanner ( Epson_Scanner * s) {

	if( s->hw->connection == SANE_EPSON_SCSI)
		sanei_scsi_close( s->fd);
	else if ( s->hw->connection == SANE_EPSON_PIO) 
		sanei_pio_close( s->fd);
	else if ( s->hw->connection == SANE_EPSON_USB)
		close( s->fd);

	return;
}

/*
//
//
*/

static SANE_Status open_scanner ( Epson_Scanner * s) {
	SANE_Status status = 0;

	if( s->hw->connection == SANE_EPSON_SCSI) {
		if( SANE_STATUS_GOOD != ( status = sanei_scsi_open( s->hw->sane.name, &s->fd, NULL, NULL))) {
			DBG( 1, "sane_start: %s open failed: %s\n", s->hw->sane.name, sane_strstatus( status));
			return status;
		}
	} else if ( s->hw->connection == SANE_EPSON_PIO) {
		if( SANE_STATUS_GOOD != ( status = sanei_pio_open( s->hw->sane.name, &s->fd))) {
			DBG( 1, "sane_start: %s open failed: %s\n", s->hw->sane.name, sane_strstatus( status));
			return status;
		}
	} else if (s->hw->connection == SANE_EPSON_USB) {
		int flags;

#ifdef _O_RDWR
 flags = _O_RDWR;
#else
 flags = O_RDWR;
#endif
#ifdef _O_EXCL
 flags |= _O_EXCL;
#else
 flags |= O_EXCL;
#endif
#ifdef _O_BINARY
 flags |= _O_BINARY;
#endif
#ifdef O_BINARY
 flags |= O_BINARY;
#endif

		s->fd = open(s->hw->sane.name, flags);
		if (s->fd < 0) {
			DBG( 1, "sane_start: %s open failed: %s\n", 
				s->hw->sane.name, strerror(errno));
			status = (errno == EACCES) ? SANE_STATUS_ACCESS_DENIED 
				: SANE_STATUS_INVAL;
			return status;
		}
		
	}

	return status;
}

/*
//
//
*/
#if 0
static SANE_Status direct_cmd ( Epson_Scanner * s, unsigned char cmd) {
	SANE_Status status;
	unsigned char params [ 2];

	if( ! cmd)
		return SANE_STATUS_UNSUPPORTED;

	if( SANE_STATUS_GOOD != ( status = open_scanner( s)))
		return status;

	params[ 0] = '\033';
	params[ 1] = cmd;

	send( s, params, 2, &status);

	if( SANE_STATUS_GOOD != ( status = expect_ack( s) ) ) {
		close_scanner( s);
		return status;
	}

	close_scanner( s);
	return status;
}
#endif
/*
//
//
*/

static SANE_Status eject ( Epson_Scanner * s) {
	SANE_Status status;
	unsigned char params [ 2];
	unsigned char cmd = s->hw->cmd->eject;

	if( ! cmd)
		return SANE_STATUS_UNSUPPORTED;

	if( SANE_STATUS_GOOD != ( status = open_scanner( s)))
		return status;

	params[ 0] = cmd;

	send( s, params, 1, &status);

	if( SANE_STATUS_GOOD != ( status = expect_ack( s) ) ) {
		close_scanner( s);
		return status;
	}

	close_scanner( s);
	return status;
}

/*
//
//
*/

static Epson_Device dummy_dev =
{ { NULL, "Epson", NULL, "flatbed scanner" }
};


#if 1
static EpsonHdr command ( Epson_Scanner * s, const u_char * cmd, 
      size_t cmd_size, SANE_Status * status) 
{
	EpsonHdr head;
	u_char * buf;

	if( NULL == ( head = walloc( EpsonHdrRec))) {
		*status = SANE_STATUS_NO_MEM;
		return ( EpsonHdr) 0;
	}

	send( s, cmd, cmd_size, status);

	if( SANE_STATUS_GOOD != *status)
		return ( EpsonHdr) 0;

	buf = ( u_char *) head;

	if( s->hw->connection == SANE_EPSON_SCSI) 
	{
		receive( s, buf, 4, status);
		buf += 4;
	}
	else if (s->hw->connection == SANE_EPSON_USB)
        {
                int     bytes_read;
                bytes_read = receive( s, buf, 4, status);
                buf += bytes_read;
	} 
	else 
	{
		receive( s, buf, 1, status);
		buf += 1;
	}

	if( SANE_STATUS_GOOD != *status)
		return ( EpsonHdr) 0;

	DBG( 4, "code   %02x\n", (int) head->code);

	switch (head->code) 
        {
	  default:
            if( 0 == head->code)
	      DBG( 1, "Incompatible printer port (probably bi/directional)\n");
            else if( cmd[cmd_size - 1] == head->code)
	      DBG( 1, "Incompatible printer port (probably not bi/directional)\n");

            DBG( 2, "Illegal response of scanner for command: %02x\n", head->code);
            break;

          case NAK:
		/* fall through */
          case ACK:
            break;	/* no need to read any more data after ACK or NAK */

          case STX:
	    if(  s->hw->connection == SANE_EPSON_SCSI) 
	    {
		/* nope */
	    } 
	    else if (s->hw->connection == SANE_EPSON_USB)
            {
                /* we've already read the complete data */
            } 
	    else
            {
		receive (s, buf, 3, status);
/*		buf += 3; */
	    }

            if( SANE_STATUS_GOOD != *status)
	      return (EpsonHdr) 0;

	    DBG( 4, "status %02x\n", (int) head->status);
	    DBG( 4, "count  %d\n", (int) head->count);

            if( NULL == (head = realloc (head, sizeof (EpsonHdrRec) + head->count)))
	    {
	      *status = SANE_STATUS_NO_MEM;
	      return (EpsonHdr) 0;
	    }

            buf = head->buf;
            receive (s, buf, head->count, status);

            if( SANE_STATUS_GOOD != *status)
	      return (EpsonHdr) 0;

/*          buf += head->count; */

/*          sanei_hexdmp( head, sizeof( EpsonHdrRec) + head->count, "epson"); */

            break;
        }

        return head;
}
#endif

/*
//
//
*/

static SANE_Status attach ( const char * dev_name, Epson_Device * * devp) {
	SANE_Status status;
	Epson_Scanner * s = walloca( Epson_Scanner);
	char * str;
	struct Epson_Device * dev;

	DBG(1, "%s\n", SANE_EPSON_VERSION);

/*
//  set dummy values.
*/

	s->hw = dev = &dummy_dev;			/* use dummy device record */
	s->hw->cmd = &epson_cmd[EPSON_LEVEL_DEFAULT];	/* use default function level */
        s->hw->connection = SANE_EPSON_NODEV;		/* no device configured yet */

	DBG( 3, "attach: opening %s\n", dev_name);

	s->hw->last_res = s->hw->last_res_preview = 0;	/* set resolution to safe values */

/*
//  decide if interface is USB, SCSI or parallel.
*/

	/*
	 * if the config file contains a line "usb /dev/usbscanner", then handle this
 	 * here and use the USB device from now on.
	 */
	if (strncmp(dev_name, SANE_EPSON_CONFIG_USB, strlen(SANE_EPSON_CONFIG_USB)) == 0) {
		/* we have a match for the USB string and adjust the device name */
		dev_name += strlen(SANE_EPSON_CONFIG_USB);
		dev_name  = sanei_config_skip_whitespace(dev_name);
		s->hw->connection = SANE_EPSON_USB;
	}
	/*
	 * if the config file contains a line "pio 0xXXX", then handle this case here
	 * and use the PIO (parallel interface) device from now on.
	 */
	else if (strncmp(dev_name, SANE_EPSON_CONFIG_PIO, strlen(SANE_EPSON_CONFIG_PIO)) == 0) {
		/* we have a match for the PIO string and adjust the device name */
		dev_name += strlen(SANE_EPSON_CONFIG_PIO);
		dev_name  = sanei_config_skip_whitespace(dev_name);
		s->hw->connection = SANE_EPSON_PIO;
	}
	else {		/* legacy mode */
		char * end;

		strtol( dev_name, &end, 0);

		if( ( end == dev_name) || *end) {
			s->hw->connection = SANE_EPSON_SCSI;
		} else {
			s->hw->connection = SANE_EPSON_PIO;
	      	}
	}

	if (s->hw->connection == SANE_EPSON_NODEV) 
	{
		/* 
		   With the current code this can neve happen, because 
		   the routine to handle the legacy mode will always 
		   return a SCSI device. If this gets changed however,
		   here is the test to return with an error code.
		*/
		return SANE_STATUS_INVAL;
	}

/*
//  if interface is SCSI do an inquiry.
*/

	if( s->hw->connection == SANE_EPSON_SCSI) {
#define  INQUIRY_BUF_SIZE	36

		unsigned char buf[ INQUIRY_BUF_SIZE + 1];
		size_t buf_size = INQUIRY_BUF_SIZE;

		if( SANE_STATUS_GOOD != ( status = sanei_scsi_open( dev_name, &s->fd, NULL, NULL))) {
			DBG( 1, "attach: open failed: %s\n", sane_strstatus( status));
			if (dummy_dev.sane.name != NULL)
				free(dummy_dev.sane.name);
			dummy_dev.sane.name = NULL;
			return status;
		}

		DBG( 3, "attach: sending INQUIRY\n");
/*		buf_size = sizeof buf; */

		if( SANE_STATUS_GOOD != ( status = inquiry( s->fd, 0, buf, &buf_size))) {
			DBG( 1, "attach: inquiry failed: %s\n", sane_strstatus( status));
			if (dummy_dev.sane.name != NULL)
				free(dummy_dev.sane.name);
			dummy_dev.sane.name = NULL;
			close_scanner( s);
			return status;
		}

		buf[ INQUIRY_BUF_SIZE] = 0;
		DBG( 1, ">%s<\n", buf + 8);

		/* 
		 * For USB and PIO scanners this will be done later, once
		 * we have communication established with the device.
		 */

		if( buf[ 0] != TYPE_PROCESSOR
			|| strncmp( buf + 8, "EPSON", 5) != 0
			|| (strncmp( buf + 16, "SCANNER ", 8) != 0
				&& strncmp( buf + 14, "SCANNER ", 8) != 0
				&& strncmp( buf + 16, "Perfection", 10) != 0
				&& strncmp( buf + 16, "Expression", 10) != 0))
		{
			DBG( 1, "attach: device doesn't look like an Epson scanner\n");
			if (dummy_dev.sane.name != NULL)
				free(dummy_dev.sane.name);
			dummy_dev.sane.name = NULL;
			close_scanner( s);
			return SANE_STATUS_INVAL;
		}

#if 0
		/* 
		 * Wait with the setting of the device name until the
		 * scanner itself can tell us.
		 */
		str = malloc( 8 + 1);
		str[ 8] = '\0';
		dummy_dev.sane.model = ( char *) memcpy( str, buf + 16 + 8, 8);
#endif

/*
//  else parallel or USB.
*/

	} else if (s->hw->connection == SANE_EPSON_PIO) {
		if( SANE_STATUS_GOOD != ( status = sanei_pio_open( dev_name, &s->fd))) {
			DBG( 1, "dev_open: %s: can't open %s as a parallel-port device\n",
				sane_strstatus( status), dev_name);
			if (dummy_dev.sane.name != NULL)
				free(dummy_dev.sane.name);
			dummy_dev.sane.name = NULL;
			return status;
		}
	} else if (s->hw->connection == SANE_EPSON_USB) {
		int flags;

#ifdef _O_RDWR
		flags = _O_RDWR;
#else
		flags = O_RDWR;
#endif
#ifdef _O_EXCL
		flags |= _O_EXCL;
#else
		flags |= O_EXCL;
#endif
#ifdef _O_BINARY
		flags |= _O_BINARY;
#endif
#ifdef O_BINARY
		flags |= O_BINARY;
#endif

		s->fd = open(dev_name, flags);
		if (s->fd < 0) {
			DBG( 1, "sane_start: %s open (USB) failed: %s\n", 
				dev_name, strerror(errno));
			status = (errno == EACCES) ? SANE_STATUS_ACCESS_DENIED 
				: SANE_STATUS_INVAL;
			if (dummy_dev.sane.name != NULL)
				free(dummy_dev.sane.name);
			dummy_dev.sane.name = NULL;
			return status;
		}
	}

/*
// Initialize (ESC @).
*/
/* NOTE: disabled cause of batch use of scanimage with ADF. */

#if 1
	{
		(void) command( s, "\033@", 2, &status);
	}
#endif
/*
//  Identification Request (ESC I).
*/

	{
		EpsonIdent ident;
		u_char * buf;

/*
//		if( ! s->hw->cmd->request_identity)
//			return SANE_STATUS_INVAL;
*/
		if( NULL == ( ident = ( EpsonIdent) command( s, "\033I", 2, &status) ) ) {
			DBG( 0, "ident failed\n");
/*
// status should be SANE_STATUS_INVAL anyway, only a try.
//			return status;
*/
			return SANE_STATUS_INVAL;
		}

		DBG( 1, "type  %3c 0x%02x\n", ident->type, ident->type);
		DBG( 1, "level %3c 0x%02x\n", ident->level, ident->level);

		{
			char * force = getenv( "SANE_EPSON_CMD_LVL");

			if( force) {
				ident->type = force[ 0];
				ident->level = force[ 1];

				DBG( 1, "type  %3c 0x%02x\n", ident->type, ident->type);
				DBG( 1, "level %3c 0x%02x\n", ident->level, ident->level);

				DBG( 1, "forced\n");
			}
		}

/*
//  check if option equipment is installed.
*/

		if( ident->status & STATUS_OPTION) {
			DBG( 1, "option equipment is installed\n");
			dev->extension = SANE_TRUE;
		} else {
			DBG( 1, "no option equipment installed\n");
			dev->extension = SANE_FALSE;
		}

		dev->TPU = SANE_FALSE;
		dev->ADF = SANE_FALSE;

/*
//  set command type and level.
*/

	    {
		int n;

		for( n = 0; n < NELEMS( epson_cmd); n++)
			if( ! strncmp( &ident->type, epson_cmd[ n].level, 2) )
				break;

		if( n < NELEMS( epson_cmd) ) {
			dev->cmd = &epson_cmd[ n];
		} else {
			dev->cmd = &epson_cmd[EPSON_LEVEL_DEFAULT];
			DBG( 0, "Unknown type %c or level %c, using %s\n", 
				ident->buf[0], ident->buf[1], dev->cmd->level);
		}

		s->hw->level = dev->cmd->level[ 1] - '0';
	    }	/* set comand type and level */ 

/*
//  Setting available resolutions and xy ranges for sane frontend.
*/

	s->hw->res_list_size = 0;
	s->hw->res_list = ( SANE_Int *) calloc( s->hw->res_list_size, sizeof( SANE_Int));

	if( NULL == s->hw->res_list) {
		DBG( 0, "out of memory\n");
/*		exit( 0); */
		return SANE_STATUS_NO_MEM;
	}

	{
		int n, k;
		int x = 0, y = 0;

		for( n = ident->count, buf = ident->buf; n; n -= k, buf += k) {
			switch (*buf) {
			case 'R':
			{
				int val = buf[ 2] << 8 | buf[ 1];

				s->hw->res_list_size++;
				s->hw->res_list = ( SANE_Int *) realloc( s->hw->res_list, s->hw->res_list_size * sizeof( SANE_Int));

				if( NULL == s->hw->res_list) {
					DBG( 0, "out of memory\n");
/*					exit( 0); */
					return SANE_STATUS_NO_MEM;
				}

				s->hw->res_list[ s->hw->res_list_size - 1] = ( SANE_Int) val;

				DBG( 1, "resolution (dpi): %d\n", val);
				k = 3;
				continue;
	    		}
			case 'A':
			{
				x = buf[ 2] << 8 | buf[ 1];
				y = buf[ 4] << 8 | buf[ 3];

				DBG( 1, "maximum scan area: x %d y %d\n", x, y);
				k = 5;
				continue;
	    		}
			default:
				break;
			} /* case */

			break;
		} /* for */

		dev->dpi_range.min = s->hw->res_list[ 0];
		dev->dpi_range.max = s->hw->res_list[ s->hw->res_list_size - 1];
		dev->dpi_range.quant = 0;

		dev->fbf_x_range.min = 0;
		dev->fbf_x_range.max = SANE_FIX( x * 25.4 / dev->dpi_range.max);
		dev->fbf_x_range.quant = 0;

		dev->fbf_y_range.min = 0;
		dev->fbf_y_range.max = SANE_FIX( y * 25.4 / dev->dpi_range.max);
		dev->fbf_y_range.quant = 0;

		DBG( 5, "fbf tlx %f tly %f brx %f bry %f [mm]\n"
			, SANE_UNFIX( dev->fbf_x_range.min)
			, SANE_UNFIX( dev->fbf_y_range.min)
			, SANE_UNFIX( dev->fbf_x_range.max)
			, SANE_UNFIX( dev->fbf_y_range.max)
			);

	}
	
	/*
	 * Copy the resolution list to the resolution_list array so that the frontend can
	 * display the correct values
	 */

	resolution_list = malloc( (s->hw->res_list_size +1) * sizeof(SANE_Word));

	if (resolution_list == NULL)
	{
		DBG( 0, "out of memory\n");
		return SANE_STATUS_NO_MEM;
	}
	*resolution_list = s->hw->res_list_size;
	memcpy(&resolution_list[1], s->hw->res_list, s->hw->res_list_size * sizeof(SANE_Word));


	} /* ESC I */

/*
//  Set defaults for no extension.
*/

	dev->x_range = &dev->fbf_x_range;
	dev->y_range = &dev->fbf_y_range;

/*
//  Extended status flag request (ESC f).
//    this also requests the scanner device name from the the scanner
*/
#if 0
	if( SANE_TRUE == dev->extension) 
#endif
	/*
	 * because we are also using the device name from this command, 
	 * we have to run this block even if the scanner does not report
	 * an extension. The extensions are only reported if the ADF or
	 * the TPU are actually detected. 
	 */
	{
		u_char * buf;
		EpsonHdr head;
		SANE_String_Const * source_list_add = source_list;

		if( NULL == ( head = ( EpsonHdr) command( s, "\033f", 2, &status) ) ) {
			DBG( 0, "Extended status flag request failed\n");
			return status;
		}

		buf = &head->buf[ 0];

/*
//  FBF
*/

		*source_list_add++ = FBF_STR;

/*
//  ADF
*/

		if( buf[ 1] & EXT_STATUS_IST) {
			DBG( 1, "ADF detected\n");

			if( buf[ 1] & EXT_STATUS_EN) {
				DBG( 1, "ADF is enabled\n");
				dev->x_range = &dev->adf_x_range;
				dev->y_range = &dev->adf_y_range;
			}

			dev->adf_x_range.min = 0;
			dev->adf_x_range.max = SANE_FIX( ( buf[  3] << 8 | buf[ 2]) * 25.4 / dev->dpi_range.max);
			dev->adf_x_range.quant = 0;

			dev->adf_y_range.min = 0;
			dev->adf_y_range.max = SANE_FIX( ( buf[  5] << 8 | buf[ 4]) * 25.4 / dev->dpi_range.max);
			dev->adf_y_range.quant = 0;

			DBG( 5, "adf tlx %f tly %f brx %f bry %f [mm]\n"
				, SANE_UNFIX( dev->adf_x_range.min)
				, SANE_UNFIX( dev->adf_y_range.min)
				, SANE_UNFIX( dev->adf_x_range.max)
				, SANE_UNFIX( dev->adf_y_range.max)
				);

			*source_list_add++ = ADF_STR;

			dev->ADF = SANE_TRUE;
		}


/*
//  TPU
*/

		if( buf[ 6] & EXT_STATUS_IST) {
			DBG( 1, "TPU detected\n");

			if( buf[ 6] & EXT_STATUS_EN) {
				DBG( 1, "TPU is enabled\n");
				dev->x_range = &dev->tpu_x_range;
				dev->y_range = &dev->tpu_y_range;
			}

			dev->tpu_x_range.min = 0;
			dev->tpu_x_range.max = SANE_FIX( ( buf[  8] << 8 | buf[ 7]) * 25.4 / dev->dpi_range.max);
			dev->tpu_x_range.quant = 0;

			dev->tpu_y_range.min = 0;
			dev->tpu_y_range.max = SANE_FIX( ( buf[ 10] << 8 | buf[ 9]) * 25.4 / dev->dpi_range.max);
			dev->tpu_y_range.quant = 0;

			DBG( 5, "tpu tlx %f tly %f brx %f bry %f [mm]\n"
				, SANE_UNFIX( dev->tpu_x_range.min)
				, SANE_UNFIX( dev->tpu_y_range.min)
				, SANE_UNFIX( dev->tpu_x_range.max)
				, SANE_UNFIX( dev->tpu_y_range.max)
				);

			*source_list_add++ = TPU_STR;

			dev->TPU = SANE_TRUE;
		}

		*source_list_add = NULL;
/*
 *	Get the device name and copy it to dummy_dev.sane.model
 *	The device name starts at buf[0x1A] and is up to 16 bytes long
 *	We are overwriting whatever was set previously!
 */
 		{
#define DEVICE_NAME_LEN	(16)		
			char device_name[DEVICE_NAME_LEN + 1];
			char *end_ptr;
			int len;

			/* make sure that the end of string is marked */
			device_name[DEVICE_NAME_LEN] = '\0';

			/* copy the string to an area where we can work with it */
			memcpy(device_name, buf + 0x1A, DEVICE_NAME_LEN);
			end_ptr = strchr(device_name, ' ');
			if (end_ptr != NULL)
			{
				*end_ptr = '\0';
			}

			len = strlen(device_name);

			/* free the old string */
			if (dummy_dev.sane.model != NULL)
				free(dummy_dev.sane.model);

			str = malloc( len + 1);
			str[len] = '\0';

			/* finally copy the device name to the structure */
			dummy_dev.sane.model = ( char *) memcpy( str, device_name, len);
		}
	}

/*
//  Set values for quick format "max" entry.
*/

	qf_params[ XtNumber( qf_params) - 1].tl_x = dev->x_range->min;
	qf_params[ XtNumber( qf_params) - 1].tl_y = dev->y_range->min;
	qf_params[ XtNumber( qf_params) - 1].br_x = dev->x_range->max;
	qf_params[ XtNumber( qf_params) - 1].br_y = dev->y_range->max;

/*
//
*/

#if 1
	{
		u_char * buf;
		EpsonHdr head;

#define  PUSH_BUTTON_STATUS_EN		0x01

		if( NULL == ( head = ( EpsonHdr) command( s, "\033!", 2, &status) ) ) {
			DBG( 0, "Request the push button status failed\n");
			return status;
		}

		buf = &head->buf[ 0];

		if( buf[ 0] & PUSH_BUTTON_STATUS_EN)
			DBG( 1, "Push button was pressed\n");


	}
#endif

/*
//	now we can finally set the device name
*/
	str = malloc( strlen( dev_name) + 1);
	dummy_dev.sane.name = strcpy( str, dev_name);


	close_scanner( s);

	return SANE_STATUS_GOOD;
}

/*
//
*/

static SANE_Status attach_one ( const char *dev) {
	return attach( dev, 0);
}

/*
//
*/

SANE_Status sane_init (SANE_Int * version_code, SANE_Auth_Callback authorize) {
  char dev_name[PATH_MAX] = "/dev/scanner";
  size_t len;
  FILE *fp;

  DBG_INIT ();
#if defined PACKAGE && defined VERSION
  DBG( 2, "sane_init: " PACKAGE " " VERSION "\n");
#endif

  if( version_code != NULL)
    *version_code = SANE_VERSION_CODE (V_MAJOR, V_MINOR, 0);

  /* default to /dev/scanner instead of insisting on config file */
  if( (fp = sanei_config_open (EPSON_CONFIG_FILE)))
    {
      char line[PATH_MAX];

      while (sanei_config_read (line, sizeof (line), fp))
	{
	  DBG( 4, "sane_init, >%s<\n", line);
	  if( line[0] == '#')		/* ignore line comments */
	    continue;
	  len = strlen (line);
	  if( line[len - 1] == '\n')
            line[--len] = '\0';
	  if( !len)
            continue;			/* ignore empty lines */
	  DBG( 4, "sane_init, >%s<\n", line);
	  strcpy (dev_name, line);
	}
      fclose (fp);
    }

  /* read the option section and assign the connection type to the
     scanner structure - which we don't have at this time. So I have
     to come up with something :-) */

  sanei_config_attach_matching_devices (dev_name, attach_one);
  return SANE_STATUS_GOOD;
}

/*
//
//
*/

void sane_exit ( void) {
	free(dummy_dev.sane.model);
	free(dummy_dev.sane.name);
}

/*
//
//
*/

SANE_Status sane_get_devices ( const SANE_Device * * * device_list, SANE_Bool local_only) {
	static const SANE_Device *devlist [ 2];
	int i;

	i = 0;
	if( dummy_dev.sane.name != NULL)
		devlist[ i++] = &dummy_dev.sane;

	devlist[ i] = NULL;

	*device_list = devlist;
	return SANE_STATUS_GOOD;
}

/*
//
//
*/

static SANE_Status init_options ( Epson_Scanner * s) {
	int i;

	for( i = 0; i < NUM_OPTIONS; ++i) {
		s->opt[ i].size = sizeof( SANE_Word);
		s->opt[ i].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
	}

	s->opt[ OPT_NUM_OPTS].title	= SANE_TITLE_NUM_OPTIONS;
	s->opt[ OPT_NUM_OPTS].desc	= SANE_DESC_NUM_OPTIONS;
	s->opt[ OPT_NUM_OPTS].cap	= SANE_CAP_SOFT_DETECT;
	s->val[ OPT_NUM_OPTS].w		= NUM_OPTIONS;

	/* "Scan Mode" group: */

	s->opt[ OPT_MODE_GROUP].title	= "Scan Mode";
	s->opt[ OPT_MODE_GROUP].desc	= "";
	s->opt[ OPT_MODE_GROUP].type	= SANE_TYPE_GROUP;
	s->opt[ OPT_MODE_GROUP].cap	= 0;

		/* scan mode */
		s->opt[ OPT_MODE].name = SANE_NAME_SCAN_MODE;
		s->opt[ OPT_MODE].title = SANE_TITLE_SCAN_MODE;
		s->opt[ OPT_MODE].desc = SANE_DESC_SCAN_MODE;
		s->opt[ OPT_MODE].type = SANE_TYPE_STRING;
		s->opt[ OPT_MODE].size = max_string_size(mode_list);
		s->opt[ OPT_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
		s->opt[ OPT_MODE].constraint.string_list = mode_list;
		s->val[ OPT_MODE].w = 0;		/* Binary */

		/* halftone */
		s->opt[ OPT_HALFTONE].name	= SANE_NAME_HALFTONE;
		s->opt[ OPT_HALFTONE].title	= SANE_TITLE_HALFTONE;
		s->opt[ OPT_HALFTONE].desc	= "Selects the halftone.";

		s->opt[ OPT_HALFTONE].type = SANE_TYPE_STRING;
		s->opt[ OPT_HALFTONE].size = max_string_size(halftone_list_7);
		s->opt[ OPT_HALFTONE].constraint_type = SANE_CONSTRAINT_STRING_LIST;

	if( s->hw->level >= 7)
		s->opt[ OPT_HALFTONE].constraint.string_list = halftone_list_7;
	else if( s->hw->level >= 4)
		s->opt[ OPT_HALFTONE].constraint.string_list = halftone_list_4;
	else
		s->opt[ OPT_HALFTONE].constraint.string_list = halftone_list;

		s->val[ OPT_HALFTONE].w = 1;	/* Halftone A */

		/* dropout */
		s->opt[ OPT_DROPOUT].name = "dropout";
		s->opt[ OPT_DROPOUT].title = "Dropout";
		s->opt[ OPT_DROPOUT].desc = "Selects the dropout.";

		s->opt[ OPT_DROPOUT].type = SANE_TYPE_STRING;
		s->opt[ OPT_DROPOUT].size = max_string_size(dropout_list);
		s->opt[ OPT_DROPOUT].cap |= SANE_CAP_ADVANCED;
		s->opt[ OPT_DROPOUT].constraint_type = SANE_CONSTRAINT_STRING_LIST;
		s->opt[ OPT_DROPOUT].constraint.string_list = dropout_list;
		s->val[ OPT_DROPOUT].w = 0;	/* None */

		/* brightness */
		s->opt[ OPT_BRIGHTNESS].name = SANE_NAME_BRIGHTNESS;
		s->opt[ OPT_BRIGHTNESS].title = SANE_TITLE_BRIGHTNESS;
		s->opt[ OPT_BRIGHTNESS].desc = "Selects the brightness.";

		s->opt[ OPT_BRIGHTNESS].type = SANE_TYPE_INT;
		s->opt[ OPT_BRIGHTNESS].unit = SANE_UNIT_NONE;
		s->opt[ OPT_BRIGHTNESS].constraint_type = SANE_CONSTRAINT_RANGE;
		s->opt[ OPT_BRIGHTNESS].constraint.range = &s->hw->cmd->bright_range;
		s->val[ OPT_BRIGHTNESS].w = 0;	/* Normal */

		if( ! s->hw->cmd->set_bright) {
			s->opt[ OPT_BRIGHTNESS].cap |= SANE_CAP_INACTIVE;
		}

		/* sharpness */
		s->opt[ OPT_SHARPNESS].name     = "sharpness";
		s->opt[ OPT_SHARPNESS].title    = "Sharpness";
		s->opt[ OPT_SHARPNESS].desc     = "";

		s->opt[ OPT_SHARPNESS].type = SANE_TYPE_INT;
		s->opt[ OPT_SHARPNESS].unit = SANE_UNIT_NONE;
		s->opt[ OPT_SHARPNESS].constraint_type = SANE_CONSTRAINT_RANGE;
		s->opt[ OPT_SHARPNESS].constraint.range = &outline_emphasis_range;
		s->val[ OPT_SHARPNESS].w = 0;	/* Normal */

		if( ! s->hw->cmd->set_outline_emphasis) {
			s->opt[ OPT_SHARPNESS].cap |= SANE_CAP_INACTIVE;
		}


		/* gamma */
		s->opt[ OPT_GAMMA_CORRECTION].name     = SANE_NAME_GAMMA_CORRECTION;
		s->opt[ OPT_GAMMA_CORRECTION].title    = SANE_TITLE_GAMMA_CORRECTION;
		s->opt[ OPT_GAMMA_CORRECTION].desc     = SANE_DESC_GAMMA_CORRECTION;

		s->opt[ OPT_GAMMA_CORRECTION].type = SANE_TYPE_STRING;
		s->opt[ OPT_GAMMA_CORRECTION].size = max_string_size(gamma_list);
/*		s->opt[ OPT_GAMMA_CORRECTION].cap |= SANE_CAP_ADVANCED; */
		s->opt[ OPT_GAMMA_CORRECTION].constraint_type = SANE_CONSTRAINT_STRING_LIST;
		s->opt[ OPT_GAMMA_CORRECTION].constraint.string_list = gamma_list;
		s->val[ OPT_GAMMA_CORRECTION].w = 0;		/* Default */

		if( ! s->hw->cmd->set_gamma) {
			s->opt[ OPT_GAMMA_CORRECTION].cap |= SANE_CAP_INACTIVE;
		}


		/* gamma vector */
		s->opt[ OPT_GAMMA_VECTOR].name  = SANE_NAME_GAMMA_VECTOR;
		s->opt[ OPT_GAMMA_VECTOR].title = SANE_TITLE_GAMMA_VECTOR;
		s->opt[ OPT_GAMMA_VECTOR].desc  = SANE_DESC_GAMMA_VECTOR;

		s->opt[ OPT_GAMMA_VECTOR].type = SANE_TYPE_INT;
/*		s->opt[ OPT_GAMMA_VECTOR].cap |= SANE_CAP_ADVANCED; */
		s->opt[ OPT_GAMMA_VECTOR].unit = SANE_UNIT_NONE;
		s->opt[ OPT_GAMMA_VECTOR].size = 256 * sizeof (SANE_Word);
		s->opt[ OPT_GAMMA_VECTOR].constraint_type = SANE_CONSTRAINT_RANGE;
		s->opt[ OPT_GAMMA_VECTOR].constraint.range = &u8_range;
		s->val[ OPT_GAMMA_VECTOR].wa = &s->gamma_table [ 0] [ 0];


		/* red gamma vector */
		s->opt[ OPT_GAMMA_VECTOR_R].name  = SANE_NAME_GAMMA_VECTOR_R;
		s->opt[ OPT_GAMMA_VECTOR_R].title = SANE_TITLE_GAMMA_VECTOR_R;
		s->opt[ OPT_GAMMA_VECTOR_R].desc  = SANE_DESC_GAMMA_VECTOR_R;

		s->opt[ OPT_GAMMA_VECTOR_R].type = SANE_TYPE_INT;
/*		s->opt[ OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_ADVANCED; */
		s->opt[ OPT_GAMMA_VECTOR_R].unit = SANE_UNIT_NONE;
		s->opt[ OPT_GAMMA_VECTOR_R].size = 256 * sizeof (SANE_Word);
		s->opt[ OPT_GAMMA_VECTOR_R].constraint_type = SANE_CONSTRAINT_RANGE;
		s->opt[ OPT_GAMMA_VECTOR_R].constraint.range = &u8_range;
		s->val[ OPT_GAMMA_VECTOR_R].wa = &s->gamma_table [ 1] [ 0];


		/* green gamma vector */
		s->opt[ OPT_GAMMA_VECTOR_G].name  = SANE_NAME_GAMMA_VECTOR_G;
		s->opt[ OPT_GAMMA_VECTOR_G].title = SANE_TITLE_GAMMA_VECTOR_G;
		s->opt[ OPT_GAMMA_VECTOR_G].desc  = SANE_DESC_GAMMA_VECTOR_G;

		s->opt[ OPT_GAMMA_VECTOR_G].type = SANE_TYPE_INT;
/*		s->opt[ OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_ADVANCED; */
		s->opt[ OPT_GAMMA_VECTOR_G].unit = SANE_UNIT_NONE;
		s->opt[ OPT_GAMMA_VECTOR_G].size = 256 * sizeof (SANE_Word);
		s->opt[ OPT_GAMMA_VECTOR_G].constraint_type = SANE_CONSTRAINT_RANGE;
		s->opt[ OPT_GAMMA_VECTOR_G].constraint.range = &u8_range;
		s->val[ OPT_GAMMA_VECTOR_G].wa = &s->gamma_table [ 2] [ 0];


		/* red gamma vector */
		s->opt[ OPT_GAMMA_VECTOR_B].name  = SANE_NAME_GAMMA_VECTOR_B;
		s->opt[ OPT_GAMMA_VECTOR_B].title = SANE_TITLE_GAMMA_VECTOR_B;
		s->opt[ OPT_GAMMA_VECTOR_B].desc  = SANE_DESC_GAMMA_VECTOR_B;

		s->opt[ OPT_GAMMA_VECTOR_B].type = SANE_TYPE_INT;
/*		s->opt[ OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_ADVANCED; */
		s->opt[ OPT_GAMMA_VECTOR_B].unit = SANE_UNIT_NONE;
		s->opt[ OPT_GAMMA_VECTOR_B].size = 256 * sizeof (SANE_Word);
		s->opt[ OPT_GAMMA_VECTOR_B].constraint_type = SANE_CONSTRAINT_RANGE;
		s->opt[ OPT_GAMMA_VECTOR_B].constraint.range = &u8_range;
		s->val[ OPT_GAMMA_VECTOR_B].wa = &s->gamma_table [ 3] [ 0];

#if 0
		if( ! s->hw->cmd->set_gamma_table) {
#endif
			s->opt[ OPT_GAMMA_VECTOR].cap |= SANE_CAP_INACTIVE;
			s->opt[ OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
			s->opt[ OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
			s->opt[ OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;
#if 0
		}
#endif


		/* color correction */
		s->opt[ OPT_COLOR_CORRECTION].name     = "color-correction";
		s->opt[ OPT_COLOR_CORRECTION].title    = "Color correction";
		s->opt[ OPT_COLOR_CORRECTION].desc     = "Sets the color correction table for the selected output device.";

		s->opt[ OPT_COLOR_CORRECTION].type = SANE_TYPE_STRING;
		s->opt[ OPT_COLOR_CORRECTION].size = 32;
		s->opt[ OPT_COLOR_CORRECTION].cap |= SANE_CAP_ADVANCED;
		s->opt[ OPT_COLOR_CORRECTION].constraint_type = SANE_CONSTRAINT_STRING_LIST;
		s->opt[ OPT_COLOR_CORRECTION].constraint.string_list = color_list;
/*		s->val[ OPT_COLOR_CORRECTION].w = 5; */		/* scanner default: CRT monitors */
		s->val[ OPT_COLOR_CORRECTION].w = 4;		/* scanner default: CRT monitors */

		if( ! s->hw->cmd->set_color_correction) {
			s->opt[ OPT_COLOR_CORRECTION].cap |= SANE_CAP_INACTIVE;
		}

		/* resolution */
		s->opt[ OPT_RESOLUTION].name = SANE_NAME_SCAN_RESOLUTION;
		s->opt[ OPT_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
		s->opt[ OPT_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;

		s->opt[ OPT_RESOLUTION].type = SANE_TYPE_INT;
		s->opt[ OPT_RESOLUTION].unit = SANE_UNIT_DPI;
		s->opt[ OPT_RESOLUTION].constraint_type = SANE_CONSTRAINT_WORD_LIST;
		s->opt[ OPT_RESOLUTION].constraint.word_list = resolution_list;
		s->val[ OPT_RESOLUTION].w = s->hw->dpi_range.min;


	s->opt[ OPT_CCT_GROUP].title	= "Color correction coefficients";
	s->opt[ OPT_CCT_GROUP].desc	= "";
	s->opt[ OPT_CCT_GROUP].type	= SANE_TYPE_GROUP;
	s->opt[ OPT_CCT_GROUP].cap	= SANE_CAP_ADVANCED;


		/* color correction coefficients */
		s->opt[ OPT_CCT_1].name  = "cct-1";
		s->opt[ OPT_CCT_2].name  = "cct-2";
		s->opt[ OPT_CCT_3].name  = "cct-3";
		s->opt[ OPT_CCT_4].name  = "cct-4";
		s->opt[ OPT_CCT_5].name  = "cct-5";
		s->opt[ OPT_CCT_6].name  = "cct-6";
		s->opt[ OPT_CCT_7].name  = "cct-7";
		s->opt[ OPT_CCT_8].name  = "cct-8";
		s->opt[ OPT_CCT_9].name  = "cct-9";

		s->opt[ OPT_CCT_1].title = "d1";
		s->opt[ OPT_CCT_2].title = "d2";
		s->opt[ OPT_CCT_3].title = "d3";
		s->opt[ OPT_CCT_4].title = "d4";
		s->opt[ OPT_CCT_5].title = "d5";
		s->opt[ OPT_CCT_6].title = "d6";
		s->opt[ OPT_CCT_7].title = "d7";
		s->opt[ OPT_CCT_8].title = "d8";
		s->opt[ OPT_CCT_9].title = "d9";

		s->opt[ OPT_CCT_1].desc  = "";
		s->opt[ OPT_CCT_2].desc  = "";
		s->opt[ OPT_CCT_3].desc  = "";
		s->opt[ OPT_CCT_4].desc  = "";
		s->opt[ OPT_CCT_5].desc  = "";
		s->opt[ OPT_CCT_6].desc  = "";
		s->opt[ OPT_CCT_7].desc  = "";
		s->opt[ OPT_CCT_8].desc  = "";
		s->opt[ OPT_CCT_9].desc  = "";

		s->opt[ OPT_CCT_1].type = SANE_TYPE_INT;
		s->opt[ OPT_CCT_2].type = SANE_TYPE_INT;
		s->opt[ OPT_CCT_3].type = SANE_TYPE_INT;
		s->opt[ OPT_CCT_4].type = SANE_TYPE_INT;
		s->opt[ OPT_CCT_5].type = SANE_TYPE_INT;
		s->opt[ OPT_CCT_6].type = SANE_TYPE_INT;
		s->opt[ OPT_CCT_7].type = SANE_TYPE_INT;
		s->opt[ OPT_CCT_8].type = SANE_TYPE_INT;
		s->opt[ OPT_CCT_9].type = SANE_TYPE_INT;

		s->opt[ OPT_CCT_1].cap |= SANE_CAP_ADVANCED;
		s->opt[ OPT_CCT_2].cap |= SANE_CAP_ADVANCED;
		s->opt[ OPT_CCT_3].cap |= SANE_CAP_ADVANCED;
		s->opt[ OPT_CCT_4].cap |= SANE_CAP_ADVANCED;
		s->opt[ OPT_CCT_5].cap |= SANE_CAP_ADVANCED;
		s->opt[ OPT_CCT_6].cap |= SANE_CAP_ADVANCED;
		s->opt[ OPT_CCT_7].cap |= SANE_CAP_ADVANCED;
		s->opt[ OPT_CCT_8].cap |= SANE_CAP_ADVANCED;
		s->opt[ OPT_CCT_9].cap |= SANE_CAP_ADVANCED;

		s->opt[ OPT_CCT_1].unit = SANE_UNIT_NONE;
		s->opt[ OPT_CCT_2].unit = SANE_UNIT_NONE;
		s->opt[ OPT_CCT_3].unit = SANE_UNIT_NONE;
		s->opt[ OPT_CCT_4].unit = SANE_UNIT_NONE;
		s->opt[ OPT_CCT_5].unit = SANE_UNIT_NONE;
		s->opt[ OPT_CCT_6].unit = SANE_UNIT_NONE;
		s->opt[ OPT_CCT_7].unit = SANE_UNIT_NONE;
		s->opt[ OPT_CCT_8].unit = SANE_UNIT_NONE;
		s->opt[ OPT_CCT_9].unit = SANE_UNIT_NONE;

		s->opt[ OPT_CCT_1].constraint_type = SANE_CONSTRAINT_RANGE;
		s->opt[ OPT_CCT_2].constraint_type = SANE_CONSTRAINT_RANGE;
		s->opt[ OPT_CCT_3].constraint_type = SANE_CONSTRAINT_RANGE;
		s->opt[ OPT_CCT_4].constraint_type = SANE_CONSTRAINT_RANGE;
		s->opt[ OPT_CCT_5].constraint_type = SANE_CONSTRAINT_RANGE;
		s->opt[ OPT_CCT_6].constraint_type = SANE_CONSTRAINT_RANGE;
		s->opt[ OPT_CCT_7].constraint_type = SANE_CONSTRAINT_RANGE;
		s->opt[ OPT_CCT_8].constraint_type = SANE_CONSTRAINT_RANGE;
		s->opt[ OPT_CCT_9].constraint_type = SANE_CONSTRAINT_RANGE;

		s->opt[ OPT_CCT_1].constraint.range = &s8_range;
		s->opt[ OPT_CCT_2].constraint.range = &s8_range;
		s->opt[ OPT_CCT_3].constraint.range = &s8_range;
		s->opt[ OPT_CCT_4].constraint.range = &s8_range;
		s->opt[ OPT_CCT_5].constraint.range = &s8_range;
		s->opt[ OPT_CCT_6].constraint.range = &s8_range;
		s->opt[ OPT_CCT_7].constraint.range = &s8_range;
		s->opt[ OPT_CCT_8].constraint.range = &s8_range;
		s->opt[ OPT_CCT_9].constraint.range = &s8_range;

		s->val[ OPT_CCT_1].w = 0;
		s->val[ OPT_CCT_2].w = 0;
		s->val[ OPT_CCT_3].w = 0;
		s->val[ OPT_CCT_4].w = 0;
		s->val[ OPT_CCT_5].w = 0;
		s->val[ OPT_CCT_6].w = 0;
		s->val[ OPT_CCT_7].w = 0;
		s->val[ OPT_CCT_8].w = 0;
		s->val[ OPT_CCT_9].w = 0;

/*		if( ! s->hw->cmd->set_color_correction_coefficients) */{
			s->opt[ OPT_CCT_1].cap |= SANE_CAP_INACTIVE;
			s->opt[ OPT_CCT_2].cap |= SANE_CAP_INACTIVE;
			s->opt[ OPT_CCT_3].cap |= SANE_CAP_INACTIVE;
			s->opt[ OPT_CCT_4].cap |= SANE_CAP_INACTIVE;
			s->opt[ OPT_CCT_5].cap |= SANE_CAP_INACTIVE;
			s->opt[ OPT_CCT_6].cap |= SANE_CAP_INACTIVE;
			s->opt[ OPT_CCT_7].cap |= SANE_CAP_INACTIVE;
			s->opt[ OPT_CCT_8].cap |= SANE_CAP_INACTIVE;
			s->opt[ OPT_CCT_9].cap |= SANE_CAP_INACTIVE;
		}


	/* "Advanced" group: */
	s->opt[ OPT_ADVANCED_GROUP].title	= "Advanced";
	s->opt[ OPT_ADVANCED_GROUP].desc	= "";
	s->opt[ OPT_ADVANCED_GROUP].type	= SANE_TYPE_GROUP;
	s->opt[ OPT_ADVANCED_GROUP].cap		= SANE_CAP_ADVANCED;


		/* mirror */
		s->opt[ OPT_MIRROR].name     = "mirror";
		s->opt[ OPT_MIRROR].title    = "Mirror image";
		s->opt[ OPT_MIRROR].desc     = "Mirror the image.";

		s->opt[ OPT_MIRROR].type = SANE_TYPE_BOOL;
		s->val[ OPT_MIRROR].w = SANE_FALSE;

		if( ! s->hw->cmd->mirror_image) {
			s->opt[ OPT_MIRROR].cap |= SANE_CAP_INACTIVE;
		}


		/* speed */
		s->opt[ OPT_SPEED].name     = "speed";
		s->opt[ OPT_SPEED].title    = "Speed";
		s->opt[ OPT_SPEED].desc     = "";

		s->opt[ OPT_SPEED].type = SANE_TYPE_BOOL;
		s->val[ OPT_SPEED].w = SANE_FALSE;

		if( ! s->hw->cmd->set_speed) {
			s->opt[ OPT_SPEED].cap |= SANE_CAP_INACTIVE;
		}

		/* preview speed */
		s->opt[ OPT_PREVIEW_SPEED].name     = "preview-speed";
		s->opt[ OPT_PREVIEW_SPEED].title    = "Speed";
		s->opt[ OPT_PREVIEW_SPEED].desc     = "";

		s->opt[ OPT_PREVIEW_SPEED].type = SANE_TYPE_BOOL;
		s->val[ OPT_PREVIEW_SPEED].w = SANE_FALSE;

		if( ! s->hw->cmd->set_speed) {
			s->opt[ OPT_PREVIEW_SPEED].cap |= SANE_CAP_INACTIVE;
		}

		/* auto area segmentation */
		s->opt[ OPT_AAS].name	= "auto-area-segmentation";
		s->opt[ OPT_AAS].title	= "Auto area segmentation";
		s->opt[ OPT_AAS].desc	= "";

		s->opt[ OPT_AAS].type	= SANE_TYPE_BOOL;
		s->val[ OPT_AAS].w	= SANE_TRUE;

		if( ! s->hw->cmd->control_auto_area_segmentation) {
			s->opt[ OPT_AAS].cap |= SANE_CAP_INACTIVE;
		}

	/* "Preview settings" group: */
	s->opt[ OPT_PREVIEW_GROUP].title = SANE_TITLE_PREVIEW;
	s->opt[ OPT_PREVIEW_GROUP].desc = "";
	s->opt[ OPT_PREVIEW_GROUP].type = SANE_TYPE_GROUP;
	s->opt[ OPT_PREVIEW_GROUP].cap = SANE_CAP_ADVANCED;

		/* preview */
		s->opt[ OPT_PREVIEW].name     = SANE_NAME_PREVIEW;
		s->opt[ OPT_PREVIEW].title    = SANE_TITLE_PREVIEW;
		s->opt[ OPT_PREVIEW].desc     = SANE_DESC_PREVIEW;

		s->opt[ OPT_PREVIEW].type = SANE_TYPE_BOOL;
		s->val[ OPT_PREVIEW].w = SANE_FALSE;

	/* "Geometry" group: */
	s->opt[ OPT_GEOMETRY_GROUP].title = "Geometry";
	s->opt[ OPT_GEOMETRY_GROUP].desc = "";
	s->opt[ OPT_GEOMETRY_GROUP].type = SANE_TYPE_GROUP;
	s->opt[ OPT_GEOMETRY_GROUP].cap = SANE_CAP_ADVANCED;

		/* top-left x */
		s->opt[ OPT_TL_X].name = SANE_NAME_SCAN_TL_X;
		s->opt[ OPT_TL_X].title = SANE_TITLE_SCAN_TL_X;
		s->opt[ OPT_TL_X].desc = SANE_DESC_SCAN_TL_X;

		s->opt[ OPT_TL_X].type = SANE_TYPE_FIXED;
		s->opt[ OPT_TL_X].unit = SANE_UNIT_MM;
		s->opt[ OPT_TL_X].constraint_type = SANE_CONSTRAINT_RANGE;
		s->opt[ OPT_TL_X].constraint.range = s->hw->x_range;
		s->val[ OPT_TL_X].w = 0;

		/* top-left y */
		s->opt[ OPT_TL_Y].name = SANE_NAME_SCAN_TL_Y;
		s->opt[ OPT_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
		s->opt[ OPT_TL_Y].desc = SANE_DESC_SCAN_TL_Y;

		s->opt[ OPT_TL_Y].type = SANE_TYPE_FIXED;
		s->opt[ OPT_TL_Y].unit = SANE_UNIT_MM;
		s->opt[ OPT_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
		s->opt[ OPT_TL_Y].constraint.range = s->hw->y_range;
		s->val[ OPT_TL_Y].w = 0;

		/* bottom-right x */
		s->opt[ OPT_BR_X].name = SANE_NAME_SCAN_BR_X;
		s->opt[ OPT_BR_X].title = SANE_TITLE_SCAN_BR_X;
		s->opt[ OPT_BR_X].desc = SANE_DESC_SCAN_BR_X;

		s->opt[ OPT_BR_X].type = SANE_TYPE_FIXED;
		s->opt[ OPT_BR_X].unit = SANE_UNIT_MM;
		s->opt[ OPT_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
		s->opt[ OPT_BR_X].constraint.range = s->hw->x_range;
		s->val[ OPT_BR_X].w = s->hw->x_range->max;

		/* bottom-right y */
		s->opt[ OPT_BR_Y].name = SANE_NAME_SCAN_BR_Y;
		s->opt[ OPT_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
		s->opt[ OPT_BR_Y].desc = SANE_DESC_SCAN_BR_Y;

		s->opt[ OPT_BR_Y].type = SANE_TYPE_FIXED;
		s->opt[ OPT_BR_Y].unit = SANE_UNIT_MM;
		s->opt[ OPT_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
		s->opt[ OPT_BR_Y].constraint.range = s->hw->y_range;
		s->val[ OPT_BR_Y].w = s->hw->y_range->max;

		/* Quick format */
		s->opt[ OPT_QUICK_FORMAT].name     = "quick-format";
		s->opt[ OPT_QUICK_FORMAT].title    = "Quick format";
		s->opt[ OPT_QUICK_FORMAT].desc     = "";

		s->opt[ OPT_QUICK_FORMAT].type = SANE_TYPE_STRING;
		s->opt[ OPT_QUICK_FORMAT].size = max_string_size(qf_list);
		s->opt[ OPT_QUICK_FORMAT].cap |= SANE_CAP_ADVANCED;
		s->opt[ OPT_QUICK_FORMAT].constraint_type = SANE_CONSTRAINT_STRING_LIST;
		s->opt[ OPT_QUICK_FORMAT].constraint.string_list = qf_list;
		s->val[ OPT_QUICK_FORMAT].w = XtNumber( qf_params) - 1;		/* max */

	/* "Optional equipment" group: */
	s->opt[ OPT_EQU_GROUP].title	= "Optional equipment";
	s->opt[ OPT_EQU_GROUP].desc	= "";
	s->opt[ OPT_EQU_GROUP].type	= SANE_TYPE_GROUP;
	s->opt[ OPT_EQU_GROUP].cap	= SANE_CAP_ADVANCED;


		/* source */
		s->opt[ OPT_SOURCE].name	= SANE_NAME_SCAN_SOURCE;
		s->opt[ OPT_SOURCE].title	= SANE_TITLE_SCAN_SOURCE;
		s->opt[ OPT_SOURCE].desc	= SANE_DESC_SCAN_SOURCE;

		s->opt[ OPT_SOURCE].type	= SANE_TYPE_STRING;
		s->opt[ OPT_SOURCE].size	= max_string_size(source_list);

		s->opt[ OPT_SOURCE].constraint_type		= SANE_CONSTRAINT_STRING_LIST;
		s->opt[ OPT_SOURCE].constraint.string_list	= source_list;

		if( ! s->hw->extension) {
			s->opt[ OPT_SOURCE].cap |= SANE_CAP_INACTIVE;
		}
		s->val[ OPT_SOURCE].w	= 0;	/* always use Flatbed as default */


		/* film type */
		s->opt[ OPT_FILM_TYPE].name	= "film-type";
		s->opt[ OPT_FILM_TYPE].title	= "Film type";
		s->opt[ OPT_FILM_TYPE].desc	= "";

		s->opt[ OPT_FILM_TYPE].type	= SANE_TYPE_STRING;
		s->opt[ OPT_FILM_TYPE].size	= max_string_size(film_list);

		s->opt[ OPT_FILM_TYPE].constraint_type		= SANE_CONSTRAINT_STRING_LIST;
		s->opt[ OPT_FILM_TYPE].constraint.string_list	= film_list;

		s->val[ OPT_FILM_TYPE].w	= 0;

		if( ( ! s->hw->TPU) && ( ! s->hw->cmd->set_bay) ) {		/* Hack: Using set_bay to indicate. */
			s->opt[ OPT_FILM_TYPE].cap |= SANE_CAP_INACTIVE;
		}


		/* forward feed / eject */
		s->opt[ OPT_EJECT].name		= "eject";
		s->opt[ OPT_EJECT].title	= "Eject";
		s->opt[ OPT_EJECT].desc		= "";

		s->opt[ OPT_EJECT].type	= SANE_TYPE_BUTTON;

		if( ( ! s->hw->ADF) && ( ! s->hw->cmd->set_bay) ) {		/* Hack: Using set_bay to indicate. */
			s->opt[ OPT_EJECT].cap |= SANE_CAP_INACTIVE;
		}


		/* auto forward feed / eject */
		s->opt[ OPT_AUTO_EJECT].name	= "auto-eject";
		s->opt[ OPT_AUTO_EJECT].title	= "Auto eject";
		s->opt[ OPT_AUTO_EJECT].desc	= "Eject document after scanning";

		s->opt[ OPT_AUTO_EJECT].type	= SANE_TYPE_BOOL;
		s->val[ OPT_AUTO_EJECT].w	= SANE_FALSE;

		if( ! s->hw->ADF) {		/* Hack: Using set_bay to indicate. */
			s->opt[ OPT_AUTO_EJECT].cap |= SANE_CAP_INACTIVE;
		}


		/* select bay */
		s->opt[ OPT_BAY].name		= "bay";
		s->opt[ OPT_BAY].title		= "Bay";
		s->opt[ OPT_BAY].desc		= "select bay to scan";

		s->opt[ OPT_BAY].type		= SANE_TYPE_STRING;
		s->opt[ OPT_BAY].size		= max_string_size(bay_list);
		s->opt[ OPT_BAY].constraint_type		= SANE_CONSTRAINT_STRING_LIST;
		s->opt[ OPT_BAY].constraint.string_list		= bay_list;
		s->val[ OPT_BAY].w		= 0;					/* Bay 1 */

		if( ! s->hw->cmd->set_bay) {
			s->opt[ OPT_BAY].cap |= SANE_CAP_INACTIVE;
		}


#if 0
		/* button test */
		s->opt[ OPT_FORMAT_MAX].name     = "max.";
		s->opt[ OPT_FORMAT_MAX].title    = "max.";
		s->opt[ OPT_FORMAT_MAX].desc     = "";

		s->opt[ OPT_FORMAT_MAX].type = SANE_TYPE_BUTTON;
		s->opt[ OPT_FORMAT_MAX].unit = SANE_UNIT_NONE;
		s->opt[ OPT_FORMAT_MAX].constraint_type = SANE_CONSTRAINT_NONE;
		s->opt[ OPT_FORMAT_MAX].constraint.range = NULL;
#endif






	return SANE_STATUS_GOOD;
}

/*
//
//
*/

SANE_Status sane_open ( SANE_String_Const devicename, SANE_Handle * handle) {
	Epson_Scanner * s;

/* ????? */
	if( devicename[ 0] == '\0')
		devicename = dummy_dev.sane.name;

/* ????? */
	if( ! devicename || devicename[0] == '\0' || strcmp( devicename, dummy_dev.sane.name) != 0)
		return SANE_STATUS_INVAL;

	if( NULL == ( s = calloc( 1, sizeof( Epson_Scanner) ) ) )
		return SANE_STATUS_NO_MEM;

	s->fd = -1;
	s->hw = &dummy_dev;
	init_options( s);

	*handle = ( SANE_Handle) s;
	return SANE_STATUS_GOOD;
}

/*
//
//
*/

void sane_close ( SANE_Handle handle) {
	Epson_Scanner * s = ( Epson_Scanner *) handle;

	free( s->buf);

	if( s->fd != -1)
		sanei_scsi_close( s->fd);

	free( s);
}

/*
//
//
*/

const SANE_Option_Descriptor * sane_get_option_descriptor ( SANE_Handle handle, SANE_Int option) {
	Epson_Scanner *s = (Epson_Scanner *) handle;

	if( option < 0 || option >= NUM_OPTIONS)
		return NULL;

	return s->opt + option;
}

/*
//
//
*/

static const SANE_String_Const * search_string_list ( const SANE_String_Const * list, SANE_String value) {
	while( *list != NULL && strcmp( value, *list) != 0)
		++list;

	if( *list == NULL)
		return NULL;

	return list;
}

/*
//
//
*/

SANE_Status sane_control_option ( SANE_Handle handle, SANE_Int option, SANE_Action action, void * value, SANE_Int * info) {
	Epson_Scanner * s = ( Epson_Scanner *) handle;
	SANE_Status status;
	const SANE_String_Const * optval;

	if( option < 0 || option >= NUM_OPTIONS)
		return SANE_STATUS_INVAL;

	if( info != NULL)
		*info = 0;

	switch (action) {

	case SANE_ACTION_GET_VALUE:

		switch (option) {

		/* word-array options: */

		case OPT_GAMMA_VECTOR:
		case OPT_GAMMA_VECTOR_R:
		case OPT_GAMMA_VECTOR_G:
		case OPT_GAMMA_VECTOR_B:
			memcpy( value, s->val[ option].wa, s->opt[ option].size);
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
			*( ( SANE_Word *) value) = s->val[ option].w;
			break;

		case OPT_MODE:
		case OPT_HALFTONE:
		case OPT_DROPOUT:
		case OPT_QUICK_FORMAT:
		case OPT_SOURCE:
		case OPT_FILM_TYPE:
		case OPT_GAMMA_CORRECTION:
		case OPT_COLOR_CORRECTION:
		case OPT_BAY:
			strcpy( ( char *) value, s->opt[ option].constraint.string_list[ s->val[ option].w]);
			break;
#if 0
		case OPT_MODEL:
			strcpy( value, s->val[ option].s);
			break;
#endif


		default:
			return SANE_STATUS_INVAL;

		}

		break;
	case SANE_ACTION_SET_VALUE:
		status = sanei_constrain_value( s->opt + option, value, info);

		if( status != SANE_STATUS_GOOD)
			return status;

		optval = NULL;

		if( s->opt[ option].constraint_type == SANE_CONSTRAINT_STRING_LIST) {
			optval = search_string_list( s->opt[ option].constraint.string_list, ( char *) value);

			if( optval == NULL)
				return SANE_STATUS_INVAL;
		}

		switch (option) {

		/* side-effect-free word-array options: */

		case OPT_GAMMA_VECTOR:
		case OPT_GAMMA_VECTOR_R:
		case OPT_GAMMA_VECTOR_G:
		case OPT_GAMMA_VECTOR_B:
			memcpy( s->val[ option].wa, value, s->opt[ option].size);
			break;

		case OPT_EJECT:
/*			return eject( s); */
			eject( s);
			break;
			
		case OPT_RESOLUTION:
		{
			int n, k = 0, f;
			int min_d = s->hw->res_list[ s->hw->res_list_size - 1];
			int v = *(SANE_Word *) value;
			int best = v;
			int * last = ( OPT_RESOLUTION == option) ? &s->hw->last_res : &s->hw->last_res_preview;

			for( n = 0; n < s->hw->res_list_size; n++) {
				int d = abs( v - s->hw->res_list[ n]);

				if( d < min_d) {
					min_d = d;
					k = n;
					best = s->hw->res_list[ n];
			  	}
			}

/*
// problem: does not reach all values cause of scroll bar resolution.
//
*/
			if( (v != best) && *last) {
				for( f = 0; f < s->hw->res_list_size; f++)
					if( *last == s->hw->res_list[ f])
						break;

				if( f != k && f != k - 1 && f != k + 1) {

					if( k > f)
						best = s->hw->res_list[ f + 1];
								
					if( k < f)
						best = s->hw->res_list[ f - 1];
				}
			}

			*last = best;
			s->val[ option].w = ( SANE_Word) best;

			DBG(3, "Selected resolution %d dpi\n", best);

			break;
		}

		case OPT_CCT_1:
		case OPT_CCT_2:
		case OPT_CCT_3:
		case OPT_CCT_4:
		case OPT_CCT_5:
		case OPT_CCT_6:
		case OPT_CCT_7:
		case OPT_CCT_8:
		case OPT_CCT_9:
			s->val[ option].w = *( ( SANE_Word *) value);
			break;

		case OPT_TL_X:
		case OPT_TL_Y:
		case OPT_BR_X:
		case OPT_BR_Y:

			s->val[ option].w = *( ( SANE_Word *) value);

			DBG( 1, "set = %f\n", SANE_UNFIX( s->val[ option].w));

			if( NULL != info)
				*info |= SANE_INFO_RELOAD_PARAMS;

			break;

	case OPT_SOURCE:
	{
		int force_max = SANE_FALSE;

		DBG( 1, "source = %s\n", ( char *) value);

		if(  s->val[ OPT_TL_X].w == s->hw->x_range->min
		  && s->val[ OPT_TL_Y].w == s->hw->y_range->min
		  && s->val[ OPT_BR_X].w == s->hw->x_range->max
		  && s->val[ OPT_BR_Y].w == s->hw->y_range->max
		  )
			force_max = SANE_TRUE;

		if( ! strcmp( ADF_STR,( char *) value) ) {
			s->hw->x_range = &s->hw->adf_x_range;
			s->hw->y_range = &s->hw->adf_y_range;
			s->hw->use_extension = SANE_TRUE;
		} else if( ! strcmp( TPU_STR,( char *) value) ) {
			s->hw->x_range = &s->hw->tpu_x_range;
			s->hw->y_range = &s->hw->tpu_y_range;
			s->hw->use_extension = SANE_TRUE;
		} else {
			s->hw->x_range = &s->hw->fbf_x_range;
			s->hw->y_range = &s->hw->fbf_y_range;
			s->hw->use_extension = SANE_FALSE;
		}

		qf_params[ XtNumber( qf_params) - 1].tl_x = s->hw->x_range->min;
		qf_params[ XtNumber( qf_params) - 1].tl_y = s->hw->y_range->min;
		qf_params[ XtNumber( qf_params) - 1].br_x = s->hw->x_range->max;
		qf_params[ XtNumber( qf_params) - 1].br_y = s->hw->y_range->max;

		s->opt[ OPT_BR_X].constraint.range = s->hw->x_range;
		s->opt[ OPT_BR_Y].constraint.range = s->hw->y_range;

		if( s->val[ OPT_TL_X].w < s->hw->x_range->min || force_max)
			s->val[ OPT_TL_X].w = s->hw->x_range->min;

		if( s->val[ OPT_TL_Y].w < s->hw->y_range->min || force_max)
			s->val[ OPT_TL_Y].w = s->hw->y_range->min;

		if( s->val[ OPT_BR_X].w > s->hw->x_range->max || force_max)
			s->val[ OPT_BR_X].w = s->hw->x_range->max;

		if( s->val[ OPT_BR_Y].w > s->hw->y_range->max || force_max)
			s->val[ OPT_BR_Y].w = s->hw->y_range->max;

		if( NULL != info)
			*info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;

		s->val[ option].w = optval - s->opt[ option].constraint.string_list;

		break;
	}
	case OPT_MODE:
		s->val[ option].w = optval - s->opt[ option].constraint.string_list;

		if( mode_params[ optval - mode_list].depth != 1) {
			s->opt[ OPT_HALFTONE].cap |= SANE_CAP_INACTIVE;
		} else {
			s->opt[ OPT_HALFTONE].cap &= ~SANE_CAP_INACTIVE;
		}

		if( mode_params[ optval - mode_list].color)
			s->opt[ OPT_DROPOUT].cap |= SANE_CAP_INACTIVE;
		else
			s->opt[OPT_DROPOUT].cap &= ~SANE_CAP_INACTIVE;

		if( s->hw->cmd->control_auto_area_segmentation) {
			if(  halftone_params[ s->val[ OPT_HALFTONE].w] == 0x03		/* THT */
			  && mode_params[ s->val[ OPT_MODE].w].depth == 1
			  )
				s->opt[ OPT_AAS].cap |= SANE_CAP_INACTIVE;
			else
				s->opt[ OPT_AAS].cap &= ~SANE_CAP_INACTIVE;
		}

		if( NULL != info)
			*info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;

		break;
	case OPT_HALFTONE:
		s->val[ option].w = optval - s->opt[ option].constraint.string_list;

		if( s->hw->cmd->control_auto_area_segmentation) {
			if(  halftone_params[ s->val[ OPT_HALFTONE].w] == 0x03		/* THT */
			  && mode_params[ s->val[ OPT_MODE].w].depth == 1
			  )
				s->opt[ OPT_AAS].cap |= SANE_CAP_INACTIVE;
			else
				s->opt[ OPT_AAS].cap &= ~SANE_CAP_INACTIVE;
		}

		if( NULL != info)
			*info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;

		break;

		case OPT_DROPOUT:
		case OPT_FILM_TYPE:
		case OPT_COLOR_CORRECTION:
		case OPT_BAY:
			s->val[ option].w = optval - s->opt[ option].constraint.string_list;
			break;

		case OPT_GAMMA_CORRECTION:
			s->val[ option].w = optval - s->opt[ option].constraint.string_list;

/*
 * If "User defined", then enable custom gamma tables.
*/

			if (s->val[ option].w != 1) {
				s->opt[ OPT_GAMMA_VECTOR].cap |= SANE_CAP_INACTIVE;
				s->opt[ OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
				s->opt[ OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
				s->opt[ OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;
			}
			else {
				s->opt[ OPT_GAMMA_VECTOR].cap &= ~SANE_CAP_INACTIVE;
				s->opt[ OPT_GAMMA_VECTOR_R].cap &= ~SANE_CAP_INACTIVE;
				s->opt[ OPT_GAMMA_VECTOR_G].cap &= ~SANE_CAP_INACTIVE;
				s->opt[ OPT_GAMMA_VECTOR_B].cap &= ~SANE_CAP_INACTIVE;
			}
			if( NULL != info)
				*info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;
			break;

		case OPT_MIRROR:
		case OPT_SPEED:
		case OPT_PREVIEW_SPEED:
		case OPT_AAS:
		case OPT_PREVIEW:							/* needed? */
		case OPT_BRIGHTNESS:
		case OPT_SHARPNESS:
		case OPT_AUTO_EJECT:
			s->val[ option].w = *( ( SANE_Word *) value);
			break;

	case OPT_QUICK_FORMAT:
		s->val[ option].w = optval - s->opt[ option].constraint.string_list;

		s->val[ OPT_TL_X].w = qf_params[ s->val[ option].w].tl_x;
		s->val[ OPT_TL_Y].w = qf_params[ s->val[ option].w].tl_y;
		s->val[ OPT_BR_X].w = qf_params[ s->val[ option].w].br_x;
		s->val[ OPT_BR_Y].w = qf_params[ s->val[ option].w].br_y;

		if( s->val[ OPT_TL_X].w < s->hw->x_range->min)
			s->val[ OPT_TL_X].w = s->hw->x_range->min;

		if( s->val[ OPT_TL_Y].w < s->hw->y_range->min)
			s->val[ OPT_TL_Y].w = s->hw->y_range->min;

		if( s->val[ OPT_BR_X].w > s->hw->x_range->max)
			s->val[ OPT_BR_X].w = s->hw->x_range->max;

		if( s->val[ OPT_BR_Y].w > s->hw->y_range->max)
			s->val[ OPT_BR_Y].w = s->hw->y_range->max;

		if( NULL != info)
			*info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;

		break;
	default:
		return SANE_STATUS_INVAL;
	}
      break;
    default:
      return SANE_STATUS_INVAL;
    }

  return SANE_STATUS_GOOD;
}

/*
//
//
*/

SANE_Status sane_get_parameters ( SANE_Handle handle, SANE_Parameters * params) {
	Epson_Scanner * s = ( Epson_Scanner *) handle;
	int ndpi;

	memset( &s->params, 0, sizeof( SANE_Parameters));

	ndpi = s->val[ OPT_RESOLUTION].w;

	s->params.pixels_per_line = SANE_UNFIX( s->val[ OPT_BR_X].w - s->val[ OPT_TL_X].w) / 25.4 * ndpi;
	s->params.lines = SANE_UNFIX( s->val[ OPT_BR_Y].w - s->val[ OPT_TL_Y].w) / 25.4 * ndpi;

		DBG( 3, "Preview = %d\n", s->val[OPT_PREVIEW].w);
		DBG( 3, "Resolution = %d\n", s->val[OPT_RESOLUTION].w);

		DBG( 1, "get para %p %p tlx %f tly %f brx %f bry %f [mm]\n"
			, ( void * ) s
			, ( void * ) s->val
			, SANE_UNFIX( s->val[ OPT_TL_X].w)
			, SANE_UNFIX( s->val[ OPT_TL_Y].w)
			, SANE_UNFIX( s->val[ OPT_BR_X].w)
			, SANE_UNFIX( s->val[ OPT_BR_Y].w)
			);

	/* pixels_per_line seems to be 8 * n.  */
	s->params.pixels_per_line = s->params.pixels_per_line & ~7;

	s->params.last_frame = SANE_TRUE;
	s->params.depth = mode_params[ s->val[ OPT_MODE].w].depth;

	if( mode_params[ s->val[ OPT_MODE].w].color) {
		s->params.format = SANE_FRAME_RGB;
		s->params.bytes_per_line = 3 * s->params.pixels_per_line;
	} else {
		s->params.format = SANE_FRAME_GRAY;
		s->params.bytes_per_line = s->params.pixels_per_line * s->params.depth / 8;
	}

	if( NULL != params)
		*params = s->params;

	return SANE_STATUS_GOOD;
}

/*
//
//
*/

SANE_Status sane_start ( SANE_Handle handle) {
	Epson_Scanner * s = ( Epson_Scanner *) handle;
	SANE_Status status;
	const struct mode_param * mparam;
	int ndpi;
	int left, top;
	int lcount;

	DBG( 1, "preview %d\n", s->val[ OPT_PREVIEW].w);

#if 0
	status = sane_get_parameters( handle, NULL);
	if( status != SANE_STATUS_GOOD)
		return status;
#endif

	open_scanner( s);
/*
// NOTE: added cause there was error reported for some scanner.
// NOTE: disabled cause of batch use scanimage and ADF.
//	reset( s);
*/

/*
//
*/

/*
//  There is some undocumented special with TPU enable/disable.
//      TPU power	ESC e		status
//	on		0		NAK
//	on		1		ACK
//	off		0		ACK
//	off		1		NAK
//
// probably it make no sense to scan with TPU powered on and source flatbed, cause light
// will come from both sides.
*/


#if 0
	if( s->hw->TPU) {

/*
// NOTE: should remove source TPU, and switch automatically.
// NOTE: not shure if that is a good idea.
*/




	} else
#endif

/*
//
*/

	if( s->hw->extension) {
		unsigned char * buf;
		EpsonHdr head;

		unsigned char params [ 1];

		DBG( 1, "use extension = %d\n", s->hw->use_extension);

		if( NULL == ( head = ( EpsonHdr) command( s, "\033e", 2, &status) ) ) {
			DBG( 0, "control of an extension failed\n");
			return status;
		}

		params[ 0] = s->hw->use_extension;	/* 1: effective, 0: ineffective */
		send( s, params, 1, &status); /* to make (in)effective an extension unit*/
		status = expect_ack ( s);

		if( SANE_STATUS_GOOD != status) {
			DBG( 0, "Probably you have to power %s your TPU\n"
				, s->hw->use_extension ? "on" : "off");

			DBG( 0, "Also you have to restart sane, cause it gives a ....\n");
			DBG( 0, "about the return code I'm sending.\n");

			return status;
		}

		if( NULL == ( head = ( EpsonHdr) command( s, "\033f", 2, &status) ) ) {
			DBG( 0, "Extended status flag request failed\n");
			return status;
		}

		buf = &head->buf[ 0];

		if( buf[ 0] & EXT_STATUS_FER) {
			DBG( 0, "option: fatal error\n");
			status = SANE_STATUS_INVAL;
		}

		if( buf[ 1] & EXT_STATUS_ERR) {
			DBG( 0, "ADF: other error\n");
			status = SANE_STATUS_INVAL;
		}

		if( buf[ 1] & EXT_STATUS_PE) {
			DBG( 0, "ADF: no paper\n");
			status = SANE_STATUS_INVAL;
		}

		if( buf[ 1] & EXT_STATUS_PJ) {
			DBG( 0, "ADF: paper jam\n");
			status = SANE_STATUS_INVAL;
		}

		if( buf[ 1] & EXT_STATUS_OPN) {
			DBG( 0, "ADF: cover open\n");
			status = SANE_STATUS_INVAL;
		}

		if( buf[ 6] & EXT_STATUS_ERR) {
			DBG( 0, "TPU: other error\n");
			status = SANE_STATUS_INVAL;
		}

		if( SANE_STATUS_GOOD != status) {
			close_scanner( s);
			return status;
		}
	}

/*
//
*/

	mparam = mode_params + s->val[ OPT_MODE].w;
/*	status = set_data_format( s, s->params.depth); */
	status = set_data_format( s, mparam->depth);

	if( SANE_STATUS_GOOD != status) {
		DBG( 1, "sane_start: set_data_format failed: %s\n", sane_strstatus( status));
		return status;
	}

	if( s->hw->level >= 5 && mparam->mode_flags == 0x02)
		status = set_color_mode( s, 0x13);
	else
		status = set_color_mode( s
			, mparam->mode_flags
			  | ( mparam->dropout_mask
			    & dropout_params[ s->val[ OPT_DROPOUT].w]
			    )
			);

	if( SANE_STATUS_GOOD != status) {
		DBG( 1, "sane_start: set_color_mode failed: %s\n", sane_strstatus( status));
		return status;
	}



	if( SANE_OPTION_IS_ACTIVE( s->opt[ OPT_HALFTONE].cap) ) {
		status = set_halftoning( s, halftone_params[ s->val[ OPT_HALFTONE].w]);

		if( SANE_STATUS_GOOD != status) {
			DBG( 1, "sane_start: set_halftoning failed: %s\n", sane_strstatus( status));
			return status;
		}
	}


	if( SANE_OPTION_IS_ACTIVE( s->opt[ OPT_BRIGHTNESS].cap) ) {
		status = set_bright( s, s->val[OPT_BRIGHTNESS].w);

		if( SANE_STATUS_GOOD != status) {
			DBG( 1, "sane_start: set_bright failed: %s\n", sane_strstatus( status));
			return status;
		}
	}

	if( SANE_OPTION_IS_ACTIVE( s->opt[ OPT_MIRROR].cap) ) {

		status = mirror_image( s, mirror_params[ s->val[ OPT_MIRROR].w]);

		if( SANE_STATUS_GOOD != status) {
			DBG( 1, "sane_start: mirror_image failed: %s\n", sane_strstatus( status));
			return status;
		}

	}

#if 1
	if( SANE_OPTION_IS_ACTIVE( s->opt[ OPT_SPEED].cap) ) {

		if( s->val[ OPT_PREVIEW].w)
			status = set_speed( s, speed_params[ s->val[ OPT_PREVIEW_SPEED].w]);
		else
			status = set_speed( s, speed_params[ s->val[ OPT_SPEED].w]);

		if( SANE_STATUS_GOOD != status) {
			DBG( 1, "sane_start: set_speed failed: %s\n", sane_strstatus( status));
			return status;
		}

	}
#else
	status = set_speed( s, mode_params[ s->val[ OPT_MODE]].depth == 1 ? 1 : 0);
#endif

/*
//  use of speed_params is ok here since they are false and true.
//  NOTE: I think I should throw that "params" stuff as long w is already the value.
*/

	if( SANE_OPTION_IS_ACTIVE( s->opt[ OPT_AAS].cap) ) {

		status = control_auto_area_segmentation( s, speed_params[ s->val[ OPT_AAS].w]);

		if( SANE_STATUS_GOOD != status) {
			DBG( 1, "sane_start: control_auto_area_segmentation failed: %s\n", sane_strstatus( status));
			return status;
		}
	}

	if( SANE_OPTION_IS_ACTIVE( s->opt[ OPT_FILM_TYPE].cap) ) {

		status = set_film_type( s, film_params[ s->val[ OPT_FILM_TYPE].w]);

		if( SANE_STATUS_GOOD != status) {
			DBG( 1, "sane_start: set_film_type failed: %s\n", sane_strstatus( status));
			return status;
		}
	}

	if( SANE_OPTION_IS_ACTIVE( s->opt[ OPT_BAY].cap) ) {

		status = set_bay( s, s->val[ OPT_BAY].w);

		if( SANE_STATUS_GOOD != status) {
			DBG( 1, "sane_start: set_bay: %s\n", sane_strstatus( status));
			return status;
		}
	}

	if( SANE_OPTION_IS_ACTIVE( s->opt[ OPT_SHARPNESS].cap) ) {

		status = set_outline_emphasis( s, s->val[ OPT_SHARPNESS].w);

		if( SANE_STATUS_GOOD != status) {
			DBG( 1, "sane_start: set_outline_emphasis failed: %s\n", sane_strstatus( status));
			return status;
		}
	}

	if( SANE_OPTION_IS_ACTIVE( s->opt[ OPT_GAMMA_CORRECTION].cap) ) {
		int val = gamma_params[ s->val[ OPT_GAMMA_CORRECTION].w];

		/*
		 * If "Default" is selected then determine the actual value
		 * to send to the scanner: If bilevel mode, just send the 
		 * value from the table (0x01), for grayscale or color mode 
		 * add one and send 0x02.
		 */
		if( s->val[ OPT_GAMMA_CORRECTION].w <= 1) {
			val += mparam->depth == 1 ? 0 : 1;
		}

		status = set_gamma( s, val);

		if( SANE_STATUS_GOOD != status) {
			DBG( 1, "sane_start: set_gamma failed: %s\n", sane_strstatus( status));
			return status;
		}
	}

	if( 2 == s->val[ OPT_GAMMA_CORRECTION].w) {	/* user defined. */
		status = set_gamma_table( s);

		if( SANE_STATUS_GOOD != status) {
			DBG( 1, "sane_start: set_gamma_table failed: %s\n", sane_strstatus (status));
			return status;
		}
	}

#if 0
	status = set_color_correction( s, 0x80);

	if( SANE_STATUS_GOOD != status) {
		DBG( 1, "sane_start: set_color_correction failed: %s\n", sane_strstatus (status));
		return status;
	}
#else
/*
// TODO: think about if SANE_OPTION_IS_ACTIVE is a good criteria to send commands.
*/

	if( SANE_OPTION_IS_ACTIVE( s->opt[ OPT_COLOR_CORRECTION].cap) ) {
		int val = color_params[ s->val[ OPT_COLOR_CORRECTION].w];

		status = set_color_correction( s, val);

		if( SANE_STATUS_GOOD != status) {
			DBG( 1, "sane_start: set_color_correction failed: %s\n", sane_strstatus( status));
			return status;
		}
	}
#if 0
	if( 1 == s->val[ OPT_COLOR_CORRECTION].w) {	/* user defined. */
		status = set_color_correction_coefficients( s);

		if( SANE_STATUS_GOOD != status) {
			DBG( 1, "sane_start: set_color_correction_coefficients failed: %s\n", sane_strstatus( status));
			return status;
		}
	}
#endif
#endif

	ndpi = s->val[ OPT_RESOLUTION].w;

	status = set_resolution( s, ndpi, ndpi);

	if( SANE_STATUS_GOOD != status) {
		DBG( 1, "sane_start: set_resolution failed: %s\n", sane_strstatus( status));
		return status;
    	}

	status = sane_get_parameters( handle, NULL);

	if( status != SANE_STATUS_GOOD)
		return status;

/*
//  Now s->params is initialized.
*/

/*
//  in file:frontend/preview.c
//
//  The preview strategy is as follows:
//
//   1) A preview always acquires an image that covers the entire
//      scan surface.  This is necessary so the user can see not
//      only what is, but also what isn't selected.
*/

	left = SANE_UNFIX( s->val[OPT_TL_X].w) / 25.4 * ndpi + 0.5;
	top = SANE_UNFIX( s->val[OPT_TL_Y].w) / 25.4 * ndpi + 0.5;

	status = set_scan_area( s, left, top, s->params.pixels_per_line, s->params.lines);

	if( SANE_STATUS_GOOD != status) {
		DBG( 1, "sane_start: set_scan_area failed: %s\n", sane_strstatus( status));
		return status;
	}

	s->block = SANE_FALSE;
	lcount = 1;

	if( s->hw->level >= 5 || ( s->hw->level >= 4 && ! mode_params[s->val[ OPT_MODE].w].color)) {
		s->block = SANE_TRUE;
		lcount = sanei_scsi_max_request_size / s->params.bytes_per_line;

	if( lcount > 255)
		lcount = 255;

	if( lcount == 0)
		return SANE_STATUS_NO_MEM;

	status = set_lcount( s, lcount);

	if( SANE_STATUS_GOOD != status) {
		DBG( 1, "sane_start: set_lcount failed: %s\n", sane_strstatus (status));
		return status;
	}
    }

	if( SANE_TRUE == s->hw->extension) { /* make sure if any errors */

/* TODO	*/

		unsigned char result[ 4];		/* with an extension */
		unsigned char * buf;
		size_t len;

		send( s, "\033f", 2, &status);		/* send ESC f (request extension status) */

		if( SANE_STATUS_GOOD != status)
			return status;

		len = 4;
		receive( s, result, len, &status);
		len = result[ 3] << 8 | result[ 2];
		buf = alloca( len);
		receive( s, buf, len, &status);

		if( buf[ 0] & 0x80) {
			close_scanner( s);
			return SANE_STATUS_INVAL;
		}
	}

/*
//  for debug purpose
//  check scanner conditions
*/
#if 1
{
	unsigned char result [ 4];
	unsigned char * buf;
	size_t len;
/*     int i; */

	send( s, "\033S", 2, &status);		/* send ESC S (request scanner setting status) */

    if( SANE_STATUS_GOOD != status)
      return status;

    len = 4;
    receive( s, result, len, &status);

    if( SANE_STATUS_GOOD != status)
      return status;

    len = result[ 3] << 8 | result[ 2];
    buf = alloca( len);
    receive( s, buf, len, &status);

    if( SANE_STATUS_GOOD != status)
      return status;

/*     DBG(10, "SANE_START: length=%d\n", len); */
/*     for (i = 1; i <= len; i++) { */
/*       DBG(10, "SANE_START: %d: %c\n", i, buf[i-1]); */
/*     } */
    DBG( 5, "SANE_START: color: %d\n", (int) buf[1]);
    DBG( 5, "SANE_START: resolution (x, y): (%d, %d)\n", 
        (int) (buf[4]<<8|buf[3]), (int) (buf[6]<<8|buf[5]));
    DBG( 5, "SANE_START: area[dots] (x-offset, y-offset), (x-range, y-range): (%d, %d), (%d, %d)\n",
	(int) (buf[9]<<8|buf[8]), (int) (buf[11]<<8|buf[10]), 
        (int) (buf[13]<<8|buf[12]), (int) (buf[15]<<8|buf[14]));
    DBG( 5, "SANE_START: data format: %d\n", (int) buf[17]);
    DBG( 5, "SANE_START: halftone: %d\n", (int) buf[19]);
    DBG( 5, "SANE_START: brightness: %d\n", (int) buf[21]);
    DBG( 5, "SANE_START: gamma: %d\n", (int) buf[23]);
    DBG( 5, "SANE_START: zoom[percentage] (x, y): (%d, %d)\n", (int) buf[26], (int) buf[25]);
    DBG( 5, "SANE_START: color correction: %d\n", (int) buf[28]);
    DBG( 5, "SANE_START: outline emphasis: %d\n", (int) buf[30]);
    DBG( 5, "SANE_START: read mode: %d\n", (int) buf[32]);
    DBG( 5, "SANE_START: mirror image: %d\n", (int) buf[34]);
    DBG( 5, "SANE_START: (new B6 or B7 command ESC s): %d\n", (int) buf[36]);
    DBG( 5, "SANE_START: (new B6 or B7 command ESC t): %d\n", (int) buf[38]);
    DBG( 5, "SANE_START: line counter: %d\n", (int) buf[40]);
    DBG( 5, "SANE_START: extension control: %d\n", (int) buf[42]);
    DBG( 5, "SANE_START: (new B6 or B7 command ESC N): %d\n", (int) buf[44]);
}
#endif
/*
//  for debug purpose
//  check scanner conditions
*/

	send( s, "\033G", 2, &status);

	if( SANE_STATUS_GOOD != status) {
		DBG( 1, "sane_start: start failed: %s\n", sane_strstatus( status));
		return status;
    	}

	s->eof = SANE_FALSE;
	s->buf = realloc( s->buf, lcount * s->params.bytes_per_line);
	s->ptr = s->end = s->buf;
	s->canceling = SANE_FALSE;

	return SANE_STATUS_GOOD;
} /* sane_start */

/*
//
// TODO: clean up the eject and direct cmd mess.
*/

SANE_Status sane_auto_eject ( Epson_Scanner * s) {

	if( s->hw->ADF && s->hw->use_extension && s->val[ OPT_AUTO_EJECT].w) {		/* sequence! */
		SANE_Status status;

		unsigned char params [ 1];
		unsigned char cmd = s->hw->cmd->eject;

		if( ! cmd)
			return SANE_STATUS_UNSUPPORTED;

		params[ 0] = cmd;

		send( s, params, 1, &status);

		if( SANE_STATUS_GOOD != ( status = expect_ack( s) ) ) {
			return status;
		}
	}

	return SANE_STATUS_GOOD;
}

/*
//
//
*/

static SANE_Status read_data_block ( Epson_Scanner * s, EpsonDataRec * result) {
	SANE_Status status;

	receive( s, result, s->block ? 6 : 4, &status);

	if( SANE_STATUS_GOOD != status)
		return status;

	if( STX != result->code) {
		DBG( 1, "code   %02x\n", ( int) result->code);
		DBG( 1, "error, expected STX\n");

		return SANE_STATUS_INVAL;
	}

	if( result->status & STATUS_FER) {

		DBG( 1, "fatal error\n");

		/* check extended status if the option bit in status is set */
		if( result->status & STATUS_OPTION) {
			status = check_ext_status( s);
		} else
			status = SANE_STATUS_INVAL;
	}

	return status;
}

/*
//
//
*/

SANE_Status sane_read ( SANE_Handle handle, SANE_Byte * data, SANE_Int max_length, SANE_Int * length) {
	Epson_Scanner * s = ( Epson_Scanner *) handle;
	SANE_Status status;

	DBG( 5, "sane_read: begin\n");

	if( s->ptr == s->end) {
		EpsonDataRec result;
		size_t buf_len;

		if( s->eof) {
			free( s->buf);
			s->buf = NULL;
			sane_auto_eject( s);
			close_scanner( s);
			s->fd = -1;
			*length = 0;

			return SANE_STATUS_EOF;
		}

		DBG( 5, "sane_read: begin scan1\n");

		if( SANE_STATUS_GOOD != ( status = read_data_block( s, &result)))
			return status;

		buf_len = result.buf[ 1] << 8 | result.buf[ 0];

		DBG( 5, "sane_read: buf len = %lu\n", (u_long) buf_len);

		if( s->block)
			buf_len *= ( result.buf[ 3] << 8 | result.buf[ 2]);

		DBG( 5, "sane_read: buf len = %lu\n", (u_long) buf_len);

		if( !s->block && SANE_FRAME_RGB == s->params.format) {

			if( SANE_STATUS_GOOD != ( status = read_data_block( s, &result)))
				return status;

			send( s, S_ACK, 1, &status);
			if( SANE_STATUS_GOOD != ( status = read_data_block( s, &result)))
				return status;

			buf_len = result.buf[ 1] << 8 | result.buf[ 0];

			if( s->block)
				buf_len *= ( result.buf[ 3] << 8 | result.buf[ 2]);

			DBG( 5, "sane_read: buf len2 = %lu\n", (u_long) buf_len);

			receive( s, s->buf, buf_len, &status);

			if( SANE_STATUS_GOOD != status)
				return status;

			send( s, S_ACK, 1, &status);
			if( SANE_STATUS_GOOD != ( status = read_data_block( s, &result)))
				return status;

			buf_len = result.buf[ 1] << 8 | result.buf[ 0];

			if( s->block)
				buf_len *= ( result.buf[ 3] << 8 | result.buf[ 2]);

			DBG( 5, "sane_read: buf len3 = %lu\n", (u_long) buf_len);

			receive( s, s->buf + 2 * s->params.pixels_per_line, buf_len, &status);

			if( SANE_STATUS_GOOD != status)
				return status;
		} else {
			receive( s, s->buf, buf_len, &status);

			if( SANE_STATUS_GOOD != status)
				return status;
		}

		if( result.status & STATUS_AREA_END)
			s->eof = SANE_TRUE;
		else {
			if( s->canceling) {
				send( s, S_CAN, 1, &status);
				expect_ack( s);
				free( s->buf);
				s->buf = NULL;
				sane_auto_eject( s);
				close_scanner( s);
				s->fd = -1;
				*length = 0;
				return SANE_STATUS_CANCELLED;
			} else
				send( s, S_ACK, 1, &status);
		}

		s->end = s->buf + buf_len;
		s->ptr = s->buf;

		DBG( 5, "sane_read: begin scan2\n");
	}


	if( ! s->block && SANE_FRAME_RGB == s->params.format) {

		max_length /= 3;

		if( max_length > s->end - s->ptr)
			max_length = s->end - s->ptr;

		*length = 3 * max_length;

		while( max_length-- != 0) {
			*data++ = s->ptr[ 0];
			*data++ = s->ptr[ s->params.pixels_per_line];
			*data++ = s->ptr[ 2 * s->params.pixels_per_line];
			++s->ptr;
		}
	} else {

		if( max_length > s->end - s->ptr)
			max_length = s->end - s->ptr;

		*length = max_length;

		if( 1 == s->params.depth) {
			while( max_length-- != 0)
				*data++ = ~*s->ptr++;
		} else {
			memcpy( data, s->ptr, max_length);
			s->ptr += max_length;
		}
	}

	DBG( 5, "sane_read: end\n");

	return SANE_STATUS_GOOD;
}

/*
//
//
*/

void sane_cancel ( SANE_Handle handle) {
	Epson_Scanner * s = ( Epson_Scanner *) handle;

	if( s->buf != NULL)
		s->canceling = SANE_TRUE;
}

/*
//
//
*/

SANE_Status sane_set_io_mode ( SANE_Handle handle, SANE_Bool non_blocking) {
	return SANE_STATUS_UNSUPPORTED;
}

/*
//
//
*/

SANE_Status sane_get_select_fd ( SANE_Handle handle, SANE_Int * fd) {
	return SANE_STATUS_UNSUPPORTED;
}
