/*.............................................................................
 * Project : SANE library for Plustek flatbed scanners.
 *.............................................................................
 */

/** @file plustek-usb.h
 *  @brief Main defines for the USB devices.
 *
 * Based on sources acquired from Plustek Inc.<br>
 * Copyright (C) 2001-2002 Gerhard Jaeger <gerhard@gjaeger.de>
 *
 * History:
 * - 0.40 - starting version of the USB support
 * - 0.41 - added workaround flag to struct DevCaps
 * - 0.42 - added MODEL_NOPLUSTEK
 *        - replaced fLM9831 by chip (valid entries: _LM9831, _LM9832, _LM9833)
 *        - added _WAF_MISC_IO3_LAMP for UMAX 3400
 * - 0.43 - added _WAF_MISC_IOx_LAMP (x=1,2,4,5)
 *        - added CLKDef
 * - 0.44 - added vendor and product ID to struct DeviceDef
 *        - added _WAF_BYPASS_CALIBRATION
 *		  - added _WAF_INV_NEGATIVE_MAP
 * - 0.45 - added _WAF_SKIP_FINE for skipping fine calibration
 *          added _WAF_SKIP_WHITEFINE for skipping fine white calibration
 *          added MCLK setting for 16 bit modes
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
#ifndef __PLUSTEK_USB_H__
#define __PLUSTEK_USB_H__

/** CCD ID (PCB ID): total 3 bits */
#define	kNEC3799	0
#define kSONY518	1
#define	kSONY548	2
#define	kNEC8861	3
#define	kNEC3778	4
#define	kNECSLIM	5

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

/** Chip-types */
typedef enum _CHIPSET
{
	_LM9831,
	_LM9832,
	_LM9833
} eChipDef;

/** ScanParam.bCalibration */
enum _SHADINGID
{
	PARAM_Scan,
	PARAM_Gain,
	PARAM_DarkShading,
	PARAM_WhiteShading,
	PARAM_Offset
};

/** ScanParam.bDataType */
enum _SCANDATATYPE
{
	SCANDATATYPE_BW,
	SCANDATATYPE_Gray,
	SCANDATATYPE_Color
};

/** DCapsDef.bSensorColor */
enum _SENSORCOLOR
{
	SENSORORDER_rgb,
	SENSORORDER_rbg,
	SENSORORDER_gbr,
	SENSORORDER_grb,
	SENSORORDER_brg,
	SENSORORDER_bgr
};

/** DCapsDef.wFlags */
enum _DEVCAPSFLAG
{
	DEVCAPSFLAG_Normal		= 0x0001,
	DEVCAPSFLAG_Positive	= 0x0002,
	DEVCAPSFLAG_Negative	= 0x0004,
	DEVCAPSFLAG_TPA			= 0x0006,
	DEVCAPSFLAG_Adf			= 0x0008
};

/** to allow some workarounds */
enum _WORKAROUNDS
{
	_WAF_NONE               = 0x00000000, /* no fix anywhere needed          */
	_WAF_BSHIFT7_BUG		= 0x00000001, /* to fix U12 bug in 14bit mode    */
	_WAF_MISC_IO_LAMPS      = 0x00000002, /* special lamp switching          */
	_WAF_BLACKFINE          = 0x00000004, /* use black calibration strip     */
	_WAF_BYPASS_CALIBRATION = 0x00000008, /* no calibration,use linear gamma */
	_WAF_INV_NEGATIVE_MAP   = 0x00000010, /* the backend does the neg. stuff */
	_WAF_SKIP_FINE          = 0x00000020, /* skip the fine calbration        */
	_WAF_SKIP_WHITEFINE     = 0x00000040  /* skip the fine white calbration  */
};

/** for lamps connected to the misc I/O pins*/
enum _LAMPS
{
	_NO_MIO = 0,
	_MIO1   = 0x0001,
	_MIO2   = 0x0002,
	_MIO3   = 0x0004,
	_MIO4   = 0x0008,
	_MIO5   = 0x0010,
	_MIO6   = 0x0020
};

