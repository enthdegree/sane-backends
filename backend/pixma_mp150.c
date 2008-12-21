/* SANE - Scanner Access Now Easy.

   Copyright (C) 2007-2008 Nicolas Martin, <nicols-guest at alioth dot debian dot org>
   Copyright (C) 2006-2007 Wittawat Yamwong <wittawat@web.de>

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
 */
/* test cases
   1. short USB packet (must be no -ETIMEDOUT)
   2. cancel using button on the printer (look for abort command)
   3. start scan while busy (status 0x1414)
   4. cancel using ctrl-c (must send abort command)
 */

#define TPU_48         /* uncomment to activate TPU scan at 48 bits */
/*#define DEBUG_TPU_48*/   /* uncomment to debug 48 bits TPU on a non TPU device */
/*#define DEBUG_TPU_24*/   /* uncomment to debug 24 bits TPU on a non TPU device */

#include "../include/sane/config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>		/* localtime(C90) */

#include "pixma_rename.h"
#include "pixma_common.h"
#include "pixma_io.h"

/* Some macro code to enhance readability */
#define RET_IF_ERR(x) do {	\
    if ((error = (x)) < 0)	\
      return error;		\
  } while(0)
  
#define WAIT_INTERRUPT(x) do {			\
    error = handle_interrupt (s, x);		\
    if (s->cancel)				\
      return PIXMA_ECANCELED;			\
    if (error != PIXMA_ECANCELED && error < 0)	\
      return error;				\
  } while(0)

#ifdef __GNUC__
# define UNUSED(v) (void) v
#else
# define UNUSED(v)
#endif

/* Size of the command buffer should be multiple of wMaxPacketLength and
   greater than 4096+24.
   4096 = size of gamma table. 24 = header + checksum */
#define IMAGE_BLOCK_SIZE (512*1024)
#define CMDBUF_SIZE (4096 + 24)
#define DEFAULT_GAMMA 1.0
#define UNKNOWN_PID 0xffff


#define CANON_VID 0x04a9

/* Generation 1 */
#define MP150_PID 0x1709
#define MP170_PID 0x170a
#define MP450_PID 0x170b
#define MP500_PID 0x170c
#define MP530_PID 0x1712
#define MP800_PID 0x170d
#define MP800R_PID 0x170e
#define MP830_PID 0x1713

/* Generation 2 */
#define MP160_PID 0x1714
#define MP180_PID 0x1715
#define MP460_PID 0x1716
#define MP510_PID 0x1717
#define MP600_PID 0x1718
#define MP600R_PID 0x1719
#define MP810_PID 0x171a
#define MP960_PID 0x171b

#define MP140_PID 0x172b

/* Generation 3 */
/* PIXMA 2007 vintage */
#define MX7600_PID 0x171c
#define MP210_PID 0x1721
#define MP220_PID 0x1722
#define MP470_PID 0x1723
#define MP520_PID 0x1724
#define MP610_PID 0x1725
#define MP970_PID 0x1726
#define MX300_PID 0x1727
#define MX310_PID 0x1728
#define MX700_PID 0x1729
#define MX850_PID 0x172c

/* PIXMA 2008 vintage */
#define MP980_PID 0x172d    /* Untested */
#define MP630_PID 0x172e
#define MP620_PID 0x172f
#define MP540_PID 0x1730    /* Untested */
#define MP480_PID 0x1731    /* Untested */
#define MP240_PID 0x1732    /* Untested */
#define MP260_PID 0x1733    /* Untested */
#define MP190_PID 0x1734



enum mp150_state_t
{
  state_idle,
  state_warmup,
  state_scanning,
  state_transfering,
  state_finished
};

enum mp150_cmd_t
{
  cmd_start_session = 0xdb20,
  cmd_select_source = 0xdd20,
  cmd_gamma = 0xee20,
  cmd_scan_param = 0xde20,
  cmd_status = 0xf320,
  cmd_abort_session = 0xef20,
  cmd_time = 0xeb80,
  cmd_read_image = 0xd420,
  cmd_error_info = 0xff20,

  cmd_start_calibrate_ccd_3 = 0xd520,
  cmd_end_calibrate_ccd_3 = 0xd720,
  cmd_scan_param_3 = 0xd820,
  cmd_scan_start_3 = 0xd920,
  cmd_status_3 = 0xda20,
  cmd_get_tpu_info_3 = 0xf520,
  cmd_set_tpu_info_3 = 0xea20,

  cmd_e920 = 0xe920		/* seen in MP800 */
};

typedef struct mp150_t
{
  enum mp150_state_t state;
  pixma_cmdbuf_t cb;
  uint8_t *imgbuf;
  uint8_t current_status[16];
  unsigned last_block;
  uint8_t generation;
  /* for Generation 3 and CCD shift */
  uint8_t *linebuf;
  uint8_t *data_left_ofs;
  unsigned data_left_len;
  int shift[3];
  unsigned color_shift;
  unsigned stripe_shift;
  uint8_t tpu_datalen;
  uint8_t tpu_data[0x40];
} mp150_t;

/*
  STAT:  0x0606 = ok,
         0x1515 = failed (PIXMA_ECANCELED),
	 0x1414 = busy (PIXMA_EBUSY)

  Transaction scheme
    1. command_header/data | result_header
    2. command_header      | result_header/data
    3. command_header      | result_header/image_data

  - data has checksum in the last byte.
  - image_data has no checksum.
  - data and image_data begins in the same USB packet as
    command_header or result_header.

  command format #1:
   u16be      cmd
   u8[6]      0
   u8[4]      0
   u32be      PLEN parameter length
   u8[PLEN-1] parameter
   u8         parameter check sum
  result:
   u16be      STAT
   u8         0
   u8         0 or 0x21 if STAT == 0x1414
   u8[4]      0

  command format #2:
   u16be      cmd
   u8[6]      0
   u8[4]      0
   u32be      RLEN result length
  result:
   u16be      STAT
   u8[6]      0
   u8[RLEN-1] result
   u8         result check sum

  command format #3: (only used by read_image_block)
   u16be      0xd420
   u8[6]      0
   u8[4]      0
   u32be      max. block size + 8
  result:
   u16be      STAT
   u8[6]      0
   u8         block info bitfield: 0x8 = end of scan, 0x10 = no more paper, 0x20 = no more data
   u8[3]      0
   u32be      ILEN image data size
   u8[ILEN]   image data
 */

static void mp150_finish_scan (pixma_t * s);

