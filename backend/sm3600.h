/* sane - Scanner Access Now Easy.
   Copyright (C) 1996, 1997 David Mosberger-Tang
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

   This file implements a dynamic linking based SANE meta backend.  It
   allows managing an arbitrary number of SANE backends by using
   dynamic linking to load backends on demand.  */
#ifndef _H_SM3600
#define _H_SM3600

/* ======================================================================

sm3600.h

SANE backend master module.

Definitions ported from "scantool.h" 5.4.2001.

(C) Marian Matthias Eichholz 2001

Start: 2.4.2001

====================================================================== */

#define DEBUG_SCAN     0x0001
#define DEBUG_COMM     0x0002
#define DEBUG_ORIG     0x0004
#define DEBUG_BASE     0x0011
#define DEBUG_DEVSCAN  0x0012
#define DEBUG_REPLAY   0x0014
#define DEBUG_BUFFER   0x0018
#define DEBUG_SIGNALS  0x0020

#define DEBUG_CRITICAL 1
#define DEBUG_VERBOSE  2
#define DEBUG_INFO     3
#define DEBUG_JUNK     5

#define USB_TIMEOUT_JIFFIES  2000

#define SCANNER_VENDOR     0x05DA

/* ====================================================================== */

typedef enum { false, true } TBool;

typedef SANE_Status TState;

typedef struct {
  int           xMargin; /* in 1/600 inch */
  int           yMargin; /* in 1/600 inch */
  unsigned char nHoleGray;
  unsigned char nBarGray;
  long          rgbBias;
} TCalibration;

typedef struct {
  int x;
  int y;
  int cx;
  int cy;
  int res; /* like all parameters in 1/1200 inch */
  int nBrightness; /* -255 ... 255 */
  int nContrast;   /* -128 ... 127 */
} TScanParam;

typedef enum { fast, high, best } TQuality;
typedef enum { color, gray, line, halftone } TMode;

#define INST_ASSERT() { if (this->nErrorState) return this->nErrorState; }
#define CHECK_POINTER(p) \
if (!p) return SetError(this,SANE_STATUS_NO_MEM,"memory failed in %d",__LINE__)

#define dprintf debug_printf

typedef struct TInstance *PTInstance;
typedef TState (*TReadLineCB)(PTInstance);

typedef struct TScanState {
  TBool           bEOF;         /* EOF marker for sane_read */
  TBool           bCanceled;
  TBool           bScanning;    /* block is active? */
  TBool           bLastBulk;    /* EOF announced */
  int             iReadPos;     /* read() interface */
  int             iBulkReadPos; /* bulk read pos */
  int             iLine;        /* log no. line */
  int             cchBulk;      /* available bytes in bulk buffer */
  int             cchLineOut;   /* buffer size */
  int             cxPixel,cyPixel; /* real pixel */
  int             cxMax;        /* uninterpolated in real pixels */
  int             cxWindow;     /* Window with in 600 DPI */
  int             cyWindow;     /* Path length in 600 DPI */
  int             cyTotalPath;  /* from bed start to window end in 600 dpi */
  int             nFixAspect;   /* aspect ratio in percent, 75-100 */
  int             cBacklog;     /* depth of ppchLines */
  int             ySensorSkew;  /* distance in pixel between sensors */
  char           *szOrder;      /* 123 or 231 or whatever */
  unsigned char  *pchBuf;       /* bulk transfer buffer */
  short         **ppchLines;    /* for error diffusion and color corr. */
  unsigned char  *pchLineOut;   /* read() interface */
  TReadLineCB     ReadProc;     /* line getter callback */
} TScanState;


#ifndef INSANE_VERSION

#define NUM_OPTIONS 18

typedef union
  {  
    SANE_Word w;
    SANE_Word *wa;              /* word array */
    SANE_String s;
  }
TOptionValue;

typedef struct TDevice {
  struct TDevice        *pNext;
  struct usb_device     *pdev;
  SANE_Device            sane;
} TDevice;

#endif

