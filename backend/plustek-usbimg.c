/*.............................................................................
 * Project : SANE library for Plustek flatbed scanners.
 *.............................................................................
 */

/** @file plustek-usbimg.c
 *  @brief Image processing functions for copying and scaling image lines.
 *
 * Based on sources acquired from Plustek Inc.<br>
 * Copyright (C) 2001-2002 Gerhard Jaeger <gerhard@gjaeger.de>
 *
 * History:
 * - 0.40 - starting version of the USB support
 * - 0.41 - fixed the 14bit problem for LM9831 devices
 * - 0.42 - no changes
 * - 0.43 - no changes
 * - 0.44 - added CIS parts and dumpPic function
 * - 0.45 - added gray scaling functions for CIS devices
 *        - fixed usb_GrayScale16 function
 *        - fixed a bug in usb_ColorScale16_2 function
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

#define _SCALER		1000

static u_char   bShift, Shift;
static u_char  *pbSrce, *pbDest;
static int	    iNext;
static u_long   dwPixels, dwBitsPut;
static u_short	wSum, Mask;
static u_short *pwDest;
static u_short	wR, wG, wB;
static pHiLoDef pwm;

/*
 *
 */
static u_char BitsReverseTable[256] =
{
	0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0, 0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
	0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8, 0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
	0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4, 0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
	0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec, 0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
	0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2, 0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
	0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea, 0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
	0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6, 0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
	0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee, 0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
	0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1, 0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
	0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9, 0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
	0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5, 0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
	0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed, 0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
	0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3, 0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
	0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb, 0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
	0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7, 0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
	0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef, 0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff
};
#if 0
inline void ReverseBits(int b, u_charPBYTE &pTar, int &iByte, int &iWeightSum,
						int iSrcWeight = 0, int iTarWeight = 0, int cMax = 8);
#endif

static void ReverseBits( int b, u_char **pTar, int *iByte, int *iWeightSum,
						 int iSrcWeight, int iTarWeight, int cMax )
{
	int bit;

	cMax = (1 << cMax);
	if(iSrcWeight == iTarWeight)
	{
		for( bit = 1; bit < cMax; bit <<= 1) {
			*iByte <<= 1;
			if(b & bit)
				*iByte |= 1;
			if(*iByte >= 0x100)	{
				**pTar++ = (u_char)*iByte;
				*iByte = 1;
			}
		}
	}
	else
	{
		for( bit = 1; bit < cMax; bit <<= 1 ) {

			*iWeightSum += iTarWeight;
			while(*iWeightSum >= iSrcWeight)
			{
				*iWeightSum -= iSrcWeight;
				*iByte <<= 1;
				if(b & bit)
					*iByte |= 1;
				if(*iByte >= 0x100)
				{
					**pTar++ = (u_char)*iByte;
					*iByte = 1;
				}
			}
		}
	}
}
#if 0
void ReverseBitStream(PBYTE pSrc, PBYTE pTar, int iPixels, int iBufSize,
  					  int iSrcWeight = 0, int iTarWeight = 0, int iPadBit = 1);
#endif
static void usb_ReverseBitStream( u_char *pSrc, u_char *pTar, int iPixels,
                                  int iBufSize, int iSrcWeight/* = 0*/,
                                  int iTarWeight/* = 0*/, int iPadBit/* = 1*/)
{
	int i;
	int iByte = 1;
	int cBytes = iPixels / 8;
	int cBits = iPixels % 8;
	u_char bPad = iPadBit? 0xff: 0;
	u_char *pTarget = pTar;
	int iWeightSum = 0;

	if(iSrcWeight == iTarWeight)
	{
		if(cBits)
		{
			int cShift = 8 - cBits;
			for(i = cBytes, pSrc = pSrc + cBytes - 1; i > 0; i--, pSrc--, pTar++)
				*pTar = BitsReverseTable[(u_char)((*pSrc << cBits) | (*(pSrc+1) >> cShift))];
			ReverseBits(*(pSrc+1) >> cShift, &pTar, &iByte, &iWeightSum, iSrcWeight, iTarWeight, cBits);
		}
		else /* byte boundary */
		{
			for(i = cBytes, pSrc = pSrc + cBytes - 1; i > 0; i--, pSrc--, pTar++)
				*pTar = BitsReverseTable[*pSrc];
		}
	}
	else /* To shrink or enlarge */
	{
		if(cBits)
		{
			int cShift = 8 - cBits;
			for(i = cBytes, pSrc = pSrc + cBytes - 1; i > 0; i--, pSrc--)
				ReverseBits((*pSrc << cBits) | (*(pSrc+1) >> cShift), &pTar, &iByte, &iWeightSum, iSrcWeight, iTarWeight, 8);
			ReverseBits(*(pSrc+1) >> cShift, &pTar, &iByte, &iWeightSum, iSrcWeight, iTarWeight, cBits);
		}
		else /* byte boundary */
		{
			for(i = cBytes, pSrc = pSrc + cBytes - 1; i > 0; i--, pSrc--)
				ReverseBits(*pSrc, &pTar, &iByte, &iWeightSum, iSrcWeight, iTarWeight, 8);
		}
	}

	if(iByte != 1)
	{
		while(iByte < 0x100)
		{
			iByte <<= 1;
			iByte |= iPadBit;
		}
		*pTar++ = (u_char)iByte;
	}

	cBytes = (int)(pTar - pTarget);

	for(i = iBufSize - cBytes; i > 0; i--, pTar++)
		*pTar = bPad;
}

/*.............................................................................
 *
 */
static void usb_AverageColorByte( struct Plustek_Device *dev )
{
	u_long   dw;
    pScanDef scanning = &dev->scanning;

	if((scanning->sParam.bSource == SOURCE_Negative ||
		scanning->sParam.bSource == SOURCE_Transparency) &&
											scanning->sParam.PhyDpi.x > 800) {

		for (dw = 0; dw < (scanning->sParam.Size.dwPhyPixels - 1); dw++)
		{
			scanning->Red.pcb[dw].a_bColor[0] =
						(u_char)(((u_short)scanning->Red.pcb[dw].a_bColor[0] +
						(u_short)scanning->Red.pcb[dw + 1].a_bColor[0]) / 2);

			scanning->Green.pcb[dw].a_bColor[0] =
						(u_char)(((u_short)scanning->Green.pcb[dw].a_bColor[0] +
						(u_short)scanning->Green.pcb[dw + 1].a_bColor[0]) / 2);

			scanning->Blue.pcb[dw].a_bColor[0] =
						(u_char)(((u_short)scanning->Blue.pcb[dw].a_bColor[0] +
						(u_short)scanning->Blue.pcb[dw + 1].a_bColor[0]) / 2);
		}
	}
}

