/* sane - Scanner Access Now Easy.
   Copyright (C) 1997 David Mosberger-Tang
   This file is part of the SANE package.

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

   Portions of this code are derived from Scott Laird's qcam driver.
   It's copyright notice is reproduced here:

   Copyright (C) 1996 by Scott Laird

   Permission is hereby granted, free of charge, to any person
   obtaining a copy of this software and associated documentation
   files (the "Software"), to deal in the Software without
   restriction, including without limitation the rights to use, copy,
   modify, merge, publish, distribute, sublicense, and/or sell copies
   of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
   NONINFRINGEMENT.  IN NO EVENT SHALL SCOTT LAIRD BE LIABLE FOR ANY
   CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
   CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
   WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.  */

#ifndef v4l_h
#define v4l_h

#include <sane/sane.h>

typedef enum
  {
    V4L_MONO	= 0x01,
    V4L_COLOR	= 0x10
  }
V4L_Model;

typedef enum
  {
    V4L_RES_LOW = 0,
    V4L_RES_HIGH
  }
V4L_Resolution;

/* commands common to all quick-cameras: */
typedef enum
  {
    V4L_SEND_VIDEO_FRAME		=  7,
    V4L_SET_BRIGHTNESS		= 11,
    V4L_SET_TOP			= 13,
    V4L_SET_LEFT			= 15,
    V4L_SET_NUM_V		= 17,
    V4L_SET_NUM_H		= 19,
    V4L_SEND_VERSION		= 23,
    V4L_SET_BLACK		= 29,
    V4L_SET_WHITE		= 31,
    V4L_SET_SATURATION		= 35,
    V4L_SEND_STATUS		= 41,
    V4L_SET_SPEED		= 45
  }
V4L_Command;

/* commands for grayscale camera: */
typedef enum
  {
    V4L_MONO_SET_CONTRAST	= 25,
    V4L_MONO_AUTO_ADJUST_OFFSET	= 27,
    V4L_MONO_GET_OFFSET		= 33
  }
V4L_Mono_Command;

/* commands for color camera: */
typedef enum
  {
    V4L_COL_LOAD_RAM		= 27,
    V4L_COL_SET_HUE		= 33,
    V4L_COL_SET_CONTRAST		= 37
  }
V4L_Col_Command;

typedef enum
  {
    OPT_NUM_OPTS = 0,

    OPT_MODE_GROUP,
    OPT_DEPTH,			/* 4 or 6 (b&w) or 24 (color) */
    OPT_RESOLUTION,		/* resolution in pixels */
    OPT_XFER_SCALE,		/* transfer-scale */
    OPT_DESPECKLE,		/* turn on despeckling? */
    OPT_TEST,			/* test image */

    OPT_GEOMETRY_GROUP,
    OPT_TL_X,			/* top-left x */
    OPT_TL_Y,			/* top-left y */
    OPT_BR_X,			/* bottom-right x */
    OPT_BR_Y,			/* bottom-right y */

    OPT_ENHANCEMENT_GROUP,
    OPT_BRIGHTNESS,
    OPT_CONTRAST,
    OPT_BLACK_LEVEL,
    OPT_WHITE_LEVEL,
    OPT_HUE,
    OPT_SATURATION,

    /* must come last: */
    NUM_OPTIONS
  }
V4L_Option;

typedef union
  {
    SANE_Word w;
    SANE_Word *wa;		/* word array */
    SANE_String s;
  }
Option_Value;

typedef enum
  {
    V4L_UNIDIR,
    V4L_BIDIR
  }
V4L_Port_Mode;

typedef struct
  {
    size_t num_bytes;		/* # of bytes to read */
    V4L_Resolution resolution;	/* high-resolution? */
    SANE_Parameters params;	/* other parameters */
    u_int mode;			/* qcam scan code (get video data command) */
    int despeckle;		/* apply despeckling filter? */
  }
V4L_Scan_Request;

typedef struct V4L_Device
  {
    struct V4L_Device * next;
    SANE_Device sane;
    V4L_Port_Mode port_mode;
    int version;		/* camera version */
    int lock_fd;		/* used for locking protocol */
  }
V4L_Device;

typedef struct V4L_Scanner
  {
    struct V4L_Scanner *next;

    SANE_Option_Descriptor opt[NUM_OPTIONS];
    Option_Value val[NUM_OPTIONS];
    V4L_Resolution resolution;
    SANE_Parameters params;
    char* devicename;		/* Name of the Device */
    int fd;			/* Filedescriptor */
    SANE_Int user_corner;	/* bitmask of user-selected coordinates */
    SANE_Int value_changed;	/* bitmask of options that were set */
    SANE_Bool scanning;
    SANE_Bool deliver_eof;
    SANE_Bool mmap;		/* Do we use mmap ? */
    /* state for reading a frame: */
    size_t num_bytes;		/* # of bytes read so far */
    size_t bytes_per_frame;	/* total number of bytes in frame */
    /* internal state for v4l_readbytes(): */
    int readbytes_state;
    unsigned int saved_bits;
  }
V4L_Scanner;

#endif /* v4l_h */
