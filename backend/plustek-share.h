/*.............................................................................
 * Project : SANE library for Plustek flatbed scanners.
 *.............................................................................
 * File:	 plustek-share.h - definitions for the backend and the driver
 *.............................................................................
 *
 * Copyright (C) 2000/2001 Gerhard Jaeger <g.jaeger@earthling.net>
 * Last Update:
 *		Gerhard Jaeger <g.jaeger@earthling.net>
 *.............................................................................
 * History:
 * 0.36 - initial version
 * 0.37 - updated scanner info list
 *        removed override switches
 * 0.38 - changed the dwFlag entry in ScannerCaps and its meaning
 *        changed _NO_BASE
 *        fixed model list
 *		  removed gray-scale capabilities for TPA scans
 * 0.39 - added user-space stuff
 *        added Genius Colorpage Vivid III V2 stuff
 * 0.40 - added stuff to share with USB and Parport
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
#ifndef __PLUSTEK_SHARE_H__
#define __PLUSTEK_SHARE_H__

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
#define _PTDRV_PUT_IMAGEINFO 	_IOW('x', 4, CmdBlk)		/* put image info*/
#define _PTDRV_GET_CROPINFO 	_IOR('x', 5, CropInfo)		/* get crop		 */
#define _PTDRV_SET_ENV 			_IOWR('x',6, ScanInfo)		/* set env.		 */
#define _PTDRV_START_SCAN 		_IOR('x', 7, StartScan)		/* start scan 	 */
#define _PTDRV_STOP_SCAN 		_IOWR('x', 8, int)			/* stop scan 	 */
#define _PTDRV_CLOSE_DEVICE 	_IO('x',  9)				/* close 		 */
#define _PTDRV_ACTION_BUTTON	_IOR('x', 10, int)	 		/* rd act. button*/

/*
 * this version MUST match the one inside the driver to make sure, that
 * both sides use the same structures. This version changes each time
 * the ioctl interface changes
 */
#define _PTDRV_IOCTL_VERSION	0x0101

/*.............................................................................
 * the structures for driver communication
 */
typedef struct {
  	unsigned long dwFlag;  			/* refer to SECTION (1.2)			*/
	unsigned long dwBytesPerLine;
	unsigned long dwLinesPerScan;
} StartScan, *pStartScan;

typedef struct {
	unsigned short x;
	unsigned short y;
} XY, *pXY;

typedef struct {
	unsigned short x;
    unsigned short y;
    unsigned short cx;
    unsigned short cy;
} CropRect, *pCropRect;

typedef struct {
	unsigned long	dwFlag;
    CropRect 		crArea;
    XY		 		xyDpi;
    unsigned short	wDataType;
    unsigned short  wBits;			/* see section 3.4 					*/
    unsigned short	wLens;
} ImgDef, *pImgDef;

typedef struct {
    unsigned long	dwPixelsPerLine;
    unsigned long	dwBytesPerLine;
    unsigned long	dwLinesPerArea;
    unsigned long	dwOffsetX;
    unsigned long	dwOffsetY;
    ImgDef 	        ImgDef;
} CropInfo, *pCropInfo;

typedef struct {
    unsigned char*	pDither;
    void*	    	pMap;
    ImgDef	    	ImgDef;
	unsigned short	wMapType;		/* refer to SECTION (3.2)			*/
	unsigned short	wDither;		/* refer to SECTION (3.3)			*/
    short	    	siBrightness;	/* refer to SECTION (3.5)			*/
    short	    	siContrast;    	/* refer to SECTION (3.6)			*/
} ScanInfo, *pScanInfo;

typedef struct {
    unsigned short	wMin;       /* minimum value						*/
    unsigned short	wDef;       /* default value						*/
    unsigned short	wMax;		/* software maximum value				*/
    unsigned short	wPhyMax;	/* hardware maximum value (for DPI only)*/
} RANGE, *PRANGE;

typedef struct {
    RANGE	    	rDataType;      /* available scan modes 			*/
    unsigned long	dwBits;			/* refer to SECTION (1.1)           */
    unsigned long	dwFlag;		    /* refer to SECTION (1.2)           */
    unsigned short	wIOBase;		/* refer to SECTION (1.3)			*/
    unsigned short	wLens;			/* number of sensors 				*/
    unsigned short	wMaxExtentX;	/* scanarea width					*/
    unsigned short	wMaxExtentY;	/* scanarea height					*/
    unsigned short	AsicID;  		/* copy of RegAsicID 				*/
  	unsigned short	Model;  		/* model as best we can determine 	*/
    unsigned short	Version;		/* drivers version					*/
} ScannerCaps, *pScannerCaps;

typedef struct {
    RANGE	    	rDpiX;
    RANGE	    	rDpiY;
    RANGE	    	rExtentX;
    RANGE	    	rExtentY;
    unsigned short	wBeginX;		/* offset from left */
    unsigned short	wBeginY;		/* offset from top	*/
} LensInfo, *pLensInfo;

