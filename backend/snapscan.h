/* sane - Scanner Access Now Easy.
 
   Copyright (C) 1997, 1998, 1999, 2001  Franck Schnefra, Michel Roelofs,
   Emmanuel Blot, Mikko Tyolajarvi, David Mosberger-Tang, Wolfgang Goeller,
   Petter Reinholdtsen, Gary Plewa, Sebastien Sable, Mikael Magnusson,
   Oliver Schwartz and Kevin Charter

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

   This file is a component of the implementation of a backend for many
   of the the AGFA SnapScan and Acer Vuego/Prisa flatbed scanners. */

/* $Id$
   SANE SnapScan backend */

#ifndef snapscan_h
#define snapscan_h

#define UNREFERENCED_PARAMETER(x)           ((void) x)

/* snapscan device field values */

#define DEFAULT_DEVICE "/dev/scanner" /* Check this if config is missing */
#define SNAPSCAN_TYPE      "flatbed scanner"
#define TMP_FILE_PREFIX "/var/tmp/snapscan"
#define SNAPSCAN_CONFIG_FILE "snapscan.conf"
#define FIRMWARE_KW "firmware"
#define OPTIONS_KW "options"

/* Define the colour channel order in arrays */
#define R_CHAN    0
#define G_CHAN    1
#define B_CHAN    2

typedef enum
{
  UNKNOWN_BUS,
  SCSI,
  USB
} SnapScan_Bus;

typedef enum
{
    UNKNOWN,
    SNAPSCAN,           /* the original SnapScan */
    SNAPSCAN300,        /* the SnapScan 300 */
    SNAPSCAN310,        /* the SnapScan 310 */
    SNAPSCAN600,        /* the SnapScan 600 */
    SNAPSCAN1236,       /* the SnapScan 1236 */
    SNAPSCAN1212U,
    SNAPSCANE20,        /* SnapScan e20/e25, 600 DPI */
    SNAPSCANE50,        /* SnapScan e40/e50, 1200 DPI */
    SNAPSCANE52,        /* SnapScan e52, 1200 DPI, no quality calibration */
    ACER300F,
    VUEGO310S,          /* Vuego-Version of SnapScan 310 WG changed */
    VUEGO610S,          /* Vuego 610S and 610plus SJU changed */
    PRISA620S,          /* Acer ScanPrisa 620 - 600 DPI */
    PRISA640,           /* Acer ScanPrisa 640 - 600 DPI */
    PRISA1240,          /* Acer ScanPrisa 1240 - 1200 DPI */
    PRISA4300,          /* Acer ScanPrisa 3300/4300 - 600 DPI */
    PRISA4300_2,        /* Acer ScanPrisa 3300/4300 - 600 DPI, 42 bit*/
    PRISA5000,          /* Acer ScanPrisa 5000 - 1200 DPI */
    PRISA5300,          /* Acer ScanPrisa 5300 - 1200 DPI */
    PERFECTION660,      /* Epson Perfection 660 - 1200 DPI */
    ARCUS1200		/* Agfa Arcus 1200 - 1200 DPI (rebadged Acer?) */
} SnapScan_Model;

struct SnapScan_Driver_desc {
    SnapScan_Model id;
    char *driver_name;
};

static struct SnapScan_Driver_desc drivers[] =
{
    /* enum value -> Driver name */
    {UNKNOWN,        "Unknown"},
    {SNAPSCAN,       "SnapScan"},
    {SNAPSCAN300,    "SnapScan300"},
    {SNAPSCAN310,    "SnapScan310"},
    {SNAPSCAN600,    "SnapScan600"},
    {SNAPSCAN1236,   "SnapScan1236"},
    {SNAPSCAN1212U,  "SnapScan1212"},
    {SNAPSCANE20,    "SnapScanE20"},
    {SNAPSCANE50,    "SnapScanE50"},
    {SNAPSCANE52,    "SnapScanE52"},
    {ACER300F,       "Acer300"},
    {VUEGO310S,      "Acer310"},
    {VUEGO610S,      "Acer610"},
    {PRISA620S,      "Acer620"},
    {PRISA640,       "Acer640"},
    {PRISA4300,      "Acer4300"},
    {PRISA4300_2,    "Acer4300 (42 bit)"},
    {PRISA1240,      "Acer1240"},
    {PRISA5000,      "Acer5000"},
    {PRISA5300,      "Acer5300"},
    {PERFECTION660,  "Perfection 660"},
    {ARCUS1200,      "Arcus1200"}
};

#define known_drivers ((int) (sizeof(drivers)/sizeof(drivers[0])))

