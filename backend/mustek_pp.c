/* sane - Scanner Access Now Easy.
   Copyright (C) 2000 Jochen Eisinger <jochen.eisinger@gmx.net>
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

   This file implements a SANE backend for Mustek PP flatbed scanners.  */

#include "../include/sane/config.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include <sys/time.h>
#include <sys/types.h>

#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"
#include "../include/sane/saneopts.h"
#include "../include/sane/sanei_pa4s2.h"

#include "mustek_pp.h"

#define BACKEND_NAME	mustek_pp
#include "../include/sane/sanei_backend.h"

#include "../include/sane/sanei_config.h"
#define MUSTEK_PP_CONFIG_FILE "mustek_pp.conf"

#define MIN(a,b)	((a) < (b) ? (a) : (b))

/* #define if you want to patch for ASIC 1505 */
/* #undef	PATCH_MUSTEK_PP_1505 */

/* #define if you want authorization support */
/* please note:
 * 	to use user authorization, you need to modify
 * 		a) SANED: the protocoll doesn't realy support auth
 * 		b) NET: ...nor does the backend
 * 		c) your frontend: at least (x)scanimage doesn't support it
 * 		d) the makefiles: libcrypt is needed for crypt()
 *
 * 	-> so, don't do it...
 *
 * 	the format of the file .auth is
 * 	ressource:user:password
 * 	ressource:user:password
 * 	...
 *
 * 	where password is encoded using crypt()
 * 	
 * 	please note, that the password is passed to the backend in plain text
 * 	
 */
/* #undef	HAVE_AUTHORIZATION */

/* #define for invert option
 * inversion doesn't really work */
/* #undef	HAVE_INVERSION */

/* TODO:
 * 	- perhaps option bw should be set at runtime (i.e. SANE_OPT_TRESHOLD)
 * 	- remove line ending handling code
 * 	- test with 1015/ CCD 04&05 scanners
 * 	- add 1505 code
 * 	- sane_read should operate on a fd
 * 	- there is an transparency adapter -> support it
 * 	- make it possible to turn lamp off during scan
 * 	- add API docu (what's this?)
 * 	- make the user authentification work
 * 	- make use of ppdev
 *
 * The primary goal at the moment is to support ASIC 1015 - CCD 01
 */

/* DEBUG
 * 	for debug output, set SANE_DEBUG_MUSTEK_PP to
 * 		0	for nothing
 * 		1	for errors
 * 		2	for warnings
 * 		3	for additional information
 * 		4	for debug information
 * 		5	for code flow protocoll (there isn't any)
 * 		6	old debug info
 * 		129	if you want to know which parameters are unused
 */

/* history:
 *  1.0.0-devel		SANE backend structure taken from mustek backend
 *  1.0.1-devel		hardware functions taken from various sources
 *  1.0.2-devel		ld correction works now for ASIC 1015
 *  1.0.3-alpha		code tested for ASIC 1015 CCD-Type 0
 *  1.0.4-devel		added lots of options
 *  1.0.5-devel		removed AUTO options, added option niceload & auth
 *  			fixed bug in set_ccd_channel_1013
 *  1.0.6-alpha		fixed bug in sane_cancel, changed return_home
 *  			fixed bug in return_home_1013, added option wait-lamp
 *  			added motor control codes for ASIC 1015 CCD 01
 *  			fixed bug in config_ccd_1013 & config_ccd_1015
 *  			added resolution code to attach & sane_get_parameters
 *  1.0.7-devel		added functions common the both 101x chipsets
 *  			added option bw, fixed bug in sane_open
 *  			fixed bug in sane_exit
 *  1.0.8-devel		added some code to config_ccd_1015 & set_dpi_value
 *  			replaced fgets by sanei_config_read
 *  			replaced #include <sane/...> by #include "sane/..."
 *  			added option use600
 *  1.0.9-devel		added tons off 600 dpi code: kind of works for
 *  			ASIC 1015 CCD 01 scanners (grayscale)
 *  			option preview defaults to color mode
 *  			replaced #include "sane/..." by
 *  			#include "../include/sane/..."
 */

/* if you change the source, please set MUSTEK_PP_STATE to "devel". Do *not*
 * change the MUSTEK_PP_BUILD. */
#define MUSTEK_PP_BUILD	9
#define MUSTEK_PP_STATE	"devel"

static int num_devices = 0;

static Mustek_PP_Descriptor *devlist = NULL;
static SANE_Device **devarray = NULL;

static Mustek_PP_Device *first_dev = NULL;

static int strip_height = 0;
static unsigned int wait_bank = 700;

/* 1 Meg scan buffer */
static long int buf_size = 1024 * 1024;

static int niceload = 0;

static int wait_lamp = 5;

static int bw = 127;

#ifdef HAVE_AUTHORIZATION
static SANE_Auth_Callback auth_callback;
#endif

/* use600 test vars */
/* static char motor_command=0x43; */
/* static int first_time=1; */
/* static char unk1= 0xAA; */
/* static char expose_time = 0x64; */
/* static char volt[3]; */
/*static int do_calib=0; */
static void config_ccd_101x (Mustek_PP_Device * dev);
static void config_ccd (Mustek_PP_Device * dev);
static void lamp (Mustek_PP_Device * dev, int lamp_on);
/* end of test vars */

static const SANE_String_Const mode_list[] = {
  "Lineart", "Gray", "Color", 0
};

static const SANE_Range u8_range = {
  0,				/* minimum */
  255,				/* maximum */
  0				/* quantization */
};


#define MUSTEK_PP_CHANNEL_RED		0
#define MUSTEK_PP_CHANNEL_GREEN		1
#define MUSTEK_PP_CHANNEL_BLUE		2
#define MUSTEK_PP_CHANNEL_GRAY		1

#define MUSTEK_PP_STATE_SCANNING	2
#define MUSTEK_PP_STATE_CANCELLED	1
#define MUSTEK_PP_STATE_IDLE		0

#define MUSTEK_PP_MODE_LINEART		0
#define MUSTEK_PP_MODE_GRAYSCALE	1
#define MUSTEK_PP_MODE_COLOR		2

#define MM_PER_INCH			25.4
#define MM_TO_PIXEL(mm, res)	(SANE_UNFIX(mm) * (float )res / MM_PER_INCH)
#define PIXEL_TO_MM(px, res)	(SANE_FIX((float )(px * MM_PER_INCH / (res / 10)) / 10.0))

/* scan area is mixture of 8.5" x 11" (letter) and 8.2" x 11.6" (a4) */
/* actually it is 8.7" x 11.6" ;-) */ 
#define MUSTEK_PP_101x_MAX_H_PIXEL	2600
#define MUSTEK_PP_101x_MAX_V_PIXEL	3500

#define MUSTEK_PP_DEFAULT_PORT		0x378


#define MUSTEK_PP_ASIC_1013	0xA8
#define MUSTEK_PP_ASIC_1015	0xA5
#define MUSTEK_PP_ASIC_1505	0xA2


/*
 *  Here starts the driver code for the different chipsets
 *
 *  The 1013 & 1015 chipsets share large portions of the code. This
 *  shared functions end with _101x.
 */

static const u_char chan_codes_1013[] = { 0x82, 0x42, 0xC2 };
static const u_char chan_codes_1015[] = { 0x80, 0x40, 0xC0 };
static const u_char fullstep[] = { 0x09, 0x0C, 0x06, 0x03 };
static const u_char halfstep[] = { 0x02, 0x03, 0x01, 0x09,
  0x08, 0x0C, 0x04, 0x06
};
static const u_char voltages[4][3] = { {0x5C, 0x5A, 0x63},
{0xE6, 0xB4, 0xBE},
{0xB4, 0xB4, 0xB4},
{0x64, 0x50, 0x64}
};

/* Forward declarations of 1013/1015 functions */
static void set_ccd_channel_1013 (Mustek_PP_Device * dev, int channel);
static void motor_backward_1013 (Mustek_PP_Device * dev);
static void return_home_1013 (Mustek_PP_Device * dev, SANE_Bool nowait);
static void motor_forward_1013 (Mustek_PP_Device * dev);
static void config_ccd_1013 (Mustek_PP_Device * dev);

static void set_ccd_channel_1015 (Mustek_PP_Device * dev, int channel);
/* static void motor_backward_1015 (Mustek_PP_Device * dev); */
static void return_home_1015 (Mustek_PP_Device * dev, SANE_Bool nowait);
static void motor_forward_1015 (Mustek_PP_Device * dev);
static void config_ccd_1015 (Mustek_PP_Device * dev);


/* These functions are common to all 1013/1015 chipsets */

/*
static void
set_led (Mustek_PP_Device * dev)
{

  sanei_pa4s2_writebyte (dev->fd, 6,
			 (dev->motor_step % 5 == 0 ? 0x03 : 0x13));

}*/

#define set_led(dev)	sanei_pa4s2_writebyte (dev->fd, 6, \
			(dev->motor_step % 5 == 0 ? 0x03 : 0x13))

/*
static void
set_sti (Mustek_PP_Device * dev)
{

  sanei_pa4s2_writebyte (dev->fd, 3, 0);
  dev->bank_count++;
  dev->bank_count &= 7;

}*/

#define set_sti(dev)	do { \
			if (dev->desc->use600 == SANE_TRUE) \
			  sanei_pa4s2_writebyte(dev->fd, 3, 0xFF); \
			else \
			  sanei_pa4s2_writebyte (dev->fd, 3, 0); \
			dev->bank_count++; \
			dev->bank_count &= 7; \
			} while (0)
/*
static void
get_bank_count (Mustek_PP_Device * dev)
{

  u_char val;

  sanei_pa4s2_readbegin (dev->fd, 3);
  sanei_pa4s2_readbyte (dev->fd, &val);
  sanei_pa4s2_readend (dev->fd);

  dev->bank_count = (val & 0x07);

}*/
#define get_bank_count(dev)	do { \
				sanei_pa4s2_readbegin (dev->fd, 3); \
				sanei_pa4s2_readbyte \
					(dev->fd, \
					 (u_char *)&dev->bank_count); \
				sanei_pa4s2_readend (dev->fd); \
				dev->bank_count &= 0x07; \
				} while (0)

/*
static void
reset_bank_count (Mustek_PP_Device * dev)
{

  sanei_pa4s2_writebyte (dev->fd, 6, 7);

}*/
#define reset_bank_count(dev)	sanei_pa4s2_writebyte (dev->fd, 6, 7)


static void
wait_bank_change (Mustek_PP_Device * dev, int bankcount)
{
  struct timeval start, end;
  unsigned long diff;
  int firsttime = 1;

  if (dev->desc->use600)
    {

      if (niceload)
	usleep (2);

      return;

    }

  gettimeofday (&start, NULL);

  do
    {
      if (niceload)
	{
	  if (firsttime)
	    firsttime = 0;
	  else
	    usleep (10);	/* for a little nicer load */
	}
      get_bank_count (dev);

      gettimeofday (&end, NULL);
      diff = (end.tv_sec * 1000 + end.tv_usec / 1000) -
	(start.tv_sec * 1000 + start.tv_usec / 1000);

    }
  while ((dev->bank_count != bankcount) && (diff < dev->desc->wait_bank));

}

static void
set_dpi_value (Mustek_PP_Device * dev)
{
  u_char val = 0;

  sanei_pa4s2_writebyte (dev->fd, 6, 0x80);

  switch (dev->CCD.hwres * (600 / dev->desc->max_res))
    {
    case 100:
      val = 0x08;
      break;
    case 200:
      val = 0x00;
      break;
    case 300:
      val = 0x50;
      break;
    case 400:
      val = 0x10;
      break;
    case 600:
      val = 0x20;
      break;
      /*
         case 100: val=0x00; break;
         case 200: val=0x10; break;
         case 300: val=0x20; break; */
    }


  if (dev->ccd_type == 1)
    val |= 0x01;

  sanei_pa4s2_writebyte (dev->fd, 5, val);

  sanei_pa4s2_writebyte (dev->fd, 6, 0x00);

  DBG (4, "set_dpi_value: value 0x%02x\n", val);

}

static void
set_line_adjust (Mustek_PP_Device * dev)
{
  int adjustline;



  if (dev->desc->use600 == SANE_FALSE)
    {
      adjustline =
	(dev->BottomX - dev->TopX) * dev->CCD.hwres / dev->desc->max_res;
      dev->CCD.adjustskip =
	dev->CCD.adjustskip * dev->CCD.hwres / dev->desc->max_res;
    }
  else
    {

      adjustline = dev->params.pixels_per_line;

      adjustline = adjustline * dev->CCD.hwres / dev->CCD.res;

      if (dev->CCD.mode == MUSTEK_PP_MODE_COLOR)
	adjustline <<= 3;

/*	  adjustline = adjustline * dev->CCD.hwres / dev->CCD.res; */
      adjustline += 2;

      dev->CCD.adjustskip =
	dev->CCD.adjustskip * dev->CCD.hwres / dev->CCD.res;

      DBG (4, "set_line_adjust: value = %u (0x%04x)\n", adjustline,
	   adjustline);

    }

  DBG (4, "set_line_adjust: ppl %u (%u), adjust %u, skip %u\n",
       dev->params.pixels_per_line, (dev->BottomX - dev->TopX), adjustline,
       dev->CCD.adjustskip);


  sanei_pa4s2_writebyte (dev->fd, 6, 0x11);
  sanei_pa4s2_writebyte (dev->fd, 5, (adjustline + dev->CCD.adjustskip) >> 8);
  sanei_pa4s2_writebyte (dev->fd, 6, 0x21);
  sanei_pa4s2_writebyte (dev->fd, 5,
			 (adjustline + dev->CCD.adjustskip) & 0xFF);

  sanei_pa4s2_writebyte (dev->fd, 6, 0x01);

}

static void
set_lamp (Mustek_PP_Device * dev, int lamp_on)
{

  int ctr;

  if (dev->desc->use600 == SANE_FALSE)
    sanei_pa4s2_writebyte (dev->fd, 6, 0xC3);

  for (ctr = 0; ctr < 3; ctr++)
    {
      sanei_pa4s2_writebyte (dev->fd, 6, (lamp_on ? 0x47 : 0x57));
      if (dev->desc->use600 == SANE_FALSE)
	sanei_pa4s2_writebyte (dev->fd, 6, 0x77);
    }

  dev->motor_step = lamp_on;

  if (dev->desc->use600 == SANE_FALSE)
    set_led (dev);

}

static void
send_voltages (Mustek_PP_Device * dev)
{

  int voltage, sel = 8, ctr;

  switch (dev->ccd_type)
    {
    case 0:
      voltage = 0;
      break;
    case 1:
      voltage = 1;
/*      if (dev->asic_id == MUSTEK_PP_ASIC_1015)
	voltage = 3; */
      break;
    default:
      voltage = 2;
      break;
    }

  for (ctr = 0; ctr < 3; ctr++)
    {

      sel <<= 1;
      sanei_pa4s2_writebyte (dev->fd, 6, sel);
      sanei_pa4s2_writebyte (dev->fd, 5, voltages[voltage][ctr]);

    }

  sanei_pa4s2_writebyte (dev->fd, 6, 0x00);

}

int
compar (const void *a, const void *b)
{
  return (signed int) (*(const SANE_Byte *) a) -
    (signed int) (*(const SANE_Byte *) b);
}

static void
set_ccd_channel_101x (Mustek_PP_Device * dev, int channel)
{
  switch (dev->asic_id)
    {
    case MUSTEK_PP_ASIC_1013:
      set_ccd_channel_1013 (dev, channel);
      break;

    case MUSTEK_PP_ASIC_1015:
      set_ccd_channel_1015 (dev, channel);
      break;
    }
}

static void
motor_forward_101x (Mustek_PP_Device * dev)
{
  switch (dev->asic_id)
    {
    case MUSTEK_PP_ASIC_1013:
      motor_forward_1013 (dev);
      break;

    case MUSTEK_PP_ASIC_1015:
      motor_forward_1015 (dev);
      break;
    }
}

