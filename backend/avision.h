/*******************************************************************************
 * SANE - Scanner Access Now Easy.

   avision.h 

   This file (C) 1999, 2000  Meino Christian Cramer and Rene Rebe

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

   *****************************************************************************

   This file implements a SANE backend for the Avision AV 630CS scanner with
   SCSI-2 command set.

   (feedback to:  mccramer@s.netic.de and rene.rebe@myokay.net)

   Very much thanks to:
     Avision INC for the documentation we got! ;-)
     Gunter Wagner for some fixes and the transparency option

   *****************************************************************************/

#ifndef avision_h
#define avision_h

#include <sys/types.h>


enum Avision_Option
{
    OPT_NUM_OPTS = 0,
    
    OPT_MODE_GROUP,
    OPT_MODE,
#define OPT_MODE_DEFAULT 3
    OPT_RESOLUTION,
#define OPT_RESOLUTION_DEFAULT 300
    OPT_SPEED,
    OPT_PREVIEW,
   
    OPT_GEOMETRY_GROUP,
    OPT_TL_X,			/* top-left x */
    OPT_TL_Y,			/* top-left y */
    OPT_BR_X,			/* bottom-right x */
    OPT_BR_Y,			/* bottom-right y */

    OPT_ENHANCEMENT_GROUP,
    OPT_BRIGHTNESS,
    OPT_CONTRAST,
    OPT_THRESHOLD,
    OPT_QSCAN,
    OPT_QCALIB,
    OPT_TRANS,                  /* Transparency Mode */
#if 0
    OPT_CUSTOM_GAMMA,		/* use custom gamma tables? */
    /* The gamma vectors MUST appear in the order gray, red, green,
       blue.  */
    OPT_GAMMA_VECTOR,
    OPT_GAMMA_VECTOR_R,
    OPT_GAMMA_VECTOR_G,
    OPT_GAMMA_VECTOR_B,

    OPT_HALFTONE_DIMENSION,
    OPT_HALFTONE_PATTERN,
#endif

    /* must come last: */
    NUM_OPTIONS
};


typedef union
{
    SANE_Word w;
    SANE_Word *wa;		/* word array */
    SANE_String s;
} Option_Value;

typedef struct Avision_Dimensions
{
    long tlx;
    long tly;
    long brx;
    long bry;
    long wid;
    long len;
    long pixelnum;
    long linenum;
    int resx;
    int rexy;
    int res;
} Avision_Dimensions;

typedef struct Avision_Device
{
    struct Avision_Device *next;
    SANE_Device sane;
    SANE_Range dpi_range;
    SANE_Range x_range;
    SANE_Range y_range;
    SANE_Range speed_range;
    unsigned flags;
} Avision_Device;

typedef struct Avision_Scanner
{
    /* all the state needed to define a scan request: */
    struct Avision_Scanner *next;
    
    SANE_Option_Descriptor opt[NUM_OPTIONS];
    Option_Value val[NUM_OPTIONS];
    SANE_Int gamma_table[4][256];
    
    int scanning;
    int pass;			/* pass number */
    int line;			/* current line number */
    SANE_Parameters params;
    
    /* Parsed option values and variables that are valid only during
       actual scanning: */
    int mode;
    Avision_Dimensions avdimen;  /* Used for internal calculationg */
    
    int fd;			/* SCSI filedescriptor */
    pid_t reader_pid;		/* process id of reader */
    int pipe;			/* pipe to reader process */
    
    /* scanner dependent/low-level state: */
    Avision_Device *hw;
} Avision_Scanner;

#define AV_ADF_ON     0x80

#define AV_QSCAN_ON   0x10
#define AV_QCALIB_ON  0x08
#define AV_TRANS_ON   0x80
#define AV_INVERSE_ON 0x20

#define THRESHOLDED 0
#define DITHERED 1
#define GREYSCALE 2
#define TRUECOLOR 3

