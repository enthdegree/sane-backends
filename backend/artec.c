/* sane - Scanner Access Now Easy.
   Copyright (C) 1997 David Mosberger-Tang
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

   This file implements a SANE backend for the Ultima/Artec AT3 and
   A6000C scanners.

   Copyright (C) 1998, Chris Pinkham
   Released under the terms of the GPL.
   *NO WARRANTY*

   *********************************************************************
   For feedback/information:

   cpinkham@sh001.infi.net
   http://www4.infi.net/~cpinkham/sane/sane-artec-doc.html
   *********************************************************************
 */

#include <sane/config.h>

#include <limits.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include <sane/sane.h>
#include <sane/saneopts.h>
#include <sane/sanei_scsi.h>
#include <artec.h>

#include <sane/sanei_backend.h>

#define BACKEND_NAME    artec

#define ARTEC_MAJOR     0
#define ARTEC_MINOR     3
#define ARTEC_LAST_MOD  "10/18/1998 22:45"

/*
 * Uncomment the following line to compile the library in pixel mode.  All
 * measurements (starting x, starting y, height, width) can then be given
 * in pixels instead of millimeters.
 */
/*
#define PREFER_PIXEL_MODE
 */

#ifndef PREFER_PIXEL_MODE
#define MM_PER_INCH	25.4
#endif

#ifndef PATH_MAX
#define PATH_MAX	1024
#endif

#include <sane/sanei_config.h>
#define ARTEC_CONFIG_FILE "artec.conf"
#define ARTEC_MAX_READ_SIZE 32768


static int num_devices;
static ARTEC_Device *first_dev;
static ARTEC_Scanner *first_handle;

static const SANE_String_Const mode_list[] =
{
  "Lineart", "Halftone", "Gray", "Color",
  0
};

static const SANE_String_Const filter_type_list[] =
{
  "Mono", "Red", "Green", "Blue",
  0
};

static const SANE_String_Const halftone_pattern_list[] =
{
  "User defined (unsupported)", "4x4 Spiral", "4x4 Bayer", "8x8 Spiral",
  "8x8 Bayer",
  0
};

#define INQ_LEN	0x60
static const u_int8_t inquiry[] =
{
  0x12, 0x00, 0x00, 0x00, INQ_LEN, 0x00
};

