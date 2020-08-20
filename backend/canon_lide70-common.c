/* sane - Scanner Access Now Easy.

   BACKEND canon_lide70

   Copyright (C) 2019 Juergen Ernst and pimvantend.

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

   This file implements a SANE backend for the Canon CanoScan LiDE 70 */

#include <errno.h>
#include <fcntl.h>		/* open */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>		/* usleep */
#include <time.h>
#include <math.h>		/* exp() */
#ifdef HAVE_OS2_H
#include <sys/types.h>		/* mode_t */
#endif
#include <sys/stat.h>

#define USB_TYPE_VENDOR   (0x02 << 5)
#define USB_RECIP_DEVICE   0x00
#define USB_DIR_OUT        0x00
#define USB_DIR_IN         0x80

#define MSEC               1000	/* 1ms = 1000us */

/* Assign status and verify a good return code */
#define CHK(A) {if ((status = A) != SANE_STATUS_GOOD) {\
                DBG (1, "Failure on line of %s: %d\n", \
                     __FILE__, __LINE__ ); return A; }}

typedef SANE_Byte byte;

/*****************************************************
           Canon LiDE70 calibration and scan
******************************************************/

/* at 600 dpi */
#define CANON_MAX_WIDTH    5104	/* 8.5in */
/* this may not be right */
#define CANON_MAX_HEIGHT   7300	/* 11.66in */
/* Just for my scanner, or is this universal?  Calibrate? */

/* data structures and constants */
typedef struct CANON_Handle
{
  /* options */
  SANE_Option_Descriptor opt[num_options];
  Option_Value val[num_options];
  SANE_Parameters params;

  SANE_Word graymode;
  char *product;		/* product name */
  int productcode;		/* product code, 0x2224 or 0x2225 */
  int fd;			/* scanner fd */
  int x1, x2, y1, y2;		/* in pixels, at 600 dpi */
  long width, height;		/* at scan resolution */
  unsigned char value_08, value_09;	/* left */
  unsigned char value_0a, value_0b;	/* right */
  unsigned char value_67, value_68;	/* bottom */
  unsigned char value_51;	/* lamp colors */
  int resolution;		/* dpi */
  char *fname;			/* output file name */
  FILE *fp;			/* output file pointer (for reading) */
  unsigned char absolute_threshold;
}
CANON_Handle;


static byte cmd_buffer[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/*****************************************************
            CP2155 communication primitives
   Provides I/O routines to Philips CP2155BE chip
******************************************************/

typedef int CP2155_Register;

/* Write single byte to CP2155 register */
static SANE_Status
cp2155_set (int fd, CP2155_Register reg, byte data)
{
  SANE_Status status;
  size_t count;

  cmd_buffer[0] = (reg >> 8) & 0xff;
  cmd_buffer[1] = (reg) & 0xff;
  cmd_buffer[2] = 0x01;
  cmd_buffer[3] = 0x00;
  cmd_buffer[4] = data;

  count = 5;
  status = sanei_usb_write_bulk (fd, cmd_buffer, &count);

  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "cp2155_set: sanei_usb_write_bulk error\n");
    }

  return status;
}

/* Read single byte from CP2155 register */
static SANE_Status
cp2155_get (int fd, CP2155_Register reg, byte * data)
{
  SANE_Status status;
  size_t count;

  cmd_buffer[0] = 0x01;
  cmd_buffer[1] = (reg) & 0xff;
  cmd_buffer[2] = 0x01;
  cmd_buffer[3] = 0x00;

  count = 4;
  status = sanei_usb_write_bulk (fd, cmd_buffer, &count);

  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "cp2155_get: sanei_usb_write_bulk error\n");
      return status;
    }

  usleep (1 * MSEC);

  count = 1;
  status = sanei_usb_read_bulk (fd, data, &count);

  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "cp2155_get: sanei_usb_read_bulk error\n");
    }

  return status;
}

/* Write a block of data to CP2155 chip */
static SANE_Status
cp2155_write (int fd, byte * data, size_t size)
{
  SANE_Status status;
  size_t count = size + 4;

  cmd_buffer[0] = 0x04;
  cmd_buffer[1] = 0x70;
  cmd_buffer[2] = (size) & 0xff;
  cmd_buffer[3] = (size >> 8) & 0xff;
  memcpy (cmd_buffer + 4, data, size);

  status = sanei_usb_write_bulk (fd, cmd_buffer, &count);

  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "cp2155_write: sanei_usb_write_bulk error\n");
    }

  return status;
}

/* Read a block of data from CP2155 chip */
static SANE_Status
cp2155_read (int fd, byte * data, size_t size)
{
  SANE_Status status;
  size_t count;

  cmd_buffer[0] = 0x05;
  cmd_buffer[1] = 0x70;
  cmd_buffer[2] = (size) & 0xff;
  cmd_buffer[3] = (size >> 8) & 0xff;

  count = 4;
  status = sanei_usb_write_bulk (fd, cmd_buffer, &count);

  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "cp2155_read: sanei_usb_write_bulk error\n");
      return status;
    }

  usleep (1 * MSEC);

  count = size;
  status = sanei_usb_read_bulk (fd, data, &count);
/*
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "cp2155_read: sanei_usb_read_bulk error %lu\n", (u_long) count);
    }
*/
  return status;
}

/*****************************************************/

static void
cp2155_block1 (int fd, byte v001, unsigned int addr, byte * data, size_t size)
{
  size_t count = size;

  while ((count & 0x0f) != 0)
    {
      count++;
    }

  byte pgLO = (count) & 0xff;
  byte pgHI = (count >> 8) & 0xff;
/*
  DBG (1, "cp2155_block1 %06x %02x %04lx %04lx\n", addr, v001, (u_long) size,
       (u_long) count);
*/
  cp2155_set (fd, 0x71, 0x01);
  cp2155_set (fd, 0x0230, 0x11);
  cp2155_set (fd, 0x71, v001);
  cp2155_set (fd, 0x72, pgHI);
  cp2155_set (fd, 0x73, pgLO);
  cp2155_set (fd, 0x74, (addr >> 16) & 0xff);
  cp2155_set (fd, 0x75, (addr >> 8) & 0xff);
  cp2155_set (fd, 0x76, (addr) & 0xff);
  cp2155_set (fd, 0x0239, 0x40);
  cp2155_set (fd, 0x0238, 0x89);
  cp2155_set (fd, 0x023c, 0x2f);
  cp2155_set (fd, 0x0264, 0x20);
  cp2155_write (fd, data, count);
}

/* size=0x0100 */
/* gamma table red*/
static byte cp2155_gamma_red_data[] = {
  0x00, 0x14, 0x1c, 0x26, 0x2a, 0x2e, 0x34, 0x37, 0x3a, 0x3f, 0x42, 0x44,
  0x48, 0x4a, 0x4c, 0x50,
  0x52, 0x53, 0x57, 0x58, 0x5c, 0x5d, 0x5f, 0x62, 0x63, 0x64, 0x67, 0x68,
  0x6a, 0x6c, 0x6e, 0x6f,
  0x71, 0x72, 0x74, 0x76, 0x77, 0x78, 0x7a, 0x7c, 0x7e, 0x7f, 0x80, 0x82,
  0x83, 0x84, 0x86, 0x87,
  0x88, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f, 0x91, 0x92, 0x93, 0x95, 0x96,
  0x97, 0x98, 0x99, 0x9b,
  0x9b, 0x9c, 0x9e, 0x9f, 0x9f, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
  0xa8, 0xa9, 0xaa, 0xab,
  0xac, 0xad, 0xae, 0xaf, 0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb6,
  0xb8, 0xb8, 0xb9, 0xba,
  0xbb, 0xbc, 0xbd, 0xbe, 0xbf, 0xc0, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5,
  0xc5, 0xc6, 0xc7, 0xc8,
  0xc9, 0xc9, 0xca, 0xcb, 0xcc, 0xcc, 0xce, 0xce, 0xcf, 0xd0, 0xd1, 0xd2,
  0xd2, 0xd3, 0xd4, 0xd5,
  0xd5, 0xd6, 0xd7, 0xd7, 0xd9, 0xd9, 0xda, 0xdb, 0xdb, 0xdc, 0xdd, 0xdd,
  0xdf, 0xdf, 0xe0, 0xe1,
  0xe1, 0xe2, 0xe3, 0xe3, 0xe4, 0xe5, 0xe5, 0xe6, 0xe7, 0xe7, 0xe8, 0xe9,
  0xe9, 0xea, 0xeb, 0xeb,
  0xec, 0xed, 0xed, 0xee, 0xef, 0xef, 0xf0, 0xf1, 0xf1, 0xf2, 0xf3, 0xf3,
  0xf4, 0xf5, 0xf5, 0xf6,
  0xf7, 0xf7, 0xf8, 0xf9, 0xfa, 0xfa, 0xfa, 0xfb, 0xfc, 0xfc, 0xfd, 0xfe,
  0xfe, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff
};

/* size=0x0100 */
/* gamma table */
static byte cp2155_gamma_greenblue_data[] = {
  0x00, 0x14, 0x1c, 0x21, 0x26, 0x2a, 0x2e, 0x31, 0x34, 0x37, 0x3a, 0x3d,
  0x3f, 0x42, 0x44, 0x46,
  0x48, 0x4a, 0x4c, 0x4e, 0x50, 0x52, 0x53, 0x55, 0x57, 0x58, 0x5a, 0x5c,
  0x5d, 0x5f, 0x60, 0x62,
  0x63, 0x64, 0x66, 0x67, 0x68, 0x6a, 0x6b, 0x6c, 0x6e, 0x6f, 0x70, 0x71,
  0x72, 0x74, 0x75, 0x76,
  0x77, 0x78, 0x79, 0x7a, 0x7c, 0x7d, 0x7e, 0x7f, 0x80, 0x81, 0x82, 0x83,
  0x84, 0x85, 0x86, 0x87,
  0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8d, 0x8e, 0x8f, 0x90, 0x91, 0x92,
  0x93, 0x94, 0x95, 0x96,
  0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0x9f,
  0xa0, 0xa1, 0xa2, 0xa3,
  0xa3, 0xa4, 0xa5, 0xa6, 0xa6, 0xa7, 0xa8, 0xa9, 0xa9, 0xaa, 0xab, 0xac,
  0xac, 0xad, 0xae, 0xaf,
  0xaf, 0xb0, 0xb1, 0xb1, 0xb2, 0xb3, 0xb4, 0xb4, 0xb5, 0xb6, 0xb6, 0xb7,
  0xb8, 0xb8, 0xb9, 0xba,
  0xba, 0xbb, 0xbc, 0xbc, 0xbd, 0xbe, 0xbe, 0xbf, 0xc0, 0xc0, 0xc1, 0xc1,
  0xc2, 0xc3, 0xc3, 0xc4,
  0xc5, 0xc5, 0xc6, 0xc6, 0xc7, 0xc8, 0xc8, 0xc9, 0xc9, 0xca, 0xcb, 0xcb,
  0xcc, 0xcc, 0xcd, 0xce,
  0xce, 0xcf, 0xcf, 0xd0, 0xd1, 0xd1, 0xd2, 0xd2, 0xd3, 0xd3, 0xd4, 0xd5,
  0xd5, 0xd6, 0xd6, 0xd7,
  0xd7, 0xd8, 0xd9, 0xd9, 0xda, 0xda, 0xdb, 0xdb, 0xdc, 0xdc, 0xdd, 0xdd,
  0xde, 0xdf, 0xdf, 0xe0,
  0xe0, 0xe1, 0xe1, 0xe2, 0xe2, 0xe3, 0xe3, 0xe4, 0xe4, 0xe5, 0xe5, 0xe6,
  0xe6, 0xe7, 0xe7, 0xe8,
  0xe8, 0xe9, 0xe9, 0xea, 0xea, 0xeb, 0xeb, 0xec, 0xec, 0xed, 0xed, 0xee,
  0xee, 0xef, 0xef, 0xf0,
  0xf0, 0xf1, 0xf1, 0xf2, 0xf2, 0xf3, 0xf3, 0xf4, 0xf4, 0xf5, 0xf5, 0xf6,
  0xf6, 0xf7, 0xf7, 0xf8,
  0xf8, 0xf9, 0xf9, 0xfa, 0xfa, 0xfa, 0xfb, 0xfb, 0xfc, 0xfc, 0xfd, 0xfd,
  0xfe, 0xfe, 0xff, 0xff
};