static void
motor_backward_101x (Mustek_PP_Device * dev)
{
  switch (dev->asic_id)
    {
    case MUSTEK_PP_ASIC_1013:
      motor_backward_1013 (dev);
      break;

    case MUSTEK_PP_ASIC_1015:
/*      motor_backward_1015 (dev); */
      break;
    }
}

static void
move_motor_101x (Mustek_PP_Device * dev, int forward)
{
  if (forward == SANE_TRUE)
    motor_forward_101x (dev);
  else
    motor_backward_101x (dev);

  wait_bank_change (dev, dev->bank_count);
  reset_bank_count (dev);
}


static void
config_ccd_101x (Mustek_PP_Device * dev)
{
  switch (dev->asic_id)
    {
    case MUSTEK_PP_ASIC_1013:
      config_ccd_1013 (dev);
      break;

    case MUSTEK_PP_ASIC_1015:
      config_ccd_1015 (dev);
      break;
    }
}



static void
read_line_101x (Mustek_PP_Device * dev, SANE_Byte * buf, SANE_Int pixel,
		SANE_Int RefBlack, SANE_Byte * calib, SANE_Int * gamma)
{

  SANE_Byte *cal = calib;
  u_char color;
  int ctr, skips = dev->CCD.adjustskip + 1, cval;

  if (pixel <= 0)
    return;

  sanei_pa4s2_readbegin (dev->fd, 1);


  if (dev->desc->use600 == SANE_TRUE)
    {


      if (dev->CCD.hwres == dev->CCD.res)
	{

	  while (skips--)
	    sanei_pa4s2_readbyte (dev->fd, &color);

	  for (ctr = 0; ctr < pixel; ctr++)
	    sanei_pa4s2_readbyte (dev->fd, &buf[ctr]);

	}
      else
	{

	  int pos = 0, bpos = 0;

	  while (skips--)
	    sanei_pa4s2_readbyte (dev->fd, &color);

	  ctr = 0;

	  do
	    {

	      sanei_pa4s2_readbyte (dev->fd, &color);

	      if (ctr < (pos >> SANE_FIXED_SCALE_SHIFT))
		{
		  ctr++;
		  continue;
		}

	      ctr++;
	      pos += dev->CCD.res_step;


	      buf[bpos++] = color;

	    }
	  while (bpos < pixel);

	}

      sanei_pa4s2_readend (dev->fd);

      return;

    }

  if (dev->CCD.hwres == dev->CCD.res)
    {

      while (skips--)
	sanei_pa4s2_readbyte (dev->fd, &color);

      for (ctr = 0; ctr < pixel; ctr++)
	{

	  sanei_pa4s2_readbyte (dev->fd, &color);

	  cval = color;

	  if (cval < RefBlack)
	    cval = 0;
	  else
	    cval -= RefBlack;

	  if (cal)
	    {
	      if (cval >= cal[ctr])
		cval = 0xFF;
	      else
		{
		  cval <<= 8;
		  cval /= (int) cal[ctr];
		}
	    }

	  if (gamma)
	    cval = gamma[cval];

	  buf[ctr] = cval;

	}

    }
  else
    {

      int pos = 0, bpos = 0;

      while (skips--)
	sanei_pa4s2_readbyte (dev->fd, &color);

      ctr = 0;

      do
	{

	  sanei_pa4s2_readbyte (dev->fd, &color);

	  cval = color;

	  if (ctr < (pos >> SANE_FIXED_SCALE_SHIFT))
	    {
	      ctr++;
	      continue;
	    }

	  ctr++;
	  pos += dev->CCD.res_step;


	  if (cval < RefBlack)
	    cval = 0;
	  else
	    cval -= RefBlack;

	  if (cal)
	    {
	      if (cval >= cal[bpos])
		cval = 0xFF;
	      else
		{
		  cval <<= 8;
		  cval /= (int) cal[bpos];
		}
	    }

	  if (gamma)
	    cval = gamma[cval];

	  buf[bpos++] = cval;

	}
      while (bpos < pixel);

    }

  sanei_pa4s2_readend (dev->fd);

}

static void
read_average_line_101x (Mustek_PP_Device * dev, SANE_Byte * buf, int pixel,
			int RefBlack)
{

  SANE_Byte lbuf[4][MUSTEK_PP_101x_MAX_H_PIXEL * 2];
  int ctr, sum;

  for (ctr = 0; ctr < 4; ctr++)
    {

      wait_bank_change (dev, dev->bank_count);
      read_line_101x (dev, lbuf[ctr], pixel, RefBlack, NULL, NULL);
      reset_bank_count (dev);
      if (ctr < 3)
	set_sti (dev);

    }

  for (ctr = 0; ctr < pixel; ctr++)
    {

      sum = lbuf[0][ctr] + lbuf[1][ctr] + lbuf[2][ctr] + lbuf[3][ctr];

      buf[ctr] = (sum / 4);

    }

}

static void
find_black_side_edge_101x (Mustek_PP_Device * dev)
{
  SANE_Byte buf[MUSTEK_PP_101x_MAX_H_PIXEL * 2];
  SANE_Byte blackposition[5];
  int pos = 0, ctr, blackpos;

  for (ctr = 0; ctr < 20; ctr++)
    {

      motor_forward_101x (dev);
      wait_bank_change (dev, dev->bank_count);
      read_line_101x (dev, buf, dev->desc->max_h_size, 0, NULL, NULL);
      reset_bank_count (dev);

      dev->ref_black = dev->ref_red = dev->ref_green = dev->ref_blue = buf[0];

      blackpos = dev->desc->max_h_size / 4;

      while ((abs (buf[blackpos] - buf[0]) >= 15) && (blackpos > 0))
	blackpos--;

      if (blackpos > 1)
	blackposition[pos++] = blackpos;

      if (pos == 5)
	break;

    }

  blackpos = 0;

  for (ctr = 0; ctr < pos; ctr++)
    if (blackposition[ctr] > blackpos)
      blackpos = blackposition[ctr];

  if (blackpos < 0x66)
    blackpos = 0x6A;

  dev->blackpos = blackpos;
  dev->Saved_CCD.skipcount = (blackpos + 12) & 0xFF;

}

static void
min_color_levels_101x (Mustek_PP_Device * dev)
{

  SANE_Byte buf[MUSTEK_PP_101x_MAX_H_PIXEL * 2];
  int ctr, sum = 0;

  for (ctr = 0; ctr < 8; ctr++)
    {

      set_ccd_channel_101x (dev, MUSTEK_PP_CHANNEL_RED);
      set_sti (dev);
      wait_bank_change (dev, dev->bank_count);

      read_line_101x (dev, buf, dev->desc->max_h_size, 0, NULL, NULL);

      reset_bank_count (dev);

      sum += buf[3];

    }

  dev->ref_red = sum / 8;

  sum = 0;

  for (ctr = 0; ctr < 8; ctr++)
    {

      set_ccd_channel_101x (dev, MUSTEK_PP_CHANNEL_GREEN);
      set_sti (dev);
      wait_bank_change (dev, dev->bank_count);

      read_line_101x (dev, buf, dev->desc->max_h_size, 0, NULL, NULL);

      reset_bank_count (dev);

      sum += buf[3];

    }

  dev->ref_green = sum / 8;

  sum = 0;

  for (ctr = 0; ctr < 8; ctr++)
    {

      set_ccd_channel_101x (dev, MUSTEK_PP_CHANNEL_BLUE);
      set_sti (dev);
      wait_bank_change (dev, dev->bank_count);

      read_line_101x (dev, buf, dev->desc->max_h_size, 0, NULL, NULL);

      reset_bank_count (dev);

      sum += buf[3];

    }

  dev->ref_blue = sum / 8;

}


static void
max_color_levels_101x (Mustek_PP_Device * dev)
{

  int ctr, line, sum;
  SANE_Byte rbuf[32][MUSTEK_PP_101x_MAX_H_PIXEL * 2];
  SANE_Byte gbuf[32][MUSTEK_PP_101x_MAX_H_PIXEL * 2];
  SANE_Byte bbuf[32][MUSTEK_PP_101x_MAX_H_PIXEL * 2];

  SANE_Byte maxbuf[32];

  for (ctr = 0; ctr < 32; ctr++)
    {

      if (dev->CCD.mode == MUSTEK_PP_MODE_COLOR)
	{

	  set_ccd_channel_101x (dev, MUSTEK_PP_CHANNEL_RED);
	  motor_forward_101x (dev);

	  read_average_line_101x (dev, rbuf[ctr], dev->params.pixels_per_line,
				  dev->ref_red);

	  set_ccd_channel_101x (dev, MUSTEK_PP_CHANNEL_GREEN);
	  set_sti (dev);

	  read_average_line_101x (dev, gbuf[ctr], dev->params.pixels_per_line,
				  dev->ref_green);

	  set_ccd_channel_101x (dev, MUSTEK_PP_CHANNEL_BLUE);
	  set_sti (dev);

	  read_average_line_101x (dev, bbuf[ctr], dev->params.pixels_per_line,
				  dev->ref_blue);

	}
      else
	{

	  dev->CCD.channel = MUSTEK_PP_CHANNEL_GRAY;

	  motor_forward_101x (dev);

	  read_average_line_101x (dev, gbuf[ctr], dev->params.pixels_per_line,
				  dev->ref_black);

	}

    }


  for (ctr = 0; ctr < dev->params.pixels_per_line; ctr++)
    {
      for (line = 0; line < 32; line++)
	maxbuf[line] = gbuf[line][ctr];

      qsort (maxbuf, 32, sizeof (maxbuf[0]), compar);

      sum = maxbuf[4] + maxbuf[5] + maxbuf[6] + maxbuf[7];

      dev->calib_g[ctr] = sum / 4;

    }

  if (dev->CCD.mode == MUSTEK_PP_MODE_COLOR)
    {

      for (ctr = 0; ctr < dev->params.pixels_per_line; ctr++)
	{
	  for (line = 0; line < 32; line++)
	    maxbuf[line] = rbuf[line][ctr];

	  qsort (maxbuf, 32, sizeof (maxbuf[0]), compar);

	  sum = maxbuf[4] + maxbuf[5] + maxbuf[6] + maxbuf[7];

	  dev->calib_r[ctr] = sum / 4;

	}

      for (ctr = 0; ctr < dev->params.pixels_per_line; ctr++)
	{
	  for (line = 0; line < 32; line++)
	    maxbuf[line] = bbuf[line][ctr];

	  qsort (maxbuf, 32, sizeof (maxbuf[0]), compar);

	  sum = maxbuf[4] + maxbuf[5] + maxbuf[6] + maxbuf[7];

	  dev->calib_b[ctr] = sum / 4;

	}

    }

}

static void
find_black_top_edge_101x (Mustek_PP_Device * dev)
{

  int lines = 0, ctr, pos;
  SANE_Byte buf[MUSTEK_PP_101x_MAX_H_PIXEL * 2];

  dev->CCD.channel = MUSTEK_PP_CHANNEL_GRAY;

  do
    {

      motor_forward_101x (dev);
      wait_bank_change (dev, dev->bank_count);

      read_line_101x
	(dev, buf, dev->desc->max_h_size, dev->ref_black, NULL, NULL);

      reset_bank_count (dev);

      pos = 0;

      for (ctr = dev->blackpos; ctr > dev->blackpos - 10; ctr--)
	if (buf[ctr] <= 15)
	  pos++;

    }
  while ((pos >= 8) && (lines++ < 67));

}

