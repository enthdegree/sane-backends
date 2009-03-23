#ifndef CANON_DR_H
#define CANON_DR_H

/* 
 * Part of SANE - Scanner Access Now Easy.
 * Please see opening comment in canon_dr.c
 */

/* ------------------------------------------------------------------------- 
 * This option list has to contain all options for all scanners supported by
 * this driver. If a certain scanner cannot handle a certain option, there's
 * still the possibility to say so, later.
 */
enum scanner_Option
{
  OPT_NUM_OPTS = 0,

  OPT_STANDARD_GROUP,
  OPT_SOURCE, /*fb/adf/front/back/duplex*/
  OPT_MODE,   /*mono/gray/color*/
  OPT_X_RES,  /*a range or a list*/
  OPT_Y_RES,  /*a range or a list*/

  OPT_GEOMETRY_GROUP,
  OPT_TL_X,
  OPT_TL_Y,
  OPT_BR_X,
  OPT_BR_Y,
  OPT_PAGE_WIDTH,
  OPT_PAGE_HEIGHT,

  OPT_ENHANCEMENT_GROUP,
  OPT_BRIGHTNESS,
  OPT_CONTRAST,
  OPT_THRESHOLD,
  OPT_RIF,

  OPT_ADVANCED_GROUP,
  OPT_COMPRESS,
  OPT_COMPRESS_ARG,
  OPT_DF_THICKNESS,
  OPT_DF_LENGTH,
  OPT_ROLLERDESKEW,
  OPT_STAPLEDETECT,
  OPT_DROPOUT_COLOR_F,
  OPT_DROPOUT_COLOR_B,
  OPT_BUFFERMODE,

  /*sensor group*/
  OPT_SENSOR_GROUP,
  OPT_START,
  OPT_STOP,
  OPT_NEWFILE,
  OPT_COUNTONLY,
  OPT_BYPASSMODE,
  OPT_COUNTER,

  /* must come last: */
  NUM_OPTIONS
};

struct scanner
{
  /* --------------------------------------------------------------------- */
  /* immutable values which are set during init of scanner.                */
  struct scanner *next;
  char device_name[1024];             /* The name of the device from sanei */
  int missing; 				/* used to mark unplugged scanners */

  /* --------------------------------------------------------------------- */
  /* immutable values which are set during reading of config file.         */
  int buffer_size;
  int connection;               /* hardware interface type */

  /* --------------------------------------------------------------------- */
  /* immutable values which are set during inquiry probing of the scanner. */
  /* members in order found in scsi data...                                */
  char vendor_name[9];          /* raw data as returned by SCSI inquiry.   */
  char model_name[17];          /* raw data as returned by SCSI inquiry.   */
  char version_name[5];         /* raw data as returned by SCSI inquiry.   */

  /* --------------------------------------------------------------------- */
  /* immutable values which are set during std VPD probing of the scanner. */
  /* members in order found in scsi data...                                */
  int basic_x_res;
  int basic_y_res;
  int step_x_res;
  int step_y_res;
  int max_x_res;
  int max_y_res;
  int min_x_res;
  int min_y_res;

  int std_res_200;
  int std_res_180;
  int std_res_160;
  int std_res_150;
  int std_res_120;
  int std_res_100;
  int std_res_75;
  int std_res_60;
  int std_res_1200;
  int std_res_800;
  int std_res_600;
  int std_res_480;
  int std_res_400;
  int std_res_320;
  int std_res_300;
  int std_res_240;

  /* max scan size in pixels comes from scanner in basic res units */
  int max_x_basic;
  int max_y_basic;

  /*FIXME: 4 more unknown values here*/
  int can_grayscale;
  int can_halftone;
  int can_monochrome;
  int can_overflow;

  /* --------------------------------------------------------------------- */
  /* immutable values which are hard coded because they are not in vpd     */

  int brightness_steps;
  int threshold_steps;
  int contrast_steps;

