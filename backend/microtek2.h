/*******************************************************************************
 * SANE - Scanner Access Now Easy.

   microtek2.h 

   This file (C) 1998, 1999 Bernd Schroeder

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


*******************************************************************************/


#ifndef microtek2_h
#define microtek2_h

#include <sys/types.h>


/******************************************************************************/
/* Miscellaneous defines                                                      */
/******************************************************************************/

#ifndef PATH_MAX
# define PATH_MAX	    1024
#endif

#define MM_PER_INCH         25.4

#define ENDIAN_TYPE(d)          { unsigned  i, test = 0; \
                                  for (i=0; i < sizeof(int); i++ ) \
                                    test += i << (8 * i); \
                                  d = ((char *) &test)[0] == 0 ? 0 : 1; }


#define MIN(a,b)                ((a) < (b)) ? (a) : (b)
#define MAX(a,b)                ((a) > (b)) ? (a) : (b)

#define MICROTEK2_MAJOR         0
#define MICROTEK2_MINOR	        6
#define MICROTEK2_CONFIG_FILE   "microtek2.conf"


/******************************************************************************/
/* defines that ar common to all devices                                      */
/******************************************************************************/

#define MD_RESOLUTION_DEFAULT   72 << SANE_FIXED_SCALE_SHIFT
#define MD_BRIGHTNESS_DEFAULT	100 << SANE_FIXED_SCALE_SHIFT
#define MD_CONTRAST_DEFAULT     100 << SANE_FIXED_SCALE_SHIFT
#define MD_THRESHOLD_DEFAULT    128
#define MD_GAMMA_DEFAULT        SANE_FIX(2.2)
#define MD_SHADOW_DEFAULT       0
#define MD_MIDTONE_DEFAULT      128
#define MD_HIGHLIGHT_DEFAULT    255
#define MD_EXPOSURE_DEFAULT     0
#define M_BRIGHTNESS_DEFAULT	128
#define M_CONTRAST_DEFAULT      128
#define M_SHADOW_DEFAULT        0
#define M_MIDTONE_DEFAULT       128
#define M_HIGHLIGHT_DEFAULT     255
#define M_EXPOSURE_DEFAULT      0
#define M_THRESHOLD_DEFAULT     128


/******************************************************************************/
/* SCSI commands used by the scanner                                          */
/******************************************************************************/

/* INQUIRY */
#define INQ_CMD(d)              d[0] = 0x12; d[1] = 0x00; d[2] = 0x00; \
                                d[3] = 0x00; d[4] = 0x00; d[5] = 0x00
#define INQ_CMD_L               6
#define INQ_ALLOC_L             5      /* first get 5 bytes */
#define INQ_ALLOC_P             4
#define INQ_SET_ALLOC(d,s)      (d)[4] = (s)

#define INQ_GET_INQLEN(d,s)     d = (s)[4]
#define INQ_GET_QUAL(d,s)       d = ((s)[0] >> 5) & 0x07
#define INQ_GET_DEVT(d,s)       d = (s)[0] & 0x1f
#define INQ_GET_VERSION(d,s)    d = (s)[2] & 0x02
#define INQ_VENDOR_L  	        8
#define INQ_GET_VENDOR(d,s)     strncpy(d, &(s)[8], INQ_VENDOR_L); \
                                d[INQ_VENDOR_L] = '\0'
#define INQ_MODEL_L             16
#define INQ_GET_MODEL(d,s)      strncpy(d, &(s)[16], INQ_MODEL_L); \
                                d[INQ_MODEL_L] = '\0'
#define INQ_REV_L      	        4
#define INQ_GET_REV(d,s)        strncpy(d, &(s)[32], INQ_REV_L); \
                                d[INQ_REV_L] = '\0'
#define INQ_GET_MODELCODE(d,s)  d = (s)[36]



/* TEST_UNIT_READY */
#define TUR_CMD(d)              d[0]=0x00; d[1]=0x00; d[2]=0x00; \
                                d[3]=0x00; d[4]=0x00; d[5]=0x00
#define TUR_CMD_L               6


/* READ GAMMA TABLE */
#define RG_CMD(d)               (d)[0] = 0x28; (d)[1] = 0x00; (d)[2] = 0x03; \
                                (d)[3] = 0x00; (d)[4] = 0x00; (d)[5] = 0x00; \
                                (d)[6] = 0x00; (d)[7] = 0x00; (d)[8] = 0x00; \
                                (d)[9] = 0x00
#define RG_CMD_L                10
#define RG_PCORMAC(d,p)         (d)[5] |= (((p) << 7) & 0x80)
#define RG_COLOR(d,p)           (d)[5] |= (((p) << 5) & 0x60)
#define RG_WORD(d,p)            (d)[5] |= ((p) & 0x01)
#define RG_TRANSFERLENGTH(d,p)  (d)[7] = (((p) >> 8) & 0xff); \
                                (d)[8] = ((p) & 0xff) 

/* SEND GAMMA TABLE */
#define SG_SET_CMD(d)           (d)[0] = 0x2a; (d)[1] = 0x00; (d)[2] = 0x03; \
                                (d)[3] = 0x00; (d)[4] = 0x00; (d)[5] = 0x00; \
                                (d)[6] = 0x00; (d)[7] = 0x00; (d)[8] = 0x00; \
                                (d)[9] = 0x00
#define SG_CMD_L                10
#define SG_SET_PCORMAC(d,p)     (d)[5] |= (((p) << 7) & 0x80)
#define SG_SET_COLOR(d,p)       (d)[5] |= (((p) << 5) & 0x60)
#define SG_SET_WORD(d,p)        (d)[5] |= ((p) & 0x01)
#define SG_SET_TRANSFERLENGTH(d,p)  (d)[7] = (((p) >> 8) & 0xff); \
                                    (d)[8] = ((p) & 0xff) 
#define SG_DATA_P               SG_CMD_L


/* READ_SCANNER_ATTRIBUTES */
#define RSA_CMD(d)              d[0]=0x28; d[1]=0x00; d[2]=0x82; d[3]=0x00; \
                                d[4]=0x00; d[5]=0x00; d[6]=0x00; d[7]=0x00; \
                                d[8]=0x28; d[9]=0x00