/** for encoding a misc I/O register as TPA */
#define _TPA(register)          ((u_long)(register << 16))

/** Mask to check for available TPA */
#define _HAS_TPA(flag)		(flag & 0xFFFF0000)

/** Get the TPA misc I/O register */
#define _GET_TPALAMP(flag)	(flag >> 16)

/** motor types */
typedef enum
{
	MODEL_KaoHsiung = 0,
	MODEL_HuaLien,
	MODEL_Tokyo600,
	MODEL_NOPLUSTEK_600,  /**< for 600 dpi models   */
	MODEL_NOPLUSTEK_1200, /**< for 1200 dpi models  */
	MODEL_EPSON,          /**< for EPSON1250/1260   */
	MODEL_MUSTEK600,      /**< for BearPaw 1200     */
	MODEL_MUSTEK1200,     /**< for BearPaw 2400     */
	MODEL_HP,             /**< for HP2x00           */
	MODEL_CANON650 ,      /**< for Canon N650U/656U */
	MODEL_CANON1220,      /**< for Canon N1220U     */
	MODEL_CANON670 ,      /**< for Canon N670U/676U */
	MODEL_CANON1240,      /**< for Canon N1240U     */
	MODEL_UMAX,           /**< for UMAX 3400/3450   */
	MODEL_LAST
} eModelDef;

/** to distinguish between Plustek and other devices */
#define _IS_PLUSTEKMOTOR(x) (x<=MODEL_Tokyo600)

/** Generic usage */
enum _CHANNEL
{
	CHANNEL_red,
	CHANNEL_green,
	CHANNEL_blue,
	CHANNEL_rgb
};

/** motor movement */
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

/** SCANDEF.dwFlags */
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
	XY     DataOrigin;		/**< The origin x is from visible pixel not CCD  */
                            /*   pixel 0, in 300 DPI base.                   */
							/*   The origin y is from visible top            */
                            /*  (glass area), in 300 DPI                     */
	short  ShadingOriginY;	/**< The origin y is from top of scanner body    */
	short  DarkShadOrgY;    /**< if the device has a dark calibration strip  */
	XY     Size;			/**< Scanning width/height, in 300 DPI base.     */
	XY     MinDpi;			/**< Minimum dpi supported for scanning          */
	u_char bMinDataType;	/**< Minimum data type supports                  */

} SrcAttrDef, *pSrcAttrDef;

typedef struct DevCaps
{
	SrcAttrDef	Normal;			/**< Reflection                              */
	SrcAttrDef	Positive;		/**< Positive film                           */
	SrcAttrDef	Negative;		/**< Negative film                           */
	SrcAttrDef	Adf;			/**< Adf device                              */
	XY   		OpticDpi;		/**< Maximum DPI                             */
	u_short		wFlags;			/**< Flag to indicate what kinds of elements */
                                /*   are available                           */
	u_char		bSensorOrder;	/**< CCD color sequences, see _SENSORORDER   */
	u_char		bSensorDistance;/**< CCD Color distance                      */
	u_char		bButtons;		/**< Number of buttons                       */
	u_char		bCCD;			/**< CCD ID                                  */
	u_char		bPCB;			/**< PCB ID                                  */
	u_long		workaroundFlag;	/**< Flag to allow special work arounds, see */
	                            /*   _WORKAROUNDS                            */
	u_long      lamp;           /**< for lamp: loword: normal, hiword: tpa   */

} DCapsDef, *pDCapsDef;

/**
 * for keeping intial illumination settings
 */
typedef struct
{
	u_char  mode;

	u_short red_lamp_on;
	u_short red_lamp_off;
	u_short green_lamp_on;
	u_short green_lamp_off;
	u_short blue_lamp_on;
	u_short blue_lamp_off;

} IllumiDef, *pIllumiDef;


/** basic register settings
 */
