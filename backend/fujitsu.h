#ifndef FUJITSU_H
#define FUJITSU_H

/* 
 * Part of SANE - Scanner Access Now Easy.
 * Please see opening comment in fujitsu.c
 */

/* until SANE 1.1.0 is released, we wont have this frame type */
#define SANE_FRAME_JPEG 11

/* ------------------------------------------------------------------------- 
 * This option list has to contain all options for all scanners supported by
 * this driver. If a certain scanner cannot handle a certain option, there's
 * still the possibility to say so, later.
 */
enum fujitsu_Option
{
  OPT_NUM_OPTS = 0,

  OPT_MODE_GROUP,
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
  OPT_GAMMA,
  OPT_THRESHOLD,
  OPT_RIF,

  OPT_ADVANCED_GROUP,
  OPT_COMPRESS,
  OPT_COMPRESS_ARG,
  OPT_DF_DETECT,
  OPT_DF_DIFF,
  OPT_BG_COLOR,
  OPT_DROPOUT_COLOR,
  OPT_BUFF_MODE,
  OPT_PREPICK,
  OPT_OVERSCAN,
  OPT_SLEEP_TIME,
  OPT_DUPLEX_OFFSET,
  OPT_GREEN_OFFSET,
  OPT_BLUE_OFFSET,
  OPT_USE_SWAPFILE,

  OPT_SENSOR_GROUP,
  OPT_TOP,
  OPT_A3,
  OPT_B4,
  OPT_A4,
  OPT_B5,
  OPT_HOPPER,
  OPT_OMR,
  OPT_ADF_OPEN,
  OPT_SLEEP,
  OPT_SEND_SW,
  OPT_MANUAL_FEED,
  OPT_SCAN_SW,
  OPT_FUNCTION,
  OPT_INK_EMPTY,
  OPT_DOUBLE_FEED,
  OPT_ERROR_CODE,
  OPT_SKEW_ANGLE,
  OPT_INK_REMAIN,
  OPT_DUPLEX_SW,
  OPT_DENSITY_SW,

  /* must come last: */
  NUM_OPTIONS
};

struct fujitsu
{
  /* --------------------------------------------------------------------- */
  /* immutable values which are set during init of scanner.                */
  struct fujitsu *next;
  char *device_name;            /* The name of the scanner device for sane */

  /* --------------------------------------------------------------------- */
  /* immutable values which are set during reading of config file.         */
  int buffer_size;
  int connection;               /* hardware interface type */

  /* --------------------------------------------------------------------- */
  /* immutable values which are set during inquiry probing of the scanner. */
  /* members in order found in scsi data...                                */
  SANE_Device sane;

  char vendor_name[9];          /* raw data as returned by SCSI inquiry.   */
  char product_name[17];        /* raw data as returned by SCSI inquiry.   */
  char version_name[5];         /* raw data as returned by SCSI inquiry.   */

  int color_raster_offset;      /* offset between r and b scan line and    */
                                /* between b and g scan line (0 or 4)      */
  int has_bg_front;             /* background color can be changed for f/r */
  int has_bg_back;

  int duplex_raster_offset;     /* offset between front and rear page when */
                                /* when scanning 3091 style duplex         */

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

  int can_overflow;
  int can_monochrome;
  int can_halftone;
  int can_grayscale;
  int can_color_grayscale;

  /* --------------------------------------------------------------------- */
  /* immutable values which are set during vndr VPD probing of the scanner */
  /* members in order found in scsi data...                                */
  int has_operator_panel;
  int has_barcode;
  int has_imprinter;
  int has_duplex;
  int has_transparency;
  int has_flatbed;
  int has_adf;

  int adbits;
  int buffer_bytes;

  /*FIXME: do we need the std cmd list? */
  int has_cmd_msen;

  /*FIXME: there are more vendor cmds? */
  int has_cmd_subwindow;
  int has_cmd_endorser;
  int has_cmd_hw_status;
  int has_cmd_scanner_ctl;

  /*FIXME: do we need the vendor window param list? */

  int brightness_steps;
  int threshold_steps;
  int contrast_steps;

  int num_internal_gamma;
  int num_download_gamma;
  int num_internal_dither;
  int num_download_dither;

  int has_rif;
  int has_auto1;
  int has_auto2;
  int has_outline;
  int has_emphasis;
  int has_autosep;
  int has_mirroring;
  int has_white_level_follow;
  int has_subwindow;

  int has_comp_MH;
  int has_comp_MR;
  int has_comp_MMR;
  int has_comp_JBIG;
  int has_comp_JPG1;
  int has_comp_JPG2;
  int has_comp_JPG3;

  /*FIXME: endorser data? */

  /*FIXME: barcode data? */

  /* overscan size in pixels comes from scanner in basic res units */
  int os_x_basic;
  int os_y_basic;