  /* the scan size in 1/1200th inches, NOT basic_units or sane units */
  int max_x;
  int max_y;
  int min_x;
  int min_y;
  int max_x_fb;
  int max_y_fb;

  int can_color; /* actually might be in vpd, but which bit? */

  int has_counter;
  int has_rif;
  int has_adf;
  int has_flatbed;
  int has_duplex;
  int has_back;         /* not all duplex scanners can do adf back side only */
  int has_comp_JPEG;
  int has_buffer;
  int rgb_format;       /* meaning unknown */
  int padding;          /* meaning unknown */

  int invert_tly;       /* weird bug in some smaller scanners */
  int unknown_byte2;    /* weird byte, required, meaning unknown */
  int padded_read;      /* some machines need extra 12 bytes on reads */
  int fixed_width;      /* some machines always scan full width */

  int color_interlace;  /* different models interlace colors differently     */
  int duplex_interlace; /* different models interlace sides differently      */
  int head_interlace;   /* different models interlace heads differently      */
  int jpeg_interlace;   /* different models interlace jpeg sides differently */

  int reverse_by_mode[6]; /* mode specific */

  /* --------------------------------------------------------------------- */
  /* immutable values which are set during serial number probing scanner   */
  char serial_name[28];        /* 16 char model, ':', 10 byte serial, null */

  /* --------------------------------------------------------------------- */
  /* struct with pointers to device/vendor/model names, and a type value */
  /* used to inform sane frontend about the device */
  SANE_Device sane;

  /* --------------------------------------------------------------------- */
  /* changeable SANE_Option structs provide our interface to frontend.     */
  /* some options require lists of strings or numbers, we keep them here   */
  /* instead of in global vars so that they can differ for each scanner    */

  /* long array of option structs */
  SANE_Option_Descriptor opt[NUM_OPTIONS];

  /*mode group*/
  SANE_String_Const mode_list[7];
  SANE_String_Const source_list[5];

  SANE_Int x_res_list[17];
  SANE_Int y_res_list[17];
  SANE_Range x_res_range;
  SANE_Range y_res_range;

  /*geometry group*/
  SANE_Range tl_x_range;
  SANE_Range tl_y_range;
  SANE_Range br_x_range;
  SANE_Range br_y_range;
  SANE_Range paper_x_range;
  SANE_Range paper_y_range;

  /*enhancement group*/
  SANE_Range brightness_range;
  SANE_Range contrast_range;
  SANE_Range threshold_range;

  /*advanced group*/
  SANE_String_Const compress_list[3];
  SANE_Range compress_arg_range;
  SANE_String_Const do_color_list[8];

  /*sensor group*/
  SANE_Range counter_range;

  /* --------------------------------------------------------------------- */
  /* changeable vars to hold user input. modified by SANE_Options above    */

  /*mode group*/
  int mode;           /*color,lineart,etc*/
  int source;         /*fb,adf front,adf duplex,etc*/
  int resolution_x;   /* X resolution in dpi                       */
  int resolution_y;   /* Y resolution in dpi                       */

  /*geometry group*/
  /* The desired size of the scan, all in 1/1200 inch */
  int tl_x;
  int tl_y;
  int br_x;
  int br_y;
  int page_width;
  int page_height;

  /*enhancement group*/
  int brightness;
  int contrast;
  int threshold;
  int rif;

  /*advanced group*/
  int compress;
  int compress_arg;
  int df_length;
  int df_thickness;
  int dropout_color_f;
  int dropout_color_b;
  int buffermode;
  int rollerdeskew;
  int stapledetect;

  /* --------------------------------------------------------------------- */
  /* values which are derived from setting the options above */
  /* the user never directly modifies these */

  /* this is defined in sane spec as a struct containing:
	SANE_Frame format;
	SANE_Bool last_frame;
	SANE_Int lines;
	SANE_Int depth; ( binary=1, gray=8, color=8 (!24) )
	SANE_Int pixels_per_line;
	SANE_Int bytes_per_line;
  */
  SANE_Parameters params;

