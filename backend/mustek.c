/* sane - Scanner Access Now Easy.
   Copyright (C) 1996, 1997 David Mosberger-Tang and Andreas Czechanowski,
   1998 Andreas Bolsch for extension to ScanExpress models version 0.6,
   2000 Henning Meier-Geinitz
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

   This file implements a SANE backend for Mustek and some Trust flatbed
   scanners with SCSI or proprietary interface.  */


/**************************************************************************/
/* Mustek backend version                                                 */
#define BUILD 97
/**************************************************************************/

#include "sane/config.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "sane/sane.h"
#include "sane/sanei.h"
#include "sane/saneopts.h"
#include "sane/sanei_scsi.h"
#include "sane/sanei_ab306.h"
#include "mustek.h"

#define BACKEND_NAME	mustek
#include "sane/sanei_backend.h"

#ifndef PATH_MAX
# define PATH_MAX	1024
#endif

#include "sane/sanei_config.h"
#define MUSTEK_CONFIG_FILE "mustek.conf"

#define MM_PER_INCH	25.4
#define MIN(a,b)	((a) < (b) ? (a) : (b))
#define MAX(a,b)	((a) > (b) ? (a) : (b))

/* Maximum time to wait for scanner to become ready.  */
#define MAX_WAITING_TIME	60
/* Some Paragon and the ScanExpress scanners require this for
   line-distance correction */
#define MAX_LINE_DIST		40
/* At least the Paragon 800 II SP doesn't like scanning in large blocks */
#define MAX_BLOCKSIZE           (1024*1024*4)

static unsigned int debug_level;

/* Maximum # of inches to scan in one swoop.  0 means "unlimited."
   This is here to be nice on the SCSI bus---Mustek scanners don't
   disconnect while scanning is in progress, which confuses some
   drivers that expect no reasonable SCSI request would take more than
   10 seconds.  That's not really true for Mustek scanners operating
   in certain modes, hence this limit.  */
static double strip_height;

/* Should we wait for the scan slider to return after each scan? */
static SANE_Bool force_wait;

static int num_devices;
static Mustek_Device *first_dev;
static Mustek_Scanner *first_handle;

static Mustek_Device **new_dev;	/* array of newly attached devices */
static int new_dev_len;		/* length of new_dev array */
static int new_dev_alloced;	/* number of entries alloced for new_dev */

/* Used for line-distance correction: */
static const int color_seq[] =
  {
    1, 2, 0			/* green, blue, red */
  };

static const SANE_String_Const mode_list_paragon[] =
  {
    "Lineart", "Halftone", "Gray", "Color",
    0
  };

static const SANE_String_Const mode_list_pro[] =
  {
    "Lineart", "Gray", "Gray fast", "Color",
    0
  };

static const SANE_String_Const mode_list_se[] =
  {
    "Lineart", "Gray", "Color",
    0
  };

static const SANE_String_Const speed_list[] =
  {
    "Slowest", "Slower", "Normal", "Faster", "Fastest",
    0
  };

static const SANE_String_Const source_list[] =
  {
    "Flatbed", 
    0
  };

static const SANE_String_Const adf_source_list[] =
  {
    "Flatbed", "Automatic Document Feeder",
    0
  };

static const SANE_String_Const ta_source_list[] =
  {
    "Flatbed", "Transparency Adapter",
    0
  };

static const SANE_Range u8_range =
  {
      0,				/* minimum */
    255,				/* maximum */
      0				/* quantization */
  };

static const SANE_String_Const halftone_list[] =
  {
    "8x8 coarse", "8x8 normal", "8x8 fine", "8x8 very fine", "6x6 normal",
    "5x5 coarse", "5x5 fine", "4x4 coarse", "4x4 normal", "4x4 fine",
    "3x3 normal", "2x2 normal", "8x8 custom", "6x6 custom", "5x5 custom",
    "4x4 custom", "3x3 custom", "2x2 custom",
    0
  };

static const SANE_Range percentage_range =
  {
    -100 << SANE_FIXED_SCALE_SHIFT,	/* minimum */
     100 << SANE_FIXED_SCALE_SHIFT,	/* maximum */
       1 << SANE_FIXED_SCALE_SHIFT	/* quantization */
  };


/* Color band codes: */
#define MUSTEK_CODE_GRAY		0
#define MUSTEK_CODE_RED			1
#define MUSTEK_CODE_GREEN		2
#define MUSTEK_CODE_BLUE		3

/* SCSI commands that the Mustek scanners understand (or not): */
#define MUSTEK_SCSI_TEST_UNIT_READY	0x00
#define MUSTEK_SCSI_REQUEST_SENSE       0x03
#define MUSTEK_SCSI_AREA_AND_WINDOWS	0x04
#define MUSTEK_SCSI_READ_SCANNED_DATA	0x08
#define MUSTEK_SCSI_GET_IMAGE_STATUS	0x0f
#define MUSTEK_SCSI_ADF_AND_BACKTRACK	0x10
#define MUSTEK_SCSI_CCD_DISTANCE	0x11
#define MUSTEK_SCSI_INQUIRY		0x12
#define MUSTEK_SCSI_MODE_SELECT 	0x15
#define MUSTEK_SCSI_START_STOP		0x1b
#define MUSTEK_SCSI_SET_WINDOW		0x24
#define MUSTEK_SCSI_GET_WINDOW		0x25
#define MUSTEK_SCSI_READ_DATA		0x28
#define MUSTEK_SCSI_SEND_DATA		0x2a
#define MUSTEK_SCSI_LOOKUP_TABLE	0x55


#define INQ_LEN	0x60
static const u_int8_t scsi_inquiry[] =
{
  MUSTEK_SCSI_INQUIRY, 0x00, 0x00, 0x00, INQ_LEN, 0x00
};

