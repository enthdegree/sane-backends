/*.............................................................................
 * Project : linux driver for Plustek USB scanners
 *.............................................................................
 * File:	 plustek-usb.h
 *           here are the definitions we need...
 *.............................................................................
 *
 * based on sources acquired from Plustek Inc.
 * Copyright (C) 2001 Gerhard Jaeger <g.jaeger@earthling.net>
 * Last Update:
 *		Gerhard Jaeger <g.jaeger@earthling.net>
 *.............................................................................
 * History:
 * 0.40 - starting version of the USB support
 * 0.41 - added workaround flag to struct DevCaps
 *
 *.............................................................................
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#ifndef __PLUSTEK_USB_H__
#define __PLUSTEK_USB_H__

typedef int Bool;

/* CCD ID (PCB ID): total 3 bits */
#define	kNEC3799	0
#define kSONY518	1
#define	kSONY548	2
#define	kNEC8861	3
#define	kNEC3778	4

/*********************************** plustek_types.h!!! ************************/

/* makes trouble with gcc3
#define _SWAP(x,y)	(x)^=(y)^=(x)^=(y)
*/
#define _SWAP(x,y)	{(x)^=(y); (x)^=((y)^=(x));}

#define _LOWORD(x)	((u_short)(x & 0xffff))
#define _HIWORD(x)	((u_short)(x >> 16))
#define _LOBYTE(x)  ((u_char)((x) & 0xFF))
#define _HIBYTE(x)  ((u_char)((x) >> 8))

#define _HILO2WORD(x)	((u_short)x.bHi * 256U + x.bLo)
#define _PHILO2WORD(x)	((u_short)x->bHi * 256U + x->bLo)

/* useful for RGB-values */
typedef struct {
	u_char Red;
	u_char Green;
	u_char Blue;
} RGBByteDef, *pRGBByteDef;

typedef struct {
	u_short Red;
	u_short Green;
	u_short Blue;
} RGBUShortDef, *pRGBUShortDef;

typedef struct {
	u_long Red;
	u_long Green;
	u_long Blue;
} RGBULongDef, *pRGBULongDef;

typedef struct {
	u_char a_bColor[3];
} ColorByteDef, *pColorByteDef;

typedef struct {
	u_char bHi;
	u_char bLo;
} HiLoDef, *pHiLoDef;

typedef union {
	HiLoDef HiLo[3];
	u_short	Colors[3];
} ColorWordDef, *pColorWordDef;

typedef union {
	HiLoDef	HiLo;
	u_short	Mono;
} MonoWordDef, *pMonoWordDef;

typedef union {

	u_char	     *pb;
	u_short      *pw;
	pMonoWordDef  pmw;
	pColorByteDef pcb;
	pColorWordDef pcw;
	pRGBByteDef   pb_rgb;
	pRGBUShortDef pw_rgb;
	pHiLoDef	  philo;

} AnyPtr, *pAnyPtr;

/*****************************************************************************/

#define IDEAL_GainNormal				0xf000UL		/* 240 */
#define IDEAL_GainPositive				0xfe00UL		/* 254 */
#define IDEAL_Offset					0x1000UL		/* 20  */

#define GAIN_Target						65535UL

#define DRAM_UsedByAsic8BitMode			216				/* in KB */
#define DRAM_UsedByAsic16BitMode		196 /*192*/		/* in KB */


/* ScanParam.bCalibration */
enum _SHADINGID
{
	PARAM_Scan,
	PARAM_Gain,
	PARAM_DarkShading,
	PARAM_WhiteShading,
	PARAM_Offset
};

/* ScanParam.bDataType */
enum _SCANDATATYPE
{
	SCANDATATYPE_BW,
	SCANDATATYPE_Gray,
	SCANDATATYPE_Color
};

/* DCapsDef.bSensorColor */
enum _SENSORCOLOR
{
	SENSORORDER_rgb,
	SENSORORDER_rbg,
	SENSORORDER_gbr,
	SENSORORDER_grb,
	SENSORORDER_brg,
	SENSORORDER_bgr
};

