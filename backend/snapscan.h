/* sane - Scanner Access Now Easy.

   Copyright (C) 1997, 1998 Franck Schnefra, Michel Roelofs,
   Emmanuel Blot, Mikko Tyolajarvi, David Mosberger-Tang, Wolfgang Goeller
   and Kevin Charter

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

   This file implements a backend for the AGFA SnapScan flatbed
   scanner. */

/* $Id$
   SANE SnapScan backend */

#ifndef snapscan_h
#define snapscan_h

/* snapscan device field values */

#define DEFAULT_DEVICE "/dev/scanner" /* Check this if config is missing */
#define SNAPSCAN_TYPE      "flatbed scanner"
/*#define	INOPERATIVE*/
#define TMP_FILE_PREFIX "/var/tmp/snapscan"

typedef enum
{
  UNKNOWN,
  SNAPSCAN300,			/* the original SnapScan or SnapScan 300 */
  SNAPSCAN310,			/* the SnapScan 310 */
  SNAPSCAN600,			/* the SnapScan 600 */
  SNAPSCAN1236S,		/* the SnapScan 1236s */
  VUEGO310S			/* Vuego-Version of SnapScan 310 WG changed */
} SnapScan_Model;

struct SnapScan_Model_desc
{
  char *scsi_name;
  SnapScan_Model id;
};

static struct SnapScan_Model_desc scanners[] =
{
  /* SCSI model name -> enum value */
  { "FlatbedScanner_4",	VUEGO310S },
  { "SNAPSCAN 1236",	SNAPSCAN1236S },
  { "SNAPSCAN 310",	SNAPSCAN310 },
  { "SNAPSCAN 600",	SNAPSCAN600 },
  { "SnapScan",		SNAPSCAN300 },
};
#define known_scanners (sizeof(scanners)/sizeof(scanners[0]))

static char *vendors[] =
{
  /* SCSI Vendor name */
  "AGFA",
  "COLOR",
};
#define known_vendors (sizeof(vendors)/sizeof(vendors[0]))

typedef enum
  {
    OPT_COUNT = 0,		/* option count */
    OPT_SCANRES,		/* scan resolution */
    OPT_PREVIEW,		/* preview mode toggle */
    OPT_MODE,			/* scan mode */
    OPT_PREVIEW_MODE,		/* preview mode */
    OPT_TLX,			/* top left x */
    OPT_TLY,			/* top left y */
    OPT_BRX,			/* bottom right x */
    OPT_BRY,			/* bottom right y */
    OPT_PREDEF_WINDOW,		/* predefined window configuration */
    OPT_HALFTONE,		/* halftone flag */
    OPT_HALFTONE_PATTERN,	/* halftone matrix */
    OPT_GAMMA_GS,		/* gamma correction (greyscale) */
    OPT_GAMMA_R,		/* gamma correction (red) */
    OPT_GAMMA_G,		/* gamma correction (green) */
    OPT_GAMMA_B,		/* gamma correction (blue) */
    OPT_NEGATIVE,		/* swap black and white */
    OPT_THRESHOLD,		/* threshold for line art */
#ifdef INOPERATIVE
    OPT_BRIGHTNESS,		/* brightness */
    OPT_CONTRAST,		/* contrast */
#endif
    OPT_RGB_LPR,		/* lines per scsi read (RGB) */
    OPT_GS_LPR,			/* lines per scsi read (GS) */
    OPT_SCSI_CMDS,		/* a group */
    OPT_INQUIRY,		/* inquiry command (button) */
    OPT_SELF_TEST,		/* self test command (button) */
    OPT_REQ_SENSE,		/* request sense command (button) */
    OPT_REL_UNIT,		/* release unit command (button) */
    NUM_OPTS			/* dummy (gives number of options) */
  } SnapScan_Options;

typedef enum
{
  MD_COLOUR = 0,		/* full colour */
  MD_BILEVELCOLOUR,		/* 1-bit per channel colour */
  MD_GREYSCALE,			/* grey scale */
  MD_LINEART,			/* black and white */
  MD_NUM_MODES
} SnapScan_Mode;