static const u_int8_t test_unit_ready[] =
{
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static struct
  {
    SANE_String model;
    SANE_String type;
    SANE_Word x_min;
    SANE_Word x_max;
    double width;
    SANE_Word y_min;
    SANE_Word y_max;
    double height;
    SANE_Word x_dpi_min;
    SANE_Word x_dpi_max;
    SANE_Word y_dpi_min;
    SANE_Word y_dpi_max;
    SANE_Word contrast_min;
    SANE_Word contrast_max;
    SANE_Word threshold_min;
    SANE_Word threshold_max;
    SANE_Bool req_shading_calibrate;
    SANE_Bool req_rgb_line_offset;
    SANE_Bool req_rgb_char_shift;
    SANE_Bool opt_brightness;
    SANE_Word brightness_min;
    SANE_Word brightness_max;
    SANE_Word setwindow_cmd_size;
    SANE_Bool calibrate_method;
    SANE_String horz_resolution_str;
    SANE_String vert_resolution_str;
  }
cap_data[] =
{
  {
    "AT3", "flatbed", 0, 2550, 8.5, 0, 3300, 11, 50, 300,
      50, 600, 0, 255, 0, 255, SANE_TRUE, SANE_TRUE, SANE_TRUE, SANE_FALSE,
      0, 0, 55, ARTEC_CALIB_RGB, "50,100,200,300", "50,100,200,300,600"
  }
  ,
  {
    "A6000C", "flatbed", 0, 2550, 8.5, 0, 3300, 14, 50, 300,
      50, 600, 0, 255, 0, 255, SANE_TRUE, SANE_TRUE, SANE_TRUE, SANE_FALSE,
      0, 0, 55, ARTEC_CALIB_RGB, "50,100,200,300", "50,100,200,300,600"
  }
  ,
  {
    "AT6", "flatbed", 0, 2550, 8.5, 0, 3300, 11, 50, 300,
      50, 600, 0, 255, 0, 255, SANE_TRUE, SANE_TRUE, SANE_TRUE, SANE_FALSE,
      0, 0, 55, ARTEC_CALIB_RGB, "50,100,200,300", "50,100,200,300,600"
  }
  ,
  {
    "AT12", "flatbed", 0, 2550, 8.5, 0, 3300, 11, 50, 300,
    /* no brightness, calibration works slower so disabled */
      50, 600, 0, 255, 0, 255, SANE_FALSE, SANE_FALSE, SANE_FALSE, SANE_FALSE,
      0, 255, 67, ARTEC_CALIB_DARK_WHITE,
      "25,50,100,200,300,400,500,600",
      "25,50,100,200,300,400,500,600,700,800,900,1000,1100,1200"
  }
  ,
};

static u_char cap_buf[256];	/* DB buffer for cap data (AT12 needs 59 bytes) */

static SANE_Status
artec_str_list_to_word_list (SANE_Word ** word_list_ptr, SANE_String str)
{
  SANE_Word *word_list;
  char *start;
  char *end;
  char temp_str[1024];
  int comma_count = 1;

  if ((str == NULL) ||
      (strlen (str) == 0))
    {
      /* alloc space for word which stores length (0 in this case) */
      word_list = (SANE_Word *) malloc (sizeof (SANE_Word));
      if (word_list == NULL)
	return (SANE_STATUS_NO_MEM);

      word_list[0] = 0;
      *word_list_ptr = word_list;
      return (SANE_STATUS_GOOD);
    }

  /* make temp copy of input string (only hold 1024 for now) */
  strncpy (temp_str, str, 1023);
  temp_str[1023] = '\0';

  end = strchr (temp_str, ',');
  while (end != NULL)
    {
      comma_count++;
      start = end + 1;
      end = strchr (start, ',');
    }

  word_list = (SANE_Word *) calloc (comma_count + 1,
				    sizeof (SANE_Word));
  if (word_list == NULL)
    return (SANE_STATUS_NO_MEM);

  word_list[0] = comma_count;

  comma_count = 1;
  start = temp_str;
  end = strchr (temp_str, ',');
  while (end != NULL)
    {
      *end = '\0';
      word_list[comma_count] = atol (start);

      start = end + 1;
      comma_count++;
      end = strchr (start, ',');
    }
  word_list[comma_count] = atol (start);

  *word_list_ptr = word_list;
  return (SANE_STATUS_GOOD);
}

static size_t
artec_get_str_index (const SANE_String_Const strings[], char *str)
{
  size_t index;

  index = 0;
  while ((strings[index]) && strcmp (strings[index], str))
    {
      index++;
    }

  if (!strings[index])
    {
      index = 0;
    }

  return (index);
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

  return (max_size);
}

/* DB added a sense handler */

static SANE_Status
sense_handler (int fd, u_char * sense, void *arg)
{
  int s;

  s = 0;
  if (sense[18] & 0x01)
    {
      DBG (5, "sense:  ADF PAPER JAM\n");
      s++;
    }
  if (sense[18] & 0x02)
    {
      DBG (5, "sense:  ADF NO DOCUMENT IN BIN\n");
      s++;
    }
  if (sense[18] & 0x04)
    {
      DBG (5, "sense:  ADF SWITCH COVER OPEN\n");
      s++;
    }
  /* DB : next is, i think no failure, so no incrementing s */
  if (sense[18] & 0x08)
    {
      DBG (5, "sense:  ADF SET CORRECTLY ON TARGET\n");
    }
  /* The following only for AT12, its reserved (zero?) on other models,  */
  if (sense[18] & 0x10)
    {
      DBG (5, "sense:  ADF LENGTH TOO SHORT\n");
      s++;
    }
  if (sense[18] & 0x20)
    {
      DBG (5, "sense:  LAMP FAIL : NOT WARM \n");
      s++;
    }
  if (sense[18] & 0x40)
    {
      DBG (5, "sense:  NOT READY STATE\n");
      s++;
    }
  /* --- */
  /* The following not for AT12, its reserved so should not give problems */
  if (sense[19] & 0x01)
    {
      DBG (5, "sense:  ROM ERROR\n");
      s++;
    }
  if (sense[19] & 0x02)
    {
      DBG (5, "sense:  CPU RAM ERROR\n");
      s++;
    }
  if (sense[19] & 0x04)
    {
      DBG (5, "sense:  SHAD.CORR.RAM W/R ERROR\n");
      s++;
    }
  if (sense[19] & 0x08)
    {
      DBG (5, "sense:  LINE RAM W/R ERROR\n");
      s++;
    }
  if (sense[19] & 0x10)
    {
      DBG (5, "sense:  CCD CONTR.CIRCUIT ERROR\n");
      s++;
    }
  if (sense[19] & 0x20)
    {
      DBG (5, "sense:  MOTOR END SWITCH ERROR\n");
      s++;
    }
  if (sense[19] & 0x40)
    {
      DBG (5, "sense:  LAMP ERROR\n");
      s++;
    }
  if (sense[19] & 0x80)
    {
      DBG (5, "sense:  OPTICAL SHADING ERROR\n");
      s++;
    }
  /* --- */
  /* The following only for AT12, its reserved so should not give problems */
  /* These are the self test results for tests 0-15 */
  if (sense[22] & 0x01)
    {
      DBG (5, "sense:  8031 INT MEM R/W ERROR\n");
      s++;
    }
  if (sense[22] & 0x02)
    {
      DBG (5, "sense:  EEPROM R/W ERROR\n");
      s++;
    }
  if (sense[22] & 0x04)
    {
      DBG (5, "sense:  ASIC TEST ERROR\n");
      s++;
    }
  if (sense[22] & 0x08)
    {
      DBG (5, "sense:  LINE RAM R/W ERROR\n");
      s++;
    }
  if (sense[22] & 0x10)
    {
      DBG (5, "sense:  PSRAM R/W TEST ERROR\n");
      s++;
    }
  if (sense[22] & 0x20)
    {
      DBG (5, "sense:  POSITIONING ERROR\n");
      s++;
    }
  if (sense[22] & 0x40)
    {
      DBG (5, "sense:  TEST 6 ERROR\n");
      s++;
    }
  if (sense[22] & 0x80)
    {
      DBG (5, "sense:  TEST 7 ERROR\n");
      s++;
    }
  if (sense[23] & 0x01)
    {
      DBG (5, "sense:  TEST 8 ERROR\n");
      s++;
    }
  if (sense[23] & 0x02)
    {
      DBG (5, "sense:  TEST 9 ERROR\n");
      s++;
    }
  if (sense[23] & 0x04)
    {
      DBG (5, "sense:  TEST 10 ERROR\n");
      s++;
    }
  if (sense[23] & 0x08)
    {
      DBG (5, "sense:  TEST 11 ERROR\n");
      s++;
    }
  if (sense[23] & 0x10)
    {
      DBG (5, "sense:  TEST 12 ERROR\n");
      s++;
    }
  if (sense[23] & 0x20)
    {
      DBG (5, "sense:  TEST 13 ERROR\n");
      s++;
    }
  if (sense[23] & 0x40)
    {
      DBG (5, "sense:  TEST 14 ERROR\n");
      s++;
    }
  if (sense[23] & 0x80)
    {
      DBG (5, "sense:  TEST 15 ERROR\n");
      s++;
    }
  /* --- */
  if (s)
    return SANE_STATUS_IO_ERROR;

  switch (sense[0])
    {
    case 0x70:			/* ALWAYS */
      switch (sense[2])
	{
	case 0x00:		/* SUCCES */
	  return SANE_STATUS_GOOD;
	case 0x02:		/* NOT READY */
	  DBG (5, "sense:  NOT READY\n");
	  return SANE_STATUS_IO_ERROR;
	case 0x03:		/* MEDIUM ERROR */
	  DBG (5, "sense:  MEDIUM ERROR\n");
	  return SANE_STATUS_IO_ERROR;
	case 0x04:		/* HARDWARE ERROR */
	  DBG (5, "sense:  HARDWARE ERROR\n");
	  return SANE_STATUS_IO_ERROR;
	case 0x05:		/* ILLEGAL REQUEST */
	  DBG (5, "sense:  ILLEGAL REQUEST\n");
	  return SANE_STATUS_IO_ERROR;
	case 0x06:		/* UNIT ATTENTION */
	  DBG (5, "sense:  UNIT ATTENTION\n");
	  return SANE_STATUS_GOOD;
	default:		/* SENSE KEY UNKNOWN */
	  DBG (5, "sense:  SENSE KEY UNKNOWN (%02x)\n", sense[2]);
	  return SANE_STATUS_IO_ERROR;
	}
    default:
      DBG (5, "sense: Unkown Error Code Qualifier (%02x)\n", sense[0]);
      return SANE_STATUS_IO_ERROR;
    }
  DBG (5, "sense: Should not come here!\n");
  return SANE_STATUS_IO_ERROR;
}


/* DB added a wait routine for the scanner to come ready */
static SANE_Status
wait_ready (int fd)
{
  SANE_Status status;
  int retry = 30;		/* make this tuneable? */

  DBG (3, "wait_ready\n");
  while (retry-- > 0)
    {
      status = sanei_scsi_cmd (fd, test_unit_ready, sizeof (test_unit_ready), 0, 0);
      if (status == SANE_STATUS_GOOD)
	return status;
      if (status == SANE_STATUS_DEVICE_BUSY)
	{
	  sleep (1);
	  continue;
	}
      /* status != GOOD && != BUSY */
      DBG (3, "wait_ready: '%s'\n", sane_strstatus (status));
      return status;
    }
  /* BUSY after n retries */
  DBG (3, "wait_ready: '%s'\n", sane_strstatus (status));
  return status;
}

/* DB added a abort routine, executed via mode select */
static SANE_Status
abort_scan (SANE_Handle handle)
{
  ARTEC_Scanner *s = handle;
  u_int8_t *data, comm[22] =
  {0x15, 0x10, 0, 0, 0x10, 0};

  DBG (3, "mode_select_abort\n");
  data = comm + 6;
  data[0] = 0x00;		/* mode data length */
  data[1] = 0x00;		/* medium type */
  data[2] = 0x00;		/* device specific parameter */
  data[3] = 0x00;		/* block descriptor length */
  data[4] = 0x00;		/* control page parameters */
  data[5] = 0x0a;		/* parameter length */
  data[6] = 0x03;		/* abort+inhibitADF flag */
  data[7] = 0x00;		/* reserved */
  data[8] = 0x00;		/* reserved */

  DBG (3, "abort: sending abort command\n");
  sanei_scsi_cmd (s->fd, comm, 6 + comm[4], 0, 0);

  DBG (3, "abort: wait for scanner to come ready...\n");
  wait_ready (s->fd);

  DBG (3, "abort: resetting abort status\n");
  data[6] = 0x01;		/* reset abort flags */
  sanei_scsi_cmd (s->fd, comm, 6 + comm[4], 0, 0);

  DBG (3, "abort: wait for scanner to come ready...\n");
  return wait_ready (s->fd);
}


static SANE_Status
read_data (int fd, int data_type_code, u_char * dest, size_t * len)
{
  static u_char read_6[10];

  DBG (3, "read_data\n");

  memset (read_6, 0, sizeof (read_6));
  read_6[0] = 0x28;
  read_6[2] = data_type_code;
  read_6[6] = *len >> 16;
  read_6[7] = *len >> 8;
  read_6[8] = *len;

  return (sanei_scsi_cmd (fd, read_6, sizeof (read_6), dest, len));
}

static int
artec_get_status (int fd)
{
  u_char write_10[10];
  u_char read_12[12];
  size_t nread;

  DBG (3, "artec_get_status\n");

  nread = 12;

  memset (write_10, 0, 10);
  write_10[0] = 0x34;
  write_10[8] = 0x0c;

  sanei_scsi_cmd (fd, write_10, 10, read_12, &nread);

  nread = (read_12[9] << 16) + (read_12[10] << 8) + read_12[11];
  DBG (3, "artec_status: %lu\n", (u_long) nread);

  return (nread);
}

static SANE_Status
artec_line_rgb_to_byte_rgb (SANE_Byte * data, SANE_Int len)
{
  SANE_Byte tmp_buf[8192];	/* Artec max dpi 300 * 8.5 inches * 3 = 7650 */
  int count, to;

  /* copy the rgb data to our temp buffer */
  memcpy (tmp_buf, data, len * 3);

  /* now copy back to *data in RGB format */
  for (count = 0, to = 0; count < len; count++, to += 3)
    {
      data[to] = tmp_buf[count];	/* R byte */
      data[to + 1] = tmp_buf[count + len];	/* G byte */
      data[to + 2] = tmp_buf[count + (len * 2)];	/* B byte */
    }

  return (SANE_STATUS_GOOD);
}

static SANE_Byte *tmp_line_buf = NULL;
static SANE_Byte **r_line_buf = NULL;
static SANE_Byte **g_line_buf = NULL;
static SANE_Int r_buf_lines;
static SANE_Int g_buf_lines;

static SANE_Status
artec_buffer_line_offset (SANE_Int line_offset, SANE_Byte * data, size_t *len)
{
  static SANE_Int width;
  static SANE_Int cur_line;
  SANE_Byte *tmp_r_buf_ptr;
  SANE_Byte *tmp_g_buf_ptr;
  int count;

  if (*len == 0)
    return (SANE_STATUS_GOOD);

  if (tmp_line_buf == NULL)
    {
      width = *len / 3;
      cur_line = 0;

      DBG (5, "buffer_line_offset: offset = %d, len = %lu, width = %d\n",
	   line_offset, (u_long) *len, width);

      tmp_line_buf = malloc (*len);
      if (tmp_line_buf == NULL)
	{
	  DBG (5, "couldn't allocate memory for temp line buffer\n");
	  return (SANE_STATUS_NO_MEM);
	}

      r_buf_lines = line_offset * 2;
      r_line_buf = malloc (r_buf_lines * sizeof (SANE_Byte *));
      if (r_line_buf == NULL)
	{
	  DBG (5,
	       "couldn't allocate memory for red line buffer pointers\n");
	  return (SANE_STATUS_NO_MEM);
	}

      for (count = 0; count < r_buf_lines; count++)
	{
	  r_line_buf[count] = malloc (width * sizeof (SANE_Byte));
	  if (r_line_buf[count] == NULL)
	    {
	      DBG (5, "couldn't allocate memory for red line buffer %d\n",
		   count);
	      return (SANE_STATUS_NO_MEM);
	    }
	}

      g_buf_lines = line_offset;
      g_line_buf = malloc (g_buf_lines * sizeof (SANE_Byte *));
      if (g_line_buf == NULL)
	{
	  DBG (5,
	       "couldn't allocate memory for green line buffer pointers\n");
	  return (SANE_STATUS_NO_MEM);
	}

      for (count = 0; count < g_buf_lines; count++)
	{
	  g_line_buf[count] = malloc (width * sizeof (SANE_Byte));
	  if (g_line_buf[count] == NULL)
	    {
	      DBG (5, "couldn't allocate memory for green line buffer %d\n",
		   count);
	      return (SANE_STATUS_NO_MEM);
	    }
	}

      DBG (5, "buffer_line_offset: r lines = %d, g lines = %d\n",
	   r_buf_lines, g_buf_lines);
    }

  cur_line++;

  if (cur_line > r_buf_lines)
    {
      /* get the red line info from r_buf_lines ago */
      memcpy (tmp_line_buf, r_line_buf[0], width);

      /* get the green line info from g_buf_lines ago */
      memcpy (tmp_line_buf + width, g_line_buf[0], width);
    }

  /* move all the buffered red lines down (just move the ptrs for speed) */
  tmp_r_buf_ptr = r_line_buf[0];
  for (count = 0; count < (r_buf_lines - 1); count++)
    {
      r_line_buf[count] = r_line_buf[count + 1];
    }
  r_line_buf[r_buf_lines - 1] = tmp_r_buf_ptr;

  /* insert the supplied red line data at the end of our FIFO */
  memcpy (r_line_buf[r_buf_lines - 1], data, width);

  /* move all the buffered green lines down (just move the ptrs for speed) */
  tmp_g_buf_ptr = g_line_buf[0];
  for (count = 0; count < (g_buf_lines - 1); count++)
    {
      g_line_buf[count] = g_line_buf[count + 1];
    }
  g_line_buf[g_buf_lines - 1] = tmp_g_buf_ptr;

  /* insert the supplied green line data at the end of our FIFO */
  memcpy (g_line_buf[g_buf_lines - 1], data + width, width);

  if (cur_line > r_buf_lines)
    {
      /* copy the red and green data in with the original blue */
      memcpy (data, tmp_line_buf, width * 2);
    }
  else
    {
      /* if we are in the first r_buf_lines, then we don't return anything */
      *len = 0;
    }

  return (SANE_STATUS_GOOD);
}

static SANE_Status
artec_buffer_line_offset_free (void)
{
  int count;

  free (tmp_line_buf);
  tmp_line_buf = NULL;

  for (count = 0; count < r_buf_lines; count++)
    {
      free (r_line_buf[count]);
    }
  free (r_line_buf);
  r_line_buf = NULL;

  for (count = 0; count < g_buf_lines; count++)
    {
      free (g_line_buf[count]);
    }
  free (g_line_buf);
  g_line_buf = NULL;

  return (SANE_STATUS_GOOD);
}

static SANE_Status
artec_set_scan_window (SANE_Handle handle)
{
  ARTEC_Scanner *s = handle;
  char write_6[4096];
  char *data;
  int counter;

  DBG (3, "artec_set_scan_window\n");

  /*
   * if we can, start before the desired window since we have to throw away
   * s->line_offset number of rows because of the RGB fixup.
   */
  if ((s->line_offset > 0) && (s->tl_x > 0))
    s->tl_x -= s->line_offset;

  data = write_6 + 10;

  DBG (20, "Scan window info:\n");
  DBG (20, "  X resolution: %5d (%d-%d)\n",
       s->x_resolution,
       s->hw->x_dpi_range.min, s->hw->x_dpi_range.max);
  DBG (20, "  Y resolution: %5d (%d-%d)\n",
       s->y_resolution,
       s->hw->y_dpi_range.min, s->hw->y_dpi_range.max);
  DBG (20, "  TL_X (pixel): %5d\n",
       s->tl_x);
  DBG (20, "  TL_Y (pixel): %5d\n",
       s->tl_y);

#ifdef PREFER_PIXEL_MODE
  DBG (20, "  Width       : %5d (%d-%d)\n",
       s->params.pixels_per_line,
       s->hw->x_range.min,
       (int) (s->hw->x_range.max));
  DBG (20, "  Height      : %5d (%d-%d)\n",
       s->params.lines,
       s->hw->y_range.min,
       (int) (s->hw->y_range.max));
#else
  DBG (20, "  Width       : %5d (%d-%d)\n",
       s->params.pixels_per_line,
       s->hw->x_range.min,
       (int) ((SANE_UNFIX (s->hw->x_range.max) / MM_PER_INCH) *
	      s->x_resolution));
  DBG (20, "  Height      : %5d (%d-%d)\n",
       s->params.lines,
       s->hw->y_range.min,
       (int) ((SANE_UNFIX (s->hw->y_range.max) / MM_PER_INCH) *
	      s->y_resolution));
#endif

  DBG (20, "  Image Comp. : %s\n", s->mode);
  DBG (20, "  Line Offset : %lu\n", (u_long) s->line_offset);

  memset (write_6, 0, 4096);
  write_6[0] = 0x24;
  write_6[8] = s->hw->setwindow_cmd_size;	/* total size of command */

  /* beginning of set window data header */
  data[7] = s->hw->setwindow_cmd_size - 8;	/* actual data byte count */
  data[10] = s->x_resolution >> 8;	/* x resolution */
  data[11] = s->x_resolution;
  data[12] = s->y_resolution >> 8;	/* y resolution */
  data[13] = s->y_resolution;
  data[14] = s->tl_x >> 24;	/* top left X value */
  data[15] = s->tl_x >> 16;
  data[16] = s->tl_x >> 8;
  data[17] = s->tl_x;
  data[18] = s->tl_y >> 24;	/* top left Y value */
  data[19] = s->tl_y >> 16;
  data[20] = s->tl_y >> 8;
  data[21] = s->tl_y;
  data[22] = s->params.pixels_per_line >> 24;	/* width */
  data[23] = s->params.pixels_per_line >> 16;
  data[24] = s->params.pixels_per_line >> 8;
  data[25] = s->params.pixels_per_line;
  data[26] = (s->params.lines + (s->line_offset * 2)) >> 24;	/* height */
  data[27] = (s->params.lines + (s->line_offset * 2)) >> 16;
  data[28] = (s->params.lines + (s->line_offset * 2)) >> 8;
  data[29] = (s->params.lines + (s->line_offset * 2));
  data[30] = s->val[OPT_BRIGHTNESS].w;	/* brightness (if supported) */
  data[31] = s->val[OPT_THRESHOLD].w;	/* threshold */
  data[32] = s->val[OPT_CONTRAST].w;	/* contrast */

  if (strcmp (s->mode, "Lineart") == 0)
    {
      data[33] = ARTEC_COMP_LINEART;
    }
  else if (strcmp (s->mode, "Halftone") == 0)
    {
      data[33] = ARTEC_COMP_HALFTONE;
    }
  else if (strcmp (s->mode, "Gray") == 0)
    {
      data[33] = ARTEC_COMP_GRAY;
    }
  else if (strcmp (s->mode, "Color") == 0)
    {
      data[33] = ARTEC_COMP_COLOR;
    }

  data[34] = s->params.depth;	/* bits per pixel */

  data[35] = artec_get_str_index (halftone_pattern_list,
				  s->val[OPT_HALFTONE_PATTERN].s);	/* halftone pattern */
  /* user supplied halftone pattern not supported so override with 8x8 Bayer */
  if (data[35] == 0)
    {
      data[35] = 4;
    }

  /* reverse image format in 1bpp mode.  normal = 0, reverse = 0x80 */
  if (s->val[OPT_NEGATIVE].w == SANE_TRUE)
    {
      data[37] = 0x80;
    }
  else
    {
      data[37] = 0x0;
    }

  /* NOTE: AT12 doesn't support mono according to docs. */
  data[48] = artec_get_str_index (filter_type_list,
				  s->val[OPT_FILTER_TYPE].s);	/* filter mode */

  if (s->hw->setwindow_cmd_size > 55)
    {
      data[48] = 0x2;		/* DB filter type green for AT12,see above */
      /* this stuff is for models like the AT12 which support additional info */
      data[55] = 0x00;		/* buffer full line count */
      data[56] = 0x00;		/* buffer full line count */
      data[57] = 0x00;		/* buffer full line count */
      data[58] = 0x0a;		/* buffer full line count *//* guessing at this value */
      data[59] = 0x00;		/* access line count */
      data[60] = 0x00;		/* access line count */
      data[61] = 0x00;		/* access line count */
      data[62] = 0x0a;		/* access line count *//* guessing at this value */

      /* DB : following fields : high order bit (0x80) is enable */
      data[63] = 0x80;		/* scanner handles line offset fixup, 0 = driver handles */
      data[64] = 0;		/* disable pixel average function, 0x80 = enable */
      data[65] = 0;		/* disable lineart edge enhancement function, 0x80 = enable */
      data[66] = 0;		/* data is R-G-B format, 0x80 = G-B-R format (reversed) */
    }

  DBG (100, "Set Window data : \n");
  for (counter = 0; counter < s->hw->setwindow_cmd_size; counter++)
    {
      DBG (100, "  byte %2d = %02x \n", counter, data[counter] & 0xff);		/* DB */
    }
  DBG (100, "\n");

  /* set the scan window */
  return (sanei_scsi_cmd (s->fd, write_6, 10 +
			  s->hw->setwindow_cmd_size, 0, 0));
}

static SANE_Status
artec_start_scan (SANE_Handle handle)
{
  ARTEC_Scanner *s = handle;
  char write_7[7];

  DBG (3, "artec_start_scan\n");

  /* setup cmd to start scanning */
  memset (write_7, 0, 7);
  write_7[0] = 0x1b;
  write_7[4] = 0x01;

  /* start the scan */
  return (sanei_scsi_cmd (s->fd, write_7, 7, 0, 0));
}

static SANE_Status
artec_calibrate_shading (SANE_Handle handle)
{
  ARTEC_Scanner *s = handle;
  SANE_Status status;		/* DB added */
  u_char buf[76800];		/* should be big enough */
  size_t len;
  SANE_Word save_x_resolution;
  SANE_Word save_pixels_per_line;

  DBG (3, "artec_calibrate_white_shading\n");

  if (s->hw->calibrate_method == ARTEC_CALIB_RGB)
    {
      /* this method scans in 4 lines each of Red, Green, and Blue */
      len = 4 * 2592;		/* 4 lines of data, 2592 pixels wide */

      read_data (s->fd, ARTEC_DATA_RED_SHADING, buf, &len);
      read_data (s->fd, ARTEC_DATA_GREEN_SHADING, buf, &len);
      read_data (s->fd, ARTEC_DATA_BLUE_SHADING, buf, &len);
    }
  else if (s->hw->calibrate_method == ARTEC_CALIB_DARK_WHITE)
    {
      /* this method scans black, then white data */
      len = 3 * 5100;		/* 1 line of data, 5100 pixels wide, RGB data */
      read_data (s->fd, ARTEC_DATA_DARK_SHADING, buf, &len);
      save_x_resolution = s->x_resolution;
      s->x_resolution = 600;
      save_pixels_per_line = s->params.pixels_per_line;
      s->params.pixels_per_line = s->hw->x_dpi_range.max;
      s->params.pixels_per_line = 600 * 8.5;	/* ?this? or ?above line? */
      /* DB added wait_ready */
      status = wait_ready (s->fd);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (1, "wait for scanner ready failed: %s\n", sane_strstatus (status));
	  return status;
	}
      /* next line should use ARTEC_DATA_WHITE_SHADING_TRANS if using ADF */
      read_data (s->fd, ARTEC_DATA_WHITE_SHADING_OPT, buf, &len);
      s->x_resolution = save_x_resolution;
      s->params.pixels_per_line = save_pixels_per_line;
    }

  return (0);
}