#define RSA_CMD_L               10
#define RSA_SETMEDIA(d,p)       d[5] |= ((p) & 0x77)             
#define RSA_TRANSFERLENGTH      40

#define RSA_COLOR(d,s)          d = (((s)[0] >> 7) & 0x01)
#define RSA_ONEPASS(d,s)        d = (((s)[0] >> 6) & 0x01)
#define RSA_SCANNERTYPE(d,s)    d = (((s)[0] >> 4) & 0x03)
#define RSA_FEPROM(d,s)         d = (((s)[0] >> 3) & 0x01)
#define RSA_DATAFORMAT(d,s)     d = ((s)[0] & 0x07)
#define RSA_COLORSEQUENCE_L     3
#define RSA_COLORSEQUENCE(d,s)  { \
                                   d[0] = (((s)[1] >> 6) & 0x03); \
                                   d[1] = (((s)[1] >> 4) & 0x03); \
                                   d[2] = (((s)[1] >> 2) & 0x03); \
                                }
#define RSA_DATSEQ(d,s)         d = ((s)[1] & 0x01)
#define RSA_CCDGAP(d,s)         d = (s)[2]               
#define RSA_MAX_XRESOLUTION(d,s)    d = ((s)[3] << 8) + (s)[4]
#define RSA_MAX_YRESOLUTION(d,s)    d = ((s)[5] << 8) + (s)[6]
#define RSA_GEOWIDTH(d,s)       d = ((s)[7] << 8) + (s)[8]
#define RSA_GEOHEIGHT(d,s)      d = ((s)[9] << 8) + (s)[10]
#define RSA_OPTRESOLUTION(d,s)  d = ((s)[11] << 8) + (s)[12]
#define RSA_DEPTH(d,s)          d = (((s)[13] >> 4) & 0x07)
#define RSA_SCANMODE(d,s)       d = (s)[13] & 0x0f
#define RSA_CCDPIXELS(d,s)      d = ((s)[14] << 8) + (s)[15]
#define RSA_LUTCAP(d,s)         d = (s)[16] & 0x1f
#define RSA_DNLDPTRN(d,s)       d = (((s)[17] >> 7) & 0x01)           
#define RSA_GRAINSLCT(d,s)      d = (s)[17] & 0x7f           
#define RSA_SUPPOPT(d,s)        d = (s)[18] & 0xf3           
#define RSA_CALIBWHITE(d,s)     d = ((s)[19] << 24) + ((s)[20] << 16) \
                                  + ((s)[21] << 8) + (s)[22]
#define RSA_CALIBSPACE(d,s)     d = ((s)[23] << 24) + ((s)[24] << 16) \
                                  + ((s)[25] << 8) + (s)[26]
#define RSA_NLENS(d,s)          d = (s)[27]
#define RSA_NWINDOWS(d,s)       d = (s)[28]
#define RSA_SHTRNSFEREQU(d,s)   d = (((s)[29] >> 2) & 0x01) 
#define RSA_SCNBTTN(d,s)        d = (((s)[29] >> 1) & 0x01)
#define RSA_BUFTYPE(d,s)        d = (s)[29] & 0x01


/* READ IMAGE INFORMATION */
#define RII_SET_CMD(d)          (d)[0] = 0x28; (d)[1] = 0x00; (d)[2] = 0x80; \
                                (d)[3] = 0x00; (d)[4] = 0x00; (d)[5] = 0x00; \
                                (d)[6] = 0x00; (d)[7] = 0x00; (d)[8] = 0x10; \
                                (d)[9] = 0x00
#define RII_CMD_L               10
#define RII_RESULT_L            16

#define RII_GET_WIDTHPIXEL(d,s) d = ((s)[0] << 24) + ((s)[1] << 16) \
                                  + ((s)[2] << 8) + (s)[3]
#define RII_GET_WIDTHBYTES(d,s) d = ((s)[4] << 24) + ((s)[5] << 16) \
                                  + ((s)[6] << 8) + (s)[7]
#define RII_GET_HEIGHTLINES(d,s)      d = ((s)[8] << 24) + ((s)[9] << 16) \
                                        + ((s)[10] << 8) + (s)[11]
#define RII_GET_REMAINBYTES(d,s)      d = ((s)[12] << 24) + ((s)[13] << 16) \
                                        + ((s)[14] << 8) + (s)[15]

/* The V300 returns some values in only two bytes */
#define RII_GET_V300_WIDTHPIXEL(d,s)  d = ((s)[0] << 8) + (s)[1]
#define RII_GET_V300_WIDTHBYTES(d,s)  d = ((s)[2] << 8) + (s)[3]
#define RII_GET_V300_HEIGHTLINES(d,s) d = ((s)[4] << 8) + (s)[5]
#define RII_GET_V300_REMAINBYTES(d,s) d = ((s)[6] << 24) + ((s)[7] << 16) \
                                        + ((s)[8] << 8) + (s)[9]


/* READ IMAGE  */
#define RI_SET_CMD(d)           (d)[0] = 0x28; (d)[1] = 0x00; (d)[2] = 0x00; \
                                (d)[3] = 0x00; (d)[4] = 0x00; (d)[5] = 0x00; \
                                (d)[6] = 0x00; (d)[7] = 0x00; (d)[8] = 0x00; \
                                (d)[9] = 0x00
#define RI_CMD_L                10


#define RI_SET_PCORMAC(d,s)     (d)[4] |= (((s) << 7) & 0x80)
#define RI_SET_COLOR(d,s)       (d)[4] |= (((s) << 5) & 0x60)
#define RI_SET_TRANSFERLENGTH(d,s)  (d)[6] = (((s) >> 16) & 0xff); \
                                (d)[7] = (((s) >> 8) & 0xff); \
                                (d)[8] = ((s) & 0xff);

  
