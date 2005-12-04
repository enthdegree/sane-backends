
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
   scanners by Renebe and Meino Cramer.
   
   Check the avision.c file for detailed copyright and change-log
   information.

********************************************************************************/

#ifndef avision_h
#define avision_h

#include <sys/types.h>

#ifndef PATH_MAX
# define PATH_MAX 1024
#endif

typedef enum Avision_ConnectionType {
  AV_SCSI,
  AV_USB
} Avision_ConnectionType;

/* information needed for device access */
typedef struct Avision_Connection {
  Avision_ConnectionType connection_type;
  int scsi_fd;			/* SCSI filedescriptor */
  SANE_Int usb_dn;		/* USB (libusb or scanner.c) device number */
  enum {
    AVISION_USB_UNTESTED_STATUS, /* status type untested */
    AVISION_USB_INT_STATUS,      /* interrupt endp. (USB 1.x device) status */
    AVISION_USB_BULK_STATUS      /* bulk endp. (USB 2.0 device) status */
  } usb_status;
  
} Avision_Connection;

typedef struct Avision_HWEntry {
  const char* scsi_mfg;
  const char* scsi_model;

  int   usb_vendor;
  int   usb_product;

  const char* real_mfg;
  const char* real_model;
  
  /* feature overwrites */
  enum {
    /* force no calibration */
    AV_NO_CALIB = (1),
    
    /* force all in one command calibration */
    AV_ONE_CALIB_CMD = (1<<1),
    
    /* no gamma table */
    AV_NO_GAMMA = (1<<2),
    
    /* light check is bogus */
    AV_LIGHT_CHECK_BOGUS = (1<<3),
    
    /* do not use line packing even if line_difference */
    AV_NO_LINE_DIFFERENCE = (1<<4),
    
    /* if the scan area needs to be forced to A3 */
    AV_FORCE_A3 = (1<<5),
    
    /* if the scan area and resolution needs to be forced for films */
    AV_FORCE_FILM = (1<<6),
    
    /* some (?) USB devices like firmware */
    AV_FIRMWARE = (1<<7),
    
    /* is film scanner - no detection yet */
    AV_FILMSCANNER = (1<<8),

    /* fujitsu adaption */
    AV_FUJITSU = (1<<9),

    /* gray calibration data has to be uploaded on the blue channel ... ? */
    AV_GRAY_CALIB_BLUE = (1<<10),
    
    /* Interrupt endpoint button readout (so far AV220) */
    AV_INT_BUTTON = (1<<11),

    /* send acceleration table ... */
    AV_ACCEL_TABLE = (1<<12),

    /* non-interlaced scanns up to 300 dpi (AV32xx / AV83xx) */
    AV_NON_INTERLACED_DUPLEX_300 = (1<<13),

    /* do not send 3x3 matrix */
    AV_NO_MATRIX = (1<<14),

    /* force channel-by-channel calibration */
    AV_MULTI_CALIB_CMD = (1<<15)

    /* maybe more ...*/
  } feature_type;
    
} Avision_HWEntry;

typedef enum {
  AV_ASIC_Cx = 0,
  AV_ASIC_C1 = 1,
  AV_ASIC_W1 = 2,
  AV_ASIC_C2 = 3,
  AV_ASIC_C5 = 5,
  AV_ASIC_C6 = 6,
  AV_ASIC_OA980 = 128,
  AV_ASIC_OA982 = 129
} asic_type;

typedef enum {
  AV_THRESHOLDED,
  AV_DITHERED,
  AV_GRAYSCALE,       /* all gray needs to be before color for is_color() */
  AV_GRAYSCALE12,
  AV_GRAYSCALE16,
  AV_TRUECOLOR,
  AV_TRUECOLOR12,
  AV_TRUECOLOR16,
  AV_COLOR_MODE_LAST
} color_mode;

typedef enum {
  AV_NORMAL,
  AV_TRANSPARENT,
  AV_ADF,
  AV_ADF_REAR,
  AV_ADF_DUPLEX,
  AV_SOURCE_MODE_LAST
} source_mode;

typedef enum {
  AV_NORMAL_DIM,
  AV_TRANSPARENT_DIM,
  AV_ADF_DIM,
  AV_SOURCE_MODE_DIM_LAST
} source_mode_dim;

enum Avision_Option
{
  OPT_NUM_OPTS = 0,      /* must come first */
  
  OPT_MODE_GROUP,
  OPT_MODE,
  OPT_RESOLUTION,
#define OPT_RESOLUTION_DEFAULT 150
  OPT_SPEED,
  OPT_PREVIEW,
  
  OPT_SOURCE,            /* scan source normal, transparency, ADF */
  
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
  
