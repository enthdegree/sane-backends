/* sane - Scanner Access Now Easy.

   Copyright (C) 1998, 1999
   Kazuya Fukuda, Abel Deuring based on BYTEC GmbH Germany
   Written by Helmut Koeberle previous Work on canon.c file from the 
   SANE package.

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

   This file implements a SANE backend for Sharp flatbed scanners.  */

#include <sane/config.h>

#include <limits.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include <sane/sane.h>
#include <sane/saneopts.h>
#include <sane/sanei_scsi.h>
#include <sharp.h>

#define BACKEND_NAME sharp
#include <sane/sanei_backend.h>

#ifndef PATH_MAX
#define PATH_MAX	1024
#endif

#define DEFAULT_MUD_JX610 25
#define DEFAULT_MUD_JX330 1200
#define DEFAULT_MUD_JX250 1200

#define PIX_TO_MM(x, mud) ((x) * 25.4 / mud)
#define MM_TO_PIX(x, mud) ((x) * mud / 25.4)

#include <sane/sanei_config.h>
#define SHARP_CONFIG_FILE "sharp.conf"

static int num_devices = 0;
static SHARP_Device *first_dev = NULL;
static SHARP_Scanner *first_handle = NULL;

#define SCSI_BUFF_SIZE sanei_scsi_max_request_size

typedef enum
  {
    MODES_LINEART  = 0,
    MODES_GRAY,
    MODES_LINEART_COLOR,
    MODES_COLOR
  }
Modes;

#define M_LINEART            "Lineart"
#define M_GRAY               "Gray"
#define M_LINEART_COLOR      "Lineart Color"
#define M_COLOR              "Color"
static const SANE_String_Const mode_list[] =
{
  M_LINEART, M_GRAY, M_LINEART_COLOR, M_COLOR,
  0
};

#define M_BILEVEL        "none"
#define M_BAYER          "Dither Bayer"
#define M_SPIRAL         "Dither Spiral"
#define M_DISPERSED      "Dither Dispersed"
#define M_ERRDIFFUSION   "Error Diffusion"

static const SANE_String_Const halftone_list[] =
{
  M_BILEVEL, M_BAYER, M_SPIRAL, M_DISPERSED, M_ERRDIFFUSION,
  0
};

#define LIGHT_GREEN "green"
#define LIGHT_RED   "red"
#define LIGHT_BLUE  "blue"
#define LIGHT_WHITE "white"

static const SANE_String_Const light_color_list[] =
{
  LIGHT_GREEN, LIGHT_RED, LIGHT_BLUE, LIGHT_WHITE,
  0
};

#define PAPER_MAX  10
#define W_LETTER  "11\"x17\""
#define INVOICE   "8.5\"x5.5\""
static const SANE_String_Const paper_list_jx610[] =
{
  "A3", "A4", "A5", "A6", "B4", "B5",
  W_LETTER, "Legal", "Letter", INVOICE,
  0
};

static const SANE_String_Const paper_list_jx330[] =
{
  "A4", "A5", "A6", "B5",
  0
};

#define GAMMA10    "1.0"
#define GAMMA22    "2.2"

static const SANE_String_Const gamma_list[] =
{
  GAMMA10, GAMMA22,
  0
};

#define SPEED_NORMAL    "Normal"
#define SPEED_FAST      "Fast"
static const SANE_String_Const speed_list[] =
{
  SPEED_NORMAL, SPEED_FAST,
  0
};

#define RESOLUTION_MAX_JX610 8
static const SANE_String_Const resolution_list_jx610[] =
{
  "50", "75", "100", "150", "200", "300", "400", "600", "Select",
  0
};

#define RESOLUTION_MAX_JX250 7
static const SANE_String_Const resolution_list_jx250[] =
{
  "50", "75", "100", "150", "200", "300", "400", "Select",
  0
};

#define EDGE_NONE    "None"
#define EDGE_MIDDLE  "Middle"
#define EDGE_STRONG  "Strong"
#define EDGE_BLUR    "Blur"
static const SANE_String_Const edge_emphasis_list[] =
{
  EDGE_NONE, EDGE_MIDDLE, EDGE_STRONG, EDGE_BLUR,
  0
};

#ifdef USE_CUSTOM_GAMMA
static const SANE_Range u8_range =
  {
      0,				/* minimum */
    255,				/* maximum */
      0				/* quantization */
  };
#endif
static SANE_Status
test_unit_ready (int fd)
{
  static u_char cmd[] = {TEST_UNIT_READY, 0, 0, 0, 0, 0};
  SANE_Status status;
  DBG (11, "<< test_unit_ready ");

  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), 0, 0);

  DBG (11, ">>\n");
  return (status);
}

#if 0
static SANE_Status
request_sense (int fd, void *sense_buf, size_t *sense_size)
{
  static u_char cmd[] = {REQUEST_SENSE, 0, 0, 0, SENSE_LEN, 0};
  SANE_Status status;
  DBG (11, "<< request_sense ");

  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), sense_buf, sense_size);

  DBG (11, ">>\n");
  return (status);
}
#endif

static SANE_Status
inquiry (int fd, void *inq_buf, size_t *inq_size)
{
  static u_char cmd[] = {INQUIRY, 0, 0, 0, INQUIRY_LEN, 0};
  SANE_Status status;
  DBG (11, "<< inquiry ");

  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), inq_buf, inq_size);

  DBG (11, ">>\n");
  return (status);
}

static SANE_Status
mode_select6 (int fd, int mud)
{
  static u_char cmd[6 + MODEPARAM_LEN] = 
                        {MODE_SELECT6, 0x10, 0, 0, MODEPARAM_LEN, 0};
  mode_select_param *mp;
  SANE_Status status;
  DBG (11, "<< mode_select6 ");

  mp = (mode_select_param *)(cmd + 6);
  memset (mp, 0, MODEPARAM_LEN);
  mp->page_code = 3;
  mp->constant6 = 6;
  mp->mud[0] = mud >> 8;
  mp->mud[1] = mud & 0xFF;

  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), 0, 0);

  DBG (11, ">>\n");
  return (status);
}

#if 0
static SANE_Status
reserve_unit (int fd)
{
  static u_char cmd[] = {RESERVE_UNIT, 0, 0, 0, 0, 0};
  SANE_Status status;
  DBG (11, "<< reserve_unit ");

  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), 0, 0);

  DBG (11, ">>\n");
  return (status);
}
#endif

#if 0
static SANE_Status
release_unit (int fd)
{
  static u_char cmd[] = {RELEASE_UNIT, 0, 0, 0, 0, 0};
  SANE_Status status;
  DBG (11, "<< release_unit ");

  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), 0, 0);

  DBG (11, ">>\n");
  return (status);
}
#endif

static SANE_Status
mode_sense6 (int fd, void *modeparam_buf, size_t * modeparam_size)
{
  static u_char cmd[6];
  SANE_Status status;
  DBG (11, "<< mode_sense6 ");

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = 0x1a;
  cmd[2] = 3;
  cmd[4] = 20;
  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), modeparam_buf, 
			   modeparam_size);

  DBG (11, ">>\n");
  return (status);
}

static SANE_Status
scan (int fd)
{
  static u_char cmd[] = {SCAN, 0, 0, 0, 0, 0};
  SANE_Status status;
  DBG (11, "<< scan ");

  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), 0, 0);

  DBG (11, ">>\n");
  return (status);
}

#if 0
static SANE_Status
send_diagnostics (int fd)
{
  static u_char cmd[] = {SEND_DIAGNOSTIC, 0x04, 0, 0, 0, 0};
  SANE_Status status;
  DBG (11, "<< send_diagnostics ");

  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), 0, 0);

  DBG (11, ">>\n");
  return (status);
}
#endif

static SANE_Status
send (int fd, SHARP_Send * ss)
{
  static u_char cmd[] = {SEND, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  SANE_Status status;
  DBG (11, "<< send ");

  cmd[2] = ss->dtc;
  cmd[4] = ss->dtq >> 8;
  cmd[5] = ss->dtq;
  cmd[6] = ss->length >> 16;
  cmd[7] = ss->length >>  8;
  cmd[8] = ss->length >>  0;

  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), 0, 0);

  DBG (11, ">>\n");
  return (status);

}

static SANE_Status
set_window (int fd, window_param *wp, int len)
{
  static u_char cmd[10 + WINDOW_LEN] = 
                        {SET_WINDOW, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  window_param *winp;
  SANE_Status status;
  DBG (11, "<< set_window ");

  cmd[8] = len;
  winp = (window_param *)(cmd + 10);
  memset (winp, 0, WINDOW_LEN);
  memcpy (winp, wp, len);
  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), 0, 0);

  DBG (11, ">>\n");
  return (status);

}

