/* sane - Scanner Access Now Easy.
   Copyright (C) 2001 Stéphane Voltz <svoltz@wanadoo.fr>
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

   This file implements a SANE backend for Umax PP flatbed scanners.  */

#include <stdlib.h>
#include <stdio.h>
#include "../include/sane/config.h"
#include "../include/sane/sanei_debug.h"

#define __MAIN__


#include "umax_pp_mid.h"

/* this function locks the parallel port so that other devices */
/* won't interfere. Returns UMAX1220P_BUSY is port cannot be   */
/* lock or UMAX1220P_OK if it is locked                        */
int locked = 0;

static int
lock_parport (void)
{
  DBG_INIT ();
#ifdef HAVE_LINUX_PPDEV_H
  if ((sanei_umax_pp_getparport () > 0) && (!locked))
    {
      if (ioctl (sanei_umax_pp_getparport (), PPCLAIM))
	{
	  return (UMAX1220P_BUSY);
	}
      locked = 1;
    }
#else
  locked = 1;
#endif
  return (UMAX1220P_OK);
}


/* this function release parport */
static int
unlock_parport (void)
{
#ifdef HAVE_LINUX_PPDEV_H
  if ((sanei_umax_pp_getparport () > 0) && (locked))
    {
      ioctl (sanei_umax_pp_getparport (), PPRELEASE);
      locked = 1;
    }
#endif
  locked = 0;
  return (UMAX1220P_OK);
}




/* 
 *
 *  This function recognize the scanner model by sending an image
 * filter command. 1220P will use it as is, but 2000P will return
 * it back modified.
 *
 */
int
sanei_umax_pp_model (int port, int *model)
{
  int recover = 0, rc;

  /* set up port */
  sanei_umax_pp_setport (port);
  if (lock_parport () == UMAX1220P_BUSY)
    return (UMAX1220P_BUSY);

  /* init transport layer */
  /* 0: failed
     1: success
     2: retry
     3: busy
   */
  do
    {
      rc = sanei_umax_pp_InitTransport (recover);
    }
  while (rc == 2);

  if (rc == 3)
    return (UMAX1220P_BUSY);
  if (rc != 1)
    {
      DBG (0, "sanei_umax_pp_InitTransport() failed (%s:%d)\n", __FILE__,
	   __LINE__);
      return (UMAX1220P_TRANSPORT_FAILED);
    }
  rc = sanei_umax_pp_CheckModel ();
  sanei_umax_pp_EndSession ();
  unlock_parport ();
  if (rc < 610)
    {
      DBG (0, "sanei_umax_pp_CheckModel() failed (%s:%d)\n", __FILE__,
	   __LINE__);
      return (UMAX1220P_PROBE_FAILED);
    }
  *model = rc;


  /* OK */
  return (UMAX1220P_OK);
}

int
sanei_umax_pp_attach (int port)
{
  int recover = 0;

  /* set up port */
  sanei_umax_pp_setport (port);
  if (sanei_umax_pp_InitPort (port) != 1)
    return (UMAX1220P_PROBE_FAILED);
  if (lock_parport () == UMAX1220P_BUSY)
    return (UMAX1220P_BUSY);
  if (sanei_umax_pp_ProbeScanner (recover) != 1)
    {
      if (recover)
	{
	  sanei_umax_pp_InitTransport (recover);
	  sanei_umax_pp_EndSession ();
	  if (sanei_umax_pp_ProbeScanner (recover) != 1)
	    {
	      DBG (0, "Recover failed ....\n");
	      unlock_parport ();
	      return (UMAX1220P_PROBE_FAILED);
	    }
	}
      else
	{
	  unlock_parport ();
	  return (UMAX1220P_PROBE_FAILED);
	}
    }
  sanei_umax_pp_EndSession ();
  unlock_parport ();


  /* OK */
  return (UMAX1220P_OK);
}