  OPT_GAMMA_VECTOR,      /* first must be gray */
  OPT_GAMMA_VECTOR_R,    /* then r g b vector */
  OPT_GAMMA_VECTOR_G,
  OPT_GAMMA_VECTOR_B,
  
  /* too bad the SANE API does not allow bool vectors ... */
  OPT_BUTTON_0,           /* scanner button pressed */
  OPT_BUTTON_1,
  OPT_BUTTON_2,
  OPT_BUTTON_3,
  OPT_BUTTON_4,
  OPT_BUTTON_5,
  OPT_BUTTON_6,
  OPT_BUTTON_7,
  OPT_BUTTON_LAST = OPT_BUTTON_7,

  OPT_MESSAGE,
  
  OPT_FRAME,             /* Film holder control */
  
  NUM_OPTIONS            /* must come last */
};

typedef struct Avision_Dimensions
{
  /* in dpi */
  int xres;
  int yres;
  
  /* in pixels */
  long tlx;
  long tly;
  long brx;
  long bry;
  
  /* in pixels */
  int line_difference;

  /* interlaced duplex scan */
  SANE_Bool interlaced_duplex;
  
} Avision_Dimensions;

/* this contains our low-level info - not relevant for the SANE interface  */
typedef struct Avision_Device
{
  struct Avision_Device* next;
  SANE_Device sane;
  Avision_Connection connection;
  
  /* structs used to store config options */
  SANE_Range dpi_range;
  SANE_Range x_range;
  SANE_Range y_range;
  SANE_Range speed_range;

  asic_type inquiry_asic_type;
  SANE_Bool inquiry_new_protocol;
  SANE_Bool inquiry_light_box;
  SANE_Bool inquiry_adf;
  SANE_Bool inquiry_duplex;
  SANE_Bool inquiry_duplex_interlaced;
  SANE_Bool inquiry_paper_length;
  SANE_Bool inquiry_detect_accessories;
  SANE_Bool inquiry_needs_calibration;
  SANE_Bool inquiry_needs_gamma;
  SANE_Bool inquiry_keeps_gamma;
  SANE_Bool inquiry_keeps_window;
  SANE_Bool inquiry_calibration;
  SANE_Bool inquiry_3x3_matrix;
  SANE_Bool inquiry_needs_software_colorpack;
  SANE_Bool inquiry_needs_line_pack;
  SANE_Bool inquiry_adf_need_mirror;
  SANE_Bool inquiry_adf_bgr_order;
  SANE_Bool inquiry_light_detect;
  SANE_Bool inquiry_light_control;
  SANE_Bool inquiry_button_control;
  int       inquiry_max_shading_target;
  int       inquiry_buttons;
  
  enum {AV_FLATBED,
	AV_FILM,
	AV_SHEETFEED
  } scanner_type;
  
  /* the list of available color modes */
  SANE_String_Const color_list[AV_COLOR_MODE_LAST + 1];
  color_mode color_list_num[AV_COLOR_MODE_LAST];
  color_mode color_list_default;
  
  /* the list of available source modes */
  SANE_String_Const source_list[AV_SOURCE_MODE_LAST + 1];
  source_mode source_list_num[AV_SOURCE_MODE_LAST];
  
  int inquiry_optical_res;        /* in dpi */
  int inquiry_max_res;            /* in dpi */
  
  double inquiry_x_ranges  [AV_SOURCE_MODE_DIM_LAST]; /* in mm */
  double inquiry_y_ranges  [AV_SOURCE_MODE_DIM_LAST]; /* in mm */
  
  int inquiry_color_boundary;
  int inquiry_gray_boundary;
  int inquiry_dithered_boundary;
  int inquiry_thresholded_boundary;
  int inquiry_line_difference; /* software color pack */

  int inquiry_channels_per_pixel;
  int inquiry_bits_per_channel;
  
  int scsi_buffer_size; /* nice to have SCSI buffer size */

  /* additional information - read delayed until sane_open() */
  
  SANE_Bool additional_probe;
  
  /* film scanner atributes - maybe these should be in the scanner struct? */
  SANE_Range frame_range;
  SANE_Word current_frame;
  SANE_Word holder_type;
  
  /* some versin corrections */
  u_int16_t data_dq; /* was ox0A0D - but hangs some new scanners */
  
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
  
  /* we now save the calib data because we might need it for 16bit software
     calibration :-( */
  u_int8_t* dark_avg_data;
  u_int8_t* white_avg_data;
  
  /* Parsed option values and variables that are valid only during
     the actual scan: */
  
  SANE_Bool scanning;           /* scan in progress */
  SANE_Parameters params;       /* scan window */
  Avision_Dimensions avdimen;   /* scan window - detailed internals */
  
  u_int32_t param_cksum;        /* checksum of scan parameters */
  
  /* Internal data for duplex scans */
  char duplex_rear_fname [PATH_MAX];
  SANE_Bool duplex_rear_valid;
  
  color_mode c_mode;
  source_mode source_mode;
  source_mode_dim source_mode_dim;
  
  /* Avision HW Access Connection (SCSI/USB abstraction) */
  Avision_Connection av_con;

  pid_t reader_pid;		/* process id of reader */
  int reader_fds;		/* pipe write handler for reader */
  int pipe;			/* pipe to reader process */
  int line;			/* current line number during scan */
  
} Avision_Scanner;