static void
calibrate_device_101x (Mustek_PP_Device * dev)
{

  int saved_ppl = dev->params.pixels_per_line, ctr;

  DBG (1, "calibrate_device_101x: use600 = %s\n",
       (dev->desc->use600 ? "yes" : "no"));

  if (dev->desc->use600)
    {

      int i, j;
/*	FILE *fptr; */
      SANE_Byte buf[MUSTEK_PP_101x_MAX_H_PIXEL * 2];

      dev->Saved_CCD = dev->CCD;
      dev->CCD.mode = MUSTEK_PP_MODE_COLOR;
      dev->CCD.hwres = dev->CCD.res = 600;
      dev->unknown_value = 0xAA;
      dev->expose_time = 0x64;
      dev->CCD.skipcount = dev->CCD.skipimagebytes = 0;
      dev->params.pixels_per_line = 10;

      config_ccd (dev);



      sanei_pa4s2_writebyte (dev->fd, 6, 0x37);
      sanei_pa4s2_writebyte (dev->fd, 6, 0x67);
      sanei_pa4s2_writebyte (dev->fd, 6, 0x17);
      sanei_pa4s2_writebyte (dev->fd, 6, 0x07);
      sanei_pa4s2_writebyte (dev->fd, 6, 0x27);
      sanei_pa4s2_writebyte (dev->fd, 6, 0x12);

      for (i = 0; i < 8; i++)
	for (j = 0; j < 255; j++)
	  sanei_pa4s2_writebyte (dev->fd, 5, j);

      for (j = 0; j < 8; j++)
	sanei_pa4s2_writebyte (dev->fd, 5, j);

      sanei_pa4s2_writebyte (dev->fd, 6, 0x02);

/*	fptr = fopen("/tmp/calib.1", "w"); */


      for (ctr = 0; ctr < 100; ctr++)
	{

	  reset_bank_count (dev);
	  sanei_pa4s2_writebyte (dev->fd, 6, 0x27);
	  read_line_101x (dev, buf, 2048, 0, NULL, NULL);

/*		fwrite(buf,2048,1,fptr); */

	}

/*	fclose (fptr); */

      dev->CCD.hwres = dev->CCD.res = 300;
      dev->unknown_value = 0xFE;
      dev->expose_time = 0xFF;
      dev->params.pixels_per_line = 150;
      dev->voltages[0] = 0x6b;
      dev->voltages[1] = 0x4f;
      dev->voltages[2] = 0x65;

      dev->send_voltages = 1;

      config_ccd (dev);
      get_bank_count (dev);

      dev->motor_ctrl = 0x43;
      for (ctr = 0; ctr < 3; ctr++)
	move_motor_101x (dev, SANE_TRUE);

/*	fptr = fopen("/tmp/calib.2","w"); */

      read_line_101x (dev, buf, 150, 0, NULL, NULL);

/*	fwrite(buf,150,1,fptr); 
	fclose(fptr); */


      move_motor_101x (dev, SANE_TRUE);

      dev->CCD = dev->Saved_CCD;

      dev->CCD.mode = MUSTEK_PP_MODE_GRAYSCALE;

      dev->voltages[0] = 0x32;
      dev->voltages[1] = 0x32;
      dev->voltages[3] = 0x32;
      dev->params.pixels_per_line = saved_ppl;

      config_ccd (dev);

      /*
         if ((dev->CCD.hwres >= 300) && (dev->CCD.mode != MUSTEK_PP_MODE_COLOR))
         motor_command = 0x63; */

      for (ctr = 0; ctr < 4; ctr++)
	{
	  get_bank_count (dev);
	  set_sti (dev);
	}

      dev->CCD.adjustskip = 3;

      sanei_pa4s2_writebyte (dev->fd, 6, 0x10);
      sanei_pa4s2_writebyte (dev->fd, 5, 0x7f);
      sanei_pa4s2_writebyte (dev->fd, 6, 0x20);
      sanei_pa4s2_writebyte (dev->fd, 5, 0x7f);
      sanei_pa4s2_writebyte (dev->fd, 6, 0x40);
      sanei_pa4s2_writebyte (dev->fd, 5, 0x7f);
      sanei_pa4s2_writebyte (dev->fd, 6, 0x00);

/*	fptr = fopen("/tmp/calib.3","w"); */

      j = 0;


      for (ctr = 0; ctr < 4; ctr++)
	{

	  read_line_101x (dev, buf, saved_ppl, 0, NULL, NULL);




/*		fwrite(buf,saved_ppl, 1,fptr); */
	  reset_bank_count (dev);
	  get_bank_count (dev);
	  set_sti (dev);

	}




      sanei_pa4s2_writebyte (dev->fd, 6, 0x10);
      sanei_pa4s2_writebyte (dev->fd, 5, 0x3f);
      sanei_pa4s2_writebyte (dev->fd, 6, 0x20);
      sanei_pa4s2_writebyte (dev->fd, 5, 0x3f);
      sanei_pa4s2_writebyte (dev->fd, 6, 0x40);
      sanei_pa4s2_writebyte (dev->fd, 5, 0x3f);
      sanei_pa4s2_writebyte (dev->fd, 6, 0x00);


      for (ctr = 0; ctr < 4; ctr++)
	{

	  read_line_101x (dev, buf, saved_ppl, 0, NULL, NULL);
/*		fwrite(buf,saved_ppl, 1,fptr); */
	  reset_bank_count (dev);
	  get_bank_count (dev);
	  set_sti (dev);

	}

      sanei_pa4s2_writebyte (dev->fd, 6, 0x10);
      sanei_pa4s2_writebyte (dev->fd, 5, 0x1f);
      sanei_pa4s2_writebyte (dev->fd, 6, 0x20);
      sanei_pa4s2_writebyte (dev->fd, 5, 0x1f);
      sanei_pa4s2_writebyte (dev->fd, 6, 0x40);
      sanei_pa4s2_writebyte (dev->fd, 5, 0x1f);
      sanei_pa4s2_writebyte (dev->fd, 6, 0x00);


      for (ctr = 0; ctr < 4; ctr++)
	{

	  read_line_101x (dev, buf, saved_ppl, 0, NULL, NULL);
/*		fwrite(buf,saved_ppl, 1,fptr); */
	  reset_bank_count (dev);
	  get_bank_count (dev);
	  set_sti (dev);

	}

      sanei_pa4s2_writebyte (dev->fd, 6, 0x10);
      sanei_pa4s2_writebyte (dev->fd, 5, 0x2f);
      sanei_pa4s2_writebyte (dev->fd, 6, 0x20);
      sanei_pa4s2_writebyte (dev->fd, 5, 0x2f);
      sanei_pa4s2_writebyte (dev->fd, 6, 0x40);
      sanei_pa4s2_writebyte (dev->fd, 5, 0x2f);
      sanei_pa4s2_writebyte (dev->fd, 6, 0x00);


      for (ctr = 0; ctr < 4; ctr++)
	{

	  read_line_101x (dev, buf, saved_ppl, 0, NULL, NULL);
/*		fwrite(buf,saved_ppl, 1,fptr);*/
	  reset_bank_count (dev);
	  get_bank_count (dev);
	  set_sti (dev);

	}

      for (i = 0; i < 4; i++)
	{

	  sanei_pa4s2_writebyte (dev->fd, 6, 0x10);
	  sanei_pa4s2_writebyte (dev->fd, 5, 0x37);
	  sanei_pa4s2_writebyte (dev->fd, 6, 0x20);
	  sanei_pa4s2_writebyte (dev->fd, 5, 0x37);
	  sanei_pa4s2_writebyte (dev->fd, 6, 0x40);
	  sanei_pa4s2_writebyte (dev->fd, 5, 0x37);
	  sanei_pa4s2_writebyte (dev->fd, 6, 0x00);


	  for (ctr = 0; ctr < 4; ctr++)
	    {

	      read_line_101x (dev, buf, saved_ppl, 0, NULL, NULL);
/*		fwrite(buf,saved_ppl, 1,fptr); */
	      reset_bank_count (dev);
	      get_bank_count (dev);
	      set_sti (dev);

	    }
	}

      sanei_pa4s2_writebyte (dev->fd, 6, 0x10);
      sanei_pa4s2_writebyte (dev->fd, 5, 0x37);
      sanei_pa4s2_writebyte (dev->fd, 6, 0x20);
      sanei_pa4s2_writebyte (dev->fd, 5, 0x37);
      sanei_pa4s2_writebyte (dev->fd, 6, 0x40);
      sanei_pa4s2_writebyte (dev->fd, 5, 0x37);
      sanei_pa4s2_writebyte (dev->fd, 6, 0x00);

      for (ctr = 0; ctr < 43; ctr++)
	{


	  read_line_101x (dev, buf, saved_ppl, 0, NULL, NULL);
/*		fwrite(buf,saved_ppl, 1,fptr); */
	  reset_bank_count (dev);
	  get_bank_count (dev);
	  motor_forward_1015 (dev);

	}

      lamp (dev, SANE_FALSE);

      for (ctr = 0; ctr < 13; ctr++)
	{

	  read_line_101x (dev, buf, saved_ppl, 0, NULL, NULL);

	  i = saved_ppl;
	  while ((abs (buf[i] - buf[0]) >= 15) && (i > 0))
	    i--;

	  if (i > j)
	    j = i;


	  /*

	     for (i=0 ; i<saved_ppl ; i++)
	     if (abs(buf[i] - buf[0]) >= 15) {


	     if (i > j)
	     j=i;

	     break;


	     } */
/*		fwrite(buf,saved_ppl, 1,fptr); */
	  reset_bank_count (dev);
	  get_bank_count (dev);
	  set_sti (dev);
	}
      lamp (dev, SANE_TRUE);

/*	fclose(fptr); */

      sanei_pa4s2_writebyte (dev->fd, 6, 0x10);
      sanei_pa4s2_writebyte (dev->fd, 5, 0x37);
      sanei_pa4s2_writebyte (dev->fd, 6, 0x20);
      sanei_pa4s2_writebyte (dev->fd, 5, 0x37);
      sanei_pa4s2_writebyte (dev->fd, 6, 0x40);
      sanei_pa4s2_writebyte (dev->fd, 5, 0x37);
      sanei_pa4s2_writebyte (dev->fd, 6, 0x00);




      dev->blackpos = 0x6a;
      dev->Saved_CCD.skipcount = 0x6A;	/*(120 + 12) & 0xFF; */

      dev->CCD = dev->Saved_CCD;

      dev->send_voltages = 0;
      config_ccd (dev);

      for (ctr = 0; ctr < 3; ctr++)
	{

	  motor_forward_1015 (dev);


	}

      for (ctr = 0; ctr < 100; ctr++)
	{
	  read_line_101x (dev, buf, 150, 0, NULL, NULL);
	  motor_forward_1015 (dev);
	}





      dev->CCD = dev->Saved_CCD;

      dev->send_voltages = 0;
      config_ccd (dev);




      return;
    }

  dev->Saved_CCD = dev->CCD;
  dev->params.pixels_per_line = dev->desc->max_h_size;
  dev->CCD.hwres = dev->CCD.res = dev->desc->max_res;
  dev->CCD.mode = MUSTEK_PP_MODE_GRAYSCALE;
  dev->CCD.skipcount = dev->CCD.skipimagebytes = 0;
  dev->CCD.invert = SANE_FALSE;
  dev->CCD.channel = MUSTEK_PP_CHANNEL_GRAY;

  config_ccd_101x (dev);
  get_bank_count (dev);

  find_black_side_edge_101x (dev);

  for (ctr = 0; ctr < 4; ctr++)
    move_motor_101x (dev, SANE_TRUE);

  dev->CCD = dev->Saved_CCD;

  dev->CCD.hwres = dev->CCD.res = dev->desc->max_res;
  dev->CCD.skipcount = dev->CCD.skipimagebytes = 0;
  dev->CCD.invert = SANE_FALSE;

  config_ccd_101x (dev);
  get_bank_count (dev);

  if ((dev->CCD.mode == MUSTEK_PP_MODE_COLOR) && (dev->ccd_type != 0))
    min_color_levels_101x (dev);

  dev->CCD = dev->Saved_CCD;
  dev->params.pixels_per_line = saved_ppl;
  dev->CCD.invert = SANE_FALSE;

  config_ccd_101x (dev);
  get_bank_count (dev);

  max_color_levels_101x (dev);

  dev->params.pixels_per_line = dev->desc->max_h_size;
  dev->CCD.mode = MUSTEK_PP_MODE_GRAYSCALE;
  dev->CCD.hwres = dev->CCD.res = dev->desc->max_res;
  dev->CCD.skipcount = dev->CCD.skipimagebytes = 0;
  dev->CCD.invert = SANE_FALSE;

  config_ccd_101x (dev);
  get_bank_count (dev);

  find_black_top_edge_101x (dev);

  dev->CCD = dev->Saved_CCD;

  dev->params.pixels_per_line = saved_ppl;

  config_ccd_101x (dev);
  get_bank_count (dev);

}

static void
get_grayscale_line_101x (Mustek_PP_Device * dev, SANE_Byte * buf)
{

  int skips;

  dev->line_diff +=
    SANE_FIX ((float) dev->desc->max_res / (float) dev->CCD.res);

  skips = (dev->line_diff >> SANE_FIXED_SCALE_SHIFT);

  while (--skips)
    {
      motor_forward_101x (dev);
      wait_bank_change (dev, dev->bank_count);
      reset_bank_count (dev);
    }

  dev->line_diff &= 0xFFFF;

  motor_forward_101x (dev);
  wait_bank_change (dev, dev->bank_count);

  read_line_101x (dev, buf, dev->params.pixels_per_line, dev->ref_black,
		  dev->calib_g,
		  (dev->val[OPT_CUSTOM_GAMMA].w ?
		   dev->gamma_table[0] : NULL));

  reset_bank_count (dev);

}

static void
get_lineart_line_101x (Mustek_PP_Device * dev, SANE_Byte * buf)
{

  int ctr;
  SANE_Byte gbuf[MUSTEK_PP_101x_MAX_H_PIXEL * 2];

  get_grayscale_line_101x (dev, gbuf);

  memset (buf, 0xFF, dev->params.bytes_per_line);

  for (ctr = 0; ctr < dev->params.pixels_per_line; ctr++)
    buf[ctr >> 3] ^= ((gbuf[ctr] > dev->desc->bw) ? (1 << (7 - ctr % 8)) : 0);

}

static void
get_color_line_101x (Mustek_PP_Device * dev, SANE_Byte * buf)
{

  SANE_Byte *red, *blue, *src, *dest;
  int gotline = 0, ctr;
  int gored, goblue, gogreen;
  int step = dev->CCD.line_step;

  do
    {

      red = dev->red[dev->redline];
      blue = dev->blue[dev->blueline];

      dev->ccd_line++;

      if ((dev->rdiff >> SANE_FIXED_SCALE_SHIFT) == dev->ccd_line)
	{
	  gored = 1;
	  dev->rdiff += step;
	}
      else
	gored = 0;

      if ((dev->bdiff >> SANE_FIXED_SCALE_SHIFT) == dev->ccd_line)
	{
	  goblue = 1;
	  dev->bdiff += step;
	}
      else
	goblue = 0;

      if ((dev->gdiff >> SANE_FIXED_SCALE_SHIFT) == dev->ccd_line)
	{
	  gogreen = 1;
	  dev->gdiff += step;
	}
      else
	gogreen = 0;

      if (!gored && !goblue && !gogreen)
	{
	  motor_forward_101x (dev);
	  wait_bank_change (dev, dev->bank_count);
	  reset_bank_count (dev);
	  if (dev->ccd_line >= (dev->CCD.line_step >> SANE_FIXED_SCALE_SHIFT))
	    dev->redline = ++dev->redline % dev->green_offs;
	  if (dev->ccd_line >=
	      dev->blue_offs + (dev->CCD.line_step >> SANE_FIXED_SCALE_SHIFT))
	    dev->blueline = ++dev->blueline % dev->blue_offs;
	  continue;
	}

      if (gored)
	dev->CCD.channel = MUSTEK_PP_CHANNEL_RED;
      else if (goblue)
	dev->CCD.channel = MUSTEK_PP_CHANNEL_BLUE;
      else
	dev->CCD.channel = MUSTEK_PP_CHANNEL_GREEN;

      motor_forward_101x (dev);
      wait_bank_change (dev, dev->bank_count);

      if (dev->ccd_line >= dev->green_offs && gogreen)
	{
	  src = red;
	  dest = buf;

	  for (ctr = 0; ctr < dev->params.pixels_per_line; ctr++)
	    {
	      *dest = *src++;
	      dest += 3;
	    }
	}

      if (gored)
	{

	  read_line_101x (dev, red, dev->params.pixels_per_line, dev->ref_red,
			  dev->calib_r,
			  (dev->val[OPT_CUSTOM_GAMMA].w ?
			   dev->gamma_table[1] : NULL));

	  reset_bank_count (dev);

	}

      dev->redline = ++dev->redline % dev->green_offs;

      if (dev->ccd_line >= dev->green_offs && gogreen)
	{
	  src = blue;
	  dest = buf + 2;


	  for (ctr = 0; ctr < dev->params.pixels_per_line; ctr++)
	    {
	      *dest = *src++;
	      dest += 3;
	    }

	}

      if (goblue)
	{
	  if (gored)
	    {
	      set_ccd_channel_101x (dev, MUSTEK_PP_CHANNEL_BLUE);
	      set_sti (dev);
	      wait_bank_change (dev, dev->bank_count);
	    }

	  read_line_101x (dev, blue, dev->params.pixels_per_line,
			  dev->ref_blue, dev->calib_b,
			  (dev->val[OPT_CUSTOM_GAMMA].w ? dev->
			   gamma_table[3] : NULL));

	  reset_bank_count (dev);

	}

      if (dev->ccd_line >=
	  dev->blue_offs + (dev->CCD.line_step >> SANE_FIXED_SCALE_SHIFT))
	dev->blueline = ++dev->blueline % dev->blue_offs;

      if (gogreen)
	{

	  if (gored || goblue)
	    {
	      set_ccd_channel_101x (dev, MUSTEK_PP_CHANNEL_GREEN);
	      set_sti (dev);
	      wait_bank_change (dev, dev->bank_count);
	    }

	  read_line_101x (dev, dev->green, dev->params.pixels_per_line,
			  dev->ref_green, dev->calib_g,
			  (dev->val[OPT_CUSTOM_GAMMA].w ?
			   dev->gamma_table[2] : NULL));

	  reset_bank_count (dev);

	  src = dev->green;
	  dest = buf + 1;

	  for (ctr = 0; ctr < dev->params.pixels_per_line; ctr++)
	    {
	      *dest = *src++;
	      dest += 3;
	    }

	  gotline = 1;
	}

    }
  while (!gotline);

}



/* these functions are for the 1013 chipset */

static void
set_ccd_channel_1013 (Mustek_PP_Device * dev, int channel)
{
  dev->CCD.channel = channel;
  sanei_pa4s2_writebyte (dev->fd, 6, chan_codes_1013[channel]);
}

static void
motor_backward_1013 (Mustek_PP_Device * dev)
{

  dev->motor_step++;
  set_led (dev);

  if (dev->motor_phase > 3)
    dev->motor_phase = 3;

  sanei_pa4s2_writebyte (dev->fd, 6, 0x62);
  sanei_pa4s2_writebyte (dev->fd, 5, fullstep[dev->motor_phase]);

  dev->motor_phase = (dev->motor_phase == 0 ? 3 : dev->motor_phase - 1);

  set_ccd_channel_1013 (dev, dev->CCD.channel);
  set_sti (dev);

}

static void
return_home_1013 (Mustek_PP_Device * dev, SANE_Bool nowait)
{
  u_char ishome;
  int ctr, saved_niceload;

  /* 1013 can't return home all alone, nowait ignored */

  saved_niceload = niceload;

  niceload = 0;

  for (ctr = 0; ctr < 4500; ctr++)
    {

      /* check_is_home_1013 */
      sanei_pa4s2_readbegin (dev->fd, 2);
      sanei_pa4s2_readbyte (dev->fd, &ishome);
      sanei_pa4s2_readend (dev->fd);

      /* yes, it should be is_not_home */
      if ((ishome & 1) == 0)
	break;

      motor_backward_1013 (dev);
      wait_bank_change (dev, dev->bank_count);
      reset_bank_count (dev);

    }

  niceload = saved_niceload;

}

