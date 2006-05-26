/* SANE - Scanner Access Now Easy.

   Copyright (C) 2006 Wittawat Yamwong <wittawat@web.de>

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
#include "../include/sane/config.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>		/* POSIX */

#include "pixma_rename.h"
#include "pixma_common.h"
#include "pixma_io.h"


/* Credits should go to Martin Schewe (http://pixma.schewe.com) who analysed
   the protocol of MP750. */

/* TODO: remove lines marked with SIM. They are inserted so that I can test
   the subdriver with the simulator. WY */

#ifdef __GNUC__
# define UNUSED(v) (void) v
#else
# define UNUSED(v)
#endif

#define IMAGE_BLOCK_SIZE 0xc000
#define CMDBUF_SIZE 512
#define HAS_PAPER(s) (s[1] == 0)
#define IS_WARMING_UP(s) (s[7] != 3)
#define IS_CALIBRATED(s) (s[8] == 0xf)


enum mp750_state_t
{
  state_idle,
  state_warmup,
  state_scanning,
  state_transfering,
  state_finished
};

enum mp750_cmd_t
{
  cmd_start_session = 0xdb20,
  cmd_select_source = 0xdd20,
  cmd_scan_param = 0xde20,
  cmd_status = 0xf320,
  cmd_abort_session = 0xef20,
  cmd_time = 0xeb80,
  cmd_read_image = 0xd420,

  cmd_activate = 0xcf60,
  cmd_calibrate = 0xe920,
  cmd_reset = 0xff20
};

typedef struct mp750_t
{
  enum mp750_state_t state;
  pixma_cmdbuf_t cb;
  unsigned raw_width, raw_height;
  uint8_t current_status[12];

  uint8_t *buf, *rawimg, *img;
  unsigned rawimg_left, imgbuf_len, last_block_size, imgbuf_ofs;
  int shifted_bytes;
  unsigned last_block;

  unsigned monochrome:1;
  unsigned needs_time:1;
  unsigned needs_abort:1;
} mp750_t;



static void mp750_finish_scan (pixma_t * s);
static void check_status (pixma_t * s);

static int
has_paper (pixma_t * s)
{
  mp750_t *mp = (mp750_t *) s->subdriver;
  return HAS_PAPER (mp->current_status);
}

static int
is_warming_up (pixma_t * s)
{
  mp750_t *mp = (mp750_t *) s->subdriver;
  return IS_WARMING_UP (mp->current_status);
}

static int
is_calibrated (pixma_t * s)
{
  mp750_t *mp = (mp750_t *) s->subdriver;
  return IS_CALIBRATED (mp->current_status);
}

static void
drain_bulk_in (pixma_t * s)
{
  mp750_t *mp = (mp750_t *) s->subdriver;
  while (pixma_read (s->io, mp->buf, IMAGE_BLOCK_SIZE) == IMAGE_BLOCK_SIZE);
}

static int
abort_session (pixma_t * s)
{
  mp750_t *mp = (mp750_t *) s->subdriver;
  return pixma_exec_short_cmd (s, &mp->cb, cmd_abort_session);
}

static int
query_status (pixma_t * s)
{
  mp750_t *mp = (mp750_t *) s->subdriver;
  uint8_t *data;
  int error;

  data = pixma_newcmd (&mp->cb, cmd_status, 0, 12);
  error = pixma_exec (s, &mp->cb);
  if (error >= 0)
    {
      memcpy (mp->current_status, data, 12);
      PDBG (pixma_dbg (2, "Current status: %s %s %s\n",
		       has_paper (s) ? "HasPaper" : "NoPaper",
		       is_warming_up (s) ? "WarmingUp" : "LampReady",
		       is_calibrated (s) ? "Calibrated" : "Calibrating"));
    }
  return error;
}

static int
activate (pixma_t * s, uint8_t x)
{
  mp750_t *mp = (mp750_t *) s->subdriver;
  uint8_t *data = pixma_newcmd (&mp->cb, cmd_activate, 10, 0);
  data[0] = 1;
  data[3] = x;
  return pixma_exec (s, &mp->cb);
}