int
sanei_umax_pp_open (int port)
{
  int rc;
  int recover = 0;

  /* set up port */
  sanei_umax_pp_setport (port);
  if (lock_parport () == UMAX1220P_BUSY)
    return (UMAX1220P_BUSY);

  /* init transport layer */
  /* 0: failed
     1: success
     2: retry
     3: scanner busy
   */
  do
    {
      rc = sanei_umax_pp_InitTransport (recover);
    }
  while (rc == 2);

  if (rc == 3)
    {
      unlock_parport ();
      return (UMAX1220P_BUSY);
    }

  if (rc != 1)
    {

      DBG (0, "sanei_umax_pp_InitTransport() failed (%s:%d)\n", __FILE__,
	   __LINE__);
      unlock_parport ();
      return (UMAX1220P_TRANSPORT_FAILED);
    }
  /* init scanner */
  if (sanei_umax_pp_InitScanner (recover) == 0)
    {
      DBG (0, "sanei_umax_pp_InitScanner() failed (%s:%d)\n", __FILE__,
	   __LINE__);
      sanei_umax_pp_EndSession ();
      unlock_parport ();
      return (UMAX1220P_SCANNER_FAILED);
    }

  /* OK */
  unlock_parport ();
  return (UMAX1220P_OK);
}


int
sanei_umax_pp_cancel (void)
{
  if (lock_parport () == UMAX1220P_BUSY)
    return (UMAX1220P_BUSY);

  /* maybe bus reset here if exists */
  sanei_umax_pp_CmdSync (0xC2);
  sanei_umax_pp_CmdSync (0x00);
  sanei_umax_pp_CmdSync (0x00);
  if (sanei_umax_pp_Park () == 0)
    {
      DBG (0, "Park failed !!! (%s:%d)\n", __FILE__, __LINE__);
      return (UMAX1220P_PARK_FAILED);
    }
  /* EndSession() cancels any pending command  */
  /* such as parking ...., so we only return   */
  sanei_umax_pp_ReleaseScanner ();
  unlock_parport ();
  return (UMAX1220P_OK);
}



int
sanei_umax_pp_start (int x, int y, int width, int height, int dpi, int color,
		     int gain, int highlight, int *rbpp, int *rtw, int *rth)
{
  int col = BW_MODE;

  if (lock_parport () == UMAX1220P_BUSY)
    return (UMAX1220P_BUSY);
  /* end session isn't done by cancel any more */
  sanei_umax_pp_EndSession ();

  if (color)
    col = RGB_MODE;
  if (sanei_umax_pp_StartScan
      (x + 144, y, width, height, dpi, col, gain, highlight, rbpp, rtw,
       rth) != 1)
    {
      sanei_umax_pp_EndSession ();
      unlock_parport ();
      return (UMAX1220P_START_FAILED);
    }
  return (UMAX1220P_OK);
}

int
sanei_umax_pp_read (long len, int window, int dpi, int last,
		    unsigned char *buffer)
{
  if (sanei_umax_pp_ReadBlock (len, window, dpi, last, buffer) == 0)
    {
      sanei_umax_pp_EndSession ();
      return (UMAX1220P_READ_FAILED);
    }
  return (UMAX1220P_OK);
}



int
sanei_umax_pp_lamp (int on)
{
  /* init transport layer */
  if (lock_parport () == UMAX1220P_BUSY)
    return (UMAX1220P_BUSY);
  if (sanei_umax_pp_InitTransport (0) != 1)
    {
      DBG (0, "sanei_umax_pp_IniTransport() failed (%s:%d)\n", __FILE__,
	   __LINE__);
      sanei_umax_pp_EndSession ();
      return (UMAX1220P_TRANSPORT_FAILED);
    }
  if (sanei_umax_pp_SetLamp (on) == 0)
    {
      DBG (0, "Setting lamp state failed!\n");
    }
  sanei_umax_pp_EndSession ();
  unlock_parport ();
  return (UMAX1220P_OK);
}




int
sanei_umax_pp_status (void)
{
  int status;

  if (lock_parport () == UMAX1220P_BUSY)
    return (UMAX1220P_BUSY);
  /* check if head is at home */
  sanei_umax_pp_CmdSync (0x40);
  status = sanei_umax_pp_ScannerStatus ();
  if ((status & MOTOR_BIT) == 0x00)
    return (UMAX1220P_BUSY);
  unlock_parport ();

  return (UMAX1220P_OK);
}