/*.............................................................................
 *
 */
static void usb_AverageColorWord( struct Plustek_Device *dev )
{
	u_long   dw;
    pScanDef scanning = &dev->scanning;

	if((scanning->sParam.bSource == SOURCE_Negative ||
		scanning->sParam.bSource == SOURCE_Transparency) &&
		scanning->sParam.PhyDpi.x > 800)
	{
		scanning->Red.pcw[0].Colors[0] = _HILO2WORD (scanning->Red.pcw[0].HiLo[0]) >> 2;
		scanning->Green.pcw[0].Colors[0] = _HILO2WORD (scanning->Green.pcw[0].HiLo[0]) >> 2;
		scanning->Blue.pcw[0].Colors[0] = _HILO2WORD (scanning->Blue.pcw[0].HiLo[0]) >> 2;

		for (dw = 0; dw < (scanning->sParam.Size.dwPhyPixels - 1); dw++)
		{
			scanning->Red.pcw[dw + 1].Colors[0] = _HILO2WORD (scanning->Red.pcw[dw + 1].HiLo[0]) >> 2;
			scanning->Green.pcw[dw + 1].Colors[0] = _HILO2WORD (scanning->Green.pcw[dw + 1].HiLo[0]) >> 2;
			scanning->Blue.pcw[dw + 1].Colors[0] = _HILO2WORD (scanning->Blue.pcw[dw + 1].HiLo[0]) >> 2;

			scanning->Red.pcw[dw].Colors[0] = (u_short)(((u_long)scanning->Red.pcw[dw].Colors[0] + (u_long)scanning->Red.pcw[dw + 1].Colors[0]) / 2);
			scanning->Green.pcw[dw].Colors[0] = (u_short)(((u_long)scanning->Green.pcw[dw].Colors[0] + (u_long)scanning->Green.pcw[dw + 1].Colors[0]) / 2);
			scanning->Blue.pcw[dw].Colors[0] = (u_short)(((u_long)scanning->Blue.pcw[dw].Colors[0] + (u_long)scanning->Blue.pcw[dw + 1].Colors[0]) / 2);

			scanning->Red.pcw[dw].Colors[0] = _HILO2WORD (scanning->Red.pcw[dw].HiLo[0]) << 2;
			scanning->Green.pcw[dw].Colors[0] = _HILO2WORD (scanning->Green.pcw[dw].HiLo[0]) << 2;
			scanning->Blue.pcw[dw].Colors[0] = _HILO2WORD (scanning->Blue.pcw[dw].HiLo[0]) << 2;
		}

		scanning->Red.pcw[dw].Colors[0] = _HILO2WORD (scanning->Red.pcw[dw].HiLo[0]) << 2;
		scanning->Green.pcw[dw].Colors[0] = _HILO2WORD (scanning->Green.pcw[dw].HiLo[0]) << 2;
		scanning->Blue.pcw[dw].Colors[0] = _HILO2WORD (scanning->Blue.pcw[dw].HiLo[0]) << 2;
	}
}

/**
 *
 */
static void usb_AverageGrayByte( struct Plustek_Device *dev )
{
	u_long   dw;
    pScanDef scanning = &dev->scanning;

	if((scanning->sParam.bSource == SOURCE_Negative ||
		scanning->sParam.bSource == SOURCE_Transparency) &&
		scanning->sParam.PhyDpi.x > 800)
	{
		for (dw = 0; dw < (scanning->sParam.Size.dwPhyPixels - 1); dw++)
			scanning->Green.pb[dw] = (u_char)(((u_short)scanning->Green.pb[dw]+
									   (u_short)scanning->Green.pb[dw+1]) / 2);
	}

}

/*.............................................................................
 *
 */
static void usb_AverageGrayWord( struct Plustek_Device *dev )
{
	u_long   dw;
    pScanDef scanning = &dev->scanning;

	if((scanning->sParam.bSource == SOURCE_Negative ||
		scanning->sParam.bSource == SOURCE_Transparency) &&
		scanning->sParam.PhyDpi.x > 800)
	{
		scanning->Green.pw[0] = _HILO2WORD(scanning->Green.philo[0]) >> 2;

		for (dw = 0; dw < (scanning->sParam.Size.dwPhyPixels - 1); dw++)
		{
			scanning->Green.pw[dw + 1] = _HILO2WORD(scanning->Green.philo[dw+1]) >> 2;

			scanning->Green.pw[dw] = (u_short)(((u_long)scanning->Green.pw[dw]+
									 (u_long)scanning->Green.pw[dw+1]) / 2);

			scanning->Green.pw[dw] = _HILO2WORD(scanning->Green.philo[dw]) << 2;
		}

		scanning->Green.pw[dw] = _HILO2WORD(scanning->Green.philo[dw]) << 2;
	}
}

/**
 * returns the zoom value, used for our scaling algorithm (DDA algo
 * digital differential analyzer).
 */
static int usb_GetScaler( pScanDef scanning )
{
	double ratio;

	ratio = (double)scanning->sParam.UserDpi.x/
			(double)scanning->sParam.PhyDpi.x;	
			
	return (int)(1.0/ratio * _SCALER);			
}

/**
 *
 */
static void usb_ColorScaleGray( struct Plustek_Device *dev )
{
	int      izoom, ddax;
	u_long   dw;
    pScanDef scanning = &dev->scanning;

	usb_AverageColorByte( dev );

	dw = scanning->sParam.Size.dwPixels;

	if( scanning->sParam.bSource == SOURCE_ADF ) {
		iNext    = -1;
		dwPixels = scanning->sParam.Size.dwPixels - 1;
	} else {
		iNext    = 1;
		dwPixels = 0;
	}
	
	izoom = usb_GetScaler( scanning );			

	switch( scanning->fGrayFromColor ) {
	
	case 1:
	 	for( dwBitsPut = 0, ddax = 0; dw; dwBitsPut++ ) {

	 		ddax -= _SCALER;

 			while((ddax < 0) && (dw > 0)) {

 				scanning->UserBuf.pb[dwPixels] =
 									scanning->Red.pcb[dwBitsPut].a_bColor[0];
  									
 				dwPixels = dwPixels + iNext;
 				ddax    += izoom;
 				dw--;
 			}
 		} 			
		break;
		
	case 2:
	 	for( dwBitsPut = 0, ddax = 0; dw; dwBitsPut++ ) {

	 		ddax -= _SCALER;

 			while((ddax < 0) && (dw > 0)) {

 				scanning->UserBuf.pb[dwPixels] =
 									scanning->Green.pcb[dwBitsPut].a_bColor[0];
  									
 				dwPixels = dwPixels + iNext;
 				ddax    += izoom;
 				dw--;
 			}
 		} 			
		break;
		
	case 3:
	 	for( dwBitsPut = 0, ddax = 0; dw; dwBitsPut++ ) {

	 		ddax -= _SCALER;

 			while((ddax < 0) && (dw > 0)) {

 				scanning->UserBuf.pb[dwPixels] =
 									scanning->Blue.pcb[dwBitsPut].a_bColor[0];
  									
 				dwPixels = dwPixels + iNext;
 				ddax    += izoom;
 				dw--;
 			}
 		} 			
		break;
	}
}