/* READ SYSTEM_STATUS  */
#define RSS_CMD(d)              (d)[0] = 0x28; (d)[1] = 0x00; (d)[2] = 0x81; \
                                (d)[3] = 0x00; (d)[4] = 0x00; (d)[5] = 0x00; \
                                (d)[6] = 0x00; (d)[7] = 0x00; (d)[8] = 0x09; \
                                (d)[9] = 0x00
#define RSS_CMD_L               10
#define RSS_RESULT_L            9

#define RSS_NTRACK(s)           (s)[0] & 0x08
#define RSS_NCALIB(s)           (s)[0] & 0x04
#define RSS_TLAMP(s)            (s)[0] & 0x02
#define RSS_FLAMP(s)            (s)[0] & 0x01
#define RSS_RDYMAN(s)           (s)[1] & 0x04
#define RSS_TRDY(s)             (s)[1] & 0x02
#define RSS_FRDY(s)             (s)[1] & 0x01
#define RSS_ADP(s)              (s)[2] & 0x80
#define RSS_DETECT(s)           (s)[2] & 0x40
#define RSS_ADPTIME(s)          (s)[2] & 0x3f
#define RSS_LENSSTATUS(s)       (s)[3]
#define RSS_ALOFF(s)            (s)[4] & 0x80
#define RSS_TIMEREMAIN(s)       (s)[4] & 0x7f
#define RSS_TMACNT(s)           (s)[5] & 0x04
#define RSS_PAPER(s)            (s)[5] & 0x02
#define RSS_ADFCNT(s)           (s)[5] & 0x01
#define RSS_CURRENTMODE(s)      (s)[6] & 0x03 
#define RSS_BUTTONCOUNT(s)      (s)[7]


/* SEND SYSTEM STATUS */
#define SSS_CMD(d)              (d)[0] = 0x2a; (d)[1] = 0x00; (d)[2] = 0x81; \
                                (d)[3] = 0x00; (d)[4] = 0x00; (d)[5] = 0x00; \
                                (d)[6] = 0x00; (d)[7] = 0x00; (d)[8] = 0x09; \
                                (d)[9] = 0x00
#define SSS_CMD_L               10
#define SSS_DATA_L              9

#define SSS_NTRACK(d,p)         d[0] |= (p) & 0x08
#define SSS_NCALIB(d,p)         d[0] |= (p) & 0x04
#define SSS_TLAMP(d,p)          d[0] |= (p) & 0x02
#define SSS_FLAMP(d,p)          d[0] |= (p) & 0x01
#define SSS_RDYMAN(d,p)         d[1] |= (p) & 0x04
#define SSS_TRDY(d,p)           d[1] |= (p) & 0x02
#define SSS_FRDY(d,p)           d[1] |= (p) & 0x01
#define SSS_ADP(d,p)            d[2] |= (p) & 0x80
#define SSS_DETECT(d,p)         d[2] |= (p) & 0x40
#define SSS_ADPTIME(d,p)        d[2] |= (p) & 0x3f
#define SSS_LENSSTATUS(d,p)     d[3] |= (p)
#define SSS_ALOFF(d,p)          d[4] |= (p) & 0x80
#define SSS_TIMEREMAIN(d,p)     d[4] |= (p) & 0x7f
#define SSS_TMACNT(d,p)         d[5] |= (p) & 0x04
#define SSS_PAPER(d,p)          d[5] |= (p) & 0x02
#define SSS_ADFCNT(d,p)         d[5] |= (p) & 0x01
#define SSS_CURRENTMODE(d,p)    d[6] |= (p) & 0x07
#define SSS_BUTTONCOUNT(d,p)    d[6] |= (p)


/* SET WINDOW */
#define SW_CMD(d)               d[0]=0x24; d[1]=0x00; d[2]=0x00; d[3]=0x00; \
                                d[4]=0x00; d[5]=0x00; d[6]=0x00; d[7]=0x00;\
                                d[8]=0x00; d[9]=0x00

#define SW_CMD_L                10
#define SW_HEADER_L             8
#define SW_BODY_L               61
#define SW_CMD_P                0     /* command at postion 0 */
#define SW_HEADER_P             SW_CMD_L
#define SW_BODY_P(n)            SW_CMD_L + SW_HEADER_L + (n) * SW_BODY_L 

/* d: SW_CMD_P, SW_HEADER_P, SW_BODY_P(n) */
#define SW_PARAM_LENGTH(d,p)    (d)[6] = ((p) >> 16) & 0xff; \
                                (d)[7] = ((p) >> 8) & 0xff;\
                                (d)[8] = (p) & 0xff
#define SW_WNDDESCVAL           SW_BODY_L
#define SW_WNDDESCLEN(d,p)      (d)[6] = ((p) >> 8) & 0xff; \
                                (d)[7] = (p) & 0xff  
#define SW_WNDID(d,p)           (d)[0] = (p)
#define SW_XRESDPI(d,p)         (d)[2] = ((p) >> 8) & 0xff; \
                                (d)[3] = (p) & 0xff
#define SW_YRESDPI(d,p)         (d)[4] = ((p) >> 8) & 0xff; \
                                (d)[5] = (p) & 0xff
#define SW_XPOSTL(d,p)          (d)[6] = ((p) >> 24) & 0xff; \
                                (d)[7] = ((p) >> 16) & 0xff; \
                                (d)[8] = ((p) >> 8) & 0xff; \
                                (d)[9] = ((p)) & 0xff
#define SW_YPOSTL(d,p)          (d)[10] = ((p) >> 24) & 0xff; \
                                (d)[11] = ((p) >> 16) & 0xff; \
                                (d)[12] = ((p) >> 8) & 0xff; \
                                (d)[13] = (p) & 0xff   
#define SW_WNDWIDTH(d,p)        (d)[14] = ((p) >> 24) & 0xff; \
                                (d)[15] = ((p) >> 16) & 0xff; \
                                (d)[16] = ((p) >> 8) & 0xff; \
                                (d)[17] = (p) & 0xff   
#define SW_WNDHEIGHT(d,p)       (d)[18] = ((p) >> 24) & 0xff; \
                                (d)[19] = ((p) >> 16) & 0xff; \
                                (d)[20] = ((p) >> 8) & 0xff; \
                                (d)[21] = (p) & 0xff   