static SANE_Status
get_window (int fd, void *buf, size_t * buf_size)
{

  static u_char cmd[10] = {GET_WINDOW, 0, 0, 0, 0, 0, 0, 0, WINDOW_LEN, 0};
  SANE_Status status;
  DBG (11, "<< get_window ");

  cmd[8] = *buf_size;
  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), buf, buf_size);

  DBG (11, ">>\n");
  return (status);
}

static SANE_Status
read_data (int fd, void *buf, size_t * buf_size)
{
  static u_char cmd[] = {READ, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  SANE_Status status;
  DBG (11, "<< read_data ");

  cmd[6] = *buf_size >> 16;
  cmd[7] = *buf_size >> 8;
  cmd[8] = *buf_size;
  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), buf, buf_size);

  DBG (11, ">>\n");
  return (status);
}

static size_t
max_string_size (const SANE_String_Const strings[])
{
  size_t size, max_size = 0;
  int i;
  DBG (10, "<< max_string_size ");

  for (i = 0; strings[i]; ++i)
    {
      size = strlen (strings[i]) + 1;
      if (size > max_size)
	max_size = size;
    }

  DBG (10, ">>\n");
  return max_size;
}

static SANE_Status
wait_ready(int fd)
{
  SANE_Status status;
  int retry = 0;

  while ((status = test_unit_ready (fd)) != SANE_STATUS_GOOD)
  {
    DBG (5, "wait_ready failed (%d)\n", retry);
    if (retry++ > 15){
	return SANE_STATUS_IO_ERROR;
    }
    sleep(3);
  }
  return (status);
    
}

static SANE_Status
attach (const char *devnam, SHARP_Device ** devp)
{
  SANE_Status status;
  SHARP_Device *dev;

  int fd;
  unsigned char inquiry_data[INQUIRY_LEN];
  const char *model_name;
  mode_sense_param msp;
  mode_select_param mp;
  size_t buf_size;
  DBG (10, "<< attach ");

  for (dev = first_dev; dev; dev = dev->next)
    {
      if (strcmp (dev->sane.name, devnam) == 0)
	{
	  if (devp)
	    *devp = dev;
	  return (SANE_STATUS_GOOD);
	}
    }

  DBG (3, "attach: opening %s\n", devnam);
  status = sanei_scsi_open (devnam, &fd, NULL, NULL);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "attach: open failed: %s\n", sane_strstatus (status));
      return (status);
    }

  DBG (3, "attach: sending INQUIRY\n");
  memset (inquiry_data, 0, sizeof (inquiry_data));
  buf_size = sizeof (inquiry_data);
  status = inquiry (fd, inquiry_data, &buf_size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "attach: inquiry failed: %s\n", sane_strstatus (status));
      sanei_scsi_close (fd);
      return (status);
    }

  if (inquiry_data[0] != 6
      || strncmp (inquiry_data + 8, "SHARP", 5) != 0
      || (strncmp (inquiry_data + 16, "JX610", 5) != 0
	  && strncmp (inquiry_data + 16, "JX330", 5) != 0
	  && strncmp (inquiry_data + 16, "JX250", 5) != 0
	  )
     )
    {
      DBG (1, "attach: device doesn't look like a Sharp scanner\n");
      sanei_scsi_close (fd);
      return (SANE_STATUS_INVAL);
    }

  DBG (3, "attach: sending TEST_UNIT_READY\n");
  status = test_unit_ready (fd);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "attach: test unit ready failed (%s)\n",
	   sane_strstatus (status));
      sanei_scsi_close (fd);
      return (status);
    }

  DBG (3, "attach: sending MODE SELECT\n");
  /* JX-610 probably supports only 25 MUD size */
  if (strncmp (inquiry_data + 16, "JX610", 5) == 0)
    status = mode_select6 (fd, DEFAULT_MUD_JX610);
  else
    status = mode_select6 (fd, DEFAULT_MUD_JX330); /* is JX-250 same as JX-330 ? */
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "attach: MODE_SELECT6 failed\n");
      sanei_scsi_close (fd);
      return (SANE_STATUS_INVAL);
    }

  DBG (3, "attach: sending MODE SENSE\n");
  memset (&msp, 0, sizeof (mp));
  buf_size = sizeof (msp);
  status = mode_sense6 (fd, &msp, &buf_size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "attach: MODE_SENSE6 failed\n");
      sanei_scsi_close (fd);
      return (SANE_STATUS_INVAL);
    }
#if 0
  {
  SHARP_Send ss;
  char gamma[256];
  memset (&gamma, 0, sizeof(gamma));
  ss.dtc = 0x03;
  ss.dtq = 0x00;
  ss.length = 256*3;
  ss.data = (SANE_Byte *)gamma;
  DBG (5, "start: SEND\n");
  status = send (fd,  &ss);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "send failed: %s\n", sane_strstatus (status));
      sanei_scsi_close (fd);
      return (status);
    }
  }
#endif
  sanei_scsi_close (fd);

  dev = malloc (sizeof (*dev));
  if (!dev)
    return (SANE_STATUS_NO_MEM);
  memset (dev, 0, sizeof (*dev));

  dev->sane.name = strdup (devnam);
  dev->sane.vendor = "SHARP";
  model_name = inquiry_data + 16;
  dev->sane.model  = strndup (model_name, 10);
  dev->sane.type = "flatbed scanner";

  DBG (5, "dev->sane.name = %s\n", dev->sane.name);
  DBG (5, "dev->sane.vendor = %s\n", dev->sane.vendor);
  DBG (5, "dev->sane.model = %s\n", dev->sane.model);
  DBG (5, "dev->sane.type = %s\n", dev->sane.type);

  dev->info.xres_range.quant = 0;
  dev->info.yres_range.quant = 0;
  dev->info.x_range.min = SANE_FIX(0);
  dev->info.y_range.min = SANE_FIX(0);
  dev->info.x_range.quant = SANE_FIX(0);
  dev->info.y_range.quant = SANE_FIX(0);

  dev->info.xres_default = 150;
  dev->info.yres_default = 150;
  dev->info.x_range.max = SANE_FIX(210);
  dev->info.y_range.max = SANE_FIX(297);

  dev->info.bmu = msp.bmu;
  dev->info.mud = (msp.mud[0] << 8) + msp.mud[1];

  if (strncmp (model_name, "JX610 SCSI", 10) == 0)
    {
      dev->info.xres_range.max = 600;
      dev->info.xres_range.min = 30;

      dev->info.yres_range.max = 600;
      dev->info.yres_range.min = 30;
      dev->info.x_default = SANE_FIX(210);
      dev->info.x_range.max = SANE_FIX(304); /* 304.8mm is the real max */

      dev->info.y_default = SANE_FIX(297);
      dev->info.y_range.max = SANE_FIX(431); /* 431.8 is the real max */
    }
  else if (strncmp(model_name, "JX330 SCSI", 10) == 0)
    {
      dev->info.xres_range.max = 600;
      dev->info.xres_range.min = 30;

      dev->info.yres_range.max = 600;
      dev->info.yres_range.min = 30;
      dev->info.x_default = SANE_FIX(210);
      /* I don't know if the max values of x_range and y_range
         are correct for the JX 330. The values are taken from
         the JX 250 manual - I could not find the max values in
         the JX 330 manual. 
         Abel
      */
      dev->info.x_range.max = SANE_FIX(PIX_TO_MM(10223, dev->info.mud));

      dev->info.y_default = SANE_FIX(297);
      dev->info.y_range.max = SANE_FIX(PIX_TO_MM(14063, dev->info.mud));
    }
  else if (strncmp(model_name, "JX250 SCSI", 10) == 0)
    {
      dev->info.xres_range.max = 400;
      dev->info.xres_range.min = 30;

      dev->info.yres_range.max = 400;
      dev->info.yres_range.min = 30;
      dev->info.x_default = SANE_FIX(210);
      dev->info.x_range.max = SANE_FIX(PIX_TO_MM(10223, dev->info.mud));

      dev->info.y_default = SANE_FIX(297);
      dev->info.y_range.max = SANE_FIX(PIX_TO_MM(14063, dev->info.mud));
    }
  else
    {
      dev->info.xres_range.max = 300;
      dev->info.xres_range.min = 30;

      dev->info.yres_range.max = 300;
      dev->info.yres_range.min = 30;
      dev->info.x_default = SANE_FIX(210);
      dev->info.x_range.max =SANE_FIX(210);

      dev->info.y_default = SANE_FIX(297);
      dev->info.y_range.max = SANE_FIX(297);
    }      

  dev->info.threshold_range.min = 1;
  dev->info.threshold_range.max = 255;
  dev->info.threshold_range.quant = 0;

  DBG (5, "xres_default=%d\n", dev->info.xres_default);
  DBG (5, "xres_range.max=%d\n", dev->info.xres_range.max);
  DBG (5, "xres_range.min=%d\n", dev->info.xres_range.min);
  DBG (5, "xres_range.quant=%d\n", dev->info.xres_range.quant);
  DBG (5, "yres_default=%d\n", dev->info.yres_default);
  DBG (5, "yres_range.max=%d\n", dev->info.yres_range.max);
  DBG (5, "yres_range.min=%d\n", dev->info.yres_range.min);
  DBG (5, "xres_range.quant=%d\n", dev->info.xres_range.quant);

  DBG (5, "x_default=%f\n", SANE_UNFIX(dev->info.x_default));
  DBG (5, "x_range.max=%f\n", SANE_UNFIX(dev->info.x_range.max));
  DBG (5, "x_range.min=%f\n", SANE_UNFIX(dev->info.x_range.min));
  DBG (5, "x_range.quant=%d\n", dev->info.x_range.quant);
  DBG (5, "y_default=%f\n", SANE_UNFIX(dev->info.y_default));
  DBG (5, "y_range.max=%f\n", SANE_UNFIX(dev->info.y_range.max));
  DBG (5, "y_range.min=%f\n", SANE_UNFIX(dev->info.y_range.min));
  DBG (5, "y_range.quant=%d\n", dev->info.y_range.quant);

  DBG (5, "bmu=%d\n", dev->info.bmu);
  DBG (5, "mud=%d\n", dev->info.mud);

  ++num_devices;
  dev->next = first_dev;
  first_dev = dev;

  if (devp)
    *devp = dev;

  DBG (10, ">>\n");
  return (SANE_STATUS_GOOD);
}

