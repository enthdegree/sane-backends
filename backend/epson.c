/* epson.c - SANE library for Epson flatbed scanners.

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

#ifdef _AIX
# include <lalloca.h>		/* MUST come first for AIX! */
#endif

#include <sane/config.h>

#include <lalloca.h>

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include <sane/sane.h>
#include <sane/saneopts.h>
#include <sane/sanei_scsi.h>
#include <sane/sanei_pio.h>
#include "epson.h"

#define BACKEND_NAME	epson
#include <sane/sanei_backend.h>

#include <sane/sanei_config.h>
#define EPSON_CONFIG_FILE "epson.conf"

#ifndef PATH_MAX
# define PATH_MAX 1024
#endif

#define TEST_UNIT_READY_COMMAND 0x00
#define READ_6_COMMAND 0x08
#define WRITE_6_COMMAND 0x0a
#define INQUIRY_COMMAND 0x12
#define TYPE_PROCESSOR 0x03

#define  walloc(x)	( x *) malloc( sizeof( x) )
#define  walloca(x)	( x *) alloca( sizeof( x) )

#define	STX	0x02
#define	ACK	0x06
#define	NAK	0x15
#define	CAN	0x18
#define	ESC	0x1B

#define	EPSON_LEVEL_A1		0
#define	EPSON_LEVEL_A2		1
#define	EPSON_LEVEL_B1		2
#define	EPSON_LEVEL_B2		3
#define	EPSON_LEVEL_B3		4
#define	EPSON_LEVEL_B4		5
#define	EPSON_LEVEL_B5		6
#define	EPSON_LEVEL_B6		7

#define	EPSON_LEVEL_DEFAULT	EPSON_LEVEL_B3

static EpsonCmdRec epson_cmd[] =
{
/*
         request identity
         |  request status
         |  |  request condition
         |  |  |  set color mode
         |  |  |  |  start scanning
         |  |  |  |  |  set data format
         |  |  |  |  |  |  set resolution
         |  |  |  |  |  |  |  set zoom
         |  |  |  |  |  |  |  |  set scan area
         |  |  |  |  |  |  |  |  |  set brightness
         |  |  |  |  |  |  |  |  |  |  set gamma
         |  |  |  |  |  |  |  |  |  |  |  set halftoning
         |  |  |  |  |  |  |  |  |  |  |  |  set color correction
         |  |  |  |  |  |  |  |  |  |  |  |  |  initialize scanner
         |  |  |  |  |  |  |  |  |  |  |  |  |  |  set speed
         |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  set lcount
         |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |
 */
  {"A1", 1, 1, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1}
  ,
  {"A2", 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1}
  ,
  {"B1", 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 1, 0, 0, 0, 1}
  ,
  {"B2", 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1}
  ,
  {"B3", 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1}
  ,
  {"B4", 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1}
  ,
  {"B5", 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1}
  ,
  {"B6", 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1}
};

/* TODO: speed */

static int 
scsi_write (int fd, const void *buf, size_t buf_size, SANE_Status * status)
{
  unsigned char *cmd;

  cmd = alloca (6 + buf_size);
  memset (cmd, 0, 6);
  cmd[0] = WRITE_6_COMMAND;
  cmd[2] = buf_size >> 16;
  cmd[3] = buf_size >> 8;
  cmd[4] = buf_size;
  memcpy (cmd + 6, buf, buf_size);

  if (SANE_STATUS_GOOD == (*status = sanei_scsi_cmd (fd, cmd, 6 + buf_size, NULL, NULL)))
    return buf_size;

  return 0;
}

static int 
send (Epson_Scanner * s, const void *buf, size_t buf_size, SANE_Status * status)
{

	DBG( 3, "send buf, size = %lu\n", (u_long) buf_size);
/*
{
	size_t k;
	const char * s = buf;

	for( k = 0; k < buf_size; k++) {
		DBG( 3, "buf[%u] %02x %c\n", k, s[ k], isprint( s[ k]) ? s[ k] : '.');
	}
}
*/
  if (s->hw->is_scsi)
    {
      return scsi_write (s->fd, buf, buf_size, status);
    }
  else
    {
      int n;

      if (buf_size == (n = sanei_pio_write (s->fd, buf, buf_size)))
	*status = SANE_STATUS_GOOD;
      else
	*status = SANE_STATUS_INVAL;

      return n;
    }

  /* never reached */
}

static int 
scsi_read (int fd, void *buf, size_t buf_size, SANE_Status * status)
{
  unsigned char cmd[6];

  memset (cmd, 0, 6);
  cmd[0] = READ_6_COMMAND;
  cmd[2] = buf_size >> 16;
  cmd[3] = buf_size >> 8;
  cmd[4] = buf_size;

  if (SANE_STATUS_GOOD == (*status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), buf, &buf_size)))
    return buf_size;

  return 0;
}

static int 
receive (Epson_Scanner * s, void *buf, size_t buf_size, SANE_Status * status)
{
  int n;

  if (s->hw->is_scsi)
    {
       n = scsi_read (s->fd, buf, buf_size, status);
    }
  else
    {

      if (buf_size == (n = sanei_pio_read (s->fd, buf, buf_size)))
	*status = SANE_STATUS_GOOD;
      else
	*status = SANE_STATUS_INVAL;

    }

  DBG(3, "receive buf, expected = %lu, got = %d\n", (u_long) buf_size, n);
/*
{
	int k;
	const char * s = buf;

	for( k = 0; k < n; k++) {
		DBG( 3, "buf[%u] %02x %c\n", k, s[ k], isprint( s[ k]) ? s[ k] : '.');
 	}
}
*/
      return n;
}

