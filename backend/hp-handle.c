/* sane - Scanner Access Now Easy.
   Copyright (C) 1997 Geoffrey T. Dairiki
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

   This file is part of a SANE backend for HP Scanners supporting
   HP Scanner Control Language (SCL).
*/

#include <sane/config.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <string.h>
#include <signal.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>

#include "hp.h"

#include <sane/sanei_backend.h>

#include "hp-device.h"
#include "hp-option.h"
#include "hp-accessor.h"
#include "hp-scsi.h"
#include "hp-scl.h"

#if (defined(__IBMC__) || defined(__IBMCPP__))
#ifndef _AIX
#define inline /* */
#endif
#endif

struct hp_handle_s
{
    HpData		data;
    HpDevice		dev;
    SANE_Parameters	scan_params;

    pid_t		reader_pid;
    size_t		bytes_left;
    int			pipefd;

    sig_atomic_t	cancelled;
};


static inline hp_bool_t
hp_handle_isScanning (HpHandle this)
{
  return this->reader_pid != 0;
}

static SANE_Status
hp_handle_startReader (HpHandle this, HpScsi scsi, size_t count,
                       int mirror, int bytes_per_line, int bpc)
{
  int	fds[2];
  sigset_t 		sig_set, old_set;
  struct SIGACTION	sa;

  assert(this->reader_pid == 0);
  this->cancelled = 0;

  if (pipe(fds))
      return SANE_STATUS_IO_ERROR;

  sigfillset(&sig_set);
  sigprocmask(SIG_BLOCK, &sig_set, &old_set);

  if ((this->reader_pid = fork()) != 0)
    {
      sigprocmask(SIG_SETMASK, &old_set, 0);
      close(fds[1]);

      if (this->reader_pid == -1)
	{
	  close(fds[0]);
	  return SANE_STATUS_IO_ERROR;
	}

      this->pipefd = fds[0];
      DBG(1, "start_reader: reader proces %d started\n", this->reader_pid);
      return SANE_STATUS_GOOD;
    }

  close(fds[0]);

  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = SIG_DFL;
  sigaction(SIGTERM, &sa, 0);
  sigdelset(&sig_set, SIGTERM);
  sigprocmask(SIG_SETMASK, &sig_set, 0);

  _exit(sanei_hp_scsi_pipeout(scsi,fds[1],count,mirror,bytes_per_line,bpc));
}

static SANE_Status
hp_handle_stopScan (HpHandle this)
{
  HpScsi	scsi;

  this->cancelled = 0;
  this->bytes_left = 0;

  if (this->reader_pid)
    {
      int info;
      DBG(3, "do_cancel: killing child (%d)\n", this->reader_pid);
      kill(this->reader_pid, SIGTERM);
      waitpid(this->reader_pid, &info, 0);
      DBG(1, "do_cancel: child %s = %d\n",
	  WIFEXITED(info) ? "exited, status" : "signalled, signal",
	  WIFEXITED(info) ? WEXITSTATUS(info) : WTERMSIG(info));
      close(this->pipefd);
      this->reader_pid = 0;

      if (WIFSIGNALED(info)
	  && !FAILED( sanei_hp_scsi_new(&scsi, this->dev->sanedev.name) ))
	{
	  /*
	  sanei_hp_scl_set(scsi, SCL_CLEAR_ERRORS, 0);
	  sanei_hp_scl_errcheck(scsi);
	  */
	  sanei_hp_scl_reset(scsi);
	  sanei_hp_scsi_destroy(scsi);
	}
    }
  return SANE_STATUS_GOOD;
}