static int
activate_cs (pixma_t * s, uint8_t x)
{
   /*SIM*/ check_status (s);
  return activate (s, x);
}

static int
start_session (pixma_t * s)
{
  mp750_t *mp = (mp750_t *) s->subdriver;
  return pixma_exec_short_cmd (s, &mp->cb, cmd_start_session);
}

static int
select_source (pixma_t * s)
{
  mp750_t *mp = (mp750_t *) s->subdriver;
  uint8_t *data = pixma_newcmd (&mp->cb, cmd_select_source, 10, 0);
  data[0] = (s->param->source == PIXMA_SOURCE_ADF) ? 2 : 1;
  data[1] = 1;
  return pixma_exec (s, &mp->cb);
}

static int
send_scan_param (pixma_t * s)
{
  mp750_t *mp = (mp750_t *) s->subdriver;
  uint8_t *data;
  unsigned mode;

  mode = 0x0818;

  data = pixma_newcmd (&mp->cb, cmd_scan_param, 0x2e, 0);
  pixma_set_be16 (s->param->xdpi | 0x8000, data + 4);
  pixma_set_be16 (s->param->ydpi | 0x8000, data + 6);
  pixma_set_be32 (s->param->x, data + 8);
  pixma_set_be32 (s->param->y, data + 12);
  pixma_set_be32 (mp->raw_width, data + 16);
  pixma_set_be32 (mp->raw_height, data + 20);
  pixma_set_be16 (mode, data + 24);
  data[32] = 0xff;
  data[35] = 0x81;
  data[38] = 0x02;
  data[39] = 0x01;
  data[41] = mp->monochrome ? 0 : 1;

  return pixma_exec (s, &mp->cb);
}

static int
calibrate (pixma_t * s)
{
  mp750_t *mp = (mp750_t *) s->subdriver;
  return pixma_exec_short_cmd (s, &mp->cb, cmd_calibrate);
}

static int
calibrate_cs (pixma_t * s)
{
   /*SIM*/ check_status (s);
  return calibrate (s);
}

static int
reset_scanner (pixma_t * s)
{
  mp750_t *mp = (mp750_t *) s->subdriver;
  pixma_newcmd (&mp->cb, cmd_reset, 0, 16);
  return pixma_exec (s, &mp->cb);
}

static int
request_image_block_ex (pixma_t * s, unsigned *size, uint8_t * info,
			unsigned flag)
{
  mp750_t *mp = (mp750_t *) s->subdriver;
  int error;

  memset (mp->cb.buf, 0, 10);
  pixma_set_be16 (cmd_read_image, mp->cb.buf);
  mp->cb.buf[7] = *size >> 8;
  mp->cb.buf[8] = 4 | flag;
  mp->cb.reslen = pixma_cmd_transaction (s, mp->cb.buf, 10, mp->cb.buf, 6);
  mp->cb.expected_reslen = 0;
  error = pixma_check_result (&mp->cb);
  if (error >= 0)
    {
      if (mp->cb.reslen == 6)
	{
	  *info = mp->cb.buf[2];
	  *size = pixma_get_be16 (mp->cb.buf + 4);
	}
      else
	{
	  error = -EPROTO;
	}
    }
  return error;
}

static int
request_image_block (pixma_t * s, unsigned *size, uint8_t * info)
{
  return request_image_block_ex (s, size, info, 0);
}

static int
request_image_block2 (pixma_t * s, uint8_t * info)
{
  unsigned temp = 0;
  return request_image_block_ex (s, &temp, info, 0x20);
}

static int
read_image_block (pixma_t * s, uint8_t * data)
{
  int count, temp;

  count = pixma_read (s->io, data, IMAGE_BLOCK_SIZE);
  if (count < 0)
    return count;
  if (count == IMAGE_BLOCK_SIZE)
    pixma_read (s->io, &temp, 0);
  return count;
}