typedef enum
  {
    ST_IDLE,			/* between scans */
    ST_SCAN_INIT,		/* scan initialization */
    ST_SCANNING,		/* actively scanning data */
    ST_CANCEL_INIT		/* cancellation begun */
  } SnapScan_State;

typedef struct snapscan_device
  {
    SANE_Device dev;
    SnapScan_Model model;	/* type of AGFA scanner */
    u_char *depths;		/* bit depth table */
    struct snapscan_device *pnext;
  } SnapScan_Device;

#define MAX_SCSI_CMD_LEN 256	/* not that large */
#define SCANNER_BUF_SZ 31744

typedef struct snapscan_scanner
  {
    SANE_String devname;	/* the scsi device name */
    SnapScan_Device *pdev;	/* the device */
    int fd;			/* scsi file descriptor */
    int opens;			/* open count */
    SANE_String tmpfname;	/* temporary file name */
    int tfd;			/* temp file descriptor */
    int rpipe[2];		/* reader pipe descriptors */
    int orig_rpipe_flags;	/* initial reader pipe flags */
    pid_t child;		/* child reader process pid */
    SnapScan_Mode mode;		/* mode */
    SnapScan_Mode preview_mode;	/* preview mode */
    SnapScan_State state;	/* scanner state */
    u_char cmd[MAX_SCSI_CMD_LEN];	/* scsi command buffer */
    u_char buf[SCANNER_BUF_SZ];	/* data buffer */
    size_t buf_sz;		/* effective buffer size */
    size_t expected_read_bytes;	/* expected amount of data in a single read */
    size_t read_bytes;		/* amount of actual data read */
    size_t expected_data_len;	/* total amount of expected data in scan */
    size_t actual_res;		/* actual resolution */
    size_t lines;		/* number of scan lines */
    size_t bytes_per_line;	/* bytes per scan line */
    size_t pixels_per_line;	/* pixels per scan line */
    u_char hconfig;		/* hardware configuration byte */
    float ms_per_line;		/* speed: milliseconds per scan line */
    SANE_Bool nonblocking;	/* wait on reads for data? */
    char *sense_str;		/* sense string */
    char *as_str;		/* additional sense string */
    u_char asi1;		/* first additional sense info byte */
    u_char asi2;		/* second additional sense info byte */

      SANE_Option_Descriptor
      options[NUM_OPTS];	/* the option descriptors */
    /* the options themselves... */
    SANE_Int res;		/* resolution */
    SANE_Bool preview;		/* preview mode toggle */
    SANE_String mode_s;		/* scanning mode */
    SANE_String preview_mode_s;	/* scanning mode for preview */
    SANE_Fixed tlx;		/* window top left x */
    SANE_Fixed tly;		/* window top left y */
    SANE_Fixed brx;		/* window bottom right x */
    SANE_Fixed bry;		/* window bottom right y */
#ifdef INOPERATIVE
    int bright;			/* brightness */
    int contrast;		/* contrast */
#endif
    SANE_String predef_window;	/* predefined window name */
    SANE_Fixed gamma_gs;	/* gamma correction value (greyscale) */
    SANE_Fixed gamma_r;		/* gamma correction value (red) */
    SANE_Fixed gamma_g;		/* gamma correction value (green) */
    SANE_Fixed gamma_b;		/* gamma correction value (blue) */
    SANE_Bool halftone;		/* halftone toggle */
    SANE_String dither_matrix;	/* the halftone dither matrix */
    SANE_Bool negative;		/* swap black and white */
    SANE_Int threshold;		/* threshold for line art */
    SANE_Int rgb_lpr;		/* lines per scsi read (RGB) */
    SANE_Int gs_lpr;		/* lines per scsi read (greyscale) */
    struct
      {				/* RGB ring buffer for 310/600 model */
	SANE_Byte *data;	/* buffer data */
	SANE_Int line_in;	/* virtual position */
	SANE_Int pixel_pos;
	SANE_Int line_out;	/* read lines */
	SANE_Byte g_offset;	/* green offset */
	SANE_Byte b_offset;	/* blue offset */
	SANE_Byte r_offset;	/* red offset */
      } rgb_buf;
  }