/* DCapsDef.wFlags */
enum _DEVCAPSFLAG
{
	DEVCAPSFLAG_Normal		= 0x0001,
	DEVCAPSFLAG_Positive	= 0x0002,
	DEVCAPSFLAG_Negative	= 0x0004,
	DEVCAPSFLAG_TPA			= 0x0006,
	DEVCAPSFLAG_Adf			= 0x0008
};

enum _WORKAROUNDS
{
	_WAF_NONE               = 0x00000000,   /* no fix anywhere needed        */
	_WAF_BSHIFT7_BUG		= 0x00000001,	/* to fix U12 bug in 14bit mode  */
	_WAF_MISC_IO6_LAMP      = 0x00000002    /* to make EPSON1250 lamp work   */
};

/* Generic usage */
enum _CHANNEL
{
	CHANNEL_red,
	CHANNEL_green,
	CHANNEL_blue,
	CHANNEL_rgb
};

/* motor movement */
enum MODULEMOVE
{
	MOVE_Forward,
	MOVE_Backward,
	MOVE_Both,
	MOVE_ToPaperSensor,
	MOVE_EjectAllPapers,
	MOVE_SkipPaperSensor,
	MOVE_ToShading
};

/* SCANDEF.dwFlags */
enum SCANFLAG
{
	SCANFLAG_bgr			= SCANDEF_ColorBGROrder,
	SCANFLAG_BottomUp		= SCANDEF_BmpStyle,
	SCANFLAG_Invert			= SCANDEF_Inverse,
	SCANFLAG_DWORDBoundary	= SCANDEF_BoundaryDWORD,
	SCANFLAG_RightAlign		= SCANDEF_RightAlign,
	SCANFLAG_StillModule	= SCANDEF_DontBackModule,
/*	SCANFLAG_EnvirOk		= 0x80000000, */
	SCANFLAG_StartScan		= 0x40000000,
	SCANFLAG_Scanning		= 0x20000080,
	SCANFLAG_ThreadActivated= 0x10000200,
	SCANFLAG_Pseudo48		= 0x08000000,
	SCANFLAG_SampleY		= 0x04000000
};

typedef	struct Origins
{
	long lLeft;  /* How many pix to move the scanning org left, in optic res */
	long lUp;	 /* How many pix to move the scanning or up, in optic res    */
} OrgDef, *pOrgDef;

typedef struct SrcAttr
{
	XY     DataOrigin;		/* The origin x is from visible pixel not CCD    */
                            /* pixel 0, in 300 DPI base.                     */
							/* The origin y is from visible top (glass area),*/
                            /* in 300 DPI                                    */
	short  ShadingOriginY;	/* The origin y is from top of scanner body      */
	XY     Size;			/* Scanning width/height, in 300 DPI base.       */
	XY     MinDpi;			/* Minimum dpi supported for scanning            */
	u_char bMinDataType;	/* Minimum data type supports                    */

} SrcAttrDef, *pSrcAttrDef;

typedef struct DevCaps
{
	SrcAttrDef	Normal;			/* Reflection                                */
	SrcAttrDef	Positive;		/* Positive film                             */
	SrcAttrDef	Negative;		/* Negative film                             */
	SrcAttrDef	Adf;			/* Adf device                                */
	XY   		OpticDpi;		/* Maximum DPI                               */
	u_short		wFlags;			/* Flag to indicate what kinds of elements   */
                                /* are available                             */
	u_char		bSensorOrder;	/* CCD color sequences, see _SENSORORDER     */
	u_char		bSensorDistance;/* CCD Color distance                        */
	u_char		bButtons;		/* Number of buttons                         */
	u_char		bCCD;			/* CCD ID                                    */
	u_char		bPCB;			/* PCB ID                                    */
	u_long		workaroundFlag;	/* Flag to allow special work arounds, see   */
	                            /* _WORKAROUNDS                              */

} DCapsDef, *pDCapsDef;

/*
 * TODO: strip down non-used stuff
 */
typedef enum
{
	MODEL_KaoHsiung,
	MODEL_HuaLien,
	MODEL_Tokyo600
} eModelDef;