static SANE_Status
end_scan (SANE_Handle handle)
{
  ARTEC_Scanner *s = handle;
  /* DB
     u_int8_t write_6[6] =
     {0x1B, 0, 0, 0, 0, 0};
   */

  DBG (3, "end_scan...\n");
  s->scanning = SANE_FALSE;

  /* DB
     return (sanei_scsi_cmd (s->fd, write_6, 6, 0, 0));
   */
  return abort_scan (s);
}


static SANE_Status
artec_get_cap_data (ARTEC_Device * dev, int fd)
{
  int cap_model, loop;
  SANE_Status status;

  /* DB always use the hard-coded capability info first 
   * if we get cap data from the scanner, we override */
  cap_model = -1;
  for (loop = 0; loop < NELEMS (cap_data); loop++)
    {
      if (strcmp (cap_data[loop].model, dev->sane.model) == 0)
	{
	  cap_model = loop;
	}
    }

  if (cap_model == -1)
    {
      DBG (5, "unable to identify Artec model '%s', check artec.h\n",
	   dev->sane.model);
      return (SANE_STATUS_UNSUPPORTED);
    }

  dev->x_range.min = 0;
  dev->x_range.quant = 1;

#ifdef PREFER_PIXEL_MODE
  dev->x_range.max = cap_data[cap_model].x_max;
#else
  dev->x_range.max = SANE_FIX (cap_data[cap_model].width) * MM_PER_INCH;
#endif

  dev->width = cap_data[cap_model].width;

  dev->y_range.min = 0;
  dev->y_range.quant = 1;

#ifdef PREFER_PIXEL_MODE
  dev->y_range.max = cap_data[cap_model].y_max;
#else
  dev->y_range.max = SANE_FIX (cap_data[cap_model].height) * MM_PER_INCH;
#endif

  dev->height = cap_data[cap_model].height;

  dev->x_dpi_range.min = cap_data[cap_model].x_dpi_min;
  dev->x_dpi_range.max = cap_data[cap_model].x_dpi_max;
  dev->x_dpi_range.quant = 1;
  status = artec_str_list_to_word_list (&dev->horz_resolution_list,
				   cap_data[cap_model].horz_resolution_str);

  dev->y_dpi_range.min = cap_data[cap_model].y_dpi_min;
  dev->y_dpi_range.max = cap_data[cap_model].y_dpi_max;
  dev->y_dpi_range.quant = 1;
  status = artec_str_list_to_word_list (&dev->vert_resolution_list,
				   cap_data[cap_model].vert_resolution_str);

  dev->contrast_range.min = cap_data[cap_model].contrast_min;
  dev->contrast_range.max = cap_data[cap_model].contrast_max;
  dev->contrast_range.quant = 1;

  dev->threshold_range.min = cap_data[cap_model].threshold_min;
  dev->threshold_range.max = cap_data[cap_model].threshold_max;
  dev->threshold_range.quant = 1;

  dev->sane.type = cap_data[cap_model].type;

  dev->req_shading_calibrate = cap_data[cap_model].req_shading_calibrate;
  dev->req_rgb_line_offset = cap_data[cap_model].req_rgb_line_offset;
  dev->req_rgb_char_shift = cap_data[cap_model].req_rgb_char_shift;

  dev->opt_brightness = cap_data[cap_model].opt_brightness;
  dev->brightness_range.min = cap_data[cap_model].brightness_min;
  dev->brightness_range.max = cap_data[cap_model].brightness_max;
  dev->brightness_range.quant = 1;

  dev->setwindow_cmd_size = cap_data[cap_model].setwindow_cmd_size;

  dev->calibrate_method = cap_data[cap_model].calibrate_method;

  if (dev->support_cap_data_retrieve)	/* DB */
    {
      /* DB added reading capability data from scanner */
      char info[80];		/* for printing debugging info */
      size_t len = sizeof (cap_buf);

      /* read the capability data from the scanner */
      DBG (10, "reading capability data from scanner...\n");

      wait_ready (fd);

      read_data (fd, ARTEC_DATA_CAPABILITY_DATA, cap_buf, &len);

      DBG (100, "scanner capability data : \n");
      /*
         for (i = 0; i < 59; i++)
         {
         DBG (100, "  byte %2d = %02x \n", i, cap_buf[i]&0xff); 
         }
         DBG (100, "\n");
       */

      strncpy (info, (const char *) &cap_buf[0], 8);
      DBG (100, "  Vendor                    : %s\n", info);
      strncpy (info, (const char *) &cap_buf[8], 16);
      DBG (100, "  Device Name               : %s\n", info);
      strncpy (info, (const char *) &cap_buf[24], 4);
      DBG (100, "  Version Number            : %s\n", info);
      sprintf (info, "%d ", cap_buf[29]);
      DBG (100, "  CCD Type                  : %s\n", info);
      sprintf (info, "%d ", cap_buf[30]);
      DBG (100, "  AD Converter Type         : %s\n", info);
      sprintf (info, "%d ", (cap_buf[31] << 8) | cap_buf[32]);
      DBG (100, "  Buffer size               : %s\n", info);
      sprintf (info, "%d ", cap_buf[33]);
      DBG (100, "  Channels of RGB Gamma     : %s\n", info);
      sprintf (info, "%d ", (cap_buf[34] << 8) | cap_buf[35]);
      DBG (100, "  Opt. res. of R channel    : %s\n", info);
      sprintf (info, "%d ", (cap_buf[36] << 8) | cap_buf[37]);
      DBG (100, "  Opt. res. of G channel    : %s\n", info);
      sprintf (info, "%d ", (cap_buf[38] << 8) | cap_buf[39]);
      DBG (100, "  Opt. res. of B channel    : %s\n", info);
      sprintf (info, "%d ", (cap_buf[40] << 8) | cap_buf[41]);
      DBG (100, "  Min. Hor. Resolution      : %s\n", info);
      sprintf (info, "%d ", (cap_buf[42] << 8) | cap_buf[43]);
      DBG (100, "  Max. Vert. Resolution     : %s\n", info);
      sprintf (info, "%d ", (cap_buf[44] << 8) | cap_buf[45]);
      DBG (100, "  Min. Vert. Resolution     : %s\n", info);
      sprintf (info, "%s ", cap_buf[46] == 0x80 ? "yes" : "no");
      DBG (100, "  Chunky Data Format        : %s\n", info);
      sprintf (info, "%s ", cap_buf[47] == 0x80 ? "yes" : "no");
      DBG (100, "  RGB Data Format           : %s\n", info);
      sprintf (info, "%s ", cap_buf[48] == 0x80 ? "yes" : "no");
      DBG (100, "  BGR Data Format           : %s\n", info);
      sprintf (info, "%d ", cap_buf[49]);
      DBG (100, "  Line Offset               : %s\n", info);
      sprintf (info, "%s ", cap_buf[50] == 0x80 ? "yes" : "no");
      DBG (100, "  Channel Valid Sequence    : %s\n", info);
      sprintf (info, "%s ", cap_buf[51] == 0x80 ? "yes" : "no");
      DBG (100, "  True Gray                 : %s\n", info);
      sprintf (info, "%s ", cap_buf[52] == 0x80 ? "yes" : "no");
      DBG (100, "  Force Host Not Do Shading : %s\n", info);
      sprintf (info, "%s ", cap_buf[53] == 0x00 ? "AT006" : "AT010");
      DBG (100, "  ASIC                      : %s\n", info);
      sprintf (info, "%s ", cap_buf[54] == 0x82 ? "SCSI2" : "PAR-or-SCSI1");
      DBG (100, "  Interface                 : %s\n", info);
      sprintf (info, "%d ", (cap_buf[55] << 8) | cap_buf[56]);
      DBG (100, "  Phys. Area Width          : %s\n", info);
      sprintf (info, "%d ", (cap_buf[57] << 8) | cap_buf[58]);
      DBG (100, "  Phys. Area Length         : %s\n", info);

      /* fill in the information we've got from the scanner */

      dev->width = ((float) ((cap_buf[55] << 8) | cap_buf[56])) / 1000;
      dev->height = ((float) ((cap_buf[57] << 8) | cap_buf[58])) / 1000;

      dev->x_dpi_range.min = (cap_buf[40] << 8) | cap_buf[41];
      /* pick for the hor. max resolution the R channel max */
      dev->x_dpi_range.max = (cap_buf[34] << 8) | cap_buf[35];
      dev->y_dpi_range.min = (cap_buf[44] << 8) | cap_buf[45];
      dev->y_dpi_range.max = (cap_buf[42] << 8) | cap_buf[43];

      /* for test */
      dev->x_dpi_range.min = 50;
      dev->y_dpi_range.min = 50;

      /* DB ----- */

    }

  DBG (5, "Scanner capability info.\n");
  DBG (5, "  Vendor      : %s\n", dev->sane.vendor);
  DBG (5, "  Model       : %s\n", dev->sane.model);
  DBG (5, "  Type        : %s\n", dev->sane.type);
  DBG (5, "  Width       : %.2f inches\n", dev->width);
  DBG (5, "  Height      : %.2f inches\n", dev->height);

#ifdef PREFER_PIXEL_MODE
  DBG (5, "  X Range(pel): %d-%d\n",
       dev->x_range.min, dev->x_range.max);
  DBG (5, "  Y Range(pel): %d-%d\n",
       dev->y_range.min, dev->y_range.max);
#else
  DBG (5, "  X Range(mm) : %d-%d\n",
       dev->x_range.min,
       (int) (SANE_UNFIX (dev->x_range.max)));
  DBG (5, "  Y Range(mm) : %d-%d\n",
       dev->y_range.min,
       (int) (SANE_UNFIX (dev->y_range.max)));
#endif

  DBG (5, "  Horz. DPI   : %d-%d\n",
       dev->x_dpi_range.min, dev->x_dpi_range.max);
  DBG (5, "  Vert. DPI   : %d-%d\n",
       dev->y_dpi_range.min, dev->y_dpi_range.max);
  DBG (5, "  Contrast    : %d-%d\n",
       dev->contrast_range.min, dev->contrast_range.max);
  DBG (5, "  REQ Sh. Cal.: %d\n",
       dev->req_shading_calibrate);
  DBG (5, "  REQ Ln. Offs: %d\n",
       dev->req_rgb_line_offset);
  DBG (5, "  REQ Ch. Shft: %d\n",
       dev->req_rgb_char_shift);
  DBG (5, "  SetWind Size: %d\n",
       dev->setwindow_cmd_size);
  DBG (5, "  Calib Method: %d\n",
       dev->calibrate_method);

  return (SANE_STATUS_GOOD);
}