/* size=0x01f4 */
static byte cp2155_slope09_back[] = {
  0x80, 0x25, 0x00, 0x25, 0x84, 0x24, 0x0b, 0x24, 0x96, 0x23, 0x23, 0x23,
  0xb3, 0x22, 0x46, 0x22,
  0xdb, 0x21, 0x73, 0x21, 0x0e, 0x21, 0xab, 0x20, 0x4a, 0x20, 0xeb, 0x1f,
  0x8f, 0x1f, 0x34, 0x1f,
  0xdc, 0x1e, 0x85, 0x1e, 0x31, 0x1e, 0xde, 0x1d, 0x8d, 0x1d, 0x3e, 0x1d,
  0xf0, 0x1c, 0xa4, 0x1c,
  0x59, 0x1c, 0x10, 0x1c, 0xc9, 0x1b, 0x83, 0x1b, 0x3e, 0x1b, 0xfa, 0x1a,
  0xb8, 0x1a, 0x77, 0x1a,
  0x38, 0x1a, 0xf9, 0x19, 0xbc, 0x19, 0x80, 0x19, 0x44, 0x19, 0x0a, 0x19,
  0xd1, 0x18, 0x99, 0x18,
  0x62, 0x18, 0x2c, 0x18, 0xf7, 0x17, 0xc3, 0x17, 0x8f, 0x17, 0x5d, 0x17,
  0x2b, 0x17, 0xfa, 0x16,
  0xca, 0x16, 0x9b, 0x16, 0x6c, 0x16, 0x3e, 0x16, 0x11, 0x16, 0xe5, 0x15,
  0xb9, 0x15, 0x8e, 0x15,
  0x64, 0x15, 0x3a, 0x15, 0x11, 0x15, 0xe9, 0x14, 0xc1, 0x14, 0x9a, 0x14,
  0x73, 0x14, 0x4d, 0x14,
  0x27, 0x14, 0x02, 0x14, 0xde, 0x13, 0xba, 0x13, 0x96, 0x13, 0x74, 0x13,
  0x51, 0x13, 0x2f, 0x13,
  0x0d, 0x13, 0xec, 0x12, 0xcc, 0x12, 0xab, 0x12, 0x8c, 0x12, 0x6c, 0x12,
  0x4d, 0x12, 0x2f, 0x12,
  0x11, 0x12, 0xf3, 0x11, 0xd5, 0x11, 0xb8, 0x11, 0x9c, 0x11, 0x80, 0x11,
  0x64, 0x11, 0x48, 0x11,
  0x2d, 0x11, 0x12, 0x11, 0xf7, 0x10, 0xdd, 0x10, 0xc3, 0x10, 0xa9, 0x10,
  0x90, 0x10, 0x77, 0x10,
  0x5e, 0x10, 0x46, 0x10, 0x2e, 0x10, 0x16, 0x10, 0xfe, 0x0f, 0xe7, 0x0f,
  0xd0, 0x0f, 0xb9, 0x0f,
  0xa2, 0x0f, 0x8c, 0x0f, 0x76, 0x0f, 0x60, 0x0f, 0x4b, 0x0f, 0x35, 0x0f,
  0x20, 0x0f, 0x0b, 0x0f,
  0xf7, 0x0e, 0xe2, 0x0e, 0xce, 0x0e, 0xba, 0x0e, 0xa6, 0x0e, 0x92, 0x0e,
  0x7f, 0x0e, 0x6c, 0x0e,
  0x59, 0x0e, 0x46, 0x0e, 0x33, 0x0e, 0x21, 0x0e, 0x0f, 0x0e, 0xfd, 0x0d,
  0xeb, 0x0d, 0xd9, 0x0d,
  0xc8, 0x0d, 0xb6, 0x0d, 0xa5, 0x0d, 0x94, 0x0d, 0x83, 0x0d, 0x73, 0x0d,
  0x62, 0x0d, 0x52, 0x0d,
  0x41, 0x0d, 0x31, 0x0d, 0x22, 0x0d, 0x12, 0x0d, 0x02, 0x0d, 0xf3, 0x0c,
  0xe3, 0x0c, 0xd4, 0x0c,
  0xc5, 0x0c, 0xb6, 0x0c, 0xa7, 0x0c, 0x99, 0x0c, 0x8a, 0x0c, 0x7c, 0x0c,
  0x6e, 0x0c, 0x60, 0x0c,
  0x52, 0x0c, 0x44, 0x0c, 0x36, 0x0c, 0x28, 0x0c, 0x1b, 0x0c, 0x0d, 0x0c,
  0x00, 0x0c, 0xf3, 0x0b,
  0xe6, 0x0b, 0xd9, 0x0b, 0xcc, 0x0b, 0xbf, 0x0b, 0xb3, 0x0b, 0xa6, 0x0b,
  0x9a, 0x0b, 0x8e, 0x0b,
  0x81, 0x0b, 0x75, 0x0b, 0x69, 0x0b, 0x5d, 0x0b, 0x52, 0x0b, 0x46, 0x0b,
  0x3a, 0x0b, 0x2f, 0x0b,
  0x23, 0x0b, 0x18, 0x0b, 0x0d, 0x0b, 0x02, 0x0b, 0xf6, 0x0a, 0xeb, 0x0a,
  0xe1, 0x0a, 0xd6, 0x0a,
  0xcb, 0x0a, 0xc0, 0x0a, 0xb6, 0x0a, 0xab, 0x0a, 0xa1, 0x0a, 0x97, 0x0a,
  0x8c, 0x0a, 0x82, 0x0a,
  0x78, 0x0a, 0x6e, 0x0a, 0x64, 0x0a, 0x5a, 0x0a, 0x50, 0x0a, 0x47, 0x0a,
  0x3d, 0x0a, 0x33, 0x0a,
  0x2a, 0x0a, 0x20, 0x0a, 0x17, 0x0a, 0x0e, 0x0a, 0x04, 0x0a, 0xfb, 0x09,
  0xf2, 0x09, 0xe9, 0x09,
  0xe0, 0x09, 0xd7, 0x09, 0xce, 0x09, 0xc6, 0x09, 0xbd, 0x09, 0xb4, 0x09,
  0xab, 0x09, 0xa3, 0x09,
  0x9a, 0x09, 0x92, 0x09, 0x8a, 0x09, 0x81, 0x09, 0x79, 0x09, 0x71, 0x09,
  0x69, 0x09, 0x61, 0x09,
  0x59, 0x09, 0x51, 0x09, 0x49, 0x09, 0x41, 0x09, 0x39, 0x09, 0x31, 0x09,
  0x29, 0x09, 0x22, 0x09,
  0x1a, 0x09, 0x12, 0x09, 0x0b, 0x09, 0x03, 0x09, 0xfc, 0x08, 0xf5, 0x08,
  0xed, 0x08, 0xe6, 0x08,
  0xdf, 0x08, 0xd8, 0x08, 0xd0, 0x08, 0xc9, 0x08, 0xc2, 0x08, 0xbb, 0x08,
  0xb4, 0x08, 0xad, 0x08,
  0xa6, 0x08, 0xa0, 0x08
};

/* size=0x0018 */
static byte cp2155_slope10_back[] = {
  0x80, 0x25, 0xc0, 0x1c, 0x4f, 0x17, 0x9a, 0x13, 0xe9, 0x10, 0xde, 0x0e,
  0x44, 0x0d, 0xfa, 0x0b,
  0xea, 0x0a, 0x07, 0x0a, 0x46, 0x09, 0xa0, 0x08
};

static void
cp2155_block2 (int fd, unsigned int addr)
{
  DBG (1, "cp2155_block2 %06x\n", addr);
  cp2155_block1 (fd, 0x16, addr, cp2155_gamma_red_data, 0x0100);
}

static void
cp2155_block3 (int fd, unsigned int addr)
{
  DBG (1, "cp2155_block3 %06x\n", addr);
  cp2155_block1 (fd, 0x16, addr, cp2155_gamma_greenblue_data, 0x0100);
}

static void
cp2155_set_slope (int fd, unsigned int addr, byte * data, size_t size)
{
/*
  DBG (1, "cp2155_set_slope %06x %04lx\n", addr, (u_long) size);
*/
  cp2155_block1 (fd, 0x14, addr, data, size);
}

/* size=0x0075 */
static byte cp2155_set_regs_data6[] = {
  0x00, 0x00, 0x00, 0x69, 0x00, 0xe8, 0x1d, 0x00, 0x00, 0x70, 0x00, 0x00,
  0x00, 0x2e, 0x00, 0x04,
  0x04, 0xf8, 0x07, 0x32, 0x32, 0x32, 0x32, 0x00, 0x01, 0x00, 0x01, 0x00,
  0x01, 0x00, 0x01, 0x02,
  0x00, 0x03, 0x15, 0x15, 0x15, 0x15, 0x04, 0x07, 0x29, 0x29, 0x09, 0x09,
  0x02, 0x06, 0x12, 0x12,
  0x03, 0x05, 0x05, 0x03, 0x05, 0x41, 0x61, 0x21, 0x21, 0x25, 0x25, 0x25,
  0x40, 0x40, 0x40, 0x06,
  0x40, 0x06, 0x00, 0x36, 0xd0, 0x00, 0x00, 0x06, 0x00, 0x00, 0x02, 0x83,
  0x7c, 0x02, 0x1c, 0x9c,
  0x38, 0x28, 0x28, 0x27, 0x27, 0x25, 0x25, 0x21, 0x21, 0x1c, 0x1c, 0x16,
  0x16, 0x0f, 0x0f, 0x08,
  0x08, 0x00, 0x00, 0x08, 0x08, 0x0f, 0x0f, 0x16, 0x16, 0x1c, 0x1c, 0x21,
  0x21, 0x25, 0x25, 0x27,
  0x27, 0x02, 0x02, 0x22, 0x00
};

/* size=0x0075 */
static byte cp2155_set_regs_nr[] = {
  0x07, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0xa0, 0xa1, 0xa2, 0xa3, 0x64, 0x65,
  0x61, 0x62, 0x63, 0x50,
  0x50, 0x90, 0x51, 0x5a, 0x5b, 0x5c, 0x5d, 0x52, 0x53, 0x54, 0x55, 0x56,
  0x57, 0x58, 0x59, 0x5e,
  0x5f, 0x5f, 0x60, 0x60, 0x60, 0x60, 0x50, 0x51, 0x81, 0x81, 0x82, 0x82,
  0x83, 0x84, 0x80, 0x80,
  0xb0, 0x10, 0x10, 0x9b, 0x10, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
  0x12, 0x13, 0x16, 0x21,
  0x22, 0x20, 0x1d, 0x1e, 0x1f, 0x66, 0x67, 0x68, 0x1a, 0x1b, 0x1c, 0x15,
  0x14, 0x17, 0x43, 0x44,
  0x45, 0x23, 0x33, 0x24, 0x34, 0x25, 0x35, 0x26, 0x36, 0x27, 0x37, 0x28,
  0x38, 0x29, 0x39, 0x2a,
  0x3a, 0x2b, 0x3b, 0x2c, 0x3c, 0x2d, 0x3d, 0x2e, 0x3e, 0x2f, 0x3f, 0x30,
  0x40, 0x31, 0x41, 0x32,
  0x42, 0xca, 0xca, 0xca, 0x18
};

static void
cp2155_set_regs (int fd, byte * data)
{
  DBG (1, "cp2155_set_regs\n");
  int i;

  for (i = 0; i < 0x0075; i++)
    {
      if (cp2155_set_regs_nr[i] != 0x90)
	{
	  cp2155_set (fd, cp2155_set_regs_nr[i], data[i]);
	}
    }
}

static void
cp2155_block5 (int fd, byte v001)
{
  DBG (1, "cp2155_block5 %02x\n", v001);
  cp2155_set (fd, 0x90, 0xd8);
  cp2155_set (fd, 0x90, 0xd8);
  cp2155_set (fd, 0xb0, v001);
}

static void
cp2155_block6 (int fd, byte v001, byte v002)
{
  DBG (1, "cp2155_block6 %02x %02x\n", v001, v002);
  cp2155_set (fd, 0x80, v001);
  cp2155_set (fd, 0x11, v002);
}

static void
cp2155_block8 (int fd)
{
  DBG (1, "cp2155_block8\n");
  cp2155_set (fd, 0x04, 0x0c);
  cp2155_set (fd, 0x05, 0x00);
  cp2155_set (fd, 0x06, 0x00);
}

