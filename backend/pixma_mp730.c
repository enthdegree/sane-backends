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


#ifdef __GNUC__
# define UNUSED(v) (void) v
#else
# define UNUSED(v)
#endif

#define IMAGE_BLOCK_SIZE (0xc000)
#define CMDBUF_SIZE 512


enum mp730_state_t
{
  state_idle,
  state_warmup,
  state_scanning,
  state_transfering,
  state_finished
};

enum mp730_cmd_t
{
  cmd_start_session = 0xdb20,
  cmd_select_source = 0xdd20,
  cmd_gamma = 0xee20,
  cmd_scan_param = 0xde20,
  cmd_status = 0xf320,
  cmd_abort_session = 0xef20,
  cmd_time = 0xeb80,
  cmd_read_image = 0xd420,

  cmd_activate = 0xcf60
};

typedef struct mp730_t
{
  enum mp730_state_t state;
  pixma_cmdbuf_t cb;
  unsigned raw_width;
  uint8_t current_status[12];

  uint8_t *buf, *imgbuf, *lbuf;
  unsigned imgbuf_len;

  unsigned last_block:1;
} mp730_t;


static void mp730_finish_scan (pixma_t * s);

static int
has_paper (pixma_t * s)
{
  mp730_t *mp = (mp730_t *) s->subdriver;
  return (mp->current_status[1] == 0);
}

static void
drain_bulk_in (pixma_t * s)
{
  mp730_t *mp = (mp730_t *) s->subdriver;
  while (pixma_read (s->io, mp->imgbuf, IMAGE_BLOCK_SIZE) ==
	 IMAGE_BLOCK_SIZE);
}

static int
abort_session (pixma_t * s)
{
  mp730_t *mp = (mp730_t *) s->subdriver;
  return pixma_exec_short_cmd (s, &mp->cb, cmd_abort_session);
}

static int
query_status (pixma_t * s)
{
  mp730_t *mp = (mp730_t *) s->subdriver;
  uint8_t *data;
  int error;

  data = pixma_newcmd (&mp->cb, cmd_status, 0, 12);
  error = pixma_exec (s, &mp->cb);
  if (error >= 0)
    {
      memcpy (mp->current_status, data, 12);
    }
  return error;
}

static int
activate (pixma_t * s, uint8_t x)
{
  mp730_t *mp = (mp730_t *) s->subdriver;
  uint8_t *data = pixma_newcmd (&mp->cb, cmd_activate, 10, 0);
  data[0] = 1;
  data[3] = x;
  return pixma_exec (s, &mp->cb);
}

static int
start_session (pixma_t * s)
{
  mp730_t *mp = (mp730_t *) s->subdriver;
  return pixma_exec_short_cmd (s, &mp->cb, cmd_start_session);
}

static int
select_source (pixma_t * s)
{
  mp730_t *mp = (mp730_t *) s->subdriver;
  uint8_t *data = pixma_newcmd (&mp->cb, cmd_select_source, 10, 0);
  data[0] = (s->param->source == PIXMA_SOURCE_ADF) ? 2 : 1;
  return pixma_exec (s, &mp->cb);
}

static int
send_scan_param (pixma_t * s)
{
  mp730_t *mp = (mp730_t *) s->subdriver;
  uint8_t *data;
  unsigned mode;

  data = pixma_newcmd (&mp->cb, cmd_scan_param, 0x2e, 0);
  pixma_set_be16 (s->param->xdpi | 0x1000, data + 0x04);
  pixma_set_be16 (s->param->ydpi | 0x1000, data + 0x06);
  pixma_set_be32 (s->param->x, data + 0x08);
  pixma_set_be32 (s->param->y, data + 0x0c);
  pixma_set_be32 (mp->raw_width, data + 0x10);
  pixma_set_be32 (s->param->h, data + 0x14);
  mode = (s->param->channels == 1) ? 0x0408 : 0x0818;
  pixma_set_be16 (mode, data + 0x18);
  data[0x1f] = 0x7f;
  data[0x20] = 0xff;
  data[0x23] = 0x81;
  return pixma_exec (s, &mp->cb);
}

