/* sane - Scanner Access Now Easy.
   Copyright (C) 1996, 1997 David Mosberger-Tang and Andreas Czechanowski,
   1998 Andreas Bolsch for extension to ScanExpress models version 0.6
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

   This file implements a SANE backend for Mustek flatbed scanners.  */

#include <sane/config.h>

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

#include <sane/sane.h>
#include <sane/sanei.h>
#include <sane/saneopts.h>
#include <sane/sanei_scsi.h>
#include <sane/sanei_ab306.h>
#include <mustek.h>

#define BACKEND_NAME	mustek
#include <sane/sanei_backend.h>

#ifndef PATH_MAX
# define PATH_MAX	1024
#endif

#include <sane/sanei_config.h>
#define MUSTEK_CONFIG_FILE "mustek.conf"

#define MM_PER_INCH	25.4
#define MIN(a,b)	((a) < (b) ? (a) : (b))
#define MAX(a,b)	((a) > (b) ? (a) : (b))

/* Maximum time to wait for scanner to become ready.  */
#define MAX_WAITING_TIME	60
/* Number of queued read requests/buffers */
#define REQUESTS		1
/* Max. allocated buffers for read requests */
#define BUFFERS			64
/* The 600 II N and ScanExpress require this for line-distance correction */
#define MAX_LINE_DIST		32

/* Maximum # of inches to scan in one swoop.  0 means "unlimited."
   This is here to be nice on the SCSI bus---Mustek scanners don't
   disconnect while scanning is in progress, which confuses some
   drivers that expect no reasonable SCSI request would take more than
   10 seconds.  That's not really true for Mustek scanners operating
   in certain modes, hence this limit.  */
static double strip_height;

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
        
static const SANE_String_Const mode_list[] =
  {
    "Lineart", "Halftone", "Gray",
    "Color Lineart", "Color Halftone", "Color",
    0
  };

static const SANE_String_Const pp_mode_list[] =
  {
    "Lineart", "Halftone", "Gray", "Color",
    0
  };

static const SANE_String_Const se_mode_list[] =
  {
    "Lineart", "Gray", "Color",
    0
  };

static const SANE_String_Const speed_list[] =
  {
    "Slowest", "Slower", "Normal", "Faster", "Fastest",
    0
  };

enum Scan_Source
  {
    FLATBED, ADF, TA
  };

static const SANE_String_Const source_list[] =
  {
    "Flatbed", "Automatic Document Feeder", "Transparency Adapter",
    0
  };

static const SANE_Int grain_list[] =
  {
    6,				/* # of elements */
    2, 3, 4, 5, 6, 8
  };

static const SANE_Range u8_range =
  {
      0,				/* minimum */
    255,				/* maximum */
      0				/* quantization */
  };

static const SANE_Int pattern_dimension_list[] =
  {
    8,				/* # of elements */
    0, 2, 3, 4, 5, 6, 7, 8
  };

static const SANE_Range percentage_range =
  {
    -100 << SANE_FIXED_SCALE_SHIFT,	/* minimum */
     100 << SANE_FIXED_SCALE_SHIFT,	/* maximum */
       1 << SANE_FIXED_SCALE_SHIFT	/* quantization */
  };

/* for three-pass (ax) scanners: */

static const SANE_Range ax_brightness_range =
  {
    -36 << SANE_FIXED_SCALE_SHIFT,	/* minimum */
     36 << SANE_FIXED_SCALE_SHIFT,	/* maximum */
      3 << SANE_FIXED_SCALE_SHIFT	/* quantization */
  };

static const SANE_Range ax_contrast_range =
  {
    -84 << SANE_FIXED_SCALE_SHIFT,	/* minimum */
     84 << SANE_FIXED_SCALE_SHIFT,	/* maximum */
      7 << SANE_FIXED_SCALE_SHIFT	/* quantization */
  };

/* Color band codes: */
#define MUSTEK_CODE_GRAY		0
#define MUSTEK_CODE_RED			1
#define MUSTEK_CODE_GREEN		2
#define MUSTEK_CODE_BLUE		3

/* SCSI commands that the Mustek scanners understand (or not): */
#define MUSTEK_SCSI_TEST_UNIT_READY	0x00
#define MUSTEK_SCSI_AREA_AND_WINDOWS	0x04
#define MUSTEK_SCSI_READ_SCANNED_DATA	0x08
#define MUSTEK_SCSI_GET_IMAGE_STATUS	0x0f
#define MUSTEK_SCSI_ADF_AND_BACKTRACK	0x10
#define MUSTEK_SCSI_CCD_DISTANCE	0x11
#define MUSTEK_SCSI_INQUIRY		0x12
#define MUSTEK_SCSI_MODE_SELECT		0x15
#define MUSTEK_SCSI_START_STOP		0x1b
#define MUSTEK_SCSI_LOOKUP_TABLE	0x55
#define MUSTEK_SCSI_SET_WINDOW		0x24
#define MUSTEK_SCSI_GET_WINDOW		0x25
#define MUSTEK_SCSI_READ_DATA		0x28
#define MUSTEK_SCSI_SEND_DATA		0x2a

#define INQ_LEN	0x60
static const u_int8_t inquiry[] =
{
  MUSTEK_SCSI_INQUIRY, 0x00, 0x00, 0x00, INQ_LEN, 0x00
};