static const u_int8_t scsi_test_unit_ready[] =
{
  MUSTEK_SCSI_TEST_UNIT_READY, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const u_int8_t scsi_area_and_windows[] =
{
  MUSTEK_SCSI_AREA_AND_WINDOWS, 0x00, 0x00, 0x00, 0x09, 0x00
};

static const u_int8_t scsi_request_sense[] =
{
  MUSTEK_SCSI_REQUEST_SENSE, 0x00, 0x00, 0x00, 0x04, 0x00
};

static const u_int8_t scsi_start_stop[] =
{
  MUSTEK_SCSI_START_STOP, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const u_int8_t scsi_ccd_distance[] =
{
  MUSTEK_SCSI_CCD_DISTANCE, 0x00, 0x00, 0x00, 0x05, 0x00
};

static const u_int8_t scsi_get_image_status[] =
{
  MUSTEK_SCSI_GET_IMAGE_STATUS, 0x00, 0x00, 0x00, 0x06, 0x00
};

static const u_int8_t scsi_set_window[] =
{
  MUSTEK_SCSI_SET_WINDOW, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00
};

static const u_int8_t scsi_get_window[] =
{
  MUSTEK_SCSI_GET_WINDOW, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00
};

static const u_int8_t scsi_read_data[] =
{
  MUSTEK_SCSI_READ_DATA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const u_int8_t scsi_send_data[] =
{
  MUSTEK_SCSI_SEND_DATA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const u_int8_t scsi_lookup_table[] =
{
  MUSTEK_SCSI_LOOKUP_TABLE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};


/* For the next macros suffix 'L' means little endian, 'B' big endian */
#define STORE16L(cp,v)				\
do {						\
    int value = (v);				\
						\
    *(cp)++ = (value >> 0) & 0xff;		\
    *(cp)++ = (value >> 8) & 0xff;		\
} while (0)

#define STORE16B(cp,v)				\
do {						\
    int value = (v);				\
						\
    *(cp)++ = (value >> 8) & 0xff;		\
    *(cp)++ = (value >> 0) & 0xff;		\
} while (0)

#define STORE32B(cp,v)				\
do {						\
    long int value = (v);			\
						\
    *(cp)++ = (value >> 24) & 0xff;		\
    *(cp)++ = (value >> 16) & 0xff;		\
    *(cp)++ = (value >>  8) & 0xff;		\
    *(cp)++ = (value >>  0) & 0xff;		\
} while (0)

static SANE_Status adf_and_backtrack (Mustek_Scanner *s);
static SANE_Status area_and_windows (Mustek_Scanner *s);

/* Used for Pro series. First value seems to be always 85, second one varies.
   First bit of second value clear == device ready (?) */
static SANE_Status
scsi_sense_wait_ready (int fd)
{
  struct timeval now, start;
  SANE_Status status;
  size_t len;
  u_int8_t sense_buffer [4];
  u_int8_t bytetxt[300], dbgtxt[300], *pp;

  gettimeofday (&start, 0);

  while (1)
    {
      len = sizeof (sense_buffer);

      DBG(5, "scsi_sense_wait_ready: command size = %d, sense size = %ld\n", 
	  sizeof (scsi_request_sense), len);
      status = sanei_scsi_cmd (fd, scsi_request_sense, 
			       sizeof (scsi_request_sense), sense_buffer, 
			       &len);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG(1, "scsi_sense_wait_ready: failed: %s\n", 
	      sane_strstatus (status));
	  return status;
	}
      
      dbgtxt[0] = '\0';
      for (pp = sense_buffer; pp < ( sense_buffer + 4); pp++)
	{
	  sprintf ((char *) bytetxt, " %02x", *pp);
	  strcat ((char *) dbgtxt, (char *) bytetxt);
	}
      DBG(5, "scsi_sense_wait_ready: sensebuffer: %s\n", dbgtxt);

      if (!(sense_buffer[1] & 0x01))
	{
	  DBG(4, "scsi_sense_wait_ready: ok\n");
	  return SANE_STATUS_GOOD;
	}
      else
	{
	  gettimeofday (&now, 0);
	  if (now.tv_sec - start.tv_sec >= MAX_WAITING_TIME)
	    {
	      DBG(1, "scsi_sense_wait_ready: timed out after %lu seconds\n",
		  (u_long) (now.tv_sec - start.tv_sec));
	      return SANE_STATUS_INVAL;
	    }
	  usleep (100000);	/* retry after 100ms */
	}
    }
  return SANE_STATUS_INVAL;
}

/* Used for 3pass series */
static SANE_Status
scsi_area_wait_ready (Mustek_Scanner *s)
{
  struct timeval now, start;
  SANE_Status status;

  gettimeofday (&start, 0);

  DBG(5, "scsi_area_wait_ready\n");
  while (1)
    {
      status = area_and_windows (s);
      switch (status)
	{
	default:
	  /* Ignore errors while waiting for scanner to become ready.
	     Some SCSI drivers return EIO while the scanner is
	     returning to the home position.  */
	  DBG(3, "scsi_area_wait_ready: failed (%s)\n",
	      sane_strstatus (status));
	  /* fall through */
	case SANE_STATUS_DEVICE_BUSY:
	  gettimeofday (&now, 0);
	  if (now.tv_sec - start.tv_sec >= MAX_WAITING_TIME)
	    {
	      DBG(1, "scsi_area_wait_ready: timed out after %lu seconds\n",
		  (u_long) (now.tv_sec - start.tv_sec));
	      return SANE_STATUS_INVAL;
	    }
	  usleep (100000);	/* retry after 100ms */
	  break;

	case SANE_STATUS_GOOD:
	  return status;
	}
    }
  return SANE_STATUS_INVAL;
}

static SANE_Status
scsi_wait_ready (int fd)
{
  struct timeval now, start;
  SANE_Status status;

  gettimeofday (&start, 0);

  while (1)
    {
      DBG(5, "scsi_wait_ready: sending TEST_UNIT_READY\n");

      status = sanei_scsi_cmd (fd, scsi_test_unit_ready, 
			       sizeof (scsi_test_unit_ready), 0, 0);
      DBG(5, "scsi_wait_ready: TEST_UNIT_READY finished\n");
      switch (status)
	{
	default:
	  /* Ignore errors while waiting for scanner to become ready.
	     Some SCSI drivers return EIO while the scanner is
	     returning to the home position.  */
	  DBG(3, "scsi_wait_ready: test unit ready failed (%s)\n",
	      sane_strstatus (status));
	  /* fall through */
	case SANE_STATUS_DEVICE_BUSY:
	  gettimeofday (&now, 0);
	  if (now.tv_sec - start.tv_sec >= MAX_WAITING_TIME)
	    {
	      DBG(1, "scsi_wait_ready: timed out after %lu seconds\n",
		  (u_long) (now.tv_sec - start.tv_sec));
	      return SANE_STATUS_INVAL;
	    }
	  usleep (100000);	/* retry after 100ms */
	  break;

	case SANE_STATUS_GOOD:
	  return status;
	}
    }
  return SANE_STATUS_INVAL;
}

static SANE_Status
n_wait_ready (int fd)
{
  struct timeval now, start;
  SANE_Status status;

  gettimeofday (&start, 0);

  while (1)
    {
      status = sanei_ab306_test_ready (fd);
      if (status == SANE_STATUS_GOOD)
	return SANE_STATUS_GOOD;

      gettimeofday (&now, 0);
      if (now.tv_sec - start.tv_sec >= MAX_WAITING_TIME)
	{
	  DBG(1, "n_wait_ready: timed out after %lu seconds\n",
	      (u_long) (now.tv_sec - start.tv_sec));
	  return SANE_STATUS_INVAL;
	}
      usleep (100000);	/* retry after 100ms */
    }
}

static SANE_Status
dev_wait_ready (Mustek_Scanner *s)
{
  if (s->hw->flags & MUSTEK_FLAG_N)
    return n_wait_ready (s->fd);
  else if (s->hw->flags & MUSTEK_FLAG_THREE_PASS)
    return scsi_area_wait_ready (s);
  else return scsi_wait_ready (s->fd);
}

static SANE_Status
dev_open (const char * devname, Mustek_Scanner * s,
	  SANEI_SCSI_Sense_Handler handler)
{
  SANE_Status status;

  DBG(5, "dev_open %s\n", devname);

#ifdef HAVE_SANEI_SCSI_OPEN_EXTENDED
  s->hw->buffer_size = s->hw->max_buffer_size;
  status = sanei_scsi_open_extended (devname, &s->fd, handler, 0, 
				     &s->hw->buffer_size);
#else
  s->hw->buffer_size = MIN(sanei_scsi_max_request_size,
			   s->hw->max_buffer_size);
  status = sanei_scsi_open (devname, &s->fd, handler, 0);
#endif

  if (status == SANE_STATUS_GOOD)
    {
      DBG(3, "dev_open: %s is a SCSI device\n", devname);
      DBG(4, "dev_open: wanted %d kbytes, got %d kbytes buffer\n", 
	  s->hw->max_buffer_size / 1024, s->hw->buffer_size / 1024);
      if (s->hw->buffer_size  < 4096) 
	{
	  DBG(1, "dev_open: sanei_scsi_open buffer too small\n");
	  sanei_scsi_close (s->fd);
	  return SANE_STATUS_NO_MEM;
	}
    }
  else
    {
      DBG(3, "dev_open: %s: can't open %s as a SCSI device\n",
	  sane_strstatus (status), devname);

      status = sanei_ab306_open (devname, &s->fd);
      if (status == SANE_STATUS_GOOD)
	{
	  s->hw->flags |= MUSTEK_FLAG_N;
	  DBG(3, "dev_open: %s is an AB306N device\n", devname);
	}
      else
	{
	  DBG(3, "dev_open: %s: can't open %s as an AB306N device\n",
	      sane_strstatus (status), devname);
	  DBG(1, "dev_open: can't open %s\n", devname);
	  return SANE_STATUS_INVAL;
	}
    }
  return SANE_STATUS_GOOD;
}

static SANE_Status
dev_cmd (Mustek_Scanner * s, const void * src, size_t src_size,
	 void * dst, size_t * dst_size)
{
  if (s->hw->flags & MUSTEK_FLAG_N)
    return sanei_ab306_cmd (s->fd, src, src_size, dst, dst_size);
  else
    return sanei_scsi_cmd (s->fd, src, src_size, dst, dst_size);
}

static SANE_Status
dev_req_wait (void *id)
{
  if (!id)
    return SANE_STATUS_GOOD;
  else
    return sanei_scsi_req_wait (id);
}

static SANE_Status
dev_block_read_start (Mustek_Scanner *s, int lines, SANE_Bool first_time)
{
  DBG(4, "dev_block_read_start: entering block for %d lines\n", lines);
  if (s->hw->flags & MUSTEK_FLAG_N)
    {
      u_int8_t readlines[6];
      
      memset (readlines, 0, sizeof (readlines));
      readlines[0] = MUSTEK_SCSI_READ_SCANNED_DATA;
      readlines[2] = (lines >> 16) & 0xff;
      readlines[3] = (lines >>  8) & 0xff;
      readlines[4] = (lines >>  0) & 0xff;
      return sanei_ab306_cmd (s->fd, readlines, sizeof (readlines), 0, 0);
    }
  else if ((s->hw->flags & MUSTEK_FLAG_PARAGON_2) 
	   || (s->hw->flags & MUSTEK_FLAG_PARAGON_1))
    {
      u_int8_t buffer[6];
      size_t len;
      int color;
      SANE_Status status;

      /* reset line-distance values */
      if (s->mode && MUSTEK_MODE_COLOR)
	{
	  for (color = 0; color < 3; ++color)
	    {
	      s->ld.quant[color] = s->ld.max_value;
	      s->ld.index[color] = -s->ld.dist[color];
	    }
	  s->ld.lmod3 = -1;
	  s->ld.ld_line = 0;
	}
	
      /* Get image status (necessary to start new block) */
      len = sizeof (buffer);
      status = dev_cmd (s, scsi_get_image_status, 
			sizeof (scsi_get_image_status), buffer, &len);
      if (status != SANE_STATUS_GOOD)
	return status;

      if (first_time)
	{
	  s->val[OPT_BACKTRACK].w = SANE_FALSE; /* xxx */
	  status = adf_and_backtrack (s);
	  if (status != SANE_STATUS_GOOD)
	    return status;
	}

      /* tell scanner how many lines to scan in one block */
      memset (buffer, 0, sizeof (buffer));
      buffer[0] = MUSTEK_SCSI_READ_SCANNED_DATA;
      buffer[2] = (lines >> 16) & 0xff;
      buffer[3] = (lines >>  8) & 0xff;
      buffer[4] = (lines >>  0) & 0xff;
      buffer[5] = 0x04;
      return sanei_scsi_cmd (s->fd, buffer, sizeof (buffer), 0, 0);
    }
  else
    return SANE_STATUS_GOOD;
}

static SANE_Status
dev_read_req_enter (Mustek_Scanner *s, SANE_Byte *buf, int lines, int bpl,
		    size_t *lenp, void **idp, int bank)
{
  *lenp = lines * bpl;

  if (s->hw->flags & MUSTEK_FLAG_N)
    {
      int planes;

      *idp = 0;
      planes = (s->mode & MUSTEK_MODE_COLOR) ? 3 : 1;

      return sanei_ab306_rdata (s->fd, planes, buf, lines, bpl);
    }
  else
    {
      if (s->hw->flags & MUSTEK_FLAG_SE)
	{
	  u_int8_t readlines[10];
	  if (s->mode & MUSTEK_MODE_COLOR)
	    lines *= 3;
	    
	  memset (readlines, 0, sizeof (readlines));
	  readlines[0] = MUSTEK_SCSI_READ_DATA;
	  readlines[6] = bank;	/* buffer bank not used ??? */
	  readlines[7] = (lines >> 8) & 0xff;
	  readlines[8] = (lines >> 0) & 0xff;
	  return sanei_scsi_req_enter (s->fd, readlines, sizeof (readlines),
				       buf, lenp, idp); 
	}
      else if (s->hw->flags & MUSTEK_FLAG_PRO)
	{
	  u_int8_t readlines[6];

	  DBG(5, "enter read request\n");	  			
	  memset (readlines, 0, sizeof (readlines));
	  readlines[0] = MUSTEK_SCSI_READ_SCANNED_DATA;
	  readlines[2] = ((lines * bpl) >> 16) & 0xff;
	  readlines[3] = ((lines * bpl) >> 8) & 0xff;
	  readlines[4] = ((lines * bpl) >> 0) & 0xff;
	  return sanei_scsi_req_enter (s->fd, readlines, sizeof (readlines),
				       buf, lenp, idp); 
	}
      else /* Paragon series */
      	{	    
	  u_int8_t readlines[6];

	  memset (readlines, 0, sizeof (readlines));
	  readlines[0] = MUSTEK_SCSI_READ_SCANNED_DATA;
	  readlines[2] = (lines >> 16) & 0xff;
	  readlines[3] = (lines >> 8) & 0xff;
	  readlines[4] = (lines >> 0) & 0xff;
	  return sanei_scsi_req_enter (s->fd, readlines, sizeof (readlines),
				       buf, lenp, idp); 
	}
    }
}

static void
dev_close (Mustek_Scanner * s)
{
  if (s->hw->flags & MUSTEK_FLAG_N)
    sanei_ab306_close (s->fd);
  else
    sanei_scsi_close (s->fd);
}

static SANE_Status
sense_handler (int scsi_fd, u_char *result, void *arg)
{
  if (!result)
    {
      DBG(5, "sense_handler: no sense buffer\n");
      return SANE_STATUS_IO_ERROR;
    }
  if (!arg)
    DBG(5, "sense_handler: got sense code %02x for fd %d (arg = null)\n",
	result[0], scsi_fd);
  else
    DBG(5, "sense_handler: got sense code %02x for fd %d (arg = %uc)\n",
	result[0], scsi_fd, *(unsigned char*) arg);
  switch (result[0])
    {
    case 0x00:
      break; 

    case 0x82:
      if (result[1] & 0x80)
	{
	  DBG(3, "sense_handler: ADF is jammed\n");
	  return SANE_STATUS_JAMMED;	/* ADF is jammed */
	}
      break;

    case 0x83:
      if (result[2] & 0x02)
	{
	  DBG(3, "sense_handler: ADF is out of documents\n");
	  return SANE_STATUS_NO_DOCS; /* ADF out of documents */
	}
      break;

    case 0x84:
      if (result[1] & 0x10)
	{
	  DBG(3, "sense_handler: transparency adapter cover open\n");
	  return SANE_STATUS_COVER_OPEN; /* open transparency adapter cover */
	}
      break;

    default:
      DBG(1, "sense_handler: got unknown sense code %02x for fd %d\n",
	  result[0], scsi_fd);
      return SANE_STATUS_IO_ERROR;
    }
  return SANE_STATUS_GOOD;
}

static SANE_Status
inquiry (Mustek_Scanner *s)
{
  char result[INQ_LEN];
  size_t size;
  SANE_Status status;

  DBG(5, "inquiry: sending INQUIRY\n");
  size = sizeof (result);

  status = dev_cmd (s, scsi_inquiry, sizeof (scsi_inquiry), result, &size);
  if (status != SANE_STATUS_GOOD)
    return status;
    
  /* checking ADF status */
  if (s->hw->flags & MUSTEK_FLAG_ADF)
    {
      if (result[63] & (1 << 3))
	{
	  s->hw->flags |= MUSTEK_FLAG_ADF_READY;
	}
      else
	{
	  s->hw->flags &= ~MUSTEK_FLAG_ADF_READY;
	}
    }
  if (!result[0])
    return SANE_STATUS_DEVICE_BUSY;
  return SANE_STATUS_GOOD;
}

static SANE_Status
attach (const char *devname, Mustek_Device **devp, int may_wait)
{
  int mustek_scanner, fw_revision;
  u_int8_t result[INQ_LEN];
  u_int8_t inquiry_byte_list [50], inquiry_text_list [17];
  u_int8_t inquiry_byte [5], inquiry_text [5];
  u_int8_t *model_name = result + 44;
  Mustek_Scanner s;
  Mustek_Device *dev, new_dev;
  SANE_Status status;
  size_t size;
  char *scsi_device_type[] = 
    {
      "Direct-Access", "Sequential-Access", "Printer", "Processor", 
      "Write-Once", "CD-ROM", "Scanner", "Optical Memory", "Medium Changer",
      "Communications"
    };
  u_int8_t scsi_vendor[9];
  u_int8_t scsi_product[17];
  u_int8_t scsi_revision[5];
  u_int8_t *pp;
  SANE_Bool warning = SANE_FALSE;
  int firmware_format = 0;
  int firmware_revision_system = 0;

  if (devp)
    *devp = 0;
    
  for (dev = first_dev; dev; dev = dev->next)
    if (strcmp (dev->sane.name, devname) == 0)
      {
	if (devp)
	  *devp = dev;
	return SANE_STATUS_GOOD;
      }

  memset (&new_dev, 0, sizeof (new_dev));
  memset (&s, 0, sizeof (s));
  s.hw = &new_dev;
  s.hw->max_buffer_size = 8 * 1024;

  DBG(3, "attach: trying device %s\n", devname);

  status = dev_open (devname, &s, sense_handler);
  if (status != SANE_STATUS_GOOD)
    return status;

  if (may_wait)
    dev_wait_ready (&s);

  DBG(5, "attach: sending INQUIRY\n");
  size = sizeof (result);
  status = dev_cmd (&s, scsi_inquiry, sizeof (scsi_inquiry), result, &size);
  if (status != SANE_STATUS_GOOD || size != INQ_LEN)
    {
      DBG(1, "attach: inquiry for device %s failed (%s)\n", devname,
	  sane_strstatus (status));
      dev_close (&s);
      return status;
    }

  status = dev_wait_ready (&s);
  dev_close (&s);

  if (status != SANE_STATUS_GOOD)
    return status;

  if ((result[0] & 0x1f) != 0x06)
    {
      DBG(1, "attach: device %s doesn't look like a scanner at all (%d)\n", 
	  devname, result[0] & 0x1f);
      return SANE_STATUS_INVAL;
    }

  if (debug_level >= 3)
    {
      /* clear spaces and special chars */
      strncpy ((char *) scsi_vendor, (char *) result + 8, 8);
      scsi_vendor[8] = '\0';
      pp = scsi_vendor + 7;
      while (pp >= scsi_vendor && (*pp == ' ' || *pp >= 127))
	*pp-- = '\0';
      strncpy ((char *) scsi_product, (char *) result + 16, 16);
      scsi_product[16] = '\0';
      pp = scsi_product + 15;
      while (pp >= scsi_product && (*pp == ' ' || *pp >= 127))
	*pp-- = '\0';
      strncpy ((char *) scsi_revision, (char *) result + 32, 4);
      scsi_revision[4] = '\0';
      pp = scsi_revision + 3;
      while (pp >= scsi_revision && (*pp == ' ' || *pp >= 127))
	*pp-- = '\0';
      DBG(3, "attach: SCSI Vendor: `%-8s' Model: `%-16s' Rev.: `%-4s'\n",
	  scsi_vendor,  scsi_product, scsi_revision);
      DBG(3, "attach: SCSI Type: %s; ANSI rev.: %d\n", 
	  ((result[0] & 0x1f) < 0x10) ? 
	  scsi_device_type[result[0] & 0x1f] : "Unknown", result [2] & 0x03);
      DBG(3, "attach: SCSI flags: %s%s%s%s%s%s%s\n", 
	  (result [7] & 0x80) ? "RelAdr " : "",
	  (result [7] & 0x40) ? "WBus32 " : "",
	  (result [7] & 0x20) ? "WBus16 " : "",
	  (result [7] & 0x10) ? "Sync " : "",
	  (result [7] & 0x08) ? "Linked " : "",
	  (result [7] & 0x02) ? "CmdQue " : "",
	  (result [7] & 0x01) ? "SftRe " : "");
    }
  
  if (debug_level >= 4)
    {
      /* print out inquiry */
      DBG(4, "attach: inquiry output:\n");
      inquiry_byte_list [0] = '\0';
      inquiry_text_list [0] = '\0';
      for (pp = result; pp < (result + INQ_LEN); pp++)
	{
	  sprintf ((char *) inquiry_text, "%c", 
		   (*pp < 127) && (*pp > 31) ? *pp : '.');
	  strcat ((char *) inquiry_text_list, (char *) inquiry_text);
	  sprintf ((char *) inquiry_byte, " %02x", *pp);
	  strcat ((char *) inquiry_byte_list, (char *) inquiry_byte);
	  if ((pp - result) % 0x10 == 0x0f)
	    {
	      DBG(4, "%s  %s\n", inquiry_byte_list, inquiry_text_list);
	      inquiry_byte_list [0] = '\0';
	      inquiry_text_list [0] = '\0';
	    }
	}
    }

  /* first check for new firmware format: */
  mustek_scanner = (strncmp ((char *) result + 36, "MUSTEK", 6) == 0);
  if (mustek_scanner)
    {
      if (result[43] == 'M')
	{
	  DBG(3, "attach: found Mustek scanner (pro series firmware "
	      "format)\n");
	  firmware_format = 2;
	  model_name = result + 43;
	}
      else
	{
	  DBG(3, "attach: found Mustek scanner (new firmware format)\n");
	  firmware_format = 1;
	}
    }
  else
    {
      /* check for old format: */
      mustek_scanner = (strncmp ((char *) result + 8, "MUSTEK", 6) == 0);
      if (mustek_scanner)
	{
	  model_name = result + 16;
	  DBG(3, "attach: found Mustek scanner (old firmware format)\n");
	  firmware_format = 0;
	}
      else
	{
	  DBG(1, "attach: device %s doesn't look like a Mustek scanner\n", 
	      devname);
	  return SANE_STATUS_INVAL;
	}
    }
  
  /* get firmware revision as BCD number:             */
  /* General format: x.yz                             */
  /* Newer ScanExpress scanners (ID XC06): Vxyz       */ 
  if (result[33] == '.')
    {
      fw_revision =
	(result[32] - '0') << 8 | (result[34] - '0') << 4 | (result[35] - '0');
      firmware_revision_system = 0;
      DBG(4, "attach: old firmware revision system\n");
    }
  else
    {
      fw_revision =
	(result[33] - '0') << 8 | (result[34] - '0') << 4 | (result[35] - '0');
      firmware_revision_system = 1;
      DBG(4, "attach: new firmware revision system\n");
    }
  DBG(3, "attach: firmware revision %d.%02x\n",
      fw_revision >> 8, fw_revision & 0xff);

  dev = malloc (sizeof (*dev));
  if (!dev)
    return SANE_STATUS_NO_MEM;

  memcpy (dev, &new_dev, sizeof (*dev));

  dev->name = strdup (devname);
  dev->sane.name = (SANE_String_Const) dev->name;
  dev->sane.vendor = "Mustek";
  dev->sane.type   = "flatbed scanner";

  dev->x_range.min = 0;
  dev->y_range.min = 0;
  dev->x_range.quant = 0;
  dev->y_range.quant = 0;
  dev->x_trans_range.min = 0;
  dev->y_trans_range.min = 0;
  /* default to something really small to be on the safe side: */
  dev->x_trans_range.max = SANE_FIX (8.0 * MM_PER_INCH);
  dev->y_trans_range.max = SANE_FIX (5.0 * MM_PER_INCH);
  dev->x_trans_range.quant = 0;
  dev->y_trans_range.quant = 0;
  if (dev->flags & MUSTEK_FLAG_N)
    /* ab306 N scanners make unhealthy noises below 51 dpi... */
    dev->dpi_range.min = SANE_FIX (51);
  else
    dev->dpi_range.min = SANE_FIX (30); /* some scanners don't like low dpi */
  dev->dpi_range.quant = SANE_FIX (1);
  /* default to 128 kB */
  dev->max_buffer_size = 64 * 1024;
  dev->max_block_buffer_size = 1024 * 1024 * 1024;
  dev->firmware_format = firmware_format;
  dev->firmware_revision_system = firmware_revision_system;
  
  DBG(3, "attach: scanner id: %.11s\n", model_name);
  if (strncmp ((char *) model_name + 10, "PRO", 3) == 0)
    DBG(3, "attach: this is probably a Paragon Pro series scanner\n");
  else if (strncmp ((char *) model_name, "MFC", 3) == 0)
    DBG(3, "attach: this is probably a Paragon series II scanner\n");
  else if (strncmp ((char *) model_name, "M", 1) == 0)
    DBG(3, "attach: this is probably a Paragon series I or 3-pass scanner\n");
  else if (strncmp ((char *) model_name, " C", 2) == 0)
    DBG(3, "attach: this is probably a ScanExpress series scanner\n");
  else if (strncmp ((char *) model_name, "XC", 2) == 0)
    DBG(3, "attach: this is probably a ScanExpress Plus series scanner\n");
  else
    DBG(3, "attach: I am not sure what type of scanner this is\n");

  /* Paragon 3-pass series */
  if (strncmp ((char *) model_name, "MFS-12000CX", 11) == 0)
    {
      /* These values were measured and compared to those from the Windows
	 driver. Tested with a Paragon MFS-12000CX v4.00 */
      dev->x_range.min = SANE_FIX (0.0);
      dev->x_range.max = SANE_FIX (8.5 * MM_PER_INCH);
      dev->y_range.min = SANE_FIX (0.0);
      dev->y_range.max = SANE_FIX (14.00 * MM_PER_INCH);
      dev->x_trans_range.min = SANE_FIX (1.0);
      dev->y_trans_range.min = SANE_FIX (1.0);
      dev->x_trans_range.max = SANE_FIX (205.0);
      dev->y_trans_range.max = SANE_FIX (255.0);
      dev->dpi_range.max = SANE_FIX (1200);
      dev->sane.model = "MFS-12000CX";
    }
  /* There are two different versions of the MFS-6000CX, one has the model
     name "MFS-06000CX", the other one is "MSF-06000CZ"   */
  else if (strncmp ((char *) model_name, "MFS-06000CX", 11) == 0)
    {
      /* These values were measured and tested with a Paragon MFS-6000CX
	 v4.06 */
      dev->x_range.min = SANE_FIX (0.0);
      dev->x_range.max = SANE_FIX (8.5 * MM_PER_INCH);
      dev->y_range.min = SANE_FIX (0.0);
      dev->y_range.max = SANE_FIX (13.86 * MM_PER_INCH);
      dev->x_trans_range.min = SANE_FIX (1.0);
      dev->y_trans_range.min = SANE_FIX (2.0);
      dev->x_trans_range.max = SANE_FIX (203.0);
      dev->y_trans_range.max = SANE_FIX (255.0);

      dev->dpi_range.max = SANE_FIX (600);
      dev->sane.model = "MFS-6000CX";
    }
  else if (strncmp((char *) model_name, "MSF-06000CZ", 11) == 0)
    {
      /* These values were measured and compared to those from the Windows
	 driver. Tested with a Paragon MFS-6000CX v4.00 */
      dev->x_range.min = SANE_FIX (0.0);
      dev->x_range.max = SANE_FIX (8.5 * MM_PER_INCH);
      dev->y_range.min = SANE_FIX (0.0);
      dev->y_range.max = SANE_FIX (13.85 * MM_PER_INCH);
      dev->x_trans_range.min = SANE_FIX (1.0);
      dev->y_trans_range.min = SANE_FIX (2.0);
      dev->x_trans_range.max = SANE_FIX (205.0);
      dev->y_trans_range.max = SANE_FIX (255.0);

      dev->dpi_range.max = SANE_FIX (600);
      dev->sane.model = "MFS-6000CX";
    }

  /* Paragon 1-pass 14" series I */

  /* I haven't seen a single report for this, but it is mentioned in
     the old man page. All reported Paragon 1200SP had a model name 
     "MFS-12000SP" */
  else if (strncmp ((char *) model_name, "MSF-12000SP", 11) == 0)
    {
      /* These values are not tested. */
      dev->x_range.max = SANE_FIX (8.5 * MM_PER_INCH);  
      dev->y_range.max = SANE_FIX (13.85 * MM_PER_INCH);
      dev->x_trans_range.min = SANE_FIX (1.0);
      dev->y_trans_range.min = SANE_FIX (1.0);
      dev->x_trans_range.max = SANE_FIX (200.0);
      dev->y_trans_range.max = SANE_FIX (250.0);
      dev->dpi_range.max = SANE_FIX (1200);
      /* This is a bit of a guess.  The reports by Andreas Gaumann 
	 indicate that at least the MSF-06000SP with firmware revision
	 3.12 does not require any line-distance correction.  I'm
	 guessing this is true for all firmware revisions and for
	 MSF-12000SP and MSF-06000SP. */
      dev->flags |= MUSTEK_FLAG_LD_NONE;
      dev->sane.model = "MFS-12000SP";
      dev->flags |= MUSTEK_FLAG_PARAGON_1;
      warning = SANE_TRUE;      
    }
  /* MFS-8000 SP v 1.x */
  else if (strncmp ((char *) model_name, "MSF-08000SP", 11) == 0)
    {
      /* These values were measured and compared to those from the Windows
	 driver. Tested with a Paragon MFS-8000SP v1.20 */
      dev->x_range.min = SANE_FIX (0.0);
      dev->x_range.max = SANE_FIX (8.5 * MM_PER_INCH);
      dev->y_range.min = SANE_FIX (2.5);
      dev->y_range.max = SANE_FIX (355.6);
      dev->x_trans_range.min = SANE_FIX (1.0);
      dev->y_trans_range.min = SANE_FIX (1.0);
      dev->x_trans_range.max = SANE_FIX (205.0);
      dev->y_trans_range.max = SANE_FIX (255.0);

      dev->dpi_range.max = SANE_FIX (800);
      /* At least scanners with firmware 1.20 need a gamma table upload
	 in color mode, otherwise the image is red */
      if (fw_revision == 0x120)
	dev->flags |= MUSTEK_FLAG_FORCE_GAMMA;
      
      dev->sane.model = "MFS-8000SP";
      dev->flags |= MUSTEK_FLAG_PARAGON_1;
      dev->flags |= MUSTEK_FLAG_LD_BLOCK;
    }
  /* This model name exists */
  else if (strncmp ((char *) model_name, "MSF-06000SP", 11) == 0)
    {
      /* These values were measured and compared to those from the Windows
	 driver. Tested with a Paragon MFS-6000SP v3.12 */
      dev->x_range.min = SANE_FIX (0.0);
      dev->x_range.max = SANE_FIX (8.5 * MM_PER_INCH);
      dev->y_range.min = SANE_FIX (4.0);
      dev->y_range.max = SANE_FIX (355.6);
      dev->x_trans_range.min = SANE_FIX (1.0);
      dev->y_trans_range.min = SANE_FIX (1.0);
      dev->x_trans_range.max = SANE_FIX (205.0);
      dev->y_trans_range.max = SANE_FIX (255.0);
      dev->dpi_range.max = SANE_FIX (600);
      /* MSF-06000SP with firmware revision 3.12 does not require any
	 line-distance correction. */
      dev->sane.model = "MFS-6000SP";
      dev->flags |= MUSTEK_FLAG_PARAGON_1;
      dev->flags |= MUSTEK_FLAG_LD_BLOCK;
    }

  /* This one was reported multiple times */
  else if (strncmp ((char *) model_name, "MFS-12000SP", 11) == 0)
    {
      /* These values were measured and compared to those from the Windows
	 driver. Tested with a Paragon MFS-12000SP v1.02 and v1.00 */
      dev->x_range.min = SANE_FIX (0.0);
      dev->x_range.max = SANE_FIX (217.0);		
      dev->y_range.min = SANE_FIX (2.0);
      dev->y_range.max = SANE_FIX (352.0);   
      dev->x_trans_range.min = SANE_FIX (0.0);
      dev->y_trans_range.min = SANE_FIX (0.0);
      dev->x_trans_range.max = SANE_FIX (205.0);
      dev->y_trans_range.max = SANE_FIX (250.0);

      dev->dpi_range.max = SANE_FIX (1200);
      /* Earlier versions of this source code used MUSTEK_FLAG_LD_MFS
	 for firmware versions < 1.02 and LD_NONE for the rest. This
	 didn't work for my scanners.  1.00 doesn't need any LD
	 correction, 1.02, 1.07 and 1.11 do need normal LD
	 corrections.  Maybe all != 1.00 need normal LD */

      if ((fw_revision != 0x111) && (fw_revision != 0x107) && 
	  (fw_revision != 0x106) && (fw_revision != 0x102) )
	dev->flags |= MUSTEK_FLAG_LD_NONE;
      dev->sane.model = "MFS-12000SP";
      dev->flags |= MUSTEK_FLAG_PARAGON_1;
      dev->flags |= MUSTEK_FLAG_LD_BLOCK;
    }
  /* MFS-8000 SP v2.x */
  else if (strncmp ((char *) model_name, "MFS-08000SP", 11) == 0)
    {
      /* These values are tested with a MFS-08000SP v 2.04 */
      dev->x_range.min = SANE_FIX (0.0);
      dev->x_range.max = SANE_FIX (8.5 * MM_PER_INCH);
      dev->y_range.min = SANE_FIX (2.5);
      dev->y_range.max = SANE_FIX (355.6);
      dev->x_trans_range.min = SANE_FIX (1.0);
      dev->y_trans_range.min = SANE_FIX (1.0);
      dev->x_trans_range.max = SANE_FIX (205.0);
      dev->y_trans_range.max = SANE_FIX (255.0);

      dev->dpi_range.max = SANE_FIX (800);
      /* At least scanners with firmware 1.20 need a gamma table upload
	 in color mode, otherwise the image is red */
      if (fw_revision == 0x120)
	dev->flags |= MUSTEK_FLAG_FORCE_GAMMA;
      dev->sane.model = "MFS-8000SP";
      dev->flags |= MUSTEK_FLAG_PARAGON_1;
      dev->flags |= MUSTEK_FLAG_LD_BLOCK;
    }
  /* I have never seen one of those */
  else if (strncmp ((char *) model_name, "MFS-06000SP", 11) == 0)
    {
      /* These values are not tested. */
      dev->x_range.max = SANE_FIX (8.5 * MM_PER_INCH);  
      dev->y_range.max = SANE_FIX (13.84 * MM_PER_INCH);
      /* copied from MSF-06000SP */
      dev->x_trans_range.min = SANE_FIX (1.0);
      dev->y_trans_range.min = SANE_FIX (1.0);
      dev->x_trans_range.max = SANE_FIX (205.0);
      dev->y_trans_range.max = SANE_FIX (255.0);
      dev->dpi_range.max = SANE_FIX (600);
      dev->sane.model = "MFS-6000SP";
      dev->flags |= MUSTEK_FLAG_PARAGON_1;
      dev->flags |= MUSTEK_FLAG_LD_BLOCK;
      warning = SANE_TRUE;      
    }

  /* Paragon 1-pass A4 series II */
  else if (strncmp ((char *) model_name, "MFC-08000CZ", 11) == 0)
    {
      /* These values were measured and compared to those from the Windows
	 driver. Tested with a Paragon 800 II SP v1.06. */
      dev->x_range.min = SANE_FIX (1.5);
      dev->x_range.max = SANE_FIX (218.0);		
      dev->y_range.min = SANE_FIX (0.0);
      dev->y_range.max = SANE_FIX (288.0);   
      dev->x_trans_range.min = SANE_FIX (0.0);
      dev->y_trans_range.min = SANE_FIX (0.0);
      dev->x_trans_range.max = SANE_FIX (205.0);
      dev->y_trans_range.max = SANE_FIX (254.0);

      dev->dpi_range.max = SANE_FIX (800);
      dev->sane.model = "800S/800 II SP";
      dev->flags |= MUSTEK_FLAG_PARAGON_2;
      dev->flags |= MUSTEK_FLAG_LD_BLOCK;
      dev->max_block_buffer_size = 2 * 1024 * 1024;
    }
  else if (strncmp ((char *) model_name, "MFC-06000CZ", 11) == 0)
    {
      /* These values were measured and compared to those from the
	 Windows driver. Tested with a Paragon 600 II CD, a Paragon
	 MFC-600S and a Paragon 600 II N. */
      dev->x_range.min = SANE_FIX (0.0);
      dev->x_range.max = SANE_FIX (215.9);
      dev->y_range.min = SANE_FIX (2.0);
      dev->y_range.max = SANE_FIX (288.0); 
      dev->x_trans_range.min = SANE_FIX (0.0);
      dev->y_trans_range.min = SANE_FIX (0.0);
      dev->x_trans_range.max = SANE_FIX (201.0);
      dev->y_trans_range.max = SANE_FIX (257.0);
      
      dev->dpi_range.max = SANE_FIX (600);
      /* This model comes in a non-scsi version, too. It is supplied
	 with its own parallel-port like adapter, an AB306N. Two
	 firmware revisions are known: 1.01 and 2.00. Each needs its
	 own line-distance correction code. */
      if (dev->flags & MUSTEK_FLAG_N)
	{
	  if (fw_revision < 0x200)
	    dev->flags |= MUSTEK_FLAG_LD_N1;
	  else
	    dev->flags |= MUSTEK_FLAG_LD_N2;
	  dev->x_trans_range.min = SANE_FIX (33.0);
	  dev->y_trans_range.min = SANE_FIX (62.0);
	  dev->x_trans_range.max = SANE_FIX (183.0);
	  dev->y_trans_range.max = SANE_FIX (238.0);
	  dev->max_block_buffer_size = 1024 * 1024 * 1024;
	  dev->sane.model = "600 II N";
	}
      else
	{
	  dev->sane.model = "600S/600 II CD";
	  dev->flags |= MUSTEK_FLAG_PARAGON_2;
	  dev->flags |= MUSTEK_FLAG_LD_BLOCK;
	}
    }

  /* ScanExpress and ScanMagic series */
  else if (strncmp((char *) model_name, " C03", 4) == 0)
    {
      /* These values were measured and compared to those from the Windows
	 driver. Tested with a ScannExpress 6000SP 1.00 */
      dev->x_range.max = SANE_FIX (215);
      dev->y_range.min = SANE_FIX (2.5);
      dev->y_range.max = SANE_FIX (294.5);
      dev->x_trans_range.min = SANE_FIX (33.0);
      dev->y_trans_range.min = SANE_FIX (62.0);
      dev->x_trans_range.max = SANE_FIX (183.0);
      dev->y_trans_range.max = SANE_FIX (238.0);

      dev->dpi_range.max = SANE_FIX (600);
      dev->dpi_range.min = SANE_FIX (75);
      dev->flags |= MUSTEK_FLAG_SE;
      /* At least the SE 6000SP with firmware 1.00 limits its
	 x-resolution to 300 dpi and does *no* interpolation at higher
	 resolutions. So this has to be done in software. */
      dev->flags |= MUSTEK_FLAG_ENLARGE_X;
      dev->sane.model = "ScanExpress 6000SP";
    }
  /* There are two different versions of the ScanExpress 12000SP, one
     has the model name " C06", the other one is "XC06". The latter
     seems to be used in current models, especially the ScanExpress
     12000SP Plus */
  else if (strncmp((char *) model_name, " C06", 4) == 0)
    {
      /* These values were measured and compared to those from the Windows
	 driver. Tested with a ScaneExpress 12000SP 2.02 and a ScanMagic
         9636S v 1.01 */
      dev->x_range.max = SANE_FIX (216);
      dev->y_range.min = SANE_FIX (2.5);
      dev->y_range.max = SANE_FIX (294.5);
      dev->x_trans_range.min = SANE_FIX (33.0);
      dev->y_trans_range.min = SANE_FIX (62.0);
      dev->x_trans_range.max = SANE_FIX (183.0);
      dev->y_trans_range.max = SANE_FIX (238.0);

      dev->dpi_range.max = SANE_FIX (1200);
      dev->dpi_range.min = SANE_FIX (75);
      dev->flags |= MUSTEK_FLAG_SE;
      /* The ScanExpress models limit their x-resolution to 600 dpi
	 and do *no* interpolation at higher resolutions. So this has
	 to be done in software. */
      dev->flags |= MUSTEK_FLAG_ENLARGE_X;
      dev->sane.model = "ScanExpress 12000SP";
    }
  else if (strncmp((char *) model_name, "XC06", 4) == 0)
    {
      /* These values are not tested. */
      dev->x_range.max = SANE_FIX (216);
      dev->y_range.min = SANE_FIX (2.5);
      dev->y_range.max = SANE_FIX (294.5);
      dev->x_trans_range.min = SANE_FIX (33.0);
      dev->y_trans_range.min = SANE_FIX (62.0);
      dev->x_trans_range.max = SANE_FIX (183.0);
      dev->y_trans_range.max = SANE_FIX (238.0);

      dev->dpi_range.max = SANE_FIX (1200);
      dev->dpi_range.min = SANE_FIX (75);
      dev->flags |= MUSTEK_FLAG_SE;
      /* The ScanExpress models limit their x-resolution to 600 dpi
	 and do *no* interpolation at higher resolutions. So this has
	 to be done in software. */
      dev->flags |= MUSTEK_FLAG_ENLARGE_X;
      dev->sane.model = "ScanExpress 12000SP (Plus)";
    }
  /* ScanExpress A3 SP */
  else if (strncmp((char *) model_name, " L03", 4) == 0)
    {
      /* These values were measured with a ScannExpress A3 SP 2.00 */
      dev->x_range.max = SANE_FIX (297);
      dev->y_range.min = SANE_FIX (0);
      dev->y_range.max = SANE_FIX (430);
      /* TA couldn't be tested due to lack of equipment */
      /*
      dev->x_trans_range.min = SANE_FIX (33.0);
      dev->y_trans_range.min = SANE_FIX (62.0);
      dev->x_trans_range.max = SANE_FIX (183.0);
      dev->y_trans_range.max = SANE_FIX (238.0);
      */
      dev->dpi_range.max = SANE_FIX (600);
      dev->dpi_range.min = SANE_FIX (75);
      dev->flags |= MUSTEK_FLAG_SE;
      /* The ScanExpress models limit their x-resolution to 300 dpi
	 and do *no* interpolation at higher resolutions. So this has
	 to be done in software. */
      dev->flags |= MUSTEK_FLAG_ENLARGE_X;
      dev->sane.model = "ScanExpress A3 SP";
    }
  /* Paragon 1200 SP Pro */
  else if (strncmp((char *) model_name, "MFS-1200SPPRO", 13) == 0)
    {
      /* These values were measured with a Paragon 1200 SP Pro v2.01 */
      dev->x_range.max = SANE_FIX (8 * MM_PER_INCH);
      dev->y_range.max = SANE_FIX (13.85 * MM_PER_INCH);
      dev->dpi_range.max = SANE_FIX (1200);
      dev->sane.model = "1200 SP PRO";
      dev->flags |= MUSTEK_FLAG_LD_NONE;
      dev->flags |= MUSTEK_FLAG_ENLARGE_X;
      warning = SANE_TRUE;
    }
  /* No documentation, but it works: Paragon 1200 A3 PRO  */
  else if (strncmp((char *) model_name, "MFS-1200A3PRO", 13) == 0)
    {
      /* These values were measured and compared to those from the Windows
	 driver. Tested with a Paragon 1200 A3 Pro v1.10 */
      dev->x_range.max = SANE_FIX (11.7 * MM_PER_INCH);
      dev->y_range.max = SANE_FIX (17 * MM_PER_INCH);
      dev->dpi_range.max = SANE_FIX (1200);
      dev->sane.model = "1200 A3 PRO";
      dev->flags |= MUSTEK_FLAG_LD_NONE;
      dev->flags |= MUSTEK_FLAG_ENLARGE_X;
      warning = SANE_TRUE;
    }
  else
    {
      DBG(0, "attach: this Mustek scanner (ID: %s) is not supported yet\n",
	  model_name);
      DBG(0, "attach: please set the debug level to 5 and send a debug "
	  "report\n");
      DBG(0, "attach: to sane-devel@mostang.com (export "
	  "SANE_DEBUG_MUSTEK=5\n");
      DBG(0, "attach: scanimage -L 2>debug.txt . Thank you.\n");
      free (dev);
      return SANE_STATUS_INVAL;
    }

  if (dev->flags & MUSTEK_FLAG_SE)
    {      
      DBG(3, "attach: this is a single-pass scanner\n");
    }
  else    
    {
      if (result[57] & (1 << 6))
	{
	  DBG(3, "attach: this is a single-pass scanner\n");
	  if (dev->flags & MUSTEK_FLAG_LD_MFS)
	    DBG(4, "attach: using special line-distance algorithm\n");
	  else if (dev->flags & MUSTEK_FLAG_LD_NONE)
	    DBG(4, "attach: scanner doesn't need line-distance correction\n");
	  else if (dev->flags & MUSTEK_FLAG_LD_N1)
	    DBG(4, "attach: scanner has N1 line-distance correction\n");
	  else if (dev->flags & MUSTEK_FLAG_LD_N2)
	    DBG(4, "attach: scanner has N2 line-distance correction\n");
	  else if (dev->flags & MUSTEK_FLAG_LD_BLOCK)
	    DBG(4, "attach: scanner has block line-distance correction\n");
	  else
	    DBG(4, "attach: scanner has normal line-distance correction\n");
	}
      else
        {
	  dev->flags |= MUSTEK_FLAG_THREE_PASS;
          /* three-pass scanners quantize to 0.5% of the maximum resolution: */
          dev->dpi_range.quant = dev->dpi_range.max / 200;
          dev->dpi_range.min = dev->dpi_range.quant;
	  DBG(3, "attach: this is a three-pass scanner\n");
        }
      if (result[57] & (1 << 5))
	{
	  DBG(3, "attach: this is a professional series scanner\n");
	  dev->flags |= MUSTEK_FLAG_PRO;
	}
      if (result[63] & (1 << 2))
	{
	  dev->flags |= MUSTEK_FLAG_ADF;
	  DBG(3, "attach: found automatic document feeder (ADF)\n");
	  if (result[63] & (1 << 3))
	    {
	      dev->flags |= MUSTEK_FLAG_ADF_READY;
	      DBG(4, "attach: automatic document feeder is ready\n");
	    }
	  else
	    {
	      DBG(4, "attach: automatic document feeder is out of "
		  "documents\n");
	    }
	}

      if (result[63] & (1 << 6))
	{
	  dev->flags |= MUSTEK_FLAG_TA;
	  DBG(3, "attach: found transparency adapter (TA)\n");
	}
    }

  if (! (result[62] & (1 << 0)) && (dev->flags & MUSTEK_FLAG_SE) )
    {
      DBG(4, "attach: scanner cover is open\n");
    }


  if (warning == SANE_TRUE)
    {
      DBG(0, "WARNING: Your scanner was detected by the SANE Mustek backend, "
	  "but\n  it is not fully tested. It may or may not work. Be "
	  "carefull and read\n  the PROBLEMS file in the sane directory. "
	  "Please set the debug level of this\n  backend to maximum "
	  "(export SANE_DEBUG_MUSTEK=255) and send the output of\n  "
	  "scanimage -L to the SANE mailing list sane-devel@mostang.com. "
	  "Please include\n  the exact model name of your scanner and to "
	  "which extend it works.\n");
    }

  DBG(2, "attach: found Mustek %s %s, %s%s%s%s\n",
      dev->sane.model, dev->sane.type,
      (dev->flags & MUSTEK_FLAG_THREE_PASS) ? "3-pass" : "1-pass",
      (dev->flags & MUSTEK_FLAG_ADF) ? ", ADF" : "",
      (dev->flags & MUSTEK_FLAG_TA)  ? ", TA" : "",
      (dev->flags & MUSTEK_FLAG_SE) ? ", SE" : "");

  ++num_devices;
  dev->next = first_dev;
  first_dev = dev;

  if (devp)
    *devp = dev;
  return SANE_STATUS_GOOD;
}

static size_t
max_string_size (const SANE_String_Const strings[])
{
  size_t size, max_size = 0;
  int i;

  for (i = 0; strings[i]; ++i)
    {
      size = strlen (strings[i]) + 1;
      if (size > max_size)
	max_size = size;
    }
  return max_size;
}

static SANE_Status
constrain_value (Mustek_Scanner *s, SANE_Int option, void *value,
		 SANE_Int *info)
{
  SANE_Fixed w, dpi;

  if (option == OPT_BR_Y)
    {
      DBG(5, "constrain_value: value = %f, max = %f\n", 
	  SANE_UNFIX (*(SANE_Word *) value), 
	  SANE_UNFIX (s->opt[option].constraint.range->max));
    }

  if (option == OPT_RESOLUTION && (s->hw->flags & MUSTEK_FLAG_THREE_PASS))
    {
      /* The three pass scanners use a 0.5% of the maximum resolution
         increment for resolutions less than or equal to half of the
         maximum resolution. The MFS-06000CX uses a 5% of the maximum
         resolution increment for larger resolutions.  The models
         MFS-12000CX and MSF-06000CZ use 1% of the maximum resolution.
         We can't represent this easily in SANE, so the constraint is
         simply for 0.5% and then we round to the 5% or 1% increments
         if necessary.  */
      SANE_Fixed max_dpi, quant, half_res;

      w = *(SANE_Word *) value;
      max_dpi = s->hw->dpi_range.max;
      half_res = max_dpi / 2;

      if (w > half_res)
	{
	  /* quantizize to 1% step*/
	  quant = max_dpi / 100;

	  dpi = (w + quant / 2) / quant;
	  dpi *= quant;
	  if (dpi != w)
	    {
	      *(SANE_Word *) value = dpi;
	      if (info)
		*info |= SANE_INFO_INEXACT;
	    }
	}
	
      DBG(5, "constrain_value: resolution = %.2f (was %.2f)\n", 
	  SANE_UNFIX(*(SANE_Word *) value), SANE_UNFIX(w)); 
    }
  
  return sanei_constrain_value (s->opt + option, value, info);
}

/* Quantize s->req.resolution and return the resolution code for the
   quantized resolution.  Quantization depends on scanner type (single
   pass vs. three-pass) and resolution */
static int
encode_resolution (Mustek_Scanner *s)
{
  SANE_Fixed max_dpi, dpi;
  int code, mode = 0;

  dpi = s->val[OPT_RESOLUTION].w;

  if (!(s->hw->flags & MUSTEK_FLAG_THREE_PASS))
    {
      code = dpi >> SANE_FIXED_SCALE_SHIFT;
    }
  else
    {
      SANE_Fixed quant, half_res;

      max_dpi = s->hw->dpi_range.max;
      half_res = max_dpi / 2;

      if (dpi <= half_res)
	{
	  /* quantizize to 0.5% step */
	  quant = max_dpi / 200;
	}
      else
	{
	  /* quantizize to 1% step*/
	  quant = max_dpi / 100;
	  mode = 0x100;	/* indicate 5% or 1% quantization */
	}

      code = (dpi + quant / 2) / quant;
      if (code < 1)
	code = 1;

    }
  DBG(5, "encode_resolution: code = 0x%x (%d); mode = %x\n", code, code, 
      mode); 
  return code | mode;
}

static int
encode_percentage (Mustek_Scanner *s, double value)
{
  int max, code, sign = 0;

  if (s->hw->flags & MUSTEK_FLAG_THREE_PASS)
    {
      code = (int) ((value / 100.0 * 12) + 12.5);
      max = 0x18;
    }
  else
    {
      if (value < 0.0)
	{
	  value = -value;
	  sign = 0x80;
	}
      code = (int) (value / 100.0 * 127 + 0.5);
      code |= sign;
      max = 0xff;
    }
  if (code > max)
    code = max;
  if (code < 0)
    code = 0x00;
  return code;
}

/* encode halftone pattern type and size */
static SANE_Status
encode_halftone (Mustek_Scanner *s)
{
  SANE_String selection = s->val[OPT_HALFTONE_DIMENSION].s;
  int i = 0;

  while ((halftone_list != 0) 
	 && (strcmp (selection, halftone_list[i]) != 0))
    {
      i++;
    }
  if (halftone_list[i] == 0)
    return SANE_STATUS_INVAL;
  
  if (i < 0x0c) /* standard pattern */
    {
      s->custom_halftone_pattern = SANE_FALSE;
      s->halftone_pattern_type = i;
    }
  else /* custom pattern */
    {
      s->custom_halftone_pattern = SANE_TRUE;
      i -= 0x0c;
      i = 8 - i;
      if (i < 8)
	i--;
      i = i + (i << 4);
      s->halftone_pattern_type = i;
    }
      
  DBG(5, "encode_halftone: %s pattern type %x\n",
      s->custom_halftone_pattern ? "custom" : "standard",
      s->halftone_pattern_type);
  return SANE_STATUS_GOOD;
}

/* Paragon series */
static SANE_Status
area_and_windows (Mustek_Scanner *s)
{
  u_int8_t cmd[117], *cp;
  int i, offset;

  DBG(5, "area_and_windows\n");
  /* setup SCSI command (except length): */
  memset (cmd, 0, sizeof (cmd));
  cmd[0] = MUSTEK_SCSI_AREA_AND_WINDOWS;

  cp = cmd + 6;

  /* The 600 II N v1.01 needs a larger scanarea for line-distance correction */
  offset = 0;
  if ((s->hw->flags & MUSTEK_FLAG_LD_N1) && (s->mode & MUSTEK_MODE_COLOR))
    offset = MAX_LINE_DIST;

  /* fill in frame header: */

  if (s->hw->flags & MUSTEK_FLAG_USE_EIGHTS)
    {
      double eights_per_mm = 8 / MM_PER_INCH;

      /*
       * The MSF-06000CZ seems to lock-up if the pixel-unit is used.
       * Using 1/8" works.
       */
      *cp++ = ((s->mode & MUSTEK_MODE_LINEART) ? 0x00 : 0x01);
      STORE16L(cp, SANE_UNFIX(s->val[OPT_TL_X].w) * eights_per_mm + 0.5);
      STORE16L(cp, SANE_UNFIX(s->val[OPT_TL_Y].w) * eights_per_mm + 0.5);
      STORE16L(cp, SANE_UNFIX(s->val[OPT_BR_X].w) * eights_per_mm + 0.5);
      STORE16L(cp, SANE_UNFIX(s->val[OPT_BR_Y].w) * eights_per_mm + 0.5);
    }
  else
    {
      double pixels_per_mm = SANE_UNFIX (s->hw->dpi_range.max) / MM_PER_INCH;

      if (s->hw->flags & MUSTEK_FLAG_THREE_PASS)
	/* 3pass scanners use 1/2 of the max resolution as base */
	pixels_per_mm /= 2;

      /* pixel unit and halftoning: */  
      *cp++ = 0x8 | ((s->mode & MUSTEK_MODE_LINEART) ? 0x00 : 0x01);

      /* fill in scanning area: */
      STORE16L(cp, SANE_UNFIX (s->val[OPT_TL_X].w) * pixels_per_mm + 0.5);
      STORE16L(cp, SANE_UNFIX (s->val[OPT_TL_Y].w) * pixels_per_mm + 0.5);
      STORE16L(cp, SANE_UNFIX (s->val[OPT_BR_X].w) * pixels_per_mm + 0.5);
      STORE16L(cp, SANE_UNFIX (s->val[OPT_BR_Y].w) * pixels_per_mm + 0.5 +
	       offset);
    }
  
  if (s->custom_halftone_pattern)
    {
      *cp++ = 0x40;			/* mark presence of user pattern */
      *cp++ = s->halftone_pattern_type;	/* set pattern length */
      for (i = 0; i < (s->halftone_pattern_type & 0x0f) * 
	     ((s->halftone_pattern_type >> 4) & 0x0f); ++i)
	*cp++ = s->val[OPT_HALFTONE_PATTERN].wa[i];
    }

  cmd[4] = (cp - cmd) - 6;

  return dev_cmd (s, cmd, (cp - cmd), 0, 0);
}

/* ScanExpress */
static SANE_Status
set_window_se (Mustek_Scanner *s, int lamp)
{
  u_int8_t cmd[58], *cp;
  double pixels_per_mm;
  int offset;

  /* setup SCSI command (except length): */
  memset (cmd, 0, sizeof (cmd));
  cmd[0] = MUSTEK_SCSI_SET_WINDOW;
  cp = cmd + sizeof (scsi_set_window);	/* skip command block		*/

  if (s->mode & MUSTEK_MODE_COLOR)
    {
      /* We have to increase the specified resolution to the next	*/
      /* "standard" resolution due to a firmware bug(?) in color mode	*/
      /* Additionally we must increase the window length slightly to    */
      /* compensate for different line counts for r/g/b			*/
      s->ld.peak_res = SANE_UNFIX (s->hw->dpi_range.max);
      while (((s->ld.peak_res / 2) >= SANE_UNFIX (s->val[OPT_RESOLUTION].w)) &
	     ((s->ld.peak_res / 2) >= 150))
        s->ld.peak_res /= 2;
      offset = MAX_LINE_DIST;	 	/* distance r/b lines		*/
    }
  else  
    {
      s->ld.peak_res = SANE_UNFIX (s->val[OPT_RESOLUTION].w);
      offset = 0;
    }
  DBG(5, "set_window_se: resolution set to %d\n", s->ld.peak_res);      
  STORE16B(cp, 0);			/* window identifier		*/
  STORE16B(cp, s->ld.peak_res);		/* x and y resolution		*/
  STORE16B(cp, 0);			/* not used acc. to specs       */

  pixels_per_mm = SANE_UNFIX (s->hw->dpi_range.max) / MM_PER_INCH;
  /* fill in scanning area, begin and length(!) */
  STORE32B(cp, (SANE_UNFIX (s->val[OPT_TL_X].w) - \
		SANE_UNFIX (s->hw->x_range.min)) * pixels_per_mm + 0.5);
  STORE32B(cp, (SANE_UNFIX (s->val[OPT_TL_Y].w) - \
		SANE_UNFIX (s->hw->y_range.min)) * pixels_per_mm + 0.5);
  STORE32B(cp, SANE_UNFIX (s->val[OPT_BR_X].w - s->val[OPT_TL_X].w) \
               * pixels_per_mm + 0.5); 
  STORE32B(cp, (SANE_UNFIX (s->val[OPT_BR_Y].w - s->val[OPT_TL_Y].w)) \
               * pixels_per_mm + 0.5 + offset); 

  *cp++ = 0x00;				/* brightness, not impl.	*/
  *cp++ = 0x80;				/* threshold, not impl.		*/
  *cp++ = 0x00;				/* contrast, not impl.		*/

  /* Note that 'image composition' has no meaning for the SE series	*/
  /* Mode selection is accomplished solely by bits/pixel (1, 8, 24)	*/
  if (s->mode & MUSTEK_MODE_COLOR)
    {
      *cp++ = 0x05;			/* actually not used!		*/
      *cp++ = 24;			/* 24 bits/pixel in color mode	*/
    }
  else if (s->mode & MUSTEK_MODE_GRAY)
    {
      *cp++ = 0x02;   			/* actually not used!		*/
      *cp++ = 8;			/* 8 bits/pixel in gray mode	*/
    }
  else 
    {
      *cp++ = 0x00;			/* actually not used!		*/
      *cp++ = 1;			/* 1 bit/pixel in lineart mode	*/
    }
          
  cp += 13;				/* skip reserved bytes		*/
  *cp++ = lamp;				/* 0 = normal, 1 = on, 2 = off  */
  cp += 7;				/* skip reserved bytes		*/
  
  cmd[8] = cp - cmd - sizeof (scsi_set_window);
  return dev_cmd (s, cmd, (cp - cmd), 0, 0);
}

/* Pro series */
static SANE_Status
set_window_pro (Mustek_Scanner *s)
{
  u_int8_t cmd[20], *cp;
  double pixels_per_mm;

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = MUSTEK_SCSI_SET_WINDOW;
  cmd[8] = 0x0a;
  cp = cmd + sizeof (scsi_set_window);	/* skip command block		*/

  *cp++ = 0; /* what's this? */
  pixels_per_mm = SANE_UNFIX (s->hw->dpi_range.max) / MM_PER_INCH;

  /* The next for 16 bit values are x0, y0, x1, y1 in pixels at max res */
  STORE16L(cp, (SANE_UNFIX (s->val[OPT_TL_X].w) - \
		SANE_UNFIX (s->hw->x_range.min)) * pixels_per_mm + 0.5);
  STORE16L(cp, (SANE_UNFIX (s->val[OPT_TL_Y].w) - \
		SANE_UNFIX (s->hw->y_range.min)) * pixels_per_mm + 0.5);
  STORE16L(cp, SANE_UNFIX (s->val[OPT_BR_X].w) * pixels_per_mm + 0.5);
  STORE16L(cp, SANE_UNFIX (s->val[OPT_BR_Y].w) * pixels_per_mm + 0.5);
  *cp++ = 0x14; /* what's this? */
  DBG(5, "set_window_pro\n");      

  return dev_cmd (s, cmd, (cp - cmd), 0, 0);
}

/* ScanExpress series */
#if 0
static SANE_Status
calibration (Mustek_Scanner *s)
{
  SANE_Status status;
  u_int8_t cmd[10 + 100000];
  size_t num;

  num = s->hw->cal.bytes * s->hw->cal.lines;
  memset (cmd, 0, sizeof (cmd));

  cmd[0] = MUSTEK_SCSI_READ_DATA;
  cmd[2] = 0x01;
  cmd[6] = (num >> 16) & 0xff;
  cmd[7] = (num >>  8) & 0xff;
  cmd[8] = (num >>  0) & 0xff;

  status = dev_cmd (s, cmd, sizeof (read_data), 
		    cmd + sizeof (read_data), &num); 

  if (status != SANE_STATUS_GOOD)
    {
      DBG(1, "Calibration: read failed\n");
      return status;
    }

  num = s->hw->cal.bytes * s->hw->cal.lines;
  
  cmd[0] = MUSTEK_SCSI_SEND_DATA;
  cmd[2] = 0x01;
  cmd[6] = (num >> 16) & 0xff;
  cmd[7] = (num >>  8) & 0xff;
  cmd[8] = (num >>  0) & 0xff;
  
  status = dev_cmd (s, cmd, sizeof (send_data) + num, 0, 0);		  

  if (status != SANE_STATUS_GOOD)
    {
      DBG(1, "Calibration: send failed\n");
      return status;
    } 	  

  return SANE_STATUS_GOOD;
}  
#endif

/* Pro series */
static SANE_Status
get_calibration_size_pro (Mustek_Scanner *s)
{
  SANE_Status status;
  u_int8_t cmd [6];
  u_int8_t result[6];
  size_t len;

  memset (cmd, 0, sizeof (cmd));
  memset (result, 0, sizeof (result));
  cmd[0] = MUSTEK_SCSI_GET_IMAGE_STATUS;
  cmd[4] = 0x06; /* size of result */
  cmd[5] = 0x80; /* get back buffer size and number of buffers */
  len = sizeof (result);
  status = dev_cmd (s, cmd, sizeof (cmd), result, &len);
  if (status != SANE_STATUS_GOOD)
    return status;
  
  s->hw->cal.bytes = result[1] | (result[2] << 8);
  s->hw->cal.lines = result[3] | (result[4] << 8);

  DBG(4, "get_calibration_size_pro: bytes=%d, lines=%d\n", s->hw->cal.bytes,
      s->hw->cal.lines);
  return SANE_STATUS_GOOD;
}

/* Pro series */
static SANE_Status
get_calibration_lines_pro (Mustek_Scanner *s)
{
  SANE_Status status;
  u_int8_t cmd[10];
  size_t len;
  int line;

  DBG(2, "read_calibration_lines_pro: please wait for warmup\n");
  memset (cmd, 0, sizeof (cmd));
  cmd[0] = MUSTEK_SCSI_READ_DATA;
  len = s->hw->cal.bytes;
  cmd[6] = (len >> 16) & 0xff;
  cmd[7] = (len >>  8) & 0xff;
  cmd[8] = (len >>  0) & 0xff;

  
  for (line = 0; line < s->hw->cal.lines; line++)
    {
      status = dev_cmd (s, cmd, sizeof (scsi_read_data), 
			s->hw->cal.buffer + line * len, &len); 

      if ((status != SANE_STATUS_GOOD) || (len != (unsigned) s->hw->cal.bytes))
	{
	  DBG(1, "get_calibration_lines_pro: read failed\n");
	  return status;
	}
    }
  return SANE_STATUS_GOOD;
}  

/* Pro series */
static SANE_Status
send_calibration_lines_pro (Mustek_Scanner *s)
{
  SANE_Status status;
  u_int8_t *cmd;
  size_t buf_size;

  DBG(5, "send_calibration_lines_pro\n");

  buf_size = s->hw->cal.bytes / 2;
  cmd = (u_int8_t *) malloc (buf_size + sizeof (scsi_send_data));
  if (!cmd)
    {
      DBG(1, "send_calibration_lines_pro: failed to malloc %d bytes for "
	  "sending lines\n", buf_size + sizeof (scsi_send_data));
      return SANE_STATUS_NO_MEM;
    }
  memset (cmd, 0, sizeof (scsi_send_data)); 
  memset (cmd + sizeof (scsi_send_data), 0xff, buf_size); 

  cmd[0] = MUSTEK_SCSI_SEND_DATA;
  cmd[6] = (buf_size >> 16) & 0xff;
  cmd[7] = (buf_size >>  8) & 0xff;
  cmd[8] = (buf_size >>  0) & 0xff;

  cmd[9] = 0; /* first line ? */
  status = dev_cmd (s, cmd, buf_size + 10, 0, 0);		  
  if (status != SANE_STATUS_GOOD)
    {
      DBG(1, "send_calibration_lines_pro: send failed\n");
      return status;
    } 	  

  cmd[9] = 0x80; /* second line ? */
  memset (cmd + sizeof (scsi_send_data), 0x03, buf_size); 
  status = dev_cmd (s, cmd, buf_size + 10, 0, 0);		  
  if (status != SANE_STATUS_GOOD)
    {
      DBG(1, "send_calibration_lines_pro: send failed\n");
      return status;
    } 	  
  free (cmd);
  return SANE_STATUS_GOOD;
}  

/* Pro series */
static SANE_Status
calibration_pro (Mustek_Scanner *s)
{
  SANE_Status status;

  if (s->val[OPT_QUALITY_CAL].w)
    DBG(4, "calibration_pro: doing calibration\n");
  else
    return SANE_STATUS_GOOD;

  status = get_calibration_size_pro (s);
  if (status != SANE_STATUS_GOOD)
    return status;

  s->hw->cal.buffer = (unsigned char *) malloc (s->hw->cal.bytes *
						s->hw->cal.lines);
  if (!s->hw->cal.buffer)
    {
      DBG(1, "calibration_pro: failed to malloc %d bytes for buffer\n",
	  s->hw->cal.bytes * s->hw->cal.lines);
      return SANE_STATUS_NO_MEM;
    }

  status = get_calibration_lines_pro (s);
  if (status != SANE_STATUS_GOOD)
    return status;

  status = send_calibration_lines_pro (s);
  if (status != SANE_STATUS_GOOD)
    return status;

  free (s->hw->cal.buffer);
  return SANE_STATUS_GOOD;
}

/* ScanExpress series */
static SANE_Status
send_gamma_table_se (Mustek_Scanner *s)
{
  SANE_Status status;
  u_int8_t gamma[10 + 4096], *cp;
  int color, factor, val_a, val_b;
  int i, j;
# define CLIP(x)	((x) < 0 ? 0 : ((x) > 255 ? 255 : (x)))

  memset (gamma, 0, sizeof (scsi_send_data));

  gamma[0] = MUSTEK_SCSI_SEND_DATA;
  gamma[2] = 0x03;			/* indicates gamma table */

  if ((s->mode & MUSTEK_MODE_GRAY) || (s->mode & MUSTEK_MODE_COLOR))
    {
      if (s->hw->gamma_length + sizeof (scsi_send_data) > sizeof (gamma))
        return SANE_STATUS_NO_MEM;
      gamma[7] = (s->hw->gamma_length >> 8) & 0xff;
      gamma[8] = (s->hw->gamma_length >> 0) & 0xff;

      factor = s->hw->gamma_length / 256;
      color = (s->mode & MUSTEK_MODE_COLOR) ? 1 : 0;

      do 
        {
          gamma[6] = color;
	  
	  if (color == 0)	
	    {
	      val_a = s->gamma_table[0][1];
	      val_b = s->gamma_table[0][0];
	    }
	  else
	    {
	      /* compose intensity gamma and color channel gamma: */
	      val_a = s->gamma_table[0][s->gamma_table[color][1]];
	      val_b = s->gamma_table[0][s->gamma_table[color][0]];
	    }
	  /* Now val_a is extrapolated from [0] and [1]	     */
	  val_a = MAX( 2 * val_b - val_a, 0);

	  /* Interpolate first entries from 256 entry table  */
	  cp = gamma + sizeof (scsi_send_data);
	  for (j = 0; j < factor; j++)
	    *cp++ = CLIP(((factor - j) * val_a + j * val_b 
			  + factor / 2) / factor);

	  for (i = 1; i < 256; i++)
	    {
	      if (color == 0)
		{
		  val_a = s->gamma_table[0][i-1];
		  val_b = s->gamma_table[0][i];
		}
	      else
		{
		  /* compose intensity gamma and color channel gamma: */
		  val_a = s->gamma_table[0][s->gamma_table[color][i-1]];
		  val_b = s->gamma_table[0][s->gamma_table[color][i]];
		}
	    
	      /* Interpolate next entries from the 256 entry table */
	      for (j = 0; j < factor; j++)
	        *cp++ = CLIP(((factor - j) * val_a + j * val_b 
			      + factor / 2 ) / factor);
	    }  

	  DBG(5, "send_gamma_table_se: sending table for color %d\n",
	      gamma[6]);
	  status = dev_cmd (s, gamma, sizeof (scsi_send_data) 
	  		    + s->hw->gamma_length, 0, 0);
	  ++color;
	}
      while ((color !=1) & (color < 4)	& (status == SANE_STATUS_GOOD));
      
      return status;
    }
  else
    {
      /* In lineart mode the threshold is encoded in byte 8 as follows */ 
      /* brightest -> 00 01 02 ... 7F 80 81 82 ... FF <- darkest image */ 
      gamma[6] = 0x04;
      gamma[8] = 128 - 127 * SANE_UNFIX(s->val[OPT_BRIGHTNESS].w) / 100.0; 
   
      DBG(5, "send_gamma_table_se: sending lineart threshold %2X\n", gamma[8]);
      return dev_cmd (s, gamma, sizeof (scsi_send_data), 0, 0);
    }
}  

/* Paragon series */
static SANE_Status
mode_select_paragon (Mustek_Scanner *s, int color_code)
{
  int speed_code;
  u_int8_t mode[19], *cp;

  /* same goes for speed: */
  for (speed_code = 0; speed_list[speed_code]; ++speed_code)
    {
      if (strcmp (speed_list[speed_code], s->val[OPT_SPEED].s) == 0)
	break;
    }
  if (speed_code > 4)
    speed_code = 4;
  else if (speed_code < 0)
    speed_code = 0;
  if (s->hw->flags & MUSTEK_FLAG_THREE_PASS)
    {
      speed_code = 5 - speed_code;	/* 1 is fast, 5 is slow */
    }
  else
    {
      speed_code = 4 - speed_code;	/* 0 is fast, 4 is slow */
    }
  memset (mode, 0, sizeof (mode));
  mode[0] = MUSTEK_SCSI_MODE_SELECT;

  /* set command length and resolution code: */
  if (s->hw->flags & MUSTEK_FLAG_THREE_PASS)
    {
      mode[4] = 0x0b;
      mode[7] = s->resolution_code;
    }
  else
    {
      mode[4] = 0x0d;
      cp = mode + 17;
      STORE16L(cp, s->resolution_code);
    }
  /* set mode byte: */
  mode[6] = 0x83 | (color_code << 5);
  if (!(s->hw->flags & MUSTEK_FLAG_USE_EIGHTS))
    mode[6] |= 0x08;
  if (s->custom_halftone_pattern)
    mode[6] |= 0x10;
  if (s->hw->flags & MUSTEK_FLAG_PARAGON_2)
    {
      mode[8] =
	encode_percentage (s, SANE_UNFIX (s->val[OPT_BRIGHTNESS].w));
      mode[9] = 
	encode_percentage (s, SANE_UNFIX (s->val[OPT_CONTRAST].w));
      mode[10] = 0;             /* grain */
      mode[11] = 0;		/* speed */
      mode[12] = 0;		/* shadow param not used by Mustek */
      mode[13] = 0;		/* highlight param not used by Mustek */
      mode[14] = 0x5c;		/* paper- */
      mode[15] = 0x00;		/* length */
      mode[16] = 0x41;		/* midtone param not used by Mustek */
    }
  else if (s->hw->flags & MUSTEK_FLAG_THREE_PASS)
    {
      if (s->mode & MUSTEK_MODE_COLOR)
	{
	  mode[8] = encode_percentage 
	    (s, SANE_UNFIX (s->val[OPT_BRIGHTNESS + s->pass + 1].w - 1));
	  mode[9] = encode_percentage
	    (s, SANE_UNFIX (s->val[OPT_CONTRAST + s->pass + 1].w - 1));
	}
      else
	{
	  mode[8] = encode_percentage 
	    (s, SANE_UNFIX (s->val[OPT_BRIGHTNESS].w - 1));
	  mode[9] = encode_percentage
	    (s, SANE_UNFIX (s->val[OPT_CONTRAST].w - 1));
	}
      mode[10] = s->halftone_pattern_type;
      mode[11] = speed_code;	/* lamp setting not supported yet */
      mode[12] = 0;		/* shadow param not used by Mustek */
      mode[13] = 0;		/* highlight param not used by Mustek */
      mode[14] = mode[15] = 0;	/* paperlength not used by Mustek */
      mode[16] = 0;		/* midtone param not used by Mustek */
    }
  else
    {
      mode[8] =
	encode_percentage (s, SANE_UNFIX (s->val[OPT_BRIGHTNESS].w));
      mode[9] = 
	encode_percentage (s, SANE_UNFIX (s->val[OPT_CONTRAST].w));
      mode[10] = s->halftone_pattern_type;
      mode[11] = speed_code;	/* lamp setting not supported yet */
      mode[12] = 0;		/* shadow param not used by Mustek */
      mode[13] = 0;		/* highlight param not used by Mustek */
      mode[14] = mode[15] = 0;	/* paperlength not used by Mustek */
      mode[16] = 0;		/* midtone param not used by Mustek */
    }

  return dev_cmd (s, mode, 6 + mode[4], 0, 0);
}

/* Pro series */
static SANE_Status
mode_select_pro (Mustek_Scanner *s)
{
  u_int8_t mode[19], *cp;

  memset (mode, 0, sizeof (mode));

  mode[0] = MUSTEK_SCSI_MODE_SELECT;
  mode[4] = 0x0d;

  if (s->mode & MUSTEK_MODE_COLOR)
    mode[6] = 0x60;
  else if (s->mode & MUSTEK_MODE_GRAY)
    mode[6] = 0x40;  
  else if (s->mode & MUSTEK_MODE_GRAY_FAST)
    mode[6] = 0x20; 
  else
    mode[6] = 0x00; /* lineart */

  mode[11] = 0x00;  /* what's this? */
  mode[16] = 0x41;  /* what's this? */
  cp = mode + 17;
  STORE16L(cp, s->resolution_code);

  return dev_cmd (s, mode, 6 + mode[4], 0, 0);
}

/* Paragon and Pro series. According to Mustek, the only builtin gamma
   table is a linear table, so all we support here is user-defined
   gamma tables.  */
static SANE_Status
gamma_correction (Mustek_Scanner *s, int color_code)
{
  int i, j, table = 0, len, bytes_per_channel, num_channels = 1;
  u_int8_t gamma[4096+10], val, *cp;  /* for Paragon models 3 x 256 is the
					 maximum. Pro needs 4096 bytes */

  if ((s->hw->flags & MUSTEK_FLAG_N)
      && ((s->mode & MUSTEK_MODE_LINEART) || (s->mode & MUSTEK_MODE_HALFTONE)))
    {
      /* sigh! - the 600 II N needs a (dummy) table download even for
	 lineart and halftone mode, else it produces a completely
	 white image. Thank Mustek for their buggy firmware !  */
      memset (gamma, 0, sizeof (gamma));
      gamma[0] = MUSTEK_SCSI_LOOKUP_TABLE;
      gamma[2] = 0x0;		/* indicate any preloaded gamma table */
      DBG(5, "gamma_correction: sending dummy gamma table\n");
      return dev_cmd (s, gamma, 6, 0, 0);
    }

  if (((s->mode & MUSTEK_MODE_LINEART) || (s->mode & MUSTEK_MODE_HALFTONE))
       && !(s->hw->flags & MUSTEK_FLAG_PRO))
    {
      DBG(5, "gamma_correction: nothing to do in lineart mode -- exiting\n");
      return SANE_STATUS_GOOD;
    }

  if ((!s->val[OPT_CUSTOM_GAMMA].w) && (!(s->hw->flags & MUSTEK_FLAG_PRO)))
    {
      /* Do we need to upload a gamma table even if the user didn't select
	 this option? Some scanners need this work around. */
      if (!(s->hw->flags & MUSTEK_FLAG_FORCE_GAMMA) || 
	  !(s->mode & MUSTEK_MODE_COLOR))
	{
	  DBG(5, "gamma_correction: no custom table selected -- exititing\n");
	  return SANE_STATUS_GOOD;
	}
    }

  if (s->mode & MUSTEK_MODE_COLOR)
    {
      table = 1;
      if (s->hw->flags & MUSTEK_FLAG_THREE_PASS)
	table += s->pass;
      else
	{
	  if ((color_code == MUSTEK_CODE_GRAY) 
	      && !(s->hw->flags & MUSTEK_FLAG_PRO))
	    num_channels = 3;
	  else
	    table = color_code;
	}
    }

  memset (gamma, 0, sizeof (gamma));
  gamma[0] = MUSTEK_SCSI_LOOKUP_TABLE;

  if (s->hw->flags & MUSTEK_FLAG_PRO)
    {
      bytes_per_channel = 4096;
      len = bytes_per_channel;
      if (s->mode == MUSTEK_MODE_COLOR)
	gamma[9] = (color_code << 6); /* color */
      else if ((s->mode == MUSTEK_MODE_GRAY) 
	    || (s->mode == MUSTEK_MODE_GRAY_FAST))
	gamma[9] = 0x80; /* grayscale */
      else /* lineart */
	{
	  gamma[2] = 128 - 127 * SANE_UNFIX(s->val[OPT_BRIGHTNESS].w) / 100.0; 
	  gamma[9] = 0x80; /* grayscale/lineart */
	  DBG(5, "gamma_correction: sending brightness information\n");
	}
    }
  else
    {
      bytes_per_channel = 256;
      gamma[2] = 0x27;		/* indicate user-selected gamma table */
      gamma[9] = (color_code << 6);
      len = num_channels * bytes_per_channel;
    }
  gamma[7] = (len >> 8) & 0xff;	/* big endian! */
  gamma[8] = (len >> 0) & 0xff;

  if (len > 0)
    {
      cp = gamma + 10;
      for (j = 0; j < num_channels; ++j, ++table)
	for (i = 0; i < bytes_per_channel; ++i)
	  {
	    val = s->gamma_table[table][i * 256 / bytes_per_channel];
	       if (s->mode & MUSTEK_MODE_COLOR)
		 /* compose intensity gamma and color channel gamma: */
		 val = s->gamma_table[0][val];
	    *cp++ = val;
	  }
    }
  DBG(5, "gamma_correction: sending gamma table of %d bytes\n", len);
  return dev_cmd (s, gamma, 10 + len, 0, 0);
}

static SANE_Status
send_gamma_table (Mustek_Scanner * s)
{
  SANE_Status status;

  if (s->one_pass_color_scan)
    {
      if (s->hw->flags & MUSTEK_FLAG_N)
	/* This _should_ work for all one-pass scanners (not just
	   AB306N scanners), but it doesn't work for my Paragon
	   600 II SP with firmware rev 1.01.  Too bad, since it would
	   simplify the gamma correction code quite a bit.  */
	status = gamma_correction (s, MUSTEK_CODE_GRAY);
      else
	{
	  status = gamma_correction (s, MUSTEK_CODE_RED);
	  if (status != SANE_STATUS_GOOD)
	    return status;

	  status = gamma_correction (s, MUSTEK_CODE_GREEN);
	  if (status != SANE_STATUS_GOOD)
	    return status;

	  status = gamma_correction (s, MUSTEK_CODE_BLUE);
	}
    }
  else
    status = gamma_correction (s, MUSTEK_CODE_GRAY);
  return status;
}


/* ScanExpress and Paragon series */
static SANE_Status
start_scan (Mustek_Scanner *s)
{
  u_int8_t start[6];

  memset (start, 0, sizeof (start));
  start[0] = MUSTEK_SCSI_START_STOP;
  start[4] = 0x01;

  DBG(4, "start_scan\n");
  /* ScanExpress and Pro models don't have any variants */
  if (!(s->hw->flags & MUSTEK_FLAG_SE) && !(s->hw->flags & MUSTEK_FLAG_PRO) )
    {
      if (s->mode & MUSTEK_MODE_COLOR)
        {
          if (s->hw->flags & MUSTEK_FLAG_THREE_PASS)
	    start[4] |= ((s->pass + 1) << 3);
          else
	    start[4] |= 0x20;
        }
      /* or in single/multi bit: */
      start[4] |= ((s->mode & MUSTEK_MODE_LINEART) 
		   || (s->mode & MUSTEK_MODE_HALFTONE)) ?  0 : (1 << 6);

      if (s->hw->flags & MUSTEK_FLAG_THREE_PASS)
        /* or in expanded resolution bit: */
        start[4] |= (s->resolution_code & 0x100) >> 1;

      /* ??? */
      if ((s->hw->flags & MUSTEK_FLAG_PARAGON_2) && 
	  (s->val[OPT_RESOLUTION].w > (s->hw->dpi_range.max / 2)))
	start[4] |= 1 << 7;

      if ((s->hw->flags & MUSTEK_FLAG_PARAGON_2) 
	  || (s->hw->flags & MUSTEK_FLAG_PARAGON_1))
	/* block mode */
	start[5] = 0x08; 
    }
    
  return dev_cmd (s, start, sizeof (start), 0, 0);
}

static SANE_Status
stop_scan (Mustek_Scanner *s)
{
  DBG(4, "stop_scan\n");
  if (s->hw->flags & MUSTEK_FLAG_PRO)
    scsi_sense_wait_ready (s->fd);
  if ((s->hw->flags & MUSTEK_FLAG_PARAGON_2) 
	  || (s->hw->flags & MUSTEK_FLAG_PARAGON_1))
    {
      while (inquiry (s) == SANE_STATUS_DEVICE_BUSY)
	usleep (500000);
      return sanei_scsi_cmd (s->fd, scsi_test_unit_ready, 
			     sizeof (scsi_test_unit_ready), 0, 0);
    }
  else if (s->hw->flags & MUSTEK_FLAG_THREE_PASS)
    {
      if (s->cancelled)
	return dev_cmd (s, scsi_start_stop, sizeof (scsi_start_stop), 0, 0);
      else
	return SANE_STATUS_GOOD;
    }
  return dev_cmd (s, scsi_start_stop, sizeof (scsi_start_stop), 0, 0);

}

static SANE_Status
do_eof (Mustek_Scanner *s)
{
  if (s->pipe >= 0)
    {
      close (s->pipe);
      s->pipe = -1;
      DBG(5, "do_eof: closing pipe\n");
    }
  return SANE_STATUS_EOF;
}

static SANE_Status
do_stop (Mustek_Scanner *s)
{
  SANE_Status status = SANE_STATUS_GOOD;

  DBG(5, "do_stop\n");

  if (s->cancelled)
    status = SANE_STATUS_CANCELLED;

  s->scanning = SANE_FALSE;
  s->pass = 0;

  if (s->reader_pid > 0)
    {
      int exit_status;
      struct timeval now;
      long int scan_time;
      long int scan_size;

      /* print scanning time */
      gettimeofday (&now, 0);
      scan_time = now.tv_sec - s->start_time;
      if (scan_time < 1)
	scan_time = 1;
      scan_size = s->hw->bpl * s->hw->lines / 1024;
      DBG(2, "Scanning time was %ld seconds, %ld kB/s\n", scan_time,
	  scan_size / scan_time);

      /* ensure child knows it's time to stop: */
      DBG(5, "do_stop: terminating reader process\n");
      kill (s->reader_pid, SIGTERM);
      waitpid (s->reader_pid, &exit_status, 0);
      DBG(5, "do_stop: reader process terminated with status 0x%x\n",
	  exit_status);
      if (status != SANE_STATUS_CANCELLED && WIFEXITED(exit_status))
	status = WEXITSTATUS(exit_status);
      s->reader_pid = 0;
    }

  if (s->fd >= 0)
    {
      if ((status == SANE_STATUS_CANCELLED) && 
	  !(s->hw->flags & MUSTEK_FLAG_N)
	  && !(s->hw->flags & MUSTEK_FLAG_PRO))
	{
	  DBG(5, "do_stop: waiting for scanner to become ready\n");
	  dev_wait_ready (s);
	}
      DBG(5, "do_stop: sending STOP command\n");
      stop_scan (s);
      if (force_wait)
	{
	  DBG(5, "do_stop: waiting for scanner to be ready\n");
	  dev_wait_ready (s);
	}
      DBG(5, "do_stop: closing scanner\n");
      dev_close (s);
      s->fd = -1;
    }

  return status;
}

/* Determine the CCD's distance between the primary color lines.  */
static SANE_Status
line_distance (Mustek_Scanner *s)
{
  int factor, color, res, peak_res;
  SANE_Status status;
  u_int8_t result[5];
  size_t len;

  res = SANE_UNFIX (s->val[OPT_RESOLUTION].w) + 0.5;
  peak_res = SANE_UNFIX (s->hw->dpi_range.max) + 0.5;

  s->ld.buf[0] = NULL;
  
  if (s->hw->flags & MUSTEK_FLAG_LD_MFS)
    {
      /* At least the MFS12000SP (v1.00 + 1.01) scanner needs a
	 special form of line-distance correction and goes wild if
	 they receive an LD command. */
      s->ld.peak_res = res;
      return SANE_STATUS_GOOD;
    }

  len = sizeof (result);
  status = dev_cmd (s, scsi_ccd_distance, sizeof (scsi_ccd_distance),
		    result, &len);
  if (status != SANE_STATUS_GOOD)
    return status;

  DBG(3, "line_distance: got factor=%d, (r/g/b)=(%d/%d/%d)\n",
      result[0] | (result[1] << 8), result[2], result[3], result[4]);

  if (s->hw->flags & MUSTEK_FLAG_LD_FIX)
    {
      result[0] = 0xff;
      result[1] = 0xff;
      if (s->mode & MUSTEK_MODE_COLOR)
	{
	  if (s->hw->flags & MUSTEK_FLAG_N)
	    {
	      /* According to Andreas Czechanowski, the line-distance
		 values returned for the AB306N scanners are
		 garbage, so we have to fix things up manually.  Not
		 good. This is true for firmware 2.00. AB306N scanners
		 with firmware 1.01 don't need this fix. */
	      if (peak_res == 600)
		{
		  if (res < 51)
		    {
		      result[0] = 8; result[1] = 0;
		      result[2] = 0; result[3] = 2; result[4] = 3;
		    }
		  else if (res < 75 || (res > 90 && res < 150))
		    {
		      /* 51-74 and 91-149 dpi: */
		      result[0] = 4; result[1] = 0;
		      result[2] = 0; result[3] = 3; result[4] = 5;
		    }
		  else if (res <= 90 || (res >= 150 && res <= 300))
		    {
		      /* 75-90 and 150-300 dpi: */
		      result[0] = 2; result[1] = 0;
		      result[2] = 0; result[3] = 5; result[4] = 9;
		    }
		  else
		    {
		      /* 301-600 dpi: */
		      result[0] = 1; result[1] = 0;
		      result[2] = 0; result[3] = 9; result[4] = 23;
		    }
		}
	      else
		DBG(1, "don't know how to fix up line-distance for %d dpi\n",
		    peak_res);
	    }
	  else if (!(s->hw->flags & MUSTEK_FLAG_LD_NONE))
	    {
	      if (peak_res == 600)
		{
		  if (res < 51)
		    {
		      /* 1-50 dpi: */
		      result[0] = 4; result[1] = 0;
		      result[2] = 0; result[3] = 3; result[4] = 5;
		    }
		  else if (res <= 300)
		    {
		      /* 51-300 dpi: */
		      result[0] = 2; result[1] = 0;
		      result[2] = 0; result[3] = 5; result[4] = 9;
		    }
		  else
		    {
		      /*301-600 dpi: */
		      result[0] = 1; result[1] = 0;
		      result[2] = 0; result[3] = 9; result[4] = 17;
		    }
		}
	      else if (peak_res == 800)
		{
		  if (res < 72)
		    {
		      /* 1-71 dpi: */
		      result[0] = 4; result[1] = 0;
		      result[2] = 0; result[3] = 3; result[4] = 5;
		    }
		  else if (res <= 400)
		    {
		      /* 72-400 dpi: */
		      result[0] = 2; result[1] = 0;
		      result[2] = 0; result[3] = 9; result[4] = 17;
		    }
		  else
		    {
		      /*401-800 dpi: */
		      result[0] = 1; result[1] = 0;
		      result[2] = 0; result[3] = 16; result[4] = 32;
		    }
		}
	    }
	}
      DBG(4, "line_distance: fixed up to factor=%d, (r/g/b)=(%d/%d/%d)\n",
	  result[0] | (result[1] << 8), result[2], result[3], result[4]);
    }

  factor = result[0] | (result[1] << 8);
  if (factor != 0xffff)
    {
      /* need to do line-distance adjustment ourselves... */

      s->ld.max_value = peak_res;

      if (factor == 0)
	{
	  if (res <= peak_res / 2)
	    res *= 2;
	}
      else
	res *= factor;
      s->ld.peak_res = res;
      for (color = 0; color < 3; ++color)
	{
	  s->ld.dist[color] = result[2 + color];
	  s->ld.quant[color] = s->ld.max_value;
	}

      if ((s->hw->flags & MUSTEK_FLAG_N) 
	  || (s->hw->flags & MUSTEK_FLAG_LD_BLOCK))
	{
	  for (color = 0; color < 3; ++color)
	    s->ld.index[color] = -s->ld.dist[color];
	  s->ld.lmod3 = -1;
	}
      DBG(4, "line_distance: max_value = %d, peak_res = %d, ld.quant = "
	  "(%d, %d, %d)\n", s->ld.max_value, s->ld.peak_res, s->ld.quant[0],
	  s->ld.quant[1], s->ld.quant[2]);
    }
  else
    s->ld.max_value = 0;
    
  return SANE_STATUS_GOOD;
}

/* Paragon + Pro series */
static SANE_Status
get_image_status (Mustek_Scanner *s, SANE_Int *bpl, SANE_Int *lines)
{
  u_int8_t result[6];
  SANE_Status status;
  size_t len;
  int busy, offset;
  long res, half_res;

  /* The 600 II N v1.01 needs a larger scanarea for line-distance correction */
  offset = 0;
  if ((s->hw->flags & MUSTEK_FLAG_LD_N1) && (s->mode & MUSTEK_MODE_COLOR))
    offset = MAX_LINE_DIST * SANE_UNFIX (s->val[OPT_RESOLUTION].w) 
      /  SANE_UNFIX (s->hw->dpi_range.max);
     
  do
    {
      len = sizeof (result);
      status = dev_cmd (s, scsi_get_image_status, 
			sizeof (scsi_get_image_status), result, &len);
      if (status != SANE_STATUS_GOOD)
	return status;

      busy = result[0];
      if (busy)
	usleep (100000);

      if (!s->scanning) /* ? */
	if (!(s->hw->flags & MUSTEK_FLAG_PRO))
	  return do_stop (s);
    }
  while (busy);

  s->hw->bpl = result[1] | (result[2] << 8);
  s->hw->lines = result[3] | (result[4] << 8) | (result[5] << 16);
  
  res = SANE_UNFIX (s->val[OPT_RESOLUTION].w);
  half_res = SANE_UNFIX (s->hw->dpi_range.max) / 2;
  /* Need to interpolate resolutions > max x-resolution? */
  if ((s->hw->flags & MUSTEK_FLAG_ENLARGE_X) &&  (res > half_res))
    {
      *bpl = (s->hw->bpl * res) / half_res;
      DBG(4, "get_image_status: resolution > x-max; enlarge %d bpl to "
	  "%d bpl\n", s->hw->bpl, *bpl);
    }
  else
    *bpl = s->hw->bpl;

  *lines = s->hw->lines - offset;

  DBG(3, "get_image_status: bytes_per_line=%d, lines=%d (offset = %d)\n",
      *bpl, *lines, offset);
  return SANE_STATUS_GOOD;
}

/* ScanExpress models */
static SANE_Status
get_window (Mustek_Scanner *s, SANE_Int *bpl, SANE_Int *lines,
	       SANE_Int *pixels)
{
  u_int8_t result[48];
  SANE_Status status;
  size_t len;
  int color;

  len = sizeof (result);
  status = dev_cmd (s, scsi_get_window, sizeof (scsi_get_window), result, 
		    &len);
  if (status != SANE_STATUS_GOOD)
    return status;

  if (!s->scanning)
    return do_stop (s);

  s->hw->cal.bytes = (result[6] << 24) | (result[7] << 16) |
		     (result[8] <<  8) | (result[9] <<  0);
  s->hw->cal.lines = (result[10] << 24) | (result[11] << 16) |
		     (result[12] <<  8) | (result[13] <<  0);

  DBG(4, "get_window: calib_bytes=%d, calib_lines=%d\n",
      s->hw->cal.bytes, s->hw->cal.lines);

  s->hw->bpl = (result[14] << 24) | (result[15] << 16) |
	       (result[16] <<  8) | result[17];  
  s->hw->lines = (result[18] << 24) | (result[19] << 16) | 
		 (result[20] <<  8) | result[21];

  DBG(4, "get_window: bytes_per_line=%d, lines=%d\n",
      s->hw->bpl, s->hw->lines);
      
  s->hw->gamma_length = 1 << result[26];
  DBG(4, "get_window: gamma length=%d\n",  s->hw->gamma_length);    

  if (s->mode & MUSTEK_MODE_COLOR) 
    {
      long res, half_res;

      s->ld.buf[0] = NULL;
      for (color = 0; color < 3; ++color)
	{
	  s->ld.dist[color] = result[42 + color];
	}
    
      DBG(4, "line_distance: got res=%d, (r/g/b)=(%d/%d/%d)\n",
	  (result[40] << 8) | result[41], s->ld.dist[0], 
	  s->ld.dist[1], s->ld.dist[2]);

      /* In color mode scale the image according to desired resolution */
      res = SANE_UNFIX (s->val[OPT_RESOLUTION].w);
      half_res = SANE_UNFIX (s->hw->dpi_range.max) / 2;
      /* For some SE we must interpolate resolutions > max x-resolution */
      if ((s->hw->flags & MUSTEK_FLAG_ENLARGE_X) &&  (res > half_res))
	{
	  *bpl = *pixels = (((s->hw->bpl / 3) * res) / half_res) * 3;
	  DBG(4, "get_window: resolution > x-max; enlarge %d bpl to "
	      "%d bpl\n", s->hw->bpl, *bpl);
	}
      else
	{
	  *bpl = *pixels = (((s->hw->bpl / 3 ) * res) / s->ld.peak_res) * 3;
	}
      *lines = ((s->hw->lines - s->ld.dist[2]) * res) / s->ld.peak_res;
    }
  else
    {
      /* Linart and gray seems to work with arbitrary resolution */
      /* For some SE we must interpolate resolutions > max x-resolution */
      if ((s->hw->flags & MUSTEK_FLAG_ENLARGE_X) && 
	  (SANE_UNFIX (s->val[OPT_RESOLUTION].w) 
	   > (SANE_UNFIX (s->hw->dpi_range.max) / 2)))
	{
	  *bpl = s->hw->bpl * (SANE_UNFIX (s->val[OPT_RESOLUTION].w) / 
			       (SANE_UNFIX (s->hw->dpi_range.max) / 2)); 
	  DBG(4, "get_window: resolution > x-max; enlarge %d bpl to %d "
	      "bpl\n", s->hw->bpl, *bpl);
	}
      else
	{
	  *bpl = s->hw->bpl;
	}
      *lines = s->hw->lines;
    }   
  return SANE_STATUS_GOOD;
}

static SANE_Status
adf_and_backtrack (Mustek_Scanner *s)
{
  u_int8_t backtrack[6];
  int code = 0x80;

  if (s->val[OPT_BACKTRACK].w)
    code |= 0x02;

  if (strcmp (s->val[OPT_SOURCE].s, "Automatic Document Feeder") == 0)
    code |= 0x01;
  else if (strcmp (s->val[OPT_SOURCE].s, "Transparency Adapter") == 0)
    code |= 0x04;
  memset (backtrack, 0, sizeof (backtrack));
  backtrack[0] = MUSTEK_SCSI_ADF_AND_BACKTRACK;
  backtrack[4] = code;
  
  DBG(4, "adf_and_backtrack: backtrack: %s; ADF: %s; TA: %s\n",
      code & 0x02 ? "yes" : "no", code & 0x01 ? "yes" : "no",
      code & 0x04 ? "yes" : "no");
  return dev_cmd (s, backtrack, sizeof (backtrack), 0, 0);
}

/* MFS-12000SP f/w 1.00 and 1.01 */
static void
fix_line_distance_mfs (Mustek_Scanner * s, int num_lines, int bpl,
		       u_int8_t *raw, u_int8_t *out)
{
  u_int8_t *out_ptr, *ptr, *ptr_end, *src;
  u_int y, dy;
  int bpc;
# define RED	0
# define GRN	1	/* green, spelled funny */


/* MFS scanner have three separate sensor bars (one per primary color)
   and these sensor bars are vertically 1/72" apart from each other.
   So when scanning at a resolution of RES dots/inch, then the first
   red strip goes with the green strip that is dy=round(RES/72)
   further down and the blue strip that is 2*dy further down.  */
  bpc = bpl / 3;
  dy = (s->ld.peak_res + 36) / 72; 

  if (!s->ld.buf[RED])
    {
      /* The red buffer must be able to hold up to 2*dy lines whereas
	 the green buffer must be able to hold up to 1*dy lines. */
      s->ld.buf[RED] = malloc (3 * dy * (long) bpc);
      s->ld.buf[GRN] = s->ld.buf[RED] + 2 * dy * bpc;
      memset (s->ld.buf[RED], 0, 3 * dy * (long) bpc);
      
      DBG(5, "fix_line_distance_mfs: malloc and clear.\n");
    }

  /* restore the red and green lines from the previous buffer: */
  for (y = 0; y < 2*dy && y < (u_int) num_lines; ++y)
    {
      ptr = s->ld.buf[RED] + y*bpc;
      ptr_end = ptr + bpc;
      out_ptr = out + y*bpl + 0;
      while (ptr != ptr_end)
	{
	  *out_ptr = *ptr++;
	  out_ptr += 3;
	}
    }		         
  for (y = 0; y < dy && y < (u_int) num_lines; ++y)
    {
      ptr = s->ld.buf[GRN] + y*bpc;
      ptr_end = ptr + bpc;
      out_ptr = out + y*bpl + 1;
      while (ptr != ptr_end)
	{
	  *out_ptr = *ptr++;
	  out_ptr += 3;
	}
    }	

  for (y = 0; y < (u_int) num_lines; ++y)
    {
      if (y >= 2*dy)
	{
	  ptr = raw + (y - 2*dy)*bpl + 0*bpc;
	  ptr_end = ptr + bpc;
	  out_ptr = out + y*bpl + 0;
	  while (ptr != ptr_end)
	    {
	      *out_ptr = *ptr++;
	      out_ptr += 3;
	    }	      	  	      
	}
      if (y >= 1*dy)
	{
	  ptr = raw + (y - 1*dy)*bpl + 1*bpc;
	  ptr_end = ptr + bpc;
	  out_ptr = out + y*bpl + 1;
	  while (ptr != ptr_end)
	    {
	      *out_ptr = *ptr++;
	      out_ptr += 3;
	    }	      	  	      
	}
      /* if (y >= 0*dy) */
	{
	  ptr = raw + (y - 0*dy)*bpl + 2*bpc;
	  ptr_end = ptr + bpc;
	  out_ptr = out + y*bpl + 2;
	  while (ptr != ptr_end)
	    {
	      *out_ptr = *ptr++;
	      out_ptr += 3;
	    }	      	  	      
	}
    }

  /* save red and green lines: */
  for (y = 0; y < 2*dy; ++y)
    {
      if (num_lines + y >= 2*dy)
	src = raw + (num_lines - 2*dy + y)*bpl;
      else
	src = s->ld.buf[RED] + (y + num_lines)*bpc;
      memcpy (s->ld.buf[RED] + y*bpc, src, bpc);
    }
  for (y = 0; y < 1*dy; ++y)
    {
      if (num_lines + y >= 2*dy)
	src = raw + (num_lines - 1*dy + y)*bpl + bpc;
      else
	src = s->ld.buf[GRN] + (y + num_lines)*bpc;
      memcpy (s->ld.buf[GRN] + y*bpc, src, bpc);
    }
}

/* 600 II N firmware 2.x */
static int
fix_line_distance_n_2 (Mustek_Scanner *s, int num_lines, int bpl,
			u_int8_t *raw, u_int8_t *out)
{
  u_int8_t *out_end, *out_ptr, *raw_end = raw + num_lines * bpl;
  int c, num_saved_lines, line;

  if (!s->ld.buf[0])
    {
      /* This buffer must be big enough to hold maximum line distance
         times max_bpl bytes.  The maximum line distance for the 
	 Paragon 600 II N scanner is 23, so 40 should be safe.  */
      DBG(5, "fix_line_distance_n_2: allocating temp buffer of %d*%d bytes\n",
	  MAX_LINE_DIST, bpl);
      s->ld.buf[0] = malloc (MAX_LINE_DIST * (long) bpl);
      if (!s->ld.buf[0])
	{
	  DBG(1, "fix_line_distance_n_2: failed to malloc temporary buffer\n");
	  return 0;
	}
    }

  num_saved_lines = s->ld.index[0] - s->ld.index[2];
  if (num_saved_lines > 0)
    /* restore the previously saved lines: */
    memcpy (out, s->ld.buf[0], num_saved_lines * bpl);

  while (1)
    {
      if (++s->ld.lmod3 >= 3)
	s->ld.lmod3 = 0;

      c = color_seq[s->ld.lmod3];
      if (s->ld.index[c] < 0)
	++s->ld.index[c];
      else if (s->ld.index[c] < s->params.lines)
	{
	  s->ld.quant[c] += s->ld.peak_res;
	  if (s->ld.quant[c] > s->ld.max_value)
	    {
	      s->ld.quant[c] -= s->ld.max_value;
	      line = s->ld.index[c]++ - s->ld.ld_line;
	      out_ptr = out + line * bpl + c;
	      out_end = out_ptr + bpl;
	      while (out_ptr != out_end)
		{
		  *out_ptr = *raw++;
		  out_ptr += 3;
		}		  			      	       	      

	      if (raw >= raw_end)
		{
		  DBG (3, "fix_line_distance_n_2: lmod3=%d, "
		       "index=(%d,%d,%d)\n", s->ld.lmod3,
		       s->ld.index[0], s->ld.index[1], s->ld.index[2]);
		  num_lines = s->ld.index[2] - s->ld.ld_line;

		  /* copy away the lines with at least one missing
		     color component, so that we can interleave them
		     with new scan data on the next call */
		  num_saved_lines = s->ld.index[0] - s->ld.index[2];
		  memcpy (s->ld.buf[0], out + num_lines * bpl,
			  num_saved_lines * bpl);

		  /* notice the number of lines we processed */
		  s->ld.ld_line = s->ld.index[2];
		  /* return number of complete (r+g+b) lines */
		  return num_lines;
		}
	    }
	}
    }
}

/* 600 II N firmware 1.x */
static int
fix_line_distance_n_1 (Mustek_Scanner *s, int num_lines, int bpl,
			  u_int8_t *raw, u_int8_t *out)
{
  u_int8_t *out_end, *out_ptr, *raw_end = raw + num_lines * bpl;
  int c, num_saved_lines, line;

  /* For firmware 1.x the scanarea must be soemwhat bigger than needed
     because of the linedistance correction */
  
  if (!s->ld.buf[0])
    {
      /* This buffer must be big enough to hold maximum line distance
         times max_bpl bytes.  The maximum line distance for the 600 II N 
         is 23, so 40 is safe. */
      DBG(5, "fix_line_distance_n_1: allocating temp buffer of %d*%d bytes\n",
	  MAX_LINE_DIST, bpl);
      s->ld.buf[0] = malloc (MAX_LINE_DIST * (long) bpl);
      if (!s->ld.buf[0])
	{
	  DBG(1, "fix_line_distance_n_1: failed to malloc temporary buffer\n");
	  return 0;
	}
    }
  num_saved_lines = s->ld.index[0] - s->ld.index[1];
  DBG(5, "fix_line_distance_n_1: got %d lines, %d bpl\n", num_lines, bpl);
  DBG(5, "fix_line_distance_n_1: num_saved_lines = %d; peak_res = %d; "
       "max_value = %d\n", num_saved_lines, s->ld.peak_res, s->ld.max_value);
  if (num_saved_lines > 0)
    /* restore the previously saved lines: */
    memcpy (out, s->ld.buf[0], num_saved_lines * bpl);

  while (1)
    {
      if (++s->ld.lmod3 >= 3)
	s->ld.lmod3 = 0;
      c = s->ld.lmod3;
      if (s->ld.index[c] < 0)
	++s->ld.index[c];
      else
	{
	  s->ld.quant[c] += s->ld.peak_res;
	  if (s->ld.quant[c] > s->ld.max_value)
	    {
	      s->ld.quant[c] -= s->ld.max_value;
	      line = s->ld.index[c]++ - s->ld.ld_line;
	      out_ptr = out + line * bpl + c;
	      out_end = out_ptr + bpl;
	      while (out_ptr != out_end)
		{
		  *out_ptr = *raw++;
		  out_ptr += 3;
		}		
	      DBG (5, "fix_line_distance_n_1: copied line %d (color %d)\n",
		   line, c);
	    }
	}      
      if ((raw >= raw_end) || ((s->ld.index[0] >= s->params.lines) &&
			       (s->ld.index[1] >= s->params.lines) &&
			       (s->ld.index[2] >= s->params.lines)))
	{
	  DBG (3, "fix_line_distance_n_1: lmod3=%d, index=(%d,%d,%d)%s\n",
	       s->ld.lmod3,
	       s->ld.index[0], s->ld.index[1], s->ld.index[2],
	       raw >= raw_end ? " raw >= raw_end" : "");
	  num_lines = s->ld.index[1] - s->ld.ld_line;
	  if (num_lines < 0)
	    num_lines = 0;
	  DBG (4, "fix_line_distance_n_1: lines ready: %d\n", num_lines);
	  
	  /* copy away the lines with at least one missing
	     color component, so that we can interleave them
	     with new scan data on the next call */
	  num_saved_lines = s->ld.index[0] - s->ld.index[1];
	  DBG (4, "fix_line_distance_n_1: copied %d lines to "
	       "ld.buf\n", num_saved_lines);
	  memcpy (s->ld.buf[0], out + num_lines * bpl,
		  num_saved_lines * bpl);	  
	  /* notice the number of lines we processed */
	  s->ld.ld_line = s->ld.index[1];
	  if (s->ld.ld_line < 0)
	    s->ld.ld_line = 0;
	  /* return number of complete (r+g+b) lines */
	  return num_lines;
	}
      
    }
}


/* For ScanExpress models */
static int
fix_line_distance_se (Mustek_Scanner *s, int num_lines, int bpl,
		      u_int8_t *raw, u_int8_t *out)
{
  u_int8_t *raw_end = raw + num_lines * bpl;
  u_int8_t *out_ptr[3], *ptr;
  int index[3], lines[3], quant[3];
  int color, pixel, res, scale;
  int bpc = bpl / 3; 		/* bytes per color (per line) */

  res = SANE_UNFIX (s->val[OPT_RESOLUTION].w);
  
  if (!s->ld.buf[0])
    {
      /* This buffer must be big enough to hold maximum line distance times
         3*bpl bytes.  The maximum line distance for 1200 dpi is 32 */
      DBG(5, "fix_line_distance_se: allocating temp buffer of %d*%d bytes\n",
	  3 * MAX_LINE_DIST, bpc);
      s->ld.buf[0] = malloc (3 * MAX_LINE_DIST * (long) bpc);

      if (!s->ld.buf[0])
	{
	  DBG(1, "fix_line_distance_se: failed to malloc temporary buffer\n");
	  return 0;
	}

      /* Note that either s->ld.buf[1] or  s->ld.buf[2] is never used. */
      s->ld.buf[1] = s->ld.buf[2] = 
		     s->ld.buf[0] + 2 * MAX_LINE_DIST * (long) bpc;

      /* Since the blocks don't start necessarily with red note color. */
      s->ld.color = 0;
      
      /* The scan area must be longer than desired because of the line
         distance. So me must count complete (r+g+b) lines already
         submitted to the fronted. */
      s->ld.ld_line = s->params.lines;
      
      for (color = 0; color < 3; ++color)
	{
	  s->ld.index[color] = - s->ld.dist[color];
	  s->ld.quant[color] = 0;
	  s->ld.saved[color] = 0;
	}
    }
  
  num_lines *= 3;
  DBG(5, "fix_line_distance_se: start color: %d\n", s->ld.color);
  DBG(5, "read lines: %d\n", num_lines);

  /* First scan the lines read and count red, green and blue ones. 
     Since we will step thru the lines a second time we must not
     alter any global variables here! */
  for (color = 0; color < 3; ++color)
    {
      index[color] = s->ld.index[color];
      lines[color] = s->ld.saved[color];
      quant[color] = s->ld.quant[color];  
    }

  color = s->ld.color;
  while (num_lines > 0)
    {
      if (index[color] < 0)
	++index[color];
      else
	{
	  quant[color] += res;
	  if (quant[color] >= s->ld.peak_res)
	    {
	      /* This line must be processed, not dropped. */
	      quant[color] -= s->ld.peak_res;
	      ++lines[color];
	    }	      
	  --num_lines;
	}
      if (++color > 2) color = 0;
     }		                                                    
      
  /* Calculate how many triples of color lines we can output now. 
     Because the number of available red lines is always greater 
     than for the other colors we may ignore the red ones here. */
  num_lines = MIN( lines[1], lines[2]);  

  DBG(5, "fix_line_distance_se: saved lines: %d/%d/%d\n", s->ld.saved[0],
      s->ld.saved[1], s->ld.saved[2]); 
  DBG(5, "fix_line_distance_se: available:  %d/%d/%d\n", lines[0], lines[1],
      lines[2]);
  DBG(5, "fix_line_distance_se: triples: %d\n", num_lines);

  lines[0] = lines[1] = lines[2] = num_lines;
  
  /* Output the color lines saved in previous call first.  
     Note that data is converted in r/g/b interleave on the fly. */
  for (color = 0; color < 3; ++color) 
    {
      out_ptr[color] = out + color;
      ptr = s->ld.buf[color];
      while ((s->ld.saved[color] > 0) && (lines[color] > 0)) 
	{
	  scale = 0;
	  /* need to enlarge x-resolution? */
	  if ((s->hw->flags & MUSTEK_FLAG_ENLARGE_X) && 
	      (s->val[OPT_RESOLUTION].w > (s->hw->dpi_range.max / 2)))
	    {
	      int half_res = SANE_UNFIX (s->hw->dpi_range.max) / 2;
	      u_int8_t *ptr_start = ptr;
	      for (pixel = 0; pixel < s->params.pixels_per_line; ++pixel)
		{
		  *out_ptr[color] = *ptr;
		  out_ptr[color] += 3;
		  scale += half_res;
		  if (scale >= half_res)
		    {
		      scale -= res;
		      ++ptr;
		    }
		}
	      DBG(5, "fix_line_distance_se: saved: color: %d; raw bytes: %d; "
		  "out bytes: %d\n", color, ptr - ptr_start, 
		  s->params.pixels_per_line);
	      ptr = ptr_start + bpc;
	    }
	  else
	    {
	      for (pixel = 0; pixel < bpc; ++pixel)
		{
		  scale += res;
		  if (scale >= s->ld.peak_res)
		    {
		      scale -= s->ld.peak_res;
		      *out_ptr[color] = *ptr;
		      out_ptr[color] += 3;
		    }
		  ++ptr;				 
		}
	    }
	  --(s->ld.saved[color]);
	  --lines[color];
	}  	
      if (s->ld.saved[color] > 0) 
	memmove(s->ld.buf[color], ptr, s->ld.saved[color] * bpc);     	
    }
      
  while (1)
    {
      if (s->ld.index[s->ld.color] < 0)
	++(s->ld.index[s->ld.color]);
      else
	{
	  s->ld.quant[s->ld.color] += res;
	  if (s->ld.quant[s->ld.color] >= s->ld.peak_res)
	    {
	      /* This line must be processed, not dropped. */
	      s->ld.quant[s->ld.color] -= s->ld.peak_res;

	      if (lines[s->ld.color] > 0)
		{
		  /* There's still a line to be output for current color.
		     Then shuffle current color line to output buffer. */
		  scale = 0;
		  /* need to enlarge x-resolution? */
		  if ((s->hw->flags & MUSTEK_FLAG_ENLARGE_X) && 
		      (s->val[OPT_RESOLUTION].w > (s->hw->dpi_range.max / 2)))
		    {
		      int half_res = SANE_UNFIX (s->hw->dpi_range.max) / 2;
		      u_int8_t *raw_start = raw;
		      for (pixel = 0; pixel < s->params.pixels_per_line;
			   ++pixel)
			{
			  *out_ptr[s->ld.color] = *raw;
			  out_ptr[s->ld.color] += 3;
			  scale += half_res;
			  if (scale >= half_res)
			    {
			      scale -= res;
			      ++raw;
			    }
			}
		      DBG(5, "fix_line_distance_se: color: %d; raw bytes: %d; "
			  "out bytes: %d\n",  s->ld.color, raw - raw_start, 
			  s->params.pixels_per_line);
		      raw = raw_start + bpc;
		    }
		  else
		    {
		      for (pixel = 0; pixel < bpc; ++pixel)
			{
			  scale += res;
			  if (scale >= s->ld.peak_res)
			    {
			      scale -= s->ld.peak_res;
			      *out_ptr[s->ld.color] = *raw;
			      out_ptr[s->ld.color] += 3;
			    }
			  ++raw;				 
			}
		    }
		  --lines[s->ld.color];
		}
	      else
	        {
		  /* At least one component missing, so save this line. */
		  memcpy(s->ld.buf[s->ld.color] + s->ld.saved[s->ld.color]
			 * bpc, raw, bpc);
		  ++(s->ld.saved[s->ld.color]);
		  raw += bpc;
		}
	    }
	  else  
	    raw += bpc;			         	

	  if (raw >= raw_end) 
	    {
	      /* Reduce num_lines if we encounter excess lines. */
	      if (num_lines > s->ld.ld_line)
	        num_lines = s->ld.ld_line;
	      s->ld.ld_line -= num_lines;

	      if (++s->ld.color > 2) s->ld.color = 0;
	      return num_lines; 
	    }
	}
      if (++s->ld.color > 2) s->ld.color = 0;
    }
}


/* For Pro models. Not really a linedistance correction (they don't need one)
   only enlarging x-res here */
static void
fix_line_distance_pro (Mustek_Scanner *s, int num_lines, int bpl,
		       u_int8_t *raw, u_int8_t *out)
{
  u_int8_t *raw_end = raw + num_lines * bpl;
  u_int8_t *out_ptr, *ptr, *ptr_start;
  int pixel, res, half_res, scale;

  res = SANE_UNFIX (s->val[OPT_RESOLUTION].w);
  half_res = SANE_UNFIX (s->hw->dpi_range.max) / 2;
  
  /* need to enlarge x-resolution? */
  if ((s->hw->flags & MUSTEK_FLAG_ENLARGE_X) && (res > half_res))
    {
      ptr = raw;
      out_ptr = out;
      while (ptr < raw_end)
	{
	  ptr_start = ptr;
	  scale = 0;
	  
	  for (pixel = 0; pixel < s->params.pixels_per_line; ++pixel)
	    {
	      *(out_ptr++) = *ptr;
	      *(out_ptr++) = *(ptr + 1);
	      *(out_ptr++) = *(ptr + 2);
	      scale += half_res;
	      if (scale >= half_res)
		{
		  scale -= res;
		  ptr += 3;
		}
	    }
	  DBG(5, "fix_line_distance_pro: raw bytes: %d; out bytes: %d\n",
	      ptr - ptr_start,  s->params.pixels_per_line);
	  ptr = ptr_start + bpl;
	}
    }
  else
    memcpy (out, raw, num_lines * bpl);
  return;
}

/* For MFS-08000SP, MFS-06000SP. MFC-08000CZ, MFC-06000CZ */
static void
fix_line_distance_normal (Mustek_Scanner *s, int num_lines, int bpl,
			  u_int8_t *raw, u_int8_t *out)
{
  u_int8_t *out_end, *out_ptr, *raw_end = raw + num_lines * bpl;
  int index[3];	/* index of the next output line for color C */
  int i, color;

  /* Initialize the indices with the line distances that were returned
     by the CCD linedistance command or set manually (option
     linedistance-fix).  We want to skip data for the first OFFSET
     rounds, so we initialize the indices to the negative of this
     offset.  */

  DBG(5, "fix_line_distance_normal: %d lines, %d bpl\n", num_lines, bpl);

  for (color = 0; color < 3; ++color)
    index[color] = -s->ld.dist[color];

  while (1)
    {
      for (i = 0; i < 3; ++i)
	{
	  color = color_seq[i];
	  if (index[color] < 0)
	    ++index[color];
	  else if (index[color] < num_lines)
	    {
	      s->ld.quant[color] += s->ld.peak_res;
	      if (s->ld.quant[color] > s->ld.max_value)
		{
		  s->ld.quant[color] -= s->ld.max_value;
		  out_ptr = out + index[color] * bpl + color;
		  out_end = out_ptr + bpl;
		  while (out_ptr != out_end)
		    {
		      *out_ptr = *raw++;
		      out_ptr += 3;
		    }
		  ++index[color];
		  if (raw >= raw_end)
		    return;
		}
	    }
	}
    }
}

/* Paragon series II */
static int
fix_line_distance_block (Mustek_Scanner *s, int num_lines, int bpl,
			  u_int8_t *raw, u_int8_t *out)
{
  u_int8_t *out_end, *out_ptr, *raw_end = raw + num_lines * bpl;
  int c, num_saved_lines, line;

  if (!s->ld.buf[0])
    {
      DBG(5, "fix_line_distance_block: allocating temp buffer of %d*%d "
	  "bytes\n", MAX_LINE_DIST, bpl);
      s->ld.buf[0] = malloc (MAX_LINE_DIST * (long) bpl);
      if (!s->ld.buf[0])
	{
	  DBG(1, "fix_line_distance_block: failed to malloc temporary "
	      "buffer\n");
	  return 0;
	}
    }
  num_saved_lines = s->ld.index[0] - s->ld.index[2];
  if ((num_saved_lines < 0) || (s->ld.index[0] == 0))
    num_saved_lines = 0;
  /* restore the previously saved lines: */
  memcpy (out, s->ld.buf[0], num_saved_lines * bpl);
  DBG (5, "fix_line_distance_block: copied %d lines from "
       "ld.buf to buffer\n", num_saved_lines);
  while (1)
    {
      if (++s->ld.lmod3 >= 3)
	s->ld.lmod3 = 0;

      c = color_seq[s->ld.lmod3];
      if (s->ld.index[c] < 0)
	++s->ld.index[c];
      else if (s->ld.index[c] < s->hw->lines_per_block)
	{
	  s->ld.quant[c] += s->ld.peak_res;
	  if (s->ld.quant[c] > s->ld.max_value)
	    {
	      s->ld.quant[c] -= s->ld.max_value;
	      line = s->ld.index[c]++ - s->ld.ld_line;
	      out_ptr = out + line * bpl + c;
	      out_end = out_ptr + bpl;
	      while (out_ptr != out_end)
		{
		  *out_ptr = *raw++;
		  out_ptr += 3;
		}		  			      	       	      
	      DBG (5, "fix_line_distance_block: copied line %d (color %d)\n",
		   line, c);
	      if (raw >= raw_end)
		{
		  num_lines = s->ld.index[2] - s->ld.ld_line;
		  if (num_lines < 0)
		    num_lines = 0;
		  
		  /* copy away the lines with at least one missing
		     color component, so that we can interleave them
		     with new scan data on the next call */
		  num_saved_lines = s->ld.index[0] - s->ld.index[2];
		  memcpy (s->ld.buf[0], out + num_lines * bpl,
			  num_saved_lines * bpl);
		  DBG (5, "fix_line_distance_block: copied %d lines to "
		       "ld.buf\n", num_saved_lines);

		  /* notice the number of lines we processed */
		  s->ld.ld_line = s->ld.index[2];
		  if (s->ld.ld_line <0 )
		    s->ld.ld_line = 0;

		  DBG (4, "fix_line_distance_block: lmod3=%d, "
		       "index=(%d,%d,%d), line = %d, lines = %d\n",
		       s->ld.lmod3,
		       s->ld.index[0], s->ld.index[1], s->ld.index[2],
		       s->ld.ld_line, num_lines);
		  /* return number of complete (r+g+b) lines */
		  return num_lines;
		}
	    }
	}
    }
}

static SANE_Status
init_options (Mustek_Scanner *s)
{
  int i, j, gammasize;

  memset (s->opt, 0, sizeof (s->opt));
  memset (s->val, 0, sizeof (s->val));

  for (i = 0; i < NUM_OPTIONS; ++i)
    {
      s->opt[i].size = sizeof (SANE_Word);
      s->opt[i].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    }

  s->opt[OPT_NUM_OPTS].title = SANE_TITLE_NUM_OPTIONS;
  s->opt[OPT_NUM_OPTS].desc = SANE_DESC_NUM_OPTIONS;
  s->opt[OPT_NUM_OPTS].type = SANE_TYPE_INT;
  s->opt[OPT_NUM_OPTS].cap = SANE_CAP_SOFT_DETECT;
  s->val[OPT_NUM_OPTS].w = NUM_OPTIONS;

  /* "Mode" group: */

  s->opt[OPT_MODE_GROUP].title = "Scan Mode";
  s->opt[OPT_MODE_GROUP].desc = "";
  s->opt[OPT_MODE_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_MODE_GROUP].cap = 0;
  s->opt[OPT_MODE_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* scan mode */
  s->opt[OPT_MODE].name = SANE_NAME_SCAN_MODE;
  s->opt[OPT_MODE].title = SANE_TITLE_SCAN_MODE;
  s->opt[OPT_MODE].desc = SANE_DESC_SCAN_MODE;
  s->opt[OPT_MODE].type = SANE_TYPE_STRING;
  s->opt[OPT_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  if (s->hw->flags & MUSTEK_FLAG_SE)
    {
      s->opt[OPT_MODE].size = max_string_size (mode_list_se);
      s->opt[OPT_MODE].constraint.string_list = mode_list_se;
      s->val[OPT_MODE].s = strdup (mode_list_se[1]);
    }
  else if (s->hw->flags & MUSTEK_FLAG_PRO)
    {
      s->opt[OPT_MODE].size = max_string_size (mode_list_pro);
      s->opt[OPT_MODE].constraint.string_list = mode_list_pro;
      s->val[OPT_MODE].s = strdup (mode_list_pro[1]);
    }
  else
    {
      s->opt[OPT_MODE].size = max_string_size (mode_list_paragon);
      s->opt[OPT_MODE].constraint.string_list = mode_list_paragon;
      s->val[OPT_MODE].s = strdup (mode_list_paragon[2]);
    }

  /* resolution */
  s->opt[OPT_RESOLUTION].name = SANE_NAME_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].type = SANE_TYPE_FIXED;
  s->opt[OPT_RESOLUTION].unit = SANE_UNIT_DPI;
  s->opt[OPT_RESOLUTION].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_RESOLUTION].constraint.range = &s->hw->dpi_range;
  s->val[OPT_RESOLUTION].w = MAX(SANE_FIX(18), s->hw->dpi_range.min);

  /* speed */
  s->opt[OPT_SPEED].name = SANE_NAME_SCAN_SPEED;
  s->opt[OPT_SPEED].title = SANE_TITLE_SCAN_SPEED;
  s->opt[OPT_SPEED].desc = SANE_DESC_SCAN_SPEED;
  s->opt[OPT_SPEED].type = SANE_TYPE_STRING;
  s->opt[OPT_SPEED].size = max_string_size (speed_list);
  s->opt[OPT_SPEED].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_SPEED].constraint.string_list = speed_list;
  s->val[OPT_SPEED].s = strdup (speed_list[4]);

  /* source */
  s->opt[OPT_SOURCE].name = SANE_NAME_SCAN_SOURCE;
  s->opt[OPT_SOURCE].title = SANE_TITLE_SCAN_SOURCE;
  s->opt[OPT_SOURCE].desc = SANE_DESC_SCAN_SOURCE;
  s->opt[OPT_SOURCE].type = SANE_TYPE_STRING;

  if ((s->hw->flags & MUSTEK_FLAG_SE) || (s->hw->flags & MUSTEK_FLAG_N) 
      || (s->hw->flags & MUSTEK_FLAG_TA))
    {
      s->opt[OPT_SOURCE].size = max_string_size (ta_source_list);
      s->opt[OPT_SOURCE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
      s->opt[OPT_SOURCE].constraint.string_list = ta_source_list;
      s->val[OPT_SOURCE].s = strdup (ta_source_list[0]);
    }
  else if (s->hw->flags & MUSTEK_FLAG_ADF)
    {
      s->opt[OPT_SOURCE].size = max_string_size (adf_source_list);
      s->opt[OPT_SOURCE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
      s->opt[OPT_SOURCE].constraint.string_list = adf_source_list;
      s->val[OPT_SOURCE].s = strdup (adf_source_list[0]);
    }
  else
    {
      s->opt[OPT_SOURCE].size = max_string_size (source_list);
      s->opt[OPT_SOURCE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
      s->opt[OPT_SOURCE].constraint.string_list = source_list;
      s->val[OPT_SOURCE].s = strdup (source_list[0]);
      s->opt[OPT_SOURCE].cap |= SANE_CAP_INACTIVE;
    }


  /* backtrack */
  s->opt[OPT_BACKTRACK].name = SANE_NAME_BACKTRACK;
  s->opt[OPT_BACKTRACK].title = SANE_TITLE_BACKTRACK;
  s->opt[OPT_BACKTRACK].desc = SANE_DESC_BACKTRACK;
  s->opt[OPT_BACKTRACK].type = SANE_TYPE_BOOL;
  s->val[OPT_BACKTRACK].w = SANE_TRUE;
  
  /* preview */
  s->opt[OPT_PREVIEW].name = SANE_NAME_PREVIEW;
  s->opt[OPT_PREVIEW].title = SANE_TITLE_PREVIEW;
  s->opt[OPT_PREVIEW].desc = SANE_DESC_PREVIEW;
  s->opt[OPT_PREVIEW].cap = SANE_CAP_SOFT_DETECT | SANE_CAP_SOFT_SELECT;
  s->val[OPT_PREVIEW].w = 0;

  /* gray preview */
  s->opt[OPT_GRAY_PREVIEW].name = SANE_NAME_GRAY_PREVIEW;
  s->opt[OPT_GRAY_PREVIEW].title = SANE_TITLE_GRAY_PREVIEW;
  s->opt[OPT_GRAY_PREVIEW].desc = SANE_DESC_GRAY_PREVIEW;
  s->opt[OPT_GRAY_PREVIEW].type = SANE_TYPE_BOOL;
  s->val[OPT_GRAY_PREVIEW].w = SANE_FALSE;

  /* "Geometry" group: */

  s->opt[OPT_GEOMETRY_GROUP].title = "Geometry";
  s->opt[OPT_GEOMETRY_GROUP].desc = "";
  s->opt[OPT_GEOMETRY_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_GEOMETRY_GROUP].cap = SANE_CAP_ADVANCED;
  s->opt[OPT_GEOMETRY_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* top-left x */
  s->opt[OPT_TL_X].name = SANE_NAME_SCAN_TL_X;
  s->opt[OPT_TL_X].title = SANE_TITLE_SCAN_TL_X;
  s->opt[OPT_TL_X].desc = SANE_DESC_SCAN_TL_X;
  s->opt[OPT_TL_X].type = SANE_TYPE_FIXED;
  s->opt[OPT_TL_X].unit = SANE_UNIT_MM;
  s->opt[OPT_TL_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TL_X].constraint.range = &s->hw->x_range;
  s->val[OPT_TL_X].w = s->hw->x_range.min;

  /* top-left y */
  s->opt[OPT_TL_Y].name = SANE_NAME_SCAN_TL_Y;
  s->opt[OPT_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
  s->opt[OPT_TL_Y].desc = SANE_DESC_SCAN_TL_Y;
  s->opt[OPT_TL_Y].type = SANE_TYPE_FIXED;
  s->opt[OPT_TL_Y].unit = SANE_UNIT_MM;
  s->opt[OPT_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TL_Y].constraint.range = &s->hw->y_range;
  s->val[OPT_TL_Y].w = s->hw->y_range.min;

  /* bottom-right x */
  s->opt[OPT_BR_X].name = SANE_NAME_SCAN_BR_X;
  s->opt[OPT_BR_X].title = SANE_TITLE_SCAN_BR_X;
  s->opt[OPT_BR_X].desc = SANE_DESC_SCAN_BR_X;
  s->opt[OPT_BR_X].type = SANE_TYPE_FIXED;
  s->opt[OPT_BR_X].unit = SANE_UNIT_MM;
  s->opt[OPT_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_X].constraint.range = &s->hw->x_range;
  s->val[OPT_BR_X].w = s->hw->x_range.max;

  /* bottom-right y */
  s->opt[OPT_BR_Y].name = SANE_NAME_SCAN_BR_Y;
  s->opt[OPT_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
  s->opt[OPT_BR_Y].desc = SANE_DESC_SCAN_BR_Y;
  s->opt[OPT_BR_Y].type = SANE_TYPE_FIXED;
  s->opt[OPT_BR_Y].unit = SANE_UNIT_MM;
  s->opt[OPT_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_Y].constraint.range = &s->hw->y_range;
  s->val[OPT_BR_Y].w = s->hw->y_range.max;

  /* "Enhancement" group: */

  s->opt[OPT_ENHANCEMENT_GROUP].title = "Enhancement";
  s->opt[OPT_ENHANCEMENT_GROUP].desc = "";
  s->opt[OPT_ENHANCEMENT_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_ENHANCEMENT_GROUP].cap = 0;
  s->opt[OPT_ENHANCEMENT_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* brightness */
  s->opt[OPT_BRIGHTNESS].name = SANE_NAME_BRIGHTNESS;
  s->opt[OPT_BRIGHTNESS].title = SANE_TITLE_BRIGHTNESS;
  s->opt[OPT_BRIGHTNESS].desc = SANE_DESC_BRIGHTNESS
    "  For one-pass scanners this option is active for lineart/halftone"
    " modes only.  For multibit modes (grey/color) use the gamma-table(s).";
  s->opt[OPT_BRIGHTNESS].type = SANE_TYPE_FIXED;
  s->opt[OPT_BRIGHTNESS].unit = SANE_UNIT_PERCENT;
  s->opt[OPT_BRIGHTNESS].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BRIGHTNESS].constraint.range = &percentage_range;
  if (!s->hw->flags & MUSTEK_FLAG_THREE_PASS)
    /* 1-pass scanners don't support brightness in multibit mode */
    s->opt[OPT_BRIGHTNESS].cap |= SANE_CAP_INACTIVE;
  s->val[OPT_BRIGHTNESS].w = 0;

  /* brightness red */
  s->opt[OPT_BRIGHTNESS_R].name = SANE_NAME_BRIGHTNESS "_r";
  s->opt[OPT_BRIGHTNESS_R].title = SANE_TITLE_BRIGHTNESS " red";
  s->opt[OPT_BRIGHTNESS_R].desc = SANE_DESC_BRIGHTNESS " Red channel.";
  s->opt[OPT_BRIGHTNESS_R].type = SANE_TYPE_FIXED;
  s->opt[OPT_BRIGHTNESS_R].unit = SANE_UNIT_PERCENT;
  s->opt[OPT_BRIGHTNESS_R].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BRIGHTNESS_R].constraint.range = &percentage_range;
  s->opt[OPT_BRIGHTNESS_R].cap |= SANE_CAP_INACTIVE;
  s->val[OPT_BRIGHTNESS_R].w = 0;

  /* brightness green */
  s->opt[OPT_BRIGHTNESS_G].name = SANE_NAME_BRIGHTNESS "_g";
  s->opt[OPT_BRIGHTNESS_G].title = SANE_TITLE_BRIGHTNESS " green";
  s->opt[OPT_BRIGHTNESS_G].desc = SANE_DESC_BRIGHTNESS " Green channel.";
  s->opt[OPT_BRIGHTNESS_G].type = SANE_TYPE_FIXED;
  s->opt[OPT_BRIGHTNESS_G].unit = SANE_UNIT_PERCENT;
  s->opt[OPT_BRIGHTNESS_G].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BRIGHTNESS_G].constraint.range = &percentage_range;
  s->opt[OPT_BRIGHTNESS_G].cap |= SANE_CAP_INACTIVE;
  s->val[OPT_BRIGHTNESS_G].w = 0;

  /* brightness blue */
  s->opt[OPT_BRIGHTNESS_B].name = SANE_NAME_BRIGHTNESS "_b";
  s->opt[OPT_BRIGHTNESS_B].title = SANE_TITLE_BRIGHTNESS " blue";
  s->opt[OPT_BRIGHTNESS_B].desc = SANE_DESC_BRIGHTNESS " Blue channel.";
  s->opt[OPT_BRIGHTNESS_B].type = SANE_TYPE_FIXED;
  s->opt[OPT_BRIGHTNESS_B].unit = SANE_UNIT_PERCENT;
  s->opt[OPT_BRIGHTNESS_B].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BRIGHTNESS_B].constraint.range = &percentage_range;
  s->opt[OPT_BRIGHTNESS_B].cap |= SANE_CAP_INACTIVE;
  s->val[OPT_BRIGHTNESS_B].w = 0;

  /* contrast */
  s->opt[OPT_CONTRAST].name = SANE_NAME_CONTRAST;
  s->opt[OPT_CONTRAST].title = SANE_TITLE_CONTRAST;
  s->opt[OPT_CONTRAST].desc = SANE_DESC_CONTRAST
    "  For one-pass scanners this option is active for lineart/halftone"
    " modes only.  For multibit modes (grey/color) use the gamma-table(s).";
  s->opt[OPT_CONTRAST].type = SANE_TYPE_FIXED;
  s->opt[OPT_CONTRAST].unit = SANE_UNIT_PERCENT;
  s->opt[OPT_CONTRAST].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_CONTRAST].constraint.range = &percentage_range;
  if (!(s->hw->flags & MUSTEK_FLAG_THREE_PASS))
    /* 1-pass scanners don't support brightness in multibit mode */
    s->opt[OPT_CONTRAST].cap |= SANE_CAP_INACTIVE;
  s->val[OPT_CONTRAST].w = 0;

  /* contrast red */
  s->opt[OPT_CONTRAST_R].name = SANE_NAME_CONTRAST "_r";
  s->opt[OPT_CONTRAST_R].title = SANE_TITLE_CONTRAST " red";
  s->opt[OPT_CONTRAST_R].desc = SANE_DESC_CONTRAST " Red channel.";
  s->opt[OPT_CONTRAST_R].type = SANE_TYPE_FIXED;
  s->opt[OPT_CONTRAST_R].unit = SANE_UNIT_PERCENT;
  s->opt[OPT_CONTRAST_R].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_CONTRAST_R].constraint.range = &percentage_range;
  s->opt[OPT_CONTRAST_R].cap |= SANE_CAP_INACTIVE;
  s->val[OPT_CONTRAST_R].w = 0;

  /* contrast green */
  s->opt[OPT_CONTRAST_G].name = SANE_NAME_CONTRAST "_g";
  s->opt[OPT_CONTRAST_G].title = SANE_TITLE_CONTRAST " green";
  s->opt[OPT_CONTRAST_G].desc = SANE_DESC_CONTRAST " Green channel.";
  s->opt[OPT_CONTRAST_G].type = SANE_TYPE_FIXED;
  s->opt[OPT_CONTRAST_G].unit = SANE_UNIT_PERCENT;
  s->opt[OPT_CONTRAST_G].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_CONTRAST_G].constraint.range = &percentage_range;
  s->opt[OPT_CONTRAST_G].cap |= SANE_CAP_INACTIVE;
  s->val[OPT_CONTRAST_G].w = 0;

  /* contrast blue */
  s->opt[OPT_CONTRAST_B].name = SANE_NAME_CONTRAST "_b";
  s->opt[OPT_CONTRAST_B].title = SANE_TITLE_CONTRAST " blue";
  s->opt[OPT_CONTRAST_B].desc = SANE_DESC_CONTRAST " Blue channel.";
  s->opt[OPT_CONTRAST_B].type = SANE_TYPE_FIXED;
  s->opt[OPT_CONTRAST_B].unit = SANE_UNIT_PERCENT;
  s->opt[OPT_CONTRAST_B].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_CONTRAST_B].constraint.range = &percentage_range;
  s->opt[OPT_CONTRAST_B].cap |= SANE_CAP_INACTIVE;
  s->val[OPT_CONTRAST_B].w = 0;

  /* gamma */
    gammasize = 256;
  for (i = 0; i < 4; ++i)
    for (j = 0; j < gammasize; ++j)
      s->gamma_table[i][j] = j;

  /* custom-gamma table */
  s->opt[OPT_CUSTOM_GAMMA].name = SANE_NAME_CUSTOM_GAMMA;
  s->opt[OPT_CUSTOM_GAMMA].title = SANE_TITLE_CUSTOM_GAMMA;
  s->opt[OPT_CUSTOM_GAMMA].desc = SANE_DESC_CUSTOM_GAMMA;
  s->opt[OPT_CUSTOM_GAMMA].type = SANE_TYPE_BOOL;
  s->val[OPT_CUSTOM_GAMMA].w = SANE_FALSE;

  /* grayscale gamma vector */
  s->opt[OPT_GAMMA_VECTOR].name = SANE_NAME_GAMMA_VECTOR;
  s->opt[OPT_GAMMA_VECTOR].title = SANE_TITLE_GAMMA_VECTOR;
  s->opt[OPT_GAMMA_VECTOR].desc = SANE_DESC_GAMMA_VECTOR;
  s->opt[OPT_GAMMA_VECTOR].type = SANE_TYPE_INT;
  s->opt[OPT_GAMMA_VECTOR].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_GAMMA_VECTOR].unit = SANE_UNIT_NONE;
  s->opt[OPT_GAMMA_VECTOR].size = 256 * sizeof (SANE_Word);
  s->val[OPT_GAMMA_VECTOR].wa = &s->gamma_table[0][0];
  s->opt[OPT_GAMMA_VECTOR].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_GAMMA_VECTOR].constraint.range = &u8_range;

  /* red gamma vector */
  s->opt[OPT_GAMMA_VECTOR_R].name = SANE_NAME_GAMMA_VECTOR_R;
  s->opt[OPT_GAMMA_VECTOR_R].title = SANE_TITLE_GAMMA_VECTOR_R;
  s->opt[OPT_GAMMA_VECTOR_R].desc = SANE_DESC_GAMMA_VECTOR_R;
  s->opt[OPT_GAMMA_VECTOR_R].type = SANE_TYPE_INT;
  s->opt[OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_GAMMA_VECTOR_R].unit = SANE_UNIT_NONE;
  s->opt[OPT_GAMMA_VECTOR_R].size = 256 * sizeof (SANE_Word);
  s->val[OPT_GAMMA_VECTOR_R].wa = &s->gamma_table[1][0];
  s->opt[OPT_GAMMA_VECTOR_R].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_GAMMA_VECTOR_R].constraint.range = &u8_range;

  /* green gamma vector */
  s->opt[OPT_GAMMA_VECTOR_G].name = SANE_NAME_GAMMA_VECTOR_G;
  s->opt[OPT_GAMMA_VECTOR_G].title = SANE_TITLE_GAMMA_VECTOR_G;
  s->opt[OPT_GAMMA_VECTOR_G].desc = SANE_DESC_GAMMA_VECTOR_G;
  s->opt[OPT_GAMMA_VECTOR_G].type = SANE_TYPE_INT;
  s->opt[OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_GAMMA_VECTOR_G].unit = SANE_UNIT_NONE;
  s->opt[OPT_GAMMA_VECTOR_G].size = 256 * sizeof (SANE_Word);
  s->val[OPT_GAMMA_VECTOR_G].wa = &s->gamma_table[2][0];
  s->opt[OPT_GAMMA_VECTOR_G].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_GAMMA_VECTOR_G].constraint.range = &u8_range;

  /* blue gamma vector */
  s->opt[OPT_GAMMA_VECTOR_B].name = SANE_NAME_GAMMA_VECTOR_B;
  s->opt[OPT_GAMMA_VECTOR_B].title = SANE_TITLE_GAMMA_VECTOR_B;
  s->opt[OPT_GAMMA_VECTOR_B].desc = SANE_DESC_GAMMA_VECTOR_B;
  s->opt[OPT_GAMMA_VECTOR_B].type = SANE_TYPE_INT;
  s->opt[OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_GAMMA_VECTOR_B].unit = SANE_UNIT_NONE;
  s->opt[OPT_GAMMA_VECTOR_B].size = 256 * sizeof (SANE_Word);
  s->val[OPT_GAMMA_VECTOR_B].wa = &s->gamma_table[3][0];
  s->opt[OPT_GAMMA_VECTOR_B].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_GAMMA_VECTOR_B].constraint.range = &u8_range;

  /* quality calibration */
  s->opt[OPT_QUALITY_CAL].name = SANE_NAME_QUALITY_CAL;
  s->opt[OPT_QUALITY_CAL].title = SANE_TITLE_QUALITY_CAL;
  s->opt[OPT_QUALITY_CAL].desc = SANE_DESC_QUALITY_CAL;
  s->opt[OPT_QUALITY_CAL].type = SANE_TYPE_BOOL;
  s->val[OPT_QUALITY_CAL].w = SANE_FALSE;
  s->opt[OPT_QUALITY_CAL].cap |= SANE_CAP_INACTIVE;

  /* halftone dimension */
  s->opt[OPT_HALFTONE_DIMENSION].name = SANE_NAME_HALFTONE_DIMENSION;
  s->opt[OPT_HALFTONE_DIMENSION].title = SANE_TITLE_HALFTONE_DIMENSION;
  s->opt[OPT_HALFTONE_DIMENSION].desc = SANE_DESC_HALFTONE_DIMENSION;
  s->opt[OPT_HALFTONE_DIMENSION].type = SANE_TYPE_STRING;
  s->opt[OPT_HALFTONE_DIMENSION].size = max_string_size (halftone_list);
  s->opt[OPT_HALFTONE_DIMENSION].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_HALFTONE_DIMENSION].constraint.string_list = halftone_list;
  s->val[OPT_HALFTONE_DIMENSION].s = strdup (halftone_list[0]);
  s->opt[OPT_HALFTONE_DIMENSION].cap |= SANE_CAP_INACTIVE;

  /* halftone pattern */
  s->opt[OPT_HALFTONE_PATTERN].name = SANE_NAME_HALFTONE_PATTERN;
  s->opt[OPT_HALFTONE_PATTERN].title = SANE_TITLE_HALFTONE_PATTERN;
  s->opt[OPT_HALFTONE_PATTERN].desc = SANE_DESC_HALFTONE_PATTERN;
  s->opt[OPT_HALFTONE_PATTERN].type = SANE_TYPE_INT;
  s->opt[OPT_HALFTONE_PATTERN].size = 0;
  s->opt[OPT_HALFTONE_PATTERN].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_HALFTONE_PATTERN].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_HALFTONE_PATTERN].constraint.range = &u8_range;
  s->val[OPT_HALFTONE_PATTERN].wa = s->halftone_pattern;

  if ((s->hw->flags & MUSTEK_FLAG_SE) || (s->hw->flags & MUSTEK_FLAG_N))
    {
      /* SE and N models don't support speed and backtrack */
      s->opt[OPT_SPEED].cap |= SANE_CAP_INACTIVE;
      s->opt[OPT_BACKTRACK].cap |= SANE_CAP_INACTIVE;
    }

  if (s->hw->flags & MUSTEK_FLAG_PRO)
    {
      /* Pro models don't support speed */
      s->opt[OPT_SPEED].cap |= SANE_CAP_INACTIVE;
      s->opt[OPT_QUALITY_CAL].cap &= ~SANE_CAP_INACTIVE;
    }

  return SANE_STATUS_GOOD;
}

