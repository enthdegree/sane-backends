#ifndef FUJITSU_H
#define FUJITSU_H

/* 
 * Part of SANE - Scanner Access Now Easy.
 * 
 * Please see opening comment in fujitsu.c
 */

/* ------------------------------------------------------------------------- */

static int num_devices;
static struct fujitsu *first_dev;

typedef union
  {
    SANE_Word w;
    SANE_String s;
  }
Option_Value;

/* ------------------------------------------------------------------------- 
 * This option list has to contain all options for all scanners supported by
 * this driver. If a certain scanner cannot handle a certain option, there's
 * still the possibility to say so, later.
 */
enum fujitsu_Option
{
  OPT_NUM_OPTS = 0,

  OPT_MODE_GROUP,
  OPT_SOURCE,
  OPT_MODE,
  OPT_DUPLEX,
  OPT_X_RES,
  OPT_Y_RES,

  OPT_GEOMETRY_GROUP,
  OPT_TL_X,			/* in mm/2^16 */
  OPT_TL_Y,			/* in mm/2^16 */
  OPT_BR_X,			/* in mm/2^16 */
  OPT_BR_Y,			/* in mm/2^16 */
  OPT_PAGE_WIDTH,
  OPT_PAGE_HEIGHT,


  OPT_ENHANCEMENT_GROUP,
  OPT_AVERAGING,
  OPT_BRIGHTNESS,
  OPT_THRESHOLD,
  OPT_CONTRAST,

  OPT_RIF,
  OPT_COMPRESSION,

  OPT_DTC_SELECTION,
  OPT_GAMMA,
  OPT_OUTLINE_EXTRACTION,
  OPT_EMPHASIS,
  OPT_AUTOSEP,
  OPT_MIRROR_IMAGE,
  OPT_VARIANCE_RATE,
  OPT_THRESHOLD_CURVE,
  OPT_GRADATION,
  OPT_SMOOTHING_MODE,
  OPT_FILTERING,
  OPT_BACKGROUND,
  OPT_NOISE_REMOVAL,
  OPT_MATRIX2X2,
  OPT_MATRIX3X3,
  OPT_MATRIX4X4,
  OPT_MATRIX5X5,
  OPT_WHITE_LEVEL_FOLLOW,

  OPT_PAPER_SIZE,
  OPT_PAPER_WIDTH,
  OPT_PAPER_HEIGHT,
  OPT_PAPER_ORIENTATION,

  OPT_DROPOUT_COLOR,
  OPT_START_BUTTON,

  OPT_TUNING_GROUP,
  OPT_LAMP_COLOR,
  OPT_BLUE_OFFSET,
  OPT_GREEN_OFFSET,
  OPT_USE_SWAPFILE,

  OPT_IMPRINTER_GROUP,
  OPT_IMPRINTER,
  OPT_IMPRINTER_DIR,
  OPT_IMPRINTER_YOFFSET,
  OPT_IMPRINTER_STRING,
  OPT_IMPRINTER_CTR_INIT,
  OPT_IMPRINTER_CTR_STEP,
  OPT_IMPRINTER_CTR_DIR,

  OPT_SLEEP_MODE,

  /* must come last: */
  NUM_OPTIONS
};


typedef enum {				/* hardware connection to the scanner */
        SANE_FUJITSU_NODEV,		/* default, no HW specified yet */
	SANE_FUJITSU_SCSI,		/* SCSI interface */
	SANE_FUJITSU_PIO,		/* parallel interface */
	SANE_FUJITSU_USB		/* USB interface */
} Fujitsu_Connection_Type;

struct fujitsu
{
  struct fujitsu *next;

  SANE_Option_Descriptor opt[NUM_OPTIONS];
  Option_Value val[NUM_OPTIONS];
  SANE_Device sane;


  /* Immutable values which are set during initial probing of the scanner. */
  /* --------------------------------------------------------------------- */