static int
is_scanning_from_adf (pixma_t * s)
{
  return (s->param->source == PIXMA_SOURCE_ADF
	  || s->param->source == PIXMA_SOURCE_ADFDUP);
}

static int
is_scanning_from_adfdup (pixma_t * s)
{
  return (s->param->source == PIXMA_SOURCE_ADFDUP);
}

static int
is_scanning_from_tpu (pixma_t * s)
{
  return (s->param->source == PIXMA_SOURCE_TPU);
}

static void
new_cmd_tpu_msg (pixma_t *s, pixma_cmdbuf_t * cb, uint16_t cmd)
{
  pixma_newcmd (cb, cmd, 0, 0);
  cb->buf[3] = (is_scanning_from_tpu (s)) ? 0x01 : 0x00;
}

static int
start_session (pixma_t * s)
{
  mp150_t *mp = (mp150_t *) s->subdriver;

  new_cmd_tpu_msg (s, &mp->cb, cmd_start_session);
  return pixma_exec (s, &mp->cb);
}

static int
start_scan_3 (pixma_t * s)
{
  mp150_t *mp = (mp150_t *) s->subdriver;

  new_cmd_tpu_msg (s, &mp->cb, cmd_scan_start_3);
  return pixma_exec (s, &mp->cb);
}

static int
send_cmd_start_calibrate_ccd_3 (pixma_t * s)
{
  mp150_t *mp = (mp150_t *) s->subdriver;
  
  pixma_newcmd (&mp->cb, cmd_start_calibrate_ccd_3, 0, 0);
  mp->cb.buf[3] = 0x01;
  return pixma_exec (s, &mp->cb);
}

static int
is_calibrated (pixma_t * s)
{
  mp150_t *mp = (mp150_t *) s->subdriver;
  if (mp->generation == 3)
    {
      return ((mp->current_status[0] & 0x01) == 1);
    }
  if (mp->generation == 1)
    {
      return (mp->current_status[8] == 1);
    }
  else
    {
      return (mp->current_status[9] == 1);
    }
}

static int
has_paper (pixma_t * s)
{
  mp150_t *mp = (mp150_t *) s->subdriver;
  
  if (is_scanning_from_adfdup (s))
    return (mp->current_status[1] == 0 || mp->current_status[2] == 0);
  else
    return (mp->current_status[1] == 0);
}

static void
drain_bulk_in (pixma_t * s)
{
  mp150_t *mp = (mp150_t *) s->subdriver;
  while (pixma_read (s->io, mp->imgbuf, IMAGE_BLOCK_SIZE) >= 0);
}

static int
abort_session (pixma_t * s)
{
  mp150_t *mp = (mp150_t *) s->subdriver;
  return pixma_exec_short_cmd (s, &mp->cb, cmd_abort_session);
}

static int
send_cmd_e920 (pixma_t * s)
{
  mp150_t *mp = (mp150_t *) s->subdriver;
  return pixma_exec_short_cmd (s, &mp->cb, cmd_e920);
}

static int
select_source (pixma_t * s)
{
  mp150_t *mp = (mp150_t *) s->subdriver;
  uint8_t *data;

  data = pixma_newcmd (&mp->cb, cmd_select_source, 12, 0);
  data[5] = ((mp->generation == 2) ? 1 : 0);
  switch (s->param->source)
    {
      case PIXMA_SOURCE_FLATBED:
        data[0] = 1;
        data[1] = 1;
        break;
        
      case PIXMA_SOURCE_ADF:
        data[0] = 2;
        data[5] = 1;
        data[6] = 1;
        break;
        
      case PIXMA_SOURCE_ADFDUP:
        data[0] = 2;
        data[5] = 3;
        data[6] = 3;
        break;
        
      case PIXMA_SOURCE_TPU:
        data[0] = 4;
        data[1] = 2;
        break;
    }
/*  if (s->cfg->pid == MP830_PID)
    {
      switch (s->param->source)
        {
        case PIXMA_SOURCE_ADF:
          data[0] = 2;
          data[5] = 1;
          data[6] = 1;
          break;

        case PIXMA_SOURCE_ADFDUP:
          data[0] = 2;
          data[5] = 3;
          data[6] = 3;
          break;

        case PIXMA_SOURCE_TPU:
          PDBG (pixma_dbg (1, "BUG:select_source(): unsupported source %d\n",
               s->param->source));

        case PIXMA_SOURCE_FLATBED:
          data[0] = 1;
          data[1] = 1;
          break;
        }
    }
  else
    {
      data[0] = (s->param->source == PIXMA_SOURCE_ADF) ? 2 : 1;
      data[1] = 1;
      if (mp->generation == 2)
        {
          data[5] = 1;
        }
    } */
  return pixma_exec (s, &mp->cb);
}

static int
send_get_tpu_info_3 (pixma_t * s)
{
  mp150_t *mp = (mp150_t *) s->subdriver;
  uint8_t *data;
  int error;

  data = pixma_newcmd (&mp->cb, cmd_get_tpu_info_3, 0, 0x34);
  RET_IF_ERR (pixma_exec (s, &mp->cb));
  memcpy (mp->tpu_data, data, 0x34);
  return error;
}

static int
send_set_tpu_info (pixma_t * s)
{
  mp150_t *mp = (mp150_t *) s->subdriver;
  uint8_t *data;

  if (mp->tpu_datalen == 0)
    return 0;
  data = pixma_newcmd (&mp->cb, cmd_set_tpu_info_3, 0x34, 0);
  memcpy (data, mp->tpu_data, 0x34);
  return pixma_exec (s, &mp->cb);
}

static int
send_gamma_table (pixma_t * s)
{
  mp150_t *mp = (mp150_t *) s->subdriver;
  const uint8_t *lut = s->param->gamma_table;
  uint8_t *data;

  if (mp->generation == 1)
    {
      data = pixma_newcmd (&mp->cb, cmd_gamma, 4096 + 8, 0);
      data[0] = (s->param->channels == 3) ? 0x10 : 0x01;
      pixma_set_be16 (0x1004, data + 2);
      if (lut)
	      memcpy (data + 4, lut, 4096);
      else
        pixma_fill_gamma_table (DEFAULT_GAMMA, data + 4, 4096);
    }
  else
    {
      /* FIXME: Gamma table for 2nd generation: 1024 * uint16_le */
      data = pixma_newcmd (&mp->cb, cmd_gamma, 2048 + 8, 0);
      data[0] = 0x10;
      pixma_set_be16 (0x0804, data + 2);
      if (lut)
        {
          int i;
          for (i = 0; i < 1024; i++)
            {
              int j = (i << 2) + (i >> 8);
              data[4 + 2 * i + 0] = lut[j];
              data[4 + 2 * i + 1] = lut[j];
            }
        }
      else
        {
          int i;
          pixma_fill_gamma_table (DEFAULT_GAMMA, data + 4, 2048);
          for (i = 0; i < 1024; i++)
            {
              int j = (i << 1) + (i >> 9);
              data[4 + 2 * i + 0] = data[4 + j];
              data[4 + 2 * i + 1] = data[4 + j];
            }
        }
    }
  return pixma_exec (s, &mp->cb);
}

