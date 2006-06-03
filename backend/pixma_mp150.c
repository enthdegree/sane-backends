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
/* test cases
   1. short USB packet (must be no -ETIMEDOUT)
   2. cancel using button on the printer (look for abort command)
   3. start scan while busy (status 0x1414)
   4. cancel using ctrl-c (must send abort command)
 */
#include "../include/sane/config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>		/* localtime(C90) */
#include <errno.h>		/* POSIX */

#include "pixma_rename.h"
#include "pixma_common.h"
#include "pixma_io.h"


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

  cmd_error_info = 0xff20,	/* seen in MP800 */
  cmd_e920 = 0xe920		/*     "         */
};

typedef struct mp150_t
{
  enum mp150_state_t state;
  pixma_cmdbuf_t cb;
  uint8_t *imgbuf;
  uint8_t current_status[12];

  unsigned last_block:1;
} mp150_t;


/*
  STAT:  0x0606 = ok,
         0x1515 = failed (-ECANCELED),
	 0x1414 = busy (-EBUSY)

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
   u8         block info byte 0x38 = last image data block
   u8[3]      0
   u32be      ILEN image data size
   u8[ILEN]   image data
 */

static void mp150_finish_scan (pixma_t * s);

static int
is_calibrated (pixma_t * s)
{
  mp150_t *mp = (mp150_t *) s->subdriver;
  return (mp->current_status[8] == 1);
}

static int
has_paper (pixma_t * s)
{
  mp150_t *mp = (mp150_t *) s->subdriver;
  return (mp->current_status[1] == 0);	/* FIXME: correct? */
}

static void
drain_bulk_in (pixma_t * s)
{
  mp150_t *mp = (mp150_t *) s->subdriver;
  while (pixma_read (s->io, mp->imgbuf, IMAGE_BLOCK_SIZE) ==
	 IMAGE_BLOCK_SIZE);
}

static int
abort_session (pixma_t * s)
{
  mp150_t *mp = (mp150_t *) s->subdriver;
  return pixma_exec_short_cmd (s, &mp->cb, cmd_abort_session);
}

static int
start_session (pixma_t * s)
{
  mp150_t *mp = (mp150_t *) s->subdriver;
  return pixma_exec_short_cmd (s, &mp->cb, cmd_start_session);
}

static int
select_source (pixma_t * s)
{
  mp150_t *mp = (mp150_t *) s->subdriver;
  uint8_t *data;

  data = pixma_newcmd (&mp->cb, cmd_select_source, 12, 0);
  data[0] = (s->param->source == PIXMA_SOURCE_ADF) ? 2 : 1;
  data[1] = 1;
  return pixma_exec (s, &mp->cb);
}

static int
send_gamma_table (pixma_t * s)
{
  mp150_t *mp = (mp150_t *) s->subdriver;
  const uint8_t *lut = s->param->gamma_table;
  uint8_t *data;

  data = pixma_newcmd (&mp->cb, cmd_gamma, 4096 + 8, 0);
  data[0] = (s->param->channels == 3) ? 0x10 : 0x01;
  pixma_set_be16 (0x1004, data + 2);
  if (lut)
    memcpy (data + 4, lut, 4096);
  else
    pixma_fill_gamma_table (DEFAULT_GAMMA, data + 4, 4096);
  return pixma_exec (s, &mp->cb);
}

static int
send_scan_param (pixma_t * s)
{
  mp150_t *mp = (mp150_t *) s->subdriver;
  uint8_t *data;
  unsigned raw_width;

  /* NOTE: MP150 can cope with "unaligned" width. It always correctly
     returns padded rows. Can other models do this, too? */
  if (s->param->channels == 1)
    raw_width = ALIGN (s->param->w, 12);
  else
    raw_width = ALIGN (s->param->w, 4);

  data = pixma_newcmd (&mp->cb, cmd_scan_param, 0x30, 0);
  pixma_set_be16 (s->param->xdpi | 0x8000, data + 0x04);
  pixma_set_be16 (s->param->ydpi | 0x8000, data + 0x06);
  pixma_set_be32 (s->param->x, data + 0x08);
  pixma_set_be32 (s->param->y, data + 0x0c);
  pixma_set_be32 (raw_width, data + 0x10);
  pixma_set_be32 (s->param->h, data + 0x14);
  data[0x18] = (s->param->channels == 1) ? 0x04 : 0x08;
  data[0x19] = s->param->channels * s->param->depth;	/* bits per pixel */
  data[0x20] = 0xff;
  data[0x23] = 0x81;
  data[0x26] = 0x02;
  data[0x27] = 0x01;
  return pixma_exec (s, &mp->cb);
}