static int
read_image_block (pixma_t * s, uint8_t * header, uint8_t * data)
{
  static const uint8_t cmd[10] =	/* 0xd420 cmd */
  { 0xd4, 0x20, 0, 0, 0, 0, 0, IMAGE_BLOCK_SIZE / 256, 4, 0 };
  mp730_t *mp = (mp730_t *) s->subdriver;
  const int hlen = 2 + 4;
  int error, datalen;

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
	  if (error < 0)
	    return error;
	  datalen += error;
	}
    }

  mp->state = state_scanning;
  mp->cb.expected_reslen = 0;
  error = pixma_check_result (&mp->cb);
  if (error < 0)
    return error;
  if (mp->cb.reslen < hlen)
    return -EPROTO;
  return datalen;
}

static int
step1 (pixma_t * s)
{
  int error;

  error = query_status (s);
  if (error < 0)
    return error;
  if (s->param->source == PIXMA_SOURCE_ADF && !has_paper (s))
    return -ENODATA;
  if (error >= 0)
    error = activate (s, 0);
  if (error >= 0)
    error = activate (s, 4);
  return error;
}

static void
pack_rgb (const uint8_t * src, unsigned nlines, unsigned w, uint8_t * dst)
{
  unsigned w2, stride;

  w2 = 2 * w;
  stride = 3 * w;
  for (; nlines != 0; nlines--)
    {
      unsigned x;
      for (x = 0; x != w; x++)
	{
	  *dst++ = src[x + 0];
	  *dst++ = src[x + w];
	  *dst++ = src[x + w2];
	}
      src += stride;
    }
}

static int
mp730_open (pixma_t * s)
{
  mp730_t *mp;
  uint8_t *buf;

  mp = (mp730_t *) calloc (1, sizeof (*mp));
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

  mp->cb.buf = buf;
  mp->cb.size = CMDBUF_SIZE;
  mp->cb.res_header_len = 2;
  mp->cb.cmd_header_len = 10;
  mp->cb.cmd_len_field_ofs = 7;

  return 0;
}

static void
mp730_close (pixma_t * s)
{
  mp730_t *mp = (mp730_t *) s->subdriver;

  mp730_finish_scan (s);
  free (mp->cb.buf);
  free (mp);
  s->subdriver = NULL;
}

static unsigned
calc_raw_width (const pixma_t * s, const pixma_scan_param_t * sp)
{
  unsigned raw_width, maxw;
  /* FIXME: Does MP730 need the alignment? */
  if (sp->channels == 1)
    raw_width = ALIGN (sp->w, 12);
  else
    raw_width = ALIGN (sp->w, 4);
  maxw = s->cfg->width * (sp->xdpi / 75);
  return (raw_width < maxw) ? raw_width : maxw;
}

static int
mp730_check_param (pixma_t * s, pixma_scan_param_t * sp)
{
  unsigned raw_width;

  UNUSED (s);

  sp->depth = 8;		/* FIXME: Does MP730 supports other depth? */
  raw_width = calc_raw_width (s, sp);
  sp->line_size = raw_width * sp->channels;
  return 0;
}

static int
mp730_scan (pixma_t * s)
{
  int error, n;
  mp730_t *mp = (mp730_t *) s->subdriver;
  uint8_t *buf;

  if (mp->state != state_idle)
    return -EBUSY;

  mp->raw_width = calc_raw_width (s, s->param);

  n = IMAGE_BLOCK_SIZE / s->param->line_size + 1;
  buf = (uint8_t *) malloc ((n + 1) * s->param->line_size + IMAGE_BLOCK_SIZE);
  if (!buf)
    return -ENOMEM;
  mp->buf = buf;
  mp->lbuf = buf;
  mp->imgbuf = buf + n * s->param->line_size;
  mp->imgbuf_len = 0;

  error = step1 (s);
  if (error >= 0)
    error = start_session (s);
  if (error >= 0)
    mp->state = state_scanning;
  if (error >= 0)
    error = select_source (s);
  if (error >= 0)
    error = send_scan_param (s);
  if (error < 0)
    {
      mp730_finish_scan (s);
      return error;
    }
  mp->last_block = 0;
  return 0;
}