static SANE_Status
init_options (SHARP_Scanner * s)
{
  int i;
  DBG (10, "<< init_options ");

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

  /* Mode group: */
  s->opt[OPT_MODE_GROUP].title = "Scan Mode";
  s->opt[OPT_MODE_GROUP].desc = "";
  s->opt[OPT_MODE_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_MODE_GROUP].cap = 0;
  s->opt[OPT_MODE_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* scan mode */
  s->opt[XOPT_MODE].name = SANE_NAME_SCAN_MODE;
  s->opt[XOPT_MODE].title = SANE_TITLE_SCAN_MODE;
  s->opt[XOPT_MODE].desc = SANE_DESC_SCAN_MODE;
  s->opt[XOPT_MODE].type = SANE_TYPE_STRING;
  s->opt[XOPT_MODE].size = max_string_size (mode_list);
  s->opt[XOPT_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[XOPT_MODE].constraint.string_list = mode_list;
  s->val[XOPT_MODE].s = strdup (mode_list[3]); /* color scan */

  /* half tone */
  s->opt[OPT_HALFTONE].name = SANE_NAME_HALFTONE_PATTERN;
  s->opt[OPT_HALFTONE].title = SANE_TITLE_HALFTONE_PATTERN;
  s->opt[OPT_HALFTONE].desc = SANE_DESC_HALFTONE " (JX-330 only)";
  s->opt[OPT_HALFTONE].type = SANE_TYPE_STRING;
  s->opt[OPT_HALFTONE].size = max_string_size (halftone_list);
  s->opt[OPT_HALFTONE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_HALFTONE].constraint.string_list = halftone_list;
  s->val[OPT_HALFTONE].s = strdup (halftone_list[0]);
  if (   strcmp(s->dev->sane.model, "JX250 SCSI") == 0
      || strcmp(s->dev->sane.model, "JX610 SCSI") == 0
     )
    s->opt[OPT_HALFTONE].cap |= SANE_CAP_INACTIVE;

  s->opt[OPT_PAPER].name = "Paper size";
  s->opt[OPT_PAPER].title = "Paper size";
  s->opt[OPT_PAPER].desc = "Paper size";
  s->opt[OPT_PAPER].type = SANE_TYPE_STRING;
  if (strcmp(s->dev->sane.model, "JX610 SCSI") == 0)
    {
      s->opt[OPT_PAPER].size = max_string_size (paper_list_jx610);
      s->opt[OPT_PAPER].constraint_type = SANE_CONSTRAINT_STRING_LIST;
      s->opt[OPT_PAPER].constraint.string_list = paper_list_jx610;
      s->val[OPT_PAPER].s = strdup (paper_list_jx610[1]);
    }
  else 
    {
      s->opt[OPT_PAPER].size = max_string_size (paper_list_jx330);
      s->opt[OPT_PAPER].constraint_type = SANE_CONSTRAINT_STRING_LIST;
      s->opt[OPT_PAPER].constraint.string_list = paper_list_jx330;
      s->val[OPT_PAPER].s = strdup (paper_list_jx330[0]);
    }
  /* gamma */
  s->opt[OPT_GAMMA].name = "Gamma";
  s->opt[OPT_GAMMA].title = "Gamma";
  s->opt[OPT_GAMMA].desc = "Gamma";
  s->opt[OPT_GAMMA].type = SANE_TYPE_STRING;
  s->opt[OPT_GAMMA].size = max_string_size (gamma_list);
  s->opt[OPT_GAMMA].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_GAMMA].constraint.string_list = gamma_list;
  s->val[OPT_GAMMA].s = strdup (gamma_list[1]);

  /* scan speed */
  s->opt[OPT_SPEED].name = SANE_NAME_SCAN_SPEED;
  s->opt[OPT_SPEED].title = "Scan speed [fast]";
  s->opt[OPT_SPEED].desc = SANE_DESC_SCAN_SPEED;
  s->opt[OPT_SPEED].type = SANE_TYPE_BOOL;
  s->val[OPT_SPEED].w = SANE_TRUE;

  /* Resolution Group */
  s->opt[OPT_RESOLUTION_GROUP].title = "Resolution";
  s->opt[OPT_RESOLUTION_GROUP].desc = "";
  s->opt[OPT_RESOLUTION_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_RESOLUTION_GROUP].cap = 0;
  s->opt[OPT_RESOLUTION_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* select resolution */
  s->opt[OPT_RESOLUTION].name = "Resolution";
  s->opt[OPT_RESOLUTION].title = "Resolution";
  s->opt[OPT_RESOLUTION].desc = "Resolution";
  s->opt[OPT_RESOLUTION].type = SANE_TYPE_STRING;
  if (   strcmp(s->dev->sane.model, "JX610 SCSI") == 0
      || strcmp(s->dev->sane.model, "JX330 SCSI") == 0)
    {
      s->opt[OPT_RESOLUTION].size = max_string_size (resolution_list_jx610);
      s->opt[OPT_RESOLUTION].constraint_type = SANE_CONSTRAINT_STRING_LIST;
      s->opt[OPT_RESOLUTION].constraint.string_list = resolution_list_jx610;
      s->val[OPT_RESOLUTION].s 
        = strdup (resolution_list_jx610[RESOLUTION_MAX_JX610]);
    }
  else /* must be a JX 250 */
    {
      s->opt[OPT_RESOLUTION].size = max_string_size (resolution_list_jx250);
      s->opt[OPT_RESOLUTION].constraint_type = SANE_CONSTRAINT_STRING_LIST;
      s->opt[OPT_RESOLUTION].constraint.string_list = resolution_list_jx250;
      s->val[OPT_RESOLUTION].s 
        = strdup (resolution_list_jx250[RESOLUTION_MAX_JX250]);
    }

  /* x resolution */
  s->opt[OPT_X_RESOLUTION].name = "X" SANE_NAME_SCAN_RESOLUTION;
  s->opt[OPT_X_RESOLUTION].title = "X " SANE_TITLE_SCAN_RESOLUTION;
  s->opt[OPT_X_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;
  s->opt[OPT_X_RESOLUTION].type = SANE_TYPE_INT;
  s->opt[OPT_X_RESOLUTION].unit = SANE_UNIT_DPI;
  s->opt[OPT_X_RESOLUTION].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_X_RESOLUTION].constraint.range = &s->dev->info.xres_range;
  s->val[OPT_X_RESOLUTION].w = s->dev->info.xres_default;

  /* y resolution */
  s->opt[OPT_Y_RESOLUTION].name = "Y" SANE_NAME_SCAN_RESOLUTION;
  s->opt[OPT_Y_RESOLUTION].title = "Y " SANE_TITLE_SCAN_RESOLUTION;
  s->opt[OPT_Y_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;
  s->opt[OPT_Y_RESOLUTION].type = SANE_TYPE_INT;
  s->opt[OPT_Y_RESOLUTION].unit = SANE_UNIT_DPI;
  s->opt[OPT_Y_RESOLUTION].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_Y_RESOLUTION].constraint.range = &s->dev->info.yres_range;
  s->val[OPT_Y_RESOLUTION].w = s->dev->info.yres_default;

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
  s->opt[OPT_TL_X].constraint.range = &s->dev->info.x_range;
  s->val[OPT_TL_X].w = s->dev->info.x_range.min;

  /* top-left y */
  s->opt[OPT_TL_Y].name = SANE_NAME_SCAN_TL_Y;
  s->opt[OPT_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
  s->opt[OPT_TL_Y].desc = SANE_DESC_SCAN_TL_Y;
  s->opt[OPT_TL_Y].type = SANE_TYPE_FIXED;
  s->opt[OPT_TL_Y].unit = SANE_UNIT_MM;
  s->opt[OPT_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TL_Y].constraint.range = &s->dev->info.y_range;
  s->val[OPT_TL_Y].w = s->dev->info.y_range.min;

  /* bottom-right x */
  s->opt[OPT_BR_X].name = SANE_NAME_SCAN_BR_X;
  s->opt[OPT_BR_X].title = SANE_TITLE_SCAN_BR_X;
  s->opt[OPT_BR_X].desc = SANE_DESC_SCAN_BR_X;
  s->opt[OPT_BR_X].type = SANE_TYPE_FIXED;
  s->opt[OPT_BR_X].unit = SANE_UNIT_MM;
  s->opt[OPT_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_X].constraint.range = &s->dev->info.x_range;
  s->val[OPT_BR_X].w = s->dev->info.x_default;

  /* bottom-right y */
  s->opt[OPT_BR_Y].name = SANE_NAME_SCAN_BR_Y;
  s->opt[OPT_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
  s->opt[OPT_BR_Y].desc = SANE_DESC_SCAN_BR_Y;
  s->opt[OPT_BR_Y].type = SANE_TYPE_FIXED;
  s->opt[OPT_BR_Y].unit = SANE_UNIT_MM;
  s->opt[OPT_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_Y].constraint.range = &s->dev->info.y_range;
  s->val[OPT_BR_Y].w = s->dev->info.y_default;

  /* "Enhancement" group: */
  s->opt[OPT_ENHANCEMENT_GROUP].title = "Enhancement";
  s->opt[OPT_ENHANCEMENT_GROUP].desc = "";
  s->opt[OPT_ENHANCEMENT_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_ENHANCEMENT_GROUP].cap = 0;
  s->opt[OPT_ENHANCEMENT_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* edge emphasis */
  s->opt[OPT_EDGE_EMPHASIS].name = "Edge emphasis";
  s->opt[OPT_EDGE_EMPHASIS].title = "Edge emphasis";
  s->opt[OPT_EDGE_EMPHASIS].desc = "Edge emphasis";
  s->opt[OPT_EDGE_EMPHASIS].type = SANE_TYPE_STRING;
  s->opt[OPT_EDGE_EMPHASIS].size = max_string_size (edge_emphasis_list);
  s->opt[OPT_EDGE_EMPHASIS].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_EDGE_EMPHASIS].constraint.string_list = edge_emphasis_list;
  s->val[OPT_EDGE_EMPHASIS].s = strdup (edge_emphasis_list[0]);
  if (strcmp(s->dev->sane.model, "JX250 SCSI") == 0)
    s->opt[OPT_EDGE_EMPHASIS].cap |= SANE_CAP_INACTIVE;

  /* threshold */
  s->opt[XOPT_THRESHOLD].name = SANE_NAME_THRESHOLD;
  s->opt[XOPT_THRESHOLD].title = SANE_TITLE_THRESHOLD;
  s->opt[XOPT_THRESHOLD].desc = SANE_DESC_THRESHOLD;
  s->opt[XOPT_THRESHOLD].type = SANE_TYPE_INT;
  s->opt[XOPT_THRESHOLD].unit = SANE_UNIT_NONE;
  s->opt[XOPT_THRESHOLD].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[XOPT_THRESHOLD].constraint.range = &s->dev->info.threshold_range;
  s->val[XOPT_THRESHOLD].w = 128;

  /* light color (for gray scale and line art scans) */
  s->opt[OPT_LIGHTCOLOR].name = "LightColor";
  s->opt[OPT_LIGHTCOLOR].title = "Light Color";
  s->opt[OPT_LIGHTCOLOR].desc = "Light Color";
  s->opt[OPT_LIGHTCOLOR].type = SANE_TYPE_STRING;
  s->opt[OPT_LIGHTCOLOR].size = max_string_size (light_color_list);
  s->opt[OPT_LIGHTCOLOR].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_LIGHTCOLOR].constraint.string_list = light_color_list;
  s->val[OPT_LIGHTCOLOR].s = strdup (light_color_list[3]);
  s->opt[OPT_LIGHTCOLOR].cap |= SANE_CAP_INACTIVE;

#ifdef USE_CUSTOM_GAMMA
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

#endif
  DBG (10, ">>\n");
  return SANE_STATUS_GOOD;
}

static SANE_Status
do_cancel (SHARP_Scanner * s)
{
  static u_char cmd[] = {READ, 0, 0, 0, 0, 2, 0, 0, 0, 0};
  DBG (10, "<< do_cancel ");

  if (s->scanning == SANE_TRUE)
    {
      sanei_scsi_cmd (s->fd, cmd, sizeof (cmd), 0, 0);
    }

  s->scanning = SANE_FALSE;

  if (s->fd >= 0)
    {
      sanei_scsi_close (s->fd);
      s->fd = -1;
    }

  DBG (10, ">>\n");
  return (SANE_STATUS_CANCELLED);
}

SANE_Status
sane_init (SANE_Int * version_code, SANE_Auth_Callback authorize)
{
  char devnam[PATH_MAX] = "/dev/scanner";

  DBG_INIT ();
  DBG (10, "<< sane_init ");

#if defined PACKAGE && defined VERSION
  DBG (2, "sane_init: " PACKAGE " " VERSION "\n");
#endif

  if (version_code)
    *version_code = SANE_VERSION_CODE (V_MAJOR, V_MINOR, 0);

  attach (devnam, 0);

  DBG (10, ">>\n");
  return (SANE_STATUS_GOOD);
}

void
sane_exit (void)
{
  SHARP_Device *dev, *next;
  DBG (10, "<< sane_exit ");

  for (dev = first_dev; dev; dev = next)
    {
      next = dev->next;
      free ((void *) dev->sane.name);
      free ((void *) dev->sane.model);
      free (dev);
    }

  DBG (10, ">>\n");
}

SANE_Status
sane_get_devices (const SANE_Device *** device_list, SANE_Bool local_only)
{
  static const SANE_Device **devlist = 0;
  SHARP_Device *dev;
  int i;
  DBG (10, "<< sane_get_devices ");

  if (devlist)
    free (devlist);
  devlist = malloc ((num_devices + 1) * sizeof (devlist[0]));
  if (!devlist)
    return (SANE_STATUS_NO_MEM);

  i = 0;
  for (dev = first_dev; dev; dev = dev->next)
    devlist[i++] = &dev->sane;
  devlist[i++] = 0;

  *device_list = devlist;

  DBG (10, ">>\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_open (SANE_String_Const devnam, SANE_Handle * handle)
{
  SANE_Status status;
  SHARP_Device *dev;
  SHARP_Scanner *s;
#ifdef USE_CUSTOM_GAMMA
  int i, j;
#endif

  DBG (10, "<< sane_open ");

  if (devnam[0] == '\0')
    {
      for (dev = first_dev; dev; dev = dev->next)
	{
	  if (strcmp (dev->sane.name, devnam) == 0)
	    break;
	}

      if (!dev)
	{
	  status = attach (devnam, &dev);
	  if (status != SANE_STATUS_GOOD)
	    return (status);
	}
    }
  else
    {
      dev = first_dev;
    }

  if (!dev)
    return (SANE_STATUS_INVAL);

  s = malloc (sizeof (*s));
  if (!s)
    return SANE_STATUS_NO_MEM;
  memset (s, 0, sizeof (*s));

  s->fd = -1;
  s->dev = dev;
  
  s->buffer = malloc(sanei_scsi_max_request_size);
  if (!s->buffer) {
    free(s);
    return SANE_STATUS_NO_MEM;
  }
  
#ifdef USE_CUSTOM_GAMMA
  for (i = 0; i < 4; ++i)
    for (j = 0; j < 256; ++j)
      s->gamma_table[i][j] = j;
#endif
  init_options (s);

  s->next = first_handle;
  first_handle = s;

  *handle = s;

  DBG (10, ">>\n");
  return SANE_STATUS_GOOD;
}

void
sane_close (SANE_Handle handle)
{
  SHARP_Scanner *s = (SHARP_Scanner *) handle;
  DBG (10, "<< sane_close ");

  if (s->fd != -1)
    sanei_scsi_close (s->fd);
  free(s->buffer);
  free (s);

  DBG (10, ">>\n");
}

const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
  SHARP_Scanner *s = handle;
  DBG (10, "<< sane_get_option_descriptor ");

  if ((unsigned) option >= NUM_OPTIONS)
    return (0);

  DBG (10, ">>\n");
  return (s->opt + option);
}

SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option,
		     SANE_Action action, void *val, SANE_Int * info)
{
  SHARP_Scanner *s = handle;
  SANE_Status status;
#ifdef USE_CUSTOM_GAMMA
  SANE_Word w, cap;
#else
  SANE_Word cap;
#endif
  int i;
  DBG (10, "<< sane_control_option %i", option);

  if (info)
    *info = 0;

  if (s->scanning)
    return (SANE_STATUS_DEVICE_BUSY);
  if (option >= NUM_OPTIONS)
    return (SANE_STATUS_INVAL);

  cap = s->opt[option].cap;
  if (!SANE_OPTION_IS_ACTIVE (cap))
    return (SANE_STATUS_INVAL);

  if (action == SANE_ACTION_GET_VALUE)
    {
      switch (option)
	{
	  /* word options: */
	case OPT_X_RESOLUTION:
	case OPT_Y_RESOLUTION:
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
	case OPT_NUM_OPTS:
	case XOPT_THRESHOLD:
	case OPT_SPEED:
#ifdef USE_CUSTOM_GAMMA
	case OPT_CUSTOM_GAMMA:
#endif
	  *(SANE_Word *) val = s->val[option].w;
	  if (info)
	    *info |= SANE_INFO_RELOAD_PARAMS;
	  return (SANE_STATUS_GOOD);

#ifdef USE_CUSTOM_GAMMA
	  /* word-array options: */
	case OPT_GAMMA_VECTOR:
	case OPT_GAMMA_VECTOR_R:
	case OPT_GAMMA_VECTOR_G:
	case OPT_GAMMA_VECTOR_B:
	  memcpy (val, s->val[option].wa, s->opt[option].size);
	  return SANE_STATUS_GOOD;
#endif

	  /* string options: */
	case XOPT_MODE:
	case OPT_HALFTONE:
	case OPT_PAPER:
	case OPT_GAMMA:
	case OPT_RESOLUTION:
	case OPT_EDGE_EMPHASIS:
	case OPT_LIGHTCOLOR:
	  strcpy (val, s->val[option].s);
	  if (info)
	    *info |= SANE_INFO_RELOAD_PARAMS;
	  return (SANE_STATUS_GOOD);

	}
    }
  else if (action == SANE_ACTION_SET_VALUE)
    {
      if (!SANE_OPTION_IS_SETTABLE (cap))
	return (SANE_STATUS_INVAL);

      status = sanei_constrain_value (s->opt + option, val, info);
      if (status != SANE_STATUS_GOOD)
	return status;

      switch (option)
	{
	  /* (mostly) side-effect-free word options: */
	case OPT_X_RESOLUTION:
	case OPT_Y_RESOLUTION:
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
	case OPT_NUM_OPTS:
	case XOPT_THRESHOLD:
	case OPT_SPEED:
	  if (info && s->val[option].w != *(SANE_Word *) val)
	    *info |= SANE_INFO_RELOAD_PARAMS;
	  s->val[option].w = *(SANE_Word *) val;
	  return (SANE_STATUS_GOOD);

	case XOPT_MODE:
	  if (   strcmp (val, M_LINEART) == 0 
	      || strcmp (val, M_LINEART_COLOR) == 0)
	    {
	      s->opt[XOPT_THRESHOLD].cap &= ~SANE_CAP_INACTIVE;
	      if (strcmp(s->dev->sane.model, "JX330 SCSI") == 0)
                s->opt[OPT_HALFTONE].cap &= ~SANE_CAP_INACTIVE;
	    }
	  else
	    {
	      s->opt[XOPT_THRESHOLD].cap |= SANE_CAP_INACTIVE;
              s->opt[OPT_HALFTONE].cap |= SANE_CAP_INACTIVE;
            }
          
	  if (   strcmp (val, M_LINEART) == 0 
	      || strcmp (val, M_GRAY) == 0)
            {
	      s->opt[OPT_LIGHTCOLOR].cap &= ~SANE_CAP_INACTIVE;
            }
          else
            {
	      s->opt[OPT_LIGHTCOLOR].cap |= SANE_CAP_INACTIVE;
            }
          
	  *info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;
	case OPT_GAMMA:
	case OPT_HALFTONE:
	case OPT_EDGE_EMPHASIS:
	case OPT_LIGHTCOLOR:
	  if (info && strcmp (s->val[option].s, (SANE_String) val))
	    *info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;
	  if (s->val[option].s)
	    free (s->val[option].s);
	  s->val[option].s = strdup (val);
	  return (SANE_STATUS_GOOD);

	case OPT_PAPER:
	  *info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;
	  if (s->val[option].s)
	    free (s->val[option].s);
	  s->val[option].s = strdup (val);
	  s->val[OPT_TL_X].w = SANE_FIX(0);
	  s->val[OPT_TL_Y].w = SANE_FIX(0);
	  if (strcmp (s->val[option].s, "A3") == 0){
	      s->val[OPT_BR_X].w = SANE_FIX(297);
	      s->val[OPT_BR_Y].w = SANE_FIX(420);
	      *info |= SANE_INFO_RELOAD_PARAMS;
	  }else if (strcmp (s->val[option].s, "A4") == 0){
	      s->val[OPT_BR_X].w = SANE_FIX(210);
	      s->val[OPT_BR_Y].w = SANE_FIX(297);
	      *info |= SANE_INFO_RELOAD_PARAMS;
	  }else if (strcmp (s->val[option].s, "A5") == 0){
	      s->val[OPT_BR_X].w = SANE_FIX(148.5);
	      s->val[OPT_BR_Y].w = SANE_FIX(210);
	      *info |= SANE_INFO_RELOAD_PARAMS;
	  }else if (strcmp (s->val[option].s, "A6") == 0){
	      s->val[OPT_BR_X].w = SANE_FIX(105);
	      s->val[OPT_BR_Y].w = SANE_FIX(148.5);
	      *info |= SANE_INFO_RELOAD_PARAMS;
	  }else if (strcmp (s->val[option].s, "B4") == 0){
	      s->val[OPT_BR_X].w = SANE_FIX(250);
	      s->val[OPT_BR_Y].w = SANE_FIX(353);
	      *info |= SANE_INFO_RELOAD_PARAMS;
	  }else if (strcmp (s->val[option].s, "B5") == 0){
	      s->val[OPT_BR_X].w = SANE_FIX(182);
	      s->val[OPT_BR_Y].w = SANE_FIX(257);
	      *info |= SANE_INFO_RELOAD_PARAMS;
	  }else if (strcmp (s->val[option].s, W_LETTER) == 0){
	      s->val[OPT_BR_X].w = SANE_FIX(279.4);
	      s->val[OPT_BR_Y].w = SANE_FIX(431.8);
	      *info |= SANE_INFO_RELOAD_PARAMS;
	  }else if (strcmp (s->val[option].s, "Legal") == 0){
	      s->val[OPT_BR_X].w = SANE_FIX(215.9);
	      s->val[OPT_BR_Y].w = SANE_FIX(355.6);
	      *info |= SANE_INFO_RELOAD_PARAMS;
	  }else if (strcmp (s->val[option].s, "Letter") == 0){
	      s->val[OPT_BR_X].w = SANE_FIX(215.9);
	      s->val[OPT_BR_Y].w = SANE_FIX(279.4);
	      *info |= SANE_INFO_RELOAD_PARAMS;
	  }else if (strcmp (s->val[option].s, INVOICE) == 0){
	      s->val[OPT_BR_X].w = SANE_FIX(215.9);
	      s->val[OPT_BR_Y].w = SANE_FIX(139.7);
	      *info |= SANE_INFO_RELOAD_PARAMS;
	  }else{
	  }
	  return (SANE_STATUS_GOOD);

	case OPT_RESOLUTION:
	  *info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;
	  if (s->val[option].s)
	    free (s->val[option].s);
	  s->val[option].s = strdup (val);

	  for (i = 0; s->opt[OPT_RESOLUTION].constraint.string_list[i]; i++) {
	    if (strcmp (s->val[option].s, 
	          s->opt[OPT_RESOLUTION].constraint.string_list[i]) == 0){
	      s->val[OPT_X_RESOLUTION].w 
	        = atoi(s->opt[OPT_RESOLUTION].constraint.string_list[i]);
	      s->val[OPT_Y_RESOLUTION].w 
	        = atoi(s->opt[OPT_RESOLUTION].constraint.string_list[i]);
	      *info |= SANE_INFO_RELOAD_PARAMS;
	      break;
	    }
	  }
	  if (s->val[option].s)
	    free (s->val[option].s);
	  s->val[option].s 
	    = strdup (resolution_list_jx610[RESOLUTION_MAX_JX610]);
	  return (SANE_STATUS_GOOD);
#ifdef USE_CUSTOM_GAMMA
	  /* side-effect-free word-array options: */
	case OPT_GAMMA_VECTOR:
	case OPT_GAMMA_VECTOR_R:
	case OPT_GAMMA_VECTOR_G:
	case OPT_GAMMA_VECTOR_B:
	  memcpy (s->val[option].wa, val, s->opt[option].size);
	  return SANE_STATUS_GOOD;

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
#endif
	}
    }

  DBG (10, ">>\n");
  return (SANE_STATUS_INVAL);
}

SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters * params)
{
  int width, length, xres, yres;
  const char *mode;
  SHARP_Scanner *s = handle;
  DBG (10, "<< sane_get_parameters ");

  xres = s->val[OPT_X_RESOLUTION].w;
  yres = s->val[OPT_Y_RESOLUTION].w;
  if (!s->scanning)
    {
      /* make best-effort guess at what parameters will look like once
         scanning starts.  */
      memset (&s->params, 0, sizeof (s->params));

      width = MM_TO_PIX(  SANE_UNFIX(s->val[OPT_BR_X].w) 
                        - SANE_UNFIX(s->val[OPT_TL_X].w),
			s->dev->info.mud);
      length = MM_TO_PIX(  SANE_UNFIX(s->val[OPT_BR_Y].w)
                          - SANE_UNFIX(s->val[OPT_TL_Y].w),
			 s->dev->info.mud);
#if 0
      /* the JX 610 works only with scan window withs and lengths which
         are integral multiples of 1/25 inch.
         make sure that the window size sent with set_window
         is at least as large as chosen by the user but does not
         exceed the maximum scan size of 300/25 inch * 425/25 inch 
      */
      if (strcmp(s->dev->sane.model, "JX610 SCSI") == 0)
        {
          width = (width + 47) / 48;
          if (width > 300) 
            width = 300;
          width *= 25;
          length = (length + 47) / 48;
          if (length > 425) 
            length = 425;
          length *= 25;
        }
#endif
      s->width = width;
      s->length = length;
      s->params.pixels_per_line = width * xres / s->dev->info.mud;
      s->params.lines = length * yres / s->dev->info.mud;
      s->unscanned_lines = s->params.lines;
    }
  else
    {
      static u_char cmd[] = {READ, 0, 0x81, 0, 0, 0, 0, 0, 4, 0};
      static u_char buf[4];
      size_t len = 4;
      SANE_Status status;

      status = sanei_scsi_cmd (s->fd, cmd, sizeof (cmd), buf, &len);
      
      if (status != SANE_STATUS_GOOD)
        {
          do_cancel(s);
          return (status);
        }
      s->params.pixels_per_line = (buf[1] << 8) + buf[0];
      s->params.lines = (buf[3] << 8) + buf[2];
    }

  xres = s->val[OPT_X_RESOLUTION].w;
  yres = s->val[OPT_Y_RESOLUTION].w;

  mode = s->val[XOPT_MODE].s;

  if (strcmp (mode, M_LINEART) == 0)
     {
       s->params.format = SANE_FRAME_GRAY;
       s->params.bytes_per_line = (s->params.pixels_per_line + 7) / 8;
       s->params.depth = 1;
       s->modes = MODES_LINEART;
     }
  else if (strcmp (mode, M_GRAY) == 0)
     {
       s->params.format = SANE_FRAME_GRAY;
       s->params.bytes_per_line = s->params.pixels_per_line;
       s->params.depth = 8;
       s->modes = MODES_GRAY;
     }
  else if (strcmp (mode, M_LINEART_COLOR) == 0)
     {
       s->params.format = SANE_FRAME_RGB;
       s->params.bytes_per_line = 3 * ((s->params.pixels_per_line + 7) / 8);
       s->params.depth = 1;
       s->modes = MODES_LINEART_COLOR;
     }
  else
     {
       s->params.format = SANE_FRAME_RGB;
       s->params.bytes_per_line = 3 * s->params.pixels_per_line;
       s->params.depth = 8;
       s->modes = MODES_COLOR;
     }
  s->params.last_frame = SANE_TRUE;

  if (params)
    *params = s->params;

  DBG (10, ">>\n");
  return (SANE_STATUS_GOOD);
}

SANE_Status
sane_start (SANE_Handle handle)
{
  char *mode, *halftone, *paper, *gamma, *edge, *lightcolor;
  SHARP_Scanner *s = handle;
  SANE_Status status;
  size_t buf_size;
  SHARP_Send ss;
  window_param wp;

  DBG (10, "<< sane_start ");

  /* First make sure we have a current parameter set.  Some of the
     parameters will be overwritten below, but that's OK.  */
  status = sane_get_parameters (s, 0);
  if (status != SANE_STATUS_GOOD)
    return status;

  status = sanei_scsi_open (s->dev->sane.name, &s->fd, 0, 0);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "open of %s failed: %s\n",
         s->dev->sane.name, sane_strstatus (status));
      return (status);
    }

  DBG (5, "start: TEST_UNIT_READY\n");
  status = test_unit_ready (s->fd);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "TEST UNIT READY failed: %s\n", sane_strstatus (status));
      sanei_scsi_close (s->fd);
      return (status);
    }

  DBG (3, "start: sending MODE SELECT\n");
  status = mode_select6 (s->fd, s->dev->info.mud);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "start: MODE_SELECT6 failed\n");
      sanei_scsi_close (s->fd);
      return (status);
    }

  mode = s->val[XOPT_MODE].s;
  halftone = s->val[OPT_HALFTONE].s;
  paper = s->val[OPT_PAPER].s;
  gamma = s->val[OPT_GAMMA].s;
  edge = s->val[OPT_EDGE_EMPHASIS].s;
  lightcolor = s->val[OPT_LIGHTCOLOR].s;
  s->speed = s->val[OPT_SPEED].w;
  s->xres = s->val[OPT_X_RESOLUTION].w;
  s->yres = s->val[OPT_Y_RESOLUTION].w;
  s->ulx = MM_TO_PIX(SANE_UNFIX(s->val[OPT_TL_X].w), s->dev->info.mud);
  s->uly = MM_TO_PIX(SANE_UNFIX(s->val[OPT_TL_Y].w), s->dev->info.mud);