static void
cp2155_set_gamma (int fd)
{
  DBG (1, "cp2155_set_gamma\n");
/* gamma tables */
  cp2155_block3 (fd, 0x000000);
  cp2155_block3 (fd, 0x000100);
  cp2155_block3 (fd, 0x000200);
}

static void
cp2155_set_gamma600 (int fd)
{
  DBG (1, "cp2155_set_gamma\n");
/* gamma tables */
  cp2155_block2 (fd, 0x000000);
  cp2155_block3 (fd, 0x000100);
  cp2155_block3 (fd, 0x000200);
}

static void
cp2155_motor (int fd, byte v001, byte v002)
{
  DBG (1, "cp2155_motor %02x %02x\n", v001, v002);
  cp2155_set (fd, 0x10, v001);
  cp2155_set (fd, 0x11, v002);
  cp2155_set (fd, 0x60, 0x15);
  cp2155_set (fd, 0x80, 0x12);
  cp2155_set (fd, 0x03, 0x01);	/* starts motor */
}

void
make_buf (size_t count, unsigned char *buf)
{
  size_t i = 4;
  int hiword = 62756;
  int loword = 20918;
  unsigned char hihi = (hiword >> 8) & 0xff;
  unsigned char hilo = (hiword) & 0xff;
  unsigned char lohi = (loword >> 8) & 0xff;
  unsigned char lolo = (loword) & 0xff;
  buf[0] = 0x04;
  buf[1] = 0x70;
  buf[2] = (count - 4) & 0xff;
  buf[3] = ((count - 4) >> 8) & 0xff;
  while (i < count)
    {
      buf[i] = hilo;
      i++;
      buf[i] = hihi;
      i++;
      buf[i] = lolo;
      i++;
      buf[i] = lohi;
      i++;
    }
}

void
write_buf (int fd, size_t count, unsigned char *buf,
	   unsigned char value_74, unsigned char value_75)
{
  unsigned char value_72, value_73;
  value_72 = ((count - 4) >> 8) & 0xff;
  value_73 = (count - 4) & 0xff;
  cp2155_set (fd, 0x71, 0x01);
  cp2155_set (fd, 0x0230, 0x11);
  cp2155_set (fd, 0x71, 0x14);
  cp2155_set (fd, 0x72, value_72);
  cp2155_set (fd, 0x73, value_73);
  cp2155_set (fd, 0x74, value_74);
  cp2155_set (fd, 0x75, value_75);
  cp2155_set (fd, 0x76, 0x00);
  cp2155_set (fd, 0x0239, 0x40);
  cp2155_set (fd, 0x0238, 0x89);
  cp2155_set (fd, 0x023c, 0x2f);
  cp2155_set (fd, 0x0264, 0x20);
  sanei_usb_write_bulk (fd, buf, &count);
}

void
big_write (int fd, size_t count, unsigned char *buf)
{
  make_buf (count, buf);
  write_buf (fd, count, buf, 0x00, 0x00);
  write_buf (fd, count, buf, 0x00, 0xb0);
  write_buf (fd, count, buf, 0x01, 0x60);
}

void
general_motor (int fd)
{
  cp2155_set (fd, 0x9b, 0x02);
  cp2155_set (fd, 0x10, 0x05);
  cp2155_set (fd, 0x11, 0x91);
  cp2155_set (fd, 0x60, 0x15);
  cp2155_set (fd, 0x80, 0x12);
  cp2155_set (fd, 0x03, 0x01);
  cp2155_set (fd, 0x71, 0x01);
  cp2155_set (fd, 0x0230, 0x11);
  cp2155_set (fd, 0x71, 0x18);
  cp2155_set (fd, 0x72, 0x00);
  cp2155_set (fd, 0x73, 0x10);
  cp2155_set (fd, 0x0239, 0x40);
  cp2155_set (fd, 0x0238, 0x89);
  cp2155_set (fd, 0x023c, 0x2f);
  cp2155_set (fd, 0x0264, 0x20);
}

void
startblob0075 (CANON_Handle * chndl, unsigned char *buf)
{

  int fd;
  fd = chndl->fd;
  size_t count;

  cp2155_set (fd, 0x90, 0xd8);
  cp2155_set (fd, 0x90, 0xd8);
  cp2155_set (fd, 0xb0, 0x03);
  cp2155_set (fd, 0x07, 0x00);
  cp2155_set (fd, 0x07, 0x00);
  cp2155_set (fd, 0x08, chndl->value_08);
  cp2155_set (fd, 0x09, chndl->value_09);
  cp2155_set (fd, 0x0a, chndl->value_0a);
  cp2155_set (fd, 0x0b, chndl->value_0b);
  cp2155_set (fd, 0xa0, 0x1d);
  cp2155_set (fd, 0xa1, 0x00);
  cp2155_set (fd, 0xa2, 0x06);
  cp2155_set (fd, 0xa3, 0x70);
  cp2155_set (fd, 0x64, 0x00);
  cp2155_set (fd, 0x65, 0x00);
  cp2155_set (fd, 0x61, 0x00);
  cp2155_set (fd, 0x62, 0x2e);
  cp2155_set (fd, 0x63, 0x00);
  cp2155_set (fd, 0x50, 0x04);
  cp2155_set (fd, 0x50, 0x04);
  cp2155_set (fd, 0x51, chndl->value_51);
  cp2155_set (fd, 0x5a, 0x32);
  cp2155_set (fd, 0x5b, 0x32);
  cp2155_set (fd, 0x5c, 0x32);
  cp2155_set (fd, 0x5d, 0x32);
  cp2155_set (fd, 0x52, 0x09);
  cp2155_set (fd, 0x53, 0x5a);
  cp2155_set (fd, 0x54, 0x06);
  cp2155_set (fd, 0x55, 0x08);
  cp2155_set (fd, 0x56, 0x05);
  cp2155_set (fd, 0x57, 0x5f);
  cp2155_set (fd, 0x58, 0xa9);
  cp2155_set (fd, 0x59, 0xce);
  cp2155_set (fd, 0x5e, 0x02);
  cp2155_set (fd, 0x5f, 0x00);
  cp2155_set (fd, 0x5f, 0x03);
  cp2155_set (fd, 0x60, 0x15);
  cp2155_set (fd, 0x60, 0x15);
  cp2155_set (fd, 0x60, 0x15);
  cp2155_set (fd, 0x60, 0x15);
  cp2155_set (fd, 0x50, 0x04);
  cp2155_set (fd, 0x51, chndl->value_51);
  cp2155_set (fd, 0x81, 0x29);
  cp2155_set (fd, 0x81, 0x29);
  cp2155_set (fd, 0x82, 0x09);
  cp2155_set (fd, 0x82, 0x09);
  cp2155_set (fd, 0x83, 0x02);
  cp2155_set (fd, 0x84, 0x06);
  cp2155_set (fd, 0x80, 0x12);
  cp2155_set (fd, 0x80, 0x12);
  cp2155_set (fd, 0xb0, 0x0b);

  big_write (fd, 0x5174, buf);

  cp2155_set (fd, 0x10, 0x05);
  cp2155_set (fd, 0x10, 0x05);
  cp2155_set (fd, 0x9b, 0x03);
  cp2155_set (fd, 0x10, 0x05);
  cp2155_set (fd, 0x11, 0xc1);
  cp2155_set (fd, 0x11, 0xc1);
  cp2155_set (fd, 0x11, 0x81);
  cp2155_set (fd, 0x11, 0x81);
  cp2155_set (fd, 0x11, 0x81);
  cp2155_set (fd, 0x11, 0x81);
  cp2155_set (fd, 0x11, 0x81);
  cp2155_set (fd, 0x12, 0x40);
  cp2155_set (fd, 0x13, 0x40);
  cp2155_set (fd, 0x16, 0x40);
  cp2155_set (fd, 0x21, 0x06);
  cp2155_set (fd, 0x22, 0x40);
  cp2155_set (fd, 0x20, 0x06);
  cp2155_set (fd, 0x1d, 0x00);
  cp2155_set (fd, 0x1e, 0x00);
  cp2155_set (fd, 0x1f, 0xf0);
  cp2155_set (fd, 0x66, 0x00);
  cp2155_set (fd, 0x67, chndl->value_67);
  cp2155_set (fd, 0x68, chndl->value_68);
  cp2155_set (fd, 0x1a, 0x00);
  cp2155_set (fd, 0x1b, 0x00);
  cp2155_set (fd, 0x1c, 0x02);
  cp2155_set (fd, 0x15, 0x83);
  cp2155_set (fd, 0x14, 0x7c);
  cp2155_set (fd, 0x17, 0x02);
  cp2155_set (fd, 0x43, 0x1c);
  cp2155_set (fd, 0x44, 0x9c);
  cp2155_set (fd, 0x45, 0x38);
  cp2155_set (fd, 0x23, 0x28);
  cp2155_set (fd, 0x33, 0x28);
  cp2155_set (fd, 0x24, 0x27);
  cp2155_set (fd, 0x34, 0x27);
  cp2155_set (fd, 0x25, 0x25);
  cp2155_set (fd, 0x35, 0x25);
  cp2155_set (fd, 0x26, 0x21);
  cp2155_set (fd, 0x36, 0x21);
  cp2155_set (fd, 0x27, 0x1c);
  cp2155_set (fd, 0x37, 0x1c);
  cp2155_set (fd, 0x28, 0x16);
  cp2155_set (fd, 0x38, 0x16);
  cp2155_set (fd, 0x29, 0x0f);
  cp2155_set (fd, 0x39, 0x0f);
  cp2155_set (fd, 0x2a, 0x08);
  cp2155_set (fd, 0x3a, 0x08);
  cp2155_set (fd, 0x2b, 0x00);
  cp2155_set (fd, 0x3b, 0x00);
  cp2155_set (fd, 0x2c, 0x08);
  cp2155_set (fd, 0x3c, 0x08);
  cp2155_set (fd, 0x2d, 0x0f);
  cp2155_set (fd, 0x3d, 0x0f);
  cp2155_set (fd, 0x2e, 0x16);
  cp2155_set (fd, 0x3e, 0x16);
  cp2155_set (fd, 0x2f, 0x1c);
  cp2155_set (fd, 0x3f, 0x1c);
  cp2155_set (fd, 0x30, 0x21);
  cp2155_set (fd, 0x40, 0x21);
  cp2155_set (fd, 0x31, 0x25);
  cp2155_set (fd, 0x41, 0x25);
  cp2155_set (fd, 0x32, 0x27);
  cp2155_set (fd, 0x42, 0x27);
  cp2155_set (fd, 0xca, 0x01);
  cp2155_set (fd, 0xca, 0x01);
  cp2155_set (fd, 0xca, 0x11);
  cp2155_set (fd, 0x18, 0x00);

  memcpy (buf + 0x00000000,
	  "\x04\x70\x00\x01\x80\x25\x80\x25\x80\x25\x80\x25\x80\x25\x80\x25",
	  16);
  memcpy (buf + 0x00000010,
	  "\x80\x25\x80\x25\x80\x25\x80\x25\x80\x25\x80\x25\x80\x25\x80\x25",
	  16);
  memcpy (buf + 0x00000020,
	  "\x80\x25\x80\x25\x80\x25\x80\x25\x80\x25\x80\x25\x80\x25\x80\x25",
	  16);
  memcpy (buf + 0x00000030,
	  "\x80\x25\x80\x25\x80\x25\x80\x25\x80\x25\x80\x25\x80\x25\x80\x25",
	  16);
  memcpy (buf + 0x00000040,
	  "\x80\x25\x80\x25\x80\x25\x80\x25\x80\x25\x80\x25\x80\x25\x80\x25",
	  16);
  memcpy (buf + 0x00000050,
	  "\x80\x25\x80\x25\x80\x25\x80\x25\x80\x25\x80\x25\x80\x25\x80\x25",
	  16);
  memcpy (buf + 0x00000060,
	  "\x80\x25\x80\x25\x80\x25\x80\x25\x80\x25\xf0\x23\x80\x22\x2c\x21",
	  16);
  memcpy (buf + 0x00000070,
	  "\xf1\x1f\xcd\x1e\xbd\x1d\xc0\x1c\xd2\x1b\xf4\x1a\x22\x1a\x5e\x19",
	  16);
  memcpy (buf + 0x00000080,
	  "\xa4\x18\xf5\x17\x4f\x17\xb2\x16\x1d\x16\x90\x15\x09\x15\x89\x14",
	  16);
  memcpy (buf + 0x00000090,
	  "\x0e\x14\x9a\x13\x2a\x13\xc0\x12\x59\x12\xf8\x11\x9a\x11\x3f\x11",
	  16);
  memcpy (buf + 0x000000a0,
	  "\xe9\x10\x96\x10\x46\x10\xf8\x0f\xae\x0f\x66\x0f\x21\x0f\xde\x0e",
	  16);
  memcpy (buf + 0x000000b0,
	  "\x9e\x0e\x60\x0e\x23\x0e\xe9\x0d\xb0\x0d\x7a\x0d\x44\x0d\x11\x0d",
	  16);
  memcpy (buf + 0x000000c0,
	  "\xdf\x0c\xaf\x0c\x80\x0c\x52\x0c\x25\x0c\xfa\x0b\xd0\x0b\xa7\x0b",
	  16);
  memcpy (buf + 0x000000d0,
	  "\x80\x0b\x59\x0b\x33\x0b\x0e\x0b\xea\x0a\xc8\x0a\xa5\x0a\x84\x0a",
	  16);
  memcpy (buf + 0x000000e0,
	  "\x64\x0a\x44\x0a\x25\x0a\x07\x0a\xe9\x09\xcd\x09\xb0\x09\x95\x09",
	  16);
  memcpy (buf + 0x000000f0,
	  "\x7a\x09\x60\x09\x46\x09\x2c\x09\x14\x09\xfc\x08\xe4\x08\xcd\x08",
	  16);
  memcpy (buf + 0x00000100, "\xb6\x08\xa0\x08", 4);
  count = 260;
  write_buf (fd, count, buf, 0x03, 0x00);
  write_buf (fd, count, buf, 0x03, 0x02);
  write_buf (fd, count, buf, 0x03, 0x06);

  memcpy (buf + 0x00000000,
	  "\x04\x70\x18\x00\x80\x25\xc0\x1c\x4f\x17\x9a\x13\xe9\x10\xde\x0e",
	  16);
  memcpy (buf + 0x00000010,
	  "\x44\x0d\xfa\x0b\xea\x0a\x07\x0a\x46\x09\xa0\x08\x80\x25\x80\x25",
	  16);
  memcpy (buf + 0x00000020, "\x80\x25\x80\x25", 4);
  count = 36;
  write_buf (fd, count, buf, 0x03, 0x04);
  write_buf (fd, count, buf, 0x03, 0x08);

  general_motor (fd);
}

