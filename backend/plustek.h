/*.............................................................................
 * Project : SANE library for Plustek flatbed scanners.
 *.............................................................................
 */

/** @file plustek.h
 *  @brief Definitions for the backend.
 *
 * Based on Kazuhiro Sasayama previous
 * Work on plustek.[ch] file from the SANE package.<br>
 *
 * original code taken from sane-0.71<br>
 * Copyright (C) 1997 Hypercore Software Design, Ltd.<br>
 * Copyright (C) 2001-2003 Gerhard Jaeger <gerhard@gjaeger.de>
 *
 * History:
 * - 0.30 - initial version
 * - 0.31 - no changes
 * - 0.32 - no changes
 * - 0.33 - no changes
 * - 0.34 - moved some definitions and typedefs from plustek.c
 * - 0.35 - removed OPT_MODEL from options list
 *		  - added max_y to struct Plustek_Scan
 * - 0.36 - added reader_pid, pipe and bytes_read to struct Plustek_Scanner
 *		  - removed unused variables from struct Plustek_Scanner
 *        - moved fd from struct Plustek_Scanner to Plustek_Device
 *		  - added next members to struct Plustek_Scanner and Plustek_Device
 * - 0.37 - added max_x to struct Plustek_Device
 * - 0.38 - added caps to struct Plustek_Device
 *        - added exit code to struct Plustek_Scanner
 *        - removed dropout stuff
 * - 0.39 - PORTTYPE enum
 *        - added function pointers to control a scanner device
 *        (Parport and USB)
 * - 0.40 - added USB stuff
 * - 0.41 - added configuration stuff
 * - 0.42 - added custom gamma tables
 *        - changed usbId to static array
 *		  - added _MAX_ID_LEN
 * - 0.43 - no changes
 * - 0.44 - added flag initialized
 * - 0.45 - added readLine function
 * .
 * <hr>
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
 * <hr>
 */
#ifndef __PLUSTEK_H__
#define __PLUSTEK_H__

/************************ some definitions ***********************************/

#define MM_PER_INCH         25.4

#define PLUSTEK_CONFIG_FILE	"plustek.conf"

#ifndef PATH_MAX
# define PATH_MAX 1024
#endif

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

#define _MAX_ID_LEN	20

/************************ some structures ************************************/

enum {
    OPT_NUM_OPTS = 0,
    OPT_MODE_GROUP,
    OPT_MODE,
	OPT_EXT_MODE,
    OPT_RESOLUTION,
    OPT_PREVIEW,
    OPT_GEOMETRY_GROUP,
    OPT_TL_X,
    OPT_TL_Y,
    OPT_BR_X,
    OPT_BR_Y,
	OPT_ENHANCEMENT_GROUP,
    OPT_HALFTONE,
    OPT_BRIGHTNESS,
    OPT_CONTRAST,
    OPT_CUSTOM_GAMMA,
    OPT_GAMMA_VECTOR,
    OPT_GAMMA_VECTOR_R,
    OPT_GAMMA_VECTOR_G,
    OPT_GAMMA_VECTOR_B,
    NUM_OPTIONS
};

/*
 * to distinguish between parallelport and USB device
 */
typedef enum {
	PARPORT = 0,
	USB,
	NUM_PORTTYPES
} PORTTYPE;

typedef struct Plustek_Device
{
	SANE_Bool              initialized;      /* device already initialized?  */
	struct Plustek_Device *next;             /* pointer to next dev in list  */
	int 				   fd;				 /* device handle                */
    char                  *name;             /* (to avoid compiler warnings!)*/
    SANE_Device 		   sane;             /* info struct                  */
	SANE_Int			   max_x;            /* max XY-extension of the scan-*/
	SANE_Int			   max_y;            /* area                         */
    SANE_Range 			   dpi_range;        /* resolution range             */
    SANE_Range 			   x_range;          /* x-range of the scan-area     */
    SANE_Range 			   y_range;          /* y-range of the scan-area     */
    SANE_Int  		 	  *res_list;         /* to hold the available phys.  */
    SANE_Int 			   res_list_size;    /* resolution values            */
    ScannerCaps            caps;             /* caps reported by the driver  */
	AdjDef                 adj;	             /* for driver adjustment        */
	
    /**************************** USB-stuff **********************************/
    char                   usbId[_MAX_ID_LEN];/* to keep Vendor and product  */
                                             /* ID string (from conf) file   */
#ifdef _PLUSTEK_USB
    ScanDef                scanning;         /* here we hold all stuff for   */
                                             /* the USB-scanner              */
	DeviceDef              usbDev;	
	struct itimerval       saveSettings;     /* for lamp timer               */
#endif

    /*
     * each device we support may need other access functions...
     */
    int  (*open)       ( const char*, void* );
    int  (*close)      ( struct Plustek_Device* );
    void (*shutdown)   ( struct Plustek_Device* );
    int  (*getCaps)    ( struct Plustek_Device* );
    int  (*getLensInfo)( struct Plustek_Device*, pLensInfo  );
    int  (*getCropInfo)( struct Plustek_Device*, pCropInfo  );
    int  (*putImgInfo) ( struct Plustek_Device*, pImgDef    );
    int  (*setScanEnv) ( struct Plustek_Device*, pScanInfo  );
    int  (*setMap)     ( struct Plustek_Device*, SANE_Word*,
												 SANE_Word, SANE_Word );
    int  (*startScan)  ( struct Plustek_Device*, pStartScan );
    int  (*stopScan)   ( struct Plustek_Device*, int* );
    int  (*readImage)  ( struct Plustek_Device*, SANE_Byte*, unsigned long );

    int  (*prepare)    ( struct Plustek_Device*, SANE_Byte* );
    int  (*readLine)   ( struct Plustek_Device* );

} Plustek_Device, *pPlustek_Device;

typedef union
{
	SANE_Word w;
	SANE_Word *wa;		/* word array */
	SANE_String s;
} Option_Value;

typedef struct Plustek_Scanner
{
    struct Plustek_Scanner *next;
    pid_t 					reader_pid;		/* process id of reader          */
    SANE_Status             exit_code;      /* status of the reader process  */
    int 					pipe;			/* pipe to reader process        */
	unsigned long			bytes_read;		/* number of bytes currently read*/
    Plustek_Device 		   *hw;				/* pointer to current device     */
    Option_Value 			val[NUM_OPTIONS];
    SANE_Byte 			   *buf;            /* the image buffer              */
    SANE_Bool 				scanning;       /* TRUE during scan-process      */
    SANE_Parameters 		params;         /* for keeping the parameter     */
	
	/************************** gamma tables *********************************/
	
	SANE_Word	gamma_table[4][4096];
	SANE_Range	gamma_range;
	int 		gamma_length;

    SANE_Option_Descriptor	opt[NUM_OPTIONS];

} Plustek_Scanner, *pPlustek_Scanner;


typedef const struct mode_param {
	int color;
	int depth;
	int scanmode;
} ModeParam, *pModeParam;


/*
 * for collecting configuration info...
 */
typedef struct {
	
	char     devName[PATH_MAX];
	PORTTYPE porttype;
	char     usbId[_MAX_ID_LEN];	

	/* contains the stuff to adjust... */
	AdjDef   adj;

} CnfDef, *pCnfDef;

#endif	/* guard __PLUSTEK_H__ */

/* END PLUSTEK.H.............................................................*/