SnapScan_Scanner;


#endif

/* $Log$
 * Revision 1.1  1999/08/09 18:05:53  pere
 * Initial revision
 *
 * Revision 1.25  1998/12/16  18:40:53  charter
 * Commented the INOPERATIVE define to get rid of spurious brightness
 * and contrast controls accidentally reintroduced previously.
 *
 * Revision 1.24  1998/09/07  06:04:58  charter
 * Merged in Wolfgang Goeller's changes (Vuego 310S, bugfixes).
 *
 * Revision 1.23  1998/05/11  17:03:22  charter
 * Added Mikko's threshold stuff
 *
 * Revision 1.22  1998/03/10 23:43:05  eblot
 * Changing 310/600 models support (structure)
 *
 * Revision 1.21  1998/03/08 14:24:43  eblot
 * Debugging
 *
 * Revision 1.20  1998/02/15  21:55:03  charter
 * From Emmanuel Blot:
 * Added rgb ring buffer to handle snapscan 310 data specs.
 *
 * Revision 1.19  1998/02/06  02:29:52  charter
 * Added SnapScan_Mode and SnapScan_Model enums.
 *
 * Revision 1.18  1998/01/31  23:59:51  charter
 * Changed window coordinates type to SANE_Fixed (what it should be
 * for a length).
 *
 * Revision 1.17  1998/01/30  19:18:41  charter
 * Added sense_str and as_str to SnapScan_Scanner; these are intended to
 * be set by the sense handler.
 *
 * Revision 1.16  1998/01/30  11:02:17  charter
 * Added opens to the SnapScan_Scanner to support open_scanner() and
 * close_scanner().
 *
 * Revision 1.15  1998/01/25  09:57:32  charter
 * Added more SCSI command options and a group for them.
 *
 * Revision 1.14  1998/01/25  08:50:49  charter
 * Added preview mode option.
 *
 * Revision 1.13  1998/01/25  02:24:31  charter
 * Added OPT_NEGATIVE and the extra sense data bytes.
 *
 * Revision 1.12  1998/01/24  05:14:56  charter
 * Added stuff for RGB gamma correction and for BW mode halftoning.
 *
 * Revision 1.11  1998/01/23  13:02:45  charter
 * Added rgb_lpr and gs_lpr so the user can tune scanning performance.
 *
 * Revision 1.10  1998/01/23  07:39:08  charter
 * Reindented using GNU convention at David Mosberger-Tang's request.
 * Added ms_per_line to SnapScan_Scanner.
 *
 * Revision 1.9  1998/01/22  05:14:23  charter
 * The bit depth option has been replaced with a mode option. We support
 * full color, greyscale and lineart modes.
 *
 * Revision 1.8  1998/01/21  20:40:13  charter
 * Added copyright info; added the new SnapScan_State type and
 * replaced the scanning member of SnapScan_Scanner with a state
 * member. This is for supporting cancellation.
 *
 * Revision 1.7  1998/01/21  11:05:20  charter
 * Inoperative options now #defined out.
 *
 * Revision 1.6  1997/11/26  15:40:24  charter
 * Brightness and contrast added by Michel.
 *
 * Revision 1.5  1997/11/12  12:52:16  charter
 * Added OPT_INQUIRY for the inquiry button.
 *
 * Revision 1.4  1997/11/10  05:51:45  charter
 * Added stuff for the child reader process and pipe.
 *
 * Revision 1.3  1997/11/03  03:16:46  charter
 * Added buffers and window parameter variables to the scanner structure.
 *
 * Revision 1.2  1997/10/14  05:59:53  charter
 * Basic options and structures added.
 *
 * Revision 1.1  1997/10/13  02:25:54  charter
 * Initial revision
 * */