struct SnapScan_Model_desc
{
    char *scsi_name;
    SnapScan_Model id;
};

static struct SnapScan_Model_desc scanners[] =
{
    /* SCSI model name -> enum value */
    {"FlatbedScanner_2",    VUEGO310S},
    {"FlatbedScanner_4",    VUEGO610S},
    {"FlatbedScanner_5",    PRISA620S},
    {"FlatbedScanner_9",    PRISA620S},
    {"FlatbedScanner13",    PRISA620S},
    {"FlatbedScanner16",    PRISA620S},
    {"FlatbedScanner17",    PRISA620S},
    {"FlatbedScanner18",    PRISA620S},
    {"FlatbedScanner19",    PRISA1240},
    {"FlatbedScanner20",    PRISA640},
    {"FlatbedScanner21",    PRISA4300},
    {"FlatbedScanner22",    PRISA4300_2},
    {"FlatbedScanner23",    PRISA4300_2},
    {"FlatbedScanner24",    PRISA5300},
    {"FlatbedScanner25",    PRISA5000},
    {"SNAPSCAN 1212U",      SNAPSCAN1212U},
    {"SNAPSCAN 1212U_2",    SNAPSCAN1212U},
    {"SNAPSCAN e10",        SNAPSCANE20},
    {"SNAPSCAN e20",        SNAPSCANE20},
    {"SNAPSCAN e25",        SNAPSCANE20},
    {"SNAPSCAN e26",        SNAPSCANE20},
    {"SNAPSCAN e40",        SNAPSCANE50},
    {"SNAPSCAN e42",        SNAPSCANE52},
    {"SNAPSCAN e50",        SNAPSCANE50},
    {"SNAPSCAN e52",        SNAPSCANE52},
    {"SNAPSCAN 1236",       SNAPSCAN1236},
    {"SNAPSCAN 1236U",      SNAPSCAN1236},
    {"SNAPSCAN 300",        SNAPSCAN300},
    {"SNAPSCAN 310",        SNAPSCAN310},
    {"SNAPSCAN 600",        SNAPSCAN600},
    {"SnapScan",            SNAPSCAN},
    {"ACERSCAN_A4____1",    ACER300F},
    {"Perfection 660",      PERFECTION660},
    {"ARCUS 1200",          ARCUS1200}
};
#define known_scanners ((int) (sizeof(scanners)/sizeof(scanners[0])))

static char *vendors[] =
{
    /* SCSI Vendor name */
    "AGFA",
    "COLOR",
    "Color",
    "ACERPER",
    "EPSON"
};
#define known_vendors ((int) (sizeof(vendors)/sizeof(vendors[0])))

static SANE_Word usb_vendor_ids[] =
{
    /* USB Vendor IDs */
    0x06bd,     /* Agfa */
    0x04a5,     /* Acer */
    0x04b8      /* Epson */
};
#define known_usb_vendor_ids ((int) (sizeof(usb_vendor_ids)/sizeof(usb_vendor_ids[0])))

struct SnapScan_USB_Model_desc
{
    SANE_Word vendor_id;
    SANE_Word product_id;
    SnapScan_Model id;
};

static struct SnapScan_USB_Model_desc usb_scanners[] =
{
    {0x04a5, 0x1a20, SNAPSCAN310},  /* Acer 310U */
    {0x04a5, 0x2022, SNAPSCAN310}   /* Acer 320U */
};
#define known_usb_scanners ((int) (sizeof(usb_scanners)/sizeof(usb_scanners[0])))