static SANE_Status
inquiry (int fd, int page_code, void *buf, size_t * buf_size)
{
  unsigned char cmd[6];
  int status;

  memset (cmd, 0, 6);
  cmd[0] = INQUIRY_COMMAND;
  cmd[2] = page_code;
  cmd[4] = *buf_size > 255 ? 255 : *buf_size;
  status = sanei_scsi_cmd (fd, cmd, sizeof cmd, buf, buf_size);
  return status;
}

static SANE_Status
expect_ack (Epson_Scanner * s)
{
  unsigned char result[1];
  size_t len;
  SANE_Status status;

  len = sizeof result;

  receive (s, result, len, &status);

  if (status != SANE_STATUS_GOOD)
    return status;

  if (result[0] != ACK)
    return SANE_STATUS_INVAL;

  return SANE_STATUS_GOOD;
}

static SANE_Status
set_mode (Epson_Scanner * s, int mode)
{
  SANE_Status status;
  unsigned char params[1];

  if (!s->hw->cmd->C)
    return SANE_STATUS_GOOD;

  send (s, "\033C", 2, &status);
  status = expect_ack (s);
  if (status != SANE_STATUS_GOOD)
    return status;
  params[0] = mode;
  send (s, params, 1, &status);
  status = expect_ack (s);
  return status;
}

static SANE_Status
set_resolution (Epson_Scanner * s, int xres, int yres)
{
  SANE_Status status;
  unsigned char params[4];

  if (!s->hw->cmd->R)
    return SANE_STATUS_GOOD;

  send (s, "\033R", 2, &status);
  status = expect_ack (s);
  if (status != SANE_STATUS_GOOD)
    return status;
  params[0] = xres;
  params[1] = xres >> 8;
  params[2] = yres;
  params[3] = yres >> 8;
  send (s, params, 4, &status);
  status = expect_ack (s);
  return status;
}

static SANE_Status
set_area (Epson_Scanner * s, int x, int y, int width, int height)
{
  SANE_Status status;
  unsigned char params[8];

  if (!s->hw->cmd->A)
    return SANE_STATUS_GOOD;

  send (s, "\033A", 2, &status);
  status = expect_ack (s);
  if (status != SANE_STATUS_GOOD)
    return status;
  params[0] = x;
  params[1] = x >> 8;
  params[2] = y;
  params[3] = y >> 8;
  params[4] = width;
  params[5] = width >> 8;
  params[6] = height;
  params[7] = height >> 8;
  send (s, params, 8, &status);
  status = expect_ack (s);
  return status;
}

static SANE_Status
set_depth (Epson_Scanner * s, int depth)
{
  SANE_Status status;
  unsigned char params[1];

  if (!s->hw->cmd->D)
    return SANE_STATUS_GOOD;

  send (s, "\033D", 2, &status);
  status = expect_ack (s);
  if (status != SANE_STATUS_GOOD)
    return status;
  params[0] = depth;
  send (s, params, 1, &status);
  status = expect_ack (s);
  return status;
}

static SANE_Status
set_halftone (Epson_Scanner * s, int halftone)
{
  SANE_Status status;
  unsigned char params[1];

  if (!s->hw->cmd->B)
    return SANE_STATUS_GOOD;

  send (s, "\033B", 2, &status);
  status = expect_ack (s);
  if (status != SANE_STATUS_GOOD)
    return status;
  params[0] = halftone;
  send (s, params, 1, &status);
  status = expect_ack (s);
  return status;
}

static SANE_Status
set_gamma (Epson_Scanner * s, int gamma)
{
  SANE_Status status;
  unsigned char params[1];

  if (!s->hw->cmd->Z)
    return SANE_STATUS_GOOD;

  send (s, "\033Z", 2, &status);
  status = expect_ack (s);
  if (status != SANE_STATUS_GOOD)
    return status;
  params[0] = gamma;
  send (s, params, 1, &status);
  status = expect_ack (s);
  return status;
}

static SANE_Status
set_color (Epson_Scanner * s, int color)
{
  SANE_Status status;
  unsigned char params[1];

  if (!s->hw->cmd->M)
    return SANE_STATUS_GOOD;

  send (s, "\033M", 2, &status);
  status = expect_ack (s);
  if (status != SANE_STATUS_GOOD)
    return status;
  params[0] = color;
  send (s, params, 1, &status);
  status = expect_ack (s);
  return status;
}

static SANE_Status
set_speed (Epson_Scanner * s, int flags)
{
  SANE_Status status;
  unsigned char params[1];

  if (!s->hw->cmd->g)
    return SANE_STATUS_GOOD;

  send (s, "\033g", 2, &status);
  status = expect_ack (s);
  if (status != SANE_STATUS_GOOD)
    return status;
  params[0] = flags;
  send (s, params, 1, &status);
  status = expect_ack (s);
  return status;
}

static SANE_Status
set_lcount (Epson_Scanner * s, int lcount)
{
  SANE_Status status;
  unsigned char params[1];

  if (!s->hw->cmd->d)
    return SANE_STATUS_GOOD;

  send (s, "\033d", 2, &status);
  status = expect_ack (s);
  if (status != SANE_STATUS_GOOD)
    return status;
  params[0] = lcount;
  send (s, params, 1, &status);
  status = expect_ack (s);
  return status;
}

static SANE_Status
set_brightness (Epson_Scanner * s, int brightness)
{
  SANE_Status status;
  unsigned char params[1];

  if (!s->hw->cmd->L)
    return SANE_STATUS_GOOD;

  send (s, "\033L", 2, &status);
  status = expect_ack (s);
  if (status != SANE_STATUS_GOOD)
    return status;
  params[0] = brightness;
  send (s, params, 1, &status);
  status = expect_ack (s);
  return status;
}

