/*******************************************************************************
 * SANE - Scanner Access Now Easy.

   avision.h 

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

   This backend is based upon the Tamarack backend and adapted to the Avision
   scanners by René Rebe and Meino Cramer.
   
   Copyright 1999, 2000, 2001, 2002 by
                "René Rebe" <rene.rebe@gmx.net>
                "Meino Christian Cramer" <mccramer@s.netic.de>
                "Jose Paulo Moitinho de Almeida" <moitinho@civil.ist.utl.pt>
   
   Additional Contributers:
                "Gunter Wagner"
                  (some fixes and the transparency option)
                "Martin Jelínek" <mates@sirrah.troja.mff.cuni.cz>
                   nice attach debug output
                "Marcin Siennicki" <m.siennicki@cloos.pl>
                   found some typos and contributed fixes for the HP 7400
                "Frank Zago" <fzago@greshamstorage.com>
                   Mitsubishi IDs and report
   
   Very much thanks to:
                Oliver Neukum who sponsored a HP 5300 USB scanner !!! ;-)
                Avision INC for the documentation we got! ;-)
   
   Check the avision.c file for a ChangeLog ...
   
********************************************************************************/

#ifndef avision_h
#define avision_h

#include <sys/types.h>

typedef struct Avision_HWEntry {
  char* mfg;
  char* model;
  const char* real_mfg;
  const char* real_model;
  
  enum {AV_SCSI,
	AV_USB
  } connection_type;
  
  enum {AV_FLATBED,
	AV_FILM,
	AV_SHEETFEED
  } scanner_type;
  
  /* feature overwrites */
  enum {
    /* use single command calibraion send (i.e. all new scanners) */
    AV_CALIB2 = 1,
    /* use 512 bytes gamma table (i.e. Minolta film-scanner) */
    AV_GAMMA2 = 2,
    /* use 256 bytes gamma table (i.e. HP 5370C) */
    AV_GAMMA3 = 4  
    /* more to come ... */
  } feature_type;
  
} Avision_HWEntry;

enum Avision_Option
{
  OPT_NUM_OPTS = 0,      /* must come first */
  
  OPT_MODE_GROUP,
  OPT_MODE,
#define OPT_MODE_DEFAULT 3
  OPT_RESOLUTION,
#define OPT_RESOLUTION_DEFAULT 300
  OPT_SPEED,
  OPT_PREVIEW,
  
  OPT_GEOMETRY_GROUP,
  OPT_TL_X,	         /* top-left x */
  OPT_TL_Y,	         /* top-left y */
  OPT_BR_X,	         /* bottom-right x */
  OPT_BR_Y,		 /* bottom-right y */
  
  OPT_ENHANCEMENT_GROUP,
  OPT_BRIGHTNESS,
  OPT_CONTRAST,
  OPT_QSCAN,
  OPT_QCALIB,
  OPT_TRANS,             /* Transparency Mode */
  
  OPT_GAMMA_VECTOR,      /* first must be grey */
  OPT_GAMMA_VECTOR_R,    /* then r g b vector */
  OPT_GAMMA_VECTOR_G,
  OPT_GAMMA_VECTOR_B,
  
  OPT_FRAME,             /* Film holder control */
  
  NUM_OPTIONS            /* must come last */
};

typedef union Option_Value
{
  SANE_Word w;
  SANE_Word *wa;	 /* word array */
  SANE_String s;
} Option_Value;

typedef struct Avision_Dimensions
{
  /* in dpi */
  int res;
  int resx;
  int rexy;
  
  /* in 1200/dpi */
  long tlx;
  long tly;
  long brx;
  long bry;
  
  long width;
  long length;
  
  /* in pixels */
  int line_difference;
  
} Avision_Dimensions;

/* this contains our low-level info - not relevant for the SANE interface  */
typedef struct Avision_Device
{
  struct Avision_Device* next;
  SANE_Device sane;
  
  /* structs used to store config options */
  SANE_Range dpi_range;
  SANE_Range x_range;
  SANE_Range y_range;
  SANE_Range speed_range;

  SANE_Bool inquiry_new_protocol;
  SANE_Bool inquiry_detect_accessories;
  SANE_Bool inquiry_needs_calibration;
  SANE_Bool inquiry_needs_gamma;
  SANE_Bool inquiry_needs_software_colorpack;
  
  int inquiry_optical_res;     /* in dpi */
  int inquiry_max_res;         /* in dpi */
  
  double inquiry_x_range;         /* in mm */
  double inquiry_y_range;         /* in mm */
  
  double inquiry_adf_x_range;     /* in mm */
  double inquiry_adf_y_range;     /* in mm */
  
  double inquiry_transp_x_range;  /* in mm */
  double inquiry_transp_y_range;  /* in mm */
  
  int inquiry_color_boundary;
  int inquiry_grey_boundary;
  int inquiry_dithered_boundary;
  int inquiry_thresholded_boundary;
  int inquiry_line_difference; /* software color pack */
  
  /* int inquiry_bits_per_channel; */
  
  /* accessories */
  SANE_Bool inquiry_adf;
  SANE_Bool inquiry_light_box;
  
  /* film scanner atributes - maybe these should be in the scanner struct? */
  SANE_Range frame_range;
  SANE_Word current_frame;
  SANE_Word holder_type;
  
  /* driver state */
  SANE_Bool is_calibrated;
  
  Avision_HWEntry* hw;
  
} Avision_Device;