static SANE_Status
attach (const char *devname, ARTEC_Device ** devp)
{
  char result[INQ_LEN];
  char product_revision[5];
  char *str, *t;
  int fd;
  SANE_Status status;
  ARTEC_Device *dev;
  size_t size;

  DBG (1, "Artec/Ultima backend version %d.%d, last mod: %s\n",
       ARTEC_MAJOR, ARTEC_MINOR, ARTEC_LAST_MOD);
  DBG (1, "http://www4.infi.net/~cpinkham/sane/sane-artec-doc.html\n");

  for (dev = first_dev; dev; dev = dev->next)
    {
      if (strcmp (dev->sane.name, devname) == 0)
	{
	  if (devp)
	    *devp = dev;
	  return (SANE_STATUS_GOOD);
	}
    }

  DBG (3, "attach: opening %s\n", devname);
  /* DB added sense handler (make it configurable?) */
  status = sanei_scsi_open (devname, &fd, sense_handler, 0);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "attach: open failed (%s)\n", sane_strstatus (status));
      return (SANE_STATUS_INVAL);
    }

  DBG (3, "attach: sending INQUIRY\n");
  size = sizeof (result);
  status = sanei_scsi_cmd (fd, inquiry, sizeof (inquiry), result, &size);
  if (status != SANE_STATUS_GOOD || size < 16)
    {
      DBG (1, "attach: inquiry failed (%s)\n", sane_strstatus (status));
      sanei_scsi_close (fd);
      return (status);
    }

  /* are we really dealing with a scanner by ULTIMA/ARTEC? */
  if ((result[0] != 0x06) ||
      (strncmp (result + 8, "ULTIMA", 6) != 0))
    {
      DBG (1, "attach: device doesn't look like a ULTIMA scanner\n");
      sanei_scsi_close (fd);
      return (SANE_STATUS_INVAL);
    }

  /* DB changed
     DBG (3, "attach: sending TEST_UNIT_READY\n");
     status = sanei_scsi_cmd (fd, test_unit_ready, sizeof (test_unit_ready),
     0, 0);
   */
  DBG (3, "attach: wait for scanner to come ready\n");
  status = wait_ready (fd);

  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "attach: test unit ready failed (%s)\n",
	   sane_strstatus (status));
      sanei_scsi_close (fd);
      return (status);
    }

  dev = malloc (sizeof (*dev));
  if (!dev)
    return (SANE_STATUS_NO_MEM);

  memset (dev, 0, sizeof (*dev));
  dev->sane.name = strdup (devname);

  /* get the model info */
  str = malloc (17);
  memcpy (str, result + 16, 16);
  str[16] = ' ';
  t = strchr (str, ' ');
  *t = '\0';
  dev->sane.model = str;

  /* get the product revision */
  strncpy (product_revision, result + 32, 4);
  product_revision[4] = ' ';
  t = strchr (product_revision, ' ');
  *t = '\0';

  /* get the vendor info */
  str = malloc (9);
  memcpy (str, result + 8, 8);
  str[8] = ' ';
  t = strchr (str, ' ');
  *t = '\0';
  dev->sane.vendor = str;

  /* Artec docs say if bytes 36-43 = "ULTIMA  ", then supports read cap. data */
  if (strncmp (result + 36, "ULTIMA  ", 8) == 0)
    {
      DBG (5, "scanner supports read capability data function\n");
      dev->support_cap_data_retrieve = SANE_TRUE;
    }
  else
    {
      DBG (5, "scanner does NOT support read capability data function\n");
      dev->support_cap_data_retrieve = SANE_FALSE;
    }

  DBG (3, "attach: getting scanner capability data\n");
  status = artec_get_cap_data (dev, fd);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "attach: artec_get_cap_data failed (%s)\n",
	   sane_strstatus (status));
      sanei_scsi_close (fd);
      return (status);
    }

  sanei_scsi_close (fd);

  DBG (1, "attach: found %s model %s version %s, x=%d-%d, y=%d-%d, "
       "xres=%d-%ddpi, yres=%d-%ddpi\n",
       dev->sane.vendor, dev->sane.model, product_revision,
       dev->x_range.min, dev->x_range.max,
       dev->y_range.min, dev->y_range.max,
       dev->x_dpi_range.min, dev->x_dpi_range.max,
       dev->y_dpi_range.min, dev->y_dpi_range.max);

  ++num_devices;
  dev->next = first_dev;
  first_dev = dev;

  if (devp)
    *devp = dev;

  return (SANE_STATUS_GOOD);
}