static SANE_Status
reset (Epson_Scanner * s)
{
  SANE_Status status;

  if (!s->hw->cmd->AT)
    return SANE_STATUS_GOOD;

  send (s, "\033@", 2, &status);
  status = expect_ack (s);
  return status;
}

static SANE_Status
identify (Epson_Scanner * s, struct Epson_Device *dev)
{
  unsigned char result[4];
  size_t len;
  SANE_Status status;
  unsigned char *buf;

  if (!s->hw->cmd->I)
    return SANE_STATUS_INVAL;

  send (s, "\033I", 2, &status);
  if (status != SANE_STATUS_GOOD)
    return status;
  len = 4;
  receive (s, result, len, &status);
  if (status != SANE_STATUS_GOOD)
    return status;
  if (result[0] != STX)
    return SANE_STATUS_INVAL;

  len = result[3] << 8 | result[2];
  buf = alloca (len);
  receive (s, buf, len, &status);
  if (buf[2] != 'R' || buf[len - 5] != 'A')
    return SANE_STATUS_INVAL;

  if (buf[0] != 'B')
    DBG (1, "Unknown type %c\n", buf[0]);
  dev->level = buf[1] - '0';
  DBG (2, "Command level is %d\n", dev->level);

  {
    int n;

    for (n = 0; n < NELEMS (epson_cmd); n++)
      if (!strncmp (&buf[0], epson_cmd[n].level, 2))
	break;

    if (n < NELEMS (epson_cmd))
      {
	dev->cmd = &epson_cmd[n];
      }
    else
      {
	dev->cmd = &epson_cmd[EPSON_LEVEL_DEFAULT];
	DBG (1, "Unknown type %c or level %c, using %s\n", buf[0], buf[1], dev->cmd->level);
      }
  }

  dev->dpi_range.min = buf[4] << 8 | buf[3];
  dev->dpi_range.max = buf[len - 6] << 8 | buf[len - 7];
  dev->dpi_range.quant = 0;
  dev->x_range.min = 0;
  dev->x_range.max = SANE_FIX ((buf[len - 3] << 8 | buf[len - 4]) * 25.4 / dev->dpi_range.max);
  dev->x_range.quant = 0;
  dev->y_range.min = 0;
  dev->y_range.max = SANE_FIX ((buf[len - 1] << 8 | buf[len - 2]) * 25.4 / dev->dpi_range.max);
  dev->y_range.quant = 0;
  return SANE_STATUS_GOOD;
}

static void 
myclose (Epson_Scanner * s)
{

  if (s->hw->is_scsi)
    sanei_scsi_close (s->fd);
  else
    sanei_pio_close (s->fd);

  return;
}

static Epson_Device dummy_dev =
{
  {
    NULL,
    "Epson",
    NULL,
    "flatbed scanner"
  }
};

#if 1

#pragma	pack(1)

typedef struct
  {
    u_char code;
    u_char status;
    u_short count;

    u_char buf[1];

  }
EpsonHdrRec, *EpsonHdr;

typedef struct
  {
    u_char code;
    u_char status;
    u_short count;

    u_char type;
    u_char level;

    u_char buf[1];

  }
EpsonIdentRec, *EpsonIdent;

#pragma	pack()

#endif

#if 1
static EpsonHdr 
command (Epson_Scanner * s, const u_char * cmd, size_t cmd_size, SANE_Status * status)
{
  EpsonHdr head;
  u_char *buf;

  if (NULL == (head = walloc (EpsonHdrRec)))
    {
      *status = SANE_STATUS_NO_MEM;
      return (EpsonHdr) 0;
    }

  send (s, cmd, cmd_size, status);

  if (SANE_STATUS_GOOD != *status)
    return (EpsonHdr) 0;

  buf = (u_char *) head;
  if (s->hw->is_scsi)
    receive (s, buf, 4, status);
  else
    receive (s, buf, 1, status);


  if (SANE_STATUS_GOOD != *status)
    return (EpsonHdr) 0;

  buf++;

  DBG (4, "code   %02x\n", (int) head->code);

  switch (head->code)
    {
    default:
      if (0 == head->code)
	DBG (1, "Incompatible printer port (probably bi/directional)\n");
      else if (cmd[cmd_size - 1] == head->code)
	DBG (1, "Incompatible printer port (probably not bi/directional)\n");

      DBG (2, "Illegal response of scanner for command: %02x\n", head->code);
      break;

    case NAK:
      break;

    case ACK:
      break;

    case STX:
      if (!s->hw->is_scsi)
        {
      	  receive (s, buf, 3, status);
/*	  buf += 3;*/
	}

      if (SANE_STATUS_GOOD != *status)
	return (EpsonHdr) 0;

      DBG (4, "status %02x\n", (int) head->status);
      DBG (4, "count  %d\n", (int) head->count);

      if (NULL == (head = realloc (head, sizeof (EpsonHdrRec) + head->count)))
	{
	  *status = SANE_STATUS_NO_MEM;
	  return (EpsonHdr) 0;
	}

      buf = head->buf;
      receive (s, buf, head->count, status);

      if (SANE_STATUS_GOOD != *status)
	return (EpsonHdr) 0;

/*              buf += head->count; */

/*              sanei_hexdmp( head, sizeof( EpsonHdrRec) + head->count, "epson"); */

      break;
    }


  return head;
}
#endif

