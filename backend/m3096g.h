#ifndef M3096G_H
#define M3096G_H

static const char RCSid_h[] = "$Header$";
/* sane - Scanner Access Now Easy.

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

   This file implements a SANE backend for Fujitsu M3096G
   flatbed/ADF scanners.  It was derived from the COOLSCAN driver.
   Written by Randolph Bentson <bentson@holmsjoen.com> */

/* ------------------------------------------------------------------------- */
/*
 * $Log$
 * Revision 1.2  2000/03/05 13:55:07  pere
 * Merged main branch with current DEVEL_1_9.
 *
 * Revision 1.1.2.2  2000/01/26 03:51:47  pere
 * Updated backends sp15c (v1.12) and m3096g (v1.11).
 *
 * Revision 1.8  2000/01/25 16:25:34  bentson
 * clean-up compiler warnings
 *
 * Revision 1.7  2000/01/05 05:26:00  bentson
 * indent to barfin' GNU style
 *
 * Revision 1.6  1999/11/24 20:06:58  bentson
 * add license verbiage
 *
 * Revision 1.5  1999/11/23 18:47:43  bentson
 * add some constraint checking
 *
 * Revision 1.4  1999/11/19 17:30:07  bentson
 * enhance control of device; remove unused cruft
 *
 * Revision 1.3  1999/11/18 18:13:36  bentson
 * basic grayscale scanning works
 *
 * Revision 1.2  1999/11/17 00:36:22  bentson
 * basic lineart scanning works
 *
 * Revision 1.1  1999/11/12 05:41:45  bentson
 * can move paper, but not yet scan
 *
 */

static int num_devices;
static struct m3096g *first_dev;

enum m3096g_Option
  {
    OPT_NUM_OPTS = 0,

    OPT_MODE_GROUP,
    OPT_SOURCE,
    OPT_MODE,
    OPT_TYPE,
    OPT_X_RES,
    OPT_Y_RES,
    OPT_PRESCAN,
    OPT_PREVIEW_RES,

    OPT_GEOMETRY_GROUP,
    OPT_TL_X,			/* in mm/2^16 */
    OPT_TL_Y,			/* in mm/2^16 */
    OPT_BR_X,			/* in mm/2^16 */
    OPT_BR_Y,			/* in mm/2^16 */

    OPT_ENHANCEMENT_GROUP,
    OPT_AVERAGING,
    OPT_BRIGHTNESS,
    OPT_THRESHOLD,

    OPT_ADVANCED_GROUP,
    OPT_PREVIEW,

    /* must come last: */
    NUM_OPTIONS
  };

struct m3096g
  {
    struct m3096g *next;

    SANE_Option_Descriptor opt[NUM_OPTIONS];
    SANE_Device sane;

    char vendor[9];
    char product[17];
    char version[5];

    char *devicename;		/* name of the scanner device */
    int sfd;			/* output file descriptor, scanner device */
    int pipe;

    int scanning;		/* "in progress" flag */
    int autofeeder;		/* detected */
    int use_adf;		/* requested */
    int reader_pid;		/* child is running */
    int prescan;		/* ??? */

/***** terms for "set window" command *****/
    int x_res;			/* resolution in */
    int y_res;			/* pixels/inch */
    int tl_x;			/* top left position, */
    int tl_y;			/* in inch/1200 units */
    int br_x;			/* bottom right position, */
    int br_y;			/* in inch/1200 units */

    int brightness;
    int threshold;
    int contrast;
    int composition;
    int bitsperpixel;
    int halftone;
    int rif;
    int bitorder;
    int compress_type;
    int compress_arg;
    int vendor_id_code;
    int outline;
    int emphasis;
    int auto_sep;
    int mirroring;
    int var_rate_dyn_thresh;
    int white_level_follow;
    int subwindow_list;
    int paper_size;
    int paper_width_X;
    int paper_length_Y;
/***** end of "set window" terms *****/

    /* buffer used for scsi-transfer */
    unsigned char *buffer;
    unsigned int row_bufsize;

  };

/* ------------------------------------------------------------------------- */

#define MM_PER_INCH	25.4
#define length_quant SANE_UNFIX(SANE_FIX(MM_PER_INCH / 1200.0))
#define mmToIlu(mm) ((mm) / length_quant)
#define iluToMm(ilu) ((ilu) * length_quant)
#define M3096G_CONFIG_FILE "m3096g.conf"

/* ------------------------------------------------------------------------- */

static void
  m3096g_do_inquiry (struct m3096g *s);

/* ------------------------------------------------------------------------- */

SANE_Status
sane_init (SANE_Int * version_code, SANE_Auth_Callback authorize);

SANE_Status
sane_get_devices (const SANE_Device *** device_list, SANE_Bool local_only);

SANE_Status
sane_open (SANE_String_Const name, SANE_Handle * handle);

SANE_Status
sane_set_io_mode (SANE_Handle h, SANE_Bool non_blocking);

SANE_Status
sane_get_select_fd (SANE_Handle h, SANE_Int * fdp);

const SANE_Option_Descriptor *
  sane_get_option_descriptor (SANE_Handle handle, SANE_Int option);

SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option,
		     SANE_Action action, void *val, SANE_Int * info);

SANE_Status
sane_start (SANE_Handle handle);

SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters * params);

SANE_Status
sane_read (SANE_Handle handle, SANE_Byte * buf,
	   SANE_Int max_len, SANE_Int * len);

void
  sane_cancel (SANE_Handle h);

void
  sane_close (SANE_Handle h);

void
  sane_exit (void);

/* ------------------------------------------------------------------------- */

static SANE_Status
  attach_scanner (const char *devicename, struct m3096g **devp);

static SANE_Status
  sense_handler (int scsi_fd, u_char * result, void *arg);

static int
  request_sense_parse (u_char * sensed_data);

static int
  m3096g_identify_scanner (struct m3096g *s);

static void
  m3096g_do_inquiry (struct m3096g *s);

static int
  do_scsi_cmd (int fd, char *cmd, int cmd_len, char *out, size_t out_len);

static void
  hexdump (int level, char *comment, unsigned char *p, int l);

static SANE_Status
  init_options (struct m3096g *scanner);

static int
  m3096g_check_values (struct m3096g *s);

static int
  m3096g_grab_scanner (struct m3096g *s);

static int
  m3096g_free_scanner (struct m3096g *s);

static int
  wait_scanner (struct m3096g *s);

static int
  m3096g_object_position (struct m3096g *s);

static SANE_Status
  do_cancel (struct m3096g *scanner);

static void
  swap_res (struct m3096g *s);

static int
  m3096g_object_discharge (struct m3096g *s);

static int
  m3096g_set_window_param (struct m3096g *s, int prescan);

static size_t
  max_string_size (const SANE_String_Const strings[]);

static int
  m3096g_start_scan (struct m3096g *s);

static int
  reader_process (struct m3096g *scanner, int pipe_fd);

static SANE_Status
  do_eof (struct m3096g *scanner);

static int
  pixels_per_line (struct m3096g *s);

static int
  lines_per_scan (struct m3096g *s);

static int
  bytes_per_line (struct m3096g *s);

static void
  m3096g_trim_rowbufsize (struct m3096g *s);

static int
  m3096g_read_data_block (struct m3096g *s, unsigned int length);

static SANE_Status
  attach_one (const char *name);

static int
  m3096g_valid_number (int value, const int *acceptable);

#endif /* M3096G_H */