/* Some Avision driver internal defines */
#define AV_WINID 0

/* Avision SCSI over USB error codes */
#define AVISION_USB_GOOD                    0x00
#define AVISION_USB_REQUEST_SENSE           0x02
#define AVISION_USB_BUSY                    0x08

/* SCSI commands that the Avision scanners understand: */

#define AVISION_SCSI_TEST_UNIT_READY        0x00
#define AVISION_SCSI_REQUEST_SENSE          0x03
#define AVISION_SCSI_MEDIA_CHECK            0x08
#define AVISION_SCSI_INQUIRY                0x12
#define AVISION_SCSI_MODE_SELECT            0x15
#define AVISION_SCSI_RESERVE_UNIT           0x16
#define AVISION_SCSI_RELEASE_UNIT           0x17
#define AVISION_SCSI_SCAN                   0x1b
#define AVISION_SCSI_SET_WINDOW             0x24
#define AVISION_SCSI_READ                   0x28
#define AVISION_SCSI_SEND                   0x2a
#define AVISION_SCSI_OBJECT_POSITION        0x31
#define AVISION_SCSI_GET_DATA_STATUS        0x34

#define AVISION_SCSI_OP_REJECT_PAPER        0x00
#define AVISION_SCSI_OP_LOAD_PAPER          0x01
#define AVISION_SCSI_OP_GO_HOME             0x02
#define AVISION_SCSI_OP_TRANS_CALIB_GRAY    0x04
#define AVISION_SCSI_OP_TRANS_CALIB_COLOR   0x05

/* The SCSI structures that we have to send to an avision to get it to
   do various stuff... */

typedef struct command_header
{
  u_int8_t opc;
  u_int8_t pad0 [3];
  u_int8_t len;
  u_int8_t pad1;
} command_header;

typedef struct command_set_window
{
  u_int8_t opc;
  u_int8_t reserved0 [5];
  u_int8_t transferlen [3];
  u_int8_t control;
} command_set_window;

typedef struct command_read
{
  u_int8_t opc;
  u_int8_t bitset1;
  u_int8_t datatypecode;
  u_int8_t readtype;
  u_int8_t datatypequal [2];
  u_int8_t transferlen [3];
  u_int8_t control;
} command_read;

typedef struct command_scan
{
  u_int8_t opc;
  u_int8_t bitset0;
  u_int8_t reserved0 [2];
  u_int8_t transferlen;
  u_int8_t bitset1;
} command_scan;

typedef struct command_send
{
  u_int8_t opc;
  u_int8_t bitset1;
  u_int8_t datatypecode;
  u_int8_t reserved0;  
  u_int8_t datatypequal [2];
  u_int8_t transferlen [3];
  u_int8_t reserved1;
} command_send;

typedef struct command_set_window_window
{
  struct {
    u_int8_t reserved0 [6];
    u_int8_t desclen [2];
  } header;
  
  struct {
    u_int8_t winid;
    u_int8_t reserved0;
    u_int8_t xres [2];
    u_int8_t yres [2];
    u_int8_t ulx [4];
    u_int8_t uly [4];
    u_int8_t width [4];
    u_int8_t length [4];
    u_int8_t brightness;
    u_int8_t threshold;
    u_int8_t contrast;
    u_int8_t image_comp;
    u_int8_t bpc;
    u_int8_t halftone [2];
    u_int8_t padding_and_bitset;
    u_int8_t bitordering [2];
    u_int8_t compr_type;
    u_int8_t compr_arg;
    u_int8_t paper_length[2];
    u_int8_t reserved1 [4];

    /* Avision specific parameters */
    u_int8_t vendor_specific;
    u_int8_t paralen; /* bytes following after this byte */
  } descriptor;
  
  struct {
    u_int8_t bitset1;
    u_int8_t highlight;
    u_int8_t shadow;
    u_int8_t line_width [2];
    u_int8_t line_count [2];
    
    /* the tail is quite version and model sepecific */
    union {
      struct {
	u_int8_t bitset2;
	u_int8_t reserved;
      } old;
      
      struct {
	u_int8_t bitset2;
	u_int8_t ir_exposure_time;
	
	/* optional */
	u_int8_t r_exposure_time [2];
	u_int8_t g_exposure_time [2];
	u_int8_t b_exposure_time [2];
	
	u_int8_t bitset3; /* reserved in the v2 */
	u_int8_t auto_focus;
	u_int8_t line_width_msb;
	u_int8_t line_count_msb;
	u_int8_t edge_threshold; /* background lines? */
      } normal;
      
      struct {
	u_int8_t reserved0 [4];
	u_int8_t paper_size;
	u_int8_t paperx [4];
	u_int8_t papery [4];
	u_int8_t reserved1 [2];
      } fujitsu;
    } type;
  } avision;
} command_set_window_window;