/* TODO: move it out the way...*/
typedef struct {
    union {
		unsigned short	wShadingBufSize;	/* output			*/
		unsigned short	wMapType;			/* input			*/
		unsigned short	wIDOwner;			/* output			*/
		unsigned short	wLensNumber;		/* input/output		*/
		unsigned short	wCmd;				/* GetScannerStatus	*/
		unsigned long	wError; 			/* Get last error	*/
		ScanInfo 		sInf;				/* input			*/
		CropInfo 		cInf;				/* input			*/
		unsigned long	dwSize;				/* prefer size		*/
    } ucmd;
} CmdBlk, *pCmdBlk;

/*
 * useful for description tables
 */
typedef struct {
	int	  id;
	char *desc;	
} TabDef, *pTabDef;



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
#define MODEL_OP_USB	 20 /* some USB scanner device                       */

/******************************************************************************
 * Section 1
 * (1.1): SCANNERINFO.dwBits
 */
#define _BITS_8					0x00000001
#define _BITS_10				0x00000002
#define _BITS_12				0x00000003

/* (1.2): SCANNERINFO.dwFlag */
#define SFLAG_MULTIFUNC         0x00000001  /* is multifunction device      */
#define SFLAG_SCANNERDEV        0x00000002  /* is scannerdevice             */
#define SFLAG_FLATBED           0x00000004  /* is flatbed scanner           */
#define SFLAG_PRINTEROPT        0x00000008  /* has printer option           */

#define SFLAG_ADF		    	0x00000010  /* Automatic document feeder    */
#define SFLAG_MFP       	    0x00000020	/* MF-Keypad support		    */
#define SFLAG_SheetFed		    0x00000040  /* Sheetfed support             */
#define SFLAG_TPA       	    0x00000080	/* has transparency	adapter     */
#define SFLAG_BUTTONOPT         0x00000100  /* has buttons                  */

/*
 * (1.2.1) Provide the scanner ID. This field is valid when the wIOBase
 * field of ScannerCaps is not _NO_BASE
 */
#define SFLAG_CCDTypeMask	    0x000f0000		/* CCD type, system reserved*/
#define SFLAG_IDMask		    0x00f00000		/* Scanner ID				*/

/* (1.3): SCANNERINFO.wIOBase */
#define _NO_BASE	0xFFFF

/******************************************************************************
 * Section 2
 * (2.1): MAPINFO.wRGB
 */
#define RGB_Master		    1
#define RGB_RGB 		    3

/******************************************************************************
 * Section 3
 * (3.1): SCANINFO.dwFlag
 */
#define SCANDEF_Inverse 	    	0x00000001
#define SCANDEF_UnlimitLength	    0x00000002
#define SCANDEF_StopWhenPaperOut	0x00000004
#define SCANDEF_BoundaryDWORD	    0x00000008
#define SCANDEF_ColorBGROrder	    0x00000010
#define SCANDEF_BmpStyle	    	0x00000020
#define SCANDEF_BoundaryWORD		0x00000040
#define SCANDEF_NoMap		    	0x00000080	/* specified this flag will	 */
												/* cause system ignores the	 */
												/* siBrightness & siContrast */
#define SCANDEF_Transparency	    0x00000100	/* Scanning from transparency*/
#define SCANDEF_Negative	    	0x00000200	/* Scanning from negative    */
#define SCANDEF_QualityScan	    	0x00000400	/* Scanning in quality mode  */
#define SCANDEF_BuildBwMap	    	0x00000800	/* Set default map			 */
#define SCANDEF_ContinuousScan      0x00001000
#define SCANDEF_DontBackModule      0x00002000  /* module will not back to   */
												/* home after image scanned  */
#define SCANDEF_RightAlign	    	0x00008000	/* 12-bit					 */

#define SCANDEF_WindowStyle	    	0x00000038
#define SCANDEF_TPA                 (SCANDEF_Transparency | SCANDEF_Negative)

#define SCANDEF_Adf                 0x00020000  /* Scan from ADF tray        */


/* these values will be combined with ScannerInfo.dwFlag */
#define _SCANNER_SCANNING	    	0x8000000
#define _SCANNER_PAPEROUT			0x4000000


/* (3.2): SCANINFO.wMapType */
#define CHANNEL_Master		    0	/* used only MAPINFO.wRGB == RGB_Master	*/
									/* or scan gray							*/
									/* it is illegal when in RGB_RGB mode.  */
#define CHANNEL_RGB		    	1
#define CHANNEL_Default 	    2

/* these are used only to pointer out which color are used as gray map
 * It also pointers out which channel will be used to scan gray data
 * (CHANNEL_Default = Device default)
 */
