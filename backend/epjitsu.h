#ifndef EPJITSU_H
#define EPJITSU_H

/* 
 * Part of SANE - Scanner Access Now Easy.
 * Please see opening comment in epjitsu.c
 */

/* ------------------------------------------------------------------------- 
 * This option list has to contain all options for all scanners supported by
 * this driver. If a certain scanner cannot handle a certain option, there's
 * still the possibility to say so, later.
 */
enum scanner_Option
{
  OPT_NUM_OPTS = 0,

  OPT_MODE_GROUP,
  OPT_SOURCE,   /*adffront/adfback/adfduplex/fb*/
  OPT_MODE,     /*mono/gray/color*/
  OPT_X_RES,
  OPT_Y_RES,

  /* must come last: */
  NUM_OPTIONS
};

#define FIRMWARE_LENGTH 0x10000
#define MAX_IMG_PASS 0x10000
#define MAX_IMG_BLOCK 0x80000

struct transfer {
  int height;
  
  int width_pix;
  int width_bytes;

  int total_pix;
  int total_bytes;

  int rx_bytes;
  int tx_bytes;

  unsigned char * buffer;
};

struct scanner
{
  /* --------------------------------------------------------------------- */
  /* immutable values which are set during init of scanner.                */
  struct scanner *next;

  int model;

  int has_fb;
  int has_adf;
  int x_res_150;
  int x_res_300;
  int x_res_600;
  int y_res_150;
  int y_res_300;
  int y_res_600;

  /* --------------------------------------------------------------------- */
  /* immutable values which are set during inquiry probing of the scanner. */
  SANE_Device sane; /*contains: name, vendor, model, type*/

  /* --------------------------------------------------------------------- */
  /* changeable SANE_Option structs provide our interface to frontend.     */

  /* long array of option structs */
  SANE_Option_Descriptor opt[NUM_OPTIONS];

  /* --------------------------------------------------------------------- */
  /* some options require lists of strings or numbers, we keep them here   */
  /* instead of in global vars so that they can differ for each scanner    */

  /*mode group, room for lineart, gray, color, null */
  SANE_String_Const source_list[5];
  SANE_String_Const mode_list[4];
  SANE_Int x_res_list[4];
  SANE_Int y_res_list[4];

  /* --------------------------------------------------------------------- */
  /* changeable vars to hold user input. modified by SANE_Options above    */

  /*mode group*/
  int source;         /* adf or fb */
  int mode;           /* color,lineart,etc */
  int res;            /* from a limited list, x and y same */
  int resolution_x;   /* unused dummy */
  int resolution_y;   /* unused dummy */
  int threshold;
  int height;         /* may run out on adf */

  /* --------------------------------------------------------------------- */
  /* values which are set by user parameter changes, scanner specific      */
  unsigned char * setWindowCoarseCal;   /* sent before coarse cal */
  size_t setWindowCoarseCalLen;

  unsigned char * setWindowFineCal;   /* sent before fine cal */
  size_t setWindowFineCalLen;

  unsigned char * setWindowSendCal;   /* sent before send cal */
  size_t setWindowSendCalLen;

  unsigned char * sendCal1Header; /* part of 1b c3 command */
  size_t sendCal1HeaderLen;

  unsigned char * sendCal2Header; /* part of 1b c4 command */
  size_t sendCal2HeaderLen;

  unsigned char * setWindowScan;  /* sent before scan */
  size_t setWindowScanLen;
  
  /* --------------------------------------------------------------------- */
  /* values which are set by scanning functions to keep track of pages, etc */
  int started;
  int side;
  int send_eof; /*we've sent all of image*/

  /* requested size params (almost no relation to actual data?) */
  int req_width;   /* pixel width of first read-head? */
  int head_width;
  int pad_width;

  /* holds temp buffer for getting 1 line of cal data */
  struct transfer coarsecal;