/**
 *
 */
static void usb_ColorScaleGray_2( struct Plustek_Device *dev )
{
	int      izoom, ddax;
	u_long   dw;
    pScanDef scanning = &dev->scanning;

	usb_AverageColorByte( dev );

	dw = scanning->sParam.Size.dwPixels;

	if( scanning->sParam.bSource == SOURCE_ADF ) {
		iNext    = -1;
		dwPixels = scanning->sParam.Size.dwPixels - 1;
	} else {
		iNext    = 1;
		dwPixels = 0;
	}

	izoom = usb_GetScaler( scanning );

	switch( scanning->fGrayFromColor ) {

	case 1:
	 	for( dwBitsPut = 0, ddax = 0; dw; dwBitsPut++ ) {

	 		ddax -= _SCALER;

 			while((ddax < 0) && (dw > 0)) {

 				scanning->UserBuf.pb[dwPixels] =
 									scanning->Red.pb[dwBitsPut];

 				dwPixels = dwPixels + iNext;
 				ddax    += izoom;
 				dw--;
 			}
 		}
		break;

	case 2:
	 	for( dwBitsPut = 0, ddax = 0; dw; dwBitsPut++ ) {

	 		ddax -= _SCALER;

 			while((ddax < 0) && (dw > 0)) {

 				scanning->UserBuf.pb[dwPixels] =
 									scanning->Green.pb[dwBitsPut];

 				dwPixels = dwPixels + iNext;
 				ddax    += izoom;
 				dw--;
 			}
 		}
		break;

	case 3:
	 	for( dwBitsPut = 0, ddax = 0; dw; dwBitsPut++ ) {

	 		ddax -= _SCALER;

 			while((ddax < 0) && (dw > 0)) {

 				scanning->UserBuf.pb[dwPixels] = scanning->Blue.pb[dwBitsPut];

 				dwPixels = dwPixels + iNext;
 				ddax    += izoom;
 				dw--;
 			}
 		}
		break;
	}
}

/*.............................................................................
 * here we copy and scale from scanner world to user world...
 */
static void usb_ColorScale8( struct Plustek_Device *dev )
{
	int      izoom, ddax;
	u_long   dw;
    pScanDef scanning = &dev->scanning;

	usb_AverageColorByte( dev );

	dw = scanning->sParam.Size.dwPixels;

	if( scanning->sParam.bSource == SOURCE_ADF ) {
		iNext    = -1;
		dwPixels = scanning->sParam.Size.dwPixels - 1;
	} else {
		iNext    = 1;
		dwPixels = 0;
	}

	izoom = usb_GetScaler( scanning );			

	for( dwBitsPut = 0, ddax = 0; dw; dwBitsPut++ ) {

		ddax -= _SCALER;

		while((ddax < 0) && (dw > 0)) {

			scanning->UserBuf.pb_rgb[dwPixels].Red =
									scanning->Red.pcb[dwBitsPut].a_bColor[0];
									
			scanning->UserBuf.pb_rgb[dwPixels].Green =
									scanning->Green.pcb[dwBitsPut].a_bColor[0];
									
			scanning->UserBuf.pb_rgb[dwPixels].Blue =
									scanning->Blue.pcb[dwBitsPut].a_bColor[0];
			
			dwPixels = dwPixels + iNext;
			ddax    += izoom;
			dw--;
		}
	} 			
}

static void usb_ColorScale8_2( struct Plustek_Device *dev )
{
	int      izoom, ddax;
	u_long   dw;
    pScanDef scanning = &dev->scanning;

	dw = scanning->sParam.Size.dwPixels;

	if( scanning->sParam.bSource == SOURCE_ADF ) {
		iNext    = -1;
		dwPixels = scanning->sParam.Size.dwPixels - 1;
	} else {
		iNext    = 1;
		dwPixels = 0;
	}

	izoom = usb_GetScaler( scanning );			

	for( dwBitsPut = 0, ddax = 0; dw; dwBitsPut++ ) {

		ddax -= _SCALER;

		while((ddax < 0) && (dw > 0)) {

			scanning->UserBuf.pb_rgb[dwPixels].Red =
												scanning->Red.pb[dwBitsPut];
									
			scanning->UserBuf.pb_rgb[dwPixels].Green =
												scanning->Green.pb[dwBitsPut];
									
			scanning->UserBuf.pb_rgb[dwPixels].Blue =
												scanning->Blue.pb[dwBitsPut];
			
			dwPixels = dwPixels + iNext;
			ddax    += izoom;
			dw--;
		}
	} 			
}

/**
 *
 */
static void usb_ColorScale16( struct Plustek_Device *dev )
{
	int      izoom, ddax;
	u_long   dw;
    pScanDef scanning = &dev->scanning;

	usb_AverageColorWord( dev );

	dw = scanning->sParam.Size.dwPixels;

	if( scanning->sParam.bSource == SOURCE_ADF ) {
		iNext    = -1;
		dwPixels = scanning->sParam.Size.dwPixels - 1;
	} else {
		iNext    = 1;
		dwPixels = 0;
	}

	izoom = usb_GetScaler( scanning );			
	
	if( scanning->dwFlag & SCANFLAG_RightAlign ) {
	
		for( dwBitsPut = 0, ddax = 0; dw; dwBitsPut++ ) {

			ddax -= _SCALER;

			while((ddax < 0) && (dw > 0)) {

				scanning->UserBuf.pw_rgb[dwPixels].Red =
						_HILO2WORD(scanning->Red.pcw[dwBitsPut].HiLo[0]) >> Shift;
				
				scanning->UserBuf.pw_rgb[dwPixels].Green =
						_HILO2WORD(scanning->Green.pcw[dwBitsPut].HiLo[0]) >> Shift;
						
				scanning->UserBuf.pw_rgb[dwPixels].Blue =
						_HILO2WORD(scanning->Blue.pcw[dwBitsPut].HiLo[0]) >> Shift;
			
				dwPixels = dwPixels + iNext;
				ddax    += izoom;
				dw--;
			}
		} 			
	
	} else {
		
		for( dwBitsPut = 0, ddax = 0; dw; dwBitsPut++ ) {

			ddax -= _SCALER;

			while((ddax < 0) && (dw > 0)) {

				scanning->UserBuf.pw_rgb[dwPixels].Red =
							_HILO2WORD(scanning->Red.pcw[dwBitsPut].HiLo[0]);
				
				scanning->UserBuf.pw_rgb[dwPixels].Green =
							_HILO2WORD(scanning->Green.pcw[dwBitsPut].HiLo[0]);
						
				scanning->UserBuf.pw_rgb[dwPixels].Blue =
							_HILO2WORD(scanning->Blue.pcw[dwBitsPut].HiLo[0]);
			
				dwPixels = dwPixels + iNext;
				ddax    += izoom;
				dw--;
			}
		} 			
	}	
}