/* request sense */
#define VALID                       (SANE_Byte) (0x01<<7)
#define VALID_BYTE                  (SANE_Int ) (0x00)
#define ERRCODESTND                 (SANE_Byte) (0x70)
#define ERRCODEAV                   (SANE_Byte) (0x7F)
#define ERRCODEMASK                 (SANE_Byte) (0x7F)
#define ERRCODE_BYTE                (SANE_Int ) (0x00) 
#define FILMRK                      (SANE_Byte) (0x00)
#define EOSMASK                     (SANE_Byte) (0x01<<6)
#define EOS_BYTE                    (SANE_Int ) (0x02)
#define INVALIDLOGICLEN             (SANE_Byte) (0x01<<5)
#define ILI_BYTE                    (SANE_Int ) (0x02)
#define SKSVMASK                    (SANE_Byte) (0x01<<7)
#define SKSV_BYTE                   (SANE_Int ) (0x0F)
#define BPVMASK                     (SANE_Byte) (0x01<<3)
#define BPV_BYTE                    (SANE_Int ) (0x0F)
#define CDMASK                      (SANE_Byte) (0x01<<6)
#define CD_ERRINDATA                (SANE_Byte) (0x00)
#define CD_ERRINPARAM               (SANE_Byte) (0x01)
#define CD_BYTE                     (SANE_Int ) (0x0F)
#define BITPOINTERMASK              (SANE_Byte) (0x07)
#define BITPOINTER_BYTE             (SANE_Int ) (0x0F)
#define BYTEPOINTER_BYTE1           (SANE_Int ) (0x10)
#define BYTEPOINTER_BYTE2           (SANE_Int ) (0x11)
#define SENSEKEY_BYTE               (SANE_Int ) (0x02)
#define SENSEKEY_MASK               (SANE_Byte) (0x02)
#define NOSENSE                     (SANE_Byte) (0x00)
#define NOTREADY                    (SANE_Byte) (0x02)
#define MEDIUMERROR                 (SANE_Byte) (0x03)
#define HARDWAREERROR               (SANE_Byte) (0x04)
#define ILLEGALREQUEST              (SANE_Byte) (0x05)
#define UNIT_ATTENTION              (SANE_Byte) (0x06)
#define VENDORSPEC                  (SANE_Byte) (0x09)
#define ABORTEDCOMMAND              (SANE_Byte) (0x0B)
#define ASC_BYTE                    (SANE_Int ) (0x0C)
#define ASCQ_BYTE                   (SANE_Int ) (0x0D)
#define ASCFILTERPOSERR             (SANE_Byte) (0xA0)
#define ASCQFILTERPOSERR            (SANE_Byte) (0x01)
#define ASCADFPAPERJAM              (SANE_Byte) (0x80)
#define ASCQADFPAPERJAM             (SANE_Byte) (0x01)
#define ASCADFCOVEROPEN             (SANE_Byte) (0x80)
#define ASCQADFCOVEROPEN            (SANE_Byte) (0x02)
#define ASCADFPAPERCHUTEEMPTY       (SANE_Byte) (0x80)
#define ASCQADFPAPERCHUTEEMPTY      (SANE_Byte) (0x03)
#define ASCINTERNALTARGETFAIL       (SANE_Byte) (0x44)
#define ASCQINTERNALTARGETFAIL      (SANE_Byte) (0x00)
#define ASCSCSIPARITYERROR          (SANE_Byte) (0x47)
#define ASCQSCSIPARITYERROR         (SANE_Byte) (0x00)
#define ASCINVALIDCOMMANDOPCODE     (SANE_Byte) (0x20)
#define ASCQINVALIDCOMMANDOPCODE    (SANE_Byte) (0x00)
#define ASCINVALIDFIELDCDB          (SANE_Byte) (0x24)
#define ASCQINVALIDFIELDCDB         (SANE_Byte) (0x00)
#define ASCLUNNOTSUPPORTED          (SANE_Byte) (0x25)
#define ASCQLUNNOTSUPPORTED         (SANE_Byte) (0x00)
#define ASCINVALIDFIELDPARMLIST     (SANE_Byte) (0x26)
#define ASCQINVALIDFIELDPARMLIST    (SANE_Byte) (0x00)
#define ASCINVALIDCOMBINATIONWIN    (SANE_Byte) (0x2C)
#define ASCQINVALIDCOMBINATIONWIN   (SANE_Byte) (0x02)
#define ASCMSGERROR                 (SANE_Byte) (0x43)
#define ASCQMSGERROR                (SANE_Byte) (0x00)
#define ASCCOMMCLREDANOTHINITIATOR  (SANE_Byte) (0x2F)
#define ASCQCOMMCLREDANOTHINITIATOR (SANE_Byte) (0x00)
#define ASCIOPROCTERMINATED         (SANE_Byte) (0x00)
#define ASCQIOPROCTERMINATED        (SANE_Byte) (0x06)
#define ASCINVBITIDMSG              (SANE_Byte) (0x3D)
#define ASCQINVBITIDMSG             (SANE_Byte) (0x00)
#define ASCINVMSGERROR              (SANE_Byte) (0x49)
#define ASCQINVMSGERROR             (SANE_Byte) (0x00)
#define ASCLAMPFAILURE              (SANE_Byte) (0x60)
#define ASCQLAMPFAILURE             (SANE_Byte) (0x00)
#define ASCMECHPOSERROR             (SANE_Byte) (0x15)
#define ASCQMECHPOSERROR            (SANE_Byte) (0x01)
#define ASCPARAMLISTLENERROR        (SANE_Byte) (0x1A)
#define ASCQPARAMLISTLENERROR       (SANE_Byte) (0x00)
#define ASCPARAMNOTSUPPORTED        (SANE_Byte) (0x26)
#define ASCQPARAMNOTSUPPORTED       (SANE_Byte) (0x01)
#define ASCPARAMVALINVALID          (SANE_Byte) (0x26)
#define ASCQPARAMVALINVALID         (SANE_Byte) (0x02)
#define ASCPOWERONRESET             (SANE_Byte) (0x26)
#define ASCQPOWERONRESET            (SANE_Byte) (0x03)
#define ASCSCANHEADPOSERROR         (SANE_Byte) (0x62)
#define ASCQSCANHEADPOSERROR        (SANE_Byte) (0x00)