static int
query_status (pixma_t * s)
{
  mp150_t *mp = (mp150_t *) s->subdriver;
  uint8_t *data;
  int error;

  data = pixma_newcmd (&mp->cb, cmd_status, 0, 12);
  error = pixma_exec (s, &mp->cb);
  if (error >= 0)
    {
      memcpy (mp->current_status, data, 12);
      PDBG (pixma_dbg (3, "Current status: %s %s\n",
		       is_calibrated (s) ? "Calibrated" : "Calibrating",
		       has_paper (s) ? "HasPaper" : "NoPaper"));
    }
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
  static const uint8_t cmd[16] =	/* 0xd420 cmd */
  { 0xd4, 0x20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    IMAGE_BLOCK_SIZE / 65536, 0, 8
  };
  mp150_t *mp = (mp150_t *) s->subdriver;
  const int hlen = 8 + 8;
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

/*
handle_interrupt() waits until it receives an interrupt packet or times out.
It calls send_time() and query_status() if necessary. Therefore, make sure
that handle_interrupt() is only called from a safe context for send_time()
and query_status().

   Returns:
   0     timed out
   1     interrupt packet received
   -EINTR interrupted by signal
   <0    error
*/
static int
handle_interrupt (pixma_t * s, int timeout)
{
  uint8_t buf[16];
  int len;

  len = pixma_wait_interrupt (s->io, buf, sizeof (buf), timeout);
  if (len == -ETIMEDOUT)
    return 0;
  if (len < 0)
    return len;
  if (len != 16)
    {
      PDBG (pixma_dbg (1, "WARNING:unexpected interrupt packet length %d\n",
		       len));
      return -EPROTO;
    }

  switch (buf[0])
    {
    case 0:
      /* no button pressed */
      break;
    case 1:
      s->events = PIXMA_EV_BUTTON1 | buf[1];
      break;
    case 2:
      s->events = PIXMA_EV_BUTTON2 | buf[1];
      break;
    default:
      PDBG (pixma_dbg (2, "Interesting interrupt packet\n"));
      PDBG (pixma_hexdump (2, buf, 16));
    }
  /* More than one event can be reported at the same time. */
  if (buf[3] & 1)
    send_time (s);
  if (buf[9] & 2)
    query_status (s);
  return 1;
}

static int
wait_until_ready (pixma_t * s)
{
  int error, tmo = 60;

  error = query_status (s);
  if (error < 0)
    return error;
  while (!is_calibrated (s))
    {
      error = handle_interrupt (s, 1000);
      if (s->cancel)
	return -ECANCELED;
      if (error != -EINTR && error < 0)
	return error;
      if (--tmo == 0)
	{
	  PDBG (pixma_dbg (1, "WARNING:Timed out in wait_until_ready()\n"));
	  return -ETIMEDOUT;
	}
#if 0
      /* If we use sanei_usb_*, we sometimes lose interrupts! So poll the
       * status here. */
      error = query_status (s);
      if (error < 0)
	return error;
#endif
    }
  return 0;
}

static int
mp150_open (pixma_t * s)
{
  mp150_t *mp;
  uint8_t *buf;

  mp = (mp150_t *) calloc (1, sizeof (*mp));
  if (!mp)
    return -ENOMEM;

  buf = (uint8_t *) malloc (CMDBUF_SIZE + IMAGE_BLOCK_SIZE);
  if (!buf)
    {
      free (mp);
      return -ENOMEM;
    }

  s->subdriver = mp;
  mp->state = state_idle;

  mp->cb.buf = buf;
  mp->cb.size = CMDBUF_SIZE;
  mp->cb.res_header_len = 8;
  mp->cb.cmd_header_len = 16;
  mp->cb.cmd_len_field_ofs = 14;

  mp->imgbuf = buf + CMDBUF_SIZE;

  handle_interrupt (s, 200);
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
  unsigned raw_width;

  UNUSED (s);

  sp->depth = 8;		/* MP150 only supports 8 bit per channel. */
  if (sp->channels == 1)
    raw_width = ALIGN (sp->w, 12);
  else
    raw_width = ALIGN (sp->w, 4);
  sp->line_size = raw_width * sp->channels;
  return 0;
}

static int
mp150_scan (pixma_t * s)
{
  int error, tmo;
  mp150_t *mp = (mp150_t *) s->subdriver;

  if (mp->state != state_idle)
    return -EBUSY;

  while (handle_interrupt (s, 0) > 0)
    {
    }				/* clear interrupt packets buffer */

  if (s->param->source == PIXMA_SOURCE_ADF)
    {
      error = query_status (s);
      if (error < 0)
	return error;
#ifndef NDEBUG
      pixma_dbg (1,
		 "Support for ADF is untested. If it doesn't work, please set\n");
      pixma_dbg (1,
		 "the debug level to 10 and send me debug messages for these\n");
      pixma_dbg (1, "2 cases:\n");
      pixma_dbg (1, "  1. no paper in ADF\n");
      pixma_dbg (1, "  2. paper in ADF\n");
#endif
      if (!has_paper (s))
	return -ENODATA;
    }

  error = start_session (s);
  tmo = 10;
  while (error == -EBUSY && --tmo >= 0)
    {
      if (s->cancel)
	{
	  error = -ECANCELED;
	  break;
	}
      PDBG (pixma_dbg
	    (2, "Scanner is busy. Timed out in %d sec.\n", tmo + 1));
      pixma_sleep (1000000);
      error = start_session (s);
    }
  if (error == -EBUSY || error == -ETIMEDOUT)
    {
      /* The scanner maybe hangs. We try to empty output buffer of the
       * scanner and issue the cancel command. */
      PDBG (pixma_dbg (2, "Scanner hangs? Sending abort_session command.\n"));
      drain_bulk_in (s);
      abort_session (s);
      pixma_sleep (500000);
      error = start_session (s);
    }
  if (error >= 0)
    mp->state = state_warmup;
  if (error >= 0)
    error = select_source (s);
  if (error >= 0)
    error = send_gamma_table (s);
  if (error >= 0)
    error = send_scan_param (s);
  if (error < 0)
    {
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
  unsigned block_size, bytes_received;
  uint8_t header[16];

  if (mp->state == state_warmup)
    {
      error = wait_until_ready (s);
      if (error < 0)
	return error;
      pixma_sleep (1500000);	/* No need to wait, actually, but Window's driver
				 * waits 1.5 sec. */
      mp->state = state_scanning;
      mp->last_block = 0;
    }

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

      error = read_image_block (s, header, mp->imgbuf);
      if (error < 0)
	return error;

      bytes_received = error;
      block_size = pixma_get_be32 (header + 12);
      mp->last_block = (header[8] == 0x38);
      if (header[8] != 0 && header[8] != 0x38)
	{
	  PDBG (pixma_dbg (1, "WARNING: Unexpected result header\n"));
	  PDBG (pixma_hexdump (1, header, 16));
	}
      PASSERT (bytes_received == block_size);

      if (block_size == 0)
	{
	  /* no image data at this moment. */
	  pixma_sleep (10000);
	}
    }
  while (block_size == 0);

  ib->rptr = mp->imgbuf;
  ib->rend = mp->imgbuf + bytes_received;
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
      error = abort_session (s);
      if (error < 0)
	PDBG (pixma_dbg (1, "WARNING:abort_session() failed: %s\n",
			 strerror (-error)));
      /* fall through */
    case state_finished:
      mp->state = state_idle;
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

  error = query_status (s);
  if (error < 0)
    return error;
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

#define DEVICE(name, pid, dpi, cap) {		\
        name,              /* name */		\
	0x04a9, pid,       /* vid pid */	\
	0,                 /* iface */		\
	&pixma_mp150_ops,  /* ops */		\
	dpi, 2*(dpi),      /* xdpi, ydpi */	\
	638, 877,          /* width, height */	\
	PIXMA_CAP_EASY_RGB|PIXMA_CAP_GRAY|	\
        PIXMA_CAP_GAMMA_TABLE|PIXMA_CAP_EVENTS|cap	\
}

const pixma_config_t pixma_mp150_devices[] = {
  DEVICE ("Canon PIXMA MP150", 0x1709, 1200, 0),
  DEVICE ("Canon PIXMA MP170", 0x170a, 1200, 0),
  DEVICE ("Canon PIXMA MP450", 0x170b, 1200, 0),
  DEVICE ("Canon PIXMA MP500", 0x170c, 1200, 0),
  DEVICE ("Canon PIXMA MP530", 0xffff, 1200,
	  PIXMA_CAP_ADF | PIXMA_CAP_EXPERIMENT),
  DEVICE ("Canon PIXMA MP800", 0x170d, 2400, PIXMA_CAP_TPU),
  DEVICE ("Canon PIXMA MP800R", 0xffff, 2400,
	  PIXMA_CAP_TPU | PIXMA_CAP_EXPERIMENT),
  DEVICE ("Canon PIXMA MP830", 0x1713, 2400, PIXMA_CAP_ADF),
  DEVICE (NULL, 0, 0, 0)
};