static unsigned
calc_raw_width (const mp150_t * mp, const pixma_scan_param_t * param)
{
  unsigned raw_width;
  /* NOTE: Actually, we can send arbitary width to MP150. Lines returned
     are always padded to multiple of 4 or 12 pixels. Is this valid for
     other models, too? */
  if (mp->generation >= 2)
    {
      raw_width = ALIGN (param->w, 32);
    }
  else if (param->channels == 1)
    {
      raw_width = ALIGN (param->w, 12);
    }
  else
    {
      raw_width = ALIGN (param->w, 4);
    }
  return raw_width;
}

static int
has_ccd_sensor (pixma_t * s)
{
  return ((s->cfg->cap & PIXMA_CAP_CCD) != 0);
}

static int
is_ccd_grayscale (pixma_t * s)
{
  return (has_ccd_sensor (s) && (s->param->channels == 1));
}

/* CCD sensors don't have a Grayscale mode, but use color mode instead */
static unsigned
get_cis_ccd_line_size (pixma_t * s)
{
  return (s->param->line_size * ((is_ccd_grayscale (s)) ? 3 : 1));
}
 
static unsigned
calc_shifting (pixma_t * s)
{
  mp150_t *mp = (mp150_t *) s->subdriver;

  /* If stripes shift needed (CCD devices), how many pixels shift */
  mp->stripe_shift = 0;
  switch (s->cfg->pid)
    {
      case MP970_PID:	/* MP970 at 4800 dpi */
        if (s->param->xdpi == 4800)
          {
             if (is_scanning_from_tpu(s))
               mp->stripe_shift = 6;
             else
               mp->stripe_shift = 3;
           }
        break;

      case MP800_PID:
      case MP800R_PID:
      case MP830_PID:
      case MP960_PID:
      case MP810_PID:
        if (s->param->xdpi == 2400) 
          mp->stripe_shift = 3;
        break;

      default:     /* Default, and all CIS devices */
        break;
    }
  /* If color plane shift (CCD devices), how many pixels shift */
  mp->color_shift = mp->shift[0] = mp->shift[1] = mp->shift[2] = 0;
  if (s->param->ydpi > 75)
    {
      switch (s->cfg->pid)
        {
          case MP970_PID:
            mp->color_shift = s->param->ydpi / 50;
            mp->shift[1] = mp->color_shift * get_cis_ccd_line_size (s);
            mp->shift[0] = 0;
            mp->shift[2] = 2 * mp->shift[1];
            break;

          case MP800_PID:
          case MP800R_PID:
          case MP830_PID:
            mp->color_shift = s->param->ydpi / ((s->param->ydpi < 1200) ? 150 : 75);
            if (is_scanning_from_tpu (s)) 
              mp->color_shift = s->param->ydpi / 75;
            mp->shift[1] = mp->color_shift * get_cis_ccd_line_size (s);
            mp->shift[0] = 2 * mp->shift[1];
            mp->shift[2] = 0;
            break;

          case MP810_PID:
          case MP960_PID:
            mp->color_shift = s->param->ydpi / 50;
            mp->shift[1] = mp->color_shift * get_cis_ccd_line_size (s);
            mp->shift[0] = 2 * mp->shift[1];
            mp->shift[2] = 0;
            break;
            
          default:
            break;
        }
    }
  return (2 * mp->color_shift + mp->stripe_shift);
}

static int
send_scan_param (pixma_t * s)
{
  mp150_t *mp = (mp150_t *) s->subdriver;
  uint8_t *data;
  unsigned raw_width = calc_raw_width (mp, s->param);
  unsigned h = MIN (s->param->h + calc_shifting (s), 
                    s->cfg->height * s->param->ydpi / 75);

  if (mp->generation <= 2)
    {
      data = pixma_newcmd (&mp->cb, cmd_scan_param, 0x30, 0);
      pixma_set_be16 (s->param->xdpi | 0x8000, data + 0x04);
      pixma_set_be16 (s->param->ydpi | 0x8000, data + 0x06);
      pixma_set_be32 (s->param->x, data + 0x08);
      pixma_set_be32 (s->param->y, data + 0x0c);
      pixma_set_be32 (raw_width, data + 0x10);
      pixma_set_be32 (h, data + 0x14);
      data[0x18] = ((s->param->channels != 1) || is_ccd_grayscale (s)) ? 0x08 : 0x04;
      data[0x19] = s->param->depth * ((is_ccd_grayscale (s)) ? 3 : s->param->channels);	/* bits per pixel */
      data[0x1a] = (is_scanning_from_tpu (s) ? 1 : 0);
      data[0x20] = 0xff;
      data[0x23] = 0x81;
      data[0x26] = 0x02;
      data[0x27] = 0x01;
    }
  else
    {
      data = pixma_newcmd (&mp->cb, cmd_scan_param_3, 0x38, 0);
      data[0x00] = (is_scanning_from_adf (s)) ? 0x02 : 0x01;
      data[0x01] = 0x01;
      if (is_scanning_from_tpu (s))
        {
          data[0x00] = 0x04;
          data[0x01] = 0x02;
          data[0x1e] = 0x02;
        }
      data[0x02] = 0x01;
      if (is_scanning_from_adfdup (s))
        {
          data[0x02] = 0x03;
          data[0x03] = 0x03;
        }
      data[0x05] = 0x01;	/* This one also seen at 0. Don't know yet what's used for */
      pixma_set_be16 (s->param->xdpi | 0x8000, data + 0x08);
      pixma_set_be16 (s->param->ydpi | 0x8000, data + 0x0a);
      pixma_set_be32 (s->param->x, data + 0x0c);
      pixma_set_be32 (s->param->y, data + 0x10);
      pixma_set_be32 (raw_width, data + 0x14);
      pixma_set_be32 (h, data + 0x18);
      data[0x1c] = ((s->param->channels != 1) || is_ccd_grayscale (s)) ? 0x08 : 0x04;
      
#ifdef DEBUG_TPU_48
      data[0x1d] = 24;
#else
      data[0x1d] = (is_scanning_from_tpu (s)) ? 48 :
                   s->param->depth * ((is_ccd_grayscale (s)) ? 3 : s->param->channels);  /* bits per pixel */
#endif

      data[0x1f] = 0x01;
      data[0x20] = 0xff;
      data[0x21] = 0x81;
      data[0x23] = 0x02;
      data[0x24] = 0x01;
      data[0x30] = 0x01;
    }
  return pixma_exec (s, &mp->cb);
}