#define SW_BRIGHTNESS_M(d,p)    (d)[22] = (p)
#define SW_THRESHOLD(d,p)       (d)[23] = (p)
#define SW_CONTRAST_M(d,p)      (d)[24] = (p)
#define SW_IMGCOMP(d,p)         (d)[25] = (p)
#define SW_BITSPERPIXEL(d,p)    (d)[26] = (p)
#define SW_EXPOSURE_M(d,p)      (d)[27] = (p)
#define SW_EXTHT(d,p)           (d)[28] |= (((p) << 7) & 0x80)
#define SW_INTHTINDEX(d,p)      (d)[28] |= ((p) & 0x7f)
#define SW_RIF(d,p)             (d)[29] |= (((p) << 7) & 0x80)
#define SW_LENS(d,p)            (d)[30] = (p)
#define SW_INFINITE(d,p)        (d)[31] |= (((p) << 7) & 0x80)
#define SW_STAY(d,p)            (d)[31] |= (((p) << 6) & 0x40)
#define SW_RAWDAT(d,p)          (d)[31] |= (((p) << 5) & 0x20)
#define SW_QUALITY(d,p)         (d)[31] |= (((p) << 4) & 0x10)
#define SW_FASTSCAN(d,p)        (d)[31] |= (((p) << 3) & 0x08)
#define SW_MEDIA(d,p)           (d)[31] |= ((p) & 0x07)
#define SW_SHADOW_M(d,p)        (d)[40] = (p)
#define SW_MIDTONE_M(d,p)       (d)[41] = (p)
#define SW_HIGHLIGHT_M(d,p)     (d)[42] = (p)
#define SW_BRIGHTNESS_R(d,p)    (d)[43] = (p)
#define SW_CONTRAST_R(d,p)      (d)[44] = (p)
#define SW_EXPOSURE_R(d,p)      (d)[45] = (p)
#define SW_SHADOW_R(d,p)        (d)[46] = (p)
#define SW_MIDTONE_R(d,p)       (d)[47] = (p)
#define SW_HIGHLIGHT_R(d,p)     (d)[48] = (p)
#define SW_BRIGHTNESS_G(d,p)    (d)[49] = (p)
#define SW_CONTRAST_G(d,p)      (d)[50] = (p)
#define SW_EXPOSURE_G(d,p)      (d)[51] = (p)
#define SW_SHADOW_G(d,p)        (d)[52] = (p)
#define SW_MIDTONE_G(d,p)       (d)[53] = (p)
#define SW_HIGHLIGHT_G(d,p)     (d)[54] = (p)
#define SW_BRIGHTNESS_B(d,p)    (d)[55] = (p)
#define SW_CONTRAST_B(d,p)      (d)[56] = (p)
#define SW_EXPOSURE_B(d,p)      (d)[57] = (p)
#define SW_SHADOW_B(d,p)        (d)[58] = (p)
#define SW_MIDTONE_B(d,p)       (d)[59] = (p)
#define SW_HIGHLIGHT_B(d,p)     (d)[60] = (p)


/* READ IMAGE STATUS */
#define RIS_SET_CMD(d)          (d)[0] = 0x28; (d)[1] = 0x00; (d)[2] = 0x83; \
                                (d)[3] = 0x00; (d)[4] = 0x00; (d)[5] = 0x00; \
                                (d)[6] = 0x00; (d)[7] = 0x00; (d)[8] = 0x00; \
                                (d)[9] = 0x00
#define RIS_CMD_L               10 
#define RIS_SET_PCORMAC(d,p)    (d)[4] |= (((p) << 7) & 0x80)
#define RIS_SET_COLOR(d,p)      (d)[4] |= (((p) << 5) & 0x60)


/* REQUEST SENSE */
#define RQS_CMD(d)              (d)[0] = 0x03; (d)[1] = 0x00; (d)[2] = 0x00; \
                                (d)[3] = 0x00; (d)[5] = 0x00
#define RQS_CMD_L               6
#define RQS_ALLOCLENGTH(d,p)    (d)[4] = (p)

#define RQS_LENGTH(s)           (s)[7] + 7

#define RQS_SENSEKEY_NOSENSE    0x00
#define RQS_SENSEKEY_HWERR      0x04
#define RQS_SENSEKEY_ILLEGAL    0x05
#define RQS_SENSEKEY_VENDOR     0x09
#define RQS_SENSEKEY(s)         ((s)[2] & 0x0f)

#define RQS_INFO(s)             ((s)[3] << 24) + ((s)[4] << 16) \
                                + ((s)[5] << 8) + ((s)[6])
#define RQS_ASL(s)              (s)[7]
#define RQS_REMAINBYTES(s)      ((s)[8] << 24) + ((s)[9] << 16) \
                                + ((s)[10] << 8) + ((s)[11])
#define RQS_ASC(s)              (s)[12]
/* ASC == 0x40 */
#define RQS_ASCQ_CPUERR         0x81
#define RQS_ASCQ_SRAMERR        0x82
#define RQS_ASCQ_DRAMERR        0x84
#define RQS_ASCQ_DCOFF          0x88
#define RQS_ASCQ_GAIN           0x90
#define RQS_ASCQ_POS            0xa0
#define RQS_ASCQ(s)             (s)[13]

#define RQS_ASINFO(s)           &((s)[18])
#define RQS_ASINFOLENGTH(s)     (s)[7] - 11

/******************************************************************************/
/* enumeration of Option Descriptors                                          */
/******************************************************************************/

enum Microtek2_Option
{
  /*0*/
  OPT_NUM_OPTS = 0,
  
  /*1*/OPT_MODE_GROUP,
  OPT_SOURCE,
  OPT_MODE,           
  OPT_RESOLUTION,    
  OPT_X_RESOLUTION,
  OPT_Y_RESOLUTION,
  OPT_RESOLUTION_BIND,
  OPT_DISABLE_BACKTRACK,
  OPT_PREVIEW,
  