typedef enum
{
    OPT_COUNT = 0,         /* option count */
    OPT_MODE_GROUP,        /* scan mode group */
    OPT_SCANRES,           /* scan resolution */
    OPT_PREVIEW,           /* preview mode toggle */
    OPT_MODE,              /* scan mode */
    OPT_PREVIEW_MODE,      /* preview mode */
    OPT_SOURCE,            /* scan source (flatbed / TPO) */
    OPT_GEOMETRY_GROUP,    /* geometry group */
    OPT_TLX,               /* top left x */
    OPT_TLY,               /* top left y */
    OPT_BRX,               /* bottom right x */
    OPT_BRY,               /* bottom right y */
    OPT_PREDEF_WINDOW,     /* predefined window configuration */
    OPT_ENHANCEMENT_GROUP, /* enhancement group */
    OPT_QUALITY_CAL,       /* quality calibration */
    OPT_HALFTONE,          /* halftone flag */
    OPT_HALFTONE_PATTERN,  /* halftone matrix */
    OPT_CUSTOM_GAMMA,      /* use custom gamma tables */
    OPT_GAMMA_BIND,        /* use same gamma value for all colors */
    OPT_GAMMA_GS,          /* gamma correction (greyscale) */
    OPT_GAMMA_R,           /* gamma correction (red) */
    OPT_GAMMA_G,           /* gamma correction (green) */
    OPT_GAMMA_B,           /* gamma correction (blue) */
    OPT_GAMMA_VECTOR_GS,   /* gamma correction vector (greyscale) */
    OPT_GAMMA_VECTOR_R,    /* gamma correction vector (red) */
    OPT_GAMMA_VECTOR_G,    /* gamma correction vector (green) */
    OPT_GAMMA_VECTOR_B,    /* gamma correction vector (blue) */
    OPT_NEGATIVE,          /* swap black and white */
    OPT_THRESHOLD,         /* threshold for line art */
    OPT_BRIGHTNESS,        /* brightness */
    OPT_CONTRAST,          /* contrast */
    OPT_ADVANCED_GROUP,    /* advanced group */
    OPT_RGB_LPR,           /* lines per scsi read (RGB) */
    OPT_GS_LPR,            /* lines per scsi read (GS) */
    NUM_OPTS               /* dummy (gives number of options) */
} SnapScan_Options;

typedef union
  {
    SANE_Bool b;
    SANE_Word w;
    SANE_Word *wa;              /* word array */
    SANE_String s;
  }
Option_Value;

typedef enum
{
    MD_COLOUR = 0,       /* full colour */
    MD_BILEVELCOLOUR,    /* 1-bit per channel colour */
    MD_GREYSCALE,        /* grey scale */
    MD_LINEART,          /* black and white */
    MD_NUM_MODES
} SnapScan_Mode;

typedef enum
{
    SRC_FLATBED = 0,    /* Flatbed (normal) */
    SRC_TPO,            /* Transparency unit */
    SRC_ADF
} SnapScan_Source;

typedef enum
{
    ST_IDLE,            /* between scans */
    ST_SCAN_INIT,        /* scan initialization */
    ST_SCANNING,        /* actively scanning data */
    ST_CANCEL_INIT        /* cancellation begun */
} SnapScan_State;

typedef struct snapscan_device
{
    SANE_Device dev;
    SANE_Range x_range;           /* x dimension of scan area */
    SANE_Range y_range;           /* y dimension of scan area */
    SnapScan_Model model;         /* type of scanner */
    SnapScan_Bus bus;             /* bus of the device usb/scsi */
    u_char *depths;               /* bit depth table */
    SANE_Char *firmware_filename; /* The name of the firmware file for USB scanners */
    struct snapscan_device *pnext;
}
SnapScan_Device;

#define MAX_SCSI_CMD_LEN 256    /* not that large */
#define DEFAULT_SCANNER_BUF_SZ 1024*63

typedef struct snapscan_scanner SnapScan_Scanner;

#include <snapscan-sources.h>