void
startblob0150 (CANON_Handle * chndl, unsigned char *buf)
{

  int fd;
  fd = chndl->fd;
  size_t count;

  cp2155_set (fd, 0x90, 0xd8);
  cp2155_set (fd, 0x90, 0xd8);
  cp2155_set (fd, 0xb0, 0x02);
  cp2155_set (fd, 0x07, 0x00);
  cp2155_set (fd, 0x07, 0x00);
  cp2155_set (fd, 0x08, chndl->value_08);
  cp2155_set (fd, 0x09, chndl->value_09);
  cp2155_set (fd, 0x0a, chndl->value_0a);
  cp2155_set (fd, 0x0b, chndl->value_0b);
  cp2155_set (fd, 0xa0, 0x1d);
  cp2155_set (fd, 0xa1, 0x00);
  cp2155_set (fd, 0xa2, 0x0c);
  cp2155_set (fd, 0xa3, 0xd0);
  cp2155_set (fd, 0x64, 0x00);
  cp2155_set (fd, 0x65, 0x00);
  cp2155_set (fd, 0x61, 0x00);
  cp2155_set (fd, 0x62, 0x1e);
  cp2155_set (fd, 0x63, 0xa0);
  cp2155_set (fd, 0x50, 0x04);
  cp2155_set (fd, 0x50, 0x04);
  cp2155_set (fd, 0x51, chndl->value_51);
  cp2155_set (fd, 0x5a, 0x32);
  cp2155_set (fd, 0x5b, 0x32);
  cp2155_set (fd, 0x5c, 0x32);
  cp2155_set (fd, 0x5d, 0x32);
  cp2155_set (fd, 0x52, 0x09);
  cp2155_set (fd, 0x53, 0x5a);
  cp2155_set (fd, 0x54, 0x06);
  cp2155_set (fd, 0x55, 0x08);
  cp2155_set (fd, 0x56, 0x05);
  cp2155_set (fd, 0x57, 0x5f);
  cp2155_set (fd, 0x58, 0xa9);
  cp2155_set (fd, 0x59, 0xce);
  cp2155_set (fd, 0x5e, 0x02);
  cp2155_set (fd, 0x5f, 0x00);
  cp2155_set (fd, 0x5f, 0x03);
  cp2155_set (fd, 0x60, 0x15);
  cp2155_set (fd, 0x60, 0x15);
  cp2155_set (fd, 0x60, 0x15);
  cp2155_set (fd, 0x60, 0x15);
  cp2155_set (fd, 0x50, 0x04);
  cp2155_set (fd, 0x51, chndl->value_51);
  cp2155_set (fd, 0x81, 0x29);
  cp2155_set (fd, 0x81, 0x29);
  cp2155_set (fd, 0x82, 0x09);
  cp2155_set (fd, 0x82, 0x09);
  cp2155_set (fd, 0x83, 0x02);
  cp2155_set (fd, 0x84, 0x06);
  cp2155_set (fd, 0x80, 0x12);
  cp2155_set (fd, 0x80, 0x12);
  cp2155_set (fd, 0xb0, 0x0a);

  big_write (fd, 0x5174, buf);

  cp2155_set (fd, 0x10, 0x05);
  cp2155_set (fd, 0x10, 0x05);
  cp2155_set (fd, 0x9b, 0x03);
  cp2155_set (fd, 0x10, 0x05);
  cp2155_set (fd, 0x11, 0x81);
  cp2155_set (fd, 0x11, 0x81);
  cp2155_set (fd, 0x11, 0x81);
  cp2155_set (fd, 0x11, 0x81);
  cp2155_set (fd, 0x11, 0x81);
  cp2155_set (fd, 0x11, 0x81);
  cp2155_set (fd, 0x11, 0x81);
  cp2155_set (fd, 0x12, 0x40);
  cp2155_set (fd, 0x13, 0x40);
  cp2155_set (fd, 0x16, 0x40);
  cp2155_set (fd, 0x21, 0x06);
  cp2155_set (fd, 0x22, 0x40);
  cp2155_set (fd, 0x20, 0x06);
  cp2155_set (fd, 0x1d, 0x00);
  cp2155_set (fd, 0x1e, 0x00);
  cp2155_set (fd, 0x1f, 0x04);
  cp2155_set (fd, 0x66, 0x00);
  cp2155_set (fd, 0x67, chndl->value_67);
  cp2155_set (fd, 0x68, chndl->value_68);
  cp2155_set (fd, 0x1a, 0x00);
  cp2155_set (fd, 0x1b, 0x00);
  cp2155_set (fd, 0x1c, 0x02);
  cp2155_set (fd, 0x15, 0x84);
  cp2155_set (fd, 0x14, 0x7c);
  cp2155_set (fd, 0x17, 0x02);
  cp2155_set (fd, 0x43, 0x1c);
  cp2155_set (fd, 0x44, 0x9c);
  cp2155_set (fd, 0x45, 0x38);
  cp2155_set (fd, 0x23, 0x28);
  cp2155_set (fd, 0x33, 0x28);
  cp2155_set (fd, 0x24, 0x27);
  cp2155_set (fd, 0x34, 0x27);
  cp2155_set (fd, 0x25, 0x25);
  cp2155_set (fd, 0x35, 0x25);
  cp2155_set (fd, 0x26, 0x21);
  cp2155_set (fd, 0x36, 0x21);
  cp2155_set (fd, 0x27, 0x1c);
  cp2155_set (fd, 0x37, 0x1c);
  cp2155_set (fd, 0x28, 0x16);
  cp2155_set (fd, 0x38, 0x16);
  cp2155_set (fd, 0x29, 0x0f);
  cp2155_set (fd, 0x39, 0x0f);
  cp2155_set (fd, 0x2a, 0x08);
  cp2155_set (fd, 0x3a, 0x08);
  cp2155_set (fd, 0x2b, 0x00);
  cp2155_set (fd, 0x3b, 0x00);
  cp2155_set (fd, 0x2c, 0x08);
  cp2155_set (fd, 0x3c, 0x08);
  cp2155_set (fd, 0x2d, 0x0f);
  cp2155_set (fd, 0x3d, 0x0f);
  cp2155_set (fd, 0x2e, 0x16);
  cp2155_set (fd, 0x3e, 0x16);
  cp2155_set (fd, 0x2f, 0x1c);
  cp2155_set (fd, 0x3f, 0x1c);
  cp2155_set (fd, 0x30, 0x21);
  cp2155_set (fd, 0x40, 0x21);
  cp2155_set (fd, 0x31, 0x25);
  cp2155_set (fd, 0x41, 0x25);
  cp2155_set (fd, 0x32, 0x27);
  cp2155_set (fd, 0x42, 0x27);
  cp2155_set (fd, 0xca, 0x01);
  cp2155_set (fd, 0xca, 0x01);
  cp2155_set (fd, 0xca, 0x11);
  cp2155_set (fd, 0x18, 0x00);
  cp2155_set (fd, 0x71, 0x01);
  cp2155_set (fd, 0x0230, 0x11);
  cp2155_set (fd, 0x71, 0x14);
  cp2155_set (fd, 0x72, 0x01);
  cp2155_set (fd, 0x73, 0x00);
  cp2155_set (fd, 0x74, 0x03);
  cp2155_set (fd, 0x75, 0x00);
  cp2155_set (fd, 0x76, 0x00);
  cp2155_set (fd, 0x0239, 0x40);
  cp2155_set (fd, 0x0238, 0x89);
  cp2155_set (fd, 0x023c, 0x2f);
  cp2155_set (fd, 0x0264, 0x20);
  memcpy (buf + 0x00000000,
	  "\x04\x70\x00\x01\x80\x25\xd7\x24\x35\x24\x98\x23\x00\x23\x6d\x22",
	  16);
  memcpy (buf + 0x00000010,
	  "\xdf\x21\x56\x21\xd1\x20\x50\x20\xd2\x1f\x59\x1f\xe3\x1e\x70\x1e",
	  16);
  memcpy (buf + 0x00000020,
	  "\x01\x1e\x95\x1d\x2c\x1d\xc6\x1c\x62\x1c\x02\x1c\xa3\x1b\x47\x1b",
	  16);
  memcpy (buf + 0x00000030,
	  "\xee\x1a\x97\x1a\x42\x1a\xef\x19\x9e\x19\x4f\x19\x02\x19\xb7\x18",
	  16);
  memcpy (buf + 0x00000040,
	  "\x6d\x18\x25\x18\xdf\x17\x9a\x17\x57\x17\x16\x17\xd6\x16\x97\x16",
	  16);
  memcpy (buf + 0x00000050,
	  "\x59\x16\x1d\x16\xe2\x15\xa8\x15\x70\x15\x38\x15\x02\x15\xcd\x14",
	  16);
  memcpy (buf + 0x00000060,
	  "\x99\x14\x66\x14\x33\x14\x02\x14\xd2\x13\xa2\x13\x74\x13\x46\x13",
	  16);
  memcpy (buf + 0x00000070,
	  "\x19\x13\xed\x12\xc2\x12\x98\x12\x6e\x12\x45\x12\x1d\x12\xf5\x11",
	  16);
  memcpy (buf + 0x00000080,
	  "\xce\x11\xa8\x11\x82\x11\x5d\x11\x39\x11\x15\x11\xf2\x10\xcf\x10",
	  16);
  memcpy (buf + 0x00000090,
	  "\xad\x10\x8b\x10\x6a\x10\x4a\x10\x2a\x10\x0a\x10\xeb\x0f\xcc\x0f",
	  16);
  memcpy (buf + 0x000000a0,
	  "\xae\x0f\x90\x0f\x73\x0f\x56\x0f\x3a\x0f\x1e\x0f\x02\x0f\xe7\x0e",
	  16);
  memcpy (buf + 0x000000b0,
	  "\xcc\x0e\xb2\x0e\x97\x0e\x7e\x0e\x64\x0e\x4b\x0e\x32\x0e\x1a\x0e",
	  16);
  memcpy (buf + 0x000000c0,
	  "\x02\x0e\xea\x0d\xd3\x0d\xbc\x0d\xa5\x0d\x8e\x0d\x78\x0d\x62\x0d",
	  16);
  memcpy (buf + 0x000000d0,
	  "\x4d\x0d\x37\x0d\x22\x0d\x0d\x0d\xf8\x0c\xe4\x0c\xd0\x0c\xbc\x0c",
	  16);
  memcpy (buf + 0x000000e0,
	  "\xa8\x0c\x95\x0c\x82\x0c\x6f\x0c\x5c\x0c\x4a\x0c\x37\x0c\x25\x0c",
	  16);
  memcpy (buf + 0x000000f0,
	  "\x14\x0c\x02\x0c\xf0\x0b\xdf\x0b\xce\x0b\xbd\x0b\xac\x0b\x9c\x0b",
	  16);
  memcpy (buf + 0x00000100, "\x8c\x0b\x7c\x0b", 4);
  count = 260;
  write_buf (fd, count, buf, 0x03, 0x00);
  write_buf (fd, count, buf, 0x03, 0x02);
  write_buf (fd, count, buf, 0x03, 0x06);

  memcpy (buf + 0x00000000,
	  "\x04\x70\x18\x00\x80\x25\x18\x1f\x8f\x1a\x2d\x17\x8f\x14\x79\x12",
	  16);
  memcpy (buf + 0x00000010,
	  "\xc6\x10\x5b\x0f\x2a\x0e\x24\x0d\x41\x0c\x7c\x0b\xe3\x1e\x70\x1e",
	  16);
  memcpy (buf + 0x00000020, "\x01\x1e\x95\x1d", 4);
  count = 36;
  write_buf (fd, count, buf, 0x03, 0x04);
  write_buf (fd, count, buf, 0x03, 0x08);

  general_motor (fd);
}

