/* sane - Scanner Access Now Easy.
   Copyright (C) 1997 BYTEC GmbH Germany
   Written by Helmut Koeberle, Email: helmut.koeberle@bytec.de
   Modified by Manuel Panea <Manuel.Panea@rzg.mpg.de>
   and Markus Mertinat <Markus.Mertinat@Physik.Uni-Augsburg.DE>
   FB620 support by Mitsuru Okaniwa <m-okaniwa@bea.hi-ho.ne.jp>
   FS2710 support by Ulrich Deiters <ukd@xenon.pc.uni-koeln.de>
   (Canon refused to make the FS2710 documentation available,
   hence not all features are supported.)

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
   If you do not wish that, delete this exception notice. */

/* This file implements the sane-api */

/* SANE-FLOW-DIAGRAMM

   - sane_init() : initialize backend, attach scanners(devicename,0)
   . - sane_get_devices() : query list of scanner-devices
   . - sane_open() : open a particular scanner-device and attach_scanner(devicename,&dev)
   . . - sane_set_io_mode : set blocking-mode
   . . - sane_get_select_fd : get scanner-fd
   . . - sane_get_option_descriptor() : get option informations
   . . - sane_control_option() : change option values
   . .
   . . - sane_start() : start image aquisition
   . .   - sane_get_parameters() : returns actual scan-parameters
   . .   - sane_read() : read image-data (from pipe)
   . . - sane_cancel() : cancel operation, kill reader_process
   
   . - sane_close() : close opened scanner-device, do_cancel, free buffer and handle
   - sane_exit() : terminate use of backend, free devicename and device-struture
*/

/* This driver's flow:

 - sane_init
 . - attach_one
 . . - inquiry
 . . - test_unit_ready
 . . - medium_position
 . . - extended inquiry
 . . - mode sense
 . . - get_density_curve
 - sane_get_devices
 - sane_open
 . - init_options
 - sane_set_io_mode : set blocking-mode
 - sane_get_select_fd : get scanner-fd
 - sane_get_option_descriptor() : get option informations
 - sane_control_option() : change option values
 - sane_start() : start image aquisition
   - sane_get_parameters() : returns actual scan-parameters
   - sane_read() : read image-data (from pipe)
   - sane_cancel() : cancel operation, kill reader_process
 - sane_close() : close opened scanner-device, do_cancel, free buffer and handle
 - sane_exit() : terminate use of backend, free devicename and device-struture
*/

#include <sane/config.h>

#include <limits.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <time.h>

#include <sane/sane.h>
#include <sane/saneopts.h>
#include <sane/sanei_scsi.h>
#include <canon.h>

#define BACKEND_NAME canon

#include <sane/sanei_backend.h>

#ifndef PATH_MAX
#define PATH_MAX	1024
#endif

#include <sane/sanei_config.h>
#define CANON_CONFIG_FILE "canon.conf"

#define MM_PER_INCH	 25.4

#define GAMMA_ENTRIES_12   256	/* modification for FS2710 */
#define GAMMA_LENGTH_12   1*GAMMA_ENTRIES_12	/* modification for FS2710 */
#define GAMMA_TRANSFER_12  0x03	/* modification for FS2710 */

static int num_devices = 0;
static CANON_Device *first_dev = NULL;
static CANON_Scanner *first_handle = NULL;

static const SANE_String_Const mode_list[] = {
  "Lineart", "Halftone", "Gray", "Color", 0
};

/* modification for FS2710 */
static const SANE_String_Const mode_list_fs2710[] = {
  "Color", "Raw", 0
};

/* modification for FB620S */
static const SANE_String_Const mode_list_fb620[] = {
  "Lineart", "Gray", "Color", "Fine color", 0
};

static const SANE_String_Const tpu_dc_mode_list[] = {
  "No transparency correction",
  "Correction according to Film type",
  "Correction according to Transparency Ratio",
  0
};

static const SANE_String_Const page_list[] = {
  "Show normal options",
  "Show advanced options",
  "Show all options",
  0
};

static const SANE_String_Const filmtype_list[] = {
  "Negatives",
  "Slides",
  0
};

static const SANE_String_Const negative_filmtype_list[] = {
  "Film type 0", "Film type 1", "Film type 2", "Film type 3",
  0
};

static const SANE_String_Const scanning_speed_list[] = {
  "Automatic", "Normal speed", "1/2 normal speed", "1/3 normal speed",
  0
};

static const SANE_String_Const tpu_filmtype_list[] = {
  "Film 0", "Film 1", "Film 2", "Film 3",
  0
};

static const SANE_String_Const papersize_list[] = {
  "A4", "Letter", "B5", "Maximal",
  0
};

/**************************************************/

static const SANE_Range u8_range = {
  0,				/* minimum */
  255,				/* maximum */
  0				/* quantization */
};

#include "canon-scsi.c"

/**************************************************************************/

static size_t
max_string_size (const SANE_String_Const strings[])
{
  size_t size, max_size = 0;
  int i;
  DBG (11, ">> max_string_size\n");

  for (i = 0; strings[i]; ++i)
    {
      size = strlen (strings[i]) + 1;
      if (size > max_size)
	max_size = size;
    }

  DBG (11, "<< max_string_size\n");
  return max_size;
}

/**************************************************************************/

static void
get_tpu_stat (int fd, CANON_Device * dev)
{
  unsigned char tbuf[12 + 5];
  size_t buf_size, i;
  SANE_Status status;

  DBG (3, ">> get tpu stat\n");

  /* Check wether it is a CS-600 at all */
  if (strncmp (dev->sane.model, "IX-06015", 8))
    {
      dev->tpu.Status = TPU_STAT_NONE;
      return;
    }

  memset (tbuf, 0, sizeof (tbuf));
  buf_size = sizeof (tbuf);
  status = get_scan_mode (fd, TRANSPARENCY_UNIT, tbuf, &buf_size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "get scan mode failed: %s\n", sane_strstatus (status));
      return;
    }

  for (i = 0; i < buf_size; i++)
    {
      DBG (3, "scan mode control byte[%d] = %d\n", i, tbuf[i]);
    }
  dev->tpu.Status = (tbuf[2 + 4 + 5] >> 7) ?
    TPU_STAT_INACTIVE : TPU_STAT_NONE;
  if (dev->tpu.Status == SANE_TRUE)	/* TPU available */
    {
      dev->tpu.Status = (tbuf[2 + 4 + 5] && 0x04) ?
	TPU_STAT_INACTIVE : TPU_STAT_ACTIVE;
    }
  dev->tpu.ControlMode = tbuf[3 + 4 + 5] && 0x03;
  dev->tpu.Transparency = tbuf[4 + 4 + 5] * 256 + tbuf[5 + 4 + 5];
  dev->tpu.PosNeg = tbuf[6 + 4 + 5] && 0x01;
  dev->tpu.FilmType = tbuf[7 + 4 + 5];

  DBG (11, "TPU Status: %d\n", dev->tpu.Status);
  DBG (11, "TPU ControlMode: %d\n", dev->tpu.ControlMode);
  DBG (11, "TPU Transparency: %d\n", dev->tpu.Transparency);
  DBG (11, "TPU PosNeg: %d\n", dev->tpu.PosNeg);
  DBG (11, "TPU FilmType: %d\n", dev->tpu.FilmType);

  DBG (3, "<< get tpu stat\n");

  return;
}

/**************************************************************************/

static void
get_adf_stat (int fd, CANON_Device * dev)
{
  size_t buf_size = 0x0C, i;
  unsigned char abuf[0x0C];
  SANE_Status status;

  DBG (3, ">> get adf stat\n");

  if (strncmp (dev->sane.model, FB620S, 9) == 0)
    {
      dev->adf.Status = ADF_STAT_NONE;
      return;
    }

  memset (abuf, 0, buf_size);
  status = get_scan_mode (fd, AUTO_DOC_FEEDER_UNIT, abuf, &buf_size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "get scan mode failed: %s\n", sane_strstatus (status));
      perror ("get scan mode failed");
      return;
    }

  for (i = 0; i < buf_size; i++)
    {
      DBG (3, "scan mode control byte[%d] = %d\n", i, abuf[i]);
/*    printf("scan mode control byte[%d] = %d\n", i, abuf[i]); */
    }

  dev->adf.Status = (abuf[ADF_Status] & ADF_NOT_PRESENT) ?
    ADF_STAT_NONE : ADF_STAT_INACTIVE;

  if (dev->adf.Status == SANE_TRUE)	/* ADF available / INACTIVE */
    {
      dev->adf.Status = (abuf[ADF_Status] & ADF_PROBLEM) ?
	ADF_STAT_INACTIVE : ADF_STAT_ACTIVE;
    }
  dev->adf.Problem = (abuf[ADF_Status] & ADF_PROBLEM);
  dev->adf.Priority = (abuf[ADF_Settings] & ADF_PRIORITY);
  dev->adf.Feeder = (abuf[ADF_Settings] & ADF_FEEDER);

/*#ifndef NDEBUG
  printf("ADF Status: %d\n", dev->adf.Status);
  printf("ADF Priority: %d\n", dev->adf.Priority);
  printf("ADF Problem: %d\n", dev->adf.Problem);
  printf("ADF Feeder: %d\n", dev->adf.Feeder);
# else */
  DBG (11, "ADF Status: %d\n", dev->adf.Status);
  DBG (11, "ADF Priority: %d\n", dev->adf.Priority);
  DBG (11, "ADF Problem: %d\n", dev->adf.Problem);
  DBG (11, "ADF Feeder: %d\n", dev->adf.Feeder);

  DBG (3, "<< get adf stat\n");
/* # endif */
  return;
}

/**************************************************************************/