  char vendorName[9];		/* raw data as returned by SCSI inquiry.     */
  char productName[17];		/* raw data as returned by SCSI inquiry.     */
  char versionName[5];		/* raw data as returned by SCSI inquiry.     */

  int model;			/* The scanner model.                        */

  char *devicename;		/* The name of the scanner device.           */

  Fujitsu_Connection_Type connection;	/* hardware interface type */

  int sfd;			/* The scanner device file descriptor.       */

  int color_raster_offset;	/* offset between r and b scan line and    */
  /* between b and g scan line (4 for all      */
  /* known models)                             */
  int duplex_raster_offset;	/* offset between front and rear page when   */
  /* when scanning duplex                   */

  int can_read_alternate;	/* duplex transfer mode front/back/front/back... */
  int has_adf;		        /* true if an ADF has been detected.       */
  int has_fb;
  int duplex_present;		/* true if a duplex unit has been detected. */
  int ipc_present;		/* true if ipc2/3 option detected         */
  int cmp_present;		/* true if cmp2 present                   */
  int has_hw_status;		/* true for M409X series                  */
  int has_outline;		/* ............                           */
  int has_emphasis;		/* ............                           */
  int has_autosep;		/* ............                           */
  int has_mirroring;		/* ............                           */
  int has_reverse;
  int has_white_level_follow;	/* ............                           */
  int has_subwindow;		/* ............                            */
  int has_dropout_color;	/*  */
  int has_imprinter;
  int has_threshold;
  int has_brightness;
  int has_contrast;

  SANE_Range adf_width_range;
  SANE_Range adf_height_range;
  SANE_Range x_range;
  SANE_Range y_range;

  SANE_Range x_res_range;
  SANE_Range y_res_range;
  SANE_Range x_grey_res_range;
  SANE_Range y_grey_res_range;

  SANE_String_Const vpd_mode_list[7];
  SANE_String_Const compression_mode_list[9];

  SANE_Int x_res_list[17];	/* range of x and y resolution */
  SANE_Int y_res_list[17];	/* if scanner has only fixed resolutions */
  SANE_Int x_res_list_grey[17];
  SANE_Int y_res_list_grey[17];

  SANE_Range threshold_range;
  SANE_Range brightness_range;
  SANE_Range contrast_range;

  /* User settable values (usually mirrored in SANE options)               */
  /* --------------------------------------------------------------------- */

  int duplex_mode;		/* Use DUPLEX_* constants below              */
  int resolution_x;		/* X resolution in dpi                       */
  int resolution_y;		/* Y resolution in dpi                       */
  int resolution_linked;	/* When true, Y resolution is set whenever   */
  /* X resolution gets changed. true initially, */
  /* becomes false if Y resolution is ever     */
  /* set independently                         */
  int top_left_x;		/* The desired size of the scan, in          */
  int top_left_y;		/*   mm/2^16. Notice that there's a second   */
  int bottom_right_x;		/*   group of these with the "rounded"       */
  int bottom_right_y;		/*   prefix, below.                          */

  /* A note on the scan are size (topLeft/bottomRight) values. When the    
   * user (the front-end) sets one of these, we change the above values   
   * and immediately calculate from it the scan height and width in      
   * 1/1200 inches (the internal scanner unit). Depending on the scan 
   * mode, depth, etc., other rules may be used to change the value, 
   * e.g. in 1bpp scans, the width is always increased until each scan
   * line has a full number of bytes.
   *
   * The results of these calculations are the "top_margin", "left_margin",
   * "scan_width", and "scan_height" values below. In addition, these
   * values are now converted back into SANE units (1/2^16mm) and stored
   * in roundedTopLeft... and roundedBottomRight... - when the front-end
   * _sets_ the scan area size, the topLeft... and bottomRight... values
   * will be set, but when the front-end _gets_ the scan area size, 
   * the rounded values will be returned.
   *
   * By keeping the original values instead of rounding them "in place",
   * we can revise our rounded values later. Assume the resolution is set
   * to 75dpi, and then the scan size is set - it will be rounded to
   * the nearest 1/75 inch. Later the resolution is changed to 600dpi,
   * making a better approximation of the original desired size possible.
   *
   * All these calculations are done within the "calculateDerivedValues"
   * function.
   */