  /* holds temp buffer for getting 32 lines of cal data */
  struct transfer darkcal;

  /* holds temp buffer for getting 32 lines of cal data */
  struct transfer lightcal;

  /* holds temp buffer for building calibration data */
  struct transfer sendcal;

  /* scanner transmits more data per line than requested */
  /* due to padding and/or duplex interlacing */
  /* the scan struct holds these larger numbers, but buffer is unused */
  struct transfer scan;

  /* scanner transmits data in blocks, up to 512k */
  /* but always ends on a scanline. */
  /* the block struct holds the most recent buffer */
  struct transfer block;

  /* final-sized front image, always used */
  struct transfer front;

  /* final-sized back image, only used during duplex/backside */
  struct transfer back;

  /* --------------------------------------------------------------------- */
  /* values used by the command and data sending function                  */
  int fd;                       /* The scanner device file descriptor.     */

};

#define MODEL_S300 0
#define MODEL_FI60F 1

#define USB_COMMAND_TIME   10000
#define USB_DATA_TIME      10000

#define SIDE_FRONT 0
#define SIDE_BACK 1

#define SOURCE_FLATBED 0
#define SOURCE_ADF_FRONT 1
#define SOURCE_ADF_BACK 2
#define SOURCE_ADF_DUPLEX 3

#define MODE_COLOR 0
#define MODE_GRAYSCALE 1
#define MODE_LINEART 2

#define WINDOW_COARSECAL 0
#define WINDOW_FINECAL 1
#define WINDOW_SENDCAL 2
#define WINDOW_SCAN 3

/* ------------------------------------------------------------------------- */

#define MM_PER_INCH    25.4
#define MM_PER_UNIT_UNFIX SANE_UNFIX(SANE_FIX(MM_PER_INCH / 1200.0))
#define MM_PER_UNIT_FIX SANE_FIX(SANE_UNFIX(SANE_FIX(MM_PER_INCH / 1200.0)))

#define SCANNER_UNIT_TO_FIXED_MM(number) SANE_FIX((number) * MM_PER_UNIT_UNFIX)
#define FIXED_MM_TO_SCANNER_UNIT(number) SANE_UNFIX(number) / MM_PER_UNIT_UNFIX

#define CONFIG_FILE "epjitsu.conf"

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

static SANE_Status attach_one (const char *devicename);
static SANE_Status connect_fd (struct scanner *s);
static SANE_Status disconnect_fd (struct scanner *s);

static SANE_Status
do_cmd(struct scanner *s, int shortTime,
 unsigned char * cmdBuff, size_t cmdLen,
 unsigned char * outBuff, size_t outLen,
 unsigned char * inBuff, size_t * inLen
);

/*
static SANE_Status load_calibration (struct scanner *s);
static SANE_Status read_from_scanner_gray(struct scanner *s);
*/

/* commands */
static SANE_Status load_fw(struct scanner *s);
static SANE_Status get_ident(struct scanner *s);

static SANE_Status change_params(struct scanner *s);
void update_block_totals(struct scanner * s);

static SANE_Status teardown_buffers(struct scanner *s);
static SANE_Status setup_buffers(struct scanner *s);

static SANE_Status ingest(struct scanner *s);
static SANE_Status coarsecal(struct scanner *s);
static SANE_Status finecal(struct scanner *s);
static SANE_Status lamp(struct scanner *s, unsigned char set);
static SANE_Status set_window(struct scanner *s, int window);
static SANE_Status scan(struct scanner *s);

static SANE_Status read_from_scanner(struct scanner *s, struct transfer *tp);
static SANE_Status fill_frontback_buffers_S300(struct scanner *s);
static SANE_Status fill_frontback_buffers_FI60F(struct scanner *s);

/* utils */
static void hexdump (int level, char *comment, unsigned char *p, int l);
static size_t maxStringSize (const SANE_String_Const strings[]);

#endif /* EPJITSU_H */
