/*.............................................................................
 * Project : linux driver for Plustek parallel-port scanners
 *.............................................................................
 * File:     plustek-pp_types.h - some typedefs and error codes
 *.............................................................................
 *
 * Copyright (C) 2000-2003 Gerhard Jaeger <gerhard@gjaeger.de>
 *.............................................................................
 * History:
 * 0.30 - initial version
 * 0.31 - no changes
 * 0.32 - added _VAR_NOT_USED()
 * 0.33 - no changes
 * 0.34 - no changes
 * 0.35 - no changes
 * 0.36 - added _E_ABORT and _E_VERSION
 * 0.37 - moved _MAX_DEVICES to plustek_scan.h
 *        added pChar and TabDef
 * 0.38 - comment change for _E_NOSUPP
 *        added RGBByteDef, RGBWordDef and RGBULongDef
 *        replaced AllPointer by DataPointer
 *        replaced AllType by DataType
 *        added _LOBYTE and _HIBYTE stuff
 *        added _E_NO_ASIC and _E_NORESOURCE
 * 0.39 - no changes
 * 0.40 - moved _VAR_NOT_USED and TabDef to plustek-share.h
 * 0.41 - no changes
 * 0.42 - moved errorcodes to plustek-share.h
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
#ifndef __DRV_TYPES_H__
#define __DRV_TYPES_H__

/* define some useful types */
typedef int 			Bool;
typedef char 			Char;
typedef char 		   *pChar;
typedef unsigned char 	UChar;
typedef UChar 		   *pUChar;
typedef unsigned char   Byte;
typedef Byte 		   *pByte;

typedef short 			Short;
typedef unsigned short	UShort;
typedef UShort 		   *pUShort;

typedef unsigned int	UInt;
typedef UInt		   *pUInt;

typedef long 		  	Long;
typedef long  		   *pLong;
typedef unsigned long 	ULong;
typedef ULong 		   *pULong;

typedef void 		   *pVoid;

/*
 * the boolean values
 */
#ifndef _TRUE
#	define _TRUE    1
#endif
#ifndef _FALSE
#	define _FALSE   0
#endif

#define _LOWORD(x)	((UShort)(x & 0xffff))
#define _HIWORD(x)	((UShort)(x >> 16))
#define _LOBYTE(x)  ((Byte)((x) & 0xFF))
#define _HIBYTE(x)  ((Byte)((x) >> 8))

/*
 * some useful things...
 */
typedef struct
{
	Byte b1st;
	Byte b2nd;
} WordVal, *pWordVal;

typedef struct
{
    WordVal w1st;
    WordVal w2nd;
} DWordVal, *pDWordVal;

/* useful for RGB-values */
typedef struct {
	Byte Red;
	Byte Green;
	Byte Blue;
} RGBByteDef, *pRGBByteDef;

typedef struct {
	UShort Red;
	UShort Green;
	UShort Blue;
} RGBUShortDef, *pRGBUShortDef;

typedef struct {

    union {
        pUChar  bp;
        pUShort usp;
        pULong  ulp;
    } red;
    union {
        pUChar  bp;
        pUShort usp;
        pULong  ulp;
    } green;
    union {
        pUChar  bp;
        pUShort usp;
        pULong  ulp;
    } blue;

} RBGPtrDef;


typedef struct {
	ULong Red;
	ULong Green;
	ULong Blue;
} RGBULongDef, *pRGBULongDef;

typedef union {
	pUChar	       pb;
	pUShort	       pw;
	pULong	       pdw;
    pRGBByteDef    pbrgb;
    pRGBUShortDef  pusrgb;
    pRGBULongDef   pulrgb;
} DataPointer, *pDataPointer;

typedef union {
	WordVal	 wOverlap;
	DWordVal dwOverlap;
	ULong	 dwValue;
	UShort	 wValue;
	Byte	 bValue;
} DataType, *pDataType;

#endif /* guard __DRV_TYPES_H__ */

/* END PLUSTEK-PP_TYPES.H ...................................................*/