static SANE_Status
init_options (ARTEC_Scanner * s)
{
  int i;

  DBG (3, "init_options\n");

  memset (s->opt, 0, sizeof (s->opt));
  memset (s->val, 0, sizeof (s->val));

  for (i = 0; i < NUM_OPTIONS; ++i)
    {
      s->opt[i].size = sizeof (SANE_Word);
      s->opt[i].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    }

  s->opt[OPT_NUM_OPTS].title = SANE_TITLE_NUM_OPTIONS;
  s->opt[OPT_NUM_OPTS].desc = SANE_DESC_NUM_OPTIONS;
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
  s->opt[OPT_MODE].size = max_string_size (mode_list);
  s->opt[OPT_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_MODE].constraint.string_list = mode_list;
  s->val[OPT_MODE].s = strdup (mode_list[3]);

  /* horizontal resolution */
  s->opt[OPT_X_RESOLUTION].name = SANE_NAME_SCAN_RESOLUTION;
  s->opt[OPT_X_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
  s->opt[OPT_X_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;
  s->opt[OPT_X_RESOLUTION].type = SANE_TYPE_INT;
  s->opt[OPT_X_RESOLUTION].unit = SANE_UNIT_DPI;
  s->opt[OPT_X_RESOLUTION].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_X_RESOLUTION].constraint.range = &s->hw->x_dpi_range;
  s->val[OPT_X_RESOLUTION].w = 100;

  s->opt[OPT_X_RESOLUTION].constraint_type = SANE_CONSTRAINT_WORD_LIST;
  s->opt[OPT_X_RESOLUTION].constraint.word_list = s->hw->horz_resolution_list;

  /* vertical resolution */
  s->opt[OPT_Y_RESOLUTION].name = SANE_NAME_SCAN_Y_RESOLUTION;
  s->opt[OPT_Y_RESOLUTION].title = SANE_TITLE_SCAN_Y_RESOLUTION;
  s->opt[OPT_Y_RESOLUTION].desc = SANE_DESC_SCAN_Y_RESOLUTION;
  s->opt[OPT_Y_RESOLUTION].type = SANE_TYPE_INT;
  s->opt[OPT_Y_RESOLUTION].unit = SANE_UNIT_DPI;
  s->opt[OPT_Y_RESOLUTION].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_Y_RESOLUTION].constraint.range = &s->hw->y_dpi_range;
  s->opt[OPT_Y_RESOLUTION].cap |= SANE_CAP_INACTIVE;
  s->val[OPT_Y_RESOLUTION].w = 100;

  s->opt[OPT_Y_RESOLUTION].constraint_type = SANE_CONSTRAINT_WORD_LIST;
  s->opt[OPT_Y_RESOLUTION].constraint.word_list = s->hw->vert_resolution_list;

  /* bind resolution */
  s->opt[OPT_RESOLUTION_BIND].name = SANE_NAME_RESOLUTION_BIND;
  s->opt[OPT_RESOLUTION_BIND].title = SANE_TITLE_RESOLUTION_BIND;
  s->opt[OPT_RESOLUTION_BIND].desc = SANE_DESC_RESOLUTION_BIND;
  s->opt[OPT_RESOLUTION_BIND].type = SANE_TYPE_BOOL;
  s->val[OPT_RESOLUTION_BIND].w = SANE_TRUE;

  /*
   * For some reason I can't get the scanner to scan at different X & Y
   * resolutions, so this option is disabled for now, forcing X res & Y res
   * to be equal.
   */
  s->opt[OPT_RESOLUTION_BIND].cap |= SANE_CAP_INACTIVE;

  /* Preview Mode */
  s->opt[OPT_PREVIEW].name = SANE_NAME_PREVIEW;
  s->opt[OPT_PREVIEW].title = SANE_TITLE_PREVIEW;
  s->opt[OPT_PREVIEW].desc = SANE_DESC_PREVIEW;
  s->opt[OPT_PREVIEW].type = SANE_TYPE_BOOL;
  s->opt[OPT_PREVIEW].unit = SANE_UNIT_NONE;
  s->opt[OPT_PREVIEW].size = sizeof (SANE_Word);
  s->val[OPT_PREVIEW].w = SANE_FALSE;

  /* Grayscale Preview Mode */
  s->opt[OPT_GRAY_PREVIEW].name = SANE_NAME_GRAY_PREVIEW;
  s->opt[OPT_GRAY_PREVIEW].title = SANE_TITLE_GRAY_PREVIEW;
  s->opt[OPT_GRAY_PREVIEW].desc = SANE_DESC_GRAY_PREVIEW;
  s->opt[OPT_GRAY_PREVIEW].type = SANE_TYPE_BOOL;
  s->opt[OPT_GRAY_PREVIEW].unit = SANE_UNIT_NONE;
  s->opt[OPT_GRAY_PREVIEW].size = sizeof (SANE_Word);
  s->val[OPT_GRAY_PREVIEW].w = SANE_FALSE;

  /* negative */
  s->opt[OPT_NEGATIVE].name = SANE_NAME_NEGATIVE;
  s->opt[OPT_NEGATIVE].title = SANE_TITLE_NEGATIVE;
  s->opt[OPT_NEGATIVE].desc = SANE_DESC_NEGATIVE;
  s->opt[OPT_NEGATIVE].type = SANE_TYPE_BOOL;
  s->val[OPT_NEGATIVE].w = SANE_FALSE;

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

#ifdef PREFER_PIXEL_MODE
  s->opt[OPT_TL_X].type = SANE_TYPE_INT;
  s->opt[OPT_TL_X].unit = SANE_UNIT_PIXEL;
#else
  s->opt[OPT_TL_X].type = SANE_TYPE_FIXED;
  s->opt[OPT_TL_X].unit = SANE_UNIT_MM;
#endif

  s->opt[OPT_TL_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TL_X].constraint.range = &s->hw->x_range;
  s->val[OPT_TL_X].w = s->hw->x_range.min;

  /* top-left y */
  s->opt[OPT_TL_Y].name = SANE_NAME_SCAN_TL_Y;
  s->opt[OPT_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
  s->opt[OPT_TL_Y].desc = SANE_DESC_SCAN_TL_Y;

#ifdef PREFER_PIXEL_MODE
  s->opt[OPT_TL_Y].type = SANE_TYPE_INT;
  s->opt[OPT_TL_Y].unit = SANE_UNIT_PIXEL;
#else
  s->opt[OPT_TL_Y].type = SANE_TYPE_FIXED;
  s->opt[OPT_TL_Y].unit = SANE_UNIT_MM;
#endif

  s->opt[OPT_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TL_Y].constraint.range = &s->hw->y_range;
  s->val[OPT_TL_Y].w = s->hw->y_range.min;

  /* bottom-right x */
  s->opt[OPT_BR_X].name = SANE_NAME_SCAN_BR_X;
  s->opt[OPT_BR_X].title = SANE_TITLE_SCAN_BR_X;
  s->opt[OPT_BR_X].desc = SANE_DESC_SCAN_BR_X;

#ifdef PREFER_PIXEL_MODE
  s->opt[OPT_BR_X].type = SANE_TYPE_INT;
  s->opt[OPT_BR_X].unit = SANE_UNIT_PIXEL;
#else
  s->opt[OPT_BR_X].type = SANE_TYPE_FIXED;
  s->opt[OPT_BR_X].unit = SANE_UNIT_MM;
#endif

  s->opt[OPT_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_X].constraint.range = &s->hw->x_range;

#ifdef PREFER_PIXEL_MODE
  s->val[OPT_BR_X].w = s->hw->width *
    s->val[OPT_X_RESOLUTION].w;
#else
  s->val[OPT_BR_X].w = s->hw->x_range.max;
#endif

  /* bottom-right y */
  s->opt[OPT_BR_Y].name = SANE_NAME_SCAN_BR_Y;
  s->opt[OPT_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
  s->opt[OPT_BR_Y].desc = SANE_DESC_SCAN_BR_Y;

#ifdef PREFER_PIXEL_MODE
  s->opt[OPT_BR_Y].type = SANE_TYPE_INT;
  s->opt[OPT_BR_Y].unit = SANE_UNIT_PIXEL;
#else
  s->opt[OPT_BR_Y].type = SANE_TYPE_FIXED;
  s->opt[OPT_BR_Y].unit = SANE_UNIT_MM;
#endif

  s->opt[OPT_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_Y].constraint.range = &s->hw->y_range;

#ifdef PREFER_PIXEL_MODE
  s->val[OPT_BR_Y].w = s->hw->height *
    s->val[OPT_Y_RESOLUTION].w;
#else
  s->val[OPT_BR_Y].w = s->hw->y_range.max;
#endif

  /* Enhancement group: */
  s->opt[OPT_ENHANCEMENT_GROUP].title = "Enhancement";
  s->opt[OPT_ENHANCEMENT_GROUP].desc = "";
  s->opt[OPT_ENHANCEMENT_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_ENHANCEMENT_GROUP].cap = SANE_CAP_ADVANCED;
  s->opt[OPT_ENHANCEMENT_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* Shading Calibrate */
  s->opt[OPT_QUALITY_CAL].name = SANE_NAME_QUALITY_CAL;
  s->opt[OPT_QUALITY_CAL].title = SANE_TITLE_QUALITY_CAL;
  s->opt[OPT_QUALITY_CAL].desc = SANE_DESC_QUALITY_CAL;
  s->opt[OPT_QUALITY_CAL].type = SANE_TYPE_BOOL;
  s->val[OPT_QUALITY_CAL].w = SANE_TRUE;
  if (s->hw->req_shading_calibrate == SANE_FALSE)
    {
      s->opt[OPT_QUALITY_CAL].cap |= SANE_CAP_INACTIVE;
    }

  /* filter mode */
  s->opt[OPT_FILTER_TYPE].name = "filter-type";
  s->opt[OPT_FILTER_TYPE].title = "Filter Type";
  s->opt[OPT_FILTER_TYPE].desc = "Filter Type for mono scans";
  s->opt[OPT_FILTER_TYPE].type = SANE_TYPE_STRING;
  s->opt[OPT_FILTER_TYPE].size = max_string_size (filter_type_list);
  s->opt[OPT_FILTER_TYPE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_FILTER_TYPE].constraint.string_list = filter_type_list;
  s->val[OPT_FILTER_TYPE].s = strdup (filter_type_list[0]);

  /* contrast */
  s->opt[OPT_CONTRAST].name = SANE_NAME_CONTRAST;
  s->opt[OPT_CONTRAST].title = SANE_TITLE_CONTRAST;
  s->opt[OPT_CONTRAST].desc = SANE_DESC_CONTRAST;
  s->opt[OPT_CONTRAST].type = SANE_TYPE_INT;
  s->opt[OPT_CONTRAST].unit = SANE_UNIT_NONE;
  s->opt[OPT_CONTRAST].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_CONTRAST].constraint.range = &s->hw->contrast_range;
  s->val[OPT_CONTRAST].w = 0x80;

  /* threshold */
  s->opt[OPT_THRESHOLD].name = SANE_NAME_THRESHOLD;
  s->opt[OPT_THRESHOLD].title = SANE_TITLE_THRESHOLD;
  s->opt[OPT_THRESHOLD].desc = SANE_DESC_THRESHOLD;
  s->opt[OPT_THRESHOLD].type = SANE_TYPE_INT;
  s->opt[OPT_THRESHOLD].unit = SANE_UNIT_NONE;
  s->opt[OPT_THRESHOLD].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_THRESHOLD].constraint.range = &s->hw->threshold_range;
  s->val[OPT_THRESHOLD].w = 0x80;

  /* brightness */
  s->opt[OPT_BRIGHTNESS].name = SANE_NAME_BRIGHTNESS;
  s->opt[OPT_BRIGHTNESS].title = SANE_TITLE_BRIGHTNESS;
  s->opt[OPT_BRIGHTNESS].desc = SANE_DESC_BRIGHTNESS;
  s->opt[OPT_BRIGHTNESS].type = SANE_TYPE_INT;
  s->opt[OPT_BRIGHTNESS].unit = SANE_UNIT_NONE;
  s->opt[OPT_BRIGHTNESS].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BRIGHTNESS].constraint.range = &s->hw->brightness_range;
  s->val[OPT_BRIGHTNESS].w = 0x80;

  if (!s->hw->opt_brightness)
    {
      s->opt[OPT_BRIGHTNESS].cap |= SANE_CAP_INACTIVE;
    }

  /* halftone pattern */
  s->opt[OPT_HALFTONE_PATTERN].name = SANE_NAME_HALFTONE_PATTERN;
  s->opt[OPT_HALFTONE_PATTERN].title = SANE_TITLE_HALFTONE_PATTERN;
  s->opt[OPT_HALFTONE_PATTERN].desc = SANE_DESC_HALFTONE_PATTERN;
  s->opt[OPT_HALFTONE_PATTERN].type = SANE_TYPE_STRING;
  s->opt[OPT_HALFTONE_PATTERN].size = max_string_size (halftone_pattern_list);
  s->opt[OPT_HALFTONE_PATTERN].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_HALFTONE_PATTERN].constraint.string_list = halftone_pattern_list;
  s->val[OPT_HALFTONE_PATTERN].s = strdup (halftone_pattern_list[1]);

  return (SANE_STATUS_GOOD);
}