typedef struct HWDefault
{
	double				dMaxMotorSpeed;			/* Inches/second, max. scan speed */
	double				dMaxMoveSpeed;			/* Inches/second, max. move speed */
	u_short				wIntegrationTimeLowLamp;
	u_short				wIntegrationTimeHighLamp;
	u_short				wMotorDpi;				/* Full step DPI */
	u_short				wDRAMSize;				/* in KB         */
	u_short				wMinIntegrationTimeLowres;
												/* in ms.        */
	u_short				wMinIntegrationTimeHighres;
												/* in ms.        */
	u_short				wGreenPWMDutyCycleLow;
	u_short				wGreenPWMDutyCycleHigh;
	/* Registers */
	u_char				bSensorConfiguration;	/* 0x0b */
	/* Sensor control settings */
	u_char				bReg_0x0c;
	u_char				bReg_0x0d;
	u_char				bReg_0x0e;
	u_char				bReg_0x0f_Mono [10];	/* 0x0f to 0x18 */
	u_char				bReg_0x0f_Color [10];	/* 0x0f to 0x18 */
	
	/* 0x1a & 0x1b, remember the u_char order is not Intel
	 * format, you have to pay your attention when you
	 * write this value to register.
	 */
	u_short				StepperPhaseCorrection;	
	
	/* Sensor Pixel Configuration
	 * Actually, the wActivePixelsStart will be set to 0 for shading purpose.
     * We have to keep these values to adjust the origins when user does the
     * scan. These settings are based on optic resolution.
     */
	u_char				bOpticBlackStart;		/* 0x1c        */
	u_char				bOpticBlackEnd;			/* 0x1d        */
	u_short				wActivePixelsStart;		/* 0x1e & 0x1f */
	u_short				wLineEnd;				/* 0x20 & 0x21 */
	/* Misc */
	u_char				bReg_0x45;
	u_short				wStepsAfterPaperSensor2;/* 0x4c & 0x4d */
	u_char				bReg_0x51;
	u_char				bReg_0x54;
	u_char				bReg_0x55;
	u_char				bReg_0x56;
	u_char				bReg_0x57;
	u_char				bReg_0x58;
	u_char				bReg_0x59;
	u_char				bReg_0x5a;
	u_char				bReg_0x5b;
	u_char				bReg_0x5c;
	u_char				bReg_0x5d;
	u_char				bReg_0x5e;
	
	/* To identify LM9831 */
	Bool				fLM9831;
	/* To identify Model */
    eModelDef			ScannerModel;
} HWDef, *pHWDef;

/*
 *
 */
typedef struct DeviceDef
{
	char*       ModelStr;        /* pointer to our model string              */
	DCapsDef    Caps;			 /* pointer to the attribute of current dev  */
	HWDef       HwSetting;	     /* Pointer to the characteristics of device */
	pSrcAttrDef pSource;		 /* Scanning src, it's equal to Caps.Normal  */
							     /* on the source that the user specified.   */
	OrgDef	    Normal;		     /* Reflection - Pix to adjust scanning orgs */
	OrgDef	    Positive;		 /* Pos film - Pix to adjust scanning orgs   */
	OrgDef	    Negative;		 /* Neg film - Pix to adjust scanning orgs   */
	OrgDef	    Adf;			 /* Adf - Pixels to adjust scanning origins  */
	u_long	    dwWarmup;		 /* Ticks to wait for lamp stable, in ms.    */
	u_long	    dwTicksLampOn;   /* The ticks when lamp turns on             */
	u_long	    dwLampOnPeriod;  /* How many seconds to keep lamp on         */
	SANE_Bool	bLampOffOnEnd;   /* switch lamp off on end or keep cur. state*/
	u_char	    bCurrentLamp;	 /* The lamp ID                              */
	u_char	    bLastColor;	     /* The last color channel comes from CCD    */
	u_char	    bStepsToReverse; /* reg 0x50, this value is from registry    */
	u_long      dwBufferSize;    /*                                          */

} DeviceDef, *pDeviceDef;


typedef struct Settings
{
    char      *pIDString;
	pDCapsDef  pDevCaps;
	pHWDef     pHwDef;
	char      *pModelString;

} SetDef, *pSetDef;