static void
motor_forward_1013 (Mustek_PP_Device * dev)
{

  int ctr;

  dev->motor_step++;
  set_led (dev);

  for (ctr = 0; ctr < 2; ctr++)
    {

      sanei_pa4s2_writebyte (dev->fd, 6, 0x62);
      sanei_pa4s2_writebyte (dev->fd, 5, halfstep[dev->motor_phase]);

      dev->motor_phase = (dev->motor_phase == 7 ? 0 : dev->motor_phase + 1);

    }

  set_ccd_channel_1013 (dev, dev->CCD.channel);
  set_sti (dev);
}



static void
config_ccd_1013 (Mustek_PP_Device * dev)
{

  if (dev->CCD.res != 0)
    dev->CCD.res_step =
      SANE_FIX ((float) dev->CCD.hwres / (float) dev->CCD.res);

  set_dpi_value (dev);

  /* set_start_channel_1013 (dev); */

  sanei_pa4s2_writebyte (dev->fd, 6, 0x05);

  switch (dev->CCD.mode)
    {
    case MUSTEK_PP_MODE_LINEART:
    case MUSTEK_PP_MODE_GRAYSCALE:
      dev->CCD.channel = MUSTEK_PP_CHANNEL_GRAY;
      break;

    case MUSTEK_PP_MODE_COLOR:
      dev->CCD.channel = MUSTEK_PP_CHANNEL_RED;
      break;

    }

  set_ccd_channel_1013 (dev, dev->CCD.channel);

  /* set_invert_1013 (dev); */

  sanei_pa4s2_writebyte (dev->fd, 6,
			 (dev->CCD.invert == SANE_TRUE ? 0x04 : 0x14));

  sanei_pa4s2_writebyte (dev->fd, 6, 0x37);
  reset_bank_count (dev);

  sanei_pa4s2_writebyte (dev->fd, 6, 0x27);
  sanei_pa4s2_writebyte (dev->fd, 6, 0x67);
  sanei_pa4s2_writebyte (dev->fd, 6, 0x17);
  sanei_pa4s2_writebyte (dev->fd, 6, 0x77);

  /* set_initial_skip_1013 (dev); */

  sanei_pa4s2_writebyte (dev->fd, 6, 0x41);

  dev->CCD.adjustskip = dev->CCD.skipcount + dev->CCD.skipimagebytes;

  DBG (4, "config_ccd_1013: adjustskip %u\n", dev->CCD.adjustskip);

  sanei_pa4s2_writebyte (dev->fd, 5, dev->CCD.adjustskip / 16 + 2);

  dev->CCD.adjustskip %= 16;

  sanei_pa4s2_writebyte (dev->fd, 6, 0x81);
  sanei_pa4s2_writebyte (dev->fd, 5, 0x70);
  sanei_pa4s2_writebyte (dev->fd, 6, 0x01);


  set_line_adjust (dev);

  get_bank_count (dev);

}

/* these functions are for the 1015 chipset */


static void
motor_control_1015 (Mustek_PP_Device * dev, u_char control)
{


  u_char val;

  DBG (4, "motor_controll_1015: control code 0x%02x\n",
       (unsigned int) control);

  if (dev->desc->use600 == SANE_FALSE)
    sanei_pa4s2_writebyte (dev->fd, 6, 0xF6);
  sanei_pa4s2_writebyte (dev->fd, 6, 0x22);
  sanei_pa4s2_writebyte (dev->fd, 5, control);
  sanei_pa4s2_writebyte (dev->fd, 6, 0x02);

  do
    {

      sanei_pa4s2_readbegin (dev->fd, 2);
      sanei_pa4s2_readbyte (dev->fd, &val);
      sanei_pa4s2_readend (dev->fd);

    }
  while ((val & 0x08) != 0);

}

static void
return_home_1015 (Mustek_PP_Device * dev, SANE_Bool nowait)
{

  u_char ishome, control = 0xC3;

  if (dev->desc->use600 == SANE_TRUE)
    {

      switch (dev->ccd_type)
	{
	case 1:
	  control = 0xD3;
	  break;

	default:
	  control = 0xC3;
	  break;
	}
    }

  motor_control_1015 (dev, control);

  do
    {

      /* check_is_home_1015 */
      sanei_pa4s2_readbegin (dev->fd, 2);
      sanei_pa4s2_readbyte (dev->fd, &ishome);
      sanei_pa4s2_readend (dev->fd);

      if (nowait)
	break;

      usleep (1000);		/* much nicer load */

    }
  while ((ishome & 2) == 0);

}

static void
motor_forward_1015 (Mustek_PP_Device * dev)
{
  u_char control = 0x1B;

  dev->motor_step++;
  if (dev->desc->use600 == SANE_FALSE)
    set_led (dev);

  if (dev->desc->use600 == SANE_TRUE)
    {

      switch (dev->ccd_type)
	{
	case 1:
	  control = dev->motor_ctrl;
	  break;

	default:
	  control = 0x1B;
	  break;
	}
    }

  motor_control_1015 (dev, control);
  /*

     if ((dev->desc->use600 == SANE_TRUE) && (dev->CCD.mode != MUSTEK_PP_MODE_COLOR) && (dev->CCD.hwres < 300))
     motor_control_1015 (dev, control); */

  if (dev->desc->use600 == SANE_FALSE)
    set_ccd_channel_1015 (dev, dev->CCD.channel);
  set_sti (dev);

}

/*
static void
motor_backward_1015 (Mustek_PP_Device * dev)
{
  u_char control = 0x43;

	
  dev->motor_step++;

  set_led (dev);

  switch (dev->ccd_type)
    {
    case 1:
      control = 0x1B;
      break;

    default:
      control = 0x43;
      break;
    }

  motor_control_1015 (dev, control);

  set_ccd_channel_1015 (dev, dev->CCD.channel);
  set_sti (dev);

}
*/


static void
set_ccd_channel_1015 (Mustek_PP_Device * dev, int channel)
{

  u_char chancode = chan_codes_1015[channel];

  dev->CCD.channel = channel;
  if (dev->desc->use600 == SANE_TRUE)
    {
      chancode |= 0x14;
    }
  else
    {

      dev->image_control &= 0x34;
      chancode |= dev->image_control;

    }

  dev->image_control = chancode;

  sanei_pa4s2_writebyte (dev->fd, 6, chancode);

}


static void
config_ccd_1015 (Mustek_PP_Device * dev)
{

  u_char val;

  if (dev->CCD.res != 0)
    dev->CCD.res_step =
      SANE_FIX ((float) dev->CCD.hwres / (float) dev->CCD.res);

  if (dev->desc->use600 == SANE_TRUE)
    {

      if (dev->first_time)
	sanei_pa4s2_writebyte (dev->fd, 6, 0x86);
      else
	sanei_pa4s2_writebyte (dev->fd, 6, 0xC6);

      dev->first_time = 0;
    }

  set_dpi_value (dev);

  dev->image_control = 4;

  /* set_start_channel_1015 (dev); */

  switch (dev->CCD.mode)
    {
    case MUSTEK_PP_MODE_LINEART:
    case MUSTEK_PP_MODE_GRAYSCALE:
      dev->CCD.channel = MUSTEK_PP_CHANNEL_GRAY;
      break;

    case MUSTEK_PP_MODE_COLOR:
      dev->CCD.channel = MUSTEK_PP_CHANNEL_RED;
      break;

    }

  set_ccd_channel_1015 (dev, dev->CCD.channel);


  /* set_invert_1015 (dev); */

  dev->image_control &= 0xE4;

  if (dev->CCD.invert == SANE_FALSE)
    dev->image_control |= 0x10;


  if (dev->desc->use600 == SANE_TRUE)
    {

      sanei_pa4s2_writebyte (dev->fd, 6, 0x13);
      sanei_pa4s2_writebyte (dev->fd, 5, dev->unknown_value);	/* FIXME */
      sanei_pa4s2_writebyte (dev->fd, 6, 0x03);

    }
  else
    {
      sanei_pa4s2_writebyte (dev->fd, 6, dev->image_control);
    }

  sanei_pa4s2_writebyte (dev->fd, 6, 0x23);
  sanei_pa4s2_writebyte (dev->fd, 5, 0x00);

  if (dev->desc->use600 == SANE_TRUE)
    sanei_pa4s2_writebyte (dev->fd, 6, 0x03);

  sanei_pa4s2_writebyte (dev->fd, 6, 0x43);

  switch (dev->ccd_type)
    {
    case 1:
      if (dev->desc->use600 == SANE_TRUE)
	val = 0x0F;
      else
	val = 0x6B;
      break;
    case 4:
      val = 0x9F;
      break;
    default:
      val = 0x92;
      break;
    }

  sanei_pa4s2_writebyte (dev->fd, 5, val);
  sanei_pa4s2_writebyte (dev->fd, 6, 0x03);

  if (dev->desc->use600 == SANE_TRUE)
    sanei_pa4s2_writebyte (dev->fd, 6, 0x45);	/* or 0x05 for no 8kbank */

  sanei_pa4s2_writebyte (dev->fd, 6, 0x37);
  reset_bank_count (dev);

  sanei_pa4s2_writebyte (dev->fd, 6, 0x27);
  sanei_pa4s2_writebyte (dev->fd, 6, 0x67);
  sanei_pa4s2_writebyte (dev->fd, 6, 0x17);
  sanei_pa4s2_writebyte (dev->fd, 6, 0x77);

  /* set_initial_skip_1015 (dev); */

  sanei_pa4s2_writebyte (dev->fd, 6, 0x41);

  dev->CCD.adjustskip = dev->CCD.skipcount + dev->CCD.skipimagebytes;

  /* if (dev->CCD.mode == MUSTEK_PP_MODE_COLOR)
     dev->CCD.adjustskip <<= 3; */


  if ((dev->desc->use600 == SANE_TRUE) && (dev->CCD.adjustskip > 32))
    dev->CCD.adjustskip += 26;

  sanei_pa4s2_writebyte (dev->fd, 5,
			 dev->CCD.adjustskip / 32 + (dev->desc->use600 ==
						     SANE_TRUE ? 0 : 1));

/*  if (dev->desc->use600 == SANE_FALSE)  */
  dev->CCD.adjustskip %= 32;


  if (dev->desc->use600 == SANE_TRUE)
    sanei_pa4s2_writebyte (dev->fd, 6, 0x01);

  sanei_pa4s2_writebyte (dev->fd, 6, 0x81);

  /* expose time */
  switch (dev->ccd_type)
    {
    case 1:

      if (dev->desc->use600 == SANE_TRUE)
	val = dev->expose_time;
      else
	val = 0xA8;
      break;
    case 0:
      val = 0x8A;
      break;
    default:
      val = 0xA8;
      break;
    }

  sanei_pa4s2_writebyte (dev->fd, 5, val);

  sanei_pa4s2_writebyte (dev->fd, 6, 0x01);

  if ((dev->desc->use600 == SANE_TRUE) && (dev->send_voltages == 1))
    {

      sanei_pa4s2_writebyte (dev->fd, 6, 0x81);
      sanei_pa4s2_writebyte (dev->fd, 5, 0xFD);
      sanei_pa4s2_writebyte (dev->fd, 6, 0x01);

      sanei_pa4s2_writebyte (dev->fd, 6, 0x13);
      sanei_pa4s2_writebyte (dev->fd, 5, 0xFE);
      sanei_pa4s2_writebyte (dev->fd, 6, 0x03);

      sanei_pa4s2_writebyte (dev->fd, 6, 0x10);
      sanei_pa4s2_writebyte (dev->fd, 5, dev->voltages[0]);
      sanei_pa4s2_writebyte (dev->fd, 6, 0x20);
      sanei_pa4s2_writebyte (dev->fd, 5, dev->voltages[1]);
      sanei_pa4s2_writebyte (dev->fd, 6, 0x40);
      sanei_pa4s2_writebyte (dev->fd, 5, dev->voltages[2]);
      sanei_pa4s2_writebyte (dev->fd, 6, 0x00);


    }

  set_line_adjust (dev);

  get_bank_count (dev);

}

/* these functions are for the 1505 chipset */
static void
return_home_1505 (Mustek_PP_Device * dev, SANE_Bool nowait)
{
}

static void
config_ccd_1505 (Mustek_PP_Device * dev)
{
}

static void
lamp_1505 (Mustek_PP_Device * dev, int lamp_on)
{
}

static void
send_voltages_1505 (Mustek_PP_Device * dev)
{
}

static void
move_motor_1505 (Mustek_PP_Device * dev, int forward)
{
}

static void
calibrate_device_1505 (Mustek_PP_Device * dev)
{
}

static void
get_grayscale_line_1505 (Mustek_PP_Device * dev, SANE_Byte * buf)
{

}

static void
get_lineart_line_1505 (Mustek_PP_Device * dev, SANE_Byte * buf)
{

}

static void
get_color_line_1505 (Mustek_PP_Device * dev, SANE_Byte * buf)
{

}

/* these functions are interfaces only */
static void
config_ccd (Mustek_PP_Device * dev)
{
  DBG (6, "config_ccd: %d dpi, mode %d, invert %d, size %d\n",
       dev->CCD.hwres, dev->CCD.mode, dev->CCD.invert,
       dev->params.pixels_per_line);

  switch (dev->asic_id)
    {
    case MUSTEK_PP_ASIC_1013:
      config_ccd_1013 (dev);
      break;

    case MUSTEK_PP_ASIC_1015:
      config_ccd_1015 (dev);
      break;

    case MUSTEK_PP_ASIC_1505:
      config_ccd_1505 (dev);
      break;
    }

}

static void
return_home (Mustek_PP_Device * dev, SANE_Bool nowait)
{

  CCD_Info CCD = dev->CCD;


  if (dev->desc->use600)
    {
      dev->CCD.mode = MUSTEK_PP_MODE_COLOR;
      dev->CCD.hwres = dev->CCD.res = 600;
    }
  else
    {
      dev->CCD.hwres = dev->CCD.res = 100;
      dev->CCD.mode = MUSTEK_PP_MODE_GRAYSCALE;
    }

  dev->CCD.skipcount = dev->CCD.skipimagebytes = 0;

  config_ccd (dev);

  switch (dev->asic_id)
    {
    case MUSTEK_PP_ASIC_1013:
      return_home_1013 (dev, nowait);
      break;

    case MUSTEK_PP_ASIC_1015:
      return_home_1015 (dev, nowait);
      break;

    case MUSTEK_PP_ASIC_1505:
      return_home_1505 (dev, nowait);
      break;
    }

  dev->CCD = CCD;

  dev->motor_step = 0;

  config_ccd (dev);
}

static void
lamp (Mustek_PP_Device * dev, int lamp_on)
{

  switch (dev->asic_id)
    {
    case MUSTEK_PP_ASIC_1013:
    case MUSTEK_PP_ASIC_1015:
      set_lamp (dev, lamp_on);
      break;

    case MUSTEK_PP_ASIC_1505:
      lamp_1505 (dev, lamp_on);
      break;
    }

}

static void
set_voltages (Mustek_PP_Device * dev)
{
  switch (dev->asic_id)
    {
    case MUSTEK_PP_ASIC_1013:
    case MUSTEK_PP_ASIC_1015:
      send_voltages (dev);
      break;

    case MUSTEK_PP_ASIC_1505:
      send_voltages_1505 (dev);
      break;

    }
}