static SANE_Status
do_cancel (ARTEC_Scanner * s)
{
  DBG (3, "do_cancel\n");

  s->scanning = SANE_FALSE;

  if (s->fd >= 0)
    {
      sanei_scsi_close (s->fd);
      s->fd = -1;
    }

  return (SANE_STATUS_CANCELLED);
}


static SANE_Status
attach_one (const char *dev)
{
  DBG (3, "attach_one\n");

  attach (dev, 0);
  return (SANE_STATUS_GOOD);
}


SANE_Status
sane_init (SANE_Int * version_code, SANE_Auth_Callback authorize)
{
  char dev_name[PATH_MAX];
  size_t len;
  FILE *fp;

  DBG (3, "sane_init: Artec driver version %d.%d\n",
       ARTEC_MAJOR, ARTEC_MINOR);

  DBG_INIT ();

  if (version_code)
    *version_code = SANE_VERSION_CODE (V_MAJOR, V_MINOR, 0);

  fp = sanei_config_open (ARTEC_CONFIG_FILE);
  if (!fp)
    {
      /* default to /dev/scanner instead of insisting on config file */
      attach ("/dev/scanner", 0);
      return (SANE_STATUS_GOOD);
    }

  while (fgets (dev_name, sizeof (dev_name), fp))
    {
      if (dev_name[0] == '#')	/* ignore line comments */
	continue;
      len = strlen (dev_name);
      if (dev_name[len - 1] == '\n')
	dev_name[--len] = '\0';

      if (!len)
	continue;		/* ignore empty lines */

      sanei_config_attach_matching_devices (dev_name, attach_one);
    }
  fclose (fp);

  return (SANE_STATUS_GOOD);
}