/* The following three functions execute as a child process.  The
   reason for using a subprocess is that some (most?) generic SCSI
   interfaces block a SCSI request until it has completed.  With a
   subprocess, we can let it block waiting for the request to finish
   while the main process can go about to do more important things
   (such as recognizing when the user presses a cancel button).

   WARNING: Since this is executed as a subprocess, it's NOT possible
   to update any of the variables in the main process (in particular
   the scanner state cannot be updated).

   NOTE: At least for Linux, it seems that we could get rid of the
   subprocess.  Linux v2.0 does seem to allow select() on SCSI
   descriptors.  */

static void
output_data (Mustek_Scanner *s, FILE *fp,
	     SANE_Byte *data, int lines_per_buffer, int bpl,
	     SANE_Byte *extra)
{
  SANE_Byte *ptr, *ptr_end;
  int y, num_lines;

  DBG(5, "output_data: data=%p, lpb=%d, bpl=%d, extra=%p\n",
      data, lines_per_buffer, bpl, extra);

  /* convert to pixel-interleaved format: */
  if ((s->mode & MUSTEK_MODE_COLOR)
      && !(s->hw->flags & MUSTEK_FLAG_THREE_PASS)) 
    {
      num_lines = lines_per_buffer;

      /* need to correct for distance between r/g/b sensors: */
      if (s->hw->flags & MUSTEK_FLAG_PRO)
	fix_line_distance_pro (s, num_lines, bpl, data, extra);
      else if (s->hw->flags & MUSTEK_FLAG_SE)
	num_lines = fix_line_distance_se (s, num_lines, bpl, data, extra);
      else if (s->hw->flags & MUSTEK_FLAG_LD_MFS) 
	fix_line_distance_mfs (s, num_lines, bpl, data, extra);
      else if (s->hw->flags & MUSTEK_FLAG_N) 
	{
	  if (s->hw->flags & MUSTEK_FLAG_LD_N2)
	    num_lines = fix_line_distance_n_2 (s, num_lines, bpl, data,
					       extra);
	  else
	    num_lines = fix_line_distance_n_1 (s, num_lines, bpl, data,
					       extra);
	}  
      else if ((s->hw->flags & MUSTEK_FLAG_LD_BLOCK) && (s->ld.max_value != 0))
	num_lines = fix_line_distance_block (s, num_lines, bpl, data, extra);
      else if (!(s->hw->flags & MUSTEK_FLAG_LD_NONE) && (s->ld.max_value != 0))
	fix_line_distance_normal (s, num_lines, bpl, data, extra);
      else
        {
	  /* Just shuffle around while copying from *data to *extra */ 
          SANE_Byte *red_ptr, *grn_ptr, *blu_ptr;
          
          ptr = extra;
          red_ptr = data;
          for (y = 0; y < num_lines; ++y)
            {
	      grn_ptr = red_ptr + bpl / 3;
	      blu_ptr = grn_ptr + bpl / 3;
	      ptr_end = red_ptr + bpl;            

	      while (blu_ptr != ptr_end)
		{
		  *ptr++ = *red_ptr++;
		  *ptr++ = *grn_ptr++;
		  *ptr++ = *blu_ptr++;
		}
	      red_ptr = ptr_end;
	    }
	}

      if (strcmp (s->val[OPT_SOURCE].s, "Automatic Document Feeder") == 0)
	{
	  /* need to revert line direction */
	  int line_number;
	  int byte_number;
	  
	  DBG(5, "output_data: ADF found, mirroring lines\n");
	  for (line_number = 0; line_number < num_lines;
	       line_number++)
	    {
	      for (byte_number = bpl - 3;  byte_number >= 0; 
		   byte_number -= 3)
		{
		  fputc (*(extra + line_number * bpl + byte_number), fp); 
		  fputc (*(extra + line_number * bpl + byte_number + 1), fp); 
		  fputc (*(extra + line_number * bpl + byte_number + 2), fp); 
		}
	    }
	}
      else
	fwrite (extra, num_lines, s->params.bytes_per_line, fp);
    }
  else
    {
      DBG(5, "output_data: write %d lpb; %d bpl\n", lines_per_buffer, bpl);
      /* Scale x-resolution above 1/2 of the maximum resolution for 
	 SE and Pro scanners */
      if ((s->hw->flags & MUSTEK_FLAG_ENLARGE_X) && 
	  (s->val[OPT_RESOLUTION].w > (s->hw->dpi_range.max / 2)))
	{
	  int x;
	  int half_res = SANE_UNFIX (s->hw->dpi_range.max) / 2;
	  int res = SANE_UNFIX (s->val[OPT_RESOLUTION].w);
	  int res_counter;
	  int enlarged_x;
	  
	  DBG(5, "output_data: enlarge lines from %d bpl to %d bpl\n", 
	      s->hw->bpl, s->params.bytes_per_line);

	  for (y = 0; y < lines_per_buffer; y++)
	    {
	      unsigned char byte = 0;

	      x = 0;
	      res_counter = 0;
	      enlarged_x = 0;

	      while (enlarged_x < s->params.pixels_per_line)
		{
		  if ((s->mode & MUSTEK_MODE_GRAY) 
		      || (s->mode & MUSTEK_MODE_GRAY_FAST))
		    {
		      fputc (*(data + y * bpl + x), fp);
		      res_counter += half_res;
		      if (res_counter >= half_res)
			{
			  res_counter -= res;
			  x++;
			}
		      enlarged_x++;
		    }
		  else /* lineart */
		    {
		      if (*(data + x / 8 + y * bpl) & (1 << (7 - (x % 8))))
			byte |= 1 << (7 - (enlarged_x % 8));

		      if ((enlarged_x % 8) == 7)
			{
			  fputc (~byte, fp); /* invert image */
			  byte = 0;
			}
		      res_counter += half_res;
		      if (res_counter >= half_res)
			{
			  res_counter -= res;
			  x++;
			}
		      enlarged_x++;
		    }
		}
	    }
	}
      else /* lineart, gray or halftone (nothing to scale) */
	{
	  if ((s->mode & MUSTEK_MODE_LINEART) 
	      || (s->mode & MUSTEK_MODE_HALFTONE))
	    {
	      /* in singlebit mode, the scanner returns 1 for black. ;-( */
	      ptr = data;
	      ptr_end = ptr + lines_per_buffer * bpl;
	      
	      if (strcmp (s->val[OPT_SOURCE].s, 
			  "Automatic Document Feeder") == 0)
		{
		  while (ptr != ptr_end)
		    {
		      *ptr++ = ~*ptr;
		      /* need to revert bit direction */
		      *ptr= ((*ptr & 0x80) >> 7) + ((*ptr & 0x40) >> 5)
			+ ((*ptr & 0x20) >> 3) + ((*ptr & 0x10) >> 1) 
			+ ((*ptr & 0x08) << 1) + ((*ptr & 0x04) << 3) 
			+ ((*ptr & 0x02) << 5) + ((*ptr & 0x01) << 7);
		    }
		}
	      else
		while (ptr != ptr_end)
		  *ptr++ = ~*ptr;
	    }
	  if (strcmp (s->val[OPT_SOURCE].s, "Automatic Document Feeder") == 0)
	    {
	      /* need to revert line direction */
	      int line_number;
	      int byte_number;
	      
	      DBG(5, "output_data: ADF found, mirroring lines\n");
	      for (line_number = 0; line_number < lines_per_buffer;
		   line_number++)
		{
		  for (byte_number = bpl - 1; byte_number >= 0;
		       byte_number--)
		    {
		      fputc (*(data + line_number * bpl + byte_number), fp); 
		    }
		}
	    }
	  else
	    {
	      fwrite (data, lines_per_buffer, bpl, fp);
	    }
	}
    }
  DBG(5, "output_data: end\n");
}