static void
move_motor (Mustek_PP_Device * dev, int count, int forward)
{

  int ctr;
  int saved_niceload = niceload;

  DBG (4, "move_motor: %u steps (%s)\n", count,
       (forward == SANE_TRUE ? "forward" : "backward"));

  niceload = 0;

  for (ctr = 0; ctr < count; ctr++)
    {

      switch (dev->asic_id)
	{
	case MUSTEK_PP_ASIC_1013:
	case MUSTEK_PP_ASIC_1015:
	  move_motor_101x (dev, forward);
	  break;

	case MUSTEK_PP_ASIC_1505:
	  move_motor_1505 (dev, forward);
	  break;
	}

    }

  niceload = saved_niceload;

}

static void
calibrate (Mustek_PP_Device * dev)
{

  DBG (1, "calibrate entered (asic = 0x%02x)\n", dev->asic_id);

  switch (dev->asic_id)
    {
    case MUSTEK_PP_ASIC_1015:
    case MUSTEK_PP_ASIC_1013:
      calibrate_device_101x (dev);
      break;

    case MUSTEK_PP_ASIC_1505:
      calibrate_device_1505 (dev);
      break;
    }

  DBG (3, "calibrate: ref_black %d, blackpos %d\n",
       dev->ref_black, dev->blackpos);

}


static void
get_lineart_line (Mustek_PP_Device * dev, SANE_Byte * buf)
{
  switch (dev->asic_id)
    {
    case MUSTEK_PP_ASIC_1015:
    case MUSTEK_PP_ASIC_1013:
      get_lineart_line_101x (dev, buf);
      break;

    case MUSTEK_PP_ASIC_1505:
      get_lineart_line_1505 (dev, buf);
      break;

    }
}

static void
get_grayscale_line (Mustek_PP_Device * dev, SANE_Byte * buf)
{

  switch (dev->asic_id)
    {
    case MUSTEK_PP_ASIC_1015:
    case MUSTEK_PP_ASIC_1013:
      get_grayscale_line_101x (dev, buf);
      break;

    case MUSTEK_PP_ASIC_1505:
      get_grayscale_line_1505 (dev, buf);
      break;

    }

}

static void
get_color_line (Mustek_PP_Device * dev, SANE_Byte * buf)
{

  switch (dev->asic_id)
    {
    case MUSTEK_PP_ASIC_1015:
    case MUSTEK_PP_ASIC_1013:
      get_color_line_101x (dev, buf);
      break;

    case MUSTEK_PP_ASIC_1505:
      get_color_line_1505 (dev, buf);
      break;

    }

}


#ifdef HAVE_AUTHORIZATION

static SANE_Status
do_authorization (SANE_String_Const ressource)
{
  /* This function implements a simple authorization function. It looks */
  /* up an entry in the file SANE_PATH_CONFIG_DIR/.auth. Such an entry */
  /* must be of the form device:user:password where password is a crypt() */
  /* encrypted password. If several users are allowed to access a device */
  /* an entry must be created for each user. If no entry exists for device */
  /* or the file does not exist the authentication failes. If the */
  /* file exists, but can't be opened the authentication fails */

  SANE_Status status;
  FILE *fp;
  int device_found;
  char username[SANE_MAX_USERNAME_LEN];
  char password[SANE_MAX_PASSWORD_LEN];
  char line[MAX_LINE_LEN];
  char *linep;
  char *device;
  char *user;
  char *passwd;
  char *p;

  if (auth_callback == NULL)
    {
      DBG (2, "auth: no auth callback\n");
      DEBUG ();
      return SANE_STATUS_ACCESS_DENIED;
    }

  /* first check if an entry exists in for this device */

  fp = sanei_config_open (PASSWD_FILE);

  if (fp == NULL)
    {
      DBG (2, "auth: password file is missing\n");
      DEBUG ();
      return SANE_STATUS_ACCESS_DENIED;
    }

  linep = &line[0];
  device_found = SANE_FALSE;

  while (sanei_config_read (line, MAX_LINE_LEN, fp))
    {
      p = index (linep, SEPARATOR);
      if (p)
	{
	  *p = '\0';
	  device = linep;
	  if (strcmp (device, ressource) == 0)
	    {
	      device_found = SANE_TRUE;
	      break;
	    }
	}
    }

  if (device_found == SANE_FALSE)
    {
      DBG (2, "auth: requested resource (%s) not in file\n", ressource);
      DEBUG ();
      return SANE_STATUS_ACCESS_DENIED;
    }

  fseek (fp, 0L, SEEK_SET);

  (*auth_callback) (ressource, username, password);

  status = SANE_STATUS_ACCESS_DENIED;

  do
    {
      sanei_config_read (line, MAX_LINE_LEN, fp);
      if (!ferror (fp) && !feof (fp))
	{
	  /* neither strsep(3) nor strtok(3) seem to work on my system */
	  p = index (linep, SEPARATOR);
	  if (p == NULL)
	    continue;
	  *p = '\0';
	  device = linep;
	  if (strcmp (device, ressource) != 0)	/* not a matching entry */
	    continue;

	  linep = ++p;
	  p = index (linep, SEPARATOR);
	  if (p == NULL)
	    continue;

	  *p = '\0';
	  user = linep;
	  if (strncmp (user, username, SANE_MAX_USERNAME_LEN) != 0)
	    continue;		/* username doesn´t match */

	  linep = ++p;
	  /* rest of the line is considered to be the password */
	  passwd = linep;
	  /* remove newline */
	  *(passwd + strlen (passwd) - 1) = '\0';
	  p = crypt (password, SALT);
	  if (strcmp (p, passwd) == 0)
	    {
	      /* authentication ok */
	      status = SANE_STATUS_GOOD;
	      break;
	    }
	  else
	    continue;
	}
    }
  while (!ferror (fp) && !feof (fp));
  fclose (fp);

  if (status != SANE_STATUS_GOOD)
    DBG (1, "auth: access denied\n");

  return status;
}
#endif

