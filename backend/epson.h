/* epson.h - SANE library for Epson flatbed scanners.

   based on Kazuhiro Sasayama previous
   Work on epson.[ch] file from the SANE package.

   original code taken from sane-0.71
   Copyright (C) 1997 Hypercore Software Design, Ltd.

   modifications
   Copyright (C) 1998 Christian Bucher
   Copyright (C) 1998 Kling & Hautzinger GmbH

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
   If you do not wish that, delete this exception notice.  */

#ifndef epson_h
#define epson_h 1

typedef struct
  {
    char *level;

    int I:1			/* request identity             */
     ,F:1			/* request status               */
     ,S:1			/* request condition            */
     ,C:1			/* set color mode               */
     ,G:1			/* start scanning               */
     ,D:1			/* set data format              */
     ,R:1			/* set resolution               */
     ,H:1			/* set zoom                     */
     ,A:1			/* set scan area                */
     ,L:1			/* set bright                   */
     ,Z:1			/* set gamma                    */
     ,B:1			/* set halftoning               */
     ,M:1			/* set color correction         */
     ,AT:1			/* initialize scanner           */
     ,g:1			/* set speed                    */
     ,d:1			/* set lcount                   */
     ;

  }
EpsonCmdRec, *EpsonCmd;

enum
  {
    OPT_NUM_OPTS = 0,
    OPT_MODE_GROUP,
    OPT_MODE,
    OPT_HALFTONE,
    OPT_DROPOUT,
    OPT_BRIGHTNESS,
    OPT_RESOLUTION,
    OPT_GEOMETRY_GROUP,
    OPT_TL_X,
    OPT_TL_Y,
    OPT_BR_X,
    OPT_BR_Y,
    NUM_OPTIONS
  };

struct Epson_Device
  {
    SANE_Device sane;
    SANE_Int level;
    SANE_Range dpi_range;
    SANE_Range x_range;
    SANE_Range y_range;
    SANE_Bool is_scsi;
    SANE_Int *res_list;
    SANE_Int res_list_size;
    EpsonCmd cmd;
  };

typedef struct Epson_Device Epson_Device;

struct Epson_Scanner
  {
    int fd;
    Epson_Device *hw;
    SANE_Option_Descriptor opt[NUM_OPTIONS];
    unsigned long val[NUM_OPTIONS];
    SANE_Parameters params;
    SANE_Bool block;
    SANE_Bool eof;
    SANE_Byte *buf, *end, *ptr;
    SANE_Bool canceling;
  };

typedef struct Epson_Scanner Epson_Scanner;

#endif /* not epson_h */