typedef struct
{
	/* User Information */
	u_long dwBytes;
	u_long dwPixels;
	u_long dwLines;

	/* Driver Info */
	u_long dwValidPixels; /* only valid pixels, not incl. pad pix (B/W, Gray)*/
	u_long dwPhyPixels;	  /* inlcude pad pixels for ASIC (B/W, Gray)         */
	u_long dwPhyBytes;
	u_long dwPhyLines;	  /* should include the extra lines accord to the    */
                          /* request dpi (CCD lines distance)                */
	u_long dwTotalBytes;  /* Total bytes per scan                            */
} WinInfo, *pWinInfo;

/*
 *
 */
typedef struct
{
	/* OUTPUT - Driver returned area. All are based on physical
     * scanning conditions.
     */
	WinInfo	Size;			/* i/p:
							 * dwPixels, dwBytes(without u_long boundary factor)
                             * dwLines in user specified dpi
							 * o/p:
							 * dwPhyPixels, dwPhyBytes, dwPhyLines
							 * so after called, caller have to change i
                             */
	XY      PhyDpi;			/* Driver DPI */

	/* INPUT - User info. All sizes and coordinates are specified in the
     * unit based on 300 DPI
     */
	XY      UserDpi;		/* User specified DPI                            */
	XY   	Origin;			/* Scanning origin in optic dpi                  */
	double	dMCLK;			/* for positive & negative & Adf                 */
	short	brightness;		
	short	contrast;		
	short	siThreshold;	/* only for B/W output                           */
	u_char	bSource;		/* Reflection/Positive/Negative/Adf (SOURCE_xxx) */
	u_char	bDataType;		/* Bw, Gray or Color (see _SCANDATATYPE)         */
	u_char	bBitDepth;		/* 1/8/14                                        */
	u_char	bChannels;		/* Color or Gray                                 */
	u_char  bCalibration;	/* 1 or 2: the origin.x is from CCD pixel 0 and the
							 *		   the origin.y is from Top of scanner.
							 * 		   In this case, the WININFO.dwPhyLines
                             *         will not included the extra lines for
                             *         color distance factor.
							 * 0: normal scan, the both directions have to
                             *    add the distance
                             */
	int		swOffset[3];
	int	    swGain[3];
} ScanParam, *pScanParam;

struct Plustek_Device;

/*
 *
 */
typedef struct ScanDef
{
    /*
     * from calibration...
     */
    Bool                fCalibrated;

    /*
     * the other stuff...
     */
	u_long				dwFlag;

	ScanParam			sParam;

	/* User buffer */
	AnyPtr 				UserBuf;
	u_long				dwLinesUser;	/* Number of lines of user buffer */
	u_long				dwBytesLine;	/* Bytes per line of user buffer. */

	void (*pfnProcess)(struct Plustek_Device*);/* Image process routine */

	/* Scan buffer */
	u_char*				pScanBuffer;
	u_long				dwTotalBytes;	/* Total bytes of image */
	u_long				dwLinesPerScanBufs;
	u_long				dwNumberOfScanBufs;

	u_char*				pbScanBufBegin;
	u_char*				pbScanBufEnd;
	u_char*             pbGetDataBuf;
	u_long				dwBytesScanBuf;
	u_long				dwLinesScanBuf;
	u_long				dwLinesDiscard;

	u_long				dwRedShift;
	u_long				dwGreenShift;
	u_long				dwBlueShift;

	AnyPtr				Green;
	AnyPtr				Red;
	AnyPtr				Blue;
	
	long				lBufAdjust;		/* bytes to	adjust buffer pointer */
										/* after a image line processed   */
	u_short				wSumY;			/* for lines sampling */
	
	u_char				bLineDistance;	/* Color offset in specific dpi y  */
	int					fGrayFromColor;

	u_char				bLinesToSkip;

} ScanDef, *pScanDef;


/*** some error codes... ****/

#define _E_LAMP_NOT_IN_POS	-9600
#define _E_LAMP_NOT_STABLE	-9601
#define _E_INTERNAL         -9610
#define _E_ALLOC            -9611
#define _E_NODATA           -9620
#define _E_BUFFER_TOO_SMALL -9621
#define _E_DATAREAD         -9630

#endif	/* guard __PLUSTEK_USB_H__ */

/* END PLUSTEK_USB.H ........................................................*/