static SANE_Status
attach (const char *devname)
{
  Mustek_PP_Descriptor *dev;
  int i, fd;
  SANE_Status status;
  u_char val;


  for (i = 0; i < num_devices; i++)
    if (strcmp (devlist[i].port, devname) == 0)
      return SANE_STATUS_GOOD;

  status = sanei_pa4s2_open (devname, &fd);

  if (status != SANE_STATUS_GOOD)
    {
      DBG (2, "attach: couldn't attach to `%s' (%s)\n", devname,
	   sane_strstatus (status));
      DEBUG ();
      return status;
    }


  sanei_pa4s2_enable (fd, SANE_TRUE);
  sanei_pa4s2_readbegin (fd, 0);
  sanei_pa4s2_readbyte (fd, &val);
  sanei_pa4s2_readend (fd);
  sanei_pa4s2_enable (fd, SANE_FALSE);


#ifndef PATCH_MUSTEK_PP_1505

  if (val == MUSTEK_PP_ASIC_1505)
    {

      DBG (2, "attach: this scanner reports ASIC 0x%02x\n", (int) val);
      DBG (2, "attach: you'll have to enable this in the source\n");
      sanei_pa4s2_close (fd);
      return SANE_STATUS_INVAL;

    }

#endif


  dev = malloc (sizeof (Mustek_PP_Descriptor) * (num_devices + 1));

  if (dev == NULL)
    {
      DBG (2, "attach: not enough memory for device descriptor\n");
      DEBUG ();
      sanei_pa4s2_close (fd);
      return SANE_STATUS_NO_MEM;
    }

  memset (dev, 0, sizeof (Mustek_PP_Descriptor) * (num_devices + 1));

  if (num_devices > 0)
    {
      memcpy (dev + 1, devlist,
	      sizeof (Mustek_PP_Descriptor) * (num_devices));
      free (devlist);
    }

  devlist = dev;
  num_devices++;

  dev->sane.name = strdup (devname);
  dev->sane.vendor = strdup ("Mustek");
  dev->sane.type = "flatbed scanner";

  dev->port = strdup (devname);

  dev->buf_size = buf_size;

  dev->requires_auth = SANE_FALSE;

  dev->wait_lamp = wait_lamp;

  dev->bw = bw;

  dev->asic = val;

  sanei_pa4s2_enable (fd, SANE_TRUE);
  sanei_pa4s2_readbegin (fd, 2);
  sanei_pa4s2_readbyte (fd, &val);
  sanei_pa4s2_readend (fd);
  sanei_pa4s2_enable (fd, SANE_FALSE);

  dev->ccd = (val & (dev->asic == MUSTEK_PP_ASIC_1013 ? 0x04 : 0x05));
  /* FIXME: ASIC 1505 scanners do *not* report their CCD type ... 
     at least not this way :-( */

  sanei_pa4s2_close (fd);


  switch (dev->asic)
    {
    case MUSTEK_PP_ASIC_1013:
      dev->max_res = 300;
      dev->max_h_size = MUSTEK_PP_101x_MAX_H_PIXEL;
      dev->max_v_size = MUSTEK_PP_101x_MAX_V_PIXEL;
      dev->wait_bank = wait_bank;
      dev->strip_height = strip_height;
      dev->sane.model = strdup ("MFS-600IIIP");

      dev->use600 = SANE_FALSE;
      break;

    case MUSTEK_PP_ASIC_1015:

/*      if (dev->ccd == 1)
	{
          dev->max_res = 600;
          dev->max_h_size = MUSTEK_PP_101x_MAX_H_PIXEL * 2;
          dev->max_v_size = MUSTEK_PP_101x_MAX_V_PIXEL * 2;
	}
      else
	{*/
      dev->max_res = 300;
      dev->max_h_size = MUSTEK_PP_101x_MAX_H_PIXEL;
      dev->max_v_size = MUSTEK_PP_101x_MAX_V_PIXEL;
/*	}*/

      dev->wait_bank = wait_bank;
      dev->strip_height = strip_height;
      dev->sane.model = strdup ("MFS-600IIIP");

      dev->use600 = SANE_FALSE;
      break;

    case MUSTEK_PP_ASIC_1505:

      DBG (2, "attach: don't know parameters for ASIC 1505 scanner\n");
      DEBUG ();

      dev->max_res = 600;
      dev->max_h_size = 5100;
      dev->max_v_size = 7000;
      dev->wait_bank = wait_bank;
      dev->strip_height = strip_height;
      dev->sane.model = strdup ("MFS-600IIIP");
      dev->use600 = SANE_TRUE;
      break;
    }



  DBG (3, "attach: device %s attached\n", devname);

  DBG (3, "attach: asic 0x%02x, ccd %02d\n", dev->asic, dev->ccd);

  DBG (4, "attach: use600 is `%s'\n",
       (dev->use600 == SANE_TRUE ? "yes" : "no"));

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
init_options (Mustek_PP_Device * dev)
{
  int i;

  /* only this code is tested so far */
  ASSERT ((dev->asic_id == MUSTEK_PP_ASIC_1013) ||
	  ((dev->asic_id == MUSTEK_PP_ASIC_1015) && (dev->ccd_type == 0)));

  memset (dev->opt, 0, sizeof (dev->opt));
  memset (dev->val, 0, sizeof (dev->val));

  for (i = 0; i < NUM_OPTIONS; ++i)
    {
      dev->opt[i].size = sizeof (SANE_Word);
      dev->opt[i].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    }

  dev->opt[OPT_NUM_OPTS].title = SANE_TITLE_NUM_OPTIONS;
  dev->opt[OPT_NUM_OPTS].desc = SANE_DESC_NUM_OPTIONS;
  dev->opt[OPT_NUM_OPTS].type = SANE_TYPE_INT;
  dev->opt[OPT_NUM_OPTS].cap = SANE_CAP_SOFT_DETECT;
  dev->val[OPT_NUM_OPTS].w = NUM_OPTIONS;

  /* "Mode" group: */

  dev->opt[OPT_MODE_GROUP].title = "Scan Mode";
  dev->opt[OPT_MODE_GROUP].desc = "";
  dev->opt[OPT_MODE_GROUP].type = SANE_TYPE_GROUP;
  dev->opt[OPT_MODE_GROUP].cap = 0;
  dev->opt[OPT_MODE_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* scan mode */
  dev->opt[OPT_MODE].name = SANE_NAME_SCAN_MODE;
  dev->opt[OPT_MODE].title = SANE_TITLE_SCAN_MODE;
  dev->opt[OPT_MODE].desc = SANE_DESC_SCAN_MODE;
  dev->opt[OPT_MODE].type = SANE_TYPE_STRING;
  dev->opt[OPT_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  dev->opt[OPT_MODE].size = max_string_size (mode_list);
  dev->opt[OPT_MODE].constraint.string_list = mode_list;
  dev->val[OPT_MODE].s = strdup (mode_list[2]);

  /* resolution */
  dev->opt[OPT_RESOLUTION].name = SANE_NAME_SCAN_RESOLUTION;
  dev->opt[OPT_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
  dev->opt[OPT_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;
  dev->opt[OPT_RESOLUTION].type = SANE_TYPE_FIXED;
  dev->opt[OPT_RESOLUTION].unit = SANE_UNIT_DPI;
  dev->opt[OPT_RESOLUTION].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_RESOLUTION].constraint.range = &dev->dpi_range;
  dev->val[OPT_RESOLUTION].w = dev->dpi_range.max;

  /* preview */
  dev->opt[OPT_PREVIEW].name = SANE_NAME_PREVIEW;
  dev->opt[OPT_PREVIEW].title = SANE_TITLE_PREVIEW;
  dev->opt[OPT_PREVIEW].desc = SANE_DESC_PREVIEW;
  dev->opt[OPT_PREVIEW].type = SANE_TYPE_BOOL;
  dev->val[OPT_PREVIEW].w = SANE_FALSE;

  /* gray preview */
  dev->opt[OPT_GRAY_PREVIEW].name = SANE_NAME_GRAY_PREVIEW;
  dev->opt[OPT_GRAY_PREVIEW].title = SANE_TITLE_GRAY_PREVIEW;
  dev->opt[OPT_GRAY_PREVIEW].desc = SANE_DESC_GRAY_PREVIEW;
  dev->opt[OPT_GRAY_PREVIEW].type = SANE_TYPE_BOOL;
  dev->val[OPT_GRAY_PREVIEW].w = SANE_FALSE;

  /* "Geometry" group: */

  dev->opt[OPT_GEOMETRY_GROUP].title = "Geometry";
  dev->opt[OPT_GEOMETRY_GROUP].desc = "";
  dev->opt[OPT_GEOMETRY_GROUP].type = SANE_TYPE_GROUP;
  dev->opt[OPT_GEOMETRY_GROUP].cap = 0;
  dev->opt[OPT_GEOMETRY_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* top-left x */
  dev->opt[OPT_TL_X].name = SANE_NAME_SCAN_TL_X;
  dev->opt[OPT_TL_X].title = SANE_TITLE_SCAN_TL_X;
  dev->opt[OPT_TL_X].desc = SANE_DESC_SCAN_TL_X;
  dev->opt[OPT_TL_X].type = SANE_TYPE_FIXED;
  dev->opt[OPT_TL_X].unit = SANE_UNIT_MM;
  dev->opt[OPT_TL_X].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_TL_X].constraint.range = &dev->x_range;
  dev->val[OPT_TL_X].w = 0;

  /* top-left y */
  dev->opt[OPT_TL_Y].name = SANE_NAME_SCAN_TL_Y;
  dev->opt[OPT_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
  dev->opt[OPT_TL_Y].desc = SANE_DESC_SCAN_TL_Y;
  dev->opt[OPT_TL_Y].type = SANE_TYPE_FIXED;
  dev->opt[OPT_TL_Y].unit = SANE_UNIT_MM;
  dev->opt[OPT_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_TL_Y].constraint.range = &dev->y_range;
  dev->val[OPT_TL_Y].w = 0;

  /* bottom-right x */
  dev->opt[OPT_BR_X].name = SANE_NAME_SCAN_BR_X;
  dev->opt[OPT_BR_X].title = SANE_TITLE_SCAN_BR_X;
  dev->opt[OPT_BR_X].desc = SANE_DESC_SCAN_BR_X;
  dev->opt[OPT_BR_X].type = SANE_TYPE_FIXED;
  dev->opt[OPT_BR_X].unit = SANE_UNIT_MM;
  dev->opt[OPT_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_BR_X].constraint.range = &dev->x_range;
  dev->val[OPT_BR_X].w = dev->x_range.max;

  /* bottom-right y */
  dev->opt[OPT_BR_Y].name = SANE_NAME_SCAN_BR_Y;
  dev->opt[OPT_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
  dev->opt[OPT_BR_Y].desc = SANE_DESC_SCAN_BR_Y;
  dev->opt[OPT_BR_Y].type = SANE_TYPE_FIXED;
  dev->opt[OPT_BR_Y].unit = SANE_UNIT_MM;
  dev->opt[OPT_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_BR_Y].constraint.range = &dev->y_range;
  dev->val[OPT_BR_Y].w = dev->y_range.max;

  /* "Enhancement" group: */

  dev->opt[OPT_ENHANCEMENT_GROUP].title = "Enhancement";
  dev->opt[OPT_ENHANCEMENT_GROUP].desc = "";
  dev->opt[OPT_ENHANCEMENT_GROUP].type = SANE_TYPE_GROUP;
  dev->opt[OPT_ENHANCEMENT_GROUP].cap = SANE_CAP_ADVANCED;
  dev->opt[OPT_ENHANCEMENT_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

#ifdef HAVE_INVERSION

  /* invert */
  dev->oPt[OPT_INVERT].name = SANE_NAME_NEGATIVE;
  dev->opt[OPT_INVERT].title = SANE_TITLE_NEGATIVE;
  dev->opt[OPT_INVERT].desc = "Invert colors colors/swap black and white";
  dev->opt[OPT_INVERT].type = SANE_TYPE_BOOL;
  dev->opt[OPT_INVERT].cap |= SANE_CAP_ADVANCED;
  dev->val[OPT_INVERT].w = SANE_FALSE;

#endif

  /* custom-gamma table */
  dev->opt[OPT_CUSTOM_GAMMA].name = SANE_NAME_CUSTOM_GAMMA;
  dev->opt[OPT_CUSTOM_GAMMA].title = SANE_TITLE_CUSTOM_GAMMA;
  dev->opt[OPT_CUSTOM_GAMMA].desc = SANE_DESC_CUSTOM_GAMMA;
  dev->opt[OPT_CUSTOM_GAMMA].type = SANE_TYPE_BOOL;
  dev->opt[OPT_CUSTOM_GAMMA].cap |= SANE_CAP_ADVANCED;
  dev->val[OPT_CUSTOM_GAMMA].w = SANE_FALSE;

  /* grayscale gamma vector */
  dev->opt[OPT_GAMMA_VECTOR].name = SANE_NAME_GAMMA_VECTOR;
  dev->opt[OPT_GAMMA_VECTOR].title = SANE_TITLE_GAMMA_VECTOR;
  dev->opt[OPT_GAMMA_VECTOR].desc = SANE_DESC_GAMMA_VECTOR;
  dev->opt[OPT_GAMMA_VECTOR].type = SANE_TYPE_INT;
  dev->opt[OPT_GAMMA_VECTOR].cap |= SANE_CAP_INACTIVE | SANE_CAP_ADVANCED;
  dev->opt[OPT_GAMMA_VECTOR].unit = SANE_UNIT_NONE;
  dev->opt[OPT_GAMMA_VECTOR].size = 256 * sizeof (SANE_Word);
  dev->opt[OPT_GAMMA_VECTOR].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_GAMMA_VECTOR].constraint.range = &u8_range;
  dev->val[OPT_GAMMA_VECTOR].wa = &dev->gamma_table[0][0];

  /* red gamma vector */
  dev->opt[OPT_GAMMA_VECTOR_R].name = SANE_NAME_GAMMA_VECTOR_R;
  dev->opt[OPT_GAMMA_VECTOR_R].title = SANE_TITLE_GAMMA_VECTOR_R;
  dev->opt[OPT_GAMMA_VECTOR_R].desc = SANE_DESC_GAMMA_VECTOR_R;
  dev->opt[OPT_GAMMA_VECTOR_R].type = SANE_TYPE_INT;
  dev->opt[OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE | SANE_CAP_ADVANCED;
  dev->opt[OPT_GAMMA_VECTOR_R].unit = SANE_UNIT_NONE;
  dev->opt[OPT_GAMMA_VECTOR_R].size = 256 * sizeof (SANE_Word);
  dev->opt[OPT_GAMMA_VECTOR_R].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_GAMMA_VECTOR_R].constraint.range = &u8_range;
  dev->val[OPT_GAMMA_VECTOR_R].wa = &dev->gamma_table[1][0];

  /* green gamma vector */
  dev->opt[OPT_GAMMA_VECTOR_G].name = SANE_NAME_GAMMA_VECTOR_G;
  dev->opt[OPT_GAMMA_VECTOR_G].title = SANE_TITLE_GAMMA_VECTOR_G;
  dev->opt[OPT_GAMMA_VECTOR_G].desc = SANE_DESC_GAMMA_VECTOR_G;
  dev->opt[OPT_GAMMA_VECTOR_G].type = SANE_TYPE_INT;
  dev->opt[OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE | SANE_CAP_ADVANCED;
  dev->opt[OPT_GAMMA_VECTOR_G].unit = SANE_UNIT_NONE;
  dev->opt[OPT_GAMMA_VECTOR_G].size = 256 * sizeof (SANE_Word);
  dev->opt[OPT_GAMMA_VECTOR_G].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_GAMMA_VECTOR_G].constraint.range = &u8_range;
  dev->val[OPT_GAMMA_VECTOR_G].wa = &dev->gamma_table[2][0];

  /* blue gamma vector */
  dev->opt[OPT_GAMMA_VECTOR_B].name = SANE_NAME_GAMMA_VECTOR_B;
  dev->opt[OPT_GAMMA_VECTOR_B].title = SANE_TITLE_GAMMA_VECTOR_B;
  dev->opt[OPT_GAMMA_VECTOR_B].desc = SANE_DESC_GAMMA_VECTOR_B;
  dev->opt[OPT_GAMMA_VECTOR_B].type = SANE_TYPE_INT;
  dev->opt[OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE | SANE_CAP_ADVANCED;
  dev->opt[OPT_GAMMA_VECTOR_B].unit = SANE_UNIT_NONE;
  dev->opt[OPT_GAMMA_VECTOR_B].size = 256 * sizeof (SANE_Word);
  dev->opt[OPT_GAMMA_VECTOR_B].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_GAMMA_VECTOR_B].constraint.range = &u8_range;
  dev->val[OPT_GAMMA_VECTOR_B].wa = &dev->gamma_table[3][0];

  return SANE_STATUS_GOOD;
}



SANE_Status
sane_init (SANE_Int * version_code, SANE_Auth_Callback authorize)
{
  char dev_name[PATH_MAX], *cp;
  size_t len;
  FILE *fp;
  u_int io_mode = SANEI_PA4S2_OPT_DEFAULT;

  DBG_INIT ();

  if (version_code != NULL)
    *version_code = SANE_VERSION_CODE (V_MAJOR, V_MINOR, MUSTEK_PP_BUILD);

  DBG (3, "init: SANE v%s, backend v%d.%d.%d-%s\n", VERSION, V_MAJOR, V_MINOR,
       MUSTEK_PP_BUILD, MUSTEK_PP_STATE);

#ifdef PATCH_MUSTEK_PP_1505

  DBG (3, "init: patches for ASIC 1505 (0xA2) enabled\n");

#endif

#ifdef HAVE_AUTHORIZATION

  auth_callback = authorize;

  ASSERT (authorize != NULL);

#else

  DBG (1, "I wouldn't let myself be root if I were you...\n");

#endif

  fp = sanei_config_open (MUSTEK_PP_CONFIG_FILE);


  if (fp == NULL)
    {
      DBG (2, "init: no configuration file, using default `port 0x%03x'\n",
	   MUSTEK_PP_DEFAULT_PORT);

      attach (STRINGIFY (MUSTEK_PP_DEFAULT_PORT));
      return SANE_STATUS_GOOD;
    }

  while (sanei_config_read (dev_name, sizeof (dev_name), fp))
    {
      cp = (char *) sanei_config_skip_whitespace (dev_name);
      if (!*cp || *cp == '#')	/* ignore line comments & empty lines */
	continue;

      len = strlen (cp);
      if (cp[len - 1] == '\n')
	cp[--len] = '\0';

      while (len && isspace (cp[len - 1]))
	cp[--len] = '\0';

      if (!len)
	continue;		/* ignore empty lines */


      if (strncmp (cp, "option", 6) == 0 && isspace (cp[6]))
	{

	  cp += 7;
	  cp = (char *) sanei_config_skip_whitespace (cp);

	  if (strncmp (cp, "strip-height", 12) == 0 && isspace (cp[12]))
	    {
	      char *end;
	      int val;

	      errno = 0;
	      cp += 13;
	      val = strtol (cp, &end, 0);
	      if ((end == cp || errno) || (val < 0))
		{
		  DBG (2, "init: invalid value `%s`, falling back to %d\n",
		       cp, strip_height);
		  val = strip_height;	/* safe fallback */
		}

	      DBG (3, "init: option strip-height %d\n", val);

	      if (num_devices == 0)
		{
		  DBG (3, "init: setting global option strip-height to %d\n",
		       val);
		  strip_height = val;
		}
	      else
		{
		  DBG (3, "init: setting strip-height to %d for device %s\n",
		       val, devlist[0].sane.name);
		  devlist[0].strip_height = val;
		}
	    }
	  else if (strncmp (cp, "io-mode", 7) == 0 && isspace (cp[7]))
	    {

	      cp += 8;
	      cp = (char *) sanei_config_skip_whitespace (cp);

	      if (strncmp (cp, "try_mode_uni", 12) == 0)
		io_mode |= SANEI_PA4S2_OPT_TRY_MODE_UNI;
	      else if (strncmp (cp, "alt_lock", 8) == 0)
		io_mode |= SANEI_PA4S2_OPT_ALT_LOCK;
	      else
		DBG (2, "init: unknown io-mode `%s'\n", cp);

	      DBG (3, "init: option io-mode %d\n", io_mode);

	    }
	  else if (strncmp (cp, "wait-bank", 9) == 0 && isspace (cp[9]))
	    {
	      char *end;
	      int val;

	      cp += 10;

	      errno = 0;
	      val = strtol (cp, &end, 0);

	      if ((end == cp || errno) || (val < 0))
		{
		  DBG (2, "init: invalid value `%s`, falling back to %d\n",
		       cp, wait_bank);
		  val = wait_bank;	/* safe fallback */
		}

	      DBG (3, "init: option wait-bank %d\n", val);

	      if (num_devices == 0)
		{
		  DBG (3, "init: setting global option wait-bank to %d\n",
		       val);
		  wait_bank = val;
		}
	      else
		{
		  DBG (3, "init: setting wait-bank to %d for device %s\n",
		       val, devlist[0].sane.name);
		  devlist[0].wait_bank = val;
		}
	    }
	  else if (strncmp (cp, "bw", 2) == 0 && isspace (cp[2]))
	    {
	      char *end;
	      int val;

	      cp += 3;

	      errno = 0;
	      val = strtol (cp, &end, 0);

	      if ((end == cp || errno) || (val < 0) || (val > 255))
		{
		  DBG (2, "init: invalid value `%s`, falling back to %d\n",
		       cp, bw);
		  val = bw;	/* safe fallback */
		}

	      DBG (3, "init: option bw %d\n", val);

	      if (num_devices == 0)
		{
		  DBG (3, "init: setting global option bw to %d\n", val);
		  bw = val;
		}
	      else
		{
		  DBG (3, "init: setting bw to %d for device %s\n",
		       val, devlist[0].sane.name);
		  devlist[0].bw = val;
		}
	    }
	  else if (strncmp (cp, "wait-lamp", 9) == 0 && isspace (cp[9]))
	    {
	      char *end;
	      int val;

	      cp += 10;

	      errno = 0;
	      val = strtol (cp, &end, 0);

	      if ((end == cp || errno) || (val < 0))
		{
		  DBG (2, "init: invalid value `%s`, falling back to %d\n",
		       cp, wait_lamp);
		  val = wait_lamp;	/* safe fallback */
		}

	      DBG (3, "init: option wait-lamp %d\n", val);

	      if (num_devices == 0)
		{
		  DBG (3, "init: setting global option wait-lamp to %d\n",
		       val);
		  wait_lamp = val;
		}
	      else
		{
		  DBG (3, "init: setting wait-lamp to %d for device %s\n",
		       val, devlist[0].sane.name);
		  devlist[0].wait_lamp = val;
		}
	    }
	  else if (strncmp (cp, "buffer", 6) == 0 && isspace (cp[6]))
	    {
	      char *end;
	      long int val;

	      cp += 7;

	      errno = 0;
	      val = strtol (cp, &end, 0);

	      if ((end == cp || errno) || (val < 0))
		{
		  DBG (2, "init: invalid value `%s`, falling back to %ld\n",
		       cp, buf_size);
		  val = buf_size;	/* safe fallback */
		}

	      DBG (3, "init: option buffer %ld\n", val);

	      if (num_devices == 0)
		{
		  DBG (3, "init: setting global option buffer to %ld\n", val);
		  buf_size = val;
		}
	      else
		{
		  DBG (3, "init: setting buffer to %ld for device %s\n",
		       val, devlist[0].sane.name);
		  devlist[0].buf_size = val;
		}
	    }
	  else if (strncmp (cp, "niceload", 8) == 0)
	    {
	      DBG (3, "init: option niceload\n");
	      niceload = 1;
	    }
	  else if (strncmp (cp, "use600", 6) == 0)
	    {
	      if (num_devices == 0)
		DBG (2, "init: option use600 isn't a global option\n");
	      else
		{
		  DBG (3, "init: device %s is a 600 dpi scanner\n",
		       devlist[0].sane.name);
		  if (devlist[0].use600 == SANE_FALSE)
		    {
		      devlist[0].use600 = SANE_TRUE;
		      devlist[0].max_res = 600;
		      devlist[0].max_h_size = MUSTEK_PP_101x_MAX_H_PIXEL * 2;
		      devlist[0].max_v_size = MUSTEK_PP_101x_MAX_V_PIXEL * 2;
		    }
		  else
		    {
		      DBG (2, "init: option use600 alreay present...\n");
		    }
		}
	    }
	  else if (strncmp (cp, "auth", 4) == 0)
	    {
#ifndef HAVE_AUTHORIZATION
	      DBG (1, "init: user authentification support not compiled\n");
	      DEBUG ();
#endif
	      if (num_devices == 0)
		DBG (2, "init: option auth isn't a global option\n");
	      else
		{
		  DBG (3, "init: device %s requires user authentification\n",
		       devlist[0].sane.name);
		  devlist[0].requires_auth = SANE_TRUE;
		}
	    }
	  else
	    DBG (2, "init: unknown option `%s'\n", cp);
	  continue;
	}
      else if ((strncmp (cp, "port", 4) == 0) && isspace (cp[4]))
	{

	  cp += 5;
	  cp = (char *) sanei_config_skip_whitespace (cp);

	  DBG (3, "init: trying port `%s'\n", cp);

	  if (*cp)
	    if (attach (cp) != SANE_STATUS_GOOD)
	      DBG (2, "init: couldn't attach to port `%s'\n", cp);
	}
      else if ((strncmp (cp, "name", 4) == 0) && isspace (cp[4]))
	{
	  cp += 5;
	  cp = (char *) sanei_config_skip_whitespace (cp);

	  if (num_devices == 0)
	    DBG (2, "init: 'name' only allowed after 'port'\n");
	  else
	    {
	      DBG (3, "init: naming device %s '%s'\n", devlist[0].port, cp);
	      free (devlist[0].sane.name);
	      devlist[0].sane.name = strdup (cp);
	    }
	}
      else if ((strncmp (cp, "model", 5) == 0) && isspace (cp[5]))
	{
	  cp += 6;
	  cp = (char *) sanei_config_skip_whitespace (cp);

	  if (num_devices == 0)
	    DBG (2, "init: 'model' only allowed after 'port'\n");
	  else
	    {
	      DBG (3, "init: device %s is a '%s'\n", devlist[0].port, cp);
	      free (devlist[0].sane.model);
	      devlist[0].sane.model = strdup (cp);
	    }
	}
      else if ((strncmp (cp, "vendor", 6) == 0) && isspace (cp[6]))
	{
	  cp += 7;
	  cp = (char *) sanei_config_skip_whitespace (cp);

	  if (num_devices == 0)
	    DBG (2, "init: 'vendor' only allowed after 'port'\n");
	  else
	    {
	      DBG (3, "init: device %s is from '%s'\n", devlist[0].port, cp);
	      free (devlist[0].sane.vendor);
	      devlist[0].sane.vendor = strdup (cp);
	    }
	}
      else
	DBG (2, "init: don't know what to do with `%s'\n", cp);
    }

  fclose (fp);

  sanei_pa4s2_options (&io_mode, SANE_TRUE);

  return SANE_STATUS_GOOD;
}

void
sane_exit (void)
{
  int i;
  Mustek_PP_Device *dev;

  if (first_dev)
    DBG (3, "exit: closing open devices\n");

  while (first_dev)
    {
      dev = first_dev;
      sane_close (dev);
    }

  for (i = 0; i < num_devices; i++)
    {
      free (devlist[i].port);
      free (devlist[i].sane.name);
      free (devlist[i].sane.model);
      free (devlist[i].sane.vendor);
    }

  if (devlist != NULL)
    free (devlist);

  if (devarray != NULL)
    free (devarray);

  DBG (3, "exit: (...)\n");

  num_devices = 0;
}

SANE_Status
sane_get_devices (const SANE_Device *** device_list, SANE_Bool local_only)
{
  int i;

  DBG (129, "unused arg: local_only = %d\n", (int) local_only);

  if (devarray != NULL)
    free (devarray);

  devarray = malloc ((num_devices + 1) * sizeof (devarray[0]));

  if (devarray == NULL)
    {
      DBG (2, "get_devices: not enough memory for device list\n");
      DEBUG ();
      return SANE_STATUS_NO_MEM;
    }

  for (i = 0; i < num_devices; i++)
    devarray[i] = &devlist[i].sane;

  devarray[num_devices] = NULL;
  *device_list = devarray;

  return SANE_STATUS_GOOD;
}

SANE_Status
sane_open (SANE_String_Const devicename, SANE_Handle * handle)
{
  Mustek_PP_Device *dev;
  Mustek_PP_Descriptor *desc;
  SANE_Status status;
  int i, j, fd;

  DBG (3, "open: device `%s'\n", devicename);

  if (devicename[0])
    {
      for (i = 0; i < num_devices; i++)
	if (strcmp (devlist[i].sane.name, devicename) == 0)
	  break;

      if (i >= num_devices)
	for (i = 0; i < num_devices; i++)
	  if (strcmp (devlist[i].port, devicename) == 0)
	    break;

      if (i >= num_devices)
	{
	  DBG (2, "open: device doesn't exist\n");
	  DEBUG ();
	  return SANE_STATUS_INVAL;
	}

      desc = &devlist[i];

#ifdef HAVE_AUTHORIZATION

      if (desc->requires_auth == SANE_TRUE)
	if (do_authorization (devlist[i].port) != SANE_STATUS_GOOD)
	  {
	    DBG (2, "open: access denied\n");
	    DEBUG ();
	    return SANE_STATUS_ACCESS_DENIED;
	  }

#else

      /* authorization should realy be compiled */
/*      DBG (1, "I wouldn't let myself be root if I were you\n"); */

      if (desc->requires_auth == SANE_TRUE)
	{
	  DBG (1, "open: ressource %s requires user authentification\n",
	       desc->port);
	  DBG (3, "open: ... which isn't compiled :-(\n");
	  DBG (2, "open: access denied\n");
	  return SANE_STATUS_ACCESS_DENIED;
	}

#endif

      status = sanei_pa4s2_open (devlist[i].port, &fd);
    }
  else
    {

      if (num_devices == 0)
	{
	  DBG (1, "open: no devices present\n");
	  return SANE_STATUS_INVAL;
	}

      DBG (3, "open: trying default device %s\n", devlist[0].sane.name);

#ifdef HAVE_AUTHORIZATION

      if (devlist[0].requires_auth == SANE_TRUE)
	if (do_authorization (devlist[0].port) != SANE_STATUS_GOOD)
	  {
	    DBG (2, "open: access denied\n");
	    DEBUG ();
	    return SANE_STATUS_ACCESS_DENIED;
	  }

#else

      /* authorization should realy be compiled */
/*      DBG (1, "I wouldn't let myself be root if I were you\n"); */
      if (devlist[0].requires_auth == SANE_TRUE)
	{
	  DBG (1, "open: ressource %s requires user authentification\n",
	       devlist[0].port);
	  DBG (3, "open: ... which isn't compiled :-(\n");
	  DBG (2, "open: access denied\n");
	  return SANE_STATUS_ACCESS_DENIED;
	}

#endif


      status = sanei_pa4s2_open (devlist[0].port, &fd);

      desc = &devlist[0];
    }

  if (status != SANE_STATUS_GOOD)
    {
      DBG (2, "open: device not found (%s)\n", sane_strstatus (status));
      DEBUG ();
      return status;
    }

  dev = (Mustek_PP_Device *) malloc (sizeof (*dev));

  if (dev == NULL)
    {
      DBG (2, "open: not enough memory for device descriptor\n");
      DEBUG ();
      sanei_pa4s2_close (fd);
      return SANE_STATUS_NO_MEM;
    }

  memset (dev, 0, sizeof (*dev));

  dev->fd = fd;

  dev->desc = desc;

  dev->asic_id = dev->desc->asic;
  dev->ccd_type = dev->desc->ccd;

  dev->motor_ctrl = 0x43;
  dev->first_time = 1;
  dev->unknown_value = 0xAA;
  dev->expose_time = 0x64;
  dev->send_voltages = 0;

  for (i = 0; i < 4; ++i)
    for (j = 0; j < 256; ++j)
      dev->gamma_table[i][j] = j;

  if (dev->desc->buf_size < 3 * dev->desc->max_h_size)
    {

      DBG (2, "open: scan buffer to small, falling back to %d bytes\n",
	   dev->desc->max_h_size * 3);
      dev->desc->buf_size = dev->desc->max_h_size * 3;
    }

  dev->buf = malloc (dev->desc->buf_size);
  dev->bufsize = dev->desc->buf_size;

  dev->dpi_range.min = SANE_FIX (50);
  dev->dpi_range.max = SANE_FIX (dev->desc->max_res);
  dev->dpi_range.quant = SANE_FIX (1);

  dev->x_range.min = 0;
  dev->x_range.max =
    PIXEL_TO_MM (dev->desc->max_h_size, (float) dev->desc->max_res);
  dev->x_range.quant = 0;

  dev->y_range.min = 0;
  dev->y_range.max =
    PIXEL_TO_MM (dev->desc->max_v_size, (float) dev->desc->max_res);
  dev->y_range.quant = 0;

  DBG (6, "open: range (0.0,0.0)-(%f,%f)\n", SANE_UNFIX (dev->x_range.max),
       SANE_UNFIX (dev->y_range.max));


  if (dev->buf == NULL)
    {
      DBG (2, "open: not enough memory for scan buffer (%lu bytes)\n",
	   (long int) dev->desc->buf_size);
      DEBUG ();
      sanei_pa4s2_close (fd);
      free (dev);
      return SANE_STATUS_NO_MEM;
    }

  init_options (dev);

  dev->next = first_dev;
  first_dev = dev;

  sanei_pa4s2_enable (dev->fd, SANE_TRUE);

  lamp (dev, SANE_TRUE);

  if (dev->desc->use600)
    dev->params.pixels_per_line = 10;

  dev->CCD.hwres = dev->CCD.res = dev->desc->max_res;
  dev->CCD.mode = MUSTEK_PP_MODE_COLOR;

  return_home (dev, SANE_TRUE);


  sanei_pa4s2_enable (dev->fd, SANE_FALSE);

  dev->lamp_on = time (NULL);
  dev->max_lines = dev->desc->strip_height;

  *handle = dev;

  DBG (3, "open: success\n");

  return SANE_STATUS_GOOD;
}

void
sane_close (SANE_Handle handle)
{
  Mustek_PP_Device *prev, *dev;

  /* remove handle from list of open handles: */
  prev = NULL;

  for (dev = first_dev; dev; dev = dev->next)
    {
      if (dev == handle)
	break;
      prev = dev;
    }

  if (dev == NULL)
    {
      DBG (2, "close: unknown device\n");
      DEBUG ();
      return;			/* oops, not a handle we know about */
    }

  if (dev->state == MUSTEK_PP_STATE_SCANNING)
    sane_cancel (handle);	/* remember: sane_cancel is a macro and
				   expands to sane_mustek_pp_cancel ()... */

  if (prev != NULL)
    prev->next = dev->next;
  else
    first_dev = dev->next;	/* mustek.c had a bug at this point :-) */

  DBG (3, "close: maybe waiting for lamp...\n");
  while (time (NULL) - dev->lamp_on < 2)
    sleep (1);

  sanei_pa4s2_enable (dev->fd, SANE_TRUE);
  lamp (dev, SANE_FALSE);
  return_home (dev, SANE_FALSE);
  sanei_pa4s2_enable (dev->fd, SANE_FALSE);

  sanei_pa4s2_close (dev->fd);
  free (dev->buf);

  DBG (3, "close: device closed\n");

  free (handle);

}

const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
  Mustek_PP_Device *dev = handle;

  if ((unsigned) option >= NUM_OPTIONS)
    {
      DBG (2, "get_option_descriptor: option %d doesn't exist\n", option);
      DEBUG ();
      return NULL;
    }

  DBG (6, "get_option_descriptor: requested option %d (%s)\n",
       option, dev->opt[option].name);

  return dev->opt + option;
}

SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option,
		     SANE_Action action, void *val, SANE_Int * info)
{
  Mustek_PP_Device *dev = handle;
  SANE_Status status;
  SANE_Word w, cap;

  DBG (6, "control_option: option %d, action %d\n", option, action);

  if (info)
    *info = 0;

  if (dev->state == MUSTEK_PP_STATE_SCANNING)
    {
      DBG (2, "control_option: device is scanning\n");
      DEBUG ();
      return SANE_STATUS_DEVICE_BUSY;
    }

  if ((unsigned int) option >= NUM_OPTIONS)
    {
      DBG (2, "control_option: option doesn't exist\n");
      DEBUG ();
      return SANE_STATUS_INVAL;
    }

  cap = dev->opt[option].cap;

  if (!SANE_OPTION_IS_ACTIVE (cap))
    {
      DBG (2, "control_option: option isn't active\n");
      DEBUG ();
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
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
	case OPT_NUM_OPTS:
	case OPT_CUSTOM_GAMMA:
#ifdef HAVE_INVERSION
	case OPT_INVERT:
#endif

	  *(SANE_Word *) val = dev->val[option].w;
	  return SANE_STATUS_GOOD;

	  /* word-array options: */
	case OPT_GAMMA_VECTOR:
	case OPT_GAMMA_VECTOR_R:
	case OPT_GAMMA_VECTOR_G:
	case OPT_GAMMA_VECTOR_B:

	  memcpy (val, dev->val[option].wa, dev->opt[option].size);
	  return SANE_STATUS_GOOD;

	  /* string options: */
	case OPT_MODE:

	  strcpy (val, dev->val[option].s);
	  return SANE_STATUS_GOOD;
	}
    }
  else if (action == SANE_ACTION_SET_VALUE)
    {

      if (!SANE_OPTION_IS_SETTABLE (cap))
	{
	  DBG (2, "control_option: option can't be set\n");
	  DEBUG ();
	  return SANE_STATUS_INVAL;
	}

      status = sanei_constrain_value (dev->opt + option, val, info);

      if (status != SANE_STATUS_GOOD)
	{
	  DBG (2, "control_option: constrain_value failed (%s)\n",
	       sane_strstatus (status));
	  DEBUG ();
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
	case OPT_PREVIEW:
	case OPT_GRAY_PREVIEW:
#ifdef HAVE_INVERSION
	case OPT_INVERT:
#endif

	  if (info)
	    *info |= SANE_INFO_RELOAD_PARAMS;

	  dev->val[option].w = *(SANE_Word *) val;
	  return SANE_STATUS_GOOD;

	  /* side-effect-free word-array options: */
	case OPT_GAMMA_VECTOR:
	case OPT_GAMMA_VECTOR_R:
	case OPT_GAMMA_VECTOR_G:
	case OPT_GAMMA_VECTOR_B:

	  memcpy (dev->val[option].wa, val, dev->opt[option].size);
	  return SANE_STATUS_GOOD;


	  /* options with side-effects: */

	case OPT_CUSTOM_GAMMA:
	  w = *(SANE_Word *) val;

	  if (w == dev->val[OPT_CUSTOM_GAMMA].w)
	    return SANE_STATUS_GOOD;	/* no change */

	  if (info)
	    *info |= SANE_INFO_RELOAD_OPTIONS;

	  dev->val[OPT_CUSTOM_GAMMA].w = w;

	  if (w == SANE_TRUE)
	    {
	      const char *mode = dev->val[OPT_MODE].s;

	      if (strcmp (mode, "Gray") == 0)
		dev->opt[OPT_GAMMA_VECTOR].cap &= ~SANE_CAP_INACTIVE;
	      else if (strcmp (mode, "Color") == 0)
		{
		  dev->opt[OPT_GAMMA_VECTOR].cap &= ~SANE_CAP_INACTIVE;
		  dev->opt[OPT_GAMMA_VECTOR_R].cap &= ~SANE_CAP_INACTIVE;
		  dev->opt[OPT_GAMMA_VECTOR_G].cap &= ~SANE_CAP_INACTIVE;
		  dev->opt[OPT_GAMMA_VECTOR_B].cap &= ~SANE_CAP_INACTIVE;
		}
	    }
	  else
	    {
	      dev->opt[OPT_GAMMA_VECTOR].cap |= SANE_CAP_INACTIVE;
	      dev->opt[OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
	      dev->opt[OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
	      dev->opt[OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;
	    }

	  return SANE_STATUS_GOOD;

	case OPT_MODE:
	  {
	    char *old_val = dev->val[option].s;

	    if (old_val)
	      {
		if (strcmp (old_val, val) == 0)
		  return SANE_STATUS_GOOD;	/* no change */

		free (old_val);
	      }

	    if (info)
	      *info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;

	    dev->val[option].s = strdup (val);

	    dev->opt[OPT_CUSTOM_GAMMA].cap |= SANE_CAP_INACTIVE;
	    dev->opt[OPT_GAMMA_VECTOR].cap |= SANE_CAP_INACTIVE;
	    dev->opt[OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
	    dev->opt[OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
	    dev->opt[OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;

	    if (strcmp (val, "Lineart") != 0)
	      dev->opt[OPT_CUSTOM_GAMMA].cap &= ~SANE_CAP_INACTIVE;

	    if (dev->val[OPT_CUSTOM_GAMMA].w == SANE_TRUE)
	      {
		if (strcmp (val, "Gray") == 0)
		  dev->opt[OPT_GAMMA_VECTOR].cap &= ~SANE_CAP_INACTIVE;
		else if (strcmp (val, "Color") == 0)
		  {
		    dev->opt[OPT_GAMMA_VECTOR].cap &= ~SANE_CAP_INACTIVE;
		    dev->opt[OPT_GAMMA_VECTOR_R].cap &= ~SANE_CAP_INACTIVE;
		    dev->opt[OPT_GAMMA_VECTOR_G].cap &= ~SANE_CAP_INACTIVE;
		    dev->opt[OPT_GAMMA_VECTOR_B].cap &= ~SANE_CAP_INACTIVE;
		  }
	      }

	    return SANE_STATUS_GOOD;
	  }
	}
    }

  DBG (2, "control_option: unknown action\n");
  DEBUG ();
  return SANE_STATUS_INVAL;
}


SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters * params)
{
  Mustek_PP_Device *dev = handle;
  char *mode;

  if (dev->state != MUSTEK_PP_STATE_SCANNING)
    {

      int dpi;

      memset (&dev->params, 0, sizeof (dev->params));

      mode = dev->val[OPT_MODE].s;

      if (strcmp (mode, "Lineart") == 0)
	dev->CCD.mode = MUSTEK_PP_MODE_LINEART;
      else if (strcmp (mode, "Gray") == 0)
	dev->CCD.mode = MUSTEK_PP_MODE_GRAYSCALE;
      else
	dev->CCD.mode = MUSTEK_PP_MODE_COLOR;


      if (dev->val[OPT_PREVIEW].w == SANE_TRUE)
	{

	  if (dev->val[OPT_GRAY_PREVIEW].w == SANE_TRUE)
	    dev->CCD.mode = MUSTEK_PP_MODE_GRAYSCALE;
	  else
	    dev->CCD.mode = MUSTEK_PP_MODE_COLOR;

	}

      DBG (3, "get_parameters: mode `%s'\n", mode);

      dpi = (int) (SANE_UNFIX (dev->val[OPT_RESOLUTION].w) + 0.5);
      dev->CCD.res = dpi;

      DBG (3, "get_parameters: %d dpi\n", dpi);

      if (dpi <= 100)
	dev->CCD.hwres = 100;
      else if (dpi <= 200)
	dev->CCD.hwres = 200;
      else if (dpi <= 300)
	dev->CCD.hwres = 300;
      else if (dpi <= 400)
	dev->CCD.hwres = 400;
      else if (dpi <= 600)
	dev->CCD.hwres = 600;

      /* TODO: add 1505 code for hw resolution here */

      DBG (6, "get_parameters: resolution %d dpi -> hardware %d dpi\n",
	   dev->CCD.res, dev->CCD.hwres);

#ifdef HAVE_INVERSION
      dev->CCD.invert = dev->val[OPT_INVERT].w;
#else
      dev->CCD.invert = SANE_FALSE;
#endif

      dev->TopX =
	MIN ((int)
	     (MM_TO_PIXEL (dev->val[OPT_TL_X].w, (float) dev->desc->max_res) +
	      0.5), dev->desc->max_h_size);
      dev->TopY =
	MIN ((int)
	     (MM_TO_PIXEL (dev->val[OPT_TL_Y].w, (float) dev->desc->max_res) +
	      0.5), dev->desc->max_v_size);

      dev->BottomX =
	MIN ((int)
	     (MM_TO_PIXEL (dev->val[OPT_BR_X].w, (float) dev->desc->max_res) +
	      0.5), dev->desc->max_h_size);
      dev->BottomY =
	MIN ((int)
	     (MM_TO_PIXEL (dev->val[OPT_BR_Y].w, (float) dev->desc->max_res) +
	      0.5), dev->desc->max_v_size);

      dev->params.pixels_per_line = (dev->BottomX - dev->TopX) * dev->CCD.res
	/ dev->desc->max_res;

      dev->params.bytes_per_line = dev->params.pixels_per_line;

      switch (dev->CCD.mode)
	{

	case MUSTEK_PP_MODE_LINEART:
	  dev->params.bytes_per_line /= 8;

	  if ((dev->params.pixels_per_line % 8) != 0)
	    dev->params.bytes_per_line++;

	  dev->params.depth = 1;
	  break;

	case MUSTEK_PP_MODE_GRAYSCALE:
	  dev->params.depth = 8;
	  dev->params.format = SANE_FRAME_GRAY;
	  break;

	case MUSTEK_PP_MODE_COLOR:
	  dev->params.depth = 8;
	  dev->params.bytes_per_line *= 3;
	  dev->params.format = SANE_FRAME_RGB;
	  break;

	}

      dev->params.last_frame = SANE_TRUE;

      dev->params.lines = (dev->BottomY - dev->TopY) * dev->CCD.res /
	dev->desc->max_res;

      DBG (3, "get_parameters: %dx%d pixels\n", dev->params.pixels_per_line,
	   dev->params.lines);

      dev->CCD.skipimagebytes = dev->TopX;

      ASSERT (dev->params.lines > 0);
      ASSERT (dev->params.pixels_per_line > 0);

    }
  else
    {
      DBG (2, "get_parameters: can't set parameters while scanning\n");
      DEBUG ();
    }

  if (params != NULL)
    *params = dev->params;

  return SANE_STATUS_GOOD;

}

SANE_Status
sane_start (SANE_Handle handle)
{
  Mustek_PP_Device *dev = handle;

  if (dev->state == MUSTEK_PP_STATE_SCANNING)
    {
      DBG (2, "start: device is already scanning\n");
      DEBUG ();

      return SANE_STATUS_GOOD;
    }

  sane_get_parameters (handle, NULL);

  DBG (3, "start: maybe waiting for lamp...\n");
  /* image quality increases when lamp isn't to cold */
  while ((time (NULL) - dev->lamp_on) < dev->desc->wait_lamp)
    sleep (1);

  sanei_pa4s2_enable (dev->fd, SANE_TRUE);

  if (dev->desc->use600 == SANE_FALSE)
    {

      config_ccd (dev);
      set_voltages (dev);

      get_bank_count (dev);
      ASSERT (dev->bank_count == 0);

    }

  return_home (dev, SANE_FALSE);

  dev->motor_step = 0;

  /* allocating memory for calibration */

  dev->calib_g = malloc (dev->params.pixels_per_line);

  if (dev->calib_g == NULL)
    {
      sanei_pa4s2_enable (dev->fd, SANE_FALSE);
      DBG (2, "start: not enough memory for calibration buffer\n");
      DEBUG ();

      return SANE_STATUS_NO_MEM;
    }

  if (dev->CCD.mode == MUSTEK_PP_MODE_COLOR)
    {

      dev->calib_r = malloc (dev->params.pixels_per_line);
      dev->calib_b = malloc (dev->params.pixels_per_line);

      if ((dev->calib_r == NULL) || (dev->calib_b == NULL))
	{
	  free (dev->calib_g);
	  if (dev->calib_r)
	    free (dev->calib_r);
	  if (dev->calib_b)
	    free (dev->calib_b);

	  sanei_pa4s2_enable (dev->fd, SANE_FALSE);

	  DBG (2, "start: not enough memory for color calib buffer\n");
	  DEBUG ();

	  return SANE_STATUS_NO_MEM;
	}
    }

  DBG (3, "start: executing calibration\n");

  calibrate (dev);

  /* line distance correction for color mode */
  if (dev->ccd_type == 1)
    {

      dev->blue_offs = 4;
      dev->green_offs = 8;

    }
  else
    {

      dev->blue_offs = 8;
      dev->green_offs = 16;

    }

  if (dev->desc->use600)
    {

      dev->blue_offs = 2;
      dev->green_offs = 4;

      if (dev->CCD.hwres > 300)
	{

	  dev->blue_offs = 4;
	  dev->green_offs = 8;

	}


    }


  if (dev->desc->use600)
    {

      dev->motor_ctrl = 0x63;

      move_motor (dev, /* (dev->CCD.mode == MUSTEK_PP_MODE_COLOR ? */ 136	/* : 95) */
		   + dev->TopY -
		  (dev->CCD.mode ==
		   MUSTEK_PP_MODE_COLOR ? dev->green_offs : 0), SANE_TRUE);

      dev->motor_ctrl = 0x43;
    }
  else
    /* musteka4s2 says 56 lines instead of 47 */
    move_motor (dev, 47 + dev->TopY -
		(dev->CCD.mode == MUSTEK_PP_MODE_COLOR ? dev->green_offs : 0),
		SANE_TRUE);

  if ((dev->ccd_type == 1) && (dev->desc->use600 == SANE_FALSE))
    sanei_pa4s2_writebyte (dev->fd, 6, 0x15);

  sanei_pa4s2_enable (dev->fd, SANE_FALSE);

  if (dev->CCD.mode == MUSTEK_PP_MODE_COLOR)
    {

      int failed = SANE_FALSE, cnt;

      dev->CCD.line_step =
	SANE_FIX ((float) dev->desc->max_res / (float) dev->CCD.res);
      dev->rdiff = dev->CCD.line_step;
      dev->bdiff = dev->rdiff + (dev->blue_offs << SANE_FIXED_SCALE_SHIFT);
      dev->gdiff = dev->rdiff + (dev->green_offs << SANE_FIXED_SCALE_SHIFT);



      dev->red = malloc (sizeof (SANE_Byte *) * dev->green_offs);
      dev->blue = malloc (sizeof (SANE_Byte *) * dev->blue_offs);
      dev->green = malloc (dev->params.pixels_per_line);

      if ((dev->red == NULL) || (dev->blue == NULL) || (dev->green == NULL))
	{

	  free (dev->calib_r);
	  free (dev->calib_b);
	  free (dev->calib_g);

	  if (dev->red != NULL)
	    free (dev->red);
	  if (dev->green != NULL)
	    free (dev->green);
	  if (dev->blue != NULL)
	    free (dev->blue);

	  DBG (2, "start: not enough memory for ld correction buffers\n");
	  DEBUG ();

	  return SANE_STATUS_NO_MEM;

	}

      for (cnt = 0; cnt < dev->green_offs; cnt++)
	if ((dev->red[cnt] = malloc (dev->params.pixels_per_line)) == NULL)
	  failed = SANE_TRUE;

      for (cnt = 0; cnt < dev->blue_offs; cnt++)
	if ((dev->blue[cnt] = malloc (dev->params.pixels_per_line)) == NULL)
	  failed = SANE_TRUE;

      if (failed == SANE_TRUE)
	{

	  free (dev->calib_r);
	  free (dev->calib_b);
	  free (dev->calib_g);


	  for (cnt = 0; cnt < dev->green_offs; cnt++)
	    if (dev->red[cnt] != NULL)
	      free (dev->red[cnt]);
	  for (cnt = 0; cnt < dev->blue_offs; cnt++)
	    if (dev->blue[cnt] != NULL)
	      free (dev->blue[cnt]);

	  free (dev->red);
	  free (dev->green);
	  free (dev->blue);

	  DBG (2, "start: not enough memory for ld correction buffers\n");
	  DEBUG ();

	  return SANE_STATUS_NO_MEM;

	}

      dev->redline = 0;
      dev->blueline = 0;
      dev->ccd_line = 0;

    }

  dev->line = 0;
  dev->lines_left = dev->params.lines;

  dev->state = MUSTEK_PP_STATE_SCANNING;

  DBG (3, "start: device ready for scanning\n");

  return SANE_STATUS_GOOD;

}


SANE_Status
sane_read (SANE_Handle handle, SANE_Byte * buf, SANE_Int max_len,
	   SANE_Int * len)
{
  Mustek_PP_Device *dev = handle;
  long int n, cnt;

  *len = 0;

  if ((dev->state != MUSTEK_PP_STATE_SCANNING) ||
      ((dev->lines_left == 0) && (dev->buflen == 0)))
    {
      /* device isn't scanning */

      if (dev->state != MUSTEK_PP_STATE_SCANNING)
	{
	  DBG (2, "read: device isn't scanning\n");
	  DEBUG ();
	  dev->state = MUSTEK_PP_STATE_IDLE;
	  return SANE_STATUS_CANCELLED;
	}
      else
	{
	  DBG (2, "read: no more image data available\n");
	  DEBUG ();

	  DBG (3, "read: finishing scan request using sane_cancel\n");

	  sane_cancel (handle);

	}

      return SANE_STATUS_EOF;
    }


  if (dev->buflen == 0)
    {

      /* no image data in scan buffer -> get new */

      if (dev->max_lines > 0)
	cnt = MIN (dev->bufsize / dev->params.bytes_per_line, dev->max_lines);
      else
	cnt = dev->bufsize / dev->params.bytes_per_line;

      cnt = MIN (cnt, dev->lines_left);

      ASSERT (cnt > 0);

      DBG (3, "read: scanning next %ld lines\n", cnt);

      sanei_pa4s2_enable (dev->fd, SANE_TRUE);

      for (n = 0; n < cnt; n++)
	{

	  switch (dev->CCD.mode)
	    {

	    case MUSTEK_PP_MODE_LINEART:

	      get_lineart_line (dev,
				&dev->buf[dev->params.bytes_per_line * n]);
	      break;

	    case MUSTEK_PP_MODE_GRAYSCALE:

	      get_grayscale_line (dev,
				  &dev->buf[dev->params.bytes_per_line * n]);
	      break;

	    case MUSTEK_PP_MODE_COLOR:

	      get_color_line (dev, &dev->buf[dev->params.bytes_per_line * n]);
	      break;

	    }

	  dev->lines_left--;
	  dev->line++;

	}

      dev->buflen = cnt * dev->params.bytes_per_line;

      if (dev->lines_left == 0)
	{
	  DBG (3, "read: scan finished\n");

	  return_home (dev, SANE_TRUE);

	}

      sanei_pa4s2_enable (dev->fd, SANE_FALSE);

    }

  /* return requested amount of data from buffer */

  ASSERT (dev->buflen > 0);

  memcpy (buf, dev->buf, MIN (max_len, dev->buflen));

  *len = MIN (max_len, dev->buflen);

  ASSERT (*len > 0) DBG (3, "read: delivering %d bytes\n", *len);

  if (*len == dev->buflen)
    dev->buflen = 0;
  else
    {
      dev->buflen -= *len;
      memmove (dev->buf, &dev->buf[*len], dev->buflen);
    }

  return SANE_STATUS_GOOD;

}

void
sane_cancel (SANE_Handle handle)
{
  Mustek_PP_Device *dev = handle;

  if (dev->state == MUSTEK_PP_STATE_SCANNING)
    {

      /* device is scanning: return lamp and free line distance correction
         and calibration buffers
       */

      DBG (3, "cancel: stopping current scan\n");

      sanei_pa4s2_enable (dev->fd, SANE_TRUE);
      return_home (dev, SANE_TRUE);
      sanei_pa4s2_enable (dev->fd, SANE_FALSE);

      free (dev->calib_g);

      if (dev->CCD.mode == MUSTEK_PP_MODE_COLOR)
	{

	  int cnt;

	  free (dev->calib_r);
	  free (dev->calib_b);

	  for (cnt = 0; cnt < dev->green_offs; cnt++)
	    free (dev->red[cnt]);
	  for (cnt = 0; cnt < dev->blue_offs; cnt++)
	    free (dev->blue[cnt]);

	  free (dev->red);
	  free (dev->blue);
	  free (dev->green);

	}

      dev->buflen = 0;

      dev->state = MUSTEK_PP_STATE_CANCELLED;

    }
  else
    {

      DBG (2, "cancel: device isn't scanning\n");
      DEBUG ();
    }
}

SANE_Status
sane_set_io_mode (SANE_Handle handle, SANE_Bool non_blocking)
{
  DBG (129, "unused arg: handle = %p, non_blocking = %d\n",
       handle, (int) non_blocking);

  DBG (2, "set_io_mode: not supported\n");

  return SANE_STATUS_UNSUPPORTED;
}

SANE_Status
sane_get_select_fd (SANE_Handle handle, SANE_Int * fd)
{

  DBG (129, "unused arg: handle = %p, fd = %p\n", handle, fd);

  DBG (2, "get_select_fd: not supported\n");

  return SANE_STATUS_UNSUPPORTED;
}
