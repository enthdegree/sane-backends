/* sane - Scanner Access Now Easy.

   Copyright (C) 2002 Sergey Vlasov <vsu@altlinux.ru>
   AFE offset/gain setting by David Stevenson <david.stevenson@zoom.co.uk>
   Copyright (C) 2002 Henning Meier-Geinitz <henning@meier-geinitz.de>

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

#include "gt68xx_high.h"
#include "gt68xx_mid.c"

#include <unistd.h>
#include <math.h>

static SANE_Status
gt68xx_afe_ccd_auto (GT68xx_Scanner * scanner, GT68xx_Scan_Request * request);
static SANE_Status gt68xx_afe_cis_auto (GT68xx_Scanner * scanner);


SANE_Status
gt68xx_calibrator_new (SANE_Int width,
		       SANE_Int white_level, GT68xx_Calibrator ** cal_return)
{
  DECLARE_FUNCTION_NAME ("gt68xx_calibrator_new") GT68xx_Calibrator *cal;
  SANE_Int i;

  XDBG ((5, "%s: enter: width=%d, white_level=%d\n",
	 function_name, width, white_level));

  *cal_return = 0;

  if (width <= 0)
    {
      XDBG ((5, "%s: invalid width=%d\n", function_name, width));
      return SANE_STATUS_INVAL;
    }

  cal = (GT68xx_Calibrator *) malloc (sizeof (GT68xx_Calibrator));
  if (!cal)
    {
      XDBG ((5, "%s: no memory for GT68xx_Calibrator\n", function_name));
      return SANE_STATUS_NO_MEM;
    }

  cal->k_white = NULL;
  cal->k_black = NULL;
  cal->white_line = NULL;
  cal->black_line = NULL;
  cal->width = width;
  cal->white_level = white_level;
  cal->white_count = 0;
  cal->black_count = 0;
#ifdef TUNE_CALIBRATOR
  cal->min_clip_count = cal->max_clip_count = 0;
#endif /* TUNE_CALIBRATOR */

  cal->k_white = (unsigned int *) malloc (width * sizeof (unsigned int));
  cal->k_black = (unsigned int *) malloc (width * sizeof (unsigned int));
  cal->white_line = (double *) malloc (width * sizeof (double));
  cal->black_line = (double *) malloc (width * sizeof (double));

  if (!cal->k_white || !cal->k_black | !cal->white_line || !cal->black_line)
    {
      XDBG ((5, "%s: no memory for calibration data\n", function_name));
      gt68xx_calibrator_free (cal);
      return SANE_STATUS_NO_MEM;
    }

  for (i = 0; i < width; ++i)
    {
      cal->k_white[i] = 0;
      cal->k_black[i] = 0;
      cal->white_line[i] = 0.0;
      cal->black_line[i] = 0.0;
    }

  *cal_return = cal;
  XDBG ((5, "%s: leave: ok\n", function_name));
  return SANE_STATUS_GOOD;
}

SANE_Status
gt68xx_calibrator_free (GT68xx_Calibrator * cal)
{
  DECLARE_FUNCTION_NAME ("gt68xx_calibrator_free")
    XDBG ((5, "%s: enter\n", function_name));

  if (!cal)
    {
      XDBG ((5, "%s: cal==NULL\n", function_name));
      return SANE_STATUS_INVAL;
    }

#ifdef TUNE_CALIBRATOR
  XDBG ((5, "%s: min_clip_count=%d, max_clip_count=%d\n",
	 function_name, cal->min_clip_count, cal->max_clip_count));
#endif /* TUNE_CALIBRATOR */

  if (cal->k_white)
    {
      free (cal->k_white);
      cal->k_white = NULL;
    }

  if (cal->k_black)
    {
      free (cal->k_black);
      cal->k_black = NULL;
    }

  if (cal->white_line)
    {
      free (cal->white_line);
      cal->white_line = NULL;
    }

  if (cal->black_line)
    {
      free (cal->black_line);
      cal->black_line = NULL;
    }

  free (cal);

  XDBG ((5, "%s: leave: ok\n", function_name));
  return SANE_STATUS_GOOD;
}

SANE_Status
gt68xx_calibrator_add_white_line (GT68xx_Calibrator * cal, unsigned int *line)
{
  DECLARE_FUNCTION_NAME ("gt68xx_calibrator_add_white_line") SANE_Int i;
  SANE_Int width = cal->width;

  SANE_Int sum = 0;

  cal->white_count++;

  for (i = 0; i < width; ++i)
    {
      cal->white_line[i] += line[i];
      sum += line[i];
#ifdef SAVE_WHITE_CALIBRATION
      printf ("%c", line[i] >> 8);
#endif
    }
  if (sum / width / 256 < 0x50)
    XDBG ((1,
	   "%s: WARNING: dark calibration line: %2d medium white: 0x%02x\n",
	   function_name, cal->white_count - 1, sum / width / 256));
  else
    XDBG ((5, "%s: line: %2d medium white: 0x%02x\n", function_name,
	   cal->white_count - 1, sum / width / 256));

  return SANE_STATUS_GOOD;
}

SANE_Status
gt68xx_calibrator_eval_white (GT68xx_Calibrator * cal, double factor)
{
  SANE_Int i;
  SANE_Int width = cal->width;

  for (i = 0; i < width; ++i)
    {
      cal->white_line[i] = cal->white_line[i] / cal->white_count * factor;
    }

  return SANE_STATUS_GOOD;
}

SANE_Status
gt68xx_calibrator_add_black_line (GT68xx_Calibrator * cal, unsigned int *line)
{
  DECLARE_FUNCTION_NAME ("gt68xx_calibrator_add_black_line") SANE_Int i;
  SANE_Int width = cal->width;

  SANE_Int sum = 0;

  cal->black_count++;

  for (i = 0; i < width; ++i)
    {
      cal->black_line[i] += line[i];
      sum += line[i];
#ifdef SAVE_BLACK_CALIBRATION
      printf ("%c", line[i] >> 8);
#endif
    }

  XDBG ((5, "%s: line: %2d medium black: 0x%02x\n", function_name,
	 cal->black_count - 1, sum / width / 256));
  return SANE_STATUS_GOOD;
}

SANE_Status
gt68xx_calibrator_eval_black (GT68xx_Calibrator * cal, double factor)
{
  SANE_Int i;
  SANE_Int width = cal->width;

  for (i = 0; i < width; ++i)
    {
      cal->black_line[i] = cal->black_line[i] / cal->black_count - factor;
    }

  return SANE_STATUS_GOOD;
}