static SANE_Status 
attach (const char *dev_name, Epson_Device * *devp)
{
  SANE_Status status;
  Epson_Scanner *s = walloca (Epson_Scanner);

  unsigned char buf[36];
  size_t buf_size;
  char *str;

  s->hw = &dummy_dev;
  s->hw->cmd = &epson_cmd[EPSON_LEVEL_DEFAULT];

  DBG (3, "attach: opening %s\n", dev_name);

  {
    char *end;

    strtol (dev_name, &end, 0);

    if ((end == dev_name) || *end)
      {
	s->hw->is_scsi = 1;
      }
    else
      {
	s->hw->is_scsi = 0;
      }
  }

  if (s->hw->is_scsi)
    {
      if (SANE_STATUS_GOOD != (status = sanei_scsi_open (dev_name, &s->fd, NULL, NULL)))
	{
	  DBG (1, "attach: open failed: %s\n", sane_strstatus (status));
	  return status;
	}

      DBG (3, "attach: sending INQUIRY\n");
      buf_size = sizeof buf;

      if (SANE_STATUS_GOOD != (status = inquiry (s->fd, 0, buf, &buf_size)))
	{
	  DBG (1, "attach: inquiry failed: %s\n", sane_strstatus (status));
	  myclose (s);
	  return status;
	}

      if (buf[0] != TYPE_PROCESSOR
	  || strncmp (buf + 8, "EPSON", 5) != 0
	  || (strncmp (buf + 16, "SCANNER ", 8) != 0
	      && strncmp (buf + 14, "SCANNER ", 8) != 0
	      && strncmp (buf + 16, "Perfection", 10) != 0))
	{
	  DBG (1, "attach: device doesn't look like an Epson scanner\n");
	  myclose (s);
	  return SANE_STATUS_INVAL;
	}
    }
  else
    {
      if (SANE_STATUS_GOOD != (status = sanei_pio_open (dev_name, &s->fd)))
	{
	  DBG (1, "dev_open: %s: can't open %s as a parallel-port device\n", sane_strstatus (status), dev_name);
	  return status;
	}
    }

#if 1
  /* EpsonHdr head = command( s, "\033@", 2, &status); */

  {
    EpsonIdent ident = (EpsonIdent) command (s, "\033I", 2, &status);
    u_char *buf;
    int n, k;

	if( NULL == ident) {
		DBG (0, "ident failed\n");
		return status;
	}

    DBG (1, "type  %3c 0x%02x\n", ident->type, ident->type);
    DBG (1, "level %3c 0x%02x\n", ident->level, ident->level);

    s->hw->res_list_size = 0;
    s->hw->res_list = (SANE_Int *) calloc (s->hw->res_list_size, sizeof (SANE_Int));

    if (NULL == s->hw->res_list)
      {
	DBG (0, "no mem\n");
	exit (0);
      }

    for (n = ident->count, buf = ident->buf; n; n -= k, buf += k)
      {
	switch (*buf)
	  {
	  case 'R':
	    {
	      int val = buf[2] << 8 | buf[1];

	      s->hw->res_list_size++;
	      s->hw->res_list = (SANE_Int *) realloc (s->hw->res_list, s->hw->res_list_size * sizeof (SANE_Int));

	      if (NULL == s->hw->res_list)
		{
		  DBG (0, "no mem\n");
		  exit (0);
		}

	      s->hw->res_list[s->hw->res_list_size - 1] = (SANE_Int) val;

	      DBG (1, "resolution (dpi): %d\n", val);
	      k = 3;
	      continue;
	    }
	  case 'A':
	    {
	      int x = buf[2] << 8 | buf[1];
	      int y = buf[4] << 8 | buf[3];

	      DBG (1, "maximum scan area: %dx%d\n", x, y);
	      k = 5;
	      continue;
	    }
	  default:
	    break;
	  }

	break;
      }

  }
#endif

  status = identify (s, &dummy_dev);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "attach: device doesn't look like an Epson scanner\n");
      myclose (s);
      return SANE_STATUS_INVAL;
    }

  myclose (s);

  str = malloc (strlen (dev_name) + 1);
  dummy_dev.sane.name = strcpy (str, dev_name);

  str = malloc (8 + 1);
  str[8] = '\0';
  dummy_dev.sane.model = (char *) memcpy (str, buf + 16 + 8, 8);

  return SANE_STATUS_GOOD;
}