/**
 *
 */
static void usb_ColorScale16_2( struct Plustek_Device *dev )
{
	HiLoDef  tmp;
	int      izoom, ddax;
	u_long   dw;
    pScanDef scanning = &dev->scanning;

	usb_AverageColorWord( dev );

	dw = scanning->sParam.Size.dwPixels;

	if( scanning->sParam.bSource == SOURCE_ADF ) {
		iNext    = -1;
		dwPixels = scanning->sParam.Size.dwPixels - 1;
	} else {
		iNext    = 1;
		dwPixels = 0;
	}

	izoom = usb_GetScaler( scanning );

	if( scanning->dwFlag & SCANFLAG_RightAlign ) {

		for( dwBitsPut = 0, ddax = 0; dw; dwBitsPut++ ) {

			ddax -= _SCALER;

			while((ddax < 0) && (dw > 0)) {

				tmp = *((pHiLoDef)&scanning->Red.pw[dwBitsPut]);
				scanning->UserBuf.pw_rgb[dwPixels].Red = _HILO2WORD(tmp) >> Shift;
        	
				tmp = *((pHiLoDef)&scanning->Green.pw[dwBitsPut]);
				scanning->UserBuf.pw_rgb[dwPixels].Green = _HILO2WORD(tmp) >> Shift;

				tmp = *((pHiLoDef)&scanning->Blue.pw[dwBitsPut]);
				scanning->UserBuf.pw_rgb[dwPixels].Blue = _HILO2WORD(tmp) >> Shift;

				dwPixels = dwPixels + iNext;
				ddax    += izoom;
				dw--;
			}
		}

	} else {

		for( dwBitsPut = 0, ddax = 0; dw; dwBitsPut++ ) {

			ddax -= _SCALER;

			while((ddax < 0) && (dw > 0)) {

				tmp = *((pHiLoDef)&scanning->Red.pw[dwBitsPut]);
				scanning->UserBuf.pw_rgb[dwPixels].Red = _HILO2WORD(tmp);

				tmp = *((pHiLoDef)&scanning->Green.pw[dwBitsPut]);
				scanning->UserBuf.pw_rgb[dwPixels].Green = _HILO2WORD(tmp);

				tmp = *((pHiLoDef)&scanning->Blue.pw[dwBitsPut]);
				scanning->UserBuf.pw_rgb[dwPixels].Blue = _HILO2WORD(tmp);

				dwPixels = dwPixels + iNext;
				ddax    += izoom;
				dw--;
			}
		}
	}
}

/*.............................................................................
 *
 */
static void usb_ColorScalePseudo16( struct Plustek_Device *dev )
{
	int      izoom, ddax;
	u_long   dw;
    pScanDef scanning = &dev->scanning;

	usb_AverageColorByte( dev );

	dw = scanning->sParam.Size.dwPixels;

	if( scanning->sParam.bSource == SOURCE_ADF ) {
		iNext    = -1;
		dwPixels = scanning->sParam.Size.dwPixels - 1;
	} else {
		iNext    = 1;
		dwPixels = 0;
	}
	
	izoom = usb_GetScaler( scanning );			

	wR = (u_short)scanning->Red.pcb[0].a_bColor[0];
	wG = (u_short)scanning->Green.pcb[0].a_bColor[1];
	wB = (u_short)scanning->Blue.pcb[0].a_bColor[2];

	for( dwBitsPut = 0, ddax = 0; dw; dwBitsPut++ ) {

		ddax -= _SCALER;

		while((ddax < 0) && (dw > 0)) {

			scanning->UserBuf.pw_rgb[dwPixels].Red =
				(wR + scanning->Red.pcb[dwBitsPut].a_bColor[0]) << bShift;
				
			scanning->UserBuf.pw_rgb[dwPixels].Green =
				(wG + scanning->Green.pcb[dwBitsPut].a_bColor[0]) << bShift;
				
			scanning->UserBuf.pw_rgb[dwPixels].Blue =
				(wB + scanning->Blue.pcb[dwBitsPut].a_bColor[0]) << bShift;
		
			dwPixels = dwPixels + iNext;
			ddax    += izoom;
			dw--;
		}
		
		wR = (u_short)scanning->Red.pcb[dwBitsPut].a_bColor[0];
		wG = (u_short)scanning->Green.pcb[dwBitsPut].a_bColor[0];
		wB = (u_short)scanning->Blue.pcb[dwBitsPut].a_bColor[0];
	} 			
}

/*.............................................................................
 * do a simple memcopy from "driver-space" to "user-space"
 */
static void usb_ColorDuplicate8( struct Plustek_Device *dev )
{
	u_long   dw;
    pScanDef scanning = &dev->scanning;

	usb_AverageColorByte( dev );

	if( scanning->sParam.bSource == SOURCE_ADF ) {
		iNext    = -1;
		dwPixels = scanning->sParam.Size.dwPixels - 1;
	} else {
		iNext    = 1;
		dwPixels = 0;
	}

	for( dw = 0; dw < scanning->sParam.Size.dwPixels;
										dw++, dwPixels = dwPixels + iNext ) {
										
		scanning->UserBuf.pb_rgb[dwPixels].Red =
										   scanning->Red.pcb[dw].a_bColor[0];
											
		scanning->UserBuf.pb_rgb[dwPixels].Green =
										   scanning->Green.pcb[dw].a_bColor[0];
										
		scanning->UserBuf.pb_rgb[dwPixels].Blue =
										   scanning->Blue.pcb[dw].a_bColor[0];
	}
}

/**
 * reorder from rgb line to rgb pixel (CIS scanner)
 */
static void usb_ColorDuplicate8_2( struct Plustek_Device *dev )
{
	u_long   dw;
    pScanDef scanning = &dev->scanning;

	if( scanning->sParam.bSource == SOURCE_ADF ) {
		iNext    = -1;
		dwPixels = scanning->sParam.Size.dwPixels - 1;
	} else {
		iNext    = 1;
		dwPixels = 0;
	}

	for( dw = 0; dw < scanning->sParam.Size.dwPixels;
										dw++, dwPixels = dwPixels + iNext ) {
										
		scanning->UserBuf.pb_rgb[dwPixels].Red   = (u_char)scanning->Red.pb[dw];
		scanning->UserBuf.pb_rgb[dwPixels].Green = (u_char)scanning->Green.pb[dw];
		scanning->UserBuf.pb_rgb[dwPixels].Blue  = (u_char)scanning->Blue.pb[dw];
	}
}