SANE_Status
gt68xx_calibrator_finish_setup (GT68xx_Calibrator * cal)
{
#ifdef TUNE_CALIBRATOR
  DECLARE_FUNCTION_NAME ("gt68xx_calibrator_finish_setup")
    double ave_black = 0.0;
  double ave_diff = 0.0;
#endif /* TUNE_CALIBRATOR */
  int i;
  int width = cal->width;
  unsigned int max_value = 65535;

  for (i = 0; i < width; ++i)
    {
      unsigned int white = cal->white_line[i];
      unsigned int black = cal->black_line[i];
      unsigned int diff = (white > black) ? white - black : 1;
      if (diff > max_value)
	diff = max_value;
      cal->k_white[i] = diff;
      cal->k_black[i] = black;
#ifdef TUNE_CALIBRATOR
      ave_black += black;
      ave_diff += diff;
#endif /* TUNE_CALIBRATOR */
    }

#ifdef TUNE_CALIBRATOR
  ave_black /= width;
  ave_diff /= width;
  XDBG ((5, "%s: ave_black=%f, ave_diff=%f\n",
	 function_name, ave_black, ave_diff));
#endif /* TUNE_CALIBRATOR */

  return SANE_STATUS_GOOD;
}

SANE_Status
gt68xx_calibrator_process_line (GT68xx_Calibrator * cal, unsigned int *line)
{
  int i;
  int width = cal->width;
  unsigned int white_level = cal->white_level;

  for (i = 0; i < width; ++i)
    {
      unsigned int src_value = line[i];
      unsigned int black = cal->k_black[i];
      unsigned int value;

      if (src_value > black)
	{
	  value = (src_value - black) * white_level / cal->k_white[i];
	  if (value > 0xffff)
	    {
	      value = 0xffff;
#ifdef TUNE_CALIBRATOR
	      cal->max_clip_count++;
#endif /* TUNE_CALIBRATOR */
	    }
	}
      else
	{
	  value = 0;
#ifdef TUNE_CALIBRATOR
	  if (src_value < black)
	    cal->min_clip_count++;
#endif /* TUNE_CALIBRATOR */
	}

      line[i] = value;
    }

  return SANE_STATUS_GOOD;
}

SANE_Status
gt68xx_scanner_new (GT68xx_Device * dev, GT68xx_Scanner ** scanner_return)
{
  DECLARE_FUNCTION_NAME ("gt68xx_scanner_new") GT68xx_Scanner *scanner;

  *scanner_return = NULL;

  scanner = (GT68xx_Scanner *) malloc (sizeof (GT68xx_Scanner));
  if (!scanner)
    {
      XDBG ((5, "%s: no memory for GT68xx_Scanner\n", function_name));
      return SANE_STATUS_NO_MEM;
    }

  scanner->dev = dev;
  scanner->reader = NULL;
  scanner->cal_gray = NULL;
  scanner->cal_r = NULL;
  scanner->cal_g = NULL;
  scanner->cal_b = NULL;

  *scanner_return = scanner;
  return SANE_STATUS_GOOD;
}

static void
gt68xx_scanner_free_calibrators (GT68xx_Scanner * scanner)
{
  if (scanner->cal_gray)
    {
      gt68xx_calibrator_free (scanner->cal_gray);
      scanner->cal_gray = NULL;
    }

  if (scanner->cal_r)
    {
      gt68xx_calibrator_free (scanner->cal_r);
      scanner->cal_r = NULL;
    }

  if (scanner->cal_g)
    {
      gt68xx_calibrator_free (scanner->cal_g);
      scanner->cal_g = NULL;
    }

  if (scanner->cal_b)
    {
      gt68xx_calibrator_free (scanner->cal_b);
      scanner->cal_b = NULL;
    }
}

SANE_Status
gt68xx_scanner_free (GT68xx_Scanner * scanner)
{
  DECLARE_FUNCTION_NAME ("gt68xx_scanner_free") if (!scanner)
    {
      XDBG ((5, "%s: scanner==NULL\n", function_name));
      return SANE_STATUS_INVAL;
    }

  if (scanner->reader)
    {
      gt68xx_line_reader_free (scanner->reader);
      scanner->reader = NULL;
    }

  gt68xx_scanner_free_calibrators (scanner);

  free (scanner);

  return SANE_STATUS_GOOD;
}

static SANE_Status
gt68xx_scanner_wait_for_positioning (GT68xx_Scanner * scanner)
{
  SANE_Status status;
  SANE_Bool moving;

  usleep (100000);		/* needed by the BP 2400 CU Plus? */
  while (SANE_TRUE)
    {
      RIE (gt68xx_device_is_moving (scanner->dev, &moving));
      if (!moving)
	break;
      usleep (100000);
    }

  return SANE_STATUS_GOOD;
}


static SANE_Status
gt68xx_scanner_internal_start_scan (GT68xx_Scanner * scanner)
{
  DECLARE_FUNCTION_NAME ("gt68xx_scanner_internal_start_scan")
    SANE_Status status;
  SANE_Bool ready;
  SANE_Int repeat_count;

  status = gt68xx_scanner_wait_for_positioning (scanner);
  if (status != SANE_STATUS_GOOD)
    {
      XDBG ((5, "%s: gt68xx_scanner_wait_for_positioning error: %s\n",
	     function_name, sane_strstatus (status)));
      return status;
    }

  status = gt68xx_device_start_scan (scanner->dev);
  if (status != SANE_STATUS_GOOD)
    {
      XDBG ((5, "%s: gt68xx_device_start_scan error: %s\n",
	     function_name, sane_strstatus (status)));
      return status;
    }

  for (repeat_count = 0; repeat_count < 30 * 10; ++repeat_count)
    {
      status = gt68xx_device_read_scanned_data (scanner->dev, &ready);
      if (status != SANE_STATUS_GOOD)
	{
	  XDBG ((5, "%s: gt68xx_device_read_scanned_data error: %s\n",
		 function_name, sane_strstatus (status)));
	  return status;
	}
      if (ready)
	break;
      usleep (100000);
    }
  if (!ready)
    {
      XDBG ((5, "%s: scanner still not ready - giving up\n", function_name));
      return SANE_STATUS_DEVICE_BUSY;
    }

  status = gt68xx_device_read_start (scanner->dev);
  if (status != SANE_STATUS_GOOD)
    {
      XDBG ((5, "%s: gt68xx_device_read_start error: %s\n",
	     function_name, sane_strstatus (status)));
      return status;
    }

  return SANE_STATUS_GOOD;
}