void
startblob0300 (CANON_Handle * chndl, unsigned char *buf)
{

  int fd;
  fd = chndl->fd;
  size_t count;

  cp2155_set (fd, 0x90, 0xd8);
  cp2155_set (fd, 0x90, 0xd8);
  cp2155_set (fd, 0xb0, 0x01);
  cp2155_set (fd, 0x07, 0x00);
  cp2155_set (fd, 0x07, 0x00);
  cp2155_set (fd, 0x08, chndl->value_08);
  cp2155_set (fd, 0x09, chndl->value_09);
  cp2155_set (fd, 0x0a, chndl->value_0a);
  cp2155_set (fd, 0x0b, chndl->value_0b);
  cp2155_set (fd, 0xa0, 0x1d);
  cp2155_set (fd, 0xa1, 0x00);
  cp2155_set (fd, 0xa2, 0x19);
  cp2155_set (fd, 0xa3, 0x30);
  cp2155_set (fd, 0x64, 0x00);
  cp2155_set (fd, 0x65, 0x00);
  cp2155_set (fd, 0x61, 0x00);
  cp2155_set (fd, 0x62, 0x2a);
  cp2155_set (fd, 0x63, 0x80);
  cp2155_set (fd, 0x50, 0x04);
  cp2155_set (fd, 0x50, 0x04);
  cp2155_set (fd, 0x51, chndl->value_51);
  cp2155_set (fd, 0x5a, 0x32);
  cp2155_set (fd, 0x5b, 0x32);
  cp2155_set (fd, 0x5c, 0x32);
  cp2155_set (fd, 0x5d, 0x32);
  cp2155_set (fd, 0x52, 0x09);
  cp2155_set (fd, 0x53, 0x5a);
  cp2155_set (fd, 0x54, 0x06);
  cp2155_set (fd, 0x55, 0x08);
  cp2155_set (fd, 0x56, 0x05);
  cp2155_set (fd, 0x57, 0x5f);
  cp2155_set (fd, 0x58, 0xa9);
  cp2155_set (fd, 0x59, 0xce);
  cp2155_set (fd, 0x5e, 0x02);
  cp2155_set (fd, 0x5f, 0x00);
  cp2155_set (fd, 0x5f, 0x03);
  cp2155_set (fd, 0x60, 0x15);
  cp2155_set (fd, 0x60, 0x15);
  cp2155_set (fd, 0x60, 0x15);
  cp2155_set (fd, 0x60, 0x15);
  cp2155_set (fd, 0x50, 0x04);
  cp2155_set (fd, 0x51, chndl->value_51);
  cp2155_set (fd, 0x81, 0x29);
  cp2155_set (fd, 0x81, 0x29);
  cp2155_set (fd, 0x82, 0x09);
  cp2155_set (fd, 0x82, 0x09);
  cp2155_set (fd, 0x83, 0x02);
  cp2155_set (fd, 0x84, 0x06);
  cp2155_set (fd, 0x80, 0x12);
  cp2155_set (fd, 0x80, 0x12);
  cp2155_set (fd, 0xb0, 0x09);

  big_write (fd, 0x5174, buf);

  cp2155_set (fd, 0x10, 0x05);
  cp2155_set (fd, 0x10, 0x05);
  cp2155_set (fd, 0x9b, 0x01);
  cp2155_set (fd, 0x10, 0x05);
  cp2155_set (fd, 0x11, 0x81);
  cp2155_set (fd, 0x11, 0x81);
  cp2155_set (fd, 0x11, 0x81);
  cp2155_set (fd, 0x11, 0x81);
  cp2155_set (fd, 0x11, 0x81);
  cp2155_set (fd, 0x11, 0x81);
  cp2155_set (fd, 0x11, 0x81);
  cp2155_set (fd, 0x12, 0x0c);
  cp2155_set (fd, 0x13, 0x0c);
  cp2155_set (fd, 0x16, 0x0c);
  cp2155_set (fd, 0x21, 0x06);
  cp2155_set (fd, 0x22, 0x0c);
  cp2155_set (fd, 0x20, 0x06);
  cp2155_set (fd, 0x1d, 0x00);
  cp2155_set (fd, 0x1e, 0x00);
  cp2155_set (fd, 0x1f, 0x04);
  cp2155_set (fd, 0x66, 0x00);
  cp2155_set (fd, 0x67, chndl->value_67);
  cp2155_set (fd, 0x68, chndl->value_68);
  cp2155_set (fd, 0x1a, 0x00);
  cp2155_set (fd, 0x1b, 0x00);
  cp2155_set (fd, 0x1c, 0x02);
  cp2155_set (fd, 0x15, 0x83);
  cp2155_set (fd, 0x14, 0x7c);
  cp2155_set (fd, 0x17, 0x02);
  cp2155_set (fd, 0x43, 0x1c);
  cp2155_set (fd, 0x44, 0x9c);
  cp2155_set (fd, 0x45, 0x38);
  cp2155_set (fd, 0x23, 0x14);
  cp2155_set (fd, 0x33, 0x14);
  cp2155_set (fd, 0x24, 0x14);
  cp2155_set (fd, 0x34, 0x14);
  cp2155_set (fd, 0x25, 0x14);
  cp2155_set (fd, 0x35, 0x14);
  cp2155_set (fd, 0x26, 0x14);
  cp2155_set (fd, 0x36, 0x14);
  cp2155_set (fd, 0x27, 0x14);
  cp2155_set (fd, 0x37, 0x14);
  cp2155_set (fd, 0x28, 0x14);
  cp2155_set (fd, 0x38, 0x14);
  cp2155_set (fd, 0x29, 0x14);
  cp2155_set (fd, 0x39, 0x14);
  cp2155_set (fd, 0x2a, 0x14);
  cp2155_set (fd, 0x3a, 0x14);
  cp2155_set (fd, 0x2b, 0x14);
  cp2155_set (fd, 0x3b, 0x14);
  cp2155_set (fd, 0x2c, 0x14);
  cp2155_set (fd, 0x3c, 0x14);
  cp2155_set (fd, 0x2d, 0x14);
  cp2155_set (fd, 0x3d, 0x14);
  cp2155_set (fd, 0x2e, 0x14);
  cp2155_set (fd, 0x3e, 0x14);
  cp2155_set (fd, 0x2f, 0x14);
  cp2155_set (fd, 0x3f, 0x14);
  cp2155_set (fd, 0x30, 0x14);
  cp2155_set (fd, 0x40, 0x14);
  cp2155_set (fd, 0x31, 0x14);
  cp2155_set (fd, 0x41, 0x14);
  cp2155_set (fd, 0x32, 0x14);
  cp2155_set (fd, 0x42, 0x14);
  cp2155_set (fd, 0xca, 0x00);
  cp2155_set (fd, 0xca, 0x00);
  cp2155_set (fd, 0xca, 0x00);
  cp2155_set (fd, 0x18, 0x00);

  memcpy (buf + 0x00000000,
	  "\x04\x70\x30\x00\x80\x25\x36\x25\xee\x24\xa8\x24\x62\x24\x1d\x24",
	  16);
  memcpy (buf + 0x00000010,
	  "\xd9\x23\x96\x23\x54\x23\x13\x23\xd3\x22\x94\x22\x56\x22\x19\x22",
	  16);
  memcpy (buf + 0x00000020,
	  "\xdc\x21\xa1\x21\x66\x21\x2c\x21\xf3\x20\xba\x20\x82\x20\x4b\x20",
	  16);
  memcpy (buf + 0x00000030, "\x15\x20\xe0\x1f", 4);
  count = 52;
  write_buf (fd, count, buf, 0x03, 0x00);
  write_buf (fd, count, buf, 0x03, 0x02);
  write_buf (fd, count, buf, 0x03, 0x06);

  memcpy (buf + 0x00000000,
	  "\x04\x70\x18\x00\x80\x25\xe8\x24\x55\x24\xc7\x23\x3d\x23\xb7\x22",
	  16);
  memcpy (buf + 0x00000010,
	  "\x35\x22\xb6\x21\x3c\x21\xc4\x20\x50\x20\xe0\x1f\x56\x22\x19\x22",
	  16);
  memcpy (buf + 0x00000020, "\xdc\x21\xa1\x21", 4);
  count = 36;
  write_buf (fd, count, buf, 0x03, 0x04);
  write_buf (fd, count, buf, 0x03, 0x08);

  general_motor (fd);
}