/* all the state relevant for the SANE interface */
typedef struct Avision_Scanner
{
  struct Avision_Scanner* next;
  Avision_Device* hw;
  
  SANE_Option_Descriptor opt [NUM_OPTIONS];
  Option_Value val [NUM_OPTIONS];
  SANE_Int gamma_table [4][256];
  
  /* page (ADF) state */
  int page;
  
  /* Parsed option values and variables that are valid only during
     the actual scan: */
  
  SANE_Bool scanning;           /* scan in progress */
  SANE_Parameters params;       /* scan window */
  Avision_Dimensions avdimen;   /* scan window - detailed internals */
  int mode;
  
  int fd;			/* SCSI filedescriptor */
  pid_t reader_pid;		/* process id of reader */
  int pipe;			/* pipe to reader process */
  int line;			/* current line number during scan */
  
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
#define AVISION_SCSI_MEDIA_CHECK            0x08
#define AVISION_SCSI_INQUIRY                0x12
#define AVISION_SCSI_MODE_SELECT            0x15
#define AVISION_SCSI_SCAN                   0x1b
#define AVISION_SCSI_SET_WINDOW             0x24
#define AVISION_SCSI_READ                   0x28
#define AVISION_SCSI_SEND                   0x2a
#define AVISION_SCSI_OBJECT_POSITION        0x31
#define AVISION_SCSI_GET_DATA_STATUS        0x34

#define AVISION_SCSI_OP_REJECT_PAPER        0x00
#define AVISION_SCSI_OP_LOAD_PAPER          0x01
#define AVISION_SCSI_OP_GO_HOME             0x02
#define AVISION_SCSI_OP_TRANS_CALIB_GREY    0x04
#define AVISION_SCSI_OP_TRANS_CALIB_COLOR   0x05

/* The structures that you have to send to an avision to get it to
   do various stuff... */

struct command_header
{
  u_int8_t opc;
  u_int8_t pad0 [3];
  u_int8_t len;
  u_int8_t pad1;
};

struct command_set_window
{
  u_int8_t opc;
  u_int8_t reserved0 [5];
  u_int8_t transferlen [3];
  u_int8_t control;
};

struct command_read
{
  u_int8_t opc;
  u_int8_t bitset1;
  u_int8_t datatypecode;
  u_int8_t calibchn;
  u_int8_t datatypequal [2];
  u_int8_t transferlen [3];
  u_int8_t control;
};

struct command_scan
{
  u_int8_t opc;
  u_int8_t pad0 [3];
  u_int8_t transferlen;
  u_int8_t bitset1;
};

struct command_send
{
  u_int8_t opc;
  u_int8_t bitset1;
  u_int8_t datatypecode;
  u_int8_t reserved0;  
  u_int8_t datatypequal [2];
  u_int8_t transferlen [3];
  u_int8_t reserved1;
};

struct command_set_window_window_header
{
  u_int8_t reserved0 [6];
  u_int8_t desclen [2];
};

struct command_set_window_window_descriptor
{
  u_int8_t winid;
  u_int8_t pad0;
  u_int8_t xres [2];
  u_int8_t yres [2];
  u_int8_t ulx [4];
  u_int8_t uly [4];
  u_int8_t width [4];
  u_int8_t length [4];
  u_int8_t brightness;
  u_int8_t thresh;
  u_int8_t contrast;
  u_int8_t image_comp;
  u_int8_t bpc;
  u_int8_t halftone [2];
  u_int8_t pad_type;
  u_int8_t bitordering [2];
  u_int8_t compr_type;
  u_int8_t compr_arg;
  u_int8_t pad4 [6];
  u_int8_t vendor_specid;
  u_int8_t paralen;
  u_int8_t bitset1;
  u_int8_t highlight;
  u_int8_t shadow;
  u_int8_t linewidth [2];
  u_int8_t linecount [2];
  u_int8_t bitset2;
  u_int8_t pad5;
  
  u_int8_t r_exposure_time [2];
  u_int8_t g_exposure_time [2];
  u_int8_t b_exposure_time [2];
};

struct page_header
{
  u_int8_t pad0 [4];
  u_int8_t code;
  u_int8_t length;
};

struct avision_page
{
  u_int8_t gamma;
  u_int8_t thresh;
  u_int8_t masks;
  u_int8_t delay;
  u_int8_t features;
  u_int8_t pad0;
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

#define BIT(n, p) ((n & ( 1 << p))?1:0)

/* These should be in saneopts.h */
#define SANE_NAME_FRAME "frame"
#define SANE_TITLE_FRAME SANE_I18N("Number of the frame to scan")
#define SANE_DESC_FRAME  SANE_I18N("Selects the number of the frame to scan")

#endif /* avision_h */