static SANE_Status
gt68xx_scanner_start_scan_extended (GT68xx_Scanner * scanner,
				    GT68xx_Scan_Request * request,
				    GT68xx_Scan_Action action,
				    GT68xx_Scan_Parameters * params)
{
  DECLARE_FUNCTION_NAME ("gt68xx_scanner_start_scan_extended")
    SANE_Status status;
  GT68xx_AFE_Parameters *afe = scanner->dev->afe;

  status = gt68xx_scanner_wait_for_positioning (scanner);
  if (status != SANE_STATUS_GOOD)
    {
      XDBG ((5, "%s: gt68xx_scanner_wait_for_positioning error: %s\n",
	     function_name, sane_strstatus (status)));
      return status;
    }

  status = gt68xx_device_setup_scan (scanner->dev, request, action, params);
  if (status != SANE_STATUS_GOOD)
    {
      XDBG ((5, "%s: gt68xx_device_setup_scan failed: %s\n", function_name,
	     sane_strstatus (status)));
      return status;
    }

  status = gt68xx_line_reader_new (scanner->dev, params,
				   action == SA_SCAN ? SANE_TRUE : SANE_FALSE,
				   &scanner->reader);
  if (status != SANE_STATUS_GOOD)
    {
      XDBG ((5, "%s: gt68xx_line_reader_new failed: %s\n", function_name,
	     sane_strstatus (status)));
      return status;
    }

  if (scanner->dev->model->is_cis)
    {
      status =
	gt68xx_device_set_exposure_time (scanner->dev,
					 scanner->dev->exposure);
      if (status != SANE_STATUS_GOOD)
	{
	  XDBG ((5, "%s: gt68xx_device_set_exposure_time failed: %s\n",
		 function_name, sane_strstatus (status)));
	  return status;
	}
    }
  DBG (5,
       "gt68xx_start_scan_extended: afe: %02X %02X  %02X %02X  %02X %02X\n",
       afe->r_offset, afe->r_pga, afe->g_offset, afe->g_pga, afe->b_offset,
       afe->b_pga);

  status = gt68xx_device_set_afe (scanner->dev, afe);
  if (status != SANE_STATUS_GOOD)
    {
      XDBG ((5, "%s: gt68xx_device_set_afe failed: %s\n", function_name,
	     sane_strstatus (status)));
      return status;
    }

  status = gt68xx_scanner_internal_start_scan (scanner);

  if (status != SANE_STATUS_GOOD)
    {
      XDBG ((5, "%s: gt68xx_scanner_internal_start_scan failed: %s\n",
	     function_name, sane_strstatus (status)));
      return status;
    }

  return SANE_STATUS_GOOD;
}

static SANE_Status
gt68xx_scanner_create_calibrator (GT68xx_Scan_Parameters * params,
				  GT68xx_Calibrator ** cal_return)
{
  return gt68xx_calibrator_new (params->pixel_xs, 65535, cal_return);
}

static SANE_Status
gt68xx_scanner_create_color_calibrators (GT68xx_Scanner * scanner,
					 GT68xx_Scan_Parameters * params)
{
  SANE_Status status;

  if (!scanner->cal_r)
    {
      status = gt68xx_scanner_create_calibrator (params, &scanner->cal_r);
      if (status != SANE_STATUS_GOOD)
	return status;
    }
  if (!scanner->cal_g)
    {
      status = gt68xx_scanner_create_calibrator (params, &scanner->cal_g);
      if (status != SANE_STATUS_GOOD)
	return status;
    }
  if (!scanner->cal_b)
    {
      status = gt68xx_scanner_create_calibrator (params, &scanner->cal_b);
      if (status != SANE_STATUS_GOOD)
	return status;
    }

  return SANE_STATUS_GOOD;
}

static SANE_Status
gt68xx_scanner_create_gray_calibrators (GT68xx_Scanner * scanner,
					GT68xx_Scan_Parameters * params)
{
  SANE_Status status;

  if (!scanner->cal_gray)
    {
      status = gt68xx_scanner_create_calibrator (params, &scanner->cal_gray);
      if (status != SANE_STATUS_GOOD)
	return status;
    }

  return SANE_STATUS_GOOD;
}

static SANE_Status
gt68xx_scanner_calibrate_color_white_line (GT68xx_Scanner * scanner,
					   unsigned int **buffer_pointers)
{

  gt68xx_calibrator_add_white_line (scanner->cal_r, buffer_pointers[0]);
  gt68xx_calibrator_add_white_line (scanner->cal_g, buffer_pointers[1]);
  gt68xx_calibrator_add_white_line (scanner->cal_b, buffer_pointers[2]);

  return SANE_STATUS_GOOD;
}

static SANE_Status
gt68xx_scanner_calibrate_gray_white_line (GT68xx_Scanner * scanner,
					  unsigned int **buffer_pointers)
{
  gt68xx_calibrator_add_white_line (scanner->cal_gray, buffer_pointers[0]);

  return SANE_STATUS_GOOD;
}

static SANE_Status
gt68xx_scanner_calibrate_color_black_line (GT68xx_Scanner * scanner,
					   unsigned int **buffer_pointers)
{
  gt68xx_calibrator_add_black_line (scanner->cal_r, buffer_pointers[0]);
  gt68xx_calibrator_add_black_line (scanner->cal_g, buffer_pointers[1]);
  gt68xx_calibrator_add_black_line (scanner->cal_b, buffer_pointers[2]);

  return SANE_STATUS_GOOD;
}

static SANE_Status
gt68xx_scanner_calibrate_gray_black_line (GT68xx_Scanner * scanner,
					  unsigned int **buffer_pointers)
{
  gt68xx_calibrator_add_black_line (scanner->cal_gray, buffer_pointers[0]);

  return SANE_STATUS_GOOD;
}