  int page_width;		/* paper width and height may influence the  */
  int page_height;		/* way the ADF operates  - in 1/1200 inch    */

  int output_depth;		/* How many bits per pixel the user wants.   */
  int scanner_depth;		/* How many bpp the scanner will send.       */

  /* A note on "depth" values. Many colour scanners require you to use
   * the value "8" for depth if you're doing colour scans even if they 
   * deliver 24. The front-end, when it calls "sane_get_parameters", will
   * expect to see the value 8 as well. However, internally, we use
   * scanner_depth as the real number of bits sent per pixel, which is 
   * obvoiusly 24 for coulour scans. 
   * 
   * output_depth uses the same logic and normally both values would be
   * identical; however it may be desirable to implement, say, a 4-bit
   * grayscale mode in the backend even if the scanner only supports
   * 8-bit grayscale. In that case, the backend would to the reduction
   * "on the fly", and output_depth would be 4, while scanner_depth is 8.
   */

  int color_mode;		/* Color/Gray/..., use MODE_* constants      */
  int use_adf;			/* Whether the ADF should be used or not.    */
  int use_temp_file;		/* Whether to use a temp file for duplex.    */
  int mirror;			/* Whether to mirror scan area for rear side */
  int lamp_color;		/* lamp color to use for monochrome scans    */
  int green_offset;		/* color tuning - green scan line offset     */
  int blue_offset;		/* color tuning - blue scan line offset      */



  /* Derived values (calculated by calculateDerivedValues from the above)  */
  /* --------------------------------------------------------------------- */

  int rounded_top_left_x;	/* Same as topLeft... and bottomRight...,    */
  int rounded_top_left_y;	/*   but "rounded" to the nearest 1/1200     */
  int rounded_bottom_right_x;	/*   value, i.e. these are the values really */
  int rounded_bottom_right_y;	/*   used for scanning.                      */
  int top_margin;		/* Top margin in 1/1200 inch                 */
  int left_margin;		/* Left margin in 1/1200 inch                */
  int scan_width;		/* Width of scan in 1/1200 inch              */
  int scan_height;		/* Height of scan in 1/1200 inch             */
  int scan_width_pixels;	/* Pixels per scan line for this job         */
  int scan_height_pixels;	/* Number of lines in this job               */
  int bytes_per_scan_line;	/* Number of bytes per line in this job      */

  /* Operational values (contain internal status information etc.)         */
  /* --------------------------------------------------------------------- */

  int default_pipe;		/* Pipe between reader process and backend.  */
  int duplex_pipe;		/* Additional pipe for duplex scans.         */
  int reader_pid;		/* Process ID of the reader process.         */

  int reverse;			/* Whether to reverse the image. Not user    */
  /* settable but required internally.         */

  int i_transfer_length;	/* needed when the scanner returns           */
  /* compressed data and size of return data   */
  /* is unknown in advance */



  /* This replaced the former "scanning" flag. object_count represents the
   * number of objects acquired (or being acquired) by the scanner which
   * haven't been read. Thus, an object_count of != 0 means the scanner is
   * busy. If the scanner is scanning the first page, it's 1; if the scanner
   * is scanning the second page of a duplex scan, it's 2.
   */
  int object_count;

  /* This is set to "true" by sane_read when it encounters the end of a data
   * stream.
   */
  int eof;

  unsigned char *buffer;	/* Buffer used for SCSI transfers.           */
  unsigned int scsi_buf_size;	/* Max amount of data in one SCSI call.      */

  /** these can be set */

  int brightness;
  int threshold;
  int contrast;