static const u_int8_t test_unit_ready[] =
{
  MUSTEK_SCSI_TEST_UNIT_READY, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const u_int8_t stop[] =
{
  MUSTEK_SCSI_START_STOP, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const u_int8_t distance[] =
{
  MUSTEK_SCSI_CCD_DISTANCE, 0x00, 0x00, 0x00, 0x05, 0x00
};

static const u_int8_t get_status[] =
{
  MUSTEK_SCSI_GET_IMAGE_STATUS, 0x00, 0x00, 0x00, 0x06, 0x00
};

static const u_int8_t set_window[] =
{
  MUSTEK_SCSI_SET_WINDOW, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00
};

static const u_int8_t get_window[] =
{
  MUSTEK_SCSI_GET_WINDOW, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00
};

static const u_int8_t read_data[] =
{
  MUSTEK_SCSI_READ_DATA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const u_int8_t send_data[] =
{
  MUSTEK_SCSI_SEND_DATA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/* For the next macros suffix 'L' means little endian, 'B' big endian */
#define STORE16L(p,v)				\
do {						\
    int value = (v);				\
						\
    *(cp)++ = (value >> 0) & 0xff;		\
    *(cp)++ = (value >> 8) & 0xff;		\
} while (0)

#define STORE16B(p,v)				\
do {						\
    int value = (v);				\
						\
    *(cp)++ = (value >> 8) & 0xff;		\
    *(cp)++ = (value >> 0) & 0xff;		\
} while (0)

#define STORE32B(p,v)				\
do {						\
    long int value = (v);			\
						\
    *(cp)++ = (value >> 24) & 0xff;		\
    *(cp)++ = (value >> 16) & 0xff;		\
    *(cp)++ = (value >>  8) & 0xff;		\
    *(cp)++ = (value >>  0) & 0xff;		\
} while (0)

static SANE_Status
scsi_wait_ready (int fd)
{
  struct timeval now, start;
  SANE_Status status;

  gettimeofday (&start, 0);

  while (1)
    {
      DBG(3, "scsi_wait_ready: sending TEST_UNIT_READY\n");

      status = sanei_scsi_cmd (fd, test_unit_ready, sizeof (test_unit_ready),
			       0, 0);
      switch (status)
	{
	default:
	  /* Ignore errors while waiting for scanner to become ready.
	     Some SCSI drivers return EIO while the scanner is
	     returning to the home position.  */
	  DBG(1, "scsi_wait_ready: test unit ready failed (%s)\n",
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
pp_wait_ready (int fd)
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
	  DBG(1, "pp_wait_ready: timed out after %lu seconds\n",
	      (u_long) (now.tv_sec - start.tv_sec));
	  return SANE_STATUS_INVAL;
	}
      usleep (100000);	/* retry after 100ms */
    }
}

static SANE_Status
dev_wait_ready (Mustek_Scanner *s)
{
  if (s->hw->flags & MUSTEK_FLAG_PP)
    return pp_wait_ready (s->fd);
  else
    return scsi_wait_ready (s->fd);
}

static SANE_Status
dev_open (const char * devname, Mustek_Scanner * s,
	  SANEI_SCSI_Sense_Handler handler)
{
  SANE_Status status;

  status = sanei_scsi_open (devname, &s->fd, handler, 0);
  if (status != SANE_STATUS_GOOD)
    {
      DBG(2, "dev_open: %s: can't open %s as a SCSI device\n",
	  sane_strstatus (status), devname);

      status = sanei_ab306_open (devname, &s->fd);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG(1, "dev_open: %s: can't open %s as a parallel-port device\n",
	      sane_strstatus (status), devname);
	  return SANE_STATUS_INVAL;
	}
      s->hw->flags |= MUSTEK_FLAG_PP;
    }
  return SANE_STATUS_GOOD;
}

static SANE_Status
dev_cmd (Mustek_Scanner * s, const void * src, size_t src_size,
	 void * dst, size_t * dst_size)
{
  if (s->hw->flags & MUSTEK_FLAG_PP)
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
dev_read_start (Mustek_Scanner *s)
{
  int lines = s->hw->lines;
  
  if (s->hw->flags & MUSTEK_FLAG_PP)
    {
      if (s->hw->flags & MUSTEK_FLAG_SE)
	{
	  u_int8_t readlines[10];

	  if (s->mode & MUSTEK_MODE_COLOR)
	    lines *= 3;
	    
	  memset (readlines, 0, sizeof (readlines));
	  readlines[0] = MUSTEK_SCSI_READ_DATA;
	  readlines[7] = (lines >> 8) & 0xff;
	  readlines[8] = (lines >> 0) & 0xff;
	  return sanei_ab306_cmd (s->fd, readlines, sizeof (readlines), 0, 0);
	}
      else
	{      		      
	  u_int8_t readlines[6];

	  memset (readlines, 0, sizeof (readlines));
	  readlines[0] = MUSTEK_SCSI_READ_SCANNED_DATA;
	  readlines[2] = (lines >> 16) & 0xff;
	  readlines[3] = (lines >>  8) & 0xff;
	  readlines[4] = (lines >>  0) & 0xff;
	  return sanei_ab306_cmd (s->fd, readlines, sizeof (readlines), 0, 0);
	}
    }
  else
    return SANE_STATUS_GOOD;
}

static SANE_Status
dev_read_req_enter (Mustek_Scanner *s, SANE_Byte *buf, int lines, int bpl,
		    size_t *lenp, void **idp, int bank)
{
  *lenp = lines * bpl;
  DBG(1, "reader: about to read %lu bytes\n", (unsigned long) *lenp);

  if (s->hw->flags & MUSTEK_FLAG_PP)
    {
      int planes;

      *idp = 0;

      planes = (s->mode & MUSTEK_MODE_COLOR) ? 3 : 1;

      /* for color lineart/halftone modes, try this : */
      if ((s->mode & MUSTEK_MODE_COLOR) && !(s->mode & MUSTEK_MODE_MULTIBIT))
	lines /= 3;

      return sanei_ab306_rdata (s->fd, planes, buf, lines, bpl);
    }
  else
    {
      if (s->hw->flags & MUSTEK_FLAG_SE)
	{
	  u_int8_t readlines[10];
	  DBG(1, "buffer_bank: %d\n", bank);	  			
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
      else
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
  if (s->hw->flags & MUSTEK_FLAG_PP)
    sanei_ab306_close (s->fd);
  else
    sanei_scsi_close (s->fd);
}

static SANE_Status
sense_handler (int scsi_fd, u_char *result, void *arg)
{
  switch (result[0])
    {
    case 0x00:
      break; 

    case 0x82:
      if (result[1] & 0x80)
	return SANE_STATUS_JAMMED;	/* ADF is jammed */
      break;

    case 0x83:
      if (result[2] & 0x02)
	return SANE_STATUS_NO_DOCS;	/* ADF out of documents */
      break;

    case 0x84:
      if (result[1] & 0x10)
	return SANE_STATUS_COVER_OPEN;	/* open transparency adapter cover */
      break;

    default:
      DBG(1, "sense_handler: got unknown sense code %02x\n", result[0]);
      return SANE_STATUS_IO_ERROR;
    }
  return SANE_STATUS_GOOD;
}

static SANE_Status
do_inquiry (Mustek_Scanner *s)
{
  char result[INQ_LEN];
  size_t size;

  DBG(3, "do_inquiry: sending INQUIRY\n");
  size = sizeof (result);
  return dev_cmd (s, inquiry, sizeof (inquiry), result, &size);
}

static SANE_Status
attach (const char *devname, Mustek_Device **devp, int may_wait)
{
  int mustek_scanner, fw_revision;
  char result[INQ_LEN];
  const char *model_name = result + 44;
  Mustek_Scanner s;
  Mustek_Device *dev, new_dev;
  SANE_Status status;
  size_t size;

  if (devp)
    *devp = 0;
    
  for (dev = first_dev; dev; dev = dev->next)
    if (strcmp (dev->sane.name, devname) == 0)
      {
	if (devp)
	  *devp = dev;
	return SANE_STATUS_GOOD;
      }

  DBG(3, "attach: opening %s as scsi device\n", devname);

  memset (&new_dev, 0, sizeof (new_dev));
  memset (&s, 0, sizeof (s));
  s.hw = &new_dev;

  status = dev_open (devname, &s, sense_handler);
  if (status != SANE_STATUS_GOOD)
    return status;

  if (may_wait)
    dev_wait_ready (&s);

  DBG(3, "attach: sending INQUIRY\n");
  size = sizeof (result);
  status = dev_cmd (&s, inquiry, sizeof (inquiry), result, &size);
  if (status != SANE_STATUS_GOOD || size != INQ_LEN)
    {
      DBG(1, "attach: inquiry failed (%s)\n", sane_strstatus (status));
      
      dev_close (&s);
      return status;
    }

  status = dev_wait_ready (&s);
  dev_close (&s);
  if (status != SANE_STATUS_GOOD)
    return status;

  /* first check for new firmware format: */
  mustek_scanner = (strncmp (result + 36, "MUSTEK", 6) == 0);
  if (!mustek_scanner)
    {
      /* check for old format: */
      mustek_scanner = (strncmp (result + 8, "MUSTEK", 6) == 0);
      model_name = result + 16;
    }
  mustek_scanner = mustek_scanner && (result[0] == 0x06);

  if (!mustek_scanner)
    {
      DBG(1, "attach: device doesn't look like a Mustek scanner "
	  "(result[0]=%#02x)\n", result[0]);
      return SANE_STATUS_INVAL;
    }

  /* get firmware revision as BCD number: */
  fw_revision =
      (result[32] - '0') << 8 | (result[34] - '0') << 4 | (result[35] - '0');
  DBG(1, "attach: firmware revision %d.%02x\n",
      fw_revision >> 8, fw_revision & 0xff);

  dev = malloc (sizeof (*dev));
  if (!dev)
    return SANE_STATUS_NO_MEM;

  memcpy (dev, &new_dev, sizeof (*dev));

  dev->sane.name   = strdup (devname);
  dev->sane.vendor = "Mustek";
  dev->sane.model  = strndup (model_name, 11);
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
  if (dev->flags & MUSTEK_FLAG_PP)
    /* parallel-port scanners make unhealthy noises below 51 dpi... */
    dev->dpi_range.min = SANE_FIX (51);
  else
    dev->dpi_range.min = SANE_FIX (1);
  dev->dpi_range.quant = SANE_FIX (1);

  if (strncmp (model_name, "MFS-12000CX", 11) == 0)
    {
      dev->x_range.max = SANE_FIX (8.5 * MM_PER_INCH);
      dev->y_range.max = SANE_FIX (13.85 * MM_PER_INCH);
      dev->dpi_range.max = SANE_FIX (1200);
      dev->flags |= MUSTEK_FLAG_USE_EIGHTS;
    }
  else if (strncmp (model_name, "MFS-06000CX", 11) == 0)
    {
      dev->x_range.max = SANE_FIX (8.5 * MM_PER_INCH);
      dev->y_range.max = SANE_FIX (13.85 * MM_PER_INCH);
      dev->dpi_range.max = SANE_FIX (600);
      dev->flags |= MUSTEK_FLAG_USE_EIGHTS;
    }
  else if (strncmp (model_name, "MSF-12000SP", 11) == 0)
    {
      dev->x_range.max = SANE_FIX (8.5 * MM_PER_INCH);  /* is this correct? */
      dev->y_range.max = SANE_FIX (13.85 * MM_PER_INCH);/* is this correct? */
      dev->dpi_range.max = SANE_FIX (1200);
      /* This is a bit of a guess.  The reports by Andreas Gaumann
	 indicate that at least the MSF-06000SP with firmware revision
	 3.12 does not require any line-distance correction.  I'm
	 guessing this is true for all firmware revisions and for
	 MSF-12000SP and MSF-06000SP. */
      dev->flags |= MUSTEK_FLAG_LD_NONE;
    }
  else if (strncmp (model_name, "MSF-08000SP", 11) == 0)
    {
      dev->x_range.max = SANE_FIX (8.50 * MM_PER_INCH);
      dev->y_range.max = SANE_FIX (13.85 * MM_PER_INCH);
      dev->dpi_range.max = SANE_FIX (800);
      /* This is a bit of a guess.  The reports by Andreas Gaumann
	 indicate that at least the MSF-06000SP with firmware revision
	 3.12 does not require any line-distance correction.  I'm
	 guessing this is true for all firmware revisions and for
	 MSF-12000SP and MSF-06000SP. */
      dev->flags |= MUSTEK_FLAG_LD_NONE;
    }
  else if (strncmp (model_name, "MSF-06000SP", 11) == 0)
    {
      dev->x_range.max = SANE_FIX (220);		/* measured */
      dev->y_range.max = SANE_FIX (360);		/* measured */
      dev->dpi_range.max = SANE_FIX (600);
      /* This is a bit of a guess.  The reports by Andreas Gaumann
	 indicate that at least the MSF-06000SP with firmware revision
	 3.12 does not require any line-distance correction.  I'm
	 guessing this is true for all firmware revisions and for
	 MSF-12000SP and MSF-06000SP. */
      dev->flags |= MUSTEK_FLAG_LD_NONE;
    }
  else if (strncmp (model_name, "MFC-08000CZ", 11) == 0)
    {
      dev->x_range.max = SANE_FIX (220.0);		/* measured */
      dev->y_range.max = SANE_FIX (292.0);		/* measured */
      dev->dpi_range.max = SANE_FIX (800);
    }
  else if (strncmp (model_name, "MFC-06000CZ", 11) == 0)
    {
      dev->x_range.max = SANE_FIX (220.0);		/* measured */
      dev->y_range.max = SANE_FIX (292.0);		/* measured */
      dev->dpi_range.max = SANE_FIX (600);
    }
  /* No documentation for these, but they do exist.  Duh... */
  else if (strncmp (model_name, "MFS-12000SP", 11) == 0)
    {
      dev->x_range.max = SANE_FIX (8.5 * MM_PER_INCH);
      dev->y_range.max = SANE_FIX (13.85 * MM_PER_INCH);
      /* This is a conservative setting.  It is 3mm less than what the
         windows driver uses, but this way the scanner doesn't make
         ugly noises. */
      dev->x_trans_range.max = SANE_FIX (8.0 * MM_PER_INCH);
      dev->y_trans_range.max = SANE_FIX (9.7 * MM_PER_INCH);

      dev->dpi_range.max = SANE_FIX (1200);
      /* Revision 1.00 of the MFS-12000SP firmware is buggy and needs
	 this workaround.  Maybe others need it too, but this one is
	 for sure.  We know for certain that revision 1.02 or newer
	 has this bug fixed.  */
      if (fw_revision < 0x102)
	dev->flags |= MUSTEK_FLAG_LD_MFS;
      else
	dev->flags |= MUSTEK_FLAG_LD_NONE;
    }
  else if (strncmp (model_name, "MFS-08000SP", 11) == 0)
    {
      dev->x_range.max = SANE_FIX (8.50 * MM_PER_INCH);
      dev->y_range.max = SANE_FIX (13.85 * MM_PER_INCH);
      dev->dpi_range.max = SANE_FIX (800);
    }
  else if (strncmp (model_name, "MFS-06000SP", 11) == 0)
    {
      dev->x_range.max = SANE_FIX (8.5 * MM_PER_INCH);  /* is this correct? */
      dev->y_range.max = SANE_FIX (13.84 * MM_PER_INCH);/* is this correct? */
      dev->dpi_range.max = SANE_FIX (600);
    }
  else if (strncmp(model_name, "MSF-06000CZ", 11) == 0)
    {
      dev->x_range.max = SANE_FIX (8.5 * MM_PER_INCH);
      dev->y_range.max = SANE_FIX (13.85 * MM_PER_INCH);
      dev->dpi_range.max = SANE_FIX (600);
      dev->flags |= MUSTEK_FLAG_USE_EIGHTS | MUSTEK_FLAG_DOUBLE_RES;
    }
  else if (strncmp(model_name, " C03", 4) == 0)
    {
      dev->x_range.max = SANE_FIX (216);
      dev->y_range.min = SANE_FIX (2.5);
      dev->y_range.max = SANE_FIX (294.5);
      dev->dpi_range.max = SANE_FIX (600);
      dev->dpi_range.min = SANE_FIX (75);
      dev->flags |= MUSTEK_FLAG_SE;
    }
  else if (strncmp(model_name, " C04", 4) == 0)
    {
      dev->x_range.max = SANE_FIX (216);
      dev->y_range.min = SANE_FIX (2.5);
      dev->y_range.max = SANE_FIX (294.5);
      dev->dpi_range.max = SANE_FIX (800);
      dev->dpi_range.min = SANE_FIX (75);
      dev->flags |= MUSTEK_FLAG_SE;
    }
  else if (strncmp(model_name, " C06", 4) == 0)
    {
      dev->x_range.max = SANE_FIX (216);		/* measured	*/
      dev->y_range.min = SANE_FIX (2.5);
      dev->y_range.max = SANE_FIX (294.5);		/* measured	*/
      dev->dpi_range.max = SANE_FIX (1200);
      dev->dpi_range.min = SANE_FIX (75);
      dev->flags |= MUSTEK_FLAG_SE;
    }
  else if (strncmp(model_name, " C12", 4) == 0)
    {
      dev->x_range.max = SANE_FIX (216);
      dev->y_range.max = SANE_FIX (294.5);
      dev->dpi_range.max = SANE_FIX (1200);
      dev->dpi_range.min = SANE_FIX (75); 
      dev->flags |= MUSTEK_FLAG_SE;
    }
  else
    {
      DBG(1, "attach: unknown model `%s'\n", model_name);
      free (dev);
      return SANE_STATUS_INVAL;
    }

  if (dev->flags & MUSTEK_FLAG_LD_MFS)
    DBG(1, "attach: using special line-distance algorithm\n");
  if (dev->flags & MUSTEK_FLAG_LD_NONE)
    DBG(1, "attach: scanner has automatic line-distance correction\n");
  if (dev->flags & MUSTEK_FLAG_SE)
    dev->flags |= MUSTEK_FLAG_SINGLE_PASS;
  else    
    {
      if (result[57] & (1 << 6))
        dev->flags |= MUSTEK_FLAG_SINGLE_PASS;
      else
        {
          /* three-pass scanners quantize to 1% of the maximum resolution/2: */
          dev->dpi_range.quant = dev->dpi_range.max / 2 / 100;
          dev->dpi_range.min = dev->dpi_range.quant;
        }
      if (result[63] & (1 << 2))
        dev->flags |= MUSTEK_FLAG_ADF;
      if (result[63] & (1 << 3))
        dev->flags |= MUSTEK_FLAG_ADF_READY;
      if (result[63] & (1 << 6))
        dev->flags |= MUSTEK_FLAG_TA;
    }

  if (! (result[62] & (1 << 0)))
    {
      DBG(1, "attach: cover open\n");
    }
          
  DBG(2, "attach: found Mustek scanner model %s (%s), %s%s%s%s\n",
      dev->sane.model, dev->sane.type,
      (dev->flags & MUSTEK_FLAG_SINGLE_PASS) ? "1-pass" : "3-pass",
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

  if (option == OPT_RESOLUTION && !(s->hw->flags & MUSTEK_FLAG_SINGLE_PASS))
    {
      /* The three pass scanners use a 1% of the maximum resolution
         increment for resolutions less than or equal to half of the
         maximum resolution and a 10% of the maximum resolution
         increment for larger resolutions.  We can't represent this
         easily in SANE, so the constraint is simply for 1dpi and then
         we round to the 10% increments if necessary.  */
      SANE_Fixed max_dpi, quant, half_res;

      w = *(SANE_Word *) value;
      max_dpi = s->hw->dpi_range.max;
      half_res = max_dpi / 2;

      if (w > half_res)
	{
	  /* round to 10% step: */
	  quant = (half_res / 10);
	  dpi = (w + quant / 2) / quant;
	  dpi *= quant;
	  if (dpi != w)
	    {
	      *(SANE_Word *) value = dpi;
	      if (info)
		*info |= SANE_INFO_INEXACT;
	    }
	}
    }
  return sanei_constrain_value (s->opt + option, value, info);
}

/* Quantize s->req.resolution and return the resolution code for the
   quantized resolution.  Quantization depends on scanner type (single
   pass vs. three-pass).  */
static int
encode_resolution (Mustek_Scanner *s)
{
  SANE_Fixed max_dpi, dpi;
  int code, mode = 0;

  dpi = s->val[OPT_RESOLUTION].w;

  if (s->hw->flags & MUSTEK_FLAG_SINGLE_PASS)
    code = dpi >> SANE_FIXED_SCALE_SHIFT;
  else
    {
      SANE_Fixed quant, half_res;

      max_dpi = s->hw->dpi_range.max;
      half_res = max_dpi / 2;

      if (s->hw->flags & MUSTEK_FLAG_DOUBLE_RES)
	{
	  dpi /= 2;
	  mode = 0x100;		/* indicate double resultion */
	}

      if (dpi <= half_res)
	{
	  quant = half_res / 100;
	  code = (dpi + quant / 2) / quant;
	  if (code < 1)
	    code = 1;
	}
      else
	{
	  /* quantize to 10% step: */
	  quant = (half_res / 10);
	  code = (dpi + quant / 2) / quant;
	  mode = 0x100;	/* indicate 10% quantization */
	}
    }
  return code | mode;
}

static int
encode_percentage (Mustek_Scanner *s, double value, double quant)
{
  int max, code, sign = 0;

  if (s->hw->flags & MUSTEK_FLAG_SINGLE_PASS)
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
  else
    {
      code = (int) ((value / quant) + 12.5);
      max = 0x18;
    }
  if (code > max)
    code = max;
  if (code < 0)
    code = 0x00;
  return code;
}

static SANE_Status
scan_area_and_windows (Mustek_Scanner *s)
{
  u_int8_t cmd[117], *cp;
  int i, dim;

  /* setup SCSI command (except length): */
  memset (cmd, 0, sizeof (cmd));
  cmd[0] = MUSTEK_SCSI_AREA_AND_WINDOWS;

  cp = cmd + 6;

  /* fill in frame header: */

  if (s->hw->flags & MUSTEK_FLAG_USE_EIGHTS)
    {
      double eights_per_mm = 8 / MM_PER_INCH;

      /*
       * The MSF-06000CZ seems to lock-up if the pixel-unit is used.
       * Using 1/8" works.
       */
      *cp++ = ((s->mode & MUSTEK_MODE_HALFTONE) ? 0x01 : 0x00);
      STORE16L(cp, SANE_UNFIX(s->val[OPT_TL_X].w) * eights_per_mm + 0.5);
      STORE16L(cp, SANE_UNFIX(s->val[OPT_TL_Y].w) * eights_per_mm + 0.5);
      STORE16L(cp, SANE_UNFIX(s->val[OPT_BR_X].w) * eights_per_mm + 0.5);
      STORE16L(cp, SANE_UNFIX(s->val[OPT_BR_Y].w) * eights_per_mm + 0.5);
    }
  else
    {
      double pixels_per_mm = SANE_UNFIX (s->hw->dpi_range.max) / MM_PER_INCH;

      /* pixel unit and halftoning: */  
      *cp++ = 0x8 | ((s->mode & MUSTEK_MODE_HALFTONE) ? 0x01 : 0x00);

      /* fill in scanning area: */
      STORE16L(cp, SANE_UNFIX (s->val[OPT_TL_X].w) * pixels_per_mm + 0.5);
      STORE16L(cp, SANE_UNFIX (s->val[OPT_TL_Y].w) * pixels_per_mm + 0.5);
      STORE16L(cp, SANE_UNFIX (s->val[OPT_BR_X].w) * pixels_per_mm + 0.5);
      STORE16L(cp, SANE_UNFIX (s->val[OPT_BR_Y].w) * pixels_per_mm + 0.5);
    }
  
  dim = s->val[OPT_HALFTONE_DIMENSION].w;
  if (dim)
    {
      *cp++ = 0x40;			/* mark presence of user pattern */
      *cp++ = (dim << 4) | dim;		/* set pattern length */
      for (i = 0; i < dim * dim; ++i)
	*cp++ = s->val[OPT_HALFTONE_PATTERN].wa[i];
    }

  cmd[4] = (cp - cmd) - 6;
  return dev_cmd (s, cmd, (cp - cmd), 0, 0);
}

static SANE_Status
do_set_window (Mustek_Scanner *s, int lamp)
{
  u_int8_t cmd[58], *cp;
  double pixels_per_mm;
  int offset;

  /* setup SCSI command (except length): */
  memset (cmd, 0, sizeof (cmd));
  cmd[0] = MUSTEK_SCSI_SET_WINDOW;
  cp = cmd + sizeof (set_window);	/* skip command block		*/

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
      *cp++ = 0x05;			/* actually not used !		*/
      *cp++ = 24;			/* 24 bits/pixel in color mode	*/
    }
  else if (s->mode & MUSTEK_MODE_MULTIBIT)
    {
      *cp++ = 0x02;   			/* actually not used !		*/
      *cp++ = 8;			/* 8 bits/pixel in gray mode	*/
    }
  else 
    {
      *cp++ = 0x00;			/* actually not used !		*/
      *cp++ = 1;			/* 1 bit/pixel in lineart mode	*/
    }
          
  cp += 13;				/* skip reserved bytes		*/
  *cp++ = lamp;				/* 0 = normal, 1 = on, 2 = off  */
  cp += 7;				/* skip reserved bytes		*/
  
  cmd[8] = cp - cmd - sizeof (set_window);
  return dev_cmd (s, cmd, (cp - cmd), 0, 0);
}

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

static SANE_Status
send_gamma_se (Mustek_Scanner *s)
{
  SANE_Status status;
  u_int8_t gamma[10 + 4096], *cp;
  int color, factor, val_a, val_b;
  int i, j;
# define CLIP(x)	((x) < 0 ? 0 : ((x) > 255 ? 255 : (x)))

  memset (gamma, 0, sizeof (send_data));

  gamma[0] = MUSTEK_SCSI_SEND_DATA;
  gamma[2] = 0x03;			/* indicates gamma table */

  if (s->mode & MUSTEK_MODE_MULTIBIT)
    {
      if (s->hw->gamma_length + sizeof (send_data) > sizeof (gamma))
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
	  cp = gamma + sizeof (send_data);
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

	  DBG(3, "send_gamma_se: sending table for color %d\n", gamma[6]);
	  status = dev_cmd (s, gamma, sizeof (send_data) 
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
   
      DBG(3, "send_gamma_se: sending lineart threshold %2X\n", gamma[8]);
      return dev_cmd (s, gamma, sizeof (send_data), 0, 0);
    }
}  

static SANE_Status
mode_select (Mustek_Scanner *s, int color_code)
{
  int grain_code, speed_code;
  u_int8_t mode[19], *cp;

  /* the scanners use a funky code for the grain size, let's compute it: */
  grain_code = s->val[OPT_GRAIN_SIZE].w;
  if (grain_code > 7)
    grain_code = 7;		/* code 0 is 8x8, not 7x7 */
  grain_code = 7 - grain_code;

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
  speed_code = 4 - speed_code;	/* 0 is fast, 4 is slow */

  memset (mode, 0, sizeof (mode));
  mode[0] = MUSTEK_SCSI_MODE_SELECT;

  /* set command length and resolution code: */
  if (s->hw->flags & MUSTEK_FLAG_SINGLE_PASS)
    {
      mode[4] = 0x0d;
      cp = mode + 17;
      STORE16L(cp, s->resolution_code);
    }
  else
    {
      mode[4] = 0x0b;
      mode[7] = s->resolution_code;
    }
  /* set mode byte: */
  mode[6] = 0x83 | (color_code << 5);
  if (!(s->hw->flags & MUSTEK_FLAG_USE_EIGHTS))
    mode[6] |= 0x08;
  if (s->val[OPT_HALFTONE_DIMENSION].w)
    mode[6] |= 0x10;
  mode[8] = encode_percentage (s, SANE_UNFIX (s->val[OPT_BRIGHTNESS].w), 3.0);
  mode[9] = encode_percentage (s, SANE_UNFIX (s->val[OPT_CONTRAST].w), 7.0);
  mode[10] = grain_code;
  mode[11] = speed_code;	/* lamp setting not supported yet */
  mode[12] = 0;			/* shadow param not used by Mustek */
  mode[13] = 0;			/* highlist param not used by Mustek */
  mode[14] = mode[15] = 0;	/* paperlength not used by Mustek */
  mode[16] = 0;			/* midtone param not used by Mustek */

  return dev_cmd (s, mode, 6 + mode[4], 0, 0);
}

/* According to Mustek, the only builtin gamma table is a linear
   table, so all we support here is user-defined gamma tables.  */
static SANE_Status
gamma_correction (Mustek_Scanner *s, int color_code)
{
  int i, j, table = 0, len, num_channels = 1;
  u_int8_t gamma[3*256+10], val, *cp;

  if ((s->hw->flags & MUSTEK_FLAG_PP)
      && !(s->mode & MUSTEK_MODE_MULTIBIT))
    {
      /* sigh! - the 600 II N needs a (dummy) table download even for
	 lineart and halftone mode, else it produces a completely
	 white image. Thank Mustek for their buggy firmware !  */
      memset (gamma, 0, sizeof (gamma));
      gamma[0] = MUSTEK_SCSI_LOOKUP_TABLE;
      gamma[2] = 0x0;		/* indicate any preloaded gamma table */
      DBG(3, "gamma_correction: sending dummy gamma table\n");
      return dev_cmd (s, gamma, 6, 0, 0);
    }

  if (!s->val[OPT_CUSTOM_GAMMA].w
      || !(s->mode & MUSTEK_MODE_MULTIBIT))
    return SANE_STATUS_GOOD;

  if (s->mode & MUSTEK_MODE_COLOR)
    {
      table = 1;
      if (s->hw->flags & MUSTEK_FLAG_SINGLE_PASS)
	{
	  if (color_code == MUSTEK_CODE_GRAY)
	    num_channels = 3;
	  else
	    table = color_code;
	}
      else
	table += s->pass;
    }

  if (s->mode & MUSTEK_MODE_MULTIBIT)
    len = num_channels*256;
  else
    len = 0;

  memset (gamma, 0, sizeof (gamma));
  gamma[0] = MUSTEK_SCSI_LOOKUP_TABLE;
  gamma[2] = 0x27;		/* indicate user-selected gamma table */
  gamma[7] = (len >> 8) & 0xff;	/* big endian! */
  gamma[8] = (len >> 0) & 0xff;
  gamma[9] = (color_code << 6);
  if (len > 0)
    {
      cp = gamma + 10;
      for (j = 0; j < num_channels; ++j, ++table)
	for (i = 0; i < 256; ++i)
	  {
	    val = s->gamma_table[table][i];
	    if (s->mode & MUSTEK_MODE_COLOR)
	      /* compose intensity gamma and color channel gamma: */
	      val = s->gamma_table[0][val];
	    *cp++ = val;
	  }
    }
  DBG(3, "gamma_correction: sending gamma table of %d bytes\n", len);
  return dev_cmd (s, gamma, 10 + len, 0, 0);
}

static SANE_Status
send_gamma_table (Mustek_Scanner * s)
{
  SANE_Status status;

  if (s->one_pass_color_scan)
    {
      if (s->hw->flags & MUSTEK_FLAG_PP)
	/* This _should_ work for all one-pass scanners (not just
	   parallel-port scanners), but it doesn't work for my Paragon
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

static SANE_Status
start_scan (Mustek_Scanner *s)
{
  u_int8_t start[6];

  memset (start, 0, sizeof (start));
  start[0] = MUSTEK_SCSI_START_STOP;
  start[4] = 0x01;

  /* ScanExpress models don't have any variants */
  if (! (s->hw->flags & MUSTEK_FLAG_SE) )
    {
      if (s->mode & MUSTEK_MODE_COLOR)
        {
          if (s->hw->flags & MUSTEK_FLAG_SINGLE_PASS)
	    start[4] |= 0x20;
          else
	    start[4] |= ((s->pass + 1) << 3);
        }
      /* or in single/multi bit: */
      start[4] |= (s->mode & MUSTEK_MODE_MULTIBIT) ? (1 << 6) : 0;

      if (!(s->hw->flags & MUSTEK_FLAG_SINGLE_PASS))
        /* or in expanded resolution bit: */
        start[4] |= (s->resolution_code & 0x100) >> 1;
    }
    
  return dev_cmd (s, start, sizeof (start), 0, 0);
}

static SANE_Status
stop_scan (Mustek_Scanner *s)
{
  return dev_cmd (s, stop, sizeof (stop), 0, 0);
}

static SANE_Status
do_eof (Mustek_Scanner *s)
{
  if (s->pipe >= 0)
    {
      close (s->pipe);
      s->pipe = -1;
    }
  return SANE_STATUS_EOF;
}

static SANE_Status
do_stop (Mustek_Scanner *s)
{
  SANE_Status status = SANE_STATUS_GOOD;

  if (!s->scanning)
    status = SANE_STATUS_CANCELLED;

  s->scanning = SANE_FALSE;
  s->pass = 0;

  do_eof (s);

  if (s->reader_pid > 0)
    {
      int exit_status;

      /* ensure child knows it's time to stop: */
      DBG(4, "do_stop: terminating reader process\n");
      kill (s->reader_pid, SIGTERM);
      while (wait (&exit_status) != s->reader_pid);
      DBG(4, "do_stop: reader process terminated with status 0x%x\n",
	  exit_status);
      if (status != SANE_STATUS_CANCELLED && WIFEXITED(exit_status))
	status = WEXITSTATUS(exit_status);
      s->reader_pid = 0;
    }

  if (s->fd >= 0)
    {
      if (status == SANE_STATUS_CANCELLED)
	{
	  DBG(4, "do_stop: waiting for scanner to become ready\n");
	  dev_wait_ready (s);
	}
      DBG(4, "do_stop: sending STOP command\n");
      stop_scan (s);
      DBG(4, "do_stop: closing scanner\n");
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
      /* At least the MFS12000SP scanner needs a special form of
	 line-distance correction and goes wild if they receive an LD
	 command. */
      s->ld.peak_res = res;
      return SANE_STATUS_GOOD;
    }

  len = sizeof (result);
  status = dev_cmd (s, distance, sizeof (distance), result, &len);
  if (status != SANE_STATUS_GOOD)
    return status;

  DBG(1, "line_distance: got factor=%d, (r/g/b)=(%d/%d/%d)\n",
      result[0] | (result[1] << 8), result[2], result[3], result[4]);

  if (s->hw->flags & MUSTEK_FLAG_LD_FIX)
    {
      result[0] = 0xff;
      result[1] = 0xff;
      if (s->mode & MUSTEK_MODE_COLOR)
	{
	  if (s->hw->flags & MUSTEK_FLAG_PP)
	    {
	      /* According to Andreas Czechanowski, the line-distance
		 values returned for the parallel-port scanners are
		 garbage, so we have to fix things up manually.  Not
		 good.  */
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
      DBG(1, "line_distance: fixed up to factor=%d, (r/g/b)=(%d/%d/%d)\n",
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

      if (s->hw->flags & MUSTEK_FLAG_PP)
	{
	  for (color = 0; color < 3; ++color)
	    s->ld.index[color] = -s->ld.dist[color];
	  s->ld.lmod3 = -1;
	}
    }
  return SANE_STATUS_GOOD;
}

static SANE_Status
get_image_status (Mustek_Scanner *s, SANE_Int *bpl, SANE_Int *lines)
{
  u_int8_t result[6];
  SANE_Status status;
  size_t len;
  int busy;

  do
    {
      len = sizeof (result);
      status = dev_cmd (s, get_status, sizeof (get_status), result, &len);
      if (status != SANE_STATUS_GOOD)
	return status;

      busy = result[0];
      if (busy)
	usleep (100000);

      if (!s->scanning)
	return do_stop (s);
    }
  while (busy);

  s->hw->bpl = result[1] | (result[2] << 8);
  s->hw->lines = result[3] | (result[4] << 8) | (result[5] << 16);

  *bpl = s->hw->bpl;
  *lines = s->hw->lines;
  DBG(2, "get_image_status: bytes_per_line=%d, lines=%d\n",
      *bpl, *lines);
  return SANE_STATUS_GOOD;
}

static SANE_Status
do_get_window (Mustek_Scanner *s, SANE_Int *bpl, SANE_Int *lines,
	       SANE_Int *pixels)
{
  u_int8_t result[48];
  SANE_Status status;
  size_t len;
  int color;

  len = sizeof (result);
  status = dev_cmd (s, get_window, sizeof (get_window), result, &len);
  if (status != SANE_STATUS_GOOD)
    return status;

  if (!s->scanning)
    return do_stop (s);

  s->hw->cal.bytes = (result[6] << 24) | (result[7] << 16) |
		     (result[8] <<  8) | (result[9] <<  0);
  s->hw->cal.lines = (result[10] << 24) | (result[11] << 16) |
		     (result[12] <<  8) | (result[13] <<  0);

  DBG(3, "get_window: calib_bytes=%d, calib_lines=%d\n",
      s->hw->cal.bytes, s->hw->cal.lines);

  s->hw->bpl = (result[14] << 24) | (result[15] << 16) |
	       (result[16] <<  8) | result[17];  
  s->hw->lines = (result[18] << 24) | (result[19] << 16) | 
		 (result[20] <<  8) | result[21];

  DBG(2, "get_window: bytes_per_line=%d, lines=%d\n",
      s->hw->bpl, s->hw->lines);
      
  s->hw->gamma_length = 1 << result[26];
  DBG(2, "get_window: gamma length=%d\n",  s->hw->gamma_length);    

  if (s->mode & MUSTEK_MODE_COLOR) 
    {
      long res;

      s->ld.buf[0] = NULL;
      for (color = 0; color < 3; ++color)
	{
	  s->ld.dist[color] = result[42 + color];
	}
    
      DBG(1, "line_distance: got res=%d, (r/g/b)=(%d/%d/%d)\n",
	  (result[40] << 8) | result[41], s->ld.dist[0], 
	  s->ld.dist[1], s->ld.dist[2]);

      /* In color mode scale the image according to desired resolution */

      res = SANE_UNFIX (s->val[OPT_RESOLUTION].w);
      *bpl = *pixels = (((s->hw->bpl / 3 ) * res) / s->ld.peak_res) * 3;
      *lines = ((s->hw->lines - s->ld.dist[2]) * res) / s->ld.peak_res;
    }
  else
    {
      /* Linart and gray seems to work with arbitrary resolution */
      *bpl = s->hw->bpl;
      *lines = s->hw->lines;
    }    
  
  DBG(2, "get_window: bytes_per_line=%d, lines=%d, pixels=%d\n",
      *bpl, *lines, *pixels);

  return SANE_STATUS_GOOD;
}

static SANE_Status
backtrack_and_adf (Mustek_Scanner *s)
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

  return dev_cmd (s, backtrack, sizeof (backtrack), 0, 0);
}

/* MFS scanner have three separate sensor bars (one per primary color)
   and these sensor bars are vertically 1/72" apart from each other.
   So when scanning at a resolution of RES dots/inch, then the first
   red strip goes with the green strip that is dy=round(RES/72)
   further down and the blue strip that is 2*dy further down.  */
static void
fix_line_distance_mfs (Mustek_Scanner * s, int num_lines, int bpl,
		       u_int8_t *raw, u_int8_t *out)
{
  u_int8_t *out_ptr, *ptr, *ptr_end, *src;
  u_int y, dy;
  int bpc;
# define RED	0
# define GRN	1	/* green, spelled funny */

  bpc = bpl / 3;
  dy = (s->ld.peak_res + 36) / 72; 

  if (!s->ld.buf[RED])
    {
      /* The red buffer must be able to hold up to 2*dy lines whereas
	 the green buffer must be able to hold up to 1*dy lines. */
      s->ld.buf[RED] = malloc (3 * dy * (long) bpc);
      s->ld.buf[GRN] = s->ld.buf[RED] + 2 * dy * bpc;
    }

  /* restore the red and green lines from the previous buffer: */
  for (y = 0; y < 2*dy && y < num_lines; ++y)
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
  for (y = 0; y < dy && y < num_lines; ++y)
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

  for (y = 0; y < num_lines; ++y)
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
      if (y >= 0*dy)
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
      if (num_lines - 2*dy + y >= 0)
	src = raw + (num_lines - 2*dy + y)*bpl;
      else
	src = s->ld.buf[RED] + (y + num_lines)*bpc;
      memcpy (s->ld.buf[RED] + y*bpc, src, bpc);
    }
  for (y = 0; y < 1*dy; ++y)
    {
      if (num_lines - 1*dy + y >= 0)
	src = raw + (num_lines - 1*dy + y)*bpl + bpc;
      else
	src = s->ld.buf[GRN] + (y + num_lines)*bpc;
      memcpy (s->ld.buf[GRN] + y*bpc, src, bpc);
    }
}

static int
fix_line_distance_pp (Mustek_Scanner *s, int num_lines, int bpl,
		      u_int8_t *raw, u_int8_t *out)
{
  u_int8_t *out_end, *out_ptr, *raw_end = raw + num_lines * bpl;
  int c, num_saved_lines, line;

  if (!s->ld.buf[0])
    {
      /* This buffer must be big enough to hold maximum line distance
         times max_bpl bytes.  The maximum line distance for 600 dpi
         parallel-port scanners is 23.  We use 32 to play it safe...  */
      DBG(2, "fix_line_distance_pp: allocating temp buffer of %d*%d bytes\n",
	  32, bpl);
      s->ld.buf[0] = malloc (32 * (long) bpl);
      if (!s->ld.buf[0])
	{
	  DBG(1, "fix_line_distance_pp: failed to malloc temporary buffer\n");
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
		  DBG (1, "fix_line_distance_pp: lmod3=%d, index=(%d,%d,%d)\n",
		       s->ld.lmod3,
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
      DBG(2, "fix_line_distance_se: allocating temp buffer of %d*%d bytes\n",
	  3 * MAX_LINE_DIST, bpc);
      s->ld.buf[0] = malloc (3 * MAX_LINE_DIST * (long) bpc);

      if (!s->ld.buf[0])
	{
	  DBG(1, "fix_line_distance_se: failed to malloc temporary buffer\n");
	  return 0;
	}

      /* Note that either s->ld.buf[1] or  s->ld.buf[2] is never user. */
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
  DBG(4, "start color: %d\n", s->ld.color);
  DBG(4, "read lines: %d\n", num_lines);

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

  DBG(4, "saved lines: %d/%d/%d\n", s->ld.saved[0], s->ld.saved[1],
      s->ld.saved[2]); 
  DBG(4, "available:  %d/%d/%d\n", lines[0], lines[1], lines[2]);
  DBG(4, "triples: %d\n", num_lines);

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

static void
fix_line_distance_normal (Mustek_Scanner *s, int num_lines, int bpl,
			  u_int8_t *raw, u_int8_t *out)
{
  u_int8_t *out_end, *out_ptr, *raw_end = raw + num_lines * bpl;
  int index[3];			/* index of the next output line for color C */
  int i, color;

  /* Initialize the indices with the line distances that were returned
     by the CCD linedistance command.  We want to skip data for the
     first OFFSET rounds, so we initialize the indices to the negative
     of this offset.  */
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

static SANE_Status
init_options (Mustek_Scanner *s)
{
  int i;

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
  if (s->hw->flags & MUSTEK_FLAG_PP)
    {
      s->opt[OPT_MODE].size = max_string_size (pp_mode_list);
      s->opt[OPT_MODE].constraint.string_list = pp_mode_list;
      s->val[OPT_MODE].s = strdup (pp_mode_list[2]);
    }
  else if (s->hw->flags & MUSTEK_FLAG_SE)
    {
      s->opt[OPT_MODE].size = max_string_size (se_mode_list);
      s->opt[OPT_MODE].constraint.string_list = se_mode_list;
      s->val[OPT_MODE].s = strdup (se_mode_list[1]);
    }
  else
    {
      s->opt[OPT_MODE].size = max_string_size (mode_list);
      s->opt[OPT_MODE].constraint.string_list = mode_list;
      s->val[OPT_MODE].s = strdup (mode_list[2]);
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
  s->val[OPT_SPEED].s = strdup (speed_list[2]);

  /* source */
  s->opt[OPT_SOURCE].name = SANE_NAME_SCAN_SOURCE;
  s->opt[OPT_SOURCE].title = SANE_TITLE_SCAN_SOURCE;
  s->opt[OPT_SOURCE].desc = SANE_DESC_SCAN_SOURCE;
  s->opt[OPT_SOURCE].type = SANE_TYPE_STRING;
  s->opt[OPT_SOURCE].size = max_string_size (source_list);
  s->opt[OPT_SOURCE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_SOURCE].constraint.string_list = source_list;
  s->val[OPT_SOURCE].s = strdup (source_list[0]);

  /* backtrack */
  s->opt[OPT_BACKTRACK].name = SANE_NAME_BACKTRACK;
  s->opt[OPT_BACKTRACK].title = SANE_TITLE_BACKTRACK;
  s->opt[OPT_BACKTRACK].desc = SANE_DESC_BACKTRACK;
  s->opt[OPT_BACKTRACK].type = SANE_TYPE_BOOL;
  s->val[OPT_BACKTRACK].w = SANE_FALSE;
  
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

  /* grain-size */
  s->opt[OPT_GRAIN_SIZE].name = SANE_NAME_GRAIN_SIZE;
  s->opt[OPT_GRAIN_SIZE].title = SANE_TITLE_GRAIN_SIZE;
  s->opt[OPT_GRAIN_SIZE].desc = SANE_DESC_GRAIN_SIZE;
  s->opt[OPT_GRAIN_SIZE].type = SANE_TYPE_INT;
  s->opt[OPT_GRAIN_SIZE].unit = SANE_UNIT_PIXEL;
  s->opt[OPT_GRAIN_SIZE].constraint_type = SANE_CONSTRAINT_WORD_LIST;
  s->opt[OPT_GRAIN_SIZE].constraint.word_list = grain_list;
  s->val[OPT_GRAIN_SIZE].w = grain_list[1];

  /* brightness */
  s->opt[OPT_BRIGHTNESS].name = SANE_NAME_BRIGHTNESS;
  s->opt[OPT_BRIGHTNESS].title = SANE_TITLE_BRIGHTNESS;
  s->opt[OPT_BRIGHTNESS].desc = SANE_DESC_BRIGHTNESS
    "  This option is active for lineart/halftone modes only.  "
    "For multibit modes (grey/color) use the gamma-table(s).";
  s->opt[OPT_BRIGHTNESS].type = SANE_TYPE_FIXED;
  s->opt[OPT_BRIGHTNESS].unit = SANE_UNIT_PERCENT;
  s->opt[OPT_BRIGHTNESS].constraint_type = SANE_CONSTRAINT_RANGE;
  if (s->hw->flags & MUSTEK_FLAG_SINGLE_PASS)
    {
      /* 1-pass scanners don't support brightness in colormode: */
      s->opt[OPT_BRIGHTNESS].cap |= SANE_CAP_INACTIVE;
      s->opt[OPT_BRIGHTNESS].constraint.range = &percentage_range;
    }
  else
    s->opt[OPT_BRIGHTNESS].constraint.range = &ax_brightness_range;
  s->val[OPT_BRIGHTNESS].w = 0;

  /* contrast */
  s->opt[OPT_CONTRAST].name = SANE_NAME_CONTRAST;
  s->opt[OPT_CONTRAST].title = SANE_TITLE_CONTRAST;
  s->opt[OPT_CONTRAST].desc = SANE_DESC_CONTRAST
    "  This option is active for lineart/halftone modes only.  "
    "For multibit modes (grey/color) use the gamma-table(s).";
  s->opt[OPT_CONTRAST].type = SANE_TYPE_FIXED;
  s->opt[OPT_CONTRAST].unit = SANE_UNIT_PERCENT;
  s->opt[OPT_CONTRAST].constraint_type = SANE_CONSTRAINT_RANGE;
  if (s->hw->flags & MUSTEK_FLAG_SINGLE_PASS)
    {
      /* 1-pass scanners don't support contrast in colormode: */
      s->opt[OPT_CONTRAST].cap |= SANE_CAP_INACTIVE;
      s->opt[OPT_CONTRAST].constraint.range = &percentage_range;
    }
  else
    s->opt[OPT_CONTRAST].constraint.range = &ax_contrast_range;
  s->val[OPT_CONTRAST].w = 0;

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
  s->opt[OPT_GAMMA_VECTOR].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_GAMMA_VECTOR].constraint.range = &u8_range;
  s->val[OPT_GAMMA_VECTOR].wa = &s->gamma_table[0][0];

  /* red gamma vector */
  s->opt[OPT_GAMMA_VECTOR_R].name = SANE_NAME_GAMMA_VECTOR_R;
  s->opt[OPT_GAMMA_VECTOR_R].title = SANE_TITLE_GAMMA_VECTOR_R;
  s->opt[OPT_GAMMA_VECTOR_R].desc = SANE_DESC_GAMMA_VECTOR_R;
  s->opt[OPT_GAMMA_VECTOR_R].type = SANE_TYPE_INT;
  s->opt[OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_GAMMA_VECTOR_R].unit = SANE_UNIT_NONE;
  s->opt[OPT_GAMMA_VECTOR_R].size = 256 * sizeof (SANE_Word);
  s->opt[OPT_GAMMA_VECTOR_R].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_GAMMA_VECTOR_R].constraint.range = &u8_range;
  s->val[OPT_GAMMA_VECTOR_R].wa = &s->gamma_table[1][0];

  /* green gamma vector */
  s->opt[OPT_GAMMA_VECTOR_G].name = SANE_NAME_GAMMA_VECTOR_G;
  s->opt[OPT_GAMMA_VECTOR_G].title = SANE_TITLE_GAMMA_VECTOR_G;
  s->opt[OPT_GAMMA_VECTOR_G].desc = SANE_DESC_GAMMA_VECTOR_G;
  s->opt[OPT_GAMMA_VECTOR_G].type = SANE_TYPE_INT;
  s->opt[OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_GAMMA_VECTOR_G].unit = SANE_UNIT_NONE;
  s->opt[OPT_GAMMA_VECTOR_G].size = 256 * sizeof (SANE_Word);
  s->opt[OPT_GAMMA_VECTOR_G].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_GAMMA_VECTOR_G].constraint.range = &u8_range;
  s->val[OPT_GAMMA_VECTOR_G].wa = &s->gamma_table[2][0];

  /* blue gamma vector */
  s->opt[OPT_GAMMA_VECTOR_B].name = SANE_NAME_GAMMA_VECTOR_B;
  s->opt[OPT_GAMMA_VECTOR_B].title = SANE_TITLE_GAMMA_VECTOR_B;
  s->opt[OPT_GAMMA_VECTOR_B].desc = SANE_DESC_GAMMA_VECTOR_B;
  s->opt[OPT_GAMMA_VECTOR_B].type = SANE_TYPE_INT;
  s->opt[OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_GAMMA_VECTOR_B].unit = SANE_UNIT_NONE;
  s->opt[OPT_GAMMA_VECTOR_B].size = 256 * sizeof (SANE_Word);
  s->opt[OPT_GAMMA_VECTOR_B].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_GAMMA_VECTOR_B].constraint.range = &u8_range;
  s->val[OPT_GAMMA_VECTOR_B].wa = &s->gamma_table[3][0];

  /* halftone dimension */
  s->opt[OPT_HALFTONE_DIMENSION].name = SANE_NAME_HALFTONE_DIMENSION;
  s->opt[OPT_HALFTONE_DIMENSION].title = SANE_TITLE_HALFTONE_DIMENSION;
  s->opt[OPT_HALFTONE_DIMENSION].desc = SANE_DESC_HALFTONE_DIMENSION;
  s->opt[OPT_HALFTONE_DIMENSION].type = SANE_TYPE_INT;
  s->opt[OPT_HALFTONE_DIMENSION].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_HALFTONE_DIMENSION].unit = SANE_UNIT_PIXEL;
  s->opt[OPT_HALFTONE_DIMENSION].constraint_type = SANE_CONSTRAINT_WORD_LIST;
  s->opt[OPT_HALFTONE_DIMENSION].constraint.word_list = pattern_dimension_list;
  s->val[OPT_HALFTONE_DIMENSION].w = 0;

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

  if (s->hw->flags & MUSTEK_FLAG_SE)
    {
      /* SE models don't support speed, source, backtrack, grain size */
      s->opt[OPT_SPEED].cap |= SANE_CAP_INACTIVE;
      s->opt[OPT_SOURCE].cap |= SANE_CAP_INACTIVE;
      s->opt[OPT_BACKTRACK].cap |= SANE_CAP_INACTIVE;
      s->opt[OPT_GRAIN_SIZE].cap |= SANE_CAP_INACTIVE;
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
  int y, bit, num_lines;

  DBG(4, "mustek.output_data: data=%p, lpb=%d, bpl=%d, extra=%p\n",
      data, lines_per_buffer, bpl, extra);

  /* convert to pixel-interleaved format: */
  if ((s->mode & MUSTEK_MODE_COLOR)
      && (s->hw->flags & MUSTEK_FLAG_SINGLE_PASS))
    {
      num_lines = lines_per_buffer;

      /* need to correct for distance between r/g/b sensors: */
      if (s->hw->flags & MUSTEK_FLAG_SE)
	num_lines = fix_line_distance_se (s, num_lines, bpl, data, extra);
      else if (s->hw->flags & MUSTEK_FLAG_LD_MFS) 
	fix_line_distance_mfs (s, num_lines, bpl, data, extra);
      else if (s->ld.max_value)
	{
	  if (s->hw->flags & MUSTEK_FLAG_PP)
	    num_lines = fix_line_distance_pp (s, num_lines, bpl, data, extra);
	  else
	    fix_line_distance_normal (s, num_lines, bpl, data, extra);
	}
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

      if (s->mode & MUSTEK_MODE_MULTIBIT)
	{
	  /* each r/g/b sample is 8 bits in line-interleaved format */
	  fwrite (extra, num_lines, s->params.bytes_per_line, fp);
	}
      else
	{
	  /* each r/g/b/ sample is 1 bit in line-interleaved format */
	  ptr = extra;
	  ptr_end = ptr + num_lines * bpl;
	  while (ptr != ptr_end)
	    {
	      for (bit = 7; bit >= 0; --bit)
		fputc ( (*ptr & (1 << bit)) ? 0xff : 0x00, fp);
	      ++ptr;
	    }
	}
    }
  else
    {
      if (! (s->mode & MUSTEK_MODE_MULTIBIT))
	{
	  /* in singlebit mode, the scanner returns 1 for black. ;-( */
	  ptr = data;
	  ptr_end = ptr + lines_per_buffer * bpl;

	  while (ptr != ptr_end)
	    *ptr++ = ~*ptr;
	}    
      fwrite (data, lines_per_buffer, bpl, fp);
    }
}

static RETSIGTYPE
sigterm_handler (int signal)
{
  sanei_scsi_req_flush_all ();		/* flush SCSI queue */
  _exit (SANE_STATUS_GOOD);
}

static int
reader_process (Mustek_Scanner *s, int fd)
{
  int index, last, lines_per_buffer, bpl, buffers, qu = 0, rd = 0;
  SANE_Byte *extra = 0, *ptr;
  sigset_t sigterm_set;
  struct SIGACTION act;
  SANE_Status status;
  FILE *fp;
  struct
    {
      void *id;		/* scsi queue id */
      SANE_Byte *data;	/* data buffer */
      int lines;	/* # lines in buffer */
      size_t num_read;	/* # of bytes read (return value) */
      int bank;		/* needed by SE models */
    } bstat[BUFFERS];

  sigemptyset (&sigterm_set);
  sigaddset (&sigterm_set, SIGTERM);

  fp = fdopen (fd, "w");
  if (!fp)
    return SANE_STATUS_IO_ERROR;

  bpl = s->hw->bpl;

  if ((s->hw->flags & MUSTEK_FLAG_SINGLE_PASS)
      && ((s->mode & (MUSTEK_MODE_COLOR | MUSTEK_MODE_MULTIBIT))
	  == MUSTEK_MODE_COLOR))
    /* In single-bit, single-pass color mode we expand every bit of
       information into a byte (0x00 or 0xff).  */
    bpl /= 8;

  /* Request size must be limited to 64 kByte for ScanExpress. */ 
  if (s->hw->flags & MUSTEK_FLAG_SE) 
    {
      buffers = BUFFERS;
      lines_per_buffer = MIN( sanei_scsi_max_request_size, 128 * 1024 / 2) / bpl;
    }
  else
    {
      buffers = 1;
      lines_per_buffer = sanei_scsi_max_request_size / bpl;
    }
    
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
      DBG(1, "bpl (%d) > sanei_scsi_max_request_size (%d)\n",
	  bpl, sanei_scsi_max_request_size);
      return SANE_STATUS_NO_MEM;		/* resolution is too high */
    }
  DBG(3, "lines_per_buffer=%d, bytes_per_line=%d\n", lines_per_buffer, bpl);

  bstat[0].data = malloc (buffers * lines_per_buffer * (long) bpl);

  if (!bstat[0].data)
    {
      DBG(1, "reader_process: failed to malloc %ld bytes\n",
	  buffers * lines_per_buffer * (long) bpl);
      return SANE_STATUS_NO_MEM;
    }

  for (index = 1; index < buffers; ++index)
    bstat[index].data = bstat[index - 1].data + lines_per_buffer * (long) bpl;

  /* Touch all pages of the buffer to fool the memory management. */ 
  ptr = bstat[0].data + buffers * lines_per_buffer * (long) bpl - 256;
  while (ptr >= bstat[0].data)
    {
      *ptr = 0x00;
      ptr -= 256;
    }   
  
  if (s->hw->flags & MUSTEK_FLAG_SINGLE_PASS)
    {
      /* get temporary buffer for line-distance correction and/or bit
	 expansion. The 600 II N needs more space because the data must
	 be read in as single big block (cut up into pieces of
	 lines_per_buffer). This requires that the line distance correction
	 continues on every call exactly where it stopped if the image
	 shall be reconstructed without any stripes. The older SCSI scanners
	 allow the line distance correction to initialize for every
	 lines_per_buffer lines that are read on request. 
	 The ScanExpress is similar to the 600 II N in this respect. */
      if (s->hw->flags & (MUSTEK_FLAG_PP || MUSTEK_FLAG_SE))   
	extra = malloc ((lines_per_buffer + MAX_LINE_DIST) * (long) bpl);
      else
	extra = malloc (lines_per_buffer * (long) bpl);

      if (!extra)
	{	
	  DBG(1, "reader_process: failed to malloc extra buffer\n");
	  return SANE_STATUS_NO_MEM;
	}
    }
    
  memset (&act, 0, sizeof (act));
  act.sa_handler = sigterm_handler;
  sigaction (SIGTERM, &act, 0);

  if (s->hw->flags & MUSTEK_FLAG_PP)
    {
      /* reacquire port access rights (lost because of fork()): */
      sanei_ab306_get_io_privilege (s->fd);
      /* reset counter of line number for line-dictance correction */
      s->ld.ld_line = 0;
    }

  status = dev_read_start (s);
  if (status != SANE_STATUS_GOOD)
    return status;

  while (s->line < s->hw->lines)
    {
      /* Enqueue read requests as long as there is more to scan and as
	 long as the queue is not full: */

      last = (s->line >= s->hw->lines || qu >= buffers);
      
      while (! last && (qu - rd) < REQUESTS)
	{
	  if (s->line + lines_per_buffer >= s->hw->lines)
	    {
	      /* do the last few lines: */
	      bstat[qu].lines = s->hw->lines - s->line;
	      bstat[qu].bank = 0x01;
	    }
	  else 
	    {
	      bstat[qu].lines = lines_per_buffer;
	      bstat[qu].bank = (qu < (buffers - 1)) ? 0x00 : 0x01;    
	    }
	  s->line += bstat[qu].lines;

	  sigprocmask (SIG_BLOCK, &sigterm_set, 0); 
	  status = dev_read_req_enter (s, bstat[qu].data, bstat[qu].lines, bpl,
				       &bstat[qu].num_read, &bstat[qu].id,
				       bstat[qu].bank);
	  sigprocmask (SIG_UNBLOCK, &sigterm_set, 0); 
	  ++qu;

	  if (status != SANE_STATUS_GOOD)
	    {
	      DBG(1,
		  "reader_process:  failed to enqueue read req, status: %s\n",
		  sane_strstatus (status));
	      return status;
	    }
	  else
	    {
	      DBG(4, "reader_process: line=%d (num_lines=%d), num_reqs=%d\n",
		  s->line, s->params.lines, (qu - rd));
	    }

	  last = (s->line >= s->hw->lines || qu >= buffers);
	}

      /* wait for request(s) to complete */

      while ((qu - rd) >= REQUESTS || (last && (rd < qu)))
	{
	  status = dev_req_wait (bstat[rd].id); 

	  if (status != SANE_STATUS_GOOD)
	    {
	      DBG(1, "reader_process: failed to read data, status: %s\n",
		  sane_strstatus (status));
	      return status;
	    }
	  else
	    DBG(4, "reader_process: %lu bytes read.\n",
		(unsigned long) bstat[rd].num_read);
	    
	  ++rd;
	}

      if (last)	
	{
	  for (rd = 0; rd < qu; ++rd)	
	    output_data (s, fp, bstat[rd].data, bstat[rd].lines, bpl, extra);
	  qu = 0;
	  rd = 0;
	}  

      /* This is said to fix the scanner hangs that reportedly show on
	 some MFS-12000SP scanners.  */
      if (s->mode == 0 && (s->hw->flags & MUSTEK_FLAG_LINEART_FIX)) 
	usleep (200000);
    }

  fclose (fp);
  free (bstat[0].data);
  if (s->ld.buf[0])
    free (s->ld.buf[0]);
  if (extra) 
    free (extra);
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

SANE_Status
sane_init (SANE_Int *version_code, SANE_Auth_Callback authorize)
{
  char dev_name[PATH_MAX], *cp;
  size_t len;
  FILE *fp;
  int i;

  DBG_INIT();

  if (version_code)
    *version_code = SANE_VERSION_CODE (V_MAJOR, V_MINOR, 0);

  fp = sanei_config_open (MUSTEK_CONFIG_FILE);
  if (!fp)
    {
      /* default to /dev/scanner instead of insisting on config file */
      attach ("/dev/scanner", 0, SANE_FALSE);
      return SANE_STATUS_GOOD;
    }

  while (fgets (dev_name, sizeof (dev_name), fp))
    {
      cp = (char *) sanei_config_skip_whitespace (dev_name);
      if (!*cp || *cp == '#')   /* ignore line comments & empty lines */
	continue;
                    
      len = strlen (cp);
      if (cp[len - 1] == '\n')
	cp[--len] = '\0';

      if (!len)
	continue;			/* ignore empty lines */

      if (strncmp (cp, "option", 6) == 0 && isspace (cp[6]))
	{
	  cp += 7;
	  cp = (char *) sanei_config_skip_whitespace (cp);

	  if (strncmp (cp, "strip-height", 12) == 0 && isspace (cp[12]))
	    {
	      char * end;

	      errno = 0;
	      cp += 13;
	      strip_height = strtod (cp, &end);
	      if (end == cp || errno)
		{
		  DBG(1, "%s: strip-height `%s' is invalid\n",
		      MUSTEK_CONFIG_FILE, cp);
		  strip_height = 1.0;		/* safe fallback */
		}
	      else
		{
		  if (strip_height < 0.0)
		    strip_height = 0.0;
		  DBG(2, "sane_init: strip-height set to %g inches\n",
		      strip_height);
		}
	    }
	  else if (strncmp (cp, "linedistance-fix", 16) == 0 &&
		   (isspace (cp[16]) || !cp[16]))
	    for (i = 0; i < new_dev_len; ++i)
	      {
		new_dev[i]->flags |= MUSTEK_FLAG_LD_FIX;
		DBG(2, "sane_init: enabling linedistance-fix for %s\n",
		    new_dev[i]->sane.name);
		break;
	      }
	  else if (strncmp (cp, "lineart-fix", 11) == 0 &&
		   (isspace (cp[11]) || !cp[11]))
	    for (i = 0; i < new_dev_len; ++i)
	      {
		new_dev[i]->flags |= MUSTEK_FLAG_LINEART_FIX;
		DBG(2, "sane_init: enabling lineart-fix for %s\n",
		    new_dev[i]->sane.name);
		break;
	      }
	  else
	    DBG(1, "%s: ignoring unknown option `%s'\n",
		MUSTEK_CONFIG_FILE, cp);
	  continue;
	}
      else
	{ 
	  new_dev_len = 0;
	  sanei_config_attach_matching_devices (cp, attach_one_device);
	}
    }     
  if (new_dev_alloced > 0)
    {     
      new_dev_len = new_dev_alloced = 0;
      free (new_dev);
    }
  fclose (fp);
  return SANE_STATUS_GOOD;
}

void
sane_exit (void)
{
  Mustek_Device *dev, *next;

  for (dev = first_dev; dev; dev = next)
    {
      next = dev->next;
      free ((void *) dev->sane.name);
      free ((void *) dev->sane.model);
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
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_open (SANE_String_Const devicename, SANE_Handle *handle)
{
  Mustek_Device *dev;
  SANE_Status status;
  Mustek_Scanner *s;
  int i, j;

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
  for (i = 0; i < 4; ++i)
    for (j = 0; j < 256; ++j)
      s->gamma_table[i][j] = j;

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
      DBG(1, "close: invalid handle %p\n", handle);
      return;		/* oops, not a handle we know about */
    }

  if (s->scanning)
    do_stop (handle);

  if (s->ld.buf[0])
    free (s->ld.buf[0]);

  if (prev)
    prev->next = s->next;
  else
    first_handle = s;

  free (handle);
}

const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
  Mustek_Scanner *s = handle;

  if ((unsigned) option >= NUM_OPTIONS)
    return 0;
  return s->opt + option;
}

SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option,
		     SANE_Action action, void *val, SANE_Int *info)
{
  Mustek_Scanner *s = handle;
  SANE_Status status;
  SANE_Word w, cap;

  if (info)
    *info = 0;

  if (s->scanning)
    return SANE_STATUS_DEVICE_BUSY;

  if (option >= NUM_OPTIONS)
    return SANE_STATUS_INVAL;

  cap = s->opt[option].cap;

  if (!SANE_OPTION_IS_ACTIVE (cap))
    return SANE_STATUS_INVAL;

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
	case OPT_GRAIN_SIZE:
	case OPT_BRIGHTNESS:
	case OPT_CONTRAST:
	case OPT_CUSTOM_GAMMA:
	case OPT_HALFTONE_DIMENSION:
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
	  strcpy (val, s->val[option].s);
	  return SANE_STATUS_GOOD;
	}
    }
  else if (action == SANE_ACTION_SET_VALUE)
    {
      if (!SANE_OPTION_IS_SETTABLE (cap))
	return SANE_STATUS_INVAL;

      status = constrain_value (s, option, val, info);
      if (status != SANE_STATUS_GOOD)
	return status;

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
	case OPT_GRAIN_SIZE:
	case OPT_BRIGHTNESS:
	case OPT_CONTRAST:
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

	      if (strcmp (mode, "Gray") == 0)
		s->opt[OPT_GAMMA_VECTOR].cap &= ~SANE_CAP_INACTIVE;
	      else if (strcmp (mode, "Color") == 0)
		{
		  s->opt[OPT_GAMMA_VECTOR].cap   &= ~SANE_CAP_INACTIVE;
		  s->opt[OPT_GAMMA_VECTOR_R].cap &= ~SANE_CAP_INACTIVE;
		  s->opt[OPT_GAMMA_VECTOR_G].cap &= ~SANE_CAP_INACTIVE;
		  s->opt[OPT_GAMMA_VECTOR_B].cap &= ~SANE_CAP_INACTIVE;
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
	    *info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;

	    s->val[option].s = strdup (val);

	    s->opt[OPT_BRIGHTNESS].cap |= SANE_CAP_INACTIVE;
	    s->opt[OPT_CONTRAST].cap |= SANE_CAP_INACTIVE;
	    s->opt[OPT_CUSTOM_GAMMA].cap |= SANE_CAP_INACTIVE;
	    s->opt[OPT_GAMMA_VECTOR].cap |= SANE_CAP_INACTIVE;
	    s->opt[OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
	    s->opt[OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
	    s->opt[OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;
	    s->opt[OPT_HALFTONE_DIMENSION].cap |= SANE_CAP_INACTIVE;
	    s->opt[OPT_HALFTONE_PATTERN].cap |= SANE_CAP_INACTIVE;

	    halftoning = (strcmp (val, "Halftone") == 0
			  || strcmp (val, "Color Halftone") == 0);
	    binary = (halftoning
		      || strcmp (val, "Lineart") == 0
		      || strcmp (val, "Color Lineart") == 0);

	    if (!(s->hw->flags & MUSTEK_FLAG_SINGLE_PASS) || binary)
	      {
		/* enable brightness/contrast for 3-pass scanners or
		   for 1-pass scanners  when in a binary mode */
		s->opt[OPT_BRIGHTNESS].cap &= ~SANE_CAP_INACTIVE;
		/* The SE models support only threshold in lineart */
		if (!(s->hw->flags & MUSTEK_FLAG_SE))
		  s->opt[OPT_CONTRAST].cap &= ~SANE_CAP_INACTIVE;

		if (halftoning)
		  {
		    s->opt[OPT_HALFTONE_DIMENSION].cap &= ~SANE_CAP_INACTIVE;
		    if (s->val[OPT_HALFTONE_DIMENSION].w)
		      s->opt[OPT_HALFTONE_PATTERN].cap &= ~SANE_CAP_INACTIVE;
		  }
	      }

	    if (!binary)
	      s->opt[OPT_CUSTOM_GAMMA].cap &= ~SANE_CAP_INACTIVE;

	    if (s->val[OPT_CUSTOM_GAMMA].w)
	      {
		if (strcmp (val, "Gray") == 0)
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
	    unsigned dim = *(SANE_Word *) val;

	    if (s->val[option].w == dim)
	      return SANE_STATUS_GOOD;		/* no change */

	    if (info)
	      *info |= SANE_INFO_RELOAD_OPTIONS;

	    s->val[option].w = dim;
	    s->opt[OPT_HALFTONE_PATTERN].cap |= SANE_CAP_INACTIVE;
	    if (dim > 0)
	      {
		s->opt[OPT_HALFTONE_PATTERN].cap &= ~SANE_CAP_INACTIVE;
		s->opt[OPT_HALFTONE_PATTERN].size = dim * sizeof (SANE_Word);
	      }
	    return SANE_STATUS_GOOD;
	  }

	case OPT_SOURCE:
	  if (info)
	    *info |= SANE_INFO_RELOAD_OPTIONS;
	  if (s->val[option].s)
	    free (s->val[option].s);
	  s->val[option].s = strdup (val);

	  if (strcmp (val, source_list[TA]) == 0)
	    {
	      s->opt[OPT_BR_X].constraint.range = &s->hw->x_trans_range;
	      s->opt[OPT_BR_Y].constraint.range = &s->hw->y_trans_range;
	    }
	  else
	    {
	      s->opt[OPT_BR_X].constraint.range = &s->hw->x_range;
	      s->opt[OPT_BR_Y].constraint.range = &s->hw->y_range;
	    }
	  return SANE_STATUS_GOOD;
	}
    }
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
      else
	{
	  /* it's one of the color modes... */

	  if (s->hw->flags & MUSTEK_FLAG_SINGLE_PASS)
	    {
	      /* all color modes are treated the same since lineart and
		 halftoning with 1 bit color pixels still results in 3
		 bytes/pixel.  */
	      s->params.format = SANE_FRAME_RGB;
	      s->params.bytes_per_line = 3 * s->params.pixels_per_line;
	      s->params.depth = 8;
	    }
	  else
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
	}
    }
  else if ((s->mode & MUSTEK_MODE_COLOR)
	   && !(s->hw->flags & MUSTEK_FLAG_SINGLE_PASS))
    s->params.format = SANE_FRAME_RED + s->pass;
  s->params.last_frame = (s->params.format != SANE_FRAME_RED
			  && s->params.format != SANE_FRAME_GREEN);
  if (params)
    *params = s->params;
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_start (SANE_Handle handle)
{
  Mustek_Scanner *s = handle;
  SANE_Status status;
  int fds[2];

  /* First make sure we have a current parameter set.  Some of the
     parameters will be overwritten below, but that's OK.  */
  status = sane_get_parameters (s, 0);
  if (status != SANE_STATUS_GOOD)
      return status;

  if (s->fd < 0)
    {
      /* this is the first (and maybe only) pass... */
      const char *mode;

      /* translate options into s->mode for convenient access: */
      mode = s->val[OPT_MODE].s;
      if (strcmp (mode, "Lineart") == 0)
	s->mode = 0;
      else if (strcmp (mode, "Halftone") == 0)
	s->mode = MUSTEK_MODE_HALFTONE;
      else if (strcmp (mode, "Gray") == 0)
	s->mode = MUSTEK_MODE_MULTIBIT;
      else if (strcmp (mode, "Color Lineart") == 0)
	s->mode = MUSTEK_MODE_COLOR;
      else if (strcmp (mode, "Color Halftone") == 0)
	s->mode = MUSTEK_MODE_COLOR | MUSTEK_MODE_HALFTONE;
      else if (strcmp (mode, "Color") == 0)
	s->mode = MUSTEK_MODE_COLOR | MUSTEK_MODE_MULTIBIT;

      s->one_pass_color_scan = 0;
      if (s->mode & MUSTEK_MODE_COLOR)
	{
	  if (s->val[OPT_PREVIEW].w && s->val[OPT_GRAY_PREVIEW].w)
	    {
	      /* Force gray-scale mode when previewing.  */
	      s->mode &= ~MUSTEK_MODE_COLOR;
	      s->params.format = SANE_FRAME_GRAY;
	      s->params.bytes_per_line = s->params.pixels_per_line;
	      s->params.last_frame = SANE_TRUE;
	      if (!(s->mode & MUSTEK_MODE_MULTIBIT))
		{
		  s->params.bytes_per_line = (s->params.pixels_per_line + 7)/8;
		  s->params.depth = 1;
		}
	    }
	  else if (s->hw->flags & MUSTEK_FLAG_SINGLE_PASS)
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
      DBG(1, "open: wait_ready() failed: %s\n", sane_strstatus (status));
      goto stop_scanner_and_return;
    }

  status = do_inquiry (s);
  if (status != SANE_STATUS_GOOD)
    {
      DBG(1, "open: inquiry command failed: %s\n", sane_strstatus (status));
      goto stop_scanner_and_return;
    }
    
  if (s->hw->flags & MUSTEK_FLAG_SE)
    { 
      status = do_set_window (s, 0);	
      if (status != SANE_STATUS_GOOD)
	{
	  DBG(1, "open: set window command failed: %s\n",
	      sane_strstatus (status));
	  goto stop_scanner_and_return;
	}

      s->scanning = SANE_TRUE;
      status = do_get_window (s, &s->params.bytes_per_line,
			      &s->params.lines, &s->params.pixels_per_line);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG(1, "open: get window command failed: %s\n",
	      sane_strstatus (status));
	  goto stop_scanner_and_return;
	}

      status = start_scan (s);
      if (status != SANE_STATUS_GOOD) 
	goto stop_scanner_and_return;

      status = send_gamma_se (s);
      if (status != SANE_STATUS_GOOD) 
	goto stop_scanner_and_return;
/*
      status = calibration (s);
      if (status != SANE_STATUS_GOOD) 
	goto stop_scanner_and_return; */
    }
  else 
    {
      status = scan_area_and_windows (s);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG(1, "open: set scan area command failed: %s\n",
	      sane_strstatus (status));
	  goto stop_scanner_and_return;
	}
                                  
      status = backtrack_and_adf (s);
      if (status != SANE_STATUS_GOOD)
	goto stop_scanner_and_return;

      if (s->one_pass_color_scan)
	{
	  status = mode_select (s, MUSTEK_CODE_RED);
	  if (status != SANE_STATUS_GOOD)
	    goto stop_scanner_and_return;

	  status = mode_select (s, MUSTEK_CODE_GREEN);
	  if (status != SANE_STATUS_GOOD)
	    goto stop_scanner_and_return;

	  status = mode_select (s, MUSTEK_CODE_BLUE);
	}
      else
	status = mode_select (s, MUSTEK_CODE_GRAY);

      s->scanning = SANE_TRUE;

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
      if (s->hw->flags & MUSTEK_FLAG_SINGLE_PASS)
	{
	  status = line_distance (s);
	  if (status != SANE_STATUS_GOOD)
	    goto stop_scanner_and_return;
	}

      status = backtrack_and_adf (s);
      if (status != SANE_STATUS_GOOD)
	goto stop_scanner_and_return;

      status = get_image_status (s, &s->params.bytes_per_line, 
				 &s->params.lines);
      if (status != SANE_STATUS_GOOD)
      	goto stop_scanner_and_return;
    }

  if ((s->mode & (MUSTEK_MODE_COLOR | MUSTEK_MODE_MULTIBIT))
      == MUSTEK_MODE_COLOR
      && (s->hw->flags & MUSTEK_FLAG_SINGLE_PASS))
    /* In single-bit, single-pass color mode we expand every bit of
       information into a byte (0x00 or 0xff).  */
    s->params.bytes_per_line *= 8;

  s->params.pixels_per_line = s->params.bytes_per_line;
  if (s->one_pass_color_scan)
    s->params.pixels_per_line /= 3;
  else if (!(s->mode & MUSTEK_MODE_MULTIBIT))
    s->params.pixels_per_line *= 8;

  s->line = 0;

  if (pipe (fds) < 0)
    return SANE_STATUS_IO_ERROR;

  s->reader_pid = fork ();
  if (s->reader_pid == 0)
    {
      sigset_t ignore_set;
      struct SIGACTION act;

      close (fds[0]);

      sigfillset (&ignore_set);
      sigdelset (&ignore_set, SIGTERM);
      sigprocmask (SIG_SETMASK, &ignore_set, 0);

      memset (&act, 0, sizeof (act));
      sigaction (SIGTERM, &act, 0);

      /* don't use exit() since that would run the atexit() handlers... */
      _exit (reader_process (s, fds[1]));
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
  ssize_t nread;

  *len = 0;

  nread = read (s->pipe, buf, max_len);
  DBG(3, "read %ld bytes\n", (long) nread);

  if (!s->scanning)
    return do_stop (s);

  if (nread < 0)
    {
      if (errno == EAGAIN)
	return SANE_STATUS_GOOD;
      else
	{
	  do_stop (s);
	  return SANE_STATUS_IO_ERROR;
	}
    }

  *len = nread;

  if (nread == 0)
    {
      if ((s->hw->flags & MUSTEK_FLAG_SINGLE_PASS)
	  || !(s->mode & MUSTEK_MODE_COLOR) || ++s->pass >= 3)
	{
	  status = do_stop (s);
	  if (status != SANE_STATUS_CANCELLED && status != SANE_STATUS_GOOD)
	    return status;			/* something went wrong */
	}
      return do_eof (s);
    }
  return SANE_STATUS_GOOD;
}

void
sane_cancel (SANE_Handle handle)
{
  Mustek_Scanner *s = handle;

  if (s->reader_pid > 0)
    {
      kill (s->reader_pid, SIGTERM);
      waitpid (s->reader_pid, 0, 0);
      s->reader_pid = 0;
    }
  s->scanning = SANE_FALSE;
}

SANE_Status
sane_set_io_mode (SANE_Handle handle, SANE_Bool non_blocking)
{
  Mustek_Scanner *s = handle;

  if (!s->scanning)
    return SANE_STATUS_INVAL;

  if (fcntl (s->pipe, F_SETFL, non_blocking ? O_NONBLOCK : 0) < 0)
    return SANE_STATUS_IO_ERROR;

  return SANE_STATUS_GOOD;
}

SANE_Status
sane_get_select_fd (SANE_Handle handle, SANE_Int *fd)
{
  Mustek_Scanner *s = handle;

  if (!s->scanning)
    return SANE_STATUS_INVAL;

  *fd = s->pipe;
  return SANE_STATUS_GOOD;
}