SANE_Status
gt68xx_scanner_calibrate (GT68xx_Scanner * scanner,
			  GT68xx_Scan_Request * request)
{
  DECLARE_FUNCTION_NAME ("gt68xx_scanner_calibrate") SANE_Status status;
  GT68xx_Scan_Parameters params;
  GT68xx_Scan_Request req;
  SANE_Int i;
  unsigned int *buffer_pointers[3];
  GT68xx_AFE_Parameters *afe = scanner->dev->afe;
  GT68xx_Exposure_Parameters *exposure = scanner->dev->exposure;

  memcpy (&req, request, sizeof (req));

  gt68xx_scanner_free_calibrators (scanner);

  if (scanner->auto_afe)
    {
      if (scanner->dev->model->is_cis)
	status = gt68xx_afe_cis_auto (scanner);
      else
	status = gt68xx_afe_ccd_auto (scanner, request);

      if (status != SANE_STATUS_GOOD)
	{
	  XDBG ((5, "%s: gt68xx_set_gain failed: %s\n",
		 function_name, sane_strstatus (status)));
	  return status;
	}
      req.mbs = SANE_FALSE;
    }
  else
    req.mbs = SANE_TRUE;

  DBG (3, "afe 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n", afe->r_offset,
       afe->r_pga, afe->g_offset, afe->g_pga, afe->b_offset, afe->b_pga);
  DBG (3, "exposure 0x%02x 0x%02x 0x%02x\n", exposure->r_time,
       exposure->g_time, exposure->b_time);

  if (!scanner->calib)
    return SANE_STATUS_GOOD;

  req.mds = SANE_TRUE;
  req.mas = SANE_FALSE;

  if (scanner->dev->model->is_cis == SANE_TRUE)
    req.color = SANE_TRUE;

  if (req.use_ta)
    {
      req.lamp = SANE_FALSE;
      status =
	gt68xx_device_lamp_control (scanner->dev, SANE_FALSE, SANE_TRUE);
    }
  else
    {
      req.lamp = SANE_TRUE;
      status =
	gt68xx_device_lamp_control (scanner->dev, SANE_TRUE, SANE_FALSE);
    }

  status = gt68xx_scanner_start_scan_extended (scanner, &req, SA_CALIBRATE,
					       &params);
  if (status != SANE_STATUS_GOOD)
    {
      XDBG ((5, "%s: gt68xx_scanner_start_scan_extended failed: %s\n",
	     function_name, sane_strstatus (status)));
      return status;
    }

  if (params.color)
    status = gt68xx_scanner_create_color_calibrators (scanner, &params);
  else
    status = gt68xx_scanner_create_gray_calibrators (scanner, &params);

#if defined(SAVE_WHITE_CALIBRATION) || defined(SAVE_BLACK_CALIBRATION)
  printf ("P5\n%d %d\n255\n", params.pixel_xs, params.pixel_ys);
#endif
  for (i = 0; i < params.pixel_ys; ++i)
    {
      status = gt68xx_line_reader_read (scanner->reader, buffer_pointers);
      if (status != SANE_STATUS_GOOD)
	{
	  XDBG ((5, "%s: gt68xx_line_reader_read failed: %s\n",
		 function_name, sane_strstatus (status)));
	  return status;
	}

      if (params.color)
	status = gt68xx_scanner_calibrate_color_white_line (scanner,
							    buffer_pointers);
      else
	status = gt68xx_scanner_calibrate_gray_white_line (scanner,
							   buffer_pointers);

      if (status != SANE_STATUS_GOOD)
	{
	  XDBG ((5, "%s: calibration failed: %s\n",
		 function_name, sane_strstatus (status)));
	  return status;
	}
    }
  gt68xx_scanner_stop_scan (scanner);

  if (params.color)
    {
      gt68xx_calibrator_eval_white (scanner->cal_r, 1);
      gt68xx_calibrator_eval_white (scanner->cal_g, 1);
      gt68xx_calibrator_eval_white (scanner->cal_b, 1);
    }
  else
    {
      gt68xx_calibrator_eval_white (scanner->cal_gray, 1);
    }

  req.mbs = SANE_FALSE;
  req.mds = SANE_FALSE;
  req.mas = SANE_FALSE;
  req.lamp = SANE_FALSE;

  status = gt68xx_device_lamp_control (scanner->dev, SANE_FALSE, SANE_FALSE);
  if (status != SANE_STATUS_GOOD)
    {
      XDBG ((5, "%s: gt68xx_device_lamp_control failed: %s\n",
	     function_name, sane_strstatus (status)));
      return status;
    }

  if (!scanner->dev->model->is_cis)
    usleep (500000);
  status = gt68xx_scanner_start_scan_extended (scanner, &req, SA_CALIBRATE,
					       &params);
  if (status != SANE_STATUS_GOOD)
    {
      XDBG ((5, "%s: gt68xx_scanner_start_scan_extended failed: %s\n",
	     function_name, sane_strstatus (status)));
      return status;
    }

  for (i = 0; i < params.pixel_ys; ++i)
    {
      status = gt68xx_line_reader_read (scanner->reader, buffer_pointers);
      if (status != SANE_STATUS_GOOD)
	{
	  XDBG ((5, "%s: gt68xx_line_reader_read failed: %s\n",
		 function_name, sane_strstatus (status)));
	  return status;
	}

      if (params.color)
	status = gt68xx_scanner_calibrate_color_black_line (scanner,
							    buffer_pointers);
      else
	status = gt68xx_scanner_calibrate_gray_black_line (scanner,
							   buffer_pointers);

      if (status != SANE_STATUS_GOOD)
	{
	  XDBG ((5, "%s: calibration failed: %s\n",
		 function_name, sane_strstatus (status)));
	  return status;
	}
    }
  gt68xx_scanner_stop_scan (scanner);

  if (req.use_ta)
    status = gt68xx_device_lamp_control (scanner->dev, SANE_FALSE, SANE_TRUE);
  else
    status = gt68xx_device_lamp_control (scanner->dev, SANE_TRUE, SANE_FALSE);

  if (status != SANE_STATUS_GOOD)
    {
      XDBG ((5, "%s: gt68xx_device_lamp_control failed: %s\n",
	     function_name, sane_strstatus (status)));
      return status;
    }

  if (!scanner->dev->model->is_cis)
    usleep (500000);

  if (params.color)
    {
      gt68xx_calibrator_eval_black (scanner->cal_r, 0.0);
      gt68xx_calibrator_eval_black (scanner->cal_g, 0.0);
      gt68xx_calibrator_eval_black (scanner->cal_b, 0.0);

      gt68xx_calibrator_finish_setup (scanner->cal_r);
      gt68xx_calibrator_finish_setup (scanner->cal_g);
      gt68xx_calibrator_finish_setup (scanner->cal_b);
    }
  else
    {
      gt68xx_calibrator_eval_black (scanner->cal_gray, 0.0);
      gt68xx_calibrator_finish_setup (scanner->cal_gray);
    }

  return SANE_STATUS_GOOD;
}

SANE_Status
gt68xx_scanner_start_scan (GT68xx_Scanner * scanner,
			   GT68xx_Scan_Request * request,
			   GT68xx_Scan_Parameters * params)
{
  request->mbs = SANE_FALSE;	/* don't go home before real scan */
  request->mds = SANE_TRUE;
  request->mas = SANE_FALSE;
  if (request->use_ta)
    {
      gt68xx_device_lamp_control (scanner->dev, SANE_FALSE, SANE_TRUE);
      request->lamp = SANE_FALSE;
    }
  else
    {
      gt68xx_device_lamp_control (scanner->dev, SANE_TRUE, SANE_FALSE);
      request->lamp = SANE_TRUE;
    }
  if (!scanner->dev->model->is_cis)
    sleep (2);
  return gt68xx_scanner_start_scan_extended (scanner, request, SA_SCAN,
					     params);
}