static SANE_Status
sense_handler (int scsi_fd, u_char * result, void *arg)
{
  static char me[] = "canon_sense_handler";
  CANON_Scanner *s = (CANON_Scanner *) arg;
  u_char sense, asc, ascq;
  char *sense_str = NULL;
  SANE_Status status = SANE_STATUS_GOOD;

  DBG (1, ">> sense_handler\n");

  DBG (11, "%s(%ld, %p, %p)\n", me, (long) scsi_fd,
       (void *) result, (void *) arg);

  DBG (11, "sense buffer: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x \
%02x %02x %02x %02x %02x %02x\n", result[0], result[1], result[2], result[3], result[4], result[5], result[6], result[7], result[8], result[9], result[10], result[11], result[12], result[13], result[14], result[15]);

  if (s)
    {

      if (s->hw->info.model != FB620 && s->hw->info.model != CS3_600)
	{
	  sense = result[0];

	  switch (sense)
	    {
	    case 0x00:		/* Good */
	      sense_str = "Good. Command has executed normally";
	      break;
	    case 0x02:		/* Check Condition */
	      sense_str = "Check Condition";
	      break;
	    case 0x08:		/* Busy */
	      sense_str = "Scanner is busy";
	      break;
	    case 0x18:		/* Reservation Conflict */
	      sense_str = "The scanner is reserved by another SCSI device";
	      break;
	    default:
	      DBG (5, "%s: no handling for sense %x.\n", me, sense);
	      break;
	    }
	}
      else
	{
/* sense hander for FB620S */
	  sense = result[2] & 0x0f;
	  if (result[7] > 3)
	    {
	      asc = result[12];
	      ascq = result[13];
	      switch (sense)
		{
		case 0x01:
		  switch (asc)
		    {
		    case 0x37:
		      switch (ascq)
			{
			case 0x00:
			  sense_str = "Rounded Parameter";
			  status = SANE_STATUS_GOOD;
			  break;
			}
		      break;
		    }
		  break;

		case 0x03:
		  switch (asc)
		    {
		    case 0x80:
		      switch (ascq)
			{
			case 0x00:
			  sense_str = "ADF Jam";
			  status = SANE_STATUS_IO_ERROR;
			  break;
			case 0x01:
			  sense_str = "ADF Cover Open";
			  status = SANE_STATUS_IO_ERROR;
			  break;
			}
		      break;
		    }
		  break;

		case 0x04:
		  switch (asc)
		    {
		    case 0x60:
		      switch (ascq)
			{
			case 0x00:
			  sense_str = "Lamp Failure";
			  status = SANE_STATUS_IO_ERROR;
			  break;
			}
		      break;

		    case 0x62:
		      switch (ascq)
			{
			case 0x00:
			  sense_str = "Scan Head Positioning Error";
			  status = SANE_STATUS_IO_ERROR;
			  break;
			}
		      break;

		    case 0x80:
		      switch (ascq)
			{
			case 0x01:
			  sense_str = "CPU check Error";
			  status = SANE_STATUS_IO_ERROR;
			  break;
			case 0x02:
			  sense_str = "RAM check Error";
			  status = SANE_STATUS_IO_ERROR;
			  break;
			case 0x03:
			  sense_str = "ROM check Error";
			  status = SANE_STATUS_IO_ERROR;
			  break;
			case 0x04:
			  sense_str = "Hardware check Error";
			  status = SANE_STATUS_IO_ERROR;
			  break;
			case 0x05:
			  sense_str = "Transparency Unit Lamp Failure";
			  status = SANE_STATUS_IO_ERROR;
			  break;
			case 0x06:
			  sense_str =
			    "Transparency Unit Scan Head Positioning Failure";
			  status = SANE_STATUS_IO_ERROR;
			  break;

			}
		      break;
		    }
		  break;

		case 0x05:
		  switch (asc)
		    {
		    case 0x1a:
		      switch (ascq)
			{
			case 0x00:
			  sense_str = "Parameter List Length Error";
			  status = SANE_STATUS_IO_ERROR;
			  break;
			}
		      break;

		    case 0x20:
		      switch (ascq)
			{
			case 0x00:
			  sense_str = "Invalid Command Operation Code";
			  status = SANE_STATUS_IO_ERROR;
			  break;
			}
		      break;

		    case 0x24:
		      switch (ascq)
			{
			case 0x00:
			  sense_str = "Invalid Field in CDB";
			  status = SANE_STATUS_IO_ERROR;
			  break;
			}
		      break;

		    case 0x25:
		      switch (ascq)
			{
			case 0x00:
			  sense_str = "Unsupported LUN";
			  status = SANE_STATUS_UNSUPPORTED;
			  break;
			}
		      break;

		    case 0x26:
		      switch (ascq)
			{
			case 0x00:
			  sense_str = "Invalid Field in Parameter List";
			  status = SANE_STATUS_UNSUPPORTED;
			  break;
			}
		      break;

		    case 0x2c:
		      switch (ascq)
			{
			case 0x00:
			  sense_str = "Command Sequence Error";
			  status = SANE_STATUS_UNSUPPORTED;
			  break;

			case 0x01:
			  sense_str = "Too Many Windows Specified";
			  status = SANE_STATUS_UNSUPPORTED;
			  break;
			}
		      break;

		    case 0x3a:
		      switch (ascq)
			{
			case 0x00:
			  sense_str = "Medium Not Present";
			  status = SANE_STATUS_IO_ERROR;
			  break;
			}
		      break;

		    case 0x3d:
		      switch (ascq)
			{
			case 0x00:
			  sense_str = "Invalid Bit IDENTIFY Message";
			  status = SANE_STATUS_UNSUPPORTED;
			  break;
			}
		      break;

		    case 0x80:
		      switch (ascq)
			{
			case 0x02:
			  sense_str = "Option Not Connect";
			  status = SANE_STATUS_UNSUPPORTED;
			  break;
			}
		      break;

		    }
		  break;

		case 0x06:
		  switch (asc)
		    {
		    case 0x29:
		      switch (ascq)
			{
			case 0x00:
			  sense_str = "Power on Reset / Bus Device Reset";
			  status = SANE_STATUS_GOOD;
			  break;
			}
		      break;

		    case 0x2a:
		      switch (ascq)
			{
			case 0x00:
			  sense_str =
			    "Parameter Changed by another Initiator";
			  status = SANE_STATUS_IO_ERROR;
			  break;
			}
		      break;
		    }
		  break;

		case 0x0b:
		  switch (asc)
		    {
		    case 0x00:
		      switch (ascq)
			{
			case 0x00:
			  sense_str = "No Additional Sense Information";
			  status = SANE_STATUS_IO_ERROR;
			  break;
			}
		      break;

		    case 0x45:
		      switch (ascq)
			{
			case 0x00:
			  sense_str = "Reselect Failure";
			  status = SANE_STATUS_IO_ERROR;
			  break;
			}
		      break;

		    case 0x47:
		      switch (ascq)
			{
			case 0x00:
			  sense_str = "SCSI Parity Error";
			  status = SANE_STATUS_IO_ERROR;
			  break;
			}
		      break;

		    case 0x48:
		      switch (ascq)
			{
			case 0x00:
			  sense_str =
			    "Initiator Detected Error Messege Received";
			  status = SANE_STATUS_IO_ERROR;
			  break;
			}
		      break;

		    case 0x49:
		      switch (ascq)
			{
			case 0x00:
			  sense_str = "Invalid Message Error";
			  status = SANE_STATUS_UNSUPPORTED;
			  break;
			}
		      break;

		    case 0x80:
		      switch (ascq)
			{
			case 0x00:
			  sense_str = "Time out Error";
			  status = SANE_STATUS_IO_ERROR;
			  break;
			case 0x01:
			  sense_str = "Trancparency Unit Shading Error";
			  status = SANE_STATUS_IO_ERROR;
			  break;
			case 0x03:
			  sense_str = "Lamp not Stabilized";
			  status = SANE_STATUS_IO_ERROR;
			  break;
			}
		      break;

		    }
		  break;
		}
	    }
	}
    }
  DBG (1, "sense buffer: %s\n", sense_str);
  if (s)
    {
      s->sense_str = sense_str;
    }
  DBG (1, "<< sense_handler\n");
  return status;
}

/***************************************************************/
static SANE_Status
do_gamma (CANON_Scanner * s)
{
  SANE_Status status;
  u_char gbuf[256];
  size_t buf_size;
  int i, j, neg, transfer_data_type, from;


  DBG (7, "sending SET_DENSITY_CURVE\n");
  buf_size = 256 * sizeof (u_char);
  transfer_data_type = 0x03;

  neg = (s->hw->info.model == CS2700 || s->hw->info.model == FS2710) ?
    strcmp (filmtype_list[1],
	    s->val[OPT_NEGATIVE].s) : s->val[OPT_HNEGATIVE].w;

  if (!strcmp (s->val[OPT_MODE].s, "Gray"))
    {
      /* If scanning in gray mode, use the first curve for the
         scanner's monochrome gamma component                    */
      for (j = 0; j < 256; j++)
	{
	  if (neg == SANE_FALSE)
	    {
	      gbuf[j] = (u_char) s->gamma_table[0][j];
	      DBG (22, "set_density %d: gbuf[%d] = [%d]\n", 0, j, gbuf[j]);
	    }
	  else
	    {
	      gbuf[255 - j] = (u_char) (255 - s->gamma_table[0][j]);
	      DBG (22, "set_density %d: gbuf[%d] = [%d]\n", 0, 255 - j,
		   gbuf[255 - j]);
	    }
	}
      if ((status = set_density_curve (s->fd, 0, gbuf, &buf_size,
				       transfer_data_type)) !=
	  SANE_STATUS_GOOD)
	{
	  DBG (7, "SET_DENSITY_CURVE\n");
	  sanei_scsi_close (s->fd);
	  s->fd = -1;
	  return (SANE_STATUS_INVAL);
	}
    }
  else
    {				/* colour mode */
      /* If in RGB mode but with gamma bind, use the first curve
         for all 3 colors red, green, blue */
      for (i = 1; i < 4; i++)
	{
	  from = (s->val[OPT_CUSTOM_GAMMA_BIND].w == SANE_TRUE) ? 0 : i;
	  for (j = 0; j < 256; j++)
	    {
	      if (neg == SANE_FALSE)
		{
		  gbuf[j] = (u_char) s->gamma_table[from][j];
		  DBG (22, "set_density %d: gbuf[%d] = [%d]\n", i, j,
		       gbuf[j]);
		}
	      else
		{
		  gbuf[255 - j] = (u_char) (255 - s->gamma_table[from][j]);
		  DBG (22, "set_density %d: gbuf[%d] = [%d]\n", i, 255 - j,
		       gbuf[255 - j]);
		}
	    }
	  if (s->hw->info.model == FS2710)
	    status = set_density_curve_fs2710 (s, i, gbuf);
	  else
	    {
	      if ((status = set_density_curve (s->fd, i, gbuf, &buf_size,
					       transfer_data_type)) !=
		  SANE_STATUS_GOOD)
		{
		  DBG (7, "SET_DENSITY_CURVE\n");
		  sanei_scsi_close (s->fd);
		  s->fd = -1;
		  return (SANE_STATUS_INVAL);
		}
	    }
	}
    }

  return (SANE_STATUS_GOOD);
}

/**************************************************************************/

static SANE_Status
attach (const char *devnam, CANON_Device ** devp)
{
  SANE_Status status;
  CANON_Device *dev;

  int fd;
  u_char ibuf[36], ebuf[74], mbuf[12];
  size_t buf_size, i;
  char *str;

  DBG (1, ">> attach\n");

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
  status = sanei_scsi_open (devnam, &fd, 0, 0);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "attach: open failed: %s\n", sane_strstatus (status));
      return (status);
    }

  DBG (3, "attach: sending (standard) INQUIRY\n");
  memset (ibuf, 0, sizeof (ibuf));
  buf_size = sizeof (ibuf);
  status = inquiry (fd, 0, ibuf, &buf_size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "attach: inquiry failed: %s\n", sane_strstatus (status));
      sanei_scsi_close (fd);
      fd = -1;
      return (status);
    }

  if (ibuf[0] != 6
      || strncmp (ibuf + 8, "CANON", 5) != 0
      || strncmp (ibuf + 16, "IX-", 3) != 0)
    {
      DBG (1, "attach: device doesn't look like a Canon scanner\n");
      sanei_scsi_close (fd);
      fd = -1;
      return (SANE_STATUS_INVAL);
    }

  DBG (3, "attach: sending TEST_UNIT_READY\n");
  status = test_unit_ready (fd);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "attach: test unit ready failed (%s)\n",
	   sane_strstatus (status));
      sanei_scsi_close (fd);
      fd = -1;
      return (status);
    }

#if 0
  DBG (3, "attach: sending REQUEST SENSE\n");
  memset (sbuf, 0, sizeof (sbuf));
  buf_size = sizeof (sbuf);
  status = request_sense (fd, sbuf, &buf_size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "attach: REQUEST_SENSE failed\n");
      sanei_scsi_close (fd);
      fd = -1;
      return (SANE_STATUS_INVAL);
    }

  DBG (3, "attach: sending MEDIUM POSITION\n");
  status = medium_position (fd);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "attach: MEDIUM POSTITION failed\n");
      sanei_scsi_close (fd);
      fd = -1;
      return (SANE_STATUS_INVAL);
    }