static int
handle_interrupt (pixma_t * s, int timeout)
{
  mp750_t *mp = (mp750_t *) s->subdriver;
  int error;
  uint8_t intr[16];

  error = pixma_wait_interrupt (s->io, intr, sizeof (intr), timeout);
  if (error == -ETIMEDOUT)
    return 0;
  if (error < 0)
    return error;
  if (error != 16)
    {
      PDBG (pixma_dbg (1, "WARNING:unexpected interrupt packet length %d\n",
		       error));
      return -EPROTO;
    }

  if (intr[10] & 0x40)
    mp->needs_time = 1;
  if (intr[12] & 0x40)
    query_status (s);
  return 1;
}

static void
check_status (pixma_t * s)
{
  while (handle_interrupt (s, 0) > 0);
}

static int
step1 (pixma_t * s)
{
  int error;

  error = activate (s, 0);
  if (error >= 0)
    error = query_status (s);
  if (error >= 0)
    {
      if (s->param->source == PIXMA_SOURCE_ADF && !has_paper (s))
	return -ENODATA;
    }
  if (error >= 0)
    error = activate_cs (s, 0);
  /*SIM*/ if (error >= 0)
    error = activate_cs (s, 0x20);
  if (error >= 0)
    error = calibrate_cs (s);
  return error;
}

static void
shift_rgb (const uint8_t * src, unsigned pixels,
	   int sr, int sg, int sb, uint8_t * dst)
{
  for (; pixels != 0; pixels--)
    {
      *(dst++ + sr) = *src++;
      *(dst++ + sg) = *src++;
      *(dst++ + sb) = *src++;
    }
}


static int
mp750_open (pixma_t * s)
{
  mp750_t *mp;
  uint8_t *buf;

  mp = (mp750_t *) calloc (1, sizeof (*mp));
  if (!mp)
    return -ENOMEM;

  buf = (uint8_t *) malloc (CMDBUF_SIZE);
  if (!buf)
    {
      free (mp);
      return -ENOMEM;
    }

  s->subdriver = mp;
  mp->state = state_idle;

  /* ofs:   0   1    2  3  4  5  6  7  8  9
     cmd: cmd1 cmd2 00 00 00 00 00 00 00 00
     data length-^^^^^    => cmd_len_field_ofs
     |--------- cmd_header_len --------|

     res: res1 res2
     |---------| res_header_len
   */
  mp->cb.buf = buf;
  mp->cb.size = CMDBUF_SIZE;
  mp->cb.res_header_len = 2;
  mp->cb.cmd_header_len = 10;
  mp->cb.cmd_len_field_ofs = 7;

  return 0;
}

static void
mp750_close (pixma_t * s)
{
  mp750_t *mp = (mp750_t *) s->subdriver;

  mp750_finish_scan (s);
  free (mp->cb.buf);
  free (mp);
  s->subdriver = NULL;
}

static int
mp750_check_param (pixma_t * s, pixma_scan_param_t * sp)
{
  unsigned raw_width;

  UNUSED (s);

  sp->depth = 8;		/* FIXME: Does MP750 supports other depth? */
  if (sp->channels == 1)
    raw_width = ALIGN (sp->w, 12);
  else
    raw_width = ALIGN (sp->w, 4);
  sp->line_size = raw_width * sp->channels;
  return 0;
}