static SANE_Status
attach_one (const char *dev)
{
  attach (dev, 0);
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_init (SANE_Int * version_code, SANE_Auth_Callback authorize)
{
  char dev_name[PATH_MAX] = "/dev/scanner";
  size_t len;
  FILE *fp;

  DBG_INIT ();
#if defined PACKAGE && defined VERSION
  DBG (2, "sane_init: " PACKAGE " " VERSION "\n");
#endif

  if (version_code != NULL)
    *version_code = SANE_VERSION_CODE (V_MAJOR, V_MINOR, 0);

  /* default to /dev/scanner instead of insisting on config file */
  if ((fp = sanei_config_open (EPSON_CONFIG_FILE)))
    {
      char line[PATH_MAX];

      while (fgets (line, sizeof (line), fp))
	{
	  DBG (4, "sane_init, >%s<\n", line);
	  if (line[0] == '#')		/* ignore line comments */
	    continue;
	  len = strlen (line);
	  if (line[len - 1] == '\n')
            line[--len] = '\0';
	  if (!len)
            continue;			/* ignore empty lines */
	  DBG (4, "sane_init, >%s<\n", line);
	  strcpy (dev_name, line);
	}
      fclose (fp);
    }
  sanei_config_attach_matching_devices (dev_name, attach_one);
  return SANE_STATUS_GOOD;
}

void
sane_exit (void)
{
  free ((char *) dummy_dev.sane.model);
  free ((char *) dummy_dev.sane.name);
}

SANE_Status
sane_get_devices (const SANE_Device *** device_list, SANE_Bool local_only)
{
  static const SANE_Device *devlist[2];
  int i;

  i = 0;
  if (dummy_dev.sane.name != NULL)
    devlist[i++] = &dummy_dev.sane;
  devlist[i] = NULL;

  *device_list = devlist;
  return SANE_STATUS_GOOD;
}

struct mode_param
{
  int color;
  int mode_flags;
  int dropout_mask;
  int depth;
};

static const struct mode_param mode_params[] =
{
  {0, 0x00, 0x30, 1},
  {0, 0x00, 0x30, 8},
  {1, 0x02, 0x00, 8},
};

static const SANE_String_Const mode_list[] =
{
  "Binary",
  "Gray",
  "Color",
  NULL
};

static int halftone_params[] =
{
  0x01,
  0x00,
  0x10,
  0x20,
  0x80,
  0x90,
  0xa0,
  0xb0
};

static const SANE_String_Const halftone_list[] =
{
  "None",
  "Halftone A",
  "Halftone B",
  "Halftone C",
  NULL
};

static const SANE_String_Const halftone_list_4[] =
{
  "None",
  "Halftone A",
  "Halftone B",
  "Halftone C",
  "Dither A",
  "Dither B",
  "Dither C",
  "Dither D",
  NULL
};

static int dropout_params[] =
{
  0x00,
  0x10,
  0x20,
  0x30,
};

static const SANE_String_Const dropout_list[] =
{
  "None",
  "Red",
  "Green",
  "Blue",
  NULL
};

static int brightness_params[] =
{3, 2, 1, 0, -1, -2, -3};

static const SANE_String_Const brightness_list[] =
{
  "Very light"
  ,"Lighter"
  ,"Light"
  ,"Normal"
  ,"Dark"
  ,"Darker"
  ,"Very dark"
  ,NULL
};

static SANE_Status
init_options (Epson_Scanner * s)
{
  int i;

  for (i = 0; i < NUM_OPTIONS; ++i)
    {
      s->opt[i].size = sizeof (SANE_Word);
      s->opt[i].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    }

  s->opt[OPT_NUM_OPTS].title = SANE_TITLE_NUM_OPTIONS;
  s->opt[OPT_NUM_OPTS].desc = SANE_DESC_NUM_OPTIONS;
  s->opt[OPT_NUM_OPTS].cap = SANE_CAP_SOFT_DETECT;
  s->val[OPT_NUM_OPTS] = NUM_OPTIONS;

  /* "Scan Mode" group: */

  s->opt[OPT_MODE_GROUP].title = "Scan Mode";
  s->opt[OPT_MODE_GROUP].desc = "";
  s->opt[OPT_MODE_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_MODE_GROUP].cap = 0;

  /* scan mode */
  s->opt[OPT_MODE].name = SANE_NAME_SCAN_MODE;
  s->opt[OPT_MODE].title = SANE_TITLE_SCAN_MODE;
  s->opt[OPT_MODE].desc = SANE_DESC_SCAN_MODE;
  s->opt[OPT_MODE].type = SANE_TYPE_STRING;
  s->opt[OPT_MODE].size = 32;
  s->opt[OPT_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_MODE].constraint.string_list = mode_list;
  s->val[OPT_MODE] = 0;		/* Binary */

  /* halftone */
  s->opt[OPT_HALFTONE].name = SANE_NAME_HALFTONE;
  s->opt[OPT_HALFTONE].title = SANE_TITLE_HALFTONE;
  s->opt[OPT_HALFTONE].desc = "Selects the halftone.";
  s->opt[OPT_HALFTONE].type = SANE_TYPE_STRING;
  s->opt[OPT_HALFTONE].size = 32;
  s->opt[OPT_HALFTONE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  if (s->hw->level >= 4)
    s->opt[OPT_HALFTONE].constraint.string_list = halftone_list_4;
  else
    s->opt[OPT_HALFTONE].constraint.string_list = halftone_list;
  s->val[OPT_HALFTONE] = 1;	/* Halftone A */

  /* dropout */
  s->opt[OPT_DROPOUT].name = "dropout";
  s->opt[OPT_DROPOUT].title = "Dropout";
  s->opt[OPT_DROPOUT].desc = "Selects the dropout.";
  s->opt[OPT_DROPOUT].type = SANE_TYPE_STRING;
  s->opt[OPT_DROPOUT].size = 32;
  s->opt[OPT_DROPOUT].cap |= SANE_CAP_ADVANCED;
  s->opt[OPT_DROPOUT].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_DROPOUT].constraint.string_list = dropout_list;
  s->val[OPT_DROPOUT] = 0;	/* None */

  /* brightness */
  s->opt[OPT_BRIGHTNESS].name = SANE_NAME_BRIGHTNESS;
  s->opt[OPT_BRIGHTNESS].title = SANE_TITLE_BRIGHTNESS;
  s->opt[OPT_BRIGHTNESS].desc = "Selects the brightness.";
  s->opt[OPT_BRIGHTNESS].type = SANE_TYPE_STRING;
  s->opt[OPT_BRIGHTNESS].size = 32;
  s->opt[OPT_BRIGHTNESS].cap |= SANE_CAP_ADVANCED;
  s->opt[OPT_BRIGHTNESS].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_BRIGHTNESS].constraint.string_list = brightness_list;
  s->val[OPT_BRIGHTNESS] = 3;	/* Normal */

  /* resolution */
  s->opt[OPT_RESOLUTION].name = SANE_NAME_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].type = SANE_TYPE_INT;
  s->opt[OPT_RESOLUTION].unit = SANE_UNIT_DPI;

  s->opt[OPT_RESOLUTION].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_RESOLUTION].constraint.range = &s->hw->dpi_range;
  s->val[OPT_RESOLUTION] = s->hw->dpi_range.min;

  /* "Geometry" group: */

  s->opt[OPT_GEOMETRY_GROUP].title = "Geometry";
  s->opt[OPT_GEOMETRY_GROUP].desc = "";
  s->opt[OPT_GEOMETRY_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_GEOMETRY_GROUP].cap = SANE_CAP_ADVANCED;

  /* top-left x */
  s->opt[OPT_TL_X].name = SANE_NAME_SCAN_TL_X;
  s->opt[OPT_TL_X].title = SANE_TITLE_SCAN_TL_X;
  s->opt[OPT_TL_X].desc = SANE_DESC_SCAN_TL_X;
  s->opt[OPT_TL_X].type = SANE_TYPE_FIXED;
  s->opt[OPT_TL_X].unit = SANE_UNIT_MM;
  s->opt[OPT_TL_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TL_X].constraint.range = &s->hw->x_range;
  s->val[OPT_TL_X] = 0;

  /* top-left y */
  s->opt[OPT_TL_Y].name = SANE_NAME_SCAN_TL_Y;
  s->opt[OPT_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
  s->opt[OPT_TL_Y].desc = SANE_DESC_SCAN_TL_Y;
  s->opt[OPT_TL_Y].type = SANE_TYPE_FIXED;
  s->opt[OPT_TL_Y].unit = SANE_UNIT_MM;
  s->opt[OPT_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TL_Y].constraint.range = &s->hw->y_range;
  s->val[OPT_TL_Y] = 0;

  /* bottom-right x */
  s->opt[OPT_BR_X].name = SANE_NAME_SCAN_BR_X;
  s->opt[OPT_BR_X].title = SANE_TITLE_SCAN_BR_X;
  s->opt[OPT_BR_X].desc = SANE_DESC_SCAN_BR_X;
  s->opt[OPT_BR_X].type = SANE_TYPE_FIXED;
  s->opt[OPT_BR_X].unit = SANE_UNIT_MM;
  s->opt[OPT_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_X].constraint.range = &s->hw->x_range;
  s->val[OPT_BR_X] = s->hw->x_range.max;

  /* bottom-right y */
  s->opt[OPT_BR_Y].name = SANE_NAME_SCAN_BR_Y;
  s->opt[OPT_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
  s->opt[OPT_BR_Y].desc = SANE_DESC_SCAN_BR_Y;
  s->opt[OPT_BR_Y].type = SANE_TYPE_FIXED;
  s->opt[OPT_BR_Y].unit = SANE_UNIT_MM;
  s->opt[OPT_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_Y].constraint.range = &s->hw->y_range;
  s->val[OPT_BR_Y] = s->hw->y_range.max;

  return SANE_STATUS_GOOD;
}

SANE_Status
sane_open (SANE_String_Const devicename, SANE_Handle * handle)
{
  Epson_Scanner *s;

  if (devicename[0] == '\0')
    devicename = dummy_dev.sane.name;

  if (!devicename || devicename[0] == '\0'
      || strcmp (devicename, dummy_dev.sane.name) != 0)
    return SANE_STATUS_INVAL;

  s = calloc (1, sizeof (Epson_Scanner));
  if (s == NULL)
    return SANE_STATUS_NO_MEM;

  s->fd = -1;
  s->hw = &dummy_dev;

  init_options (s);

  *handle = (SANE_Handle) s;
  return SANE_STATUS_GOOD;
}

void
sane_close (SANE_Handle handle)
{
  Epson_Scanner *s = (Epson_Scanner *) handle;

  free (s->buf);

  if (s->fd != -1)
    sanei_scsi_close (s->fd);

  free (s);
}

const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
  Epson_Scanner *s = (Epson_Scanner *) handle;

  if (option < 0 || option >= NUM_OPTIONS)
    return NULL;

  return s->opt + option;
}