static SANE_Status
hp_handle_uploadParameters (HpHandle this, HpScsi scsi)
{
  SANE_Parameters * p	 = &this->scan_params;
  int data_width;

  assert(scsi);

  p->last_frame = SANE_TRUE;
  /* inquire resulting size of image after setting it up */
  RETURN_IF_FAIL( sanei_hp_scl_inquire(scsi, SCL_PIXELS_PER_LINE,
				 &p->pixels_per_line,0,0) );
  RETURN_IF_FAIL( sanei_hp_scl_inquire(scsi, SCL_BYTES_PER_LINE,
				 &p->bytes_per_line,0,0) );
  RETURN_IF_FAIL( sanei_hp_scl_inquire(scsi, SCL_NUMBER_OF_LINES,
				 &p->lines,0,0));
  RETURN_IF_FAIL( sanei_hp_scl_inquire(scsi, SCL_DATA_WIDTH,
                                &data_width,0,0));

  switch (sanei_hp_optset_scanmode(this->dev->options, this->data)) {
  case HP_SCANMODE_LINEART: /* Lineart */
  case HP_SCANMODE_HALFTONE: /* Halftone */
      p->format = SANE_FRAME_GRAY;
      p->depth  = 1;
      break;
  case HP_SCANMODE_GRAYSCALE: /* Grayscale */
      p->format = SANE_FRAME_GRAY;
      p->depth  = 8;
      break;
  case HP_SCANMODE_COLOR: /* RGB */
      p->format = SANE_FRAME_RGB;
      p->depth  = data_width/3;
      DBG(1, "hp_handle_uploadParameters: data with %d\n", data_width);
      break;
  default:
      assert(!"Aack");
      return SANE_STATUS_INVAL;
  }

  return SANE_STATUS_GOOD;
}



HpHandle
sanei_hp_handle_new (HpDevice dev)
{
  HpHandle new	= sanei_hp_allocz(sizeof(*new));

  if (!new)
      return 0;

  if (!(new->data = sanei_hp_data_dup(dev->data)))
    {
      sanei_hp_free(new);
      return 0;
    }

  new->dev = dev;
  return new;
}

void
sanei_hp_handle_destroy (HpHandle this)
{
  hp_handle_stopScan(this);
  sanei_hp_data_destroy(this->data);
  sanei_hp_free(this);
}

const SANE_Option_Descriptor *
sanei_hp_handle_saneoption (HpHandle this, SANE_Int optnum)
{
  return sanei_hp_optset_saneoption(this->dev->options, this->data, optnum);
}

SANE_Status
sanei_hp_handle_control(HpHandle this, SANE_Int optnum,
		  SANE_Action action, void *valp, SANE_Int *info)
{
  SANE_Status status;
  HpScsi  scsi;
  hp_bool_t immediate;

#if 0
  if (hp_handle_isScanning(this))
      return SANE_STATUS_DEVICE_BUSY;
#endif

  if (hp_handle_isScanning(this))
    return SANE_STATUS_DEVICE_BUSY;

  RETURN_IF_FAIL( sanei_hp_scsi_new(&scsi, this->dev->sanedev.name) );

  immediate = sanei_hp_optset_isImmediate(this->dev->options, optnum);

  status = sanei_hp_optset_control(this->dev->options, this->data,
                                   optnum, action, valp, info, scsi,
                                   immediate);
  sanei_hp_scsi_destroy ( scsi );

  return status;
}

SANE_Status
sanei_hp_handle_getParameters (HpHandle this, SANE_Parameters *params)
{
  if (!params)
      return SANE_STATUS_GOOD;

  if (hp_handle_isScanning(this))
    {
      *params = this->scan_params;
      return SANE_STATUS_GOOD;
    }

  return sanei_hp_optset_guessParameters(this->dev->options,
                                         this->data, params);
}

SANE_Status
sanei_hp_handle_startScan (HpHandle this)
{
  SANE_Status	status;
  HpScsi	scsi;
  hp_bool_t     mirror_vertical;
  hp_bool_t     adf_scan;

  /* FIXME: setup preview mode stuff? */

  if (hp_handle_isScanning(this))
      RETURN_IF_FAIL( hp_handle_stopScan(this) );

  RETURN_IF_FAIL( sanei_hp_scsi_new(&scsi, this->dev->sanedev.name) );

  status = sanei_hp_optset_download(this->dev->options, this->data, scsi);

  if (!FAILED(status))
     status = hp_handle_uploadParameters(this, scsi);

  if (FAILED(status))
    {
      sanei_hp_scsi_destroy(scsi);
      return status;
    }

  mirror_vertical = sanei_hp_optset_mirror_vert (this->dev->options, this->data,
                                           scsi);
  DBG(1, "start: %s to mirror image vertically\n", mirror_vertical ?
         "Request" : "No request" );

  adf_scan = sanei_hp_optset_adf_scan (this->dev->options, this->data, scsi);

  DBG(1, "start: %s to mirror image vertically\n", mirror_vertical ?
         "Request" : "No request" );

  this->bytes_left = ( this->scan_params.bytes_per_line
  		       * this->scan_params.lines );

  DBG(1, "start: %d pixels per line, %d bytes, %d lines high\n",
      this->scan_params.pixels_per_line, this->scan_params.bytes_per_line,
      this->scan_params.lines);

  status = sanei_hp_scl_startScan(scsi, adf_scan);

  if (!FAILED( status ))
      status = hp_handle_startReader(this, scsi, this->bytes_left,
                 (int)mirror_vertical, (int)this->scan_params.bytes_per_line,
                 (int)this->scan_params.depth);

  sanei_hp_scsi_destroy(scsi);

  return status;
}