  /* --------------------------------------------------------------------- */
  /* immutable values which are gathered by mode_sense command     */

  int has_MS_color;
  int has_MS_prepick;
  int has_MS_sleep;
  int has_MS_duplex;
  int has_MS_rand;
  int has_MS_bg;
  int has_MS_df;
  int has_MS_dropout; /* dropout color specified in mode select data */
  int has_MS_buff;
  int has_MS_auto;
  int has_MS_lamp;
  int has_MS_jobsep;

  /* --------------------------------------------------------------------- */
  /* immutable values which are hard coded because they are not in vpd     */
  /* this section replaces all the old 'switch (s->model)' code            */

  /* the scan size in 1/1200th inches, NOT basic_units or sane units */
  int max_x;
  int max_y;
  int min_x;
  int min_y;

  int has_back;       /* not all duplex scanners can do adf back side only */
  int color_interlace;  /* different models interlace colors differently   */
  int duplex_interlace; /* different models interlace sides differently    */
  int even_scan_line; /* need even number of bytes in a scanline (fi-5900) */
  int window_vid;    /* some models want different vendor ID in set window */
  int ghs_in_rs;
  int window_gamma;

  int has_SW_dropout; /* dropout color specified in set window data */

  int reverse_by_mode[6]; /* mode specific */

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
  SANE_Range gamma_range;
  SANE_Range threshold_range;

  /*ipc group*/

  /*advanced group*/
  SANE_String_Const compress_list[3];
  SANE_Range compress_arg_range;
  SANE_String_Const df_detect_list[6];
  SANE_String_Const df_diff_list[5];
  SANE_String_Const bg_color_list[4];
  SANE_String_Const do_color_list[5];
  SANE_String_Const lamp_color_list[5];
  SANE_String_Const buff_mode_list[4];
  SANE_String_Const prepick_list[4];
  SANE_String_Const overscan_list[4];
  SANE_Range sleep_time_range;
  SANE_Range duplex_offset_range;
  SANE_Range green_offset_range;
  SANE_Range blue_offset_range;

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
  int gamma;
  int threshold;
  int rif;

  /*advanced group*/
  int compress;
  int compress_arg;
  int df_detect;
  int df_diff;
  int bg_color;
  int dropout_color;
  int buff_mode;
  int prepick;
  int overscan;
  int lamp_color;
  int sleep_time;
  int duplex_offset;
  int green_offset;
  int blue_offset;
  int use_temp_file;

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
  int img_count; /* how many 'sides' delivered */

  /* total to read/write */
  int bytes_tot[2];

  /* how far we have read */
  int bytes_rx[2];
  int lines_rx[2]; /*only used by 3091*/

  /* how far we have written */
  int bytes_tx[2];

  unsigned char * buffers[2];
  int fds[2];

  /* --------------------------------------------------------------------- */
  /* values used by the compression functions, esp. jpeg with duplex       */
  int jpeg_stage;
  int jpeg_ff_offset;
  int jpeg_front_rst;
  int jpeg_back_rst;
  int jpeg_x_bit;

  /* --------------------------------------------------------------------- */
  /* values which used by the command and data sending functions (scsi/usb)*/
  int fd;                      /* The scanner device file descriptor.     */
  size_t rs_info;

  /* --------------------------------------------------------------------- */
  /* values which are used by the get hardware status command              */
  time_t last_ghs;

  int hw_top;
  int hw_A3;
  int hw_B4;
  int hw_A4;
  int hw_B5;

  int hw_hopper;
  int hw_omr;
  int hw_adf_open;

  int hw_sleep;
  int hw_send_sw;
  int hw_manual_feed;
  int hw_scan_sw;

  int hw_function;

  int hw_ink_empty;
  int hw_double_feed;

  int hw_error_code;
  int hw_skew_angle;
  int hw_ink_remain;

