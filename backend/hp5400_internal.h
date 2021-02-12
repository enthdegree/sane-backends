#ifndef _HP5400_INTERNAL_H_
#define _HP5400_INTERNAL_H_

/* sane - Scanner Access Now Easy.
   Copyright (C) 2020 Ralph Little <skelband@gmail.com>
   (C) 2003 Thomas Soumarmon <thomas.soumarmon@cogitae.net>
   (c) 2003 Martijn van Oosterhout, kleptog@svana.org
   (c) 2002 Bertrik Sikken, bertrik@zonnet.nl

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
   along with this program.  If not, see <https://www.gnu.org/licenses/>.

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

   HP5400/5470 Test util.
   Currently is only able to read back the scanner version string,
   but this basically demonstrates ability to communicate with the scanner.

   Massively expanded. Can do calibration scan, upload gamma and calibration
   tables and stores the results of a scan. - 19/02/2003 Martijn
*/

#include "../include/_stdint.h"

#ifdef __GNUC__
#define PACKED __attribute__ ((packed))
#else
#define PACKED
#endif

/* If this is enabled, a copy of the raw data from the scanner will be saved to
   imagedebug.dat and the attempted conversion to imagedebug.ppm */
/* #define IMAGE_DEBUG */

/* If this is defined you get extra info on the calibration */
/* #define CALIB_DEBUG */

#define CMD_GETVERSION   0x1200
#define CMD_GETUITEXT    0xf00b
#define CMD_GETCMDID     0xc500
#define CMD_SCANREQUEST  0x2505	/* This is for previews */
#define CMD_SCANREQUEST2 0x2500	/* This is for real scans */
#define CMD_SCANRESPONSE 0x3400
#define CMD_GETSENSORS   0x2000
#define CMD_READPANEL    0x2100 // Reads info from the scanner. BW/Col + Copy Count. Others, not sure.
#define CMD_WRITEPANEL   0x2200 // Ditto for setting.

/* Testing stuff to make it work */
#define CMD_SETDPI       0x1500	/* ??? */
#define CMD_STOPSCAN     0x1B01	/* 0x40 = lamp in on, 0x00 = lamp is off */
#define CMD_STARTSCAN    0x1B05	/* 0x40 = lamp in on, 0x00 = lamp is off */
#define CMD_UNKNOWN      0x2300	/* Send fixed string */
#define CMD_UNKNOWN2     0xD600	/* ??? Set to 0x04 */
#define CMD_UNKNOWN3     0xC000	/* ??? Set to 02 03 03 3C */
#define CMD_SETOFFSET    0xE700	/* two ints in network order. X-offset, Y-offset of full scan */
				  /* Given values seem to be 0x0054 (=4.57mm) and 0x0282 (=54.36mm) */

#define CMD_INITBULK1   0x0087	/* send 0x14 */
#define CMD_INITBULK2   0x0083	/* send 0x24 */
#define CMD_INITBULK3   0x0082	/* transfer length 0xf000 */


struct ScanRequest
{
  uint8_t x1;			/* Set to 0x08 */
  uint16_t dpix, dpiy;		/* Set to 75, 150 or 300 in network order */
  uint16_t offx, offy;		/* Offset to scan, in 1/300th of dpi, in network order */
  uint16_t lenx, leny;		/* Size of scan, in 1/300th of dpi, in network order */
  uint16_t flags1, flags2, flags3;	/* Undetermined flag info */
  /* Known combinations are:
     1st calibration scan: 0x0000, 0x0010, 0x1820  =  24bpp
     2nd calibration scan: 0x0000, 0x0010, 0x3020  =  48bpp ???
     3rd calibration scan: 0x0000, 0x0010, 0x3024  =  48bpp ???
     Preview scan:         0x0080, 0x0000, 0x18E8  =   8bpp
     4th & 5th like 2nd and 3rd
     B&W scan:             0x0080, 0x0040, 0x08E8  =   8bpp
     6th & 7th like 2nd and 3rd
     True colour scan      0x0080, 0x0040, 0x18E8  =  24bpp
   */
  uint8_t zero;			/* Seems to always be zero */
  uint16_t gamma[3];		/* Set to 100 in network order. Gamma? */
  uint16_t pad[3];		/* Zero padding to 32 bytes??? */
}
PACKED;

     /* More known combos (All 24-bit):
        300 x  300 light calibration: 0x0000, 0x0010, 0x1820
        300 x  300 dark calibration:  0x0000, 0x0010, 0x3024
        75 x   75 preview scan:      0x0080, 0x0000, 0x18E8
        300 x  300 full scan:         0x0080, 0x0000, 0x18E8
        600 x  300 light calibration: 0x0000, 0x0010, 0x3000
        600 x  300 dark calibration:  0x0000, 0x0010, 0x3004
        600 x  600 full scan:         0x0080, 0x0000, 0x18C8
        1200 x  300 light calibration: 0x0000, 0x0010, 0x3000
        1200 x  300 dark calibration:  0x0000, 0x0010, 0x3004
        1200 x 1200 full scan:         0x0080, 0x0000, 0x18C8
        2400 x  300 light calibration: 0x0000, 0x0010, 0x3000
        2400 x  300 dark calibration:  0x0000, 0x0010, 0x3004
        2400 x 2400 full scan:         0x0080, 0x0000, 0x18C0
      */