struct snapscan_scanner
{
    SANE_String devname;          /* the scsi device name */
    SnapScan_Device *pdev;        /* the device */
    int fd;                       /* scsi file descriptor */
    int opens;                    /* open count */
    int rpipe[2];                 /* reader pipe descriptors */
    int orig_rpipe_flags;         /* initial reader pipe flags */
    pid_t child;                  /* child reader process pid */
    SnapScan_Mode mode;           /* mode */
    SnapScan_Mode preview_mode;   /* preview mode */
    SnapScan_Source source;       /* scanning source */
    SnapScan_State state;         /* scanner state */
    u_char cmd[MAX_SCSI_CMD_LEN]; /* scsi command buffer */
    u_char *buf;                  /* data buffer */
    size_t phys_buf_sz;           /* physical buffer size */
    size_t buf_sz;                /* effective buffer size */
    size_t expected_read_bytes;   /* expected amount of data in a single read */
    size_t read_bytes;            /* amount of actual data read */
    size_t bytes_remaining;       /* remaining bytes expected from scanner */
    size_t actual_res;            /* actual resolution */
    size_t lines;                 /* number of scan lines */
    size_t bytes_per_line;        /* bytes per scan line */
    size_t pixels_per_line;       /* pixels per scan line */
    u_char hconfig;               /* hardware configuration byte */
    float ms_per_line;            /* speed: milliseconds per scan line */
    SANE_Bool nonblocking;        /* wait on reads for data? */
    char *sense_str;              /* sense string */
    char *as_str;                 /* additional sense string */
    u_char asi1;                  /* first additional sense info byte */
    u_char asi2;                  /* second additional sense info byte */
    SANE_Byte chroma_offset[3];   /* chroma offsets */
    SANE_Int chroma;
    Source *psrc;                 /* data source */
    SANE_Option_Descriptor options[NUM_OPTS];  /* the option descriptors */
    Option_Value val[NUM_OPTS];  /* the options themselves... */
    SANE_Int res;                /* resolution */
    SANE_Bool preview;           /* preview mode toggle */
    SANE_String mode_s;          /* scanning mode */
    SANE_String source_s;        /* scanning source */
    SANE_String preview_mode_s;  /* scanning mode for preview */
    SANE_Fixed tlx;              /* window top left x */
    SANE_Fixed tly;              /* window top left y */
    SANE_Fixed brx;              /* window bottom right x */
    SANE_Fixed bry;              /* window bottom right y */
    int bright;                  /* brightness */
    int contrast;                /* contrast */
    SANE_String predef_window;   /* predefined window name */
    SANE_Fixed gamma_gs;         /* gamma correction value (greyscale) */
    SANE_Fixed gamma_r;          /* gamma correction value (red) */
    SANE_Fixed gamma_g;          /* gamma correction value (green) */
    SANE_Fixed gamma_b;          /* gamma correction value (blue) */
    SANE_Int *gamma_tables;      /* gamma correction vectors */
    SANE_Int *gamma_table_gs;    /* gamma correction vector (greyscale) */
    SANE_Int *gamma_table_r;     /* gamma correction vector (red) */
    SANE_Int *gamma_table_g;     /* gamma correction vector (green) */
    SANE_Int *gamma_table_b;     /* gamma correction vector (blue) */
    int gamma_length;            /* length of gamma vectors */
    SANE_Bool halftone;          /* halftone toggle */
    SANE_String dither_matrix;   /* the halftone dither matrix */
    SANE_Bool negative;          /* swap black and white */
    SANE_Int threshold;          /* threshold for line art */
    SANE_Int rgb_lpr;            /* lines per scsi read (RGB) */
    SANE_Int gs_lpr;             /* lines per scsi read (greyscale) */
};

#endif