void
sane_exit (void)
{
  ARTEC_Device *dev, *next;

  DBG (3, "sane_exit\n");

  for (dev = first_dev; dev; dev = next)
    {
      next = dev->next;
      free ((void *) dev->sane.name);
      free ((void *) dev->sane.model);
      free (dev);
    }
}

SANE_Status
sane_get_devices (const SANE_Device *** device_list, SANE_Bool local_only)
{
  static const SANE_Device **devlist = 0;
  ARTEC_Device *dev;
  int i;

  DBG (3, "sane_get_devices\n");

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

  return (SANE_STATUS_GOOD);
}

SANE_Status
sane_open (SANE_String_Const devicename, SANE_Handle * handle)
{
  SANE_Status status;
  ARTEC_Device *dev;
  ARTEC_Scanner *s;

  DBG (3, "sane_open\n");

  if (devicename[0])
    {
      for (dev = first_dev; dev; dev = dev->next)
	if (strcmp (dev->sane.name, devicename) == 0)
	  break;

      if (!dev)
	{
	  status = attach (devicename, &dev);
	  if (status != SANE_STATUS_GOOD)
	    return (status);
	}
    }
  else
    {
      /* empty devicname -> use first device */
      dev = first_dev;
    }

  if (!dev)
    return SANE_STATUS_INVAL;

  s = malloc (sizeof (*s));
  if (!s)
    return SANE_STATUS_NO_MEM;
  memset (s, 0, sizeof (*s));
  s->fd = -1;
  s->hw = dev;

  init_options (s);

  /* insert newly opened handle into list of open handles: */
  s->next = first_handle;
  first_handle = s;

  *handle = s;

  return (SANE_STATUS_GOOD);
}

void
sane_close (SANE_Handle handle)
{
  ARTEC_Scanner *prev, *s;

  DBG (3, "sane_close\n");

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
      DBG (1, "close: invalid handle %p\n", handle);
      return;			/* oops, not a handle we know about */
    }

  if (s->scanning)
    do_cancel (handle);


  if (prev)
    prev->next = s->next;
  else
    first_handle = s;

  free (handle);
}

const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
  ARTEC_Scanner *s = handle;

  if ((unsigned) option >= NUM_OPTIONS)
    return (0);

  return (s->opt + option);
}

SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option,
		     SANE_Action action, void *val, SANE_Int * info)
{
  ARTEC_Scanner *s = handle;
  SANE_Status status;
  SANE_Word cap;

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
      DBG (10, "sane_control_option %d, get value\n", option);

      switch (option)
	{
	  /* word options: */
	case OPT_X_RESOLUTION:
	case OPT_Y_RESOLUTION:
	case OPT_PREVIEW:
	case OPT_GRAY_PREVIEW:
	case OPT_RESOLUTION_BIND:
	case OPT_NEGATIVE:
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
	case OPT_NUM_OPTS:
	case OPT_QUALITY_CAL:
	case OPT_CONTRAST:
	case OPT_THRESHOLD:
	  *(SANE_Word *) val = s->val[option].w;
	  return (SANE_STATUS_GOOD);

	  /* string options: */
	case OPT_MODE:
	case OPT_FILTER_TYPE:
	case OPT_HALFTONE_PATTERN:
	  strcpy (val, s->val[option].s);
	  return (SANE_STATUS_GOOD);
	}
    }
  else if (action == SANE_ACTION_SET_VALUE)
    {
      DBG (10, "sane_control_option %d, set value\n", option);

      if (!SANE_OPTION_IS_SETTABLE (cap))
	return (SANE_STATUS_INVAL);

      status = sanei_constrain_value (s->opt + option, val, info);
      if (status != SANE_STATUS_GOOD)
	return (status);

      switch (option)
	{
	  /* (mostly) side-effect-free word options: */
	case OPT_X_RESOLUTION:
	case OPT_Y_RESOLUTION:
	case OPT_BR_X:
	case OPT_BR_Y:
	case OPT_TL_X:
	case OPT_TL_Y:
	  if (info && s->val[option].w != *(SANE_Word *) val)
	    *info |= SANE_INFO_RELOAD_PARAMS;

#ifdef PREFER_PIXEL_MODE
	  /* check to make sure we don't exceed bounds of scanner */
	  if (((s->val[OPT_BR_X].w / s->val[OPT_X_RESOLUTION].w) >
	       s->hw->width) ||
	      ((s->val[OPT_BR_Y].w / s->val[OPT_Y_RESOLUTION].w) >
	       s->hw->height))
	    {
	      DBG (5,
		   "scan size * res exceeds cap., x %d, y %d, res %d\n",
		   s->val[OPT_BR_X].w, s->val[OPT_BR_Y].w,
		   s->val[OPT_RESOLUTION].w);

	      if ((s->val[OPT_BR_X].w / s->val[OPT_X_RESOLUTION].w) >
		  s->hw->width)
		{
		  s->val[OPT_BR_X].w = s->val[OPT_X_RESOLUTION].w *
		    s->hw->width;
		}

	      if ((s->val[OPT_BR_Y].w / s->val[OPT_Y_RESOLUTION].w) >
		  s->hw->height)
		{
		  s->val[OPT_BR_Y].w = s->val[OPT_Y_RESOLUTION].w *
		    s->hw->height;
		}

	      return (SANE_STATUS_INVAL);
	    }
#endif

	  /* fall through */
	case OPT_PREVIEW:
	case OPT_GRAY_PREVIEW:
	case OPT_QUALITY_CAL:
	case OPT_NUM_OPTS:
	case OPT_NEGATIVE:
	case OPT_CONTRAST:
	case OPT_THRESHOLD:
	  s->val[option].w = *(SANE_Word *) val;
	  return (SANE_STATUS_GOOD);

	case OPT_MODE:
	case OPT_FILTER_TYPE:
	case OPT_HALFTONE_PATTERN:
	  if (s->val[option].s)
	    free (s->val[option].s);
	  s->val[option].s = strdup (val);
	  return (SANE_STATUS_GOOD);

	case OPT_RESOLUTION_BIND:
	  if (s->val[option].w != *(SANE_Word *) val)
	    {
	      s->val[option].w = *(SANE_Word *) val;

	      if (info)
		{
		  *info |= SANE_INFO_RELOAD_OPTIONS;
		}

	      if (s->val[option].w == SANE_FALSE)
		{		/* don't bind */
		  s->opt[OPT_Y_RESOLUTION].cap &= ~SANE_CAP_INACTIVE;
		  s->opt[OPT_X_RESOLUTION].title =
		    SANE_TITLE_SCAN_X_RESOLUTION;
		  s->opt[OPT_X_RESOLUTION].name =
		    SANE_NAME_SCAN_X_RESOLUTION;
		  s->opt[OPT_X_RESOLUTION].desc =
		    SANE_DESC_SCAN_X_RESOLUTION;
		}
	      else
		{		/* bind */
		  s->opt[OPT_Y_RESOLUTION].cap |= SANE_CAP_INACTIVE;
		  s->opt[OPT_X_RESOLUTION].title =
		    SANE_TITLE_SCAN_RESOLUTION;
		  s->opt[OPT_X_RESOLUTION].name =
		    SANE_NAME_SCAN_RESOLUTION;
		  s->opt[OPT_X_RESOLUTION].desc =
		    SANE_DESC_SCAN_RESOLUTION;
		}
	    }
	  return (SANE_STATUS_GOOD);

	}
    }

  return (SANE_STATUS_INVAL);
}

SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters * params)
{
  ARTEC_Scanner *s = handle;

  if (!s->scanning)
    {
      double width, height;

      memset (&s->params, 0, sizeof (s->params));

      s->x_resolution = s->val[OPT_X_RESOLUTION].w;
      s->y_resolution = s->val[OPT_Y_RESOLUTION].w;

      if ((s->val[OPT_RESOLUTION_BIND].w == SANE_TRUE) ||
	  (s->val[OPT_PREVIEW].w == SANE_TRUE))
	{
	  s->y_resolution = s->x_resolution;
	}

#ifdef PREFER_PIXEL_MODE
      s->tl_x = s->val[OPT_TL_X].w;
      s->tl_y = s->val[OPT_TL_Y].w;
      width = s->val[OPT_BR_X].w - s->val[OPT_TL_X].w;
      height = s->val[OPT_BR_Y].w - s->val[OPT_TL_Y].w;
#else
      s->tl_x = SANE_UNFIX (s->val[OPT_TL_X].w) / MM_PER_INCH
	* s->x_resolution;
      s->tl_y = SANE_UNFIX (s->val[OPT_TL_Y].w) / MM_PER_INCH
	* s->y_resolution;
      width = SANE_UNFIX (s->val[OPT_BR_X].w - s->val[OPT_TL_X].w);
      height = SANE_UNFIX (s->val[OPT_BR_Y].w - s->val[OPT_TL_Y].w);
#endif

      if ((s->x_resolution > 0.0) &&
	  (s->y_resolution > 0.0) &&
	  (width > 0.0) &&
	  (height > 0.0))
	{
#ifdef PREFER_PIXEL_MODE
	  s->params.pixels_per_line = width;
	  s->params.lines = height;
#else
	  s->params.pixels_per_line = width * s->x_resolution / MM_PER_INCH + 1;
	  s->params.lines = height * s->y_resolution / MM_PER_INCH + 1;
#endif
	}

      if ((s->val[OPT_PREVIEW].w == SANE_TRUE) &&
	  (s->val[OPT_GRAY_PREVIEW].w == SANE_TRUE))
	{
	  s->mode = "Gray";
	}
      else
	{
	  s->mode = s->val[OPT_MODE].s;
	}

      if ((strcmp (s->mode, "Lineart") == 0) ||
	  (strcmp (s->mode, "Halftone") == 0))
	{
	  s->params.format = SANE_FRAME_GRAY;
	  s->params.bytes_per_line = (s->params.pixels_per_line + 7) / 8;
	  s->params.depth = 1;
	  s->line_offset = 0;
	}
      else if (strcmp (s->mode, "Gray") == 0)
	{
	  s->params.format = SANE_FRAME_GRAY;
	  s->params.bytes_per_line = s->params.pixels_per_line;
	  s->params.depth = 8;
	  s->line_offset = 0;
	}
      else
	{
	  /* XXX assume single pass--are there any one-pass ARTECs? */
	  s->params.format = SANE_FRAME_RGB;
	  s->params.bytes_per_line = 3 * s->params.pixels_per_line;
	  s->params.depth = 8;

	  switch (s->y_resolution)
	    {
	    case 600:
	      s->line_offset = 16;
	      break;
	    case 300:
	      s->line_offset = 8;
	      break;
	    case 200:
	      s->line_offset = 5;
	      break;
	    case 100:
	      s->line_offset = 2;
	      break;
	    case 50:
	      s->line_offset = 1;
	      break;
	    default:
	      s->line_offset = 8 *
		(int) (s->y_resolution / 300.0);
	      break;
	    }
	}
      s->params.last_frame = SANE_TRUE;
    }
  if (params)
    *params = s->params;

  return (SANE_STATUS_GOOD);
}