void
startblob0600 (CANON_Handle * chndl, unsigned char *buf)
{

  int fd;
  fd = chndl->fd;
  size_t count;

  cp2155_set (fd, 0x90, 0xd8);
  cp2155_set (fd, 0x90, 0xd8);
  cp2155_set (fd, 0xb0, 0x00);
  cp2155_set (fd, 0x07, 0x00);
  cp2155_set (fd, 0x07, 0x00);
  cp2155_set (fd, 0x08, chndl->value_08);
  cp2155_set (fd, 0x09, chndl->value_09);
  cp2155_set (fd, 0x0a, chndl->value_0a);
  cp2155_set (fd, 0x0b, chndl->value_0b);
  cp2155_set (fd, 0xa0, 0x1d);
  cp2155_set (fd, 0xa1, 0x00);
  cp2155_set (fd, 0xa2, 0x77);
  cp2155_set (fd, 0xa3, 0xb0);
  cp2155_set (fd, 0x64, 0x00);
  cp2155_set (fd, 0x65, 0x00);
  cp2155_set (fd, 0x61, 0x00);
  cp2155_set (fd, 0x62, 0x15);
  cp2155_set (fd, 0x63, 0xe0);
  cp2155_set (fd, 0x50, 0x04);
  cp2155_set (fd, 0x50, 0x04);
  cp2155_set (fd, 0x51, chndl->value_51);
  cp2155_set (fd, 0x5a, 0x32);
  cp2155_set (fd, 0x5b, 0x32);
  cp2155_set (fd, 0x5c, 0x32);
  cp2155_set (fd, 0x5d, 0x32);
  cp2155_set (fd, 0x52, 0x07);
  cp2155_set (fd, 0x53, 0xd0);
  cp2155_set (fd, 0x54, 0x07);
  cp2155_set (fd, 0x55, 0xd0);
  cp2155_set (fd, 0x56, 0x07);
  cp2155_set (fd, 0x57, 0xd0);
  cp2155_set (fd, 0x58, 0x00);
  cp2155_set (fd, 0x59, 0x01);
  cp2155_set (fd, 0x5e, 0x02);
  cp2155_set (fd, 0x5f, 0x00);
  cp2155_set (fd, 0x5f, 0x03);
  cp2155_set (fd, 0x60, 0x15);
  cp2155_set (fd, 0x60, 0x15);
  cp2155_set (fd, 0x60, 0x15);
  cp2155_set (fd, 0x60, 0x15);
  cp2155_set (fd, 0x50, 0x04);
  cp2155_set (fd, 0x51, chndl->value_51);
  cp2155_set (fd, 0x81, 0x29);
  cp2155_set (fd, 0x81, 0x29);
  cp2155_set (fd, 0x82, 0x09);
  cp2155_set (fd, 0x82, 0x09);
  cp2155_set (fd, 0x83, 0x02);
  cp2155_set (fd, 0x84, 0x06);
  cp2155_set (fd, 0x80, 0x12);
  cp2155_set (fd, 0x80, 0x12);
  cp2155_set (fd, 0xb0, 0x00);
  cp2155_set (fd, 0x10, 0x05);
  cp2155_set (fd, 0x10, 0x05);
  cp2155_set (fd, 0x9b, 0x01);
  cp2155_set (fd, 0x10, 0x05);
  cp2155_set (fd, 0x11, 0x83);
  cp2155_set (fd, 0x11, 0x83);
  cp2155_set (fd, 0x11, 0xc3);
  cp2155_set (fd, 0x11, 0xc3);
  cp2155_set (fd, 0x11, 0xc3);
  cp2155_set (fd, 0x11, 0xc1);
  cp2155_set (fd, 0x11, 0xc1);
  cp2155_set (fd, 0x12, 0x12);
  cp2155_set (fd, 0x13, 0x00);
  cp2155_set (fd, 0x16, 0x12);
  cp2155_set (fd, 0x21, 0x06);
  cp2155_set (fd, 0x22, 0x12);
  cp2155_set (fd, 0x20, 0x06);
  cp2155_set (fd, 0x1d, 0x00);
  cp2155_set (fd, 0x1e, 0x00);
  cp2155_set (fd, 0x1f, 0x04);
  cp2155_set (fd, 0x66, 0x00);
  cp2155_set (fd, 0x67, chndl->value_67);
  cp2155_set (fd, 0x68, chndl->value_68);
  cp2155_set (fd, 0x1a, 0x00);
  cp2155_set (fd, 0x1b, 0x00);
  cp2155_set (fd, 0x1c, 0x02);
  cp2155_set (fd, 0x15, 0x01);
  cp2155_set (fd, 0x14, 0x01);
  cp2155_set (fd, 0x17, 0x01);
  cp2155_set (fd, 0x43, 0x1c);
  cp2155_set (fd, 0x44, 0x9c);
  cp2155_set (fd, 0x45, 0x38);
  cp2155_set (fd, 0x23, 0x14);
  cp2155_set (fd, 0x33, 0x14);
  cp2155_set (fd, 0x24, 0x14);
  cp2155_set (fd, 0x34, 0x14);
  cp2155_set (fd, 0x25, 0x14);
  cp2155_set (fd, 0x35, 0x14);
  cp2155_set (fd, 0x26, 0x14);
  cp2155_set (fd, 0x36, 0x14);
  cp2155_set (fd, 0x27, 0x14);
  cp2155_set (fd, 0x37, 0x14);
  cp2155_set (fd, 0x28, 0x14);
  cp2155_set (fd, 0x38, 0x14);
  cp2155_set (fd, 0x29, 0x14);
  cp2155_set (fd, 0x39, 0x14);
  cp2155_set (fd, 0x2a, 0x14);
  cp2155_set (fd, 0x3a, 0x14);
  cp2155_set (fd, 0x2b, 0x14);
  cp2155_set (fd, 0x3b, 0x14);
  cp2155_set (fd, 0x2c, 0x14);
  cp2155_set (fd, 0x3c, 0x14);
  cp2155_set (fd, 0x2d, 0x14);
  cp2155_set (fd, 0x3d, 0x14);
  cp2155_set (fd, 0x2e, 0x14);
  cp2155_set (fd, 0x3e, 0x14);
  cp2155_set (fd, 0x2f, 0x14);
  cp2155_set (fd, 0x3f, 0x14);
  cp2155_set (fd, 0x30, 0x14);
  cp2155_set (fd, 0x40, 0x14);
  cp2155_set (fd, 0x31, 0x14);
  cp2155_set (fd, 0x41, 0x14);
  cp2155_set (fd, 0x32, 0x14);
  cp2155_set (fd, 0x42, 0x14);
  cp2155_set (fd, 0xca, 0x00);
  cp2155_set (fd, 0xca, 0x00);
  cp2155_set (fd, 0xca, 0x00);
  cp2155_set (fd, 0x18, 0x00);

  memcpy (buf + 0x0000,
	  "\x04\x70\x50\x00\x80\x25\x58\x25\x32\x25\x0b\x25\xe5\x24\xc0\x24",
	  16);
  memcpy (buf + 0x0010,
	  "\x9a\x24\x75\x24\x50\x24\x2b\x24\x07\x24\xe3\x23\xbf\x23\x9c\x23",
	  16);
  memcpy (buf + 0x0020,
	  "\x79\x23\x56\x23\x33\x23\x11\x23\xee\x22\xcd\x22\xab\x22\x8a\x22",
	  16);
  memcpy (buf + 0x0030,
	  "\x68\x22\x48\x22\x27\x22\x07\x22\xe6\x21\xc7\x21\xa7\x21\x87\x21",
	  16);
  memcpy (buf + 0x0040,
	  "\x68\x21\x49\x21\x2a\x21\x0c\x21\xee\x20\xd0\x20\x00\x00\x00\x00",
	  16);
  memcpy (buf + 0x0050, "\x00\x00\x00\x00", 4);
  count = 84;
  write_buf (fd, count, buf, 0x03, 0x00);
  write_buf (fd, count, buf, 0x03, 0x02);
  write_buf (fd, count, buf, 0x03, 0x06);

  memcpy (buf + 0x0000,
	  "\x04\x70\x20\x00\x80\x25\x04\x25\x8c\x24\x18\x24\xa5\x23\x36\x23",
	  16);
  memcpy (buf + 0x0010,
	  "\xca\x22\x60\x22\xf8\x21\x93\x21\x30\x21\xd0\x20\x00\x00\x00\x00",
	  16);
  memcpy (buf + 0x0020, "\x00\x00\x00\x00", 4);
  count = 36;
  write_buf (fd, count, buf, 0x03, 0x04);
  write_buf (fd, count, buf, 0x03, 0x08);

  general_motor (fd);
}

void
startblob1200 (CANON_Handle * chndl, unsigned char *buf)
{

  int fd;
  fd = chndl->fd;
  size_t count;

  cp2155_set (fd, 0x90, 0xc8);
  cp2155_set (fd, 0x90, 0xe8);
  cp2155_set (fd, 0xb0, 0x00);
  cp2155_set (fd, 0x07, 0x00);
  cp2155_set (fd, 0x07, 0x00);
  cp2155_set (fd, 0x08, chndl->value_08);
  cp2155_set (fd, 0x09, chndl->value_09);
  cp2155_set (fd, 0x0a, chndl->value_0a);
  cp2155_set (fd, 0x0b, chndl->value_0b);
  cp2155_set (fd, 0xa0, 0x1d);
  cp2155_set (fd, 0xa1, 0x00);
  cp2155_set (fd, 0xa2, 0x63);
  cp2155_set (fd, 0xa3, 0xd0);
  cp2155_set (fd, 0x64, 0x00);
  cp2155_set (fd, 0x65, 0x00);
  cp2155_set (fd, 0x61, 0x00);
  cp2155_set (fd, 0x62, 0xaa);
  cp2155_set (fd, 0x63, 0x00);
  cp2155_set (fd, 0x50, 0x04);
  cp2155_set (fd, 0x50, 0x04);
  cp2155_set (fd, 0x51, chndl->value_51);
  cp2155_set (fd, 0x5a, 0x32);
  cp2155_set (fd, 0x5b, 0x32);
  cp2155_set (fd, 0x5c, 0x32);
  cp2155_set (fd, 0x5d, 0x32);
  cp2155_set (fd, 0x52, 0x11);
  cp2155_set (fd, 0x53, 0x50);
  cp2155_set (fd, 0x54, 0x0c);
  cp2155_set (fd, 0x55, 0x01);
  cp2155_set (fd, 0x56, 0x0a);
  cp2155_set (fd, 0x57, 0xae);
  cp2155_set (fd, 0x58, 0xa9);
  cp2155_set (fd, 0x59, 0xce);
  cp2155_set (fd, 0x5e, 0x02);
  cp2155_set (fd, 0x5f, 0x00);
  cp2155_set (fd, 0x5f, 0x03);
  cp2155_set (fd, 0x60, 0x15);
  cp2155_set (fd, 0x60, 0x15);
  cp2155_set (fd, 0x60, 0x15);
  cp2155_set (fd, 0x60, 0x15);
  cp2155_set (fd, 0x50, 0x04);
  cp2155_set (fd, 0x51, chndl->value_51);
  cp2155_set (fd, 0x81, 0x29);
  cp2155_set (fd, 0x81, 0x29);
  cp2155_set (fd, 0x82, 0x09);
  cp2155_set (fd, 0x82, 0x09);
  cp2155_set (fd, 0x83, 0x02);
  cp2155_set (fd, 0x84, 0x06);
  cp2155_set (fd, 0x80, 0x12);
  cp2155_set (fd, 0x80, 0x12);
  cp2155_set (fd, 0xb0, 0x08);

  big_write (fd, 0xa1a4, buf);

  cp2155_set (fd, 0x10, 0x05);
  cp2155_set (fd, 0x10, 0x05);
  cp2155_set (fd, 0x9b, 0x01);
  cp2155_set (fd, 0x10, 0x05);
  cp2155_set (fd, 0x11, 0x81);
  cp2155_set (fd, 0x11, 0x81);
  cp2155_set (fd, 0x11, 0x81);
  cp2155_set (fd, 0x11, 0x81);
  cp2155_set (fd, 0x11, 0x81);
  cp2155_set (fd, 0x11, 0x81);
  cp2155_set (fd, 0x11, 0x81);
  cp2155_set (fd, 0x12, 0x06);
  cp2155_set (fd, 0x13, 0x06);
  cp2155_set (fd, 0x16, 0x06);
  cp2155_set (fd, 0x21, 0x06);
  cp2155_set (fd, 0x22, 0x06);
  cp2155_set (fd, 0x20, 0x06);
  cp2155_set (fd, 0x1d, 0x00);
  cp2155_set (fd, 0x1e, 0x00);
  cp2155_set (fd, 0x1f, 0x04);
  cp2155_set (fd, 0x66, 0x00);
  cp2155_set (fd, 0x67, chndl->value_67);
  cp2155_set (fd, 0x68, chndl->value_68);
  cp2155_set (fd, 0x1a, 0x00);
  cp2155_set (fd, 0x1b, 0x00);
  cp2155_set (fd, 0x1c, 0x02);
  cp2155_set (fd, 0x15, 0x80);
  cp2155_set (fd, 0x14, 0x7c);
  cp2155_set (fd, 0x17, 0x01);
  cp2155_set (fd, 0x43, 0x1c);
  cp2155_set (fd, 0x44, 0x9c);
  cp2155_set (fd, 0x45, 0x38);
  cp2155_set (fd, 0x23, 0x14);
  cp2155_set (fd, 0x33, 0x14);
  cp2155_set (fd, 0x24, 0x14);
  cp2155_set (fd, 0x34, 0x14);
  cp2155_set (fd, 0x25, 0x12);
  cp2155_set (fd, 0x35, 0x12);
  cp2155_set (fd, 0x26, 0x11);
  cp2155_set (fd, 0x36, 0x11);
  cp2155_set (fd, 0x27, 0x0e);
  cp2155_set (fd, 0x37, 0x0e);
  cp2155_set (fd, 0x28, 0x0b);
  cp2155_set (fd, 0x38, 0x0b);
  cp2155_set (fd, 0x29, 0x08);
  cp2155_set (fd, 0x39, 0x08);
  cp2155_set (fd, 0x2a, 0x04);
  cp2155_set (fd, 0x3a, 0x04);
  cp2155_set (fd, 0x2b, 0x00);
  cp2155_set (fd, 0x3b, 0x00);
  cp2155_set (fd, 0x2c, 0x04);
  cp2155_set (fd, 0x3c, 0x04);
  cp2155_set (fd, 0x2d, 0x08);
  cp2155_set (fd, 0x3d, 0x08);
  cp2155_set (fd, 0x2e, 0x0b);
  cp2155_set (fd, 0x3e, 0x0b);
  cp2155_set (fd, 0x2f, 0x0e);
  cp2155_set (fd, 0x3f, 0x0e);
  cp2155_set (fd, 0x30, 0x11);
  cp2155_set (fd, 0x40, 0x11);
  cp2155_set (fd, 0x31, 0x12);
  cp2155_set (fd, 0x41, 0x12);
  cp2155_set (fd, 0x32, 0x14);
  cp2155_set (fd, 0x42, 0x14);
  cp2155_set (fd, 0xca, 0x00);
  cp2155_set (fd, 0xca, 0x00);
  cp2155_set (fd, 0xca, 0x00);
  cp2155_set (fd, 0x18, 0x01);

  memcpy (buf + 0x00000000,
	  "\x04\x70\x18\x00\x00\xff\x00\xff\x00\xff\x00\xff\x00\xff\x00\xff",
	  16);
  memcpy (buf + 0x00000010,
	  "\x00\xff\x00\xff\x00\xff\x00\xff\x00\xff\x00\xff\x00\x00\x00\x00",
	  16);
  memcpy (buf + 0x00000020, "\x00\x00\x00\x00", 4);
  count = 36;
  write_buf (fd, count, buf, 0x03, 0x00);
  write_buf (fd, count, buf, 0x03, 0x02);
  write_buf (fd, count, buf, 0x03, 0x06);
  write_buf (fd, count, buf, 0x03, 0x04);
  write_buf (fd, count, buf, 0x03, 0x08);

  general_motor (fd);
}