/**
 *
 */
static void usb_ColorDuplicate16( struct Plustek_Device *dev )
{
	u_long   dw;
    pScanDef scanning = &dev->scanning;

	usb_AverageColorWord( dev );

	if( scanning->sParam.bSource == SOURCE_ADF ) {
		iNext    = -1;
		dwPixels = scanning->sParam.Size.dwPixels - 1;
	} else {
		iNext    = 1;
		dwPixels = 0;
	}

	if( scanning->dwFlag & SCANFLAG_RightAlign ) {
	
		for (dw = 0; dw < scanning->sParam.Size.dwPixels; dw++,
												dwPixels = dwPixels + iNext) {
			scanning->UserBuf.pw_rgb[dwPixels].Red =
							_HILO2WORD(scanning->Red.pcw[dw].HiLo[0]) >> Shift;
			scanning->UserBuf.pw_rgb[dwPixels].Green =
						  _HILO2WORD(scanning->Green.pcw[dw].HiLo[0]) >> Shift;
			scanning->UserBuf.pw_rgb[dwPixels].Blue =
						   _HILO2WORD(scanning->Blue.pcw[dw].HiLo[0]) >> Shift;
		}
	} else {
	
		for (dw = 0; dw < scanning->sParam.Size.dwPixels; dw++,
												dwPixels = dwPixels + iNext) {

			scanning->UserBuf.pw_rgb[dwPixels].Red =
									_HILO2WORD(scanning->Red.pcw[dw].HiLo[0]);
			scanning->UserBuf.pw_rgb[dwPixels].Green =
									_HILO2WORD(scanning->Green.pcw[dw].HiLo[0]);
			scanning->UserBuf.pw_rgb[dwPixels].Blue =
									_HILO2WORD(scanning->Blue.pcw[dw].HiLo[0]);
		}
	}
}

/**
 *
 */
static void usb_ColorDuplicate16_2( struct Plustek_Device *dev )
{
	HiLoDef  tmp;
	u_long   dw;
    pScanDef scanning = &dev->scanning;

	usb_AverageColorWord( dev );

	if( scanning->sParam.bSource == SOURCE_ADF ) {
		iNext    = -1;
		dwPixels = scanning->sParam.Size.dwPixels - 1;
	} else {
		iNext    = 1;
		dwPixels = 0;
	}

	if( scanning->dwFlag & SCANFLAG_RightAlign ) {

		for (dw = 0; dw < scanning->sParam.Size.dwPixels; dw++, dwPixels = dwPixels + iNext)
		{
			tmp = *((pHiLoDef)&scanning->Red.pw[dw]);
			scanning->UserBuf.pw_rgb[dwPixels].Red = _HILO2WORD(tmp) >> Shift;

			tmp = *((pHiLoDef)&scanning->Green.pw[dw]);
			scanning->UserBuf.pw_rgb[dwPixels].Green = _HILO2WORD(tmp) >> Shift;

			tmp = *((pHiLoDef)&scanning->Blue.pw[dw]);
			scanning->UserBuf.pw_rgb[dwPixels].Blue = _HILO2WORD(tmp) >> Shift;
		}
	} else {

		for (dw = 0; dw < scanning->sParam.Size.dwPixels; dw++, dwPixels = dwPixels + iNext)
		{
			tmp = *((pHiLoDef)&scanning->Red.pw[dw]);
			scanning->UserBuf.pw_rgb[dwPixels].Red = _HILO2WORD(tmp);

			tmp = *((pHiLoDef)&scanning->Green.pw[dw]);
			scanning->UserBuf.pw_rgb[dwPixels].Green = _HILO2WORD(tmp);

			tmp = *((pHiLoDef)&scanning->Blue.pw[dw]);
			scanning->UserBuf.pw_rgb[dwPixels].Blue = _HILO2WORD(tmp);
		}
	}
}

/*.............................................................................
 *
 */
static void usb_ColorDuplicatePseudo16( struct Plustek_Device *dev )
{
	u_long   dw;
    pScanDef scanning = &dev->scanning;

	usb_AverageColorByte( dev );

	if (scanning->sParam.bSource == SOURCE_ADF)
	{
		iNext = -1;
		dwPixels = scanning->sParam.Size.dwPixels - 1;
	}
	else
	{
		iNext = 1;
		dwPixels = 0;
	}

	wR = (u_short)scanning->Red.pcb[0].a_bColor[0];
	wG = (u_short)scanning->Green.pcb[0].a_bColor[0];
	wB = (u_short)scanning->Blue.pcb[0].a_bColor[0];

	for (dw = 0; dw < scanning->sParam.Size.dwPixels; dw++, dwPixels = dwPixels + iNext)
	{
		scanning->UserBuf.pw_rgb[dwPixels].Red = (wR + scanning->Red.pcb[dw].a_bColor[0]) << bShift;
		scanning->UserBuf.pw_rgb[dwPixels].Green = (wG + scanning->Green.pcb[dw].a_bColor[0]) << bShift;
		scanning->UserBuf.pw_rgb[dwPixels].Blue = (wB + scanning->Blue.pcb[dw].a_bColor[0]) << bShift;
		wR = (u_short)scanning->Red.pcb[dw].a_bColor[0];
		wG = (u_short)scanning->Green.pcb[dw].a_bColor[0];
		wB = (u_short)scanning->Blue.pcb[dw].a_bColor[0];
	}
}

/**
 *
 */
static void usb_ColorDuplicateGray( struct Plustek_Device *dev )
{
	u_long   dw;
    pScanDef scanning = &dev->scanning;

	usb_AverageColorByte( dev );

	if (scanning->sParam.bSource == SOURCE_ADF)
	{
		iNext = -1;
		dwPixels = scanning->sParam.Size.dwPixels - 1;
	}
	else
	{
		iNext = 1;
		dwPixels = 0;
	}

	switch(scanning->fGrayFromColor)
	{
	case 1:
		for (dw = 0; dw < scanning->sParam.Size.dwPixels; dw++, dwPixels = dwPixels + iNext)
			scanning->UserBuf.pb[dwPixels] = scanning->Red.pcb[dw].a_bColor[0];
		break;
	case 2:
		for (dw = 0; dw < scanning->sParam.Size.dwPixels; dw++, dwPixels = dwPixels + iNext)
			scanning->UserBuf.pb[dwPixels] = scanning->Green.pcb[dw].a_bColor[0];
		break;
	case 3:
		for (dw = 0; dw < scanning->sParam.Size.dwPixels; dw++, dwPixels = dwPixels + iNext)
			scanning->UserBuf.pb[dwPixels] = scanning->Blue.pcb[dw].a_bColor[0];
		break;
	}
}