#if 0
  /* xxx now set in sane_get_parameters: */
  s->width = MM_TO_PIX(  SANE_UNFIX(s->val[OPT_BR_X].w)
                       - SANE_UNFIX(s->val[OPT_TL_X].w));
  s->length = MM_TO_PIX(  SANE_UNFIX(s->val[OPT_BR_Y].w)
                        - SANE_UNFIX(s->val[OPT_TL_Y].w));
#endif
  s->threshold = s->val[XOPT_THRESHOLD].w;
  s->bpp = s->params.depth;

  if (strcmp (mode, M_LINEART) == 0)
    {
      s->reverse = 0;
      if (strcmp(halftone, M_BILEVEL) == 0)
        {
          s->halftone = 1;
          s->image_composition = 0;
        }
      else if (strcmp(halftone, M_BAYER) == 0)
        {
          s->halftone = 2;
          s->image_composition = 1;
        }
      else if (strcmp(halftone, M_SPIRAL) == 0)
        {
          s->halftone = 3;
          s->image_composition = 1;
        }
      else if (strcmp(halftone, M_DISPERSED) == 0)
        {
          s->halftone = 4;
          s->image_composition = 1;
        }
      else if (strcmp(halftone, M_ERRDIFFUSION) == 0)
        {
          s->halftone = 5;
          s->image_composition = 1;
        }
    }
  else if (strcmp (mode, M_GRAY) == 0)
    {
      s->image_composition = 2;
      s->reverse = 1;
    }
  else if (strcmp (mode, M_LINEART_COLOR) == 0)
    {
      s->reverse = 0;
      if (strcmp(halftone, M_BILEVEL) == 0)
        {
          s->halftone = 1;
          s->image_composition = 3;
        }
      else if (strcmp(halftone, M_BAYER) == 0)
        {
          s->halftone = 2;
          s->image_composition = 4;
        }
      else if (strcmp(halftone, M_SPIRAL) == 0)
        {
          s->halftone = 3;
          s->image_composition = 4;
        }
      else if (strcmp(halftone, M_DISPERSED) == 0)
        {
          s->halftone = 4;
          s->image_composition = 4;
        }
      else if (strcmp(halftone, M_ERRDIFFUSION) == 0)
        {
          s->halftone = 5;
          s->image_composition = 5;
        }
    }
  else if (strcmp (mode, M_COLOR) == 0)
    {
      s->image_composition = 5;
      s->reverse = 1;
    }

  if (strcmp (edge, EDGE_NONE) == 0)
    {
      DBG (11, "EDGE EMPHASIS NONE\n");
      s->edge = 0;
    }
  else if (strcmp (edge, EDGE_MIDDLE) == 0)
    {
      DBG (11, "EDGE EMPHASIS MIDDLE\n");
      s->edge = 1;
    }
  else if (strcmp (edge, EDGE_STRONG) == 0)
    {
      DBG (11, "EDGE EMPHASIS STRONG\n");
      s->edge = 2;
    }
  else if (strcmp (edge, EDGE_BLUR) == 0)
    {
      DBG (11, "EDGE EMPHASIS BLUR\n");
      s->edge = 3;
    }
  
  s->lightcolor = 3;
  if (strcmp(lightcolor, LIGHT_GREEN) == 0)
    s->lightcolor = 0;
  else if (strcmp(lightcolor, LIGHT_RED) == 0)
    s->lightcolor = 1;
  else if (strcmp(lightcolor, LIGHT_BLUE) == 0)
    s->lightcolor = 2;
  else if (strcmp(lightcolor, LIGHT_WHITE) == 0)
    s->lightcolor = 3;

  if (strcmp(s->dev->sane.model, "JX250 SCSI") != 0)
    {
      ss.dtc = 0x03;
      if (strcmp (gamma, GAMMA10) == 0)
      {
          ss.dtq = 0x01;
      }else{
          ss.dtq = 0x02;
      }
      ss.length = 0;
      DBG (5, "start: SEND\n");
      status = send (s->fd,  &ss);
      if (status != SANE_STATUS_GOOD)
        {
          DBG (1, "send failed: %s\n", sane_strstatus (status));
          sanei_scsi_close (s->fd);
          return (status);
        }
   }
   
  if (strcmp(s->dev->sane.model, "JX250 SCSI") != 0)
    {
      ss.dtc = 0x86;
      ss.dtq = 0x05;
      ss.length = 0;
      DBG (5, "start: SEND\n");
      status = send (s->fd,  &ss);
      if (status != SANE_STATUS_GOOD)
        {
          DBG (1, "send failed: %s\n", sane_strstatus (status));
          sanei_scsi_close (s->fd);
          return (status);
        }
    }

  memset (&wp, 0, sizeof (wp));
  /* The JX 250 needs a window descriptor block different from that
     for the JX610/JX330
  */
  if (strcmp(s->dev->sane.model, "JX250 SCSI") != 0)
    {
      buf_size = sizeof(WDB);
    }
  else
    {
      buf_size = sizeof (WDB) + sizeof(WDBX250);
    }
    
  wp.wpdh.wdl[0] = buf_size >> 8;
  wp.wpdh.wdl[1] = buf_size;
  wp.wdb.x_res[0] = s->xres >> 8;
  wp.wdb.x_res[1] = s->xres;
  wp.wdb.y_res[0] = s->yres >> 8;
  wp.wdb.y_res[1] = s->yres;
  wp.wdb.x_ul[0] = s->ulx >> 24;
  wp.wdb.x_ul[1] = s->ulx >> 16;
  wp.wdb.x_ul[2] = s->ulx >> 8;
  wp.wdb.x_ul[3] = s->ulx;
  wp.wdb.y_ul[0] = s->uly >> 24;
  wp.wdb.y_ul[1] = s->uly >> 16;
  wp.wdb.y_ul[2] = s->uly >> 8;
  wp.wdb.y_ul[3] = s->uly;
  wp.wdb.width[0] = s->width >> 24;
  wp.wdb.width[1] = s->width >> 16;
  wp.wdb.width[2] = s->width >> 8;
  wp.wdb.width[3] = s->width;
  wp.wdb.length[0] = s->length >> 24;
  wp.wdb.length[1] = s->length >> 16;
  wp.wdb.length[2] = s->length >> 8;
  wp.wdb.length[3] = s->length;
  wp.wdb.brightness = 0;
  wp.wdb.threshold = s->threshold;
  wp.wdb.image_composition = s->image_composition;
  wp.wdb.bpp = s->bpp;
  wp.wdb.ht_pattern[0] = 0;
  wp.wdb.ht_pattern[1] = s->halftone;
  wp.wdb.rif_padding = (s->reverse * 128) + 0;
  wp.wdb.eletu = (!s->speed << 2) + (s->edge << 6) + (s->lightcolor << 4);

  DBG (5, "wdl=%d\n", (wp.wpdh.wdl[0] << 8) + wp.wpdh.wdl[1]);
  DBG (5, "xres=%d\n", (wp.wdb.x_res[0] << 8) + wp.wdb.x_res[1]);
  DBG (5, "yres=%d\n", (wp.wdb.y_res[0] << 8) + wp.wdb.y_res[1]);
  DBG (5, "ulx=%d\n", (wp.wdb.x_ul[0] << 24) + (wp.wdb.x_ul[1] << 16) +
                      (wp.wdb.x_ul[2] << 8) + wp.wdb.x_ul[3]);
  DBG (5, "uly=%d\n", (wp.wdb.y_ul[0] << 24) + (wp.wdb.y_ul[1] << 16) +
                      (wp.wdb.y_ul[2] << 8) + wp.wdb.y_ul[3]);
  DBG (5, "width=%d\n", (wp.wdb.width[0] << 8) + (wp.wdb.width[1] << 16) +
                        (wp.wdb.width[2] << 8) + wp.wdb.width[3]);
  DBG (5, "length=%d\n", (wp.wdb.length[0] << 16) + (wp.wdb.length[1] << 16) +
                         (wp.wdb.length[2] << 8) + wp.wdb.length[3]);

  DBG (5, "threshold=%d\n", wp.wdb.threshold);
  DBG (5, "image_composition=%d\n", wp.wdb.image_composition);
  DBG (5, "bpp=%d\n", wp.wdb.bpp);
  DBG (5, "rif_padding=%d\n", wp.wdb.rif_padding);
  DBG (5, "eletu=%d\n", wp.wdb.eletu);
  