SANE_Status
gt68xx_scanner_read_line (GT68xx_Scanner * scanner,
			  unsigned int **buffer_pointers)
{
  DECLARE_FUNCTION_NAME ("gt68xx_scanner_read_line") SANE_Status status;

  status = gt68xx_line_reader_read (scanner->reader, buffer_pointers);

  if (status != SANE_STATUS_GOOD)
    {
      XDBG ((5, "%s: gt68xx_line_reader_read failed: %s\n",
	     function_name, sane_strstatus (status)));
      return status;
    }

  if (scanner->calib)
    {
      if (scanner->reader->params.color)
	{
	  gt68xx_calibrator_process_line (scanner->cal_r, buffer_pointers[0]);
	  gt68xx_calibrator_process_line (scanner->cal_g, buffer_pointers[1]);
	  gt68xx_calibrator_process_line (scanner->cal_b, buffer_pointers[2]);
	}
      else
	{
	  if (scanner->dev->model->is_cis == SANE_TRUE)
	    gt68xx_calibrator_process_line (scanner->cal_g,
					    buffer_pointers[0]);
	  else
	    gt68xx_calibrator_process_line (scanner->cal_gray,
					    buffer_pointers[0]);
	}
    }

  return SANE_STATUS_GOOD;
}

SANE_Status
gt68xx_scanner_stop_scan (GT68xx_Scanner * scanner)
{
  gt68xx_line_reader_free (scanner->reader);
  scanner->reader = NULL;

  return gt68xx_device_stop_scan (scanner->dev);
}

/************************************************************************/
/*                                                                      */
/* AFE offset/gain automatic configuration                              */
/*                                                                      */
/************************************************************************/

typedef struct GT68xx_Afe_Values GT68xx_Afe_Values;

struct GT68xx_Afe_Values
{
  SANE_Int black;		/* minimum black (0-255) */
  SANE_Int white;		/* maximum of the average white segments (0-255) */
  SANE_Int total_white;		/* average white of the complete line (0-65536) */
  SANE_Int calwidth;
  SANE_Int callines;
  SANE_Int max_width;
  SANE_Int scan_dpi;
  SANE_Fixed start_black;
  SANE_Int offset_direction;
};

#ifndef NDEBUG
static void
gt68xx_afe_dump (SANE_String_Const phase, int i, GT68xx_AFE_Parameters * afe)
{
  XDBG ((5, "set afe %s %2d: RGB offset/pga: %02x %02x  %02x %02x  "
	 "%02x %02x\n",
	 phase, i, afe->r_offset, afe->r_pga, afe->g_offset, afe->g_pga,
	 afe->b_offset, afe->b_pga));
}
#endif /* not NDEBUG */

#ifndef NDEBUG
static void
gt68xx_afe_exposure_dump (SANE_String_Const phase, int i,
			  GT68xx_Exposure_Parameters * exposure)
{
  XDBG ((5, "set exposure %s %2d: RGB exposure time: %02x %02x %02x\n",
	 phase, i, exposure->r_time, exposure->g_time, exposure->b_time));
}
#endif /* not NDEBUG */

/************************************************************************/
/* CCD scanners                                                         */
/************************************************************************/

/** Calculate average black and maximum white
 *
 * This function is used for CCD scanners. The black mark to the left ist used
 * for the calculation of average black. The remaining calibration strip    
 * is used for searching the segment whose white average is the highest.
 *
 * @param values AFE values
 * @param buffer scanned line
 */
static void
gt68xx_afe_ccd_calc (GT68xx_Afe_Values * values, unsigned int *buffer)
{
  DECLARE_FUNCTION_NAME ("gt68xx_afe_ccd_calc") SANE_Int start_black;
  SANE_Int end_black;
  SANE_Int start_white;
  SANE_Int end_white;
  SANE_Int segment;
  SANE_Int segment_count;
  SANE_Int i, j;
  SANE_Int black_acc = 0;
  SANE_Int black_count = 0;
  SANE_Int max_white = 0;
  SANE_Int total_white = 0;

  /* set size of black mark and white segments */
  start_black =
    SANE_UNFIX (values->start_black) * values->scan_dpi / MM_PER_INCH;
  end_black = start_black + 1.0 * values->scan_dpi / MM_PER_INCH;	/* 1 mm */

  /* 5mm after mark */
  start_white = end_black + 5.0 * values->scan_dpi / MM_PER_INCH;
  end_white = values->calwidth;

  if (values->scan_dpi >= 300)
    segment = 50;
  else if (values->scan_dpi >= 75)
    segment = 15;
  else
    segment = 10;

  start_white = (start_white / segment) * segment;
  segment_count = (end_white - start_white) / segment;

  XDBG ((5,
	 "%s: dpi=%d, start_black=%d, end_black=%d, start_white=%d, end_white=%d\n",
	 function_name, values->scan_dpi, start_black, end_black, start_white,
	 end_white));

  /* calc avg black value */
  for (i = start_black; i < end_black; i++)
    {
      black_acc += buffer[i] >> 8;
      black_count++;
    }
  /* find segment with max average value */
  for (i = 0; i < segment_count; ++i)
    {
      unsigned int *segment_ptr = buffer + start_white + i * segment;
      SANE_Int avg_white = 0;
      for (j = 0; j < segment; ++j)
	{
	  avg_white += (segment_ptr[j] >> 8);
	  total_white += segment_ptr[j];
	}
      avg_white /= segment;
      if (avg_white > max_white)
	max_white = avg_white;
    }
  values->total_white = total_white / (segment_count * segment);
  values->black = black_acc / black_count;
  values->white = max_white;
  if (values->white < 50 || values->black > 150
      || values->white - values->black < 30)
    XDBG ((1, "%s: WARNING: max_white %3d   avg_black %3d\n",
	   function_name, values->white, values->black));
  else
    XDBG ((5, "%s: max_white %3d   avg_black %3d\n",
	   function_name, values->white, values->black));
}