/**
 *
 */
static void usb_ColorDuplicateGray_2( struct Plustek_Device *dev )
{
	u_long   dw;
    pScanDef scanning = &dev->scanning;

	usb_AverageColorByte( dev );

	if (scanning->sParam.bSource == SOURCE_ADF)
	{
		iNext = -1;
		dwPixels = scanning->sParam.Size.dwPixels - 1;
	}
	else
	{
		iNext = 1;
		dwPixels = 0;
	}

	switch(scanning->fGrayFromColor)
	{
	case 1:
		for (dw = 0; dw < scanning->sParam.Size.dwPixels; dw++, dwPixels = dwPixels + iNext)
			scanning->UserBuf.pb[dwPixels] = scanning->Red.pb[dw];
		break;
	case 2:
		for (dw = 0; dw < scanning->sParam.Size.dwPixels; dw++, dwPixels = dwPixels + iNext)
			scanning->UserBuf.pb[dwPixels] = scanning->Green.pb[dw];
		break;
	case 3:
		for (dw = 0; dw < scanning->sParam.Size.dwPixels; dw++, dwPixels = dwPixels + iNext)
			scanning->UserBuf.pb[dwPixels] = scanning->Blue.pb[dw];
		break;
	}
}

/**
 *
 */
static void usb_BWScale( struct Plustek_Device *dev )
{
	u_char   tmp;
	int      izoom, ddax;
	u_long   i, dw;
    pScanDef scanning = &dev->scanning;

	pbSrce = scanning->Green.pb;
	if( scanning->sParam.bSource == SOURCE_ADF ) {
		int iSum = wSum;
		usb_ReverseBitStream(scanning->Green.pb, scanning->UserBuf.pb,
							 scanning->sParam.Size.dwValidPixels,
							 scanning->dwBytesLine,
							 scanning->sParam.PhyDpi.x,
							 scanning->sParam.UserDpi.x, 1 );
		wSum = iSum;
		return;
		
	} else {
		pbDest = scanning->UserBuf.pb;
		iNext = 1;
	}

	izoom = usb_GetScaler( scanning );			
	
	memset( pbDest, 0, scanning->dwBytesLine );
	ddax = 0;
	dw 	 = 0;

	for( i = 0; i < scanning->sParam.Size.dwValidPixels; i++ ) {

		ddax -= _SCALER;

		while( ddax < 0 ) {

			tmp = pbSrce[(i>>3)];
				
			if((dw>>3) < scanning->sParam.Size.dwValidPixels ) {
			
				if( 0 != (tmp &= (1 << ((~(i & 0x7))&0x7))))
					pbDest[dw>>3] |= (1 << ((~(dw & 0x7))&0x7));
			}
			dw++;
			ddax += izoom;
		}
	}
}

/**
 *
 */
static void usb_BWDuplicate( struct Plustek_Device *dev )
{
    pScanDef scanning = &dev->scanning;

	if(scanning->sParam.bSource == SOURCE_ADF)
	{
		usb_ReverseBitStream( scanning->Green.pb, scanning->UserBuf.pb,
							  scanning->sParam.Size.dwValidPixels,
							  scanning->dwBytesLine, 0, 0, 1 );
	} else {
		memcpy( scanning->UserBuf.pb,
				scanning->Green.pb, scanning->sParam.Size.dwBytes );
	}				
}

/**
 *
 */
static void usb_GrayScale8( struct Plustek_Device *dev )
{
	int      izoom, ddax;
    pScanDef scanning = &dev->scanning;

	usb_AverageGrayByte( dev );

	pbSrce = scanning->Green.pb;
	if( scanning->sParam.bSource == SOURCE_ADF ) {
		pbDest = scanning->UserBuf.pb + scanning->sParam.Size.dwPixels - 1;
		iNext = -1;
	} else {
		pbDest = scanning->UserBuf.pb;
		iNext = 1;
	}
	
	izoom = usb_GetScaler( scanning );			

	for( dwPixels = scanning->sParam.Size.dwPixels, ddax = 0; dwPixels; pbSrce++ ) {

		ddax -= _SCALER;

		while((ddax < 0) && (dwPixels > 0)) {

			*pbDest = *pbSrce;
			pbDest  = pbDest + iNext;
			ddax   += izoom;
			dwPixels--;
		} 			
	}
}

/**
 *
 */
static void usb_GrayScale16( struct Plustek_Device *dev )
{
	int      izoom, ddax;
    pScanDef scanning = &dev->scanning;

	usb_AverageGrayWord( dev);

	pwm  = scanning->Green.philo;
	wSum = scanning->sParam.PhyDpi.x;

	if( scanning->sParam.bSource == SOURCE_ADF ) {
		iNext  = -1;
		pwDest = scanning->UserBuf.pw + scanning->sParam.Size.dwPixels - 1;
	} else {
		iNext  = 1;
		pwDest = scanning->UserBuf.pw;
	}
	
	izoom = usb_GetScaler( scanning );			
	ddax  = 0;

	if( scanning->dwFlag & SCANFLAG_RightAlign ) {

		for( dwPixels = scanning->sParam.Size.dwPixels; dwPixels; pwm++ ) {

			ddax -= _SCALER;

			while((ddax < 0) && (dwPixels > 0)) {

				*pwDest = _PHILO2WORD( pwm ) >> Shift;
				pwDest  = pwDest + iNext;
  				ddax   += izoom;
				dwPixels--;
			}	
		} 			
	
	} else {

		for( dwPixels = scanning->sParam.Size.dwPixels; dwPixels; pwm++ ) {

			ddax -= _SCALER;

			while((ddax < 0) && (dwPixels > 0)) {

				*pwDest = _PHILO2WORD( pwm );
				pwDest  = pwDest + iNext;
				ddax   += izoom;
				dwPixels--;
			}	
		} 			
	}
}

/**
 *
 */
static void usb_GrayScalePseudo16( struct Plustek_Device *dev )
{
	int      izoom, ddax;
    pScanDef scanning = &dev->scanning;

	usb_AverageGrayByte( dev );

	pbSrce = scanning->Green.pb;
	wSum = scanning->sParam.PhyDpi.x;
	wG = (u_short) *pbSrce;
	if( scanning->sParam.bSource == SOURCE_ADF ) {
		iNext  = -1;
		pwDest = scanning->UserBuf.pw + scanning->sParam.Size.dwPixels - 1;
	} else {
		iNext  = 1;
		pwDest = scanning->UserBuf.pw;
	}

	izoom = usb_GetScaler( scanning );			
	ddax  = 0;

	for( dwPixels = scanning->sParam.Size.dwPixels, ddax = 0; dwPixels; pbSrce++ ) {

		ddax -= _SCALER;

		while((ddax < 0) && (dwPixels > 0)) {

			*pwDest = (wG + *pbSrce) << bShift;
			pbDest  = pbDest + iNext;
			ddax   += izoom;
			dwPixels--;
		} 			
		wG = (u_short)*pbSrce;
	}
}