static RETSIGTYPE
sigterm_handler (int signal)
{
  sanei_scsi_req_flush_all ();		/* flush SCSI queue */
  DBG(5, "sigterm_handler: signal %d\n", signal);
  _exit (SANE_STATUS_GOOD);
}

static int
reader_process (Mustek_Scanner *s, int fd)
{
  int lines_per_buffer, bpl;
  SANE_Byte *extra = 0, *ptr;
  sigset_t sigterm_set;
  struct SIGACTION act;
  SANE_Status status;
  FILE *fp;
  int buffer_count, max_buffers;

  struct  
    {
      void *id;	          /* scsi queue id */
      SANE_Byte *data;    /* data buffer */
      int lines;          /* # lines in buffer */
      size_t num_read;    /* # of bytes read (return value) */
      int bank;           /* needed by SE models */
    } bstat;

  sigemptyset (&sigterm_set);
  sigaddset (&sigterm_set, SIGTERM);

  fp = fdopen (fd, "w");
  if (!fp)
    return SANE_STATUS_IO_ERROR;

  bpl = s->hw->bpl;

  /* buffer size is scanner dependant */
  lines_per_buffer = s->hw->buffer_size / bpl;
    
  if (strip_height > 0.0) 
    {
      int max_lines;
      double dpi;

      dpi = SANE_UNFIX (s->val[OPT_RESOLUTION].w);
      max_lines = (int) (strip_height * dpi + 0.5);

      if (lines_per_buffer > max_lines)
	{
	  DBG(2, "reader_process: limiting strip height to %g inches "
	      "(%d lines)\n", strip_height, max_lines);
	  lines_per_buffer = max_lines;
	}
    }

  if (!lines_per_buffer)
    {
      DBG(1, "reader_process: bpl (%d) > SCSI buffer size / 2 (%d)\n",
	  bpl, s->hw->buffer_size / 2);
      return SANE_STATUS_NO_MEM;		/* resolution is too high */
    }

  DBG(4, "reader_process: %d lines per buffer, %d bytes per line, "
      "%d bytes total\n",  lines_per_buffer, bpl, lines_per_buffer * bpl);

  bstat.data = malloc (lines_per_buffer * (long) bpl);
  if (!bstat.data)
    {
      DBG(1, "reader_process: failed to malloc %ld bytes for buffer\n",
	  lines_per_buffer * (long) bpl);
      return SANE_STATUS_NO_MEM;
    }
  /* Touch all pages of the buffer to fool the memory management. */ 
  ptr = bstat.data + lines_per_buffer * (long) bpl - 1;
  while (ptr >= bstat.data)
    {
      *ptr = 0x00;
      ptr -= 256;
    }   

  if (!(s->hw->flags & MUSTEK_FLAG_THREE_PASS))
    {
      /* get temporary buffer for line-distance correction and/or bit
	 expansion. For some scanners more space is needed because the
	 data must be read in as single big block (cut up into pieces
	 of lines_per_buffer). This requires that the line distance
	 correction continues on every call exactly where it stopped
	 if the image shall be reconstructed without any stripes. */

      extra = malloc ((lines_per_buffer + MAX_LINE_DIST) 
		      * (long) s->params.bytes_per_line);
      if (!extra)
	{	
	  DBG(1, "reader_process: failed to malloc extra buffer\n");
	  return SANE_STATUS_NO_MEM;
	}
    }

  memset (&act, 0, sizeof (act));
  act.sa_handler = sigterm_handler;
  sigaction (SIGTERM, &act, 0);

  if (s->hw->flags & MUSTEK_FLAG_N)
    {
      /* reacquire port access rights (lost because of fork()): */
      sanei_ab306_get_io_privilege (s->fd);
    }

  if ((s->hw->flags & MUSTEK_FLAG_N) || (s->hw->flags & MUSTEK_FLAG_LD_BLOCK))
    {
      /* reset counter of line number for line-dictance correction */
      s->ld.ld_line = 0;
    }

  max_buffers = s->hw->max_block_buffer_size / (lines_per_buffer * bpl);
  DBG(4, "reader_process: limiting block read to %d buffers (%d lines)\n",
      max_buffers, MIN(s->hw->lines, (max_buffers * lines_per_buffer)));

  while (s->line < s->hw->lines)
    {
      s->hw->lines_per_block = 
	MIN(s->hw->lines - s->line, (max_buffers * lines_per_buffer));
      if (s->line == 0)
	status = dev_block_read_start (s, s->hw->lines_per_block, SANE_TRUE);
      else
	status = dev_block_read_start (s, s->hw->lines_per_block, SANE_FALSE);
      if (status != SANE_STATUS_GOOD)
	return status;

      buffer_count = 0;

      while ((buffer_count < max_buffers) && (s->line < s->hw->lines))
	{
	  if (s->line + lines_per_buffer >= s->hw->lines)
	    {
	      /* do the last few lines: */
	      bstat.lines = s->hw->lines - s->line;
	      bstat.bank = 0x01;
	    }
	  else 
	    {
	      bstat.lines = lines_per_buffer;
	      bstat.bank = 0x00;    
	    }
	  
	  s->line += bstat.lines;
	  
	  DBG(4, "reader_process: entering read request for %d "
	      "bytes (buffer %d)\n",  bstat.lines * bpl, buffer_count);
	  fflush (stderr);
	  sigprocmask (SIG_BLOCK, &sigterm_set, 0); 
	  status = dev_read_req_enter (s, bstat.data, bstat.lines, bpl,
				       &bstat.num_read, &bstat.id, bstat.bank);
	  sigprocmask (SIG_UNBLOCK, &sigterm_set, 0); 
	  
	  if (status == SANE_STATUS_GOOD)
	    {
	      DBG(5, "reader_process: entered (line %d of %d,"
		  " buffer %d)\n", s->line, s->hw->lines, buffer_count);
	    }
	  else
	    {
	      DBG(1, "reader_process: failed to enter read "
		  "request, status: %s\n", sane_strstatus (status));
	      return status;
	    }

	  DBG(4, "reader_process: waiting for request to be ready\n");
	  status = dev_req_wait (bstat.id);
	  if (status == SANE_STATUS_GOOD)
	    {
	      DBG(4, "reader_process: buffer is ready, wanted %d, "
		  "got %d bytes\n", bstat.lines * bpl, bstat.num_read);
	    }
	  else
	    {
	      DBG(1, "reader_process: failed to read data, status: %s\n",
		  sane_strstatus (status));
	      return status;
	    }
	  
	  DBG(4, "reader_process: sending %d bytes to output_data\n",
	      bstat.num_read);
	  output_data (s, fp, bstat.data,  bstat.lines, bpl, extra);

	  buffer_count++;

	  /* This is said to fix the scanner hangs that reportedly show on
	     some MFS-12000SP scanners.  */
	  if (s->mode == 0 && (s->hw->flags & MUSTEK_FLAG_LINEART_FIX)) 
	    usleep (200000);
	}
    }
  
  fclose (fp);
  free (bstat.data);
  if (s->ld.buf[0])
    free (s->ld.buf[0]);
  if (extra) 
    free (extra);
  close (fd);
  return SANE_STATUS_GOOD;
}