#define CHANNEL_Red		    	3
#define CHANNEL_Green		    4
#define CHANNEL_Blue		    5

/* (3.3): SCANINFO.wDither */
#define DITHER_CoarseFatting	0
#define DITHER_FineFatting	    1
#define DITHER_Bayer		    2
#define DITHER_VerticalLine	    3
#define DITHER_UserDefine	    4

/* (3.4): SCANINFO.wBits */
#define OUTPUT_8Bits		    0
#define OUTPUT_10Bits		    1
#define OUTPUT_12Bits		    2

/* (3.5): SCANINFO.siBrightness */
#define BRIGHTNESS_Minimum	    -127
#define BRIGHTNESS_Maximum	    127
#define BRIGHTNESS_Default	    0

/* (3.6): SCANINFO.siContrast */
#define CONTRAST_Minimum	    -127
#define CONTRAST_Maximum	    127
#define CONTRAST_Default	    0

/* (3.7): SCANINFO.dwInput */
#define LENS_Current		    0xffff

/******************************************************************************
 *			SECTION 4 - Generic Equates
 */
#define _MEASURE_BASE		300UL

/* for GetLensInformation */
#define SOURCE_Reflection	0
#define SOURCE_Transparency	1
#define SOURCE_Negative 	2
#define SOURCE_ADF          3

/******************************************************************************
 * Section 5 - Scanmodes
 */
#define _ScanMode_Color         0
#define _ScanMode_AverageOut	1	/* CCD averaged 2 pixels value for output*/
#define _ScanMode_Mono			2   /* not color mode						 */

/******************************************************************************
 * Section 6 - additional definitions
 */

/* scan modes */
#define COLOR_BW			0
#define COLOR_HALFTONE		1
#define COLOR_256GRAY		2
#define COLOR_TRUE24		3
#define COLOR_TRUE32		4
#define COLOR_TRUE48		4  /* not sure if this should be the same as 32 */
#define COLOR_TRUE36		5

/* We don't support halftone mode now --> Plustek statement for USB */
#define COLOR_GRAY16		COLOR_HALFTONE


/* IDs the ASIC return */
#define _ASIC_IS_96001		0x0f	/* value for 96001	*/
#define _ASIC_IS_96003		0x10	/* value for 96003  */
#define _ASIC_IS_98001		0x81	/* value for 98001	*/
#define _ASIC_IS_98003		0x83	/* value for 98003	*/
#define _ASIC_IS_USB        0x42    /* for the USB stuff*/

/*
 * transparency/negative mode set ranges
 */
#define _TPAPageWidth		500U			/* org. was 450 = 38.1 mm */
#define _TPAPageHeight		510U 			/* org. was 460 = 38.9 mm */
#define _TPAModeSupportMin	COLOR_TRUE24
#define _TPAModeSupportMax	COLOR_TRUE48
#define _TPAModeSupportDef	COLOR_TRUE24
#define _TPAMinDpi		    150

#define _Transparency48OriginOffsetX	375
#define _Transparency48OriginOffsetY	780

#define _Transparency96OriginOffsetX  	0x03DB  /* org. was 0x0430	*/
#define _Negative96OriginOffsetX	  	0x03F3	/* org. was 0x0428	*/

#define _NegativePageWidth				460U	/* 38.9 mm */
#define _NegativePageHeight	    		350U	/* 29.6 mm */

#define _DEF_DPI		 		 50

/*
 * additional shared stuff between user-world and kernel mode
 */
#define _VAR_NOT_USED(x)	((x)=(x))


/*
 * stuff needed for user space stuff
 */
#ifdef _USER_MODE

int PtDrvInit	  ( int portAddr, int lamp_off, int warm_up );
int PtDrvShutdown ( void );
int PtDrvOpen	  ( void );
int PtDrvClose	  ( void );
int PtDrvIoctl	  ( unsigned int cmd, void *arg );
int PtDrvRead	  ( unsigned char *buffer, int count );

#define _INIT(portAddr,lamp_off,warmup)	PtDrvInit(portAddr,lamp_off,warmup)
#define _DOWN()							PtDrvShutdown()

#define _OPEN(dev)			PtDrvOpen()
#define _CLOSE(hd)			PtDrvClose()
#define _IOCTL(hd,cmd,arg)	PtDrvIoctl(cmd,arg)
#define _READ(hd,buf,len)	PtDrvRead(buf,len)

#else

#define _INIT(portAddr,lamp_off,warmup)
#define _DOWN()

#define _OPEN(dev)			open(dev,O_RDONLY)
#define _CLOSE(hd)			close(hd)
#define _IOCTL(hd,cmd,arg)  ioctl(hd,cmd,arg)
#define _READ(hd,buf,len)	read(hd,buf,len)
#endif

#endif	/* guard __PLUSTEK_SHARE_H__ */

/* END PLUSTEK-SHARE.H.......................................................*/