/**
 *
 */
static void usb_GrayDuplicate8( struct Plustek_Device *dev )
{
    pScanDef scanning = &dev->scanning;

	usb_AverageGrayByte( dev );

	if( scanning->sParam.bSource == SOURCE_ADF ) {

		dwPixels = scanning->sParam.Size.dwPixels;
		pbSrce   = scanning->Green.pb;
		pbDest   = scanning->UserBuf.pb + dwPixels - 1;

		for(; dwPixels; dwPixels--, pbSrce++, pbDest--)
			*pbDest = *pbSrce;

	} else
		memcpy( scanning->UserBuf.pb,
				scanning->Green.pb, scanning->sParam.Size.dwBytes );
}

/**
 *
 */
static void usb_GrayDuplicate16( struct Plustek_Device *dev )
{
    pScanDef scanning = &dev->scanning;

	usb_AverageGrayWord( dev );

	if (scanning->sParam.bSource == SOURCE_ADF)
	{
		iNext = -1;
		pwDest = scanning->UserBuf.pw + scanning->sParam.Size.dwPixels - 1;
	}
	else
	{
		iNext = 1;
		pwDest = scanning->UserBuf.pw;
	}
	pwm = scanning->Green.philo;
	if (scanning->dwFlag & SCANFLAG_RightAlign)
		for (dwPixels = scanning->sParam.Size.dwPixels; dwPixels--; pwm++, pwDest = pwDest + iNext)
			*pwDest = (_PHILO2WORD(pwm)) >> Shift;
	else
		for (dwPixels = scanning->sParam.Size.dwPixels; dwPixels--; pwm++, pwDest = pwDest + iNext)
			*pwDest = (_PHILO2WORD(pwm)) & Mask;
}

/**
 *
 */
static void usb_GrayDuplicatePseudo16( struct Plustek_Device *dev )
{
    pScanDef scanning = &dev->scanning;

	usb_AverageGrayByte( dev );

	pbSrce = scanning->UserBuf.pb;
	if (scanning->sParam.bSource == SOURCE_ADF)
	{
		iNext = -1;
		pwDest = scanning->Green.pw + scanning->sParam.Size.dwPixels - 1;
	}
	else
	{
		iNext = 1;
		pwDest = scanning->Green.pw;
	}
	wG = (u_short)*pbSrce;

	for( dwPixels = scanning->sParam.Size.dwPixels;
		 dwPixels++; pbSrce++, pwDest = pwDest + iNext ) {
		*pwDest = (wG + *pbSrce) << bShift;
		wG = (u_short)*pbSrce;
	}
}

/*.............................................................................
 * function to select the apropriate pixel copy function
 */
static void usb_GetImageProc( struct Plustek_Device *dev )
{
    pScanDef  scanning = &dev->scanning;
	pDCapsDef sc       = &dev->usbDev.Caps;
	pHWDef    hw       = &dev->usbDev.HwSetting;

    bShift = 0;

	if( scanning->sParam.UserDpi.x != scanning->sParam.PhyDpi.x ) {

		/* Pixel scaling... */
		switch( scanning->sParam.bDataType ) {

			case SCANDATATYPE_Color:
				if (scanning->sParam.bBitDepth > 8) {

					if( hw->bReg_0x26 & _ONE_CH_COLOR ) {
						scanning->pfnProcess = usb_ColorScale16_2;
						DBG( _DBG_INFO, "ImageProc is: ColorScale16_2\n" );
					} else {
						scanning->pfnProcess = usb_ColorScale16;
						DBG( _DBG_INFO, "ImageProc is: ColorScale16\n" );
					}
					
				} else if (scanning->dwFlag & SCANFLAG_Pseudo48) {
					scanning->pfnProcess = usb_ColorScalePseudo16;
					DBG( _DBG_INFO, "ImageProc is: ColorScalePseudo16\n" );
					
				} else if (scanning->fGrayFromColor) {

					if( hw->bReg_0x26 & _ONE_CH_COLOR ) {
						scanning->pfnProcess = usb_ColorScaleGray_2;
						DBG( _DBG_INFO, "ImageProc is: ColorScaleGray_2\n" );
					} else {
						scanning->pfnProcess = usb_ColorScaleGray;
						DBG( _DBG_INFO, "ImageProc is: ColorScaleGray\n" );
					}
					
				} else {

					if( hw->bReg_0x26 & _ONE_CH_COLOR ) {
						scanning->pfnProcess = usb_ColorScale8_2;
						DBG( _DBG_INFO, "ImageProc is: ColorScale8_2\n" );
					} else {
						scanning->pfnProcess = usb_ColorScale8;
						DBG( _DBG_INFO, "ImageProc is: ColorScale8\n" );
					}
				}	
				break;

			case SCANDATATYPE_Gray:
				if (scanning->sParam.bBitDepth > 8) {
					scanning->pfnProcess = usb_GrayScale16;
					DBG( _DBG_INFO, "ImageProc is: GrayScale16\n" );
				} else {

					if (scanning->dwFlag & SCANFLAG_Pseudo48) {
						scanning->pfnProcess = usb_GrayScalePseudo16;
						DBG( _DBG_INFO, "ImageProc is: GrayScalePseudo16\n" );
					} else {
						scanning->pfnProcess = usb_GrayScale8;
						DBG( _DBG_INFO, "ImageProc is: GrayScale8\n" );
					}						
				}		
				break;

			default:
				scanning->pfnProcess = usb_BWScale;
				DBG( _DBG_INFO, "ImageProc is: BWScale\n" );
				break;
		}

	} else {

		/* Pixel copy */
		switch( scanning->sParam.bDataType ) {

			case SCANDATATYPE_Color:
				if (scanning->sParam.bBitDepth > 8) {
					if( hw->bReg_0x26 & _ONE_CH_COLOR ) {
						scanning->pfnProcess = usb_ColorDuplicate16_2;
						DBG( _DBG_INFO, "ImageProc is: ColorDuplicate16_2\n" );
					} else {
						scanning->pfnProcess = usb_ColorDuplicate16;
						DBG( _DBG_INFO, "ImageProc is: ColorDuplicate16\n" );
					}
				} else if (scanning->dwFlag & SCANFLAG_Pseudo48) {
					scanning->pfnProcess = usb_ColorDuplicatePseudo16;
					DBG( _DBG_INFO, "ImageProc is: ColorDuplicatePseudo16\n" );
				} else if (scanning->fGrayFromColor) {
					if( hw->bReg_0x26 & _ONE_CH_COLOR ) {
						scanning->pfnProcess = usb_ColorDuplicateGray_2;
						DBG( _DBG_INFO, "ImageProc is: ColorDuplicateGray_2\n" );
					} else {
						scanning->pfnProcess = usb_ColorDuplicateGray;
						DBG( _DBG_INFO, "ImageProc is: ColorDuplicateGray\n" );
					}
				} else {
					if( hw->bReg_0x26 & _ONE_CH_COLOR ) {
						scanning->pfnProcess = usb_ColorDuplicate8_2;
						DBG( _DBG_INFO, "ImageProc is: ColorDuplicate8_2\n" );
					} else {
						scanning->pfnProcess = usb_ColorDuplicate8;
						DBG( _DBG_INFO, "ImageProc is: ColorDuplicate8\n" );
					}
				}	
				break;

			case SCANDATATYPE_Gray:
				if (scanning->sParam.bBitDepth > 8) {
					scanning->pfnProcess = usb_GrayDuplicate16;
					DBG( _DBG_INFO, "ImageProc is: GrayDuplicate16\n" );
				} else {
					if (scanning->dwFlag & SCANFLAG_Pseudo48) {
						scanning->pfnProcess = usb_GrayDuplicatePseudo16;
						DBG( _DBG_INFO, "ImageProc is: GrayDuplicatePseudo16\n" );
					} else {
						scanning->pfnProcess = usb_GrayDuplicate8;
						DBG( _DBG_INFO, "ImageProc is: GrayDuplicate8\n" );
					}	
				}		
				break;

			default:
				scanning->pfnProcess = usb_BWDuplicate;
				DBG( _DBG_INFO, "ImageProc is: BWDuplicate\n" );
				break;
		}
	}
	
	if( scanning->sParam.bBitDepth == 8 ) {
	
		if( scanning->dwFlag & SCANFLAG_Pseudo48 ) {
			if( scanning->dwFlag & SCANFLAG_RightAlign ) {
				bShift = 5;
			} else {

				/*
				 * this should fix the Bearpaw/U12 discrepancy
				 * in general the fix is needed, but not for the U12
				 * why? - no idea!
				 */			
				if(_WAF_BSHIFT7_BUG == (_WAF_BSHIFT7_BUG & sc->workaroundFlag))
					bShift = 0; /* Holger Bischof 16.12.2001 */
				else
					bShift = 7;
			}
			DBG( _DBG_INFO, "bShift adjusted: %u\n", bShift );
		}
	}

	if( _LM9833 == hw->chip ) {
		Shift = 0;
		Mask  = 0xFFFF;
	} else {
		Shift = 2;
		Mask  = 0xFFFC;
	}
}