static SANE_Status
attach_one_device (const char *devname)
{
  Mustek_Device *dev;

  attach (devname, &dev, SANE_FALSE);
  if (dev)
    {
      /* Keep track of newly attached devices so we can set options as
	 necessary.  */
      if (new_dev_len >= new_dev_alloced)
	{
	  new_dev_alloced += 4;
	  if (new_dev)
	    new_dev = realloc (new_dev, new_dev_alloced * sizeof (new_dev[0]));
	  else
	    new_dev = malloc (new_dev_alloced * sizeof (new_dev[0]));
	  if (!new_dev)
	    {
	      DBG (1, "attach_one_device: out of memory\n");
	      return SANE_STATUS_NO_MEM;
	    }
	}
      new_dev[new_dev_len++] = dev;
    }
  return SANE_STATUS_GOOD;
}

/**************************************************************************/
/*                            SANE API calls                              */
/**************************************************************************/

SANE_Status
sane_init (SANE_Int *version_code, SANE_Auth_Callback authorize)
{
  char line[PATH_MAX], *word, *end;
  const char *cp;
  int linenumber;
  FILE *fp;
  
  DBG_INIT();

#ifdef DBG_LEVEL
  debug_level = DBG_LEVEL;
#else
  debug_level = 0;
#endif

#ifdef VERSION
  DBG(2, "SANE Mustek backend version %d.%d build %d (SANE %s)\n", V_MAJOR,
      V_MINOR, BUILD, VERSION);
#else
  DBG(2, "SANE Mustek backend version %d.%d build %d\n", V_MAJOR,
      V_MINOR, BUILD);
#endif

  if (version_code)
    *version_code = SANE_VERSION_CODE (V_MAJOR, V_MINOR, BUILD);

  DBG(5, "sane_init: authorize %s null\n", authorize ? "!=" : "==");

#ifdef HAVE_SANEI_SCSI_OPEN_EXTENDED
  DBG(5, "sane_init: using sanei_scsi_open_extended\n");
#else
  DBG(5, "sane_init: using sanei_scsi_open with buffer size = %d bytes\n",
      sanei_scsi_max_request_size);
#endif
  
  num_devices = 0;
  force_wait = SANE_FALSE;

  fp = sanei_config_open (MUSTEK_CONFIG_FILE);
  if (!fp)
    {
      /* default to /dev/scanner instead of insisting on config file */
      DBG(3, "sane_init: couldn't find config file (%s), trying "
	  "/dev/scanner directly\n", MUSTEK_CONFIG_FILE);
      attach ("/dev/scanner", 0, SANE_FALSE);
      return SANE_STATUS_GOOD;
    }
  linenumber = 0;
  DBG(4, "sane_init: reading config file `%s'\n",  MUSTEK_CONFIG_FILE);
  while (sanei_config_read (line, sizeof (line), fp))
    {
      word = 0;
      linenumber++;

      cp = sanei_config_get_string (line, &word);
      if (!word || cp == line)
	{
	  DBG(5, "sane_init: config file line %d: ignoring empty line\n",
	      linenumber);
	  continue;
	}
      if (word[0] == '#')
	{
	  DBG(5, "sane_init: config file line %d: ignoring comment line\n",
	      linenumber);
	  free (word);
	  continue;
	}
                    
      if (strcmp (word, "option") == 0)
	{
	  free (word);
	  word = 0;
	  cp = sanei_config_get_string (cp, &word);

	  if (strcmp (word, "strip-height") == 0)
	    {
	      free (word);
	      word = 0;
	      cp = sanei_config_get_string (cp, &word);
	      errno = 0;
	      strip_height = strtod (word, &end);
	      if (end == word)
		{
		  DBG(3, "sane-init: config file line %d: strip-height "
		      "must have a parameter; using 1 inch\n", linenumber);
		  strip_height = 1.0;
		}
	      if (errno)
		{
		  DBG(3, "sane-init: config file line %d: strip-height `%s' "
		      "is invalid (%s); using 1 inch\n",  linenumber,
		      word, strerror (errno));
		  strip_height = 1.0;	
		}
	      else
		{
		  if (strip_height < 0.1)
		    strip_height = 0.1;
		  DBG(3, "sane_init: config file line %d: strip-height set "
		      "to %g inches\n", linenumber, strip_height);
		}
	      if (word)
		free (word);
	      word = 0;
	    }
	  if (strcmp (word, "force-wait") == 0)
	    {
	      DBG(3, "sane_init: config file line %d: enabling force-wait\n",
		   linenumber);
	      force_wait = SANE_TRUE;
	      if (word)
		free (word);
	      word = 0;
	    }
	  else if (strcmp (word, "linedistance-fix") == 0)
	    {
	      if (new_dev_len > 0)
		{
		  new_dev[new_dev_len - 1]->flags |= MUSTEK_FLAG_LD_FIX;
		  DBG(3, "sane_init: config file line %d: enabling "
		      "linedistance-fix for %s\n", linenumber,
		      new_dev[new_dev_len - 1]->sane.name);
		}
	      else
		{
		  DBG(3, "sane_init: config file line %d: option "
		      "linedistance-fix ignored, was set before any device "
		      "name\n", linenumber);
		}
	      if (word)
		free (word);
	      word = 0;
	      }
	  else if (strcmp (word, "lineart-fix") == 0)
	    {
	      if (new_dev_len > 0)
		{
		  new_dev[new_dev_len - 1]->flags |= MUSTEK_FLAG_LINEART_FIX;
		  DBG(3, "sane_init: config file line %d: enabling "
		      "lineart-fix for %s\n", linenumber, 
		      new_dev[new_dev_len - 1]->sane.name);
		}
	      else
		{
		  DBG(3, "sane_init: config file line %d: option "
		      "lineart-fix ignored, was set before any device name\n",
		      linenumber);
		}
	      if (word)
		free (word);
	      word = 0;
	      }
	  else if (strcmp (word, "buffersize") == 0)
	    {
	      long buffer_size;

	      free (word);
	      word = 0;
	      cp = sanei_config_get_string (cp, &word);
	      errno = 0;
	      buffer_size = strtol (word, &end, 0);

	      if (end == word)
		{
		  DBG(3, "sane-init: config file line %d:: buffersize must "
		      "have a parameter; using default (%d kb)\n", linenumber, 
		      new_dev[new_dev_len - 1]->max_buffer_size);
		}
	      if (errno)
		{
		  DBG(3, "sane-init: config file line %d: buffersize `%s' "
		      "is invalid (%s); using default (%d kb)\n",  linenumber,
		      word, strerror (errno),
		      new_dev[new_dev_len - 1]->max_buffer_size);
		}
	      else
		{
		  if (new_dev_len > 0)
		    {
		      if (buffer_size < 32.0)
			buffer_size = 32.0;
		      new_dev[new_dev_len - 1]->max_buffer_size =
			buffer_size * 1024;
		      DBG(3, "sane_init: config file line %d: buffersize set "
			  "to %ld kb for %s\n", linenumber, buffer_size,
			  new_dev[new_dev_len - 1]->sane.name);
		    }
		  else
		    {
		      DBG(3, "sane_init: config file line %d: option "
			  "buffersize ignored, was set before any device "
			  "name\n", linenumber);
		    }
		}
	      if (word)
		free (word);
	      word = 0;
	    }
	  else if (strcmp (word, "blocksize") == 0)
	    {
	      long block_size;

	      free (word);
	      word = 0;
	      cp = sanei_config_get_string (cp, &word);
	      errno = 0;
	      block_size = strtol (word, &end, 0);

	      if (end == word)
		{
		  DBG(3, "sane-init: config file line %d:: blocksize must "
		      "have a parameter; using default (1 GB)\n", linenumber);
		}
	      if (errno)
		{
		  DBG(3, "sane-init: config file line %d: blocksize `%s' "
		      "is invalid (%s); using default (1 GB)\n",  linenumber,
		      word, strerror (errno));
		}
	      else
		{
		  if (new_dev_len > 0)
		    {
		      if (block_size < 256.0)
			block_size = 256.0;
		      new_dev[new_dev_len - 1]->max_block_buffer_size =
			block_size * 1024;
		      DBG(3, "sane_init: config file line %d: blocksize set "
			  "to %ld kb for %s\n", linenumber, block_size,
			  new_dev[new_dev_len - 1]->sane.name);
		    }
		  else
		    {
		      DBG(3, "sane_init: config file line %d: option "
			  "blocksize ignored, was set before any device "
			  "name\n", linenumber);
		    }
		}
	      if (word)
		free (word);
	      word = 0;
	    }
	  else
	    {
	      DBG(3, "sane_init: config file line %d: ignoring unknown "
		  "option `%s'\n", linenumber, cp);
	      if (word)
		free (word);
	      word = 0;
	    }
	}
      else
	{ 
	  new_dev_len = 0;
	  DBG(4, "sane_init: config file line %d: trying to attach `%s'\n",
	      linenumber, line);
	  sanei_config_attach_matching_devices (line, attach_one_device);
	  if (word)
	    free (word);
	  word = 0;
	}
    }     

  if (new_dev_alloced > 0)
    {     
      new_dev_len = new_dev_alloced = 0;
      free (new_dev);
    }
  fclose (fp);
  DBG(5, "sane_init: end\n");
  return SANE_STATUS_GOOD;
}