typedef struct HWDefault
{
	double				dMaxMotorSpeed;			/* Inches/second, max. scan speed */
	double				dMaxMoveSpeed;			/* Inches/second, max. move speed */
	double				dIntegrationTimeLowLamp;
	double				dIntegrationTimeHighLamp;
	u_short				wMotorDpi;				/* Full step DPI */
	u_short				wDRAMSize;				/* in KB         */
	double				dMinIntegrationTimeLowres;
												/* in ms.        */
	double				dMinIntegrationTimeHighres;
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

	/* color mode settings */	
	u_char              bReg_0x26;
	u_char              bReg_0x27;
	
	/* illumination mode reg 0x29 (runtime) */
	u_char              bReg_0x29;

	/* initial illumination settings */
	IllumiDef           illu_mono;
	IllumiDef           illu_color;
	
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
	
	/* illumination settings (runtime) */
	u_short             red_lamp_on;			/* 0x2c & 0x2d */
	u_short             red_lamp_off;			/* 0x2e & 0x2f */
	u_short             green_lamp_on;			/* 0x30 & 0x31 */
	u_short             green_lamp_off;			/* 0x32 & 0x33 */
	u_short             blue_lamp_on;			/* 0x34 & 0x35 */
	u_short             blue_lamp_off;			/* 0x36 & 0x37 */
	
	/* Misc */
	u_char				bReg_0x45;
	u_short				wStepsAfterPaperSensor2;/* 0x4c & 0x4d */
	u_char              bStepsToReverse;        /* 0x50        */
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
	
	eChipDef            chip;           /* chiptype               */
    eModelDef			motorModel; 	/* to identify used motor */

} HWDef, *pHWDef;

/** device description during runtime
 */
typedef struct DeviceDef
{
	char*       ModelStr;      /**< pointer to our model string              */
	int         vendor;        /**< vendor ID                                */
	int         product;       /**< product ID                               */
	DCapsDef    Caps;          /**< pointer to the attribute of current dev  */
	HWDef       HwSetting;     /**< Pointer to the characteristics of device */
	pSrcAttrDef pSource;       /**< Scanning src, it's equal to Caps.Normal  */
							   /**< on the source that the user specified.   */
	OrgDef	    Normal;        /**< Reflection - Pix to adjust scanning orgs */
	OrgDef	    Positive;      /**< Pos film - Pix to adjust scanning orgs   */
	OrgDef	    Negative;      /**< Neg film - Pix to adjust scanning orgs   */
	OrgDef	    Adf;           /**< Adf - Pixels to adjust scanning origins  */
	u_long	    dwWarmup;      /**< Ticks to wait for lamp stable, in ms.    */
	u_long	    dwTicksLampOn; /**< The ticks when lamp turns on             */
	u_long	    dwLampOnPeriod;/**< How many seconds to keep lamp on         */
	SANE_Bool	bLampOffOnEnd; /**< switch lamp off on end or keep cur. state*/
	int		    currentLamp;   /**< The lamp ID of the currently used lamp   */

} DeviceDef, *pDeviceDef;


typedef struct Settings
{
    char      *pIDString;
	pDCapsDef  pDevCaps;
	pHWDef     pHwDef;
	char      *pModelString;

} SetDef, *pSetDef;

/**
 */
typedef struct
{
	/** User Information */
	u_long dwBytes;       /**< bytes per line  */
	u_long dwPixels;      /**< pixels per line */
	u_long dwLines;       /**< lines           */

	/** Driver Info */
	u_long dwValidPixels; /**< only valid pixels, not incl. pad pix(B/W,Gray)*/
	u_long dwPhyPixels;	  /**< inlcude pad pixels for ASIC (B/W, Gray)       */
	u_long dwPhyBytes;    /**< bytes to read from ASIC                       */
	u_long dwPhyLines;	  /**< should include the extra lines accord to the  */
                          /*   request dpi (CCD lines distance)              */
	u_long dwTotalBytes;  /**< Total bytes per scan                          */

} WinInfo, *pWinInfo;