#if 0
  {
    unsigned char *p = (unsigned char*) &wp.wdb;
    int i;
    DBG(50, "set window:\n");
    for (i = 0; i < sizeof(wp.wdb) + sizeof(wp.wdbx250); i += 16)
     {
      DBG(1, "%2x %2x %2x %2x %2x %2x %2x %2x - %2x %2x %2x %2x %2x %2x %2x %2x\n",
      p[i], p[i+1], p[i+2], p[i+3], p[i+4], p[i+5], p[i+6], p[i+7], p[i+8],
      p[i+9], p[i+10], p[i+11], p[i+12], p[i+13], p[i+14], p[i+15]);
     }
  }
#endif
 
  buf_size += sizeof(WPDH);
  DBG (5, "start: SET WINDOW\n");
  status = set_window (s->fd, &wp, buf_size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "SET WINDOW failed: %s\n", sane_strstatus (status));
      sanei_scsi_close (s->fd);
      return (status);
    }

  memset (&wp, 0, buf_size);
  DBG (5, "start: GET WINDOW\n");
  status = get_window (s->fd, &wp, &buf_size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "GET WINDOW failed: %s\n", sane_strstatus (status));
      sanei_scsi_close (s->fd);
      return (status);
    }
  DBG (5, "xres=%d\n", (wp.wdb.x_res[0] << 8) + wp.wdb.x_res[1]);
  DBG (5, "yres=%d\n", (wp.wdb.y_res[0] << 8) + wp.wdb.y_res[1]);
  DBG (5, "ulx=%d\n", (wp.wdb.x_ul[0] << 24) + (wp.wdb.x_ul[1] << 16) +
                      (wp.wdb.x_ul[2] << 8) + wp.wdb.x_ul[3]);
  DBG (5, "uly=%d\n", (wp.wdb.y_ul[0] << 24) + (wp.wdb.y_ul[1] << 16) +
       (wp.wdb.y_ul[2] << 8) + wp.wdb.y_ul[3]);
  DBG (5, "width=%d\n", (wp.wdb.width[0] << 24) + (wp.wdb.width[1] << 16) +
                        (wp.wdb.width[2] << 8) + wp.wdb.width[3]);
  DBG (5, "length=%d\n", (wp.wdb.length[0] << 24) + (wp.wdb.length[1] << 16) +
                         (wp.wdb.length[2] << 8) + wp.wdb.length[3]);

  DBG (5, "start: SCAN\n");
  s->scanning = SANE_TRUE;
  s->busy = SANE_TRUE;
  s->cancel = SANE_FALSE;
  status = scan (s->fd);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "start of scan failed: %s\n", sane_strstatus (status));
      sanei_scsi_close (s->fd);
      s->busy = SANE_FALSE;
      s->cancel = SANE_FALSE;
      return (status);
    }

  /* ask the scanner for the scan size */
  wait_ready(s->fd);
  sane_get_parameters(s, 0);
  s->bytes_to_read = s->params.bytes_per_line * s->params.lines;

  DBG (1, "%d pixels per line, %d bytes, %d lines high, total %lu bytes, "
       "dpi=%d\n", s->params.pixels_per_line, s->params.bytes_per_line,
       s->params.lines, (u_long) s->bytes_to_read, s->val[OPT_Y_RESOLUTION].w);

  s->busy = SANE_FALSE;
  s->buf_used = 0;
  s->buf_pos = 0;

  if (s->cancel == SANE_TRUE) 
    {
      do_cancel(s);
      DBG (10, ">>\n");
      return(SANE_STATUS_CANCELLED);
    }

  DBG (10, ">>\n");
  return (SANE_STATUS_GOOD);

}