static int
query_status_3 (pixma_t * s)
{
  mp150_t *mp = (mp150_t *) s->subdriver;
  uint8_t *data;
  int error, status_len;

  status_len = 8;
  data = pixma_newcmd (&mp->cb, cmd_status_3, 0, status_len);
  RET_IF_ERR (pixma_exec (s, &mp->cb));
  memcpy (mp->current_status, data, status_len);
  return error;
}

static int
query_status (pixma_t * s)
{
  mp150_t *mp = (mp150_t *) s->subdriver;
  uint8_t *data;
  int error, status_len;

  status_len = (mp->generation == 1) ? 12 : 16;
  data = pixma_newcmd (&mp->cb, cmd_status, 0, status_len);
  RET_IF_ERR (pixma_exec (s, &mp->cb));
  memcpy (mp->current_status, data, status_len);
  PDBG (pixma_dbg (3, "Current status: paper=%u cal=%u lamp=%u busy=%u\n",
		       data[1], data[8], data[7], data[9]));
  return error;
}

static int
send_time (pixma_t * s)
{
  /* Why does a scanner need a time? */
  time_t now;
  struct tm *t;
  uint8_t *data;
  mp150_t *mp = (mp150_t *) s->subdriver;

  data = pixma_newcmd (&mp->cb, cmd_time, 20, 0);
  pixma_get_time (&now, NULL);
  t = localtime (&now);
  snprintf ((char *) data, 16,
	    "%02d/%02d/%02d %02d:%02d",
	    t->tm_year % 100, t->tm_mon + 1, t->tm_mday,
	    t->tm_hour, t->tm_min);
  PDBG (pixma_dbg (3, "Sending time: '%s'\n", (char *) data));
  return pixma_exec (s, &mp->cb);
}

/* TODO: Simplify this function. Read the whole data packet in one shot. */
static int
read_image_block (pixma_t * s, uint8_t * header, uint8_t * data)
{
  uint8_t cmd[16];
  mp150_t *mp = (mp150_t *) s->subdriver;
  const int hlen = 8 + 8;
  int error, datalen;

  memset (cmd, 0, sizeof (cmd));
  pixma_set_be16 (cmd_read_image, cmd);
  if ((mp->last_block & 0x20) == 0)
    pixma_set_be32 ((IMAGE_BLOCK_SIZE / 65536) * 65536 + 8, cmd + 0xc);
  else
    pixma_set_be32 (32 + 8, cmd + 0xc);

  mp->state = state_transfering;
  mp->cb.reslen =
    pixma_cmd_transaction (s, cmd, sizeof (cmd), mp->cb.buf, 512);
  datalen = mp->cb.reslen;
  if (datalen < 0)
    return datalen;

  memcpy (header, mp->cb.buf, hlen);
  if (datalen >= hlen)
    {
      datalen -= hlen;
      memcpy (data, mp->cb.buf + hlen, datalen);
      data += datalen;
      if (mp->cb.reslen == 512)
        {
          error = pixma_read (s->io, data, IMAGE_BLOCK_SIZE - 512 + hlen);
          RET_IF_ERR (error);
          datalen += error;
        }
    }

  mp->state = state_scanning;
  mp->cb.expected_reslen = 0;
  RET_IF_ERR (pixma_check_result (&mp->cb));
  if (mp->cb.reslen < hlen)
    return PIXMA_EPROTO;
  return datalen;
}

static int
read_error_info (pixma_t * s, void *buf, unsigned size)
{
  unsigned len = 16;
  mp150_t *mp = (mp150_t *) s->subdriver;
  uint8_t *data;
  int error;

  data = pixma_newcmd (&mp->cb, cmd_error_info, 0, len);
  RET_IF_ERR (pixma_exec (s, &mp->cb));
  if (buf && len < size)
    {
      size = len;
      /* NOTE: I've absolutely no idea what the returned data mean. */
      memcpy (buf, data, size);
      error = len;
    }
  return error;
}

/*
handle_interrupt() waits until it receives an interrupt packet or times out.
It calls send_time() and query_status() if necessary. Therefore, make sure
that handle_interrupt() is only called from a safe context for send_time()
and query_status().

   Returns:
   0     timed out
   1     an interrupt packet received
   PIXMA_ECANCELED interrupted by signal
   <0    error
*/
static int
handle_interrupt (pixma_t * s, int timeout)
{
  uint8_t buf[16];
  int len;

  len = pixma_wait_interrupt (s->io, buf, sizeof (buf), timeout);
  if (len == PIXMA_ETIMEDOUT)
    return 0;
  if (len < 0)
    return len;
  if (len != 16)
    {
      PDBG (pixma_dbg
	    (1, "WARNING:unexpected interrupt packet length %d\n", len));
      return PIXMA_EPROTO;
    }

  /* More than one event can be reported at the same time. */
  if (buf[3] & 1)
    send_time (s);
  if (buf[9] & 2)
    query_status (s);
  if (buf[0] & 2)
    s->events = PIXMA_EV_BUTTON2 | buf[1];	/* b/w scan */
  if (buf[0] & 1)
    s->events = PIXMA_EV_BUTTON1 | buf[1];	/* color scan */
  return 1;
}

static int
init_ccd_lamp_3 (pixma_t * s)
{
  mp150_t *mp = (mp150_t *) s->subdriver;
  uint8_t *data;
  int error, status_len, tmo;

  status_len = 8;
  RET_IF_ERR (query_status (s));
  RET_IF_ERR (query_status (s));
  RET_IF_ERR (send_cmd_start_calibrate_ccd_3 (s));
  RET_IF_ERR (query_status (s));
  tmo = 20;  /* like Windows driver, CCD lamp adjustment */
  while (--tmo >= 0)
    {
      data = pixma_newcmd (&mp->cb, cmd_end_calibrate_ccd_3, 0, status_len);
      RET_IF_ERR (pixma_exec (s, &mp->cb));
      memcpy (mp->current_status, data, status_len);
      PDBG (pixma_dbg (3, "Lamp status: %u , timeout in: %u\n", data[0], tmo));
      if (mp->current_status[0] == 3 || !is_scanning_from_tpu (s))
        break;
      WAIT_INTERRUPT (1000);
    }
  return error;
}