/*   s->val[OPT_AF_NOW].w == SANE_TRUE; */
#endif

  DBG (3, "attach: sending RESERVE UNIT\n");
  status = reserve_unit (fd);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "attach: RESERVE UNIT failed\n");
      sanei_scsi_close (fd);
      fd = -1;
      return (SANE_STATUS_INVAL);
    }

/*   DBG (3, "attach: sending GET SCAN MODE for transparency unit\n"); */
/*   memset (ebuf, 0, sizeof (ebuf)); */
/*   buf_size = sizeof (ebuf); */
/*   buf_size = 12; */
/*   status = get_scan_mode (fd, TRANSPARENCY_UNIT, ebuf, &buf_size); */
/*   if (status != SANE_STATUS_GOOD) */
/*   { */
/*     DBG (1, "attach: GET SCAN MODE for transparency unit failed\n"); */
/*     sanei_scsi_close (fd); */
/*     return (SANE_STATUS_INVAL); */
/*   } */
/*   for (i=0; i<buf_size; i++) */
/*   { */
/*     DBG(3, "scan mode trans byte[%d] = %d\n", i, ebuf[i]); */
/*   } */

  DBG (3, "attach: sending GET SCAN MODE for scan control conditions\n");
  memset (ebuf, 0, sizeof (ebuf));
  buf_size = sizeof (ebuf);
  status = get_scan_mode (fd, SCAN_CONTROL_CONDITIONS, ebuf, &buf_size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "attach: GET SCAN MODE for scan control conditions failed\n");
      sanei_scsi_close (fd);
      return (SANE_STATUS_INVAL);
    }
  for (i = 0; i < buf_size; i++)
    {
      DBG (3, "scan mode byte[%d] = %d\n", i, ebuf[i]);
    }

  DBG (3, "attach: sending (extended) INQUIRY\n");
  memset (ebuf, 0, sizeof (ebuf));
  buf_size = sizeof (ebuf);
  status = inquiry (fd, 1, ebuf, &buf_size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "attach: (extended) INQUIRY failed\n");
      sanei_scsi_close (fd);
      fd = -1;
      return (SANE_STATUS_INVAL);
    }

#if 0
  DBG (3, "attach: sending GET SCAN MODE for transparency unit\n");


  memset (ebuf, 0, sizeof (ebuf));
  buf_size = 64;
  status = get_scan_mode (fd, ALL_SCAN_MODE_PAGES,	/* TRANSPARENCY_UNIT, */
			  ebuf, &buf_size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "attach: GET SCAN MODE for scan control conditions failed\n");
      sanei_scsi_close (fd);
      return (SANE_STATUS_INVAL);
    }
  for (i = 0; i < buf_size; i++)
    {
      DBG (3, "scan mode control byte[%d] = %d\n", i, ebuf[i]);
    }
#endif

/*   DBG (3, "attach: sending GET SCAN MODE for all scan mode pages\n"); */
/*   memset (ebuf, 0, sizeof (ebuf)); */
/*   buf_size = 32; */
/*   status = get_scan_mode (fd, (u_char)ALL_SCAN_MODE_PAGES,  */
/* 			  ebuf, &buf_size); */
/*   if (status != SANE_STATUS_GOOD) */
/*   { */
/*     DBG (1, "attach: GET SCAN MODE for scan control conditions failed\n"); */
/*     sanei_scsi_close (fd); */
/*     return (SANE_STATUS_INVAL); */
/*   } */
/*   for (i=0; i<buf_size; i++) */
/*   { */
/*     DBG(3, "scan mode control byte[%d] = %d\n", i, ebuf[i]); */
/*   } */


  DBG (3, "attach: sending MODE SENSE\n");
  memset (mbuf, 0, sizeof (mbuf));
  buf_size = sizeof (mbuf);
  status = mode_sense (fd, mbuf, &buf_size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "attach: MODE_SENSE failed\n");
      sanei_scsi_close (fd);
      fd = -1;
      return (SANE_STATUS_INVAL);
    }

  dev = malloc (sizeof (*dev));
  if (!dev)
    {
      sanei_scsi_close (fd);
      fd = -1;
      return (SANE_STATUS_NO_MEM);
    }
  memset (dev, 0, sizeof (*dev));

  dev->sane.name = strdup (devnam);
  dev->sane.vendor = "CANON";
  str = malloc (16 + 1);
  memset (str, 0, sizeof (str));
  strncpy (str, ibuf + 16, 16);
  dev->sane.model = str;
  /* if (!strncmp(str, "IX-27015", 5)) */
  if (strncmp (str, "IX-27015", 8) == 0)
    {
      dev->info.model = CS2700;
      dev->sane.type = "film scanner";
    }
  else if (strncmp (str, "IX-27025E", 9) == 0)	/* modification for FS2710 */
    {
      dev->info.model = FS2710;
      dev->sane.type = "film scanner";
    }
  else if (strncmp (str, "IX-06035E", 9) == 0)	/* modification for FB620S */
    {
      dev->info.model = FB620;
      dev->sane.type = "flatbed scanner";
    }
  else
    {
      dev->info.model = CS3_600;
      dev->sane.type = "flatbed scanner";
    }


  DBG (5, "dev->sane.name = '%s'\n", dev->sane.name);
  DBG (5, "dev->sane.vendor = '%s'\n", dev->sane.vendor);
  DBG (5, "dev->sane.model = '%s'\n", dev->sane.model);
  DBG (5, "dev->sane.type = '%s'\n", dev->sane.type);

  get_tpu_stat (fd, dev);	/* Query TPU */
  get_adf_stat (fd, dev);	/* Query ADF */

  dev->info.bmu = mbuf[6];
  DBG (5, "bmu=%d\n", dev->info.bmu);
  dev->info.mud = (mbuf[8] * 256) + mbuf[9];
  DBG (5, "mud=%d\n", dev->info.mud);

  dev->info.xres_default = (ebuf[5] * 256) + ebuf[6];
  DBG (5, "xres_default=%d\n", dev->info.xres_default);
  dev->info.xres_range.max = (ebuf[10] * 256) + ebuf[11];
  DBG (5, "xres_range.max=%d\n", dev->info.xres_range.max);
  dev->info.xres_range.min = (ebuf[14] * 256) + ebuf[15];
  DBG (5, "xres_range.min=%d\n", dev->info.xres_range.min);
  dev->info.xres_range.quant = ebuf[9] >> 4;
  DBG (5, "xres_range.quant=%d\n", dev->info.xres_range.quant);

  dev->info.yres_default = (ebuf[7] * 256) + ebuf[8];
  DBG (5, "yres_default=%d\n", dev->info.yres_default);
  dev->info.yres_range.max = (ebuf[12] * 256) + ebuf[13];
  DBG (5, "yres_range.max=%d\n", dev->info.yres_range.max);
  dev->info.yres_range.min = (ebuf[16] * 256) + ebuf[17];
  DBG (5, "yres_range.min=%d\n", dev->info.yres_range.min);
  dev->info.yres_range.quant = ebuf[9] & 0x0f;
  DBG (5, "xres_range.quant=%d\n", dev->info.xres_range.quant);

  dev->info.x_range.min = SANE_FIX (0.0);
  dev->info.x_range.max = (ebuf[20] * 256 * 256 * 256)
    + (ebuf[21] * 256 * 256) + (ebuf[22] * 256) + ebuf[23] - 1;
  dev->info.x_range.max =
    SANE_FIX (dev->info.x_range.max * MM_PER_INCH / dev->info.mud);
  DBG (5, "x_range.max=%d\n", dev->info.x_range.max);
  dev->info.x_range.quant = 0;

  dev->info.y_range.min = SANE_FIX (0.0);
  dev->info.y_range.max = (ebuf[24] * 256 * 256 * 256)
    + (ebuf[25] * 256 * 256) + (ebuf[26] * 256) + ebuf[27] - 1;
  dev->info.y_range.max =
    SANE_FIX (dev->info.y_range.max * MM_PER_INCH / dev->info.mud);
  DBG (5, "y_range.max=%d\n", dev->info.y_range.max);
  dev->info.y_range.quant = 0;

  dev->info.x_adf_range.max = (ebuf[30] * 256 * 256 * 256)
    + (ebuf[31] * 256 * 256) + (ebuf[32] * 256) + ebuf[33] - 1;
  DBG (5, "x_adf_range.max=%d\n", dev->info.x_adf_range.max);
  dev->info.y_adf_range.max = (ebuf[34] * 256 * 256 * 256)
    + (ebuf[35] * 256 * 256) + (ebuf[36] * 256) + ebuf[37] - 1;
  DBG (5, "y_adf_range.max=%d\n", dev->info.y_adf_range.max);


  dev->info.brightness_range.min = 0;
  dev->info.brightness_range.max = 255;
  dev->info.brightness_range.quant = 0;

  dev->info.contrast_range.min = 1;
  dev->info.contrast_range.max = 255;
  dev->info.contrast_range.quant = 0;

  dev->info.threshold_range.min = 1;
  dev->info.threshold_range.max = 255;
  dev->info.threshold_range.quant = 0;

  dev->info.HiliteR_range.min = 0;
  dev->info.HiliteR_range.max = 255;
  dev->info.HiliteR_range.quant = 0;

  dev->info.ShadowR_range.min = 0;
  dev->info.ShadowR_range.max = 254;
  dev->info.ShadowR_range.quant = 0;

  dev->info.HiliteG_range.min = 0;
  dev->info.HiliteG_range.max = 255;
  dev->info.HiliteG_range.quant = 0;

  dev->info.ShadowG_range.min = 0;
  dev->info.ShadowG_range.max = 254;
  dev->info.ShadowG_range.quant = 0;

  dev->info.HiliteB_range.min = 0;
  dev->info.HiliteB_range.max = 255;
  dev->info.HiliteB_range.quant = 0;

  dev->info.ShadowB_range.min = 0;
  dev->info.ShadowB_range.max = 254;
  dev->info.ShadowB_range.quant = 0;

  dev->info.focus_range.min = 0;
  dev->info.focus_range.max = 255;
  dev->info.focus_range.quant = 0;

  dev->info.TPU_Transparency_range.min = 0;
  dev->info.TPU_Transparency_range.max = 10000;
  dev->info.TPU_Transparency_range.quant = 100;

  sanei_scsi_close (fd);
  fd = -1;

  ++num_devices;
  dev->next = first_dev;
  first_dev = dev;

  if (devp)
    *devp = dev;

  DBG (1, "<< attach\n");
  return (SANE_STATUS_GOOD);
}

/**************************************************************************/

static SANE_Status
do_cancel (CANON_Scanner * s)
{
  SANE_Status status;

  DBG (1, ">> do_cancel\n");

  s->scanning = SANE_FALSE;

  if (s->fd >= 0)
    {
      if (s->val[OPT_EJECT_AFTERSCAN].w == SANE_TRUE)
	{
	  DBG (3, "attach: sending MEDIUM POSITION\n");
	  status = medium_position (s->fd);
	  if (status != SANE_STATUS_GOOD)
	    {
	      DBG (1, "attach: MEDIUM POSTITION failed\n");
	      return (SANE_STATUS_INVAL);
	    }
	  s->AF_NOW = SANE_TRUE;
	  DBG (1, "do_cancel AF_NOW = '%d'\n", s->AF_NOW);
	}

      DBG (21, "do_cancel: reset_flag = %d\n", s->reset_flag);
      if ((s->reset_flag == 1) && (s->hw->info.model == FB620))
	{
	  status = reset_scanner (s->fd);
	  if (status != SANE_STATUS_GOOD)
	    {
	      DBG (21, "RESET SCANNER failed\n");
	      sanei_scsi_close (s->fd);
	      s->fd = -1;
	      return (SANE_STATUS_INVAL);
	    }
	  DBG (21, "RESET SCANNER\n");
	  s->reset_flag = 0;
	  DBG (21, "do_cancel: reset_flag = %d\n", s->reset_flag);
	  s->time0 = -1;
	  DBG (21, "time0 = %ld\n", s->time0);
	}
      sanei_scsi_close (s->fd);
      s->fd = -1;
    }

  DBG (1, "<< do_cancel\n");
  return (SANE_STATUS_CANCELLED);
}

