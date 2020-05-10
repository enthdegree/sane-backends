/* sane - Scanner Access Now Easy.

   Copyright (C) 2019 Touboul Nathane
   Copyright (C) 2019 Thierry HUCHARD <thierry@ordissimo.com>

   This file is part of the SANE package.

   SANE is free software; you can redistribute it and/or modify it under
   the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 3 of the License, or (at your
   option) any later version.

   SANE is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
   for more details.

   You should have received a copy of the GNU General Public License
   along with sane; see the file COPYING.  If not, write to the Free
   Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   This file implements a SANE backend for eSCL scanners.  */


#ifndef __ESCL_H__
#define __ESCL_H__

#include "../include/sane/config.h"

#if !(HAVE_LIBCURL && defined(WITH_AVAHI) && defined(HAVE_LIBXML2))
#error "The escl backend requires libcurl, libavahi and libxml2"
#endif

#ifndef HAVE_LIBJPEG
/* FIXME: Make JPEG support optional.
   Support for PNG and PDF is to be added later but currently only
   JPEG is supported.  Absence of JPEG support makes the backend a
   no-op at present.
 */
#error "The escl backend currently requires libjpeg"
#endif

#include "../include/sane/sane.h"

#include <stdio.h>
#include <math.h>

#ifndef BACKEND_NAME
#define BACKEND_NAME escl
#endif

#define DEBUG_NOT_STATIC
#include "../include/sane/sanei_debug.h"

#ifndef DBG_LEVEL
#define DBG_LEVEL       PASTE(sanei_debug_, BACKEND_NAME)
#endif
#ifndef NDEBUG
# define DBGDUMP(level, buf, size) \
    do { if (DBG_LEVEL >= (level)) sanei_escl_dbgdump(buf, size); } while (0)
#else
# define DBGDUMP(level, buf, size)
#endif

#define ESCL_CONFIG_FILE "escl.conf"


enum {
   PLATEN = 0,
   ADFSIMPLEX,
   ADFDUPLEX
};


typedef struct {
    int             p1_0;
    int             p2_0;
    int             p3_3;
    int             DocumentType;
    int             p4_0;
    int             p5_0;
    int             p6_1;
    int             reserve[11];
} ESCL_SCANOPTS;


typedef struct ESCL_Device {
    struct ESCL_Device *next;

    char    *model_name;
    int             port_nb;
    char      *ip_address;
    char *type;
    SANE_Bool https;
    char      *unix_socket;
} ESCL_Device;

typedef struct capst
{
    int height;
    int width;
    int pos_x;
    int pos_y;
    SANE_String default_color;
    SANE_String default_format;
    SANE_Int default_resolution;
    int MinWidth;
    int MaxWidth;
    int MinHeight;
    int MaxHeight;
    int MaxScanRegions;
    SANE_String_Const *ColorModes;
    int ColorModesSize;
    SANE_String_Const *ContentTypes;
    int ContentTypesSize;
    SANE_String_Const *DocumentFormats;
    int DocumentFormatsSize;
    int format_ext;
    SANE_Int *SupportedResolutions;
    int SupportedResolutionsSize;
    SANE_String_Const *SupportedIntents;
    int SupportedIntentsSize;
    SANE_String_Const SupportedIntentDefault;
    int MaxOpticalXResolution;
    int RiskyLeftMargin;
    int RiskyRightMargin;
    int RiskyTopMargin;
    int RiskyBottomMargin;
    int duplex;
} caps_t;

typedef struct capabilities
{
    caps_t caps[3];
    int source;
    SANE_String_Const *Sources;
    int SourcesSize;
    FILE *tmp;
    unsigned char *img_data;
    long img_size;
    long img_read;
    SANE_Bool work;
} capabilities_t;

typedef struct {
    int                             XRes;
    int                             YRes;
    int                             Left;
    int                             Top;
    int                             Right;
    int                             Bottom;
    int                             ScanMode;
    int                             ScanMethod;
    ESCL_SCANOPTS  opts;
} ESCL_ScanParam;


enum
{
    OPT_NUM_OPTS = 0,
    OPT_MODE_GROUP,
    OPT_MODE,
    OPT_RESOLUTION,
    OPT_PREVIEW,
    OPT_GRAY_PREVIEW,

    OPT_GEOMETRY_GROUP,
    OPT_TL_X,
    OPT_TL_Y,
    OPT_BR_X,
    OPT_BR_Y,

    OPT_SCAN_SOURCE,

    NUM_OPTIONS
};

#define PIXEL_TO_MM(pixels, dpi) SANE_FIX((double)pixels * 25.4 / (dpi))
#define MM_TO_PIXEL(millimeters, dpi) (SANE_Word)round(SANE_UNFIX(millimeters) * (dpi) / 25.4)

ESCL_Device *escl_devices(SANE_Status *status);
SANE_Status escl_device_add(int port_nb, const char *model_name,
		            char *ip_address, char *type);
SANE_Status escl_status(const ESCL_Device *device,
                        int source,
                        const char* jobId,
                        SANE_Status *job);
capabilities_t *escl_capabilities(const ESCL_Device *device, SANE_Status *status);
char *escl_newjob(capabilities_t *scanner, const ESCL_Device *device,
		  SANE_Status *status);
SANE_Status escl_scan(capabilities_t *scanner, const ESCL_Device *device,
	              char *result);
void escl_scanner(const ESCL_Device *device, char *result);

typedef void CURL;
void escl_curl_url(CURL *handle, const ESCL_Device *device, SANE_String_Const path);

unsigned char *escl_crop_surface(capabilities_t *scanner, unsigned char *surface,
	                      int w, int h, int bps, int *width, int *height);

// JPEG
SANE_Status get_JPEG_data(capabilities_t *scanner, int *width, int *height, int *bps);

// PNG
SANE_Status get_PNG_data(capabilities_t *scanner, int *width, int *height, int *bps);

// TIFF
SANE_Status get_TIFF_data(capabilities_t *scanner, int *width, int *height, int *bps);

// PDF
SANE_Status get_PDF_data(capabilities_t *scanner, int *width, int *height, int *bps);

#endif