static const SANE_String_Const *
search_string_list (const SANE_String_Const * list, SANE_String value)
{
  while (*list != NULL && strcmp (value, *list) != 0)
    ++list;

  if (*list == NULL)
    return NULL;

  return list;
}

SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option,
		     SANE_Action action, void *value,
		     SANE_Int * info)
{
  Epson_Scanner *s = (Epson_Scanner *) handle;
  SANE_Status status;
  const SANE_String_Const *optval;

  if (option < 0 || option >= NUM_OPTIONS)
    return SANE_STATUS_INVAL;

  if (info != NULL)
    *info = 0;

  switch (action)
    {
    case SANE_ACTION_GET_VALUE:
      switch (option)
	{
	case OPT_NUM_OPTS:
	case OPT_RESOLUTION:
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
	  *(SANE_Word *) value = s->val[option];
	  break;
	case OPT_MODE:
	case OPT_HALFTONE:
	case OPT_DROPOUT:
	case OPT_BRIGHTNESS:
	  strcpy ((char *) value,
		  s->opt[option].constraint.string_list[s->val[option]]);
	  break;
	default:
	  return SANE_STATUS_INVAL;
	}
      break;
    case SANE_ACTION_SET_VALUE:
      status = sanei_constrain_value (s->opt + option, value, info);
      if (status != SANE_STATUS_GOOD)
	return status;

      optval = NULL;
      if (s->opt[option].constraint_type == SANE_CONSTRAINT_STRING_LIST)
	{
	  optval = search_string_list (s->opt[option].constraint.string_list,
				       (char *) value);
	  if (optval == NULL)
	    return SANE_STATUS_INVAL;
	}

      switch (option)
	{
	case OPT_RESOLUTION:
	  {
	    int n;
	    int min_d = s->hw->res_list[s->hw->res_list_size - 1];
	    int v = *(SANE_Word *) value;
	    int best = v;

	    for (n = 0; n < s->hw->res_list_size; n++)
	      {
		int d = abs (v - s->hw->res_list[n]);

		if (d < min_d)
		  {
		    min_d = d;
		    best = s->hw->res_list[n];
		  }
	      }

	    s->val[option] = (SANE_Word) best;

	    break;
	  }
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
	  s->val[option] = *(SANE_Word *) value;
	  if (info != NULL)
	    *info |= SANE_INFO_RELOAD_PARAMS;
	  break;
	case OPT_MODE:
	  if (mode_params[optval - mode_list].depth != 1)
	    s->opt[OPT_HALFTONE].cap |= SANE_CAP_INACTIVE;
	  else
	    s->opt[OPT_HALFTONE].cap &= ~SANE_CAP_INACTIVE;
	  if (mode_params[optval - mode_list].color)
	    s->opt[OPT_DROPOUT].cap |= SANE_CAP_INACTIVE;
	  else
	    s->opt[OPT_DROPOUT].cap &= ~SANE_CAP_INACTIVE;
	  if (info != NULL)
	    *info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;
	  /* fall through */
	case OPT_HALFTONE:
	case OPT_DROPOUT:
	case OPT_BRIGHTNESS:
	  s->val[option] = optval - s->opt[option].constraint.string_list;
	  break;
	default:
	  return SANE_STATUS_INVAL;
	}
      break;
    default:
      return SANE_STATUS_INVAL;
    }

  return SANE_STATUS_GOOD;
}

SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters * params)
{
  Epson_Scanner *s = (Epson_Scanner *) handle;
  int ndpi;

  memset (&s->params, 0, sizeof (SANE_Parameters));

  ndpi = s->val[OPT_RESOLUTION];

  s->params.pixels_per_line = SANE_UNFIX (s->val[OPT_BR_X] - s->val[OPT_TL_X]) / 25.4 * ndpi;
  s->params.lines = SANE_UNFIX (s->val[OPT_BR_Y] - s->val[OPT_TL_Y]) / 25.4 * ndpi;
  /* pixels_per_line seems to be 8 * n.  */
  s->params.pixels_per_line = s->params.pixels_per_line & ~7;

  s->params.last_frame = SANE_TRUE;
  s->params.depth = mode_params[s->val[OPT_MODE]].depth;
  if (mode_params[s->val[OPT_MODE]].color)
    {
      s->params.format = SANE_FRAME_RGB;
      s->params.bytes_per_line = 3 * s->params.pixels_per_line;
    }
  else
    {
      s->params.format = SANE_FRAME_GRAY;
      s->params.bytes_per_line = s->params.pixels_per_line * s->params.depth / 8;
    }

  if (params != NULL)
    *params = s->params;

  return SANE_STATUS_GOOD;
}

SANE_Status
sane_start (SANE_Handle handle)
{
  Epson_Scanner *s = (Epson_Scanner *) handle;
  SANE_Status status;
  const struct mode_param *mparam;
  int ndpi;
  int left, top;
  int lcount;

  status = sane_get_parameters (handle, NULL);
  if (status != SANE_STATUS_GOOD)
    return status;

  if (s->hw->is_scsi)
    {
      if (SANE_STATUS_GOOD != (status = sanei_scsi_open (s->hw->sane.name, &s->fd, NULL, NULL)))
	{
	  DBG (1, "sane_start: %s open failed: %s\n", s->hw->sane.name, sane_strstatus (status));
	  return status;
	}
    }
  else
    {
      if (SANE_STATUS_GOOD != (status = sanei_pio_open (s->hw->sane.name, &s->fd)))
	{
	  DBG (1, "sane_start: %s open failed: %s\n", s->hw->sane.name, sane_strstatus (status));
	  return status;
	}
    }

  reset (s);

  mparam = mode_params + s->val[OPT_MODE];
  status = set_depth (s, s->params.depth);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "sane_start: set_depth failed: %s\n", sane_strstatus (status));
      return status;
    }
  if (s->hw->level >= 5 && mparam->mode_flags == 0x02)
    status = set_mode (s, 0x13);
  else
    status = set_mode (s,
		       mparam->mode_flags
		       | (mparam->dropout_mask
			  & dropout_params[s->val[OPT_DROPOUT]]));
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "sane_start: set_mode failed: %s\n", sane_strstatus (status));
      return status;
    }
  status = set_halftone (s, halftone_params[s->val[OPT_HALFTONE]]);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "sane_start: set_halftone failed: %s\n", sane_strstatus (status));
      return status;
    }
  status = set_brightness (s, brightness_params[s->val[OPT_BRIGHTNESS]]);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "sane_start: set_brightness failed: %s\n", sane_strstatus (status));
      return status;
    }
  status = set_gamma (s, s->params.depth == 1 ? 1 : 2);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "sane_start: set_gamma failed: %s\n", sane_strstatus (status));
      return status;
    }
  status = set_color (s, 0x80);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "sane_start: set_color failed: %s\n", sane_strstatus (status));
      return status;
    }
  status = set_speed (s, mode_params[s->val[OPT_MODE]].depth == 1 ? 1 : 0);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "sane_start: set_speed failed: %s\n", sane_strstatus (status));
      return status;
    }
  ndpi = s->val[OPT_RESOLUTION];
  status = set_resolution (s, ndpi, ndpi);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "sane_start: set_resolution failed: %s\n", sane_strstatus (status));
      return status;
    }
  left = SANE_UNFIX (s->val[OPT_TL_X]) / 25.4 * ndpi + 0.5;
  top = SANE_UNFIX (s->val[OPT_TL_Y]) / 25.4 * ndpi + 0.5;
  status = set_area (s, left, top, s->params.pixels_per_line,
		     s->params.lines);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "sane_start: set_area failed: %s\n", sane_strstatus (status));
      return status;
    }

  s->block = SANE_FALSE;
  lcount = 1;
  if (s->hw->level >= 5
      || (s->hw->level >= 4 && !mode_params[s->val[OPT_MODE]].color))
    {
      s->block = SANE_TRUE;
      lcount = sanei_scsi_max_request_size / s->params.bytes_per_line;
      if (lcount > 255)
	lcount = 255;
      if (lcount == 0)
	return SANE_STATUS_NO_MEM;
      status = set_lcount (s, lcount);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (1, "sane_start: set_lcount failed: %s\n", sane_strstatus (status));
	  return status;
	}
    }

  send (s, "\033G", 2, &status);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "sane_start: start failed: %s\n", sane_strstatus (status));
      return status;
    }

  s->eof = SANE_FALSE;
  s->buf = realloc (s->buf, lcount * s->params.bytes_per_line);
  s->ptr = s->end = s->buf;
  s->canceling = SANE_FALSE;
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_read (SANE_Handle handle, SANE_Byte * data,
	   SANE_Int max_length, SANE_Int * length)
{
  Epson_Scanner *s = (Epson_Scanner *) handle;
  SANE_Status status;

  DBG (5, "sane_read: beginn\n");

  if (s->ptr == s->end)
    {
      unsigned char result[6];
      size_t len;
      size_t buf_len;


      if (s->eof)
	{
	  free (s->buf);
	  s->buf = NULL;
	  myclose (s);
	  s->fd = -1;
	  *length = 0;

	  return SANE_STATUS_EOF;
	}

      DBG (5, "sane_read: beginn scan\n");

      len = s->block ? 6 : 4;
      receive (s, result, len, &status);

      buf_len = result[3] << 8 | result[2];

      DBG (5, "sane_read: buf len = %lu\n", (u_long) buf_len);

      if (s->block)
	buf_len *= (result[5] << 8 | result[4]);

      DBG (5, "sane_read: buf len = %lu\n", (u_long) buf_len);

      if (!s->block && SANE_FRAME_RGB == s->params.format)
	{

	  receive (s, s->buf + s->params.pixels_per_line, buf_len, &status);
	  send (s, "\006", 1, &status);
      	  len = s->block ? 6 : 4;
	  receive (s, result, len, &status);
	  buf_len = result[3] << 8 | result[2];
      	  if (s->block)
	    buf_len *= (result[5] << 8 | result[4]);

	  DBG (5, "sane_read: buf len2 = %lu\n", (u_long) buf_len);

	  receive (s, s->buf, buf_len, &status);
	  send (s, "\006", 1, &status);
      	  len = s->block ? 6 : 4;
	  receive (s, result, len, &status);
	  buf_len = result[3] << 8 | result[2];
      	  if (s->block)
	    buf_len *= (result[5] << 8 | result[4]);

	  DBG (5, "sane_read: buf len3 = %lu\n", (u_long) buf_len);

	  receive (s, s->buf + 2 * s->params.pixels_per_line, buf_len, &status);
	}
      else
	{
	  receive (s, s->buf, buf_len, &status);
	}

      if (result[1] & 0x20)
	s->eof = SANE_TRUE;
      else
	{
	  if (s->canceling)
	    {
	      send (s, "\030", 1, &status);
	      expect_ack (s);
	      free (s->buf);
	      s->buf = NULL;
	      myclose (s);
	      s->fd = -1;
	      *length = 0;
	      return SANE_STATUS_CANCELLED;
	    }
	  else
	    send (s, "\006", 1, &status);
	}

      s->end = s->buf + buf_len;
      s->ptr = s->buf;

      DBG (5, "sane_read: beginn scan\n");
    }



  if (!s->block && s->params.format == SANE_FRAME_RGB)
    {

      max_length /= 3;
      if (max_length > s->end - s->ptr)
	max_length = s->end - s->ptr;
      *length = 3 * max_length;
      while (max_length-- != 0)
	{
	  *data++ = s->ptr[0];
	  *data++ = s->ptr[s->params.pixels_per_line];
	  *data++ = s->ptr[2 * s->params.pixels_per_line];
	  ++s->ptr;
	}
    }
  else
    {
      if (max_length > s->end - s->ptr)
	max_length = s->end - s->ptr;
      *length = max_length;
      if (s->params.depth == 1)
	{
	  while (max_length-- != 0)
	    *data++ = ~*s->ptr++;
	}
      else
	{
	  memcpy (data, s->ptr, max_length);
	  s->ptr += max_length;
	}
    }

  DBG (5, "sane_read: end\n");

  return SANE_STATUS_GOOD;
}

void
sane_cancel (SANE_Handle handle)
{
  Epson_Scanner *s = (Epson_Scanner *) handle;

  if (s->buf != NULL)
    s->canceling = SANE_TRUE;
}

SANE_Status
sane_set_io_mode (SANE_Handle handle, SANE_Bool non_blocking)
{
  return SANE_STATUS_UNSUPPORTED;
}

SANE_Status
sane_get_select_fd (SANE_Handle handle, SANE_Int * fd)
{
  return SANE_STATUS_UNSUPPORTED;
}