  int bitorder;
  int compress_type;
  int compress_arg;
  int vendor_id_code;
  int gamma;
  int outline;
  int emphasis;
  int auto_sep;
  int mirroring;
  int var_rate_dyn_thresh;
  int white_level_follow;
  int gradation;
  int smoothing_mode;
  int filtering;
  int background;
  int matrix2x2;
  int matrix3x3;
  int matrix4x4;
  int matrix5x5;
  int noise_removal;
  int paper_orientation;
  int paper_selection;
  int paper_size;
  int dtc_threshold_curve;
  int dtc_selection;

  int subwindow_list;
  /***** end of "set window" terms *****/

  int dropout_color;

  SANE_Bool use_imprinter;
  int imprinter_direction;
  SANE_Word imprinter_y_offset;
  SANE_Char imprinter_string[max_imprinter_string_length];
  int imprinter_ctr_init;
  int imprinter_ctr_step;
  int imprinter_ctr_dir;

  SANE_Int sleep_time;
};

/** scan only front page */
#define DUPLEX_FRONT 1
/** scan only reverse page */
#define DUPLEX_BACK  2
/** scan both sides and return them in two different image acquisition operations */
#define DUPLEX_BOTH  3

#define MODEL_FORCE 0
/** Fujitsu M3091DCd (300x600dpi, color, duplex, ADF-only) */
#define MODEL_3091 1
/** Fujitsu M3096 (400x400dpi grayscale, simplex, FB and optional ADF) */
#define MODEL_3096 2
/** Fujitsu ScanPartner 15C (....) */
#define MODEL_SP15 3
/** 3093 */
#define MODEL_3093 4
/** 4097 */
#define MODEL_4097 5
/** fi-???? */
#define MODEL_FI   6
#define MODEL_3097 7
/** Fujitsu M3092DCd (300x600dpi, color, duplex, FB and ADF) */
#define MODEL_3092 8

/* A note regarding the MODEL... constants. There's a place in
 * identifyScanner() where the INQUIRY data is parsed and the model
 * set accordingly. Now while there is, for example, only one 3091
 * model (called M3091DCd), there may be different sub-models (e.g.
 * the 1000abc and the 1000xyz).
 * 
 * It is suggested that a new MODEL... type be introduced for these if
 * there are significant differences in capability that have to be
 * treated differently in the backend. For example, if the "abc" model
 * has an optional ADF and the "xyz" model has the ADF built-in, we
 * don't need to distinguish these models, and it's sufficient to set
 * the "has_adf" variable accordingly.
 * 
 * The same would apply if two sub-models behave in the same way but
 * one of them is faster, or has some exotic extra capability that we
 * don't support anyway.
 *
 * If, on the other hand, one model supports a different resolution,
 * page size, different scan parameters and modes, and such, then it's
 * probably necessary to create a new MODEL... constant.
 * 
 * It may also be feasible to introduce a "subModel" member in the
 * structure above for those cases where subtle, but important,
 * differences exist - e.g. one model is 100% the same as another but
 * supports twice the resolution.
 */

/** Black-and-White (Line Art) - depth always 1 */
#define MODE_LINEART 1
/** Black-and-White (Halftone) - depth always 1 */
#define MODE_HALFTONE 2
/** Grayscale */
#define MODE_GRAYSCALE 3
/** RGB Color */
#define MODE_COLOR 4
/** Binary RGB Color*/
#define MODE_BIN_COLOR 5
/** half-tones RGB Color*/
#define MODE_HALFTONE_COLOR 6

/* ------------------------------------------------------------------------- */

#define MM_PER_INCH    25.4
#define length_quant SANE_UNFIX(SANE_FIX(MM_PER_INCH / 1200.0))
#define mmToIlu(mm) ((mm) / length_quant)
#define iluToMm(ilu) ((ilu) * length_quant)
#define FUJITSU_CONFIG_FILE "fujitsu.conf"

#define FIXED_MM_TO_SCANNER_UNIT(number) SANE_UNFIX(number) * 1200 / MM_PER_INCH
#define SCANNER_UNIT_TO_FIXED_MM(number) SANE_FIX(number * MM_PER_INCH / 1200)

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