static SANE_Status
sane_read_direct (SANE_Handle handle, SANE_Byte *dst_buf, SANE_Int max_len,
	   SANE_Int * len)
{
  SHARP_Scanner *s = handle;
  SANE_Status status;
  size_t nread;
  DBG (10, "<< sane_read_direct ");

  *len = 0;

  if (s->bytes_to_read == 0)
    {
      do_cancel (s);
      return (SANE_STATUS_EOF);
    }

  if (!s->scanning)
    return (do_cancel (s));
  nread = max_len;
  if (nread > s->bytes_to_read)
    nread = s->bytes_to_read;
  if (nread > sanei_scsi_max_request_size)
    nread = sanei_scsi_max_request_size;
  wait_ready(s->fd);
  status = read_data (s->fd, dst_buf, &nread);
  if (status != SANE_STATUS_GOOD)
    {
      do_cancel (s);
      return (SANE_STATUS_IO_ERROR);
    }
  *len = nread;
  s->bytes_to_read -= nread;

  DBG (10, ">>\n");
  return (SANE_STATUS_GOOD);
}

static SANE_Status
sane_read_shuffled (SANE_Handle handle, SANE_Byte *dst_buf, SANE_Int max_len,
	   SANE_Int * len)
{
  SHARP_Scanner *s = handle;
  SANE_Status status;
  SANE_Byte *dest, *red, *green, *blue;
  size_t nread, transfer, pixel, max_pixel, line, max_line;
  DBG (10, "<< sane_read_shuffled ");

  *len = 0;
  if (s->bytes_to_read == 0 && s->buf_pos == s->buf_used) 
    {
      do_cancel (s);
      DBG (10, ">>\n");
      return (SANE_STATUS_EOF);
    }
    
  if (!s->scanning) 
    {
      DBG (10, ">>\n");
      return(do_cancel(s));
    }

  if (s->buf_pos < s->buf_used)
    {
      transfer = s->buf_used - s->buf_pos;
      if (transfer > max_len)
        transfer = max_len;
      
      memcpy(dst_buf, &(s->buffer[s->buf_pos]), transfer);
      s->buf_pos += transfer;
      max_len -= transfer;
      *len = transfer;
    }

  while (max_len > 0 && s->bytes_to_read > 0) 
    {
      wait_ready(s->fd);
      nread = sanei_scsi_max_request_size / s->params.bytes_per_line - 1;
      nread *= s->params.bytes_per_line;
      if (nread > s->bytes_to_read)
        nread = s->bytes_to_read;
      status = read_data (s->fd, &(s->buffer[s->params.bytes_per_line]), 
                          &nread);
      if (status != SANE_STATUS_GOOD)
        {
          do_cancel (s);
          DBG (10, ">>\n");
          return (SANE_STATUS_IO_ERROR);
        }
      
      if (nread % s->params.bytes_per_line != 0) 
        {
          /* if this happens, the approach to read the data
             of an integral number of scanlines with one scsi command
             if probably wrong. On the other hand, the documentation
             of the JX 250 does not mention any restrictions or
             situations where this approach could fail.
          */
          DBG(1, "Warning: could not read an integral number of scan lines\n");
          DBG(1, "         image will be scrambled\n");
        }
      
      
      s->buf_used = nread;
      s->buf_pos = 0;
      s->bytes_to_read -= nread;
      dest = s->buffer;
      max_line = nread / s->params.bytes_per_line;
      max_pixel = s->params.bytes_per_line / 3;
      for (line = 1; line <= max_line; line++)
        {
          red = &(s->buffer[line * s->params.bytes_per_line]);
          green = &(red[max_pixel]);
          blue = &(green[max_pixel]);
          for (pixel = 0; pixel < max_pixel; pixel++)
            {
              *dest++ = *red++;
              *dest++ = *green++;
              *dest++ = *blue++;
            }
        }
      
      transfer = max_len;
      if (transfer > s->buf_used)
        transfer = s->buf_used;
      memcpy(&(dst_buf[*len]), s->buffer, transfer);
      
      max_len -= transfer;
      s->buf_pos += transfer;
      *len += transfer;
    }

  if (s->bytes_to_read == 0 && s->buf_pos == s->buf_used)
    do_cancel (s);
  DBG (10, ">>\n");
  return (SANE_STATUS_GOOD);
}