static int
mp750_scan (pixma_t * s)
{
  mp750_t *mp = (mp750_t *) s->subdriver;
  int error;
  uint8_t *buf;
  unsigned size, dpi, spare;

  if (mp->state != state_idle)
    return -EBUSY;

  if (s->param->channels == 1)
    mp->raw_width = ALIGN (s->param->w, 12);
  else
    mp->raw_width = ALIGN (s->param->w, 4);

  dpi = s->param->ydpi;
  spare = 4 * dpi / 75 + 1;
  mp->raw_height = s->param->h + spare;
  PDBG (pixma_dbg (3, "raw_width=%u raw_height=%u dpi=%u\n",
		   mp->raw_width, mp->raw_height, dpi));

  size = 8 + 2 * IMAGE_BLOCK_SIZE + spare * s->param->line_size;
  buf = (uint8_t *) malloc (size);
  if (!buf)
    return -ENOMEM;
  mp->buf = buf;
  mp->rawimg = buf;
  mp->imgbuf_ofs = spare * s->param->line_size;
  mp->img = mp->rawimg + IMAGE_BLOCK_SIZE + 8;
  mp->imgbuf_len = IMAGE_BLOCK_SIZE + mp->imgbuf_ofs;
  mp->rawimg_left = 0;
  mp->last_block_size = 0;
  mp->shifted_bytes = -(int) mp->imgbuf_ofs;

  error = step1 (s);
  if (error >= 0)
    error = start_session (s);
  if (error >= 0)
    mp->state = state_warmup;
  if (error >= 0)
    error = select_source (s);
  if (error >= 0)
    error = send_scan_param (s);
  if (error < 0)
    {
      mp750_finish_scan (s);
      return error;
    }
  return 0;
}

static int
mp750_fill_buffer (pixma_t * s, pixma_imagebuf_t * ib)
{
  mp750_t *mp = (mp750_t *) s->subdriver;
  int error;
  uint8_t info;
  unsigned block_size, bytes_received, n;
  int shift[3], base_shift;

  if (mp->state == state_warmup)
    {
      int tmo = 60;

      query_status (s);
      check_status (s);
      /*SIM*/ while (!is_calibrated (s) && --tmo >= 0)
	{
	  if (s->cancel)
	    return -ECANCELED;
	  if (handle_interrupt (s, 1000) > 0)
	    {
	      block_size = 0;
	      error = request_image_block (s, &block_size, &info);
	      /*SIM*/ if (error < 0)
		return error;
	    }
	}
      if (tmo < 0)
	{
	  PDBG (pixma_dbg (1, "WARNING:Timed out waiting for calibration\n"));
	  return -ETIMEDOUT;
	}
      pixma_sleep (100000);
      query_status (s);
      if (is_warming_up (s) || !is_calibrated (s))
	{
	  PDBG (pixma_dbg (1, "WARNING:Wrong status: wup=%d cal=%d\n",
			   is_warming_up (s), is_calibrated (s)));
	  return -EPROTO;
	}
      block_size = 0;
      request_image_block (s, &block_size, &info);
      /*SIM*/ mp->state = state_scanning;
      mp->last_block = 0;
    }

  /* TODO: Move to other place, values are constant. */
  base_shift = 2 * s->param->ydpi / 75 * s->param->line_size;
  if (s->param->source == PIXMA_SOURCE_ADF)
    {
      shift[0] = 0;
      shift[1] = -base_shift;
      shift[2] = -2 * base_shift;
    }
  else
    {
      shift[0] = -2 * base_shift;
      shift[1] = -base_shift;
      shift[2] = 0;
    }

  do
    {
      if (mp->last_block_size > 0)
	{
	  block_size = mp->imgbuf_len - mp->last_block_size;
	  memcpy (mp->img, mp->img + mp->last_block_size, block_size);
	}

      do
	{
	  if (s->cancel)
	    return -ECANCELED;
	  if (mp->last_block)
	    {
	      /* end of image */
	      info = mp->last_block;
	      if (info != 0x38)
		{
		  query_status (s);
		  /*SIM*/ while ((info & 0x28) != 0x28)
		    {
		      pixma_sleep (10000);
		      error = request_image_block2 (s, &info);
		      if (s->cancel)
			return -ECANCELED;
		      if (error < 0)
			return error;
		    }
		}
	      mp->needs_abort = (info != 0x38);
	      mp->state = state_finished;
	      return 0;
	    }

	  check_status (s);
	  /*SIM*/ while (handle_interrupt (s, 1) > 0);
	  /*SIM*/ block_size = IMAGE_BLOCK_SIZE;
	  error = request_image_block (s, &block_size, &info);
	  if (error < 0)
	    return error;
	  mp->last_block = info;
	  if (info != 0 && info != 0x38)
	    {
	      PDBG (pixma_dbg (1, "WARNING: Unknown info byte %x\n", info));
	    }
	  if (block_size == 0)
	    {
	      /* no image data at this moment. */
	      pixma_sleep (10000);
	    }
	}
      while (block_size == 0);

      error = read_image_block (s, mp->rawimg + mp->rawimg_left);
      if (error < 0)
	{
	  mp->state = state_transfering;
	  return error;
	}
      bytes_received = error;
      PASSERT (bytes_received == block_size);

      /* TODO: simplify! */
      mp->rawimg_left += bytes_received;
      n = mp->rawimg_left / 3;
      /* n = number of pixels in the buffer */
      shift_rgb (mp->rawimg, n, shift[0], shift[1], shift[2],
		 mp->img + mp->imgbuf_ofs);
      n *= 3;
      mp->shifted_bytes += n;
      mp->rawimg_left -= n;	/* rawimg_left = 0, 1 or 2 bytes left in the buffer. */
      mp->last_block_size = n;
      memcpy (mp->rawimg, mp->rawimg + n, mp->rawimg_left);
    }
  while (mp->shifted_bytes <= 0);

  if ((unsigned) mp->shifted_bytes < mp->last_block_size)
    ib->rptr = mp->img + mp->last_block_size - mp->shifted_bytes;
  else
    ib->rptr = mp->img;
  ib->rend = mp->img + mp->last_block_size;
  return ib->rend - ib->rptr;
}