static int
wait_until_ready (pixma_t * s)
{
  mp150_t *mp = (mp150_t *) s->subdriver;
  int error, tmo = 60;

  RET_IF_ERR ((mp->generation == 3) ? query_status_3 (s) 
                                    : query_status (s));
  while (!is_calibrated (s))
    {
      WAIT_INTERRUPT (1000);
      if (mp->generation == 3)
        RET_IF_ERR (query_status_3 (s));
      if (--tmo == 0)
        {
          PDBG (pixma_dbg (1, "WARNING:Timed out in wait_until_ready()\n"));
          PDBG (query_status (s));
          return PIXMA_ETIMEDOUT;
        }
#if 0
      /* If we use sanei_usb_*, we sometimes lose interrupts! So poll the
       * status here. */
      RET_IF_ERR (query_status (s));
#endif
    }
  return 0;
}

static uint8_t *
shift_colors (uint8_t * dptr, uint8_t * sptr, 
              unsigned w, unsigned dpi, unsigned pid, unsigned c, 
              int * colshft, unsigned strshft)
{
  unsigned i, sr, sg, sb, st;
  UNUSED(dpi);
  UNUSED(pid);
  sr = colshft[0]; sg = colshft[1]; sb = colshft[2];
  
  for (i = 0; i < w; i++)
    {
      /* stripes shift for MP970 at 4800 dpi, MP800, MP800R, MP810 at 2400 dpi */
      st = (i % 2 == 0) ? strshft : 0;
        
      *sptr++ = *(dptr++ + sr + st);
      if (c == 6) *sptr++ = *(dptr++ + sr + st);
      *sptr++ = *(dptr++ + sg + st);
      if (c == 6) *sptr++ = *(dptr++ + sg + st);
      *sptr++ = *(dptr++ + sb + st);
      if (c == 6) *sptr++ = *(dptr++ + sb + st);
    }
  return dptr;
}

static uint8_t *
rgb_to_gray (uint8_t * gptr, uint8_t * sptr, unsigned w, unsigned c)
{
  unsigned i, j, g;

  for (i = 0; i < w; i++)
    {
      for (j = 0, g = 0; j < 3; j++)
        {
          g += *sptr++;
          if (c == 6) g += (*sptr++ << 8);
        }
        
      g /= 3;
      *gptr++ = g;
      if (c == 6) *gptr++ = (g >> 8);
    }
  return gptr;
}

static void
reorder_pixels (uint8_t * linebuf, uint8_t * sptr, unsigned c, unsigned n, 
                unsigned m, unsigned w, unsigned line_size)
{
  unsigned i;

  for (i = 0; i < w; i++)
    {
      memcpy (linebuf + c * (n * (i % m) + i / m), sptr + c * i, c);
    }
  memcpy (sptr, linebuf, line_size);
}

static void
mp970_reorder_pixels (uint8_t * linebuf, uint8_t * sptr, unsigned c, 
                      unsigned w, unsigned line_size)
{
  unsigned i, i8;

  for (i = 0; i < w; i++)
    {
      i8 = i % 8;
      memcpy (linebuf + c * (i + i8 - ((i8 > 3) ? 7 : 0)), sptr + c * i, c);
    }
  memcpy (sptr, linebuf, line_size);
}

#ifndef TPU_48
static unsigned
pack_48_24_bpc (uint8_t * sptr, unsigned n)
{
  unsigned i;
  uint8_t *cptr, lsb;
  static uint8_t offset = 0;
  
  cptr = sptr;
  if (n % 2 != 0)
    PDBG (pixma_dbg (3, "WARNING: misaligned image.\n"));
  for (i = 0; i < n; i += 2)
    {
      /* offset = 1 + (offset % 3); */
      lsb = *sptr++;
      *cptr++ = ((*sptr++) << offset) | lsb >> (8 - offset);
    }
  return (n / 2);
}
#endif

/* This function deals both with PIXMA CCD sensors producing shifted color 
 * planes images, Grayscale CCD scan and Generation 3 high dpi images.
 * Each complete line in mp->imgbuf is processed for shifting CCD sensor 
 * color planes, reordering pixels above 600 dpi for Generation 3, and
 * converting to Grayscale for CCD sensors. */
static unsigned
post_process_image_data (pixma_t * s, pixma_imagebuf_t * ib)
{
  mp150_t *mp = (mp150_t *) s->subdriver;
  unsigned c, lines, i, line_size, n, m;
  uint8_t *sptr, *dptr, *gptr;

  c = ((is_ccd_grayscale (s)) ? 3 : s->param->channels) * s->param->depth / 8;
  
  if (mp->generation == 3)
    n = s->param->xdpi / 600;
  else    /* FIXME: maybe need different values for CIS and CCD sensors */
    n = s->param->xdpi / 2400;
  if (s->cfg->pid == MP970_PID) 
    n = MIN (n, 4);
  
  m = (n > 0) ? s->param->w / n : 1;
  sptr = dptr = gptr = mp->imgbuf;
  line_size = get_cis_ccd_line_size (s);

  lines = (mp->data_left_ofs - mp->imgbuf) / line_size;
  if (lines > 2 * mp->color_shift + mp->stripe_shift)
    {
      lines -= 2 * mp->color_shift + mp->stripe_shift;
      for (i = 0; i < lines; i++, sptr += line_size)
        {
          /* Color plane and stripes shift needed by e.g. CCD */
          if (c >= 3)
            dptr = shift_colors (dptr, sptr, 
                                 s->param->w, s->param->xdpi, s->cfg->pid, c,
                                 mp->shift, mp->stripe_shift);
                       
          /* special image format for *most* devices at high dpi. 
           * MP220 is a gen3 exception */
          if (s->cfg->pid != MP220_PID && n > 0)
              reorder_pixels (mp->linebuf, sptr, c, n, m, s->param->w, line_size);
          
          /* MP970 specific reordering for 4800 dpi */
          if (s->cfg->pid == MP970_PID && s->param->xdpi == 4800)
              mp970_reorder_pixels (mp->linebuf, sptr, c, s->param->w, line_size);

          /* Color to Grayscale convert for CCD sensor */
          if (is_ccd_grayscale (s))
              gptr = rgb_to_gray (gptr, sptr, s->param->w, c);
        }
    }
  ib->rptr = mp->imgbuf;
  ib->rend = (is_ccd_grayscale (s)) ? gptr : sptr;
  return mp->data_left_ofs - sptr;    /* # of non processed bytes */
}