static SANE_Bool
gt68xx_afe_ccd_adjust_offset_gain (GT68xx_Afe_Values * values,  
				   unsigned int *buffer, SANE_Byte * offset,
				   SANE_Byte * pga)
{
  SANE_Int black_low = 3, black_high = 18;
  SANE_Int white_low = 234, white_high = 252;
  SANE_Bool done = SANE_TRUE;

  gt68xx_afe_ccd_calc (values, buffer);

  if (values->white > white_high)
    {
      if (values->black > black_high)
	*offset += values->offset_direction;
      else if (values->black < black_low)
	(*pga)--;
      else
	{
	  *offset += values->offset_direction;
	  (*pga)--;
	}
      done = SANE_FALSE;
      goto finish;
    }
  else if (values->white < white_low)
    {
      if (values->black < black_low)
	*offset -= values->offset_direction;
      else if (values->black > black_high)
	(*pga)++;
      else
	{
	  *offset -= values->offset_direction;
	  (*pga)++;
	}
      done = SANE_FALSE;
      goto finish;
    }
  if (values->black > black_high)
    {
      if (values->white > white_high)
	*offset += values->offset_direction;
      else if (values->white < white_low)
	(*pga)++;
      else
	{
	  *offset += values->offset_direction;
	  (*pga)++;
	}
      done = SANE_FALSE;
      goto finish;
    }
  else if (values->black < black_low)
    {
      if (values->white < white_low)
	*offset -= values->offset_direction;
      else if (values->white > white_high)
	(*pga)--;
      else
	{
	  *offset -= values->offset_direction;
	  (*pga)--;
	}
      done = SANE_FALSE;
      goto finish;
    }
 finish:
  DBG (5, "%swhite=%d, black=%d, offset=%d, gain=%d\n",
       done ? "DONE: " : "", values->white, values->black, *offset, *pga);
  return done;

}

/** Select best AFE gain and offset parameters.
 *
 * This function must be called before the main scan to choose the best values
 * for the AFE gains and offsets.  It performs several one-line scans of the
 * calibration strip.
 *
 * @param scanner Scanner object.
 * @param orig_request Scan parameters.
 *
 * @returns
 * - #SANE_STATUS_GOOD - gain and offset setting completed successfully
 * - other error value - failure of some internal function
 */
static SANE_Status
gt68xx_afe_ccd_auto (GT68xx_Scanner * scanner,
		     GT68xx_Scan_Request * orig_request)
{
  DECLARE_FUNCTION_NAME ("gt68xx_afe_ccd_auto") SANE_Status status;
  GT68xx_Scan_Parameters params;
  GT68xx_Scan_Request request;
  int i;
  GT68xx_Afe_Values values;
  unsigned int *buffer_pointers[3];
  GT68xx_AFE_Parameters *afe = scanner->dev->afe;
  SANE_Bool done;
  SANE_Int last_white = 0;

  values.offset_direction = 1;
  if (scanner->dev->model->flags & GT68XX_FLAG_OFFSET_INV)
    values.offset_direction = -1;

  request.x0 = SANE_FIX (0.0);
  request.xs = scanner->dev->model->x_size;
  request.xdpi = 300;
  request.ydpi = 300;
  request.depth = 8;
  request.color = orig_request->color;
  request.mas = SANE_FALSE;
  request.mbs = SANE_FALSE;
  request.mds = SANE_TRUE;
  request.calculate = SANE_FALSE;
  request.use_ta = orig_request->use_ta;

  if (orig_request->use_ta)
    {
      gt68xx_device_lamp_control (scanner->dev, SANE_FALSE, SANE_TRUE);
      request.lamp = SANE_FALSE;
    }
  else
    {
      gt68xx_device_lamp_control (scanner->dev, SANE_TRUE, SANE_FALSE);
      request.lamp = SANE_TRUE;
    }

  /* read line */
  status = gt68xx_scanner_start_scan_extended (scanner, &request,
					       SA_CALIBRATE_ONE_LINE,
					       &params);
  if (status != SANE_STATUS_GOOD)
    {
      XDBG ((3, "%s: gt68xx_scanner_start_scan_extended failed: %s\n",
	     function_name, sane_strstatus (status)));
      return status;
    }
  values.scan_dpi = params.xdpi;
  values.calwidth = params.pixel_xs;
  values.max_width =
    (params.pixel_xs * scanner->dev->model->optical_xdpi) / params.xdpi;
  if (orig_request->use_ta)
    values.start_black = SANE_FIX (20.0);
  else
    values.start_black = scanner->dev->model->x_offset_mark;
  request.mds = SANE_FALSE;
  XDBG ((5, "%s: scan_dpi=%d, calwidth=%d, max_width=%d, "
	 "start_black=%.1f mm\n", function_name, values.scan_dpi,
	 values.calwidth, values.max_width, SANE_UNFIX (values.start_black)));

  status = gt68xx_line_reader_read (scanner->reader, buffer_pointers);
  if (status != SANE_STATUS_GOOD)
    {
      XDBG ((3, "%s: gt68xx_line_reader_read failed: %s\n",
	     function_name, sane_strstatus (status)));
      return status;
    }
  gt68xx_scanner_stop_scan (scanner);

  /* loop waiting for lamp to give stable brightness */
  for (i = 0; i < 80; i++)
    {
      usleep (200000);
      if (i == 10)
	DBG (0, "Please wait for lamp warm-up\n");

      /* read line */
      status = gt68xx_scanner_start_scan_extended (scanner, &request,
						   SA_CALIBRATE_ONE_LINE,
						   &params);
      if (status != SANE_STATUS_GOOD)
	{
	  XDBG ((3, "%s: gt68xx_scanner_start_scan_extended lamp brightness "
		 "failed: %s\n", function_name, sane_strstatus (status)));
	  return status;
	}
      status = gt68xx_line_reader_read (scanner->reader, buffer_pointers);
      if (status != SANE_STATUS_GOOD)
	{
	  XDBG ((3, "%s: gt68xx_line_reader_read failed: %s\n",
		 function_name, sane_strstatus (status)));
	  return status;
	}
      gt68xx_scanner_stop_scan (scanner);

      gt68xx_afe_ccd_calc (&values, buffer_pointers[0]);

      XDBG ((5, "%s: check lamp stable: this white = %d, last white = %d\n",
	     function_name, values.total_white, last_white));

      if (scanner->val[OPT_AUTO_WARMUP].w == SANE_TRUE)
	{
	  if (values.total_white <= (last_white + 20))
	    break;		/* lamp is warmed up */
	}
      else
	{			/* insist on 30 seconds */
	  struct timeval now;
	  int secs;

	  gettimeofday (&now, 0);
	  secs = now.tv_sec - scanner->lamp_on_time.tv_sec;
	  if (secs >= WARMUP_TIME)
	    break;
	}
      last_white = values.total_white;
    }

  i = 0;
  do
    {
      i++;
      IF_DBG (gt68xx_afe_dump ("scan", i, afe));
      /* read line */
      status = gt68xx_scanner_start_scan_extended (scanner, &request,
						   SA_CALIBRATE_ONE_LINE,
						   &params);
      if (status != SANE_STATUS_GOOD)
	{
	  XDBG ((3, "%s: gt68xx_scanner_start_scan_extended failed: %s\n",
		 function_name, sane_strstatus (status)));
	  return status;
	}

      status = gt68xx_line_reader_read (scanner->reader, buffer_pointers);
      if (status != SANE_STATUS_GOOD)
	{
	  XDBG ((3, "%s: gt68xx_line_reader_read failed: %s\n",
		 function_name, sane_strstatus (status)));
	  return status;
	}

      if (params.color)
	{
	  done =
	    gt68xx_afe_ccd_adjust_offset_gain (&values, buffer_pointers[0],
						 &afe->r_offset, &afe->r_pga);
	  done &=
	    gt68xx_afe_ccd_adjust_offset_gain (&values, buffer_pointers[1],
						 &afe->g_offset, &afe->g_pga);
	  done &=
	    gt68xx_afe_ccd_adjust_offset_gain (&values, buffer_pointers[2],
						 &afe->b_offset, &afe->b_pga);
	}
      else
	{
	  done =
	    gt68xx_afe_ccd_adjust_offset_gain (&values, buffer_pointers[0],
						 &afe->g_offset, &afe->g_pga);
	}

      gt68xx_scanner_stop_scan (scanner);
    }
  while (!done && i < 100);

  return status;
}