void
sane_exit (void)
{
  Mustek_Device *dev, *next;

  DBG(4, "sane_exit\n");
  for (dev = first_dev; dev; dev = next)
    {
      next = dev->next;
      free (dev->name);
      free (dev);
    }

  sanei_ab306_exit ();		/* may have to do some cleanup */
}

SANE_Status
sane_get_devices (const SANE_Device ***device_list, SANE_Bool local_only)
{
  static const SANE_Device **devlist = 0;
  Mustek_Device *dev;
  int i;

  DBG(4, "sane_get_devices: %d devices %s\n", num_devices, local_only ? "(local only)" : "");
  if (devlist)
    free (devlist);

  devlist = malloc ((num_devices + 1) * sizeof (devlist[0]));
  if (!devlist)
    return SANE_STATUS_NO_MEM;

  i = 0;
  for (dev = first_dev; i < num_devices; dev = dev->next)
    devlist[i++] = &dev->sane;
  devlist[i++] = 0;

  *device_list = devlist;
  DBG(5, "sane_get_devices: end\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_open (SANE_String_Const devicename, SANE_Handle *handle)
{
  Mustek_Device *dev;
  SANE_Status status;
  Mustek_Scanner *s;

  DBG(4, "sane_open\n");
  if (devicename[0])
    {
      for (dev = first_dev; dev; dev = dev->next)
	if (strcmp (dev->sane.name, devicename) == 0)
	  break;

      if (!dev)
	{
	  status = attach (devicename, &dev, SANE_TRUE);
	  if (status != SANE_STATUS_GOOD)
	    return status;
	}
    }
  else
    /* empty devicname -> use first device */
    dev = first_dev;

  if (!dev)
    return SANE_STATUS_INVAL;

  s = malloc (sizeof (*s));
  if (!s)
    return SANE_STATUS_NO_MEM;
  memset (s, 0, sizeof (*s));
  s->fd = -1;
  s->pipe = -1;
  s->hw = dev;

  init_options (s);

  /* insert newly opened handle into list of open handles: */
  s->next = first_handle;
  first_handle = s;

  *handle = s;
  return SANE_STATUS_GOOD;
}

void
sane_close (SANE_Handle handle)
{
  Mustek_Scanner *prev, *s;

  DBG(4, "sane_close\n");
  /* remove handle from list of open handles: */
  prev = 0;
  for (s = first_handle; s; s = s->next)
    {
      if (s == handle)
	break;
      prev = s;
    }
  if (!s)
    {
      DBG(1, "sane_close: invalid handle %p\n", handle);
      return;		/* oops, not a handle we know about */
    }

  if (s->scanning)
    do_stop (handle);

  if (s->ld.buf[0])
    free (s->ld.buf[0]);

  if (prev)
    prev->next = s->next;
  else
    first_handle = s->next;

  free (handle);
}

const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
  Mustek_Scanner *s = handle;

  if (((unsigned) option >= NUM_OPTIONS) || (option < 0))
    {
      DBG(4, "sane_get_option_descriptor: option %d >= NUM_OPTIONS or < 0\n",
	  option);
      return 0;
    }
  DBG(5, "sane_get_option_descriptor for option %s\n", s->opt[option].name);
  return s->opt + option;
}

SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option,
		     SANE_Action action, void *val, SANE_Int *info)
{
  Mustek_Scanner *s = handle;
  SANE_Status status;
  SANE_Word w, cap;

  if (((unsigned) option >= NUM_OPTIONS) || (option < 0))
    {
      DBG(4, "sane_control_option: option %d < 0 or >= NUM_OPTIONS\n", option);
      return SANE_STATUS_INVAL;
    }

  DBG(5, "sane_control_option (option %s)\n", s->opt[option].name);

  if (info)
    *info = 0;

  if (s->scanning)
    {
      DBG(4, "sane_control_option: don't use wile scanning (option %s)\n",
	  s->opt[option].name);
      return SANE_STATUS_DEVICE_BUSY;
    }

  cap = s->opt[option].cap;

  if (!SANE_OPTION_IS_ACTIVE (cap))
    {
      DBG(4, "sane_control_option: option %s is inactive\n",
	  s->opt[option].name);
      return SANE_STATUS_INVAL;
    }

  if (action == SANE_ACTION_GET_VALUE)
    {
      switch (option)
	{
	  /* word options: */
	case OPT_PREVIEW:
	case OPT_GRAY_PREVIEW:
	case OPT_RESOLUTION:
	case OPT_BACKTRACK:
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
	case OPT_NUM_OPTS:
	case OPT_BRIGHTNESS:
	case OPT_BRIGHTNESS_R:
	case OPT_BRIGHTNESS_G:
	case OPT_BRIGHTNESS_B:
	case OPT_CONTRAST:
	case OPT_CONTRAST_R:
	case OPT_CONTRAST_G:
	case OPT_CONTRAST_B:
	case OPT_CUSTOM_GAMMA:
	case OPT_QUALITY_CAL:
	  *(SANE_Word *) val = s->val[option].w;
	  return SANE_STATUS_GOOD;

	  /* word-array options: */
	case OPT_GAMMA_VECTOR:
	case OPT_GAMMA_VECTOR_R:
	case OPT_GAMMA_VECTOR_G:
	case OPT_GAMMA_VECTOR_B:
	case OPT_HALFTONE_PATTERN:
	  memcpy (val, s->val[option].wa, s->opt[option].size);
	  return SANE_STATUS_GOOD;

	  /* string options: */
	case OPT_SPEED:
	case OPT_SOURCE:
	case OPT_MODE:
	case OPT_HALFTONE_DIMENSION:
	  strcpy (val, s->val[option].s);
	  return SANE_STATUS_GOOD;
	}
    }
  else if (action == SANE_ACTION_SET_VALUE)
    {
      if (!SANE_OPTION_IS_SETTABLE (cap))
	{
	  DBG(4, "sane_control_option: option %s is not setable\n",
	      s->opt[option].name);
	  return SANE_STATUS_INVAL;
	}
      
      status = constrain_value (s, option, val, info);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG(4, "sane_control_option: constrain_value error (option %s)\n",
	      s->opt[option].name);
	  return status;
	}

      switch (option)
	{
	  /* (mostly) side-effect-free word options: */
	case OPT_RESOLUTION:
	case OPT_TL_X:
	case OPT_BR_X:
	case OPT_TL_Y:
	case OPT_BR_Y:
	  if (info)
	    *info |= SANE_INFO_RELOAD_PARAMS;
	  /* fall through */
	case OPT_PREVIEW:
	case OPT_GRAY_PREVIEW:
	case OPT_BACKTRACK:
	case OPT_BRIGHTNESS:
	case OPT_BRIGHTNESS_R:
	case OPT_BRIGHTNESS_G:
	case OPT_BRIGHTNESS_B:
	case OPT_CONTRAST:
	case OPT_CONTRAST_R:
	case OPT_CONTRAST_G:
	case OPT_CONTRAST_B:
	case OPT_QUALITY_CAL:
	  s->val[option].w = *(SANE_Word *) val;
	  return SANE_STATUS_GOOD;

	  /* side-effect-free word-array options: */
	case OPT_HALFTONE_PATTERN:
	case OPT_GAMMA_VECTOR:
	case OPT_GAMMA_VECTOR_R:
	case OPT_GAMMA_VECTOR_G:
	case OPT_GAMMA_VECTOR_B:
	  memcpy (s->val[option].wa, val, s->opt[option].size);
	  return SANE_STATUS_GOOD;

	  /* side-effect-free single-string options: */
	case OPT_SPEED:
	  if (s->val[option].s)
	    free (s->val[option].s);
	  s->val[option].s = strdup (val);
	  return SANE_STATUS_GOOD;

	  /* options with side-effects: */

	case OPT_CUSTOM_GAMMA:
	  w = *(SANE_Word *) val;

	  if (w == s->val[OPT_CUSTOM_GAMMA].w)
	    return SANE_STATUS_GOOD;		/* no change */

	  if (info)
	    *info |= SANE_INFO_RELOAD_OPTIONS;

	  s->val[OPT_CUSTOM_GAMMA].w = w;
	  if (w)
	    {
	      const char *mode = s->val[OPT_MODE].s;

	      if ((strcmp (mode, "Gray") == 0) 
		  || (strcmp (mode, "Gray fast") == 0))
		s->opt[OPT_GAMMA_VECTOR].cap &= ~SANE_CAP_INACTIVE;
	      else if (strcmp (mode, "Color") == 0)
		{
		  s->opt[OPT_GAMMA_VECTOR].cap   &= ~SANE_CAP_INACTIVE;
		  s->opt[OPT_GAMMA_VECTOR_R].cap &= ~SANE_CAP_INACTIVE;
		  s->opt[OPT_GAMMA_VECTOR_G].cap &= ~SANE_CAP_INACTIVE;
		  s->opt[OPT_GAMMA_VECTOR_B].cap &= ~SANE_CAP_INACTIVE;
		}
	      else if ((strcmp (val, "Lineart") == 0) 
		       && (s->hw->flags & MUSTEK_FLAG_PRO))
		{
		  s->opt[OPT_GAMMA_VECTOR].cap &= ~SANE_CAP_INACTIVE;
		}
	    }
	  else
	    {
	      s->opt[OPT_GAMMA_VECTOR].cap   |= SANE_CAP_INACTIVE;
	      s->opt[OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
	      s->opt[OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
	      s->opt[OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;
	    }
	  return SANE_STATUS_GOOD;

	case OPT_MODE:
	  {
	    char *old_val = s->val[option].s;
	    int halftoning, binary;

	    if (old_val)
	      {
		if (strcmp (old_val, val) == 0)
		  return SANE_STATUS_GOOD;		/* no change */
		free (old_val);
	      }
	    if (info)
	      *info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;

	    s->val[option].s = strdup (val);

	    s->opt[OPT_BRIGHTNESS].cap |= SANE_CAP_INACTIVE;
	    s->opt[OPT_BRIGHTNESS_R].cap |= SANE_CAP_INACTIVE;
	    s->opt[OPT_BRIGHTNESS_G].cap |= SANE_CAP_INACTIVE;
	    s->opt[OPT_BRIGHTNESS_B].cap |= SANE_CAP_INACTIVE;
	    s->opt[OPT_CONTRAST].cap |= SANE_CAP_INACTIVE;
	    s->opt[OPT_CONTRAST_R].cap |= SANE_CAP_INACTIVE;
	    s->opt[OPT_CONTRAST_G].cap |= SANE_CAP_INACTIVE;
	    s->opt[OPT_CONTRAST_B].cap |= SANE_CAP_INACTIVE;
	    s->opt[OPT_CUSTOM_GAMMA].cap |= SANE_CAP_INACTIVE;
	    s->opt[OPT_GAMMA_VECTOR].cap |= SANE_CAP_INACTIVE;
	    s->opt[OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
	    s->opt[OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
	    s->opt[OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;
	    s->opt[OPT_HALFTONE_DIMENSION].cap |= SANE_CAP_INACTIVE;
	    s->opt[OPT_HALFTONE_PATTERN].cap |= SANE_CAP_INACTIVE;

	    halftoning = strcmp (val, "Halftone") == 0;
	    binary = (halftoning || strcmp (val, "Lineart") == 0);

	    if (binary)
	      {
		/* enable brightness/contrast for  when in a binary mode */
		s->opt[OPT_BRIGHTNESS].cap &= ~SANE_CAP_INACTIVE;
		/* The SE and paragon models support only threshold 
		   in lineart */
		if (!(s->hw->flags & MUSTEK_FLAG_SE) 
		    && !(s->hw->flags & MUSTEK_FLAG_PRO))
		  s->opt[OPT_CONTRAST].cap &= ~SANE_CAP_INACTIVE;

		if (halftoning)
		  {
		    s->opt[OPT_HALFTONE_DIMENSION].cap &= ~SANE_CAP_INACTIVE;
		    encode_halftone (s);
		    if (s->custom_halftone_pattern)
		      {
			s->opt[OPT_HALFTONE_PATTERN].cap 
			  &= ~SANE_CAP_INACTIVE;
		      }
		    else
		      {
			s->opt[OPT_HALFTONE_PATTERN].cap 
			  |= SANE_CAP_INACTIVE;
		      }
		  }
	      }
	    else
	      {
		s->opt[OPT_CUSTOM_GAMMA].cap &= ~SANE_CAP_INACTIVE;
	      }

	    if (s->hw->flags & MUSTEK_FLAG_THREE_PASS)
	      {
		if (strcmp (s->val[OPT_MODE].s, "Color") == 0)
		  {
		    s->opt[OPT_BRIGHTNESS_R].cap &= ~SANE_CAP_INACTIVE;
		    s->opt[OPT_BRIGHTNESS_G].cap &= ~SANE_CAP_INACTIVE;
		    s->opt[OPT_BRIGHTNESS_B].cap &= ~SANE_CAP_INACTIVE;
		    s->opt[OPT_CONTRAST_R].cap &= ~SANE_CAP_INACTIVE;
		    s->opt[OPT_CONTRAST_G].cap &= ~SANE_CAP_INACTIVE;
		    s->opt[OPT_CONTRAST_B].cap &= ~SANE_CAP_INACTIVE;
		  }
		else
		  {
		    s->opt[OPT_BRIGHTNESS].cap &= ~SANE_CAP_INACTIVE;
		    s->opt[OPT_CONTRAST].cap &= ~SANE_CAP_INACTIVE;
		  }
	      }

	    if (s->val[OPT_CUSTOM_GAMMA].w)
	      {
		if ((strcmp (val, "Gray") == 0) 
		    || (strcmp (val, "Gray fast") == 0))
		  s->opt[OPT_GAMMA_VECTOR].cap &= ~SANE_CAP_INACTIVE;
		else if (strcmp (val, "Color") == 0)
		  {
		    s->opt[OPT_GAMMA_VECTOR].cap   &= ~SANE_CAP_INACTIVE;
		    s->opt[OPT_GAMMA_VECTOR_R].cap &= ~SANE_CAP_INACTIVE;
		    s->opt[OPT_GAMMA_VECTOR_G].cap &= ~SANE_CAP_INACTIVE;
		    s->opt[OPT_GAMMA_VECTOR_B].cap &= ~SANE_CAP_INACTIVE;
		  }
	      }
	    return SANE_STATUS_GOOD;
	  }

	case OPT_HALFTONE_DIMENSION:
	  /* halftone pattern dimension affects halftone pattern option: */
	  {
	    if (strcmp(s->val[option].s,  (SANE_String) val) == 0)
	      return SANE_STATUS_GOOD;		/* no change */

	    if (info)
	      *info |= SANE_INFO_RELOAD_OPTIONS;
	    
	    s->val[option].s = strdup (val);
	    encode_halftone (s);
	    s->opt[OPT_HALFTONE_PATTERN].cap |= SANE_CAP_INACTIVE;
	    if (s->custom_halftone_pattern)
	      {
		s->opt[OPT_HALFTONE_PATTERN].cap &= ~SANE_CAP_INACTIVE;
		s->opt[OPT_HALFTONE_PATTERN].size = 
		  (s->halftone_pattern_type & 0x0f) * sizeof (SANE_Word);
	      }
	    return SANE_STATUS_GOOD;
	  }

	case OPT_SOURCE:
	  if (info)
	    *info |= SANE_INFO_RELOAD_OPTIONS;
	  if (s->val[option].s)
	    free (s->val[option].s);
	  s->val[option].s = strdup (val);

	  if (strcmp (val, "Transparency Adapter") == 0)
	    {
	      s->opt[OPT_TL_X].constraint.range = &s->hw->x_trans_range;
	      s->opt[OPT_TL_Y].constraint.range = &s->hw->y_trans_range;
	      s->opt[OPT_BR_X].constraint.range = &s->hw->x_trans_range;
	      s->opt[OPT_BR_Y].constraint.range = &s->hw->y_trans_range;
	    }
	  else
	    {
	      s->opt[OPT_TL_X].constraint.range = &s->hw->x_range;
	      s->opt[OPT_TL_Y].constraint.range = &s->hw->y_range;
	      s->opt[OPT_BR_X].constraint.range = &s->hw->x_range;
	      s->opt[OPT_BR_Y].constraint.range = &s->hw->y_range;
	    }
	  return SANE_STATUS_GOOD;
	}
    }
  DBG(4, "sane_control_option: unknown action for option %s\n",
      s->opt[option].name);
  return SANE_STATUS_INVAL;
}

SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters *params)
{
  Mustek_Scanner *s = handle;
  const char *mode;

  if (!s->scanning)
    {
      double width, height, dpi;

      memset (&s->params, 0, sizeof (s->params));

      width = SANE_UNFIX (s->val[OPT_BR_X].w - s->val[OPT_TL_X].w);
      height = SANE_UNFIX (s->val[OPT_BR_Y].w - s->val[OPT_TL_Y].w);
      dpi = SANE_UNFIX (s->val[OPT_RESOLUTION].w);

      /* make best-effort guess at what parameters will look like once
         scanning starts.  */
      if (dpi > 0.0 && width > 0.0 && height > 0.0)
	{
	  double dots_per_mm = dpi / MM_PER_INCH;

	  s->params.pixels_per_line = width * dots_per_mm;
	  s->params.lines = height * dots_per_mm;
	}
      encode_halftone (s);
      mode = s->val[OPT_MODE].s;
      if (strcmp (mode, "Lineart") == 0 || strcmp (mode, "Halftone") == 0)
	{
	  s->params.format = SANE_FRAME_GRAY;
	  s->params.bytes_per_line = (s->params.pixels_per_line + 7) / 8;
	  s->params.depth = 1;
	}
      else if (strcmp (mode, "Gray") == 0)
	{
	  s->params.format = SANE_FRAME_GRAY;
	  s->params.bytes_per_line = s->params.pixels_per_line;
	  s->params.depth = 8;
	}
      else if (strcmp (mode, "Gray fast") == 0)
	{
	  s->params.format = SANE_FRAME_GRAY;
	  s->params.bytes_per_line = s->params.pixels_per_line;
	  s->params.depth = 8;
	}
      else
	{
	  /* it's one of the color modes... */

	  if (s->hw->flags & MUSTEK_FLAG_THREE_PASS)
	    {
	      s->params.format = SANE_FRAME_RED + s->pass;
	      if (strcmp (mode, "Color") == 0)
		{
		  s->params.bytes_per_line = s->params.pixels_per_line;
		  s->params.depth = 8;
		}
	      else
		{
		  s->params.bytes_per_line
		    = (s->params.pixels_per_line + 7) / 8;
		  s->params.depth = 1;
		}
	    }
	  else
	    {
	      /* all color modes are treated the same since lineart and
		 halftoning with 1 bit color pixels still results in 3
		 bytes/pixel.  */
	      s->params.format = SANE_FRAME_RGB;
	      s->params.bytes_per_line = 3 * s->params.pixels_per_line;
	      s->params.depth = 8;
	    }
	}
    }
  else if ((s->mode & MUSTEK_MODE_COLOR)
	   && (s->hw->flags & MUSTEK_FLAG_THREE_PASS))
    s->params.format = SANE_FRAME_RED + s->pass;
  s->params.last_frame = (s->params.format != SANE_FRAME_RED
			  && s->params.format != SANE_FRAME_GREEN);
  if (params)
    *params = s->params;
  DBG(5, "sane_get_parameters: lines = %d; ppl = %d; bpl = %d\n",
      s->params.lines, s->params.pixels_per_line, s->params.bytes_per_line);

  return SANE_STATUS_GOOD;
}

SANE_Status
sane_start (SANE_Handle handle)
{
  Mustek_Scanner *s = handle;
  SANE_Status status;
  int fds[2];

  DBG(4, "sane_start\n");
  /* First make sure we have a current parameter set.  Some of the
     parameters will be overwritten below, but that's OK.  */
  status = sane_get_parameters (s, 0);
  if (status != SANE_STATUS_GOOD)
    return status;

  if (s->fd < 0)
    {
      /* this is the first (and maybe only) pass... */
      const char *mode;
      struct timeval start;

      /* save start time */
      gettimeofday (&start, 0);
      s->start_time = start.tv_sec;
      /* translate options into s->mode for convenient access: */
      mode = s->val[OPT_MODE].s;
      if (strcmp (mode, "Lineart") == 0)
	s->mode = MUSTEK_MODE_LINEART;
      else if (strcmp (mode, "Halftone") == 0)
	s->mode = MUSTEK_MODE_HALFTONE;
      else if (strcmp (mode, "Gray") == 0)
	s->mode = MUSTEK_MODE_GRAY;
      else if (strcmp (mode, "Gray fast") == 0)
	s->mode = MUSTEK_MODE_GRAY_FAST;
      else if (strcmp (mode, "Color") == 0)
	s->mode = MUSTEK_MODE_COLOR;

      s->one_pass_color_scan = 0;
      if (s->mode & MUSTEK_MODE_COLOR)
	{
	  if (s->val[OPT_PREVIEW].w && s->val[OPT_GRAY_PREVIEW].w)
	    {
	      /* Force gray-scale mode when previewing.  */
	      s->mode = MUSTEK_MODE_GRAY;
	      s->params.format = SANE_FRAME_GRAY;
	      s->params.bytes_per_line = s->params.pixels_per_line;
	      s->params.last_frame = SANE_TRUE;
	    }
	  else if (!(s->hw->flags & MUSTEK_FLAG_THREE_PASS))
	    s->one_pass_color_scan = 1;
	}
      s->resolution_code = encode_resolution (s);

      status = dev_open (s->hw->sane.name, s, sense_handler);
      if (status != SANE_STATUS_GOOD)
	return status;
    } 

  status = dev_wait_ready (s);
  if (status != SANE_STATUS_GOOD)
    {
      DBG(1, "sane_start: wait_ready() failed: %s\n", sane_strstatus (status));
      goto stop_scanner_and_return;
    }

  status = inquiry (s);
  if (status != SANE_STATUS_GOOD)
    {
      DBG(1, "sane_start: inquiry command failed: %s\n",
	  sane_strstatus (status));
      goto stop_scanner_and_return;
    }

  if ((strcmp(s->val[OPT_SOURCE].s, "Automatic Document Feeder") == 0) &&
      !(s->hw->flags & MUSTEK_FLAG_ADF_READY))
    {
      DBG(2, "sane_start: automatic document feeder is out of documents\n");
      status = SANE_STATUS_NO_DOCS;                                 
      goto stop_scanner_and_return;
    }
    
  if (s->hw->flags & MUSTEK_FLAG_SE)
    { 
      status = set_window_se (s, 0);	
      if (status != SANE_STATUS_GOOD)
	{
	  DBG(1, "sane_start: set window command failed: %s\n",
	      sane_strstatus (status));
	  goto stop_scanner_and_return;
	}

      s->scanning = SANE_TRUE;
      s->cancelled = SANE_FALSE;
      status = get_window (s, &s->params.bytes_per_line,
			      &s->params.lines, &s->params.pixels_per_line);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG(1, "sane_start: get window command failed: %s\n",
	      sane_strstatus (status));
	  goto stop_scanner_and_return;
	}

      status = start_scan (s);
      if (status != SANE_STATUS_GOOD) 
	goto stop_scanner_and_return;

      status = send_gamma_table_se (s);
      if (status != SANE_STATUS_GOOD) 
	goto stop_scanner_and_return;
/*
      status = calibration (s);
      if (status != SANE_STATUS_GOOD) 
	goto stop_scanner_and_return; */
    }


  else if (s->hw->flags & MUSTEK_FLAG_PRO)
    {       

      status = dev_wait_ready (s);	
      if (status != SANE_STATUS_GOOD)
	goto stop_scanner_and_return;

      status = set_window_pro (s);	
      if (status != SANE_STATUS_GOOD)
	goto stop_scanner_and_return;

      s->scanning = SANE_TRUE;
      s->cancelled = SANE_FALSE;

      status = adf_and_backtrack (s);
      if (status != SANE_STATUS_GOOD)
	goto stop_scanner_and_return;

      status = mode_select_pro (s);	
      if (status != SANE_STATUS_GOOD)
	goto stop_scanner_and_return;
      
      status = calibration_pro (s);	
      if (status != SANE_STATUS_GOOD)
	goto stop_scanner_and_return;
      
      status = send_gamma_table (s);
      if (status != SANE_STATUS_GOOD) 
	goto stop_scanner_and_return;

      status = start_scan (s);
      if (status != SANE_STATUS_GOOD) 
	goto stop_scanner_and_return;

      status = get_image_status (s, &s->params.bytes_per_line, 
				 &s->params.lines);
      if (status != SANE_STATUS_GOOD)
      	goto stop_scanner_and_return;
    }

  else /* Paragon series */
    {
      status = area_and_windows (s);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG(1, "sane_start: set scan area command failed: %s\n",
	      sane_strstatus (status));
	  goto stop_scanner_and_return;
	}
      /* xxx */                                  
      /*s->val[OPT_BACKTRACK].w = SANE_FALSE;*/
      status = adf_and_backtrack (s);
      if (status != SANE_STATUS_GOOD)
	goto stop_scanner_and_return;

      if (s->one_pass_color_scan)
	{
	  status = mode_select_paragon (s, MUSTEK_CODE_RED);
	  if (status != SANE_STATUS_GOOD)
	    goto stop_scanner_and_return;

	  status = mode_select_paragon (s, MUSTEK_CODE_GREEN);
	  if (status != SANE_STATUS_GOOD)
	    goto stop_scanner_and_return;

	  status = mode_select_paragon (s, MUSTEK_CODE_BLUE);
	}
      else
	status = mode_select_paragon (s, MUSTEK_CODE_GRAY);

      s->scanning = SANE_TRUE;
      s->cancelled = SANE_FALSE;

      status = send_gamma_table (s);
      if (status != SANE_STATUS_GOOD)
	goto stop_scanner_and_return;
      
      status = start_scan (s);
      if (status != SANE_STATUS_GOOD)
	goto stop_scanner_and_return;

      status = send_gamma_table (s);
      if (status != SANE_STATUS_GOOD)
	goto stop_scanner_and_return;

      s->ld.max_value = 0;
      if (!(s->hw->flags & MUSTEK_FLAG_THREE_PASS))
	{
	  status = line_distance (s);
	  if (status != SANE_STATUS_GOOD)
	    goto stop_scanner_and_return;
	}

      /* xxx
      status = adf_and_backtrack (s);
      if (status != SANE_STATUS_GOOD)
	goto stop_scanner_and_return;
      */
      status = get_image_status (s, &s->params.bytes_per_line, 
				 &s->params.lines);
      if (status != SANE_STATUS_GOOD)
      	goto stop_scanner_and_return;
    }

  s->params.pixels_per_line = s->params.bytes_per_line;
  if (s->one_pass_color_scan)
    s->params.pixels_per_line /= 3;
  else if ((s->mode & MUSTEK_MODE_LINEART) 
	   || (s->mode & MUSTEK_MODE_HALFTONE))
    s->params.pixels_per_line *= 8;

  s->line = 0;

  if (pipe (fds) < 0)
    return SANE_STATUS_IO_ERROR;

  s->reader_pid = fork ();
  if (s->reader_pid == 0)
    {
      int status;
      sigset_t ignore_set;
      struct SIGACTION act;

      close (fds[0]);

      sigfillset (&ignore_set);
      sigdelset (&ignore_set, SIGTERM);
      sigprocmask (SIG_SETMASK, &ignore_set, 0);
      memset (&act, 0, sizeof (act));
      sigaction (SIGTERM, &act, 0);

      status = reader_process (s, fds[1]);

      /* don't use exit() since that would run the atexit() handlers... */
      _exit (status);
    }
  close (fds[1]);
  s->pipe = fds[0];

  return SANE_STATUS_GOOD;

stop_scanner_and_return:
  do_stop (s);
  return status;
}

SANE_Status
sane_read (SANE_Handle handle, SANE_Byte *buf, SANE_Int max_len, SANE_Int *len)
{
  Mustek_Scanner *s = handle;
  SANE_Status status;
  ssize_t ntotal;
  ssize_t nread;
  

  DBG(5, "sane_read\n");
  *len = 0;
  ntotal = 0;

  if (s->cancelled)
    {
      DBG(4, "sane_read: scan was cancelled\n");
      return SANE_STATUS_CANCELLED;
    }

  if (!s->scanning)
    {
      DBG(3, "sane_read: must call sane_start before sane_read\n");
      return SANE_STATUS_INVAL;
    }

  while (*len < max_len)
    {
      nread = read (s->pipe, buf + *len, max_len - *len);

      if (s->cancelled)
	{
	  DBG(4, "sane_read: scan was cancelled\n");
	  *len = 0;
	  return SANE_STATUS_CANCELLED;
	}

      if (nread < 0)
	{
	  if (errno == EAGAIN)
	    {
	      if (*len == 0)
		DBG(5, "sane_read: no more data at the moment--try again\n");
	      else
		DBG(5, "sane_read: read buffer of %d bytes\n", *len);
	      return SANE_STATUS_GOOD;
	    }
	  else
	    {
	      DBG(1, "sane_read: IO error\n");
	      do_stop (s);
	      *len = 0;
	      return SANE_STATUS_IO_ERROR;
	    }
	}

      *len += nread;
      
      if (nread == 0)
	{
	  if (*len == 0)
	    {
	      if (!(s->hw->flags & MUSTEK_FLAG_THREE_PASS)
		  || !(s->mode & MUSTEK_MODE_COLOR) || ++s->pass >= 3)
		{
         	  DBG(5, "sane_read: pipe was closed ... calling do_stop\n");
		  status = do_stop (s);
		  if (status != SANE_STATUS_CANCELLED 
		      && status != SANE_STATUS_GOOD)
		    return status; /* something went wrong */
		}
	      else /* 3pass color first or second pass */
		{
		  DBG(5, "sane_read: pipe was closed ... finishing pass %d\n",
		      s->pass);
		}

	      return do_eof (s);
	    }
	  else
	    {
	      DBG(5, "sane_read: read last buffer of %d bytes\n",
		  *len);
	      return SANE_STATUS_GOOD;
	    }
	}
    }
  DBG(5, "sane_read: read full buffer of %d bytes\n", *len);
  return SANE_STATUS_GOOD;
}

void
sane_cancel (SANE_Handle handle)
{
  Mustek_Scanner *s = handle;

  DBG(4, "sane_cancel\n");
  if (s->scanning)
    {
      s->cancelled = SANE_TRUE;
      do_stop (handle);
    }
  DBG(4, "sane_cancel finished\n");
}

SANE_Status
sane_set_io_mode (SANE_Handle handle, SANE_Bool non_blocking)
{
  Mustek_Scanner *s = handle;

  DBG(4, "sane_set_io_mode: %s\n", non_blocking ? "non-blocking" : "blocking");

  if (!s->scanning)
    {
      DBG(1, "sane_set_io_mode: call sane_start before sane_set_io_mode");
      return SANE_STATUS_INVAL;
    }

  if (fcntl (s->pipe, F_SETFL, non_blocking ? O_NONBLOCK : 0) < 0)
    {
      DBG(1, "sane_set_io_mode: can't set io mode");
      return SANE_STATUS_IO_ERROR;
    }

  return SANE_STATUS_GOOD;
}

SANE_Status
sane_get_select_fd (SANE_Handle handle, SANE_Int *fd)
{
  Mustek_Scanner *s = handle;

  DBG(4, "sane_get_select_fd\n");
  if (!s->scanning)
    return SANE_STATUS_INVAL;

  *fd = s->pipe;
  return SANE_STATUS_GOOD;
}