  /* --------------------------------------------------------------------- */
  /* values which are set by scanning functions to keep track of pages, etc */
  int started;
  int reading;
  int cancelled;
  int side;
  int jpeg_stage;
  int jpeg_ff_offset;

  /* total to read/write */
  int bytes_tot[2];

  /* how far we have read */
  int bytes_rx[2];
  int lines_rx[2]; /*only used by 3091*/

  /* how far we have written */
  int bytes_tx[2];

  unsigned char * buffers[2];

  /* --------------------------------------------------------------------- */
  /* values used by the command and data sending functions (scsi/usb)      */
  int fd;                      /* The scanner device file descriptor.      */
  size_t rs_info;

  /* --------------------------------------------------------------------- */
  /* values used to hold hardware or control panel status                  */

  time_t last_panel;
  int panel_start;
  int panel_stop;
  int panel_new_file;
  int panel_count_only;
  int panel_bypass_mode;
  int panel_enable_led;
  int panel_counter;
};

#define CONNECTION_SCSI   0 /* SCSI interface */
#define CONNECTION_USB    1 /* USB interface */

#define SIDE_FRONT 0
#define SIDE_BACK 1

#define SOURCE_FLATBED 0
#define SOURCE_ADF_FRONT 1
#define SOURCE_ADF_BACK 2
#define SOURCE_ADF_DUPLEX 3

#define COMP_NONE WD_cmp_NONE
#define COMP_JPEG WD_cmp_JPEG

#define JPEG_STAGE_NONE 0
#define JPEG_STAGE_SOF 1

/* these are same as scsi data to make code easier */
#define MODE_LINEART WD_comp_LA
#define MODE_HALFTONE WD_comp_HT
#define MODE_GRAYSCALE WD_comp_GS
#define MODE_COLOR WD_comp_CG

enum {
 COLOR_NONE = 0,
 COLOR_RED,
 COLOR_GREEN,
 COLOR_BLUE,
 COLOR_EN_RED,
 COLOR_EN_GREEN,
 COLOR_EN_BLUE
};

/* these are same as scsi data to make code easier */
#define COLOR_WHITE 1
#define COLOR_BLACK 2

#define HEAD_INTERLACE_NONE 0
#define HEAD_INTERLACE_2510 1

#define COLOR_INTERLACE_RGB 0
#define COLOR_INTERLACE_BGR 1
#define COLOR_INTERLACE_RRGGBB 2

#define DUPLEX_INTERLACE_NONE 0
#define DUPLEX_INTERLACE_ALT 1
#define DUPLEX_INTERLACE_BYTE 2

#define JPEG_INTERLACE_ALT 0 
#define JPEG_INTERLACE_NONE 1 

#define CROP_RELATIVE 0
#define CROP_ABSOLUTE 1

/* ------------------------------------------------------------------------- */

#define MM_PER_INCH    25.4
#define MM_PER_UNIT_UNFIX SANE_UNFIX(SANE_FIX(MM_PER_INCH / 1200.0))
#define MM_PER_UNIT_FIX SANE_FIX(SANE_UNFIX(SANE_FIX(MM_PER_INCH / 1200.0)))

#define SCANNER_UNIT_TO_FIXED_MM(number) SANE_FIX((number) * MM_PER_UNIT_UNFIX)
#define FIXED_MM_TO_SCANNER_UNIT(number) SANE_UNFIX(number) / MM_PER_UNIT_UNFIX

#define CANON_DR_CONFIG_FILE "canon_dr.conf"

#ifndef PATH_MAX
#  define PATH_MAX 1024
#endif

#ifndef PATH_SEP
#ifdef HAVE_OS2_H
#  define PATH_SEP       '\\'
#else
#  define PATH_SEP       '/'
#endif
#endif

/* ------------------------------------------------------------------------- */

SANE_Status sane_init (SANE_Int * version_code, SANE_Auth_Callback authorize);

SANE_Status sane_get_devices (const SANE_Device *** device_list,
                              SANE_Bool local_only);