SANE_Status
sane_start (SANE_Handle handle)
{
  ARTEC_Scanner *s = handle;
  SANE_Status status;

  DBG (3, "sane_start\n");

  /* First make sure we have a current parameter set.  Some of the
   * parameters will be overwritten below, but that's OK.  */
  status = sane_get_parameters (s, 0);
  if (status != SANE_STATUS_GOOD)
    return status;

  /* DB added sense handler (make it configurable?) */
  status = sanei_scsi_open (s->hw->sane.name, &s->fd, sense_handler, 0);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "open of %s failed: %s\n",
	   s->hw->sane.name, sane_strstatus (status));
      return status;
    }

  /* DB added wait_ready */
  status = wait_ready (s->fd);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "wait for scanner ready failed: %s\n", sane_strstatus (status));
      return status;
    }

  s->bytes_to_read = s->params.bytes_per_line * s->params.lines;

  DBG (1, "%d pixels per line, %d bytes, %d lines high, xdpi = %d, "
       "ydpi = %d, btr = %lu\n",
       s->params.pixels_per_line, s->params.bytes_per_line, s->params.lines,
       s->x_resolution, s->y_resolution, (u_long) s->bytes_to_read);

  /* do a calibrate if scanner requires/recommends it */
  if ((s->hw->req_shading_calibrate) &&
      (s->val[OPT_QUALITY_CAL].w == SANE_TRUE))
    {
      status = artec_calibrate_shading (s);

      if (status != SANE_STATUS_GOOD)
	{
	  DBG (1, "shading calibration failed: %s\n",
	       sane_strstatus (status));
	  return status;
	}
    }

  /* DB added wait_ready */
  status = wait_ready (s->fd);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "wait for scanner ready failed: %s\n", sane_strstatus (status));
      return status;
    }

  /* now set our scan window */
  status = artec_set_scan_window (s);

  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "set scan window failed: %s\n",
	   sane_strstatus (status));
      return status;
    }

  /* DB added wait_ready */
  status = wait_ready (s->fd);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "wait for scanner ready failed: %s\n", sane_strstatus (status));
      return status;
    }

  /* now we can start the actual scan */
  status = artec_start_scan (s);

  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "start scan: %s\n",
	   sane_strstatus (status));
      return status;
    }

  s->scanning = SANE_TRUE;

  return (SANE_STATUS_GOOD);
}

SANE_Status
sane_read (SANE_Handle handle, SANE_Byte * buf, SANE_Int max_len, SANE_Int * len)
{
  ARTEC_Scanner *s = handle;
  SANE_Status status;
  size_t nread;
  size_t lread;
  int bytes_read;
  int rows_read;
  int max_read_rows;
  int max_ret_rows;
  int rows_available;
  int line;
  SANE_Byte temp_buf[ARTEC_MAX_READ_SIZE];
  SANE_Byte line_buf[ARTEC_MAX_READ_SIZE];

  DBG (3, "sane_read\n");

  *len = 0;

  if (s->bytes_to_read == 0)
    {
      return (SANE_STATUS_EOF);
    }

  if (!s->scanning)
    return do_cancel (s);

  if (s->bytes_to_read < max_len)
    {
      max_len = s->bytes_to_read;
    }

  max_read_rows = ARTEC_MAX_READ_SIZE / s->params.bytes_per_line;
  max_ret_rows = max_len / s->params.bytes_per_line;

  while (artec_get_status (s->fd) == 0)
    {
      DBG (3, "hokey loop till data available\n");
      usleep (50000);		/* sleep for .05 second */
    }

  rows_read = 0;
  bytes_read = 0;
  while (rows_read < max_ret_rows)
    {
      while ((rows_available = artec_get_status (s->fd)) == 0)
	{
	  DBG (3, "hokey loop till data available\n");
	  usleep (50000);	/* sleep for .05 second */
	}

      if (s->bytes_to_read <= s->params.bytes_per_line * max_read_rows)
	{
	  nread = s->bytes_to_read;
	}
      else
	{
	  nread = s->params.bytes_per_line * max_read_rows;
	}

      lread = nread / s->params.bytes_per_line;

      if ((max_ret_rows - rows_read) < lread)
	{
	  lread = max_ret_rows - rows_read;
	  nread = lread * s->params.bytes_per_line;
	}

      /* DB check if done reading (lineart and halftone only?)
       * the number of pre-computed bytes_to_read is not accurate
       * so follow the number of available rows returned from the scanner
       * the last time it is set (AT12) to the maximum again
       * Reason is when asking for a read with a number of bytes less than a line
       * you get a timeout on the read command with the AT12
       */
      DBG (5, "rows_available=%d,params.lines=%d,btr=%lu,bpl=%d\n",
	   rows_available, s->params.lines,
	   (u_long) s->bytes_to_read, s->params.bytes_per_line);
      /* kludge for the ??AT12 model?? */
      /* this breaks the AT3 & A6000C+ if turned on for them */
      if ((!strcmp (s->hw->sane.model, "AT12")) &&
	  ((strcmp (s->mode, "Lineart") == 0) ||
	   (strcmp (s->mode, "Halftone") == 0)))
	{
	  /* DB byte alignment for the AT12 */
	  nread = (8192 / s->params.bytes_per_line) * s->params.bytes_per_line;
	  if ((rows_available == s->params.lines) &&
	   s->bytes_to_read != (s->params.lines * s->params.bytes_per_line))
	    {
	      s->bytes_to_read = 0;
	      end_scan (s);
	      do_cancel (s);
	      return (SANE_STATUS_EOF);
	    }
	}

      DBG (5, "bytes_to_read = %lu, max_len = %d, max_rows = %d, "
	   "rows_avail = %d\n",
	   (u_long) s->bytes_to_read, max_len, max_ret_rows, rows_available);
      DBG (5, "nread = %lu, lread = %lu, bytes_read = %d, rows_read = %d\n",
	   (u_long) nread, (u_long) lread, bytes_read, rows_read);

      status = read_data (s->fd, ARTEC_DATA_IMAGE, temp_buf, &nread);
      if (status != SANE_STATUS_GOOD)
	{
	  end_scan (s);
	  do_cancel (s);
	  return (SANE_STATUS_IO_ERROR);
	}

      if ((strcmp (s->mode, "Color") == 0) &&
	  (s->hw->req_rgb_line_offset))
	{
	  for (line = 0; line < lread; line++)
	    {
	      memcpy (line_buf,
		      temp_buf + (line * s->params.bytes_per_line),
		      s->params.bytes_per_line);

	      nread = s->params.bytes_per_line;
	      artec_buffer_line_offset (s->line_offset, line_buf, &nread);

	      if (nread > 0)
		{
		  if (s->hw->req_rgb_char_shift)
		    {
		      artec_line_rgb_to_byte_rgb (line_buf,
						  s->params.pixels_per_line);
		    }
		  memcpy (buf + bytes_read, line_buf,
			  s->params.bytes_per_line);
		  bytes_read += nread;
		  rows_read++;
		}
	    }
	}
      else
	{
	  memcpy (buf + bytes_read, temp_buf, nread);
	  bytes_read += nread;
	  rows_read += lread;
	}
    }

  *len = bytes_read;
  s->bytes_to_read -= bytes_read;

  DBG (5, "sane_read() returning, we read %d bytes, %lu left\n",
       *len, (u_long) s->bytes_to_read);

  if ((s->bytes_to_read == 0) &&
      (s->hw->req_rgb_line_offset) &&
      (tmp_line_buf != NULL))
    {
      artec_buffer_line_offset_free ();
    }

  return (SANE_STATUS_GOOD);
}

void
sane_cancel (SANE_Handle handle)
{
  /* DB deleted
     u_char abort;
   */
  ARTEC_Scanner *s = handle;

  DBG (3, "sane_cancel\n");

  if (s->scanning)
    {
      s->scanning = SANE_FALSE;

      /* DB changed 
         abort = 0x6;

         sanei_scsi_cmd( s->fd, &abort, 1, NULL, NULL );
       */
      abort_scan (s);

      do_cancel (s);
    }
}

SANE_Status
sane_set_io_mode (SANE_Handle handle, SANE_Bool non_blocking)
{
  DBG (3, "sane_set_io_mode\n");

  return (SANE_STATUS_UNSUPPORTED);
}

SANE_Status
sane_get_select_fd (SANE_Handle handle, SANE_Int * fd)
{
  DBG (3, "sane_get_select_fd\n");

  return (SANE_STATUS_UNSUPPORTED);
}