static void
mp750_finish_scan (pixma_t * s)
{
  int error;
  mp750_t *mp = (mp750_t *) s->subdriver;

  switch (mp->state)
    {
    case state_transfering:
      drain_bulk_in (s);
      /* fall through */
    case state_scanning:
    case state_warmup:
      error = abort_session (s);
      if (error < 0)
	{
	  PDBG (pixma_dbg (1, "WARNING:abort_session() failed: %s\n",
			   strerror (-error)));
	  reset_scanner (s);
	}
      /* fall through */
    case state_finished:
      query_status (s);
      /*SIM*/ activate (s, 0);
      if (mp->needs_abort)
	{
	  mp->needs_abort = 0;
	  abort_session (s);
	}
      free (mp->buf);
      mp->buf = mp->rawimg = NULL;
      mp->state = state_idle;
      /* fall through */
    case state_idle:
      break;
    }
}

static void
mp750_wait_event (pixma_t * s, int timeout)
{
  /* FIXME: timeout is not correct. See usbGetCompleteUrbNoIntr() for
   * instance. */
  while (s->events == 0 && handle_interrupt (s, timeout) > 0)
    {
    }
}

static const pixma_scan_ops_t pixma_mp750_ops = {
  mp750_open,
  mp750_close,
  mp750_scan,
  mp750_fill_buffer,
  mp750_finish_scan,
  mp750_wait_event,
  mp750_check_param
};

/* FIXME: What is the maximum resolution? 2400 DPI?*/

#define DEVICE(name, pid, dpi, cap) {			\
	name,                  /* name */		\
	0x04a9, pid,           /* vid pid */		\
	0,                     /* iface */		\
	&pixma_mp750_ops,      /* ops */		\
	dpi, 2*(dpi),          /* xdpi, ydpi */		\
	638, 877,              /* width, height */	\
	PIXMA_CAP_ADF|cap		\
}

const pixma_config_t pixma_mp750_devices[] = {
  DEVICE ("Canon PIXMA MP750", 0x1706, 2400, PIXMA_CAP_EXPERIMENT),
  DEVICE ("Canon PIXMA MP780", 0x1707, 2400, PIXMA_CAP_EXPERIMENT),
  DEVICE ("Canon PIXMA MP760", 0x1708, 2400, PIXMA_CAP_EXPERIMENT),
  DEVICE (NULL, 0, 0, 0)
};