/************************************************************************/
/* CIS scanners                                                         */
/************************************************************************/


static void
gt68xx_afe_cis_calc_black (GT68xx_Afe_Values * values,
			   unsigned int *black_buffer)
{
  DECLARE_FUNCTION_NAME ("gt68xx_afe_cis_calc_black") SANE_Int start_black;
  SANE_Int end_black;
  SANE_Int i, j;
  SANE_Int min_black = 255;

  start_black = 0;
  end_black = values->calwidth;

  /* find min average black value */
  for (i = start_black; i < end_black; ++i)
    {
      SANE_Int avg_black = 0;
      for (j = 0; j < values->callines; j++)
	avg_black += (*(black_buffer + i + j * values->calwidth) >> 8);
      avg_black /= values->callines;
      if (avg_black < min_black)
	min_black = avg_black;
    }
  values->black = min_black;
  XDBG ((5, "%s: min_black=%02x\n", function_name, values->black));
}

static void
gt68xx_afe_cis_calc_white (GT68xx_Afe_Values * values,
			   unsigned int *white_buffer)
{
  DECLARE_FUNCTION_NAME ("gt68xx_afe_cis_calc_white") SANE_Int start_white;
  SANE_Int end_white;
  SANE_Int i, j;
  SANE_Int max_white = 0;

  start_white = 0;
  end_white = values->calwidth;

  /* find max average white value */
  for (i = start_white; i < end_white; ++i)
    {
      SANE_Int avg_white = 0;
      for (j = 0; j < values->callines; j++)
	avg_white += (*(white_buffer + i + j * values->calwidth) >> 8);
      avg_white /= values->callines;
      if (avg_white > max_white)
	max_white = avg_white;
    }
  values->white = max_white;
  XDBG ((5, "%s: max_white=%02x\n", function_name, values->white));
}

static SANE_Bool
gt68xx_afe_cis_adjust_offset (GT68xx_Afe_Values * values,
			      unsigned int *black_buffer,
			      SANE_Byte * offset)
{
  SANE_Int offs = 0, tmp_offset = *offset;
  SANE_Int low = 8, high = 22;

  gt68xx_afe_cis_calc_black (values, black_buffer);
  if (values->black < low)
    {
      offs = (values->offset_direction * (low - values->black) / 4);
      if (offs == 0)
	offs = values->offset_direction;
      DBG (5, "black = %d (too low) --> offs = %d\n", values->black, offs);
    }
  else if (values->black > high)
    {
      offs = -(values->offset_direction * (values->black - high) / 7);
      if (offs == 0)
	offs = -values->offset_direction;
      DBG (5, "black = %d (too high) --> offs = %d\n", values->black, offs);
    }
  else
    {
      DBG (5, "black = %d (ok)\n", values->black);
    }

  if (offs == 0)
    return SANE_TRUE;

  tmp_offset += offs;
  if (tmp_offset < 0)
    tmp_offset = 0;
  if (tmp_offset > 63)
    tmp_offset = 63;
  *offset = tmp_offset;
  return SANE_FALSE;
}

static SANE_Bool
gt68xx_afe_cis_adjust_gain (GT68xx_Afe_Values * values,
			    unsigned int *white_buffer, SANE_Byte * gain)
{
  SANE_Int g = *gain;

  gt68xx_afe_cis_calc_white (values, white_buffer);

  if (values->white < 235)
    {
      g += 1;
      DBG (5, "white = %d (too low) --> gain += 1\n", values->white);
    }
  else if (values->white > 250)
    {
      g -= 1;
      DBG (5, "white = %d (too high) --> gain -= 1\n", values->white);
    }
  else
    {
      DBG (5, "white = %d (ok)\n", values->white);
    }
  if (g < 0)
    g = 0;
  if (g > 63)
    g = 63;

  if (g == *gain)
    return SANE_TRUE;
  *gain = g;
  return SANE_FALSE;
}

static SANE_Bool
gt68xx_afe_cis_adjust_exposure (GT68xx_Afe_Values * values,
				unsigned int *white_buffer,
				SANE_Int border, SANE_Int * exposure_time)
{
  gt68xx_afe_cis_calc_white (values, white_buffer);
  if (values->white < border)
    {
      *exposure_time += ((border - values->white) * 2);
      DBG (5, "white = %d (too low) --> += %d\n",
	   values->white, ((border - values->white) * 2));
      return SANE_FALSE;
    }
  else if (values->white > border + 10)
    {
      *exposure_time -= ((values->white - (border + 10)) * 2);
      DBG (5, "white = %d (too high) --> -= %d\n",
	   values->white, ((values->white - (border + 10)) * 2));
      return SANE_FALSE;
    }
  else
    {
      DBG (5, "white = %d (ok)\n", values->white);
    }
  return SANE_TRUE;
}