static int
mp150_open (pixma_t * s)
{
  mp150_t *mp;
  uint8_t *buf;

  mp = (mp150_t *) calloc (1, sizeof (*mp));
  if (!mp)
    return PIXMA_ENOMEM;

  buf = (uint8_t *) malloc (CMDBUF_SIZE + IMAGE_BLOCK_SIZE);
  if (!buf)
    {
      free (mp);
      return PIXMA_ENOMEM;
    }

  s->subdriver = mp;
  mp->state = state_idle;

  mp->cb.buf = buf;
  mp->cb.size = CMDBUF_SIZE;
  mp->cb.res_header_len = 8;
  mp->cb.cmd_header_len = 16;
  mp->cb.cmd_len_field_ofs = 14;

  mp->imgbuf = buf + CMDBUF_SIZE;

  /* General rules for setting Pixma protocol generation # */
  mp->generation = (s->cfg->pid >= MP160_PID) ? 2 : 1;
  if (s->cfg->pid >= MX7600_PID)
    mp->generation = 3;

  /* And exceptions to be added here */
  if (s->cfg->pid == MP140_PID)
    mp->generation = 2;

  /* TPU info data setup */
  mp->tpu_datalen = 0;
  
  query_status (s);
  handle_interrupt (s, 200);
  if (mp->generation == 3 && has_ccd_sensor (s)) 
    send_cmd_start_calibrate_ccd_3 (s);
  return 0;
}

static void
mp150_close (pixma_t * s)
{
  mp150_t *mp = (mp150_t *) s->subdriver;

  mp150_finish_scan (s);
  free (mp->cb.buf);
  free (mp);
  s->subdriver = NULL;
}

static int
mp150_check_param (pixma_t * s, pixma_scan_param_t * sp)
{
  mp150_t *mp = (mp150_t *) s->subdriver;

  sp->depth = 8;		/* MP150 only supports 8 bit per channel. */
#ifdef TPU_48
#ifndef DEBUG_TPU_48
  if (sp->source == PIXMA_SOURCE_TPU)
#endif
      sp->depth = 16;   /* TPU in 16 bits mode */
#endif

  if (mp->generation >= 2)
    {
      /* mod 32 and expansion of the X scan limits */
      sp->w += (sp->x) % 32;
      sp->x = ALIGN_INF (sp->x, 32);
    }
  sp->w = calc_raw_width (mp, sp);
  sp->line_size = sp->w * sp->channels * (sp->depth / 8);
  
  /* Some exceptions here for particular devices */
  /* MX850 and MX7600 can scan up to 14" with ADF, but A4 11.7" in flatbed */
  if ((s->cfg->pid == MX850_PID || s->cfg->pid == MX7600_PID)
       && sp->source == PIXMA_SOURCE_FLATBED)
    sp->h = MIN (sp->h, 877 * sp->ydpi / 75);
    
  /* TPU mode: lowest res is 150 or 300 dpi */
  if (sp->source == PIXMA_SOURCE_TPU)
    {
      uint8_t k;
      if (mp->generation >= 3)
        k = MAX (sp->xdpi, 300) / sp->xdpi;
      else
        k = MAX (sp->xdpi, 150) / sp->xdpi;
      sp->x *= k;
      sp->y *= k;
      sp->w *= k;
      sp->h *= k;
      sp->xdpi *= k;
      sp->ydpi = sp->xdpi;
    }
    
  return 0;
}

static int
mp150_scan (pixma_t * s)
{
  int error = 0, tmo, i;
  mp150_t *mp = (mp150_t *) s->subdriver;

  if (mp->state != state_idle)
    return PIXMA_EBUSY;

  /* clear interrupt packets buffer */
  while (handle_interrupt (s, 0) > 0)
    {
    }

  /* FIXME: Duplex ADF: check paper status only before odd pages (1,3,5,...). */
  if (is_scanning_from_adf (s))
    {
      if ((error = query_status (s)) < 0)
        return error;
      tmo = 10;
      while (!has_paper (s) && --tmo >= 0)
        {
          WAIT_INTERRUPT (1000);
          PDBG (pixma_dbg
            (2, "No paper in ADF. Timed out in %d sec.\n", tmo));
        }
      if (!has_paper (s))
        return PIXMA_ENO_PAPER;
    }

  if (has_ccd_sensor (s) && (mp->generation <= 2))
    {
      error = send_cmd_e920 (s);
      switch (error)
        {
          case PIXMA_ECANCELED:
          case PIXMA_EBUSY:
            PDBG (pixma_dbg (2, "cmd e920 or d520 returned %s\n", 
                             pixma_strerror (error)));
            /* fall through */
          case 0:
            query_status (s);
            break;
          default:
            PDBG (pixma_dbg (1, "WARNING:cmd e920 or d520 failed %s\n", 
                             pixma_strerror (error)));
            return error;
        }
      tmo = 3;  /* like Windows driver, CCD calibration ? */
      while (--tmo >= 0)
        {
          WAIT_INTERRUPT (1000);
          PDBG (pixma_dbg (2, "CCD Calibration ends in %d sec.\n", tmo));
        }
      /* pixma_sleep(2000000); */
    }

  tmo = 10;
  if (s->param->adf_pageid == 0 || mp->generation <= 2)
    {
      error = start_session (s);
      while (error == PIXMA_EBUSY && --tmo >= 0)
        {
          if (s->cancel)
            {
              error = PIXMA_ECANCELED;
              break;
            }
          PDBG (pixma_dbg
          (2, "Scanner is busy. Timed out in %d sec.\n", tmo + 1));
          pixma_sleep (1000000);
          error = start_session (s);
        }
      if (error == PIXMA_EBUSY || error == PIXMA_ETIMEDOUT)
        {
          /* The scanner maybe hangs. We try to empty output buffer of the
           * scanner and issue the cancel command. */
          PDBG (pixma_dbg (2, "Scanner hangs? Sending abort_session command.\n"));
          drain_bulk_in (s);
          abort_session (s);
          pixma_sleep (500000);
          error = start_session (s);
        }
      if ((error >= 0) || (mp->generation == 3))
        mp->state = state_warmup;
      if ((error >= 0) && (mp->generation <= 2))
        error = select_source (s);
      if ((error >= 0) && (mp->generation == 3) && has_ccd_sensor (s))
        error = init_ccd_lamp_3 (s);
      if ((error >= 0) && !is_scanning_from_tpu (s))
        for (i = (mp->generation == 3) ? 3 : 1 ; i > 0 && error >= 0; i--)
          error = send_gamma_table (s);
      else /* if (mp->generation == 3)   FIXME: Does this apply also to gen1/2 ? YES */
        error = send_set_tpu_info (s);
    }
  else   /* ADF pageid != 0 and gen3 */
    pixma_sleep (1000000);
  if ((error >= 0) || (mp->generation == 3))
    mp->state = state_warmup;    
  if (error >= 0)
    error = send_scan_param (s);
  if ((error >= 0) && (mp->generation == 3))
    error = start_scan_3 (s);
  if (error < 0)
    {
      mp->last_block = 0x38;   /* Force abort session if ADF scan */
      mp150_finish_scan (s);
      return error;
    }
  return 0;
}