  /*10*/ OPT_GEOMETRY_GROUP,  
  OPT_TL_X,             /* top-left x */
  OPT_TL_Y,             /* top-left y */
  OPT_BR_X,             /* bottom-right x */
  OPT_BR_Y,             /* bottom-right y */ 
  
  /*15*/  OPT_ENHANCEMENT_GROUP,
  OPT_BRIGHTNESS, 
  OPT_CONTRAST, 
  OPT_THRESHOLD,
  OPT_HALFTONE,

  /*20*/ OPT_GAMMA_GROUP,
  OPT_GAMMA_MODE, 
  OPT_GAMMA_SCALAR,
  OPT_GAMMA_SCALAR_R,
  OPT_GAMMA_SCALAR_G, 
  OPT_GAMMA_SCALAR_B,
  OPT_GAMMA_CUSTOM,
  OPT_GAMMA_CUSTOM_R,
  OPT_GAMMA_CUSTOM_G,
  OPT_GAMMA_CUSTOM_B,
  OPT_GAMMA_BIND,

  /*31*/ OPT_SMH_GROUP,
  /* these options must appear in exactly this order, */
  /* sane_control_option relies on it */
  OPT_CHANNEL,
  OPT_SHADOW,   
  OPT_MIDTONE, 
  OPT_HIGHLIGHT,  
  OPT_SHADOW_R,   
  OPT_MIDTONE_R,
  OPT_HIGHLIGHT_R,  
  OPT_SHADOW_G,   
  OPT_MIDTONE_G, 
  OPT_HIGHLIGHT_G,  
  OPT_SHADOW_B,  
  OPT_MIDTONE_B, 
  OPT_HIGHLIGHT_B,  
  OPT_EXPOSURE,
  OPT_EXPOSURE_R,
  OPT_EXPOSURE_G,
  OPT_EXPOSURE_B,

  /*49*/ NUM_OPTIONS
};


/******************************************************************************/
/* value structure for scanner options                                        */
/******************************************************************************/

typedef union {
  SANE_Word w;
  SANE_Word *wa;		/* word array */
  SANE_String s;
} Microtek2_Option_Value;



/******************************************************************************/
/* scanner hardware info (as discovered by INQUIRY and READ ATTRIBUTES)       */
/******************************************************************************/

typedef struct Microtek2_Info {
    /* from inquiry */
    SANE_Byte device_qualifier;
#define MI_DEVTYPE_SCANNER             0x06
    SANE_Byte device_type;
#define MI_SCSI_II_VERSION             0x02
    SANE_Byte scsi_version;
    SANE_Char vendor[INQ_VENDOR_L + 1];
    SANE_Char model[INQ_MODEL_L + 1];
    SANE_Char revision[INQ_REV_L + 1];
    SANE_Byte model_code;
    /* from read scanner attributes */
    /* #define MI_HAS_COLOR            SANE_TRUE */
    SANE_Bool color;
#define MI_HAS_ONEPASS                 SANE_TRUE
    SANE_Bool onepass;
    /* the following 3 defines must correspond to byte 31 of SET WINDOW cmd */
#define MI_TYPE_FLATBED                0x01
#define MI_TYPE_SHEEDFEED              0x02
#define MI_TYPE_TRANSPARENCY           0x03
    SANE_Byte scanner_type;
#define MI_HAS_FEPROM                  SANE_TRUE
    SANE_Bool feprom;
    /* MI_DATA_FORMAT_X must correspond to Byte 0 in READ SCANNER ATTRIBUTE */
#define MI_DATAFMT_CHUNKY              1
#define MI_DATAFMT_LPLCONCAT           2
#define MI_DATAFMT_LPLSEGREG           3
#define MI_DATAFMT_WORDCHUNKY          5
    SANE_Byte data_format;
#define MI_COLSEQ_RED                  0
#define MI_COLSEQ_GREEN                1
#define MI_COLSEQ_BLUE                 2
#define MI_COLSEQ_ILLEGAL              3
    SANE_Byte color_sequence[RSA_COLORSEQUENCE_L];
    SANE_Byte ccd_gap;
    SANE_Int max_xresolution;
    SANE_Int max_yresolution;
    SANE_Int geo_width;
    SANE_Int geo_height;
    SANE_Int opt_resolution;
#define MI_HASDEPTH_NIBBLE             1
#define MI_HASDEPTH_10                 2
#define MI_HASDEPTH_12                 4
    SANE_Byte depth;
#define MI_HASMODE_LINEART             1
#define MI_HASMODE_HALFTONE            2
#define MI_HASMODE_GRAY                4
#define MI_HASMODE_COLOR               8
    SANE_Byte scanmode;
    SANE_Int ccd_pixels;
#define MI_LUTCAP_NONE(d)              (((d) & 0x1f) == 0)
#define MI_LUTCAP_256B                 1
#define MI_LUTCAP_1024B                2
#define MI_LUTCAP_1024W                4
#define MI_LUTCAP_4096B                8
#define MI_LUTCAP_4096W                16
    SANE_Byte lut_cap;
#define MI_HAS_DNLDPTRN                SANE_TRUE
    SANE_Bool has_dnldptrn;         
    SANE_Byte grain_slct;
#define MI_OPTDEV_ADF                  0x01
#define MI_OPTDEV_TMA                  0x02
#define MI_OPTDEV_ADP                  0x10
#define MI_OPTDEV_APS                  0x20
#define MI_OPTDEV_STRIPE               0x40
#define MI_OPTDEV_SLIDE                0x80
    SANE_Byte option_device;
    SANE_Int calib_white;
    SANE_Int calib_space;
    SANE_Byte nlens;
    SANE_Byte nwindows;
#define MI_SH_TRNSFEREQU_0             0x00
#define MI_SH_TRNSFEREQU_1             0x01
#define MI_SH_TRNSFEREQU_2             0x02
#define MI_SH_TRNSFEREQU_3             0x03
#define MI_SH_TRNSFEREQU_4             0x04
    SANE_Byte shtrnsferequ;
#define MI_HAS_SCNBTTN                 SANE_TRUE
    SANE_Bool scnbuttn;
#define MI_HAS_PIPOBUF                 SANE_TRUE
    SANE_Bool buftype;
} Microtek2_Info;