SANE_Status
sane_read (SANE_Handle handle, SANE_Byte *dst_buf, SANE_Int max_len,
	   SANE_Int * len)
{
  SHARP_Scanner *s = handle;
  SANE_Status status;

  s->busy = SANE_TRUE;
  if (s->cancel == SANE_TRUE) 
    {
      do_cancel(s);
      *len = 0;
      return (SANE_STATUS_CANCELLED);
    }
  
  /* RGB scans with a JX 250 must be handled differently: */
  if (   strncmp(s->dev->sane.model, "JX250 SCSI", 10) != 0
      || s->image_composition <= 2)
    status = sane_read_direct(handle, dst_buf, max_len, len);
  else
    status = sane_read_shuffled(handle, dst_buf, max_len, len);
  
  s->busy = SANE_FALSE;
  if (s->cancel == SANE_TRUE)
    {
      do_cancel(s);
      return (SANE_STATUS_CANCELLED);
    }
    
  return (status);
}

void
sane_cancel (SANE_Handle handle)
{
  SHARP_Scanner *s = handle;
  DBG (10, "<< sane_cancel ");

  s->cancel = SANE_TRUE;
  if (s->busy == SANE_FALSE)
      do_cancel(handle);

  DBG (10, ">>\n");
}

SANE_Status
sane_set_io_mode (SANE_Handle handle, SANE_Bool non_blocking)
{
  DBG (10, "<< sane_set_io_mode");
  DBG (10, ">>\n");

  return SANE_STATUS_UNSUPPORTED;
}

SANE_Status
sane_get_select_fd (SANE_Handle handle, SANE_Int * fd)
{
  DBG (10, "<< sane_get_select_fd");
  DBG (10, ">>\n");

  return SANE_STATUS_UNSUPPORTED;
}