static int
mp150_fill_buffer (pixma_t * s, pixma_imagebuf_t * ib)
{
  int error;
  mp150_t *mp = (mp150_t *) s->subdriver;
  unsigned block_size, bytes_received, proc_buf_size, line_size;
  uint8_t header[16];

  if (mp->state == state_warmup)
    {
      RET_IF_ERR (wait_until_ready (s));
      pixma_sleep (1000000);	/* No need to sleep, actually, but Window's driver
				 * sleep 1.5 sec. */
      mp->state = state_scanning;
      mp->last_block = 0;

      line_size = get_cis_ccd_line_size (s);
      proc_buf_size = (2 * calc_shifting (s) + 2) * line_size;
      mp->cb.buf = realloc (mp->cb.buf,
             CMDBUF_SIZE + IMAGE_BLOCK_SIZE + proc_buf_size);
      if (!mp->cb.buf)
        return PIXMA_ENOMEM;
      mp->linebuf = mp->cb.buf + CMDBUF_SIZE;
      mp->imgbuf = mp->data_left_ofs = mp->linebuf + line_size;
      mp->data_left_len = 0;
    }

  do
    {
      if (s->cancel)
        return PIXMA_ECANCELED;
      if ((mp->last_block & 0x28) == 0x28)
        {       /* end of image */
           mp->state = state_finished;
           return 0;
        }
      memmove (mp->imgbuf, mp->data_left_ofs, mp->data_left_len);
      error = read_image_block (s, header, mp->imgbuf + mp->data_left_len);
      if (error < 0)
        {
          if (error == PIXMA_ECANCELED)
            {
               /* NOTE: I see this in traffic logs but I don't know its meaning. */
               read_error_info (s, NULL, 0);
            }
          return error;
        }

      bytes_received = error;
      block_size = pixma_get_be32 (header + 12);
      mp->last_block = header[8] & 0x38;
      if ((header[8] & ~0x38) != 0)
        {
          PDBG (pixma_dbg (1, "WARNING: Unexpected result header\n"));
          PDBG (pixma_hexdump (1, header, 16));
        }
      PASSERT (bytes_received == block_size);

      if (block_size == 0)
        {     /* no image data at this moment. */
          pixma_sleep (10000);
        }
      /* For TPU at 48 bits/pixel to output at 24 bits/pixel */
#ifndef DEBUG_TPU_48
#ifndef TPU_48
#ifndef DEBUG_TPU_24
      if (is_scanning_from_tpu (s))
#endif
          bytes_received = pack_48_24_bpc (mp->imgbuf + mp->data_left_len, bytes_received);
#endif
#endif        
      /* Post-process the image data */
      mp->data_left_ofs = mp->imgbuf + mp->data_left_len + bytes_received;
      mp->data_left_len = post_process_image_data (s, ib);
      mp->data_left_ofs -= mp->data_left_len;
    }
  while (ib->rend == ib->rptr);

  return ib->rend - ib->rptr;
}

static void
mp150_finish_scan (pixma_t * s)
{
  int error;
  mp150_t *mp = (mp150_t *) s->subdriver;

  switch (mp->state)
    {
    case state_transfering:
      drain_bulk_in (s);
      /* fall through */
    case state_scanning:
    case state_warmup:
    case state_finished:
      /* Send the get TPU info message */
      if (is_scanning_from_tpu (s) && mp->tpu_datalen == 0)
        send_get_tpu_info_3 (s);
      /* FIXME: to process several pages ADF scan, must not send 
       * abort_session and start_session between pages (last_block=0x28) */
      if (mp->generation <= 2 || !is_scanning_from_adf (s) || mp->last_block == 0x38)
        {
          error = abort_session (s);  /* FIXME: it probably doesn't work in duplex mode! */
          if (error < 0)
            PDBG (pixma_dbg (1, "WARNING:abort_session() failed %d\n", error));
          mp->state = state_idle;
        }
      /* fall through */
    case state_idle:
      break;
    }
}

static void
mp150_wait_event (pixma_t * s, int timeout)
{
  /* FIXME: timeout is not correct. See usbGetCompleteUrbNoIntr() for
   * instance. */
  while (s->events == 0 && handle_interrupt (s, timeout) > 0)
    {
    }
}

static int
mp150_get_status (pixma_t * s, pixma_device_status_t * status)
{
  int error;

  RET_IF_ERR (query_status (s));
  status->hardware = PIXMA_HARDWARE_OK;
  status->adf = (has_paper (s)) ? PIXMA_ADF_OK : PIXMA_ADF_NO_PAPER;
  status->cal =
    (is_calibrated (s)) ? PIXMA_CALIBRATION_OK : PIXMA_CALIBRATION_OFF;
  return 0;
}

static const pixma_scan_ops_t pixma_mp150_ops = {
  mp150_open,
  mp150_close,
  mp150_scan,
  mp150_fill_buffer,
  mp150_finish_scan,
  mp150_wait_event,
  mp150_check_param,
  mp150_get_status
};

#define DEVICE(name, model, pid, dpi, w, h, cap) { \
        name,              /* name */               \
        model,             /* model */              \
        CANON_VID, pid,    /* vid pid */            \
        0,                 /* iface */              \
        &pixma_mp150_ops,  /* ops */                \
        dpi, 2*(dpi),      /* xdpi, ydpi */         \
        w, h,              /* width, height */      \
        PIXMA_CAP_EASY_RGB|PIXMA_CAP_GRAY|          \
        PIXMA_CAP_GAMMA_TABLE|PIXMA_CAP_EVENTS|cap  \
}