struct ScanResponse
{
  uint16_t x1;                  /* Usually 0x0000 or 0x4000 */
  uint32_t transfersize;        /* Number of bytes to be transferred */
  uint32_t xsize;               /* Shape of returned bitmap */
  uint16_t ysize;               /*   Why does the X get more bytes? */
  uint16_t pad[2];              /* Zero padding to 16 bytes??? */
}
PACKED;

/*
 * Note: this is the structure of the response we get from CMD_READPANEL.
 * We only know about two items for the moment. The rest will be ignored
 * until we understand it better.
 *
 * 44 bytes in total.
 *
 * Since we don't know what the other things mean, I will assume that they
 * mean the same things for Get and SET. For SET, we will have to GET, change
 * what we wish change and SET it back otherwise goodness knows what evil
 * we will unleash.
 *
 * Note that for setting, different values in the buffer seem to apply between the copy count
 * and the colour/BW switch setting. I don't know what that means at the moment.
 *
 * I'm calling it PanelInfo because I can't think of anything better.
 * That may change as the other values are revealed.
 *
 */
struct PanelInfo
{
  uint32_t unknown1[10];
  uint8_t unknown2;
  uint8_t copycount;    // 0..99 from the LCD display.
  uint8_t bwcolour;     // 1 or 2 from the Colour/BW leds.
  uint8_t unknown3;
}
PACKED;

HP5400_SANE_STATIC
int
InitScan2 (enum ScanType type, struct ScanRequest *req,
		      THWParams * pHWParams, struct ScanResponse *res,
		      int iColourOffset, int code);

HP5400_SANE_STATIC
void
FinishScan (THWParams * pHWParams);

HP5400_SANE_STATIC
int
WriteByte (int iHandle, int cmd, char data);

HP5400_SANE_STATIC
int
SetLamp (THWParams * pHWParams, int fLampOn);

HP5400_SANE_STATIC
int
GetSensors (THWParams * pHWParams, uint16_t *sensorMap);

HP5400_SANE_STATIC
int
GetPanelInfo (THWParams * pHWParams, TPanelInfo *panelInfo);

HP5400_SANE_STATIC
int
SetCopyCount(THWParams * pHWParams, SANE_Word copyCount);

HP5400_SANE_STATIC
int
SetColourBW(THWParams * pHWParams, SANE_Word colourBW);

HP5400_SANE_STATIC
int
WarmupLamp (int iHandle);

HP5400_SANE_STATIC
int
SetCalibration (int iHandle, int numPixels,
			   unsigned int *low_vals[3],
			   unsigned int *high_vals[3], int dpi);

HP5400_SANE_STATIC
void
WriteGammaCalibTable (int iHandle, const int *pabGammaR,
				  const int *pabGammaG,
				  const int *pabGammaB);
#ifdef STANDALONE
HP5400_SANE_STATIC
void
SetDefaultGamma (int iHandle);
#endif

HP5400_SANE_STATIC
void
CircBufferInit (int iHandle, TDataPipe * p, int iBytesPerLine,
			    int bpp, int iMisAlignment, int blksize,
			    int iTransferSize);

HP5400_SANE_STATIC
int
CircBufferGetLine (int iHandle, TDataPipe * p, void *pabLine);

HP5400_SANE_STATIC
void
CircBufferExit (TDataPipe * p);

#ifdef STANDALONE
HP5400_SANE_STATIC
void
DecodeImage (FILE * file, int planes, int bpp, int xsize, int ysize,
			 const char *filename);

HP5400_SANE_STATIC
int
hp5400_test_scan_response (struct ScanResponse *resp,
				      struct ScanRequest *req);
#endif


HP5400_SANE_STATIC
int
DoAverageScan (int iHandle, struct ScanRequest *req, int code,
			  unsigned int **array);

#ifdef STANDALONE
HP5400_SANE_STATIC
int
DoScan (int iHandle, struct ScanRequest *req, const char *filename, int code,
		   struct ScanResponse *res);
#endif

HP5400_SANE_STATIC
int
Calibrate (int iHandle, int dpi);

#ifdef STANDALONE
HP5400_SANE_STATIC
int
hp5400_scan (int iHandle, TScanParams * params, THWParams * pHWParams,
			const char *filename);

HP5400_SANE_STATIC
int
PreviewScan (int iHandle);

HP5400_SANE_STATIC
int
InitScanner (int iHandle);
#endif

HP5400_SANE_STATIC
int
InitScan (enum ScanType scantype, TScanParams * pParams,
		     THWParams * pHWParams);

HP5400_SANE_STATIC
void
FinishScan (THWParams * pHWParams);

HP5400_SANE_STATIC
int
HP5400Open (THWParams * params, const char *filename);

HP5400_SANE_STATIC
void
HP5400Close (THWParams * params);

HP5400_SANE_STATIC
int
HP5400Detect (const char *filename,
			 int (*_ReportDevice) (TScannerModel * pModel,
					       const char *pszDeviceName));


HP5400_SANE_STATIC
int
InitHp5400_internal( void );

HP5400_SANE_STATIC
int
FreeHp5400_internal( void );


#ifdef STANDALONE
int
main (int argc, char *argv[]);
#endif

#endif