  int hw_duplex_sw;
  int hw_density_sw;
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
#define COMP_JPEG WD_cmp_JPG1

#define JPEG_STAGE_HEAD 0
#define JPEG_STAGE_SOF 1
#define JPEG_STAGE_SOS 2
#define JPEG_STAGE_FRONT 3
#define JPEG_STAGE_BACK 4
#define JPEG_STAGE_EOI 5

/* these are same as scsi data to make code easier */
#define MODE_LINEART WD_comp_LA
#define MODE_HALFTONE WD_comp_HT
#define MODE_GRAYSCALE WD_comp_GS
#define MODE_COLOR_LINEART WD_comp_CL
#define MODE_COLOR_HALFTONE WD_comp_CH
#define MODE_COLOR WD_comp_CG

/* these are same as dropout scsi data to make code easier */
#define COLOR_DEFAULT 0
#define COLOR_GREEN 8
#define COLOR_RED 9
#define COLOR_BLUE 11

#define COLOR_WHITE 1
#define COLOR_BLACK 2

#define COLOR_INTERLACE_NONE 0
#define COLOR_INTERLACE_3091 1
#define COLOR_INTERLACE_BGR 2
#define COLOR_INTERLACE_RRGGBB 3

#define DUPLEX_INTERLACE_ALT 0 
#define DUPLEX_INTERLACE_NONE 1 
#define DUPLEX_INTERLACE_3091 2 

#define DF_DEFAULT 0
#define DF_NONE 1
#define DF_THICKNESS 2
#define DF_LENGTH 3
#define DF_BOTH 4

/* ------------------------------------------------------------------------- */

#define MM_PER_INCH    25.4
#define MM_PER_UNIT_UNFIX SANE_UNFIX(SANE_FIX(MM_PER_INCH / 1200.0))
#define MM_PER_UNIT_FIX SANE_FIX(SANE_UNFIX(SANE_FIX(MM_PER_INCH / 1200.0)))

#define SCANNER_UNIT_TO_FIXED_MM(number) SANE_FIX((number) * MM_PER_UNIT_UNFIX)
#define FIXED_MM_TO_SCANNER_UNIT(number) SANE_UNFIX(number) / MM_PER_UNIT_UNFIX

#define FUJITSU_CONFIG_FILE "fujitsu.conf"

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

static SANE_Status find_scanners (void);

static SANE_Status attach_one_scsi (const char *name);
static SANE_Status attach_one_usb (const char *name);
static SANE_Status attach_one (const char *devicename, int connType);

static SANE_Status connect_fd (struct fujitsu *s);
static SANE_Status disconnect_fd (struct fujitsu *s);

static SANE_Status sense_handler (int scsi_fd, u_char * result, void *arg);

static SANE_Status init_inquire (struct fujitsu *s);
static SANE_Status init_vpd (struct fujitsu *s);
static SANE_Status init_ms (struct fujitsu *s);
static SANE_Status init_model (struct fujitsu *s);
static SANE_Status init_user (struct fujitsu *s);
static SANE_Status init_options (struct fujitsu *scanner);

static SANE_Status
do_cmd(struct fujitsu *s, int runRS, int shortTime,
 unsigned char * cmdBuff, size_t cmdLen,
 unsigned char * outBuff, size_t outLen,
 unsigned char * inBuff, size_t * inLen
);

static SANE_Status
do_scsi_cmd(struct fujitsu *s, int runRS, int shortTime,
 unsigned char * cmdBuff, size_t cmdLen,
 unsigned char * outBuff, size_t outLen,
 unsigned char * inBuff, size_t * inLen
);

static SANE_Status
do_usb_cmd(struct fujitsu *s, int runRS, int shortTime,
 unsigned char * cmdBuff, size_t cmdLen,
 unsigned char * outBuff, size_t outLen,
 unsigned char * inBuff, size_t * inLen
);

static int wait_scanner (struct fujitsu *s);

static int object_position (struct fujitsu *s, int i_load);

static SANE_Status do_cancel (struct fujitsu *scanner);

static SANE_Status scanner_control (struct fujitsu *s, int function);

static SANE_Status mode_select_df(struct fujitsu *s);

static SANE_Status mode_select_dropout(struct fujitsu *s);

static SANE_Status mode_select_bg(struct fujitsu *s);

static SANE_Status mode_select_buff (struct fujitsu *s);

static SANE_Status mode_select_prepick (struct fujitsu *s);

static SANE_Status mode_select_overscan (struct fujitsu *s);

static SANE_Status set_sleep_mode(struct fujitsu *s);

int get_current_side (struct fujitsu *s);
int get_page_width (struct fujitsu *s);
int get_page_height (struct fujitsu *s);

static SANE_Status send_lut (struct fujitsu *s);
static int set_window (struct fujitsu *s);

static SANE_Status get_pixelsize (struct fujitsu *s, int*, int*, int*, int*);

static int start_scan (struct fujitsu *s);

static SANE_Status read_from_JPEGduplex(struct fujitsu *s);
static SANE_Status read_from_3091duplex(struct fujitsu *s);
static SANE_Status read_from_scanner(struct fujitsu *s, int side);

static SANE_Status copy_3091(struct fujitsu *s, unsigned char * buf, int len, int side);
static SANE_Status copy_buffer(struct fujitsu *s, unsigned char * buf, int len, int side);

static SANE_Status read_from_buffer(struct fujitsu *s, SANE_Byte * buf, SANE_Int max_len, SANE_Int * len, int side);

static SANE_Status setup_buffers (struct fujitsu *s);

static SANE_Status get_hardware_status (struct fujitsu *s);

static void hexdump (int level, char *comment, unsigned char *p, int l);

static size_t maxStringSize (const SANE_String_Const strings[]);

#endif /* FUJITSU_H */