SANE_Status sane_open (SANE_String_Const name, SANE_Handle * handle);

SANE_Status sane_set_io_mode (SANE_Handle h, SANE_Bool non_blocking);

SANE_Status sane_get_select_fd (SANE_Handle h, SANE_Int * fdp);

const SANE_Option_Descriptor * sane_get_option_descriptor (SANE_Handle handle,
                                                          SANE_Int option);

SANE_Status sane_control_option (SANE_Handle handle, SANE_Int option,
                                 SANE_Action action, void *val,
                                 SANE_Int * info);

SANE_Status sane_start (SANE_Handle handle);

SANE_Status sane_get_parameters (SANE_Handle handle,
                                 SANE_Parameters * params);

SANE_Status sane_read (SANE_Handle handle, SANE_Byte * buf, SANE_Int max_len,
                       SANE_Int * len);

void sane_cancel (SANE_Handle h);

void sane_close (SANE_Handle h);

void sane_exit (void);

/* ------------------------------------------------------------------------- */

static SANE_Status attach_one_scsi (const char *name);
static SANE_Status attach_one_usb (const char *name);
static SANE_Status attach_one (const char *devicename, int connType);

static SANE_Status connect_fd (struct scanner *s);
static SANE_Status disconnect_fd (struct scanner *s);

static SANE_Status sense_handler (int scsi_fd, u_char * result, void *arg);

static SANE_Status init_inquire (struct scanner *s);
static SANE_Status init_vpd (struct scanner *s);
static SANE_Status init_model (struct scanner *s);
static SANE_Status init_panel (struct scanner *s);
static SANE_Status init_user (struct scanner *s);
static SANE_Status init_options (struct scanner *s);

static SANE_Status
do_cmd(struct scanner *s, int runRS, int shortTime,
 unsigned char * cmdBuff, size_t cmdLen,
 unsigned char * outBuff, size_t outLen,
 unsigned char * inBuff, size_t * inLen
);

static SANE_Status
do_scsi_cmd(struct scanner *s, int runRS, int shortTime,
 unsigned char * cmdBuff, size_t cmdLen,
 unsigned char * outBuff, size_t outLen,
 unsigned char * inBuff, size_t * inLen
);

static SANE_Status
do_usb_cmd(struct scanner *s, int runRS, int shortTime,
 unsigned char * cmdBuff, size_t cmdLen,
 unsigned char * outBuff, size_t outLen,
 unsigned char * inBuff, size_t * inLen
);

static SANE_Status do_usb_clear(struct scanner *s, int runRS);

static SANE_Status wait_scanner (struct scanner *s);

static SANE_Status object_position (struct scanner *s, int i_load);

static SANE_Status ssm_buffer (struct scanner *s);
static SANE_Status ssm_do (struct scanner *s);
static SANE_Status ssm_df (struct scanner *s);

static int get_page_width (struct scanner *s);
static int get_page_height (struct scanner *s);

static SANE_Status set_window (struct scanner *s);

static SANE_Status read_panel(struct scanner *s);
static SANE_Status send_panel(struct scanner *s);

static SANE_Status start_scan (struct scanner *s);

static SANE_Status cancel(struct scanner *s);

static SANE_Status read_from_scanner(struct scanner *s, int side);
static SANE_Status read_from_scanner_duplex(struct scanner *s);

static SANE_Status copy_buffer(struct scanner *s, unsigned char * buf, int len, int side);
static SANE_Status copy_buffer_2510(struct scanner *s, unsigned char * buf, int len, int side);

static SANE_Status read_from_buffer(struct scanner *s, SANE_Byte * buf, SANE_Int max_len, SANE_Int * len, int side);

static SANE_Status setup_buffers (struct scanner *s);

static void hexdump (int level, char *comment, unsigned char *p, int l);
static void default_globals (void);

static size_t maxStringSize (const SANE_String_Const strings[]);

#endif /* CANON_DR_H */