const SANE_Option_Descriptor *sane_get_option_descriptor (SANE_Handle handle,
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

static void doInquiry (struct fujitsu *s);

static SANE_Status attachScanner (const char *devicename,
				  struct fujitsu **devp);

static SANE_Status scsiSenseHandler (int scsi_fd, u_char * result, void *arg);

static int identifyScanner (struct fujitsu *s);

static void doInquiry (struct fujitsu *s);

static int
do_cmd (Fujitsu_Connection_Type connection, int fd, unsigned char *cmd,
	     int cmd_len, unsigned char *out, size_t req_out_len,
	     size_t *res_out_len);

static int do_scsi_cmd (int fd, unsigned char *cmd, int cmd_len,
			unsigned char *out, size_t req_out_len, 
			size_t *res_out_len);

static int
do_usb_cmd (int fd, unsigned char *cmd,
	     int cmd_len, unsigned char *out, size_t req_out_len,
	     size_t *res_out_len);

static void hexdump (int level, char *comment, unsigned char *p, int l);

static SANE_Status init_options (struct fujitsu *scanner);

static int grabScanner (struct fujitsu *s);

static int freeScanner (struct fujitsu *s);

static int wait_scanner (struct fujitsu *s);

static int object_position (struct fujitsu *s, int i_load);

static SANE_Status doCancel (struct fujitsu *scanner);

static SANE_Status do_reset (struct fujitsu *scanner);

/*static int objectDischarge (struct fujitsu *s);*/

static int fujitsu_set_sleep_mode(struct fujitsu *s);

static int set_mode_params (struct fujitsu *s);

static int imprinter(struct fujitsu *s);

static int fujitsu_send(struct fujitsu *s);

static int setWindowParam (struct fujitsu *s);

static size_t maxStringSize (const SANE_String_Const strings[]);

static int startScan (struct fujitsu *s);

static int reader_process (struct fujitsu *scanner, int fd1, int fd2);

static unsigned int reader3091ColorDuplex (struct fujitsu *scanner, FILE * fd,
					   FILE * fd2);
static unsigned int reader3091ColorSimplex (struct fujitsu *scanner,
					    FILE * fd);
static unsigned int reader3091GrayDuplex (struct fujitsu *scanner, FILE * fd,
					  FILE * fd2);
static unsigned int reader3092ColorDuplex (struct fujitsu *scanner, FILE * fd,
                                          FILE * fd2);
static unsigned int reader3092ColorSimplex (struct fujitsu *scanner,
                                           FILE * fd);
static unsigned int reader3092GrayDuplex (struct fujitsu *scanner, FILE * fd,
                                         FILE * fd2);
static unsigned int reader_gray_duplex_sequential (struct fujitsu *scanner,
						   FILE * fd, FILE * fd2);
static unsigned int reader_gray_duplex_alternate (struct fujitsu *scanner,
						  FILE * fd, FILE * fd2);

static unsigned int readerGenericPassthrough (struct fujitsu *scanner,
					      FILE * fd, int i_window_id);

static int read_large_data_block (struct fujitsu *s,
				  unsigned char *buffer, unsigned int length,
				  int i_window_id, 
				  unsigned int *i_data_read);

static SANE_Status attachOne (const char *name);

static int modelMatch (const char *product);

static void setDefaults3091 (struct fujitsu *scanner);
static void setDefaults3092 (struct fujitsu *scanner);
static void setDefaults3096 (struct fujitsu *scanner);
static void setDefaultsSP15 (struct fujitsu *scanner);

static SANE_Status setMode3091 (struct fujitsu *scanner, int mode);
static SANE_Status setMode3092 (struct fujitsu *scanner, int mode);
static SANE_Status setMode3096 (struct fujitsu *scanner, int mode);
static SANE_Status setModeSP15 (struct fujitsu *scanner, int mode);

static void calculateDerivedValues (struct fujitsu *scanner);

static int makeTempFile (void);
static SANE_Status get_hardware_status (struct fujitsu *s);
static void fujitsu_set_standard_size (SANE_Handle handle);


#endif /* FUJITSU_H */