void
send_start_blob (CANON_Handle * chndl)
{
  unsigned char buf[0xf000];

  int fd;
  fd = chndl->fd;

/* value_51: lamp colors
   bit 0 set: red on, bit 1 set: green on, bit 2 set: blue on
   all bits off: no scan is made
*/
  chndl->value_51 = 0x07;

  switch (chndl->val[opt_resolution].w)
    {
    case 75:
      chndl->value_67 = 0x0a;	/* 3*7300/8 */
      chndl->value_68 = 0xb1;
      break;
    case 150:
      chndl->value_67 = 0x15;	/* 3*7300/4 */
      chndl->value_68 = 0x63;
      break;
    case 300:
      chndl->value_67 = 0x2a;	/* 3*7300/2 */
      chndl->value_68 = 0xc6;
      break;
    case 600:
      chndl->value_67 = 0x55;	/* 3*7300 */
      chndl->value_68 = 0x8c;
      break;
    case 1200:
      chndl->value_67 = 0xab;	/* 6*7300 */
      chndl->value_68 = 0x18;
    }

  cp2155_block6 (fd, 0x12, 0x83);
  cp2155_set (fd, 0x90, 0xf8);
  cp2155_block6 (fd, 0x12, 0x83);
/* start preparing real scan */
  cp2155_set (fd, 0x01, 0x29);
  cp2155_block8 (fd);
  cp2155_set (fd, 0x01, 0x29);
  cp2155_set_gamma (fd);

  switch (chndl->val[opt_resolution].w)
    {
    case 75:
      startblob0075 (chndl, buf);
      break;
    case 150:
      startblob0150 (chndl, buf);
      break;
    case 300:
      startblob0300 (chndl, buf);
      break;
    case 600:
      cp2155_set_gamma600 (fd);
      startblob0600 (chndl, buf);
      break;
    case 1200:
      startblob1200 (chndl, buf);
    }
}

/* Wait until data ready */
static long
wait_for_data (CANON_Handle * chndl)
{
  int fd;
  fd = chndl->fd;
  time_t start_time = time (NULL);
  long size;
  byte value;

  DBG (12, "waiting...\n");

  while (1)
    {
      size = 0;
      cp2155_get (fd, 0x46, &value);
      DBG (1, "home sensor: %02x\n", value);
      if (value == 0)
	{
	  send_start_blob (chndl);
	  cp2155_get (fd, 0x46, &value);
	  DBG (1, "home sensor: %02x\n", value);
	}

      if (cp2155_get (fd, 0xa5, &value) != SANE_STATUS_GOOD)
	{
	  return -1;
	}

      size += value;

      if (cp2155_get (fd, 0xa6, &value) != SANE_STATUS_GOOD)
	{
	  return -1;
	}

      size <<= 8;
      size += value;

      if (cp2155_get (fd, 0xa7, &value) != SANE_STATUS_GOOD)
	{
	  return -1;
	}

      size <<= 8;
      size += value;

      if (size != 0)
	{
	  return 2 * size;
	}

      /* Give it 5 seconds */
      if ((time (NULL) - start_time) > 5)
	{
	  DBG (1, "wait_for_data: timed out (%ld)\n", size);
	  return -1;
	}

      usleep (1 * MSEC);
    }
}

static void
go_home_without_wait (int fd)
{
  byte value;
  cp2155_get (fd, 0x46, &value);
  if (value == 0x08)
    {
      return;
    }
  cp2155_block6 (fd, 0x12, 0xc1);
  cp2155_set (fd, 0x01, 0x29);
  cp2155_block8 (fd);
  cp2155_set (fd, 0x01, 0x29);
  cp2155_set_gamma (fd);
  cp2155_block5 (fd, 0x03);
  cp2155_set_regs (fd, cp2155_set_regs_data6);
  cp2155_set_slope (fd, 0x030000, cp2155_slope09_back, 0x01f4);
  cp2155_set_slope (fd, 0x030200, cp2155_slope09_back, 0x01f4);
  cp2155_set_slope (fd, 0x030400, cp2155_slope10_back, 0x0018);
  cp2155_set_slope (fd, 0x030600, cp2155_slope09_back, 0x01f4);
  cp2155_set_slope (fd, 0x030800, cp2155_slope10_back, 0x0018);

  cp2155_motor (fd, 0x05, 0x35);
}


static int
go_home (int fd)
{
  byte value;
  cp2155_get (fd, 0x46, &value);
  if (value == 0x08)
    {
      return 0;
    }

  go_home_without_wait (fd);

  while (1)
    {
      usleep (200 * MSEC);
      cp2155_get (fd, 0x46, &value);
      DBG (1, "home sensor: %02x\n", value);

      if (value == 0x08)
	{
	  break;
	}
    }
  return 0;
}


/* Scanner init, called at calibration and scan time.
   Returns:
    1 if this was the first time the scanner was plugged in,
    0 afterward, and
   -1 on error. */

static int
init (CANON_Handle * chndl)
{
  int fd = chndl->fd;
  byte value;
  int result = 0;

  cp2155_get (fd, 0xd0, &value);
  /* Detect if scanner is plugged in */
  if (value != 0x81 && value != 0x40)
    {
      DBG (0, "INIT: unexpected value: %x\n", value);
    }

  if (value == 0x00)
    {
      return -1;
    }

  cp2155_set (fd, 0x02, 0x01);
  cp2155_set (fd, 0x02, 0x00);
  cp2155_set (fd, 0x01, 0x00);
  cp2155_set (fd, 0x01, 0x28);
  cp2155_set (fd, 0x90, 0x4f);
  cp2155_set (fd, 0x92, 0xff);
  cp2155_set (fd, 0x93, 0x00);
  cp2155_set (fd, 0x91, 0x1f);
  cp2155_set (fd, 0x95, 0x1f);
  cp2155_set (fd, 0x97, 0x1f);
  cp2155_set (fd, 0x9b, 0x00);
  cp2155_set (fd, 0x9c, 0x07);
  cp2155_set (fd, 0x90, 0x4d);
  cp2155_set (fd, 0x90, 0xcd);
  cp2155_set (fd, 0x90, 0xcc);
  cp2155_set (fd, 0x9b, 0x01);
  cp2155_set (fd, 0xa0, 0x04);
  cp2155_set (fd, 0xa0, 0x05);
  cp2155_set (fd, 0x01, 0x28);
  cp2155_set (fd, 0x04, 0x0c);
  cp2155_set (fd, 0x05, 0x00);
  cp2155_set (fd, 0x06, 0x00);
  cp2155_set (fd, 0x98, 0x00);
  cp2155_set (fd, 0x98, 0x00);
  cp2155_set (fd, 0x98, 0x02);
  cp2155_set (fd, 0x99, 0x28);
  cp2155_set (fd, 0x9a, 0x03);
  cp2155_set (fd, 0x80, 0x10);
  cp2155_set (fd, 0x8d, 0x00);
  cp2155_set (fd, 0x8d, 0x04);

  cp2155_set (fd, 0x85, 0x00);
  cp2155_set (fd, 0x87, 0x00);
  cp2155_set (fd, 0x88, 0x70);

  cp2155_set (fd, 0x85, 0x03);
  cp2155_set (fd, 0x87, 0x00);
  cp2155_set (fd, 0x88, 0x28);

  cp2155_set (fd, 0x85, 0x06);
  cp2155_set (fd, 0x87, 0x00);
  cp2155_set (fd, 0x88, 0x28);


  DBG (1, "INIT state: %0d\n", result);
  return result;
}

/* Scan and save the resulting image as r,g,b non-interleaved PPM file */
static SANE_Status
preread (CANON_Handle * chndl, SANE_Byte * data, FILE * fp)
{
  SANE_Status status = SANE_STATUS_GOOD;

  static byte linebuf[0x40000];
  byte readbuf[0xf000];
  int fd = chndl->fd;
  long width = chndl->params.pixels_per_line;
  /* set width to next multiple of 0x10 */
  while ((width % 0x10) != 0xf)
    {
      width++;
    }

  width++;

  byte *srcptr = readbuf;
  static byte *dstptr = linebuf;
  byte *endptr = linebuf + 3 * width;	/* Red line + Green line + Blue line */
  long datasize = 0;
  static long line = 0;
  size_t offset = 0;
  size_t bytes_written;
  static byte slot = 0;

  /* Data coming back is "width" bytes Red data, width bytes Green,
     width bytes Blue, repeat for "height" lines. */
/*  while (line < height)  process one buffer from the scanner */
  long startline = line;

  if (line >= (chndl->y1) * chndl->val[opt_resolution].w / 600
      + chndl->params.lines)
    {
      status = SANE_STATUS_EOF;
      init (chndl);
      line = 0;
      slot = 0;
      dstptr = linebuf;
      return status;
    }
  datasize = wait_for_data (chndl);

  if (datasize < 0)
    {
      DBG (1, "no data\n");
      status = SANE_STATUS_EOF;
      return status;
    }

  if (datasize > 0xf000)
    {
      datasize = 0xf000;
    }

  DBG (12, "scan line %ld %ld\n", line, datasize);

  cp2155_set (fd, 0x72, (datasize >> 8) & 0xff);
  cp2155_set (fd, 0x73, (datasize) & 0xff);

  status = cp2155_read (fd, readbuf, datasize);

  if (status != SANE_STATUS_GOOD)
    {
      status = SANE_STATUS_INVAL;
      return status;
    }

  /* Contorsions to convert data from line-by-line RGB to byte-by-byte RGB,
     without reading in the whole buffer first.  One image line is
     constructed in buffer linebuf and written to temp file if complete. */
  int idx = 0;
  srcptr = readbuf;

  while (idx < datasize)
    {
      *dstptr = (byte) * srcptr;
      idx++;
      srcptr += 1;
      dstptr += 3;

      if (dstptr >= endptr)	/* line of one color complete */
	{
	  slot++;		/* next color for this line */
	  dstptr = linebuf + slot;	/* restart shortly after beginning */
	  if (slot == 3)	/* all colors done */
	    {
	      slot = 0;		/* back to first color */
	      dstptr = linebuf;	/* back to beginning of line */
	      line++;		/* number of line just completed */
	      /* use scanner->width instead of width to remove pad bytes */
	      if (line > (chndl->y1) * chndl->val[opt_resolution].w / 600)
		{
		  if (chndl->params.format == SANE_FRAME_RGB)
		    {
		      memcpy (data + offset, linebuf, 3 * chndl->width);
		      offset += 3 * chndl->width;
		    }
		  else
		    {
		      int grayvalue;
		      int lineelement = 0;
		      while (lineelement < chndl->width)
			{
			  grayvalue = linebuf[3 * lineelement] +
			    linebuf[3 * lineelement + 1] +
			    linebuf[3 * lineelement + 2];
			  grayvalue /= 3;
			  if (chndl->params.depth == 8)	/* gray */
			    {
			      data[offset + lineelement] = (byte) grayvalue;
			    }
			  else	/* lineart */
			    {
			      if (lineelement % 8 == 0)
				{
				  data[offset + (lineelement >> 3)] = 0;
				}
			      if ((byte) grayvalue <
				  chndl->absolute_threshold)
				{
				  data[offset + (lineelement >> 3)] |=
				    (1 << (7 - lineelement % 8));
				}
			    }
			  lineelement++;
			}
		      offset += chndl->params.bytes_per_line;
		    }
		  DBG (6, "line %ld written...\n", line);
		}

	      if (line == (chndl->y1) * chndl->val[opt_resolution].w / 600
		  + chndl->params.lines)
		{
		  break;
		}

	    }
	}
    }				/* one readbuf processed */
  bytes_written = fwrite (data, 1, offset, fp);
  DBG (6, "%ld bytes written\n", bytes_written);
  if (bytes_written != offset)
    {
      status = SANE_STATUS_IO_ERROR;
    }
  DBG (6, "%ld lines from readbuf\n", line - startline);
  return status;		/*  to escape from this loop
				   after processing only one data buffer */
}