/* Some Avision driver internal defines */
#define WINID 0

/* SCSI commands that the Avision scanners understand: */

#define AVISION_SCSI_TEST_UNIT_READY        0x00
#define AVISION_SCSI_INQUIRY		    0x12
#define AVISION_SCSI_MODE_SELECT	    0x15 
#define AVISION_SCSI_START_STOP		    0x1b
#define AVISION_SCSI_AREA_AND_WINDOWS	    0x24
#define AVISION_SCSI_READ_SCANNED_DATA	    0x28
#define AVISION_SCSI_GET_DATA_STATUS	    0x34 

/* The structures that you have to send to the avision to get it to
   do various stuff... */

struct win_desc_header {
    unsigned char pad0[6];
    unsigned char wpll[2];
};

struct win_desc_block {
     unsigned char winid;
     unsigned char pad0;
     unsigned char xres[2];
     unsigned char yres[2];
     unsigned char ulx[4];
     unsigned char uly[4];
     unsigned char width[4];  
     unsigned char length[4];
     unsigned char brightness;
     unsigned char thresh;
     unsigned char contrast;
     unsigned char image_comp;
     unsigned char bpp;
     unsigned char halftone[2];
     unsigned char pad_type;
     unsigned char bitordering[2];
     unsigned char compr_type;
     unsigned char compr_arg;
     unsigned char pad4[6];
     unsigned char vendor_specid;
     unsigned char paralen;
     unsigned char bitset1;
     unsigned char highlight;
     unsigned char shadow;
     unsigned char linewidth[2];
     unsigned char linecount[2];
     unsigned char bitset2;
     unsigned char pad5;
#if 1
     unsigned char r_exposure_time[2];
     unsigned char g_exposure_time[2];
     unsigned char b_exposure_time[2];
#endif
    
};

struct command_header {
  unsigned char opc;
  unsigned char pad0[3];  
  unsigned char len;
  unsigned char pad1;
};

struct command_header_10 {
    unsigned char opc;
    unsigned char pad0[5];  
    unsigned char len[3];
    unsigned char pad1;
};

struct command_read {
    unsigned char opc;
    unsigned char bitset1;
    unsigned char datatypecode;
    unsigned char calibchn;  
    unsigned char datatypequal[2];
    unsigned char transferlen[3];     
    unsigned char pad0;
};

struct command_scan {
    unsigned char opc;
    unsigned char pad0[3];  
    unsigned char transferlen;
    unsigned char bitset1;
};

struct def_win_par {
    struct command_header_10 dwph;
    struct win_desc_header wdh;
    struct win_desc_block wdb;
};

struct page_header{
    char pad0[4];
    char code;
    char length;
};

struct avision_page {
    char gamma;
    char thresh;
    char masks;
    char delay;
    char features;
    char pad0;
};


/* set SCSI highended variables. Declare them as an array of chars */
/* endianness-safe, int-size safe... */
#define set_double(var,val) var[0] = ((val) >> 8) & 0xff;  \
                            var[1] = ((val)     ) & 0xff;

#define set_triple(var,val) var[0] = ((val) >> 16) & 0xff; \
                            var[1] = ((val) >> 8 ) & 0xff; \
                            var[2] = ((val)      ) & 0xff;

#define set_quad(var,val)   var[0] = ((val) >> 24) & 0xff; \
                            var[1] = ((val) >> 16) & 0xff; \
                            var[2] = ((val) >> 8 ) & 0xff; \
                            var[3] = ((val)      ) & 0xff;

#endif /* avision_h */