/**************************************************************************/

static SANE_Status
adjust_hilo_points (CANON_Scanner * s)
{
  int mode;
  char *mode_str;
  SANE_Status status;
  u_char wbuf[72], dbuf[28], gbuf[256];
  size_t buf_size;

  size_t nread, bread, i, j;
  SANE_Byte *adjbuf;
  double histo[3][256], sum[3], partsum[3], newsum;
  double percentage, next_percentage;
  float gamma[3];
  int maxhi, minlo, transfer_data_type;
  SANE_Word save_xres, new_xres;

  if (s->hw->info.model == FS2710)
    transfer_data_type = GAMMA_TRANSFER_12;
  else
    transfer_data_type = 0x03;

  DBG (1, ">> adjust_hilo_points\n");

  /* First make sure we have a current parameter set.  Some of the
     parameters will be overwritten below, but that's OK.  */
  status = sane_get_parameters (s, 0);
  if (status != SANE_STATUS_GOOD)
    return status;

  status = sanei_scsi_open (s->hw->sane.name, &s->fd, sense_handler, s);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "open of %s failed: %s\n",
	   s->hw->sane.name, sane_strstatus (status));
      return (status);
    }

  if (s->val[OPT_CUSTOM_GAMMA].w == 1)
    {
      buf_size = 256 * sizeof (u_char);
      for (i = 0; i < 4; i++)
	{
	  for (j = 0; j < 256; j++)
	    {
	      /* Use a straight intensity curve for all colors */
	      gbuf[j] = (u_char) j;
	      DBG (22, "set_density %d: gbuf[%d] = [%d]\n", i, j, gbuf[j]);
	    }

	  status =
	    set_density_curve (s->fd, i, gbuf, &buf_size, transfer_data_type);
	  if (status != SANE_STATUS_GOOD)
	    {
	      DBG (7, "SET_DENSITY_CURVE failed\n");
	      sanei_scsi_close (s->fd);
	      s->fd = -1;
	      return (SANE_STATUS_INVAL);
	    }
	}
    }

  mode_str = s->val[OPT_MODE].s;

  save_xres = (SANE_Word) s->val[OPT_X_RESOLUTION].w;

  s->scanning = SANE_FALSE;
  new_xres = 170;
  sane_control_option (s, OPT_X_RESOLUTION, SANE_ACTION_SET_VALUE,
		       &new_xres, 0);

  status = sane_get_parameters (s, 0);
  if (status != SANE_STATUS_GOOD)
    return status;

  s->xres = s->val[OPT_X_RESOLUTION].w;
  s->yres = s->val[OPT_X_RESOLUTION].w;

  s->ulx = SANE_UNFIX (s->val[OPT_TL_X].w) * s->hw->info.mud / MM_PER_INCH;
  s->uly = SANE_UNFIX (s->val[OPT_TL_Y].w) * s->hw->info.mud / MM_PER_INCH;

  s->width = SANE_UNFIX (s->val[OPT_BR_X].w - s->val[OPT_TL_X].w)
    * s->hw->info.mud / MM_PER_INCH;
  s->length = SANE_UNFIX (s->val[OPT_BR_Y].w - s->val[OPT_TL_Y].w)
    * s->hw->info.mud / MM_PER_INCH;

  DBG (11, "ahl: s->width='%d', s->length='%d'\n", s->width, s->length);

  if (s->hw->info.model != CS2700 && s->hw->info.model != FS2710)
    {
      if ((strcmp (mode_str, "Lineart") == 0) ||
	  (strcmp (mode_str, "Halftone") == 0))
	{
	  s->RIF = s->val[OPT_HNEGATIVE].w;
	}
      else
	{
	  s->RIF = !s->val[OPT_HNEGATIVE].w;
	}
    }

  s->brightness = s->val[OPT_BRIGHTNESS].w;
  s->contrast = s->val[OPT_CONTRAST].w;
  s->threshold = s->val[OPT_THRESHOLD].w;
  s->bpp = s->params.depth;

  s->GRC = s->val[OPT_CUSTOM_GAMMA].w;
  s->Mirror = s->val[OPT_MIRROR].w;
  s->AE = 0;

  s->HiliteR = 255;
  s->ShadowR = 0;
  s->HiliteG = 255;
  s->ShadowG = 0;
  s->HiliteB = 255;
  s->ShadowB = 0;

  if (strcmp (mode_str, "Lineart") == 0)
    {
      mode = 4;
      s->image_composition = 0;
    }
  else if (strcmp (mode_str, "Halftone") == 0)
    {
      mode = 4;
      s->image_composition = 1;
    }
  else if (strcmp (mode_str, "Gray") == 0)
    {
      mode = 5;
      s->image_composition = 2;
    }
  else if (strcmp (mode_str, "Color") == 0 ||
	   strcmp (mode_str, "Fine color") == 0)
    {
      mode = 6;
      s->image_composition = 5;
    }
  else if (strcmp (mode_str, "Raw") == 0)
    {
      mode = 6;
      s->image_composition = 5;
    }
  else
    {
      mode = 6;
      s->image_composition = 5;
    }

  DBG (11, "ahl: s->xres='%d', s->yres='%d'\n", s->xres, s->yres);

  memset (wbuf, 0, sizeof (wbuf));
  wbuf[7] = 64;
  wbuf[10] = s->xres >> 8;
  wbuf[11] = s->xres;
  wbuf[12] = s->yres >> 8;
  wbuf[13] = s->yres;
  wbuf[14] = s->ulx >> 24;
  wbuf[15] = s->ulx >> 16;
  wbuf[16] = s->ulx >> 8;
  wbuf[17] = s->ulx;
  wbuf[18] = s->uly >> 24;
  wbuf[19] = s->uly >> 16;
  wbuf[20] = s->uly >> 8;
  wbuf[21] = s->uly;
  wbuf[22] = s->width >> 24;
  wbuf[23] = s->width >> 16;
  wbuf[24] = s->width >> 8;
  wbuf[25] = s->width;
  wbuf[26] = s->length >> 24;
  wbuf[27] = s->length >> 16;
  wbuf[28] = s->length >> 8;
  wbuf[29] = s->length;
  wbuf[30] = s->brightness;
  wbuf[31] = s->threshold;
  wbuf[32] = s->contrast;
  wbuf[33] = s->image_composition;
  wbuf[34] = (s->hw->info.model == FS2710) ? 12 : s->bpp;
  wbuf[36] = 1;
  wbuf[37] = (1 << 7) + 3;
/*   wbuf[50] = (s->GRC << 3) | (s->Mirror << 2) | (s->AE); */
  wbuf[50] = (s->GRC << 3) | (s->Mirror << 2);
  wbuf[54] = 2;
  wbuf[57] = 1;
  wbuf[58] = 1;
  wbuf[59] = s->HiliteR;
  wbuf[60] = s->ShadowR;
  wbuf[62] = s->HiliteG;
  wbuf[64] = s->ShadowG;
  wbuf[70] = s->HiliteB;
  wbuf[71] = s->ShadowB;


  DBG (7, "ahl: RIF=%d, GRC=%d, Mirror=%d, AE=%d\n",
       s->RIF, s->GRC, s->Mirror, s->AE);
  DBG (7, "ahl: HR=%d, SR=%d, HG=%d, SG=%d, HB=%d, SB=%d\n",
       s->HiliteR, s->ShadowR,
       s->HiliteG, s->ShadowG, s->HiliteB, s->ShadowB);

  if (s->hw->info.model == FB620)	/* modification for FB620S */
    {
      wbuf[36] = 0;
      wbuf[50] = s->GRC << 3;
      wbuf[54] = 0;
      wbuf[57] = 0;
      wbuf[58] = 0;
    }

  buf_size = sizeof (wbuf);
  status = set_window (s->fd, wbuf);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "SET WINDOW failed: %s\n", sane_strstatus (status));
      return (status);
    }

  DBG (7, "adjust_hilo: set_window done\n");

  buf_size = sizeof (wbuf);
  memset (wbuf, 0, buf_size);
  status = get_window (s->fd, wbuf, &buf_size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "GET WINDOW failed: %s\n", sane_strstatus (status));
      return (status);
    }

  DBG (5, "ahl: xres=%d\n", (wbuf[10] * 256) + wbuf[11]);
  DBG (5, "ahl: yres=%d\n", (wbuf[12] * 256) + wbuf[13]);
  DBG (5, "ahl: ulx=%d\n", (wbuf[14] * 256 * 256 * 256)
       + (wbuf[15] * 256 * 256) + (wbuf[16] * 256) + wbuf[17]);
  DBG (5, "ahl: uly=%d\n", (wbuf[18] * 256 * 256 * 256)
       + (wbuf[19] * 256 * 256) + (wbuf[20] * 256) + wbuf[21]);
  DBG (5, "ahl: width=%d\n", (wbuf[22] * 256 * 256 * 256)
       + (wbuf[23] * 256 * 256) + (wbuf[24] * 256) + wbuf[25]);
  DBG (5, "ahl: length=%d\n", (wbuf[26] * 256 * 256 * 256)
       + (wbuf[27] * 256 * 256) + (wbuf[28] * 256) + wbuf[29]);


  status = scan (s->fd);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "start of scan failed: %s\n", sane_strstatus (status));
      return (status);
    }

  buf_size = sizeof (dbuf);
  memset (dbuf, 0, buf_size);
  status = get_data_status (s->fd, dbuf, &buf_size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "GET DATA STATUS failed: %s\n", sane_strstatus (status));
      return (status);
    }
  DBG (5, "ahl: Magnified Width=%d\n", (dbuf[12] * 256 * 256 * 256)
       + (dbuf[13] * 256 * 256) + (dbuf[14] * 256) + dbuf[15]);
  DBG (5, "ahl: Magnified Length=%d\n", (dbuf[16] * 256 * 256 * 256)
       + (dbuf[17] * 256 * 256) + (dbuf[18] * 256) + dbuf[19]);
  DBG (5, "ahl: Data=%d bytes\n", (dbuf[20] * 256 * 256 * 256)
       + (dbuf[21] * 256 * 256) + (dbuf[22] * 256) + dbuf[23]);

  if (s->xres > 0 && s->yres > 0 && s->width > 0 && s->length > 0)
    {
      s->params.pixels_per_line = s->width * s->xres / s->hw->info.mud;
      s->params.lines = s->length * s->yres / s->hw->info.mud;
    }
  s->bytes_to_read = s->params.bytes_per_line * s->params.lines;

  DBG (1,
       "ahl: %d pixels per line, %d bytes, %d lines high, total %lu bytes, "
       "dpi=%d\n", s->params.pixels_per_line, s->params.bytes_per_line,
       s->params.lines, (u_long) s->bytes_to_read,
       s->val[OPT_X_RESOLUTION].w);

  if (s->bytes_to_read == 0)
    {
      return (SANE_STATUS_GOOD);
    }

  if ((adjbuf = (SANE_Byte *) malloc (s->bytes_to_read)) == NULL)
    {
      DBG (1, "adjust_hilo: malloc failed\n");
      do_cancel (s);
      return (SANE_STATUS_IO_ERROR);
    }

  bread = 0;
  nread = 16384;
  while (s->bytes_to_read > 0)
    {
      if (nread > s->bytes_to_read)
	nread = s->bytes_to_read;
      status = read_data (s->fd, &adjbuf[bread], &nread);
      bread += nread;
      s->bytes_to_read -= nread;
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (1, "adjust_hilo: read_data failed: %s\n",
	       sane_strstatus (status));
	  do_cancel (s);
	  return (SANE_STATUS_IO_ERROR);
	}
    }

  DBG (7, "adjust_hilo: bread='%lu'\n", (unsigned long) bread);

  sane_control_option (s, OPT_X_RESOLUTION, SANE_ACTION_SET_VALUE, &save_xres,
		       0);
  status = sane_get_parameters (s, 0);
  if (status != SANE_STATUS_GOOD)
    return status;

  DBG (7, "adjust_hilo: building histograms\n");

  /* Build the histograms */
  DBG (7, "sizeof(histo)='%lu'\n", (unsigned long) sizeof (histo));
  memset (histo, 0, sizeof (histo));

  for (i = 0; i < bread; i += 3)
    {
      ++histo[RED][(int) adjbuf[i]];
      ++histo[GREEN][(int) adjbuf[i + 1]];
      ++histo[BLUE][(int) adjbuf[i + 2]];
    }

  for (i = 0; i < 256; i++)
    {
      DBG (7, "adjust_hilo: histo[%d]=(%03.3f, %03.3f, %03.3f)\n",
	   i, histo[RED][i], histo[GREEN][i], histo[BLUE][i]);
    }


  /* For each color */
  minlo = 255;
  maxhi = 0;
  for (i = RED; i <= BLUE; i++)
    {
      /* Count the "total weight" of the histogram */
      for (j = 0; j < 256; j++)
	{
	  sum[i] += histo[i][j];
	}

      /* Find the "middle weight point" in the histogram */
      j = 0;
      while ((partsum[i] <= sum[i] / 2) && (j < 256))
	{
	  partsum[i] += histo[i][j];
	  j++;
	}

      /* We set the gamma value to the middle weight point */
      /*      s->gamma_table[][] = */
      gamma[i] = (float) --j;
      DBG (1, "adjust_hilo: gamma[%d] = '%f'\n", i, gamma[i]);

      /* Find the shadow point */
      newsum = 0.0;
      for (j = 0; j < 255; j++)
	{
	  newsum += histo[i][j];
	  percentage = newsum / sum[i];
	  next_percentage = (newsum + histo[i][j + 1]) / sum[i];

	  if (fabs (percentage - 0.004) < fabs (next_percentage - 0.004))
	    {
	      minlo = j + 1;
	      break;
	    }
	}

      /* Find the hilight point */
      newsum = 0.0;
      for (j = 255; j > 0; j--)
	{
	  newsum += histo[i][j];
	  percentage = newsum / sum[i];
	  next_percentage = (newsum + histo[i][j - 1]) / sum[i];

	  if (fabs (percentage - 0.004) < fabs (next_percentage - 0.004))
	    {
	      maxhi = j + 32;
	      break;
	    }

	}

      s->val[OPT_SHADOW_R + i * 2].w = *((SANE_Word *) & minlo);
      DBG (1, "adjust_hilo: lo[%d]='%d'\n", i, minlo);
      DBG (1, "adjust_hilo: s->val[OPT_SHADOW_R+%d*2]='%d'\n",
	   i, s->val[OPT_SHADOW_R + i * 2].w);

      s->val[OPT_HILITE_R + i * 2].w = *((SANE_Word *) & maxhi);
      DBG (1, "adjust_hilo: hi[%d]='%d'\n", i, maxhi);
      DBG (1, "adjust_hilo: s->val[OPT_HILITE_R+%d*2]='%d'\n",
	   i, s->val[OPT_HILITE_R + i * 2].w);
    }

  DBG (1, "<< adjust_hilo_points\n");


  free (adjbuf);
  sanei_scsi_close (s->fd);
  s->fd = -1;

  return (SANE_STATUS_GOOD);
}