static int
mp730_fill_buffer (pixma_t * s, pixma_imagebuf_t * ib)
{
  int error, n;
  mp730_t *mp = (mp730_t *) s->subdriver;
  unsigned block_size, bytes_received;
  uint8_t header[16];

  do
    {
      do
	{
	  if (s->cancel)
	    return -ECANCELED;
	  if (mp->last_block)
	    {
	      /* end of image */
	      mp->state = state_finished;
	      return 0;
	    }

	  error = read_image_block (s, header, mp->imgbuf + mp->imgbuf_len);
	  if (error < 0)
	    return error;

	  bytes_received = error;
	  block_size = pixma_get_be16 (header + 4);
	  mp->last_block = (header[2] == 0x38);
	  if (header[2] != 0 && header[2] != 0x38)
	    {
	      PDBG (pixma_dbg (1, "WARNING: Unexpected result header\n"));
	      PDBG (pixma_hexdump (1, header, 16));
	    }
	  PASSERT (bytes_received == block_size);

	  if (block_size == 0)
	    {
	      /* no image data at this moment. */
	      pixma_sleep (100000);	/* FIXME: too short, too long? */
	    }
	}
      while (block_size == 0);

      mp->imgbuf_len += bytes_received;
      n = mp->imgbuf_len / s->param->line_size;
      /* n = number of full lines (rows) we have in the buffer. */
      if (n != 0)
	{
	  pack_rgb (mp->imgbuf, n, mp->raw_width, mp->lbuf);
	  block_size = n * s->param->line_size;
	  mp->imgbuf_len -= block_size;
	  memcpy (mp->imgbuf, mp->imgbuf + block_size, mp->imgbuf_len);
	}
    }
  while (n == 0);

  ib->rptr = mp->lbuf;
  ib->rend = mp->lbuf + block_size;
  return ib->rend - ib->rptr;
}

static void
mp730_finish_scan (pixma_t * s)
{
  int error;
  mp730_t *mp = (mp730_t *) s->subdriver;

  switch (mp->state)
    {
    case state_transfering:
      drain_bulk_in (s);
      /* fall through */
    case state_scanning:
    case state_warmup:
      error = abort_session (s);
      if (error < 0)
	PDBG (pixma_dbg (1, "WARNING:abort_session() failed: %s\n",
			 strerror (-error)));
      /* fall through */
    case state_finished:
      query_status (s);
      query_status (s);
      activate (s, 0);
      free (mp->buf);
      mp->buf = mp->lbuf = mp->imgbuf = NULL;
      mp->state = state_idle;
      /* fall through */
    case state_idle:
      break;
    }
}


static const pixma_scan_ops_t pixma_mp730_ops = {
  mp730_open,
  mp730_close,
  mp730_scan,
  mp730_fill_buffer,
  mp730_finish_scan,
  NULL /*mp730_wait_event */ ,
  mp730_check_param
};

#define DEVICE(name, pid, dpi, w, h, cap) {     \
        name,              /* name */		\
	0x04a9, pid,       /* vid pid */	\
	1,                 /* iface */		\
	&pixma_mp730_ops,  /* ops */		\
	dpi, dpi,          /* xdpi, ydpi */	\
	w, h,              /* width, height */	\
        cap                                     \
}
const pixma_config_t pixma_mp730_devices[] = {
/* TODO: check area limits */
  DEVICE ("Canon MultiPASS MP700", 0x2630, 1200, 637, 877, 0),
  DEVICE ("Canon MultiPASS MP730", 0x262f, 1200, 637, 870,
	  PIXMA_CAP_ADF | PIXMA_CAP_EXPERIMENT),
  DEVICE (NULL, 0, 0, 0, 0, 0)
};