static SANE_Status
gt68xx_afe_cis_read_lines (GT68xx_Afe_Values * values,
			   GT68xx_Scanner * scanner, SANE_Bool lamp,
			   SANE_Bool first, unsigned int *r_buffer,
			   unsigned int *g_buffer, unsigned int *b_buffer)
{
  DECLARE_FUNCTION_NAME ("gt68xx_afe_cis_read_lines") SANE_Status status;
  int line;
  unsigned int *buffer_pointers[3];
  GT68xx_Scan_Request request;
  GT68xx_Scan_Parameters params;

  request.x0 = SANE_FIX (0.0);
  request.xs = scanner->dev->model->x_size;
  request.xdpi = 300;
  request.ydpi = 300;
  request.depth = 8;
  request.color = SANE_TRUE;
  request.mas = SANE_FALSE;
  request.calculate = SANE_FALSE;
  request.use_ta = SANE_FALSE;

  if (first)			/* go home */
    {
      request.mbs = SANE_TRUE;
      request.mds = SANE_TRUE;
    }
  else
    {
      request.mbs = SANE_FALSE;
      request.mds = SANE_FALSE;
    }
  request.lamp = lamp;

  if (!r_buffer)		/* First, set the size parameters */
    {
      request.calculate = SANE_TRUE;
      RIE (gt68xx_device_setup_scan
	   (scanner->dev, &request, SA_CALIBRATE_ONE_LINE, &params));
      values->scan_dpi = params.xdpi;
      values->calwidth = params.pixel_xs;
      values->callines = params.pixel_ys;
      return SANE_STATUS_GOOD;
    }

  status =
    gt68xx_scanner_start_scan_extended (scanner, &request,
					SA_CALIBRATE_ONE_LINE, &params);
  if (status != SANE_STATUS_GOOD)
    {
      XDBG ((5, "%s: gt68xx_scanner_start_scan_extended failed: %s\n",
	     function_name, sane_strstatus (status)));
      return status;
    }
  values->scan_dpi = params.xdpi;
  values->calwidth = params.pixel_xs;
  values->callines = params.pixel_ys;

  if (r_buffer && g_buffer && b_buffer)
    for (line = 0; line < values->callines; line++)
      {
	status = gt68xx_line_reader_read (scanner->reader, buffer_pointers);
	if (status != SANE_STATUS_GOOD)
	  {
	    XDBG ((5, "%s: gt68xx_line_reader_read failed: %s\n",
		   function_name, sane_strstatus (status)));
	    return status;
	  }
	memcpy (r_buffer + values->calwidth * line, buffer_pointers[0],
		values->calwidth * sizeof (unsigned int));
	memcpy (g_buffer + values->calwidth * line, buffer_pointers[1],
		values->calwidth * sizeof (unsigned int));
	memcpy (b_buffer + values->calwidth * line, buffer_pointers[2],
		values->calwidth * sizeof (unsigned int));
      }

  status = gt68xx_scanner_stop_scan (scanner);
  if (status != SANE_STATUS_GOOD)
    {
      XDBG ((5, "%s: gt68xx_scanner_stop_scan failed: %s\n",
	     function_name, sane_strstatus (status)));
      return status;
    }

  return SANE_STATUS_GOOD;
}

static SANE_Status
gt68xx_afe_cis_auto (GT68xx_Scanner * scanner)
{
  DECLARE_FUNCTION_NAME ("gt68xx_afe_cis_auto") SANE_Status status;
  int total_count, offset_count, exposure_count;
  GT68xx_Afe_Values values;
  GT68xx_AFE_Parameters *afe = scanner->dev->afe;
  GT68xx_Exposure_Parameters *exposure = scanner->dev->exposure;
  SANE_Int done;
  SANE_Bool first = SANE_TRUE;
  unsigned int *r_buffer = 0, *g_buffer = 0, *b_buffer = 0;

  XDBG ((5, "%s: start\n", function_name));

  RIE (gt68xx_afe_cis_read_lines (&values, scanner, SANE_FALSE, SANE_FALSE,
				  r_buffer, g_buffer, b_buffer));

  r_buffer =
    malloc (values.calwidth * values.callines * sizeof (unsigned int));
  g_buffer =
    malloc (values.calwidth * values.callines * sizeof (unsigned int));
  b_buffer =
    malloc (values.calwidth * values.callines * sizeof (unsigned int));
  if (!r_buffer || !g_buffer || !b_buffer)
    return SANE_STATUS_NO_MEM;

  total_count = 0;
  do
    {
      offset_count = 0;
      values.offset_direction = 1;
      if (scanner->dev->model->flags & GT68XX_FLAG_OFFSET_INV)
	values.offset_direction = -1;
      exposure->r_time = exposure->g_time = exposure->b_time = 0x157;
      do
	{
	  /* AFE offset */
	  IF_DBG (gt68xx_afe_dump ("offset", total_count, afe));

	  /* read black line */
	  RIE (gt68xx_afe_cis_read_lines (&values, scanner, SANE_FALSE, first,
					  r_buffer, g_buffer, b_buffer));

	  done =
	    gt68xx_afe_cis_adjust_offset (&values, r_buffer, &afe->r_offset);
	  done &=
	    gt68xx_afe_cis_adjust_offset (&values, g_buffer, &afe->g_offset);
	  done &=
	    gt68xx_afe_cis_adjust_offset (&values, b_buffer, &afe->b_offset);

	  offset_count++;
	  total_count++;
	  first = SANE_FALSE;
	}
      while (offset_count < 10 && !done);

      /* AFE gain */
      IF_DBG (gt68xx_afe_dump ("gain", total_count, afe));

      /* read white line */
      RIE (gt68xx_afe_cis_read_lines (&values, scanner, SANE_TRUE, SANE_FALSE,
				      r_buffer, g_buffer, b_buffer));

      done = gt68xx_afe_cis_adjust_gain (&values, r_buffer, &afe->r_pga);
      done &= gt68xx_afe_cis_adjust_gain (&values, g_buffer, &afe->g_pga);
      done &= gt68xx_afe_cis_adjust_gain (&values, b_buffer, &afe->b_pga);

      total_count++;
    }
  while (total_count < 100 && !done);

  IF_DBG (gt68xx_afe_dump ("final", total_count, afe));

  /* Exposure time */
  exposure_count = 0;
  do
    {
      IF_DBG (gt68xx_afe_exposure_dump ("exposure", total_count, exposure));

      /* read white line */
      RIE (gt68xx_afe_cis_read_lines (&values, scanner, SANE_TRUE, SANE_FALSE,
				      r_buffer, g_buffer, b_buffer));
      done = gt68xx_afe_cis_adjust_exposure (&values, r_buffer, 230,
					     &exposure->r_time);
      done &= gt68xx_afe_cis_adjust_exposure (&values, g_buffer, 230,
					      &exposure->g_time);
      done &= gt68xx_afe_cis_adjust_exposure (&values, b_buffer, 230,
					      &exposure->b_time);
      exposure_count++;
      total_count++;
    }
  while (!done && exposure_count < 10);

  free (r_buffer);
  free (g_buffer);
  free (b_buffer);
  XDBG ((4, "%s: total_count: %d\n", function_name, total_count));

  return SANE_STATUS_GOOD;
}


/* vim: set sw=2 cino=>2se-1sn-1s{s^-1st0(0u0 smarttab expandtab: */