#define END_OF_DEVICE_LIST DEVICE(NULL, NULL, 0, 0, 0, 0, 0)

const pixma_config_t pixma_mp150_devices[] = {
  /* Generation 1: CIS */
  DEVICE ("Canon PIXMA MP150", "MP150", MP150_PID, 1200, 638, 877, PIXMA_CAP_CIS),
  DEVICE ("Canon PIXMA MP170", "MP170", MP170_PID, 1200, 638, 877, PIXMA_CAP_CIS),
  DEVICE ("Canon PIXMA MP450", "MP450", MP450_PID, 1200, 638, 877, PIXMA_CAP_CIS),
  DEVICE ("Canon PIXMA MP500", "MP500", MP500_PID, 1200, 638, 877, PIXMA_CAP_CIS),
  DEVICE ("Canon PIXMA MP530", "MP530", MP530_PID, 1200, 638, 877, PIXMA_CAP_CIS | PIXMA_CAP_ADF),

  /* Generation 1: CCD */
  DEVICE ("Canon PIXMA MP800", "MP800", MP800_PID, 2400, 638, 877, PIXMA_CAP_CCD | PIXMA_CAP_TPU | PIXMA_CAP_48BIT),
  DEVICE ("Canon PIXMA MP800R", "MP800R", MP800R_PID, 2400, 638, 877, PIXMA_CAP_CCD | PIXMA_CAP_TPU | PIXMA_CAP_48BIT),
  DEVICE ("Canon PIXMA MP830", "MP830", MP830_PID, 2400, 638, 877, PIXMA_CAP_CCD | PIXMA_CAP_ADFDUP | PIXMA_CAP_48BIT),

  /* Generation 2: CIS */
  DEVICE ("Canon PIXMA MP140", "MP140", MP140_PID, 600, 638, 877, PIXMA_CAP_CIS),
  DEVICE ("Canon PIXMA MP160", "MP160", MP160_PID, 600, 638, 877, PIXMA_CAP_CIS),
  DEVICE ("Canon PIXMA MP180", "MP180", MP180_PID, 1200, 638, 877, PIXMA_CAP_CIS),
  DEVICE ("Canon PIXMA MP460", "MP460", MP460_PID, 1200, 638, 877, PIXMA_CAP_CIS),
  DEVICE ("Canon PIXMA MP510", "MP510", MP510_PID, 1200, 638, 877, PIXMA_CAP_CIS),
  DEVICE ("Canon PIXMA MP600", "MP600", MP600_PID, 2400, 638, 877, PIXMA_CAP_CIS),
  DEVICE ("Canon PIXMA MP600R", "MP600R", MP600R_PID, 2400, 638, 877, PIXMA_CAP_CIS),

  /* Generation 2: CCD */
  DEVICE ("Canon PIXMA MP810", "MP810", MP810_PID, 4800, 638, 877, PIXMA_CAP_CCD | PIXMA_CAP_TPU),
  DEVICE ("Canon PIXMA MP960", "MP960", MP960_PID, 4800, 638, 877, PIXMA_CAP_CCD | PIXMA_CAP_TPU),

  /* Generation 3: CIS */
  DEVICE ("Canon PIXMA MP210", "MP210", MP210_PID, 600, 638, 877, PIXMA_CAP_CIS),
  DEVICE ("Canon PIXMA MP220", "MP220", MP220_PID, 1200, 638, 877, PIXMA_CAP_CIS),
  DEVICE ("Canon PIXMA MP470", "MP470", MP470_PID, 2400, 638, 877, PIXMA_CAP_CIS),
  DEVICE ("Canon PIXMA MP520", "MP520", MP520_PID, 2400, 638, 877, PIXMA_CAP_CIS),
  DEVICE ("Canon PIXMA MP610", "MP610", MP610_PID, 4800, 638, 877, PIXMA_CAP_CIS),

  DEVICE ("Canon PIXMA MX300", "MX300", MX300_PID, 600, 638, 877, PIXMA_CAP_CIS),
  DEVICE ("Canon PIXMA MX310", "MX310", MX310_PID, 1200, 638, 877, PIXMA_CAP_CIS | PIXMA_CAP_ADF),
  DEVICE ("Canon PIXMA MX700", "MX700", MX700_PID, 2400, 638, 877, PIXMA_CAP_CIS | PIXMA_CAP_ADF),
  DEVICE ("Canon PIXMA MX850", "MX850", MX850_PID, 2400, 638, 1050, PIXMA_CAP_CIS | PIXMA_CAP_ADFDUP),
  DEVICE ("Canon PIXMA MX7600", "MX7600", MX7600_PID, 4800, 638, 1050, PIXMA_CAP_CIS | PIXMA_CAP_ADFDUP),

  /* Generation 3 CCD not managed as Generation 2 */
  DEVICE ("Canon Pixma MP970", "MP970", MP970_PID, 4800, 638, 877, PIXMA_CAP_CCD | PIXMA_CAP_TPU),

  /* PIXMA 2008 vintage */
  DEVICE ("Canon MP980 series", "MP980", MP980_PID, 4800, 638, 877, PIXMA_CAP_CCD | PIXMA_CAP_TPU),

  DEVICE ("Canon PIXMA MP630", "MP630", MP630_PID, 4800, 638, 877, PIXMA_CAP_CIS),
  DEVICE ("Canon PIXMA MP620", "MP620", MP620_PID, 2400, 638, 877, PIXMA_CAP_CIS),
  DEVICE ("Canon PIXMA MP540", "MP540", MP540_PID, 2400, 638, 877, PIXMA_CAP_CIS),
  DEVICE ("Canon PIXMA MP480", "MP480", MP480_PID, 2400, 638, 877, PIXMA_CAP_CIS),
  DEVICE ("Canon PIXMA MP240", "MP240", MP240_PID, 1200, 638, 877, PIXMA_CAP_CIS),
  DEVICE ("Canon PIXMA MP260", "MP260", MP260_PID, 1200, 638, 877, PIXMA_CAP_CIS),
  DEVICE ("Canon PIXMA MP190", "MP190", MP190_PID, 600, 638, 877, PIXMA_CAP_CIS),

  END_OF_DEVICE_LIST
};