/******************************************************************************/
/* device structure (one for each device in config file)                      */
/******************************************************************************/

typedef struct Microtek2_Device {
    struct Microtek2_Device *next;        /* next, for linked list */

#define MD_SOURCE_FLATBED             0
#define MD_SOURCE_ADF                 1
#define MD_SOURCE_TMA                 2
#define MD_SOURCE_SLIDE               3
#define MD_SOURCE_STRIPE              4
    Microtek2_Info info[5];               /* detailed scanner spec */
    SANE_Device sane;                     /* SANE generic device block */
    char name[PATH_MAX];                  /* name from config file */  

    SANE_Int *custom_gamma_table[4];      /* used for the custom gamma */
                                          /* values before a scan starts */
    /* the following two are derived from lut_cap */
    int max_lut_size;                     /* in bytes */
    int lut_entry_size;                   /* word or byte transfer in LUT */
    u_int8_t scan_source;    
    double revision;

    /* the following structure describes the device status */
#define MD_TLAMP_ON                   2
#define MD_FLAMP_ON                   1
#define MD_NCALIB_ON                  4
#define MD_NTRACK_ON                  8
#define MD_CURRENT_MODE_FLATBED       0
    struct {
      u_int8_t ntrack;        
      u_int8_t ncalib;
      u_int8_t tlamp;
      u_int8_t flamp;
      u_int8_t rdyman;
      u_int8_t trdy;
      u_int8_t frdy;
      u_int8_t adp;
      u_int8_t detect;
      u_int8_t adptime;
      u_int8_t lensstatus;
      u_int8_t aloff;
      u_int8_t timeremain;
      u_int8_t tmacnt;
      u_int8_t paper;
      u_int8_t adfcnt;
      u_int8_t currentmode;
      u_int8_t buttoncount;
    } status;


#define MD_MODESTRING_NUMS             9
#define MD_MODESTRING_COLOR36          "36-bit Color"
#define MD_MODESTRING_COLOR30          "30-bit Color"
#define MD_MODESTRING_COLOR24          "24-bit Color"
#define MD_MODESTRING_GRAY12           "12-bit Gray"
#define MD_MODESTRING_GRAY10           "10-bit Gray"
#define MD_MODESTRING_GRAY8            "8-bit Gray"
#define MD_MODESTRING_GRAY2            "2-bit Gray"
#define MD_MODESTRING_HALFTONE         "Halftone"
#define MD_MODESTRING_LINEART          "LineArt"
    SANE_String_Const scanmode_list[MD_MODESTRING_NUMS + 1];

#define MD_SOURCESTRING_NUMS          5
#define MD_SOURCESTRING_FLATBED       "Flatbed      "
#define MD_SOURCESTRING_ADF           "ADF"
#define MD_SOURCESTRING_TMA           "TMA"
#define MD_SOURCESTRING_STRIPE        "Filmstrip"
#define MD_SOURCESTRING_SLIDE         "Slide"
    SANE_String_Const scansource_list[MD_SOURCESTRING_NUMS + 1];

#define MD_HALFTONE_NUMS              12 
#define MD_HALFTONE0                  "53-dot screen (53 gray levels)"
#define MD_HALFTONE1                  "Horiz. screen (65 gray levels)"
#define MD_HALFTONE2                  "Vert. screen (65 gray levels)"
#define MD_HALFTONE3                  "Mixed page (33 gray levels)"
#define MD_HALFTONE4                  "71-dot screen (29 gray levels)"
#define MD_HALFTONE5                  "60-dot #1 (26 gray levels)"
#define MD_HALFTONE6                  "60-dot #2 (26 gray levels)"
#define MD_HALFTONE7                  "Fine detail #1 (17 gray levels)"
#define MD_HALFTONE8                  "Fine detail #2 (17 gray levels)"
#define MD_HALFTONE9                  "Slant line (17 gray levels)"
#define MD_HALFTONE10                 "Posterizing (10 gray levels)"
#define MD_HALFTONE11                 "High Contrast (5 gray levels)"
    SANE_String_Const halftone_mode_list[MD_HALFTONE_NUMS + 1];

#define MD_CHANNEL_NUMS               4
#define MD_CHANNEL_MASTER             "Master"
#define MD_CHANNEL_RED                "Red"
#define MD_CHANNEL_GREEN              "Green"
#define MD_CHANNEL_BLUE               "Blue"
    SANE_String_Const channel_list[MD_CHANNEL_NUMS + 1];
    
#define MD_GAMMAMODE_NUMS             3
#define MD_GAMMAMODE_LINEAR           "None"
#define MD_GAMMAMODE_SCALAR           "Scalar"
#define MD_GAMMAMODE_CUSTOM           "Custom"
    SANE_String_Const gammamode_list[MD_GAMMAMODE_NUMS + 1];

    SANE_Range x_res_range_dpi;           /* X resolution in dpi */ 
    SANE_Range y_res_range_dpi;           /* Y resolution in dpi */ 
    SANE_Range x_range_mm;                /* scan width in mm */
    SANE_Range y_range_mm;                /* scan height in mm */
    SANE_Range percentage_range;          /* for brightness, shadow, ... */
    SANE_Range custom_gamma_range;        /* for custom gamma values */
    SANE_Range scalar_gamma_range;        /* for scalar gamma values */
    SANE_Range shadow_range;              /* shadow of blue channel */
    SANE_Range midtone_range;             /* midtone shadow of blue channel */
    SANE_Range exposure_range;            /* for lengthening exposure time */
    SANE_Range highlight_range;           /* highlight of master channel */
    SANE_Range threshold_range;           /* 1 - 255 */
} Microtek2_Device;



/******************************************************************************/
/* scanner structure (one for each device in use)                             */
/*       ....all the state needed to define a scan request                    */
/******************************************************************************/

