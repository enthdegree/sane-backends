/*.............................................................................
 * Project : SANE library for Plustek flatbed scanners.
 *.............................................................................
 */

/** @file plustek-pp.h
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
 * - 0.01 - initial version
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
#ifndef __PLUSTEKPP_H__
#define __PLUSTEKPP_H__

/************************ some definitions ***********************************/

/* NOTE: needs to be kept in sync with table below */
#define MODELSTR static char *ModelStr[] = { \
    "unknown",						 \
    "Primax 4800",  				 \
    "Primax 4800 Direct",  			 \
    "Primax 4800 Direct 30Bit", 	 \
    "Primax 9600 Direct 30Bit", 	 \
    "4800P",  						 \
    "4830P",  						 \
    "600P/6000P",					 \
    "4831P",  						 \
    "9630P",  						 \
    "9630PL",  						 \
    "9636P",  						 \
    "A3I",    						 \
    "12000P/96000P",				 \
    "9636P+/Turbo",					 \
    "9636T/12000T",					 \
	"P8",							 \
	"P12",							 \
	"PT12",							 \
    "Genius Colorpage Vivid III V2", \
	"USB-Device"					 \
}

/* the models */
#define MODEL_OP_UNKNOWN  0	/* unknown */
#define MODEL_PMX_4800	  1 /* Primax Colorado 4800 like OP 4800 			 */
#define MODEL_PMX_4800D   2 /* Primax Compact 4800 Direct, OP 600 R->G, G->R */
#define MODEL_PMX_4800D3  3 /* Primax Compact 4800 Direct 30                 */
#define MODEL_PMX_9600D3  4 /* Primax Compact 9600 Direct 30                 */
#define MODEL_OP_4800P 	  5 /* 32k,  96001 ASIC, 24 bit, 300x600, 8.5x11.69  */
#define MODEL_OP_4830P 	  6 /* 32k,  96003 ASIC, 30 bit, 300x600, 8.5x11.69  */
#define MODEL_OP_600P 	  7	/* 32k,  96003 ASIC, 30 bit, 300x600, 8.5x11.69  */
#define MODEL_OP_4831P 	  8 /* 128k, 96003 ASIC, 30 bit, 300x600, 8.5x11.69  */
#define MODEL_OP_9630P 	  9	/* 128k, 96003 ASIC, 30 bit, 600x1200, 8.5x11.69 */
#define MODEL_OP_9630PL	 10	/* 128k, 96003 ASIC, 30 bit, 600x1200, 8.5x14	 */
#define MODEL_OP_9636P 	 11	/* 512k, 98001 ASIC, 36 bit, 600x1200, 8.5x11.69 */
#define MODEL_OP_A3I 	 12	/* 128k, 96003 ASIC, 30 bit, 400x800,  11.69x17  */
#define MODEL_OP_12000P  13	/* 128k, 96003 ASIC, 30 bit, 600x1200, 8.5x11.69 */
#define MODEL_OP_9636PP  14	/* 512k, 98001 ASIC, 36 bit, 600x1200, 8.5x11.69 */
#define MODEL_OP_9636T 	 15	/* like OP_9636PP + transparency 				 */
#define MODEL_OP_P8      16 /* 512k, 98003 ASIC, 36 bit,  300x600, 8.5x11.69 */
#define MODEL_OP_P12     17 /* 512k, 98003 ASIC, 36 bit, 600x1200, 8.5x11.69 */
#define MODEL_OP_PT12    18 /* like OP_P12 + transparency 					 */
#define MODEL_GEN_CPV2   19 /* Genius Colorpage Vivid III V2, ASIC 98003     */
#define MODEL_UNKNOWN	 20 /* not known/supported                           */

#define _NO_BASE	0xFFFF

/******************** from former plustek-share.h ***************************/

/*
 * for other OS than Linux, we might have to define the _IO macros
 */
#ifndef _IOC
#define _IOC(dir,type,nr,size) \
	(((dir)  << 30) | \
	 ((type) << 8)  | \
	 ((nr)   << 0)  | \
	 ((size) << 16))
#endif

#ifndef _IO
#define _IO(type,nr)		_IOC(0U,(type),(nr),0)
#endif

#ifndef _IOR
#define _IOR(type,nr,size)	_IOC(2U,(type),(nr),sizeof(size))
#endif