/**************************************************************************/

static SANE_Status
init_options (CANON_Scanner * s)
{
  int i;
  DBG (1, ">> init_options\n");

  memset (s->opt, 0, sizeof (s->opt));
  memset (s->val, 0, sizeof (s->val));

  s->AF_NOW = SANE_TRUE;

  for (i = 0; i < NUM_OPTIONS; ++i)
    {
      s->opt[i].size = sizeof (SANE_Word);
      s->opt[i].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    }

  s->opt[OPT_NUM_OPTS].title = SANE_TITLE_NUM_OPTIONS;
  s->opt[OPT_NUM_OPTS].desc = SANE_DESC_NUM_OPTIONS;
  s->opt[OPT_NUM_OPTS].cap = SANE_CAP_SOFT_DETECT;
  s->val[OPT_NUM_OPTS].w = NUM_OPTIONS;

  s->opt[OPT_PAGE].name = "options-page";
  s->opt[OPT_PAGE].title = "";
  s->opt[OPT_PAGE].desc = "Selects the options page to show";
  s->opt[OPT_PAGE].type = SANE_TYPE_STRING;
  s->opt[OPT_PAGE].size = max_string_size (page_list);
  s->opt[OPT_PAGE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_PAGE].constraint.string_list = page_list;
  s->val[OPT_PAGE].s = strdup (page_list[0]);

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

  if (s->hw->info.model == FB620)
    {				/* modification for FB620S */
      s->opt[OPT_MODE].size = max_string_size (mode_list_fb620);
      s->opt[OPT_MODE].constraint.string_list = mode_list_fb620;
      s->val[OPT_MODE].s = strdup (mode_list_fb620[3]);
    }
  else if (s->hw->info.model == FS2710)
    {				/* modification for FS2710 */
      s->opt[OPT_MODE].size = max_string_size (mode_list_fs2710);
      s->opt[OPT_MODE].constraint.string_list = mode_list_fs2710;
      s->val[OPT_MODE].s = strdup (mode_list_fs2710[0]);
    }
  else
    {
      s->opt[OPT_MODE].size = max_string_size (mode_list);
      s->opt[OPT_MODE].constraint.string_list = mode_list;
      s->val[OPT_MODE].s = strdup (mode_list[3]);
    }

  /* Slides or negatives */
  s->opt[OPT_NEGATIVE].name = "film-type";
  s->opt[OPT_NEGATIVE].title = "Film type";
  s->opt[OPT_NEGATIVE].desc =
    "Selects the film type, i.e. negatives or slides";
  s->opt[OPT_NEGATIVE].type = SANE_TYPE_STRING;
  s->opt[OPT_NEGATIVE].size = max_string_size (filmtype_list);
  s->opt[OPT_NEGATIVE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_NEGATIVE].constraint.string_list = filmtype_list;
  s->opt[OPT_NEGATIVE].cap |=
    (s->hw->info.model == CS2700 || s->hw->info.model == FS2710) ?
    0 : SANE_CAP_INACTIVE;
  s->val[OPT_NEGATIVE].s = strdup (filmtype_list[1]);

  /* Negative film type */
  s->opt[OPT_NEGATIVE_TYPE].name = "negative-film-type";
  s->opt[OPT_NEGATIVE_TYPE].title = "Negative film type";
  s->opt[OPT_NEGATIVE_TYPE].desc = "Selects the negative film type";
  s->opt[OPT_NEGATIVE_TYPE].type = SANE_TYPE_STRING;
  s->opt[OPT_NEGATIVE_TYPE].size = max_string_size (negative_filmtype_list);
  s->opt[OPT_NEGATIVE_TYPE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_NEGATIVE_TYPE].constraint.string_list = negative_filmtype_list;
  s->opt[OPT_NEGATIVE_TYPE].cap |=
    (s->hw->info.model == CS2700 || s->hw->info.model == FS2710) ?
    0 : SANE_CAP_INACTIVE;
  s->val[OPT_NEGATIVE_TYPE].s = strdup (negative_filmtype_list[0]);

  /* Scanning speed */
  s->opt[OPT_SCANNING_SPEED].name = "scanning-speed";
  s->opt[OPT_SCANNING_SPEED].title = "Scanning speed";
  s->opt[OPT_SCANNING_SPEED].desc = "Selects the scanning speed";
  s->opt[OPT_SCANNING_SPEED].type = SANE_TYPE_STRING;
  s->opt[OPT_SCANNING_SPEED].size = max_string_size (scanning_speed_list);
  s->opt[OPT_SCANNING_SPEED].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_SCANNING_SPEED].constraint.string_list = scanning_speed_list;
  s->opt[OPT_SCANNING_SPEED].cap |=
    (s->hw->info.model == CS2700 || s->hw->info.model == FS2710) ?
    0 : SANE_CAP_INACTIVE;
  s->val[OPT_SCANNING_SPEED].s = strdup (scanning_speed_list[0]);

  /* "Resolution" group: */
  s->opt[OPT_RESOLUTION_GROUP].title = "Scan Resolution";
  s->opt[OPT_RESOLUTION_GROUP].desc = "";
  s->opt[OPT_RESOLUTION_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_RESOLUTION_GROUP].cap = 0;;
  s->opt[OPT_RESOLUTION_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* bind resolution */
  s->opt[OPT_RESOLUTION_BIND].name = SANE_NAME_RESOLUTION_BIND;
  s->opt[OPT_RESOLUTION_BIND].title = SANE_TITLE_RESOLUTION_BIND;
  s->opt[OPT_RESOLUTION_BIND].desc = SANE_DESC_RESOLUTION_BIND;
  s->opt[OPT_RESOLUTION_BIND].type = SANE_TYPE_BOOL;
  s->val[OPT_RESOLUTION_BIND].w = SANE_TRUE;

  /* hardware resolution only */
  s->opt[OPT_HW_RESOLUTION_ONLY].name = "hw-resolution-only";
  s->opt[OPT_HW_RESOLUTION_ONLY].title = "Hardware resolution";
  s->opt[OPT_HW_RESOLUTION_ONLY].desc = "Use only hardware resolutions";
  s->opt[OPT_HW_RESOLUTION_ONLY].type = SANE_TYPE_BOOL;
  s->val[OPT_HW_RESOLUTION_ONLY].w = SANE_TRUE;
  s->opt[OPT_HW_RESOLUTION_ONLY].cap |= (s->hw->info.model == CS2700 || s->hw->info.model == FS2710 || s->hw->info.model == FB620) ? 0 : SANE_CAP_INACTIVE;	/* mod. for FB620S */

  /* x-resolution */
  s->opt[OPT_X_RESOLUTION].name = SANE_NAME_SCAN_RESOLUTION;
  s->opt[OPT_X_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
  s->opt[OPT_X_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;
  s->opt[OPT_X_RESOLUTION].type = SANE_TYPE_INT;
  s->opt[OPT_X_RESOLUTION].unit = SANE_UNIT_DPI;
  s->opt[OPT_X_RESOLUTION].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_X_RESOLUTION].constraint.range = &s->hw->info.xres_range;
  s->val[OPT_X_RESOLUTION].w =
    (s->hw->info.model == CS2700 || s->hw->info.model == FS2710) ? 340 : 300;

  /* 990320, ss: use only hardware resolutions           */
  /* to get option menue instead of slider in xscanimage */
  if (s->hw->info.model == CS2700 || s->hw->info.model == FS2710 ||
      s->hw->info.model == FB620)
    {
      int iCnt, iNum;
      float iRes;		/* modification for FB620S */
      s->opt[OPT_X_RESOLUTION].constraint_type = SANE_CONSTRAINT_WORD_LIST;
      iNum = 0;
      iCnt = 0;

      iRes = s->hw->info.xres_range.max;
      DBG (5, "hw->info.xres_range.max=%d\n", s->hw->info.xres_range.max);
      s->opt[OPT_X_RESOLUTION].constraint.word_list = s->xres_word_list;

      /* go to minimum resolution by dividing by 2 */
      while (iRes >= s->hw->info.xres_range.min)
	{
	  iRes /= 2;
	}
      /* fill array upto maximum resolution */
      while (iRes < s->hw->info.xres_range.max)
	{
	  iCnt++;
	  iRes *= 2;
	  s->xres_word_list[iCnt] = iRes;
	}
      s->xres_word_list[0] = iCnt;
      s->val[OPT_X_RESOLUTION].w = s->xres_word_list[2];	/* 340 */
    }

  /* y-resolution */
  s->opt[OPT_Y_RESOLUTION].name = SANE_NAME_SCAN_Y_RESOLUTION;
  s->opt[OPT_Y_RESOLUTION].title = SANE_TITLE_SCAN_Y_RESOLUTION;
  s->opt[OPT_Y_RESOLUTION].desc = SANE_DESC_SCAN_Y_RESOLUTION;
  s->opt[OPT_Y_RESOLUTION].type = SANE_TYPE_INT;
  s->opt[OPT_Y_RESOLUTION].unit = SANE_UNIT_DPI;
  s->opt[OPT_Y_RESOLUTION].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_Y_RESOLUTION].constraint.range = &s->hw->info.yres_range;
  s->val[OPT_Y_RESOLUTION].w =
    (s->hw->info.model == CS2700 || s->hw->info.model == FS2710) ? 340 : 300;
  s->opt[OPT_Y_RESOLUTION].cap |= SANE_CAP_INACTIVE;

  /* 990320, ss: use only hardware resolutions           */
  /* to get option menue instead of slider in xscanimage */
  if (s->hw->info.model == CS2700 || s->hw->info.model == FS2710 ||
      s->hw->info.model == FB620)
    {
      int iCnt, iNum;
      float iRes;		/* modification for FB620S */
      s->opt[OPT_Y_RESOLUTION].constraint_type = SANE_CONSTRAINT_WORD_LIST;
      iNum = 0;
      iCnt = 0;

      iRes = s->hw->info.yres_range.max;
      DBG (5, "hw->info.yres_range.max=%d\n", s->hw->info.yres_range.max);
      s->opt[OPT_Y_RESOLUTION].constraint.word_list = s->yres_word_list;

      /* go to minimum resolution by dividing by 2 */
      while (iRes >= s->hw->info.yres_range.min)
	{
	  iRes /= 2;
	}
      /* fill array upto maximum resolution */
      while (iRes < s->hw->info.yres_range.max)
	{
	  iCnt++;
	  iRes *= 2;
	  s->yres_word_list[iCnt] = iRes;
	}
      s->yres_word_list[0] = iCnt;
      s->val[OPT_Y_RESOLUTION].w = s->yres_word_list[2];	/* 340 */
    }

  /* Focus group: */
  s->opt[OPT_FOCUS_GROUP].title = "Focus";
  s->opt[OPT_FOCUS_GROUP].desc = "";
  s->opt[OPT_FOCUS_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_FOCUS_GROUP].cap = SANE_CAP_ADVANCED;
  s->opt[OPT_FOCUS_GROUP].constraint_type = SANE_CONSTRAINT_NONE;
  s->opt[OPT_FOCUS_GROUP].cap |= (s->hw->info.model == FB620) ? SANE_CAP_INACTIVE : 0;	/* modification for FB620S */

  /* Auto-Focus switch */
  s->opt[OPT_AF].name = "af";
  s->opt[OPT_AF].title = "Auto Focus";
  s->opt[OPT_AF].desc = "Enable/disable Auto Focus";
  s->opt[OPT_AF].type = SANE_TYPE_BOOL;
  s->opt[OPT_AF].cap |= (s->hw->info.model == FB620) ? SANE_CAP_INACTIVE : 0;	/* modification for FB620S */
  s->val[OPT_AF].w = (s->hw->info.model == FB620) ? SANE_FALSE : SANE_TRUE;

  /* Auto-Focus once switch */
  s->opt[OPT_AF_ONCE].name = "afonce";
  s->opt[OPT_AF_ONCE].title = "Auto Focus only once";
  s->opt[OPT_AF_ONCE].desc = "Do Auto Focus only once between ejects";
  s->opt[OPT_AF_ONCE].type = SANE_TYPE_BOOL;
  s->opt[OPT_AF_ONCE].cap |=
    (s->hw->info.model == CS2700 || s->hw->info.model == FS2710) ?
    0 : SANE_CAP_INACTIVE;
  s->val[OPT_AF_ONCE].w =
    (s->hw->info.model == CS2700 || s->hw->info.model == CS2700) ?
    SANE_TRUE : SANE_FALSE;

  /* Manual focus */
  s->opt[OPT_FOCUS].name = "focus";
  s->opt[OPT_FOCUS].title = "Manual focus position";
  s->opt[OPT_FOCUS].desc =
    "Set the optical system's focus position by hand. The default position is 128.";
  s->opt[OPT_FOCUS].type = SANE_TYPE_INT;
  s->opt[OPT_FOCUS].unit = SANE_UNIT_NONE;
  s->opt[OPT_FOCUS].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_FOCUS].constraint.range = &s->hw->info.focus_range;
  s->opt[OPT_FOCUS].cap |= (s->hw->info.model == FB620) ? SANE_CAP_INACTIVE : 0;	/* modification for FB620S */
  s->val[OPT_FOCUS].w = (s->hw->info.model == FB620) ? SANE_FALSE : 128;

  /* Margins group: */
  s->opt[OPT_MARGINS_GROUP].title = "Scan margins";
  s->opt[OPT_MARGINS_GROUP].desc = "";
  s->opt[OPT_MARGINS_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_MARGINS_GROUP].cap = SANE_CAP_ADVANCED;
  s->opt[OPT_MARGINS_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* top-left x */
  s->opt[OPT_TL_X].name = SANE_NAME_SCAN_TL_X;
  s->opt[OPT_TL_X].title = SANE_TITLE_SCAN_TL_X;
  s->opt[OPT_TL_X].desc = SANE_DESC_SCAN_TL_X;
  s->opt[OPT_TL_X].type = SANE_TYPE_FIXED;
  s->opt[OPT_TL_X].unit = SANE_UNIT_MM;
  s->opt[OPT_TL_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TL_X].constraint.range = &s->hw->info.x_range;
  s->val[OPT_TL_X].w = 0;

  /* top-left y */
  s->opt[OPT_TL_Y].name = SANE_NAME_SCAN_TL_Y;
  s->opt[OPT_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
  s->opt[OPT_TL_Y].desc = SANE_DESC_SCAN_TL_Y;
  s->opt[OPT_TL_Y].type = SANE_TYPE_FIXED;
  s->opt[OPT_TL_Y].unit = SANE_UNIT_MM;
  s->opt[OPT_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TL_Y].constraint.range = &s->hw->info.y_range;
  s->val[OPT_TL_Y].w = 0;

  /* bottom-right x */
  s->opt[OPT_BR_X].name = SANE_NAME_SCAN_BR_X;
  s->opt[OPT_BR_X].title = SANE_TITLE_SCAN_BR_X;
  s->opt[OPT_BR_X].desc = SANE_DESC_SCAN_BR_X;
  s->opt[OPT_BR_X].type = SANE_TYPE_FIXED;
  s->opt[OPT_BR_X].unit = SANE_UNIT_MM;
  s->opt[OPT_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_X].constraint.range = &s->hw->info.x_range;
  s->val[OPT_BR_X].w = s->hw->info.x_range.max;

  /* bottom-right y */
  s->opt[OPT_BR_Y].name = SANE_NAME_SCAN_BR_Y;
  s->opt[OPT_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
  s->opt[OPT_BR_Y].desc = SANE_DESC_SCAN_BR_Y;
  s->opt[OPT_BR_Y].type = SANE_TYPE_FIXED;
  s->opt[OPT_BR_Y].unit = SANE_UNIT_MM;
  s->opt[OPT_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_Y].constraint.range = &s->hw->info.y_range;
  s->val[OPT_BR_Y].w = s->hw->info.y_range.max;

  /* Colors group: */
  s->opt[OPT_COLORS_GROUP].title = "Extra color adjustments";
  s->opt[OPT_COLORS_GROUP].desc = "";
  s->opt[OPT_COLORS_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_COLORS_GROUP].cap = SANE_CAP_ADVANCED;
  s->opt[OPT_COLORS_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* Positive/Negative switch for the CanoScan 300/600 models */
  s->opt[OPT_HNEGATIVE].name = SANE_NAME_NEGATIVE;
  s->opt[OPT_HNEGATIVE].title = SANE_TITLE_NEGATIVE;
  s->opt[OPT_HNEGATIVE].desc = SANE_DESC_NEGATIVE;
  s->opt[OPT_HNEGATIVE].type = SANE_TYPE_BOOL;
  s->opt[OPT_HNEGATIVE].cap |=
    (s->hw->info.model == CS2700 || s->hw->info.model == FS2710) ?
    SANE_CAP_INACTIVE : 0;
  s->val[OPT_HNEGATIVE].w = SANE_FALSE;

  /* Same values vor highlight and shadow points for red, green, blue */
  s->opt[OPT_BIND_HILO].name = "bind-highlight-shadow-points";
  s->opt[OPT_BIND_HILO].title = SANE_TITLE_RGB_BIND;
  s->opt[OPT_BIND_HILO].desc = SANE_DESC_RGB_BIND;
  s->opt[OPT_BIND_HILO].type = SANE_TYPE_BOOL;
  s->opt[OPT_BIND_HILO].cap |=
    (s->hw->info.model == FB620) ? SANE_CAP_INACTIVE : 0;
  s->val[OPT_BIND_HILO].w = SANE_TRUE;

  /* highlight point for red   */
  s->opt[OPT_HILITE_R].name = SANE_NAME_HIGHLIGHT_R;
  s->opt[OPT_HILITE_R].title = SANE_TITLE_HIGHLIGHT_R;
  s->opt[OPT_HILITE_R].desc = SANE_DESC_HIGHLIGHT_R;
  s->opt[OPT_HILITE_R].type = SANE_TYPE_INT;
  s->opt[OPT_HILITE_R].unit = SANE_UNIT_NONE;
  s->opt[OPT_HILITE_R].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_HILITE_R].constraint.range = &s->hw->info.HiliteR_range;
  s->opt[OPT_HILITE_R].cap |= SANE_CAP_INACTIVE;
  s->val[OPT_HILITE_R].w = 255;

  /* shadow    point for red   */
  s->opt[OPT_SHADOW_R].name = SANE_NAME_SHADOW_R;
  s->opt[OPT_SHADOW_R].title = SANE_TITLE_SHADOW_R;
  s->opt[OPT_SHADOW_R].desc = SANE_DESC_SHADOW_R;
  s->opt[OPT_SHADOW_R].type = SANE_TYPE_INT;
  s->opt[OPT_SHADOW_R].unit = SANE_UNIT_NONE;
  s->opt[OPT_SHADOW_R].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_SHADOW_R].constraint.range = &s->hw->info.ShadowR_range;
  s->opt[OPT_SHADOW_R].cap |= SANE_CAP_INACTIVE;
  s->val[OPT_SHADOW_R].w = 0;

  /* highlight point for green */
  s->opt[OPT_HILITE_G].name = SANE_NAME_HIGHLIGHT;
  s->opt[OPT_HILITE_G].title = SANE_TITLE_HIGHLIGHT;
  s->opt[OPT_HILITE_G].desc = SANE_DESC_HIGHLIGHT;
  s->opt[OPT_HILITE_G].type = SANE_TYPE_INT;
  s->opt[OPT_HILITE_G].unit = SANE_UNIT_NONE;
  s->opt[OPT_HILITE_G].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_HILITE_G].constraint.range = &s->hw->info.HiliteG_range;
  s->val[OPT_HILITE_G].w = 255;

  /* shadow    point for green */
  s->opt[OPT_SHADOW_G].name = SANE_NAME_SHADOW;
  s->opt[OPT_SHADOW_G].title = SANE_TITLE_SHADOW;
  s->opt[OPT_SHADOW_G].desc = SANE_DESC_SHADOW;
  s->opt[OPT_SHADOW_G].type = SANE_TYPE_INT;
  s->opt[OPT_SHADOW_G].unit = SANE_UNIT_NONE;
  s->opt[OPT_SHADOW_G].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_SHADOW_G].constraint.range = &s->hw->info.ShadowG_range;
  s->val[OPT_SHADOW_G].w = 0;

  /* highlight point for blue  */
  s->opt[OPT_HILITE_B].name = SANE_NAME_HIGHLIGHT_B;
  s->opt[OPT_HILITE_B].title = SANE_TITLE_HIGHLIGHT_B;
  s->opt[OPT_HILITE_B].desc = SANE_DESC_HIGHLIGHT_B;
  s->opt[OPT_HILITE_B].type = SANE_TYPE_INT;
  s->opt[OPT_HILITE_B].unit = SANE_UNIT_NONE;
  s->opt[OPT_HILITE_B].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_HILITE_B].constraint.range = &s->hw->info.HiliteB_range;
  s->opt[OPT_HILITE_B].cap |= SANE_CAP_INACTIVE;
  s->val[OPT_HILITE_B].w = 255;

  /* shadow    point for blue  */
  s->opt[OPT_SHADOW_B].name = SANE_NAME_SHADOW_B;
  s->opt[OPT_SHADOW_B].title = SANE_TITLE_SHADOW_B;
  s->opt[OPT_SHADOW_B].desc = SANE_DESC_SHADOW_B;
  s->opt[OPT_SHADOW_B].type = SANE_TYPE_INT;
  s->opt[OPT_SHADOW_B].unit = SANE_UNIT_NONE;
  s->opt[OPT_SHADOW_B].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_SHADOW_B].constraint.range = &s->hw->info.ShadowB_range;
  s->opt[OPT_SHADOW_B].cap |= SANE_CAP_INACTIVE;
  s->val[OPT_SHADOW_B].w = 0;


  /* "Enhancement" group: */
  s->opt[OPT_ENHANCEMENT_GROUP].title = "Enhancement";
  s->opt[OPT_ENHANCEMENT_GROUP].desc = "";
  s->opt[OPT_ENHANCEMENT_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_ENHANCEMENT_GROUP].cap = 0;
  s->opt[OPT_ENHANCEMENT_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* brightness */
  s->opt[OPT_BRIGHTNESS].name = SANE_NAME_BRIGHTNESS;
  s->opt[OPT_BRIGHTNESS].title = SANE_TITLE_BRIGHTNESS;
  s->opt[OPT_BRIGHTNESS].desc = SANE_DESC_BRIGHTNESS;
  s->opt[OPT_BRIGHTNESS].type = SANE_TYPE_INT;
  s->opt[OPT_BRIGHTNESS].unit = SANE_UNIT_NONE;
  s->opt[OPT_BRIGHTNESS].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BRIGHTNESS].constraint.range = &s->hw->info.brightness_range;
  s->opt[OPT_BRIGHTNESS].cap |= 0;
  s->val[OPT_BRIGHTNESS].w = 128;

  /* contrast */
  s->opt[OPT_CONTRAST].name = SANE_NAME_CONTRAST;
  s->opt[OPT_CONTRAST].title = SANE_TITLE_CONTRAST;
  s->opt[OPT_CONTRAST].desc = SANE_DESC_CONTRAST;
  s->opt[OPT_CONTRAST].type = SANE_TYPE_INT;
  s->opt[OPT_CONTRAST].unit = SANE_UNIT_NONE;
  s->opt[OPT_CONTRAST].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_CONTRAST].constraint.range = &s->hw->info.contrast_range;
  s->opt[OPT_CONTRAST].cap |= 0;
  s->val[OPT_CONTRAST].w = 128;

  /* threshold */
  s->opt[OPT_THRESHOLD].name = SANE_NAME_THRESHOLD;
  s->opt[OPT_THRESHOLD].title = SANE_TITLE_THRESHOLD;
  s->opt[OPT_THRESHOLD].desc = SANE_DESC_THRESHOLD;
  s->opt[OPT_THRESHOLD].type = SANE_TYPE_INT;
  s->opt[OPT_THRESHOLD].unit = SANE_UNIT_NONE;
  s->opt[OPT_THRESHOLD].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_THRESHOLD].constraint.range = &s->hw->info.threshold_range;
  s->opt[OPT_THRESHOLD].cap |= SANE_CAP_INACTIVE;
  s->val[OPT_THRESHOLD].w = 128;

  s->opt[OPT_MIRROR].name = "mirror";
  s->opt[OPT_MIRROR].title = "Mirror image";
  s->opt[OPT_MIRROR].desc = "Mirror the image horizontally";
  s->opt[OPT_MIRROR].type = SANE_TYPE_BOOL;
  s->opt[OPT_MIRROR].cap |= (s->hw->info.model == FB620) ? SANE_CAP_INACTIVE : 0;	/* modification for FB620S */
  s->val[OPT_MIRROR].w = SANE_FALSE;

  /* analog-gamma curve */
  s->opt[OPT_CUSTOM_GAMMA].name = SANE_NAME_CUSTOM_GAMMA;
  s->opt[OPT_CUSTOM_GAMMA].title = SANE_TITLE_CUSTOM_GAMMA;
  s->opt[OPT_CUSTOM_GAMMA].desc = SANE_DESC_CUSTOM_GAMMA;
  s->opt[OPT_CUSTOM_GAMMA].type = SANE_TYPE_BOOL;
  s->val[OPT_CUSTOM_GAMMA].w = SANE_FALSE;

  /* bind analog-gamma */
  s->opt[OPT_CUSTOM_GAMMA_BIND].name = "bind-custom-gamma";
  s->opt[OPT_CUSTOM_GAMMA_BIND].title = SANE_TITLE_RGB_BIND;
  s->opt[OPT_CUSTOM_GAMMA_BIND].desc = SANE_DESC_RGB_BIND;
  s->opt[OPT_CUSTOM_GAMMA_BIND].type = SANE_TYPE_BOOL;
  s->opt[OPT_CUSTOM_GAMMA_BIND].cap |= SANE_CAP_INACTIVE;
  s->val[OPT_CUSTOM_GAMMA_BIND].w = SANE_TRUE;

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

  s->opt[OPT_AE].name = "ae";
  s->opt[OPT_AE].title = "Auto Exposure";
  s->opt[OPT_AE].desc = "Enable/disable the Auto Exposure feature";
  s->opt[OPT_AE].cap |= (s->hw->info.model == CS2700 || s->hw->info.model == FS2710 || s->hw->info.model == FB620) ? SANE_CAP_INACTIVE : 0;	/* mod. for FB620S */
  s->opt[OPT_AE].type = SANE_TYPE_BOOL;
  s->val[OPT_AE].w = SANE_FALSE;


  /* "Calibration" group (active only for FB620S) */
  s->opt[OPT_CALIBRATION_GROUP].title = "Calibration";
  s->opt[OPT_CALIBRATION_GROUP].desc = "";
  s->opt[OPT_CALIBRATION_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_CALIBRATION_GROUP].cap |= (s->hw->info.model == FB620) ? 0 : SANE_CAP_INACTIVE;	/* modification for FB620S */
  s->opt[OPT_CALIBRATION_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* calibration now for FB620S */
  s->opt[OPT_CALIBRATION_NOW].name = "calibration-now";
  s->opt[OPT_CALIBRATION_NOW].title = "Calibration now";
  s->opt[OPT_CALIBRATION_NOW].desc = "Execute calibration *now*";
  s->opt[OPT_CALIBRATION_NOW].type = SANE_TYPE_BUTTON;
  s->opt[OPT_CALIBRATION_NOW].unit = SANE_UNIT_NONE;
  s->opt[OPT_CALIBRATION_NOW].cap |=
    (s->hw->info.model == FB620) ? 0 : SANE_CAP_INACTIVE;
  s->opt[OPT_CALIBRATION_NOW].constraint_type = SANE_CONSTRAINT_NONE;
  s->opt[OPT_CALIBRATION_NOW].constraint.range = NULL;

  /* scanner self diagnostic for FB620S */
  s->opt[OPT_SCANNER_SELF_DIAGNOSTIC].name = "self-diagnostic";
  s->opt[OPT_SCANNER_SELF_DIAGNOSTIC].title = "Self diagnostic";
  s->opt[OPT_SCANNER_SELF_DIAGNOSTIC].desc =
    "Perform scanner self diagnostic";
  s->opt[OPT_SCANNER_SELF_DIAGNOSTIC].type = SANE_TYPE_BUTTON;
  s->opt[OPT_SCANNER_SELF_DIAGNOSTIC].unit = SANE_UNIT_NONE;
  s->opt[OPT_SCANNER_SELF_DIAGNOSTIC].cap |=
    (s->hw->info.model == FB620) ? 0 : SANE_CAP_INACTIVE;
  s->opt[OPT_SCANNER_SELF_DIAGNOSTIC].constraint_type = SANE_CONSTRAINT_NONE;
  s->opt[OPT_SCANNER_SELF_DIAGNOSTIC].constraint.range = NULL;

  /* reset scanner for FB620S */
  s->opt[OPT_RESET_SCANNER].name = "reset-scanner";
  s->opt[OPT_RESET_SCANNER].title = "Reset scanner";
  s->opt[OPT_RESET_SCANNER].desc = "Reste the scanner";
  s->opt[OPT_RESET_SCANNER].type = SANE_TYPE_BUTTON;
  s->opt[OPT_RESET_SCANNER].unit = SANE_UNIT_NONE;
  s->opt[OPT_RESET_SCANNER].cap |=
    (s->hw->info.model == FB620) ? 0 : SANE_CAP_INACTIVE;
  s->opt[OPT_RESET_SCANNER].constraint_type = SANE_CONSTRAINT_NONE;
  s->opt[OPT_RESET_SCANNER].constraint.range = NULL;

  /* "Eject" group (active only for model 2700F) */
  s->opt[OPT_EJECT_GROUP].title = "Medium handling";
  s->opt[OPT_EJECT_GROUP].desc = "";
  s->opt[OPT_EJECT_GROUP].type = SANE_TYPE_GROUP;
/*   s->opt[OPT_EJECT_GROUP].cap |=  */
/*     (s->hw->info.model == CS2700) ? 0 : SANE_CAP_INACTIVE; */
  s->opt[OPT_EJECT_GROUP].cap = 0;
  s->opt[OPT_EJECT_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* eject after scan */
  s->opt[OPT_EJECT_AFTERSCAN].name = "eject-after-scan";
  s->opt[OPT_EJECT_AFTERSCAN].title = "Eject film after each scan";
  s->opt[OPT_EJECT_AFTERSCAN].desc =
    "Automatically eject the film from the device after each scan";
  s->opt[OPT_EJECT_AFTERSCAN].cap |=
    (s->hw->info.model == CS2700 || s->hw->info.model == FS2710) ?
    0 : SANE_CAP_INACTIVE;
  s->opt[OPT_EJECT_AFTERSCAN].type = SANE_TYPE_BOOL;
  s->val[OPT_EJECT_AFTERSCAN].w = SANE_FALSE;

  /* eject before exit */
  s->opt[OPT_EJECT_BEFOREEXIT].name = "eject-before-exit";
  s->opt[OPT_EJECT_BEFOREEXIT].title = "Eject film before exit";
  s->opt[OPT_EJECT_BEFOREEXIT].desc =
    "Automatically eject the film from the device before exiting the program";
  s->opt[OPT_EJECT_BEFOREEXIT].cap |=
    (s->hw->info.model == CS2700 || s->hw->info.model == FS2710) ?
    0 : SANE_CAP_INACTIVE;
  s->opt[OPT_EJECT_BEFOREEXIT].type = SANE_TYPE_BOOL;
  s->val[OPT_EJECT_BEFOREEXIT].w =
    (s->hw->info.model == CS2700 || s->hw->info.model == FS2710) ?
    SANE_TRUE : SANE_FALSE;

  /* eject now */
  s->opt[OPT_EJECT_NOW].name = "eject-now";
  s->opt[OPT_EJECT_NOW].title = "Eject film now";
  s->opt[OPT_EJECT_NOW].desc = "Eject the film from the device *now*";
  s->opt[OPT_EJECT_NOW].type = SANE_TYPE_BUTTON;
  s->opt[OPT_EJECT_NOW].unit = SANE_UNIT_NONE;
  s->opt[OPT_EJECT_NOW].cap |=
    (s->hw->info.model == CS2700 || s->hw->info.model == FS2710) ?
    0 : SANE_CAP_INACTIVE;
  s->opt[OPT_EJECT_NOW].constraint_type = SANE_CONSTRAINT_NONE;
  s->opt[OPT_EJECT_NOW].constraint.range = NULL;

  /* "NO-ADF" option: */
  s->opt[OPT_ADF_GROUP].title = "Document Feeder Extras";
  s->opt[OPT_ADF_GROUP].desc = "";
  s->opt[OPT_ADF_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_ADF_GROUP].cap = 0;
  s->opt[OPT_ADF_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  s->opt[OPT_FLATBED_ONLY].name = "noadf";
  s->opt[OPT_FLATBED_ONLY].title = "Flatbed Only";
  s->opt[OPT_FLATBED_ONLY].desc =
    "Disable Auto Document Feeder and use Flatbed only";
  s->opt[OPT_FLATBED_ONLY].type = SANE_TYPE_BOOL;
  s->opt[OPT_FLATBED_ONLY].unit = SANE_UNIT_NONE;
  s->opt[OPT_FLATBED_ONLY].size = sizeof (SANE_Word);
  s->opt[OPT_FLATBED_ONLY].cap |=
    (s->hw->adf.Status == ADF_STAT_NONE) ? SANE_CAP_INACTIVE : 0;
  s->val[OPT_FLATBED_ONLY].w = SANE_FALSE;

  /* "TPU" group: */
  s->opt[OPT_TPU_GROUP].title = "Transparency Unit";
  s->opt[OPT_TPU_GROUP].desc = "";
  s->opt[OPT_TPU_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_TPU_GROUP].cap = 0;
  s->opt[OPT_TPU_GROUP].constraint_type = SANE_CONSTRAINT_NONE;
  s->opt[OPT_TPU_GROUP].cap |=
    (s->hw->tpu.Status != TPU_STAT_NONE) ? 0 : SANE_CAP_INACTIVE;


  /* Transparency Unit (FAU, Film Adapter Unit) */
  s->opt[OPT_TPU_ON].name = "transparency-unit-on-off";
  s->opt[OPT_TPU_ON].title =
    (s->hw->tpu.Status == TPU_STAT_ACTIVE) ?
    "Turn Off the Transparency Unit" : "Turn On the Transparency Unit";
  s->opt[OPT_TPU_ON].desc =
    "Switch on/off the Transparency Unit (FAU, Film Adapter Unit)";
  s->opt[OPT_TPU_ON].type = SANE_TYPE_BOOL;
  s->opt[OPT_TPU_ON].unit = SANE_UNIT_NONE;
  s->val[OPT_TPU_ON].w = s->hw->tpu.Status;
  s->opt[OPT_TPU_ON].cap |=
    (s->hw->tpu.Status != TPU_STAT_NONE) ? 0 : SANE_CAP_INACTIVE;

  s->opt[OPT_TPU_PN].name = "transparency-unit-negative-film";
  s->opt[OPT_TPU_PN].title = "Negative Film";
  s->opt[OPT_TPU_PN].desc = "Positive or Negative Film";
  s->opt[OPT_TPU_PN].type = SANE_TYPE_BOOL;
  s->opt[OPT_TPU_PN].unit = SANE_UNIT_NONE;
  s->val[OPT_TPU_PN].w = s->hw->tpu.PosNeg;
  s->opt[OPT_TPU_PN].cap |=
    (s->hw->tpu.Status == TPU_STAT_ACTIVE) ? 0 : SANE_CAP_INACTIVE;

  /* density control mode */
  s->opt[OPT_TPU_DCM].name = "TPMDC";
  s->opt[OPT_TPU_DCM].title = "Density Control";
  s->opt[OPT_TPU_DCM].desc = "Set Density Control Mode";
  s->opt[OPT_TPU_DCM].type = SANE_TYPE_STRING;
  s->opt[OPT_TPU_DCM].size = max_string_size (tpu_dc_mode_list);
  s->opt[OPT_TPU_DCM].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_TPU_DCM].constraint.string_list = tpu_dc_mode_list;
  s->val[OPT_TPU_DCM].s = strdup (tpu_dc_mode_list[s->hw->tpu.ControlMode]);
  s->opt[OPT_TPU_DCM].cap |=
    (s->hw->tpu.Status == TPU_STAT_ACTIVE) ? 0 : SANE_CAP_INACTIVE;

  /* Transparency Ratio */
  s->opt[OPT_TPU_TRANSPARENCY].name = "Transparency-Ratio";
  s->opt[OPT_TPU_TRANSPARENCY].title = "Transparency Ratio";
  s->opt[OPT_TPU_TRANSPARENCY].desc = "";
  s->opt[OPT_TPU_TRANSPARENCY].type = SANE_TYPE_INT;
  s->opt[OPT_TPU_TRANSPARENCY].unit = SANE_UNIT_NONE;
  s->opt[OPT_TPU_TRANSPARENCY].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TPU_TRANSPARENCY].constraint.range =
    &s->hw->info.TPU_Transparency_range;
  s->val[OPT_TPU_TRANSPARENCY].w = s->hw->tpu.Transparency;
  s->opt[OPT_TPU_TRANSPARENCY].cap |=
    (s->hw->tpu.Status == TPU_STAT_ACTIVE &&
     s->hw->tpu.ControlMode == 3) ? 0 : SANE_CAP_INACTIVE;


  /* Select Film type */
  s->opt[OPT_TPU_FILMTYPE].name = "Filmtype";
  s->opt[OPT_TPU_FILMTYPE].title = "Select Film type";
  s->opt[OPT_TPU_FILMTYPE].desc = "Select the film type";
  s->opt[OPT_TPU_FILMTYPE].type = SANE_TYPE_STRING;
  s->opt[OPT_TPU_FILMTYPE].size = max_string_size (tpu_filmtype_list);
  s->opt[OPT_TPU_FILMTYPE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_TPU_FILMTYPE].constraint.string_list = tpu_filmtype_list;
  s->val[OPT_TPU_FILMTYPE].s =
    strdup (tpu_filmtype_list[s->hw->tpu.FilmType]);
  s->opt[OPT_TPU_FILMTYPE].cap |= (s->hw->tpu.Status == TPU_STAT_ACTIVE
				   && s->hw->tpu.ControlMode ==
				   1) ? 0 : SANE_CAP_INACTIVE;


  /* preview */
  s->opt[OPT_PREVIEW].name = SANE_NAME_PREVIEW;
  s->opt[OPT_PREVIEW].title = SANE_TITLE_PREVIEW;
  s->opt[OPT_PREVIEW].desc = SANE_DESC_PREVIEW;
  s->opt[OPT_PREVIEW].type = SANE_TYPE_BOOL;
  s->opt[OPT_PREVIEW].cap = SANE_CAP_SOFT_DETECT | SANE_CAP_SOFT_SELECT;
  s->val[OPT_PREVIEW].w = SANE_FALSE;

  DBG (1, "<< init_options\n");
  return SANE_STATUS_GOOD;
}