/*
 * $Log$
 * Revision 1.21  2003/04/30 20:49:40  oliverschwartz
 * SnapScan backend 1.4.26
 *
 * Revision 1.38  2003/04/30 20:42:22  oliverschwartz
 * Added support for Agfa Arcus 1200 (supplied by Valtteri Vuorikoski)
 *
 * Revision 1.37  2003/02/05 22:11:11  oliverschwartz
 * Added Epson Perfection 660
 *
 * Revision 1.36  2003/01/08 21:16:36  oliverschwartz
 * Added support for Acer / Benq 310U
 *
 * Revision 1.35  2002/12/10 20:14:12  oliverschwartz
 * Enable color offset correction for SnapScan300
 *
 * Revision 1.34  2002/10/12 10:40:48  oliverschwartz
 * Added support for Snapscan e10
 *
 * Revision 1.33  2002/09/24 16:07:47  oliverschwartz
 * Added support for Benq 5000
 *
 * Revision 1.32  2002/07/12 22:22:47  oliverschwartz
 * Correct driver description for 4300_2
 *
 * Revision 1.31  2002/04/27 14:44:27  oliverschwartz
 * - Remove SCSI debug options
 *
 * Revision 1.30  2002/04/23 22:51:00  oliverschwartz
 * Cleanup, support for ADF
 *
 * Revision 1.29  2002/03/24 12:14:34  oliverschwartz
 * Add Snapcan_Driver_desc
 *
 * Revision 1.28  2002/01/23 20:38:20  oliverschwartz
 * Fix model ID for e42
 * Improve recognition of Acer 320U
 *
 * Revision 1.27  2002/01/06 18:34:02  oliverschwartz
 * Added support for Snapscan e42 thanks to Yari Adán Petralanda
 *
 * Revision 1.26  2001/12/20 23:18:01  oliverschwartz
 * Remove tmpfname
 *
 * Revision 1.25  2001/12/18 18:28:35  oliverschwartz
 * Removed temporary file
 *
 * Revision 1.24  2001/12/12 19:44:59  oliverschwartz
 * Clean up CVS log
 *
 * Revision 1.23  2001/11/25 18:51:41  oliverschwartz
 * added support for SnapScan e52 thanks to Rui Lopes
 *
 * Revision 1.22  2001/11/16 20:56:47  oliverschwartz
 * additional identification string for e26 added
 *
 * Revision 1.21  2001/11/16 20:28:35  oliverschwartz
 * add support for Snapscan e26
 *
 * Revision 1.20  2001/11/16 20:23:16  oliverschwartz
 * Merge with sane-1.0.6
 *   - Check USB vendor IDs to avoid hanging scanners
 *   - fix bug in dither matrix computation
 *
 * Revision 1.19  2001/10/11 14:02:10  oliverschwartz
 * Distinguish between e20/e25 and e40/e50
 *
 * Revision 1.18  2001/10/10 10:11:10  oliverschwartz
 * Add support for Snapscan e25 thanks to Rodolphe Suescun
 *
 * Revision 1.17  2001/10/08 18:22:02  oliverschwartz
 * - Disable quality calibration for Acer Vuego 310F
 * - Use sanei_scsi_max_request_size as scanner buffer size
 *   for SCSI devices
 * - Added new devices to snapscan.desc
 *
 * Revision 1.16  2001/09/28 13:39:16  oliverschwartz
 * - Added "Snapscan 300" ID string
 * - cleanup
 * - more debugging messages in snapscan-sources.c
 *
 * Revision 1.15  2001/09/18 15:01:07  oliverschwartz
 * - Read scanner id string again after firmware upload
 *   to indentify correct model
 * - Make firmware upload work for AGFA scanners
 * - Change copyright notice
 *
 * Revision 1.14  2001/09/17 10:01:08  sable
 * Added model AGFA 1236U
 *
 * Revision 1.13  2001/09/09 20:39:52  oliverschwartz
 * add identification for 620ST+
 *
 * Revision 1.12  2001/09/09 18:06:32  oliverschwartz
 * add changes from Acer (new models; automatic firmware upload for USB scanners); fix distorted colour scans after greyscale scans (call set_window only in sane_start); code cleanup
 *
 * Revision 1.11  2001/04/10 12:38:21  sable
 * Adding e20 support thanks to Steffen Hübner
 *
 * Revision 1.10  2001/04/10 11:04:31  sable
 * Adding support for snapscan e40 an e50 thanks to Giuseppe Tanzilli
 *
 * Revision 1.9  2001/03/17 22:53:21  sable
 * Applying Mikael Magnusson patch concerning Gamma correction
 * Support for 1212U_2
 *
 * Revision 1.8  2000/11/10 01:01:59  sable
 * USB (kind of) autodetection
 *
 * Revision 1.7  2000/11/01 01:26:43  sable
 * Support for 1212U
 *
 * Revision 1.6  2000/10/28 14:06:35  sable
 * Add support for Acer300f
 *
 * Revision 1.5  2000/10/15 17:54:58  cbagwell
 * Adding USB files for optional USB compiles.
 *
 * Revision 1.4  2000/10/13 03:50:27  cbagwell
 * Updating to source from SANE 1.0.3.  Calling this versin 1.1
 *
 * Revision 1.3  2000/08/12 15:09:37  pere
 * Merge devel (v1.0.3) into head branch.
 *
 * Revision 1.1.1.1.2.2  2000/07/13 04:47:50  pere
 * New snapscan backend version dated 20000514 from Steve Underwood.
 *
 * Revision 1.2.1  2000/05/14 13:30:20  coppice
 * Some reformatting a minor tidying.
 *
 * Revision 1.2  2000/03/05 13:55:21  pere
 * Merged main branch with current DEVEL_1_9.
 *
 * Revision 1.1.1.1.2.1  1999/09/15 18:20:02  charter
 * Early version 1.0 snapscan.h
 *
 * Revision 2.2  1999/09/09 18:25:02  charter
 * Checkpoint. Removed references to snapscan-310.c stuff using
 * "#ifdef OBSOLETE".
 *
 * Revision 2.1  1999/09/08 03:05:05  charter
 * Start of branch 2; same as 1.30.
 *
 * Revision 1.30  1999/09/07 20:54:07  charter
 * Changed expected_data_len to bytes_remaining.
 *
 * Revision 1.29  1999/09/02 05:29:46  charter
 * Fixed the spelling of Petter's name (again).
 *
 * Revision 1.28  1999/09/02 05:28:50  charter
 * Added Gary Plewa's name to the list of contributors.
 *
 * Revision 1.27  1999/09/02 04:48:25  charter
 * Added models and strings for the Acer PRISA 620s (thanks to Gary Plewa).
 *
 * Revision 1.26  1999/09/02 02:01:46  charter
 * Checking in rev 1.26 (for backend version 0.7) again.
 * This is part of the recovery from the great disk crash of Sept 1, 1999.
 *
 * Revision 1.26  1999/07/09 20:54:34  charter
 * Modifications for SnapScan 1236s (Petter Reinholdsten).
 *
 * Revision 1.25  1998/12/16 18:40:53  charter
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