#ifndef _IOW
#define _IOW(type,nr,size)	_IOC(1U,(type),(nr),sizeof(size))
#endif

#ifndef _IOWR
#define _IOWR(type,nr,size)	_IOC(3U,(type),(nr),sizeof(size))
#endif

/*.............................................................................
 * the ioctl interface
 */
#define _PTDRV_OPEN_DEVICE 	    _IOW('x', 1, unsigned short)/* open			 */
#define _PTDRV_GET_CAPABILITIES _IOR('x', 2, ScannerCaps)	/* get caps		 */
#define _PTDRV_GET_LENSINFO 	_IOR('x', 3, LensInfo)		/* get lenscaps	 */
#define _PTDRV_PUT_IMAGEINFO 	_IOW('x', 4, ImgDef)		/* put image info*/
#define _PTDRV_GET_CROPINFO 	_IOR('x', 5, CropInfo)		/* get crop		 */
#define _PTDRV_SET_ENV 			_IOWR('x',6, ScanInfo)		/* set env.		 */
#define _PTDRV_START_SCAN 		_IOR('x', 7, StartScan)		/* start scan 	 */
#define _PTDRV_STOP_SCAN 		_IOWR('x', 8, int)			/* stop scan 	 */
#define _PTDRV_CLOSE_DEVICE 	_IO('x',  9)				/* close 		 */
#define _PTDRV_ACTION_BUTTON	_IOR('x', 10, int)	 		/* rd act. button*/
#define _PTDRV_ADJUST           _IOR('x', 11, AdjDef)		/* adjust driver */
#define _PTDRV_SETMAP           _IOR('x', 12, MapDef)		/* download gamma*/

/*
 * this version MUST match the one inside the driver to make sure, that
 * both sides use the same structures. This version changes each time
 * the ioctl interface changes
 */
#define _PTDRV_COMPAT_IOCTL_VERSION	0x0102
#define _PTDRV_IOCTL_VERSION		0x0103

/** for adjusting the parport stuff
 */
typedef struct {
	int     lampOff;
	int     lampOffOnEnd;
	int     warmup;
	int     enableTpa;

	OffsDef pos; 	/* for adjusting normal scan area       */
	OffsDef tpa; 	/* for adjusting transparency scan area */
	OffsDef neg; 	/* for adjusting negative scan area     */

	/* for adjusting the default gamma settings */
	double  rgamma;
	double  ggamma;
	double  bgamma;

	double  graygamma;

} PPAdjDef, *pPPAdjDef;

/** for adjusting the scanner settings
 */
typedef struct {

	int 	direct_io;
	int     mov;

	int     lampOff;
	int     lampOffOnEnd;
	int     warmup;

	OffsDef pos; 	/* for adjusting normal scan area       */
	OffsDef tpa; 	/* for adjusting transparency scan area */
	OffsDef neg; 	/* for adjusting negative scan area     */

	/* for adjusting the default gamma settings */
	double  rgamma;
	double  ggamma;
	double  bgamma;

	double  graygamma;

} AdjDef, *pAdjDef;


#ifndef __KERNEL__

#define MM_PER_INCH         25.4

#define PLUSTEK_CONFIG_FILE	"plustek_pp.conf"

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

/** for compatiblity to version 0x0102 drivers
 */
typedef struct {

	int     lampOff;
	int     lampOffOnEnd;
	int     warmup;

	OffsDef pos; 	/* for adjusting normal scan area       */
	OffsDef tpa; 	/* for adjusting transparency scan area */
	OffsDef neg; 	/* for adjusting negative scan area     */

} CompatAdjDef, *pCompatAdjDef;

/**
 */
typedef struct Plustek_Device
{
	SANE_Int               initialized;      /* device already initialized?  */
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

#ifndef SANE_OPTION
/* for compatibility with older versions */
typedef union
{
	SANE_Word w;
	SANE_Word *wa;		/* word array */
	SANE_String s;
} Option_Value;
#endif

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

/** for collecting configuration info...
 */
typedef struct {
	
	char   devName[PATH_MAX];

	/* contains the stuff to adjust... */
	AdjDef adj;

} CnfDef, *pCnfDef;
#endif /* guard __KERNEL__ */

#endif	/* guard __PLUSTEKPP_H__ */

/* END PLUSTEK-PP.H .........................................................*/
