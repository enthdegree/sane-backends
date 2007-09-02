/**************************************************************************
   lexmark.h - SANE library for Lexmark scanners.
   Copyright (C) 2003-2004 Lexmark International, Inc. (original source)
   Copyright (C) 2005 Fred Odendaal

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
   **************************************************************************/
#ifndef LEXMARK_H
#define LEXMARK_H

/* Force the backend name for all files using this include */
#ifdef BACKEND_NAME
#undef BACKEND_NAME
#define BACKEND_NAME lexmark
#endif

#define DEBUG_NOT_STATIC
#define SANE_NAME_PAPER_SIZE "paper-size"
#define SANE_TITLE_PAPER_SIZE SANE_I18N("Paper size")
#define SANE_DESC_PAPER_SIZE \
SANE_I18N("Selects the size of the area to be scanned.");


typedef enum
{
  OPT_NUM_OPTS = 0,
  OPT_MODE,
/*   OPT_X_DPI, */
/*   OPT_Y_DPI, */
  OPT_RESOLUTION,
  OPT_PREVIEW,
  OPT_SCAN_SIZE,
  OPT_THRESHOLD,
  /* must come last: */
  NUM_OPTIONS
}
Lexmark_Options;

typedef enum
{
  RED = 0,
  GREEN,
  BLUE
}
Scan_Regions;

/** @name Option_Value union
 * convenience union to access option values given to the backend
 * @{
 */
#ifndef SANE_OPTION
#define SANE_OPTION 1
typedef union
{
  SANE_Bool b;
  SANE_Word w;
  SANE_Word *wa;		/* word array */
  SANE_String s;
}
Option_Value;
#endif
/* @} */

typedef struct Read_Buffer
{
  SANE_Int gray_offset;
  SANE_Int max_gray_offset;
  SANE_Int region;
  SANE_Int red_offset;
  SANE_Int green_offset;
  SANE_Int blue_offset;
  SANE_Int max_red_offset;
  SANE_Int max_green_offset;
  SANE_Int max_blue_offset;
  SANE_Byte *data;
  SANE_Byte *readptr;
  SANE_Byte *writeptr;
  SANE_Byte *max_writeptr;
  size_t size;
  size_t linesize;
  SANE_Bool empty;
  SANE_Int image_line_no;
  SANE_Int bit_counter;
  SANE_Int max_lineart_offset;
}
Read_Buffer;

typedef struct Lexmark_Device
{
  struct Lexmark_Device *next;

  SANE_Device sane;
  SANE_Option_Descriptor opt[NUM_OPTIONS];
  Option_Value val[NUM_OPTIONS];
  SANE_Parameters params;
  SANE_Int devnum;
  long data_size;
  SANE_Int pixel_height;
  SANE_Int pixel_width;
  SANE_Bool initialized;
  SANE_Bool eof;
  SANE_Int x_dpi;
  SANE_Int y_dpi;
  long data_ctr;
  SANE_Bool device_cancelled;
  SANE_Int cancel_ctr;
  SANE_Byte *transfer_buffer;
  size_t bytes_remaining;
  size_t bytes_in_buffer;
  SANE_Byte *read_pointer;
  Read_Buffer *read_buffer;
  SANE_Byte threshold;
}
Lexmark_Device;

/* Maximum transfer size */
#define MAX_XFER_SIZE 0xFFC0

/* Non-static Function Proto-types (called by lexmark.c) */
SANE_Status sanei_lexmark_x1100_init (void);
void sanei_lexmark_x1100_destroy (Lexmark_Device * dev);
SANE_Status sanei_lexmark_x1100_open_device (SANE_String_Const devname, 
					     SANE_Int * devnum);
void sanei_lexmark_x1100_close_device (SANE_Int devnum);
SANE_Bool sanei_lexmark_x1100_search_home_fwd (Lexmark_Device * dev);
void sanei_lexmark_x1100_move_fwd (SANE_Int distance, Lexmark_Device * dev);
SANE_Bool sanei_lexmark_x1100_search_home_bwd (Lexmark_Device * dev);
SANE_Int sanei_lexmark_x1100_find_start_line (SANE_Int devnum);
SANE_Status sanei_lexmark_x1100_set_scan_regs (Lexmark_Device * dev, 
					       SANE_Int offset);
SANE_Status sanei_lexmark_x1100_start_scan (Lexmark_Device * dev);
long sanei_lexmark_x1100_read_scan_data (SANE_Byte * data, SANE_Int size, 
					 Lexmark_Device * dev);

#endif /* LEXMARK_H */