SANE_Status
sanei_hp_handle_read (HpHandle this, void * buf, size_t *lengthp)
{
  ssize_t	nread;
  SANE_Status	status;

  DBG(3, "read: trying to read %lu bytes\n", (unsigned long) *lengthp);

  if (!hp_handle_isScanning(this))
    {
      DBG(1, "read: not scanning\n");
      return SANE_STATUS_INVAL;
    }

  if (this->cancelled)
    {
      DBG(1, "read: cancelled\n");
      RETURN_IF_FAIL( hp_handle_stopScan(this) );
      return SANE_STATUS_CANCELLED;
    }

  if (*lengthp == 0)
      return SANE_STATUS_GOOD;

  if (*lengthp > this->bytes_left)
      *lengthp = this->bytes_left;

  if ((nread = read(this->pipefd, buf, *lengthp)) < 0)
    {
      *lengthp = 0;
      if (errno == EAGAIN)
	  return SANE_STATUS_GOOD;
      DBG(1, "read: read from pipe: %s\n", strerror(errno));
      hp_handle_stopScan(this);
      return SANE_STATUS_IO_ERROR;
    }

  this->bytes_left -= (*lengthp = nread);

  if (nread > 0)
    {
      DBG(3, "read: read %lu bytes\n", (unsigned long) nread);
      return SANE_STATUS_GOOD;
    }

  DBG(1, "read: EOF from pipe\n");
  status = this->bytes_left ? SANE_STATUS_IO_ERROR : SANE_STATUS_EOF;
  RETURN_IF_FAIL( hp_handle_stopScan(this) );

  /* Check unload after scan */
  if (status == SANE_STATUS_EOF)
  {
    const HpDeviceInfo *hpinfo;
    hpinfo = sanei_hp_device_info_get ( this->dev->sanedev.name );
    if ( hpinfo && hpinfo->unload_after_scan )
    {
      HpScsi scsi;
      if (    sanei_hp_scsi_new(&scsi, this->dev->sanedev.name)
           == SANE_STATUS_GOOD )
      {
        sanei_hp_scl_set(scsi, SCL_UNLOAD, 0);
        sanei_hp_scsi_destroy(scsi);
      }
    }
  }
  return status;
}

void
sanei_hp_handle_cancel (HpHandle this)
{
  this->cancelled = 1;
}

SANE_Status
sanei_hp_handle_setNonblocking (HpHandle this, hp_bool_t non_blocking)
{
  if (!hp_handle_isScanning(this))
      return SANE_STATUS_INVAL;

  if (this->cancelled)
    {
      RETURN_IF_FAIL( hp_handle_stopScan(this) );
      return SANE_STATUS_CANCELLED;
    }

  if (fcntl(this->pipefd, F_SETFL, non_blocking ? O_NONBLOCK : 0) < 0)
      return SANE_STATUS_IO_ERROR;

  return SANE_STATUS_GOOD;
}

SANE_Status
sanei_hp_handle_getPipefd (HpHandle this, SANE_Int *fd)
{
  if (! hp_handle_isScanning(this))
      return SANE_STATUS_INVAL;

  if (this->cancelled)
    {
      RETURN_IF_FAIL( hp_handle_stopScan(this) );
      return SANE_STATUS_CANCELLED;
    }

  *fd = this->pipefd;
  return SANE_STATUS_GOOD;
}