typedef struct TInstance {
#ifndef INSANE_VERSION
  struct TInstance         *pNext;
  SANE_Option_Descriptor    aoptDesc[NUM_OPTIONS];
  TOptionValue              aoptVal[NUM_OPTIONS];
#endif
  SANE_Int           agammaGray[4096];
  SANE_Int           agammaR[4096];
  SANE_Int           agammaG[4096];
  SANE_Int           agammaB[4096];
  TScanState         state;
  TCalibration       calibration;
  TState             nErrorState;
  char              *szErrorReason;
  TBool              bSANE;
  TScanParam         param;
  TBool              bWriteRaw;
  TBool              bVerbose;
  TBool              bOptSkipOriginate;
  TQuality           quality;
  TMode              mode;
  usb_dev_handle    *hScanner;
  FILE              *fhLog;
  FILE              *fhScan;
} TInstance;

#define TRUE  1
#define FALSE 0

/* ====================================================================== */

#define ERR_FAILED -1
#define OK         0

#define NUM_SCANREGS      74

/* ====================================================================== */

#define R_ALL    0x01

/* have to become an enumeration */

typedef enum { none, hpos, hposH, hres } TRegIndex;

/* WORD */
#define R_SPOS   0x01
#define R_XRES   0x03
/* WORD */
#define R_SWID   0x04
/* WORD */
#define R_STPS   0x06
/* WORD */
#define R_YRES   0x08
/* WORD */
#define R_SLEN   0x0A
/* WORD*/
#define R_INIT   0x12
#define RVAL_INIT 0x1540
/* RGB */
#define R_CCAL   0x2F

/* WORD */
#define R_CSTAT  0x42
#define R_CTL    0x46
/* WORD */
#define R_POS    0x52
/* WORD */
#define R_LMP    0x44
#define R_QLTY   0x4A
#define R_STAT   0x54

#define LEN_MAGIC   0x24EA

/* ====================================================================== */
#define USB_CHUNK_SIZE 0x8000

/* scanutil.c */
static int SetError(TInstance *this, int nError, const char *szFormat, ...);
static void debug_printf(unsigned long ulType, const char *szFormat, ...);
static void DumpBuffer(FILE *fh, const char *pch, int cch);
static void FixExposure(unsigned char *pchBuf,
			int cchBulk,
			int nBrightness,
			int nContrast);
static TState FreeState(TInstance *this, TState nReturn);
static TState EndScan(TInstance *this);
static TState ReadChunk(TInstance *this, unsigned char *achOut,
			int cchMax, int *pcchRead);
static TState DoScanFile(TInstance *this);
static void   GetAreaSize(TInstance *this);
static TState InitGammaTables(TInstance *this);
static TState CancelScan(TInstance *this);

/* scanmtek.c */
extern unsigned short aidProduct[];
static TState DoInit(TInstance *this);
static TState DoReset(TInstance *this);
static TState WaitWhileBusy(TInstance *this,int cSecs);
static TState WaitWhileScanning(TInstance *this,int cSecs);
static TState DoJog(TInstance *this,int nDistance);
static TState DoLampSwitch(TInstance *this,int nPattern);
static TState DoCalibration(TInstance *this);
static TState UploadGammaTable(TInstance *this, int iByteAddress, SANE_Int *pnGamma);

/* scanusb.c */
static TState RegWrite(TInstance *this,int iRegister, int cb, unsigned long ulValue);
static TState RegWriteArray(TInstance *this,int iRegister, int cb, 
			    unsigned char *pchBuffer);
static TState RegCheck(TInstance *this,int iRegister, int cch, unsigned long ulValue);
static int BulkRead(TInstance *this,FILE *fhOut, unsigned int cchBulk);
static int BulkReadBuffer(TInstance *this,unsigned char *puchBufferOut,
			  unsigned int cchBulk); /* gives count */
static unsigned int RegRead(TInstance *this,int iRegister, int cch);
static TState MemReadArray(TInstance *this, int iAddress, int cb,
			   unsigned char *pchBuffer);
static TState MemWriteArray(TInstance *this, int iAddress, int cb,
			    unsigned char *pchBuffer);

/* gray.c */
static TState StartScanGray(TInstance *this);
/* color.c */
static TState StartScanColor(TInstance *this);

/* homerun.c */
static TState DoOriginate(TInstance *this, TBool bStepOut);

/* ====================================================================== */

#endif