/**************************************************************************/

static SANE_Status
attach_one (const char *dev)
{
  DBG (1, ">> attach_one\n");
  attach (dev, 0);
  DBG (1, "<< attach_one\n");
  return SANE_STATUS_GOOD;
}

/**************************************************************************/

static SANE_Status
do_focus (CANON_Scanner * s)
{
  SANE_Status status;
  u_char ebuf[74];
  size_t buf_size;

  DBG (3, "do_focus: sending GET FILM STATUS\n");
  memset (ebuf, 0, sizeof (ebuf));
  buf_size = 4;
  status = get_film_status (s->fd, ebuf, &buf_size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "do_focus: GET FILM STATUS failed\n");
      sanei_scsi_close (s->fd);
      s->fd = -1;
      return (SANE_STATUS_INVAL);
    }
  DBG (3, "focus point before autofocus : %d\n", ebuf[3]);

  if (s->val[OPT_AF].w == SANE_TRUE)
    {
      /* Auto-Focus */
      status = execute_auto_focus (s->fd, AUTO_FOCUS,
				   (s->scanning_speed ==
				    0) ? AUTO_SCAN_SPEED : NO_AUTO_SCAN_SPEED,
				   (s->AE ==
				    0) ? NO_AUTO_EXPOSURE : AUTO_EXPOSURE, 0);
    }
  else
    {
      /* Manual Focus */
      status = execute_auto_focus (s->fd, MANUAL_FOCUS,
				   (s->scanning_speed ==
				    0) ? AUTO_SCAN_SPEED : NO_AUTO_SCAN_SPEED,
				   (s->AE ==
				    0) ? NO_AUTO_EXPOSURE : AUTO_EXPOSURE,
				   s->val[OPT_FOCUS].w);
    }
  if (status != SANE_STATUS_GOOD)
    {
      DBG (7, "EXECUTE_AUTO_FOCUS failed\n");
      sanei_scsi_close (s->fd);
      s->fd = -1;
      return (SANE_STATUS_INVAL);
    }

  DBG (3, "do_focus: sending GET FILM STATUS\n");
  memset (ebuf, 0, sizeof (ebuf));
  buf_size = 4;
  status = get_film_status (s->fd, ebuf, &buf_size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "do_focus: GET FILM STATUS failed\n");
      sanei_scsi_close (s->fd);
      s->fd = -1;
      return (SANE_STATUS_INVAL);
    }
  DBG (3, "focus point after autofocus : %d\n", ebuf[3]);

  return (SANE_STATUS_GOOD);
}

/**************************************************************************/

#include "canon-sane.c"