/**
 * here we read the image data into our intermediate buffer (in the NT version
 * the function was implemented as thread)
 */
static SANE_Int usb_ReadData( struct Plustek_Device *dev )
{
	u_long   dw, dwRet, dwBytes, dwAdjust;
    pScanDef scanning = &dev->scanning;
	pHWDef   hw       = &dev->usbDev.HwSetting;

	DBG( _DBG_READ, "usb_ReadData()\n" );

	dwAdjust = 1;                                                

	/* for 1 channel color, we have to adjust the phybytes... */
	if( hw->bReg_0x26 & _ONE_CH_COLOR ) {
		if( scanning->sParam.bDataType == SCANDATATYPE_Color ) {
			dwAdjust = 3;
		}
	}

	while( scanning->sParam.Size.dwTotalBytes ) {
	
		if( usb_IsEscPressed()) {
			DBG( _DBG_INFO, "usb_ReadData() - Cancel detected...\n" );
			return 0;
		}
   		
   		if( scanning->sParam.Size.dwTotalBytes > scanning->dwBytesScanBuf )
   			dw = scanning->dwBytesScanBuf;
   		else
   			dw = scanning->sParam.Size.dwTotalBytes;
   			
   		scanning->sParam.Size.dwTotalBytes -= dw;

   		if(!scanning->sParam.Size.dwTotalBytes && dw < (m_dwPauseLimit * 1024))
   		{
   			if(!(a_bRegs[0x4e] = (u_char)ceil((double)dw /
													(4.0 * hw->wDRAMSize)))) {
   				a_bRegs[0x4e] = 1;
			}
   				
   			a_bRegs[0x4f] = 0;
				
   			sanei_lm983x_write( dev->fd, 0x4e, &a_bRegs[0x4e], 2, SANE_TRUE );
   		}

   		while( scanning->bLinesToSkip ) {
			
   			DBG( _DBG_READ, "Skipping %u lines\n", scanning->bLinesToSkip );

   			dwBytes = scanning->bLinesToSkip*scanning->sParam.Size.dwPhyBytes;
			dwBytes *= dwAdjust;
				
   			if (dwBytes > scanning->dwBytesScanBuf) {
				
   				dwBytes = scanning->dwBytesScanBuf;
   				scanning->bLinesToSkip -= scanning->dwLinesScanBuf;
   			} else {
   				scanning->bLinesToSkip = 0;
   			}						
					
   			usb_ScanReadImage( dev, scanning->pbGetDataBuf, dwBytes );
   		}
		
   	    if( usb_ScanReadImage( dev, scanning->pbGetDataBuf, dw )) {

			dumpPic( "plustek-pic.raw", scanning->pbGetDataBuf, dw );
		
   	    	if( scanning->dwLinesDiscard ) {

	   			DBG( _DBG_READ, "Discarding %lu lines\n",
			   										scanning->dwLinesDiscard );
   	    	   	    	
   	    		dwRet = dw / (scanning->sParam.Size.dwPhyBytes * dwAdjust);
   	    		
   	    		if (scanning->dwLinesDiscard > dwRet) {
   	    			scanning->dwLinesDiscard -= dwRet;
   	    			dwRet = 0;
   	    		} else	{
   	    			dwRet -= scanning->dwLinesDiscard;
   	    			scanning->dwLinesDiscard = 0;
   	    		}
   	    	} else {

   	    		dwRet = dw / (scanning->sParam.Size.dwPhyBytes * dwAdjust);
   	    	}	

   			scanning->pbGetDataBuf += scanning->dwBytesScanBuf;
   			if( scanning->pbGetDataBuf >= scanning->pbScanBufEnd ) {
   				scanning->pbGetDataBuf = scanning->pbScanBufBegin;
   			}	
   	    	
   	    	if( dwRet )
   	    		return dwRet;
   	    }
	}
	
	return 0;
}

/* END PLUSTEK-USBIMG.C .....................................................*/