typedef struct Microtek2_Scanner {
    struct Microtek2_Scanner *next;             /* for linked list */
    Microtek2_Device *dev;                      /* raw device info */
    Microtek2_Option_Value val[NUM_OPTIONS + 1]; /* option values for session */
    SANE_Parameters params;   /* format, lastframe, lines, depth, ppl, bpl */
    SANE_Option_Descriptor sod[NUM_OPTIONS + 1]; /* option list for session */
    
    u_int8_t *gamma_table;

/* the following defines must correspond to byte 25 of SET WINDOW body */
#define MS_MODE_LINEART                0x00
#define MS_MODE_HALFTONE               0x01
#define MS_MODE_GRAY                   0x02
#define MS_MODE_COLOR                  0x05

/* the following defines must correspond to byte 31 of SET WINDOW body */
#define MS_SOURCE_FLATBED              0x00
#define MS_SOURCE_ADF                  0x01
#define MS_SOURCE_TMA                  0x02
#define MS_SOURCE_STRIPE               0x05
#define MS_SOURCE_SLIDE                0x06

    SANE_Int mode;
    SANE_Int depth;
    SANE_Int x_resolution_dpi;
    SANE_Int y_resolution_dpi;
    SANE_Int x1_dots;            /* x-position in units of optical resolution */
    SANE_Int y1_dots;            /* same for y-position */
    SANE_Int width_dots;         /* scan width in units of optical resolution */
    SANE_Int height_dots;        /* same for height */
    u_int8_t brightness_m;
    u_int8_t contrast_m;
    u_int8_t exposure_m;
    u_int8_t shadow_m;
    u_int8_t midtone_m;
    u_int8_t highlight_m;
    u_int8_t brightness_r;
    u_int8_t contrast_r;
    u_int8_t exposure_r;
    u_int8_t shadow_r;
    u_int8_t midtone_r;
    u_int8_t highlight_r;
    u_int8_t brightness_g;
    u_int8_t contrast_g;
    u_int8_t exposure_g;
    u_int8_t shadow_g;
    u_int8_t midtone_g;
    u_int8_t highlight_g;
    u_int8_t brightness_b;
    u_int8_t contrast_b;
    u_int8_t exposure_b;
    u_int8_t shadow_b;
    u_int8_t midtone_b;
    u_int8_t highlight_b;
    u_int8_t threshold;
    SANE_Bool use_external_ht;
    SANE_Byte internal_ht_index;
    u_int8_t rawdat;
    SANE_Bool quality;
    SANE_Bool fastscan;
    SANE_Byte scan_source;
    int no_backtracking;
    int current_pass;              /* current pass if 3-pass scan */ 
    int lut_size;                  /* size of gamma lookup table */
    int lut_entry_size;            /* size of one entry in lookup table */
    u_int16_t lut_size_bytes;      /* size of LUT in bytes */
    u_int8_t word;                 /* word transfer, used in some read cmds */ 
    /* MS _COLOR_X must correspond to color field in READ IMAGE STATUS */
#define MS_COLOR_RED                   0
#define MS_COLOR_GREEN                 1
#define MS_COLOR_BLUE                  2
#define MS_COLOR_ALL                   3
    u_int8_t current_color;        /* for gamma clac. and 3-pass scanners */
    u_int32_t ppl;                 /* pixels per line as returned by RII */
    u_int32_t bpl;                 /* bytes per line as returned by RII */
    u_int32_t remaining_bytes;     /* remaining bytes as returned by RII */
    u_int32_t real_remaining_bytes;/* bytes to transfer to the frontend */
    u_int32_t real_bpl;            /* bytes to transfer to the frontend */
    SANE_Int src_remaining_lines;  /* remaining lines to read */
    SANE_Int src_lines_to_read;    /* actual number of lines read */
    SANE_Int src_max_lines;        /* maximum number of lines that fit */
                                   /* into the scsi buffer */
                                   /* sent to the frontend */
    int bits_per_pixel_in;         /* bits per pixel transferred from scanner */
    int bits_per_pixel_out;        /* bits per pixel transf. to frontend */
    unsigned long src_buffer_size; /* size of this buffer */
    int transfer_length;           /* transfer length for RI command */
    struct {
      u_int8_t *src_buffer[2];     /* two buffers because of CCD gap */
      u_int8_t *src_buf;
      int current_src;
      unsigned int free_lines;
      unsigned int free_max_lines;
      u_int8_t *current_pos[3];    /* actual position in the source buffer */
      int planes[2][3];            /* # of red, green, blue planes in the */
                                   /* current source buffer and leftover */
                                   /* planes from previous "read image" */
      } buf;

    SANE_Bool onepass;

    int scanning;             /* true == between sane_start & sane_read=EOF */
    int cancelled;
    int sfd;                  /* SCSI filedescriptor */
    int fd[2];                /* file descriptors for pipe */
    pid_t pid;                /* pid of child process */
    FILE *fp;

} Microtek2_Scanner;

/******************************************************************************/
/* Description of options not included in saneopts.h                          */
/******************************************************************************/

#define M_NAME_SCANSOURCE      "scan-source"
#define M_TITLE_SCANSOURCE     "Scan source"
#define M_DESC_SCANSOURCE      "Selects the scan source, i.e. flatbed," \
                               " transparency media adapter (TMA) or" \
                               " automatic document feeder (ADF)."
   
#define M_NAME_NOBACKTRACK     "no-backtracking"
#define M_TITLE_NOBACKTRACK    "Disable backtracking"
#define M_DESC_NOBACKTRACK     "If checked the scanner does not perform" \
                               " backtracking."
   
#define M_NAME_GAMMA_MODE      "gamma-correction"
#define M_TITLE_GAMMA_MODE     "Gamma correction"
#define M_DESC_GAMMA_MODE      "Selects the gamma correction mode."
   
#define M_NAME_GAMMA_BIND      "bind-gamma"
#define M_TITLE_GAMMA_BIND     "Bind gamma"
#define M_DESC_GAMMA_BIND      "Use same gamma values for all colour channels."