/* Scan and save the resulting image as r,g,b non-interleaved PPM file */
static SANE_Status
do_scan (CANON_Handle * chndl)
{
  SANE_Status status = SANE_STATUS_GOOD;
  SANE_Byte outbuf[0x40000];
  FILE *fp;
  fp = fopen (chndl->fname, "w");
  if (!fp)
    {
      DBG (1, "err:%s when opening %s\n", strerror (errno), chndl->fname);
      return SANE_STATUS_IO_ERROR;
    }
  int fd = chndl->fd;
  long width = chndl->params.pixels_per_line;
  if (chndl->val[opt_resolution].w < 600)
    {
      width = width * 600 / chndl->val[opt_resolution].w;
    }
  /* set width to next multiple of 0x10 */
  while ((width % 0x10) != 0xf)
    {
      width++;
    }

  long x_start;
  long x_end;
  long left_edge = 0x69;
  switch (chndl->val[opt_resolution].w)
    {
    case 75:
    case 150:
    case 300:
    case 600:
      left_edge = 0x69;
      break;
    case 1200:
      left_edge = 0x87;
    }
  x_start = left_edge + chndl->x1 * chndl->val[opt_resolution].w / 600;
  if (chndl->val[opt_resolution].w < 600)
    {
      x_start = left_edge + chndl->x1;
    }
  x_end = x_start + (width);
  width++;

  chndl->value_08 = (x_start >> 8) & 0xff;
  chndl->value_09 = (x_start) & 0xff;
  chndl->value_0a = (x_end >> 8) & 0xff;
  chndl->value_0b = (x_end) & 0xff;

  DBG (3, "val_08: %02x\n", chndl->value_08);
  DBG (3, "val_09: %02x\n", chndl->value_09);
  DBG (3, "val_0a: %02x\n", chndl->value_0a);
  DBG (3, "val_0b: %02x\n", chndl->value_0b);
  DBG (3, "chndl->width: %04lx\n", chndl->width);

  send_start_blob (chndl);

  while (status == SANE_STATUS_GOOD)
    {
      status = preread (chndl, outbuf, fp);
    }
  go_home_without_wait (fd);

  if (status == SANE_STATUS_EOF)
    {
      status = SANE_STATUS_GOOD;
    }

  fclose (fp);
  DBG (6, "created scan file %s\n", chndl->fname);

  return status;
}

/* Scan sequence */
/* resolution is 75,150,300,600,1200
   scan coordinates in 600-dpi pixels */

static SANE_Status
scan (CANON_Handle * chndl)
{
  SANE_Status status = SANE_STATUS_GOOD;
  /* Resolution: dpi 75, 150, 300, 600, 1200 */
  switch (chndl->val[opt_resolution].w)
    {
    case 75:
    case 150:
    case 300:
    case 600:
    case 1200:
      break;
    default:
      chndl->val[opt_resolution].w = 600;
    }

  chndl->width = chndl->params.pixels_per_line;
  chndl->height =
    (chndl->y2 - chndl->y1) * chndl->val[opt_resolution].w / 600;
  DBG (1, "dpi=%d\n", chndl->val[opt_resolution].w);
  DBG (1, "x1=%d y1=%d\n", chndl->x1, chndl->y1);
  DBG (1, "x2=%d y2=%d\n", chndl->x2, chndl->y2);
  DBG (1, "width=%ld height=%ld\n", chndl->width, chndl->height);

  CHK (do_scan (chndl));
  return status;
}


static SANE_Status
CANON_set_scan_parameters (CANON_Handle * chndl)
{
  int left;
  int top;
  int right;
  int bottom;

  double leftf;
  double rightf;
  double topf;
  double bottomf;

  double widthf;
  double heightf;
  int widthi;
  int heighti;

  int top_edge = 7;
  if (chndl->val[opt_resolution].w < 300)
    {
      top_edge = 0;
    }

  left = SANE_UNFIX (chndl->val[opt_tl_x].w) / MM_IN_INCH * 600;
  top = (top_edge + SANE_UNFIX (chndl->val[opt_tl_y].w)) / MM_IN_INCH * 600;
  right = SANE_UNFIX (chndl->val[opt_br_x].w) / MM_IN_INCH * 600;
  bottom =
    (top_edge + SANE_UNFIX (chndl->val[opt_br_y].w)) / MM_IN_INCH * 600;

  leftf = SANE_UNFIX (chndl->val[opt_tl_x].w);
  rightf = SANE_UNFIX (chndl->val[opt_br_x].w);
  topf = SANE_UNFIX (chndl->val[opt_tl_y].w);
  bottomf = SANE_UNFIX (chndl->val[opt_br_y].w);

  widthf = (rightf - leftf) / MM_PER_INCH * 600;
  widthi = (int) widthf;
  heightf = (bottomf - topf) / MM_PER_INCH * 600;
  heighti = (int) heightf;

  DBG (2, "CANON_set_scan_parameters:\n");
  DBG (2, "widthf = %f\n", widthf);
  DBG (2, "widthi = %d\n", widthi);
  DBG (2, "in 600dpi pixels:\n");
  DBG (2, "left  = %d, top    = %d\n", left, top);
  DBG (2, "right = %d, bottom = %d\n", right, bottom);

  /* Validate the input parameters */
  if ((left < 0) || (right > CANON_MAX_WIDTH))
    {
      return SANE_STATUS_INVAL;
    }

  if ((top < 0) || (bottom > CANON_MAX_HEIGHT))
    {
      return SANE_STATUS_INVAL;
    }

  if (((right - left) < 10) || ((bottom - top) < 10))
    {
      return SANE_STATUS_INVAL;
    }

  if ((chndl->val[opt_resolution].w != 75) &&
      (chndl->val[opt_resolution].w != 150) &&
      (chndl->val[opt_resolution].w != 300) &&
      (chndl->val[opt_resolution].w != 600) &&
      (chndl->val[opt_resolution].w != 1200))
    {
      return SANE_STATUS_INVAL;
    }

  /* Store params */
  chndl->x1 = left;
  chndl->x2 = left + widthi;
  chndl->y1 = top;
  chndl->y2 = top + heighti;
  chndl->absolute_threshold = (chndl->val[opt_threshold].w * 255) / 100;
  return SANE_STATUS_GOOD;
}


static SANE_Status
CANON_close_device (CANON_Handle * scan)
{
  DBG (3, "CANON_close_device:\n");
  sanei_usb_close (scan->fd);
  return SANE_STATUS_GOOD;
}


static SANE_Status
CANON_open_device (CANON_Handle * scan, const char *dev)
{
  SANE_Word vendor;
  SANE_Word product;
  SANE_Status res;

  DBG (3, "CANON_open_device: `%s'\n", dev);

  scan->fname = NULL;
  scan->fp = NULL;

  res = sanei_usb_open (dev, &scan->fd);

  if (res != SANE_STATUS_GOOD)
    {
      DBG (1, "CANON_open_device: couldn't open device `%s': %s\n", dev,
	   sane_strstatus (res));
      return res;
    }

  scan->product = "unknown";

#ifndef NO_AUTODETECT
  /* We have opened the device. Check that it is a USB scanner. */
  if (sanei_usb_get_vendor_product (scan->fd, &vendor, &product) !=
      SANE_STATUS_GOOD)
    {
      DBG (1, "CANON_open_device: sanei_usb_get_vendor_product failed\n");
      /* This is not a USB scanner, or SANE or the OS doesn't support it. */
      sanei_usb_close (scan->fd);
      scan->fd = -1;
      return SANE_STATUS_UNSUPPORTED;
    }

  /* Make sure we have a CANON scanner */
  if (vendor == 0x04a9)
    {
      scan->product = "Canon";
      scan->productcode = product;
      if (product == 0x2224)
	{
	  scan->product = "CanoScan LiDE 600F";
	}
      else if (product == 0x2225)
	{
	  scan->product = "CanoScan LiDE 70";
	}
      else
	{
	  DBG (1, "CANON_open_device: incorrect vendor/product (0x%x/0x%x)\n",
	       vendor, product);
	  sanei_usb_close (scan->fd);
	  scan->fd = -1;
	  return SANE_STATUS_UNSUPPORTED;
	}
    }
#endif

  return SANE_STATUS_GOOD;
}


static const char *
CANON_get_device_name (CANON_Handle * chndl)
{
  return chndl->product;
}


static SANE_Status
CANON_finish_scan (CANON_Handle * chndl)
{
  DBG (3, "CANON_finish_scan:\n");

  if (chndl->fp)
    {
      fclose (chndl->fp);
    }

  chndl->fp = NULL;

  /* remove temp file */
  if (chndl->fname)
    {
      DBG (4, "removing temp file %s\n", chndl->fname);
      unlink (chndl->fname);
      free (chndl->fname);
    }

  chndl->fname = NULL;
  return SANE_STATUS_GOOD;
}


static SANE_Status
CANON_start_scan (CANON_Handle * chndl)
{
  SANE_Status status;
  int result;
  int fd;
  DBG (3, "CANON_start_scan called\n");

  /* choose a temp file name for scan data */
  chndl->fname = strdup ("/tmp/scan.XXXXXX");
  fd = mkstemp (chndl->fname);

  if (!fd)
    {
      return SANE_STATUS_IO_ERROR;
    }

  close (fd);

  /* check if calibration needed */
  result = init (chndl);

  if (result < 0)
    {
      DBG (1, "Can't talk on USB.\n");
      return SANE_STATUS_IO_ERROR;
    }

  go_home (chndl->fd);

  /* scan */
  if ((status = scan (chndl)) != SANE_STATUS_GOOD)
    {
      CANON_finish_scan (chndl);
      return status;
    }

  /* read the temp file back out */
  chndl->fp = fopen (chndl->fname, "r");
  DBG (4, "reading %s\n", chndl->fname);

  if (!chndl->fp)
    {
      DBG (1, "open %s", chndl->fname);
      return SANE_STATUS_IO_ERROR;
    }

  return SANE_STATUS_GOOD;
}


static SANE_Status
CANON_read (CANON_Handle * chndl, SANE_Byte * data,
	    SANE_Int max_length, SANE_Int * length)
{
  SANE_Status status;
  int read_len;

  DBG (5, "CANON_read called\n");

  if (!chndl->fp)
    {
      return SANE_STATUS_INVAL;
    }

  read_len = fread (data, 1, max_length, chndl->fp);
  /* return some data */
  if (read_len > 0)
    {
      *length = read_len;
      DBG (5, "CANON_read returned (%d/%d)\n", *length, max_length);
      return SANE_STATUS_GOOD;
    }

  /* EOF or file err */
  *length = 0;

  if (feof (chndl->fp))
    {
      DBG (4, "EOF\n");
      status = SANE_STATUS_EOF;
    }
  else
    {
      DBG (4, "IO ERR\n");
      status = SANE_STATUS_IO_ERROR;
    }

  CANON_finish_scan (chndl);
  DBG (5, "CANON_read returned (%d/%d)\n", *length, max_length);
  return status;
}