typedef struct page_header
{
  u_int8_t pad0 [4];
  u_int8_t code;
  u_int8_t length;
} page_header;

typedef struct avision_page
{
  u_int8_t gamma;
  u_int8_t thresh;
  u_int8_t masks;
  u_int8_t delay;
  u_int8_t features;
  u_int8_t pad0;
} avision_page;

typedef struct calibration_format
{
  u_int16_t pixel_per_line;
  u_int8_t bytes_per_channel;
  u_int8_t lines;
  u_int8_t flags;
  u_int8_t ability1;
  u_int8_t r_gain;
  u_int8_t g_gain;
  u_int8_t b_gain;
  u_int16_t r_shading_target;
  u_int16_t g_shading_target;
  u_int16_t b_shading_target;
  u_int16_t r_dark_shading_target;
  u_int16_t g_dark_shading_target;
  u_int16_t b_dark_shading_target;
  
  /* not returned but usefull in some places */
  u_int8_t channels;
} calibration_format;

typedef struct matrix_3x3
{
  u_int16_t v[9];
} matrix_3x3;

typedef struct acceleration_info 
{
  u_int16_t total_steps;
  u_int16_t stable_steps;
  u_int32_t table_units;
  u_int32_t base_units;
  u_int16_t start_speed;
  u_int16_t target_speed;
  u_int8_t ability;
  u_int8_t table_count;
  u_int8_t reserved[6];
} acceleration_info;

/* set/get SCSI highended (big-endian) variables. Declare them as an array
 * of chars endianness-safe, int-size safe ... */
#define set_double(var,val) var[0] = ((val) >> 8) & 0xff;  \
                            var[1] = ((val)     ) & 0xff

#define set_triple(var,val) var[0] = ((val) >> 16) & 0xff; \
                            var[1] = ((val) >> 8 ) & 0xff; \
                            var[2] = ((val)      ) & 0xff

#define set_quad(var,val)   var[0] = ((val) >> 24) & 0xff; \
                            var[1] = ((val) >> 16) & 0xff; \
                            var[2] = ((val) >> 8 ) & 0xff; \
                            var[3] = ((val)      ) & 0xff

#define get_double(var) ((*var << 8) + *(var + 1))

#define get_triple(var) ((*var << 16) + \
                         (*(var + 1) << 8) + *(var + 2))

#define get_quad(var)   ((*var << 24) + \
                         (*(var + 1) << 16) + \
                         (*(var + 2) << 8) + *(var + 3))

/* set/get Avision lowended (little-endian) shading data */
#define set_double_le(var,val) var[0] = ((val)     ) & 0xff;  \
                               var[1] = ((val) >> 8) & 0xff

#define get_double_le(var) ((*(var + 1) << 8) + *var)

#define BIT(n, p) ((n & (1 << p)) ? 1 : 0)

#define SET_BIT(n, p) (n |= (1 << p))
#define CLEAR_BIT(n, p) (n &= ~(1 << p))

/* These should be in saneopts.h */
#define SANE_NAME_FRAME "frame"
#define SANE_TITLE_FRAME SANE_I18N("Number of the frame to scan")
#define SANE_DESC_FRAME  SANE_I18N("Selects the number of the frame to scan")

#define SANE_NAME_DUPLEX "duplex"
#define SANE_TITLE_DUPLEX SANE_I18N("Duplex scan")
#define SANE_DESC_DUPLEX SANE_I18N("Duplex scan provide a scan of the front and back side of the document")

#ifdef AVISION_ENHANCED_SANE
#warning "Compiled Avision backend will violate the SANE standard"
/* Some Avision SANE extensions */
typedef enum
{
  SANE_STATUS_LAMP_WARMING = SANE_STATUS_ACCESS_DENIED + 1	/* lamp is warming up */
}
SANE_Avision_Status;

/* public API extension */

extern SANE_Status ENTRY(media_check) (SANE_Handle handle);

#endif

#endif /* avision_h */