#define M_NAME_GAMMA_SCALAR    "scalar-gamma"
#define M_TITLE_GAMMA_SCALAR   "Scalar gamma"
#define M_DESC_GAMMA_SCALAR    "Selects a value for scalar gamma correction."

#define M_NAME_GAMMA_SCALAR_R  "scalar-gamma-r"
#define M_TITLE_GAMMA_SCALAR_R "Scalar gamma red"
#define M_DESC_GAMMA_SCALAR_R  "Selects a value for scalar gamma correction" \
                               " (red channel)"

#define M_NAME_GAMMA_SCALAR_G  "scalar-gamma-g"
#define M_TITLE_GAMMA_SCALAR_G "Scalar gamma green"
#define M_DESC_GAMMA_SCALAR_G  "Selects a value for scalar gamma correction" \
                               " (green channel)"

#define M_NAME_GAMMA_SCALAR_B  "scalar-gamma-b"
#define M_TITLE_GAMMA_SCALAR_B "Scalar gamma blue"
#define M_DESC_GAMMA_SCALAR_B  "Selects a value for scalar gamma correction" \
                               " (blue channel)"

#define M_NAME_CHANNEL         "channel"
#define M_TITLE_CHANNEL        "Channel"
#define M_DESC_CHANNEL         "Selects the colour band, \"Master\" means," \
                               " that all colours are affected."

#define M_NAME_MIDTONE         "midtone"
#define M_TITLE_MIDTONE        "Midtone"
#define M_DESC_MIDTONE         "Selects which radiance level should be" \
                               " considered \"50 % gray\"."

#define M_NAME_MIDTONE_R       "midtone-r"
#define M_TITLE_MIDTONE_R      "Midtone for red"
#define M_DESC_MIDTONE_R       "Selects which radiance level should be" \
                               " considered \"50 % red\"."

#define M_NAME_MIDTONE_G       "midtone-g"
#define M_TITLE_MIDTONE_G      "Midtone for green"
#define M_DESC_MIDTONE_G       "Selects which radiance level should be" \
                               " considered \"50 % green\"."

#define M_NAME_MIDTONE_B       "midtone-b"
#define M_TITLE_MIDTONE_B      "Midtone for blue"
#define M_DESC_MIDTONE_B       "Selects which radiance level should be" \
                               " considered \"50 % blue\"."

/******************************************************************************/
/* Function prototypes                                                        */
/******************************************************************************/

static SANE_Status
add_device_list(SANE_String_Const, Microtek2_Device **);

static SANE_Status
attach(Microtek2_Device *);

static SANE_Status
attach_one (const char *);

static SANE_Status
calculate_gamma(Microtek2_Scanner *, u_int8_t *, int);

static SANE_Status
cancel_scan(Microtek2_Scanner *);

static SANE_Status
check_inquiry(Microtek2_Info *, SANE_String *);

static SANE_Status
chunky_copy_pixels(u_int8_t *, u_int32_t, int, FILE *);

static SANE_Status
chunky_proc_data(Microtek2_Scanner *);

static void
cleanup_scanner(Microtek2_Scanner *);

#if 0
static SANE_Status
do_dummy_scan(Microtek2_Scanner *);
#endif

static SANE_Status
dump_area(u_int8_t *, int, char *);

static SANE_Status
dump_area2(u_int8_t *, int, char *);

static SANE_Status 
dump_attributes(Microtek2_Info *);

static SANE_Status
get_scan_mode_and_depth(Microtek2_Scanner *, int *, SANE_Int *, int *, int *);

static SANE_Status
get_scan_parameters(Microtek2_Scanner *);

static SANE_Status
get_lut_size(Microtek2_Info *, int *, int *);

static SANE_Status
init_options(Microtek2_Scanner *, u_int8_t);

static SANE_Status
lplconcat_copy_pixels(u_int8_t **, u_int32_t, int, FILE *);

static SANE_Status                
lplconcat_proc_data(Microtek2_Scanner *ms);

static size_t
max_string_size (const SANE_String_Const * /* strings[] */);

static SANE_Status                
gray_proc_data(Microtek2_Scanner *);

static SANE_Status                
proc_onebit_data(Microtek2_Scanner *);

static SANE_Status 
reader_process(Microtek2_Scanner *);

static SANE_Status                
restore_gamma_options(SANE_Option_Descriptor *, Microtek2_Option_Value *);

static SANE_Status
segreg_copy_pixels(u_int8_t **, u_int32_t, int, FILE *);

static SANE_Status                
segreg_proc_data(Microtek2_Scanner *ms);

static SANE_Status                
set_option_dependencies(SANE_Option_Descriptor *, Microtek2_Option_Value *); 

static RETSIGTYPE
sigterm_handler (int);

static SANE_Status
wordchunky_copy_pixels(u_int8_t *, u_int32_t, int, FILE *);

static SANE_Status
wordchunky_proc_data(Microtek2_Scanner *);
              


/******************************************************************************/
/* Function prototypes for basic SCSI commands                                */
/******************************************************************************/

static SANE_Status
scsi_inquiry(Microtek2_Info *, char *);

static SANE_Status
scsi_read_attributes(Microtek2_Info *, char *, u_int8_t);

/* currently not used */
/* static SANE_Status
scsi_read_gamma(Microtek2_Scanner *, int); */

static SANE_Status
scsi_read_image(Microtek2_Scanner *);

static SANE_Status
scsi_read_image_info(Microtek2_Scanner *);

static SANE_Status
scsi_read_image_status(Microtek2_Scanner *);

static SANE_Status
scsi_read_system_status(Microtek2_Device *, int);

/* currently not used */
/* static SANE_Status
scsi_request_sense(Microtek2_Scanner *); */

static SANE_Status
scsi_send_gamma(Microtek2_Scanner *);

static SANE_Status
scsi_send_system_status(Microtek2_Device *, int);

static SANE_Status
scsi_sense_handler (int, u_char *, void *);

static SANE_Status
scsi_test_unit_ready(Microtek2_Device *);

static SANE_Status
scsi_set_window(Microtek2_Scanner *, int);

static SANE_Status
scsi_wait_for_image(Microtek2_Scanner *);

#endif