/**
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
	XY      UserDpi;		/**< User specified DPI                          */
	XY   	Origin;			/**> Scanning origin in optic dpi                */
	double	dMCLK;			/**< for positive & negative & Adf               */
	short	brightness;		
	short	contrast;		
	short	siThreshold;	/**< only for B/W output                         */
	u_char	bSource;		/**< Reflection/Positive/Negative/Adf(SOURCE_xxx)*/
	u_char	bDataType;		/**< Bw, Gray or Color (see _SCANDATATYPE)       */
	u_char	bBitDepth;		/**< 1/8/14                                      */
	u_char	bChannels;		/**< Color or Gray                               */
	u_char  bCalibration;	/**< 1 or 2: the origin.x is from CCD pixel 0 and
							 *		   the origin.y is from Top of scanner.
							 * 		   In this case, the WININFO.dwPhyLines
                             *         will not included the extra lines for
                             *         color distance factor.
							 * 0: normal scan, the both directions have to
                             *    add the distance
                             */
	int		swOffset[3];	/**< for calibration adjustment                  */
	int	    swGain[3];      /**< for calibration adjustment                  */

} ScanParam, *pScanParam;

struct Plustek_Device;

/** structure to hold all necessary buffer informations for current scan
 */
typedef struct ScanDef
{
    SANE_Bool           fCalibrated;    /**< calibrated or not              */
	u_long				dwFlag;         /**< scan attributes                */

	ScanParam			sParam;         /**< all we need to scan            */

	AnyPtr 				UserBuf;        /**< pointer to the user buffer     */
	u_long				dwLinesUser;	/**< Number of lines of user buffer */
	u_long				dwBytesLine;	/**< Bytes per line of user buffer. */

	/** Image processing routine according to the scan mode  */
	void (*pfnProcess)(struct Plustek_Device*);

	u_char*				pScanBuffer;	/**< our scan buffer */

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
	
	long				lBufAdjust;		/**< bytes to adjust buffer pointer  */
										/*   after a image line processed    */
	u_short				wSumY;			/**<  for line sampling              */
	
	u_char				bLineDistance;	/**< Color offset in specific dpi y  */
	int					fGrayFromColor; /**< channel to use for gray mode    */

	u_char				bLinesToSkip;	/**< how many lines to skip at start */

} ScanDef, *pScanDef;


/** max number of different colck settings */
#define _MAX_CLK	10

/** structure to hold PWN settings
 */
typedef struct
{
	u_char pwm;
	u_char pwm_duty;

} MDef, *pMDef;

/** array used to get motor-settings and mclk-settings
 */
static int dpi_ranges[] = {	75,100,150,200,300,400,600,800,1200,2400 };

/** according to the CCD and motor, we provide various settings
 */
typedef struct {

	eModelDef motorModel;	/**< the motor ID */

	u_char pwm_fast;		/**< PWM during fast movement      */
	u_char pwm_duty_fast;   /**< PWM duty during fast movement */
	u_char mclk_fast;       /**< MCLK during fast movement     */

    /**
     * here we define some ranges for better supporting
     * non-Plustek devices with it's different hardware
     * we can set the MCLK and the motor PWM stuff for color
     * and gray modes (8bit and 14/16bit modes)
     *    0    1     2     3     4     5     6      7     8      9
     * <= 75 <=100 <=150 <=200 <=300 <=400 <=600 <= 800 <=1200 <=2400DPI
     */
	MDef   motor_sets[_MAX_CLK];	/**< motor PWM settings during scan      */
	double color_mclk_8[_MAX_CLK];  /**< MCLK settings for color scan        */
	double color_mclk_16[_MAX_CLK]; /**< MCLK settings for color (16bit) scan*/
	double gray_mclk_8[_MAX_CLK];   /**< MCLK settings for gray scan         */
	double gray_mclk_16[_MAX_CLK];  /**< MCLK settings for gray (16bit) scan */

} ClkMotorDef, *pClkMotorDef;

#endif /* guard __PLUSTEK_USB_H__ */

/* END PLUSTEK-USB.H ........................................................*/
